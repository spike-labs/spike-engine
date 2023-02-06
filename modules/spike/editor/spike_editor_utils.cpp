/**
 * spike_editor_utils.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "spike_editor_utils.h"

#include "scene/gui/popup_menu.h"
#include "spike_editor_node.h"
#include "spike_filesystem_dock.h"

#define EDITOR_MENU_TOOLS_CUSTOM 34 // EditorNode::MenuOptions::TOOLS_CUSTOM

static int _find_popupmenu_item(PopupMenu *p_menu, const String &p_iname, const String &p_mname) {
	int idx = -1;
	for (size_t i = 0; i < p_menu->get_item_count(); i++) {
		auto item_text = p_menu->get_item_text(i);
		if (item_text == p_mname || item_text == p_iname) {
			idx = i;
			break;
		}
	}
	return idx;
}

static void _remove_submenu_item(PopupMenu *p_menu, Node *p_child) {
	if (p_menu && p_child) {
		auto submenu = p_child->get_name();
		for (size_t i = 0; i < p_menu->get_item_count(); i++) {
			if (p_menu->get_item_submenu(i) == submenu) {
				p_menu->remove_item(i);
				p_menu->reset_size();
				break;
			}
		}
	}
}

static PopupMenu *_add_main_menu_item(const String &menu_path, const Callable &callable, const Ref<Texture2D> &p_icon, int *r_idx) {
	SpikeEditorNode *spike = (SpikeEditorNode *)SpikeEditorNode::get_singleton();
	auto menus = menu_path.split("/");
	int index;
	auto arg_menus = PackedStringArray(menus);
	auto popup = spike->add_menu_item(arg_menus, index);
	if (popup) {
		auto host = Object::cast_to<Node>(callable.get_object());
		if (host) {
			spike->add_menu_ref(host, menus);
		}
		if (r_idx)
			*r_idx = index;

		popup->set_item_metadata(index, callable);
		popup->set_item_icon(index, p_icon);
	} else {
		WARN_PRINT("Fail to add menu [" + menu_path + "]");
	}
	return popup;
}
void EditorUtils::_bind_methods() {
	ClassDB::bind_static_method("EditorUtils", D_METHOD("add_menu_item", "menu_path", "callable", "icon"), &EditorUtils::add_menu_item, DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_static_method("EditorUtils", D_METHOD("add_menu_check_item", "menu_path", "callable", "checked", "icon"), &EditorUtils::add_menu_check_item, DEFVAL(false), DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_static_method("EditorUtils", D_METHOD("add_menu_radio_check_item", "menu_path", "callable", "checked", "icon"), &EditorUtils::add_menu_radio_check_item, DEFVAL(false), DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_static_method("EditorUtils", D_METHOD("remove_menu_item", "menu_path"), &EditorUtils::remove_menu_item);
	ClassDB::bind_static_method("EditorUtils", D_METHOD("execute_and_show_output", "title", "path", "arguments", "close_on_ok", "close_on_errors"), &EditorUtils::execute_and_show_output, DEFVAL(false), DEFVAL(false));
}

PopupMenu *EditorUtils::make_menu(Node *p_menu, PackedStringArray &p_menus, Callable p_menu_pressed, int &r_idx) {
	auto iname = p_menus[0];
	auto mname = TTR(iname);
	p_menus.remove_at(0);

	if (p_menus.size() > 0) {
		Node *submenu = p_menu->find_child(iname, false, false);
		if (submenu == nullptr)
			submenu = p_menu->find_child(mname, false, false);
		if (submenu == nullptr) {
			submenu = memnew(PopupMenu);
			submenu->set_name(iname);
			p_menu->add_child(submenu);
			if (auto popmenu = Object::cast_to<PopupMenu>(p_menu)) {
				popmenu->add_submenu_item(mname, iname);
			}
			auto pop = Object::cast_to<PopupMenu>(submenu);
			pop->connect("index_pressed", p_menu_pressed.bind(pop));
		}
		return make_menu(submenu, p_menus, p_menu_pressed, r_idx);
	} else if (auto submenu = Object::cast_to<PopupMenu>(p_menu)) {
		r_idx = _find_popupmenu_item(submenu, iname, mname);
		return submenu;
	} else {
		return nullptr;
	}
}

bool EditorUtils::remove_menu(Node *p_menu, PackedStringArray &p_menus) {
	auto iname = p_menus[0];
	auto mname = TTR(iname);
	p_menus.remove_at(0);

	if (p_menus.size() > 0) {
		Node *child = p_menu->find_child(iname, false, false);
		if (child == nullptr)
			child = p_menu->find_child(mname, false, false);
		if (child) {
			if (remove_menu(child, p_menus)) {
				auto submenu = Object::cast_to<PopupMenu>(child);
				if (submenu && submenu->get_item_count() == 0) {
					p_menu->remove_child(child);
					_remove_submenu_item(Object::cast_to<PopupMenu>(p_menu), child);
					memdelete(child);
				}
				return true;
			}
		}
	} else if (auto submenu = Object::cast_to<PopupMenu>(p_menu)) {
		int idx = _find_popupmenu_item(submenu, iname, mname);
		if (idx >= 0) {
			auto subname = submenu->get_item_submenu(idx);
			if (IS_EMPTY(subname)) {
				submenu->remove_item(idx);
				submenu->reset_size();
				return true;
			}
		}
	}

	return false;
}

void EditorUtils::add_menu_item(const String &menu_path, const Callable &callable, const Ref<Texture2D> &p_icon) {
	int index;
	PopupMenu *popup = nullptr;
	if (menu_path.begins_with("FileSystem/")) {
		if (auto fsd = Object::cast_to<SpikeFileSystemDock>(SpikeFileSystemDock::get_singleton())) {
			auto menus = menu_path.split("/");
			menus.remove_at(0);
			fsd->register_menu_item(menus, callable, p_icon);
		}
	} else {
		popup = _add_main_menu_item(menu_path, callable, p_icon, &index);
	}

	if (popup) {
		popup->set_item_as_checkable(index, false);
	}
}

void EditorUtils::add_menu_check_item(const String &menu_path, const Callable &callable, bool p_checked, const Ref<Texture2D> &p_icon) {
	int index;
	PopupMenu *popup = nullptr;
	if (menu_path.begins_with("FileSystem/")) {
		WARN_PRINT("Add CheckItem to FileSystemDock context menu is NOT supported, fallback to normal Item");
		add_menu_item(menu_path, callable, p_icon);
		return;
	} else {
		popup = _add_main_menu_item(menu_path, callable, p_icon, &index);
	}
	if (popup) {
		popup->set_item_as_checkable(index, true);
		popup->set_item_checked(index, p_checked);
	}
}

void EditorUtils::add_menu_radio_check_item(const String &menu_path, const Callable &callable, bool p_checked, const Ref<Texture2D> &p_icon) {
	int index;
	PopupMenu *popup = nullptr;
	if (menu_path.begins_with("FileSystem/")) {
		WARN_PRINT("Add RadioCheckItem to FileSystemDock context menu is NOT supported, fallback to normal Item");
		add_menu_item(menu_path, callable, p_icon);
		return;
	} else {
		popup = _add_main_menu_item(menu_path, callable, p_icon, &index);
	}
	if (popup) {
		popup->set_item_as_radio_checkable(index, true);
		popup->set_item_checked(index, p_checked);
	}
}

void EditorUtils::remove_menu_item(const String &menu_path) {
	if (menu_path.begins_with("FileSystem/")) {
		if (auto fsd = Object::cast_to<SpikeFileSystemDock>(SpikeFileSystemDock::get_singleton())) {
			auto menus = menu_path.split("/");
			menus.remove_at(0);
			fsd->unregister_menu_item(menus);
		}
	} else {
		auto arg_menus = menu_path.split("/");
		bool ret = ((SpikeEditorNode *)SpikeEditorNode::get_singleton())->remove_menu_item(arg_menus);
		if (!ret) {
			WARN_PRINT("Fail to remove menu [" + menu_path + "]");
		}
	}
}

int EditorUtils::execute_and_show_output(const String &p_title, const String &p_path, const Array &p_arguments, bool p_close_on_ok, bool p_close_on_errors) {
	List<String> list;
	for (size_t i = 0; i < p_arguments.size(); i++) {
		list.push_back(p_arguments[i]);
	}
	return EditorNode::get_singleton()->execute_and_show_output(p_title, p_path, list, p_close_on_ok, p_close_on_errors);
}
