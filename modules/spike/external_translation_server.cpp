/**
 * external_translation_server.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "external_translation_server.h"
#include "godot/core/error/error_macros.h"
#include "godot/core/io/dir_access.h"
#include "godot/core/os/os.h"
#include <core/config/project_settings.h>
#include <core/io/file_access_memory.h>
#include <core/io/translation_loader_po.h>
#include <core/string/translation_po.h>
#include <sstream>

#define LIST_DIR(dir, reader, arg)                         \
	Ref<DirAccess> dir_access = DirAccess::open(dir);      \
	if (dir_access.is_valid()) {                           \
		dir_access->list_dir_begin();                      \
		String file_name = dir_access->get_next();         \
		while (!file_name.is_empty()) {                    \
			if (!dir_access->current_is_dir()) {           \
				if (!file_name.ends_with(".uids")) {       \
					reader(dir.path_join(file_name), arg); \
				}                                          \
			}                                              \
			file_name = dir_access->get_next();            \
		}                                                  \
		dir_access->list_dir_end();                        \
	}

String get_pointer_address_path(const void *ptr) {
	std::ostringstream ss;
	ss << (void *)&ptr;
	return String(ss.str().c_str());
}

Error ExternalTranslationList::add_translation(const Ref<FileAccess> &p_file, const ExternalTranslationScheme &p_scheme, const String &p_owner, const bool &p_notify_change) {
	Ref<ExternalTranslation> translation = ExternalTranslationServer::create_translation(p_file, p_owner, p_scheme);
	if (translation.is_null()) {
		return ERR_CANT_CREATE;
	}
	push_front(translation);
	if (p_notify_change) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
	return OK;
}

Error ExternalTranslationList::remove_translation(const String &p_owner, const bool &p_notify_change) {
	Ref<ExternalTranslation> translation = find_translation(p_owner);
	ERR_FAIL_COND_V(translation == nullptr, ERR_DOES_NOT_EXIST);
	erase(translation);
	if (p_notify_change) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
	return OK;
}

Error ExternalTranslationList::add_file_translation(const Ref<FileAccess> &p_file, const bool &p_notify_change) {
	String owner = p_file->get_path();
	ERR_FAIL_COND_V_MSG(has_translation(owner), ERR_ALREADY_EXISTS, owner);
	ExternalTranslationServer::Scheme scheme = ExternalTranslationServer::Scheme::CSV;
	if (owner.ends_with(".po") || owner.ends_with(".mo")) {
		scheme = ExternalTranslationServer::Scheme::GETTEXT;
	}
	return add_translation(p_file, scheme, owner, p_notify_change);
}

Error ExternalTranslationList::add_stream_translation(const PackedByteArray &p_stream, const ExternalTranslationScheme &p_scheme, const void *p_owner, const bool &p_notify_change) {
	String owner = get_pointer_address_path(nullptr != p_owner ? p_owner : (void *)&p_stream);
	ERR_FAIL_COND_V_MSG(has_translation(owner), ERR_ALREADY_EXISTS, owner);
	Ref<FileAccessMemory> fa;
	fa.instantiate();
	fa->open_custom(p_stream.ptr(), p_stream.size());
	return add_translation(fa, p_scheme, owner, p_notify_change);
}

void ExternalTranslationList::load_dir(const String &p_dir, const bool &p_notify_change) {
	LIST_DIR(p_dir, add_file_translation, false);
	if (p_notify_change) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
}

void ExternalTranslationList::unload_dir(const String &p_dir, const bool &p_notify_change) {
	LIST_DIR(p_dir, remove_file_translation, false);
	if (p_notify_change) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_TRANSLATION_CHANGED);
	}
}

Ref<ExternalTranslation> ExternalTranslationList::find_translation(const String &p_owner) const {
	for (const Element *I = front(); I; I = I->next()) {
		Ref<ExternalTranslation> iek = I->get();
		if (iek.is_valid() && iek->owner == p_owner) {
			return iek;
		}
	}
	return nullptr;
}

StringName ExternalTranslationList::find_translation_message(const String &p_locale, const StringName &p_message, const StringName &p_context) const {
	for (const Element *I = front(); I; I = I->next()) {
		Ref<ExternalTranslation> iek = I->get();
		if (iek.is_valid() && iek->translation->get_locale() == p_locale) {
			StringName r = iek->translation->get_message(p_message, p_context);
			if (r) {
				return r;
			}
		}
	}
	return StringName();
}

StringName ExternalTranslationList::find_translation_plural_message(const String &p_locale, const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	for (const Element *I = front(); I; I = I->next()) {
		Ref<ExternalTranslation> iek = I->get();
		if (iek.is_valid() && iek->translation->get_locale() == p_locale) {
			StringName r = iek->translation->get_plural_message(p_message, p_message_plural, p_n, p_context);
			if (r) {
				return r;
			}
		}
	}
	return StringName();
}

ExternalTranslationServer *ExternalTranslationServer::singleton = nullptr;

Ref<ExternalTranslation> ExternalTranslationServer::create_translation(const Ref<FileAccess> &p_access, const String &p_owner, ExternalTranslationServer::Scheme p_scheme) {
	ERR_FAIL_COND_V_MSG(p_access.is_null(), nullptr, ERR_FILE_CANT_OPEN);
	Ref<ExternalTranslation> spk_translation = nullptr;
	String file_path = p_access->get_path();
	if (p_scheme == ExternalTranslationServer::Scheme::GETTEXT) {
		Ref<Translation> translation = TranslationLoaderPO::load_translation(p_access);
		if (translation.is_valid()) {
			spk_translation = Ref<ExternalTranslation>(memnew(ExternalTranslation));
			spk_translation->translation = translation;
		}
	} else if (p_scheme == ExternalTranslationServer::Scheme::CSV) {
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
			spk_translation = Ref<ExternalTranslation>(memnew(ExternalTranslation));
			spk_translation->owner = p_owner;
			spk_translation->translation = Ref<TranslationPO>(memnew(TranslationPO));
			spk_translation->translation->set_locale(locale);
			line = p_access->get_csv_line();
			while (line.size() > 1) {
				if (locale_index < line.size()) {
					String key = line[0];
					if (!key.is_empty()) {
						spk_translation->translation->add_message(key, line[locale_index].c_unescape());
					}
				}
				line = p_access->get_csv_line();
			}
		}
	}
	return spk_translation;
}

void ExternalTranslationServer::load_translations() {
	TranslationServer::load_translations();
	load_external_dir(OS::get_singleton()->get_resource_dir().path_join("tr"), false);
	load_external_dir(OS::get_singleton()->get_user_data_dir().path_join("tr"), false);
	load_external_dir(OS::get_singleton()->get_executable_path().get_base_dir().path_join("tr"), false);
	//main pack
	String main_pack_path = ProjectSettings::get_singleton()->get_setting("application/run/main_pack", String());
	if (!main_pack_path.is_empty()) {
		main_pack_path = main_pack_path.get_base_dir().path_join("tr");
		if (main_pack_path != OS::get_singleton()->get_executable_path().get_base_dir().path_join("tr")) {
			load_external_dir(main_pack_path, false);
		}
	}

#ifdef MACOS_ENABLED
	if (!Engine::get_singleton()->is_editor_hint()) {
		load_external_dir(OS::get_singleton()->get_bundle_resource_dir().path_join("tr"), false);
	}
#endif
}

StringName ExternalTranslationServer::translate(const StringName &p_message, const StringName &p_context) const {
	if (enabled) {
		StringName r = external_translations.find_translation_message(get_locale(), p_message, p_context);
		if (r) {
			return pseudolocalization_enabled ? pseudolocalize(r) : r;
		}
	}
	return TranslationServer::translate(p_message, p_context);
}

StringName ExternalTranslationServer::translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	if (enabled) {
		StringName r = external_translations.find_translation_plural_message(get_locale(), p_message, p_message_plural, p_n, p_context);
		if (r) {
			return r;
		}
	}
	return TranslationServer::translate_plural(p_message, p_message_plural, p_n, p_context);
}

void ExternalTranslationServer::_bind_methods() {
	BIND_ENUM_CONSTANT(CSV);
	BIND_ENUM_CONSTANT(GETTEXT);
	ClassDB::bind_static_method("ExternalTranslationServer", D_METHOD("load_external_file", "file", "notify_change"), &ExternalTranslationServer::load_external_file, DEFVAL(true));
	ClassDB::bind_static_method("ExternalTranslationServer", D_METHOD("unload_external_file", "file", "notify_change"), &ExternalTranslationServer::unload_external_file, DEFVAL(true));
	ClassDB::bind_static_method("ExternalTranslationServer", D_METHOD("load_external_stream", "stream", "scheme", "notify_change"), &ExternalTranslationServer::load_external_stream, DEFVAL(true));
	ClassDB::bind_static_method("ExternalTranslationServer", D_METHOD("unload_external_stream", "file", "notify_change"), &ExternalTranslationServer::unload_external_stream, DEFVAL(true));
	ClassDB::bind_static_method("ExternalTranslationServer", D_METHOD("load_external_dir", "dir", "notify_change"), &ExternalTranslationServer::load_external_dir, DEFVAL(true));
	ClassDB::bind_static_method("ExternalTranslationServer", D_METHOD("unload_external_dir", "dir", "notify_change"), &ExternalTranslationServer::unload_external_dir, DEFVAL(true));
}