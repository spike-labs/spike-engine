/**
 * annotation_resource.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "annotation/annotation_export_plugin.h"
#include "annotation/annotation_plugin.h"
#include "core/io/json.h"
#include "core/io/resource.h"
#include "core/object/script_language.h"
#include "core/variant/typed_array.h"
#include "filesystem_server/file_provider.h"

#pragma region Base
class Annotation : public Resource {
	GDCLASS(Annotation, Resource)
protected:
	static void _bind_methods();

	static void collect_serialize_properties_recurrently(const StringName &p_class, Vector<PropertyInfo> &r_props);
	static Vector<PropertyInfo> get_serialize_property_list(const StringName &p_class);

	Vector<PropertyInfo> get_serialize_property_list() const;

public:
	virtual String get_display_name() const { return {}; }
	virtual Vector<Ref<Annotation>> get_member_annotations() const { return {}; }

	String get_comment() const { return comment; }
	void set_comment(const String &p_comment) { comment = p_comment; }

	virtual Dictionary serialize_to_json() const;
	virtual bool deserialize_from_json(const Dictionary &p_json);

private:
	String comment;

	static String annotations_to_json_bind(const TypedArray<class BaseResourceAnnotation> &p_annotations);
	String get_display_name_bind() const { return this->get_display_name(); }
	TypedArray<Annotation> get_member_annotations_bind() const {
		TypedArray<Annotation> ret;
		for (auto &&e : this->get_member_annotations()) {
			ret.push_back(e);
		}
		return ret;
	}
};

class MemberAnnotation : public Annotation {
	GDCLASS(MemberAnnotation, Annotation)
protected:
	static void _bind_methods();

public:
	virtual Vector<Ref<Annotation>> get_member_annotations() const override { return {}; }
	virtual String get_display_name() const override { return get_member_name(); }

	StringName get_member_name() const { return member_name; }
	void set_member_name(const StringName &p_member_name) { return member_name = p_member_name; }

private:
	StringName member_name;
};

class BaseResourceAnnotation : public Annotation {
	GDCLASS(BaseResourceAnnotation, Annotation)
protected:
	static void _bind_methods();

public:
	virtual String get_display_name() const override { return target_resource_path; }

	String get_class_name_or_script_path() const { return class_name_or_script_path; }
	void set_class_name_or_script_path(const String &p_class_name_or_script_path) { class_name_or_script_path = p_class_name_or_script_path; }

	Ref<Resource> get_target_resource();
	void set_target_resource(const Ref<Resource> &p_target_resource);

	void set_target_resource_path(const String &p_path);
	String get_target_resource_path();

	bool is_open_source() const { return open_source; }
	void set_open_source(bool p_open_source) { open_source = p_open_source; }

private:
	String class_name_or_script_path;
	String target_resource_path;
	bool open_source = false;
	// Not storage member.
	Ref<Resource> target_resource;

	String get_class_name_or_script_path_bind();
};
#pragma endregion

class EnumElementAnnotation : public MemberAnnotation {
	GDCLASS(EnumElementAnnotation, MemberAnnotation)
protected:
	static void _bind_methods();

public:
	virtual String get_display_name() const override { return String(get_member_name()) + " = " + itos(get_value()); }

	int32_t get_value() const { return value; }
	void set_value(uint32_t p_value) { value = p_value; }

private:
	int value = 0;
};

class EnumAnnotation : public MemberAnnotation {
	GDCLASS(EnumAnnotation, MemberAnnotation)
protected:
	static void _bind_methods();

public:
	virtual Vector<Ref<Annotation>> get_member_annotations() const override {
		Vector<Ref<Annotation>> ret;
		ret.resize(elements.size());
		for (auto i = 0; i < elements.size(); ++i) {
			ret.set(i, elements[i]);
		}
		return ret;
	}

	Vector<Ref<EnumElementAnnotation>> get_elements() const { return elements; }
	void set_elements(Vector<Ref<EnumElementAnnotation>> p_elements) { elements = p_elements; }

	void push_element(Ref<EnumElementAnnotation> p_enum_element_annotation) { elements.push_back(p_enum_element_annotation); }

private:
	Vector<Ref<EnumElementAnnotation>> elements;

	TypedArray<EnumElementAnnotation> get_elements_bind() const;
	void set_elements_bind(TypedArray<EnumElementAnnotation> p_elements);
};

class VariableAnnotation : public MemberAnnotation {
	GDCLASS(VariableAnnotation, MemberAnnotation)
protected:
	static Vector<String> type_texts;
	static void _bind_methods();

public:
	// virtual String get_display_name() const override {
	// 	return String(get_member_name()) + ": " + (get_class_name().is_empty() ? type_texts[type] : get_class_name());
	// }

	Variant::Type get_type() const { return type; }
	void set_type(Variant::Type p_type) { type = p_type; }

	String get_class_name() const { return class_name; }
	void set_class_name(const String &p_class_name) { class_name = p_class_name; }

private:
	Variant::Type type = Variant::NIL;
	String class_name;
};

class ParameterAnnotation : public VariableAnnotation {
	GDCLASS(ParameterAnnotation, VariableAnnotation)
protected:
	static void _bind_methods();

public:
	// virtual String get_display_name() const override { return VariableAnnotation::get_display_name() + (default_value_text.is_empty() ? "" : (" " + default_value_text)); }

	String get_default_value_text() const { return default_value_text; }
	void set_default_value_text(const String &p_default_value_text) { default_value_text = p_default_value_text; }

private:
	String default_value_text;
};

class PropertyAnnotation : public ParameterAnnotation {
	GDCLASS(PropertyAnnotation, ParameterAnnotation)
protected:
	static void _bind_methods();

public:
	// virtual String get_display_name() const override { return (readonly ? "readonly " : "") + ParameterAnnotation::get_display_name(); }

	bool is_readonly() const { return readonly; }
	void set_readonly(bool p_readonly) { readonly = p_readonly; }

private:
	bool readonly = false;
};

class ConstantAnnotation : public VariableAnnotation {
	GDCLASS(ConstantAnnotation, VariableAnnotation)
protected:
	static void _bind_methods();

public:
	// virtual String get_display_name() const override { return VariableAnnotation::get_display_name() + (value_text.is_empty() ? "" : value_text); }

	String get_value_text() const { return value_text; }
	void set_value_text(const String &p_value_text) { value_text = p_value_text; }

private:
	String value_text;
};

class MethodAnnotation : public MemberAnnotation {
	GDCLASS(MethodAnnotation, MemberAnnotation)
protected:
	static void _bind_methods();

public:
	// virtual String get_display_name() const override { return (static_ ? "static " : "") + MemberAnnotation::get_display_name(); }
	virtual Vector<Ref<Annotation>> get_member_annotations() const override {
		Vector<Ref<Annotation>> ret;
		ret.resize(parameters.size() + 1);
		for (auto i = 0; i < parameters.size(); ++i) {
			ret.set(i, parameters[i]);
		}
		ret.set(ret.size() - 1, return_value);
		return ret;
	}

	TypedArray<ParameterAnnotation> get_parameters() const { return parameters; }
	void set_parameters(TypedArray<ParameterAnnotation> p_parameters) { parameters = p_parameters; }

	Ref<VariableAnnotation> get_return_value() const { return return_value; }
	void set_return_value(Ref<VariableAnnotation> p_return_value) {
		return_value = p_return_value;
		if (return_value.is_valid()) {
			return_value->set_member_name("return");
		}
	}

	bool is_static() const { return static_; }
	void set_static(bool p_static) { static_ = p_static; }

	void push_parameter(Ref<VariableAnnotation> p_param) { parameters.push_back(p_param); }
	// void pop_parameter(Ref<VariableAnnotation> p_param) { parameters.resize(parameters.size() - 1); }
	Ref<ParameterAnnotation> get_parameter(int p_idx) { return parameters[p_idx]; }
	int get_parameter_count() { return parameters.size(); }

private:
	TypedArray<ParameterAnnotation> parameters;
	Ref<VariableAnnotation> return_value;
	bool static_ = false;
};

class SignalAnnotation : public MemberAnnotation {
	GDCLASS(SignalAnnotation, MemberAnnotation)
protected:
	static void _bind_methods();

public:
	virtual Vector<Ref<Annotation>> get_member_annotations() const override {
		Vector<Ref<Annotation>> ret;
		ret.resize(parameters.size());
		for (auto i = 0; i < parameters.size(); ++i) {
			ret.set(i, parameters[i]);
		}
		return ret;
	}

	TypedArray<ParameterAnnotation> get_parameters() const { return parameters; }
	void set_parameters(TypedArray<ParameterAnnotation> p_parameters) { parameters = p_parameters; }

	void push_parameter(Ref<VariableAnnotation> p_param) { parameters.push_back(p_param); }
	// void pop_parameter(Ref<VariableAnnotation> p_param) { parameters.resize(parameters.size() - 1); }
	Ref<ParameterAnnotation> get_parameter(int p_idx) { return parameters[p_idx]; }
	int get_parameter_count() { return parameters.size(); }

private:
	TypedArray<ParameterAnnotation> parameters;
};

class NodeAnnotation : public Annotation {
	GDCLASS(NodeAnnotation, Annotation)
protected:
	static void _bind_methods();

	virtual Dictionary serialize_to_json() const override {
		Dictionary ret;
		ret["comment"] = get_comment();
		ret["node_path"] = get_node_path();
		Array props;
		for (auto &&p : properties) {
			props.push_back(p->serialize_to_json());
		}
		ret["properties"] = props;
		return ret;
	}
	virtual bool deserialize_from_json(const Dictionary &p_json) override {
		set_comment(p_json.get("comment", ""));
		set_node_path({ p_json.get("node_path", "") });
		auto arr = p_json.get("properties", Array());
		ERR_FAIL_COND_V(arr.get_type() != Variant::ARRAY, false);
		auto props = arr.operator Array();
		properties.resize(props.size());
		for (auto i = 0; i < props.size(); ++i) {
			ERR_FAIL_COND_V(props[i].get_type() != Variant::DICTIONARY, false);
			Ref<PropertyAnnotation> pa;
			pa.instantiate();
			ERR_FAIL_COND_V(pa->deserialize_from_json(props[i]) == false, false);
			properties.set(i, pa);
		}
		return true;
	}

public:
	virtual String get_display_name() const override { return { node_path }; }
	virtual Vector<Ref<Annotation>> get_member_annotations() const override {
		Vector<Ref<Annotation>> ret;
		ret.resize(properties.size());
		for (auto i = 0; i < properties.size(); ++i) {
			ret.set(i, properties[i]);
		}
		return ret;
	}

	void set_node_path(const NodePath &p_node_path) { node_path = p_node_path; }
	NodePath get_node_path() const { return node_path; }

	void set_properties(const Vector<Ref<PropertyAnnotation>> &p_properties) { properties = p_properties; }
	Vector<Ref<PropertyAnnotation>> get_properties() const { return properties; }

private:
	NodePath node_path;
	Vector<Ref<PropertyAnnotation>> properties;

	TypedArray<PropertyAnnotation> get_properties_bind() const;
	void set_properties_bind(const TypedArray<PropertyAnnotation> &p_properties);
};

class SceneAnnotation : public BaseResourceAnnotation {
	GDCLASS(SceneAnnotation, BaseResourceAnnotation)
protected:
	static void _bind_methods();

public:
	virtual Vector<Ref<Annotation>> get_member_annotations() const override {
		Vector<Ref<Annotation>> ret;
		ret.resize(node_annotations.size());
		for (auto i = 0; i < node_annotations.size(); ++i) {
			ret.set(i, node_annotations[i]);
		}
		return ret;
	}

	Vector<Ref<NodeAnnotation>> get_node_annotations() const { return node_annotations; }
	void set_node_annotations(const Vector<Ref<NodeAnnotation>> &p_node_annotations) { node_annotations = p_node_annotations; }

private:
	Vector<Ref<NodeAnnotation>> node_annotations;

	friend class Annotation;

	TypedArray<NodeAnnotation> get_node_annotations_bind() const;
	void set_node_annotations_bind(const TypedArray<NodeAnnotation> &p_node_annotations);
};

class ResourceAnnotation : public BaseResourceAnnotation {
	GDCLASS(ResourceAnnotation, BaseResourceAnnotation)
protected:
	static void _bind_methods();

public:
	virtual Vector<Ref<Annotation>> get_member_annotations() const override {
		Vector<Ref<Annotation>> ret;
		ret.resize(properties.size());
		for (auto i = 0; i < properties.size(); ++i) {
			ret.set(i, properties[i]);
		}
		return ret;
	}

	TypedArray<PropertyAnnotation> get_properties() const { return properties; }
	void set_properties(TypedArray<PropertyAnnotation> p_properties) { properties = p_properties; }

private:
	TypedArray<PropertyAnnotation> properties;
};

class ClassAnnotation : public BaseResourceAnnotation {
	GDCLASS(ClassAnnotation, BaseResourceAnnotation)
protected:
	static void _bind_methods();

public:
	virtual Vector<Ref<Annotation>> get_member_annotations() const override;

	String get_base_class_name_or_script_path() const { return base_class_name_or_script_path; }
	TypedArray<ClassAnnotation> get_internal_classes() const { return internal_classes; }
	TypedArray<EnumAnnotation> get_enums() const { return enums; }
	TypedArray<ConstantAnnotation> get_constants() const { return constants; }
	TypedArray<PropertyAnnotation> get_properties() const { return properties; }
	TypedArray<MethodAnnotation> get_methods() const { return methods; }
	TypedArray<SignalAnnotation> get_signals() const { return signals; }
	bool is_tool() const { return tool; }

	void set_base_class_name_or_script_path(const String &p_base) { base_class_name_or_script_path = p_base; }
	void set_internal_classes(TypedArray<ClassAnnotation> p_internal_classes) { internal_classes = p_internal_classes; }
	void set_enums(TypedArray<EnumAnnotation> p_enums) { enums = p_enums; }
	void set_constants(TypedArray<ConstantAnnotation> p_constants) { constants = p_constants; }
	void set_properties(TypedArray<PropertyAnnotation> p_properties) { properties = p_properties; }
	void set_methods(TypedArray<MethodAnnotation> p_methods) { methods = p_methods; }
	void set_signals(TypedArray<SignalAnnotation> p_signals) { signals = p_signals; }
	void set_tool(bool p_tool) { tool = p_tool; }

	void add_internal_class(Ref<ClassAnnotation> p_internal_class) { internal_classes.push_back(p_internal_class); }
	void add_enum(Ref<EnumAnnotation> p_enum) { enums.push_back(p_enum); }
	void add_const(Ref<ConstantAnnotation> p_const) { constants.push_back(p_const); }
	void add_property(Ref<PropertyAnnotation> p_property) { properties.push_back(p_property); }
	void add_method(Ref<MethodAnnotation> p_method) { methods.push_back(p_method); }
	void add_signal(Ref<SignalAnnotation> p_signal) { signals.push_back(p_signal); }

	bool is_needed() const;

private:
	String base_class_name_or_script_path;
	TypedArray<ClassAnnotation> internal_classes;
	TypedArray<EnumAnnotation> enums;
	TypedArray<ConstantAnnotation> constants;
	TypedArray<PropertyAnnotation> properties;
	TypedArray<MethodAnnotation> methods;
	TypedArray<SignalAnnotation> signals;
	bool tool = false;
};

class ModuleAnnotation : public Annotation {
	GDCLASS(ModuleAnnotation, Annotation)
protected:
	static void _bind_methods();

public:
	virtual String get_display_name() const override { return get_module_name(); }
	virtual Vector<Ref<Annotation>> get_member_annotations() const override {
		Vector<Ref<Annotation>> ret;
		ret.resize(resource_annotations.size());
		for (auto i = 0; i < resource_annotations.size(); ++i) {
			ret.set(i, resource_annotations[i]);
		}
		return ret;
	}

	String get_module_name() const;
	void set_module_name(const String &p_module_name);

	TypedArray<Resource> get_resources() const;
	void set_resources(const TypedArray<Resource> &p_resources);

	Dictionary serialize_to_json() const override;
	bool deserialize_from_json(const Dictionary &p_json) override;

	List<Ref<BaseResourceAnnotation>> get_resource_annotations() const;

	static String get_modules_description_json(const List<Ref<ModuleAnnotation>> &p_modules, const Ref<ModuleAnnotation> &p_main_module = nullptr);
	static Vector<Ref<ModuleAnnotation>> generate_module_annotations(Ref<FileProvider> p_provider);
	// "name":String, "comment":String, "path":String, "resource_paths": Array[String]
	static Dictionary get_main_module_info(Ref<FileProvider> p_provider);

	bool _set(const StringName &p_name, const Variant &p_property);
	bool _get(const StringName &p_name, Variant &r_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	// Only avalivale which generated by `ModuleAnnotation::generate_module_annotations`
	HashSet<String> get_open_source_resources();

private:
	String module_name;
	// Unstorage after exported.
	List<Ref<Resource>> resources;
	// For export.
	PackedStringArray resource_paths;
	// Only avalivale which generated by `ModuleAnnotation::generate_module_annotations`
	List<Ref<BaseResourceAnnotation>> resource_annotations;

	template <typename TArr>
	Error _find_anno(const String &p_res_path, TArr &p_arr) {
		Ref<BaseResourceAnnotation> anno;
		for (auto i = 0; i < p_arr.size(); ++i) {
			anno = p_arr[i];
			if (anno.is_null()) {
				return ERR_PARSE_ERROR;
			}
			if (anno->get_target_resource_path() == p_res_path) {
				break;
			}
			anno = Variant();
		}
		if (anno.is_valid()) {
			resource_annotations.push_back(anno);
			return OK;
		}
		return ERR_DOES_NOT_EXIST;
	}

	template <typename TArr>
	static TArr _generate_module_annotations(Ref<FileProvider> p_provider) {
		Error error;
		auto fa = p_provider->open(AnnotationExportPlugin::module_annotation_json_path, FileAccess::READ, &error);

		if (fa.is_null()) {
			return TArr();
		}

		// Validate and cast type.
		String module_json = fa->get_as_text();
		Variant modules_json = JSON::parse_string(module_json);
		ERR_FAIL_COND_V_MSG(modules_json.get_type() != Variant::DICTIONARY, {}, "Parsing 'p_modules_annotation_json_text' failed.");
		auto modules_dict = modules_json.operator Dictionary();
		ERR_FAIL_COND_V_MSG(!modules_dict.has("modules"), {}, "Unrecognized 'p_modules_annotation_json_text'.");
		auto modules_arr = modules_dict["modules"];
		ERR_FAIL_COND_V_MSG(modules_arr.get_type() != Variant::ARRAY, {}, "Unrecognized 'p_modules_annotation_json_text'.");
		auto modules = modules_arr.operator Array();

		// Try to Collect resources annotations.
		Dictionary resources_json;
		fa.unref();
		fa = p_provider->open(AnnotationExportPlugin::annotation_json_path, FileAccess::READ, &error);
		if (fa.is_valid()) {
			String anno_json = fa->get_as_text();
			resources_json = AnnotationPlugin::load_annotations_json(anno_json);
		}

		TypedArray<ClassAnnotation> class_annos = resources_json.get("ClassAnnotation", TypedArray<ClassAnnotation>());
		TypedArray<SceneAnnotation> scene_annos = resources_json.get("SceneAnnotation", TypedArray<SceneAnnotation>());
		TypedArray<ResourceAnnotation> res_annos = resources_json.get("ResourceAnnotation", TypedArray<ResourceAnnotation>());

		//
		TArr ret;
		for (auto i = 0; i < modules.size(); ++i) {
			auto mp = modules[i];
			Ref<ModuleAnnotation> ma = p_provider->load(mp);
			ERR_FAIL_COND_V_MSG(ma.is_null(), {}, "Unrecognized 'p_modules_annotation_json_text'.");
			if (ma->resource_paths.size() > 0) {
				for (auto &&res_path : ma->resource_paths) {
					Error err = OK;

					err = ma->_find_anno(res_path, class_annos);
					ERR_FAIL_COND_V_MSG(err == ERR_PARSE_ERROR, {}, "generate_modules_annotation_from_json() failed, error code: " + itos(err));
					if (err == OK) {
						continue;
					}
					err = ma->_find_anno(res_path, scene_annos);
					ERR_FAIL_COND_V_MSG(err == ERR_PARSE_ERROR, {}, "generate_modules_annotation_from_json() failed, error code: " + itos(err));
					if (err == OK) {
						continue;
					}
					err = ma->_find_anno(res_path, res_annos);
					ERR_FAIL_COND_V_MSG(err == ERR_PARSE_ERROR, {}, "generate_modules_annotation_from_json() failed, error code: " + itos(err));
					if (err == OK) {
						continue;
					}
					WARN_PRINT("Missing resource annotation: " + res_path);
				}
			} else {
				for (auto &&res : ma->resources) {
					Error err = OK;
					if (res->is_class("Script")) {
						err = ma->_find_anno(res->get_path(), class_annos);
					} else if (res->is_class("PackedScene")) {
						err = ma->_find_anno(res->get_path(), scene_annos);
					} else if (res->is_class("Resource")) {
						err = ma->_find_anno(res->get_path(), res_annos);
					} else {
						err = ERR_UNAVAILABLE;
					}

					if (err == ERR_DOES_NOT_EXIST) {
						WARN_PRINT("Missing resource annotation: " + res->get_path());
					} else {
						ERR_FAIL_COND_V_MSG(err != OK, {}, "generate_modules_annotation_from_json() failed, error code: " + itos(err));
					}
				}
			}
			ret.push_back(ma);
		}

		return ret;
	}

	void copy_for_export(const Ref<ModuleAnnotation> &p_origin);
	friend class AnnotationExportPlugin;

	TypedArray<BaseResourceAnnotation> get_resource_annotations_bind() const;
	static TypedArray<ModuleAnnotation> generate_module_annotations_bind(Ref<FileProvider> p_provider);
};