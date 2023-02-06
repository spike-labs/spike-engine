/**
 * external_translation_server.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include <spike/external_translation_server.h>
#include <editor/spike_editor_translations.gen.h>
#include <editor/doc_translations.gen.h>
#include <editor/editor_settings.h>

void ExternalTranslationServer::load_tool_default_stream(const String &p_locale, const bool& p_notify_change) {
	EditorTranslationList* etl = _editor_translations;
	while (etl->data) {
		if (etl->lang == p_locale) {
			Vector<uint8_t> data;
			data.resize(etl->uncomp_size);
			int ret = Compression::decompress(data.ptrw(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);
			ERR_FAIL_COND_MSG(ret == -1, "Compressed file is corrupt.");
			if (load_tool_external_stream(data, Scheme::GETTEXT, etl->data, p_notify_change) == OK) {
				break;
			}
		}
		etl++;
	}
}

void ExternalTranslationServer::unload_tool_default_stream(const bool& p_notify_change) {
	String locale = get_tool_locale();
	EditorTranslationList* etl = _editor_translations;
	while (etl->data) {
		if (etl->lang == locale) {
			unload_tool_external_stream(etl->data, p_notify_change);
			break;
		}
		etl++;
	}
}

void ExternalTranslationServer::load_doc_default_stream(const String &p_locale, const bool& p_notify_change) {
	DocTranslationList* etl = _doc_translations;
	while (etl->data) {
		if (etl->lang == p_locale) {
			Vector<uint8_t> data;
			data.resize(etl->uncomp_size);
			int ret = Compression::decompress(data.ptrw(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);
			ERR_FAIL_COND_MSG(ret == -1, "Compressed file is corrupt.");
			if (load_doc_external_stream(data, Scheme::GETTEXT, etl->data, p_notify_change) == OK) {
				break;
			}
		}
		etl++;
	}
}

void ExternalTranslationServer::unload_doc_default_stream(const bool& p_notify_change) {
	String locale = get_tool_locale();
	DocTranslationList* etl = _doc_translations;
	while (etl->data) {
		if (etl->lang == locale) {
			unload_doc_external_stream(etl->data, p_notify_change);
			break;
		}
		etl++;
	}
}

void ExternalTranslationServer::set_editor_pseudolocalization(bool p_enabled) {
	TranslationServer::set_editor_pseudolocalization(p_enabled);
	if (!is_spike_editor_translation_loaded) {
		is_spike_editor_translation_loaded = true;
		String lang = EditorSettings::get_singleton()->get("interface/editor/editor_language");
		load_tool_default_stream(lang, false);
		load_doc_default_stream(lang, false);
	}
}

StringName ExternalTranslationServer::tool_translate(const StringName &p_message, const StringName &p_context) const {
	StringName r;
	if (p_context == CONTEXT_SPIKE) {
		r = tool_external_translations.find_translation_message(get_tool_locale(), p_message, p_context);
		if (r) {
			return editor_pseudolocalization ? tool_pseudolocalize(r) : r;
		}
	}
    else if (!p_context) {
        r = tool_external_translations.find_translation_message(get_tool_locale(), p_message, CONTEXT_OVERRIDE);
        if (r) {
			return editor_pseudolocalization ? tool_pseudolocalize(r) : r;
		}
    }
	return TranslationServer::tool_translate(p_message, p_context);
}

StringName ExternalTranslationServer::tool_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	StringName r = tool_external_translations.find_translation_plural_message(get_tool_locale(), p_message, p_message_plural, p_n, p_context);
	if (r) {
		return r;
	}
	return TranslationServer::tool_translate_plural(p_message, p_message_plural, p_n, p_context);
}

StringName ExternalTranslationServer::doc_translate(const StringName &p_message, const StringName &p_context) const {
    StringName r = doc_external_translations.find_translation_message(get_tool_locale(), p_message, p_context);
    if (r) {
        return r;
    }
	return TranslationServer::doc_translate(p_message, p_context);
}

StringName ExternalTranslationServer::doc_translate_plural(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context) const {
	StringName r = doc_external_translations.find_translation_plural_message(get_tool_locale(), p_message, p_message_plural, p_n, p_context);
	if (r) {
		return r;
	}
	return TranslationServer::doc_translate_plural(p_message, p_message_plural, p_n, p_context);
}