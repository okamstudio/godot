/**************************************************************************/
/*  openxr_api_extension.h                                                */
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

#ifndef OPENXR_API_EXTENSION_H
#define OPENXR_API_EXTENSION_H

#include "openxr_api.h"

#include "core/object/ref_counted.h"
#include "core/os/thread_safe.h"
#include "core/variant/native_ptr.h"

class OpenXRExtensionWrapperExtension;

class OpenXRAPIExtension : public RefCounted {
	GDCLASS(OpenXRAPIExtension, RefCounted);

protected:
	_THREAD_SAFE_CLASS_

	static void _bind_methods();

public:
	uint64_t get_instance();
	uint64_t get_system_id();
	uint64_t get_session();

	// Helper method to convert an XrPosef to a Transform3D.
	Transform3D transform_from_pose(GDExtensionConstPtr<const void> p_pose);

	bool xr_result(uint64_t result, String format, Array args = Array());

	static bool openxr_is_enabled(bool p_check_run_in_editor = true);

	//TODO workaround as GDExtensionPtr<void> return type results in build error in godot-cpp
	uint64_t get_instance_proc_addr(String p_name);
	String get_error_string(uint64_t result);
	String get_swapchain_format_name(int64_t p_swapchain_format);
	void set_object_name(int64_t p_object_type, uint64_t p_object_handle, const String &p_object_name);
	void begin_debug_label_region(const String &p_label_name);
	void end_debug_label_region();
	void insert_debug_label(const String &p_label_name);

	bool is_initialized();
	bool is_running();

	uint64_t get_play_space();
	int64_t get_predicted_display_time();
	int64_t get_next_frame_time();
	bool can_render();

	uint64_t get_hand_tracker(int p_hand_index);

	void register_composition_layer_provider(OpenXRExtensionWrapperExtension *p_extension);
	void unregister_composition_layer_provider(OpenXRExtensionWrapperExtension *p_extension);

	void register_projection_views_extension(OpenXRExtensionWrapperExtension *p_extension);
	void unregister_projection_views_extension(OpenXRExtensionWrapperExtension *p_extension);

	double get_render_state_z_near();
	double get_render_state_z_far();

	void set_velocity_texture(RID p_render_target);
	void set_velocity_depth_texture(RID p_render_target);
	void set_velocity_target_size(const Size2i &p_target_size);

	PackedInt64Array get_supported_swapchain_formats();

	uint64_t openxr_swapchain_create(XrSwapchainCreateFlags p_create_flags, XrSwapchainUsageFlags p_usage_flags, int64_t p_swapchain_format, uint32_t p_width, uint32_t p_height, uint32_t p_sample_count, uint32_t p_array_size);
	void openxr_swapchain_free(uint64_t p_swapchain_info);
	uint64_t openxr_swapchain_get_swapchain(uint64_t p_swapchain_info);
	void openxr_swapchain_acquire(uint64_t p_swapchain_info);
	RID openxr_swapchain_get_image(uint64_t p_swapchain_info);
	void openxr_swapchain_release(uint64_t p_swapchain_info);

	uint64_t get_projection_layer();

	void set_render_region(const Rect2i &p_render_region);

	enum OpenXRAlphaBlendModeSupport {
		OPENXR_ALPHA_BLEND_MODE_SUPPORT_NONE = 0,
		OPENXR_ALPHA_BLEND_MODE_SUPPORT_REAL = 1,
		OPENXR_ALPHA_BLEND_MODE_SUPPORT_EMULATING = 2,
	};

	void set_emulate_environment_blend_mode_alpha_blend(bool p_enabled);
	OpenXRAlphaBlendModeSupport is_environment_blend_mode_alpha_blend_supported();

	OpenXRAPIExtension();
};

VARIANT_ENUM_CAST(OpenXRAPIExtension::OpenXRAlphaBlendModeSupport);

#endif // OPENXR_API_EXTENSION_H
