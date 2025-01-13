/**************************************************************************/
/*  editor_file_server.cpp                                                */
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

#include "editor_file_server.h"

#include "../editor_settings.h"
#include "editor/editor_node.h"
#include "editor/export/editor_export_platform.h"

#define FILESYSTEM_PROTOCOL_VERSION 1
#define PASSWORD_LENGTH 32
#define MAX_FILE_BUFFER_SIZE 100 * 1024 * 1024 // 100mb max file buffer size (description of files to update, compressed).

static void _add_file(String f, const uint64_t &p_modified_time, HashMap<String, uint64_t> &files_to_send, HashMap<String, uint64_t> &cached_files) {
	f = f.replace_first("res://", ""); // remove res://
	const uint64_t *cached_mt = cached_files.getptr(f);
	if (cached_mt && *cached_mt == p_modified_time) {
		// File is good, skip it.
		cached_files.erase(f); // Erase to mark this file as existing. Remaining files not added to files_to_send will be considered erased here, so they need to be erased in the client too.
		return;
	}
	files_to_send.insert(f, p_modified_time);
}

void EditorFileServer::_scan_files_changed(EditorFileSystemDirectory *efd, const Vector<String> &p_tags, HashMap<String, uint64_t> &files_to_send, HashMap<String, uint64_t> &cached_files) {
	for (int i = 0; i < efd->get_file_count(); i++) {
		String f = efd->get_file_path(i);
		if (FileAccess::exists(f + ".import")) {
			// is imported, determine what to do
			// Todo the modified times of remapped files should most likely be kept in EditorFileSystem to speed this up in the future.
			Ref<ConfigFile> cf;
			cf.instantiate();
			Error err = cf->load(f + ".import");

			ERR_CONTINUE(err != OK);
			{
				uint64_t mt = FileAccess::get_modified_time(f + ".import");
				_add_file(f + ".import", mt, files_to_send, cached_files);
			}

			if (!cf->has_section("remap")) {
				continue;
			}

			List<String> remaps;
			cf->get_section_keys("remap", &remaps);

			for (const String &remap : remaps) {
				if (remap == "path") {
					String remapped_path = cf->get_value("remap", remap);
					uint64_t mt = FileAccess::get_modified_time(remapped_path);
					_add_file(remapped_path, mt, files_to_send, cached_files);
				} else if (remap.begins_with("path.")) {
					String feature = remap.get_slice(".", 1);
					if (p_tags.has(feature)) {
						String remapped_path = cf->get_value("remap", remap);
						uint64_t mt = FileAccess::get_modified_time(remapped_path);
						_add_file(remapped_path, mt, files_to_send, cached_files);
					}
				}
			}
		} else {
			uint64_t mt = efd->get_file_modified_time(i);
			_add_file(f, mt, files_to_send, cached_files);
		}
	}

	for (int i = 0; i < efd->get_subdir_count(); i++) {
		_scan_files_changed(efd->get_subdir(i), p_tags, files_to_send, cached_files);
	}
}

static void _add_custom_file(const String &f, HashMap<String, uint64_t> &files_to_send, HashMap<String, uint64_t> &cached_files) {
	if (!FileAccess::exists(f)) {
		return;
	}
	_add_file(f, FileAccess::get_modified_time(f), files_to_send, cached_files);
}

void EditorFileServer::poll() {
	if (!active) {
		return;
	}

	if (!server->is_connection_available()) {
		return;
	}

	Ref<StreamPeerTCP> tcp_peer = server->take_connection();
	ERR_FAIL_COND(tcp_peer.is_null());

	// Got a connection!
	EditorProgress pr("updating_remote_file_system", TTR("Updating assets on target device:"), 105);

	pr.step(TTR("Syncing headers"), 0, true);
	print_verbose("EFS: Connecting taken!");
	char header[4];
	Error err = tcp_peer->get_data((uint8_t *)&header, 4);
	ERR_FAIL_COND(err != OK);
	ERR_FAIL_COND(header[0] != 'G');
	ERR_FAIL_COND(header[1] != 'R');
	ERR_FAIL_COND(header[2] != 'F');
	ERR_FAIL_COND(header[3] != 'S');

	uint32_t protocol_version = tcp_peer->get_u32();
	ERR_FAIL_COND(protocol_version != FILESYSTEM_PROTOCOL_VERSION);

	char cpassword[PASSWORD_LENGTH + 1];
	err = tcp_peer->get_data((uint8_t *)cpassword, PASSWORD_LENGTH);
	cpassword[PASSWORD_LENGTH] = 0;
	ERR_FAIL_COND(err != OK);
	print_verbose("EFS: Got password: " + String(cpassword));
	ERR_FAIL_COND_MSG(password != cpassword, "Client disconnected because password mismatch.");

	uint32_t tag_count = tcp_peer->get_u32();
	print_verbose("EFS: Getting tags: " + itos(tag_count));

	ERR_FAIL_COND(tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED);
	Vector<String> tags;
	for (uint32_t i = 0; i < tag_count; i++) {
		String tag = tcp_peer->get_utf8_string();
		print_verbose("EFS: tag #" + itos(i) + ": " + tag);
		ERR_FAIL_COND(tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED);
		tags.push_back(tag);
	}

	uint32_t file_buffer_decompressed_size = tcp_peer->get_32();
	HashMap<String, uint64_t> cached_files;

	if (file_buffer_decompressed_size > 0) {
		pr.step(TTR("Getting remote file system"), 1, true);

		// Got files cached by client.
		uint32_t file_buffer_size = tcp_peer->get_32();
		print_verbose("EFS: Getting file buffer: compressed - " + String::humanize_size(file_buffer_size) + " decompressed: " + String::humanize_size(file_buffer_decompressed_size));

		ERR_FAIL_COND(tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED);
		ERR_FAIL_COND(file_buffer_size > MAX_FILE_BUFFER_SIZE);
		LocalVector<uint8_t> file_buffer;
		file_buffer.resize(file_buffer_size);
		LocalVector<uint8_t> file_buffer_decompressed;
		file_buffer_decompressed.resize(file_buffer_decompressed_size);

		err = tcp_peer->get_data(file_buffer.ptr(), file_buffer_size);

		pr.step(TTR("Decompressing remote file system"), 2, true);

		ERR_FAIL_COND(err != OK);
		// Decompress the text with all the files
		Compression::decompress(file_buffer_decompressed.ptr(), file_buffer_decompressed.size(), file_buffer.ptr(), file_buffer.size(), Compression::MODE_ZSTD);
		String files_text = String::utf8((const char *)file_buffer_decompressed.ptr(), file_buffer_decompressed.size());
		Vector<String> files = files_text.split("\n");

		print_verbose("EFS: Total cached files received: " + itos(files.size()));
		for (int i = 0; i < files.size(); i++) {
			if (files[i].get_slice_count("::") != 2) {
				continue;
			}
			String file = files[i].get_slice("::", 0);
			uint64_t modified_time = files[i].get_slice("::", 1).to_int();

			cached_files.insert(file, modified_time);
		}
	} else {
		// Client does not have any files stored.
	}

	pr.step(TTR("Scanning for local changes"), 3, true);

	print_verbose("EFS: Scanning changes:");

	HashMap<String, uint64_t> files_to_send;
	// Scan files to send.
	_scan_files_changed(EditorFileSystem::get_singleton()->get_filesystem(), tags, files_to_send, cached_files);
	// Add forced export files
	Vector<String> forced_export = EditorExportPlatform::get_forced_export_files();
	for (int i = 0; i < forced_export.size(); i++) {
		_add_custom_file(forced_export[i], files_to_send, cached_files);
	}

	_add_custom_file("res://project.godot", files_to_send, cached_files);
	// Check which files were removed and also add them
	for (KeyValue<String, uint64_t> K : cached_files) {
		if (!files_to_send.has(K.key)) {
			files_to_send.insert(K.key, 0); //0 means removed
		}
	}

	tcp_peer->put_32(files_to_send.size());

	print_verbose("EFS: Sending list of changed files.");
	pr.step(TTR("Sending list of changed files:"), 4, true);

	// Send list of changed files first, to ensure that if connecting breaks, the client is not found in a broken state.
	for (KeyValue<String, uint64_t> K : files_to_send) {
		tcp_peer->put_utf8_string(K.key);
		tcp_peer->put_64(K.value);
	}

	print_verbose("EFS: Sending " + itos(files_to_send.size()) + " files.");

	int idx = 0;
	for (KeyValue<String, uint64_t> K : files_to_send) {
		pr.step(TTR("Sending file:") + " " + K.key.get_file(), 5 + idx * 100 / files_to_send.size(), false);
		idx++;

		if (K.value == 0 || !FileAccess::exists("res://" + K.key)) { // File was removed
			continue;
		}

		Vector<uint8_t> array = FileAccess::_get_file_as_bytes("res://" + K.key);
		tcp_peer->put_64(array.size());
		tcp_peer->put_data(array.ptr(), array.size());
		ERR_FAIL_COND(tcp_peer->get_status() != StreamPeerTCP::STATUS_CONNECTED);
	}

	tcp_peer->put_data((const uint8_t *)"GEND", 4); // End marker.

	print_verbose("EFS: Done.");
}

void EditorFileServer::start() {
	if (active) {
		stop();
	}
	port = EDITOR_GET("filesystem/file_server/port");
	password = EDITOR_GET("filesystem/file_server/password");
	Error err = server->listen(port);
	ERR_FAIL_COND_MSG(err != OK, "EditorFileServer: Unable to listen on port " + itos(port));
	active = true;
}

bool EditorFileServer::is_active() const {
	return active;
}

void EditorFileServer::stop() {
	if (active) {
		server->stop();
		active = false;
	}
}

EditorFileServer::EditorFileServer() {
	server.instantiate();
}

EditorFileServer::~EditorFileServer() {
	stop();
}
