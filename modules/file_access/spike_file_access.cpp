/**
 * spike_file_access.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "spike_file_access.h"
#include "core/io/file_access_memory.h"
#include "core/io/config_file.h"

#include "core/config/project_settings.h"

SpikePackSource *SpikePackSource::instance = nullptr;

bool SpikePackSource::try_open_pack(const String &p_path, const String &p_key, bool p_replace_files, uint64_t p_offset = 0) {
	List<String> file_list;
    Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}

	bool pck_header_found = false;

	// Search for the header at the start offset - standalone PCK file.
	f->seek(p_offset);
	uint32_t magic = f->get_32();
	if (magic == PACK_HEADER_MAGIC) {
		pck_header_found = true;
	}

	// Search for the header in the executable "pck" section - self contained executable.
	if (!pck_header_found) {
		// Loading with offset feature not supported for self contained exe files.
		if (p_offset != 0) {
			ERR_FAIL_V_MSG(false, "Loading self-contained executable with offset not supported.");
		}

		int64_t pck_off = OS::get_singleton()->get_embedded_pck_offset();
		if (pck_off != 0) {
			// Search for the header, in case PCK start and section have different alignment.
			for (int i = 0; i < 8; i++) {
				f->seek(pck_off);
				magic = f->get_32();
				if (magic == PACK_HEADER_MAGIC) {
#ifdef DEBUG_ENABLED
					print_verbose("PCK header found in executable pck section, loading from offset 0x" + String::num_int64(pck_off - 4, 16));
#endif
					pck_header_found = true;
					break;
				}
				pck_off++;
			}
		}
	}

	// Search for the header at the end of file - self contained executable.
	if (!pck_header_found) {
		// Loading with offset feature not supported for self contained exe files.
		if (p_offset != 0) {
			ERR_FAIL_V_MSG(false, "Loading self-contained executable with offset not supported.");
		}

		f->seek_end();
		f->seek(f->get_position() - 4);
		magic = f->get_32();

		if (magic == PACK_HEADER_MAGIC) {
			f->seek(f->get_position() - 12);
			uint64_t ds = f->get_64();
			f->seek(f->get_position() - ds - 8);
			magic = f->get_32();
			if (magic == PACK_HEADER_MAGIC) {
#ifdef DEBUG_ENABLED
				print_verbose("PCK header found at the end of executable, loading from offset 0x" + String::num_int64(f->get_position() - 4, 16));
#endif
				pck_header_found = true;
			}
		}
	}

	if (!pck_header_found) {
		return false;
	}

	uint32_t version = f->get_32();
	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	f->get_32(); // patch number, not used for validation.

	ERR_FAIL_COND_V_MSG(version != PACK_FORMAT_VERSION, false, "Pack version unsupported: " + itos(version) + ".");
	ERR_FAIL_COND_V_MSG(ver_major > VERSION_MAJOR || (ver_major == VERSION_MAJOR && ver_minor > VERSION_MINOR), false, "Pack created with a newer version of the engine: " + itos(ver_major) + "." + itos(ver_minor) + ".");

	uint32_t pack_flags = f->get_32();
	uint64_t file_base = f->get_64();

	bool enc_directory = (pack_flags & PACK_DIR_ENCRYPTED);

	for (int i = 0; i < 16; i++) {
		//reserved
		f->get_32();
	}

	int file_count = f->get_32();

	if (enc_directory) {
		Ref<FileAccessEncrypted> fae;
		fae.instantiate();
		ERR_FAIL_COND_V_MSG(fae.is_null(), false, "Can't open encrypted pack directory.");

		Vector<uint8_t> key;
		key.resize(32);
		for (int i = 0; i < key.size(); i++) {
			key.write[i] = script_encryption_key[i];
		}

		Error err = fae->open_and_parse(f, key, FileAccessEncrypted::MODE_READ, false);
		ERR_FAIL_COND_V_MSG(err, false, "Can't open encrypted pack directory.");
		f = fae;
	}

	for (int i = 0; i < file_count; i++) {
		uint32_t sl = f->get_32();
		CharString cs;
		cs.resize(sl + 1);
		f->get_buffer((uint8_t *)cs.ptr(), sl);
		cs[sl] = 0;

		String path;
		path.parse_utf8(cs.ptr());


		if(!p_key.is_empty()) {
			path = path.insert(RES_HEAD_INDEX, p_key + "/");
			if(path.ends_with(".remap") || path.ends_with(".import")) {
				file_list.push_back(path);
			}
		}

		uint64_t ofs = file_base + f->get_64();
		uint64_t size = f->get_64();
		uint8_t md5[16];
		f->get_buffer(md5, 16);
		uint32_t flags = f->get_32();

		PackedData::get_singleton()->add_path(p_path, path, ofs + p_offset, size, md5, this, p_replace_files, (flags & PACK_FILE_ENCRYPTED));
	}

	_fix_remap_and_import_files(&file_list, p_key);

	return true;
}

void SpikePackSource::_fix_remap_and_import_files(List<String> *files, const String &p_key) {
	for (const String &fp : *files) {
		Ref<ConfigFile> config;
		config.instantiate();
		if(config->load(fp) == OK){
			if (config->has_section_key("remap", "path")) {
				String p = config->get_value("remap", "path");
				p = p.insert(RES_HEAD_INDEX, p_key + "/");
				config->set_value("remap", "path", p);
			}
			if (config->has_section_key("deps", "source_file")) {
				String source_file = config->get_value("deps", "source_file");
				source_file = source_file.insert(RES_HEAD_INDEX, p_key + "/");
				config->set_value("deps", "source_file", source_file);
			}
			if (config->has_section_key("deps", "dest_files")) {
				Vector<String> dest_files = config->get_value("deps", "dest_files");
				Vector<String> new_dest;
				for (int i = 0; i < dest_files.size(); i++) {
					new_dest.append(dest_files[i].insert(RES_HEAD_INDEX, p_key + "/"));
				}
				config->set_value("deps", "dest_files", new_dest);
			}

			FileAccessVirtual::register_file(fp, config->encode_to_text().to_ascii_buffer());
		}
	}
}

Error SpikePackSource::add_uids_from_cache(const String &p_key) {
	auto cache_file = ProjectSettings::get_singleton()->get_project_data_path().path_join("uid_cache.bin");
	cache_file = cache_file.insert(RES_HEAD_INDEX, p_key + "/");
	Ref<FileAccess> f = FileAccess::open(cache_file, FileAccess::READ);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}

	uint32_t entry_count = f->get_32();
	for (uint32_t i = 0; i < entry_count; i++) {
		int64_t id = f->get_64();
		int32_t len = f->get_32();
		CharString cs;
		cs.resize(len + 1);
		ERR_FAIL_COND_V(cs.size() != len + 1, ERR_FILE_CORRUPT); // out of memory
		cs[len] = 0;
		int32_t rl = f->get_buffer((uint8_t *)cs.ptrw(), len);
		ERR_FAIL_COND_V(rl != len, ERR_FILE_CORRUPT);

		auto path = String::utf8(cs.ptr());
		path = path.insert(RES_HEAD_INDEX, p_key + "/");
		ResourceUID::get_singleton()->add_id(id, path);
	}

	ResourceUID::get_singleton()->save_to_cache();
	return OK;
}

// Error SpikePackSource::print_cache() {
// 	Ref<FileAccess> f = FileAccess::open(ResourceUID::get_cache_file(), FileAccess::READ);
// 	if (f.is_null()) {
// 		return ERR_CANT_OPEN;
// 	}

// 	uint32_t entry_count = f->get_32();
// 	for (uint32_t i = 0; i < entry_count; i++) {
// 		int64_t id = f->get_64();
// 		int32_t len = f->get_32();
// 		CharString cs;
// 		cs.resize(len + 1);
// 		ERR_FAIL_COND_V(cs.size() != len + 1, ERR_FILE_CORRUPT); // out of memory
// 		cs[len] = 0;
// 		int32_t rl = f->get_buffer((uint8_t *)cs.ptrw(), len);
// 		ERR_FAIL_COND_V(rl != len, ERR_FILE_CORRUPT);

// 		print_line(String::utf8(cs.ptr()));
// 	}
// 	return OK;
// }

bool SpikePackSource::try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset) {
	return try_open_pack(p_path, "", p_replace_files, p_offset);
};

Ref<FileAccess> SpikePackSource::get_file(const String &p_path, PackedData::PackedFile *p_file) {
	return memnew(FileAccessPack(p_path, *p_file));
}

SpikePackSource *SpikePackSource::get_singleton() {
	if (instance == nullptr) {
		instance = memnew(SpikePackSource);
	}

	return instance;
}

SpikePackSource::SpikePackSource() {
	instance = this;
}

SpikePackSource::~SpikePackSource() {
}


static HashMap<String, Vector<uint8_t>> *files = nullptr;

void FileAccessVirtual::register_file(String p_path, Vector<uint8_t> p_data) {
	if (!files) {
		files = memnew((HashMap<String, Vector<uint8_t>>));
	}
	(*files)[p_path.md5_text()] = p_data;
}

bool FileAccessVirtual::has_virual_file(String p_path){
	return files && (files->find(p_path.md5_text()) != nullptr);
}

void FileAccessVirtual::cleanup() {
	if (!files) {
		return;
	}

	memdelete(files);
}

Ref<FileAccess> FileAccessVirtual::create() {
	return memnew(FileAccessVirtual);
}

bool FileAccessVirtual::file_exists(const String &p_path) {
	return files && (files->find(p_path.md5_text()) != nullptr);
}

Error FileAccessVirtual::open_custom(const uint8_t *p_data, uint64_t p_len) {
	data = (uint8_t *)p_data;
	length = p_len;
	pos = 0;
	return OK;
}

Error FileAccessVirtual::open_internal(const String &p_path, int p_mode_flags) {
	if(!files){
		return ERR_FILE_NOT_FOUND;
	}

	HashMap<String, Vector<uint8_t>>::Iterator E = files->find(p_path.md5_text());
	if(!E) {
		return ERR_FILE_NOT_FOUND;
	}

	data = E->value.ptrw();
	length = E->value.size();
	pos = 0;

	//print_line("Open Virtual File : " + p_path);
	return OK;
}

bool FileAccessVirtual::is_open() const {
	return data != nullptr;
}

void FileAccessVirtual::seek(uint64_t p_position) {
	ERR_FAIL_COND(!data);
	pos = p_position;
}

void FileAccessVirtual::seek_end(int64_t p_position) {
	ERR_FAIL_COND(!data);
	pos = length + p_position;
}

uint64_t FileAccessVirtual::get_position() const {
	ERR_FAIL_COND_V(!data, 0);
	return pos;
}

uint64_t FileAccessVirtual::get_length() const {
	ERR_FAIL_COND_V(!data, 0);
	return length;
}

bool FileAccessVirtual::eof_reached() const {
	return pos > length;
}

uint8_t FileAccessVirtual::get_8() const {
	uint8_t ret = 0;
	if (pos < length) {
		ret = data[pos];
	}
	++pos;

	return ret;
}

uint64_t FileAccessVirtual::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);
	ERR_FAIL_COND_V(!data, -1);

	uint64_t left = length - pos;
	uint64_t read = MIN(p_length, left);

	if (read < p_length) {
		WARN_PRINT("Reading less data than requested");
	}

	memcpy(p_dst, &data[pos], read);
	pos += p_length;

	return read;
}

Error FileAccessVirtual::get_error() const {
	return pos >= length ? ERR_FILE_EOF : OK;
}

void FileAccessVirtual::flush() {
	ERR_FAIL_COND(!data);
}

void FileAccessVirtual::store_8(uint8_t p_byte) {
	ERR_FAIL_COND(!data);
	ERR_FAIL_COND(pos >= length);
	data[pos++] = p_byte;
}

void FileAccessVirtual::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND(!p_src && p_length > 0);
	uint64_t left = length - pos;
	uint64_t write = MIN(p_length, left);
	if (write < p_length) {
		WARN_PRINT("Writing less data than requested");
	}

	memcpy(&data[pos], p_src, write);
	pos += p_length;
}
