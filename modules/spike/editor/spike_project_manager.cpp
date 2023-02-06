/**
 * spike_project_manager.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "spike_project_manager.h"

#include "core/version.h"
#include "scene/gui/link_button.h"
#include "spike/version.h"

SpikeProjectManager::SpikeProjectManager() {
	auto nodes = find_children("", "LinkButton", true, false);
	if (nodes.size() == 0)
		return;

	LinkButton *version_btn = Object::cast_to<LinkButton>(nodes[0]);
	if (version_btn == nullptr)
		return;

	String hash = String(VERSION_HASH);
	if (hash.length() != 0) {
		hash = " " + vformat("[%s]", hash.left(9));
	}

	version_btn->set_text("v" SPIKE_FULL_BUILD + hash);
}
