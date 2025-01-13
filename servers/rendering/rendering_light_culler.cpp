/**************************************************************************/
/*  rendering_light_culler.cpp                                            */
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

#include "rendering_light_culler.h"

#include "core/math/plane.h"
#include "core/math/projection.h"
#include "rendering_server_globals.h"

#ifdef RENDERING_LIGHT_CULLER_DEBUG_STRINGS
const char *RenderingLightCuller::Data::string_planes[] = {
	"NEAR",
	"FAR",
	"LEFT",
	"TOP",
	"RIGHT",
	"BOTTOM",
};
const char *RenderingLightCuller::Data::string_points[] = {
	"FAR_LEFT_TOP",
	"FAR_LEFT_BOTTOM",
	"FAR_RIGHT_TOP",
	"FAR_RIGHT_BOTTOM",
	"NEAR_LEFT_TOP",
	"NEAR_LEFT_BOTTOM",
	"NEAR_RIGHT_TOP",
	"NEAR_RIGHT_BOTTOM",
};

String RenderingLightCuller::Data::plane_bitfield_to_string(unsigned int BF) {
	String sz;

	for (int n = 0; n < 6; n++) {
		unsigned int bit = 1 << n;
		if (BF & bit) {
			sz += String(string_planes[n]) + ", ";
		}
	}

	return sz;
}
#endif

void RenderingLightCuller::prepare_directional_light(const RendererSceneCull::Instance *p_instance, int32_t p_directional_light_id) {
	//data.directional_light = p_instance;
	// Something is probably going wrong, we shouldn't have this many directional lights...
	ERR_FAIL_COND(p_directional_light_id > 512);
	DEV_ASSERT(p_directional_light_id >= 0);

	// First make sure we have enough directional lights to hold this one.
	if (p_directional_light_id >= (int32_t)data.directional_cull_planes.size()) {
		data.directional_cull_planes.resize(p_directional_light_id + 1);
	}

	_prepare_light(*p_instance, p_directional_light_id);
}

bool RenderingLightCuller::_prepare_light(const RendererSceneCull::Instance &p_instance, int32_t p_directional_light_id) {
	if (!data.is_active()) {
		return true;
	}

	LightSource lsource;
	switch (RSG::light_storage->light_get_type(p_instance.base)) {
		case RS::LIGHT_SPOT:
			lsource.type = LightSource::ST_SPOTLIGHT;
			lsource.angle = RSG::light_storage->light_get_param(p_instance.base, RS::LIGHT_PARAM_SPOT_ANGLE);
			lsource.range = RSG::light_storage->light_get_param(p_instance.base, RS::LIGHT_PARAM_RANGE);
			break;
		case RS::LIGHT_OMNI:
			lsource.type = LightSource::ST_OMNI;
			lsource.range = RSG::light_storage->light_get_param(p_instance.base, RS::LIGHT_PARAM_RANGE);
			break;
		case RS::LIGHT_DIRECTIONAL:
			lsource.type = LightSource::ST_DIRECTIONAL;
			// Could deal with a max directional shadow range here? NYI
			// LIGHT_PARAM_SHADOW_MAX_DISTANCE
			break;
	}

	lsource.pos = p_instance.transform.origin;
	lsource.dir = -p_instance.transform.basis.get_column(2);
	lsource.dir.normalize();

	bool visible;
	if (p_directional_light_id == -1) {
		visible = _add_light_camera_planes(data.regular_cull_planes, lsource);
	} else {
		visible = _add_light_camera_planes(data.directional_cull_planes[p_directional_light_id], lsource);
	}

	if (data.light_culling_active) {
		return visible;
	}
	return true;
}

bool RenderingLightCuller::cull_directional_light(const RendererSceneCull::InstanceBounds &p_bound, int32_t p_directional_light_id) {
	if (!data.is_active() || !is_caster_culling_active()) {
		return true;
	}

	ERR_FAIL_INDEX_V(p_directional_light_id, (int32_t)data.directional_cull_planes.size(), true);

	LightCullPlanes &cull_planes = data.directional_cull_planes[p_directional_light_id];

	Vector3 mins = Vector3(p_bound.bounds[0], p_bound.bounds[1], p_bound.bounds[2]);
	Vector3 maxs = Vector3(p_bound.bounds[3], p_bound.bounds[4], p_bound.bounds[5]);
	AABB bb(mins, maxs - mins);

	real_t r_min, r_max;
	for (int p = 0; p < cull_planes.num_cull_planes; p++) {
		bb.project_range_in_plane(cull_planes.cull_planes[p], r_min, r_max);
		if (r_min > 0.0f) {
#ifdef LIGHT_CULLER_DEBUG_DIRECTIONAL_LIGHT
			cull_planes.rejected_count++;
#endif

			return false;
		}
	}

	return true;
}

void RenderingLightCuller::cull_regular_light(PagedArray<RendererSceneCull::Instance *> &r_instance_shadow_cull_result) {
	if (!data.is_active() || !is_caster_culling_active()) {
		return;
	}

	// If the light is out of range, no need to check anything, just return 0 casters.
	// Ideally an out of range light should not even be drawn AT ALL (no shadow map, no PCF etc).
	if (data.out_of_range) {
		return;
	}

	// Shorter local alias.
	PagedArray<RendererSceneCull::Instance *> &list = r_instance_shadow_cull_result;

#ifdef LIGHT_CULLER_DEBUG_LOGGING
	uint32_t count_before = r_instance_shadow_cull_result.size();
#endif

	// Go through all the casters in the list (the list will hopefully shrink as we go).
	for (int n = 0; n < (int)list.size(); n++) {
		// World space aabb.
		const AABB &bb = list[n]->transformed_aabb;

#ifdef LIGHT_CULLER_DEBUG_LOGGING
		if (is_logging()) {
			print_line("bb : " + String(bb));
		}
#endif

		real_t r_min, r_max;
		bool show = true;

		for (int p = 0; p < data.regular_cull_planes.num_cull_planes; p++) {
			// As we only need r_min, could this be optimized?
			bb.project_range_in_plane(data.regular_cull_planes.cull_planes[p], r_min, r_max);

#ifdef LIGHT_CULLER_DEBUG_LOGGING
			if (is_logging()) {
				print_line("\tplane " + itos(p) + " : " + String(data.regular_cull_planes.cull_planes[p]) + " r_min " + String(Variant(r_min)) + " r_max " + String(Variant(r_max)));
			}
#endif

			if (r_min > 0.0f) {
				show = false;
				break;
			}
		}

		// Remove.
		if (!show) {
			list.remove_at_unordered(n);

			// Repeat this element next iteration of the loop as it has been removed and replaced by the last.
			n--;

#ifdef LIGHT_CULLER_DEBUG_REGULAR_LIGHT
			data.regular_rejected_count++;
#endif
		}
	}

#ifdef LIGHT_CULLER_DEBUG_LOGGING
	uint32_t removed = r_instance_shadow_cull_result.size() - count_before;
	if (removed) {
		if (((data.debug_count) % 60) == 0) {
			print_line("[" + itos(data.debug_count) + "] linear cull before " + itos(count_before) + " after " + itos(r_instance_shadow_cull_result.size()));
		}
	}
#endif
}

void RenderingLightCuller::LightCullPlanes::add_cull_plane(const Plane &p) {
	ERR_FAIL_COND(num_cull_planes >= MAX_CULL_PLANES);
	cull_planes[num_cull_planes++] = p;
}

// Directional lights are different to points, as the origin is infinitely in the distance, so the plane third
// points are derived differently.
bool RenderingLightCuller::add_light_camera_planes_directional(LightCullPlanes &r_cull_planes, const LightSource &p_light_source) {
	uint32_t lookup = 0;
	r_cull_planes.num_cull_planes = 0;

	// Directional light, we will use dot against the light direction to determine back facing planes.
	for (int n = 0; n < 6; n++) {
		float dot = data.frustum_planes[n].normal.dot(p_light_source.dir);
		if (dot > 0.0f) {
			lookup |= 1 << n;

			// Add backfacing camera frustum planes.
			r_cull_planes.add_cull_plane(data.frustum_planes[n]);
		}
	}

	ERR_FAIL_COND_V(lookup >= LUT_SIZE, true);

	// Deal with special case... if the light is INSIDE the view frustum (i.e. all planes face away)
	// then we will add the camera frustum planes to clip the light volume .. there is no need to
	// render shadow casters outside the frustum as shadows can never re-enter the frustum.

	// Should never happen with directional light?? This may be able to be removed.
	if (lookup == 63) {
		r_cull_planes.num_cull_planes = 0;
		for (int n = 0; n < data.frustum_planes.size(); n++) {
			r_cull_planes.add_cull_plane(data.frustum_planes[n]);
		}

		return true;
	}

// Each edge forms a plane.
#ifdef RENDERING_LIGHT_CULLER_CALCULATE_LUT
	const LocalVector<uint8_t> &entry = _calculated_LUT[lookup];

	// each edge forms a plane
	int n_edges = entry.size() - 1;
#else
	uint8_t *entry = &data.LUT_entries[lookup][0];
	int n_edges = data.LUT_entry_sizes[lookup] - 1;
#endif

	for (int e = 0; e < n_edges; e++) {
		int i0 = entry[e];
		int i1 = entry[e + 1];
		const Vector3 &pt0 = data.frustum_points[i0];
		const Vector3 &pt1 = data.frustum_points[i1];

		// Create a third point from the light direction.
		Vector3 pt2 = pt0 - p_light_source.dir;

		if (!_is_colinear_tri(pt0, pt1, pt2)) {
			// Create plane from 3 points.
			Plane p(pt0, pt1, pt2);
			r_cull_planes.add_cull_plane(p);
		}
	}

	// Last to 0 edge.
	if (n_edges) {
		int i0 = entry[n_edges]; // Last.
		int i1 = entry[0]; // First.

		const Vector3 &pt0 = data.frustum_points[i0];
		const Vector3 &pt1 = data.frustum_points[i1];

		// Create a third point from the light direction.
		Vector3 pt2 = pt0 - p_light_source.dir;

		if (!_is_colinear_tri(pt0, pt1, pt2)) {
			// Create plane from 3 points.
			Plane p(pt0, pt1, pt2);
			r_cull_planes.add_cull_plane(p);
		}
	}

#ifdef LIGHT_CULLER_DEBUG_LOGGING
	if (is_logging()) {
		print_line("lcam.pos is " + String(p_light_source.pos));
	}
#endif

	return true;
}

bool RenderingLightCuller::_add_light_camera_planes(LightCullPlanes &r_cull_planes, const LightSource &p_light_source) {
	if (!data.is_active()) {
		return true;
	}

	// We should have called prepare_camera before this.
	ERR_FAIL_COND_V(data.frustum_planes.size() != 6, true);

	switch (p_light_source.type) {
		case LightSource::ST_SPOTLIGHT:
		case LightSource::ST_OMNI:
			break;
		case LightSource::ST_DIRECTIONAL:
			return add_light_camera_planes_directional(r_cull_planes, p_light_source);
			break;
		default:
			return false; // not yet supported
			break;
	}

	// Start with 0 cull planes.
	r_cull_planes.num_cull_planes = 0;
	data.out_of_range = false;
	uint32_t lookup = 0;

	// Find which of the camera planes are facing away from the light.
	// We can also test for the situation where the light max range means it cannot
	// affect the camera frustum. This is absolutely worth doing because it is relatively
	// cheap, and if the entire light can be culled this can vastly improve performance
	// (much more than just culling casters).

	// POINT LIGHT (spotlight, omni)
	// Instead of using dot product to compare light direction to plane, we can simply
	// find out which side of the plane the camera is on. By definition this marks the point at which the plane
	// becomes invisible.

	// OMNIS
	if (p_light_source.type == LightSource::ST_OMNI) {
		for (int n = 0; n < 6; n++) {
			float dist = data.frustum_planes[n].distance_to(p_light_source.pos);
			if (dist < 0.0f) {
				lookup |= 1 << n;

				// Add backfacing camera frustum planes.
				r_cull_planes.add_cull_plane(data.frustum_planes[n]);
			} else {
				// Is the light out of range?
				// This is one of the tests. If the point source is more than range distance from a frustum plane, it can't
				// be seen.
				if (dist >= p_light_source.range) {
					// If the light is out of range, no need to do anything else, everything will be culled.
					data.out_of_range = true;
					return false;
				}
			}
		}
	} else {
		// SPOTLIGHTs, more complex to cull.
		Vector3 pos_end = p_light_source.pos + (p_light_source.dir * p_light_source.range);

		// This is the radius of the cone at distance 1.
		float radius_at_dist_one = Math::tan(Math::deg_to_rad(p_light_source.angle));

		// The worst case radius of the cone at the end point can be calculated
		// (the radius will scale linearly with length along the cone).
		float end_cone_radius = radius_at_dist_one * p_light_source.range;

		for (int n = 0; n < 6; n++) {
			float dist = data.frustum_planes[n].distance_to(p_light_source.pos);
			if (dist < 0.0f) {
				// Either the plane is backfacing or we are inside the frustum.
				lookup |= 1 << n;

				// Add backfacing camera frustum planes.
				r_cull_planes.add_cull_plane(data.frustum_planes[n]);
			} else {
				// The light is in front of the plane.

				// Is the light out of range?
				if (dist >= p_light_source.range) {
					data.out_of_range = true;
					return false;
				}

				// For a spotlight, we can use an extra test
				// at this point the cone start is in front of the plane...
				// If the cone end point is further than the maximum possible distance to the plane
				// we can guarantee that the cone does not cross the plane, and hence the cone
				// is outside the frustum.
				float dist_end = data.frustum_planes[n].distance_to(pos_end);

				if (dist_end >= end_cone_radius) {
					data.out_of_range = true;
					return false;
				}
			}
		}
	}

	// The lookup should be within the LUT, logic should prevent this.
	ERR_FAIL_COND_V(lookup >= LUT_SIZE, true);

	// Deal with special case... if the light is INSIDE the view frustum (i.e. all planes face away)
	// then we will add the camera frustum planes to clip the light volume .. there is no need to
	// render shadow casters outside the frustum as shadows can never re-enter the frustum.
	if (lookup == 63) {
		r_cull_planes.num_cull_planes = 0;
		for (int n = 0; n < data.frustum_planes.size(); n++) {
			r_cull_planes.add_cull_plane(data.frustum_planes[n]);
		}

		return true;
	}

	// Each edge forms a plane.
	uint8_t *entry = &data.LUT_entries[lookup][0];
	int n_edges = data.LUT_entry_sizes[lookup] - 1;

	const Vector3 &pt2 = p_light_source.pos;

	for (int e = 0; e < n_edges; e++) {
		int i0 = entry[e];
		int i1 = entry[e + 1];
		const Vector3 &pt0 = data.frustum_points[i0];
		const Vector3 &pt1 = data.frustum_points[i1];

		if (!_is_colinear_tri(pt0, pt1, pt2)) {
			// Create plane from 3 points.
			Plane p(pt0, pt1, pt2);
			r_cull_planes.add_cull_plane(p);
		}
	}

	// Last to 0 edge.
	if (n_edges) {
		int i0 = entry[n_edges]; // Last.
		int i1 = entry[0]; // First.

		const Vector3 &pt0 = data.frustum_points[i0];
		const Vector3 &pt1 = data.frustum_points[i1];

		if (!_is_colinear_tri(pt0, pt1, pt2)) {
			// Create plane from 3 points.
			Plane p(pt0, pt1, pt2);
			r_cull_planes.add_cull_plane(p);
		}
	}

#ifdef LIGHT_CULLER_DEBUG_LOGGING
	if (is_logging()) {
		print_line("lsource.pos is " + String(p_light_source.pos));
	}
#endif

	return true;
}

bool RenderingLightCuller::prepare_camera(const Transform3D &p_cam_transform, const Projection &p_cam_matrix) {
	data.debug_count++;
	if (data.debug_count >= 120) {
		data.debug_count = 0;
	}

	// For debug flash off and on.
#ifdef LIGHT_CULLER_DEBUG_FLASH
	if (!Engine::get_singleton()->is_editor_hint()) {
		int dc = Engine::get_singleton()->get_process_frames() / LIGHT_CULLER_DEBUG_FLASH_FREQUENCY;
		bool bnew_active;
		bnew_active = (dc % 2) == 0;

		if (bnew_active != data.light_culling_active) {
			data.light_culling_active = bnew_active;
			print_line("switching light culler " + String(Variant(data.light_culling_active)));
		}
	}
#endif

	if (!data.is_active()) {
		return false;
	}

	// Get the camera frustum planes in world space.
	data.frustum_planes = p_cam_matrix.get_projection_planes(p_cam_transform);
	DEV_CHECK_ONCE(data.frustum_planes.size() == 6);

	data.regular_cull_planes.num_cull_planes = 0;

#ifdef LIGHT_CULLER_DEBUG_DIRECTIONAL_LIGHT
	if (is_logging()) {
		for (uint32_t n = 0; n < data.directional_cull_planes.size(); n++) {
			print_line("LightCuller directional light " + itos(n) + " rejected " + itos(data.directional_cull_planes[n].rejected_count) + " instances.");
		}
	}
#endif
#ifdef LIGHT_CULLER_DEBUG_REGULAR_LIGHT
	if (data.regular_rejected_count) {
		print_line("LightCuller regular lights rejected " + itos(data.regular_rejected_count) + " instances.");
	}
	data.regular_rejected_count = 0;
#endif

	data.directional_cull_planes.resize(0);

#ifdef LIGHT_CULLER_DEBUG_LOGGING
	if (is_logging()) {
		for (int p = 0; p < 6; p++) {
			print_line("plane " + itos(p) + " : " + String(data.frustum_planes[p]));
		}
	}
#endif

	// We want to calculate the frustum corners in a specific order.
	const Projection::Planes intersections[8][3] = {
		{ Projection::PLANE_FAR, Projection::PLANE_LEFT, Projection::PLANE_TOP },
		{ Projection::PLANE_FAR, Projection::PLANE_LEFT, Projection::PLANE_BOTTOM },
		{ Projection::PLANE_FAR, Projection::PLANE_RIGHT, Projection::PLANE_TOP },
		{ Projection::PLANE_FAR, Projection::PLANE_RIGHT, Projection::PLANE_BOTTOM },
		{ Projection::PLANE_NEAR, Projection::PLANE_LEFT, Projection::PLANE_TOP },
		{ Projection::PLANE_NEAR, Projection::PLANE_LEFT, Projection::PLANE_BOTTOM },
		{ Projection::PLANE_NEAR, Projection::PLANE_RIGHT, Projection::PLANE_TOP },
		{ Projection::PLANE_NEAR, Projection::PLANE_RIGHT, Projection::PLANE_BOTTOM },
	};

	for (int i = 0; i < 8; i++) {
		// 3 plane intersection, gives us a point.
		bool res = data.frustum_planes[intersections[i][0]].intersect_3(data.frustum_planes[intersections[i][1]], data.frustum_planes[intersections[i][2]], &data.frustum_points[i]);

		// What happens with a zero frustum? NYI - deal with this.
		ERR_FAIL_COND_V(!res, false);

#ifdef LIGHT_CULLER_DEBUG_LOGGING
		if (is_logging()) {
			print_line("point " + itos(i) + " -> " + String(data.frustum_points[i]));
		}
#endif
	}

	return true;
}

RenderingLightCuller::RenderingLightCuller() {
	// Used to determine which frame to give debug output.
	data.debug_count = -1;

	// Uncomment below to switch off light culler in the editor.
	// data.caster_culling_active = Engine::get_singleton()->is_editor_hint() == false;

#ifdef RENDERING_LIGHT_CULLER_CALCULATE_LUT
	create_LUT();
#endif
}

/* clang-format off */
uint8_t RenderingLightCuller::Data::LUT_entry_sizes[LUT_SIZE] = {0, 4, 4, 0, 4, 6, 6, 8, 4, 6, 6, 8, 6, 6, 6, 6, 4, 6, 6, 8, 0, 8, 8, 0, 6, 6, 6, 6, 8, 6, 6, 4, 4, 6, 6, 8, 6, 6, 6, 6, 0, 8, 8, 0, 8, 6, 6, 4, 6, 6, 6, 6, 8, 6, 6, 4, 8, 6, 6, 4, 0, 4, 4, 0, };

// The lookup table used to determine which edges form the silhouette of the camera frustum,
// depending on the viewing angle (defined by which camera planes are backward facing).
uint8_t RenderingLightCuller::Data::LUT_entries[LUT_SIZE][8] = {
{0, 0, 0, 0, 0, 0, 0, 0, },
{7, 6, 4, 5, 0, 0, 0, 0, },
{1, 0, 2, 3, 0, 0, 0, 0, },
{0, 0, 0, 0, 0, 0, 0, 0, },
{1, 5, 4, 0, 0, 0, 0, 0, },
{1, 5, 7, 6, 4, 0, 0, 0, },
{4, 0, 2, 3, 1, 5, 0, 0, },
{5, 7, 6, 4, 0, 2, 3, 1, },
{0, 4, 6, 2, 0, 0, 0, 0, },
{0, 4, 5, 7, 6, 2, 0, 0, },
{6, 2, 3, 1, 0, 4, 0, 0, },
{2, 3, 1, 0, 4, 5, 7, 6, },
{0, 1, 5, 4, 6, 2, 0, 0, },
{0, 1, 5, 7, 6, 2, 0, 0, },
{6, 2, 3, 1, 5, 4, 0, 0, },
{2, 3, 1, 5, 7, 6, 0, 0, },
{2, 6, 7, 3, 0, 0, 0, 0, },
{2, 6, 4, 5, 7, 3, 0, 0, },
{7, 3, 1, 0, 2, 6, 0, 0, },
{3, 1, 0, 2, 6, 4, 5, 7, },
{0, 0, 0, 0, 0, 0, 0, 0, },
{2, 6, 4, 0, 1, 5, 7, 3, },
{7, 3, 1, 5, 4, 0, 2, 6, },
{0, 0, 0, 0, 0, 0, 0, 0, },
{2, 0, 4, 6, 7, 3, 0, 0, },
{2, 0, 4, 5, 7, 3, 0, 0, },
{7, 3, 1, 0, 4, 6, 0, 0, },
{3, 1, 0, 4, 5, 7, 0, 0, },
{2, 0, 1, 5, 4, 6, 7, 3, },
{2, 0, 1, 5, 7, 3, 0, 0, },
{7, 3, 1, 5, 4, 6, 0, 0, },
{3, 1, 5, 7, 0, 0, 0, 0, },
{3, 7, 5, 1, 0, 0, 0, 0, },
{3, 7, 6, 4, 5, 1, 0, 0, },
{5, 1, 0, 2, 3, 7, 0, 0, },
{7, 6, 4, 5, 1, 0, 2, 3, },
{3, 7, 5, 4, 0, 1, 0, 0, },
{3, 7, 6, 4, 0, 1, 0, 0, },
{5, 4, 0, 2, 3, 7, 0, 0, },
{7, 6, 4, 0, 2, 3, 0, 0, },
{0, 0, 0, 0, 0, 0, 0, 0, },
{3, 7, 6, 2, 0, 4, 5, 1, },
{5, 1, 0, 4, 6, 2, 3, 7, },
{0, 0, 0, 0, 0, 0, 0, 0, },
{3, 7, 5, 4, 6, 2, 0, 1, },
{3, 7, 6, 2, 0, 1, 0, 0, },
{5, 4, 6, 2, 3, 7, 0, 0, },
{7, 6, 2, 3, 0, 0, 0, 0, },
{3, 2, 6, 7, 5, 1, 0, 0, },
{3, 2, 6, 4, 5, 1, 0, 0, },
{5, 1, 0, 2, 6, 7, 0, 0, },
{1, 0, 2, 6, 4, 5, 0, 0, },
{3, 2, 6, 7, 5, 4, 0, 1, },
{3, 2, 6, 4, 0, 1, 0, 0, },
{5, 4, 0, 2, 6, 7, 0, 0, },
{6, 4, 0, 2, 0, 0, 0, 0, },
{3, 2, 0, 4, 6, 7, 5, 1, },
{3, 2, 0, 4, 5, 1, 0, 0, },
{5, 1, 0, 4, 6, 7, 0, 0, },
{1, 0, 4, 5, 0, 0, 0, 0, },
{0, 0, 0, 0, 0, 0, 0, 0, },
{3, 2, 0, 1, 0, 0, 0, 0, },
{5, 4, 6, 7, 0, 0, 0, 0, },
{0, 0, 0, 0, 0, 0, 0, 0, },
};

/* clang-format on */

#ifdef RENDERING_LIGHT_CULLER_CALCULATE_LUT

// See e.g. http://lspiroengine.com/?p=153 for reference.
// Principles are the same, but differences to the article:
// * Order of planes / points is different in Godot.
// * We use a lookup table at runtime.
void RenderingLightCuller::create_LUT() {
	// Each pair of planes that are opposite can have an edge.
	for (int plane_0 = 0; plane_0 < PLANE_TOTAL; plane_0++) {
		// For each neighbor of the plane.
		PlaneOrder neighs[4];
		get_neighbouring_planes((PlaneOrder)plane_0, neighs);

		for (int n = 0; n < 4; n++) {
			int plane_1 = neighs[n];

			// If these are opposite we need to add the 2 points they share.
			PointOrder pts[2];
			get_corners_of_planes((PlaneOrder)plane_0, (PlaneOrder)plane_1, pts);

			add_LUT(plane_0, plane_1, pts);
		}
	}

	for (uint32_t n = 0; n < LUT_SIZE; n++) {
		compact_LUT_entry(n);
	}

	debug_print_LUT();
	debug_print_LUT_as_table();
}

// we can pre-create the entire LUT and store it hard coded as a static inside the executable!
// it is only small in size, 64 entries with max 8 bytes per entry
void RenderingLightCuller::debug_print_LUT_as_table() {
	print_line("\nLIGHT VOLUME TABLE BEGIN\n");

	print_line("Copy this to LUT_entry_sizes:\n");
	String sz = "{";
	for (int n = 0; n < LUT_SIZE; n++) {
		const LocalVector<uint8_t> &entry = _calculated_LUT[n];

		sz += itos(entry.size()) + ", ";
	}
	sz += "}";
	print_line(sz);
	print_line("\nCopy this to LUT_entries:\n");

	for (int n = 0; n < LUT_SIZE; n++) {
		const LocalVector<uint8_t> &entry = _calculated_LUT[n];

		String sz = "{";

		// First is the number of points in the entry.
		int s = entry.size();

		for (int p = 0; p < 8; p++) {
			if (p < s) {
				sz += itos(entry[p]);
			} else {
				sz += "0"; // just a spacer
			}

			sz += ", ";
		}

		sz += "},";
		print_line(sz);
	}

	print_line("\nLIGHT VOLUME TABLE END\n");
}

void RenderingLightCuller::debug_print_LUT() {
	for (int n = 0; n < LUT_SIZE; n++) {
		String sz;
		sz = "LUT" + itos(n) + ":\t";

		sz += Data::plane_bitfield_to_string(n);
		print_line(sz);

		const LocalVector<uint8_t> &entry = _calculated_LUT[n];

		sz = "\t" + string_LUT_entry(entry);

		print_line(sz);
	}
}

String RenderingLightCuller::string_LUT_entry(const LocalVector<uint8_t> &p_entry) {
	String string;

	for (uint32_t n = 0; n < p_entry.size(); n++) {
		uint8_t val = p_entry[n];
		DEV_ASSERT(val < 8);
		const char *sz_point = Data::string_points[val];
		string += sz_point;
		string += ", ";
	}

	return string;
}

String RenderingLightCuller::debug_string_LUT_entry(const LocalVector<uint8_t> &p_entry, bool p_pair) {
	String string;

	for (uint32_t i = 0; i < p_entry.size(); i++) {
		int pt_order = p_entry[i];
		if (p_pair && ((i % 2) == 0)) {
			string += itos(pt_order) + "-";
		} else {
			string += itos(pt_order) + ", ";
		}
	}

	return string;
}

void RenderingLightCuller::add_LUT(int p_plane_0, int p_plane_1, PointOrder p_pts[2]) {
	// Note that some entries to the LUT will be "impossible" situations,
	// because it contains all combinations of plane flips.
	uint32_t bit0 = 1 << p_plane_0;
	uint32_t bit1 = 1 << p_plane_1;

	// All entries of the LUT that have plane 0 set and plane 1 not set.
	for (uint32_t n = 0; n < 64; n++) {
		// If bit0 not set...
		if (!(n & bit0)) {
			continue;
		}

		// If bit1 set...
		if (n & bit1) {
			continue;
		}

		// Meets criteria.
		add_LUT_entry(n, p_pts);
	}
}

void RenderingLightCuller::add_LUT_entry(uint32_t p_entry_id, PointOrder p_pts[2]) {
	DEV_ASSERT(p_entry_id < LUT_SIZE);
	LocalVector<uint8_t> &entry = _calculated_LUT[p_entry_id];

	entry.push_back(p_pts[0]);
	entry.push_back(p_pts[1]);
}

void RenderingLightCuller::compact_LUT_entry(uint32_t p_entry_id) {
	DEV_ASSERT(p_entry_id < LUT_SIZE);
	LocalVector<uint8_t> &entry = _calculated_LUT[p_entry_id];

	int num_pairs = entry.size() / 2;

	if (num_pairs == 0) {
		return;
	}

	LocalVector<uint8_t> temp;

	String string;
	string = "Compact LUT" + itos(p_entry_id) + ":\t";
	string += debug_string_LUT_entry(entry, true);
	print_line(string);

	// Add first pair.
	temp.push_back(entry[0]);
	temp.push_back(entry[1]);
	unsigned int BFpairs = 1;

	string = debug_string_LUT_entry(temp) + " -> ";
	print_line(string);

	// Attempt to add a pair each time.
	for (int done = 1; done < num_pairs; done++) {
		string = "done " + itos(done) + ": ";
		// Find a free pair.
		for (int p = 1; p < num_pairs; p++) {
			unsigned int bit = 1 << p;
			// Is it done already?
			if (BFpairs & bit) {
				continue;
			}

			// There must be at least 1 free pair.
			// Attempt to add.
			int a = entry[p * 2];
			int b = entry[(p * 2) + 1];

			string += "[" + itos(a) + "-" + itos(b) + "], ";

			int found_a = temp.find(a);
			int found_b = temp.find(b);

			// Special case, if they are both already in the list, no need to add
			// as this is a link from the tail to the head of the list.
			if ((found_a != -1) && (found_b != -1)) {
				string += "foundAB link " + itos(found_a) + ", " + itos(found_b) + " ";
				BFpairs |= bit;
				goto found;
			}

			// Find a.
			if (found_a != -1) {
				string += "foundA " + itos(found_a) + " ";
				temp.insert(found_a + 1, b);
				BFpairs |= bit;
				goto found;
			}

			// Find b.
			if (found_b != -1) {
				string += "foundB " + itos(found_b) + " ";
				temp.insert(found_b, a);
				BFpairs |= bit;
				goto found;
			}

		} // Check each pair for adding.

		// If we got here before finding a link, the whole set of planes is INVALID
		// e.g. far and near plane only, does not create continuous sillouhette of edges.
		print_line("\tINVALID");
		entry.clear();
		return;

	found:;
		print_line(string);
		string = "\ttemp now : " + debug_string_LUT_entry(temp);
		print_line(string);
	}

	// temp should now be the sorted entry .. delete the old one and replace by temp.
	entry.clear();
	entry = temp;
}

void RenderingLightCuller::get_neighbouring_planes(PlaneOrder p_plane, PlaneOrder r_neigh_planes[4]) const {
	// Table of neighboring planes to each.
	static const PlaneOrder neigh_table[PLANE_TOTAL][4] = {
		{ // LSM_FP_NEAR
				PLANE_LEFT,
				PLANE_RIGHT,
				PLANE_TOP,
				PLANE_BOTTOM },
		{ // LSM_FP_FAR
				PLANE_LEFT,
				PLANE_RIGHT,
				PLANE_TOP,
				PLANE_BOTTOM },
		{ // LSM_FP_LEFT
				PLANE_TOP,
				PLANE_BOTTOM,
				PLANE_NEAR,
				PLANE_FAR },
		{ // LSM_FP_TOP
				PLANE_LEFT,
				PLANE_RIGHT,
				PLANE_NEAR,
				PLANE_FAR },
		{ // LSM_FP_RIGHT
				PLANE_TOP,
				PLANE_BOTTOM,
				PLANE_NEAR,
				PLANE_FAR },
		{ // LSM_FP_BOTTOM
				PLANE_LEFT,
				PLANE_RIGHT,
				PLANE_NEAR,
				PLANE_FAR },
	};

	for (int n = 0; n < 4; n++) {
		r_neigh_planes[n] = neigh_table[p_plane][n];
	}
}

// Given two planes, returns the two points shared by those planes.  The points are always
// returned in counter-clockwise order, assuming the first input plane is facing towards
// the viewer.

// param p_plane_a The plane facing towards the viewer.
// param p_plane_b A plane neighboring p_plane_a.
// param r_points An array of exactly two elements to be filled with the indices of the points
// on return.

void RenderingLightCuller::get_corners_of_planes(PlaneOrder p_plane_a, PlaneOrder p_plane_b, PointOrder r_points[2]) const {
	static const PointOrder fp_table[PLANE_TOTAL][PLANE_TOTAL][2] = {
		{
				// LSM_FP_NEAR
				{
						// LSM_FP_NEAR
						PT_NEAR_LEFT_TOP, PT_NEAR_RIGHT_TOP, // Invalid combination.
				},
				{
						// LSM_FP_FAR
						PT_FAR_RIGHT_TOP, PT_FAR_LEFT_TOP, // Invalid combination.
				},
				{
						// LSM_FP_LEFT
						PT_NEAR_LEFT_TOP,
						PT_NEAR_LEFT_BOTTOM,
				},
				{
						// LSM_FP_TOP
						PT_NEAR_RIGHT_TOP,
						PT_NEAR_LEFT_TOP,
				},
				{
						// LSM_FP_RIGHT
						PT_NEAR_RIGHT_BOTTOM,
						PT_NEAR_RIGHT_TOP,
				},
				{
						// LSM_FP_BOTTOM
						PT_NEAR_LEFT_BOTTOM,
						PT_NEAR_RIGHT_BOTTOM,
				},
		},

		{
				// LSM_FP_FAR
				{
						// LSM_FP_NEAR
						PT_FAR_LEFT_TOP, PT_FAR_RIGHT_TOP, // Invalid combination.
				},
				{
						// LSM_FP_FAR
						PT_FAR_RIGHT_TOP, PT_FAR_LEFT_TOP, // Invalid combination.
				},
				{
						// LSM_FP_LEFT
						PT_FAR_LEFT_BOTTOM,
						PT_FAR_LEFT_TOP,
				},
				{
						// LSM_FP_TOP
						PT_FAR_LEFT_TOP,
						PT_FAR_RIGHT_TOP,
				},
				{
						// LSM_FP_RIGHT
						PT_FAR_RIGHT_TOP,
						PT_FAR_RIGHT_BOTTOM,
				},
				{
						// LSM_FP_BOTTOM
						PT_FAR_RIGHT_BOTTOM,
						PT_FAR_LEFT_BOTTOM,
				},
		},

		{
				// LSM_FP_LEFT
				{
						// LSM_FP_NEAR
						PT_NEAR_LEFT_BOTTOM,
						PT_NEAR_LEFT_TOP,
				},
				{
						// LSM_FP_FAR
						PT_FAR_LEFT_TOP,
						PT_FAR_LEFT_BOTTOM,
				},
				{
						// LSM_FP_LEFT
						PT_FAR_LEFT_BOTTOM, PT_FAR_LEFT_BOTTOM, // Invalid combination.
				},
				{
						// LSM_FP_TOP
						PT_NEAR_LEFT_TOP,
						PT_FAR_LEFT_TOP,
				},
				{
						// LSM_FP_RIGHT
						PT_FAR_LEFT_BOTTOM, PT_FAR_LEFT_BOTTOM, // Invalid combination.
				},
				{
						// LSM_FP_BOTTOM
						PT_FAR_LEFT_BOTTOM,
						PT_NEAR_LEFT_BOTTOM,
				},
		},

		{
				// LSM_FP_TOP
				{
						// LSM_FP_NEAR
						PT_NEAR_LEFT_TOP,
						PT_NEAR_RIGHT_TOP,
				},
				{
						// LSM_FP_FAR
						PT_FAR_RIGHT_TOP,
						PT_FAR_LEFT_TOP,
				},
				{
						// LSM_FP_LEFT
						PT_FAR_LEFT_TOP,
						PT_NEAR_LEFT_TOP,
				},
				{
						// LSM_FP_TOP
						PT_NEAR_LEFT_TOP, PT_FAR_LEFT_TOP, // Invalid combination.
				},
				{
						// LSM_FP_RIGHT
						PT_NEAR_RIGHT_TOP,
						PT_FAR_RIGHT_TOP,
				},
				{
						// LSM_FP_BOTTOM
						PT_FAR_LEFT_BOTTOM, PT_NEAR_LEFT_BOTTOM, // Invalid combination.
				},
		},

		{
				// LSM_FP_RIGHT
				{
						// LSM_FP_NEAR
						PT_NEAR_RIGHT_TOP,
						PT_NEAR_RIGHT_BOTTOM,
				},
				{
						// LSM_FP_FAR
						PT_FAR_RIGHT_BOTTOM,
						PT_FAR_RIGHT_TOP,
				},
				{
						// LSM_FP_LEFT
						PT_FAR_RIGHT_BOTTOM, PT_FAR_RIGHT_BOTTOM, // Invalid combination.
				},
				{
						// LSM_FP_TOP
						PT_FAR_RIGHT_TOP,
						PT_NEAR_RIGHT_TOP,
				},
				{
						// LSM_FP_RIGHT
						PT_FAR_RIGHT_BOTTOM, PT_FAR_RIGHT_BOTTOM, // Invalid combination.
				},
				{
						// LSM_FP_BOTTOM
						PT_NEAR_RIGHT_BOTTOM,
						PT_FAR_RIGHT_BOTTOM,
				},
		},

		// ==

		//	P_NEAR,
		//	P_FAR,
		//	P_LEFT,
		//	P_TOP,
		//	P_RIGHT,
		//	P_BOTTOM,

		{
				// LSM_FP_BOTTOM
				{
						// LSM_FP_NEAR
						PT_NEAR_RIGHT_BOTTOM,
						PT_NEAR_LEFT_BOTTOM,
				},
				{
						// LSM_FP_FAR
						PT_FAR_LEFT_BOTTOM,
						PT_FAR_RIGHT_BOTTOM,
				},
				{
						// LSM_FP_LEFT
						PT_NEAR_LEFT_BOTTOM,
						PT_FAR_LEFT_BOTTOM,
				},
				{
						// LSM_FP_TOP
						PT_NEAR_LEFT_BOTTOM, PT_FAR_LEFT_BOTTOM, // Invalid combination.
				},
				{
						// LSM_FP_RIGHT
						PT_FAR_RIGHT_BOTTOM,
						PT_NEAR_RIGHT_BOTTOM,
				},
				{
						// LSM_FP_BOTTOM
						PT_FAR_LEFT_BOTTOM, PT_NEAR_LEFT_BOTTOM, // Invalid combination.
				},
		},

		// ==

	};
	r_points[0] = fp_table[p_plane_a][p_plane_b][0];
	r_points[1] = fp_table[p_plane_a][p_plane_b][1];
}

#endif
