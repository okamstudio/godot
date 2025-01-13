/**************************************************************************/
/*  mesh_storage.cpp                                                      */
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

#include "mesh_storage.h"

#include "core/math/transform_interpolator.h"

#if defined(DEBUG_ENABLED) && defined(TOOLS_ENABLED)
#include "core/config/project_settings.h"
#endif

RID RendererMeshStorage::multimesh_allocate() {
	return _multimesh_allocate();
}

void RendererMeshStorage::multimesh_initialize(RID p_rid) {
	_multimesh_initialize(p_rid);
}

void RendererMeshStorage::multimesh_free(RID p_rid) {
	_multimesh_free(p_rid);
}

void RendererMeshStorage::multimesh_allocate_data(RID p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, bool p_use_colors, bool p_use_custom_data) {
	MultiMeshInterpolator *mmi = _multimesh_get_interpolator(p_multimesh);
	if (mmi) {
		mmi->_transform_format = p_transform_format;
		mmi->_use_colors = p_use_colors;
		mmi->_use_custom_data = p_use_custom_data;
		mmi->_num_instances = p_instances;

		mmi->_vf_size_xform = p_transform_format == RS::MULTIMESH_TRANSFORM_2D ? 8 : 12;
		mmi->_vf_size_color = p_use_colors ? 4 : 0;
		mmi->_vf_size_data = p_use_custom_data ? 4 : 0;

		mmi->_stride = mmi->_vf_size_xform + mmi->_vf_size_color + mmi->_vf_size_data;

		int size_in_floats = p_instances * mmi->_stride;
		mmi->_data_curr.resize_zeroed(size_in_floats);
		mmi->_data_prev.resize_zeroed(size_in_floats);
		mmi->_data_interpolated.resize_zeroed(size_in_floats);
	}

	_multimesh_allocate_data(p_multimesh, p_instances, p_transform_format, p_use_colors, p_use_custom_data);
}

int RendererMeshStorage::multimesh_get_instance_count(RID p_multimesh) const {
	return _multimesh_get_instance_count(p_multimesh);
}

void RendererMeshStorage::multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
	_multimesh_set_mesh(p_multimesh, p_mesh);
}

void RendererMeshStorage::multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) {
	MultiMeshInterpolator *mmi = _multimesh_get_interpolator(p_multimesh);
	if (mmi && mmi->interpolated) {
		ERR_FAIL_COND(p_index >= mmi->_num_instances);
		ERR_FAIL_COND(mmi->_vf_size_xform != 12);

		int start = p_index * mmi->_stride;
		float *ptr = mmi->_data_curr.ptrw();
		ptr += start;

		const Transform3D &t = p_transform;
		ptr[0] = t.basis.rows[0][0];
		ptr[1] = t.basis.rows[0][1];
		ptr[2] = t.basis.rows[0][2];
		ptr[3] = t.origin.x;
		ptr[4] = t.basis.rows[1][0];
		ptr[5] = t.basis.rows[1][1];
		ptr[6] = t.basis.rows[1][2];
		ptr[7] = t.origin.y;
		ptr[8] = t.basis.rows[2][0];
		ptr[9] = t.basis.rows[2][1];
		ptr[10] = t.basis.rows[2][2];
		ptr[11] = t.origin.z;

		_multimesh_add_to_interpolation_lists(p_multimesh, *mmi);

#if defined(DEBUG_ENABLED) && defined(TOOLS_ENABLED)
		if (!Engine::get_singleton()->is_in_physics_frame()) {
			PHYSICS_INTERPOLATION_WARNING("MultiMesh interpolation is being triggered from outside physics process, this might lead to issues");
		}
#endif

		return;
	}

	_multimesh_instance_set_transform(p_multimesh, p_index, p_transform);
}

void RendererMeshStorage::multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
	_multimesh_instance_set_transform_2d(p_multimesh, p_index, p_transform);
}

void RendererMeshStorage::multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) {
	MultiMeshInterpolator *mmi = _multimesh_get_interpolator(p_multimesh);
	if (mmi && mmi->interpolated) {
		ERR_FAIL_COND(p_index >= mmi->_num_instances);
		ERR_FAIL_COND(mmi->_vf_size_color == 0);

		int start = (p_index * mmi->_stride) + mmi->_vf_size_xform;
		float *ptr = mmi->_data_curr.ptrw();
		ptr += start;

		if (mmi->_vf_size_color == 4) {
			for (int n = 0; n < 4; n++) {
				ptr[n] = p_color.components[n];
			}
		} else {
#ifdef DEV_ENABLED
			// The options are currently 4 or zero, but just in case this changes in future...
			ERR_FAIL_COND(mmi->_vf_size_color != 0);
#endif
		}
		_multimesh_add_to_interpolation_lists(p_multimesh, *mmi);
		return;
	}

	_multimesh_instance_set_color(p_multimesh, p_index, p_color);
}

void RendererMeshStorage::multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) {
	MultiMeshInterpolator *mmi = _multimesh_get_interpolator(p_multimesh);
	if (mmi && mmi->interpolated) {
		ERR_FAIL_COND(p_index >= mmi->_num_instances);
		ERR_FAIL_COND(mmi->_vf_size_data == 0);

		int start = (p_index * mmi->_stride) + mmi->_vf_size_xform + mmi->_vf_size_color;
		float *ptr = mmi->_data_curr.ptrw();
		ptr += start;

		if (mmi->_vf_size_data == 4) {
			for (int n = 0; n < 4; n++) {
				ptr[n] = p_color.components[n];
			}
		} else {
#ifdef DEV_ENABLED
			// The options are currently 4 or zero, but just in case this changes in future...
			ERR_FAIL_COND(mmi->_vf_size_data != 0);
#endif
		}
		_multimesh_add_to_interpolation_lists(p_multimesh, *mmi);
		return;
	}

	_multimesh_instance_set_custom_data(p_multimesh, p_index, p_color);
}

void RendererMeshStorage::multimesh_set_custom_aabb(RID p_multimesh, const AABB &p_aabb) {
	_multimesh_set_custom_aabb(p_multimesh, p_aabb);
}

AABB RendererMeshStorage::multimesh_get_custom_aabb(RID p_multimesh) const {
	return _multimesh_get_custom_aabb(p_multimesh);
}

RID RendererMeshStorage::multimesh_get_mesh(RID p_multimesh) const {
	return _multimesh_get_mesh(p_multimesh);
}

Transform3D RendererMeshStorage::multimesh_instance_get_transform(RID p_multimesh, int p_index) const {
	return _multimesh_instance_get_transform(p_multimesh, p_index);
}

Transform2D RendererMeshStorage::multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const {
	return _multimesh_instance_get_transform_2d(p_multimesh, p_index);
}

Color RendererMeshStorage::multimesh_instance_get_color(RID p_multimesh, int p_index) const {
	return _multimesh_instance_get_color(p_multimesh, p_index);
}

Color RendererMeshStorage::multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const {
	return _multimesh_instance_get_custom_data(p_multimesh, p_index);
}

void RendererMeshStorage::multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer) {
	MultiMeshInterpolator *mmi = _multimesh_get_interpolator(p_multimesh);
	if (mmi && mmi->interpolated) {
		ERR_FAIL_COND_MSG(p_buffer.size() != mmi->_data_curr.size(), vformat("Buffer should have %d elements, got %d instead.", mmi->_data_curr.size(), p_buffer.size()));

		mmi->_data_curr = p_buffer;
		_multimesh_add_to_interpolation_lists(p_multimesh, *mmi);

#if defined(DEBUG_ENABLED) && defined(TOOLS_ENABLED)
		if (!Engine::get_singleton()->is_in_physics_frame()) {
			PHYSICS_INTERPOLATION_WARNING("MultiMesh interpolation is being triggered from outside physics process, this might lead to issues");
		}
#endif

		return;
	}

	_multimesh_set_buffer(p_multimesh, p_buffer);
}

RID RendererMeshStorage::multimesh_get_buffer_rd_rid(RID p_multimesh) const {
	return _multimesh_get_buffer_rd_rid(p_multimesh);
}

Vector<float> RendererMeshStorage::multimesh_get_buffer(RID p_multimesh) const {
	return _multimesh_get_buffer(p_multimesh);
}

void RendererMeshStorage::multimesh_set_buffer_interpolated(RID p_multimesh, const Vector<float> &p_buffer, const Vector<float> &p_buffer_prev) {
	MultiMeshInterpolator *mmi = _multimesh_get_interpolator(p_multimesh);
	if (mmi) {
		ERR_FAIL_COND_MSG(p_buffer.size() != mmi->_data_curr.size(), vformat("Buffer for current frame should have %d elements, got %d instead.", mmi->_data_curr.size(), p_buffer.size()));
		ERR_FAIL_COND_MSG(p_buffer_prev.size() != mmi->_data_prev.size(), vformat("Buffer for previous frame should have %d elements, got %d instead.", mmi->_data_prev.size(), p_buffer_prev.size()));

		// We are assuming that mmi->interpolated is the case. (Can possibly assert this?)
		// Even if this flag hasn't been set - just calling this function suggests interpolation is desired.
		mmi->_data_prev = p_buffer_prev;
		mmi->_data_curr = p_buffer;
		_multimesh_add_to_interpolation_lists(p_multimesh, *mmi);

#if defined(DEBUG_ENABLED) && defined(TOOLS_ENABLED)
		if (!Engine::get_singleton()->is_in_physics_frame()) {
			PHYSICS_INTERPOLATION_WARNING("MultiMesh interpolation is being triggered from outside physics process, this might lead to issues");
		}
#endif
	}
}

void RendererMeshStorage::multimesh_set_physics_interpolated(RID p_multimesh, bool p_interpolated) {
	MultiMeshInterpolator *mmi = _multimesh_get_interpolator(p_multimesh);
	if (mmi) {
		mmi->interpolated = p_interpolated;
	}
}

void RendererMeshStorage::multimesh_set_physics_interpolation_quality(RID p_multimesh, RS::MultimeshPhysicsInterpolationQuality p_quality) {
	ERR_FAIL_COND((p_quality < 0) || (p_quality > 1));
	MultiMeshInterpolator *mmi = _multimesh_get_interpolator(p_multimesh);
	if (mmi) {
		mmi->quality = (int)p_quality;
	}
}

void RendererMeshStorage::multimesh_instance_reset_physics_interpolation(RID p_multimesh, int p_index) {
	MultiMeshInterpolator *mmi = _multimesh_get_interpolator(p_multimesh);
	if (mmi) {
		ERR_FAIL_INDEX(p_index, mmi->_num_instances);

		float *w = mmi->_data_prev.ptrw();
		const float *r = mmi->_data_curr.ptr();
		int start = p_index * mmi->_stride;

		for (int n = 0; n < mmi->_stride; n++) {
			w[start + n] = r[start + n];
		}
	}
}

void RendererMeshStorage::multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
	return _multimesh_set_visible_instances(p_multimesh, p_visible);
}

int RendererMeshStorage::multimesh_get_visible_instances(RID p_multimesh) const {
	return _multimesh_get_visible_instances(p_multimesh);
}

AABB RendererMeshStorage::multimesh_get_aabb(RID p_multimesh) {
	return _multimesh_get_aabb(p_multimesh);
}

void RendererMeshStorage::_multimesh_add_to_interpolation_lists(RID p_multimesh, MultiMeshInterpolator &r_mmi) {
	if (!r_mmi.on_interpolate_update_list) {
		r_mmi.on_interpolate_update_list = true;
		_interpolation_data.multimesh_interpolate_update_list.push_back(p_multimesh);
	}

	if (!r_mmi.on_transform_update_list) {
		r_mmi.on_transform_update_list = true;
		_interpolation_data.multimesh_transform_update_list_curr->push_back(p_multimesh);
	}
}

void RendererMeshStorage::InterpolationData::notify_free_multimesh(RID p_rid) {
	// If the instance was on any of the lists, remove.
	multimesh_interpolate_update_list.erase_multiple_unordered(p_rid);
	multimesh_transform_update_lists[0].erase_multiple_unordered(p_rid);
	multimesh_transform_update_lists[1].erase_multiple_unordered(p_rid);
}

void RendererMeshStorage::update_interpolation_tick(bool p_process) {
	// Detect any that were on the previous transform list that are no longer active,
	// we should remove them from the interpolate list.

	for (unsigned int n = 0; n < _interpolation_data.multimesh_transform_update_list_prev->size(); n++) {
		const RID &rid = (*_interpolation_data.multimesh_transform_update_list_prev)[n];

		bool active = true;

		// No longer active? (Either the instance deleted or no longer being transformed.)

		MultiMeshInterpolator *mmi = _multimesh_get_interpolator(rid);
		if (mmi && !mmi->on_transform_update_list) {
			active = false;
			mmi->on_interpolate_update_list = false;

			// Make sure the most recent transform is set...
			mmi->_data_interpolated = mmi->_data_curr; // TODO: Copy data rather than use Packed = function?

			// ... and that both prev and current are the same, just in case of any interpolations.
			mmi->_data_prev = mmi->_data_curr;
		}

		if (!mmi) {
			active = false;
		}

		if (!active) {
			_interpolation_data.multimesh_interpolate_update_list.erase(rid);
		}
	}

	if (p_process) {
		for (unsigned int i = 0; i < _interpolation_data.multimesh_transform_update_list_curr->size(); i++) {
			const RID &rid = (*_interpolation_data.multimesh_transform_update_list_curr)[i];

			MultiMeshInterpolator *mmi = _multimesh_get_interpolator(rid);
			if (mmi) {
				// Reset for next tick.
				mmi->on_transform_update_list = false;
				mmi->_data_prev = mmi->_data_curr;
			}
		}
	}

	// If any have left the transform list, remove from the interpolate list.

	// We maintain a mirror list for the transform updates, so we can detect when an instance
	// is no longer being transformed, and remove it from the interpolate list.
	SWAP(_interpolation_data.multimesh_transform_update_list_curr, _interpolation_data.multimesh_transform_update_list_prev);

	// Prepare for the next iteration.
	_interpolation_data.multimesh_transform_update_list_curr->clear();
}

void RendererMeshStorage::update_interpolation_frame(bool p_process) {
	if (p_process) {
		// Only need 32 bits for interpolation, don't use real_t.
		float f = Engine::get_singleton()->get_physics_interpolation_fraction();

		for (unsigned int c = 0; c < _interpolation_data.multimesh_interpolate_update_list.size(); c++) {
			const RID &rid = _interpolation_data.multimesh_interpolate_update_list[c];

			// We could use the TransformInterpolator here to slerp transforms, but that might be too expensive,
			// so just using a Basis lerp for now.
			MultiMeshInterpolator *mmi = _multimesh_get_interpolator(rid);
			if (mmi) {
				// Make sure arrays are the correct size.
				DEV_ASSERT(mmi->_data_prev.size() == mmi->_data_curr.size());

				if (mmi->_data_interpolated.size() < mmi->_data_curr.size()) {
					mmi->_data_interpolated.resize(mmi->_data_curr.size());
				}
				DEV_ASSERT(mmi->_data_interpolated.size() >= mmi->_data_curr.size());

				DEV_ASSERT((mmi->_data_curr.size() % mmi->_stride) == 0);
				int num = mmi->_data_curr.size() / mmi->_stride;

				const float *pf_prev = mmi->_data_prev.ptr();
				const float *pf_curr = mmi->_data_curr.ptr();
				float *pf_int = mmi->_data_interpolated.ptrw();

				bool use_lerp = mmi->quality == 0;

				// Temporary transform (needed for swizzling).
				Transform3D tp, tc, tr; // (transform prev, curr and result)

				// Test for cache friendliness versus doing branchless.
				for (int n = 0; n < num; n++) {
					// Transform.
					if (use_lerp) {
						for (int i = 0; i < mmi->_vf_size_xform; i++) {
							pf_int[i] = Math::lerp(pf_prev[i], pf_curr[i], f);
						}
					} else {
						// Silly swizzling, this will slow things down.
						// No idea why it is using this format...
						// ... maybe due to the shader.
						tp.basis.rows[0][0] = pf_prev[0];
						tp.basis.rows[0][1] = pf_prev[1];
						tp.basis.rows[0][2] = pf_prev[2];
						tp.basis.rows[1][0] = pf_prev[4];
						tp.basis.rows[1][1] = pf_prev[5];
						tp.basis.rows[1][2] = pf_prev[6];
						tp.basis.rows[2][0] = pf_prev[8];
						tp.basis.rows[2][1] = pf_prev[9];
						tp.basis.rows[2][2] = pf_prev[10];
						tp.origin.x = pf_prev[3];
						tp.origin.y = pf_prev[7];
						tp.origin.z = pf_prev[11];

						tc.basis.rows[0][0] = pf_curr[0];
						tc.basis.rows[0][1] = pf_curr[1];
						tc.basis.rows[0][2] = pf_curr[2];
						tc.basis.rows[1][0] = pf_curr[4];
						tc.basis.rows[1][1] = pf_curr[5];
						tc.basis.rows[1][2] = pf_curr[6];
						tc.basis.rows[2][0] = pf_curr[8];
						tc.basis.rows[2][1] = pf_curr[9];
						tc.basis.rows[2][2] = pf_curr[10];
						tc.origin.x = pf_curr[3];
						tc.origin.y = pf_curr[7];
						tc.origin.z = pf_curr[11];

						TransformInterpolator::interpolate_transform_3d(tp, tc, tr, f);

						pf_int[0] = tr.basis.rows[0][0];
						pf_int[1] = tr.basis.rows[0][1];
						pf_int[2] = tr.basis.rows[0][2];
						pf_int[4] = tr.basis.rows[1][0];
						pf_int[5] = tr.basis.rows[1][1];
						pf_int[6] = tr.basis.rows[1][2];
						pf_int[8] = tr.basis.rows[2][0];
						pf_int[9] = tr.basis.rows[2][1];
						pf_int[10] = tr.basis.rows[2][2];
						pf_int[3] = tr.origin.x;
						pf_int[7] = tr.origin.y;
						pf_int[11] = tr.origin.z;
					}

					pf_prev += mmi->_vf_size_xform;
					pf_curr += mmi->_vf_size_xform;
					pf_int += mmi->_vf_size_xform;

					// Color.
					if (mmi->_vf_size_color == 4) {
						for (int i = 0; i < 4; i++) {
							pf_int[i] = Math::lerp(pf_prev[i], pf_curr[i], f);
						}

						pf_prev += 4;
						pf_curr += 4;
						pf_int += 4;
					}

					// Custom data.
					if (mmi->_vf_size_data == 4) {
						for (int i = 0; i < 4; i++) {
							pf_int[i] = Math::lerp(pf_prev[i], pf_curr[i], f);
						}

						pf_prev += 4;
						pf_curr += 4;
						pf_int += 4;
					}
				}

				_multimesh_set_buffer(rid, mmi->_data_interpolated);

				// TODO: Make sure AABBs are constantly up to date through the interpolation?
				// NYI.
			}
		}
	}
}
