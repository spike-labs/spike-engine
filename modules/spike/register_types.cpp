/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"

#include "configuration/resource_format_configurable.h"
#include "core/config/project_settings.h"
#include "core/version.h"
#include "engine_utils.h"
#include "servers/display_server_universal.h"
#include "spike/version.h"
#include "spike_define.h"
#include "translation/translation_patch_server.h"
#include INC_CLASS_DB

/******************************* EDITOR INC *******************************/
#ifdef TOOLS_ENABLED
#include "editor/account/account_manage.h"
#include "editor/editor_plugin.h"
#include "editor/spike_editor_node.h"
#include "editor/spike_editor_utils.h"
#include "editor/spike_format_loader.h"
#include "editor/spike_project_manager.h"

/// To be the last `ResourceFormatLoader`, auto generate uid for Resource like `GDScript`, `Shader` ...
static Ref<SpikeFormatLoader> final_loader4uid;

static void spike_editor_init() {
	REF_INSTANTIATE(final_loader4uid);
	ResourceLoader::add_resource_format_loader(final_loader4uid);
}

#endif

class ModSpikeEngine : public SpikeModule {
public:
	static void core(bool do_init) {
		if (do_init) {
			MEM_REPLACE_OBJ(ResourceFormatLoaderBinary::singleton, ResourceFormatLoaderBinary, OverrideFormatLoaderBinary);
			GDREGISTER_ABSTRACT_CLASS(EngineUtils);
		}
	}

	static void servers(bool do_init) {
		if (do_init) {
			String gl_skinning = "rendering/gl_compatibility/skinning";
			GLOBAL_DEF_RST(gl_skinning, 0);
			ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::INT, gl_skinning, PROPERTY_HINT_ENUM, "Default,Software Skinning"));
			DisplayServerUniversal::override_create_func();
		} else {
		}
	}

	static void scene(bool do_init) {
		if (do_init) {
			// ClassDB::register_class<TransposedTexture>();
			MEM_REPLACE_OBJ(ResourceFormatLoaderText::singleton, ResourceFormatLoaderText, OverrideFormatLoaderText);
		} else {
		}
	}

#ifdef TOOLS_ENABLED
	static void editor(bool do_init) {
		if (do_init) {
			GDREGISTER_ABSTRACT_CLASS(EditorUtils);
			GDREGISTER_CLASS(AccountManage);
			EditorNode::add_init_callback(&spike_editor_init);
		} else {
			REMOVE_FORMAT_LOADER(final_loader4uid);
		}
	}
#endif
};

IMPL_SPIKE_MODULE(spike, ModSpikeEngine)

extern Object *create_godot_object(const void *p_class) {
#ifdef TOOLS_ENABLED
	if (EditorNode::get_class_ptr_static() == p_class) {
		return memnew(SpikeEditorNode);
	}

	if (ProjectManager::get_class_ptr_static() == p_class) {
		return memnew(SpikeProjectManager);
	}
#endif

	if (TranslationServer::get_class_ptr_static() == p_class) {
		return memnew(TranslationPatchServer);
	}

	return nullptr;
}

extern String get_version_build() {
	return String(SPIKE_FULL_BUILD);
}
