/**
 * modular_data_inspector_plugin.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "annotation/annotation_plugin.h"
#ifdef TOOLS_ENABLED

#include "editor/editor_inspector.h"
#include "modular_system/editor/modular_override_utils.h"
#include "scene/gui/dialogs.h"

class ModularResource;
class Tree;
class TreeItem;
class EditorFileSystemDirectory;
class CheckButton;
class ModularResourcesSelectDialog : public ConfirmationDialog {
private:
	const Vector<String> folders_to_ignore;
	const Vector<String> files_to_ignore;

	Ref<ModularResource> data;
	Tree *select_tree = nullptr;

	CheckButton *show_hidden_btn = nullptr;
	CheckButton *show_recognized_resources_only_btn = nullptr;
	LineEdit *filter = nullptr;
	LineEdit *exclude = nullptr;

	RichTextLabel *hidden_files_hint = nullptr;

	void _select_tree_item_edited();
	void _confirm_pressed();
	void _cancel_pressed();

	void _show_hidden_pressed();
	void _show_recognized_resources_only_pressed();
	void _filter_changed(const String &p_new_text);
	void _exclude_changed(const String &p_new_text);

	void _collapse_tree();
	void _expand_tree();

	void refresh();

	bool refresh_recursively(TreeItem *p_item, bool p_show_hidden, bool p_show_resources_only, const Vector<String> &p_filters, const Vector<String> &p_excludes);
	void setup_tree_recursively(TreeItem *p_tree_item, const String &p_dir);
	void set_item_checked_recursively(TreeItem *p_item, bool p_checked);

	Vector<String> selected_files;

	friend class ModularDataInspectorPlugin;

	static ModularResourcesSelectDialog *singleton;

public:
	static ModularResourcesSelectDialog *get_singleton() { return singleton; }

	void setup_and_popup(Ref<ModularResource> &p_data);

	ModularResourcesSelectDialog();
	~ModularResourcesSelectDialog();
};

class ModularResourcesEditorProperty : public EditorProperty {
private:
	class Button *select_btn = nullptr;

	friend class ModularDataInspectorPlugin;

	void _select_btn_pressed();

public:
	ModularResourcesEditorProperty();
	virtual void update_property() override;
};

class ModularDataInspectorPlugin : public EditorInspectorPlugin {
public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;

	void init(Control *p_editor_modular_interface) { p_editor_modular_interface->add_child(memnew(ModularResourcesSelectDialog)); }

	static ModularDataInspectorPlugin *singleton;
	static ModularDataInspectorPlugin *get_singleton() { return singleton; }
	void inspect_requested(const Ref<class ModularResource> &p_data);
};

#endif