/**************************************************************************/
/*  export.cpp                                                            */
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

#include "export.h"

#include "export_plugin.h"

#include "core/os/os.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export.h"

String get_default_android_sdk_path();

void register_android_exporter_types() {
	GDREGISTER_VIRTUAL_CLASS(EditorExportPlatformAndroid);
}

void register_android_exporter() {
	// TODO: Move to editor_settings.cpp
	EDITOR_DEF_BASIC("export/android/debug_keystore", EditorPaths::get_singleton()->get_debug_keystore_path());
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/android/debug_keystore", PROPERTY_HINT_GLOBAL_FILE, "*.keystore,*.jks"));
	EDITOR_DEF_BASIC("export/android/debug_keystore_user", DEFAULT_ANDROID_KEYSTORE_DEBUG_USER);
	EDITOR_DEF_BASIC("export/android/debug_keystore_pass", DEFAULT_ANDROID_KEYSTORE_DEBUG_PASSWORD);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/android/debug_keystore_pass", PROPERTY_HINT_PASSWORD));

#ifdef ANDROID_ENABLED
	EDITOR_DEF_BASIC("export/android/install_exported_apk", true);
#else
	EDITOR_DEF_BASIC("export/android/java_sdk_path", OS::get_singleton()->get_environment("JAVA_HOME"));
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/android/java_sdk_path", PROPERTY_HINT_GLOBAL_DIR));

	EDITOR_DEF_BASIC("export/android/android_sdk_path", OS::get_singleton()->has_environment("ANDROID_HOME") ? OS::get_singleton()->get_environment("ANDROID_HOME") : get_default_android_sdk_path());
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "export/android/android_sdk_path", PROPERTY_HINT_GLOBAL_DIR));

	EDITOR_DEF("export/android/force_system_user", false);

	EDITOR_DEF("export/android/shutdown_adb_on_exit", true);

	EDITOR_DEF("export/android/one_click_deploy_clear_previous_install", false);

	EDITOR_DEF("export/android/use_wifi_for_remote_debug", false);
	EDITOR_DEF("export/android/wifi_remote_debug_host", "localhost");
#endif

	Ref<EditorExportPlatformAndroid> exporter = Ref<EditorExportPlatformAndroid>(memnew(EditorExportPlatformAndroid));
	EditorExport::get_singleton()->add_export_platform(exporter);
}

inline String get_default_android_sdk_path() {
#ifdef WINDOWS_ENABLED
	return OS::get_singleton()->get_environment("LOCALAPPDATA").path_join("Android/Sdk");
#elif LINUXBSD_ENABLED
	return OS::get_singleton()->get_environment("HOME").path_join("Android/Sdk");
#elif MACOS_ENABLED
	return OS::get_singleton()->get_environment("HOME").path_join("Library/Android/sdk");
#else
	return String();
#endif
}
