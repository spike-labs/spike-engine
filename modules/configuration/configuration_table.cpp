/**
 * configuration_table.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "configuration_table.h"
#include "configuration_server.h"

#define KEY_FORMAT(r, c) Vector2i(r, c)

void ConfigurationTable::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_cell", "row", "column"), &ConfigurationTable::get_cell);
	ClassDB::bind_method(D_METHOD("get_row_count"), &ConfigurationTable::get_row_count);
	ClassDB::bind_method(D_METHOD("get_row", "row"), &ConfigurationTable::get_row);
	ClassDB::bind_method(D_METHOD("find_row", "combination"), &ConfigurationTable::find_row);
	ClassDB::bind_method(D_METHOD("get_column_count"), &ConfigurationTable::get_column_count);
	ClassDB::bind_method(D_METHOD("get_column", "column"), &ConfigurationTable::get_column);
}

bool ConfigurationTable::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "__source") {
		source_path = p_value;
	} else if (p_name == "table") {
		table = p_value;
	} else if (p_name == "rows") {
		rows = p_value;
	} else if (p_name == "columns") {
		columns = p_value;
	} else {
		return false;
	}
	return true;
}

bool ConfigurationTable::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "__source") {
		r_ret = source_path;
	} else if (p_name == "table") {
		r_ret = table;
	} else if (p_name == "rows") {
		r_ret = rows;
	} else if (p_name == "columns") {
		r_ret = columns;
	} else {
		return false;
	}
	return true;
}

void ConfigurationTable::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::STRING, "__source", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	p_list->push_back(PropertyInfo(Variant::INT, "rows", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	p_list->push_back(PropertyInfo(Variant::INT, "columns", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	p_list->push_back(PropertyInfo(Variant::ARRAY, "table", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
}

Variant ConfigurationTable::_get_cell(const int &p_row, const int &p_column, const Variant &p_default) const {
	if (!is_patch) {
		auto patches = ConfigurationServer::get_patches(get_path());
		if (nullptr != patches && patches->size() > 0) {
			for (int i = patches->size() - 1; i >= 0; i--) {
				Ref<ConfigurationTable> elem = patches->get(i);
				if (elem == this)
					break;
				if (elem.is_valid()) {
					if (elem->has_cell(p_row, p_column)) {
						return elem->get_cell(p_row, p_column);
					}
				}
			}
		}
	}

	Vector2i key = KEY_FORMAT(p_row, p_column);
	if (table.has(key)) {
		return table.get(key, p_default);
	}

	if (is_patch) {
		Ref<ConfigurationTable> source_table = source_res;
		if (source_table.is_valid()) {
			return source_table->_get_cell(p_row, p_column, p_default);
		}
	}

	return p_default;
}

void ConfigurationTable::_set_cell(const int &p_row, const int &p_column, const Variant &p_value) {
	Vector2i key = KEY_FORMAT(p_row, p_column);

	if (is_patch) {
		Ref<ConfigurationTable> source_table = source_res;
		if (source_table.is_valid() && source_table->table.get(key, Variant()) == p_value) {
			if (table.has(key)) {
				table.erase(key);
				return;
			}
		}
	}

	if (p_value.get_type() != Variant::NIL) {
		table[key] = p_value;
	} else if (table.has(key)) {
		table.erase(key);
	}

	rows = MAX(p_row + 1, rows);
	columns = MAX(p_column + 1, columns);
}

int ConfigurationTable::get_editable_rows() const {
	if (is_patch) {
		Ref<ConfigurationTable> source_table = source_res;
		if (source_table.is_valid()) {
			return source_table->rows;
		}
	}
	return rows;
}

int ConfigurationTable::get_editable_columns() const {
	if (is_patch) {
		Ref<ConfigurationTable> source_table = source_res;
		if (source_table.is_valid()) {
			return source_table->columns;
		}
	}
	return columns;
}

int ConfigurationTable::get_row_count() const {
	return rows;
}

int ConfigurationTable::get_column_count() const {
	return columns;
}

String ConfigurationTable::get_column_name(const int &p_column) const {
	return _get_cell(NAME_ROW, p_column, "");
}

void ConfigurationTable::set_column_name(const int &p_column, const String &p_name) {
	_set_cell(NAME_ROW, p_column, p_name.is_empty() ? Variant() : Variant(p_name));
}

int ConfigurationTable::get_column_type(const int &p_column) const {
	return _get_cell(TYPE_ROW, p_column, Variant::STRING);
}

void ConfigurationTable::set_column_type(const int &p_column, const int &p_type) {
	_set_cell(TYPE_ROW, p_column, p_type == Variant::STRING ? Variant() : Variant(p_type));
}

bool ConfigurationTable::has_cell(const int &p_row, const int &p_column) const {
	return table.has(KEY_FORMAT(p_row, p_column));
}

Variant ConfigurationTable::get_cell(const int &p_row, const int &p_column) const {
	return _get_cell(p_row, p_column);
}

void ConfigurationTable::set_cell(const int &p_row, const int &p_column, const Variant &p_value) {
	_set_cell(p_row, p_column, p_value.is_zero() ? Variant() : p_value);
}

Ref<ConfigurationTableColumn> ConfigurationTable::get_column(const int &p_column) {
#ifdef TOOLS_ENABLED
	Ref<ConfigurationTableColumn> column = memnew(ConfigurationTableColumn(p_column, get_column_type(p_column), get_column_name(p_column)));
	for (int r = 0; r < get_editable_rows(); r++) {
		column->values_ptr()->append(get_cell(r, p_column));
	}
	return column;
#else
	if (columns_cache.has(p_column)) {
		return columns_cache[p_column];
	} else {
		Ref<ConfigurationTableColumn> column = memnew(ConfigurationTableColumn(p_column, get_column_type(p_column), get_column_name(p_column)));
		for (int r = 0; r < get_editable_rows(); r++) {
			column->values_ptr()->append(get_cell(r, p_column));
		}
		columns_cache[p_column] = column;
		return column;
	}
#endif
}

void ConfigurationTable::set_column(const Ref<ConfigurationTableColumn> &p_column) {
	if (p_column.is_valid()) {
		set_column_type(p_column->get_column(), p_column->get_type());
		set_column_name(p_column->get_column(), p_column->get_name());
		int row_count = MAX(p_column->size(), get_editable_rows());
		for (int r = 0; r < row_count; r++) {
			set_cell(r, p_column->get_column(), p_column->get_at(r));
		}
	}
}

Ref<ConfigurationTableRow> ConfigurationTable::get_row(const int &p_row) {
#ifdef TOOLS_ENABLED
	Ref<ConfigurationTableRow> row = memnew(ConfigurationTableRow(p_row));
	for (int column = 0; column < get_editable_columns(); column++) {
		row->keys_ptr()->append(get_column_name(column));
		row->values_ptr()->append(get_cell(p_row, column));
	}
	return row;
#else
	if (rows_cache.has(p_row)) {
		return rows_cache[p_row];
	} else {
		Ref<ConfigurationTableRow> row = memnew(ConfigurationTableRow(p_row));
		for (int column = 0; column < get_editable_columns(); column++) {
			row->keys_ptr()->append(get_column_name(column));
			row->values_ptr()->append(get_cell(p_row, column));
		}
		rows_cache[p_row] = row;
		return row;
	}
#endif
}

void ConfigurationTable::set_row(const Ref<ConfigurationTableRow> p_row) {
	if (p_row.is_valid()) {
		int column_count = MAX(p_row->size(), get_editable_columns());
		for (int c = 0; c < column_count; c++) {
			set_cell(p_row->get_row(), c, p_row->get_at(c));
		}
	}
}

Ref<ConfigurationTableRow> ConfigurationTable::find_row(const Dictionary &p_combination) {
	Dictionary dict = (Dictionary)p_combination;
	List<Variant> keys;
	dict.get_key_list(&keys);

	ConfigurationDescriptionKey combination_key;
	List<Variant>::Element *E = keys.front();
	for (; E; E = E->next()) {
		String name = E->get();
		combination_key.keys.append(name);
		combination_key.keys.append(dict[name]);
	}

	if (rows_map.has(combination_key)) {
		return rows_map[combination_key];
	}

	for (int r = 0; r < get_editable_rows(); r++) {
		Ref<ConfigurationTableRow> row = get_row(r);
		ConfigurationDescriptionKey new_key;
		List<Variant>::Element *E = keys.front();
		for (; E; E = E->next()) {
			String name = E->get();
			Variant value = row->get(name);
			if (value.get_type() != Variant::NIL) {
				new_key.keys.append(name);
				new_key.keys.append(value);
			}
		}
		if (new_key == combination_key) {
			rows_map[combination_key] = row;
			return row;
		}
	}

	return nullptr;
}

void ConfigurationTable::insert_row(const int &p_row, const Ref<ConfigurationTableRow> &p_row_data) {
	if (p_row <= rows) {
		Dictionary new_table;
		List<Variant> keys;
		table.get_key_list(&keys);
		for (int i = 0; i < keys.size(); i++) {
			const Vector2i &key = keys[i];
			const int &row = key.x;
			const int &column = key.y;
			if (row < p_row) {
				new_table[key] = table[key];
			} else {
				new_table[KEY_FORMAT(row + 1, column)] = table[key];
			}
		}
		table = new_table;
		if (p_row_data.is_valid()) {
			const Array &cells = p_row_data->values_ref();
			for (int column = 0; column < cells.size(); column++) {
				set_cell(p_row, column, cells[column]);
			}
		}
		rows = rows + 1;
	}
}

void ConfigurationTable::delete_row(const int &p_row) {
	if (p_row < get_editable_rows()) {
		Dictionary new_table;
		List<Variant> keys;
		table.get_key_list(&keys);
		for (int i = 0; i < keys.size(); i++) {
			const Vector2i &key = keys[i];
			const int &row = key.x;
			const int &column = key.y;
			if (row < p_row) {
				new_table[key] = table[key];
			} else if (row > p_row) {
				new_table[KEY_FORMAT(row - 1, column)] = table[key];
			}
		}
		table = new_table;
		rows = rows - 1;
	}
}

void ConfigurationTable::insert_column(const int &p_column, const Ref<ConfigurationTableColumn> &p_column_data) {
	if (p_column <= columns) {
		Dictionary new_table;
		List<Variant> keys;
		table.get_key_list(&keys);
		for (int i = 0; i < keys.size(); i++) {
			const Vector2i &key = keys[i];
			const int &row = key.x;
			const int &column = key.y;
			if (column < p_column) {
				new_table[key] = table[key];
			} else {
				new_table[KEY_FORMAT(row, column + 1)] = table[key];
			}
		}
		table = new_table;
		if (p_column_data.is_valid()) {
			set_column_type(p_column, p_column_data->get_type());
			set_column_name(p_column, p_column_data->get_name());
			const Array &cells = p_column_data->values_ref();
			for (int row = 0; row < cells.size(); row++) {
				set_cell(row, p_column, cells[row]);
			}
		}
		columns = columns + 1;
	}
}

void ConfigurationTable::delete_column(const int &p_column) {
	if (p_column < columns) {
		Dictionary new_table;
		List<Variant> keys;
		table.get_key_list(&keys);
		for (int i = 0; i < keys.size(); i++) {
			const Vector2i &key = keys[i];
			const int &row = key.x;
			const int &column = key.y;
			if (column < p_column) {
				new_table[key] = table[key];
			} else if (column > p_column) {
				new_table[KEY_FORMAT(row, column - 1)] = table[key];
			}
		}
		table = new_table;
		columns = columns - 1;
	}
}

ConfigurationTable::ConfigurationTable() {
}

ConfigurationTable::~ConfigurationTable() {
}