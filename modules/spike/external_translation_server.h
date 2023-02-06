/**
 * external_translation_server.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include <modules/spike_define.h>
#include <core/string/translation.h>
#include <core/string/translation_po.h>

String get_pointer_address_path(const void *ptr);

enum ExternalTranslationScheme {
	CSV,
	GETTEXT
};

class ExternalTranslation : public RefCounted {
public:
	Ref<Translation> translation;
	String owner;
};

class ExternalTranslationList : public List<Ref<ExternalTranslation>> {
public:
	Error add_translation(const Ref<FileAccess> &p_access, const ExternalTranslationScheme &p_scheme, const String &p_owner, const bool &p_notify_change = true);
	Error remove_translation(const String &p_owner, const bool &p_notify_change = true);

	Error add_file_translation(const Ref<FileAccess> &p_file, const bool &p_notify_change = true);
	Error remove_file_translation(const Ref<FileAccess> &p_file, const bool &p_notify_change = true) {
		return remove_translation(p_file->get_path(), p_notify_change);
	}

	Error add_file_translation(const String &p_file, const bool &p_notify_change = true) {
		return add_file_translation(FileAccess::open(p_file, FileAccess::ModeFlags::READ), p_notify_change);
	}
	Error remove_file_translation(const String &p_file, const bool &p_notify_change = true) {
		return remove_translation(p_file, p_notify_change);
	}

	Error add_stream_translation(const PackedByteArray &p_stream, const ExternalTranslationScheme &p_scheme, const void *p_owner, const bool &p_notify_change = true);
	Error remove_stream_translation(const void *p_owner, const bool &p_notify_change = true) {
		return remove_translation(get_pointer_address_path(p_owner), p_notify_change);
	}

	void load_dir(const String &p_dir, const bool &p_notify_change = true);
	void unload_dir(const String &p_dir, const bool &p_notify_change = true);

	bool has_translation(const String &p_owner) const {
		return find_translation(p_owner) != nullptr;
	}
	Ref<ExternalTranslation> find_translation(const String &p_owner) const;
	StringName find_translation_message(const String &p_locale, const StringName &p_message, const StringName &p_context) const;
	StringName find_translation_plural_message(const String &p_locale, const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const;
};

class ExternalTranslationServer : public TranslationServer {
	GDCLASS(ExternalTranslationServer, TranslationServer);

protected:
	static void _bind_methods();
	static ExternalTranslationServer *singleton;
	ExternalTranslationList external_translations;

public:
	using Scheme = ExternalTranslationScheme;

	static ExternalTranslationServer *get_singleton() {
		if (nullptr == singleton) {
			singleton = (ExternalTranslationServer*)TranslationServer::get_singleton();
		}
		return singleton;
	}

	static Ref<ExternalTranslation> create_translation(const Ref<FileAccess> &p_file, const String &p_owner, Scheme p_scheme);

	static Error load_external_file(const Ref<FileAccess> &p_file, const bool &p_notify_change = true) {
		return get_singleton()->external_translations.add_file_translation(p_file, p_notify_change);
	}

	static Error unload_external_file(const Ref<FileAccess> &p_file, const bool &p_notify_change = true) {
		return get_singleton()->external_translations.remove_file_translation(p_file, p_notify_change);
	}

	static Error load_external_stream(const PackedByteArray &p_stream, Scheme p_scheme, const bool &p_notify_change = true) {
		return get_singleton()->external_translations.add_stream_translation(p_stream, p_scheme, &p_stream, p_notify_change);
	}

	static Error unload_external_stream(const PackedByteArray &p_stream, const bool &p_notify_change = true) {
		return get_singleton()->external_translations.remove_stream_translation(&p_stream, p_notify_change);
	}

	static void load_external_dir(const String &p_dir, const bool &p_notify_change = true) {
		get_singleton()->external_translations.load_dir(p_dir, p_notify_change);
	}

	static void unload_external_dir(const String &p_dir, const bool &p_notify_change = true) {
		get_singleton()->external_translations.unload_dir(p_dir, p_notify_change);
	}

	virtual void load_translations() override;
	virtual StringName translate(const StringName& p_message, const StringName& p_context = "") const override;
	virtual StringName translate_plural(const StringName& p_message, const StringName& p_message_plural, int p_n, const StringName& p_context = "") const override;

#ifdef TOOLS_ENABLED
protected:
	bool is_spike_editor_translation_loaded;
	ExternalTranslationList tool_external_translations;
	ExternalTranslationList doc_external_translations;

public:
	Error load_tool_external_file(const Ref<FileAccess> &p_file, const bool &p_notify_change = true) {
		return tool_external_translations.add_file_translation(p_file, p_notify_change);
	}
	Error unload_tool_external_file(const Ref<FileAccess> &p_file, const bool &p_notify_change = true) {
		return tool_external_translations.remove_file_translation(p_file, p_notify_change);
	}

	Error load_tool_external_stream(const PackedByteArray &p_stream, Scheme p_scheme, const void *p_owner, const bool &p_notify_change = true) {
		return tool_external_translations.add_stream_translation(p_stream, p_scheme, p_owner, p_notify_change);
	}
	Error unload_tool_external_stream(const void *p_owner, const bool &p_notify_change = true) {
		return tool_external_translations.remove_stream_translation(p_owner, p_notify_change);
	}

	void load_tool_external_dir(const String &p_dir, const bool &p_notify_change = true) {
		tool_external_translations.load_dir(p_dir, p_notify_change);
	}
	void unload_tool_external_dir(const String &p_dir, const bool &p_notify_change = true) {
		tool_external_translations.unload_dir(p_dir, p_notify_change);
	}

	void load_tool_default_stream(const String &p_locale, const bool &p_notify_change = true);
	void unload_tool_default_stream(const bool &p_notify_change = true);

	Error load_doc_external_file(const Ref<FileAccess> &p_file, const bool &p_notify_change = true) {
		return doc_external_translations.add_file_translation(p_file, p_notify_change);
	}
	Error unload_doc_external_file(const Ref<FileAccess> &p_file, const bool &p_notify_change = true) {
		return doc_external_translations.remove_file_translation(p_file, p_notify_change);
	}

	Error load_doc_external_stream(const PackedByteArray &p_stream, Scheme p_scheme, const void *p_owner, const bool &p_notify_change = true) {
		return doc_external_translations.add_stream_translation(p_stream, p_scheme, p_owner, p_notify_change);
	}
	Error unload_doc_external_stream(const void *p_owner, const bool &p_notify_change = true) {
		return doc_external_translations.remove_stream_translation(p_owner, p_notify_change);
	}

	void load_doc_external_dir(const String &p_dir, const bool &p_notify_change = true) {
		doc_external_translations.load_dir(p_dir, p_notify_change);
	}
	void unload_doc_external_dir(const String &p_dir, const bool &p_notify_change = true) {
		doc_external_translations.unload_dir(p_dir, p_notify_change);
	}

	void load_doc_default_stream(const String &p_locale, const bool &p_notify_change = true);
	void unload_doc_default_stream(const bool &p_notify_change = true);

	virtual void set_editor_pseudolocalization(bool p_enabled) override;

	virtual StringName tool_translate(const StringName &p_message, const StringName &p_context = "") const override;
	virtual StringName tool_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context = "") const override;

	virtual StringName doc_translate(const StringName &p_message, const StringName &p_context = "") const override;
	virtual StringName doc_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context = "") const override;
#endif
};

VARIANT_ENUM_CAST(ExternalTranslationServer::Scheme);