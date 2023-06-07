/**
 * modular_graph_loader.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "scene/main/http_request.h"
#include "spike_define.h"

#define MODULAR_LAUNCH "modular.json"

class ModularGraphLoader : public HTTPRequest {
	GDCLASS(ModularGraphLoader, HTTPRequest)

	static ModularGraphLoader *singleton;

public:
	class DataHandler : public RefCounted {
	public:
		virtual Variant add_node(int p_engine, const String &p_source, const String &p_type) = 0;
		virtual void add_connection(Variant p_from, Variant p_to) = 0;
		virtual void loaded() { };
	};

protected:
	struct Task {
		int depth;
		String modular_url;
		Ref<DataHandler> handler;

		Variant main_node;
		List<Variant> node_list;
	};
	void from_json_v0(List<Task>::Element *p_task, const Dictionary &p_dic);
	void from_json_v1(List<Task>::Element *p_task, const Dictionary &p_dic);

	List<Task> task_queue;
	List<Task>::Element *current_task;

	void _request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	void _begin_task(List<Task>::Element *p_task);
	void _end_task(List<Task>::Element *p_task);
	void _load_from_json(List<Task>::Element *p_task, const Dictionary &p_dic);
	void _load_current_task(const Dictionary &p_data);
	void _queue_sub_task(const String &p_json, List<Task>::Element *p_task);

	void _notification(int p_what);
	static void _bind_methods();

public:
	void queue_load(const String &p_json, const Ref<DataHandler> &p_handler);

	ModularGraphLoader();

	static ModularGraphLoader* get_singleton();

};