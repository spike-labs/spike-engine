/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#pragma once

#include "core/io/resource_loader.h"
#include "godot_lua_table_api.h"
#include "lib/lua/lua.hpp"
#include "luascript_language.h"

#define PROTECT_CYCLE_REF
#define TABLE_2_CONTAINER_FLAG LuaScriptLanguage::get_object()

#define TO_USERDATA(T, i) static_cast<T *>(lua_touserdata(L, i))

#define print_call_info(self, method, p_args, argc, ret)                  \
	do {                                                                  \
		String args_str = String("");                                     \
		for (int i = 0; i < argc; i++) {                                  \
			args_str = args_str + p_args[i]->operator String();           \
			if (i < argc)                                                 \
				args_str = args_str + ",";                                \
		}                                                                 \
		DLog("godot_call: %s.%s(%s) => %s", self, method, args_str, ret); \
	} while (0)

inline static const char *_begin_cycle_check(lua_State *L, int t) {
#ifdef PROTECT_CYCLE_REF
	lua_rawgetp(L, t, TABLE_2_CONTAINER_FLAG);
	if (!lua_isnil(L, -1)) {
		const char *cycle = luaL_tolstring(L, -1, nullptr);
		lua_pop(L, 2);
		return cycle;
	}
	lua_pop(L, 1);

	lua_pushvalue(L, t);
	lua_rawsetp(L, t, TABLE_2_CONTAINER_FLAG);
#endif
	return nullptr;
}

inline static void _end_cycle_check(lua_State *L, int t) {
#ifdef PROTECT_CYCLE_REF
	lua_pushnil(L);
	lua_rawsetp(L, t, TABLE_2_CONTAINER_FLAG);
#endif
}

Object *lua_to_godot_object(lua_State *L, int pos, bool *valid = nullptr);

Variant lua_to_godot_variant(lua_State *L, int pos);

Variant lua_to_godot_array(lua_State *L, int t);

Vector<Variant> lua_to_godot_variant_vector(lua_State *L, int t);

Variant lua_to_godot_dictionary(lua_State *L, int t);

int godot_typeof(lua_State *L, int pos);

bool lua_check_godot_variant(lua_State *L, int pos, Variant::Type type, Variant &value);

void lua_push_godot_array(lua_State *L, const Array &a);

void lua_push_godot_dictionary(lua_State *L, const Dictionary &d);
