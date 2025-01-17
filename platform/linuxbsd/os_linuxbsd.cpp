/**************************************************************************/
/*  os_linuxbsd.cpp                                                       */
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

#include "os_linuxbsd.h"

#include "core/io/certs_compressed.gen.h"
#include "core/io/dir_access.h"
#include "drivers/sdl/joypad_sdl.h"
#include "main/main.h"
#include "servers/display_server.h"
#include "servers/rendering_server.h"

#ifdef X11_ENABLED
#include "x11/display_server_x11.h"
#endif

#ifdef WAYLAND_ENABLED
#include "wayland/display_server_wayland.h"
#endif

#include "modules/modules_enabled.gen.h" // For regex.
#ifdef MODULE_REGEX_ENABLED
#include "modules/regex/regex.h"
#endif

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#ifdef HAVE_MNTENT
#include <mntent.h>
#endif

#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

void OS_LinuxBSD::alert(const String &p_alert, const String &p_title) {
	const char *message_programs[] = { "zenity", "kdialog", "Xdialog", "xmessage" };

	String path = get_environment("PATH");
	Vector<String> path_elems = path.split(":", false);
	String program;

	for (int i = 0; i < path_elems.size(); i++) {
		for (uint64_t k = 0; k < sizeof(message_programs) / sizeof(char *); k++) {
			String tested_path = path_elems[i].path_join(message_programs[k]);

			if (FileAccess::exists(tested_path)) {
				program = tested_path;
				break;
			}
		}

		if (program.length()) {
			break;
		}
	}

	List<String> args;

	if (program.ends_with("zenity")) {
		args.push_back("--warning");
		args.push_back("--width");
		args.push_back("500");
		args.push_back("--title");
		args.push_back(p_title);
		args.push_back("--text");
		args.push_back(p_alert);
	}

	if (program.ends_with("kdialog")) {
		// `--sorry` uses the same icon as `--warning` in Zenity.
		// As of KDialog 22.12.1, its `--warning` options are only available for yes/no questions.
		args.push_back("--sorry");
		args.push_back(p_alert);
		args.push_back("--title");
		args.push_back(p_title);
	}

	if (program.ends_with("Xdialog")) {
		args.push_back("--title");
		args.push_back(p_title);
		args.push_back("--msgbox");
		args.push_back(p_alert);
		args.push_back("0");
		args.push_back("0");
	}

	if (program.ends_with("xmessage")) {
		args.push_back("-center");
		args.push_back("-title");
		args.push_back(p_title);
		args.push_back(p_alert);
	}

	if (program.length()) {
		execute(program, args);
	} else {
		print_line(p_alert);
	}
}

void OS_LinuxBSD::initialize() {
	crash_handler.initialize();

	OS_Unix::initialize_core();

	system_dir_desktop_cache = get_system_dir(SYSTEM_DIR_DESKTOP);
}

void OS_LinuxBSD::initialize_joypads() {
#ifdef SDL_ENABLED
	joypad_sdl = memnew(JoypadSDL(Input::get_singleton()));
	if (joypad_sdl->initialize() == OK) {
		return;
	}
	// SDL init failed, fallback to the native driver
	memdelete(joypad_sdl);
	joypad_sdl = nullptr;
#endif
#ifdef JOYDEV_ENABLED
	joypad = memnew(JoypadLinux(Input::get_singleton()));
#endif
}

String OS_LinuxBSD::get_unique_id() const {
	static String machine_id;
	if (machine_id.is_empty()) {
#if defined(__FreeBSD__)
		const int mib[2] = { CTL_KERN, KERN_HOSTUUID };
		char buf[4096];
		memset(buf, 0, sizeof(buf));
		size_t len = sizeof(buf) - 1;
		if (sysctl(mib, 2, buf, &len, 0x0, 0) != -1) {
			machine_id = String::utf8(buf).replace("-", "");
		}
#else
		Ref<FileAccess> f = FileAccess::open("/etc/machine-id", FileAccess::READ);
		if (f.is_valid()) {
			while (machine_id.is_empty() && !f->eof_reached()) {
				machine_id = f->get_line().strip_edges();
			}
		}
#endif
	}
	return machine_id;
}

String OS_LinuxBSD::get_processor_name() const {
#if defined(__FreeBSD__)
	const int mib[2] = { CTL_HW, HW_MODEL };
	char buf[4096];
	memset(buf, 0, sizeof(buf));
	size_t len = sizeof(buf) - 1;
	if (sysctl(mib, 2, buf, &len, 0x0, 0) != -1) {
		return String::utf8(buf);
	}
#else
	Ref<FileAccess> f = FileAccess::open("/proc/cpuinfo", FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), "", String("Couldn't open `/proc/cpuinfo` to get the CPU model name. Returning an empty string."));

	while (!f->eof_reached()) {
		const String line = f->get_line();
		if (line.to_lower().contains("model name")) {
			return line.split(":")[1].strip_edges();
		}
	}
#endif

	ERR_FAIL_V_MSG("", String("Couldn't get the CPU model. Returning an empty string."));
}

bool OS_LinuxBSD::is_sandboxed() const {
	// This function is derived from SDL:
	// https://github.com/libsdl-org/SDL/blob/main/src/core/linux/SDL_sandbox.c#L28-L45

	if (access("/.flatpak-info", F_OK) == 0) {
		return true;
	}

	// For Snap, we check multiple variables because they might be set for
	// unrelated reasons. This is the same thing WebKitGTK does.
	if (has_environment("SNAP") && has_environment("SNAP_NAME") && has_environment("SNAP_REVISION")) {
		return true;
	}

	if (access("/run/host/container-manager", F_OK) == 0) {
		return true;
	}

	return false;
}

void OS_LinuxBSD::finalize() {
	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;

#ifdef ALSAMIDI_ENABLED
	driver_alsamidi.close();
#endif

#ifdef JOYDEV_ENABLED
	if (joypad) {
		memdelete(joypad);
	}
#endif

#ifdef SDL_ENABLED
	if (joypad_sdl) {
		memdelete(joypad_sdl);
	}
#endif
}

MainLoop *OS_LinuxBSD::get_main_loop() const {
	return main_loop;
}

void OS_LinuxBSD::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;
}

void OS_LinuxBSD::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
}

String OS_LinuxBSD::get_identifier() const {
	return "linuxbsd";
}

String OS_LinuxBSD::get_name() const {
#ifdef __linux__
	return "Linux";
#elif defined(__FreeBSD__)
	return "FreeBSD";
#elif defined(__NetBSD__)
	return "NetBSD";
#elif defined(__OpenBSD__)
	return "OpenBSD";
#else
	return "BSD";
#endif
}

String OS_LinuxBSD::get_systemd_os_release_info_value(const String &key) const {
	Ref<FileAccess> f = FileAccess::open("/etc/os-release", FileAccess::READ);
	if (f.is_valid()) {
		while (!f->eof_reached()) {
			const String line = f->get_line();
			if (line.contains(key)) {
				String value = line.split("=")[1].strip_edges();
				value = value.trim_prefix("\"");
				return value.trim_suffix("\"");
			}
		}
	}
	return "";
}

String OS_LinuxBSD::get_distribution_name() const {
	static String distribution_name = get_systemd_os_release_info_value("NAME"); // returns a value for systemd users, otherwise an empty string.
	if (!distribution_name.is_empty()) {
		return distribution_name;
	}
	struct utsname uts; // returns a decent value for BSD family.
	uname(&uts);
	distribution_name = uts.sysname;
	return distribution_name;
}

String OS_LinuxBSD::get_version() const {
	static String release_version = get_systemd_os_release_info_value("VERSION"); // returns a value for systemd users, otherwise an empty string.
	if (!release_version.is_empty()) {
		return release_version;
	}
	struct utsname uts; // returns a decent value for BSD family.
	uname(&uts);
	release_version = uts.version;
	return release_version;
}

Vector<String> OS_LinuxBSD::get_video_adapter_driver_info() const {
	if (RenderingServer::get_singleton() == nullptr) {
		return Vector<String>();
	}

	static Vector<String> info;
	if (!info.is_empty()) {
		return info;
	}

	const String rendering_device_name = RenderingServer::get_singleton()->get_video_adapter_name(); // e.g. `NVIDIA GeForce GTX 970`
	const String rendering_device_vendor = RenderingServer::get_singleton()->get_video_adapter_vendor(); // e.g. `NVIDIA`
	const String card_name = rendering_device_name.trim_prefix(rendering_device_vendor).strip_edges(); // -> `GeForce GTX 970`

	String vendor_device_id_mappings;
	List<String> lspci_args;
	lspci_args.push_back("-n");
	Error err = const_cast<OS_LinuxBSD *>(this)->execute("lspci", lspci_args, &vendor_device_id_mappings);
	if (err != OK || vendor_device_id_mappings.is_empty()) {
		return Vector<String>();
	}

	// Usually found under "VGA", but for example NVIDIA mobile/laptop adapters are often listed under "3D" and some AMD adapters are under "Display".
	const String dc_vga = "0300"; // VGA compatible controller
	const String dc_display = "0302"; // Display controller
	const String dc_3d = "0380"; // 3D controller

	// splitting results by device class allows prioritizing, if multiple devices are found.
	Vector<String> class_vga_device_candidates;
	Vector<String> class_display_device_candidates;
	Vector<String> class_3d_device_candidates;

#ifdef MODULE_REGEX_ENABLED
	RegEx regex_id_format = RegEx();
	regex_id_format.compile("^[a-f0-9]{4}:[a-f0-9]{4}$"); // e.g. `10de:13c2`; IDs are always in hexadecimal
#endif

	Vector<String> value_lines = vendor_device_id_mappings.split("\n", false); // example: `02:00.0 0300: 10de:13c2 (rev a1)`
	for (const String &line : value_lines) {
		Vector<String> columns = line.split(" ", false);
		if (columns.size() < 3) {
			continue;
		}
		String device_class = columns[1].trim_suffix(":");
		const String &vendor_device_id_mapping = columns[2];

#ifdef MODULE_REGEX_ENABLED
		if (regex_id_format.search(vendor_device_id_mapping).is_null()) {
			continue;
		}
#endif

		if (device_class == dc_vga) {
			class_vga_device_candidates.push_back(vendor_device_id_mapping);
		} else if (device_class == dc_display) {
			class_display_device_candidates.push_back(vendor_device_id_mapping);
		} else if (device_class == dc_3d) {
			class_3d_device_candidates.push_back(vendor_device_id_mapping);
		}
	}

	// Check results against currently used device (`card_name`), in case the user has multiple graphics cards.
	const String device_lit = "Device"; // line of interest
	class_vga_device_candidates = OS_LinuxBSD::lspci_device_filter(class_vga_device_candidates, dc_vga, device_lit, card_name);
	class_display_device_candidates = OS_LinuxBSD::lspci_device_filter(class_display_device_candidates, dc_display, device_lit, card_name);
	class_3d_device_candidates = OS_LinuxBSD::lspci_device_filter(class_3d_device_candidates, dc_3d, device_lit, card_name);

	// Get driver names and filter out invalid ones, because some adapters are dummys used only for passthrough.
	// And they have no indicator besides certain driver names.
	const String kernel_lit = "Kernel driver in use"; // line of interest
	const String dummys = "vfio"; // for e.g. pci passthrough dummy kernel driver `vfio-pci`
	Vector<String> class_vga_device_drivers = OS_LinuxBSD::lspci_get_device_value(class_vga_device_candidates, kernel_lit, dummys);
	Vector<String> class_display_device_drivers = OS_LinuxBSD::lspci_get_device_value(class_display_device_candidates, kernel_lit, dummys);
	Vector<String> class_3d_device_drivers = OS_LinuxBSD::lspci_get_device_value(class_3d_device_candidates, kernel_lit, dummys);

	String driver_name;
	String driver_version;

	// Use first valid value:
	for (const String &driver : class_3d_device_drivers) {
		driver_name = driver;
		break;
	}
	if (driver_name.is_empty()) {
		for (const String &driver : class_display_device_drivers) {
			driver_name = driver;
			break;
		}
	}
	if (driver_name.is_empty()) {
		for (const String &driver : class_vga_device_drivers) {
			driver_name = driver;
			break;
		}
	}

	info.push_back(driver_name);

	String modinfo;
	List<String> modinfo_args;
	modinfo_args.push_back(driver_name);
	err = const_cast<OS_LinuxBSD *>(this)->execute("modinfo", modinfo_args, &modinfo);
	if (err != OK || modinfo.is_empty()) {
		info.push_back(""); // So that this method always either returns an empty array, or an array of length 2.
		return info;
	}
	Vector<String> lines = modinfo.split("\n", false);
	for (const String &line : lines) {
		Vector<String> columns = line.split(":", false, 1);
		if (columns.size() < 2) {
			continue;
		}
		if (columns[0].strip_edges() == "version") {
			driver_version = columns[1].strip_edges(); // example value: `510.85.02` on Linux/BSD
			break;
		}
	}

	info.push_back(driver_version);

	return info;
}

Vector<String> OS_LinuxBSD::lspci_device_filter(Vector<String> vendor_device_id_mapping, String class_suffix, String check_column, String whitelist) const {
	// NOTE: whitelist can be changed to `Vector<String>`, if the need arises.
	const String sep = ":";
	Vector<String> devices;
	for (const String &mapping : vendor_device_id_mapping) {
		String device;
		List<String> d_args;
		d_args.push_back("-d");
		d_args.push_back(mapping + sep + class_suffix);
		d_args.push_back("-vmm");
		Error err = const_cast<OS_LinuxBSD *>(this)->execute("lspci", d_args, &device); // e.g. `lspci -d 10de:13c2:0300 -vmm`
		if (err != OK) {
			return Vector<String>();
		} else if (device.is_empty()) {
			continue;
		}

		Vector<String> device_lines = device.split("\n", false);
		for (const String &line : device_lines) {
			Vector<String> columns = line.split(":", false, 1);
			if (columns.size() < 2) {
				continue;
			}
			if (columns[0].strip_edges() == check_column) {
				// for `column[0] == "Device"` this may contain `GM204 [GeForce GTX 970]`
				bool is_valid = true;
				if (!whitelist.is_empty()) {
					is_valid = columns[1].strip_edges().contains(whitelist);
				}
				if (is_valid) {
					devices.push_back(mapping);
				}
				break;
			}
		}
	}
	return devices;
}

Vector<String> OS_LinuxBSD::lspci_get_device_value(Vector<String> vendor_device_id_mapping, String check_column, String blacklist) const {
	// NOTE: blacklist can be changed to `Vector<String>`, if the need arises.
	const String sep = ":";
	Vector<String> values;
	for (const String &mapping : vendor_device_id_mapping) {
		String device;
		List<String> d_args;
		d_args.push_back("-d");
		d_args.push_back(mapping);
		d_args.push_back("-k");
		Error err = const_cast<OS_LinuxBSD *>(this)->execute("lspci", d_args, &device); // e.g. `lspci -d 10de:13c2 -k`
		if (err != OK) {
			return Vector<String>();
		} else if (device.is_empty()) {
			continue;
		}

		Vector<String> device_lines = device.split("\n", false);
		for (const String &line : device_lines) {
			Vector<String> columns = line.split(":", false, 1);
			if (columns.size() < 2) {
				continue;
			}
			if (columns[0].strip_edges() == check_column) {
				// for `column[0] == "Kernel driver in use"` this may contain `nvidia`
				bool is_valid = true;
				const String value = columns[1].strip_edges();
				if (!blacklist.is_empty()) {
					is_valid = !value.contains(blacklist);
				}
				if (is_valid) {
					values.push_back(value);
				}
				break;
			}
		}
	}
	return values;
}

Error OS_LinuxBSD::shell_open(const String &p_uri) {
	Error ok;
	int err_code;
	List<String> args;
	args.push_back(p_uri);

	// Agnostic
	ok = execute("xdg-open", args, nullptr, &err_code);
	if (ok == OK && !err_code) {
		return OK;
	} else if (err_code == 2) {
		return ERR_FILE_NOT_FOUND;
	}
	// GNOME
	args.push_front("open"); // The command is `gio open`, so we need to add it to args
	ok = execute("gio", args, nullptr, &err_code);
	if (ok == OK && !err_code) {
		return OK;
	} else if (err_code == 2) {
		return ERR_FILE_NOT_FOUND;
	}
	args.pop_front();
	ok = execute("gvfs-open", args, nullptr, &err_code);
	if (ok == OK && !err_code) {
		return OK;
	} else if (err_code == 2) {
		return ERR_FILE_NOT_FOUND;
	}
	// KDE
	ok = execute("kde-open5", args, nullptr, &err_code);
	if (ok == OK && !err_code) {
		return OK;
	}
	ok = execute("kde-open", args, nullptr, &err_code);
	return !err_code ? ok : FAILED;
}

bool OS_LinuxBSD::_check_internal_feature_support(const String &p_feature) {
#ifdef FONTCONFIG_ENABLED
	if (p_feature == "system_fonts") {
		return font_config_initialized;
	}
#endif

#ifndef __linux__
	// `bsd` includes **all** BSD, not only "other BSD" (see `get_name()`).
	if (p_feature == "bsd") {
		return true;
	}
#endif

	if (p_feature == "pc") {
		return true;
	}

	// Match against the specific OS (`linux`, `freebsd`, `netbsd`, `openbsd`).
	if (p_feature == get_name().to_lower()) {
		return true;
	}

	return false;
}

uint64_t OS_LinuxBSD::get_embedded_pck_offset() const {
	Ref<FileAccess> f = FileAccess::open(get_executable_path(), FileAccess::READ);
	if (f.is_null()) {
		return 0;
	}

	// Read and check ELF magic number.
	{
		uint32_t magic = f->get_32();
		if (magic != 0x464c457f) { // 0x7F + "ELF"
			return 0;
		}
	}

	// Read program architecture bits from class field.
	int bits = f->get_8() * 32;

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
			return 0;
		}
		f->get_buffer(strings, string_data_size);
	}

	// Search for the "pck" section.
	int64_t off = 0;
	for (int i = 0; i < num_sections; ++i) {
		int64_t section_header_pos = section_table_pos + i * section_header_size;
		f->seek(section_header_pos);

		uint32_t name_offset = f->get_32();
		if (strcmp((char *)strings + name_offset, "pck") == 0) {
			if (bits == 32) {
				f->seek(section_header_pos + 0x10);
				off = f->get_32();
			} else { // 64
				f->seek(section_header_pos + 0x18);
				off = f->get_64();
			}
			break;
		}
	}
	memfree(strings);

	return off;
}

Vector<String> OS_LinuxBSD::get_system_fonts() const {
#ifdef FONTCONFIG_ENABLED
	if (!font_config_initialized) {
		ERR_FAIL_V_MSG(Vector<String>(), "Unable to load fontconfig, system font support is disabled.");
	}

	HashSet<String> font_names;
	Vector<String> ret;
	static const char *allowed_formats[] = { "TrueType", "CFF" };
	for (size_t i = 0; i < sizeof(allowed_formats) / sizeof(const char *); i++) {
		FcPattern *pattern = FcPatternCreate();
		ERR_CONTINUE(!pattern);

		FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
		FcPatternAddString(pattern, FC_FONTFORMAT, reinterpret_cast<const FcChar8 *>(allowed_formats[i]));

		FcFontSet *font_set = FcFontList(config, pattern, object_set);
		if (font_set) {
			for (int j = 0; j < font_set->nfont; j++) {
				char *family_name = nullptr;
				if (FcPatternGetString(font_set->fonts[j], FC_FAMILY, 0, reinterpret_cast<FcChar8 **>(&family_name)) == FcResultMatch) {
					if (family_name) {
						font_names.insert(String::utf8(family_name));
					}
				}
			}
			FcFontSetDestroy(font_set);
		}
		FcPatternDestroy(pattern);
	}

	for (const String &E : font_names) {
		ret.push_back(E);
	}
	return ret;
#else
	ERR_FAIL_V_MSG(Vector<String>(), "Godot was compiled without fontconfig, system font support is disabled.");
#endif
}

#ifdef FONTCONFIG_ENABLED
int OS_LinuxBSD::_weight_to_fc(int p_weight) const {
	if (p_weight < 150) {
		return FC_WEIGHT_THIN;
	} else if (p_weight < 250) {
		return FC_WEIGHT_EXTRALIGHT;
	} else if (p_weight < 325) {
		return FC_WEIGHT_LIGHT;
	} else if (p_weight < 375) {
		return FC_WEIGHT_DEMILIGHT;
	} else if (p_weight < 390) {
		return FC_WEIGHT_BOOK;
	} else if (p_weight < 450) {
		return FC_WEIGHT_REGULAR;
	} else if (p_weight < 550) {
		return FC_WEIGHT_MEDIUM;
	} else if (p_weight < 650) {
		return FC_WEIGHT_DEMIBOLD;
	} else if (p_weight < 750) {
		return FC_WEIGHT_BOLD;
	} else if (p_weight < 850) {
		return FC_WEIGHT_EXTRABOLD;
	} else if (p_weight < 925) {
		return FC_WEIGHT_BLACK;
	} else {
		return FC_WEIGHT_EXTRABLACK;
	}
}

int OS_LinuxBSD::_stretch_to_fc(int p_stretch) const {
	if (p_stretch < 56) {
		return FC_WIDTH_ULTRACONDENSED;
	} else if (p_stretch < 69) {
		return FC_WIDTH_EXTRACONDENSED;
	} else if (p_stretch < 81) {
		return FC_WIDTH_CONDENSED;
	} else if (p_stretch < 93) {
		return FC_WIDTH_SEMICONDENSED;
	} else if (p_stretch < 106) {
		return FC_WIDTH_NORMAL;
	} else if (p_stretch < 137) {
		return FC_WIDTH_SEMIEXPANDED;
	} else if (p_stretch < 144) {
		return FC_WIDTH_EXPANDED;
	} else if (p_stretch < 162) {
		return FC_WIDTH_EXTRAEXPANDED;
	} else {
		return FC_WIDTH_ULTRAEXPANDED;
	}
}
#endif // FONTCONFIG_ENABLED

Vector<String> OS_LinuxBSD::get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale, const String &p_script, int p_weight, int p_stretch, bool p_italic) const {
#ifdef FONTCONFIG_ENABLED
	if (!font_config_initialized) {
		ERR_FAIL_V_MSG(Vector<String>(), "Unable to load fontconfig, system font support is disabled.");
	}

	Vector<String> ret;
	static const char *allowed_formats[] = { "TrueType", "CFF" };
	for (size_t i = 0; i < sizeof(allowed_formats) / sizeof(const char *); i++) {
		FcPattern *pattern = FcPatternCreate();
		if (pattern) {
			FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
			FcPatternAddString(pattern, FC_FONTFORMAT, reinterpret_cast<const FcChar8 *>(allowed_formats[i]));
			FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>(p_font_name.utf8().get_data()));
			FcPatternAddInteger(pattern, FC_WEIGHT, _weight_to_fc(p_weight));
			FcPatternAddInteger(pattern, FC_WIDTH, _stretch_to_fc(p_stretch));
			FcPatternAddInteger(pattern, FC_SLANT, p_italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);

			FcCharSet *char_set = FcCharSetCreate();
			for (int j = 0; j < p_text.size(); j++) {
				FcCharSetAddChar(char_set, p_text[j]);
			}
			FcPatternAddCharSet(pattern, FC_CHARSET, char_set);

			FcLangSet *lang_set = FcLangSetCreate();
			FcLangSetAdd(lang_set, reinterpret_cast<const FcChar8 *>(p_locale.utf8().get_data()));
			FcPatternAddLangSet(pattern, FC_LANG, lang_set);

			FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
			FcDefaultSubstitute(pattern);

			FcResult result;
			FcPattern *match = FcFontMatch(nullptr, pattern, &result);
			if (match) {
				char *file_name = nullptr;
				if (FcPatternGetString(match, FC_FILE, 0, reinterpret_cast<FcChar8 **>(&file_name)) == FcResultMatch) {
					if (file_name) {
						ret.push_back(String::utf8(file_name));
					}
				}
				FcPatternDestroy(match);
			}
			FcPatternDestroy(pattern);
			FcCharSetDestroy(char_set);
			FcLangSetDestroy(lang_set);
		}
	}

	return ret;
#else
	ERR_FAIL_V_MSG(Vector<String>(), "Godot was compiled without fontconfig, system font support is disabled.");
#endif
}

String OS_LinuxBSD::get_system_font_path(const String &p_font_name, int p_weight, int p_stretch, bool p_italic) const {
#ifdef FONTCONFIG_ENABLED
	if (!font_config_initialized) {
		ERR_FAIL_V_MSG(String(), "Unable to load fontconfig, system font support is disabled.");
	}

	static const char *allowed_formats[] = { "TrueType", "CFF" };
	for (size_t i = 0; i < sizeof(allowed_formats) / sizeof(const char *); i++) {
		FcPattern *pattern = FcPatternCreate();
		if (pattern) {
			bool allow_substitutes = (p_font_name.to_lower() == "sans-serif") || (p_font_name.to_lower() == "serif") || (p_font_name.to_lower() == "monospace") || (p_font_name.to_lower() == "cursive") || (p_font_name.to_lower() == "fantasy");

			FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
			FcPatternAddString(pattern, FC_FONTFORMAT, reinterpret_cast<const FcChar8 *>(allowed_formats[i]));
			FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>(p_font_name.utf8().get_data()));
			FcPatternAddInteger(pattern, FC_WEIGHT, _weight_to_fc(p_weight));
			FcPatternAddInteger(pattern, FC_WIDTH, _stretch_to_fc(p_stretch));
			FcPatternAddInteger(pattern, FC_SLANT, p_italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);

			FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
			FcDefaultSubstitute(pattern);

			FcResult result;
			FcPattern *match = FcFontMatch(nullptr, pattern, &result);
			if (match) {
				if (!allow_substitutes) {
					char *family_name = nullptr;
					if (FcPatternGetString(match, FC_FAMILY, 0, reinterpret_cast<FcChar8 **>(&family_name)) == FcResultMatch) {
						if (family_name && String::utf8(family_name).to_lower() != p_font_name.to_lower()) {
							FcPatternDestroy(match);
							FcPatternDestroy(pattern);
							continue;
						}
					}
				}
				char *file_name = nullptr;
				if (FcPatternGetString(match, FC_FILE, 0, reinterpret_cast<FcChar8 **>(&file_name)) == FcResultMatch) {
					if (file_name) {
						String ret = String::utf8(file_name);
						FcPatternDestroy(match);
						FcPatternDestroy(pattern);
						return ret;
					}
				}
				FcPatternDestroy(match);
			}
			FcPatternDestroy(pattern);
		}
	}

	return String();
#else
	ERR_FAIL_V_MSG(String(), "Godot was compiled without fontconfig, system font support is disabled.");
#endif
}

String OS_LinuxBSD::get_config_path() const {
	if (has_environment("XDG_CONFIG_HOME")) {
		if (get_environment("XDG_CONFIG_HOME").is_absolute_path()) {
			return get_environment("XDG_CONFIG_HOME");
		} else {
			WARN_PRINT_ONCE("`XDG_CONFIG_HOME` is a relative path. Ignoring its value and falling back to `$HOME/.config` or `.` per the XDG Base Directory specification.");
			return has_environment("HOME") ? get_environment("HOME").path_join(".config") : ".";
		}
	} else if (has_environment("HOME")) {
		return get_environment("HOME").path_join(".config");
	} else {
		return ".";
	}
}

String OS_LinuxBSD::get_data_path() const {
	if (has_environment("XDG_DATA_HOME")) {
		if (get_environment("XDG_DATA_HOME").is_absolute_path()) {
			return get_environment("XDG_DATA_HOME");
		} else {
			WARN_PRINT_ONCE("`XDG_DATA_HOME` is a relative path. Ignoring its value and falling back to `$HOME/.local/share` or `get_config_path()` per the XDG Base Directory specification.");
			return has_environment("HOME") ? get_environment("HOME").path_join(".local/share") : get_config_path();
		}
	} else if (has_environment("HOME")) {
		return get_environment("HOME").path_join(".local/share");
	} else {
		return get_config_path();
	}
}

String OS_LinuxBSD::get_cache_path() const {
	if (has_environment("XDG_CACHE_HOME")) {
		if (get_environment("XDG_CACHE_HOME").is_absolute_path()) {
			return get_environment("XDG_CACHE_HOME");
		} else {
			WARN_PRINT_ONCE("`XDG_CACHE_HOME` is a relative path. Ignoring its value and falling back to `$HOME/.cache` or `get_config_path()` per the XDG Base Directory specification.");
			return has_environment("HOME") ? get_environment("HOME").path_join(".cache") : get_config_path();
		}
	} else if (has_environment("HOME")) {
		return get_environment("HOME").path_join(".cache");
	} else {
		return get_config_path();
	}
}

String OS_LinuxBSD::get_system_dir(SystemDir p_dir, bool p_shared_storage) const {
	if (p_dir == SYSTEM_DIR_DESKTOP && !system_dir_desktop_cache.is_empty()) {
		return system_dir_desktop_cache;
	}

	String xdgparam;

	switch (p_dir) {
		case SYSTEM_DIR_DESKTOP: {
			xdgparam = "DESKTOP";
		} break;
		case SYSTEM_DIR_DCIM: {
			xdgparam = "PICTURES";
		} break;
		case SYSTEM_DIR_DOCUMENTS: {
			xdgparam = "DOCUMENTS";
		} break;
		case SYSTEM_DIR_DOWNLOADS: {
			xdgparam = "DOWNLOAD";
		} break;
		case SYSTEM_DIR_MOVIES: {
			xdgparam = "VIDEOS";
		} break;
		case SYSTEM_DIR_MUSIC: {
			xdgparam = "MUSIC";
		} break;
		case SYSTEM_DIR_PICTURES: {
			xdgparam = "PICTURES";
		} break;
		case SYSTEM_DIR_RINGTONES: {
			xdgparam = "MUSIC";
		} break;
	}

	String pipe;
	List<String> arg;
	arg.push_back(xdgparam);
	Error err = const_cast<OS_LinuxBSD *>(this)->execute("xdg-user-dir", arg, &pipe);
	if (err != OK) {
		return ".";
	}
	return pipe.strip_edges();
}

void OS_LinuxBSD::run() {
	if (!main_loop) {
		return;
	}

	main_loop->initialize();

	//uint64_t last_ticks=get_ticks_usec();

	//int frames=0;
	//uint64_t frame=0;

	while (true) {
		DisplayServer::get_singleton()->process_events(); // get rid of pending events
#ifdef SDL_ENABLED
		if (joypad_sdl) {
			joypad_sdl->process_events();
		}
#endif
#ifdef JOYDEV_ENABLED
		if (joypad) {
			joypad->process_joypads();
		}
#endif
		if (Main::iteration()) {
			break;
		}
	}

	main_loop->finalize();
}

void OS_LinuxBSD::disable_crash_handler() {
	crash_handler.disable();
}

bool OS_LinuxBSD::is_disable_crash_handler() const {
	return crash_handler.is_disabled();
}

static String get_mountpoint(const String &p_path) {
	struct stat s;
	if (stat(p_path.utf8().get_data(), &s)) {
		return "";
	}

#ifdef HAVE_MNTENT
	dev_t dev = s.st_dev;
	FILE *fd = setmntent("/proc/mounts", "r");
	if (!fd) {
		return "";
	}

	struct mntent mnt;
	char buf[1024];
	size_t buflen = 1024;
	while (getmntent_r(fd, &mnt, buf, buflen)) {
		if (!stat(mnt.mnt_dir, &s) && s.st_dev == dev) {
			endmntent(fd);
			return String(mnt.mnt_dir);
		}
	}

	endmntent(fd);
#endif
	return "";
}

Error OS_LinuxBSD::move_to_trash(const String &p_path) {
	// We try multiple methods, until we find one that works.
	// So we only return on success until we exhausted possibilities.

	String path = p_path.rstrip("/"); // Strip trailing slash when path points to a directory.
	int err_code;
	List<String> args;
	args.push_back(path);

	args.push_front("trash"); // The command is `gio trash <file_name>` so we add it before the path.
	Error result = execute("gio", args, nullptr, &err_code); // For GNOME based machines.
	if (result == OK && err_code == 0) { // Success.
		return OK;
	}

	args.pop_front();
	args.push_front("move");
	args.push_back("trash:/"); // The command is `kioclient5 move <file_name> trash:/`.
	result = execute("kioclient5", args, nullptr, &err_code); // For KDE based machines.
	if (result == OK && err_code == 0) {
		return OK;
	}

	args.pop_front();
	args.pop_back();
	result = execute("gvfs-trash", args, nullptr, &err_code); // For older Linux machines.
	if (result == OK && err_code == 0) {
		return OK;
	}

	// If the commands `kioclient5`, `gio` or `gvfs-trash` don't work on the system we do it manually.
	String trash_path = "";
	String mnt = get_mountpoint(path);

	// If there is a directory "[Mountpoint]/.Trash-[UID], use it as the trash can.
	if (!mnt.is_empty()) {
		String mountpoint_trash_path(mnt + "/.Trash-" + itos(getuid()));
		struct stat s;
		if (!stat(mountpoint_trash_path.utf8().get_data(), &s)) {
			trash_path = mountpoint_trash_path;
		}
	}

	// Otherwise, if ${XDG_DATA_HOME} is defined, use "${XDG_DATA_HOME}/Trash" as the trash can.
	if (trash_path.is_empty()) {
		char *dhome = getenv("XDG_DATA_HOME");
		if (dhome) {
			trash_path = String::utf8(dhome) + "/Trash";
		}
	}

	// Otherwise, if ${HOME} is defined, use "${HOME}/.local/share/Trash" as the trash can.
	if (trash_path.is_empty()) {
		char *home = getenv("HOME");
		if (home) {
			trash_path = String::utf8(home) + "/.local/share/Trash";
		}
	}

	// Issue an error if none of the previous locations is appropriate for the trash can.
	ERR_FAIL_COND_V_MSG(trash_path.is_empty(), FAILED, "Could not determine the trash can location");

	// Create needed directories for decided trash can location.
	{
		Ref<DirAccess> dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		Error err = dir_access->make_dir_recursive(trash_path);

		// Issue an error if trash can is not created properly.
		ERR_FAIL_COND_V_MSG(err != OK, err, "Could not create the trash path \"" + trash_path + "\"");
		err = dir_access->make_dir_recursive(trash_path + "/files");
		ERR_FAIL_COND_V_MSG(err != OK, err, "Could not create the trash path \"" + trash_path + "/files\"");
		err = dir_access->make_dir_recursive(trash_path + "/info");
		ERR_FAIL_COND_V_MSG(err != OK, err, "Could not create the trash path \"" + trash_path + "/info\"");
	}

	// The trash can is successfully created, now we check that we don't exceed our file name length limit.
	// If the file name is too long trim it so we can add the identifying number and ".trashinfo".
	// Assumes that the file name length limit is 255 characters.
	String file_name = path.get_file();
	if (file_name.length() > 240) {
		file_name = file_name.substr(0, file_name.length() - 15);
	}

	String dest_path = trash_path + "/files/" + file_name;
	struct stat buff;
	int id_number = 0;
	String fn = file_name;

	// Checks if a resource with the same name already exist in the trash can,
	// if there is, add an identifying number to our resource's name.
	while (stat(dest_path.utf8().get_data(), &buff) == 0) {
		id_number++;

		// Added a limit to check for identically named files already on the trash can
		// if there are too many it could make the editor unresponsive.
		ERR_FAIL_COND_V_MSG(id_number > 99, FAILED, "Too many identically named resources already in the trash can.");
		fn = file_name + "." + itos(id_number);
		dest_path = trash_path + "/files/" + fn;
	}
	file_name = fn;

	String renamed_path = path.get_base_dir() + "/" + file_name;

	// Generates the .trashinfo file
	OS::DateTime dt = OS::get_singleton()->get_datetime(false);
	String timestamp = vformat("%04d-%02d-%02dT%02d:%02d:", dt.year, (int)dt.month, dt.day, dt.hour, dt.minute);
	timestamp = vformat("%s%02d", timestamp, dt.second); // vformat only supports up to 6 arguments.
	String trash_info = "[Trash Info]\nPath=" + path.uri_encode() + "\nDeletionDate=" + timestamp + "\n";
	{
		Error err;
		{
			Ref<FileAccess> file = FileAccess::open(trash_path + "/info/" + file_name + ".trashinfo", FileAccess::WRITE, &err);
			ERR_FAIL_COND_V_MSG(err != OK, err, "Can't create trashinfo file: \"" + trash_path + "/info/" + file_name + ".trashinfo\"");
			file->store_string(trash_info);
		}

		// Rename our resource before moving it to the trash can.
		Ref<DirAccess> dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		err = dir_access->rename(path, renamed_path);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Can't rename file \"" + path + "\" to \"" + renamed_path + "\"");
	}

	// Move the given resource to the trash can.
	// Do not use DirAccess:rename() because it can't move files across multiple mountpoints.
	List<String> mv_args;
	mv_args.push_back(renamed_path);
	mv_args.push_back(trash_path + "/files");
	{
		int retval;
		Error err = execute("mv", mv_args, nullptr, &retval);

		// Issue an error if "mv" failed to move the given resource to the trash can.
		if (err != OK || retval != 0) {
			ERR_PRINT("move_to_trash: Could not move the resource \"" + path + "\" to the trash can \"" + trash_path + "/files\"");
			Ref<DirAccess> dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			err = dir_access->rename(renamed_path, path);
			ERR_FAIL_COND_V_MSG(err != OK, err, "Could not rename \"" + renamed_path + "\" back to its original name: \"" + path + "\"");
			return FAILED;
		}
	}
	return OK;
}

String OS_LinuxBSD::get_system_ca_certificates() {
	String certfile;
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	// Compile time preferred certificates path.
	if (!String(_SYSTEM_CERTS_PATH).is_empty() && da->file_exists(_SYSTEM_CERTS_PATH)) {
		certfile = _SYSTEM_CERTS_PATH;
	} else if (da->file_exists("/etc/ssl/certs/ca-certificates.crt")) {
		// Debian/Ubuntu
		certfile = "/etc/ssl/certs/ca-certificates.crt";
	} else if (da->file_exists("/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem")) {
		// Fedora
		certfile = "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem";
	} else if (da->file_exists("/etc/ca-certificates/extracted/tls-ca-bundle.pem")) {
		// Arch Linux
		certfile = "/etc/ca-certificates/extracted/tls-ca-bundle.pem";
	} else if (da->file_exists("/var/lib/ca-certificates/ca-bundle.pem")) {
		// openSUSE
		certfile = "/var/lib/ca-certificates/ca-bundle.pem";
	} else if (da->file_exists("/etc/ssl/cert.pem")) {
		// FreeBSD/OpenBSD
		certfile = "/etc/ssl/cert.pem";
	}

	if (certfile.is_empty()) {
		return "";
	}

	Ref<FileAccess> f = FileAccess::open(certfile, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), "", vformat("Failed to open system CA certificates file: '%s'", certfile));

	return f->get_as_text();
}

OS_LinuxBSD::OS_LinuxBSD() {
	main_loop = nullptr;

#ifdef PULSEAUDIO_ENABLED
	AudioDriverManager::add_driver(&driver_pulseaudio);
#endif

#ifdef ALSA_ENABLED
	AudioDriverManager::add_driver(&driver_alsa);
#endif

#ifdef X11_ENABLED
	DisplayServerX11::register_x11_driver();
#endif

#ifdef WAYLAND_ENABLED
	DisplayServerWayland::register_wayland_driver();
#endif

#ifdef FONTCONFIG_ENABLED
#ifdef SOWRAP_ENABLED
#ifdef DEBUG_ENABLED
	int dylibloader_verbose = 1;
#else
	int dylibloader_verbose = 0;
#endif
	font_config_initialized = (initialize_fontconfig(dylibloader_verbose) == 0);
#else
	font_config_initialized = true;
#endif
	if (font_config_initialized) {
		bool ver_ok = false;
		int version = FcGetVersion();
		ver_ok = ((version / 100 / 100) == 2 && (version / 100 % 100) >= 11) || ((version / 100 / 100) > 2); // 2.11.0
		print_verbose(vformat("FontConfig %d.%d.%d detected.", version / 100 / 100, version / 100 % 100, version % 100));
		if (!ver_ok) {
			font_config_initialized = false;
		}
	}

	if (font_config_initialized) {
		config = FcInitLoadConfigAndFonts();
		if (!config) {
			font_config_initialized = false;
		}
		object_set = FcObjectSetBuild(FC_FAMILY, FC_FILE, nullptr);
		if (!object_set) {
			font_config_initialized = false;
		}
	}
#endif // FONTCONFIG_ENABLED
}

OS_LinuxBSD::~OS_LinuxBSD() {
#ifdef FONTCONFIG_ENABLED
	if (object_set) {
		FcObjectSetDestroy(object_set);
	}
	if (config) {
		FcConfigDestroy(config);
	}
#endif // FONTCONFIG_ENABLED
}
