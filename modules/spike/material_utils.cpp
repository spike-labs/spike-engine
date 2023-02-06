/**
 * material_utils.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "material_utils.h"

typedef const char* Alias;

static Alias albedo_alias[] = { "albedo_color", nullptr };
static Alias texture_albedo_alias[] = { "albedo_texture", nullptr };

static Alias texture_metallic_alias[] = { "metallic_texture", nullptr };

static Alias texture_roughness_alias[] = { "roughness_texture", nullptr };

static Alias specular_alias[] = { "metallic_specular", nullptr };

static Alias texture_emission_alias[] = { "emission_texture", nullptr };
static Alias emission_energy_alias[] = { "emission_energy_multiplier", nullptr };

static Alias texture_normal_alias[] = { "normal_texture", nullptr };

static Alias texture_rim_alias[] = { "rim_texture", nullptr };

static Alias texture_ambient_occlusion_alias[] = { "ao_texture", nullptr };

static Alias texture_heightmap_alias[] = { "heightmap_texture", nullptr };

static Map<StringName, Alias *> *parameter_alias_map = nullptr;

static Alias *get_paramater_alias(const StringName &param_name) {
    if (parameter_alias_map == nullptr) {
        parameter_alias_map = new Map<StringName, Alias *>();
        parameter_alias_map->insert("albedo", albedo_alias);
        parameter_alias_map->insert("texture_albedo", texture_albedo_alias);
        parameter_alias_map->insert("texture_metallic", texture_metallic_alias);
        parameter_alias_map->insert("texture_roughness", texture_roughness_alias);
		parameter_alias_map->insert("specular", specular_alias);
        parameter_alias_map->insert("texture_emission", texture_emission_alias);
        parameter_alias_map->insert("emission_energy", emission_energy_alias);
        parameter_alias_map->insert("texture_normal", texture_normal_alias);
        parameter_alias_map->insert("texture_rim", texture_rim_alias);
        parameter_alias_map->insert("texture_ambient_occlusion", texture_ambient_occlusion_alias);
        parameter_alias_map->insert("texture_heightmap", texture_heightmap_alias);
    }
    auto alias = parameter_alias_map->getptr(param_name);
    return alias ? *alias : nullptr;
}

static Variant::Type try_remap_parameter_name(const Map<StringName, Variant::Type> &dst_param_types, StringName &param_name) {
	auto ptype = dst_param_types.getptr(param_name);
	Variant::Type type = Variant::NIL;
	if (ptype == nullptr) {
		Alias *alias = get_paramater_alias(param_name);
		if (alias) {
			while (*alias) {
				ptype = dst_param_types.getptr(*alias);
				if (ptype) {
					type = *ptype;
					param_name = *alias;
					break;
				}
				alias++;
			}
		}
	} else {
		type = *ptype;
	}
	return type;
}

void MaterialUtils::convert_shader_parameters(const Ref<Material> src, Ref<Material> dst) {
	Map<StringName, Variant> parameters;
	RID src_id = src->get_rid();

	List<PropertyInfo> param_list;
	RS::get_singleton()->get_shader_parameter_list(src->get_shader_rid(), &param_list);
	for (auto &param : param_list) {
		Variant value = RS::get_singleton()->material_get_param(src_id, param.name);
		if (value.get_type() != Variant::NIL) {
			parameters.insert(param.name, value);
		}
	}

	Map<StringName, Variant::Type> dst_param_types;
	List<PropertyInfo> dst_param_list;

	if (dst->is_class_ptr(ShaderMaterial::get_class_ptr_static())) {
		RS::get_singleton()->get_shader_parameter_list(dst->get_shader_rid(), &dst_param_list);
	} else {
		dst->get_property_list(&dst_param_list);
	}
	for (auto &param : dst_param_list) {
		dst_param_types.insert(param.name, param.type);
	}

	for (auto &kv : parameters) {
		StringName param_name = kv.key;
		auto type = try_remap_parameter_name(dst_param_types, param_name);
		if (type == kv.value.get_type()) {
			dst->set(param_name, kv.value);
		}
	}
}
