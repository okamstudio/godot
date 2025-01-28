/**************************************************************************/
/*  remote_filesystem_client.cpp                                          */
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

#include "remote_filesystem_client.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/stream_peer_tcp.h"
#include "core/string/string_builder.h"

#define FILESYSTEM_CACHE_VERSION 1
#define FILESYSTEM_PROTOCOL_VERSION 1
#define PASSWORD_LENGTH 32

#define FILES_SUBFOLDER "remote_filesystem_files"
#define FILES_CACHE_FILE "remote_filesystem.cache"

Vector<RemoteFilesystemClient::FileCache> RemoteFilesystemClient::_load_cache_file() {
	Ref<FileAccess> fa = FileAccess::open(cache_path.path_join(FILES_CACHE_FILE), FileAccess::READ);
	if (fa.is_null()) {
		return Vector<FileCache>(); // No cache, return empty
	}

	int version = fa->get_line().to_int();
	if (version != FILESYSTEM_CACHE_VERSION) {
		return Vector<FileCache>(); // Version mismatch, ignore everything.
	}

	String file_path = cache_path.path_join(FILES_SUBFOLDER);

	Vector<FileCache> file_cache;

	while (!fa->eof_reached()) {
		String l = fa->get_line();
		Vector<String> fields = l.split("::");
		if (fields.size() != 3) {
			break;
		}
		FileCache fc;
		fc.path = fields[0];
		fc.server_modified_time = fields[1].to_int();
		fc.modified_time = fields[2].to_int();

		String full_path = file_path.path_join(fc.path);
		if (!FileAccess::exists(full_path)) {
			continue; // File is gone.
		}

		if (FileAccess::get_modified_time(full_path) != fc.modified_time) {
			DirAccess::remove_absolute(full_path); // Take the chance to remove this file and assume we no longer have it.
			continue;
		}

		file_cache.push_back(fc);
	}

	return file_cache;
}

Error RemoteFilesystemClient::_store_file(const String &p_path, const LocalVector<uint8_t> &p_file, uint64_t &modified_time) {
	modified_time = 0;
	String full_path = cache_path.path_join(FILES_SUBFOLDER).path_join(p_path);
	String base_file_dir = full_path.get_base_dir();

	if (!validated_directories.has(base_file_dir)) {
		// Verify that path exists before writing file, but only verify once for performance.
		DirAccess::make_dir_recursive_absolute(base_file_dir);
		validated_directories.insert(base_file_dir);
	}

	Ref<FileAccess> f = FileAccess::open(full_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_FILE_CANT_OPEN, vformat("Unable to open file for writing to remote filesystem cache: '%s'.", p_path));
	f->store_buffer(p_file.ptr(), p_file.size());
	Error err = f->get_error();
	if (err) {
		return err;
	}
	f.unref(); // Unref to ensure file is not locked and modified time can be obtained.

	modified_time = FileAccess::get_modified_time(full_path);
	return OK;
}

Error RemoteFilesystemClient::_remove_file(const String &p_path) {
	return DirAccess::remove_absolute(cache_path.path_join(FILES_SUBFOLDER).path_join(p_path));
}
Error RemoteFilesystemClient::_store_cache_file(const Vector<FileCache> &p_cache) {
	String full_path = cache_path.path_join(FILES_CACHE_FILE);
	String base_file_dir = full_path.get_base_dir();
	Error err = DirAccess::make_dir_recursive_absolute(base_file_dir);
	ERR_FAIL_COND_V_MSG(err != OK && err != ERR_ALREADY_EXISTS, err, vformat("Unable to create base directory to store cache file: '%s'.", base_file_dir));

	Ref<FileAccess> f = FileAccess::open(full_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_FILE_CANT_OPEN, vformat("Unable to open the remote cache file for writing: '%s'.", full_path));
	f->store_line(itos(FILESYSTEM_CACHE_VERSION));
	for (int i = 0; i < p_cache.size(); i++) {
		String l = p_cache[i].path + "::" + itos(p_cache[i].server_modified_time) + "::" + itos(p_cache[i].modified_time);
		f->store_line(l);
	}
	return OK;
}

Error RemoteFilesystemClient::synchronize_with_server(const String &p_host, int p_port, const String &p_password, String &r_cache_path) {
	Error err = _synchronize_with_server(p_host, p_port, p_password, r_cache_path);
	// Ensure no memory is kept
	validated_directories.reset();
	cache_path = String();
	return err;
}

void RemoteFilesystemClient::_update_cache_path(String &r_cache_path) {
	r_cache_path = cache_path.path_join(FILES_SUBFOLDER);
}

Error RemoteFilesystemClient::_synchronize_with_server(const String &p_host, int p_port, const String &p_password, String &r_cache_path) {
	cache_path = r_cache_path;
	{
		Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		dir->change_dir(cache_path);
		cache_path = dir->get_current_dir();
	}

	Ref<StreamPeerTCP> tcp_client;
	tcp_client.instantiate();

	IPAddress ip = p_host.is_valid_ip_address() ? IPAddress(p_host) : IP::get_singleton()->resolve_hostname(p_host);
	ERR_FAIL_COND_V_MSG(!ip.is_valid(), ERR_INVALID_PARAMETER, vformat("Unable to resolve remote filesystem server hostname: '%s'.", p_host));
	print_verbose(vformat("Remote Filesystem: Connecting to host %s, port %d.", ip, p_port));
	Error err = tcp_client->connect_to_host(ip, p_port);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Unable to open connection to remote file server (%s, port %d) failed.", String(p_host), p_port));

	while (tcp_client->get_status() == StreamPeerTCP::STATUS_CONNECTING) {
		tcp_client->poll();
		OS::get_singleton()->delay_usec(100);
	}

	if (tcp_client->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
		ERR_FAIL_V_MSG(ERR_CANT_CONNECT, vformat("Connection to remote file server (%s, port %d) failed.", String(p_host), p_port));
	}

	// Connection OK, now send the current file state.
	print_verbose("Remote Filesystem: Connection OK.");

	// Header (GRFS) - Godot Remote File System
	print_verbose("Remote Filesystem: Sending header");
	tcp_client->put_u8('G');
	tcp_client->put_u8('R');
	tcp_client->put_u8('F');
	tcp_client->put_u8('S');
	// Protocol version
	tcp_client->put_32(FILESYSTEM_PROTOCOL_VERSION);
	print_verbose("Remote Filesystem: Sending password");
	uint8_t password[PASSWORD_LENGTH]; // Send fixed size password, since it's easier and safe to validate.
	for (int i = 0; i < PASSWORD_LENGTH; i++) {
		if (i < p_password.length()) {
			password[i] = p_password[i];
		} else {
			password[i] = 0;
		}
	}
	tcp_client->put_data(password, PASSWORD_LENGTH);
	print_verbose("Remote Filesystem: Tags.");
	Vector<String> tags;
	{
		tags.push_back(OS::get_singleton()->get_identifier());
		switch (OS::get_singleton()->get_preferred_texture_format()) {
			case OS::PREFERRED_TEXTURE_FORMAT_S3TC_BPTC: {
				tags.push_back("bptc");
				tags.push_back("s3tc");
			} break;
			case OS::PREFERRED_TEXTURE_FORMAT_ETC2_ASTC: {
				tags.push_back("etc2");
				tags.push_back("astc");
			} break;
		}
	}

	tcp_client->put_32(tags.size());
	for (int i = 0; i < tags.size(); i++) {
		tcp_client->put_utf8_string(tags[i]);
	}
	// Size of compressed list of files
	print_verbose("Remote Filesystem: Sending file list");

	Vector<FileCache> file_cache = _load_cache_file();

	// Encode file cache to send it via network.
	Vector<uint8_t> file_cache_buffer;
	if (file_cache.size()) {
		StringBuilder sbuild;
		for (int i = 0; i < file_cache.size(); i++) {
			sbuild.append(file_cache[i].path);
			sbuild.append("::");
			sbuild.append(itos(file_cache[i].server_modified_time));
			sbuild.append("\n");
		}
		String s = sbuild.as_string();
		CharString cs = s.utf8();
		file_cache_buffer.resize(Compression::get_max_compressed_buffer_size(cs.length(), Compression::MODE_ZSTD));
		int res_len = Compression::compress(file_cache_buffer.ptrw(), (const uint8_t *)cs.ptr(), cs.length(), Compression::MODE_ZSTD);
		file_cache_buffer.resize(res_len);

		tcp_client->put_32(cs.length()); // Size of buffer uncompressed
		tcp_client->put_32(file_cache_buffer.size()); // Size of buffer compressed
		tcp_client->put_data(file_cache_buffer.ptr(), file_cache_buffer.size()); // Buffer
	} else {
		tcp_client->put_32(0); // No file cache buffer
	}

	tcp_client->poll();
	ERR_FAIL_COND_V_MSG(tcp_client->get_status() != StreamPeerTCP::STATUS_CONNECTED, ERR_CONNECTION_ERROR, "Remote filesystem server disconnected after sending header.");

	uint32_t file_count = tcp_client->get_u32();

	ERR_FAIL_COND_V_MSG(tcp_client->get_status() != StreamPeerTCP::STATUS_CONNECTED, ERR_CONNECTION_ERROR, "Remote filesystem server disconnected while waiting for file list");

	LocalVector<uint8_t> file_buffer;

	Vector<FileCache> temp_file_cache;

	HashSet<String> files_processed;
	for (uint32_t i = 0; i < file_count; i++) {
		String file = tcp_client->get_utf8_string();
		ERR_FAIL_COND_V_MSG(file == String(), ERR_CONNECTION_ERROR, "Invalid file name received from remote filesystem.");
		uint64_t server_modified_time = tcp_client->get_u64();
		ERR_FAIL_COND_V_MSG(tcp_client->get_status() != StreamPeerTCP::STATUS_CONNECTED, ERR_CONNECTION_ERROR, "Remote filesystem server disconnected while waiting for file info.");

		FileCache fc;
		fc.path = file;
		fc.server_modified_time = server_modified_time;
		temp_file_cache.push_back(fc);

		files_processed.insert(file);
	}

	Vector<FileCache> new_file_cache;

	// Get the actual files. As a robustness measure, if the connection is interrupted here, any file not yet received will be considered removed.
	// Since the file changed anyway, this makes it the easiest way to keep robustness.

	bool server_disconnected = false;
	for (uint32_t i = 0; i < file_count; i++) {
		String file = temp_file_cache[i].path;

		if (temp_file_cache[i].server_modified_time == 0 || server_disconnected) {
			// File was removed, or server disconnected before transferring it. Since it's no longer valid, remove anyway.
			_remove_file(file);
			continue;
		}

		uint64_t file_size = tcp_client->get_u64();
		file_buffer.resize(file_size);

		err = tcp_client->get_data(file_buffer.ptr(), file_size);
		if (err != OK) {
			ERR_PRINT(vformat("Error retrieving file from remote filesystem: '%s'.", file));
			server_disconnected = true;
		}

		if (tcp_client->get_status() != StreamPeerTCP::STATUS_CONNECTED) {
			// Early disconnect, stop accepting files.
			server_disconnected = true;
		}

		if (server_disconnected) {
			// No more server, transfer is invalid, remove this file.
			_remove_file(file);
			continue;
		}

		uint64_t modified_time = 0;
		err = _store_file(file, file_buffer, modified_time);
		if (err != OK) {
			server_disconnected = true;
			continue;
		}
		FileCache fc = temp_file_cache[i];
		fc.modified_time = modified_time;
		new_file_cache.push_back(fc);
	}

	print_verbose("Remote Filesystem: Updating the cache file.");

	// Go through the list of local files read initially (file_cache) and see which ones are
	// unchanged (not sent again from the server).
	// These need to be re-saved in the new list (new_file_cache).

	for (int i = 0; i < file_cache.size(); i++) {
		if (files_processed.has(file_cache[i].path)) {
			continue; // This was either added or removed, so skip.
		}
		new_file_cache.push_back(file_cache[i]);
	}

	err = _store_cache_file(new_file_cache);
	ERR_FAIL_COND_V_MSG(err != OK, ERR_FILE_CANT_OPEN, "Error writing the remote filesystem file cache.");

	print_verbose("Remote Filesystem: Update success.");

	_update_cache_path(r_cache_path);
	return OK;
}
