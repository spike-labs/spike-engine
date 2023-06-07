/**
 * external_translation_server.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "translation_patch_server.h"
#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/dir_access.h"
#include "core/io/file_access_memory.h"
#include "core/io/translation_loader_po.h"
#include "core/os/os.h"
#include "core/string/translation_po.h"
#include "sstream"

String get_pointer_address_path(const void *ptr) {
	std::ostringstream ss;
	ss << (void *)&ptr;
	return String(ss.str().c_str());
}

Error TranslationPatchList::add_patch(const Ref<TranslationPatch> &p_patch, const bool &p_notify) {
	if (p_patch.is_null()) {
		return ERR_CANT_CREATE;
	}
	if (p_patch->translation->get_locale() == TranslationPatchServer::get_singleton()->get_tool_locale()) {
		patch_list.push_front(p_patch);
		if (p_notify && OS::get_singleton()->get_main_loop()) {
			OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
		}
		return OK;
	}
	return ERR_UNAVAILABLE;
}

Error TranslationPatchList::remove_patch(const String &p_owner, const bool &p_notify) {
	Ref<TranslationPatch> patch = find_patch(p_owner);
	ERR_FAIL_COND_V(!patch.is_valid(), ERR_DOES_NOT_EXIST);
	patch_list.erase(patch);
	if (p_notify && OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
	return OK;
}

Ref<TranslationPatch> TranslationPatchList::find_patch(const String &p_owner) const {
	for (const List<Ref<TranslationPatch>>::Element *I = patch_list.front(); I; I = I->next()) {
		Ref<TranslationPatch> iek = I->get();
		if (iek.is_valid() && iek->owner == p_owner) {
			return iek;
		}
	}
	return nullptr;
}
void TranslationPatchList::load_dir(const String &p_dir, const bool &p_notify) {
	Ref<DirAccess> dir_access = DirAccess::open(p_dir);
	if (dir_access.is_valid()) {
		dir_access->list_dir_begin();
		String file_name = dir_access->get_next();
		while (!file_name.is_empty()) {
			if (!dir_access->current_is_dir()) {
				if (!file_name.ends_with(".uids")) {
					add_patch(TranslationPatchServer::create_file_patch(p_dir.path_join(file_name)), false);
				}
			}
			file_name = dir_access->get_next();
		}
		dir_access->list_dir_end();
	}
	if (p_notify) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
}

void TranslationPatchList::unload_dir(const String &p_dir, const bool &p_notify_change) {
	Ref<DirAccess> dir_access = DirAccess::open(p_dir);
	if (dir_access.is_valid()) {
		dir_access->list_dir_begin();
		String file_name = dir_access->get_next();
		while (!file_name.is_empty()) {
			if (!dir_access->current_is_dir()) {
				if (!file_name.ends_with(".uids")) {
					remove_patch(p_dir.path_join(file_name), false);
				}
			}
			file_name = dir_access->get_next();
		}
		dir_access->list_dir_end();
	}
	if (p_notify_change) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
}

StringName TranslationPatchList::find_translation_message(const String &p_locale, const StringName &p_message, const StringName &p_context) const {
	for (const List<Ref<TranslationPatch>>::Element *I = patch_list.front(); I; I = I->next()) {
		Ref<TranslationPatch> iek = I->get();
		if (iek.is_valid() && iek->translation->get_locale() == p_locale) {
			StringName r = iek->translation->get_message(p_message, p_context);
			if (r) {
				return r;
			}
		}
	}
	return StringName();
}

StringName TranslationPatchList::find_translation_plural_message(const String &p_locale, const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	for (const List<Ref<TranslationPatch>>::Element *I = patch_list.front(); I; I = I->next()) {
		Ref<TranslationPatch> iek = I->get();
		if (iek.is_valid() && iek->translation->get_locale() == p_locale) {
			StringName r = iek->translation->get_plural_message(p_message, p_message_plural, p_n, p_context);
			if (r) {
				return r;
			}
		}
	}
	return StringName();
}

TranslationPatchServer *TranslationPatchServer::singleton = nullptr;

TranslationPatchServer *TranslationPatchServer::get_singleton() {
	if (nullptr == singleton) {
		singleton = (TranslationPatchServer *)TranslationServer::get_singleton();
	}
	return singleton;
}

Vector<String> TranslationPatchServer::csv_extensions;
Vector<String> TranslationPatchServer::gettext_extensions;

bool TranslationPatchServer::is_csv_extension(const String &p_path) {
	for (int i = 0; i < csv_extensions.size(); i++) {
		if (p_path.ends_with(csv_extensions[i])) {
			return true;
		}
	}
	return false;
}

bool TranslationPatchServer::append_csv_extension(const String &p_extension) {
	if (!csv_extensions.has(p_extension)) {
		csv_extensions.append(p_extension);
		return true;
	}
	return false;
}

bool TranslationPatchServer::is_gettext_extension(const String &p_path) {
	for (int i = 0; i < gettext_extensions.size(); i++) {
		if (p_path.ends_with(gettext_extensions[i])) {
			return true;
		}
	}
	return false;
}

bool TranslationPatchServer::append_gettext_extension(const String &p_extension) {
	if (!gettext_extensions.has(p_extension)) {
		gettext_extensions.append(p_extension);
		return true;
	}
	return false;
}

//gettext patch
Ref<TranslationPatch> TranslationPatchServer::create_gettext_patch(const Ref<FileAccess> &p_access, const String &p_owner) {
	Ref<TranslationPatch> patch = nullptr;
	if (p_access.is_valid()) {
		Ref<Translation> translation = TranslationLoaderPO::load_translation(p_access);
		if (translation.is_valid()) {
			patch = Ref<TranslationPatch>(memnew(TranslationPatch));
			patch->translation = translation;
			patch->owner = p_owner;
		}
	}
	return patch;
}

Ref<TranslationPatch> TranslationPatchServer::create_gettext_patch(const PackedByteArray &p_stream, const String &p_owner) {
	Ref<FileAccessMemory> fam;
	fam.instantiate();
	fam->open_custom(p_stream.ptr(), p_stream.size());
	return create_gettext_patch(fam, p_owner);
}

//csv patch
Ref<TranslationPatch> TranslationPatchServer::create_csv_patch(const Ref<FileAccess> &p_access, const String &p_owner) {
	Ref<TranslationPatch> patch = nullptr;
	if (p_access.is_valid()) {
		String locale = get_singleton()->get_tool_locale();
		Vector<String> line = p_access->get_csv_line();
		ERR_FAIL_COND_V_MSG(line.size() <= 1, nullptr, ERR_PARSE_ERROR);
		int locale_index = 0;
		for (int i = 1; i < line.size(); i++) {
			if (locale == get_singleton()->standardize_locale(line[i])) {
				locale_index = i;
				break;
			}
		}
		if (locale_index > 0) {
			patch = Ref<TranslationPatch>(memnew(TranslationPatch));
			patch->translation = Ref<TranslationPO>(memnew(TranslationPO));
			patch->translation->set_locale(locale);
			patch->owner = p_owner;
			line = p_access->get_csv_line();
			while (line.size() > 1) {
				if (locale_index < line.size()) {
					String key = line[0];
					if (!key.is_empty()) {
						patch->translation->add_message(key, line[locale_index].c_unescape());
					}
				}
				line = p_access->get_csv_line();
			}
		}
	}
	return patch;
}
//patch
Ref<TranslationPatch> TranslationPatchServer::create_file_patch(const String &p_file) {
	if (p_file.ends_with(".translation") || p_file.ends_with(".tres") || p_file.ends_with(".res")) {
		Ref<Translation> translation = ResourceLoader::load(p_file);
		if (translation.is_valid()) {
			Ref<TranslationPatch> patch;
			patch.instantiate();
			patch->translation = translation;
			patch->owner = p_file;
			return patch;
		}
	} else {
		if (is_gettext_extension(p_file)) {
			Ref<FileAccess> file_access = FileAccess::open(p_file, FileAccess::ModeFlags::READ);
			if (file_access.is_valid()) {
				return create_gettext_patch(file_access, p_file);
			}
		} else if (is_csv_extension(p_file)) {
			Ref<FileAccess> file_access = FileAccess::open(p_file, FileAccess::ModeFlags::READ);
			if (file_access.is_valid()) {
				return create_csv_patch(file_access, p_file);
			}
		}
	}
	return nullptr;
}

Error TranslationPatchServer::add_translation_patch(const Ref<Translation> &p_translation, const bool &p_notify) {
	Ref<TranslationPatch> patch;
	patch.instantiate();
	patch->translation = p_translation;
	patch->owner = get_pointer_address_path(p_translation.ptr());
	return get_singleton()->patch_list.add_patch(patch, p_notify);
}

Error TranslationPatchServer::remove_translation_patch(const Ref<Translation> &p_translation, const bool &p_notify) {
	return get_singleton()->patch_list.remove_patch(get_pointer_address_path(p_translation.ptr()));
}

Error TranslationPatchServer::add_file_patch(const String &p_file, const bool &p_notify) {
	return get_singleton()->patch_list.add_patch(create_file_patch(p_file), p_notify);
}

Error TranslationPatchServer::remove_file_patch(const String &p_file, const bool &p_notify) {
	return get_singleton()->patch_list.remove_patch(p_file, p_notify);
}

Error TranslationPatchServer::add_gettext_stream_patch(const PackedByteArray &p_stream, const bool &p_notify) {
	return get_singleton()->patch_list.add_patch(create_gettext_patch(p_stream, get_pointer_address_path((void *)&p_stream)), p_notify);
}

Error TranslationPatchServer::remove_gettext_stream_patch(const PackedByteArray &p_stream, const bool &p_notify) {
	return get_singleton()->patch_list.remove_patch(get_pointer_address_path(&p_stream), p_notify);
}

Error TranslationPatchServer::add_patch(const Variant &p_patch, const bool &p_notify) {
	if (p_patch.get_type() == Variant::STRING) {
		return add_file_patch(p_patch, p_notify);
	} else if (p_patch.get_type() == Variant::PACKED_BYTE_ARRAY) {
		return add_gettext_stream_patch(p_patch, p_notify);
	} else if (p_patch.get_type() == Variant::OBJECT) {
		Ref<Translation> patch = p_patch;
		if (patch.is_valid()) {
			return add_translation_patch(patch);
		}
	}
	return ERR_INVALID_PARAMETER;
}

Error TranslationPatchServer::remove_patch(const Variant &p_patch, const bool &p_notify) {
	if (p_patch.get_type() == Variant::STRING) {
		return remove_file_patch(p_patch, p_notify);
	} else if (p_patch.get_type() == Variant::PACKED_BYTE_ARRAY) {
		return remove_gettext_stream_patch(p_patch, p_notify);
	} else if (p_patch.get_type() == Variant::OBJECT) {
		Ref<Translation> patch = p_patch;
		if (patch.is_valid()) {
			return remove_translation_patch(patch);
		}
	}
	return ERR_INVALID_PARAMETER;
}

void TranslationPatchServer::add_patch_dir(const String &p_dir, const bool &p_notify_change) {
	get_singleton()->patch_list.load_dir(p_dir, p_notify_change);
}

void TranslationPatchServer::remove_patch_dir(const String &p_dir, const bool &p_notify_change) {
	get_singleton()->patch_list.unload_dir(p_dir, p_notify_change);
}

void TranslationPatchServer::load_translations() {
	TranslationServer::load_translations();
	add_patch_dir("res://tr", false);
	add_patch_dir(OS::get_singleton()->get_user_data_dir().path_join("tr"), false);
	add_patch_dir(OS::get_singleton()->get_executable_path().get_base_dir().path_join("tr"), false);
	//main pack
	String main_pack_path = ProjectSettings::get_singleton()->get_setting("application/run/main_pack", String());
	if (!main_pack_path.is_empty()) {
		main_pack_path = main_pack_path.get_base_dir().path_join("tr");
		if (main_pack_path != OS::get_singleton()->get_executable_path().get_base_dir().path_join("tr")) {
			add_patch_dir(main_pack_path, false);
		}
	}

#ifdef MACOS_ENABLED
	if (!Engine::get_singleton()->is_editor_hint()) {
		load_external_dir(OS::get_singleton()->get_bundle_resource_dir().path_join("tr"), false);
	}
#endif
}

StringName TranslationPatchServer::translate(const StringName &p_message, const StringName &p_context) const {
	if (enabled) {
		StringName r = patch_list.find_translation_message(get_locale(), p_message, p_context);
		if (r) {
			return pseudolocalization_enabled ? pseudolocalize(r) : r;
		}
	}
	return TranslationServer::translate(p_message, p_context);
}

StringName TranslationPatchServer::translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	if (enabled) {
		StringName r = patch_list.find_translation_plural_message(get_locale(), p_message, p_message_plural, p_n, p_context);
		if (r) {
			return r;
		}
	}
	return TranslationServer::translate_plural(p_message, p_message_plural, p_n, p_context);
}

void TranslationPatchServer::_bind_methods() {
	ClassDB::bind_static_method("TranslationPatchServer", D_METHOD("append_csv_extension", "extension"), &TranslationPatchServer::append_csv_extension);
	ClassDB::bind_static_method("TranslationPatchServer", D_METHOD("append_gettext_extension", "extension"), &TranslationPatchServer::append_gettext_extension);
	ClassDB::bind_static_method("TranslationPatchServer", D_METHOD("add_patch", "patch", "notify"), &TranslationPatchServer::add_patch, DEFVAL(true));
	ClassDB::bind_static_method("TranslationPatchServer", D_METHOD("remove_patch", "patch", "notify"), &TranslationPatchServer::remove_patch, DEFVAL(true));
	ClassDB::bind_static_method("TranslationPatchServer", D_METHOD("add_patch_dir", "dir", "notify"), &TranslationPatchServer::add_patch_dir, DEFVAL(true));
	ClassDB::bind_static_method("TranslationPatchServer", D_METHOD("remove_patch_dir", "dir", "notify"), &TranslationPatchServer::remove_patch_dir, DEFVAL(true));
}