/**************************************************************************/
/*  render_scene_buffers.h                                                */
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

#include "core/object/ref_counted.h"
#include "servers/rendering_server.h"

class RenderSceneBuffersConfiguration : public RefCounted {
	GDCLASS(RenderSceneBuffersConfiguration, RefCounted);

private:
	RID render_target;

	Size2i internal_size;
	Size2i target_size;
	uint32_t view_count = 1;

	RS::ViewportScaling3DMode scaling_3d_mode = RS::VIEWPORT_SCALING_3D_MODE_OFF;
	RS::ViewportMSAA msaa_3d = RS::VIEWPORT_MSAA_DISABLED;
	RS::ViewportScreenSpaceAA screen_space_aa = RS::VIEWPORT_SCREEN_SPACE_AA_DISABLED;
	RS::ViewportAnisotropicFiltering anisotropic_filtering_level = RS::VIEWPORT_ANISOTROPY_4X;

	float fsr_sharpness = 0.0;
	float texture_mipmap_bias = 0.0;
	bool use_taa = false;
	bool use_debanding = false;

protected:
	static void _bind_methods();

public:
	RID get_render_target() const { return render_target; }
	void set_render_target(RID p_render_target) { render_target = p_render_target; }

	Size2i get_internal_size() const { return internal_size; }
	void set_internal_size(Size2i p_internal_size) { internal_size = p_internal_size; }

	Size2i get_target_size() const { return target_size; }
	void set_target_size(Size2i p_target_size) { target_size = p_target_size; }

	uint32_t get_view_count() const { return view_count; }
	void set_view_count(uint32_t p_view_count) { view_count = p_view_count; }

	RS::ViewportScaling3DMode get_scaling_3d_mode() const { return scaling_3d_mode; }
	void set_scaling_3d_mode(RS::ViewportScaling3DMode p_scaling_3d_mode) { scaling_3d_mode = p_scaling_3d_mode; }

	RS::ViewportMSAA get_msaa_3d() const { return msaa_3d; }
	void set_msaa_3d(RS::ViewportMSAA p_msaa_3d) { msaa_3d = p_msaa_3d; }

	RS::ViewportScreenSpaceAA get_screen_space_aa() const { return screen_space_aa; }
	void set_screen_space_aa(RS::ViewportScreenSpaceAA p_screen_space_aa) { screen_space_aa = p_screen_space_aa; }

	float get_fsr_sharpness() const { return fsr_sharpness; }
	void set_fsr_sharpness(float p_fsr_sharpness) { fsr_sharpness = p_fsr_sharpness; }

	float get_texture_mipmap_bias() const { return texture_mipmap_bias; }
	void set_texture_mipmap_bias(float p_texture_mipmap_bias) { texture_mipmap_bias = p_texture_mipmap_bias; }

	RS::ViewportAnisotropicFiltering get_anisotropic_filtering_level() const { return anisotropic_filtering_level; }
	void set_anisotropic_filtering_level(RS::ViewportAnisotropicFiltering p_anisotropic_filtering_level) { anisotropic_filtering_level = p_anisotropic_filtering_level; }

	bool get_use_taa() const { return use_taa; }
	void set_use_taa(bool p_use_taa) { use_taa = p_use_taa; }

	bool get_use_debanding() const { return use_debanding; }
	void set_use_debanding(bool p_use_debanding) { use_debanding = p_use_debanding; }

	RenderSceneBuffersConfiguration() {}
	virtual ~RenderSceneBuffersConfiguration() {}
};

class RenderSceneBuffers : public RefCounted {
	GDCLASS(RenderSceneBuffers, RefCounted);

protected:
	static void _bind_methods();

public:
	RenderSceneBuffers() {}
	virtual ~RenderSceneBuffers() {}

	virtual void configure(const RenderSceneBuffersConfiguration *p_config) = 0;

	// for those settings that are unlikely to require buffers to be recreated, we'll add setters
	virtual void set_fsr_sharpness(float p_fsr_sharpness) = 0;
	virtual void set_texture_mipmap_bias(float p_texture_mipmap_bias) = 0;
	virtual void set_anisotropic_filtering_level(RS::ViewportAnisotropicFiltering p_anisotropic_filtering_level) = 0;
	virtual void set_use_debanding(bool p_use_debanding) = 0;
};

class RenderSceneBuffersExtension : public RenderSceneBuffers {
	GDCLASS(RenderSceneBuffersExtension, RenderSceneBuffers);

protected:
	static void _bind_methods();

	GDVIRTUAL1(_configure, const RenderSceneBuffersConfiguration *)
	GDVIRTUAL1(_set_fsr_sharpness, float)
	GDVIRTUAL1(_set_texture_mipmap_bias, float)
	GDVIRTUAL1(_set_anisotropic_filtering_level, int)
	GDVIRTUAL1(_set_use_debanding, bool)

public:
	virtual ~RenderSceneBuffersExtension() {}

	virtual void configure(const RenderSceneBuffersConfiguration *p_config) override;

	virtual void set_fsr_sharpness(float p_fsr_sharpness) override;
	virtual void set_texture_mipmap_bias(float p_texture_mipmap_bias) override;
	virtual void set_anisotropic_filtering_level(RS::ViewportAnisotropicFiltering p_anisotropic_filtering_level) override;
	virtual void set_use_debanding(bool p_use_debanding) override;
};
