/**
 * spike_editor_node.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#ifndef SPIKE_EDITOR_NODE_H
#define SPIKE_EDITOR_NODE_H

#include "editor/editor_node.h"
#include "node_context_ref.h"
#include "spike_define.h"

class SpikeEditorNode : public EditorNode {
	GDCLASS(SpikeEditorNode, EditorNode)

	enum SpikeMenuOption {
		__OPTION_START = 200,
		HELP_SPIKE_WEBSITE,
		HELP_SUPPORT_SPIKE_DEVELOPMENT,
	};

	NodeContextRef<PackedStringArray> *custom_menus;

	AcceptDialog *_load_info_dialog;

	Button *_custom_warning_btn;
	Callable _custom_warning_callable;

public:
	SpikeEditorNode();
	~SpikeEditorNode();
	static SpikeEditorNode *get_instance() { return (SpikeEditorNode *)singleton; }

protected:
	static void _bind_methods();
	void _file_moved_on_dock(const String &old_file, const String &new_file);
	void _file_removed_on_dock(const String &file);
	void _filesystem_first_scanned(bool exists);

	void _submenu_pressed(int index, PopupMenu *popup);

	// Override Editor Warning Dialog
	void _warning_action(const String &p_str);
	void _warning_closed();

	// Override Editor Menu
	void _init_help_menu();
	void _help_menu_option(int p_option);

	// Override About Dialog
	RichTextLabel *rtl_spike_license;
	void _init_about_dialog();
	void _see_about_content(int p_index);
	void _about_will_popup();
	void _about_meta_clicked(Variant p_meta);
	void _about_version_lnk_pressed(const LinkButton *p_btn);
	void _select_license_item(int p_index);

public:
	EditorRun *get_editor_run() { return &editor_run; }

	PopupMenu *add_menu_item(PackedStringArray &menus, int &r_idx);
	bool remove_menu_item(PackedStringArray &menus);
	void add_menu_ref(Node *p_host, const PackedStringArray &p_menus) {
		custom_menus->make_ref(p_host, p_menus);
	}

	void show_custom_warning(const Callable &p_callable, const String &p_custom, const String &p_text, const String &p_title = TTR("Warning!"));

	void add_main_screen_editor_plugin(EditorPlugin *p_plugin, int p_tab_index = -1);
	void exit_editor(int p_exit_code) { _exit_editor(p_exit_code); }
};

#endif // SPIKE_EDITOR_NODE_H