/**
 * modular_data.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "spike_define.h"

#include "core/io/resource.h"
#include "core/object/gdvirtual.gen.inc"
#include "core/object/script_language.h"
#include "filesystem_server/file_provider.h"
#include "scene/gui/popup_menu.h"
#include "scene/resources/packed_scene.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_paths.h"
#define EDITOR_CACHE EditorPaths::get_singleton()->get_cache_dir()
#define EXPORT_CACHE PATH_JOIN(EDITOR_CACHE, "modular")
#endif

#define ANY_MODULE "*"
#define TEXT_MODULE ".text"
#define CONF_MODULE ".conf"
#define UNANNO_MODULE ".unannotated"
#define PCK_CACHE_FILE(url) vformat("%s/modulardata_%s.%s", EDITOR_CACHE, url.md5_text(), url.get_extension())

#define REQUIRE_TOOL_SCRIPT(self)                          \
	do {                                                   \
		auto inst = (self)->get_script_instance();         \
		if (inst && !inst->get_script()->is_tool()) {      \
			ERR_PRINT("The script is not a tool script!"); \
		}                                                  \
	} while (0);

class ModularData : public Resource {
	GDCLASS(ModularData, Resource);

protected:
	String export_name;
	GDVIRTUAL0(_validation)
	GDVIRTUAL1(_edit_data, Object *)
	GDVIRTUAL0R(String, _get_module_name)
	GDVIRTUAL0R(String, _export_data)
	GDVIRTUAL0R(String, _get_export_name)
	GDVIRTUAL0R(Dictionary, _get_module_info)
	GDVIRTUAL1(_inspect_modular, Node *)

	static void _bind_methods();

public:
	void _set_export_name(const String &p_path);
	String _get_export_name() const;

	virtual void validation();
	virtual String get_module_name() const;
	virtual void edit_data(Object *p_context);
	virtual String export_data();
	virtual String get_export_name();
	virtual Dictionary get_module_info();
	virtual void inspect_modular(Node *p_parent);
	bool has_module(const String &p_module) {
		return get_module_info().has(p_module);
	}
};

class ModularResource : public ModularData {
	GDCLASS(ModularResource, ModularData);

protected:
	int ext = 0;
	Array resources;

	String module_name = "";
	String module_comment = "";

	Ref<PackedScene> main_scene;

	static void _bind_methods();

public:
	void set_module_name(const String &p_name);
	virtual String get_module_name() const override;

	void set_module_comment(const String &p_comment);
	String get_module_comment() const;

	void set_main_scene(const Ref<PackedScene> &p_scene);
	Ref<PackedScene> get_main_scene() const;
	void set_resources(const Array &p_resources);
	Array get_resources();
	void set_ext(int p_ext);
	int get_ext() const;
	virtual void edit_data(Object *p_context) override;
	virtual String export_data() override;
	virtual void inspect_modular(Node *p_parent);
	void change_main_scene(const Ref<PackedScene> &p_scene, Node *p_node);
};

class ModularPackedFile : public ModularData {
	GDCLASS(ModularPackedFile, ModularData);

public:
	enum FileSource {
		CLEAR_FILE,
		LOCAL_FILE,
		REMOTE_FILE,
		RELOAD_REMOTE_FILE,
	};

private:
	uint64_t _provider_mtime;
	Ref<FileProvider> _provider;
	Dictionary module_info;
	bool module_info_loaded = false;

protected:
	String file_path;

	GDVIRTUAL0R(String, _get_local_path)

	static void _bind_methods();

	void _menu_button_pressed(PopupMenu *p_menu);

public:
	void _set_file_path(const String &p_path);

	Ref<FileProvider> get_provider(bool *r_is_new = nullptr);
	String get_save_path() const;
	String get_local_path() const;
	String get_cache_dir() const;
	void queue_download_file(const String &p_url);
	virtual void validation() override;
	virtual String get_module_name() const override;
	virtual void edit_data(Object *p_context) override;
	virtual String export_data() override;
	virtual String get_export_name() override;
	virtual Dictionary get_module_info() override;
	virtual void inspect_modular(Node *p_parent) override;
};

struct ModularDataHasher {
	static _FORCE_INLINE_ uint32_t hash(const Ref<ModularData> &p_data) {
		Ref<ModularPackedFile> data = p_data;
		if (data.is_valid()) {
			return data->get_save_path().hash();
		}
		return hash_one_uint64((uint64_t)p_data.ptr());
	}
};

struct ModularDataComparator {
	static bool compare(const Ref<ModularData> &p_lhs, const Ref<ModularData> &p_rhs) {
		if (p_lhs == p_rhs)
			return true;

		Ref<ModularPackedFile> lhs = p_lhs;
		Ref<ModularPackedFile> rhs = p_rhs;
		if (lhs.is_valid() && rhs.is_valid() && lhs->get_save_path() == rhs->get_save_path()) {
			return true;
		}

		return false;
	}
};