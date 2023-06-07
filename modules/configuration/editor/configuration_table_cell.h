/**
 * configuration_table_cell.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "../configuration_table.h"
#include "configuration_table_utility.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "spike/editor/variant_editor.h"
#include "spike_define.h"

#define CELL_MIN_WIDTH 120
#define CELL_MIN_HEIGHT 30
#define CELL_MIN_SIZE Size2i(CELL_MIN_WIDTH, CELL_MIN_HEIGHT)
#define ORIGIN_COLUMN_CELL_MIN_SIZE Size2i(60, CELL_MIN_HEIGHT)

class ToggleBox : public Button {
	GDCLASS(ToggleBox, Button);

	struct ThemeCache {
		Ref<Texture2D> checked;
		Ref<Texture2D> unchecked;
	} theme_cache;

protected:
	Size2 get_icon_size() const {
		Size2 tex_size = Size2(0, 0);
		if (!theme_cache.checked.is_null()) {
			tex_size = Size2(theme_cache.checked->get_width(), theme_cache.checked->get_height());
		}
		if (!theme_cache.unchecked.is_null()) {
			tex_size = Size2(MAX(tex_size.width, theme_cache.unchecked->get_width()), MAX(tex_size.height, theme_cache.unchecked->get_height()));
		}
		return tex_size;
	}

	Size2 get_minimum_size() const override {
		Size2 minsize = Button::get_minimum_size();
		Size2 tex_size = get_icon_size();
		minsize.width += tex_size.width;
		minsize.height = MAX(minsize.height, tex_size.height);
		return minsize;
	}

	virtual void _update_theme_item_cache() override {
		Button::_update_theme_item_cache();
		theme_cache.checked = get_theme_icon(SNAME("checked"), SNAME("CheckBox"));
		theme_cache.unchecked = get_theme_icon(SNAME("unchecked"), SNAME("CheckBox"));
	}

	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_ENTER_TREE: {
				add_theme_style_override("pressed", get_theme_stylebox(SNAME("normal")));
				add_theme_style_override("hover", get_theme_stylebox(SNAME("normal")));
			} break;

			case NOTIFICATION_DRAW: {
				RID ci = get_canvas_item();
				Ref<Texture2D> on_tex = theme_cache.checked;
				Ref<Texture2D> off_tex = theme_cache.unchecked;
				Vector2 ofs;
				ofs.x = int((get_size().x - get_icon_size().width) / 2);
				ofs.y = int((get_size().height - get_icon_size().height) / 2);
				if (is_pressed()) {
					on_tex->draw(ci, ofs);
				} else {
					off_tex->draw(ci, ofs);
				}
			} break;
		}
	}

public:
	ToggleBox() :
			Button(String()) {
		set_toggle_mode(true);
	}
};

class Cell : public MarginContainer {
	GDCLASS(Cell, MarginContainer);

protected:
	static void _bind_methods();

protected:
	struct ThemeCache {
		Ref<Texture2D> patched;
	} theme_cache;

	Variant value;
	bool edit_control_mouse_focused = false;
	bool is_patched = false;

	bool _set_value(const Variant &p_value, const Variant &p_default);

	void _emit_value_changed(const Variant &p_old_value, const Variant &p_new_value);
	void _emit_focus_entered();
	void _emit_focus_exited();
	void _emit_right_click();

	virtual void _value_changed(const Variant &p_new_value);
	virtual void _focus_entered();
	virtual void _focus_exited();

	void _focus_button_focus_entered();
	void _focus_button_focus_exited();
	void _focus_button_gui_input(const Ref<InputEvent> &p_event);
	void _focus_button_draw();

	void _focus_control_mouse_entered();
	void _focus_control_mouse_exited();
	void _focus_control_focus_exited();

	void _expand_editor();

	void _update_theme_item_cache() override;
	void _notification(int p_what);

public:
	int row;
	int column;
	Button *focus_button = nullptr;
	Control *edit_control = nullptr;
	struct FocusControlTheme {
		Ref<StyleBox> normal;
		Ref<StyleBox> hover;
		Ref<StyleBox> pressed;
		Ref<StyleBox> disabled;
		Ref<StyleBox> read_only;
	} focus_control_theme;

	virtual void set_focusable(bool p_focusable = true);
	virtual void set_editable(bool p_editable);
	virtual void add_edit_control(Control *p_control);

	virtual Variant::Type get_value_type() const = 0;
	virtual void set_value(const Variant &p_value) = 0;
	virtual const Variant &get_value() const { return value; }

	void update_patched(const Ref<ConfigurationTable> &p_table);
	void focus() { focus_button->grab_focus(); }

	Cell(const int &p_row, const int &p_column, const Size2i &p_minimum_size);
};

class TypeCell : public Cell {
	GDCLASS(TypeCell, Cell);

protected:
	virtual void _value_changed(const Variant &p_value) override;

public:
	OptionButton *option;

	virtual Variant::Type get_value_type() const override { return Variant::INT; }
	virtual void set_value(const Variant &p_value) override;
	virtual void set_editable(bool p_editable) override;

	TypeCell(
			const int &p_row,
			const int &p_column,
			const Variant &p_type = Variant(),
			const Size2i &p_minimum_size = CELL_MIN_SIZE);
};

class TextCell : public Cell {
	GDCLASS(TextCell, Cell);

protected:
	virtual void _focus_exited() override;

public:
	LineEdit *text;

	virtual Variant::Type get_value_type() const override { return Variant::STRING; }
	virtual void set_value(const Variant &p_value) override;
	virtual void set_editable(bool p_editable) override;

	TextCell(
			const int &p_row,
			const int &p_column,
			const Variant &p_value = Variant(),
			const Size2i &p_minimum_size = CELL_MIN_SIZE);
};

class ReadOnlyCell : public TextCell {
	GDCLASS(ReadOnlyCell, TextCell);

public:
	virtual Variant::Type get_value_type() const override { return Variant::STRING; }
	virtual void set_value(const Variant &p_value) override;

	ReadOnlyCell(
			const int &p_row,
			const int &p_column,
			const Variant &p_value = Variant(),
			const Size2i &p_minimum_size = CELL_MIN_SIZE,
			const bool &p_focusable = true);
};

class IntegerCell : public TextCell {
	GDCLASS(IntegerCell, TextCell);

protected:
	virtual void _value_changed(const Variant &p_value) override;
	virtual void _focus_exited() override;

public:
	virtual Variant::Type get_value_type() const override { return Variant::INT; }
	virtual void set_value(const Variant &p_value) override;

	IntegerCell(
			const int &p_row,
			const int &p_column,
			const Variant &p_value = Variant(),
			const Size2i &p_minimum_size = CELL_MIN_SIZE);
};

class FloatCell : public TextCell {
	GDCLASS(FloatCell, Cell);

protected:
	virtual void _value_changed(const Variant &p_value) override;
	virtual void _focus_exited() override;

public:
	virtual Variant::Type get_value_type() const override { return Variant::FLOAT; }
	virtual void set_value(const Variant &p_value) override;

	FloatCell(
			const int &p_row,
			const int &p_column,
			const Variant &p_value = Variant(),
			const Size2i &p_minimum_size = CELL_MIN_SIZE);
};

class BoolCell : public Cell {
	GDCLASS(BoolCell, Cell);

public:
	ToggleBox *check_box;

	virtual Variant::Type get_value_type() const override { return Variant::BOOL; }
	virtual void set_value(const Variant &p_value) override;

	BoolCell(
			const int &p_row,
			const int &p_column,
			const Variant &p_value = Variant(),
			const Size2i &p_minimum_size = CELL_MIN_SIZE);
};

class ColorCell : public Cell {
	GDCLASS(ColorCell, Cell);

public:
	ColorPickerButton *picker = nullptr;

	virtual Variant::Type get_value_type() const override { return Variant::COLOR; }
	virtual void set_value(const Variant &p_value) override;

	ColorCell(
			const int &p_row,
			const int &p_column,
			const Variant &p_value = Variant(),
			const Size2i &p_minimum_size = CELL_MIN_SIZE);
};

class ButtonCell : public Cell {
	GDCLASS(ButtonCell, Cell);

	Variant::Type value_type;

public:
	Button *button;

	virtual Variant::Type get_value_type() const override { return value_type; }
	virtual void set_value(const Variant &p_value) override;

	ButtonCell(
			const int &p_row,
			const int &p_column,
			const String &p_text,
			const Size2i &p_minimum_size = CELL_MIN_SIZE);

	ButtonCell(
			const int &p_row,
			const int &p_column,
			const Variant &p_value = Variant(),
			const Variant::Type &p_type = Variant::OBJECT,
			const Size2i &p_minimum_size = CELL_MIN_SIZE);
};