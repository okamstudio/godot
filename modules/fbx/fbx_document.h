/**************************************************************************/
/*  fbx_document.h                                                        */
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

#include "fbx_state.h"

#include "modules/gltf/gltf_defines.h"
#include "modules/gltf/gltf_document.h"

#include <ufbx.h>

class FBXDocument : public GLTFDocument {
	GDCLASS(FBXDocument, GLTFDocument);

public:
	enum {
		TEXTURE_TYPE_GENERIC = 0,
		TEXTURE_TYPE_NORMAL = 1,
	};

	static Transform3D _as_xform(const ufbx_matrix &p_mat);
	static String _as_string(const ufbx_string &p_string);
	static Vector3 _as_vec3(const ufbx_vec3 &p_vector);
	static String _gen_unique_name(HashSet<String> &unique_names, const String &p_name);

public:
	Error append_from_file(String p_path, Ref<GLTFState> p_state, uint32_t p_flags = 0, String p_base_path = String()) override;
	Error append_from_buffer(PackedByteArray p_bytes, String p_base_path, Ref<GLTFState> p_state, uint32_t p_flags = 0) override;
	Error append_from_scene(Node *p_node, Ref<GLTFState> p_state, uint32_t p_flags = 0) override;

	Node *generate_scene(Ref<GLTFState> p_state, float p_bake_fps = 30.0f, bool p_trimming = false, bool p_remove_immutable_tracks = true) override;
	PackedByteArray generate_buffer(Ref<GLTFState> p_state) override;
	Error write_to_filesystem(Ref<GLTFState> p_state, const String &p_path) override;

private:
	String _get_texture_path(const String &p_base_directory, const String &p_source_file_path) const;
	void _process_uv_set(PackedVector2Array &uv_array);
	void _zero_unused_elements(Vector<float> &cur_custom, int start, int end, int num_channels);
	Error _parse_scenes(Ref<FBXState> p_state);
	Error _parse_nodes(Ref<FBXState> p_state);
	String _sanitize_animation_name(const String &p_name);
	String _gen_unique_animation_name(Ref<FBXState> p_state, const String &p_name);
	Ref<Texture2D> _get_texture(Ref<FBXState> p_state,
			const GLTFTextureIndex p_texture, int p_texture_type);
	Error _parse_meshes(Ref<FBXState> p_state);
	Ref<Image> _parse_image_bytes_into_image(Ref<FBXState> p_state, const Vector<uint8_t> &p_bytes, const String &p_filename, int p_index);
	GLTFImageIndex _parse_image_save_image(Ref<FBXState> p_state, const Vector<uint8_t> &p_bytes, const String &p_file_extension, int p_index, Ref<Image> p_image);
	Error _parse_images(Ref<FBXState> p_state, const String &p_base_path);
	Error _parse_materials(Ref<FBXState> p_state);
	Error _parse_skins(Ref<FBXState> p_state);
	Error _parse_animations(Ref<FBXState> p_state);
	BoneAttachment3D *_generate_bone_attachment(Ref<FBXState> p_state,
			Skeleton3D *p_skeleton,
			const GLTFNodeIndex p_node_index,
			const GLTFNodeIndex p_bone_index);
	ImporterMeshInstance3D *_generate_mesh_instance(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index);
	Camera3D *_generate_camera(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index);
	Light3D *_generate_light(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index);
	Node3D *_generate_spatial(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index);
	void _assign_node_names(Ref<FBXState> p_state);
	Error _parse_cameras(Ref<FBXState> p_state);
	Error _parse_lights(Ref<FBXState> p_state);

public:
	Error _parse_fbx_state(Ref<FBXState> p_state, const String &p_search_path);
	void _process_mesh_instances(Ref<FBXState> p_state, Node *p_scene_root);
	void _generate_scene_node(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index, Node *p_scene_parent, Node *p_scene_root);
	void _generate_skeleton_bone_node(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index, Node *p_scene_parent, Node *p_scene_root);
	void _import_animation(Ref<FBXState> p_state, AnimationPlayer *p_animation_player,
			const GLTFAnimationIndex p_index, const bool p_trimming, const bool p_remove_immutable_tracks);
	Error _parse(Ref<FBXState> p_state, String p_path, Ref<FileAccess> p_file);
};
