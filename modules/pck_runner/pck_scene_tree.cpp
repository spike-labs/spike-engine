/**
 * pck_scene_tree.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "pck_scene_tree.h"
#include "core/config/project_settings.h"
#include "core/object/method_bind.h"
#include "pck_runner/pck_runner.h"
#include "scene/main/node.h"

void PCKSceneTree::_bind_methods() {}

void PCKSceneTree::push_pck(const String &p_scene_file) {
	pck_stack.push_back(p_scene_file);
}

void PCKSceneTree::pck_quit(int p_exit_code) {
	if (pck_stack.size() > 0) {
		auto to_run = pck_stack.back()->get();
		if (OK == change_scene_to_file(to_run)) {
			pck_stack.pop_back();

			set_pause(false);

			PCKRunner::get_singleton()->_finish_quit();
		}
	} else {
		quit(p_exit_code);
	}
}
