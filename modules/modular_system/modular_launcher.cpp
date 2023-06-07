/**
 * modular_launcher.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_launcher.h"
#include "configuration/configuration_resource.h"
#include "configuration/configuration_server.h"
#include "core/input/input_map.h"
#include "core/io/config_file.h"
#include "core/io/file_access_memory.h"
#include "core/io/json.h"
#include "modular_graph_loader.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/scroll_container.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"
#include "spike_define.h"
#include "translation/translation_patch_server.h"
#ifdef TOOLS_ENABLED
#include "editor/modular_graph.h"
#include "editor/modular_override_utils.h"
#endif

#define DEFAULT_LAUNCHER_NAME "-->Default Launcher<--"
#define MODULAR_DOWNLOAD_TEMP_FILE "user://downloaded_modular_node.tmp"

typedef void (*NodeLoader)(const String &p_file, const String &p_type);
static NodeLoader node_loader = nullptr;

HashMap<StringName, ProjectSettings::AutoloadInfo> *ModularLauncher::autoloads = nullptr;

static void _collect_autoloads(Dictionary &p_settings) {
	for (const Variant *key = p_settings.next(nullptr); key; key = p_settings.next(key)) {
		String setting = *key;
		if (setting.begins_with("autoload/")) {
			String node_name = setting.split("/")[1];
			ProjectSettings::AutoloadInfo autoload;
			autoload.name = node_name;
			String path = p_settings[setting];
			if (path.begins_with("*")) {
				autoload.is_singleton = true;
				autoload.path = path.substr(1);
			} else {
				autoload.path = path;
			}
			ModularLauncher::autoloads->insert(node_name, autoload);
		}
	}
}

static void _merge_input_mapping(Dictionary &p_settings) {
	for (const Variant *key = p_settings.next(nullptr); key; key = p_settings.next(key)) {
		String setting = *key;
		if (setting.begins_with("input/")) {
			// Override Input Mapping.
			Dictionary action = p_settings[setting];
			float deadzone = action.has("deadzone") ? (float)action["deadzone"] : 0.5f;
			String name = setting.substr(setting.find("/") + 1);
			if (!InputMap::get_singleton()->has_action(name)) {
				InputMap::get_singleton()->add_action(name, deadzone);
			} else {
				InputMap::get_singleton()->action_set_deadzone(name, deadzone);
			}

			Array events = action["events"];
			for (int i = 0; i < events.size(); i++) {
				Ref<InputEvent> event = events[i];
				if (event.is_null()) {
					continue;
				}
				InputMap::get_singleton()->action_add_event(name, event);
			}
		}
	}
}

static bool _load_resource_pack(const String &p_path) {
	if (PackedData::get_singleton()->is_disabled()) {
		return false;
	}

	bool ok = PackedData::get_singleton()->add_pack(p_path, true, 0) == OK;

	if (!ok) {
		return false;
	}

	// Global classes.
	Ref<FileProviderPack> provider = FileProviderPack::create(p_path);
	String global_class_list_path = ProjectSettings::get_singleton()->get_global_class_list_path();
	if (!provider->has_file(global_class_list_path)) {
		if (global_class_list_path.begins_with("res://.godot")) {
			global_class_list_path = global_class_list_path.replace_first("res://.godot", "res://godot");
		} else {
			global_class_list_path = global_class_list_path.replace_first("res://godot", "res://.godot");
		}
	}

	if (provider->has_file(global_class_list_path)) {
		// We assume that there has no conflicts between all global classs and autoloads from different packed files.
		Ref<ConfigFile> cfg;
		cfg.instantiate();
		cfg->parse(provider->open(global_class_list_path, FileAccess::READ)->get_as_text());
		Array global_classes_from_provider = cfg->get_value("", "list", Array());

		List<StringName> existing_global_classes;
		ScriptServer::get_global_class_list(&existing_global_classes);

		for (auto i = 0; i < global_classes_from_provider.size(); ++i) {
			Dictionary c = global_classes_from_provider[i];
			if (existing_global_classes.find(c["class"].operator StringName())) {
				continue;
			}
			ScriptServer::add_global_class(c["class"], c["base"], c["language"], c["path"]);
			existing_global_classes.push_back(c["class"]);
		}
	}

	// Autoloads
	Dictionary settigns = provider->get_project_environment()->get_project_settings();
	_collect_autoloads(settigns);

	return true;
}

static void _load_modular_packed_conf(const String &p_file) {
	Ref<FileProviderPack> provider = memnew(FileProviderPack);
	bool ret = provider->load_pack(p_file);
	DLog("Load configuration pack: '%s' %s.", p_file, ret ? "success" : "failed");
	if (ret) {
		FileSystemServer::get_singleton()->push_current_provider(provider);
		for (auto &file : provider->get_project_environment()->get_resource_paths()) {
			auto res_type = provider->get_resource_type(file);
			if (!ClassDB::is_parent_class(res_type, ConfigurationResource::get_class_static()))
				continue;

			ConfigurationServer::add_patch(provider->load(file));
		}
		FileSystemServer::get_singleton()->pop_current_provider();
	}
}

static void _load_modular_packed_tr(const String &p_file) {
	Ref<FileProviderPack> provider = memnew(FileProviderPack);
	bool ret = provider->load_pack(p_file);
	DLog("Load translation pack: '%s' %s.", p_file, ret ? "success" : "failed");
	if (ret) {
		FileSystemServer::get_singleton()->push_current_provider(provider);
		for (auto &file : provider->get_project_environment()->get_resource_paths()) {
			auto res_type = provider->get_resource_type(file);
			if (!ClassDB::is_parent_class(res_type, Translation::get_class_static()))
				continue;

			TranslationPatchServer::add_patch(file);
		}
		FileSystemServer::get_singleton()->pop_current_provider();
	}
}

static void _load_modular_node(const String &p_file, const String &p_type) {
	if (p_file.ends_with(".pck") || p_file.ends_with(".zip")) {
		if (p_type == "conf") {
			_load_modular_packed_conf(p_file);
		} else if (p_type == "tr") {
			_load_modular_packed_tr(p_file);
		} else {
			bool ret = _load_resource_pack(p_file);
			DLog("Load modular pack: '%s' %s.", p_file, ret ? "success" : "failed");
		}
	}
}

#ifdef TOOLS_ENABLED
static void _load_modular_res(const String &p_file, const String &p_type) {
	if (p_file.ends_with(".pck") || p_file.ends_with(".zip")) {
		String main_pack_path = ProjectSettings::get_singleton()->get_setting("application/run/main_pack", String());
		if (ProjectSettings::get_singleton()->globalize_path(main_pack_path) == ProjectSettings::get_singleton()->globalize_path(p_file)) {
			return;
		}

		if (p_type == "conf") {
			_load_modular_packed_conf(p_file);
		} else if (p_type == "tr") {
			_load_modular_packed_tr(p_file);
		} else {
			Ref<FileProviderPack> provider = memnew(FileProviderPack);
			bool sucess = provider->load_pack(p_file);
			DLog("Load modular pack: '%s' %s.", p_file, sucess ? "success" : "failed");
			if (sucess) {
				FileSystemServer::get_singleton()->add_provider(provider);
				Ref<ProjectEnvironment> env = provider->get_project_environment();
				Dictionary settigns = env->get_project_settings();
				if (p_type == "main") {
					_merge_input_mapping(settigns);
				}
				_collect_autoloads(settigns);
			}
		}
	} else if (p_type == "conf") {
		ConfigurationServer::add_patch(ResourceLoader::load(p_file));
		DLog("Configuration patch added: %s", p_file);
	} else if (p_type == "tr") {
		TranslationPatchServer::add_patch(p_file, false);
		DLog("Translation loaded: %s", p_file);
	}
}

void ModularLauncher::_apply_running_conf(const Dictionary &p_conf, const String &p_main_node) {
	Dictionary pck_dic = p_conf["pck"];
	Array tr_arr = p_conf["tr"];
	Array conf_arr = p_conf["conf"];
	Array graph_arr = p_conf["graph"];
	Array remap = p_conf["remap"];

	for (const Variant *key = pck_dic.next(nullptr); key; key = pck_dic.next(key)) {
		String node_name = *key;
		String pck_file = pck_dic[node_name];
		add_modular_node(0, pck_file, node_name == p_main_node ? "main" : "");
	}

	for (int i = 0; i < conf_arr.size(); ++i) {
		add_modular_node(0, conf_arr[i], "conf");
	}

	for (int i = 0; i < tr_arr.size(); ++i) {
		add_modular_node(0, tr_arr[i], "tr");
	}

	for (int i = 0; i < graph_arr.size(); ++i) {
		_apply_running_conf(graph_arr[i], String());
	}

	for (int i = 0; i < remap.size(); ++i) {
		String dir = remap[i];
		auto conf = ModularOverrideUtils::get_loaded_remap_config(dir);
		List<String> sections;
		conf->get_sections(&sections);
		for (auto &file : sections) {
			String file_path = PATH_JOIN(dir, file);
			String origin_file = conf->get_value(file, MODULAR_REMAP_ORIGIN_FILE_KEY);
			String extend_file = conf->get_value(file, MODULAR_REMAP_EXTENDS_KEY);
			String pck_file = conf->get_value(file, MODULAR_REMAP_PACKED_FILE_KEY);
			//modular_get_absolute_path(pck_file);
			remap_provider->add_provider_remap(origin_file, default_provider, file_path);
			auto pck_provider = ModularOverrideUtils::get_or_create_packed_file_provider_with_caching(pck_file);
			remap_provider->add_provider_remap(extend_file, pck_provider, origin_file);
		}
	}
}

#endif

void ModularLauncher::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
#ifdef TOOLS_ENABLED
			if (Engine::get_singleton()->is_editor_hint()) {
				break;
			}
#endif
			if (get_script_instance() == nullptr) {
				if (get_name() == DEFAULT_LAUNCHER_NAME) {
					if (main_scene.get_file().get_basename().ends_with(".launcher")) {
						_launch();
						return;
					}
				}
				_default_init();
			}
			_load_modular_pack();
			set_process(true);
			break;
		case NOTIFICATION_PROCESS:
			if (node_list.size()) {
				if (current_node == nullptr) {
					current_node = node_list.front();
					_load_current_node();
				} else if (request && request->get_http_client_status() == HTTPClient::STATUS_BODY) {
					_loading_modular(current_node->get().source, request->get_downloaded_bytes(), request->get_body_size());
				}
			}
		case NOTIFICATION_PREDELETE:
			if (get_script_instance() == nullptr) {
				for (int i = 0; i < FileSystemServer::get_singleton()->get_provider_count(); ++i) {
					auto provider = FileSystemServer::get_singleton()->get_provider(i);
					if (provider->is_class_ptr(LauncherProvider::get_class_ptr_static())) {
						FileSystemServer::get_singleton()->remove_provider(provider);
						break;
					}
				}
			}
			break;
	}
}

bool ModularLauncher::_set(const StringName &p_name, const Variant &p_property) {
	if (p_name == "main_scene") {
		main_scene = p_property;
		return true;
	}
	return false;
}

bool ModularLauncher::_get(const StringName &p_name, Variant &r_property) const {
	if (p_name == "main_scene") {
		r_property = main_scene;
		return true;
	}
	return false;
}

void ModularLauncher::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::STRING, "main_scene"));
}

void ModularLauncher::_default_init() {
	auto scroll = memnew(ScrollContainer);
	add_child(scroll);
	scroll->set_anchors_preset(Control::PRESET_FULL_RECT);

	auto vbox = memnew(VBoxContainer);
	scroll->add_child(vbox);
	vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	auto label = memnew(Label);
	vbox->add_child(label);
	label->set_visible(false);

	preset_control = label;
}

void ModularLauncher::_load_modular_pack() {
	node_list.clear();
	current_node = nullptr;

#ifdef TOOLS_ENABLED
	auto main_args = OS::get_singleton()->get_cmdline_args();
	String preffix = LAUNCH_MODULAR_PRE;
	for (auto const &arg : main_args) {
		if (arg.begins_with(preffix)) {
			String modular_file = arg.substr(preffix.length());
			PackedStringArray modular_args = modular_file.split("#", false);
			if (modular_args.size() < 3)
				break;

			default_provider.instantiate();
			remap_provider.instantiate();
			FileSystemServer::get_singleton()->add_provider(remap_provider);
			String node_name = modular_args[1];
			Dictionary modular_conf = JSON::parse_string(FileAccess::open(modular_args[0], FileAccess::READ)->get_as_text());
			_apply_running_conf(modular_conf, node_name);
			node_loader = &_load_modular_res;
			if (node_list.size() == 0) {
				MessageQueue::get_singleton()->push_callable(callable_mp(this, &ModularLauncher::_modular_loaded));
			}
			return;
		}
	}
#endif

	String modular_json_file = RunGraphHandler::find_modular_json_path();
	if (!IS_EMPTY(modular_json_file)) {
		auto handler = memnew(RunGraphHandler(this));
		ModularGraphLoader::get_singleton()->queue_load(modular_json_file, handler);
	} else {
		_launch();
	}
	node_loader = &_load_modular_node;
}

void ModularLauncher::_load_current_node() {
	NodeData &data = current_node->get();
	String local_path = data.source;
	_loading_modular(data.source, 0, 1);
	if (data.source.begins_with("http")) {
		String ext = data.source.get_extension();
		String download_file = vformat("user://%s.%s", data.source.md5_text(), ext);
		if (!FileAccess::exists(download_file)) {
			if (request == nullptr) {
				request = memnew(HTTPRequest);
				add_child(request);
				request->connect("request_completed", callable_mp(this, &ModularLauncher::_download_completed));
			}

			request->set_download_file(download_file + ".tmp");
			request->request(data.source);
			return;
		}
		local_path = download_file;
	}

	_loading_modular(data.source, 1, 1);
	node_loader(local_path, data.type);
	_current_node_loaded();
}

void ModularLauncher::_download_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != OK) {
		return;
	}

	if (p_code != 200) {
		return;
	}

	_loading_modular(current_node->get().source, request->get_downloaded_bytes(), request->get_body_size());
	String download_file = request->get_download_file();
	String pck_file = download_file.get_basename();
	DirAccess::create_for_path(pck_file)->rename(download_file, pck_file);

	node_loader(pck_file, current_node->get().type);
	_current_node_loaded();
}

void ModularLauncher::_current_node_loaded() {
	current_node = nullptr;
	current_control = nullptr;
	node_list.pop_front();
	if (node_list.size() == 0) {
		_modular_loaded();
	}
}

void ModularLauncher::_loading_modular(const String &p_item, int p_step, int p_total) {
	if (GDVIRTUAL_CALL(_loading_modular, p_item, p_step, p_total))
		return;

	if (preset_control == nullptr)
		return;

	if (current_control == nullptr) {
		current_control = Object::cast_to<Label>(preset_control->duplicate());
		preset_control->get_parent()->add_child(current_control);
		current_control->set_visible(true);
	}

	auto label = Object::cast_to<Label>(current_control);
	label->set_text(vformat("%s %s (%d/%d)", "Loading...", p_item, p_step, p_total));
}

void ModularLauncher::_modular_loaded() {
	const String global_class_list_path = ProjectSettings::get_singleton()->get_global_class_list_path();
	if (FileSystemServer::get_singleton()->get_os_file_access()->file_exists(global_class_list_path)) {
		const String tmp_file = global_class_list_path.get_base_dir().path_join(".tmp_global_class_list_path");
		DirAccess::rename_absolute(global_class_list_path, tmp_file);
		ScriptServer::save_global_classes();
		DirAccess::rename_absolute(tmp_file, global_class_list_path);
	} else {
		ScriptServer::save_global_classes();
		if (DirAccess::exists(global_class_list_path)) {
			DirAccess::remove_absolute(global_class_list_path);
		}
	}

	_autoloads();

	if (GDVIRTUAL_CALL(_modular_loaded))
		return;

	if (preset_control == nullptr)
		return;

	_launch();
	// auto launch_btn = memnew(Button);
	// preset_label->get_parent()->add_child(launch_btn);
	// launch_btn->set_text("Launch Application");
	// launch_btn->connect("pressed", callable_mp(this, &ModularLauncher::_launch));
}

void ModularLauncher::_autoloads() {
	if (ModularLauncher::autoloads) {
		auto sml = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
		//autoload
		Engine::get_singleton()->startup_benchmark_begin_measure("load_autoloads");
		HashMap<StringName, ProjectSettings::AutoloadInfo> autoloads = *ModularLauncher::autoloads;

		for (auto &kv : autoloads) {
			ProjectSettings::get_singleton()->add_autoload(kv.value);
		}

		//first pass, add the constants so they exist before any script is loaded
		for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : autoloads) {
			const ProjectSettings::AutoloadInfo &info = E.value;

			if (info.is_singleton) {
				for (int i = 0; i < ScriptServer::get_language_count(); i++) {
					ScriptServer::get_language(i)->add_global_constant(info.name, Variant());
				}
			}
		}

		//second pass, load into global constants
		List<Node *> to_add;
		for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : autoloads) {
			const ProjectSettings::AutoloadInfo &info = E.value;

			Node *n = nullptr;
			if (ResourceLoader::get_resource_type(info.path) == "PackedScene") {
				// Cache the scene reference before loading it (for cyclic references)
				Ref<PackedScene> scn;
				scn.instantiate();
				scn->set_path(info.path);
				scn->reload_from_file();
				ERR_CONTINUE_MSG(!scn.is_valid(), vformat("Can't autoload: %s.", info.path));

				if (scn.is_valid()) {
					n = scn->instantiate();
				}
			} else {
				Ref<Resource> res = ResourceLoader::load(info.path);
				ERR_CONTINUE_MSG(res.is_null(), vformat("Can't autoload: %s.", info.path));

				Ref<Script> script_res = res;
				if (script_res.is_valid()) {
					StringName ibt = script_res->get_instance_base_type();
					bool valid_type = ClassDB::is_parent_class(ibt, "Node");
					ERR_CONTINUE_MSG(!valid_type, vformat("Script does not inherit from Node: %s.", info.path));

					Object *obj = ClassDB::instantiate(ibt);

					ERR_CONTINUE_MSG(!obj, vformat("Cannot instance script for autoload, expected 'Node' inheritance, got: %s."));

					n = Object::cast_to<Node>(obj);
					n->set_script(script_res);
				}
			}

			ERR_CONTINUE_MSG(!n, vformat("Path in autoload not a node or script: %s.", info.path));
			n->set_name(info.name);

			//defer so references are all valid on _ready()
			to_add.push_back(n);

			if (info.is_singleton) {
				for (int i = 0; i < ScriptServer::get_language_count(); i++) {
					ScriptServer::get_language(i)->add_global_constant(info.name, n);
				}
			}
		}

		for (Node *E : to_add) {
			sml->get_root()->add_child(E);
			DLog("Add autoload: %s", E->get_name());
		}
		Engine::get_singleton()->startup_benchmark_end_measure(); // load autoloads

		delete ModularLauncher::autoloads;
		ModularLauncher::autoloads = nullptr;
	}
}

void ModularLauncher::_launch() {
	SceneTree::get_singleton()->change_scene_to_file(main_scene);
}

void ModularLauncher::_bind_methods() {
	GDVIRTUAL_BIND(_loading_modular, "item", "step", "total");
	GDVIRTUAL_BIND(_modular_loaded);
}

void ModularLauncher::add_modular_node(uint32_t p_engine, const String &p_source, const String &p_type) {
	NodeData node_data;
	node_data.engine = p_engine;
	node_data.source = p_source;
	node_data.type = p_type;
	node_list.push_back(node_data);
}

RunGraphHandler::RunGraphHandler(ModularLauncher *p_launcher) {
	launcher = p_launcher;
}

Variant RunGraphHandler::add_node(int p_engine, const String &p_source, const String &p_type) {
	if (launcher) {
		launcher->add_modular_node(p_engine, p_source, p_type);
	} else {
		_load_modular_node(p_source, p_type);
	}
	return p_engine < 0 ? Variant() : Variant(p_source);
}

String RunGraphHandler::find_modular_json_path() {
	String exec_path = PATH_JOIN(OS::get_singleton()->get_executable_path().get_base_dir(), MODULAR_LAUNCH);
	if (FileAccess::exists(exec_path))
		return exec_path;

	String main_pack_path = ProjectSettings::get_singleton()->get_setting("application/run/main_pack", String());
	if (!main_pack_path.is_empty()) {
		String pack_path = PATH_JOIN(main_pack_path.get_base_dir(), MODULAR_LAUNCH);
		if (FileAccess::exists(pack_path))
			return pack_path;
	}

	String user_path = PATH_JOIN(OS::get_singleton()->get_user_data_dir(), MODULAR_LAUNCH);
	if (FileAccess::exists(user_path))
		return user_path;

	String res_path = String("res://") + MODULAR_LAUNCH;
	if (FileAccess::exists(res_path))
		return res_path;

	return String();
}

Ref<FileAccess> LauncherProvider::open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error) const {
	Ref<FileAccessMemory> launcher;
	if (p_path == DEFAULT_LAUNCHER_PATH) {
		REF_INSTANTIATE(launcher);
		Error err = launcher->open_custom(data.ptr(), data.size());
		if (r_error)
			*r_error = err;
	}
	return launcher;
};

PackedStringArray LauncherProvider::get_files() const {
	PackedStringArray files;
	files.push_back(DEFAULT_LAUNCHER_PATH);
	return files;
};

bool LauncherProvider::has_file(const String &p_name) const {
	return DEFAULT_LAUNCHER_PATH == p_name;
};

/*
[gd_scene format=3 uid="uid://0"]

[node name="ModularLauncher" type="ModularLauncher"]
main_scene = "res://test.tscn"
*/
LauncherProvider *LauncherProvider::create(const String &p_main_scene) {
	auto instance = memnew(LauncherProvider);
	String tscn = vformat("[gd_scene format=3]\n\n[node name=\"%s\" type=\"ModularLauncher\"]\nmain_scene = \"%s\"\n", DEFAULT_LAUNCHER_NAME, p_main_scene);
	instance->data = tscn.to_utf8_buffer();
	return instance;
}
