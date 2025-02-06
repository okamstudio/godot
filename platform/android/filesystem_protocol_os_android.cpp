/**************************************************************************/
/*  filesystem_protocol_os_android.cpp                                    */
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

#include "filesystem_protocol_os_android.h"
#include "core/io/filesystem.h"
#include "file_access_android.h"
#include <android/asset_manager_jni.h>

String FileSystemProtocolOSAndroid::fix_path(const String &p_path) {
	String r_path = FileSystem::fix_path(p_path);
	return r_path;
}

Ref<FileAccess> FileSystemProtocolOSAndroid::open_file(const String &p_path, int p_mode_flags, Error &r_error) const {
	Ref<FileAccessAndroid> file = Ref<FileAccessAndroid>();
	file.instantiate();

	r_error = file->open_internal(p_path, p_mode_flags);

	if (r_error != OK) {
		file.unref();
	}

	return file;
}

bool FileSystemProtocolOSAndroid::file_exists(const String &p_path) const {
	String path = fix_path(p_path).simplify_path();
	if (path.begins_with("/")) {
		path = path.substr(1, path.length());
	}

	AAsset *at = AAssetManager_open(FileAccessAndroid::asset_manager, path.utf8().get_data(), AASSET_MODE_STREAMING);

	if (!at) {
		return false;
	}

	AAsset_close(at);
	return true;
}
