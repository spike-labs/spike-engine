/**
 * configuration_table_cell.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "configuration_table_cell.h"

//Cell
void Cell::_bind_methods() {
	ADD_SIGNAL(MethodInfo("cell_init"));
	ADD_SIGNAL(MethodInfo("cell_gui"));
	ADD_SIGNAL(MethodInfo("cell_value_changed"));
	ADD_SIGNAL(MethodInfo("cell_right_click"));
	ADD_SIGNAL(MethodInfo("cell_focus_entered"));
	ADD_SIGNAL(MethodInfo("cell_focus_exited"));
}

bool Cell::_set_value(const Variant &p_value, const Variant &p_default) {
	if (p_value.get_type() != get_value_type()) {
		value = p_default;
		return false;
	}
	value = p_value;
	return true;
}

void Cell::_emit_value_changed(const Variant &p_old, const Variant &p_new) {
	emit_signal("cell_value_changed", this, p_old, p_new);
}

void Cell::_emit_focus_entered() {
	emit_signal("cell_focus_entered", this);
}

void Cell::_emit_focus_exited() {
	emit_signal("cell_focus_exited", this);
}

void Cell::_emit_right_click() {
	emit_signal("cell_right_click", this);
}

void Cell::_value_changed(const Variant &p_new_value) {
	_emit_value_changed(value, p_new_value);
	value = p_new_value;
}

void Cell::_focus_entered() {
	if (nullptr != edit_control && edit_control->get_focus_mode() != FocusMode::FOCUS_NONE) {
		focus_button->set_mouse_filter(MouseFilter::MOUSE_FILTER_IGNORE);
	}
	_emit_focus_entered();
}

void Cell::_focus_exited() {
	if (nullptr != edit_control && edit_control->get_focus_mode() != FocusMode::FOCUS_NONE) {
		focus_button->set_mouse_filter(MouseFilter::MOUSE_FILTER_STOP);
	}
	_emit_focus_exited();
}

void Cell::_focus_button_focus_entered() {
	_focus_entered();
}

void Cell::_focus_button_focus_exited() {
	if (!edit_control_mouse_focused) {
		_focus_exited();
	}
}

void Cell::_focus_button_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> b = p_event;
	if (b.is_valid()) {
		if (b->is_pressed() && b->get_button_index() == MouseButton::RIGHT) {
			focus_button->grab_focus();
			_emit_right_click();
			accept_event();
		}
	}
}
void Cell::_focus_button_draw() {
	if (is_patched) {
		Ref<Texture2D> patched_icon = theme_cache.patched;
		Vector2 ofs;
		ofs.x = int((focus_button->get_size().x - patched_icon->get_width()));
		ofs.y = 0;
		focus_button->draw_texture(patched_icon, ofs);
	}
}

void Cell::_focus_control_mouse_entered() {
	edit_control_mouse_focused = true;
}

void Cell::_focus_control_mouse_exited() {
	edit_control_mouse_focused = false;
}

void Cell::_focus_control_focus_exited() {
	_focus_exited();
}

void Cell::_expand_editor() {
	VariantEditor *editor = VariantEditor::get_editor("CellVariantEditor");
	editor->variant_value_changed = callable_mp(this, &Cell::_value_changed);
	editor->pop_editor(this, get_value_type(), value);
}

void Cell::_update_theme_item_cache() {
	theme_cache.patched = get_theme_icon(SNAME("Error"), SNAME("EditorIcons"));
}

void Cell::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (nullptr != edit_control) {
				if (focus_control_theme.normal.is_valid())
					edit_control->add_theme_style_override("normal", focus_control_theme.normal);
				if (focus_control_theme.hover.is_valid())
					edit_control->add_theme_style_override("hover", focus_control_theme.hover);
				if (focus_control_theme.pressed.is_valid())
					edit_control->add_theme_style_override("pressed", focus_control_theme.pressed);
				if (focus_control_theme.disabled.is_valid())
					edit_control->add_theme_style_override("disabled", focus_control_theme.disabled);
				if (focus_control_theme.read_only.is_valid())
					edit_control->add_theme_style_override("read_only", focus_control_theme.read_only);
			}
		} break;
	}
}

void Cell::update_patched(const Ref<ConfigurationTable> &p_table) {
	is_patched = p_table->is_patch && p_table->has_cell(row, column);
	focus_button->queue_redraw();
}

void Cell::set_focusable(bool p_focusable) {
	focus_button->set_focus_mode(p_focusable ? FocusMode::FOCUS_ALL : FocusMode::FOCUS_NONE);
}

void Cell::set_editable(bool p_editable) {
	edit_control->set_focus_mode(p_editable ? FocusMode::FOCUS_ALL : FocusMode::FOCUS_NONE);
}

void Cell::add_edit_control(Control *p_control) {
	add_child(p_control);
	move_child(p_control, 0);
	edit_control = p_control;
	edit_control->connect("mouse_entered", callable_mp(this, &Cell::_focus_control_mouse_entered));
	edit_control->connect("mouse_exited", callable_mp(this, &Cell::_focus_control_mouse_exited));
	edit_control->connect("focus_exited", callable_mp(this, &Cell::_focus_control_focus_exited));
}

Cell::Cell(const int &p_row, const int &p_column, const Size2i &p_minimum_size) {
	row = p_row;
	column = p_column;
	edit_control_mouse_focused = false;
	edit_control = nullptr;
	set_custom_minimum_size(p_minimum_size * EDSCALE);
	focus_button = CREATE_NODE(this, Button);
	focus_button->set_flat(true);
	focus_button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	focus_button->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	focus_button->connect("gui_input", callable_mp(this, &Cell::_focus_button_gui_input));
	focus_button->connect("focus_entered", callable_mp(this, &Cell::_focus_button_focus_entered));
	focus_button->connect("focus_exited", callable_mp(this, &Cell::_focus_button_focus_exited));
	focus_button->connect("draw", callable_mp(this, &Cell::_focus_button_draw));
}

//TypeCell
void TypeCell::_value_changed(const Variant &p_value) {
	int new_value = option->get_item_id(p_value);
	_emit_value_changed(value, new_value);
	value = new_value;
}

void TypeCell::set_value(const Variant &p_value) {
	_set_value(p_value, Variant::STRING);
	option->select(option->get_item_index(value));
}

void TypeCell::set_editable(bool p_editable) {
	Cell::set_editable(p_editable);
	option->set_disabled(!p_editable);
}

TypeCell::TypeCell(
		const int &p_row,
		const int &p_column,
		const Variant &p_type,
		const Size2i &p_minimum_size) :
		Cell(p_row, p_column, p_minimum_size) {
	option = memnew(OptionButton);
	option->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	option->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	option->connect("item_selected", callable_mp(this, &TypeCell::_value_changed));
	for (const KeyValue<Variant::Type, String> &E : Utility::TYPE_MAPPING) {
		option->add_icon_item(EditorNode::get_singleton()->get_gui_base()->get_theme_icon(
									  Variant::get_type_name(E.key), "EditorIcons"),
				Utility::get_type_custom_name(E.key), E.key);
	}
	add_edit_control(option);
	set_value(p_type);
}

//TextCell
void TextCell::_focus_exited() {
	Cell::_focus_exited();
	String txt = "";
	if (value.get_type() == Variant::STRING)
		txt = value;
	if (txt != text->get_text()) {
		_value_changed(text->get_text());
	}
}

void TextCell::set_value(const Variant &p_value) {
	value = p_value;
	if (value.get_type() == Variant::NIL) {
		text->set_text("");
	} else {
		text->set_text(value);
	}
}

void TextCell::set_editable(bool p_editable) {
	Cell::set_editable(p_editable);
	text->set_editable(p_editable);
}

TextCell::TextCell(
		const int &p_row,
		const int &p_column,
		const Variant &p_value,
		const Size2i &p_minimum_size) :
		Cell(p_row, p_column, p_minimum_size) {
	text = memnew(LineEdit);
	text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	text->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	text->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_LEFT);
	text->connect("text_submitted", callable_mp((Cell *)this, &TextCell::_value_changed));
	add_edit_control(text);
	set_value(p_value);
}

//ReadOnlyCell
void ReadOnlyCell::set_value(const Variant &p_value) {
	value = p_value;
	if (value.get_type() == Variant::NIL) {
		text->set_placeholder("");
	} else {
		text->set_placeholder(value);
	}
}

ReadOnlyCell::ReadOnlyCell(
		const int &p_row,
		const int &p_column,
		const Variant &p_value,
		const Size2i &p_minimum_size,
		const bool &p_focusable) :
		TextCell(p_row, p_column, String(), p_minimum_size) {
	focus_control_theme.read_only = get_theme_stylebox(SNAME("normal"), SNAME("Button"));
	text->add_theme_constant_override("minimum_character_width", 0);
	text->set_editable(false);
	text->set_context_menu_enabled(false);
	set_editable(false);
	set_focusable(p_focusable);
	set_value(p_value);
}

//IntegerCell
void IntegerCell::_focus_exited() {
	Cell::_focus_exited();
	String txt = text->get_text();
	if (txt.is_valid_int() && !value.hash_compare(txt.to_int())) {
		_value_changed(txt);
	}
}

void IntegerCell::_value_changed(const Variant &p_value) {
	String string_value = p_value;
	if (string_value.is_valid_int()) {
		int new_value = string_value.to_int();
		_emit_value_changed(value, new_value);
		value = new_value;
	} else {
		int cursor_pos = text->get_caret_column() - 1;
		text->set_text(value);
		text->set_caret_column(cursor_pos);
	}
}

void IntegerCell::set_value(const Variant &p_value) {
	if (_set_value(p_value, Variant())) {
		text->set_text(value);
	} else {
		text->set_text("");
	}
}

IntegerCell::IntegerCell(
		const int &p_row,
		const int &p_column,
		const Variant &p_value,
		const Size2i &p_minimum_size) :
		TextCell(p_row, p_column, p_value, p_minimum_size) {}

//FloatCell
void FloatCell::_focus_exited() {
	Cell::_focus_exited();
	String txt = text->get_text();
	if (txt.is_valid_float() && !value.hash_compare(txt.to_float())) {
		_value_changed(txt);
	}
}

void FloatCell::_value_changed(const Variant &p_value) {
	String string_value = p_value;
	if (string_value.is_valid_float()) {
		float new_value = string_value.to_float();
		_emit_value_changed(value, new_value);
		value = new_value;
	} else {
		int cursor_pos = text->get_caret_column() - 1;
		text->set_text(value);
		text->set_caret_column(cursor_pos);
	}
}

void FloatCell::set_value(const Variant &p_value) {
	if (_set_value(p_value, Variant())) {
		text->set_text(value);
	} else {
		text->set_text("");
	}
}

FloatCell::FloatCell(
		const int &p_row,
		const int &p_column,
		const Variant &p_value,
		const Size2i &p_minimum_size) :
		TextCell(p_row, p_column, p_value, p_minimum_size) {}

//BoolCell
void BoolCell::set_value(const Variant &p_value) {
	_set_value(p_value, false);
	check_box->set_pressed_no_signal(p_value);
}

BoolCell::BoolCell(
		const int &p_row,
		const int &p_column,
		const Variant &p_value,
		const Size2i &p_minimum_size) :
		Cell(p_row, p_column, p_minimum_size) {
	check_box = memnew(ToggleBox);
	check_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	check_box->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	check_box->connect("toggled", callable_mp((Cell *)this, &BoolCell::_value_changed));
	add_edit_control(check_box);
	set_value(p_value);
}

//ColorCell
void ColorCell::set_value(const Variant &p_value) {
	if (_set_value(p_value, Variant())) {
		picker->set_pick_color(value);
	} else {
		picker->set_pick_color(Color(0, 0, 0, 1));
	}
}

ColorCell::ColorCell(
		const int &p_row,
		const int &p_column,
		const Variant &p_value,
		const Size2i &p_minimum_size) :
		Cell(p_row, p_column, p_minimum_size) {
	picker = memnew(ColorPickerButton);
	picker->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	picker->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	picker->connect("color_changed", callable_mp((Cell *)this, &ColorCell::_value_changed));
	add_edit_control(picker);
	set_value(p_value);
}

//ButtonCell
void ButtonCell::set_value(const Variant &p_value) {
	_set_value(p_value, Variant());
}

ButtonCell::ButtonCell(
		const int &p_row,
		const int &p_column,
		const String &p_text,
		const Size2i &p_minimum_size) :
		Cell(p_row, p_column, p_minimum_size) {
	value_type = Variant::NIL;
	button = memnew(Button);
	button->connect("pressed", callable_mp((Cell *)this, &ButtonCell::_expand_editor));
	button->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	button->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	button->set_icon(EditorNode::get_singleton()->get_gui_base()->get_theme_icon("Edit", "EditorIcons"));
	button->set_icon_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_RIGHT);
	button->set_text(p_text);
	add_edit_control(button);
}

ButtonCell::ButtonCell(
		const int &p_row,
		const int &p_column,
		const Variant &p_value,
		const Variant::Type &p_type,
		const Size2i &p_minimum_size) :
		ButtonCell(p_row, p_column, Utility::get_type_custom_name(p_type)) {
	value_type = p_type;
	set_value(p_value);
}
