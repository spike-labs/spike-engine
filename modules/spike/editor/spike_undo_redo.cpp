/**
 * spike_undo_redo.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "spike_undo_redo.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "scene/main/viewport.h"

HashMap<String, SpikeUndoRedo *> SpikeUndoRedo::undo_redo_map;

void SpikeUndoRedo::register_undo_redo(const String &p_name, SpikeUndoRedo *undo_redo) {
	if (!undo_redo_map.has(p_name)) {
		undo_redo_map[p_name] = undo_redo;
	}
}
void SpikeUndoRedo::unregister_undo_redo(const String &p_name) {
	if (undo_redo_map.has(p_name)) {
		undo_redo_map.erase(p_name);
	}
}

SpikeUndoRedo *SpikeUndoRedo::get_undo_redo(const String &p_name) {
	if (undo_redo_map.has(p_name)) {
		return undo_redo_map[p_name];
	}
	return nullptr;
}

EditorUndoRedoManager *SpikeUndoRedo::get_undo_redo_manager(const String &p_name) {
	SpikeUndoRedo *undo_redo = get_undo_redo(p_name);
	if (nullptr != undo_redo) {
		return undo_redo->undo_redo_manager;
	}
	return nullptr;
}

void SpikeUndoRedo::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());
	ERR_FAIL_COND(nullptr == undo_redo_manager);
	Viewport *viewport = control->get_viewport();
	COND_MET_RETURN(nullptr == viewport);
	viewport->get_base_window();
	Control *focus = viewport->gui_get_focus_owner();
	COND_MET_RETURN(nullptr == focus);
	COND_MET_RETURN(focus != control && !focus->find_parent(control->get_class_name()));
	const Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed()) {
		bool handled = false;

		if (ED_IS_SHORTCUT("ui_undo", p_event)) {
			String action = undo_redo_manager->get_current_action_name();
			if (!action.is_empty()) {
				EditorNode::get_log()->add_message("Undo: " + action, EditorLog::MSG_TYPE_EDITOR);
			}
			undo_redo_manager->undo();
			handled = true;
		}

		if (ED_IS_SHORTCUT("ui_redo", p_event)) {
			undo_redo_manager->redo();
			String action = undo_redo_manager->get_current_action_name();
			if (!action.is_empty()) {
				EditorNode::get_log()->add_message("Redo: " + action, EditorLog::MSG_TYPE_EDITOR);
			}
			handled = true;
		}

		if (handled) {
			viewport->set_input_as_handled();
		}
	}
}

SpikeUndoRedo::SpikeUndoRedo(Control *p_control) {
	control = p_control;
	if (nullptr != EditorUndoRedoManager::get_singleton()) {
		undo_redo_manager = memnew(EditorUndoRedoManager);
	}
}

SpikeUndoRedo::~SpikeUndoRedo() {
	memdelete(undo_redo_manager);
}