/**
 * compatible_define.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#ifndef __COMPATIBLE_DEFINE_H
#define __COMPATIBLE_DEFINE_H
//#define GODOT_3_X

#ifdef GODOT_3_X
#define GDREGISTER_CLASS(T) ClassDB::register_class<T>()
#define INC_CLASS_DB "core/class_db.h"
#define INC_VISUAL_INSTANCE_3D "scene/3d/visual_instance.h"
#define INC_NODE_3D_EDITOR_PLUGIN <editor/plugins/spatial_editor_plugin.h>
#define INC_REF_COUNTED "core/reference.h"
#define INC_FILE_ACCESS "core/os/file_access.h"
#define INC_DIR_ACCESS "core/os/dir_access.h"
#define BaseTexture Texture
#define UndoRedoRef UndoRedo *
#define CALL_ERROR Variant::CallError
#define ITER_BEGIN(x) (x).front()
#define ITER_NEXT(e) e = e->next()
#define ITER_GET(e) e->get()
#define TYPE_REAL Variant::REAL
#define IS_EMPTY(s) (s).empty()
#define CLS_INSTANTIATE(n) ClassDB::instance(n)
#define REF_INSTANTIATE(r) (r).instance()
#define LOOKUP_RESULT_TYPE_CLASS ScriptLanguage::LookupResult::RESULT_CLASS
#define GD_CALL(o, m, args, n, err) (o).call(m, args, n, err)
#define VAR_VALL(v, m, args, n, r, err) Variant r = (v).call(m, args, n, err)
#define callable_mp_n(this, klass, callable) (this), #callable
#define callable_mp_b(this, klass, callable, binds) (this), #callable, binds
#define GET_UNDO_REDO(ed) (ed).get_undo_redo()
#define GET_EDITOR_NODE(ed) ed
#define EDITOR_DATA_DIR() EditorSettings::get_singleton()->get_data_dir()
#define FILE_REF_IS_NULL(f) !f
#define MEM_DEL_FILE(f) \
	if (f != nullptr) { \
		memdelete(f);   \
	}
#define RBSet Set
#define PATH_JOIN(s, f) s.plus_file(f)
#else
#define ADD_FORMAT_LOADER(loader) \
	REF_INSTANTIATE(loader);      \
	ResourceLoader::add_resource_format_loader(loader)

#define ADD_FORMAT_SAVER(saver) \
	REF_INSTANTIATE(saver);     \
	ResourceSaver::add_resource_format_saver(saver)

#define REMOVE_FORMAT_LOADER(loader)                           \
	if ((loader).is_valid()) {                                 \
		ResourceLoader::remove_resource_format_loader(loader); \
		(loader).unref();                                      \
	}

#define REMOVE_FORMAT_SAVER(saver)                      \
	ResourceSaver::remove_resource_format_saver(saver); \
	saver.unref();

#define INC_CLASS_DB "core/object/class_db.h"
#define INC_VISUAL_INSTANCE_3D "scene/3d/visual_instance_3d.h"
#define INC_NODE_3D_EDITOR_PLUGIN <editor/plugins/node_3d_editor_plugin.h>
#define INC_REF_COUNTED "core/object/ref_counted.h"
#define INC_FILE_ACCESS "core/io/file_access.h"
#define INC_DIR_ACCESS "core/io/dir_access.h"
#define Set HashSet
#define Map HashMap
#define Spatial Node3D
#define Camera Camera3D
#define BaseTexture Texture2D
#define ToolButton Button
#define VisualInstance VisualInstance3D
#define SpatialEditor Node3DEditor
#define VisualServer RenderingServer
#define DirAccessRef Ref<DirAccess>
#define UndoRedoRef Ref<EditorUndoRedoManager>
#define CALL_ERROR Callable::CallError
#define ITER_BEGIN(x) (x).begin()
#define ITER_NEXT(e) e++
#define ITER_GET(e) e->value
#define Reference RefCounted
#define PoolStringArray PackedStringArray
#define ScriptCodeCompletionOption CodeCompletionOption
#define TYPE_REAL Variant::FLOAT
#define RES Ref<Resource>
#define FileAccessRef Ref<FileAccess>
#define IS_EMPTY(s) (s).is_empty()
#define CLS_INSTANTIATE(n) ClassDB::instantiate(n)
#define REF_INSTANTIATE(r) (r).instantiate()
#define REF Ref<RefCounted>
#define LOOKUP_RESULT_TYPE_CLASS ScriptLanguage::LookupResultType::LOOKUP_RESULT_CLASS
#define GlobalConstants CoreConstants
#define Quat Quaternion
#define Transform Transform3D
#define GD_CALL(o, m, args, n, err) (o).callp(m, args, n, err)
#define VAR_VALL(v, m, args, n, r, err) \
	Variant r;                          \
	(v).callp(m, args, n, r, err)
//(this), &klass::callable
#define callable_mp_n(this, klass, callable) Callable(memnew(CallableCustomMethodPointer(this, &klass::callable)))
#define callable_mp_b(this, klass, callable, binds) Callable(memnew(CallableCustomBind(callable_mp_n(this, klass, callable), binds)))
#define GET_UNDO_REDO(ed) EditorNode::get_undo_redo()
#define GET_EDITOR_NODE(ed) EditorNode::get_singleton()
#define EDITOR_DATA_DIR() EditorPaths::get_singleton()->get_data_dir()
#define FILE_REF_IS_NULL(f) f.is_null()
#define MEM_DEL_FILE(f)
#define PATH_JOIN(s, f) (s).path_join(f)
#endif

#endif
