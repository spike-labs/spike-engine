/**
 * modular_data.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_data.h"

#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "filesystem_server/filesystem_server.h"
#include "filesystem_server/providers/file_provider_pack.h"
#include "modular_export_plugin.h"
#include "modular_export_utils.h"
#include "modular_graph.h"
#include "modular_override_utils.h"
#include "modular_system/editor/modular_data_inspector_plugin.h"
#include "package_system/editor/editor_package_system.h"
#include "scene/gui/menu_button.h"

#ifdef TOOLS_ENABLED
#include "annotation/annotation_export_plugin.h"
#include "annotation/annotation_resource.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/export/editor_export.h"
#include "editor/export/editor_export_preset.h"
#include "modular_editor_plugin.h"
#include "modular_export_plugin.h"
#include "modular_resource_picker.h"
#endif

#ifdef WINDOWS_ENABLED
#include "platform/windows/export/export_plugin.h"
#elif defined(UWP_ENABLED)
#include "platform/uwp/export/export_plugin.h"
#elif defined(MACOS_ENABLED)
#include "platform/macos/export/export_plugin.h"
#elif defined(UNIX_ENABLED)
#include "platform/linuxbsd/export/export_plugin.h"
#endif

// ModularData
void ModularData::_set_export_name(const String &p_path) {
	export_name = p_path;
}

String ModularData::_get_export_name() const {
	return export_name;
}

void ModularData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_export_name"), &ModularData::_set_export_name);
	ClassDB::bind_method(D_METHOD("get_export_name"), &ModularData::_get_export_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "export_name"), "set_export_name", "get_export_name");

	ClassDB::bind_method(D_METHOD("export_data"), &ModularData::export_data);
	ADD_SIGNAL(MethodInfo("name_changed", PropertyInfo(Variant::STRING, "name")));

	GDVIRTUAL_BIND(_validation)
	GDVIRTUAL_BIND(_get_module_name)
	GDVIRTUAL_BIND(_edit_data, "context")
	GDVIRTUAL_BIND(_export_data)
	GDVIRTUAL_BIND(_get_export_name)
	GDVIRTUAL_BIND(_get_module_info)
	GDVIRTUAL_BIND(_inspect_modular, "node")
}

void ModularData::validation() {
	REQUIRE_TOOL_SCRIPT(this);
	GDVIRTUAL_CALL(_validation);
}

String ModularData::get_module_name() const {
	REQUIRE_TOOL_SCRIPT(this);
	String ret;
	if (const_cast<ModularData *>(this)->GDVIRTUAL_CALL(_get_module_name, ret)) {
		return ret;
	}
	return String();
}

void ModularData::edit_data(Object *p_context) {
	REQUIRE_TOOL_SCRIPT(this);
	GDVIRTUAL_REQUIRED_CALL(_edit_data, p_context);
}

String ModularData::export_data() {
	REQUIRE_TOOL_SCRIPT(this);
	String ret;
	GDVIRTUAL_REQUIRED_CALL(_export_data, ret);
	return ret;
}

String ModularData::get_export_name() {
	REQUIRE_TOOL_SCRIPT(this);
	String ret;
	if (!GDVIRTUAL_CALL(_get_export_name, ret)) {
		ret = export_name;
	}
	return ret;
}

Dictionary ModularData::get_module_info() {
	REQUIRE_TOOL_SCRIPT(this);
	Dictionary ret;
	GDVIRTUAL_CALL(_get_module_info, ret);
	return ret;
}

void ModularData::inspect_modular(Node *p_parent) {
	REQUIRE_TOOL_SCRIPT(this);
	GDVIRTUAL_CALL(_inspect_modular, p_parent);
}

// ModularResource

void ModularResource::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_main_scene", "scene"), &ModularResource::set_main_scene);
	ClassDB::bind_method(D_METHOD("get_main_scene"), &ModularResource::get_main_scene);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "main_scene", PROPERTY_HINT_RESOURCE_TYPE, PackedScene::get_class_static()), "set_main_scene", "get_main_scene");

	// TODO:: Change to PackedStringArray.
	ClassDB::bind_method(D_METHOD("set_resources", "resources"), &ModularResource::set_resources);
	ClassDB::bind_method(D_METHOD("get_resources"), &ModularResource::get_resources);
	// String hint = vformat("%d/%d:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, Resource::get_class_static());
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "resources"), "set_resources", "get_resources");

	ClassDB::bind_method(D_METHOD("set_ext", "ext"), &ModularResource::set_ext);
	ClassDB::bind_method(D_METHOD("get_ext"), &ModularResource::get_ext);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ext", PROPERTY_HINT_ENUM, "PCK,ZIP"), "set_ext", "get_ext");

	ClassDB::bind_method(D_METHOD("set_module_name", "module_name"), &ModularResource::set_module_name);
	ClassDB::bind_method(D_METHOD("get_module_name"), &ModularResource::get_module_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "module_name"), "set_module_name", "get_module_name");

	ClassDB::bind_method(D_METHOD("set_module_comment", "module_comment"), &ModularResource::set_module_comment);
	ClassDB::bind_method(D_METHOD("get_module_comment"), &ModularResource::get_module_comment);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "module_comment"), "set_module_comment", "get_module_comment");
}

void ModularResource::set_module_name(const String &p_name) {
	if (module_name != p_name) {
		module_name = p_name;
		emit_signal("name_changed", p_name.is_empty() ? STTR("[Unnamed]") : p_name);
	}
}
String ModularResource::get_module_name() const {
	return module_name;
}
void ModularResource::set_module_comment(const String &p_comment) {
	module_comment = p_comment;
}
String ModularResource::get_module_comment() const {
	return module_comment;
}

void ModularResource::set_main_scene(const Ref<PackedScene> &p_scene) {
	main_scene = p_scene;
}

Ref<PackedScene> ModularResource::get_main_scene() const {
	return main_scene;
}

void ModularResource::set_resources(const Array &p_resources) {
	resources = p_resources;
	ModularOverrideUtils::scan_and_remap_override_scripts_path(get_resources());
	emit_changed();
}

// All elements will be converted to String Path.
Array ModularResource::get_resources() {
	auto fa = FileSystemServer::get_singleton()->get_os_file_access();
	for (auto i = 0; i < resources.size(); ++i) {
		Variant var = resources[i];
		switch (var.get_type()) {
			case Variant::OBJECT: {
				auto res = Object::cast_to<Resource>(var);
				if (res && fa->file_exists(res->get_path())) {
					// Conver to UID text.
					auto uid = ResourceLoader::get_resource_uid(res->get_path());
					resources[i] = ResourceUID::get_singleton()->id_to_text(uid);
					continue;
				}
			} break;
			case Variant::STRING: {
				String path = var;
				if (path.is_empty()) {
					break;
				}

				if (ResourceLoader::exists(path)) {
					// If is resource path, ensure to check by using real path.
					auto uid = ResourceLoader::get_resource_uid(path);
					if (uid != ResourceUID::INVALID_ID) {
						path = ResourceUID::get_singleton()->get_id_path(uid);
						// Store as UID text.
						resources[i] = ResourceUID::get_singleton()->id_to_text(uid);
					}
				}

				if (fa->file_exists(path)) {
					continue;
				}
			} break;

			default: {
			} break;
		}

		resources.remove_at(i);
		i -= 1;
	}
	return resources;
}

void ModularResource::set_ext(int p_ext) {
	ext = p_ext;
	if (!IS_EMPTY(export_name)) {
		String expect_ext;
		switch (ext) {
			case 0:
				expect_ext = "pck";
				break;
			case 1:
				expect_ext = "zip";
				break;
			default:
				return;
		}
		String ext_str = export_name.get_extension();
		if (ext_str.length() > 0 && expect_ext != ext_str) {
			String base_name = export_name.get_basename();
			export_name = base_name + "." + expect_ext;
		}
	}
}

int ModularResource::get_ext() const {
	return ext;
}

void ModularResource::edit_data(Object *p_context) {
	REQUIRE_TOOL_SCRIPT(this);
	if (GDVIRTUAL_CALL(_edit_data, p_context)) {
		return;
	}

	if (resources.size() <= 0) {
		return;
	}

	Object *obj;
	for (auto i = 0; i < resources.size(); ++i) {
		Variant var = resources[i];
		switch (var.get_type()) {
			case Variant::OBJECT: {
				obj = var;
			} break;
			case Variant::STRING: {
				if (ResourceLoader::exists(var)) {
					obj = ResourceLoader::load(var).ptr();
				}
			} break;
			default:
				break;
		}

		if (obj) {
			break;
		}
	}

	if (obj) {
		auto &editor_data = EditorNode::get_editor_data();
		for (size_t i = 0; i < editor_data.get_editor_plugin_count(); i++) {
			auto plugin = editor_data.get_editor_plugin(i);
			if (plugin->handles(obj)) {
				plugin->edit(obj);
			}
		}
	}
	//DLog("todo: edit %s", this);
}

String ModularResource::export_data() {
	REQUIRE_TOOL_SCRIPT(this);
	String ret;
	if (GDVIRTUAL_CALL(_export_data, ret))
		return ret;

#ifndef TOOLS_ENABLED
	ERR_PRINT("Export modular data is only available in the editor.");
	return String();
#endif

	auto fa = FileSystemServer::get_singleton()->get_os_file_access();
	Vector<Ref<Resource>> res_list;
	Vector<String> file_list;
	Ref<PackedScene> main_scene = get_main_scene();
	if (main_scene.is_valid()) {
		res_list.push_back(main_scene);
	}
	for (int i = 0; i < resources.size(); i++) {
		Variant var = resources[i];
		switch (var.get_type()) {
			case Variant::OBJECT: {
				auto res = Object::cast_to<Resource>(var);
				if (res) {
					res_list.push_back(res);
				}
			} break;
			case Variant::STRING: {
				if (ResourceLoader::exists(var)) {
					res_list.push_back(ResourceLoader::load(var));
					continue;
				}
				// As general file.
				if (fa->file_exists(var)) {
					file_list.push_back(var);
				}
			} break;

			default:
				break;
		}
	}

	const String anno_cache_path = "res://.godot/module_anno/";
	String cache_file = "";

	Ref<ModuleAnnotation> cache;
	cache.instantiate();

	if (module_name.strip_edges().is_empty()) {
		WARN_PRINT(vformat("ModularResource has not a valid module name, Will be named as \"Untitled\"."));
		cache->set_module_name("Untitled");
	} else {
		cache->set_module_name(module_name);
	}

	TypedArray<Resource> to_set;
	to_set.resize(res_list.size());
	for (auto i = 0; i < res_list.size(); ++i) {
		to_set.set(i, res_list.get(i));
	}

	cache->set_comment(module_comment);
	cache->set_resources(to_set);

	auto cache_file_name = module_name + "_" + generate_scene_unique_id() + ".res";
	cache_file = anno_cache_path.path_join(cache_file_name);

	if (!DirAccess::dir_exists_absolute(anno_cache_path)) {
		DirAccess::make_dir_recursive_absolute(anno_cache_path);
	}
	ResourceSaver::save(cache, cache_file);
	cache = ResourceLoader::load(cache_file);

	cache->set_meta("_main_module_", true);

	res_list.resize(res_list.size() + 1);
	res_list.set(res_list.size() - 1, res_list[0]);
	res_list.set(0, cache);

	String save_path = ModularExportUtils::export_specific_resources(main_scene, res_list, file_list, true, ext);

	if (FileAccess::exists(cache_file)) {
		DirAccess::remove_absolute(cache_file);
		auto files = DirAccess::open(anno_cache_path)->get_files();
		if (files.size() == 1 && files[0] == ".uids") {
			DirAccess::remove_absolute(anno_cache_path.path_join(files[0]));
		}
		if (DirAccess::open(anno_cache_path)->get_files().size() == 0) {
			DirAccess::remove_absolute(anno_cache_path);
		}
	}
	return save_path;
}

void ModularResource::inspect_modular(Node *p_parent) {
	auto graph_node = find_parent_type<GraphNode>(p_parent);
	if (!graph_node)
		return;

	String out = EditorModularInterface::_get_node_output(graph_node, 0);
	if (out == TEXT_MODULE) {
	} else if (out == CONF_MODULE) {
	} else if (EditorModularInterface::_node_is_composite(graph_node)) {
		{
			auto label = memnew(Label);
			p_parent->add_child(label);
			label->set_vertical_alignment(VERTICAL_ALIGNMENT_BOTTOM);
			label->set_text(STTR("Main Scene:"));

			auto res_picker = memnew(ModularResourcePicker());
			p_parent->add_child(res_picker);
			res_picker->set_base_type("PackedScene");
			res_picker->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			res_picker->connect("resource_changed", callable_mp(this, &ModularResource::change_main_scene).bind(res_picker));
			res_picker->set_edited_resource(get_main_scene());
		}

		{
			auto hbox = memnew(HBoxContainer);

			hbox->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
			auto label = memnew(Label);
			label->set_text(STTR("Resources:"));

			class ResourcesButton : public Button {
			public:
				void modular_data_changed(Ref<ModularResource> p_data) {
					ERR_FAIL_COND(p_data.is_null());
					set_text(STTR("Count") + ": " + itos(p_data->get_resources().size()));
				}
			};

			auto btn = memnew(ResourcesButton);
			btn->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
			btn->connect("pressed", callable_mp(ModularResourcesSelectDialog::get_singleton(), &ModularResourcesSelectDialog::setup_and_popup).bind(this));
			connect("changed", callable_mp(btn, &ResourcesButton::modular_data_changed).bind(this));
			btn->modular_data_changed(this);

			hbox->add_child(label);
			hbox->add_child(btn);
			p_parent->add_child(hbox);
		}
	}
}

void ModularResource::change_main_scene(const Ref<PackedScene> &p_scene, Node *p_node) {
	auto res_picker = Object::cast_to<EditorResourcePicker>(p_node);
	auto interface = ModularEditorPlugin::find_interface();
	if (interface) {
		auto &his = interface->create_undo_redo(STTR("Change main scene"));
		his.undo_redo->add_do_method(callable_mp(this, &ModularResource::set_main_scene).bind(p_scene));
		his.undo_redo->add_do_method(callable_mp(res_picker, &EditorResourcePicker::set_edited_resource).bind(p_scene));
		his.undo_redo->add_undo_method(callable_mp(this, &ModularResource::set_main_scene).bind(main_scene));
		his.undo_redo->add_undo_method(callable_mp(res_picker, &EditorResourcePicker::set_edited_resource).bind(main_scene));
		interface->commit_undo_redo(his);
	}
}

// ModularPCK

void ModularPackedFile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_cache_dir"), &ModularPackedFile::get_cache_dir);
	ClassDB::bind_method(D_METHOD("set_file_path", "file_path"), &ModularPackedFile::_set_file_path);
	ClassDB::bind_method(D_METHOD("get_file_path"), &ModularPackedFile::get_save_path);
	ClassDB::bind_method(D_METHOD("queue_download_file"), &ModularPackedFile::queue_download_file);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "file_path", PROPERTY_HINT_GLOBAL_FILE, "*.pck,*.zip"), "set_file_path", "get_file_path");

	GDVIRTUAL_BIND(_get_local_path)
}

void ModularPackedFile::_menu_button_pressed(PopupMenu *p_menu) {
	p_menu->clear();

	p_menu->add_item(STTR("Add Local File..."), LOCAL_FILE);
	p_menu->add_item(STTR("Add Remote File..."), REMOTE_FILE);
	p_menu->add_item(TTR("Clear"), CLEAR_FILE);
	if (file_path.begins_with("http")) {
		p_menu->add_separator();
		p_menu->add_item(TTR("Reload"), RELOAD_REMOTE_FILE);
	}

	p_menu->reset_size();
}

void ModularPackedFile::_set_file_path(const String &p_path) {
	auto relative = modular_get_relative_path(p_path);
	if (file_path != relative) {
		file_path = relative;
		String module_name = get_module_name();
		emit_signal("name_changed", module_name.is_empty() ? STTR("[Unnamed]") : module_name);
		_provider.unref();
	}
}

String ModularPackedFile::get_save_path() const {
	return file_path;
}

String ModularPackedFile::get_local_path() const {
	REQUIRE_TOOL_SCRIPT(this);
	String ret;
	if (const_cast<ModularPackedFile *>(this)->GDVIRTUAL_CALL(_get_local_path, ret)) {
		return ret;
	}

	if (file_path.begins_with("http")) {
		return PCK_CACHE_FILE(file_path);
	}
	return file_path;
}

String ModularPackedFile::get_cache_dir() const {
	return EDITOR_CACHE;
}

void ModularPackedFile::queue_download_file(const String &p_url) {
	auto interface = ModularEditorPlugin::find_interface();
	if (interface) {
		interface->queue_download_packed_file(p_url);
	}
}

void ModularPackedFile::validation() {
	REQUIRE_TOOL_SCRIPT(this);
	if (GDVIRTUAL_CALL(_validation)) {
		return;
	}
	String download_file = get_local_path();
	if (!FileAccess::exists(download_file) && file_path.begins_with("http")) {
		queue_download_file(file_path);
	}
}

String ModularPackedFile::get_module_name() const {
	REQUIRE_TOOL_SCRIPT(this);
	String ret;
	if (const_cast<ModularPackedFile *>(this)->GDVIRTUAL_CALL(_get_module_name, ret)) {
		return ret;
	}

	return file_path.get_file().get_basename();
}

Ref<FileProvider> ModularPackedFile::get_provider(bool *r_is_new) {
	String path = get_local_path();

	bool packed_file_changed = false;
	if (_provider.is_valid()) {
		if (_provider->get_source() != path) {
			packed_file_changed = true;
		}
	}

	if (!IS_EMPTY(path)) {
		_provider = ModularOverrideUtils::get_or_create_packed_file_provider_with_caching(path, r_is_new);
	}

	if (r_is_new && packed_file_changed) {
		*r_is_new = true;
	}

	return _provider;
}

void ModularPackedFile::edit_data(Object *p_context) {
	REQUIRE_TOOL_SCRIPT(this);
	if (GDVIRTUAL_CALL(_edit_data, p_context)) {
		return;
	}

	if (file_path.begins_with("http")) {
	} else if (!IS_EMPTY(file_path)) {
		auto modular_intreface = ModularEditorPlugin::find_interface();
		if (modular_intreface) {
			modular_intreface->_add_packed_file_menu(LOCAL_FILE, Object::cast_to<GraphNode>(p_context));
		}
	}
}

static Array generate_anno_data_list(const Vector<Ref<Annotation>> &p_member_annotations) {
	Array anno_list;
	for (auto &member : p_member_annotations) {
		Array anno_data;
		Dictionary desc_dict;
		anno_data.push_back(member->get_display_name().strip_edges()); // Name
		anno_data.push_back(member->get_comment().strip_edges()); // Comment
		anno_data.push_back(desc_dict); // Desc
		anno_data.push_back(generate_anno_data_list(member->get_member_annotations())); // Submember

		desc_dict["type"] = "";
		if (auto method_anno = Object::cast_to<MethodAnnotation>(member.ptr())) {
			desc_dict["type"] = "Method";
			desc_dict["has_args"] = true;
			desc_dict["var_type"] = method_anno->get_return_value()->get_type();
			if (!method_anno->get_return_value()->get_class_name().is_empty()) {
				desc_dict["class_name"] = method_anno->get_return_value()->get_class_name();
			}
			desc_dict["static"] = method_anno->is_static();
		} else if (member->is_class_ptr(SignalAnnotation::get_class_ptr_static())) {
			desc_dict["type"] = "Signal";
			desc_dict["has_args"] = true;
		} else if (member->is_class_ptr(ConstantAnnotation::get_class_ptr_static())) {
			desc_dict["type"] = "Constant";
		} else if (member->is_class_ptr(EnumAnnotation::get_class_ptr_static())) {
			desc_dict["type"] = "Enum";
		} else if (member->is_class_ptr(EnumElementAnnotation::get_class_ptr_static())) {
			desc_dict["type"] = "EnumElement";
		} else if (member->is_class_ptr(ClassAnnotation::get_class_ptr_static())) {
			desc_dict["type"] = "Class";
		} else if (member->is_class_ptr(PropertyAnnotation::get_class_ptr_static())) {
			desc_dict["type"] = "Property";
		}

		if (auto variable_anno = Object::cast_to<VariableAnnotation>(member.ptr())) {
			desc_dict["var_type"] = variable_anno->get_type();
			if (!variable_anno->get_class_name().is_empty()) {
				desc_dict["class_name"] = variable_anno->get_class_name();
			}
		}

		if (auto param_anno = Object::cast_to<ParameterAnnotation>(member.ptr())) {
			if (!param_anno->get_default_value_text().is_empty()) {
				desc_dict["var_value"] = param_anno->get_default_value_text();
			}
		} else if (auto param_anno = Object::cast_to<ConstantAnnotation>(member.ptr())) {
			if (!param_anno->get_value_text().is_empty()) {
				desc_dict["var_value"] = param_anno->get_value_text();
			}
		}

		if (auto param_anno = Object::cast_to<PropertyAnnotation>(member.ptr())) {
			if (!param_anno->get_default_value_text().is_empty()) {
				desc_dict["readonly"] = param_anno->is_readonly();
			}
		}

		anno_list.push_back(anno_data);
	}
	return anno_list;
}

String ModularPackedFile::export_data() {
	REQUIRE_TOOL_SCRIPT(this);
	String ret;
	if (GDVIRTUAL_CALL(_export_data, ret))
		return ret;

	return file_path;
}

String ModularPackedFile::get_export_name() {
	auto ret = ModularData::get_export_name();
	if (IS_EMPTY(ret)) {
		if (file_path.is_relative_path()) {
			String resource_path = ProjectSettings::get_singleton()->get_resource_path();
			return PATH_JOIN(resource_path, file_path).simplify_path();
		}
		return file_path;
	}
	return ret;
}

Dictionary ModularPackedFile::get_module_info() {
	REQUIRE_TOOL_SCRIPT(this);
	Dictionary ret;
	if (GDVIRTUAL_CALL(_get_module_info, ret)) {
		return ret;
	}

	bool is_new;
	auto provider = get_provider(&is_new);
	if (is_new)
		module_info.clear();

	if (provider.is_valid()) {
		{
			Array mod;
			mod.push_back(STTR("Text, Localization"));
			mod.push_back(STTR("Visual texts and localizations."));
			module_info[TEXT_MODULE] = mod;
		}

		{
			Array mod;
			mod.push_back(STTR("Configuration, Serialization"));
			mod.push_back(STTR("Program Datas."));
			module_info[CONF_MODULE] = mod;
		}

		auto local_path = get_local_path();

		if (provider->is_class_ptr(FileProviderPack::get_class_ptr_static())) {
			if (is_new || !module_info_loaded) {
				PackedStringArray annotated_resources;
				// Load explict exposed resources first.
				auto mod_annos = ModuleAnnotation::generate_module_annotations(provider);
				for (auto &mod_anno : mod_annos) {
					Array mod;
					auto menber_annos = mod_anno->get_member_annotations();
					mod.push_back(mod_anno->get_display_name().strip_edges());
					mod.push_back(mod_anno->get_comment().strip_edges());
					mod.push_back(Dictionary());
					mod.push_back(generate_anno_data_list(menber_annos));

					mod.push_back(local_path);
					module_info[mod_anno->get_display_name()] = mod;

					if (!mod_anno->get_path().is_empty() && !annotated_resources.has(mod_anno->get_path())) {
						annotated_resources.append(mod_anno->get_path());
					}
					for (auto &&menber_annos : menber_annos) {
						if (!menber_annos->get_path().is_empty() && !annotated_resources.has(menber_annos->get_path())) {
							annotated_resources.append(menber_annos->get_path());
						}
						if (auto anno = cast_to<BaseResourceAnnotation>(menber_annos.ptr())) {
							if (!annotated_resources.has(anno->get_target_resource_path())) {
								annotated_resources.push_back(anno->get_target_resource_path());
							}
						}
					}
				}
				// Load unannotated resources as single module.
				{
					Array unannotated_files;
					for (auto &&fp : provider->get_files()) {
						if (fp == AnnotationExportPlugin::annotation_json_path || fp == AnnotationExportPlugin::module_annotation_json_path) {
							continue;
						}
						if (fp.contains("/.godot/") || fp.contains("/.ANNOTATION/") ||
								fp.contains("/spike.plugins/") ||
								fp.ends_with(".remap")) {
							continue;
						}

						if (!annotated_resources.has(fp)) {
							Array file_desc;
							file_desc.push_back(fp);
							file_desc.push_back(fp);
							file_desc.push_back(Dictionary());
							file_desc.push_back(Array());
							unannotated_files.push_back(file_desc);
						}
					}

					if (unannotated_files.size() > 0) {
						Array mod;
						mod.push_back(STTR("Unannotated Files"));
						mod.push_back(STTR("Unannotated files."));
						mod.push_back(Dictionary());
						mod.push_back(unannotated_files);

						mod.push_back(local_path);
						module_info[UNANNO_MODULE] = mod;
					}
				}
				module_info_loaded = true;
			}
		} else {
		}
	}
	return module_info;
}

void ModularPackedFile::inspect_modular(Node *p_parent) {
	REQUIRE_TOOL_SCRIPT(this);
	if (GDVIRTUAL_CALL(_inspect_modular, p_parent)) {
		return;
	}

	HBoxContainer *hbox = memnew(HBoxContainer);
	p_parent->add_child(hbox);

	Label *file_lb = memnew(Label);
	hbox->add_child(file_lb);
	file_lb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	file_lb->set_clip_text(true);
	file_lb->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	file_lb->set_text(file_path);

	MenuButton *menu_btn = memnew(MenuButton);
	hbox->add_child(menu_btn);
	menu_btn->set_icon(EDITOR_THEME(icon, "arrow", "Tree"));
	menu_btn->connect("about_to_popup", callable_mp(this, &ModularPackedFile::_menu_button_pressed).bind(menu_btn->get_popup()));

	auto graph_node = find_parent_type<GraphNode>(p_parent);
	auto modular_intreface = ModularEditorPlugin::find_interface();
	menu_btn->get_popup()->connect("id_pressed", callable_mp(modular_intreface, &EditorModularInterface::_add_packed_file_menu).bind(graph_node));
}
