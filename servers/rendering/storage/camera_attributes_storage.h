/**************************************************************************/
/*  camera_attributes_storage.h                                           */
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

#include "core/templates/rid_owner.h"
#include "servers/rendering_server.h"

class RendererCameraAttributes {
private:
	static RendererCameraAttributes *singleton;

	struct CameraAttributes {
		float exposure_multiplier = 1.0;
		float exposure_normalization = 1.0;
		float exposure_sensitivity = 100.0; // In ISO.

		bool use_auto_exposure = false;
		float auto_exposure_min_sensitivity = 50.0;
		float auto_exposure_max_sensitivity = 800.0;
		float auto_exposure_adjust_speed = 1.0;
		float auto_exposure_scale = 1.0;
		uint64_t auto_exposure_version = 0;

		bool dof_blur_far_enabled = false;
		float dof_blur_far_distance = 10;
		float dof_blur_far_transition = 5;
		bool dof_blur_near_enabled = false;
		float dof_blur_near_distance = 2;
		float dof_blur_near_transition = 1;
		float dof_blur_amount = 0.1;
	};

	RS::DOFBlurQuality dof_blur_quality = RS::DOF_BLUR_QUALITY_MEDIUM;
	RS::DOFBokehShape dof_blur_bokeh_shape = RS::DOF_BOKEH_HEXAGON;
	bool dof_blur_use_jitter = false;
	static uint64_t auto_exposure_counter;

	mutable RID_Owner<CameraAttributes, true> camera_attributes_owner;

public:
	static RendererCameraAttributes *get_singleton() { return singleton; }

	RendererCameraAttributes();
	~RendererCameraAttributes();

	CameraAttributes *get_camera_attributes(RID p_rid) { return camera_attributes_owner.get_or_null(p_rid); }
	bool owns_camera_attributes(RID p_rid) { return camera_attributes_owner.owns(p_rid); }

	RID camera_attributes_allocate();
	void camera_attributes_initialize(RID p_rid);
	void camera_attributes_free(RID p_rid);

	void camera_attributes_set_dof_blur_quality(RS::DOFBlurQuality p_quality, bool p_use_jitter);
	void camera_attributes_set_dof_blur_bokeh_shape(RS::DOFBokehShape p_shape);

	void camera_attributes_set_dof_blur(RID p_camera_attributes, bool p_far_enable, float p_far_distance, float p_far_transition, bool p_near_enable, float p_near_distance, float p_near_transition, float p_amount);
	bool camera_attributes_get_dof_far_enabled(RID p_camera_attributes);
	float camera_attributes_get_dof_far_distance(RID p_camera_attributes);
	float camera_attributes_get_dof_far_transition(RID p_camera_attributes);
	bool camera_attributes_get_dof_near_enabled(RID p_camera_attributes);
	float camera_attributes_get_dof_near_distance(RID p_camera_attributes);
	float camera_attributes_get_dof_near_transition(RID p_camera_attributes);
	float camera_attributes_get_dof_blur_amount(RID p_camera_attributes);

	_FORCE_INLINE_ bool camera_attributes_uses_dof(RID p_camera_attributes) {
		CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);

		return cam_attributes && (cam_attributes->dof_blur_near_enabled || cam_attributes->dof_blur_far_enabled) && cam_attributes->dof_blur_amount > 0.0;
	}

	void camera_attributes_set_exposure(RID p_camera_attributes, float p_multiplier, float p_exposure_normalization);
	float camera_attributes_get_exposure_normalization_factor(RID p_camera_attributes);

	void camera_attributes_set_auto_exposure(RID p_camera_attributes, bool p_enable, float p_min_sensitivity, float p_max_sensitivity, float p_speed, float p_scale);
	float camera_attributes_get_auto_exposure_min_sensitivity(RID p_camera_attributes);
	float camera_attributes_get_auto_exposure_max_sensitivity(RID p_camera_attributes);
	float camera_attributes_get_auto_exposure_adjust_speed(RID p_camera_attributes);
	float camera_attributes_get_auto_exposure_scale(RID p_camera_attributes);
	uint64_t camera_attributes_get_auto_exposure_version(RID p_camera_attributes);

	_FORCE_INLINE_ bool camera_attributes_uses_auto_exposure(RID p_camera_attributes) {
		CameraAttributes *cam_attributes = camera_attributes_owner.get_or_null(p_camera_attributes);

		return cam_attributes && cam_attributes->use_auto_exposure;
	}

	_FORCE_INLINE_ RS::DOFBlurQuality camera_attributes_get_dof_blur_quality() {
		return dof_blur_quality;
	}

	_FORCE_INLINE_ RS::DOFBokehShape camera_attributes_get_dof_blur_bokeh_shape() {
		return dof_blur_bokeh_shape;
	}

	_FORCE_INLINE_ bool camera_attributes_get_dof_blur_use_jitter() {
		return dof_blur_use_jitter;
	}
};
