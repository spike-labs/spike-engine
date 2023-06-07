/**
 * modular_graph_loader.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#include "modular_graph_loader.h"
#include "core/io/json.h"
#include "scene/main/window.h"

#define JSON_KEY_GRAPH_VER ".graph_version"
#define JSON_KEY_GRAPH_NODES "nodes"

ModularGraphLoader *ModularGraphLoader::singleton = nullptr;

void ModularGraphLoader::from_json_v0(List<Task>::Element *p_task, const Dictionary &p_dic) {
	auto &task = p_task->get();
	auto &p_json = task.modular_url;
	auto &p_handler = task.handler;
	String dir_path = p_json.get_base_dir();
	String dir_name = dir_path.get_file();

	auto &node_list = task.node_list;
	PackedStringArray sub_graphs;

	for (const Variant *key = p_dic.next(nullptr); key; key = p_dic.next(key)) {
		String node_key = *key;
		String value = p_dic[node_key];
		String node_id = node_key.get_basename();
		if (IS_EMPTY(node_key.get_extension())) {
			sub_graphs.push_back(PATH_JOIN(PATH_JOIN(dir_path, node_key), MODULAR_LAUNCH));
		} else {
			String type;
			if (!IS_EMPTY(value) && node_id != dir_name) {
				type = ".";
			}
			void *node = p_handler->add_node(-1, node_key, type);
			if (type == String()) {
				task.main_node = node;
			} else {
				node_list.push_back(node);
			}
		}
	}

	for (int i = 0; i < sub_graphs.size(); ++i) {
		//queue_load(PATH_JOIN(sub_graphs[i], MODULAR_LAUNCH), p_handler, p_task.depth + 1);
	}
}

/**
 * @brief JSON structure
 * {
 *      ".graph_version" : 1,
 * 	    "nodes" : [
 * 	        { "source": "1.pck", "engine" : 0x100, },
 * 	        { "source": "2.pck", "engine" : 0x100, },
 * 	        { "source": "3", },
 * 	        { "source": "tr.pck", "engine" : 0x100, "type" : "tr" },
 * 	        { "source": "conf.pck", "engine" : 0x100, "type" : "conf" },
 * 		],
 * }
 */
void ModularGraphLoader::from_json_v1(List<Task>::Element *p_task, const Dictionary &p_dic) {
	auto &task = p_task->get();
	String dir_path = task.modular_url.get_base_dir();
	String dir_name = dir_path.get_file();

	Array nodes = p_dic[JSON_KEY_GRAPH_NODES];
	auto &node_list = task.node_list;

	PackedStringArray sub_graphs;
	for (int i = 0; i < nodes.size(); ++i) {
		Dictionary node = nodes[i];
		String source = node["source"];
		String full_source = source;
		if (source.is_relative_path()) {
			full_source = PATH_JOIN(dir_path, source).simplify_path();
		}
		auto engine_ptr = node.getptr("engine");
		if (engine_ptr == nullptr) {
			if (task.main_node == Variant()) {
				auto node = task.handler->add_node(-1, full_source, "");
				task.main_node = node;
				if (node == Variant()) {
					sub_graphs.push_back(full_source);
				}
			} else {
				sub_graphs.push_back(full_source);
			}
		} else {
			uint32_t engine = *engine_ptr;
			if (task.main_node == Variant()) {
				auto node = task.handler->add_node(engine, full_source, "");
				task.main_node = node;
			} else {
				String type = ".";
				auto type_ptr = node.getptr("type");
				if (type_ptr) {
					type = *type_ptr;
				}
				auto node = task.handler->add_node(engine, full_source, type);
				node_list.push_back(node);
			}
		}
	}

	for (int i = 0; i < sub_graphs.size(); ++i) {
		_queue_sub_task(PATH_JOIN(sub_graphs[i], MODULAR_LAUNCH), p_task);
	}
}

void ModularGraphLoader::_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	Error error;
	Dictionary data;
	String response_json;
	Ref<JSON> jason;

	if (p_result != OK) {
		ELog("Modular request invalid: \"%s\"", current_task->get().modular_url);
		goto LOAD_TASK;
	}
	if (p_code != 200) {
		ELog("Modular request failure: \"%s\"", current_task->get().modular_url);
		goto LOAD_TASK;
	}

	error = response_json.parse_utf8((const char *)p_body.ptr(), p_body.size());
	if (error != OK) {
		goto LOAD_TASK;
	}

	jason.instantiate();
	error = jason->parse(response_json);
	if (error != OK) {
		ELog("Modular data parse failure: \"%s\"", current_task->get().modular_url);
		goto LOAD_TASK;
	}

	data = jason->get_data();

LOAD_TASK:
	MessageQueue::get_singleton()->push_callable(callable_mp(this, &ModularGraphLoader::_load_current_task), data);
}

void ModularGraphLoader::_begin_task(List<Task>::Element *p_task) {
	current_task = p_task;

	auto &task = current_task->get();
	if (task.modular_url.begins_with("http")) {
		request(task.modular_url);
	} else {
		Error err;
		Ref<FileAccess> f = FileAccess::open(task.modular_url, FileAccess::READ, &err);
		if (f.is_valid()) {
			auto data = JSON::parse_string(f->get_as_text());
			MessageQueue::get_singleton()->push_callable(callable_mp(this, &ModularGraphLoader::_load_current_task), data);
		} else {
			ELog("Modular file not found: \"%s\"", task.modular_url);
		}
	}
}

void ModularGraphLoader::_end_task(List<Task>::Element *p_task) {
	auto &task = p_task->get();
	for (const auto &node : task.node_list) {
		task.handler->add_connection(node, task.main_node);
	}

	auto parent_task = p_task->prev();
	if (parent_task) {
		parent_task->get().node_list.push_back(task.main_node);
	} else {
		task.handler->loaded();
	}
}

void ModularGraphLoader::_load_from_json(List<Task>::Element *p_task, const Dictionary &p_dic) {
	int ver = -1;
	if (!p_dic.is_empty()) {
		auto ver_ptr = p_dic.getptr(JSON_KEY_GRAPH_VER);
		ver = ver_ptr ? (int)*ver_ptr : 0;
	}
	switch (ver) {
		case 1:
			from_json_v1(p_task, p_dic);
			break;
		case 0:
			from_json_v0(p_task, p_dic);
			break;
	}

	if (p_task->get().main_node == Variant()) {
		ELog("Unsupport Modular Graph Version: '%d' (%s)", ver, p_task->get().modular_url);
	}

	for (;;) {
		auto next_itor = p_task->next();
		if (next_itor) {
			int next_depth = next_itor->get().depth;
			if (next_depth > p_task->get().depth) {
				// Begin child task
				_begin_task(next_itor);
				break;
			} else if (next_depth == p_task->get().depth) {
				_end_task(p_task);
				p_task->erase();
				// Begin siblind task
				_begin_task(next_itor);
				break;
			}
		}

		// Eng this task, goto parent task
		_end_task(p_task);
		auto prev_task = p_task->prev();
		p_task->erase();
		if (prev_task == nullptr) {
			break;
		}
		p_task = prev_task;
	}
}

void ModularGraphLoader::_load_current_task(const Dictionary &p_data) {
	_load_from_json(current_task, p_data);
}

void ModularGraphLoader::_queue_sub_task(const String &p_json, List<Task>::Element *p_task) {
	Task task;
	task.depth = p_task->get().depth + 1;
	task.modular_url = p_json;
	task.handler = p_task->get().handler;
	task_queue.insert_after(p_task, task);
}

void ModularGraphLoader::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			connect("request_completed", callable_mp(this, &ModularGraphLoader::_request_completed));
			if (task_queue.size()) {
				_begin_task(task_queue.front());
			}
			break;
		default:
			break;
	}
}

void ModularGraphLoader::_bind_methods() {
}

void ModularGraphLoader::queue_load(const String &p_json, const Ref<DataHandler> &p_handler) {
	Task task;
	task.depth = 0;
	task.modular_url = p_json;
	task.handler = p_handler;
	task_queue.push_back(task);

	if (get_parent() && task_queue.size() == 1) {
		_begin_task(task_queue.front());
	}
}

ModularGraphLoader::ModularGraphLoader() {
}

ModularGraphLoader *ModularGraphLoader::get_singleton() {
	if (singleton == nullptr) {
		singleton = memnew(ModularGraphLoader);
		auto scene_tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
		if (scene_tree) {
			scene_tree->get_root()->call_deferred("add_child", singleton);
		}
	}
	return singleton;
}
