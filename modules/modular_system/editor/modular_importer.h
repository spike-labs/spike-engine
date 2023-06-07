/**
 * modular_importer.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/object/gdvirtual.gen.inc"
#include "editor/editor_file_dialog.h"
#include "modular_graph.h"
#include "modular_system/modular_graph_loader.h"
#include "scene/gui/dialogs.h"
#include "spike_define.h"

class ImportGraphHandler : public ModularGraphLoader::DataHandler {
private:
	Ref<ModularGraph> graph;
	Callable onloaded;

public:
	ImportGraphHandler(const Ref<ModularGraph> &p_graph, const Callable &p_callback);

	virtual Variant add_node(int p_engine, const String &p_source, const String &p_type) override;

	virtual void add_connection(Variant p_from, Variant p_to) override;

	virtual void loaded() override;
};

class ModularImporter : public ModularMenuNode {
	GDCLASS(ModularImporter, ModularMenuNode);

protected:
	Object *context;

	GDVIRTUAL0R(String, _get_menu_name)
	GDVIRTUAL1(_import_for, Object *)
	GDVIRTUAL1(_import_as, Ref<ModularGraph>)
	GDVIRTUAL1R(Ref<ModularData>, _import_data, String)

	void _import_from_json_file(const String &p_json_file);
	void _modular_graph_imported(Ref<ModularGraph> p_modular);
	static void _bind_methods();

public:
	virtual String get_menu_name() override;
	virtual void import_for(Object *p_context);
	virtual void import_as(Ref<ModularGraph> p_modular);
	virtual Ref<ModularData> import_data(String p_source);
};

class ModularImporterFile : public ModularImporter {
	GDCLASS(ModularImporterFile, ModularImporter);

public:
	virtual void import_for(Object *p_context) override;
};

class ModularImporterUrl : public ModularImporter {
	GDCLASS(ModularImporterUrl, ModularImporter);

private:
	ConfirmationDialog *dialog = nullptr;
	LineEdit *url_edit;

protected:
	void _modular_url_confirmed();

public:
	virtual void import_for(Object *p_context) override;
};
