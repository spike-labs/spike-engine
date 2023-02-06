/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include "godot_lua_cfuntions.h"
#include "godot_lua_convert_lib.h"
#include "luascript_instance.h"
#include "modules/godot_wrap_for_lua.gen.h" // This file is used to trick the builder to executing the code generation script..
#include "modules/lua_codes.gen.h"
#include "wrapper/godot_wrap_for_lua.gen.h"
#ifdef GODOT_3_X
#include "core/global_constants.h"
#else
#include "core/core_constants.h"
#endif

extern int godot_udata_method_call(lua_State *L);

static int godot_lua_call_error(lua_State *L) {
	const char *msg = lua_tostring(L, 1);
	luaL_traceback(L, L, msg, 1);
	const char *traceback = lua_tostring(L, -1);
	lua_pop(L, 1);
	print_error(String::utf8(traceback));
	lua_pushvalue(L, 1);
	return 1;
}

int godot_lua_xpcall(lua_State *L, int n, int r) {
	lua_pushcfunction(L, &godot_lua_call_error);
	int f = lua_absindex(L, -n - 2);
	lua_insert(L, f);
	int ret = lua_pcall(L, n, r, f);
	lua_remove(L, f);
	if (ret != LUA_OK)
		lua_pop(L, 1);
	return ret;
}

static int godot_lua_dostring(lua_State *L, const char *libname, const char *libsource) {
	int ret = luaL_loadbuffer(L, libsource, strlen(libsource), libname);
	if (ret == LUA_OK) {
		ret = godot_lua_xpcall(L, 0, 1);
	} else {
		auto err_msg = lua_tostring(L, -1);
		ELog("load '%s.lua' error: %s", libname, err_msg);
		lua_pop(L, 1);
	}
	return ret;
}

static void load_global_chunk(lua_State *L, const char *libname, const char *libsource) {
	if (godot_lua_dostring(L, libname, libsource) == LUA_OK) {
		if (lua_type(L, -1) != LUA_TNIL) {
			lua_setglobal(L, libname);
		} else {
			lua_pop(L, 1);
		}
	}
}

static void try_load_value_type(lua_State *L, const char *libname, const char *libsource, lua2godot_TypeConvert lua2godot, godot2lua_TypeConvert godot2lua) {
	LuaScriptLanguage::add_lua2godot(StringName(libname), lua2godot);
	LuaScriptLanguage::add_godot2lua(StringName(libname), godot2lua);

	load_global_chunk(L, libname, libsource);
}

static int variant_utility_function_call(lua_State *L) {
	const char *func_name = luaL_optstring(L, lua_upvalueindex(1), nullptr);
	StringName p_name = StringName(func_name);
	Variant ret;
	LUA_TO_ARGS(p_args, argcount, 1);
	CALL_ERROR error;
	Variant::call_utility_function(p_name, &ret, p_args, argcount, error);
	if (error.error != CALL_ERROR::CALL_OK) {
		ELog("call_utility_function error: %d", error.error);
	}
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

static int variant_utility_functions_indexer(lua_State *L) {
	const char *func_name = lua_tostring(L, 2);
	StringName p_name = StringName(func_name);
	if (Variant::has_utility_function(p_name)) {
		lua_pushstring(L, func_name); // t|k|n
		lua_pushcclosure(L, &variant_utility_function_call, 1); // t|k|f
		lua_pushvalue(L, 2); // t|k|f|k
		lua_pushvalue(L, -2); // t|k|f|k|f
		lua_rawset(L, -5); // t|k|f
		return 1;
	}
	lua_getglobal(L, "CONST");
	lua_pushvalue(L, 2);
	lua_gettable(L, -2);
	lua_remove(L, -2);
	return 1;
}

extern void init_lua_global(lua_State *L) {
	lua_register(L, "topointer", &godot_lua_topointer);
	lua_register(L, "print", &godot_lua_print);
	lua_register(L, "printerr", &godot_lua_printerr);
	lua_register(L, "push_error", &godot_lua_push_error);
	lua_register(L, "typeof", &godot_global_typeof);
	lua_register(L, "type_exists", &godot_global_type_exists);
	lua_register(L, "funcref", &godot_lua_funcref);
	lua_register(L, "parse_json", &godot_global_parse_json);
	lua_register(L, "to_json", &godot_global_to_json);
	lua_register(L, "new_variant", &lua_godot_variant_create);
	lua_register(L, "godot_call", &godot_udata_method_call);

	// Push Godot GlobalConstants into global talbe `CONST`
	lua_newtable(L);
	for (size_t i = 0; i < GlobalConstants::get_global_constant_count(); i++) {
		const char *name = GlobalConstants::get_global_constant_name(i);
		int value = GlobalConstants::get_global_constant_value(i);
		lua_pushinteger(L, value);
		lua_setfield(L, -2, name);
	}
	lua_setglobal(L, "CONST");

	// Push Godot VariantUtilityFunctions into global table `godot`
	lua_newtable(L);
	lua_createtable(L, 0, 1);
	luatable_rawset(L, -1, "__index", &variant_utility_functions_indexer);
	lua_setmetatable(L, -2);
	lua_setglobal(L, "godot");

	load_global_chunk(L, "globals", code_godot_globals_lua);
}

extern void load_thirdlib_serpent(lua_State *L) {
	load_global_chunk(L, "serpent", code_serpent_lua);
}

extern void load_godot_value_types(lua_State *L) {
	try_load_value_type(L, "Vector2", code_vector2_lua, &convert_to_vector2, &lua_push_godot_vector2);
	try_load_value_type(L, "Rect2", code_rect2_lua, &convert_to_rect2, &lua_push_godot_rect2);
	try_load_value_type(L, "Vector3", code_vector3_lua, &convert_to_vector3, &lua_push_godot_vector3);
}

#ifdef GODOT_3_X
static int godot_object_connect(lua_State *L) {
	if (auto gd = lua_to_godot_object(L, 1)) {
		const char *signal = lua_tostring(L, 2);
		Object *obj = lua_to_godot_object(L, 3);
		const char *method = lua_tostring(L, 4);
		Vector<Variant> binds = lua_to_godot_variant_vector(L, 5);
		int flag = luaL_optinteger(L, 6, 0);
		Error ret = gd->connect(signal, obj, method, binds, flag);
		lua_pushinteger(L, (lua_Integer)ret);
		return 1;
	}
	return 0;
}
static int godot_object_disconnect(lua_State *L) {
	if (Object *gd = lua_to_godot_object(L, 1)) {
		const char *signal = lua_tostring(L, 2);
		Object *obj = lua_to_godot_object(L, 3);
		const char *method = lua_tostring(L, 4);
		gd->disconnect(signal, obj, method);
	}
	return 0;
}

static int godot_object_is_connected(lua_State *L) {
	if (Object *gd = lua_to_godot_object(L, 1)) {
		const char *signal = lua_tostring(L, 2);
		Object *obj = lua_to_godot_object(L, 3);
		const char *method = lua_tostring(L, 4);
		bool ret = gd->is_connected(signal, obj, method);
		lua_pushboolean(L, ret);
		return 1;
	}
	return 0;
}
#else
static void get_callable_and_flags(lua_State *L, int pos, Callable &callable, int &flags) {
	if (lua_type(L, pos) == LUA_TUSERDATA) {
		auto arg = lua_to_godot_variant(L, pos);
		if (arg.get_type() == Variant::CALLABLE) {
			callable = arg.operator Callable();
		}
		flags = luaL_optinteger(L, pos + 1, 0);
	} else {
		Object *obj = lua_to_godot_object(L, pos);
		const char *method = lua_tostring(L, pos + 1);
		flags = luaL_optinteger(L, pos + 3, 0);
		Vector<Variant> binds = lua_to_godot_variant_vector(L, pos + 2);
		callable = Callable(memnew(CallableCustomBind(Callable(obj, method), binds)));
	}
}

static int godot_object_connect(lua_State *L) {
	Callable callable;
	int flags = 0;

	auto var = lua_to_godot_variant(L, 1);
	auto vtype = var.get_type();
	if (vtype == Variant::OBJECT) {
		auto gd = var.operator Object *();
		const char *signal = lua_tostring(L, 2);
		get_callable_and_flags(L, 3, callable, flags);
		if (callable.is_valid()) {
			Error ret = gd->connect(signal, callable, flags);
			lua_pushinteger(L, (lua_Integer)ret);
			return 1;
		}
	} else if (vtype == Variant::SIGNAL) {
		auto signal = var.operator Signal();
		get_callable_and_flags(L, 2, callable, flags);
		if (callable.is_valid()) {
			Error ret = signal.connect(callable, flags);
			lua_pushinteger(L, (lua_Integer)ret);
			return 1;
		}
	}

	const char *obj_str = luaL_tolstring(L, 1, nullptr);
	lua_pop(L, 1);
	luaL_error(L, "fail to call `connect` on: %s", obj_str);
	return 0;
}
#endif

static int godot_object_disconnect(lua_State *L) {
	Callable callable;
	int flags = 0;

	auto var = lua_to_godot_variant(L, 1);
	auto vtype = var.get_type();
	if (vtype == Variant::OBJECT) {
		auto gd = var.operator Object *();
		const char *signal = lua_tostring(L, 2);
		get_callable_and_flags(L, 3, callable, flags);
		if (callable.is_valid()) {
			gd->disconnect(signal, callable);
			return 0;
		}
	} else if (vtype == Variant::SIGNAL) {
		auto signal = var.operator Signal();
		get_callable_and_flags(L, 2, callable, flags);
		if (callable.is_valid()) {
			signal.disconnect(callable);
			return 0;
		}
	}

	const char *obj_str = luaL_tolstring(L, 1, nullptr);
	lua_pop(L, 1);
	luaL_error(L, "fail to call `disconnect` on: %s", obj_str);
	return 0;
}

static int godot_object_is_connected(lua_State *L) {
	Callable callable;
	int flags = 0;

	auto var = lua_to_godot_variant(L, 1);
	auto vtype = var.get_type();
	if (vtype == Variant::OBJECT) {
		auto gd = var.operator Object *();
		const char *signal = lua_tostring(L, 2);
		get_callable_and_flags(L, 3, callable, flags);
		if (callable.is_valid()) {
			gd->is_connected(signal, callable);
			return 0;
		}
	} else if (vtype == Variant::SIGNAL) {
		auto signal = var.operator Signal();
		get_callable_and_flags(L, 2, callable, flags);
		if (callable.is_valid()) {
			signal.is_connected(callable);
			return 0;
		}
	}

	const char *obj_str = luaL_tolstring(L, 1, nullptr);
	lua_pop(L, 1);
	luaL_error(L, "fail to call `is_connected` on: %s", obj_str);
	return 0;
}

extern void create_gdsingleton_for_lua(Object *singleton) {
	Ref<LuaScript> script;
	REF_INSTANTIATE(script);
	script->set_script_path("gdsingleton.lua");
	script->set_source_code(code_gdsingleton_lua);
	script->reload();
	script->instance_create(singleton);

	lua_State *L = LUA_STATE;
	script->push_self(L);
	luatable_rawset(L, -1, "connect", &godot_object_connect);
	luatable_rawset(L, -1, "disconnect", &godot_object_disconnect);
	luatable_rawset(L, -1, "is_connected", &godot_object_is_connected);
	lua_pop(L, 1);
}

extern int ref_luascript_api(lua_State *L) {
	if (godot_lua_dostring(L, "luascript", code_luascript_lua) == LUA_OK) {
		return luaL_ref(L, LUA_REGISTRYINDEX);
	}
	return 0;
}

static Variant godot_variant_call(lua_State *L, const char *method, int udata, int arg_start) {
	if (method == nullptr)
		return Variant();

	udata = lua_absindex(L, udata);
	arg_start = lua_absindex(L, arg_start);

	Object *gd = nullptr;
	if (luaL_getmetafield(L, udata, "__type")) {
		// This is a full userdata: Variant
		lua_pop(L, 1);
		gdlua_tovariant(L, udata, var);
		if (var != nullptr) {
			if (var->get_type() != Variant::OBJECT) {
				LUA_TO_ARGS(p_args, argcount, arg_start);
				CALL_ERROR error;
				VAR_VALL(*var, method, p_args, argcount, ret, error);
				return ret;
			}
			gd = var->operator Object *();
		}
	} else {
		gd = lua_to_godot_object(L, udata);
	}

	StringName class_name;
	if (gd) {
		class_name = gd->get_class_name();
	} else {
		const char *c_name;
		if (luatable_rawget(L, udata, "__name", c_name)) {
			class_name = StringName(c_name);
		}
	}

	if (class_name != StringName()) {
		CALL_ERROR error;
		Variant ret;
		LUA_TO_ARGS(p_args, argcount, arg_start);
		auto lua_inst = LuaScriptInstance::get_script_instance(gd);
		if (lua_inst) {
			MethodBind *methodbind = ClassDB::get_method(class_name, method);
			if (methodbind) {
				ret = methodbind->call(gd, p_args, argcount, error);
			} else {
				WARN_PRINT(vformat("lua script instance call other script's method is not supported.(%s.%s())", class_name, method));
			}
		} else {
			if (gd) {
				ret = GD_CALL(*gd, method, p_args, argcount, error);
			} else {
				MethodBind *methodbind = ClassDB::get_method(class_name, method);
				if (methodbind) {
					if (methodbind->is_static()) {
						ret = methodbind->call(nullptr, p_args, argcount, error);
					} else {
						WARN_PRINT(vformat("Non static method `%s::%s` is called with null instance.", class_name, method));
					}
				} else {
					WARN_PRINT(vformat("static method: `%s::%s` NOT EXIST!", class_name, method));
				}
			}
		}
		return ret;
	}

	push_error(L, "missing object instance on call, try `instance:%s(...)`.", method);
	return Variant();
}

static int godot_method_call(lua_State *L) {
	const char *method = luaL_optstring(L, lua_upvalueindex(1), nullptr);
	auto ret = godot_variant_call(L, method, 1, 2);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

/**
 * @brief Call method of Godot Object/Variant
 */
extern int godot_udata_method_call(lua_State *L) {
	const char *method = luaL_optstring(L, 1, nullptr);
	auto ret = godot_variant_call(L, method, 2, 3);
	LuaScriptLanguage::push_variant(L, &ret);
	return 1;
}

extern int push_not_lua_method(lua_State *L, const char *p_method) {
	if (!LuaScriptLanguage::push_godot_method(L, p_method)) {
		lua_pushstring(L, p_method);
		lua_pushcclosure(L, &godot_method_call, 1);
		LuaScriptLanguage::register_godot_method(L, p_method, -1);
	}
	return 1;
}
