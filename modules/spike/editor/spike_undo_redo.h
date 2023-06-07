/**
 * spike_undo_redo.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "editor/editor_undo_redo_manager.h"
#include "scene/gui/control.h"
#include "spike_define.h"

class SpikeUndoRedo : public RefCounted {
	GDCLASS(SpikeUndoRedo, RefCounted);

	static HashMap<String, SpikeUndoRedo *> undo_redo_map;

public:
	static void register_undo_redo(const String &p_name, SpikeUndoRedo *undo_redo);
	static void unregister_undo_redo(const String &p_name);
	static SpikeUndoRedo *get_undo_redo(const String &p_name);
	static EditorUndoRedoManager *get_undo_redo_manager(const String &p_name);

protected:
	Control *control;
	EditorUndoRedoManager *undo_redo_manager = nullptr;

public:
	void shortcut_input(const Ref<InputEvent> &p_event);
	SpikeUndoRedo(Control *p_control);
	~SpikeUndoRedo();
};