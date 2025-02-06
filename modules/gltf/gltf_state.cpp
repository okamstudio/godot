/**************************************************************************/
/*  gltf_state.cpp                                                        */
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

#include "gltf_state.h"

#include "gltf_template_convert.h"

void GLTFState::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_used_extension", "extension_name", "required"), &GLTFState::add_used_extension);
	ClassDB::bind_method(D_METHOD("append_data_to_buffers", "data", "deduplication"), &GLTFState::append_data_to_buffers);
	ClassDB::bind_method(D_METHOD("append_gltf_node", "gltf_node", "godot_scene_node", "parent_node_index"), &GLTFState::append_gltf_node);

	ClassDB::bind_method(D_METHOD("get_json"), &GLTFState::get_json);
	ClassDB::bind_method(D_METHOD("set_json", "json"), &GLTFState::set_json);
	ClassDB::bind_method(D_METHOD("get_major_version"), &GLTFState::get_major_version);
	ClassDB::bind_method(D_METHOD("set_major_version", "major_version"), &GLTFState::set_major_version);
	ClassDB::bind_method(D_METHOD("get_minor_version"), &GLTFState::get_minor_version);
	ClassDB::bind_method(D_METHOD("set_minor_version", "minor_version"), &GLTFState::set_minor_version);
	ClassDB::bind_method(D_METHOD("get_copyright"), &GLTFState::get_copyright);
	ClassDB::bind_method(D_METHOD("set_copyright", "copyright"), &GLTFState::set_copyright);
	ClassDB::bind_method(D_METHOD("get_glb_data"), &GLTFState::get_glb_data);
	ClassDB::bind_method(D_METHOD("set_glb_data", "glb_data"), &GLTFState::set_glb_data);
	ClassDB::bind_method(D_METHOD("get_use_named_skin_binds"), &GLTFState::get_use_named_skin_binds);
	ClassDB::bind_method(D_METHOD("set_use_named_skin_binds", "use_named_skin_binds"), &GLTFState::set_use_named_skin_binds);
	ClassDB::bind_method(D_METHOD("get_nodes"), &GLTFState::get_nodes);
	ClassDB::bind_method(D_METHOD("set_nodes", "nodes"), &GLTFState::set_nodes);
	ClassDB::bind_method(D_METHOD("get_buffers"), &GLTFState::get_buffers);
	ClassDB::bind_method(D_METHOD("set_buffers", "buffers"), &GLTFState::set_buffers);
	ClassDB::bind_method(D_METHOD("get_buffer_views"), &GLTFState::get_buffer_views);
	ClassDB::bind_method(D_METHOD("set_buffer_views", "buffer_views"), &GLTFState::set_buffer_views);
	ClassDB::bind_method(D_METHOD("get_accessors"), &GLTFState::get_accessors);
	ClassDB::bind_method(D_METHOD("set_accessors", "accessors"), &GLTFState::set_accessors);
	ClassDB::bind_method(D_METHOD("get_meshes"), &GLTFState::get_meshes);
	ClassDB::bind_method(D_METHOD("set_meshes", "meshes"), &GLTFState::set_meshes);
	ClassDB::bind_method(D_METHOD("get_animation_players_count", "idx"), &GLTFState::get_animation_players_count);
	ClassDB::bind_method(D_METHOD("get_animation_player", "idx"), &GLTFState::get_animation_player);
	ClassDB::bind_method(D_METHOD("get_materials"), &GLTFState::get_materials);
	ClassDB::bind_method(D_METHOD("set_materials", "materials"), &GLTFState::set_materials);
	ClassDB::bind_method(D_METHOD("get_scene_name"), &GLTFState::get_scene_name);
	ClassDB::bind_method(D_METHOD("set_scene_name", "scene_name"), &GLTFState::set_scene_name);
	ClassDB::bind_method(D_METHOD("get_base_path"), &GLTFState::get_base_path);
	ClassDB::bind_method(D_METHOD("set_base_path", "base_path"), &GLTFState::set_base_path);
	ClassDB::bind_method(D_METHOD("get_filename"), &GLTFState::get_filename);
	ClassDB::bind_method(D_METHOD("set_filename", "filename"), &GLTFState::set_filename);
	ClassDB::bind_method(D_METHOD("get_root_nodes"), &GLTFState::get_root_nodes);
	ClassDB::bind_method(D_METHOD("set_root_nodes", "root_nodes"), &GLTFState::set_root_nodes);
	ClassDB::bind_method(D_METHOD("get_textures"), &GLTFState::get_textures);
	ClassDB::bind_method(D_METHOD("set_textures", "textures"), &GLTFState::set_textures);
	ClassDB::bind_method(D_METHOD("get_texture_samplers"), &GLTFState::get_texture_samplers);
	ClassDB::bind_method(D_METHOD("set_texture_samplers", "texture_samplers"), &GLTFState::set_texture_samplers);
	ClassDB::bind_method(D_METHOD("get_images"), &GLTFState::get_images);
	ClassDB::bind_method(D_METHOD("set_images", "images"), &GLTFState::set_images);
	ClassDB::bind_method(D_METHOD("get_skins"), &GLTFState::get_skins);
	ClassDB::bind_method(D_METHOD("set_skins", "skins"), &GLTFState::set_skins);
	ClassDB::bind_method(D_METHOD("get_cameras"), &GLTFState::get_cameras);
	ClassDB::bind_method(D_METHOD("set_cameras", "cameras"), &GLTFState::set_cameras);
	ClassDB::bind_method(D_METHOD("get_lights"), &GLTFState::get_lights);
	ClassDB::bind_method(D_METHOD("set_lights", "lights"), &GLTFState::set_lights);
	ClassDB::bind_method(D_METHOD("get_unique_names"), &GLTFState::get_unique_names);
	ClassDB::bind_method(D_METHOD("set_unique_names", "unique_names"), &GLTFState::set_unique_names);
	ClassDB::bind_method(D_METHOD("get_unique_animation_names"), &GLTFState::get_unique_animation_names);
	ClassDB::bind_method(D_METHOD("set_unique_animation_names", "unique_animation_names"), &GLTFState::set_unique_animation_names);
	ClassDB::bind_method(D_METHOD("get_skeletons"), &GLTFState::get_skeletons);
	ClassDB::bind_method(D_METHOD("set_skeletons", "skeletons"), &GLTFState::set_skeletons);
	ClassDB::bind_method(D_METHOD("get_create_animations"), &GLTFState::get_create_animations);
	ClassDB::bind_method(D_METHOD("set_create_animations", "create_animations"), &GLTFState::set_create_animations);
	ClassDB::bind_method(D_METHOD("get_import_as_skeleton_bones"), &GLTFState::get_import_as_skeleton_bones);
	ClassDB::bind_method(D_METHOD("set_import_as_skeleton_bones", "import_as_skeleton_bones"), &GLTFState::set_import_as_skeleton_bones);
	ClassDB::bind_method(D_METHOD("get_animations"), &GLTFState::get_animations);
	ClassDB::bind_method(D_METHOD("set_animations", "animations"), &GLTFState::set_animations);
	ClassDB::bind_method(D_METHOD("get_scene_node", "idx"), &GLTFState::get_scene_node);
	ClassDB::bind_method(D_METHOD("get_node_index", "scene_node"), &GLTFState::get_node_index);
	ClassDB::bind_method(D_METHOD("get_additional_data", "extension_name"), &GLTFState::get_additional_data);
	ClassDB::bind_method(D_METHOD("set_additional_data", "extension_name", "additional_data"), &GLTFState::set_additional_data);
	ClassDB::bind_method(D_METHOD("get_handle_binary_image"), &GLTFState::get_handle_binary_image);
	ClassDB::bind_method(D_METHOD("set_handle_binary_image", "method"), &GLTFState::set_handle_binary_image);
	ClassDB::bind_method(D_METHOD("set_bake_fps", "value"), &GLTFState::set_bake_fps);
	ClassDB::bind_method(D_METHOD("get_bake_fps"), &GLTFState::get_bake_fps);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "json"), "set_json", "get_json"); // Dictionary
	ADD_PROPERTY(PropertyInfo(Variant::INT, "major_version"), "set_major_version", "get_major_version"); // int
	ADD_PROPERTY(PropertyInfo(Variant::INT, "minor_version"), "set_minor_version", "get_minor_version"); // int
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "copyright"), "set_copyright", "get_copyright"); // String
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "glb_data"), "set_glb_data", "get_glb_data"); // Vector<uint8_t>
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_named_skin_binds"), "set_use_named_skin_binds", "get_use_named_skin_binds"); // bool
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "nodes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_nodes", "get_nodes"); // Vector<Ref<GLTFNode>>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "buffers"), "set_buffers", "get_buffers"); // Vector<Vector<uint8_t>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "buffer_views", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_buffer_views", "get_buffer_views"); // Vector<Ref<GLTFBufferView>>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "accessors", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_accessors", "get_accessors"); // Vector<Ref<GLTFAccessor>>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "meshes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_meshes", "get_meshes"); // Vector<Ref<GLTFMesh>>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "materials", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_materials", "get_materials"); // Vector<Ref<Material>
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "scene_name"), "set_scene_name", "get_scene_name"); // String
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_path"), "set_base_path", "get_base_path"); // String
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "filename"), "set_filename", "get_filename"); // String
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "root_nodes"), "set_root_nodes", "get_root_nodes"); // Vector<int>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "textures", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_textures", "get_textures"); // Vector<Ref<GLTFTexture>>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "texture_samplers", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_texture_samplers", "get_texture_samplers"); //Vector<Ref<GLTFTextureSampler>>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "images", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_images", "get_images"); // Vector<Ref<Texture>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "skins", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_skins", "get_skins"); // Vector<Ref<GLTFSkin>>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "cameras", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_cameras", "get_cameras"); // Vector<Ref<GLTFCamera>>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "lights", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_lights", "get_lights"); // Vector<Ref<GLTFLight>>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "unique_names", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_unique_names", "get_unique_names"); // Set<String>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "unique_animation_names", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_unique_animation_names", "get_unique_animation_names"); // Set<String>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "skeletons", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_skeletons", "get_skeletons"); // Vector<Ref<GLTFSkeleton>>
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "create_animations"), "set_create_animations", "get_create_animations"); // bool
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "import_as_skeleton_bones"), "set_import_as_skeleton_bones", "get_import_as_skeleton_bones"); // bool
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "animations", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_animations", "get_animations"); // Vector<Ref<GLTFAnimation>>
	ADD_PROPERTY(PropertyInfo(Variant::INT, "handle_binary_image", PROPERTY_HINT_ENUM, "Discard All Textures,Extract Textures,Embed as Basis Universal,Embed as Uncompressed", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL | PROPERTY_USAGE_EDITOR), "set_handle_binary_image", "get_handle_binary_image"); // enum
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bake_fps", PROPERTY_HINT_RANGE, "0.001,120,0.0001,or_greater"), "set_bake_fps", "get_bake_fps");

	BIND_CONSTANT(HANDLE_BINARY_DISCARD_TEXTURES);
	BIND_CONSTANT(HANDLE_BINARY_EXTRACT_TEXTURES);
	BIND_CONSTANT(HANDLE_BINARY_EMBED_AS_BASISU);
	BIND_CONSTANT(HANDLE_BINARY_EMBED_AS_UNCOMPRESSED);
}

void GLTFState::add_used_extension(const String &p_extension_name, bool p_required) {
	if (!extensions_used.has(p_extension_name)) {
		extensions_used.push_back(p_extension_name);
	}
	if (p_required) {
		if (!extensions_required.has(p_extension_name)) {
			extensions_required.push_back(p_extension_name);
		}
	}
}

Dictionary GLTFState::get_json() {
	return json;
}

void GLTFState::set_json(Dictionary p_json) {
	json = p_json;
}

int GLTFState::get_major_version() {
	return major_version;
}

void GLTFState::set_major_version(int p_major_version) {
	major_version = p_major_version;
}

int GLTFState::get_minor_version() {
	return minor_version;
}

void GLTFState::set_minor_version(int p_minor_version) {
	minor_version = p_minor_version;
}

String GLTFState::get_copyright() const {
	return copyright;
}

void GLTFState::set_copyright(const String &p_copyright) {
	copyright = p_copyright;
}

Vector<uint8_t> GLTFState::get_glb_data() {
	return glb_data;
}

void GLTFState::set_glb_data(Vector<uint8_t> p_glb_data) {
	glb_data = p_glb_data;
}

bool GLTFState::get_use_named_skin_binds() {
	return use_named_skin_binds;
}

void GLTFState::set_use_named_skin_binds(bool p_use_named_skin_binds) {
	use_named_skin_binds = p_use_named_skin_binds;
}

TypedArray<GLTFNode> GLTFState::get_nodes() {
	return GLTFTemplateConvert::to_array(nodes);
}

void GLTFState::set_nodes(TypedArray<GLTFNode> p_nodes) {
	GLTFTemplateConvert::set_from_array(nodes, p_nodes);
}

TypedArray<PackedByteArray> GLTFState::get_buffers() {
	return GLTFTemplateConvert::to_array(buffers);
}

void GLTFState::set_buffers(TypedArray<PackedByteArray> p_buffers) {
	GLTFTemplateConvert::set_from_array(buffers, p_buffers);
}

TypedArray<GLTFBufferView> GLTFState::get_buffer_views() {
	return GLTFTemplateConvert::to_array(buffer_views);
}

void GLTFState::set_buffer_views(TypedArray<GLTFBufferView> p_buffer_views) {
	GLTFTemplateConvert::set_from_array(buffer_views, p_buffer_views);
}

TypedArray<GLTFAccessor> GLTFState::get_accessors() {
	return GLTFTemplateConvert::to_array(accessors);
}

void GLTFState::set_accessors(TypedArray<GLTFAccessor> p_accessors) {
	GLTFTemplateConvert::set_from_array(accessors, p_accessors);
}

TypedArray<GLTFMesh> GLTFState::get_meshes() {
	return GLTFTemplateConvert::to_array(meshes);
}

void GLTFState::set_meshes(TypedArray<GLTFMesh> p_meshes) {
	GLTFTemplateConvert::set_from_array(meshes, p_meshes);
}

TypedArray<Material> GLTFState::get_materials() {
	return GLTFTemplateConvert::to_array(materials);
}

void GLTFState::set_materials(TypedArray<Material> p_materials) {
	GLTFTemplateConvert::set_from_array(materials, p_materials);
}

String GLTFState::get_scene_name() {
	return scene_name;
}

void GLTFState::set_scene_name(String p_scene_name) {
	scene_name = p_scene_name;
}

PackedInt32Array GLTFState::get_root_nodes() {
	return root_nodes;
}

void GLTFState::set_root_nodes(PackedInt32Array p_root_nodes) {
	root_nodes = p_root_nodes;
}

TypedArray<GLTFTexture> GLTFState::get_textures() {
	return GLTFTemplateConvert::to_array(textures);
}

void GLTFState::set_textures(TypedArray<GLTFTexture> p_textures) {
	GLTFTemplateConvert::set_from_array(textures, p_textures);
}

TypedArray<GLTFTextureSampler> GLTFState::get_texture_samplers() {
	return GLTFTemplateConvert::to_array(texture_samplers);
}

void GLTFState::set_texture_samplers(TypedArray<GLTFTextureSampler> p_texture_samplers) {
	GLTFTemplateConvert::set_from_array(texture_samplers, p_texture_samplers);
}

TypedArray<Texture2D> GLTFState::get_images() {
	return GLTFTemplateConvert::to_array(images);
}

void GLTFState::set_images(TypedArray<Texture2D> p_images) {
	GLTFTemplateConvert::set_from_array(images, p_images);
}

TypedArray<GLTFSkin> GLTFState::get_skins() {
	return GLTFTemplateConvert::to_array(skins);
}

void GLTFState::set_skins(TypedArray<GLTFSkin> p_skins) {
	GLTFTemplateConvert::set_from_array(skins, p_skins);
}

TypedArray<GLTFCamera> GLTFState::get_cameras() {
	return GLTFTemplateConvert::to_array(cameras);
}

void GLTFState::set_cameras(TypedArray<GLTFCamera> p_cameras) {
	GLTFTemplateConvert::set_from_array(cameras, p_cameras);
}

TypedArray<GLTFLight> GLTFState::get_lights() {
	return GLTFTemplateConvert::to_array(lights);
}

void GLTFState::set_lights(TypedArray<GLTFLight> p_lights) {
	GLTFTemplateConvert::set_from_array(lights, p_lights);
}

TypedArray<String> GLTFState::get_unique_names() {
	return GLTFTemplateConvert::to_array(unique_names);
}

void GLTFState::set_unique_names(TypedArray<String> p_unique_names) {
	GLTFTemplateConvert::set_from_array(unique_names, p_unique_names);
}

TypedArray<String> GLTFState::get_unique_animation_names() {
	return GLTFTemplateConvert::to_array(unique_animation_names);
}

void GLTFState::set_unique_animation_names(TypedArray<String> p_unique_animation_names) {
	GLTFTemplateConvert::set_from_array(unique_animation_names, p_unique_animation_names);
}

TypedArray<GLTFSkeleton> GLTFState::get_skeletons() {
	return GLTFTemplateConvert::to_array(skeletons);
}

void GLTFState::set_skeletons(TypedArray<GLTFSkeleton> p_skeletons) {
	GLTFTemplateConvert::set_from_array(skeletons, p_skeletons);
}

bool GLTFState::get_create_animations() {
	return create_animations;
}

void GLTFState::set_create_animations(bool p_create_animations) {
	create_animations = p_create_animations;
}

bool GLTFState::get_import_as_skeleton_bones() {
	return import_as_skeleton_bones;
}

void GLTFState::set_import_as_skeleton_bones(bool p_import_as_skeleton_bones) {
	import_as_skeleton_bones = p_import_as_skeleton_bones;
}

TypedArray<GLTFAnimation> GLTFState::get_animations() {
	return GLTFTemplateConvert::to_array(animations);
}

void GLTFState::set_animations(TypedArray<GLTFAnimation> p_animations) {
	GLTFTemplateConvert::set_from_array(animations, p_animations);
}

Node *GLTFState::get_scene_node(GLTFNodeIndex idx) {
	if (!scene_nodes.has(idx)) {
		return nullptr;
	}
	return scene_nodes[idx];
}

GLTFNodeIndex GLTFState::get_node_index(Node *p_node) {
	for (KeyValue<GLTFNodeIndex, Node *> x : scene_nodes) {
		if (x.value == p_node) {
			return x.key;
		}
	}
	return -1;
}

int GLTFState::get_animation_players_count(int idx) {
	return animation_players.size();
}

AnimationPlayer *GLTFState::get_animation_player(int idx) {
	ERR_FAIL_INDEX_V(idx, animation_players.size(), nullptr);
	return animation_players[idx];
}

void GLTFState::set_discard_meshes_and_materials(bool p_discard_meshes_and_materials) {
	discard_meshes_and_materials = p_discard_meshes_and_materials;
}

bool GLTFState::get_discard_meshes_and_materials() {
	return discard_meshes_and_materials;
}

String GLTFState::get_base_path() {
	return base_path;
}

void GLTFState::set_base_path(const String &p_base_path) {
	base_path = p_base_path;
	if (extract_path.is_empty()) {
		extract_path = p_base_path;
	}
}

String GLTFState::get_extract_path() {
	return extract_path;
}

void GLTFState::set_extract_path(const String &p_extract_path) {
	extract_path = p_extract_path;
}

String GLTFState::get_extract_prefix() {
	return extract_prefix;
}

void GLTFState::set_extract_prefix(const String &p_extract_prefix) {
	extract_prefix = p_extract_prefix;
}

String GLTFState::get_filename() const {
	return filename;
}

void GLTFState::set_filename(const String &p_filename) {
	filename = p_filename;
	if (extract_prefix.is_empty()) {
		extract_prefix = p_filename.get_basename();
	}
}

Variant GLTFState::get_additional_data(const StringName &p_extension_name) {
	return additional_data[p_extension_name];
}

void GLTFState::set_additional_data(const StringName &p_extension_name, Variant p_additional_data) {
	additional_data[p_extension_name] = p_additional_data;
}

GLTFBufferViewIndex GLTFState::append_data_to_buffers(const Vector<uint8_t> &p_data, const bool p_deduplication = false) {
	if (p_deduplication) {
		for (int i = 0; i < buffer_views.size(); i++) {
			Ref<GLTFBufferView> buffer_view = buffer_views[i];
			Vector<uint8_t> buffer_view_data = buffer_view->load_buffer_view_data(this);
			if (buffer_view_data == p_data) {
				return i;
			}
		}
	}
	// Append the given data to a buffer and create a buffer view for it.
	if (unlikely(buffers.is_empty())) {
		buffers.push_back(Vector<uint8_t>());
	}
	Vector<uint8_t> &destination_buffer = buffers.write[0];
	Ref<GLTFBufferView> buffer_view;
	buffer_view.instantiate();
	buffer_view->set_buffer(0);
	buffer_view->set_byte_offset(destination_buffer.size());
	buffer_view->set_byte_length(p_data.size());
	destination_buffer.append_array(p_data);
	const int new_index = buffer_views.size();
	buffer_views.push_back(buffer_view);
	return new_index;
}

GLTFNodeIndex GLTFState::append_gltf_node(Ref<GLTFNode> p_gltf_node, Node *p_godot_scene_node, GLTFNodeIndex p_parent_node_index) {
	p_gltf_node->set_parent(p_parent_node_index);
	const GLTFNodeIndex new_index = nodes.size();
	nodes.append(p_gltf_node);
	scene_nodes.insert(new_index, p_godot_scene_node);
	if (p_parent_node_index == -1) {
		root_nodes.append(new_index);
	} else if (p_parent_node_index < new_index) {
		nodes.write[p_parent_node_index]->append_child_index(new_index);
	}
	return new_index;
}
