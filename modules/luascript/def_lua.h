/**
 * This file is part of Lua binding for Godot Engine.
 *
 */
#ifndef _DEF_LUA_
#define _DEF_LUA_

#include "spike_define.h"
#include "lib/lua/lua.hpp"

#define K_LUA_UDATA "__udata"
#define K_LUA_CLASS "__class"
#define K_LUA_EXTENDS "__extends"
#define K_LUA_ICON "__icon"
#define K_LUA_SIGNAL "__signals"
#define K_LUA_TOOL "__tool"
#define META_NATIVE_FIELD "__native"

#define K_EXEC_PATH "text_editor/external/lua_exec_path"
#define K_EDITOR_BUILTIN "Builtin"
#define K_EDITOR_VSCODE "Visual Studio Code"
#define K_EDITOR_LIST K_EDITOR_BUILTIN "," K_EDITOR_VSCODE

#define GD_UTF8_STR(s) (s).utf8().get_data()
#define GD_UTF8_NAME(n) GD_UTF8_STR((String)(n))
#define GD_STR_DATA(s) (s).ascii().get_data()
#define GD_STR_NAME_DATA(n) GD_STR_DATA((String)(n))
#define GD_CLASS_NAME(gd) GD_STR_NAME_DATA(gd->get_class_name())
#define LUA_STATE (LuaScriptLanguage::get_singleton()->state)
#define LOCK_LUA_SCRIPT MutexLock lock(LuaScriptLanguage::get_singleton()->mutex);

#define OID_2_INT(id) static_cast<lua_Integer>((int64_t)id)
#define INT_2_OID(i) ((ObjectID)static_cast<int64_t>(i))

#define GD_STR_HOLD(varname, gdstr)                   \
	CharString cs_##varname = ((String)gdstr).utf8(); \
	const char *varname = cs_##varname.get_data()

#define LUA_TO_ARGS(p_args, argcount, start)            \
	int argcount = MIN(5, lua_gettop(L) + 1 - start);   \
	const Variant *p_args[5] = {};                      \
	Variant __args[5] = {};                             \
	for (int i = 0; i < argcount; i++) {                \
		__args[i] = lua_to_godot_variant(L, i + start); \
		p_args[i] = &__args[i];                         \
	}

#ifdef DEBUG_ENABLED
#define gdlua_setmetatable(L, pos)                   \
	if (lua_type(L, pos) == LUA_TLIGHTUSERDATA)      \
		ELog("%s", "setmetatable on lightuserdata"); \
	lua_setmetatable(L, pos)

#else
#define gdlua_setmetatable(L, pos) lua_setmetatable(L, pos)
#endif
#define gdluaL_newmetatable(L, name) luaL_newmetatable(L, name)

#define gdlua_tovariant(L, n, u)       \
	Variant *u = nullptr;              \
	void *_##u = lua_touserdata(L, n); \
	if (_##u != nullptr)               \
	u = static_cast<Variant *>(_##u)

#define gdlua_toobject(L, n, u)        \
	Object *u = nullptr;               \
	void *_##u = lua_touserdata(L, n); \
	if (_##u != nullptr)               \
	u = static_cast<Object *>(_##u)

#define push_error(L, fmt, ...) LuaScriptLanguage::push_lua_error(L, GD_UTF8_STR(vformat(String("") + fmt, __VA_ARGS__)))

/**
 * @brief Calls a function (or a callable object) in protected mode.
 * All arguments and the function value are popped.
 * If there are no errors during the call, all results from the function are pushed onto the stack.
 * However, if there is any error, print traceback, and nothing will be pushed onto the stack.
 */
int godot_lua_xpcall(lua_State *L, int n, int r);

/**
 * @brief Register method of Godot Object/Variant via function name with upvalue in closure
 */
extern int push_not_lua_method(lua_State *L, const char *p_method);

inline static bool check_hidden_key(const char *key) {
	return key == nullptr || key[0] == '.' || (key[0] == '_' && key[1] == '_' && key[2] != '_');
}

#endif // _DEF_LUA_
