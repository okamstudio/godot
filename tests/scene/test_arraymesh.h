/**************************************************************************/
/*  test_arraymesh.h                                                      */
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
#include "scene/resources/mesh.h"

#include "tests/test_macros.h"

namespace TestArrayMesh {

TEST_CASE("[SceneTree][ArrayMesh] Adding and modifying blendshapes.") {
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	StringName name_a{ "ShapeA" };
	StringName name_b{ "ShapeB" };

	SUBCASE("Adding a blend shape to the mesh before a surface is added.") {
		mesh->add_blend_shape(name_a);
		mesh->add_blend_shape(name_b);

		CHECK(mesh->get_blend_shape_name(0) == name_a);
		CHECK(mesh->get_blend_shape_name(1) == name_b);
	}

	SUBCASE("Add same blend shape multiple times appends name with number.") {
		mesh->add_blend_shape(name_a);
		mesh->add_blend_shape(name_a);
		mesh->add_blend_shape(name_a);

		CHECK(mesh->get_blend_shape_name(0) == "ShapeA");
		bool all_different = (static_cast<String>(mesh->get_blend_shape_name(0)) != static_cast<String>(mesh->get_blend_shape_name(1))) &&
				(static_cast<String>(mesh->get_blend_shape_name(1)) != static_cast<String>(mesh->get_blend_shape_name(2))) &&
				(static_cast<String>(mesh->get_blend_shape_name(0)) != static_cast<String>(mesh->get_blend_shape_name(2)));
		bool all_have_name = static_cast<String>(mesh->get_blend_shape_name(1)).contains("ShapeA") &&
				static_cast<String>(mesh->get_blend_shape_name(2)).contains("ShapeA");
		CHECK((all_different && all_have_name));
	}

	SUBCASE("ArrayMesh keeps correct count of number of blend shapes") {
		mesh->add_blend_shape(name_a);
		mesh->add_blend_shape(name_a);
		mesh->add_blend_shape(name_b);
		mesh->add_blend_shape(name_b);
		mesh->add_blend_shape(name_b);

		REQUIRE(mesh->get_blend_shape_count() == 5);
	}

	SUBCASE("Adding blend shape after surface is added causes error") {
		Ref<CylinderMesh> cylinder;
		cylinder.instantiate();
		Array cylinder_array;
		cylinder_array.resize(Mesh::ARRAY_MAX);
		cylinder->create_mesh_array(cylinder_array, 3.f, 3.f, 5.f);
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array);

		ERR_PRINT_OFF
		mesh->add_blend_shape(name_a);
		ERR_PRINT_ON
		CHECK(mesh->get_blend_shape_count() == 0);
	}

	SUBCASE("Adding blend shapes once all surfaces have been removed is allowed") {
		Ref<CylinderMesh> cylinder;
		cylinder.instantiate();
		Array cylinder_array;
		cylinder_array.resize(Mesh::ARRAY_MAX);
		cylinder->create_mesh_array(cylinder_array, 3.f, 3.f, 5.f);
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array);
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array);

		mesh->surface_remove(0);
		ERR_PRINT_OFF
		mesh->add_blend_shape(name_a);
		ERR_PRINT_ON
		CHECK(mesh->get_blend_shape_count() == 0);

		mesh->surface_remove(0);
		mesh->add_blend_shape(name_a);
		CHECK(mesh->get_blend_shape_count() == 1);
	}

	SUBCASE("Change blend shape name after adding.") {
		mesh->add_blend_shape(name_a);
		mesh->set_blend_shape_name(0, name_b);

		CHECK(mesh->get_blend_shape_name(0) == name_b);
	}

	SUBCASE("Change blend shape name to the name of one already there, should append number to end") {
		mesh->add_blend_shape(name_a);
		mesh->add_blend_shape(name_b);
		mesh->set_blend_shape_name(0, name_b);

		String name_string = mesh->get_blend_shape_name(0);
		CHECK(name_string.contains("ShapeB"));
		CHECK(name_string.length() > static_cast<String>(name_b).size());
	}

	SUBCASE("Clear all blend shapes before surface has been added.") {
		mesh->add_blend_shape(name_a);
		mesh->add_blend_shape(name_b);
		CHECK(mesh->get_blend_shape_count() == 2);

		mesh->clear_blend_shapes();
		CHECK(mesh->get_blend_shape_count() == 0);
	}

	SUBCASE("Clearing all blend shapes once all surfaces have been removed is allowed") {
		mesh->add_blend_shape(name_a);
		mesh->add_blend_shape(name_b);
		Ref<CylinderMesh> cylinder;
		cylinder.instantiate();
		Array cylinder_array;
		cylinder_array.resize(Mesh::ARRAY_MAX);
		cylinder->create_mesh_array(cylinder_array, 3.f, 3.f, 5.f);
		Array blend_shape;
		blend_shape.resize(Mesh::ARRAY_MAX);
		blend_shape[Mesh::ARRAY_VERTEX] = cylinder_array[Mesh::ARRAY_VERTEX];
		blend_shape[Mesh::ARRAY_NORMAL] = cylinder_array[Mesh::ARRAY_NORMAL];
		blend_shape[Mesh::ARRAY_TANGENT] = cylinder_array[Mesh::ARRAY_TANGENT];
		Array blend_shapes;
		blend_shapes.push_back(blend_shape);
		blend_shapes.push_back(blend_shape);
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array, blend_shapes);
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array, blend_shapes);

		mesh->surface_remove(0);
		ERR_PRINT_OFF
		mesh->clear_blend_shapes();
		ERR_PRINT_ON
		CHECK(mesh->get_blend_shape_count() == 2);

		mesh->surface_remove(0);
		mesh->clear_blend_shapes();
		CHECK(mesh->get_blend_shape_count() == 0);
	}

	SUBCASE("Can't add surface with incorrect number of blend shapes.") {
		mesh->add_blend_shape(name_a);
		mesh->add_blend_shape(name_b);
		Array cylinder_array;
		ERR_PRINT_OFF
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array);
		ERR_PRINT_ON
		CHECK(mesh->get_surface_count() == 0);
	}

	SUBCASE("Can't clear blend shapes after surface had been added.") {
		mesh->add_blend_shape(name_a);
		mesh->add_blend_shape(name_b);
		Ref<CylinderMesh> cylinder;
		cylinder.instantiate();
		Array cylinder_array;
		cylinder_array.resize(Mesh::ARRAY_MAX);
		cylinder->create_mesh_array(cylinder_array, 3.f, 3.f, 5.f);
		Array blend_shape;
		blend_shape.resize(Mesh::ARRAY_MAX);
		blend_shape[Mesh::ARRAY_VERTEX] = cylinder_array[Mesh::ARRAY_VERTEX];
		blend_shape[Mesh::ARRAY_NORMAL] = cylinder_array[Mesh::ARRAY_NORMAL];
		blend_shape[Mesh::ARRAY_TANGENT] = cylinder_array[Mesh::ARRAY_TANGENT];
		Array blend_shapes;
		blend_shapes.push_back(blend_shape);
		blend_shapes.push_back(blend_shape);
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array, blend_shapes);

		ERR_PRINT_OFF
		mesh->clear_blend_shapes();
		ERR_PRINT_ON
		CHECK(mesh->get_blend_shape_count() == 2);
	}

	SUBCASE("Set the blend shape mode of ArrayMesh and underlying mesh RID.") {
		mesh->set_blend_shape_mode(Mesh::BLEND_SHAPE_MODE_RELATIVE);
		CHECK(mesh->get_blend_shape_mode() == Mesh::BLEND_SHAPE_MODE_RELATIVE);
	}
}

TEST_CASE("[SceneTree][ArrayMesh] Surface metadata tests.") {
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	Ref<CylinderMesh> cylinder;
	cylinder.instantiate();
	Array cylinder_array;
	cylinder_array.resize(Mesh::ARRAY_MAX);
	cylinder->create_mesh_array(cylinder_array, 3.f, 3.f, 5.f);
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array);

	Ref<BoxMesh> box;
	box.instantiate();
	Array box_array;
	box_array.resize(Mesh::ARRAY_MAX);
	box->create_mesh_array(box_array, Vector3(2.f, 1.2f, 1.6f));
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, box_array);

	SUBCASE("Add 2 surfaces and count the number of surfaces in the mesh.") {
		REQUIRE(mesh->get_surface_count() == 2);
	}

	SUBCASE("Get the surface array from mesh.") {
		REQUIRE(mesh->surface_get_arrays(0)[0] == cylinder_array[0]);
		REQUIRE(mesh->surface_get_arrays(1)[0] == box_array[0]);
	}

	SUBCASE("Get the array length of a particular surface.") {
		CHECK(mesh->surface_get_array_len(0) == static_cast<Vector<Vector3>>(cylinder_array[RenderingServer::ARRAY_VERTEX]).size());
		CHECK(mesh->surface_get_array_len(1) == static_cast<Vector<Vector3>>(box_array[RenderingServer::ARRAY_VERTEX]).size());
	}

	SUBCASE("Get the index array length of a particular surface.") {
		CHECK(mesh->surface_get_array_index_len(0) == static_cast<Vector<Vector3>>(cylinder_array[RenderingServer::ARRAY_INDEX]).size());
		CHECK(mesh->surface_get_array_index_len(1) == static_cast<Vector<Vector3>>(box_array[RenderingServer::ARRAY_INDEX]).size());
	}

	SUBCASE("Get correct primitive type") {
		CHECK(mesh->surface_get_primitive_type(0) == Mesh::PRIMITIVE_TRIANGLES);
		CHECK(mesh->surface_get_primitive_type(1) == Mesh::PRIMITIVE_TRIANGLES);
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLE_STRIP, box_array);
		CHECK(mesh->surface_get_primitive_type(2) == Mesh::PRIMITIVE_TRIANGLE_STRIP);
	}

	SUBCASE("Returns correct format for the mesh") {
		int format = RS::ARRAY_FORMAT_BLEND_SHAPE_MASK | RS::ARRAY_FORMAT_TEX_UV | RS::ARRAY_FORMAT_INDEX;
		CHECK((mesh->surface_get_format(0) & format) != 0);
		CHECK((mesh->surface_get_format(1) & format) != 0);
	}

	SUBCASE("Set a surface name and retrieve it by name.") {
		mesh->surface_set_name(0, "surf1");
		CHECK(mesh->surface_find_by_name("surf1") == 0);
		CHECK(mesh->surface_get_name(0) == "surf1");
	}

	SUBCASE("Set material to two different surfaces.") {
		Ref<Material> mat;
		mat.instantiate();
		mesh->surface_set_material(0, mat);
		CHECK(mesh->surface_get_material(0) == mat);
		mesh->surface_set_material(1, mat);
		CHECK(mesh->surface_get_material(1) == mat);
	}

	SUBCASE("Set same material multiple times doesn't change material of surface.") {
		Ref<Material> mat;
		mat.instantiate();
		mesh->surface_set_material(0, mat);
		mesh->surface_set_material(0, mat);
		mesh->surface_set_material(0, mat);
		CHECK(mesh->surface_get_material(0) == mat);
	}

	SUBCASE("Set material of surface then change to different material.") {
		Ref<Material> mat1;
		mat1.instantiate();
		Ref<Material> mat2;
		mat2.instantiate();
		mesh->surface_set_material(1, mat1);
		CHECK(mesh->surface_get_material(1) == mat1);
		mesh->surface_set_material(1, mat2);
		CHECK(mesh->surface_get_material(1) == mat2);
	}

	SUBCASE("Get the LOD of the mesh.") {
		Dictionary lod;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array, TypedArray<Array>(), lod);
		CHECK(mesh->surface_get_lods(2) == lod);
	}

	SUBCASE("Get the blend shape arrays from the mesh.") {
		TypedArray<Array> blend;
		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array, blend);
		CHECK(mesh->surface_get_blend_shape_arrays(2) == blend);
	}
}

TEST_CASE("[SceneTree][ArrayMesh] Get/Set mesh metadata and actions") {
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	Ref<CylinderMesh> cylinder;
	cylinder.instantiate();
	Array cylinder_array;
	cylinder_array.resize(Mesh::ARRAY_MAX);
	constexpr float cylinder_radius = 3.f;
	constexpr float cylinder_height = 5.f;
	cylinder->create_mesh_array(cylinder_array, cylinder_radius, cylinder_radius, cylinder_height);
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, cylinder_array);

	Ref<BoxMesh> box;
	box.instantiate();
	Array box_array;
	box_array.resize(Mesh::ARRAY_MAX);
	const Vector3 box_size = Vector3(2.f, 1.2f, 1.6f);
	box->create_mesh_array(box_array, box_size);
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, box_array);

	SUBCASE("Set the shadow mesh.") {
		Ref<ArrayMesh> shadow;
		shadow.instantiate();
		mesh->set_shadow_mesh(shadow);
		CHECK(mesh->get_shadow_mesh() == shadow);
	}

	SUBCASE("Set the shadow mesh multiple times.") {
		Ref<ArrayMesh> shadow;
		shadow.instantiate();
		mesh->set_shadow_mesh(shadow);
		mesh->set_shadow_mesh(shadow);
		mesh->set_shadow_mesh(shadow);
		mesh->set_shadow_mesh(shadow);
		CHECK(mesh->get_shadow_mesh() == shadow);
	}

	SUBCASE("Set the same shadow mesh on multiple meshes.") {
		Ref<ArrayMesh> shadow;
		shadow.instantiate();
		Ref<ArrayMesh> mesh2;
		mesh2.instantiate();
		mesh->set_shadow_mesh(shadow);
		mesh2->set_shadow_mesh(shadow);

		CHECK(mesh->get_shadow_mesh() == shadow);
		CHECK(mesh2->get_shadow_mesh() == shadow);
	}

	SUBCASE("Set the shadow mesh and then change it.") {
		Ref<ArrayMesh> shadow;
		shadow.instantiate();
		mesh->set_shadow_mesh(shadow);
		CHECK(mesh->get_shadow_mesh() == shadow);
		Ref<ArrayMesh> shadow2;
		shadow2.instantiate();
		mesh->set_shadow_mesh(shadow2);
		CHECK(mesh->get_shadow_mesh() == shadow2);
	}

	SUBCASE("Set custom AABB.") {
		AABB bound;
		mesh->set_custom_aabb(bound);
		CHECK(mesh->get_custom_aabb() == bound);
	}

	SUBCASE("Set custom AABB multiple times.") {
		AABB bound;
		mesh->set_custom_aabb(bound);
		mesh->set_custom_aabb(bound);
		mesh->set_custom_aabb(bound);
		mesh->set_custom_aabb(bound);
		CHECK(mesh->get_custom_aabb() == bound);
	}

	SUBCASE("Set custom AABB then change to another AABB.") {
		AABB bound;
		AABB bound2;
		mesh->set_custom_aabb(bound);
		CHECK(mesh->get_custom_aabb() == bound);
		mesh->set_custom_aabb(bound2);
		CHECK(mesh->get_custom_aabb() == bound2);
	}

	SUBCASE("Clear all surfaces should leave zero count.") {
		mesh->clear_surfaces();
		CHECK(mesh->get_surface_count() == 0);
	}

	SUBCASE("Able to get correct mesh RID.") {
		RID rid = mesh->get_rid();
		CHECK(RS::get_singleton()->mesh_get_surface_count(rid) == 2);
	}

	SUBCASE("Create surface from raw SurfaceData data.") {
		RID mesh_rid = mesh->get_rid();
		RS::SurfaceData surface_data = RS::get_singleton()->mesh_get_surface(mesh_rid, 0);
		Ref<ArrayMesh> mesh2;
		mesh2.instantiate();
		mesh2->add_surface(surface_data.format, Mesh::PRIMITIVE_TRIANGLES, surface_data.vertex_data, surface_data.attribute_data,
				surface_data.skin_data, surface_data.vertex_count, surface_data.index_data, surface_data.index_count, surface_data.aabb);
		CHECK(mesh2->get_surface_count() == 1);
		CHECK(mesh2->surface_get_primitive_type(0) == Mesh::PRIMITIVE_TRIANGLES);
		CHECK((mesh2->surface_get_format(0) & surface_data.format) != 0);
		CHECK(mesh2->get_aabb().is_equal_approx(surface_data.aabb));
	}

	SUBCASE("Removing a surface decreases surface count.") {
		REQUIRE(mesh->get_surface_count() == 2);
		mesh->surface_remove(0);
		CHECK(mesh->get_surface_count() == 1);
		mesh->surface_remove(0);
		CHECK(mesh->get_surface_count() == 0);
	}

	SUBCASE("Remove the first surface and check the mesh's AABB.") {
		REQUIRE(mesh->get_surface_count() >= 1);
		mesh->surface_remove(0);
		const AABB box_aabb = AABB(-box_size / 2, box_size);
		CHECK(mesh->get_aabb().is_equal_approx(box_aabb));
	}

	SUBCASE("Remove the last surface and check the mesh's AABB.") {
		REQUIRE(mesh->get_surface_count() >= 1);
		mesh->surface_remove(mesh->get_surface_count() - 1);
		const AABB cylinder_aabb = AABB(Vector3(-cylinder_radius, -cylinder_height / 2, -cylinder_radius),
				Vector3(2 * cylinder_radius, cylinder_height, 2 * cylinder_radius));
		CHECK(mesh->get_aabb().is_equal_approx(cylinder_aabb));
	}

	SUBCASE("Remove all surfaces and check the mesh's AABB.") {
		while (mesh->get_surface_count()) {
			mesh->surface_remove(0);
		}
		CHECK(mesh->get_aabb() == AABB());
	}

	SUBCASE("Removing a non-existent surface causes error.") {
		ERR_PRINT_OFF
		mesh->surface_remove(42);
		ERR_PRINT_ON
		CHECK(mesh->get_surface_count() == 2);
	}
}

} // namespace TestArrayMesh
