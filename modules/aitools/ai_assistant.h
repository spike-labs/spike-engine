/**
 * ai_assistant.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#pragma once

#include "scene/main/http_request.h"
#include "spike_define.h"

class AIAssistant : public HTTPRequest {
	GDCLASS(AIAssistant, HTTPRequest)
public:
	enum Status {
		STATUS_NONE,
		STATUS_READY,
		STATUS_ERROR,
	};

	enum AIRole {
        ROLE_UNKNWON,
		ROLE_SYSTEM,
		ROLE_ASSISTANT,
		ROLE_USER,
	};

	enum APIType {
		API_NONE,
		API_LIST_MODELS,
		API_CHAT_COMPLETIONS,
	};

private:
	void _handle_list_models(const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _handle_chat_completions(const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

protected:
	APIType api = API_NONE;
	Status status = STATUS_NONE;
	String api_key;
	int current_model;

	Array models;
	Array messages;

	void _notification(int p_what);
	static void _bind_methods();

	PackedStringArray _get_request_headers();
	void set_status(Status p_status);

public:
	void setup(const String &p_apikey);
    void setup_network(const String &p_host, int p_port, bool p_use_threads);
	int get_status();

    int get_model_count();
	String get_model_id(int p_index);
	void select_model(int p_index);
	int get_current_model();

    int get_message_count();
    int get_message_role(int p_index);
    String get_message_content(int p_index);
    void clear_messages();
	void add_message(int p_role, const String &p_content);
	bool send_message(const String &p_content);
};