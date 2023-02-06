/**
 * node_context_ref.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "scene/main/node.h"
#include "spike_define.h"

template <class T>
class NodeContextRef : public Object {
	Map<ObjectID, List<T>> context_map;

	void context_exited(Node *p_context) {
		if (unregister == nullptr) {
			WARN_PRINT("unregister is null");
			return;
		}

		ObjectID id = p_context->get_instance_id();
		if (context_map.has(id)) {
			for (auto item : context_map[id]) {
				unregister(item);
			}
			context_map.erase(id);
		}
	}

public:
	void (*unregister)(T &);

	void make_ref(Node *p_context, const T &p_item) {
		if (unregister == nullptr) {
			WARN_PRINT("unregister is null");
			return;
		}

		ObjectID id = p_context->get_instance_id();
		if (context_map.has(id)) {
			context_map[id].push_back(p_item);
		} else {
			List<T> list;
			list.push_back(p_item);
			context_map.insert(id, list);

			p_context->connect("tree_exited", callable_mp(this, &NodeContextRef::context_exited).bind(p_context));
		}
	}
};
