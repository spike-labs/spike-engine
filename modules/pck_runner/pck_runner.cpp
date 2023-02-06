/**
 * pck_runner.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "pck_runner.h"
#include "core/config/engine.h"
#include "core/io/file_access_pack.h"
#include "core/io/marshalls.h"
#include "core/io/resource_loader.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/worker_thread_pool.h"
#include "pck_scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"

PCKRunner *PCKRunner::instance = nullptr;
Mutex PCKRunner::mtx = Mutex();

#define EMIT_ERR_COND(m_cond, m_pck, m_err)                                                   \
	if (m_cond) {                                                                             \
		get_singleton()->call_deferred(SNAME("emit_signal"), "pck_run_failed", m_pck, m_err); \
		ERR_FAIL_COND(m_cond);                                                                \
	}
#define EMIT_ERR_FAILD_COND(m_cond, m_pck)                                                     \
	if (m_cond) {                                                                              \
		get_singleton()->call_deferred(SNAME("emit_signal"), "pck_run_failed", m_pck, FAILED); \
		ERR_FAIL_COND(m_cond);                                                                 \
	}
#define EMIT_ERR_FAILD(m_pck)                                                              \
	get_singleton()->call_deferred(SNAME("emit_signal"), "pck_run_failed", m_pck, FAILED); \
	ERR_FAIL()
#define EMIT_ERR(m_pck, m_err)                                                            \
	get_singleton()->call_deferred(SNAME("emit_signal"), "pck_run_failed", m_pck, m_err); \
	ERR_FAIL()

void PCKRunner::_bind_methods() {
	ClassDB::bind_static_method("PCKRunner", D_METHOD("run_pck", "pck_file", "threaded_load"), &PCKRunner::run_pck, DEFVAL(false));
	ClassDB::bind_static_method("PCKRunner", D_METHOD("stop_pck"), &PCKRunner::stop_pck, DEFVAL(EXIT_SUCCESS));
	ClassDB::bind_static_method("PCKRunner", D_METHOD("is_loading"), &PCKRunner::is_loading);
	ClassDB::bind_static_method("PCKRunner", D_METHOD("get_threaded_load_progress", "pck_file"), &PCKRunner::get_threaded_load_progress, DEFVAL(String()));

	ClassDB::bind_static_method("PCKRunner", D_METHOD("get_singleton"), &PCKRunner::get_singleton);
	ADD_SIGNAL(MethodInfo("pck_started", PropertyInfo(Variant::STRING, "pck_file")));
	ADD_SIGNAL(MethodInfo("pck_run_failed", PropertyInfo(Variant::STRING, "pck_file"), PropertyInfo(Variant::INT, "error")));
	ADD_SIGNAL(MethodInfo("pck_stopped", PropertyInfo(Variant::STRING, "pck_file")));

	// ClassDB::classes["PCKRunner"].creation_func = nullptr;
}

void PCKRunner::_change_scene(const String &p_pck_path, const String &p_scene) {
	auto scene_tree = _get_pck_scene_tree();
	if (scene_tree) {
		auto scene_file = scene_tree->get_current_scene()->get_scene_file_path();
		auto err = scene_tree->change_scene_to_file(p_scene);
		EMIT_ERR_COND(OK != err, p_pck_path, err);

		scene_tree->push_pck(scene_file);
		emit_signal("pck_started", p_pck_path);
		return;
	}
	EMIT_ERR_FAILD(p_pck_path);
}

PCKSceneTree *PCKRunner::_get_pck_scene_tree() {
	auto scene_tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (scene_tree) {
		return Object::cast_to<PCKSceneTree>(scene_tree);
	}
	return nullptr;
}

void PCKRunner::_add_autoloads(String p_pck_file) {
	auto settings = loaded_pcks[p_pck_file];

	ERR_FAIL_COND(settings == nullptr);

	auto autoloads = settings->get_autoload_list();

	for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : settings->get_autoload_list()) {
		const ProjectSettings::AutoloadInfo &info = E.value;

		if (info.is_singleton) {
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->add_global_constant(info.name, Variant());
			}
		}
	}

	auto scene_tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	List<Node *> to_add;
	for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : autoloads) {
		const ProjectSettings::AutoloadInfo &info = E.value;

		Node *n = nullptr;
		if (ResourceLoader::get_resource_type(info.path) == "PackedScene") {
			// Cache the scene reference before loading it (for cyclic references)
			Ref<PackedScene> scn;
			scn.instantiate();
			scn->set_path(info.path);
			scn->reload_from_file();
			ERR_CONTINUE_MSG(!scn.is_valid(), vformat("Can't autoload: %s.", info.path));

			if (scn.is_valid()) {
				n = scn->instantiate();
			}
		} else {
			Ref<Resource> res = ResourceLoader::load(info.path);
			ERR_CONTINUE_MSG(res.is_null(), vformat("Can't autoload: %s.", info.path));

			auto existed = scene_tree->get_root()->find_child(info.name, false, false);

			Ref<Script> script_res = res;
			if (script_res.is_valid()) {
				StringName ibt = script_res->get_instance_base_type();
				bool valid_type = ClassDB::is_parent_class(ibt, "Node");
				ERR_CONTINUE_MSG(!valid_type, vformat("Script does not inherit from Node: %s.", info.path));

				Object *obj = ClassDB::instantiate(ibt);

				ERR_CONTINUE_MSG(!obj, vformat("Cannot instance script for autoload, expected 'Node' inheritance, got: %s."));

				n = Object::cast_to<Node>(obj);
				n->set_script(script_res);
			}
		}

		settings->instanciated_autoloads.insert(info.name, n);

		ERR_CONTINUE_MSG(!n, vformat("Path in autoload not a node or script: %s.", info.path));
		n->set_name(info.name);

		//defer so references are all valid on _ready()
		to_add.push_back(n);

		if (info.is_singleton) {
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->add_global_constant(info.name, n);
			}
		}
	}
	for (Node *E : to_add) {
		scene_tree->get_root()->call_deferred("add_child", E);
	}
}

void PCKRunner::_remove_autoloads(String p_pck_file) {
	auto settings = loaded_pcks[p_pck_file];

	ERR_FAIL_COND(settings == nullptr);
	auto root = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop())->get_root();

	for (auto it = settings->instanciated_autoloads.begin(); it != settings->instanciated_autoloads.end(); ++it) {
		root->remove_child(it->value);
		it->value->queue_free();
	}

	settings->instanciated_autoloads.clear();
}

void reload_scripts(Ref<DirAccess> p_dir, const String &p_pck_file, const Vector<String> &p_script_extensions) {
	for (auto file : p_dir->get_files()) {
		if (p_script_extensions.has(file.get_extension())) {
			auto script_path = p_dir->get_current_dir().path_join(file);
			auto packed_file = PackedData::get_singleton()->try_get_packed_file(script_path);
			if (packed_file && packed_file->pack == p_pck_file) {
				Ref<Script> s = ResourceLoader::load(script_path, "Script");
				if (s->is_valid()) {
					s->reload();
				}
			}
		}
	}

	for (auto dir : p_dir->get_directories()) {
		auto dir_path = PackedData::get_singleton()->try_open_directory(p_dir->get_current_dir().path_join(dir));
		if (dir_path.is_valid()) {
			reload_scripts(dir_path, p_pck_file, p_script_extensions);
		}
	}
}

struct RunData {
	String pck_file;
	String main_scene;
	bool threaded_load;
	bool need_reload_scripts;
	RunData(const String &p_pck_file, const String &p_main_scene, bool p_threaded_load, bool p_need_reload_scripts) :
			pck_file(p_pck_file), main_scene(p_main_scene), threaded_load(p_threaded_load), need_reload_scripts(p_need_reload_scripts) {}
};

void PCKRunner::_run_pck(void *p_run_data) {
	auto run_data = (RunData *)p_run_data;
	String pck_file = run_data->pck_file;
	auto settings = get_singleton()->loaded_pcks[pck_file];
	String main_scene_path = settings->get_setting("application/run/main_scene", String());

	// reload scripts
	if (run_data->need_reload_scripts) {
		auto dir = PackedData::get_singleton()->try_open_directory("res://");
		if (dir.is_valid()) {
			Vector<String> script_extensions;
			script_extensions.resize(ScriptServer::get_language_count());
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				script_extensions.set(i, ScriptServer::get_language(i)->get_extension());
			}
			reload_scripts(dir, pck_file, script_extensions);
		}
	}

	// autoloads
	get_singleton()->_add_autoloads(pck_file);

	// main scenes
	if (!main_scene_path.is_empty()) {
		mtx.lock();
		get_singleton()->running_pcks.push_back(pck_file);
		mtx.unlock();
		if (run_data->threaded_load) {
			auto scene_tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
			EMIT_ERR_FAILD_COND(scene_tree == nullptr, pck_file);

			auto load_threaded_err = ResourceLoader::load_threaded_request(main_scene_path, "PackedScene", false);

			EMIT_ERR_COND(load_threaded_err != OK, pck_file, load_threaded_err);

			LoadingData data;
			data.main_scene_file = main_scene_path;
			data.pck_file = pck_file;
			mtx.lock();
			get_singleton()->threaded_loading_pcks.insert(pck_file, data);
			mtx.unlock();

			if (!scene_tree->is_connected("process_frame", callable_mp(get_singleton(), &PCKRunner::_on_process_frame))) {
				scene_tree->connect("process_frame", callable_mp(get_singleton(), &PCKRunner::_on_process_frame));
			}
		} else {
			MessageQueue::get_singleton()->push_callable(callable_mp(get_singleton(), &PCKRunner::_change_scene).bind(pck_file, main_scene_path));
			return;
		}
	} else {
		EMIT_ERR(pck_file, ERR_DOES_NOT_EXIST);
	}
}

void PCKRunner::run_pck(const String &p_pck_file, const bool p_thread_load) {
	const auto TEXT_CONFIG_FILE = String("res://project.godot");
	const auto BINARY_CONFIG_FILE = String("res://project.binary");

	bool need_reload_scripts = false;
	if (!get_singleton()->loaded_pcks.has(p_pck_file)) {
		auto text_md5 = FileAccess::get_md5(TEXT_CONFIG_FILE);
		auto binary_md5 = FileAccess::get_md5(BINARY_CONFIG_FILE);
		if (ProjectSettings::get_singleton()->call(StringName("load_resource_pack"), p_pck_file)) {
			auto pck_settings = SpikeProjectSettings::create();

			auto new_text_md5 = FileAccess::get_md5(TEXT_CONFIG_FILE);
			auto new_binary_md5 = FileAccess::get_md5(BINARY_CONFIG_FILE);
			if (new_binary_md5 != binary_md5) {
				pck_settings->load_settings_binary(BINARY_CONFIG_FILE);
			} else if (new_text_md5 != text_md5) {
				pck_settings->load_settings_text(TEXT_CONFIG_FILE);
			}
			mtx.lock();
			get_singleton()->loaded_pcks.insert(p_pck_file, pck_settings);
			mtx.unlock();
		}
	} else {
		need_reload_scripts = true;
	}

	auto settings = get_singleton()->loaded_pcks[p_pck_file];
	EMIT_ERR_FAILD_COND(settings == nullptr, p_pck_file);

	RunData run_data(p_pck_file, settings->get_setting("application/run/main_scene", String()), p_thread_load, need_reload_scripts);
	if (p_thread_load) {
		WorkerThreadPool::get_singleton()->add_native_task(&_run_pck, &run_data, true);
	} else {
		_run_pck(&run_data);
	}
}

void PCKRunner::stop_pck(int p_exit_code) {
	auto pck_scene_tree = get_singleton()->_get_pck_scene_tree();
	ERR_FAIL_COND(pck_scene_tree == nullptr);
	pck_scene_tree->pck_quit(p_exit_code);
}

Error PCKRunner::_load_binary_config(const String &p_file, HashMap<String, Variant> &r_settings) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_file, FileAccess::READ, &err);
	if (err != OK) {
		return err;
	}

	uint8_t hdr[4];
	f->get_buffer(hdr, 4);
	ERR_FAIL_COND_V_MSG((hdr[0] != 'E' || hdr[1] != 'C' || hdr[2] != 'F' || hdr[3] != 'G'), ERR_FILE_CORRUPT, "Corrupted header in binary project.binary (not ECFG).");

	uint32_t count = f->get_32();

	for (uint32_t i = 0; i < count; i++) {
		uint32_t slen = f->get_32();
		CharString cs;
		cs.resize(slen + 1);
		cs[slen] = 0;
		f->get_buffer((uint8_t *)cs.ptr(), slen);
		String key;
		key.parse_utf8(cs.ptr());

		uint32_t vlen = f->get_32();
		Vector<uint8_t> d;
		d.resize(vlen);
		f->get_buffer(d.ptrw(), vlen);
		Variant value;
		err = decode_variant(value, d.ptr(), d.size(), nullptr, true);
		ERR_CONTINUE_MSG(err != OK, "Error decoding property: " + key + ".");
		r_settings.insert(key, value);
	}

	return OK;
}

void PCKRunner::_on_process_frame() {
	for (auto it = threaded_loading_pcks.begin(); it != threaded_loading_pcks.end(); ++it) {
		auto pck = it->key;
		auto &data = it->value;
		data.status = ResourceLoader::load_threaded_get_status(data.main_scene_file, &data.progress);

		if (data.status != ResourceLoader::ThreadLoadStatus::THREAD_LOAD_IN_PROGRESS) {
			--it;
			threaded_loading_pcks.erase(pck);

			_change_scene(pck, data.main_scene_file);
			if (it == threaded_loading_pcks.end()) {
				break;
			}
		}
	}

	if (threaded_loading_pcks.size() == 0) {
		Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop())->disconnect(SNAME("process_frame"), callable_mp(this, &PCKRunner::_on_process_frame));
	}
}

float PCKRunner::get_threaded_load_progress(const String &p_pck_file) {
	String pck_file;
	if (p_pck_file.is_empty()) {
		pck_file = get_singleton()->running_pcks.back()->get();
	}
	if (get_singleton()->threaded_loading_pcks.has(pck_file)) {
		return get_singleton()->threaded_loading_pcks[pck_file].progress;
	}
	return 1.0;
}

PCKRunner::PCKRunner() {
	instance = this;
}

PCKRunner::~PCKRunner() {
	for (auto kv : loaded_pcks) {
		memdelete(kv.value);
	}
}

void PCKRunner::_finish_quit() {
	auto back = running_pcks.back()->get();
	running_pcks.pop_back();

	_remove_autoloads(back);

	threaded_loading_pcks.erase(back);
	emit_signal(SNAME("pck_stopped"), back);
}
