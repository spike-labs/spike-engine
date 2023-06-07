/**
 * status_prompt.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_rect.h"
#include "spike_define.h"

class StatusPrompt : public HBoxContainer {
	GDCLASS(StatusPrompt, HBoxContainer);

private:
	TextureRect *icon;
	Label *label;
	void _notification(int p_what);

public:
	enum Status {
		Warning,
		Error,
		Success,
	};
	void set_text(const String &p_text);
	void set_status(const Status &p_status);
	StatusPrompt(const Status &p_status);
	StatusPrompt();
	~StatusPrompt();
};
