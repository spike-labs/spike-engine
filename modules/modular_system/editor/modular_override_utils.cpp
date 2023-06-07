/**
 * modular_override_utils.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_override_utils.h"

#include "annotation/script_annotation_parser_manager.h"
#include "core/crypto/aes_context.h"

#include "filesystem_server/providers/file_provider_pack.h"
#include "filesystem_server/providers/file_provider_remap.h"

#include "core/io/file_access.h"
#include "editor/editor_file_system.h"
#include "filesystem_server/filesystem_server.h"

Ref<FileProviderPack> ModularOverrideUtils::get_or_create_packed_file_provider_with_caching(const String &p_packed_file, bool *r_is_new) {
	if (r_is_new) {
		*r_is_new = false;
	}

	Ref<FileProviderPack> provider = FileSystemServer::get_singleton()->get_provider_by_source(p_packed_file);

	if (!FileAccess::exists(p_packed_file) && provider.is_valid()) {
		FileSystemServer::get_singleton()->remove_provider(provider);
		return {};
	}

	if (provider.is_null()) {
		if (!FileSystemServer::get_singleton()->get_os_file_access()->file_exists(p_packed_file)) {
			return provider;
		}
		provider = FileProviderPack::create(p_packed_file);
		provider->set_meta(MODIFY_TIME_META_NAME, -1);
		FileSystemServer::get_singleton()->add_provider(provider);
	}

	auto current_modify_time = FileAccess::get_modified_time(p_packed_file);
	decltype(current_modify_time) modify_time = provider->get_meta(MODIFY_TIME_META_NAME);

	if (modify_time != current_modify_time) {
		provider->clear();
		if (provider->load_pack(p_packed_file)) {
			provider->set_meta(MODIFY_TIME_META_NAME, current_modify_time);
		}
		if (r_is_new) {
			*r_is_new = true;
		}
	}
	return provider;
}

bool ModularOverrideUtils::try_override_script_from_packed_file(const String &p_override_file, const String &p_packed_file, const String &p_file_path,
		const String &p_modular_override_folder) {
	auto provider = FileSystemServer::get_singleton()->get_provider_by_source(p_packed_file);
	if (!FileAccess::exists(p_override_file) && provider.is_valid()) {
		auto fa = FileAccess::open(p_override_file, FileAccess::WRITE);
		auto f = provider->open(p_file_path, FileAccess::READ);
		if (f.is_valid()) {
			Ref<Script> origin_script = provider->load(p_file_path, "Script");
			ScriptLanguage *script_language = origin_script->get_language();
			if (!script_language->can_inherit_from_file()) {
				// If language can't inherir from file, just copy its content.
				// TODO:: maybe it is more reasonable to throw a error and return false.
				fa->store_buffer(f->get_buffer(f->get_length()));
			} else {
				auto templates = script_language->get_built_in_templates("Object");
				if (templates.size() <= 0) {
					fa->store_buffer(f->get_buffer(f->get_length()));
				} else {
					const String md5 = (p_file_path.md5_text() + f->get_as_text().md5_text()).md5_text();

					// 1. Create a new script which extends from the origin script in pck with FileProviderRemap.
					auto remapped_extends_file = vformat(".%s_%s.%s", p_file_path.get_file().get_basename(), md5, p_file_path.get_extension());
					auto remapped_extends_path = p_file_path.get_base_dir().path_join(remapped_extends_file);
					auto overried_script = script_language->make_template(templates[0].content, "Override" + p_file_path.get_file().get_basename().capitalize().replace(" ", ""), "\"" + remapped_extends_path + "\"");

					// 2. Generate overridable methods as comment for the new script.
					auto code = overried_script->get_source_code();

					code += "\n";
					auto class_anno = ScriptAnnotationParserManager::get_script_annotation_by_script(origin_script);
					List<MethodInfo> methods;

					origin_script->get_script_method_list(&methods);

					static const auto find_method_info = [methods](const String &p_method_name, MethodInfo &r_method_info) {
						for (auto &&mi : methods) {
							if (mi.name == p_method_name) {
								r_method_info = mi;
								return true;
							}
						}
						return false;
					};

					// Get method annotations recursively.
					Vector<Ref<MethodAnnotation>> method_annos;
					auto base_class_anno = class_anno;
					while (base_class_anno.is_valid()) {
						auto ms = base_class_anno->get_methods();
						for (auto i = 0; i < ms.size(); ++i) {
							Ref<MethodAnnotation> to_add = ms[i];
							if (to_add.is_null()) {
								continue;
							}
							bool existed = false;
							for (auto j = 0; j < method_annos.size(); ++j) {
								Ref<MethodAnnotation> existed = method_annos[j];
								if (existed->get_member_name() == to_add->get_member_name()) {
									existed = true;
								}
							}
							if (!existed) {
								method_annos.push_back(to_add);
							}
						}

						auto base = base_class_anno->get_base_class_name_or_script_path();
						if (ClassDB::class_exists(base)) {
							break; // Built-in class
						}
						if (!base.is_empty() && provider->has_file(base)) {
							Ref<Script> base_script = provider->load(base);
							base_class_anno = ScriptAnnotationParserManager::get_script_annotation_by_script(base_script);
						} else {
							base_class_anno.unref();
						}
					}

					auto delimiter = ScriptAnnotationParserManager::get_single_line_comment_delimiter(script_language->get_extension());

					for (auto mi = 0; mi < method_annos.size(); ++mi) {
						Ref<MethodAnnotation> ma = method_annos[mi];

						MethodInfo method_info;
						if (!find_method_info(ma->get_member_name(), method_info)) {
							continue;
						}

						code += "\n";

						static const auto add_memnber_comment = [&code, delimiter](const String &p_comment) {
							if (!p_comment.is_empty()) {
								auto comment_lines = p_comment.split("\n");
								code += " " + comment_lines[0];
								for (auto i = 1; i < comment_lines.size(); ++i) {
									code += delimiter + "\t\t" + comment_lines[i];
								}
							}
						};

						// Generate annotation comment.
						code += "\n" + delimiter + " @anno_method: " + ma->get_member_name();
						add_memnber_comment(ma->get_comment());

						for (auto pi = 0; pi < ma->get_parameter_count(); ++pi) {
							auto p = ma->get_parameter(pi);
							code += "\n" + delimiter + " @param: " + p->get_member_name();
							if (!p->get_class_name().is_empty()) {
								code += " @type: " + p->get_class_name();
							} else if (p->get_type() != Variant::NIL) {
								code += " @type: " + Variant::get_type_name(p->get_type());
							}
							add_memnber_comment(p->get_comment());
						}

						auto r = ma->get_return_value();
						auto r_type = r->get_class_name();
						if (r_type.is_empty() && r->get_type() != Variant::NIL) {
							r_type = Variant::get_type_name(r->get_type());
						}
						if (!r_type.is_empty() || !r->get_comment().is_empty()) {
							code += "\n" + delimiter + " @return";
							if (!r_type.is_empty()) {
								code += ": " + r_type;
							}
							add_memnber_comment(r->get_comment());
						}

						// Gemerate method template
						PackedStringArray args;
						for (auto &&arg : method_info.arguments) {
							String type = arg.class_name;
							if (type.is_empty() && arg.type != Variant::NIL) {
								type = Variant::get_type_name(arg.type);
							} else {
								type = "var";
							}
							args.push_back(arg.name + ":" + type);
						}

						auto parser = ScriptAnnotationParserManager::get_annotation_parser(script_language->get_extension());
						ERR_FAIL_COND_V(parser.is_null(), false);

						r_type = "";
						if (!r->get_class_name().is_empty() && ClassDB::class_exists(r->get_class_name())) {
							r_type = r->get_class_name();
						}

						if (r_type.is_empty()) {
							r_type = Variant::get_type_name(r->get_type());
							if (r_type == "Nil") {
								r_type = "";
							}
						}

						// We assume that a variant return value should has comment.
						auto method_lines = parser->make_function("ReplaceOverrideClass", ma->get_member_name(), args,
														  r_type, r_type.is_empty() && !r->get_comment().is_empty(), script_language->get_extension())
													.split("\n");

						for (auto i = 0; i < method_lines.size() - 1; ++i) {
							code += "\n" + delimiter + method_lines[i];
						}

						if (method_lines[method_lines.size() - 1].is_empty()) {
							code += "\n";
						} else {
							code += "\n" + delimiter + method_lines[method_lines.size() - 1];
						}
					}

					fa->store_string(code);

					// 3. Save to .remap
					Ref<ConfigFile> remap_conf = get_loaded_remap_config(p_modular_override_folder);
					remap_conf->set_value(p_override_file.get_file(), MODULAR_REMAP_PACKED_FILE_KEY, p_packed_file);
					remap_conf->set_value(p_override_file.get_file(), MODULAR_REMAP_ORIGIN_FILE_KEY, p_file_path);
					remap_conf->set_value(p_override_file.get_file(), MODULAR_REMAP_EXTENDS_KEY, remapped_extends_path);
					save_remap_config(remap_conf);

					// 4. Remap extends script path from pck.
					Ref<FileProviderRemap> remapper = get_or_create_provider_remapper_with_caching();
					if (!remapper->has_remap(remapped_extends_path)) {
						remapper->add_provider_remap(remapped_extends_path, provider, p_file_path);
					}
				}
			}
		}
		fa.unref();
		EditorFileSystem::get_singleton()->update_file(p_override_file);
		return true;
	}
	return false;
}

Ref<FileProviderRemap> ModularOverrideUtils::get_or_create_provider_remapper_with_caching() {
	for (auto i = 0; i < FileSystemServer::get_singleton()->get_provider_count(); ++i) {
		Ref<FileProviderRemap> remapper = FileSystemServer::get_singleton()->get_provider(i);
		if (remapper.is_valid() && remapper->has_meta(MODULAR_COMPOSITE_FILE_REMAPPER_META_NAME)) {
			return remapper;
		}
	}
	Ref<FileProviderRemap> remapper;
	remapper.instantiate();
	remapper->set_meta(MODULAR_COMPOSITE_FILE_REMAPPER_META_NAME, MODULAR_COMPOSITE_FILE_REMAPPER_META_NAME);
	FileSystemServer::get_singleton()->add_provider(remapper);
	return remapper;
}

bool _try_add_remap(const Ref<ConfigFile> &p_remap_conf, const String &p_file_name, Ref<FileProviderRemap> p_remapper = Variant()) {
	if (!p_remap_conf->has_section(p_file_name) ||
			!p_remap_conf->has_section_key(p_file_name, MODULAR_REMAP_PACKED_FILE_KEY) ||
			!p_remap_conf->has_section_key(p_file_name, MODULAR_REMAP_ORIGIN_FILE_KEY) ||
			!p_remap_conf->has_section_key(p_file_name, MODULAR_REMAP_EXTENDS_KEY)) {
		return false;
	}

	const String packed_file_path = p_remap_conf->get_value(p_file_name, MODULAR_REMAP_PACKED_FILE_KEY);
	if (!FileAccess::exists(packed_file_path)) {
		return false;
	}

	if (p_remapper.is_null()) {
		p_remapper = ModularOverrideUtils::get_or_create_provider_remapper_with_caching();
	}

	auto provider = ModularOverrideUtils::get_or_create_packed_file_provider_with_caching(packed_file_path);

	if (!p_remapper->has_remap(p_remap_conf->get_value(p_file_name, MODULAR_REMAP_EXTENDS_KEY))) {
		p_remapper->add_provider_remap(
				p_remap_conf->get_value(p_file_name, MODULAR_REMAP_EXTENDS_KEY),
				provider,
				p_remap_conf->get_value(p_file_name, MODULAR_REMAP_ORIGIN_FILE_KEY));
	}
	return true;
}

void ModularOverrideUtils::scan_and_remap_override_scripts_path(const Array &p_resources) {
	String folder_dir;
	bool has_remap_conf = true;
	Ref<ConfigFile> remap_conf;
	Ref<FileProviderRemap> remapper;
	for (auto i = 0; i < p_resources.size(); ++i) {
		const String path = p_resources[i];
		if (!ResourceLoader::exists(path) || ResourceLoader::get_resource_type(path) != Script::get_class_static()) {
			continue;
		}
		Ref<Script> script = ResourceLoader::load(path);
		if (script.is_valid()) {
			Ref<Script> script = ResourceLoader::load(path);
			if (folder_dir != script->get_path().get_base_dir()) {
				folder_dir = script->get_path().get_base_dir();

				if (!FileAccess::exists(folder_dir.path_join(MODULAR_REMAP_FILES))) {
					has_remap_conf = false;
					continue;
				}

				remap_conf = get_loaded_remap_config(folder_dir);
			} else if (!has_remap_conf) {
				continue;
			}

			const auto script_file_name = script->get_path().get_file();
			_try_add_remap(remap_conf, script_file_name, remap_conf);
		}
	}
}

void ModularOverrideUtils::trace_file_path_change(const String &p_old_file, const String &p_new_file) {
	auto old_conf = get_loaded_remap_config(p_old_file.get_base_dir());
	auto old_file_name = p_old_file.get_file();
	if (!old_conf->has_section(old_file_name)) {
		return;
	}

	List<String> keys;
	old_conf->get_section_keys(old_file_name, &keys);

	if (p_new_file.is_empty()) {
		if (keys.size() <= 1) {
			DirAccess::remove_absolute(old_conf->get_meta(SAVE_PATH_META_NAME, ""));
		} else {
			old_conf->erase_section(old_file_name);
			save_remap_config(old_conf);
		}
		return;
	}

	List<Variant> values;
	for (const auto &k : keys) {
		values.push_back(old_conf->get_value(old_file_name, k));
	}

	if (keys.size() <= 1) {
		DirAccess::remove_absolute(old_conf->get_meta(SAVE_PATH_META_NAME, ""));
	} else {
		old_conf->erase_section(old_file_name);
		save_remap_config(old_conf);
	}

	auto new_conf = get_loaded_remap_config(p_new_file.get_base_dir());
	while (!keys.is_empty()) {
		new_conf->set_value(p_new_file.get_file(), keys.front()->get(), values.front()->get());
		keys.pop_front();
		values.pop_front();
	}

	save_remap_config(new_conf);
}
