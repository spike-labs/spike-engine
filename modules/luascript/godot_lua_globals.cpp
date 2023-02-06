/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include "constants.h"
#ifdef GODOT_3_X
#include "core/func_ref.h"
#else
#include "core/debugger/engine_debugger.h"
#include "core/debugger/script_debugger.h"
#endif
#include "core/config/engine.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "godot_lua_convert_api.h"
#include "godot_lua_table_api.h"
#include "lib/lua/lua.hpp"
#include "luascript_language.h"

int godot_lua_atpanic(lua_State *L) {
	luaL_traceback(L, L, lua_tostring(L, -1), 0);
	const char *error_msg_plus_traceback = lua_tostring(L, -1);
	ELog("LUA PANIC: %s", String::utf8(error_msg_plus_traceback));
	return 1;
}

static String _warn_str = String();
void godot_lua_warn(void *ud, const char *msg, int tocont) {
	_warn_str += msg;
	if (!tocont) {
		print_line(String("Lua warning: ") + _warn_str);
		_warn_str = String();
	}
}

int godot_lua_print(lua_State *L) {
	String content = String("");
	int n = lua_gettop(L); /* number of arguments */
	int i;
	for (i = 1; i <= n; i++) { /* for each argument */
		size_t l;
		const char *s = luaL_tolstring(L, i, &l); /* convert it to string */
		if (i > 1) /* not the first element? */
			content += " "; /* add a tab before it */
		content += String::utf8(s); /* print it */
		lua_pop(L, 1); /* pop result */
	}

	print_line(content);
	return 0;
}

int godot_lua_printerr(lua_State *L) {
	String content = String("");
	int n = lua_gettop(L); /* number of arguments */
	int i;
	for (i = 1; i <= n; i++) { /* for each argument */
		size_t l;
		const char *s = luaL_tolstring(L, i, &l); /* convert it to string */
		if (i > 1) /* not the first element? */
			content += " "; /* add a tab before it */
		content += String::utf8(s); /* print it */
		lua_pop(L, 1); /* pop result */
	}

	print_error(content);
	return 0;
}

int godot_lua_funcref(lua_State *L) {
#ifdef GODOT_3_X
	FuncRef *ref = memnew(FuncRef);
	ref->set_instance(lua_to_godot_object(L, 1));
	ref->set_function(lua_tostring(L, 2));
	LuaScriptLanguage::push_godot_object(L, ref);
	return 1;
#else
	return 0;
#endif
}

static String find_exists_modpath(String &modpath) {
	String respath = "user://" + modpath + ".lua";
	if (!FileAccess::exists(respath)) {
		respath = "res://" + modpath + ".lua";
	}
	return respath;
}

int godot_lua_loader(lua_State *L) {
	const char *modstr = luaL_optstring(L, 1, nullptr);
	if (modstr != nullptr) {
		String modpath = String(modstr).replace(".", "/");
		String respath = find_exists_modpath(modpath);
		RES res = ResourceLoader::load(respath);
		if (!res.is_null()) {
			LuaScriptLanguage::push_godot_object(L, res.ptr());
			return 1;
		}

		ELog("mod file '%s' load fail.", modstr);
	}
	return 0;
}

int godot_lua_seacher(lua_State *L) {
	const char *modstr = luaL_optstring(L, 1, nullptr);
	if (modstr != nullptr) {
		String modpath = String(modstr);
		if (modpath.find("/") >= 0) {
			lua_pushfstring(L, "invalid mod path: '%s' (don't use '/')", modstr);
			return 1;
		}

		modpath = modpath.replace(".", "/");
		String respath = find_exists_modpath(modpath);
		if (respath.length() > 0) {
			lua_pushcfunction(L, &godot_lua_loader);
			lua_pushnil(L);
			return 2;
		} else {
			lua_pushfstring(L, "no res 'user://%s.lua' nor 'res://%s.lua'", modstr, modstr);
			return 1;
		}
	}
	lua_pushnil(L);
	return 1;
}

int godot_lua_push_error(lua_State *L) {
	LuaScriptLanguage::push_lua_error(L, lua_tostring(L, 1));
	return 0;
}

void godot_lua_hook(lua_State *L, lua_Debug *ar) {
#ifdef GODOT_3_X
	auto dbg = ScriptDebugger::get_singleton();
#else
	auto dbg = EngineDebugger::get_singleton()->get_script_debugger();
#endif
	if (ar->event == LUA_HOOKLINE) {
		bool do_break = false;

		if (dbg->get_lines_left() > 0) {
			if (dbg->get_depth() <= 0) {
				dbg->set_lines_left(dbg->get_lines_left() - 1);
			}
			if (dbg->get_lines_left() <= 0) {
				do_break = true;
			}
		}

		auto *bps = dbg->get_breakpoints().getptr(ar->currentline);
		if (do_break || bps) {
			lua_getinfo(L, "Slnt", ar);
			if (!do_break) {
				StringName source = ar->source;
				if (bps->find(source)) {
					do_break = true;
				}
			}
		}

		if (do_break) {
			LuaScriptLanguage::breakpoint(L);
			LuaScriptLanguage::trackback(ar->source, ar->currentline, ar->name);
			LuaScriptLanguage::record_stack_info(L, 1, ar);
			dbg->debug(LuaScriptLanguage::get_singleton());
		}
	} else if (ar->event == LUA_HOOKCALL) {
		if (dbg->get_lines_left() > 0 && dbg->get_depth() >= 0) {
			dbg->set_depth(dbg->get_depth() + 1);
		}
	} else if (ar->event == LUA_HOOKRET) {
		if (dbg->get_lines_left() > 0 && dbg->get_depth() >= 0) {
			dbg->set_depth(dbg->get_depth() - 1);
		}
	}
}

int godot_global_indexer(lua_State *L) {
	// auto oldtop = lua_gettop(L);
	const char *key = luaL_optstring(L, 2, nullptr);
	bool found = false;
	if (key) {
		StringName className = key;
		if (Engine::get_singleton()->has_singleton(className)) {
			auto singleton = Engine::get_singleton()->get_singleton_object(className);
			LuaScriptLanguage::push_godot_object(L, singleton);
			found = true;
		} else if (LuaScriptLanguage::import_godot_object(L, className)) {
			found = true;
		} else if (auto var_type = LuaScriptLanguage::find_variant_type(className)) {
			LuaScriptLanguage::register_variant(L, var_type, key);
			found = true;
		} else if (ScriptServer::is_global_class(className)) {
			const String resPath = ScriptServer::get_global_class_path(className);
			auto res = ResourceLoader::load(resPath);
			if (res.is_valid()) {
				LuaScriptLanguage::push_godot_object(L, res.ptr());
				found = true;
			}
		}
	}

	if (found) {
		lua_pushvalue(L, -1);
		lua_setfield(L, 1, key);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

int godot_lua_topointer(lua_State *L) {
	lua_pushinteger(L, (lua_Integer)lua_topointer(L, 1));
	return 1;
}

int godot_global_typeof(lua_State *L) {
	lua_pushinteger(L, godot_typeof(L, 1));
	return 1;
}

int godot_global_type_exists(lua_State *L) {
	const char *class_name = lua_tostring(L, 1);
	lua_pushboolean(L, ClassDB::class_exists(class_name));
	return 1;
}

int godot_global_parse_json(lua_State *L) {
	const char *content = luaL_optstring(L, 1, nullptr);
	if (content != nullptr) {
		Variant ret;
		String err_str;
		int err_line;
#ifdef GODOT_3_X
		Error err = JSON::parse(content, ret, err_str, err_line);
#else
		Ref<JSON> jason;
		jason.instantiate();
		Error err = jason->parse(content);
		err_str = jason->get_error_message();
		err_line = jason->get_error_line();
		ret = jason->get_data();
#endif
		if (err == Error::OK) {
			LuaScriptLanguage::push_variant(L, &ret);
			return 1;
		} else {
			lua_pushnil(L);
			lua_pushfstring(L, "error at line %d: %s", err_line, err_str.utf8().get_data());
			return 2;
		}
	}
	return 0;
}

int godot_global_to_json(lua_State *L) {
	Variant var = lua_to_godot_variant(L, 1);
#ifdef GODOT_3_X
	auto str = JSON::print(var);
#else
	auto str = JSON::stringify(var);
#endif
	lua_pushstring(L, GD_UTF8_STR(str));
	return 1;
}

int lua_godot_variant_create(lua_State *L) {
	auto type_id = luaL_optinteger(L, 1, -1);
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
}