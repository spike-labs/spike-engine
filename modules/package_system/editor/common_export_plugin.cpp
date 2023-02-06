/**
 * common_export_plugin.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "common_export_plugin.h"
#include "core/config/project_settings.h"
#include "editor/export/editor_export.h"
#include "scene/resources/resource_format_text.h"

CommonExportPlugin *CommonExportPlugin::singleton = nullptr;

CommonExportPlugin::CommonExportPlugin() {
	singleton = this;
}

CommonExportPlugin::~CommonExportPlugin() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

String CommonExportPlugin::_get_name() const {
	return "CommonExport";
}

void CommonExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	// Search first `export-settings`, and mark this folder searched.
	auto dir_path = p_path.get_base_dir();
	if (!checked_dirs.has(dir_path)) {
		checked_dirs.insert(dir_path);
		Ref<DirAccess> da = DirAccess::open(p_path.get_base_dir());
		if (da.is_valid()) {
			String settings_path;
			da->list_dir_begin();
			for (auto n = da->get_next(); !IS_EMPTY(n); n = da->get_next()) {
				if (!da->current_is_dir() && n.get_extension().to_lower() == EXPORT_SETTINGS_EXT) {
					settings_path = dir_path.path_join(n);
					break;
				}
			}
			da->list_dir_end();
			if (!IS_EMPTY(settings_path)) {
				auto res = ResourceLoader::load(settings_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
				if (res.is_valid()) {
					auto export_settings = Object::cast_to<EditorExportSettings>(res.ptr());
					if (export_settings) {
						export_settings->apply_settings(*this);
					}
				}
			}
		}
	}

	String fit_path;
	Settings settings;
	for (auto E : settings_map) {
		if (p_path.begins_with(E.key) && fit_path.length() < p_path.length()) {
			fit_path = p_path;
			settings = E.value;
		}
	}
	if (!IS_EMPTY(fit_path)) {
		if (settings.platform_exclude == settings.platforms.has(platform)) {
			skip();
		}
	}
}

void CommonExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	settings_map.clear();

	for (size_t i = 0; i < EditorExport::get_singleton()->get_export_platform_count(); i++) {
		auto export_platform = EditorExport::get_singleton()->get_export_platform(i);
		auto platform_name = export_platform->get_os_name().to_lower();
		if (p_features.has(platform_name)) {
			platform = platform_name;
			break;
		}
	}
	debug = p_debug;
	path = p_path;
	flags = p_flags;
}

Error EditorExportSettingsSaver::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	return ResourceFormatSaverText::singleton->save(p_resource, p_path, p_flags);
}

void EditorExportSettingsSaver::get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<EditorExportSettings>(p_resource.ptr())) {
		p_extensions->push_front(EXPORT_SETTINGS_EXT);
	}
}

bool EditorExportSettingsSaver::recognize(const RES &p_resource) const {
	return Object::cast_to<EditorExportSettings>(*p_resource) != nullptr;
}

Ref<Resource> EditorExportSettingsLoader::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	return ResourceFormatLoaderText::singleton->load(p_path, p_original_path, r_error, p_use_sub_threads, r_progress, p_cache_mode);
}

void EditorExportSettingsLoader::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back(EXPORT_SETTINGS_EXT);
}

bool EditorExportSettingsLoader::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, EditorExportSettings::get_class_static());
}

String EditorExportSettingsLoader::get_resource_type(const String &p_path) const {
	return p_path.get_extension().to_lower() == EXPORT_SETTINGS_EXT ? EditorExportSettings::get_class_static() : String();
}
