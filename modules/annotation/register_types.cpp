/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"

#ifdef TOOLS_ENABLED
#include "annotation/annotation_export_plugin.h"
#include "annotation/annotation_plugin.h"
#include "annotation/script_annotation_parser_manager.h"
#include "annotation/script_annotation_parsers/gdscript_annotation_parser.h"
#include "annotation/script_annotation_parsers/luascript_annotation_parser.h"
#include "annotation_resource.h"
#include "core/object/class_db.h"
#include "editor/editor_node.h"
#include "resource_format_annotation.h"

#include "../compatible_define.h"
#endif

class MODAnnotation : public SpikeModule {
public:
#ifdef TOOLS_ENABLED
	static Ref<ResourceFormatLoaderAnnotation> anno_loader;
	static Ref<ResourceFormatSaverAnnotation> anno_saver;

	static void editor(bool do_init) {
		if (do_init) {
			GDREGISTER_ABSTRACT_CLASS(Annotation);
			GDREGISTER_ABSTRACT_CLASS(MemberAnnotation);
			GDREGISTER_ABSTRACT_CLASS(BaseResourceAnnotation);
			GDREGISTER_ABSTRACT_CLASS(AnnotationExportPlugin);

			GDREGISTER_CLASS(EnumElementAnnotation);
			GDREGISTER_CLASS(EnumAnnotation);
			GDREGISTER_CLASS(VariableAnnotation);
			GDREGISTER_CLASS(ParameterAnnotation);
			GDREGISTER_CLASS(PropertyAnnotation);
			GDREGISTER_CLASS(ConstantAnnotation);
			GDREGISTER_CLASS(MethodAnnotation);
			GDREGISTER_CLASS(SignalAnnotation);
			GDREGISTER_CLASS(NodeAnnotation);
			GDREGISTER_CLASS(SceneAnnotation);
			GDREGISTER_CLASS(ResourceAnnotation);
			GDREGISTER_CLASS(ClassAnnotation);
			GDREGISTER_CLASS(ModuleAnnotation);

			ADD_FORMAT_LOADER(anno_loader);
			ADD_FORMAT_SAVER(anno_saver);

			GDREGISTER_ABSTRACT_CLASS(EditorPropertyAnnotation);

			ScriptAnnotationParserManager::register_annotation_parser(memnew(GDScriptAnnotationParser));
			ScriptAnnotationParserManager::register_annotation_parser(memnew(LuaScriptAnnotationParser));

			EditorNode::add_init_callback(&editor_inited);

		} else {
			REMOVE_FORMAT_LOADER(anno_loader);
			REMOVE_FORMAT_SAVER(anno_saver);

			ScriptAnnotationParserManager::clean();
		}
	}

	static void editor_inited() {
		EditorNode::get_singleton()->add_editor_plugin(AnnotationPlugin::get_singleton());
		ScriptAnnotationParserManager::setup();
	}

#endif
};

#ifdef TOOLS_ENABLED
Ref<ResourceFormatLoaderAnnotation> MODAnnotation::anno_loader = nullptr;
Ref<ResourceFormatSaverAnnotation> MODAnnotation::anno_saver = nullptr;
#endif

IMPL_SPIKE_MODULE(annotation, MODAnnotation)