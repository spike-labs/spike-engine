/**
 * modular_port_dialog.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_port_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "modular_editor_plugin.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/split_container.h"

#define MISSING_MODULE_FMT STTR("Module Missing! No module named '%s' was found in the current node. Please check the resources referenced by the node.")

void ModularPortDialog::_add_item(const String &p_meta, const String &p_title, const String &p_desc, bool p_input, bool p_output) {
	auto root = port_tree->get_root();
	auto port = port_tree->create_item(root);
	auto meta = PackedStringArray();
	auto title = p_title;
	meta.push_back(p_meta);
	if (p_meta == ANY_MODULE) {
		meta.push_back(STTR("NEW Modules..."));
		title = "NEW Modules...";
		p_input = true;
		p_output = true;
	} else {
		meta.push_back(p_desc);
	}

	port->set_metadata(0, meta);
	if (!IS_EMPTY(title)) {
		port->set_text(0, title);
		port->clear_custom_color(0);
	} else {
		port->set_text(0, vformat("%s(%s)", p_meta, STTR("Missing")));
		port->set_custom_color(0, Color::hex(0xFF0000FF));
	}
	port->add_button(1, p_input ? checked_icon : unchecked_icon, 0, p_meta == ANY_MODULE);
	port->add_button(2, p_output ? checked_icon : unchecked_icon, 0, p_meta == ANY_MODULE);
}

void ModularPortDialog::_tree_item_selected() {
	auto item = port_tree->get_selected();
	if (item) {
		PackedStringArray meta = item->get_metadata(0);
		if (!IS_EMPTY(meta[1])) {
			desc_label->set_text(meta[1]);
		} else {
			desc_label->set_text(vformat(MISSING_MODULE_FMT, meta[0]));
		}
	} else {
		desc_label->set_text(String());
	}
}

void ModularPortDialog::_tree_item_activated() {
}

void ModularPortDialog::_tree_button_clicked(TreeItem *p_item, int p_col, int p_id, int p_mouse_button) {
	PackedStringArray array = p_item->get_metadata(0);
	String port = array[0];
	PackedStringArray *selections = nullptr;
	if (p_col == 1) {
		selections = &input_selections;
	} else if (p_col == 2) {
		selections = &output_selections;
	}
	if (selections == nullptr)
		return;

	int index = selections->find(port);
	if (index < 0) {
		selections->push_back(port);
		p_item->set_button(p_col, p_id, checked_icon);
	} else {
		selections->remove_at(index);
		p_item->set_button(p_col, p_id, unchecked_icon);
	}
}

void ModularPortDialog::update_tree(GraphNode *p_node) {
	if (checked_icon.is_null()) {
		checked_icon = p_node->get_theme_icon(SNAME("TileChecked"), SNAME("EditorIcons"));
	}
	if (unchecked_icon.is_null()) {
		unchecked_icon = p_node->get_theme_icon(SNAME("TileUnchecked"), SNAME("EditorIcons"));
	}

	graph_node = p_node;
	port_tree->clear();
	port_tree->create_item();

	List<String> port_keys;
	input_selections = EditorModularInterface::_get_node_inputs(p_node);
	output_selections = EditorModularInterface::_get_node_outputs(p_node);
	Map<String, Array> port_map;
	for (int i = 0; i < input_selections.size(); i++) {
		port_map.insert(input_selections[i], Array());
		port_keys.push_back(input_selections[i]);
	}
	for (int i = 0; i < output_selections.size(); i++) {
		auto n = port_map.size();
		port_map.insert(output_selections[i], Array());
		if (n < port_map.size()) {
			port_keys.push_back(output_selections[i]);
		}
	}

	auto res = EditorModularInterface::_get_node_res(p_node);
	if (res.is_valid()) {
		Dictionary info = res->get_module_info();
		for (const Variant *key = info.next(nullptr); key; key = info.next(key)) {
			String meta = *key;
			auto n = port_map.size();
			port_map.insert(meta, info[meta]);
			if (n < port_map.size()) {
				port_keys.push_back(meta);
			}
		}
	}

	port_keys.sort();
	port_keys.erase(ANY_MODULE);
	port_keys.push_front(ANY_MODULE);
	for (auto &meta : port_keys) {
		Array array = port_map[meta];
		if (array.size() > 1) {
			_add_item(meta, array[0], array[1], input_selections.has(meta), output_selections.has(meta));
		} else {
			_add_item(meta, String(), String(), input_selections.has(meta), output_selections.has(meta));
		}
	}

	desc_label->set_text(String());
}

void ModularPortDialog::get_selections(PackedStringArray &r_inputs, PackedStringArray &r_outputs) {
	r_inputs = input_selections;
	r_outputs = output_selections;

	r_inputs.sort();
	r_inputs.erase(ANY_MODULE);
	r_inputs.insert(0, ANY_MODULE);

	r_outputs.sort();
	r_outputs.erase(ANY_MODULE);
	r_outputs.insert(0, ANY_MODULE);
}

ModularPortDialog::ModularPortDialog() {
	auto hsplit = memnew(HSplitContainer);
	add_child(hsplit);
	hsplit->set_split_offset(300 * EDSCALE);

	port_tree = memnew(Tree);
	hsplit->add_child(port_tree);
	port_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	port_tree->set_hide_root(true);
	port_tree->set_select_mode(Tree::SELECT_ROW);
	port_tree->set_columns(3);
	port_tree->set_column_titles_visible(true);
	port_tree->set_column_title(0, STTR("Available Modules"));
	port_tree->set_column_title(1, STTR("In", "ModularPort"));
	port_tree->set_column_title(2, STTR("Out", "ModularPort"));
	port_tree->connect("item_selected", callable_mp(this, &ModularPortDialog::_tree_item_selected));
	port_tree->connect("item_activated", callable_mp(this, &ModularPortDialog::_tree_item_activated));
	port_tree->connect("button_clicked", callable_mp(this, &ModularPortDialog::_tree_button_clicked));

	auto desc_scroll_bg = memnew(PanelContainer);
	hsplit->add_child(desc_scroll_bg);
	desc_scroll_bg->add_theme_style_override("panel", EDITOR_THEME(stylebox, "panel", "Tree"));

	auto desc_scroll = memnew(ScrollContainer);
	desc_scroll_bg->add_child(desc_scroll);
	desc_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	desc_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	auto desc_vbox = memnew(VBoxContainer);
	desc_scroll->add_child(desc_vbox);
	desc_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	desc_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	desc_label = memnew(Label);
	desc_vbox->add_child(desc_label);
	desc_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	desc_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	desc_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
}
