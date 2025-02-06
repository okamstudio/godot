/**************************************************************************/
/*  export_plugin.h                                                       */
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

#include "core/io/file_access.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export_platform_pc.h"
#include "scene/resources/image_texture.h"

class EditorExportPlatformLinuxBSD : public EditorExportPlatformPC {
	GDCLASS(EditorExportPlatformLinuxBSD, EditorExportPlatformPC);

	HashMap<String, String> extensions;

	struct SSHCleanupCommand {
		String host;
		String port;
		Vector<String> ssh_args;
		String cmd_args;
		bool wait = false;

		SSHCleanupCommand() {}
		SSHCleanupCommand(const String &p_host, const String &p_port, const Vector<String> &p_ssh_arg, const String &p_cmd_args, bool p_wait = false) {
			host = p_host;
			port = p_port;
			ssh_args = p_ssh_arg;
			cmd_args = p_cmd_args;
			wait = p_wait;
		}
	};

	Ref<ImageTexture> run_icon;
	Ref<ImageTexture> stop_icon;

	Vector<SSHCleanupCommand> cleanup_commands;
	OS::ProcessID ssh_pid = 0;
	int menu_options = 0;

	bool is_elf(const String &p_path) const;
	bool is_shebang(const String &p_path) const;

	Error _export_debug_script(const Ref<EditorExportPreset> &p_preset, const String &p_app_name, const String &p_pkg_name, const String &p_path);
	String _get_exe_arch(const String &p_path) const;

public:
	virtual void get_export_options(List<ExportOption> *r_options) const override;
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual bool get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const override;
	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	virtual String get_template_file_name(const String &p_target, const String &p_arch) const override;
	virtual Error fixup_embedded_pck(const String &p_path, int64_t p_embedded_start, int64_t p_embedded_size) override;
	virtual bool is_executable(const String &p_path) const override;

	virtual Ref<Texture2D> get_run_icon() const override;
	virtual bool poll_export() override;
	virtual Ref<ImageTexture> get_option_icon(int p_index) const override;
	virtual int get_options_count() const override;
	virtual String get_option_label(int p_index) const override;
	virtual String get_option_tooltip(int p_index) const override;
	virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) override;
	virtual void cleanup() override;

	EditorExportPlatformLinuxBSD();
};
