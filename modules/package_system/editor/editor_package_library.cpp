/**
 * editor_package_library.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "editor_package_library.h"
#include "core/config/project_settings.h"
#include "core/io/json.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor_package_system.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/split_container.h"
#include "spike/editor/spike_editor_utils.h"

#define TREE_PANEL_STYLE EditorNode::get_singleton()->get_gui_base()->get_theme_stylebox("panel", "Tree")
#define BACKGROUND_STYLE EditorNode::get_singleton()->get_gui_base()->get_theme_stylebox("Background", "EditorStyles")
#define EDITOR_ADD_ICON EditorNode::get_singleton()->get_gui_base()->get_theme_icon("Add", "EditorIcons")
#define UPGRADE_ICON EditorNode::get_singleton()->get_gui_base()->get_theme_icon("ArrowUp", "EditorIcons")
#define INSTALLED_ICON EditorNode::get_singleton()->get_gui_base()->get_theme_icon("ImportCheck", "EditorIcons")
#define REFRESH_ICON EditorNode::get_singleton()->get_gui_base()->get_theme_icon("Reload", "EditorIcons")

#define ASSIGN_CFG_VALUE(v, cfg, s, k)  \
	if ((cfg)->has_section_key(s, k)) { \
		v = (cfg)->get_value(s, k);     \
	}
#define ASSIGN_CFG_VALUE_IF_EMPTY(v, cfg, s, k)        \
	if (IS_EMPTY(v) && (cfg)->has_section_key(s, k)) { \
		v = (cfg)->get_value(s, k);                    \
	}

static inline void setup_http_request(HTTPRequest *request) {
	request->set_use_threads(EDITOR_DEF("asset_library/use_threads", true));

	const String proxy_host = EDITOR_GET("network/http_proxy/host");
	const int proxy_port = EDITOR_GET("network/http_proxy/port");
	request->set_http_proxy(proxy_host, proxy_port);
	request->set_https_proxy(proxy_host, proxy_port);
}

void EditorPackageLibrary::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			add_theme_style_override("panel", get_theme_stylebox(SNAME("bg"), SNAME("AssetLib")));
			_extend_package_context_menu();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible_in_tree()) {
				if (initial_loading) {
					_update_installed_packages();
					_request_list_ver();
					EditorPackageSystem::get_singleton()->connect(PLUGIN_PROC_FINISHED,
							callable_mp(this, &EditorPackageLibrary::_update_package_tree));
				}
			}
		} break;
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			setup_http_request(request);
		} break;
	}
}

void EditorPackageLibrary::_menu_option_pressed(int p_id) {
	switch (p_id) {
		case MenuOption::IMPORT_FROM_GIT:
			git_dialog->popup_centered(Size2i(600 * EDSCALE, 0));
			break;
		case MenuOption::IMPORT_FROM_DISK:
			disk_dialog->popup_file_dialog();
			break;
		case MenuOption::FILTER_REGISTY:
		case MenuOption::FILTER_IN_PROJECT:
			selected_package_id = String();
			uncollapsed_packages.clear();
			_update_package_tree();
			break;
		default:
			break;
	}
}

void EditorPackageLibrary::_import_package_from_disk(const String &p_path) {
	// TODO
}

void EditorPackageLibrary::_filter_edit_submitted(const String &p_new_text) {
}

void EditorPackageLibrary::_api_request(const String &p_request, RequestType p_request_type, const String &p_arguments) {
	if (requesting != REQ_NONE) {
		request->cancel_request();
	}

	requesting = p_request_type;

	request->request(host + p_request + p_arguments);

	ED_MSG("[Package] requesting: %s%s", p_request, p_arguments);
}

void EditorPackageLibrary::_http_request_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data) {
	RequestType requested = requesting;
	requesting = REQ_NONE;

	String str = String::utf8((const char *)p_data.ptr(), p_data.size());
	bool error_abort = true;
	switch (p_status) {
		case HTTPRequest::RESULT_CANT_RESOLVE: {
			error_label->set_text(TTR("Can't resolve hostname:") + " " + host);
		} break;
		case HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED:
		case HTTPRequest::RESULT_CONNECTION_ERROR:
		case HTTPRequest::RESULT_CHUNKED_BODY_SIZE_MISMATCH: {
			error_label->set_text(TTR("Connection error, please try again."));
		} break;
		case HTTPRequest::RESULT_TLS_HANDSHAKE_ERROR:
		case HTTPRequest::RESULT_CANT_CONNECT: {
			error_label->set_text(TTR("Can't connect to host:") + " " + host);
		} break;
		case HTTPRequest::RESULT_NO_RESPONSE: {
			error_label->set_text(TTR("No response from host:") + " " + host);
		} break;
		case HTTPRequest::RESULT_REQUEST_FAILED: {
			error_label->set_text(TTR("Request failed, return code:") + " " + itos(p_code));
		} break;
		case HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED: {
			error_label->set_text(TTR("Request failed, too many redirects"));

		} break;
		default: {
			if (p_code != 200) {
				error_label->set_text(TTR("Request failed, return code:") + " " + itos(p_code));
			} else {
				error_abort = false;
			}
		} break;
	}

	if (error_abort) {
		if (initial_loading) {
			error_label->show();
			right_vbox->hide();
		}

		ELog("Request to `%s` failed.", host);
		return;
	}
	error_label->hide();

	switch (requested) {
		case REQ_LIST_VER:
			list_ver = str;
			_api_request("list.json", REQ_LIST_CONTENT, "?v=" + str);
			break;
		case REQ_LIST_CONTENT: {
			initial_loading = false;
			JSON json;
			ERR_FAIL_COND_MSG(json.parse(str) != OK, "Parse package list for packages failed.");

			Dictionary d = json.get_data();

			uncollapsed_packages.clear();
			package_map.clear();
			package_name_list.clear();
			Dictionary packages = d["packages"];
			for (const Variant *key = packages.next(nullptr); key; key = packages.next(key)) {
				Dictionary info = packages[*key];
				if (info.is_empty())
					continue;

				PackageInfo pkg_info = {
					{
							PackageSource::REGISTRY,
							info["name"],
							info["version"],
							info["description"],
							info["author"],
							info["date"],
					},
					*key,
				};
				package_map.insert(*key, pkg_info);
				package_name_list.push_back(*key);
			}
			package_name_list.sort();
			_update_package_tree();
		} break;
		case REQ_PACKAGE_VER:
			_api_request(selected_package_id + "/list.json", REQ_VERSION_CONTENT, "?v=" + str);
			break;
		case REQ_VERSION_CONTENT: {
			JSON json;
			ERR_FAIL_COND_MSG(json.parse(str) != OK, "Parse version list for packages failed.");

			Dictionary d = json.get_data();
			String package_id = d["id"];
			Array versions = d["versions"];
			auto &info = package_map[package_id];
			info.versions.clear();
			for (int i = 0; i < versions.size(); ++i) {
				Dictionary data = versions[i];
				if (data.is_empty())
					continue;
				info.versions.push_back({
						PackageSource::REGISTRY,
						data["name"],
						data["version"],
						data["description"],
						data["author"],
						data["date"],
				});
			}
			info.versions.sort_custom<PackageVersionCompare>();
			_update_package_tree();
		} break;
		default:
			break;
	}
}

void EditorPackageLibrary::_request_list_ver() {
	error_label->show();
	right_vbox->hide();
	error_label->set_text(TTR("Loading..."));

	_api_request("ver.txt", REQ_LIST_VER, "?t=" + itos(OS::get_singleton()->get_ticks_msec()));
}

void EditorPackageLibrary::_request_package_ver() {
	_api_request(selected_package_id + "/ver.txt", REQ_PACKAGE_VER, "?v=" + list_ver);
}

TreeItem *EditorPackageLibrary::create_package_item(TreeItem *p_parent, const PackageInfo &p_info) {
	auto &view_ver = IS_EMPTY(p_info.install_ver) ? p_info.ver : p_info.install_ver;
	auto item = create_package_subitem(p_parent, p_info, view_ver);
	item->set_text(0, p_info.name);
	item->set_tooltip_text(0, p_info.id);
	item->set_metadata(2, p_info.install_source == PackageSource::NONE ? p_info.source : p_info.install_source);
	item->set_collapsed(!uncollapsed_packages.has(p_info.id));

	auto versions = p_info.versions;
	bool has_ver = true;
	if (p_info.source == PackageSource::REGISTRY) {
		has_ver = p_info.has_version(PackageSource::REGISTRY, p_info.ver);
		if (!has_ver) {
			versions.push_back({ p_info.source, p_info.name, p_info.ver, p_info.desc, p_info.author, p_info.date });
			versions.sort_custom<PackageVersionCompare>();
		}
	}

	bool installed = false;
	bool has_update = false;
	for (int n = versions.size() - 1; n >= 0; n--) {
		auto sub_item = create_package_subitem(item, p_info, versions[n].ver);
		sub_item->set_text_alignment(0, HORIZONTAL_ALIGNMENT_RIGHT);
		sub_item->set_custom_font_size(0, 12 * EDSCALE);
		sub_item->set_metadata(2, versions[n].source);
		if (p_info.install_source == versions[n].source && p_info.install_ver == versions[n].ver) {
			sub_item->set_text(0, STTR("Currently Installed"));
			installed = true;
		} else if (!has_update && !IS_EMPTY(p_info.install_ver) &&
				versions[n].source == PackageSource::REGISTRY &&
				compare_version_smaller(p_info.install_ver, versions[n].ver)) {
			sub_item->set_text(0, STTR("Update Available"));
			has_update = true;
		}
	}

	if (has_update) {
		item->set_icon(2, UPGRADE_ICON);
	} else if (installed) {
		item->set_icon(2, INSTALLED_ICON);
	}

	if (!has_ver) {
		auto other = create_package_subitem(item, p_info, "");
		other->set_text(0, STTR("See other versions"));
		other->set_text_alignment(0, HORIZONTAL_ALIGNMENT_RIGHT);
		other->set_custom_font_size(0, 12 * EDSCALE);
	}

	return item;
}

void EditorPackageLibrary::_update_installed_packages() {
	Set<String> installed_list;
	for (auto itor = package_map.begin(); itor != package_map.end(); ++itor) {
		auto &info = itor->value;
		if (!IS_EMPTY(info.install_ver)) {
			installed_list.insert(itor->key);
		}
	}

	Ref<ConfigFile> manifest;
	REF_INSTANTIATE(manifest);
	manifest->load(PATH_MANIFEST);
	if (manifest->has_section(SECTION_PLUGINS)) {
		List<String> keys;
		manifest->get_section_keys(SECTION_PLUGINS, &keys);
		for (auto &key : keys) {
			installed_list.erase(key);
			auto &info = package_map[key];
			if (IS_EMPTY(info.id)) {
				info.id = key;
				info.source = PackageSource::NONE;
			}

			Ref<ConfigFile> cfg;
			REF_INSTANTIATE(cfg);
			PackageData install_data = { PackageSource::NONE };
			if (cfg->load(DIR_PLUGINS + key + "/plugin.cfg") == OK) {
				ASSIGN_CFG_VALUE(install_data.name, cfg, "plugin", "name");
				ASSIGN_CFG_VALUE(install_data.ver, cfg, "plugin", "version");
				ASSIGN_CFG_VALUE(install_data.desc, cfg, "plugin", "description");
				ASSIGN_CFG_VALUE(install_data.author, cfg, "plugin", "author");
				ASSIGN_CFG_VALUE(install_data.date, cfg, "plugin", "date");
			}

			auto ftag = FileAccess::open(DIR_PLUGINS + key + "/.tag", FileAccess::READ);
			auto tag = ftag.is_valid() ? ftag->get_as_text() : String();
			int pos = tag.find_char(':');
			if (pos > 0) {
				tag = tag.substr(0, pos);
				if (tag == "git") {
					install_data.source = PackageSource::GIT;
				} else if (tag == "registry") {
					install_data.source = PackageSource::REGISTRY;
				}
			}
			info.install_source = install_data.source;
			info.install_ver = install_data.ver;

			if (info.source == PackageSource::NONE) {
				info.source = install_data.source;
				info.name = install_data.name;
				info.ver = install_data.ver;
				info.desc = install_data.desc;
				info.author = install_data.author;
				info.date = install_data.date;
			}

			if (install_data.source == PackageSource::REGISTRY) {
				for (int i = info.versions.size() - 1; i >= 0; i--) {
					if (info.versions[i].source != PackageSource::REGISTRY) {
						info.versions.remove_at(i);
					}
				}
			}

			if (install_data.source != PackageSource::REGISTRY || !install_data.is_version(info)) {
				if (!info.has_version(install_data.source, install_data.ver)) {
					info.versions.push_back(install_data);
					info.versions.sort_custom<PackageVersionCompare>();
				}
			}
		}
	}

	for (const auto &key : installed_list) {
		auto &info = package_map[key];
		info.install_source = PackageSource::NONE;
		info.install_ver = String();
	}
}

void EditorPackageLibrary::_update_package_tree() {
	_update_installed_packages();
	building_tree = true;
	switch (filter_opt->get_selected_id()) {
		case MenuOption::FILTER_REGISTY:
			_update_package_tree_filter_registry();
			break;
		case MenuOption::FILTER_IN_PROJECT:
			_update_package_tree_filter_in_project();

		default:
			break;
	}
	building_tree = false;
}

void EditorPackageLibrary::_update_package_tree_filter_registry() {
	package_tree->clear();
	auto root = package_tree->create_item();

	TreeItem *to_select = nullptr;
	for (int i = 0; i < package_name_list.size(); i++) {
		const auto &info = package_map[package_name_list[i]];
		auto item = create_package_item(root, info);
		if (selected_package_id == info.id) {
			to_select = item;
		}
	}

	if (to_select) {
		package_tree->set_selected(to_select);
	}
}

void EditorPackageLibrary::_update_package_tree_filter_in_project() {
	Map<String, List<String>> categories;
	for (auto itor = package_map.begin(); itor != package_map.end(); ++itor) {
		auto &info = itor->value;
		auto author = info.author;
		if (IS_EMPTY(author))
			author = "[anonymous]";

		List<String> *ptr_list = categories.getptr(author);
		if (ptr_list == nullptr) {
			categories.insert(author, List<String>());
			ptr_list = categories.getptr(author);
		}
		if (!IS_EMPTY(info.install_ver)) {
			ptr_list->push_back(info.id);
		}
	}

	package_tree->clear();
	auto root = package_tree->create_item();

	TreeItem *to_select = nullptr;
	for (auto itor = categories.begin(); itor != categories.end(); ++itor) {
		auto list = itor->value;
		if (list.size() == 0)
			continue;
		list.sort();
		auto category = package_tree->create_item(root);
		category->set_selectable(0, false);
		category->set_selectable(1, false);
		category->set_selectable(2, false);
		category->set_text(0, itor->key);
		category->set_collapsed(true);

		for (const auto &id : list) {
			const auto &info = package_map[id];
			auto item = create_package_item(category, info);
			if (selected_package_id == info.id) {
				to_select = item;
			}
		}
	}

	if (to_select) {
		package_tree->set_selected(to_select);
	}
}

void EditorPackageLibrary::_tree_item_selected() {
	auto item = package_tree->get_selected();
	Variant package_id = item->get_metadata(0);
	if (package_id.is_zero())
		return;

	String package_ver = item->get_metadata(1);
	PackageSource package_src = PackageSource((int)item->get_metadata(2));
	const auto &info = package_map[package_id];
	selected_package_id = info.id;

	if (IS_EMPTY(package_ver)) {
		item->set_metadata(0, Variant());
		item->set_text(0, STTR("Loading..."));
		_request_package_ver();
		return;
	}

	String short_src;
	if (package_src == PackageSource::GIT) {
		short_src = "Git";
	} else if (package_src == PackageSource::NONE) {
		short_src = STTR("Develop");
	}

	right_vbox->hide();
	name_label->set_text(info.name);
	author_label->set_text(info.author);
	version_label->set_text(vformat("%s %s - %s", STTR("Version"), package_ver, package_src == PackageSource::REGISTRY ? info.date : short_src));
	String desc = info.desc.strip_edges(false, true);
	// if (!IS_EMPTY(info.install_ver) && info.install_source != PackageSource::REGISTRY) {
	// 	desc += " \n \n" + STTR("Installed From:") + " \n  " + EditorPackageSystem::get_singleton()->get_package_source(info.id);
	// }
	desc_label->set_text(desc);

	install_btn->set_visible(false);
	reinstall_btn->set_visible(false);
	if (IS_EMPTY(info.install_ver)) {
		install_btn->set_visible(true);
		install_btn->set_text(STTR("Install"));
	} else {
		if (info.install_source != package_src || info.install_ver != package_ver) {
			reinstall_btn->set_visible(true);
			reinstall_btn->set_text(vformat(STTR("Update to %s"), package_ver));
		} else {
			//reinstall_btn->set_text(vformat(STTR("Update to %s"), info.ver));
			install_btn->set_visible(true);
			install_btn->set_text(STTR("Remove"));
		}
	}
	right_vbox->show();
}

void EditorPackageLibrary::_tree_item_collapsed(TreeItem *p_item) {
	Variant package_id = p_item->get_metadata(0);
	if (package_id.is_zero())
		return;

	const auto &info = package_map[package_id];
	if (p_item->is_collapsed()) {
		uncollapsed_packages.erase(info.id);
		if (!building_tree) {
			p_item->select(0);
		}
	} else {
		uncollapsed_packages.insert(info.id);
		// Auto select newest or installed version
		TreeItem *prefer = p_item->get_child(0);
		for (int i = 1; i < p_item->get_child_count(); ++i) {
			TreeItem *child = p_item->get_child(i);
			String child_ver = child->get_metadata(1);
			PackageSource child_src = PackageSource((int)child->get_metadata(2));
			if (info.install_source != PackageSource::NONE && info.install_source == child_src && info.install_ver == child_ver) {
				prefer = child;
			}
		}

		if (!building_tree) {
			prefer->select(0);
		}
	}
}

void EditorPackageLibrary::_install_selected_package() {
	auto item = package_tree->get_selected();
	String package_id = item->get_metadata(0);
	String package_ver = item->get_metadata(1);
	auto package_src = PackageSource(int(item->get_metadata(2)));

	const auto &info = package_map[package_id];

	if (IS_EMPTY(info.install_ver) || info.install_source != package_src || info.install_ver != package_ver) {
		EditorPackageSystem::get_singleton()->install_package(info.id, package_ver);
	} else {
		EditorPackageSystem::get_singleton()->uninstall_package(info.id);
	}
}

void EditorPackageLibrary::_extend_package_context_menu() {
	EditorUtils::add_menu_item("FileSystem/Reload Package Plugin", callable_mp(this, &EditorPackageLibrary::_package_reload_action));
}

bool EditorPackageLibrary::_package_reload_action(bool p_validate, const PackedStringArray p_paths) {
	if (p_paths.size() != 1 || !p_paths[0].begins_with(DIR_PLUGINS))
		return false;

	String plugin_path = PATH_JOIN(p_paths[0], "plugin.cfg");

	if (p_validate) {
		return FileAccess::exists(plugin_path);
	}

	EditorPackageSystem::get_singleton()->_disable_plugin(plugin_path);
	MessageQueue::get_singleton()->push_callable(callable_mp(EditorPackageSystem::get_singleton(), &EditorPackageSystem::_enable_plugin).bind(plugin_path, String()));
	return true;
}

EditorPackageLibrary::EditorPackageLibrary() {
	initial_loading = true;
	building_tree = false;
	host = DEFAULT_REGISTRY_URL;

	request = memnew(HTTPRequest);
	add_child(request);
	setup_http_request(request);
	request->connect("request_completed", callable_mp(this, &EditorPackageLibrary::_http_request_completed));

	auto container = memnew(VBoxContainer);
	add_child(container);
	container->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	auto bar_hbox = memnew(HBoxContainer);
	container->add_child(bar_hbox);
	bar_hbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	auto add_menu = memnew(MenuButton);
	bar_hbox->add_child(add_menu);
	add_menu->set_icon(EDITOR_ADD_ICON);
	add_menu->set_flat(false);
	add_menu->get_popup()->connect("id_pressed", callable_mp(this, &EditorPackageLibrary::_menu_option_pressed));
	add_menu->get_popup()->add_item(STTR("Add package from git URL..."), MenuOption::IMPORT_FROM_GIT);
	// add_menu->get_popup()->add_item(STTR("Add package from disk..."), MenuOption::IMPORT_FROM_DISK);

	auto refresh_btn = memnew(Button);
	bar_hbox->add_child(refresh_btn);
	refresh_btn->set_icon(REFRESH_ICON);
	refresh_btn->connect("pressed", callable_mp(this, &EditorPackageLibrary::_request_list_ver));

	bar_hbox->add_child(memnew(VSeparator));

	filter_opt = memnew(OptionButton);
	bar_hbox->add_child(filter_opt);
	filter_opt->connect("item_selected", callable_mp(this, &EditorPackageLibrary::_menu_option_pressed));
	filter_opt->add_item(STTR("Registry"), MenuOption::FILTER_REGISTY);
	filter_opt->add_item(STTR("In Project"), MenuOption::FILTER_IN_PROJECT);

	auto fill = memnew(Control);
	bar_hbox->add_child(fill);
	fill->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	filter_edit = memnew(LineEdit);
	bar_hbox->add_child(filter_edit);
	filter_edit->set_custom_minimum_size(Size2(300 * EDSCALE, 0));
	filter_edit->set_placeholder(STTR("Filter Packages"));
	filter_edit->connect("text_submitted", callable_mp(this, &EditorPackageLibrary::_filter_edit_submitted));

	// Not implemented
	filter_edit->hide();

	auto split = memnew(HSplitContainer);
	container->add_child(split);
	split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	split->set_split_offset(400 * EDSCALE);

	// Left Split
	auto tree_vbox = memnew(VBoxContainer);
	split->add_child(tree_vbox);

	package_tree = memnew(Tree);
	tree_vbox->add_child(package_tree);
	package_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	package_tree->set_hide_root(true);
	package_tree->set_columns(3);
	package_tree->set_column_expand(1, false);
	package_tree->set_column_expand(2, false);
	package_tree->set_select_mode(Tree::SELECT_ROW);
	package_tree->add_theme_constant_override("draw_relationship_lines", 0);
	package_tree->connect("item_selected", callable_mp(this, &EditorPackageLibrary::_tree_item_selected));
	package_tree->connect("item_collapsed", callable_mp(this, &EditorPackageLibrary::_tree_item_collapsed));

	// Right Split
	auto detail_scroll_bg = memnew(PanelContainer);
	split->add_child(detail_scroll_bg);
	detail_scroll_bg->add_theme_style_override("panel", TREE_PANEL_STYLE);

	right_vbox = memnew(VBoxContainer);
	detail_scroll_bg->add_child(right_vbox);

	auto detail_scroll = memnew(ScrollContainer);
	detail_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	detail_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	right_vbox->add_child(detail_scroll);

	auto info_vbox = memnew(VBoxContainer);
	detail_scroll->add_child(info_vbox);
	info_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	name_label = memnew(Label);
	info_vbox->add_child(name_label);
	name_label->add_theme_font_size_override("font_size", 30 * EDSCALE);
	name_label->add_theme_constant_override("outline_size", 1);

	author_label = memnew(Label);
	info_vbox->add_child(author_label);

	version_label = memnew(Label);
	info_vbox->add_child(version_label);
	version_label->add_theme_constant_override("outline_size", 1);

	info_vbox->add_child(memnew(Control));

	auto h_sep = memnew(HSeparator);
	info_vbox->add_child(h_sep);
	h_sep->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	desc_label = memnew(Label);
	info_vbox->add_child(desc_label);

	auto bottom_panel = memnew(PanelContainer);
	right_vbox->add_child(bottom_panel);
	bottom_panel->add_theme_style_override("panel", BACKGROUND_STYLE);

	auto btn_hbox = memnew(HBoxContainer);
	bottom_panel->add_child(btn_hbox);
	btn_hbox->set_alignment(BoxContainer::ALIGNMENT_END);

	reinstall_btn = memnew(Button);
	btn_hbox->add_child(reinstall_btn);
	reinstall_btn->set_visible(false);
	reinstall_btn->connect("pressed", callable_mp(this, &EditorPackageLibrary::_install_selected_package));

	install_btn = memnew(Button);
	btn_hbox->add_child(install_btn);
	install_btn->set_visible(false);
	install_btn->connect("pressed", callable_mp(this, &EditorPackageLibrary::_install_selected_package));

	right_vbox->hide();

	error_label = memnew(Label);
	detail_scroll_bg->add_child(error_label);
	error_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	error_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);

	git_dialog = memnew(AddGitSourceDialog);
	add_child(git_dialog);

	disk_dialog = memnew(EditorFileDialog);
	add_child(disk_dialog);
	disk_dialog->add_filter("plugin.cfg");
	disk_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	String res_path = ProjectSettings::get_singleton()->get_resource_path();
	disk_dialog->set_current_dir(ProjectSettings::get_singleton()->globalize_path(res_path).get_base_dir());
	disk_dialog->connect("file_selected", callable_mp(this, &EditorPackageLibrary::_import_package_from_disk));
}

EditorPackageLibrary::~EditorPackageLibrary() {
}

void PackageLibraryEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		tab_container->show();
	} else {
		tab_container->hide();
	}
}

PackageLibraryEditorPlugin::PackageLibraryEditorPlugin() {
	auto main_screen_control = EditorNode::get_singleton()->get_main_screen_control();

	tab_container = memnew(TabContainer);
	main_screen_control->add_child(tab_container);
	tab_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tab_container->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	tab_container->set_tab_alignment(TabBar::ALIGNMENT_CENTER);

	package_library = memnew(EditorPackageLibrary);
	tab_container->add_child(package_library);
	package_library->set_name(STTR("Packages"));

	if (AssetLibraryEditorPlugin::is_available()) {
		auto childern = EditorNode::get_singleton()->get_main_screen_control()->find_children("", EditorAssetLibrary::get_class_static(), false, false);
		if (childern.size()) {
			addon_library = Object::cast_to<EditorAssetLibrary>(childern[0]);
			main_screen_control->remove_child(addon_library);

			tab_container->add_child(addon_library);
			addon_library->set_name(STTR("Addons"));
		}
	}

	tab_container->hide();
}

PackageLibraryEditorPlugin::~PackageLibraryEditorPlugin() {
}

// AddGitSourceDialog

void AddGitSourceDialog::ok_pressed() {
	String repo_url = repo_edit->get_text();
	String branch = branch_edit->get_text();
	String subpath = subpath_edit->get_text();

	String package_id = repo_url.get_basename().get_file();

	if (!IS_EMPTY(branch)) {
		repo_url = repo_url + "#" + branch;
	}
	if (!IS_EMPTY(subpath)) {
		repo_url = repo_url + "?path=" + subpath;
	}

	EditorPackageSystem::get_singleton()->install_package(package_id, repo_url);

	hide();
}

AddGitSourceDialog::AddGitSourceDialog() {
	set_title(STTR("Add package from git URL..."));
	auto vbox = memnew(VBoxContainer);
	add_child(vbox);

	auto repo_lb = memnew(Label);
	vbox->add_child(repo_lb);
	repo_lb->set_text(STTR("Git Repo URL:"));

	repo_edit = memnew(LineEdit);
	vbox->add_child(repo_edit);
	repo_edit->set_placeholder("https://www.mycompany.com/com.mycompany.mypackage.git");

	auto branch_lb = memnew(Label);
	vbox->add_child(branch_lb);
	branch_lb->set_text(STTR("Branch or Tag (optional):"));

	branch_edit = memnew(LineEdit);
	vbox->add_child(branch_edit);
	branch_edit->set_placeholder("main");

	auto subpath_lb = memnew(Label);
	vbox->add_child(subpath_lb);
	subpath_lb->set_text(STTR("Sub path (optional):"));

	subpath_edit = memnew(LineEdit);
	vbox->add_child(subpath_edit);
	subpath_edit->set_placeholder("spike.plugins/com.mycompany.mypackage");
}

AddGitSourceDialog::~AddGitSourceDialog() {
}
