/**************************************************************************/
/*  remote_filesystem_client.h                                            */
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

#ifndef REMOTE_FILESYSTEM_CLIENT_H
#define REMOTE_FILESYSTEM_CLIENT_H

#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"

class RemoteFilesystemClient {
	String cache_path;
	HashSet<String> validated_directories;

protected:
	String _get_cache_path() { return cache_path; }
	struct FileCache {
		String path; // Local path (as in "folder/to/file.png")
		uint64_t server_modified_time = 0; // MD5 checksum.
		uint64_t modified_time = 0;
	};
	virtual bool _is_configured() { return !cache_path.is_empty(); }
	// Can be re-implemented per platform. If so, feel free to ignore get_cache_path()
	virtual Vector<FileCache> _load_cache_file();
	virtual Error _store_file(const String &p_path, const LocalVector<uint8_t> &p_file, uint64_t &modified_time);
	virtual Error _remove_file(const String &p_path);
	virtual Error _store_cache_file(const Vector<FileCache> &p_cache);
	virtual Error _synchronize_with_server(const String &p_host, int p_port, const String &p_password, String &r_cache_path);

	virtual void _update_cache_path(String &r_cache_path);

public:
	Error synchronize_with_server(const String &p_host, int p_port, const String &p_password, String &r_cache_path);
	virtual ~RemoteFilesystemClient() {}
};

#endif // REMOTE_FILESYSTEM_CLIENT_H
