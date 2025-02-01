/**************************************************************************/
/*  gi.h                                                                  */
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

#ifdef GLES3_ENABLED

#include "servers/rendering/environment/renderer_gi.h"

namespace GLES3 {

class GI : public RendererGI {
public:
	/* VOXEL GI API */

	virtual RID voxel_gi_allocate() override;
	virtual void voxel_gi_free(RID p_rid) override;
	virtual void voxel_gi_initialize(RID p_rid) override;
	virtual void voxel_gi_allocate_data(RID p_voxel_gi, const Transform3D &p_to_cell_xform, const AABB &p_aabb, const Vector3i &p_octree_size, const Vector<uint8_t> &p_octree_cells, const Vector<uint8_t> &p_data_cells, const Vector<uint8_t> &p_distance_field, const Vector<int> &p_level_counts) override;

	virtual AABB voxel_gi_get_bounds(RID p_voxel_gi) const override;
	virtual Vector3i voxel_gi_get_octree_size(RID p_voxel_gi) const override;
	virtual Vector<uint8_t> voxel_gi_get_octree_cells(RID p_voxel_gi) const override;
	virtual Vector<uint8_t> voxel_gi_get_data_cells(RID p_voxel_gi) const override;
	virtual Vector<uint8_t> voxel_gi_get_distance_field(RID p_voxel_gi) const override;

	virtual Vector<int> voxel_gi_get_level_counts(RID p_voxel_gi) const override;
	virtual Transform3D voxel_gi_get_to_cell_xform(RID p_voxel_gi) const override;

	virtual void voxel_gi_set_dynamic_range(RID p_voxel_gi, float p_range) override;
	virtual float voxel_gi_get_dynamic_range(RID p_voxel_gi) const override;

	virtual void voxel_gi_set_propagation(RID p_voxel_gi, float p_range) override;
	virtual float voxel_gi_get_propagation(RID p_voxel_gi) const override;

	virtual void voxel_gi_set_energy(RID p_voxel_gi, float p_range) override;
	virtual float voxel_gi_get_energy(RID p_voxel_gi) const override;

	virtual void voxel_gi_set_baked_exposure_normalization(RID p_voxel_gi, float p_baked_exposure) override;
	virtual float voxel_gi_get_baked_exposure_normalization(RID p_voxel_gi) const override;

	virtual void voxel_gi_set_bias(RID p_voxel_gi, float p_range) override;
	virtual float voxel_gi_get_bias(RID p_voxel_gi) const override;

	virtual void voxel_gi_set_normal_bias(RID p_voxel_gi, float p_range) override;
	virtual float voxel_gi_get_normal_bias(RID p_voxel_gi) const override;

	virtual void voxel_gi_set_interior(RID p_voxel_gi, bool p_enable) override;
	virtual bool voxel_gi_is_interior(RID p_voxel_gi) const override;

	virtual void voxel_gi_set_use_two_bounces(RID p_voxel_gi, bool p_enable) override;
	virtual bool voxel_gi_is_using_two_bounces(RID p_voxel_gi) const override;

	virtual uint32_t voxel_gi_get_version(RID p_voxel_gi) const override;

	virtual void sdfgi_reset() override;
};

}; // namespace GLES3

#endif // GLES3_ENABLED
