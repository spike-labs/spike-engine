/**
 * file_provider_pack.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "file_provider_pack.h"
#include "core/io/file_access.h"
#include "core/io/file_access_pack.h"
#include "core/version_generated.gen.h"
#include "filesystem_server/filesystem_server.h"
#include "filesystem_server/providers/file_provider_pack.h"

Ref<FileProviderPack> FileProviderPack::create(const String &p_path) {
	Ref<FileProviderPack> provider;
	provider.instantiate();
	provider->load_pack(p_path);
	return provider;
}

bool FileProviderPack::load_pack(const String &p_path) {
	ERR_FAIL_COND_V_MSG(!source.is_empty(), false, "Pack already loaded.");

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}

	bool pck_header_found = false;

	uint32_t magic = f->get_32();
	if (magic == PACK_HEADER_MAGIC) {
		pck_header_found = true;
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
	ERR_FAIL_COND_V_MSG(enc_directory, false, "Unsupport for encrypted pack directory.");

	for (int i = 0; i < 16; i++) {
		//reserved
		f->get_32();
	}

	int file_count = f->get_32();
	for (int i = 0; i < file_count; i++) {
		String path = f->get_pascal_string();

		uint64_t ofs = file_base + f->get_64();
		uint64_t size = f->get_64();
		uint8_t md5[16];
		f->get_buffer(md5, 16);
		uint32_t flags = f->get_32();

		add_path(p_path, path, ofs, size, md5, nullptr, false, (flags & PACK_FILE_ENCRYPTED));
		file_list.push_back(path);
	}

	source = p_path;

	return true;
}

Ref<FileAccess> FileProviderPack::open(const String &p_path, FileAccess::ModeFlags p_mode_flags, Error *r_error) const {
	auto packed_file = this->try_get_packed_file(p_path);

	if (r_error) {
		*r_error = packed_file ? OK : ERR_FILE_NOT_FOUND;
	}

	if (packed_file) {
		return memnew(FileAccessPack(p_path, *packed_file));
	}

	return Ref<FileAccess>();
}

void FileProviderPack::add_path(const String &p_pkg_path, const String &p_path, uint64_t p_ofs, uint64_t p_size, const uint8_t *p_md5, PackSource *p_src, bool p_replace_files, bool p_encrypted) {
	PathMD5 pmd5(p_path.md5_buffer());

	bool exists = files.has(pmd5);

	PackedData::PackedFile pf;
	pf.encrypted = p_encrypted;
	pf.pack = p_pkg_path;
	pf.offset = p_ofs;
	pf.size = p_size;
	for (int i = 0; i < 16; i++) {
		pf.md5[i] = p_md5[i];
	}
	pf.src = p_src;

	if (!exists || p_replace_files) {
		files[pmd5] = pf;
	}
}

const PackedData::PackedFile *FileProviderPack::try_get_packed_file(const String &p_path) const {
	PathMD5 pmd5(p_path.md5_buffer());
	auto E = files.find(pmd5);
	if (!E) {
		return nullptr; //not found
	}
	if (E->value.offset == 0) {
		return nullptr; //was erased
	}
	return &E->value;
}

PackedStringArray FileProviderPack::get_files() const {
	return PackedStringArray(file_list);
}

bool FileProviderPack::has_file(const String &p_path) const {
	return try_get_packed_file(p_path) != nullptr;
}

void FileProviderPack::on_added_to_filesystem_server() {
	// Global classes.
	// TODO:: check
	auto global_classes = get_project_environment()->get_global_classes();
	for (int i = 0; i < global_classes.size(); i++) {
		Dictionary c = global_classes[i];
		if (!c.has("class") || !c.has("language") || !c.has("path") || !c.has("base")) {
			continue;
		}
		ScriptServer::add_global_class(c["class"], c["base"], c["language"], c["path"]);
		DLog("Add global class [%s]: %s (%s)", c["language"], c["class"], c["base"]);
	}
}

void FileProviderPack::on_removed_to_filesystem_server() {
	// Global classes.
	// TODO:: check
	auto global_classes = get_project_environment()->get_global_classes();
	for (int i = 0; i < global_classes.size(); i++) {
		Dictionary c = global_classes[i];
		if (!c.has("class") || !c.has("language") || !c.has("path") || !c.has("base")) {
			continue;
		}
		ScriptServer::remove_global_class(c["class"]);
		DLog("Remove global class [%s]: %s (%s)", c["language"], c["class"], c["base"]);
	}
}

Ref<ProjectEnvironment> FileProviderPack::get_project_environment() {
	if (!project_environment.is_valid()) {
		project_environment.instantiate();
		project_environment->parse_provider(Ref<FileProviderPack>(this));
	}
	return project_environment;
}

void FileProviderPack::clear() {
	files.clear();
	file_list.clear();
	project_environment = Ref<ProjectEnvironment>();
	source = "";
}

void FileProviderPack::_bind_methods() {
	ClassDB::bind_static_method("FileProviderPack", D_METHOD("create", "path"), &FileProviderPack::create);
	ClassDB::bind_method(D_METHOD("load_pack", "path"), &FileProviderPack::load_pack);
	ClassDB::bind_method(D_METHOD("get_project_environment"), &FileProviderPack::get_project_environment);
	ClassDB::bind_method(D_METHOD("clear"), &FileProviderPack::clear);
}

FileProviderPack::FileProviderPack() {
}

FileProviderPack::~FileProviderPack() {
}