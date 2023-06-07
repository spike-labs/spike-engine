/**
 * modular_exporter.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "modular_graph.h"
#include "spike_define.h"

class ModularExporter : public ModularMenuNode {
	GDCLASS(ModularExporter, ModularMenuNode);

protected:
	Object *context;

	GDVIRTUAL0R(String, _get_menu_name)
    GDVIRTUAL1R(bool, _available, Node *);
	GDVIRTUAL2(_export_with, Ref<ModularGraph>, Object *)

	static void _bind_methods();

public:
	virtual String get_menu_name() override;
    virtual bool is_available(Node *p_node) override;
	virtual void export_with(Ref<ModularGraph> p_graph, Object *p_context);
};

class ModularExporterFile : public ModularExporter {
	GDCLASS(ModularExporterFile, ModularExporter);

protected:
	void _export_to_file(const String &p_path);

public:
	virtual String get_menu_name() override;
    virtual bool is_available(Node *p_node) override;
	virtual void export_with(Ref<ModularGraph> p_graph, Object *p_context) override;
};

class ModularExporterFileAndRun : public ModularExporter {
	GDCLASS(ModularExporterFileAndRun, ModularExporter);

protected:
	void _export_to_file_and_run(const String &p_path);

public:
	virtual String get_menu_name() override;
    virtual bool is_available(Node *p_node) override;
	virtual void export_with(Ref<ModularGraph> p_graph, Object *p_context) override;
};
