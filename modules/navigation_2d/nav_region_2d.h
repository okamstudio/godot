/**************************************************************************/
/*  nav_region_2d.h                                                       */
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

#ifndef NAV_REGION_2D_H
#define NAV_REGION_2D_H

#include "nav_base_2d.h"
#include "nav_utils_2d.h"

#include "core/os/rw_lock.h"
#include "scene/resources/2d/navigation_polygon.h"

struct NavRegionIteration2D;

class NavRegion2D : public NavBase2D {
	RWLock region_rwlock;

	NavMap2D *map = nullptr;
	Transform2D transform;
	bool enabled = true;

	bool use_edge_connections = true;

	bool polygons_dirty = true;

	LocalVector<nav_2d::Polygon> navmesh_polygons;

	real_t surface_area = 0.0;
	Rect2 bounds;

	RWLock navmesh_rwlock;
	Vector<Vector2> pending_navmesh_vertices;
	Vector<Vector<int>> pending_navmesh_polygons;

	SelfList<NavRegion2D> sync_dirty_request_list_element;

public:
	NavRegion2D();
	~NavRegion2D();

	void scratch_polygons() {
		polygons_dirty = true;
	}

	void set_enabled(bool p_enabled);
	bool get_enabled() const { return enabled; }

	void set_map(NavMap2D *p_map);
	NavMap2D *get_map() const {
		return map;
	}

	void set_use_edge_connections(bool p_enabled);
	bool get_use_edge_connections() const {
		return use_edge_connections;
	}

	void set_transform(const Transform2D &p_transform);
	const Transform2D &get_transform() const {
		return transform;
	}

	void set_navigation_polygon(Ref<NavigationPolygon> p_navigation_polygon);

	LocalVector<nav_2d::Polygon> const &get_polygons() const {
		return navmesh_polygons;
	}

	nav_2d::ClosestPointQueryResult get_closest_point_info(const Vector2 &p_point) const;
	Vector2 get_random_point(uint32_t p_navigation_layers, bool p_uniformly) const;

	real_t get_surface_area() const { return surface_area; }
	Rect2 get_bounds() const { return bounds; }

	bool sync();
	void request_sync();
	void cancel_sync_request();

	void get_iteration_update(NavRegionIteration2D &r_iteration);

private:
	void update_polygons();
};

#endif // NAV_REGION_2D_H
