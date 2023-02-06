/**
 * spike_resource_importer_texture_atlas.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "spike_resource_importer_texture_atlas.h"

#include "../spike_dependency_remove_dialog.h"
#include "../spike_filesystem_dock.h"
#include "spike_define.h"
#include "core/io/file_access.h"
#include "core/io/image_loader.h"
#include "core/io/resource_saver.h"
#include "editor/editor_atlas_packer.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"

#include "../spike_filesystem_dock.h"
#include "../spike_editor_node.h"
#include "core/io/config_file.h"
#include "editor/editor_node.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/import_dock.h"

#include "core/math/geometry_2d.h"



String convert_to_real_path(const String &p_path) {
	auto ruid = ResourceLoader::get_resource_uid(p_path);
	if (ruid != ResourceUID::INVALID_ID && ResourceUID::get_singleton()->has_id(ruid)) {
		return ResourceUID::get_singleton()->get_id_path(ruid);
	}
	return p_path;
}

String get_resource_impoter_name(const String &p_res_file_path) {
	auto import_file_path = p_res_file_path + ".import";
	ERR_FAIL_COND_V_MSG(!FileAccess::exists(import_file_path), "", p_res_file_path + " has not be imported.");
	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(import_file_path);
	ERR_FAIL_COND_V_MSG(err != OK, "", ("Load '" + import_file_path + "' failed, error code: " + itos(err) + "."));
	ERR_FAIL_COND_V_MSG(!config->has_section_key("remap", "importer"), "", "Invalid .import file: " + import_file_path);
	return config->get_value("remap", "importer");
}

static void _plot_triangle(Vector2i *vertices, const Vector2i &p_offset, bool p_transposed, Ref<Image> p_image, const Ref<Image> &p_src_image) {
	int width = p_image->get_width();
	int height = p_image->get_height();
	int src_width = p_src_image->get_width();
	int src_height = p_src_image->get_height();

	int x[3];
	int y[3];

	for (int j = 0; j < 3; j++) {
		x[j] = vertices[j].x;
		y[j] = vertices[j].y;
	}

	// sort the points vertically
	if (y[1] > y[2]) {
		SWAP(x[1], x[2]);
		SWAP(y[1], y[2]);
	}
	if (y[0] > y[1]) {
		SWAP(x[0], x[1]);
		SWAP(y[0], y[1]);
	}
	if (y[1] > y[2]) {
		SWAP(x[1], x[2]);
		SWAP(y[1], y[2]);
	}

	double dx_far = double(x[2] - x[0]) / (y[2] - y[0] + 1);
	double dx_upper = double(x[1] - x[0]) / (y[1] - y[0] + 1);
	double dx_low = double(x[2] - x[1]) / (y[2] - y[1] + 1);
	double xf = x[0];
	double xt = x[0] + dx_upper; // if y[0] == y[1], special case
	int max_y = MIN(y[2], height - p_offset.y - 1);
	for (int yi = y[0]; yi < max_y; yi++) {
		if (yi >= 0) {
			for (int xi = (xf > 0 ? int(xf) : 0); xi < (xt <= src_width ? xt : src_width); xi++) {
				int px = xi, py = yi;
				int sx = px, sy = py;
				sx = CLAMP(sx, 0, src_width - 1);
				sy = CLAMP(sy, 0, src_height - 1);
				Color color = p_src_image->get_pixel(sx, sy);
				if (p_transposed) {
					SWAP(px, py);
				}
				px += p_offset.x;
				py += p_offset.y;

				//may have been cropped, so don't blit what is not visible?
				if (px < 0 || px >= width) {
					continue;
				}
				if (py < 0 || py >= height) {
					continue;
				}
				p_image->set_pixel(px, py, color);
			}

			for (int xi = (xf < src_width ? int(xf) : src_width - 1); xi >= (xt > 0 ? xt : 0); xi--) {
				int px = xi, py = yi;
				int sx = px, sy = py;
				sx = CLAMP(sx, 0, src_width - 1);
				sy = CLAMP(sy, 0, src_height - 1);
				Color color = p_src_image->get_pixel(sx, sy);
				if (p_transposed) {
					SWAP(px, py);
				}
				px += p_offset.x;
				py += p_offset.y;

				//may have been cropped, so don't blit what is not visible?
				if (px < 0 || px >= width) {
					continue;
				}
				if (py < 0 || py >= height) {
					continue;
				}
				p_image->set_pixel(px, py, color);
			}
		}
		xf += dx_far;
		if (yi < y[1]) {
			xt += dx_upper;
		} else {
			xt += dx_low;
		}
	}
}

Error SpikeResourceImporterTextureAtlas::import_group_file(const String &p_group_file, const HashMap<String, HashMap<StringName, Variant>> &p_source_file_options, const HashMap<String, String> &p_base_paths) {
	ERR_FAIL_COND_V(p_source_file_options.size() == 0, ERR_BUG); //should never happen

	Vector<EditorAtlasPacker::Chart> charts;
	Vector<PackData> pack_data_files;

	pack_data_files.resize(p_source_file_options.size());

	int idx = 0;
	for (const KeyValue<String, HashMap<StringName, Variant>> &E : p_source_file_options) {
		PackData &pack_data = pack_data_files.write[idx];
		const String &source = E.key;
		const HashMap<StringName, Variant> &options = E.value;

		Ref<Image> image;
		image.instantiate();
		Error err = ImageLoader::load_image(source, image);
		ERR_CONTINUE(err != OK);

		pack_data.image = image;
		pack_data.is_cropped = options["crop_to_region"];

		int mode = options["import_mode"];
		bool trim_alpha_border_from_region = options["trim_alpha_border_from_region"];

		if (mode == IMPORT_MODE_REGION) {
			pack_data.is_mesh = false;

			EditorAtlasPacker::Chart chart;

			Rect2i used_rect = Rect2i(Vector2i(), image->get_size());
			if (trim_alpha_border_from_region) {
				// Clip a region from the image.
				used_rect = image->get_used_rect();
			}
			pack_data.region = used_rect;

			chart.vertices.push_back(used_rect.position);
			chart.vertices.push_back(used_rect.position + Vector2i(used_rect.size.x, 0));
			chart.vertices.push_back(used_rect.position + Vector2i(used_rect.size.x, used_rect.size.y));
			chart.vertices.push_back(used_rect.position + Vector2i(0, used_rect.size.y));
			EditorAtlasPacker::Chart::Face f;
			f.vertex[0] = 0;
			f.vertex[1] = 1;
			f.vertex[2] = 2;
			chart.faces.push_back(f);
			f.vertex[0] = 0;
			f.vertex[1] = 2;
			f.vertex[2] = 3;
			chart.faces.push_back(f);
			chart.can_transpose = false;
			pack_data.chart_vertices.push_back(chart.vertices);
			pack_data.chart_pieces.push_back(charts.size());
			charts.push_back(chart);

		} else {
			pack_data.is_mesh = true;

			Ref<BitMap> bit_map;
			bit_map.instantiate();
			bit_map->create_from_image_alpha(image);
			Vector<Vector<Vector2>> polygons = bit_map->clip_opaque_to_polygons(Rect2(0, 0, image->get_width(), image->get_height()));

			for (int j = 0; j < polygons.size(); j++) {
				EditorAtlasPacker::Chart chart;
				chart.vertices = polygons[j];
				chart.can_transpose = true;

				Vector<int> poly = Geometry2D::triangulate_polygon(polygons[j]);
				for (int i = 0; i < poly.size(); i += 3) {
					EditorAtlasPacker::Chart::Face f;
					f.vertex[0] = poly[i + 0];
					f.vertex[1] = poly[i + 1];
					f.vertex[2] = poly[i + 2];
					chart.faces.push_back(f);
				}

				pack_data.chart_pieces.push_back(charts.size());
				charts.push_back(chart);

				pack_data.chart_vertices.push_back(polygons[j]);
			}
		}
		idx++;
	}

	//pack the charts
	int atlas_width, atlas_height;
	EditorAtlasPacker::chart_pack(charts, atlas_width, atlas_height);

	//blit the atlas
	Ref<Image> new_atlas = Image::create_empty(atlas_width, atlas_height, false, Image::FORMAT_RGBA8);

	// Detect source changes to regenerate old atlas
	bool need_reimport = false;
	for (const KeyValue<String, HashMap<StringName, Variant>> &E : p_source_file_options) {
		if (_atlas_file_changed(E.key, p_group_file)) {
			need_reimport = true;
		}
	}

	for (int i = 0; i < pack_data_files.size(); i++) {
		PackData &pack_data = pack_data_files.write[i];

		for (int j = 0; j < pack_data.chart_pieces.size(); j++) {
			const EditorAtlasPacker::Chart &chart = charts[pack_data.chart_pieces[j]];
			for (int k = 0; k < chart.faces.size(); k++) {
				Vector2i positions[3];
				for (int l = 0; l < 3; l++) {
					int vertex_idx = chart.faces[k].vertex[l];
					positions[l] = Vector2i(chart.vertices[vertex_idx]);
				}

				_plot_triangle(positions, Vector2i(chart.final_offset), chart.transposed, new_atlas, pack_data.image);
			}
		}
	}

	auto real_path = convert_to_real_path(p_group_file);

	//save the atlas
	auto err = new_atlas->save_png(real_path);
	ERR_FAIL_COND_V_MSG(err, err, ("Save atlas file failed: " + real_path));

	//update cache if existing, else create
	Ref<Texture2D> cache;
	cache = ResourceCache::get_ref(real_path);
	if (!cache.is_valid()) {
		Ref<ImageTexture> res_cache = ImageTexture::create_from_image(new_atlas);
		res_cache->set_path(real_path);
		cache = res_cache;
	}

	//save the images
	idx = 0;
	for (const KeyValue<String, HashMap<StringName, Variant>> &E : p_source_file_options) {
		PackData &pack_data = pack_data_files.write[idx];

		Ref<Texture2D> texture;

		if (!pack_data.is_mesh) {
			Vector2 offset = charts[pack_data.chart_pieces[0]].vertices[0] + charts[pack_data.chart_pieces[0]].final_offset;

			//region
			Ref<AtlasTexture> atlas_texture;
			atlas_texture.instantiate();
			atlas_texture->set_atlas(cache);
			atlas_texture->set_region(Rect2(offset, pack_data.region.size));

			if (!pack_data.is_cropped) {
				atlas_texture->set_margin(Rect2(pack_data.region.position, pack_data.image->get_size() - pack_data.region.size));
			}

			texture = atlas_texture;
		} else {
			Ref<ArrayMesh> mesh;
			mesh.instantiate();

			for (int i = 0; i < pack_data.chart_pieces.size(); i++) {
				const EditorAtlasPacker::Chart &chart = charts[pack_data.chart_pieces[i]];
				Vector<Vector2> vertices;
				Vector<int> indices;
				Vector<Vector2> uvs;
				int vc = chart.vertices.size();
				int fc = chart.faces.size();
				vertices.resize(vc);
				uvs.resize(vc);
				indices.resize(fc * 3);

				{
					Vector2 *vw = vertices.ptrw();
					int *iw = indices.ptrw();
					Vector2 *uvw = uvs.ptrw();

					for (int j = 0; j < vc; j++) {
						vw[j] = chart.vertices[j];
						Vector2 uv = chart.vertices[j];
						if (chart.transposed) {
							SWAP(uv.x, uv.y);
						}
						uv += chart.final_offset;
						uv /= new_atlas->get_size(); //normalize uv to 0-1 range
						uvw[j] = uv;
					}

					for (int j = 0; j < fc; j++) {
						iw[j * 3 + 0] = chart.faces[j].vertex[0];
						iw[j * 3 + 1] = chart.faces[j].vertex[1];
						iw[j * 3 + 2] = chart.faces[j].vertex[2];
					}
				}

				Array arrays;
				arrays.resize(Mesh::ARRAY_MAX);
				arrays[Mesh::ARRAY_VERTEX] = vertices;
				arrays[Mesh::ARRAY_TEX_UV] = uvs;
				arrays[Mesh::ARRAY_INDEX] = indices;

				mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
			}

			Ref<MeshTexture> mesh_texture;
			mesh_texture.instantiate();
			mesh_texture->set_base_texture(cache);
			mesh_texture->set_image_size(pack_data.image->get_size());
			mesh_texture->set_mesh(mesh);

			texture = mesh_texture;
		}

		String save_path = p_base_paths[E.key] + ".res";
		ResourceSaver::save(texture, save_path);
		idx++;
	}

	if (need_reimport) {
		_try_reimport();
	}

	if (!FileAccess::exists(p_group_file + ".import")) {
		EditorFileSystem::get_singleton()->scan_changes();
	}
	return OK;
}



#pragma region 实现散图的变化跟踪与图集重新生成
SpikeResourceImporterTextureAtlas::SpikeResourceImporterTextureAtlas() {
	MessageQueue::get_singleton()->push_callable(callable_mp(this, &SpikeResourceImporterTextureAtlas::_connect_signal));
}

void SpikeResourceImporterTextureAtlas::_bind_methods() {}

void SpikeResourceImporterTextureAtlas::_dirs_and_files_before_remove(Vector<String> files_to_delete, Vector<String> dirs_to_delete) {
	may_delete_source2atlas.clear();
	may_delete_folder2atlas_list.clear();

	for (size_t i = 0; i < files_to_delete.size(); i++) {
		auto f = files_to_delete[i];
		if (FileAccess::exists(f + ".import") && get_resource_impoter_name(f) == get_importer_name()) {
			List<String> deps;
			ResourceLoader::get_dependencies(f, &deps);
			if (deps.size() > 0) {
				// Scattered pic only depends on one atlas
				may_delete_source2atlas.insert(f, deps[0]);
			}
		}
		// }
	}

	for (size_t i = 0; i < dirs_to_delete.size(); i++) {
		auto dir = dirs_to_delete[i];
		List<String> atlas_textures;

		_get_atlas_texture_source_file_recuresivly(EditorFileSystem::get_singleton()->get_filesystem_path(dir), atlas_textures);

		if (atlas_textures.size() > 0) {
			may_delete_folder2atlas_list[dir] = List<String>();
			for (size_t i = 0; i < atlas_textures.size(); i++) {
				List<String> deps;
				ResourceLoader::get_dependencies(atlas_textures[i], &deps);
				if (deps.size() > 0 && !may_delete_folder2atlas_list[dir].find(deps[0])) {
					// Scattered pic only depends on one atlas
					may_delete_folder2atlas_list[dir].push_back(deps[0]);
				}
			}
		}
	}
}

void SpikeResourceImporterTextureAtlas::_get_atlas_texture_source_file_recuresivly(EditorFileSystemDirectory *efsd, List<String> &r_files) {
	if (!efsd)
		return;

	for (size_t i = 0; i < efsd->get_subdir_count(); i++) {
		_get_atlas_texture_source_file_recuresivly(efsd->get_subdir(i), r_files);
	}

	for (int i = 0; i < efsd->get_file_count(); i++) {
		auto fp = efsd->get_file_path(i);
		if (FileAccess::exists(fp + ".import") && get_resource_impoter_name(fp) == get_importer_name()) {
			r_files.push_back(fp);
		}
	}
}

void SpikeResourceImporterTextureAtlas::_file_removed(const String &removed_file) {
	if (may_delete_source2atlas.has(removed_file)) {
		if (!queue_reimport_atlas.find(may_delete_source2atlas[removed_file])) {
			queue_reimport_atlas.push_back(may_delete_source2atlas[removed_file]);
			_try_reimport();
		}
		may_delete_source2atlas.erase(removed_file);
	}
}

void SpikeResourceImporterTextureAtlas::_folder_removed(const String &removed_folder) {
	if (may_delete_folder2atlas_list.has(removed_folder)) {
		List<String> related_atlases = may_delete_folder2atlas_list[removed_folder];
		for (size_t i = 0; i < related_atlases.size(); i++) {
			String atlas_file = related_atlases[i];
			if (!queue_reimport_atlas.find(atlas_file)) {
				queue_reimport_atlas.push_back(atlas_file);
				_try_reimport();
			}
		}

		may_delete_folder2atlas_list.erase(removed_folder);
	}
}

bool SpikeResourceImporterTextureAtlas::_atlas_file_changed(const String &changed_file_path, const String &target_atlas_path) {
	if (get_resource_impoter_name(changed_file_path) == get_importer_name()) {
		List<String> deps;
		ResourceLoader::get_dependencies(changed_file_path, &deps);

		auto target_ruid = ResourceLoader::get_resource_uid(target_atlas_path);

		for (size_t i = 0; i < deps.size(); i++) {
			auto ruid = ResourceLoader::get_resource_uid(deps[i]);
			if (ruid != target_ruid && !queue_reimport_atlas.find(deps[i])) {
				queue_reimport_atlas.push_back(deps[i]);
				_try_reimport();
			}
		}
		if (deps.size() > 0 && deps.size() == queue_reimport_atlas.size()) {
			return true;
		}
	}
	return false;
}

void SpikeResourceImporterTextureAtlas::_try_reimport() {
	if (EditorFileSystem::get_singleton()->is_importing() || MessageQueue::get_singleton()->is_flushing()) {
		return;
	}
	if (EditorNode::get_singleton()->get_tree()->is_connected("process_frame", callable_mp(this, &SpikeResourceImporterTextureAtlas::_try_reimport))) {
		EditorNode::get_singleton()->get_tree()->disconnect("process_frame", callable_mp(this, &SpikeResourceImporterTextureAtlas::_try_reimport));
	}
	while (!queue_reimport_atlas.is_empty()) {
		auto atlas_path = queue_reimport_atlas.back()->get();
		queue_reimport_atlas.pop_back();

		atlas_path = convert_to_real_path(atlas_path);

		List<String> owners;
		_get_resource_owners(atlas_path, EditorFileSystem::get_singleton()->get_filesystem(), owners);

		Vector<String> files;
		files.resize(owners.size());
		for (size_t i = 0; i < owners.size(); i++) {
			files.set(i, owners[i]);
		}

		HashMap<String, HashMap<StringName, Variant>> source_file_options;
		HashMap<String, String> base_paths;
		_collect_import_infos(atlas_path, files, source_file_options, base_paths);

		if (source_file_options.size() > 0) {
			EditorFileSystem::get_singleton()->reimport_files(files);
			return;
		} else {
			_simulate_delete_file(atlas_path);
		}
	}
}

void SpikeResourceImporterTextureAtlas::_on_resources_reimported(PackedStringArray resoueces) {
	if (EditorFileSystem::get_singleton()->is_importing() || MessageQueue::get_singleton()->is_flushing()) {
		return;
	}
	if (!EditorNode::get_singleton()->get_tree()->is_connected("process_frame", callable_mp(this, &SpikeResourceImporterTextureAtlas::_try_reimport))) {
		EditorNode::get_singleton()->get_tree()->connect("process_frame", callable_mp(this, &SpikeResourceImporterTextureAtlas::_try_reimport));
	}
}

void SpikeResourceImporterTextureAtlas::_simulate_delete_file(const String &file_to_delete) {
	static SpikeDependencyRemoveDialog *spike_dependency_remove_dialog = nullptr;
	auto real_path = convert_to_real_path(file_to_delete);

	if (spike_dependency_remove_dialog == nullptr) {
		auto fd = FileSystemDock::get_singleton();
		for (size_t i = 0; i < fd->get_child_count(); i++) {
			auto c = static_cast<SpikeDependencyRemoveDialog *>(fd->get_child(i));
			if (c) {
				spike_dependency_remove_dialog = c;
				break;
			}
		}
	}

	spike_dependency_remove_dialog->remove_file(real_path);
}

void SpikeResourceImporterTextureAtlas::_connect_signal() {
	SpikeFileSystemDock *fd = static_cast<SpikeFileSystemDock *>(FileSystemDock::get_singleton());
	fd->connect("file_removed", callable_mp(this, &SpikeResourceImporterTextureAtlas::_file_removed));
	fd->connect("folder_removed", callable_mp(this, &SpikeResourceImporterTextureAtlas::_folder_removed));
	fd->connect("dirs_and_files_before_remove", callable_mp(this, &SpikeResourceImporterTextureAtlas::_dirs_and_files_before_remove));

	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	efs->connect("resources_reimported", callable_mp(this, &SpikeResourceImporterTextureAtlas::_on_resources_reimported));
}

/**
 * @brief 拾取导入需要的信息
 *
 * @param p_group_file 图集文件路径
 * @param p_files 散图路径
 */
void SpikeResourceImporterTextureAtlas::_collect_import_infos(const String &p_group_file, const Vector<String> &p_files,
		HashMap<String, HashMap<StringName, Variant>> &source_file_options, HashMap<String, String> &base_paths) {
	String importer_name;
	for (int i = 0; i < p_files.size(); i++) {
		Ref<ConfigFile> config;
		config.instantiate();

		if (!FileAccess::exists(p_files[i] + ".import")) {
			continue;
		}

		Error err = config->load(p_files[i] + ".import");
		ERR_CONTINUE(err != OK);
		ERR_CONTINUE(!config->has_section_key("remap", "importer"));
		String file_importer_name = config->get_value("remap", "importer");
		ERR_CONTINUE(file_importer_name.is_empty());

		if (!importer_name.is_empty() && importer_name != file_importer_name) {
			EditorNode::get_singleton()->show_warning(vformat(STTR("There are multiple importers for different types pointing to file %s, import aborted"), p_group_file));
			ERR_FAIL();
		}

		source_file_options[p_files[i]] = HashMap<StringName, Variant>();
		importer_name = file_importer_name;

		if (importer_name == "keep") {
			continue; //do nothing
		}

		Ref<ResourceImporter> importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(importer_name);
		ERR_FAIL_COND(!importer.is_valid());
		List<ResourceImporter::ImportOption> options;
		importer->get_import_options(p_files[i], &options);
		//set default values
		for (const ResourceImporter::ImportOption &E : options) {
			source_file_options[p_files[i]][E.option.name] = E.default_value;
		}

		if (config->has_section("params")) {
			List<String> sk;
			config->get_section_keys("params", &sk);
			for (const String &param : sk) {
				Variant value = config->get_value("params", param);
				//override with whatever is in file
				source_file_options[p_files[i]][param] = value;
			}
		}

		base_paths[p_files[i]] = ResourceFormatImporter::get_singleton()->get_import_base_path(p_files[i]);
	}
}

void SpikeResourceImporterTextureAtlas::_get_resource_owners(const String &p_resource_path, EditorFileSystemDirectory *efsd, List<String> &r_owners) {
	if (!efsd) {
		return;
	}

	for (int i = 0; i < efsd->get_subdir_count(); i++) {
		_get_resource_owners(p_resource_path, efsd->get_subdir(i), r_owners);
	}

	auto resource_uid = ResourceLoader::get_resource_uid(p_resource_path);
	if (resource_uid == -1)
		return;

	for (int i = 0; i < efsd->get_file_count(); i++) {
		if (efsd->get_file_import_is_valid(i)) {
			Vector<String> deps = efsd->get_file_deps(i);
			bool found = false;
			for (int j = 0; j < deps.size(); j++) {
				if (ResourceLoader::get_resource_uid(deps[j]) == resource_uid) {
					found = true;
					break;
				}
			}
			if (!found) {
				continue;
			}

			r_owners.push_back(efsd->get_file_path(i));
		}
	}
}

#pragma endregion