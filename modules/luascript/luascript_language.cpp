/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include <string>

#include "constants.h"
#ifdef GODOT_3_X
#include "core/engine.h"
#include "core/list.h"
#include "core/os/dir_access.h"
#else
#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/string/translation.h"
#include "core/templates/list.h"
#endif
#include "core/debugger/engine_debugger.h"
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "editor/editor_settings.h"
#include "godot_lua_cfuntions.h"
#include "godot_lua_convert_api.h"
#include "godot_lua_table_api.h"
#include "luascript.h"
#include "luascript_instance.h"
#include "luascript_language.h"
#include "modules/lua_templates.gen.h"

/*****************************************************************************/
extern void init_lua_global(lua_State *L);
extern void load_thirdlib_serpent(lua_State *L);
extern void register_godot_userdata_types(lua_State *L);
extern void load_godot_value_types(lua_State *L);
extern void create_gdsingleton_for_lua(Object *singleton);
extern int ref_luascript_api(lua_State *L);

void godot_lua_warn(void *ud, const char *msg, int tocont);

static void ref_global_var(const char *key, int &ref) {
	lua_State *L = LUA_STATE;
	lua_getglobal(L, key);
	if (lua_isnil(L, -1))
		ELog("ref_global_var [\"%s\"] = nil!", key);
	ref = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_pushnil(L);
	lua_setglobal(L, key);
}

static void global_method_to_object(const char *g_method, const char *method) {
	lua_State *L = LUA_STATE;
	lua_getglobal(L, g_method);
	LuaScriptLanguage::get_singleton()->register_godot_method(L, method, -1);
	lua_pop(L, 1);
	lua_pushnil(L);
	lua_setglobal(L, g_method);
}

LuaScriptLanguage *LuaScriptLanguage::singleton = nullptr;

LuaScriptLanguage::LuaScriptLanguage() {
	ERR_FAIL_COND(singleton);
	this->singleton = this;

	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		auto type_name = Variant::get_type_name(Variant::Type(i));
		if (type_name != "") {
			variant_type_map.insert(type_name, Variant::Type(i));
		}
	}
}

LuaScriptLanguage::~LuaScriptLanguage() {
	if (this->singleton == this) {
		this->singleton = nullptr;
	}
}

void LuaScriptLanguage::init() {
	lua_State *L = state = luaL_newstate();
	lua_atpanic(L, &godot_lua_atpanic);
	lua_setwarnf(L, &godot_lua_warn, nullptr);
	luaL_openlibs(L);

	// Sets metatable for global to index Godot global members
	lua_pushglobaltable(L);
	lua_newtable(L);
	luatable_rawset(L, -1, "__index", &godot_global_indexer);
	gdlua_setmetatable(L, -2);
	lua_settop(L, 0);

	// Create a table to hold all lua scripts
	lua_newtable(L);
	scripts = luaL_ref(L, LUA_REGISTRYINDEX);

	// Create a table to hold all closure. (Use to call Godot methods)
	lua_newtable(L);
	methods = luaL_ref(L, LUA_REGISTRYINDEX);

#ifdef USE_LUA_REQUIRE
	lua_getglobal(L, "package");
	lua_getfield(L, -1, "searchers");
	lua_pushinteger(L, 2);
	lua_pushcfunction(L, &godot_lua_seacher);
	lua_settable(L, -3);
	lua_pop(L, 2);
#else
	lua_register(L, "require", &godot_lua_loader);
#endif

	init_lua_global(L);
	//register_godot_userdata_types(L);
	load_thirdlib_serpent(L);
	load_godot_value_types(L);

	// Extends string methods
	lua_getglobal(L, "string");
	lua_newtable(L);
	luatable_rawset(L, -1, "__index", &godot_string_indexer);
	lua_setmetatable(L, -2);
	lua_pop(L, 1);

#if ENABLE_LOCAL_ENV
	lua_newtable(L);
	lua_pushglobaltable(L);
	lua_setfield(L, -2, "__index");
	env_meta = luaL_ref(L, LUA_REGISTRYINDEX);
#endif
	luascript_api = ref_luascript_api(L);
	ref_global_var("__reassign_script", fn_reassign_script);

	singleton4lua = CLS_INSTANTIATE("Object");
	create_gdsingleton_for_lua(singleton4lua);

	// Replace signal methods to support signal bind with lua funciton
	global_method_to_object("__godot_object_connect", "connect");
	global_method_to_object("__godot_object_disconnect", "disconnect");
	global_method_to_object("__godot_object_is_connected", "is_connected");

#ifdef GODOT_3_X
	auto debugger = ScriptDebugger::get_singleton();
	if (debugger && debugger->is_remote()) {
		lua_sethook(L, &godot_lua_hook, LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET, 0);
	}
#else
	auto debugger = EngineDebugger::get_singleton();
	if (debugger && !Engine::get_singleton()->is_editor_hint()) {
		lua_sethook(L, &godot_lua_hook, LUA_MASKLINE | LUA_MASKCALL | LUA_MASKRET, 0);
	}
#endif
}

String LuaScriptLanguage::get_name() const {
	return LUA_NAME;
}

String LuaScriptLanguage::get_type() const {
	return LUA_TYPE;
}

String LuaScriptLanguage::get_extension() const {
	return LUA_EXTENSION;
}

Error LuaScriptLanguage::execute_file(const String &p_path) {
	return OK;
}

void LuaScriptLanguage::finish() {
	if (singleton4lua) {
		memdelete(singleton4lua);
		singleton4lua = nullptr;
	}
	if (state) {
		lua_close(state);
		state = nullptr;
	}
}

void LuaScriptLanguage::_get_reserved_words(List<String> *p_words) {
	static const char *keywords[] = {

		"and",
		"break",
		"do",
		"else",
		"elseif",
		"end",
		"false",
		"for",
		"function",
		"goto",
		"if",
		"in",
		"local",
		"nil",
		"not",
		"or",
		"repeat",
		"return",
		"then",
		"true",
		"until",
		"while",

		nullptr
	};

	const char **w = keywords;

	while (*w) {
		p_words->push_back(*w);
		w++;
	}
}

void LuaScriptLanguage::get_reserved_words(List<String> *p_words) const {
	return _get_reserved_words(p_words);
}

bool LuaScriptLanguage::is_control_flow_keyword(String p_keyword) const {
	return p_keyword == "break" ||
			p_keyword == "else" ||
			p_keyword == "elseif" ||
			p_keyword == "do" ||
			p_keyword == "for" ||
			p_keyword == "goto" ||
			p_keyword == "if" ||
			p_keyword == "repeat" ||
			p_keyword == "return" ||
			p_keyword == "then" ||
			p_keyword == "until" ||
			p_keyword == "while";
}

void LuaScriptLanguage::get_comment_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("--[[ ]]");
	p_delimiters->push_back("--");
}

void LuaScriptLanguage::get_string_delimiters(List<String> *p_delimiters) const {
	p_delimiters->push_back("\" \"");
	p_delimiters->push_back("' '");
	p_delimiters->push_back("[[ ]]"); // Mult-line strings
}

/**
 * @brief Do not capitalize class names with uppercase letters
 */
static String check_class_name(const String &p_class_name) {
#ifdef GODOT_3_X
	const CharType *cstr = p_class_name.c_str();
#else
	String cstr = p_class_name;
#endif
	for (int i = 1; i < p_class_name.size(); i++) {
		bool is_upper = cstr[i] >= 'A' && cstr[i] <= 'Z';
		if (is_upper)
			return p_class_name;
	}
	return p_class_name.capitalize().replace(" ", "").strip_edges();
}
#ifdef GODOT_3_X
Ref<Script> LuaScriptLanguage::get_template(const String &p_class_name, const String &p_base_class_name) const {
	String _template = String() +

			"---@class %CLASS% : %BASE%\n" +
			"local %CLASS% = class(%BASE%)\n\n" +
			"---Declare member variables. Examples:\n" +
			"-- %CLASS%.a = 2\n" +
			"-- %CLASS%.b = export(\"text\")\n\n" +
			"---Called when the node enters the scene tree for the first time.\n" +
			"function %CLASS%:_ready()\n" +
			"    -- Replace with function body.\n" +
			"end\n\n" +
			"---Called every frame. 'delta' is the elapsed time since the previous frame.\n" +
			"--function %CLASS%:_process(delta)\n\n" +
			"--end\n\n" +
			"return %CLASS%\n";

	_template = _template.replace("%BASE%", p_base_class_name);
	_template = _template.replace("%CLASS%", check_class_name(p_class_name));

	Ref<LuaScript> script;
	REF_INSTANTIATE(script);
	script->set_source_code(_template);
	script->set_name(p_class_name);

	script->valid = true;

	return script;
}

void LuaScriptLanguage::make_template(const String &p_class_name, const String &p_base_class_name, Ref<Script> &p_script) {
	String src = p_script->get_source_code();
	src = src.replace("%BASE%", p_base_class_name)
				  .replace("%CLASS%", check_class_name(p_class_name))
				  .replace("%TS%", get_indentation());
	p_script->set_source_code(src);
}
#else

Ref<Script> LuaScriptLanguage::make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const {
	String _template = p_template.replace("_BASE_", p_base_class_name);
	_template = _template.replace("_CLASS_", check_class_name(p_class_name));

	Ref<LuaScript> script;
	REF_INSTANTIATE(script);
	script->set_source_code(_template);
	script->set_name(p_class_name);

	script->valid = true;
	return script;
}

Vector<ScriptLanguage::ScriptTemplate> LuaScriptLanguage::get_built_in_templates(StringName p_object) {
	Vector<ScriptLanguage::ScriptTemplate> templates;
#ifdef TOOLS_ENABLED
	for (int i = 0; i < LUA_TEMPLATES_ARRAY_SIZE; i++) {
		if (LUA_TEMPLATES[i].inherit == p_object) {
			templates.append(LUA_TEMPLATES[i]);
		}
	}
#endif
	return templates;
}
#endif
bool LuaScriptLanguage::is_using_templates() {
	return true;
}

#ifdef GODOT_3_X
bool LuaScriptLanguage::validate(const String &p_script, int &r_line_error, int &r_col_error, String &r_test_error, const String &p_path,
		List<String> *r_functions, List<Warning> *r_warnings, Set<int> *r_safe_lines) const {
	bool ret = true;
	lua_State *L = LUA_STATE;
	if (luaL_loadstring(L, GD_UTF8_STR(p_script)) != LUA_OK) {
		// [string "tool()..."]:7: syntax error near 'LuaNode'
		// [string "tool()..."]:15: 'end' expected (to close 'function' at line 7) near <eof>
		// [string "tool()..."]:10: 'then' expected near 'end'
		auto err_msg = String(lua_tostring(L, 1));
		int spos = err_msg.find("\"]:");
		if (spos > 0) {
			spos += 3;
			auto lstr = err_msg.substr(spos);
			spos = lstr.find(": ");
			r_test_error = spos > 0 ? lstr.substr(spos + 2) : lstr;
			int npos = lstr.find(" at line ");
			if (npos > 0) {
				lstr = lstr.substr(npos + 9);
			}
			r_line_error = lstr.to_int();
			r_col_error = 1;
			ret = false;
		}
	}
	lua_pop(L, 1);
	return ret;
}
#else
bool LuaScriptLanguage::validate(const String &p_script, const String &p_path,
		List<String> *r_functions, List<ScriptError> *r_errors, List<Warning> *r_warnings,
		HashSet<int> *r_safe_lines) const {
	bool ret = false;
	lua_State *L = LUA_STATE;
	int top = lua_gettop(L);
	if (luaL_loadstring(L, GD_UTF8_STR(p_script)) != LUA_OK) {
		// [string "tool()..."]:7: syntax error near 'LuaNode'
		// [string "tool()..."]:15: 'end' expected (to close 'function' at line 7) near <eof>
		// [string "tool()..."]:10: 'then' expected near 'end'
		String err_str;
		int err_line;
		auto err_msg = String(lua_tostring(L, 1));
		int spos = err_msg.find("\"]:");
		if (spos > 0) {
			spos += 3;
			auto lstr = err_msg.substr(spos);
			spos = lstr.find(": ");
			err_str = spos > 0 ? lstr.substr(spos + 2) : lstr;
			int npos = lstr.find(" at line ");
			if (npos > 0) {
				lstr = lstr.substr(npos + 9);
			}
			err_line = lstr.to_int();
			//r_col_error = 1;
		}

		if (r_errors) {
			ScriptLanguage::ScriptError e;
			e.line = err_line;
			e.column = 1;
			e.message = err_str;
			r_errors->push_back(e);
		}
	} else {
		if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
			if (r_errors) {
				ScriptLanguage::ScriptError e;
				e.line = 0;
				e.column = 0;
				e.message = lua_tostring(L, -1);
				r_errors->push_back(e);
			}
		} else {
			int rtype = lua_type(L, -1);
			if (rtype != LUA_TTABLE) {
				if (r_errors) {
					ScriptLanguage::ScriptError e;
					e.line = 0;
					e.column = 0;
					String msg_fmt = STTR("table expected, return %s");
					e.message = vformat(msg_fmt, lua_typename(L, rtype));
					r_errors->push_back(e);
				}
			} else {
				ret = true;
				lua_pushnil(L);
				while (lua_next(L, -2)) {
					if (lua_isfunction(L, -1)) {
						auto func_name = String(lua_tostring(L, -2));
						lua_Debug ar;
     					lua_getinfo(L, ">S", &ar);
						r_functions->push_back(func_name + ":" + itos(ar.linedefined));
					} else {
						lua_pop(L, 1);
					}
				}
			}
		}
	}

	lua_settop(L, top);
	return ret;
}
#endif

String LuaScriptLanguage::validate_path(const String &p_path) const {
	return EMPTY_STRING;
}

Script *LuaScriptLanguage::create_script() const {
	return memnew(LuaScript);
}

bool LuaScriptLanguage::has_named_classes() const {
	return false;
}

bool LuaScriptLanguage::supports_builtin_mode() const {
	return true;
}

bool LuaScriptLanguage::can_inherit_from_file() const {
	return true;
}

int LuaScriptLanguage::find_function(const String &p_function, const String &p_code) const {
	return -1;
}

String LuaScriptLanguage::make_function(const String &p_class, const String &p_name, const PoolStringArray &p_args) const {
	String funcDef = "function " + p_class + ":" + p_name + "(";
	for (int i = 0; i < p_args.size(); i++) {
		if (i > 0) {
			funcDef += ", ";
		}
		funcDef += p_args[i].get_slice(":", 0);
	}

	funcDef += ")\n\nend\n";

	return funcDef;
}

Error LuaScriptLanguage::open_in_external_editor(const Ref<Script> &p_script, int p_line, int p_col) {
#if TOOLS_ENABLED
	String script_ide = EditorSettings::get_singleton()->get_setting(K_EXEC_PATH);

	if (script_ide == K_EDITOR_VSCODE) {
		String exec_path = "code.cmd";
		auto d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		String res_path = ProjectSettings::get_singleton()->get_resource_path();
		String ws_path = PATH_JOIN(res_path, ".vscode");
		Error err = d->change_dir(ws_path);
		if (err != OK) {
			d->make_dir(ws_path);
			d->change_dir(ws_path);
		}

		String app_name = ProjectSettings::get_singleton()->get_setting("application/config/name");
		String ws_file = PATH_JOIN(ws_path, app_name + ".code-workspace");
		if (!d->file_exists(ws_file)) {
			String doc_path = PATH_JOIN(OS::get_singleton()->get_executable_path().get_base_dir(), "doc/lua-" + TranslationServer::get_singleton()->get_locale());
			FileAccessRef f = FileAccess::open(ws_file, FileAccess::WRITE, &err);
			f->store_line("{");
			f->store_line("\t\"folders\": [");
			f->store_line("\t\t{");
			f->store_line("\t\t\t\"path\": \"..\"");
			f->store_line("\t\t},");
			f->store_line("\t\t{");
			f->store_line("\t\t\t\"path\": \"" + doc_path + "\"");
			f->store_line("\t\t}");
			f->store_line("\t]");
			f->store_line("}");
		}

		String ext_file = PATH_JOIN(ws_path, "extensions.json");
		if (!d->file_exists(ext_file)) {
			FileAccessRef f = FileAccess::open(ext_file, FileAccess::WRITE, &err);
			f->store_line("{");
			f->store_line("\t\"recommendations\": [");
			f->store_line("\t\t\"tangzx.emmylua\",");
			f->store_line("\t\t\"geequlim.godot-tools\"");
			f->store_line("\t]");
			f->store_line("}");
		}

#ifdef GODOT_3_X
		memdelete(d);
#endif

		String script_path = ProjectSettings::get_singleton()->globalize_path(p_script->get_path());
		String location = script_path + ":" + String::num_int64(p_line) + ":" + String::num_int64(p_col);
		List<String> args;
		args.push_back(ws_file);
		args.push_back("--goto");
		args.push_back(location);
		return OS::get_singleton()->execute(exec_path, args);
	}
	return FAILED;
#else
	return ERR_UNAVAILABLE;
#endif
}

bool LuaScriptLanguage::overrides_external_editor() {
#if TOOLS_ENABLED
	String script_ide = EditorSettings::get_singleton()->get_setting(K_EXEC_PATH);
	return script_ide.length() != 0 && script_ide != K_EDITOR_BUILTIN;
#else
	return false;
#endif
}

Error LuaScriptLanguage::complete_code(const String &p_code, const String &p_path, Object *p_owner,
		List<ScriptCodeCompletionOption> *r_options, bool &r_force, String &r_call_hint) {
	// DLog("LuaScriptLanguage::complete_code=%s\n%s\n%s", p_code, p_path, p_owner);
	// for (int i = 0; i < r_options->size(); i++) {
	// 	DLog("LuaScriptLanguage::complete_code opts: %s (%s)", (*r_options)[i].display, (*r_options)[i].insert_text);
	// }

	return ERR_UNAVAILABLE;
}

Error LuaScriptLanguage::lookup_code(const String &p_code, const String &p_symbol, const String &p_base_path, Object *p_owner, LookupResult &r_result) {
	// Before parsing, try the usual stuff.
	if (ClassDB::class_exists(p_symbol)) {
		r_result.type = LOOKUP_RESULT_TYPE_CLASS;
		r_result.class_name = p_symbol;
		return OK;
	} else {
		String under_prefix = "_" + p_symbol;
		if (ClassDB::class_exists(under_prefix)) {
			r_result.type = LOOKUP_RESULT_TYPE_CLASS;
			r_result.class_name = p_symbol;
			return OK;
		}
	}

	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		Variant::Type t = Variant::Type(i);
		if (Variant::get_type_name(t) == p_symbol) {
			r_result.type = LOOKUP_RESULT_TYPE_CLASS;
			r_result.class_name = Variant::get_type_name(t);
			return OK;
		}
	}

	//DLog("LuaScriptLanguage::lookup_symbol: %s\nbase_path:%s\n%s", p_symbol, p_base_path, p_code);

	return ERR_UNAVAILABLE;
}

void LuaScriptLanguage::auto_indent_code(String &p_code, int p_from_line, int p_to_line) const {
}

void LuaScriptLanguage::add_global_constant(const StringName &p_variable, const Variant &p_value) {
	auto L = LUA_STATE;
	push_variant(L, &p_value);
	lua_setglobal(L, GD_UTF8_NAME(p_variable));
}

void LuaScriptLanguage::add_named_global_constant(const StringName &p_name, const Variant &p_value) {
}

void LuaScriptLanguage::remove_named_global_constant(const StringName &p_name) {
}

void LuaScriptLanguage::thread_enter() {
}

void LuaScriptLanguage::thread_exit() {
}

String LuaScriptLanguage::debug_get_error() const {
	return EMPTY_STRING;
}

int LuaScriptLanguage::debug_get_stack_level_count() const {
	return _stack_info.size();
}

int LuaScriptLanguage::debug_get_stack_level_line(int p_level) const {
	if (p_level < _stack_info.size()) {
		return _stack_info[p_level].line;
	}
	return -1;
}

String LuaScriptLanguage::debug_get_stack_level_function(int p_level) const {
	if (p_level < _stack_info.size()) {
		return _stack_info[p_level].func;
	}
	return EMPTY_STRING;
}

String LuaScriptLanguage::debug_get_stack_level_source(int p_level) const {
	if (p_level < _stack_info.size()) {
		return _stack_info[p_level].file;
	}
	return EMPTY_STRING;
}

void LuaScriptLanguage::debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	lua_State *L = _debug_state;
	lua_Debug ar;
	lua_getstack(L, p_level, &ar);
	int n = 1;
	while (p_max_subitems-- != 0 && n != p_max_depth) {
		const char *name = lua_getlocal(L, &ar, n);
		if (name == nullptr)
			break;

		if (strcmp(name, "(temporary)") != 0) {
			Variant value = lua_to_godot_variant(L, -1);
			if (n == 1 && value.get_type() == Variant::OBJECT && value.operator Object *()->get_script_instance()->get_language() == this) {
				// Ignore if the first value is LuaScriptInstance
			} else {
				p_locals->push_back(String(name));
				p_values->push_back(value);
			}
		}
		lua_pop(L, 1);
		n += 1;
	}
}

void LuaScriptLanguage::debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	auto inst = debug_get_stack_level_instance(p_level);
	if (inst) {
		LuaScriptInstance *lua_inst = (LuaScriptInstance *)inst;

		Ref<LuaScript> script = lua_inst->get_script();
		if (script.is_valid()) {
			List<PropertyInfo> props;
			lua_State *L = _debug_state;
			script->get_script_property_list(&props);
			lua_inst->push_self(L);
			for (auto E = props.front(); E; E = E->next()) {
				auto name = E->get().name.ascii();
				const char *field = name.get_data();
				lua_pushstring(L, field);
				lua_rawget(L, -2);
				if (lua_isnil(L, -1)) {
					lua_pop(L, 1);
					if (luaL_getmetafield(L, -1, field) == LUA_TNIL) {
						lua_pushnil(L);
					}
				}
				p_members->push_back(E->get().name);
				p_values->push_back(lua_to_godot_variant(L, -1));
				lua_pop(L, 1);
			}
			lua_pop(L, 1);
		}
	}
}

ScriptInstance *LuaScriptLanguage::debug_get_stack_level_instance(int p_level) {
	lua_State *L = _debug_state;
	int top = lua_gettop(L);
	ScriptInstance *inst = nullptr;
	lua_Debug ar;
	lua_getstack(L, p_level, &ar);
	const char *name = lua_getlocal(L, &ar, 1);
	if (strcmp(name, "self") == 0) {
		lua_getfield(L, -1, K_LUA_UDATA);
		auto owner = lua_to_godot_object(L, -1);
		if (owner != nullptr) {
			ScriptInstance *_inst = owner->get_script_instance();
			if (_inst && _inst->get_language() == this) {
				inst = _inst;
			}
		}
	}
	lua_settop(L, top);
	return inst;
}

void LuaScriptLanguage::debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
	//DLog("LuaScriptLanguage::debug_get_globals(%s)", "...");
}

String LuaScriptLanguage::debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) { // TODO
	//DLog("LuaScriptLanguage::debug_parse_stack_level_expression(%d, %s, %d, %d)", p_level, p_expression, p_max_subitems, p_max_depth);
	return EMPTY_STRING;
}

Vector<ScriptLanguage::StackInfo> LuaScriptLanguage::debug_get_current_stack_info() {
	Vector<StackInfo> vector;
	vector.append_array(_stack_info);
	return vector;
}

void LuaScriptLanguage::reload_all_scripts() {
#ifdef DEBUG_ENABLED
	List<Ref<LuaScript>> scripts;

	{
		LOCK_LUA_SCRIPT
		SelfList<LuaScript> *elem = script_list.first();
		while (elem) {
			if (elem->self()->get_path().is_resource_file()) {
				scripts.push_back(Ref<LuaScript>(elem->self()));
			}
			elem = elem->next();
		}
	}

	scripts.sort();

	for (List<Ref<LuaScript>>::Element *E = scripts.front(); E; E = E->next()) {
		E->get()->load_source_code(E->get()->get_path());
		E->get()->reload(true);
	}
#endif
}

void LuaScriptLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
}

void LuaScriptLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back(LUA_EXTENSION);
}

void LuaScriptLanguage::get_public_functions(List<MethodInfo> *p_functions) const {
}

void LuaScriptLanguage::get_public_constants(List<Pair<String, Variant>> *p_constants) const {
}

#ifndef GODOT_3_X
void LuaScriptLanguage::get_public_annotations(List<MethodInfo> *p_annotations) const {
}
#endif

void LuaScriptLanguage::profiling_start() {
}

void LuaScriptLanguage::profiling_stop() {
}

int LuaScriptLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) { // TODO

	return -1;
}

int LuaScriptLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) { // TODO

	return -1;
}

void *LuaScriptLanguage::alloc_instance_binding_data(Object *p_object) { // TODO

	return nullptr;
}

void LuaScriptLanguage::free_instance_binding_data(void *p_data) {
}

void LuaScriptLanguage::refcount_incremented_instance_binding(Object *p_object) {
}

bool LuaScriptLanguage::refcount_decremented_instance_binding(Object *p_object) {
	return true;
}

void LuaScriptLanguage::frame() {
}

bool LuaScriptLanguage::handles_global_class_type(const String &p_type) const {
	return p_type == LUA_TYPE;
}

String LuaScriptLanguage::get_global_class_name(const String &p_path, String *r_base_type, String *r_icon_path) const {
	String klass;
	lua_State *L = state;
	int top = lua_gettop(L);
	RES res = ResourceLoader::load(p_path);
	LuaScript *script = res.is_valid() ? Object::cast_to<LuaScript>(res.ptr()) : nullptr;
	if (script && script->is_valid()) {
		script->push_self(L);
		if (lua_istable(L, -1)) {
			const char *value;
			if (luatable_rawget(L, -1, K_LUA_CLASS, value)) {
				klass = String(value);
			}
			if (r_base_type) {
				*r_base_type = script->get_instance_base_type();
			}
			if (r_icon_path && luatable_rawget(L, -1, K_LUA_ICON, value)) {
				*r_icon_path = String(value);
			}
		}
	}

	lua_settop(L, top);
	return klass;
}

String LuaScriptLanguage::get_indentation() const {
	if (Engine::get_singleton()->is_editor_hint()) {
		bool use_space_indentation = EDITOR_DEF("text_editor/indent/type", 0);

		if (use_space_indentation) {
			int indent_size = EDITOR_DEF("text_editor/indent/size", LUA_PIL_IDENTATION_SIZE);

			String space_indent = EMPTY_STRING;
			for (int i = 0; i < indent_size; i++) {
				space_indent += " ";
			}
			return space_indent;
		}
	}

	return LUA_PIL_IDENTATION;
}

Error LuaScriptLanguage::compile_script(lua_State *L, const String &path, const char *buffer, size_t size) {
	int top = lua_gettop(L);
	int pos = 0;

	GD_STR_HOLD(res_path, path);
	int ret = luaL_loadbuffer(L, buffer, size, res_path);
	if (ret != LUA_OK) {
		ELog("luaL_loadstring error: %s", lua_tostring(L, -1));
		goto ERROR;
	}

#if ENABLE_LOCAL_ENV
	// create _ENV
	lua_newtable(L); // +2
	lua_pushstring(L, res_path);
	lua_setfield(L, -2, "__res_path");
	// push metatable for _ENV
	lua_rawgeti(L, LUA_REGISTRYINDEX, LuaScriptLanguage::get_singleton()->env_meta); // +3
	// set _ENV metatable
	gdlua_setmetatable(L, -2); // +2
	// set _ENV upvalue
	lua_setupvalue(L, -2, 1); // +1
#endif

	if (godot_lua_xpcall(L, 0, 1) != LUA_OK) {
		goto ERROR;
	}

	if (!lua_istable(L, -1))
		goto ERROR;

	if (!_register_luaclass_neasted(L, path)) {
		goto REGISTY;
	}

REGISTY:
	pos = lua_absindex(L, -1);
	lua_rawgeti(L, LUA_REGISTRYINDEX, singleton->scripts);

	lua_rawgeti(L, LUA_REGISTRYINDEX, singleton->fn_reassign_script);
	lua_getfield(L, -2, res_path);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 2);
		lua_pushvalue(L, pos);
		lua_setfield(L, -2, res_path);
	} else {
		lua_pushvalue(L, pos);
		godot_lua_xpcall(L, 2, 0);
	}

ERROR:
	lua_settop(L, top);
	return OK;
}

Error LuaScriptLanguage::compile_script(lua_State *L, const String &path) {
	Error error;
	if (path.ends_with(".luac")) {
		Vector<uint8_t> source = FileAccess::get_file_as_bytes(path, &error);
		if (error == OK) {
			String origin_path = path.get_basename() + ".lua";
			error = compile_script(L, origin_path, (char *)source.ptr(), source.size());
		}
	} else {
		String source = FileAccess::get_file_as_string(path, &error);
		if (error == OK) {
			CharString source_str = source.utf8();
			error = compile_script(L, path, source_str.get_data(), source_str.length());
		}
	}
	return error;
}

bool LuaScriptLanguage::_register_luaclass_neasted(lua_State *L, const String &path) {
	// search and init sub classes
	lua_pushnil(L);
	while (lua_next(L, -2)) {
		if (lua_istable(L, -1) && luatable_has(L, -1, "!inherits_gdo")) {
			String sub_path = path + "::" + lua_tostring(L, -2);
			_register_luaclass_neasted(L, sub_path);
		}
		lua_pop(L, 1);
	}

	if (luatable_has(L, -1, "!inherits_gdo")) {
		_register_luaclass(L, -1, path);
		return true;
	}
	return false;
}

void LuaScriptLanguage::_register_luaclass(lua_State *L, int pos, const String &path) {
	luatable_remove(L, pos, "!inherits_gdo");

	pos = lua_absindex(L, pos);
	lua_pushnil(L);
	lua_setmetatable(L, pos);

	luatable_rawset(L, pos, "__name", GD_UTF8_STR(path));
	luatable_rawset(L, pos, "__index", &lua_godot_object_indexer);
	luatable_rawset(L, pos, "__newindex", &lua_godot_object_newindexer);
	luatable_rawset(L, pos, "__tostring", &lua_godot_object_tostring);
	luatable_rawset(L, pos, "emit_signal", &script_emit_signal);

	lua_pushstring(L, "!metatable");
	lua_rawget(L, pos);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		const char *base_name = nullptr;
		luatable_rawget(L, pos, K_LUA_EXTENDS, base_name);
		if (base_name == nullptr || strlen(base_name) == 0) {
#ifdef GODOT_3_X
			base_name = "Reference";
#else
			base_name = "RefCounted";
#endif
			luatable_rawset(L, pos, K_LUA_EXTENDS, base_name);
		}

		if (!ClassDB::class_exists(base_name) && !ClassDB::class_exists(String("_") + base_name)) {
			String extends = String(base_name);
			if (extends.find("://") < 0) {
				if (extends.find(LUA_EXTENSION) < 0) {
					extends = String("res://") + extends.replace(".", "/") + "." + LUA_EXTENSION;
				} else {
					// Try to find scripts under relative paths
					extends = PATH_JOIN(path.get_base_dir(), extends);
				}
				luatable_rawset(L, pos, K_LUA_EXTENDS, GD_STR_DATA(extends));
			}

			RES base = ResourceLoader::load(extends);
			if (base.is_valid()) {
				LuaScript *script = Object::cast_to<LuaScript>(base.ptr());
				if (script && script->is_valid()) {
					script->push_self(L); // local mt = LuaScript
				} else {
					LuaScriptLanguage::push_godot_object(L, base.ptr());
				}
			} else {
				LuaScriptLanguage::import_godot_object(L, LUA_TYPE);
			}
		} else {
			LuaScriptLanguage::import_godot_object(L, LUA_TYPE);
		}
	} else {
		lua_pushstring(L, "!metatable");
		lua_pushnil(L);
		lua_rawset(L, pos);
	}

	gdlua_setmetatable(L, pos); // setmetatable(Class, mt)
}

#ifndef GODOT_3_X

// Ref<LuaScript> LuaScriptLanguage::get_shallow_script(const String &p_path, const String &p_owner) {
// 	MutexLock lock(singleton->cache_mutex);

// 	if (!p_owner.is_empty()) {
// 		//singleton->dependencies[p_owner].insert(p_path);
// 	}
// 	if (singleton->full_script_cache.has(p_path)) {
// 		return singleton->full_script_cache[p_path];
// 	}
// 	if (singleton->shallow_script_cache.has(p_path)) {
// 		return singleton->shallow_script_cache[p_path];
// 	}

// 	Ref<LuaScript> script;
// 	script.instantiate();
// 	script->set_path(p_path, true);
// 	//script->set_script_path(p_path);
// 	if (!p_path.ends_with(".luac")) {
// 		script->load_source_code(p_path);
// 	}

// 	singleton->shallow_script_cache[p_path] = script.ptr();
// 	return script;
// }

// Ref<LuaScript> LuaScriptLanguage::get_full_script(const String &p_path, Error &r_error, const String &p_owner) {
// 	MutexLock lock(singleton->cache_mutex);

// 	if (!p_owner.is_empty()) {
// 		//singleton->dependencies[p_owner].insert(p_path);
// 	}

// 	r_error = OK;
// 	if (singleton->full_script_cache.has(p_path)) {
// 		return singleton->full_script_cache[p_path];
// 	}

// 	Ref<LuaScript> script = get_shallow_script(p_path);
// 	ERR_FAIL_COND_V(script.is_null(), Ref<LuaScript>());

// 	r_error = script->load_source_code(p_path);

// 	if (r_error) {
// 		return script;
// 	}

// 	r_error = script->reload();
// 	if (r_error) {
// 		return script;
// 	}

// 	singleton->full_script_cache[p_path] = script.ptr();
// 	singleton->shallow_script_cache.erase(p_path);

// 	return script;
// }

// Error LuaScriptLanguage::finish_compiling(const String &p_owner) {
// 	Ref<LuaScript> script = get_shallow_script(p_owner);
// 	singleton->full_script_cache[p_owner] = script.ptr();
// 	singleton->shallow_script_cache.erase(p_owner);

// 	// HashSet<String> depends = singleton->dependencies[p_owner];

// 	Error err = OK;
// 	// for (const String &E : depends) {
// 	// 	Error this_err = OK;
// 	// 	// No need to save the script. We assume it's already referenced in the owner.
// 	// 	get_full_script(E, this_err);

// 	// 	if (this_err != OK) {
// 	// 		err = this_err;
// 	// 	}
// 	// }

// 	//singleton->dependencies.erase(p_owner);

// 	return err;
// }

#endif
