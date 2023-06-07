/**
 * item_list_split.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "item_list_split.h"
#include "spike_define.h"
#include "editor/editor_scale.h"

void ItemListHSplit::_bind_methods() {
    ADD_SIGNAL(MethodInfo("item_selected"));
    ADD_SIGNAL(MethodInfo("make_item_context_menu"));
}

void ItemListHSplit::_item_clicked(int p_item, Vector2 p_local_mouse_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index == MouseButton::RIGHT) {
        emit_signal("make_item_context_menu", p_item, context_menu);
        context_menu->set_position(get_screen_position() + get_local_mouse_position());
        context_menu->reset_size();
        context_menu->popup();
	}
}

void ItemListHSplit::_item_selected(const int &p_idx) {
    emit_signal("item_selected", p_idx, item_editor_container);
}

void ItemListHSplit::insert_item(const int &p_index, const String &p_name, const Ref<Texture2D> &p_icon, const String &p_tooltip) {
    add_item(p_name, p_icon, p_tooltip);
    item_list->move_item(item_list->get_item_count() - 1, p_index);
}

void ItemListHSplit::add_item(const String &p_name, const Ref<Texture2D> &p_icon, const String &p_tooltip) {
    item_list->add_item(p_name, p_icon);
    item_list->set_item_tooltip(item_list->get_item_count() - 1, p_tooltip);
}

void ItemListHSplit::remove_item(const int p_index) {
    item_list->remove_item(p_index);
}

void ItemListHSplit::move_item(int p_from_idx, int p_to_idx) {
    item_list->move_item(p_from_idx, p_to_idx);
}

void ItemListHSplit::select_item(const int &p_index) {
    item_list->select(p_index);
    _item_selected(p_index);
}

ItemListHSplit::ItemListHSplit() {
    set_split_offset(70 * EDSCALE);

    VBoxContainer *vbox = CREATE_NODE(this, VBoxContainer);
    vbox->set_v_size_flags(SIZE_EXPAND_FILL);

    item_list = CREATE_NODE(vbox, ItemList);
    item_list->set_custom_minimum_size(Size2(150, 60) * EDSCALE);
    item_list->set_v_size_flags(SIZE_EXPAND_FILL);
    item_list->connect("item_clicked", callable_mp(this, &ItemListHSplit::_item_clicked));
    item_list->connect("item_selected", callable_mp(this, &ItemListHSplit::_item_selected));
	item_list->set_allow_rmb_select(true);

    item_editor_container = CREATE_NODE(this, VBoxContainer);

    context_menu = memnew(PopupMenu);
	add_child(context_menu);
}

ItemListHSplit::~ItemListHSplit() {

}