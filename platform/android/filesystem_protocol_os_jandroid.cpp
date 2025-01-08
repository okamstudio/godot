/**************************************************************************/
/*  filesystem_protocol_os_jandroid.cpp                                   */
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

#include "filesystem_protocol_os_jandroid.h"
#include "core/io/filesystem.h"
#include "file_access_filesystem_jandroid.h"
#include "thread_jandroid.h"

String FileSystemProtocolOSJAndroid::fix_path(const String &p_path) {
	String r_path = FileSystem::fix_path(p_path);
	return r_path;
}

Ref<FileAccess> FileSystemProtocolOSJAndroid::open_file(const String &p_path, int p_mode_flags, Error &r_error) const {
	Ref<FileAccessFilesystemJAndroid> file = Ref<FileAccessFilesystemJAndroid>();
	file.instantiate();

	r_error = file->open_internal(p_path, p_mode_flags);

	if (r_error != OK) {
		file.unref();
	}

	return file;
}

bool FileSystemProtocolOSJAndroid::file_exists(const String &p_path) const {
	if (FileAccessFilesystemJAndroid::_file_exists) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);

		String path = fix_path(p_path).simplify_path();
		jstring js = env->NewStringUTF(path.utf8().get_data());
		bool result = env->CallBooleanMethod(FileAccessFilesystemJAndroid::file_access_handler, FileAccessFilesystemJAndroid::_file_exists, js);
		env->DeleteLocalRef(js);
		return result;
	} else {
		return false;
	}
}

uint64_t FileSystemProtocolOSJAndroid::get_modified_time(const String &p_file) const {
	if (FileAccessFilesystemJAndroid::_file_last_modified) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);

		String path = fix_path(p_file).simplify_path();
		jstring js = env->NewStringUTF(path.utf8().get_data());
		uint64_t result = env->CallLongMethod(FileAccessFilesystemJAndroid::file_access_handler, FileAccessFilesystemJAndroid::_file_last_modified, js);
		env->DeleteLocalRef(js);
		return result;
	} else {
		return 0;
	}
}
