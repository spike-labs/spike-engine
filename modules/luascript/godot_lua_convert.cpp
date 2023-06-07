/**
 * This file is part of Lua binding for Godot Engine.
 *
 */
#include "godot_lua_convert_api.h"

Object *lua_to_godot_object(lua_State *L, int pos, bool *valid) {
	void *udata = nullptr;
	if (valid)
		*valid = true;
	pos = lua_absindex(L, pos);
	switch (lua_type(L, pos)) {
		case LUA_TUSERDATA:
		case LUA_TLIGHTUSERDATA:
			udata = lua_touserdata(L, pos);
			break;
		case LUA_TTABLE: {
			lua_pushstring(L, K_LUA_UDATA);
			lua_rawget(L, pos);
			int tt = lua_type(L, -1);
			if (tt == LUA_TTABLE) {
				udata = lua_to_godot_object(L, -1);
			} else if (tt == LUA_TNUMBER) {
				auto id = lua_tointeger(L, -1);
				udata = id != 0 ? ObjectDB::get_instance(INT_2_OID(id)) : nullptr;
			} else if (tt != LUA_TNIL) {
				udata = lua_touserdata(L, -1);
			} else {
				if (valid)
					*valid = false;
			}
			lua_pop(L, 1);
			break;
		}
		default:
			if (valid)
				*valid = false;
			break;
	}
	return udata ? static_cast<Object *>(udata) : nullptr;
}

Variant lua_to_godot_variant(lua_State *L, int pos) {
	pos = lua_absindex(L, pos);
	switch (lua_type(L, pos)) {
		case LUA_TBOOLEAN:
			return Variant((bool)lua_toboolean(L, pos));
		case LUA_TNUMBER:
			return lua_isinteger(L, pos) ? Variant((int64_t)lua_tointeger(L, pos)) : Variant(lua_tonumber(L, pos));
		case LUA_TSTRING:
			return Variant(String::utf8(lua_tostring(L, pos)));
		case LUA_TTABLE: {
			if (luaL_getmetafield(L, pos, "__name")) {
				auto class_name = lua_tostring(L, -1);
				lua_pop(L, 1);
				auto converter = LuaScriptLanguage::find_lua2godot(StringName(class_name));
				if (converter) {
					return converter(L, pos);
				} else {
					if (luaL_getmetafield(L, pos, "__type")) {
						// This is a full userdata: Variant
						auto type = lua_tointeger(L, -1);
						lua_pop(L, 1);
						CALL_ERROR r_error;
#ifdef GODOT_3_X
						return Variant::construct(Variant::Type(type), nullptr, 0, r_error);
#else
						Variant ret;
						Variant::construct(Variant::Type(type), ret, nullptr, 0, r_error);
						return ret;
#endif
					}
					return Variant(lua_to_godot_object(L, pos));
				}
			} else if (luatable_has(L, pos, K_LUA_UDATA)) {
				return Variant(lua_to_godot_object(L, pos));
			} else {
				bool is_array = true;
				lua_pushnil(L);
				if (lua_next(L, pos)) {
					if (!lua_isinteger(L, -2) || lua_tointeger(L, -2) != 1) {
						is_array = false;
					}
					lua_pop(L, 2);
				}
				if (is_array) {
					return lua_to_godot_array(L, pos);
				} else {
					return lua_to_godot_dictionary(L, pos);
				}
			}
			break;
		}
		case LUA_TLIGHTUSERDATA: {
			gdlua_toobject(L, pos, obj);
			if (obj != nullptr)
				return Variant(obj);
			break;
		}
		case LUA_TUSERDATA:
			if (luaL_getmetafield(L, pos, "__type") == LUA_TNIL) {
				if (luaL_getmetafield(L, pos, "__name") != LUA_TNIL) {
					auto type_name = lua_tostring(L, -1);
					lua_pop(L, 1);
					auto conv = LuaScriptLanguage::find_udata2var(type_name);
					if (conv != nullptr) {
						return conv(lua_touserdata(L, pos));
					}
				}
			} else {
				lua_pop(L, 1);
				gdlua_tovariant(L, pos, var);
				if (var != nullptr)
					return *var;
				break;
			}
		case LUA_TNIL:
			return Variant();
		default:
			const char *str = luaL_tolstring(L, -1, nullptr);
			lua_pop(L, 1);
			return Variant(str);
	}
	return Variant();
}

Variant lua_to_godot_array(lua_State *L, int t) {
	Array array;
	if (lua_istable(L, t)) {
		t = lua_absindex(L, t);
		const char *ret = _begin_cycle_check(L, t);
		if (ret != nullptr) {
			return Variant(ret);
		}

		int i = 0;
		lua_pushnil(L);
		while (lua_next(L, t) != 0) {
			i += 1;
			if (lua_isinteger(L, -2) && lua_tointeger(L, -2) == i) {
				array.append(lua_to_godot_variant(L, -1));
			}
			lua_pop(L, 1);
		}

		_end_cycle_check(L, t);
	}
	return Variant(array);
}

Vector<Variant> lua_to_godot_variant_vector(lua_State *L, int t) {
	Vector<Variant> array;
	if (lua_istable(L, t)) {
		t = lua_absindex(L, t);
		const char *ret = _begin_cycle_check(L, t);
		if (ret != nullptr) {
			return Variant(ret);
		}

		int i = 0;
		lua_pushnil(L);
		while (lua_next(L, t) != 0) {
			i += 1;
			if (lua_isinteger(L, -2) && lua_tointeger(L, -2) == i) {
				array.push_back(lua_to_godot_variant(L, -1));
			}
			lua_pop(L, 1);
		}

		_end_cycle_check(L, t);
	}
	return array;
}

Variant lua_to_godot_dictionary(lua_State *L, int t) {
	Dictionary dict;
	if (lua_istable(L, t)) {
		t = lua_absindex(L, t);
		const char *ret = _begin_cycle_check(L, t);
		if (ret != nullptr) {
			return Variant(ret);
		}

		lua_pushnil(L);
		while (lua_next(L, t) != 0) {
			auto key = lua_to_godot_variant(L, -2);
			if (key != Variant(TABLE_2_CONTAINER_FLAG)) {
				auto value = lua_to_godot_variant(L, -1);
				dict[key] = value;
			}

			lua_pop(L, 1);
		}

		_end_cycle_check(L, t);
	}
	return Variant(dict);
}

bool lua_check_godot_variant(lua_State *L, int pos, Variant::Type type, Variant &value) {
	Variant::Type vt = Variant::Type(godot_typeof(L, pos));
	if (vt == type) {
		value = lua_to_godot_variant(L, pos);
		return true;
	}
	return false;
}

void lua_push_godot_array(lua_State *L, const Array &a) {
	auto size = a.size();
	lua_createtable(L, size, 0);
	for (int i = 0; i < size; i++) {
		lua_pushinteger(L, i + 1);
		LuaScriptLanguage::push_variant(L, &a[i]);
		lua_settable(L, -3);
	}
}

void lua_push_godot_dictionary(lua_State *L, const Dictionary &d) {
	List<Variant> keys;
	d.get_key_list(&keys);

	lua_createtable(L, 0, keys.size());
	for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
		auto k = E->get();
		auto v = d[k];
		lua_pushstring(L, GD_UTF8_STR(k.operator String()));
		LuaScriptLanguage::push_variant(L, &v);
		lua_settable(L, -3);
	}
}

int godot_typeof(lua_State *L, int pos) {
	int ttype = lua_type(L, pos);
	switch (ttype) {
		case LUA_TLIGHTUSERDATA:
			return Variant::OBJECT;
		case LUA_TUSERDATA: {
			int vtype = Variant::NIL;
			if (luaL_getmetafield(L, 1, "__type") == LUA_TNIL) {
				if (luaL_getmetafield(L, 1, "__name") != LUA_TNIL) {
					auto type_name = lua_tostring(L, -1);
					lua_pop(L, 1);
					vtype = LuaScriptLanguage::find_variant_type(type_name);
				}
			} else {
				vtype = lua_tointeger(L, -1);
				lua_pop(L, 1);
			}
			return vtype;
		}
		case LUA_TTABLE: {
			int vtype = Variant::NIL;
			if (luaL_getmetafield(L, 1, "__name")) {
				const char *tname = lua_tostring(L, -1);
				lua_pop(L, 1);
				if (strncmp(tname, "res://", 6) == 0) {
					if (ResourceLoader::exists(tname))
						vtype = Variant::OBJECT;
				} else {
					auto type_name = StringName(tname);
					vtype = LuaScriptLanguage::find_variant_type(type_name);
					if (vtype == Variant::NIL) {
						if (ClassDB::class_exists(type_name))
							vtype = Variant::OBJECT;
					}
				}
			} else {
				bool is_array = true;
				lua_pushnil(L);
				if (lua_next(L, 1)) {
					if (!lua_isinteger(L, -2) || lua_tointeger(L, -2) != 1) {
						is_array = false;
					}
					lua_pop(L, 2);
				}
				vtype = is_array ? Variant::ARRAY : Variant::DICTIONARY;
			}
			lua_pushinteger(L, vtype);
			return vtype;
		}
		case LUA_TBOOLEAN:
			return Variant::BOOL;
		case LUA_TSTRING:
			return Variant::STRING;
		case LUA_TNUMBER:
			return lua_isinteger(L, 1) ? Variant::INT : TYPE_REAL;
		default:
			break;
	}

	return Variant::NIL;
}
