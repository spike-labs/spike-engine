/**
 * editor_export_defination.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "editor_export_defination.h"
#include "editor/export/editor_export.h"

bool EditorExportDefination::_set(const StringName &p_name, const Variant &p_value) {
	String strname = p_name;
	if (strname != "exclude" && !strname.begins_with("platform_")) {
		return false;
	}

	auto key = get_path().get_base_dir();
	if (strname == "exclude") {
		bool bvalue = p_value;
		if (settings.platform_exclude != bvalue) {
			settings.platform_exclude = bvalue;

			for (size_t i = 0; i < EditorExport::get_singleton()->get_export_platform_count(); i++) {
				auto export_platform = EditorExport::get_singleton()->get_export_platform(i);
				auto platform_name = export_platform->get_os_name().to_lower();
				if (settings.platforms.has(platform_name)) {
					settings.platforms.erase(platform_name);
				} else {
					settings.platforms.insert(platform_name);
				}
			}
		}
	} else {
		strname = strname.substr(strlen("platform_"));
		if (p_value) {
			settings.platforms.insert(strname);
		} else {
			settings.platforms.erase(strname);
		}
	}
	return true;
}

bool EditorExportDefination::_get(const StringName &p_name, Variant &r_ret) const {
	String strname = p_name;
	if (strname != "exclude" && !strname.begins_with("platform_")) {
		return false;
	}

	if (strname == "exclude") {
		r_ret = settings.platform_exclude;
	} else {
		strname = strname.substr(strlen("platform_"));
		r_ret = settings.platforms.has(strname);
	}

	return true;
}

void EditorExportDefination::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::BOOL, PNAME("exclude")));
	p_list->push_back(PropertyInfo(Variant::NIL, "Platforms", PROPERTY_HINT_NONE, "platform_", PROPERTY_USAGE_GROUP));

	if (EditorExport::get_singleton()) {
		for (size_t i = 0; i < EditorExport::get_singleton()->get_export_platform_count(); i++) {
			auto export_platform = EditorExport::get_singleton()->get_export_platform(i);
			auto platform_name = export_platform->get_os_name().to_lower();
			p_list->push_back(PropertyInfo(Variant::BOOL, "platform_" + platform_name));
		}
	}
}

void EditorExportDefination::apply_settings(CommonExportPlugin &common_export) {
	common_export.update_settings(get_path().get_base_dir(), settings);
}
