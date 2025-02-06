/**************************************************************************/
/*  mobile_vr_interface.h                                                 */
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
#include "servers/xr/xr_positional_tracker.h"
#include "servers/xr/xr_vrs.h"

/**
	The mobile interface is a native VR interface that can be used on Android and iOS phones.
	It contains a basic implementation supporting 3DOF tracking if a gyroscope and accelerometer are
	present and sets up the proper projection matrices based on the values provided.

	We're planning to eventually do separate interfaces towards mobile SDKs that have far more capabilities and
	do not rely on the user providing most of these settings (though enhancing this with auto detection features
	based on the device we're running on would be cool). I'm mostly adding this as an example or base plate for
	more advanced interfaces.
*/

class MobileVRInterface : public XRInterface {
	GDCLASS(MobileVRInterface, XRInterface);

private:
	bool initialized = false;
	XRInterface::TrackingStatus tracking_state;
	XRPose::TrackingConfidence tracking_confidence = XRPose::XR_TRACKING_CONFIDENCE_NONE;

	// Just set some defaults for these. At some point we need to look at adding a lookup table for common device + headset combos and/or support reading cardboard QR codes
	double eye_height = 1.85;
	uint64_t last_ticks = 0;

	double intraocular_dist = 6.0;
	double display_width = 14.5;
	double display_to_lens = 4.0;
	double oversample = 1.5;

	Rect2 offset_rect = Rect2(0, 0, 1, 1); // Full screen rect.

	double k1 = 0.215;
	double k2 = 0.215;
	double aspect = 1.0;

	// at a minimum we need a tracker for our head
	Ref<XRPositionalTracker> head;
	Transform3D head_transform;

	XRVRS xr_vrs;

	/*
		logic for processing our sensor data, this was originally in our positional tracker logic but I think
		that doesn't make sense in hindsight. It only makes marginally more sense to park it here for now,
		this probably deserves an object of its own
	*/
	Vector3 scale_magneto(const Vector3 &p_magnetometer);
	Basis combine_acc_mag(const Vector3 &p_grav, const Vector3 &p_magneto);

	int mag_count = 0;
	bool has_gyro = false;
	bool sensor_first = false;
	Vector3 last_accerometer_data;
	Vector3 last_magnetometer_data;
	Vector3 mag_current_min;
	Vector3 mag_current_max;
	Vector3 mag_next_min;
	Vector3 mag_next_max;

	///@TODO a few support functions for trackers, most are math related and should likely be moved elsewhere
	float floor_decimals(const float p_value, const float p_decimals) {
		float power_of_10 = pow(10.0f, p_decimals);
		return floor(p_value * power_of_10) / power_of_10;
	}

	Vector3 floor_decimals(const Vector3 &p_vector, const float p_decimals) {
		return Vector3(floor_decimals(p_vector.x, p_decimals), floor_decimals(p_vector.y, p_decimals), floor_decimals(p_vector.z, p_decimals));
	}

	Vector3 low_pass(const Vector3 &p_vector, const Vector3 &p_last_vector, const float p_factor) {
		return p_vector + (p_factor * (p_last_vector - p_vector));
	}

	Vector3 scrub(const Vector3 &p_vector, const Vector3 &p_last_vector, const float p_decimals, const float p_factor) {
		return low_pass(floor_decimals(p_vector, p_decimals), p_last_vector, p_factor);
	}

	void set_position_from_sensors();

protected:
	static void _bind_methods();

public:
	void set_eye_height(const double p_eye_height);
	double get_eye_height() const;

	void set_iod(const double p_iod);
	double get_iod() const;

	void set_display_width(const double p_display_width);
	double get_display_width() const;

	void set_offset_rect(const Rect2 &p_offset_rect);
	Rect2 get_offset_rect() const;

	void set_display_to_lens(const double p_display_to_lens);
	double get_display_to_lens() const;

	void set_oversample(const double p_oversample);
	double get_oversample() const;

	void set_k1(const double p_k1);
	double get_k1() const;

	void set_k2(const double p_k2);
	double get_k2() const;

	float get_vrs_min_radius() const;
	void set_vrs_min_radius(float p_vrs_min_radius);

	float get_vrs_strength() const;
	void set_vrs_strength(float p_vrs_strength);

	virtual StringName get_name() const override;
	virtual uint32_t get_capabilities() const override;

	virtual TrackingStatus get_tracking_status() const override;

	virtual bool is_initialized() const override;
	virtual bool initialize() override;
	virtual void uninitialize() override;
	virtual Dictionary get_system_info() override;

	virtual bool supports_play_area_mode(XRInterface::PlayAreaMode p_mode) override;
	virtual XRInterface::PlayAreaMode get_play_area_mode() const override;
	virtual bool set_play_area_mode(XRInterface::PlayAreaMode p_mode) override;

	virtual Size2 get_render_target_size() override;
	virtual uint32_t get_view_count() override;
	virtual Transform3D get_camera_transform() override;
	virtual Transform3D get_transform_for_view(uint32_t p_view, const Transform3D &p_cam_transform) override;
	virtual Projection get_projection_for_view(uint32_t p_view, double p_aspect, double p_z_near, double p_z_far) override;
	virtual Vector<BlitToScreen> post_draw_viewport(RID p_render_target, const Rect2 &p_screen_rect) override;

	virtual void process() override;

	virtual RID get_vrs_texture() override;

	MobileVRInterface();
	~MobileVRInterface();
};
