/**************************************************************************/
/*  engine_update_label.cpp                                               */
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

#include "engine_update_label.h"

#include "core/io/json.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "scene/main/http_request.h"

bool EngineUpdateLabel::_can_check_updates() const {
	return int(EDITOR_GET("network/connection/network_mode")) == EditorSettings::NETWORK_ONLINE &&
			UpdateMode(int(EDITOR_GET("network/connection/engine_version_update_mode"))) != UpdateMode::DISABLED;
}

void EngineUpdateLabel::_check_update() {
	checked_update = true;
	_set_status(UpdateStatus::BUSY);
	http->request("https://godotengine.org/versions.json");
}

void EngineUpdateLabel::_http_request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	if (p_result != OK) {
		_set_status(UpdateStatus::ERROR);
		_set_message(vformat(TTR("Failed to check for updates. Error: %d."), p_result), theme_cache.error_color);
		return;
	}

	if (p_response_code != 200) {
		_set_status(UpdateStatus::ERROR);
		_set_message(vformat(TTR("Failed to check for updates. Response code: %d."), p_response_code), theme_cache.error_color);
		return;
	}

	Array version_array;
	{
		String s;
		const uint8_t *r = p_body.ptr();
		s.parse_utf8((const char *)r, p_body.size());

		Variant result = JSON::parse_string(s);
		if (result == Variant()) {
			_set_status(UpdateStatus::ERROR);
			_set_message(TTR("Failed to parse version JSON."), theme_cache.error_color);
			return;
		}
		if (result.get_type() != Variant::ARRAY) {
			_set_status(UpdateStatus::ERROR);
			_set_message(TTR("Received JSON data is not a valid version array."), theme_cache.error_color);
			return;
		}
		version_array = result;
	}

	UpdateMode update_mode = UpdateMode(int(EDITOR_GET("network/connection/engine_version_update_mode")));
	bool stable_only = update_mode == UpdateMode::NEWEST_STABLE || update_mode == UpdateMode::NEWEST_PATCH;

	const Dictionary current_version_info = Engine::get_singleton()->get_version_info();
	int current_major = current_version_info.get("major", 0);
	int current_minor = current_version_info.get("minor", 0);
	int current_patch = current_version_info.get("patch", 0);

	for (const Variant &data_bit : version_array) {
		const Dictionary version_info = data_bit;

		const String base_version_string = version_info.get("name", "");
		const PackedStringArray version_bits = base_version_string.split(".");

		if (version_bits.size() < 2) {
			continue;
		}

		int minor = version_bits[1].to_int();
		if (version_bits[0].to_int() != current_major || minor < current_minor) {
			continue;
		}

		int patch = 0;
		if (version_bits.size() >= 3) {
			patch = version_bits[2].to_int();
		}

		if (minor == current_minor && patch < current_patch) {
			continue;
		}

		if (update_mode == UpdateMode::NEWEST_PATCH && minor > current_minor) {
			continue;
		}

		const Array releases = version_info.get("releases", Array());
		if (releases.is_empty()) {
			continue;
		}

		const Dictionary newest_release = releases[0];
		const String release_string = newest_release.get("name", "unknown");

		int release_index;
		VersionType release_type = _get_version_type(release_string, &release_index);

		if (minor > current_minor || patch > current_patch) {
			if (stable_only && release_type != VersionType::STABLE) {
				continue;
			}

			available_newer_version = vformat("%s-%s", base_version_string, release_string);
			break;
		}

		int current_version_index;
		VersionType current_version_type = _get_version_type(current_version_info.get("status", "unknown"), &current_version_index);

		if (int(release_type) > int(current_version_type)) {
			break;
		}

		if (int(release_type) == int(current_version_type) && release_index <= current_version_index) {
			break;
		}

		available_newer_version = vformat("%s-%s", base_version_string, release_string);
		break;
	}

	if (!available_newer_version.is_empty()) {
		_set_status(UpdateStatus::UPDATE_AVAILABLE);
		_set_message(vformat(TTR("Update available: %s."), available_newer_version), theme_cache.update_color);
	} else if (available_newer_version.is_empty()) {
		_set_status(UpdateStatus::UP_TO_DATE);
	}
}

void EngineUpdateLabel::_set_message(const String &p_message, const Color &p_color) {
	if (is_disabled()) {
		add_theme_color_override("font_disabled_color", p_color);
	} else {
		add_theme_color_override(SceneStringName(font_color), p_color);
	}
	set_text(p_message);
}

void EngineUpdateLabel::_set_status(UpdateStatus p_status) {
	status = p_status;
	if (status == UpdateStatus::BUSY || status == UpdateStatus::UP_TO_DATE) {
		// Hide the label to prevent unnecessary distraction.
		hide();
		return;
	} else {
		show();
	}

	switch (status) {
		case UpdateStatus::OFFLINE: {
			set_disabled(false);
			if (int(EDITOR_GET("network/connection/network_mode")) == EditorSettings::NETWORK_OFFLINE) {
				_set_message(TTR("Offline mode, update checks disabled."), theme_cache.disabled_color);
			} else {
				_set_message(TTR("Update checks disabled."), theme_cache.disabled_color);
			}
			set_tooltip_text("");
			break;
		}

		case UpdateStatus::ERROR: {
			set_disabled(false);
			set_tooltip_text(TTR("An error has occurred. Click to try again."));
		} break;

		case UpdateStatus::UPDATE_AVAILABLE: {
			set_disabled(false);
			set_tooltip_text(TTR("Click to open download page."));
		} break;

		default: {
		}
	}
}

EngineUpdateLabel::VersionType EngineUpdateLabel::_get_version_type(const String &p_string, int *r_index) const {
	VersionType type = VersionType::UNKNOWN;
	String index_string;

	static HashMap<String, VersionType> type_map;
	if (type_map.is_empty()) {
		type_map["stable"] = VersionType::STABLE;
		type_map["rc"] = VersionType::RC;
		type_map["beta"] = VersionType::BETA;
		type_map["alpha"] = VersionType::ALPHA;
		type_map["dev"] = VersionType::DEV;
	}

	for (const KeyValue<String, VersionType> &kv : type_map) {
		if (p_string.begins_with(kv.key)) {
			index_string = p_string.trim_prefix(kv.key);
			type = kv.value;
			break;
		}
	}

	if (r_index) {
		if (index_string.is_empty()) {
			*r_index = DEV_VERSION;
		} else {
			*r_index = index_string.to_int();
		}
	}
	return type;
}

String EngineUpdateLabel::_extract_sub_string(const String &p_line) const {
	int j = p_line.find_char('"') + 1;
	return p_line.substr(j, p_line.find_char('"', j) - j);
}

void EngineUpdateLabel::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorSettings::get_singleton()->check_changed_settings_in_group("network/connection")) {
				break;
			}

			if (_can_check_updates()) {
				if (!checked_update) {
					_check_update();
				} else {
					// This will be wrong when user toggles online mode twice when update is available, but it's not worth handling.
					_set_status(UpdateStatus::UP_TO_DATE);
				}
			} else {
				_set_status(UpdateStatus::OFFLINE);
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			theme_cache.default_color = get_theme_color(SceneStringName(font_color), "Button");
			theme_cache.disabled_color = get_theme_color("font_disabled_color", "Button");
			theme_cache.error_color = get_theme_color("error_color", EditorStringName(Editor));
			theme_cache.update_color = get_theme_color("warning_color", EditorStringName(Editor));
		} break;

		case NOTIFICATION_READY: {
			if (_can_check_updates()) {
				_check_update();
			} else {
				_set_status(UpdateStatus::OFFLINE);
			}
		} break;
	}
}

void EngineUpdateLabel::_bind_methods() {
	ADD_SIGNAL(MethodInfo("offline_clicked"));
}

void EngineUpdateLabel::pressed() {
	switch (status) {
		case UpdateStatus::OFFLINE: {
			emit_signal("offline_clicked");
		} break;

		case UpdateStatus::ERROR: {
			_check_update();
		} break;

		case UpdateStatus::UPDATE_AVAILABLE: {
			OS::get_singleton()->shell_open("https://godotengine.org/download/archive/" + available_newer_version);
		} break;

		default: {
		}
	}
}

EngineUpdateLabel::EngineUpdateLabel() {
	set_underline_mode(UNDERLINE_MODE_ON_HOVER);

	http = memnew(HTTPRequest);
	http->set_https_proxy(EDITOR_GET("network/http_proxy/host"), EDITOR_GET("network/http_proxy/port"));
	http->set_timeout(10.0);
	add_child(http);
	http->connect("request_completed", callable_mp(this, &EngineUpdateLabel::_http_request_completed));
}
