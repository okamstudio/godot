/**************************************************************************/
/*  rect2.h                                                               */
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

#include "core/error/error_macros.h"
#include "core/math/vector2.h"

class String;
struct Rect2i;
struct Transform2D;

struct [[nodiscard]] Rect2 {
	Point2 position;
	Size2 size;

	const Vector2 &get_position() const { return position; }
	void set_position(const Vector2 &p_pos) { position = p_pos; }
	const Vector2 &get_size() const { return size; }
	void set_size(const Vector2 &p_size) { size = p_size; }

	real_t get_area() const { return size.width * size.height; }

	_FORCE_INLINE_ Vector2 get_center() const { return position + (size * 0.5f); }

	inline bool intersects(const Rect2 &p_rect, bool p_include_borders = false) const {
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0 || p_rect.size.x < 0 || p_rect.size.y < 0)) {
			ERR_PRINT("Rect2 size is negative, this is not supported. Use Rect2.abs() to get a Rect2 with a positive size.");
		}
#endif
		if (p_include_borders) {
			if (position.x > (p_rect.position.x + p_rect.size.width)) {
				return false;
			}
			if ((position.x + size.width) < p_rect.position.x) {
				return false;
			}
			if (position.y > (p_rect.position.y + p_rect.size.height)) {
				return false;
			}
			if ((position.y + size.height) < p_rect.position.y) {
				return false;
			}
		} else {
			if (position.x >= (p_rect.position.x + p_rect.size.width)) {
				return false;
			}
			if ((position.x + size.width) <= p_rect.position.x) {
				return false;
			}
			if (position.y >= (p_rect.position.y + p_rect.size.height)) {
				return false;
			}
			if ((position.y + size.height) <= p_rect.position.y) {
				return false;
			}
		}

		return true;
	}

	inline real_t distance_to(const Vector2 &p_point) const {
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0)) {
			ERR_PRINT("Rect2 size is negative, this is not supported. Use Rect2.abs() to get a Rect2 with a positive size.");
		}
#endif
		real_t dist = 0.0;
		bool inside = true;

		if (p_point.x < position.x) {
			real_t d = position.x - p_point.x;
			dist = d;
			inside = false;
		}
		if (p_point.y < position.y) {
			real_t d = position.y - p_point.y;
			dist = inside ? d : MIN(dist, d);
			inside = false;
		}
		if (p_point.x >= (position.x + size.x)) {
			real_t d = p_point.x - (position.x + size.x);
			dist = inside ? d : MIN(dist, d);
			inside = false;
		}
		if (p_point.y >= (position.y + size.y)) {
			real_t d = p_point.y - (position.y + size.y);
			dist = inside ? d : MIN(dist, d);
			inside = false;
		}

		if (inside) {
			return 0;
		} else {
			return dist;
		}
	}

	bool intersects_transformed(const Transform2D &p_xform, const Rect2 &p_rect) const;

	bool intersects_segment(const Point2 &p_from, const Point2 &p_to, Point2 *r_pos = nullptr, Point2 *r_normal = nullptr) const;

	inline bool encloses(const Rect2 &p_rect) const {
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0 || p_rect.size.x < 0 || p_rect.size.y < 0)) {
			ERR_PRINT("Rect2 size is negative, this is not supported. Use Rect2.abs() to get a Rect2 with a positive size.");
		}
#endif
		return (p_rect.position.x >= position.x) && (p_rect.position.y >= position.y) &&
				((p_rect.position.x + p_rect.size.x) <= (position.x + size.x)) &&
				((p_rect.position.y + p_rect.size.y) <= (position.y + size.y));
	}

	_FORCE_INLINE_ bool has_area() const {
		return size.x > 0.0f && size.y > 0.0f;
	}

	// Returns the intersection between two Rect2s or an empty Rect2 if there is no intersection.
	inline Rect2 intersection(const Rect2 &p_rect) const {
		Rect2 new_rect = p_rect;

		if (!intersects(new_rect)) {
			return Rect2();
		}

		new_rect.position = p_rect.position.max(position);

		Point2 p_rect_end = p_rect.position + p_rect.size;
		Point2 end = position + size;

		new_rect.size = p_rect_end.min(end) - new_rect.position;

		return new_rect;
	}

	inline Rect2 merge(const Rect2 &p_rect) const { ///< return a merged rect
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0 || p_rect.size.x < 0 || p_rect.size.y < 0)) {
			ERR_PRINT("Rect2 size is negative, this is not supported. Use Rect2.abs() to get a Rect2 with a positive size.");
		}
#endif
		Rect2 new_rect;

		new_rect.position = p_rect.position.min(position);

		new_rect.size = (p_rect.position + p_rect.size).max(position + size);

		new_rect.size = new_rect.size - new_rect.position; // Make relative again.

		return new_rect;
	}

	inline bool has_point(const Point2 &p_point) const {
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0)) {
			ERR_PRINT("Rect2 size is negative, this is not supported. Use Rect2.abs() to get a Rect2 with a positive size.");
		}
#endif
		if (p_point.x < position.x) {
			return false;
		}
		if (p_point.y < position.y) {
			return false;
		}

		if (p_point.x >= (position.x + size.x)) {
			return false;
		}
		if (p_point.y >= (position.y + size.y)) {
			return false;
		}

		return true;
	}

	bool is_equal_approx(const Rect2 &p_rect) const;
	bool is_finite() const;

	bool operator==(const Rect2 &p_rect) const { return position == p_rect.position && size == p_rect.size; }
	bool operator!=(const Rect2 &p_rect) const { return position != p_rect.position || size != p_rect.size; }

	inline Rect2 grow(real_t p_amount) const {
		Rect2 g = *this;
		g.grow_by(p_amount);
		return g;
	}

	inline void grow_by(real_t p_amount) {
		position.x -= p_amount;
		position.y -= p_amount;
		size.width += p_amount * 2;
		size.height += p_amount * 2;
	}

	inline Rect2 grow_side(Side p_side, real_t p_amount) const {
		Rect2 g = *this;
		g = g.grow_individual((SIDE_LEFT == p_side) ? p_amount : 0,
				(SIDE_TOP == p_side) ? p_amount : 0,
				(SIDE_RIGHT == p_side) ? p_amount : 0,
				(SIDE_BOTTOM == p_side) ? p_amount : 0);
		return g;
	}

	inline Rect2 grow_side_bind(uint32_t p_side, real_t p_amount) const {
		return grow_side(Side(p_side), p_amount);
	}

	inline Rect2 grow_individual(real_t p_left, real_t p_top, real_t p_right, real_t p_bottom) const {
		Rect2 g = *this;
		g.position.x -= p_left;
		g.position.y -= p_top;
		g.size.width += p_left + p_right;
		g.size.height += p_top + p_bottom;

		return g;
	}

	_FORCE_INLINE_ Rect2 expand(const Vector2 &p_vector) const {
		Rect2 r = *this;
		r.expand_to(p_vector);
		return r;
	}

	inline void expand_to(const Vector2 &p_vector) { // In place function for speed.
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0)) {
			ERR_PRINT("Rect2 size is negative, this is not supported. Use Rect2.abs() to get a Rect2 with a positive size.");
		}
#endif
		Vector2 begin = position;
		Vector2 end = position + size;

		if (p_vector.x < begin.x) {
			begin.x = p_vector.x;
		}
		if (p_vector.y < begin.y) {
			begin.y = p_vector.y;
		}

		if (p_vector.x > end.x) {
			end.x = p_vector.x;
		}
		if (p_vector.y > end.y) {
			end.y = p_vector.y;
		}

		position = begin;
		size = end - begin;
	}

	_FORCE_INLINE_ Rect2 abs() const {
		return Rect2(position + size.minf(0), size.abs());
	}

	_FORCE_INLINE_ Rect2 round() const {
		return Rect2(position.round(), size.round());
	}

	Vector2 get_support(const Vector2 &p_direction) const {
		Vector2 support = position;
		if (p_direction.x > 0.0f) {
			support.x += size.x;
		}
		if (p_direction.y > 0.0f) {
			support.y += size.y;
		}
		return support;
	}

	_FORCE_INLINE_ bool intersects_filled_polygon(const Vector2 *p_points, int p_point_count) const {
		Vector2 center = get_center();
		int side_plus = 0;
		int side_minus = 0;
		Vector2 end = position + size;

		int i_f = p_point_count - 1;
		for (int i = 0; i < p_point_count; i++) {
			const Vector2 &a = p_points[i_f];
			const Vector2 &b = p_points[i];
			i_f = i;

			Vector2 r = (b - a);
			const real_t l = r.length();
			if (l == 0.0f) {
				continue;
			}

			// Check inside.
			Vector2 tg = r.orthogonal();
			const real_t s = tg.dot(center) - tg.dot(a);
			if (s < 0.0f) {
				side_plus++;
			} else {
				side_minus++;
			}

			// Check ray box.
			r /= l;
			Vector2 ir(1.0f / r.x, 1.0f / r.y);

			// lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
			// r.org is origin of ray
			Vector2 t13 = (position - a) * ir;
			Vector2 t24 = (end - a) * ir;

			const real_t tmin = MAX(MIN(t13.x, t24.x), MIN(t13.y, t24.y));
			const real_t tmax = MIN(MAX(t13.x, t24.x), MAX(t13.y, t24.y));

			// if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
			if (tmax < 0 || tmin > tmax || tmin >= l) {
				continue;
			}

			return true;
		}

		if (side_plus * side_minus == 0) {
			return true; // All inside.
		} else {
			return false;
		}
	}

	_FORCE_INLINE_ void set_end(const Vector2 &p_end) {
		size = p_end - position;
	}

	_FORCE_INLINE_ Vector2 get_end() const {
		return position + size;
	}

	operator String() const;
	operator Rect2i() const;

	Rect2() {}
	Rect2(real_t p_x, real_t p_y, real_t p_width, real_t p_height) :
			position(Point2(p_x, p_y)),
			size(Size2(p_width, p_height)) {
	}
	Rect2(const Point2 &p_pos, const Size2 &p_size) :
			position(p_pos),
			size(p_size) {
	}
};
