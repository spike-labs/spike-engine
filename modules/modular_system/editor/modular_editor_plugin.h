/**
 * modular_editor_plugin.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "spike_define.h"

#include "core/core_string_names.h"
#include "editor/editor_file_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "editor/editor_quick_open.h"
#include "editor/editor_resource_picker.h"
#include "editor/editor_undo_redo_manager.h"
#include "modular_definition_dialog.h"
#include "modular_graph.h"
#include "modular_port_dialog.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/item_list.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/separator.h"
#include "scene/main/http_request.h"

#define COMPOSITE_TITLE "Composite"
#define OUTPUT_TITLE "Output"

#define META_COMPSITE "composite"
#define META_PICKER "picker"
#define META_LANG_EDIT "lang_edit"
#define META_TGL_LANGS_BTN "tgl_langs_btn"
#define META_PLAY_BTN "play_btn"
#define META_EXPORT_BTN "export_btn"
#define META_IN "in"
#define META_OUT "out"

#define DLG_CALLBACK "callback"

#define GET_EXIST_MAPPING(mapping, exist_files)                                \
	if (mapping.is_valid()) {                                                  \
		List<String> sections;                                                 \
		mapping->get_sections(&sections);                                      \
		for (auto section : sections) {                                        \
			String file = mapping->get_value(section, MODULAR_REMAP_PATH_KEY); \
			if (!IS_EMPTY(file)) {                                             \
				exist_files.insert(file, section);                             \
			}                                                                  \
		}                                                                      \
	}

class EditorModularInterface : public PanelContainer {
	GDCLASS(EditorModularInterface, PanelContainer);

public:
	enum FileMenuOptions {
		FILE_NEW,
		FILE_OPEN,
		FILE_SAVE,
		FILE_SAVE_AS,
		FILE_SAVE_ALL,
		FILE_CLOSE,
		FILE_CLOSE_ALL,
		FILE_CLOSE_OTHER,
		SHOW_IN_FILESYSTEM,
		FILE_MAX
	};

	enum RightClickOptions {
		ADD,
		CUT,
		COPY,
		PASTE,
		DELETE,
		DUPLICATE,
		LOCATE
	};

	struct EditedModular {
		Ref<ModularGraph> modular;
		Ref<ModularGraph> editing;
		bool unsaved = false;
		int history_id = 0;
		List<ModularNodeID> neasted_nodes;
	};

	static String get_id(const GraphNode *p_node) { return p_node ? String(p_node->get_name()) : String(); }

	template <class T>
	static T *_get_sub_control(const GraphNode *p_node, const String &p_meta) {
		return p_node ? Object::cast_to<T>(p_node->get_meta(p_meta)) : nullptr;
	}

	static ItemList *_get_module_item_list(const GraphNode *p_node) {
		auto array = p_node->find_children("", ItemList::get_class_static(), true, false);
		if (array.size())
			return Object::cast_to<ItemList>(array[0]);

		return nullptr;
	}

	static bool _node_is_composite(GraphNode *p_node) {
		return p_node->has_meta(META_COMPSITE);
	}

	static void _set_node_input(GraphNode *p_node, int p_port, const String &p_input) {
		if (p_port < p_node->get_connection_input_count()) {
			int slot = p_node->get_connection_input_slot(p_port);
			p_node->get_child(slot)->set_meta(META_IN, p_input);
		}
	}

	static String _get_node_input(GraphNode *p_node, int p_port) {
		if (p_port < p_node->get_connection_input_count()) {
			int slot = p_node->get_connection_input_slot(p_port);
			return p_node->get_child(slot)->get_meta(META_IN);
		}
		return String();
	}

	static PackedStringArray _get_node_inputs(GraphNode *p_node) {
		PackedStringArray inputs;
		for (int i = 0; i < p_node->get_connection_input_count(); i++) {
			String input = _get_node_input(p_node, i);
			if (!IS_EMPTY(input)) {
				inputs.push_back(input);
			}
		}
		return inputs;
	}

	static void _set_node_output(GraphNode *p_node, int p_port, const String &p_output) {
		if (p_port < p_node->get_connection_output_count()) {
			int slot = p_node->get_connection_output_slot(p_port);
			p_node->get_child(slot)->set_meta(META_OUT, p_output);
		}
	}
	static String _get_node_output(GraphNode *p_node, int p_port) {
		if (p_port < p_node->get_connection_output_count()) {
			int slot = p_node->get_connection_output_slot(p_port);
			return p_node->get_child(slot)->get_meta(META_OUT);
		}
		return String();
	}

	static PackedStringArray _get_node_outputs(GraphNode *p_node) {
		PackedStringArray outputs;
		for (int i = 0; i < p_node->get_connection_output_count(); i++) {
			String output = _get_node_output(p_node, i);
			if (!IS_EMPTY(output)) {
				outputs.push_back(output);
			}
		}
		return outputs;
	}

	static void _on_node_res_changed(GraphNode *p_node, const Ref<ModularData> &p_res) {
		auto node = p_node->get_node_or_null(NodePath("Inspector"));
		if (node) {
			p_node->remove_child(node);
			node->queue_free();
		}
		auto sep = p_node->get_node_or_null(NodePath("-SEP-"));
		if (p_res.is_valid()) {
			if (sep == nullptr) {
				sep = memnew(HSeparator);
				p_node->add_child(sep);
				sep->set_name("-SEP-");
			}

			Ref<ModularData> res = p_res;
			node = memnew(VBoxContainer);
			p_node->add_child(node);
			node->set_name("Inspector");

			res->inspect_modular(node);
		} else if (sep != nullptr) {
			sep->queue_free();
		}
	}

	static Ref<ModularData> _get_node_res(const GraphNode *p_node) {
		auto picker = _get_sub_control<EditorResourcePicker>(p_node, META_PICKER);
		if (picker) {
			return picker->get_edited_resource();
		}
		return Ref<ModularData>();
	}

	static void _set_node_res(GraphNode *p_node, Ref<ModularData> p_res) {
		auto picker = _get_sub_control<EditorResourcePicker>(p_node, META_PICKER);
		if (picker) {
			auto prev_res = picker->get_edited_resource();
			EditorModularInterface *interface = find_parent_type<EditorModularInterface>(p_node);
			Callable callable = callable_mp(p_node, &GraphNode::set_title);
			if (prev_res.is_valid()) {
				if (prev_res->is_connected("name_changed", callable)) {
					prev_res->disconnect("name_changed", callable);
				}
			}
			picker->set_meta("prev_res", p_res);
			picker->set_edited_resource(p_res);
			if (p_res.is_valid()) {
				p_res->validation();
			}
			if (picker->is_editable()) {
				_on_node_res_changed(p_node, p_res);
			}

			String output = _get_node_output(p_node, 0);
			if (output.size() == 0 || output == ANY_MODULE) {
				String module_name;
				if (p_res.is_valid()) {
					p_res->connect("name_changed", callable);
					module_name = p_res->get_module_name();
				}
				if (IS_EMPTY(module_name)) {
					module_name = STTR("[Unnamed]");
				}
				p_node->set_title(module_name);
			}
		}
	}

	static Ref<ModularData> _get_node_prev_res(const GraphNode *p_node) {
		auto picker = _get_sub_control<EditorResourcePicker>(p_node, META_PICKER);
		if (picker && picker->has_meta("prev_res")) {
			return picker->get_meta("prev_res");
		}
		return Ref<ModularData>();
	}

	GraphNode *node_modular_to_graph(const Ref<ModularNode> &p_modular_node) {
		return find_graph_node(p_modular_node->get_id());
	}

	GraphNode *find_graph_node(const String &p_id) {
		Node *node = graph->find_child(p_id, false, false);
		return Object::cast_to<GraphNode>(node);
	}

	GraphNode *get_graph_node_from(GraphNode *p_node, const String &p_output = String()) {
		List<GraphEdit::Connection> conn_list;
		graph->get_connection_list(&conn_list);
		for (auto &conn : conn_list) {
			if (conn.from == p_node->get_name() && (p_output == String() || _get_node_output(p_node, conn.from_port) == p_output)) {
				if (auto node = find_graph_node(conn.to))
					return node;
			}
		}
		return nullptr;
	}

	Ref<ModularNode> node_graph_to_modular(const GraphNode *p_graph_node) {
		if (edited_index < 0 || edited_index >= (int)edited_modulars.size())
			return Ref<ModularNode>();
		auto &edited = edited_modulars[edited_index];
		auto current = _get_neasted_modular_graph(edited);
		ERR_FAIL_COND_V(current.is_null(), Ref<ModularNode>());
		return current->find_node(get_id(p_graph_node));
	}

	List<GraphNode *> get_graph_node_list() {
		List<GraphNode *> list;
		for (int i = 0; i < graph->get_child_count(); i++) {
			if (auto graph_node = Object::cast_to<GraphNode>(graph->get_child(i))) {
				list.push_back(graph_node);
			}
		}
		return list;
	}

	List<GraphNode *> get_selected_node_list() {
		List<GraphNode *> list;
		for (int i = 0; i < graph->get_child_count(); i++) {
			if (auto graph_node = Object::cast_to<GraphNode>(graph->get_child(i))) {
				if (graph_node->is_selected()) {
					list.push_back(graph_node);
				}
			}
		}
		return list;
	}

	bool _graph_has_connection_out(const String &p_name, int p_port) {
		List<GraphEdit::Connection> conn_list;
		graph->get_connection_list(&conn_list);
		for (auto &conn : conn_list) {
			if (conn.from_port == p_port && conn.from == p_name) {
				return true;
			}
		}
		return false;
	}

	bool _graph_has_connection_in(const String &p_name, int p_port) {
		List<GraphEdit::Connection> conn_list;
		graph->get_connection_list(&conn_list);
		for (auto &conn : conn_list) {
			if (conn.to_port == p_port && conn.to == p_name) {
				return true;
			}
		}
		return false;
	}

	void _update_modular_data(Ref<ModularData> p_res, const String &p_path, const String &p_name) {
		if (p_res.is_valid()) {
			if (p_res->_get_export_name() != p_name) {
				p_res->_set_export_name(p_name);
				if (!p_res->get_path().is_resource_file()) {
					_set_edited_modular_unsaved(edited_index, true);
				}
			}
		}
	}

private:
	int _node_index;

	// Define Undo Redo
	EditorUndoRedoManager::History _empty_history;
	Map<int, EditorUndoRedoManager::History> history_map;
	int _history_index = 0;
	EditorUndoRedoManager::History &get_or_creat_history(EditedModular *edited = nullptr) {
		if (nullptr == edited) {
			if (edited_index >= 0 && edited_index < (int)edited_modulars.size()) {
				edited = &edited_modulars[edited_index];
			}
		}
		if (nullptr == edited)
			return _empty_history;

		if (edited->history_id == 0) {
			edited->history_id = ++_history_index;
		}
		if (!history_map.has(edited->history_id)) {
			EditorUndoRedoManager::History history;
			history.undo_redo = memnew(UndoRedo);
			history.id = edited->history_id;
			history_map[edited->history_id] = history;
		}
		return history_map[edited->history_id];
	}

	void _add_graph_node_undo(int p_type, const PackedStringArray &p_inputs, const PackedStringArray &p_outputs, Vector2 p_offset) {
		_remove_graph_node(find_graph_node(itos(_node_index--)));
	}
	void _add_graph_node_redo(int p_type, const PackedStringArray &p_inputs, const PackedStringArray &p_outputs, Vector2 p_offset) {
		auto graph_node = _create_graph_node(p_type, p_inputs, p_outputs, itos(++_node_index));

		if (p_type == ModularNode::MODULAR_OUTPUT_NODE) {
			auto output = p_outputs.size() ? p_outputs[0] : String();
			if (output == TEXT_MODULE) {
				auto res = Ref(memnew(ModularResource));
				res->set_name(STTR("Text, Localization"));
				_set_node_res(graph_node, res);
			} else if (output == CONF_MODULE) {
				auto res = Ref(memnew(ModularResource));
				res->set_name(STTR("Configuration, Serialization"));
				_set_node_res(graph_node, res);
			}
			graph_node->connect("resized", callable_mp(this, &EditorModularInterface::_init_graph_node_size).bind(graph_node, p_offset),
					Object::CONNECT_ONE_SHOT | Object::CONNECT_DEFERRED);
		} else {
			graph_node->set_position_offset(p_offset);
		}
	}

	void _del_graph_node_redo(const Ref<ModularGraph> &p_modular) {
		for (int i = 0; i < p_modular->get_node_count(); i++) {
			auto node = find_graph_node(p_modular->get_node(i)->get_id());
			if (node) {
				_remove_graph_node(node);
			}
		}
	}
	void _del_graph_node_undo(const Ref<ModularGraph> &p_modular) {
		_make_graph_by_modular(p_modular);
	}

	void _move_graph_node_redo(const String &p_node, Vector2 p_from, Vector2 p_to) {
		if (GraphNode *node = find_graph_node(p_node)) {
			node->set_position_offset(p_to);
		}
	}
	void _move_graph_node_undo(const String &p_node, Vector2 p_from, Vector2 p_to) {
		if (GraphNode *node = find_graph_node(p_node)) {
			node->set_position_offset(p_from);
		}
	}

	void _set_graph_node_res_undo_redo(const String &p_node, const Ref<ModularData> &p_res) {
		if (GraphNode *node = find_graph_node(p_node)) {
			_set_node_res(node, p_res);
		}
	}

	void _set_graph_node_ports_undo_redo(const String &p_node, const PackedStringArray &p_inputs, const PackedStringArray &p_outputs) {
		if (GraphNode *node = find_graph_node(p_node)) {
			_change_graph_node_ports(node, p_inputs, p_outputs);
		}
	}

	void _set_graph_node_res_source_undo_redo(const String &p_node, const String &p_source) {
		if (GraphNode *node = find_graph_node(p_node)) {
			auto res = _get_node_res(node);
			Ref<ModularPackedFile> pck_res = res;
			if (pck_res.is_valid()) {
				pck_res->_set_file_path(p_source);
			} else {
				return;
			}

			res->validation();
		}
	}

	void _resume_graph_ports_connections(const Array &p_conn_arr) {
		for (int i = 0; i < p_conn_arr.size(); i++) {
			Array item = p_conn_arr[i];
			StringName from = item[0];
			int from_port = item[1];
			StringName to = item[2];
			int to_port = item[3];
			if (!graph->is_node_connected(from, from_port, to, to_port)) {
				_create_graph_connection(from, from_port, to, to_port);
			}
		}
	}

public:
	EditorUndoRedoManager::History &create_undo_redo(const String &p_name) {
		auto &his = get_or_creat_history();
		if (his.undo_redo) {
			his.undo_redo->create_action(p_name);
		}
		return his;
	}

	void commit_undo_redo(const EditorUndoRedoManager::History &p_history) {
		p_history.undo_redo->commit_action(true);
		_set_edited_modular_unsaved(edited_index, true);
	}

protected:
	PopupMenu *import_menu;
	PopupMenu *context_import_menu;
	ItemList *modular_list;
	Button *path_btn;
	GraphEdit *graph;
	EditorFileDialog *modular_file_dlg;
	PopupMenu *right_click_menu;
	PopupMenu *item_context_menu;
	PopupMenu *export_menu;
	ModularPortDialog *modular_port_dlg;
	ModularDefinitionDialog *modular_def_dlg;
	ConfirmationDialog *close_unsaved_files_dlg;
	EditorQuickOpen *quick_open;
	Button *status_node;

	HTTPRequest *request;
	List<String> packed_file_tasks;

	GraphNode *context_node;

	LocalVector<EditedModular> edited_modulars;
	int edited_index;

	Ref<ModularGraph> paste_modular;

	void _update_modular_list();
	void _add_script_item_type(const StringName &p_type, HashSet<StringName> &p_vector);
	void _update_node_menu(GraphNode *p_node, PopupMenu *p_popup, const String &p_base, HashSet<StringName> &p_types);
	void _update_import_menu(PopupMenu *p_popup);

	void _update_graph();
	void _update_status();
	void _count_modular_remote_files(const Ref<ModularGraph> &p_graph, int &r_total, int &r_saved);
	Ref<ModularGraph> _graph_to_modular(const List<GraphNode *> &p_list);
	Ref<ModularGraph> _get_neasted_modular_graph(const EditedModular &p_edited);

	void _add_modular_graph(const Ref<ModularGraph> &graph);
	void _remove_modular_graph(int p_index);
	void _save_modular_graph(int p_index);
	void _set_edited_modular_unsaved(int p_index, bool p_unsaved);
	void _cached_unsaved_modular(EditedModular &p_edited);
	void _reset_file_dialog(EditorFileDialog::FileMode p_mode, EditorFileDialog::Access p_access, const String &p_filters);

	void _on_resource_saved(const Ref<Resource> &p_resource);
	void _menu_item_pressed(int p_id);
	void _import_menu_pressed(int p_index, PopupMenu *p_popup);
	void _export_menu_pressed(int p_index);
	void _modular_selected(int p_index);
	void _modular_list_clicked(int p_item, Vector2 p_local_mouse_pos, MouseButton p_mouse_button_index);
	void _show_save_file_dialog();
	void _begin_download_packed_file(const String &p_url);
	void _packed_file_downloaded(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	// Signal Handlers - GraphEdit
	void _init_graph_node_size(GraphNode *p_node, Vector2 p_release_position);
	void _graph_popup_request(Vector2 p_positoin);
	void _graph_connection_from_empty(const StringName &p_to_node, int p_to_port, Vector2 p_release_position);
	void _graph_connection_to_empty(const StringName &p_from_node, int p_from_port, Vector2 p_release_position);
	void _graph_delete_nodes_request(const TypedArray<StringName> p_nodes);
	void _graph_connection_request(const StringName &p_from_node, int p_from_port, const String &p_to_node, int p_to_port);
	void _graph_disconnection_request(const StringName &p_from_node, int p_from_port, const String &p_to_node, int p_to_port);
	void _node_close_request(GraphNode *p_node);
	void _node_dragged(Vector2 p_from, Vector2 p_to, GraphNode *p_node);
	void _resolve_graph_status();

	// Signal Handlers - Controls at GraphNode
	void _set_node_inout_click(GraphNode *p_node);
	void _view_node_define_click(GraphNode *p_node);
	void _set_node_play_click(GraphNode *p_node);
	void _set_node_export_click(Control *p_node);
	void _node_resource_selected(const Ref<ModularData> &p_res, bool p_inspect, GraphNode *p_node);
	void _node_resource_changed(const Ref<ModularData> &p_res, GraphNode *p_node);
	void _toggle_add_language(bool p_pressed, Button *p_toggle);
	void _toggle_text_language(bool p_pressed, Button *p_toggle);
	void _toggle_config_list(bool p_pressed, Button *p_toggle);
	void _open_override_lang_file(const String &p_dir, const String &p_lang, GraphNode *p_node);
	void _open_conf_patch_file(const String &p_dir, const String &p_conf, GraphNode *p_node);
	void _open_override_module_file(const String &p_dir, const String &p_file_path, GraphNode *p_node, const String &p_packed_file);
	void _activated_module_item(int p_index, ItemList *p_list);
	void _activated_module_file(const String &p_module_file);
	void _select_sub_graph(int p_depth);

	// Signal Handlers - Menus
	void _right_click_menu_id_pressed(int p_id);
	void _open_modular_file(const String &p_file);
	void _open_modular_dir(const String &p_dir);
	void _cancel_modular_file_dlg();
	void _create_modular_graph(const String &p_file);
	void _open_modular_graph(const String &p_file);
	void _import_modular_json(const String &p_file);
	void _modular_loaded_from_json(Ref<ModularGraph> p_modular);
	void _save_modular_graph_at(const String &p_file, int p_index, const Ref<ModularGraph> &p_res);
	void _graph_node_resource_source(const String &p_source, GraphNode *p_node);

	// Signal Handlers - Dialogs
	void _file_quick_selected();
	void _select_modular_port();
	void _close_modular_port_dialog();
	void _save_files_before_close();
	void _close_files_cancel_action();
	void _close_files_without_save();
	void _close_files_custom_action(const String &p_action);

	// Graph Creation API
	void _make_graph_by_modular(const Ref<ModularGraph> &p_modular);
	Ref<ModularNode> _create_modular_node(GraphNode *p_node);
	GraphNode *_new_graph_node();
	GraphNode *_create_graph_node(int p_type, const PackedStringArray &p_inputs, const PackedStringArray &p_outputs, const String &p_id = String());
	Node *_create_graph_node_slot(GraphNode *p_node, int p_slot, const String &p_in, const String &p_out);
	void _create_graph_node_slots(GraphNode *p_node, int p_start, const PackedStringArray &p_inputs, const PackedStringArray &p_outputs);
	void _create_graph_connection(const String &p_from, int p_from_port, const String &p_to, int p_to_port);
	void _remove_graph_node(GraphNode *p_node = nullptr);
	void _remove_graph_connection(const String &p_from, int p_from_port, const String &p_to, int p_to_port);
	void _change_graph_node_ports(GraphNode *p_node,
			const PackedStringArray &p_from_inputs, const PackedStringArray &p_to_inputs,
			const PackedStringArray &p_from_outputs, const PackedStringArray &p_to_outputs);
	void _change_graph_node_ports(GraphNode *p_node, const PackedStringArray &p_inputs, const PackedStringArray &p_outputs);

	void _notification(int p_what);

	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;

public:
	void _add_packed_file_menu(int p_id, GraphNode *p_node);

	Ref<ModularGraph> get_edited_modular() {
		if (edited_index >= 0 && edited_index < (int)edited_modulars.size()) {
			return edited_modulars[edited_index].modular;
		}
		return Ref<ModularGraph>();
	}
	void edit_modular_graph(const Ref<ModularGraph> &p_modular_graph) {
		auto index = edited_index;
		if (p_modular_graph.is_valid()) {
			const_cast<ModularGraph *>(p_modular_graph.ptr())->init_packed_file_providers();
		}
		_add_modular_graph(p_modular_graph);
		if (edited_index == index) {
			auto &edited = edited_modulars[edited_index];
			if (edited.neasted_nodes.size()) {
				edited.neasted_nodes.clear();
				_update_graph();
			}
		}
	}

	int find_modular_graph(const Ref<ModularGraph> &p_modular_graph) {
		for (int i = 0; i < (int)edited_modulars.size(); ++i) {
			if (edited_modulars[i].modular == p_modular_graph) {
				return i;
			}
		}
		return -1;
	}

	void change_graph_node_resource(const Ref<ModularData> &p_res, GraphNode *p_node) {
		_node_resource_changed(p_res, p_node);
	}

	EditorFileDialog *get_file_dialog(EditorFileDialog::FileMode p_mode, EditorFileDialog::Access p_access, const String &p_filters) {
		_reset_file_dialog(p_mode, p_access, p_filters);
		return modular_file_dlg;
	}

	void queue_download_packed_file(const String &p_url);

	void popup_import_menu(Node *p_node);

	EditorModularInterface();
	~EditorModularInterface();
};

class ModularEditorPlugin : public EditorPlugin {
	GDCLASS(ModularEditorPlugin, EditorPlugin);

	EditorModularInterface *modular_interface = nullptr;

	Ref<class ModularDataInspectorPlugin> inspector_plugin;

	void _file_removed(const String &p_file);
	void _files_moved(const String &p_old_file, const String &p_new_file);

public:
	virtual String get_name() const override { return "Modular"; }
	bool has_main_screen() const override { return true; }
	virtual void edit(Object *p_object) override {
		if (auto modular_graph = Object::cast_to<ModularGraph>(p_object)) {
			if (modular_graph->get_path().is_resource_file()) {
				modular_interface->edit_modular_graph(modular_graph);
			}
		}
	}
	virtual bool handles(Object *p_object) const override {
		return Object::cast_to<ModularGraph>(p_object) != nullptr;
	}
	virtual void make_visible(bool p_visible) override {
		modular_interface->set_visible(p_visible);
	}

	ModularEditorPlugin();
	~ModularEditorPlugin();

	static EditorModularInterface *find_interface() {
		int plugin_count = EditorNode::get_singleton()->get_editor_data().get_editor_plugin_count();
		for (int i = 0; i < plugin_count; ++i) {
			auto plugin = EditorNode::get_singleton()->get_editor_data().get_editor_plugin(i);
			if (plugin->get_name() == "Modular") {
				return Object::cast_to<ModularEditorPlugin>(plugin)->modular_interface;
			}
		}
		return nullptr;
	}
};