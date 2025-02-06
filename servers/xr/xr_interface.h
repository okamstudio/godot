/**************************************************************************/
/*  xr_interface.h                                                        */
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

#include "core/math/projection.h"
#include "core/os/thread_safe.h"
#include "servers/xr_server.h"
#include "xr_vrs.h"

// forward declaration
struct BlitToScreen;

/**
	The XR interface is a template class on top of which we build interface to different AR, VR and tracking SDKs.
	The idea is that we subclass this class, implement the logic, and then instantiate a singleton of each interface
	when Godot starts. These instances do not initialize themselves but register themselves with the AR/VR server.

	If the user wants to enable AR/VR, they can choose the interface they want to use and initialize it.

	Note that we may make this into a fully instantiable class for GDExtension support.
*/

class XRInterface : public RefCounted {
	GDCLASS(XRInterface, RefCounted);

public:
	enum Capabilities { /* purely metadata, provides some info about what this interface supports */
		XR_NONE = 0, /* no capabilities */
		XR_MONO = 1, /* can be used with mono output */
		XR_STEREO = 2, /* can be used with stereo output */
		XR_QUAD = 4, /* can be used with quad output (not currently supported) */
		XR_VR = 8, /* offers VR support */
		XR_AR = 16, /* offers AR support */
		XR_EXTERNAL = 32 /* renders to external device */
	};

	enum TrackingStatus { /* tracking status currently based on AR but we can start doing more with this for VR as well */
		XR_NORMAL_TRACKING,
		XR_EXCESSIVE_MOTION,
		XR_INSUFFICIENT_FEATURES,
		XR_UNKNOWN_TRACKING,
		XR_NOT_TRACKING
	};

	enum PlayAreaMode { /* defines the mode used by the XR interface for tracking */
		XR_PLAY_AREA_UNKNOWN, /* Area mode not set or not available */
		XR_PLAY_AREA_3DOF, /* Only support orientation tracking, no positional tracking, area will center around player */
		XR_PLAY_AREA_SITTING, /* Player is in seated position, limited positional tracking, fixed guardian around player */
		XR_PLAY_AREA_ROOMSCALE, /* Player is free to move around, full positional tracking */
		XR_PLAY_AREA_STAGE, /* Same as roomscale but origin point is fixed to the center of the physical space */
	};

	enum EnvironmentBlendMode {
		XR_ENV_BLEND_MODE_OPAQUE, /* You cannot see the real world, VR like */
		XR_ENV_BLEND_MODE_ADDITIVE, /* You can see the real world, AR like */
		XR_ENV_BLEND_MODE_ALPHA_BLEND, /* Real world is passed through where alpha channel is 0.0 and gradually blends to opaque for value 1.0. */
	};

protected:
	_THREAD_SAFE_CLASS_

	static void _bind_methods();

public:
	/** general interface information **/
	virtual StringName get_name() const = 0;
	virtual uint32_t get_capabilities() const = 0;

	bool is_primary();
	void set_primary(bool p_is_primary);

	virtual bool is_initialized() const = 0; /* returns true if we've initialized this interface */
	virtual bool initialize() = 0; /* initialize this interface, if this has an HMD it becomes the primary interface */
	virtual void uninitialize() = 0; /* deinitialize this interface */
	virtual Dictionary get_system_info() = 0; /* return a dictionary with info about our system */

	/** input and output **/

	virtual PackedStringArray get_suggested_tracker_names() const; /* return a list of likely/suggested tracker names */
	virtual PackedStringArray get_suggested_pose_names(const StringName &p_tracker_name) const; /* return a list of likely/suggested action names for this tracker */
	virtual TrackingStatus get_tracking_status() const; /* get the status of our current tracking */
	virtual void trigger_haptic_pulse(const String &p_action_name, const StringName &p_tracker_name, double p_frequency, double p_amplitude, double p_duration_sec, double p_delay_sec = 0); /* trigger a haptic pulse */

	/** specific to VR **/
	virtual bool supports_play_area_mode(XRInterface::PlayAreaMode p_mode); /* query if this interface supports this play area mode */
	virtual XRInterface::PlayAreaMode get_play_area_mode() const; /* get the current play area mode */
	virtual bool set_play_area_mode(XRInterface::PlayAreaMode p_mode); /* change the play area mode, note that this should return false if the mode is not available */
	virtual PackedVector3Array get_play_area() const; /* if available, returns an array of vectors denoting the play area the player can move around in */

	/** specific to AR **/
	virtual bool get_anchor_detection_is_enabled() const;
	virtual void set_anchor_detection_is_enabled(bool p_enable);
	virtual int get_camera_feed_id();

	/** rendering and internal **/

	// These methods are called from the main thread.
	virtual Transform3D get_camera_transform() = 0; /* returns the position of our camera, only used for updating reference frame. For monoscopic this is equal to the views transform, for stereoscopic this should be an average */
	virtual void process() = 0;

	// These methods can be called from both main and render thread.
	virtual Size2 get_render_target_size() = 0; /* returns the recommended render target size per eye for this device */
	virtual uint32_t get_view_count() = 0; /* returns the view count we need (1 is monoscopic, 2 is stereoscopic but can be more) */

	// These methods are called from the rendering thread.
	virtual Transform3D get_transform_for_view(uint32_t p_view, const Transform3D &p_cam_transform) = 0; /* get each views transform */
	virtual Projection get_projection_for_view(uint32_t p_view, double p_aspect, double p_z_near, double p_z_far) = 0; /* get each view projection matrix */
	virtual RID get_color_texture(); /* obtain color output texture (if applicable) */
	virtual RID get_depth_texture(); /* obtain depth output texture (if applicable, used for reprojection) */
	virtual RID get_velocity_texture(); /* obtain velocity output texture (if applicable, used for spacewarp) */
	virtual RID get_velocity_depth_texture();
	virtual Size2i get_velocity_target_size();
	virtual Rect2i get_render_region();
	virtual void pre_render() {}
	virtual bool pre_draw_viewport(RID p_render_target) { return true; } /* inform XR interface we are about to start our viewport draw process */
	virtual Vector<BlitToScreen> post_draw_viewport(RID p_render_target, const Rect2 &p_screen_rect) = 0; /* inform XR interface we finished our viewport draw process */
	virtual void end_frame() {}

	/** passthrough **/

	virtual bool is_passthrough_supported() { return false; }
	virtual bool is_passthrough_enabled() { return false; }
	virtual bool start_passthrough() { return false; }
	virtual void stop_passthrough() {}

	/** environment blend mode **/
	virtual Array get_supported_environment_blend_modes();
	virtual XRInterface::EnvironmentBlendMode get_environment_blend_mode() const { return XR_ENV_BLEND_MODE_OPAQUE; }
	virtual bool set_environment_blend_mode(EnvironmentBlendMode mode) { return false; }

	/** VRS **/
	virtual RID get_vrs_texture(); /* obtain VRS texture */

	XRInterface();
	~XRInterface();
};

VARIANT_ENUM_CAST(XRInterface::Capabilities);
VARIANT_ENUM_CAST(XRInterface::TrackingStatus);
VARIANT_ENUM_CAST(XRInterface::PlayAreaMode);
VARIANT_ENUM_CAST(XRInterface::EnvironmentBlendMode);
