/**************************************************************************/
/*  curve.h                                                               */
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

#ifndef CURVE_H
#define CURVE_H

#include "core/io/resource.h"

// y(x) curve
class Curve : public Resource {
	GDCLASS(Curve, Resource);

public:
	static const char *SIGNAL_RANGE_CHANGED;
	static const char *SIGNAL_DOMAIN_CHANGED;

	enum TangentMode {
		TANGENT_FREE = 0,
		TANGENT_LINEAR,
		TANGENT_MODE_COUNT
	};

	struct Point {
		Vector2 position;
		real_t left_tangent = 0.0;
		real_t right_tangent = 0.0;
		TangentMode left_mode = TANGENT_FREE;
		TangentMode right_mode = TANGENT_FREE;

		Point() {
		}

		Point(const Vector2 &p_position,
				real_t p_left = 0.0,
				real_t p_right = 0.0,
				TangentMode p_left_mode = TANGENT_FREE,
				TangentMode p_right_mode = TANGENT_FREE) {
			position = p_position;
			left_tangent = p_left;
			right_tangent = p_right;
			left_mode = p_left_mode;
			right_mode = p_right_mode;
		}
	};

	Curve();

	int get_point_count() const { return _points.size(); }

	void set_point_count(int p_count);

	int add_point(Vector2 p_position,
			real_t left_tangent = 0,
			real_t right_tangent = 0,
			TangentMode left_mode = TANGENT_FREE,
			TangentMode right_mode = TANGENT_FREE);
	int add_point_no_update(Vector2 p_position,
			real_t left_tangent = 0,
			real_t right_tangent = 0,
			TangentMode left_mode = TANGENT_FREE,
			TangentMode right_mode = TANGENT_FREE);
	void remove_point(int p_index);
	void clear_points();

	int get_index(real_t p_offset) const;

	void set_point_value(int p_index, real_t p_position);
	int set_point_offset(int p_index, real_t p_offset);
	Vector2 get_point_position(int p_index) const;

	Point get_point(int p_index) const;

	real_t get_min_value() const { return _min_value; }
	void set_min_value(real_t p_min);
	real_t get_max_value() const { return _max_value; }
	void set_max_value(real_t p_max);
	real_t get_value_range() const { return _max_value - _min_value; }

	real_t get_min_domain() const { return _min_domain; }
	void set_min_domain(real_t p_min);
	real_t get_max_domain() const { return _max_domain; }
	void set_max_domain(real_t p_max);
	real_t get_domain_range() const { return _max_domain - _min_domain; }

	Array get_limits() const;
	void set_limits(const Array &p_input);

	real_t sample(real_t p_offset) const;
	real_t sample_local_nocheck(int p_index, real_t p_local_offset) const;

	void clean_dupes();

	void set_point_left_tangent(int p_index, real_t p_tangent);
	void set_point_right_tangent(int p_index, real_t p_tangent);
	void set_point_left_mode(int p_index, TangentMode p_mode);
	void set_point_right_mode(int p_index, TangentMode p_mode);

	real_t get_point_left_tangent(int p_index) const;
	real_t get_point_right_tangent(int p_index) const;
	TangentMode get_point_left_mode(int p_index) const;
	TangentMode get_point_right_mode(int p_index) const;

	void update_auto_tangents(int i);

	Array get_data() const;
	void set_data(Array input);

	void bake();
	void _bake() const;
	int get_bake_resolution() const { return _bake_resolution; }
	void set_bake_resolution(int p_resolution);
	real_t sample_baked(real_t p_offset) const;

	void ensure_default_setup(real_t p_min, real_t p_max);

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

protected:
	static void _bind_methods();

private:
	void mark_dirty();
	int _add_point(Vector2 p_position,
			real_t left_tangent = 0,
			real_t right_tangent = 0,
			TangentMode left_mode = TANGENT_FREE,
			TangentMode right_mode = TANGENT_FREE);
	void _remove_point(int p_index);

	LocalVector<Point> _points;
	mutable bool _baked_cache_dirty = false;
	mutable Vector<real_t> _baked_cache;
	int _bake_resolution = 100;
	real_t _min_value = 0.0;
	real_t _max_value = 1.0;
	real_t _min_domain = 0.0;
	real_t _max_domain = 1.0;
};

VARIANT_ENUM_CAST(Curve::TangentMode)

class Curve2D : public Resource {
	GDCLASS(Curve2D, Resource);

	struct Point {
		Vector2 in;
		Vector2 out;
		Vector2 position;
	};

	LocalVector<Point> points;

	struct BakedPoint {
		real_t ofs = 0.0;
		Vector2 point;
	};

	mutable bool baked_cache_dirty = false;
	mutable PackedVector2Array baked_point_cache;
	mutable PackedVector2Array baked_forward_vector_cache;
	mutable Vector<real_t> baked_dist_cache;
	mutable real_t baked_max_ofs = 0.0;

	void mark_dirty();

	static Vector2 _calculate_tangent(const Vector2 &p_begin, const Vector2 &p_control_1, const Vector2 &p_control_2, const Vector2 &p_end, const real_t p_t);
	void _bake() const;

	real_t bake_interval = 5.0;

	struct Interval {
		int idx;
		real_t frac;
	};
	Interval _find_interval(real_t p_offset) const;
	Vector2 _sample_baked(Interval p_interval, bool p_cubic) const;
	Transform2D _sample_posture(Interval p_interval) const;

	void _bake_segment2d(RBMap<real_t, Vector2> &r_bake, real_t p_begin, real_t p_end, const Vector2 &p_a, const Vector2 &p_out, const Vector2 &p_b, const Vector2 &p_in, int p_depth, int p_max_depth, real_t p_tol) const;
	void _bake_segment2d_even_length(RBMap<real_t, Vector2> &r_bake, real_t p_begin, real_t p_end, const Vector2 &p_a, const Vector2 &p_out, const Vector2 &p_b, const Vector2 &p_in, int p_depth, int p_max_depth, real_t p_length) const;
	Dictionary _get_data() const;
	void _set_data(const Dictionary &p_data);

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _add_point(const Vector2 &p_position, const Vector2 &p_in = Vector2(), const Vector2 &p_out = Vector2(), int p_atpos = -1);
	void _remove_point(int p_index);

	Vector<RBMap<real_t, Vector2>> _tessellate_even_length(int p_max_stages = 5, real_t p_length = 0.2) const;

protected:
	static void _bind_methods();

public:
	int get_point_count() const;
	void set_point_count(int p_count);
	void add_point(const Vector2 &p_position, const Vector2 &p_in = Vector2(), const Vector2 &p_out = Vector2(), int p_atpos = -1);
	void set_point_position(int p_index, const Vector2 &p_position);
	Vector2 get_point_position(int p_index) const;
	void set_point_in(int p_index, const Vector2 &p_in);
	Vector2 get_point_in(int p_index) const;
	void set_point_out(int p_index, const Vector2 &p_out);
	Vector2 get_point_out(int p_index) const;
	void remove_point(int p_index);
	void clear_points();

	Vector2 sample(int p_index, real_t p_offset) const;
	Vector2 samplef(real_t p_findex) const;

	void set_bake_interval(real_t p_tolerance);
	real_t get_bake_interval() const;

	real_t get_baked_length() const;
	Vector2 sample_baked(real_t p_offset, bool p_cubic = false) const;
	Transform2D sample_baked_with_rotation(real_t p_offset, bool p_cubic = false) const;
	PackedVector2Array get_points() const;
	PackedVector2Array get_baked_points() const; //useful for going through
	Vector2 get_closest_point(const Vector2 &p_to_point) const;
	real_t get_closest_offset(const Vector2 &p_to_point) const;

	PackedVector2Array tessellate(int p_max_stages = 5, real_t p_tolerance = 4) const; //useful for display
	PackedVector2Array tessellate_even_length(int p_max_stages = 5, real_t p_length = 20.0) const; // Useful for baking.

	Curve2D();
};

class Curve3D : public Resource {
	GDCLASS(Curve3D, Resource);

	struct Point {
		Vector3 in;
		Vector3 out;
		Vector3 position;
		real_t tilt = 0.0;
	};

	LocalVector<Point> points;
#ifdef TOOLS_ENABLED
	// For Path3DGizmo.
	mutable Vector<size_t> points_in_cache;
#endif

	bool closed = false;

	mutable bool baked_cache_dirty = false;
	mutable PackedVector3Array baked_point_cache;
	mutable Vector<real_t> baked_tilt_cache;
	mutable PackedVector3Array baked_up_vector_cache;
	mutable PackedVector3Array baked_forward_vector_cache;
	mutable Vector<real_t> baked_dist_cache;
	mutable real_t baked_max_ofs = 0.0;

	void mark_dirty();

	static Vector3 _calculate_tangent(const Vector3 &p_begin, const Vector3 &p_control_1, const Vector3 &p_control_2, const Vector3 &p_end, const real_t p_t);
	void _bake() const;

	struct Interval {
		int idx;
		real_t frac;
	};
	Interval _find_interval(real_t p_offset) const;
	Vector3 _sample_baked(Interval p_interval, bool p_cubic) const;
	real_t _sample_baked_tilt(Interval p_interval) const;
	Basis _sample_posture(Interval p_interval, bool p_apply_tilt = false) const;
	Basis _compose_posture(int p_index) const;

	real_t bake_interval = 0.2;
	bool up_vector_enabled = true;

	void _bake_segment3d(RBMap<real_t, Vector3> &r_bake, real_t p_begin, real_t p_end, const Vector3 &p_a, const Vector3 &p_out, const Vector3 &p_b, const Vector3 &p_in, int p_depth, int p_max_depth, real_t p_tol) const;
	void _bake_segment3d_even_length(RBMap<real_t, Vector3> &r_bake, real_t p_begin, real_t p_end, const Vector3 &p_a, const Vector3 &p_out, const Vector3 &p_b, const Vector3 &p_in, int p_depth, int p_max_depth, real_t p_length) const;
	Dictionary _get_data() const;
	void _set_data(const Dictionary &p_data);

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	void _add_point(const Vector3 &p_position, const Vector3 &p_in = Vector3(), const Vector3 &p_out = Vector3(), int p_atpos = -1);
	void _remove_point(int p_index);

	Vector<RBMap<real_t, Vector3>> _tessellate_even_length(int p_max_stages = 5, real_t p_length = 0.2) const;

protected:
	static void _bind_methods();

public:
#ifdef TOOLS_ENABLED
	// For Path3DGizmo.
	Basis get_point_baked_posture(int p_index, bool p_apply_tilt = false) const;
#endif

	int get_point_count() const;
	void set_point_count(int p_count);
	void add_point(const Vector3 &p_position, const Vector3 &p_in = Vector3(), const Vector3 &p_out = Vector3(), int p_atpos = -1);
	void set_point_position(int p_index, const Vector3 &p_position);
	Vector3 get_point_position(int p_index) const;
	void set_point_tilt(int p_index, real_t p_tilt);
	real_t get_point_tilt(int p_index) const;
	void set_point_in(int p_index, const Vector3 &p_in);
	Vector3 get_point_in(int p_index) const;
	void set_point_out(int p_index, const Vector3 &p_out);
	Vector3 get_point_out(int p_index) const;
	void remove_point(int p_index);
	void clear_points();

	Vector3 sample(int p_index, real_t p_offset) const;
	Vector3 samplef(real_t p_findex) const;

	void set_closed(bool p_closed);
	bool is_closed() const;
	void set_bake_interval(real_t p_tolerance);
	real_t get_bake_interval() const;
	void set_up_vector_enabled(bool p_enable);
	bool is_up_vector_enabled() const;

	real_t get_baked_length() const;
	Vector3 sample_baked(real_t p_offset, bool p_cubic = false) const;
	Transform3D sample_baked_with_rotation(real_t p_offset, bool p_cubic = false, bool p_apply_tilt = false) const;
	real_t sample_baked_tilt(real_t p_offset) const;
	Vector3 sample_baked_up_vector(real_t p_offset, bool p_apply_tilt = false) const;
	PackedVector3Array get_baked_points() const; // Useful for going through.
	Vector<real_t> get_baked_tilts() const; //useful for going through
	PackedVector3Array get_baked_up_vectors() const;
	Vector3 get_closest_point(const Vector3 &p_to_point) const;
	real_t get_closest_offset(const Vector3 &p_to_point) const;
	PackedVector3Array get_points() const;

	PackedVector3Array tessellate(int p_max_stages = 5, real_t p_tolerance = 4) const; // Useful for display.
	PackedVector3Array tessellate_even_length(int p_max_stages = 5, real_t p_length = 0.2) const; // Useful for baking.

	Curve3D();
};

#endif // CURVE_H
