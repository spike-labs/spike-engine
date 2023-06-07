/**
 * sectioned_tree.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "editor/editor_inspector.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tree.h"

class SectionedTreeFilter;

class SectionedTree : public HSplitContainer {
	GDCLASS(SectionedTree, HSplitContainer);

	ObjectID obj;
	SectionedTreeFilter *filter = nullptr;

	LineEdit *search_box = nullptr;

	VBoxContainer *left_container = nullptr;
	Tree *sections = nullptr;

	VBoxContainer *right_container = nullptr;

	HashMap<String, TreeItem *> section_map;

	String selected_category;

	bool restrict_to_basic = false;

	static void _bind_methods();
	void _section_selected();

	void _search_changed(const String &p_what);

protected:
	void _notification(int p_what);

public:
	void register_search_box(LineEdit *p_box);
	VBoxContainer *get_left_container();
	VBoxContainer *get_right_container();
	void edit(Object *p_object);
	String get_full_item_path(const String &p_item);

	void set_current_section(const String &p_section);
	String get_current_section() const;

	void set_restrict_to_basic_settings(bool p_restrict);
	void update_category_list();

	SectionedTreeFilter *get_filter() { return filter; }

	SectionedTree();
	~SectionedTree();
};