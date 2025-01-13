/**************************************************************************/
/*  export_plugin.cpp                                                     */
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

#include "export_plugin.h"

#include "logo_svg.gen.h"
#include "run_icon_svg.gen.h"

#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_string_names.h"
#include "editor/export/editor_export.h"
#include "editor/themes/editor_scale.h"

#include "modules/modules_enabled.gen.h" // For svg.
#ifdef MODULE_SVG_ENABLED
#include "modules/svg/image_loader_svg.h"
#endif

Error EditorExportPlatformLinuxBSD::_export_debug_script(const Ref<EditorExportPreset> &p_preset, const String &p_app_name, const String &p_pkg_name, const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Debug Script Export"), vformat(TTR("Could not open file \"%s\"."), p_path));
		return ERR_CANT_CREATE;
	}

	f->store_line("#!/bin/sh");
	f->store_line("echo -ne '\\033c\\033]0;" + p_app_name + "\\a'");
	f->store_line("base_path=\"$(dirname \"$(realpath \"$0\")\")\"");
	f->store_line("\"$base_path/" + p_pkg_name + "\" \"$@\"");

	return OK;
}

Error EditorExportPlatformLinuxBSD::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	String custom_debug = p_preset->get("custom_template/debug");
	String custom_release = p_preset->get("custom_template/release");
	String arch = p_preset->get("binary_format/architecture");

	String template_path = p_debug ? custom_debug : custom_release;
	template_path = template_path.strip_edges();
	if (!template_path.is_empty()) {
		String exe_arch = _get_exe_arch(template_path);
		if (arch != exe_arch) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), vformat(TTR("Mismatching custom export template executable architecture, found \"%s\", expected \"%s\"."), exe_arch, arch));
			return ERR_CANT_CREATE;
		}
	}

	bool export_as_zip = p_path.ends_with("zip");

	String pkg_name;
	if (String(GLOBAL_GET("application/config/name")) != "") {
		pkg_name = String(GLOBAL_GET("application/config/name"));
	} else {
		pkg_name = "Unnamed";
	}

	pkg_name = OS::get_singleton()->get_safe_dir_name(pkg_name);

	// Setup temp folder.
	String path = p_path;
	String tmp_dir_path = EditorPaths::get_singleton()->get_temp_dir().path_join(pkg_name);

	Ref<DirAccess> tmp_app_dir = DirAccess::create_for_path(tmp_dir_path);
	if (export_as_zip) {
		if (tmp_app_dir.is_null()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), vformat(TTR("Could not create and open the directory: \"%s\""), tmp_dir_path));
			return ERR_CANT_CREATE;
		}
		if (DirAccess::exists(tmp_dir_path)) {
			if (tmp_app_dir->change_dir(tmp_dir_path) == OK) {
				tmp_app_dir->erase_contents_recursive();
			}
		}
		tmp_app_dir->make_dir_recursive(tmp_dir_path);
		path = tmp_dir_path.path_join(p_path.get_file().get_basename());
	}

	// Export project.
	Error err = EditorExportPlatformPC::export_project(p_preset, p_debug, path, p_flags);
	if (err != OK) {
		// Message is supplied by the subroutine method.
		return err;
	}

	// Save console wrapper.
	int con_scr = p_preset->get("debug/export_console_wrapper");
	if ((con_scr == 1 && p_debug) || (con_scr == 2)) {
		String scr_path = path.get_basename() + ".sh";
		err = _export_debug_script(p_preset, pkg_name, path.get_file(), scr_path);
		FileAccess::set_unix_permissions(scr_path, 0755);
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Debug Console Export"), TTR("Could not create console wrapper."));
		}
	}

	// ZIP project.
	if (export_as_zip) {
		if (FileAccess::exists(p_path)) {
			OS::get_singleton()->move_to_trash(p_path);
		}

		Ref<FileAccess> io_fa_dst;
		zlib_filefunc_def io_dst = zipio_create_io(&io_fa_dst);
		zipFile zip = zipOpen2(p_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io_dst);

		zip_folder_recursive(zip, tmp_dir_path, "", pkg_name);

		zipClose(zip, nullptr);

		if (tmp_app_dir->change_dir(tmp_dir_path) == OK) {
			tmp_app_dir->erase_contents_recursive();
			tmp_app_dir->change_dir("..");
			tmp_app_dir->remove(pkg_name);
		}
	}

	return err;
}

String EditorExportPlatformLinuxBSD::get_template_file_name(const String &p_target, const String &p_arch) const {
	return "linux_" + p_target + "." + p_arch;
}

List<String> EditorExportPlatformLinuxBSD::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back(p_preset->get("binary_format/architecture"));
	list.push_back("zip");

	return list;
}

bool EditorExportPlatformLinuxBSD::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	if (p_preset == nullptr) {
		return true;
	}

	bool advanced_options_enabled = p_preset->are_advanced_options_enabled();

	// Hide SSH options.
	bool ssh = p_preset->get("ssh_remote_deploy/enabled");
	if (!ssh && p_option != "ssh_remote_deploy/enabled" && p_option.begins_with("ssh_remote_deploy/")) {
		return false;
	}
	if (p_option == "dotnet/embed_build_outputs") {
		return advanced_options_enabled;
	}
	return true;
}

void EditorExportPlatformLinuxBSD::get_export_options(List<ExportOption> *r_options) const {
	EditorExportPlatformPC::get_export_options(r_options);

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "binary_format/architecture", PROPERTY_HINT_ENUM, "x86_64,x86_32,arm64,arm32,rv64,ppc64,ppc32,loongarch64"), "x86_64"));

	String run_script = "#!/usr/bin/env bash\n"
						"export DISPLAY=:0\n"
						"unzip -o -q \"{temp_dir}/{archive_name}\" -d \"{temp_dir}\"\n"
						"\"{temp_dir}/{exe_name}\" {cmd_args}";

	String cleanup_script = "#!/usr/bin/env bash\n"
							"kill $(pgrep -x -f \"{temp_dir}/{exe_name} {cmd_args}\")\n"
							"rm -rf \"{temp_dir}\"";

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "ssh_remote_deploy/enabled"), false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/host"), "user@host_ip"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/port"), "22"));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/extra_args_ssh", PROPERTY_HINT_MULTILINE_TEXT), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/extra_args_scp", PROPERTY_HINT_MULTILINE_TEXT), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/run_script", PROPERTY_HINT_MULTILINE_TEXT), run_script));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/cleanup_script", PROPERTY_HINT_MULTILINE_TEXT), cleanup_script));
}

bool EditorExportPlatformLinuxBSD::is_elf(const String &p_path) const {
	Ref<FileAccess> fb = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(fb.is_null(), false, vformat("Can't open file: \"%s\".", p_path));
	uint32_t magic = fb->get_32();
	return (magic == 0x464c457f);
}

bool EditorExportPlatformLinuxBSD::is_shebang(const String &p_path) const {
	Ref<FileAccess> fb = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(fb.is_null(), false, vformat("Can't open file: \"%s\".", p_path));
	uint16_t magic = fb->get_16();
	return (magic == 0x2123);
}

bool EditorExportPlatformLinuxBSD::is_executable(const String &p_path) const {
	return is_elf(p_path) || is_shebang(p_path);
}

bool EditorExportPlatformLinuxBSD::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	String err;
	bool valid = EditorExportPlatformPC::has_valid_export_configuration(p_preset, err, r_missing_templates, p_debug);

	String custom_debug = p_preset->get("custom_template/debug").operator String().strip_edges();
	String custom_release = p_preset->get("custom_template/release").operator String().strip_edges();
	String arch = p_preset->get("binary_format/architecture");

	if (!custom_debug.is_empty() && FileAccess::exists(custom_debug)) {
		String exe_arch = _get_exe_arch(custom_debug);
		if (arch != exe_arch) {
			err += vformat(TTR("Mismatching custom debug export template executable architecture: found \"%s\", expected \"%s\"."), exe_arch, arch) + "\n";
		}
	}
	if (!custom_release.is_empty() && FileAccess::exists(custom_release)) {
		String exe_arch = _get_exe_arch(custom_release);
		if (arch != exe_arch) {
			err += vformat(TTR("Mismatching custom release export template executable architecture: found \"%s\", expected \"%s\"."), exe_arch, arch) + "\n";
		}
	}

	if (!err.is_empty()) {
		r_error = err;
	}

	return valid;
}

String EditorExportPlatformLinuxBSD::_get_exe_arch(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return "invalid";
	}

	// Read and check ELF magic number.
	{
		uint32_t magic = f->get_32();
		if (magic != 0x464c457f) { // 0x7F + "ELF"
			return "invalid";
		}
	}

	// Process header.
	int64_t header_pos = f->get_position();
	f->seek(header_pos + 14);
	uint16_t machine = f->get_16();
	f->close();

	switch (machine) {
		case 0x0003:
			return "x86_32";
		case 0x003e:
			return "x86_64";
		case 0x0014:
			return "ppc32";
		case 0x0015:
			return "ppc64";
		case 0x0028:
			return "arm32";
		case 0x00b7:
			return "arm64";
		case 0x00f3:
			return "rv64";
		case 0x0102:
			return "loongarch64";
		default:
			return "unknown";
	}
}

Error EditorExportPlatformLinuxBSD::fixup_embedded_pck(const String &p_path, int64_t p_embedded_start, int64_t p_embedded_size) {
	// Patch the header of the "pck" section in the ELF file so that it corresponds to the embedded data.

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ_WRITE);
	if (f.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), vformat(TTR("Failed to open executable file \"%s\"."), p_path));
		return ERR_CANT_OPEN;
	}

	// Read and check ELF magic number.
	{
		uint32_t magic = f->get_32();
		if (magic != 0x464c457f) { // 0x7F + "ELF"
			add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("Executable file header corrupted."));
			return ERR_FILE_CORRUPT;
		}
	}

	// Read program architecture bits from class field.

	int bits = f->get_8() * 32;

	if (bits == 32 && p_embedded_size >= 0x100000000) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("32-bit executables cannot have embedded data >= 4 GiB."));
	}

	// Get info about the section header table.

	int64_t section_table_pos;
	int64_t section_header_size;
	if (bits == 32) {
		section_header_size = 40;
		f->seek(0x20);
		section_table_pos = f->get_32();
		f->seek(0x30);
	} else { // 64
		section_header_size = 64;
		f->seek(0x28);
		section_table_pos = f->get_64();
		f->seek(0x3c);
	}
	int num_sections = f->get_16();
	int string_section_idx = f->get_16();

	// Load the strings table.
	uint8_t *strings;
	{
		// Jump to the strings section header.
		f->seek(section_table_pos + string_section_idx * section_header_size);

		// Read strings data size and offset.
		int64_t string_data_pos;
		int64_t string_data_size;
		if (bits == 32) {
			f->seek(f->get_position() + 0x10);
			string_data_pos = f->get_32();
			string_data_size = f->get_32();
		} else { // 64
			f->seek(f->get_position() + 0x18);
			string_data_pos = f->get_64();
			string_data_size = f->get_64();
		}

		// Read strings data.
		f->seek(string_data_pos);
		strings = (uint8_t *)memalloc(string_data_size);
		if (!strings) {
			return ERR_OUT_OF_MEMORY;
		}
		f->get_buffer(strings, string_data_size);
	}

	// Search for the "pck" section.

	bool found = false;
	for (int i = 0; i < num_sections; ++i) {
		int64_t section_header_pos = section_table_pos + i * section_header_size;
		f->seek(section_header_pos);

		uint32_t name_offset = f->get_32();
		if (strcmp((char *)strings + name_offset, "pck") == 0) {
			// "pck" section found, let's patch!

			if (bits == 32) {
				f->seek(section_header_pos + 0x10);
				f->store_32(p_embedded_start);
				f->store_32(p_embedded_size);
			} else { // 64
				f->seek(section_header_pos + 0x18);
				f->store_64(p_embedded_start);
				f->store_64(p_embedded_size);
			}

			found = true;
			break;
		}
	}

	memfree(strings);

	if (!found) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("Executable \"pck\" section not found."));
		return ERR_FILE_CORRUPT;
	}
	return OK;
}

Ref<Texture2D> EditorExportPlatformLinuxBSD::get_run_icon() const {
	return run_icon;
}

bool EditorExportPlatformLinuxBSD::poll_export() {
	Ref<EditorExportPreset> preset;

	for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
		Ref<EditorExportPreset> ep = EditorExport::get_singleton()->get_export_preset(i);
		if (ep->is_runnable() && ep->get_platform() == this) {
			preset = ep;
			break;
		}
	}

	int prev = menu_options;
	menu_options = (preset.is_valid() && preset->get("ssh_remote_deploy/enabled").operator bool());
	if (ssh_pid != 0 || !cleanup_commands.is_empty()) {
		if (menu_options == 0) {
			cleanup();
		} else {
			menu_options += 1;
		}
	}
	return menu_options != prev;
}

Ref<ImageTexture> EditorExportPlatformLinuxBSD::get_option_icon(int p_index) const {
	return p_index == 1 ? stop_icon : EditorExportPlatform::get_option_icon(p_index);
}

int EditorExportPlatformLinuxBSD::get_options_count() const {
	return menu_options;
}

String EditorExportPlatformLinuxBSD::get_option_label(int p_index) const {
	return (p_index) ? TTR("Stop and uninstall") : TTR("Run on remote Linux/BSD system");
}

String EditorExportPlatformLinuxBSD::get_option_tooltip(int p_index) const {
	return (p_index) ? TTR("Stop and uninstall running project from the remote system") : TTR("Run exported project on remote Linux/BSD system");
}

void EditorExportPlatformLinuxBSD::cleanup() {
	if (ssh_pid != 0 && OS::get_singleton()->is_process_running(ssh_pid)) {
		print_line("Terminating connection...");
		OS::get_singleton()->kill(ssh_pid);
		OS::get_singleton()->delay_usec(1000);
	}

	if (!cleanup_commands.is_empty()) {
		print_line("Stopping and deleting previous version...");
		for (const SSHCleanupCommand &cmd : cleanup_commands) {
			if (cmd.wait) {
				ssh_run_on_remote(cmd.host, cmd.port, cmd.ssh_args, cmd.cmd_args);
			} else {
				ssh_run_on_remote_no_wait(cmd.host, cmd.port, cmd.ssh_args, cmd.cmd_args);
			}
		}
	}
	ssh_pid = 0;
	cleanup_commands.clear();
}

Error EditorExportPlatformLinuxBSD::run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	cleanup();
	if (p_device) { // Stop command, cleanup only.
		return OK;
	}

	EditorProgress ep("run", TTR("Running..."), 5);

	const String dest = EditorPaths::get_singleton()->get_temp_dir().path_join("linuxbsd");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (!da->dir_exists(dest)) {
		Error err = da->make_dir_recursive(dest);
		if (err != OK) {
			EditorNode::get_singleton()->show_warning(TTR("Could not create temp directory:") + "\n" + dest);
			return err;
		}
	}

	String host = p_preset->get("ssh_remote_deploy/host").operator String();
	String port = p_preset->get("ssh_remote_deploy/port").operator String();
	if (port.is_empty()) {
		port = "22";
	}
	Vector<String> extra_args_ssh = p_preset->get("ssh_remote_deploy/extra_args_ssh").operator String().split(" ", false);
	Vector<String> extra_args_scp = p_preset->get("ssh_remote_deploy/extra_args_scp").operator String().split(" ", false);

	const String basepath = dest.path_join("tmp_linuxbsd_export");

#define CLEANUP_AND_RETURN(m_err)                      \
	{                                                  \
		if (da->file_exists(basepath + ".zip")) {      \
			da->remove(basepath + ".zip");             \
		}                                              \
		if (da->file_exists(basepath + "_start.sh")) { \
			da->remove(basepath + "_start.sh");        \
		}                                              \
		if (da->file_exists(basepath + "_clean.sh")) { \
			da->remove(basepath + "_clean.sh");        \
		}                                              \
		return m_err;                                  \
	}                                                  \
	((void)0)

	if (ep.step(TTR("Exporting project..."), 1)) {
		return ERR_SKIP;
	}
	Error err = export_project(p_preset, true, basepath + ".zip", p_debug_flags);
	if (err != OK) {
		DirAccess::remove_file_or_error(basepath + ".zip");
		return err;
	}

	String cmd_args;
	{
		Vector<String> cmd_args_list = gen_export_flags(p_debug_flags);
		for (int i = 0; i < cmd_args_list.size(); i++) {
			if (i != 0) {
				cmd_args += " ";
			}
			cmd_args += cmd_args_list[i];
		}
	}

	const bool use_remote = p_debug_flags.has_flag(DEBUG_FLAG_REMOTE_DEBUG) || p_debug_flags.has_flag(DEBUG_FLAG_DUMB_CLIENT);
	int dbg_port = EditorSettings::get_singleton()->get("network/debug/remote_port");

	print_line("Creating temporary directory...");
	ep.step(TTR("Creating temporary directory..."), 2);
	String temp_dir;
	err = ssh_run_on_remote(host, port, extra_args_ssh, "mktemp -d", &temp_dir);
	if (err != OK || temp_dir.is_empty()) {
		CLEANUP_AND_RETURN(err);
	}

	print_line("Uploading archive...");
	ep.step(TTR("Uploading archive..."), 3);
	err = ssh_push_to_remote(host, port, extra_args_scp, basepath + ".zip", temp_dir);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}

	{
		String run_script = p_preset->get("ssh_remote_deploy/run_script");
		run_script = run_script.replace("{temp_dir}", temp_dir);
		run_script = run_script.replace("{archive_name}", basepath.get_file() + ".zip");
		run_script = run_script.replace("{exe_name}", basepath.get_file());
		run_script = run_script.replace("{cmd_args}", cmd_args);

		Ref<FileAccess> f = FileAccess::open(basepath + "_start.sh", FileAccess::WRITE);
		if (f.is_null()) {
			CLEANUP_AND_RETURN(err);
		}

		f->store_string(run_script);
	}

	{
		String clean_script = p_preset->get("ssh_remote_deploy/cleanup_script");
		clean_script = clean_script.replace("{temp_dir}", temp_dir);
		clean_script = clean_script.replace("{archive_name}", basepath.get_file() + ".zip");
		clean_script = clean_script.replace("{exe_name}", basepath.get_file());
		clean_script = clean_script.replace("{cmd_args}", cmd_args);

		Ref<FileAccess> f = FileAccess::open(basepath + "_clean.sh", FileAccess::WRITE);
		if (f.is_null()) {
			CLEANUP_AND_RETURN(err);
		}

		f->store_string(clean_script);
	}

	print_line("Uploading scripts...");
	ep.step(TTR("Uploading scripts..."), 4);
	err = ssh_push_to_remote(host, port, extra_args_scp, basepath + "_start.sh", temp_dir);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}
	err = ssh_run_on_remote(host, port, extra_args_ssh, vformat("chmod +x \"%s/%s\"", temp_dir, basepath.get_file() + "_start.sh"));
	if (err != OK || temp_dir.is_empty()) {
		CLEANUP_AND_RETURN(err);
	}
	err = ssh_push_to_remote(host, port, extra_args_scp, basepath + "_clean.sh", temp_dir);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}
	err = ssh_run_on_remote(host, port, extra_args_ssh, vformat("chmod +x \"%s/%s\"", temp_dir, basepath.get_file() + "_clean.sh"));
	if (err != OK || temp_dir.is_empty()) {
		CLEANUP_AND_RETURN(err);
	}

	print_line("Starting project...");
	ep.step(TTR("Starting project..."), 5);
	err = ssh_run_on_remote_no_wait(host, port, extra_args_ssh, vformat("\"%s/%s\"", temp_dir, basepath.get_file() + "_start.sh"), &ssh_pid, (use_remote) ? dbg_port : -1);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}

	cleanup_commands.clear();
	cleanup_commands.push_back(SSHCleanupCommand(host, port, extra_args_ssh, vformat("\"%s/%s\"", temp_dir, basepath.get_file() + "_clean.sh")));

	print_line("Project started.");

	CLEANUP_AND_RETURN(OK);
#undef CLEANUP_AND_RETURN
}

EditorExportPlatformLinuxBSD::EditorExportPlatformLinuxBSD() {
	if (EditorNode::get_singleton()) {
#ifdef MODULE_SVG_ENABLED
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);

		ImageLoaderSVG::create_image_from_string(img, _linuxbsd_logo_svg, EDSCALE, upsample, false);
		set_logo(ImageTexture::create_from_image(img));

		ImageLoaderSVG::create_image_from_string(img, _linuxbsd_run_icon_svg, EDSCALE, upsample, false);
		run_icon = ImageTexture::create_from_image(img);
#endif

		Ref<Theme> theme = EditorNode::get_singleton()->get_editor_theme();
		if (theme.is_valid()) {
			stop_icon = theme->get_icon(SNAME("Stop"), EditorStringName(EditorIcons));
		} else {
			stop_icon.instantiate();
		}
	}
}
