/**************************************************************************/
/*  gltf_skin.h                                                           */
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

#include "../gltf_defines.h"

#include "core/io/resource.h"
#include "scene/resources/3d/skin.h"

template <typename T>
class TypedArray;

class GLTFSkin : public Resource {
	GDCLASS(GLTFSkin, Resource);
	friend class GLTFDocument;
	friend class SkinTool;
	friend class FBXDocument;

private:
	// The "skeleton" property defined in the gltf spec. -1 = Scene Root
	GLTFNodeIndex skin_root = -1;

	Vector<GLTFNodeIndex> joints_original;
	Vector<Transform3D> inverse_binds;

	// Note: joints + non_joints should form a complete subtree, or subtrees
	// with a common parent

	// All nodes that are skins that are caught in-between the original joints
	// (inclusive of joints_original)
	Vector<GLTFNodeIndex> joints;

	// All Nodes that are caught in-between skin joint nodes, and are not
	// defined as joints by any skin
	Vector<GLTFNodeIndex> non_joints;

	// The roots of the skin. In the case of multiple roots, their parent *must*
	// be the same (the roots must be siblings)
	Vector<GLTFNodeIndex> roots;

	// The GLTF Skeleton this Skin points to (after we determine skeletons)
	GLTFSkeletonIndex skeleton = -1;

	// A mapping from the joint indices (in the order of joints_original) to the
	// Godot Skeleton's bone_indices
	HashMap<int, int> joint_i_to_bone_i;
	HashMap<int, StringName> joint_i_to_name;

	// The Actual Skin that will be created as a mapping between the IBM's of
	// this skin to the generated skeleton for the mesh instances.
	Ref<Skin> godot_skin;

protected:
	static void _bind_methods();

public:
	GLTFNodeIndex get_skin_root();
	void set_skin_root(GLTFNodeIndex p_skin_root);

	Vector<GLTFNodeIndex> get_joints_original();
	void set_joints_original(Vector<GLTFNodeIndex> p_joints_original);

	TypedArray<Transform3D> get_inverse_binds();
	void set_inverse_binds(TypedArray<Transform3D> p_inverse_binds);

	Vector<GLTFNodeIndex> get_joints();
	void set_joints(Vector<GLTFNodeIndex> p_joints);

	Vector<GLTFNodeIndex> get_non_joints();
	void set_non_joints(Vector<GLTFNodeIndex> p_non_joints);

	Vector<GLTFNodeIndex> get_roots();
	void set_roots(Vector<GLTFNodeIndex> p_roots);

	int get_skeleton();
	void set_skeleton(int p_skeleton);

	Dictionary get_joint_i_to_bone_i();
	void set_joint_i_to_bone_i(Dictionary p_joint_i_to_bone_i);

	Dictionary get_joint_i_to_name();
	void set_joint_i_to_name(Dictionary p_joint_i_to_name);

	Ref<Skin> get_godot_skin();
	void set_godot_skin(Ref<Skin> p_godot_skin);

	Dictionary to_dictionary();
	Error from_dictionary(const Dictionary &dict);
};
