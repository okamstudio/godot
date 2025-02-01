/**************************************************************************/
/*  render_data_rd.h                                                      */
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

#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/storage/render_data.h"

class RenderDataRD : public RenderData {
	GDCLASS(RenderDataRD, RenderData);

public:
	// Access methods to expose data externally
	virtual Ref<RenderSceneBuffers> get_render_scene_buffers() const override;
	virtual RenderSceneData *get_render_scene_data() const override;

	virtual RID get_environment() const override;
	virtual RID get_camera_attributes() const override;

	// Members are publicly accessible within the render engine.
	Ref<RenderSceneBuffersRD> render_buffers;
	RenderSceneDataRD *scene_data = nullptr;

	const PagedArray<RenderGeometryInstance *> *instances = nullptr;
	const PagedArray<RID> *lights = nullptr;
	const PagedArray<RID> *reflection_probes = nullptr;
	const PagedArray<RID> *voxel_gi_instances = nullptr;
	const PagedArray<RID> *decals = nullptr;
	const PagedArray<RID> *lightmaps = nullptr;
	const PagedArray<RID> *fog_volumes = nullptr;
	RID environment;
	RID camera_attributes;
	RID compositor;
	RID shadow_atlas;
	RID occluder_debug_tex;
	RID reflection_atlas;
	RID reflection_probe;
	int reflection_probe_pass = 0;

	RID cluster_buffer;
	uint32_t cluster_size = 0;
	uint32_t cluster_max_elements = 0;

	uint32_t directional_light_count = 0;
	bool directional_light_soft_shadows = false;

	bool lightmap_bicubic_filter = false;

	RenderingMethod::RenderInfo *render_info = nullptr;

	/* Viewport data */
	bool transparent_bg = false;
	Rect2i render_region;

	/* Shadow data */
	const RendererSceneRender::RenderShadowData *render_shadows = nullptr;
	int render_shadow_count = 0;

	LocalVector<int> cube_shadows;
	LocalVector<int> shadows;
	LocalVector<int> directional_shadows;

	/* GI info */
	const RendererSceneRender::RenderSDFGIData *render_sdfgi_regions = nullptr;
	int render_sdfgi_region_count = 0;
	const RendererSceneRender::RenderSDFGIUpdateData *sdfgi_update_data = nullptr;

	uint32_t voxel_gi_count = 0;
};
