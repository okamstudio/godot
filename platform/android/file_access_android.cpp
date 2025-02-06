/**************************************************************************/
/*  file_access_android.cpp                                               */
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

#include "file_access_android.h"

#include "core/string/print_string.h"
#include "thread_jandroid.h"

#include <android/asset_manager_jni.h>

AAssetManager *FileAccessAndroid::asset_manager = nullptr;
jobject FileAccessAndroid::j_asset_manager = nullptr;

String FileAccessAndroid::get_path() const {
	return path_src;
}

String FileAccessAndroid::get_path_absolute() const {
	return absolute_path;
}

Error FileAccessAndroid::open_internal(const String &p_path, int p_mode_flags) {
	_close();

	path_src = p_path;
	String path = fix_path(p_path).simplify_path();
	absolute_path = path;
	if (path.begins_with("/")) {
		path = path.substr(1);
	} else if (path.begins_with("res://")) {
		path = path.substr(6);
	}

	ERR_FAIL_COND_V(p_mode_flags & FileAccess::WRITE, ERR_UNAVAILABLE); //can't write on android..
	asset = AAssetManager_open(asset_manager, path.utf8().get_data(), AASSET_MODE_STREAMING);
	if (!asset) {
		return ERR_CANT_OPEN;
	}
	len = AAsset_getLength(asset);
	pos = 0;
	eof = false;

	return OK;
}

void FileAccessAndroid::_close() {
	if (!asset) {
		return;
	}
	AAsset_close(asset);
	asset = nullptr;
}

bool FileAccessAndroid::is_open() const {
	return asset != nullptr;
}

void FileAccessAndroid::seek(uint64_t p_position) {
	ERR_FAIL_NULL(asset);

	AAsset_seek(asset, p_position, SEEK_SET);
	pos = p_position;
	if (pos > len) {
		pos = len;
		eof = true;
	} else {
		eof = false;
	}
}

void FileAccessAndroid::seek_end(int64_t p_position) {
	ERR_FAIL_NULL(asset);
	AAsset_seek(asset, p_position, SEEK_END);
	pos = len + p_position;
}

uint64_t FileAccessAndroid::get_position() const {
	return pos;
}

uint64_t FileAccessAndroid::get_length() const {
	return len;
}

bool FileAccessAndroid::eof_reached() const {
	return eof;
}

uint64_t FileAccessAndroid::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);

	int r = AAsset_read(asset, p_dst, p_length);

	if (pos + p_length > len) {
		eof = true;
	}

	if (r >= 0) {
		pos += r;
		if (pos > len) {
			pos = len;
		}
	}

	return r;
}

Error FileAccessAndroid::get_error() const {
	return eof ? ERR_FILE_EOF : OK; // not sure what else it may happen
}

void FileAccessAndroid::flush() {
	ERR_FAIL();
}

bool FileAccessAndroid::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_V(false);
}

bool FileAccessAndroid::file_exists(const String &p_path) {
	String path = fix_path(p_path).simplify_path();
	if (path.begins_with("/")) {
		path = path.substr(1);
	} else if (path.begins_with("res://")) {
		path = path.substr(6);
	}

	AAsset *at = AAssetManager_open(asset_manager, path.utf8().get_data(), AASSET_MODE_STREAMING);

	if (!at) {
		return false;
	}

	AAsset_close(at);
	return true;
}

void FileAccessAndroid::close() {
	_close();
}

FileAccessAndroid::~FileAccessAndroid() {
	_close();
}

void FileAccessAndroid::setup(jobject p_asset_manager) {
	JNIEnv *env = get_jni_env();
	j_asset_manager = env->NewGlobalRef(p_asset_manager);
	asset_manager = AAssetManager_fromJava(env, j_asset_manager);
}

void FileAccessAndroid::terminate() {
	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL(env);

	env->DeleteGlobalRef(j_asset_manager);
}
