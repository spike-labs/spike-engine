/**
 * item_list_split.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "scene/gui/box_container.h"
#include "scene/gui/item_list.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/split_container.h"

class ItemListHSplit : public HSplitContainer {
	GDCLASS(ItemListHSplit, HSplitContainer);

protected:
	static void _bind_methods();

private:
	ItemList *item_list = nullptr;
	VBoxContainer *item_editor_container = nullptr;
	PopupMenu *context_menu = nullptr;
	void _item_clicked(int p_item, Vector2 p_local_mouse_pos, MouseButton p_mouse_button_index);
	void _item_selected(const int &p_idx);

public:
	void insert_item(const int &p_index, const String &p_name, const Ref<Texture2D> &p_icon = nullptr, const String &p_tooltip = String());
	void add_item(const String &p_name, const Ref<Texture2D> &p_icon = nullptr, const String &p_tooltip = String());
	void move_item(int p_from_idx, int p_to_idx);
	void remove_item(const int p_index);
	void select_item(const int &p_index);
	ItemList *get_item_list() { return item_list; }
	PopupMenu *get_context_menu() { return context_menu; }
	VBoxContainer *get_item_editor_container() { return item_editor_container; }
	ItemListHSplit();
	~ItemListHSplit();
};