/**************************************************************************/
/*  openxr_performance_settings_extension.cpp                             */
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

#include "openxr_performance_settings_extension.h"

#include "../openxr_api.h"

OpenXRPerformanceSettingsExtension *OpenXRPerformanceSettingsExtension::singleton = nullptr;

OpenXRPerformanceSettingsExtension *OpenXRPerformanceSettingsExtension::get_singleton() {
	return singleton;
}

OpenXRPerformanceSettingsExtension::OpenXRPerformanceSettingsExtension() {
	singleton = this;
}

OpenXRPerformanceSettingsExtension::~OpenXRPerformanceSettingsExtension() {
	singleton = nullptr;
}

HashMap<String, bool *> OpenXRPerformanceSettingsExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	request_extensions[XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME] = &available;

	return request_extensions;
}

void OpenXRPerformanceSettingsExtension::on_instance_created(const XrInstance p_instance) {
	if (available) {
		EXT_INIT_XR_FUNC(xrPerfSettingsSetPerformanceLevelEXT);
	}
}

bool OpenXRPerformanceSettingsExtension::on_event_polled(const XrEventDataBuffer &event) {
	switch (event.type) {
		case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
			const XrEventDataPerfSettingsEXT *perf_settings_event = (XrEventDataPerfSettingsEXT *)&event;

			OpenXRInterface::PerfSettingsSubDomain sub_domain = openxr_to_sub_domain(perf_settings_event->subDomain);
			OpenXRInterface::PerfSettingsNotificationLevel from_level = openxr_to_notification_level(perf_settings_event->fromLevel);
			OpenXRInterface::PerfSettingsNotificationLevel to_level = openxr_to_notification_level(perf_settings_event->toLevel);

			OpenXRInterface *openxr_interface = OpenXRAPI::get_singleton()->get_xr_interface();

			if (perf_settings_event->domain == XR_PERF_SETTINGS_DOMAIN_CPU_EXT) {
				openxr_interface->on_cpu_level_changed(sub_domain, from_level, to_level);
			} else if (perf_settings_event->domain == XR_PERF_SETTINGS_DOMAIN_GPU_EXT) {
				openxr_interface->on_gpu_level_changed(sub_domain, from_level, to_level);
			} else {
				print_error(vformat("OpenXR: no matching performance settings domain for %s", perf_settings_event->domain));
			}
			return true;
		} break;
		default:
			return false;
	}
}

bool OpenXRPerformanceSettingsExtension::is_available() {
	return available;
}

void OpenXRPerformanceSettingsExtension::set_cpu_level(OpenXRInterface::PerfSettingsLevel p_level) {
	XrSession session = OpenXRAPI::get_singleton()->get_session();
	XrPerfSettingsLevelEXT xr_level = level_to_openxr(p_level);

	XrResult result = xrPerfSettingsSetPerformanceLevelEXT(session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, xr_level);
	if (XR_FAILED(result)) {
		print_error(vformat("OpenXR: failed to set CPU performance level [%s]", OpenXRAPI::get_singleton()->get_error_string(result)));
	}
}

void OpenXRPerformanceSettingsExtension::set_gpu_level(OpenXRInterface::PerfSettingsLevel p_level) {
	XrSession session = OpenXRAPI::get_singleton()->get_session();
	XrPerfSettingsLevelEXT xr_level = level_to_openxr(p_level);

	XrResult result = xrPerfSettingsSetPerformanceLevelEXT(session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, xr_level);
	if (XR_FAILED(result)) {
		print_error(vformat("OpenXR: failed to set GPU performance level [%s]", OpenXRAPI::get_singleton()->get_error_string(result)));
	}
}

XrPerfSettingsLevelEXT OpenXRPerformanceSettingsExtension::level_to_openxr(OpenXRInterface::PerfSettingsLevel p_level) {
	switch (p_level) {
		case OpenXRInterface::PerfSettingsLevel::PERF_SETTINGS_LEVEL_POWER_SAVINGS:
			return XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT;
			break;
		case OpenXRInterface::PerfSettingsLevel::PERF_SETTINGS_LEVEL_SUSTAINED_LOW:
			return XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT;
			break;
		case OpenXRInterface::PerfSettingsLevel::PERF_SETTINGS_LEVEL_SUSTAINED_HIGH:
			return XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
		case OpenXRInterface::PerfSettingsLevel::PERF_SETTINGS_LEVEL_BOOST:
			return XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
		default:
			print_error("Invalid performance settings level.");
			return XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
	}
}

OpenXRInterface::PerfSettingsSubDomain OpenXRPerformanceSettingsExtension::openxr_to_sub_domain(XrPerfSettingsSubDomainEXT p_sub_domain) {
	switch (p_sub_domain) {
		case XR_PERF_SETTINGS_SUB_DOMAIN_COMPOSITING_EXT:
			return OpenXRInterface::PerfSettingsSubDomain::PERF_SETTINGS_SUB_DOMAIN_COMPOSITING;
		case XR_PERF_SETTINGS_SUB_DOMAIN_RENDERING_EXT:
			return OpenXRInterface::PerfSettingsSubDomain::PERF_SETTINGS_SUB_DOMAIN_RENDERING;
		case XR_PERF_SETTINGS_SUB_DOMAIN_THERMAL_EXT:
			return OpenXRInterface::PerfSettingsSubDomain::PERF_SETTINGS_SUB_DOMAIN_THERMAL;
		default:
			print_error("Invalid performance settings sub domain.");
			return OpenXRInterface::PerfSettingsSubDomain::PERF_SETTINGS_SUB_DOMAIN_COMPOSITING;
	}
}

OpenXRInterface::PerfSettingsNotificationLevel OpenXRPerformanceSettingsExtension::openxr_to_notification_level(XrPerfSettingsNotificationLevelEXT p_notification_level) {
	switch (p_notification_level) {
		case XR_PERF_SETTINGS_NOTIF_LEVEL_NORMAL_EXT:
			return OpenXRInterface::PerfSettingsNotificationLevel::PERF_SETTINGS_NOTIF_LEVEL_NORMAL;
		case XR_PERF_SETTINGS_NOTIF_LEVEL_WARNING_EXT:
			return OpenXRInterface::PerfSettingsNotificationLevel::PERF_SETTINGS_NOTIF_LEVEL_WARNING;
		case XR_PERF_SETTINGS_NOTIF_LEVEL_IMPAIRED_EXT:
			return OpenXRInterface::PerfSettingsNotificationLevel::PERF_SETTINGS_NOTIF_LEVEL_IMPAIRED;
		default:
			print_error("Invalid performance settings notification level.");
			return OpenXRInterface::PerfSettingsNotificationLevel::PERF_SETTINGS_NOTIF_LEVEL_NORMAL;
	}
}
