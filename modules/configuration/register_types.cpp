/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"

#include "configuration_server.h"
#include "configuration_set.h"
#include "configuration_table.h"
#include "editor/configuration_editor_plugin.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"

static void _editor_init() {
	EditorNode::get_singleton()->add_editor_plugin(memnew(ConfigurationEditorPlugin));
}
#endif

class ConfigurationModule : public SpikeModule {
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
		if(do_init) {
			GDREGISTER_CLASS(ConfigurationServer);
			GDREGISTER_ABSTRACT_CLASS(ConfigurationResource);
			GDREGISTER_CLASS(ConfigurationSet);
			GDREGISTER_CLASS(ConfigurationTable);
			GDREGISTER_CLASS(ConfigurationTableColumn);
			GDREGISTER_CLASS(ConfigurationTableRow);

		} else {

		}
	}

	static void scene(bool do_init) {
		if (do_init) {
		} else {
			memdelete(ConfigurationServer::get_singleton());
		}
	}
};

IMPL_SPIKE_MODULE(configuration, ConfigurationModule)
