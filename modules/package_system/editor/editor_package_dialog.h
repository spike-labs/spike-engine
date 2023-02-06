/**
 * editor_package_dialog.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "scene/gui/dialogs.h"
#include "spike_define.h"

class EditorPackageDialog : public AcceptDialog {
	GDCLASS(EditorPackageDialog, AcceptDialog);
	friend class EditorPackageSystem;
	String package_name;

protected:
	virtual void ok_pressed() override;
	virtual void custom_action(const String &p_action) override;
	void _package_has_loaded(const String &p_package, const String &p_error);
	void _package_unload_error(const String &p_error_info);

public:
	EditorPackageDialog();
};