/**
 * configuration_set_inspector.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "configuration/configuration_set.h"
#include "editor/editor_inspector.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"

class EditorUndoRedoManager;
class ConfigurationSetEditorProperty;
class ConfigurationSetInspector : public VBoxContainer {
	GDCLASS(ConfigurationSetInspector, VBoxContainer);

protected:
	static void _bind_methods();

protected:
	Ref<ConfigurationSet> resource;

	HashMap<StringName, ConfigurationSetEditorProperty *> editor_property_map;
	ConfigurationSetEditorProperty *property_selected = nullptr;

	EditorUndoRedoManager *undo_redo_mgr;

	HBoxContainer *add_container;
	LineEdit *name_edit = nullptr;
	OptionButton *type_option = nullptr;
	Button *add_button = nullptr;
	Label *prompt_label = nullptr;

	ScrollContainer *scroll_container;
	VBoxContainer *scroll_content;

	EditorInspector *inspector = nullptr;

	void _emit_new_modification() { emit_signal("new_modification"); }

	void _property_selected(const String &p_path, int p_focusable);
	void _property_changed(const String &p_path, const Variant &p_value, const String &p_name, bool p_changing);
	void _property_deleted(const String &p_path);

	void _notification(int p_what);

	void _create_property_editor(const int &p_index, const String &p_path, const Variant &p_value);

	void _create_new_config();
	void _do_add_config(const String &p_config_name, const Variant &p_value);
	void _do_insert_config(const int &p_index, const String &p_path, const Variant &p_value);
	void _do_remove_config(const String &p_config_name);

	void _name_edit_text_changed(const String &p_text);

public:
	void edit(const Ref<ConfigurationSet> &p_resource);

	ConfigurationSetInspector();
	~ConfigurationSetInspector();
};