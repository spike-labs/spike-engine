/**
 * configuration_table.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "configuration_set.h"
#include "core/variant/typed_array.h"
#include "spike_define.h"

#define ORIGIN_COLUMN -1
#define ORIGIN_ROW -3
#define TYPE_ROW -2
#define NAME_ROW -1

class ConfigurationTableColumn : public RefCounted {
	GDCLASS(ConfigurationTableColumn, RefCounted);

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("size"), &ConfigurationTableColumn::size);
		ClassDB::bind_method(D_METHOD("get_column"), &ConfigurationTableColumn::get_column);
		ClassDB::bind_method(D_METHOD("get_type"), &ConfigurationTableColumn::get_type);
		ClassDB::bind_method(D_METHOD("get_name"), &ConfigurationTableColumn::get_name);
		ClassDB::bind_method(D_METHOD("values"), &ConfigurationTableColumn::values);
		ClassDB::bind_method(D_METHOD("get_at", "row"), &ConfigurationTableColumn::get_at);
	}

private:
	int _column;
	int _type;
	String _name;
	Array _values;

public:
	int size() const { return _values.size(); }
	int get_column() const { return _column; }
	int get_type() const { return _type; }
	String get_name() const { return _name; }
	const Array &values_ref() const { return _values; }
	Array *values_ptr() { return &_values; }
	Array values() const { return _values; }
	Variant get_at(const int &p_row) const {
		if (p_row < _values.size()) {
			return _values.get(p_row);
		}
		return Variant();
	}
	ConfigurationTableColumn() {}
	ConfigurationTableColumn(
			const int &p_column,
			const int &p_type,
			const Variant &p_name,
			const Array &p_values = Array()) :
			_column(p_column),
			_type(p_type),
			_name(p_name) {}
};

class ConfigurationTableRow : public RefCounted {
	GDCLASS(ConfigurationTableRow, RefCounted);

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("size"), &ConfigurationTableRow::size);
		ClassDB::bind_method(D_METHOD("get_row"), &ConfigurationTableRow::get_row);
		ClassDB::bind_method(D_METHOD("keys"), &ConfigurationTableRow::keys);
		ClassDB::bind_method(D_METHOD("values"), &ConfigurationTableRow::values);
		ClassDB::bind_method(D_METHOD("get_at", "column"), &ConfigurationTableRow::get_at);
	}

private:
	int _row;
	Vector<String> _keys;
	Array _values;

protected:
	bool _get(const StringName &p_name, Variant &r_ret) const {
		int index = _keys.find(p_name);
		if (index >= 0) {
			r_ret = _values.get(index);
			return true;
		}
		return false;
	}

public:
	int size() const { return _values.size(); }
	int get_row() const { return _row; }
	const Vector<String> &keys_ref() const { return _keys; }
	Vector<String> *keys_ptr() { return &_keys; }
	Vector<String> keys() const { return _keys; }
	const Array &values_ref() const { return _values; }
	Array *values_ptr() { return &_values; }
	Array values() const { return _values; }
	Variant get_at(const int &p_column) const {
		if (p_column < _values.size()) {
			return _values.get(p_column);
		}
		return Variant();
	}
	ConfigurationTableRow() {}
	ConfigurationTableRow(
			const int &p_row,
			const Array &p_values = Array()) :
			_row(p_row),
			_values(p_values) {}
};

class ConfigurationTable : public ConfigurationResource {
	GDCLASS(ConfigurationTable, ConfigurationResource);

	struct ConfigurationDescriptionKey {
		Vector<Variant> keys;
		bool operator==(const ConfigurationDescriptionKey &p_b) const {
			int a_key_count = keys.size();
			int b_key_count = p_b.keys.size();
			if (a_key_count != b_key_count) {
				return false;
			} else {
				const Variant *a_ptr = keys.ptr();
				const Variant *b_ptr = p_b.keys.ptr();
				for (int i = 0; i < a_key_count; i++) {
					const Variant &a = a_ptr[i];
					const Variant &b = b_ptr[i];
					if (a != b) {
						return false;
					}
				}
				return true;
			}
		}

		uint32_t hash() const {
			int key_size = keys.size();
			uint32_t h = hash_murmur3_one_32(key_size);
			const Variant *ptr = keys.ptr();
			for (int i = 0; i < key_size; i++) {
				h = hash_murmur3_one_32(ptr[i].hash(), h);
			}
			return hash_fmix32(h);
		}
	};

	struct ConfigurationDescriptionHash {
		static _FORCE_INLINE_ uint32_t hash(const ConfigurationDescriptionKey &p_key) {
			return p_key.hash();
		}
	};

protected:
	static void _bind_methods();

private:
	int rows = 0;
	int columns = 0;
	Dictionary table;

	HashMap<int, Ref<ConfigurationTableColumn>> columns_cache;
	HashMap<int, Ref<ConfigurationTableRow>> rows_cache;
	HashMap<ConfigurationDescriptionKey, Ref<ConfigurationTableRow>, ConfigurationDescriptionHash> rows_map;

	Variant _get_cell(const int &p_row, const int &p_column, const Variant &p_default = Variant()) const;
	void _set_cell(const int &p_row, const int &p_column, const Variant &p_value);

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	int get_editable_rows() const;
	int get_editable_columns() const;

	int get_row_count() const;
	int get_column_count() const;

	Ref<ConfigurationTableColumn> get_column(const int &p_column);
	void set_column(const Ref<ConfigurationTableColumn> &p_column);

	Ref<ConfigurationTableRow> get_row(const int &p_row);
	void set_row(const Ref<ConfigurationTableRow> p_row);
	Ref<ConfigurationTableRow> find_row(const Dictionary &p_combination);

	String get_column_name(const int &p_column) const;
	void set_column_name(const int &p_column, const String &p_name);

	int get_column_type(const int &p_column) const;
	void set_column_type(const int &p_column, const int &p_type);

	bool has_cell(const int &p_row, const int &p_column) const;
	Variant get_cell(const int &p_row, const int &p_column) const;
	void set_cell(const int &p_row, const int &p_column, const Variant &p_value);

	void insert_row(const int &p_row, const Ref<ConfigurationTableRow> &p_row_data = nullptr);
	void delete_row(const int &p_row);

	void insert_column(const int &p_column, const Ref<ConfigurationTableColumn> &p_column_data = nullptr);
	void delete_column(const int &p_column);

	ConfigurationTable();
	~ConfigurationTable();
};
