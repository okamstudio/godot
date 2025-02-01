/**************************************************************************/
/*  openxr_extension_wrapper.h                                            */
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

#include "core/error/error_macros.h"
#include "core/math/projection.h"
#include "core/templates/hash_map.h"
#include "core/templates/rid.h"
#include "core/variant/variant.h"

#include <openxr/openxr.h>

class OpenXRAPI;
class OpenXRActionMap;

// `OpenXRExtensionWrapper` allows us to implement OpenXR extensions.
class OpenXRExtensionWrapper {
public:
	// `get_requested_extensions` should return a list of OpenXR extensions related to this extension.
	// If the bool * is a nullptr this extension is mandatory
	// If the bool * points to a boolean, the boolean will be updated
	// to true if the extension is enabled.
	virtual HashMap<String, bool *> get_requested_extensions() = 0;

	// These functions allow an extension to add entries to a struct chain.
	// `p_next_pointer` points to the last struct that was created for this chain
	// and should be used as the value for the `pNext` pointer in the first struct you add.
	// You should return the pointer to the last struct you define as your result.
	// If you are not adding any structs, just return `p_next_pointer`.
	// See existing extensions for examples of this implementation.
	virtual void *set_system_properties_and_get_next_pointer(void *p_next_pointer) { return p_next_pointer; } // Add additional data structures when we interrogate OpenXRS system abilities.
	virtual void *set_instance_create_info_and_get_next_pointer(void *p_next_pointer) { return p_next_pointer; } // Add additional data structures when we create our OpenXR instance.
	virtual void *set_session_create_and_get_next_pointer(void *p_next_pointer) { return p_next_pointer; } // Add additional data structures when we create our OpenXR session.
	virtual void *set_swapchain_create_info_and_get_next_pointer(void *p_next_pointer) { return p_next_pointer; } // Add additional data structures when creating OpenXR swap chains.
	virtual void *set_hand_joint_locations_and_get_next_pointer(int p_hand_index, void *p_next_pointer) { return p_next_pointer; }
	virtual void *set_projection_views_and_get_next_pointer(int p_view_index, void *p_next_pointer) { return p_next_pointer; }

	virtual PackedStringArray get_suggested_tracker_names() { return PackedStringArray(); }

	// `on_register_metadata` allows extensions to register additional controller metadata.
	// This function is called even when OpenXRApi is not constructured as the metadata
	// needs to be available to the editor.
	// Also extensions should provide metadata regardless of whether they are supported
	// on the host system as the controller data is used to setup action maps for users
	// who may have access to the relevant hardware.
	virtual void on_register_metadata() {}

	virtual void on_before_instance_created() {} // `on_before_instance_created` is called before we create our OpenXR instance.
	virtual void on_instance_created(const XrInstance p_instance) {} // `on_instance_created` is called right after we've successfully created our OpenXR instance.
	virtual void on_instance_destroyed() {} // `on_instance_destroyed` is called right before we destroy our OpenXR instance.
	virtual void on_session_created(const XrSession p_session) {} // `on_session_created` is called right after we've successfully created our OpenXR session.
	virtual void on_session_destroyed() {} // `on_session_destroyed` is called right before we destroy our OpenXR session.

	// `on_process` is called as part of our OpenXR process handling,
	// this happens right before physics process and normal processing is run.
	// This is when controller data is queried and made available to game logic.
	virtual void on_process() {}
	virtual void on_pre_render() {} // `on_pre_render` is called right before we start rendering our XR viewports.
	virtual void on_main_swapchains_created() {} // `on_main_swapchains_created` is called right after our main swapchains are (re)created.
	virtual void on_pre_draw_viewport(RID p_render_target) {} // `on_pre_draw_viewport` is called right before we start rendering this viewport
	virtual void on_post_draw_viewport(RID p_render_target) {} // `on_port_draw_viewport` is called right after we start rendering this viewport (note that on Vulkan draw commands may only be queued)

	virtual void on_state_idle() {} // `on_state_idle` is called when the OpenXR session state is changed to idle.
	virtual void on_state_ready() {} // `on_state_ready` is called when the OpenXR session state is changed to ready, this means OpenXR is ready to setup our session.
	virtual void on_state_synchronized() {} // `on_state_synchronized` is called when the OpenXR session state is changed to synchronized, note that OpenXR also returns to this state when our application looses focus.
	virtual void on_state_visible() {} // `on_state_visible` is called when the OpenXR session state is changed to visible, OpenXR is now ready to receive frames.
	virtual void on_state_focused() {} // `on_state_focused` is called when the OpenXR session state is changed to focused, this state is the active state when our game runs.
	virtual void on_state_stopping() {} // `on_state_stopping` is called when the OpenXR session state is changed to stopping.
	virtual void on_state_loss_pending() {} // `on_state_loss_pending` is called when the OpenXR session state is changed to loss pending.
	virtual void on_state_exiting() {} // `on_state_exiting` is called when the OpenXR session state is changed to exiting.

	virtual void *set_viewport_composition_layer_and_get_next_pointer(const XrCompositionLayerBaseHeader *p_layer, const Dictionary &p_property_values, void *p_next_pointer) { return p_next_pointer; } // Add additional data structures to composition layers created via OpenXRCompositionLayer.
	virtual void on_viewport_composition_layer_destroyed(const XrCompositionLayerBaseHeader *p_layer) {} // `on_viewport_composition_layer_destroyed` is called when a composition layer created via OpenXRCompositionLayer is destroyed.
	virtual void get_viewport_composition_layer_extension_properties(List<PropertyInfo> *p_property_list) {} // Get additional property definitions for OpenXRCompositionLayer.
	virtual Dictionary get_viewport_composition_layer_extension_property_defaults() { return Dictionary(); } // Get the default values for the additional property definitions for OpenXRCompositionLayer.
	virtual void *set_android_surface_swapchain_create_info_and_get_next_pointer(const Dictionary &p_property_values, void *p_next_pointer) { return p_next_pointer; }

	// `on_event_polled` is called when there is an OpenXR event to process.
	// Should return true if the event was handled, false otherwise.
	virtual bool on_event_polled(const XrEventDataBuffer &event) {
		return false;
	}

	OpenXRExtensionWrapper() = default;
	virtual ~OpenXRExtensionWrapper() = default;
};

// `OpenXRGraphicsExtensionWrapper` implements specific logic for each supported graphics API.
class OpenXRGraphicsExtensionWrapper : public OpenXRExtensionWrapper {
public:
	virtual void get_usable_swapchain_formats(Vector<int64_t> &p_usable_swap_chains) = 0; // `get_usable_swapchain_formats` should return a list of usable color formats.
	virtual void get_usable_depth_formats(Vector<int64_t> &p_usable_swap_chains) = 0; // `get_usable_depth_formats` should return a list of usable depth formats.
	virtual String get_swapchain_format_name(int64_t p_swapchain_format) const = 0; // `get_swapchain_format_name` should return the constant name of a given format.
	virtual bool get_swapchain_image_data(XrSwapchain p_swapchain, int64_t p_swapchain_format, uint32_t p_width, uint32_t p_height, uint32_t p_sample_count, uint32_t p_array_size, void **r_swapchain_graphics_data) = 0; // `get_swapchain_image_data` extracts image IDs for the swapchain images and stores there in an implementation dependent data structure.
	virtual void cleanup_swapchain_graphics_data(void **p_swapchain_graphics_data) = 0; // `cleanup_swapchain_graphics_data` cleans up the data held in our implementation dependent data structure and should free up its memory.
	virtual bool create_projection_fov(const XrFovf p_fov, double p_z_near, double p_z_far, Projection &r_camera_matrix) = 0; // `create_projection_fov` creates a proper projection matrix based on asymmetric FOV data provided by OpenXR.
	virtual RID get_texture(void *p_swapchain_graphics_data, int p_image_index) = 0; // `get_texture` returns a Godot texture RID for the current active texture in our swapchain.
};
