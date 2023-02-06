/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#pragma once

#include "def_lua.h"

#ifdef GODOT_3_X
#include "core/engine.h"
#include "core/script_language.h"
#else
#include "core/config/engine.h"
#include "core/object/script_language.h"
#include "core/templates/rb_set.h"
#endif
#include "luascript_language.h"

class LuaScript : public Script {
	GDCLASS(LuaScript, Script)

	friend class LuaScriptInstance;
	friend class LuaScriptLanguage;

public:
	enum MemberFlags {
		PROPERTY = 1,
		FUNCTION = 2,
		MEMBER = 3,
	};

private:
	bool valid;

	SelfList<LuaScript> self;

	String source;

	RBSet<Object *> instances;

	String script_path;

	/**
	 * @brief Neasted's ownder
	 */
	LuaScript *_owner;

	/**
	 * @brief Neasted's name path
	 */
	String neasted_path;

#ifdef TOOLS_ENABLED
	bool source_changed_cache;
	bool placeholder_fallback_enabled;
	RBSet<PlaceHolderScriptInstance *> placeholders;
#endif

public:
	LuaScript();
	virtual ~LuaScript();

#ifdef GODOT_3_X
	virtual bool can_instance() const override;
#else
	virtual bool can_instantiate() const override;

#ifdef TOOLS_ENABLED
	virtual Vector<DocData::ClassDoc> get_documentation() const override {
		return Vector<DocData::ClassDoc>();
	}
#endif

	virtual const Variant get_rpc_config() const override {
		return Variant();
	}
#endif

	virtual Ref<Script> get_base_script() const override;
	virtual StringName get_global_name() const override;
	virtual bool inherits_script(const Ref<Script> &p_script) const override;

	virtual StringName get_instance_base_type() const override;
	virtual ScriptInstance *instance_create(Object *p_this) override;
	virtual PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) override;
	virtual bool instance_has(const Object *p_this) const override;

	virtual bool has_source_code() const override;
	virtual String get_source_code() const override;
	virtual void set_source_code(const String &p_code) override;
	virtual Error reload(bool p_keep_state = false) override;

	virtual bool has_method(const StringName &p_method) const override;
	virtual MethodInfo get_method_info(const StringName &p_method) const override;

	virtual bool is_tool() const override;

	virtual bool is_valid() const override {
		return valid;
	};

	virtual ScriptLanguage *get_language() const override;

	virtual bool has_script_signal(const StringName &p_signal) const override;
	virtual void get_script_signal_list(List<MethodInfo> *r_signals) const override;

	virtual bool get_property_default_value(const StringName &p_property, Variant &r_value) const override;

	virtual void update_exports() override;
	virtual void get_script_method_list(List<MethodInfo> *p_list) const override;
	virtual void get_script_property_list(List<PropertyInfo> *p_list) const override;

	virtual int get_member_line(const StringName &p_member) const override;

	virtual void get_constants(Map<StringName, Variant> *p_constants) override;
	virtual void get_members(Set<StringName> *p_constants) override;

	void get_script_property_map(const List<PropertyInfo> &properties, Map<StringName, Variant> &p_values) {
		for (auto E = properties.front(); E; E = E->next()) {
			const StringName prop_name = E->get().name;
			Variant value;
			if (get_property_default_value(prop_name, value)) {
				p_values.insert(prop_name, value);
			}
		}
	}
	void push_self(lua_State *L) const {
		if (_owner == nullptr) {
			LuaScriptLanguage::push_luascript(L, GD_STR_DATA(script_path));
		} else {
			_owner->push_self(L);
			if (lua_istable(L, -1)) {
				lua_pushstring(L, GD_UTF8_STR(neasted_path));
				lua_rawget(L, -2);
			} else {
				lua_pushnil(L);
			}
			lua_remove(L, -2);
		}
	}
	bool push_method(lua_State *L, const char *method) const {
		return _push_script_member(L, method, MemberFlags::FUNCTION);
	}
	bool push_member(lua_State *L, const StringName &p_name) const {
		return _push_script_member(L, p_name, MemberFlags::MEMBER);
	}
	bool push_member(lua_State *L, const char *p_name) const {
		return _push_script_member(L, p_name, MemberFlags::MEMBER);
	}

	const char *get_extends(lua_State *L) const {
		const char *extends = nullptr;
		push_self(L);
		if (lua_istable(L, -1)) {
			lua_pushstring(L, K_LUA_EXTENDS);
			lua_rawget(L, -2);
			extends = lua_tostring(L, -1);
			lua_pop(L, 1);
		}
		lua_pop(L, 1);
		return extends;
	}

	void set_script_path(const String &path) {
		script_path = path;
	}

#ifdef TOOLS_ENABLED
	virtual bool is_placeholder_fallback_enabled() const override;
#endif

	Error load_source_code(const String &p_path);

	Variant _new(const Variant **p_args, int p_argcount, CALL_ERROR &r_error);

	// Supports sorting based on inheritance; parent must came first // TODO
	bool operator()(const Ref<LuaScript> &a, const Ref<LuaScript> &b) const {
		return true;
	}

protected:
	virtual bool editor_can_reload_from_file() override;

	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_property);
	bool _get(const StringName &p_name, Variant &r_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _get_script_property_list(const ScriptInstance *instance, List<PropertyInfo> *p_list) const;
	bool _push_script_member(lua_State *L, const StringName &p_name, MemberFlags flags) const;
#ifdef GODOT_3_X
	virtual Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, CALL_ERROR &r_error) override;
#else
	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, CALL_ERROR &r_error) override;
#endif
	ScriptInstance *_instance_create(Object *p_this);
	void _init_neasted_scripts(lua_State *L, int pos);

private:
#ifdef TOOLS_ENABLED
	virtual void _placeholder_erased(PlaceHolderScriptInstance *p_placeholder) override;
#endif
};
