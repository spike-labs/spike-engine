/**
 * resource_format_annotation.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "resource_format_annotation.h"
#include "annotation_resource.h"
#include "core/io/resource_format_binary.h"

#pragma region ResourceFormatLoaderAnnotation
Ref<Resource> ResourceFormatLoaderAnnotation::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	return ResourceFormatLoaderBinary::singleton->load(p_path, p_original_path, r_error, p_use_sub_threads, r_progress, p_cache_mode);
}

void ResourceFormatLoaderAnnotation::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("anno");
	p_extensions->push_back("manno");
}

bool ResourceFormatLoaderAnnotation::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, SNAME("BaseResourceAnnotation")) || ClassDB::is_parent_class(p_type, SNAME("MemberAnnotation"));
}

String ResourceFormatLoaderAnnotation::get_resource_type(const String &p_path) const {
	auto el = p_path.get_extension().to_lower();
	if (el == "anno") {
		return "BaseResourceAnnotation";
	}
	if (el == "manno") {
		return "MemberAnnotation";
	}
	return "";
}

#pragma endregion

#pragma region ResourceFormatSaverAnnotation
Error ResourceFormatSaverAnnotation::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	return ResourceFormatSaverBinary::singleton->save(p_resource, p_path, p_flags);
}

void ResourceFormatSaverAnnotation::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<BaseResourceAnnotation>(*p_resource)) {
		p_extensions->push_back("anno");
	}
	if (Object::cast_to<MemberAnnotation>(*p_resource)) {
		p_extensions->push_back("manno");
	}
}

bool ResourceFormatSaverAnnotation::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<BaseResourceAnnotation>(*p_resource) || Object::cast_to<MemberAnnotation>(*p_resource);
}
#pragma endregion