/**
 * file_provider_pack.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/io/file_access_pack.h"
#include "core/templates/hash_map.h"
#include "filesystem_server/file_provider.h"
#include "filesystem_server/project_environment.h"

class FileProviderPack : public FileProvider {
	GDCLASS(FileProviderPack, FileProvider);

private:
	struct PathMD5 {
		uint64_t a = 0;
		uint64_t b = 0;

		bool operator==(const PathMD5 &p_val) const {
			return (a == p_val.a) && (b == p_val.b);
		}
		static uint32_t hash(const PathMD5 &p_val) {
			uint32_t h = hash_murmur3_one_32(p_val.a);
			return hash_fmix32(hash_murmur3_one_32(p_val.b, h));
		}

		PathMD5() {}

		explicit PathMD5(const Vector<uint8_t> &p_buf) {
			a = *((uint64_t *)&p_buf[0]);
			b = *((uint64_t *)&p_buf[8]);
		}
	};

	HashMap<PathMD5, PackedData::PackedFile, PathMD5> files;

	PackedStringArray file_list;

protected:
	static void _bind_methods();

	Ref<ProjectEnvironment> project_environment;

public:
	void add_path(const String &p_pkg_path, const String &p_path, uint64_t p_ofs, uint64_t p_size, const uint8_t *p_md5, PackSource *p_src, bool p_replace_files, bool p_encrypted = false); // for PackSource
	_FORCE_INLINE_ const PackedData::PackedFile *try_get_packed_file(const String &p_path) const;

	static Ref<FileProviderPack> create(const String &p_path);

	bool load_pack(const String &p_path);
	virtual Ref<FileAccess> open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error = nullptr) const override;
	virtual PackedStringArray get_files() const override;
	virtual bool has_file(const String &p_path) const override;

	virtual void on_added_to_filesystem_server() override;
	virtual void on_removed_to_filesystem_server() override;

	Ref<ProjectEnvironment> get_project_environment();

	void clear();

	FileProviderPack();
	~FileProviderPack();
};
