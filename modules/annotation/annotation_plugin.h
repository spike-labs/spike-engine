/**
 * annotation_plugin.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "annotation/annotation_export_plugin.h"
#include "editor/editor_inspector.h"
#include "editor/editor_plugin.h"
#include "scene/gui/panel_container.h"

const String ANNO_RESOURCE_DIR = "res://.ANNOTATIONS/";

class EditorPropertyAnnotation : public PanelContainer {
	GDCLASS(EditorPropertyAnnotation, PanelContainer)
private:
	Ref<Resource> target_resource;
	Ref<class Annotation> annotation;
	const StringName ANNO_META_NAME = SNAME("__anno__");

	class EditorPropertyResource *editor_property_resource;
	class Label *label;

	void _annotation_changed(const StringName &p_property, Variant p_value, const StringName &p_name, bool p_changing);

protected:
	static void _bind_methods() {}

public:
	EditorPropertyAnnotation();
	virtual ~EditorPropertyAnnotation();

	bool _set(const StringName &p_name, const Variant &p_property);
	bool _get(const StringName &p_name, Variant &r_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void setup(Ref<class Annotation> p_anno, Ref<Resource> p_target_resource, const String &p_base_type);
	void set_read_only(bool p_read_only);
};

class AnnotationInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(AnnotationInspectorPlugin, EditorInspectorPlugin)
protected:
public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
	virtual void parse_end(Object *p_object) override;
};

class AnnotationPlugin : public EditorPlugin {
	GDCLASS(AnnotationPlugin, EditorPlugin);

private:
	Ref<AnnotationInspectorPlugin> anno_inspector_plugin;

	void _check_resource_fiile_removed(const String &p_file);
	void _check_resource_fiile_moved(const String &p_old_file, const String &p_new_file);

	static AnnotationPlugin *singleton;

protected:
public:
	virtual String get_name() const override { return "AnnotationPlugin"; }
	virtual String to_string() override { return vformat("[AnnotationPlugin:%d]", get_instance_id()); }

	static String to_annotations_json(const HashMap<StringName, List<Ref<class BaseResourceAnnotation>>> &export_annos, const String &p_indent = "");
	static Dictionary load_annotations_json(const String &p_annotations_json);

	AnnotationPlugin();
	~AnnotationPlugin();

	static AnnotationPlugin *get_singleton() {
		if (singleton == nullptr) {
			singleton = memnew(AnnotationPlugin);
		}
		return singleton;
	}

	static String get_annotation_resource_path(Ref<Resource> p_resource);
	static String get_annotation_resource_path_by_path(const String &p_resource_path);

	static Ref<BaseResourceAnnotation> get_annotation_resource(Ref<Resource> p_resource);
	static Ref<BaseResourceAnnotation> get_annotation_resource_by_path(const String &p_resource_path);
};
