/**
 * editor_export_defination.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "common_export_plugin.h"
#include "core/io/resource.h"

class EditorExportDefination : public EditorExportSettings {
	GDCLASS(EditorExportDefination, EditorExportSettings);
protected:
    CommonExportPlugin::Settings settings;
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	virtual void apply_settings(CommonExportPlugin &common_export) override;
};
