/**************************************************************************/
/*  test_primitives.h                                                     */
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

#include "scene/resources/3d/primitive_meshes.h"

#include "tests/test_macros.h"

namespace TestPrimitives {

TEST_CASE("[SceneTree][Primitive][Capsule] Capsule Primitive") {
	Ref<CapsuleMesh> capsule = memnew(CapsuleMesh);

	SUBCASE("[SceneTree][Primitive][Capsule] Default values should be valid") {
		CHECK_MESSAGE(capsule->get_radius() > 0,
				"Radius of default capsule positive.");
		CHECK_MESSAGE(capsule->get_height() > 0,
				"Height of default capsule positive.");
		CHECK_MESSAGE(capsule->get_radial_segments() >= 0,
				"Radius Segments of default capsule positive.");
		CHECK_MESSAGE(capsule->get_rings() >= 0,
				"Number of rings of default capsule positive.");
	}

	SUBCASE("[SceneTree][Primitive][Capsule] Set properties of the capsule and get them with accessor methods") {
		capsule->set_height(7.1f);
		capsule->set_radius(1.3f);
		capsule->set_radial_segments(16);
		capsule->set_rings(32);

		CHECK_MESSAGE(capsule->get_radius() == doctest::Approx(1.3f),
				"Get/Set radius work with one set.");
		CHECK_MESSAGE(capsule->get_height() == doctest::Approx(7.1f),
				"Get/Set radius work with one set.");
		CHECK_MESSAGE(capsule->get_radial_segments() == 16,
				"Get/Set radius work with one set.");
		CHECK_MESSAGE(capsule->get_rings() == 32,
				"Get/Set radius work with one set.");
	}

	SUBCASE("[SceneTree][Primitive][Capsule] If set segments negative, default to at least 0") {
		ERR_PRINT_OFF;
		capsule->set_radial_segments(-5);
		capsule->set_rings(-17);
		ERR_PRINT_ON;

		CHECK_MESSAGE(capsule->get_radial_segments() >= 0,
				"Ensure number of radial segments is >= 0.");
		CHECK_MESSAGE(capsule->get_rings() >= 0,
				"Ensure number of rings is >= 0.");
	}

	SUBCASE("[SceneTree][Primitive][Capsule] If set height < 2*radius, adjust radius and height to radius=height*0.5") {
		capsule->set_radius(1.f);
		capsule->set_height(0.5f);

		CHECK_MESSAGE(capsule->get_radius() >= capsule->get_height() * 0.5,
				"Ensure radius >= height * 0.5 (needed for capsule to exist).");
	}

	SUBCASE("[Primitive][Capsule] Check mesh is correct") {
		Array data{};
		data.resize(RS::ARRAY_MAX);
		float radius{ 0.5f };
		float height{ 4.f };
		int num_radial_segments{ 4 };
		int num_rings{ 8 };
		CapsuleMesh::create_mesh_array(data, radius, height, num_radial_segments, num_rings);
		Vector<Vector3> points = data[RS::ARRAY_VERTEX];

		SUBCASE("[Primitive][Capsule] Ensure all vertices positions are within bounding radius and height") {
			// Get mesh data

			// Check all points within radius of capsule
			float dist_to_yaxis = 0.f;
			for (Vector3 point : points) {
				float new_dist_to_y = point.x * point.x + point.z * point.z;
				if (new_dist_to_y > dist_to_yaxis) {
					dist_to_yaxis = new_dist_to_y;
				}
			}

			CHECK(dist_to_yaxis <= radius * radius);

			// Check highest point and lowest point are within height of each other
			float max_y{ 0.f };
			float min_y{ 0.f };
			for (Vector3 point : points) {
				if (point.y > max_y) {
					max_y = point.y;
				}
				if (point.y < min_y) {
					min_y = point.y;
				}
			}

			CHECK(max_y - min_y <= height);
		}

		SUBCASE("[Primitive][Capsule] If normal.y == 0, then mesh makes a cylinder.") {
			Vector<Vector3> normals = data[RS::ARRAY_NORMAL];
			for (int ii = 0; ii < points.size(); ++ii) {
				float point_dist_from_yaxis = Math::sqrt(points[ii].x * points[ii].x + points[ii].z * points[ii].z);
				Vector3 yaxis_to_point{ points[ii].x / point_dist_from_yaxis, 0.f, points[ii].z / point_dist_from_yaxis };
				if (normals[ii].y == 0.f) {
					float mag_of_normal = Math::sqrt(normals[ii].x * normals[ii].x + normals[ii].z * normals[ii].z);
					Vector3 normalized_normal = normals[ii] / mag_of_normal;
					CHECK_MESSAGE(point_dist_from_yaxis == doctest::Approx(radius),
							"Points on the tube of the capsule are radius away from y-axis.");
					CHECK_MESSAGE(normalized_normal.is_equal_approx(yaxis_to_point),
							"Normal points orthogonal from mid cylinder.");
				}
			}
		}
	}
} // End capsule tests

TEST_CASE("[SceneTree][Primitive][Box] Box Primitive") {
	Ref<BoxMesh> box = memnew(BoxMesh);

	SUBCASE("[SceneTree][Primitive][Box] Default values should be valid") {
		CHECK(box->get_size().x > 0);
		CHECK(box->get_size().y > 0);
		CHECK(box->get_size().z > 0);
		CHECK(box->get_subdivide_width() >= 0);
		CHECK(box->get_subdivide_height() >= 0);
		CHECK(box->get_subdivide_depth() >= 0);
	}

	SUBCASE("[SceneTree][Primitive][Box] Set properties and get them with accessor methods") {
		Vector3 size{ 2.1, 3.3, 1.7 };
		box->set_size(size);
		box->set_subdivide_width(3);
		box->set_subdivide_height(2);
		box->set_subdivide_depth(4);

		CHECK(box->get_size().is_equal_approx(size));
		CHECK(box->get_subdivide_width() == 3);
		CHECK(box->get_subdivide_height() == 2);
		CHECK(box->get_subdivide_depth() == 4);
	}

	SUBCASE("[SceneTree][Primitive][Box] Set subdivides to negative and ensure they are >= 0") {
		ERR_PRINT_OFF;
		box->set_subdivide_width(-2);
		box->set_subdivide_height(-2);
		box->set_subdivide_depth(-2);
		ERR_PRINT_ON;

		CHECK(box->get_subdivide_width() >= 0);
		CHECK(box->get_subdivide_height() >= 0);
		CHECK(box->get_subdivide_depth() >= 0);
	}

	SUBCASE("[Primitive][Box] Check mesh is correct.") {
		Array data{};
		data.resize(RS::ARRAY_MAX);
		Vector3 size{ 0.5f, 1.2f, .9f };
		int subdivide_width{ 3 };
		int subdivide_height{ 2 };
		int subdivide_depth{ 8 };
		BoxMesh::create_mesh_array(data, size, subdivide_width, subdivide_height, subdivide_depth);
		Vector<Vector3> points = data[RS::ARRAY_VERTEX];
		Vector<Vector3> normals = data[RS::ARRAY_NORMAL];

		SUBCASE("Only 6 distinct normals.") {
			Vector<Vector3> distinct_normals{};
			distinct_normals.push_back(normals[0]);

			for (const Vector3 &normal : normals) {
				bool add_normal{ true };
				for (const Vector3 &vec : distinct_normals) {
					if (vec.is_equal_approx(normal)) {
						add_normal = false;
					}
				}

				if (add_normal) {
					distinct_normals.push_back(normal);
				}
			}

			CHECK_MESSAGE(distinct_normals.size() == 6,
					"There are exactly 6 distinct normals in the mesh data.");

			// All normals are orthogonal, or pointing in same direction.
			bool normal_correct_direction{ true };
			for (int rowIndex = 0; rowIndex < distinct_normals.size(); ++rowIndex) {
				for (int colIndex = rowIndex + 1; colIndex < distinct_normals.size(); ++colIndex) {
					if (!Math::is_equal_approx(distinct_normals[rowIndex].normalized().dot(distinct_normals[colIndex].normalized()), 0) &&
							!Math::is_equal_approx(distinct_normals[rowIndex].normalized().dot(distinct_normals[colIndex].normalized()), 1) &&
							!Math::is_equal_approx(distinct_normals[rowIndex].normalized().dot(distinct_normals[colIndex].normalized()), -1)) {
						normal_correct_direction = false;
						break;
					}
				}
				if (!normal_correct_direction) {
					break;
				}
			}

			CHECK_MESSAGE(normal_correct_direction,
					"All normals are either orthogonal or colinear.");
		}
	}
} // End box tests

TEST_CASE("[SceneTree][Primitive][Cylinder] Cylinder Primitive") {
	Ref<CylinderMesh> cylinder = memnew(CylinderMesh);

	SUBCASE("[SceneTree][Primitive][Cylinder] Default values should be valid") {
		CHECK(cylinder->get_top_radius() > 0);
		CHECK(cylinder->get_bottom_radius() > 0);
		CHECK(cylinder->get_height() > 0);
		CHECK(cylinder->get_radial_segments() > 0);
		CHECK(cylinder->get_rings() >= 0);
	}

	SUBCASE("[SceneTree][Primitive][Cylinder] Set properties and get them") {
		cylinder->set_top_radius(4.3f);
		cylinder->set_bottom_radius(1.2f);
		cylinder->set_height(9.77f);
		cylinder->set_radial_segments(12);
		cylinder->set_rings(16);
		cylinder->set_cap_top(false);
		cylinder->set_cap_bottom(false);

		CHECK(cylinder->get_top_radius() == doctest::Approx(4.3f));
		CHECK(cylinder->get_bottom_radius() == doctest::Approx(1.2f));
		CHECK(cylinder->get_height() == doctest::Approx(9.77f));
		CHECK(cylinder->get_radial_segments() == 12);
		CHECK(cylinder->get_rings() == 16);
		CHECK(!cylinder->is_cap_top());
		CHECK(!cylinder->is_cap_bottom());
	}

	SUBCASE("[SceneTree][Primitive][Cylinder] Ensure num segments is >= 0") {
		ERR_PRINT_OFF;
		cylinder->set_radial_segments(-12);
		cylinder->set_rings(-16);
		ERR_PRINT_ON;

		CHECK(cylinder->get_radial_segments() >= 0);
		CHECK(cylinder->get_rings() >= 0);
	}

	SUBCASE("[Primitive][Cylinder] Actual cylinder mesh tests (top and bottom radius the same).") {
		Array data{};
		data.resize(RS::ARRAY_MAX);
		real_t radius = .9f;
		real_t height = 3.2f;
		int radial_segments = 8;
		int rings = 5;
		bool top_cap = true;
		bool bottom_cap = true;
		CylinderMesh::create_mesh_array(data, radius, radius, height, radial_segments, rings, top_cap, bottom_cap);
		Vector<Vector3> points = data[RS::ARRAY_VERTEX];
		Vector<Vector3> normals = data[RS::ARRAY_NORMAL];

		SUBCASE("[Primitive][Cylinder] Side points are radius away from y-axis.") {
			bool is_radius_correct{ true };
			for (int index = 0; index < normals.size(); ++index) {
				if (Math::is_equal_approx(normals[index].y, 0)) {
					if (!Math::is_equal_approx((points[index] - Vector3(0, points[index].y, 0)).length_squared(), radius * radius)) {
						is_radius_correct = false;
						break;
					}
				}
			}

			CHECK(is_radius_correct);
		}

		SUBCASE("[Primitive][Cylinder] Only possible normals point in direction of point or in positive/negative y direction.") {
			bool is_correct_normals{ true };
			for (int index = 0; index < normals.size(); ++index) {
				Vector3 yaxis_to_point = points[index] - Vector3(0.f, points[index].y, 0.f);
				Vector3 point_to_normal = normals[index].normalized() - yaxis_to_point.normalized();
				//				std::cout << "<" << point_to_normal.x << ", " << point_to_normal.y << ", " << point_to_normal.z << ">\n";
				if (!(point_to_normal.is_equal_approx(Vector3(0, 0, 0))) &&
						(!Math::is_equal_approx(Math::abs(normals[index].normalized().y), 1))) {
					is_correct_normals = false;
					break;
				}
			}

			CHECK(is_correct_normals);
		}

		SUBCASE("[Primitive][Cylinder] Points on top and bottom are height/2 away from origin.") {
			bool is_height_correct{ true };
			real_t half_height = 0.5 * height;
			for (int index = 0; index < normals.size(); ++index) {
				if (Math::is_equal_approx(normals[index].x, 0) &&
						Math::is_equal_approx(normals[index].z, 0) &&
						normals[index].y > 0) {
					if (!Math::is_equal_approx(points[index].y, half_height)) {
						is_height_correct = false;
						break;
					}
				}
				if (Math::is_equal_approx(normals[index].x, 0) &&
						Math::is_equal_approx(normals[index].z, 0) &&
						normals[index].y < 0) {
					if (!Math::is_equal_approx(points[index].y, -half_height)) {
						is_height_correct = false;
						break;
					}
				}
			}

			CHECK(is_height_correct);
		}

		SUBCASE("[Primitive][Cylinder] Does mesh obey cap parameters?") {
			CylinderMesh::create_mesh_array(data, radius, radius, height, radial_segments, rings, top_cap, false);
			points = data[RS::ARRAY_VERTEX];
			normals = data[RS::ARRAY_NORMAL];
			bool no_bottom_cap{ true };

			for (int index = 0; index < normals.size(); ++index) {
				if (Math::is_equal_approx(normals[index].x, 0) &&
						Math::is_equal_approx(normals[index].z, 0) &&
						normals[index].y < 0) {
					no_bottom_cap = false;
					break;
				}
			}

			CHECK_MESSAGE(no_bottom_cap,
					"Check there is no bottom cap.");

			CylinderMesh::create_mesh_array(data, radius, radius, height, radial_segments, rings, false, bottom_cap);
			points = data[RS::ARRAY_VERTEX];
			normals = data[RS::ARRAY_NORMAL];
			bool no_top_cap{ true };

			for (int index = 0; index < normals.size(); ++index) {
				if (Math::is_equal_approx(normals[index].x, 0) &&
						Math::is_equal_approx(normals[index].z, 0) &&
						normals[index].y > 0) {
					no_top_cap = false;
					break;
				}
			}

			CHECK_MESSAGE(no_top_cap,
					"Check there is no top cap.");
		}
	}

	SUBCASE("[Primitive][Cylinder] Slanted cylinder mesh (top and bottom radius different).") {
		Array data{};
		data.resize(RS::ARRAY_MAX);
		real_t top_radius = 2.f;
		real_t bottom_radius = 1.f;
		real_t height = 1.f;
		int radial_segments = 8;
		int rings = 5;
		CylinderMesh::create_mesh_array(data, top_radius, bottom_radius, height, radial_segments, rings, false, false);
		Vector<Vector3> points = data[RS::ARRAY_VERTEX];
		Vector<Vector3> normals = data[RS::ARRAY_NORMAL];

		SUBCASE("[Primitive][Cylinder] Side points lie correct distance from y-axis") {
			bool is_radius_correct{ true };
			for (int index = 0; index < points.size(); ++index) {
				real_t radius = ((top_radius - bottom_radius) / height) * (points[index].y - 0.5 * height) + top_radius;
				Vector3 distance_to_yaxis = points[index] - Vector3(0.f, points[index].y, 0.f);
				if (!Math::is_equal_approx(distance_to_yaxis.length_squared(), radius * radius)) {
					is_radius_correct = false;
					break;
				}
			}

			CHECK(is_radius_correct);
		}

		SUBCASE("[Primitive][Cylinder] Normal on side is orthogonal to side tangent vector") {
			bool is_normal_correct{ true };
			for (int index = 0; index < points.size(); ++index) {
				Vector3 yaxis_to_point = points[index] - Vector3(0.f, points[index].y, 0.f);
				Vector3 yaxis_to_rb = yaxis_to_point.normalized() * bottom_radius;
				Vector3 rb_to_point = yaxis_to_point - yaxis_to_rb;
				Vector3 y_to_bottom = -Vector3(0.f, points[index].y + 0.5 * height, 0.f);
				Vector3 side_tangent = rb_to_point - y_to_bottom;

				if (!Math::is_equal_approx(normals[index].dot(side_tangent), 0)) {
					is_normal_correct = false;
					break;
				}
			}

			CHECK(is_normal_correct);
		}
	}

} // End cylinder tests

TEST_CASE("[SceneTree][Primitive][Plane] Plane Primitive") {
	Ref<PlaneMesh> plane = memnew(PlaneMesh);

	SUBCASE("[SceneTree][Primitive][Plane] Default values should be valid") {
		CHECK(plane->get_size().x > 0);
		CHECK(plane->get_size().y > 0);
		CHECK(plane->get_subdivide_width() >= 0);
		CHECK(plane->get_subdivide_depth() >= 0);
		CHECK((plane->get_orientation() == PlaneMesh::FACE_X || plane->get_orientation() == PlaneMesh::FACE_Y || plane->get_orientation() == PlaneMesh::FACE_Z));
	}

	SUBCASE("[SceneTree][Primitive][Plane] Set properties and get them.") {
		Size2 size{ 3.2, 1.8 };
		Vector3 offset{ -7.3, 0.4, -1.7 };
		plane->set_size(size);
		plane->set_subdivide_width(15);
		plane->set_subdivide_depth(29);
		plane->set_center_offset(offset);
		plane->set_orientation(PlaneMesh::FACE_X);

		CHECK(plane->get_size().is_equal_approx(size));
		CHECK(plane->get_subdivide_width() == 15);
		CHECK(plane->get_subdivide_depth() == 29);
		CHECK(plane->get_center_offset().is_equal_approx(offset));
		CHECK(plane->get_orientation() == PlaneMesh::FACE_X);
	}

	SUBCASE("[SceneTree][Primitive][Plane] Ensure number of segments is >= 0.") {
		ERR_PRINT_OFF;
		plane->set_subdivide_width(-15);
		plane->set_subdivide_depth(-29);
		ERR_PRINT_ON;

		CHECK(plane->get_subdivide_width() >= 0);
		CHECK(plane->get_subdivide_depth() >= 0);
	}
}

TEST_CASE("[SceneTree][Primitive][Quad] QuadMesh Primitive") {
	Ref<QuadMesh> quad = memnew(QuadMesh);

	SUBCASE("[Primitive][Quad] Orientation on initialization is in z direction") {
		CHECK(quad->get_orientation() == PlaneMesh::FACE_Z);
	}
}

TEST_CASE("[SceneTree][Primitive][Prism] Prism Primitive") {
	Ref<PrismMesh> prism = memnew(PrismMesh);

	SUBCASE("[Primitive][Prism] There are valid values of properties on initialization.") {
		CHECK(prism->get_left_to_right() >= 0);
		CHECK(prism->get_size().x >= 0);
		CHECK(prism->get_size().y >= 0);
		CHECK(prism->get_size().z >= 0);
		CHECK(prism->get_subdivide_width() >= 0);
		CHECK(prism->get_subdivide_height() >= 0);
		CHECK(prism->get_subdivide_depth() >= 0);
	}

	SUBCASE("[Primitive][Prism] Are able to change prism properties.") {
		Vector3 size{ 4.3, 9.1, 0.43 };
		prism->set_left_to_right(3.4f);
		prism->set_size(size);
		prism->set_subdivide_width(36);
		prism->set_subdivide_height(5);
		prism->set_subdivide_depth(64);

		CHECK(prism->get_left_to_right() == doctest::Approx(3.4f));
		CHECK(prism->get_size().is_equal_approx(size));
		CHECK(prism->get_subdivide_width() == 36);
		CHECK(prism->get_subdivide_height() == 5);
		CHECK(prism->get_subdivide_depth() == 64);
	}

	SUBCASE("[Primitive][Prism] Ensure number of segments always >= 0") {
		ERR_PRINT_OFF;
		prism->set_subdivide_width(-36);
		prism->set_subdivide_height(-5);
		prism->set_subdivide_depth(-64);
		ERR_PRINT_ON;

		CHECK(prism->get_subdivide_width() >= 0);
		CHECK(prism->get_subdivide_height() >= 0);
		CHECK(prism->get_subdivide_depth() >= 0);
	}
}

TEST_CASE("[SceneTree][Primitive][Sphere] Sphere Primitive") {
	Ref<SphereMesh> sphere = memnew(SphereMesh);

	SUBCASE("[Primitive][Sphere] There are valid values of properties on initialization.") {
		CHECK(sphere->get_radius() >= 0);
		CHECK(sphere->get_height() >= 0);
		CHECK(sphere->get_radial_segments() >= 0);
		CHECK(sphere->get_rings() >= 0);
	}

	SUBCASE("[Primitive][Sphere] Are able to change prism properties.") {
		sphere->set_radius(3.4f);
		sphere->set_height(2.2f);
		sphere->set_radial_segments(36);
		sphere->set_rings(5);
		sphere->set_is_hemisphere(true);

		CHECK(sphere->get_radius() == doctest::Approx(3.4f));
		CHECK(sphere->get_height() == doctest::Approx(2.2f));
		CHECK(sphere->get_radial_segments() == 36);
		CHECK(sphere->get_rings() == 5);
		CHECK(sphere->get_is_hemisphere());
	}

	SUBCASE("[Primitive][Sphere] Ensure number of segments always >= 0") {
		ERR_PRINT_OFF;
		sphere->set_radial_segments(-36);
		sphere->set_rings(-5);
		ERR_PRINT_ON;

		CHECK(sphere->get_radial_segments() >= 0);
		CHECK(sphere->get_rings() >= 0);
	}

	SUBCASE("[Primitive][Sphere] Sphere mesh tests.") {
		Array data{};
		data.resize(RS::ARRAY_MAX);
		real_t radius = 1.1f;
		int radial_segments = 8;
		int rings = 5;
		SphereMesh::create_mesh_array(data, radius, 2 * radius, radial_segments, rings);
		Vector<Vector3> points = data[RS::ARRAY_VERTEX];
		Vector<Vector3> normals = data[RS::ARRAY_NORMAL];

		SUBCASE("[Primitive][Sphere] All points lie radius away from origin.") {
			bool is_radius_correct = true;
			for (Vector3 point : points) {
				if (!Math::is_equal_approx(point.length_squared(), radius * radius)) {
					is_radius_correct = false;
					break;
				}
			}

			CHECK(is_radius_correct);
		}

		SUBCASE("[Primitive][Sphere] All normals lie in direction of corresponding point.") {
			bool is_normals_correct = true;
			for (int index = 0; index < points.size(); ++index) {
				if (!Math::is_equal_approx(normals[index].normalized().dot(points[index].normalized()), 1)) {
					is_normals_correct = false;
					break;
				}
			}

			CHECK(is_normals_correct);
		}
	}
}

TEST_CASE("[SceneTree][Primitive][Torus] Torus Primitive") {
	Ref<TorusMesh> torus = memnew(TorusMesh);
	Ref<PrimitiveMesh> prim = memnew(PrimitiveMesh);

	SUBCASE("[Primitive][Torus] There are valid values of properties on initialization.") {
		CHECK(torus->get_inner_radius() > 0);
		CHECK(torus->get_outer_radius() > 0);
		CHECK(torus->get_rings() >= 0);
		CHECK(torus->get_ring_segments() >= 0);
	}

	SUBCASE("[Primitive][Torus] Are able to change properties.") {
		torus->set_inner_radius(3.2f);
		torus->set_outer_radius(9.5f);
		torus->set_rings(19);
		torus->set_ring_segments(43);

		CHECK(torus->get_inner_radius() == doctest::Approx(3.2f));
		CHECK(torus->get_outer_radius() == doctest::Approx(9.5f));
		CHECK(torus->get_rings() == 19);
		CHECK(torus->get_ring_segments() == 43);
	}
}

TEST_CASE("[SceneTree][Primitive][TubeTrail] TubeTrail Primitive") {
	Ref<TubeTrailMesh> tube = memnew(TubeTrailMesh);

	SUBCASE("[Primitive][TubeTrail] There are valid values of properties on initialization.") {
		CHECK(tube->get_radius() > 0);
		CHECK(tube->get_radial_steps() >= 0);
		CHECK(tube->get_sections() >= 0);
		CHECK(tube->get_section_length() > 0);
		CHECK(tube->get_section_rings() >= 0);
		CHECK(tube->get_curve().is_null());
		CHECK(tube->get_builtin_bind_pose_count() >= 0);
	}

	SUBCASE("[Primitive][TubeTrail] Are able to change properties.") {
		tube->set_radius(7.2f);
		tube->set_radial_steps(9);
		tube->set_sections(33);
		tube->set_section_length(5.5f);
		tube->set_section_rings(12);
		Ref<Curve> curve = memnew(Curve);
		tube->set_curve(curve);

		CHECK(tube->get_radius() == doctest::Approx(7.2f));
		CHECK(tube->get_section_length() == doctest::Approx(5.5f));
		CHECK(tube->get_radial_steps() == 9);
		CHECK(tube->get_sections() == 33);
		CHECK(tube->get_section_rings() == 12);
		CHECK(tube->get_curve() == curve);
	}

	SUBCASE("[Primitive][TubeTrail] Setting same curve more than once, it remains the same.") {
		Ref<Curve> curve = memnew(Curve);
		tube->set_curve(curve);
		tube->set_curve(curve);
		tube->set_curve(curve);

		CHECK(tube->get_curve() == curve);
	}

	SUBCASE("[Primitive][TubeTrail] Setting curve, then changing to different curve.") {
		Ref<Curve> curve1 = memnew(Curve);
		Ref<Curve> curve2 = memnew(Curve);
		tube->set_curve(curve1);
		CHECK(tube->get_curve() == curve1);

		tube->set_curve(curve2);
		CHECK(tube->get_curve() == curve2);
	}

	SUBCASE("[Primitive][TubeTrail] Assign same curve to two different tube trails") {
		Ref<TubeTrailMesh> tube2 = memnew(TubeTrailMesh);
		Ref<Curve> curve = memnew(Curve);
		tube->set_curve(curve);
		tube2->set_curve(curve);

		CHECK(tube->get_curve() == curve);
		CHECK(tube2->get_curve() == curve);
	}
}

TEST_CASE("[SceneTree][Primitive][RibbonTrail] RibbonTrail Primitive") {
	Ref<RibbonTrailMesh> ribbon = memnew(RibbonTrailMesh);

	SUBCASE("[Primitive][RibbonTrail] There are valid values of properties on initialization.") {
		CHECK(ribbon->get_size() > 0);
		CHECK(ribbon->get_sections() >= 0);
		CHECK(ribbon->get_section_length() > 0);
		CHECK(ribbon->get_section_segments() >= 0);
		CHECK(ribbon->get_builtin_bind_pose_count() >= 0);
		CHECK(ribbon->get_curve().is_null());
		CHECK((ribbon->get_shape() == RibbonTrailMesh::SHAPE_CROSS ||
				ribbon->get_shape() == RibbonTrailMesh::SHAPE_FLAT));
	}

	SUBCASE("[Primitive][RibbonTrail] Able to change properties.") {
		Ref<Curve> curve = memnew(Curve);
		ribbon->set_size(4.3f);
		ribbon->set_sections(16);
		ribbon->set_section_length(1.3f);
		ribbon->set_section_segments(9);
		ribbon->set_curve(curve);

		CHECK(ribbon->get_size() == doctest::Approx(4.3f));
		CHECK(ribbon->get_section_length() == doctest::Approx(1.3f));
		CHECK(ribbon->get_sections() == 16);
		CHECK(ribbon->get_section_segments() == 9);
		CHECK(ribbon->get_curve() == curve);
	}

	SUBCASE("[Primitive][RibbonTrail] Setting same curve more than once, it remains the same.") {
		Ref<Curve> curve = memnew(Curve);
		ribbon->set_curve(curve);
		ribbon->set_curve(curve);
		ribbon->set_curve(curve);

		CHECK(ribbon->get_curve() == curve);
	}

	SUBCASE("[Primitive][RibbonTrail] Setting curve, then changing to different curve.") {
		Ref<Curve> curve1 = memnew(Curve);
		Ref<Curve> curve2 = memnew(Curve);
		ribbon->set_curve(curve1);
		CHECK(ribbon->get_curve() == curve1);

		ribbon->set_curve(curve2);
		CHECK(ribbon->get_curve() == curve2);
	}

	SUBCASE("[Primitive][RibbonTrail] Assign same curve to two different ribbon trails") {
		Ref<RibbonTrailMesh> ribbon2 = memnew(RibbonTrailMesh);
		Ref<Curve> curve = memnew(Curve);
		ribbon->set_curve(curve);
		ribbon2->set_curve(curve);

		CHECK(ribbon->get_curve() == curve);
		CHECK(ribbon2->get_curve() == curve);
	}
}

TEST_CASE("[SceneTree][Primitive][Text] Text Primitive") {
	Ref<TextMesh> text = memnew(TextMesh);

	SUBCASE("[Primitive][Text] There are valid values of properties on initialization.") {
		CHECK((text->get_horizontal_alignment() == HORIZONTAL_ALIGNMENT_CENTER ||
				text->get_horizontal_alignment() == HORIZONTAL_ALIGNMENT_LEFT ||
				text->get_horizontal_alignment() == HORIZONTAL_ALIGNMENT_RIGHT ||
				text->get_horizontal_alignment() == HORIZONTAL_ALIGNMENT_FILL));
		CHECK((text->get_vertical_alignment() == VERTICAL_ALIGNMENT_BOTTOM ||
				text->get_vertical_alignment() == VERTICAL_ALIGNMENT_TOP ||
				text->get_vertical_alignment() == VERTICAL_ALIGNMENT_CENTER ||
				text->get_vertical_alignment() == VERTICAL_ALIGNMENT_FILL));
		CHECK(text->get_font().is_null());
		CHECK(text->get_font_size() > 0);
		CHECK(text->get_line_spacing() >= 0);
		CHECK((text->get_autowrap_mode() == TextServer::AUTOWRAP_OFF ||
				text->get_autowrap_mode() == TextServer::AUTOWRAP_ARBITRARY ||
				text->get_autowrap_mode() == TextServer::AUTOWRAP_WORD ||
				text->get_autowrap_mode() == TextServer::AUTOWRAP_WORD_SMART));
		CHECK((text->get_text_direction() == TextServer::DIRECTION_AUTO ||
				text->get_text_direction() == TextServer::DIRECTION_LTR ||
				text->get_text_direction() == TextServer::DIRECTION_RTL));
		CHECK((text->get_structured_text_bidi_override() == TextServer::STRUCTURED_TEXT_DEFAULT ||
				text->get_structured_text_bidi_override() == TextServer::STRUCTURED_TEXT_URI ||
				text->get_structured_text_bidi_override() == TextServer::STRUCTURED_TEXT_FILE ||
				text->get_structured_text_bidi_override() == TextServer::STRUCTURED_TEXT_EMAIL ||
				text->get_structured_text_bidi_override() == TextServer::STRUCTURED_TEXT_LIST ||
				text->get_structured_text_bidi_override() == TextServer::STRUCTURED_TEXT_GDSCRIPT ||
				text->get_structured_text_bidi_override() == TextServer::STRUCTURED_TEXT_CUSTOM));
		CHECK(text->get_structured_text_bidi_override_options().size() >= 0);
		CHECK(text->get_width() > 0);
		CHECK(text->get_depth() > 0);
		CHECK(text->get_curve_step() > 0);
		CHECK(text->get_pixel_size() > 0);
	}

	SUBCASE("[Primitive][Text] Change the properties of the mesh.") {
		Ref<Font> font = memnew(Font);
		Array options{};
		Point2 offset{ 30.8, 104.23 };
		text->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
		text->set_vertical_alignment(VERTICAL_ALIGNMENT_BOTTOM);
		text->set_text("Hello");
		text->set_font(font);
		text->set_font_size(12);
		text->set_line_spacing(1.7f);
		text->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		text->set_text_direction(TextServer::DIRECTION_RTL);
		text->set_language("French");
		text->set_structured_text_bidi_override(TextServer::STRUCTURED_TEXT_EMAIL);
		text->set_structured_text_bidi_override_options(options);
		text->set_uppercase(true);
		real_t width{ 0.6 };
		real_t depth{ 1.7 };
		real_t pixel_size{ 2.8 };
		real_t curve_step{ 4.8 };
		text->set_width(width);
		text->set_depth(depth);
		text->set_curve_step(curve_step);
		text->set_pixel_size(pixel_size);
		text->set_offset(offset);

		CHECK(text->get_horizontal_alignment() == HORIZONTAL_ALIGNMENT_RIGHT);
		CHECK(text->get_vertical_alignment() == VERTICAL_ALIGNMENT_BOTTOM);
		CHECK(text->get_text_direction() == TextServer::DIRECTION_RTL);
		CHECK(text->get_text() == "Hello");
		CHECK(text->get_font() == font);
		CHECK(text->get_font_size() == 12);
		CHECK(text->get_autowrap_mode() == TextServer::AUTOWRAP_WORD_SMART);
		CHECK(text->get_language() == "French");
		CHECK(text->get_structured_text_bidi_override() == TextServer::STRUCTURED_TEXT_EMAIL);
		CHECK(text->get_structured_text_bidi_override_options() == options);
		CHECK(text->is_uppercase() == true);
		CHECK(text->get_offset() == offset);
		CHECK(text->get_line_spacing() == doctest::Approx(1.7f));
		CHECK(text->get_width() == doctest::Approx(width));
		CHECK(text->get_depth() == doctest::Approx(depth));
		CHECK(text->get_curve_step() == doctest::Approx(curve_step));
		CHECK(text->get_pixel_size() == doctest::Approx(pixel_size));
	}

	SUBCASE("[Primitive][Text] Set objects multiple times.") {
		Ref<Font> font = memnew(Font);
		Array options{};
		Point2 offset{ 30.8, 104.23 };

		text->set_font(font);
		text->set_font(font);
		text->set_font(font);
		text->set_structured_text_bidi_override_options(options);
		text->set_structured_text_bidi_override_options(options);
		text->set_structured_text_bidi_override_options(options);
		text->set_offset(offset);
		text->set_offset(offset);
		text->set_offset(offset);

		CHECK(text->get_font() == font);
		CHECK(text->get_structured_text_bidi_override_options() == options);
		CHECK(text->get_offset() == offset);
	}

	SUBCASE("[Primitive][Text] Set then change objects.") {
		Ref<Font> font1 = memnew(Font);
		Ref<Font> font2 = memnew(Font);
		Array options1{};
		Array options2{};
		Point2 offset1{ 30.8, 104.23 };
		Point2 offset2{ -30.8, -104.23 };

		text->set_font(font1);
		text->set_structured_text_bidi_override_options(options1);
		text->set_offset(offset1);

		CHECK(text->get_font() == font1);
		CHECK(text->get_structured_text_bidi_override_options() == options1);
		CHECK(text->get_offset() == offset1);

		text->set_font(font2);
		text->set_structured_text_bidi_override_options(options2);
		text->set_offset(offset2);

		CHECK(text->get_font() == font2);
		CHECK(text->get_structured_text_bidi_override_options() == options2);
		CHECK(text->get_offset() == offset2);
	}

	SUBCASE("[Primitive][Text] Assign same font to two Textmeshes.") {
		Ref<TextMesh> text2 = memnew(TextMesh);
		Ref<Font> font = memnew(Font);

		text->set_font(font);
		text2->set_font(font);

		CHECK(text->get_font() == font);
		CHECK(text2->get_font() == font);
	}
}

} // namespace TestPrimitives
