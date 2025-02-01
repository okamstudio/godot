/**************************************************************************/
/*  openxr_fb_display_refresh_rate_extension.h                            */
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

// This extension gives us access to the possible display refresh rates
// supported by the HMD.
// While this is an FB extension it has been adopted by most runtimes and
// will likely become core in the near future.

#include "../openxr_api.h"
#include "../util.h"
#include "openxr_extension_wrapper.h"

class OpenXRDisplayRefreshRateExtension : public OpenXRExtensionWrapper {
public:
	static OpenXRDisplayRefreshRateExtension *get_singleton();

	OpenXRDisplayRefreshRateExtension();
	virtual ~OpenXRDisplayRefreshRateExtension() override;

	virtual HashMap<String, bool *> get_requested_extensions() override;

	virtual void on_instance_created(const XrInstance p_instance) override;
	virtual void on_instance_destroyed() override;
	virtual bool on_event_polled(const XrEventDataBuffer &event) override;

	float get_refresh_rate() const;
	void set_refresh_rate(float p_refresh_rate);

	Array get_available_refresh_rates() const;

private:
	static OpenXRDisplayRefreshRateExtension *singleton;

	bool display_refresh_rate_ext = false;

	// OpenXR API call wrappers
	EXT_PROTO_XRRESULT_FUNC4(xrEnumerateDisplayRefreshRatesFB, (XrSession), session, (uint32_t), displayRefreshRateCapacityInput, (uint32_t *), displayRefreshRateCountOutput, (float *), displayRefreshRates);
	EXT_PROTO_XRRESULT_FUNC2(xrGetDisplayRefreshRateFB, (XrSession), session, (float *), display_refresh_rate);
	EXT_PROTO_XRRESULT_FUNC2(xrRequestDisplayRefreshRateFB, (XrSession), session, (float), display_refresh_rate);
};
