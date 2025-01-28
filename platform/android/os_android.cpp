/**************************************************************************/
/*  os_android.cpp                                                        */
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

#include "os_android.h"

#include "dir_access_jandroid.h"
#include "display_server_android.h"
#include "file_access_android.h"
#include "file_access_filesystem_jandroid.h"
#include "java_godot_io_wrapper.h"
#include "java_godot_wrapper.h"
#include "net_socket_android.h"

#include "core/config/project_settings.h"
#include "core/extension/gdextension_manager.h"
#include "core/io/xml_parser.h"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"
#include "main/main.h"
#include "scene/main/scene_tree.h"
#include "servers/rendering_server.h"

#include <dlfcn.h>
#include <sys/system_properties.h>

const char *OS_Android::ANDROID_EXEC_PATH = "apk";

String _remove_symlink(const String &dir) {
	// Workaround for Android 6.0+ using a symlink.
	// Save the current directory.
	char current_dir_name[2048];
	getcwd(current_dir_name, 2048);
	// Change directory to the external data directory.
	chdir(dir.utf8().get_data());
	// Get the actual directory without the potential symlink.
	char dir_name_without_symlink[2048];
	getcwd(dir_name_without_symlink, 2048);
	// Convert back to a String.
	String dir_without_symlink(dir_name_without_symlink);
	// Restore original current directory.
	chdir(current_dir_name);
	return dir_without_symlink;
}

class AndroidLogger : public Logger {
public:
	virtual void logv(const char *p_format, va_list p_list, bool p_err) {
		__android_log_vprint(p_err ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, "godot", p_format, p_list);
	}

	virtual ~AndroidLogger() {}
};

void OS_Android::alert(const String &p_alert, const String &p_title) {
	ERR_FAIL_NULL(godot_java);
	godot_java->alert(p_alert, p_title);
}

void OS_Android::initialize_core() {
	OS_Unix::initialize_core();

#ifdef TOOLS_ENABLED
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_RESOURCES);
#else
	if (use_apk_expansion) {
		FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_RESOURCES);
	} else {
		FileAccess::make_default<FileAccessAndroid>(FileAccess::ACCESS_RESOURCES);
	}
#endif
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessFilesystemJAndroid>(FileAccess::ACCESS_FILESYSTEM);

#ifdef TOOLS_ENABLED
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_RESOURCES);
#else
	if (use_apk_expansion) {
		DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_RESOURCES);
	} else {
		DirAccess::make_default<DirAccessJAndroid>(DirAccess::ACCESS_RESOURCES);
	}
#endif
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessJAndroid>(DirAccess::ACCESS_FILESYSTEM);

	NetSocketAndroid::make_default();
}

void OS_Android::initialize() {
	initialize_core();
}

void OS_Android::initialize_joypads() {
	Input::get_singleton()->set_fallback_mapping(godot_java->get_input_fallback_mapping());

	// This queries/updates the currently connected devices/joypads.
	godot_java->init_input_devices();
}

void OS_Android::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
}

void OS_Android::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
		main_loop = nullptr;
	}
}

void OS_Android::finalize() {
}

OS_Android *OS_Android::get_singleton() {
	return static_cast<OS_Android *>(OS::get_singleton());
}

GodotJavaWrapper *OS_Android::get_godot_java() {
	return godot_java;
}

GodotIOJavaWrapper *OS_Android::get_godot_io_java() {
	return godot_io_java;
}

bool OS_Android::request_permission(const String &p_name) {
	return godot_java->request_permission(p_name);
}

bool OS_Android::request_permissions() {
	return godot_java->request_permissions();
}

Vector<String> OS_Android::get_granted_permissions() const {
	return godot_java->get_granted_permissions();
}

bool OS_Android::copy_dynamic_library(const String &p_library_path, const String &p_target_dir, String *r_copy_path) {
	if (!FileAccess::exists(p_library_path)) {
		return false;
	}

	Ref<DirAccess> da_ref = DirAccess::create_for_path(p_library_path);
	if (da_ref.is_null()) {
		return false;
	}

	String copy_path = p_target_dir.path_join(p_library_path.get_file());
	bool copy_exists = FileAccess::exists(copy_path);
	if (copy_exists) {
		print_verbose("Deleting existing library copy " + copy_path);
		if (da_ref->remove(copy_path) != OK) {
			print_verbose("Unable to delete " + copy_path);
		}
	}

	print_verbose("Copying " + p_library_path + " to " + p_target_dir);
	Error create_dir_result = da_ref->make_dir_recursive(p_target_dir);
	if (create_dir_result == OK || create_dir_result == ERR_ALREADY_EXISTS) {
		copy_exists = da_ref->copy(p_library_path, copy_path) == OK;
	}

	if (copy_exists && r_copy_path != nullptr) {
		*r_copy_path = copy_path;
	}

	return copy_exists;
}

Error OS_Android::open_dynamic_library(const String &p_path, void *&p_library_handle, GDExtensionData *p_data) {
	String path = p_path;
	bool so_file_exists = true;
	if (!FileAccess::exists(path)) {
		path = p_path.get_file();
		so_file_exists = false;
	}

	p_library_handle = dlopen(path.utf8().get_data(), RTLD_NOW);
	if (!p_library_handle && so_file_exists) {
		// The library (and its dependencies) may be on the sdcard and thus inaccessible.
		// Try to copy to the internal directory for access.
		const String dynamic_library_path = get_dynamic_libraries_path();

		if (p_data != nullptr && p_data->library_dependencies != nullptr && !p_data->library_dependencies->is_empty()) {
			// Copy the library dependencies
			print_verbose("Copying library dependencies..");
			for (const String &library_dependency_path : *p_data->library_dependencies) {
				String internal_library_dependency_path;
				if (!copy_dynamic_library(library_dependency_path, dynamic_library_path.path_join(library_dependency_path.get_base_dir()), &internal_library_dependency_path)) {
					ERR_PRINT(vformat("Unable to copy library dependency %s", library_dependency_path));
				} else {
					void *lib_dependency_handle = dlopen(internal_library_dependency_path.utf8().get_data(), RTLD_NOW);
					if (!lib_dependency_handle) {
						ERR_PRINT(vformat("Can't open dynamic library dependency: %s. Error: %s.", internal_library_dependency_path, dlerror()));
					}
				}
			}
		}

		String internal_path;
		print_verbose("Copying library " + p_path);
		const bool internal_so_file_exists = copy_dynamic_library(p_path, dynamic_library_path.path_join(p_path.get_base_dir()), &internal_path);

		if (internal_so_file_exists) {
			print_verbose("Opening library " + internal_path);
			p_library_handle = dlopen(internal_path.utf8().get_data(), RTLD_NOW);
			if (p_library_handle) {
				path = internal_path;
			}
		}
	}

	ERR_FAIL_NULL_V_MSG(p_library_handle, ERR_CANT_OPEN, vformat("Can't open dynamic library: %s. Error: %s.", p_path, dlerror()));

	if (p_data != nullptr && p_data->r_resolved_path != nullptr) {
		*p_data->r_resolved_path = path;
	}

	return OK;
}

String OS_Android::get_name() const {
	return "Android";
}

String OS_Android::get_system_property(const char *key) const {
	String value;
	char value_str[PROP_VALUE_MAX];
	if (__system_property_get(key, value_str)) {
		value = String(value_str);
	}
	return value;
}

String OS_Android::get_distribution_name() const {
	if (!get_system_property("ro.havoc.version").is_empty()) {
		return "Havoc OS";
	} else if (!get_system_property("org.pex.version").is_empty()) { // Putting before "Pixel Experience", because it's derivating from it.
		return "Pixel Extended";
	} else if (!get_system_property("org.pixelexperience.version").is_empty()) {
		return "Pixel Experience";
	} else if (!get_system_property("ro.potato.version").is_empty()) {
		return "POSP";
	} else if (!get_system_property("ro.xtended.version").is_empty()) {
		return "Project-Xtended";
	} else if (!get_system_property("org.evolution.version").is_empty()) {
		return "Evolution X";
	} else if (!get_system_property("ro.corvus.version").is_empty()) {
		return "Corvus-Q";
	} else if (!get_system_property("ro.pa.version").is_empty()) {
		return "Paranoid Android";
	} else if (!get_system_property("ro.crdroid.version").is_empty()) {
		return "crDroid Android";
	} else if (!get_system_property("ro.syberia.version").is_empty()) {
		return "Syberia Project";
	} else if (!get_system_property("ro.arrow.version").is_empty()) {
		return "ArrowOS";
	} else if (!get_system_property("ro.lineage.version").is_empty()) { // Putting LineageOS last, just in case any derivative writes to "ro.lineage.version".
		return "LineageOS";
	}

	if (!get_system_property("ro.modversion").is_empty()) { // Handles other Android custom ROMs.
		return vformat("%s %s", get_name(), "Custom ROM");
	}

	// Handles stock Android.
	return get_name();
}

String OS_Android::get_version() const {
	const Vector<const char *> roms = { "ro.havoc.version", "org.pex.version", "org.pixelexperience.version",
		"ro.potato.version", "ro.xtended.version", "org.evolution.version", "ro.corvus.version", "ro.pa.version",
		"ro.crdroid.version", "ro.syberia.version", "ro.arrow.version", "ro.lineage.version" };
	for (int i = 0; i < roms.size(); i++) {
		String rom_version = get_system_property(roms[i]);
		if (!rom_version.is_empty()) {
			return rom_version;
		}
	}

	String mod_version = get_system_property("ro.modversion"); // Handles other Android custom ROMs.
	if (!mod_version.is_empty()) {
		return mod_version;
	}

	// Handles stock Android.
	String sdk_version = get_system_property("ro.build.version.sdk_int");
	String build = get_system_property("ro.build.version.incremental");
	if (!sdk_version.is_empty()) {
		if (!build.is_empty()) {
			return vformat("%s.%s", sdk_version, build);
		}
		return sdk_version;
	}

	return "";
}

MainLoop *OS_Android::get_main_loop() const {
	return main_loop;
}

void OS_Android::main_loop_begin() {
	if (main_loop) {
		main_loop->initialize();
	}
}

bool OS_Android::main_loop_iterate(bool *r_should_swap_buffers) {
	if (!main_loop) {
		return false;
	}
	DisplayServerAndroid::get_singleton()->reset_swap_buffers_flag();
	DisplayServerAndroid::get_singleton()->process_events();
	uint64_t current_frames_drawn = Engine::get_singleton()->get_frames_drawn();
	bool exit = Main::iteration();

	if (r_should_swap_buffers) {
		*r_should_swap_buffers = !is_in_low_processor_usage_mode() ||
				DisplayServerAndroid::get_singleton()->should_swap_buffers() ||
				RenderingServer::get_singleton()->has_changed() ||
				current_frames_drawn != Engine::get_singleton()->get_frames_drawn();
	}

	return exit;
}

void OS_Android::main_loop_end() {
	if (main_loop) {
		SceneTree *scene_tree = Object::cast_to<SceneTree>(main_loop);
		if (scene_tree) {
			scene_tree->quit();
		}
		main_loop->finalize();
	}
}

void OS_Android::main_loop_focusout() {
	DisplayServerAndroid::get_singleton()->send_window_event(DisplayServer::WINDOW_EVENT_FOCUS_OUT);
	if (OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_APPLICATION_FOCUS_OUT);
	}
	audio_driver_android.set_pause(true);
}

void OS_Android::main_loop_focusin() {
	DisplayServerAndroid::get_singleton()->send_window_event(DisplayServer::WINDOW_EVENT_FOCUS_IN);
	if (OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN);
	}
	audio_driver_android.set_pause(false);
}

Error OS_Android::shell_open(const String &p_uri) {
	return godot_io_java->open_uri(p_uri);
}

String OS_Android::get_resource_dir() const {
#ifdef TOOLS_ENABLED
	return OS_Unix::get_resource_dir();
#else
	if (remote_fs_dir.is_empty()) {
		return "/"; // Android has its own filesystem for resources inside the APK
	} else {
		return remote_fs_dir;
	}
#endif
}

String OS_Android::get_locale() const {
	String locale = godot_io_java->get_locale();
	if (!locale.is_empty()) {
		return locale;
	}

	return OS_Unix::get_locale();
}

String OS_Android::get_model_name() const {
	String model = godot_io_java->get_model();
	if (!model.is_empty()) {
		return model;
	}

	return OS_Unix::get_model_name();
}

String OS_Android::get_data_path() const {
	return OS::get_user_data_dir();
}

void OS_Android::_load_system_font_config() const {
	font_aliases.clear();
	fonts.clear();
	font_names.clear();

	Ref<XMLParser> parser;
	parser.instantiate();

	Error err = parser->open(String(getenv("ANDROID_ROOT")).path_join("/etc/fonts.xml"));
	if (err == OK) {
		bool in_font_node = false;
		String fb, fn;
		FontInfo fi;

		while (parser->read() == OK) {
			if (parser->get_node_type() == XMLParser::NODE_ELEMENT) {
				in_font_node = false;
				if (parser->get_node_name() == "familyset") {
					int ver = parser->has_attribute("version") ? parser->get_named_attribute_value("version").to_int() : 0;
					if (ver < 21) {
						ERR_PRINT(vformat("Unsupported font config version %s", ver));
						break;
					}
				} else if (parser->get_node_name() == "alias") {
					String name = parser->has_attribute("name") ? parser->get_named_attribute_value("name").strip_edges() : String();
					String to = parser->has_attribute("to") ? parser->get_named_attribute_value("to").strip_edges() : String();
					if (!name.is_empty() && !to.is_empty()) {
						font_aliases[name] = to;
					}
				} else if (parser->get_node_name() == "family") {
					fn = parser->has_attribute("name") ? parser->get_named_attribute_value("name").strip_edges() : String();
					String lang_code = parser->has_attribute("lang") ? parser->get_named_attribute_value("lang").strip_edges() : String();
					Vector<String> lang_codes = lang_code.split(",");
					for (int i = 0; i < lang_codes.size(); i++) {
						Vector<String> lang_code_elements = lang_codes[i].split("-");
						if (lang_code_elements.size() >= 1 && lang_code_elements[0] != "und") {
							// Add missing script codes.
							if (lang_code_elements[0] == "ko") {
								fi.script.insert("Hani");
								fi.script.insert("Hang");
							}
							if (lang_code_elements[0] == "ja") {
								fi.script.insert("Hani");
								fi.script.insert("Kana");
								fi.script.insert("Hira");
							}
							if (!lang_code_elements[0].is_empty()) {
								fi.lang.insert(lang_code_elements[0]);
							}
						}
						if (lang_code_elements.size() >= 2) {
							// Add common codes for variants and remove variants not supported by HarfBuzz/ICU.
							if (lang_code_elements[1] == "Aran") {
								fi.script.insert("Arab");
							}
							if (lang_code_elements[1] == "Cyrs") {
								fi.script.insert("Cyrl");
							}
							if (lang_code_elements[1] == "Hanb") {
								fi.script.insert("Hani");
								fi.script.insert("Bopo");
							}
							if (lang_code_elements[1] == "Hans" || lang_code_elements[1] == "Hant") {
								fi.script.insert("Hani");
							}
							if (lang_code_elements[1] == "Syrj" || lang_code_elements[1] == "Syre" || lang_code_elements[1] == "Syrn") {
								fi.script.insert("Syrc");
							}
							if (!lang_code_elements[1].is_empty() && lang_code_elements[1] != "Zsym" && lang_code_elements[1] != "Zsye" && lang_code_elements[1] != "Zmth") {
								fi.script.insert(lang_code_elements[1]);
							}
						}
					}
				} else if (parser->get_node_name() == "font") {
					in_font_node = true;
					fb = parser->has_attribute("fallbackFor") ? parser->get_named_attribute_value("fallbackFor").strip_edges() : String();
					fi.weight = parser->has_attribute("weight") ? parser->get_named_attribute_value("weight").to_int() : 400;
					fi.italic = parser->has_attribute("style") && parser->get_named_attribute_value("style").strip_edges() == "italic";
				}
			}
			if (parser->get_node_type() == XMLParser::NODE_TEXT) {
				if (in_font_node) {
					fi.filename = parser->get_node_data().strip_edges();
					fi.font_name = fn;
					if (!fb.is_empty() && fn.is_empty()) {
						fi.font_name = fb;
						fi.priority = 2;
					}
					if (fi.font_name.is_empty()) {
						fi.font_name = "sans-serif";
						fi.priority = 5;
					}
					if (fi.font_name.ends_with("-condensed")) {
						fi.stretch = 75;
						fi.font_name = fi.font_name.trim_suffix("-condensed");
					}
					fonts.push_back(fi);
					font_names.insert(fi.font_name);
				}
			}
			if (parser->get_node_type() == XMLParser::NODE_ELEMENT_END) {
				in_font_node = false;
				if (parser->get_node_name() == "font") {
					fb = String();
					fi.font_name = String();
					fi.priority = 0;
					fi.weight = 400;
					fi.stretch = 100;
					fi.italic = false;
				} else if (parser->get_node_name() == "family") {
					fi = FontInfo();
					fn = String();
				}
			}
		}
		parser->close();
	} else {
		ERR_PRINT("Unable to load font config");
	}

	font_config_loaded = true;
}

Vector<String> OS_Android::get_system_fonts() const {
	if (!font_config_loaded) {
		_load_system_font_config();
	}
	Vector<String> ret;
	for (const String &E : font_names) {
		ret.push_back(E);
	}
	return ret;
}

Vector<String> OS_Android::get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale, const String &p_script, int p_weight, int p_stretch, bool p_italic) const {
	if (!font_config_loaded) {
		_load_system_font_config();
	}
	String font_name = p_font_name.to_lower();
	if (font_aliases.has(font_name)) {
		font_name = font_aliases[font_name];
	}
	String root = String(getenv("ANDROID_ROOT")).path_join("fonts");
	String lang_prefix = p_locale.split("_")[0];
	Vector<String> ret;
	int best_score = 0;
	for (const List<FontInfo>::Element *E = fonts.front(); E; E = E->next()) {
		int score = 0;
		if (!E->get().script.is_empty() && !p_script.is_empty() && !E->get().script.has(p_script)) {
			continue;
		}
		float sim = E->get().font_name.similarity(font_name);
		if (sim > 0.0) {
			score += (60 * sim + 5 - E->get().priority);
		}
		if (E->get().lang.has(p_locale)) {
			score += 120;
		} else if (E->get().lang.has(lang_prefix)) {
			score += 115;
		}
		if (E->get().script.has(p_script)) {
			score += 240;
		}
		score += (20 - Math::abs(E->get().weight - p_weight) / 50);
		score += (20 - Math::abs(E->get().stretch - p_stretch) / 10);
		if (E->get().italic == p_italic) {
			score += 30;
		}
		if (score > best_score) {
			best_score = score;
			if (!ret.has(root.path_join(E->get().filename))) {
				ret.insert(0, root.path_join(E->get().filename));
			}
		} else if (score == best_score || E->get().script.is_empty()) {
			if (!ret.has(root.path_join(E->get().filename))) {
				ret.push_back(root.path_join(E->get().filename));
			}
		}
		if (score >= 490) {
			break; // Perfect match.
		}
	}

	return ret;
}

String OS_Android::get_system_font_path(const String &p_font_name, int p_weight, int p_stretch, bool p_italic) const {
	if (!font_config_loaded) {
		_load_system_font_config();
	}
	String font_name = p_font_name.to_lower();
	if (font_aliases.has(font_name)) {
		font_name = font_aliases[font_name];
	}
	String root = String(getenv("ANDROID_ROOT")).path_join("fonts");

	int best_score = 0;
	const List<FontInfo>::Element *best_match = nullptr;

	for (const List<FontInfo>::Element *E = fonts.front(); E; E = E->next()) {
		int score = 0;
		if (E->get().font_name == font_name) {
			score += (65 - E->get().priority);
		}
		score += (20 - Math::abs(E->get().weight - p_weight) / 50);
		score += (20 - Math::abs(E->get().stretch - p_stretch) / 10);
		if (E->get().italic == p_italic) {
			score += 30;
		}
		if (score >= 60 && score > best_score) {
			best_score = score;
			best_match = E;
		}
		if (score >= 140) {
			break; // Perfect match.
		}
	}
	if (best_match) {
		return root.path_join(best_match->get().filename);
	}
	return String();
}

String OS_Android::get_executable_path() const {
	// Since unix process creation is restricted on Android, we bypass
	// OS_Unix::get_executable_path() so we can return ANDROID_EXEC_PATH.
	// Detection of ANDROID_EXEC_PATH allows to handle process creation in an Android compliant
	// manner.
	return OS::get_executable_path();
}

String OS_Android::get_user_data_dir(const String &p_user_dir) const {
	if (!data_dir_cache.is_empty()) {
		return data_dir_cache;
	}

	String data_dir = godot_io_java->get_user_data_dir(p_user_dir);
	if (!data_dir.is_empty()) {
		data_dir_cache = _remove_symlink(data_dir);
		return data_dir_cache;
	}
	return ".";
}

String OS_Android::get_dynamic_libraries_path() const {
	return get_cache_path().path_join("dynamic_libraries");
}

String OS_Android::get_cache_path() const {
	if (!cache_dir_cache.is_empty()) {
		return cache_dir_cache;
	}

	String cache_dir = godot_io_java->get_cache_dir();
	if (!cache_dir.is_empty()) {
		cache_dir_cache = _remove_symlink(cache_dir);
		return cache_dir_cache;
	}
	return ".";
}

String OS_Android::get_temp_path() const {
	if (!temp_dir_cache.is_empty()) {
		return temp_dir_cache;
	}

	String temp_dir = godot_io_java->get_temp_dir();
	if (!temp_dir.is_empty()) {
		temp_dir_cache = _remove_symlink(temp_dir);
		return temp_dir_cache;
	}
	return ".";
}

String OS_Android::get_unique_id() const {
	String unique_id = godot_io_java->get_unique_id();
	if (!unique_id.is_empty()) {
		return unique_id;
	}

	return OS::get_unique_id();
}

String OS_Android::get_system_dir(SystemDir p_dir, bool p_shared_storage) const {
	return godot_io_java->get_system_dir(p_dir, p_shared_storage);
}

Error OS_Android::move_to_trash(const String &p_path) {
	Ref<DirAccess> da_ref = DirAccess::create_for_path(p_path);
	if (da_ref.is_null()) {
		return FAILED;
	}

	// Check if it's a directory
	if (da_ref->dir_exists(p_path)) {
		Error err = da_ref->change_dir(p_path);
		if (err) {
			return err;
		}
		// This is directory, let's erase its contents
		err = da_ref->erase_contents_recursive();
		if (err) {
			return err;
		}
		// Remove the top directory
		return da_ref->remove(p_path);
	} else if (da_ref->file_exists(p_path)) {
		// This is a file, let's remove it.
		return da_ref->remove(p_path);
	} else {
		return FAILED;
	}
}

void OS_Android::set_display_size(const Size2i &p_size) {
	display_size = p_size;
}

Size2i OS_Android::get_display_size() const {
	return display_size;
}

void OS_Android::set_opengl_extensions(const char *p_gl_extensions) {
#if defined(GLES3_ENABLED)
	ERR_FAIL_NULL(p_gl_extensions);
	gl_extensions = p_gl_extensions;
#endif
}

void OS_Android::set_native_window(ANativeWindow *p_native_window) {
#if defined(VULKAN_ENABLED)
	native_window = p_native_window;
#endif
}

ANativeWindow *OS_Android::get_native_window() const {
#if defined(VULKAN_ENABLED)
	return native_window;
#else
	return nullptr;
#endif
}

void OS_Android::vibrate_handheld(int p_duration_ms, float p_amplitude) {
	godot_java->vibrate(p_duration_ms, p_amplitude);
}

String OS_Android::get_config_path() const {
	return OS::get_user_data_dir().path_join("config");
}

void OS_Android::benchmark_begin_measure(const String &p_context, const String &p_what) {
#ifdef TOOLS_ENABLED
	godot_java->begin_benchmark_measure(p_context, p_what);
#endif
}

void OS_Android::benchmark_end_measure(const String &p_context, const String &p_what) {
#ifdef TOOLS_ENABLED
	godot_java->end_benchmark_measure(p_context, p_what);
#endif
}

void OS_Android::benchmark_dump() {
#ifdef TOOLS_ENABLED
	if (!is_use_benchmark_set()) {
		return;
	}
	godot_java->dump_benchmark(get_benchmark_file());
#endif
}

#ifdef TOOLS_ENABLED
Error OS_Android::sign_apk(const String &p_input_path, const String &p_output_path, const String &p_keystore_path, const String &p_keystore_user, const String &p_keystore_password) {
	return godot_java->sign_apk(p_input_path, p_output_path, p_keystore_path, p_keystore_user, p_keystore_password);
}

Error OS_Android::verify_apk(const String &p_apk_path) {
	return godot_java->verify_apk(p_apk_path);
}
#endif

bool OS_Android::_check_internal_feature_support(const String &p_feature) {
	if (p_feature == "macos" || p_feature == "web_ios" || p_feature == "web_macos" || p_feature == "windows") {
		return false;
	}

	if (p_feature == "system_fonts") {
		return true;
	}
	if (p_feature == "mobile") {
		return true;
	}
#if defined(__aarch64__)
	if (p_feature == "arm64-v8a" || p_feature == "arm64") {
		return true;
	}
#elif defined(__ARM_ARCH_7A__)
	if (p_feature == "armeabi-v7a" || p_feature == "armeabi" || p_feature == "arm32") {
		return true;
	}
#elif defined(__arm__)
	if (p_feature == "armeabi" || p_feature == "arm") {
		return true;
	}
#endif

	if (godot_java->has_feature(p_feature)) {
		return true;
	}

	return false;
}

OS_Android::OS_Android(GodotJavaWrapper *p_godot_java, GodotIOJavaWrapper *p_godot_io_java, bool p_use_apk_expansion) {
	display_size.width = DEFAULT_WINDOW_WIDTH;
	display_size.height = DEFAULT_WINDOW_HEIGHT;

	use_apk_expansion = p_use_apk_expansion;

	main_loop = nullptr;

#if defined(GLES3_ENABLED)
	gl_extensions = nullptr;
#endif

#if defined(VULKAN_ENABLED)
	native_window = nullptr;
#endif

	godot_java = p_godot_java;
	godot_io_java = p_godot_io_java;

	Vector<Logger *> loggers;
	loggers.push_back(memnew(AndroidLogger));
	_set_logger(memnew(CompositeLogger(loggers)));

	AudioDriverManager::add_driver(&audio_driver_android);

	DisplayServerAndroid::register_android_driver();
}

Error OS_Android::execute(const String &p_path, const List<String> &p_arguments, String *r_pipe, int *r_exitcode, bool read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
	if (p_path == ANDROID_EXEC_PATH) {
		return create_instance(p_arguments);
	} else {
		return OS_Unix::execute(p_path, p_arguments, r_pipe, r_exitcode, read_stderr, p_pipe_mutex, p_open_console);
	}
}

Error OS_Android::create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) {
	if (p_path == ANDROID_EXEC_PATH) {
		return create_instance(p_arguments, r_child_id);
	} else {
		return OS_Unix::create_process(p_path, p_arguments, r_child_id, p_open_console);
	}
}

Error OS_Android::create_instance(const List<String> &p_arguments, ProcessID *r_child_id) {
	int instance_id = godot_java->create_new_godot_instance(p_arguments);
	if (instance_id == -1) {
		return FAILED;
	}
	if (r_child_id) {
		*r_child_id = instance_id;
	}
	return OK;
}

Error OS_Android::kill(const ProcessID &p_pid) {
	if (godot_java->force_quit(nullptr, p_pid)) {
		return OK;
	}
	return OS_Unix::kill(p_pid);
}

String OS_Android::get_system_ca_certificates() {
	return godot_java->get_ca_certificates();
}

Error OS_Android::setup_remote_filesystem(const String &p_server_host, int p_port, const String &p_password, String &r_project_path) {
	r_project_path = OS::get_user_data_dir();
	Error err = OS_Unix::setup_remote_filesystem(p_server_host, p_port, p_password, r_project_path);
	if (err == OK) {
		remote_fs_dir = r_project_path;
		FileAccess::make_default<FileAccessFilesystemJAndroid>(FileAccess::ACCESS_RESOURCES);
	}
	return err;
}

void OS_Android::load_platform_gdextensions() const {
	Vector<String> extension_list_config_file = godot_java->get_gdextension_list_config_file();
	for (String config_file_path : extension_list_config_file) {
		GDExtensionManager::LoadStatus err = GDExtensionManager::get_singleton()->load_extension(config_file_path);
		ERR_CONTINUE_MSG(err == GDExtensionManager::LOAD_STATUS_FAILED, "Error loading platform extension: " + config_file_path);
	}
}

OS_Android::~OS_Android() {
}
