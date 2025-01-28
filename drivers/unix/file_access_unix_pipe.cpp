/**************************************************************************/
/*  file_access_unix_pipe.cpp                                             */
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

#include "file_access_unix_pipe.h"

#if defined(UNIX_ENABLED)

#include "core/os/os.h"
#include "core/string/print_string.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

Error FileAccessUnixPipe::open_existing(int p_rfd, int p_wfd, bool p_blocking) {
	// Open pipe using handles created by pipe(fd) call in the OS.execute_with_pipe.
	_close();

	path_src = String();
	unlink_on_close = false;
	ERR_FAIL_COND_V_MSG(fd[0] >= 0 || fd[1] >= 0, ERR_ALREADY_IN_USE, "Pipe is already in use.");
	fd[0] = p_rfd;
	fd[1] = p_wfd;

	if (!p_blocking) {
		fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK);
		fcntl(fd[1], F_SETFL, fcntl(fd[1], F_GETFL) | O_NONBLOCK);
	}

	last_error = OK;
	return OK;
}

Error FileAccessUnixPipe::open_internal(const String &p_path, int p_mode_flags) {
	_close();

	path_src = p_path;
	ERR_FAIL_COND_V_MSG(fd[0] >= 0 || fd[1] >= 0, ERR_ALREADY_IN_USE, "Pipe is already in use.");

	path = String("/tmp/") + p_path.replace("pipe://", "").replace("/", "_");
	const CharString path_utf8 = path.utf8();

	struct stat st = {};
	int err = stat(path_utf8.get_data(), &st);
	if (err) {
		if (mkfifo(path_utf8.get_data(), 0600) != 0) {
			last_error = ERR_FILE_CANT_OPEN;
			return last_error;
		}
		unlink_on_close = true;
	} else {
		ERR_FAIL_COND_V_MSG(!S_ISFIFO(st.st_mode), ERR_ALREADY_IN_USE, "Pipe name is already used by file.");
	}

	int f = ::open(path_utf8.get_data(), O_RDWR | O_CLOEXEC | O_NONBLOCK);
	if (f < 0) {
		switch (errno) {
			case ENOENT: {
				last_error = ERR_FILE_NOT_FOUND;
			} break;
			default: {
				last_error = ERR_FILE_CANT_OPEN;
			} break;
		}
		return last_error;
	}

	// Set close on exec to avoid leaking it to subprocesses.
	fd[0] = f;
	fd[1] = f;

	last_error = OK;
	return OK;
}

void FileAccessUnixPipe::_close() {
	if (fd[0] < 0) {
		return;
	}

	if (fd[1] != fd[0]) {
		::close(fd[1]);
	}
	::close(fd[0]);
	fd[0] = -1;
	fd[1] = -1;

	if (unlink_on_close) {
		::unlink(path.utf8().ptr());
	}
	unlink_on_close = false;
}

bool FileAccessUnixPipe::is_open() const {
	return (fd[0] >= 0 || fd[1] >= 0);
}

String FileAccessUnixPipe::get_path() const {
	return path_src;
}

String FileAccessUnixPipe::get_path_absolute() const {
	return path_src;
}

uint64_t FileAccessUnixPipe::get_buffer(uint8_t *p_dst, uint64_t p_length) const {
	ERR_FAIL_COND_V_MSG(fd[0] < 0, -1, "Pipe must be opened before use.");
	ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);

	ssize_t read = ::read(fd[0], p_dst, p_length);
	if (read == -1) {
		last_error = ERR_FILE_CANT_READ;
		read = 0;
	} else if (read != (ssize_t)p_length) {
		last_error = ERR_FILE_CANT_READ;
	} else {
		last_error = OK;
	}
	return read;
}

Error FileAccessUnixPipe::get_error() const {
	return last_error;
}

bool FileAccessUnixPipe::store_buffer(const uint8_t *p_src, uint64_t p_length) {
	ERR_FAIL_COND_V_MSG(fd[1] < 0, false, "Pipe must be opened before use.");
	ERR_FAIL_COND_V(!p_src && p_length > 0, false);

	if (::write(fd[1], p_src, p_length) != (ssize_t)p_length) {
		last_error = ERR_FILE_CANT_WRITE;
		return false;
	} else {
		last_error = OK;
		return true;
	}
}

void FileAccessUnixPipe::close() {
	_close();
}

FileAccessUnixPipe::~FileAccessUnixPipe() {
	_close();
}

#endif // UNIX_ENABLED
