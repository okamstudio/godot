/**************************************************************************/
/*  file_access_pack.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/string/print_string.h"
#include "core/templates/hash_set.h"
#include "core/templates/list.h"

// Godot's packed file magic header ("GDPC" in ASCII).
#define PACK_HEADER_MAGIC 0x43504447
// The current packed file format version number.
#define PACK_FORMAT_VERSION 2

enum PackFlags {
	PACK_DIR_ENCRYPTED = 1 << 0,
	PACK_REL_FILEBASE = 1 << 1,
};

enum PackFileFlags {
	PACK_FILE_ENCRYPTED = 1 << 0,
	PACK_FILE_REMOVAL = 1 << 1,
};

class PackSource;

class PackedData {
	friend class FileAccessPack;
	friend class DirAccessPack;
	friend class PackSource;

public:
	struct PackedFile {
		String pack;
		uint64_t offset; //if offset is ZERO, the file was ERASED
		uint64_t size;
		uint8_t md5[16];
		PackSource *src = nullptr;
		bool encrypted;
	};

private:
	struct PackedDir {
		PackedDir *parent = nullptr;
		String name;
		HashMap<String, PackedDir *> subdirs;
		HashSet<String> files;
	};

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

	HashMap<PathMD5, PackedFile, PathMD5> files;

	Vector<PackSource *> sources;

	PackedDir *root = nullptr;

	static PackedData *singleton;
	bool disabled = false;

	void _free_packed_dirs(PackedDir *p_dir);
	void _get_file_paths(PackedDir *p_dir, const String &p_parent_dir, HashSet<String> &r_paths) const;

public:
	void add_pack_source(PackSource *p_source);
	void add_path(const String &p_pkg_path, const String &p_path, uint64_t p_ofs, uint64_t p_size, const uint8_t *p_md5, PackSource *p_src, bool p_replace_files, bool p_encrypted = false); // for PackSource
	void remove_path(const String &p_path);
	uint8_t *get_file_hash(const String &p_path);
	HashSet<String> get_file_paths() const;

	void set_disabled(bool p_disabled) { disabled = p_disabled; }
	_FORCE_INLINE_ bool is_disabled() const { return disabled; }

	static PackedData *get_singleton() { return singleton; }
	Error add_pack(const String &p_path, bool p_replace_files, uint64_t p_offset);

	void clear();

	_FORCE_INLINE_ Ref<FileAccess> try_open_path(const String &p_path);
	_FORCE_INLINE_ bool has_path(const String &p_path);

	_FORCE_INLINE_ Ref<DirAccess> try_open_directory(const String &p_path);
	_FORCE_INLINE_ bool has_directory(const String &p_path);

	PackedData();
	~PackedData();
};

class PackSource {
public:
	virtual bool try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset) = 0;
	virtual Ref<FileAccess> get_file(const String &p_path, PackedData::PackedFile *p_file) = 0;
	virtual ~PackSource() {}
};

class PackedSourcePCK : public PackSource {
public:
	virtual bool try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset) override;
	virtual Ref<FileAccess> get_file(const String &p_path, PackedData::PackedFile *p_file) override;
};

class PackedSourceDirectory : public PackSource {
	void add_directory(const String &p_path, bool p_replace_files);

public:
	virtual bool try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset) override;
	virtual Ref<FileAccess> get_file(const String &p_path, PackedData::PackedFile *p_file) override;
};

class FileAccessPack : public FileAccess {
	PackedData::PackedFile pf;

	mutable uint64_t pos;
	mutable bool eof;
	uint64_t off;

	Ref<FileAccess> f;
	virtual Error open_internal(const String &p_path, int p_mode_flags) override;
	virtual uint64_t _get_modified_time(const String &p_file) override { return 0; }
	virtual BitField<FileAccess::UnixPermissionFlags> _get_unix_permissions(const String &p_file) override { return 0; }
	virtual Error _set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) override { return FAILED; }

	virtual bool _get_hidden_attribute(const String &p_file) override { return false; }
	virtual Error _set_hidden_attribute(const String &p_file, bool p_hidden) override { return ERR_UNAVAILABLE; }
	virtual bool _get_read_only_attribute(const String &p_file) override { return false; }
	virtual Error _set_read_only_attribute(const String &p_file, bool p_ro) override { return ERR_UNAVAILABLE; }

public:
	virtual bool is_open() const override;

	virtual void seek(uint64_t p_position) override;
	virtual void seek_end(int64_t p_position = 0) override;
	virtual uint64_t get_position() const override;
	virtual uint64_t get_length() const override;

	virtual bool eof_reached() const override;

	virtual uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;

	virtual void set_big_endian(bool p_big_endian) override;

	virtual Error get_error() const override;

	virtual Error resize(int64_t p_length) override { return ERR_UNAVAILABLE; }
	virtual void flush() override;
	virtual bool store_buffer(const uint8_t *p_src, uint64_t p_length) override;

	virtual bool file_exists(const String &p_name) override;

	virtual void close() override;

	FileAccessPack(const String &p_path, const PackedData::PackedFile &p_file);
};

Ref<FileAccess> PackedData::try_open_path(const String &p_path) {
	String simplified_path = p_path.simplify_path().trim_prefix("res://");
	PathMD5 pmd5(simplified_path.md5_buffer());
	HashMap<PathMD5, PackedFile, PathMD5>::Iterator E = files.find(pmd5);
	if (!E) {
		return nullptr; // Not found.
	}

	return E->value.src->get_file(p_path, &E->value);
}

bool PackedData::has_path(const String &p_path) {
	return files.has(PathMD5(p_path.simplify_path().trim_prefix("res://").md5_buffer()));
}

bool PackedData::has_directory(const String &p_path) {
	Ref<DirAccess> da = try_open_directory(p_path);
	if (da.is_valid()) {
		return true;
	} else {
		return false;
	}
}

class DirAccessPack : public DirAccess {
	PackedData::PackedDir *current;

	List<String> list_dirs;
	List<String> list_files;
	bool cdir = false;

	PackedData::PackedDir *_find_dir(const String &p_dir);

public:
	virtual Error list_dir_begin() override;
	virtual String get_next() override;
	virtual bool current_is_dir() const override;
	virtual bool current_is_hidden() const override;
	virtual void list_dir_end() override;

	virtual int get_drive_count() override;
	virtual String get_drive(int p_drive) override;

	virtual Error change_dir(String p_dir) override;
	virtual String get_current_dir(bool p_include_drive = true) const override;

	virtual bool file_exists(String p_file) override;
	virtual bool dir_exists(String p_dir) override;

	virtual Error make_dir(String p_dir) override;

	virtual Error rename(String p_from, String p_to) override;
	virtual Error remove(String p_name) override;

	uint64_t get_space_left() override;

	virtual bool is_link(String p_file) override { return false; }
	virtual String read_link(String p_file) override { return p_file; }
	virtual Error create_link(String p_source, String p_target) override { return FAILED; }

	virtual String get_filesystem_type() const override;

	DirAccessPack();
};

Ref<DirAccess> PackedData::try_open_directory(const String &p_path) {
	Ref<DirAccess> da = memnew(DirAccessPack());
	if (da->change_dir(p_path) != OK) {
		da = Ref<DirAccess>();
	}
	return da;
}
