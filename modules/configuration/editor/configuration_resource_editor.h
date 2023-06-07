/**
 * configuration_resource_editor.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "configuration/configuration_resource.h"
#include "configuration_set_inspector.h"
#include "configuration_table_inspector.h"
#include "editor/editor_file_dialog.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/menu_button.h"
#include "spike/editor/inspector/sectioned_tree.h"
#include "spike/editor/item_list_split.h"
#include "spike/editor/spike_undo_redo.h"
#include "spike/editor/status_prompt.h"

class ConfigurationResourceEditor : public VBoxContainer {
	GDCLASS(ConfigurationResourceEditor, VBoxContainer);

	enum {
		C_FILE_NEW,
		C_FILE_NEW_SET,
		C_FILE_NEW_TABLE,
		C_FILE_OPEN,
		C_FILE_SAVE,
		C_FILE_SAVE_AS,
		C_FILE_SAVE_ALL,
		C_FILE_CLOSE,
		C_CLOSE_ALL,
		C_CLOSE_OTHER_TABS,
	};

private:
	Ref<ConfigurationResource> editing_resource;
	MenuButton *file_menu = nullptr;
	ItemListHSplit *item_list_hsplit = nullptr;
	StatusPrompt *patched_mode = nullptr;
	ConfigurationSetInspector *set_inspector = nullptr;
	ConfigurationTableInspector *table_inspector = nullptr;
	Control *sectioned_inspector = nullptr;
	SpikeUndoRedo *undo_redo;
	EditorFileDialog *file_dialog = nullptr;
	int file_dialog_option = -1;

protected:
	Vector<String> opened_files_pathes;
	Vector<Ref<ConfigurationResource>> opened_files;

	void _resource_saved(Ref<ConfigurationResource> p_resource);
	void _files_moved(const String &p_old_file, const String &p_new_file);
	void _file_removed(const String &p_removed_file);

	void _selected_opened_file(const int &p_item_idx, VBoxContainer *p_container);
	void _close_opened_file(const Ref<ConfigurationResource> &p_resource);
	void _update_opened_file(const Ref<ConfigurationResource> &p_resource);

	void _menu_option(const int &p_option);
	void _file_dialog_action(const String &p_file);
	void _new_modification();
	void _notification(int p_what);

	void shortcut_input(const Ref<InputEvent> &p_event) override;

public:
	void edit(Ref<ConfigurationResource> p_resource);

	ConfigurationResourceEditor();
	~ConfigurationResourceEditor();
};