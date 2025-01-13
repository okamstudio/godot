/**************************************************************************/
/*  openxr_select_runtime.cpp                                             */
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

#include "openxr_select_runtime.h"

#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"

void OpenXRSelectRuntime::_update_items() {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	OS *os = OS::get_singleton();
	Dictionary runtimes = EDITOR_GET("xr/openxr/runtime_paths");

	int current_runtime = 0;
	int index = 0;
	String current_path = os->get_environment("XR_RUNTIME_JSON");

	// Parse the user's home folder.
	String home_folder = os->get_environment("HOME");
	if (home_folder.is_empty()) {
		home_folder = os->get_environment("HOMEDRIVE") + os->get_environment("HOMEPATH");
	}

	clear();
	add_item("Default", -1);
	set_item_metadata(index, "");
	index++;

	Array keys = runtimes.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		String path = runtimes[key];
		String adj_path = path.replace("~", home_folder);

		if (da->file_exists(adj_path)) {
			add_item(key, index);
			set_item_metadata(index, adj_path);

			if (current_path == adj_path) {
				current_runtime = index;
			}
			index++;
		}
	}

	select(current_runtime);
}

void OpenXRSelectRuntime::_on_item_selected(int p_which) {
	OS *os = OS::get_singleton();

	if (p_which == 0) {
		// Return to default runtime
		os->set_environment("XR_RUNTIME_JSON", "");
	} else {
		// Select the runtime we want
		String runtime_path = get_item_metadata(p_which);
		os->set_environment("XR_RUNTIME_JSON", runtime_path);
	}
}

void OpenXRSelectRuntime::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_ENTER_TREE: {
			// Update dropdown
			_update_items();

			// Connect signal
			connect(SceneStringName(item_selected), callable_mp(this, &OpenXRSelectRuntime::_on_item_selected));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			// Disconnect signal
			disconnect(SceneStringName(item_selected), callable_mp(this, &OpenXRSelectRuntime::_on_item_selected));
		} break;
	}
}

OpenXRSelectRuntime::OpenXRSelectRuntime() {
	Dictionary default_runtimes;

	// Add known common runtimes by default.
#ifdef WINDOWS_ENABLED
	default_runtimes["Meta"] = "C:\\Program Files\\Oculus\\Support\\oculus-runtime\\oculus_openxr_64.json";
	default_runtimes["SteamVR"] = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\SteamVR\\steamxr_win64.json";
	default_runtimes["Varjo"] = "C:\\Program Files\\Varjo\\varjo-openxr\\VarjoOpenXR.json";
	default_runtimes["WMR"] = "C:\\WINDOWS\\system32\\MixedRealityRuntime.json";
#endif
#ifdef LINUXBSD_ENABLED
	default_runtimes["Monado"] = "/usr/share/openxr/1/openxr_monado.json";
	default_runtimes["SteamVR"] = "~/.steam/steam/steamapps/common/SteamVR/steamxr_linux64.json";
#endif

	// TODO: Move to editor_settings.cpp
	EDITOR_DEF_RST("xr/openxr/runtime_paths", default_runtimes);

	set_flat(true);
	set_theme_type_variation("TopBarOptionButton");
	set_fit_to_longest_item(false);
	set_focus_mode(Control::FOCUS_NONE);
	set_tooltip_text(TTR("Choose an XR runtime."));
}
