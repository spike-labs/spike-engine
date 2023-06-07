/**
 * editor_aitools_panel.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "aitools/ai_assistant.h"
#include "scene/gui/box_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/rich_text_label.h"
#include "scene/main/http_request.h"
#include "scene/main/timer.h"

class EditorNode;

class EditorAIToolsPanel : public VBoxContainer {
	GDCLASS(EditorAIToolsPanel, VBoxContainer)

	enum SettingsMenu {
		MENU_PROMPTS,
	};

	enum RequestAPI {
		API_NONE,
		API_GET_PROMPTS,
		API_LIST_MODELS,
		API_AI_CHAT,
	};

	VBoxContainer *chat_container;
	LineEdit *chat_input;
	Ref<StyleBoxFlat> style_system;
	Ref<StyleBoxFlat> style_assistant;
	Ref<StyleBoxFlat> style_user;

	void _ensure_control_visible();
	void _wait_for_response();
	Control *append_chat_content(bool p_answer);
	void clear_chat_history();

	void _validate_apikey();
	void _switch_settings_panel(bool p_toggled);
	void _model_selected(int p_index);
	void _prompts_edit_sumitted(const String &p_text);
	void _assistant_status_changed(int p_status);
	void _assistant_message_received(int p_role, String &p_content);
	void on_submitted_chat_text(const String &p_text);
	void on_assistant_meta_clicked(Variant p_meta);
	void on_waiting_response();

	void _init_chat_panel(Node *p_root);
	void _init_settings_menu(Node *p_root);
	void _init_settings_panel(Node *p_root);

	String convert_gdscript_code(const String &p_code);

protected:
	AIAssistant *assistant;
	Timer *resp_timer;

	MenuButton *settings_menu;
	Control *settings_panel;
	OptionButton *aimodel_opts;
	LineEdit *prompts_edit;
	RichTextLabel *resp_label;
	Control *tail_ctrl;
	//Button *retry_btn;

	String prompts_file;
	String prompts_content;

	void _notification(int what);

public:
	EditorAIToolsPanel();
};