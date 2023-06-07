/**
 * configuration_resource_editor.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "configuration_resource_editor.h"
#include "configuration_set_inspector.h"
#include "configuration_table_inspector.h"
#include "core/variant/callable.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/filesystem_dock.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"

void ConfigurationResourceEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			EditorNode::get_singleton()->connect("resource_saved", callable_mp(this, &ConfigurationResourceEditor::_resource_saved));
			FileSystemDock::get_singleton()->connect("files_moved", callable_mp(this, &ConfigurationResourceEditor::_files_moved));
			FileSystemDock::get_singleton()->connect("file_removed", callable_mp(this, &ConfigurationResourceEditor::_file_removed));
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				set_process_shortcut_input(true);
				grab_focus();
			} else {
				set_process_shortcut_input(false);
			}
		} break;
	}
}

void ConfigurationResourceEditor::_selected_opened_file(const int &p_idx, VBoxContainer *p_container) {
	editing_resource = opened_files[p_idx];
	if (editing_resource.is_valid()) {
		if (editing_resource->is_class_ptr(ConfigurationSet::get_class_ptr_static())) {
			if (nullptr != table_inspector) {
				table_inspector->set_visible(false);
			}
			if (nullptr == set_inspector) {
				set_inspector = CREATE_NODE(p_container, ConfigurationSetInspector);
				set_inspector->connect("new_modification", callable_mp(this, &ConfigurationResourceEditor::_new_modification));
				set_inspector->set_v_size_flags(Control::SIZE_EXPAND_FILL);
			}
			set_inspector->set_visible(true);
			set_inspector->edit(editing_resource);
			sectioned_inspector = set_inspector;
		} else if (editing_resource->is_class_ptr(ConfigurationTable::get_class_ptr_static())) {
			if (nullptr != set_inspector) {
				set_inspector->set_visible(false);
			}
			if (nullptr == table_inspector) {
				table_inspector = CREATE_NODE(p_container, ConfigurationTableInspector);
				table_inspector->connect("new_modification", callable_mp(this, &ConfigurationResourceEditor::_new_modification));
				table_inspector->set_v_size_flags(Control::SIZE_EXPAND_FILL);
			}
			table_inspector->set_visible(true);
			table_inspector->edit(editing_resource);
			sectioned_inspector = table_inspector;
		}
		sectioned_inspector->set_focus_mode(FocusMode::FOCUS_ALL);
		sectioned_inspector->grab_focus();
		patched_mode->set_visible(editing_resource->is_patch);
	}
}

void ConfigurationResourceEditor::_close_opened_file(const Ref<ConfigurationResource> &p_resource) {
	COND_MET_RETURN(!p_resource.is_valid());
	int idx = opened_files.find(p_resource);
	if (idx >= 0) {
		opened_files.remove_at(idx);
		opened_files_pathes.remove_at(idx);
		item_list_hsplit->remove_item(idx);
		if (p_resource == editing_resource) {
			if (opened_files.size() > 0) {
				edit(opened_files[0]);
				return;
			}
			patched_mode->set_visible(false);
			if (sectioned_inspector) {
				sectioned_inspector->set_visible(false);
				sectioned_inspector = nullptr;
			}
			editing_resource = nullptr;
		}
	}
}

void ConfigurationResourceEditor::_update_opened_file(const Ref<ConfigurationResource> &p_resource) {
	for (int i = 0; i < opened_files.size(); i++) {
		if (opened_files[i] == p_resource) {
			const String path = p_resource->get_path();
			opened_files_pathes.set(i, path);
			item_list_hsplit->get_item_list()->set_item_text(i, path.get_file().get_basename());
			item_list_hsplit->get_item_list()->set_item_tooltip(i, path);
			break;
		}
	}
}

void ConfigurationResourceEditor::_new_modification() {
	int index = opened_files.find(editing_resource);
	if (index >= 0) {
		String path = editing_resource->get_path();
		if (!path.is_empty()) {
			item_list_hsplit->get_item_list()->set_item_text(index,
					"(*)" + path.get_file().get_basename());
		}
	}
}

void ConfigurationResourceEditor::edit(Ref<ConfigurationResource> p_resource) {
	int index = opened_files.find(p_resource);
	if (index == -1) {
		String path = p_resource->get_path();
		opened_files.insert(0, p_resource);
		opened_files_pathes.insert(0, path);
		String name = path.get_file().get_basename();
		if (name.is_empty()) {
			name = "(*)" + TTR("[unsaved]");
		}
		String tooltip = path;
		if (tooltip.is_empty()) {
			tooltip = TTR("Unsaved file.");
		}
		item_list_hsplit->insert_item(0, name, nullptr, tooltip);
	} else {
		opened_files.remove_at(index);
		opened_files_pathes.remove_at(index);
		opened_files.insert(0, p_resource);
		opened_files_pathes.insert(0, p_resource->get_path());
		item_list_hsplit->move_item(index, 0);
	}
	item_list_hsplit->select_item(0);
	set_process_shortcut_input(true);
}

void ConfigurationResourceEditor::shortcut_input(const Ref<InputEvent> &p_event) {
	if (nullptr != undo_redo) {
		undo_redo->shortcut_input(p_event);
	}
}

void ConfigurationResourceEditor::_menu_option(const int &p_option) {
	switch (p_option) {
		case C_FILE_NEW_SET: {
			Ref<ConfigurationSet> new_set;
			new_set.instantiate();
			edit(new_set);
		} break;

		case C_FILE_NEW_TABLE: {
			Ref<ConfigurationTable> new_table;
			new_table.instantiate();
			edit(new_table);
		} break;

		case C_FILE_OPEN: {
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
			file_dialog_option = C_FILE_OPEN;
			file_dialog->clear_filters();
			file_dialog->add_filter("*.tres");
			file_dialog->add_filter("*.res");
			file_dialog->popup_file_dialog();
			file_dialog->set_title(TTR("Open File"));
		} break;

		case C_FILE_SAVE: {
			if (editing_resource.is_valid()) {
				EditorNode::get_singleton()->save_resource(editing_resource);
			}
		} break;

		case C_FILE_SAVE_AS: {
			if (editing_resource.is_valid()) {
				EditorNode::get_singleton()->save_resource_as(editing_resource);
			}
		} break;

		case C_FILE_CLOSE: {
			if (editing_resource.is_valid()) {
				_close_opened_file(editing_resource);
			}
		} break;

		case C_CLOSE_ALL: {
			opened_files.clear();
			item_list_hsplit->get_item_list()->clear();
			patched_mode->set_visible(false);
			if (sectioned_inspector) {
				sectioned_inspector->set_visible(false);
				sectioned_inspector = nullptr;
			}
			editing_resource = nullptr;
		} break;

		case C_CLOSE_OTHER_TABS: {
			opened_files.clear();
			item_list_hsplit->get_item_list()->clear();
			edit(editing_resource);
		} break;
	}
}

void ConfigurationResourceEditor::_file_dialog_action(const String &p_file) {
	switch (file_dialog_option) {
		case C_FILE_OPEN: {
			Ref<ConfigurationResource> resource = ResourceLoader::load(p_file);
			if (!resource.is_valid()) {
				EditorNode::get_singleton()->show_warning(TTR("Could not load file at:") + "\n\n" + p_file, TTR("Error!"));
				return;
			}
			edit(resource);
			file_dialog_option = -1;
		} break;
	}
}

void ConfigurationResourceEditor::_resource_saved(Ref<ConfigurationResource> p_resource) {
	_update_opened_file(p_resource);
}

void ConfigurationResourceEditor::_files_moved(const String &p_old_file, const String &p_new_file) {
	int idx = opened_files_pathes.find(p_old_file);
	if (idx >= 0) {
		Ref<ConfigurationResource> res = opened_files[idx];
		res->set_path(p_new_file);
		_update_opened_file(res);
	}
}

void ConfigurationResourceEditor::_file_removed(const String &p_removed_file) {
	Ref<ConfigurationResource> removed_file;
	for (int i = 0; i < opened_files_pathes.size(); i++) {
		if (opened_files_pathes[i] == p_removed_file) {
			removed_file = opened_files[i];
			break;
		}
	}
	_close_opened_file(removed_file);
}

ConfigurationResourceEditor::ConfigurationResourceEditor() {
	set_name("ConfigurationResourceEditor");
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_focus_mode(FocusMode::FOCUS_ALL);

	HBoxContainer *h_menu_container = CREATE_NODE(this, HBoxContainer);
	h_menu_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	file_menu = CREATE_NODE(h_menu_container, MenuButton);

	file_menu->set_text(TTR("File"));
	file_menu->set_switch_on_hover(true);
	file_menu->set_shortcut_context(this);
	file_menu->get_popup()->connect("id_pressed", callable_mp(this, &ConfigurationResourceEditor::_menu_option));

	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("configuration_editor/new_set", STTR("New Configuration Set..."), KeyModifierMask::CMD_OR_CTRL | Key::N), C_FILE_NEW_SET);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("configuration_editor/new_table", TTR("New Configuration Table..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::N), C_FILE_NEW_TABLE);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("configuration_editor/open", TTR("Open...")), C_FILE_OPEN);

	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("configuration_editor/save", TTR("Save"), KeyModifierMask::CMD_OR_CTRL | Key::S), C_FILE_SAVE);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("configuration_editor/save_as", TTR("Save As..."), KeyModifierMask::SHIFT | KeyModifierMask::CMD_OR_CTRL | Key::S), C_FILE_SAVE_AS);

	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("configuration_editor/close_file", TTR("Close"), KeyModifierMask::CMD_OR_CTRL | Key::W), C_FILE_CLOSE);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("configuration_editor/close_all", TTR("Close All")), C_CLOSE_ALL);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("configuration_editor/close_other_tabs", TTR("Close Other Tabs")), C_CLOSE_OTHER_TABS);

	item_list_hsplit = CREATE_NODE(this, ItemListHSplit);
	item_list_hsplit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	item_list_hsplit->connect("item_selected", callable_mp(this, &ConfigurationResourceEditor::_selected_opened_file));

	patched_mode = CREATE_NODE(item_list_hsplit->get_item_editor_container(), StatusPrompt);
	patched_mode->set_status(StatusPrompt::Status::Warning);
	patched_mode->set_text(TTR("Currently in patch mode."));
	patched_mode->set_visible(false);

	PopupMenu *context_menu = item_list_hsplit->get_context_menu();
	context_menu->connect("id_pressed", callable_mp(this, &ConfigurationResourceEditor::_menu_option));
	context_menu->add_shortcut(ED_GET_SHORTCUT("configuration_editor/save"), C_FILE_SAVE);
	context_menu->add_shortcut(ED_GET_SHORTCUT("configuration_editor/save_as"), C_FILE_SAVE_AS);
	context_menu->add_shortcut(ED_GET_SHORTCUT("configuration_editor/close_file"), C_FILE_CLOSE);
	context_menu->add_shortcut(ED_GET_SHORTCUT("configuration_editor/close_all"), C_CLOSE_ALL);
	context_menu->add_shortcut(ED_GET_SHORTCUT("configuration_editor/close_other_tabs"), C_CLOSE_OTHER_TABS);

	file_dialog = CREATE_NODE(this, EditorFileDialog);
	file_dialog->connect("file_selected", callable_mp(this, &ConfigurationResourceEditor::_file_dialog_action));

	undo_redo = memnew(SpikeUndoRedo(this));
	SpikeUndoRedo::register_undo_redo("Configuration", undo_redo);
}

ConfigurationResourceEditor::~ConfigurationResourceEditor() {
	SpikeUndoRedo::unregister_undo_redo("Configuration");
	memdelete(undo_redo);
}
