/**
 * file_provider_remap.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/templates/hash_map.h"
#include "filesystem_server/file_provider.h"

class FileProviderRemap : public FileProvider {
	GDCLASS(FileProviderRemap, FileProvider);

private:
	struct Remap {
		String from;
		String to;
		Ref<FileProvider> provider;
	};

	HashMap<String, Remap> remaps;

protected:
	static void _bind_methods();

public:
	void add_remap(const String &p_path, const String &p_to);
	void add_provider_remap(const String &p_path, const Ref<FileProvider> &p_provider, const String &p_to);

	bool has_remap(const String &p_path) const;

	bool remove_remap(const String &p_path);

	virtual Ref<FileAccess> open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error = nullptr) const override;

	virtual PackedStringArray get_files() const override;
	virtual bool has_file(const String &p_path) const override;

	FileProviderRemap();
	~FileProviderRemap();
};
