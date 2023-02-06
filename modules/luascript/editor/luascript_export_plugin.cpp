#include "luascript_export_plugin.h"

#include "luascript/luascript.h"

static int spike_lua_Writer(lua_State *L, const void *p, size_t sz, void *u) {
	Vector<uint8_t> *file = (Vector<uint8_t> *)u;
	for (size_t i = 0; i < sz; i++) {
		file->push_back(*((uint8_t *)p + i));
	}
	return 0;
}

void LuaScriptExportPlugin::_export_file(const String &p_path, const String &p_type, const Set<String> &p_features) {
	if (!p_path.ends_with(".lua"))
		return;

	// TODO: check if export lua script as bytecode.

	Error error;
	String code = FileAccess::get_file_as_string(p_path, &error);
	if (error != OK) {
		DLog("get_file_as_string NOT OK(%d): '%s'", error, p_path);
		return;
	}

	auto L = LUA_STATE;
	auto source_str = code.utf8();
	GD_STR_HOLD(res_path, p_path);
	int ret = luaL_loadbuffer(L, source_str.get_data(), source_str.length(), res_path);
	if (ret != LUA_OK) {
		DLog("luaL_loadbuffer NOT OK(%d): '%s'", error, p_path);
		return;
	}

	Vector<uint8_t> file;
	lua_dump(L, spike_lua_Writer, &file, 0);
	lua_pop(L, 1);

	add_file(p_path.get_basename() + ".luac", file, true);
}
