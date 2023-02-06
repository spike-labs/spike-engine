/**
 * native_wrapper.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#ifndef NATIVE_PLUGIN_H
#define NATIVE_PLUGIN_H

#include "spike_define.h"
#ifdef GODOT_3_X
#include "core/resource.h"
#else
#include "core/io/resource.h"
#endif
#include "core/io/config_file.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"

#define NATIVE_PLUGIN_EXTENSION "gdwrapper"

struct NativeArg {
	union {
		int64_t i;
		double f;
		const char *s;
	} value;
};

typedef int (*NativeWrapperMethodCall)(void **p_args, int p_argcount, NativeArg *p_ret);

class NativeWrapper : public Resource {
	GDCLASS(NativeWrapper, Resource)

	friend class NativeWrapperResourceLoader;
	friend class NativeWrapperResourceSaver;

public:
	enum RetType {
		RET_NONE,
		RET_INT,
		RET_FLOAT,
		RET_CHAR_PTR,
		RET_CONST_CHAR_PTR,
	};

protected:
	void *library;
	String symbol_prefix;
	Dictionary libraries;
	Dictionary dependencies;

	String find_library();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

public:
	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override;

	Error open_library(const String &lib_path);
	void close_library();

	NativeWrapper();
	~NativeWrapper();
};

class NativeWrapperResourceLoader : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE);
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual bool handles_type(const String &p_type) const;
	virtual String get_resource_type(const String &p_path) const;
};

class NativeWrapperResourceSaver : public ResourceFormatSaver {
public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0);
	virtual void get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const;
	virtual bool recognize(const RES &p_resource) const;
};

#endif