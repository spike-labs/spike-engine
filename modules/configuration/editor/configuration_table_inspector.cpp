/**
 * configuration_table_inspector.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "configuration_table_inspector.h"
#include "configuration_table_cell.h"
#include "core/input/input_event.h"
#include "core/math/expression.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "spike/editor/spike_undo_redo.h"

#define SEPARATION 2
#define TABLE_MIN_ROW 30
#define TABLE_MIN_COLUMN 26
#define ORIGIN_COLUMN_CELL_MIN_SIZE Size2i(60, CELL_MIN_HEIGHT)
class MarqueeRect : public Control {
	GDCLASS(MarqueeRect, Control);

	struct ThemeCache {
		Ref<StyleBoxFlat> rectangle;
		Ref<StyleBoxFlat> border_no_left;
		Ref<StyleBoxFlat> border_no_left_right;
		Ref<StyleBoxFlat> border_no_top;
		Ref<StyleBoxFlat> border_no_top_bottom;
	} theme_cache;

public:
	enum DrawMode {
		RECTANGLE,
		BORDER_NO_LEFT,
		BORDER_NO_LEFT_RIGHT,
		BORDER_NO_TOP,
		BORDER_NO_TOP_BOTTOM,
	};

protected:
	DrawMode draw_mode;

	virtual void _update_theme_item_cache() override {
		Ref<StyleBoxFlat> button_focus = get_theme_stylebox(SNAME("focus"), SNAME("Button"));
		theme_cache.rectangle = button_focus->duplicate();

		theme_cache.border_no_left = button_focus->duplicate();
		theme_cache.border_no_left->set_corner_radius_all(0);
		theme_cache.border_no_left->set_border_width(Side::SIDE_LEFT, 0);

		theme_cache.border_no_left_right = button_focus->duplicate();
		theme_cache.border_no_left_right->set_corner_radius_all(0);
		theme_cache.border_no_left_right->set_border_width(Side::SIDE_LEFT, 0);
		theme_cache.border_no_left_right->set_border_width(Side::SIDE_RIGHT, 0);

		theme_cache.border_no_top = button_focus->duplicate();
		theme_cache.border_no_top->set_corner_radius_all(0);
		theme_cache.border_no_top->set_border_width(Side::SIDE_TOP, 0);

		theme_cache.border_no_top_bottom = button_focus->duplicate();
		theme_cache.border_no_top_bottom->set_corner_radius_all(0);
		theme_cache.border_no_top_bottom->set_border_width(Side::SIDE_TOP, 0);
		theme_cache.border_no_top_bottom->set_border_width(Side::SIDE_BOTTOM, 0);
	}

	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_DRAW: {
				RID ci = get_canvas_item();
				Size2 size = get_size();
				Ref<StyleBox> style;
				if (draw_mode == DrawMode::BORDER_NO_LEFT) {
					style = theme_cache.border_no_left;
				} else if (draw_mode == DrawMode::BORDER_NO_LEFT_RIGHT) {
					style = theme_cache.border_no_left_right;
				} else if (draw_mode == DrawMode::BORDER_NO_TOP) {
					style = theme_cache.border_no_top;
				} else if (draw_mode == DrawMode::BORDER_NO_TOP_BOTTOM) {
					style = theme_cache.border_no_top_bottom;
				} else {
					style = theme_cache.rectangle;
				}
				style->draw(ci, Rect2(Point2(), size));
			} break;
		}
	}

public:
	MarqueeRect() :
			draw_mode(DrawMode::RECTANGLE) {
		set_mouse_filter(MouseFilter::MOUSE_FILTER_IGNORE);
		set_v_size_flags(Control::SIZE_SHRINK_BEGIN);
		set_custom_minimum_size(Size2i(0, CELL_MIN_HEIGHT));
	}

	void set_draw_mode(const DrawMode &p_mode) {
		draw_mode = p_mode;
	}
};

class Marquee : public Control {
	GDCLASS(Marquee, Control);

public:
	ScrollContainer *scroll_container;
	Control *scroll_content;
	MarqueeRect *marquee_rect;
	Marquee() {
		set_v_size_flags(Control::SIZE_EXPAND_FILL);
		set_h_size_flags(Control::SIZE_EXPAND_FILL);
		set_mouse_filter(MouseFilter::MOUSE_FILTER_IGNORE);
		scroll_container = CREATE_NODE(this, ScrollContainer);
		scroll_container->set_mouse_filter(MouseFilter::MOUSE_FILTER_IGNORE);
		scroll_container->get_h_scroll_bar()->set_self_modulate(Color(1, 1, 1, 0));
		scroll_container->get_h_scroll_bar()->set_mouse_filter(MouseFilter::MOUSE_FILTER_IGNORE);
		scroll_container->get_v_scroll_bar()->set_self_modulate(Color(1, 1, 1, 0));
		scroll_container->get_v_scroll_bar()->set_mouse_filter(MouseFilter::MOUSE_FILTER_IGNORE);
		scroll_content = CREATE_NODE(scroll_container, Control);
		scroll_content->set_mouse_filter(MouseFilter::MOUSE_FILTER_IGNORE);
		marquee_rect = CREATE_NODE(scroll_content, MarqueeRect);
		marquee_rect->set_mouse_filter(MouseFilter::MOUSE_FILTER_IGNORE);
	}
};

void ConfigurationTableInspector::_bind_methods() {
	ADD_SIGNAL(MethodInfo("new_modification"));
}

void ConfigurationTableInspector::_draw_origin_column(const Size2i p_minimum_size, const int &p_focus) {
	VBoxContainer *column_vb = origin_column_header_fixed_container;
	if (column_vb->get_child_count() == 0) {
		ReadOnlyCell *cell = CREATE_NODE(column_vb, ReadOnlyCell(0, 0, String(), p_minimum_size, false));
		cell->text->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
		cell = CREATE_NODE(column_vb, ReadOnlyCell(1, 0, STTR("Type"), p_minimum_size, false));
		cell->text->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
		cell = CREATE_NODE(column_vb, ReadOnlyCell(2, 0, STTR("Name"), p_minimum_size, false));
		cell->text->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
	}
	column_vb = origin_column_body_scroll_content;
	for (int r = 0; r < row_count; r++) {
		ReadOnlyCell *cell = nullptr;
		if (r < column_vb->get_child_count()) {
			cell = (ReadOnlyCell *)column_vb->get_child(r);
			cell->row = r;
			cell->column = ORIGIN_COLUMN;
			cell->set_value(r + 1);
		} else {
			cell = CREATE_NODE(column_vb, ReadOnlyCell(r, ORIGIN_COLUMN, r + 1, p_minimum_size));
			cell->text->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
			cell->focus_button->set_default_cursor_shape(CursorShape::CURSOR_POINTING_HAND);
			cell->focus_button->set_focus_mode(FocusMode::FOCUS_ALL);
			cell->connect("cell_right_click", callable_mp(this, &ConfigurationTableInspector::_origin_column_cell_right_click));
			cell->connect("cell_focus_entered", callable_mp(this, &ConfigurationTableInspector::_origin_column_cell_focus_entered));
			cell->connect("cell_focus_exited", callable_mp(this, &ConfigurationTableInspector::_origin_column_cell_focus_exited));
		}
	}
	int free_count = column_vb->get_child_count() - row_count;
	if (free_count > 0) {
		for (int i = 0; i < free_count; i++) {
			column_vb->get_child(i + row_count)->queue_free();
		}
	}

	if (p_focus >= 0) {
		Cell *cell = (Cell *)origin_column_body_scroll_content->get_child(p_focus);
		MessageQueue::get_singleton()->push_callable(callable_mp(cell, &Cell::focus));
	}
}

void ConfigurationTableInspector::_draw_column(const Ref<ConfigurationTableColumn> &p_column, const Size2i p_minimum_size) {
	VBoxContainer *column_vb = nullptr;
	int column_index = p_column->get_column();
	int column_type = p_column->get_type();
	//Header
	ReadOnlyCell *origin_cell = nullptr;
	TypeCell *type_cell = nullptr;
	TextCell *name_cell = nullptr;
	if (column_index < header_scroll_content->get_child_count()) {
		column_vb = (VBoxContainer *)header_scroll_content->get_child(column_index);
		origin_cell = (ReadOnlyCell *)column_vb->get_child(0);
		type_cell = (TypeCell *)column_vb->get_child(1);
		name_cell = (TextCell *)column_vb->get_child(2);
	} else {
		column_vb = CREATE_NODE(header_scroll_content, VBoxContainer);
		column_vb->add_theme_constant_override("separation", SEPARATION);

		origin_cell = CREATE_NODE(column_vb, ReadOnlyCell(ORIGIN_ROW, column_index, Utility::get_column_header(column_index), p_minimum_size));
		origin_cell->text->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
		origin_cell->focus_button->set_default_cursor_shape(CursorShape::CURSOR_POINTING_HAND);
		origin_cell->focus_button->set_focus_mode(FocusMode::FOCUS_ALL);
		origin_cell->connect("cell_right_click", callable_mp(this, &ConfigurationTableInspector::_origin_row_cell_right_click));
		origin_cell->connect("cell_focus_entered", callable_mp(this, &ConfigurationTableInspector::_origin_row_cell_focus_entered));
		origin_cell->connect("cell_focus_exited", callable_mp(this, &ConfigurationTableInspector::_origin_row_cell_focus_exited));

		type_cell = CREATE_NODE(column_vb, TypeCell(TYPE_ROW, column_index, p_column->get_type(), p_minimum_size));
		type_cell->focus_control_theme.normal = get_theme_stylebox(SNAME("normal"), SNAME("Button"));
		type_cell->focus_control_theme.hover = get_theme_stylebox(SNAME("hover"), SNAME("Button"));
		type_cell->focus_control_theme.pressed = get_theme_stylebox(SNAME("pressed"), SNAME("Button"));
		type_cell->connect("cell_value_changed", callable_mp(this, &ConfigurationTableInspector::_header_type_cell_value_changed));

		name_cell = CREATE_NODE(column_vb, TextCell(NAME_ROW, column_index, p_column->get_name(), p_minimum_size));
		name_cell->text->set_horizontal_alignment(HorizontalAlignment::HORIZONTAL_ALIGNMENT_CENTER);
		name_cell->focus_control_theme.normal = get_theme_stylebox(SNAME("normal"), SNAME("Button"));
		name_cell->connect("cell_value_changed", callable_mp(this, &ConfigurationTableInspector::_header_name_cell_value_changed));
	}
	origin_cell->set_value(Utility::get_column_header(column_index));
	type_cell->set_value(p_column->get_type());
	type_cell->set_editable(!resource->is_patch);
	name_cell->set_value(p_column->get_name());
	name_cell->set_editable(!resource->is_patch);

	//Body
	if (column_index < body_scroll_content->get_child_count()) {
		column_vb = (VBoxContainer *)body_scroll_content->get_child(column_index);
	} else {
		column_vb = CREATE_NODE(body_scroll_content, VBoxContainer());
		column_vb->add_theme_constant_override("separation", SEPARATION);
	}
	int child_count = column_vb->get_child_count();
	if (resource->get_column_type(column_index) != column_type) {
		if (child_count > 0) {
			for (int i = child_count - 1; i >= 0; i--) {
				auto child = column_vb->get_child(i);
				column_vb->remove_child(child);
				child->queue_free();
			}
		}
	}
	for (int r = 0; r < row_count; r++) {
		Cell *cell = nullptr;
		if (r < column_vb->get_child_count()) {
			cell = (Cell *)column_vb->get_child(r);
			cell->row = r;
			cell->column = column_index;
		} else {
			cell = Utility::create_cell(column_vb, column_type, r, column_index, Variant(), p_minimum_size);
			cell->connect("cell_value_changed", callable_mp(this, &ConfigurationTableInspector::_body_cell_value_changed));
		}
		cell->set_value(p_column->get_at(r));
		cell->set_editable(!resource->is_patch || p_column->get_column() < resource->get_editable_columns() && r < resource->get_editable_rows());
		cell->update_patched(resource);
	}
	int free_count = get_child_count() - row_count;
	if (free_count > 0) {
		for (int i = 0; i < free_count; i++) {
			get_child(i + row_count)->queue_free();
		}
	}
}

void ConfigurationTableInspector::_draw_columns(const int &p_focus_column) {
	_draw_origin_column(ORIGIN_COLUMN_CELL_MIN_SIZE);

	int draw_column_count = MAX(column_count, 1);
	for (int c = 0; c < draw_column_count; c++) {
		_draw_column(resource->get_column(c), CELL_MIN_SIZE);
	}
	//Header
	int free_count = header_scroll_content->get_child_count() - draw_column_count;
	if (free_count > 0) {
		for (int i = 0; i < free_count; i++) {
			header_scroll_content->get_child(i + draw_column_count)->queue_free();
		}
	}
	//Body
	free_count = body_scroll_content->get_child_count() - draw_column_count;
	if (free_count > 0) {
		for (int i = 0; i < free_count; i++) {
			body_scroll_content->get_child(i + draw_column_count)->queue_free();
		}
	}

	if (p_focus_column >= 0) {
		Cell *focus_column = (Cell *)header_scroll_content->get_child(p_focus_column)->get_child(0);
		MessageQueue::get_singleton()->push_callable(callable_mp(focus_column, &Cell::focus));
	}
}

void ConfigurationTableInspector::_draw_rows(const int &p_focus_row) {
	_draw_origin_column(ORIGIN_COLUMN_CELL_MIN_SIZE, p_focus_row);
	for (int i = 0; i < body_scroll_content->get_child_count(); i++) {
		_draw_column(resource->get_column(i), CELL_MIN_SIZE);
	}
}

void ConfigurationTableInspector::_origin_column_scroll_container_v_scrolling(const float &p_value) {
	body_scroll_container->set_v_scroll(p_value);
}

void ConfigurationTableInspector::_origin_column_cell_focus_entered(Cell *p_cell) {
	Control *body_first_column = (Control *)body_scroll_content->get_child(0);
	Cell *body_first_column_focus_cell = (Cell *)body_first_column->get_child(p_cell->row);
	body_marquee->marquee_rect->set_global_position(body_first_column_focus_cell->get_global_position());
	body_marquee->marquee_rect->set_draw_mode(MarqueeRect::DrawMode::BORDER_NO_LEFT);
	body_marquee->marquee_rect->set_size(Size2i(body_scroll_content->get_size().width, p_cell->get_size().height));
	body_marquee->set_visible(true);
}

void ConfigurationTableInspector::_origin_column_cell_focus_exited(Cell *p_cell) {
	body_marquee->set_visible(false);
}

void ConfigurationTableInspector::_origin_column_cell_right_click(Cell *p_cell) {
	COND_MET_RETURN(resource->is_patch);
	Callable callable = callable_mp(this, &ConfigurationTableInspector::_context_menu_option).bind(p_cell);
	context_menu->clear();
	if (context_menu->is_connected("id_pressed", callable)) {
		context_menu->disconnect("id_pressed", callable);
	}
	context_menu->connect("id_pressed", callable);
	context_menu->add_item(STTR("Insert 1 row above"), MENU_INSERT_ROW_ABOVE);
	context_menu->add_item(STTR("Insert 1 row below"), MENU_INSERT_ROW_BELOW);
	context_menu->add_separator();
	context_menu->add_item(STTR("Delete row"), MENU_DELETE_ROW);
	context_menu->set_position(get_screen_position() + get_local_mouse_position());
	context_menu->reset_size();
	context_menu->popup();
}

void ConfigurationTableInspector::_origin_row_cell_focus_entered(Cell *p_cell) {
	VBoxContainer *header_focus_column = (VBoxContainer *)header_scroll_content->get_child(p_cell->column);
	Cell *type_cell = (Cell *)header_focus_column->get_child(1);
	header_marquee->marquee_rect->set_global_position(type_cell->get_global_position());
	header_marquee->marquee_rect->set_draw_mode(MarqueeRect::DrawMode::BORDER_NO_TOP_BOTTOM);
	header_marquee->marquee_rect->set_size(Size2i(p_cell->get_size().width, header_scroll_content->get_size().height));
	header_marquee->set_visible(true);

	Control *body_focus_column = (Control *)body_scroll_content->get_child(p_cell->column);
	body_marquee->marquee_rect->set_global_position(((Cell *)body_focus_column->get_child(0))->get_global_position());
	body_marquee->marquee_rect->set_draw_mode(MarqueeRect::DrawMode::BORDER_NO_TOP);
	body_marquee->marquee_rect->set_size(Size2i(p_cell->get_size().width, body_scroll_content->get_size().height));
	body_marquee->set_visible(true);
}

void ConfigurationTableInspector::_origin_row_cell_focus_exited(Cell *p_cell) {
	body_marquee->set_visible(false);
	header_marquee->set_visible(false);
}

void ConfigurationTableInspector::_origin_row_cell_right_click(Cell *p_cell) {
	COND_MET_RETURN(resource->is_patch);
	Callable callable = callable_mp(this, &ConfigurationTableInspector::_context_menu_option).bind(p_cell);
	context_menu->clear();
	if (context_menu->is_connected("id_pressed", callable)) {
		context_menu->disconnect("id_pressed", callable);
	}
	context_menu->connect("id_pressed", callable);
	context_menu->add_item(STTR("Insert 1 column left"), MENU_INSERT_COLUMN_LEFT);
	context_menu->add_item(STTR("Insert 1 column right"), MENU_INSERT_COLUMN_RIGHT);
	context_menu->add_separator();
	context_menu->add_item(STTR("Delete column"), MENU_DELETE_COLUMN);
	context_menu->set_position(get_screen_position() + get_local_mouse_position());
	context_menu->reset_size();
	context_menu->popup();
}

void ConfigurationTableInspector::_header_scroll_content_rect_changed() {
	header_scroll_container->set_custom_minimum_size(Size2(0, header_scroll_content->get_size().y));
	header_marquee->scroll_content->set_custom_minimum_size(header_scroll_content->get_size());
}

void ConfigurationTableInspector::_header_scroll_container_rect_changed() {
	header_marquee->scroll_container->set_global_position(header_scroll_container->get_global_position());
	header_marquee->scroll_container->set_size(header_scroll_container->get_size() + Size2(0, SEPARATION));
}

void ConfigurationTableInspector::_header_type_cell_value_changed(Cell *p_cell, const Variant &p_prev, const Variant &p_current) {
	COND_MET_RETURN(nullptr == undo_redo_mgr);

	Ref<ConfigurationTableColumn> old_column = resource->get_column(p_cell->column);
	Ref<ConfigurationTableColumn> new_column = memnew(ConfigurationTableColumn(p_cell->column, p_current, old_column->get_name()));

	undo_redo_mgr->create_action(vformat(TTR("Change cell(%s, %s) value"), p_cell->row, p_cell->column));

	UndoRedo *undo_redo = undo_redo_mgr->get_history_for_object(this).undo_redo;

	undo_redo->add_do_method(callable_mp(resource.ptr(), &ConfigurationTable::set_column).bind(new_column));
	undo_redo->add_undo_method(callable_mp(resource.ptr(), &ConfigurationTable::set_column).bind(old_column));

	BodyColumn *content_column = (BodyColumn *)body_scroll_content->get_child(p_cell->column);
	undo_redo->add_do_method(callable_mp(this, &ConfigurationTableInspector::_draw_column).bind(new_column, CELL_MIN_SIZE));
	undo_redo->add_undo_method(callable_mp(this, &ConfigurationTableInspector::_draw_column).bind(old_column, CELL_MIN_SIZE));

	undo_redo->add_undo_method(callable_mp(p_cell, &Cell::set_value).bind(old_column->get_type()));

	undo_redo_mgr->commit_action();

	emit_signal("new_modification");
}

void ConfigurationTableInspector::_header_name_cell_value_changed(Cell *p_cell, const Variant &p_prev, const Variant &p_current) {
	COND_MET_RETURN(nullptr == undo_redo_mgr);
	undo_redo_mgr->create_action(vformat(TTR("Change cell(%s, %s) value"), p_cell->row, p_cell->column));

	UndoRedo *undo_redo = undo_redo_mgr->get_history_for_object(this).undo_redo;
	undo_redo->add_do_method(callable_mp(resource.ptr(), &ConfigurationTable::set_column_name).bind(p_cell->column, p_current));
	undo_redo->add_undo_method(callable_mp(resource.ptr(), &ConfigurationTable::set_column_name).bind(p_cell->column, p_prev));
	undo_redo->add_undo_method(callable_mp(p_cell, &Cell::set_value).bind(p_prev));

	undo_redo_mgr->commit_action();

	emit_signal("new_modification");
}

void ConfigurationTableInspector::_body_scroll_content_rect_changed() {
	body_marquee->scroll_content->set_custom_minimum_size(body_scroll_content->get_size());
}

void ConfigurationTableInspector::_body_scroll_container_rect_changed() {
	body_marquee->scroll_container->set_global_position(body_scroll_container->get_global_position());
	body_marquee->scroll_container->set_size(body_scroll_container->get_size());
}

void ConfigurationTableInspector::_body_scroll_container_h_scrolling(const float &p_value) {
	header_scroll_container->set_h_scroll(p_value);
	header_marquee->scroll_container->set_h_scroll(p_value);
	body_marquee->scroll_container->set_h_scroll(p_value);
}

void ConfigurationTableInspector::_body_scroll_container_v_scrolling(const float &p_value) {
	origin_column_body_scroll_container->set_v_scroll(p_value);
	body_marquee->scroll_container->set_v_scroll(p_value);
}

void ConfigurationTableInspector::_body_cell_value_changed(Cell *p_cell, const Variant &p_prev, const Variant &p_current) {
	COND_MET_RETURN(nullptr == undo_redo_mgr);
	undo_redo_mgr->create_action(vformat(TTR("Change cell(%s, %s) value"), p_cell->row, p_cell->column));

	UndoRedo *undo_redo = undo_redo_mgr->get_history_for_object(this).undo_redo;
	undo_redo->add_do_method(callable_mp(resource.ptr(), &ConfigurationTable::set_cell).bind(p_cell->row, p_cell->column, p_current));
	undo_redo->add_do_method(callable_mp(p_cell, &Cell::update_patched).bind(resource));
	undo_redo->add_undo_method(callable_mp(resource.ptr(), &ConfigurationTable::set_cell).bind(p_cell->row, p_cell->column, p_prev));
	undo_redo->add_undo_method(callable_mp(p_cell, &Cell::update_patched).bind(resource));
	undo_redo->add_undo_method(callable_mp(p_cell, &Cell::set_value).bind(p_prev));

	undo_redo_mgr->commit_action();

	emit_signal("new_modification");
}

void ConfigurationTableInspector::_context_menu_option(const int &p_option, Cell *p_cell) {
	switch (p_option) {
		case MENU_INSERT_ROW_ABOVE:
		case MENU_INSERT_ROW_BELOW: {
			COND_MET_RETURN(nullptr == undo_redo_mgr);
			int insert_row_index = p_cell->row;
			if (p_option == MENU_INSERT_ROW_BELOW) {
				insert_row_index = insert_row_index + 1;
			}
			undo_redo_mgr->create_action(vformat(TTR("Insert row: %s"), insert_row_index));
			Ref<ConfigurationTableRow> new_row = nullptr;
			UndoRedo *undo_redo = undo_redo_mgr->get_history_for_object(this).undo_redo;
			undo_redo->add_do_method(callable_mp(this, &ConfigurationTableInspector::_insert_row).bind(insert_row_index, new_row));
			undo_redo->add_undo_method(callable_mp(this, &ConfigurationTableInspector::_delete_row).bind(insert_row_index));
			undo_redo_mgr->commit_action();
		} break;

		case MENU_DELETE_ROW: {
			COND_MET_RETURN(nullptr == undo_redo_mgr);
			int delete_row_index = p_cell->row;
			undo_redo_mgr->create_action(vformat(TTR("Delete row: %s"), delete_row_index));
			Ref<ConfigurationTableRow> delete_row_data = resource->get_row(delete_row_index);
			UndoRedo *undo_redo = undo_redo_mgr->get_history_for_object(this).undo_redo;
			undo_redo->add_do_method(callable_mp(this, &ConfigurationTableInspector::_delete_row).bind(delete_row_index));
			undo_redo->add_undo_method(callable_mp(this, &ConfigurationTableInspector::_insert_row).bind(delete_row_index, delete_row_data));
			undo_redo_mgr->commit_action();
		} break;

		case MENU_INSERT_COLUMN_LEFT:
		case MENU_INSERT_COLUMN_RIGHT: {
			COND_MET_RETURN(nullptr == undo_redo_mgr);
			int insert_column_index = p_cell->column;
			if (p_option == MENU_INSERT_COLUMN_RIGHT) {
				insert_column_index = insert_column_index + 1;
			}
			undo_redo_mgr->create_action(vformat(TTR("Insert column: %s"), insert_column_index));
			Ref<ConfigurationTableColumn> new_column = nullptr;
			UndoRedo *undo_redo = undo_redo_mgr->get_history_for_object(this).undo_redo;
			undo_redo->add_do_method(callable_mp(this, &ConfigurationTableInspector::_insert_column).bind(insert_column_index, new_column));
			undo_redo->add_undo_method(callable_mp(this, &ConfigurationTableInspector::_delete_column).bind(insert_column_index));
			undo_redo_mgr->commit_action();
		} break;

		case MENU_DELETE_COLUMN: {
			COND_MET_RETURN(nullptr == undo_redo_mgr);
			int delete_column_index = p_cell->column;
			undo_redo_mgr->create_action(vformat(TTR("Delete column: %s"), delete_column_index));
			Ref<ConfigurationTableColumn> delete_column_data = resource->get_column(delete_column_index);
			UndoRedo *undo_redo = undo_redo_mgr->get_history_for_object(this).undo_redo;
			undo_redo->add_do_method(callable_mp(this, &ConfigurationTableInspector::_delete_column).bind(delete_column_index));
			undo_redo->add_undo_method(callable_mp(this, &ConfigurationTableInspector::_insert_column).bind(delete_column_index, delete_column_data));
			undo_redo_mgr->commit_action();
		} break;
	}
}

void ConfigurationTableInspector::_insert_row(const int &p_row, const Ref<ConfigurationTableRow> &p_row_data) {
	resource->insert_row(p_row, p_row_data);
	row_count = row_count + 1;
	_draw_rows(p_row);
	emit_signal("new_modification");
}

void ConfigurationTableInspector::_delete_row(const int &p_row) {
	resource->delete_row(p_row);
	row_count = MAX(row_count - 1, 0);
	_draw_rows(MIN(p_row, row_count));
	emit_signal("new_modification");
}

void ConfigurationTableInspector::_insert_column(const int &p_column, const Ref<ConfigurationTableColumn> &p_column_data) {
	resource->insert_column(p_column, p_column_data);
	column_count = column_count + 1;
	_draw_columns(p_column);
	emit_signal("new_modification");
}

void ConfigurationTableInspector::_delete_column(const int &p_column) {
	resource->delete_column(p_column);
	column_count = MAX(column_count - 1, 0);
	_draw_columns(MIN(p_column, column_count));
	emit_signal("new_modification");
}

void ConfigurationTableInspector::_notification(int p_what) {
}

void ConfigurationTableInspector::edit(const Ref<ConfigurationTable> &p_resource) {
	resource = p_resource;
	row_count = MAX(TABLE_MIN_ROW, resource->get_editable_rows());
	column_count = MAX(TABLE_MIN_COLUMN, resource->get_editable_columns());
	_draw_columns();
}

ConfigurationTableInspector::ConfigurationTableInspector() {
	undo_redo_mgr = SpikeUndoRedo::get_undo_redo_manager("Configuration");

	Utility::init_type_mapping();

	//Root container
	HBoxContainer *root_container = CREATE_NODE(this, HBoxContainer);
	root_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	root_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root_container->add_theme_constant_override("separation", SEPARATION);

	//Origin column
	VBoxContainer *origin_column_container = CREATE_NODE(root_container, VBoxContainer);
	origin_column_container->add_theme_constant_override("separation", SEPARATION);
	origin_column_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	origin_column_header_fixed_container = CREATE_NODE(origin_column_container, VBoxContainer);
	origin_column_header_fixed_container->add_theme_constant_override("separation", SEPARATION);
	origin_column_body_scroll_container = CREATE_NODE(origin_column_container, ScrollContainer);
	origin_column_body_scroll_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	origin_column_body_scroll_container->set_vertical_scroll_mode(ScrollContainer::ScrollMode::SCROLL_MODE_SHOW_NEVER);
	origin_column_body_scroll_container->set_horizontal_scroll_mode(ScrollContainer::ScrollMode::SCROLL_MODE_SHOW_ALWAYS);
	origin_column_body_scroll_container->get_h_scroll_bar()->set_self_modulate(Color(1, 1, 1, 0));
	origin_column_body_scroll_container->get_v_scroll_bar()->connect("value_changed", callable_mp(this, &ConfigurationTableInspector::_origin_column_scroll_container_v_scrolling));
	origin_column_body_scroll_content = CREATE_NODE(origin_column_body_scroll_container, VBoxContainer);
	origin_column_body_scroll_content->add_theme_constant_override("separation", SEPARATION);

	//Table container
	VBoxContainer *table_container = CREATE_NODE(root_container, VBoxContainer);
	table_container->add_theme_constant_override("separation", SEPARATION);
	table_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	table_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	//Header
	header_scroll_container = CREATE_NODE(table_container, ScrollContainer);
	header_scroll_container->connect("item_rect_changed", callable_mp(this, &ConfigurationTableInspector::_header_scroll_container_rect_changed));
	header_scroll_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header_scroll_container->set_horizontal_scroll_mode(ScrollContainer::ScrollMode::SCROLL_MODE_SHOW_NEVER);
	header_scroll_container->set_vertical_scroll_mode(ScrollContainer::ScrollMode::SCROLL_MODE_SHOW_ALWAYS);
	header_scroll_container->get_v_scroll_bar()->set_self_modulate(Color(1, 1, 1, 0));
	header_scroll_content = CREATE_NODE(header_scroll_container, HBoxContainer);
	header_scroll_content->connect("item_rect_changed", callable_mp(this, &ConfigurationTableInspector::_header_scroll_content_rect_changed));
	header_scroll_content->add_theme_constant_override("separation", SEPARATION);
	header_scroll_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header_marquee = CREATE_NODE(this, Marquee);
	header_marquee->set_visible(false);

	//Body
	body_scroll_container = CREATE_NODE(table_container, ScrollContainer);
	body_scroll_container->connect("item_rect_changed", callable_mp(this, &ConfigurationTableInspector::_body_scroll_container_rect_changed));
	body_scroll_container->get_h_scroll_bar()->connect("value_changed", callable_mp(this, &ConfigurationTableInspector::_body_scroll_container_h_scrolling));
	body_scroll_container->get_v_scroll_bar()->connect("value_changed", callable_mp(this, &ConfigurationTableInspector::_body_scroll_container_v_scrolling));
	body_scroll_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	body_scroll_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	body_scroll_content = CREATE_NODE(body_scroll_container, HBoxContainer);
	body_scroll_content->connect("item_rect_changed", callable_mp(this, &ConfigurationTableInspector::_body_scroll_content_rect_changed));
	body_scroll_content->add_theme_constant_override("separation", SEPARATION);
	body_scroll_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	body_marquee = CREATE_NODE(this, Marquee);
	body_marquee->set_visible(false);

	context_menu = CREATE_NODE(this, PopupMenu());

	VariantEditor::register_editor("CellVariantEditor", CREATE_NODE(this, VariantEditor));
}

ConfigurationTableInspector::~ConfigurationTableInspector() {
	VariantEditor::unregister_editor("CellVariantEditor");
}