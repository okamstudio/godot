/**************************************************************************/
/*  curve.cpp                                                             */
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

#include "curve.h"

#include "core/math/math_funcs.h"

const char *Curve::SIGNAL_RANGE_CHANGED = "range_changed";
const char *Curve::SIGNAL_DOMAIN_CHANGED = "domain_changed";

Curve::Curve() {
}

void Curve::set_point_count(int p_count) {
	ERR_FAIL_COND(p_count < 0);
	int old_size = _points.size();
	if (old_size == p_count) {
		return;
	}

	if (old_size > p_count) {
		_points.resize(p_count);
		mark_dirty();
	} else {
		for (int i = p_count - old_size; i > 0; i--) {
			_add_point(Vector2());
		}
	}
	notify_property_list_changed();
}

int Curve::_add_point(Vector2 p_position, real_t p_left_tangent, real_t p_right_tangent, TangentMode p_left_mode, TangentMode p_right_mode) {
	// Add a point and preserve order.

	// Points must remain within the given value and domain ranges.
	p_position.x = CLAMP(p_position.x, _min_domain, _max_domain);
	p_position.y = CLAMP(p_position.y, _min_value, _max_value);

	int ret = -1;

	if (_points.size() == 0) {
		_points.push_back(Point(p_position, p_left_tangent, p_right_tangent, p_left_mode, p_right_mode));
		ret = 0;

	} else if (_points.size() == 1) {
		// TODO Is the `else` able to handle this block already?

		real_t diff = p_position.x - _points[0].position.x;

		if (diff > 0) {
			_points.push_back(Point(p_position, p_left_tangent, p_right_tangent, p_left_mode, p_right_mode));
			ret = 1;
		} else {
			_points.insert(0, Point(p_position, p_left_tangent, p_right_tangent, p_left_mode, p_right_mode));
			ret = 0;
		}

	} else {
		int i = get_index(p_position.x);

		if (i == 0 && p_position.x < _points[0].position.x) {
			// Insert before anything else.
			_points.insert(0, Point(p_position, p_left_tangent, p_right_tangent, p_left_mode, p_right_mode));
			ret = 0;
		} else {
			// Insert between i and i+1.
			++i;
			_points.insert(i, Point(p_position, p_left_tangent, p_right_tangent, p_left_mode, p_right_mode));
			ret = i;
		}
	}

	update_auto_tangents(ret);

	mark_dirty();

	return ret;
}

int Curve::add_point(Vector2 p_position, real_t p_left_tangent, real_t p_right_tangent, TangentMode p_left_mode, TangentMode p_right_mode) {
	int ret = _add_point(p_position, p_left_tangent, p_right_tangent, p_left_mode, p_right_mode);
	notify_property_list_changed();

	return ret;
}

// TODO: Needed to make the curve editor function properly until https://github.com/godotengine/godot/issues/76985 is fixed.
int Curve::add_point_no_update(Vector2 p_position, real_t p_left_tangent, real_t p_right_tangent, TangentMode p_left_mode, TangentMode p_right_mode) {
	int ret = _add_point(p_position, p_left_tangent, p_right_tangent, p_left_mode, p_right_mode);

	return ret;
}

int Curve::get_index(real_t p_offset) const {
	// Lower-bound float binary search.

	int imin = 0;
	int imax = _points.size() - 1;

	while (imax - imin > 1) {
		int m = (imin + imax) / 2;

		real_t a = _points[m].position.x;
		real_t b = _points[m + 1].position.x;

		if (a < p_offset && b < p_offset) {
			imin = m;
		} else if (a > p_offset) {
			imax = m;
		} else {
			return m;
		}
	}

	// Will happen if the offset is out of bounds.
	if (p_offset > _points[imax].position.x) {
		return imax;
	}
	return imin;
}

void Curve::clean_dupes() {
	bool dirty = false;

	for (int i = 1; i < _points.size(); ++i) {
		real_t diff = _points[i - 1].position.x - _points[i].position.x;
		if (diff <= CMP_EPSILON) {
			_points.remove_at(i);
			--i;
			dirty = true;
		}
	}

	if (dirty) {
		mark_dirty();
	}
}

void Curve::set_point_left_tangent(int p_index, real_t p_tangent) {
	ERR_FAIL_INDEX(p_index, _points.size());
	_points.write[p_index].left_tangent = p_tangent;
	_points.write[p_index].left_mode = TANGENT_FREE;
	mark_dirty();
}

void Curve::set_point_right_tangent(int p_index, real_t p_tangent) {
	ERR_FAIL_INDEX(p_index, _points.size());
	_points.write[p_index].right_tangent = p_tangent;
	_points.write[p_index].right_mode = TANGENT_FREE;
	mark_dirty();
}

void Curve::set_point_left_mode(int p_index, TangentMode p_mode) {
	ERR_FAIL_INDEX(p_index, _points.size());
	_points.write[p_index].left_mode = p_mode;
	if (p_index > 0) {
		if (p_mode == TANGENT_LINEAR) {
			Vector2 v = (_points[p_index - 1].position - _points[p_index].position).normalized();
			_points.write[p_index].left_tangent = v.y / v.x;
		}
	}
	mark_dirty();
}

void Curve::set_point_right_mode(int p_index, TangentMode p_mode) {
	ERR_FAIL_INDEX(p_index, _points.size());
	_points.write[p_index].right_mode = p_mode;
	if (p_index + 1 < _points.size()) {
		if (p_mode == TANGENT_LINEAR) {
			Vector2 v = (_points[p_index + 1].position - _points[p_index].position).normalized();
			_points.write[p_index].right_tangent = v.y / v.x;
		}
	}
	mark_dirty();
}

real_t Curve::get_point_left_tangent(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, _points.size(), 0);
	return _points[p_index].left_tangent;
}

real_t Curve::get_point_right_tangent(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, _points.size(), 0);
	return _points[p_index].right_tangent;
}

Curve::TangentMode Curve::get_point_left_mode(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, _points.size(), TANGENT_FREE);
	return _points[p_index].left_mode;
}

Curve::TangentMode Curve::get_point_right_mode(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, _points.size(), TANGENT_FREE);
	return _points[p_index].right_mode;
}

void Curve::_remove_point(int p_index) {
	ERR_FAIL_INDEX(p_index, _points.size());
	_points.remove_at(p_index);
	mark_dirty();
}

void Curve::remove_point(int p_index) {
	_remove_point(p_index);
	notify_property_list_changed();
}

void Curve::clear_points() {
	if (_points.is_empty()) {
		return;
	}

	_points.clear();
	mark_dirty();
	notify_property_list_changed();
}

void Curve::set_point_value(int p_index, real_t p_position) {
	ERR_FAIL_INDEX(p_index, _points.size());
	_points.write[p_index].position.y = p_position;
	update_auto_tangents(p_index);
	mark_dirty();
}

int Curve::set_point_offset(int p_index, real_t p_offset) {
	ERR_FAIL_INDEX_V(p_index, _points.size(), -1);
	Point p = _points[p_index];
	_remove_point(p_index);
	int i = _add_point(Vector2(p_offset, p.position.y));
	_points.write[i].left_tangent = p.left_tangent;
	_points.write[i].right_tangent = p.right_tangent;
	_points.write[i].left_mode = p.left_mode;
	_points.write[i].right_mode = p.right_mode;
	if (p_index != i) {
		update_auto_tangents(p_index);
	}
	update_auto_tangents(i);
	return i;
}

Vector2 Curve::get_point_position(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, _points.size(), Vector2(0, 0));
	return _points[p_index].position;
}

Curve::Point Curve::get_point(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, _points.size(), Point());
	return _points[p_index];
}

void Curve::update_auto_tangents(int p_index) {
	Point &p = _points.write[p_index];

	if (p_index > 0) {
		if (p.left_mode == TANGENT_LINEAR) {
			Vector2 v = (_points[p_index - 1].position - p.position).normalized();
			p.left_tangent = v.y / v.x;
		}
		if (_points[p_index - 1].right_mode == TANGENT_LINEAR) {
			Vector2 v = (_points[p_index - 1].position - p.position).normalized();
			_points.write[p_index - 1].right_tangent = v.y / v.x;
		}
	}

	if (p_index + 1 < _points.size()) {
		if (p.right_mode == TANGENT_LINEAR) {
			Vector2 v = (_points[p_index + 1].position - p.position).normalized();
			p.right_tangent = v.y / v.x;
		}
		if (_points[p_index + 1].left_mode == TANGENT_LINEAR) {
			Vector2 v = (_points[p_index + 1].position - p.position).normalized();
			_points.write[p_index + 1].left_tangent = v.y / v.x;
		}
	}
}

#define MIN_X_RANGE 0.01
#define MIN_Y_RANGE 0.01

Array Curve::get_limits() const {
	Array output;
	output.resize(4);

	output[0] = _min_value;
	output[1] = _max_value;
	output[2] = _min_domain;
	output[3] = _max_domain;

	return output;
}

void Curve::set_limits(const Array &p_input) {
	if (p_input.size() != 4) {
		WARN_PRINT_ED(vformat(R"(Could not find Curve limit values when deserializing "%s". Resetting limits to default values.)", this->get_path()));
		_min_value = 0;
		_max_value = 1;
		_min_domain = 0;
		_max_domain = 1;
		return;
	}

	// Do not use setters because we don't want to enforce their logical constraints during deserialization.
	_min_value = p_input[0];
	_max_value = p_input[1];
	_min_domain = p_input[2];
	_max_domain = p_input[3];
}

void Curve::set_min_value(real_t p_min) {
	_min_value = MIN(p_min, _max_value - MIN_Y_RANGE);

	for (const Point &p : _points) {
		_min_value = MIN(_min_value, p.position.y);
	}

	emit_signal(SNAME(SIGNAL_RANGE_CHANGED));
}

void Curve::set_max_value(real_t p_max) {
	_max_value = MAX(p_max, _min_value + MIN_Y_RANGE);

	for (const Point &p : _points) {
		_max_value = MAX(_max_value, p.position.y);
	}

	emit_signal(SNAME(SIGNAL_RANGE_CHANGED));
}

void Curve::set_min_domain(real_t p_min) {
	_min_domain = MIN(p_min, _max_domain - MIN_X_RANGE);

	if (_points.size() > 0 && _min_domain > _points[0].position.x) {
		_min_domain = _points[0].position.x;
	}

	mark_dirty();
	emit_signal(SNAME(SIGNAL_DOMAIN_CHANGED));
}

void Curve::set_max_domain(real_t p_max) {
	_max_domain = MAX(p_max, _min_domain + MIN_X_RANGE);

	if (_points.size() > 0 && _max_domain < _points[_points.size() - 1].position.x) {
		_max_domain = _points[_points.size() - 1].position.x;
	}

	mark_dirty();
	emit_signal(SNAME(SIGNAL_DOMAIN_CHANGED));
}

real_t Curve::sample(real_t p_offset) const {
	if (_points.size() == 0) {
		return 0;
	}
	if (_points.size() == 1) {
		return _points[0].position.y;
	}

	int i = get_index(p_offset);

	if (i == _points.size() - 1) {
		return _points[i].position.y;
	}

	real_t local = p_offset - _points[i].position.x;

	if (i == 0 && local <= 0) {
		return _points[0].position.y;
	}

	return sample_local_nocheck(i, local);
}

real_t Curve::sample_local_nocheck(int p_index, real_t p_local_offset) const {
	const Point a = _points[p_index];
	const Point b = _points[p_index + 1];

	/* Cubic bézier
	 *
	 *       ac-----bc
	 *      /         \
	 *     /           \     Here with a.right_tangent > 0
	 *    /             \    and b.left_tangent < 0
	 *   /               \
	 *  a                 b
	 *
	 *  |-d1--|-d2--|-d3--|
	 *
	 * d1 == d2 == d3 == d / 3
	 */

	// Control points are chosen at equal distances.
	real_t d = b.position.x - a.position.x;
	if (Math::is_zero_approx(d)) {
		return b.position.y;
	}
	p_local_offset /= d;
	d /= 3.0;
	real_t yac = a.position.y + d * a.right_tangent;
	real_t ybc = b.position.y - d * b.left_tangent;

	real_t y = Math::bezier_interpolate(a.position.y, yac, ybc, b.position.y, p_local_offset);

	return y;
}

void Curve::mark_dirty() {
	_baked_cache_dirty = true;
	emit_changed();
}

Array Curve::get_data() const {
	Array output;
	const unsigned int ELEMS = 5;
	output.resize(_points.size() * ELEMS);

	for (int j = 0; j < _points.size(); ++j) {
		const Point p = _points[j];
		int i = j * ELEMS;

		output[i] = p.position;
		output[i + 1] = p.left_tangent;
		output[i + 2] = p.right_tangent;
		output[i + 3] = p.left_mode;
		output[i + 4] = p.right_mode;
	}

	return output;
}

void Curve::set_data(const Array p_input) {
	const unsigned int ELEMS = 5;
	ERR_FAIL_COND(p_input.size() % ELEMS != 0);

	// Validate input
	for (int i = 0; i < p_input.size(); i += ELEMS) {
		ERR_FAIL_COND(p_input[i].get_type() != Variant::VECTOR2);
		ERR_FAIL_COND(!p_input[i + 1].is_num());
		ERR_FAIL_COND(p_input[i + 2].get_type() != Variant::FLOAT);

		ERR_FAIL_COND(p_input[i + 3].get_type() != Variant::INT);
		int left_mode = p_input[i + 3];
		ERR_FAIL_COND(left_mode < 0 || left_mode >= TANGENT_MODE_COUNT);

		ERR_FAIL_COND(p_input[i + 4].get_type() != Variant::INT);
		int right_mode = p_input[i + 4];
		ERR_FAIL_COND(right_mode < 0 || right_mode >= TANGENT_MODE_COUNT);
	}
	int old_size = _points.size();
	int new_size = p_input.size() / ELEMS;
	if (old_size != new_size) {
		_points.resize(new_size);
	}

	for (int j = 0; j < _points.size(); ++j) {
		Point &p = _points.write[j];
		int i = j * ELEMS;

		p.position = p_input[i];
		p.left_tangent = p_input[i + 1];
		p.right_tangent = p_input[i + 2];
		int left_mode = p_input[i + 3];
		int right_mode = p_input[i + 4];
		p.left_mode = (TangentMode)left_mode;
		p.right_mode = (TangentMode)right_mode;
	}

	mark_dirty();
	if (old_size != new_size) {
		notify_property_list_changed();
	}
}

void Curve::bake() {
	_bake();
}

void Curve::_bake() const {
	_baked_cache.clear();

	_baked_cache.resize(_bake_resolution);

	for (int i = 1; i < _bake_resolution - 1; ++i) {
		real_t x = get_domain_range() * i / static_cast<real_t>(_bake_resolution - 1) + _min_domain;
		real_t y = sample(x);
		_baked_cache.write[i] = y;
	}

	if (_points.size() != 0) {
		_baked_cache.write[0] = _points[0].position.y;
		_baked_cache.write[_baked_cache.size() - 1] = _points[_points.size() - 1].position.y;
	}

	_baked_cache_dirty = false;
}

void Curve::set_bake_resolution(int p_resolution) {
	ERR_FAIL_COND(p_resolution < 1);
	ERR_FAIL_COND(p_resolution > 1000);
	_bake_resolution = p_resolution;
	_baked_cache_dirty = true;
}

real_t Curve::sample_baked(real_t p_offset) const {
	// Make sure that p_offset is finite.
	ERR_FAIL_COND_V_MSG(!Math::is_finite(p_offset), 0, "Offset is non-finite");

	if (_baked_cache_dirty) {
		// Last-second bake if not done already.
		_bake();
	}

	// Special cases if the cache is too small.
	if (_baked_cache.size() == 0) {
		if (_points.size() == 0) {
			return 0;
		}
		return _points[0].position.y;
	} else if (_baked_cache.size() == 1) {
		return _baked_cache[0];
	}

	// Get interpolation index.
	real_t fi = (p_offset - _min_domain) / get_domain_range() * (_baked_cache.size() - 1);
	int i = Math::floor(fi);
	if (i < 0) {
		i = 0;
		fi = 0;
	} else if (i >= _baked_cache.size()) {
		i = _baked_cache.size() - 1;
		fi = 0;
	}

	// Sample.
	if (i + 1 < _baked_cache.size()) {
		real_t t = fi - i;
		return Math::lerp(_baked_cache[i], _baked_cache[i + 1], t);
	} else {
		return _baked_cache[_baked_cache.size() - 1];
	}
}

void Curve::ensure_default_setup(real_t p_min, real_t p_max) {
	if (_points.size() == 0 && _min_value == 0 && _max_value == 1) {
		add_point(Vector2(0, 1));
		add_point(Vector2(1, 1));
		set_min_value(p_min);
		set_max_value(p_max);
	}
}

bool Curve::_set(const StringName &p_name, const Variant &p_value) {
	Vector<String> components = String(p_name).split("/", true, 2);
	if (components.size() >= 2 && components[0].begins_with("point_") && components[0].trim_prefix("point_").is_valid_int()) {
		int point_index = components[0].trim_prefix("point_").to_int();
		const String &property = components[1];
		if (property == "position") {
			Vector2 position = p_value.operator Vector2();
			set_point_offset(point_index, position.x);
			set_point_value(point_index, position.y);
			return true;
		} else if (property == "left_tangent") {
			set_point_left_tangent(point_index, p_value);
			return true;
		} else if (property == "left_mode") {
			int mode = p_value;
			set_point_left_mode(point_index, (TangentMode)mode);
			return true;
		} else if (property == "right_tangent") {
			set_point_right_tangent(point_index, p_value);
			return true;
		} else if (property == "right_mode") {
			int mode = p_value;
			set_point_right_mode(point_index, (TangentMode)mode);
			return true;
		}
	}
	return false;
}

bool Curve::_get(const StringName &p_name, Variant &r_ret) const {
	Vector<String> components = String(p_name).split("/", true, 2);
	if (components.size() >= 2 && components[0].begins_with("point_") && components[0].trim_prefix("point_").is_valid_int()) {
		int point_index = components[0].trim_prefix("point_").to_int();
		const String &property = components[1];
		if (property == "position") {
			r_ret = get_point_position(point_index);
			return true;
		} else if (property == "left_tangent") {
			r_ret = get_point_left_tangent(point_index);
			return true;
		} else if (property == "left_mode") {
			r_ret = get_point_left_mode(point_index);
			return true;
		} else if (property == "right_tangent") {
			r_ret = get_point_right_tangent(point_index);
			return true;
		} else if (property == "right_mode") {
			r_ret = get_point_right_mode(point_index);
			return true;
		}
	}
	return false;
}

void Curve::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < _points.size(); i++) {
		PropertyInfo pi = PropertyInfo(Variant::VECTOR2, vformat("point_%d/position", i));
		pi.usage &= ~PROPERTY_USAGE_STORAGE;
		p_list->push_back(pi);

		if (i != 0) {
			pi = PropertyInfo(Variant::FLOAT, vformat("point_%d/left_tangent", i));
			pi.usage &= ~PROPERTY_USAGE_STORAGE;
			p_list->push_back(pi);

			pi = PropertyInfo(Variant::INT, vformat("point_%d/left_mode", i), PROPERTY_HINT_ENUM, "Free,Linear");
			pi.usage &= ~PROPERTY_USAGE_STORAGE;
			p_list->push_back(pi);
		}

		if (i != _points.size() - 1) {
			pi = PropertyInfo(Variant::FLOAT, vformat("point_%d/right_tangent", i));
			pi.usage &= ~PROPERTY_USAGE_STORAGE;
			p_list->push_back(pi);

			pi = PropertyInfo(Variant::INT, vformat("point_%d/right_mode", i), PROPERTY_HINT_ENUM, "Free,Linear");
			pi.usage &= ~PROPERTY_USAGE_STORAGE;
			p_list->push_back(pi);
		}
	}
}

void Curve::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_point_count"), &Curve::get_point_count);
	ClassDB::bind_method(D_METHOD("set_point_count", "count"), &Curve::set_point_count);
	ClassDB::bind_method(D_METHOD("add_point", "position", "left_tangent", "right_tangent", "left_mode", "right_mode"), &Curve::add_point, DEFVAL(0), DEFVAL(0), DEFVAL(TANGENT_FREE), DEFVAL(TANGENT_FREE));
	ClassDB::bind_method(D_METHOD("remove_point", "index"), &Curve::remove_point);
	ClassDB::bind_method(D_METHOD("clear_points"), &Curve::clear_points);
	ClassDB::bind_method(D_METHOD("get_point_position", "index"), &Curve::get_point_position);
	ClassDB::bind_method(D_METHOD("set_point_value", "index", "y"), &Curve::set_point_value);
	ClassDB::bind_method(D_METHOD("set_point_offset", "index", "offset"), &Curve::set_point_offset);
	ClassDB::bind_method(D_METHOD("sample", "offset"), &Curve::sample);
	ClassDB::bind_method(D_METHOD("sample_baked", "offset"), &Curve::sample_baked);
	ClassDB::bind_method(D_METHOD("get_point_left_tangent", "index"), &Curve::get_point_left_tangent);
	ClassDB::bind_method(D_METHOD("get_point_right_tangent", "index"), &Curve::get_point_right_tangent);
	ClassDB::bind_method(D_METHOD("get_point_left_mode", "index"), &Curve::get_point_left_mode);
	ClassDB::bind_method(D_METHOD("get_point_right_mode", "index"), &Curve::get_point_right_mode);
	ClassDB::bind_method(D_METHOD("set_point_left_tangent", "index", "tangent"), &Curve::set_point_left_tangent);
	ClassDB::bind_method(D_METHOD("set_point_right_tangent", "index", "tangent"), &Curve::set_point_right_tangent);
	ClassDB::bind_method(D_METHOD("set_point_left_mode", "index", "mode"), &Curve::set_point_left_mode);
	ClassDB::bind_method(D_METHOD("set_point_right_mode", "index", "mode"), &Curve::set_point_right_mode);
	ClassDB::bind_method(D_METHOD("get_min_value"), &Curve::get_min_value);
	ClassDB::bind_method(D_METHOD("set_min_value", "min"), &Curve::set_min_value);
	ClassDB::bind_method(D_METHOD("get_max_value"), &Curve::get_max_value);
	ClassDB::bind_method(D_METHOD("set_max_value", "max"), &Curve::set_max_value);
	ClassDB::bind_method(D_METHOD("get_value_range"), &Curve::get_value_range);
	ClassDB::bind_method(D_METHOD("get_min_domain"), &Curve::get_min_domain);
	ClassDB::bind_method(D_METHOD("set_min_domain", "min"), &Curve::set_min_domain);
	ClassDB::bind_method(D_METHOD("get_max_domain"), &Curve::get_max_domain);
	ClassDB::bind_method(D_METHOD("set_max_domain", "max"), &Curve::set_max_domain);
	ClassDB::bind_method(D_METHOD("get_domain_range"), &Curve::get_domain_range);
	ClassDB::bind_method(D_METHOD("_get_limits"), &Curve::get_limits);
	ClassDB::bind_method(D_METHOD("_set_limits", "data"), &Curve::set_limits);
	ClassDB::bind_method(D_METHOD("clean_dupes"), &Curve::clean_dupes);
	ClassDB::bind_method(D_METHOD("bake"), &Curve::bake);
	ClassDB::bind_method(D_METHOD("get_bake_resolution"), &Curve::get_bake_resolution);
	ClassDB::bind_method(D_METHOD("set_bake_resolution", "resolution"), &Curve::set_bake_resolution);
	ClassDB::bind_method(D_METHOD("_get_data"), &Curve::get_data);
	ClassDB::bind_method(D_METHOD("_set_data", "data"), &Curve::set_data);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_domain", PROPERTY_HINT_RANGE, "-1024,1024,0.01,or_greater,or_less", PROPERTY_USAGE_EDITOR), "set_min_domain", "get_min_domain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_domain", PROPERTY_HINT_RANGE, "-1024,1024,0.01,or_greater,or_less", PROPERTY_USAGE_EDITOR), "set_max_domain", "get_max_domain");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "min_value", PROPERTY_HINT_RANGE, "-1024,1024,0.01,or_greater,or_less", PROPERTY_USAGE_EDITOR), "set_min_value", "get_min_value");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_value", PROPERTY_HINT_RANGE, "-1024,1024,0.01,or_greater,or_less", PROPERTY_USAGE_EDITOR), "set_max_value", "get_max_value");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "_limits", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_limits", "_get_limits");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bake_resolution", PROPERTY_HINT_RANGE, "1,1000,1"), "set_bake_resolution", "get_bake_resolution");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_data", "_get_data");
	ADD_ARRAY_COUNT("Points", "point_count", "set_point_count", "get_point_count", "point_");

	ADD_SIGNAL(MethodInfo(SIGNAL_RANGE_CHANGED));
	ADD_SIGNAL(MethodInfo(SIGNAL_DOMAIN_CHANGED));

	BIND_ENUM_CONSTANT(TANGENT_FREE);
	BIND_ENUM_CONSTANT(TANGENT_LINEAR);
	BIND_ENUM_CONSTANT(TANGENT_MODE_COUNT);
}

int Curve2D::get_point_count() const {
	return points.size();
}

void Curve2D::set_point_count(int p_count) {
	ERR_FAIL_COND(p_count < 0);
	int old_size = points.size();
	if (old_size == p_count) {
		return;
	}

	if (old_size > p_count) {
		points.resize(p_count);
		mark_dirty();
	} else {
		for (int i = p_count - old_size; i > 0; i--) {
			_add_point(Vector2());
		}
	}
	notify_property_list_changed();
}

void Curve2D::_add_point(const Vector2 &p_position, const Vector2 &p_in, const Vector2 &p_out, int p_atpos) {
	Point n;
	n.position = p_position;
	n.in = p_in;
	n.out = p_out;
	if (p_atpos >= 0 && p_atpos < points.size()) {
		points.insert(p_atpos, n);
	} else {
		points.push_back(n);
	}

	mark_dirty();
}

void Curve2D::add_point(const Vector2 &p_position, const Vector2 &p_in, const Vector2 &p_out, int p_atpos) {
	_add_point(p_position, p_in, p_out, p_atpos);
	notify_property_list_changed();
}

void Curve2D::set_point_position(int p_index, const Vector2 &p_position) {
	ERR_FAIL_INDEX(p_index, points.size());

	points.write[p_index].position = p_position;
	mark_dirty();
}

Vector2 Curve2D::get_point_position(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, points.size(), Vector2());
	return points[p_index].position;
}

void Curve2D::set_point_in(int p_index, const Vector2 &p_in) {
	ERR_FAIL_INDEX(p_index, points.size());

	points.write[p_index].in = p_in;
	mark_dirty();
}

Vector2 Curve2D::get_point_in(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, points.size(), Vector2());
	return points[p_index].in;
}

void Curve2D::set_point_out(int p_index, const Vector2 &p_out) {
	ERR_FAIL_INDEX(p_index, points.size());

	points.write[p_index].out = p_out;
	mark_dirty();
}

Vector2 Curve2D::get_point_out(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, points.size(), Vector2());
	return points[p_index].out;
}

void Curve2D::_remove_point(int p_index) {
	ERR_FAIL_INDEX(p_index, points.size());
	points.remove_at(p_index);
	mark_dirty();
}

void Curve2D::remove_point(int p_index) {
	_remove_point(p_index);
	notify_property_list_changed();
}

void Curve2D::clear_points() {
	if (!points.is_empty()) {
		points.clear();
		mark_dirty();
		notify_property_list_changed();
	}
}

Vector2 Curve2D::sample(int p_index, const real_t p_offset) const {
	int pc = points.size();
	ERR_FAIL_COND_V(pc == 0, Vector2());

	if (p_index >= pc - 1) {
		return points[pc - 1].position;
	} else if (p_index < 0) {
		return points[0].position;
	}

	Vector2 p0 = points[p_index].position;
	Vector2 p1 = p0 + points[p_index].out;
	Vector2 p3 = points[p_index + 1].position;
	Vector2 p2 = p3 + points[p_index + 1].in;

	return p0.bezier_interpolate(p1, p2, p3, p_offset);
}

Vector2 Curve2D::samplef(real_t p_findex) const {
	if (p_findex < 0) {
		p_findex = 0;
	} else if (p_findex >= points.size()) {
		p_findex = points.size();
	}

	return sample((int)p_findex, Math::fmod(p_findex, (real_t)1.0));
}

void Curve2D::mark_dirty() {
	baked_cache_dirty = true;
	emit_changed();
}

void Curve2D::_bake_segment2d(RBMap<real_t, Vector2> &r_bake, real_t p_begin, real_t p_end, const Vector2 &p_a, const Vector2 &p_out, const Vector2 &p_b, const Vector2 &p_in, int p_depth, int p_max_depth, real_t p_tol) const {
	real_t mp = p_begin + (p_end - p_begin) * 0.5;
	Vector2 beg = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, p_begin);
	Vector2 mid = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, mp);
	Vector2 end = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, p_end);

	Vector2 na = (mid - beg).normalized();
	Vector2 nb = (end - mid).normalized();
	real_t dp = na.dot(nb);

	if (dp < Math::cos(Math::deg_to_rad(p_tol))) {
		r_bake[mp] = mid;
	}

	if (p_depth < p_max_depth) {
		_bake_segment2d(r_bake, p_begin, mp, p_a, p_out, p_b, p_in, p_depth + 1, p_max_depth, p_tol);
		_bake_segment2d(r_bake, mp, p_end, p_a, p_out, p_b, p_in, p_depth + 1, p_max_depth, p_tol);
	}
}

void Curve2D::_bake_segment2d_even_length(RBMap<real_t, Vector2> &r_bake, real_t p_begin, real_t p_end, const Vector2 &p_a, const Vector2 &p_out, const Vector2 &p_b, const Vector2 &p_in, int p_depth, int p_max_depth, real_t p_length) const {
	Vector2 beg = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, p_begin);
	Vector2 end = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, p_end);

	real_t length = beg.distance_to(end);

	if (length > p_length && p_depth < p_max_depth) {
		real_t mp = (p_begin + p_end) * 0.5;
		Vector2 mid = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, mp);
		r_bake[mp] = mid;

		_bake_segment2d_even_length(r_bake, p_begin, mp, p_a, p_out, p_b, p_in, p_depth + 1, p_max_depth, p_length);
		_bake_segment2d_even_length(r_bake, mp, p_end, p_a, p_out, p_b, p_in, p_depth + 1, p_max_depth, p_length);
	}
}

Vector2 Curve2D::_calculate_tangent(const Vector2 &p_begin, const Vector2 &p_control_1, const Vector2 &p_control_2, const Vector2 &p_end, const real_t p_t) {
	// Handle corner cases.
	if (Math::is_zero_approx(p_t - 0.0f) && p_control_1.is_equal_approx(p_begin)) {
		return (p_end - p_begin).normalized();
	}

	if (Math::is_zero_approx(p_t - 1.0f) && p_control_2.is_equal_approx(p_end)) {
		return (p_end - p_begin).normalized();
	}

	return p_begin.bezier_derivative(p_control_1, p_control_2, p_end, p_t).normalized();
}

void Curve2D::_bake() const {
	if (!baked_cache_dirty) {
		return;
	}

	baked_max_ofs = 0;
	baked_cache_dirty = false;

	if (points.size() == 0) {
		baked_point_cache.clear();
		baked_dist_cache.clear();
		baked_forward_vector_cache.clear();
		return;
	}

	if (points.size() == 1) {
		baked_point_cache.resize(1);
		baked_point_cache.set(0, points[0].position);
		baked_dist_cache.resize(1);
		baked_dist_cache.set(0, 0.0);
		baked_forward_vector_cache.resize(1);
		baked_forward_vector_cache.set(0, Vector2(0.0, 0.1));

		return;
	}

	// Tessellate curve to (almost) even length segments.
	{
		Vector<RBMap<real_t, Vector2>> midpoints = _tessellate_even_length(10, bake_interval);

		int pc = 1;
		for (int i = 0; i < points.size() - 1; i++) {
			pc++;
			pc += midpoints[i].size();
		}

		baked_point_cache.resize(pc);
		baked_dist_cache.resize(pc);
		baked_forward_vector_cache.resize(pc);

		Vector2 *bpw = baked_point_cache.ptrw();
		Vector2 *bfw = baked_forward_vector_cache.ptrw();

		// Collect positions and sample tilts and tangents for each baked points.
		bpw[0] = points[0].position;
		bfw[0] = _calculate_tangent(points[0].position, points[0].position + points[0].out, points[1].position + points[1].in, points[1].position, 0.0);
		int pidx = 0;

		for (int i = 0; i < points.size() - 1; i++) {
			for (const KeyValue<real_t, Vector2> &E : midpoints[i]) {
				pidx++;
				bpw[pidx] = E.value;
				bfw[pidx] = _calculate_tangent(points[i].position, points[i].position + points[i].out, points[i + 1].position + points[i + 1].in, points[i + 1].position, E.key);
			}

			pidx++;
			bpw[pidx] = points[i + 1].position;
			bfw[pidx] = _calculate_tangent(points[i].position, points[i].position + points[i].out, points[i + 1].position + points[i + 1].in, points[i + 1].position, 1.0);
		}

		// Recalculate the baked distances.
		real_t *bdw = baked_dist_cache.ptrw();
		bdw[0] = 0.0;
		for (int i = 0; i < pc - 1; i++) {
			bdw[i + 1] = bdw[i] + bpw[i].distance_to(bpw[i + 1]);
		}
		baked_max_ofs = bdw[pc - 1];
	}
}

real_t Curve2D::get_baked_length() const {
	if (baked_cache_dirty) {
		_bake();
	}

	return baked_max_ofs;
}

Curve2D::Interval Curve2D::_find_interval(real_t p_offset) const {
	Interval interval = {
		-1,
		0.0
	};
	ERR_FAIL_COND_V_MSG(baked_cache_dirty, interval, "Backed cache is dirty");

	int pc = baked_point_cache.size();
	ERR_FAIL_COND_V_MSG(pc < 2, interval, "Less than two points in cache");

	int start = 0;
	int end = pc;
	int idx = (end + start) / 2;
	// Binary search to find baked points.
	while (start < idx) {
		real_t offset = baked_dist_cache[idx];
		if (p_offset <= offset) {
			end = idx;
		} else {
			start = idx;
		}
		idx = (end + start) / 2;
	}

	real_t offset_begin = baked_dist_cache[idx];
	real_t offset_end = baked_dist_cache[idx + 1];

	real_t idx_interval = offset_end - offset_begin;
	ERR_FAIL_COND_V_MSG(p_offset < offset_begin || p_offset > offset_end, interval, "Offset out of range.");

	interval.idx = idx;
	if (idx_interval < FLT_EPSILON) {
		interval.frac = 0.5; // For a very short interval, 0.5 is a reasonable choice.
		ERR_FAIL_V_MSG(interval, "Zero length interval.");
	}

	interval.frac = (p_offset - offset_begin) / idx_interval;
	return interval;
}

Vector2 Curve2D::_sample_baked(Interval p_interval, bool p_cubic) const {
	// Assuming p_interval is valid.
	ERR_FAIL_INDEX_V_MSG(p_interval.idx, baked_point_cache.size(), Vector2(), "Invalid interval");

	int idx = p_interval.idx;
	real_t frac = p_interval.frac;

	const Vector2 *r = baked_point_cache.ptr();
	int pc = baked_point_cache.size();

	if (p_cubic) {
		Vector2 pre = idx > 0 ? r[idx - 1] : r[idx];
		Vector2 post = (idx < (pc - 2)) ? r[idx + 2] : r[idx + 1];
		return r[idx].cubic_interpolate(r[idx + 1], pre, post, frac);
	} else {
		return r[idx].lerp(r[idx + 1], frac);
	}
}

Transform2D Curve2D::_sample_posture(Interval p_interval) const {
	// Assuming that p_interval is valid.
	ERR_FAIL_INDEX_V_MSG(p_interval.idx, baked_point_cache.size(), Transform2D(), "Invalid interval");

	int idx = p_interval.idx;
	real_t frac = p_interval.frac;

	Vector2 forward_begin = baked_forward_vector_cache[idx];
	Vector2 forward_end = baked_forward_vector_cache[idx + 1];

	// Build frames at both ends of the interval, then interpolate.
	const Vector2 forward = forward_begin.slerp(forward_end, frac).normalized();
	const Vector2 side = Vector2(-forward.y, forward.x);

	return Transform2D(forward, side, Vector2(0.0, 0.0));
}

Vector2 Curve2D::sample_baked(real_t p_offset, bool p_cubic) const {
	// Make sure that p_offset is finite.
	ERR_FAIL_COND_V_MSG(!Math::is_finite(p_offset), Vector2(), "Offset is non-finite");

	if (baked_cache_dirty) {
		_bake();
	}

	// Validate: Curve may not have baked points.
	int pc = baked_point_cache.size();
	ERR_FAIL_COND_V_MSG(pc == 0, Vector2(), "No points in Curve2D.");

	if (pc == 1) {
		return baked_point_cache[0];
	}

	p_offset = CLAMP(p_offset, 0.0, get_baked_length()); // PathFollower implement wrapping logic.

	Curve2D::Interval interval = _find_interval(p_offset);
	return _sample_baked(interval, p_cubic);
}

Transform2D Curve2D::sample_baked_with_rotation(real_t p_offset, bool p_cubic) const {
	// Make sure that p_offset is finite.
	ERR_FAIL_COND_V_MSG(!Math::is_finite(p_offset), Transform2D(), "Offset is non-finite");

	if (baked_cache_dirty) {
		_bake();
	}

	// Validate: Curve may not have baked points.
	const int point_count = baked_point_cache.size();
	ERR_FAIL_COND_V_MSG(point_count == 0, Transform2D(), "No points in Curve3D.");

	if (point_count == 1) {
		Transform2D t;
		t.set_origin(baked_point_cache.get(0));
		ERR_FAIL_V_MSG(t, "Only 1 point in Curve2D.");
	}

	p_offset = CLAMP(p_offset, 0.0, get_baked_length()); // PathFollower implement wrapping logic.

	// 0. Find interval for all sampling steps.
	Curve2D::Interval interval = _find_interval(p_offset);

	// 1. Sample position.
	Vector2 pos = _sample_baked(interval, p_cubic);

	// 2. Sample rotation frame.
	Transform2D frame = _sample_posture(interval);
	frame.set_origin(pos);

	return frame;
}

PackedVector2Array Curve2D::get_baked_points() const {
	if (baked_cache_dirty) {
		_bake();
	}

	return baked_point_cache;
}

void Curve2D::set_bake_interval(real_t p_tolerance) {
	bake_interval = p_tolerance;
	mark_dirty();
}

real_t Curve2D::get_bake_interval() const {
	return bake_interval;
}

PackedVector2Array Curve2D::get_points() const {
	return _get_data()["points"];
}

Vector2 Curve2D::get_closest_point(const Vector2 &p_to_point) const {
	// Brute force method.

	if (baked_cache_dirty) {
		_bake();
	}

	// Validate: Curve may not have baked points.
	int pc = baked_point_cache.size();
	ERR_FAIL_COND_V_MSG(pc == 0, Vector2(), "No points in Curve2D.");

	if (pc == 1) {
		return baked_point_cache.get(0);
	}

	const Vector2 *r = baked_point_cache.ptr();

	Vector2 nearest;
	real_t nearest_dist = -1.0f;

	for (int i = 0; i < pc - 1; i++) {
		const real_t interval = baked_dist_cache[i + 1] - baked_dist_cache[i];
		Vector2 origin = r[i];
		Vector2 direction = (r[i + 1] - origin) / interval;

		real_t d = CLAMP((p_to_point - origin).dot(direction), 0.0f, interval);
		Vector2 proj = origin + direction * d;

		real_t dist = proj.distance_squared_to(p_to_point);

		if (nearest_dist < 0.0f || dist < nearest_dist) {
			nearest = proj;
			nearest_dist = dist;
		}
	}

	return nearest;
}

real_t Curve2D::get_closest_offset(const Vector2 &p_to_point) const {
	// Brute force method.

	if (baked_cache_dirty) {
		_bake();
	}

	// Validate: Curve may not have baked points.
	int pc = baked_point_cache.size();
	ERR_FAIL_COND_V_MSG(pc == 0, 0.0f, "No points in Curve2D.");

	if (pc == 1) {
		return 0.0f;
	}

	const Vector2 *r = baked_point_cache.ptr();

	real_t nearest = 0.0f;
	real_t nearest_dist = -1.0f;
	real_t offset = 0.0f;

	for (int i = 0; i < pc - 1; i++) {
		offset = baked_dist_cache[i];

		const real_t interval = baked_dist_cache[i + 1] - baked_dist_cache[i];
		Vector2 origin = r[i];
		Vector2 direction = (r[i + 1] - origin) / interval;

		real_t d = CLAMP((p_to_point - origin).dot(direction), 0.0f, interval);
		Vector2 proj = origin + direction * d;

		real_t dist = proj.distance_squared_to(p_to_point);

		if (nearest_dist < 0.0f || dist < nearest_dist) {
			nearest = offset + d;
			nearest_dist = dist;
		}
	}

	return nearest;
}

Dictionary Curve2D::_get_data() const {
	Dictionary dc;

	PackedVector2Array d;
	d.resize(points.size() * 3);
	Vector2 *w = d.ptrw();

	for (int i = 0; i < points.size(); i++) {
		w[i * 3 + 0] = points[i].in;
		w[i * 3 + 1] = points[i].out;
		w[i * 3 + 2] = points[i].position;
	}

	dc["points"] = d;

	return dc;
}

void Curve2D::_set_data(const Dictionary &p_data) {
	ERR_FAIL_COND(!p_data.has("points"));

	PackedVector2Array rp = p_data["points"];
	int pc = rp.size();
	ERR_FAIL_COND(pc % 3 != 0);
	int old_size = points.size();
	int new_size = pc / 3;
	if (old_size != new_size) {
		points.resize(new_size);
	}
	const Vector2 *r = rp.ptr();

	for (int i = 0; i < points.size(); i++) {
		points.write[i].in = r[i * 3 + 0];
		points.write[i].out = r[i * 3 + 1];
		points.write[i].position = r[i * 3 + 2];
	}

	mark_dirty();
	if (old_size != new_size) {
		notify_property_list_changed();
	}
}

PackedVector2Array Curve2D::tessellate(int p_max_stages, real_t p_tolerance) const {
	PackedVector2Array tess;

	if (points.size() == 0) {
		return tess;
	}

	// The current implementation requires a sorted map.
	Vector<RBMap<real_t, Vector2>> midpoints;

	midpoints.resize(points.size() - 1);

	int pc = 1;
	for (int i = 0; i < points.size() - 1; i++) {
		_bake_segment2d(midpoints.write[i], 0, 1, points[i].position, points[i].out, points[i + 1].position, points[i + 1].in, 0, p_max_stages, p_tolerance);
		pc++;
		pc += midpoints[i].size();
	}

	tess.resize(pc);
	Vector2 *bpw = tess.ptrw();
	bpw[0] = points[0].position;
	int pidx = 0;

	for (int i = 0; i < points.size() - 1; i++) {
		for (const KeyValue<real_t, Vector2> &E : midpoints[i]) {
			pidx++;
			bpw[pidx] = E.value;
		}

		pidx++;
		bpw[pidx] = points[i + 1].position;
	}

	return tess;
}

Vector<RBMap<real_t, Vector2>> Curve2D::_tessellate_even_length(int p_max_stages, real_t p_length) const {
	Vector<RBMap<real_t, Vector2>> midpoints;
	ERR_FAIL_COND_V_MSG(points.size() < 2, midpoints, "Curve must have at least 2 control point");

	midpoints.resize(points.size() - 1);

	for (int i = 0; i < points.size() - 1; i++) {
		_bake_segment2d_even_length(midpoints.write[i], 0, 1, points[i].position, points[i].out, points[i + 1].position, points[i + 1].in, 0, p_max_stages, p_length);
	}
	return midpoints;
}

PackedVector2Array Curve2D::tessellate_even_length(int p_max_stages, real_t p_length) const {
	PackedVector2Array tess;

	Vector<RBMap<real_t, Vector2>> midpoints = _tessellate_even_length(p_max_stages, p_length);
	if (midpoints.size() == 0) {
		return tess;
	}

	int pc = 1;
	for (int i = 0; i < points.size() - 1; i++) {
		pc++;
		pc += midpoints[i].size();
	}

	tess.resize(pc);
	Vector2 *bpw = tess.ptrw();
	bpw[0] = points[0].position;
	int pidx = 0;

	for (int i = 0; i < points.size() - 1; i++) {
		for (const KeyValue<real_t, Vector2> &E : midpoints[i]) {
			pidx++;
			bpw[pidx] = E.value;
		}

		pidx++;
		bpw[pidx] = points[i + 1].position;
	}

	return tess;
}

bool Curve2D::_set(const StringName &p_name, const Variant &p_value) {
	Vector<String> components = String(p_name).split("/", true, 2);
	if (components.size() >= 2 && components[0].begins_with("point_") && components[0].trim_prefix("point_").is_valid_int()) {
		int point_index = components[0].trim_prefix("point_").to_int();
		const String &property = components[1];
		if (property == "position") {
			set_point_position(point_index, p_value);
			return true;
		} else if (property == "in") {
			set_point_in(point_index, p_value);
			return true;
		} else if (property == "out") {
			set_point_out(point_index, p_value);
			return true;
		}
	}
	return false;
}

bool Curve2D::_get(const StringName &p_name, Variant &r_ret) const {
	Vector<String> components = String(p_name).split("/", true, 2);
	if (components.size() >= 2 && components[0].begins_with("point_") && components[0].trim_prefix("point_").is_valid_int()) {
		int point_index = components[0].trim_prefix("point_").to_int();
		const String &property = components[1];
		if (property == "position") {
			r_ret = get_point_position(point_index);
			return true;
		} else if (property == "in") {
			r_ret = get_point_in(point_index);
			return true;
		} else if (property == "out") {
			r_ret = get_point_out(point_index);
			return true;
		}
	}
	return false;
}

void Curve2D::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < points.size(); i++) {
		PropertyInfo pi = PropertyInfo(Variant::VECTOR2, vformat("point_%d/position", i));
		pi.usage &= ~PROPERTY_USAGE_STORAGE;
		p_list->push_back(pi);

		if (i != 0) {
			pi = PropertyInfo(Variant::VECTOR2, vformat("point_%d/in", i));
			pi.usage &= ~PROPERTY_USAGE_STORAGE;
			p_list->push_back(pi);
		}

		if (i != points.size() - 1) {
			pi = PropertyInfo(Variant::VECTOR2, vformat("point_%d/out", i));
			pi.usage &= ~PROPERTY_USAGE_STORAGE;
			p_list->push_back(pi);
		}
	}
}

void Curve2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_point_count"), &Curve2D::get_point_count);
	ClassDB::bind_method(D_METHOD("set_point_count", "count"), &Curve2D::set_point_count);
	ClassDB::bind_method(D_METHOD("add_point", "position", "in", "out", "index"), &Curve2D::add_point, DEFVAL(Vector2()), DEFVAL(Vector2()), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("set_point_position", "idx", "position"), &Curve2D::set_point_position);
	ClassDB::bind_method(D_METHOD("get_point_position", "idx"), &Curve2D::get_point_position);
	ClassDB::bind_method(D_METHOD("set_point_in", "idx", "position"), &Curve2D::set_point_in);
	ClassDB::bind_method(D_METHOD("get_point_in", "idx"), &Curve2D::get_point_in);
	ClassDB::bind_method(D_METHOD("set_point_out", "idx", "position"), &Curve2D::set_point_out);
	ClassDB::bind_method(D_METHOD("get_point_out", "idx"), &Curve2D::get_point_out);
	ClassDB::bind_method(D_METHOD("remove_point", "idx"), &Curve2D::remove_point);
	ClassDB::bind_method(D_METHOD("clear_points"), &Curve2D::clear_points);
	ClassDB::bind_method(D_METHOD("sample", "idx", "t"), &Curve2D::sample);
	ClassDB::bind_method(D_METHOD("samplef", "fofs"), &Curve2D::samplef);
	//ClassDB::bind_method(D_METHOD("bake","subdivs"),&Curve2D::bake,DEFVAL(10));
	ClassDB::bind_method(D_METHOD("set_bake_interval", "distance"), &Curve2D::set_bake_interval);
	ClassDB::bind_method(D_METHOD("get_bake_interval"), &Curve2D::get_bake_interval);

	ClassDB::bind_method(D_METHOD("get_baked_length"), &Curve2D::get_baked_length);
	ClassDB::bind_method(D_METHOD("sample_baked", "offset", "cubic"), &Curve2D::sample_baked, DEFVAL(0.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("sample_baked_with_rotation", "offset", "cubic"), &Curve2D::sample_baked_with_rotation, DEFVAL(0.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_baked_points"), &Curve2D::get_baked_points);
	ClassDB::bind_method(D_METHOD("get_closest_point", "to_point"), &Curve2D::get_closest_point);
	ClassDB::bind_method(D_METHOD("get_closest_offset", "to_point"), &Curve2D::get_closest_offset);
	ClassDB::bind_method(D_METHOD("tessellate", "max_stages", "tolerance_degrees"), &Curve2D::tessellate, DEFVAL(5), DEFVAL(4));
	ClassDB::bind_method(D_METHOD("tessellate_even_length", "max_stages", "tolerance_length"), &Curve2D::tessellate_even_length, DEFVAL(5), DEFVAL(20.0));

	ClassDB::bind_method(D_METHOD("_get_data"), &Curve2D::_get_data);
	ClassDB::bind_method(D_METHOD("_set_data", "data"), &Curve2D::_set_data);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_interval", PROPERTY_HINT_RANGE, "0.01,512,0.01"), "set_bake_interval", "get_bake_interval");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_data", "_get_data");
	ADD_ARRAY_COUNT("Points", "point_count", "set_point_count", "get_point_count", "point_");
}

Curve2D::Curve2D() {}

/***********************************************************************************/
/***********************************************************************************/
/***********************************************************************************/
/***********************************************************************************/
/***********************************************************************************/
/***********************************************************************************/

int Curve3D::get_point_count() const {
	return points.size();
}

void Curve3D::set_point_count(int p_count) {
	ERR_FAIL_COND(p_count < 0);
	int old_size = points.size();
	if (old_size == p_count) {
		return;
	}

	if (old_size > p_count) {
		points.resize(p_count);
		mark_dirty();
	} else {
		for (int i = p_count - old_size; i > 0; i--) {
			_add_point(Vector3());
		}
	}
	notify_property_list_changed();
}

void Curve3D::_add_point(const Vector3 &p_position, const Vector3 &p_in, const Vector3 &p_out, int p_atpos) {
	Point n;
	n.position = p_position;
	n.in = p_in;
	n.out = p_out;
	if (p_atpos >= 0 && p_atpos < points.size()) {
		points.insert(p_atpos, n);
	} else {
		points.push_back(n);
	}

	mark_dirty();
}

void Curve3D::add_point(const Vector3 &p_position, const Vector3 &p_in, const Vector3 &p_out, int p_atpos) {
	_add_point(p_position, p_in, p_out, p_atpos);
	notify_property_list_changed();
}

void Curve3D::set_point_position(int p_index, const Vector3 &p_position) {
	ERR_FAIL_INDEX(p_index, points.size());

	points.write[p_index].position = p_position;
	mark_dirty();
}

Vector3 Curve3D::get_point_position(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, points.size(), Vector3());
	return points[p_index].position;
}

void Curve3D::set_point_tilt(int p_index, real_t p_tilt) {
	ERR_FAIL_INDEX(p_index, points.size());

	points.write[p_index].tilt = p_tilt;
	mark_dirty();
}

real_t Curve3D::get_point_tilt(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, points.size(), 0);
	return points[p_index].tilt;
}

void Curve3D::set_point_in(int p_index, const Vector3 &p_in) {
	ERR_FAIL_INDEX(p_index, points.size());

	points.write[p_index].in = p_in;
	mark_dirty();
}

Vector3 Curve3D::get_point_in(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, points.size(), Vector3());
	return points[p_index].in;
}

void Curve3D::set_point_out(int p_index, const Vector3 &p_out) {
	ERR_FAIL_INDEX(p_index, points.size());

	points.write[p_index].out = p_out;
	mark_dirty();
}

Vector3 Curve3D::get_point_out(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, points.size(), Vector3());
	return points[p_index].out;
}

void Curve3D::_remove_point(int p_index) {
	ERR_FAIL_INDEX(p_index, points.size());
	points.remove_at(p_index);
	mark_dirty();
}

void Curve3D::remove_point(int p_index) {
	_remove_point(p_index);
	if (closed && points.size() < 2) {
		set_closed(false);
	}
	notify_property_list_changed();
}

void Curve3D::clear_points() {
	if (!points.is_empty()) {
		points.clear();
		mark_dirty();
		notify_property_list_changed();
	}
}

Vector3 Curve3D::sample(int p_index, real_t p_offset) const {
	int pc = points.size();
	ERR_FAIL_COND_V(pc == 0, Vector3());

	if (p_index >= pc - 1) {
		if (!closed) {
			return points[pc - 1].position;
		} else {
			p_index = pc - 1;
		}
	} else if (p_index < 0) {
		return points[0].position;
	}

	Vector3 p0 = points[p_index].position;
	Vector3 p1 = p0 + points[p_index].out;
	Vector3 p3, p2;
	if (!closed || p_index < pc - 1) {
		p3 = points[p_index + 1].position;
		p2 = p3 + points[p_index + 1].in;
	} else {
		p3 = points[0].position;
		p2 = p3 + points[0].in;
	}

	return p0.bezier_interpolate(p1, p2, p3, p_offset);
}

Vector3 Curve3D::samplef(real_t p_findex) const {
	if (p_findex < 0) {
		p_findex = 0;
	} else if (p_findex >= points.size()) {
		p_findex = points.size();
	}

	return sample((int)p_findex, Math::fmod(p_findex, (real_t)1.0));
}

void Curve3D::mark_dirty() {
	baked_cache_dirty = true;
	emit_changed();
}

void Curve3D::_bake_segment3d(RBMap<real_t, Vector3> &r_bake, real_t p_begin, real_t p_end, const Vector3 &p_a, const Vector3 &p_out, const Vector3 &p_b, const Vector3 &p_in, int p_depth, int p_max_depth, real_t p_tol) const {
	real_t mp = p_begin + (p_end - p_begin) * 0.5;
	Vector3 beg = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, p_begin);
	Vector3 mid = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, mp);
	Vector3 end = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, p_end);

	Vector3 na = (mid - beg).normalized();
	Vector3 nb = (end - mid).normalized();
	real_t dp = na.dot(nb);

	if (dp < Math::cos(Math::deg_to_rad(p_tol))) {
		r_bake[mp] = mid;
	}
	if (p_depth < p_max_depth) {
		_bake_segment3d(r_bake, p_begin, mp, p_a, p_out, p_b, p_in, p_depth + 1, p_max_depth, p_tol);
		_bake_segment3d(r_bake, mp, p_end, p_a, p_out, p_b, p_in, p_depth + 1, p_max_depth, p_tol);
	}
}

void Curve3D::_bake_segment3d_even_length(RBMap<real_t, Vector3> &r_bake, real_t p_begin, real_t p_end, const Vector3 &p_a, const Vector3 &p_out, const Vector3 &p_b, const Vector3 &p_in, int p_depth, int p_max_depth, real_t p_length) const {
	Vector3 beg = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, p_begin);
	Vector3 end = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, p_end);

	real_t length = beg.distance_to(end);

	if (length > p_length && p_depth < p_max_depth) {
		real_t mp = (p_begin + p_end) * 0.5;
		Vector3 mid = p_a.bezier_interpolate(p_a + p_out, p_b + p_in, p_b, mp);
		r_bake[mp] = mid;

		_bake_segment3d_even_length(r_bake, p_begin, mp, p_a, p_out, p_b, p_in, p_depth + 1, p_max_depth, p_length);
		_bake_segment3d_even_length(r_bake, mp, p_end, p_a, p_out, p_b, p_in, p_depth + 1, p_max_depth, p_length);
	}
}

Vector3 Curve3D::_calculate_tangent(const Vector3 &p_begin, const Vector3 &p_control_1, const Vector3 &p_control_2, const Vector3 &p_end, const real_t p_t) {
	// Handle corner cases.
	if (Math::is_zero_approx(p_t - 0.0f) && p_control_1.is_equal_approx(p_begin)) {
		return (p_end - p_begin).normalized();
	}

	if (Math::is_zero_approx(p_t - 1.0f) && p_control_2.is_equal_approx(p_end)) {
		return (p_end - p_begin).normalized();
	}

	return p_begin.bezier_derivative(p_control_1, p_control_2, p_end, p_t).normalized();
}

void Curve3D::_bake() const {
	if (!baked_cache_dirty) {
		return;
	}

	baked_max_ofs = 0;
	baked_cache_dirty = false;

	if (points.size() == 0) {
#ifdef TOOLS_ENABLED
		points_in_cache.clear();
#endif
		baked_point_cache.clear();
		baked_tilt_cache.clear();
		baked_dist_cache.clear();

		baked_forward_vector_cache.clear();
		baked_up_vector_cache.clear();
		return;
	}

	if (points.size() == 1) {
#ifdef TOOLS_ENABLED
		points_in_cache.resize(1);
		points_in_cache.set(0, 0);
#endif

		baked_point_cache.resize(1);
		baked_point_cache.set(0, points[0].position);
		baked_tilt_cache.resize(1);
		baked_tilt_cache.set(0, points[0].tilt);
		baked_dist_cache.resize(1);
		baked_dist_cache.set(0, 0.0);
		baked_forward_vector_cache.resize(1);
		baked_forward_vector_cache.set(0, Vector3(0.0, 0.0, 1.0));

		if (up_vector_enabled) {
			baked_up_vector_cache.resize(1);
			baked_up_vector_cache.set(0, Vector3(0.0, 1.0, 0.0));
		} else {
			baked_up_vector_cache.clear();
		}

		return;
	}

	// Step 1: Tessellate curve to (almost) even length segments.
	{
		Vector<RBMap<real_t, Vector3>> midpoints = _tessellate_even_length(10, bake_interval);

		const int num_intervals = closed ? points.size() : points.size() - 1;

#ifdef TOOLS_ENABLED
		points_in_cache.resize(closed ? (points.size() + 1) : points.size());
		points_in_cache.set(0, 0);
#endif

		// Point Count: Begins at 1 to account for the last point.
		int pc = 1;
		for (int i = 0; i < num_intervals; i++) {
			pc++;
			pc += midpoints[i].size();
#ifdef TOOLS_ENABLED
			points_in_cache.set(i + 1, pc - 1);
#endif
		}

		baked_point_cache.resize(pc);
		baked_tilt_cache.resize(pc);
		baked_dist_cache.resize(pc);
		baked_forward_vector_cache.resize(pc);

		Vector3 *bpw = baked_point_cache.ptrw();
		real_t *btw = baked_tilt_cache.ptrw();
		Vector3 *bfw = baked_forward_vector_cache.ptrw();

		// Collect positions and sample tilts and tangents for each baked points.
		bpw[0] = points[0].position;
		bfw[0] = _calculate_tangent(points[0].position, points[0].position + points[0].out, points[1].position + points[1].in, points[1].position, 0.0);
		btw[0] = points[0].tilt;
		int pidx = 0;

		for (int i = 0; i < num_intervals; i++) {
			for (const KeyValue<real_t, Vector3> &E : midpoints[i]) {
				pidx++;
				bpw[pidx] = E.value;
				if (!closed || i < num_intervals - 1) {
					bfw[pidx] = _calculate_tangent(points[i].position, points[i].position + points[i].out, points[i + 1].position + points[i + 1].in, points[i + 1].position, E.key);
					btw[pidx] = Math::lerp(points[i].tilt, points[i + 1].tilt, E.key);
				} else {
					bfw[pidx] = _calculate_tangent(points[i].position, points[i].position + points[i].out, points[0].position + points[0].in, points[0].position, E.key);
					btw[pidx] = Math::lerp(points[i].tilt, points[0].tilt, E.key);
				}
			}

			pidx++;
			if (!closed || i < num_intervals - 1) {
				bpw[pidx] = points[i + 1].position;
				bfw[pidx] = _calculate_tangent(points[i].position, points[i].position + points[i].out, points[i + 1].position + points[i + 1].in, points[i + 1].position, 1.0);
				btw[pidx] = points[i + 1].tilt;
			} else {
				bpw[pidx] = points[0].position;
				bfw[pidx] = _calculate_tangent(points[i].position, points[i].position + points[i].out, points[0].position + points[0].in, points[0].position, 1.0);
				btw[pidx] = points[0].tilt;
			}
		}

		// Recalculate the baked distances.
		real_t *bdw = baked_dist_cache.ptrw();
		bdw[0] = 0.0;
		for (int i = 0; i < pc - 1; i++) {
			bdw[i + 1] = bdw[i] + bpw[i].distance_to(bpw[i + 1]);
		}
		baked_max_ofs = bdw[pc - 1];
	}

	if (!up_vector_enabled) {
		baked_up_vector_cache.resize(0);
		return;
	}

	// Step 2: Calculate the up vectors and the whole local reference frame.
	//
	// See Dougan, Carl. "The parallel transport frame." Game Programming Gems 2 (2001): 215-219.
	// for an example discussing about why not the Frenet frame.
	{
		int point_count = baked_point_cache.size();

		baked_up_vector_cache.resize(point_count);
		Vector3 *up_write = baked_up_vector_cache.ptrw();

		const Vector3 *forward_ptr = baked_forward_vector_cache.ptr();
		const Vector3 *points_ptr = baked_point_cache.ptr();

		Basis frame; // X-right, Y-up, -Z-forward.
		Basis frame_prev;

		// Set the initial frame based on Y-up rule.
		{
			Vector3 forward = forward_ptr[0];

			if (abs(forward.dot(Vector3(0, 1, 0))) > 1.0 - UNIT_EPSILON) {
				frame_prev = Basis::looking_at(forward, Vector3(1, 0, 0));
			} else {
				frame_prev = Basis::looking_at(forward, Vector3(0, 1, 0));
			}

			up_write[0] = frame_prev.get_column(1);
		}

		// Calculate the Parallel Transport Frame.
		for (int idx = 1; idx < point_count; idx++) {
			Vector3 forward = forward_ptr[idx];

			Basis rotate;
			rotate.rotate_to_align(-frame_prev.get_column(2), forward);
			frame = rotate * frame_prev;
			frame.orthonormalize(); // Guard against float error accumulation.

			up_write[idx] = frame.get_column(1);
			frame_prev = frame;
		}

		bool is_loop = true;
		// Loop smoothing only applies when the curve is a loop, which means two ends meet, and share forward directions.
		{
			if (!points_ptr[0].is_equal_approx(points_ptr[point_count - 1])) {
				is_loop = false;
			}

			real_t dot = forward_ptr[0].dot(forward_ptr[point_count - 1]);
			if (dot < 1.0 - UNIT_EPSILON) { // Alignment should not be too tight, or it doesn't work for coarse bake interval.
				is_loop = false;
			}
		}

		// Twist up vectors, so that they align at two ends of the curve.
		if (is_loop) {
			const Vector3 up_start = up_write[0];
			const Vector3 up_end = up_write[point_count - 1];

			real_t sign = SIGN(up_end.cross(up_start).dot(forward_ptr[0]));
			real_t full_angle = Quaternion(up_end, up_start).get_angle();

			if (abs(full_angle) < CMP_EPSILON) {
				return;
			} else {
				const real_t *dists = baked_dist_cache.ptr();
				for (int idx = 1; idx < point_count; idx++) {
					const real_t frac = dists[idx] / baked_max_ofs;
					const real_t angle = Math::lerp((real_t)0.0, full_angle, frac);
					Basis twist(forward_ptr[idx] * sign, angle);

					up_write[idx] = twist.xform(up_write[idx]);
				}
			}
		}
	}
}

real_t Curve3D::get_baked_length() const {
	if (baked_cache_dirty) {
		_bake();
	}

	return baked_max_ofs;
}

Curve3D::Interval Curve3D::_find_interval(real_t p_offset) const {
	Interval interval = {
		-1,
		0.0
	};
	ERR_FAIL_COND_V_MSG(baked_cache_dirty, interval, "Backed cache is dirty");

	int pc = baked_point_cache.size();
	ERR_FAIL_COND_V_MSG(pc < 2, interval, "Less than two points in cache");

	int start = 0;
	int end = pc;
	int idx = (end + start) / 2;
	// Binary search to find baked points.
	while (start < idx) {
		real_t offset = baked_dist_cache[idx];
		if (p_offset <= offset) {
			end = idx;
		} else {
			start = idx;
		}
		idx = (end + start) / 2;
	}

	real_t offset_begin = baked_dist_cache[idx];
	real_t offset_end = baked_dist_cache[idx + 1];

	real_t idx_interval = offset_end - offset_begin;
	ERR_FAIL_COND_V_MSG(p_offset < offset_begin || p_offset > offset_end, interval, "Offset out of range.");

	interval.idx = idx;
	if (idx_interval < FLT_EPSILON) {
		interval.frac = 0.5; // For a very short interval, 0.5 is a reasonable choice.
		ERR_FAIL_V_MSG(interval, "Zero length interval.");
	}

	interval.frac = (p_offset - offset_begin) / idx_interval;
	return interval;
}

Vector3 Curve3D::_sample_baked(Interval p_interval, bool p_cubic) const {
	// Assuming p_interval is valid.
	ERR_FAIL_INDEX_V_MSG(p_interval.idx, baked_point_cache.size(), Vector3(), "Invalid interval");

	int idx = p_interval.idx;
	real_t frac = p_interval.frac;

	const Vector3 *r = baked_point_cache.ptr();
	int pc = baked_point_cache.size();

	if (p_cubic) {
		Vector3 pre = idx > 0 ? r[idx - 1] : r[idx];
		Vector3 post = (idx < (pc - 2)) ? r[idx + 2] : r[idx + 1];
		return r[idx].cubic_interpolate(r[idx + 1], pre, post, frac);
	} else {
		return r[idx].lerp(r[idx + 1], frac);
	}
}

real_t Curve3D::_sample_baked_tilt(Interval p_interval) const {
	// Assuming that p_interval is valid.
	ERR_FAIL_INDEX_V_MSG(p_interval.idx, baked_tilt_cache.size(), 0.0, "Invalid interval");

	int idx = p_interval.idx;
	real_t frac = p_interval.frac;

	const real_t *r = baked_tilt_cache.ptr();

	return Math::lerp(r[idx], r[idx + 1], frac);
}

// Internal method for getting posture at a baked point. Assuming caller
// make all safety checks.
Basis Curve3D::_compose_posture(int p_index) const {
	Vector3 forward = baked_forward_vector_cache[p_index];

	Vector3 up;
	if (up_vector_enabled) {
		up = baked_up_vector_cache[p_index];
	} else {
		up = Vector3(0.0, 1.0, 0.0);
	}

	const Basis frame = Basis::looking_at(forward, up);
	return frame;
}

Basis Curve3D::_sample_posture(Interval p_interval, bool p_apply_tilt) const {
	// Assuming that p_interval is valid.
	ERR_FAIL_INDEX_V_MSG(p_interval.idx, baked_point_cache.size(), Basis(), "Invalid interval");
	if (up_vector_enabled) {
		ERR_FAIL_INDEX_V_MSG(p_interval.idx, baked_up_vector_cache.size(), Basis(), "Invalid interval");
	}

	int idx = p_interval.idx;
	real_t frac = p_interval.frac;

	// Get frames at both ends of the interval, then interpolate.
	const Basis frame_begin = _compose_posture(idx);
	const Basis frame_end = _compose_posture(idx + 1);
	const Basis frame = frame_begin.slerp(frame_end, frac).orthonormalized();

	if (!p_apply_tilt) {
		return frame;
	}

	// Applying tilt.
	const real_t tilt = _sample_baked_tilt(p_interval);
	Vector3 tangent = -frame.get_column(2);

	const Basis twist(tangent, tilt);
	return twist * frame;
}

#ifdef TOOLS_ENABLED
// Get posture at a control point. Needed for Gizmo implementation.
Basis Curve3D::get_point_baked_posture(int p_index, bool p_apply_tilt) const {
	if (baked_cache_dirty) {
		_bake();
	}

	// Assuming that p_idx is valid.
	ERR_FAIL_INDEX_V_MSG(p_index, points_in_cache.size(), Basis(), "Invalid control point index");

	int baked_idx = points_in_cache[p_index];
	Basis frame = _compose_posture(baked_idx);

	if (!p_apply_tilt) {
		return frame;
	}

	// Applying tilt.
	const real_t tilt = points[p_index].tilt;
	Vector3 tangent = -frame.get_column(2);
	const Basis twist(tangent, tilt);

	return twist * frame;
}
#endif

Vector3 Curve3D::sample_baked(real_t p_offset, bool p_cubic) const {
	// Make sure that p_offset is finite.
	ERR_FAIL_COND_V_MSG(!Math::is_finite(p_offset), Vector3(), "Offset is non-finite");

	if (baked_cache_dirty) {
		_bake();
	}

	// Validate: Curve may not have baked points.
	int pc = baked_point_cache.size();
	ERR_FAIL_COND_V_MSG(pc == 0, Vector3(), "No points in Curve3D.");

	if (pc == 1) {
		return baked_point_cache[0];
	}

	p_offset = CLAMP(p_offset, 0.0, get_baked_length()); // PathFollower implement wrapping logic.

	Curve3D::Interval interval = _find_interval(p_offset);
	return _sample_baked(interval, p_cubic);
}

Transform3D Curve3D::sample_baked_with_rotation(real_t p_offset, bool p_cubic, bool p_apply_tilt) const {
	// Make sure that p_offset is finite.
	ERR_FAIL_COND_V_MSG(!Math::is_finite(p_offset), Transform3D(), "Offset is non-finite");

	if (baked_cache_dirty) {
		_bake();
	}

	// Validate: Curve may not have baked points.
	const int point_count = baked_point_cache.size();
	ERR_FAIL_COND_V_MSG(point_count == 0, Transform3D(), "No points in Curve3D.");

	if (point_count == 1) {
		Transform3D t;
		t.origin = baked_point_cache.get(0);
		ERR_FAIL_V_MSG(t, "Only 1 point in Curve3D.");
	}

	p_offset = CLAMP(p_offset, 0.0, get_baked_length()); // PathFollower implement wrapping logic.

	// 0. Find interval for all sampling steps.
	Curve3D::Interval interval = _find_interval(p_offset);

	// 1. Sample position.
	Vector3 pos = _sample_baked(interval, p_cubic);

	// 2. Sample rotation frame.
	Basis frame = _sample_posture(interval, p_apply_tilt);

	return Transform3D(frame, pos);
}

real_t Curve3D::sample_baked_tilt(real_t p_offset) const {
	// Make sure that p_offset is finite.
	ERR_FAIL_COND_V_MSG(!Math::is_finite(p_offset), 0, "Offset is non-finite");

	if (baked_cache_dirty) {
		_bake();
	}

	// Validate: Curve may not have baked tilts.
	int pc = baked_tilt_cache.size();
	ERR_FAIL_COND_V_MSG(pc == 0, 0, "No tilts in Curve3D.");

	if (pc == 1) {
		return baked_tilt_cache.get(0);
	}

	p_offset = CLAMP(p_offset, 0.0, get_baked_length()); // PathFollower implement wrapping logic.

	Curve3D::Interval interval = _find_interval(p_offset);
	return _sample_baked_tilt(interval);
}

Vector3 Curve3D::sample_baked_up_vector(real_t p_offset, bool p_apply_tilt) const {
	// Make sure that p_offset is finite.
	ERR_FAIL_COND_V_MSG(!Math::is_finite(p_offset), Vector3(0, 1, 0), "Offset is non-finite");

	if (baked_cache_dirty) {
		_bake();
	}

	// Validate: Curve may not have baked up vectors.
	ERR_FAIL_COND_V_MSG(!up_vector_enabled, Vector3(0, 1, 0), "No up vectors in Curve3D.");

	int count = baked_up_vector_cache.size();
	if (count == 1) {
		return baked_up_vector_cache.get(0);
	}

	p_offset = CLAMP(p_offset, 0.0, get_baked_length()); // PathFollower implement wrapping logic.

	Curve3D::Interval interval = _find_interval(p_offset);
	return _sample_posture(interval, p_apply_tilt).get_column(1);
}

PackedVector3Array Curve3D::get_baked_points() const {
	if (baked_cache_dirty) {
		_bake();
	}

	return baked_point_cache;
}

Vector<real_t> Curve3D::get_baked_tilts() const {
	if (baked_cache_dirty) {
		_bake();
	}

	return baked_tilt_cache;
}

PackedVector3Array Curve3D::get_baked_up_vectors() const {
	if (baked_cache_dirty) {
		_bake();
	}

	return baked_up_vector_cache;
}

Vector3 Curve3D::get_closest_point(const Vector3 &p_to_point) const {
	// Brute force method.

	if (baked_cache_dirty) {
		_bake();
	}

	// Validate: Curve may not have baked points.
	int pc = baked_point_cache.size();
	ERR_FAIL_COND_V_MSG(pc == 0, Vector3(), "No points in Curve3D.");

	if (pc == 1) {
		return baked_point_cache.get(0);
	}

	const Vector3 *r = baked_point_cache.ptr();

	Vector3 nearest;
	real_t nearest_dist = -1.0f;

	for (int i = 0; i < pc - 1; i++) {
		const real_t interval = baked_dist_cache[i + 1] - baked_dist_cache[i];
		Vector3 origin = r[i];
		Vector3 direction = (r[i + 1] - origin) / interval;

		real_t d = CLAMP((p_to_point - origin).dot(direction), 0.0f, interval);
		Vector3 proj = origin + direction * d;

		real_t dist = proj.distance_squared_to(p_to_point);

		if (nearest_dist < 0.0f || dist < nearest_dist) {
			nearest = proj;
			nearest_dist = dist;
		}
	}

	return nearest;
}

PackedVector3Array Curve3D::get_points() const {
	return _get_data()["points"];
}

real_t Curve3D::get_closest_offset(const Vector3 &p_to_point) const {
	// Brute force method.

	if (baked_cache_dirty) {
		_bake();
	}

	// Validate: Curve may not have baked points.
	int pc = baked_point_cache.size();
	ERR_FAIL_COND_V_MSG(pc == 0, 0.0f, "No points in Curve3D.");

	if (pc == 1) {
		return 0.0f;
	}

	const Vector3 *r = baked_point_cache.ptr();

	real_t nearest = 0.0f;
	real_t nearest_dist = -1.0f;
	real_t offset;

	for (int i = 0; i < pc - 1; i++) {
		offset = baked_dist_cache[i];

		const real_t interval = baked_dist_cache[i + 1] - baked_dist_cache[i];
		Vector3 origin = r[i];
		Vector3 direction = (r[i + 1] - origin) / interval;

		real_t d = CLAMP((p_to_point - origin).dot(direction), 0.0f, interval);
		Vector3 proj = origin + direction * d;

		real_t dist = proj.distance_squared_to(p_to_point);

		if (nearest_dist < 0.0f || dist < nearest_dist) {
			nearest = offset + d;
			nearest_dist = dist;
		}
	}

	return nearest;
}

void Curve3D::set_closed(bool p_closed) {
	if (closed == p_closed) {
		return;
	}

	closed = p_closed;
	mark_dirty();
	notify_property_list_changed();
}

bool Curve3D::is_closed() const {
	return closed;
}

void Curve3D::set_bake_interval(real_t p_tolerance) {
	bake_interval = p_tolerance;
	mark_dirty();
}

real_t Curve3D::get_bake_interval() const {
	return bake_interval;
}

void Curve3D::set_up_vector_enabled(bool p_enable) {
	up_vector_enabled = p_enable;
	mark_dirty();
}

bool Curve3D::is_up_vector_enabled() const {
	return up_vector_enabled;
}

Dictionary Curve3D::_get_data() const {
	Dictionary dc;

	PackedVector3Array d;
	d.resize(points.size() * 3);
	Vector3 *w = d.ptrw();
	Vector<real_t> t;
	t.resize(points.size());
	real_t *wt = t.ptrw();

	for (int i = 0; i < points.size(); i++) {
		w[i * 3 + 0] = points[i].in;
		w[i * 3 + 1] = points[i].out;
		w[i * 3 + 2] = points[i].position;
		wt[i] = points[i].tilt;
	}

	dc["points"] = d;
	dc["tilts"] = t;

	return dc;
}

void Curve3D::_set_data(const Dictionary &p_data) {
	ERR_FAIL_COND(!p_data.has("points"));
	ERR_FAIL_COND(!p_data.has("tilts"));

	PackedVector3Array rp = p_data["points"];
	int pc = rp.size();
	ERR_FAIL_COND(pc % 3 != 0);
	int old_size = points.size();
	int new_size = pc / 3;
	if (old_size != new_size) {
		points.resize(new_size);
	}
	const Vector3 *r = rp.ptr();
	Vector<real_t> rtl = p_data["tilts"];
	const real_t *rt = rtl.ptr();

	for (int i = 0; i < points.size(); i++) {
		points.write[i].in = r[i * 3 + 0];
		points.write[i].out = r[i * 3 + 1];
		points.write[i].position = r[i * 3 + 2];
		points.write[i].tilt = rt[i];
	}

	mark_dirty();
	if (old_size != new_size) {
		notify_property_list_changed();
	}
}

PackedVector3Array Curve3D::tessellate(int p_max_stages, real_t p_tolerance) const {
	PackedVector3Array tess;

	if (points.size() == 0) {
		return tess;
	}
	Vector<RBMap<real_t, Vector3>> midpoints;

	const int num_intervals = closed ? points.size() : points.size() - 1;
	midpoints.resize(num_intervals);

	// Point Count: Begins at 1 to account for the last point.
	int pc = 1;
	for (int i = 0; i < num_intervals; i++) {
		if (!closed || i < num_intervals - 1) {
			_bake_segment3d(midpoints.write[i], 0, 1, points[i].position, points[i].out, points[i + 1].position, points[i + 1].in, 0, p_max_stages, p_tolerance);
		} else {
			_bake_segment3d(midpoints.write[i], 0, 1, points[i].position, points[i].out, points[0].position, points[0].in, 0, p_max_stages, p_tolerance);
		}
		pc++;
		pc += midpoints[i].size();
	}

	tess.resize(pc);
	Vector3 *bpw = tess.ptrw();
	bpw[0] = points[0].position;
	int pidx = 0;

	for (int i = 0; i < num_intervals; i++) {
		for (const KeyValue<real_t, Vector3> &E : midpoints[i]) {
			pidx++;
			bpw[pidx] = E.value;
		}

		pidx++;
		if (!closed || i < num_intervals - 1) {
			bpw[pidx] = points[i + 1].position;
		} else {
			bpw[pidx] = points[0].position;
		}
	}

	return tess;
}

Vector<RBMap<real_t, Vector3>> Curve3D::_tessellate_even_length(int p_max_stages, real_t p_length) const {
	Vector<RBMap<real_t, Vector3>> midpoints;
	ERR_FAIL_COND_V_MSG(points.size() < 2, midpoints, "Curve must have at least 2 control point");

	const int num_intervals = closed ? points.size() : points.size() - 1;
	midpoints.resize(num_intervals);

	for (int i = 0; i < num_intervals; i++) {
		if (!closed || i < num_intervals - 1) {
			_bake_segment3d_even_length(midpoints.write[i], 0, 1, points[i].position, points[i].out, points[i + 1].position, points[i + 1].in, 0, p_max_stages, p_length);
		} else {
			_bake_segment3d_even_length(midpoints.write[i], 0, 1, points[i].position, points[i].out, points[0].position, points[0].in, 0, p_max_stages, p_length);
		}
	}
	return midpoints;
}

PackedVector3Array Curve3D::tessellate_even_length(int p_max_stages, real_t p_length) const {
	PackedVector3Array tess;

	Vector<RBMap<real_t, Vector3>> midpoints = _tessellate_even_length(p_max_stages, p_length);
	if (midpoints.size() == 0) {
		return tess;
	}

	const int num_intervals = closed ? points.size() : points.size() - 1;
	// Point Count: Begins at 1 to account for the last point.
	int pc = 1;
	for (int i = 0; i < num_intervals; i++) {
		pc++;
		pc += midpoints[i].size();
	}

	tess.resize(pc);
	Vector3 *bpw = tess.ptrw();
	bpw[0] = points[0].position;
	int pidx = 0;

	for (int i = 0; i < num_intervals; i++) {
		for (const KeyValue<real_t, Vector3> &E : midpoints[i]) {
			pidx++;
			bpw[pidx] = E.value;
		}

		pidx++;
		if (!closed || i < num_intervals - 1) {
			bpw[pidx] = points[i + 1].position;
		} else {
			bpw[pidx] = points[0].position;
		}
	}

	return tess;
}

bool Curve3D::_set(const StringName &p_name, const Variant &p_value) {
	Vector<String> components = String(p_name).split("/", true, 2);
	if (components.size() >= 2 && components[0].begins_with("point_") && components[0].trim_prefix("point_").is_valid_int()) {
		int point_index = components[0].trim_prefix("point_").to_int();
		const String &property = components[1];
		if (property == "position") {
			set_point_position(point_index, p_value);
			return true;
		} else if (property == "in") {
			set_point_in(point_index, p_value);
			return true;
		} else if (property == "out") {
			set_point_out(point_index, p_value);
			return true;
		} else if (property == "tilt") {
			set_point_tilt(point_index, p_value);
			return true;
		}
	}
	return false;
}

bool Curve3D::_get(const StringName &p_name, Variant &r_ret) const {
	Vector<String> components = String(p_name).split("/", true, 2);
	if (components.size() >= 2 && components[0].begins_with("point_") && components[0].trim_prefix("point_").is_valid_int()) {
		int point_index = components[0].trim_prefix("point_").to_int();
		const String &property = components[1];
		if (property == "position") {
			r_ret = get_point_position(point_index);
			return true;
		} else if (property == "in") {
			r_ret = get_point_in(point_index);
			return true;
		} else if (property == "out") {
			r_ret = get_point_out(point_index);
			return true;
		} else if (property == "tilt") {
			r_ret = get_point_tilt(point_index);
			return true;
		}
	}
	return false;
}

void Curve3D::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < points.size(); i++) {
		PropertyInfo pi = PropertyInfo(Variant::VECTOR3, vformat("point_%d/position", i));
		pi.usage &= ~PROPERTY_USAGE_STORAGE;
		p_list->push_back(pi);

		if (closed || i != 0) {
			pi = PropertyInfo(Variant::VECTOR3, vformat("point_%d/in", i));
			pi.usage &= ~PROPERTY_USAGE_STORAGE;
			p_list->push_back(pi);
		}

		if (closed || i != points.size() - 1) {
			pi = PropertyInfo(Variant::VECTOR3, vformat("point_%d/out", i));
			pi.usage &= ~PROPERTY_USAGE_STORAGE;
			p_list->push_back(pi);
		}

		pi = PropertyInfo(Variant::FLOAT, vformat("point_%d/tilt", i));
		pi.usage &= ~PROPERTY_USAGE_STORAGE;
		p_list->push_back(pi);
	}
}

void Curve3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_point_count"), &Curve3D::get_point_count);
	ClassDB::bind_method(D_METHOD("set_point_count", "count"), &Curve3D::set_point_count);
	ClassDB::bind_method(D_METHOD("add_point", "position", "in", "out", "index"), &Curve3D::add_point, DEFVAL(Vector3()), DEFVAL(Vector3()), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("set_point_position", "idx", "position"), &Curve3D::set_point_position);
	ClassDB::bind_method(D_METHOD("get_point_position", "idx"), &Curve3D::get_point_position);
	ClassDB::bind_method(D_METHOD("set_point_tilt", "idx", "tilt"), &Curve3D::set_point_tilt);
	ClassDB::bind_method(D_METHOD("get_point_tilt", "idx"), &Curve3D::get_point_tilt);
	ClassDB::bind_method(D_METHOD("set_point_in", "idx", "position"), &Curve3D::set_point_in);
	ClassDB::bind_method(D_METHOD("get_point_in", "idx"), &Curve3D::get_point_in);
	ClassDB::bind_method(D_METHOD("set_point_out", "idx", "position"), &Curve3D::set_point_out);
	ClassDB::bind_method(D_METHOD("get_point_out", "idx"), &Curve3D::get_point_out);
	ClassDB::bind_method(D_METHOD("remove_point", "idx"), &Curve3D::remove_point);
	ClassDB::bind_method(D_METHOD("clear_points"), &Curve3D::clear_points);
	ClassDB::bind_method(D_METHOD("sample", "idx", "t"), &Curve3D::sample);
	ClassDB::bind_method(D_METHOD("samplef", "fofs"), &Curve3D::samplef);
	ClassDB::bind_method(D_METHOD("set_closed", "closed"), &Curve3D::set_closed);
	ClassDB::bind_method(D_METHOD("is_closed"), &Curve3D::is_closed);
	//ClassDB::bind_method(D_METHOD("bake","subdivs"),&Curve3D::bake,DEFVAL(10));
	ClassDB::bind_method(D_METHOD("set_bake_interval", "distance"), &Curve3D::set_bake_interval);
	ClassDB::bind_method(D_METHOD("get_bake_interval"), &Curve3D::get_bake_interval);
	ClassDB::bind_method(D_METHOD("set_up_vector_enabled", "enable"), &Curve3D::set_up_vector_enabled);
	ClassDB::bind_method(D_METHOD("is_up_vector_enabled"), &Curve3D::is_up_vector_enabled);

	ClassDB::bind_method(D_METHOD("get_baked_length"), &Curve3D::get_baked_length);
	ClassDB::bind_method(D_METHOD("sample_baked", "offset", "cubic"), &Curve3D::sample_baked, DEFVAL(0.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("sample_baked_with_rotation", "offset", "cubic", "apply_tilt"), &Curve3D::sample_baked_with_rotation, DEFVAL(0.0), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("sample_baked_up_vector", "offset", "apply_tilt"), &Curve3D::sample_baked_up_vector, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_baked_points"), &Curve3D::get_baked_points);
	ClassDB::bind_method(D_METHOD("get_baked_tilts"), &Curve3D::get_baked_tilts);
	ClassDB::bind_method(D_METHOD("get_baked_up_vectors"), &Curve3D::get_baked_up_vectors);
	ClassDB::bind_method(D_METHOD("get_closest_point", "to_point"), &Curve3D::get_closest_point);
	ClassDB::bind_method(D_METHOD("get_closest_offset", "to_point"), &Curve3D::get_closest_offset);
	ClassDB::bind_method(D_METHOD("tessellate", "max_stages", "tolerance_degrees"), &Curve3D::tessellate, DEFVAL(5), DEFVAL(4));
	ClassDB::bind_method(D_METHOD("tessellate_even_length", "max_stages", "tolerance_length"), &Curve3D::tessellate_even_length, DEFVAL(5), DEFVAL(0.2));

	ClassDB::bind_method(D_METHOD("_get_data"), &Curve3D::_get_data);
	ClassDB::bind_method(D_METHOD("_set_data", "data"), &Curve3D::_set_data);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "closed"), "set_closed", "is_closed");

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_interval", PROPERTY_HINT_RANGE, "0.01,512,0.01"), "set_bake_interval", "get_bake_interval");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_data", "_get_data");
	ADD_ARRAY_COUNT("Points", "point_count", "set_point_count", "get_point_count", "point_");

	ADD_GROUP("Up Vector", "up_vector_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "up_vector_enabled"), "set_up_vector_enabled", "is_up_vector_enabled");
}

Curve3D::Curve3D() {}
