/**
 * pck_runner.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#ifndef PCK_RUNNER_H
#define PCK_RUNNER_H

#include "core/config/project_settings.h"
#include "core/os/mutex.h"
#include "scene/resources/packed_scene.h"
#include <core/core_bind.h>

class SpikeProjectSettings : public ProjectSettings {
public:
	HashMap<StringName, Node *> instanciated_autoloads;

	void load_settings_binary(const String &p_binary_config_file) {
		_load_settings_binary(p_binary_config_file);
	}
	void load_settings_text(const String &p_text_config_file) {
		_load_settings_text(p_text_config_file);
	}

	static SpikeProjectSettings *create() {
		auto origin = singleton;
		singleton = nullptr;
		auto ret = memnew(SpikeProjectSettings());
		singleton = origin;
		return ret;
	}

protected:
	SpikeProjectSettings() {}
};

class PCKRunner : public Object {
	GDCLASS(PCKRunner, Object)
	friend class PCKSceneTree;

protected:
	static void _bind_methods();

	void _change_scene(const String &p_pck_path, const String &p_scene);
	class PCKSceneTree *_get_pck_scene_tree();
	static void _run_pck(void *p_pck_file);
	void _add_autoloads(String p_pck_file);
	void _remove_autoloads(String p_pck_file);

	struct LoadingData {
		String pck_file;
		String main_scene_file;
		float progress;
		ResourceLoader::ThreadLoadStatus status = ResourceLoader::ThreadLoadStatus::THREAD_LOAD_IN_PROGRESS;
	};

public:
	static void run_pck(const String &p_pck_path, const bool p_thread_load = false);
	static void stop_pck(int p_exit_code = EXIT_SUCCESS);
	static bool is_loading() { return get_singleton()->threaded_loading_pcks.size() > 0; }
	static float get_threaded_load_progress(const String &p_pck_file = String());

	static PCKRunner *get_singleton() {
		if (instance == nullptr) {
			instance = memnew(PCKRunner);
		}
		return instance;
	}
	PCKRunner();
	~PCKRunner();

private:
	void _on_process_frame();
	void _finish_quit();
	static Error _load_binary_config(const String &p_file, HashMap<String, Variant> &r_settings);
	static PCKRunner *instance;

	static Mutex mtx;

	HashMap<String, SpikeProjectSettings *> loaded_pcks;
	List<String> running_pcks;
	HashMap<String, LoadingData> threaded_loading_pcks;
};

#endif