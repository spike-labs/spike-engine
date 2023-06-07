/**
 * configuration_table_inspector.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "configuration/configuration_table.h"
#include "editor/editor_inspector.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "spike/editor/variant_editor.h"
#include "spike_define.h"

class Cell;
class TypeCell;
class TextCell;
class HeaderColumn;
class BodyColumn;
class Marquee;
class EditorUndoRedoManager;

class ConfigurationTableInspector : public MarginContainer {
	GDCLASS(ConfigurationTableInspector, MarginContainer);

	enum MenuItems {
		MENU_INSERT_ROW_ABOVE,
		MENU_INSERT_ROW_BELOW,
		MENU_DELETE_ROW,

		MENU_INSERT_COLUMN_LEFT,
		MENU_INSERT_COLUMN_RIGHT,
		MENU_DELETE_COLUMN,
	};

protected:
	static void _bind_methods();

protected:
	EditorUndoRedoManager *undo_redo_mgr;

	Ref<ConfigurationTable> resource;

	int column_count;
	int row_count;

	VBoxContainer *origin_column_header_fixed_container;
	ScrollContainer *origin_column_body_scroll_container;
	VBoxContainer *origin_column_body_scroll_content;

	ScrollContainer *header_scroll_container;
	HBoxContainer *header_scroll_content;
	Marquee *header_marquee;

	ScrollContainer *body_scroll_container;
	HBoxContainer *body_scroll_content;
	Marquee *body_marquee;

	PopupMenu *context_menu;

	void _draw_origin_column(const Size2i p_minimum_size, const int &p_focus = -1);
	void _draw_column(const Ref<ConfigurationTableColumn> &p_column, const Size2i p_minimum_size);
	void _draw_columns(const int &p_focus_column = -1);
	void _draw_rows(const int &p_focus_row = -1);

	void _origin_column_scroll_container_v_scrolling(const float &p_value);
	void _origin_column_cell_focus_entered(Cell *p_cell);
	void _origin_column_cell_focus_exited(Cell *p_cell);
	void _origin_column_cell_right_click(Cell *p_cell);

	void _origin_row_cell_focus_entered(Cell *p_cell);
	void _origin_row_cell_focus_exited(Cell *p_cell);
	void _origin_row_cell_right_click(Cell *p_cell);

	void _header_scroll_content_rect_changed();
	void _header_scroll_container_rect_changed();
	void _header_type_cell_value_changed(Cell *p_cell, const Variant &p_prev, const Variant &p_current);
	void _header_name_cell_value_changed(Cell *p_cell, const Variant &p_prev, const Variant &p_current);

	void _body_scroll_content_rect_changed();
	void _body_scroll_container_rect_changed();
	void _body_scroll_container_h_scrolling(const float &p_value);
	void _body_scroll_container_v_scrolling(const float &p_value);
	void _body_cell_value_changed(Cell *p_cell, const Variant &p_prev, const Variant &p_current);
	void _body_cell_expand_editor(Cell *p_cell);

	void _context_menu_option(const int &p_option, Cell *p_cell);

	void _insert_row(const int &p_row, const Ref<ConfigurationTableRow> &p_row_data = nullptr);
	void _delete_row(const int &p_row);

	void _insert_column(const int &p_column, const Ref<ConfigurationTableColumn> &p_column_data = nullptr);
	void _delete_column(const int &p_column);

	void _notification(int p_what);

public:
	ConfigurationTableInspector();
	~ConfigurationTableInspector();
	void edit(const Ref<ConfigurationTable> &p_resource);
};