/**************************************************************************/
/*  static_body_2d.h                                                      */
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

#ifndef STATIC_BODY_2D_H
#define STATIC_BODY_2D_H

#include "scene/2d/physics/physics_body_2d.h"

class NavigationPolygon;
class NavigationMeshSourceGeometryData2D;

class StaticBody2D : public PhysicsBody2D {
	GDCLASS(StaticBody2D, PhysicsBody2D);

private:
	Vector2 constant_linear_velocity;
	real_t constant_angular_velocity = 0.0;

	Ref<PhysicsMaterial> physics_material_override;

protected:
	static void _bind_methods();

public:
	void set_physics_material_override(const Ref<PhysicsMaterial> &p_physics_material_override);
	Ref<PhysicsMaterial> get_physics_material_override() const;

	void set_constant_linear_velocity(const Vector2 &p_vel);
	void set_constant_angular_velocity(real_t p_vel);

	Vector2 get_constant_linear_velocity() const;
	real_t get_constant_angular_velocity() const;

	StaticBody2D(PhysicsServer2D::BodyMode p_mode = PhysicsServer2D::BODY_MODE_STATIC);

private:
	static Callable _navmesh_source_geometry_parsing_callback;
	static RID _navmesh_source_geometry_parser;

public:
	static void navmesh_parse_init();
	static void navmesh_parse_source_geometry(const Ref<NavigationPolygon> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData2D> p_source_geometry_data, Node *p_node);

private:
	void _reload_physics_characteristics();
};

#endif // STATIC_BODY_2D_H
