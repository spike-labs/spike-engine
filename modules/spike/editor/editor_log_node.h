/**
 * editor_log_node.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#ifndef SPIKE_EDITOR_DEBUGGER_NODE_H
#define SPIKE_EDITOR_DEBUGGER_NODE_H

#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_log.h"
#include "editor/plugins/script_editor_plugin.h"

class EditorNode;

class SpikeEditorLogNode : public EditorLog {
	GDCLASS(SpikeEditorLogNode, EditorLog);

public:
protected:
	void _line_clicked(Variant p_line);
	virtual void _add_log_line(LogMessage &p_message, bool p_replace_previous = false) override;

public:
	static void _bind_methods();
	SpikeEditorLogNode(EditorNode *p_editor);
	~SpikeEditorLogNode();
};

#endif // SPIKE_EDITOR_DEBUGGER_NODE_H
