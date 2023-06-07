/**
 * spike_editor_menu.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "account/account_manage.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "spike_editor_node.h"

void SpikeEditorNode::_init_help_menu() {
	help_menu->disconnect("id_pressed", callable_mp((EditorNode *)this, &SpikeEditorNode::_menu_option));
	help_menu->connect("id_pressed", callable_mp(this, &SpikeEditorNode::_help_menu_option));

	help_menu->clear();
	auto link_icon = gui_base->get_theme_icon(SNAME("ExternalLink"), SNAME("EditorIcons"));
	auto heart_icon = gui_base->get_theme_icon(SNAME("Heart"), SNAME("EditorIcons"));

	help_menu->add_icon_shortcut(gui_base->get_theme_icon(SNAME("HelpSearch"), SNAME("EditorIcons")), ED_GET_SHORTCUT("editor/editor_help"), MenuOptions::HELP_SEARCH);

	help_menu->add_separator();
	help_menu->add_icon_shortcut(link_icon, ED_SHORTCUT_AND_COMMAND("editor/spike_website", STTR("Spike Website")), HELP_SPIKE_WEBSITE);
	help_menu->add_icon_shortcut(link_icon, ED_SHORTCUT_AND_COMMAND("editor/spike_developer", STTR("Spike Developer")), HELP_SPIKE_DEVELOPER);
	help_menu->add_icon_item(link_icon, STTR("Enter Decentralized Network..."), HELP_SPIKE_WEB3);
	help_menu->add_separator();

	bool global_menu = !bool(EDITOR_GET("interface/editor/use_embedded_menu")) && DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_GLOBAL_MENU);
	if (!global_menu || !OS::get_singleton()->has_feature("macos")) {
		// On macOS  "Quit" and "About" options are in the "app" menu.
		help_menu->add_icon_shortcut(gui_base->get_theme_icon(SNAME("Spike"), SNAME("EditorIcons")), ED_SHORTCUT_AND_COMMAND("editor/about", TTR("About Spike")), HELP_ABOUT);
	}

	//help_menu->add_icon_shortcut(heart_icon, ED_SHORTCUT_AND_COMMAND("editor/support_development", TTR("Support Spike Development")), HELP_SUPPORT_SPIKE_DEVELOPMENT);

	help_menu->add_separator();

	auto godot_menu = memnew(PopupMenu);
	godot_menu->set_name("godot_help");
	help_menu->add_child(godot_menu);
	help_menu->add_submenu_item(STTR("Godot Help"), "godot_help");
	help_menu->set_item_icon(help_menu->get_item_count() - 1, gui_base->get_theme_icon(SNAME("Godot"), SNAME("EditorIcons")));
	godot_menu->connect("id_pressed", callable_mp(this, &SpikeEditorNode::_help_menu_option));

	godot_menu->add_icon_shortcut(link_icon, ED_SHORTCUT_AND_COMMAND("editor/online_docs", TTR("Online Documentation")), HELP_DOCS);
	godot_menu->add_icon_shortcut(link_icon, ED_SHORTCUT_AND_COMMAND("editor/q&a", TTR("Questions & Answers")), HELP_QA);
	godot_menu->add_icon_shortcut(link_icon, ED_SHORTCUT_AND_COMMAND("editor/report_a_bug", TTR("Report a Bug")), HELP_REPORT_A_BUG);
	godot_menu->add_icon_shortcut(link_icon, ED_SHORTCUT_AND_COMMAND("editor/suggest_a_feature", TTR("Suggest a Feature")), HELP_SUGGEST_A_FEATURE);
	godot_menu->add_icon_shortcut(link_icon, ED_SHORTCUT_AND_COMMAND("editor/send_docs_feedback", TTR("Send Docs Feedback")), HELP_SEND_DOCS_FEEDBACK);
	godot_menu->add_icon_shortcut(link_icon, ED_SHORTCUT_AND_COMMAND("editor/community", TTR("Community")), HELP_COMMUNITY);
	godot_menu->add_separator();
	godot_menu->add_icon_shortcut(heart_icon, ED_SHORTCUT_AND_COMMAND("editor/support_development", TTR("Support Godot Development")), MenuOptions::HELP_SUPPORT_GODOT_DEVELOPMENT);
}

void SpikeEditorNode::_help_menu_option(int p_option) {
	if (p_option < 100) {
		_menu_option_confirm(p_option, false);
		return;
	}

	if (p_option < 200)
		return;

	switch (p_option) {
		case HELP_SPIKE_WEBSITE:
			OS::get_singleton()->shell_open("https://www.spike.fun");
			break;
		case HELP_SPIKE_DEVELOPER:
#ifdef DEBUG_ENABLED
			OS::get_singleton()->shell_open("https://spike-dev-center.vercel.app/");
#else
#endif
		case HELP_SPIKE_WEB3: {
			if (_account_manage == nullptr) {
				List<StringName> inheriters;
				ScriptServer::get_inheriters_list(AccountManage::get_class_static(), &inheriters);
				if (inheriters.size()) {
					Ref<Script> script = ResourceLoader::load(ScriptServer::get_global_class_path(inheriters[0]));
					if (script.is_valid()) {
						_account_manage = Object::cast_to<AccountManage>(script->call("new"));
					}
				}
				if (_account_manage == nullptr) {
					_account_manage = memnew(AccountManage);
				}
				gui_base->add_child(_account_manage);
			}
			Object::cast_to<AccountManage>(_account_manage)->popup_web3_account();
		} break;

		case HELP_SUPPORT_SPIKE_DEVELOPMENT:
			// OS::get_singleton()->shell_open("https://godotengine.org/donate");
			break;

		default:
			break;
	}
}

Ref<RefCounted> SpikeEditorNode::get_web3_user() {
	return Object::cast_to<AccountManage>(_account_manage)->get_user();
}
