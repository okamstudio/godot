/**************************************************************************/
/*  xr_face_modifier_3d.h                                                 */
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

#include "mesh_instance_3d.h"
#include "scene/3d/node_3d.h"

/**
	The XRFaceModifier3D node drives the blend shapes of a MeshInstance3D
	with facial expressions from an XRFaceTracking instance.

	The blend shapes provided by the mesh are interrogated, and used to
	deduce an optimal mapping from the Unified Expressions blend shapes
	provided by the	XRFaceTracking instance to drive the face.
 */

class XRFaceModifier3D : public Node3D {
	GDCLASS(XRFaceModifier3D, Node3D);

private:
	StringName tracker_name = "/user/face_tracker";
	NodePath target;

	// Map from XRFaceTracker blend shape index to mesh blend shape index.
	RBMap<int, int> blend_mapping;

	MeshInstance3D *get_mesh_instance() const;
	void _get_blend_data();
	void _update_face_blends() const;

protected:
	static void _bind_methods();

public:
	void set_face_tracker(const StringName &p_tracker_name);
	StringName get_face_tracker() const;

	void set_target(const NodePath &p_target);
	NodePath get_target() const;

	void _notification(int p_what);
};
