/**************************************************************************/
/*  multimesh_instance_3d.h                                               */
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

#ifndef MULTIMESH_INSTANCE_3D_H
#define MULTIMESH_INSTANCE_3D_H

#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/multimesh.h"

class NavigationMesh;
class NavigationMeshSourceGeometryData3D;

class MultiMeshInstance3D : public GeometryInstance3D {
	GDCLASS(MultiMeshInstance3D, GeometryInstance3D);

	Ref<MultiMesh> multimesh;

	void _refresh_interpolated();

protected:
	virtual void _physics_interpolated_changed() override;
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_multimesh(const Ref<MultiMesh> &p_multimesh);
	Ref<MultiMesh> get_multimesh() const;

	Array get_meshes() const;

	virtual AABB get_aabb() const override;

private:
	static Callable _navmesh_source_geometry_parsing_callback;
	static RID _navmesh_source_geometry_parser;

public:
	static void navmesh_parse_init();
	static void navmesh_parse_source_geometry(const Ref<NavigationMesh> &p_navigation_mesh, Ref<NavigationMeshSourceGeometryData3D> p_source_geometry_data, Node *p_node);

	MultiMeshInstance3D();
	~MultiMeshInstance3D();
};

#endif // MULTIMESH_INSTANCE_3D_H
