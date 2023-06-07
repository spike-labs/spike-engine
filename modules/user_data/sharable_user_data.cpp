/**
 * sharable_user_data.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#include "sharable_user_data.h"

#include "user_data_handler.h"

String SharableUserData::_wr_token;

void SharableUserData::_authorize(String p_appid, PackedStringArray p_fields) {
	ERR_FAIL_COND_MSG(p_appid.is_empty(), "App not initialized, call SharableUserData.initialize before authorize");

	auto self = Ref(this);
	_app = p_appid;
	_fields = p_fields;
	UserDataHandler::get_singleton()->authorize(self);
}

bool SharableUserData::_post_data(Dictionary p_data, Variant p_sn) {
	return UserDataHandler::get_singleton()->post_data(this, p_data, p_sn);
}

bool SharableUserData::_fetch_data(Array p_keys, Variant p_sn) {
	return UserDataHandler::get_singleton()->fetch_data(this, p_keys, p_sn);
}

bool SharableUserData::_post_assets(Dictionary p_assets, Callable p_callback) {
	return UserDataHandler::get_singleton()->post_assets(p_assets, p_callback);
}

bool SharableUserData::_sign_assets(Array hashes, Callable p_callback) {
	return UserDataHandler::get_singleton()->sign_assets(hashes, p_callback);
}

void SharableUserData::_initialize(String p_appid, String p_secret_key) {
	UserDataHandler::get_singleton()->appid = p_appid;
	UserDataHandler::get_singleton()->secret_key = p_secret_key;
}

void SharableUserData::_bind_user(Ref<RefCounted> p_user) {
	UserDataHandler::get_singleton()->userid = String();
	UserDataHandler::get_singleton()->current_user = p_user;
}

bool SharableUserData::_get_app_info(String p_appid, Callable p_callback) {
	SharableUserData *userdata = memnew(SharableUserData);
	userdata->_app = p_appid;
	return UserDataHandler::get_singleton()->get_app_info(userdata, p_callback);
}

Ref<RefCounted> SharableUserData::_get_bound_user() {
	return UserDataHandler::get_singleton()->current_user;
}

void SharableUserData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("authorize", "appid", "fields"), &SharableUserData::_authorize);
	ClassDB::bind_method(D_METHOD("post_data", "data", "sn"), &SharableUserData::_post_data, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("fetch_data", "keys", "sn"), &SharableUserData::_fetch_data, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("post_assets", "assets", "callback"), &SharableUserData::_post_assets);
	ClassDB::bind_method(D_METHOD("sign_assets", "hashes", "callback"), &SharableUserData::_sign_assets);
	ClassDB::bind_method(D_METHOD("get_user"), &SharableUserData::get_user);
	ClassDB::bind_method(D_METHOD("get_app"), &SharableUserData::get_app);

	ClassDB::bind_static_method(get_class_static(), D_METHOD("initialize", "appid", "secret_key"), &SharableUserData::_initialize);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("bind_user", "user"), &SharableUserData::_bind_user);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("get_app_info", "appid", "callback"), &SharableUserData::_get_app_info);
	ClassDB::bind_static_method(get_class_static(), D_METHOD("get_bound_user"), &SharableUserData::_get_bound_user);


	ADD_SIGNAL(MethodInfo("post", PropertyInfo(Variant::STRING, "error"), PropertyInfo(Variant::DICTIONARY, "data"), PropertyInfo(Variant::STRING, "sn")));
	ADD_SIGNAL(MethodInfo("fetch", PropertyInfo(Variant::STRING, "error"), PropertyInfo(Variant::DICTIONARY, "data"), PropertyInfo(Variant::STRING, "sn")));
}

String SharableUserData::get_user() {
	if (_user.is_empty()) {
		_user = UserDataHandler::get_singleton()->userid;
		if (_user == "0") {
			_user = String();
		}
	}
	return _user;
}

String SharableUserData::get_app() {
	return _app;
}

PackedStringArray SharableUserData::get_fields() {
	return _fields;
}
