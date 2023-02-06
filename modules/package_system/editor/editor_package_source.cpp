/**
 * editor_package_source.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "editor_package_source.h"

#include "core/io/dir_access.h"
#include "editor/editor_paths.h"

void EditorPackageSource::get_task_step(String &state, int &step) {
	state = repo_name;
	step = 1;
}

String EditorPackageSource::get_tag() {
	return String();
}

String EditorPackageSource::get_cache_dir() {
	return EditorPaths::get_singleton()->get_cache_dir().path_join("packages");
}

EditorPackageSource::~EditorPackageSource() {
}

EditorPackageSourceLocal::EditorPackageSourceLocal(const String &name, const String &source) :
		EditorPackageSource(name) {
	if (source.begins_with("file://")) {
		repo_url = source.replace("file://", "");
	}
}

String EditorPackageSourceLocal::get_tag() {
	return "local:" + param;
}

Error EditorPackageSourceLocal::fetch_source(const String &ws_path, String &err_info) {
	String custom_plugin_path = ws_path;
	auto proj_dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	Error err = proj_dir->copy_dir(this->repo_url, ws_path);
	if (err != OK) {
		err_info = vformat(STTR("An error(#%d) occurred while loading the package locally."), (int)err);
		if (proj_dir->exists(ws_path)) {
			OS::get_singleton()->move_to_trash(ws_path);
		}
	}
	return err;
}
