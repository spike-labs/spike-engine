/**
 * gdscript_annotation_parser.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "script_annotation_parser.h"

class GDScriptAnnotationParser : public ScriptAnnotationParser {
public:
	virtual String get_script_extension() const override { return "gd"; }
	virtual Ref<ClassAnnotation> parse(const Ref<Script> &p_script) override;

	virtual String make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args,
			const String &p_return_type = "", bool p_return_is_variant = false, const String &p_script_extension = "gd") const override;

	GDScriptAnnotationParser();

protected:
	Ref<ClassAnnotation> parse_internal_class(List<String> p_comment_block, const String &p_next_code_line) override;
	bool is_leave_internal_class_scope_at_next_code_line(const String &p_next_line) override;
	String get_next_code_line(const Vector<String> &p_lines, int &r_line_index) const override;

private:
	String indence = {};
};