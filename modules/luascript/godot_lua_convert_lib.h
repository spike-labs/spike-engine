/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#pragma once

#include "godot_lua_table_api.h"
#include "lib/lua/lua.hpp"
#include "luascript.h"

_FORCE_INLINE_ void lua_push_godot_vector2(lua_State *L, const Variant &var) {
	auto v = var.operator Vector2();
	lua_createtable(L, 0, 2);
	luatable_rawset(L, -1, "x", v.x);
	luatable_rawset(L, -1, "y", v.y);
	lua_getglobal(L, "Vector2");
	gdlua_setmetatable(L, -2);
}

_FORCE_INLINE_ void lua_push_godot_rect2(lua_State *L, const Variant &var) {
	auto v = var.operator Rect2();
	lua_createtable(L, 0, 2);
	lua_push_godot_vector2(L, v.position);
	lua_setfield(L, -2, "position");
	lua_push_godot_vector2(L, v.size);
	lua_setfield(L, -2, "size");
	lua_getglobal(L, "Rect2");
	gdlua_setmetatable(L, -2);
}

_FORCE_INLINE_ void lua_push_godot_vector3(lua_State *L, const Variant &var) {
	auto v = var.operator Vector3();
	lua_createtable(L, 0, 3);
	luatable_rawset(L, -1, "x", v.x);
	luatable_rawset(L, -1, "y", v.y);
	luatable_rawset(L, -1, "z", v.z);
	lua_getglobal(L, "Vector3");
	gdlua_setmetatable(L, -2);
}

_FORCE_INLINE_ void lua_push_godot_transform2d(lua_State *L, const Transform2D &v) {
	lua_createtable(L, 0, 3);
	lua_push_godot_vector2(L, v[0]);
	lua_setfield(L, -2, "x");
	lua_push_godot_vector2(L, v[1]);
	lua_setfield(L, -2, "y");
	lua_push_godot_vector2(L, v[2]);
	lua_setfield(L, -2, "origin");
	lua_getglobal(L, "Transform2D");
	gdlua_setmetatable(L, -2);
}

_FORCE_INLINE_ void lua_push_godot_plane(lua_State *L, const Plane &v) {
	lua_createtable(L, 0, 2);
	lua_push_godot_vector3(L, v.normal);
	lua_setfield(L, -2, "normal");
	luatable_rawset(L, -1, "d", v.d);
	lua_getglobal(L, "Plane");
	gdlua_setmetatable(L, -2);
}

_FORCE_INLINE_ void lua_push_godot_quat(lua_State *L, const Quat &v) {
	lua_createtable(L, 0, 4);
	luatable_rawset(L, -1, "x", v.x);
	luatable_rawset(L, -1, "y", v.y);
	luatable_rawset(L, -1, "z", v.z);
	luatable_rawset(L, -1, "w", v.w);
	lua_getglobal(L, "Quat");
	gdlua_setmetatable(L, -2);
}

_FORCE_INLINE_ void lua_push_godot_aabb(lua_State *L, const AABB &v) {
	lua_createtable(L, 0, 2);
	lua_push_godot_vector3(L, v.position);
	lua_setfield(L, -2, "position");
	lua_push_godot_vector3(L, v.size);
	lua_setfield(L, -2, "size");
	lua_getglobal(L, "AABB");
	gdlua_setmetatable(L, -2);
}

_FORCE_INLINE_ void lua_push_godot_basis(lua_State *L, const Basis &v) {
	lua_createtable(L, 0, 3);
	lua_push_godot_vector3(L, v[0]);
	lua_setfield(L, -2, "x");
	lua_push_godot_vector3(L, v[1]);
	lua_setfield(L, -2, "y");
	lua_push_godot_vector3(L, v[2]);
	lua_setfield(L, -2, "z");
	lua_getglobal(L, "Basis");
	gdlua_setmetatable(L, -2);
}

_FORCE_INLINE_ void lua_push_godot_transform3d(lua_State *L, const Transform &v) {
	lua_createtable(L, 0, 2);
	lua_push_godot_basis(L, v.basis);
	lua_setfield(L, -2, "basis");
	lua_push_godot_vector3(L, v.origin);
	lua_setfield(L, -2, "origin");
	lua_getglobal(L, "Transform");
	gdlua_setmetatable(L, -2);
}

_FORCE_INLINE_ void lua_push_godot_color(lua_State *L, const Color &v) {
	lua_createtable(L, 0, 4);
	lua_pushnumber(L, v[0]);
	lua_setfield(L, -2, "r");
	lua_pushnumber(L, v[1]);
	lua_setfield(L, -2, "g");
	lua_pushnumber(L, v[2]);
	lua_setfield(L, -2, "b");
	lua_pushnumber(L, v[3]);
	lua_setfield(L, -2, "a");
	lua_getglobal(L, "Color");
	gdlua_setmetatable(L, -2);
}

_FORCE_INLINE_ Variant convert_to_vector2(lua_State *L, int pos) {
	lua_Number x, y;
	luatable_get(L, pos, "x", x);
	luatable_get(L, pos, "y", y);
	return Variant(Vector2(x, y));
}

_FORCE_INLINE_ Variant convert_to_rect2(lua_State *L, int pos) {
	lua_Number x, y, w, h;
	lua_getfield(L, pos, "position");
	luatable_get(L, -1, "x", x);
	luatable_get(L, -1, "y", y);
	lua_pop(L, 1);

	lua_getfield(L, pos, "size");
	luatable_get(L, -1, "x", w);
	luatable_get(L, -1, "y", h);
	lua_pop(L, 1);

	return Variant(Rect2(x, y, w, h));
}

_FORCE_INLINE_ Variant convert_to_vector3(lua_State *L, int pos) {
	lua_Number x, y, z;
	luatable_get(L, pos, "x", x);
	luatable_get(L, pos, "y", y);
	luatable_get(L, pos, "z", z);
	return Variant(Vector3(x, y, z));
}