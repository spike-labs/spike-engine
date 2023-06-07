/**
 * modular_export_utils.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#pragma once
#include "editor/editor_file_system.h"
#include "modular_data.h"

class ModularExportUtils {
public:
	static void collect_resources(EditorFileSystemDirectory *p_fs, Vector<Ref<Resource>> &r_res_list, Vector<String> &r_file_list);
	static String export_specific_resources(const Ref<PackedScene> &p_main_scene, const Vector<Ref<Resource>> &p_res_list, const Vector<String> &p_file_list, bool p_exclude_package_files, int p_ext);
};