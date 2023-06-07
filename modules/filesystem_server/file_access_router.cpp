/**
 * file_access_router.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "file_access_router.h"
#include "core/error/error_list.h"
#include "core/io/file_access.h"
#include "filesystem_server/filesystem_server.h"
#include <stdint.h>

Error FileAccessRouter::open_internal(const String &p_path, int p_mode_flags) {
	Error err = OK;
	Ref<FileAccess> fa = FileSystemServer::get_singleton()->open(p_path, (FileAccess::ModeFlags)p_mode_flags, &err);
	if (err == OK) {
		f = fa;
	}
	return err;
}

uint64_t FileAccessRouter::_get_modified_time(const String &p_file) {
	return FileSystemServer::get_singleton()->get_modified_time(p_file);
}

bool FileAccessRouter::is_open() const {
	if (f.is_valid()) {
		return f->is_open();
	} else {
		return false;
	}
}

void FileAccessRouter::seek(uint64_t p_position) {
	ERR_FAIL_COND_MSG(f.is_null(), "File must be opened before use.");
	f->seek(p_position);
}

void FileAccessRouter::seek_end(int64_t p_position) {
	ERR_FAIL_COND_MSG(f.is_null(), "File must be opened before use.");
	f->seek_end(p_position);
}

uint64_t FileAccessRouter::get_position() const {
	ERR_FAIL_COND_V_MSG(f.is_null(), -1, "File must be opened before use.");
	return f->get_position();
}

uint64_t FileAccessRouter::get_length() const {
	ERR_FAIL_COND_V_MSG(f.is_null(), -1, "File must be opened before use.");
	return f->get_length();
}

bool FileAccessRouter::eof_reached() const {
	ERR_FAIL_COND_V_MSG(f.is_null(), true, "File must be opened before use.");
	return f->eof_reached();
}

uint8_t FileAccessRouter::get_8() const {
	ERR_FAIL_COND_V_MSG(f.is_null(), 0, "File must be opened before use.");
	return f->get_8();
}

uint64_t FileAccessRouter::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V_MSG(f.is_null(), -1, "File must be opened before use.");
	return f->get_buffer(p_dst, p_length);
}

void FileAccessRouter::set_big_endian(bool p_big_endian) {
	ERR_FAIL_COND_MSG(f.is_null(), "File must be opened before use.");
	f->set_big_endian(p_big_endian);
}

Error FileAccessRouter::get_error() const {
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_UNAVAILABLE, "File must be opened before use.");
	return f->get_error();
}

void FileAccessRouter::flush() {
	ERR_FAIL_COND_MSG(f.is_null(), "File must be opened before use.");
	f->flush();
}

void FileAccessRouter::store_8(uint8_t p_dest) {
	ERR_FAIL_COND_MSG(f.is_null(), "File must be opened before use.");
	f->store_8(p_dest);
}

void FileAccessRouter::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND_MSG(f.is_null(), "File must be opened before use.");
	f->store_buffer(p_src, p_length);
}

void FileAccessRouter::close() {
	if (f.is_valid())
		f->close();
}

bool FileAccessRouter::file_exists(const String &p_name) {
	return FileSystemServer::get_singleton()->file_exists(p_name);
}
