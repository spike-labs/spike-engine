/**
 * material_editor_inspector.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "editor/editor_file_dialog.h"
#include "editor/editor_inspector.h"
#include "editor/editor_properties.h"
#include "editor/editor_quick_open.h"
#include "scene/gui/menu_button.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"

class SpikeInspectorPluginMaterial : public EditorInspectorPlugin {
	GDCLASS(SpikeInspectorPluginMaterial, EditorInspectorPlugin)

	MenuButton *prop;
	EditorQuickOpen *quick_open;
	EditorFileDialog *save_dialog;
	AcceptDialog *warn_dialog;

	List<StringName> material_types;
	String current_type;
	Shader *shader;
	Material *material;

	enum WarnCB {
		OPEN_SAVE,
	};

protected:
	void _about_to_popup();
	void _material_type_pressed(int id);
	void _shader_quick_selected(EditorQuickOpen *quick_open);
	void _warning_confirmed();
	void _open_save_dialog();
	void _save_path_selected(const String &p_path);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class EditorPropertyPopup : public EditorProperty {
	GDCLASS(EditorPropertyPopup, EditorProperty);

private:
	MenuButton *menu;

public:
	void setup(const String &title) {
		menu->set_text(title);
	}
	MenuButton *get_menu() {
		return menu;
	}
	PopupMenu *get_popup() {
		return menu->get_popup();
	}
	EditorPropertyPopup();
};
