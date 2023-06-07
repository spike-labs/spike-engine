/**
 * spike_utils_define.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#pragma once

#include "scene/main/node.h"

template <class T>
extern T *find_parent_type(const Node *p_node) {
	if (p_node) {
		auto p = p_node->get_parent();
		if (p) {
			T *r = Object::cast_to<T>(p);
			if (r)
				return r;
			return find_parent_type<T>(p);
		}
	}
	return nullptr;
}

#ifdef TOOLS_ENABLED
#define EDITOR_THEME(cat, name, theme) EditorNode::get_singleton()->get_gui_base()->get_theme_##cat(name, theme)
#else

#endif