/**
 * modular_launcher.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/config/project_settings.h"
#include "filesystem_server/file_provider.h"
#include "filesystem_server/filesystem_server.h"
#include "filesystem_server/providers/file_provider_pack.h"
#include "filesystem_server/providers/file_provider_remap.h"
#include "modular_graph_loader.h"
#include "scene/gui/control.h"
#include "scene/main/http_request.h"
#include "scene/main/node.h"

#define DEFAULT_LAUNCHER_PATH "res://.godot/modular_launcher.tscn"
class ModularLauncher : public Node {
	GDCLASS(ModularLauncher, Node);

private:
	struct NodeData {
		int engine;
		String source;
		String type;
	};

	HTTPRequest *request = nullptr;
	Control *preset_control = nullptr;
	Control *current_control = nullptr;

	String main_scene;
	List<NodeData> node_list;
	List<NodeData>::Element *current_node;

	Ref<FileProviderDefault> default_provider;
	Ref<FileProviderRemap> remap_provider;

protected:
	GDVIRTUAL3(_loading_modular, String, int, int);
	GDVIRTUAL0(_modular_loaded);

#ifdef TOOLS_ENABLED
	void _apply_running_conf(const Dictionary &p_conf, const String &p_main_node);
#endif
	void _notification(int p_what);
	bool _set(const StringName &p_name, const Variant &p_property);
	bool _get(const StringName &p_name, Variant &r_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _default_init();
	void _load_modular_pack();
	void _load_current_node();
	void _download_completed(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _current_node_loaded();
	void _loading_modular(const String &p_item, int p_step, int p_total);
	void _modular_loaded();
	void _autoloads();
	void _launch();

	static void _bind_methods();

public:
	void add_modular_node(uint32_t p_engine, const String &p_source, const String &p_type);

	static HashMap<StringName, ProjectSettings::AutoloadInfo> *autoloads;
};

class RunGraphHandler : public ModularGraphLoader::DataHandler {
	ModularLauncher *launcher;

public:
	RunGraphHandler(ModularLauncher *p_launcher);
	virtual Variant add_node(int p_engine, const String &p_source, const String &p_type) override;
	virtual void add_connection(Variant p_from, Variant p_to) override {}

	static String find_modular_json_path();
};

class LauncherProvider : public FileProvider {
	GDCLASS(LauncherProvider, FileProvider)
private:
	PackedByteArray data;

public:
	virtual Ref<FileAccess> open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error = nullptr) const override;

	virtual PackedStringArray get_files() const override;
	virtual bool has_file(const String &p_path) const override;

	static LauncherProvider *create(const String &p_main_scene);
};
