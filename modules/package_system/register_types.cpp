/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"
#include "native_wrapper.h"
#include INC_CLASS_DB

#ifdef TOOLS_ENABLED
#include "editor/common_export_plugin.h"
#include "editor/editor_export_defination.h"
#include "editor/editor_node.h"
#include "editor/editor_package_dialog.h"
#include "editor/editor_package_library.h"
#include "editor/editor_package_system.h"
#include "editor/modular_package.h"
#include "editor/native_wrapper_export_plugin.h"
#include "editor/plugins/asset_library_editor_plugin.h"

static Ref<EditorExportSettingsLoader> editor_export_settings_loader;
static Ref<EditorExportSettingsSaver> editor_export_settings_saver;

static void _editor_init() {
	Ref<NativeWrapperExportPlugin> export_plugin;
	REF_INSTANTIATE(export_plugin);
	EditorExport::get_singleton()->add_export_plugin(export_plugin);

	Ref<CommonExportPlugin> common_export;
	REF_INSTANTIATE(common_export);
	EditorExport::get_singleton()->add_export_plugin(common_export);

	auto eps = memnew(EditorPackageSystem());
	EditorNode::get_singleton()->add_child(eps);

	if (AssetLibraryEditorPlugin::is_available()) {
		auto children = EditorNode::get_singleton()->find_children("", AssetLibraryEditorPlugin::get_class_static(), false, false);
		if (children.size()) {
			EditorNode::remove_editor_plugin(Object::cast_to<EditorPlugin>(children[0]));
			memdelete((Object *)children[0]);
		}
	}
	EditorNode::add_editor_plugin(memnew(PackageLibraryEditorPlugin));
}
#endif

static Ref<NativeWrapperResourceLoader> native_wrapper_loader;
static Ref<NativeWrapperResourceSaver> native_wrapper_saver;

class MODPackageSystem : public SpikeModule {
public:
	static void core(bool do_init) {
		if (do_init) {
			GDREGISTER_CLASS(NativeWrapper);
			ADD_FORMAT_LOADER(native_wrapper_loader);
			ADD_FORMAT_SAVER(native_wrapper_saver);
#ifdef MODULE_MODULAR_SYSTEM_ENABLED
#ifdef TOOLS_ENABLED
			GDREGISTER_CLASS(ModularPackage);
#endif
#endif
		} else {
			REMOVE_FORMAT_LOADER(native_wrapper_loader);
			REMOVE_FORMAT_SAVER(native_wrapper_saver);
		}
	}
#ifdef TOOLS_ENABLED
	static void editor(bool do_init) {
		if (do_init) {
			GDREGISTER_ABSTRACT_CLASS(EditorExportSettings);
			GDREGISTER_CLASS(EditorExportDefination);
			GDREGISTER_CLASS(EditorPackageSystem);
			EditorNode::add_init_callback(_editor_init);

			ADD_FORMAT_LOADER(editor_export_settings_loader);
			ADD_FORMAT_SAVER(editor_export_settings_saver);
		} else {
			REMOVE_FORMAT_LOADER(editor_export_settings_loader);
			REMOVE_FORMAT_SAVER(editor_export_settings_saver);
		}
	}
#endif
};

IMPL_SPIKE_MODULE(package_system, MODPackageSystem)
