/**
 * modular_graph.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_graph.h"

#include "configuration/configuration_server.h"
#include "core/config/project_settings.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "filesystem_server/filesystem_server.h"
#include "filesystem_server/providers/file_provider_pack.h"
#include "modular_data.h"
#include "modular_system/modular_graph_loader.h"
#include "modular_system/modular_launcher.h"
#include "scene/resources/packed_scene.h"
#include "spike/version.h"
#include "translation/translation_patch_server.h"

#ifdef TOOLS_ENABLED
#include "editor/debugger/editor_debugger_node.h"
#include "editor/editor_log.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "modular_editor_plugin.h"
#include "modular_importer.h"
#include "modular_override_utils.h"
#include "spike/editor/spike_editor_node.h"
#endif

#define JSON_KEY_GRAPH_VER ".graph_version"
#define JSON_KEY_GRAPH_NODES "nodes"

extern String modular_get_relative_path(const String &p_source_path) {
	String relative_source;
	if (p_source_path.is_absolute_path()) {
		String res_dir = ProjectSettings::get_singleton()->get_resource_path() + "/";
		String global_source = ProjectSettings::get_singleton()->globalize_path(p_source_path);

		for (int i = 0;; ++i) {
			if (p_source_path.begins_with(res_dir + "/")) {
				relative_source = String("../").repeat(i - 1) + p_source_path.substr(res_dir.length() + 1);
				break;
			}
			auto base_dir = res_dir.get_base_dir();
			if (base_dir == res_dir) {
				relative_source = p_source_path;
				break;
			}
			res_dir = base_dir;
		}
	} else {
		relative_source = p_source_path;
	}
	return relative_source;
}

static String modular_get_absolute_path(const String &p_source_path) {
	if (p_source_path.is_relative_path()) {
		String resource_path = ProjectSettings::get_singleton()->get_resource_path();
		return PATH_JOIN(resource_path, p_source_path).simplify_path();
	}
	return p_source_path;
}

#pragma region ModularNode

bool ModularNode::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "id") {
		id = p_value;
	} else if (p_name == "res") {
		res = p_value;
	} else {
		return false;
	}
	return true;
}

bool ModularNode::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "id") {
		r_ret = id;
	} else if (p_name == "res") {
		r_ret = res;
	} else {
		return false;
	}
	return true;
}

void ModularNode::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::STRING, "id"));
	p_list->push_back(PropertyInfo(Variant::OBJECT, "res"));
}

void ModularNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_id"), &ModularNode::get_id);
	ClassDB::bind_method(D_METHOD("get_type"), &ModularNode::get_type);
	ClassDB::bind_method(D_METHOD("get_res"), &ModularNode::get_res);
}

#pragma endregion

#pragma region ModularOutputNode

bool ModularOutputNode::_set(const StringName &p_name, const Variant &p_value) {
	if (ModularNode::_set(p_name, p_value))
		return true;

	if (p_name == "out") {
		output = p_value;
	} else {
		return false;
	}
	return true;
}

bool ModularOutputNode::_get(const StringName &p_name, Variant &r_ret) const {
	if (ModularNode::_get(p_name, r_ret))
		return true;

	if (p_name == "out") {
		r_ret = output;
	} else {
		return false;
	}
	return true;
}

void ModularOutputNode::_get_property_list(List<PropertyInfo> *p_list) const {
	ModularNode::_get_property_list(p_list);
	p_list->push_back(PropertyInfo(Variant::OBJECT, "out"));
}

int ModularOutputNode::get_output_index(const String &p_name) const {
	return p_name == output ? 0 : -1;
}

int ModularOutputNode::get_output_count() const {
	return IS_EMPTY(output) ? 0 : 1;
}

int ModularOutputNode::add_output(const String &p_name) {
	output = p_name;
	return 0;
}

void ModularOutputNode::remove_output(const String &p_name) {
	if (p_name == output) {
		output = String();
	}
}

const String ModularOutputNode::get_output_name(int p_index) const {
	return p_index == 0 ? output : String();
}
#pragma endregion

#pragma region ModularCompositeNode

bool ModularCompositeNode::_set(const StringName &p_name, const Variant &p_value) {
	if (ModularNode::_set(p_name, p_value))
		return true;

	if (p_name == "in") {
		in_list = p_value;
	} else if (p_name == "out") {
		out_list = p_value;
	} else {
		return false;
	}
	return true;
}

bool ModularCompositeNode::_get(const StringName &p_name, Variant &r_ret) const {
	if (ModularNode::_get(p_name, r_ret))
		return true;

	if (p_name == "in") {
		r_ret = in_list;
	} else if (p_name == "out") {
		r_ret = out_list;
	} else {
		return false;
	}
	return true;
}

void ModularCompositeNode::_get_property_list(List<PropertyInfo> *p_list) const {
	ModularNode::_get_property_list(p_list);
	p_list->push_back(PropertyInfo(Variant::ARRAY, "in"));
	p_list->push_back(PropertyInfo(Variant::ARRAY, "out"));
}

int ModularCompositeNode::get_input_index(const String &p_name) const {
	return in_list.find(p_name);
}

int ModularCompositeNode::get_input_count() const {
	return in_list.size();
}

const String ModularCompositeNode::get_input_name(int p_index) const {
	return p_index >= 0 && p_index < in_list.size() ? in_list[p_index] : String();
}

int ModularCompositeNode::add_input(const String &p_name) {
	int n = in_list.size();
	in_list.push_back(p_name);
	return n;
}

void ModularCompositeNode::remove_input(const String &p_name) {
	in_list.erase(p_name);
}

int ModularCompositeNode::get_output_index(const String &p_name) const {
	return out_list.find(p_name);
}

int ModularCompositeNode::get_output_count() const {
	return out_list.size();
}

const String ModularCompositeNode::get_output_name(int p_index) const {
	return p_index >= 0 && p_index < out_list.size() ? out_list[p_index] : String();
}

int ModularCompositeNode::add_output(const String &p_name) {
	int n = out_list.size();
	out_list.push_back(p_name);
	return n;
}

void ModularCompositeNode::remove_output(const String &p_name) {
	out_list.erase(p_name);
}
#pragma endregion

#pragma region ModularGraph

bool ModularGraph::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "nodes") {
		if (p_value.get_type() == Variant::ARRAY) {
			node_list.clear();
			Array arr = p_value;
			for (int i = 0; i < arr.size(); i++) {
				node_list.push_back(arr[i]);
			}
		}
	} else if (p_name == "connections") {
		if (p_value.get_type() == Variant::ARRAY) {
			Array arr = p_value;
			conn_list.clear();
			for (int i = 0; i < arr.size(); i++) {
				PackedStringArray strs = arr[i];
				_add_connection(strs[0], strs[1], strs[2]);
			}
		}
	} else if (p_name == "positions") {
		positions = p_value;
	} else {
		return false;
	}
	return true;
}

bool ModularGraph::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "nodes") {
		Array arr;
		for (int i = 0; i < node_list.size(); i++) {
			arr.push_back(node_list[i]);
		}
		r_ret = arr;
	} else if (p_name == "connections") {
		auto arr = Array();
		for (int i = 0; i < conn_list.size(); i++) {
			auto strs = PackedStringArray();
			auto conn = conn_list[i];
			strs.push_back(conn.from);
			strs.push_back(conn.to);
			strs.push_back(conn.port);
			arr.push_back(strs);
		}
		r_ret = arr;
	} else if (p_name == "positions") {
		r_ret = positions;
	} else {
		return false;
	}
	return true;
}

void ModularGraph::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::ARRAY, "nodes"));
	p_list->push_back(PropertyInfo(Variant::ARRAY, "connections"));
	p_list->push_back(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "positions"));
}

ModularGraph::Connection ModularGraph::_add_connection(ModularNodeID p_from, ModularNodeID p_to, const String &p_port) {
	auto conn = ModularGraph::Connection();
	conn.from = p_from;
	conn.to = p_to;
	conn.port = p_port;
	conn_list.push_back(conn);
	return conn;
}

String ModularGraph::_export_modular_node(const Ref<ModularNode> &p_node, const String &p_save_path) {
	String output_path = String();

	auto res = p_node->get_res();
	if (res.is_null() || exported_datas.has(res))
		return output_path;

	exported_datas.insert(res);
	String export_path = res->export_data();
	if (IS_EMPTY(export_path))
		return output_path;

	if (export_path.is_relative_path()) {
		String resource_path = ProjectSettings::get_singleton()->get_resource_path();
		export_path = PATH_JOIN(resource_path, export_path).simplify_path();
	}

	String export_name = res->get_export_name();
	if (export_path.begins_with("http")) {
		output_path = export_path;
	} else if (FileAccess::exists(export_path)) {
		String export_ext = export_path.get_extension();
		if (export_name.length() == 0) {
			output_path = p_node->get_id() + "." + export_ext;
		} else {
			if (export_name.get_extension().length() == 0) {
				export_name += "." + export_ext;
			}
			output_path = export_name;
		}
		if (output_path.begins_with("..")) {
			output_path = PATH_JOIN(p_save_path, output_path).simplify_path();
		}

		String dest_path = output_path;
		if (dest_path.is_relative_path()) {
			dest_path = PATH_JOIN(p_save_path, dest_path).simplify_path();
		}

		if (export_path != dest_path) {
			DirAccess::copy_absolute(export_path, dest_path);
		}
	} else if (DirAccess::exists(export_path)) {
		auto da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		output_path = IS_EMPTY(export_name) ? p_node->get_id() : export_name;
		if (output_path.begins_with("..")) {
			output_path = PATH_JOIN(p_save_path, output_path).simplify_path();
		}

		String dest_path = output_path;
		if (dest_path.is_relative_path()) {
			dest_path = PATH_JOIN(p_save_path, dest_path).simplify_path();
		}
		da->make_dir(dest_path);
		da->copy_dir(export_path, dest_path);
	}
	return output_path;
}

Error ModularGraph::_export_composite_node(const Ref<ModularCompositeNode> &p_node, const String &p_save_path, bool p_neasted) {
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_V(p_node.is_null(), ERR_INVALID_DATA);

	Error ret = OK;
	if (!DirAccess::exists(p_save_path)) {
		ret = DirAccess::make_dir_recursive_absolute(p_save_path);
		if (ret != OK) {
			ELog("Make dir '%s' for modular fail(#%d)", p_save_path, ret);
			return ret;
		}
	}
	Dictionary modular_json;
	modular_json[JSON_KEY_GRAPH_VER] = MODULAR_GRAPH_VERSION;
	Array modular_nodes;
	{
		String fname = _export_modular_node(p_node, p_save_path);
		ERR_FAIL_COND_V(IS_EMPTY(fname), FAILED);

		Dictionary node_inf;
		String path = fname;
		if (!path.is_absolute_path()) {
			path = PATH_JOIN(p_save_path, path);
		}
		if (!DirAccess::exists(path)) {
			node_inf["engine"] = VERSION_SPIKE_HEX;
		}
		node_inf["source"] = fname;
		modular_nodes.push_back(node_inf);
	}

	List<Ref<ModularNode>> nodes;
	for (auto &conn : conn_list) {
		if (conn.to == p_node->get_id()) {
			nodes.push_back(find_node(conn.from));
		}
	}

	for (auto &node : nodes) {
		Ref<ModularCompositeNode> composite_node = node;
		if (composite_node.is_valid()) {
			_export_composite_node(composite_node, PATH_JOIN(p_save_path, composite_node->get_id()), true);
			Dictionary node_inf;
			node_inf["source"] = composite_node->get_id();
			modular_nodes.push_back(node_inf);
		}

		String fname = _export_modular_node(node, p_save_path);
		if (!IS_EMPTY(fname)) {
			Dictionary node_inf;
			String path = fname;
			if (!path.is_absolute_path()) {
				path = PATH_JOIN(p_save_path, path);
			}
			if (!DirAccess::exists(path)) {
				node_inf["engine"] = VERSION_SPIKE_HEX;
			}
			node_inf["source"] = fname;
			auto first_out = node->get_output_name(0);
			if (first_out == TEXT_MODULE) {
				node_inf["type"] = "tr";
			} else if (first_out == CONF_MODULE) {
				node_inf["type"] = "conf";
			}
			modular_nodes.push_back(node_inf);
		}
	}
	modular_json[JSON_KEY_GRAPH_NODES] = modular_nodes;

	{
		auto fa = FileAccess::open(PATH_JOIN(p_save_path, MODULAR_LAUNCH), FileAccess::WRITE);
		if (fa.is_valid()) {
			fa->store_string(JSON::stringify(modular_json));
		}
	}

	return ret;
#else
	return FAILED;
#endif
}

void ModularGraph::_import_button_pressed(Node *p_parent) {
#ifdef TOOLS_ENABLED
	auto interface = ModularEditorPlugin::find_interface();
	if (interface) {
		interface->popup_import_menu(p_parent);
	}
#endif
}

void ModularGraph::_import_modular_json(const String &p_path, Node *p_parent) {
	reimport_from_uri(p_path, callable_mp(this, &ModularGraph::_modular_loaded_to_node).bind(p_parent));
}

void ModularGraph::_modular_loaded_to_node(Ref<ModularGraph> p_modular, Node *p_parent) {
	auto node = find_parent_type<GraphNode>(p_parent);
	auto modular_intreface = find_parent_type<EditorModularInterface>(node);
	if (modular_intreface) {
		modular_intreface->change_graph_node_resource(p_modular, node);
	}
}

void ModularGraph::_merge_module_info(Dictionary &p_dic, Ref<ModularNode> p_node) {
	if (p_node.is_null())
		return;
	auto res = p_node->get_res();

	if (res.is_null())
		return;

	p_dic.merge(res->get_module_info());

	for (auto &conn : conn_list) {
		if (conn.to == p_node->get_id()) {
			_merge_module_info(p_dic, find_node(conn.from));
		}
	}
}

Dictionary ModularGraph::_get_connection(int p_index) {
	Dictionary dic;
	auto conn = get_connection(p_index);
	dic["from"] = conn.from;
	dic["to"] = conn.to;
	dic["port"] = conn.port;
	return dic;
}

void ModularGraph::_add_connection_bind(ModularNodeID p_from, ModularNodeID p_to, const String p_name) {
	add_connection(p_from, p_to, p_name);
}

void ModularGraph::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_connection", "index"), &ModularGraph::_get_connection);
	ClassDB::bind_method(D_METHOD("get_connection_count"), &ModularGraph::get_connection_count);
	ClassDB::bind_method(D_METHOD("add_connection", "from", "to", "port"), &ModularGraph::_add_connection_bind);
	ClassDB::bind_method(D_METHOD("remove_connection", "from", "to"), &ModularGraph::remove_connection);
	ClassDB::bind_method(D_METHOD("get_node", "index"), &ModularGraph::get_node);
	ClassDB::bind_method(D_METHOD("get_node_count"), &ModularGraph::get_node_count);
	ClassDB::bind_method(D_METHOD("add_node", "node"), &ModularGraph::add_node);
	ClassDB::bind_method(D_METHOD("remove_node", "id"), &ModularGraph::remove_node);
	ClassDB::bind_method(D_METHOD("reimport_from_uri", "uri", "callback"), &ModularGraph::reimport_from_uri);
}

Ref<ModularNode> ModularGraph::find_node(ModularNodeID p_id) const {
	for (int i = 0; i < node_list.size(); i++) {
		if (node_list[i]->get_id() == p_id) {
			return node_list[i];
		}
	}
	return Ref<ModularNode>();
}

Ref<ModularNode> ModularGraph::find_main_node() const {
	Ref<ModularNode> main_node;
	for (const auto &node : node_list) {
		if (node->get_type() == ModularNode::MODULAR_COMPOSITE_NODE) {
			bool is_main_composite = true;
			for (const auto &conn : conn_list) {
				if (conn.from == node->get_id()) {
					is_main_composite = false;
					break;
				}
			}
			if (is_main_composite) {
				main_node = node;
				break;
			}
		}
	}
	return main_node;
}

Ref<ModularNode> ModularGraph::get_node(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, node_list.size(), Ref<ModularNode>());
	return node_list[p_index];
}

void ModularGraph::add_node(const Ref<ModularNode> &p_node) {
	ERR_FAIL_COND(p_node.is_null());
	ERR_FAIL_COND_MSG(node_list.find(p_node) >= 0, "ModularNode is already added.");

	node_list.push_back(p_node);
	positions.push_back(Vector2());
}

void ModularGraph::remove_node(ModularNodeID p_id) {
	for (int i = 0; i < node_list.size(); i++) {
		if (node_list[i]->get_id() == p_id) {
			node_list.remove_at(i);
			positions.remove_at(i);
			break;
		}
	}
	for (int i = conn_list.size() - 1; i >= 0; i--) {
		if (conn_list[i].from == p_id || conn_list[i].to == p_id) {
			conn_list.remove_at(i);
		}
	}
}

void ModularGraph::change_node_id(Ref<ModularNode> p_node, ModularNodeID p_id) {
	for (auto &conn : conn_list) {
		if (conn.from == p_node->get_id()) {
			conn.from = p_id;
		}
		if (conn.to == p_node->get_id()) {
			conn.to = p_id;
		}
	}
	p_node->id = p_id;
}

ModularGraph::Connection ModularGraph::get_connection(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, conn_list.size(), ModularGraph::Connection());
	return conn_list[p_index];
}

ModularGraph::Connection ModularGraph::add_connection(ModularNodeID p_from, ModularNodeID p_to, const String &p_port) {
	for (int i = 0; i < conn_list.size(); i++) {
		auto conn = conn_list[i];
		if (conn.from == p_from && conn.to == p_to) {
			conn.port = p_port;
			conn_list.set(i, conn);
			return conn;
		}
	}

	return _add_connection(p_from, p_to, p_port);
}

void ModularGraph::remove_connection(ModularNodeID p_from, ModularNodeID p_to) {
	for (int i = 0; i < conn_list.size(); i++) {
		auto conn = conn_list[i];
		if (conn.from == p_from && conn.to == p_to) {
			conn_list.remove_at(i);
			return;
		}
	}
}

void ModularGraph::set_node_pos(ModularNodeID p_id, Vector2 p_pos) {
	for (int i = 0; i < node_list.size(); i++) {
		if (node_list[i]->get_id() == p_id) {
			positions.set(i, p_pos / EDSCALE);
			break;
		}
	}
}

Vector2 ModularGraph::find_node_pos(ModularNodeID p_id) {
	for (int i = 0; i < node_list.size(); i++) {
		if (node_list[i]->get_id() == p_id) {
			return get_node_pos(i);
		}
	}
	return Vector2();
}

Vector2 ModularGraph::get_node_pos(int p_index) {
	if (p_index >= 0 && p_index < positions.size()) {
		return positions[p_index] * EDSCALE;
	}
	return Vector2();
}

void ModularGraph::move_all_nodes(const Vector2 &p_offset) {
	for (int i = 0; i < positions.size(); ++i) {
		positions.set(i, positions.get(i) + p_offset);
	}
}

Error ModularGraph::run_modular(const Ref<ModularNode> &p_node) {
#ifdef TOOLS_ENABLED
	ERR_FAIL_COND_V(p_node == nullptr || !p_node->is_class_ptr(ModularCompositeNode::get_class_ptr_static()), ERR_INVALID_DATA);

	String scene_path;
	Ref<ModularData> mres = p_node->get_res();
	Ref<ModularResource> rres = mres;
	if (rres.is_valid()) {
		// Find *.tscn to launch
		auto main_scene = rres->get_main_scene();
		if (main_scene.is_valid()) {
			scene_path = main_scene->get_path();
		}
		ERR_FAIL_COND_V(IS_EMPTY(scene_path), ERR_FILE_NOT_FOUND);

	} else {
		Ref<ModularPackedFile> res = mres;
		ERR_FAIL_COND_V(res.is_null(), ERR_FILE_NOT_FOUND);

		auto provider = res->get_provider();
		ERR_FAIL_COND_V(provider.is_null(), FAILED);

		Ref<ProjectEnvironment> env;
		REF_INSTANTIATE(env);
		env->parse_provider(provider);

		scene_path = env->get_project_setting("application/run/main_scene", String());
	}

	String conf_path = PATH_JOIN(EDITOR_CACHE, "_running_modular.json");
	{
		auto running_conf = generate_running_conf(p_node);
		auto fa = FileAccess::open(conf_path, FileAccess::WRITE);
		fa->store_string(JSON::stringify(running_conf));
	}
	//String conf_path = get_path();

	const String raw_custom_args = GLOBAL_GET("editor/run/main_run_args");
	PackedStringArray new_custom_args;
	new_custom_args.push_back(vformat(LAUNCH_MODULAR_PRE "%s#%s#%s", conf_path, p_node->get_id(), scene_path).replace(" ", "%20"));
	ProjectSettings::get_singleton()->set("editor/run/main_run_args", String(" ").join(new_custom_args));

	auto editor_node = Object::cast_to<SpikeEditorNode>(EditorNode::get_singleton());
	if (bool(EDITOR_GET("run/output/always_clear_output_on_play"))) {
		editor_node->get_log()->clear();
	}

	if (bool(EDITOR_GET("run/output/always_open_output_on_play"))) {
		editor_node->make_bottom_panel_item_visible(editor_node->get_log());
	}

	EditorDebuggerNode::get_singleton()->start();
	Error ret = editor_node->get_editor_run()->run(DEFAULT_LAUNCHER_PATH);
	if (OK != ret) {
		EditorDebuggerNode::get_singleton()->stop();
		ELog("Could not run modular.");
	}

	ProjectSettings::get_singleton()->set("editor/run/main_run_args", raw_custom_args);
	return ret;
#else
	return ERR_FAIL;
#endif
}

Error ModularGraph::export_modular(const Ref<ModularNode> &p_node, const String &p_save_path) {
	ERR_FAIL_COND_V(p_node.is_null(), ERR_INVALID_DATA);

	exported_datas.clear();
	Error ret = _export_composite_node(p_node, p_save_path, false);
	exported_datas.clear();
	return ret;
}

void ModularGraph::validation() {
	REQUIRE_TOOL_SCRIPT(this);
	GDVIRTUAL_CALL(_validation);

	for (auto &node : node_list) {
		auto res = node->get_res();
		if (res.is_valid()) {
			res->validation();
		}
	}
}

String ModularGraph::get_module_name() const {
	auto node = find_main_node();
	if (node.is_valid()) {
		auto res = node->get_res();
		if (res.is_valid() && !res->is_class_ptr(get_class_ptr_static())) {
			String name = res->get_module_name();
			if (!IS_EMPTY(name)) {
				return name;
			}
		}
	}

	String path = get_path();
	if (!IS_EMPTY(path)) {
		int index = path.find("::");
		if (index >= 0) {
			return path.substr(index + 2);
		}
		return path.get_file().get_basename();
	}

	return String();
}

void ModularGraph::edit_data(Object *p_context) {
#ifdef TOOLS_ENABLED
	EditorNode::get_singleton()->edit_resource(this);
#endif
}

String ModularGraph::export_data() {
#ifdef TOOLS_ENABLED
	auto main_node = find_main_node();
	if (main_node.is_valid()) {
		String export_dir = PATH_JOIN(EDITOR_CACHE, "_modular_graph");
		if (DirAccess::exists(export_dir)) {
			OS::get_singleton()->move_to_trash(export_dir);
		}
		exported_datas.clear();
		Error ret = _export_composite_node(main_node, export_dir, true);
		exported_datas.clear();
		return export_dir;
	}
	WLog("Cannot export: Main ModularNode NOT FOUND (%s)", get_path());
#endif
	return String();
}

Dictionary ModularGraph::get_module_info() {
	Dictionary info;
	auto main_node = find_main_node();
	if (main_node.is_valid()) {
		_merge_module_info(info, main_node);
	}
	return info;
}

void ModularGraph::inspect_modular(Node *p_parent) {
	HBoxContainer *hbox = memnew(HBoxContainer);
	p_parent->add_child(hbox);

	Label *label = memnew(Label);
	hbox->add_child(label);
	label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	label->set_text(STTR("Import"));

	Button *import_btn = memnew(Button);
	hbox->add_child(import_btn);
	import_btn->set_flat(true);
	import_btn->set_icon(EDITOR_THEME(icon, "arrow", "Tree"));
	import_btn->connect("pressed", callable_mp(this, &ModularGraph::_import_button_pressed).bind(p_parent));
}

void ModularGraph::reimport_from_uri(const String &p_uri, const Callable &p_callback) {
	node_list.clear();
	conn_list.clear();
	positions.clear();

	auto handler = memnew(ImportGraphHandler(this, p_callback));
	ModularGraphLoader::get_singleton()->queue_load(p_uri, handler);
}

#ifdef TOOLS_ENABLED
Dictionary ModularGraph::generate_running_conf(Ref<ModularNode> p_node) {
	if (p_node.is_null()) {
		p_node = find_main_node();
	}
	ERR_FAIL_COND_V(p_node.is_null(), Dictionary());

	List<Ref<ModularNode>> nodes;
	nodes.push_back(p_node);
	for (auto &conn : conn_list) {
		if (conn.to == p_node->get_id()) {
			nodes.push_back(find_node(conn.from));
		}
	}

	Dictionary conf;
	Dictionary pck_dic;
	Array tr_arr;
	Array conf_arr;
	Array graph_arr;
	Array remap;
	for (const auto &node : nodes) {
		if (node != p_node && node->get_type() == ModularNode::MODULAR_COMPOSITE_NODE) {
			graph_arr.push_back(generate_running_conf(node));
			continue;
		}
		String out = node->get_output_name(0);

		Ref<ModularData> data_res = node->get_res();

		Ref<ModularGraph> res_graph = data_res;
		if (res_graph.is_valid()) {
			graph_arr.push_back(res_graph->generate_running_conf(Ref<ModularNode>()));
			continue;
		}

		Ref<ModularPackedFile> pck_res = data_res;
		if (pck_res.is_valid()) {
			auto pck_file = pck_res->get_local_path();
			if (!IS_EMPTY(pck_file)) {
				if (out == CONF_MODULE) {
					conf_arr.push_back(modular_get_absolute_path(pck_file));
				} else if (out == TEXT_MODULE) {
					tr_arr.push_back(modular_get_absolute_path(pck_file));
				} else {
					pck_dic[node->get_id()] = modular_get_absolute_path(pck_file);
				}
			}
			continue;
		}

		Ref<ModularResource> res_res = data_res;
		if (res_res.is_valid()) {
			Array res_arr = res_res->get_resources();
			if (out == CONF_MODULE) {
				for (int i = 0; i < res_arr.size(); ++i) {
					const String path = res_arr[i];
					if (!ResourceLoader::exists(path, ConfigurationResource::get_class_static())) {
						continue;
					}
					Ref<ConfigurationResource> conf_res = ResourceLoader::load(path);
					conf_arr.push_back(conf_res->get_path());
				}
			} else if (out == TEXT_MODULE) {
				for (int i = 0; i < res_arr.size(); ++i) {
					const String path = res_arr[i];
					if (!ResourceLoader::exists(path, Translation::get_class_static())) {
						continue;
					}
					Ref<Translation> tr_res = ResourceLoader::load(path);
					tr_arr.push_back(tr_res->get_path());
				}
			} else {
				HashSet<String> remap_dirs;
				for (int i = 0; i < res_arr.size(); ++i) {
					const String path = res_arr[i];
					if (!ResourceLoader::exists(path)) {
						continue;
					}
					Ref<Resource> res = ResourceLoader::load(path);
					String dir = res->get_path().get_base_dir();
					if (FileAccess::exists(PATH_JOIN(dir, MODULAR_REMAP_FILES))) {
						remap_dirs.insert(dir);
					}
				}

				for (auto &dir : remap_dirs) {
					remap.push_back(dir);
				}
			}
			continue;
		}
	}

	conf["pck"] = pck_dic;
	conf["tr"] = tr_arr;
	conf["conf"] = conf_arr;
	conf["graph"] = graph_arr;
	conf["remap"] = remap;

	return conf;
}

void ModularGraph::apply_running_conf(const Dictionary &p_conf, const String &p_main_node) {
	Dictionary pck_dic = p_conf["pck"];
	Array tr_arr = p_conf["tr"];
	Array conf_arr = p_conf["conf"];
	Array graph_arr = p_conf["graph"];

	for (const Variant *key = pck_dic.next(nullptr); key; key = pck_dic.next(key)) {
		String node_name = *key;
		String pck_file = pck_dic[node_name];
		auto provider = FileProviderPack::create(pck_file);
		DLog("Load modular pack: '%s' %s.", pck_file, provider.is_valid() ? "success" : "failed");
		if (provider.is_valid()) {
			FileSystemServer::get_singleton()->add_provider(provider);
			Ref<ProjectEnvironment> env = provider->get_project_environment();
			if (node_name == p_main_node) {
				// Override ProjectSettings.
				Dictionary settigns = env->get_project_settings();
				Array keys = settigns.keys();
				for (int i = 0; i < keys.size(); ++i) {
					String key = keys[i];
					if (key.begins_with("input/")) {
						// Override Input Mapping.
						Dictionary action = settigns.get_valid(key);
						float deadzone = action.has("deadzone") ? (float)action["deadzone"] : 0.5f;
						String name = key.substr(key.find("/") + 1);
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
		}
	}

	for (int i = 0; i < conf_arr.size(); ++i) {
		ConfigurationServer::add_patch(ResourceLoader::load(conf_arr[i]));
		DLog("Configuration patch added: %s", conf_arr[i]);
	}

	for (int i = 0; i < tr_arr.size(); ++i) {
		TranslationPatchServer::add_patch(tr_arr[i], false);
		DLog("Translation loaded: %s", tr_arr[i]);
	}

	for (int i = 0; i < graph_arr.size(); ++i) {
		apply_running_conf(graph_arr[i], String());
	}
}

void ModularGraph::init_packed_file_providers() {
	if (!inited) {
		for (auto mnode : node_list) {
			if (auto subgraph = cast_to<ModularGraph>(mnode->get_res().ptr())) {
				subgraph->init_packed_file_providers();
			} else if (auto pack = cast_to<ModularPackedFile>(mnode->get_res().ptr())) {
				pack->get_provider();
			}
		}
		inited = true;
	}
}
#endif

#pragma endregion
