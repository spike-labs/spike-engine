/**
 * annotation_plugin.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "annotation/annotation_plugin.h"
#include "annotation/annotation_resource.h"
#include "annotation/script_annotation_parser_manager.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/object/message_queue.h"
#include "editor/editor_properties.h"
#include "editor/editor_resource_picker.h"
#include "editor/filesystem_dock.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/panel.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/packed_scene.h"


#pragma region EditorPropertyAnnotation

void EditorPropertyAnnotation::_annotation_changed(const StringName &p_property, Variant p_value, const StringName &p_name, bool p_changing) {
	if (cast_to<Annotation>(p_value) == nullptr && annotation.is_valid()) {
		auto p = annotation->get_path();
		if (FileAccess::exists(p)) {
			if (DirAccess::remove_absolute(p) == OK) {
				annotation = Variant();
				editor_property_resource->update_property();
			}
		}
	} else {
		Ref<BaseResourceAnnotation> anno = cast_to<BaseResourceAnnotation>(p_value);
		if (annotation.is_null() && anno.is_valid() && (anno->get_path().is_empty() || anno->get_target_resource_path().is_empty())) {
			if (target_resource->is_class("Script") && anno->is_class("ClassAnnotation") ||
					target_resource->is_class("PackedScene") && anno->is_class("SceneAnnotation") ||
					target_resource->is_class("Resource") && anno->is_class("ResourceAnnotation")) {
				if (!DirAccess::dir_exists_absolute(ANNO_RESOURCE_DIR)) {
					DirAccess::make_dir_recursive_absolute(ANNO_RESOURCE_DIR);
				}
				anno->set_target_resource_path(target_resource->get_path());
				auto save_path = AnnotationPlugin::get_annotation_resource_path(target_resource);
				ResourceSaver::save(anno, save_path);
				annotation = ResourceLoader::load(save_path);
				editor_property_resource->update_property();
			}
		}
	}
}
void EditorPropertyAnnotation::setup(Ref<Annotation> p_anno, Ref<Resource> p_target_resource, const String &p_base_type) {
	annotation = p_anno;
	target_resource = p_target_resource;
	editor_property_resource->setup(this, "annotation", p_base_type);
	editor_property_resource->set_label(p_base_type.capitalize());
	editor_property_resource->update_property();
	label->set_text(p_base_type.capitalize());
}

EditorPropertyAnnotation::EditorPropertyAnnotation() {
	auto panel_container = memnew(PanelContainer);
	panel_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	auto panel = memnew(Panel);
	panel->set_anchors_preset(Control::LayoutPreset::PRESET_FULL_RECT);
	panel_container->add_child(panel);
	label = memnew(Label);
	label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	panel_container->add_child(label);

	auto vbox = memnew(VBoxContainer);
	add_child(vbox);

	vbox->add_child(panel_container);

	editor_property_resource = memnew(EditorPropertyResource());
	editor_property_resource->set_selectable(true);
	editor_property_resource->set_object_and_property(this, "annotation");
	editor_property_resource->connect(SNAME("property_changed"), callable_mp(this, &EditorPropertyAnnotation::_annotation_changed));
	vbox->add_child(editor_property_resource);

	set_h_size_flags(Control::SIZE_EXPAND_FILL);
	set_v_size_flags(Control::SIZE_EXPAND_FILL);
}

EditorPropertyAnnotation::~EditorPropertyAnnotation() {}

bool EditorPropertyAnnotation::_set(const StringName &p_name, const Variant &p_property) {
	if (p_name == SNAME("annotation")) {
		annotation = p_property;
		return true;
	}
	return false;
}
bool EditorPropertyAnnotation::_get(const StringName &p_name, Variant &r_property) const {
	if (p_name == SNAME("annotation")) {
		r_property = annotation;
		return true;
	}
	return false;
}
void EditorPropertyAnnotation::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::OBJECT, "annotation", PROPERTY_HINT_RESOURCE_TYPE, "Annotation", PROPERTY_USAGE_EDITOR));
}
void EditorPropertyAnnotation::set_read_only(bool p_read_only) {
	editor_property_resource->set_read_only(p_read_only);
}
#pragma endregion

#pragma region AnnotationInspectorPlugin
bool AnnotationInspectorPlugin::can_handle(Object *p_object) {
	auto node = cast_to<Node>(p_object);
	if (node) {
		return !node->get_scene_file_path().is_empty();
	} else if (!p_object->is_class_ptr(Annotation::get_class_ptr_static())) {
		auto res = cast_to<Resource>(p_object);
		if (res && !res->get_path().is_empty()) {
			return FileAccess::exists(res->get_path());
		}
	}
	return false;
}

bool AnnotationInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (cast_to<Annotation>(p_object)) {
		if (p_path == "resource_local_to_scene" || p_path == "resource_path" || p_path == "resource_name") {
			return true;
		}
	}
	return false;
}
void AnnotationInspectorPlugin::parse_end(Object *p_object) {
	if (cast_to<Annotation>(p_object) == nullptr) {
		// Scripts( comment annotation only)
		if (auto script_ = cast_to<Script>(p_object)) {
			auto script_anno = ScriptAnnotationParserManager::get_script_annotation(script_->get_path());
			auto epa = memnew(EditorPropertyAnnotation);
			if (script_anno.is_valid()) {
				epa->setup(script_anno, script_, "ClassAnnotation");
				epa->set_read_only(true);
			} else {
				script_anno = AnnotationPlugin::get_annotation_resource(script_);
				epa->setup(script_anno, script_, "ClassAnnotation");
			}
			add_custom_control(epa);
			return;
		}

		// Scene
		if (p_object->is_class("PackedScene") || p_object->is_class("Node")) {
			Ref<PackedScene> scene = cast_to<PackedScene>(p_object);
			if (scene.is_null()) {
				scene = ResourceLoader::load(cast_to<Node>(p_object)->get_scene_file_path()).ptr();
			}
			if (scene.is_valid()) {
				auto anno = AnnotationPlugin::get_annotation_resource(scene);
				auto epa = memnew(EditorPropertyAnnotation);
				epa->setup(anno, scene, "SceneAnnotation");
				add_custom_control(epa);
				return;
			}
		}

		// Resources
		if (auto res = cast_to<Resource>(p_object)) {
			if (res->is_class("Annotation")) {
				return;
			}
			auto anno = AnnotationPlugin::get_annotation_resource(res);
			auto epa = memnew(EditorPropertyAnnotation);
			epa->setup(anno, res, "ResourceAnnotation");
			add_custom_control(epa);
			return;
		}
	}
}
#pragma endregion

#pragma region AnnotationPlugin
AnnotationPlugin *AnnotationPlugin::singleton = nullptr;

String AnnotationPlugin::to_annotations_json(const HashMap<StringName, List<Ref<BaseResourceAnnotation>>> &export_annos, const String &p_indent) {
	Dictionary anno_json;
	for (auto &&kv : export_annos) {
		auto annos = TypedArray<Dictionary>();
		for (auto &&anno : kv.value) {
			annos.push_back(anno->serialize_to_json());
		}
		anno_json[kv.key] = annos;
	}

	return JSON::stringify(anno_json, "\t");
}

Dictionary AnnotationPlugin::load_annotations_json(const String &p_annotations_json) {
	Variant json = JSON::parse_string(p_annotations_json);
	ERR_FAIL_COND_V(json.get_type() != Variant::DICTIONARY, {});

	Dictionary ret;
	Dictionary annos_json = json.operator Dictionary();
	auto keys = annos_json.keys();
	for (auto i = 0; i < keys.size(); ++i) {
		auto k = keys[i];
		auto v = annos_json[k];
		ERR_FAIL_COND_V(k.get_type() != Variant::STRING, {});
		ERR_FAIL_COND_V(v.get_type() != Variant::ARRAY, {});

		StringName anno_class = k.operator StringName();
		ERR_FAIL_COND_V(!ClassDB::is_parent_class(anno_class, "Annotation"), {});
		auto to_set = Array({}, Variant::OBJECT, anno_class, Variant());

		auto annos = v.operator Array();
		for (auto j = 0; j < annos.size(); ++j) {
			auto e = annos[j];
			ERR_FAIL_COND_V(e.get_type() != Variant::DICTIONARY, {});
			auto anno = cast_to<Annotation>(ClassDB::instantiate(anno_class));
			if (anno && anno->deserialize_from_json(e)) {
				to_set.push_back(anno);
			}
		}
		ret[anno_class] = to_set;
	}
	return ret;
}

void AnnotationPlugin::_check_resource_fiile_removed(const String &p_file) {
	auto anno_path = get_annotation_resource_path_by_path(p_file);
	if (FileAccess::exists(anno_path)) {
		DirAccess::remove_absolute(anno_path);
	}
}
void AnnotationPlugin::_check_resource_fiile_moved(const String &p_old_file, const String &p_new_file) {
	auto anno_old_path = get_annotation_resource_path_by_path(p_old_file);
	if (FileAccess::exists(anno_old_path)) {
		auto anno_new_path = get_annotation_resource_path_by_path(p_new_file);
		DirAccess::rename_absolute(anno_old_path, anno_new_path);
		Ref<BaseResourceAnnotation> anno = ResourceLoader::load(anno_new_path);
		anno->set_target_resource_path(p_new_file);
		print_line(ResourceSaver::save(anno, anno_new_path));
	}
}

AnnotationPlugin::AnnotationPlugin() {
	anno_inspector_plugin.instantiate();
	add_inspector_plugin(anno_inspector_plugin);

	FileSystemDock::get_singleton()->connect("file_removed", callable_mp(this, &AnnotationPlugin::_check_resource_fiile_removed));
	FileSystemDock::get_singleton()->connect("files_moved", callable_mp(this, &AnnotationPlugin::_check_resource_fiile_moved));
}

AnnotationPlugin::~AnnotationPlugin() {
	remove_inspector_plugin(anno_inspector_plugin);
}

String AnnotationPlugin::get_annotation_resource_path(Ref<Resource> p_resource) {
	return ANNO_RESOURCE_DIR.path_join(p_resource->get_path().md5_text() + ".anno");
}
String AnnotationPlugin::get_annotation_resource_path_by_path(const String &p_resource_path) {
	return ANNO_RESOURCE_DIR.path_join(ProjectSettings::get_singleton()->localize_path(p_resource_path).md5_text() + ".anno");
}

Ref<BaseResourceAnnotation> AnnotationPlugin::get_annotation_resource(Ref<Resource> p_resource) {
	Ref<Script> script = Object::cast_to<Script>(p_resource.ptr());
	if (script.is_valid() && script.is_valid()) {
		auto anno = ScriptAnnotationParserManager::get_script_annotation(script->get_path());
		if (anno.is_valid()) {
			return anno;
		}
	}

	auto file_path = get_annotation_resource_path(p_resource);
	if (ResourceLoader::exists(file_path) &&
			ClassDB::is_parent_class(ResourceLoader::get_resource_type(file_path), SNAME("Annotation"))) {
		return Object::cast_to<BaseResourceAnnotation>(ResourceLoader::load(file_path).ptr());
	}
	return nullptr;
}

Ref<BaseResourceAnnotation> AnnotationPlugin::get_annotation_resource_by_path(const String &p_resource_path) {
	if (!ResourceLoader::exists(p_resource_path)) {
		return nullptr;
	}

	if (ClassDB::is_parent_class(ResourceLoader::get_resource_type(p_resource_path), SNAME("Script"))) {
		auto anno = ScriptAnnotationParserManager::get_script_annotation(p_resource_path);
		if (anno.is_valid()) {
			return anno;
		}
	}

	auto file_path = get_annotation_resource_path_by_path(p_resource_path);
	if (ResourceLoader::exists(file_path) &&
			ClassDB::is_parent_class(ResourceLoader::get_resource_type(file_path), SNAME("Annotation"))) {
		return Object::cast_to<BaseResourceAnnotation>(ResourceLoader::load(file_path).ptr());
	}

	return nullptr;
}
#pragma endregion