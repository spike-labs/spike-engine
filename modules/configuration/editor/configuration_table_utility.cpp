/**
 * configuration_table_utility.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "configuration_table_utility.h"
#include "configuration_table_cell.h"

Array Utility::fill_number_array(const int &p_start, const int &p_size) {
	Array array;
	for (int i = 0; i < p_size; i++) {
		array.append(p_start + i);
	}
	return array;
}

String Utility::COLUMN_HEADER_LETTER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
String Utility::get_column_header(const int &p_index) {
	int latter_count = COLUMN_HEADER_LETTER.length();
	int latter_start_index = p_index / latter_count - 1;
	int letter_index = p_index % latter_count;
	String header = "";
	if (latter_start_index >= 0) {
		header += COLUMN_HEADER_LETTER[latter_start_index];
	}
	header += COLUMN_HEADER_LETTER[letter_index];
	return header;
}

HashMap<Variant::Type, String> Utility::TYPE_MAPPING;
void Utility::init_type_mapping() {
	if (TYPE_MAPPING.size() == 0) {
		TYPE_MAPPING[Variant::STRING] = "String";
		TYPE_MAPPING[Variant::INT] = "int";
		TYPE_MAPPING[Variant::FLOAT] = "float";
		TYPE_MAPPING[Variant::BOOL] = "bool";
		TYPE_MAPPING[Variant::VECTOR2] = "Vector2";
		TYPE_MAPPING[Variant::VECTOR3] = "Vector3";
		TYPE_MAPPING[Variant::COLOR] = "Color";
		TYPE_MAPPING[Variant::PACKED_STRING_ARRAY] = "String[]";
		TYPE_MAPPING[Variant::PACKED_INT32_ARRAY] = "int[]";
		TYPE_MAPPING[Variant::PACKED_FLOAT32_ARRAY] = "float[]";
		TYPE_MAPPING[Variant::PACKED_BYTE_ARRAY] = "byte[]";
		TYPE_MAPPING[Variant::PACKED_VECTOR2_ARRAY] = "Vector2[]";
		TYPE_MAPPING[Variant::PACKED_VECTOR3_ARRAY] = "Vector3[]";
		TYPE_MAPPING[Variant::PACKED_COLOR_ARRAY] = "Color[]";
		TYPE_MAPPING[Variant::DICTIONARY] = "Dictionary";
	}
}

String Utility::get_type_custom_name(const Variant::Type &p_type) {
	return TYPE_MAPPING[p_type];
}

Cell *Utility::create_cell(
		Node *p_parent,
		const int &p_type,
		const int &p_row,
		const int &p_column,
		const Variant &p_value,
		const Size2i &p_minimum_size) {
	Cell *cell = nullptr;
	switch (p_type) {
		case -1:
			cell = memnew(ReadOnlyCell(p_row, p_column, p_value, p_minimum_size));
			break;
		case Variant::STRING:
			cell = memnew(TextCell(p_row, p_column, p_value, p_minimum_size));
			break;
		case Variant::INT:
			cell = memnew(IntegerCell(p_row, p_column, p_value, p_minimum_size));
			break;
		case Variant::FLOAT:
			cell = memnew(FloatCell(p_row, p_column, p_value, p_minimum_size));
			break;
		case Variant::BOOL:
			cell = memnew(BoolCell(p_row, p_column, p_value, p_minimum_size));
			break;
		case Variant::COLOR:
			cell = memnew(ColorCell(p_row, p_column, p_value, p_minimum_size));
			break;
		default:
			cell = memnew(ButtonCell(p_row, p_column, p_value, (Variant::Type)p_type, p_minimum_size));
			break;
	}
	if (nullptr != p_parent) {
		p_parent->add_child(cell);
	}
	return cell;
}