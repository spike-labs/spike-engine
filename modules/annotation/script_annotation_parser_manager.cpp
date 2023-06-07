/**
 * script_annotation_parser_manager.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "script_annotation_parser_manager.h"
#include "annotation/annotation_resource.h"
#include "core/io/resource_loader.h"

Ref<DefaultScriptAnnotationParser> ScriptAnnotationParserManager::default_parser = nullptr;
Vector<Ref<ScriptAnnotationParser>> ScriptAnnotationParserManager::parsers = {};
VMap<String, String> ScriptAnnotationParserManager::single_line_comment_delimiters = {};

void ScriptAnnotationParserManager::setup() {
	for (auto i = 0; i < ScriptServer::get_language_count(); ++i) {
		auto lang = ScriptServer::get_language(i);
		List<String> delimiters;
		lang->get_comment_delimiters(&delimiters);
		for (auto &&delimiter : delimiters) {
			if (!delimiter.contains(" ")) {
				single_line_comment_delimiters.insert(lang->get_extension(), delimiter);
				break;
			}
		}
	}
	default_parser.instantiate();
}
void ScriptAnnotationParserManager::clean() {
	parsers.clear();
	default_parser = Variant();
}

bool ScriptAnnotationParserManager::register_annotation_parser(const Ref<ScriptAnnotationParser> &p_parser) {
	auto extension = p_parser->get_script_extension();
	ERR_FAIL_COND_V(extension.is_empty(), false);
	for (auto i = 0; i < parsers.size(); ++i) {
		ERR_FAIL_COND_V_MSG(parsers[i]->get_script_extension() == extension, false, vformat("Already has a `ScriptAnnotationParser` which be used to parse *.\"%s\" file.", extension));
	}
	parsers.push_back(p_parser);
	return true;
}

void ScriptAnnotationParserManager::unregister_annotation_parser(const String &p_script_extension) {
	for (auto i = 0; i < parsers.size(); ++i) {
		if (parsers[i]->get_script_extension() == p_script_extension) {
			parsers.remove_at(i);
			return;
		}
	}
}
Ref<ScriptAnnotationParser> ScriptAnnotationParserManager::get_annotation_parser(const String &p_script_extension) {
	for (auto &&parser : parsers) {
		if (parser->get_script_extension() == p_script_extension) {
			return parser;
		}
	}
	ERR_FAIL_V_MSG(nullptr, vformat("Has not a parser be used to parse *.\"\"", p_script_extension));
}

String ScriptAnnotationParserManager::get_single_line_comment_delimiter(const String &p_script_extension) {
	if (single_line_comment_delimiters.has(p_script_extension)) {
		return single_line_comment_delimiters[p_script_extension];
	}
	return {};
}

Ref<ClassAnnotation> try_parse(Ref<ScriptAnnotationParser> p_parser, const Ref<Script> &script) {
	auto anno = p_parser->parse(script);
	if (anno.is_valid()) {
		if (anno->is_needed()) {
			if (anno->get_class_name_or_script_path().is_empty()) {
				anno->set_class_name_or_script_path(script->get_path());
			}
			return anno;
		}
	}

	return nullptr;
}

Ref<ClassAnnotation> ScriptAnnotationParserManager::get_script_annotation(const String &p_script_path) {
	Ref<Script> script = ResourceLoader::load(p_script_path, "Script", ResourceFormatLoader::CACHE_MODE_IGNORE);
	if (script.is_valid() && script->is_valid()) {
		return get_script_annotation_by_script(script);
	}
	return nullptr;
}

Ref<ClassAnnotation> ScriptAnnotationParserManager::get_script_annotation_by_script(const Ref<Script> &p_script) {
	if (p_script.is_valid() && p_script->is_valid()) {
		auto extension = p_script->get_path().get_extension();
		if (single_line_comment_delimiters.has(extension)) {
			for (auto i = 0; i < parsers.size(); ++i) {
				if (parsers[i]->get_script_extension() == extension) {
					return try_parse(parsers[i], p_script);
				}
			}
			return try_parse(default_parser, p_script);
		}
	}
	return nullptr;
}