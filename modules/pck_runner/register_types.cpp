/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"

#include "core/object/class_db.h"
#include "pck_runner.h"
#include "pck_scene_tree.h"

PCKRunner *singleton;
class MODPCKRunner : public SpikeModule {
public:
	static void core(bool do_init) {
		if (do_init) {
			GDREGISTER_ABSTRACT_CLASS(PCKRunner);
			singleton = PCKRunner::get_singleton();
		} else {
			memdelete(singleton);
		}
	}

	static void scene(bool do_init) {
		if (do_init) {
			GDREGISTER_CLASS(PCKSceneTree);
#ifdef TOOLS_ENABLED
			if (!Engine::get_singleton()->is_editor_hint())
#endif
			{
				PCKSceneTree::make_current();
			}
		}
	}
};

IMPL_SPIKE_MODULE(pck_runner, MODPCKRunner)