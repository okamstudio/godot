/**************************************************************************/
/*  gltf_document.h                                                       */
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

#ifndef GLTF_DOCUMENT_H
#define GLTF_DOCUMENT_H

#include "extensions/gltf_document_extension.h"
#include "extensions/gltf_spec_gloss.h"
#include "gltf_defines.h"
#include "gltf_state.h"

#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/multimesh_instance_3d.h"

#include "modules/modules_enabled.gen.h" // For csg, gridmap.
#ifdef MODULE_CSG_ENABLED
#include "modules/csg/csg_shape.h"
#endif // MODULE_CSG_ENABLED
#ifdef MODULE_GRIDMAP_ENABLED
#include "modules/gridmap/grid_map.h"
#endif // MODULE_GRIDMAP_ENABLED

class GLTFDocument : public Resource {
	GDCLASS(GLTFDocument, Resource);

public:
	const int32_t JOINT_GROUP_SIZE = 4;

	enum {
		ARRAY_BUFFER = 34962,
		ELEMENT_ARRAY_BUFFER = 34963,
	};
	enum {
		TEXTURE_TYPE_GENERIC = 0,
		TEXTURE_TYPE_NORMAL = 1,
	};
	enum RootNodeMode {
		ROOT_NODE_MODE_SINGLE_ROOT,
		ROOT_NODE_MODE_KEEP_ROOT,
		ROOT_NODE_MODE_MULTI_ROOT,
	};

private:
	int _naming_version = 1;
	String _image_format = "PNG";
	float _lossy_quality = 0.75f;
	Ref<GLTFDocumentExtension> _image_save_extension;
	RootNodeMode _root_node_mode = RootNodeMode::ROOT_NODE_MODE_SINGLE_ROOT;

protected:
	static void _bind_methods();
	String _gen_unique_name(Ref<GLTFState> p_state, const String &p_name);
	static Vector<Ref<GLTFDocumentExtension>> all_document_extensions;
	Vector<Ref<GLTFDocumentExtension>> document_extensions;

public:
	static void register_gltf_document_extension(Ref<GLTFDocumentExtension> p_extension, bool p_first_priority = false);
	static void unregister_gltf_document_extension(Ref<GLTFDocumentExtension> p_extension);
	static void unregister_all_gltf_document_extensions();
	static Vector<Ref<GLTFDocumentExtension>> get_all_gltf_document_extensions();
	static Vector<String> get_supported_gltf_extensions();
	static HashSet<String> get_supported_gltf_extensions_hashset();

	static NodePath _find_material_node_path(Ref<GLTFState> p_state, Ref<Material> p_material);
	static Ref<GLTFObjectModelProperty> import_object_model_property(Ref<GLTFState> p_state, const String &p_json_pointer);
	static Ref<GLTFObjectModelProperty> export_object_model_property(Ref<GLTFState> p_state, const NodePath &p_node_path, const Node *p_godot_node, GLTFNodeIndex p_gltf_node_index);

	void set_naming_version(int p_version);
	int get_naming_version() const;
	void set_image_format(const String &p_image_format);
	String get_image_format() const;
	void set_lossy_quality(float p_lossy_quality);
	float get_lossy_quality() const;
	void set_root_node_mode(RootNodeMode p_root_node_mode);
	RootNodeMode get_root_node_mode() const;
	static String _gen_unique_name_static(HashSet<String> &r_unique_names, const String &p_name);

private:
	void _build_parent_hierarchy(Ref<GLTFState> p_state);
	double _filter_number(double p_float);
	void _round_min_max_components(Vector<double> &r_type_min, Vector<double> &r_type_max);
	String _get_component_type_name(const GLTFAccessor::GLTFComponentType p_component_type);
	int _get_component_type_size(const GLTFAccessor::GLTFComponentType p_component_type);
	Error _parse_scenes(Ref<GLTFState> p_state);
	Error _parse_nodes(Ref<GLTFState> p_state);
	String _get_accessor_type_name(const GLTFAccessor::GLTFAccessorType p_accessor_type);
	String _sanitize_animation_name(const String &p_name);
	String _gen_unique_animation_name(Ref<GLTFState> p_state, const String &p_name);
	String _sanitize_bone_name(const String &p_name);
	String _gen_unique_bone_name(Ref<GLTFState> p_state,
			const GLTFSkeletonIndex p_skel_i,
			const String &p_name);
	GLTFTextureIndex _set_texture(Ref<GLTFState> p_state, Ref<Texture2D> p_texture,
			StandardMaterial3D::TextureFilter p_filter_mode, bool p_repeats);
	Ref<Texture2D> _get_texture(Ref<GLTFState> p_state,
			const GLTFTextureIndex p_texture, int p_texture_type);
	GLTFTextureSamplerIndex _set_sampler_for_mode(Ref<GLTFState> p_state,
			StandardMaterial3D::TextureFilter p_filter_mode, bool p_repeats);
	Ref<GLTFTextureSampler> _get_sampler_for_texture(Ref<GLTFState> p_state,
			const GLTFTextureIndex p_texture);
	Error _parse_json(const String &p_path, Ref<GLTFState> p_state);
	Error _parse_glb(Ref<FileAccess> p_file, Ref<GLTFState> p_state);
	void _compute_node_heights(Ref<GLTFState> p_state);
	Error _parse_buffers(Ref<GLTFState> p_state, const String &p_base_path);
	Error _parse_buffer_views(Ref<GLTFState> p_state);
	GLTFAccessor::GLTFAccessorType _get_accessor_type_from_str(const String &p_string);
	Error _parse_accessors(Ref<GLTFState> p_state);
	Error _decode_buffer_view(Ref<GLTFState> p_state, double *p_dst,
			const GLTFBufferViewIndex p_buffer_view,
			const int p_skip_every, const int p_skip_bytes,
			const int p_element_size, const int p_count,
			const GLTFAccessor::GLTFAccessorType p_accessor_type, const int p_component_count,
			const GLTFAccessor::GLTFComponentType p_component_type, const int p_component_size,
			const bool p_normalized, const int p_byte_offset,
			const bool p_for_vertex);
	Vector<double> _decode_accessor(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			const bool p_for_vertex);
	Vector<float> _decode_accessor_as_floats(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			const bool p_for_vertex,
			const Vector<int> &p_packed_vertex_ids = Vector<int>());
	Vector<int> _decode_accessor_as_ints(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			const bool p_for_vertex,
			const Vector<int> &p_packed_vertex_ids = Vector<int>());
	Vector<Vector2> _decode_accessor_as_vec2(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			const bool p_for_vertex,
			const Vector<int> &p_packed_vertex_ids = Vector<int>());
	Vector<Vector3> _decode_accessor_as_vec3(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			const bool p_for_vertex,
			const Vector<int> &p_packed_vertex_ids = Vector<int>());
	Vector<Color> _decode_accessor_as_color(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			const bool p_for_vertex,
			const Vector<int> &p_packed_vertex_ids = Vector<int>());
	Vector<Quaternion> _decode_accessor_as_quaternion(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			const bool p_for_vertex);
	Vector<Transform2D> _decode_accessor_as_xform2d(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			const bool p_for_vertex);
	Vector<Basis> _decode_accessor_as_basis(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			const bool p_for_vertex);
	Vector<Transform3D> _decode_accessor_as_xform(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			const bool p_for_vertex);
	Vector<Variant> _decode_accessor_as_variant(Ref<GLTFState> p_state,
			const GLTFAccessorIndex p_accessor,
			Variant::Type p_variant_type,
			GLTFAccessor::GLTFAccessorType p_accessor_type);
	GLTFAccessorIndex _encode_accessor_as_variant(Ref<GLTFState> p_state,
			Vector<Variant> p_attribs,
			Variant::Type p_variant_type,
			GLTFAccessor::GLTFAccessorType p_accessor_type,
			GLTFAccessor::GLTFComponentType p_component_type = GLTFAccessor::COMPONENT_TYPE_SINGLE_FLOAT);
	Error _parse_meshes(Ref<GLTFState> p_state);
	Error _serialize_textures(Ref<GLTFState> p_state);
	Error _serialize_texture_samplers(Ref<GLTFState> p_state);
	Error _serialize_images(Ref<GLTFState> p_state);
	Error _serialize_lights(Ref<GLTFState> p_state);
	Ref<Image> _parse_image_bytes_into_image(Ref<GLTFState> p_state, const Vector<uint8_t> &p_bytes, const String &p_mime_type, int p_index, String &r_file_extension);
	void _parse_image_save_image(Ref<GLTFState> p_state, const Vector<uint8_t> &p_bytes, const String &p_resource_uri, const String &p_file_extension, int p_index, Ref<Image> p_image);
	Error _parse_images(Ref<GLTFState> p_state, const String &p_base_path);
	Error _parse_textures(Ref<GLTFState> p_state);
	Error _parse_texture_samplers(Ref<GLTFState> p_state);
	Error _parse_materials(Ref<GLTFState> p_state);
	void _set_texture_transform_uv1(const Dictionary &d, Ref<BaseMaterial3D> p_material);
	void spec_gloss_to_rough_metal(Ref<GLTFSpecGloss> r_spec_gloss,
			Ref<BaseMaterial3D> p_material);
	static void spec_gloss_to_metal_base_color(const Color &p_specular_factor,
			const Color &p_diffuse,
			Color &r_base_color,
			float &r_metallic);
	Error _parse_skins(Ref<GLTFState> p_state);
	Error _serialize_skins(Ref<GLTFState> p_state);
	Error _create_skins(Ref<GLTFState> p_state);
	bool _skins_are_same(const Ref<Skin> p_skin_a, const Ref<Skin> p_skin_b);
	void _remove_duplicate_skins(Ref<GLTFState> p_state);
	Error _serialize_cameras(Ref<GLTFState> p_state);
	Error _parse_cameras(Ref<GLTFState> p_state);
	Error _parse_lights(Ref<GLTFState> p_state);
	Error _parse_animations(Ref<GLTFState> p_state);
	void _parse_animation_pointer(Ref<GLTFState> p_state, const String &p_animation_json_pointer, const Ref<GLTFAnimation> p_gltf_animation, const GLTFAnimation::Interpolation p_interp, const Vector<double> &p_times, const int p_output_value_accessor_index);
	Error _serialize_animations(Ref<GLTFState> p_state);
	BoneAttachment3D *_generate_bone_attachment(Ref<GLTFState> p_state,
			Skeleton3D *p_skeleton,
			const GLTFNodeIndex p_node_index,
			const GLTFNodeIndex p_bone_index);
	ImporterMeshInstance3D *_generate_mesh_instance(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index);
	Camera3D *_generate_camera(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index);
	Light3D *_generate_light(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index);
	Node3D *_generate_spatial(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index);
	void _assign_node_names(Ref<GLTFState> p_state);
	template <typename T>
	T _interpolate_track(const Vector<double> &p_times, const Vector<T> &p_values,
			const float p_time,
			const GLTFAnimation::Interpolation p_interp);
	GLTFAccessorIndex _encode_accessor_as_quaternions(Ref<GLTFState> p_state,
			const Vector<Quaternion> p_attribs,
			const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_weights(Ref<GLTFState> p_state,
			const Vector<Color> p_attribs,
			const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_joints(Ref<GLTFState> p_state,
			const Vector<Color> p_attribs,
			const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_floats(Ref<GLTFState> p_state,
			const Vector<double> p_attribs,
			const bool p_for_vertex);
	GLTFAccessorIndex _encode_accessor_as_vec2(Ref<GLTFState> p_state,
			const Vector<Vector2> p_attribs,
			const bool p_for_vertex);

	void _calc_accessor_vec2_min_max(int p_i, const int p_element_count, Vector<double> &p_type_max, Vector2 p_attribs, Vector<double> &p_type_min) {
		if (p_i == 0) {
			for (int32_t type_i = 0; type_i < p_element_count; type_i++) {
				p_type_max.write[type_i] = p_attribs[(p_i * p_element_count) + type_i];
				p_type_min.write[type_i] = p_attribs[(p_i * p_element_count) + type_i];
			}
		}
		for (int32_t type_i = 0; type_i < p_element_count; type_i++) {
			p_type_max.write[type_i] = MAX(p_attribs[(p_i * p_element_count) + type_i], p_type_max[type_i]);
			p_type_min.write[type_i] = MIN(p_attribs[(p_i * p_element_count) + type_i], p_type_min[type_i]);
			p_type_max.write[type_i] = _filter_number(p_type_max.write[type_i]);
			p_type_min.write[type_i] = _filter_number(p_type_min.write[type_i]);
		}
	}

	GLTFAccessorIndex _encode_accessor_as_vec3(Ref<GLTFState> p_state,
			const Vector<Vector3> p_attribs,
			const bool p_for_vertex);
	GLTFAccessorIndex _encode_sparse_accessor_as_vec3(Ref<GLTFState> p_state, const Vector<Vector3> p_attribs, const Vector<Vector3> p_reference_attribs, const float p_reference_multiplier, const bool p_for_vertex, const GLTFAccessorIndex p_reference_accessor);
	GLTFAccessorIndex _encode_accessor_as_color(Ref<GLTFState> p_state,
			const Vector<Color> p_attribs,
			const bool p_for_vertex);

	void _calc_accessor_min_max(int p_i, const int p_element_count, Vector<double> &p_type_max, Vector<double> p_attribs, Vector<double> &p_type_min);

	GLTFAccessorIndex _encode_accessor_as_ints(Ref<GLTFState> p_state,
			const Vector<int32_t> p_attribs,
			const bool p_for_vertex,
			const bool p_for_indices);
	GLTFAccessorIndex _encode_accessor_as_xform(Ref<GLTFState> p_state,
			const Vector<Transform3D> p_attribs,
			const bool p_for_vertex);
	Error _encode_buffer_view(Ref<GLTFState> p_state, const double *p_src,
			const int p_count, const GLTFAccessor::GLTFAccessorType p_accessor_type,
			const GLTFAccessor::GLTFComponentType p_component_type, const bool p_normalized,
			const int p_byte_offset, const bool p_for_vertex,
			GLTFBufferViewIndex &r_accessor, const bool p_for_indices = false);

	Error _encode_accessors(Ref<GLTFState> p_state);
	Error _encode_buffer_views(Ref<GLTFState> p_state);
	Error _serialize_materials(Ref<GLTFState> p_state);
	Error _serialize_meshes(Ref<GLTFState> p_state);
	Error _serialize_nodes(Ref<GLTFState> p_state);
	Error _serialize_scenes(Ref<GLTFState> p_state);
	String interpolation_to_string(const GLTFAnimation::Interpolation p_interp);
	Error _encode_buffer_bins(Ref<GLTFState> p_state, const String &p_path);
	Error _encode_buffer_glb(Ref<GLTFState> p_state, const String &p_path);
	PackedByteArray _serialize_glb_buffer(Ref<GLTFState> p_state, Error *r_err);
	Dictionary _serialize_texture_transform_uv1(Ref<BaseMaterial3D> p_material);
	Dictionary _serialize_texture_transform_uv2(Ref<BaseMaterial3D> p_material);
	Error _serialize_asset_header(Ref<GLTFState> p_state);
	Error _serialize_file(Ref<GLTFState> p_state, const String p_path);
	Error _serialize_gltf_extensions(Ref<GLTFState> p_state) const;

public:
	// https://www.itu.int/rec/R-REC-BT.601
	// https://www.itu.int/dms_pubrec/itu-r/rec/bt/R-REC-BT.601-7-201103-I!!PDF-E.pdf
	static constexpr float R_BRIGHTNESS_COEFF = 0.299f;
	static constexpr float G_BRIGHTNESS_COEFF = 0.587f;
	static constexpr float B_BRIGHTNESS_COEFF = 0.114f;

private:
	// https://github.com/microsoft/glTF-SDK/blob/master/GLTFSDK/Source/PBRUtils.cpp#L9
	// https://bghgary.github.io/glTF/convert-between-workflows-bjs/js/babylon.pbrUtilities.js
	static float solve_metallic(float p_dielectric_specular, float p_diffuse,
			float p_specular,
			float p_one_minus_specular_strength);
	static float get_perceived_brightness(const Color p_color);
	static float get_max_component(const Color &p_color);

public:
	virtual Error append_from_file(String p_path, Ref<GLTFState> p_state, uint32_t p_flags = 0, String p_base_path = String());
	virtual Error append_from_buffer(PackedByteArray p_bytes, String p_base_path, Ref<GLTFState> p_state, uint32_t p_flags = 0);
	virtual Error append_from_scene(Node *p_node, Ref<GLTFState> p_state, uint32_t p_flags = 0);

	virtual Node *generate_scene(Ref<GLTFState> p_state, float p_bake_fps = 30.0f, bool p_trimming = false, bool p_remove_immutable_tracks = true);
	virtual PackedByteArray generate_buffer(Ref<GLTFState> p_state);
	virtual Error write_to_filesystem(Ref<GLTFState> p_state, const String &p_path);

public:
	Error _parse_gltf_state(Ref<GLTFState> p_state, const String &p_search_path);
	Error _parse_asset_header(Ref<GLTFState> p_state);
	Error _parse_gltf_extensions(Ref<GLTFState> p_state);
	void _process_mesh_instances(Ref<GLTFState> p_state, Node *p_scene_root);
	Node *_generate_scene_node_tree(Ref<GLTFState> p_state);
	void _generate_scene_node(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index, Node *p_scene_parent, Node *p_scene_root);
	void _generate_skeleton_bone_node(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index, Node *p_scene_parent, Node *p_scene_root);
	void _import_animation(Ref<GLTFState> p_state, AnimationPlayer *p_animation_player,
			const GLTFAnimationIndex p_index, const bool p_trimming, const bool p_remove_immutable_tracks);
	void _convert_mesh_instances(Ref<GLTFState> p_state);
	GLTFCameraIndex _convert_camera(Ref<GLTFState> p_state, Camera3D *p_camera);
	void _convert_light_to_gltf(Light3D *p_light, Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node);
	GLTFLightIndex _convert_light(Ref<GLTFState> p_state, Light3D *p_light);
	void _convert_spatial(Ref<GLTFState> p_state, Node3D *p_spatial, Ref<GLTFNode> p_node);
	void _convert_scene_node(Ref<GLTFState> p_state, Node *p_current,
			const GLTFNodeIndex p_gltf_current,
			const GLTFNodeIndex p_gltf_root);

#ifdef MODULE_CSG_ENABLED
	void _convert_csg_shape_to_gltf(CSGShape3D *p_current, GLTFNodeIndex p_gltf_parent, Ref<GLTFNode> p_gltf_node, Ref<GLTFState> p_state);
#endif // MODULE_CSG_ENABLED

	void _check_visibility(Node *p_node, bool &r_retflag);
	void _convert_camera_to_gltf(Camera3D *p_camera, Ref<GLTFState> p_state,
			Ref<GLTFNode> p_gltf_node);
#ifdef MODULE_GRIDMAP_ENABLED
	void _convert_grid_map_to_gltf(
			GridMap *p_grid_map,
			GLTFNodeIndex p_parent_node_index,
			GLTFNodeIndex p_root_node_index,
			Ref<GLTFNode> p_gltf_node, Ref<GLTFState> p_state);
#endif // MODULE_GRIDMAP_ENABLED
	void _convert_multi_mesh_instance_to_gltf(
			MultiMeshInstance3D *p_multi_mesh_instance,
			GLTFNodeIndex p_parent_node_index,
			GLTFNodeIndex p_root_node_index,
			Ref<GLTFNode> p_gltf_node, Ref<GLTFState> p_state);
	void _convert_skeleton_to_gltf(
			Skeleton3D *p_scene_parent, Ref<GLTFState> p_state,
			GLTFNodeIndex p_parent_node_index,
			GLTFNodeIndex p_root_node_index,
			Ref<GLTFNode> p_gltf_node);
	void _convert_bone_attachment_to_gltf(BoneAttachment3D *p_bone_attachment,
			Ref<GLTFState> p_state,
			GLTFNodeIndex p_parent_node_index,
			GLTFNodeIndex p_root_node_index,
			Ref<GLTFNode> p_gltf_node);
	void _convert_mesh_instance_to_gltf(MeshInstance3D *p_mesh_instance,
			Ref<GLTFState> p_state,
			Ref<GLTFNode> p_gltf_node);
	GLTFMeshIndex _convert_mesh_to_gltf(Ref<GLTFState> p_state,
			MeshInstance3D *p_mesh_instance);

	GLTFNodeIndex _node_and_or_bone_to_gltf_node_index(Ref<GLTFState> p_state, const Vector<StringName> &p_node_subpath, const Node *p_godot_node);
	bool _convert_animation_node_track(Ref<GLTFState> p_state,
			GLTFAnimation::NodeTrack &p_gltf_node_track,
			const Ref<Animation> &p_godot_animation,
			int32_t p_godot_anim_track_index,
			Vector<double> &p_times);
	void _convert_animation(Ref<GLTFState> p_state, AnimationPlayer *p_animation_player, const String &p_animation_track_name);

	Error _serialize(Ref<GLTFState> p_state);
	Error _parse(Ref<GLTFState> p_state, String p_path, Ref<FileAccess> p_file);
};

VARIANT_ENUM_CAST(GLTFDocument::RootNodeMode);

#endif // GLTF_DOCUMENT_H
