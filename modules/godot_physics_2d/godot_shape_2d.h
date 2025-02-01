/**************************************************************************/
/*  godot_shape_2d.h                                                      */
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

#include "servers/physics_server_2d.h"

class GodotShape2D;

class GodotShapeOwner2D {
public:
	virtual void _shape_changed() = 0;
	virtual void remove_shape(GodotShape2D *p_shape) = 0;

	virtual ~GodotShapeOwner2D() {}
};

class GodotShape2D {
	RID self;
	Rect2 aabb;
	bool configured = false;
	real_t custom_bias = 0.0;

	HashMap<GodotShapeOwner2D *, int> owners;

protected:
	const double segment_is_valid_support_threshold = 0.99998;
	const double segment_is_valid_support_threshold_lower =
			Math::sqrt(1.0 - segment_is_valid_support_threshold * segment_is_valid_support_threshold);

	void configure(const Rect2 &p_aabb);

public:
	_FORCE_INLINE_ void set_self(const RID &p_self) { self = p_self; }
	_FORCE_INLINE_ RID get_self() const { return self; }

	virtual PhysicsServer2D::ShapeType get_type() const = 0;

	_FORCE_INLINE_ Rect2 get_aabb() const { return aabb; }
	_FORCE_INLINE_ bool is_configured() const { return configured; }

	virtual bool allows_one_way_collision() const { return true; }

	virtual bool is_concave() const { return false; }

	virtual bool contains_point(const Vector2 &p_point) const = 0;

	virtual void project_rangev(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const = 0;
	virtual void project_range_castv(const Vector2 &p_cast, const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const = 0;
	virtual Vector2 get_support(const Vector2 &p_normal) const;
	virtual void get_supports(const Vector2 &p_normal, Vector2 *r_supports, int &r_amount) const = 0;

	virtual bool intersect_segment(const Vector2 &p_begin, const Vector2 &p_end, Vector2 &r_point, Vector2 &r_normal) const = 0;
	virtual real_t get_moment_of_inertia(real_t p_mass, const Size2 &p_scale) const = 0;
	virtual void set_data(const Variant &p_data) = 0;
	virtual Variant get_data() const = 0;

	_FORCE_INLINE_ void set_custom_bias(real_t p_bias) { custom_bias = p_bias; }
	_FORCE_INLINE_ real_t get_custom_bias() const { return custom_bias; }

	void add_owner(GodotShapeOwner2D *p_owner);
	void remove_owner(GodotShapeOwner2D *p_owner);
	bool is_owner(GodotShapeOwner2D *p_owner) const;
	const HashMap<GodotShapeOwner2D *, int> &get_owners() const;

	_FORCE_INLINE_ void get_supports_transformed_cast(const Vector2 &p_cast, const Vector2 &p_normal, const Transform2D &p_xform, Vector2 *r_supports, int &r_amount) const {
		get_supports(p_xform.basis_xform_inv(p_normal).normalized(), r_supports, r_amount);
		for (int i = 0; i < r_amount; i++) {
			r_supports[i] = p_xform.xform(r_supports[i]);
		}

		if (r_amount == 1) {
			if (Math::abs(p_normal.dot(p_cast.normalized())) < segment_is_valid_support_threshold_lower) {
				//make line because they are parallel
				r_amount = 2;
				r_supports[1] = r_supports[0] + p_cast;
			} else if (p_cast.dot(p_normal) > 0) {
				//normal points towards cast, add cast
				r_supports[0] += p_cast;
			}

		} else {
			if (Math::abs(p_normal.dot(p_cast.normalized())) < segment_is_valid_support_threshold_lower) {
				//optimize line and make it larger because they are parallel
				if ((r_supports[1] - r_supports[0]).dot(p_cast) > 0) {
					//larger towards 1
					r_supports[1] += p_cast;
				} else {
					//larger towards 0
					r_supports[0] += p_cast;
				}
			} else if (p_cast.dot(p_normal) > 0) {
				//normal points towards cast, add cast
				r_supports[0] += p_cast;
				r_supports[1] += p_cast;
			}
		}
	}
	GodotShape2D() {}
	virtual ~GodotShape2D();
};

//let the optimizer do the magic
#define DEFAULT_PROJECT_RANGE_CAST                                                                                                                                  \
	virtual void project_range_castv(const Vector2 &p_cast, const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const override { \
		project_range_cast(p_cast, p_normal, p_transform, r_min, r_max);                                                                                            \
	}                                                                                                                                                               \
	_FORCE_INLINE_ void project_range_cast(const Vector2 &p_cast, const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const {    \
		real_t mina, maxa;                                                                                                                                          \
		real_t minb, maxb;                                                                                                                                          \
		Transform2D ofsb = p_transform;                                                                                                                             \
		ofsb.columns[2] += p_cast;                                                                                                                                  \
		project_range(p_normal, p_transform, mina, maxa);                                                                                                           \
		project_range(p_normal, ofsb, minb, maxb);                                                                                                                  \
		r_min = MIN(mina, minb);                                                                                                                                    \
		r_max = MAX(maxa, maxb);                                                                                                                                    \
	}

class GodotWorldBoundaryShape2D : public GodotShape2D {
	Vector2 normal;
	real_t d = 0.0;

public:
	_FORCE_INLINE_ Vector2 get_normal() const { return normal; }
	_FORCE_INLINE_ real_t get_d() const { return d; }

	virtual PhysicsServer2D::ShapeType get_type() const override { return PhysicsServer2D::SHAPE_WORLD_BOUNDARY; }

	virtual void project_rangev(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const override { project_range(p_normal, p_transform, r_min, r_max); }
	virtual void get_supports(const Vector2 &p_normal, Vector2 *r_supports, int &r_amount) const override;

	virtual bool contains_point(const Vector2 &p_point) const override;
	virtual bool intersect_segment(const Vector2 &p_begin, const Vector2 &p_end, Vector2 &r_point, Vector2 &r_normal) const override;
	virtual real_t get_moment_of_inertia(real_t p_mass, const Size2 &p_scale) const override;

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	_FORCE_INLINE_ void project_range(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const {
		//real large
		r_min = -1e10;
		r_max = 1e10;
	}

	virtual void project_range_castv(const Vector2 &p_cast, const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const override {
		project_range_cast(p_cast, p_normal, p_transform, r_min, r_max);
	}

	_FORCE_INLINE_ void project_range_cast(const Vector2 &p_cast, const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const {
		//real large
		r_min = -1e10;
		r_max = 1e10;
	}
};

class GodotSeparationRayShape2D : public GodotShape2D {
	real_t length = 0.0;
	bool slide_on_slope = false;

public:
	_FORCE_INLINE_ real_t get_length() const { return length; }
	_FORCE_INLINE_ bool get_slide_on_slope() const { return slide_on_slope; }

	virtual PhysicsServer2D::ShapeType get_type() const override { return PhysicsServer2D::SHAPE_SEPARATION_RAY; }

	virtual bool allows_one_way_collision() const override { return false; }

	virtual void project_rangev(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const override { project_range(p_normal, p_transform, r_min, r_max); }
	virtual void get_supports(const Vector2 &p_normal, Vector2 *r_supports, int &r_amount) const override;

	virtual bool contains_point(const Vector2 &p_point) const override;
	virtual bool intersect_segment(const Vector2 &p_begin, const Vector2 &p_end, Vector2 &r_point, Vector2 &r_normal) const override;
	virtual real_t get_moment_of_inertia(real_t p_mass, const Size2 &p_scale) const override;

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	_FORCE_INLINE_ void project_range(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const {
		//real large
		r_max = p_normal.dot(p_transform.get_origin());
		r_min = p_normal.dot(p_transform.xform(Vector2(0, length)));
		if (r_max < r_min) {
			SWAP(r_max, r_min);
		}
	}

	DEFAULT_PROJECT_RANGE_CAST

	_FORCE_INLINE_ GodotSeparationRayShape2D() {}
	_FORCE_INLINE_ GodotSeparationRayShape2D(real_t p_length) { length = p_length; }
};

class GodotSegmentShape2D : public GodotShape2D {
	Vector2 a;
	Vector2 b;
	Vector2 n;

public:
	_FORCE_INLINE_ const Vector2 &get_a() const { return a; }
	_FORCE_INLINE_ const Vector2 &get_b() const { return b; }
	_FORCE_INLINE_ const Vector2 &get_normal() const { return n; }

	virtual PhysicsServer2D::ShapeType get_type() const override { return PhysicsServer2D::SHAPE_SEGMENT; }

	_FORCE_INLINE_ Vector2 get_xformed_normal(const Transform2D &p_xform) const {
		return (p_xform.xform(b) - p_xform.xform(a)).normalized().orthogonal();
	}
	virtual void project_rangev(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const override { project_range(p_normal, p_transform, r_min, r_max); }
	virtual void get_supports(const Vector2 &p_normal, Vector2 *r_supports, int &r_amount) const override;

	virtual bool contains_point(const Vector2 &p_point) const override;
	virtual bool intersect_segment(const Vector2 &p_begin, const Vector2 &p_end, Vector2 &r_point, Vector2 &r_normal) const override;
	virtual real_t get_moment_of_inertia(real_t p_mass, const Size2 &p_scale) const override;

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	_FORCE_INLINE_ void project_range(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const {
		//real large
		r_max = p_normal.dot(p_transform.xform(a));
		r_min = p_normal.dot(p_transform.xform(b));
		if (r_max < r_min) {
			SWAP(r_max, r_min);
		}
	}

	DEFAULT_PROJECT_RANGE_CAST

	_FORCE_INLINE_ GodotSegmentShape2D() {}
	_FORCE_INLINE_ GodotSegmentShape2D(const Vector2 &p_a, const Vector2 &p_b, const Vector2 &p_n) {
		a = p_a;
		b = p_b;
		n = p_n;
	}
};

class GodotCircleShape2D : public GodotShape2D {
	real_t radius;

public:
	_FORCE_INLINE_ const real_t &get_radius() const { return radius; }

	virtual PhysicsServer2D::ShapeType get_type() const override { return PhysicsServer2D::SHAPE_CIRCLE; }

	virtual void project_rangev(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const override { project_range(p_normal, p_transform, r_min, r_max); }
	virtual void get_supports(const Vector2 &p_normal, Vector2 *r_supports, int &r_amount) const override;

	virtual bool contains_point(const Vector2 &p_point) const override;
	virtual bool intersect_segment(const Vector2 &p_begin, const Vector2 &p_end, Vector2 &r_point, Vector2 &r_normal) const override;
	virtual real_t get_moment_of_inertia(real_t p_mass, const Size2 &p_scale) const override;

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	_FORCE_INLINE_ void project_range(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const {
		//real large
		real_t d = p_normal.dot(p_transform.get_origin());

		// figure out scale at point
		Vector2 local_normal = p_transform.basis_xform_inv(p_normal);
		real_t scale = local_normal.length();

		r_min = d - (radius)*scale;
		r_max = d + (radius)*scale;
	}

	DEFAULT_PROJECT_RANGE_CAST
};

class GodotRectangleShape2D : public GodotShape2D {
	Vector2 half_extents;

public:
	_FORCE_INLINE_ const Vector2 &get_half_extents() const { return half_extents; }

	virtual PhysicsServer2D::ShapeType get_type() const override { return PhysicsServer2D::SHAPE_RECTANGLE; }

	virtual void project_rangev(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const override { project_range(p_normal, p_transform, r_min, r_max); }
	virtual void get_supports(const Vector2 &p_normal, Vector2 *r_supports, int &r_amount) const override;

	virtual bool contains_point(const Vector2 &p_point) const override;
	virtual bool intersect_segment(const Vector2 &p_begin, const Vector2 &p_end, Vector2 &r_point, Vector2 &r_normal) const override;
	virtual real_t get_moment_of_inertia(real_t p_mass, const Size2 &p_scale) const override;

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	_FORCE_INLINE_ void project_range(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const {
		// no matter the angle, the box is mirrored anyway
		r_max = -1e20;
		r_min = 1e20;
		for (int i = 0; i < 4; i++) {
			real_t d = p_normal.dot(p_transform.xform(Vector2(((i & 1) * 2 - 1) * half_extents.x, ((i >> 1) * 2 - 1) * half_extents.y)));

			if (d > r_max) {
				r_max = d;
			}
			if (d < r_min) {
				r_min = d;
			}
		}
	}

	_FORCE_INLINE_ Vector2 get_circle_axis(const Transform2D &p_xform, const Transform2D &p_xform_inv, const Vector2 &p_circle) const {
		Vector2 local_v = p_xform_inv.xform(p_circle);

		Vector2 he(
				(local_v.x < 0) ? -half_extents.x : half_extents.x,
				(local_v.y < 0) ? -half_extents.y : half_extents.y);

		return (p_xform.xform(he) - p_circle).normalized();
	}

	_FORCE_INLINE_ Vector2 get_box_axis(const Transform2D &p_xform, const Transform2D &p_xform_inv, const GodotRectangleShape2D *p_B, const Transform2D &p_B_xform, const Transform2D &p_B_xform_inv) const {
		Vector2 a, b;

		{
			Vector2 local_v = p_xform_inv.xform(p_B_xform.get_origin());

			Vector2 he(
					(local_v.x < 0) ? -half_extents.x : half_extents.x,
					(local_v.y < 0) ? -half_extents.y : half_extents.y);

			a = p_xform.xform(he);
		}
		{
			Vector2 local_v = p_B_xform_inv.xform(p_xform.get_origin());

			Vector2 he(
					(local_v.x < 0) ? -p_B->half_extents.x : p_B->half_extents.x,
					(local_v.y < 0) ? -p_B->half_extents.y : p_B->half_extents.y);

			b = p_B_xform.xform(he);
		}

		return (a - b).normalized();
	}

	DEFAULT_PROJECT_RANGE_CAST
};

class GodotCapsuleShape2D : public GodotShape2D {
	real_t radius = 0.0;
	real_t height = 0.0;

public:
	_FORCE_INLINE_ const real_t &get_radius() const { return radius; }
	_FORCE_INLINE_ const real_t &get_height() const { return height; }

	virtual PhysicsServer2D::ShapeType get_type() const override { return PhysicsServer2D::SHAPE_CAPSULE; }

	virtual void project_rangev(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const override { project_range(p_normal, p_transform, r_min, r_max); }
	virtual void get_supports(const Vector2 &p_normal, Vector2 *r_supports, int &r_amount) const override;

	virtual bool contains_point(const Vector2 &p_point) const override;
	virtual bool intersect_segment(const Vector2 &p_begin, const Vector2 &p_end, Vector2 &r_point, Vector2 &r_normal) const override;
	virtual real_t get_moment_of_inertia(real_t p_mass, const Size2 &p_scale) const override;

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	_FORCE_INLINE_ void project_range(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const {
		// no matter the angle, the box is mirrored anyway
		Vector2 n = p_transform.basis_xform_inv(p_normal).normalized();
		real_t h = height * 0.5 - radius;

		n *= radius;
		n.y += (n.y > 0) ? h : -h;

		r_max = p_normal.dot(p_transform.xform(n));
		r_min = p_normal.dot(p_transform.xform(-n));

		if (r_max < r_min) {
			SWAP(r_max, r_min);
		}

		//ERR_FAIL_COND( r_max < r_min );
	}

	DEFAULT_PROJECT_RANGE_CAST
};

class GodotConvexPolygonShape2D : public GodotShape2D {
	struct Point {
		Vector2 pos;
		Vector2 normal; //normal to next segment
	};

	Point *points = nullptr;
	int point_count = 0;

public:
	_FORCE_INLINE_ int get_point_count() const { return point_count; }
	_FORCE_INLINE_ const Vector2 &get_point(int p_idx) const { return points[p_idx].pos; }
	_FORCE_INLINE_ const Vector2 &get_segment_normal(int p_idx) const { return points[p_idx].normal; }
	_FORCE_INLINE_ Vector2 get_xformed_segment_normal(const Transform2D &p_xform, int p_idx) const {
		Vector2 a = points[p_idx].pos;
		p_idx++;
		Vector2 b = points[p_idx == point_count ? 0 : p_idx].pos;
		return (p_xform.xform(b) - p_xform.xform(a)).normalized().orthogonal();
	}

	virtual PhysicsServer2D::ShapeType get_type() const override { return PhysicsServer2D::SHAPE_CONVEX_POLYGON; }

	virtual void project_rangev(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const override { project_range(p_normal, p_transform, r_min, r_max); }
	virtual void get_supports(const Vector2 &p_normal, Vector2 *r_supports, int &r_amount) const override;

	virtual bool contains_point(const Vector2 &p_point) const override;
	virtual bool intersect_segment(const Vector2 &p_begin, const Vector2 &p_end, Vector2 &r_point, Vector2 &r_normal) const override;
	virtual real_t get_moment_of_inertia(real_t p_mass, const Size2 &p_scale) const override;

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	_FORCE_INLINE_ void project_range(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const {
		if (!points || point_count <= 0) {
			r_min = r_max = 0;
			return;
		}

		r_min = r_max = p_normal.dot(p_transform.xform(points[0].pos));
		for (int i = 1; i < point_count; i++) {
			real_t d = p_normal.dot(p_transform.xform(points[i].pos));
			if (d > r_max) {
				r_max = d;
			}
			if (d < r_min) {
				r_min = d;
			}
		}
	}

	DEFAULT_PROJECT_RANGE_CAST

	GodotConvexPolygonShape2D() {}
	~GodotConvexPolygonShape2D();
};

class GodotConcaveShape2D : public GodotShape2D {
public:
	virtual bool is_concave() const override { return true; }

	// Returns true to stop the query.
	typedef bool (*QueryCallback)(void *p_userdata, GodotShape2D *p_convex);

	virtual void cull(const Rect2 &p_local_aabb, QueryCallback p_callback, void *p_userdata) const = 0;
};

class GodotConcavePolygonShape2D : public GodotConcaveShape2D {
	struct Segment {
		int points[2] = {};
	};

	Vector<Segment> segments;
	Vector<Point2> points;

	struct BVH {
		Rect2 aabb;
		int left = 0, right = 0;
	};

	Vector<BVH> bvh;
	int bvh_depth = 0;

	struct BVH_CompareX {
		_FORCE_INLINE_ bool operator()(const BVH &a, const BVH &b) const {
			return (a.aabb.position.x + a.aabb.size.x * 0.5) < (b.aabb.position.x + b.aabb.size.x * 0.5);
		}
	};

	struct BVH_CompareY {
		_FORCE_INLINE_ bool operator()(const BVH &a, const BVH &b) const {
			return (a.aabb.position.y + a.aabb.size.y * 0.5) < (b.aabb.position.y + b.aabb.size.y * 0.5);
		}
	};

	int _generate_bvh(BVH *p_bvh, int p_len, int p_depth);

public:
	virtual PhysicsServer2D::ShapeType get_type() const override { return PhysicsServer2D::SHAPE_CONCAVE_POLYGON; }

	virtual void project_rangev(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const override {
		r_min = 0;
		r_max = 0;
		ERR_FAIL_MSG("Unsupported call to project_rangev in GodotConcavePolygonShape2D");
	}

	void project_range(const Vector2 &p_normal, const Transform2D &p_transform, real_t &r_min, real_t &r_max) const {
		r_min = 0;
		r_max = 0;
		ERR_FAIL_MSG("Unsupported call to project_range in GodotConcavePolygonShape2D");
	}

	virtual void get_supports(const Vector2 &p_normal, Vector2 *r_supports, int &r_amount) const override;

	virtual bool contains_point(const Vector2 &p_point) const override;
	virtual bool intersect_segment(const Vector2 &p_begin, const Vector2 &p_end, Vector2 &r_point, Vector2 &r_normal) const override;

	virtual real_t get_moment_of_inertia(real_t p_mass, const Size2 &p_scale) const override { return 0; }

	virtual void set_data(const Variant &p_data) override;
	virtual Variant get_data() const override;

	virtual void cull(const Rect2 &p_local_aabb, QueryCallback p_callback, void *p_userdata) const override;

	DEFAULT_PROJECT_RANGE_CAST
};

#undef DEFAULT_PROJECT_RANGE_CAST
