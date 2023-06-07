/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"

#include "ai_assistant.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/editor_aitools_panel.h"
#endif

class MODAITools : public SpikeModule {
public:
#ifdef TOOLS_ENABLED
	static void editor_inited() {
		EditorNode::get_singleton()->add_bottom_panel_item(STTR("Coding Helper"), memnew(EditorAIToolsPanel));
	}

	static void editor(bool do_init) {
		if (do_init) {
			EditorNode::add_init_callback(editor_inited);
		} else {

		}
	}

#endif
	static void servers(bool do_init) {
		if (do_init) {
			GDREGISTER_CLASS(AIAssistant);
		}
	}
};


IMPL_SPIKE_MODULE(aitools, MODAITools)
