/**************************************************************************/
/*  file_access.compat.inc                                                */
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

#ifndef DISABLE_DEPRECATED

Ref<FileAccess> FileAccess::_open_encrypted_bind_compat_98918(const String &p_path, ModeFlags p_mode_flags, const Vector<uint8_t> &p_key) {
	return open_encrypted(p_path, p_mode_flags, p_key, Vector<uint8_t>());
}

void FileAccess::store_8_bind_compat_78289(uint8_t p_dest) {
	store_8(p_dest);
}

void FileAccess::store_16_bind_compat_78289(uint16_t p_dest) {
	store_16(p_dest);
}

void FileAccess::store_32_bind_compat_78289(uint32_t p_dest) {
	store_32(p_dest);
}

void FileAccess::store_64_bind_compat_78289(uint64_t p_dest) {
	store_64(p_dest);
}

void FileAccess::store_buffer_bind_compat_78289(const Vector<uint8_t> &p_buffer) {
	store_buffer(p_buffer);
}

void FileAccess::store_var_bind_compat_78289(const Variant &p_var, bool p_full_objects) {
	store_var(p_var, p_full_objects);
}

void FileAccess::store_half_bind_compat_78289(float p_dest) {
	store_half(p_dest);
}

void FileAccess::store_float_bind_compat_78289(float p_dest) {
	store_float(p_dest);
}

void FileAccess::store_double_bind_compat_78289(double p_dest) {
	store_double(p_dest);
}

void FileAccess::store_real_bind_compat_78289(real_t p_real) {
	store_real(p_real);
}

void FileAccess::store_string_bind_compat_78289(const String &p_string) {
	store_string(p_string);
}

void FileAccess::store_line_bind_compat_78289(const String &p_line) {
	store_line(p_line);
}

void FileAccess::store_csv_line_bind_compat_78289(const Vector<String> &p_values, const String &p_delim) {
	store_csv_line(p_values, p_delim);
}

void FileAccess::store_pascal_string_bind_compat_78289(const String &p_string) {
	store_pascal_string(p_string);
}

void FileAccess::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_static_method("FileAccess", D_METHOD("open_encrypted", "path", "mode_flags", "key"), &FileAccess::_open_encrypted_bind_compat_98918);

	ClassDB::bind_compatibility_method(D_METHOD("store_8", "value"), &FileAccess::store_8_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_16", "value"), &FileAccess::store_16_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_32", "value"), &FileAccess::store_32_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_64", "value"), &FileAccess::store_64_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_half", "value"), &FileAccess::store_half_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_float", "value"), &FileAccess::store_float_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_double", "value"), &FileAccess::store_double_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_real", "value"), &FileAccess::store_real_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_buffer", "buffer"), &FileAccess::store_buffer_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_line", "line"), &FileAccess::store_line_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_csv_line", "values", "delim"), &FileAccess::store_csv_line_bind_compat_78289, DEFVAL(","));
	ClassDB::bind_compatibility_method(D_METHOD("store_string", "string"), &FileAccess::store_string_bind_compat_78289);
	ClassDB::bind_compatibility_method(D_METHOD("store_var", "value", "full_objects"), &FileAccess::store_var_bind_compat_78289, DEFVAL(false));
	ClassDB::bind_compatibility_method(D_METHOD("store_pascal_string", "string"), &FileAccess::store_pascal_string_bind_compat_78289);
}

#endif
