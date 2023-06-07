/**
 * modular_definition_dialog.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_definition_dialog.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "filesystem_server/providers/file_provider_pack.h"
#include "modular_editor_plugin.h"
#include "modular_system/editor/modular_override_utils.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/split_container.h"

#define NAME_IDX 0
#define COMMENT_IDX 1
#define DESCRIPTION_IDX 2
#define SUBMEMBERS_IDX 3
#define SOURCE_PATH_IDX 4

void ModularDefinitionDialog::_generate_content(RichTextLabel *p_label, Array p_array, int p_depth, const String &p_parent_type) {
	p_label->push_indent(p_depth);
	String previous_type = "";
	for (int i = 0; i < p_array.size(); ++i) {
		Array arr_data = p_array[i];

		String display_name = arr_data[NAME_IDX];
		String comment = arr_data[COMMENT_IDX];
		Dictionary desc = arr_data[DESCRIPTION_IDX];
		Array submembers = arr_data[SUBMEMBERS_IDX];
		String type = desc.get("type", "");

		if (p_parent_type == "Method" && display_name == "return" &&
				comment.is_empty() && desc.get("class_name", "") == "" &&
				desc.get("var_type", 0).operator int() == int(Variant::NIL)) {
			// Ignore "void" return value.
			continue;
		}

		if (p_parent_type != "Method" && p_parent_type != "Signal") {
			p_label->add_newline();
		}

		// Member type
		if (p_parent_type != "Method" && p_parent_type != "Signal") {
			if (previous_type != type) {
				String title = "";
				if (type == "Method") {
					title = TTR("Methods");
				} else if (type == "Property") {
					title = TTR("Properties");
				} else if (type == "Constant") {
					title = TTR("Constants");
				} else if (type == "Signal") {
					title = TTR("Signals");
				} else if (type == "Enum") {
					title = TTR("Enumerations");
				} else if (type == "Class") {
					title = TTR("Classes");
				}

				if (!title.is_empty()) {
					p_label->pop();

					p_label->push_indent(p_depth - 1);
					p_label->push_font(EDITOR_THEME(font, SNAME("doc_bold"), SNAME("EditorFonts")));
					p_label->push_color(type_color);
					p_label->add_text(title);
					p_label->pop();
					p_label->pop();
					p_label->pop();

					p_label->push_indent(p_depth);

					p_label->add_newline();
				}
				previous_type = type;
			}
		}

		if (type != "Enum") {
			p_label->push_list(p_depth, RichTextLabel::LIST_DOTS, false);
		}
		// Type
		int var_type = desc.get("var_type", -1);
		String var_class_name = desc.get("class_name", "");
		if (!var_class_name.is_empty() || var_type > -1) {
			p_label->push_color(type_color);
			if (!var_class_name.is_empty()) {
				p_label->add_text(var_class_name);
			} else {
				if (desc.get("has_args", false)) {
					p_label->add_text(var_type != Variant::NIL ? Variant::get_type_name(Variant::Type(var_type)) : String("void"));
				} else {
					p_label->add_text(var_type != Variant::NIL ? Variant::get_type_name(Variant::Type(var_type)) : String("Variant"));
				}
			}
			p_label->pop();
		}

		p_label->add_text(" ");

		// Display name
		p_label->add_text(display_name);
		if (desc.get("has_args", false)) {
			// Methods and signals.
			p_label->add_text("(");

			// Args
			int arg_count = arg_count = submembers.size();
			if (int(desc.get("var_type", -1)) >= 0) {
				arg_count -= 1;
			}

			for (auto i = 0; i < arg_count; ++i) {
				Array arg_info = submembers[i];
				String arg_name = arg_info[NAME_IDX];
				Dictionary arg_desc = arg_info[DESCRIPTION_IDX];
				String arg_class_name = arg_desc.get("class_name", "");
				String value_text = arg_desc.get("var_value", "");
				if (i > 0) {
					p_label->add_text(", ");
				}
				// arg name
				p_label->add_text(arg_name);
				p_label->add_text(": ");
				// arg type
				p_label->push_color(type_color);
				if (!arg_class_name.is_empty()) {
					p_label->add_text(arg_class_name);
				} else {
					Variant::Type arg_type = Variant::Type((int)arg_desc.get("var_type", 0));
					p_label->add_text(arg_type != Variant::NIL ? Variant::get_type_name(arg_type) : String("Variant"));
				}
				p_label->pop();
				// arg default value
				if (!value_text.is_empty()) {
					p_label->push_color(value_color);
					p_label->add_text(" = ");
					p_label->add_text(value_text);
					p_label->pop();
				}
			}

			p_label->add_text(")");
			// static
			if (desc.get("static", false)) {
				p_label->push_color(type_color);
				p_label->add_text(" static");
				p_label->pop();
			}
		} else {
			bool readonly = desc.get("readonly", false);
			String var_value = desc.get("var_value", "");

			if (readonly || !var_value.is_empty()) {
				// prop, const
				if (readonly) {
					p_label->add_text(" ");
					p_label->push_color(value_color);
					p_label->add_text(STTR("Readonly"));
					p_label->pop();
				}
				if (!var_value.is_empty()) {
					p_label->add_text(" ");
					p_label->push_color(value_color);
					p_label->add_text(vformat(" [%s %s]", STTR("Value:"), var_value));
					p_label->pop();
				}
			}
		}

		if (type != "Enum") {
			p_label->pop(); // list
		}

		// Comment
		if (!comment.is_empty()) {
			if (p_parent_type == "Method" || p_parent_type == "Signal") {
				p_label->add_text("  ");
				p_label->add_text(comment);
			} else {
				p_label->add_newline();
				p_label->push_indent(1);
				p_label->add_text(comment);
				p_label->pop();
			}
		}

		p_label->add_newline();

		// Submembers
		if (submembers.size()) {
			_generate_content(p_label, submembers, p_depth + 1, type);
		}
	}

	p_label->pop();
}

void ModularDefinitionDialog::_tree_item_selected() {
	auto item = member_tree->get_selected();
	Array meta = item->get_metadata(0);
	desc_label->clear();
	auto doc_bold_font = EDITOR_THEME(font, SNAME("doc_bold"), SNAME("EditorFonts"));
	desc_label->push_font(doc_bold_font);
	desc_label->add_text(meta[COMMENT_IDX]);
	desc_label->pop();
	desc_label->add_newline();
	desc_label->add_text("(");
	desc_label->add_text(meta[NAME_IDX]);
	desc_label->add_text(")");
	desc_label->add_newline();

	if (item != member_tree->get_root()) {
		Variant members = meta[SUBMEMBERS_IDX];
		if (members.get_type() == Variant::ARRAY) {
			_generate_content(desc_label, members, 1);
		}
	}
}

void ModularDefinitionDialog::_tree_button_clicked(TreeItem *p_item, int p_col, int p_id, int p_mouse_button) {
	if (p_col == 0) {
		Array meta = p_item->get_metadata(p_col);
		Ref<ModularResource> res = EditorModularInterface::_get_node_res(current_node);
		if (res.is_null()) {
			res.reference_ptr(memnew(ModularResource));
			EditorModularInterface::_set_node_res(current_node, res);
		}

		if (callback.is_valid()) {
			String file_path = meta[NAME_IDX];
			callback.call_deferred(file_path);
		}
	}
}

void ModularDefinitionDialog::update_tree(GraphNode *p_node, const Array &p_module_info, const Ref<FileProvider> &p_provider) {
	current_node = p_node;
	bool readonly = p_module_info.size() != 1;
	Map<String, String> exist_files;
	if (!readonly) {
		Array module_info = p_module_info[0];
		packed_file_path = module_info[SOURCE_PATH_IDX];
		if (!IS_EMPTY(packed_file_path)) {
			Ref<ConfigFile> remap_conf = ModularOverrideUtils::get_loaded_remap_config(packed_file_path);
			GET_EXIST_MAPPING(remap_conf, exist_files);
		}
	}

	member_tree->clear();

	auto root = member_tree->create_item();

	for (int i = 0; i < p_module_info.size(); ++i) {
		Array module_info = p_module_info[i];
		String mod_name;
		Array members;
		if (module_info.size() > 2) {
			mod_name = module_info[NAME_IDX];
			members = module_info[SUBMEMBERS_IDX];
		}
		auto item = member_tree->create_item(root);
		item->set_text(0, mod_name);
		item->set_metadata(0, module_info);
		item->set_collapsed(readonly);

		for (int i = 0; i < members.size(); i++) {
			Array member = members[i];
			if (member.size() > 2) {
				auto klass = member_tree->create_item(item);
				String path = member[NAME_IDX];
				if (p_provider.is_valid()) {
					String type_name = p_provider->get_resource_type(path);
					klass->set_icon(0, EDITOR_THEME(icon, type_name, "EditorIcons"));
				}

				klass->set_text(0, path.get_basename().get_file().capitalize());
				klass->set_metadata(0, member);
				if (!readonly) {
					String icon = exist_files.has(path) ? "Edit" : "Add";
					klass->add_button(0, EDITOR_THEME(icon, icon, "EditorIcons"));
				}
			}
		}
	}

	desc_label->clear();
}

ModularDefinitionDialog::ModularDefinitionDialog() {
	auto hsplit = memnew(HSplitContainer);
	add_child(hsplit);
	hsplit->set_split_offset(300 * EDSCALE);

	member_tree = memnew(Tree);
	hsplit->add_child(member_tree);
	member_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	member_tree->set_hide_root(true);
	member_tree->connect("item_selected", callable_mp(this, &ModularDefinitionDialog::_tree_item_selected));
	member_tree->connect("button_clicked", callable_mp(this, &ModularDefinitionDialog::_tree_button_clicked));

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

	desc_label = memnew(RichTextLabel);
	desc_vbox->add_child(desc_label);
	desc_label->set_use_bbcode(true);
	desc_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	desc_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	desc_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);

	value_color = EDITOR_THEME(color, SNAME("value_color"), SNAME("EditorHelp"));
	type_color = EDITOR_THEME(color, SNAME("type_color"), SNAME("EditorHelp"));
}
