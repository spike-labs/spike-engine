/**
 * resource_format_configurable.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "resource_format_configurable.h"
#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "scene/resources/packed_scene.h"
#include "configuration/configuration_server.h"

#pragma region Resource Peoperties override
const String CONFIG_FILE_DIR_TEMPLATE = "res://PEOPERTY_OVERRIDE/%s.%s";

class SpikeSceneState : public SceneState {
public:
	void set_node_property(int p_node, StringName p_property_name, Variant p_value) {
		ERR_FAIL_INDEX(p_node, nodes.size());
		auto value = add_value(p_value);

		// Try to Replace.
		for (auto i = 0; i < nodes.write[p_node].properties.size(); ++i) {
			SceneState::NodeData::Property &prop = nodes.write[p_node].properties.write[i];
			if (p_property_name == get_node_property_name(p_node, i)) {
				prop.value = value;
				return;
			}
		}

		// Add new.
		auto name = add_name(p_property_name);
		add_node_property(p_node, name, value);
	}

	int find_node(const NodePath &p_path) {
		for (auto i = 0; i < nodes.size(); ++i) {
			// p_path.get_name(int p_idx)
			if (get_node_path(i) == p_path) {
				return i;
			}
		}
		return -1;
	}
};

void _override_packed_scenes_node_properties(Ref<PackedScene> &p_scene, const Dictionary &p_override_properties) {
	// Override by "_bundled"
	if (p_override_properties.has("_bundled")) {
		auto _bundled = p_override_properties["_bundled"];
		if (_bundled.get_type() == Variant::DICTIONARY) {
			p_scene->get_state()->set_bundled_scene(_bundled);
		}
	}
	// Override by "override_node_propeties"
	if (p_override_properties.has("override_node_properties")) {
		Dictionary override_node_properties = p_override_properties["override_node_properties"];

		Ref<SpikeSceneState> scene_state;
		scene_state.instantiate();
		scene_state->copy_from(p_scene->get_state());

		Array override_nodes = override_node_properties.keys();
		for (auto i = 0; i < override_nodes.size(); ++i) {
			int node = scene_state->find_node(override_nodes[i]);
			ERR_CONTINUE(node < 0);
			Dictionary properties = override_node_properties[override_nodes[i]];

			Array prop_names = properties.keys();
			for (auto j = 0; j < prop_names.size(); j++) {
				StringName prop_name = prop_names[j];
				scene_state->set_node_property(node, prop_name, properties[prop_name]);
			}
		}

		p_scene->get_state()->copy_from(scene_state);
	}
}

void override_properties(Ref<Resource> &p_res) {
	auto p = p_res->get_path();

	auto cfg_file = p_res->get_path().md5_text();

	auto cfg_file_path = vformat(CONFIG_FILE_DIR_TEMPLATE, cfg_file, "tres");
	if (!FileAccess::exists(cfg_file_path)) {
		cfg_file_path = vformat(CONFIG_FILE_DIR_TEMPLATE, cfg_file, "res");
		if (!FileAccess::exists(cfg_file_path)) {
			return;
		}
	}

	Ref<ConfigurationSet> cfg = ConfigurationServer::load_set(cfg_file_path);
	ERR_FAIL_COND(!cfg.is_valid());

	Dictionary override_properties = cfg->to_dictionary();
	if (!override_properties.is_empty()) {
		// Spacial operation for PackedScene.
		Ref<PackedScene> packed_scene = Object::cast_to<PackedScene>(p_res.ptr());
		if (packed_scene.is_valid()) {
			_override_packed_scenes_node_properties(packed_scene, override_properties);
			return;
		}

		// Other Resources;
		TypedArray<StringName> properties = override_properties.keys();
		for (auto i = 0; i < properties.size(); ++i) {
			StringName prop = properties[i];
			p_res->set(prop, override_properties[prop]);
		}
	}

	ERR_FAIL_COND(ConfigurationServer::unload(cfg_file_path) != OK);
}

#pragma endregion

Ref<Resource> OverrideFormatLoaderBinary::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	bool cached = ResourceCache::has(local_path);

	Ref<Resource> res = ResourceFormatLoaderBinary::load(p_path, p_original_path, r_error, p_use_sub_threads, r_progress, p_cache_mode);

	// Try to get override properties from ConfigurationServer, then change the loaded resource.
	if (res.is_valid() && !cached) {
		// Only overrie properties of uncached resource.
		override_properties(res);
	}
	return res;
}

Ref<Resource> OverrideFormatLoaderText::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	bool cached = ResourceCache::has(local_path);

	Ref<Resource> res = ResourceFormatLoaderText::load(p_path, p_original_path, r_error, p_use_sub_threads, r_progress, p_cache_mode);

	// Try to get override properties from ConfigurationServer, then change the loaded resource.
	if (res.is_valid() && !cached) {
		// Only overrie properties of uncached resource.
		override_properties(res);
	}
	return res;
}
