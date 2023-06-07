/**
 * translation_patch_server.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/string/translation.h"
#include "core/string/translation_po.h"
#include "modules/spike_define.h"

String get_pointer_address_path(const void *ptr);

class TranslationPatch : public RefCounted {
public:
	Ref<Translation> translation;
	String owner;
};

class TranslationPatchList {
	List<Ref<TranslationPatch>> patch_list;

public:
	Error add_patch(const Ref<TranslationPatch> &p_patch, const bool &p_notify_change = true);
	Error remove_patch(const String &p_owner, const bool &p_notify_change = true);

	bool has_patch(const String &p_owner) const { return find_patch(p_owner) != nullptr; }
	Ref<TranslationPatch> find_patch(const String &p_owner) const;

	void load_dir(const String &p_dir, const bool &p_notify_change = true);
	void unload_dir(const String &p_dir, const bool &p_notify_change = true);

	StringName find_translation_message(const String &p_locale, const StringName &p_message, const StringName &p_context) const;
	StringName find_translation_plural_message(const String &p_locale, const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const;
};

class TranslationPatchServer : public TranslationServer {
	GDCLASS(TranslationPatchServer, TranslationServer);

protected:
	static void _bind_methods();
	static TranslationPatchServer *singleton;
	static Vector<String> csv_extensions;
	static Vector<String> gettext_extensions;
	TranslationPatchList patch_list;

public:
	static TranslationPatchServer *get_singleton();

	static bool is_csv_extension(const String &p_path);
	static bool append_csv_extension(const String &p_extension);
	static bool is_gettext_extension(const String &p_path);
	static bool append_gettext_extension(const String &p_extension);

	static Ref<TranslationPatch> create_gettext_patch(const Ref<FileAccess> &p_access, const String &p_owner);
	static Ref<TranslationPatch> create_gettext_patch(const PackedByteArray &p_stream, const String &p_owner);
	static Ref<TranslationPatch> create_csv_patch(const Ref<FileAccess> &p_access, const String &p_owner);
	static Ref<TranslationPatch> create_file_patch(const String &p_file);

	static Error add_translation_patch(const Ref<Translation> &p_translation, const bool &p_notify = true);
	static Error remove_translation_patch(const Ref<Translation> &p_translation, const bool &p_notify = true);

	static Error add_file_patch(const String &p_file, const bool &p_notify = true);
	static Error remove_file_patch(const String &p_file, const bool &p_notify = true);

	static Error add_gettext_stream_patch(const PackedByteArray &p_stream, const bool &p_notify = true);
	static Error remove_gettext_stream_patch(const PackedByteArray &p_stream, const bool &p_notify = true);

	virtual void load_translations() override;
	virtual StringName translate(const StringName &p_message, const StringName &p_context = "") const override;
	virtual StringName translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context = "") const override;

public:
	static Error add_patch(const Variant &p_patch, const bool &p_notify = true);
	static Error remove_patch(const Variant &p_patch, const bool &p_notify = true);

	static void add_patch_dir(const String &p_dir, const bool &p_notify = true);
	static void remove_patch_dir(const String &p_dir, const bool &p_notify = true);

	TranslationPatchServer() {
		append_csv_extension(".csv");
		append_gettext_extension(".po");
		append_gettext_extension(".mo");
	}
	~TranslationPatchServer() {}

#ifdef TOOLS_ENABLED
protected:
	bool is_spike_editor_translation_loaded;
	TranslationPatchList tool_patch_list;
	TranslationPatchList doc_patch_list;

public:
	void tool_add_gettext_stream_patch(const String &p_locale, const bool &p_notify = true);
	void tool_remove_gettext_stream_patch(const bool &p_notify = true);

	void doc_add_gettext_stream_patch(const String &p_locale, const bool &p_notify = true);
	void doc_remove_gettext_stream_patch(const bool &p_notify = true);

	virtual void set_editor_pseudolocalization(bool p_enabled) override;

	virtual StringName tool_translate(const StringName &p_message, const StringName &p_context = "") const override;
	virtual StringName tool_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context = "") const override;

	virtual StringName doc_translate(const StringName &p_message, const StringName &p_context = "") const override;
	virtual StringName doc_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context = "") const override;
#endif
};