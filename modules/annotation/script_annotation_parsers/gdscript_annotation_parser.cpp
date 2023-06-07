/**
 * gdscript_annotation_parser.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "gdscript_annotation_parser.h"
#include "modules/gdscript/gdscript.h"

bool GDScriptAnnotationParser::is_leave_internal_class_scope_at_next_code_line(const String &p_next_line) {
	if (p_next_line.strip_escapes().is_empty()) {
		return false;
	}
	String next_indence = "";
	auto tmp = p_next_line.strip_edges(true, false);
	next_indence = p_next_line.substr(0, p_next_line.size() - tmp.size());

	if (next_indence.size() < indence.size()) {
		indence = indence.substr(1);
		return true;
	}
	return false;
}

Ref<ClassAnnotation> GDScriptAnnotationParser::parse_internal_class(List<String> p_comment_block, const String &p_next_code_line) {
	auto ret = ScriptAnnotationParser::parse_internal_class(p_comment_block, p_next_code_line);
	if (ret.is_valid()) {
		indence += "\t";
	}
	return ret;
}

String GDScriptAnnotationParser::get_next_code_line(const Vector<String> &p_lines, int &r_line_index) const {
	String next_line;
	while ((r_line_index + 1) < p_lines.size()) {
		r_line_index += 1;
		next_line += next_line.is_empty() ? p_lines[r_line_index] : p_lines[r_line_index].strip_edges(true, false);
		if (next_line.ends_with("\\") && !next_line.contains(get_single_line_comment_delimiter())) {
			continue;
		}
		break;
	}
	return next_line;
}

Ref<ClassAnnotation> GDScriptAnnotationParser::parse(const Ref<Script> &p_script) {
	auto ret = ScriptAnnotationParser::parse(p_script);
	indence = "";
	return ret;
}

GDScriptAnnotationParser::GDScriptAnnotationParser() {
	class_name_regex_pattern = R"XXX(^(\s*)class_name(\s+)(?<identifier>[a-zA-Z0-9_]+))XXX";
	const_regex_pattern = R"XXX(^(\s*)const(\s+)(?<identifier>[a-zA-Z0-9_]+))XXX";
	enum_regex_pattern = R"XXX(^(\s*)enum(\s+)(?<identifier>[a-zA-Z0-9_]+)(\s*){)XXX";
	// enum_element_regex_pattern = R"XXX(^(\s*)(?<identifier>[a-zA-Z0-9_]+)(\s*),(\s*)##(?<comment>(.*)$)?)XXX";
	method_regex_pattern = R"XXX(^(\s*)(?<static>static)?(\s*)func(\s+)(?<identifier>[a-zA-Z0-9_]+)(\s*)\()XXX";
	property_regex_pattern = R"XXX(^(\s*)(@export([a-zA-Z0-9_]+(\(.+\))?)?)?(\s*)(\s*)var(\s+)(?<identifier>[a-zA-Z0-9_]+)(\s*))XXX";
	signal_regex_pattern = R"XXX(^(\s*)signal(\s+)(?<identifier>[a-zA-Z0-9_]+)(\s*))XXX";
	internal_class_regex_pattern = R"XXX(^(\s*)class(\s+)(?<identifier>[a-zA-Z0-9_]+)(\s*))XXX";
}

String GDScriptAnnotationParser::make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args,
		const String &p_return_type, bool p_return_is_variant, const String &p_script_extension) const {
	auto ret = GDScriptLanguage::get_singleton()->make_function(p_class, p_name, p_args);
	if (ret.find("-> void") < 0) {
		return ret;
	}

	auto return_type = p_return_type;
	if (p_return_type.is_empty() && p_return_is_variant) {
		return_type = "Variant";
	}

	if (!return_type.is_empty()) {
		return ret.replace("-> void", "-> " + return_type);
	}

	return ret;
}