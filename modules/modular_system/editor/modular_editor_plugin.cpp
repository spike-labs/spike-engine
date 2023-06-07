/**
 * modular_editor_plugin.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_editor_plugin.h"

#include "annotation/annotation_resource.h"
#include "configuration/configuration_resource.h"
#include "configuration/editor/configuration_editor_plugin.h"
#include "core/config/project_settings.h"
#include "core/object/script_language.h"
#include "core/string/translation_po.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/editor_file_system.h"
#include "editor/editor_log.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/filesystem_dock.h"
#include "editor/inspector_dock.h"
#include "editor/plugins/script_editor_plugin.h"
#include "filesystem_server/editor/project_scanner.h"
#include "filesystem_server/providers/file_provider_pack.h"
#include "modular_data.h"
#include "modular_exporter.h"
#include "modular_importer.h"
#include "modular_system/editor/modular_data_inspector_plugin.h"
#include "modular_system/editor/modular_override_utils.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"
#include "scene/gui/texture_button.h"
#include "spike/editor/spike_editor_node.h"

#define SLOT_TYPE_ANY 0
#define SLOT_TYPE_NAMED 1

#define IN_COLOR Color::hex(0XFFFFFFFF)
#define IN_COLOR_CONNECTED Color::hex(0X61D9F5FF)
#define IN_COLOR_ANY_CONNECTED Color::hex(0X98FB98FF)
#define IN_COLOR_MISSING Color::hex(0xFF0000FF)
#define OUT_COLOR Color::hex(0XFFFFFFFF)
#define OUT_COLOR_CONNECTED Color::hex(0X61D9F5FF)

#define NODE_BTN_COLOR Color::hex(0x999999FF)

#define CLEAR_ICON get_theme_icon(SNAME("Clear"), SNAME("EditorIcons"))
#define LIST_SEL_ICON get_theme_icon(SNAME("ListSelect"), SNAME("EditorIcons"))
#define ADD_ICON get_theme_icon(SNAME("Add"), SNAME("EditorIcons"))
#define CLOSE_ICON get_theme_icon(SNAME("Close"), SNAME("EditorIcons"))
#define PLAY_ICON get_theme_icon(SNAME("Play"), SNAME("EditorIcons"))
#define EXPORT_ICON get_theme_icon(SNAME("MoveUp"), SNAME("EditorIcons"))

#define BTN_ICON_COLOR(btn, color) (btn)->add_theme_color_override("icon_normal_color", color)
#define IS_COMPOSITE_NODE(node) (node)->is_class_ptr(ModularCompositeNode::get_class_ptr_static())

#define CREATE_NODE_BTN(btn, icon, color) \
	auto btn = memnew(Button);            \
	hbox->add_child(btn);                 \
	btn->set_flat(true);                  \
	btn->set_icon(icon);                  \
	BTN_ICON_COLOR(btn, color)

#define ADJUST_NODE_POS(graph, pos) ((pos) + graph->get_scroll_ofs()) / graph->get_zoom()

#define REG_REDO(his, redo, ...) his.undo_redo->add_do_method(callable_mp(this, &EditorModularInterface::redo).bind(__VA_ARGS__))
#define REG_UNDO(his, undo, ...) his.undo_redo->add_undo_method(callable_mp(this, &EditorModularInterface::undo).bind(__VA_ARGS__))

#define REG_UNDO_REDO(his, undo, redo, ...) \
	do {                                    \
		REG_REDO(his, redo, __VA_ARGS__);   \
		REG_UNDO(his, undo, __VA_ARGS__);   \
	} while (0)

#define STORE_TR_MSG(f, k, v)      \
	f->store_string(k);            \
	f->store_8('"');               \
	f->store_string(v.c_escape()); \
	f->store_8('"');               \
	f->store_8('\n');

#define GET_UNSAVED_CLOSING_LIST(list) Array list = close_unsaved_files_dlg->get_meta("unsaved_list", Array())
#define SET_UNSAVED_CLOSING_LIST(list) close_unsaved_files_dlg->set_meta("unsaved_list", list)

static void generate_po_from_dic(Dictionary &p_all_msgs, const String &p_lang, Ref<FileAccess> &p_wr) {
	p_wr->store_line("msgid \"\"\nmsgstr \"\"");
	p_wr->store_string("\"Language: ");
	p_wr->store_string(p_lang);
	p_wr->store_line("\"\n");

	for (auto ctx = p_all_msgs.next(nullptr); ctx; ctx = p_all_msgs.next(ctx)) {
		String ctxt = *ctx;
		auto val = p_all_msgs[ctxt];
		if (val.get_type() == Variant::DICTIONARY) {
			Dictionary msgs = val;
			for (auto key = msgs.next(nullptr); key; key = msgs.next(key)) {
				PackedStringArray strs = msgs[*key];
				String msgid = *key;
				if (IS_EMPTY(msgid))
					continue;

				if (!IS_EMPTY(ctxt)) {
					STORE_TR_MSG(p_wr, "#msgctxt ", ctxt);
				}
				STORE_TR_MSG(p_wr, "#msgid ", msgid);
				if (strs.size() == 1) {
					STORE_TR_MSG(p_wr, "#msgstr ", strs[0]);
				} else {
					STORE_TR_MSG(p_wr, "#msgid_plural ", strs[0]);
					for (int i = 1; i < strs.size(); ++i) {
						p_wr->store_string("#msgstr[");
						p_wr->store_32(i + 1);
						STORE_TR_MSG(p_wr, "] ", strs[i]);
					}
				}

				p_wr->store_8('\n');
			}
		} else {
			// Not from *.po, a simple key-value.
			STORE_TR_MSG(p_wr, "#msgid ", ctxt);
			STORE_TR_MSG(p_wr, "#msgstr ", val.operator String());
		}
	}
}

static void copy_with_script_instance(Ref<Resource> p_src, Ref<Resource> p_tar) {
	p_src->reset_state();

	List<PropertyInfo> pi;
	p_tar->get_property_list(&pi);

	for (const PropertyInfo &E : pi) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}
		if (E.name == "resource_path" || E.name == "script") {
			continue;
		}

		p_src->set(E.name, p_tar->get(E.name));
	}
}

static Ref<FileProvider> get_provider_of_node(GraphNode *p_node) {
	if (p_node == nullptr)
		return Ref<FileProvider>();

	Ref<ModularPackedFile> res = EditorModularInterface::_get_node_res(p_node);
	if (res.is_null())
		return Ref<FileProvider>();

	return res->get_provider();
}

static String find_folder_from_node_res(const GraphNode *p_node) {
	Ref<ModularResource> res = EditorModularInterface::_get_node_res(p_node);
	if (res.is_null()) {
	}
	auto res_list = res->get_resources();
	for (int i = 0; i < res_list.size(); ++i) {
		const String path = res_list[i];
		if (ResourceLoader::exists(path)) {
			String floder = path.get_base_dir();
			if (!IS_EMPTY(floder)) {
				return floder;
			}
		}
	}
	return String();
}

static bool append_res_not_exists(Ref<ModularResource> p_res, const String &p_path) {
	if (p_res.is_valid()) {
		auto arr = p_res->get_resources();
		for (int i = 0; i < arr.size(); ++i) {
			String path = arr[i];
			if (ResourceLoader::exists(path)) {
				path = ResourceUID::get_singleton()->get_id_path(ResourceLoader::get_resource_uid(path));
			}

			if (path == p_path) {
				return false;
			}
		}

		arr.push_back(p_path);
		p_res->set_resources(arr);
		return !p_res->get_path().is_resource_file();
	}
	return false;
}

static Node *_create_menu_node_of_type(const String &p_type) {
	if (p_type == ModularImporterFile::get_class_static()) {
		return memnew(ModularImporterFile);
	} else if (p_type == ModularImporterUrl::get_class_static()) {
		return memnew(ModularImporterUrl);
	} else if (p_type == ModularExporterFile::get_class_static()) {
		return memnew(ModularExporterFile);
	} else if (p_type == ModularExporterFileAndRun::get_class_static()) {
		return memnew(ModularExporterFileAndRun);
	}

	Ref<Script> script = ResourceLoader::load(ScriptServer::get_global_class_path(p_type));
	if (script.is_valid()) {
		return Object::cast_to<Node>(script->call("new"));
	}

	return nullptr;
}

void EditorModularInterface::_update_modular_list() {
	modular_list->clear();
	for (uint32_t i = 0; i < edited_modulars.size(); i++) {
		const EditedModular &item = edited_modulars[i];
		String path = item.modular->get_path();
		String name = IS_EMPTY(path) ? TTR("[unsaved]") : path.get_file();
		if (item.unsaved)
			name += "(*)";

		modular_list->add_item(name);
		modular_list->set_item_tooltip(modular_list->get_item_count() - 1, path);
	}
}

void EditorModularInterface::_update_graph() {
	_node_index = 0;

	Node *path_parent = path_btn->get_parent();
	int child_count = path_parent->get_child_count();
	Ref<ModularGraph> current;
	if (edited_index >= 0 && edited_index < (int)edited_modulars.size()) {
		auto &edited = edited_modulars[edited_index];
		current = edited.editing.is_valid() ? edited.editing : edited.modular;
		String file_name = edited.modular->get_path().get_file();
		if (IS_EMPTY(file_name)) {
			file_name = TTR("[unsaved]") + "#" + itos(edited_index);
		}
		path_btn->set_text(file_name);
		for (int i = 0; i < edited.neasted_nodes.size(); ++i) {
			auto node_id = edited.neasted_nodes[i];
			current = current->find_node(node_id)->get_res();

			Button *btn = nullptr;
			int btn_idx = i + 1;
			if (btn_idx < child_count) {
				btn = Object::cast_to<Button>(path_parent->get_child(btn_idx));
				btn->set_visible(true);
			} else {
				btn = memnew(Button);
				path_parent->add_child(btn);
				btn->connect("pressed", callable_mp(this, &EditorModularInterface::_select_sub_graph).bind(btn_idx));
			}

			String module_name = current->get_module_name();
			if (IS_EMPTY(module_name)) {
				module_name = vformat("[ %s ]", node_id);
			}
			btn->set_text(module_name);
		}

		for (int i = edited.neasted_nodes.size() + 1; i < child_count; ++i) {
			Object::cast_to<Button>(path_parent->get_child(i))->set_visible(false);
		}
	} else {
		path_btn->set_text(":");
		for (int i = 1; i < child_count; ++i) {
			Object::cast_to<Button>(path_parent->get_child(i))->set_visible(false);
		}
	}

	graph->clear_connections();
	for (auto node : get_graph_node_list()) {
		graph->remove_child(node);
		node->queue_free();
	}
	_make_graph_by_modular(current);

	_update_status();
}

void EditorModularInterface::_add_script_item_type(const StringName &p_type, HashSet<StringName> &p_vector) {
	if (p_vector.has(p_type)) {
		return;
	}

	List<StringName> inheriters;
	ScriptServer::get_inheriters_list(p_type, &inheriters);
	for (const StringName &S : inheriters) {
		p_vector.insert(S);
		_add_script_item_type(S, p_vector);
	}
}

void EditorModularInterface::_update_node_menu(GraphNode *p_node, PopupMenu *p_popup, const String &p_base, HashSet<StringName> &p_types) {
	p_popup->clear();

	_add_script_item_type(p_base, p_types);

	const String preffix = p_base + "-";
	HashSet<Node *> menu_nodes;
	for (auto &type : p_types) {
		String node_name = preffix + type;
		auto node = get_node_or_null(node_name);
		if (node == nullptr) {
			node = _create_menu_node_of_type(type);
			if (node) {
				add_child(node);
				node->set_name(node_name);
			}
		}

		auto menu_node = Object::cast_to<ModularMenuNode>(node);
		if (menu_node && menu_node->is_available(p_node)) {
			menu_nodes.insert(node);
			String menu_name = menu_node->get_menu_name();
			if (menu_name.is_empty()) {
				menu_name = STTR(vformat("By %s...", type));
			}
			p_popup->add_item(menu_name);
			p_popup->set_item_metadata(p_popup->get_item_count() - 1, node);
		}
	}

	for (int i = 0; i < get_child_count(); ++i) {
		auto child = get_child(i);
		String child_name = child->get_name();
		if (child_name.begins_with(preffix)) {
			auto type = child_name.substr(preffix.length());
			if (!p_types.has(type)) {
				child->queue_free();
			}
		}
	}

	p_popup->reset_size();
}

void EditorModularInterface::_update_import_menu(PopupMenu *p_popup) {
	HashSet<StringName> importer_types;
	importer_types.insert(ModularImporterFile::get_class_static());
	importer_types.insert(ModularImporterUrl::get_class_static());

	_update_node_menu(nullptr, p_popup, ModularImporter::get_class_static(), importer_types);
}

void EditorModularInterface::_update_status() {
	if (edited_index >= 0 && edited_index < (int)edited_modulars.size()) {
		auto &edited = edited_modulars[edited_index];
		auto current = edited.editing.is_valid() ? edited.editing : edited.modular;
		int total = 0;
		int saved = 0;
		_count_modular_remote_files(current, total, saved);
		if (total > 0) {
			status_node->set_text(vformat(STTR("Resolved files: %d/%d") + "  ", saved, total));
			return;
		}
	}
	status_node->set_text(String());
}

void EditorModularInterface::_count_modular_remote_files(const Ref<ModularGraph> &p_graph, int &r_total, int &r_saved) {
	for (int i = 0; i < p_graph->get_node_count(); ++i) {
		Ref<ModularData> res = p_graph->get_node(i)->get_res();
		Ref<ModularPackedFile> pck_res = res;
		if (pck_res.is_valid()) {
			String save_path = pck_res->get_save_path();
			String local_path = pck_res->get_local_path();
			if (save_path != local_path) {
				r_total++;
				if (FileAccess::exists(local_path)) {
					r_saved++;
				}
			}
			continue;
		}

		Ref<ModularGraph> sub_graph = res;
		if (sub_graph.is_valid()) {
			_count_modular_remote_files(sub_graph, r_total, r_saved);
		}
	}
}

Ref<ModularGraph> EditorModularInterface::_graph_to_modular(const List<GraphNode *> &p_list) {
	Ref<ModularGraph> modular_graph;
	REF_INSTANTIATE(modular_graph);
	for (auto graph_node : p_list) {
		auto modular_node = _create_modular_node(graph_node);
		modular_graph->add_node(Ref<ModularNode>(modular_node));
		modular_graph->set_node_pos(modular_node->get_id(), graph_node->get_position_offset());
	}

	List<GraphEdit::Connection> conn_list;
	graph->get_connection_list(&conn_list);
	for (auto &conn : conn_list) {
		if (modular_graph->find_node(conn.to).is_valid() || modular_graph->find_node(conn.from).is_valid()) {
			GraphNode *to_node = find_graph_node(conn.to);
			modular_graph->add_connection(conn.from, conn.to, _get_node_input(to_node, conn.to_port));
		}
	}

	return modular_graph;
}

Ref<ModularGraph> EditorModularInterface::_get_neasted_modular_graph(const EditedModular &p_edited) {
	auto modular = p_edited.editing.is_valid() ? p_edited.editing : p_edited.modular;
	Ref<ModularGraph> sub_graph = modular;
	for (int i = 0; i < p_edited.neasted_nodes.size(); ++i) {
		sub_graph = sub_graph->find_node(p_edited.neasted_nodes[i])->get_res();
	}
	return sub_graph;
}

void EditorModularInterface::_add_modular_graph(const Ref<ModularGraph> &p_modular_graph) {
	if (p_modular_graph.is_null())
		return;

	int index = -1;
	for (int i = 0; i < (int)edited_modulars.size(); i++) {
		if (edited_modulars[i].modular == p_modular_graph) {
			index = i;
			break;
		}
	}
	if (index < 0) {
		index = edited_modulars.size();
		EditedModular edited;
		edited.modular = p_modular_graph;
		edited.unsaved = IS_EMPTY(p_modular_graph->get_path());
		edited_modulars.push_back(edited);
		_update_modular_list();
	}
	modular_list->select(index);
	_modular_selected(index);
}

void EditorModularInterface::_remove_modular_graph(int p_index) {
	ERR_FAIL_INDEX(p_index, (int)edited_modulars.size());
	auto edited = edited_modulars[p_index];
	if (edited.history_id != 0) {
		auto history = history_map.getptr(edited.history_id);
		if (history) {
			memdelete(history->undo_redo);
		}
	}

	Ref<ModularGraph> modular;
	if (edited_index >= 0 && p_index != edited_index) {
		modular = edited_modulars[edited_index].modular;
	}
	edited_modulars.remove_at(p_index);
	if (modular.is_valid()) {
		edited_index = find_modular_graph(modular);
	} else {
		edited_index = -1;
	}
}

void EditorModularInterface::_save_modular_graph(int p_index) {
	if (p_index < (int)edited_modulars.size()) {
		auto &edited = edited_modulars[p_index];
		if (edited.unsaved) {
			Ref<ModularGraph> new_modular;
			if (p_index == edited_index) {
				if (edited.neasted_nodes.size() == 0) {
					new_modular = _graph_to_modular(get_graph_node_list());
				} else {
					new_modular = edited.editing.is_valid() ? edited.editing : edited.modular;
					Ref<ModularGraph> sub_graph = _get_neasted_modular_graph(edited);
					copy_with_script_instance(sub_graph, _graph_to_modular(get_graph_node_list()));
				}
			} else {
				new_modular = edited.editing;
			}

			if (new_modular.is_valid()) {
				String path = edited.modular->get_path();
				if (path.is_resource_file() && !FileAccess::exists(path + ".import")) {
					copy_with_script_instance(edited.modular, new_modular);
					EditorNode::get_singleton()->save_resource_in_path(edited.modular, path);
				} else {
					_reset_file_dialog(EditorFileDialog::FILE_MODE_SAVE_FILE, EditorFileDialog::ACCESS_RESOURCES, "*.tres,*.res");
					modular_file_dlg->set_title(TTR("Save Resource As..."));
					modular_file_dlg->set_current_path("res://new_modular_graph.tres");
					modular_file_dlg->set_meta(DLG_CALLBACK, callable_mp(this, &EditorModularInterface::_save_modular_graph_at).bind(p_index, new_modular));
					modular_file_dlg->popup_file_dialog();
				}
			}
		}
	}
}

void EditorModularInterface::_set_edited_modular_unsaved(int p_index, bool p_unsaved) {
	ERR_FAIL_INDEX(p_index, (int)edited_modulars.size());

	auto &edited_modular = edited_modulars[p_index];
	if (p_unsaved != edited_modular.unsaved) {
		edited_modular.unsaved = p_unsaved;
		String path = edited_modular.modular->get_path();
		String name = IS_EMPTY(path) ? TTR("[unsaved]") : path.get_file();
		if (p_unsaved)
			name += "(*)";
		modular_list->set_item_text(p_index, name);
	}
}

void EditorModularInterface::_cached_unsaved_modular(EditedModular &p_edited) {
	if (p_edited.neasted_nodes.size() == 0) {
		if (p_edited.unsaved) {
			p_edited.editing.instantiate();
			p_edited.editing->copy_from(p_edited.modular);
			copy_with_script_instance(p_edited.editing, _graph_to_modular(get_graph_node_list()));
		}
	} else {
		Ref<ModularGraph> modular_graph = _get_neasted_modular_graph(p_edited);
		copy_with_script_instance(modular_graph, _graph_to_modular(get_graph_node_list()));
	}
}

void EditorModularInterface::_reset_file_dialog(EditorFileDialog::FileMode p_mode, EditorFileDialog::Access p_access, const String &p_filters) {
	modular_file_dlg->set_file_mode(p_mode);
	modular_file_dlg->set_access(p_access);
	modular_file_dlg->clear_filters();
	auto array = p_filters.split(",");
	for (int i = 0; i < array.size(); ++i) {
		modular_file_dlg->add_filter(array[i]);
	}
	modular_file_dlg->set_current_path("res://");
}

void EditorModularInterface::_on_resource_saved(const Ref<Resource> &p_resource) {
	auto save_path = p_resource->get_path();
	int saved_index = -1;
	for (int i = 0; i < (int)edited_modulars.size(); i++) {
		if (edited_modulars[i].modular == p_resource) {
			saved_index = i;
			break;
		}
		if (edited_modulars[i].modular->get_path() == save_path) {
			if (edited_modulars[i].modular != p_resource) {
				copy_with_script_instance(edited_modulars[i].modular, p_resource);
			}
			saved_index = i;
			break;
		}
	}

	if (saved_index >= 0) {
		_set_edited_modular_unsaved(saved_index, false);
		GET_UNSAVED_CLOSING_LIST(unsaved_array);
		if (unsaved_array.size() > 0 && unsaved_array[0] == p_resource) {
			unsaved_array.remove_at(0);
			SET_UNSAVED_CLOSING_LIST(unsaved_array);

			_remove_modular_graph(saved_index);
			_show_save_file_dialog();
		}
		return;
	}

	for (auto &node : get_graph_node_list()) {
		if (_get_node_res(node) == p_resource) {
			_set_node_res(node, p_resource);
		}
	}
}

void EditorModularInterface::_menu_item_pressed(int p_id) {
	switch (FileMenuOptions(p_id)) {
		case FileMenuOptions::FILE_NEW: {
			_reset_file_dialog(EditorFileDialog::FILE_MODE_SAVE_FILE, EditorFileDialog::ACCESS_RESOURCES, "*.tres,*.res");
			modular_file_dlg->set_title(STTR("Create new modular graph"));
			modular_file_dlg->set_current_path("res://new_modular_graph.tres");
			modular_file_dlg->set_meta(DLG_CALLBACK, callable_mp(this, &EditorModularInterface::_create_modular_graph));
			modular_file_dlg->popup_file_dialog();
		} break;
		case FileMenuOptions::FILE_OPEN: {
			if (quick_open == nullptr) {
				quick_open = memnew(EditorQuickOpen);
				add_child(quick_open);
				quick_open->connect("quick_open", callable_mp(this, &EditorModularInterface::_file_quick_selected));
			}
			quick_open->popup_dialog(ModularGraph::get_class_static());
			quick_open->set_title(TTR("Resource"));
		} break;
		case FileMenuOptions::FILE_SAVE: {
			_save_modular_graph(edited_index);
		} break;
		case FileMenuOptions::FILE_SAVE_AS: {
			auto current = get_edited_modular();
			if (current.is_valid()) {
				auto new_modular = _graph_to_modular(get_graph_node_list());
				EditorNode::get_singleton()->save_resource_as(new_modular);
			}
		} break;
		case FileMenuOptions::FILE_SAVE_ALL: {
			for (int i = 0; i < (int)edited_modulars.size(); ++i) {
				_save_modular_graph(i);
			}
		} break;
		case FileMenuOptions::FILE_CLOSE:
			if (edited_index >= 0) {
				Array unsaved_list;
				auto &edited = edited_modulars[edited_index];
				if (edited.unsaved) {
					_cached_unsaved_modular(edited);
					unsaved_list.push_back(edited.modular);
				} else {
					_remove_modular_graph(edited_index);
				}
				SET_UNSAVED_CLOSING_LIST(unsaved_list);
				_show_save_file_dialog();
			}
			break;
		case FileMenuOptions::FILE_CLOSE_ALL: {
			Array unsaved_list;
			for (int i = edited_modulars.size() - 1; i >= 0; --i) {
				auto &edited = edited_modulars[i];
				if (edited.unsaved) {
					unsaved_list.push_back(edited.modular);
					if (i == edited_index) {
						_cached_unsaved_modular(edited);
					}
				} else {
					_remove_modular_graph(i);
				}
			}

			SET_UNSAVED_CLOSING_LIST(unsaved_list);
			_show_save_file_dialog();
		} break;
		case FileMenuOptions::FILE_CLOSE_OTHER: {
			Array unsaved_list;
			auto &edited = edited_modulars[edited_index];
			for (int i = edited_modulars.size() - 1; i >= 0; --i) {
				if (i == edited_index)
					continue;
				auto &edited = edited_modulars[i];
				if (edited.unsaved) {
					unsaved_list.push_back(edited.modular);
				} else {
					_remove_modular_graph(i);
				}
			}

			SET_UNSAVED_CLOSING_LIST(unsaved_list);
			_show_save_file_dialog();
		} break;
		case FileMenuOptions::SHOW_IN_FILESYSTEM: {
			FileSystemDock *file_system_dock = FileSystemDock::get_singleton();
			file_system_dock->navigate_to_path(get_edited_modular()->get_path());

			// Ensure that the FileSystem dock is visible.
			if (file_system_dock->get_window() == get_tree()->get_root()) {
				TabContainer *tab_container = (TabContainer *)file_system_dock->get_parent_control();
				tab_container->set_current_tab(tab_container->get_tab_idx_from_control(file_system_dock));
			} else {
				file_system_dock->get_window()->grab_focus();
			}
		} break;
		default:
			break;
	}
}

void EditorModularInterface::_import_menu_pressed(int p_index, PopupMenu *p_popup) {
	auto importer = Object::cast_to<ModularImporter>(p_popup->get_item_metadata(p_index));
	if (importer) {
		Object *node = p_popup->has_meta("node") ? p_popup->get_meta("node").operator Object *() : (Object *)nullptr;
		importer->import_for(node);
		p_popup->remove_meta("node");
	}
}

void EditorModularInterface::_export_menu_pressed(int p_index) {
	auto exporter = Object::cast_to<ModularExporter>(export_menu->get_item_metadata(p_index));
	if (exporter) {
		ERR_FAIL_INDEX(edited_index, (int)edited_modulars.size());
		auto edited = edited_modulars[edited_index];
		if (edited.unsaved) {
			if (IS_EMPTY(edited.modular->get_path())) {
				Callable callback = callable_mp(this, &EditorModularInterface::_menu_item_pressed).bind(int(FileMenuOptions::FILE_SAVE));
				SpikeEditorNode::get_instance()->show_custom_warning(callback, TTR("Save"), STTR("You must save modular to a file before exporting."));
				return;
			}
			_menu_item_pressed(FileMenuOptions::FILE_SAVE);
		}

		auto modular = _get_neasted_modular_graph(edited);
		if (modular.is_valid()) {
			Object *node = export_menu->has_meta("node") ? export_menu->get_meta("node").operator Object *() : (Object *)nullptr;
			exporter->export_with(modular, node);
			export_menu->remove_meta("node");
		}
	}
}

void EditorModularInterface::_modular_selected(int p_index) {
	if (p_index != edited_index) {
		if (edited_index >= 0 && edited_index < (int)edited_modulars.size()) {
			auto &edited = edited_modulars[edited_index];
			if (edited.unsaved) {
				_cached_unsaved_modular(edited);
			}
		}
		edited_index = p_index;
		_update_graph();
	}
}

void EditorModularInterface::_modular_list_clicked(int p_item, Vector2 p_local_mouse_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index == MouseButton::RIGHT) {
		modular_list->select(p_item);
		_modular_selected(p_item);
		item_context_menu->set_position(modular_list->get_screen_position() + p_local_mouse_pos);
		item_context_menu->reset_size();
		item_context_menu->popup();
	}
}

void EditorModularInterface::_show_save_file_dialog() {
	_update_modular_list();
	if (edited_index < 0) {
		modular_list->deselect_all();
		_update_graph();
	} else {
		modular_list->select(edited_index);
	}

	GET_UNSAVED_CLOSING_LIST(unsaved_list);
	if (unsaved_list.size() > 0) {
		Ref<ModularGraph> first_modular = unsaved_list[0];
		String path = first_modular->get_path();
		String name = IS_EMPTY(path) ? TTR("[unsaved]") : path.get_file();
		close_unsaved_files_dlg->set_text(vformat(TTR("Close and save changes?") + "\n\"%s\"", name));
		close_unsaved_files_dlg->popup_centered();
	}
}

void EditorModularInterface::_begin_download_packed_file(const String &p_url) {
	request->cancel_request();
	request->set_download_file(PCK_CACHE_FILE(p_url) + ".tmp");
	request->request(p_url);
}

void EditorModularInterface::_packed_file_downloaded(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	auto front = packed_file_tasks.front();
	packed_file_tasks.pop_front();

	String err_msg;
	if (p_result != OK) {
		err_msg = vformat(STTR("Download packed file error: %d"), p_result);
	} else if (p_code != 200) {
		err_msg = vformat(STTR("Download packed file failed: %d"), p_code);
	}

	if (err_msg.length() == 0) {
		String download_file = request->get_download_file();
		String pck_file = download_file.get_basename();
		DirAccess::create_for_path(pck_file)->rename(download_file, pck_file);
	} else {
		WLog("%s. (%s)", err_msg, front->get());
	}

	front = packed_file_tasks.front();
	if (front) {
		_begin_download_packed_file(front->get());
	}

	_update_status();
}

void EditorModularInterface::_init_graph_node_size(GraphNode *p_node, Vector2 p_release_position) {
	p_node->set_visible(true);
	auto node_size = p_node->get_size();
	p_node->set_position_offset(p_release_position - Vector2(node_size.x, 36 * EDSCALE));
}

void EditorModularInterface::_graph_popup_request(Vector2 p_positoin) {
	bool has_selection = false;
	bool has_main_node = false;
	for (auto node : get_graph_node_list()) {
		if (!has_selection && node->is_selected()) {
			has_selection = true;
		}
		if (!has_main_node && _node_is_composite(node)) {
			has_main_node = true;
		}
	}

	int index = 0;
	right_click_menu->set_item_disabled(index++, edited_index < 0 || edited_index >= (int)edited_modulars.size());
	right_click_menu->set_item_disabled(index++, !has_main_node); // Locate main node
	index++; // Separator
	right_click_menu->set_item_disabled(index++, !has_selection); // CUT
	right_click_menu->set_item_disabled(index++, !has_selection); // COPY
	right_click_menu->set_item_disabled(index++, paste_modular.is_null()); // PASTE
	right_click_menu->set_item_disabled(index++, !has_selection); // DELETE
	right_click_menu->set_item_disabled(index++, !has_selection); // DUPLICATE
	right_click_menu->set_position(graph->get_screen_position() + p_positoin);
	right_click_menu->reset_size();
	right_click_menu->popup();
}

void EditorModularInterface::_graph_connection_from_empty(const StringName &p_to_node, int p_to_port, Vector2 p_release_position) {
	auto to_node = find_graph_node(p_to_node);
	ERR_FAIL_COND(to_node == nullptr);

	auto input = _get_node_input(to_node, p_to_port);
	ERR_FAIL_COND(IS_EMPTY(input));

	PackedStringArray outputs;
	outputs.push_back(input);
	Vector2 release_position = ADJUST_NODE_POS(graph, p_release_position);
	auto &history = create_undo_redo(STTR("Modular New&Connect Node"));
	REG_UNDO_REDO(history, _add_graph_node_undo, _add_graph_node_redo, 0, PackedStringArray(), outputs, release_position);
	REG_UNDO_REDO(history, _remove_graph_connection, _create_graph_connection, itos(_node_index + 1), 0, p_to_node, p_to_port);
	commit_undo_redo(history);
}

void EditorModularInterface::_graph_connection_to_empty(const StringName &p_from_node, int p_from_port, Vector2 p_release_position) {
}

void EditorModularInterface::_graph_delete_nodes_request(const TypedArray<StringName> p_nodes) {
}

void EditorModularInterface::_graph_connection_request(const StringName &p_from_node, int p_from_port, const String &p_to_node, int p_to_port) {
	if (p_from_node == p_to_node)
		return;

	auto from_node = find_graph_node(p_from_node);
	if (from_node == nullptr)
		return;
	auto to_node = find_graph_node(p_to_node);
	if (to_node == nullptr)
		return;
	auto from_port = _get_node_output(from_node, p_from_port);
	auto to_port = _get_node_input(to_node, p_to_port);
	ERR_FAIL_COND(IS_EMPTY(from_port));

	if (from_port != to_port && to_port != ANY_MODULE)
		return;

	auto &history = create_undo_redo(STTR("Modular Add Connection"));
	REG_UNDO_REDO(history, _remove_graph_connection, _create_graph_connection, p_from_node, p_from_port, p_to_node, p_to_port);
	commit_undo_redo(history);
}

void EditorModularInterface::_graph_disconnection_request(const StringName &p_from_node, int p_from_port, const String &p_to_node, int p_to_port) {
	auto &history = create_undo_redo(STTR("Modular Delete Connection"));
	REG_UNDO_REDO(history, _create_graph_connection, _remove_graph_connection, p_from_node, p_from_port, p_to_node, p_to_port);
	commit_undo_redo(history);
}

void EditorModularInterface::_node_close_request(GraphNode *p_node) {
	auto &history = create_undo_redo(STTR("Modular Delete Node"));
	List<GraphNode *> list;
	list.push_back(p_node);
	const auto &modular = _graph_to_modular(list);
	REG_UNDO_REDO(history, _del_graph_node_undo, _del_graph_node_redo, modular);
	commit_undo_redo(history);
}

void EditorModularInterface::_node_dragged(Vector2 p_from, Vector2 p_to, GraphNode *p_node) {
	auto &history = create_undo_redo(STTR("Modular Move Node"));
	REG_UNDO_REDO(history, _move_graph_node_undo, _move_graph_node_redo, get_id(p_node), p_from, p_to);
	commit_undo_redo(history);
}

void EditorModularInterface::_resolve_graph_status() {
}

void EditorModularInterface::_set_node_inout_click(GraphNode *p_node) {
	modular_port_dlg->set_title(STTR("Module In/Out Configuration"));
	modular_port_dlg->update_tree(p_node);
	modular_port_dlg->popup_centered_clamped(Size2(1050, 700) * EDSCALE, 0.8);
}

void EditorModularInterface::_view_node_define_click(GraphNode *p_node) {
	String module_name = _get_node_output(p_node, 0);
	Array module_list;
	GraphNode *to_node = p_node;
	if (module_name == ANY_MODULE) {
		// Show all modules of node
		Ref<ModularData> res = _get_node_res(p_node);
		if (res.is_valid()) {
			auto module_info_map = res->get_module_info();
			for (const Variant *key = module_info_map.next(nullptr); key; key = module_info_map.next(key)) {
				Array value = module_info_map[*key];
				if (value.size() > 2) {
					module_list.push_back(value);
				}
			}
		}
	} else {
		// Show single module of `to` Node
		to_node = get_graph_node_from(p_node);
		if (to_node) {
			Ref<ModularData> res = _get_node_res(to_node);
			if (res.is_valid()) {
				auto module_info_map = res->get_module_info();
				auto module_info = module_info_map.getptr(module_name);
				if (module_info) {
					module_list.push_back(*module_info);
				}
			}
		}
	}

	modular_def_dlg->set_title(STTR("Module Annotation") + vformat(" - [%s]", to_node ? to_node->get_title() : String()));
	modular_def_dlg->update_tree(p_node, module_list, get_provider_of_node(to_node));
	modular_def_dlg->popup_centered_clamped(Size2(1050, 700) * EDSCALE, 0.8);
}

void EditorModularInterface::_set_node_play_click(GraphNode *p_node) {
	ERR_FAIL_INDEX(edited_index, (int)edited_modulars.size());
	auto &edited = edited_modulars[edited_index];
	if (edited.unsaved) {
		if (IS_EMPTY(edited.modular->get_path())) {
			Callable callback = callable_mp(this, &EditorModularInterface::_menu_item_pressed).bind(int(FileMenuOptions::FILE_SAVE));
			SpikeEditorNode::get_instance()->show_custom_warning(callback, TTR("Save"), STTR("You must save modular to a file before playing."));
			return;
		}
		_menu_item_pressed(FileMenuOptions::FILE_SAVE);
	}

	Error ret = edited.modular->run_modular(node_graph_to_modular(p_node));
	if (OK != ret) {
		SpikeEditorNode::get_instance()->show_warning(STTR("Unable to run modular. See the log for details"));
	}
}

void EditorModularInterface::_set_node_export_click(Control *p_node) {
	context_node = find_parent_type<GraphNode>(p_node);
	ERR_FAIL_COND(context_node == nullptr);

	HashSet<StringName> exporter_types;
	exporter_types.insert(ModularExporterFile::get_class_static());
	exporter_types.insert(ModularExporterFileAndRun::get_class_static());

	_update_node_menu(context_node, export_menu, ModularExporter::get_class_static(), exporter_types);

	if (export_menu->get_item_count()) {
		export_menu->set_meta("node", context_node);
		auto node_size = p_node->get_size();
		node_size.y = 0;
		export_menu->set_position(p_node->get_screen_position() + node_size * graph->get_zoom());
		export_menu->reset_size();
		export_menu->popup();
	}
}

void EditorModularInterface::_right_click_menu_id_pressed(int p_id) {
	switch (RightClickOptions(p_id)) {
		case RightClickOptions::ADD: {
			PackedStringArray inputs, outputs;
			inputs.push_back(ANY_MODULE);
			outputs.push_back(ANY_MODULE);
			auto position = Vector2(right_click_menu->get_position()) - graph->get_screen_position();
			auto node_offset = ADJUST_NODE_POS(graph, position);
			auto &history = create_undo_redo(STTR("Modular Create Node"));
			REG_UNDO_REDO(history, _add_graph_node_undo, _add_graph_node_redo, ModularNode::MODULAR_COMPOSITE_NODE, inputs, outputs, node_offset);
			commit_undo_redo(history);
		} break;
		case RightClickOptions::CUT: {
			auto list = get_selected_node_list();
			if (list.size() > 0) {
				paste_modular = _graph_to_modular(list);
				for (auto node : list) {
					_remove_graph_node(node);
				}
				_set_edited_modular_unsaved(edited_index, true);
			}
		} break;
		case RightClickOptions::COPY: {
			paste_modular = _graph_to_modular(get_selected_node_list());
			for (int i = 0; i < paste_modular->get_node_count(); i++) {
				paste_modular->change_node_id(paste_modular->get_node(i), itos(_node_index + i + 1));
			}
		} break;
		case RightClickOptions::PASTE: {
			if (paste_modular.is_valid()) {
				Vector2 origin = paste_modular->get_node_pos(0);
				for (int i = 1; i < paste_modular->get_node_count(); i++) {
					auto pos = paste_modular->get_node_pos(i);
					if (pos.x < origin.x) {
						origin.x = pos.x;
					}
					if (pos.y < origin.y) {
						origin.y = pos.y;
					}
				}

				auto position = Vector2(right_click_menu->get_position()) - graph->get_screen_position();
				paste_modular->move_all_nodes(ADJUST_NODE_POS(graph, position) - origin);

				const auto modular = paste_modular;
				paste_modular.unref();

				auto &history = create_undo_redo(STTR("Modular Paste Nodes"));
				REG_UNDO_REDO(history, _del_graph_node_redo, _del_graph_node_undo, modular);
				commit_undo_redo(history);
			}
		} break;
		case RightClickOptions::DELETE: {
			auto list = get_selected_node_list();
			if (list.size()) {
				const auto &modular = _graph_to_modular(list);
				auto &history = create_undo_redo(STTR("Modular Delete Nodes"));
				REG_UNDO_REDO(history, _del_graph_node_undo, _del_graph_node_redo, modular);
				commit_undo_redo(history);
			}
		} break;
		case RightClickOptions::DUPLICATE: {
			_right_click_menu_id_pressed(RightClickOptions::COPY);
			right_click_menu->set_position(right_click_menu->get_position() + Vector2(50, 50) * EDSCALE);
			_right_click_menu_id_pressed(RightClickOptions::PASTE);
		} break;
		case RightClickOptions::LOCATE: {
			for (auto node : get_graph_node_list()) {
				if (_node_is_composite(node)) {
					auto offset = node->get_position_offset() * graph->get_zoom();
					graph->set_scroll_ofs(offset - (graph->get_size() - node->get_size()) / 2);
					break;
				}
			}
		} break;
		default:
			break;
	}
}

void EditorModularInterface::_open_modular_file(const String &p_path) {
	if (!IS_EMPTY(p_path)) {
		Callable callback = modular_file_dlg->get_meta(DLG_CALLBACK);
		modular_file_dlg->remove_meta(DLG_CALLBACK);
		if (callback.is_valid()) {
			Array args;
			args.push_back(p_path);
			callback.callv(args);
			return;
		}
	}
}

void EditorModularInterface::_open_modular_dir(const String &p_dir) {
	if (!IS_EMPTY(p_dir)) {
		Callable callback = modular_file_dlg->get_meta(DLG_CALLBACK);
		modular_file_dlg->remove_meta(DLG_CALLBACK);
		if (callback.is_valid()) {
			Array args;
			args.push_back(p_dir);
			callback.callv(args);
			return;
		}
	}
}
void EditorModularInterface::_cancel_modular_file_dlg() {
	_close_files_cancel_action();
}

void EditorModularInterface::_create_modular_graph(const String &p_file) {
	auto res = memnew(ModularGraph);
	EditorNode::get_singleton()->save_resource_in_path(res, p_file);
	_add_modular_graph(res);
}

void EditorModularInterface::_open_modular_graph(const String &p_file) {
	Ref<ModularGraph> res = ResourceLoader::load(p_file);
	if (res.is_valid()) {
		_add_modular_graph(res);
	} else {
		EditorNode::get_singleton()->show_warning(STTR("Resource type dismatch! (Expected a [ModularGraph])"));
	}
}

static void _init_modular_packed_node(Ref<ModularNode> p_node, const String &p_id, const String &p_pck_file) {
	ModularPackedFile *packed = memnew(ModularPackedFile);
	packed->set("file_path", p_pck_file);
	p_node->set("id", p_id);
	p_node->set("res", packed);
}

static Ref<ModularCompositeNode> _import_composite_node(Ref<ModularGraph> &p_graph, const String &p_file) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_file, FileAccess::READ, &err);
	ERR_FAIL_COND_V(err != OK, Ref<ModularCompositeNode>());

	Ref<ModularCompositeNode> ret_node;

	String dir_path = p_file.get_base_dir();
	String dir_name = dir_path.get_file();
	Dictionary dic = JSON::parse_string(f->get_as_text());
	List<Ref<ModularNode>> node_list;
	for (const Variant *key = dic.next(nullptr); key; key = dic.next(key)) {
		String node_key = *key;
		String value = dic[node_key];
		String node_id = node_key.get_basename();
		Ref<ModularNode> modular_node;
		if (IS_EMPTY(node_key.get_extension())) {
			modular_node = _import_composite_node(p_graph, PATH_JOIN(PATH_JOIN(dir_path, node_key), MODULAR_LAUNCH));
			p_graph->add_node(modular_node);
			node_list.push_back(modular_node);
		} else {
			if (IS_EMPTY(value) || node_id == dir_name) {
				modular_node = Ref(memnew(ModularCompositeNode));
				ret_node = modular_node;
				modular_node->add_input(ANY_MODULE);
				if (!IS_EMPTY(value)) {
					modular_node->add_output(ANY_MODULE);
				}
			} else {
				modular_node = Ref(memnew(ModularOutputNode));
				modular_node->add_output(ANY_MODULE);
				p_graph->add_node(modular_node);
				node_list.push_back(modular_node);
			}
			_init_modular_packed_node(modular_node, node_key.get_basename(), PATH_JOIN(dir_path, node_key));
		}
	}
	for (const auto &node : node_list) {
		p_graph->add_connection(node->get_id(), ret_node->get_id(), ANY_MODULE);
	}
	return ret_node;
}

void EditorModularInterface::_import_modular_json(const String &p_file) {
	Ref<ModularGraph> modular_graph = memnew(ModularGraph);
	modular_graph->reimport_from_uri(p_file, callable_mp(this, &EditorModularInterface::_modular_loaded_from_json));
}

void EditorModularInterface::_modular_loaded_from_json(Ref<ModularGraph> p_modular) {
	if (p_modular.is_valid()) {
		_add_modular_graph(p_modular);
	}
}

void EditorModularInterface::_save_modular_graph_at(const String &p_file, int p_index, const Ref<ModularGraph> &p_res) {
	edited_modulars[p_index].modular = p_res;
	EditorNode::get_singleton()->save_resource_in_path(p_res, p_file);
}

void EditorModularInterface::_graph_node_resource_source(const String &p_source, GraphNode *p_node) {
	bool valid = true;
	String cached_source;
	auto res = _get_node_res(p_node);
	Ref<ModularPackedFile> pck_res = res;
	if (pck_res.is_valid()) {
		cached_source = pck_res->get_save_path();
	} else {
		valid = false;
	}

	if (valid) {
		String node_id = get_id(p_node);
		auto &history = create_undo_redo(STTR("Set Node Resource Source"));
		REG_REDO(history, _set_graph_node_res_source_undo_redo, node_id, p_source);
		REG_UNDO(history, _set_graph_node_res_source_undo_redo, node_id, cached_source);
		commit_undo_redo(history);
	}
}

void EditorModularInterface::_file_quick_selected() {
	Ref<ModularGraph> graph = ResourceLoader::load(quick_open->get_selected());
	if (graph.is_valid()) {
		graph->init_packed_file_providers();
	}
	_add_modular_graph(graph);
}

void EditorModularInterface::_select_modular_port() {
	GraphNode *graph_node = Object::cast_to<GraphNode>(modular_port_dlg->get_graph_node());
	ERR_FAIL_COND(graph_node == nullptr);

	PackedStringArray inputs, outputs;
	modular_port_dlg->get_selections(inputs, outputs);

	String node_id = get_id(graph_node);
	List<GraphEdit::Connection> conn_list;
	Array node_conn_list;
	graph->get_connection_list(&conn_list);
	for (auto &conn : conn_list) {
		if (conn.from == node_id || conn.to == node_id) {
			Array item;
			item.push_back(conn.from);
			item.push_back(conn.from_port);
			item.push_back(conn.to);
			item.push_back(conn.to_port);
			node_conn_list.push_back(item);
		}
	}

	auto &history = create_undo_redo(STTR("Modular Change Ports"));
	REG_REDO(history, _set_graph_node_ports_undo_redo, node_id, inputs, outputs);
	REG_UNDO(history, _set_graph_node_ports_undo_redo, node_id, _get_node_inputs(graph_node), _get_node_outputs(graph_node));
	REG_UNDO(history, _resume_graph_ports_connections, node_conn_list);
	commit_undo_redo(history);

	graph->grab_focus();
}

void EditorModularInterface::_close_modular_port_dialog() {
	graph->grab_focus();
}

void EditorModularInterface::_save_files_before_close() {
	GET_UNSAVED_CLOSING_LIST(unsaved_list);
	if (unsaved_list.size() > 0) {
		Ref<ModularGraph> first_modular = unsaved_list[0];
		int index = find_modular_graph(first_modular);
		if (index >= 0) {
			_save_modular_graph(index);
		}
	}
}

void EditorModularInterface::_close_files_cancel_action() {
	GET_UNSAVED_CLOSING_LIST(unsaved_list);
	if (unsaved_list.size() > 0) {
		Ref<ModularGraph> first_modular = unsaved_list[0];
		unsaved_list.remove_at(0);
		SET_UNSAVED_CLOSING_LIST(unsaved_list);

		_show_save_file_dialog();
	}
}

void EditorModularInterface::_close_files_without_save() {
	GET_UNSAVED_CLOSING_LIST(unsaved_list);
	if (unsaved_list.size() > 0) {
		Ref<ModularGraph> first_modular = unsaved_list[0];
		unsaved_list.remove_at(0);
		SET_UNSAVED_CLOSING_LIST(unsaved_list);

		int index = find_modular_graph(first_modular);
		if (index >= 0) {
			_remove_modular_graph(index);
		}

		_show_save_file_dialog();
	}
}

void EditorModularInterface::_close_files_custom_action(const String &p_action) {
	close_unsaved_files_dlg->hide();
	_close_files_without_save();
}

void EditorModularInterface::_make_graph_by_modular(const Ref<ModularGraph> &p_modular) {
	auto current = p_modular;
	if (current.is_null())
		return;

	HashSet<Vector2i> pos_hash;
	for (int i = 0; i < current->get_node_count(); i++) {
		auto node = current->get_node(i);
		if (node.is_null())
			continue;

		int node_id = node->get_id().to_int();
		if (node_id > _node_index) {
			_node_index = node_id;
		}

		GET_MOD_OUTPUTS(outputs, node);
		GET_MOD_INPUTS(inputs, node);
		GraphNode *graph_node = _create_graph_node(node->get_type(), inputs, outputs, node->get_id());
		_set_node_res(graph_node, node->get_res());

		auto pos = current->get_node_pos(i);
		graph_node->set_position_offset(pos);
		pos_hash.insert(Vector2i(pos.x, pos.y));
	}

	for (int i = 0; i < current->get_connection_count(); i++) {
		auto conn = current->get_connection(i);
		Ref<ModularNode> from_node = current->find_node(conn.from);
		if (from_node.is_null()) {
			GraphNode *node = find_graph_node(conn.from);
			if (node) {
				from_node = _create_modular_node(node);
			}
		}
		if (from_node.is_null())
			continue;

		Ref<ModularNode> to_node = current->find_node(conn.to);
		if (to_node.is_null()) {
			GraphNode *node = find_graph_node(conn.to);
			if (node) {
				to_node = _create_modular_node(node);
			}
		}
		if (to_node.is_null())
			continue;
		int from_port = from_node->get_output_index(conn.port);
		if (from_port < 0) {
			continue;
		}
		int to_port = to_node->get_input_index(conn.port);
		if (to_port < 0)
			continue;

		_create_graph_connection(conn.from, from_port, conn.to, to_port);
	}

	if (pos_hash.size() == 1 && current->get_node_count() > 1) {
		graph->call_deferred("arrange_nodes");
	}
}

Ref<ModularNode> EditorModularInterface::_create_modular_node(GraphNode *p_node) {
	ModularNode *modular_node;
	String node_id = get_id(p_node);
	auto node_res = _get_node_res(p_node);
	if (_node_is_composite(p_node)) {
		modular_node = memnew(ModularCompositeNode(node_id, node_res));
	} else {
		modular_node = memnew(ModularOutputNode(node_id, node_res));
	}
	for (int i = 0; i < p_node->get_connection_input_count(); i++) {
		modular_node->add_input(_get_node_input(p_node, i));
	}
	for (int i = 0; i < p_node->get_connection_output_count(); i++) {
		modular_node->add_output(_get_node_output(p_node, i));
	}
	return Ref<ModularNode>(modular_node);
}

GraphNode *EditorModularInterface::_new_graph_node() {
	auto node = memnew(GraphNode);
	graph->add_child(node);
	node->set_resizable(false);
	node->set_show_close_button(true);
	node->connect("close_request", callable_mp(this, &EditorModularInterface::_node_close_request).bind(node), CONNECT_DEFERRED);
	node->connect("dragged", callable_mp(this, &EditorModularInterface::_node_dragged).bind(node), CONNECT_DEFERRED);
	return node;
}

GraphNode *EditorModularInterface::_create_graph_node(int p_type, const PackedStringArray &p_inputs, const PackedStringArray &p_outputs, const String &p_id) {
	auto graph_node = _new_graph_node();
	String node_id = IS_EMPTY(p_id) ? itos(_node_index) : p_id;
	graph_node->set_name(node_id);

	String node_module;
	int picker_min_size;
	if (p_type == ModularNode::MODULAR_OUTPUT_NODE) {
		picker_min_size = 100;
		if (p_outputs.size()) {
			node_module = p_outputs[0];
			if (node_module == TEXT_MODULE) {
				graph_node->set_title(STTR("Text"));
			} else if (node_module == CONF_MODULE) {
				graph_node->set_title(STTR("Configuration"));
			} else if (node_module == ANY_MODULE) {
				graph_node->set_title(STTR("[Unnamed]"));
			} else {
				graph_node->set_title(node_module.capitalize());
			}
		} else {
			graph_node->set_title(OUTPUT_TITLE);
		}
	} else {
		picker_min_size = 150;
		graph_node->set_title(COMPOSITE_TITLE);
		graph_node->set_meta(META_COMPSITE, true);
		auto hbox = memnew(HBoxContainer);
		graph_node->add_child(hbox);

		hbox->add_child(memnew(Label));

		auto sep = memnew(Control);
		sep->set_custom_minimum_size(Size2(40, 0) * EDSCALE);
		sep->set_h_size_flags(SIZE_EXPAND_FILL);
		sep->set_mouse_filter(MouseFilter::MOUSE_FILTER_IGNORE);
		hbox->add_child(sep);

		CREATE_NODE_BTN(play_btn, PLAY_ICON, NODE_BTN_COLOR);
		play_btn->connect("pressed", callable_mp(this, &EditorModularInterface::_set_node_play_click).bind(graph_node));
		play_btn->set_tooltip_text(STTR("Run Composite Module"));
		graph_node->set_meta(META_PLAY_BTN, play_btn);

		// CREATE_NODE_BTN(export_btn, EXPORT_ICON, NODE_BTN_COLOR);
		// export_btn->connect("pressed", callable_mp(this, &EditorModularInterface::_set_node_export_click).bind(graph_node));
		// graph_node->set_meta(META_EXPORT_BTN, export_btn);
	}

	// Create slots for input and output
	_create_graph_node_slots(graph_node, 0, p_inputs, p_outputs);

	// Resource Picker
	auto hbox = memnew(HBoxContainer);
	graph_node->add_child(hbox);

	auto resource_picker = memnew(EditorResourcePicker);
	graph_node->set_meta(META_PICKER, resource_picker);
	hbox->add_child(resource_picker);
	resource_picker->set_base_type("ModularData");
	resource_picker->set_custom_minimum_size(Size2(picker_min_size, 0) * EDSCALE);
	resource_picker->set_h_size_flags(SIZE_EXPAND_FILL);
	resource_picker->connect("resource_selected", callable_mp(this, &EditorModularInterface::_node_resource_selected).bind(graph_node));
	resource_picker->connect("resource_changed", callable_mp(this, &EditorModularInterface::_node_resource_changed).bind(graph_node), CONNECT_DEFERRED);
	auto assign_button = Object::cast_to<Button>(resource_picker->get_child(0));
	if (assign_button) {
		assign_button->set_clip_text(false);
	}

	auto edit_btn = memnew(Button);
	hbox->add_child(edit_btn);

	auto export_btn = memnew(Button);
	hbox->add_child(export_btn);
	export_btn->set_icon(EDITOR_THEME(icon, "MoveUp", "EditorIcons"));
	export_btn->connect("pressed", callable_mp(this, &EditorModularInterface::_set_node_export_click).bind(export_btn));

	if (node_module == TEXT_MODULE) {
		Object::cast_to<Button>(resource_picker->get_child(1))->set_visible(false);
		auto edit_lang = memnew(LineEdit);
		graph_node->set_meta(META_LANG_EDIT, edit_lang);
		hbox->add_child(edit_lang);
		edit_lang->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		edit_lang->set_visible(false);

		auto add_langs = memnew(Button);
		hbox->add_child(add_langs);
		add_langs->set_icon(EDITOR_THEME(icon, "Add", "EditorIcons"));
		add_langs->set_toggle_mode(true);
		add_langs->set_tooltip_text(STTR("Add Language"));
		add_langs->connect("toggled", callable_mp(this, &EditorModularInterface::_toggle_add_language).bind(add_langs));

		auto tgl_langs = edit_btn;
		graph_node->set_meta(META_TGL_LANGS_BTN, tgl_langs);
		hbox->move_child(tgl_langs, -1);
		tgl_langs->set_icon(EDITOR_THEME(icon, "arrow", "Tree"));
		tgl_langs->set_toggle_mode(true);
		tgl_langs->connect("toggled", callable_mp(this, &EditorModularInterface::_toggle_text_language).bind(tgl_langs));

		auto scroll = memnew(ScrollContainer);
		graph_node->add_child(scroll);
		scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);

		auto lang_vbox = memnew(VBoxContainer);
		scroll->add_child(lang_vbox);
		lang_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		lang_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		lang_vbox->add_child(memnew(HSeparator));
		auto langlist = memnew(ItemList);
		lang_vbox->add_child(langlist);
		langlist->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		langlist->connect("item_activated", callable_mp(this, &EditorModularInterface::_activated_module_item).bind(langlist));
		langlist->set_visible(false);
	} else if (node_module == CONF_MODULE) {
		Object::cast_to<Button>(resource_picker->get_child(1))->set_visible(false);
		edit_btn->set_icon(EDITOR_THEME(icon, "arrow", "Tree"));
		edit_btn->set_toggle_mode(true);
		edit_btn->connect("toggled", callable_mp(this, &EditorModularInterface::_toggle_config_list).bind(edit_btn));

		auto scroll = memnew(ScrollContainer);
		graph_node->add_child(scroll);
		scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);

		auto lang_vbox = memnew(VBoxContainer);
		scroll->add_child(lang_vbox);
		lang_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		lang_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		lang_vbox->add_child(memnew(HSeparator));
		auto conflist = memnew(ItemList);
		lang_vbox->add_child(conflist);
		conflist->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		conflist->connect("item_activated", callable_mp(this, &EditorModularInterface::_activated_module_item).bind(conflist));
		conflist->set_visible(false);

	} else {
		edit_btn->set_icon(LIST_SEL_ICON);
		if (p_type == ModularNode::MODULAR_OUTPUT_NODE) {
			edit_btn->connect("pressed", callable_mp(this, &EditorModularInterface::_view_node_define_click).bind(graph_node));
			edit_btn->set_tooltip_text(STTR("View module definitions..."));
		} else {
			edit_btn->connect("pressed", callable_mp(this, &EditorModularInterface::_set_node_inout_click).bind(graph_node));
			edit_btn->set_tooltip_text(STTR("Edit input and output ports..."));
		}
	}
	return graph_node;
}

Node *EditorModularInterface::_create_graph_node_slot(GraphNode *p_node, int p_port, const String &p_in, const String &p_out) {
	int slot = p_port;
	int slot_in_type = p_in == ANY_MODULE ? SLOT_TYPE_ANY : SLOT_TYPE_NAMED;
	int slot_out_type = p_out == ANY_MODULE ? SLOT_TYPE_ANY : SLOT_TYPE_NAMED;
	p_node->set_slot(slot, !IS_EMPTY(p_in), slot_in_type, IN_COLOR, !IS_EMPTY(p_out), slot_out_type, IN_COLOR);

	Node *port_node = nullptr;
	Label *in_label, *out_label;
	if (p_port < p_node->get_child_count()) {
		port_node = p_node->get_child(p_port);
		in_label = Object::cast_to<Label>(port_node->get_child(0));
		out_label = Object::cast_to<Label>(port_node->get_child(2));
	} else {
		port_node = memnew(HBoxContainer);
		p_node->add_child(port_node);
		in_label = memnew(Label());
		port_node->add_child(in_label);

		auto sep = memnew(Control);
		sep->set_custom_minimum_size(Size2(40, 0) * EDSCALE);
		sep->set_h_size_flags(SIZE_EXPAND_FILL);
		sep->set_mouse_filter(MouseFilter::MOUSE_FILTER_IGNORE);
		port_node->add_child(sep);

		out_label = memnew(Label());
		port_node->add_child(out_label);
	}

	bool is_composite = _node_is_composite(p_node);
	if (p_node->is_slot_enabled_left(slot)) {
		_set_node_input(p_node, p_port, p_in);
		if (in_label) {
			in_label->set_text(p_in);
		}
	}
	if (p_node->is_slot_enabled_right(slot)) {
		_set_node_output(p_node, p_port, p_out);
		if (out_label) {
			out_label->set_text(p_out);
		}
	}
	return port_node;
}

void EditorModularInterface::_create_graph_node_slots(GraphNode *p_node, int p_start, const PackedStringArray &p_inputs, const PackedStringArray &p_outputs) {
	for (int i = 0;; i++) {
		String in, out;
		if (i < p_inputs.size()) {
			in = p_inputs.get(i);
		}
		if (i < p_outputs.size()) {
			out = p_outputs.get(i);
		}

		if (IS_EMPTY(in) && IS_EMPTY(out))
			break;

		_create_graph_node_slot(p_node, p_start + i, in, out);
	}
}

void EditorModularInterface::_create_graph_connection(const String &p_from, int p_from_port, const String &p_to, int p_to_port) {
	graph->connect_node(p_from, p_from_port, p_to, p_to_port);

	if (GraphNode *from = find_graph_node(p_from)) {
		from->set_slot_color_right(from->get_connection_output_slot(p_from_port), OUT_COLOR_CONNECTED);
		if (_node_is_composite(from) && p_from_port == 0) {
			_get_sub_control<Button>(from, META_PLAY_BTN)->set_visible(false);
		}
	}

	if (GraphNode *to = find_graph_node(p_to)) {
		auto node_res = _get_node_res(to);
		Color port_color;
		String to_name = _get_node_input(to, p_to_port);
		if (to_name == ANY_MODULE) {
			port_color = IN_COLOR_ANY_CONNECTED;
		} else {
			port_color = node_res.is_valid() && node_res->has_module(to_name) ? IN_COLOR_CONNECTED : IN_COLOR_MISSING;
		}
		to->set_slot_color_left(to->get_connection_input_slot(p_to_port), port_color);
	}
}

void EditorModularInterface::_remove_graph_node(GraphNode *p_node) {
	if (nullptr == p_node)
		p_node = find_graph_node(itos(_node_index));

	if (nullptr == p_node)
		return;

	auto node_id = get_id(p_node);
	// Remove connections
	List<GraphEdit::Connection> list;
	graph->get_connection_list(&list);
	for (auto &conn : list) {
		if (conn.from == node_id || conn.to == node_id) {
			_remove_graph_connection(conn.from, conn.from_port, conn.to, conn.to_port);
		}
	}

	graph->remove_child(p_node);
	p_node->queue_free();
}

void EditorModularInterface::_remove_graph_connection(const String &p_from, int p_from_port, const String &p_to, int p_to_port) {
	graph->disconnect_node(p_from, p_from_port, p_to, p_to_port);

	if (!_graph_has_connection_out(p_from, p_from_port)) {
		if (GraphNode *from = find_graph_node(p_from)) {
			from->set_slot_color_right(from->get_connection_output_slot(p_from_port), OUT_COLOR);
		}
	}

	if (!_graph_has_connection_in(p_to, p_to_port)) {
		if (GraphNode *to = find_graph_node(p_to)) {
			to->set_slot_color_left(to->get_connection_input_slot(p_to_port), IN_COLOR);
		}
	}

	if (GraphNode *from = find_graph_node(p_from)) {
		if (_node_is_composite(from) && p_from_port == 0) {
			_get_sub_control<Button>(from, META_PLAY_BTN)->set_visible(true);
		}
	}
}

void EditorModularInterface::_change_graph_node_ports(GraphNode *p_node,
		const PackedStringArray &p_from_inputs, const PackedStringArray &p_to_inputs,
		const PackedStringArray &p_from_outputs, const PackedStringArray &p_to_outputs) {
	PackedInt32Array del_inputs, del_outputs;
	PackedStringArray add_inputs = p_to_inputs, add_outputs = p_to_outputs;

	for (int i = 0; i < p_from_inputs.size(); i++) {
		String cached = p_from_inputs[i];
		if (!p_to_inputs.has(cached)) {
			del_inputs.push_back(i);
		} else {
			add_inputs.erase(cached);
		}
	}

	for (int i = 0; i < p_from_outputs.size(); i++) {
		String cached = p_from_outputs[i];
		if (!p_to_outputs.has(cached)) {
			del_outputs.push_back(i);
		} else {
			add_outputs.erase(cached);
		}
	}

	if (add_outputs.size() == 1 && add_outputs[0] == String()) {
		add_outputs.clear();
	}

	if (del_inputs.size() == 0 && add_inputs.size() == 0 && del_outputs.size() == 0 && add_outputs.size() == 0) {
		// No change.
		return;
	}

	auto node_id = get_id(p_node);
	auto position_offset = p_node->get_position_offset();
	auto res = _get_node_res(p_node);

	List<GraphEdit::Connection> conn_in_caches, conn_out_caches;
	List<GraphEdit::Connection> list;
	graph->get_connection_list(&list);
	for (int i = 0; i < list.size(); i++) {
		auto conn = list[i];
		if (conn.to == node_id) {
			if (del_inputs.find(conn.to_port) >= 0) {
				_remove_graph_connection(conn.from, conn.from_port, conn.to, conn.to_port);
			} else {
				int port = p_to_inputs.find(p_from_inputs[conn.to_port]);
				if (port != conn.to_port) {
					_remove_graph_connection(conn.from, conn.from_port, conn.to, conn.to_port);
					conn.to_port = port;
					conn_in_caches.push_back(conn);
				}
			}
		}
		if (conn.from == node_id) {
			if (del_outputs.find(conn.from_port) >= 0) {
				_remove_graph_connection(conn.from, conn.from_port, conn.to, conn.to_port);
			} else {
				int port = p_to_outputs.find(p_from_outputs[conn.from_port]);
				if (port != conn.from_port) {
					_remove_graph_connection(conn.from, conn.from_port, conn.to, conn.to_port);
					conn.from_port = port;
					conn_out_caches.push_back(conn);
				}
			}
		}
	}

	graph->remove_child(p_node);
	p_node->queue_free();
	p_node = _create_graph_node(ModularNode::MODULAR_COMPOSITE_NODE, p_to_inputs, p_to_outputs, node_id);
	p_node->set_position_offset(position_offset);
	_set_node_res(p_node, res);

	for (auto &conn : conn_in_caches) {
		_create_graph_connection(conn.from, conn.from_port, conn.to, conn.to_port);
	}
	for (auto &conn : conn_out_caches) {
		_create_graph_connection(conn.from, conn.from_port, conn.to, conn.to_port);
	}

	// Restore connection port
	{
		List<GraphEdit::Connection> list;
		graph->get_connection_list(&list);
		for (auto &conn : list) {
			if (conn.from == node_id || conn.to == node_id) {
				_create_graph_connection(conn.from, conn.from_port, conn.to, conn.to_port);
			}
		}
	}

	_set_edited_modular_unsaved(edited_index, true);
}

void EditorModularInterface::_change_graph_node_ports(GraphNode *p_node, const PackedStringArray &p_inputs, const PackedStringArray &p_outputs) {
	_change_graph_node_ports(p_node, _get_node_inputs(p_node), p_inputs, _get_node_outputs(p_node), p_outputs);
}

void EditorModularInterface::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
			EditorNode::get_singleton()->connect("resource_saved", callable_mp(this, &EditorModularInterface::_on_resource_saved));
			break;
		case NOTIFICATION_VISIBILITY_CHANGED:
			if (is_visible()) {
				set_process_shortcut_input(true);
				graph->grab_focus();
			} else {
				set_process_shortcut_input(false);
			}
			break;
		default:
			break;
	}
}

void EditorModularInterface::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Viewport *viewport = get_viewport();
	if (viewport == nullptr)
		return;

	viewport->get_base_window();

	Control *focus = viewport->gui_get_focus_owner();
	if (focus == nullptr)
		return;
	if (focus->find_parent(get_class_name()) == nullptr)
		return;

	Ref<InputEventKey> k = p_event;
	if ((k.is_valid() && k->is_pressed() && !k->is_echo()) || Object::cast_to<InputEventShortcut>(*p_event)) {
		bool handled = false;
		if (ED_IS_SHORTCUT("ui_undo", p_event)) {
			auto history = get_or_creat_history();
			if (history.undo_redo) {
				history.undo_redo->undo();
				_set_edited_modular_unsaved(edited_index, true);
			}
			handled = true;
		}

		if (ED_IS_SHORTCUT("ui_redo", p_event)) {
			auto history = get_or_creat_history();
			if (history.undo_redo) {
				history.undo_redo->redo();
				_set_edited_modular_unsaved(edited_index, true);
			}
			handled = true;
		}

		if (handled) {
			viewport->set_input_as_handled();
		}
	}
}

void EditorModularInterface::_add_packed_file_menu(int p_id, GraphNode *p_node) {
	switch (p_id) {
		case ModularPackedFile::LOCAL_FILE: {
			_reset_file_dialog(EditorFileDialog::FILE_MODE_OPEN_FILE, EditorFileDialog::ACCESS_FILESYSTEM, "*.pck,*.zip");
			modular_file_dlg->set_title(STTR("Selecte packed file to add..."));
			modular_file_dlg->set_meta(DLG_CALLBACK, callable_mp(this, &EditorModularInterface::_graph_node_resource_source).bind(p_node));
			modular_file_dlg->popup_file_dialog();
		} break;
		case ModularPackedFile::REMOTE_FILE: {
		} break;
		case ModularPackedFile::CLEAR_FILE:
			_graph_node_resource_source(String(), p_node);
			break;
		case ModularPackedFile::RELOAD_REMOTE_FILE: {
			Ref<ModularPackedFile> res = _get_node_res(p_node);
			if (res.is_valid()) {
				String save_path = res->get_save_path();
				String cache_path = res->get_local_path();
				if (save_path != cache_path && FileAccess::exists(cache_path)) {
					DirAccess::remove_file_or_error(cache_path);
					_update_status();
				}
				MessageQueue::get_singleton()->push_callable(callable_mp(res.ptr(), &ModularPackedFile::validation));
			}
		} break;
		default:
			break;
	}
}

void EditorModularInterface::_node_resource_selected(const Ref<ModularData> &p_res, bool p_inspect, GraphNode *p_node) {
	Ref<ModularGraph> res_graph = p_res;
	if (res_graph.is_null() || res_graph->get_path().is_resource_file()) {
		if (p_inspect) {
			Ref<ModularData> res = p_res;
			res->edit_data(p_node);
		} else {
			InspectorDock::get_singleton()->edit_resource(p_res);
		}
	} else {
		InspectorDock::get_singleton()->edit_resource(p_res);
		auto &edited = edited_modulars[edited_index];
		_cached_unsaved_modular(edited);
		edited.neasted_nodes.push_back(get_id(p_node));
		_update_graph();
	}
}

void EditorModularInterface::_node_resource_changed(const Ref<ModularData> &p_res, GraphNode *p_node) {
	auto &history = create_undo_redo(STTR("Modular Set Resource"));
	auto node_id = get_id(p_node);
	REG_REDO(history, _set_graph_node_res_undo_redo, node_id, p_res);
	REG_UNDO(history, _set_graph_node_res_undo_redo, node_id, _get_node_prev_res(p_node));
	commit_undo_redo(history);
}

void EditorModularInterface::_toggle_add_language(bool p_pressed, Button *p_toggle) {
	const char *icon_name = p_pressed ? "ImportCheck" : "Add";
	p_toggle->set_icon(EDITOR_THEME(icon, icon_name, "EditorIcons"));

	GraphNode *graph_node = find_parent_type<GraphNode>(p_toggle);
	LineEdit *edit_lang = _get_sub_control<LineEdit>(graph_node, META_LANG_EDIT);
	if (!p_pressed) {
		String lang = edit_lang->get_text();
		if (!IS_EMPTY(lang)) {
			Button *tgl_langs = _get_sub_control<Button>(graph_node, META_TGL_LANGS_BTN);
			if (!tgl_langs->is_pressed()) {
				tgl_langs->set_pressed(true);
				tgl_langs->emit_signal("button_pressed", true);
			}

			auto itemlist = _get_module_item_list(graph_node);
			itemlist->add_item(lang + " (unsaved)");
			itemlist->set_item_metadata(-1, lang);
		}
		edit_lang->set_text(String());
	}

	_get_sub_control<EditorResourcePicker>(graph_node, META_PICKER)->set_visible(!p_pressed);
	edit_lang->set_visible(p_pressed);
}

void EditorModularInterface::_toggle_text_language(bool p_pressed, Button *p_toggle) {
	const char *icon_name = p_pressed ? "arrow_collapsed" : "arrow";
	p_toggle->set_icon(EDITOR_THEME(icon, icon_name, "Tree"));

	GraphNode *graph_node = find_parent_type<GraphNode>(p_toggle);
	graph_node->move_to_front();

	auto itemlist = _get_module_item_list(graph_node);
	if (itemlist == nullptr)
		return;

	itemlist->set_visible(p_pressed);
	Size2 size = graph_node->get_size();
	size.y += p_pressed ? 150 * EDSCALE : -150 * EDSCALE;
	graph_node->set_size(size);

	if (p_pressed) {
		itemlist->clear();
		Ref<FileProviderPack> provider = get_provider_of_node(get_graph_node_from(graph_node));
		if (provider.is_null())
			return;

		Set<String> langs;
		Ref<ModularResource> res = _get_node_res(graph_node);
		if (res.is_valid()) {
			Array reses = res->get_resources();
			for (int i = 0; i < reses.size(); ++i) {
				const String path = reses[i];
				if (!ResourceLoader::exists(path)) {
					continue;
				}
				Ref<Translation> lang_res = ResourceLoader::load(path);
				if (lang_res.is_valid()) {
					langs.insert(lang_res->get_locale());
				}
			}
		}

		auto files = provider->get_project_environment()->get_resource_paths();
		int n = 0;
		String fallback_tr_file;
		for (int i = 0; i < files.size(); i++) {
			auto res_type = provider->get_resource_type(files[i]);
			if (!ClassDB::is_parent_class(res_type, Translation::get_class_static()))
				continue;

			Ref<Translation> tr_res = provider->load(files[i]);
			String lang = tr_res->get_locale();
			Vector<String> tags;
			if (langs.has(lang)) {
				tags.push_back("merge");
				langs.erase(lang);
			}
			if (n == 0) {
				tags.push_back("fallback");
				fallback_tr_file = files[i];
			}
			itemlist->add_item(lang + " (" + String(",").join(tags) + ")");
			itemlist->set_item_metadata(-1, files[i]);
			n++;
		}

		for (auto &lang : langs) {
			itemlist->add_item(lang + " (new)");
			itemlist->set_item_metadata(-1, fallback_tr_file);
		}
	}
}

void EditorModularInterface::_toggle_config_list(bool p_pressed, Button *p_toggle) {
	const char *icon_name = p_pressed ? "arrow_collapsed" : "arrow";
	p_toggle->set_icon(EDITOR_THEME(icon, icon_name, "Tree"));

	GraphNode *graph_node = find_parent_type<GraphNode>(p_toggle);
	graph_node->move_to_front();

	auto itemlist = _get_module_item_list(graph_node);
	if (itemlist == nullptr)
		return;

	itemlist->set_visible(p_pressed);
	Size2 size = graph_node->get_size();
	size.y += p_pressed ? 150 * EDSCALE : -150 * EDSCALE;
	graph_node->set_size(size);

	if (p_pressed) {
		itemlist->clear();
		Ref<FileProviderPack> provider = get_provider_of_node(get_graph_node_from(graph_node));
		if (provider.is_null())
			return;

		auto files = provider->get_project_environment()->get_resource_paths();
		for (int i = 0; i < files.size(); i++) {
			if (!files[i].ends_with(".tres"))
				continue;

			auto file = files[i];
			String res_type = provider->get_resource_type(file);
			if (!ClassDB::is_parent_class(res_type, ConfigurationResource::get_class_static()))
				continue;

			itemlist->add_item(file.get_basename().get_file());
			itemlist->set_item_metadata(-1, file);
		}
	}
}

void _clear_translation_values_recurrently(Dictionary &p_dict) {
	for (auto k_ptr = p_dict.next(nullptr); k_ptr; k_ptr = p_dict.next(k_ptr)) {
		auto k = *k_ptr;
		if (p_dict[k].get_type() == Variant::DICTIONARY) {
			Dictionary dict = p_dict[k];
			_clear_translation_values_recurrently(dict);
		} else if (p_dict[k].get_type() == Variant::STRING || p_dict[k].get_type() == Variant::STRING_NAME) {
			p_dict[k] = "";
		} else if (p_dict[k].get_type() == Variant::PACKED_STRING_ARRAY) {
			p_dict[k] = PackedStringArray({ "" });
		} else {
			ERR_FAIL_MSG("Unrecognized translation dictionary.");
		}
	}
}

void EditorModularInterface::_open_override_lang_file(const String &p_dir, const String &p_file_path, GraphNode *p_node) {
	auto provider = get_provider_of_node(get_graph_node_from(p_node));
	ERR_FAIL_COND_MSG(provider.is_null(), "FileProvider NOT FOUND!");

	String lang;
	Dictionary all_tr_msgs;

	auto files = provider->get_files();
	if (files.has(ProjectSettings::get_singleton()->localize_path(p_file_path))) {
		Ref<Translation> tr_res = provider->load(p_file_path);
		if (tr_res.is_valid()) {
			// From packed file.
			lang = tr_res->get_locale();
			all_tr_msgs = tr_res.is_valid() ? tr_res->call("_get_messages").operator Dictionary() : Dictionary();
		}
	}

	if (lang.is_empty()) {
		// New local.
		lang = p_file_path;
	}

	String tr_file = PATH_JOIN(p_dir, lang + ".po");
	if (!FileAccess::exists(tr_file)) {
		{
			// Merge from ProjectScanner,
			Ref<ProjectScanner> ps;
			ps.instantiate();
			ps->scan(provider);

			// Merge same local translation first.
			for (auto &&tr_res_path : ps->get_translations()) {
				Ref<Translation> tr = provider->load(tr_res_path);
				CRASH_COND_MSG(tr.is_null(), "Project Scanner error.");
				if (tr->get_locale() == lang) {
					all_tr_msgs.merge(tr->call("_get_messages").operator Dictionary());
				}
			}

			// Merge from other locals and clear values.
			for (auto &&tr_res_path : ps->get_translations()) {
				Ref<Translation> tr = provider->load(tr_res_path);
				CRASH_COND_MSG(tr.is_null(), "Project Scanner error.");
				if (tr->get_locale() != lang) {
					auto tr_msgs = tr->call("_get_messages").operator Dictionary();
					_clear_translation_values_recurrently(tr_msgs);
					all_tr_msgs.merge(tr_msgs);
				}
			}

			// Merge from PackedScenes.
			if (!all_tr_msgs.has("")) {
				all_tr_msgs[""] = Dictionary();
			}
			Dictionary msgs = all_tr_msgs[""];
			for (auto &&scene_path : ps->get_scenes()) {
				// Convert to *.po style.
				auto scene_texts = ps->get_scene_texts(scene_path);
				for (auto &&kv : scene_texts) {
					if (!msgs.has(kv.value)) {
						msgs[StringName(kv.value)] = PackedStringArray({ StringName("") });
					}
				}
			}
		}

		auto fa = FileAccess::open(tr_file, FileAccess::WRITE);
		generate_po_from_dic(all_tr_msgs, lang, fa);
		fa.unref();

		EditorFileSystem::get_singleton()->update_file(tr_file);
		auto langlist = _get_module_item_list(p_node);
		auto items = langlist->get_selected_items();
		if (items.size() && langlist->get_item_metadata(items[0]) == lang) {
			langlist->set_item_text(items[0], lang + " (new)");
		}
	}
	EditorNode::get_singleton()->edit_resource(ResourceLoader::load(tr_file));

	auto node_res = _get_node_res(p_node);
	_update_modular_data(node_res, p_dir, "tr.pck");
	if (append_res_not_exists(node_res, tr_file)) {
		_set_edited_modular_unsaved(edited_index, true);
	}
}

void EditorModularInterface::_open_conf_patch_file(const String &p_dir, const String &p_conf, GraphNode *p_node) {
	Ref<FileProviderPack> provider = get_provider_of_node(get_graph_node_from(p_node));
	if (provider.is_null())
		return;

	Ref<ConfigurationResource> conf_res = provider->load(p_conf);
	if (conf_res.is_null())
		return;

	String conf_file = PATH_JOIN(p_dir, p_conf.get_basename().get_file() + ".tres");
	auto main_plugin = EditorNode::get_singleton()->get_editor_data().get_editor(conf_res.ptr());
	auto sub_plugins = EditorNode::get_singleton()->get_editor_data().get_subeditors(conf_res.ptr());
	if (main_plugin) {
		sub_plugins.insert(0, main_plugin);
	}
	for (auto plugin : sub_plugins) {
		if (auto conf_plugin = Object::cast_to<ConfigurationEditorPlugin>(plugin)) {
			conf_plugin->make_visible(true);
			conf_plugin->edit_with_patch(conf_res, p_conf, conf_file);
			break;
		}
	}

	auto node_res = _get_node_res(p_node);
	_update_modular_data(node_res, p_dir, "conf.pck");
	if (append_res_not_exists(node_res, conf_file)) {
		_set_edited_modular_unsaved(edited_index, true);
	}
}

void EditorModularInterface::_activated_module_item(int p_index, ItemList *p_list) {
	auto graph_node = find_parent_type<GraphNode>(p_list);
	auto mod_name = _get_node_output(graph_node, 0);
	auto item_meta = p_list->get_item_metadata(p_index);
	Callable callback;
	String dlg_title;
	if (mod_name == TEXT_MODULE) {
		callback = callable_mp(this, &EditorModularInterface::_open_override_lang_file).bind(item_meta, graph_node);
		dlg_title = STTR("Select a directory for the External Translation/Text file");
	} else if (mod_name == CONF_MODULE) {
		callback = callable_mp(this, &EditorModularInterface::_open_conf_patch_file).bind(item_meta, graph_node);
		dlg_title = STTR("Select a directory for the Configuration Patch file");
	}
	if (callback.is_null())
		return;

	String folder = find_folder_from_node_res(graph_node);
	if (!IS_EMPTY(folder)) {
		callback.call_deferred(folder);
	} else {
		_reset_file_dialog(EditorFileDialog::FILE_MODE_OPEN_DIR, EditorFileDialog::ACCESS_RESOURCES, "");
		modular_file_dlg->set_title(dlg_title);
		modular_file_dlg->set_meta(DLG_CALLBACK, callback);
		modular_file_dlg->popup_file_dialog();
	}
}

void EditorModularInterface::_open_override_module_file(const String &p_dir, const String &p_file_path, GraphNode *p_node, const String &p_packed_file) {
	Ref<ModularPackedFile> from_res = _get_node_res(get_graph_node_from(p_node));
	auto node_res = _get_node_res(p_node);
	_update_modular_data(node_res, p_dir, get_id(p_node) + ".pck");

	auto remap_conf = ModularOverrideUtils::get_loaded_remap_config(p_dir);
	Map<String, String> exist_files;
	GET_EXIST_MAPPING(remap_conf, exist_files);
	String override_file_name;
	if (!exist_files.has(p_file_path)) {
		String file_name = p_file_path.get_file();
		for (;;) {
			String unique_id = Resource::generate_scene_unique_id() + "_" + file_name;
			if (!remap_conf->has_section(unique_id)) {
				override_file_name = unique_id;
				remap_conf->set_value(unique_id, MODULAR_REMAP_PATH_KEY, p_file_path);
				ModularOverrideUtils::save_remap_config(remap_conf);
				break;
			}
		}
	} else {
		override_file_name = exist_files.get(p_file_path);
	}

	List<String> script_extensions;
	ResourceLoader::get_recognized_extensions_for_type("Script", &script_extensions);
	if (script_extensions.find(p_file_path.get_extension())) {
		EditorNode::get_singleton()->editor_select(EditorNode::EDITOR_SCRIPT);
		String override_file = PATH_JOIN(p_dir, override_file_name);

		ModularOverrideUtils::try_override_script_from_packed_file(override_file, p_packed_file, p_file_path, p_dir);

		ScriptEditor::get_singleton()->open_file(override_file);

		if (append_res_not_exists(node_res, override_file)) {
			_set_edited_modular_unsaved(edited_index, true);
		}
	}

	modular_def_dlg->hide();
}

void EditorModularInterface::_activated_module_file(const String &p_module_file) {
	GraphNode *current_node = modular_def_dlg->get_current_node();
	String src_packed_file = modular_def_dlg->get_packed_file();
	Callable callback = callable_mp(this, &EditorModularInterface::_open_override_module_file).bind(p_module_file, current_node, src_packed_file);
	String folder = find_folder_from_node_res(current_node);
	if (!IS_EMPTY(folder)) {
		callback.call_deferred(folder);
	} else {
		_reset_file_dialog(EditorFileDialog::FILE_MODE_OPEN_DIR, EditorFileDialog::ACCESS_RESOURCES, "");
		modular_file_dlg->set_meta(DLG_CALLBACK, callback);
		modular_file_dlg->set_title(STTR("Select a directory for the overrided module files"));
		modular_file_dlg->popup_file_dialog();
	}
}

void EditorModularInterface::_select_sub_graph(int p_depth) {
	if (edited_index >= 0 && edited_index < (int)edited_modulars.size()) {
		auto &edited = edited_modulars[edited_index];
		if (edited.neasted_nodes.size() > p_depth) {
			Ref<ModularGraph> modular_graph = _get_neasted_modular_graph(edited);
			copy_with_script_instance(modular_graph, _graph_to_modular(get_graph_node_list()));

			while (edited.neasted_nodes.size() > p_depth) {
				edited.neasted_nodes.pop_back();
			}
			_update_graph();
		}
	}
}

void EditorModularInterface::queue_download_packed_file(const String &p_url) {
	auto front = packed_file_tasks.front();
	auto elem = packed_file_tasks.find(p_url);
	if (elem != nullptr) {
		if (front != elem && front->next() != elem) {
			packed_file_tasks.move_before(elem, front->next()->next());
		}
	} else if (front) {
		packed_file_tasks.insert_after(front, p_url);
	} else {
		packed_file_tasks.push_back(p_url);
	}
	if (packed_file_tasks.size() == 1) {
		_begin_download_packed_file(p_url);
	}
}

void EditorModularInterface::popup_import_menu(Node *p_node) {
	_update_import_menu(context_import_menu);
	auto node = find_parent_type<GraphNode>(p_node);
	if (node == nullptr)
		return;

	context_import_menu->set_meta("node", node);
	auto node_size = node->get_size();
	node_size.x -= 80;
	context_import_menu->set_position(node->get_screen_position() + node_size * graph->get_zoom());
	context_import_menu->reset_size();
	context_import_menu->popup();
}

EditorModularInterface::EditorModularInterface() {
	set_name(get_class_name());
	edited_index = -1;

	auto main_split = memnew(HSplitContainer);
	add_child(main_split);

	VBoxContainer *vb = memnew(VBoxContainer);
	main_split->add_child(vb);

	HBoxContainer *file_hb = memnew(HBoxContainer);
	vb->add_child(file_hb);
	auto file_menu = memnew(MenuButton);
	file_menu->set_text(TTR("File"));
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/new", TTR("New Modular..."), KeyModifierMask::CMD_OR_CTRL | Key::N), FILE_NEW);
	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_item(TTR("Load Modular..."), FILE_OPEN);
	import_menu = memnew(PopupMenu);
	import_menu->set_name("ImportMenu");
	import_menu->connect("index_pressed", callable_mp(this, &EditorModularInterface::_import_menu_pressed).bind(import_menu));
	file_menu->get_popup()->add_child(import_menu);
	file_menu->get_popup()->add_submenu_item(STTR("Import Modular"), "ImportMenu");
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/save", TTR("Save"), KeyModifierMask::ALT | KeyModifierMask::CMD_OR_CTRL | Key::S), FILE_SAVE);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/save_as", TTR("Save As...")), FILE_SAVE_AS);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/save_all", TTR("Save All"), KeyModifierMask::SHIFT | KeyModifierMask::ALT | Key::S), FILE_SAVE_ALL);
	ED_SHORTCUT_OVERRIDE("script_editor/save_all", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::S);
	file_menu->get_popup()->add_separator();
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_file", TTR("Close"), KeyModifierMask::CMD_OR_CTRL | Key::W), FILE_CLOSE);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_all", TTR("Close All")), FILE_CLOSE_ALL);
	file_menu->get_popup()->add_shortcut(ED_SHORTCUT("script_editor/close_other_tabs", TTR("Close Other Tabs")), FILE_CLOSE_OTHER);
	file_menu->get_popup()->connect("id_pressed", callable_mp(this, &EditorModularInterface::_menu_item_pressed));
	file_menu->connect("pressed", callable_mp(this, &EditorModularInterface::_update_import_menu).bind(import_menu));
	file_hb->add_child(file_menu);

	item_context_menu = memnew(PopupMenu);
	vb->add_child(item_context_menu);
	item_context_menu->add_item(TTR("Save"), FILE_SAVE);
	item_context_menu->add_item(TTR("Save As..."), FILE_SAVE_AS);
	item_context_menu->add_item(TTR("Close"), FILE_CLOSE);
	item_context_menu->add_item(TTR("Close All"), FILE_CLOSE_ALL);
	item_context_menu->add_item(TTR("Close Other Tabs"), FILE_CLOSE_OTHER);
	item_context_menu->add_separator();
	item_context_menu->add_item(TTR("Show in FileSystem"), SHOW_IN_FILESYSTEM);
	item_context_menu->connect("id_pressed", callable_mp(this, &EditorModularInterface::_menu_item_pressed));

	modular_list = memnew(ItemList);
	modular_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vb->add_child(modular_list);
	modular_list->connect("item_selected", callable_mp(this, &EditorModularInterface::_modular_selected));
	modular_list->connect("item_clicked", callable_mp(this, &EditorModularInterface::_modular_list_clicked));
	//modular_list->set_drag_forwarding_compat(this);

	vb->set_custom_minimum_size(Size2(200, 300) * EDSCALE);

	auto graph_vbox = memnew(VBoxContainer);
	main_split->add_child(graph_vbox);

	auto path_hbox = memnew(HBoxContainer);
	graph_vbox->add_child(path_hbox);

	path_btn = memnew(Button);
	path_hbox->add_child(path_btn);
	path_btn->set_text(":");
	path_btn->connect("pressed", callable_mp(this, &EditorModularInterface::_select_sub_graph).bind(0));

	graph = memnew(GraphEdit);
	graph_vbox->add_child(graph);

	graph->get_zoom_hbox()->set_h_size_flags(SIZE_EXPAND_FILL);
	graph->set_v_size_flags(SIZE_EXPAND_FILL);
	graph->set_h_size_flags(SIZE_EXPAND_FILL);
	graph->set_show_zoom_label(true);
	//graph->set_drag_forwarding_compat(this);
	graph->set_minimap_opacity(0.5);
	//graph->set_connection_lines_curvature(graph_lines_curvature);
	graph->set_v_size_flags(SIZE_EXPAND_FILL);
	//graph->set_right_disconnects(true);
	graph->add_valid_left_disconnect_type(SLOT_TYPE_ANY);
	graph->add_valid_right_disconnect_type(SLOT_TYPE_NAMED);
	graph->connect("popup_request", callable_mp(this, &EditorModularInterface::_graph_popup_request));
	graph->connect("connection_from_empty", callable_mp(this, &EditorModularInterface::_graph_connection_from_empty));
	graph->connect("connection_to_empty", callable_mp(this, &EditorModularInterface::_graph_connection_to_empty));
	graph->connect("delete_nodes_request", callable_mp(this, &EditorModularInterface::_graph_delete_nodes_request));
	graph->connect("connection_request", callable_mp(this, &EditorModularInterface::_graph_connection_request), CONNECT_DEFERRED);
	graph->connect("disconnection_request", callable_mp(this, &EditorModularInterface::_graph_disconnection_request), CONNECT_DEFERRED);

	right_click_menu = memnew(PopupMenu);
	graph->add_child(right_click_menu);
	right_click_menu->connect("id_pressed", callable_mp(this, &EditorModularInterface::_right_click_menu_id_pressed));
	right_click_menu->add_item(TTR("Add Node Here"), RightClickOptions::ADD);
	right_click_menu->add_item(STTR("Locate at main node"), RightClickOptions::LOCATE);
	right_click_menu->add_separator();
	right_click_menu->add_item(TTR("Cut"), RightClickOptions::CUT);
	right_click_menu->add_item(TTR("Copy"), RightClickOptions::COPY);
	right_click_menu->add_item(TTR("Paste"), RightClickOptions::PASTE);
	right_click_menu->add_item(TTR("Delete"), RightClickOptions::DELETE);
	right_click_menu->add_item(TTR("Duplicate"), RightClickOptions::DUPLICATE);

	context_import_menu = memnew(PopupMenu);
	graph->add_child(context_import_menu);
	context_import_menu->connect("index_pressed", callable_mp(this, &EditorModularInterface::_import_menu_pressed).bind(context_import_menu));

	export_menu = memnew(PopupMenu);
	graph->add_child(export_menu);
	export_menu->connect("index_pressed", callable_mp(this, &EditorModularInterface::_export_menu_pressed));

	graph->get_zoom_hbox()->set_anchors_preset(Control::PRESET_TOP_WIDE);

	auto zoom_ctrl = memnew(Control);
	graph->get_zoom_hbox()->add_child(zoom_ctrl);
	zoom_ctrl->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	status_node = memnew(Button);
	graph->get_zoom_hbox()->add_child(status_node);
	status_node->set_flat(true);
	status_node->connect("pressed", callable_mp(this, &EditorModularInterface::_resolve_graph_status));

	quick_open = nullptr;

	modular_file_dlg = memnew(EditorFileDialog);
	modular_file_dlg->connect("file_selected", callable_mp(this, &EditorModularInterface::_open_modular_file));
	modular_file_dlg->connect("dir_selected", callable_mp(this, &EditorModularInterface::_open_modular_dir));
	modular_file_dlg->connect("canceled", callable_mp(this, &EditorModularInterface::_cancel_modular_file_dlg));
	modular_file_dlg->set_current_dir(ProjectSettings::get_singleton()->get_resource_path());
	add_child(modular_file_dlg);

	modular_port_dlg = memnew(ModularPortDialog);
	modular_port_dlg->connect("confirmed", callable_mp(this, &EditorModularInterface::_select_modular_port));
	modular_port_dlg->connect("canceled", callable_mp(this, &EditorModularInterface::_close_modular_port_dialog));
	add_child(modular_port_dlg);

	modular_def_dlg = memnew(ModularDefinitionDialog);
	add_child(modular_def_dlg);
	modular_def_dlg->set_selection_callback(callable_mp(this, &EditorModularInterface::_activated_module_file));

	close_unsaved_files_dlg = memnew(ConfirmationDialog);
	add_child(close_unsaved_files_dlg);
	close_unsaved_files_dlg->add_button(STTR("Discard"), false, "discard");
	close_unsaved_files_dlg->connect("confirmed", callable_mp(this, &EditorModularInterface::_save_files_before_close));
	close_unsaved_files_dlg->connect("canceled", callable_mp(this, &EditorModularInterface::_close_files_cancel_action));
	close_unsaved_files_dlg->connect("custom_action", callable_mp(this, &EditorModularInterface::_close_files_custom_action));

	request = memnew(HTTPRequest);
	add_child(request);
	const String proxy_host = EDITOR_GET("network/http_proxy/host");
	const int proxy_port = EDITOR_GET("network/http_proxy/port");
	request->set_http_proxy(proxy_host, proxy_port);
	request->set_https_proxy(proxy_host, proxy_port);
	request->connect("request_completed", callable_mp(this, &EditorModularInterface::_packed_file_downloaded));
}

EditorModularInterface::~EditorModularInterface() {
	for (int i = (int)edited_modulars.size() - 1; i >= 0; --i) {
		_remove_modular_graph(i);
	}
}

ModularEditorPlugin::ModularEditorPlugin() {
	modular_interface = memnew(EditorModularInterface);
	modular_interface->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	EditorNode::get_singleton()->get_main_screen_control()->add_child(modular_interface);
	modular_interface->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	modular_interface->hide();

	inspector_plugin.instantiate();
	inspector_plugin->init(modular_interface);
	add_inspector_plugin(inspector_plugin);

	FileSystemDock::get_singleton()->connect("file_removed", callable_mp(this, &ModularEditorPlugin::_file_removed));
	FileSystemDock::get_singleton()->connect("files_moved", callable_mp(this, &ModularEditorPlugin::_files_moved));
}

ModularEditorPlugin::~ModularEditorPlugin() {
	remove_inspector_plugin(inspector_plugin);
}

void ModularEditorPlugin::_file_removed(const String &p_file) {
	ModularOverrideUtils::trace_file_path_change(p_file);
}

void ModularEditorPlugin::_files_moved(const String &p_old_file, const String &p_new_file) {
	ModularOverrideUtils::trace_file_path_change(p_old_file, p_new_file);
}