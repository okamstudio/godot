/**************************************************************************/
/*  openxr_interface.h                                                    */
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

// A note on multithreading and thread safety in OpenXR.
//
// Most entry points will be called from the main thread in Godot
// however a number of entry points will be called from the
// rendering thread, potentially while we're already processing
// the next frame on the main thread.
//
// OpenXR itself has been designed with threading in mind including
// a high likelihood that the XR runtime runs in separate threads
// as well.
// Hence all the frame timing information, use of swapchains and
// sync functions.
// Do note that repeated calls to tracking APIs will provide
// increasingly more accurate data for the same timestamp as
// tracking data is continuously updated.
//
// For our code we mostly implement this in our OpenXRAPI class.
// We store data accessed from the rendering thread in a separate
// struct, setting values through our renderer command queue.
//
// As some data is setup before we start rendering, and cleaned up
// after we've stopped, that is accessed directly from both threads.

#include "action_map/openxr_action_map.h"
#include "extensions/openxr_hand_tracking_extension.h"
#include "openxr_api.h"

#include "servers/xr/xr_controller_tracker.h"
#include "servers/xr/xr_interface.h"

// declare some default strings
#define INTERACTION_PROFILE_NONE "/interaction_profiles/none"

class OpenXRInterface : public XRInterface {
	GDCLASS(OpenXRInterface, XRInterface);

private:
	OpenXRAPI *openxr_api = nullptr;
	bool initialized = false;
	bool reference_stage_changing = false;
	XRInterface::TrackingStatus tracking_state;

	// At a minimum we need a tracker for our head
	Ref<XRPositionalTracker> head;
	Transform3D head_transform;
	Vector3 head_linear_velocity;
	Vector3 head_angular_velocity;
	XRPose::TrackingConfidence head_confidence;
	Transform3D transform_for_view[2]; // We currently assume 2, but could be 4 for VARJO which we do not support yet

	XRVRS xr_vrs;

	void _load_action_map();

	struct Action { // An action we've registered with OpenXR
		String action_name; // Name of our action as presented to Godot (can be altered from the action map)
		OpenXRAction::ActionType action_type; // The action type of this action
		RID action_rid; // RID of the action registered with our OpenXR API
	};
	struct ActionSet { // An action set we've registered with OpenXR
		String action_set_name; // Name of our action set
		bool is_active; // If true this action set is active and we will sync it
		Vector<Action *> actions; // List of actions in this action set
		RID action_set_rid; // RID of the action registered with our OpenXR API
	};
	struct Tracker { // A tracker we've registered with OpenXR
		String tracker_name; // Name of our tracker (can be altered from the action map)
		Vector<Action *> actions; // Actions related to this tracker
		Ref<XRControllerTracker> controller_tracker; // Our positional tracker object that holds our tracker state
		RID tracker_rid; // RID of the tracker registered with our OpenXR API
		RID interaction_profile; // RID of the interaction profile bound to this tracker (can be null)
	};

	Vector<ActionSet *> action_sets;
	Vector<RID> interaction_profiles;
	Vector<Tracker *> trackers;

	ActionSet *create_action_set(const String &p_action_set_name, const String &p_localized_name, const int p_priority);
	void free_action_sets();

	Action *create_action(ActionSet *p_action_set, const String &p_action_name, const String &p_localized_name, OpenXRAction::ActionType p_action_type, const Vector<Tracker *> p_trackers);
	Action *find_action(const String &p_action_name);
	void free_actions(ActionSet *p_action_set);

	Tracker *find_tracker(const String &p_tracker_name, bool p_create = false);
	void handle_tracker(Tracker *p_tracker);
	void free_trackers();

	void free_interaction_profiles();

	void _set_default_pos(Transform3D &p_transform, double p_world_scale, uint64_t p_eye);

	void handle_hand_tracking(const String &p_path, OpenXRHandTrackingExtension::HandTrackedHands p_hand);

protected:
	static void _bind_methods();

public:
	virtual StringName get_name() const override;
	virtual uint32_t get_capabilities() const override;

	virtual PackedStringArray get_suggested_tracker_names() const override;
	virtual TrackingStatus get_tracking_status() const override;

	bool is_hand_tracking_supported();
	bool is_hand_interaction_supported() const;
	bool is_eye_gaze_interaction_supported();

	bool initialize_on_startup() const;
	virtual bool is_initialized() const override;
	virtual bool initialize() override;
	virtual void uninitialize() override;
	virtual Dictionary get_system_info() override;

	virtual void trigger_haptic_pulse(const String &p_action_name, const StringName &p_tracker_name, double p_frequency, double p_amplitude, double p_duration_sec, double p_delay_sec = 0) override;

	virtual bool supports_play_area_mode(XRInterface::PlayAreaMode p_mode) override;
	virtual XRInterface::PlayAreaMode get_play_area_mode() const override;
	virtual bool set_play_area_mode(XRInterface::PlayAreaMode p_mode) override;
	virtual PackedVector3Array get_play_area() const override;

	float get_display_refresh_rate() const;
	void set_display_refresh_rate(float p_refresh_rate);
	Array get_available_display_refresh_rates() const;

	bool is_action_set_active(const String &p_action_set) const;
	void set_action_set_active(const String &p_action_set, bool p_active);
	Array get_action_sets() const;

	double get_render_target_size_multiplier() const;
	void set_render_target_size_multiplier(double multiplier);

	bool is_foveation_supported() const;

	int get_foveation_level() const;
	void set_foveation_level(int p_foveation_level);

	bool get_foveation_dynamic() const;
	void set_foveation_dynamic(bool p_foveation_dynamic);

	float get_vrs_min_radius() const;
	void set_vrs_min_radius(float p_vrs_min_radius);

	float get_vrs_strength() const;
	void set_vrs_strength(float p_vrs_strength);

	virtual Size2 get_render_target_size() override;
	virtual uint32_t get_view_count() override;
	virtual Transform3D get_camera_transform() override;
	virtual Transform3D get_transform_for_view(uint32_t p_view, const Transform3D &p_cam_transform) override;
	virtual Projection get_projection_for_view(uint32_t p_view, double p_aspect, double p_z_near, double p_z_far) override;

	virtual Rect2i get_render_region() override;

	virtual RID get_color_texture() override;
	virtual RID get_depth_texture() override;
	virtual RID get_velocity_texture() override;
	virtual RID get_velocity_depth_texture() override;
	virtual Size2i get_velocity_target_size() override;

	virtual void process() override;
	virtual void pre_render() override;
	bool pre_draw_viewport(RID p_render_target) override;
	virtual Vector<BlitToScreen> post_draw_viewport(RID p_render_target, const Rect2 &p_screen_rect) override;
	virtual void end_frame() override;

	virtual bool is_passthrough_supported() override;
	virtual bool is_passthrough_enabled() override;
	virtual bool start_passthrough() override;
	virtual void stop_passthrough() override;

	/** environment blend mode. */
	virtual Array get_supported_environment_blend_modes() override;
	virtual XRInterface::EnvironmentBlendMode get_environment_blend_mode() const override;
	virtual bool set_environment_blend_mode(XRInterface::EnvironmentBlendMode mode) override;

	void on_state_ready();
	void on_state_visible();
	void on_state_focused();
	void on_state_stopping();
	void on_state_loss_pending();
	void on_state_exiting();
	void on_reference_space_change_pending();
	void on_refresh_rate_changes(float p_new_rate);
	void tracker_profile_changed(RID p_tracker, RID p_interaction_profile);

	/** Hand tracking. */
	enum Hand {
		HAND_LEFT,
		HAND_RIGHT,
		HAND_MAX,
	};

	enum HandMotionRange {
		HAND_MOTION_RANGE_UNOBSTRUCTED,
		HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER,
		HAND_MOTION_RANGE_MAX
	};

	void set_motion_range(const Hand p_hand, const HandMotionRange p_motion_range);
	HandMotionRange get_motion_range(const Hand p_hand) const;

	enum HandTrackedSource {
		HAND_TRACKED_SOURCE_UNKNOWN,
		HAND_TRACKED_SOURCE_UNOBSTRUCTED,
		HAND_TRACKED_SOURCE_CONTROLLER,
		HAND_TRACKED_SOURCE_MAX
	};

	HandTrackedSource get_hand_tracking_source(const Hand p_hand) const;

	enum HandJoints {
		HAND_JOINT_PALM = 0,
		HAND_JOINT_WRIST = 1,
		HAND_JOINT_THUMB_METACARPAL = 2,
		HAND_JOINT_THUMB_PROXIMAL = 3,
		HAND_JOINT_THUMB_DISTAL = 4,
		HAND_JOINT_THUMB_TIP = 5,
		HAND_JOINT_INDEX_METACARPAL = 6,
		HAND_JOINT_INDEX_PROXIMAL = 7,
		HAND_JOINT_INDEX_INTERMEDIATE = 8,
		HAND_JOINT_INDEX_DISTAL = 9,
		HAND_JOINT_INDEX_TIP = 10,
		HAND_JOINT_MIDDLE_METACARPAL = 11,
		HAND_JOINT_MIDDLE_PROXIMAL = 12,
		HAND_JOINT_MIDDLE_INTERMEDIATE = 13,
		HAND_JOINT_MIDDLE_DISTAL = 14,
		HAND_JOINT_MIDDLE_TIP = 15,
		HAND_JOINT_RING_METACARPAL = 16,
		HAND_JOINT_RING_PROXIMAL = 17,
		HAND_JOINT_RING_INTERMEDIATE = 18,
		HAND_JOINT_RING_DISTAL = 19,
		HAND_JOINT_RING_TIP = 20,
		HAND_JOINT_LITTLE_METACARPAL = 21,
		HAND_JOINT_LITTLE_PROXIMAL = 22,
		HAND_JOINT_LITTLE_INTERMEDIATE = 23,
		HAND_JOINT_LITTLE_DISTAL = 24,
		HAND_JOINT_LITTLE_TIP = 25,
		HAND_JOINT_MAX = 26,
	};

	enum HandJointFlags {
		HAND_JOINT_NONE = 0,
		HAND_JOINT_ORIENTATION_VALID = 1,
		HAND_JOINT_ORIENTATION_TRACKED = 2,
		HAND_JOINT_POSITION_VALID = 4,
		HAND_JOINT_POSITION_TRACKED = 8,
		HAND_JOINT_LINEAR_VELOCITY_VALID = 16,
		HAND_JOINT_ANGULAR_VELOCITY_VALID = 32,
	};

	BitField<HandJointFlags> get_hand_joint_flags(Hand p_hand, HandJoints p_joint) const;
	Quaternion get_hand_joint_rotation(Hand p_hand, HandJoints p_joint) const;
	Vector3 get_hand_joint_position(Hand p_hand, HandJoints p_joint) const;
	float get_hand_joint_radius(Hand p_hand, HandJoints p_joint) const;

	Vector3 get_hand_joint_linear_velocity(Hand p_hand, HandJoints p_joint) const;
	Vector3 get_hand_joint_angular_velocity(Hand p_hand, HandJoints p_joint) const;

	virtual RID get_vrs_texture() override;

	OpenXRInterface();
	~OpenXRInterface();
};

VARIANT_ENUM_CAST(OpenXRInterface::Hand)
VARIANT_ENUM_CAST(OpenXRInterface::HandMotionRange)
VARIANT_ENUM_CAST(OpenXRInterface::HandTrackedSource)
VARIANT_ENUM_CAST(OpenXRInterface::HandJoints)
VARIANT_BITFIELD_CAST(OpenXRInterface::HandJointFlags)
