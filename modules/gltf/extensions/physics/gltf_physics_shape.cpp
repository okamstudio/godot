/**************************************************************************/
/*  gltf_physics_shape.cpp                                                */
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

#include "gltf_physics_shape.h"

#include "../../gltf_state.h"

#include "core/math/convex_hull.h"
#include "scene/3d/physics/area_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#include "scene/resources/3d/cylinder_shape_3d.h"
#include "scene/resources/3d/importer_mesh.h"
#include "scene/resources/3d/sphere_shape_3d.h"

void GLTFPhysicsShape::_bind_methods() {
	ClassDB::bind_static_method("GLTFPhysicsShape", D_METHOD("from_node", "shape_node"), &GLTFPhysicsShape::from_node);
	ClassDB::bind_method(D_METHOD("to_node", "cache_shapes"), &GLTFPhysicsShape::to_node, DEFVAL(false));

	ClassDB::bind_static_method("GLTFPhysicsShape", D_METHOD("from_resource", "shape_resource"), &GLTFPhysicsShape::from_resource);
	ClassDB::bind_method(D_METHOD("to_resource", "cache_shapes"), &GLTFPhysicsShape::to_resource, DEFVAL(false));

	ClassDB::bind_static_method("GLTFPhysicsShape", D_METHOD("from_dictionary", "dictionary"), &GLTFPhysicsShape::from_dictionary);
	ClassDB::bind_method(D_METHOD("to_dictionary"), &GLTFPhysicsShape::to_dictionary);

	ClassDB::bind_method(D_METHOD("get_shape_type"), &GLTFPhysicsShape::get_shape_type);
	ClassDB::bind_method(D_METHOD("set_shape_type", "shape_type"), &GLTFPhysicsShape::set_shape_type);
	ClassDB::bind_method(D_METHOD("get_size"), &GLTFPhysicsShape::get_size);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &GLTFPhysicsShape::set_size);
	ClassDB::bind_method(D_METHOD("get_radius"), &GLTFPhysicsShape::get_radius);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &GLTFPhysicsShape::set_radius);
	ClassDB::bind_method(D_METHOD("get_height"), &GLTFPhysicsShape::get_height);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &GLTFPhysicsShape::set_height);
	ClassDB::bind_method(D_METHOD("get_is_trigger"), &GLTFPhysicsShape::get_is_trigger);
	ClassDB::bind_method(D_METHOD("set_is_trigger", "is_trigger"), &GLTFPhysicsShape::set_is_trigger);
	ClassDB::bind_method(D_METHOD("get_mesh_index"), &GLTFPhysicsShape::get_mesh_index);
	ClassDB::bind_method(D_METHOD("set_mesh_index", "mesh_index"), &GLTFPhysicsShape::set_mesh_index);
	ClassDB::bind_method(D_METHOD("get_importer_mesh"), &GLTFPhysicsShape::get_importer_mesh);
	ClassDB::bind_method(D_METHOD("set_importer_mesh", "importer_mesh"), &GLTFPhysicsShape::set_importer_mesh);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "shape_type"), "set_shape_type", "get_shape_type");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_trigger"), "set_is_trigger", "get_is_trigger");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_index"), "set_mesh_index", "get_mesh_index");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "importer_mesh", PROPERTY_HINT_RESOURCE_TYPE, "ImporterMesh"), "set_importer_mesh", "get_importer_mesh");
}

String GLTFPhysicsShape::get_shape_type() const {
	return shape_type;
}

void GLTFPhysicsShape::set_shape_type(String p_shape_type) {
	shape_type = p_shape_type;
}

Vector3 GLTFPhysicsShape::get_size() const {
	return size;
}

void GLTFPhysicsShape::set_size(Vector3 p_size) {
	size = p_size;
}

real_t GLTFPhysicsShape::get_radius() const {
	return radius;
}

void GLTFPhysicsShape::set_radius(real_t p_radius) {
	radius = p_radius;
}

real_t GLTFPhysicsShape::get_height() const {
	return height;
}

void GLTFPhysicsShape::set_height(real_t p_height) {
	height = p_height;
}

bool GLTFPhysicsShape::get_is_trigger() const {
	return is_trigger;
}

void GLTFPhysicsShape::set_is_trigger(bool p_is_trigger) {
	is_trigger = p_is_trigger;
}

GLTFMeshIndex GLTFPhysicsShape::get_mesh_index() const {
	return mesh_index;
}

void GLTFPhysicsShape::set_mesh_index(GLTFMeshIndex p_mesh_index) {
	mesh_index = p_mesh_index;
}

Ref<ImporterMesh> GLTFPhysicsShape::get_importer_mesh() const {
	return importer_mesh;
}

void GLTFPhysicsShape::set_importer_mesh(Ref<ImporterMesh> p_importer_mesh) {
	importer_mesh = p_importer_mesh;
}

Ref<ImporterMesh> _convert_hull_points_to_mesh(const Vector<Vector3> &p_hull_points) {
	Ref<ImporterMesh> importer_mesh;
	ERR_FAIL_COND_V_MSG(p_hull_points.size() < 3, importer_mesh, "GLTFPhysicsShape: Convex hull has fewer points (" + itos(p_hull_points.size()) + ") than the minimum of 3. At least 3 points are required in order to save to glTF, since it uses a mesh to represent convex hulls.");
	if (p_hull_points.size() > 255) {
		WARN_PRINT("GLTFPhysicsShape: Convex hull has more points (" + itos(p_hull_points.size()) + ") than the recommended maximum of 255. This may not load correctly in other engines.");
	}
	// Convert the convex hull points into an array of faces.
	Geometry3D::MeshData md;
	Error err = ConvexHullComputer::convex_hull(p_hull_points, md);
	ERR_FAIL_COND_V_MSG(err != OK, importer_mesh, "GLTFPhysicsShape: Failed to compute convex hull.");
	Vector<Vector3> face_vertices;
	for (uint32_t i = 0; i < md.faces.size(); i++) {
		uint32_t index_count = md.faces[i].indices.size();
		for (uint32_t j = 1; j < index_count - 1; j++) {
			face_vertices.append(p_hull_points[md.faces[i].indices[0]]);
			face_vertices.append(p_hull_points[md.faces[i].indices[j]]);
			face_vertices.append(p_hull_points[md.faces[i].indices[j + 1]]);
		}
	}
	// Create an ImporterMesh from the faces.
	importer_mesh.instantiate();
	Array surface_array;
	surface_array.resize(Mesh::ArrayType::ARRAY_MAX);
	surface_array[Mesh::ArrayType::ARRAY_VERTEX] = face_vertices;
	importer_mesh->add_surface(Mesh::PRIMITIVE_TRIANGLES, surface_array);
	return importer_mesh;
}

Ref<GLTFPhysicsShape> GLTFPhysicsShape::from_node(const CollisionShape3D *p_godot_shape_node) {
	Ref<GLTFPhysicsShape> gltf_shape;
	ERR_FAIL_NULL_V_MSG(p_godot_shape_node, gltf_shape, "Tried to create a GLTFPhysicsShape from a CollisionShape3D node, but the given node was null.");
	Ref<Shape3D> shape_resource = p_godot_shape_node->get_shape();
	ERR_FAIL_COND_V_MSG(shape_resource.is_null(), gltf_shape, "Tried to create a GLTFPhysicsShape from a CollisionShape3D node, but the given node had a null shape.");
	gltf_shape = from_resource(shape_resource);
	// Check if the shape is part of a trigger.
	Node *parent = p_godot_shape_node->get_parent();
	if (cast_to<const Area3D>(parent)) {
		gltf_shape->set_is_trigger(true);
	}
	return gltf_shape;
}

CollisionShape3D *GLTFPhysicsShape::to_node(bool p_cache_shapes) {
	CollisionShape3D *godot_shape_node = memnew(CollisionShape3D);
	to_resource(p_cache_shapes); // Sets `_shape_cache`.
	godot_shape_node->set_shape(_shape_cache);
	return godot_shape_node;
}

Ref<GLTFPhysicsShape> GLTFPhysicsShape::from_resource(const Ref<Shape3D> &p_shape_resource) {
	Ref<GLTFPhysicsShape> gltf_shape;
	gltf_shape.instantiate();
	ERR_FAIL_COND_V_MSG(p_shape_resource.is_null(), gltf_shape, "Tried to create a GLTFPhysicsShape from a Shape3D resource, but the given resource was null.");
	if (cast_to<BoxShape3D>(p_shape_resource.ptr())) {
		gltf_shape->shape_type = "box";
		Ref<BoxShape3D> box = p_shape_resource;
		gltf_shape->set_size(box->get_size());
	} else if (cast_to<const CapsuleShape3D>(p_shape_resource.ptr())) {
		gltf_shape->shape_type = "capsule";
		Ref<CapsuleShape3D> capsule = p_shape_resource;
		gltf_shape->set_radius(capsule->get_radius());
		gltf_shape->set_height(capsule->get_height());
	} else if (cast_to<const CylinderShape3D>(p_shape_resource.ptr())) {
		gltf_shape->shape_type = "cylinder";
		Ref<CylinderShape3D> cylinder = p_shape_resource;
		gltf_shape->set_radius(cylinder->get_radius());
		gltf_shape->set_height(cylinder->get_height());
	} else if (cast_to<const SphereShape3D>(p_shape_resource.ptr())) {
		gltf_shape->shape_type = "sphere";
		Ref<SphereShape3D> sphere = p_shape_resource;
		gltf_shape->set_radius(sphere->get_radius());
	} else if (cast_to<const ConvexPolygonShape3D>(p_shape_resource.ptr())) {
		gltf_shape->shape_type = "convex";
		Ref<ConvexPolygonShape3D> convex = p_shape_resource;
		Vector<Vector3> hull_points = convex->get_points();
		Ref<ImporterMesh> importer_mesh = _convert_hull_points_to_mesh(hull_points);
		ERR_FAIL_COND_V_MSG(importer_mesh.is_null(), gltf_shape, "GLTFPhysicsShape: Failed to convert convex hull points to a mesh.");
		gltf_shape->set_importer_mesh(importer_mesh);
	} else if (cast_to<const ConcavePolygonShape3D>(p_shape_resource.ptr())) {
		gltf_shape->shape_type = "trimesh";
		Ref<ConcavePolygonShape3D> concave = p_shape_resource;
		Ref<ImporterMesh> importer_mesh;
		importer_mesh.instantiate();
		Array surface_array;
		surface_array.resize(Mesh::ArrayType::ARRAY_MAX);
		surface_array[Mesh::ArrayType::ARRAY_VERTEX] = concave->get_faces();
		importer_mesh->add_surface(Mesh::PRIMITIVE_TRIANGLES, surface_array);
		gltf_shape->set_importer_mesh(importer_mesh);
	} else {
		ERR_PRINT("Tried to create a GLTFPhysicsShape from a Shape3D, but the given shape '" + String(Variant(p_shape_resource)) +
				"' had an unsupported shape type. Only BoxShape3D, CapsuleShape3D, CylinderShape3D, SphereShape3D, ConcavePolygonShape3D, and ConvexPolygonShape3D are supported.");
	}
	gltf_shape->_shape_cache = p_shape_resource;
	return gltf_shape;
}

Ref<Shape3D> GLTFPhysicsShape::to_resource(bool p_cache_shapes) {
	if (!p_cache_shapes || _shape_cache.is_null()) {
		if (shape_type == "box") {
			Ref<BoxShape3D> box;
			box.instantiate();
			box->set_size(size);
			_shape_cache = box;
		} else if (shape_type == "capsule") {
			Ref<CapsuleShape3D> capsule;
			capsule.instantiate();
			capsule->set_radius(radius);
			capsule->set_height(height);
			_shape_cache = capsule;
		} else if (shape_type == "cylinder") {
			Ref<CylinderShape3D> cylinder;
			cylinder.instantiate();
			cylinder->set_radius(radius);
			cylinder->set_height(height);
			_shape_cache = cylinder;
		} else if (shape_type == "sphere") {
			Ref<SphereShape3D> sphere;
			sphere.instantiate();
			sphere->set_radius(radius);
			_shape_cache = sphere;
		} else if (shape_type == "convex") {
			ERR_FAIL_COND_V_MSG(importer_mesh.is_null(), _shape_cache, "GLTFPhysicsShape: Error converting convex hull shape to a shape resource: The mesh resource is null.");
			Ref<ConvexPolygonShape3D> convex = importer_mesh->get_mesh()->create_convex_shape();
			_shape_cache = convex;
		} else if (shape_type == "trimesh") {
			ERR_FAIL_COND_V_MSG(importer_mesh.is_null(), _shape_cache, "GLTFPhysicsShape: Error converting concave mesh shape to a shape resource: The mesh resource is null.");
			Ref<ConcavePolygonShape3D> concave = importer_mesh->create_trimesh_shape();
			_shape_cache = concave;
		} else {
			ERR_PRINT("GLTFPhysicsShape: Error converting to a shape resource: Shape type '" + shape_type + "' is unknown.");
		}
	}
	return _shape_cache;
}

Ref<GLTFPhysicsShape> GLTFPhysicsShape::from_dictionary(const Dictionary p_dictionary) {
	ERR_FAIL_COND_V_MSG(!p_dictionary.has("type"), Ref<GLTFPhysicsShape>(), "Failed to parse GLTFPhysicsShape, missing required field 'type'.");
	Ref<GLTFPhysicsShape> gltf_shape;
	gltf_shape.instantiate();
	String shape_type = p_dictionary["type"];
	if (shape_type == "hull") {
		shape_type = "convex";
	}
	gltf_shape->shape_type = shape_type;
	if (shape_type != "box" && shape_type != "capsule" && shape_type != "cylinder" && shape_type != "sphere" && shape_type != "convex" && shape_type != "trimesh") {
		ERR_PRINT("GLTFPhysicsShape: Error parsing unknown shape type '" + shape_type + "'. Only box, capsule, cylinder, sphere, convex, and trimesh are supported.");
	}
	Dictionary properties;
	if (p_dictionary.has(shape_type)) {
		properties = p_dictionary[shape_type];
	} else {
		properties = p_dictionary;
	}
	if (properties.has("radius")) {
		gltf_shape->set_radius(properties["radius"]);
	}
	if (properties.has("height")) {
		gltf_shape->set_height(properties["height"]);
	}
	if (properties.has("size")) {
		const Array &arr = properties["size"];
		if (arr.size() == 3) {
			gltf_shape->set_size(Vector3(arr[0], arr[1], arr[2]));
		} else {
			ERR_PRINT("GLTFPhysicsShape: Error parsing the size, it must have exactly 3 numbers.");
		}
	}
	if (properties.has("isTrigger")) {
		gltf_shape->set_is_trigger(properties["isTrigger"]);
	}
	if (properties.has("mesh")) {
		gltf_shape->set_mesh_index(properties["mesh"]);
	}
	if (gltf_shape->get_mesh_index() < 0 && (shape_type == "convex" || shape_type == "trimesh")) [[unlikely]] {
		ERR_PRINT("Error parsing GLTFPhysicsShape: The mesh-based shape type '" + shape_type + "' does not have a valid mesh index.");
	}
	return gltf_shape;
}

Dictionary GLTFPhysicsShape::to_dictionary() const {
	Dictionary gltf_shape;
	gltf_shape["type"] = shape_type;
	Dictionary sub;
	if (shape_type == "box") {
		Array size_array;
		size_array.resize(3);
		size_array[0] = size.x;
		size_array[1] = size.y;
		size_array[2] = size.z;
		sub["size"] = size_array;
	} else if (shape_type == "capsule") {
		sub["radius"] = get_radius();
		sub["height"] = get_height();
	} else if (shape_type == "cylinder") {
		sub["radius"] = get_radius();
		sub["height"] = get_height();
	} else if (shape_type == "sphere") {
		sub["radius"] = get_radius();
	} else if (shape_type == "trimesh" || shape_type == "convex") {
		sub["mesh"] = get_mesh_index();
	}
	gltf_shape[shape_type] = sub;
	return gltf_shape;
}
