/**
 * spike_define.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#ifndef __SPIKE_DEFINE_H
#define __SPIKE_DEFINE_H

#include "compatible_define.h"
#include "modules/modules_enabled.gen.h"
#include "scene/main/node.h"
#include "spike/spike_utility.h"
#include "spike/string/sstring.h"
#include "spike_log.h"
#include "spike_utils_define.h"

// Replacement of godot object. object `t` must has the same size with type `T`
#define MEM_REPLACE_OBJ(t, S, T) \
	(t)->~S();                   \
	memnew_placement(t, T)

#ifdef DEBUG_METHODS_ENABLED
#define METHID_BIND_REPLACE(T, m, f)                                                          \
	do {                                                                                      \
		auto *method = ClassDB::classes.getptr(T::get_class_static())->method_map.getptr(#m); \
		if (method) {                                                                         \
			auto *sub_method = create_method_bind(f);                                         \
			sub_method->set_name((*method)->get_name());                                      \
			sub_method->set_argument_names((*method)->get_argument_names());                  \
			sub_method->set_default_arguments((*method)->get_default_arguments());            \
			sub_method->set_hint_flags((*method)->get_hint_flags());                          \
			memdelete(*method);                                                               \
			*method = sub_method;                                                             \
		}                                                                                     \
	} while (0)
#else
#define METHID_BIND_REPLACE(T, m, f)                                                          \
	do {                                                                                      \
		auto *method = ClassDB::classes.getptr(T::get_class_static())->method_map.getptr(#m); \
		if (method) {                                                                         \
			auto *sub_method = create_method_bind(f);                                         \
			sub_method->set_name((*method)->get_name());                                      \
			sub_method->set_default_arguments((*method)->get_default_arguments());            \
			sub_method->set_hint_flags((*method)->get_hint_flags());                          \
			memdelete(*method);                                                               \
			*method = sub_method;                                                             \
		}                                                                                     \
	} while (0)
#endif

class SpikeModule {
public:
	static void core(bool do_init) {}
	static void servers(bool do_init) {}
	static void scene(bool do_init) {}
	static void editor(bool do_init) {}
};

#ifdef GODOT_3_X
#define DECLEAR_REG_TYPES(m)     \
	void register_##m##_types(); \
	void unregister_##m##_types()

#define IMPL_SPIKE_MODULE(module, klass) \
	void register_##module##_types() {   \
		klass::core(true);               \
		klass::servers(true);            \
		klass::scene(true);              \
		klass::editor(true);             \
	}                                    \
	void unregister_##module##_types() { \
		klass::core(false);              \
		klass::servers(false);           \
		klass::scene(false);             \
		klass::editor(false);            \
	}
#else
#define DECLEAR_REG_TYPES(m)                                         \
	void initialize_##m##_module(ModuleInitializationLevel p_level); \
	void uninitialize_##m##_module(ModuleInitializationLevel p_level)

#define IMPL_SPIKE_MODULE(m, klass)                                     \
	void initialize_##m##_module(ModuleInitializationLevel p_level) {   \
		switch (p_level) {                                              \
			case MODULE_INITIALIZATION_LEVEL_CORE:                      \
				klass::core(true);                                      \
				break;                                                  \
			case MODULE_INITIALIZATION_LEVEL_SERVERS:                   \
				klass::servers(true);                                   \
				break;                                                  \
			case MODULE_INITIALIZATION_LEVEL_SCENE:                     \
				klass::scene(true);                                     \
				break;                                                  \
			case MODULE_INITIALIZATION_LEVEL_EDITOR:                    \
				klass::editor(true);                                    \
				break;                                                  \
			default:                                                    \
				break;                                                  \
		}                                                               \
	}                                                                   \
                                                                        \
	void uninitialize_##m##_module(ModuleInitializationLevel p_level) { \
		switch (p_level) {                                              \
			case MODULE_INITIALIZATION_LEVEL_CORE:                      \
				klass::core(false);                                     \
				break;                                                  \
			case MODULE_INITIALIZATION_LEVEL_SERVERS:                   \
				klass::servers(false);                                  \
				break;                                                  \
			case MODULE_INITIALIZATION_LEVEL_SCENE:                     \
				klass::scene(false);                                    \
				break;                                                  \
			case MODULE_INITIALIZATION_LEVEL_EDITOR:                    \
				klass::editor(false);                                   \
				break;                                                  \
			default:                                                    \
				break;                                                  \
		}                                                               \
	}

#endif

#endif
