/**
 * editor_package_source.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/io/config_file.h"
#include "core/string/ustring.h"
#include "scene/main/http_request.h"
#include "scene/main/node.h"
#include "spike_define.h"

class EditorPackageSource {
	friend class EditorPluginSystem;

public:
	String repo_name;
	virtual Error fetch_source(const String &save_path, String &err_info) = 0;
	virtual void get_task_step(String &state, int &step);
	virtual String get_tag();

	EditorPackageSource(const String &name) {
		repo_name = name;
	}

	virtual ~EditorPackageSource();

	static String get_cache_dir();
};

class EditorPackageSourceGit : public EditorPackageSource {
private:
	String repo_url;
	String branch;
	String subpath;

	String progress_info;
	Mutex progress_mutex;

	Map<int, float> progress_map;
	Ref<FileAccess> lfs_log_file;

	String clone_repo(const String &p_repo_dir, const String &p_workspace);
	String checkout_repo(const String &p_workspace);

public:
	virtual Error fetch_source(const String &save_path, String &err_info) override;
	virtual void get_task_step(String &state, int &step) override;
	virtual String get_tag() override;

public:
	EditorPackageSourceGit(const String &name, const String &source);
	virtual ~EditorPackageSourceGit();
};

class EditorPackageSourceLocal : public EditorPackageSource {
private:
	String repo_url;
	String param;

public:
	virtual Error fetch_source(const String &save_path, String &err_info) override;
	virtual String get_tag() override;

public:
	EditorPackageSourceLocal(const String &name, const String &source);
};

class PackageRegistryDownloader : public Node {
	GDCLASS(PackageRegistryDownloader, Node)

	String sha256;
	HTTPRequest *request;

	String remote_url;
	String download_file;
	String install_path;

	SafeFlag _flag_done;
	String error_text;

protected:
	void _notification(int p_what);

	void _http_download_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data);

public:
	PackageRegistryDownloader() {
		_flag_done.clear();
		request = nullptr;
	}

	~PackageRegistryDownloader() {
		if (request) {
			request->queue_free();
			request = nullptr;
		}
	}

	bool is_done() { return _flag_done.is_set(); }
	String get_error() { return error_text; }
	int get_total_size() { return request ? request->get_body_size() : 0; }
	int get_current_size() { return request ? request->get_downloaded_bytes() : 0; }

	void setup(const String &file, const String &path) {
		this->download_file = file;
		this->install_path = path;
	}

	void download(const String &url);

	void start_download(const String &remote_url, const String &download_file, const String &save_path);
};

class EditorPackageSourceRegistry : public EditorPackageSource {
private:
	String registry;
	String version;
	PackageRegistryDownloader *download;
	SafeNumeric<int> _step;

public:
	EditorPackageSourceRegistry(const String &name, const String &registry, const String &version);

	virtual Error fetch_source(const String &save_path, String &err_info) override;
	virtual void get_task_step(String &state, int &step) override;
	virtual String get_tag() override;

	Error install(const String &file, const String &path);

	static String find_registry(const String &pkg_name, const Ref<ConfigFile> cfg_file);
};
