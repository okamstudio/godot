/**************************************************************************/
/*  webxr_interface_js.h                                                  */
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

#ifdef WEB_ENABLED

#include "servers/xr/xr_controller_tracker.h"
#include "servers/xr/xr_hand_tracker.h"
#include "webxr_interface.h"

/**
	The WebXR interface is a VR/AR interface that can be used on the web.
*/

namespace GLES3 {
class TextureStorage;
}

class WebXRInterfaceJS : public WebXRInterface {
	GDCLASS(WebXRInterfaceJS, WebXRInterface);

private:
	bool initialized;
	Ref<XRPositionalTracker> head_tracker;
	Transform3D head_transform;

	String session_mode;
	String required_features;
	String optional_features;
	String requested_reference_space_types;
	String reference_space_type;
	String enabled_features;

	XRInterface::EnvironmentBlendMode environment_blend_mode = XRInterface::XR_ENV_BLEND_MODE_OPAQUE;

	Size2 render_targetsize;
	RBMap<unsigned int, RID> texture_cache;
	struct Touch {
		bool is_touching = false;
		Vector2 position;
	} touches[5];

	static constexpr uint8_t input_source_count = 16;

	struct InputSource {
		Ref<XRControllerTracker> tracker;
		bool active = false;
		TargetRayMode target_ray_mode;
		int touch_index = -1;
	} input_sources[input_source_count];

	static const int WEBXR_HAND_JOINT_MAX = 25;
	static const int HAND_MAX = 2;
	Ref<XRHandTracker> hand_trackers[HAND_MAX];

	RID color_texture;
	RID depth_texture;

	RID _get_color_texture();
	RID _get_depth_texture();
	RID _get_texture(unsigned int p_texture_id);
	Transform3D _js_matrix_to_transform(float *p_js_matrix);
	void _update_input_source(int p_input_source_id);

	Vector2 _get_screen_position_from_joy_vector(const Vector2 &p_joy_vector);

public:
	virtual void is_session_supported(const String &p_session_mode) override;
	virtual void set_session_mode(String p_session_mode) override;
	virtual String get_session_mode() const override;
	virtual void set_required_features(String p_required_features) override;
	virtual String get_required_features() const override;
	virtual void set_optional_features(String p_optional_features) override;
	virtual String get_optional_features() const override;
	virtual void set_requested_reference_space_types(String p_requested_reference_space_types) override;
	virtual String get_requested_reference_space_types() const override;
	virtual String get_reference_space_type() const override;
	virtual String get_enabled_features() const override;
	virtual bool is_input_source_active(int p_input_source_id) const override;
	virtual Ref<XRControllerTracker> get_input_source_tracker(int p_input_source_id) const override;
	virtual TargetRayMode get_input_source_target_ray_mode(int p_input_source_id) const override;
	virtual String get_visibility_state() const override;
	virtual PackedVector3Array get_play_area() const override;

	virtual float get_display_refresh_rate() const override;
	virtual void set_display_refresh_rate(float p_refresh_rate) override;
	virtual Array get_available_display_refresh_rates() const override;

	virtual Array get_supported_environment_blend_modes() override;
	virtual XRInterface::EnvironmentBlendMode get_environment_blend_mode() const override;
	virtual bool set_environment_blend_mode(EnvironmentBlendMode p_new_environment_blend_mode) override;

	virtual StringName get_name() const override;
	virtual uint32_t get_capabilities() const override;

	virtual bool is_initialized() const override;
	virtual bool initialize() override;
	virtual void uninitialize() override;
	virtual Dictionary get_system_info() override;

	virtual Size2 get_render_target_size() override;
	virtual uint32_t get_view_count() override;
	virtual Transform3D get_camera_transform() override;
	virtual Transform3D get_transform_for_view(uint32_t p_view, const Transform3D &p_cam_transform) override;
	virtual Projection get_projection_for_view(uint32_t p_view, double p_aspect, double p_z_near, double p_z_far) override;
	virtual bool pre_draw_viewport(RID p_render_target) override;
	virtual Vector<BlitToScreen> post_draw_viewport(RID p_render_target, const Rect2 &p_screen_rect) override;
	virtual RID get_color_texture() override;
	virtual RID get_depth_texture() override;
	virtual RID get_velocity_texture() override;

	virtual void process() override;

	void _on_input_event(int p_event_type, int p_input_source_id);

	// Internal setters used by callbacks from Emscripten.
	inline void _set_reference_space_type(String p_reference_space_type) { reference_space_type = p_reference_space_type; }
	inline void _set_enabled_features(String p_enabled_features) { enabled_features = p_enabled_features; }
	void _set_environment_blend_mode(String p_blend_mode_string);

	WebXRInterfaceJS();
	~WebXRInterfaceJS();

private:
	StringName tracker_names[16] = {
		StringName("left_hand"),
		StringName("right_hand"),
		StringName("tracker_2"),
		StringName("tracker_3"),
		StringName("tracker_4"),
		StringName("tracker_5"),
		StringName("tracker_6"),
		StringName("tracker_7"),
		StringName("tracker_8"),
		StringName("tracker_9"),
		StringName("tracker_10"),
		StringName("tracker_11"),
		StringName("tracker_12"),
		StringName("tracker_13"),
		StringName("tracker_14"),
		StringName("tracker_15"),
	};

	StringName touch_names[5] = {
		StringName("touch_0"),
		StringName("touch_1"),
		StringName("touch_2"),
		StringName("touch_3"),
		StringName("touch_4"),
	};

	StringName standard_axis_names[10] = {
		StringName("touchpad_x"),
		StringName("touchpad_y"),
		StringName("thumbstick_x"),
		StringName("thumbstick_y"),
		StringName("axis_4"),
		StringName("axis_5"),
		StringName("axis_6"),
		StringName("axis_7"),
		StringName("axis_8"),
		StringName("axis_9"),
	};

	StringName standard_vector_names[2] = {
		StringName("touchpad"),
		StringName("thumbstick"),
	};

	StringName standard_button_names[10] = {
		StringName("trigger_click"),
		StringName("grip_click"),
		StringName("touchpad_click"),
		StringName("thumbstick_click"),
		StringName("ax_button"),
		StringName("by_button"),
		StringName("button_6"),
		StringName("button_7"),
		StringName("button_8"),
		StringName("button_9"),
	};

	StringName standard_button_pressure_names[10] = {
		StringName("trigger"),
		StringName("grip"),
		StringName("touchpad_click_pressure"),
		StringName("thumbstick_click_pressure"),
		StringName("ax_button_pressure"),
		StringName("by_button_pressure"),
		StringName("button_pressure_6"),
		StringName("button_pressure_7"),
		StringName("button_pressure_8"),
		StringName("button_pressure_9"),
	};

	StringName unknown_button_names[10] = {
		StringName("button_0"),
		StringName("button_1"),
		StringName("button_2"),
		StringName("button_3"),
		StringName("button_4"),
		StringName("button_5"),
		StringName("button_6"),
		StringName("button_7"),
		StringName("button_8"),
		StringName("button_9"),
	};

	StringName unknown_axis_names[10] = {
		StringName("axis_0"),
		StringName("axis_1"),
		StringName("axis_2"),
		StringName("axis_3"),
		StringName("axis_4"),
		StringName("axis_5"),
		StringName("axis_6"),
		StringName("axis_7"),
		StringName("axis_8"),
		StringName("axis_9"),
	};

	StringName unknown_button_pressure_names[10] = {
		StringName("button_pressure_0"),
		StringName("button_pressure_1"),
		StringName("button_pressure_2"),
		StringName("button_pressure_3"),
		StringName("button_pressure_4"),
		StringName("button_pressure_5"),
		StringName("button_pressure_6"),
		StringName("button_pressure_7"),
		StringName("button_pressure_8"),
		StringName("button_pressure_9"),
	};
};

#endif // WEB_ENABLED
