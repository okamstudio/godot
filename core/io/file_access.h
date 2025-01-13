/**************************************************************************/
/*  file_access.h                                                         */
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

#ifndef FILE_ACCESS_H
#define FILE_ACCESS_H

#include "core/io/compression.h"
#include "core/math/math_defs.h"
#include "core/object/ref_counted.h"
#include "core/os/memory.h"
#include "core/string/ustring.h"
#include "core/typedefs.h"

/**
 * Multi-Platform abstraction for accessing to files.
 */

class FileAccess : public RefCounted {
	GDCLASS(FileAccess, RefCounted);

public:
	enum AccessType : int32_t {
		ACCESS_RESOURCES,
		ACCESS_USERDATA,
		ACCESS_FILESYSTEM,
		ACCESS_PIPE,
		ACCESS_MAX
	};

	enum ModeFlags : int32_t {
		READ = 1,
		WRITE = 2,
		READ_WRITE = 3,
		WRITE_READ = 7,
	};

	enum UnixPermissionFlags : int32_t {
		UNIX_EXECUTE_OTHER = 0x001,
		UNIX_WRITE_OTHER = 0x002,
		UNIX_READ_OTHER = 0x004,
		UNIX_EXECUTE_GROUP = 0x008,
		UNIX_WRITE_GROUP = 0x010,
		UNIX_READ_GROUP = 0x020,
		UNIX_EXECUTE_OWNER = 0x040,
		UNIX_WRITE_OWNER = 0x080,
		UNIX_READ_OWNER = 0x100,
		UNIX_RESTRICTED_DELETE = 0x200,
		UNIX_SET_GROUP_ID = 0x400,
		UNIX_SET_USER_ID = 0x800,
	};

	enum CompressionMode : int32_t {
		COMPRESSION_FASTLZ = Compression::MODE_FASTLZ,
		COMPRESSION_DEFLATE = Compression::MODE_DEFLATE,
		COMPRESSION_ZSTD = Compression::MODE_ZSTD,
		COMPRESSION_GZIP = Compression::MODE_GZIP,
		COMPRESSION_BROTLI = Compression::MODE_BROTLI,
	};

	typedef void (*FileCloseFailNotify)(const String &);

	typedef Ref<FileAccess> (*CreateFunc)();
	bool big_endian = false;
	bool real_is_double = false;

	virtual BitField<UnixPermissionFlags> _get_unix_permissions(const String &p_file) = 0;
	virtual Error _set_unix_permissions(const String &p_file, BitField<UnixPermissionFlags> p_permissions) = 0;

	virtual bool _get_hidden_attribute(const String &p_file) = 0;
	virtual Error _set_hidden_attribute(const String &p_file, bool p_hidden) = 0;
	virtual bool _get_read_only_attribute(const String &p_file) = 0;
	virtual Error _set_read_only_attribute(const String &p_file, bool p_ro) = 0;

protected:
	static void _bind_methods();

	AccessType get_access_type() const;
	virtual String fix_path(const String &p_path) const;
	virtual Error open_internal(const String &p_path, int p_mode_flags) = 0; ///< open a file
	virtual uint64_t _get_modified_time(const String &p_file) = 0;
	virtual void _set_access_type(AccessType p_access);

	static FileCloseFailNotify close_fail_notify;

#ifndef DISABLE_DEPRECATED
	static Ref<FileAccess> _open_encrypted_bind_compat_98918(const String &p_path, ModeFlags p_mode_flags, const Vector<uint8_t> &p_key);

	void store_8_bind_compat_78289(uint8_t p_dest);
	void store_16_bind_compat_78289(uint16_t p_dest);
	void store_32_bind_compat_78289(uint32_t p_dest);
	void store_64_bind_compat_78289(uint64_t p_dest);
	void store_buffer_bind_compat_78289(const Vector<uint8_t> &p_buffer);
	void store_var_bind_compat_78289(const Variant &p_var, bool p_full_objects = false);
	void store_half_bind_compat_78289(float p_dest);
	void store_float_bind_compat_78289(float p_dest);
	void store_double_bind_compat_78289(double p_dest);
	void store_real_bind_compat_78289(real_t p_real);
	void store_string_bind_compat_78289(const String &p_string);
	void store_line_bind_compat_78289(const String &p_line);
	void store_csv_line_bind_compat_78289(const Vector<String> &p_values, const String &p_delim = ",");
	void store_pascal_string_bind_compat_78289(const String &p_string);

	static void _bind_compatibility_methods();
#endif

private:
	static bool backup_save;
	thread_local static Error last_file_open_error;

	AccessType _access_type = ACCESS_FILESYSTEM;
	static CreateFunc create_func[ACCESS_MAX]; /** default file access creation function for a platform */
	template <typename T>
	static Ref<FileAccess> _create_builtin() {
		return memnew(T);
	}

	static Ref<FileAccess> _open(const String &p_path, ModeFlags p_mode_flags);

	bool _is_temp_file = false;
	bool _temp_keep_after_use = false;
	String _temp_path;
	void _delete_temp();

	static Ref<FileAccess> _create_temp(int p_mode_flags, const String &p_prefix = "", const String &p_extension = "", bool p_keep = false);

public:
	static void set_file_close_fail_notify_callback(FileCloseFailNotify p_cbk) { close_fail_notify = p_cbk; }

	virtual bool is_open() const = 0; ///< true when file is open

	virtual String get_path() const { return ""; } /// returns the path for the current open file
	virtual String get_path_absolute() const { return ""; } /// returns the absolute path for the current open file

	virtual void seek(uint64_t p_position) = 0; ///< seek to a given position
	virtual void seek_end(int64_t p_position = 0) = 0; ///< seek from the end of file with negative offset
	virtual uint64_t get_position() const = 0; ///< get position in the file
	virtual uint64_t get_length() const = 0; ///< get size of the file

	virtual bool eof_reached() const = 0; ///< reading passed EOF

	virtual uint8_t get_8() const; ///< get a byte
	virtual uint16_t get_16() const; ///< get 16 bits uint
	virtual uint32_t get_32() const; ///< get 32 bits uint
	virtual uint64_t get_64() const; ///< get 64 bits uint

	virtual float get_half() const;
	virtual float get_float() const;
	virtual double get_double() const;
	virtual real_t get_real() const;

	Variant get_var(bool p_allow_objects = false) const;

	virtual uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const = 0; ///< get an array of bytes, needs to be overwritten by children.
	Vector<uint8_t> get_buffer(int64_t p_length) const;
	virtual String get_line() const;
	virtual String get_token() const;
	virtual Vector<String> get_csv_line(const String &p_delim = ",") const;
	String get_as_text(bool p_skip_cr = false) const;
	virtual String get_as_utf8_string(bool p_skip_cr = false) const;

	/**

	 * Use this for files WRITTEN in _big_ endian machines (ie, amiga/mac)
	 * It's not about the current CPU type but file formats.
	 * This flag gets reset to `false` (little endian) on each open.
	 */
	virtual void set_big_endian(bool p_big_endian) { big_endian = p_big_endian; }
	inline bool is_big_endian() const { return big_endian; }

	virtual Error get_error() const = 0; ///< get last error

	virtual Error resize(int64_t p_length) = 0;
	virtual void flush() = 0;
	virtual bool store_8(uint8_t p_dest); ///< store a byte
	virtual bool store_16(uint16_t p_dest); ///< store 16 bits uint
	virtual bool store_32(uint32_t p_dest); ///< store 32 bits uint
	virtual bool store_64(uint64_t p_dest); ///< store 64 bits uint

	virtual bool store_half(float p_dest);
	virtual bool store_float(float p_dest);
	virtual bool store_double(double p_dest);
	virtual bool store_real(real_t p_real);

	virtual bool store_string(const String &p_string);
	virtual bool store_line(const String &p_line);
	virtual bool store_csv_line(const Vector<String> &p_values, const String &p_delim = ",");

	virtual bool store_pascal_string(const String &p_string);
	virtual String get_pascal_string();

	virtual bool store_buffer(const uint8_t *p_src, uint64_t p_length) = 0; ///< store an array of bytes, needs to be overwritten by children.
	bool store_buffer(const Vector<uint8_t> &p_buffer);

	bool store_var(const Variant &p_var, bool p_full_objects = false);

	virtual void close() = 0;

	virtual bool file_exists(const String &p_name) = 0; ///< return true if a file exists

	virtual Error reopen(const String &p_path, int p_mode_flags); ///< does not change the AccessType

	static Ref<FileAccess> create(AccessType p_access); /// Create a file access (for the current platform) this is the only portable way of accessing files.
	static Ref<FileAccess> create_for_path(const String &p_path);
	static Ref<FileAccess> open(const String &p_path, int p_mode_flags, Error *r_error = nullptr); /// Create a file access (for the current platform) this is the only portable way of accessing files.
	static Ref<FileAccess> create_temp(int p_mode_flags, const String &p_prefix = "", const String &p_extension = "", bool p_keep = false, Error *r_error = nullptr);

	static Ref<FileAccess> open_encrypted(const String &p_path, ModeFlags p_mode_flags, const Vector<uint8_t> &p_key, const Vector<uint8_t> &p_iv = Vector<uint8_t>());
	static Ref<FileAccess> open_encrypted_pass(const String &p_path, ModeFlags p_mode_flags, const String &p_pass);
	static Ref<FileAccess> open_compressed(const String &p_path, ModeFlags p_mode_flags, CompressionMode p_compress_mode = COMPRESSION_FASTLZ);
	static Error get_open_error();

	static CreateFunc get_create_func(AccessType p_access);
	static bool exists(const String &p_name); ///< return true if a file exists
	static uint64_t get_modified_time(const String &p_file);
	static BitField<FileAccess::UnixPermissionFlags> get_unix_permissions(const String &p_file);
	static Error set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions);

	static bool get_hidden_attribute(const String &p_file);
	static Error set_hidden_attribute(const String &p_file, bool p_hidden);
	static bool get_read_only_attribute(const String &p_file);
	static Error set_read_only_attribute(const String &p_file, bool p_ro);

	static void set_backup_save(bool p_enable) { backup_save = p_enable; }
	static bool is_backup_save_enabled() { return backup_save; }

	static String get_md5(const String &p_file);
	static String get_sha256(const String &p_file);
	static String get_multiple_md5(const Vector<String> &p_file);

	static Vector<uint8_t> get_file_as_bytes(const String &p_path, Error *r_error = nullptr);
	static String get_file_as_string(const String &p_path, Error *r_error = nullptr);

	static PackedByteArray _get_file_as_bytes(const String &p_path) { return get_file_as_bytes(p_path, &last_file_open_error); }
	static String _get_file_as_string(const String &p_path) { return get_file_as_string(p_path, &last_file_open_error); }

	template <typename T>
	static void make_default(AccessType p_access) {
		create_func[p_access] = _create_builtin<T>;
	}

public:
	FileAccess() {}
	virtual ~FileAccess();
};

VARIANT_ENUM_CAST(FileAccess::CompressionMode);
VARIANT_ENUM_CAST(FileAccess::ModeFlags);
VARIANT_BITFIELD_CAST(FileAccess::UnixPermissionFlags);

#endif // FILE_ACCESS_H
