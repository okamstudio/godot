/**************************************************************************/
/*  csg_capsule_2d.cpp                                                    */
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

#include "csg_capsule_2d.h"

#include "core/math/geometry_2d.h"
#include "scene/resources/world_2d.h"
#include "servers/physics_server_2d.h"

#include "thirdparty/misc/polypartition.h"

#ifdef TOOLS_ENABLED
Rect2 CSGCapsule2D::_edit_get_rect() const {
	Rect2 rect = Rect2(Vector2(-radius, -height * 0.5 - radius), Vector2(radius, height * 0.5 + radius) * 2.0);
	rect.position -= rect.size * 0.3;
	rect.size += rect.size * 0.6;
	return rect;
}

bool CSGCapsule2D::_edit_use_rect() const {
	return true;
}

bool CSGCapsule2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	return Geometry2D::is_point_in_polygon(p_point, _get_vertices());
}
#endif // TOOLS_ENABLED

CSGBrush2D *CSGCapsule2D::_build_brush() {
	CSGBrush2D *new_brush = memnew(CSGBrush2D);

	LocalVector<CSGBrush2D::Outline> &outlines = new_brush->outlines;
	outlines.resize(1);
	CSGBrush2D::Outline &outline = outlines[0];

	const Vector<Vector2> &cached_vertices = _get_vertices();

	LocalVector<Vector2> &vertices = outline.vertices;

	vertices.resize(cached_vertices.size() + 1);

	for (int i = 0; i < cached_vertices.size(); i++) {
		vertices[i] = cached_vertices[i];
	}
	vertices[cached_vertices.size()] = cached_vertices[0];

	brush_outlines.resize(1);
	brush_outlines[0] = vertices;

	new_brush->build_from_outlines(outlines);

	return new_brush;
}

Vector<Vector2> CSGCapsule2D::_get_vertices() const {
	if (vertices_cache_dirty) {
		vertices_cache_dirty = false;

		vertices_cache.resize(radial_segments * 4 + 2);
		Vector2 *vertices_cache_ptrw = vertices_cache.ptrw();
		int vertices_index = 0;

		const real_t turn_step = Math_TAU / float(radial_segments * 4);
		for (int i = 0; i < radial_segments * 4; i++) {
			Vector2 ofs = Vector2(0, (i > radial_segments && i <= radial_segments * 3) ? -height * 0.5 + radius : height * 0.5 - radius);

			vertices_cache_ptrw[vertices_index++] = Vector2(Math::sin(i * turn_step), Math::cos(i * turn_step)) * radius + ofs;
			if (i == radial_segments || i == radial_segments * 3) {
				vertices_cache_ptrw[vertices_index++] = Vector2(Math::sin(i * turn_step), Math::cos(i * turn_step)) * radius - ofs;
			}
		}
	}

	return vertices_cache;
}

void CSGCapsule2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &CSGCapsule2D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &CSGCapsule2D::get_radius);

	ClassDB::bind_method(D_METHOD("set_height", "height"), &CSGCapsule2D::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &CSGCapsule2D::get_height);

	ClassDB::bind_method(D_METHOD("set_radial_segments", "radial_segments"), &CSGCapsule2D::set_radial_segments);
	ClassDB::bind_method(D_METHOD("get_radial_segments"), &CSGCapsule2D::get_radial_segments);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,1000.0,0.001,or_greater,exp,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.001,1000.0,0.001,or_greater,exp,suffix:m"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radial_segments", PROPERTY_HINT_RANGE, "1,100,1"), "set_radial_segments", "get_radial_segments");
}

void CSGCapsule2D::set_radius(const float p_radius) {
	radius = p_radius;
	vertices_cache_dirty = true;
	_make_dirty();
}

float CSGCapsule2D::get_radius() const {
	return radius;
}

void CSGCapsule2D::set_height(const float p_height) {
	height = p_height;
	vertices_cache_dirty = true;
	_make_dirty();
}

float CSGCapsule2D::get_height() const {
	return height;
}

void CSGCapsule2D::set_radial_segments(const int p_segments) {
	radial_segments = p_segments > 0 ? p_segments : 1;
	vertices_cache_dirty = true;
	_make_dirty();
}

int CSGCapsule2D::get_radial_segments() const {
	return radial_segments;
}
