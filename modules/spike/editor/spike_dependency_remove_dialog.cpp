/**
 * spike_dependency_remove_dialog.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "spike_dependency_remove_dialog.h"

#include "core/config/project_settings.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"

void SpikeDependencyRemoveDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("dirs_and_files_before_remove", PropertyInfo(Variant::PACKED_STRING_ARRAY, "files_to_delete"),
			PropertyInfo(Variant::PACKED_STRING_ARRAY, "dirs_to_delete")));
}

void SpikeDependencyRemoveDialog::ok_pressed() {
	SpikeDependencyRemoveDialog::emit_signal("dirs_and_files_before_remove", files_to_delete.duplicate(), dirs_to_delete.duplicate());

	for (int i = 0; i < files_to_delete.size(); ++i) {
		if (ResourceCache::has(files_to_delete[i])) {
			Ref<Resource> res = ResourceCache::get_ref(files_to_delete[i]);
			res->set_path("");
		}

		// If the file we are deleting for e.g. the main scene, default environment,
		// or audio bus layout, we must clear its definition in Project Settings.
		if (files_to_delete[i] == String(ProjectSettings::get_singleton()->get("application/config/icon"))) {
			ProjectSettings::get_singleton()->set("application/config/icon", "");
		}
		if (files_to_delete[i] == String(ProjectSettings::get_singleton()->get("application/run/main_scene"))) {
			ProjectSettings::get_singleton()->set("application/run/main_scene", "");
		}
		if (files_to_delete[i] == String(ProjectSettings::get_singleton()->get("application/boot_splash/image"))) {
			ProjectSettings::get_singleton()->set("application/boot_splash/image", "");
		}
		if (files_to_delete[i] == String(ProjectSettings::get_singleton()->get("rendering/environment/defaults/default_environment"))) {
			ProjectSettings::get_singleton()->set("rendering/environment/defaults/default_environment", "");
		}
		if (files_to_delete[i] == String(ProjectSettings::get_singleton()->get("display/mouse_cursor/custom_image"))) {
			ProjectSettings::get_singleton()->set("display/mouse_cursor/custom_image", "");
		}
		if (files_to_delete[i] == String(ProjectSettings::get_singleton()->get("gui/theme/custom"))) {
			ProjectSettings::get_singleton()->set("gui/theme/custom", "");
		}
		if (files_to_delete[i] == String(ProjectSettings::get_singleton()->get("gui/theme/custom_font"))) {
			ProjectSettings::get_singleton()->set("gui/theme/custom_font", "");
		}
		if (files_to_delete[i] == String(ProjectSettings::get_singleton()->get("audio/buses/default_bus_layout"))) {
			ProjectSettings::get_singleton()->set("audio/buses/default_bus_layout", "");
		}

		String path = OS::get_singleton()->get_resource_dir() + files_to_delete[i].replace_first("res://", "/");
		print_verbose("Moving to trash: " + path);
		Error err = OS::get_singleton()->move_to_trash(path);
		if (err != OK) {
			EditorNode::get_singleton()->add_io_error(TTR("Cannot remove:") + "\n" + files_to_delete[i] + "\n");
		} else {
			DependencyRemoveDialog::emit_signal("file_removed", files_to_delete[i]);
		}
	}

	if (dirs_to_delete.size() == 0) {
		// If we only deleted files we should only need to tell the file system about the files we touched.
		for (int i = 0; i < files_to_delete.size(); ++i) {
			EditorFileSystem::get_singleton()->update_file(files_to_delete[i]);
		}
	} else {
		for (int i = 0; i < dirs_to_delete.size(); ++i) {
			String path = OS::get_singleton()->get_resource_dir() + dirs_to_delete[i].replace_first("res://", "/");
			print_verbose("Moving to trash: " + path);
			Error err = OS::get_singleton()->move_to_trash(path);
			if (err != OK) {
				EditorNode::get_singleton()->add_io_error(TTR("Cannot remove:") + "\n" + dirs_to_delete[i] + "\n");
			} else {
				DependencyRemoveDialog::emit_signal("folder_removed", dirs_to_delete[i]);
			}
		}

		EditorFileSystem::get_singleton()->scan_changes();
	}

	// If some files/dirs would be deleted, favorite dirs need to be updated
	Vector<String> previous_favorites = EditorSettings::get_singleton()->get_favorites();
	Vector<String> new_favorites;

	for (int i = 0; i < previous_favorites.size(); ++i) {
		if (previous_favorites[i].ends_with("/")) {
			if (dirs_to_delete.find(previous_favorites[i]) < 0) {
				new_favorites.push_back(previous_favorites[i]);
			}
		} else {
			if (files_to_delete.find(previous_favorites[i]) < 0) {
				new_favorites.push_back(previous_favorites[i]);
			}
		}
	}

	if (new_favorites.size() < previous_favorites.size()) {
		EditorSettings::get_singleton()->set_favorites(new_favorites);
	}
}

void SpikeDependencyRemoveDialog::remove_file(const String &file) {
	PackedStringArray files, dirs;
	files.append(file);
	SpikeDependencyRemoveDialog::emit_signal("dirs_and_files_before_remove", files, dirs);

	if (ResourceCache::has(file)) {
		Ref<Resource> res = ResourceCache::get_ref(file);
		res->set_path("");
	}

	// If the file we are deleting for e.g. the main scene, default environment,
	// or audio bus layout, we must clear its definition in Project Settings.
	if (file == String(ProjectSettings::get_singleton()->get("application/config/icon"))) {
		ProjectSettings::get_singleton()->set("application/config/icon", "");
	}
	if (file == String(ProjectSettings::get_singleton()->get("application/run/main_scene"))) {
		ProjectSettings::get_singleton()->set("application/run/main_scene", "");
	}
	if (file == String(ProjectSettings::get_singleton()->get("application/boot_splash/image"))) {
		ProjectSettings::get_singleton()->set("application/boot_splash/image", "");
	}
	if (file == String(ProjectSettings::get_singleton()->get("rendering/environment/defaults/default_environment"))) {
		ProjectSettings::get_singleton()->set("rendering/environment/defaults/default_environment", "");
	}
	if (file == String(ProjectSettings::get_singleton()->get("display/mouse_cursor/custom_image"))) {
		ProjectSettings::get_singleton()->set("display/mouse_cursor/custom_image", "");
	}
	if (file == String(ProjectSettings::get_singleton()->get("gui/theme/custom"))) {
		ProjectSettings::get_singleton()->set("gui/theme/custom", "");
	}
	if (file == String(ProjectSettings::get_singleton()->get("gui/theme/custom_font"))) {
		ProjectSettings::get_singleton()->set("gui/theme/custom_font", "");
	}
	if (file == String(ProjectSettings::get_singleton()->get("audio/buses/default_bus_layout"))) {
		ProjectSettings::get_singleton()->set("audio/buses/default_bus_layout", "");
	}

	String path = OS::get_singleton()->get_resource_dir() + file.replace_first("res://", "/");
	print_verbose("Moving to trash: " + path);
	Error err = OS::get_singleton()->move_to_trash(path);
	if (err != OK) {
		EditorNode::get_singleton()->add_io_error(TTR("Cannot remove:") + "\n" + file + "\n");
	} else {
		DependencyRemoveDialog::emit_signal("file_removed", file);
	}

	EditorFileSystem::get_singleton()->update_file(file);

	// If some files/dirs would be deleted, favorite dirs need to be updated
	Vector<String> previous_favorites = EditorSettings::get_singleton()->get_favorites();
	Vector<String> new_favorites;

	for (int i = 0; i < previous_favorites.size(); ++i) {
		if (!previous_favorites[i].ends_with("/")) {
			if (file != previous_favorites[i]) {
				new_favorites.push_back(previous_favorites[i]);
			}
		}
	}

	if (new_favorites.size() < previous_favorites.size()) {
		EditorSettings::get_singleton()->set_favorites(new_favorites);
	}
}