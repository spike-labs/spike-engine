/**
 * account_manage.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "scene/gui/dialogs.h"

class AccountManage : public AcceptDialog {
	GDCLASS(AccountManage, AcceptDialog)

	String password;
	Ref<RefCounted> account;

	void set_used_wallet(const Ref<RefCounted> &p_wallet);
protected:

	bool _password_exists();
	bool _password_verified();
	void _logout();
	void _create_password(const String &p_pass);
	bool _verify_password(const String &p_pass);
	PackedStringArray _load_account_data();
	void _save_account_data(const PackedStringArray &p_array);

	static void _bind_methods();

public:
	AccountManage();
	void popup_web3_account();
	Ref<RefCounted> get_user() { return account; }
};