/**
 * status_prompt.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "status_prompt.h"
#include "editor/editor_node.h"

void StatusPrompt::_notification(int p_what) {
}

void StatusPrompt::set_text(const String &p_text) {
	if (label) {
		label->set_text(p_text);
	}
}

void StatusPrompt::set_status(const Status &p_status) {
	if (p_status == Status::Warning) {
		icon->set_texture(EditorNode::get_singleton()->get_gui_base()->get_theme_icon(SNAME("StatusWarning"), SNAME("EditorIcons")));
	} else if (p_status == Status::Success) {
		icon->set_texture(EditorNode::get_singleton()->get_gui_base()->get_theme_icon(SNAME("StatusSuccess"), SNAME("EditorIcons")));
	} else if (p_status == Status::Error) {
		icon->set_texture(EditorNode::get_singleton()->get_gui_base()->get_theme_icon(SNAME("StatusError"), SNAME("EditorIcons")));
	}
}

StatusPrompt::StatusPrompt(const Status &p_status) :
		StatusPrompt() {
	set_status(Status::Warning);
}

StatusPrompt::StatusPrompt() {
	icon = CREATE_NODE(this, TextureRect);
	icon->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	label = CREATE_NODE(this, Label);
	label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
}

StatusPrompt::~StatusPrompt() {
}