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
#include "core/io/image_loader.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_string_names.h"
#include "editor/export/editor_export.h"
#include "editor/themes/editor_scale.h"

#include "modules/svg/image_loader_svg.h"

Error EditorExportPlatformWindows::_process_icon(const Ref<EditorExportPreset> &p_preset, const String &p_src_path, const String &p_dst_path) {
	static const uint8_t icon_size[] = { 16, 32, 48, 64, 128, 0 /*256*/ };

	struct IconData {
		Vector<uint8_t> data;
		uint8_t pal_colors = 0;
		uint16_t planes = 0;
		uint16_t bpp = 32;
	};

	HashMap<uint8_t, IconData> images;
	Error err;

	if (p_src_path.get_extension() == "ico") {
		Ref<FileAccess> f = FileAccess::open(p_src_path, FileAccess::READ, &err);
		if (err != OK) {
			return err;
		}

		// Read ICONDIR.
		f->get_16(); // Reserved.
		uint16_t icon_type = f->get_16(); // Image type: 1 - ICO.
		uint16_t icon_count = f->get_16(); // Number of images.
		ERR_FAIL_COND_V(icon_type != 1, ERR_CANT_OPEN);

		for (uint16_t i = 0; i < icon_count; i++) {
			// Read ICONDIRENTRY.
			uint16_t w = f->get_8(); // Width in pixels.
			uint16_t h = f->get_8(); // Height in pixels.
			uint8_t pal_colors = f->get_8(); // Number of colors in the palette (0 - no palette).
			f->get_8(); // Reserved.
			uint16_t planes = f->get_16(); // Number of color planes.
			uint16_t bpp = f->get_16(); // Bits per pixel.
			uint32_t img_size = f->get_32(); // Image data size in bytes.
			uint32_t img_offset = f->get_32(); // Image data offset.
			if (w != h) {
				continue;
			}

			// Read image data.
			uint64_t prev_offset = f->get_position();
			images[w].pal_colors = pal_colors;
			images[w].planes = planes;
			images[w].bpp = bpp;
			images[w].data.resize(img_size);
			f->seek(img_offset);
			f->get_buffer(images[w].data.ptrw(), img_size);
			f->seek(prev_offset);
		}
	} else {
		Ref<Image> src_image = _load_icon_or_splash_image(p_src_path, &err);
		ERR_FAIL_COND_V(err != OK || src_image.is_null() || src_image->is_empty(), ERR_CANT_OPEN);

		for (size_t i = 0; i < std::size(icon_size); ++i) {
			int size = (icon_size[i] == 0) ? 256 : icon_size[i];

			Ref<Image> res_image = src_image->duplicate();
			ERR_FAIL_COND_V(res_image.is_null() || res_image->is_empty(), ERR_CANT_OPEN);
			res_image->resize(size, size, (Image::Interpolation)(p_preset->get("application/icon_interpolation").operator int()));
			images[icon_size[i]].data = res_image->save_png_to_buffer();
		}
	}

	uint16_t valid_icon_count = 0;
	for (size_t i = 0; i < std::size(icon_size); ++i) {
		if (images.has(icon_size[i])) {
			valid_icon_count++;
		} else {
			int size = (icon_size[i] == 0) ? 256 : icon_size[i];
			add_message(EXPORT_MESSAGE_WARNING, TTR("Resources Modification"), vformat(TTR("Icon size \"%d\" is missing."), size));
		}
	}
	ERR_FAIL_COND_V(valid_icon_count == 0, ERR_CANT_OPEN);

	Ref<FileAccess> fw = FileAccess::open(p_dst_path, FileAccess::WRITE, &err);
	if (err != OK) {
		return err;
	}

	// Write ICONDIR.
	fw->store_16(0); // Reserved.
	fw->store_16(1); // Image type: 1 - ICO.
	fw->store_16(valid_icon_count); // Number of images.

	// Write ICONDIRENTRY.
	uint32_t img_offset = 6 + 16 * valid_icon_count;
	for (size_t i = 0; i < std::size(icon_size); ++i) {
		if (images.has(icon_size[i])) {
			const IconData &di = images[icon_size[i]];
			fw->store_8(icon_size[i]); // Width in pixels.
			fw->store_8(icon_size[i]); // Height in pixels.
			fw->store_8(di.pal_colors); // Number of colors in the palette (0 - no palette).
			fw->store_8(0); // Reserved.
			fw->store_16(di.planes); // Number of color planes.
			fw->store_16(di.bpp); // Bits per pixel.
			fw->store_32(di.data.size()); // Image data size in bytes.
			fw->store_32(img_offset); // Image data offset.

			img_offset += di.data.size();
		}
	}

	// Write image data.
	for (size_t i = 0; i < std::size(icon_size); ++i) {
		if (images.has(icon_size[i])) {
			const IconData &di = images[icon_size[i]];
			fw->store_buffer(di.data.ptr(), di.data.size());
		}
	}
	return OK;
}

Error EditorExportPlatformWindows::sign_shared_object(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path) {
	if (p_preset->get("codesign/enable")) {
		return _code_sign(p_preset, p_path);
	} else {
		return OK;
	}
}

Error EditorExportPlatformWindows::modify_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	if (p_preset->get("application/modify_resources")) {
		_rcedit_add_data(p_preset, p_path, false);
		String wrapper_path = p_path.get_basename() + ".console.exe";
		if (FileAccess::exists(wrapper_path)) {
			_rcedit_add_data(p_preset, wrapper_path, true);
		}
	}
	return OK;
}

Error EditorExportPlatformWindows::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	String custom_debug = p_preset->get("custom_template/debug");
	String custom_release = p_preset->get("custom_template/release");
	String arch = p_preset->get("binary_format/architecture");

	String template_path = p_debug ? custom_debug : custom_release;
	template_path = template_path.strip_edges();
	if (template_path.is_empty()) {
		template_path = find_export_template(get_template_file_name(p_debug ? "debug" : "release", arch));
	} else {
		String exe_arch = _get_exe_arch(template_path);
		if (arch != exe_arch) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), vformat(TTR("Mismatching custom export template executable architecture: found \"%s\", expected \"%s\"."), exe_arch, arch));
			return ERR_CANT_CREATE;
		}
	}

	bool export_as_zip = p_path.ends_with("zip");
	bool embedded = p_preset->get("binary_format/embed_pck");

	String pkg_name;
	if (String(ProjectSettings::get_singleton()->get("application/config/name")) != "") {
		pkg_name = String(ProjectSettings::get_singleton()->get("application/config/name"));
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
		path = tmp_dir_path.path_join(p_path.get_file().get_basename() + ".exe");
	}

	int export_angle = p_preset->get("application/export_angle");
	bool include_angle_libs = false;
	if (export_angle == 0) {
		include_angle_libs = (String(GLOBAL_GET("rendering/gl_compatibility/driver.windows")) == "opengl3_angle") && (String(GLOBAL_GET("rendering/renderer/rendering_method")) == "gl_compatibility");
	} else if (export_angle == 1) {
		include_angle_libs = true;
	}
	if (include_angle_libs) {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (da->file_exists(template_path.get_base_dir().path_join("libEGL." + arch + ".dll"))) {
			da->copy(template_path.get_base_dir().path_join("libEGL." + arch + ".dll"), path.get_base_dir().path_join("libEGL.dll"), get_chmod_flags());
		}
		if (da->file_exists(template_path.get_base_dir().path_join("libGLESv2." + arch + ".dll"))) {
			da->copy(template_path.get_base_dir().path_join("libGLESv2." + arch + ".dll"), path.get_base_dir().path_join("libGLESv2.dll"), get_chmod_flags());
		}
	}

	int export_d3d12 = p_preset->get("application/export_d3d12");
	bool agility_sdk_multiarch = p_preset->get("application/d3d12_agility_sdk_multiarch");
	bool include_d3d12_extra_libs = false;
	if (export_d3d12 == 0) {
		include_d3d12_extra_libs = (String(GLOBAL_GET("rendering/rendering_device/driver.windows")) == "d3d12") && (String(GLOBAL_GET("rendering/renderer/rendering_method")) != "gl_compatibility");
	} else if (export_d3d12 == 1) {
		include_d3d12_extra_libs = true;
	}
	if (include_d3d12_extra_libs) {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (da->file_exists(template_path.get_base_dir().path_join("D3D12Core." + arch + ".dll"))) {
			if (agility_sdk_multiarch) {
				da->make_dir_recursive(path.get_base_dir().path_join(arch));
				da->copy(template_path.get_base_dir().path_join("D3D12Core." + arch + ".dll"), path.get_base_dir().path_join(arch).path_join("D3D12Core.dll"), get_chmod_flags());
			} else {
				da->copy(template_path.get_base_dir().path_join("D3D12Core." + arch + ".dll"), path.get_base_dir().path_join("D3D12Core.dll"), get_chmod_flags());
			}
		}
		if (da->file_exists(template_path.get_base_dir().path_join("d3d12SDKLayers." + arch + ".dll"))) {
			if (agility_sdk_multiarch) {
				da->make_dir_recursive(path.get_base_dir().path_join(arch));
				da->copy(template_path.get_base_dir().path_join("d3d12SDKLayers." + arch + ".dll"), path.get_base_dir().path_join(arch).path_join("d3d12SDKLayers.dll"), get_chmod_flags());
			} else {
				da->copy(template_path.get_base_dir().path_join("d3d12SDKLayers." + arch + ".dll"), path.get_base_dir().path_join("d3d12SDKLayers.dll"), get_chmod_flags());
			}
		}
		if (da->file_exists(template_path.get_base_dir().path_join("WinPixEventRuntime." + arch + ".dll"))) {
			da->copy(template_path.get_base_dir().path_join("WinPixEventRuntime." + arch + ".dll"), path.get_base_dir().path_join("WinPixEventRuntime.dll"), get_chmod_flags());
		}
	}

	// Export project.
	String pck_path = path;
	if (embedded) {
		pck_path = pck_path.get_basename() + ".tmp";
	}

	Error err = EditorExportPlatformPC::export_project(p_preset, p_debug, pck_path, p_flags);
	if (err != OK) {
		// Message is supplied by the subroutine method.
		return err;
	}

	if (p_preset->get("codesign/enable")) {
		_code_sign(p_preset, pck_path);
		String wrapper_path = path.get_basename() + ".console.exe";
		if (FileAccess::exists(wrapper_path)) {
			_code_sign(p_preset, wrapper_path);
		}
	}

	if (embedded) {
		Ref<DirAccess> tmp_dir = DirAccess::create_for_path(path.get_base_dir());
		err = tmp_dir->rename(pck_path, path);
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), vformat(TTR("Failed to rename temporary file \"%s\"."), pck_path));
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

String EditorExportPlatformWindows::get_template_file_name(const String &p_target, const String &p_arch) const {
	return "windows_" + p_target + "_" + p_arch + ".exe";
}

List<String> EditorExportPlatformWindows::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("exe");
	list.push_back("zip");
	return list;
}

String EditorExportPlatformWindows::get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const {
	if (p_preset) {
		if (p_name == "application/icon") {
			String icon_path = ProjectSettings::get_singleton()->globalize_path(p_preset->get("application/icon"));
			if (!icon_path.is_empty() && !FileAccess::exists(icon_path)) {
				return TTR("Invalid icon path.");
			}
		} else if (p_name == "application/file_version") {
			String file_version = p_preset->get("application/file_version");
			if (!file_version.is_empty()) {
				PackedStringArray version_array = file_version.split(".", false);
				if (version_array.size() != 4 || !version_array[0].is_valid_int() ||
						!version_array[1].is_valid_int() || !version_array[2].is_valid_int() ||
						!version_array[3].is_valid_int() || file_version.contains_char('-')) {
					return TTR("Invalid file version.");
				}
			}
		} else if (p_name == "application/product_version") {
			String product_version = p_preset->get("application/product_version");
			if (!product_version.is_empty()) {
				PackedStringArray version_array = product_version.split(".", false);
				if (version_array.size() != 4 || !version_array[0].is_valid_int() ||
						!version_array[1].is_valid_int() || !version_array[2].is_valid_int() ||
						!version_array[3].is_valid_int() || product_version.contains_char('-')) {
					return TTR("Invalid product version.");
				}
			}
		}
	}
	return EditorExportPlatformPC::get_export_option_warning(p_preset, p_name);
}

bool EditorExportPlatformWindows::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	if (p_preset == nullptr) {
		return true;
	}

	// This option is not supported by "osslsigncode", used on non-Windows host.
	if (!OS::get_singleton()->has_feature("windows") && p_option == "codesign/identity_type") {
		return false;
	}

	bool advanced_options_enabled = p_preset->are_advanced_options_enabled();

	// Hide codesign.
	bool codesign = p_preset->get("codesign/enable");
	if (!codesign && p_option != "codesign/enable" && p_option.begins_with("codesign/")) {
		return false;
	}

	// Hide resources.
	bool mod_res = p_preset->get("application/modify_resources");
	if (!mod_res && p_option != "application/modify_resources" && p_option != "application/export_angle" && p_option != "application/export_d3d12" && p_option != "application/d3d12_agility_sdk_multiarch" && p_option.begins_with("application/")) {
		return false;
	}

	// Hide SSH options.
	bool ssh = p_preset->get("ssh_remote_deploy/enabled");
	if (!ssh && p_option != "ssh_remote_deploy/enabled" && p_option.begins_with("ssh_remote_deploy/")) {
		return false;
	}

	if (p_option == "dotnet/embed_build_outputs" ||
			p_option == "custom_template/debug" ||
			p_option == "custom_template/release" ||
			p_option == "application/d3d12_agility_sdk_multiarch" ||
			p_option == "application/export_angle" ||
			p_option == "application/export_d3d12" ||
			p_option == "application/icon_interpolation") {
		return advanced_options_enabled;
	}
	return true;
}

void EditorExportPlatformWindows::get_export_options(List<ExportOption> *r_options) const {
	EditorExportPlatformPC::get_export_options(r_options);

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "binary_format/architecture", PROPERTY_HINT_ENUM, "x86_64,x86_32,arm64"), "x86_64"));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/enable"), false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "codesign/identity_type", PROPERTY_HINT_ENUM, "Select automatically,Use PKCS12 file (specify *.PFX/*.P12 file),Use certificate store (specify SHA-1 hash)", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/identity", PROPERTY_HINT_GLOBAL_FILE, "*.pfx,*.p12", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/password", PROPERTY_HINT_PASSWORD, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "codesign/timestamp"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/timestamp_server_url"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "codesign/digest_algorithm", PROPERTY_HINT_ENUM, "SHA1,SHA256"), 1));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "codesign/description"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "codesign/custom_options"), PackedStringArray()));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "application/modify_resources"), true, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/icon", PROPERTY_HINT_FILE, "*.ico,*.png,*.webp,*.svg"), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/console_wrapper_icon", PROPERTY_HINT_FILE, "*.ico,*.png,*.webp,*.svg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "application/icon_interpolation", PROPERTY_HINT_ENUM, "Nearest neighbor,Bilinear,Cubic,Trilinear,Lanczos"), 4));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/file_version", PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use project version"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/product_version", PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use project version"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/company_name", PROPERTY_HINT_PLACEHOLDER_TEXT, "Company Name"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/product_name", PROPERTY_HINT_PLACEHOLDER_TEXT, "Game Name"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/file_description"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/copyright"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "application/trademarks"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "application/export_angle", PROPERTY_HINT_ENUM, "Auto,Yes,No"), 0, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "application/export_d3d12", PROPERTY_HINT_ENUM, "Auto,Yes,No"), 0, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "application/d3d12_agility_sdk_multiarch"), true, true));

	String run_script = "Expand-Archive -LiteralPath '{temp_dir}\\{archive_name}' -DestinationPath '{temp_dir}'\n"
						"$action = New-ScheduledTaskAction -Execute '{temp_dir}\\{exe_name}' -Argument '{cmd_args}'\n"
						"$trigger = New-ScheduledTaskTrigger -Once -At 00:00\n"
						"$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries\n"
						"$task = New-ScheduledTask -Action $action -Trigger $trigger -Settings $settings\n"
						"Register-ScheduledTask godot_remote_debug -InputObject $task -Force:$true\n"
						"Start-ScheduledTask -TaskName godot_remote_debug\n"
						"while (Get-ScheduledTask -TaskName godot_remote_debug | ? State -eq running) { Start-Sleep -Milliseconds 100 }\n"
						"Unregister-ScheduledTask -TaskName godot_remote_debug -Confirm:$false -ErrorAction:SilentlyContinue";

	String cleanup_script = "Stop-ScheduledTask -TaskName godot_remote_debug -ErrorAction:SilentlyContinue\n"
							"Unregister-ScheduledTask -TaskName godot_remote_debug -Confirm:$false -ErrorAction:SilentlyContinue\n"
							"Remove-Item -Recurse -Force '{temp_dir}'";

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "ssh_remote_deploy/enabled"), false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/host"), "user@host_ip"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/port"), "22"));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/extra_args_ssh", PROPERTY_HINT_MULTILINE_TEXT), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/extra_args_scp", PROPERTY_HINT_MULTILINE_TEXT), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/run_script", PROPERTY_HINT_MULTILINE_TEXT), run_script));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "ssh_remote_deploy/cleanup_script", PROPERTY_HINT_MULTILINE_TEXT), cleanup_script));
}

Error EditorExportPlatformWindows::_rcedit_add_data(const Ref<EditorExportPreset> &p_preset, const String &p_path, bool p_console_icon) {
	String rcedit_path = EDITOR_GET("export/windows/rcedit");

	if (rcedit_path != String() && !FileAccess::exists(rcedit_path)) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Resources Modification"), vformat(TTR("Could not find rcedit executable at \"%s\"."), rcedit_path));
		return ERR_FILE_NOT_FOUND;
	}

	if (rcedit_path == String()) {
		rcedit_path = "rcedit"; // try to run rcedit from PATH
	}

#ifndef WINDOWS_ENABLED
	// On non-Windows we need WINE to run rcedit
	String wine_path = EDITOR_GET("export/windows/wine");

	if (!wine_path.is_empty() && !FileAccess::exists(wine_path)) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Resources Modification"), vformat(TTR("Could not find wine executable at \"%s\"."), wine_path));
		return ERR_FILE_NOT_FOUND;
	}

	if (wine_path.is_empty()) {
		wine_path = "wine"; // try to run wine from PATH
	}
#endif

	String icon_path;
	if (p_preset->get("application/icon") != "") {
		icon_path = p_preset->get("application/icon");
	} else if (GLOBAL_GET("application/config/windows_native_icon") != "") {
		icon_path = GLOBAL_GET("application/config/windows_native_icon");
	} else {
		icon_path = GLOBAL_GET("application/config/icon");
	}
	icon_path = ProjectSettings::get_singleton()->globalize_path(icon_path);

	if (p_console_icon) {
		String console_icon_path = ProjectSettings::get_singleton()->globalize_path(p_preset->get("application/console_wrapper_icon"));
		if (!console_icon_path.is_empty() && FileAccess::exists(console_icon_path)) {
			icon_path = console_icon_path;
		}
	}

	String tmp_icon_path = EditorPaths::get_singleton()->get_temp_dir().path_join("_rcedit.ico");
	if (!icon_path.is_empty()) {
		if (_process_icon(p_preset, icon_path, tmp_icon_path) != OK) {
			add_message(EXPORT_MESSAGE_WARNING, TTR("Resources Modification"), vformat(TTR("Invalid icon file \"%s\"."), icon_path));
			icon_path = String();
		}
	}

	String file_version = p_preset->get_version("application/file_version", true);
	String product_version = p_preset->get_version("application/product_version", true);
	String company_name = p_preset->get("application/company_name");
	String product_name = p_preset->get("application/product_name");
	String file_description = p_preset->get("application/file_description");
	String copyright = p_preset->get("application/copyright");
	String trademarks = p_preset->get("application/trademarks");
	String comments = p_preset->get("application/comments");

	List<String> args;
	args.push_back(p_path);
	if (!icon_path.is_empty()) {
		args.push_back("--set-icon");
		args.push_back(tmp_icon_path);
	}
	if (!file_version.is_empty()) {
		args.push_back("--set-file-version");
		args.push_back(file_version);
	}
	if (!product_version.is_empty()) {
		args.push_back("--set-product-version");
		args.push_back(product_version);
	}
	if (!company_name.is_empty()) {
		args.push_back("--set-version-string");
		args.push_back("CompanyName");
		args.push_back(company_name);
	}
	if (!product_name.is_empty()) {
		args.push_back("--set-version-string");
		args.push_back("ProductName");
		args.push_back(product_name);
	}
	if (!file_description.is_empty()) {
		args.push_back("--set-version-string");
		args.push_back("FileDescription");
		args.push_back(file_description);
	}
	if (!copyright.is_empty()) {
		args.push_back("--set-version-string");
		args.push_back("LegalCopyright");
		args.push_back(copyright);
	}
	if (!trademarks.is_empty()) {
		args.push_back("--set-version-string");
		args.push_back("LegalTrademarks");
		args.push_back(trademarks);
	}

#ifndef WINDOWS_ENABLED
	// On non-Windows we need WINE to run rcedit
	args.push_front(rcedit_path);
	rcedit_path = wine_path;
#endif

	String str;
	Error err = OS::get_singleton()->execute(rcedit_path, args, &str, nullptr, true);

	if (FileAccess::exists(tmp_icon_path)) {
		DirAccess::remove_file_or_error(tmp_icon_path);
	}

	if (err != OK || str.contains("not found") || str.contains("not recognized")) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Resources Modification"), TTR("Could not start rcedit executable. Configure rcedit path in the Editor Settings (Export > Windows > rcedit), or disable \"Application > Modify Resources\" in the export preset."));
		return err;
	}
	print_line("rcedit (" + p_path + "): " + str);

	if (str.contains("Fatal error")) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Resources Modification"), vformat(TTR("rcedit failed to modify executable: %s."), str));
		return FAILED;
	}

	return OK;
}

Error EditorExportPlatformWindows::_code_sign(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
	List<String> args;

#ifdef WINDOWS_ENABLED
	String signtool_path = EDITOR_GET("export/windows/signtool");
	if (!signtool_path.is_empty() && !FileAccess::exists(signtool_path)) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("Could not find signtool executable at \"%s\"."), signtool_path));
		return ERR_FILE_NOT_FOUND;
	}
	if (signtool_path.is_empty()) {
		signtool_path = "signtool"; // try to run signtool from PATH
	}
#else
	String signtool_path = EDITOR_GET("export/windows/osslsigncode");
	if (!signtool_path.is_empty() && !FileAccess::exists(signtool_path)) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("Could not find osslsigncode executable at \"%s\"."), signtool_path));
		return ERR_FILE_NOT_FOUND;
	}
	if (signtool_path.is_empty()) {
		signtool_path = "osslsigncode"; // try to run signtool from PATH
	}
#endif

	args.push_back("sign");

	//identity
#ifdef WINDOWS_ENABLED
	int id_type = p_preset->get_or_env("codesign/identity_type", ENV_WIN_CODESIGN_ID_TYPE);
	if (id_type == 0) { //auto select
		args.push_back("/a");
	} else if (id_type == 1) { //pkcs12
		if (p_preset->get_or_env("codesign/identity", ENV_WIN_CODESIGN_ID) != "") {
			args.push_back("/f");
			args.push_back(p_preset->get_or_env("codesign/identity", ENV_WIN_CODESIGN_ID));
		} else {
			add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("No identity found."));
			return FAILED;
		}
	} else if (id_type == 2) { //Windows certificate store
		if (p_preset->get_or_env("codesign/identity", ENV_WIN_CODESIGN_ID) != "") {
			args.push_back("/sha1");
			args.push_back(p_preset->get_or_env("codesign/identity", ENV_WIN_CODESIGN_ID));
		} else {
			add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("No identity found."));
			return FAILED;
		}
	} else {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Invalid identity type."));
		return FAILED;
	}
#else
	int id_type = 1;
	if (p_preset->get_or_env("codesign/identity", ENV_WIN_CODESIGN_ID) != "") {
		args.push_back("-pkcs12");
		args.push_back(p_preset->get_or_env("codesign/identity", ENV_WIN_CODESIGN_ID));
	} else {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("No identity found."));
		return FAILED;
	}
#endif

	//password
	if ((id_type == 1) && (p_preset->get_or_env("codesign/password", ENV_WIN_CODESIGN_PASS) != "")) {
#ifdef WINDOWS_ENABLED
		args.push_back("/p");
#else
		args.push_back("-pass");
#endif
		args.push_back(p_preset->get_or_env("codesign/password", ENV_WIN_CODESIGN_PASS));
	}

	//timestamp
	if (p_preset->get("codesign/timestamp")) {
		if (p_preset->get("codesign/timestamp_server") != "") {
#ifdef WINDOWS_ENABLED
			args.push_back("/tr");
			args.push_back(p_preset->get("codesign/timestamp_server_url"));
			args.push_back("/td");
			if ((int)p_preset->get("codesign/digest_algorithm") == 0) {
				args.push_back("sha1");
			} else {
				args.push_back("sha256");
			}
#else
			args.push_back("-ts");
			args.push_back(p_preset->get("codesign/timestamp_server_url"));
#endif
		} else {
			add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Invalid timestamp server."));
			return FAILED;
		}
	}

	//digest
#ifdef WINDOWS_ENABLED
	args.push_back("/fd");
#else
	args.push_back("-h");
#endif
	if ((int)p_preset->get("codesign/digest_algorithm") == 0) {
		args.push_back("sha1");
	} else {
		args.push_back("sha256");
	}

	//description
	if (p_preset->get("codesign/description") != "") {
#ifdef WINDOWS_ENABLED
		args.push_back("/d");
#else
		args.push_back("-n");
#endif
		args.push_back(p_preset->get("codesign/description"));
	}

	//user options
	PackedStringArray user_args = p_preset->get("codesign/custom_options");
	for (int i = 0; i < user_args.size(); i++) {
		String user_arg = user_args[i].strip_edges();
		if (!user_arg.is_empty()) {
			args.push_back(user_arg);
		}
	}

#ifndef WINDOWS_ENABLED
	args.push_back("-in");
#endif
	args.push_back(p_path);
#ifndef WINDOWS_ENABLED
	args.push_back("-out");
	args.push_back(p_path + "_signed");
#endif

	String str;
	Error err = OS::get_singleton()->execute(signtool_path, args, &str, nullptr, true);
	if (err != OK || str.contains("not found") || str.contains("not recognized")) {
#ifdef WINDOWS_ENABLED
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Could not start signtool executable. Configure signtool path in the Editor Settings (Export > Windows > signtool), or disable \"Codesign\" in the export preset."));
#else
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Could not start osslsigncode executable. Configure signtool path in the Editor Settings (Export > Windows > osslsigncode), or disable \"Codesign\" in the export preset."));
#endif
		return err;
	}

	print_line("codesign (" + p_path + "): " + str);
#ifndef WINDOWS_ENABLED
	if (str.contains("SignTool Error")) {
#else
	if (str.contains("Failed")) {
#endif
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("Signtool failed to sign executable: %s."), str));
		return FAILED;
	}

#ifndef WINDOWS_ENABLED
	Ref<DirAccess> tmp_dir = DirAccess::create_for_path(p_path.get_base_dir());

	err = tmp_dir->remove(p_path);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("Failed to remove temporary file \"%s\"."), p_path));
		return err;
	}

	err = tmp_dir->rename(p_path + "_signed", p_path);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("Failed to rename temporary file \"%s\"."), p_path + "_signed"));
		return err;
	}
#endif

	return OK;
}

bool EditorExportPlatformWindows::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
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

	String rcedit_path = EDITOR_GET("export/windows/rcedit");
	if (p_preset->get("application/modify_resources") && rcedit_path.is_empty()) {
		err += TTR("The rcedit tool must be configured in the Editor Settings (Export > Windows > rcedit) to change the icon or app information data.") + "\n";
	}

	if (!err.is_empty()) {
		r_error = err;
	}

	return valid;
}

bool EditorExportPlatformWindows::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	String err;
	bool valid = true;

	List<ExportOption> options;
	get_export_options(&options);
	for (const EditorExportPlatform::ExportOption &E : options) {
		if (get_export_option_visibility(p_preset.ptr(), E.option.name)) {
			String warn = get_export_option_warning(p_preset.ptr(), E.option.name);
			if (!warn.is_empty()) {
				err += warn + "\n";
				if (E.required) {
					valid = false;
				}
			}
		}
	}

	if (!err.is_empty()) {
		r_error = err;
	}

	return valid;
}

String EditorExportPlatformWindows::_get_exe_arch(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return "invalid";
	}

	// Jump to the PE header and check the magic number.
	{
		f->seek(0x3c);
		uint32_t pe_pos = f->get_32();

		f->seek(pe_pos);
		uint32_t magic = f->get_32();
		if (magic != 0x00004550) {
			return "invalid";
		}
	}

	// Process header.
	uint16_t machine = f->get_16();
	f->close();

	switch (machine) {
		case 0x014c:
			return "x86_32";
		case 0x8664:
			return "x86_64";
		case 0x01c0:
		case 0x01c4:
			return "arm32";
		case 0xaa64:
			return "arm64";
		default:
			return "unknown";
	}
}

Error EditorExportPlatformWindows::fixup_embedded_pck(const String &p_path, int64_t p_embedded_start, int64_t p_embedded_size) {
	// Patch the header of the "pck" section in the PE file so that it corresponds to the embedded data

	if (p_embedded_size + p_embedded_start >= 0x100000000) { // Check for total executable size
		add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("Windows executables cannot be >= 4 GiB."));
		return ERR_INVALID_DATA;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ_WRITE);
	if (f.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), vformat(TTR("Failed to open executable file \"%s\"."), p_path));
		return ERR_CANT_OPEN;
	}

	// Jump to the PE header and check the magic number
	{
		f->seek(0x3c);
		uint32_t pe_pos = f->get_32();

		f->seek(pe_pos);
		uint32_t magic = f->get_32();
		if (magic != 0x00004550) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("Executable file header corrupted."));
			return ERR_FILE_CORRUPT;
		}
	}

	// Process header

	int num_sections;
	{
		int64_t header_pos = f->get_position();

		f->seek(header_pos + 2);
		num_sections = f->get_16();
		f->seek(header_pos + 16);
		uint16_t opt_header_size = f->get_16();

		// Skip rest of header + optional header to go to the section headers
		f->seek(f->get_position() + 2 + opt_header_size);
	}

	// Search for the "pck" section

	int64_t section_table_pos = f->get_position();

	bool found = false;
	for (int i = 0; i < num_sections; ++i) {
		int64_t section_header_pos = section_table_pos + i * 40;
		f->seek(section_header_pos);

		uint8_t section_name[9];
		f->get_buffer(section_name, 8);
		section_name[8] = '\0';

		if (strcmp((char *)section_name, "pck") == 0) {
			// "pck" section found, let's patch!

			// Set virtual size to a little to avoid it taking memory (zero would give issues)
			f->seek(section_header_pos + 8);
			f->store_32(8);

			f->seek(section_header_pos + 16);
			f->store_32(p_embedded_size);
			f->seek(section_header_pos + 20);
			f->store_32(p_embedded_start);

			found = true;
			break;
		}
	}

	if (!found) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("PCK Embedding"), TTR("Executable \"pck\" section not found."));
		return ERR_FILE_CORRUPT;
	}
	return OK;
}

Ref<Texture2D> EditorExportPlatformWindows::get_run_icon() const {
	return run_icon;
}

bool EditorExportPlatformWindows::poll_export() {
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

Ref<ImageTexture> EditorExportPlatformWindows::get_option_icon(int p_index) const {
	return p_index == 1 ? stop_icon : EditorExportPlatform::get_option_icon(p_index);
}

int EditorExportPlatformWindows::get_options_count() const {
	return menu_options;
}

String EditorExportPlatformWindows::get_option_label(int p_index) const {
	return (p_index) ? TTR("Stop and uninstall") : TTR("Run on remote Windows system");
}

String EditorExportPlatformWindows::get_option_tooltip(int p_index) const {
	return (p_index) ? TTR("Stop and uninstall running project from the remote system") : TTR("Run exported project on remote Windows system");
}

void EditorExportPlatformWindows::cleanup() {
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

Error EditorExportPlatformWindows::run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	cleanup();
	if (p_device) { // Stop command, cleanup only.
		return OK;
	}

	EditorProgress ep("run", TTR("Running..."), 5);

	const String dest = EditorPaths::get_singleton()->get_temp_dir().path_join("windows");
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

	const String basepath = dest.path_join("tmp_windows_export");

#define CLEANUP_AND_RETURN(m_err)                       \
	{                                                   \
		if (da->file_exists(basepath + ".zip")) {       \
			da->remove(basepath + ".zip");              \
		}                                               \
		if (da->file_exists(basepath + "_start.ps1")) { \
			da->remove(basepath + "_start.ps1");        \
		}                                               \
		if (da->file_exists(basepath + "_clean.ps1")) { \
			da->remove(basepath + "_clean.ps1");        \
		}                                               \
		return m_err;                                   \
	}                                                   \
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
#ifndef WINDOWS_ENABLED
	err = ssh_run_on_remote(host, port, extra_args_ssh, "powershell -command \\\"\\$tmp = Join-Path \\$Env:Temp \\$(New-Guid); New-Item -Type Directory -Path \\$tmp | Out-Null; Write-Output \\$tmp\\\"", &temp_dir);
#else
	err = ssh_run_on_remote(host, port, extra_args_ssh, "powershell -command \"$tmp = Join-Path $Env:Temp $(New-Guid); New-Item -Type Directory -Path $tmp ^| Out-Null; Write-Output $tmp\"", &temp_dir);
#endif
	if (err != OK || temp_dir.is_empty()) {
		CLEANUP_AND_RETURN(err);
	}

	print_line("Uploading archive...");
	ep.step(TTR("Uploading archive..."), 3);
	err = ssh_push_to_remote(host, port, extra_args_scp, basepath + ".zip", temp_dir);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}

	if (cmd_args.is_empty()) {
		cmd_args = " ";
	}

	{
		String run_script = p_preset->get("ssh_remote_deploy/run_script");
		run_script = run_script.replace("{temp_dir}", temp_dir);
		run_script = run_script.replace("{archive_name}", basepath.get_file() + ".zip");
		run_script = run_script.replace("{exe_name}", basepath.get_file() + ".exe");
		run_script = run_script.replace("{cmd_args}", cmd_args);

		Ref<FileAccess> f = FileAccess::open(basepath + "_start.ps1", FileAccess::WRITE);
		if (f.is_null()) {
			CLEANUP_AND_RETURN(err);
		}

		f->store_string(run_script);
	}

	{
		String clean_script = p_preset->get("ssh_remote_deploy/cleanup_script");
		clean_script = clean_script.replace("{temp_dir}", temp_dir);
		clean_script = clean_script.replace("{archive_name}", basepath.get_file() + ".zip");
		clean_script = clean_script.replace("{exe_name}", basepath.get_file() + ".exe");
		clean_script = clean_script.replace("{cmd_args}", cmd_args);

		Ref<FileAccess> f = FileAccess::open(basepath + "_clean.ps1", FileAccess::WRITE);
		if (f.is_null()) {
			CLEANUP_AND_RETURN(err);
		}

		f->store_string(clean_script);
	}

	print_line("Uploading scripts...");
	ep.step(TTR("Uploading scripts..."), 4);
	err = ssh_push_to_remote(host, port, extra_args_scp, basepath + "_start.ps1", temp_dir);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}
	err = ssh_push_to_remote(host, port, extra_args_scp, basepath + "_clean.ps1", temp_dir);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}

	print_line("Starting project...");
	ep.step(TTR("Starting project..."), 5);
	err = ssh_run_on_remote_no_wait(host, port, extra_args_ssh, vformat("powershell -file \"%s\\%s\"", temp_dir, basepath.get_file() + "_start.ps1"), &ssh_pid, (use_remote) ? dbg_port : -1);
	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}

	cleanup_commands.clear();
	cleanup_commands.push_back(SSHCleanupCommand(host, port, extra_args_ssh, vformat("powershell -file \"%s\\%s\"", temp_dir, basepath.get_file() + "_clean.ps1")));

	print_line("Project started.");

	CLEANUP_AND_RETURN(OK);
#undef CLEANUP_AND_RETURN
}

EditorExportPlatformWindows::EditorExportPlatformWindows() {
	if (EditorNode::get_singleton()) {
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);

		ImageLoaderSVG::create_image_from_string(img, _windows_logo_svg, EDSCALE, upsample, false);
		set_logo(ImageTexture::create_from_image(img));

		ImageLoaderSVG::create_image_from_string(img, _windows_run_icon_svg, EDSCALE, upsample, false);
		run_icon = ImageTexture::create_from_image(img);

		Ref<Theme> theme = EditorNode::get_singleton()->get_editor_theme();
		if (theme.is_valid()) {
			stop_icon = theme->get_icon(SNAME("Stop"), EditorStringName(EditorIcons));
		} else {
			stop_icon.instantiate();
		}
	}
}
