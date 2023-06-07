/**
 * native_wrapper_export_plugin.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "native_wrapper_export_plugin.h"
#include "core/io/config_file.h"

static bool check_all_tags_met(const Vector<String> &tags, const HashSet<String> &p_features) {
	bool all_tags_met = true;
	for (int i = 0; i < tags.size(); i++) {
		String tag = tags[i].strip_edges();
		if (!p_features.has(tag)) {
			all_tags_met = false;
			break;
		}
	}
	return all_tags_met;
}

static String _get_shared_path(const String &p_dst, String p_path) {
	if (p_path.is_absolute_path()) {
		print_line("Skipping export of out-of-project library " + p_path);
		return String();
	} else if (!p_path.is_resource_file()) {
		p_path = p_dst.get_base_dir().path_join(p_path);
	}
	return p_path;
}

void NativeWrapperExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	if (p_type != "NativeWrapper") {
		return;
	}

	Ref<ConfigFile> config;
	config.instantiate();

	Error err = config->load(p_path);
	ERR_FAIL_COND_MSG(err, "Failed to load NativeWrapper file: " + p_path);

	List<String> libraries;

	config->get_section_keys("libraries", &libraries);

	bool found = false;
	for (const String &E : libraries) {
		Vector<String> tags = E.split(".");
		if (check_all_tags_met(tags, p_features)) {
			auto value = config->get_value("libraries", E);
			switch (value.get_type()) {
				case Variant::STRING: {
					String path = _get_shared_path(p_path, value);
					if (!IS_EMPTY(path)) {
						add_shared_object(path, tags);
						found = true;
					}
				} break;
				case Variant::ARRAY: {
					Array array = value;
					for (int i = 0; i < array.size(); ++i) {
						String path = _get_shared_path(p_path, array[i]);
						if (!IS_EMPTY(path)) {
							add_shared_object(path, tags);
							found = true;
						}
					}
				} break;
				default:
					break;
			}
		}
	}

	if (!found) {
		Vector<String> features;
		for (const String &E : p_features) {
			features.append(E);
		}
	}

	List<String> dependencies;
	if (config->has_section("dependencies")) {
		config->get_section_keys("dependencies", &dependencies);

		for (const String &E : dependencies) {
			Vector<String> tags = E.split(".");
			if (check_all_tags_met(tags, p_features)) {
				Dictionary dependency = config->get_value("dependencies", E);
				for (const Variant *key = dependency.next(nullptr); key; key = dependency.next(key)) {
					String libpath = *key;
					if (libpath.is_absolute_path()) {
						print_line("Skipping export of out-of-project library " + libpath);
						continue;
					} else if (!libpath.is_resource_file()) {
						libpath = p_path.get_base_dir().path_join(libpath);
					}
					String target_path = dependency[*key];
					add_shared_object(libpath, tags, target_path);
				}
			}
		}
	}
}