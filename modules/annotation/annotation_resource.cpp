/**
 * annotation_resource.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "annotation_resource.h"
#include "annotation/annotation_plugin.h"
#include "annotation/script_annotation_parser_manager.h"
#include "scene/resources/packed_scene.h"

#define TYPED_ARR_RES_PROP_INFO(m_prop_name_str, m_resource_type_str) \
	PropertyInfo(Variant::ARRAY, m_prop_name_str, PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, m_resource_type_str))
// Annotation
void Annotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_comment"), &Annotation::get_comment);
	ClassDB::bind_method(D_METHOD("set_comment", "comment"), &Annotation::set_comment);

	ClassDB::bind_method(D_METHOD("get_display_name"), &Annotation::get_display_name_bind);
	ClassDB::bind_method(D_METHOD("get_member_annotations"), &Annotation::get_member_annotations_bind);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "comment", PROPERTY_HINT_MULTILINE_TEXT), "set_comment", "get_comment");

	ClassDB::bind_static_method("Annotation", D_METHOD("json_to_annotations", "annotations_json"), &AnnotationPlugin::load_annotations_json);
	ClassDB::bind_static_method("Annotation", D_METHOD("annotations_to_json", "annotations"), &Annotation::annotations_to_json_bind);
	ClassDB::bind_static_method("Annotation", D_METHOD("try_get_annotation", "resource"), &AnnotationPlugin::get_annotation_resource);
	ClassDB::bind_static_method("Annotation", D_METHOD("try_get_annotation_by_path", "resource_path"), &AnnotationPlugin::get_annotation_resource_by_path);
}

String Annotation::annotations_to_json_bind(const TypedArray<BaseResourceAnnotation> &p_annotations) {
	HashMap<StringName, List<Ref<BaseResourceAnnotation>>> annos;
	for (auto i = 0; i < p_annotations.size(); ++i) {
		Ref<BaseResourceAnnotation> anno = p_annotations[i];
		if (anno.is_valid()) {
			if (!annos.has(anno->get_class_name())) {
				annos[anno->get_class_name()] = {};
			}
			annos[anno->get_class_name()].push_back(anno);
		}
	}
	return AnnotationPlugin::to_annotations_json(annos);
}

void Annotation::collect_serialize_properties_recurrently(const StringName &p_class, Vector<PropertyInfo> &r_props) {
	{
		List<PropertyInfo> props;
		ClassDB::get_property_list(p_class, &props, true);
		for (auto it = props.begin(); it != props.end(); ++it) {
			r_props.push_back(*it);
		}
	}
	auto base = ClassDB::get_parent_class(p_class);
	if (SNAME("Resource") != base) {
		collect_serialize_properties_recurrently(base, r_props);
	}
}
Vector<PropertyInfo> Annotation::get_serialize_property_list(const StringName &p_class) {
	static HashMap<StringName, Vector<PropertyInfo>> anno_serialize_props;
	if (!anno_serialize_props.has(p_class)) {
		anno_serialize_props.insert(p_class, Vector<PropertyInfo>());
		collect_serialize_properties_recurrently(p_class, anno_serialize_props[p_class]);
	}
	return anno_serialize_props[p_class];
}

Vector<PropertyInfo> Annotation::get_serialize_property_list() const {
	return get_serialize_property_list(this->get_class_name());
}

Dictionary Annotation::serialize_to_json() const {
	Dictionary ret;
	// Only serialize no default and valid properties.
	for (auto &&p : this->get_serialize_property_list()) {
		if (!(p.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		if (p.type == Variant::ARRAY && p.hint == PROPERTY_HINT_TYPE_STRING) {
			auto arr = this->get(p.name).operator Array();
			if (arr.size() > 0) {
#ifdef DEBUG_ENABLE
				if (arr.is_typed() && ClassDB::is_parent_class(arr.get_typed_class_name(), SNAME("Annotation"))) {
#endif
					Array ser_arr;
					for (auto i = 0; i < arr.size(); ++i) {
						auto anno = cast_to<Annotation>(arr[i]);
#ifdef DEBUG_ENABLE
						if (anno) {
#endif
							ser_arr.push_back(anno->serialize_to_json());
#ifdef DEBUG_ENABLE
						}
#endif
					}
					ret[p.name] = ser_arr;
#ifdef DEBUG_ENABLE
				}
#endif
			}
		} else if (p.type == Variant::OBJECT && p.hint == PROPERTY_HINT_RESOURCE_TYPE) {
#ifdef DEBUG_ENABLE
			if (ClassDB::is_parent_class(p.hint_string, SNAME("Annotation"))) {
#endif
				auto anno = cast_to<Annotation>(this->get(p.name));

				if (anno) {
					ret[p.name] = anno->serialize_to_json();
				}
#ifdef DEBUG_ENABLE
			}
#endif
		} else {
			if (this->get(p.name) != ClassDB::class_get_default_property_value(this->get_class_name(), p.name)) {
				ret[p.name] = this->get(p.name);
			}
		}
	}
	return ret;
}

bool Annotation::deserialize_from_json(const Dictionary &p_json) {
	for (auto &&p : this->get_serialize_property_list()) {
		if (!(p.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}
		if (!p_json.has(p.name)) {
			this->set(p.name, ClassDB::class_get_default_property_value(this->get_class_name(), p.name));
			continue;
		}

		// For SceneAnnotation's node_properties. This property will be deprecated.
		if (p.type == Variant::DICTIONARY && this->is_class("SceneAnnotation") && p.name == "node_properties") {
			auto sa = cast_to<SceneAnnotation>(this);

			auto dict = p_json[p.name];
			if (dict.get_type() != Variant::DICTIONARY) {
				return false;
			}

			Vector<Ref<NodeAnnotation>> node_annos;

			Dictionary node_properties_dict = dict.operator Dictionary();
			auto node_paths = node_properties_dict.keys();
			for (auto i = 0; i < node_paths.size(); ++i) {
				auto arr = node_properties_dict[node_paths[i]];
				if (arr.get_type() != Variant::ARRAY) {
					return false;
				}
				Array prop_anno_dicts = arr.operator Array();

				Vector<Ref<PropertyAnnotation>> prop_annos;
				for (auto j = 0; j < prop_anno_dicts.size(); ++j) {
					Ref<PropertyAnnotation> prop_anno;
					prop_anno.instantiate();
					if (!prop_anno->deserialize_from_json(prop_anno_dicts[j])) {
						return false;
					}
					prop_annos.push_back(prop_anno);
				}

				Ref<NodeAnnotation> node_anno;
				node_anno.instantiate();
				node_anno->set_node_path(NodePath(node_paths[i]));
				node_anno->set_properties(prop_annos);
				node_annos.push_back(node_anno);
			}

			sa->set_node_annotations(node_annos);
		}

		else if (p.type == Variant::ARRAY && p.hint == PROPERTY_HINT_TYPE_STRING) {
			auto arr = p_json.get(p.name, Array()).operator Array();
			if (arr.size() > 0) {
				auto splits = p.hint_string.split(":");
				if (splits.size() == 2) {
					auto anno_class = splits[splits.size() - 1];
#ifdef DEBUG_ENABLE
					if (ClassDB::is_parent_class(anno_class, SNAME("Annotation"))) {
#endif
						Array to_set;
						for (auto i = 0; i < arr.size(); ++i) {
							auto anno = cast_to<Annotation>(ClassDB::instantiate(anno_class));
							if (anno && anno->deserialize_from_json(arr[i].operator Dictionary())) {
								to_set.push_back(anno);
							} else {
								return false;
							}
						}
						this->set(p.name, Array(to_set, Variant::OBJECT, { anno_class }, Variant()));
#ifdef DEBUG_ENABLE
					} else {
						return false;
					}
#endif
				}
			}
		} else if (p.type == Variant::OBJECT && p.hint == PROPERTY_HINT_RESOURCE_TYPE) {
			auto anno_class = p.hint_string;
#ifdef DEBUG_ENABLE
			if (ClassDB::is_parent_class(anno_class, SNAME("Annotation"))) {
#endif
				auto anno = cast_to<Annotation>(ClassDB::instantiate({ anno_class }));
				if (anno && anno->deserialize_from_json(p_json.get(p.name, Dictionary()).operator Dictionary())) {
					this->set(p.name, anno);
				} else {
					return false;
				}
#ifdef DEBUG_ENABLE
			} else {
				return false;
			}
#endif
		} else {
			this->set(p.name, p_json[p.name]);
		}
	}
	return true;
}

// MemberAnnotation
void MemberAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_member_name"), &MemberAnnotation::get_member_name);
	ClassDB::bind_method(D_METHOD("set_member_name", "member_name"), &MemberAnnotation::set_member_name);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "member_name"), "set_member_name", "get_member_name");
}

// BaseResourceAnnotation
void BaseResourceAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_class_name_or_script_path"), &BaseResourceAnnotation::get_class_name_or_script_path_bind);
	ClassDB::bind_method(D_METHOD("set_class_name_or_script_path", "class_name_or_script_path"), &BaseResourceAnnotation::set_class_name_or_script_path);

	ClassDB::bind_method(D_METHOD("get_target_resource_path"), &BaseResourceAnnotation::get_target_resource_path);
	ClassDB::bind_method(D_METHOD("set_target_resource_path", "p_target_resource"), &BaseResourceAnnotation::set_target_resource_path);

	ClassDB::bind_method(D_METHOD("is_open_source"), &BaseResourceAnnotation::is_open_source);
	ClassDB::bind_method(D_METHOD("set_open_source", "open_source"), &BaseResourceAnnotation::set_open_source);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "class_name_or_script_path"), "set_class_name_or_script_path", "get_class_name_or_script_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "open_source"), "set_open_source", "is_open_source");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "target_resource_path", PROPERTY_HINT_FILE, "", PROPERTY_USAGE_STORAGE), "set_target_resource_path", "get_target_resource_path");
}
Ref<Resource> BaseResourceAnnotation::get_target_resource() {
	if (target_resource.is_null()) {
		if (ResourceLoader::exists(target_resource_path)) {
			Ref<Resource> res = ResourceLoader::load(target_resource_path);
			if (res.is_valid()) {
				target_resource = res;
			} else {
				target_resource = Variant();
			}
		}
	}
	return target_resource;
}
void BaseResourceAnnotation::set_target_resource(const Ref<Resource> &p_target_resource) {
	target_resource = p_target_resource;
	if (target_resource.is_valid()) {
		target_resource_path = target_resource->get_path();
	} else {
		target_resource_path = "";
	}
}

void BaseResourceAnnotation::set_target_resource_path(const String &p_path) {
	target_resource_path = p_path;
}
String BaseResourceAnnotation::get_target_resource_path() {
	if (target_resource.is_valid()) {
		target_resource_path = target_resource->get_path();
	}
	return target_resource_path;
}

String BaseResourceAnnotation::get_class_name_or_script_path_bind() {
	if (!class_name_or_script_path.is_empty()) {
		return class_name_or_script_path;
	}
	if (!target_resource_path.is_empty()) {
		if (target_resource.is_null()) {
			target_resource = ResourceLoader::load(target_resource_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE);
		}
		Ref<Script> s = target_resource->get_script();
		if (s.is_valid() && s.is_valid()) {
			auto path = s->get_path();
			List<StringName> global_classes;
			ScriptServer::get_global_class_list(&global_classes);
			for (auto global_class_name : global_classes) {
				if (ScriptServer::get_global_class_path(global_class_name) == path) {
					return global_class_name;
				}
			}
			return path;
		} else {
			return target_resource.ptr()->get_class_name();
		}
	}
	return "unknown";
}

// EnumElementAnnotation
void EnumElementAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_value"), &EnumElementAnnotation::get_value);
	ClassDB::bind_method(D_METHOD("set_value", "value "), &EnumElementAnnotation::set_value);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "value"), "set_value", "get_value");
}

// EnumAnnotation
void EnumAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_elements"), &EnumAnnotation::get_elements_bind);
	ClassDB::bind_method(D_METHOD("set_elements", "elements"), &EnumAnnotation::set_elements_bind);

	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("elements", "EnumElementAnnotation"), "set_elements", "get_elements");
}

TypedArray<EnumElementAnnotation> EnumAnnotation::get_elements_bind() const {
	TypedArray<EnumElementAnnotation> ret;
	for (auto &&e : elements) {
		ret.push_back(e);
	}
	return ret;
}

void EnumAnnotation::set_elements_bind(TypedArray<EnumElementAnnotation> p_elements) {
	elements.resize(p_elements.size());
	for (auto i = 0; i < p_elements.size(); ++i) {
		elements.set(i, p_elements[i]);
	}
}

// VariableAnnotation
Vector<String> VariableAnnotation::type_texts = {};
void VariableAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_type"), &VariableAnnotation::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "type"), &VariableAnnotation::set_type);

	ClassDB::bind_method(D_METHOD("get_class_name"), &VariableAnnotation::get_class_name);
	ClassDB::bind_method(D_METHOD("set_class_name", "class_name"), &VariableAnnotation::set_class_name);

	String types;
	for (auto i = 0; i < Variant::Type::VARIANT_MAX; ++i) {
		if (!types.is_empty()) {
			types += ",";
		}
		types += Variant::get_type_name(Variant::Type(i));
	}
	ADD_PROPERTY(PropertyInfo(Variant::INT, "type", PROPERTY_HINT_ENUM, types), "set_type", "get_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "class_name"), "set_class_name", "get_class_name");
}

// ParameterAnnotation
void ParameterAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_default_value_text"), &ParameterAnnotation::get_default_value_text);
	ClassDB::bind_method(D_METHOD("set_default_value_text", "default_value_text"), &ParameterAnnotation::set_default_value_text);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "default_value_text"), "set_default_value_text", "get_default_value_text");
}

// PropertyAnnotation
void PropertyAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_readonly"), &PropertyAnnotation::is_readonly);
	ClassDB::bind_method(D_METHOD("set_readonly", "readonly"), &PropertyAnnotation::set_readonly);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "readonly"), "set_readonly", "is_readonly");
}

// ConstantAnnotation
void ConstantAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_value_text"), &ConstantAnnotation::get_value_text);
	ClassDB::bind_method(D_METHOD("set_value_text", "value_text"), &ConstantAnnotation::set_value_text);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "value_text"), "set_value_text", "get_value_text");
}

// MethodAnnotation
void MethodAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_parameters"), &MethodAnnotation::get_parameters);
	ClassDB::bind_method(D_METHOD("set_parameters", "parameters"), &MethodAnnotation::set_parameters);

	ClassDB::bind_method(D_METHOD("get_return_value"), &MethodAnnotation::get_return_value);
	ClassDB::bind_method(D_METHOD("set_return_value", "return_value"), &MethodAnnotation::set_return_value);

	ClassDB::bind_method(D_METHOD("is_static"), &MethodAnnotation::is_static);
	ClassDB::bind_method(D_METHOD("set_static", "static"), &MethodAnnotation::set_static);

	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("parameters", "ParameterAnnotation"), "set_parameters", "get_parameters");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "return_value", PROPERTY_HINT_RESOURCE_TYPE, "VariableAnnotation"), "set_return_value", "get_return_value");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "static"), "set_static", "is_static");
}

// SignalAnnotation
void SignalAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_parameters"), &SignalAnnotation::get_parameters);
	ClassDB::bind_method(D_METHOD("set_parameters", "parameters"), &SignalAnnotation::set_parameters);

	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("parameters", "ParameterAnnotation"), "set_parameters", "get_parameters");
}

// NodeAnnotation
void NodeAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_node_path"), &NodeAnnotation::get_node_path);
	ClassDB::bind_method(D_METHOD("set_node_path", "node_path"), &NodeAnnotation::set_node_path);

	ClassDB::bind_method(D_METHOD("get_properties"), &NodeAnnotation::get_properties_bind);
	ClassDB::bind_method(D_METHOD("set_properties", "properties"), &NodeAnnotation::set_properties_bind);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "node_path"), "set_node_path", "get_node_path");
	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("properties", "PropertyAnnotation"), "set_properties", "get_properties");
}

TypedArray<PropertyAnnotation> NodeAnnotation::get_properties_bind() const {
	TypedArray<PropertyAnnotation> ret;
	for (auto &&prop : properties) {
		ret.push_back(prop);
	}
	return ret;
}

void NodeAnnotation::set_properties_bind(const TypedArray<PropertyAnnotation> &p_properties) {
	properties.resize(p_properties.size());
	for (auto i = 0; i < p_properties.size(); ++i) {
		Ref<PropertyAnnotation> prop_anno = p_properties[i];
		if (prop_anno.is_null()) {
			prop_anno.instantiate();
		}
		properties.set(i, prop_anno);
	}
}

// SceneAnnotation
void SceneAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_node_annotations"), &SceneAnnotation::get_node_annotations_bind);
	ClassDB::bind_method(D_METHOD("set_node_annotations", "node_annotations"), &SceneAnnotation::set_node_annotations_bind);

	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("node_annotations", "NodeAnnotation"), "set_node_annotations", "get_node_annotations");
}

TypedArray<NodeAnnotation> SceneAnnotation::get_node_annotations_bind() const {
	TypedArray<NodeAnnotation> ret;
	for (auto &&node_anno : node_annotations) {
		ret.push_back(node_anno);
	}
	return ret;
}

void SceneAnnotation::set_node_annotations_bind(const TypedArray<NodeAnnotation> &p_node_annotations) {
	node_annotations.clear();
	node_annotations.resize(p_node_annotations.size());
	for (auto i = 0; i < p_node_annotations.size(); ++i) {
		Ref<NodeAnnotation> node_anno = p_node_annotations[i];
		if (node_anno.is_null()) {
			node_anno.instantiate();
		}
		node_annotations.set(i, node_anno);
	}
}

// ResourceAnnotation
void ResourceAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_properties"), &ResourceAnnotation::get_properties);
	ClassDB::bind_method(D_METHOD("set_properties", "properties"), &ResourceAnnotation::set_properties);

	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("properties", "PropertyAnnotation"), "set_properties", "get_properties");
}

// ClassAnnotation
void ClassAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_base_class_name_or_script_path"), &ClassAnnotation::get_base_class_name_or_script_path);
	ClassDB::bind_method(D_METHOD("get_internal_classes"), &ClassAnnotation::get_internal_classes);
	ClassDB::bind_method(D_METHOD("get_enums"), &ClassAnnotation::get_enums);
	ClassDB::bind_method(D_METHOD("get_constants"), &ClassAnnotation::get_constants);
	ClassDB::bind_method(D_METHOD("get_properties"), &ClassAnnotation::get_properties);
	ClassDB::bind_method(D_METHOD("get_methods"), &ClassAnnotation::get_methods);
	ClassDB::bind_method(D_METHOD("get_signals"), &ClassAnnotation::get_signals);
	ClassDB::bind_method(D_METHOD("is_tool"), &ClassAnnotation::is_tool);

	ClassDB::bind_method(D_METHOD("set_base_class_name_or_script_path", "base"), &ClassAnnotation::set_base_class_name_or_script_path);
	ClassDB::bind_method(D_METHOD("set_internal_classes", "internal_classes"), &ClassAnnotation::set_internal_classes);
	ClassDB::bind_method(D_METHOD("set_enums", "enums"), &ClassAnnotation::set_enums);
	ClassDB::bind_method(D_METHOD("set_constants", "constants"), &ClassAnnotation::set_constants);
	ClassDB::bind_method(D_METHOD("set_properties", "properties"), &ClassAnnotation::set_properties);
	ClassDB::bind_method(D_METHOD("set_methods", "methods"), &ClassAnnotation::set_methods);
	ClassDB::bind_method(D_METHOD("set_signals", "signals"), &ClassAnnotation::set_signals);
	ClassDB::bind_method(D_METHOD("set_tool", "tool"), &ClassAnnotation::set_tool);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_class_name_or_script_path"), "set_base_class_name_or_script_path", "get_base_class_name_or_script_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "tool"), "set_tool", "is_tool");
	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("internal_classes", "ClassAnnotation"), "set_internal_classes", "get_internal_classes");
	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("enums", "EnumAnnotation"), "set_enums", "get_enums");
	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("constants", "ConstantAnnotation"), "set_constants", "get_constants");
	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("properties", "PropertyAnnotation"), "set_properties", "get_properties");
	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("methods", "MethodAnnotation"), "set_methods", "get_methods");
	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("signals", "SignalAnnotation"), "set_signals", "get_signals");

	ClassDB::bind_static_method("ClassAnnotation", D_METHOD("get_script_annotation", "script"), &ScriptAnnotationParserManager::get_script_annotation);
}

bool ClassAnnotation::is_needed() const {
	return !get_base_class_name_or_script_path().is_empty() && (!get_name().is_empty() || !get_comment().is_empty() || !get_internal_classes().is_empty() || !get_enums().is_empty() || !get_constants().is_empty() || !get_properties().is_empty() || !get_methods().is_empty() || !get_signals().is_empty());
}

Vector<Ref<Annotation>> ClassAnnotation::get_member_annotations() const {
	Vector<Ref<Annotation>> ret;
	ret.resize(internal_classes.size() + properties.size() + methods.size() + signals.size() + constants.size() + enums.size());
	int start_idx = 0;
	for (auto i = 0; i < internal_classes.size(); ++i) {
		ret.set(start_idx + i, internal_classes[i]);
	}
	start_idx += internal_classes.size();
	for (auto i = 0; i < properties.size(); ++i) {
		ret.set(start_idx + i, properties[i]);
	}
	start_idx += properties.size();
	for (auto i = 0; i < methods.size(); ++i) {
		ret.set(start_idx + i, methods[i]);
	}
	start_idx += methods.size();
	for (auto i = 0; i < signals.size(); ++i) {
		ret.set(start_idx + i, signals[i]);
	}
	start_idx += signals.size();
	for (auto i = 0; i < constants.size(); ++i) {
		ret.set(start_idx + i, constants[i]);
	}
	start_idx += constants.size();
	for (auto i = 0; i < enums.size(); ++i) {
		ret.set(start_idx + i, enums[i]);
	}
	return ret;
}

// ModuleAnnotation
void ModuleAnnotation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_module_name"), &ModuleAnnotation::get_module_name);
	ClassDB::bind_method(D_METHOD("set_module_name", "module_name"), &ModuleAnnotation::set_module_name);

	ClassDB::bind_method(D_METHOD("get_resources"), &ModuleAnnotation::get_resources);
	ClassDB::bind_method(D_METHOD("set_resources", "resources"), &ModuleAnnotation::set_resources);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "module_name"), "set_module_name", "get_module_name");
	ADD_PROPERTY(TYPED_ARR_RES_PROP_INFO("resources", "Resource"), "set_resources", "get_resources");

	ClassDB::bind_method(D_METHOD("get_resource_annotations"), &ModuleAnnotation::get_resource_annotations_bind);
	ClassDB::bind_static_method("ModuleAnnotation", D_METHOD("generate_module_annotations", "file_provider"), &ModuleAnnotation::generate_module_annotations_bind);
	ClassDB::bind_static_method("ModuleAnnotation", D_METHOD("get_main_module_info", "file_provider"), &ModuleAnnotation::get_main_module_info);
}

bool ModuleAnnotation::_set(const StringName &p_name, const Variant &p_property) {
	if (p_name == SNAME("resource_paths")) {
		resource_paths = p_property;
		return true;
	}
	return false;
};

bool ModuleAnnotation::_get(const StringName &p_name, Variant &r_property) const {
	if (p_name == SNAME("resource_paths")) {
		r_property = resource_paths;
		return true;
	}
	return false;
}

void ModuleAnnotation::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::PACKED_STRING_ARRAY, "resource_paths", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));
}

HashSet<String> ModuleAnnotation::get_open_source_resources() {
	HashSet<String> ret;
	for (auto &&E : resource_annotations) {
		if (E->is_open_source()) {
			ret.insert(E->get_target_resource_path());
		}
	}
	return ret;
}

String ModuleAnnotation::get_module_name() const {
	return module_name;
}
void ModuleAnnotation::set_module_name(const String &p_module_name) {
	module_name = p_module_name;
}

TypedArray<Resource> ModuleAnnotation::get_resources() const {
	TypedArray<Resource> ret;
	for (auto &&res : resources) {
		ret.push_back(res);
	}
	return ret;
}

void ModuleAnnotation::set_resources(const TypedArray<Resource> &p_resource) {
	resources.clear();
	for (auto i = 0; i < p_resource.size(); ++i) {
		Ref<Resource> res = p_resource[i];
		if (res.is_valid() && res->is_class("Annotation")) {
			WARN_PRINT_ONCE("Module resource should not be a Annotation.");
			continue;
		}
		if (res.is_valid() && res->get_path().is_empty()) {
			continue;
		}
		resources.push_back(res);
	}
}

Dictionary ModuleAnnotation::serialize_to_json() const {
	Dictionary ret;
	ret["module_name"] = module_name;

	auto resource_arr = Array();
	for (auto &&res : resources) {
		resource_arr.push_back(res->get_path());
	}
	ret["resources"] = resource_arr;

	return ret;
}

bool ModuleAnnotation::deserialize_from_json(const Dictionary &p_json) {
	module_name = p_json.get("module_name", "");

	resources.clear();
	if (p_json.has("resources")) {
		auto arr = p_json.get("resources", {});
		if (arr.get_type() != Variant::ARRAY) {
			return false;
		}
		auto path_arr = arr.operator Array();
		for (auto i = 0; i < path_arr.size(); ++i) {
			if (path_arr[i].get_type() != Variant::STRING) {
				return false;
			}
			auto path = path_arr[i].operator String();
			if (!ResourceLoader::exists(path, "Resource")) {
				WARN_PRINT(vformat("Resource \"%s\" not found.", path));
				continue;
			}
			Ref<ModuleAnnotation> res = ResourceLoader::load(path);
			resources.push_back(res);
		}
	}

	return true;
}

String ModuleAnnotation::get_modules_description_json(const List<Ref<ModuleAnnotation>> &p_modules, const Ref<ModuleAnnotation> &p_main_module) {
	Dictionary json;
	Array modules_path_arr;
	for (auto &&module : p_modules) {
		modules_path_arr.push_back(module->get_path());
	}
	if (p_main_module.is_valid()) {
		Dictionary main_module_info;
		json["main"] = main_module_info;
		main_module_info["name"] = p_main_module->get_module_name();
		main_module_info["path"] = p_main_module->get_path();
		main_module_info["comment"] = p_main_module->get_comment();
		main_module_info["resource_paths"] = p_main_module->resource_paths;
	}
	json["modules"] = modules_path_arr;
	return JSON::stringify(json, "\t");
}

Vector<Ref<ModuleAnnotation>> ModuleAnnotation::generate_module_annotations(Ref<FileProvider> p_provider) {
	return _generate_module_annotations<Vector<Ref<ModuleAnnotation>>>(p_provider);
}

TypedArray<ModuleAnnotation> ModuleAnnotation::generate_module_annotations_bind(Ref<FileProvider> p_provider) {
	return _generate_module_annotations<TypedArray<ModuleAnnotation>>(p_provider);
}

Dictionary ModuleAnnotation::get_main_module_info(Ref<FileProvider> p_provider) {
	Error error;
	auto fa = p_provider->open(AnnotationExportPlugin::module_annotation_json_path, FileAccess::READ, &error);

	ERR_FAIL_COND_V(fa.is_null(), {});

	// Validate and cast type.
	String module_json = fa->get_as_text();
	Variant modules_json = JSON::parse_string(module_json);
	ERR_FAIL_COND_V_MSG(modules_json.get_type() != Variant::DICTIONARY, {}, "Parsing 'p_modules_annotation_json_text' failed.");
	auto modules_dict = modules_json.operator Dictionary();
	ERR_FAIL_COND_V_MSG(!modules_dict.has("main") || modules_dict["main"].get_type() != Variant::DICTIONARY, {}, "Unrecognized 'p_modules_annotation_json_text'.");
	return modules_dict["main"];
}

List<Ref<BaseResourceAnnotation>> ModuleAnnotation::get_resource_annotations() const {
	return resource_annotations;
}

TypedArray<BaseResourceAnnotation> ModuleAnnotation::get_resource_annotations_bind() const {
	TypedArray<BaseResourceAnnotation> ret;
	for (auto anno : resource_annotations) {
		ret.push_back(anno);
	}
	return ret;
}

void ModuleAnnotation::copy_for_export(const Ref<ModuleAnnotation> &p_origin) {
	set_comment(p_origin->get_comment());
	set_module_name(p_origin->get_module_name());

	resource_paths.resize(p_origin->resources.size());
	int added = 0;
	for (auto i = 0; i < p_origin->resources.size(); ++i) {
		auto resource_path = p_origin->resources[i]->get_path();
		ERR_CONTINUE_MSG(resource_path.is_empty(), vformat("Module resource '%s' has not path and will be skipped.", Variant(p_origin->resources[i]).stringify()));
		resource_paths.set(added, resource_path);
		++added;
	}
	if (added < p_origin->resources.size()) {
		resource_paths.resize(added);
	}
}