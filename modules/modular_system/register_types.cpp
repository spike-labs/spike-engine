/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"

#include "configuration/configuration_resource.h"
#include "configuration/configuration_server.h"
#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/string/translation.h"
#include "modular_graph_loader.h"
#include "modular_launcher.h"
#include "translation/translation_patch_server.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/modular_editor_plugin.h"
#include "editor/modular_export_plugin.h"
#include "editor/modular_graph.h"
#include "editor/modular_importer.h"
#include "editor/modular_exporter.h"
#include "modular_system/editor/modular_override_utils.h"
#include "spike/editor/spike_editor_node.h"

static Ref<ModularExportPlugin> modular_export_plugin;

static void _scan_and_init_modular_graph_recursively(const String &p_dir = "res://") {
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return;
	}

	for (const String &f : dir->get_files()) {
		const String file_path = p_dir.path_join(f);
		if (!ClassDB::is_parent_class(
					ResourceLoader::get_resource_type(file_path),
					ModularGraph::get_class_static())) {
			continue;
		}
		Ref<ModularGraph> mg = ResourceLoader::load(file_path);
		if (mg.is_valid()) {
			mg->init_packed_file_providers();
		}
	}

	for (const String &subdir : dir->get_directories()) {
		_scan_and_init_modular_graph_recursively(p_dir.path_join(subdir));
	}
}

static void _editor_init() {
	REF_INSTANTIATE(modular_export_plugin);

	SpikeEditorNode::get_instance()->add_main_screen_editor_plugin(memnew(ModularEditorPlugin), 3);

	_scan_and_init_modular_graph_recursively();
}
#endif

static void _set_launch_with_builtin_modular_launcher(const String &p_main_scene) {
	FileSystemServer::get_singleton()->add_provider(LauncherProvider::create(p_main_scene));

	auto autoloads = ProjectSettings::get_singleton()->get_autoload_list();
	for (auto &autoload : autoloads) {
		ProjectSettings::get_singleton()->remove_autoload(autoload.key);
	}
	ModularLauncher::autoloads = new (HashMap<StringName, ProjectSettings::AutoloadInfo>);
	*ModularLauncher::autoloads = autoloads;
}

class ModularModule : public SpikeModule {
public:
	static void core(bool do_init) {
		if (do_init) {
#ifdef TOOLS_ENABLED
			GDREGISTER_ABSTRACT_CLASS(ModularNode);
			GDREGISTER_CLASS(ModularOutputNode);
			GDREGISTER_CLASS(ModularCompositeNode);
			GDREGISTER_CLASS(ModularGraph);
			GDREGISTER_CLASS(ModularData);
			GDREGISTER_CLASS(ModularResource);
			GDREGISTER_CLASS(ModularPackedFile);
#endif
		}
	}

	static void scene(bool do_init) {
		if (do_init) {
			GDREGISTER_CLASS(ModularLauncher);

#ifdef TOOLS_ENABLED
			if (Engine::get_singleton()->is_editor_hint()) {
				return;
			}
#endif

#ifdef TOOLS_ENABLED
			auto main_args = OS::get_singleton()->get_cmdline_args();
			String preffix = LAUNCH_MODULAR_PRE;
			for (auto const &arg : main_args) {
				if (arg.begins_with(preffix)) {
					String modular_file = arg.substr(preffix.length());
					PackedStringArray modular_args = modular_file.split("#", false);
					if (modular_args.size() > 2) {
						_set_launch_with_builtin_modular_launcher(modular_args[2]);
						return;
					}
					break;
				}
			}
#endif
			if (!IS_EMPTY(RunGraphHandler::find_modular_json_path())) {
				String main_scene = GLOBAL_GET("application/run/main_scene");
				ProjectSettings::get_singleton()->set("application/run/main_scene", DEFAULT_LAUNCHER_PATH);
				_set_launch_with_builtin_modular_launcher(main_scene);
			}
		}
	}

#ifdef TOOLS_ENABLED
	static void editor(bool do_init) {
		if (do_init) {
			GDREGISTER_ABSTRACT_CLASS(ModularMenuNode);
			GDREGISTER_CLASS(ModularImporter);
			GDREGISTER_CLASS(ModularExporter);
			EditorNode::add_init_callback(&_editor_init);
		} else {
			if (modular_export_plugin.is_valid()) {
				modular_export_plugin.unref();
			}
		}
	}
#endif
};

IMPL_SPIKE_MODULE(modular_system, ModularModule)
