/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include "constants.h"
#include "godot_lua_convert_api.h"
#include "luascript_language.h"

int lua_godot_variant_gc(lua_State *L) {
	gdlua_tovariant(L, 1, var);
	if (var != nullptr) {
		var->~Variant();
	}
	return 0;
}

// int _variant_method_call(lua_State *L) {
// 	auto method = lua_tostring(L, lua_upvalueindex(1));
// 	gdlua_tovariant(L, 1, var);
// 	if (var != nullptr) {
// 		LUA_TO_ARGS(p_args, argcount, 2);
// 		CALL_ERROR error;
// 		VAR_VALL(*var, method, p_args, argcount, ret, error);
// 		LuaScriptLanguage::push_variant(L, &ret);
// 		return 1;
// 	}

// 	ELog("%s", "missing object instance on call, try `instance:func(...)`.");
// 	return 0;
// }

int lua_godot_variant_indexer(lua_State *L) {
	gdlua_tovariant(L, 1, var);
	if (var != nullptr) {
		int ktype = lua_type(L, 2);
		if (ktype == LUA_TSTRING) {
			auto method = lua_tostring(L, 2);
			if (var->has_method(method)) {
				push_not_lua_method(L, method);
				return 1;
			}
		}

		auto key = lua_to_godot_variant(L, 2);
		bool valid;
		auto ret = var->get(key, &valid);
		if (valid) {
			LuaScriptLanguage::push_variant(L, &ret);
			return 1;
		}
	} else {
		// Try get constant of Variant
		lua_getfield(L, 1, "__type");
		auto type_id = luaL_optinteger(L, -1, 0);
		lua_pop(L, 1);
		if (type_id > 0) {
			auto key = luaL_optstring(L, 2, nullptr);
			auto ret = Variant::get_constant_value(Variant::Type(type_id), key);
			LuaScriptLanguage::push_variant(L, &ret);
			return 1;
		}
	}

	return 0;
}

int lua_godot_variant_newindexer(lua_State *L) {
	auto key = luaL_optstring(L, 2, nullptr);
	gdlua_tovariant(L, 1, var);
	if (var != nullptr) {
		if (key) {
			auto value = lua_to_godot_variant(L, 3);
			var->set(key, value);
		}
	} else {
		//  Setting class constants is not allowed.
	}
	return 0;
}

int lua_godot_variant_call(lua_State *L) {
	if (lua_istable(L, 1)) {
		// Call with Data Type: calls the constructor
		lua_getfield(L, 1, "__type");
		auto type_id = luaL_optinteger(L, -1, 0);
		lua_pop(L, 1);
		LUA_TO_ARGS(p_args, n_arg, 2);
		CALL_ERROR error;
#ifdef GODOT_3_X
		auto ret = Variant::construct(Variant::Type(type_id), p_args, n_arg, error);
#else
		Variant ret;
		Variant::construct(Variant::Type(type_id), ret, p_args, n_arg, error);
#endif
		LuaScriptLanguage::push_variant(L, &ret);
		return 1;
	} else {
		auto var = lua_to_godot_variant(L, 1);
		CALL_ERROR error;
		Variant ret;
		switch (var.get_type()) {
#ifndef GODOT_3_X
			case Variant::CALLABLE: {
				LUA_TO_ARGS(p_args, n_arg, 2);
				var.operator Callable().callp(p_args, n_arg, ret, error);
			} break;
			case Variant::SIGNAL: {
				LUA_TO_ARGS(p_args, n_arg, 2);
				var.operator Signal().emit(p_args, n_arg);
			} break;
#endif
			default:
				break;
		}
		LuaScriptLanguage::push_variant(L, &ret);
		return 1;
	}
}

int lua_godot_variant_tostring(lua_State *L) {
	gdlua_tovariant(L, 1, var);
	if (var != nullptr) {
		lua_pushstring(L, GD_UTF8_STR(var->operator String()));
	} else {
		luaL_getmetafield(L, 1, "__name");
		const char *name = lua_tostring(L, -1);
		lua_pop(L, 1);
		lua_pushfstring(L, "{%s}", name);
	}
	return 1;
}

/**
 * @brief a == b
 */
int lua_godot_variant_eq(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);
	lua_pushboolean(L, a == b);
	return 1;
}

/**
 * @brief a + b
 */
int lua_godot_variant_add(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_ADD, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a - b
 */
int lua_godot_variant_sub(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_SUBTRACT, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a * b
 */
int lua_godot_variant_mul(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_MULTIPLY, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a / b
 */
int lua_godot_variant_div(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_DIVIDE, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a % b
 */
int lua_godot_variant_mod(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_MODULE, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a ^ b
 */
int lua_godot_variant_pow(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_POWER, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief -a
 */
int lua_godot_variant_unm(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_NEGATE, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a & b
 */
int lua_godot_variant_band(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_BIT_AND, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a | b
 */
int lua_godot_variant_bor(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_BIT_OR, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a ~ b
 */
int lua_godot_variant_bxor(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_BIT_XOR, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief ~a
 */
int lua_godot_variant_bnot(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_BIT_NEGATE, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a >> b
 */
int lua_godot_variant_shl(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_SHIFT_LEFT, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a << b
 */
int lua_godot_variant_shr(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_SHIFT_RIGHT, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a < b
 */
int lua_godot_variant_lt(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_LESS, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief a <= b
 */
int lua_godot_variant_le(lua_State *L) {
	Variant a = lua_to_godot_variant(L, 1);
	Variant b = lua_to_godot_variant(L, 2);

	auto ret = Variant::evaluate(Variant::OP_LESS_EQUAL, a, b);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}
