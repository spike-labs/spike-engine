/**
 * modular_exporter.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_exporter.h"
#include "editor/debugger/editor_debugger_node.h"
#include "modular_editor_plugin.h"
#include "spike/editor/spike_editor_node.h"

// ModularExporter

void ModularExporter::_bind_methods(){
	GDVIRTUAL_BIND(_get_menu_name)
	GDVIRTUAL_BIND(_available, "node")
	GDVIRTUAL_BIND(_export_with, "graph", "context")
}

String ModularExporter::get_menu_name() {
	REQUIRE_TOOL_SCRIPT(this);
	String ret;
	GDVIRTUAL_CALL(_get_menu_name, ret);
	return ret;
}


bool ModularExporter::is_available(Node *p_node) {
	REQUIRE_TOOL_SCRIPT(this);
	bool ret = false;
	if (GDVIRTUAL_CALL(_available, p_node, ret)) {
		return ret;
	}
	return false;
}

void ModularExporter::export_with(Ref<ModularGraph> p_graph, Object *p_context) {
	context = p_context;
	REQUIRE_TOOL_SCRIPT(this);
	GDVIRTUAL_CALL(_export_with, p_graph, p_context);
}

// ModularExporterFile

void ModularExporterFile::_export_to_file(const String &p_path) {
	auto interface = ModularEditorPlugin::find_interface();
	auto modular = interface->get_edited_modular();
	ERR_FAIL_COND(modular.is_null());

	GraphNode *context_node = Object::cast_to<GraphNode>(context);
	Error ret = modular->export_modular(interface->node_graph_to_modular(context_node), p_path);
	if (OK == ret) {
		OS::get_singleton()->shell_open(String("file://") + p_path);
	} else {
		SpikeEditorNode::get_instance()->show_warning(STTR("Unable to export modular. See the log for details"));
	}
}

String ModularExporterFile::get_menu_name() {
	return STTR("Export to local file...");
}

bool ModularExporterFile::is_available(Node *p_node) {
	return EditorModularInterface::_node_is_composite(Object::cast_to<GraphNode>(p_node));
}

void ModularExporterFile::export_with(Ref<ModularGraph> p_graph, Object *p_context) {
	context = p_context;
	auto interface = ModularEditorPlugin::find_interface();
	if (interface) {
		EditorFileDialog *dialog = interface->get_file_dialog(EditorFileDialog::FILE_MODE_OPEN_DIR, EditorFileDialog::ACCESS_FILESYSTEM, "");
		dialog->set_title(STTR("Select a directory to export modular graph"));
		dialog->set_meta(DLG_CALLBACK, callable_mp(this, &ModularExporterFile::_export_to_file));
		dialog->popup_file_dialog();
	}
}

// ModularExporterFileAndRun

void ModularExporterFileAndRun::_export_to_file_and_run(const String &p_path) {
	auto interface = ModularEditorPlugin::find_interface();
	auto modular = interface->get_edited_modular();
	ERR_FAIL_COND(modular.is_null());

	GraphNode *context_node = Object::cast_to<GraphNode>(context);
	Error ret = modular->export_modular(interface->node_graph_to_modular(context_node), p_path);
	if (OK != ret) {
		SpikeEditorNode::get_instance()->show_warning(STTR("Unable to export modular. See the log for details"));
	} else {
		String pack_name = EditorModularInterface::_get_node_res(context_node)->get_export_name();
		if (IS_EMPTY(pack_name)) {
			pack_name = EditorModularInterface::get_id(context_node) + ".pck";
		}
		EditorDebuggerNode::get_singleton()->start();

		List<String> args;
		args.push_back("--main-pack");
		args.push_back(PATH_JOIN(p_path, pack_name));

		const String debug_uri = EditorDebuggerNode::get_singleton()->get_server_uri();
		if (debug_uri.size()) {
			args.push_back("--remote-debug");
			args.push_back(debug_uri);
		}

		args.push_back("--editor-pid");
		args.push_back(itos(OS::get_singleton()->get_process_id()));

		ret = OS::get_singleton()->create_instance(args);
		if (ret == OK) {
			Object::cast_to<SpikeEditorNode>(EditorNode::get_singleton())->get_editor_run()->run_native_notify();
		} else {
			EditorDebuggerNode::get_singleton()->stop();
		}
	}
}

String ModularExporterFileAndRun::get_menu_name() {
	return STTR("Export to local file and run...");
}

bool ModularExporterFileAndRun::is_available(Node *p_node) {
	return EditorModularInterface::_node_is_composite(Object::cast_to<GraphNode>(p_node));
}

void ModularExporterFileAndRun::export_with(Ref<ModularGraph> p_graph, Object *p_context) {
	context = p_context;
	auto interface = ModularEditorPlugin::find_interface();
	if (interface) {
		EditorFileDialog *dialog = interface->get_file_dialog(EditorFileDialog::FILE_MODE_OPEN_DIR, EditorFileDialog::ACCESS_FILESYSTEM, "");
		dialog->set_title(STTR("Select a directory to export and run the modular graph"));
		dialog->set_meta(DLG_CALLBACK, callable_mp(this, &ModularExporterFileAndRun::_export_to_file_and_run));
		dialog->popup_file_dialog();
	}
}
