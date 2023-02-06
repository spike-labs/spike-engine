/* resource_loader_extension.cpp */

#include "core/object/script_language.h"
#include "modules/gdscript/gdscript.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "resource_loader_extension.h"

Ref<Resource> ResourceLoaderExtension::load(const String &p_path) {
	auto load_path = p_path;
	if(p_path.is_relative_path()) {
		ScriptLanguage *script = GDScriptLanguage::get_singleton();
		auto source_path = script->debug_get_stack_level_source(0);
		load_path = source_path.get_base_dir().path_join(p_path).simplify_path();
	}
	return ResourceLoader::load(load_path);
}

void ResourceLoaderExtension::_bind_methods() {
	ClassDB::bind_static_method("ResourceLoaderExtension", D_METHOD("load", "p_path"), &ResourceLoaderExtension::load);
}