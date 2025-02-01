/**************************************************************************/
/*  dir_access_jandroid.h                                                 */
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

#include "java_godot_lib_jni.h"

#include "core/io/dir_access.h"
#include "drivers/unix/dir_access_unix.h"

#include <stdio.h>

/// Android implementation of the DirAccess interface used to provide access to
/// ACCESS_FILESYSTEM and ACCESS_RESOURCES directory resources.
/// The implementation use jni in order to comply with Android filesystem
/// access restriction.
class DirAccessJAndroid : public DirAccessUnix {
	static jobject dir_access_handler;
	static jclass cls;

	static jmethodID _dir_open;
	static jmethodID _dir_next;
	static jmethodID _dir_close;
	static jmethodID _dir_is_dir;
	static jmethodID _dir_exists;
	static jmethodID _file_exists;
	static jmethodID _get_drive_count;
	static jmethodID _get_drive;
	static jmethodID _make_dir;
	static jmethodID _get_space_left;
	static jmethodID _rename;
	static jmethodID _remove;
	static jmethodID _current_is_hidden;

public:
	virtual Error list_dir_begin() override; ///< This starts dir listing
	virtual String get_next() override;
	virtual bool current_is_dir() const override;
	virtual bool current_is_hidden() const override;
	virtual void list_dir_end() override; ///<

	virtual int get_drive_count() override;
	virtual String get_drive(int p_drive) override;
	virtual String get_current_dir(bool p_include_drive = true) const override; ///< return current dir location

	virtual Error change_dir(String p_dir) override; ///< can be relative or absolute, return false on success

	virtual bool file_exists(String p_file) override;
	virtual bool dir_exists(String p_dir) override;

	virtual Error make_dir(String p_dir) override;
	virtual Error make_dir_recursive(const String &p_dir) override;

	virtual Error rename(String p_from, String p_to) override;
	virtual Error remove(String p_name) override;

	virtual bool is_link(String p_file) override { return false; }
	virtual String read_link(String p_file) override { return p_file; }
	virtual Error create_link(String p_source, String p_target) override { return ERR_UNAVAILABLE; }

	virtual uint64_t get_space_left() override;

	static void setup(jobject p_dir_access_handler);
	static void terminate();

	DirAccessJAndroid();
	~DirAccessJAndroid();

protected:
	String _get_root_string() const override;

private:
	int id = 0;

	int dir_open(String p_path);
	void dir_close(int p_id);
	String get_absolute_path(String p_path);
};
