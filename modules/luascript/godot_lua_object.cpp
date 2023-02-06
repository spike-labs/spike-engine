/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include "constants.h"
#include "core/core_string_names.h"
#include "godot_lua_convert_api.h"
#include "lib/lua/lua.hpp"
#include "luascript_instance.h"
#include "luascript_language.h"
#include "scene/main/node.h"

/**
 * @brief Find member from base class recursively
 */
static void _push_value_from_metatable(lua_State *L) {
	for (;;) { // ...|t|k
		int has_meta = lua_getmetatable(L, -2); // ...|t|k|mt?
		if (has_meta) {
			lua_pushvalue(L, -2); // ...|t|k|mt|k
			lua_rawget(L, -2); // ...|t|k|mt|v
			if (lua_isnil(L, -1)) {
				lua_pop(L, 1); // ...|t|k|mt
				lua_remove(L, -3); // ...|k|mt
				lua_insert(L, -2); // ...|mt|k
			} else {
				lua_insert(L, -4); // ...|v|t|k|mt
				lua_pop(L, 3); // ...|v
				break;
			}
		} else {
			lua_pop(L, 2); // ...
			lua_pushnil(L); // ...|nil
			break;
		}
	}
}

/**
 * @brief Create Godot Object
 */
int lua_godot_object_new(lua_State *L) {
	Object *gd = lua_to_godot_object(L, 1);
	if (gd == nullptr) {
		if (lua_istable(L, 1)) {
			lua_getfield(L, 1, "__name");
			auto class_name = StringName(lua_tostring(L, -1));
			lua_pop(L, 1);
			LuaScriptLanguage::push_godot_object(L, CLS_INSTANTIATE(class_name));
		} else {
			lua_pushnil(L);
		}
	} else {
		if (Resource *res = Object::cast_to<Resource>(gd)) {
			LUA_TO_ARGS(p_args, argc, 2)
			CALL_ERROR error;
			Variant ret = GD_CALL(*res, "new", p_args, argc, error);
			LuaScriptLanguage::push_variant(L, &ret);
		} else {
			lua_pushnil(L);
		}
	}

	return 1;
}

/**
 * @brief Index field from metatables
 */
static bool push_from_metatable(lua_State *L, const StringName &class_name, const char *p_method) {
	int top = lua_gettop(L);
	bool found = false;
	if (LuaScriptLanguage::import_godot_object(L, class_name)) {
		lua_pushstring(L, p_method);
		lua_rawget(L, -2);
		found = !lua_isnil(L, -1);
		if (!found) {
			lua_pop(L, 1);
			lua_pushstring(L, p_method);
			_push_value_from_metatable(L);
			found = !lua_isnil(L, -1);
		} else {
			lua_remove(L, -2);
		}
	}
	if (!found)
		lua_settop(L, top);
	return found;
}

/**
 * @brief Meta method `__index` for Godot Object
 */
int lua_godot_object_indexer(lua_State *L) {
	const char *key = lua_tostring(L, 2);
	if (auto gd = lua_to_godot_object(L, 1)) {
		auto class_name = gd->get_class_name();
		auto method = StringName(key);

		auto luascript = Object::cast_to<LuaScript>(gd);
		if (luascript && luascript->push_member(L, key)) {
			return 1;
		}

		auto instance = LuaScriptInstance::get_script_instance(gd);
		if (instance && instance->push_member(L, key)) {
			return 1;
		}

		if (push_from_metatable(L, class_name, key))
			return 1;

		if (auto script = Object::cast_to<Script>(gd))
			if (script->has_method(method))
				return push_not_lua_method(L, key);

		if (gd->has_method(method))
			return push_not_lua_method(L, key);

		bool valid;
		Variant value = gd->get(method, &valid);
		if (valid) {
			LuaScriptLanguage::push_variant(L, &value);
			return 1;
		}

		auto node = Object::cast_to<Node>(gd);
		// Child node of this node(if it is a `Node`)
		if (node != nullptr && node->is_inside_tree()) {
			auto child = node->get_node_or_null(NodePath(key));
			if (child != nullptr) {
				LuaScriptLanguage::push_godot_object(L, child);
				return 1;
			}
		}
	} else {
		lua_pushstring(L, "__name");
		lua_rawget(L, 1);
		const char *class_name = luaL_optstring(L, -1, nullptr);
		lua_pop(L, 1);
		if (class_name) {
			if (push_from_metatable(L, class_name, key)) {
				return 1;
			}

			if (ClassDB::has_method(class_name, key)) {
				return push_not_lua_method(L, key);
			}
		}
	}

	lua_pushnil(L);

	return 1;
}

/**
 * @brief Meta method `__newindex` for Godot Object
 */
int lua_godot_object_newindexer(lua_State *L) {
	if (auto gd = lua_to_godot_object(L, 1)) {
		bool valid = false;
		auto instance = LuaScriptInstance::get_script_instance(gd);
		auto prop_lock = LuaPropertyLock(instance);
		if (instance) {
			// If it is a `LuaScriptInstance`, try to assign values directly through the script.
			LuaScriptLanguage::push_luascript_api(L, "set_instance_property");
			instance->push_self(L);
			lua_pushvalue(L, 2);
			lua_pushvalue(L, 3);
			if (godot_lua_xpcall(L, 3, 1) == LUA_OK) {
				valid = lua_toboolean(L, -1);
				lua_pop(L, 1);
			}
		}

		if (!valid) {
			const char *key = lua_tostring(L, 2);
			Variant value = lua_to_godot_variant(L, 3);
			gd->set(key, value, &valid);
		}

		if (instance) {
			if (!valid) {
				// If it is a `LuaScriptInstance`, allow assignment to non-existing key values
				instance->push_self(L);
				lua_pushvalue(L, 2);
				lua_pushvalue(L, 3);
				lua_rawset(L, -3);
				lua_pop(L, 1);
			}
		}
	} else {
		const char *key = lua_tostring(L, 2);
		WARN_PRINT(vformat("try to set value for key `%s` on null instance.", key));
	}
	return 0;
}

/**
 * @brief Meta method `__gc` for Godot Object
 */
int lua_godot_object_gc(lua_State *L) {
	int tt = lua_type(L, 1);
	switch (tt) {
		case LUA_TUSERDATA: {
			// Not yet exists such situation
		} break;
		case LUA_TTABLE: {
			Object *obj = lua_to_godot_object(L, 1);
#ifdef GODOT_3_X
			if (obj && ObjectDB::instance_validate(obj))
#else
			if (obj && obj->get_instance_id().is_valid())
#endif
			{
				auto ref = Object::cast_to<Reference>(obj);
				//if (ref) DLog("unref Reference: %s -> %d", ref, ref->reference_get_count());
				if (ref && ref->unreference()) {
					memdelete(ref);
				}
			}
			luatable_remove(L, 1, K_LUA_UDATA);
		} break;
		case LUA_TLIGHTUSERDATA: {
			// lightuserdata does not triggers __gc。
		} break;
	}
	return 0;
}

/**
 * @brief Meta method `__tostring` for Godot Object
 */
int lua_godot_object_tostring(lua_State *L) {
	Object *self = lua_to_godot_object(L, 1);
	String gdstr;
	const char *cstr = nullptr;
	if (self != nullptr) {
		gdstr = self->to_string();
	} else {
		lua_pushstring(L, "__name");
		lua_rawget(L, 1);
		if (!lua_isnil(L, -1)) {
			cstr = lua_tostring(L, -1);
		} else {
			gdstr = "[Object:null]";
		}
		lua_pop(L, 1);
	}

	if (gdstr.length() > 0) {
		lua_pushstring(L, GD_UTF8_STR(gdstr));
	} else if (cstr) {
		lua_pushfstring(L, "{%s}", cstr);
	} else {
		luaL_tolstring(L, 1, NULL);
	}

	return 1;
}

/**
 * @brief Meta method `__eq` for Godot Object
 */
int lua_godot_object_eq(lua_State *L) {
	bool a_valid, b_valid;
	Object *a = lua_to_godot_object(L, 1, &a_valid);
	Object *b = lua_to_godot_object(L, 2, &b_valid);
	lua_pushboolean(L, a_valid && b_valid && a == b);
	return 1;
}

/**
 * @brief Extends string methods of Godot: __call
 */
int godot_string_call(lua_State *L) {
	const char *method = lua_tostring(L, lua_upvalueindex(1));
	const char *str = lua_tostring(L, 1);
	LUA_TO_ARGS(p_args, argc, 2);
	CALL_ERROR error;
#ifdef GODOT_3_X
	Variant ret = Variant(str).call(method, p_args, argc, error);
#else
	Variant ret;
	Variant(str).callp(method, p_args, argc, ret, error);
#endif
	LuaScriptLanguage::push_variant(L, &ret);
	//print_call_info(str, method, p_args, argc, ret);
	return 1;
}

/**
 * @brief Extends string methods of Godot: __index
 */
int godot_string_indexer(lua_State *L) {
	const char *key = lua_tostring(L, 2);
#ifdef GODOT_3_X
	if (Variant::is_method_const(Variant::STRING, StringName(key))) {
#else
	if (Variant::is_builtin_method_const(Variant::STRING, StringName(key))) {
#endif
		lua_pushvalue(L, 2);
		lua_pushcclosure(L, &godot_string_call, 1);
		lua_pushvalue(L, -1);
		lua_setfield(L, 1, key);
		return 1;
	}
	return 0;
}

/**
 * @brief Godot Object method: `Object::emit_signal`
 */
int script_emit_signal(lua_State *L) {
	if (auto gd = lua_to_godot_object(L, 1)) {
		const char *key = lua_tostring(L, 2);
		LUA_TO_ARGS(p_args, n_arg, 3);
#ifdef GODOT_3_X
		auto ret = gd->emit_signal(key, p_args, n_arg);
#else
		auto ret = gd->emit_signalp(key, p_args, n_arg);
#endif
		lua_pushinteger(L, ret);
		return 1;
	}
	return 0;
}

int godot_object_is_instance(lua_State *L) {
	const char *class_name = nullptr;
	if (lua_isstring(L, 2)) {
		class_name = lua_tostring(L, 2);
	} else if (lua_istable(L, 2)) {
		lua_pushstring(L, "__name");
		lua_rawget(L, 2);
		class_name = luaL_optstring(L, -1, nullptr);
		lua_pop(L, 1);
	}

	if (class_name) {
		lua_pushvalue(L, 1);
		while (true) {
			if (luaL_getmetafield(L, -1, META_NATIVE_FIELD)) {
				lua_pop(L, 2);
				break;
			}

			if (luaL_getmetafield(L, -1, "__name")) {
				const char *name = luaL_optstring(L, -1, nullptr);
				lua_pop(L, 1);

				if (name == class_name) {
					lua_pop(L, 1);
					lua_pushboolean(L, true);
					return 1;
				}

				lua_getmetatable(L, -1);
				lua_remove(L, -2);
			}
		}

		auto gd = lua_to_godot_object(L, 1);
		if (gd && gd->is_class(class_name)) {
			lua_pushboolean(L, true);
			return 1;
		}
	}

	lua_pushboolean(L, false);
	return 1;
}
