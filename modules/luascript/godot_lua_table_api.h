/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#pragma once

#include "lib/lua/lua.hpp"

inline static void luatable_rawset(lua_State *L, int t, const char *n, lua_CFunction f) {
	if (lua_istable(L, t)) {
		int pos = lua_absindex(L, t);
		lua_pushstring(L, n);
		lua_pushcfunction(L, f);
		lua_rawset(L, pos);
	}
}

inline static void luatable_rawset(lua_State *L, int t, const char *n, lua_Number num) {
	if (lua_istable(L, t)) {
		int pos = lua_absindex(L, t);
		lua_pushstring(L, n);
		lua_pushnumber(L, num);
		lua_rawset(L, pos);
	}
}

inline static void luatable_rawset(lua_State *L, int t, const char *n, lua_Integer num) {
	if (lua_istable(L, t)) {
		int pos = lua_absindex(L, t);
		lua_pushstring(L, n);
		lua_pushinteger(L, num);
		lua_rawset(L, pos);
	}
}

inline static void luatable_rawset(lua_State *L, int t, const char *n, const char *s) {
	if (lua_istable(L, t)) {
		int pos = lua_absindex(L, t);
		lua_pushstring(L, n);
		lua_pushstring(L, s);
		lua_rawset(L, pos);
	}
}

inline static bool luatable_get(lua_State *L, int t, const char *n, lua_Number &value) {
	bool has = false;
	if (lua_istable(L, t)) {
		if (lua_getfield(L, t, n) == LUA_TNUMBER){
			value = lua_tonumber(L, -1);
			has = true;
		}
		lua_pop(L, 1);
	}
	return has;
}

inline static bool luatable_get(lua_State *L, int t, const char *n, const char *&value) {
	bool has = false;
	if (lua_istable(L, t)) {
		if (lua_getfield(L, t, n) == LUA_TSTRING){
			value = lua_tostring(L, -1);
			has = true;
		}
		lua_pop(L, 1);
	}
	return has;
}

// [-0,+0]
inline static bool luatable_rawget(lua_State *L, int t, const char *n, const char *&value) {
	bool has = false;
	value = nullptr;
	if (lua_istable(L, t)) {
		t = lua_absindex(L, t);
		lua_pushstring(L, n);
		lua_rawget(L, t);
		if (lua_isstring(L, -1)) {
			value = lua_tostring(L, -1);
			has = true;
		}
		lua_pop(L, 1);
	}
	return has;
}

inline static bool luatable_has(lua_State *L, int t, const char *n) {
	if (lua_istable(L, t)) {
		t = lua_absindex(L, t);
		lua_pushstring(L, n);
		bool has = lua_rawget(L, t) != LUA_TNIL;
		lua_pop(L, 1);
		return has;
	}
	return false;
}

inline static void luatable_remove(lua_State *L, int t, const char *n) {
	if (lua_istable(L, t)) {
		t = lua_absindex(L, t);
		lua_pushstring(L, n);
		lua_pushnil(L);
		lua_rawset(L, t);
	}
}