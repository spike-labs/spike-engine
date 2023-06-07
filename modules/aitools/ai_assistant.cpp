/**
 * ai_assistant.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#include "ai_assistant.h"

#include "core/io/json.h"

static Dictionary parse_json_response(const PackedByteArray &p_body) {
	if (p_body.size()) {
		String response_json;
		const uint8_t *r = p_body.ptr();
		response_json.parse_utf8((const char *)r, p_body.size());

		Ref<JSON> json;
		json.instantiate();
		if (json->parse(response_json) == OK) {
			Dictionary dic = json->get_data();
			if (dic.has("error")) {
				ELog("API Error: \n%s", response_json);
			}
			return dic;
		}
	}
	return Dictionary();
}

void AIAssistant::_handle_list_models(const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	Dictionary data = parse_json_response(p_body);
	Array model_list = data.get("data", Array());
	models.clear();
	for (int i = 0; i < model_list.size(); ++i) {
		Dictionary model = model_list[i];
		String id = model["id"];
		if (id.to_lower().begins_with("gpt")) {
			models.push_back(model);
		}
	}
	set_status(models.size() > 0 ? STATUS_READY : STATUS_ERROR);
}

void AIAssistant::_handle_chat_completions(const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	bool has_error = true;
	String content;
	Dictionary data = parse_json_response(p_body);
	if (data.size()) {
		Array choices;
		choices = data.get("choices", choices);
		if (choices.size() > 0) {
			Dictionary choice = choices.get(0);
			Dictionary message;
			message = choice.get("message", message);
			content = message.get("content", content);
			has_error = false;
			add_message(ROLE_ASSISTANT, content);
		} else {
			Dictionary err = data.get("error", Dictionary());
			content = err.get("message", String());
		}
	}
	if (has_error) {
		emit_signal("message_received", -1, content);
	}
}

void AIAssistant::_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	switch (api) {
		break;
		case API_LIST_MODELS:
			_handle_list_models(p_headers, p_body);
			break;
		case API_CHAT_COMPLETIONS:
			_handle_chat_completions(p_headers, p_body);
			break;

		default:
			break;
	}

	api = API_NONE;
	if (p_result != OK || p_code != 200) {
		WLog("API request failed:%d(%d)", p_result, p_code);
	}
}

void AIAssistant::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			connect("request_completed", callable_mp(this, &AIAssistant::_request_completed));
			break;
		default:
			break;
	}
}

void AIAssistant::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup", "api_key"), &AIAssistant::setup);
	ClassDB::bind_method(D_METHOD("setup_network", "proxy_host", "proxy_port", "use_threads"), &AIAssistant::setup_network);
	ClassDB::bind_method(D_METHOD("get_status"), &AIAssistant::get_status);
	ClassDB::bind_method(D_METHOD("get_model_count"), &AIAssistant::get_model_count);
	ClassDB::bind_method(D_METHOD("get_model_id", "index"), &AIAssistant::get_model_id);
	ClassDB::bind_method(D_METHOD("select_model", "index"), &AIAssistant::select_model);
	ClassDB::bind_method(D_METHOD("get_current_model"), &AIAssistant::get_current_model);
	ClassDB::bind_method(D_METHOD("get_message_count"), &AIAssistant::get_message_count);
	ClassDB::bind_method(D_METHOD("get_message_role", "index"), &AIAssistant::get_message_role);
	ClassDB::bind_method(D_METHOD("get_message_content", "index"), &AIAssistant::get_message_content);
	ClassDB::bind_method(D_METHOD("add_message", "role", "content"), &AIAssistant::add_message);
	ClassDB::bind_method(D_METHOD("send_message", "content"), &AIAssistant::send_message);

	ADD_SIGNAL(MethodInfo("status_changed", PropertyInfo(Variant::INT, "status")));
	ADD_SIGNAL(MethodInfo("message_received", PropertyInfo(Variant::INT, "role"), PropertyInfo(Variant::STRING, "content")));
}

PackedStringArray AIAssistant::_get_request_headers() {
	PackedStringArray headers;
	headers.append("Content-Type: application/json");
	headers.append("Authorization: Bearer " + api_key);
	return headers;
}

void AIAssistant::set_status(Status p_status) {
	status = p_status;
	emit_signal("status_changed", status);
}

void AIAssistant::setup(const String &p_apikey) {
	cancel_request();
	set_status(STATUS_NONE);
	current_model = -1;

	api_key = p_apikey;

	api = API_LIST_MODELS;
	PackedStringArray headers = _get_request_headers();
	request("https://api.openai.com/v1/models", headers);
}

void AIAssistant::setup_network(const String &p_host, int p_port, bool p_use_threads) {
	set_http_proxy(p_host, p_port);
	set_https_proxy(p_host, p_port);
	set_use_threads(p_use_threads);
}

int AIAssistant::get_status() {
	return status;
}

int AIAssistant::get_model_count() {
	return models.size();
}

String AIAssistant::get_model_id(int p_index) {
	if (p_index >= 0 && p_index < models.size()) {
		Dictionary model = models[p_index];
		return model["id"];
	}
	return String();
}

void AIAssistant::select_model(int p_index) {
	current_model = p_index;
}

int AIAssistant::get_current_model() {
	return current_model;
}

int AIAssistant::get_message_count() {
	return messages.size();
}

int AIAssistant::get_message_role(int p_index) {
	if (p_index >= 0 && p_index < messages.size()) {
		Dictionary message = messages[p_index];
		String role = message.get("role", String());
		if (role == "system") {
			return ROLE_SYSTEM;
		} else if (role == "assistant") {
			return ROLE_ASSISTANT;
		} else if (role == "user") {
			return ROLE_USER;
		} else {
			return ROLE_UNKNWON;
		}
	}
	return ROLE_UNKNWON;
}

String AIAssistant::get_message_content(int p_index) {
	if (p_index >= 0 && p_index < messages.size()) {
		Dictionary message = messages[p_index];
		return  message.get("content", String());
	}
	return String();
}

void AIAssistant::clear_messages() {
	messages.clear();
}

void AIAssistant::add_message(int p_role, const String &p_content) {
	char *role_name;
	switch (p_role) {
		case ROLE_SYSTEM:
			role_name = "system";
			break;
		case ROLE_ASSISTANT:
			role_name = "assistant";
			break;
		case ROLE_USER:
			role_name = "user";
			break;
		default:
			return;
	}

	Dictionary msg;
	msg["role"] = role_name;
	msg["content"] = p_content;
	messages.push_back(msg);

	emit_signal("message_received", (int)p_role, p_content);
}

bool AIAssistant::send_message(const String &p_content) {
	if (api != API_NONE)
		return false;

	add_message(ROLE_USER, p_content);

	Dictionary data;
	data["model"] = get_model_id(get_current_model());
	data["messages"] = messages;
	api = API_CHAT_COMPLETIONS;
	PackedStringArray headers = _get_request_headers();
	return request("https://api.openai.com/v1/chat/completions", headers, HTTPClient::METHOD_POST, JSON::stringify(data)) == OK;
}
