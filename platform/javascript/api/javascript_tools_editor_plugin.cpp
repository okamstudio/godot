/**************************************************************************/
/*  javascript_tools_editor_plugin.cpp                                    */
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

#if defined(TOOLS_ENABLED) && defined(JAVASCRIPT_ENABLED)
#include "javascript_tools_editor_plugin.h"

#include "core/engine.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "core/project_settings.h"
#include "editor/editor_node.h"

#include <emscripten/emscripten.h>

// JavaScript functions defined in library_godot_editor_tools.js
extern "C" {
extern void godot_js_os_download_buffer(const uint8_t *p_buf, int p_buf_size, const char *p_name, const char *p_mime);
}

static void _javascript_editor_init_callback() {
	EditorNode::get_singleton()->add_editor_plugin(memnew(JavaScriptToolsEditorPlugin(EditorNode::get_singleton())));
}

void JavaScriptToolsEditorPlugin::initialize() {
	EditorNode::add_init_callback(_javascript_editor_init_callback);
}

JavaScriptToolsEditorPlugin::JavaScriptToolsEditorPlugin(EditorNode *p_editor) {
	Variant v;
	add_tool_menu_item("Download Project Source", this, "_download_zip", v);
}

void JavaScriptToolsEditorPlugin::_download_zip(Variant p_v) {
	if (!Engine::get_singleton() || !Engine::get_singleton()->is_editor_hint()) {
		ERR_PRINT("Downloading the project as a ZIP archive is only available in Editor mode.");
		return;
	}
	String resource_path = ProjectSettings::get_singleton()->get_resource_path();

	FileAccess *src_f;
	zlib_filefunc_def io = zipio_create_io_from_file(&src_f);

	// Name the downloded ZIP file to contain the project name and download date for easier organization.
	// Replace characters not allowed (or risky) in Windows file names with safe characters.
	// In the project name, all invalid characters become an empty string so that a name
	// like "Platformer 2: Godette's Revenge" becomes "platformer_2-_godette-s_revenge".
	const String project_name_safe =
			String(GLOBAL_GET("application/config/name")).to_lower().replace(" ", "_");
	const String datetime_safe =
			OS::get_singleton()->get_iso_date_time(true).replace("T", "_").rstrip("Z");
	const String output_name = OS::get_singleton()->get_safe_dir_name(vformat("%s_%s.zip", project_name_safe, datetime_safe));
	const String output_path = String("/tmp").plus_file(output_name);

	zipFile zip = zipOpen2(output_path.utf8().get_data(), APPEND_STATUS_CREATE, NULL, &io);
	const String base_path = resource_path.substr(0, resource_path.rfind("/")) + "/";
	_zip_recursive(resource_path, base_path, zip);
	zipClose(zip, NULL);
	FileAccess *f = FileAccess::open(output_path, FileAccess::READ);
	ERR_FAIL_COND_MSG(!f, "Unable to create ZIP file.");
	Vector<uint8_t> buf;
	buf.resize(f->get_len());
	f->get_buffer(buf.ptrw(), buf.size());
	godot_js_os_download_buffer(buf.ptr(), buf.size(), output_name.utf8().get_data(), "application/zip");

	f->close();
	memdelete(f);
	// Remove the temporary file since it was sent to the user's native filesystem as a download.
	DirAccess::remove_file_or_error(output_path);
}

void JavaScriptToolsEditorPlugin::_bind_methods() {
	ClassDB::bind_method("_download_zip", &JavaScriptToolsEditorPlugin::_download_zip);
}

void JavaScriptToolsEditorPlugin::_zip_file(String p_path, String p_base_path, zipFile p_zip) {
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	if (!f) {
		WARN_PRINT("Unable to open file for zipping: " + p_path);
		return;
	}
	Vector<uint8_t> data;
	uint64_t len = f->get_len();
	data.resize(len);
	f->get_buffer(data.ptrw(), len);
	f->close();
	memdelete(f);

	String path = p_path.replace_first(p_base_path, "");
	zipOpenNewFileInZip(p_zip,
			path.utf8().get_data(),
			NULL,
			NULL,
			0,
			NULL,
			0,
			NULL,
			Z_DEFLATED,
			Z_DEFAULT_COMPRESSION);
	zipWriteInFileInZip(p_zip, data.ptr(), data.size());
	zipCloseFileInZip(p_zip);
}

void JavaScriptToolsEditorPlugin::_zip_recursive(String p_path, String p_base_path, zipFile p_zip) {
	DirAccess *dir = DirAccess::open(p_path);
	if (!dir) {
		WARN_PRINT("Unable to open directory for zipping: " + p_path);
		return;
	}
	dir->list_dir_begin();
	String cur = dir->get_next();
	String project_data_dir_name = ProjectSettings::get_singleton()->get_project_data_dir_name();
	while (!cur.empty()) {
		String cs = p_path.plus_file(cur);
		if (cur == "." || cur == ".." || cur == project_data_dir_name) {
			// Skip
		} else if (dir->current_is_dir()) {
			String path = cs.replace_first(p_base_path, "") + "/";
			zipOpenNewFileInZip(p_zip,
					path.utf8().get_data(),
					NULL,
					NULL,
					0,
					NULL,
					0,
					NULL,
					Z_DEFLATED,
					Z_DEFAULT_COMPRESSION);
			zipCloseFileInZip(p_zip);
			_zip_recursive(cs, p_base_path, p_zip);
		} else {
			_zip_file(cs, p_base_path, p_zip);
		}
		cur = dir->get_next();
	}
}
#endif
