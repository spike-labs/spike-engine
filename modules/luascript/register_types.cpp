/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include "register_types.h"

#include "luascript.h"
#include "luascript_language.h"
#include "luascript_resource_formate_loader.h"
#include "luascript_resource_formate_saver.h"

#ifdef TOOLS_ENABLED
#ifdef GODOT_3_X
#include "editor/editor_export.h"
#else
#include "editor/export/editor_export.h"
#endif
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/luascript_export_plugin.h"
#include "editor/luascript_syntax_highlighter.h"
#include "editor/plugins/script_editor_plugin.h"

static void _editor_init() {
	Ref<LuaScriptExportPlugin> lua_export;
	REF_INSTANTIATE(lua_export);
	EditorExport::get_singleton()->add_export_plugin(lua_export);

#ifdef GODOT_3_X
	ScriptEditor::register_create_syntax_highlighter_function(LuaScriptSyntaxHighlighter::create);
#else
	Ref<LuaScriptSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	ScriptEditor::get_singleton()->register_syntax_highlighter(syntax_highlighter);
#endif

	EDITOR_DEF(K_EXEC_PATH, K_EDITOR_BUILTIN);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, K_EXEC_PATH, PROPERTY_HINT_ENUM, K_EDITOR_LIST));
}

#endif

static LuaScriptLanguage *script_language = nullptr;
static Ref<LuaScriptResourceFormatLoader> resource_loader;
static Ref<LuaScriptResourceFormatSaver> resource_saver;

class MODLuaScript : public SpikeModule {
public:
	static void servers(bool do_init) {
		if (do_init) {
			GDREGISTER_CLASS(LuaScript);
			script_language = memnew(LuaScriptLanguage);
			ScriptServer::register_language(script_language);
			ADD_FORMAT_LOADER(resource_loader);
			ADD_FORMAT_SAVER(resource_saver);
		} else {
			if (script_language) {
				ScriptServer::unregister_language(script_language);
				memdelete(script_language);
			}

			REMOVE_FORMAT_LOADER(resource_loader);
			REMOVE_FORMAT_SAVER(resource_saver);
		}
	}

#ifdef TOOLS_ENABLED
	static void editor(bool do_init) {
		if (do_init) {
			EditorNode::add_init_callback(_editor_init);
		} else {
		}
	}
#endif
};

IMPL_SPIKE_MODULE(luascript, MODLuaScript)
