/**
 * spike_file_access.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#ifndef SPIKE_FILE_ACCESS_H
#define SPIKE_FILE_ACCESS_H

#include "core/io/file_access_pack.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/file_access_memory.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/version.h"
#include "core/templates/rb_map.h"

#include <stdlib.h>

#define RES_HEAD_INDEX 6

class SpikePackSource : public PackSource {

private:
	static SpikePackSource *instance;

	void _fix_remap_and_import_files(List<String> *files, const String &p_key);

public:
	Error add_uids_from_cache(const String &p_key);
	//Error print_cache();

	virtual bool try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset) override;
	virtual bool try_open_pack(const String &p_path, const String &p_key, bool p_replace_files, uint64_t p_offset);
	Ref<FileAccess> get_file(const String &p_path, PackedData::PackedFile *p_file) override;

	static SpikePackSource *get_singleton();

	SpikePackSource();
	~SpikePackSource();
};

class SpikeFileAccess : public FileAccessPack {

};


class FileAccessVirtual : public FileAccess {
	uint8_t *data = nullptr;
	uint64_t length = 0;
	mutable uint64_t pos = 0;

	static Ref<FileAccess> create();
public:
	static void register_file(String p_name, Vector<uint8_t> p_data);
	static bool has_virual_file(String p_name);
	static void cleanup();

	virtual Error open_custom(const uint8_t *p_data, uint64_t p_len); ///< open a file
	virtual Error open_internal(const String &p_path, int p_mode_flags) override; ///< open a file
	virtual bool is_open() const override; ///< true when file is open

	virtual void seek(uint64_t p_position) override; ///< seek to a given position
	virtual void seek_end(int64_t p_position) override; ///< seek from the end of file
	virtual uint64_t get_position() const override; ///< get position in the file
	virtual uint64_t get_length() const override; ///< get size of the file

	virtual bool eof_reached() const override; ///< reading passed EOF

	virtual uint8_t get_8() const override; ///< get a byte

	virtual uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override; ///< get an array of bytes

	virtual Error get_error() const override; ///< get last error

	virtual void flush() override;
	virtual void store_8(uint8_t p_byte) override; ///< store a byte
	virtual void store_buffer(const uint8_t *p_src, uint64_t p_length) override; ///< store an array of bytes

	virtual void close() override { }
	virtual bool file_exists(const String &p_name) override; ///< return true if a file exists

	virtual uint64_t _get_modified_time(const String &p_file) override { return 0; }
	virtual uint32_t _get_unix_permissions(const String &p_file) override { return 0; }
	virtual Error _set_unix_permissions(const String &p_file, uint32_t p_permissions) override { return FAILED; }

	FileAccessVirtual() {}
};

#endif // SPIKE_FILE_ACCESS_H
