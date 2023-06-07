/**
 * luascript_annotation_parser.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "script_annotation_parser.h"

class LuaScriptAnnotationParser : public ScriptAnnotationParser {
public:
	virtual String get_script_extension() const override { return "lua"; }
	virtual Ref<ClassAnnotation> parse(const Ref<Script> &p_script) override;

protected:
	// Unsupported member type.
	Ref<ConstantAnnotation> parse_constant(List<String> p_comment_block, const String &p_next_code_line) override { return nullptr; }
	Ref<EnumAnnotation> parse_enum(List<String> p_comment_block, const String &p_next_code_line) override { return nullptr; }
	Ref<ClassAnnotation> parse_internal_class(List<String> p_comment_block, const String &p_next_code_line) override { return nullptr; }

	virtual String make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args,
			const String &p_return_type = "", bool p_return_is_variant = false, const String &p_script_extension = "gd") const override;

private:
	String parsing_script_class_name;
	void modify_regex_pattren(const Ref<Script> &p_script);
};
