/**
 * account_manage.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "account_manage.h"

#include "core/io/file_access_encrypted.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_scale.h"
#include "modules/modules_enabled.gen.h"
#include "scene/gui/box_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/separator.h"
#include "spike_define.h"
#ifdef MODULE_USER_DATA_ENABLED
#include "user_data/user_data_handler.h"
#endif

#define WEB3_PASS_SAVE_PATH PATH_JOIN(EditorPaths::get_singleton()->get_cache_dir(), "spike_wallet_9527.pass")
#define WEB3_WALLET_SAVE_PATH PATH_JOIN(EditorPaths::get_singleton()->get_cache_dir(), "spike_wallet_9527.data")

void AccountManage::set_used_wallet(const Ref<RefCounted> &p_wallet) {
	account = p_wallet;

#ifdef MODULE_USER_DATA_ENABLED
	SharableUserData::_bind_user(account);
	if (account.is_valid()) {
		SharableUserData *user_data = memnew(SharableUserData);
		UserDataHandler::get_singleton()->developer_authorize(user_data, Callable());
	}
#endif
}

bool AccountManage::_password_exists() {
	return FileAccess::exists(WEB3_PASS_SAVE_PATH);
}

bool AccountManage::_password_verified() {
	return !password.is_empty();
}

void AccountManage::_logout() {
	password = String();
	set_used_wallet(nullptr);
}

void AccountManage::_create_password(const String &p_pass) {
	auto fa = FileAccess::open(WEB3_PASS_SAVE_PATH, FileAccess::WRITE);
	password = p_pass.md5_text();
	fa->store_string(password);
}

bool AccountManage::_verify_password(const String &p_pass) {
	String md5_pass = p_pass.md5_text();
	Ref<FileAccess> f = FileAccess::open(WEB3_PASS_SAVE_PATH, FileAccess::READ);
	if (f.is_valid() && f->get_as_text() == md5_pass) {
		password = md5_pass;
		return true;
	}
	return false;
}

PackedStringArray AccountManage::_load_account_data() {
	PackedStringArray array;
	Ref<FileAccess> f = FileAccess::open(WEB3_WALLET_SAVE_PATH, FileAccess::READ);
	if (f.is_valid()) {
		Vector<uint8_t> key_md5;
		key_md5.resize(32);
		for (int i = 0; i < 32; i++) {
			key_md5.write[i] = password[i];
		}
		Ref<FileAccessEncrypted> fae;
		fae.instantiate();
		if (fae->open_and_parse(f, key_md5, FileAccessEncrypted::MODE_READ) == OK) {
			for (;;) {
				String data = fae->get_line();
				if (data.length() == 0)
					break;
				array.push_back(data);
			}
		}
	}
	return array;
}

void AccountManage::_save_account_data(const PackedStringArray &p_array) {
	Ref<FileAccess> f = FileAccess::open(WEB3_WALLET_SAVE_PATH, FileAccess::WRITE);
	if (f.is_valid()) {
		Vector<uint8_t> key_md5;
		key_md5.resize(32);
		for (int i = 0; i < 32; i++) {
			key_md5.write[i] = password[i];
		}
		Ref<FileAccessEncrypted> fae;
		fae.instantiate();
		fae->open_and_parse(f, key_md5, FileAccessEncrypted::MODE_WRITE_AES256);
		if (fae.is_valid()) {
			for (auto &data : p_array) {
				fae->store_line(data);
			}
		}
	}
}

void AccountManage::_bind_methods() {
	ClassDB::bind_method(D_METHOD("password_exists"), &AccountManage::_password_exists);
	ClassDB::bind_method(D_METHOD("password_verified"), &AccountManage::_password_verified);
	ClassDB::bind_method(D_METHOD("logout"), &AccountManage::_logout);
	ClassDB::bind_method(D_METHOD("create_password", "password"), &AccountManage::_create_password);
	ClassDB::bind_method(D_METHOD("verify_password", "password"), &AccountManage::_verify_password);
	ClassDB::bind_method(D_METHOD("load_account_data"), &AccountManage::_load_account_data);
	ClassDB::bind_method(D_METHOD("save_account_data", "data"), &AccountManage::_save_account_data);
	ClassDB::bind_method(D_METHOD("using_account", "account"), &AccountManage::set_used_wallet);
	ClassDB::bind_method(D_METHOD("get_account"), &AccountManage::get_user);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "account"), "using_account", "get_account");
}

AccountManage::AccountManage() {
	set_title(STTR("Enter Decentralized Network..."));
	get_ok_button()->set_visible(false);
}

void AccountManage::popup_web3_account() {
	popup_centered(Vector2i(420, 0) * EDSCALE);
}
