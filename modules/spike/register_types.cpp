/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"

#include "core/version.h"
#include "engine_utils.h"
#include "spike/external_translation_server.h"
#include "spike/version.h"
#include "spike_define.h"

#include INC_CLASS_DB
#ifdef TOOLS_ENABLED
#include "editor/editor_plugin.h"
#include "editor/resource_recognizer.h"
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
#include "core/config/project_settings.h"
#include "servers/display_server_universal.h"
#include "spike/external_translation_server.h"

class ModSpikeEngine : public SpikeModule {
public:
	static void core(bool do_init) {
		if (do_init) {
			GDREGISTER_ABSTRACT_CLASS(EngineUtils);
		}
	}

	static void servers(bool do_init) {
		if (do_init) {
			String gl_skinning = "rendering/gl_compatibility/skinning";
			GLOBAL_DEF_RST(gl_skinning, 0);
			ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::INT, gl_skinning, PROPERTY_HINT_ENUM, "Default,Software Skinning"));
			DisplayServerUniversal::override_create_func();
			GDREGISTER_ABSTRACT_CLASS(ExternalTranslationServer);
		} else {
		}
	}

	static void scene(bool do_init) {
		if (do_init) {

		} else {
			
		}
	}

#ifdef TOOLS_ENABLED
	static void editor(bool do_init) {
		if (do_init) {
			GDREGISTER_ABSTRACT_CLASS(EditorUtils);
			EditorNode::add_init_callback(&spike_editor_init);
		} else {
			REMOVE_FORMAT_LOADER(final_loader4uid);
		}
	}
#endif
};

IMPL_SPIKE_MODULE(spike, ModSpikeEngine)

extern Object *create_godot_object(const void *p_class) {
	if (EditorNode::get_class_ptr_static() == p_class) {
		return memnew(SpikeEditorNode);
	}

	if (TranslationServer::get_class_ptr_static() == p_class) {
		return memnew(ExternalTranslationServer);
	}

	if (ProjectManager::get_class_ptr_static() == p_class) {
		return memnew(SpikeProjectManager);
	}

	return nullptr;
}

extern String get_version_build() {
	return String(SPIKE_FULL_BUILD);
}
