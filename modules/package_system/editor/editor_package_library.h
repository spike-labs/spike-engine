/**
 * editor_package_library.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "editor/editor_file_dialog.h"
#include "editor/plugins/asset_library_editor_plugin.h"
#include "scene/gui/dialogs.h"
#include "scene/main/http_request.h"
#include "spike_define.h"

static bool compare_version_smaller(const String &p_ver_a, const String p_ver_b) {
	auto vera = p_ver_a.split(".");
	auto verb = p_ver_b.split(".");
	for (int i = 0;; ++i) {
		if (vera.size() == i || verb.size() == i)
			break;
		int va = vera[i].to_int();
		int vb = verb[i].to_int();
		if (va < vb)
			return true;
	}

	return false;
}

class AddGitSourceDialog : public ConfirmationDialog {
	GDCLASS(AddGitSourceDialog, ConfirmationDialog);

	LineEdit *repo_edit, *branch_edit, *subpath_edit;

protected:
	virtual void ok_pressed() override;

public:
	AddGitSourceDialog();
	~AddGitSourceDialog();
};

class EditorPackageLibrary : public PanelContainer {
	GDCLASS(EditorPackageLibrary, PanelContainer);

protected:
	enum MenuOption {
		// Use as index (id is index)
		FILTER_REGISTY,
		FILTER_IN_PROJECT,

		// Use as id
		IMPORT_FROM_GIT,
		IMPORT_FROM_DISK,
	};

	enum RequestType {
		REQ_NONE,
		REQ_LIST_VER,
		REQ_LIST_CONTENT,
		REQ_PACKAGE_VER,
		REQ_VERSION_CONTENT,
	};

	enum PackageSource {
		NONE,
		GIT,
		REGISTRY,
	};

	struct PackageData {
		PackageSource source;
		String name;
		String ver;
		String desc;
		String author;
		String date;

		bool is_version(const PackageData &p_data) {
			return source == p_data.source && ver == p_data.ver;
		}
	};

	struct PackageInfo : public PackageData {
		String id;
		PackageSource install_source;
		String install_ver;
		Vector<PackageData> versions;
		bool has_version(PackageSource p_source, const String &p_ver) const {
			for (const auto &v : versions) {
				if (p_source == v.source && p_ver == v.ver) {
					return true;
				}
			}
			return false;
		}
	};

private:
	struct PackageVersionCompare {
		_FORCE_INLINE_ bool operator()(const PackageData &l, const PackageData &r) const {
			if (l.source != r.source) {
				return l.source > r.source;
			}
			return compare_version_smaller(l.ver, r.ver);
		}
	};

	struct PackageCompare {
		_FORCE_INLINE_ bool operator()(const PackageInfo &l, const PackageInfo &r) const {
			return l.id < r.id;
		}
	};

	bool initial_loading;
	bool building_tree;

	HTTPRequest *request;
	String host;
	String list_ver;

	RequestType requesting = RequestType::REQ_NONE;

	Map<String, PackageInfo> package_map;
	Vector<String> package_name_list;
	String selected_package_id;
	Set<String> uncollapsed_packages;

	OptionButton *filter_opt;
	LineEdit *filter_edit;
	Control *right_vbox;
	Label *error_label;
	Label *name_label;
	Label *author_label;
	Label *version_label;
	Label *desc_label;
	Tree *package_tree;
	Button *install_btn;
	Button *reinstall_btn;

	AddGitSourceDialog *git_dialog;
	EditorFileDialog *disk_dialog;

protected:
	void _notification(int p_what);
	void _menu_option_pressed(int p_id);
	void _import_package_from_disk(const String &p_path);
	void _filter_edit_submitted(const String &p_new_text);

	void _api_request(const String &p_request, RequestType p_request_type, const String &p_arguments = "");
	void _http_request_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data);

	void _request_list_ver();
	void _request_package_ver();

	TreeItem *create_package_subitem(TreeItem *p_parent, const PackageInfo &p_info, const String &p_ver) {
		auto item = package_tree->create_item(p_parent);
		item->set_text_alignment(1, HORIZONTAL_ALIGNMENT_RIGHT);
		item->set_metadata(0, p_info.id);
		item->set_metadata(1, p_ver);
		item->set_text(1, p_ver);
		item->set_cell_mode(2, TreeItem::CELL_MODE_ICON);
		return item;
	}

	TreeItem *create_package_item(TreeItem *p_parent, const PackageInfo &p_info);

	void _update_installed_packages();
	void _update_package_tree();
	void _update_package_tree_filter_registry();
	void _update_package_tree_filter_in_project();
	void _tree_item_selected();
	void _tree_item_collapsed(TreeItem *p_item);

	void _install_selected_package();

	// Package Context Menu
	void _extend_package_context_menu();
	bool _package_reload_action(bool p_validate, const PackedStringArray p_paths);

public:
	EditorPackageLibrary();
	~EditorPackageLibrary();
};

class PackageLibraryEditorPlugin : public EditorPlugin {
	GDCLASS(PackageLibraryEditorPlugin, EditorPlugin);

	TabContainer *tab_container = nullptr;
	EditorPackageLibrary *package_library = nullptr;
	EditorAssetLibrary *addon_library = nullptr;

public:
	virtual String get_name() const override { return "AssetLib"; }
	bool has_main_screen() const override { return true; }
	virtual void edit(Object *p_object) override {}
	virtual bool handles(Object *p_object) const override { return false; }
	virtual void make_visible(bool p_visible) override;

	PackageLibraryEditorPlugin();
	~PackageLibraryEditorPlugin();
};