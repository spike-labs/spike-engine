/**
 * native_wrapper.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "native_wrapper.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/json.h"
#include "core/os/os.h"

NativeWrapper::NativeWrapper() {
	library = nullptr;
}

NativeWrapper::~NativeWrapper() {
	// TODO
	// close_library();
}

String NativeWrapper::find_library() {
	String library_path;
	List<Variant> keys;
	libraries.get_key_list(&keys);

	for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
		String k = E->get();
		Vector<String> tags = k.split(".");
		bool all_tags_met = true;
		for (int i = 0; i < tags.size(); i++) {
			String tag = tags[i].strip_edges();
			if (!OS::get_singleton()->has_feature(tag)) {
				all_tags_met = false;
				break;
			}
		}

		if (all_tags_met) {
			library_path = libraries[k];
			break;
		}
	}

	if (library_path.is_empty()) {
		const String os_arch = OS::get_singleton()->get_name().to_lower() + "." + Engine::get_singleton()->get_architecture_name();
		ERR_PRINT(vformat("No NativeWrapper library found for current OS and architecture (%s) in configuration file: %s", os_arch, get_path()));
		return String();
	}

	if (!library_path.is_resource_file() && !library_path.is_absolute_path()) {
		library_path = get_path().get_base_dir().path_join(library_path);
	}

	return library_path;
}

Error NativeWrapper::open_library(const String &lib_path) {
	if (library == nullptr) {
		ERR_FAIL_COND_V(IS_EMPTY(lib_path), ERR_DOES_NOT_EXIST);
#ifdef IOS_ENABLED
		// On iOS we use static linking by default.
		String load_path = "";

		// On iOS dylibs is not allowed, but can be replaced with .framework or .xcframework.
		// If they are used, we can run dlopen on them.
		// They should be located under Frameworks directory, so we need to replace library path.
		if (!lib_path.ends_with(".a")) {
			load_path = ProjectSettings::get_singleton()->globalize_path(lib_path);

			if (!FileAccess::exists(load_path)) {
				String lib_name = lib_path.get_basename().get_file();
				String framework_path_format = "Frameworks/$name.framework/$name";

				Dictionary format_dict;
				format_dict["name"] = lib_name;
				String framework_path = framework_path_format.format(format_dict, "$_");

				load_path = PATH_JOIN(OS::get_singleton()->get_executable_path().get_base_dir(), framework_path);
			}
		}
#elif defined(ANDROID_ENABLED)
		String load_path = lib_path.get_file();
#elif defined(UWP_ENABLED)
		String load_path = lib_path.replace("res://", "");
#elif defined(MACOS_ENABLED)
		// On OSX the exported libraries are located under the Frameworks directory.
		// So we need to replace the library path.
		String load_path = ProjectSettings::get_singleton()->globalize_path(lib_path);
		DirAccessRef da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

		if (!da->file_exists(load_path) && !da->dir_exists(load_path)) {
			load_path = PATH_JOIN(PATH_JOIN(OS::get_singleton()->get_executable_path().get_base_dir(), "../Frameworks"), lib_path.get_file());
		}

		if (da->dir_exists(load_path)) { // Target library is a ".framework", add library base name to the path.
			load_path = PATH_JOIN(load_path, load_path.get_file().get_basename());
		}
#else
		String load_path = ProjectSettings::get_singleton()->globalize_path(lib_path);
#endif

		if (!IS_EMPTY(load_path)) {
			String library_path;
			Error err = OS::get_singleton()->open_dynamic_library(load_path, library, true, &library_path);
			if (err != OK) {
				ERR_PRINT("NativeWrapper dynamic library not found: " + library_path);
				return err;
			}
		}
	}
	return OK;
}

void NativeWrapper::close_library() {
	if (library) {
		OS::get_singleton()->close_dynamic_library(library);
		library = nullptr;
	}
}

Variant NativeWrapper::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	auto ret = Resource::callp(p_method, p_args, p_argcount, r_error);
	if (r_error.error == Callable::CallError::CALL_OK) {
		return ret;
	}

	ret = Variant();
	if (library == nullptr) {
		open_library(find_library());
	}

	if (library) {
		void *funcptr = nullptr;
		auto err = OS::get_singleton()->get_dynamic_library_symbol_handle(library, symbol_prefix + p_method, funcptr, true);
		if (err == OK) {
			r_error.error = Callable::CallError::CALL_OK;

			void *argc[8];
			NativeArg args[8];
			CharString str[8];
			int argn = p_argcount < 8 ? p_argcount : 8;
			for (size_t i = 0; i < argn; i++) {
				switch (p_args[i]->get_type()) {
					case Variant::INT:
						args[i].value.i = p_args[i]->operator signed int();
						break;
					case Variant::BOOL:
						args[i].value.i = p_args[i]->operator bool() ? 1 : 0;
						break;
					case Variant::FLOAT:
						args[i].value.f = p_args[i]->operator float();
						break;
					default:
						str[i] = p_args[i]->operator String().utf8();
						args[i].value.s = str[i].get_data();
						break;
				}
				argc[i] = &args[i].value;
			}

			NativeWrapperMethodCall method = (NativeWrapperMethodCall)funcptr;
			NativeArg p_ret;
			int r_type = method(argc, argn, &p_ret);
			switch (r_type) {
				case RetType::RET_INT:
					ret = Variant(p_ret.value.i);
					break;
				case RetType::RET_FLOAT:
					ret = Variant(p_ret.value.f);
					break;
				case RetType::RET_CHAR_PTR: {
					ret = Variant(p_ret.value.s);
					free((void *)p_ret.value.s);
				} break;
				case RetType::RET_CONST_CHAR_PTR:
					ret = Variant(p_ret.value.s);
					break;
				default:
					break;
			}
		} else {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		}
	}
	return ret;
}

bool NativeWrapper::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "symbol_prefix") {
		symbol_prefix = p_value;
	} else if (p_name == "libraries") {
		libraries = p_value;
	} else if (p_name == "dependencies") {
		dependencies = p_value;
	} else {
		return false;
	}

	return true;
	return false;
}

bool NativeWrapper::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "symbol_prefix") {
		r_ret = symbol_prefix;
	} else if (p_name == "libraries") {
		r_ret = libraries;
	} else if (p_name == "dependencies") {
		r_ret = dependencies;
	} else {
		return false;
	}
	return true;
}

void NativeWrapper::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::STRING, PNAME("symbol_prefix")));
	p_list->push_back(PropertyInfo(Variant::DICTIONARY, PNAME("libraries")));
	p_list->push_back(PropertyInfo(Variant::DICTIONARY, PNAME("dependencies")));
}

void NativeWrapper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_library", "path"), &NativeWrapper::open_library);
	ClassDB::bind_method(D_METHOD("close_library"), &NativeWrapper::close_library);
}

Ref<Resource> NativeWrapperResourceLoader::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Ref<ConfigFile> config;
	config.instantiate();

	Error err = config->load(p_path);

	if (r_error) {
		*r_error = err;
	}

	if (err != OK) {
		ERR_PRINT("Error loading NativeWrapper configuration file: " + p_path);
		return Ref<Resource>();
	}

	Ref<NativeWrapper> lib;
	lib.instantiate();

	if (config->has_section_key("configuration", "symbol_prefix")) {
		lib->symbol_prefix = config->get_value("configuration", "symbol_prefix");
	}

	List<String> libraries;
	config->get_section_keys("libraries", &libraries);

	String library_path;
	for (const String &E : libraries) {
		lib->libraries[E] = config->get_value("libraries", E);
	}

	if (r_error) {
		*r_error = OK;
	}

	return lib;
}

void NativeWrapperResourceLoader::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back(NATIVE_PLUGIN_EXTENSION);
}

bool NativeWrapperResourceLoader::handles_type(const String &p_type) const {
	return p_type == "NativeWrapper";
}

String NativeWrapperResourceLoader::get_resource_type(const String &p_path) const {
	String el = p_path.get_extension().to_lower();
	if (el == NATIVE_PLUGIN_EXTENSION) {
		return "NativeWrapper";
	}
	return "";
}

Error NativeWrapperResourceSaver::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<NativeWrapper> wrapper = p_resource;

	if (wrapper.is_null())
		return ERR_INVALID_PARAMETER;

	Ref<ConfigFile> config;
	config.instantiate();

	config->set_value("configuration", "symbol_prefix", wrapper->symbol_prefix);
	for (const Variant *key = wrapper->libraries.next(nullptr); key; key = wrapper->libraries.next(key)) {
		String library_path = *key;
		String target_path = wrapper->libraries[*key];
		config->set_value("libraries", *key, wrapper->libraries[*key]);
	}

	for (const Variant *key = wrapper->dependencies.next(nullptr); key; key = wrapper->dependencies.next(key)) {
		String library_path = *key;
		String target_path = wrapper->dependencies[*key];
		config->set_value("dependencies", *key, wrapper->dependencies[*key]);
	}

	return config->save(p_path);
}

void NativeWrapperResourceSaver::get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<NativeWrapper>(*p_resource)) {
		p_extensions->push_back(NATIVE_PLUGIN_EXTENSION);
	}
}
bool NativeWrapperResourceSaver::recognize(const RES &p_resource) const {
	return Object::cast_to<NativeWrapper>(*p_resource) != nullptr;
}