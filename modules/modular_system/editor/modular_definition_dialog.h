/**
 * modular_definition_dialog.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "filesystem_server/file_provider.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/graph_node.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/tree.h"
#include "spike_define.h"

class ModularDefinitionDialog : public AcceptDialog {
	GDCLASS(ModularDefinitionDialog, AcceptDialog);

private:
	Array module_info;
	Color type_color;
	Color value_color;

	GraphNode *current_node;
	String packed_file_path;
	Callable callback;

protected:
	Tree *member_tree;
	RichTextLabel *desc_label;

	void _generate_content(RichTextLabel *p_label, Array p_array, int p_depth, const String &p_parent_type = "");
	void _tree_item_selected();
	void _tree_button_clicked(TreeItem *p_item, int p_col, int p_id, int p_mouse_button);

public:
	GraphNode* get_current_node() const { return current_node; }
	String get_packed_file() const { return packed_file_path; }
	void update_tree(GraphNode *p_node, const Array &p_module_info, const Ref<FileProvider> &p_provider);
	void set_selection_callback(const Callable &p_callback) { callback = p_callback; }
	ModularDefinitionDialog();
};