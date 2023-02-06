/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include "luascript_instance.h"
#include "constants.h"
#include "godot_lua_convert_api.h"
#include "luascript.h"
#include "luascript_language.h"
#include "scene/main/node.h"
#include "scene/scene_string_names.h"

LuaScriptInstance::LuaScriptInstance(Object *p_owner, Ref<LuaScript> p_script) :
		owner{ p_owner },
		script{ p_script },
		_property_locked(false) {
	lua_State *L = LUA_STATE;

	lua_newtable(L); // local Instance = {}
	lua_pushstring(L, K_LUA_UDATA);
	LuaScriptLanguage::push_godot_object(L, p_owner); // local udata = p_owner
	lua_rawset(L, -3); // Instance[K_LUA_UDATA] = udata

	p_script->push_self(L); // local LuaClass = <Lua Class Meta>
	gdlua_setmetatable(L, -2); // setmetatable(Instance, LuaClass)
	ref = luaL_ref(L, LUA_REGISTRYINDEX);
}

LuaScriptInstance::~LuaScriptInstance() {
	LOCK_LUA_SCRIPT
	if (script.is_valid() && owner) {
		script->instances.erase(owner);
	}

	lua_State *L = LUA_STATE;
	luaL_unref(L, LUA_REGISTRYINDEX, ref);
	ref = 0;
}

bool LuaScriptInstance::_lua_set(lua_State *L, const StringName &p_name, const Variant &p_value) {
	bool ret = false;
	GD_STR_HOLD(field, p_name);
	LuaScriptLanguage::push_luascript_api(L, "set_instance_property");
	push_self(L);
	lua_pushstring(L, field);
	LuaScriptLanguage::push_variant(L, &p_value);
	if (godot_lua_xpcall(L, 3, 1) == LUA_OK) {
		ret = lua_toboolean(L, -1);
		lua_pop(L, 1);
	}
	return ret;
}

bool LuaScriptInstance::_lua_get(lua_State *L, const StringName &p_name, Variant &r_ret) const {
	bool exists = _push_instance_member(L, GD_UTF8_NAME(p_name), MemberFlags::PROPERTY);
	if (exists) {
		r_ret = lua_to_godot_variant(L, -1);
		lua_pop(L, 1);
	}
	return exists;
}

bool LuaScriptInstance::set(const StringName &p_name, const Variant &p_value) {
	if (_property_locked)
		return false;
	return _lua_set(LUA_STATE, p_name, p_value);
}

bool LuaScriptInstance::get(const StringName &p_name, Variant &r_ret) const {
	if (_property_locked)
		return false;
	return _lua_get(LUA_STATE, p_name, r_ret);
}

void LuaScriptInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	Ref<LuaScript> script = get_script();
	if (script.is_valid()) {
		script->_get_script_property_list(this, p_properties);
	}
}

Variant::Type LuaScriptInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	Variant ret;
	if (_lua_get(LUA_STATE, p_name, ret)) {
		return ret.get_type();
	}
	return Variant::Type();
}

Object *LuaScriptInstance::get_owner() {
	return this->owner;
}

void LuaScriptInstance::get_method_list(List<MethodInfo> *p_list) const {
	Ref<LuaScript> script = get_script();
	if (script.is_valid()) {
		script->get_method_list(p_list);
	}
}

bool LuaScriptInstance::has_method(const StringName &p_method) const {
	Ref<LuaScript> script = get_script();
	if (script.is_valid()) {
		return script->has_method(p_method);
	}
	return false;
}

#ifdef GODOT_3_X
Variant LuaScriptInstance::call(const StringName &p_method, const Variant **p_args, int p_argcount, CALL_ERROR &r_error) {
#else
bool LuaScriptInstance::property_can_revert(const StringName &p_name) const {
	return false;
}
bool LuaScriptInstance::property_get_revert(const StringName &p_name, Variant &r_ret) const {
	return false;
}
Variant LuaScriptInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, CALL_ERROR &r_error) {
#endif
	r_error.error = CALL_ERROR::CALL_OK;

	if (!script.is_valid()) {
		r_error.error = CALL_ERROR::CALL_ERROR_INSTANCE_IS_NULL;
		ERR_FAIL_V(Variant());
	}

	lua_State *L = LUA_STATE;
	int top = lua_gettop(L);

	Node *nd = Object::cast_to<Node>(owner);
	if (nd != nullptr && p_method == SceneStringNames::get_singleton()->_ready) {
		// Convert NodePath to Node
		List<PropertyInfo> props;
		get_property_list(&props);
		for (auto E = props.front(); E; E = E->next()) {
			auto info = E->get();
			if (info.type == Variant::NODE_PATH) {
				Variant value;
				if (_lua_get(L, info.name, value)) {
					Node *child = nd->get_node_or_null(value.operator NodePath());
					if (child != nullptr) {
						_lua_set(L, info.name, Variant(child));
					}
				}
			}
		}
	}

	lua_settop(L, top);
	Variant var;

	if (!push_method(L, GD_STR_NAME_DATA(p_method))) {
		r_error.error = CALL_ERROR::CALL_ERROR_INVALID_METHOD;
		goto EXIT;
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, ref); // push self
	for (int i = 0; i < p_argcount; i++) {
		LuaScriptLanguage::push_variant(L, p_args[i]);
	}

	if (godot_lua_xpcall(L, 1 + p_argcount, 1) != LUA_OK) {
		r_error.error = CALL_ERROR::CALL_ERROR_INVALID_ARGUMENT;
		goto EXIT;
	}

	var = lua_to_godot_variant(L, -1);
EXIT:
	lua_settop(L, top);

	return var;
}

void LuaScriptInstance::call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount) {
	CALL_ERROR error;
	GD_CALL(*this, p_method, p_args, p_argcount, error);
}

void LuaScriptInstance::call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount) {
	CALL_ERROR error;
	GD_CALL(*this, p_method, p_args, p_argcount, error);
}

void LuaScriptInstance::notification(int p_notification) {
	const StringName method = StringName("_notification");
	if (this->has_method(method)) {
		Variant value = p_notification;
		const Variant *args[1] = { &value };
		CALL_ERROR err;
		GD_CALL(*this, method, args, 1, err);
	}
}

String LuaScriptInstance::to_string(bool *r_valid) {
	if (r_valid)
		*r_valid = false;
	return String();
}

void LuaScriptInstance::refcount_incremented() {
}

bool LuaScriptInstance::refcount_decremented() {
	return true;
}

Ref<Script> LuaScriptInstance::get_script() const {
	return this->script;
}

ScriptLanguage *LuaScriptInstance::get_language() {
	return LuaScriptLanguage::get_singleton();
}
