/**
 * editor_package_source_git.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "editor_package_source.h"

#include "core/config/project_settings.h"
#include "editor/editor_node.h"

EditorPackageSourceGit::EditorPackageSourceGit(const String &name, const String &source) :
		EditorPackageSource(name) {
	if (source.begins_with("http") && source.contains("://") && source.contains(".git")) {
		repo_url = source;
		int sidx = repo_url.rfind("?");
		if (sidx > 0) {
			String params = repo_url.substr(sidx + 1);
			auto segments = params.split("&");
			for (auto &seg : segments) {
				auto kv = seg.split("=");
				if (kv.size() == 2) {
					if (kv[0] == "path") {
						subpath = kv[1];
					}
				}
			}
			repo_url = repo_url.substr(0, sidx);
		}

		int index = repo_url.rfind("#");
		if (index > 0) {
			branch = repo_url.substr(index + 1);
			repo_url = repo_url.substr(0, index);
		}
	}
}

EditorPackageSourceGit::~EditorPackageSourceGit() {
	if (lfs_log_file.is_valid()) {
		lfs_log_file.unref();
	}
}

String EditorPackageSourceGit::get_tag() {
	String tag = "git:" + branch;
	if (!IS_EMPTY(subpath)) {
		tag = PATH_JOIN(tag, subpath);
	}
	return tag;
}

void EditorPackageSourceGit::get_task_step(String &state, int &step) {
	step = 0;

	auto frames = Engine::get_singleton()->get_process_frames();
	auto suffix_idx = frames % 80 / 20;
	const char *c_suffix_arr[] = {
		"",
		" .",
		" . .",
		" . . .",
	};
	String suffix = c_suffix_arr[suffix_idx];

	if (lfs_log_file.is_valid()) {
		while (lfs_log_file->get_position() < lfs_log_file->get_length()) {
			auto line = lfs_log_file->get_line();
			// line = "download {i}/{n} {download}/{total} {file_path}"
			auto segments = line.split_spaces();
			if (segments.size() == 4) {
				auto indices = segments[1].split_ints("/");
				auto sizes = segments[2].split_ints("/");
				if (indices.size() == 2 && sizes.size() == 2) {
					int i = indices[0];
					int n = indices[1];
					int bytes = sizes[0];
					int total = sizes[1];

					progress_map[i] = (float)bytes / total / n;
				}
			}
		}
		float progress = 0;
		for (auto p : progress_map) {
			progress += p.value;
		}
		step = floor(progress * 100);
		state = "Downloading LFS objects" + suffix;
		return;
	}

	{
		MutexLock progress_lock = MutexLock(progress_mutex);
		String progress_info_str = progress_info;
		int index = progress_info_str.rfind("%");
		if (index >= 0) {
			int n = 1;
			int num_idx = index - 1;
			while (num_idx > 0) {
				char32_t c = progress_info_str[num_idx];
				if (!is_digit(c))
					break;

				step += n * (c - '0');
				num_idx--;
				n *= 10;
			}

			num_idx--;
			int start_idx = num_idx;
			while (start_idx > 0 && !is_control(progress_info_str[start_idx])) {
				start_idx--;
			}
			state = progress_info_str.substr(start_idx + 1, num_idx - start_idx) + suffix;
			return;
		}
	}

	state = repo_name + suffix;
	step = 100;
}

String EditorPackageSourceGit::clone_repo(const String &p_repo_dir, const String &p_workspace) {
	String cmd = "git";
	int exitcode = 255;

	List<String> args;
	args.push_back("clone");
	args.push_back("--progress");
	args.push_back(repo_url);
	args.push_back(p_workspace);
	if (!IS_EMPTY(branch)) {
		args.push_back("-b");
		args.push_back(branch);
	}
	args.push_back("--depth");
	args.push_back("1");
	args.push_back("--separate-git-dir");
	args.push_back(p_repo_dir);

	progress_info.clear();

	String error;
	Error ret = OS::get_singleton()->execute(cmd, args, &progress_info, &exitcode, true, &progress_mutex);
	if (ret == OK) {
		if (exitcode == 0) {
			String git_lfs_progress = OS::get_singleton()->get_environment("GIT_LFS_PROGRESS");
			String lfs_log_path = PATH_JOIN(p_repo_dir, ".lfs_pull.log");
			OS::get_singleton()->set_environment("GIT_LFS_PROGRESS", lfs_log_path);
			{
				FileAccess::open(lfs_log_path, FileAccess::WRITE);
			}
			lfs_log_file = FileAccess::open(lfs_log_path, FileAccess::READ);

			args.clear();
			args.push_back("-C");
			args.push_back(p_workspace);
			args.push_back("lfs");
			args.push_back("pull");
			ret = OS::get_singleton()->execute(cmd, args, &progress_info, &exitcode, true, &progress_mutex);

			OS::get_singleton()->set_environment("GIT_LFS_PROGRESS", git_lfs_progress);
		} else {
			ret = FAILED;
			auto at = progress_info.rfind(":");
			if (at < 0) {
				error = "unknown error#" + itos(exitcode);
			} else {
				error = progress_info.substr(at + 1);
			}
		}
	} else {
		error = itos(ret);
	}

	return error;
}

String EditorPackageSourceGit::checkout_repo(const String &p_workspace) {
	List<String> args;
	args.push_back("-C");
	args.push_back(p_workspace);
	args.push_back("checkout");
	args.push_back(".");

	int exitcode = 255;
	Mutex output_mutex;

	String error;
	Error ret = OS::get_singleton()->execute("git", args, &progress_info, &exitcode, true, &output_mutex);
	if (ret == OK) {
		if (exitcode != 0) {
			ret = FAILED;
			auto at = progress_info.rfind(":");
			if (at < 0) {
				error = "unknown error#" + itos(exitcode);
			} else {
				error = progress_info.substr(at + 1);
			}
		}
	} else {
		error = itos(ret);
	}
	return error;
}

Error EditorPackageSourceGit::fetch_source(const String &p_save_path, String &err_info) {
	Error ret = FAILED;
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	String cache_path = ProjectSettings::get_singleton()->globalize_path(get_cache_dir());
	String repo_path = PATH_JOIN(cache_path, repo_name);
	if (!IS_EMPTY(branch)) {
		repo_path += "@" + branch;
	}

	bool has_subpath = !IS_EMPTY(this->subpath);
	String ws_path = has_subpath ? repo_path + ".ws" : p_save_path;

	if (!da->dir_exists(ws_path)) {
		ret = da->make_dir(ws_path);
		if (ret != OK) {
			err_info = vformat(STTR("An error(#%d) occurred while loading package from git repositoy."), (int)ret);
			return ret;
		}
	}

	bool is_stdout_open = OS::get_singleton()->is_stdout_enabled();
	bool is_stderr_open = OS::get_singleton()->is_stderr_enabled();
	OS::get_singleton()->set_stdout_enabled(true);
	OS::get_singleton()->set_stderr_enabled(true);

	String git_error;
	if (da->dir_exists(repo_path)) {
		String ws_git_file = PATH_JOIN(ws_path, ".git");
		if (!FileAccess::exists(ws_git_file)) {
			{
				FileAccess::open(ws_git_file, FileAccess::WRITE)->store_string("gitdir: " + repo_path);
			}
			git_error = checkout_repo(ws_path);
			ret = IS_EMPTY(git_error) ? OK : FAILED;
		} else {
			ret = OK;
		}
	} else {
		git_error = clone_repo(repo_path, ws_path);
		ret = IS_EMPTY(git_error) ? OK : FAILED;
	}

	OS::get_singleton()->set_stderr_enabled(is_stdout_open);
	OS::get_singleton()->set_stdout_enabled(is_stderr_open);

	if (ret == OK && has_subpath) {
		ret = da->copy_dir(PATH_JOIN(ws_path, subpath), p_save_path);
	}

	String git_file = p_save_path.path_join(".git");
	if (da->file_exists(git_file)) {
		if (da->remove(git_file) != OK) {
			ELog("Cannot remove file or directory: %s", git_file);
		}
	}

	if (ret != OK) {
		err_info = vformat(STTR("An error(#%d) occurred while loading package from git repositoy."), (int)ret);
		if (!IS_EMPTY(git_error)) {
			err_info = err_info + "\n" + git_error;
		}
		if (DirAccess::exists(repo_path)) {
			OS::get_singleton()->move_to_trash(repo_path);
		}
		if (DirAccess::exists(ws_path)) {
			OS::get_singleton()->move_to_trash(ws_path);
		}
	}

	return ret;
}
