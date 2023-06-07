/**
 * modular_resource_picker.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "editor/editor_resource_picker.h"
#include "spike_define.h"

class ModularResourcePicker : public EditorResourcePicker {
	GDCLASS(ModularResourcePicker, EditorResourcePicker);

	void _update_resource_preview(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, ObjectID p_obj);

protected:
	static void _bind_methods();
};