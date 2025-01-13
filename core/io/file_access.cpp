/**************************************************************************/
/*  file_access.cpp                                                       */
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

#include "file_access.h"
#include "file_access.compat.inc"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/file_access_compressed.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/file_access_pack.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/os/time.h"

FileAccess::CreateFunc FileAccess::create_func[ACCESS_MAX] = {};

FileAccess::FileCloseFailNotify FileAccess::close_fail_notify = nullptr;

bool FileAccess::backup_save = false;
thread_local Error FileAccess::last_file_open_error = OK;

Ref<FileAccess> FileAccess::create(AccessType p_access) {
	ERR_FAIL_INDEX_V(p_access, ACCESS_MAX, nullptr);
	ERR_FAIL_NULL_V(create_func[p_access], nullptr);

	Ref<FileAccess> ret = create_func[p_access]();
	ret->_set_access_type(p_access);
	return ret;
}

bool FileAccess::exists(const String &p_name) {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && PackedData::get_singleton()->has_path(p_name)) {
		return true;
	}

	// Using file_exists because it's faster than trying to open the file.
	Ref<FileAccess> ret = create_for_path(p_name);
	return ret->file_exists(p_name);
}

void FileAccess::_set_access_type(AccessType p_access) {
	_access_type = p_access;
}

Ref<FileAccess> FileAccess::create_for_path(const String &p_path) {
	Ref<FileAccess> ret;
	if (p_path.begins_with("res://") || p_path.begins_with("uid://")) {
		ret = create(ACCESS_RESOURCES);
	} else if (p_path.begins_with("user://")) {
		ret = create(ACCESS_USERDATA);
	} else if (p_path.begins_with("pipe://")) {
		ret = create(ACCESS_PIPE);
	} else {
		ret = create(ACCESS_FILESYSTEM);
	}

	return ret;
}

Ref<FileAccess> FileAccess::create_temp(int p_mode_flags, const String &p_prefix, const String &p_extension, bool p_keep, Error *r_error) {
	const String ERROR_COMMON_PREFIX = "Error while creating temporary file";

	if (!p_prefix.is_valid_filename()) {
		*r_error = ERR_FILE_BAD_PATH;
		ERR_FAIL_V_MSG(Ref<FileAccess>(), vformat(R"(%s: "%s" is not a valid prefix.)", ERROR_COMMON_PREFIX, p_prefix));
	}

	if (!p_extension.is_valid_filename()) {
		*r_error = ERR_FILE_BAD_PATH;
		ERR_FAIL_V_MSG(Ref<FileAccess>(), vformat(R"(%s: "%s" is not a valid extension.)", ERROR_COMMON_PREFIX, p_extension));
	}

	const String TEMP_DIR = OS::get_singleton()->get_temp_path();
	String extension = p_extension.trim_prefix(".");

	uint32_t suffix_i = 0;
	String path;
	while (true) {
		String datetime = Time::get_singleton()->get_datetime_string_from_system().replace("-", "").replace("T", "").replace(":", "");
		datetime += itos(Time::get_singleton()->get_ticks_usec());
		String suffix = datetime + (suffix_i > 0 ? itos(suffix_i) : "");
		path = TEMP_DIR.path_join((p_prefix.is_empty() ? "" : p_prefix + "-") + suffix + (extension.is_empty() ? "" : "." + extension));
		if (!DirAccess::exists(path)) {
			break;
		}
		suffix_i += 1;
	}

	Error err;
	{
		// Create file first with WRITE mode.
		// Otherwise, it would fail to open with a READ mode.
		Ref<FileAccess> ret = FileAccess::open(path, FileAccess::ModeFlags::WRITE, &err);
		if (err != OK) {
			*r_error = err;
			ERR_FAIL_V_MSG(Ref<FileAccess>(), vformat(R"(%s: could not create "%s".)", ERROR_COMMON_PREFIX, path));
		}
		ret->flush();
	}

	// Open then the temp file with the correct mode flag.
	Ref<FileAccess> ret = FileAccess::open(path, p_mode_flags, &err);
	if (err != OK) {
		*r_error = err;
		ERR_FAIL_V_MSG(Ref<FileAccess>(), vformat(R"(%s: could not open "%s".)", ERROR_COMMON_PREFIX, path));
	}
	if (ret.is_valid()) {
		ret->_is_temp_file = true;
		ret->_temp_keep_after_use = p_keep;
		ret->_temp_path = ret->get_path_absolute();
	}

	*r_error = OK;
	return ret;
}

Ref<FileAccess> FileAccess::_create_temp(int p_mode_flags, const String &p_prefix, const String &p_extension, bool p_keep) {
	return create_temp(p_mode_flags, p_prefix, p_extension, p_keep, &last_file_open_error);
}

void FileAccess::_delete_temp() {
	if (!_is_temp_file || _temp_keep_after_use) {
		return;
	}

	if (!FileAccess::exists(_temp_path)) {
		return;
	}

	DirAccess::remove_absolute(_temp_path);
}

Error FileAccess::reopen(const String &p_path, int p_mode_flags) {
	return open_internal(p_path, p_mode_flags);
}

Ref<FileAccess> FileAccess::open(const String &p_path, int p_mode_flags, Error *r_error) {
	//try packed data first

	Ref<FileAccess> ret;
	if (!(p_mode_flags & WRITE) && PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled()) {
		ret = PackedData::get_singleton()->try_open_path(p_path);
		if (ret.is_valid()) {
			if (r_error) {
				*r_error = OK;
			}
			return ret;
		}
	}

	ret = create_for_path(p_path);
	Error err = ret->open_internal(p_path, p_mode_flags);

	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		ret.unref();
	}

	return ret;
}

Ref<FileAccess> FileAccess::_open(const String &p_path, ModeFlags p_mode_flags) {
	Error err = OK;
	Ref<FileAccess> fa = open(p_path, p_mode_flags, &err);
	last_file_open_error = err;
	if (err) {
		return Ref<FileAccess>();
	}
	return fa;
}

Ref<FileAccess> FileAccess::open_encrypted(const String &p_path, ModeFlags p_mode_flags, const Vector<uint8_t> &p_key, const Vector<uint8_t> &p_iv) {
	Ref<FileAccess> fa = _open(p_path, p_mode_flags);
	if (fa.is_null()) {
		return fa;
	}

	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	Error err = fae->open_and_parse(fa, p_key, (p_mode_flags == WRITE) ? FileAccessEncrypted::MODE_WRITE_AES256 : FileAccessEncrypted::MODE_READ, true, p_iv);
	last_file_open_error = err;
	if (err) {
		return Ref<FileAccess>();
	}
	return fae;
}

Ref<FileAccess> FileAccess::open_encrypted_pass(const String &p_path, ModeFlags p_mode_flags, const String &p_pass) {
	Ref<FileAccess> fa = _open(p_path, p_mode_flags);
	if (fa.is_null()) {
		return fa;
	}

	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	Error err = fae->open_and_parse_password(fa, p_pass, (p_mode_flags == WRITE) ? FileAccessEncrypted::MODE_WRITE_AES256 : FileAccessEncrypted::MODE_READ);
	last_file_open_error = err;
	if (err) {
		return Ref<FileAccess>();
	}
	return fae;
}

Ref<FileAccess> FileAccess::open_compressed(const String &p_path, ModeFlags p_mode_flags, CompressionMode p_compress_mode) {
	Ref<FileAccessCompressed> fac;
	fac.instantiate();
	fac->configure("GCPF", (Compression::Mode)p_compress_mode);
	Error err = fac->open_internal(p_path, p_mode_flags);
	last_file_open_error = err;
	if (err) {
		return Ref<FileAccess>();
	}

	return fac;
}

Error FileAccess::get_open_error() {
	return last_file_open_error;
}

FileAccess::CreateFunc FileAccess::get_create_func(AccessType p_access) {
	return create_func[p_access];
}

FileAccess::AccessType FileAccess::get_access_type() const {
	return _access_type;
}

String FileAccess::fix_path(const String &p_path) const {
	// Helper used by file accesses that use a single filesystem.

	String r_path = p_path.replace("\\", "/");

	switch (_access_type) {
		case ACCESS_RESOURCES: {
			if (ProjectSettings::get_singleton()) {
				if (r_path.begins_with("uid://")) {
					r_path = ResourceUID::uid_to_path(r_path);
				}

				if (r_path.begins_with("res://")) {
					String resource_path = ProjectSettings::get_singleton()->get_resource_path();
					if (!resource_path.is_empty()) {
						return r_path.replace("res:/", resource_path);
					}
					return r_path.replace("res://", "");
				}
			}

		} break;
		case ACCESS_USERDATA: {
			if (r_path.begins_with("user://")) {
				String data_dir = OS::get_singleton()->get_user_data_dir();
				if (!data_dir.is_empty()) {
					return r_path.replace("user:/", data_dir);
				}
				return r_path.replace("user://", "");
			}

		} break;
		case ACCESS_PIPE: {
			return r_path;
		} break;
		case ACCESS_FILESYSTEM: {
			return r_path;
		} break;
		case ACCESS_MAX:
			break; // Can't happen, but silences warning
	}

	return r_path;
}

/* these are all implemented for ease of porting, then can later be optimized */
uint8_t FileAccess::get_8() const {
	uint8_t data = 0;
	get_buffer(&data, sizeof(uint8_t));

	return data;
}

uint16_t FileAccess::get_16() const {
	uint16_t data = 0;
	get_buffer(reinterpret_cast<uint8_t *>(&data), sizeof(uint16_t));

	if (big_endian) {
		data = BSWAP16(data);
	}

	return data;
}

uint32_t FileAccess::get_32() const {
	uint32_t data = 0;
	get_buffer(reinterpret_cast<uint8_t *>(&data), sizeof(uint32_t));

	if (big_endian) {
		data = BSWAP32(data);
	}

	return data;
}

uint64_t FileAccess::get_64() const {
	uint64_t data = 0;
	get_buffer(reinterpret_cast<uint8_t *>(&data), sizeof(uint64_t));

	if (big_endian) {
		data = BSWAP64(data);
	}

	return data;
}

float FileAccess::get_half() const {
	return Math::half_to_float(get_16());
}

float FileAccess::get_float() const {
	MarshallFloat m;
	m.i = get_32();
	return m.f;
}

real_t FileAccess::get_real() const {
	if (real_is_double) {
		return get_double();
	} else {
		return get_float();
	}
}

Variant FileAccess::get_var(bool p_allow_objects) const {
	uint32_t len = get_32();
	Vector<uint8_t> buff = get_buffer(len);
	ERR_FAIL_COND_V((uint32_t)buff.size() != len, Variant());

	const uint8_t *r = buff.ptr();

	Variant v;
	Error err = decode_variant(v, &r[0], len, nullptr, p_allow_objects);
	ERR_FAIL_COND_V_MSG(err != OK, Variant(), "Error when trying to encode Variant.");

	return v;
}

double FileAccess::get_double() const {
	MarshallDouble m;
	m.l = get_64();
	return m.d;
}

String FileAccess::get_token() const {
	CharString token;

	char32_t c = get_8();

	while (!eof_reached()) {
		if (c <= ' ') {
			if (token.length()) {
				break;
			}
		} else {
			token += c;
		}
		c = get_8();
	}

	return String::utf8(token.get_data());
}

class CharBuffer {
	Vector<char> vector;
	char stack_buffer[256];

	char *buffer = nullptr;
	int capacity = 0;
	int written = 0;

	bool grow() {
		if (vector.resize(next_power_of_2(1 + written)) != OK) {
			return false;
		}

		if (buffer == stack_buffer) { // first chunk?

			for (int i = 0; i < written; i++) {
				vector.write[i] = stack_buffer[i];
			}
		}

		buffer = vector.ptrw();
		capacity = vector.size();
		ERR_FAIL_COND_V(written >= capacity, false);

		return true;
	}

public:
	_FORCE_INLINE_ CharBuffer() :
			buffer(stack_buffer),
			capacity(sizeof(stack_buffer) / sizeof(char)) {
	}

	_FORCE_INLINE_ void push_back(char c) {
		if (written >= capacity) {
			ERR_FAIL_COND(!grow());
		}

		buffer[written++] = c;
	}

	_FORCE_INLINE_ const char *get_data() const {
		return buffer;
	}
};

String FileAccess::get_line() const {
	CharBuffer line;

	char32_t c = get_8();

	while (!eof_reached()) {
		if (c == '\n' || c == '\0') {
			line.push_back(0);
			return String::utf8(line.get_data());
		} else if (c != '\r') {
			line.push_back(c);
		}

		c = get_8();
	}
	line.push_back(0);
	return String::utf8(line.get_data());
}

Vector<String> FileAccess::get_csv_line(const String &p_delim) const {
	ERR_FAIL_COND_V_MSG(p_delim.length() != 1, Vector<String>(), "Only single character delimiters are supported to parse CSV lines.");
	ERR_FAIL_COND_V_MSG(p_delim[0] == '"', Vector<String>(), "The double quotation mark character (\") is not supported as a delimiter for CSV lines.");

	String line;

	// CSV can support entries with line breaks as long as they are enclosed
	// in double quotes. So our "line" might be more than a single line in the
	// text file.
	int qc = 0;
	do {
		if (eof_reached()) {
			break;
		}
		line += get_line() + "\n";
		qc = 0;
		for (int i = 0; i < line.length(); i++) {
			if (line[i] == '"') {
				qc++;
			}
		}
	} while (qc % 2);

	// Remove the extraneous newline we've added above.
	line = line.substr(0, line.length() - 1);

	Vector<String> strings;

	bool in_quote = false;
	String current;
	for (int i = 0; i < line.length(); i++) {
		char32_t c = line[i];
		// A delimiter ends the current entry, unless it's in a quoted string.
		if (!in_quote && c == p_delim[0]) {
			strings.push_back(current);
			current = String();
		} else if (c == '"') {
			// Doubled quotes are escapes for intentional quotes in the string.
			if (line[i + 1] == '"' && in_quote) {
				current += '"';
				i++;
			} else {
				in_quote = !in_quote;
			}
		} else {
			current += c;
		}
	}

	if (in_quote) {
		WARN_PRINT(vformat("Reached end of file before closing '\"' in CSV file '%s'.", get_path()));
	}

	strings.push_back(current);

	return strings;
}

String FileAccess::get_as_text(bool p_skip_cr) const {
	uint64_t original_pos = get_position();
	const_cast<FileAccess *>(this)->seek(0);

	String text = get_as_utf8_string(p_skip_cr);

	const_cast<FileAccess *>(this)->seek(original_pos);

	return text;
}

Vector<uint8_t> FileAccess::get_buffer(int64_t p_length) const {
	Vector<uint8_t> data;

	ERR_FAIL_COND_V_MSG(p_length < 0, data, "Length of buffer cannot be smaller than 0.");
	if (p_length == 0) {
		return data;
	}

	Error err = data.resize(p_length);
	ERR_FAIL_COND_V_MSG(err != OK, data, vformat("Can't resize data to %d elements.", p_length));

	uint8_t *w = data.ptrw();
	int64_t len = get_buffer(w, p_length);

	if (len < p_length) {
		data.resize(len);
	}

	return data;
}

String FileAccess::get_as_utf8_string(bool p_skip_cr) const {
	Vector<uint8_t> sourcef;
	uint64_t len = get_length();
	sourcef.resize(len + 1);

	uint8_t *w = sourcef.ptrw();
	uint64_t r = get_buffer(w, len);
	ERR_FAIL_COND_V(r != len, String());
	w[len] = 0;

	String s;
	s.parse_utf8((const char *)w, len, p_skip_cr);
	return s;
}

bool FileAccess::store_8(uint8_t p_dest) {
	return store_buffer(&p_dest, sizeof(uint8_t));
}

bool FileAccess::store_16(uint16_t p_dest) {
	if (big_endian) {
		p_dest = BSWAP16(p_dest);
	}

	return store_buffer(reinterpret_cast<uint8_t *>(&p_dest), sizeof(uint16_t));
}

bool FileAccess::store_32(uint32_t p_dest) {
	if (big_endian) {
		p_dest = BSWAP32(p_dest);
	}

	return store_buffer(reinterpret_cast<uint8_t *>(&p_dest), sizeof(uint32_t));
}

bool FileAccess::store_64(uint64_t p_dest) {
	if (big_endian) {
		p_dest = BSWAP64(p_dest);
	}

	return store_buffer(reinterpret_cast<uint8_t *>(&p_dest), sizeof(uint64_t));
}

bool FileAccess::store_real(real_t p_real) {
	if constexpr (sizeof(real_t) == 4) {
		return store_float(p_real);
	} else {
		return store_double(p_real);
	}
}

bool FileAccess::store_half(float p_dest) {
	return store_16(Math::make_half_float(p_dest));
}

bool FileAccess::store_float(float p_dest) {
	MarshallFloat m;
	m.f = p_dest;
	return store_32(m.i);
}

bool FileAccess::store_double(double p_dest) {
	MarshallDouble m;
	m.d = p_dest;
	return store_64(m.l);
}

uint64_t FileAccess::get_modified_time(const String &p_file) {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_file) || PackedData::get_singleton()->has_directory(p_file))) {
		return 0;
	}

	Ref<FileAccess> fa = create_for_path(p_file);
	ERR_FAIL_COND_V_MSG(fa.is_null(), 0, vformat("Cannot create FileAccess for path '%s'.", p_file));

	uint64_t mt = fa->_get_modified_time(p_file);
	return mt;
}

BitField<FileAccess::UnixPermissionFlags> FileAccess::get_unix_permissions(const String &p_file) {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_file) || PackedData::get_singleton()->has_directory(p_file))) {
		return 0;
	}

	Ref<FileAccess> fa = create_for_path(p_file);
	ERR_FAIL_COND_V_MSG(fa.is_null(), 0, vformat("Cannot create FileAccess for path '%s'.", p_file));

	return fa->_get_unix_permissions(p_file);
}

Error FileAccess::set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_file) || PackedData::get_singleton()->has_directory(p_file))) {
		return ERR_UNAVAILABLE;
	}

	Ref<FileAccess> fa = create_for_path(p_file);
	ERR_FAIL_COND_V_MSG(fa.is_null(), ERR_CANT_CREATE, vformat("Cannot create FileAccess for path '%s'.", p_file));

	Error err = fa->_set_unix_permissions(p_file, p_permissions);
	return err;
}

bool FileAccess::get_hidden_attribute(const String &p_file) {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_file) || PackedData::get_singleton()->has_directory(p_file))) {
		return false;
	}

	Ref<FileAccess> fa = create_for_path(p_file);
	ERR_FAIL_COND_V_MSG(fa.is_null(), false, vformat("Cannot create FileAccess for path '%s'.", p_file));

	return fa->_get_hidden_attribute(p_file);
}

Error FileAccess::set_hidden_attribute(const String &p_file, bool p_hidden) {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_file) || PackedData::get_singleton()->has_directory(p_file))) {
		return ERR_UNAVAILABLE;
	}

	Ref<FileAccess> fa = create_for_path(p_file);
	ERR_FAIL_COND_V_MSG(fa.is_null(), ERR_CANT_CREATE, vformat("Cannot create FileAccess for path '%s'.", p_file));

	Error err = fa->_set_hidden_attribute(p_file, p_hidden);
	return err;
}

bool FileAccess::get_read_only_attribute(const String &p_file) {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_file) || PackedData::get_singleton()->has_directory(p_file))) {
		return false;
	}

	Ref<FileAccess> fa = create_for_path(p_file);
	ERR_FAIL_COND_V_MSG(fa.is_null(), false, vformat("Cannot create FileAccess for path '%s'.", p_file));

	return fa->_get_read_only_attribute(p_file);
}

Error FileAccess::set_read_only_attribute(const String &p_file, bool p_ro) {
	if (PackedData::get_singleton() && !PackedData::get_singleton()->is_disabled() && (PackedData::get_singleton()->has_path(p_file) || PackedData::get_singleton()->has_directory(p_file))) {
		return ERR_UNAVAILABLE;
	}

	Ref<FileAccess> fa = create_for_path(p_file);
	ERR_FAIL_COND_V_MSG(fa.is_null(), ERR_CANT_CREATE, vformat("Cannot create FileAccess for path '%s'.", p_file));

	Error err = fa->_set_read_only_attribute(p_file, p_ro);
	return err;
}

bool FileAccess::store_string(const String &p_string) {
	if (p_string.length() == 0) {
		return true;
	}

	CharString cs = p_string.utf8();
	return store_buffer((uint8_t *)&cs[0], cs.length());
}

bool FileAccess::store_pascal_string(const String &p_string) {
	CharString cs = p_string.utf8();
	return store_32(cs.length()) && store_buffer((uint8_t *)&cs[0], cs.length());
}

String FileAccess::get_pascal_string() {
	uint32_t sl = get_32();
	CharString cs;
	cs.resize(sl + 1);
	get_buffer((uint8_t *)cs.ptr(), sl);
	cs[sl] = 0;

	String ret;
	ret.parse_utf8(cs.ptr(), sl);
	return ret;
}

bool FileAccess::store_line(const String &p_line) {
	return store_string(p_line) && store_8('\n');
}

bool FileAccess::store_csv_line(const Vector<String> &p_values, const String &p_delim) {
	ERR_FAIL_COND_V(p_delim.length() != 1, false);

	String line = "";
	int size = p_values.size();
	for (int i = 0; i < size; ++i) {
		String value = p_values[i];

		if (value.contains_char('"') || value.contains(p_delim) || value.contains_char('\n')) {
			value = "\"" + value.replace("\"", "\"\"") + "\"";
		}
		if (i < size - 1) {
			value += p_delim;
		}

		line += value;
	}

	return store_line(line);
}

bool FileAccess::store_buffer(const Vector<uint8_t> &p_buffer) {
	uint64_t len = p_buffer.size();
	if (len == 0) {
		return true;
	}

	const uint8_t *r = p_buffer.ptr();

	return store_buffer(r, len);
}

bool FileAccess::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND_V(!p_src && p_length > 0, false);
	for (uint64_t i = 0; i < p_length; i++) {
		if (unlikely(!store_8(p_src[i]))) {
			return false;
		}
	}
	return true;
}

bool FileAccess::store_var(const Variant &p_var, bool p_full_objects) {
	int len;
	Error err = encode_variant(p_var, nullptr, len, p_full_objects);
	ERR_FAIL_COND_V_MSG(err != OK, false, "Error when trying to encode Variant.");

	Vector<uint8_t> buff;
	buff.resize(len);

	uint8_t *w = buff.ptrw();
	err = encode_variant(p_var, &w[0], len, p_full_objects);
	ERR_FAIL_COND_V_MSG(err != OK, false, "Error when trying to encode Variant.");

	return store_32(len) && store_buffer(buff);
}

Vector<uint8_t> FileAccess::get_file_as_bytes(const String &p_path, Error *r_error) {
	Ref<FileAccess> f = FileAccess::open(p_path, READ, r_error);
	if (f.is_null()) {
		if (r_error) { // if error requested, do not throw error
			return Vector<uint8_t>();
		}
		ERR_FAIL_V_MSG(Vector<uint8_t>(), vformat("Can't open file from path '%s'.", String(p_path)));
	}
	Vector<uint8_t> data;
	data.resize(f->get_length());
	f->get_buffer(data.ptrw(), data.size());
	return data;
}

String FileAccess::get_file_as_string(const String &p_path, Error *r_error) {
	Error err;
	Vector<uint8_t> array = get_file_as_bytes(p_path, &err);
	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		if (r_error) {
			return String();
		}
		ERR_FAIL_V_MSG(String(), vformat("Can't get file as string from path '%s'.", String(p_path)));
	}

	String ret;
	ret.parse_utf8((const char *)array.ptr(), array.size());
	return ret;
}

String FileAccess::get_md5(const String &p_file) {
	Ref<FileAccess> f = FileAccess::open(p_file, READ);
	if (f.is_null()) {
		return String();
	}

	CryptoCore::MD5Context ctx;
	ctx.start();

	unsigned char step[32768];

	while (true) {
		uint64_t br = f->get_buffer(step, 32768);
		if (br > 0) {
			ctx.update(step, br);
		}
		if (br < 4096) {
			break;
		}
	}

	unsigned char hash[16];
	ctx.finish(hash);

	return String::md5(hash);
}

String FileAccess::get_multiple_md5(const Vector<String> &p_file) {
	CryptoCore::MD5Context ctx;
	ctx.start();

	for (int i = 0; i < p_file.size(); i++) {
		Ref<FileAccess> f = FileAccess::open(p_file[i], READ);
		ERR_CONTINUE(f.is_null());

		unsigned char step[32768];

		while (true) {
			uint64_t br = f->get_buffer(step, 32768);
			if (br > 0) {
				ctx.update(step, br);
			}
			if (br < 4096) {
				break;
			}
		}
	}

	unsigned char hash[16];
	ctx.finish(hash);

	return String::md5(hash);
}

String FileAccess::get_sha256(const String &p_file) {
	Ref<FileAccess> f = FileAccess::open(p_file, READ);
	if (f.is_null()) {
		return String();
	}

	CryptoCore::SHA256Context ctx;
	ctx.start();

	unsigned char step[32768];

	while (true) {
		uint64_t br = f->get_buffer(step, 32768);
		if (br > 0) {
			ctx.update(step, br);
		}
		if (br < 4096) {
			break;
		}
	}

	unsigned char hash[32];
	ctx.finish(hash);

	return String::hex_encode_buffer(hash, 32);
}

void FileAccess::_bind_methods() {
	ClassDB::bind_static_method("FileAccess", D_METHOD("open", "path", "flags"), &FileAccess::_open);
	ClassDB::bind_static_method("FileAccess", D_METHOD("open_encrypted", "path", "mode_flags", "key", "iv"), &FileAccess::open_encrypted, DEFVAL(Vector<uint8_t>()));
	ClassDB::bind_static_method("FileAccess", D_METHOD("open_encrypted_with_pass", "path", "mode_flags", "pass"), &FileAccess::open_encrypted_pass);
	ClassDB::bind_static_method("FileAccess", D_METHOD("open_compressed", "path", "mode_flags", "compression_mode"), &FileAccess::open_compressed, DEFVAL(0));
	ClassDB::bind_static_method("FileAccess", D_METHOD("get_open_error"), &FileAccess::get_open_error);
	ClassDB::bind_static_method("FileAccess", D_METHOD("create_temp", "mode_flags", "prefix", "extension", "keep"), &FileAccess::_create_temp, DEFVAL(""), DEFVAL(""), DEFVAL(false));

	ClassDB::bind_static_method("FileAccess", D_METHOD("get_file_as_bytes", "path"), &FileAccess::_get_file_as_bytes);
	ClassDB::bind_static_method("FileAccess", D_METHOD("get_file_as_string", "path"), &FileAccess::_get_file_as_string);

	ClassDB::bind_method(D_METHOD("resize", "length"), &FileAccess::resize);
	ClassDB::bind_method(D_METHOD("flush"), &FileAccess::flush);
	ClassDB::bind_method(D_METHOD("get_path"), &FileAccess::get_path);
	ClassDB::bind_method(D_METHOD("get_path_absolute"), &FileAccess::get_path_absolute);
	ClassDB::bind_method(D_METHOD("is_open"), &FileAccess::is_open);
	ClassDB::bind_method(D_METHOD("seek", "position"), &FileAccess::seek);
	ClassDB::bind_method(D_METHOD("seek_end", "position"), &FileAccess::seek_end, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_position"), &FileAccess::get_position);
	ClassDB::bind_method(D_METHOD("get_length"), &FileAccess::get_length);
	ClassDB::bind_method(D_METHOD("eof_reached"), &FileAccess::eof_reached);
	ClassDB::bind_method(D_METHOD("get_8"), &FileAccess::get_8);
	ClassDB::bind_method(D_METHOD("get_16"), &FileAccess::get_16);
	ClassDB::bind_method(D_METHOD("get_32"), &FileAccess::get_32);
	ClassDB::bind_method(D_METHOD("get_64"), &FileAccess::get_64);
	ClassDB::bind_method(D_METHOD("get_half"), &FileAccess::get_half);
	ClassDB::bind_method(D_METHOD("get_float"), &FileAccess::get_float);
	ClassDB::bind_method(D_METHOD("get_double"), &FileAccess::get_double);
	ClassDB::bind_method(D_METHOD("get_real"), &FileAccess::get_real);
	ClassDB::bind_method(D_METHOD("get_buffer", "length"), (Vector<uint8_t>(FileAccess::*)(int64_t) const) & FileAccess::get_buffer);
	ClassDB::bind_method(D_METHOD("get_line"), &FileAccess::get_line);
	ClassDB::bind_method(D_METHOD("get_csv_line", "delim"), &FileAccess::get_csv_line, DEFVAL(","));
	ClassDB::bind_method(D_METHOD("get_as_text", "skip_cr"), &FileAccess::get_as_text, DEFVAL(false));
	ClassDB::bind_static_method("FileAccess", D_METHOD("get_md5", "path"), &FileAccess::get_md5);
	ClassDB::bind_static_method("FileAccess", D_METHOD("get_sha256", "path"), &FileAccess::get_sha256);
	ClassDB::bind_method(D_METHOD("is_big_endian"), &FileAccess::is_big_endian);
	ClassDB::bind_method(D_METHOD("set_big_endian", "big_endian"), &FileAccess::set_big_endian);
	ClassDB::bind_method(D_METHOD("get_error"), &FileAccess::get_error);
	ClassDB::bind_method(D_METHOD("get_var", "allow_objects"), &FileAccess::get_var, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("store_8", "value"), &FileAccess::store_8);
	ClassDB::bind_method(D_METHOD("store_16", "value"), &FileAccess::store_16);
	ClassDB::bind_method(D_METHOD("store_32", "value"), &FileAccess::store_32);
	ClassDB::bind_method(D_METHOD("store_64", "value"), &FileAccess::store_64);
	ClassDB::bind_method(D_METHOD("store_half", "value"), &FileAccess::store_half);
	ClassDB::bind_method(D_METHOD("store_float", "value"), &FileAccess::store_float);
	ClassDB::bind_method(D_METHOD("store_double", "value"), &FileAccess::store_double);
	ClassDB::bind_method(D_METHOD("store_real", "value"), &FileAccess::store_real);
	ClassDB::bind_method(D_METHOD("store_buffer", "buffer"), (bool(FileAccess::*)(const Vector<uint8_t> &)) & FileAccess::store_buffer);
	ClassDB::bind_method(D_METHOD("store_line", "line"), &FileAccess::store_line);
	ClassDB::bind_method(D_METHOD("store_csv_line", "values", "delim"), &FileAccess::store_csv_line, DEFVAL(","));
	ClassDB::bind_method(D_METHOD("store_string", "string"), &FileAccess::store_string);
	ClassDB::bind_method(D_METHOD("store_var", "value", "full_objects"), &FileAccess::store_var, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("store_pascal_string", "string"), &FileAccess::store_pascal_string);
	ClassDB::bind_method(D_METHOD("get_pascal_string"), &FileAccess::get_pascal_string);

	ClassDB::bind_method(D_METHOD("close"), &FileAccess::close);

	ClassDB::bind_static_method("FileAccess", D_METHOD("file_exists", "path"), &FileAccess::exists);
	ClassDB::bind_static_method("FileAccess", D_METHOD("get_modified_time", "file"), &FileAccess::get_modified_time);

	ClassDB::bind_static_method("FileAccess", D_METHOD("get_unix_permissions", "file"), &FileAccess::get_unix_permissions);
	ClassDB::bind_static_method("FileAccess", D_METHOD("set_unix_permissions", "file", "permissions"), &FileAccess::set_unix_permissions);

	ClassDB::bind_static_method("FileAccess", D_METHOD("get_hidden_attribute", "file"), &FileAccess::get_hidden_attribute);
	ClassDB::bind_static_method("FileAccess", D_METHOD("set_hidden_attribute", "file", "hidden"), &FileAccess::set_hidden_attribute);
	ClassDB::bind_static_method("FileAccess", D_METHOD("set_read_only_attribute", "file", "ro"), &FileAccess::set_read_only_attribute);
	ClassDB::bind_static_method("FileAccess", D_METHOD("get_read_only_attribute", "file"), &FileAccess::get_read_only_attribute);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "big_endian"), "set_big_endian", "is_big_endian");

	BIND_ENUM_CONSTANT(READ);
	BIND_ENUM_CONSTANT(WRITE);
	BIND_ENUM_CONSTANT(READ_WRITE);
	BIND_ENUM_CONSTANT(WRITE_READ);

	BIND_ENUM_CONSTANT(COMPRESSION_FASTLZ);
	BIND_ENUM_CONSTANT(COMPRESSION_DEFLATE);
	BIND_ENUM_CONSTANT(COMPRESSION_ZSTD);
	BIND_ENUM_CONSTANT(COMPRESSION_GZIP);
	BIND_ENUM_CONSTANT(COMPRESSION_BROTLI);

	BIND_BITFIELD_FLAG(UNIX_READ_OWNER);
	BIND_BITFIELD_FLAG(UNIX_WRITE_OWNER);
	BIND_BITFIELD_FLAG(UNIX_EXECUTE_OWNER);
	BIND_BITFIELD_FLAG(UNIX_READ_GROUP);
	BIND_BITFIELD_FLAG(UNIX_WRITE_GROUP);
	BIND_BITFIELD_FLAG(UNIX_EXECUTE_GROUP);
	BIND_BITFIELD_FLAG(UNIX_READ_OTHER);
	BIND_BITFIELD_FLAG(UNIX_WRITE_OTHER);
	BIND_BITFIELD_FLAG(UNIX_EXECUTE_OTHER);
	BIND_BITFIELD_FLAG(UNIX_SET_USER_ID);
	BIND_BITFIELD_FLAG(UNIX_SET_GROUP_ID);
	BIND_BITFIELD_FLAG(UNIX_RESTRICTED_DELETE);
}

FileAccess::~FileAccess() {
	_delete_temp();
}
