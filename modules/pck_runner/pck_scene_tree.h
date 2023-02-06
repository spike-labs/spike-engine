/**
 * pck_scene_tree.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "core/templates/list.h"
#include "scene/main/scene_tree.h"
#include "spike_define.h"

class PCKSceneTree : public SceneTree {
	GDCLASS(PCKSceneTree, SceneTree);

protected:
	List<String> pck_stack;
	static void _bind_methods();

	static GDExtensionObjectPtr _create(void *p_user_data) {
		return memnew(PCKSceneTree);
	}

public:
	void push_pck(const String &p_scene_file);

	void pck_quit(int p_exit_code = EXIT_SUCCESS);

	static void make_current() {
		auto ti = ClassDB::classes.getptr(SceneTree::get_class_static());
		ti->gdextension = memnew(ObjectGDExtension);
		ti->gdextension->create_instance = &_create;
		METHID_BIND_REPLACE(SceneTree, quit, &PCKSceneTree::pck_quit);
	}
};
