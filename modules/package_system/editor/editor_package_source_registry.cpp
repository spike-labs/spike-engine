/**
 * editor_package_source_registry.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "core/io/zip_io.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/progress_dialog.h"
#include "editor_package_source.h"
#include "editor_package_system.h"

void PackageRegistryDownloader::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS:

			break;
	}
}

void PackageRegistryDownloader::_http_download_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data) {

	String status_text;
	String host = remote_url;

	switch (p_status) {
		case HTTPRequest::RESULT_CHUNKED_BODY_SIZE_MISMATCH:
		case HTTPRequest::RESULT_CONNECTION_ERROR:
		case HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED: {
			error_text = TTR("Connection error, please try again.");
			status_text = TTR("Can't connect.");
		} break;
		case HTTPRequest::RESULT_CANT_CONNECT:
		case HTTPRequest::RESULT_TLS_HANDSHAKE_ERROR: {
			error_text = TTR("Can't connect to host:") + " " + host;
			status_text = TTR("Can't connect.");
		} break;
		case HTTPRequest::RESULT_NO_RESPONSE: {
			error_text = TTR("No response from host:") + " " + host;
			status_text = TTR("No response.");
		} break;
		case HTTPRequest::RESULT_CANT_RESOLVE: {
			error_text = TTR("Can't resolve hostname:") + " " + host;
			status_text = TTR("Can't resolve.");
		} break;
		case HTTPRequest::RESULT_REQUEST_FAILED: {
			error_text = TTR("Request failed, return code:") + " " + itos(p_code);
			status_text = TTR("Request failed.");
		} break;
		case HTTPRequest::RESULT_DOWNLOAD_FILE_CANT_OPEN:
		case HTTPRequest::RESULT_DOWNLOAD_FILE_WRITE_ERROR: {
			error_text = TTR("Cannot save response to:") + " " + request->get_download_file();
			status_text = TTR("Write error.");
		} break;
		case HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED: {
			error_text = TTR("Request failed, too many redirects");
			status_text = TTR("Redirect loop.");
		} break;
		case HTTPRequest::RESULT_TIMEOUT: {
			error_text = TTR("Request failed, timeout");
			status_text = TTR("Timeout.");
		} break;
		default: {
			if (p_code != 200) {
				error_text = TTR("Request failed, return code:") + " " + itos(p_code);
				status_text = TTR("Failed:") + " " + itos(p_code);
			} else if (!sha256.is_empty()) {
				String download_sha256 = FileAccess::get_sha256(request->get_download_file());
				if (sha256 != download_sha256) {
					error_text = TTR("Bad download hash, assuming file has been tampered with.") + "\n";
					error_text += TTR("Expected:") + " " + sha256 + "\n" + TTR("Got:") + " " + download_sha256;
					status_text = TTR("Failed SHA-256 hash check");
				}
			}
		} break;
	}

	if (!error_text.is_empty()) {
		if (FileAccess::exists(download_file)) {
			DirAccess::remove_file_or_error(download_file);
		}
	}

	_flag_done.set();
}

void PackageRegistryDownloader::download(const String &url) {
	_flag_done.clear();
	remote_url = url;
	if (request == nullptr) {
		request = memnew(HTTPRequest);
		EditorPackageSystem::get_singleton()->add_child(request);
		request->set_use_threads(false);

		const String proxy_host = EDITOR_GET("network/http_proxy/host");
		const int proxy_port = EDITOR_GET("network/http_proxy/port");
		request->set_http_proxy(proxy_host, proxy_port);
		request->set_https_proxy(proxy_host, proxy_port);

		request->connect("request_completed", callable_mp(this, &PackageRegistryDownloader::_http_download_completed));
	} else {
		request->cancel_request();
	}
	request->set_download_file(download_file);
	auto code = request->request(url);
	if (code != OK) {
		_http_download_completed(0, code, PackedStringArray(), PackedByteArray());
	}
}

void PackageRegistryDownloader::start_download(const String & remote_url, const String &download_file, const String &save_path) {
	if (!is_inside_tree()) {
		EditorPackageSystem::get_singleton()->add_child(this);
	}
	setup(download_file, save_path);

	auto download_dir = download_file.get_base_dir();
	if (!DirAccess::exists(download_dir)) {
		DirAccess::create_for_path(download_dir)->make_dir(download_dir);
	}

	download(remote_url);
}


EditorPackageSourceRegistry::EditorPackageSourceRegistry(const String &name, const String &registry, const String &version) :
		EditorPackageSource(name) {
	download = nullptr;

	this->registry = registry;
	this->version = version;
}

String EditorPackageSourceRegistry::get_tag() {
	return "registry:" + version;
}

Error EditorPackageSourceRegistry::fetch_source(const String &save_path, String& err_info) {
	Error err = FAILED;
	String download_file;
	if (registry.find("://") < 0) {
		download_file = PATH_JOIN(registry, repo_name + "@" + version + ".zip");
	} else {
		download_file = get_cache_dir().path_join(repo_name + "@" + version + ".zip");
	}

	bool success = FileAccess::exists(download_file);
	if (!success) {
		if (download == nullptr) {
			download = memnew(PackageRegistryDownloader);
		}
		auto remote_url = registry + repo_name + "/" + version + ".zip";
		MessageQueue::get_singleton()->push_callable(
			callable_mp(download, &PackageRegistryDownloader::start_download),
			remote_url, download_file, save_path
		);

		while (!download->is_done()) {
			int total = download->get_total_size();
			int current = download->get_current_size();
			auto step = (float)current / total * 100;
			_step.set((int)step);
		}

		err_info = download->get_error();
		download->queue_free();
		success = IS_EMPTY(err_info);
		if (!success) {
			err = ERR_FILE_CORRUPT;
			DirAccess::remove_file_or_error(download_file);
			err_info = vformat(STTR("An error occurred while downloading package archive.\n%s"), err_info);
		}
	}

	if (success) {
		err = install(download_file, save_path);
		if (err != OK) {
			err_info = vformat(STTR("An error(#%d) occurred while unzip package archive."), (int)err);
			DirAccess::remove_file_or_error(download_file);
		}
	}
	return err;
}

void EditorPackageSourceRegistry::get_task_step(String &state, int &step) {
	step = _step.get();
	if (step > 100) {
		step -= 100;
		state = STTR("Uncompressing") + " " + repo_name;
	} else {
		state = STTR("Downloading") + " " + repo_name;
	}
}

Error EditorPackageSourceRegistry::install(const String &download_file, const String &install_path) {
	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);

	unzFile pkg = unzOpen2(download_file.utf8().get_data(), &io);
	if (pkg == nullptr) {
		DirAccess::remove_file_or_error(download_file);
		return ERR_FILE_CORRUPT;
	}

	Ref<DirAccess> install_dir = DirAccess::create_for_path(install_path);
	if (install_dir.is_null()) {
		return ERR_CANT_CREATE;
	}

	int total = 0;
	for (int i = unzGoToFirstFile(pkg); i == UNZ_OK; i = unzGoToNextFile(pkg)) {
		total += 1;
	}

	int idx = 0;
	int ret = unzGoToFirstFile(pkg);
	while (ret == UNZ_OK) {
		unz_file_info info;
		char fname[16384];
		ret = unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);
		if (ret != UNZ_OK) {
			//break;
			unzClose(pkg);
			return ERR_INVALID_DATA;
		}

		String name = String::utf8(fname);
		auto file = install_path.path_join(name);
		/* make sure this folder exists */
		String dir_name = file.get_base_dir();
		if (!install_dir->dir_exists(dir_name)) {
			Error dir_err = install_dir->make_dir_recursive(dir_name);
			if (dir_err) {
				unzClose(pkg);
				return ERR_CANT_CREATE;
			}
		}

		if (!IS_EMPTY(file.get_file())) {
			Vector<uint8_t> data;
			data.resize(info.uncompressed_size);
			if (unzOpenCurrentFile(pkg)) {
				unzClose(pkg);
				OS::get_singleton()->move_to_trash(dir_name);
				return ERR_FILE_CANT_OPEN;
			}
			if (unzReadCurrentFile(pkg, data.ptrw(), data.size()) < 0) {
				unzClose(pkg);
				OS::get_singleton()->move_to_trash(dir_name);
				return ERR_FILE_CANT_READ;
			}
			if (unzCloseCurrentFile(pkg)) {
				unzClose(pkg);
				OS::get_singleton()->move_to_trash(dir_name);
				return ERR_FILE_UNRECOGNIZED;
			}

			/* write the file */
			{
				Ref<FileAccess> f = FileAccess::open(file, FileAccess::WRITE);
				if (f.is_null()) {
					unzClose(pkg);
					return ERR_CANT_CREATE;
				};
				f->store_buffer(data.ptr(), data.size());
			}
		}

		idx++;
		ret = unzGoToNextFile(pkg);
		_step.set(100 + (int)((float)idx / total * 100));
	}

	unzClose(pkg);

	return OK;
}

String EditorPackageSourceRegistry::find_registry(const String &pkg_name, const Ref<ConfigFile> cfg_file) {
	// First search in engin editor folder.
	String exe_path = OS::get_singleton()->get_executable_path().get_base_dir();
	String local_path = PATH_JOIN(PATH_JOIN(exe_path, "Packages"), "LocalRegistry");
	String local_pkg_path = PATH_JOIN(local_path, pkg_name);
	if (FileAccess::exists(local_pkg_path)) {
		return local_path;
	}

	List<String> sections;
	cfg_file->get_sections(&sections);
	for (auto section : sections) {
		if (!cfg_file->has_section_key(section, "url"))
			continue;

		if (!cfg_file->has_section_key(section, "scopes"))
			continue;

		auto array = cfg_file->get_value(section, "scopes");
		if (array.get_type() == Variant::ARRAY) {
			auto scopes = array.operator Array();
			for (size_t i = 0; i < scopes.size(); i++) {
				auto scope = scopes[i].operator String();
				if (!IS_EMPTY(scope) && pkg_name.begins_with(scope)) {
					return cfg_file->get_value(section, "url");
				}
			}
		}
	}
	return String(DEFAULT_REGISTRY_URL);
}
