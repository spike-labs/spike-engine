/**
 * configuration_editor_plugin.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "configuration/configuration_resource.h"
#include "configuration_resource_editor.h"
#include "editor/editor_plugin.h"

class ConfigurationEditorPlugin : public EditorPlugin {
	GDCLASS(ConfigurationEditorPlugin, EditorPlugin);

	ConfigurationResourceEditor *resource_editor;

public:
	virtual String get_name() const override { return "Configuration"; }
	virtual bool has_main_screen() const override { return false; }
	virtual bool handles(Object *p_object) const override;
	virtual void edit(Object *p_object) override;
	virtual void edit_with_patch(Ref<ConfigurationResource> p_source, const String &p_source_path, const String &p_out_patch);
	virtual void make_visible(bool p_visible) override;
	ConfigurationEditorPlugin();
	~ConfigurationEditorPlugin();
};