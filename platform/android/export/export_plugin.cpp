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

#include "gradle_export_util.h"
#include "logo_svg.gen.h"
#include "run_icon_svg.gen.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image_loader.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/version.h"
#include "drivers/png/png_driver_common.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/export/export_template_manager.h"
#include "editor/import/resource_importer_texture_settings.h"
#include "editor/themes/editor_scale.h"
#include "main/splash.gen.h"
#include "scene/resources/image_texture.h"

#include "modules/modules_enabled.gen.h" // For mono.
#include "modules/svg/image_loader_svg.h"

#ifdef ANDROID_ENABLED
#include "../os_android.h"
#endif

#include <string.h>

static const char *ANDROID_PERMS[] = {
	"ACCESS_CHECKIN_PROPERTIES",
	"ACCESS_COARSE_LOCATION",
	"ACCESS_FINE_LOCATION",
	"ACCESS_LOCATION_EXTRA_COMMANDS",
	"ACCESS_MEDIA_LOCATION",
	"ACCESS_MOCK_LOCATION",
	"ACCESS_NETWORK_STATE",
	"ACCESS_SURFACE_FLINGER",
	"ACCESS_WIFI_STATE",
	"ACCOUNT_MANAGER",
	"ADD_VOICEMAIL",
	"AUTHENTICATE_ACCOUNTS",
	"BATTERY_STATS",
	"BIND_ACCESSIBILITY_SERVICE",
	"BIND_APPWIDGET",
	"BIND_DEVICE_ADMIN",
	"BIND_INPUT_METHOD",
	"BIND_NFC_SERVICE",
	"BIND_NOTIFICATION_LISTENER_SERVICE",
	"BIND_PRINT_SERVICE",
	"BIND_REMOTEVIEWS",
	"BIND_TEXT_SERVICE",
	"BIND_VPN_SERVICE",
	"BIND_WALLPAPER",
	"BLUETOOTH",
	"BLUETOOTH_ADMIN",
	"BLUETOOTH_PRIVILEGED",
	"BRICK",
	"BROADCAST_PACKAGE_REMOVED",
	"BROADCAST_SMS",
	"BROADCAST_STICKY",
	"BROADCAST_WAP_PUSH",
	"CALL_PHONE",
	"CALL_PRIVILEGED",
	"CAMERA",
	"CAPTURE_AUDIO_OUTPUT",
	"CAPTURE_SECURE_VIDEO_OUTPUT",
	"CAPTURE_VIDEO_OUTPUT",
	"CHANGE_COMPONENT_ENABLED_STATE",
	"CHANGE_CONFIGURATION",
	"CHANGE_NETWORK_STATE",
	"CHANGE_WIFI_MULTICAST_STATE",
	"CHANGE_WIFI_STATE",
	"CLEAR_APP_CACHE",
	"CLEAR_APP_USER_DATA",
	"CONTROL_LOCATION_UPDATES",
	"DELETE_CACHE_FILES",
	"DELETE_PACKAGES",
	"DEVICE_POWER",
	"DIAGNOSTIC",
	"DISABLE_KEYGUARD",
	"DUMP",
	"EXPAND_STATUS_BAR",
	"FACTORY_TEST",
	"FLASHLIGHT",
	"FORCE_BACK",
	"GET_ACCOUNTS",
	"GET_PACKAGE_SIZE",
	"GET_TASKS",
	"GET_TOP_ACTIVITY_INFO",
	"GLOBAL_SEARCH",
	"HARDWARE_TEST",
	"INJECT_EVENTS",
	"INSTALL_LOCATION_PROVIDER",
	"INSTALL_PACKAGES",
	"INSTALL_SHORTCUT",
	"INTERNAL_SYSTEM_WINDOW",
	"INTERNET",
	"KILL_BACKGROUND_PROCESSES",
	"LOCATION_HARDWARE",
	"MANAGE_ACCOUNTS",
	"MANAGE_APP_TOKENS",
	"MANAGE_DOCUMENTS",
	"MANAGE_EXTERNAL_STORAGE",
	"MASTER_CLEAR",
	"MEDIA_CONTENT_CONTROL",
	"MODIFY_AUDIO_SETTINGS",
	"MODIFY_PHONE_STATE",
	"MOUNT_FORMAT_FILESYSTEMS",
	"MOUNT_UNMOUNT_FILESYSTEMS",
	"NFC",
	"PERSISTENT_ACTIVITY",
	"POST_NOTIFICATIONS",
	"PROCESS_OUTGOING_CALLS",
	"READ_CALENDAR",
	"READ_CALL_LOG",
	"READ_CONTACTS",
	"READ_EXTERNAL_STORAGE",
	"READ_FRAME_BUFFER",
	"READ_HISTORY_BOOKMARKS",
	"READ_INPUT_STATE",
	"READ_LOGS",
	"READ_MEDIA_AUDIO",
	"READ_MEDIA_IMAGES",
	"READ_MEDIA_VIDEO",
	"READ_MEDIA_VISUAL_USER_SELECTED",
	"READ_PHONE_STATE",
	"READ_PROFILE",
	"READ_SMS",
	"READ_SOCIAL_STREAM",
	"READ_SYNC_SETTINGS",
	"READ_SYNC_STATS",
	"READ_USER_DICTIONARY",
	"REBOOT",
	"RECEIVE_BOOT_COMPLETED",
	"RECEIVE_MMS",
	"RECEIVE_SMS",
	"RECEIVE_WAP_PUSH",
	"RECORD_AUDIO",
	"REORDER_TASKS",
	"RESTART_PACKAGES",
	"SEND_RESPOND_VIA_MESSAGE",
	"SEND_SMS",
	"SET_ACTIVITY_WATCHER",
	"SET_ALARM",
	"SET_ALWAYS_FINISH",
	"SET_ANIMATION_SCALE",
	"SET_DEBUG_APP",
	"SET_ORIENTATION",
	"SET_POINTER_SPEED",
	"SET_PREFERRED_APPLICATIONS",
	"SET_PROCESS_LIMIT",
	"SET_TIME",
	"SET_TIME_ZONE",
	"SET_WALLPAPER",
	"SET_WALLPAPER_HINTS",
	"SIGNAL_PERSISTENT_PROCESSES",
	"STATUS_BAR",
	"SUBSCRIBED_FEEDS_READ",
	"SUBSCRIBED_FEEDS_WRITE",
	"SYSTEM_ALERT_WINDOW",
	"TRANSMIT_IR",
	"UNINSTALL_SHORTCUT",
	"UPDATE_DEVICE_STATS",
	"USE_CREDENTIALS",
	"USE_SIP",
	"VIBRATE",
	"WAKE_LOCK",
	"WRITE_APN_SETTINGS",
	"WRITE_CALENDAR",
	"WRITE_CALL_LOG",
	"WRITE_CONTACTS",
	"WRITE_EXTERNAL_STORAGE",
	"WRITE_GSERVICES",
	"WRITE_HISTORY_BOOKMARKS",
	"WRITE_PROFILE",
	"WRITE_SECURE_SETTINGS",
	"WRITE_SETTINGS",
	"WRITE_SMS",
	"WRITE_SOCIAL_STREAM",
	"WRITE_SYNC_SETTINGS",
	"WRITE_USER_DICTIONARY",
	nullptr
};

static const char *MISMATCHED_VERSIONS_MESSAGE = "Android build version mismatch:\n| Template installed: %s\n| Requested version: %s\nPlease reinstall Android build template from 'Project' menu.";

static const char *GDEXTENSION_LIBS_PATH = "libs/gdextensionlibs.json";

// This template string must be in sync with the content of 'platform/android/java/lib/res/mipmap-anydpi-v26/icon.xml'.
static const String ICON_XML_TEMPLATE =
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"<adaptive-icon xmlns:android=\"http://schemas.android.com/apk/res/android\">\n"
		"    <background android:drawable=\"@mipmap/icon_background\"/>\n"
		"    <foreground android:drawable=\"@mipmap/icon_foreground\"/>\n"
		"%s" // Placeholder for the optional monochrome tag.
		"</adaptive-icon>";

static const String ICON_XML_PATH = "res/mipmap-anydpi-v26/icon.xml";
static const String THEMED_ICON_XML_PATH = "res/mipmap-anydpi-v26/themed_icon.xml";

static const int ICON_DENSITIES_COUNT = 6;
static const char *LAUNCHER_ICON_OPTION = PNAME("launcher_icons/main_192x192");
static const char *LAUNCHER_ADAPTIVE_ICON_FOREGROUND_OPTION = PNAME("launcher_icons/adaptive_foreground_432x432");
static const char *LAUNCHER_ADAPTIVE_ICON_BACKGROUND_OPTION = PNAME("launcher_icons/adaptive_background_432x432");
static const char *LAUNCHER_ADAPTIVE_ICON_MONOCHROME_OPTION = PNAME("launcher_icons/adaptive_monochrome_432x432");

static const LauncherIcon LAUNCHER_ICONS[ICON_DENSITIES_COUNT] = {
	{ "res/mipmap-xxxhdpi-v4/icon.png", 192 },
	{ "res/mipmap-xxhdpi-v4/icon.png", 144 },
	{ "res/mipmap-xhdpi-v4/icon.png", 96 },
	{ "res/mipmap-hdpi-v4/icon.png", 72 },
	{ "res/mipmap-mdpi-v4/icon.png", 48 },
	{ "res/mipmap/icon.png", 192 }
};

static const LauncherIcon LAUNCHER_ADAPTIVE_ICON_FOREGROUNDS[ICON_DENSITIES_COUNT] = {
	{ "res/mipmap-xxxhdpi-v4/icon_foreground.png", 432 },
	{ "res/mipmap-xxhdpi-v4/icon_foreground.png", 324 },
	{ "res/mipmap-xhdpi-v4/icon_foreground.png", 216 },
	{ "res/mipmap-hdpi-v4/icon_foreground.png", 162 },
	{ "res/mipmap-mdpi-v4/icon_foreground.png", 108 },
	{ "res/mipmap/icon_foreground.png", 432 }
};

static const LauncherIcon LAUNCHER_ADAPTIVE_ICON_BACKGROUNDS[ICON_DENSITIES_COUNT] = {
	{ "res/mipmap-xxxhdpi-v4/icon_background.png", 432 },
	{ "res/mipmap-xxhdpi-v4/icon_background.png", 324 },
	{ "res/mipmap-xhdpi-v4/icon_background.png", 216 },
	{ "res/mipmap-hdpi-v4/icon_background.png", 162 },
	{ "res/mipmap-mdpi-v4/icon_background.png", 108 },
	{ "res/mipmap/icon_background.png", 432 }
};

static const LauncherIcon LAUNCHER_ADAPTIVE_ICON_MONOCHROMES[ICON_DENSITIES_COUNT] = {
	{ "res/mipmap-xxxhdpi-v4/icon_monochrome.png", 432 },
	{ "res/mipmap-xxhdpi-v4/icon_monochrome.png", 324 },
	{ "res/mipmap-xhdpi-v4/icon_monochrome.png", 216 },
	{ "res/mipmap-hdpi-v4/icon_monochrome.png", 162 },
	{ "res/mipmap-mdpi-v4/icon_monochrome.png", 108 },
	{ "res/mipmap/icon_monochrome.png", 432 }
};

static const int EXPORT_FORMAT_APK = 0;
static const int EXPORT_FORMAT_AAB = 1;

static const char *APK_ASSETS_DIRECTORY = "assets";
static const char *AAB_ASSETS_DIRECTORY = "assetPacks/installTime/src/main/assets";

static const int OPENGL_MIN_SDK_VERSION = 21; // Should match the value in 'platform/android/java/app/config.gradle#minSdk'
static const int VULKAN_MIN_SDK_VERSION = 24;
static const int DEFAULT_TARGET_SDK_VERSION = 34; // Should match the value in 'platform/android/java/app/config.gradle#targetSdk'

#ifndef ANDROID_ENABLED
void EditorExportPlatformAndroid::_check_for_changes_poll_thread(void *ud) {
	EditorExportPlatformAndroid *ea = static_cast<EditorExportPlatformAndroid *>(ud);

	while (!ea->quit_request.is_set()) {
#ifndef DISABLE_DEPRECATED
		// Check for android plugins updates
		{
			// Nothing to do if we already know the plugins have changed.
			if (!ea->android_plugins_changed.is_set()) {
				Vector<PluginConfigAndroid> loaded_plugins = get_plugins();

				MutexLock lock(ea->android_plugins_lock);

				if (ea->android_plugins.size() != loaded_plugins.size()) {
					ea->android_plugins_changed.set();
				} else {
					for (int i = 0; i < ea->android_plugins.size(); i++) {
						if (ea->android_plugins[i].name != loaded_plugins[i].name) {
							ea->android_plugins_changed.set();
							break;
						}
					}
				}

				if (ea->android_plugins_changed.is_set()) {
					ea->android_plugins = loaded_plugins;
				}
			}
		}
#endif // DISABLE_DEPRECATED

		// Check for devices updates
		String adb = get_adb_path();
		// adb.exe was locking the editor_doc_cache file on startup. Adding a check for is_editor_ready provides just enough time
		// to regenerate the doc cache.
		if (ea->has_runnable_preset.is_set() && FileAccess::exists(adb) && EditorNode::get_singleton()->is_editor_ready()) {
			String devices;
			List<String> args;
			args.push_back("devices");
			int ec;
			OS::get_singleton()->execute(adb, args, &devices, &ec);

			Vector<String> ds = devices.split("\n");
			Vector<String> ldevices;
			for (int i = 1; i < ds.size(); i++) {
				String d = ds[i];
				int dpos = d.find("device");
				if (dpos == -1) {
					continue;
				}
				d = d.substr(0, dpos).strip_edges();
				ldevices.push_back(d);
			}

			MutexLock lock(ea->device_lock);

			bool different = false;

			if (ea->devices.size() != ldevices.size()) {
				different = true;
			} else {
				for (int i = 0; i < ea->devices.size(); i++) {
					if (ea->devices[i].id != ldevices[i]) {
						different = true;
						break;
					}
				}
			}

			if (different) {
				Vector<Device> ndevices;

				for (int i = 0; i < ldevices.size(); i++) {
					Device d;
					d.id = ldevices[i];
					for (int j = 0; j < ea->devices.size(); j++) {
						if (ea->devices[j].id == ldevices[i]) {
							d.description = ea->devices[j].description;
							d.name = ea->devices[j].name;
							d.api_level = ea->devices[j].api_level;
						}
					}

					if (d.description.is_empty()) {
						//in the oven, request!
						args.clear();
						args.push_back("-s");
						args.push_back(d.id);
						args.push_back("shell");
						args.push_back("getprop");
						int ec2;
						String dp;

						OS::get_singleton()->execute(adb, args, &dp, &ec2);

						Vector<String> props = dp.split("\n");
						String vendor;
						String device;
						d.description = "Device ID: " + d.id + "\n";
						d.api_level = 0;
						for (int j = 0; j < props.size(); j++) {
							// got information by `shell cat /system/build.prop` before and its format is "property=value"
							// it's now changed to use `shell getporp` because of permission issue with Android 8.0 and above
							// its format is "[property]: [value]" so changed it as like build.prop
							String p = props[j];
							p = p.replace("]: ", "=");
							p = p.replace("[", "");
							p = p.replace("]", "");

							if (p.begins_with("ro.product.model=")) {
								device = p.get_slice("=", 1).strip_edges();
							} else if (p.begins_with("ro.product.brand=")) {
								vendor = p.get_slice("=", 1).strip_edges().capitalize();
							} else if (p.begins_with("ro.build.display.id=")) {
								d.description += "Build: " + p.get_slice("=", 1).strip_edges() + "\n";
							} else if (p.begins_with("ro.build.version.release=")) {
								d.description += "Release: " + p.get_slice("=", 1).strip_edges() + "\n";
							} else if (p.begins_with("ro.build.version.sdk=")) {
								d.api_level = p.get_slice("=", 1).to_int();
							} else if (p.begins_with("ro.product.cpu.abi=")) {
								d.architecture = p.get_slice("=", 1).strip_edges();
								d.description += "CPU: " + d.architecture + "\n";
							} else if (p.begins_with("ro.product.manufacturer=")) {
								d.description += "Manufacturer: " + p.get_slice("=", 1).strip_edges() + "\n";
							} else if (p.begins_with("ro.board.platform=")) {
								d.description += "Chipset: " + p.get_slice("=", 1).strip_edges() + "\n";
							} else if (p.begins_with("ro.opengles.version=")) {
								uint32_t opengl = p.get_slice("=", 1).to_int();
								d.description += "OpenGL: " + itos(opengl >> 16) + "." + itos((opengl >> 8) & 0xFF) + "." + itos((opengl) & 0xFF) + "\n";
							}
						}

						d.name = vendor + " " + device;
						if (device.is_empty()) {
							continue;
						}
					}

					ndevices.push_back(d);
				}

				ea->devices = ndevices;
				ea->devices_changed.set();
			}
		}

		uint64_t sleep = 200;
		uint64_t wait = 3000000;
		uint64_t time = OS::get_singleton()->get_ticks_usec();
		while (OS::get_singleton()->get_ticks_usec() - time < wait) {
			OS::get_singleton()->delay_usec(1000 * sleep);
			if (ea->quit_request.is_set()) {
				break;
			}
		}
	}

	if (ea->has_runnable_preset.is_set() && EDITOR_GET("export/android/shutdown_adb_on_exit")) {
		String adb = get_adb_path();
		if (!FileAccess::exists(adb)) {
			return; //adb not configured
		}

		List<String> args;
		args.push_back("kill-server");
		OS::get_singleton()->execute(adb, args);
	}
}

void EditorExportPlatformAndroid::_update_preset_status() {
	const int preset_count = EditorExport::get_singleton()->get_export_preset_count();
	bool has_runnable = false;

	for (int i = 0; i < preset_count; i++) {
		const Ref<EditorExportPreset> &preset = EditorExport::get_singleton()->get_export_preset(i);
		if (preset->get_platform() == this && preset->is_runnable()) {
			has_runnable = true;
			break;
		}
	}

	if (has_runnable) {
		has_runnable_preset.set();
	} else {
		has_runnable_preset.clear();
	}
	devices_changed.set();
}
#endif

String EditorExportPlatformAndroid::get_project_name(const String &p_name) const {
	String aname;
	if (!p_name.is_empty()) {
		aname = p_name;
	} else {
		aname = GLOBAL_GET("application/config/name");
	}

	if (aname.is_empty()) {
		aname = VERSION_NAME;
	}

	return aname;
}

String EditorExportPlatformAndroid::get_package_name(const String &p_package) const {
	String pname = p_package;
	String name = get_valid_basename();
	pname = pname.replace("$genname", name);
	return pname;
}

// Returns the project name without invalid characters
// or the "noname" string if all characters are invalid.
String EditorExportPlatformAndroid::get_valid_basename() const {
	String basename = GLOBAL_GET("application/config/name");
	basename = basename.to_lower();

	String name;
	bool first = true;
	for (int i = 0; i < basename.length(); i++) {
		char32_t c = basename[i];
		if (is_digit(c) && first) {
			continue;
		}
		if (is_ascii_identifier_char(c)) {
			name += String::chr(c);
			first = false;
		}
	}

	if (name.is_empty()) {
		name = "noname";
	}

	return name;
}

String EditorExportPlatformAndroid::get_assets_directory(const Ref<EditorExportPreset> &p_preset, int p_export_format) const {
	String gradle_build_directory = ExportTemplateManager::get_android_build_directory(p_preset);
	return gradle_build_directory.path_join(p_export_format == EXPORT_FORMAT_AAB ? AAB_ASSETS_DIRECTORY : APK_ASSETS_DIRECTORY);
}

bool EditorExportPlatformAndroid::is_package_name_valid(const String &p_package, String *r_error) const {
	String pname = get_package_name(p_package);

	if (pname.length() == 0) {
		if (r_error) {
			*r_error = TTR("Package name is missing.");
		}
		return false;
	}

	int segments = 0;
	bool first = true;
	for (int i = 0; i < pname.length(); i++) {
		char32_t c = pname[i];
		if (first && c == '.') {
			if (r_error) {
				*r_error = TTR("Package segments must be of non-zero length.");
			}
			return false;
		}
		if (c == '.') {
			segments++;
			first = true;
			continue;
		}
		if (!is_ascii_identifier_char(c)) {
			if (r_error) {
				*r_error = vformat(TTR("The character '%s' is not allowed in Android application package names."), String::chr(c));
			}
			return false;
		}
		if (first && is_digit(c)) {
			if (r_error) {
				*r_error = TTR("A digit cannot be the first character in a package segment.");
			}
			return false;
		}
		if (first && is_underscore(c)) {
			if (r_error) {
				*r_error = vformat(TTR("The character '%s' cannot be the first character in a package segment."), String::chr(c));
			}
			return false;
		}
		first = false;
	}

	if (segments == 0) {
		if (r_error) {
			*r_error = TTR("The package must have at least one '.' separator.");
		}
		return false;
	}

	if (first) {
		if (r_error) {
			*r_error = TTR("Package segments must be of non-zero length.");
		}
		return false;
	}

	return true;
}

bool EditorExportPlatformAndroid::is_project_name_valid() const {
	// Get the original project name and convert to lowercase.
	String basename = GLOBAL_GET("application/config/name");
	basename = basename.to_lower();
	// Check if there are invalid characters.
	if (basename != get_valid_basename()) {
		return false;
	}
	return true;
}

bool EditorExportPlatformAndroid::_should_compress_asset(const String &p_path, const Vector<uint8_t> &p_data) {
	/*
	 *  By not compressing files with little or no benefit in doing so,
	 *  a performance gain is expected at runtime. Moreover, if the APK is
	 *  zip-aligned, assets stored as they are can be efficiently read by
	 *  Android by memory-mapping them.
	 */

	// -- Unconditional uncompress to mimic AAPT plus some other

	static const char *unconditional_compress_ext[] = {
		// From https://github.com/android/platform_frameworks_base/blob/master/tools/aapt/Package.cpp
		// These formats are already compressed, or don't compress well:
		".jpg", ".jpeg", ".png", ".gif",
		".wav", ".mp2", ".mp3", ".ogg", ".aac",
		".mpg", ".mpeg", ".mid", ".midi", ".smf", ".jet",
		".rtttl", ".imy", ".xmf", ".mp4", ".m4a",
		".m4v", ".3gp", ".3gpp", ".3g2", ".3gpp2",
		".amr", ".awb", ".wma", ".wmv",
		// Godot-specific:
		".webp", // Same reasoning as .png
		".cfb", // Don't let small config files slow-down startup
		".scn", // Binary scenes are usually already compressed
		".ctex", // Streamable textures are usually already compressed
		// Trailer for easier processing
		nullptr
	};

	for (const char **ext = unconditional_compress_ext; *ext; ++ext) {
		if (p_path.to_lower().ends_with(String(*ext))) {
			return false;
		}
	}

	// -- Compressed resource?

	if (p_data.size() >= 4 && p_data[0] == 'R' && p_data[1] == 'S' && p_data[2] == 'C' && p_data[3] == 'C') {
		// Already compressed
		return false;
	}

	// --- TODO: Decide on texture resources according to their image compression setting

	return true;
}

zip_fileinfo EditorExportPlatformAndroid::get_zip_fileinfo() {
	OS::DateTime dt = OS::get_singleton()->get_datetime();

	zip_fileinfo zipfi;
	zipfi.tmz_date.tm_year = dt.year;
	zipfi.tmz_date.tm_mon = dt.month - 1; // tm_mon is zero indexed
	zipfi.tmz_date.tm_mday = dt.day;
	zipfi.tmz_date.tm_hour = dt.hour;
	zipfi.tmz_date.tm_min = dt.minute;
	zipfi.tmz_date.tm_sec = dt.second;
	zipfi.dosDate = 0;
	zipfi.external_fa = 0;
	zipfi.internal_fa = 0;

	return zipfi;
}

Vector<EditorExportPlatformAndroid::ABI> EditorExportPlatformAndroid::get_abis() {
	// Should have the same order and size as get_archs.
	Vector<ABI> abis;
	abis.push_back(ABI("armeabi-v7a", "arm32"));
	abis.push_back(ABI("arm64-v8a", "arm64"));
	abis.push_back(ABI("x86", "x86_32"));
	abis.push_back(ABI("x86_64", "x86_64"));
	return abis;
}

#ifndef DISABLE_DEPRECATED
/// List the gdap files in the directory specified by the p_path parameter.
Vector<String> EditorExportPlatformAndroid::list_gdap_files(const String &p_path) {
	Vector<String> dir_files;
	Ref<DirAccess> da = DirAccess::open(p_path);
	if (da.is_valid()) {
		da->list_dir_begin();
		while (true) {
			String file = da->get_next();
			if (file.is_empty()) {
				break;
			}

			if (da->current_is_dir() || da->current_is_hidden()) {
				continue;
			}

			if (file.ends_with(PluginConfigAndroid::PLUGIN_CONFIG_EXT)) {
				dir_files.push_back(file);
			}
		}
		da->list_dir_end();
	}

	return dir_files;
}

Vector<PluginConfigAndroid> EditorExportPlatformAndroid::get_plugins() {
	Vector<PluginConfigAndroid> loaded_plugins;

	String plugins_dir = ProjectSettings::get_singleton()->get_resource_path().path_join("android/plugins");

	// Add the prebuilt plugins
	loaded_plugins.append_array(PluginConfigAndroid::get_prebuilt_plugins(plugins_dir));

	if (DirAccess::exists(plugins_dir)) {
		Vector<String> plugins_filenames = list_gdap_files(plugins_dir);

		if (!plugins_filenames.is_empty()) {
			Ref<ConfigFile> config_file = memnew(ConfigFile);
			for (int i = 0; i < plugins_filenames.size(); i++) {
				PluginConfigAndroid config = PluginConfigAndroid::load_plugin_config(config_file, plugins_dir.path_join(plugins_filenames[i]));
				if (config.valid_config) {
					loaded_plugins.push_back(config);
				} else {
					print_error("Invalid plugin config file " + plugins_filenames[i]);
				}
			}
		}
	}

	return loaded_plugins;
}

Vector<PluginConfigAndroid> EditorExportPlatformAndroid::get_enabled_plugins(const Ref<EditorExportPreset> &p_presets) {
	Vector<PluginConfigAndroid> enabled_plugins;
	Vector<PluginConfigAndroid> all_plugins = get_plugins();
	for (int i = 0; i < all_plugins.size(); i++) {
		PluginConfigAndroid plugin = all_plugins[i];
		bool enabled = p_presets->get("plugins/" + plugin.name);
		if (enabled) {
			enabled_plugins.push_back(plugin);
		}
	}

	return enabled_plugins;
}
#endif // DISABLE_DEPRECATED

Error EditorExportPlatformAndroid::store_in_apk(APKExportData *ed, const String &p_path, const Vector<uint8_t> &p_data, int compression_method) {
	zip_fileinfo zipfi = get_zip_fileinfo();
	zipOpenNewFileInZip(ed->apk,
			p_path.utf8().get_data(),
			&zipfi,
			nullptr,
			0,
			nullptr,
			0,
			nullptr,
			compression_method,
			Z_DEFAULT_COMPRESSION);

	zipWriteInFileInZip(ed->apk, p_data.ptr(), p_data.size());
	zipCloseFileInZip(ed->apk);

	return OK;
}

Error EditorExportPlatformAndroid::save_apk_so(void *p_userdata, const SharedObject &p_so) {
	if (!p_so.path.get_file().begins_with("lib")) {
		String err = "Android .so file names must start with \"lib\", but got: " + p_so.path;
		ERR_PRINT(err);
		return FAILED;
	}
	APKExportData *ed = static_cast<APKExportData *>(p_userdata);
	Vector<ABI> abis = get_abis();
	bool exported = false;
	for (int i = 0; i < p_so.tags.size(); ++i) {
		// shared objects can be fat (compatible with multiple ABIs)
		int abi_index = -1;
		for (int j = 0; j < abis.size(); ++j) {
			if (abis[j].abi == p_so.tags[i] || abis[j].arch == p_so.tags[i]) {
				abi_index = j;
				break;
			}
		}
		if (abi_index != -1) {
			exported = true;
			String abi = abis[abi_index].abi;
			String dst_path = String("lib").path_join(abi).path_join(p_so.path.get_file());
			Vector<uint8_t> array = FileAccess::get_file_as_bytes(p_so.path);
			Error store_err = store_in_apk(ed, dst_path, array);
			ERR_FAIL_COND_V_MSG(store_err, store_err, "Cannot store in apk file '" + dst_path + "'.");
		}
	}
	if (!exported) {
		ERR_PRINT("Cannot determine architecture for library \"" + p_so.path + "\". One of the supported architectures must be used as a tag: " + join_abis(abis, " ", true));
		return FAILED;
	}
	return OK;
}

Error EditorExportPlatformAndroid::save_apk_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key, uint64_t p_seed) {
	APKExportData *ed = static_cast<APKExportData *>(p_userdata);
	const String path = ResourceUID::ensure_path(p_path);
	const String dst_path = path.replace_first("res://", "assets/");

	store_in_apk(ed, dst_path, p_data, _should_compress_asset(path, p_data) ? Z_DEFLATED : 0);
	return OK;
}

Error EditorExportPlatformAndroid::ignore_apk_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key, uint64_t p_seed) {
	return OK;
}

Error EditorExportPlatformAndroid::copy_gradle_so(void *p_userdata, const SharedObject &p_so) {
	ERR_FAIL_COND_V_MSG(!p_so.path.get_file().begins_with("lib"), FAILED,
			"Android .so file names must start with \"lib\", but got: " + p_so.path);
	Vector<ABI> abis = get_abis();
	CustomExportData *export_data = static_cast<CustomExportData *>(p_userdata);
	bool exported = false;
	for (int i = 0; i < p_so.tags.size(); ++i) {
		int abi_index = -1;
		for (int j = 0; j < abis.size(); ++j) {
			if (abis[j].abi == p_so.tags[i] || abis[j].arch == p_so.tags[i]) {
				abi_index = j;
				break;
			}
		}
		if (abi_index != -1) {
			exported = true;
			String type = export_data->debug ? "debug" : "release";
			String abi = abis[abi_index].abi;
			String filename = p_so.path.get_file();
			String dst_path = export_data->libs_directory.path_join(type).path_join(abi).path_join(filename);
			Vector<uint8_t> data = FileAccess::get_file_as_bytes(p_so.path);
			print_verbose("Copying .so file from " + p_so.path + " to " + dst_path);
			Error err = store_file_at_path(dst_path, data);
			ERR_FAIL_COND_V_MSG(err, err, "Failed to copy .so file from " + p_so.path + " to " + dst_path);
			export_data->libs.push_back(dst_path);
		}
	}
	ERR_FAIL_COND_V_MSG(!exported, FAILED,
			"Cannot determine architecture for library \"" + p_so.path + "\". One of the supported architectures must be used as a tag:" + join_abis(abis, " ", true));
	return OK;
}

bool EditorExportPlatformAndroid::_has_read_write_storage_permission(const Vector<String> &p_permissions) {
	return p_permissions.has("android.permission.READ_EXTERNAL_STORAGE") || p_permissions.has("android.permission.WRITE_EXTERNAL_STORAGE");
}

bool EditorExportPlatformAndroid::_has_manage_external_storage_permission(const Vector<String> &p_permissions) {
	return p_permissions.has("android.permission.MANAGE_EXTERNAL_STORAGE");
}

bool EditorExportPlatformAndroid::_uses_vulkan() {
	String current_renderer = GLOBAL_GET("rendering/renderer/rendering_method.mobile");
	bool uses_vulkan = (current_renderer == "forward_plus" || current_renderer == "mobile") && GLOBAL_GET("rendering/rendering_device/driver.android") == "vulkan";
	return uses_vulkan;
}

void EditorExportPlatformAndroid::_notification(int p_what) {
#ifndef ANDROID_ENABLED
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			if (EditorExport::get_singleton()) {
				EditorExport::get_singleton()->connect_presets_runnable_updated(callable_mp(this, &EditorExportPlatformAndroid::_update_preset_status));
			}
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("export/android")) {
				_create_editor_debug_keystore_if_needed();
			}
		} break;
	}
#endif
}

void EditorExportPlatformAndroid::_create_editor_debug_keystore_if_needed() {
	// Check if we have a valid keytool path.
	String keytool_path = get_keytool_path();
	if (!FileAccess::exists(keytool_path)) {
		return;
	}

	// Check if the current editor debug keystore exists.
	String editor_debug_keystore = EDITOR_GET("export/android/debug_keystore");
	if (FileAccess::exists(editor_debug_keystore)) {
		return;
	}

	// Generate the debug keystore.
	String keystore_path = EditorPaths::get_singleton()->get_debug_keystore_path();
	String keystores_dir = keystore_path.get_base_dir();
	if (!DirAccess::exists(keystores_dir)) {
		Ref<DirAccess> dir_access = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		Error err = dir_access->make_dir_recursive(keystores_dir);
		if (err != OK) {
			WARN_PRINT(TTR("Error creating keystores directory:") + "\n" + keystores_dir);
			return;
		}
	}

	if (!FileAccess::exists(keystore_path)) {
		String output;
		List<String> args;
		args.push_back("-genkey");
		args.push_back("-keystore");
		args.push_back(keystore_path);
		args.push_back("-storepass");
		args.push_back("android");
		args.push_back("-alias");
		args.push_back(DEFAULT_ANDROID_KEYSTORE_DEBUG_USER);
		args.push_back("-keypass");
		args.push_back(DEFAULT_ANDROID_KEYSTORE_DEBUG_PASSWORD);
		args.push_back("-keyalg");
		args.push_back("RSA");
		args.push_back("-keysize");
		args.push_back("2048");
		args.push_back("-validity");
		args.push_back("10000");
		args.push_back("-dname");
		args.push_back("cn=Godot, ou=Godot Engine, o=Stichting Godot, c=NL");
		Error error = OS::get_singleton()->execute(keytool_path, args, &output, nullptr, true);
		print_verbose(output);
		if (error != OK) {
			WARN_PRINT("Error: Unable to create debug keystore");
			return;
		}
	}

	// Update the editor settings.
	EditorSettings::get_singleton()->set("export/android/debug_keystore", keystore_path);
	EditorSettings::get_singleton()->set("export/android/debug_keystore_user", DEFAULT_ANDROID_KEYSTORE_DEBUG_USER);
	EditorSettings::get_singleton()->set("export/android/debug_keystore_pass", DEFAULT_ANDROID_KEYSTORE_DEBUG_PASSWORD);
	print_verbose("Updated editor debug keystore to " + keystore_path);
}

void EditorExportPlatformAndroid::_get_permissions(const Ref<EditorExportPreset> &p_preset, bool p_give_internet, Vector<String> &r_permissions) {
	const char **aperms = ANDROID_PERMS;
	while (*aperms) {
		bool enabled = p_preset->get("permissions/" + String(*aperms).to_lower());
		if (enabled) {
			r_permissions.push_back("android.permission." + String(*aperms));
		}
		aperms++;
	}
	PackedStringArray user_perms = p_preset->get("permissions/custom_permissions");
	for (int i = 0; i < user_perms.size(); i++) {
		String user_perm = user_perms[i].strip_edges();
		if (!user_perm.is_empty()) {
			r_permissions.push_back(user_perm);
		}
	}
	if (p_give_internet) {
		if (!r_permissions.has("android.permission.INTERNET")) {
			r_permissions.push_back("android.permission.INTERNET");
		}
	}
}

void EditorExportPlatformAndroid::_write_tmp_manifest(const Ref<EditorExportPreset> &p_preset, bool p_give_internet, bool p_debug) {
	print_verbose("Building temporary manifest...");
	String manifest_text =
			"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			"<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\"\n"
			"    xmlns:tools=\"http://schemas.android.com/tools\">\n";

	manifest_text += _get_screen_sizes_tag(p_preset);
	manifest_text += _get_gles_tag();

	Vector<String> perms;
	_get_permissions(p_preset, p_give_internet, perms);
	for (int i = 0; i < perms.size(); i++) {
		String permission = perms.get(i);
		if (permission == "android.permission.WRITE_EXTERNAL_STORAGE" || (permission == "android.permission.READ_EXTERNAL_STORAGE" && _has_manage_external_storage_permission(perms))) {
			manifest_text += vformat("    <uses-permission android:name=\"%s\" android:maxSdkVersion=\"29\" />\n", permission);
		} else {
			manifest_text += vformat("    <uses-permission android:name=\"%s\" />\n", permission);
		}
	}

	if (_uses_vulkan()) {
		manifest_text += "    <uses-feature tools:node=\"replace\" android:name=\"android.hardware.vulkan.level\" android:required=\"false\" android:version=\"1\" />\n";
		manifest_text += "    <uses-feature tools:node=\"replace\" android:name=\"android.hardware.vulkan.version\" android:required=\"true\" android:version=\"0x400003\" />\n";
	}

	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	for (int i = 0; i < export_plugins.size(); i++) {
		if (export_plugins[i]->supports_platform(Ref<EditorExportPlatform>(this))) {
			const String contents = export_plugins[i]->get_android_manifest_element_contents(Ref<EditorExportPlatform>(this), p_debug);
			if (!contents.is_empty()) {
				manifest_text += contents;
				manifest_text += "\n";
			}
		}
	}

	manifest_text += _get_application_tag(Ref<EditorExportPlatform>(this), p_preset, _has_read_write_storage_permission(perms), p_debug);
	manifest_text += "</manifest>\n";
	String manifest_path = ExportTemplateManager::get_android_build_directory(p_preset).path_join(vformat("src/%s/AndroidManifest.xml", (p_debug ? "debug" : "release")));

	print_verbose("Storing manifest into " + manifest_path + ": " + "\n" + manifest_text);
	store_string_at_path(manifest_path, manifest_text);
}

void EditorExportPlatformAndroid::_fix_themes_xml(const Ref<EditorExportPreset> &p_preset) {
	const String themes_xml_path = ExportTemplateManager::get_android_build_directory(p_preset).path_join("res/values/themes.xml");
	bool enable_swipe_to_dismiss = p_preset->get("wear_os/swipe_to_dismiss");

	if (!FileAccess::exists(themes_xml_path)) {
		print_error("res/values/themes.xml does not exist.");
		return;
	}

	String xml_content;
	Ref<FileAccess> file = FileAccess::open(themes_xml_path, FileAccess::READ);
	PackedStringArray lines = file->get_as_text().split("\n");
	file->close();

	// Check if the themes.xml already contains <item name="android:windowSwipeToDismiss"> element.
	// If found, update its value based on `enable_swipe_to_dismiss`.
	bool found = false;
	bool modified = false;
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		if (line.contains("<item name") && line.contains("\"android:windowSwipeToDismiss\">")) {
			lines.set(i, vformat("		<item name=\"android:windowSwipeToDismiss\">%s</item>", bool_to_string(enable_swipe_to_dismiss)));
			found = true;
			modified = true;
			break;
		}
	}

	// If <item name="android:windowSwipeToDismiss"> is not found and `enable_swipe_to_dismiss` is false:
	// Add a new <item> element before the closing </style> tag.
	if (!found && !enable_swipe_to_dismiss) {
		for (int i = 0; i < lines.size(); i++) {
			if (lines[i].contains("</style>")) {
				lines.insert(i, "		<item name=\"android:windowSwipeToDismiss\">false</item>");
				modified = true;
				break;
			}
		}
	}

	// Reconstruct the XML content from the modified lines.
	if (modified) {
		xml_content = String("\n").join(lines);
		store_string_at_path(themes_xml_path, xml_content);
		print_verbose("Successfully modified " + themes_xml_path + ": " + "\n" + xml_content);
	} else {
		print_verbose("No changes needed for " + themes_xml_path);
	}
}

void EditorExportPlatformAndroid::_fix_manifest(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &p_manifest, bool p_give_internet) {
	// Leaving the unused types commented because looking these constants up
	// again later would be annoying
	// const int CHUNK_AXML_FILE = 0x00080003;
	// const int CHUNK_RESOURCEIDS = 0x00080180;
	const int CHUNK_STRINGS = 0x001C0001;
	// const int CHUNK_XML_END_NAMESPACE = 0x00100101;
	const int CHUNK_XML_END_TAG = 0x00100103;
	// const int CHUNK_XML_START_NAMESPACE = 0x00100100;
	const int CHUNK_XML_START_TAG = 0x00100102;
	// const int CHUNK_XML_TEXT = 0x00100104;
	const int UTF8_FLAG = 0x00000100;

	Vector<String> string_table;

	uint32_t ofs = 8;

	uint32_t string_count = 0;
	uint32_t string_flags = 0;
	uint32_t string_data_offset = 0;

	uint32_t string_table_begins = 0;
	uint32_t string_table_ends = 0;
	Vector<uint8_t> stable_extra;

	String version_name = p_preset->get_version("version/name");
	int version_code = p_preset->get("version/code");
	String package_name = p_preset->get("package/unique_name");

	const int screen_orientation =
			_get_android_orientation_value(DisplayServer::ScreenOrientation(int(GLOBAL_GET("display/window/handheld/orientation"))));

	bool screen_support_small = p_preset->get("screen/support_small");
	bool screen_support_normal = p_preset->get("screen/support_normal");
	bool screen_support_large = p_preset->get("screen/support_large");
	bool screen_support_xlarge = p_preset->get("screen/support_xlarge");

	bool backup_allowed = p_preset->get("user_data_backup/allow");
	int app_category = p_preset->get("package/app_category");
	bool retain_data_on_uninstall = p_preset->get("package/retain_data_on_uninstall");
	bool exclude_from_recents = p_preset->get("package/exclude_from_recents");
	bool is_resizeable = bool(GLOBAL_GET("display/window/size/resizable"));

	Vector<String> perms;
	// Write permissions into the perms variable.
	_get_permissions(p_preset, p_give_internet, perms);
	bool has_read_write_storage_permission = _has_read_write_storage_permission(perms);

	while (ofs < (uint32_t)p_manifest.size()) {
		uint32_t chunk = decode_uint32(&p_manifest[ofs]);
		uint32_t size = decode_uint32(&p_manifest[ofs + 4]);

		switch (chunk) {
			case CHUNK_STRINGS: {
				int iofs = ofs + 8;

				string_count = decode_uint32(&p_manifest[iofs]);
				string_flags = decode_uint32(&p_manifest[iofs + 8]);
				string_data_offset = decode_uint32(&p_manifest[iofs + 12]);

				uint32_t st_offset = iofs + 20;
				string_table.resize(string_count);
				uint32_t string_end = 0;

				string_table_begins = st_offset;

				for (uint32_t i = 0; i < string_count; i++) {
					uint32_t string_at = decode_uint32(&p_manifest[st_offset + i * 4]);
					string_at += st_offset + string_count * 4;

					ERR_FAIL_COND_MSG(string_flags & UTF8_FLAG, "Unimplemented, can't read UTF-8 string table.");

					if (string_flags & UTF8_FLAG) {
					} else {
						uint32_t len = decode_uint16(&p_manifest[string_at]);
						Vector<char32_t> ucstring;
						ucstring.resize(len + 1);
						for (uint32_t j = 0; j < len; j++) {
							uint16_t c = decode_uint16(&p_manifest[string_at + 2 + 2 * j]);
							ucstring.write[j] = c;
						}
						string_end = MAX(string_at + 2 + 2 * len, string_end);
						ucstring.write[len] = 0;
						string_table.write[i] = ucstring.ptr();
					}
				}

				for (uint32_t i = string_end; i < (ofs + size); i++) {
					stable_extra.push_back(p_manifest[i]);
				}

				string_table_ends = ofs + size;

			} break;
			case CHUNK_XML_START_TAG: {
				int iofs = ofs + 8;
				uint32_t name = decode_uint32(&p_manifest[iofs + 12]);

				String tname = string_table[name];
				uint32_t attrcount = decode_uint32(&p_manifest[iofs + 20]);
				iofs += 28;

				for (uint32_t i = 0; i < attrcount; i++) {
					uint32_t attr_nspace = decode_uint32(&p_manifest[iofs]);
					uint32_t attr_name = decode_uint32(&p_manifest[iofs + 4]);
					uint32_t attr_value = decode_uint32(&p_manifest[iofs + 8]);
					uint32_t attr_resid = decode_uint32(&p_manifest[iofs + 16]);

					const String value = (attr_value != 0xFFFFFFFF) ? string_table[attr_value] : "Res #" + itos(attr_resid);
					String attrname = string_table[attr_name];
					const String nspace = (attr_nspace != 0xFFFFFFFF) ? string_table[attr_nspace] : "";

					//replace project information
					if (tname == "manifest" && attrname == "package") {
						string_table.write[attr_value] = get_package_name(package_name);
					}

					if (tname == "manifest" && attrname == "versionCode") {
						encode_uint32(version_code, &p_manifest.write[iofs + 16]);
					}

					if (tname == "manifest" && attrname == "versionName") {
						if (attr_value == 0xFFFFFFFF) {
							WARN_PRINT("Version name in a resource, should be plain text");
						} else {
							string_table.write[attr_value] = version_name;
						}
					}

					if (tname == "application" && attrname == "requestLegacyExternalStorage") {
						encode_uint32(has_read_write_storage_permission ? 0xFFFFFFFF : 0, &p_manifest.write[iofs + 16]);
					}

					if (tname == "application" && attrname == "allowBackup") {
						encode_uint32(backup_allowed, &p_manifest.write[iofs + 16]);
					}

					if (tname == "application" && attrname == "appCategory") {
						encode_uint32(_get_app_category_value(app_category), &p_manifest.write[iofs + 16]);
					}

					if (tname == "application" && attrname == "isGame") {
						encode_uint32(app_category == APP_CATEGORY_GAME, &p_manifest.write[iofs + 16]);
					}

					if (tname == "application" && attrname == "hasFragileUserData") {
						encode_uint32(retain_data_on_uninstall, &p_manifest.write[iofs + 16]);
					}

					if (tname == "activity" && attrname == "screenOrientation") {
						encode_uint32(screen_orientation, &p_manifest.write[iofs + 16]);
					}

					if (tname == "activity" && attrname == "excludeFromRecents") {
						encode_uint32(exclude_from_recents, &p_manifest.write[iofs + 16]);
					}

					if (tname == "activity" && attrname == "resizeableActivity") {
						encode_uint32(is_resizeable, &p_manifest.write[iofs + 16]);
					}

					if (tname == "provider" && attrname == "authorities") {
						string_table.write[attr_value] = get_package_name(package_name) + String(".fileprovider");
					}

					if (tname == "supports-screens") {
						if (attrname == "smallScreens") {
							encode_uint32(screen_support_small ? 0xFFFFFFFF : 0, &p_manifest.write[iofs + 16]);

						} else if (attrname == "normalScreens") {
							encode_uint32(screen_support_normal ? 0xFFFFFFFF : 0, &p_manifest.write[iofs + 16]);

						} else if (attrname == "largeScreens") {
							encode_uint32(screen_support_large ? 0xFFFFFFFF : 0, &p_manifest.write[iofs + 16]);

						} else if (attrname == "xlargeScreens") {
							encode_uint32(screen_support_xlarge ? 0xFFFFFFFF : 0, &p_manifest.write[iofs + 16]);
						}
					}

					iofs += 20;
				}

			} break;
			case CHUNK_XML_END_TAG: {
				int iofs = ofs + 8;
				uint32_t name = decode_uint32(&p_manifest[iofs + 12]);
				String tname = string_table[name];

				if (tname == "uses-feature") {
					Vector<String> feature_names;
					Vector<bool> feature_required_list;
					Vector<int> feature_versions;

					if (_uses_vulkan()) {
						// Require vulkan hardware level 1 support
						feature_names.push_back("android.hardware.vulkan.level");
						feature_required_list.push_back(false);
						feature_versions.push_back(1);

						// Require vulkan version 1.0
						feature_names.push_back("android.hardware.vulkan.version");
						feature_required_list.push_back(true);
						feature_versions.push_back(0x400003); // Encoded value for api version 1.0
					}

					if (feature_names.size() > 0) {
						ofs += 24; // skip over end tag

						// save manifest ending so we can restore it
						Vector<uint8_t> manifest_end;
						uint32_t manifest_cur_size = p_manifest.size();

						manifest_end.resize(p_manifest.size() - ofs);
						memcpy(manifest_end.ptrw(), &p_manifest[ofs], manifest_end.size());

						int32_t attr_name_string = string_table.find("name");
						ERR_FAIL_COND_MSG(attr_name_string == -1, "Template does not have 'name' attribute.");

						int32_t ns_android_string = string_table.find("http://schemas.android.com/apk/res/android");
						if (ns_android_string == -1) {
							string_table.push_back("http://schemas.android.com/apk/res/android");
							ns_android_string = string_table.size() - 1;
						}

						int32_t attr_uses_feature_string = string_table.find("uses-feature");
						if (attr_uses_feature_string == -1) {
							string_table.push_back("uses-feature");
							attr_uses_feature_string = string_table.size() - 1;
						}

						int32_t attr_required_string = string_table.find("required");
						if (attr_required_string == -1) {
							string_table.push_back("required");
							attr_required_string = string_table.size() - 1;
						}

						for (int i = 0; i < feature_names.size(); i++) {
							const String &feature_name = feature_names[i];
							bool feature_required = feature_required_list[i];
							int feature_version = feature_versions[i];
							bool has_version_attribute = feature_version != -1;

							print_line("Adding feature " + feature_name);

							int32_t feature_string = string_table.find(feature_name);
							if (feature_string == -1) {
								string_table.push_back(feature_name);
								feature_string = string_table.size() - 1;
							}

							String required_value_string = feature_required ? "true" : "false";
							int32_t required_value = string_table.find(required_value_string);
							if (required_value == -1) {
								string_table.push_back(required_value_string);
								required_value = string_table.size() - 1;
							}

							int32_t attr_version_string = -1;
							int32_t version_value = -1;
							int tag_size;
							int attr_count;
							if (has_version_attribute) {
								attr_version_string = string_table.find("version");
								if (attr_version_string == -1) {
									string_table.push_back("version");
									attr_version_string = string_table.size() - 1;
								}

								version_value = string_table.find(itos(feature_version));
								if (version_value == -1) {
									string_table.push_back(itos(feature_version));
									version_value = string_table.size() - 1;
								}

								tag_size = 96; // node and three attrs + end node
								attr_count = 3;
							} else {
								tag_size = 76; // node and two attrs + end node
								attr_count = 2;
							}
							manifest_cur_size += tag_size + 24;
							p_manifest.resize(manifest_cur_size);

							// start tag
							encode_uint16(0x102, &p_manifest.write[ofs]); // type
							encode_uint16(16, &p_manifest.write[ofs + 2]); // headersize
							encode_uint32(tag_size, &p_manifest.write[ofs + 4]); // size
							encode_uint32(0, &p_manifest.write[ofs + 8]); // lineno
							encode_uint32(-1, &p_manifest.write[ofs + 12]); // comment
							encode_uint32(-1, &p_manifest.write[ofs + 16]); // ns
							encode_uint32(attr_uses_feature_string, &p_manifest.write[ofs + 20]); // name
							encode_uint16(20, &p_manifest.write[ofs + 24]); // attr_start
							encode_uint16(20, &p_manifest.write[ofs + 26]); // attr_size
							encode_uint16(attr_count, &p_manifest.write[ofs + 28]); // num_attrs
							encode_uint16(0, &p_manifest.write[ofs + 30]); // id_index
							encode_uint16(0, &p_manifest.write[ofs + 32]); // class_index
							encode_uint16(0, &p_manifest.write[ofs + 34]); // style_index

							// android:name attribute
							encode_uint32(ns_android_string, &p_manifest.write[ofs + 36]); // ns
							encode_uint32(attr_name_string, &p_manifest.write[ofs + 40]); // 'name'
							encode_uint32(feature_string, &p_manifest.write[ofs + 44]); // raw_value
							encode_uint16(8, &p_manifest.write[ofs + 48]); // typedvalue_size
							p_manifest.write[ofs + 50] = 0; // typedvalue_always0
							p_manifest.write[ofs + 51] = 0x03; // typedvalue_type (string)
							encode_uint32(feature_string, &p_manifest.write[ofs + 52]); // typedvalue reference

							// android:required attribute
							encode_uint32(ns_android_string, &p_manifest.write[ofs + 56]); // ns
							encode_uint32(attr_required_string, &p_manifest.write[ofs + 60]); // 'name'
							encode_uint32(required_value, &p_manifest.write[ofs + 64]); // raw_value
							encode_uint16(8, &p_manifest.write[ofs + 68]); // typedvalue_size
							p_manifest.write[ofs + 70] = 0; // typedvalue_always0
							p_manifest.write[ofs + 71] = 0x03; // typedvalue_type (string)
							encode_uint32(required_value, &p_manifest.write[ofs + 72]); // typedvalue reference

							ofs += 76;

							if (has_version_attribute) {
								// android:version attribute
								encode_uint32(ns_android_string, &p_manifest.write[ofs]); // ns
								encode_uint32(attr_version_string, &p_manifest.write[ofs + 4]); // 'name'
								encode_uint32(version_value, &p_manifest.write[ofs + 8]); // raw_value
								encode_uint16(8, &p_manifest.write[ofs + 12]); // typedvalue_size
								p_manifest.write[ofs + 14] = 0; // typedvalue_always0
								p_manifest.write[ofs + 15] = 0x03; // typedvalue_type (string)
								encode_uint32(version_value, &p_manifest.write[ofs + 16]); // typedvalue reference

								ofs += 20;
							}

							// end tag
							encode_uint16(0x103, &p_manifest.write[ofs]); // type
							encode_uint16(16, &p_manifest.write[ofs + 2]); // headersize
							encode_uint32(24, &p_manifest.write[ofs + 4]); // size
							encode_uint32(0, &p_manifest.write[ofs + 8]); // lineno
							encode_uint32(-1, &p_manifest.write[ofs + 12]); // comment
							encode_uint32(-1, &p_manifest.write[ofs + 16]); // ns
							encode_uint32(attr_uses_feature_string, &p_manifest.write[ofs + 20]); // name

							ofs += 24;
						}
						memcpy(&p_manifest.write[ofs], manifest_end.ptr(), manifest_end.size());
						ofs -= 24; // go back over back end
					}
				}
				if (tname == "manifest") {
					// save manifest ending so we can restore it
					Vector<uint8_t> manifest_end;
					uint32_t manifest_cur_size = p_manifest.size();

					manifest_end.resize(p_manifest.size() - ofs);
					memcpy(manifest_end.ptrw(), &p_manifest[ofs], manifest_end.size());

					int32_t attr_name_string = string_table.find("name");
					ERR_FAIL_COND_MSG(attr_name_string == -1, "Template does not have 'name' attribute.");

					int32_t ns_android_string = string_table.find("android");
					ERR_FAIL_COND_MSG(ns_android_string == -1, "Template does not have 'android' namespace.");

					int32_t attr_uses_permission_string = string_table.find("uses-permission");
					if (attr_uses_permission_string == -1) {
						string_table.push_back("uses-permission");
						attr_uses_permission_string = string_table.size() - 1;
					}

					for (int i = 0; i < perms.size(); ++i) {
						print_line("Adding permission " + perms[i]);

						manifest_cur_size += 56 + 24; // node + end node
						p_manifest.resize(manifest_cur_size);

						// Add permission to the string pool
						int32_t perm_string = string_table.find(perms[i]);
						if (perm_string == -1) {
							string_table.push_back(perms[i]);
							perm_string = string_table.size() - 1;
						}

						// start tag
						encode_uint16(0x102, &p_manifest.write[ofs]); // type
						encode_uint16(16, &p_manifest.write[ofs + 2]); // headersize
						encode_uint32(56, &p_manifest.write[ofs + 4]); // size
						encode_uint32(0, &p_manifest.write[ofs + 8]); // lineno
						encode_uint32(-1, &p_manifest.write[ofs + 12]); // comment
						encode_uint32(-1, &p_manifest.write[ofs + 16]); // ns
						encode_uint32(attr_uses_permission_string, &p_manifest.write[ofs + 20]); // name
						encode_uint16(20, &p_manifest.write[ofs + 24]); // attr_start
						encode_uint16(20, &p_manifest.write[ofs + 26]); // attr_size
						encode_uint16(1, &p_manifest.write[ofs + 28]); // num_attrs
						encode_uint16(0, &p_manifest.write[ofs + 30]); // id_index
						encode_uint16(0, &p_manifest.write[ofs + 32]); // class_index
						encode_uint16(0, &p_manifest.write[ofs + 34]); // style_index

						// attribute
						encode_uint32(ns_android_string, &p_manifest.write[ofs + 36]); // ns
						encode_uint32(attr_name_string, &p_manifest.write[ofs + 40]); // 'name'
						encode_uint32(perm_string, &p_manifest.write[ofs + 44]); // raw_value
						encode_uint16(8, &p_manifest.write[ofs + 48]); // typedvalue_size
						p_manifest.write[ofs + 50] = 0; // typedvalue_always0
						p_manifest.write[ofs + 51] = 0x03; // typedvalue_type (string)
						encode_uint32(perm_string, &p_manifest.write[ofs + 52]); // typedvalue reference

						ofs += 56;

						// end tag
						encode_uint16(0x103, &p_manifest.write[ofs]); // type
						encode_uint16(16, &p_manifest.write[ofs + 2]); // headersize
						encode_uint32(24, &p_manifest.write[ofs + 4]); // size
						encode_uint32(0, &p_manifest.write[ofs + 8]); // lineno
						encode_uint32(-1, &p_manifest.write[ofs + 12]); // comment
						encode_uint32(-1, &p_manifest.write[ofs + 16]); // ns
						encode_uint32(attr_uses_permission_string, &p_manifest.write[ofs + 20]); // name

						ofs += 24;
					}

					// copy footer back in
					memcpy(&p_manifest.write[ofs], manifest_end.ptr(), manifest_end.size());
				}
			} break;
		}

		ofs += size;
	}

	//create new andriodmanifest binary

	Vector<uint8_t> ret;
	ret.resize(string_table_begins + string_table.size() * 4);

	for (uint32_t i = 0; i < string_table_begins; i++) {
		ret.write[i] = p_manifest[i];
	}

	ofs = 0;
	for (int i = 0; i < string_table.size(); i++) {
		encode_uint32(ofs, &ret.write[string_table_begins + i * 4]);
		ofs += string_table[i].length() * 2 + 2 + 2;
	}

	ret.resize(ret.size() + ofs);
	string_data_offset = ret.size() - ofs;
	uint8_t *chars = &ret.write[string_data_offset];
	for (int i = 0; i < string_table.size(); i++) {
		String s = string_table[i];
		encode_uint16(s.length(), chars);
		chars += 2;
		for (int j = 0; j < s.length(); j++) {
			encode_uint16(s[j], chars);
			chars += 2;
		}
		encode_uint16(0, chars);
		chars += 2;
	}

	for (int i = 0; i < stable_extra.size(); i++) {
		ret.push_back(stable_extra[i]);
	}

	//pad
	while (ret.size() % 4) {
		ret.push_back(0);
	}

	uint32_t new_stable_end = ret.size();

	uint32_t extra = (p_manifest.size() - string_table_ends);
	ret.resize(new_stable_end + extra);
	for (uint32_t i = 0; i < extra; i++) {
		ret.write[new_stable_end + i] = p_manifest[string_table_ends + i];
	}

	while (ret.size() % 4) {
		ret.push_back(0);
	}
	encode_uint32(ret.size(), &ret.write[4]); //update new file size

	encode_uint32(new_stable_end - 8, &ret.write[12]); //update new string table size
	encode_uint32(string_table.size(), &ret.write[16]); //update new number of strings
	encode_uint32(string_data_offset - 8, &ret.write[28]); //update new string data offset

	p_manifest = ret;
}

String EditorExportPlatformAndroid::_get_keystore_path(const Ref<EditorExportPreset> &p_preset, bool p_debug) {
	String keystore_preference = p_debug ? "keystore/debug" : "keystore/release";
	String keystore_env_variable = p_debug ? ENV_ANDROID_KEYSTORE_DEBUG_PATH : ENV_ANDROID_KEYSTORE_RELEASE_PATH;
	String keystore_path = p_preset->get_or_env(keystore_preference, keystore_env_variable);

	return ProjectSettings::get_singleton()->globalize_path(keystore_path).simplify_path();
}

String EditorExportPlatformAndroid::_parse_string(const uint8_t *p_bytes, bool p_utf8) {
	uint32_t offset = 0;
	uint32_t len = 0;

	if (p_utf8) {
		uint8_t byte = p_bytes[offset];
		if (byte & 0x80) {
			offset += 2;
		} else {
			offset += 1;
		}
		byte = p_bytes[offset];
		offset++;
		if (byte & 0x80) {
			len = byte & 0x7F;
			len = (len << 8) + p_bytes[offset];
			offset++;
		} else {
			len = byte;
		}
	} else {
		len = decode_uint16(&p_bytes[offset]);
		offset += 2;
		if (len & 0x8000) {
			len &= 0x7FFF;
			len = (len << 16) + decode_uint16(&p_bytes[offset]);
			offset += 2;
		}
	}

	if (p_utf8) {
		Vector<uint8_t> str8;
		str8.resize(len + 1);
		for (uint32_t i = 0; i < len; i++) {
			str8.write[i] = p_bytes[offset + i];
		}
		str8.write[len] = 0;
		String str;
		str.parse_utf8((const char *)str8.ptr(), len);
		return str;
	} else {
		String str;
		for (uint32_t i = 0; i < len; i++) {
			char32_t c = decode_uint16(&p_bytes[offset + i * 2]);
			if (c == 0) {
				break;
			}
			str += String::chr(c);
		}
		return str;
	}
}

void EditorExportPlatformAndroid::_fix_resources(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &r_manifest) {
	const int UTF8_FLAG = 0x00000100;

	uint32_t string_block_len = decode_uint32(&r_manifest[16]);
	uint32_t string_count = decode_uint32(&r_manifest[20]);
	uint32_t string_flags = decode_uint32(&r_manifest[28]);
	const uint32_t string_table_begins = 40;

	Vector<String> string_table;

	String package_name = p_preset->get("package/name");
	Dictionary appnames = GLOBAL_GET("application/config/name_localized");

	for (uint32_t i = 0; i < string_count; i++) {
		uint32_t offset = decode_uint32(&r_manifest[string_table_begins + i * 4]);
		offset += string_table_begins + string_count * 4;

		String str = _parse_string(&r_manifest[offset], string_flags & UTF8_FLAG);

		if (str.begins_with("godot-project-name")) {
			if (str == "godot-project-name") {
				//project name
				str = get_project_name(package_name);

			} else {
				String lang = str.substr(str.rfind_char('-') + 1, str.length()).replace("-", "_");
				if (appnames.has(lang)) {
					str = appnames[lang];
				} else {
					str = get_project_name(package_name);
				}
			}
		}

		string_table.push_back(str);
	}

	//write a new string table, but use 16 bits
	Vector<uint8_t> ret;
	ret.resize(string_table_begins + string_table.size() * 4);

	for (uint32_t i = 0; i < string_table_begins; i++) {
		ret.write[i] = r_manifest[i];
	}

	int ofs = 0;
	for (int i = 0; i < string_table.size(); i++) {
		encode_uint32(ofs, &ret.write[string_table_begins + i * 4]);
		ofs += string_table[i].length() * 2 + 2 + 2;
	}

	ret.resize(ret.size() + ofs);
	uint8_t *chars = &ret.write[ret.size() - ofs];
	for (int i = 0; i < string_table.size(); i++) {
		String s = string_table[i];
		encode_uint16(s.length(), chars);
		chars += 2;
		for (int j = 0; j < s.length(); j++) {
			encode_uint16(s[j], chars);
			chars += 2;
		}
		encode_uint16(0, chars);
		chars += 2;
	}

	//pad
	while (ret.size() % 4) {
		ret.push_back(0);
	}

	//change flags to not use utf8
	encode_uint32(string_flags & ~0x100, &ret.write[28]);
	//change length
	encode_uint32(ret.size() - 12, &ret.write[16]);
	//append the rest...
	int rest_from = 12 + string_block_len;
	int rest_to = ret.size();
	int rest_len = (r_manifest.size() - rest_from);
	ret.resize(ret.size() + (r_manifest.size() - rest_from));
	for (int i = 0; i < rest_len; i++) {
		ret.write[rest_to + i] = r_manifest[rest_from + i];
	}
	//finally update the size
	encode_uint32(ret.size(), &ret.write[4]);

	r_manifest = ret;
	//printf("end\n");
}

void EditorExportPlatformAndroid::_load_image_data(const Ref<Image> &p_splash_image, Vector<uint8_t> &p_data) {
	Vector<uint8_t> png_buffer;
	Error err = PNGDriverCommon::image_to_png(p_splash_image, png_buffer);
	if (err == OK) {
		p_data.resize(png_buffer.size());
		memcpy(p_data.ptrw(), png_buffer.ptr(), p_data.size());
	} else {
		String err_str = String("Failed to convert splash image to png.");
		WARN_PRINT(err_str.utf8().get_data());
	}
}

void EditorExportPlatformAndroid::_process_launcher_icons(const String &p_file_name, const Ref<Image> &p_source_image, int dimension, Vector<uint8_t> &p_data) {
	Ref<Image> working_image = p_source_image;

	if (p_source_image->get_width() != dimension || p_source_image->get_height() != dimension) {
		working_image = p_source_image->duplicate();
		working_image->resize(dimension, dimension, Image::Interpolation::INTERPOLATE_LANCZOS);
	}

	Vector<uint8_t> png_buffer;
	Error err = PNGDriverCommon::image_to_png(working_image, png_buffer);
	if (err == OK) {
		p_data.resize(png_buffer.size());
		memcpy(p_data.ptrw(), png_buffer.ptr(), p_data.size());
	} else {
		String err_str = String("Failed to convert resized icon (") + p_file_name + ") to png.";
		WARN_PRINT(err_str.utf8().get_data());
	}
}

void EditorExportPlatformAndroid::load_icon_refs(const Ref<EditorExportPreset> &p_preset, Ref<Image> &icon, Ref<Image> &foreground, Ref<Image> &background, Ref<Image> &monochrome) {
	String project_icon_path = GLOBAL_GET("application/config/icon");

	icon.instantiate();
	foreground.instantiate();
	background.instantiate();
	monochrome.instantiate();

	// Regular icon: user selection -> project icon -> default.
	String path = static_cast<String>(p_preset->get(LAUNCHER_ICON_OPTION)).strip_edges();
	print_verbose("Loading regular icon from " + path);
	if (path.is_empty() || ImageLoader::load_image(path, icon) != OK) {
		print_verbose("- falling back to project icon: " + project_icon_path);
		if (!project_icon_path.is_empty()) {
			ImageLoader::load_image(project_icon_path, icon);
		} else {
			ERR_PRINT("No project icon specified. Please specify one in the Project Settings under Application -> Config -> Icon");
		}
	}

	// Adaptive foreground: user selection -> regular icon (user selection -> project icon -> default).
	path = static_cast<String>(p_preset->get(LAUNCHER_ADAPTIVE_ICON_FOREGROUND_OPTION)).strip_edges();
	print_verbose("Loading adaptive foreground icon from " + path);
	if (path.is_empty() || ImageLoader::load_image(path, foreground) != OK) {
		print_verbose("- falling back to using the regular icon");
		foreground = icon;
	}

	// Adaptive background: user selection -> default.
	path = static_cast<String>(p_preset->get(LAUNCHER_ADAPTIVE_ICON_BACKGROUND_OPTION)).strip_edges();
	if (!path.is_empty()) {
		print_verbose("Loading adaptive background icon from " + path);
		ImageLoader::load_image(path, background);
	}

	// Adaptive monochrome: user selection -> default.
	path = static_cast<String>(p_preset->get(LAUNCHER_ADAPTIVE_ICON_MONOCHROME_OPTION)).strip_edges();
	if (!path.is_empty()) {
		print_verbose("Loading adaptive monochrome icon from " + path);
		ImageLoader::load_image(path, monochrome);
	}
}

void EditorExportPlatformAndroid::_copy_icons_to_gradle_project(const Ref<EditorExportPreset> &p_preset,
		const Ref<Image> &p_main_image,
		const Ref<Image> &p_foreground,
		const Ref<Image> &p_background,
		const Ref<Image> &p_monochrome) {
	String gradle_build_dir = ExportTemplateManager::get_android_build_directory(p_preset);

	String monochrome_tag = "";

	// Prepare images to be resized for the icons. If some image ends up being uninitialized,
	// the default image from the export template will be used.

	for (int i = 0; i < ICON_DENSITIES_COUNT; ++i) {
		if (p_main_image.is_valid() && !p_main_image->is_empty()) {
			print_verbose("Processing launcher icon for dimension " + itos(LAUNCHER_ICONS[i].dimensions) + " into " + LAUNCHER_ICONS[i].export_path);
			Vector<uint8_t> data;
			_process_launcher_icons(LAUNCHER_ICONS[i].export_path, p_main_image, LAUNCHER_ICONS[i].dimensions, data);
			store_file_at_path(gradle_build_dir.path_join(LAUNCHER_ICONS[i].export_path), data);
		}

		if (p_foreground.is_valid() && !p_foreground->is_empty()) {
			print_verbose("Processing launcher adaptive icon p_foreground for dimension " + itos(LAUNCHER_ADAPTIVE_ICON_FOREGROUNDS[i].dimensions) + " into " + LAUNCHER_ADAPTIVE_ICON_FOREGROUNDS[i].export_path);
			Vector<uint8_t> data;
			_process_launcher_icons(LAUNCHER_ADAPTIVE_ICON_FOREGROUNDS[i].export_path, p_foreground,
					LAUNCHER_ADAPTIVE_ICON_FOREGROUNDS[i].dimensions, data);
			store_file_at_path(gradle_build_dir.path_join(LAUNCHER_ADAPTIVE_ICON_FOREGROUNDS[i].export_path), data);
		}

		if (p_background.is_valid() && !p_background->is_empty()) {
			print_verbose("Processing launcher adaptive icon p_background for dimension " + itos(LAUNCHER_ADAPTIVE_ICON_BACKGROUNDS[i].dimensions) + " into " + LAUNCHER_ADAPTIVE_ICON_BACKGROUNDS[i].export_path);
			Vector<uint8_t> data;
			_process_launcher_icons(LAUNCHER_ADAPTIVE_ICON_BACKGROUNDS[i].export_path, p_background,
					LAUNCHER_ADAPTIVE_ICON_BACKGROUNDS[i].dimensions, data);
			store_file_at_path(gradle_build_dir.path_join(LAUNCHER_ADAPTIVE_ICON_BACKGROUNDS[i].export_path), data);
		}

		if (p_monochrome.is_valid() && !p_monochrome->is_empty()) {
			print_verbose("Processing launcher adaptive icon p_monochrome for dimension " + itos(LAUNCHER_ADAPTIVE_ICON_MONOCHROMES[i].dimensions) + " into " + LAUNCHER_ADAPTIVE_ICON_MONOCHROMES[i].export_path);
			Vector<uint8_t> data;
			_process_launcher_icons(LAUNCHER_ADAPTIVE_ICON_MONOCHROMES[i].export_path, p_monochrome,
					LAUNCHER_ADAPTIVE_ICON_MONOCHROMES[i].dimensions, data);
			store_file_at_path(gradle_build_dir.path_join(LAUNCHER_ADAPTIVE_ICON_MONOCHROMES[i].export_path), data);
			monochrome_tag = "    <monochrome android:drawable=\"@mipmap/icon_monochrome\"/>\n";
		}
	}

	// Finalize the icon.xml by formatting the template with the optional monochrome tag.
	store_string_at_path(gradle_build_dir.path_join(ICON_XML_PATH), vformat(ICON_XML_TEMPLATE, monochrome_tag));
}

Vector<EditorExportPlatformAndroid::ABI> EditorExportPlatformAndroid::get_enabled_abis(const Ref<EditorExportPreset> &p_preset) {
	Vector<ABI> abis = get_abis();
	Vector<ABI> enabled_abis;
	for (int i = 0; i < abis.size(); ++i) {
		bool is_enabled = p_preset->get("architectures/" + abis[i].abi);
		if (is_enabled) {
			enabled_abis.push_back(abis[i]);
		}
	}
	return enabled_abis;
}

void EditorExportPlatformAndroid::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	r_features->push_back("etc2");
	r_features->push_back("astc");

	Vector<ABI> abis = get_enabled_abis(p_preset);
	for (int i = 0; i < abis.size(); ++i) {
		r_features->push_back(abis[i].arch);
	}
}

String EditorExportPlatformAndroid::get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const {
	if (p_preset) {
		if (p_name == ("apk_expansion/public_key")) {
			bool apk_expansion = p_preset->get("apk_expansion/enable");
			String apk_expansion_pkey = p_preset->get("apk_expansion/public_key");
			if (apk_expansion && apk_expansion_pkey.is_empty()) {
				return TTR("Invalid public key for APK expansion.");
			}
		} else if (p_name == "package/unique_name") {
			String pn = p_preset->get("package/unique_name");
			String pn_err;

			if (!is_package_name_valid(pn, &pn_err)) {
				return TTR("Invalid package name:") + " " + pn_err;
			}
		} else if (p_name == "wear_os/swipe_to_dismiss") {
			bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");
			if (!bool(p_preset->get("wear_os/swipe_to_dismiss")) && !gradle_build_enabled) {
				return TTR("\"Use Gradle Build\" must be enabled to disable \"Swipe to dismiss\".");
			}
		} else if (p_name == "gradle_build/use_gradle_build") {
			bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");
			String enabled_plugins_names = _get_plugins_names(Ref<EditorExportPreset>(p_preset));
			if (!enabled_plugins_names.is_empty() && !gradle_build_enabled) {
				return TTR("\"Use Gradle Build\" must be enabled to use the plugins.");
			}
		} else if (p_name == "xr_features/xr_mode") {
			bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");
			int xr_mode_index = p_preset->get("xr_features/xr_mode");
			if (xr_mode_index == XR_MODE_OPENXR && !gradle_build_enabled) {
				return TTR("OpenXR requires \"Use Gradle Build\" to be enabled");
			}
		} else if (p_name == "gradle_build/compress_native_libraries") {
			bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");
			if (bool(p_preset->get("gradle_build/compress_native_libraries")) && !gradle_build_enabled) {
				return TTR("\"Compress Native Libraries\" is only valid when \"Use Gradle Build\" is enabled.");
			}
		} else if (p_name == "gradle_build/export_format") {
			bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");
			if (int(p_preset->get("gradle_build/export_format")) == EXPORT_FORMAT_AAB && !gradle_build_enabled) {
				return TTR("\"Export AAB\" is only valid when \"Use Gradle Build\" is enabled.");
			}
		} else if (p_name == "gradle_build/min_sdk") {
			String min_sdk_str = p_preset->get("gradle_build/min_sdk");
			int min_sdk_int = VULKAN_MIN_SDK_VERSION;
			bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");
			if (!min_sdk_str.is_empty()) { // Empty means no override, nothing to do.
				if (!gradle_build_enabled) {
					return TTR("\"Min SDK\" can only be overridden when \"Use Gradle Build\" is enabled.");
				}
				if (!min_sdk_str.is_valid_int()) {
					return vformat(TTR("\"Min SDK\" should be a valid integer, but got \"%s\" which is invalid."), min_sdk_str);
				} else {
					min_sdk_int = min_sdk_str.to_int();
					if (min_sdk_int < OPENGL_MIN_SDK_VERSION) {
						return vformat(TTR("\"Min SDK\" cannot be lower than %d, which is the version needed by the Godot library."), OPENGL_MIN_SDK_VERSION);
					}
				}
			}
		} else if (p_name == "gradle_build/target_sdk") {
			String target_sdk_str = p_preset->get("gradle_build/target_sdk");
			int target_sdk_int = DEFAULT_TARGET_SDK_VERSION;

			String min_sdk_str = p_preset->get("gradle_build/min_sdk");
			int min_sdk_int = VULKAN_MIN_SDK_VERSION;
			if (min_sdk_str.is_valid_int()) {
				min_sdk_int = min_sdk_str.to_int();
			}
			bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");
			if (!target_sdk_str.is_empty()) { // Empty means no override, nothing to do.
				if (!gradle_build_enabled) {
					return TTR("\"Target SDK\" can only be overridden when \"Use Gradle Build\" is enabled.");
				}
				if (!target_sdk_str.is_valid_int()) {
					return vformat(TTR("\"Target SDK\" should be a valid integer, but got \"%s\" which is invalid."), target_sdk_str);
				} else {
					target_sdk_int = target_sdk_str.to_int();
					if (target_sdk_int < min_sdk_int) {
						return TTR("\"Target SDK\" version must be greater or equal to \"Min SDK\" version.");
					}
				}
			}
		} else if (p_name == "package/show_in_android_tv") {
			bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");
			if (bool(p_preset->get("package/show_in_android_tv")) && !gradle_build_enabled) {
				return TTR("\"Use Gradle Build\" must be enabled to enable \"Show In Android Tv\".");
			}
		} else if (p_name == "package/show_as_launcher_app") {
			bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");
			if (bool(p_preset->get("package/show_as_launcher_app")) && !gradle_build_enabled) {
				return TTR("\"Use Gradle Build\" must be enabled to enable \"Show As Launcher App\".");
			}
		} else if (p_name == "package/show_in_app_library") {
			bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");
			if (!bool(p_preset->get("package/show_in_app_library")) && !gradle_build_enabled) {
				return TTR("\"Use Gradle Build\" must be enabled to disable \"Show In App Library\".");
			}
		}
	}
	return String();
}

void EditorExportPlatformAndroid::get_export_options(List<ExportOption> *r_options) const {
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/debug", PROPERTY_HINT_GLOBAL_FILE, "*.apk"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/release", PROPERTY_HINT_GLOBAL_FILE, "*.apk"), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "gradle_build/use_gradle_build"), false, true, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "gradle_build/gradle_build_directory", PROPERTY_HINT_PLACEHOLDER_TEXT, "res://android"), "", false, false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "gradle_build/android_source_template", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "gradle_build/compress_native_libraries"), false, false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "gradle_build/export_format", PROPERTY_HINT_ENUM, "Export APK,Export AAB"), EXPORT_FORMAT_APK, false, true));
	// Using String instead of int to default to an empty string (no override) with placeholder for instructions (see GH-62465).
	// This implies doing validation that the string is a proper int.
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "gradle_build/min_sdk", PROPERTY_HINT_PLACEHOLDER_TEXT, vformat("%d (default)", VULKAN_MIN_SDK_VERSION)), "", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "gradle_build/target_sdk", PROPERTY_HINT_PLACEHOLDER_TEXT, vformat("%d (default)", DEFAULT_TARGET_SDK_VERSION)), "", false, true));

#ifndef DISABLE_DEPRECATED
	Vector<PluginConfigAndroid> plugins_configs = get_plugins();
	for (int i = 0; i < plugins_configs.size(); i++) {
		print_verbose("Found Android plugin " + plugins_configs[i].name);
		r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, vformat("%s/%s", PNAME("plugins"), plugins_configs[i].name)), false));
	}
	android_plugins_changed.clear();
#endif // DISABLE_DEPRECATED

	// Android supports multiple architectures in an app bundle, so
	// we expose each option as a checkbox in the export dialog.
	const Vector<ABI> abis = get_abis();
	for (int i = 0; i < abis.size(); ++i) {
		const String abi = abis[i].abi;
		// All Android devices supporting Vulkan run 64-bit Android,
		// so there is usually no point in exporting for 32-bit Android.
		const bool is_default = abi == "arm64-v8a";
		r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, vformat("%s/%s", PNAME("architectures"), abi)), is_default));
	}

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/debug", PROPERTY_HINT_GLOBAL_FILE, "*.keystore,*.jks", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/debug_user", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/debug_password", PROPERTY_HINT_PASSWORD, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/release", PROPERTY_HINT_GLOBAL_FILE, "*.keystore,*.jks", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/release_user", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "keystore/release_password", PROPERTY_HINT_PASSWORD, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SECRET), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "version/code", PROPERTY_HINT_RANGE, "1,4096,1,or_greater"), 1));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "version/name", PROPERTY_HINT_PLACEHOLDER_TEXT, "Leave empty to use project version"), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "package/unique_name", PROPERTY_HINT_PLACEHOLDER_TEXT, "ext.domain.name"), "com.example.$genname", false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "package/name", PROPERTY_HINT_PLACEHOLDER_TEXT, "Game Name [default if blank]"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "package/signed"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "package/app_category", PROPERTY_HINT_ENUM, "Accessibility,Audio,Game,Image,Maps,News,Productivity,Social,Video,Undefined"), APP_CATEGORY_GAME));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "package/retain_data_on_uninstall"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "package/exclude_from_recents"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "package/show_in_android_tv"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "package/show_in_app_library"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "package/show_as_launcher_app"), false));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, LAUNCHER_ICON_OPTION, PROPERTY_HINT_FILE, "*.png"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, LAUNCHER_ADAPTIVE_ICON_FOREGROUND_OPTION, PROPERTY_HINT_FILE, "*.png"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, LAUNCHER_ADAPTIVE_ICON_BACKGROUND_OPTION, PROPERTY_HINT_FILE, "*.png"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, LAUNCHER_ADAPTIVE_ICON_MONOCHROME_OPTION, PROPERTY_HINT_FILE, "*.png"), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "graphics/opengl_debug"), false));

	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "xr_features/xr_mode", PROPERTY_HINT_ENUM, "Regular,OpenXR"), XR_MODE_REGULAR, false, true));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "wear_os/swipe_to_dismiss"), true));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screen/immersive_mode"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screen/support_small"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screen/support_normal"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screen/support_large"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screen/support_xlarge"), true));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "user_data_backup/allow"), false));

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "command_line/extra_args"), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "apk_expansion/enable"), false, false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "apk_expansion/SALT"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "apk_expansion/public_key", PROPERTY_HINT_MULTILINE_TEXT), "", false, true));

	r_options->push_back(ExportOption(PropertyInfo(Variant::PACKED_STRING_ARRAY, "permissions/custom_permissions"), PackedStringArray()));

	const char **perms = ANDROID_PERMS;
	while (*perms) {
		r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, vformat("%s/%s", PNAME("permissions"), String(*perms).to_lower())), false));
		perms++;
	}
}

bool EditorExportPlatformAndroid::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	if (p_preset == nullptr) {
		return true;
	}

	bool advanced_options_enabled = p_preset->are_advanced_options_enabled();
	if (p_option == "graphics/opengl_debug" ||
			p_option == "command_line/extra_args" ||
			p_option == "permissions/custom_permissions" ||
			p_option == "gradle_build/compress_native_libraries" ||
			p_option == "package/retain_data_on_uninstall" ||
			p_option == "package/exclude_from_recents" ||
			p_option == "package/show_in_app_library" ||
			p_option == "package/show_as_launcher_app" ||
			p_option == "wear_os/swipe_to_dismiss" ||
			p_option == "apk_expansion/enable" ||
			p_option == "apk_expansion/SALT" ||
			p_option == "apk_expansion/public_key") {
		return advanced_options_enabled;
	}
	if (p_option == "gradle_build/gradle_build_directory" || p_option == "gradle_build/android_source_template") {
		return advanced_options_enabled && bool(p_preset->get("gradle_build/use_gradle_build"));
	}
	if (p_option == "custom_template/debug" || p_option == "custom_template/release") {
		// The APK templates are ignored if Gradle build is enabled.
		return advanced_options_enabled && !bool(p_preset->get("gradle_build/use_gradle_build"));
	}

	// Hide .NET embedding option (always enabled).
	if (p_option == "dotnet/embed_build_outputs") {
		return false;
	}

	return true;
}

String EditorExportPlatformAndroid::get_name() const {
	return "Android";
}

String EditorExportPlatformAndroid::get_os_name() const {
	return "Android";
}

Ref<Texture2D> EditorExportPlatformAndroid::get_logo() const {
	return logo;
}

bool EditorExportPlatformAndroid::should_update_export_options() {
#ifndef DISABLE_DEPRECATED
	if (android_plugins_changed.is_set()) {
		// don't clear unless we're reporting true, to avoid race
		android_plugins_changed.clear();
		return true;
	}
#endif // DISABLE_DEPRECATED
	return false;
}

bool EditorExportPlatformAndroid::poll_export() {
	bool dc = devices_changed.is_set();
	if (dc) {
		// don't clear unless we're reporting true, to avoid race
		devices_changed.clear();
	}
	return dc;
}

int EditorExportPlatformAndroid::get_options_count() const {
	MutexLock lock(device_lock);
	return devices.size();
}

String EditorExportPlatformAndroid::get_options_tooltip() const {
	return TTR("Select device from the list");
}

String EditorExportPlatformAndroid::get_option_label(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, devices.size(), "");
	MutexLock lock(device_lock);
	return devices[p_index].name;
}

String EditorExportPlatformAndroid::get_option_tooltip(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, devices.size(), "");
	MutexLock lock(device_lock);
	String s = devices[p_index].description;
	if (devices.size() == 1) {
		// Tooltip will be:
		// Name
		// Description
		s = devices[p_index].name + "\n\n" + s;
	}
	return s;
}

String EditorExportPlatformAndroid::get_device_architecture(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, devices.size(), "");
	MutexLock lock(device_lock);
	return devices[p_index].architecture;
}

Error EditorExportPlatformAndroid::run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	ERR_FAIL_INDEX_V(p_device, devices.size(), ERR_INVALID_PARAMETER);

	String can_export_error;
	bool can_export_missing_templates;
	if (!can_export(p_preset, can_export_error, can_export_missing_templates)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), can_export_error);
		return ERR_UNCONFIGURED;
	}

	MutexLock lock(device_lock);

	EditorProgress ep("run", vformat(TTR("Running on %s"), devices[p_device].name), 3);

	String adb = get_adb_path();

	// Export_temp APK.
	if (ep.step(TTR("Exporting APK..."), 0)) {
		return ERR_SKIP;
	}

	const bool use_wifi_for_remote_debug = EDITOR_GET("export/android/use_wifi_for_remote_debug");
	const bool use_remote = p_debug_flags.has_flag(DEBUG_FLAG_REMOTE_DEBUG) || p_debug_flags.has_flag(DEBUG_FLAG_DUMB_CLIENT);
	const bool use_reverse = devices[p_device].api_level >= 21 && !use_wifi_for_remote_debug;

	if (use_reverse) {
		p_debug_flags.set_flag(DEBUG_FLAG_REMOTE_DEBUG_LOCALHOST);
	}

	String tmp_export_path = EditorPaths::get_singleton()->get_temp_dir().path_join("tmpexport." + uitos(OS::get_singleton()->get_unix_time()) + ".apk");

#define CLEANUP_AND_RETURN(m_err)                                        \
	{                                                                    \
		DirAccess::remove_file_or_error(tmp_export_path);                \
		if (FileAccess::exists(tmp_export_path + ".idsig")) {            \
			DirAccess::remove_file_or_error(tmp_export_path + ".idsig"); \
		}                                                                \
		return m_err;                                                    \
	}                                                                    \
	((void)0)

	// Export to temporary APK before sending to device.
	Error err = export_project_helper(p_preset, true, tmp_export_path, EXPORT_FORMAT_APK, true, p_debug_flags);

	if (err != OK) {
		CLEANUP_AND_RETURN(err);
	}

	List<String> args;
	int rv;
	String output;

	bool remove_prev = EDITOR_GET("export/android/one_click_deploy_clear_previous_install");
	String version_name = p_preset->get_version("version/name");
	String package_name = p_preset->get("package/unique_name");

	if (remove_prev) {
		if (ep.step(TTR("Uninstalling..."), 1)) {
			CLEANUP_AND_RETURN(ERR_SKIP);
		}

		print_line("Uninstalling previous version: " + devices[p_device].name);

		args.push_back("-s");
		args.push_back(devices[p_device].id);
		args.push_back("uninstall");
		if ((bool)EDITOR_GET("export/android/force_system_user") && devices[p_device].api_level >= 17) {
			args.push_back("--user");
			args.push_back("0");
		}
		args.push_back(get_package_name(package_name));

		output.clear();
		err = OS::get_singleton()->execute(adb, args, &output, &rv, true);
		print_verbose(output);
	}

	print_line("Installing to device (please wait...): " + devices[p_device].name);
	if (ep.step(TTR("Installing to device, please wait..."), 2)) {
		CLEANUP_AND_RETURN(ERR_SKIP);
	}

	args.clear();
	args.push_back("-s");
	args.push_back(devices[p_device].id);
	args.push_back("install");
	if ((bool)EDITOR_GET("export/android/force_system_user") && devices[p_device].api_level >= 17) {
		args.push_back("--user");
		args.push_back("0");
	}
	args.push_back("-r");
	args.push_back(tmp_export_path);

	output.clear();
	err = OS::get_singleton()->execute(adb, args, &output, &rv, true);
	print_verbose(output);
	if (err || rv != 0) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), vformat(TTR("Could not install to device: %s"), output));
		CLEANUP_AND_RETURN(ERR_CANT_CREATE);
	}

	if (use_remote) {
		if (use_reverse) {
			static const char *const msg = "--- Device API >= 21; debugging over USB ---";
			EditorNode::get_singleton()->get_log()->add_message(msg, EditorLog::MSG_TYPE_EDITOR);
			print_line(String(msg).to_upper());

			args.clear();
			args.push_back("-s");
			args.push_back(devices[p_device].id);
			args.push_back("reverse");
			args.push_back("--remove-all");
			output.clear();
			OS::get_singleton()->execute(adb, args, &output, &rv, true);
			print_verbose(output);

			if (p_debug_flags.has_flag(DEBUG_FLAG_REMOTE_DEBUG)) {
				int dbg_port = EDITOR_GET("network/debug/remote_port");
				args.clear();
				args.push_back("-s");
				args.push_back(devices[p_device].id);
				args.push_back("reverse");
				args.push_back("tcp:" + itos(dbg_port));
				args.push_back("tcp:" + itos(dbg_port));

				output.clear();
				OS::get_singleton()->execute(adb, args, &output, &rv, true);
				print_verbose(output);
				print_line("Reverse result: " + itos(rv));
			}

			if (p_debug_flags.has_flag(DEBUG_FLAG_DUMB_CLIENT)) {
				int fs_port = EDITOR_GET("filesystem/file_server/port");

				args.clear();
				args.push_back("-s");
				args.push_back(devices[p_device].id);
				args.push_back("reverse");
				args.push_back("tcp:" + itos(fs_port));
				args.push_back("tcp:" + itos(fs_port));

				output.clear();
				err = OS::get_singleton()->execute(adb, args, &output, &rv, true);
				print_verbose(output);
				print_line("Reverse result2: " + itos(rv));
			}
		} else {
			static const char *const api_version_msg = "--- Device API < 21; debugging over Wi-Fi ---";
			static const char *const manual_override_msg = "--- Wi-Fi remote debug enabled in project settings; debugging over Wi-Fi ---";

			const char *const msg = use_wifi_for_remote_debug ? manual_override_msg : api_version_msg;
			EditorNode::get_singleton()->get_log()->add_message(msg, EditorLog::MSG_TYPE_EDITOR);
			print_line(String(msg).to_upper());
		}
	}

	if (ep.step(TTR("Running on device..."), 3)) {
		CLEANUP_AND_RETURN(ERR_SKIP);
	}
	args.clear();
	args.push_back("-s");
	args.push_back(devices[p_device].id);
	args.push_back("shell");
	args.push_back("am");
	args.push_back("start");
	if ((bool)EDITOR_GET("export/android/force_system_user") && devices[p_device].api_level >= 17) {
		args.push_back("--user");
		args.push_back("0");
	}
	args.push_back("-a");
	args.push_back("android.intent.action.MAIN");

	// Going with implicit launch first based on the LAUNCHER category and the app's package.
	args.push_back("-c");
	args.push_back("android.intent.category.LAUNCHER");
	args.push_back(get_package_name(package_name));

	output.clear();
	err = OS::get_singleton()->execute(adb, args, &output, &rv, true);
	print_verbose(output);
	if (err || rv != 0 || output.contains("Error: Activity not started")) {
		// The implicit launch failed, let's try an explicit launch by specifying the component name before giving up.
		const String component_name = get_package_name(package_name) + "/com.godot.game.GodotApp";
		print_line("Implicit launch failed.. Trying explicit launch using", component_name);
		args.erase(get_package_name(package_name));
		args.push_back("-n");
		args.push_back(component_name);

		output.clear();
		err = OS::get_singleton()->execute(adb, args, &output, &rv, true);
		print_verbose(output);

		if (err || rv != 0 || output.begins_with("Error: Activity not started")) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), TTR("Could not execute on device."));
			CLEANUP_AND_RETURN(ERR_CANT_CREATE);
		}
	}

	CLEANUP_AND_RETURN(OK);
#undef CLEANUP_AND_RETURN
}

Ref<Texture2D> EditorExportPlatformAndroid::get_run_icon() const {
	return run_icon;
}

String EditorExportPlatformAndroid::get_java_path() {
	String exe_ext;
	if (OS::get_singleton()->get_name() == "Windows") {
		exe_ext = ".exe";
	}
	String java_sdk_path = EDITOR_GET("export/android/java_sdk_path");
	return java_sdk_path.path_join("bin/java" + exe_ext);
}

String EditorExportPlatformAndroid::get_keytool_path() {
	String exe_ext;
	if (OS::get_singleton()->get_name() == "Windows") {
		exe_ext = ".exe";
	}
	String java_sdk_path = EDITOR_GET("export/android/java_sdk_path");
	return java_sdk_path.path_join("bin/keytool" + exe_ext);
}

String EditorExportPlatformAndroid::get_adb_path() {
	String exe_ext;
	if (OS::get_singleton()->get_name() == "Windows") {
		exe_ext = ".exe";
	}
	String sdk_path = EDITOR_GET("export/android/android_sdk_path");
	return sdk_path.path_join("platform-tools/adb" + exe_ext);
}

String EditorExportPlatformAndroid::get_apksigner_path(int p_target_sdk, bool p_check_executes) {
	if (p_target_sdk == -1) {
		p_target_sdk = DEFAULT_TARGET_SDK_VERSION;
	}
	String exe_ext;
	if (OS::get_singleton()->get_name() == "Windows") {
		exe_ext = ".bat";
	}
	String apksigner_command_name = "apksigner" + exe_ext;
	String sdk_path = EDITOR_GET("export/android/android_sdk_path");
	String apksigner_path;

	Error errn;
	String build_tools_dir = sdk_path.path_join("build-tools");
	Ref<DirAccess> da = DirAccess::open(build_tools_dir, &errn);
	if (errn != OK) {
		print_error("Unable to open Android 'build-tools' directory.");
		return apksigner_path;
	}

	// There are additional versions directories we need to go through.
	Vector<String> dir_list = da->get_directories();

	// We need to use the version of build_tools that matches the Target SDK
	// If somehow we can't find that, we see if a version between 28 and the default target SDK exists.
	// We need to avoid versions <= 27 because they fail on Java versions >9
	// If we can't find that, we just use the first valid version.
	Vector<String> ideal_versions;
	Vector<String> other_versions;
	Vector<String> versions;
	bool found_target_sdk = false;
	// We only allow for versions <= 27 if specifically set
	int min_version = p_target_sdk <= 27 ? p_target_sdk : 28;
	for (String sub_dir : dir_list) {
		if (!sub_dir.begins_with(".")) {
			Vector<String> ver_numbers = sub_dir.split(".");
			// Dir not a version number, will use as last resort
			if (!ver_numbers.size() || !ver_numbers[0].is_valid_int()) {
				other_versions.push_back(sub_dir);
				continue;
			}
			int ver_number = ver_numbers[0].to_int();
			if (ver_number == p_target_sdk) {
				found_target_sdk = true;
				//ensure this is in front of the ones we check
				versions.push_back(sub_dir);
			} else {
				if (ver_number >= min_version && ver_number <= DEFAULT_TARGET_SDK_VERSION) {
					ideal_versions.push_back(sub_dir);
				} else {
					other_versions.push_back(sub_dir);
				}
			}
		}
	}
	// we will check ideal versions first, then other versions.
	versions.append_array(ideal_versions);
	versions.append_array(other_versions);

	if (!versions.size()) {
		print_error("Unable to find the 'apksigner' tool.");
		return apksigner_path;
	}

	int i;
	bool failed = false;
	String version_to_use;

	String java_sdk_path = EDITOR_GET("export/android/java_sdk_path");
	if (!java_sdk_path.is_empty()) {
		OS::get_singleton()->set_environment("JAVA_HOME", java_sdk_path);
	}

	List<String> args;
	args.push_back("--version");
	String output;
	int retval;
	Error err;
	for (i = 0; i < versions.size(); i++) {
		// Check if the tool is here.
		apksigner_path = build_tools_dir.path_join(versions[i]).path_join(apksigner_command_name);
		if (FileAccess::exists(apksigner_path)) {
			version_to_use = versions[i];
			// If we aren't exporting, just break here.
			if (!p_check_executes) {
				break;
			}
			// we only check to see if it executes on export because it is slow to load
			err = OS::get_singleton()->execute(apksigner_path, args, &output, &retval, false);
			if (err || retval) {
				failed = true;
			} else {
				break;
			}
		}
	}
	if (i == versions.size()) {
		if (failed) {
			print_error("All located 'apksigner' tools in " + build_tools_dir + " failed to execute");
			return "<FAILED>";
		} else {
			print_error("Unable to find the 'apksigner' tool.");
			return "";
		}
	}
	if (!found_target_sdk) {
		print_line("Could not find version of build tools that matches Target SDK, using " + version_to_use);
	} else if (failed && found_target_sdk) {
		print_line("Version of build tools that matches Target SDK failed to execute, using " + version_to_use);
	}

	return apksigner_path;
}

static bool has_valid_keystore_credentials(String &r_error_str, const String &p_keystore, const String &p_username, const String &p_password, const String &p_type) {
	String output;
	List<String> args;
	args.push_back("-list");
	args.push_back("-keystore");
	args.push_back(p_keystore);
	args.push_back("-storepass");
	args.push_back(p_password);
	args.push_back("-alias");
	args.push_back(p_username);
	String keytool_path = EditorExportPlatformAndroid::get_keytool_path();
	Error error = OS::get_singleton()->execute(keytool_path, args, &output, nullptr, true);
	String keytool_error = "keytool error:";
	bool valid = output.substr(0, keytool_error.length()) != keytool_error;

	if (error != OK) {
		r_error_str = TTR("Error: There was a problem validating the keystore username and password");
		return false;
	}
	if (!valid) {
		r_error_str = TTR(p_type + " Username and/or Password is invalid for the given " + p_type + " Keystore");
		return false;
	}
	r_error_str = "";
	return true;
}

bool EditorExportPlatformAndroid::has_valid_username_and_password(const Ref<EditorExportPreset> &p_preset, String &r_error) {
	String dk = _get_keystore_path(p_preset, true);
	String dk_user = p_preset->get_or_env("keystore/debug_user", ENV_ANDROID_KEYSTORE_DEBUG_USER);
	String dk_password = p_preset->get_or_env("keystore/debug_password", ENV_ANDROID_KEYSTORE_DEBUG_PASS);
	String rk = _get_keystore_path(p_preset, false);
	String rk_user = p_preset->get_or_env("keystore/release_user", ENV_ANDROID_KEYSTORE_RELEASE_USER);
	String rk_password = p_preset->get_or_env("keystore/release_password", ENV_ANDROID_KEYSTORE_RELEASE_PASS);

	bool valid = true;
	if (!dk.is_empty() && !dk_user.is_empty() && !dk_password.is_empty()) {
		String err = "";
		valid = has_valid_keystore_credentials(err, dk, dk_user, dk_password, "Debug");
		r_error += err;
	}
	if (!rk.is_empty() && !rk_user.is_empty() && !rk_password.is_empty()) {
		String err = "";
		valid = has_valid_keystore_credentials(err, rk, rk_user, rk_password, "Release");
		r_error += err;
	}
	return valid;
}

bool EditorExportPlatformAndroid::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	String err;
	bool valid = false;
	const bool gradle_build_enabled = p_preset->get("gradle_build/use_gradle_build");

#ifdef MODULE_MONO_ENABLED
	// Android export is still a work in progress, keep a message as a warning.
	err += TTR("Exporting to Android when using C#/.NET is experimental.") + "\n";
#endif

	// Look for export templates (first official, and if defined custom templates).

	if (!gradle_build_enabled) {
		String template_err;
		bool dvalid = false;
		bool rvalid = false;
		bool has_export_templates = false;

		if (p_preset->get("custom_template/debug") != "") {
			dvalid = FileAccess::exists(p_preset->get("custom_template/debug"));
			if (!dvalid) {
				template_err += TTR("Custom debug template not found.") + "\n";
			}
		} else {
			has_export_templates |= exists_export_template("android_debug.apk", &template_err);
		}

		if (p_preset->get("custom_template/release") != "") {
			rvalid = FileAccess::exists(p_preset->get("custom_template/release"));
			if (!rvalid) {
				template_err += TTR("Custom release template not found.") + "\n";
			}
		} else {
			has_export_templates |= exists_export_template("android_release.apk", &template_err);
		}

		r_missing_templates = !has_export_templates;
		valid = dvalid || rvalid || has_export_templates;
		if (!valid) {
			err += template_err;
		}
	} else {
#ifdef ANDROID_ENABLED
		err += TTR("Gradle build is not supported for the Android editor.") + "\n";
		valid = false;
#else
		// Validate the custom gradle android source template.
		bool android_source_template_valid = false;
		const String android_source_template = p_preset->get("gradle_build/android_source_template");
		if (!android_source_template.is_empty()) {
			android_source_template_valid = FileAccess::exists(android_source_template);
			if (!android_source_template_valid) {
				err += TTR("Custom Android source template not found.") + "\n";
			}
		}

		// Validate the installed build template.
		bool installed_android_build_template = FileAccess::exists(ExportTemplateManager::get_android_build_directory(p_preset).path_join("build.gradle"));
		if (!installed_android_build_template) {
			if (!android_source_template_valid) {
				r_missing_templates = !exists_export_template("android_source.zip", &err);
			}
			err += TTR("Android build template not installed in the project. Install it from the Project menu.") + "\n";
		} else {
			r_missing_templates = false;
		}

		valid = installed_android_build_template && !r_missing_templates;
#endif
	}

	// Validate the rest of the export configuration.

	String dk = _get_keystore_path(p_preset, true);
	String dk_user = p_preset->get_or_env("keystore/debug_user", ENV_ANDROID_KEYSTORE_DEBUG_USER);
	String dk_password = p_preset->get_or_env("keystore/debug_password", ENV_ANDROID_KEYSTORE_DEBUG_PASS);

	if ((dk.is_empty() || dk_user.is_empty() || dk_password.is_empty()) && (!dk.is_empty() || !dk_user.is_empty() || !dk_password.is_empty())) {
		valid = false;
		err += TTR("Either Debug Keystore, Debug User AND Debug Password settings must be configured OR none of them.") + "\n";
	}

	// Use OR to make the export UI able to show this error.
	if ((p_debug || !dk.is_empty()) && !FileAccess::exists(dk)) {
		dk = EDITOR_GET("export/android/debug_keystore");
		if (!FileAccess::exists(dk)) {
			valid = false;
			err += TTR("Debug keystore not configured in the Editor Settings nor in the preset.") + "\n";
		}
	}

	String rk = _get_keystore_path(p_preset, false);
	String rk_user = p_preset->get_or_env("keystore/release_user", ENV_ANDROID_KEYSTORE_RELEASE_USER);
	String rk_password = p_preset->get_or_env("keystore/release_password", ENV_ANDROID_KEYSTORE_RELEASE_PASS);

	if ((rk.is_empty() || rk_user.is_empty() || rk_password.is_empty()) && (!rk.is_empty() || !rk_user.is_empty() || !rk_password.is_empty())) {
		valid = false;
		err += TTR("Either Release Keystore, Release User AND Release Password settings must be configured OR none of them.") + "\n";
	}

	if (!p_debug && !rk.is_empty() && !FileAccess::exists(rk)) {
		valid = false;
		err += TTR("Release keystore incorrectly configured in the export preset.") + "\n";
	}

#ifndef ANDROID_ENABLED
	String java_sdk_path = EDITOR_GET("export/android/java_sdk_path");
	if (java_sdk_path.is_empty()) {
		err += TTR("A valid Java SDK path is required in Editor Settings.") + "\n";
		valid = false;
	} else {
		// Validate the given path by checking that `java` is present under the `bin` directory.
		Error errn;
		// Check for the bin directory.
		Ref<DirAccess> da = DirAccess::open(java_sdk_path.path_join("bin"), &errn);
		if (errn != OK) {
			err += TTR("Invalid Java SDK path in Editor Settings.") + " ";
			err += TTR("Missing 'bin' directory!");
			err += "\n";
			valid = false;
		} else {
			// Check for the `java` command.
			String java_path = get_java_path();
			if (!FileAccess::exists(java_path)) {
				err += TTR("Unable to find 'java' command using the Java SDK path.") + " ";
				err += TTR("Please check the Java SDK directory specified in Editor Settings.");
				err += "\n";
				valid = false;
			}
		}
	}

	String sdk_path = EDITOR_GET("export/android/android_sdk_path");
	if (sdk_path.is_empty()) {
		err += TTR("A valid Android SDK path is required in Editor Settings.") + "\n";
		valid = false;
	} else {
		Error errn;
		// Check for the platform-tools directory.
		Ref<DirAccess> da = DirAccess::open(sdk_path.path_join("platform-tools"), &errn);
		if (errn != OK) {
			err += TTR("Invalid Android SDK path in Editor Settings.") + " ";
			err += TTR("Missing 'platform-tools' directory!");
			err += "\n";
			valid = false;
		}

		// Validate that adb is available.
		String adb_path = get_adb_path();
		if (!FileAccess::exists(adb_path)) {
			err += TTR("Unable to find Android SDK platform-tools' adb command.") + " ";
			err += TTR("Please check in the Android SDK directory specified in Editor Settings.");
			err += "\n";
			valid = false;
		}

		// Check for the build-tools directory.
		Ref<DirAccess> build_tools_da = DirAccess::open(sdk_path.path_join("build-tools"), &errn);
		if (errn != OK) {
			err += TTR("Invalid Android SDK path in Editor Settings.") + " ";
			err += TTR("Missing 'build-tools' directory!");
			err += "\n";
			valid = false;
		}

		String target_sdk_version = p_preset->get("gradle_build/target_sdk");
		if (!target_sdk_version.is_valid_int()) {
			target_sdk_version = itos(DEFAULT_TARGET_SDK_VERSION);
		}
		// Validate that apksigner is available.
		String apksigner_path = get_apksigner_path(target_sdk_version.to_int());
		if (!FileAccess::exists(apksigner_path)) {
			err += TTR("Unable to find Android SDK build-tools' apksigner command.") + " ";
			err += TTR("Please check in the Android SDK directory specified in Editor Settings.");
			err += "\n";
			valid = false;
		}
	}
#endif

	if (!err.is_empty()) {
		r_error = err;
	}

	return valid;
}

bool EditorExportPlatformAndroid::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
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

	if (!ResourceImporterTextureSettings::should_import_etc2_astc()) {
		valid = false;
	}

	if (p_preset->get("gradle_build/use_gradle_build")) {
		String build_version_path = ExportTemplateManager::get_android_build_directory(p_preset).get_base_dir().path_join(".build_version");
		Ref<FileAccess> f = FileAccess::open(build_version_path, FileAccess::READ);
		if (f.is_valid()) {
			String current_version = ExportTemplateManager::get_android_template_identifier(p_preset);
			String installed_version = f->get_line().strip_edges();
			if (current_version != installed_version) {
				err += vformat(TTR(MISMATCHED_VERSIONS_MESSAGE), installed_version, current_version);
				err += "\n";
			}
		}
	}

	String min_sdk_str = p_preset->get("gradle_build/min_sdk");
	int min_sdk_int = VULKAN_MIN_SDK_VERSION;
	if (!min_sdk_str.is_empty()) { // Empty means no override, nothing to do.
		if (min_sdk_str.is_valid_int()) {
			min_sdk_int = min_sdk_str.to_int();
		}
	}

	String target_sdk_str = p_preset->get("gradle_build/target_sdk");
	int target_sdk_int = DEFAULT_TARGET_SDK_VERSION;
	if (!target_sdk_str.is_empty()) { // Empty means no override, nothing to do.
		if (target_sdk_str.is_valid_int()) {
			target_sdk_int = target_sdk_str.to_int();
			if (target_sdk_int > DEFAULT_TARGET_SDK_VERSION) {
				// Warning only, so don't override `valid`.
				err += vformat(TTR("\"Target SDK\" %d is higher than the default version %d. This may work, but wasn't tested and may be unstable."), target_sdk_int, DEFAULT_TARGET_SDK_VERSION);
				err += "\n";
			}
		}
	}

	String current_renderer = GLOBAL_GET("rendering/renderer/rendering_method.mobile");
	if (current_renderer == "forward_plus") {
		// Warning only, so don't override `valid`.
		err += vformat(TTR("The \"%s\" renderer is designed for Desktop devices, and is not suitable for Android devices."), current_renderer);
		err += "\n";
	}

	if (_uses_vulkan() && min_sdk_int < VULKAN_MIN_SDK_VERSION) {
		// Warning only, so don't override `valid`.
		err += vformat(TTR("\"Min SDK\" should be greater or equal to %d for the \"%s\" renderer."), VULKAN_MIN_SDK_VERSION, current_renderer);
		err += "\n";
	}

	String package_name = p_preset->get("package/unique_name");
	if (package_name.contains("$genname") && !is_project_name_valid()) {
		// Warning only, so don't override `valid`.
		err += vformat(TTR("The project name does not meet the requirement for the package name format and will be updated to \"%s\". Please explicitly specify the package name if needed."), get_valid_basename());
		err += "\n";
	}

	r_error = err;
	return valid;
}

List<String> EditorExportPlatformAndroid::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("apk");
	list.push_back("aab");
	return list;
}

String EditorExportPlatformAndroid::get_apk_expansion_fullpath(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
	int version_code = p_preset->get("version/code");
	String package_name = p_preset->get("package/unique_name");
	String apk_file_name = "main." + itos(version_code) + "." + get_package_name(package_name) + ".obb";
	String fullpath = p_path.get_base_dir().path_join(apk_file_name);
	return fullpath;
}

Error EditorExportPlatformAndroid::save_apk_expansion_file(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path) {
	String fullpath = get_apk_expansion_fullpath(p_preset, p_path);
	Error err = save_pack(p_preset, p_debug, fullpath);
	return err;
}

void EditorExportPlatformAndroid::get_command_line_flags(const Ref<EditorExportPreset> &p_preset, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, Vector<uint8_t> &r_command_line_flags) {
	String cmdline = p_preset->get("command_line/extra_args");
	Vector<String> command_line_strings = cmdline.strip_edges().split(" ");
	for (int i = 0; i < command_line_strings.size(); i++) {
		if (command_line_strings[i].strip_edges().length() == 0) {
			command_line_strings.remove_at(i);
			i--;
		}
	}

	command_line_strings.append_array(gen_export_flags(p_flags));

	bool apk_expansion = p_preset->get("apk_expansion/enable");
	if (apk_expansion) {
		String fullpath = get_apk_expansion_fullpath(p_preset, p_path);
		String apk_expansion_public_key = p_preset->get("apk_expansion/public_key");

		command_line_strings.push_back("--use_apk_expansion");
		command_line_strings.push_back("--apk_expansion_md5");
		command_line_strings.push_back(FileAccess::get_md5(fullpath));
		command_line_strings.push_back("--apk_expansion_key");
		command_line_strings.push_back(apk_expansion_public_key.strip_edges());
	}

	int xr_mode_index = p_preset->get("xr_features/xr_mode");
	if (xr_mode_index == XR_MODE_OPENXR) {
		command_line_strings.push_back("--xr_mode_openxr");
	} else { // XRMode.REGULAR is the default.
		command_line_strings.push_back("--xr_mode_regular");

		// Also override the 'xr/openxr/enabled' project setting.
		// This is useful for multi-platforms projects supporting both XR and non-XR devices. The project would need
		// to enable openxr for development, and would create multiple XR and non-XR export presets.
		// These command line args ensure that the non-XR export presets will have openxr disabled.
		command_line_strings.push_back("--xr-mode");
		command_line_strings.push_back("off");
	}

	bool immersive = p_preset->get("screen/immersive_mode");
	if (immersive) {
		command_line_strings.push_back("--fullscreen");
	}

	bool debug_opengl = p_preset->get("graphics/opengl_debug");
	if (debug_opengl) {
		command_line_strings.push_back("--debug_opengl");
	}

	if (command_line_strings.size()) {
		r_command_line_flags.resize(4);
		encode_uint32(command_line_strings.size(), &r_command_line_flags.write[0]);
		for (int i = 0; i < command_line_strings.size(); i++) {
			print_line(itos(i) + " param: " + command_line_strings[i]);
			CharString command_line_argument = command_line_strings[i].utf8();
			int base = r_command_line_flags.size();
			int length = command_line_argument.length();
			if (length == 0) {
				continue;
			}
			r_command_line_flags.resize(base + 4 + length);
			encode_uint32(length, &r_command_line_flags.write[base]);
			memcpy(&r_command_line_flags.write[base + 4], command_line_argument.ptr(), length);
		}
	}
}

Error EditorExportPlatformAndroid::sign_apk(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &export_path, EditorProgress &ep) {
	int export_format = int(p_preset->get("gradle_build/export_format"));
	if (export_format == EXPORT_FORMAT_AAB) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("AAB signing is not supported"));
		return FAILED;
	}

	String keystore;
	String password;
	String user;
	if (p_debug) {
		keystore = _get_keystore_path(p_preset, true);
		password = p_preset->get_or_env("keystore/debug_password", ENV_ANDROID_KEYSTORE_DEBUG_PASS);
		user = p_preset->get_or_env("keystore/debug_user", ENV_ANDROID_KEYSTORE_DEBUG_USER);

		if (keystore.is_empty()) {
			keystore = EDITOR_GET("export/android/debug_keystore");
			password = EDITOR_GET("export/android/debug_keystore_pass");
			user = EDITOR_GET("export/android/debug_keystore_user");
		}

		if (ep.step(TTR("Signing debug APK..."), 104)) {
			return ERR_SKIP;
		}
	} else {
		keystore = _get_keystore_path(p_preset, false);
		password = p_preset->get_or_env("keystore/release_password", ENV_ANDROID_KEYSTORE_RELEASE_PASS);
		user = p_preset->get_or_env("keystore/release_user", ENV_ANDROID_KEYSTORE_RELEASE_USER);

		if (ep.step(TTR("Signing release APK..."), 104)) {
			return ERR_SKIP;
		}
	}

	if (!FileAccess::exists(keystore)) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Could not find keystore, unable to export."));
		return ERR_FILE_CANT_OPEN;
	}

	String apk_path = export_path;
	if (apk_path.is_relative_path()) {
		apk_path = OS::get_singleton()->get_resource_dir().path_join(apk_path);
	}
	apk_path = ProjectSettings::get_singleton()->globalize_path(apk_path).simplify_path();

	Error err;
#ifdef ANDROID_ENABLED
	err = OS_Android::get_singleton()->sign_apk(apk_path, apk_path, keystore, user, password);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("Unable to sign apk."));
		return err;
	}
#else
	String target_sdk_version = p_preset->get("gradle_build/target_sdk");
	if (!target_sdk_version.is_valid_int()) {
		target_sdk_version = itos(DEFAULT_TARGET_SDK_VERSION);
	}

	String apksigner = get_apksigner_path(target_sdk_version.to_int(), true);
	print_verbose("Starting signing of the APK binary using " + apksigner);
	if (apksigner == "<FAILED>") {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("All 'apksigner' tools located in Android SDK 'build-tools' directory failed to execute. Please check that you have the correct version installed for your target sdk version. The resulting APK is unsigned."));
		return OK;
	}
	if (!FileAccess::exists(apksigner)) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("'apksigner' could not be found. Please check that the command is available in the Android SDK build-tools directory. The resulting APK is unsigned."));
		return OK;
	}

	String output;
	List<String> args;
	args.push_back("sign");
	args.push_back("--verbose");
	args.push_back("--ks");
	args.push_back(keystore);
	args.push_back("--ks-pass");
	args.push_back("pass:" + password);
	args.push_back("--ks-key-alias");
	args.push_back(user);
	args.push_back(apk_path);
	if (OS::get_singleton()->is_stdout_verbose() && p_debug) {
		// We only print verbose logs with credentials for debug builds to avoid leaking release keystore credentials.
		print_verbose("Signing debug binary using: " + String("\n") + apksigner + " " + join_list(args, String(" ")));
	} else {
		List<String> redacted_args = List<String>(args);
		redacted_args.find(keystore)->set("<REDACTED>");
		redacted_args.find("pass:" + password)->set("pass:<REDACTED>");
		redacted_args.find(user)->set("<REDACTED>");
		print_line("Signing binary using: " + String("\n") + apksigner + " " + join_list(redacted_args, String(" ")));
	}
	int retval;
	err = OS::get_singleton()->execute(apksigner, args, &output, &retval, true);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Could not start apksigner executable."));
		return err;
	}
	// By design, apksigner does not output credentials in its output unless --verbose is used
	print_line(output);
	if (retval) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("'apksigner' returned with error #%d"), retval));
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("output: \n%s"), output));
		return ERR_CANT_CREATE;
	}
#endif

	if (ep.step(TTR("Verifying APK..."), 105)) {
		return ERR_SKIP;
	}

#ifdef ANDROID_ENABLED
	err = OS_Android::get_singleton()->verify_apk(apk_path);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("Unable to verify signed apk."));
		return err;
	}
#else
	args.clear();
	args.push_back("verify");
	args.push_back("--verbose");
	args.push_back(apk_path);
	if (p_debug) {
		print_verbose("Verifying signed build using: " + String("\n") + apksigner + " " + join_list(args, String(" ")));
	}

	output.clear();
	err = OS::get_singleton()->execute(apksigner, args, &output, &retval, true);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("Could not start apksigner executable."));
		return err;
	}
	print_verbose(output);
	if (retval) {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), TTR("'apksigner' verification of APK failed."));
		add_message(EXPORT_MESSAGE_WARNING, TTR("Code Signing"), vformat(TTR("output: \n%s"), output));
		return ERR_CANT_CREATE;
	}
#endif

	print_verbose("Successfully completed signing build.");

#ifdef ANDROID_ENABLED
	bool prompt_apk_install = EDITOR_GET("export/android/install_exported_apk");
	if (prompt_apk_install) {
		OS_Android::get_singleton()->shell_open(apk_path);
	}
#endif

	return OK;
}

void EditorExportPlatformAndroid::_clear_assets_directory(const Ref<EditorExportPreset> &p_preset) {
	Ref<DirAccess> da_res = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	String gradle_build_directory = ExportTemplateManager::get_android_build_directory(p_preset);

	// Clear the APK assets directory
	String apk_assets_directory = gradle_build_directory.path_join(APK_ASSETS_DIRECTORY);
	if (da_res->dir_exists(apk_assets_directory)) {
		print_verbose("Clearing APK assets directory...");
		Ref<DirAccess> da_assets = DirAccess::open(apk_assets_directory);
		ERR_FAIL_COND(da_assets.is_null());

		da_assets->erase_contents_recursive();
		da_res->remove(apk_assets_directory);
	}

	// Clear the AAB assets directory
	String aab_assets_directory = gradle_build_directory.path_join(AAB_ASSETS_DIRECTORY);
	if (da_res->dir_exists(aab_assets_directory)) {
		print_verbose("Clearing AAB assets directory...");
		Ref<DirAccess> da_assets = DirAccess::open(aab_assets_directory);
		ERR_FAIL_COND(da_assets.is_null());

		da_assets->erase_contents_recursive();
		da_res->remove(aab_assets_directory);
	}
}

void EditorExportPlatformAndroid::_remove_copied_libs(String p_gdextension_libs_path) {
	print_verbose("Removing previously installed libraries...");
	Error error;
	String libs_json = FileAccess::get_file_as_string(p_gdextension_libs_path, &error);
	if (error || libs_json.is_empty()) {
		print_verbose("No previously installed libraries found");
		return;
	}

	JSON json;
	error = json.parse(libs_json);
	ERR_FAIL_COND_MSG(error, "Error parsing \"" + libs_json + "\" on line " + itos(json.get_error_line()) + ": " + json.get_error_message());

	Vector<String> libs = json.get_data();
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	for (int i = 0; i < libs.size(); i++) {
		print_verbose("Removing previously installed library " + libs[i]);
		da->remove(libs[i]);
	}
	da->remove(p_gdextension_libs_path);
}

String EditorExportPlatformAndroid::join_list(const List<String> &p_parts, const String &p_separator) {
	String ret;
	for (List<String>::ConstIterator itr = p_parts.begin(); itr != p_parts.end(); ++itr) {
		if (itr != p_parts.begin()) {
			ret += p_separator;
		}
		ret += *itr;
	}
	return ret;
}

String EditorExportPlatformAndroid::join_abis(const Vector<EditorExportPlatformAndroid::ABI> &p_parts, const String &p_separator, bool p_use_arch) {
	String ret;
	for (int i = 0; i < p_parts.size(); ++i) {
		if (i > 0) {
			ret += p_separator;
		}
		ret += (p_use_arch) ? p_parts[i].arch : p_parts[i].abi;
	}
	return ret;
}

String EditorExportPlatformAndroid::_get_plugins_names(const Ref<EditorExportPreset> &p_preset) const {
	Vector<String> names;

#ifndef DISABLE_DEPRECATED
	PluginConfigAndroid::get_plugins_names(get_enabled_plugins(p_preset), names);
#endif // DISABLE_DEPRECATED

	Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
	for (int i = 0; i < export_plugins.size(); i++) {
		if (export_plugins[i]->supports_platform(Ref<EditorExportPlatform>(this))) {
			names.push_back(export_plugins[i]->get_name());
		}
	}

	String plugins_names = String("|").join(names);
	return plugins_names;
}

String EditorExportPlatformAndroid::_resolve_export_plugin_android_library_path(const String &p_android_library_path) const {
	String absolute_path;
	if (!p_android_library_path.is_empty()) {
		if (p_android_library_path.is_absolute_path()) {
			absolute_path = ProjectSettings::get_singleton()->globalize_path(p_android_library_path);
		} else {
			const String export_plugin_absolute_path = String("res://addons/").path_join(p_android_library_path);
			absolute_path = ProjectSettings::get_singleton()->globalize_path(export_plugin_absolute_path);
		}
	}
	return absolute_path;
}

bool EditorExportPlatformAndroid::_is_clean_build_required(const Ref<EditorExportPreset> &p_preset) {
	bool first_build = last_gradle_build_time == 0;
	bool have_plugins_changed = false;
	String gradle_build_dir = ExportTemplateManager::get_android_build_directory(p_preset);
	bool has_build_dir_changed = last_gradle_build_dir != gradle_build_dir;

	String plugin_names = _get_plugins_names(p_preset);

	if (!first_build) {
		have_plugins_changed = plugin_names != last_plugin_names;
#ifndef DISABLE_DEPRECATED
		if (!have_plugins_changed) {
			Vector<PluginConfigAndroid> enabled_plugins = get_enabled_plugins(p_preset);
			for (int i = 0; i < enabled_plugins.size(); i++) {
				if (enabled_plugins.get(i).last_updated > last_gradle_build_time) {
					have_plugins_changed = true;
					break;
				}
			}
		}
#endif // DISABLE_DEPRECATED
	}

	last_gradle_build_time = OS::get_singleton()->get_unix_time();
	last_gradle_build_dir = gradle_build_dir;
	last_plugin_names = plugin_names;

	return have_plugins_changed || has_build_dir_changed || first_build;
}

Error EditorExportPlatformAndroid::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	int export_format = int(p_preset->get("gradle_build/export_format"));
	bool should_sign = p_preset->get("package/signed");
	return export_project_helper(p_preset, p_debug, p_path, export_format, should_sign, p_flags);
}

Error EditorExportPlatformAndroid::export_project_helper(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int export_format, bool should_sign, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

	const String base_dir = p_path.get_base_dir();
	if (!DirAccess::exists(base_dir)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Target folder does not exist or is inaccessible: \"%s\""), base_dir));
		return ERR_FILE_BAD_PATH;
	}

	String src_apk;
	Error err;

	EditorProgress ep("export", TTR("Exporting for Android"), 105, true);

	bool use_gradle_build = bool(p_preset->get("gradle_build/use_gradle_build"));
	String gradle_build_directory = use_gradle_build ? ExportTemplateManager::get_android_build_directory(p_preset) : "";
	bool p_give_internet = p_flags.has_flag(DEBUG_FLAG_DUMB_CLIENT) || p_flags.has_flag(DEBUG_FLAG_REMOTE_DEBUG);
	bool apk_expansion = p_preset->get("apk_expansion/enable");
	Vector<ABI> enabled_abis = get_enabled_abis(p_preset);

	print_verbose("Exporting for Android...");
	print_verbose("- debug build: " + bool_to_string(p_debug));
	print_verbose("- export path: " + p_path);
	print_verbose("- export format: " + itos(export_format));
	print_verbose("- sign build: " + bool_to_string(should_sign));
	print_verbose("- gradle build enabled: " + bool_to_string(use_gradle_build));
	print_verbose("- apk expansion enabled: " + bool_to_string(apk_expansion));
	print_verbose("- enabled abis: " + join_abis(enabled_abis, ",", false));
	print_verbose("- export filter: " + itos(p_preset->get_export_filter()));
	print_verbose("- include filter: " + p_preset->get_include_filter());
	print_verbose("- exclude filter: " + p_preset->get_exclude_filter());

	Ref<Image> main_image;
	Ref<Image> foreground;
	Ref<Image> background;
	Ref<Image> monochrome;

	load_icon_refs(p_preset, main_image, foreground, background, monochrome);

	Vector<uint8_t> command_line_flags;
	// Write command line flags into the command_line_flags variable.
	get_command_line_flags(p_preset, p_path, p_flags, command_line_flags);

	if (export_format == EXPORT_FORMAT_AAB) {
		if (!p_path.ends_with(".aab")) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Invalid filename! Android App Bundle requires the *.aab extension."));
			return ERR_UNCONFIGURED;
		}
		if (apk_expansion) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("APK Expansion not compatible with Android App Bundle."));
			return ERR_UNCONFIGURED;
		}
	}
	if (export_format == EXPORT_FORMAT_APK && !p_path.ends_with(".apk")) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Invalid filename! Android APK requires the *.apk extension."));
		return ERR_UNCONFIGURED;
	}
	if (export_format > EXPORT_FORMAT_AAB || export_format < EXPORT_FORMAT_APK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Unsupported export format!"));
		return ERR_UNCONFIGURED;
	}
	String err_string;
	if (!has_valid_username_and_password(p_preset, err_string)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR(err_string));
		return ERR_UNCONFIGURED;
	}

	if (use_gradle_build) {
		print_verbose("Starting gradle build...");
		//test that installed build version is alright
		{
			print_verbose("Checking build version...");
			String gradle_base_directory = gradle_build_directory.get_base_dir();
			Ref<FileAccess> f = FileAccess::open(gradle_base_directory.path_join(".build_version"), FileAccess::READ);
			if (f.is_null()) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Trying to build from a gradle built template, but no version info for it exists. Please reinstall from the 'Project' menu."));
				return ERR_UNCONFIGURED;
			}
			String current_version = ExportTemplateManager::get_android_template_identifier(p_preset);
			String installed_version = f->get_line().strip_edges();
			print_verbose("- build version: " + installed_version);
			if (installed_version != current_version) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR(MISMATCHED_VERSIONS_MESSAGE), installed_version, current_version));
				return ERR_UNCONFIGURED;
			}
		}
		const String assets_directory = get_assets_directory(p_preset, export_format);
		String java_sdk_path = EDITOR_GET("export/android/java_sdk_path");
		if (java_sdk_path.is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Java SDK path must be configured in Editor Settings at 'export/android/java_sdk_path'."));
			return ERR_UNCONFIGURED;
		}
		print_verbose("Java sdk path: " + java_sdk_path);

		String sdk_path = EDITOR_GET("export/android/android_sdk_path");
		if (sdk_path.is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Android SDK path must be configured in Editor Settings at 'export/android/android_sdk_path'."));
			return ERR_UNCONFIGURED;
		}
		print_verbose("Android sdk path: " + sdk_path);

		// TODO: should we use "package/name" or "application/config/name"?
		String project_name = get_project_name(p_preset->get("package/name"));
		err = _create_project_name_strings_files(p_preset, project_name, gradle_build_directory); //project name localization.
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Unable to overwrite res/*.xml files with project name."));
		}
		// Copies the project icon files into the appropriate Gradle project directory.
		_copy_icons_to_gradle_project(p_preset, main_image, foreground, background, monochrome);
		// Write an AndroidManifest.xml file into the Gradle project directory.
		_write_tmp_manifest(p_preset, p_give_internet, p_debug);
		// Modify res/values/themes.xml file.
		_fix_themes_xml(p_preset);

		//stores all the project files inside the Gradle project directory. Also includes all ABIs
		_clear_assets_directory(p_preset);
		String gdextension_libs_path = gradle_build_directory.path_join(GDEXTENSION_LIBS_PATH);
		_remove_copied_libs(gdextension_libs_path);
		if (!apk_expansion) {
			print_verbose("Exporting project files...");
			CustomExportData user_data;
			user_data.assets_directory = assets_directory;
			user_data.libs_directory = gradle_build_directory.path_join("libs");
			user_data.debug = p_debug;
			if (p_flags.has_flag(DEBUG_FLAG_DUMB_CLIENT)) {
				err = export_project_files(p_preset, p_debug, ignore_apk_file, nullptr, &user_data, copy_gradle_so);
			} else {
				err = export_project_files(p_preset, p_debug, rename_and_store_file_in_gradle_project, nullptr, &user_data, copy_gradle_so);
			}
			if (err != OK) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not export project files to gradle project."));
				return err;
			}
			if (user_data.libs.size() > 0) {
				Ref<FileAccess> fa = FileAccess::open(gdextension_libs_path, FileAccess::WRITE);
				fa->store_string(JSON::stringify(user_data.libs, "\t"));
			}
		} else {
			print_verbose("Saving apk expansion file...");
			err = save_apk_expansion_file(p_preset, p_debug, p_path);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not write expansion package file!"));
				return err;
			}
		}
		print_verbose("Storing command line flags...");
		store_file_at_path(assets_directory + "/_cl_", command_line_flags);

		print_verbose("Updating JAVA_HOME environment to " + java_sdk_path);
		OS::get_singleton()->set_environment("JAVA_HOME", java_sdk_path);

		print_verbose("Updating ANDROID_HOME environment to " + sdk_path);
		OS::get_singleton()->set_environment("ANDROID_HOME", sdk_path);
		String build_command;

#ifdef WINDOWS_ENABLED
		build_command = "gradlew.bat";
#else
		build_command = "gradlew";
#endif

		String build_path = ProjectSettings::get_singleton()->globalize_path(gradle_build_directory);
		build_command = build_path.path_join(build_command);

		String package_name = get_package_name(p_preset->get("package/unique_name"));
		String version_code = itos(p_preset->get("version/code"));
		String version_name = p_preset->get_version("version/name");
		String min_sdk_version = p_preset->get("gradle_build/min_sdk");
		if (!min_sdk_version.is_valid_int()) {
			min_sdk_version = itos(VULKAN_MIN_SDK_VERSION);
		}
		String target_sdk_version = p_preset->get("gradle_build/target_sdk");
		if (!target_sdk_version.is_valid_int()) {
			target_sdk_version = itos(DEFAULT_TARGET_SDK_VERSION);
		}
		String enabled_abi_string = join_abis(enabled_abis, "|", false);
		String sign_flag = should_sign ? "true" : "false";
		String zipalign_flag = "true";
		String compress_native_libraries_flag = bool(p_preset->get("gradle_build/compress_native_libraries")) ? "true" : "false";

		Vector<String> android_libraries;
		Vector<String> android_dependencies;
		Vector<String> android_dependencies_maven_repos;

#ifndef DISABLE_DEPRECATED
		Vector<PluginConfigAndroid> enabled_plugins = get_enabled_plugins(p_preset);
		PluginConfigAndroid::get_plugins_binaries(PluginConfigAndroid::BINARY_TYPE_LOCAL, enabled_plugins, android_libraries);
		PluginConfigAndroid::get_plugins_binaries(PluginConfigAndroid::BINARY_TYPE_REMOTE, enabled_plugins, android_dependencies);
		PluginConfigAndroid::get_plugins_custom_maven_repos(enabled_plugins, android_dependencies_maven_repos);
#endif // DISABLE_DEPRECATED

		bool has_dotnet_project = false;
		Vector<Ref<EditorExportPlugin>> export_plugins = EditorExport::get_singleton()->get_export_plugins();
		for (int i = 0; i < export_plugins.size(); i++) {
			if (export_plugins[i]->supports_platform(Ref<EditorExportPlatform>(this))) {
				PackedStringArray export_plugin_android_libraries = export_plugins[i]->get_android_libraries(Ref<EditorExportPlatform>(this), p_debug);
				for (int k = 0; k < export_plugin_android_libraries.size(); k++) {
					const String resolved_android_library_path = _resolve_export_plugin_android_library_path(export_plugin_android_libraries[k]);
					if (!resolved_android_library_path.is_empty()) {
						android_libraries.push_back(resolved_android_library_path);
					}
				}

				PackedStringArray export_plugin_android_dependencies = export_plugins[i]->get_android_dependencies(Ref<EditorExportPlatform>(this), p_debug);
				android_dependencies.append_array(export_plugin_android_dependencies);

				PackedStringArray export_plugin_android_dependencies_maven_repos = export_plugins[i]->get_android_dependencies_maven_repos(Ref<EditorExportPlatform>(this), p_debug);
				android_dependencies_maven_repos.append_array(export_plugin_android_dependencies_maven_repos);
			}

			PackedStringArray features = export_plugins[i]->get_export_features(Ref<EditorExportPlatform>(this), p_debug);
			if (features.has("dotnet")) {
				has_dotnet_project = true;
			}
		}

		bool clean_build_required = _is_clean_build_required(p_preset);
		String combined_android_libraries = String("|").join(android_libraries);
		String combined_android_dependencies = String("|").join(android_dependencies);
		String combined_android_dependencies_maven_repos = String("|").join(android_dependencies_maven_repos);

		List<String> cmdline;
		cmdline.push_back("validateJavaVersion");
		if (clean_build_required) {
			cmdline.push_back("clean");
		}

		String edition = has_dotnet_project ? "Mono" : "Standard";
		String build_type = p_debug ? "Debug" : "Release";
		if (export_format == EXPORT_FORMAT_AAB) {
			String bundle_build_command = vformat("bundle%s", build_type);
			cmdline.push_back(bundle_build_command);
		} else if (export_format == EXPORT_FORMAT_APK) {
			String apk_build_command = vformat("assemble%s%s", edition, build_type);
			cmdline.push_back(apk_build_command);
		}

		String addons_directory = ProjectSettings::get_singleton()->globalize_path("res://addons");

		cmdline.push_back("-p"); // argument to specify the start directory.
		cmdline.push_back(build_path); // start directory.
		cmdline.push_back("-Paddons_directory=" + addons_directory); // path to the addon directory as it may contain jar or aar dependencies
		cmdline.push_back("-Pexport_package_name=" + package_name); // argument to specify the package name.
		cmdline.push_back("-Pexport_version_code=" + version_code); // argument to specify the version code.
		cmdline.push_back("-Pexport_version_name=" + version_name); // argument to specify the version name.
		cmdline.push_back("-Pexport_version_min_sdk=" + min_sdk_version); // argument to specify the min sdk.
		cmdline.push_back("-Pexport_version_target_sdk=" + target_sdk_version); // argument to specify the target sdk.
		cmdline.push_back("-Pexport_enabled_abis=" + enabled_abi_string); // argument to specify enabled ABIs.
		cmdline.push_back("-Pplugins_local_binaries=" + combined_android_libraries); // argument to specify the list of android libraries provided by plugins.
		cmdline.push_back("-Pplugins_remote_binaries=" + combined_android_dependencies); // argument to specify the list of android dependencies provided by plugins.
		cmdline.push_back("-Pplugins_maven_repos=" + combined_android_dependencies_maven_repos); // argument to specify the list of maven repos for android dependencies provided by plugins.
		cmdline.push_back("-Pperform_zipalign=" + zipalign_flag); // argument to specify whether the build should be zipaligned.
		cmdline.push_back("-Pperform_signing=" + sign_flag); // argument to specify whether the build should be signed.
		cmdline.push_back("-Pcompress_native_libraries=" + compress_native_libraries_flag); // argument to specify whether the build should compress native libraries.
		cmdline.push_back("-Pgodot_editor_version=" + String(VERSION_FULL_CONFIG));

		// NOTE: The release keystore is not included in the verbose logging
		// to avoid accidentally leaking sensitive information when sharing verbose logs for troubleshooting.
		// Any non-sensitive additions to the command line arguments must be done above this section.
		// Sensitive additions must be done below the logging statement.
		print_verbose("Build Android project using gradle command: " + String("\n") + build_command + " " + join_list(cmdline, String(" ")));

		if (should_sign) {
			if (p_debug) {
				String debug_keystore = _get_keystore_path(p_preset, true);
				String debug_password = p_preset->get_or_env("keystore/debug_password", ENV_ANDROID_KEYSTORE_DEBUG_PASS);
				String debug_user = p_preset->get_or_env("keystore/debug_user", ENV_ANDROID_KEYSTORE_DEBUG_USER);

				if (debug_keystore.is_empty()) {
					debug_keystore = EDITOR_GET("export/android/debug_keystore");
					debug_password = EDITOR_GET("export/android/debug_keystore_pass");
					debug_user = EDITOR_GET("export/android/debug_keystore_user");
				}
				if (debug_keystore.is_relative_path()) {
					debug_keystore = OS::get_singleton()->get_resource_dir().path_join(debug_keystore).simplify_path();
				}
				if (!FileAccess::exists(debug_keystore)) {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("Could not find keystore, unable to export."));
					return ERR_FILE_CANT_OPEN;
				}

				cmdline.push_back("-Pdebug_keystore_file=" + debug_keystore); // argument to specify the debug keystore file.
				cmdline.push_back("-Pdebug_keystore_alias=" + debug_user); // argument to specify the debug keystore alias.
				cmdline.push_back("-Pdebug_keystore_password=" + debug_password); // argument to specify the debug keystore password.
			} else {
				// Pass the release keystore info as well
				String release_keystore = _get_keystore_path(p_preset, false);
				String release_username = p_preset->get_or_env("keystore/release_user", ENV_ANDROID_KEYSTORE_RELEASE_USER);
				String release_password = p_preset->get_or_env("keystore/release_password", ENV_ANDROID_KEYSTORE_RELEASE_PASS);
				if (release_keystore.is_relative_path()) {
					release_keystore = OS::get_singleton()->get_resource_dir().path_join(release_keystore).simplify_path();
				}
				if (!FileAccess::exists(release_keystore)) {
					add_message(EXPORT_MESSAGE_ERROR, TTR("Code Signing"), TTR("Could not find keystore, unable to export."));
					return ERR_FILE_CANT_OPEN;
				}

				cmdline.push_back("-Prelease_keystore_file=" + release_keystore); // argument to specify the release keystore file.
				cmdline.push_back("-Prelease_keystore_alias=" + release_username); // argument to specify the release keystore alias.
				cmdline.push_back("-Prelease_keystore_password=" + release_password); // argument to specify the release keystore password.
			}
		}

		String build_project_output;
		int result = EditorNode::get_singleton()->execute_and_show_output(TTR("Building Android Project (gradle)"), build_command, cmdline, true, false, &build_project_output);
		if (result != 0) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Building of Android project failed, check output for the error:") + "\n\n" + build_project_output);
			return ERR_CANT_CREATE;
		} else {
			print_verbose(build_project_output);
		}

		List<String> copy_args;
		String copy_command = "copyAndRenameBinary";
		copy_args.push_back(copy_command);

		copy_args.push_back("-p"); // argument to specify the start directory.
		copy_args.push_back(build_path); // start directory.

		copy_args.push_back("-Pexport_edition=" + edition.to_lower());

		copy_args.push_back("-Pexport_build_type=" + build_type.to_lower());

		String export_format_arg = export_format == EXPORT_FORMAT_AAB ? "aab" : "apk";
		copy_args.push_back("-Pexport_format=" + export_format_arg);

		String export_filename = p_path.get_file();
		String export_path = p_path.get_base_dir();
		if (export_path.is_relative_path()) {
			export_path = OS::get_singleton()->get_resource_dir().path_join(export_path);
		}
		export_path = ProjectSettings::get_singleton()->globalize_path(export_path).simplify_path();

		copy_args.push_back("-Pexport_path=file:" + export_path);
		copy_args.push_back("-Pexport_filename=" + export_filename);

		print_verbose("Copying Android binary using gradle command: " + String("\n") + build_command + " " + join_list(copy_args, String(" ")));
		String copy_binary_output;
		int copy_result = EditorNode::get_singleton()->execute_and_show_output(TTR("Moving output"), build_command, copy_args, true, false, &copy_binary_output);
		if (copy_result != 0) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Unable to copy and rename export file:") + "\n\n" + copy_binary_output);
			return ERR_CANT_CREATE;
		} else {
			print_verbose(copy_binary_output);
		}

		print_verbose("Successfully completed Android gradle build.");
		return OK;
	}
	// This is the start of the Legacy build system
	print_verbose("Starting legacy build system...");
	if (p_debug) {
		src_apk = p_preset->get("custom_template/debug");
	} else {
		src_apk = p_preset->get("custom_template/release");
	}
	src_apk = src_apk.strip_edges();
	if (src_apk.is_empty()) {
		if (p_debug) {
			src_apk = find_export_template("android_debug.apk");
		} else {
			src_apk = find_export_template("android_release.apk");
		}
		if (src_apk.is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(p_debug ? TTR("Debug export template not found: \"%s\".") : TTR("Release export template not found: \"%s\"."), src_apk));
			return ERR_FILE_NOT_FOUND;
		}
	}

	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);

	if (ep.step(TTR("Creating APK..."), 0)) {
		return ERR_SKIP;
	}

	unzFile pkg = unzOpen2(src_apk.utf8().get_data(), &io);
	if (!pkg) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not find template APK to export: \"%s\"."), src_apk));
		return ERR_FILE_NOT_FOUND;
	}

	int ret = unzGoToFirstFile(pkg);

	Ref<FileAccess> io2_fa;
	zlib_filefunc_def io2 = zipio_create_io(&io2_fa);

	String tmp_unaligned_path = EditorPaths::get_singleton()->get_temp_dir().path_join("tmpexport-unaligned." + uitos(OS::get_singleton()->get_unix_time()) + ".apk");

#define CLEANUP_AND_RETURN(m_err)                            \
	{                                                        \
		DirAccess::remove_file_or_error(tmp_unaligned_path); \
		return m_err;                                        \
	}                                                        \
	((void)0)

	zipFile unaligned_apk = zipOpen2(tmp_unaligned_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io2);

	String cmdline = p_preset->get("command_line/extra_args");

	String version_name = p_preset->get_version("version/name");
	String package_name = p_preset->get("package/unique_name");

	String apk_expansion_pkey = p_preset->get("apk_expansion/public_key");

	Vector<ABI> invalid_abis(enabled_abis);

	//To temporarily store icon xml data.
	Vector<uint8_t> themed_icon_xml_data;
	int icon_xml_compression_method = -1;

	while (ret == UNZ_OK) {
		//get filename
		unz_file_info info;
		char fname[16384];
		ret = unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);
		if (ret != UNZ_OK) {
			break;
		}

		bool skip = false;

		String file = String::utf8(fname);

		Vector<uint8_t> data;
		data.resize(info.uncompressed_size);

		//read
		unzOpenCurrentFile(pkg);
		unzReadCurrentFile(pkg, data.ptrw(), data.size());
		unzCloseCurrentFile(pkg);

		//write
		if (file == "AndroidManifest.xml") {
			_fix_manifest(p_preset, data, p_give_internet);
		}
		if (file == "resources.arsc") {
			_fix_resources(p_preset, data);
		}

		if (file == THEMED_ICON_XML_PATH) {
			// Store themed_icon.xml data.
			themed_icon_xml_data = data;
			skip = true;
		}

		if (file == ICON_XML_PATH) {
			if (monochrome.is_valid() && !monochrome->is_empty()) {
				// Defer processing of icon.xml until after themed_icon.xml is read.
				icon_xml_compression_method = info.compression_method;
				skip = true;
			}
		}

		if (file.ends_with(".png") && file.contains("mipmap")) {
			for (int i = 0; i < ICON_DENSITIES_COUNT; ++i) {
				if (main_image.is_valid() && !main_image->is_empty()) {
					if (file == LAUNCHER_ICONS[i].export_path) {
						_process_launcher_icons(file, main_image, LAUNCHER_ICONS[i].dimensions, data);
					}
				}
				if (foreground.is_valid() && !foreground->is_empty()) {
					if (file == LAUNCHER_ADAPTIVE_ICON_FOREGROUNDS[i].export_path) {
						_process_launcher_icons(file, foreground, LAUNCHER_ADAPTIVE_ICON_FOREGROUNDS[i].dimensions, data);
					}
				}
				if (background.is_valid() && !background->is_empty()) {
					if (file == LAUNCHER_ADAPTIVE_ICON_BACKGROUNDS[i].export_path) {
						_process_launcher_icons(file, background, LAUNCHER_ADAPTIVE_ICON_BACKGROUNDS[i].dimensions, data);
					}
				}
				if (monochrome.is_valid() && !monochrome->is_empty()) {
					if (file == LAUNCHER_ADAPTIVE_ICON_MONOCHROMES[i].export_path) {
						_process_launcher_icons(file, monochrome, LAUNCHER_ADAPTIVE_ICON_MONOCHROMES[i].dimensions, data);
					}
				}
			}
		}

		if (file.ends_with(".so")) {
			bool enabled = false;
			for (int i = 0; i < enabled_abis.size(); ++i) {
				if (file.begins_with("lib/" + enabled_abis[i].abi + "/")) {
					invalid_abis.erase(enabled_abis[i]);
					enabled = true;
					break;
				}
			}
			if (!enabled) {
				skip = true;
			}
		}

		if (file.begins_with("META-INF") && should_sign) {
			skip = true;
		}

		if (!skip) {
			print_line("ADDING: " + file);

			// Respect decision on compression made by AAPT for the export template
			const bool uncompressed = info.compression_method == 0;

			zip_fileinfo zipfi = get_zip_fileinfo();

			zipOpenNewFileInZip(unaligned_apk,
					file.utf8().get_data(),
					&zipfi,
					nullptr,
					0,
					nullptr,
					0,
					nullptr,
					uncompressed ? 0 : Z_DEFLATED,
					Z_DEFAULT_COMPRESSION);

			zipWriteInFileInZip(unaligned_apk, data.ptr(), data.size());
			zipCloseFileInZip(unaligned_apk);
		}

		ret = unzGoToNextFile(pkg);
	}

	// Process deferred icon.xml and replace it's data with themed_icon.xml.
	if (monochrome.is_valid() && !monochrome->is_empty()) {
		print_line("ADDING: " + ICON_XML_PATH + " (replacing with themed_icon.xml data)");

		const bool uncompressed = icon_xml_compression_method == 0;
		zip_fileinfo zipfi = get_zip_fileinfo();

		zipOpenNewFileInZip(unaligned_apk,
				ICON_XML_PATH.utf8().get_data(),
				&zipfi,
				nullptr,
				0,
				nullptr,
				0,
				nullptr,
				uncompressed ? 0 : Z_DEFLATED,
				Z_DEFAULT_COMPRESSION);

		zipWriteInFileInZip(unaligned_apk, themed_icon_xml_data.ptr(), themed_icon_xml_data.size());
		zipCloseFileInZip(unaligned_apk);
	}

	if (!invalid_abis.is_empty()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Missing libraries in the export template for the selected architectures: %s. Please build a template with all required libraries, or uncheck the missing architectures in the export preset."), join_abis(invalid_abis, ", ", false)));
		CLEANUP_AND_RETURN(ERR_FILE_NOT_FOUND);
	}

	if (ep.step(TTR("Adding files..."), 1)) {
		CLEANUP_AND_RETURN(ERR_SKIP);
	}
	err = OK;

	if (p_flags.has_flag(DEBUG_FLAG_DUMB_CLIENT)) {
		APKExportData ed;
		ed.ep = &ep;
		ed.apk = unaligned_apk;
		err = export_project_files(p_preset, p_debug, ignore_apk_file, nullptr, &ed, save_apk_so);
	} else {
		if (apk_expansion) {
			err = save_apk_expansion_file(p_preset, p_debug, p_path);
			if (err != OK) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not write expansion package file!"));
				return err;
			}
		} else {
			APKExportData ed;
			ed.ep = &ep;
			ed.apk = unaligned_apk;
			err = export_project_files(p_preset, p_debug, save_apk_file, nullptr, &ed, save_apk_so);
		}
	}

	if (err != OK) {
		unzClose(pkg);
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not export project files.")));
		CLEANUP_AND_RETURN(ERR_SKIP);
	}

	zip_fileinfo zipfi = get_zip_fileinfo();
	zipOpenNewFileInZip(unaligned_apk,
			"assets/_cl_",
			&zipfi,
			nullptr,
			0,
			nullptr,
			0,
			nullptr,
			0, // No compress (little size gain and potentially slower startup)
			Z_DEFAULT_COMPRESSION);
	zipWriteInFileInZip(unaligned_apk, command_line_flags.ptr(), command_line_flags.size());
	zipCloseFileInZip(unaligned_apk);
	zipClose(unaligned_apk, nullptr);
	unzClose(pkg);

	// Let's zip-align (must be done before signing)

	static const int ZIP_ALIGNMENT = 4;

	// If we're not signing the apk, then the next step should be the last.
	const int next_step = should_sign ? 103 : 105;
	if (ep.step(TTR("Aligning APK..."), next_step)) {
		CLEANUP_AND_RETURN(ERR_SKIP);
	}

	unzFile tmp_unaligned = unzOpen2(tmp_unaligned_path.utf8().get_data(), &io);
	if (!tmp_unaligned) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not unzip temporary unaligned APK.")));
		CLEANUP_AND_RETURN(ERR_FILE_NOT_FOUND);
	}

	ret = unzGoToFirstFile(tmp_unaligned);

	io2 = zipio_create_io(&io2_fa);
	zipFile final_apk = zipOpen2(p_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io2);

	// Take files from the unaligned APK and write them out to the aligned one
	// in raw mode, i.e. not uncompressing and recompressing, aligning them as needed,
	// following what is done in https://github.com/android/platform_build/blob/master/tools/zipalign/ZipAlign.cpp
	int bias = 0;
	while (ret == UNZ_OK) {
		unz_file_info info;
		memset(&info, 0, sizeof(info));

		char fname[16384];
		char extra[16384];
		ret = unzGetCurrentFileInfo(tmp_unaligned, &info, fname, 16384, extra, 16384 - ZIP_ALIGNMENT, nullptr, 0);
		if (ret != UNZ_OK) {
			break;
		}

		String file = String::utf8(fname);

		Vector<uint8_t> data;
		data.resize(info.compressed_size);

		// read
		int method, level;
		unzOpenCurrentFile2(tmp_unaligned, &method, &level, 1); // raw read
		long file_offset = unzGetCurrentFileZStreamPos64(tmp_unaligned);
		unzReadCurrentFile(tmp_unaligned, data.ptrw(), data.size());
		unzCloseCurrentFile(tmp_unaligned);

		// align
		int padding = 0;
		if (!info.compression_method) {
			// Uncompressed file => Align
			long new_offset = file_offset + bias;
			padding = (ZIP_ALIGNMENT - (new_offset % ZIP_ALIGNMENT)) % ZIP_ALIGNMENT;
		}

		memset(extra + info.size_file_extra, 0, padding);

		zip_fileinfo fileinfo = get_zip_fileinfo();
		zipOpenNewFileInZip2(final_apk,
				file.utf8().get_data(),
				&fileinfo,
				extra,
				info.size_file_extra + padding,
				nullptr,
				0,
				nullptr,
				method,
				level,
				1); // raw write
		zipWriteInFileInZip(final_apk, data.ptr(), data.size());
		zipCloseFileInZipRaw(final_apk, info.uncompressed_size, info.crc);

		bias += padding;

		ret = unzGoToNextFile(tmp_unaligned);
	}

	zipClose(final_apk, nullptr);
	unzClose(tmp_unaligned);

	if (should_sign) {
		// Signing must be done last as any additional modifications to the
		// file will invalidate the signature.
		err = sign_apk(p_preset, p_debug, p_path, ep);
		if (err != OK) {
			// Message is supplied by the subroutine method.
			CLEANUP_AND_RETURN(err);
		}
	}

	CLEANUP_AND_RETURN(OK);
}

void EditorExportPlatformAndroid::get_platform_features(List<String> *r_features) const {
	r_features->push_back("mobile");
	r_features->push_back("android");
}

void EditorExportPlatformAndroid::resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, HashSet<String> &p_features) {
}

EditorExportPlatformAndroid::EditorExportPlatformAndroid() {
	if (EditorNode::get_singleton()) {
		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);

		ImageLoaderSVG::create_image_from_string(img, _android_logo_svg, EDSCALE, upsample, false);
		logo = ImageTexture::create_from_image(img);

		ImageLoaderSVG::create_image_from_string(img, _android_run_icon_svg, EDSCALE, upsample, false);
		run_icon = ImageTexture::create_from_image(img);

		devices_changed.set();
#ifndef DISABLE_DEPRECATED
		android_plugins_changed.set();
#endif // DISABLE_DEPRECATED
#ifndef ANDROID_ENABLED
		_create_editor_debug_keystore_if_needed();
		_update_preset_status();
		check_for_changes_thread.start(_check_for_changes_poll_thread, this);
#endif
	}
}

EditorExportPlatformAndroid::~EditorExportPlatformAndroid() {
#ifndef ANDROID_ENABLED
	quit_request.set();
	if (check_for_changes_thread.is_started()) {
		check_for_changes_thread.wait_to_finish();
	}
#endif
}
