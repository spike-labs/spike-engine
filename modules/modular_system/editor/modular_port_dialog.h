/**
 * modular_port_dialog.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "modular_graph.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/graph_node.h"
#include "scene/gui/tree.h"

class ModularPortDialog : public ConfirmationDialog {
	GDCLASS(ModularPortDialog, ConfirmationDialog);

protected:
	// Theme
	Ref<Texture2D> checked_icon, unchecked_icon;

	Tree *port_tree;
	Label *desc_label;

	GraphNode *graph_node;
	bool is_output;

	PackedStringArray input_selections;
	PackedStringArray output_selections;

	void _add_item(const String &p_meta, const String &p_title, const String &p_desc, bool p_input, bool p_output);

	void _tree_item_selected();
	void _tree_item_activated();
	void _tree_button_clicked(TreeItem *p_item, int p_col, int p_id, int p_mouse_button);

public:
	void update_tree(GraphNode *p_node);
	Node *get_graph_node() { return graph_node; }
	bool get_is_output() { return is_output; }
	void get_selections(PackedStringArray &r_inputs, PackedStringArray &r_outputs);
	ModularPortDialog();
};