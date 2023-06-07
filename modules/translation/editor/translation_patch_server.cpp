/**
 * translation_patch_server.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "../translation_patch_server.h"
#include "editor/doc_translations.gen.h"
#include "editor/editor_settings.h"
#include "editor/spike_editor_translations.gen.h"

void TranslationPatchServer::tool_add_gettext_stream_patch(const String &p_locale, const bool &p_notify) {
	EditorTranslationList *etl = _editor_translations;
	while (etl->data) {
		if (etl->lang == p_locale) {
			Vector<uint8_t> data;
			data.resize(etl->uncomp_size);
			int ret = Compression::decompress(data.ptrw(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);
			ERR_FAIL_COND_MSG(ret == -1, "Compressed file is corrupt.");
			if (tool_patch_list.add_patch(create_gettext_patch(data, get_pointer_address_path(etl->data)), p_notify) == OK) {
				break;
			}
		}
		etl++;
	}
}

void TranslationPatchServer::tool_remove_gettext_stream_patch(const bool &p_notify) {
	String locale = get_tool_locale();
	EditorTranslationList *etl = _editor_translations;
	while (etl->data) {
		if (etl->lang == locale) {
			tool_patch_list.remove_patch(get_pointer_address_path(etl->data), p_notify);
			break;
		}
		etl++;
	}
}

void TranslationPatchServer::doc_add_gettext_stream_patch(const String &p_locale, const bool &p_notify) {
	DocTranslationList *etl = _doc_translations;
	while (etl->data) {
		if (etl->lang == p_locale) {
			Vector<uint8_t> data;
			data.resize(etl->uncomp_size);
			int ret = Compression::decompress(data.ptrw(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);
			ERR_FAIL_COND_MSG(ret == -1, "Compressed file is corrupt.");
			if (doc_patch_list.add_patch(create_gettext_patch(data, get_pointer_address_path(etl->data)), p_notify) == OK) {
				break;
			}
		}
		etl++;
	}
}

void TranslationPatchServer::doc_remove_gettext_stream_patch(const bool &p_notify) {
	String locale = get_tool_locale();
	DocTranslationList *etl = _doc_translations;
	while (etl->data) {
		if (etl->lang == locale) {
			doc_patch_list.remove_patch(get_pointer_address_path(etl->data), p_notify);
			break;
		}
		etl++;
	}
}

void TranslationPatchServer::set_editor_pseudolocalization(bool p_enabled) {
	TranslationServer::set_editor_pseudolocalization(p_enabled);
	if (!is_spike_editor_translation_loaded) {
		is_spike_editor_translation_loaded = true;
		String lang = EditorSettings::get_singleton()->get("interface/editor/editor_language");
		tool_add_gettext_stream_patch(lang, false);
		doc_add_gettext_stream_patch(lang, false);
	}
}

StringName TranslationPatchServer::tool_translate(const StringName &p_message, const StringName &p_context) const {
	StringName r;
	if (p_context == CONTEXT_SPIKE) {
		r = tool_patch_list.find_translation_message(get_tool_locale(), p_message, p_context);
		if (r) {
			return editor_pseudolocalization ? tool_pseudolocalize(r) : r;
		}
	} else if (!p_context) {
		r = tool_patch_list.find_translation_message(get_tool_locale(), p_message, CONTEXT_OVERRIDE);
		if (r) {
			return editor_pseudolocalization ? tool_pseudolocalize(r) : r;
		}
	}
	return TranslationServer::tool_translate(p_message, p_context);
}

StringName TranslationPatchServer::tool_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	StringName r = tool_patch_list.find_translation_plural_message(get_tool_locale(), p_message, p_message_plural, p_n, p_context);
	if (r) {
		return r;
	}
	return TranslationServer::tool_translate_plural(p_message, p_message_plural, p_n, p_context);
}

StringName TranslationPatchServer::doc_translate(const StringName &p_message, const StringName &p_context) const {
	StringName r = doc_patch_list.find_translation_message(get_tool_locale(), p_message, p_context);
	if (r) {
		return r;
	}
	return TranslationServer::doc_translate(p_message, p_context);
}

StringName TranslationPatchServer::doc_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	StringName r = doc_patch_list.find_translation_plural_message(get_tool_locale(), p_message, p_message_plural, p_n, p_context);
	if (r) {
		return r;
	}
	return TranslationServer::doc_translate_plural(p_message, p_message_plural, p_n, p_context);
}