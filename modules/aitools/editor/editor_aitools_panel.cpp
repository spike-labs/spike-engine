/**
 * editor_aitools_panel.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#include "editor_aitools_panel.h"

#include "core/io/json.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/project_converter_3_to_4.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "spike_define.h"

static void init_style_flat(Ref<StyleBoxFlat> &p_style, const Color &p_color) {
	p_style.instantiate();
	p_style->set_bg_color(p_color);
	p_style->set_border_color(p_color);
	p_style->set_border_width_all(10 * EDSCALE);
	p_style->set_corner_radius_all(10 * EDSCALE);
}

void EditorAIToolsPanel::_ensure_control_visible() {
	auto scroll = Object::cast_to<ScrollContainer>(chat_container->get_parent());
	SceneTree::get_singleton()->connect("process_frame", callable_mp(scroll, &ScrollContainer::ensure_control_visible).bind(tail_ctrl), CONNECT_ONE_SHOT | CONNECT_DEFERRED);
}

void EditorAIToolsPanel::_wait_for_response() {
	resp_timer->start(1);
	resp_label = Object::cast_to<RichTextLabel>(append_chat_content(true));
	resp_label->add_theme_style_override("normal", style_assistant);

	_ensure_control_visible();
}

Control *EditorAIToolsPanel::append_chat_content(bool p_answer) {
	auto hbox = memnew(HBoxContainer);
	chat_container->add_child(hbox);
	hbox->set_alignment(BoxContainer::ALIGNMENT_END);

	chat_container->move_child(tail_ctrl, -1);

	Control *label = nullptr;
	if (p_answer) {
		auto rich_label = memnew(RichTextLabel);
		rich_label->set_use_bbcode(true);
		rich_label->set_fit_content(true);
		rich_label->set_scroll_active(false);
		rich_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		rich_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		rich_label->set_selection_enabled(true);
		rich_label->set_context_menu_enabled(true);
		rich_label->connect("meta_clicked", callable_mp(this, &EditorAIToolsPanel::on_assistant_meta_clicked));
		label = rich_label;
	} else {
		label = memnew(Label);
	}
	hbox->add_child(label);
	return label;
}

void EditorAIToolsPanel::clear_chat_history() {
	for (int i = 0; i < chat_container->get_child_count(); ++i) {
		auto child = chat_container->get_child(i);
		if (child->is_class_ptr(HBoxContainer::get_class_ptr_static())) {
			child->queue_free();
		}
	}

	assistant->clear_messages();
}

void EditorAIToolsPanel::_validate_apikey() {
	String api_key = EDITOR_GET("ai_tools/gpt/api_key");
	if (api_key.length() == 0) {
		return;
	}
	assistant->setup(api_key);
}

void EditorAIToolsPanel::_switch_settings_panel(bool p_toggled) {
	settings_panel->set_visible(p_toggled);
}

void EditorAIToolsPanel::_model_selected(int p_index) {
	assistant->select_model(p_index);
}

void EditorAIToolsPanel::_prompts_edit_sumitted(const String &p_text) {
	prompts_file = String();
}

void EditorAIToolsPanel::_assistant_status_changed(int p_status) {
	if (p_status != AIAssistant::STATUS_READY) {
		return;
	}

	int selected = aimodel_opts->get_selected();
	String model_id = selected >= 0 ? aimodel_opts->get_item_text(selected) : String(EDITOR_GET("ai_tools/gpt/default_model"));
	selected = 0;
	aimodel_opts->clear();
	int n = 0;
	for (int i = 0; i < assistant->get_model_count(); ++i) {
		String id = assistant->get_model_id(i);
		if (id.to_lower().begins_with("gpt")) {
			aimodel_opts->add_item(id);
			if (id == model_id) {
				selected = n;
			}
			n++;
		}
	}
	aimodel_opts->select(selected);
	assistant->select_model(selected);
}

void EditorAIToolsPanel::_assistant_message_received(int p_role, String &p_content) {
	chat_input->set_editable(true);
	if (p_role == AIAssistant::ROLE_USER) {
		auto label = Object::cast_to<Label>(append_chat_content(false));
		label->add_theme_style_override("normal", style_user);
		if (p_content.length() >= 100) {
			label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
			label->set_custom_minimum_size(Size2(1000, 0));
		}
		label->set_text(p_content);
	} else if (p_role == AIAssistant::ROLE_SYSTEM) {
	} else {
		RichTextLabel *label = nullptr;
		if (resp_label) {
			label = resp_label;
			label->clear();
			resp_label = nullptr;
		} else {
			label = Object::cast_to<RichTextLabel>(append_chat_content(true));
			label->add_theme_style_override("normal", style_assistant);
		}

		if (p_role == AIAssistant::ROLE_ASSISTANT) {
			Ref<Font> doc_code_font = get_theme_font(SNAME("doc_source"), SNAME("EditorFonts"));
			const Color code_bg_color = get_theme_color(SNAME("code_bg_color"), SNAME("EditorHelp"));
			const Color code_color = get_theme_color(SNAME("code_color"), SNAME("EditorHelp"));
			const Color param_bg_color = get_theme_color(SNAME("param_bg_color"), SNAME("EditorHelp"));

			String content = p_content;
			PackedStringArray paragraphs;
			int startIdx = 0;
			for (;;) {
				int index = content.find("```", startIdx);
				if (index < 0)
					break;
				paragraphs.push_back(content.substr(startIdx, index - startIdx));
				index += 3;
				int n = content.find("\n", index);
				String lang = "gdscript";
				if (n < 0) {
					startIdx = index;
				} else {
					lang = content.substr(index, n - index);
					startIdx = n + 1;
				}
			}
			paragraphs.push_back(content.substr(startIdx));

			for (int i = 0; i < paragraphs.size(); ++i) {
				if (i % 2 == 0) {
					label->add_text(paragraphs[i]);
				} else {
					String code = paragraphs[i];
					if (bool(EDITOR_GET("ai_tools/gpt/optimize_for_gdscript"))) {
						code = convert_gdscript_code(code);
					}
					Dictionary meta;
					meta["type"] = "code";
					meta["value"] = code;

					label->push_indent(1);
					label->push_table(1);
					label->push_cell();
					label->set_cell_row_background_color(Color(code_bg_color, 0.6), Color(code_bg_color, 0.5));
					label->set_cell_padding(Rect2(10 * EDSCALE, 10 * EDSCALE, 10 * EDSCALE, 10 * EDSCALE));
					label->push_meta(meta);
					label->add_text(STTR("Copy code"));
					label->pop();
					label->pop();

					label->push_cell();
					label->set_cell_row_background_color(code_bg_color, Color(code_bg_color, 0.99));
					label->set_cell_padding(Rect2(10 * EDSCALE, 10 * EDSCALE, 10 * EDSCALE, 10 * EDSCALE));
					label->push_font(doc_code_font);
					//label->push_font_size(doc_code_font_size);
					label->push_color(Color(code_color, 0.8));
					label->add_text(code);
					label->pop();
					label->pop();
					label->pop();
					label->pop();
					label->pop();
				}
			}
		} else {
			label->push_color(Color::hex(0xFF0000FF));
			label->add_text(p_content.is_empty() ? String("unknown api error.") : p_content);
			label->pop();

			String content = assistant->get_message_content(assistant->get_message_count() - 1);
			chat_input->set_text(content);
		}

		_ensure_control_visible();
	}
}

void EditorAIToolsPanel::on_submitted_chat_text(const String &p_text) {
	chat_input->set_editable(false);
	if (assistant->get_message_count() == 0) {
		if (prompts_file.is_empty()) {
			String prompts_url = prompts_edit->get_text();
			if (prompts_url.begins_with("http")) {
				//request->request(prompts_url);
				return;
			} else if (FileAccess::exists(prompts_url)) {
				prompts_file = prompts_url;
				prompts_content = FileAccess::get_file_as_string(prompts_url);
			} else {
				WLog("Invalid prompts source: %s", prompts_url);
				return;
			}
		}

		assistant->add_message(AIAssistant::ROLE_SYSTEM, prompts_content);
	}

	assistant->send_message(p_text);
	chat_input->set_text(String());
	_wait_for_response();
}

void EditorAIToolsPanel::on_assistant_meta_clicked(Variant p_meta) {
	Dictionary meta = p_meta;
	String type = meta["type"];
	if (type == "code") {
		String code = meta["value"];
		DisplayServer::get_singleton()->clipboard_set(code);
	}
}

void EditorAIToolsPanel::on_waiting_response() {
	if (resp_label) {
		String text = resp_label->get_text();
		String str = String(". ");
		int n = text.length() / str.length() % 3;
		resp_label->set_text(str.repeat(n + 1));
	}
}

void EditorAIToolsPanel::_init_chat_panel(Node *p_root) {
	p_root->add_child(memnew(HSeparator));
	auto scroll = memnew(ScrollContainer);
	p_root->add_child(scroll);
	scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	chat_container = memnew(VBoxContainer);
	scroll->add_child(chat_container);
	chat_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_container->add_theme_constant_override("separation", 20 * EDSCALE);

	tail_ctrl = memnew(Control);
	chat_container->add_child(tail_ctrl);

	auto hbox = memnew(HBoxContainer);
	p_root->add_child(hbox);

	chat_input = memnew(LineEdit);
	hbox->add_child(chat_input);
	chat_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_input->set_placeholder(TTR("Type your question here..."));
	chat_input->connect("text_submitted", callable_mp(this, &EditorAIToolsPanel::on_submitted_chat_text));

	auto clear_btn = memnew(Button);
	hbox->add_child(clear_btn);
	clear_btn->set_text(TTR("Clear"));
	clear_btn->connect("pressed", callable_mp(this, &EditorAIToolsPanel::clear_chat_history));

	clear_chat_history();
}

void EditorAIToolsPanel::_init_settings_menu(Node *p_root) {
	HBoxContainer *menu_hb = memnew(HBoxContainer);
	p_root->add_child(menu_hb);
	auto settings_tgl = memnew(Button);
	menu_hb->add_child(settings_tgl);
	settings_tgl->set_toggle_mode(true);
	settings_tgl->set_pressed(false);
	settings_tgl->set_text(TTR("Settings..."));
	settings_tgl->connect("toggled", callable_mp(this, &EditorAIToolsPanel::_switch_settings_panel));
}

void EditorAIToolsPanel::_init_settings_panel(Node *p_root) {
	settings_panel = memnew(HBoxContainer);
	p_root->add_child(settings_panel);

	auto name_vb = memnew(VBoxContainer);
	settings_panel->add_child(name_vb);

	auto value_vb = memnew(VBoxContainer);
	settings_panel->add_child(value_vb);
	value_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	{
		auto ai_model_lb = memnew(Label);
		name_vb->add_child(ai_model_lb);
		ai_model_lb->set_text(STTR("Model: "));

		auto model_hb = memnew(HBoxContainer);
		value_vb->add_child(model_hb);

		aimodel_opts = memnew(OptionButton);
		model_hb->add_child(aimodel_opts);
		aimodel_opts->connect("item_selected", callable_mp(this, &EditorAIToolsPanel::_model_selected));

		auto api_key_btn = memnew(Button);
		model_hb->add_child(api_key_btn);
		api_key_btn->set_icon(EDITOR_THEME(icon, "Reload", "EditorIcons"));
		api_key_btn->connect("pressed", callable_mp(this, &EditorAIToolsPanel::_validate_apikey));
	}

	{
		auto messages_lb = memnew(Label);
		name_vb->add_child(messages_lb);
		messages_lb->set_text(STTR("Prompts File: "));

		prompts_edit = memnew(LineEdit);
		value_vb->add_child(prompts_edit);
		prompts_edit->connect("text_submitted", callable_mp(this, &EditorAIToolsPanel::_prompts_edit_sumitted));
	}

	settings_panel->set_visible(false);
}

void EditorAIToolsPanel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			_validate_apikey();
			break;
	}
}

String EditorAIToolsPanel::convert_gdscript_code(const String &p_code) {
	auto converter = memnew(ProjectConverter3To4(1024, 100000));
	return converter->convert_gdscript_code(p_code);
}

EditorAIToolsPanel::EditorAIToolsPanel() {
	EDITOR_DEF("ai_tools/gpt/optimize_for_gdscript", true);
	EDITOR_DEF("ai_tools/gpt/api_key", "");
	EDITOR_DEF("ai_tools/gpt/default_model", "gpt-3.5-turbo");

	init_style_flat(style_system, Color::hex(0x000000FF));
	init_style_flat(style_assistant, Color::hex(0x4a515dFF));
	init_style_flat(style_user, Color::hex(0x4779d6FF));

	assistant = memnew(AIAssistant);
	add_child(assistant);
	assistant->connect("status_changed", callable_mp(this, &EditorAIToolsPanel::_assistant_status_changed));
	assistant->connect("message_received", callable_mp(this, &EditorAIToolsPanel::_assistant_message_received));

	const String proxy_host = EDITOR_GET("network/http_proxy/host");
	const int proxy_port = EDITOR_GET("network/http_proxy/port");
	assistant->setup_network(proxy_host, proxy_port, EDITOR_DEF("asset_library/use_threads", true));

	resp_timer = memnew(Timer);
	add_child(resp_timer);
	resp_timer->set_one_shot(false);
	resp_timer->connect("timeout", callable_mp(this, &EditorAIToolsPanel::on_waiting_response));

	prompts_file = "builtin";
	prompts_content = "You are a coding assistant, only answer questions about coding with GDScript.";

	set_custom_minimum_size(Size2(0, 300) * EDSCALE);

	_init_settings_menu(this);
	_init_settings_panel(this);
	_init_chat_panel(this);
}
