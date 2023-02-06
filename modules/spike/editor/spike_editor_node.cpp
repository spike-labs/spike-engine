/**
 * spike_editor_node.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "spike_editor_node.h"

#include "core/config/project_settings.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_about.h"
#include "editor/editor_feature_profile.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/history_dock.h"
#include "editor/inspector_dock.h"
#include "import/spike_resource_importer_texture_atlas.h"
#include "inspector/material_editor_inspector.h"
#include "scene/gui/menu_bar.h"
#include "spike/editor/editor_log_node.h"
#include "spike_editor_utils.h"
#include "spike_filesystem_dock.h"
#include "spike_format_loader.h"

static void resume_object_connectios(Object *self, List<Object::Connection> &connections_to, List<Object::Connection> &connections_from) {
	for (auto conn : connections_to) {
		auto object = ObjectDB::get_instance(conn.callable.get_object_id());
		if (object && !self->is_connected(conn.signal.get_name(), conn.callable)) {
			self->connect(conn.signal.get_name(), conn.callable, conn.flags);
		}
	}

	for (auto conn : connections_from) {
		// auto callable = conn.callable;
		// auto object = ObjectDB::get_instance(callable.get_object_id());
		// TODO change object of callable
	}
}

static void _unregister_menu(PackedStringArray &p_menus) {
	Object::cast_to<SpikeEditorNode>(EditorNode::get_singleton())->remove_menu_item(p_menus);
}

void SpikeEditorNode::_bind_methods() {
}

SpikeEditorNode::SpikeEditorNode() {
	custom_menus = memnew(NodeContextRef<PackedStringArray>);
	custom_menus->unregister = &_unregister_menu;

	// Plugins
	{
		Ref<SpikeInspectorPluginMaterial> material_inspector;
		material_inspector.instantiate();
		EditorInspector::add_inspector_plugin(material_inspector);
	}

	// Editor menu
	file_menu->connect("index_pressed", callable_mp(this, &SpikeEditorNode::_submenu_pressed).bind(file_menu));
	project_menu->connect("index_pressed", callable_mp(this, &SpikeEditorNode::_submenu_pressed).bind(project_menu));
	debug_menu->connect("index_pressed", callable_mp(this, &SpikeEditorNode::_submenu_pressed).bind(debug_menu));
	settings_menu->connect("index_pressed", callable_mp(this, &SpikeEditorNode::_submenu_pressed).bind(settings_menu));
	help_menu->connect("index_pressed", callable_mp(this, &SpikeEditorNode::_submenu_pressed).bind(help_menu));
	_init_help_menu();

	// Replace `FileSystemDock` to `SpikeFileSystemDock`
	{
		auto dockbr = dock_slot[DOCK_SLOT_LEFT_BR];
		auto fsd = FileSystemDock::get_singleton();
		List<Object::Connection> connections_to, connections_from;
		fsd->get_all_signal_connections(&connections_to);
		fsd->get_signals_connected_to_this(&connections_from);
		dockbr->remove_child(fsd);
		memdelete(fsd);

		auto filesystem_dock = memnew(SpikeFileSystemDock());
		// FileSystem: Bottom left
		dockbr->add_child(filesystem_dock);
		dockbr->set_tab_title(dockbr->get_tab_idx_from_control(filesystem_dock), TTR("FileSystem"));

		resume_object_connectios(filesystem_dock, connections_to, connections_from);
		filesystem_dock->connect("files_moved", callable_mp(this, &SpikeEditorNode::_file_moved_on_dock));
		filesystem_dock->connect("file_removed", callable_mp(this, &SpikeEditorNode::_file_removed_on_dock));
		EditorFileSystem::get_singleton()->connect("sources_changed", callable_mp(this, &SpikeEditorNode::_filesystem_first_scanned), CONNECT_ONE_SHOT);
	}

	// Replace `ResourceImporterTextureAtlas` to `SpikeResourceImporterTextureAtlas`
	{
		Ref<SpikeResourceImporterTextureAtlas> import_texture_atlas;
		import_texture_atlas.instantiate();
		auto resource_format_imorter = ResourceFormatImporter::get_singleton();
		auto origin = resource_format_imorter->get_importer_by_name(import_texture_atlas->get_importer_name());
		resource_format_imorter->remove_importer(origin);
		resource_format_imorter->add_importer(import_texture_atlas);

		// There are bugs in multi-thread import of atlas
		ProjectSettings::get_singleton()->set_setting("editor/import/use_multiple_threads", false);
	}

	// Replace `EditorLog` to `SpikeEditorLogNode`
	{
		log->deinit();
		remove_bottom_panel_item(log);
		memdelete(log);
		auto log_node = memnew(SpikeEditorLogNode(this));
		ToolButton *output_button = add_bottom_panel_item(TTR("Output"), log_node);
		log_node->set_tool_button(output_button);
		log = log_node;
		log->register_undo_redo(EditorUndoRedoManager::get_singleton()->get_or_create_history(EditorUndoRedoManager::GLOBAL_HISTORY).undo_redo);
		log->register_undo_redo(EditorUndoRedoManager::get_singleton()->get_or_create_history(get_editor_data().get_current_edited_scene_history_id()).undo_redo);

		MessageQueue::get_singleton()->push_callable(callable_mp(log, &EditorLog::clear));
	}

	// fixed remote inspector
	auto debugger = EditorDebuggerNode::get_singleton()->get_current_debugger();
	if (debugger) {
		InspectorDock::get_singleton()->get_inspector_singleton()->connect("object_id_selected", callable_mp(debugger, &ScriptEditorDebugger::request_remote_object));
	}

	// Reuse Warning Dialog
	{
		warning->disconnect("custom_action", callable_mp((EditorNode *)this, &SpikeEditorNode::_copy_warning));
		warning->connect("custom_action", callable_mp(this, &SpikeEditorNode::_warning_action));
		warning->connect("close_requested", callable_mp(this, &SpikeEditorNode::_warning_closed));
		_custom_warning_btn = warning->add_button("", true, "custom");
		_custom_warning_btn->set_visible(false);
	}

	// About Dialog
	_init_about_dialog();
}

void SpikeEditorNode::_file_moved_on_dock(const String &p_old_file, const String &p_new_file) {
	String uids_path;
	String file_name = p_old_file.get_file();
	Ref<ConfigFile> cfg = SpikeFormatLoader::load_uids_for_path(p_old_file, uids_path);

	if (cfg->has_section_key(SECTION_UID, file_name)) {
		auto uid = cfg->get_value(SECTION_UID, file_name);
		cfg->set_value(SECTION_UID, file_name, Variant());
		if (cfg->has_section(SECTION_UID)) {
			cfg->save(uids_path);
		} else {
			DirAccess::remove_file_or_error(ProjectSettings::get_singleton()->globalize_path(uids_path));
		}

		file_name = p_new_file.get_file();
		cfg = SpikeFormatLoader::load_uids_for_path(p_new_file, uids_path);
		cfg->set_value(SECTION_UID, file_name, uid);
		cfg->save(uids_path);
	}
}

void SpikeEditorNode::_file_removed_on_dock(const String &p_file_path) {
	String uids_path;
	String file_name = p_file_path.get_file();
	Ref<ConfigFile> cfg = SpikeFormatLoader::load_uids_for_path(p_file_path, uids_path);
	if (cfg->has_section_key(SECTION_UID, file_name)) {
		auto uid = cfg->get_value(SECTION_UID, file_name);
		cfg->set_value(SECTION_UID, file_name, Variant());
		if (cfg->has_section(SECTION_UID)) {
			cfg->save(uids_path);
		} else {
			DirAccess::remove_file_or_error(ProjectSettings::get_singleton()->globalize_path(uids_path));
		}
	}
}

void SpikeEditorNode::_filesystem_first_scanned(bool exists) {
	SpikeFormatLoader::update_uids_for_filesystem(EditorFileSystem::get_singleton()->get_filesystem());
}

void SpikeEditorNode::_submenu_pressed(int index, PopupMenu *popup) {
	int id = popup->get_item_id(index);
	if (id == TOOLS_CUSTOM && IS_EMPTY(popup->get_item_submenu(index))) {
		Callable callback = popup->get_item_metadata(index);
		Callable::CallError ce;
		Variant result;
		if (popup->is_item_checkable(index)) {
			Variant checked = popup->is_item_checked(index);
			const Variant *args[] = { &checked };
			callback.callp(args, 1, result, ce);
			if (ce.error == Callable::CallError::CALL_OK) {
				popup->set_item_checked(index, result);
			}
		} else {
			callback.callp(nullptr, 0, result, ce);
		}

		if (ce.error != Callable::CallError::CALL_OK) {
			String err = Variant::get_callable_error_text(callback, nullptr, 0, ce);
			ERR_PRINT("Error calling function from tool menu: " + err);
		}
	}
}

void SpikeEditorNode::_warning_action(const String &p_str) {
	if (p_str == "copy") {
		EditorNode::_copy_warning(p_str);
	} else {
		if (_custom_warning_callable.is_valid()) {
			Variant ret;
			Callable::CallError ce;
			_custom_warning_callable.callp(nullptr, 0, ret, ce);
		}
		warning->hide();
	}
}

void SpikeEditorNode::_warning_closed() {
	_custom_warning_btn->set_visible(false);
	_custom_warning_callable = Callable();
}

PopupMenu *SpikeEditorNode::add_menu_item(PackedStringArray &menus, int &r_idx) {
	auto item_name = menus[menus.size() - 1];
	auto submenu = EditorUtils::make_menu(main_menu, menus, callable_mp(this, &SpikeEditorNode::_submenu_pressed), r_idx);
	if (submenu && r_idx < 0) {
		r_idx = submenu->get_item_count();
		submenu->add_item(TTR(item_name), TOOLS_CUSTOM);
	}
	return submenu;
}

bool SpikeEditorNode::remove_menu_item(PackedStringArray &menus) {
	return EditorUtils::remove_menu(main_menu, menus);
}

void SpikeEditorNode::show_custom_warning(const Callable &p_callable, const String &p_custom, const String &p_text, const String &p_title) {
	EditorNode::show_warning(p_text, p_title);
	_custom_warning_btn->set_visible(true);
	_custom_warning_btn->set_text(p_custom);
	_custom_warning_callable = p_callable;
}

void SpikeEditorNode::add_main_screen_editor_plugin(EditorPlugin *p_plugin, int p_tab_index) {
	int index = main_editor_button_hb->get_child_count();
	add_editor_plugin(p_plugin);
	if (p_tab_index >= 0 && p_tab_index <= index) {
		main_editor_button_hb->move_child(main_editor_button_hb->get_child(index), p_tab_index);
	}
}

SpikeEditorNode::~SpikeEditorNode() {
	memdelete(custom_menus);
}
