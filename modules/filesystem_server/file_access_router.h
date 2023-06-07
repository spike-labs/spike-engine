/**
 * file_access_router.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */


#include "core/io/file_access.h"
#include "core/object/object.h"
#include <stdint.h>
class FileAccessRouter : public FileAccess {
	GDCLASS(FileAccessRouter, FileAccess);

	Ref<FileAccess> f;

	virtual Error open_internal(const String &p_path, int p_mode_flags) override; ///< open a file
	virtual uint64_t _get_modified_time(const String &p_file) override;
	virtual uint32_t _get_unix_permissions(const String &p_file) override { return 0; }
	virtual Error _set_unix_permissions(const String &p_file, uint32_t p_permissions) override { return FAILED; }

public:
	virtual bool is_open() const override;

	virtual void seek(uint64_t p_position) override;
	virtual void seek_end(int64_t p_position = 0) override;
	virtual uint64_t get_position() const override;
	virtual uint64_t get_length() const override;

	virtual bool eof_reached() const override;

	virtual uint8_t get_8() const override;

	virtual uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;

	virtual void set_big_endian(bool p_big_endian) override;

	virtual Error get_error() const override;

	virtual void flush() override;
	virtual void store_8(uint8_t p_dest) override;

	virtual void store_buffer(const uint8_t *p_src, uint64_t p_length) override;
	
	virtual void close() override;

	virtual bool file_exists(const String &p_name) override;
};