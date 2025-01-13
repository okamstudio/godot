/**************************************************************************/
/*  web_tools_editor_plugin.cpp                                           */
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

#include "web_tools_editor_plugin.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/time.h"
#include "editor/editor_node.h"

#include <emscripten/emscripten.h>

// Web functions defined in library_godot_editor_tools.js
extern "C" {
extern void godot_js_os_download_buffer(const uint8_t *p_buf, int p_buf_size, const char *p_name, const char *p_mime);
}

static void _web_editor_init_callback() {
	EditorNode::get_singleton()->add_editor_plugin(memnew(WebToolsEditorPlugin));
}

void WebToolsEditorPlugin::initialize() {
	EditorNode::add_init_callback(_web_editor_init_callback);
}

WebToolsEditorPlugin::WebToolsEditorPlugin() {
	add_tool_menu_item("Download Project Source", callable_mp(this, &WebToolsEditorPlugin::_download_zip));
}

void WebToolsEditorPlugin::_download_zip() {
	if (!Engine::get_singleton() || !Engine::get_singleton()->is_editor_hint()) {
		ERR_PRINT("Downloading the project as a ZIP archive is only available in Editor mode.");
		return;
	}
	String resource_path = ProjectSettings::get_singleton()->get_resource_path();

	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);

	// Name the downloaded ZIP file to contain the project name and download date for easier organization.
	// Replace characters not allowed (or risky) in Windows file names with safe characters.
	// In the project name, all invalid characters become an empty string so that a name
	// like "Platformer 2: Godette's Revenge" becomes "platformer_2-_godette-s_revenge".
	const String project_name = GLOBAL_GET("application/config/name");
	const String project_name_safe = project_name.to_lower().replace(" ", "_");
	const String datetime_safe =
			Time::get_singleton()->get_datetime_string_from_system(false, true).replace(" ", "_");
	const String output_name = OS::get_singleton()->get_safe_dir_name(vformat("%s_%s.zip", project_name_safe, datetime_safe));
	const String output_path = String("/tmp").path_join(output_name);

	zipFile zip = zipOpen2(output_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io);
	const String base_path = resource_path.substr(0, resource_path.rfind_char('/')) + "/";
	_zip_recursive(resource_path, base_path, zip);
	zipClose(zip, nullptr);
	{
		Ref<FileAccess> f = FileAccess::open(output_path, FileAccess::READ);
		ERR_FAIL_COND_MSG(f.is_null(), "Unable to create ZIP file.");
		Vector<uint8_t> buf;
		buf.resize(f->get_length());
		f->get_buffer(buf.ptrw(), buf.size());
		godot_js_os_download_buffer(buf.ptr(), buf.size(), output_name.utf8().get_data(), "application/zip");
	}

	// Remove the temporary file since it was sent to the user's native filesystem as a download.
	DirAccess::remove_file_or_error(output_path);
}

void WebToolsEditorPlugin::_zip_file(String p_path, String p_base_path, zipFile p_zip) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		WARN_PRINT("Unable to open file for zipping: " + p_path);
		return;
	}
	Vector<uint8_t> data;
	uint64_t len = f->get_length();
	data.resize(len);
	f->get_buffer(data.ptrw(), len);

	String path = p_path.replace_first(p_base_path, "");
	zipOpenNewFileInZip(p_zip,
			path.utf8().get_data(),
			nullptr,
			nullptr,
			0,
			nullptr,
			0,
			nullptr,
			Z_DEFLATED,
			Z_DEFAULT_COMPRESSION);
	zipWriteInFileInZip(p_zip, data.ptr(), data.size());
	zipCloseFileInZip(p_zip);
}

void WebToolsEditorPlugin::_zip_recursive(String p_path, String p_base_path, zipFile p_zip) {
	Ref<DirAccess> dir = DirAccess::open(p_path);
	if (dir.is_null()) {
		WARN_PRINT("Unable to open directory for zipping: " + p_path);
		return;
	}
	dir->list_dir_begin();
	String cur = dir->get_next();
	String project_data_dir_name = ProjectSettings::get_singleton()->get_project_data_dir_name();
	while (!cur.is_empty()) {
		String cs = p_path.path_join(cur);
		if (cur == "." || cur == ".." || cur == project_data_dir_name) {
			// Skip
		} else if (dir->current_is_dir()) {
			String path = cs.replace_first(p_base_path, "") + "/";
			zipOpenNewFileInZip(p_zip,
					path.utf8().get_data(),
					nullptr,
					nullptr,
					0,
					nullptr,
					0,
					nullptr,
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
