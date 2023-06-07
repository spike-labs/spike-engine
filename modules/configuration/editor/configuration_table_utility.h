/**
 * configuration_table_utility.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "spike_define.h"

class Cell;

class Utility {
public:
	static Array fill_number_array(const int &p_start, const int &p_size);

	static String COLUMN_HEADER_LETTER;
	static String get_column_header(const int &p_index);

	static HashMap<Variant::Type, String> TYPE_MAPPING;
	static void init_type_mapping();

	static String get_type_custom_name(const Variant::Type &p_type);

	static Cell *create_cell(
			Node *p_parent,
			const int &p_type,
			const int &p_row,
			const int &p_column,
			const Variant &p_value = Variant(),
			const Size2i &p_minimum_size = Size2i());
};