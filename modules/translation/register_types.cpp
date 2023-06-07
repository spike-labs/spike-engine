/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"
#include "translation_patch_server.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"

static void _editor_init() {
}
#endif

class TranslationModule : public SpikeModule {
public:
#ifdef TOOLS_ENABLED
	static void editor(bool do_init) {
		if (do_init) {
			EditorNode::add_init_callback(&_editor_init);
		} else {
		}
	}
#endif

public:
	static void servers(bool do_init) {
		if (do_init) {
			GDREGISTER_ABSTRACT_CLASS(TranslationPatchServer);
		} else {
		}
	}

	static void scene(bool do_init) {
		if (do_init) {
		} else {
		}
	}
};

IMPL_SPIKE_MODULE(translation, TranslationModule)
