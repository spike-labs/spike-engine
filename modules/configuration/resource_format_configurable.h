/**
 * resource_format_configurable.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "scene/resources/resource_format_text.h"
#include "core/io/resource_format_binary.h"

class OverrideFormatLoaderBinary: public ResourceFormatLoaderBinary {
    virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
};

class OverrideFormatLoaderText : public ResourceFormatLoaderText {
    virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
};
