/**
 * spike_editor_utils.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "spike_define.h"
#include "editor/editor_node.h"

class EditorUtils : public Object {
	GDCLASS(EditorUtils, Object);

protected:
	static void _bind_methods();

public:
	static PopupMenu* make_menu(Node *p_menu, PackedStringArray &p_menus, Callable p_menu_pressed, int &r_idx);
	static bool remove_menu(Node *p_menu, PackedStringArray &p_menus);
	static void add_menu_item(const String &menu_path, const Callable &callable, const Ref<Texture2D> &p_icon = Ref<Texture2D>());
	static void add_menu_check_item(const String &menu_path, const Callable &callable, bool p_checked = false, const Ref<Texture2D> &p_icon = Ref<Texture2D>());
	static void add_menu_radio_check_item(const String &menu_path, const Callable &callable, bool p_checked = false, const Ref<Texture2D> &p_icon = Ref<Texture2D>());
	static void remove_menu_item(const String &menu_path);
	static int execute_and_show_output(const String &p_title, const String &p_path, const Array &p_arguments, bool p_close_on_ok = true, bool p_close_on_errors = false);
};
