/**
 * editor_package_dialog.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#include "editor_package_dialog.h"

#include "editor_package_system.h"
#include "spike/editor/spike_editor_node.h"

void EditorPackageDialog::ok_pressed() {
	hide();
	EditorPackageSystem::get_singleton()->update_packages();
}

void EditorPackageDialog::custom_action(const String &p_action) {
	hide();

	if (p_action == "_retry") {
		auto package_name = get_meta("package");
		EditorPackageSystem::get_singleton()->update_packages(package_name);
	} else if (p_action == "_stop") {
		SpikeEditorNode::get_instance()->exit_editor(EXIT_SUCCESS);
	}
}

void EditorPackageDialog::_package_has_loaded(const String &p_package, const String &p_error) {
	if (!IS_EMPTY(p_error)) {
		package_name = p_package;
		set_text(p_error);
		popup_centered();
	}
}

void EditorPackageDialog::_package_unload_error(const String &p_info) {
	SpikeEditorNode::get_instance()->show_custom_warning(
			callable_mp(EditorNode::get_singleton(), &SpikeEditorNode::restart_editor), TTR("Restart"), p_info);
}

EditorPackageDialog::EditorPackageDialog() {
	set_title(STTR("Spike package System"));
	set_ok_button_text(TTR("Ignore"));
	set_close_on_escape(false);
	add_button(TTR("Retry"), true, "_retry");
	add_button(TTR("Stop"), true, "_stop");
}
