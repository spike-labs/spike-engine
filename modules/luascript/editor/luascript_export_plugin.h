/**
 * This file is part of Lua binding for Godot Engine.
 *
 */
#pragma once

#include "spike_define.h"
#ifdef GODOT_3_X
#include "editor/editor_export.h"
#else
#include "editor/export/editor_export.h"
#endif

class LuaScriptExportPlugin : public EditorExportPlugin {
	GDCLASS(LuaScriptExportPlugin, EditorExportPlugin);

public:
#ifndef GODOT_3_X
	virtual String _get_name() const override {
		return "LuaScript";
	}
#endif
	virtual void _export_file(const String &p_path, const String &p_type, const Set<String> &p_features) override;
};
