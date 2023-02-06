/**
 * common_export_plugin.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "editor/export/editor_export_plugin.h"
#include "spike_define.h"

class CommonExportPlugin : public EditorExportPlugin {
	GDCLASS(CommonExportPlugin, EditorExportPlugin)

public:
	struct Settings {
		bool platform_exclude = true;
		HashSet<String> platforms;
	};

private:
	static CommonExportPlugin *singleton;
	String platform;
	String arch;
	bool debug;
	String path;
	int flags;

	HashMap<String, Settings> settings_map;
	HashSet<String> checked_dirs;

	// bool exporting = false;

protected:
	virtual String _get_name() const override;
	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;

public:
	CommonExportPlugin();
	~CommonExportPlugin();

	void update_settings(const String &key, const Settings &settings) {
		auto itor = settings_map.find(key);
		if (!itor) {
			settings_map[key] = settings;
		} else {
			itor->value = settings;
		}
	}
	static CommonExportPlugin *get_singleton() { return singleton; }
};

#define EXPORT_SETTINGS_EXT "export-settings"

class EditorExportSettings : public Resource {
	GDCLASS(EditorExportSettings, Resource);

public:
	virtual void apply_settings(CommonExportPlugin &common_export) = 0;
};

class EditorExportSettingsSaver : public ResourceFormatSaver {
public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual void get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const RES &p_resource) const override;
};

class EditorExportSettingsLoader : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};
