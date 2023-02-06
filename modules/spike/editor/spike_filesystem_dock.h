#ifndef SPIKE_FILESYSTEM_DOCK_H
#define SPIKE_FILESYSTEM_DOCK_H

#include "node_context_ref.h"
#include "spike_define.h"
#include <godot/editor/filesystem_dock.h>
#include "resource_recognizer.h"

class EditorNode;

class SpikeFileSystemDock : public FileSystemDock {
	GDCLASS(SpikeFileSystemDock, FileSystemDock);

	struct MenuItem {
		PackedStringArray menus;
		Callable action;
		Ref<Texture2D> icon;
	};

	List<MenuItem> context_menu_list;
	NodeContextRef<PackedStringArray> *custom_menus;

	PackedStringArray selected_paths;

private:
	void _dirs_and_files_before_remove(PoolStringArray files_to_delete, PoolStringArray dirs_to_delete) {
		emit_signal("dirs_and_files_before_remove", files_to_delete, dirs_to_delete);
	}
	void _try_update_file_thumbnail(PackedStringArray resources);

	Vector<String> plugin_uncollapsed_paths;

	void _item_collapsed(TreeItem *item);
	void _item_mouse_selected(const Vector2 &p_pos, MouseButton p_button);
	void _spike_tree_empty_click(const Vector2 &p_pos, MouseButton p_button);
	void _spike_file_list_item_clicked(int p_item, const Vector2 &p_pos, MouseButton p_mouse_button_index);
	void _spike_file_list_empty_clicked(const Vector2 &p_pos, MouseButton p_mouse_button_index);
	void _popup_index_pressed(int p_index, PopupMenu *popup);

	int _create_custom_menus(Node *p_root);

	bool _create_plugins_tree(EditorFileSystemDirectory *p_dir, bool p_select_in_favorites, bool p_unfold_path);

protected:
	static void _bind_methods();
	void _file_removed(String p_file);
	void _folder_removed(String p_folder);

	bool _find_file_tree_item(const String &fpath, TreeItem *&r_it, TreeItem *it = nullptr);

	void _generate_file_thumbnail_done(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata);

	virtual bool _create_tree(TreeItem *p_parent, EditorFileSystemDirectory *p_dir, Vector<String> &uncollapsed_paths, bool p_select_in_favorites, bool p_unfold_path = false) override;
	virtual void _update_file_list(bool p_keep_selection) override;

	virtual void _select_file(const String &p_path, bool p_select_in_favorites = false) override;

public:
	SpikeFileSystemDock();
	~SpikeFileSystemDock();

	void update_file_thumbnail(const String &fpath);

	void register_menu_item(const PackedStringArray &p_menus, const Callable &p_callable, const Ref<Texture2D> &p_icon = Ref<Texture2D>()) {
		MenuItem item;
		item.menus = p_menus;
		item.action = p_callable;
		item.icon = p_icon;
		context_menu_list.push_back(item);

		auto host = Object::cast_to<Node>(p_callable.get_object());
		if (host) {
			custom_menus->make_ref(host, p_menus);
		}
	}

	void unregister_menu_item(const PackedStringArray &p_menus) {
		for (auto elem = context_menu_list.front(); elem; elem = elem->next()) {
			auto menus = elem->get().menus;
			if (menus.size() >= p_menus.size()) {
				bool match = true;
				for (size_t i = 0; i < p_menus.size(); i++) {
					if (menus[i] != p_menus[i]) {
						match = false;
						break;
					}
				}
				if (match) {
					context_menu_list.erase(elem);
				}
			}
		}
	}
};

#endif // FILESYSTEM_DOCK_H
