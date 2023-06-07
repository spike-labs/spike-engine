/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#pragma once

#include "core/object/script_language.h"
#include "core/os/mutex.h"
#include "def_lua.h"

#ifdef TOOLS_ENABLED
#ifdef GODOT_3_X
#include "editor/doc/doc_data.h"
#else
#include "editor/doc_tools.h"
#endif
#endif

#define _LUA_INLINE_ _FORCE_INLINE_

typedef void (*godot_TypeRegister)(lua_State *L);
typedef Variant (*lua2godot_TypeConvert)(lua_State *L, int pos);
typedef void (*godot2lua_TypeConvert)(lua_State *L, const Variant &var);
typedef void (*udata2lua_TypeConvert)(lua_State *L, const void *udata);
typedef Variant (*udata2var_TypeConvert)(void *userdata);

template <class T>
_FORCE_INLINE_ static Variant userdata2variant(void *userdata) {
	return Variant(*(T *)userdata);
}

class LuaScript;

class LuaScriptLanguage : public ScriptLanguage {
	static LuaScriptLanguage *singleton;

	friend class LuaScript;
	friend class LuaScriptInstance;

private:
	Mutex mutex{};
	SelfList<LuaScript>::List script_list{};
	int env_meta;

	/**
	 * @brief A lua table, implement api for lua script
	 */
	int luascript_api;

	/**
	 * @brief A lua function, call to finish loading a script
	 */
	int fn_reassign_script;

	/**
	 * @brief A lua table, holds all loaded scripts
	 */
	int scripts;

	/**
	 * @brief A lua table, holds closures for Godot Object/Variant
	 */
	int methods;

	Map<StringName, godot_TypeRegister> registers_map;
	Map<StringName, udata2var_TypeConvert> udata_to_var_map;

	Map<StringName, lua2godot_TypeConvert> lua_to_godot_map;
	Map<StringName, godot2lua_TypeConvert> godot_to_lua_map;
	Map<StringName, udata2lua_TypeConvert> udata_to_lua_map;
	Map<StringName, Variant::Type> variant_type_map;

	Object *singleton4lua;

#ifndef GODOT_3_X
	Mutex cache_mutex;
	/**
	 * @brief Scripts that have been compiled
	 *
	 */
	Map<String, LuaScript *> full_script_cache;
#endif

public:
	LuaScriptLanguage();
	virtual ~LuaScriptLanguage();

	lua_State *state;

	_FORCE_INLINE_ static LuaScriptLanguage *get_singleton() {
		return singleton;
	}

	_FORCE_INLINE_ static Object *get_object() {
		return singleton->singleton4lua;
	}

	///////////////////////////////////////////////////////////////////////////
	/** USERDATA Handle: Godot Value Type                                   **/
	///////////////////////////////////////////////////////////////////////////
	_LUA_INLINE_ static void add_register(const StringName &class_name, godot_TypeRegister _register) {
		singleton->registers_map.insert(class_name, _register);
	}
	static void register_userdata(lua_State *L, const char *class_name);
	template <class T>
	_LUA_INLINE_ static void push_userdata(lua_State *L, const char *class_name, const T &userdata) {
		lua_createtable(L, 0, 1);
		auto udata = lua_newuserdata(L, sizeof(T));
		memnew_placement(udata, T(userdata));
		lua_setfield(L, -2, K_LUA_UDATA);
		if (singleton->udata_to_var_map.find(class_name) == nullptr) {
			register_userdata(L, class_name);
			singleton->udata_to_var_map.insert(class_name, &userdata2variant<T>);
		}
		gdlua_setmetatable(L, -2);
	}

	_LUA_INLINE_ static udata2var_TypeConvert find_udata2var(const StringName &type_name) {
		auto elem = singleton->udata_to_var_map.find(type_name);
		return elem ? ITER_GET(elem) : nullptr;
	}

	_LUA_INLINE_ static void add_udata2lua(const StringName &class_name, udata2lua_TypeConvert converter) {
		singleton->udata_to_lua_map.insert(class_name, converter);
	}
	_LUA_INLINE_ static udata2lua_TypeConvert find_udata2lua(const StringName &class_name) {
		auto elem = singleton->udata_to_lua_map.find(class_name);
		return elem ? ITER_GET(elem) : nullptr;
	}

	///////////////////////////////////////////////////////////////////////////

	_LUA_INLINE_ static void add_lua2godot(const StringName &class_name, lua2godot_TypeConvert converter) {
		singleton->lua_to_godot_map.insert(class_name, converter);
	}

	_LUA_INLINE_ static lua2godot_TypeConvert find_lua2godot(const StringName &class_name) {
		auto elem = singleton->lua_to_godot_map.find(class_name);
		return elem ? ITER_GET(elem) : nullptr;
	}

	_LUA_INLINE_ static void add_godot2lua(const StringName &class_name, godot2lua_TypeConvert converter) {
		singleton->godot_to_lua_map.insert(class_name, converter);
	}
	_LUA_INLINE_ static godot2lua_TypeConvert find_godot2lua(const StringName &class_name) {
		auto elem = singleton->godot_to_lua_map.find(class_name);
		return elem ? ITER_GET(elem) : nullptr;
	}

	///////////////////////////////////////////////////////////////////////////
	/** USERDATA Handle: Godot Variant                                      **/
	///////////////////////////////////////////////////////////////////////////

	_LUA_INLINE_ static Variant::Type find_variant_type(const StringName &type_name) {
		auto elem = singleton->variant_type_map.find(type_name);
		return elem ? ITER_GET(elem) : Variant::NIL;
	}
	static void register_variant(lua_State *L, Variant::Type type, const char *type_name);
	static void push_variant(lua_State *L, const Variant *variant);

	///////////////////////////////////////////////////////////////////////////
	/** USERDATA Handle: Godot Object                                       **/
	///////////////////////////////////////////////////////////////////////////
	static void register_godot_object(lua_State *L, const StringName &class_name);
	static const bool import_godot_object(lua_State *L, const String &class_name);
	static void push_godot_object(lua_State *L, Object *obj);
	static void ref_godot_object(lua_State *L, Object *obj) {
		lua_createtable(L, 0, 1);
		if (obj != nullptr) {
			if (auto ref = Object::cast_to<Reference>(obj)) {
				// Increase reference count which will decrease when `__gc`.
				ref->reference();
				lua_pushlightuserdata(L, obj);
				//DLog("push Reference: %s %d", obj, ref->reference_get_count());
			} else {
				// Use `ObjectID` in lua will be safe, because the object can be delete from anywhere.
				lua_pushinteger(L, OID_2_INT(obj->get_instance_id()));
			}
		} else {
			lua_pushinteger(L, 0);
		}
		lua_setfield(L, -2, K_LUA_UDATA);
		import_godot_object(L, obj ? obj->get_class_name() : "Object");
		gdlua_setmetatable(L, -2);
	}

	///////////////////////////////////////////////////////////////////////////

	_FORCE_INLINE_ static void push_luascript(lua_State *L, const char *res_path) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, singleton->scripts);
		lua_getfield(L, -1, res_path);
		lua_remove(L, -2);
	}

	_FORCE_INLINE_ static void push_luascript_api(lua_State *L, const char *method) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, singleton->luascript_api);
		lua_getfield(L, -1, method);
		lua_remove(L, -2);
#if DEBUG_ENABLED
		if (lua_type(L, -1) != LUA_TFUNCTION) {
			ELog("push_luascript_api '%s' = %s", method, lua_tostring(L, -1));
		}
#endif
	}

	_FORCE_INLINE_ static void register_godot_method(lua_State *L, const char *method, int pos) {
		pos = lua_absindex(L, pos);
		lua_rawgeti(L, LUA_REGISTRYINDEX, singleton->methods);
		lua_pushstring(L, method);
		lua_pushvalue(L, pos);
		lua_settable(L, -3);
		lua_pop(L, 1);
	}

	_FORCE_INLINE_ static bool push_godot_method(lua_State *L, const char *method) {
		lua_rawgeti(L, LUA_REGISTRYINDEX, singleton->methods);
		lua_getfield(L, -1, method);
		bool exists = lua_isfunction(L, -1);
		if (exists) {
			lua_remove(L, -2);
		} else {
			lua_pop(L, 2);
		}
		return exists;
	}

	static Error compile_script(lua_State *L, const String &path, const char *buffer, size_t size);
	static Error compile_script(lua_State *L, const String &path);

	virtual String get_name() const override;

	virtual void init() override;
	virtual String get_type() const override;
	virtual String get_extension() const override;
	virtual void finish() override;

	virtual void get_reserved_words(List<String> *p_words) const override;
	virtual bool is_control_flow_keyword(String p_keyword) const override;
	virtual void get_comment_delimiters(List<String> *p_delimiters) const override;
	virtual void get_string_delimiters(List<String> *p_delimiters) const override;
#ifdef GODOT_3_X
	virtual Ref<Script> get_template(const String &p_class_name, const String &p_base_class_name) const override;
	virtual void make_template(const String &p_class_name, const String &p_base_class_name, Ref<Script> &p_script) override;
#else
	virtual Ref<Script> make_template(const String &p_template, const String &p_class_name, const String &p_base_class_name) const override;
	virtual Vector<ScriptTemplate> get_built_in_templates(StringName p_object) override;
#endif
	virtual bool is_using_templates() override;
#ifdef GODOT_3_X
	virtual bool validate(const String &p_script, int &r_line_error, int &r_col_error, String &r_test_error, const String &p_path = "", List<String> *r_functions = nullptr, List<Warning> *r_warnings = nullptr, Set<int> *r_safe_lines = nullptr) const override;
#else
	virtual bool validate(const String &p_script, const String &p_path = "", List<String> *r_functions = nullptr, List<ScriptError> *r_errors = nullptr, List<Warning> *r_warnings = nullptr, HashSet<int> *r_safe_lines = nullptr) const override;
#endif
	virtual String validate_path(const String &p_path) const override;
	virtual Script *create_script() const override;
	virtual bool has_named_classes() const override;
	virtual bool supports_builtin_mode() const override;
#ifndef GODOT_3_X
	virtual bool supports_documentation() const override {
		return false;
	}
#endif
	virtual bool can_inherit_from_file() const override;
	virtual int find_function(const String &p_function, const String &p_code) const override;
	virtual String make_function(const String &p_class, const String &p_name, const PoolStringArray &p_args) const override;
	virtual Error open_in_external_editor(const Ref<Script> &p_script, int p_line, int p_col) override;
	virtual bool overrides_external_editor() override;

	virtual Error complete_code(const String &p_code, const String &p_path, Object *p_owner, List<ScriptCodeCompletionOption> *r_options, bool &r_force, String &r_call_hint) override;

	virtual Error lookup_code(const String &p_code, const String &p_symbol, const String &p_base_path, Object *p_owner, LookupResult &r_result) override;

	virtual void auto_indent_code(String &p_code, int p_from_line, int p_to_line) const override;
	virtual void add_global_constant(const StringName &p_variable, const Variant &p_value) override;
	virtual void add_named_global_constant(const StringName &p_name, const Variant &p_value) override;
	virtual void remove_named_global_constant(const StringName &p_name) override;

	virtual void thread_enter() override;
	virtual void thread_exit() override;

	virtual String debug_get_error() const override;
	virtual int debug_get_stack_level_count() const override;
	virtual int debug_get_stack_level_line(int p_level) const override;
	virtual String debug_get_stack_level_function(int p_level) const override;
	virtual String debug_get_stack_level_source(int p_level) const override;
	virtual void debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
	virtual void debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
	virtual ScriptInstance *debug_get_stack_level_instance(int p_level) override;
	virtual void debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override;
	virtual String debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems = -1, int p_max_depth = -1) override;

	virtual Vector<StackInfo> debug_get_current_stack_info() override;

	virtual void reload_all_scripts() override;
	virtual void reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) override;

	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual void get_public_functions(List<MethodInfo> *p_functions) const override;
	virtual void get_public_constants(List<Pair<String, Variant>> *p_constants) const override;
#ifndef GODOT_3_X
	virtual void get_public_annotations(List<MethodInfo> *p_annotations) const override;
#endif

	virtual void profiling_start() override;
	virtual void profiling_stop() override;

	virtual int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) override;
	virtual int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) override;

	virtual void frame() override;

	virtual bool handles_global_class_type(const String &p_type) const override;
	virtual String get_global_class_name(const String &p_path, String *r_base_type = nullptr, String *r_icon_path = nullptr) const override;

private:
	static const StringName &_import_godot_class(lua_State *L, const StringName &class_name);
	static void _get_reserved_words(List<String> *p_words);
	static void _register_luaclass(lua_State *L, int pos, const String &path);
	static bool _register_luaclass_neasted(lua_State *L, const String &path);

	String get_indentation() const;

	lua_State *_debug_state;
	Vector<StackInfo> _stack_info;

public:
	static void breakpoint(lua_State *L) {
		singleton->_debug_state = L;
		singleton->_stack_info.clear();
	}
	static void trackback(const char *source, int line, const char *function) {
		StackInfo ent;
		ent.file = source;
		ent.line = line;
		ent.func = function;
		singleton->_stack_info.push_back(ent);
	}

	static void record_stack_info(lua_State *L, int level, lua_Debug *ar) {
		while (lua_getstack(L, level++, ar)) {
			lua_getinfo(L, "Slnt", ar);
			const char *source = strlen(ar->source) < strlen(ar->short_src) ? ar->source : ar->short_src;
			trackback(source, ar->currentline, ar->name);
		}
	}

	static void push_lua_error(lua_State *L, const char *msg) {
		breakpoint(L);
		lua_Debug ar;
		record_stack_info(L, 1, &ar);
		ERR_PRINT(msg);
	}

#ifdef GODOT_3_X
#ifdef TOOLS_ENABLED
	static void save_emmylua_doc(DocData &doc, String &path);
#endif
#else
#ifdef TOOLS_ENABLED
	static void save_emmylua_doc(DocTools &doc, String &path);
#endif
	static LuaScript *get_cache_script(const String &p_path) {
		MutexLock lock(singleton->cache_mutex);
		if (singleton->full_script_cache.has(p_path)) {
			return singleton->full_script_cache[p_path];
		}
		return nullptr;
	}
	static void remove_cache_script(const String &p_path) {
		MutexLock lock(singleton->cache_mutex);
		singleton->full_script_cache.erase(p_path);
	}
#endif
};
