/**
 * spike_resource_importer_texture_atlas.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#ifndef SPIKE_RESOURCE_IMPORTER_TEXTURE_ATLAS_H
#define SPIKE_RESOURCE_IMPORTER_TEXTURE_ATLAS_H

#include "core/io/resource_importer.h"
#include "editor/editor_file_system.h"
#include <editor/import/resource_importer_texture_atlas.h>

class SpikeResourceImporterTextureAtlas : public ResourceImporterTextureAtlas {
	GDCLASS(SpikeResourceImporterTextureAtlas, ResourceImporterTextureAtlas);

	struct PackData {
		Rect2 region;
		bool is_cropped;
		bool is_mesh;
		Vector<int> chart_pieces; //one for region, many for mesh
		Vector<Vector<Vector2>> chart_vertices; //for mesh
		Ref<Image> image;
	};

public:
	virtual Error import_group_file(const String &p_group_file, const HashMap<String, HashMap<StringName, Variant>> &p_source_file_options, const HashMap<String, String> &p_base_paths) override;

	SpikeResourceImporterTextureAtlas();

#pragma region Implementing Scatter Map Change Tracking and Atlas Regeneration
private:
	void _connect_signal();
	void _get_resource_owners(const String &p_resource_path, EditorFileSystemDirectory *efsd, List<String> &r_owners);

	bool _atlas_file_changed(const String &changed_file_path, const String &target_atlas_path = "");
	void _regenerate_changed_atlas();

	void _collect_import_infos(const String &p_group_file, const Vector<String> &p_files,
			HashMap<String, HashMap<StringName, Variant>> &source_file_options, HashMap<String, String> &base_paths);

	void _delete_file(const String &file_to_delete);

	void _get_atlas_texture_source_file_recuresivly(EditorFileSystemDirectory *efsd, List<String> &r_files);
	void _dirs_and_files_before_remove(Vector<String> files_to_delete, Vector<String> dirs_to_delete);
	void _file_removed(const String &removed_file);
	void _folder_removed(const String &removed_folder);
	void _deferred_remove_and_scan();
	void _simulate_delete_file(const String &file_to_delete);
	void _on_resources_reimported(PackedStringArray resoueces);
	void _try_reimport();

	List<String> queue_reimport_atlas;

	HashMap<String, String> may_delete_source2atlas;
	HashMap<String, List<String>> may_delete_folder2atlas_list;

protected:
	static void _bind_methods();
#pragma endregion
};

#endif // RESOURCE_IMPORTER_TEXTURE_ATLAS_H
