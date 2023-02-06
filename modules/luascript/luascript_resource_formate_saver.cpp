/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include "core/os/os.h"

#include "constants.h"
#include "luascript.h"
#include "luascript_resource_formate_saver.h"

#ifdef GODOT_3_X
Error LuaScriptResourceFormatSaver::save(const String &p_path, const RES &p_resource, uint32_t p_flags) {
#else
Error LuaScriptResourceFormatSaver::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
#endif
	Ref<LuaScript> script = p_resource;

	if (script.is_null())
		return ERR_INVALID_PARAMETER;

	String source = script->get_source_code();

	Error error;
	auto file = FileAccess::open(p_path, FileAccess::WRITE, &error);

	if (error != OK)
		return error;

	file->store_string(source);
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
#ifdef GODOT_3_X
		memdelete(file);
#endif
		return ERR_CANT_CREATE;
	}
#ifdef GODOT_3_X
	file->close();
	memdelete(file);
#endif

	return OK;
}

void LuaScriptResourceFormatSaver::get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<LuaScript>(*p_resource)) {
		p_extensions->push_back(LUA_EXTENSION);
	}
}

bool LuaScriptResourceFormatSaver::recognize(const RES &p_resource) const {
	return Object::cast_to<LuaScript>(*p_resource) != nullptr;
}
