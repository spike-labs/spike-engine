/**
 * user_data_handler.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#include "user_data_handler.h"

#include "core/io/json.h"
#include "scene/main/window.h"
#if TOOLS_ENABLED
#include "editor/editor_paths.h"
#endif

#define SIGN_VALID_MS 600000

#define TOKEN_DIR "user://userdata_tokens"
#define TOKEN_FILE(t, app, user) vformat(TOKEN_DIR "/" #t "_%s_%s", app, user)
#if TOOLS_ENABLED
#define DEV_TOKEN_FILE(user) Engine::get_singleton()->is_editor_hint() ? vformat("%s/userdata_tokens/dev_0_%s", EditorPaths::get_singleton()->get_cache_dir(), user) : TOKEN_FILE(dev, "0", user)
#else
#define DEV_TOKEN_FILE(user) TOKEN_FILE(dev, "0", user)
#endif
#define GET_ASSET_TOKEN(token)                  \
	String token = SharableUserData::_wr_token; \
	if (IS_EMPTY(token)) {                      \
		token = _dev_token;                     \
	}

#define INSERT_AUTH_MSG(ud)                \
	do {                                   \
		Context msg_ctx;                   \
		msg_ctx.userdata = ud;             \
		msg_ctx.api = AUTH_MSG;            \
		_context_array.insert(0, msg_ctx); \
	} while (0)

#define UDERR_NONE Error(-2)
#define UDERR_DISCARD Error(-1)

#define ON_ASSETS_POST(e, d)                  \
	if (_upload_callback.is_valid()) {        \
		_upload_callback.call_deferred(e, d); \
		_upload_callback = Callable();        \
	}

String UserDataHandler::_dev_token;
UserDataHandler *UserDataHandler::_singleton = nullptr;

void UserDataHandler::_checking_user_id() {
	if (userid.is_empty()) {
		if (current_user.is_valid()) {
			userid = "0";
			auto var = current_user->call("get_id");
			switch (var.get_type()) {
				case Variant::OBJECT: {
					Object *obj = var;
					if (obj->has_signal("completed")) {
						obj->connect("completed", callable_mp(this, &UserDataHandler::_on_get_user_id), Object::CONNECT_ONE_SHOT);
					}
				} break;
				case Variant::INT:
				case Variant::STRING:
					_on_get_user_id(var);
					break;
				default:
					break;
			}
		}
	}
}

/**
 * @brief
 *
 * @param context The protocol context
 * @return Error (-2: do nothing, -1: discard request, 0: success, >0: failed)
 */
Error UserDataHandler::_handle_private_protocol(Context &p_context) {
	if (userid == "0")
		return UDERR_NONE;

	Error ret = UDERR_DISCARD;
	switch (p_context.api) {
		case AUTH_MSG: {
			PackedStringArray postData;
			postData.push_back("address=" + p_context.userdata->get_user().uri_encode());
			ret = _request->request(vformat("%s/api/auth/create/message?%s", base_url, String("&").join(postData)), PackedStringArray(), HTTPClient::METHOD_POST);
		} break;
		case API_SIGN: {
			auto kit = SceneTree::get_singleton()->get_root()->get_node_or_null(NodePath("SpikeBlockChainKit"));
			if (kit == nullptr) {
				ERR_PRINT("Fail to authorize for SharableUserData: SpikeBlockChainKit NOT FOUND. Ensure to import the block chain sdk 'com.spike.block_chain_sdk'");
			} else {
				auto var = current_user->call("sign_message", p_context.parameter).operator Object *();
				if (var && var->has_signal("completed")) {
					var->connect("completed", callable_mp(this, &UserDataHandler::_chain_sign_completed).bind(p_context.userdata), Object::CONNECT_ONE_SHOT);
					ret = OK;
				} else {
					ELog("UserData: cannot sign message: %s", current_user);
				}
			}
		} break;
		case WRITER_LOGIN: {
			auto fa = FileAccess::open(TOKEN_FILE(wr, appid, p_context.userdata->get_user()), FileAccess::READ);
			if (fa.is_valid()) {
				SharableUserData::_wr_token = fa->get_as_text();
				DLog("UserData: use cached write token for app#%s.", appid);
				break;
			} else if (sign_time + SIGN_VALID_MS < OS::get_singleton()->get_ticks_msec()) {
				INSERT_AUTH_MSG(p_context.userdata);
				return UDERR_NONE;
			}
			DLog("UserData: request write token for app#%s.", appid);
			PackedStringArray postData;
			postData.push_back("address=" + p_context.userdata->get_user().uri_encode());
			postData.push_back("appId=" + appid.uri_encode());
			postData.push_back("sign=" + sign_hash.uri_encode());
			postData.push_back("sk=" + secret_key.uri_encode());
			ret = _request->request(vformat("%s/api/auth/writer/login?%s", base_url, String("&").join(postData)), PackedStringArray(), HTTPClient::METHOD_POST);
		} break;
		case READER_LOGIN: {
			auto fa = FileAccess::open(TOKEN_FILE(rd, p_context.userdata->get_app(), p_context.userdata->get_user()), FileAccess::READ);
			if (fa.is_valid()) {
				p_context.userdata->_rd_token = fa->get_as_text();
				DLog("UserData: use cached read token for app#%s.", appid);
				break;
			} else if (sign_time + SIGN_VALID_MS < OS::get_singleton()->get_ticks_msec()) {
				INSERT_AUTH_MSG(p_context.userdata);
				return UDERR_NONE;
			}
			DLog("UserData: request read token for app#%s.", appid);
			PackedStringArray postData;
			postData.push_back("address=" + p_context.userdata->get_user().uri_encode());
			postData.push_back("appId=" + p_context.userdata->get_app().uri_encode());
			postData.push_back("sign=" + sign_hash.uri_encode());
			postData.push_back("fields=" + String(",").join(p_context.userdata->get_fields()).uri_encode());
			postData.push_back("type=1");
			ret = _request->request(vformat("%s/api/auth/queryer/login?%s", base_url, String("&").join(postData)), PackedStringArray(), HTTPClient::METHOD_POST);
		} break;
		case WRITER_SET: {
			PackedStringArray headers;
			headers.push_back("Authorization: Bearer " + SharableUserData::_wr_token);
			PackedStringArray postData;
			postData.push_back("json=" + JSON::stringify(p_context.parameter).uri_encode());
			ret = _request->request(vformat("%s/api/writer/user/set?%s", base_url, String("&").join(postData)), headers, HTTPClient::METHOD_POST);
		} break;
		case READER_QUERY: {
			PackedStringArray headers;
			headers.push_back("Authorization: Bearer " + p_context.userdata->_rd_token);
			ret = _request->request(base_url.path_join("api/queryer/userset/info"), headers);
		} break;
		case MINT_TOKEN: {
			PackedStringArray headers;
			GET_ASSET_TOKEN(token);
			headers.push_back("Authorization: Bearer " + token);
			ret = _request->request(base_url.path_join("api/ipfs/mint/token"), headers);
		} break;
		case QUERY_COST: {
			PackedStringArray headers;
			GET_ASSET_TOKEN(token);
			headers.push_back("Authorization: Bearer " + token);
			String urls = String(",").join(p_context.parameter).uri_encode();
			ret = _request->request(vformat("%s/api/ipfs/query/cost?hashs=%s", base_url, urls), headers, HTTPClient::METHOD_GET);
		} break;
		case DEVELOPER_LOGIN: {
			String token_path = DEV_TOKEN_FILE(p_context.userdata->get_user());
			auto fa = FileAccess::open(token_path, FileAccess::READ);
			if (fa.is_valid()) {
				_dev_token = fa->get_as_text();
				DLog("UserData: use cached develop token of user#%s.", p_context.userdata->get_user());
				break;
				// TODO: with block chain sign
				// } else if (sign_time + SIGN_VALID_MS < OS::get_singleton()->get_ticks_msec()) {
				// 	INSERT_AUTH_MSG(p_context.userdata);
				// 	return UDERR_NONE;
			}
			DLog("UserData: request develop token of user#%s.", p_context.userdata->get_user());
			PackedStringArray postData;
			postData.push_back("address=" + p_context.userdata->get_user().uri_encode());
			// postData.push_back("appId=" + appid.uri_encode());
			// postData.push_back("sign=" + sign_hash.uri_encode());
			// postData.push_back("sk=" + secret_key.uri_encode());
			ret = _request->request(vformat("%s/api/auth/developer/login?%s", base_url, String("&").join(postData)), PackedStringArray(), HTTPClient::METHOD_POST);
		} break;
		default:
			ret = UDERR_NONE;
			break;
	}
	return ret;
}

/**
 * @brief
 *
 * @param context The protocol context
 * @return Error (-2: do nothing, -1: discard request, 0: success, >0: failed)
 */
Error UserDataHandler::_handle_public_protocol(Context &p_context) {
	Error ret = UDERR_DISCARD;
	switch (p_context.api) {
		case GET_APPNAME: {
			ret = _request->request(vformat("%s/api/app/detail?appId=%s", base_url, p_context.userdata->get_app()));
		} break;
		default:
			ret = UDERR_NONE;
			break;
	}
	return ret;
}

void UserDataHandler::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			set_process(true);
			break;
		case NOTIFICATION_PROCESS: {
			_checking_user_id();

			if (_api == API_NONE && _context_array.size()) {
				Context context = _context_array.get(0);
				Error ret = _handle_public_protocol(context);

				if (ret == UDERR_NONE && !IS_EMPTY(userid) && userid != "0") {
					ret = _handle_private_protocol(context);
				}

				if (ret == OK) {
					_api = DataAPI(context.api);
				} else if (ret > UDERR_NONE) {
					_context_array.remove_at(0);
					if (ret > OK) {
						ERR_PRINT("Request failed: " + itos(ret));
					}
				}
			}
		} break;
		default:
			break;
	}
}

void UserDataHandler::_on_get_user_id(const String &p_userid) {
	userid = p_userid.to_lower();
}

void UserDataHandler::_request_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	Context context = _context_array.get(0);
	_context_array.remove_at(0);
	_api = API_NONE;

	int code = -1;
	String error = String();
	if (p_result != OK) {
		error = vformat("Request NOT OK(#%d)", p_result);
	} else if (p_code < 200 || p_code > 299) {
		error = vformat("HTTP NOT OK(#%d)", p_code);
	}

	auto userdata = context.userdata;
	if (IS_EMPTY(error)) {
		String response_json;
		const uint8_t *r = p_body.ptr();
		response_json.parse_utf8((const char *)r, p_body.size());
		DLog("UserData <= %s\n", response_json);
		Dictionary dic = JSON::parse_string(response_json);
		code = dic["code"];
		if (code == 1) {
			switch (context.api) {
				case AUTH_MSG: {
					String msg = dic["data"];

					Context context;
					context.api = API_SIGN;
					context.userdata = userdata;
					context.parameter = msg;
					_context_array.insert(0, context);
				} break;
				case WRITER_LOGIN: {
					Dictionary data = dic["data"];
					SharableUserData::_wr_token = data["tokenKey"];

					auto da = DirAccess::create(DirAccess::ACCESS_USERDATA);
					if (!da->exists(TOKEN_DIR)) {
						da->make_dir(TOKEN_DIR);
					}
					auto fa = FileAccess::open(TOKEN_FILE(wr, appid, userdata->get_user()), FileAccess::WRITE);
					fa->store_string(SharableUserData::_wr_token);
				} break;
				case READER_LOGIN: {
					userdata->_rd_token = dic["data"];

					auto da = DirAccess::create(DirAccess::ACCESS_USERDATA);
					if (!da->exists(TOKEN_DIR)) {
						da->make_dir(TOKEN_DIR);
					}

					auto fa = FileAccess::open(TOKEN_FILE(rd, userdata->get_app(), userdata->get_user()), FileAccess::WRITE);
					fa->store_string(userdata->_rd_token);
				} break;
				case WRITER_SET: {
					userdata->emit_signal("post", error, context.parameter, context.sn);
				} break;
				case READER_QUERY: {
					Dictionary data = dic["data"];
					Dictionary payload = data["payload"];
					Dictionary fields_data;
					Array keys = payload.keys();
					PackedStringArray fields = context.parameter;
					for (int i = 0; i < keys.size(); ++i) {
						String key = keys[i];
						if (fields.size() == 0 || fields.has(key)) {
							fields_data[key] = payload[key];
						}
					}
					userdata->emit_signal("fetch", error, fields_data, context.sn);
				} break;
				case GET_APPNAME: {
					Callable callback = context.parameter;
					Array args;
					args.push_back(dic["data"]);
					callback.callv(args);
				} break;
				case MINT_TOKEN: {
					String token = dic["data"];
					_pin_to_ipfs(context.parameter, token);
				} break;
				case QUERY_COST: {
					// {"code":1,"msg":"","success":true,"data":{"id":null,"amounnt":0.002000,"sign":"0x8932b0ebd4b8f1c04cc0393f36c7a851404ad9a13ab4fa7e532506ad82337e052f15fa178b21402b32abb1a5a4b5e42a23824907a06587e47ac3dff590c9edc51c","message":"2000"}
					// Dictionary data = dic["data"];
					Callable callback = context.sn;
					if (callback.is_valid()) {
						if (dic["code"].operator signed int() == 1) {
							Array args;
							args.push_back(String());
							args.push_back(dic["data"]);
							callback.callv(args);
						} else {
							Array args;
							args.push_back(dic["msg"]);
							args.push_back(Dictionary());
							callback.callv(args);
						}
					}
				} break;
				case DEVELOPER_LOGIN: {
					Dictionary data = dic["data"];
					_dev_token = data["tokenKey"];

					String token_path = DEV_TOKEN_FILE(userdata->get_user());
					String dir_path = token_path.get_base_dir();
					Ref<DirAccess> d = DirAccess::create_for_path(dir_path);
					if (!d->exists(dir_path)) {
						d->make_dir(dir_path);
					}
					auto fa = FileAccess::open(token_path, FileAccess::WRITE);
					fa->store_string(_dev_token);
					Callable callback = context.parameter;
					if (callback.is_valid()) {
						callback.callv(Array());
					}
				} break;
				default:
					break;
			}
		} else {
			error = dic["msg"];
		}
	}

	if (!IS_EMPTY(error)) {
		switch (context.api) {
			case WRITER_SET:
				userdata->emit_signal("post", error, context.parameter, context.sn);
				break;
			case READER_QUERY:
				userdata->emit_signal("fetch", error, Variant(), context.sn);
				break;
			case MINT_TOKEN: {
				ON_ASSETS_POST(error, Dictionary());
			} break;
			case QUERY_COST: {
				Callable callback = context.sn;
				if (callback.is_valid()) {
					Array args;
					args.push_back(error);
					args.push_back(Dictionary());
					callback.callv(args);
				}
			} break;

			default:
				break;
		}

		ELog("UserData request failed:%s", error);
	}
}

void UserDataHandler::_upload_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	String error = String();
	Dictionary d;

	if (p_result != OK) {
		error = vformat("Request NOT OK(#%d)", p_result);
	} else {
		String response_json;
		const uint8_t *r = p_body.ptr();
		response_json.parse_utf8((const char *)r, p_body.size());
		DLog("Upload <= %s\n", response_json);

		if (p_code < 200 || p_code > 299) {
			error = vformat("HTTP NOT OK(#%d)", p_code);
		}
		Ref<JSON> json;
		json.instantiate();
		auto ret = json->parse(response_json);
		if (ret == OK) {
			d = json->get_data();
			Variant *varptr = d.getptr("IpfsHash");
			if (varptr) {
				// PackedStringArray urls;
				// String ipfs_hash = *varptr;
				// for (int i = 0; i < _upload_files.size(); ++i) {
				// 	urls.push_back(vformat("%s/%s", ipfs_hash, _upload_files[i]));
				// }
				// urls.push_back(ipfs_hash);
				// Context context;
				// context.parameter = urls;
				// context.api = QUERY_COST;
				// _context_array.push_back(context);
				// _upload_files.clear();
			} else if (Variant *errptr = d.getptr("error")) {
				error = *errptr;
			}
		} else {
			error = itos(ret);
		}
	}

	ON_ASSETS_POST(error, d);
}

void UserDataHandler::_chain_sign_completed(const String &p_sign, const Ref<SharableUserData> &p_userdata) {
	_context_array.remove_at(0);
	_api = API_NONE;

	DLog("Chain Sign Completed: %s", p_sign);
	sign_hash = p_sign;
	sign_time = OS::get_singleton()->get_ticks_msec();
}

void UserDataHandler::_pin_to_ipfs(const Dictionary &p_assets, const String &p_token) {
	String boundary = "SpikeFormBoundaryePkpFF7tjGqx29L";
	PackedStringArray headers;
	headers.push_back("Authorization: Bearer " + p_token);
	headers.push_back("Content-Type: multipart/form-data;boundary=\"" + boundary + "\"");
	PackedByteArray body;
	PackedByteArray boundary_bytes = ("\r\n--" + boundary).to_utf8_buffer();
	PackedByteArray content_type_bytes = String("\r\nContent-Type: multipart/form-data\r\n\r\n").to_utf8_buffer();
	int asset_count = p_assets.size();
	for (const Variant *key = p_assets.next(nullptr); key; key = p_assets.next(key)) {
		String file_name = *key;
		PackedByteArray file_content = p_assets[file_name];
		PackedByteArray disposition_bytes;
		if (asset_count > 1) {
			disposition_bytes = ("\r\nContent-Disposition: form-data; name=\"file\"; filename=\"UserAssets/" + file_name + "\"").to_utf8_buffer();
		} else {
			disposition_bytes = ("\r\nContent-Disposition: form-data; name=\"file\"; filename=\"" + file_name + "\"").to_utf8_buffer();
		}
		body.append_array(boundary_bytes);
		body.append_array(disposition_bytes);
		body.append_array(content_type_bytes);
		body.append_array(file_content);
	}
	body.append_array(boundary_bytes);
	body.append_array(String("--\r\n").to_utf8_buffer());

	_uploader->request_raw("https://api.pinata.cloud/pinning/pinFileToIPFS", headers, HTTPClient::METHOD_POST, body);
}

UserDataHandler *UserDataHandler::get_singleton() {
	if (_singleton == nullptr) {
		SceneTree::get_singleton()->get_root()->call_deferred("add_child", (memnew(UserDataHandler)));
	}
	return _singleton;
}

UserDataHandler::UserDataHandler() {
	if (_singleton) {
		queue_free();
	}

	_singleton = this;
	base_url = "https://devspike.oasis.world/spike";
	_api = API_NONE;
	sign_time = -SIGN_VALID_MS;

	_request = memnew(HTTPRequest);
	add_child(_request);
	_request->connect("request_completed", callable_mp(this, &UserDataHandler::_request_completed));

	_uploader = memnew(HTTPRequest);
	add_child(_uploader);
	_uploader->connect("request_completed", callable_mp(this, &UserDataHandler::_upload_completed));
}

void UserDataHandler::authorize(Ref<SharableUserData> &p_userdata) {
	ERR_FAIL_COND_MSG(current_user.is_null(), "Current User is invalid. Must provide a valid User by method `SharableUserData.bind_user`");

	if (appid == p_userdata->get_app()) {
		auto _context = Context();
		_context.userdata = p_userdata;
		_context.api = WRITER_LOGIN;
		_context_array.push_back(_context);
	}

	auto _context = Context();
	_context.userdata = p_userdata;
	_context.api = READER_LOGIN;
	_context_array.push_back(_context);
}

bool UserDataHandler::post_data(const Ref<SharableUserData> &p_userdata, const Dictionary &p_data, const Variant p_sn) {
	ERR_FAIL_COND_V(current_user.is_null(), false);

	auto context = Context();
	context.userdata = p_userdata;
	context.parameter = p_data;
	context.sn = p_sn;
	context.api = WRITER_SET;
	_context_array.push_back(context);
	return true;
}

bool UserDataHandler::fetch_data(const Ref<SharableUserData> &p_userdata, const Array &p_keys, const Variant p_sn) {
	ERR_FAIL_COND_V(current_user.is_null(), false);

	auto context = Context();
	context.userdata = p_userdata;
	context.parameter = p_keys;
	context.sn = p_sn;
	context.api = READER_QUERY;
	_context_array.push_back(context);
	return true;
}

bool UserDataHandler::get_app_info(const Ref<SharableUserData> &p_userdata, const Callable &p_callback) {
	auto context = Context();
	context.userdata = p_userdata;
	context.parameter = p_callback;
	context.api = GET_APPNAME;
	_context_array.push_back(context);
	return true;
}

bool UserDataHandler::post_assets(const Dictionary &p_assets, const Callable &p_callback) {
	ERR_FAIL_COND_V(IS_EMPTY(userid) || userid == "0", false);

	if (_upload_callback.is_valid()) {
		return false;
	}
	_upload_callback = p_callback;

	auto context = Context();
	context.parameter = p_assets;
	//context.sn = p_callback;
	context.api = MINT_TOKEN;
	_context_array.push_back(context);
	return true;
}

bool UserDataHandler::sign_assets(Array hashes, Callable p_callback) {
	ERR_FAIL_COND_V(IS_EMPTY(userid) || userid == "0", false);

	Context context;
	context.parameter = hashes;
	context.sn = p_callback;
	context.api = QUERY_COST;
	_context_array.push_back(context);
	return true;
}

void UserDataHandler::developer_authorize(const Ref<SharableUserData> &p_userdata, const Callable &p_callback) {
	ERR_FAIL_COND_MSG(current_user.is_null(), "Current User is invalid. Must provide a valid User by method `SharableUserData.bind_user`");

	auto _context = Context();
	_context.userdata = p_userdata;
	_context.parameter = p_callback;
	_context.sn = p_callback;
	_context.api = DEVELOPER_LOGIN;
	_context_array.push_back(_context);
}
