/**************************************************************************/
/*  nav_map_builder_3d.h                                                  */
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

#ifndef NAV_MAP_BUILDER_3D_H
#define NAV_MAP_BUILDER_3D_H

#include "../nav_utils.h"

struct NavMapIterationBuild;

class NavMapBuilder3D {
	static void _build_step_gather_region_polygons(NavMapIterationBuild &r_build);
	static void _build_step_find_edge_connection_pairs(NavMapIterationBuild &r_build);
	static void _build_step_merge_edge_connection_pairs(NavMapIterationBuild &r_build);
	static void _build_step_edge_connection_margin_connections(NavMapIterationBuild &r_build);
	static void _build_step_navlink_connections(NavMapIterationBuild &r_build);
	static void _build_update_map_iteration(NavMapIterationBuild &r_build);

public:
	static gd::PointKey get_point_key(const Vector3 &p_pos, const Vector3 &p_cell_size);

	static void build_navmap_iteration(NavMapIterationBuild &r_build);
};

#endif // NAV_MAP_BUILDER_3D_H
