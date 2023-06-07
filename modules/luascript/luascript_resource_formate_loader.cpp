/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include "luascript_resource_formate_loader.h"
#include "constants.h"
#include "luascript.h"

#ifndef GODOT_3_X

void LuaScriptResourceFormatLoader::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
}

Ref<Resource> LuaScriptResourceFormatLoader::load(const String &p_path, const String &p_original_path, Error *r_error,
		bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	LuaScript *script = memnew(LuaScript);
	if (!script) {
		if (r_error)
			*r_error = ERR_OUT_OF_MEMORY;
		return nullptr;
	}

	if (LuaScriptLanguage::get_singleton()->get_cache_script(p_original_path) == nullptr) {
		script->set_path(p_original_path, true);
	}
#else
Ref<Resource> LuaScriptResourceFormatLoader::load(const String &p_path, const String &p_original_path, Error *r_error) {
	LuaScript *script = memnew(LuaScript);
	if (!script) {
		if (r_error)
			*r_error = ERR_OUT_OF_MEMORY;
		return nullptr;
	}
	script->set_path(p_original_path, true);
#endif
	script->set_script_path(p_original_path);

	Error error;
	if (!p_path.ends_with(".luac")) {
		error = script->load_source_code(p_path);
		if (error != OK) {
			if (r_error)
				*r_error = error;
			memdelete(script);
			return nullptr;
		}
	}

	error = script->reload();

	if (error != OK) {
		if (r_error)
			*r_error = error;
		memdelete(script);
		return nullptr;
	}

	if (r_error)
		*r_error = OK;

	return Ref<LuaScript>(script);
}

void LuaScriptResourceFormatLoader::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back(LUA_EXTENSION);
	p_extensions->push_back("luac");
}

bool LuaScriptResourceFormatLoader::handles_type(const String &p_type) const {
	return (p_type == SCRIPT_TYPE || p_type == LUA_TYPE);
}

String LuaScriptResourceFormatLoader::get_resource_type(const String &p_path) const {
	List<String> extensions;
	get_recognized_extensions(&extensions);
	return extensions.find(p_path.get_extension().to_lower()) ? LUA_TYPE : EMPTY_STRING;
}
