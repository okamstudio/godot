/**************************************************************************/
/*  engine_update_label.h                                                 */
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

#include "scene/gui/link_button.h"

class HTTPRequest;

class EngineUpdateLabel : public LinkButton {
	GDCLASS(EngineUpdateLabel, LinkButton);

public:
	enum class UpdateMode {
		DISABLED,
		NEWEST_UNSTABLE,
		NEWEST_STABLE,
		NEWEST_PATCH,
	};

private:
	static constexpr int DEV_VERSION = 9999; // Version index for unnumbered builds (assumed to always be newest).

	enum class VersionType {
		STABLE,
		RC,
		BETA,
		ALPHA,
		DEV,
		UNKNOWN,
	};

	enum class UpdateStatus {
		NONE,
		OFFLINE,
		BUSY,
		ERROR,
		UPDATE_AVAILABLE,
		UP_TO_DATE,
	};

	struct ThemeCache {
		Color default_color;
		Color disabled_color;
		Color error_color;
		Color update_color;
	} theme_cache;

	HTTPRequest *http = nullptr;

	UpdateStatus status = UpdateStatus::NONE;
	bool checked_update = false;
	String available_newer_version;

	bool _can_check_updates() const;
	void _check_update();
	void _http_request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

	void _set_message(const String &p_message, const Color &p_color);
	void _set_status(UpdateStatus p_status);

	VersionType _get_version_type(const String &p_string, int *r_index) const;
	String _extract_sub_string(const String &p_line) const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

	virtual void pressed() override;

public:
	EngineUpdateLabel();
};
