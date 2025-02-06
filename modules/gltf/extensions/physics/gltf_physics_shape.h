/**************************************************************************/
/*  gltf_physics_shape.h                                                  */
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

#include "../../gltf_defines.h"

#include "scene/3d/physics/collision_shape_3d.h"

class ImporterMesh;

// GLTFPhysicsShape is an intermediary between Godot's collision shape nodes
// and the OMI_physics_shape extension.
// https://github.com/omigroup/gltf-extensions/tree/main/extensions/2.0/OMI_physics_shape

class GLTFPhysicsShape : public Resource {
	GDCLASS(GLTFPhysicsShape, Resource)

protected:
	static void _bind_methods();

private:
	String shape_type;
	Vector3 size = Vector3(1.0, 1.0, 1.0);
	real_t radius = 0.5;
	real_t height = 2.0;
	bool is_trigger = false;
	GLTFMeshIndex mesh_index = -1;
	Ref<ImporterMesh> importer_mesh = nullptr;
	// Internal only, for caching Godot shape resources. Used in `to_resource` and `to_node`.
	Ref<Shape3D> _shape_cache = nullptr;

public:
	String get_shape_type() const;
	void set_shape_type(String p_shape_type);

	Vector3 get_size() const;
	void set_size(Vector3 p_size);

	real_t get_radius() const;
	void set_radius(real_t p_radius);

	real_t get_height() const;
	void set_height(real_t p_height);

	bool get_is_trigger() const;
	void set_is_trigger(bool p_is_trigger);

	GLTFMeshIndex get_mesh_index() const;
	void set_mesh_index(GLTFMeshIndex p_mesh_index);

	Ref<ImporterMesh> get_importer_mesh() const;
	void set_importer_mesh(Ref<ImporterMesh> p_importer_mesh);

	static Ref<GLTFPhysicsShape> from_node(const CollisionShape3D *p_shape_node);
	CollisionShape3D *to_node(bool p_cache_shapes = false);

	static Ref<GLTFPhysicsShape> from_resource(const Ref<Shape3D> &p_shape_resource);
	Ref<Shape3D> to_resource(bool p_cache_shapes = false);

	static Ref<GLTFPhysicsShape> from_dictionary(const Dictionary p_dictionary);
	Dictionary to_dictionary() const;
};
