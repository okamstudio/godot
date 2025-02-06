/**************************************************************************/
/*  line_2d.cpp                                                           */
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

#include "line_2d.h"

#include "core/math/geometry_2d.h"
#include "line_builder.h"

Line2D::Line2D() {
}

#ifdef DEBUG_ENABLED
Rect2 Line2D::_edit_get_rect() const {
	if (_points.size() == 0) {
		return Rect2(0, 0, 0, 0);
	}
	Vector2 min = _points[0];
	Vector2 max = min;
	for (int i = 1; i < _points.size(); i++) {
		min = min.min(_points[i]);
		max = max.max(_points[i]);
	}
	return Rect2(min, max - min).grow(_width);
}

bool Line2D::_edit_use_rect() const {
	return true;
}

bool Line2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	const real_t d = _width / 2 + p_tolerance;
	const Vector2 *points = _points.ptr();
	for (int i = 0; i < _points.size() - 1; i++) {
		Vector2 p = Geometry2D::get_closest_point_to_segment(p_point, &points[i]);
		if (p_point.distance_to(p) <= d) {
			return true;
		}
	}
	if (_closed && _points.size() > 2) {
		const Vector2 closing_segment[2] = { points[0], points[_points.size() - 1] };
		Vector2 p = Geometry2D::get_closest_point_to_segment(p_point, closing_segment);
		if (p_point.distance_to(p) <= d) {
			return true;
		}
	}

	return false;
}
#endif

void Line2D::set_points(const Vector<Vector2> &p_points) {
	_points = p_points;
	queue_redraw();
}

void Line2D::set_closed(bool p_closed) {
	_closed = p_closed;
	queue_redraw();
}

bool Line2D::is_closed() const {
	return _closed;
}

void Line2D::set_width(float p_width) {
	if (p_width < 0.0) {
		p_width = 0.0;
	}
	_width = p_width;
	queue_redraw();
}

float Line2D::get_width() const {
	return _width;
}

void Line2D::set_curve(const Ref<Curve> &p_curve) {
	if (_curve.is_valid()) {
		_curve->disconnect_changed(callable_mp(this, &Line2D::_curve_changed));
	}

	_curve = p_curve;

	if (_curve.is_valid()) {
		_curve->connect_changed(callable_mp(this, &Line2D::_curve_changed));
	}

	queue_redraw();
}

Ref<Curve> Line2D::get_curve() const {
	return _curve;
}

void Line2D::set_min_curve_line_segments(int min_curve_line_segments) {
	_min_curve_line_segments = min_curve_line_segments;
	queue_redraw();
}

void Line2D::set_curve_offset(float curve_offset) {
	_curve_offset = curve_offset;
	queue_redraw();
}

Vector<Vector2> Line2D::get_points() const {
	return _points;
}

void Line2D::set_point_position(int i, Vector2 p_pos) {
	ERR_FAIL_INDEX(i, _points.size());
	_points.set(i, p_pos);
	queue_redraw();
}

Vector2 Line2D::get_point_position(int i) const {
	ERR_FAIL_INDEX_V(i, _points.size(), Vector2());
	return _points.get(i);
}

int Line2D::get_point_count() const {
	return _points.size();
}

void Line2D::clear_points() {
	int count = _points.size();
	if (count > 0) {
		_points.clear();
		queue_redraw();
	}
}

void Line2D::add_point(Vector2 p_pos, int p_atpos) {
	if (p_atpos < 0 || _points.size() < p_atpos) {
		_points.push_back(p_pos);
	} else {
		_points.insert(p_atpos, p_pos);
	}
	queue_redraw();
}

void Line2D::remove_point(int i) {
	_points.remove_at(i);
	queue_redraw();
}

void Line2D::set_default_color(Color p_color) {
	_default_color = p_color;
	queue_redraw();
}

Color Line2D::get_default_color() const {
	return _default_color;
}

void Line2D::set_gradient(const Ref<Gradient> &p_gradient) {
	if (_gradient.is_valid()) {
		_gradient->disconnect_changed(callable_mp(this, &Line2D::_gradient_changed));
	}

	_gradient = p_gradient;

	if (_gradient.is_valid()) {
		_gradient->connect_changed(callable_mp(this, &Line2D::_gradient_changed));
	}

	queue_redraw();
}

Ref<Gradient> Line2D::get_gradient() const {
	return _gradient;
}

void Line2D::set_texture(const Ref<Texture2D> &p_texture) {
	_texture = p_texture;
	queue_redraw();
}

Ref<Texture2D> Line2D::get_texture() const {
	return _texture;
}

void Line2D::set_texture_mode(const LineTextureMode p_mode) {
	_texture_mode = p_mode;
	queue_redraw();
}

Line2D::LineTextureMode Line2D::get_texture_mode() const {
	return _texture_mode;
}

void Line2D::set_joint_mode(LineJointMode p_mode) {
	_joint_mode = p_mode;
	queue_redraw();
}

Line2D::LineJointMode Line2D::get_joint_mode() const {
	return _joint_mode;
}

void Line2D::set_begin_cap_mode(LineCapMode p_mode) {
	_begin_cap_mode = p_mode;
	queue_redraw();
}

Line2D::LineCapMode Line2D::get_begin_cap_mode() const {
	return _begin_cap_mode;
}

void Line2D::set_end_cap_mode(LineCapMode p_mode) {
	_end_cap_mode = p_mode;
	queue_redraw();
}

Line2D::LineCapMode Line2D::get_end_cap_mode() const {
	return _end_cap_mode;
}

void Line2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			_draw();
		} break;
	}
}

void Line2D::set_sharp_limit(float p_limit) {
	if (p_limit < 0.f) {
		p_limit = 0.f;
	}
	_sharp_limit = p_limit;
	queue_redraw();
}

float Line2D::get_sharp_limit() const {
	return _sharp_limit;
}

void Line2D::set_round_precision(int p_precision) {
	_round_precision = MAX(1, p_precision);
	queue_redraw();
}

int Line2D::get_round_precision() const {
	return _round_precision;
}

void Line2D::set_antialiased(bool p_antialiased) {
	_antialiased = p_antialiased;
	queue_redraw();
}

bool Line2D::get_antialiased() const {
	return _antialiased;
}

void Line2D::_draw() {
	int len = _points.size();
	if (len <= 1 || _width == 0.f) {
		return;
	}

	// NOTE:
	// Sublines refer to the lines between points before curve segmentation.

	LineBuilder lb;
	LocalVector<Vector2> &generated_draw_points = lb.points;
	LocalVector<Vector2> gradient_inclusive_points;
	// We insert gradient points that correspond to a position on our normalized line (e.g. gradient point of 0.5 means we add a point in the at the half-way point of our total line).
	if (_gradient.is_valid()) {
		int gradient_point_count = _gradient->get_point_count();

		int subline_count = len - 1;
		LocalVector<float> subline_lengths;
		LocalVector<Vector2> sublines;
		subline_lengths.reserve(subline_count);
		sublines.reserve(len + _gradient->get_point_count() - 1);

		// Gather the total line length, and distances and directions for each subline
		float total_line_length = 0.0f;
		for (int i = 0; i < subline_count; ++i) {
			Vector2 subline_dir = _points[i + 1] - _points[i];
			float subline_length = subline_dir.length();
			total_line_length += subline_length;
			subline_lengths.push_back(subline_length);
			sublines.push_back(subline_dir);
		}

		// Reserve the right amount of memory so we're not constantly reallocating as we're pushing items in the back.
		gradient_inclusive_points.reserve(static_cast<unsigned int>(_points.size()) + static_cast<unsigned int>(gradient_point_count));

		int gradient_idx = 0;

		float cumulative_line_length = 0.0f;
		for (int line_idx = 0; line_idx < subline_count; ++line_idx) {
			// All current calculations are based on the current subline.
			int wrapped_line_idx = line_idx % static_cast<int>(_points.size());
			const Vector2 &curr_subline_start = _points[wrapped_line_idx];
			const Vector2 &curr_subline = sublines[wrapped_line_idx];
			const float &curr_subline_length = subline_lengths[wrapped_line_idx];

			// This is where our current subline points lay on our normalized line scale.
			float curr_subline_start_offset = cumulative_line_length / total_line_length;
			float curr_subline_end_offset = (cumulative_line_length + curr_subline_length) / total_line_length;

			gradient_inclusive_points.push_back(curr_subline_start);
			// We check to see if there's a gradient point that has an offset between our current subline.
			// For the curve offset, we only care about the X axis that ranges from 0 to 1, corresponding with our line normalization.
			while (gradient_idx < _gradient->get_point_count()) {
				float curr_gradient_point_offset = _gradient->get_offset(gradient_idx);
				float relative_gradient_point_offset = (curr_gradient_point_offset - curr_subline_start_offset) / (curr_subline_end_offset - curr_subline_start_offset);
				// Only add a point if the point offset is between our subline start and end.
				if (relative_gradient_point_offset >= 1.0f) {
					break;
				}
				if (relative_gradient_point_offset > 0.0f) {
					gradient_inclusive_points.push_back(curr_subline_start + curr_subline * relative_gradient_point_offset);
				}
				++gradient_idx;
			}
			cumulative_line_length += curr_subline_length;
		}
		gradient_inclusive_points.push_back(_points[subline_count]);
	} else {
		gradient_inclusive_points = _points;
	}

	int subline_count = static_cast<int>(gradient_inclusive_points.size()) - 1;
	int num_segments = _curve.is_valid() ? _min_curve_line_segments : -1;
	num_segments = MAX(subline_count, num_segments);
	generated_draw_points.reserve(num_segments);
	generated_draw_points.clear();

	// We have less segments than what's defined, so we just use the points.
	if (num_segments <= subline_count) {
		generated_draw_points = gradient_inclusive_points;
	} else {
		len = gradient_inclusive_points.size();

		// TODO: Cache the memory somewhere so we don't keep allocating memory
		int line_count = _closed ? len : len - 1;
		LocalVector<float> subline_lengths;
		subline_lengths.reserve(line_count);

		// Gather the total line length, and distances and directions for each segment
		float total_line_length = 0.0f;
		for (int line_idx = 0; line_idx < line_count; ++line_idx) {
			Vector2 line_dir = gradient_inclusive_points[(line_idx + 1) % len] - gradient_inclusive_points[line_idx];
			float subline_length = line_dir.length();
			total_line_length += subline_length;
			subline_lengths.push_back(subline_length);
		}

		float segment_length = total_line_length / MAX(1, static_cast<float>(num_segments));
		for (int line_idx = 0; line_idx < line_count; ++line_idx) {
			float subline_length = subline_lengths[line_idx];
			int subline_segments_count = static_cast<int>(Math::ceil(subline_length / segment_length));
			Vector2 subline_start = gradient_inclusive_points[line_idx];
			Vector2 subline_end = gradient_inclusive_points[(line_idx + 1) % len];
			generated_draw_points.push_back(gradient_inclusive_points[line_idx]);
			for (int l = 1; l < subline_segments_count; ++l) {
				float current_segment_relative_start = segment_length * static_cast<float>(l);
				generated_draw_points.push_back(subline_start + (subline_end - subline_start) * (current_segment_relative_start / subline_length));
			}
		}

		// We don't add the last point so that the line builder can properly cap the ends
		if (!_closed)
			generated_draw_points.push_back(gradient_inclusive_points[line_count % len]);
	}

	lb.closed = _closed;
	lb.default_color = _default_color;
	lb.gradient = *_gradient;
	lb.texture_mode = _texture_mode;
	lb.joint_mode = _joint_mode;
	lb.begin_cap_mode = _begin_cap_mode;
	lb.end_cap_mode = _end_cap_mode;
	lb.round_precision = _round_precision;
	lb.sharp_limit = _sharp_limit;
	lb.width = _width;
	lb.curve = *_curve;
	lb.curve_offset = _curve_offset;

	RID texture_rid;
	if (_texture.is_valid()) {
		texture_rid = _texture->get_rid();

		lb.tile_aspect = _texture->get_size().aspect();
	}

	lb.build();

	RS::get_singleton()->canvas_item_add_triangle_array(
			get_canvas_item(),
			lb.indices,
			lb.vertices,
			lb.colors,
			lb.uvs, Vector<int>(), Vector<float>(),
			texture_rid);

	// DEBUG: Draw wireframe
	//	if (lb.indices.size() % 3 == 0) {
	//		Color col(0, 0, 0);
	//		for (int i = 0; i < lb.indices.size(); i += 3) {
	//			Vector2 a = lb.vertices[lb.indices[i]];
	//			Vector2 b = lb.vertices[lb.indices[i+1]];
	//			Vector2 c = lb.vertices[lb.indices[i+2]];
	//			draw_line(a, b, col);
	//			draw_line(b, c, col);
	//			draw_line(c, a, col);
	//		}
	//		for (int i = 0; i < lb.vertices.size(); ++i) {
	//			Vector2 p = lb.vertices[i];
	//			draw_rect(Rect2(p.x - 1, p.y - 1, 2, 2), Color(0, 0, 0, 0.5));
	//		}
	//	}
}

void Line2D::_gradient_changed() {
	queue_redraw();
}

void Line2D::_curve_changed() {
	queue_redraw();
}

// static
void Line2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_points", "points"), &Line2D::set_points);
	ClassDB::bind_method(D_METHOD("get_points"), &Line2D::get_points);

	ClassDB::bind_method(D_METHOD("set_point_position", "index", "position"), &Line2D::set_point_position);
	ClassDB::bind_method(D_METHOD("get_point_position", "index"), &Line2D::get_point_position);

	ClassDB::bind_method(D_METHOD("get_point_count"), &Line2D::get_point_count);

	ClassDB::bind_method(D_METHOD("add_point", "position", "index"), &Line2D::add_point, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("remove_point", "index"), &Line2D::remove_point);

	ClassDB::bind_method(D_METHOD("clear_points"), &Line2D::clear_points);

	ClassDB::bind_method(D_METHOD("set_closed", "closed"), &Line2D::set_closed);
	ClassDB::bind_method(D_METHOD("is_closed"), &Line2D::is_closed);

	ClassDB::bind_method(D_METHOD("set_width", "width"), &Line2D::set_width);
	ClassDB::bind_method(D_METHOD("get_width"), &Line2D::get_width);

	ClassDB::bind_method(D_METHOD("set_curve", "curve"), &Line2D::set_curve);
	ClassDB::bind_method(D_METHOD("get_curve"), &Line2D::get_curve);

	ClassDB::bind_method(D_METHOD("set_min_curve_line_segments", "curve_offset"), &Line2D::set_min_curve_line_segments);
	ClassDB::bind_method(D_METHOD("get_min_curve_line_segments"), &Line2D::get_min_curve_line_segments);

	ClassDB::bind_method(D_METHOD("set_curve_offset", "curve_offset"), &Line2D::set_curve_offset);
	ClassDB::bind_method(D_METHOD("get_curve_offset"), &Line2D::get_curve_offset);

	ClassDB::bind_method(D_METHOD("set_default_color", "color"), &Line2D::set_default_color);
	ClassDB::bind_method(D_METHOD("get_default_color"), &Line2D::get_default_color);

	ClassDB::bind_method(D_METHOD("set_gradient", "color"), &Line2D::set_gradient);
	ClassDB::bind_method(D_METHOD("get_gradient"), &Line2D::get_gradient);

	ClassDB::bind_method(D_METHOD("set_texture", "texture"), &Line2D::set_texture);
	ClassDB::bind_method(D_METHOD("get_texture"), &Line2D::get_texture);

	ClassDB::bind_method(D_METHOD("set_texture_mode", "mode"), &Line2D::set_texture_mode);
	ClassDB::bind_method(D_METHOD("get_texture_mode"), &Line2D::get_texture_mode);

	ClassDB::bind_method(D_METHOD("set_joint_mode", "mode"), &Line2D::set_joint_mode);
	ClassDB::bind_method(D_METHOD("get_joint_mode"), &Line2D::get_joint_mode);

	ClassDB::bind_method(D_METHOD("set_begin_cap_mode", "mode"), &Line2D::set_begin_cap_mode);
	ClassDB::bind_method(D_METHOD("get_begin_cap_mode"), &Line2D::get_begin_cap_mode);

	ClassDB::bind_method(D_METHOD("set_end_cap_mode", "mode"), &Line2D::set_end_cap_mode);
	ClassDB::bind_method(D_METHOD("get_end_cap_mode"), &Line2D::get_end_cap_mode);

	ClassDB::bind_method(D_METHOD("set_sharp_limit", "limit"), &Line2D::set_sharp_limit);
	ClassDB::bind_method(D_METHOD("get_sharp_limit"), &Line2D::get_sharp_limit);

	ClassDB::bind_method(D_METHOD("set_round_precision", "precision"), &Line2D::set_round_precision);
	ClassDB::bind_method(D_METHOD("get_round_precision"), &Line2D::get_round_precision);

	ClassDB::bind_method(D_METHOD("set_antialiased", "antialiased"), &Line2D::set_antialiased);
	ClassDB::bind_method(D_METHOD("get_antialiased"), &Line2D::get_antialiased);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "points"), "set_points", "get_points");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "closed"), "set_closed", "is_closed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width", PROPERTY_HINT_NONE, "suffix:px"), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "width_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_curve", "get_curve");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "min_curve_line_segments", PROPERTY_HINT_RANGE, "1,64,1,or_greater"), "set_min_curve_line_segments", "get_min_curve_line_segments");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "curve_offset", PROPERTY_HINT_RANGE, "-1.0,1.0,0.01"), "set_curve_offset", "get_curve_offset");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "default_color"), "set_default_color", "get_default_color");
	ADD_GROUP("Fill", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "gradient", PROPERTY_HINT_RESOURCE_TYPE, "Gradient"), "set_gradient", "get_gradient");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_texture", "get_texture");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_mode", PROPERTY_HINT_ENUM, "None,Tile,Stretch"), "set_texture_mode", "get_texture_mode");
	ADD_GROUP("Capping", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "joint_mode", PROPERTY_HINT_ENUM, "Sharp,Bevel,Round"), "set_joint_mode", "get_joint_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "begin_cap_mode", PROPERTY_HINT_ENUM, "None,Box,Round"), "set_begin_cap_mode", "get_begin_cap_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "end_cap_mode", PROPERTY_HINT_ENUM, "None,Box,Round"), "set_end_cap_mode", "get_end_cap_mode");
	ADD_GROUP("Border", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sharp_limit"), "set_sharp_limit", "get_sharp_limit");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "round_precision", PROPERTY_HINT_RANGE, "1,32,1"), "set_round_precision", "get_round_precision");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "antialiased"), "set_antialiased", "get_antialiased");

	BIND_ENUM_CONSTANT(LINE_JOINT_SHARP);
	BIND_ENUM_CONSTANT(LINE_JOINT_BEVEL);
	BIND_ENUM_CONSTANT(LINE_JOINT_ROUND);

	BIND_ENUM_CONSTANT(LINE_CAP_NONE);
	BIND_ENUM_CONSTANT(LINE_CAP_BOX);
	BIND_ENUM_CONSTANT(LINE_CAP_ROUND);

	BIND_ENUM_CONSTANT(LINE_TEXTURE_NONE);
	BIND_ENUM_CONSTANT(LINE_TEXTURE_TILE);
	BIND_ENUM_CONSTANT(LINE_TEXTURE_STRETCH);
}
