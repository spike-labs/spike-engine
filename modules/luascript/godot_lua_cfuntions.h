#pragma once

#include "lib/lua/lua.hpp"

int godot_lua_atpanic(lua_State *L);
int godot_lua_print(lua_State *L);
int godot_lua_printerr(lua_State *L);
int godot_lua_funcref(lua_State *L);
int godot_lua_loader(lua_State *L);
int godot_lua_seacher(lua_State *L);
int godot_lua_push_error(lua_State *L);
void godot_lua_hook(lua_State *L, lua_Debug *ar);
int godot_global_indexer(lua_State *L);
int godot_lua_topointer(lua_State *L);
int godot_global_typeof(lua_State *L);
int godot_global_type_exists(lua_State *L);
int godot_global_parse_json(lua_State *L);
int godot_global_to_json(lua_State *L);
int lua_godot_variant_create(lua_State *L);

int lua_godot_variant_call(lua_State *L);
int lua_godot_variant_indexer(lua_State *L);
int lua_godot_variant_newindexer(lua_State *L);
int lua_godot_variant_tostring(lua_State *L);
int lua_godot_variant_gc(lua_State *L);
int lua_godot_variant_eq(lua_State *L);
int lua_godot_variant_add(lua_State *L);
int lua_godot_variant_sub(lua_State *L);
int lua_godot_variant_mul(lua_State *L);
int lua_godot_variant_div(lua_State *L);
int lua_godot_variant_mod(lua_State *L);
int lua_godot_variant_pow(lua_State *L);
int lua_godot_variant_unm(lua_State *L);
int lua_godot_variant_band(lua_State *L);
int lua_godot_variant_bor(lua_State *L);
int lua_godot_variant_bxor(lua_State *L);
int lua_godot_variant_bnot(lua_State *L);
int lua_godot_variant_shl(lua_State *L);
int lua_godot_variant_shr(lua_State *L);
int lua_godot_variant_lt(lua_State *L);
int lua_godot_variant_le(lua_State *L);

int lua_godot_object_new(lua_State *L);
int lua_godot_object_indexer(lua_State *L);
int lua_godot_object_newindexer(lua_State *L);
int lua_godot_object_gc(lua_State *L);
int lua_godot_object_tostring(lua_State *L);
int lua_godot_object_eq(lua_State *L);
int godot_object_is_instance(lua_State *L);
int script_emit_signal(lua_State *L);

int godot_string_indexer(lua_State *L);
