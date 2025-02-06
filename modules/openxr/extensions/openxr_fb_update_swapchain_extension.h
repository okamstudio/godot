/**************************************************************************/
/*  openxr_fb_update_swapchain_extension.h                                */
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

// This extension implements the FB update swapchain extension.
// This is an extension Meta added to further configure the swapchain.
// Other Android based devices are implementing this as well, see:
// https://github.khronos.org/OpenXR-Inventory/extension_support.html#XR_FB_swapchain_update_state

#include "../openxr_api.h"
#include "../util.h"
#include "openxr_extension_wrapper.h"

class OpenXRFBUpdateSwapchainExtension : public OpenXRExtensionWrapper {
	friend class OpenXRFBFoveationExtension;

public:
	static OpenXRFBUpdateSwapchainExtension *get_singleton();

	OpenXRFBUpdateSwapchainExtension(const String &p_rendering_driver);
	virtual ~OpenXRFBUpdateSwapchainExtension() override;

	virtual HashMap<String, bool *> get_requested_extensions() override;

	virtual void on_instance_created(const XrInstance p_instance) override;
	virtual void on_instance_destroyed() override;

	bool is_enabled() const;

private:
	static OpenXRFBUpdateSwapchainExtension *singleton;

	// Setup
	String rendering_driver;
	bool fb_swapchain_update_state_ext = false;
	bool fb_swapchain_update_state_vulkan_ext = false;
	bool fb_swapchain_update_state_opengles_ext = false;

	// OpenXR API call wrappers
	EXT_PROTO_XRRESULT_FUNC2(xrUpdateSwapchainFB, (XrSwapchain), swapchain, (const XrSwapchainStateBaseHeaderFB *), state);
	EXT_PROTO_XRRESULT_FUNC2(xrGetSwapchainStateFB, (XrSwapchain), swapchain, (XrSwapchainStateBaseHeaderFB *), state);
};
