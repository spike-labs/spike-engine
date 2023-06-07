/**
 * modular_package.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "modules/modules_enabled.gen.h"

#ifdef MODULE_MODULAR_SYSTEM_ENABLED
#include "modular_system/editor/modular_data.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/item_list.h"

class ModularPackage : public ModularData {
	GDCLASS(ModularPackage, ModularData);

protected:
	String id;
	String ver;
	int ext = 0;
	static void _bind_methods();

	void _select_package_id();
	void _version_button_pressed(int p_id, Button *p_button);

	void _select_package_undo_redo(const String &p_id, const String &p_ver);

public:
	virtual void validation() override;
	virtual String get_module_name() const override;
	virtual void edit_data(Object *p_context) override;
	virtual String export_data() override;
	virtual Dictionary get_module_info() override;
	virtual void inspect_modular(Node *p_parent) override;

	void set_id(String p_id);
	String get_id() const { return id; }
	void set_ver(String p_ver) { ver = p_ver; };
	String get_ver() const { return ver; }
	void set_ext(int p_ext) { ext = p_ext; }
	int get_ext() const { return ext; }
};

class ModularPackageSelection : public ConfirmationDialog {
	GDCLASS(ModularPackageSelection, ConfirmationDialog);

	Variant selected_package;

	ItemList *itemlist;

	void _package_item_selected(int p_index);

public:
	ModularPackageSelection();

	Variant get_selected();
	void popup_selection(const String &p_selection);
};
#endif
