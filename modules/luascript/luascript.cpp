/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include "luascript.h"
#include "constants.h"
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "godot_lua_convert_api.h"
#include "luascript_instance.h"
#include "luascript_language.h"

#define SCRIPT_SOURCE "script/source"

LuaScript::LuaScript() :
		valid(false),
		self(this),
		_owner(nullptr) {
#ifdef TOOLS_ENABLED
	source_changed_cache = false;
	placeholder_fallback_enabled = false;
#endif

	// LOCK_LUA_SCRIPT
	LuaScriptLanguage::get_singleton()->script_list.add(&this->self);
}

LuaScript::~LuaScript() {
	// LOCK_LUA_SCRIPT
	LuaScriptLanguage::get_singleton()->script_list.remove(&this->self);
#ifndef GODOT_3_X
	LuaScriptLanguage::remove_cache_script(get_path());
#endif
}

bool LuaScript::is_tool() const {
	bool tool = false;
	lua_State *L = LUA_STATE;
	push_self(L);
	if (lua_istable(L, -1)) {
		lua_pushstring(L, "__tool");
		lua_rawget(L, -2);
		tool = lua_toboolean(L, -1);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
	return tool;
}

#ifdef GODOT_3_X
bool LuaScript::can_instance() const {
#else
bool LuaScript::can_instantiate() const {
#endif
	if (valid) {
		if (get_instance_base_type() != StringName()) {
#ifdef TOOLS_ENABLED
			return ScriptServer::is_scripting_enabled() || is_tool();
#else
			return true;
#endif
		}
	}
	return false;
}

Ref<Script> LuaScript::get_base_script() const {
	auto L = LUA_STATE;
	push_self(L);
	if (luaL_getmetafield(L, -1, K_LUA_UDATA)) {
		auto script = lua_to_godot_object(L, -1);
		lua_pop(L, 2);
		return Ref<Script>(script);
	}
	lua_pop(L, 1);

	return Ref<Script>();
}

StringName LuaScript::get_global_name() const {
	return StringName();
}

bool LuaScript::inherits_script(const Ref<Script> &p_script) const {
	if (p_script.ptr() == this)
		return true;

	Ref<Script> base_script = get_base_script();
	if (base_script.is_valid()) {
		if (base_script.ptr() == p_script.ptr())
			return true;
		return base_script->inherits_script(p_script);
	}
	return false;
}

StringName LuaScript::get_instance_base_type() const {
	Ref<Script> base_script = get_base_script();
	if (base_script.is_valid()) {
		return base_script->get_instance_base_type();
	}

	return StringName(get_extends(LUA_STATE));
}

ScriptInstance *LuaScript::_instance_create(Object *p_this) {
	LuaScriptInstance *instance = memnew(LuaScriptInstance(p_this, Ref<LuaScript>(this)));
	p_this->set_script_instance(instance);

	//LOCK_LUA_SCRIPT
	this->instances.insert(p_this);

	return instance;
}

ScriptInstance *LuaScript::instance_create(Object *p_this) {
	ERR_FAIL_COND_V(!this->valid, nullptr);

	if (!is_tool() && !ScriptServer::is_scripting_enabled()) {
#ifdef TOOLS_ENABLED
		return placeholder_instance_create(p_this);
#else
		return nullptr;
#endif
	}

	ScriptInstance *instance = _instance_create(p_this);

	CALL_ERROR error;
	GD_CALL(*instance, "_init", nullptr, 0, error);
	return instance;
}

PlaceHolderScriptInstance *LuaScript::placeholder_instance_create(Object *p_this) {
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_V_MSG(get_instance_base_type() == StringName(), nullptr, "Can't create placeholder instance of a pure lua table.");

	PlaceHolderScriptInstance *instance = memnew(PlaceHolderScriptInstance(LuaScriptLanguage::get_singleton(), Ref<Script>(this), p_this));
	placeholders.insert(instance);
	update_exports();
	return instance;
#else
	return nullptr;
#endif
}

bool LuaScript::instance_has(const Object *p_this) const {
	//LOCK_LUA_SCRIPT
	bool found = this->instances.has((Object *)p_this);

	return found;
}

bool LuaScript::has_source_code() const {
	return !IS_EMPTY(this->source);
}

String LuaScript::get_source_code() const {
	return this->source;
}

void LuaScript::set_source_code(const String &p_code) {
	if (p_code != this->source) {
		this->source = p_code;
#ifdef TOOLS_ENABLED
		source_changed_cache = true;
#endif
	}
}

void LuaScript::_init_neasted_scripts(lua_State *L, int pos) {
	pos = lua_absindex(L, pos);

	// Set `__udata` field if not exists.
	if (!luatable_has(L, pos, K_LUA_UDATA)) {
		lua_pushstring(L, K_LUA_UDATA);
		LuaScriptLanguage::ref_godot_object(L, this);
		lua_rawset(L, pos);
	}

	String tag = "::";
	// Set `__udata` field for neasted classes
	lua_pushnil(L);
	while (lua_next(L, pos)) {
		const char *sub_path;
		if (luatable_rawget(L, -1, "__name", sub_path) && String(sub_path).find(tag) >= 0) {
			Ref<LuaScript> neasted;
			REF_INSTANTIATE(neasted);
			neasted->valid = true;
			neasted->_owner = this;
			neasted->neasted_path = lua_tostring(L, -2);
			neasted->_init_neasted_scripts(L, -1);
		}
		lua_pop(L, 1);
	}
}

Error LuaScript::reload(bool p_keep_state) {
	{
		//LOCK_LUA_SCRIPT
		bool has_instances = instances.size();
		ERR_FAIL_COND_V(!p_keep_state && has_instances, ERR_ALREADY_IN_USE);
	}

	this->valid = false;

	lua_State *L = LUA_STATE;
	Error ret;
	String source_code = get_source_code();
	if (source_code.length() > 0) {
		CharString source_str = source.utf8();
		ret = LuaScriptLanguage::compile_script(L, script_path, source_str.get_data(), source_str.length());
	} else {
		String remap_path = ResourceLoader::path_remap(script_path);
		ret = LuaScriptLanguage::compile_script(L, remap_path);
	}

	if (ret == OK) {
		this->valid = true;

		push_self(L);
		if (lua_istable(L, -1)) {
			_init_neasted_scripts(L, -1);
		}
		lua_pop(L, 1);
		//LuaScriptLanguage::finish_compiling(res_path);
		auto res_path = get_path();
		if (!IS_EMPTY(res_path)) {
			LuaScriptLanguage::get_singleton()->full_script_cache[res_path] = this;
		}
	}

	return OK;
}

bool LuaScript::has_method(const StringName &p_method) const {
	bool ret = false;
	lua_State *L = LUA_STATE;
	ret = _push_script_member(L, p_method, MemberFlags::FUNCTION);
	if (ret) {
		ret = lua_isfunction(L, -1);
		lua_pop(L, 1);
	}
	if (!ret) {
		Ref<Script> base_script = get_base_script();
		if (base_script.is_valid()) {
			ret = base_script->has_method(p_method);
		}
	}

	return ret;
}

MethodInfo LuaScript::get_method_info(const StringName &p_method) const {
	MethodInfo mi;
	lua_State *L = LUA_STATE;
	if (_push_script_member(L, p_method, MemberFlags::FUNCTION)) {
		if (lua_isfunction(L, -1)) {
			mi.name = p_method;
#ifdef GODOT_3_X
			mi.flags = METHOD_FLAG_NORMAL | METHOD_FLAG_FROM_SCRIPT;
#else
			mi.flags = METHOD_FLAG_NORMAL;
#endif
			mi.return_val = PropertyInfo();

			int i = 1;
			while (true) {
				const char *arg_name = lua_getlocal(L, nullptr, i++);
				if (arg_name == nullptr)
					break;

				PropertyInfo p;
				p.name = arg_name;
				p.type = Variant::NIL;
				mi.arguments.push_back(p);
			}
		}
		lua_pop(L, 1);
	}

	if (mi.name.length() == 0) {
		Ref<Script> base_script = get_base_script();
		if (base_script.is_valid()) {
			mi = base_script->get_method_info(p_method);
		}
	}

	return mi;
}

ScriptLanguage *LuaScript::get_language() const {
	return LuaScriptLanguage::get_singleton();
}

bool LuaScript::has_script_signal(const StringName &p_signal) const {
	auto has = false;
	lua_State *L = LUA_STATE;
	auto top = lua_gettop(L);
	push_self(L);
	if (lua_istable(L, -1)) {
		lua_pushstring(L, K_LUA_SIGNAL);
		lua_rawget(L, -2);
		if (lua_istable(L, -1)) {
			lua_pushstring(L, GD_STR_NAME_DATA(p_signal));
			lua_rawget(L, -2);
			has = !lua_isnil(L, -1);
		}
	}
	lua_settop(L, top);

	if (!has) {
		Ref<Script> base_script = get_base_script();
		if (base_script.is_valid()) {
			has = base_script->has_script_signal(p_signal);
		}
	}
	return has;
}

void LuaScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
	Ref<Script> base_script = get_base_script();
	if (base_script.is_valid()) {
		base_script->get_script_signal_list(r_signals);
	}

	lua_State *L = LUA_STATE;
	auto top = lua_gettop(L);
	push_self(L);
	if (lua_istable(L, -1)) {
		lua_pushstring(L, K_LUA_SIGNAL);
		lua_rawget(L, -2);

		if (lua_istable(L, -1)) {
			lua_pushnil(L);
			while (lua_next(L, -2)) {
				MethodInfo mi;
				mi.name = lua_tostring(L, -2);
#ifdef GODOT_3_X
				mi.flags = METHOD_FLAG_FROM_SCRIPT;
#endif
				lua_pushnil(L);
				while (lua_next(L, -2)) {
					mi.arguments.push_back(PropertyInfo(Variant::NIL, lua_tostring(L, -1)));
					lua_pop(L, 1);
				}
				r_signals->push_back(mi);

				lua_pop(L, 1);
			}
		}
	}
	lua_settop(L, top);
}

bool LuaScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
	bool ret = false;
#ifdef TOOLS_ENABLED
	lua_State *L = LUA_STATE;
	ret = _push_script_member(L, p_property, MemberFlags::PROPERTY);
	if (ret) {
		r_value = lua_to_godot_variant(L, -1);
		lua_pop(L, 1);
	} else {
		Ref<Script> base_script = get_base_script();
		if (base_script.is_valid()) {
			ret = base_script->get_property_default_value(p_property, r_value);
		}
	}
#endif

	return ret;
}

void LuaScript::update_exports() {
#ifdef TOOLS_ENABLED
	List<PropertyInfo> properties;
	get_script_property_list(&properties);
	Map<StringName, Variant> values;
	get_script_property_map(properties, values);
#ifdef GODOT_3_X
	for (auto E = placeholders.front(); E; E = E->next()) {
		PlaceHolderScriptInstance *placeholder = E->get();
		placeholder->update(properties, values);
	}
#else
	for (PlaceHolderScriptInstance *E : placeholders) {
		E->update(properties, values);
	}
#endif
#endif
}

void LuaScript::get_script_method_list(List<MethodInfo> *p_list) const {
	Ref<Script> base_script = get_base_script();
	if (base_script.is_valid()) {
		base_script->get_script_method_list(p_list);
	}

	lua_State *L = LUA_STATE;
	LuaScriptLanguage::push_luascript_api(L, "get_script_method_list");
	push_self(L);
	if (godot_lua_xpcall(L, 1, 1) != LUA_OK)
		return;

	lua_pushnil(L);
	while (lua_next(L, -2)) {
		MethodInfo mi;

		lua_getfield(L, -1, "name");
		mi.name = lua_tostring(L, -1);
		lua_pop(L, 1);

		int argc = luaL_len(L, -1);
		for (int i = 1; i <= argc; i++) {
			PropertyInfo prop;
			lua_pushinteger(L, i);
			lua_gettable(L, -2);

			lua_pushinteger(L, 1);
			lua_gettable(L, -2);
			prop.name = lua_tostring(L, -1);
			lua_pop(L, 1);

			lua_pushinteger(L, 1);
			lua_gettable(L, -2);
			prop.type = Variant::Type(lua_tointeger(L, -1));
			lua_pop(L, 1);

			mi.arguments.push_back(prop);

			lua_pop(L, 1);
		}

#ifdef GODOT_3_X
		mi.flags = METHOD_FLAG_NORMAL | METHOD_FLAG_FROM_SCRIPT;
#else
		mi.flags = METHOD_FLAG_NORMAL;
#endif
		p_list->push_back(mi);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

void LuaScript::_get_script_property_list(const ScriptInstance *instance, List<PropertyInfo> *p_list) const {
	Ref<Script> base_script = get_base_script();
	if (base_script.is_valid()) {
		if (auto lua_script = Object::cast_to<LuaScript>(base_script.ptr())) {
			lua_script->_get_script_property_list(instance, p_list);
		} else {
			base_script->get_script_property_list(p_list);
		}
	}

	lua_State *L = LUA_STATE;
	LuaScriptLanguage::push_luascript_api(L, "get_script_property_list");
	push_self(L);
	if (instance) {
		static_cast<const LuaScriptInstance *>(instance)->push_self(L);
	} else {
		lua_pushnil(L);
	}
	if (godot_lua_xpcall(L, 2, 1) != LUA_OK)
		return;

	lua_pushnil(L);
	while (lua_next(L, -2)) {
		PropertyInfo prop;

		lua_getfield(L, -1, "name");
		prop.name = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "type");
		prop.type = Variant::Type(lua_tointeger(L, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "usage");
		prop.usage = lua_tointeger(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "hint");
		prop.hint = PropertyHint(lua_tointeger(L, -1));
		lua_pop(L, 1);

		lua_getfield(L, -1, "hint_string");
		prop.hint_string = lua_tostring(L, -1);
		lua_pop(L, 1);

		p_list->push_back(prop);
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

void LuaScript::get_script_property_list(List<PropertyInfo> *p_list) const {
	_get_script_property_list(nullptr, p_list);
}

int LuaScript::get_member_line(const StringName &p_member) const {
	int line = -1;
	GD_STR_HOLD(mfield, p_member);

	lua_State *L = LUA_STATE;
	push_self(L);
	if (lua_istable(L, -1)) {
		lua_pushstring(L, mfield);
		lua_rawget(L, -2);

		if (lua_isfunction(L, -1)) {
			lua_Debug inf;
			lua_getinfo(L, ">S", &inf);
			line = inf.linedefined;
		} else {
			lua_pop(L, 1);
		}
	}

	lua_pop(L, 1);
	return line;
}

void LuaScript::get_constants(Map<StringName, Variant> *p_constants) {
}

void LuaScript::get_members(Set<StringName> *p_constants) {
	List<PropertyInfo> p_propertylist;
	get_script_property_list(&p_propertylist);
	for (auto E = p_propertylist.front(); E; E = E->next()) {
		p_constants->insert(E->get().name);
	}

	List<MethodInfo> p_methodlist;
	get_script_method_list(&p_methodlist);
	for (auto E = p_methodlist.front(); E; E = E->next()) {
		p_constants->insert(E->get().name);
	}
}

#ifdef TOOLS_ENABLED
bool LuaScript::is_placeholder_fallback_enabled() const {
	return this->placeholder_fallback_enabled;
}
#endif

Error LuaScript::load_source_code(const String &p_path) {
	Error error;

	auto file = FileAccess::open(p_path, FileAccess::READ, &error);
	if (error) {
		ELog("load_source_code: open %s fail: %d", p_path, error);
		return error;
	}

#ifdef GODOT_3_X
	PoolVector<uint8_t> buffer;
	const int len = (int)file->get_len();
	buffer.resize(len + 1);
	auto w = buffer.write().ptr();
	int r = file->get_buffer(w, len);
	file->close();
	memdelete(file);
#else
	Vector<uint8_t> buffer;
	const uint64_t len = file->get_length();
	buffer.resize(len + 1);
	uint8_t *w = buffer.ptrw();
	uint64_t r = file->get_buffer(w, len);
#endif

	ERR_FAIL_COND_V(r != len, ERR_CANT_OPEN);

	w[len] = 0;

	String source;

	if (source.parse_utf8((const char *)w)) {
		ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Script '" + p_path + "' contains invalid unicode (utf-8), so it was not loaded. Please ensure that scripts are saved in valid utf-8 unicode.");
	}

	this->set_source_code(source);

	return OK;
}

bool LuaScript::editor_can_reload_from_file() {
#ifdef TOOLS_ENABLED
	if (LuaScriptLanguage::get_singleton()->overrides_external_editor()) {
		if (instances.size() == 0 && placeholders.size() == 0) {
			return true;
		}
	}
#endif
	return false;
}

void LuaScript::_bind_methods() {
	ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "new", &LuaScript::_new, MethodInfo(Variant::OBJECT, "new"));
}

bool LuaScript::_set(const StringName &p_name, const Variant &p_property) {
	if (p_name == SCRIPT_SOURCE) {
		set_source_code(p_property);
		reload();
	} else {
		return false;
	}

	return true;
}

bool LuaScript::_get(const StringName &p_name, Variant &r_property) const {
	if (p_name == SCRIPT_SOURCE) {
		r_property = get_source_code();
		return true;
	}

	auto L = LUA_STATE;
	bool r_valid = _push_script_member(L, p_name, MemberFlags::PROPERTY);
	if (r_valid) {
		r_property = lua_to_godot_variant(L, -1);
		lua_pop(L, 1);
	} else {
		auto base_script = get_base_script();
		if (base_script.is_valid()) {
			r_property = base_script->get(p_name, &r_valid);
		}
	}
	return r_valid;
}

void LuaScript::_get_property_list(List<PropertyInfo> *p_list) const {
#ifdef GODOT_3_X
	p_list->push_back(PropertyInfo(Variant::STRING, SCRIPT_SOURCE, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL));
#else
	p_list->push_back(PropertyInfo(Variant::STRING, SCRIPT_SOURCE, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));
#endif
}

bool LuaScript::_push_script_member(lua_State *L, const StringName &p_name, MemberFlags flags) const {
	bool r_valid = false;
	GD_STR_HOLD(field, p_name);
	LuaScriptLanguage::push_luascript_api(L, "get_script_member");
	push_self(L);
	lua_pushstring(L, field);
	lua_pushinteger(L, flags);
	if (godot_lua_xpcall(L, 3, 2) == LUA_OK) {
		r_valid = lua_toboolean(L, -2);
		if (r_valid) {
			lua_remove(L, -2);
		} else {
			lua_pop(L, 2);
		}
	}

	if (!r_valid && (flags & MemberFlags::PROPERTY) && strcmp(field, "super") == 0) {
		auto base_script = get_base_script();
		if (!base_script.is_null()) {
			auto luascript = Object::cast_to<LuaScript>(base_script.ptr());
			if (luascript) {
				luascript->push_self(L);
				return true;
			}
		}
		LuaScriptLanguage::import_godot_object(L, get_instance_base_type());
		return true;
	}
	return r_valid;
}

#ifdef GODOT_3_X
Variant LuaScript::call(const StringName &p_method, const Variant **p_args, int p_argcount, CALL_ERROR &r_error) {
#else
Variant LuaScript::callp(const StringName &p_method, const Variant **p_args, int p_argcount, CALL_ERROR &r_error) {
#endif
	r_error.error = CALL_ERROR::CALL_OK;

	lua_State *L = LUA_STATE;
	Variant var;
	int top = lua_gettop(L);
	if (!push_method(L, GD_STR_NAME_DATA(p_method))) {
		r_error.error = CALL_ERROR::CALL_ERROR_INVALID_METHOD;
		goto EXIT;
	}

	for (int i = 0; i < p_argcount; i++) {
		LuaScriptLanguage::push_variant(L, p_args[i]);
	}

	if (godot_lua_xpcall(L, p_argcount, 1) != LUA_OK) {
		r_error.error = CALL_ERROR::CALL_ERROR_INVALID_ARGUMENT;
		goto EXIT;
	}

	var = lua_to_godot_variant(L, -1);
EXIT:
	lua_settop(L, top);

	if (r_error.error != CALL_ERROR::CALL_OK) {
#ifdef GODOT_3_X
		var = Script::call(p_method, p_args, p_argcount, r_error);
#else
		var = Script::callp(p_method, p_args, p_argcount, r_error);
#endif
	}
	return var;
}

#ifdef TOOLS_ENABLED
void LuaScript::_placeholder_erased(PlaceHolderScriptInstance *p_placeholder) {
	this->placeholders.erase(p_placeholder);
}
#endif

Variant LuaScript::_new(const Variant **p_args, int p_argcount, CALL_ERROR &r_error) {
	if (!valid) {
		r_error.error = CALL_ERROR::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}

	r_error.error = CALL_ERROR::CALL_OK;
	REF ref;
	Object *owner = nullptr;

	StringName native_class = get_instance_base_type();
	if (((String)native_class).length() > 0) {
		owner = CLS_INSTANTIATE(native_class);
	}

	ERR_FAIL_COND_V_MSG(!owner, Variant(), "Can't create instance of a pure lua table.");

	Reference *r = Object::cast_to<Reference>(owner);
	if (r) {
		ref = REF(r);
	}

	ScriptInstance *instance = _instance_create(owner);
	if (!instance) {
		if (ref.is_null()) {
			memdelete(owner); //no owner, sorry
		}
		return Variant();
	}

	CALL_ERROR error;
	GD_CALL(*instance, "_init", p_args, p_argcount, error);
	if (ref.is_valid()) {
		return ref;
	} else {
		return owner;
	}
}
