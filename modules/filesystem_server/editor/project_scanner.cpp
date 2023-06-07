/**
 * project_scanner.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "project_scanner.h"
#include "core/io/resource_loader.h"
#include "core/string/node_path.h"
#include "core/string/string_name.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include "filesystem_server/filesystem_server.h"
#include "filesystem_server/providers/file_provider_pack.h"
#include "scene/resources/packed_scene.h"

void ProjectScanner::_scan_scene_text(const String &p_path) {
	Error err = OK;
	Ref<PackedScene> scene = _provider->load(p_path, "PackedScene", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	if (err != OK) {
		return;
	}

	HashMap<String, String> texts;

	auto state = scene->get_state();

	for (int i = 0; i < state->get_node_count(); i++) {
		NodePath path = state->get_node_path(i);
		StringName type = state->get_node_type(i);

		if (!ClassDB::is_parent_class(type, "Control") && !ClassDB::is_parent_class(type, "Window")) {
			continue;
		}

		// don't scan text from node that has auto_translate set to false
		bool auto_translate = true;
		for (int j = 0; j < state->get_node_property_count(i); j++) {
			if (state->get_node_property_name(i, j) == "auto_translate" && (bool)state->get_node_property_value(i, j) == false) {
				auto_translate = false;
				break;
			}
		}
		if (!auto_translate) {
			continue;
		}

		for (int j = 0; j < state->get_node_property_count(i); j++) {
			StringName name = state->get_node_property_name(i, j);
			Variant value = state->get_node_property_value(i, j);
			if (value.get_type() == Variant::Type::STRING) {
				// print_line("Path: " + path + " Type: " + type + " Name: " + name + " Value: " + String(value));
				texts[path.operator String() + ":" + name.operator String()] = value;
			}
		}
	}

	_scene_texts[p_path] = texts;
}

Error ProjectScanner::scan(Ref<FileProvider> p_provider) {
	Ref<FileProviderPack> provider = p_provider;
	ERR_FAIL_COND_V(provider.is_null(), ERR_PARSE_ERROR);

	_provider = provider;

	auto env = provider->get_project_environment();
	FileSystemServer::get_singleton()->push_current_provider(provider);
	for (String path : env->get_resource_paths()) {
		String type = ResourceLoader::get_resource_type(path);
		if (type == "PackedScene") {
			_scenes.push_back(path);
			_scan_scene_text(path);
		}
		else if(ClassDB::is_parent_class(type, "Translation")) {
			_translations.push_back(path);
		}
	}
	FileSystemServer::get_singleton()->pop_current_provider();
	return OK;
}

PackedStringArray ProjectScanner::get_scenes() const {
	return _scenes;
}

HashMap<String, String> ProjectScanner::get_scene_texts(const String &p_path) const {
	if (_scene_texts.has(p_path)) {
		return _scene_texts[p_path];
	}
	return HashMap<String, String>();
}

Dictionary ProjectScanner::_get_scene_texts(const String &p_path) const {
	Dictionary texts;
	if (_scene_texts.has(p_path)) {
		for (auto E : _scene_texts[p_path]) {
			texts[E.key] = E.value;
		}
	}
	return texts;
}

PackedStringArray ProjectScanner::get_translations() const {
	return _translations;
}

void ProjectScanner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("scan", "provider"), &ProjectScanner::scan);
	ClassDB::bind_method(D_METHOD("get_scenes"), &ProjectScanner::get_scenes);
	ClassDB::bind_method(D_METHOD("get_scene_texts", "path"), &ProjectScanner::_get_scene_texts);
	ClassDB::bind_method(D_METHOD("get_translations"), &ProjectScanner::get_translations);
}

ProjectScanner::ProjectScanner() {
}

ProjectScanner::~ProjectScanner() {
}
