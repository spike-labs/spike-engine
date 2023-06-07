/**
 * user_data_handler.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "scene/main/http_request.h"
#include "sharable_user_data.h"
#include "spike_define.h"

class UserDataHandler : public Node {
	GDCLASS(UserDataHandler, Node)

	enum DataAPI {
		API_NONE,
		AUTH_MSG,
		API_SIGN,
		READER_LOGIN,
		WRITER_LOGIN,
		WRITER_SET,
		WRITER_UNSET,
		READER_QUERY,
		GET_APPNAME,
		MINT_TOKEN,
		QUERY_COST,
		DEVELOPER_LOGIN,
	};

	struct Context {
		Ref<SharableUserData> userdata;
		Variant parameter;
		Variant sn;
		int api;
	};

	static UserDataHandler *_singleton;

	HTTPRequest *_request;
	HTTPRequest *_uploader;

	String base_url;

	Vector<Context> _context_array;
	DataAPI _api;

	Callable _upload_callback;

	uint64_t sign_time;
	String sign_hash;

protected:
	static String _dev_token;

	void _checking_user_id();

	Error _handle_private_protocol(Context &p_context);

	Error _handle_public_protocol(Context &p_context);

	void _notification(int p_what);

	void _on_get_user_id(const String &p_userid);

	void _request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	void _upload_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	void _chain_sign_completed(const String &p_sign, const Ref<SharableUserData> &p_userdata);

	void _pin_to_ipfs(const Dictionary &p_assets, const String &p_token);

public:
	static UserDataHandler *get_singleton();

	UserDataHandler();

	String appid;
	String secret_key;
	String userid;

	Ref<RefCounted> current_user;

	void authorize(Ref<SharableUserData> &p_userdata);
	bool post_data(const Ref<SharableUserData> &p_userdata, const Dictionary &p_data, const Variant p_sn);
	bool fetch_data(const Ref<SharableUserData> &p_userdata, const Array &p_keys, const Variant p_sn);
	bool get_app_info(const Ref<SharableUserData> &p_userdata, const Callable &p_callback);
	bool post_assets(const Dictionary &p_assets, const Callable &p_callback);
	bool sign_assets(Array hashes, Callable p_callback);

	void developer_authorize(const Ref<SharableUserData> &p_userdata, const Callable &p_callback);
};
