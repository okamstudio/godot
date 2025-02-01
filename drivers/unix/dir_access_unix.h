/**************************************************************************/
/*  dir_access_unix.h                                                     */
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

#if defined(UNIX_ENABLED)

#include "core/io/dir_access.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

class DirAccessUnix : public DirAccess {
	DIR *dir_stream = nullptr;

	bool _cisdir = false;
	bool _cishidden = false;

protected:
	String current_dir;
	virtual String fix_unicode_name(const char *p_name) const { return String::utf8(p_name); }
	virtual bool is_hidden(const String &p_name);

public:
	typedef void (*RemoveNotificationFunc)(const String &p_file);
	static RemoveNotificationFunc remove_notification_func;

	virtual Error list_dir_begin() override; ///< This starts dir listing
	virtual String get_next() override;
	virtual bool current_is_dir() const override;
	virtual bool current_is_hidden() const override;

	virtual void list_dir_end() override; ///<

	virtual int get_drive_count() override;
	virtual String get_drive(int p_drive) override;
	virtual int get_current_drive() override;
	virtual bool drives_are_shortcuts() override;

	virtual Error change_dir(String p_dir) override; ///< can be relative or absolute, return false on success
	virtual String get_current_dir(bool p_include_drive = true) const override; ///< return current dir location
	virtual Error make_dir(String p_dir) override;

	virtual bool file_exists(String p_file) override;
	virtual bool dir_exists(String p_dir) override;
	virtual bool is_readable(String p_dir) override;
	virtual bool is_writable(String p_dir) override;

	virtual uint64_t get_modified_time(String p_file);

	virtual Error rename(String p_path, String p_new_path) override;
	virtual Error remove(String p_path) override;

	virtual bool is_link(String p_file) override;
	virtual String read_link(String p_file) override;
	virtual Error create_link(String p_source, String p_target) override;

	virtual bool is_case_sensitive(const String &p_path) const override;

	virtual uint64_t get_space_left() override;

	virtual String get_filesystem_type() const override;

	DirAccessUnix();
	~DirAccessUnix();
};

#endif // UNIX_ENABLED
