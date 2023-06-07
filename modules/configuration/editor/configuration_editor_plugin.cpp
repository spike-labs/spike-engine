/**
 * configuration_editor_plugin.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "configuration_editor_plugin.h"
#include "editor/editor_node.h"

ConfigurationEditorPlugin::ConfigurationEditorPlugin() {
	resource_editor = memnew(ConfigurationResourceEditor);
	resource_editor->set_custom_minimum_size(Size2(0, 300));
	EditorNode::get_singleton()->add_bottom_panel_item(STTR("Configuration Editor"), (Control *)resource_editor);
}

bool ConfigurationEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<ConfigurationResource>(p_object) != nullptr;
}

void ConfigurationEditorPlugin::edit(Object *p_object) {
	ConfigurationResource *resource = Object::cast_to<ConfigurationResource>(p_object);
	if (nullptr != resource) {
		resource_editor->edit(resource);
		// edit_with_patch(resource, resource->get_path(), "res://test.tres");
	}
}

void ConfigurationEditorPlugin::edit_with_patch(Ref<ConfigurationResource> p_resource, const String &p_source_path, const String &p_out_patch) {
	if (p_resource.is_valid()) {
		Ref<ConfigurationResource> patch;
		if (!p_out_patch.is_empty()) {
			if (ResourceLoader::exists(p_out_patch)) {
				patch = ResourceLoader::load(p_out_patch);
			} else {
				if (p_resource->is_class_ptr(ConfigurationTable::get_class_ptr_static())) {
					Ref<ConfigurationTable> new_patch;
					new_patch.instantiate();
					patch = new_patch;
				} else if (p_resource->is_class_ptr(ConfigurationSet::get_class_ptr_static())) {
					Ref<ConfigurationSet> new_patch;
					new_patch.instantiate();
					patch = new_patch;
				}
				if (patch.is_valid()) {
					patch->set_path(p_out_patch);
				}
			}
			if (patch.is_valid() && patch != p_resource && p_resource->get_class_name() == patch->get_class_name()) {
				patch->set_source(p_resource, p_source_path);
			}
			patch->is_patch = true;
			resource_editor->edit(patch);
		}
	}
}

void ConfigurationEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		EditorNode::get_singleton()->make_bottom_panel_item_visible(resource_editor);
	}
}

ConfigurationEditorPlugin::~ConfigurationEditorPlugin() {
}