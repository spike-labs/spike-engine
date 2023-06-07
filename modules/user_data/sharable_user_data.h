/**
 * sharable_user_data.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/io/http_client.h"
#include "core/io/packet_peer.h"
#include "core/object/ref_counted.h"

class SharableUserData : public RefCounted {
	GDCLASS(SharableUserData, RefCounted)

	String _user;
	String _app;
	PackedStringArray _fields;

public:
	void _authorize(String p_appid, PackedStringArray p_fields);
	bool _post_data(Dictionary p_data, Variant p_sn = Variant());
	bool _fetch_data(Array p_keys, Variant p_sn = Variant());
	bool _post_assets(Dictionary p_assets, Callable p_callback);
	bool _sign_assets(Array hashes, Callable p_callback);

	static void _initialize(String p_appid, String p_secret_key);
	static void _bind_user(Ref<RefCounted> p_user);
	static bool _get_app_info(String p_appid, Callable p_callback);
	static Ref<RefCounted> _get_bound_user();

protected:
	static void _bind_methods();

public:
	String _rd_token;
	static String _wr_token;

	String get_user();
	String get_app();
	PackedStringArray get_fields();
};
