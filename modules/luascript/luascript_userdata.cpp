/**
 * This file is part of Lua binding for Godot Engine.
 * The user data of Godot exists in Lua:
 * LIGHTUSERDATA 	- refrence to Godot Object
 * USERDATA 		- there are two types of value
 * 		1. Objects wrapped by `Variant`.
 * 		2. Objects generated wrapped code but passed by value (such as NodePath),
 */

#include "godot_lua_cfuntions.h"
#include "godot_lua_convert_api.h"
#include "luascript.h"
#include "luascript_instance.h"

void LuaScriptLanguage::register_userdata(lua_State *L, const char *class_name) {
	if (!luaL_getmetatable(L, class_name)) {
		lua_pop(L, 1);
		gdluaL_newmetatable(L, class_name);
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");

		auto elem = singleton->registers_map.find(class_name);
		if (elem) {
#ifdef GODOT_3_X
			singleton->registers_map.erase(elem);
			(*elem->get())(L);
#else
			singleton->registers_map.erase(elem->key);
			elem->value(L);
#endif
		}
	}
}

void LuaScriptLanguage::register_variant(lua_State *L, Variant::Type type, const char *type_name) {
	if (!luaL_getmetatable(L, type_name)) {
		lua_pop(L, 1);
		gdluaL_newmetatable(L, type_name);
		luatable_rawset(L, -1, "new", &lua_godot_variant_call);
		luatable_rawset(L, -1, "__type", (lua_Integer)type);
		luatable_rawset(L, -1, "__call", &lua_godot_variant_call);
		luatable_rawset(L, -1, "__index", &lua_godot_variant_indexer);
		luatable_rawset(L, -1, "__newindex", &lua_godot_variant_newindexer);
		luatable_rawset(L, -1, "__tostring", &lua_godot_variant_tostring);
		luatable_rawset(L, -1, "__gc", &lua_godot_variant_gc);
		luatable_rawset(L, -1, "__eq", &lua_godot_variant_eq);
		luatable_rawset(L, -1, "__add", &lua_godot_variant_add);
		luatable_rawset(L, -1, "__sub", &lua_godot_variant_sub);
		luatable_rawset(L, -1, "__mul", &lua_godot_variant_mul);
		luatable_rawset(L, -1, "__div", &lua_godot_variant_div);
		luatable_rawset(L, -1, "__mod", &lua_godot_variant_mod);
		luatable_rawset(L, -1, "__pow", &lua_godot_variant_pow);
		luatable_rawset(L, -1, "__unm", &lua_godot_variant_unm);
		luatable_rawset(L, -1, "__band", &lua_godot_variant_band);
		luatable_rawset(L, -1, "__bor", &lua_godot_variant_bor);
		luatable_rawset(L, -1, "__bxor", &lua_godot_variant_bxor);
		luatable_rawset(L, -1, "__bnot", &lua_godot_variant_bnot);
		luatable_rawset(L, -1, "__shl", &lua_godot_variant_shl);
		luatable_rawset(L, -1, "__shr", &lua_godot_variant_shr);
		luatable_rawset(L, -1, "__lt", &lua_godot_variant_lt);
		luatable_rawset(L, -1, "__le", &lua_godot_variant_le);

		lua_pushvalue(L, -1);
		gdlua_setmetatable(L, -2);
	}
}

void LuaScriptLanguage::push_variant(lua_State *L, const Variant *variant) {
	auto varType = variant->get_type();
	if (variant != nullptr && varType != Variant::NIL) {
		switch (varType) {
			case Variant::BOOL:
				lua_pushboolean(L, variant->operator bool() ? 1 : 0);
				return;
			case Variant::INT:
				lua_pushinteger(L, variant->operator int64_t());
				return;
			case TYPE_REAL:
				lua_pushnumber(L, variant->operator double());
				return;
			case Variant::STRING:
#ifndef GODOT_3_X
			case Variant::STRING_NAME:
#endif
				lua_pushstring(L, GD_UTF8_STR(variant->operator String()));
				return;
			case Variant::DICTIONARY:
				lua_push_godot_dictionary(L, variant->operator Dictionary());
				return;
			case Variant::ARRAY:
				lua_push_godot_array(L, variant->operator Array());
				return;
			case Variant::OBJECT: {
				auto obj = variant->operator Object *();
				LuaScriptLanguage::push_godot_object(L, obj);
				return;
			}
			default: {
				auto type_name = Variant::get_type_name(varType);
				auto class_name = StringName(type_name);
				if (auto conv = find_godot2lua(class_name)) {
					// For types implemented in lua
					conv(L, *variant);
				} else if (auto conv = find_udata2lua(class_name)) {
					// TODO For wrapped types
					lua_pushnil(L);
				} else {
					// For full userdata <Variant>
					auto udata = lua_newuserdata(L, sizeof(Variant));
					memnew_placement(udata, Variant(*variant));
					register_variant(L, varType, GD_STR_DATA(type_name));
					lua_setmetatable(L, -2);
				}
				return;
			}
		}
	} else {
		lua_pushnil(L);
	}
}

void LuaScriptLanguage::register_godot_object(lua_State *L, const StringName &class_name) {
	//auto cname = class_name.operator String().ascii();
	GD_STR_HOLD(cname, class_name);
	if (!luaL_getmetatable(L, cname)) {
		lua_pop(L, 1);
		gdluaL_newmetatable(L, cname);
		luatable_rawset(L, -1, META_NATIVE_FIELD, cname);
		luatable_rawset(L, -1, "new", &lua_godot_object_new);
		luatable_rawset(L, -1, "__call", &lua_godot_object_new);
		luatable_rawset(L, -1, "__index", &lua_godot_object_indexer);
		luatable_rawset(L, -1, "__newindex", &lua_godot_object_newindexer);
		luatable_rawset(L, -1, "__gc", &lua_godot_object_gc);
		luatable_rawset(L, -1, "__tostring", &lua_godot_object_tostring);
		luatable_rawset(L, -1, "__eq", &lua_godot_object_eq);
		luatable_rawset(L, -1, "is_instance", &godot_object_is_instance);

		VLog("register_userdata_type: %s", class_name);

		List<String> constants;
		ClassDB::get_integer_constant_list(class_name, &constants, true);
		for (size_t i = 0; i < constants.size(); i++) {
			String const_name = constants[i];
			int const_value = ClassDB::get_integer_constant(class_name, const_name);
			lua_pushinteger(L, const_value);
			lua_setfield(L, -2, GD_STR_DATA(const_name));
		}
	}
}

const StringName &LuaScriptLanguage::_import_godot_class(lua_State *L, const StringName &class_name) {
	if (class_name == StringName())
		return class_name;

	auto registers_map = &get_singleton()->registers_map;

	if (!luaL_getmetatable(L, GD_STR_NAME_DATA(class_name))) {
		lua_pop(L, 1);
		// Get metatable of base class first.
		auto base_name = ClassDB::get_parent_class_nocheck(class_name);
		//DLog("[%d] Begin Wrap: %s -> %s", lua_gettop(L), class_name, base_name);
		auto wrap_name = _import_godot_class(L, base_name);
		auto elem = registers_map->find(class_name);
		if (elem) {
#ifdef GODOT_3_X
			registers_map->erase(elem);
			register_godot_object(L, class_name);
			(*elem->get())(L);
#else
			registers_map->erase(elem->key);
			register_godot_object(L, class_name);
			elem->value(L);
#endif
			VLog("[%d] Done Wrap: %s", lua_gettop(L), class_name);
		} else {
			register_godot_object(L, class_name);
			VLog("[%d] Auto Wrap: %s", lua_gettop(L), class_name);
		}
		if (wrap_name != StringName()) {
			lua_pushvalue(L, -2);
			gdlua_setmetatable(L, -2);
			lua_remove(L, -2);
			//DLog("[%d] setmetatable: %s <- %s", lua_gettop(L),  class_name, base_name);
		} else {
			// Make metatable for Object, the top base class.
			lua_createtable(L, 0, 3);
			luatable_rawset(L, -1, "__call", &lua_godot_object_new);
			luatable_rawset(L, -1, "__index", &lua_godot_object_indexer);
			luatable_rawset(L, -1, "__newindex", &lua_godot_object_newindexer);

			gdlua_setmetatable(L, -2);
		}
	}
	return class_name;
}

const bool LuaScriptLanguage::import_godot_object(lua_State *L, const String &class_name) {
	if (ClassDB::class_exists(class_name)) {
		_import_godot_class(L, class_name);
		return true;
	} else if (ClassDB::class_exists(String("_") + class_name)) {
		_import_godot_class(L, String("_") + class_name);
		return true;
	}
	return false;
}

void LuaScriptLanguage::push_godot_object(lua_State *L, Object *obj) {
	if (obj) {
		if (auto script = Object::cast_to<LuaScript>(obj)) {
			script->push_self(L);
			return;
		}
		auto instance = LuaScriptInstance::get_script_instance(obj);
		if (instance) {
			instance->push_self(L);
			return;
		}
	}
	ref_godot_object(L, obj);
}
