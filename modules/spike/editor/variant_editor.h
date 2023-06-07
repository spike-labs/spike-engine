/**
 * variant_editor.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "editor/editor_properties.h"
#include "scene/gui/dialogs.h"

#define EDIT_PROPERTY "edit_property"

class VariantEditor : public PopupPanel {
	GDCLASS(VariantEditor, PopupPanel);

	class DummyObject : public Object {
		GDCLASS(DummyObject, Object)

	protected:
		bool _set(const StringName &p_name, const Variant &p_value) {
			if (p_name == EDIT_PROPERTY) {
				edit_property = p_value;
				return true;
			}
			return false;
		}

		bool _get(const StringName &p_name, Variant &r_ret) const {
			if (p_name == EDIT_PROPERTY) {
				r_ret = edit_property;
				return true;
			}
			return false;
		}

	public:
		Variant edit_property;
	};

protected:
	static HashMap<String, VariantEditor *> editors;
	static void _bind_methods();

public:
	static void register_editor(const String &p_name, VariantEditor *p_editor);
	static void unregister_editor(const String &p_name);
	static VariantEditor *get_editor(const String &p_name);

private:
	const Control *control = nullptr;
	VBoxContainer *container = nullptr;
	DummyObject *dummy_object = nullptr;
	EditorProperty *property_editor = nullptr;
	HashMap<Variant::Type, EditorProperty *> property_cache;
	void _pop_editor(const Control *p_control);
	EditorProperty *_get_editor_for_property(const Variant::Type p_type);
    void _property_editor_rect_changed();
    void _property_editor_property_changed(const StringName &p_property, const Variant &p_value, const StringName &p_field, const bool &p_changing);

public:
	Callable variant_value_changed;
	VariantEditor();
	void pop_editor(const Control *p_control, const Variant::Type p_type, const Variant &p_variant, const String &p_variant_name = "");
};