/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#pragma once

#include "luascript.h"
#include "luascript_language.h"

using MemberFlags = LuaScript::MemberFlags;

class LuaScriptInstance : public ScriptInstance {
	friend class LuaScript;
	friend class LuaScriptLanguage;
	friend class LuaPropertyLock;

private:
	LuaScriptInstance(Object *p_owner, Ref<LuaScript> p_script);

private:
	Object *owner;
	Ref<LuaScript> script;
	int ref;

	bool _property_locked;

protected:
	bool _lua_set(lua_State *L, const StringName &p_name, const Variant &p_value);
	bool _lua_get(lua_State *L, const StringName &p_name, Variant &r_ret) const;

public:
	virtual ~LuaScriptInstance();

	virtual bool set(const StringName &p_name, const Variant &p_value) override;
	virtual bool get(const StringName &p_name, Variant &r_ret) const override;
	virtual void get_property_list(List<PropertyInfo> *p_properties) const override;
	virtual Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const override;

	virtual Object *get_owner() override;

	virtual void get_method_list(List<MethodInfo> *p_list) const override;
	virtual bool has_method(const StringName &p_method) const override;
	virtual void call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount);
	virtual void call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount);
	virtual void notification(int p_notification) override;
	virtual String to_string(bool *r_valid) override;

	virtual void refcount_incremented() override;
	virtual bool refcount_decremented() override;

	virtual Ref<Script> get_script() const override;

#ifdef GODOT_3_X
	virtual Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, CALL_ERROR &r_error) override;
	virtual MultiplayerAPI::RPCMode get_rpc_mode(const StringName &p_method) const override {
		return MultiplayerAPI::RPC_MODE_DISABLED;
	}
	virtual MultiplayerAPI::RPCMode get_rset_mode(const StringName &p_variable) const override {
		return MultiplayerAPI::RPC_MODE_DISABLED;
	}
#else
	virtual bool property_can_revert(const StringName &p_name) const override;
	virtual bool property_get_revert(const StringName &p_name, Variant &r_ret) const override;
	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, CALL_ERROR &r_error) override;
#endif

	virtual ScriptLanguage *get_language() override;

	bool _push_instance_member(lua_State *L, const char *field, MemberFlags flags) const {
		bool exists = false;
		LuaScriptLanguage::push_luascript_api(L, "get_instance_member");
		push_self(L);
		lua_pushstring(L, field);
		lua_pushinteger(L, flags);
		if (godot_lua_xpcall(L, 3, 2) == LUA_OK) {
			exists = lua_toboolean(L, -2);
			if (exists) {
				lua_remove(L, -2);
			} else {
				lua_pop(L, 2);
			}
		}
		if (!exists && (flags & MemberFlags::PROPERTY) && strcmp(field, "super") == 0) {
			auto base_script = script->get_base_script();
			if (!base_script.is_null()) {
				auto luascript = Object::cast_to<LuaScript>(base_script.ptr());
				if (luascript) {
					luascript->push_self(L);
					return true;
				}
			}
			LuaScriptLanguage::import_godot_object(L, script->get_instance_base_type());
			return true;
		}
		return exists;
	}
	bool push_member(lua_State *L, const StringName &field) const {
		return _push_instance_member(L, GD_STR_NAME_DATA(field), MemberFlags::MEMBER);
	}

	void push_self(lua_State *L) const {
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
	}

	bool push_method(lua_State *L, const char *method) {
		return _push_instance_member(L, method, MemberFlags::FUNCTION);
	}

	static LuaScriptInstance *get_script_instance(Object *obj) {
		auto script_instance = obj ? obj->get_script_instance() : nullptr;
		if (script_instance && !script_instance->is_placeholder() && script_instance->get_script()->is_class_ptr(LuaScript::get_class_ptr_static())) {
			return static_cast<LuaScriptInstance *>(script_instance);
		}
		return nullptr;
	}
};

class LuaPropertyLock {
private:
	LuaScriptInstance *_lua_inst;

public:
	LuaPropertyLock(LuaScriptInstance *instance) {
		_lua_inst = instance;
		if (_lua_inst) {
			_lua_inst->_property_locked = true;
		}
	}
	~LuaPropertyLock() {
		if (_lua_inst) {
			_lua_inst->_property_locked = false;
			_lua_inst = nullptr;
		}
	}
};