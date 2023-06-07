/**
 * annotation_export_plugin.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "editor/export/editor_export.h"

class AnnotationExportPlugin : public EditorExportPlugin {
	GDCLASS(AnnotationExportPlugin, EditorExportPlugin);

private:
	HashMap<StringName, List<Ref<class BaseResourceAnnotation>>> export_annos;
	List<Ref<class ModuleAnnotation>> export_module_annos;
	Ref<class ModuleAnnotation> export_main_module_anno;
	HashMap<String, String> _used_renamed_module_name;

public:
	static String annotation_json_path;
	static String module_annotation_json_path;

	static Ref<EditorExportPlugin> create_annotaion_export_plugin();

protected:
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;
	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override;
	virtual void _export_files_end() override;

	virtual String _get_name() const override {
		return "!AnnotationJsonExporter";
	}
};
