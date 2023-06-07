/**
 * modular_importer.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#include "modular_importer.h"

#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "modular_editor_plugin.h"
#include "modular_system/modular_graph_loader.h"

ImportGraphHandler::ImportGraphHandler(const Ref<ModularGraph> &p_graph, const Callable &p_callback) {
	graph = p_graph;
	onloaded = p_callback;
}

Variant ImportGraphHandler::add_node(int p_engine, const String &p_source, const String &p_type) {
	Ref<ModularNode> modular_node;
	if (p_type == String()) {
		modular_node = Ref(memnew(ModularCompositeNode));
		modular_node->add_input(ANY_MODULE);
	} else {
		modular_node = Ref(memnew(ModularOutputNode));

		if (p_type == "tr") {
			modular_node->add_output(TEXT_MODULE);
		} else if (p_type == "conf") {
			modular_node->add_output(CONF_MODULE);
		} else {
			modular_node->add_output(ANY_MODULE);
		}
	}
	graph->add_node(modular_node);

	if (p_engine < 0) {
		ModularGraph *graph = memnew(ModularGraph);
		modular_node->set("id", itos(graph->get_node_count()));
		modular_node->set("res", graph);

		graph->reimport_from_uri(PATH_JOIN(p_source, MODULAR_LAUNCH), Callable());
	} else {
		Ref<ModularData> data;
		EditorModularInterface *interface = ModularEditorPlugin::find_interface();
		for (int i = 0; i < interface->get_child_count(); ++i) {
			Node *child = interface->get_child(i);
			String child_name = child->get_name();
			if (child_name.begins_with(ModularImporter::get_class_static())) {
				data = Object::cast_to<ModularImporter>(child)->import_data(p_source);
				if (data.is_valid())
					break;
			}
		}

		if (data.is_valid()) {
			modular_node->set("id", itos(graph->get_node_count()));
			modular_node->set("res", data);
		}
	}

	return modular_node;
}

void ImportGraphHandler::add_connection(Variant p_from, Variant p_to) {
	ModularNode *from = Object::cast_to<ModularNode>(p_from);
	ModularNode *to = Object::cast_to<ModularNode>(p_to);
	String out = from->get_output_name(0);
	if (IS_EMPTY(out)) {
		out = ANY_MODULE;
		from->add_output(out);
	}
	if (to->get_input_index(out) < 0) {
		to->add_input(out);
	}
	graph->add_connection(from->get_id(), to->get_id(), out);
}

void ImportGraphHandler::loaded() {
	if (onloaded.is_valid()) {
		Array args;
		args.push_back(graph);
		onloaded.callv(args);
	}
}

void ModularImporter::_import_from_json_file(const String &p_json_file) {
	Ref<ModularGraph> modular = memnew(ModularGraph);
	modular->reimport_from_uri(p_json_file, callable_mp(this, &ModularImporter::_modular_graph_imported));
}

void ModularImporter::_modular_graph_imported(Ref<ModularGraph> p_modular) {
	import_as(p_modular);
	if (p_modular.is_null())
		return;

	auto node = Object::cast_to<GraphNode>(context);
	if (node == nullptr) {
		EditorNode::get_singleton()->edit_resource(p_modular);
	} else {
		ModularEditorPlugin::find_interface()->change_graph_node_resource(p_modular, node);
	}
	p_modular->validation();
}

void ModularImporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("import_from_path", "path"), &ModularImporter::_import_from_json_file);
	ClassDB::bind_method(D_METHOD("set_modular_imported", "modular_graph"), &ModularImporter::_modular_graph_imported);

	GDVIRTUAL_BIND(_get_menu_name)
	GDVIRTUAL_BIND(_import_for, "context")
	GDVIRTUAL_BIND(_import_as, "modular")
	GDVIRTUAL_BIND(_import_data, "source")
}

String ModularImporter::get_menu_name() {
	REQUIRE_TOOL_SCRIPT(this);
	String ret;
	GDVIRTUAL_CALL(_get_menu_name, ret);
	return ret;
}

void ModularImporter::import_for(Object *p_context) {
	context = p_context;
	REQUIRE_TOOL_SCRIPT(this);
	GDVIRTUAL_CALL(_import_for, p_context);
}

void ModularImporter::import_as(Ref<ModularGraph> p_modular) {
	REQUIRE_TOOL_SCRIPT(this);
	GDVIRTUAL_CALL(_import_as, p_modular);
}

Ref<ModularData> ModularImporter::import_data(String p_source) {
	REQUIRE_TOOL_SCRIPT(this);
	Ref<ModularData> ret;
	GDVIRTUAL_CALL(_import_data, p_source, ret);

	if (ret.is_null() && p_source.ends_with(".pck") || p_source.ends_with(".zip")) {
		ModularPackedFile *packed = memnew(ModularPackedFile);
		packed->set("file_path", modular_get_relative_path(p_source));
		ret.reference_ptr(packed);
	}
	return ret;
}

// ModularImporterFile

void ModularImporterFile::import_for(Object *p_context) {
	context = p_context;
	auto interface = ModularEditorPlugin::find_interface();
	if (interface) {
		EditorFileDialog *dialog = interface->get_file_dialog(
				EditorFileDialog::FILE_MODE_OPEN_FILE,
				EditorFileDialog::ACCESS_FILESYSTEM,
				"*.json");
		dialog->set_title(STTR("Select a modular to import"));
		dialog->set_meta(DLG_CALLBACK, callable_mp((ModularImporter *)this, &ModularImporterFile::_import_from_json_file));
		dialog->popup_file_dialog();
	}
}

// ModularImporterUrl

void ModularImporterUrl::_modular_url_confirmed() {
	String url = url_edit->get_text();
	if (!IS_EMPTY(url)) {
		_import_from_json_file(url);
	}
}

void ModularImporterUrl::import_for(Object *p_context) {
	context = p_context;

	if (dialog == nullptr) {
		dialog = memnew(ConfirmationDialog);
		add_child(dialog);
		dialog->set_theme(get_tree()->get_root()->get_theme());
		dialog->set_title(STTR("Import from url..."));
		dialog->connect("confirmed", callable_mp(this, &ModularImporterUrl::_modular_url_confirmed));

		VBoxContainer *vbox = memnew(VBoxContainer);
		dialog->add_child(vbox);

		url_edit = memnew(LineEdit);
		vbox->add_child(url_edit);
		url_edit->set_name("UrlEdit");
		url_edit->set_placeholder(STTR("Input url of a modular file..."));
	}

	dialog->popup_centered(Size2i(600, 100) * EDSCALE);
}
