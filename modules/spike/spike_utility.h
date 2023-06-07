/**
 * spike_utility.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/io/dir_access.h"
#include "scene/main/node.h"

class SpikeUtility {
public:
	template <class T>
	static T *node_add_child(Node *p_parent, T *p_node) {
		p_parent->add_child(p_node);
		return p_node;
	}

	static void open_dir(const String &p_dir, const Callable &p_reader, const bool &p_get_file_name = true, const bool &p_recursion = false) {
		Ref<DirAccess> dir_access = DirAccess::open(p_dir);
		if (dir_access.is_valid()) {
			dir_access->list_dir_begin();
			String file_name = dir_access->get_next();
			while (!file_name.is_empty()) {
				if (dir_access->current_is_dir()) {
					if (p_recursion && file_name != "." && file_name != "..") {
						open_dir(p_dir, p_reader, p_recursion);
					}
				} else {
					if (!file_name.ends_with(".uids")) {
						Array args;
						args.append(p_dir.path_join(file_name));
						if (p_get_file_name) {
							args.append(file_name);
						}
						p_reader.callv(args);
					}
				}
				file_name = dir_access->get_next();
			}
			dir_access->list_dir_end();
		}
	}
};

#define NODE_ADD_CHILD(m_parent, m_node) SpikeUtility::node_add_child(m_parent, m_node)

#define CREATE_NODE(m_parent, m_class) SpikeUtility::node_add_child(m_parent, memnew(m_class))

#define COND_MET_RETURN(m_cond) \
	if (unlikely(m_cond)) {     \
		return;                 \
	} else                      \
		((void)0)

#define COND_MET_RETURN_V(m_cond, m_ret) \
	if (unlikely(m_cond)) {              \
		return m_ret;                    \
	} else                               \
		((void)0)
