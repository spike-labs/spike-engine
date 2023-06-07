/**
 * luascript_annotation_parser.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "luascript_annotation_parser.h"
#include "luascript/luascript_language.h"

Ref<ClassAnnotation> LuaScriptAnnotationParser::parse(const Ref<Script> &p_script) {
	modify_regex_pattren(p_script);
	auto ret = ScriptAnnotationParser::parse(p_script);
	if (ret->is_needed()) {
		if (ret->get_class_name_or_script_path().is_empty()) {
			ret->set_class_name_or_script_path(parsing_script_class_name);
		}
	}
	parsing_script_class_name = "";
	return ret;
}

void LuaScriptAnnotationParser::modify_regex_pattren(const Ref<Script> &p_script) {
	auto source_code = p_script->get_source_code();
	auto idx = source_code.rfind("return");
	parsing_script_class_name = source_code.substr(idx + String("return").length()).strip_edges();

	method_regex_pattern = vformat(R"XXX((\s*)function(\s+)%s(\s*):(\s*)(?<identifier>[a-zA-Z0-9_]+)(\s*)\()XXX", parsing_script_class_name);
	property_regex_pattern = vformat(R"XXX(^(\s*)%s(\s*)\.(\s*)(?<identifier>[a-zA-Z0-9_]+)(\s*)=(\s*)(?!signal\())XXX", parsing_script_class_name);
	signal_regex_pattern = vformat(R"XXX(^(\s*)%s(\s*)\.(\s*)(?<identifier>[a-zA-Z0-9_]+)(\s*)=(\s*)signal\()XXX", parsing_script_class_name);
	class_name_regex_pattern = vformat(R"XXX(^(\s*)local(\s+)%s(\s*)=(\s*)class\()XXX", parsing_script_class_name);
}

String LuaScriptAnnotationParser::make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args,
		const String &p_return_type, bool p_return_is_variant, const String &p_script_extension) const {
	return LuaScriptLanguage::get_singleton()->make_function(p_class, p_name, p_args);
}