/**************************************************************************/
/*  line_builder.cpp                                                      */
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

#include "line_builder.h"

#include "core/math/geometry_2d.h"

// Utility method.
static inline Vector2 interpolate(const Rect2 &r, const Vector2 &v) {
	return Vector2(
			Math::lerp(r.position.x, r.position.x + r.get_size().x, v.x),
			Math::lerp(r.position.y, r.position.y + r.get_size().y, v.y));
}

LineBuilder::LineBuilder() {
}

void LineBuilder::build() {
	// Need at least 2 points to draw a line, so clear the output and return.
	if (points.size() < 2) {
		vertices.clear();
		colors.clear();
		indices.clear();
		uvs.clear();
		return;
	}

	ERR_FAIL_COND(tile_aspect <= 0.f);

	const float hw = width / 2.f;
	const float hw_sq = hw * hw;
	const float sharp_limit_sq = sharp_limit * sharp_limit;
	const int point_count = points.size();
	const bool wrap_around = closed && point_count > 2;

	_interpolate_color = gradient != nullptr;
	const bool retrieve_curve = curve != nullptr;
	const bool distance_required = _interpolate_color || retrieve_curve ||
			texture_mode == Line2D::LINE_TEXTURE_TILE ||
			texture_mode == Line2D::LINE_TEXTURE_STRETCH;

	// Initial values

	Vector2 pos0 = points[0];
	Vector2 pos1 = points[1];
	Vector2 f0 = (pos1 - pos0).normalized();
	Vector2 u0 = f0.orthogonal();
	Vector2 pos_up0 = pos0;
	Vector2 pos_down0 = pos0;

	Color color0;
	Color color1;

	float current_distance0 = 0.f;
	float current_distance1 = 0.f;
	float total_distance = 0.f;

	float width_factor = 1.f;
	float modified_hw = hw;
	if (retrieve_curve) {
		width_factor = curve->sample_baked(0.f);
		modified_hw = hw * width_factor;
	}

	if (distance_required) {
		// Calculate the total distance.
		for (int i = 1; i < point_count; ++i) {
			total_distance += points[i].distance_to(points[i - 1]);
		}
		if (wrap_around) {
			total_distance += points[point_count - 1].distance_to(pos0);
		} else {
			// Adjust the total distance.
			// The line's outer length may be a little higher due to the end caps.
			if (begin_cap_mode == Line2D::LINE_CAP_BOX || begin_cap_mode == Line2D::LINE_CAP_ROUND) {
				total_distance += modified_hw;
			}
			if (end_cap_mode == Line2D::LINE_CAP_BOX || end_cap_mode == Line2D::LINE_CAP_ROUND) {
				if (retrieve_curve) {
					total_distance += hw * curve->sample_baked(1.f);
				} else {
					total_distance += hw;
				}
			}
		}
	}

	if (_interpolate_color) {
		color0 = gradient->get_color(0);
	} else {
		colors.push_back(default_color);
	}

	float uvx0 = 0.f;
	float uvx1 = 0.f;

	pos_up0 += u0 * modified_hw;
	pos_down0 -= u0 * modified_hw;

	// Begin cap
	if (!wrap_around) {
		if (begin_cap_mode == Line2D::LINE_CAP_BOX) {
			// Push back first vertices a little bit.
			pos_up0 -= f0 * modified_hw;
			pos_down0 -= f0 * modified_hw;

			current_distance0 += modified_hw;
			current_distance1 = current_distance0;
		} else if (begin_cap_mode == Line2D::LINE_CAP_ROUND) {
			if (texture_mode == Line2D::LINE_TEXTURE_TILE) {
				uvx0 = width_factor * 0.5f / tile_aspect;
			} else if (texture_mode == Line2D::LINE_TEXTURE_STRETCH) {
				uvx0 = width * width_factor / total_distance;
			}
			new_arc(pos0, pos_up0 - pos0, -Math_PI, color0, Rect2(0.f, 0.f, uvx0 * 2, 1.f));
			current_distance0 += modified_hw;
			current_distance1 = current_distance0;
		}
		strip_begin(pos_up0, pos_down0, color0, uvx0);
	}

	/*
	 *  pos_up0 ------------- pos_up1 --------------------
	 *     |                     |
	 *   pos0 - - - - - - - - - pos1 - - - - - - - - - pos2
	 *     |                     |
	 * pos_down0 ------------ pos_down1 ------------------
	 *
	 *   i-1                     i                      i+1
	 */

	// http://labs.hyperandroid.com/tag/opengl-lines
	// (not the same implementation but visuals help a lot)

	// If the polyline wraps around, then draw two more segments with joints:
	// The last one, which should normally end with an end cap, and the one that matches the end and the beginning.
	int segments_count = wrap_around ? point_count : (point_count - 2);
	// The wraparound case starts with a "fake walk" from the end of the polyline
	// to its beginning, so that its first joint is correct, without drawing anything.
	int first_point = wrap_around ? -1 : 1;

	// If the line wraps around, these variables will be used for the final segment.
	Vector2 first_pos_up, first_pos_down;
	bool is_first_joint_sharp = false;

	// For each additional segment
	for (int i = first_point; i <= segments_count; ++i) {
		pos1 = points[(i == -1) ? point_count - 1 : i % point_count]; // First point.
		Vector2 pos2 = points[(i + 1) % point_count]; // Second point.

		Vector2 f1 = (pos2 - pos1).normalized();
		Vector2 u1 = f1.orthogonal();

		// Determine joint orientation.
		float dp = u0.dot(f1);
		const Orientation orientation = (dp > 0.f ? UP : DOWN);

		if (distance_required && i >= 1) {
			current_distance1 += pos0.distance_to(pos1);
		}
		if (_interpolate_color) {
			color1 = gradient->get_color_at_offset(current_distance1 / total_distance);
		}
		if (retrieve_curve) {
			width_factor = curve->sample_baked(current_distance1 / total_distance);
			modified_hw = hw * width_factor;
		}

		Vector2 inner_normal0 = u0 * modified_hw;
		Vector2 inner_normal1 = u1 * modified_hw;
		if (orientation == DOWN) {
			inner_normal0 = -inner_normal0;
			inner_normal1 = -inner_normal1;
		}

		/*
		 * ---------------------------
		 *                        /
		 * 0                     /    1
		 *                      /          /
		 * --------------------x------    /
		 *                    /          /    (here shown with orientation == DOWN)
		 *                   /          /
		 *                  /          /
		 *                 /          /
		 *                     2     /
		 *                          /
		 */

		// Find inner intersection at the joint.
		Vector2 corner_pos_in, corner_pos_out;
		bool is_intersecting = Geometry2D::segment_intersects_segment(
				pos0 + inner_normal0, pos1 + inner_normal0,
				pos1 + inner_normal1, pos2 + inner_normal1,
				&corner_pos_in);

		if (is_intersecting) {
			// Inner parts of the segments intersect.
			corner_pos_out = 2.f * pos1 - corner_pos_in;
		} else {
			// No intersection, segments are too sharp or they overlap.
			corner_pos_in = pos1 + inner_normal0;
			corner_pos_out = pos1 - inner_normal0;
		}

		Vector2 corner_pos_up, corner_pos_down;
		if (orientation == UP) {
			corner_pos_up = corner_pos_in;
			corner_pos_down = corner_pos_out;
		} else {
			corner_pos_up = corner_pos_out;
			corner_pos_down = corner_pos_in;
		}

		Line2D::LineJointMode current_joint_mode = joint_mode;

		Vector2 pos_up1, pos_down1;
		if (is_intersecting) {
			// Fallback on bevel if sharp angle is too high (because it would produce very long miters).
			float width_factor_sq = width_factor * width_factor;
			if (current_joint_mode == Line2D::LINE_JOINT_SHARP && corner_pos_out.distance_squared_to(pos1) / (hw_sq * width_factor_sq) > sharp_limit_sq) {
				current_joint_mode = Line2D::LINE_JOINT_BEVEL;
			}
			if (current_joint_mode == Line2D::LINE_JOINT_SHARP) {
				// In this case, we won't create joint geometry,
				// The previous and next line quads will directly share an edge.
				pos_up1 = corner_pos_up;
				pos_down1 = corner_pos_down;
			} else {
				// Bevel or round
				if (orientation == UP) {
					pos_up1 = corner_pos_up;
					pos_down1 = pos1 - u0 * modified_hw;
				} else {
					pos_up1 = pos1 + u0 * modified_hw;
					pos_down1 = corner_pos_down;
				}
			}
		} else {
			// No intersection: fallback
			if (current_joint_mode == Line2D::LINE_JOINT_SHARP) {
				// There is no fallback implementation for LINE_JOINT_SHARP so switch to the LINE_JOINT_BEVEL.
				current_joint_mode = Line2D::LINE_JOINT_BEVEL;
			}
			pos_up1 = corner_pos_up;
			pos_down1 = corner_pos_down;
		}

		// Triangles are clockwise.
		if (texture_mode == Line2D::LINE_TEXTURE_TILE) {
			uvx1 = current_distance1 / (width * tile_aspect);
		} else if (texture_mode == Line2D::LINE_TEXTURE_STRETCH) {
			uvx1 = current_distance1 / total_distance;
		}

		// Swap vars for use in the next line.
		color0 = color1;
		u0 = u1;
		f0 = f1;
		pos0 = pos1;
		if (is_intersecting) {
			if (current_joint_mode == Line2D::LINE_JOINT_SHARP) {
				pos_up0 = pos_up1;
				pos_down0 = pos_down1;
			} else {
				if (orientation == UP) {
					pos_up0 = corner_pos_up;
					pos_down0 = pos1 - u1 * modified_hw;
				} else {
					pos_up0 = pos1 + u1 * modified_hw;
					pos_down0 = corner_pos_down;
				}
			}
		} else {
			pos_up0 = pos1 + u1 * modified_hw;
			pos_down0 = pos1 - u1 * modified_hw;
		}

		// End the "fake pass" in the closed line case before the drawing subroutine.
		if (i == -1) {
			continue;
		}

		// For wrap-around polylines, store some kind of start positions of the first joint for the final connection.
		if (wrap_around && i == 0) {
			Vector2 first_pos_center = (pos_up1 + pos_down1) / 2;
			float lerp_factor = 1.0 / width_factor;
			first_pos_up = first_pos_center.lerp(pos_up1, lerp_factor);
			first_pos_down = first_pos_center.lerp(pos_down1, lerp_factor);
			is_first_joint_sharp = current_joint_mode == Line2D::LINE_JOINT_SHARP;
		}

		// Add current line body quad.
		if (wrap_around && retrieve_curve && !is_first_joint_sharp && i == segments_count) {
			// If the width curve is not seamless, we might need to fetch the line's start points to use them for the final connection.
			Vector2 first_pos_center = (first_pos_up + first_pos_down) / 2;
			strip_add_quad(first_pos_center.lerp(first_pos_up, width_factor), first_pos_center.lerp(first_pos_down, width_factor), color1, uvx1);
			return;
		} else {
			strip_add_quad(pos_up1, pos_down1, color1, uvx1);
		}

		// From this point, bu0 and bd0 concern the next segment.
		// Add joint geometry.
		if (current_joint_mode != Line2D::LINE_JOINT_SHARP) {
			/* ________________ cbegin
			 *               / \
			 *              /   \
			 * ____________/_ _ _\ cend
			 *             |     |
			 *             |     |
			 *             |     |
			 */

			Vector2 cbegin, cend;
			if (orientation == UP) {
				cbegin = pos_down1;
				cend = pos_down0;
			} else {
				cbegin = pos_up1;
				cend = pos_up0;
			}

			if (current_joint_mode == Line2D::LINE_JOINT_BEVEL && !(wrap_around && i == segments_count)) {
				strip_add_tri(cend, orientation);
			} else if (current_joint_mode == Line2D::LINE_JOINT_ROUND && !(wrap_around && i == segments_count)) {
				Vector2 vbegin = cbegin - pos1;
				Vector2 vend = cend - pos1;
				// We want to use vbegin.angle_to(vend) below, which evaluates to
				// Math::atan2(vbegin.cross(vend), vbegin.dot(vend)) but we need to
				// calculate this ourselves as we need to check if the cross product
				// in that calculation ends up being -0.f and flip it if so, effectively
				// flipping the resulting angle_delta to not return -PI but +PI instead
				float cross_product = vbegin.cross(vend);
				float dot_product = vbegin.dot(vend);
				// Note that we're comparing against -0.f for clarity but 0.f would
				// match as well, therefore we need the explicit signbit check too.
				if (cross_product == -0.f && signbit(cross_product)) {
					cross_product = 0.f;
				}
				float angle_delta = Math::atan2(cross_product, dot_product);
				strip_add_arc(pos1, angle_delta, orientation);
			}

			if (!is_intersecting) {
				// In this case the joint is too corrupted to be reused,
				// start again the strip with fallback points
				strip_begin(pos_up0, pos_down0, color1, uvx1);
			}
		}
	}

	// Draw the last (or only) segment, with its end cap logic.
	if (!wrap_around) {
		pos1 = points[point_count - 1];

		if (distance_required) {
			current_distance1 += pos0.distance_to(pos1);
		}
		if (_interpolate_color) {
			color1 = gradient->get_color(gradient->get_point_count() - 1);
		}
		if (retrieve_curve) {
			width_factor = curve->sample_baked(1.f);
			modified_hw = hw * width_factor;
		}

		Vector2 pos_up1 = pos1 + u0 * modified_hw;
		Vector2 pos_down1 = pos1 - u0 * modified_hw;

		// Add extra distance for a box end cap.
		if (end_cap_mode == Line2D::LINE_CAP_BOX) {
			pos_up1 += f0 * modified_hw;
			pos_down1 += f0 * modified_hw;

			current_distance1 += modified_hw;
		}

		if (texture_mode == Line2D::LINE_TEXTURE_TILE) {
			uvx1 = current_distance1 / (width * tile_aspect);
		} else if (texture_mode == Line2D::LINE_TEXTURE_STRETCH) {
			uvx1 = current_distance1 / total_distance;
		}

		strip_add_quad(pos_up1, pos_down1, color1, uvx1);

		// Custom drawing for a round end cap.
		if (end_cap_mode == Line2D::LINE_CAP_ROUND) {
			// Note: color is not used in case we don't interpolate.
			Color color = _interpolate_color ? gradient->get_color(gradient->get_point_count() - 1) : Color(0, 0, 0);
			float dist = 0;
			if (texture_mode == Line2D::LINE_TEXTURE_TILE) {
				dist = width_factor / tile_aspect;
			} else if (texture_mode == Line2D::LINE_TEXTURE_STRETCH) {
				dist = width * width_factor / total_distance;
			}
			new_arc(pos1, pos_up1 - pos1, Math_PI, color, Rect2(uvx1 - 0.5f * dist, 0.f, dist, 1.f));
		}
	}
}

void LineBuilder::strip_begin(Vector2 up, Vector2 down, Color color, float uvx) {
	int vi = vertices.size();

	vertices.push_back(up);
	vertices.push_back(down);

	if (_interpolate_color) {
		colors.push_back(color);
		colors.push_back(color);
	}

	if (texture_mode != Line2D::LINE_TEXTURE_NONE) {
		uvs.push_back(Vector2(uvx, 0.f));
		uvs.push_back(Vector2(uvx, 1.f));
	}

	_last_index[UP] = vi;
	_last_index[DOWN] = vi + 1;
}

void LineBuilder::strip_add_quad(Vector2 up, Vector2 down, Color color, float uvx) {
	int vi = vertices.size();

	vertices.push_back(up);
	vertices.push_back(down);

	if (_interpolate_color) {
		colors.push_back(color);
		colors.push_back(color);
	}

	if (texture_mode != Line2D::LINE_TEXTURE_NONE) {
		uvs.push_back(Vector2(uvx, 0.f));
		uvs.push_back(Vector2(uvx, 1.f));
	}

	indices.push_back(_last_index[UP]);
	indices.push_back(vi + 1);
	indices.push_back(_last_index[DOWN]);
	indices.push_back(_last_index[UP]);
	indices.push_back(vi);
	indices.push_back(vi + 1);

	_last_index[UP] = vi;
	_last_index[DOWN] = vi + 1;
}

void LineBuilder::strip_add_tri(Vector2 up, Orientation orientation) {
	int vi = vertices.size();

	vertices.push_back(up);

	if (_interpolate_color) {
		colors.push_back(colors[colors.size() - 1]);
	}

	Orientation opposite_orientation = orientation == UP ? DOWN : UP;

	if (texture_mode != Line2D::LINE_TEXTURE_NONE) {
		// UVs are just one slice of the texture all along
		// (otherwise we can't share the bottom vertex)
		uvs.push_back(uvs[_last_index[opposite_orientation]]);
	}

	indices.push_back(_last_index[opposite_orientation]);
	indices.push_back(vi);
	indices.push_back(_last_index[orientation]);

	_last_index[opposite_orientation] = vi;
}

void LineBuilder::strip_add_arc(Vector2 center, float angle_delta, Orientation orientation) {
	// Take the two last vertices and extrude an arc made of triangles
	// that all share one of the initial vertices

	Orientation opposite_orientation = orientation == UP ? DOWN : UP;
	Vector2 vbegin = vertices[_last_index[opposite_orientation]] - center;
	float radius = vbegin.length();
	float angle_step = Math_PI / static_cast<float>(round_precision);
	float steps = Math::abs(angle_delta) / angle_step;

	if (angle_delta < 0.f) {
		angle_step = -angle_step;
	}

	float t = Vector2(1, 0).angle_to(vbegin);
	float end_angle = t + angle_delta;
	Vector2 rpos(0, 0);

	// Arc vertices
	for (int ti = 0; ti < steps; ++ti, t += angle_step) {
		rpos = center + Vector2(Math::cos(t), Math::sin(t)) * radius;
		strip_add_tri(rpos, orientation);
	}

	// Last arc vertex
	rpos = center + Vector2(Math::cos(end_angle), Math::sin(end_angle)) * radius;
	strip_add_tri(rpos, orientation);
}

void LineBuilder::new_arc(Vector2 center, Vector2 vbegin, float angle_delta, Color color, Rect2 uv_rect) {
	// Make a standalone arc that doesn't use existing vertices,
	// with undistorted UVs from within a square section

	float radius = vbegin.length();
	float angle_step = Math_PI / static_cast<float>(round_precision);
	float steps = Math::abs(angle_delta) / angle_step;

	if (angle_delta < 0.f) {
		angle_step = -angle_step;
	}

	float t = Vector2(1, 0).angle_to(vbegin);
	float end_angle = t + angle_delta;
	Vector2 rpos(0, 0);
	float tt_begin = -Math_PI / 2.0f;
	float tt = tt_begin;

	// Center vertice
	int vi = vertices.size();
	vertices.push_back(center);
	if (_interpolate_color) {
		colors.push_back(color);
	}
	if (texture_mode != Line2D::LINE_TEXTURE_NONE) {
		uvs.push_back(interpolate(uv_rect, Vector2(0.5f, 0.5f)));
	}

	// Arc vertices
	for (int ti = 0; ti < steps; ++ti, t += angle_step) {
		Vector2 sc = Vector2(Math::cos(t), Math::sin(t));
		rpos = center + sc * radius;

		vertices.push_back(rpos);
		if (_interpolate_color) {
			colors.push_back(color);
		}
		if (texture_mode != Line2D::LINE_TEXTURE_NONE) {
			Vector2 tsc = Vector2(Math::cos(tt), Math::sin(tt));
			uvs.push_back(interpolate(uv_rect, 0.5f * (tsc + Vector2(1.f, 1.f))));
			tt += angle_step;
		}
	}

	// Last arc vertex
	Vector2 sc = Vector2(Math::cos(end_angle), Math::sin(end_angle));
	rpos = center + sc * radius;
	vertices.push_back(rpos);
	if (_interpolate_color) {
		colors.push_back(color);
	}
	if (texture_mode != Line2D::LINE_TEXTURE_NONE) {
		tt = tt_begin + angle_delta;
		Vector2 tsc = Vector2(Math::cos(tt), Math::sin(tt));
		uvs.push_back(interpolate(uv_rect, 0.5f * (tsc + Vector2(1.f, 1.f))));
	}

	// Make up triangles
	int vi0 = vi;
	for (int ti = 0; ti < steps; ++ti) {
		indices.push_back(vi0);
		indices.push_back(++vi);
		indices.push_back(vi + 1);
	}
}
