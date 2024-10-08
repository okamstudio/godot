/**************************************************************************/
/*  editor_version_button.cpp                                             */
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

#include "editor_version_button.h"

#include "core/os/time.h"
#include "core/version.h"

String _get_version_string(EditorVersionButton::VersionFormat p_format) {
	String main;
	switch (p_format) {
		case EditorVersionButton::FORMAT_BASIC: {
			return VERSION_FULL_CONFIG;
		} break;
		case EditorVersionButton::FORMAT_WITH_BUILD: {
			main = "v" VERSION_FULL_BUILD;
		} break;
		case EditorVersionButton::FORMAT_WITH_NAME_AND_BUILD: {
			main = VERSION_FULL_NAME;
		} break;
		default: {
			ERR_FAIL_V_MSG(VERSION_FULL_NAME, "Unexpected format: " + itos(p_format));
		} break;
	}

	String hash = VERSION_HASH;
	if (!hash.is_empty()) {
		hash = vformat(" [%s]", hash.left(9));
	}
	return main + hash;
}

void EditorVersionButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			// This can't be done in the constructor because theme cache is not ready yet.
			set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
			set_text(_get_version_string(format));
		} break;
	}
}

void EditorVersionButton::pressed() {
	DisplayServer::get_singleton()->clipboard_set(_get_version_string(FORMAT_WITH_BUILD));
}

EditorVersionButton::EditorVersionButton(VersionFormat p_format) {
	format = p_format;
	set_underline_mode(LinkButton::UNDERLINE_MODE_ON_HOVER);

	String build_date;
	if (VERSION_TIMESTAMP > 0) {
		build_date = Time::get_singleton()->get_datetime_string_from_unix_time(VERSION_TIMESTAMP, true) + " UTC";
	} else {
		build_date = TTR("(unknown)");
	}
	set_tooltip_text(vformat(TTR("Git commit date: %s\nClick to copy the version information."), build_date));
}
