/**************************************************************************/
/*  file_access_filesystem_jandroid.cpp                                   */
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

#include "file_access_filesystem_jandroid.h"

#include "thread_jandroid.h"

#include "core/os/os.h"
#include "core/templates/local_vector.h"

#include <unistd.h>

jobject FileAccessFilesystemJAndroid::file_access_handler = nullptr;
jclass FileAccessFilesystemJAndroid::cls;

jmethodID FileAccessFilesystemJAndroid::_file_open = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_get_size = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_seek = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_seek_end = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_read = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_tell = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_eof = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_set_eof = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_close = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_write = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_flush = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_exists = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_last_modified = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_last_accessed = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_resize = nullptr;
jmethodID FileAccessFilesystemJAndroid::_file_size = nullptr;

String FileAccessFilesystemJAndroid::get_path() const {
	return path_src;
}

String FileAccessFilesystemJAndroid::get_path_absolute() const {
	return absolute_path;
}

Error FileAccessFilesystemJAndroid::open_internal(const String &p_path, int p_mode_flags) {
	if (is_open()) {
		_close();
	}

	if (_file_open) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNCONFIGURED);

		String path = fix_path(p_path).simplify_path();
		jstring js = env->NewStringUTF(path.utf8().get_data());
		int res = env->CallIntMethod(file_access_handler, _file_open, js, p_mode_flags);
		env->DeleteLocalRef(js);

		if (res < 0) {
			// Errors are passed back as their negative value to differentiate from the positive file id.
			return static_cast<Error>(-res);
		}

		id = res;
		path_src = p_path;
		absolute_path = path;
		return OK;
	} else {
		return ERR_UNCONFIGURED;
	}
}

void FileAccessFilesystemJAndroid::_close() {
	if (!is_open()) {
		return;
	}

	if (_file_close) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		env->CallVoidMethod(file_access_handler, _file_close, id);
	}
	id = 0;
}

bool FileAccessFilesystemJAndroid::is_open() const {
	return id != 0;
}

void FileAccessFilesystemJAndroid::seek(uint64_t p_position) {
	if (_file_seek) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		ERR_FAIL_COND_MSG(!is_open(), "File must be opened before use.");
		env->CallVoidMethod(file_access_handler, _file_seek, id, p_position);
	}
}

void FileAccessFilesystemJAndroid::seek_end(int64_t p_position) {
	if (_file_seek_end) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		ERR_FAIL_COND_MSG(!is_open(), "File must be opened before use.");
		env->CallVoidMethod(file_access_handler, _file_seek_end, id, p_position);
	}
}

uint64_t FileAccessFilesystemJAndroid::get_position() const {
	if (_file_tell) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 0);
		ERR_FAIL_COND_V_MSG(!is_open(), 0, "File must be opened before use.");
		return env->CallLongMethod(file_access_handler, _file_tell, id);
	} else {
		return 0;
	}
}

uint64_t FileAccessFilesystemJAndroid::get_length() const {
	if (_file_get_size) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 0);
		ERR_FAIL_COND_V_MSG(!is_open(), 0, "File must be opened before use.");
		return env->CallLongMethod(file_access_handler, _file_get_size, id);
	} else {
		return 0;
	}
}

bool FileAccessFilesystemJAndroid::eof_reached() const {
	if (_file_eof) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);
		ERR_FAIL_COND_V_MSG(!is_open(), false, "File must be opened before use.");
		return env->CallBooleanMethod(file_access_handler, _file_eof, id);
	} else {
		return false;
	}
}

void FileAccessFilesystemJAndroid::_set_eof(bool eof) {
	if (_file_set_eof) {
		ERR_FAIL_COND_MSG(!is_open(), "File must be opened before use.");

		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		env->CallVoidMethod(file_access_handler, _file_set_eof, id, eof);
	}
}

String FileAccessFilesystemJAndroid::get_line() const {
	ERR_FAIL_COND_V_MSG(!is_open(), String(), "File must be opened before use.");

	const size_t buffer_size_limit = 2048;
	const uint64_t file_size = get_length();
	const uint64_t start_position = get_position();

	String result;
	LocalVector<uint8_t> line_buffer;
	size_t current_buffer_size = 0;
	uint64_t line_buffer_position = 0;

	while (true) {
		size_t line_buffer_size = MIN(buffer_size_limit, file_size - get_position());
		if (line_buffer_size <= 0) {
			const_cast<FileAccessFilesystemJAndroid *>(this)->_set_eof(true);
			break;
		}

		current_buffer_size += line_buffer_size;
		line_buffer.resize(current_buffer_size);

		uint64_t bytes_read = get_buffer(&line_buffer[line_buffer_position], current_buffer_size - line_buffer_position);
		if (bytes_read <= 0) {
			break;
		}

		for (; bytes_read > 0; line_buffer_position++, bytes_read--) {
			uint8_t elem = line_buffer[line_buffer_position];
			if (elem == '\n' || elem == '\0') {
				// Found the end of the line
				const_cast<FileAccessFilesystemJAndroid *>(this)->seek(start_position + line_buffer_position + 1);
				if (result.parse_utf8((const char *)line_buffer.ptr(), line_buffer_position, true)) {
					return String();
				}
				return result;
			}
		}
	}

	if (result.parse_utf8((const char *)line_buffer.ptr(), line_buffer_position, true)) {
		return String();
	}
	return result;
}

uint64_t FileAccessFilesystemJAndroid::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	if (_file_read) {
		ERR_FAIL_COND_V_MSG(!is_open(), 0, "File must be opened before use.");
		if (p_length == 0) {
			return 0;
		}

		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 0);

		jobject j_buffer = env->NewDirectByteBuffer(p_dst, p_length);
		int length = env->CallIntMethod(file_access_handler, _file_read, id, j_buffer);
		env->DeleteLocalRef(j_buffer);
		return length;
	} else {
		return 0;
	}
}

bool FileAccessFilesystemJAndroid::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	if (_file_write) {
		ERR_FAIL_COND_V_MSG(!is_open(), false, "File must be opened before use.");
		ERR_FAIL_COND_V(!p_src && p_length > 0, false);
		if (p_length == 0) {
			return true;
		}

		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);

		jobject j_buffer = env->NewDirectByteBuffer((void *)p_src, p_length);
		bool ok = env->CallBooleanMethod(file_access_handler, _file_write, id, j_buffer);
		env->DeleteLocalRef(j_buffer);
		return ok;
	} else {
		return false;
	}
}

Error FileAccessFilesystemJAndroid::get_error() const {
	if (eof_reached()) {
		return ERR_FILE_EOF;
	}
	return OK;
}

Error FileAccessFilesystemJAndroid::resize(int64_t p_length) {
	if (_file_resize) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, FAILED);
		ERR_FAIL_COND_V_MSG(!is_open(), FAILED, "File must be opened before use.");
		int res = env->CallIntMethod(file_access_handler, _file_resize, id, p_length);
		return static_cast<Error>(res);
	} else {
		return ERR_UNAVAILABLE;
	}
}

void FileAccessFilesystemJAndroid::flush() {
	if (_file_flush) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		ERR_FAIL_COND_MSG(!is_open(), "File must be opened before use.");
		env->CallVoidMethod(file_access_handler, _file_flush, id);
	}
}

bool FileAccessFilesystemJAndroid::file_exists(const String &p_path) {
	if (_file_exists) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);

		String path = fix_path(p_path).simplify_path();
		jstring js = env->NewStringUTF(path.utf8().get_data());
		bool result = env->CallBooleanMethod(file_access_handler, _file_exists, js);
		env->DeleteLocalRef(js);
		return result;
	} else {
		return false;
	}
}

uint64_t FileAccessFilesystemJAndroid::_get_modified_time(const String &p_file) {
	if (_file_last_modified) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 0);

		String path = fix_path(p_file).simplify_path();
		jstring js = env->NewStringUTF(path.utf8().get_data());
		uint64_t result = env->CallLongMethod(file_access_handler, _file_last_modified, js);
		env->DeleteLocalRef(js);
		return result;
	} else {
		return 0;
	}
}

uint64_t FileAccessFilesystemJAndroid::_get_access_time(const String &p_file) {
	if (_file_last_accessed) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 0);

		String path = fix_path(p_file).simplify_path();
		jstring js = env->NewStringUTF(path.utf8().get_data());
		uint64_t result = env->CallLongMethod(file_access_handler, _file_last_accessed, js);
		env->DeleteLocalRef(js);
		return result;
	} else {
		return 0;
	}
}

int64_t FileAccessFilesystemJAndroid::_get_size(const String &p_file) {
	if (_file_size) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, -1);

		String path = fix_path(p_file).simplify_path();
		jstring js = env->NewStringUTF(path.utf8().get_data());
		int64_t result = env->CallLongMethod(file_access_handler, _file_size, js);
		env->DeleteLocalRef(js);
		return result;
	} else {
		return -1;
	}
}

void FileAccessFilesystemJAndroid::setup(jobject p_file_access_handler) {
	JNIEnv *env = get_jni_env();
	file_access_handler = env->NewGlobalRef(p_file_access_handler);

	jclass c = env->GetObjectClass(file_access_handler);
	cls = (jclass)env->NewGlobalRef(c);

	_file_open = env->GetMethodID(cls, "fileOpen", "(Ljava/lang/String;I)I");
	_file_get_size = env->GetMethodID(cls, "fileGetSize", "(I)J");
	_file_tell = env->GetMethodID(cls, "fileGetPosition", "(I)J");
	_file_eof = env->GetMethodID(cls, "isFileEof", "(I)Z");
	_file_set_eof = env->GetMethodID(cls, "setFileEof", "(IZ)V");
	_file_seek = env->GetMethodID(cls, "fileSeek", "(IJ)V");
	_file_seek_end = env->GetMethodID(cls, "fileSeekFromEnd", "(IJ)V");
	_file_read = env->GetMethodID(cls, "fileRead", "(ILjava/nio/ByteBuffer;)I");
	_file_close = env->GetMethodID(cls, "fileClose", "(I)V");
	_file_write = env->GetMethodID(cls, "fileWrite", "(ILjava/nio/ByteBuffer;)Z");
	_file_flush = env->GetMethodID(cls, "fileFlush", "(I)V");
	_file_exists = env->GetMethodID(cls, "fileExists", "(Ljava/lang/String;)Z");
	_file_last_modified = env->GetMethodID(cls, "fileLastModified", "(Ljava/lang/String;)J");
	_file_last_accessed = env->GetMethodID(cls, "fileLastAccessed", "(Ljava/lang/String;)J");
	_file_resize = env->GetMethodID(cls, "fileResize", "(IJ)I");
	_file_size = env->GetMethodID(cls, "fileSize", "(Ljava/lang/String;)J");
}

void FileAccessFilesystemJAndroid::terminate() {
	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL(env);

	env->DeleteGlobalRef(cls);
	env->DeleteGlobalRef(file_access_handler);
}

void FileAccessFilesystemJAndroid::close() {
	if (is_open()) {
		_close();
	}
}

FileAccessFilesystemJAndroid::FileAccessFilesystemJAndroid() {
	id = 0;
}

FileAccessFilesystemJAndroid::~FileAccessFilesystemJAndroid() {
	if (is_open()) {
		_close();
	}
}
