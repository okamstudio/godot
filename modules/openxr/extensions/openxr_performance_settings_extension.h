/**************************************************************************/
/*  openxr_performance_settings_extension.h                               */
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

#ifndef OPENXR_PERFORMANCE_SETTINGS_EXTENSION_H
#define OPENXR_PERFORMANCE_SETTINGS_EXTENSION_H

#include "../openxr_interface.h"
#include "../util.h"
#include "openxr_extension_wrapper.h"

class OpenXRPerformanceSettingsExtension : public OpenXRExtensionWrapper {
public:
	static OpenXRPerformanceSettingsExtension *get_singleton();

	OpenXRPerformanceSettingsExtension();
	virtual ~OpenXRPerformanceSettingsExtension() override;

	virtual HashMap<String, bool *> get_requested_extensions() override;

	virtual void on_instance_created(const XrInstance p_instance) override;
	virtual bool on_event_polled(const XrEventDataBuffer &event) override;

	bool is_available();

	void set_cpu_level(OpenXRInterface::PerfSettingsLevel p_level);
	void set_gpu_level(OpenXRInterface::PerfSettingsLevel p_level);

private:
	static OpenXRPerformanceSettingsExtension *singleton;

	bool available = false;

	XrPerfSettingsLevelEXT level_to_openxr(OpenXRInterface::PerfSettingsLevel p_level);
	OpenXRInterface::PerfSettingsSubDomain openxr_to_sub_domain(XrPerfSettingsSubDomainEXT p_sub_domain);
	OpenXRInterface::PerfSettingsNotificationLevel openxr_to_notification_level(XrPerfSettingsNotificationLevelEXT p_notification_level);

	EXT_PROTO_XRRESULT_FUNC3(xrPerfSettingsSetPerformanceLevelEXT, (XrSession), session, (XrPerfSettingsDomainEXT), domain, (XrPerfSettingsLevelEXT), level)
};

#endif // OPENXR_PERFORMANCE_SETTINGS_EXTENSION_H
