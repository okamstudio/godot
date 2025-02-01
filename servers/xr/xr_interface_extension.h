/**************************************************************************/
/*  xr_interface_extension.h                                              */
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

#include "servers/xr/xr_interface.h"

class XRInterfaceExtension : public XRInterface {
	GDCLASS(XRInterfaceExtension, XRInterface);

public:
private:
	bool can_add_blits = false;
	Vector<BlitToScreen> blits;

protected:
	_THREAD_SAFE_CLASS_

	static void _bind_methods();

public:
	/** general interface information **/
	virtual StringName get_name() const override;
	virtual uint32_t get_capabilities() const override;

	GDVIRTUAL0RC(StringName, _get_name);
	GDVIRTUAL0RC(uint32_t, _get_capabilities);

	virtual bool is_initialized() const override;
	virtual bool initialize() override;
	virtual void uninitialize() override;
	virtual Dictionary get_system_info() override;

	GDVIRTUAL0RC(bool, _is_initialized);
	GDVIRTUAL0R(bool, _initialize);
	GDVIRTUAL0(_uninitialize);
	GDVIRTUAL0RC(Dictionary, _get_system_info);

	/** input and output **/

	virtual PackedStringArray get_suggested_tracker_names() const override; /* return a list of likely/suggested tracker names */
	virtual PackedStringArray get_suggested_pose_names(const StringName &p_tracker_name) const override; /* return a list of likely/suggested action names for this tracker */
	virtual TrackingStatus get_tracking_status() const override;
	virtual void trigger_haptic_pulse(const String &p_action_name, const StringName &p_tracker_name, double p_frequency, double p_amplitude, double p_duration_sec, double p_delay_sec = 0) override;

	GDVIRTUAL0RC(PackedStringArray, _get_suggested_tracker_names);
	GDVIRTUAL1RC(PackedStringArray, _get_suggested_pose_names, const StringName &);
	GDVIRTUAL0RC(XRInterface::TrackingStatus, _get_tracking_status);
	GDVIRTUAL6(_trigger_haptic_pulse, const String &, const StringName &, double, double, double, double);

	/** specific to VR **/
	virtual bool supports_play_area_mode(XRInterface::PlayAreaMode p_mode) override; /* query if this interface supports this play area mode */
	virtual XRInterface::PlayAreaMode get_play_area_mode() const override; /* get the current play area mode */
	virtual bool set_play_area_mode(XRInterface::PlayAreaMode p_mode) override; /* change the play area mode, note that this should return false if the mode is not available */
	virtual PackedVector3Array get_play_area() const override; /* if available, returns an array of vectors denoting the play area the player can move around in */

	GDVIRTUAL1RC(bool, _supports_play_area_mode, XRInterface::PlayAreaMode);
	GDVIRTUAL0RC(XRInterface::PlayAreaMode, _get_play_area_mode);
	GDVIRTUAL1RC(bool, _set_play_area_mode, XRInterface::PlayAreaMode);
	GDVIRTUAL0RC(PackedVector3Array, _get_play_area);

	/** specific to AR **/
	virtual bool get_anchor_detection_is_enabled() const override;
	virtual void set_anchor_detection_is_enabled(bool p_enable) override;
	virtual int get_camera_feed_id() override;

	GDVIRTUAL0RC(bool, _get_anchor_detection_is_enabled);
	GDVIRTUAL1(_set_anchor_detection_is_enabled, bool);
	GDVIRTUAL0RC(int, _get_camera_feed_id);

	/** rendering and internal **/

	virtual Size2 get_render_target_size() override;
	virtual uint32_t get_view_count() override;
	virtual Transform3D get_camera_transform() override;
	virtual Transform3D get_transform_for_view(uint32_t p_view, const Transform3D &p_cam_transform) override;
	virtual Projection get_projection_for_view(uint32_t p_view, double p_aspect, double p_z_near, double p_z_far) override;
	virtual RID get_vrs_texture() override;
	virtual RID get_color_texture() override;
	virtual RID get_depth_texture() override;
	virtual RID get_velocity_texture() override;

	GDVIRTUAL0R(Size2, _get_render_target_size);
	GDVIRTUAL0R(uint32_t, _get_view_count);
	GDVIRTUAL0R(Transform3D, _get_camera_transform);
	GDVIRTUAL2R(Transform3D, _get_transform_for_view, uint32_t, const Transform3D &);
	GDVIRTUAL4R(PackedFloat64Array, _get_projection_for_view, uint32_t, double, double, double);
	GDVIRTUAL0R(RID, _get_vrs_texture);
	GDVIRTUAL0R(RID, _get_color_texture);
	GDVIRTUAL0R(RID, _get_depth_texture);
	GDVIRTUAL0R(RID, _get_velocity_texture);

	void add_blit(RID p_render_target, Rect2 p_src_rect, Rect2i p_dst_rect, bool p_use_layer = false, uint32_t p_layer = 0, bool p_apply_lens_distortion = false, Vector2 p_eye_center = Vector2(), double p_k1 = 0.0, double p_k2 = 0.0, double p_upscale = 1.0, double p_aspect_ratio = 1.0);

	virtual void process() override;
	virtual void pre_render() override;
	virtual bool pre_draw_viewport(RID p_render_target) override;
	virtual Vector<BlitToScreen> post_draw_viewport(RID p_render_target, const Rect2 &p_screen_rect) override;
	virtual void end_frame() override;

	GDVIRTUAL0(_process);
	GDVIRTUAL0(_pre_render);
	GDVIRTUAL1R(bool, _pre_draw_viewport, RID);
	GDVIRTUAL2(_post_draw_viewport, RID, const Rect2 &);
	GDVIRTUAL0(_end_frame);

	/* access to some internals we need */
	RID get_render_target_texture(RID p_render_target);
	// RID get_render_target_depth(RID p_render_target);
};
