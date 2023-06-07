/**
 * project_scanner.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "core/core_bind.h"
#include "core/error/error_list.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"
#include "filesystem_server/file_provider.h"

class ProjectScanner : public RefCounted {
	GDCLASS(ProjectScanner, RefCounted);

	Ref<FileProvider> _provider;

	PackedStringArray _scenes;
	PackedStringArray _translations;
	HashMap<String, HashMap<String, String>> _scene_texts;

protected:
	void _scan_scene_text(const String &p_path);

public:
	Error scan(Ref<FileProvider> p_provider);

	PackedStringArray get_scenes() const;
	HashMap<String, String> get_scene_texts(const String &p_path) const;
	Dictionary _get_scene_texts(const String &p_path) const;

	PackedStringArray get_translations() const;

protected:
	static void _bind_methods();

public:
	ProjectScanner();
	~ProjectScanner();
};
