/**
 * script_annotation_parser_manager.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "annotation/script_annotation_parsers/script_annotation_parser.h"
#include "annotation_resource.h"

class ScriptAnnotationParserManager {
public:
	static Ref<ClassAnnotation> get_script_annotation(const String &p_script_path);
	static Ref<ClassAnnotation> get_script_annotation_by_script(const Ref<Script> &p_script);

	static bool register_annotation_parser(const Ref<ScriptAnnotationParser> &p_parser);
	static void unregister_annotation_parser(const String &p_script_extension);
	static Ref<ScriptAnnotationParser> get_annotation_parser(const String &p_script_extension);
	static String get_single_line_comment_delimiter(const String &p_script_extension);

	static void setup();
	static void clean();

private:
	static Vector<Ref<ScriptAnnotationParser>> parsers;
	static VMap<String, String> single_line_comment_delimiters;
	static Ref<DefaultScriptAnnotationParser> default_parser;
};
