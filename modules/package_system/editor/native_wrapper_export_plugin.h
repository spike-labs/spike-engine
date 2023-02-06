/**
 * native_wrapper_export_plugin.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once
#include "spike_define.h"
#include "editor/export/editor_export.h"

class NativeWrapperExportPlugin : public EditorExportPlugin {
    protected:
	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;
	virtual String _get_name() const override { return "NativeWrapper"; }
};