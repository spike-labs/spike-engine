/**
 * script_annotation_parser.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "../annotation_resource.h"
#include "core/templates/vector.h"

class ScriptAnnotationParser : public RefCounted {
public:
	virtual String get_script_extension() const { ERR_FAIL_V_MSG("", vformat("%s::get_script_extension() must be overrided to return a valid script extension.", get_class_name())); }
	virtual Ref<ClassAnnotation> parse(const Ref<class Script> &p_script);

protected:
	// Annotation keywords
	const String ANNO = "@anno";
	const String OPEN_SOURCE = "@open_source";

	// These keywords can be use to get member identifier like this:
	//		# @anno_property : prop_name
	// A better way to collect member identifier is by using “# @anno” to mark and
	// assigning regex pattern to extract identifier from next code line,
	const String ANNO_CLASS_NAME = "@anno_class_name"; // For class name
	const String ANNO_CLASS = "@anno_class"; // For internal class
	const String ANNO_PROPERTY = "@anno_property";
	const String ANNO_CONST = "@anno_const";
	const String ANNO_ENUM = "@anno_enum";
	const String ANNO_METHOD = "@anno_method";
	const String ANNO_SIGNAL = "@anno_signal";

	const String READONLY = "@readonly"; // Property only.
	const String DEFAULT = "@default"; // Must be put on the end of line.

	const String PARAM = "@param"; // (Method or Signal) Must be put on the begin of line.
	const String RETURN = "@return"; // (Method only)
	const String TYPE = "@type"; //
	const String BASE = "@base"; //
	const String VALUE = "@value"; // (For constant)
	const String ENUM_VALUE = "@enum_value"; // (For Enum)

	// Get an identifier to reconize the start of annotion comment line.
	// Return ScriptAnnotationParser::get_single_line_comment_delimiter() as default.
	virtual String get_annotion_start_identifier() const;
	// Get single line comment delimiter for this script language.
	String get_single_line_comment_delimiter() const;

	// Use next code line using comment block to parse and get annotation infomations.
	// If some type of member is not supported in your script language, please override it and just return nullptr.
	virtual Ref<PropertyAnnotation> parse_property(List<String> p_comment_block, const String &p_next_code_line);
	virtual Ref<MethodAnnotation> parse_method(List<String> p_comment_block, const String &p_next_code_line);
	virtual Ref<SignalAnnotation> parse_signal(List<String> p_comment_block, const String &p_next_code_line);
	virtual Ref<ConstantAnnotation> parse_constant(List<String> p_comment_block, const String &p_next_code_line);
	virtual Ref<EnumAnnotation> parse_enum(List<String> p_comment_block, const String &p_next_code_line);
	virtual Ref<ClassAnnotation> parse_internal_class(List<String> p_comment_block, const String &p_next_code_line);
	virtual bool parse_class_name(List<String> p_comment_block, const String &p_next_code_line, String &r_class_name, String &r_class_comment);

	// Default is not support internal class.
	virtual bool is_leave_internal_class_scope_at_next_code_line(const String &p_next_line) { return false; }

	// Some script is support seperate one code line to multi line, default is only read 1 line.
	// The passed parameter `r_line_index` is point to the end line which is read.
	// If you override this method, you should let the parameter `r_line_index` point to the line of the end which is read,
	virtual String get_next_code_line(const Vector<String> &p_lines, int &r_line_index) const;

	// In order to extract identifier, This keyword should be used in regex patterns.
	const String IDENTIFIER = "identifier";
	// Regex patterns for extract identifier in code line, you need to assign them before parsing script.
	String class_name_regex_pattern;
	String const_regex_pattern;
	String enum_regex_pattern;
	String method_regex_pattern;
	String property_regex_pattern;
	String signal_regex_pattern;
	String internal_class_regex_pattern;
	// String enum_element_regex_pattern;

	// Try get from comment line first, and will get from code line with regex if the result of getting from comment line is empty.
	String try_get_identifier(const String &p_comment_line, const String &p_annotation_keyword, const String &p_next_code_line, const String &p_regex_pattern);

	bool is_line_begins_with_annotation_keyword(const String &p_line, const String &p_keyword);
	String get_identifier_from_comment_line(const String &p_comment_line, const String &p_annotation_keyword);
	String get_identifier_from_code_line(const String &p_next_code_line, const String &p_regex_pattern);
	String get_line_end_comment(const String &p_line);
	String get_line_colon_value(const String &p_line, const String &p_keyword);
	String get_line_default_value(const String &p_line);

	//
	Ref<ClassAnnotation> get_first_parsing_class_annotation() { return parsing_class_annotations.front()->get(); }
	Ref<ClassAnnotation> get_last_parsing_class_annotation() { return parsing_class_annotations.back()->get(); }
	void pop_last_parsing_class_annotation() { parsing_class_annotations.pop_back(); }
	int get_parsing_class_annotation_count() const { return parsing_class_annotations.size(); }
	void push_parsing_class_annotation(Ref<ClassAnnotation> p_class_annotation) { parsing_class_annotations.push_back(p_class_annotation); }
	Ref<Script> get_parsing_script() { return parsing_class_annotations.back()->get()->get_target_resource(); };

	bool register_this_parser() const;
	bool unregister_this_parser() const;

private:
	List<Ref<ClassAnnotation>> parsing_class_annotations;

	void start_parse(const Ref<Script> &p_parsing_script);
	void finish_parse() { parsing_class_annotations.clear(); }

public:
	// If argument "p_return_type" is empty String, please check "p_return_is_variant", if "p_return_is_variant" is false, it means that return is "void", else return is "Variant".
	virtual String make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args,
			const String &p_return_type = "", bool p_return_is_variant = false, const String &p_script_extension = "gd") const;
};

class DefaultScriptAnnotationParser : public ScriptAnnotationParser {
public:
	virtual String get_script_extension() const {
		ERR_FAIL_COND_V(parsing_script.is_null(), "");
		return parsing_script->get_language()->get_extension();
	}
	virtual Ref<ClassAnnotation> parse(const Ref<class Script> &p_script) {
		parsing_script = p_script;
		auto ret = ScriptAnnotationParser::parse(p_script);
		parsing_script = Variant();
		return ret;
	}

private:
	Ref<Script> parsing_script;
};