/**
 * script_annotation_parser.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "script_annotation_parser.h"
#include "annotation/script_annotation_parser_manager.h"
#include "modules/regex/regex.h"

Ref<ClassAnnotation> ScriptAnnotationParser::parse(const Ref<class Script> &p_script) {
	if (p_script->get_source_code().is_empty()) {
		return nullptr;
	}
	start_parse(p_script);

	auto lines = p_script->get_source_code().split("\n", false);

	for (auto i = 0; i < lines.size(); ++i) {
		if (is_line_begins_with_annotation_keyword(lines[i], OPEN_SOURCE)) {
			get_first_parsing_class_annotation()->set_open_source(true);
		} else {
			if (is_line_begins_with_annotation_keyword(lines[i], ANNO)) {
				// Collect comment block.
				List<String> comment_block;
				comment_block.push_back(lines[i]);
				while ((i + 1) < lines.size() && lines[i + 1].begins_with(get_single_line_comment_delimiter())) {
					i += 1;
					comment_block.push_back(lines[i]);
				}
				ERR_CONTINUE(comment_block.size() <= 0);

				// Get next code line.
				String next_line = get_next_code_line(lines, i);
				if (next_line.is_empty()) {
					continue; // Invalid annotation.
				}

				// Leave internal class scope or not.
				while (is_leave_internal_class_scope_at_next_code_line(next_line)) {
					pop_last_parsing_class_annotation();
					ERR_FAIL_COND_V(get_parsing_class_annotation_count() == 0, nullptr);
				}

				// Internal class
				auto internal_class_anno = parse_internal_class(comment_block, next_line);
				if (internal_class_anno.is_valid()) {
					if (get_first_parsing_class_annotation()->is_open_source()) {
						internal_class_anno->set_open_source(true);
					}
					get_last_parsing_class_annotation()->add_internal_class(internal_class_anno);
					push_parsing_class_annotation(internal_class_anno);
					continue;
				}

				// Constant
				auto const_anno = parse_constant(comment_block, next_line);
				if (const_anno.is_valid()) {
					get_last_parsing_class_annotation()->add_const(const_anno);
					continue;
				}
				// Signal
				auto signal_anno = parse_signal(comment_block, next_line);
				if (signal_anno.is_valid()) {
					get_last_parsing_class_annotation()->add_signal(signal_anno);
					continue;
				}
				// Enum
				auto enum_anno = parse_enum(comment_block, next_line);
				if (enum_anno.is_valid()) {
					get_last_parsing_class_annotation()->add_enum(enum_anno);
					continue;
				}
				// Property
				auto property_anno = parse_property(comment_block, next_line);
				if (property_anno.is_valid()) {
					get_last_parsing_class_annotation()->add_property(property_anno);
					continue;
				}
				// Method
				auto method_anno = parse_method(comment_block, next_line);
				if (method_anno.is_valid()) {
					get_last_parsing_class_annotation()->add_method(method_anno);
					continue;
				}
				// Class name and comment
				String class_name, class_comment;
				if (parse_class_name(comment_block, next_line, class_name, class_comment)) {
					auto first_class_anno = get_first_parsing_class_annotation();
					first_class_anno->set_class_name_or_script_path(class_name);
					String comment = first_class_anno->get_comment();
					if (!comment.is_empty()) {
						first_class_anno->set_comment(comment + "\n" + class_comment);
					} else {
						first_class_anno->set_comment(class_comment);
					}
					continue;
				}
				// Just comment which need be added to annotation.
				if (is_line_begins_with_annotation_keyword(comment_block[0], "@anno ")) {
					String comment = get_first_parsing_class_annotation()->get_comment();
					for (auto i = 0; i < comment_block.size(); ++i) {
						auto segment_comment = get_line_end_comment(comment_block[i]);
						if (!comment.is_empty()) {
							comment += "\n";
						}
						comment += segment_comment;
					}
					get_first_parsing_class_annotation()->set_comment(comment);
				}
			} else {
				if (lines[i].strip_edges().is_empty()) {
					continue;
				}

				// Leave internal class scope or not.
				while (is_leave_internal_class_scope_at_next_code_line(lines[i])) {
					pop_last_parsing_class_annotation();
					ERR_FAIL_COND_V(get_parsing_class_annotation_count() == 0, nullptr);
				}

				// Internal class
				auto internal_class_anno = parse_internal_class({}, lines[i]);
				if (internal_class_anno.is_valid()) {
					if (get_first_parsing_class_annotation()->is_open_source()) {
						internal_class_anno->set_open_source(true);
					}
					get_last_parsing_class_annotation()->add_internal_class(internal_class_anno);
					push_parsing_class_annotation(internal_class_anno);
					continue;
				}
			}
		}
	}

	auto ret = get_first_parsing_class_annotation();
	finish_parse();

	return ret;
}

String ScriptAnnotationParser::get_annotion_start_identifier() const {
	return get_single_line_comment_delimiter();
}

String ScriptAnnotationParser::get_single_line_comment_delimiter() const {
	return ScriptAnnotationParserManager::get_single_line_comment_delimiter(get_script_extension());
}

bool ScriptAnnotationParser::is_line_begins_with_annotation_keyword(const String &p_line, const String &p_keyword) {
	if (p_line.strip_edges(true, false).begins_with(ScriptAnnotationParserManager::get_single_line_comment_delimiter(get_script_extension()))) {
		RegEx regex(vformat(R"XXX((\s*)%s(\s*)%s)XXX", get_annotion_start_identifier(), p_keyword));
		return regex.search(p_line).is_valid();
	}
	return false;
}

String ScriptAnnotationParser::try_get_identifier(
		const String &p_comment_line, const String &p_annotation_keyword,
		const String &p_next_code_line, const String &p_regex_pattern) {
	auto identifier = get_identifier_from_comment_line(p_comment_line, p_annotation_keyword);
	if (identifier.is_empty()) {
		return get_identifier_from_code_line(p_next_code_line, p_regex_pattern);
	}
	return identifier;
}

String ScriptAnnotationParser::get_identifier_from_comment_line(const String &p_comment_line, const String &p_annotation_keyword) {
	return get_line_colon_value(p_comment_line, p_annotation_keyword);
}

String ScriptAnnotationParser::get_identifier_from_code_line(const String &p_next_code_line, const String &p_regex_pattern) {
	RegEx regex(p_regex_pattern);
	if (p_next_code_line.strip_edges().begins_with(get_single_line_comment_delimiter())) {
		return {};
	}
	auto res = regex.search(p_next_code_line);
	if (res.is_valid()) {
		return res->get_string(IDENTIFIER);
	}
	return "";
}

String ScriptAnnotationParser::get_next_code_line(const Vector<String> &p_lines, int &r_line_index) const {
	while (r_line_index + 1 < p_lines.size()) {
		r_line_index += 1;
		if (r_line_index < p_lines.size()) {
			auto line = p_lines[r_line_index].strip_edges();

			if (p_lines[r_line_index].strip_edges().begins_with(get_single_line_comment_delimiter())) {
				return ""; // Unable to reach a valid code line.
			}

			if (!line.is_empty()) {
				return p_lines[r_line_index];
			}
		}
	}
	return "";
}

Ref<PropertyAnnotation> ScriptAnnotationParser::parse_property(List<String> p_comment_block, const String &p_next_code_line) {
	String identifier = try_get_identifier(p_comment_block[0], ANNO_PROPERTY, p_next_code_line, property_regex_pattern);
	if (identifier.is_empty()) {
		return nullptr;
	}

	List<PropertyInfo> script_properties;
	get_parsing_script()->get_script_property_list(&script_properties);

	const PropertyInfo *property_info = nullptr;
	for (auto i = 0; i < script_properties.size(); ++i) {
		if (script_properties[i].name == identifier) {
			property_info = &script_properties[i];
			break;
		}
	}

	if (property_info == nullptr) {
		return nullptr;
	}

	Ref<PropertyAnnotation> ret;
	ret.instantiate();

	ret->set_member_name(identifier);
	ret->set_type(property_info->type);

	String type_text;

	String default_text;

	String comment;

	for (auto i = 0; i < p_comment_block.size(); ++i) {
		auto line = p_comment_block[i];

		if (line.contains(READONLY)) {
			ret->set_readonly(true);
		}

		if (type_text.is_empty()) {
			type_text = get_line_colon_value(line, TYPE);
		}

		if (default_text.is_empty()) {
			default_text = get_line_default_value(line);
			if (!default_text.is_empty()) {
				continue;
			} else if (line.contains(DEFAULT)) {
				// Using @default without colon and value text to require collect default value automatically,
				// and will push a warning if parser can't get it.
				get_parsing_script()->update_exports();
				Variant val;
				get_parsing_script()->get_property_default_value(identifier, val);
				if (val.get_type() == Variant::NIL) {
					WARN_PRINT(vformat("Annotation parser warining: getting default value of property \"%s\" in \"%s\" is failed. If it is not a exported property, please specify its default value by using \"@default: default_value_text\".", identifier, get_parsing_script()->get_path()));
				} else {
					default_text = val.stringify();
				}
			}
		}

		auto segment_comment = get_line_end_comment(line);
		if (!segment_comment.is_empty()) {
			if (!comment.is_empty()) {
				comment += "\n";
			}
			comment += segment_comment;
		}
	}

	if (!type_text.is_empty()) {
		ret->set_class_name(type_text);
	} else if (!String(property_info->class_name).is_empty()) {
		ret->set_class_name(property_info->class_name);
	}

	ret->set_comment(comment);
	if (default_text.is_empty()) {
		// If still has not default value, try to collect it if valid.
		Variant val;
		if (get_parsing_script()->get_property_default_value({ property_info->name }, val)) {
			if (val.get_type() != Variant::NIL) {
				default_text = val.stringify();
			}
		}

		if (default_text.is_empty() && property_info->type == Variant::OBJECT) {
			default_text = "null";
		}
	}

	ret->set_default_value_text(default_text);

	return ret;
}

Ref<MethodAnnotation> ScriptAnnotationParser::parse_method(List<String> p_comment_block, const String &p_next_code_line) {
	String identifier = try_get_identifier(p_comment_block[0], ANNO_METHOD, p_next_code_line, method_regex_pattern);
	if (identifier.is_empty()) {
		return nullptr;
	}

	List<MethodInfo> script_methods;
	get_parsing_script()->get_script_method_list(&script_methods);

	const MethodInfo *method_info = nullptr;
	for (auto i = 0; i < script_methods.size(); ++i) {
		if (script_methods[i].name == identifier) {
			method_info = &script_methods[i];
			break;
		}
	}
	ERR_FAIL_COND_V(method_info == nullptr, nullptr);

	Ref<MethodAnnotation> ret;
	ret.instantiate();

	ret->set_member_name(identifier);
	//collect from MethodInfo
	// static
	if (method_info->flags & MethodFlags::METHOD_FLAG_STATIC) {
		ret->set_static(true);
	}

	// parameters
	HashMap<StringName, Ref<ParameterAnnotation>> params;
	for (auto &&arg_info : method_info->arguments) {
		Ref<ParameterAnnotation> arg;
		arg.instantiate();
		arg->set_member_name(arg_info.name);
		arg->set_type(arg_info.type);
		arg->set_class_name(arg_info.class_name);
		// arg->set_default_value_text(const String &p_default_value_text)
		params.insert(arg->get_member_name(), arg);
		//
		ret->push_parameter(arg);
	}

	for (auto i = 0; i < method_info->default_arguments.size(); ++i) {
		auto default_val = method_info->default_arguments[method_info->default_arguments.size() - i - 1];
		ret->get_parameter(ret->get_parameter_count() - i - 1)->set_default_value_text(default_val.stringify());
	}

	// return value
	Ref<VariableAnnotation> return_val;
	return_val.instantiate();
	return_val->set_type(method_info->return_val.type);
	return_val->set_class_name(method_info->return_val.class_name);
	ret->set_return_value(return_val);

	// Parse from comment block.
	enum class CommentType {
		METHOD,
		PARAM,
		RETURN,
	};

	CommentType comment_type = CommentType::METHOD;
	StringName commenting_arg;
	for (auto i = 0; i < p_comment_block.size(); ++i) {
		auto line = p_comment_block[i];
		// return type
		auto return_type = get_line_colon_value(line, RETURN);
		if (!return_type.is_empty()) {
			return_val->set_class_name(return_type);
			comment_type = CommentType::RETURN;
		} else if (line.contains(RETURN)) {
			comment_type = CommentType::RETURN;
		}

		// param
		auto param_name = get_line_colon_value(line, PARAM);
		if (!param_name.is_empty()) {
			if (params.has(StringName(param_name))) {
				commenting_arg = param_name;
				comment_type = CommentType::PARAM;
			}
		}

		if (comment_type == CommentType::PARAM) {
			auto param_type = get_line_colon_value(line, TYPE);
			if (!param_type.is_empty()) {
				params[commenting_arg]->set_class_name(param_type);
			}

			auto default_value = get_line_default_value(line);
			if (!default_value.is_empty()) {
				params[commenting_arg]->set_default_value_text(default_value);
			}

			auto segment_comment = get_line_end_comment(line);
			if (!segment_comment.is_empty()) {
				auto comment = params[commenting_arg]->get_comment();
				if (!comment.is_empty()) {
					comment += "\n";
				}
				comment += segment_comment;

				params[commenting_arg]->set_comment(comment);
			}
		} else {
			auto segment_comment = get_line_end_comment(line);
			if (!segment_comment.is_empty()) {
				if (comment_type == CommentType::METHOD) {
					auto comment = ret->get_comment();
					if (!comment.is_empty()) {
						comment += "\n";
					}
					comment += segment_comment;
					ret->set_comment(comment);
				} else {
					auto comment = return_val->get_comment();
					if (!comment.is_empty()) {
						comment += "\n";
					}
					comment += segment_comment;
					return_val->set_comment(comment);
				}
			}
		}
	}

	return ret;
}

Ref<SignalAnnotation> ScriptAnnotationParser::parse_signal(List<String> p_comment_block, const String &p_next_code_line) {
	String identifier = try_get_identifier(p_comment_block[0], ANNO_SIGNAL, p_next_code_line, signal_regex_pattern);
	if (identifier.is_empty()) {
		return nullptr;
	}

	List<MethodInfo> script_signals;
	get_parsing_script()->get_script_signal_list(&script_signals);

	MethodInfo signal_info;
	for (auto i = 0; i < script_signals.size(); ++i) {
		if (script_signals[i].name == identifier) {
			signal_info = script_signals[i];
			break;
		}
	}
	ERR_FAIL_COND_V(signal_info.name.is_empty(), nullptr);

	Ref<SignalAnnotation> ret;
	ret.instantiate();

	ret->set_member_name(identifier);
	//collect from MethodInfo
	// parameters
	HashMap<StringName, Ref<ParameterAnnotation>> params;
	for (auto &&arg_info : signal_info.arguments) {
		Ref<ParameterAnnotation> arg;
		arg.instantiate();
		arg->set_member_name(arg_info.name);
		arg->set_type(arg_info.type);
		arg->set_class_name(arg_info.class_name);
		// arg->set_default_value_text(const String &p_default_value_text)
		params.insert(arg->get_member_name(), arg);
		//
		ret->push_parameter(arg);
	}

	// Parse from comment block.
	enum class CommentType {
		SIGNAL,
		PARAM
	};

	CommentType comment_type = CommentType::SIGNAL;
	StringName commenting_arg;
	for (auto i = 0; i < p_comment_block.size(); ++i) {
		auto line = p_comment_block[i];

		// param
		auto param_name = get_line_colon_value(line, PARAM);
		if (!param_name.is_empty()) {
			if (params.has(StringName(param_name))) {
				commenting_arg = param_name;
				comment_type = CommentType::PARAM;
			}
		}

		if (comment_type == CommentType::PARAM) {
			auto param_type = get_line_colon_value(line, TYPE);
			if (!param_type.is_empty()) {
				params[commenting_arg]->set_class_name(param_type);
			}

			auto segment_comment = get_line_end_comment(line);
			if (!segment_comment.is_empty()) {
				auto comment = params[commenting_arg]->get_comment();
				if (!comment.is_empty()) {
					comment += "\n";
				}
				comment += segment_comment;

				params[commenting_arg]->set_comment(comment);
			}
		} else {
			auto segment_comment = get_line_end_comment(line);
			auto comment = ret->get_comment();
			if (!comment.is_empty()) {
				comment += "\n";
			}
			comment += segment_comment;
			ret->set_comment(comment);
		}
	}

	return ret;
}

Ref<ConstantAnnotation> ScriptAnnotationParser::parse_constant(List<String> p_comment_block, const String &p_next_code_line) {
	String identifier = try_get_identifier(p_comment_block[0], ANNO_CONST, p_next_code_line, const_regex_pattern);
	if (identifier.is_empty()) {
		return nullptr;
	}

	HashMap<StringName, Variant> constants;
	get_parsing_script()->get_constants(&constants);
	if (!constants.has(StringName(identifier))) {
		return nullptr;
	}
	auto constant = constants[StringName(identifier)];

	Ref<ConstantAnnotation> ret;
	ret.instantiate();

	ret->set_member_name(identifier);
	ret->set_type(constant.get_type());
	ret->set_value_text(constant.stringify());

	String comment;

	for (auto i = 0; i < p_comment_block.size(); ++i) {
		auto line = p_comment_block[i];

		if (ret->get_value_text().is_empty()) {
			ret->set_class_name(get_line_colon_value(line, TYPE));
		}

		auto value_text = get_line_colon_value(line, VALUE);
		if (!value_text.is_empty()) {
			ret->set_value_text(value_text);
		}

		auto segment_comment = get_line_end_comment(line);
		if (!segment_comment.is_empty()) {
			if (!comment.is_empty()) {
				comment += "\n";
			}
			comment += segment_comment;
		}
	}

	ret->set_comment(comment);

	return ret;
}

Ref<EnumAnnotation> ScriptAnnotationParser::parse_enum(List<String> p_comment_block, const String &p_next_code_line) {
	String identifier = try_get_identifier(p_comment_block[0], ANNO_ENUM, p_next_code_line, enum_regex_pattern);
	if (identifier.is_empty()) {
		return nullptr;
	}

	HashMap<StringName, Variant> constants;
	get_parsing_script()->get_constants(&constants);
	if (!constants.has(StringName(identifier))) {
		return nullptr;
	}

	if (constants[StringName(identifier)].get_type() != Variant::DICTIONARY) {
		return nullptr;
	}
	auto enum_dict = constants[StringName(identifier)].operator Dictionary();

	Ref<EnumAnnotation> ret;
	ret.instantiate();
	ret->set_member_name(identifier);

	auto elements = enum_dict.keys();
	HashMap<StringName, Ref<EnumElementAnnotation>> element_annos;
	for (auto i = 0; i < elements.size(); ++i) {
		Ref<EnumElementAnnotation> element_anno;
		element_anno.instantiate();

		element_anno->set_member_name(elements[i]);
		element_anno->set_value(enum_dict[elements[i]]);

		element_annos.insert(StringName(elements[i]), element_anno);
		ret->push_element(element_anno);
	}

	// Parse from comment block.
	enum class CommentType {
		ENUM,
		ENUM_VALUE,
	};

	CommentType comment_type = CommentType::ENUM;
	StringName commenting_value;
	for (auto i = 0; i < p_comment_block.size(); ++i) {
		auto line = p_comment_block[i];
		auto enum_value = get_line_colon_value(line, ENUM_VALUE);
		if (element_annos.has(enum_value)) {
			comment_type = CommentType::ENUM_VALUE;
			commenting_value = enum_value;
		}

		auto segment_comment = get_line_end_comment(line);
		if (!segment_comment.is_empty()) {
			Ref<MemberAnnotation> anno;
			if (comment_type == CommentType::ENUM) {
				anno = ret;
			} else {
				anno = element_annos[commenting_value];
			}

			auto comment = anno->get_comment();
			if (!comment.is_empty()) {
				comment += "\n";
			}
			comment += segment_comment;

			anno->set_comment(comment);
		}
	}

	return ret;
}

Ref<ClassAnnotation> ScriptAnnotationParser::parse_internal_class(List<String> p_comment_block, const String &p_next_code_line) {
	String identifier = try_get_identifier(
			p_comment_block.is_empty() ? "" : p_comment_block[0],
			ANNO_CLASS, p_next_code_line, internal_class_regex_pattern);
	if (identifier.is_empty()) {
		return nullptr;
	}
	HashMap<StringName, Variant> constants;
	get_parsing_script()->get_constants(&constants);
	if (!constants.has(StringName(identifier))) {
		return nullptr;
	}

	Ref<Script> internal_class = constants[StringName(identifier)];
	if (!internal_class->is_valid()) {
		return nullptr;
	}

	Ref<ClassAnnotation> ret;
	ret.instantiate();

	ret->set_class_name_or_script_path(identifier);
	ret->set_target_resource(internal_class);

	// TODO: complete get base class logic.
	if (internal_class->get_base_script().is_valid() && internal_class->get_base_script()->is_valid()) {
		ret->set_base_class_name_or_script_path(internal_class->get_base_script()->get_path());
	} else {
		ret->set_base_class_name_or_script_path(internal_class->get_instance_base_type());
	}

	// comment
	String comment;
	for (auto i = 0; i < p_comment_block.size(); ++i) {
		auto line = p_comment_block[i];

		auto base_text = get_line_colon_value(line, BASE);
		if (!base_text.is_empty()) {
			ret->set_base_class_name_or_script_path(base_text);
		}

		auto segment_comment = get_line_end_comment(line);
		if (!segment_comment.is_empty()) {
			if (!comment.is_empty()) {
				comment += "\n";
			}
			comment += segment_comment;
		}
	}

	ret->set_comment(comment);

	return ret;
}

bool ScriptAnnotationParser::parse_class_name(List<String> p_comment_block, const String &p_next_code_line, String &r_class_name, String &r_class_comment) {
	auto class_name = try_get_identifier(p_comment_block[0], ANNO_CLASS_NAME, p_next_code_line, class_name_regex_pattern);
	if (!class_name.is_empty()) {
		r_class_name = class_name;
		for (auto i = 0; i < p_comment_block.size(); ++i) {
			auto segment_comment = get_line_end_comment(p_comment_block[i]);
			if (!r_class_comment.is_empty()) {
				r_class_comment += "\n";
			}
			r_class_comment += segment_comment;
		}
		return true;
	}
	return false;
}

String ScriptAnnotationParser::get_line_colon_value(const String &p_line, const String &p_keyword) {
	RegEx regex(vformat(R"XXX((\s*)%s(\s*):(\s*))XXX", p_keyword));
	auto match = regex.search(p_line);
	if (match.is_valid()) {
		auto splits = p_line.split(match->get_string(0), false, 1);
		if (splits.size() > 1) {
			auto _splits = splits[splits.size() - 1].split(" ", false);
			if (_splits.size() > 0) {
				return _splits[0];
			}
		}
	}
	return "";
}

String ScriptAnnotationParser::get_line_default_value(const String &p_line) {
	RegEx regex(vformat(R"XXX((\s*)%s(\s*):(\s*))XXX", DEFAULT));
	auto match = regex.search(p_line);
	if (match.is_valid()) {
		auto splits = p_line.split(match->get_string(0), true, 1);
		if (splits.size() > 1) {
			return splits[splits.size() - 1];
		}
	}
	return "";
}

String ScriptAnnotationParser::get_line_end_comment(const String &p_line) {
	auto trimed = p_line.strip_escapes();
	if (trimed.begins_with(get_single_line_comment_delimiter())) {
		auto prefix = get_single_line_comment_delimiter().substr(0, 1);
		while (trimed.begins_with(prefix)) {
			trimed = trimed.trim_prefix(prefix);
		}
	}
	auto comment_splits = trimed.split(" ", false);
	for (auto i = 0; i < comment_splits.size(); ++i) {
		auto seg = comment_splits[i];
		if (!seg.c_escape().is_empty() && !seg.contains(ANNO) &&
				!seg.contains(ANNO_CLASS) &&
				!seg.contains(ANNO_PROPERTY) &&
				!seg.contains(ANNO_CONST) &&
				!seg.contains(ANNO_ENUM) &&
				!seg.contains(ANNO_METHOD) &&
				!seg.contains(ANNO_SIGNAL) &&
				!seg.contains(RETURN) &&
				!seg.contains(DEFAULT) &&
				!seg.contains(PARAM) &&
				!seg.contains(READONLY) &&
				!seg.contains(TYPE) &&
				!seg.ends_with(":")) {
			if (i > 0 && !comment_splits[i - 1].c_escape().ends_with(":")) {
				return String(" ").join(comment_splits.slice(i));
			}
		}
	}
	return String();
}

void ScriptAnnotationParser::start_parse(const Ref<Script> &p_parsing_script) {
	DEV_ASSERT(parsing_class_annotations.size() == 0);
	Ref<ClassAnnotation> anno;
	anno.instantiate();
	anno->set_target_resource(p_parsing_script);
	anno->set_tool(p_parsing_script->is_tool());

	if (p_parsing_script->get_base_script().is_valid()) {
		anno->set_base_class_name_or_script_path(p_parsing_script->get_base_script()->get_path());
	}
	if (anno->get_base_class_name_or_script_path().is_empty()) {
		anno->set_base_class_name_or_script_path(p_parsing_script->get_instance_base_type());
	}

	push_parsing_class_annotation(anno);
}

String ScriptAnnotationParser::make_function(const String &p_class, const String &p_name, const PackedStringArray &p_args,
		const String &p_return_type, bool p_return_is_variant, const String &p_script_extension) const {
	if (p_script_extension.is_empty()) {
		return "";
	}

	for (auto i = 0; i < ScriptServer::get_language_count(); ++i) {
		auto lang = ScriptServer::get_language(i);
		List<String> extensions;
		lang->get_recognized_extensions(&extensions);
		if (extensions.find(p_script_extension)) {
			String function = lang->make_function(p_class, p_name, p_args);
			int insert_pos = function.find("\n");
			const String return_type_hint_text = " You may need to change the return type manually.";
			if (insert_pos >= 0) {
				return function.insert(insert_pos, return_type_hint_text);
			} else {
				return function + return_type_hint_text;
			}
		}
	}

	return "";
}

bool ScriptAnnotationParser::register_this_parser() const {
	return ScriptAnnotationParserManager::register_annotation_parser(Ref<ScriptAnnotationParser>(this));
}

bool ScriptAnnotationParser::unregister_this_parser() const {
	auto extension = get_script_extension();
	ERR_FAIL_COND_V(extension.is_empty(), false);
	if (ScriptAnnotationParserManager::get_annotation_parser(extension) == this) {
		ScriptAnnotationParserManager::unregister_annotation_parser(extension);
		return true;
	}
	return false;
}
