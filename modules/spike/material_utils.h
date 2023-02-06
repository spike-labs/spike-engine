/**
 * material_utils.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#ifndef __MATERIAL_UTILS_H__
#define __MATERIAL_UTILS_H__

#include "scene/resources/material.h"
#include "spike_define.h"

class MaterialUtils {
public:
    static void convert_shader_parameters(const Ref<Material> src, const Ref<Material> dst);
};

#endif // __MATERIAL_UTILS_H__