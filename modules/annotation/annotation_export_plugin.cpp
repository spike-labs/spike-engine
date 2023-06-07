/**
 * annotation_export_plugin.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "annotation_export_plugin.h"
#include "annotation_resource.h"

#include "core/io/json.h"

String AnnotationExportPlugin::annotation_json_path = "res://spike_export_annotations.json";
String AnnotationExportPlugin::module_annotation_json_path = "res://spike_export_module_annotations.json";
const String REMAPED_MODULE_ANNOTATIONS_PATH = "res://.godot/remaped_module_annotations/";

Ref<EditorExportPlugin> AnnotationExportPlugin::create_annotaion_export_plugin() {
	Ref<AnnotationExportPlugin> ret;
	ret.instantiate();
	return ret;
}

void AnnotationExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	export_annos.clear();
	export_module_annos.clear();
	_used_renamed_module_name.clear();
	if (export_main_module_anno.is_valid()) {
		export_main_module_anno.unref();
	}
}

void AnnotationExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	auto anno = AnnotationPlugin::get_annotation_resource_by_path(p_path);
	if (anno.is_null()) {
		// For module annotation.
		if (ResourceLoader::exists(p_path) && ResourceLoader::get_resource_type(p_path) == "ModuleAnnotation") {
			Ref<ModuleAnnotation> res = ResourceLoader::load(p_path, "ModuleAnnotation");
			if (res.is_valid()) {
				export_module_annos.push_back(res);
				// remap
				Ref<ModuleAnnotation> to_export;
				to_export.instantiate();
				to_export->copy_for_export(res);
				// Main packed (only be set by modular system.)
				if (res->has_meta("_main_module_") && res->get_meta("_main_module_").operator bool() == true) {
					export_main_module_anno = to_export;
				}
				// Check duplicated name and print a warning.
				bool need_rename = false;
				for (auto &&module : export_module_annos) {
					if (module->get_module_name() == res->get_module_name() && module->get_path() != res->get_path()) {
						WARN_PRINT(vformat("Module \"%s\" and \"%s\" has the same name: %s, and will be renamed by adding \"_\" as prefix.", module->get_path(), p_path, res->get_module_name()));
						need_rename = true;
						break;
					}
				}
				if (need_rename) {
					if (!_used_renamed_module_name.has(to_export->get_module_name())) {
						_used_renamed_module_name.insert(to_export->get_module_name(), to_export->get_module_name());
					}
					auto last_name = _used_renamed_module_name.get(to_export->get_module_name());
					_used_renamed_module_name[to_export->get_module_name()] = "_" + last_name;
					to_export->set_module_name("_" + last_name);
				}
				auto file_name = p_path.get_file();
				file_name = p_path.md5_text() + "_" + file_name.substr(0, file_name.length() - file_name.get_extension().length()) + "res";
				auto remaped_path = REMAPED_MODULE_ANNOTATIONS_PATH + file_name;
				if (!DirAccess::dir_exists_absolute(REMAPED_MODULE_ANNOTATIONS_PATH)) {
					DirAccess::make_dir_recursive_absolute(REMAPED_MODULE_ANNOTATIONS_PATH);
				}
				Error err = ResourceSaver::save(to_export, remaped_path, ResourceSaver::FLAG_NONE);
				if (err == OK) {
					add_file(remaped_path, FileAccess::get_file_as_bytes(remaped_path), true);
				}
			}
		}

		return;
	}
	if (!export_annos.has(anno->get_class_name())) {
		export_annos.insert(anno->get_class_name(), {});
	}
	export_annos[anno->get_class_name()].push_back(anno);
}

void AnnotationExportPlugin::_export_files_end() {
	if (export_module_annos.size() > 0) {
		add_file(module_annotation_json_path, ModuleAnnotation::get_modules_description_json(export_module_annos, export_main_module_anno).to_utf8_buffer(), false);
	}
	if (export_annos.size() > 0) {
		add_file(annotation_json_path, AnnotationPlugin::to_annotations_json(export_annos).to_utf8_buffer(), false);
	}

	export_annos.clear();
	export_module_annos.clear();
	_used_renamed_module_name.clear();
	if (export_main_module_anno.is_valid()) {
		export_main_module_anno.unref();
	}

	if (DirAccess::dir_exists_absolute(REMAPED_MODULE_ANNOTATIONS_PATH)) {
		for (auto &&file : DirAccess::get_files_at(REMAPED_MODULE_ANNOTATIONS_PATH)) {
			DirAccess::remove_absolute(REMAPED_MODULE_ANNOTATIONS_PATH.path_join(file));
		}
		DirAccess::remove_absolute(REMAPED_MODULE_ANNOTATIONS_PATH);
	}
}