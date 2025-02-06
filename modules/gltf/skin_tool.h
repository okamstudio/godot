/**************************************************************************/
/*  skin_tool.h                                                           */
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

#include "gltf_defines.h"

#include "structures/gltf_node.h"
#include "structures/gltf_skeleton.h"
#include "structures/gltf_skin.h"

#include "core/math/disjoint_set.h"
#include "core/templates/rb_set.h"

using SkinNodeIndex = int;
using SkinSkeletonIndex = int;

class SkinTool {
public:
	static String _sanitize_bone_name(const String &p_name);
	static String _gen_unique_bone_name(HashSet<String> &r_unique_names, const String &p_name);
	static SkinNodeIndex _find_highest_node(Vector<Ref<GLTFNode>> &r_nodes, const Vector<SkinNodeIndex> &p_subset);
	static bool _capture_nodes_in_skin(const Vector<Ref<GLTFNode>> &p_nodes, Ref<GLTFSkin> p_skin, const SkinNodeIndex p_node_index);
	static void _capture_nodes_for_multirooted_skin(Vector<Ref<GLTFNode>> &r_nodes, Ref<GLTFSkin> p_skin);
	static void _recurse_children(
			Vector<Ref<GLTFNode>> &r_nodes,
			const SkinNodeIndex p_node_index,
			RBSet<SkinNodeIndex> &r_all_skin_nodes,
			HashSet<SkinNodeIndex> &r_child_visited_set);
	static Error _reparent_non_joint_skeleton_subtrees(
			Vector<Ref<GLTFNode>> &r_nodes,
			Ref<GLTFSkeleton> p_skeleton,
			const Vector<SkinNodeIndex> &p_non_joints);
	static Error _determine_skeleton_roots(
			Vector<Ref<GLTFNode>> &r_nodes,
			Vector<Ref<GLTFSkeleton>> &r_skeletons,
			const SkinSkeletonIndex p_skel_i);
	static Error _map_skin_joints_indices_to_skeleton_bone_indices(
			Vector<Ref<GLTFSkin>> &r_skins,
			Vector<Ref<GLTFSkeleton>> &r_skeletons,
			Vector<Ref<GLTFNode>> &r_nodes);
	static String _gen_unique_name(HashSet<String> &unique_names, const String &p_name);
	static bool _skins_are_same(const Ref<Skin> p_skin_a, const Ref<Skin> p_skin_b);
	static void _remove_duplicate_skins(Vector<Ref<GLTFSkin>> &r_skins);

public:
	static Error _expand_skin(Vector<Ref<GLTFNode>> &r_nodes, Ref<GLTFSkin> p_skin);
	static Error _verify_skin(Vector<Ref<GLTFNode>> &r_nodes, Ref<GLTFSkin> p_skin);
	static Error _asset_parse_skins(
			const Vector<SkinNodeIndex> &p_input_skin_indices,
			const Vector<Ref<GLTFSkin>> &p_input_skins,
			const Vector<Ref<GLTFNode>> &p_input_nodes,
			Vector<SkinNodeIndex> &r_output_skin_indices,
			Vector<Ref<GLTFSkin>> &r_output_skins,
			HashMap<GLTFNodeIndex, bool> &r_joint_mapping);
	static Error _determine_skeletons(
			Vector<Ref<GLTFSkin>> &r_skins,
			Vector<Ref<GLTFNode>> &r_nodes,
			Vector<Ref<GLTFSkeleton>> &r_skeletons,
			const Vector<GLTFNodeIndex> &p_single_skeleton_roots);
	static Error _create_skeletons(
			HashSet<String> &r_unique_names,
			Vector<Ref<GLTFSkin>> &r_skins,
			Vector<Ref<GLTFNode>> &r_nodes,
			HashMap<ObjectID, GLTFSkeletonIndex> &r_skeleton3d_to_fbx_skeleton,
			Vector<Ref<GLTFSkeleton>> &r_skeletons,
			HashMap<GLTFNodeIndex, Node *> &r_scene_nodes);
	static Error _create_skins(Vector<Ref<GLTFSkin>> &skins, Vector<Ref<GLTFNode>> &nodes, bool use_named_skin_binds, HashSet<String> &unique_names);
};
