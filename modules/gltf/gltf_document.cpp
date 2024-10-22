/**************************************************************************/
/*  gltf_document.cpp                                                     */
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

#include "gltf_document.h"

#include "extensions/gltf_spec_gloss.h"
#include "gltf_state.h"
#include "skin_tool.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_memory.h"
#include "core/io/json.h"
#include "core/io/stream_peer.h"
#include "core/object/object_id.h"
#include "core/version.h"
#include "scene/3d/bone_attachment_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/resources/3d/skin.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/portable_compressed_texture.h"
#include "scene/resources/surface_tool.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#endif

// FIXME: Hardcoded to avoid editor dependency.
#define GLTF_IMPORT_GENERATE_TANGENT_ARRAYS 8
#define GLTF_IMPORT_USE_NAMED_SKIN_BINDS 16
#define GLTF_IMPORT_DISCARD_MESHES_AND_MATERIALS 32
#define GLTF_IMPORT_FORCE_DISABLE_MESH_COMPRESSION 64

#include <stdio.h>
#include <stdlib.h>
#include <cstdint>

static void _attach_extras_to_meta(const Dictionary &p_extras, Ref<Resource> p_node) {
	if (!p_extras.is_empty()) {
		p_node->set_meta("extras", p_extras);
	}
}

static void _attach_meta_to_extras(Ref<Resource> p_node, Dictionary &p_json) {
	if (p_node->has_meta("extras")) {
		Dictionary node_extras = p_node->get_meta("extras");
		if (p_json.has("extras")) {
			Dictionary extras = p_json["extras"];
			extras.merge(node_extras);
		} else {
			p_json["extras"] = node_extras;
		}
	}
}

static Ref<ImporterMesh> _mesh_to_importer_mesh(Ref<Mesh> p_mesh) {
	Ref<ImporterMesh> importer_mesh;
	importer_mesh.instantiate();
	if (p_mesh.is_null()) {
		return importer_mesh;
	}

	Ref<ArrayMesh> array_mesh = p_mesh;
	if (p_mesh->get_blend_shape_count()) {
		ArrayMesh::BlendShapeMode shape_mode = ArrayMesh::BLEND_SHAPE_MODE_NORMALIZED;
		if (array_mesh.is_valid()) {
			shape_mode = array_mesh->get_blend_shape_mode();
		}
		importer_mesh->set_blend_shape_mode(shape_mode);
		for (int morph_i = 0; morph_i < p_mesh->get_blend_shape_count(); morph_i++) {
			importer_mesh->add_blend_shape(p_mesh->get_blend_shape_name(morph_i));
		}
	}
	for (int32_t surface_i = 0; surface_i < p_mesh->get_surface_count(); surface_i++) {
		Array array = p_mesh->surface_get_arrays(surface_i);
		Ref<Material> mat = p_mesh->surface_get_material(surface_i);
		String mat_name;
		if (mat.is_valid()) {
			mat_name = mat->get_name();
		} else {
			// Assign default material when no material is assigned.
			mat = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
		}
		importer_mesh->add_surface(p_mesh->surface_get_primitive_type(surface_i),
				array, p_mesh->surface_get_blend_shape_arrays(surface_i), p_mesh->surface_get_lods(surface_i), mat,
				mat_name, p_mesh->surface_get_format(surface_i));
	}
	importer_mesh->merge_meta_from(*p_mesh);
	return importer_mesh;
}

Error GLTFDocument::_serialize(Ref<GLTFState> p_state) {
	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		Error err = ext->export_preserialize(p_state);
		ERR_CONTINUE(err != OK);
	}

	/* STEP CONVERT MESH INSTANCES */
	_convert_mesh_instances(p_state);

	/* STEP SERIALIZE CAMERAS */
	Error err = _serialize_cameras(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP 3 CREATE SKINS */
	err = _serialize_skins(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE MESHES (we have enough info now) */
	err = _serialize_meshes(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE TEXTURES */
	err = _serialize_materials(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE TEXTURE SAMPLERS */
	err = _serialize_texture_samplers(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE ANIMATIONS */
	err = _serialize_animations(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE ACCESSORS */
	err = _encode_accessors(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE IMAGES */
	err = _serialize_images(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE TEXTURES */
	err = _serialize_textures(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	for (GLTFBufferViewIndex i = 0; i < p_state->buffer_views.size(); i++) {
		p_state->buffer_views.write[i]->buffer = 0;
	}

	/* STEP SERIALIZE BUFFER VIEWS */
	err = _encode_buffer_views(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE NODES */
	err = _serialize_nodes(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE SCENE */
	err = _serialize_scenes(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE LIGHTS */
	err = _serialize_lights(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE EXTENSIONS */
	err = _serialize_gltf_extensions(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	/* STEP SERIALIZE VERSION */
	err = _serialize_asset_header(p_state);
	if (err != OK) {
		return Error::FAILED;
	}

	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		err = ext->export_post(p_state);
		ERR_FAIL_COND_V(err != OK, err);
	}

	return OK;
}

Error GLTFDocument::_serialize_gltf_extensions(Ref<GLTFState> p_state) const {
	Vector<String> extensions_used = p_state->extensions_used;
	Vector<String> extensions_required = p_state->extensions_required;
	if (!p_state->lights.is_empty()) {
		extensions_used.push_back("KHR_lights_punctual");
	}
	if (p_state->use_khr_texture_transform) {
		extensions_used.push_back("KHR_texture_transform");
		extensions_required.push_back("KHR_texture_transform");
	}
	if (!extensions_used.is_empty()) {
		extensions_used.sort();
		p_state->json["extensionsUsed"] = extensions_used;
	}
	if (!extensions_required.is_empty()) {
		extensions_required.sort();
		p_state->json["extensionsRequired"] = extensions_required;
	}
	return OK;
}

Error GLTFDocument::_serialize_scenes(Ref<GLTFState> p_state) {
	// Godot only supports one scene per glTF file.
	Array scenes;
	Dictionary scene_dict;
	scenes.append(scene_dict);
	p_state->json["scenes"] = scenes;
	p_state->json["scene"] = 0;
	// Add nodes to the scene dict.
	if (!p_state->root_nodes.is_empty()) {
		scene_dict["nodes"] = p_state->root_nodes;
	}
	if (!p_state->scene_name.is_empty()) {
		scene_dict["name"] = p_state->scene_name;
	}
	return OK;
}

Error GLTFDocument::_parse_json(const String &p_path, Ref<GLTFState> p_state) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	if (file.is_null()) {
		return err;
	}

	Vector<uint8_t> array;
	array.resize(file->get_length());
	file->get_buffer(array.ptrw(), array.size());
	String text;
	text.parse_utf8((const char *)array.ptr(), array.size());

	JSON json;
	err = json.parse(text);
	if (err != OK) {
		_err_print_error("", p_path.utf8().get_data(), json.get_error_line(), json.get_error_message().utf8().get_data(), false, ERR_HANDLER_SCRIPT);
		return err;
	}
	p_state->json = json.get_data();

	return OK;
}

Error GLTFDocument::_parse_glb(Ref<FileAccess> p_file, Ref<GLTFState> p_state) {
	ERR_FAIL_COND_V(p_file.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_state.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_file->get_position() != 0, ERR_FILE_CANT_READ);
	uint32_t magic = p_file->get_32();
	ERR_FAIL_COND_V(magic != 0x46546C67, ERR_FILE_UNRECOGNIZED); //glTF
	p_file->get_32(); // version
	p_file->get_32(); // length
	uint32_t chunk_length = p_file->get_32();
	uint32_t chunk_type = p_file->get_32();

	ERR_FAIL_COND_V(chunk_type != 0x4E4F534A, ERR_PARSE_ERROR); //JSON
	Vector<uint8_t> json_data;
	json_data.resize(chunk_length);
	uint32_t len = p_file->get_buffer(json_data.ptrw(), chunk_length);
	ERR_FAIL_COND_V(len != chunk_length, ERR_FILE_CORRUPT);

	String text;
	text.parse_utf8((const char *)json_data.ptr(), json_data.size());

	JSON json;
	Error err = json.parse(text);
	if (err != OK) {
		_err_print_error("", "", json.get_error_line(), json.get_error_message().utf8().get_data(), false, ERR_HANDLER_SCRIPT);
		return err;
	}

	p_state->json = json.get_data();

	//data?

	chunk_length = p_file->get_32();
	chunk_type = p_file->get_32();

	if (p_file->eof_reached()) {
		return OK; //all good
	}

	ERR_FAIL_COND_V(chunk_type != 0x004E4942, ERR_PARSE_ERROR); //BIN

	p_state->glb_data.resize(chunk_length);
	len = p_file->get_buffer(p_state->glb_data.ptrw(), chunk_length);
	ERR_FAIL_COND_V(len != chunk_length, ERR_FILE_CORRUPT);

	return OK;
}

static Array _vec3_to_arr(const Vector3 &p_vec3) {
	Array array;
	array.resize(3);
	array[0] = p_vec3.x;
	array[1] = p_vec3.y;
	array[2] = p_vec3.z;
	return array;
}

static Vector3 _arr_to_vec3(const Array &p_array) {
	ERR_FAIL_COND_V(p_array.size() != 3, Vector3());
	return Vector3(p_array[0], p_array[1], p_array[2]);
}

static Array _quaternion_to_array(const Quaternion &p_quaternion) {
	Array array;
	array.resize(4);
	array[0] = p_quaternion.x;
	array[1] = p_quaternion.y;
	array[2] = p_quaternion.z;
	array[3] = p_quaternion.w;
	return array;
}

static Quaternion _arr_to_quaternion(const Array &p_array) {
	ERR_FAIL_COND_V(p_array.size() != 4, Quaternion());
	return Quaternion(p_array[0], p_array[1], p_array[2], p_array[3]);
}

static Transform3D _arr_to_xform(const Array &p_array) {
	ERR_FAIL_COND_V(p_array.size() != 16, Transform3D());

	Transform3D xform;
	xform.basis.set_column(Vector3::AXIS_X, Vector3(p_array[0], p_array[1], p_array[2]));
	xform.basis.set_column(Vector3::AXIS_Y, Vector3(p_array[4], p_array[5], p_array[6]));
	xform.basis.set_column(Vector3::AXIS_Z, Vector3(p_array[8], p_array[9], p_array[10]));
	xform.set_origin(Vector3(p_array[12], p_array[13], p_array[14]));

	return xform;
}

static Vector<real_t> _xform_to_array(const Transform3D p_transform) {
	Vector<real_t> array;
	array.resize(16);
	Vector3 axis_x = p_transform.get_basis().get_column(Vector3::AXIS_X);
	array.write[0] = axis_x.x;
	array.write[1] = axis_x.y;
	array.write[2] = axis_x.z;
	array.write[3] = 0.0f;
	Vector3 axis_y = p_transform.get_basis().get_column(Vector3::AXIS_Y);
	array.write[4] = axis_y.x;
	array.write[5] = axis_y.y;
	array.write[6] = axis_y.z;
	array.write[7] = 0.0f;
	Vector3 axis_z = p_transform.get_basis().get_column(Vector3::AXIS_Z);
	array.write[8] = axis_z.x;
	array.write[9] = axis_z.y;
	array.write[10] = axis_z.z;
	array.write[11] = 0.0f;
	Vector3 origin = p_transform.get_origin();
	array.write[12] = origin.x;
	array.write[13] = origin.y;
	array.write[14] = origin.z;
	array.write[15] = 1.0f;
	return array;
}

Error GLTFDocument::_serialize_nodes(Ref<GLTFState> p_state) {
	Array nodes;
	for (int i = 0; i < p_state->nodes.size(); i++) {
		Dictionary node;
		Ref<GLTFNode> gltf_node = p_state->nodes[i];
		Dictionary extensions;
		node["extensions"] = extensions;
		if (!gltf_node->get_name().is_empty()) {
			node["name"] = gltf_node->get_name();
		}
		if (gltf_node->camera != -1) {
			node["camera"] = gltf_node->camera;
		}
		if (gltf_node->light != -1) {
			Dictionary lights_punctual;
			extensions["KHR_lights_punctual"] = lights_punctual;
			lights_punctual["light"] = gltf_node->light;
		}
		if (gltf_node->mesh != -1) {
			node["mesh"] = gltf_node->mesh;
		}
		if (gltf_node->skin != -1) {
			node["skin"] = gltf_node->skin;
		}
		if (gltf_node->skeleton != -1 && gltf_node->skin < 0) {
		}
		if (gltf_node->transform.basis.is_orthogonal()) {
			// An orthogonal transform is decomposable into TRS, so prefer that.
			const Vector3 position = gltf_node->get_position();
			if (!position.is_zero_approx()) {
				node["translation"] = _vec3_to_arr(position);
			}
			const Quaternion rotation = gltf_node->get_rotation();
			if (!rotation.is_equal_approx(Quaternion())) {
				node["rotation"] = _quaternion_to_array(rotation);
			}
			const Vector3 scale = gltf_node->get_scale();
			if (!scale.is_equal_approx(Vector3(1.0f, 1.0f, 1.0f))) {
				node["scale"] = _vec3_to_arr(scale);
			}
		} else {
			node["matrix"] = _xform_to_array(gltf_node->transform);
		}
		if (gltf_node->children.size()) {
			Array children;
			for (int j = 0; j < gltf_node->children.size(); j++) {
				children.push_back(gltf_node->children[j]);
			}
			node["children"] = children;
		}

		Node *scene_node = nullptr;
		if (i < (int)p_state->scene_nodes.size()) {
			scene_node = p_state->scene_nodes[i];
		}
		for (Ref<GLTFDocumentExtension> ext : document_extensions) {
			ERR_CONTINUE(ext.is_null());
			Error err = ext->export_node(p_state, gltf_node, node, scene_node);
			ERR_CONTINUE(err != OK);
		}

		if (extensions.is_empty()) {
			node.erase("extensions");
		}
		_attach_meta_to_extras(gltf_node, node);
		nodes.push_back(node);
	}
	if (!nodes.is_empty()) {
		p_state->json["nodes"] = nodes;
	}
	return OK;
}

String GLTFDocument::_gen_unique_name(Ref<GLTFState> p_state, const String &p_name) {
	return _gen_unique_name_static(p_state->unique_names, p_name);
}

String GLTFDocument::_sanitize_animation_name(const String &p_name) {
	String anim_name = p_name.validate_node_name();
	return AnimationLibrary::validate_library_name(anim_name);
}

String GLTFDocument::_gen_unique_animation_name(Ref<GLTFState> p_state, const String &p_name) {
	const String s_name = _sanitize_animation_name(p_name);

	String u_name;
	int index = 1;
	while (true) {
		u_name = s_name;

		if (index > 1) {
			u_name += itos(index);
		}
		if (!p_state->unique_animation_names.has(u_name)) {
			break;
		}
		index++;
	}

	p_state->unique_animation_names.insert(u_name);

	return u_name;
}

String GLTFDocument::_sanitize_bone_name(const String &p_name) {
	String bone_name = p_name;
	bone_name = bone_name.replace(":", "_");
	bone_name = bone_name.replace("/", "_");
	return bone_name;
}

String GLTFDocument::_gen_unique_bone_name(Ref<GLTFState> p_state, const GLTFSkeletonIndex p_skel_i, const String &p_name) {
	String s_name = _sanitize_bone_name(p_name);
	if (s_name.is_empty()) {
		s_name = "bone";
	}
	String u_name;
	int index = 1;
	while (true) {
		u_name = s_name;

		if (index > 1) {
			u_name += "_" + itos(index);
		}
		if (!p_state->skeletons[p_skel_i]->unique_names.has(u_name)) {
			break;
		}
		index++;
	}

	p_state->skeletons.write[p_skel_i]->unique_names.insert(u_name);

	return u_name;
}

Error GLTFDocument::_parse_scenes(Ref<GLTFState> p_state) {
	p_state->unique_names.insert("Skeleton3D"); // Reserve skeleton name.
	ERR_FAIL_COND_V(!p_state->json.has("scenes"), ERR_FILE_CORRUPT);
	const Array &scenes = p_state->json["scenes"];
	int loaded_scene = 0;
	if (p_state->json.has("scene")) {
		loaded_scene = p_state->json["scene"];
	} else {
		WARN_PRINT("The load-time scene is not defined in the glTF2 file. Picking the first scene.");
	}

	if (scenes.size()) {
		ERR_FAIL_COND_V(loaded_scene >= scenes.size(), ERR_FILE_CORRUPT);
		const Dictionary &scene_dict = scenes[loaded_scene];
		ERR_FAIL_COND_V(!scene_dict.has("nodes"), ERR_UNAVAILABLE);
		const Array &nodes = scene_dict["nodes"];
		for (int j = 0; j < nodes.size(); j++) {
			p_state->root_nodes.push_back(nodes[j]);
		}
		// Determine what to use for the scene name.
		if (scene_dict.has("name") && !String(scene_dict["name"]).is_empty() && !((String)scene_dict["name"]).begins_with("Scene")) {
			p_state->scene_name = scene_dict["name"];
		} else if (p_state->scene_name.is_empty()) {
			p_state->scene_name = p_state->filename;
		}
		if (_naming_version == 0) {
			p_state->scene_name = _gen_unique_name(p_state, p_state->scene_name);
		}
	}

	return OK;
}

Error GLTFDocument::_parse_nodes(Ref<GLTFState> p_state) {
	ERR_FAIL_COND_V(!p_state->json.has("nodes"), ERR_FILE_CORRUPT);
	const Array &nodes = p_state->json["nodes"];
	for (int i = 0; i < nodes.size(); i++) {
		Ref<GLTFNode> node;
		node.instantiate();
		const Dictionary &n = nodes[i];

		if (n.has("name")) {
			node->set_original_name(n["name"]);
			node->set_name(n["name"]);
		}
		if (n.has("camera")) {
			node->camera = n["camera"];
		}
		if (n.has("mesh")) {
			node->mesh = n["mesh"];
		}
		if (n.has("skin")) {
			node->skin = n["skin"];
		}
		if (n.has("matrix")) {
			node->transform = _arr_to_xform(n["matrix"]);
		} else {
			if (n.has("translation")) {
				node->set_position(_arr_to_vec3(n["translation"]));
			}
			if (n.has("rotation")) {
				node->set_rotation(_arr_to_quaternion(n["rotation"]));
			}
			if (n.has("scale")) {
				node->set_scale(_arr_to_vec3(n["scale"]));
			}
		}
		node->set_additional_data("GODOT_rest_transform", node->transform);

		if (n.has("extensions")) {
			Dictionary extensions = n["extensions"];
			if (extensions.has("KHR_lights_punctual")) {
				Dictionary lights_punctual = extensions["KHR_lights_punctual"];
				if (lights_punctual.has("light")) {
					GLTFLightIndex light = lights_punctual["light"];
					node->light = light;
				}
			}
			for (Ref<GLTFDocumentExtension> ext : document_extensions) {
				ERR_CONTINUE(ext.is_null());
				Error err = ext->parse_node_extensions(p_state, node, extensions);
				ERR_CONTINUE_MSG(err != OK, "glTF: Encountered error " + itos(err) + " when parsing node extensions for node " + node->get_name() + " in file " + p_state->filename + ". Continuing.");
			}
		}

		if (n.has("extras")) {
			_attach_extras_to_meta(n["extras"], node);
		}

		if (n.has("children")) {
			const Array &children = n["children"];
			for (int j = 0; j < children.size(); j++) {
				node->children.push_back(children[j]);
			}
		}

		p_state->nodes.push_back(node);
	}

	// build the hierarchy
	for (GLTFNodeIndex node_i = 0; node_i < p_state->nodes.size(); node_i++) {
		for (int j = 0; j < p_state->nodes[node_i]->children.size(); j++) {
			GLTFNodeIndex child_i = p_state->nodes[node_i]->children[j];

			ERR_FAIL_INDEX_V(child_i, p_state->nodes.size(), ERR_FILE_CORRUPT);
			ERR_CONTINUE(p_state->nodes[child_i]->parent != -1); //node already has a parent, wtf.

			p_state->nodes.write[child_i]->parent = node_i;
		}
	}

	_compute_node_heights(p_state);

	return OK;
}

void GLTFDocument::_compute_node_heights(Ref<GLTFState> p_state) {
	p_state->root_nodes.clear();
	for (GLTFNodeIndex node_i = 0; node_i < p_state->nodes.size(); ++node_i) {
		Ref<GLTFNode> node = p_state->nodes[node_i];
		node->height = 0;

		GLTFNodeIndex current_i = node_i;
		while (current_i >= 0) {
			const GLTFNodeIndex parent_i = p_state->nodes[current_i]->parent;
			if (parent_i >= 0) {
				++node->height;
			}
			current_i = parent_i;
		}

		if (node->height == 0) {
			p_state->root_nodes.push_back(node_i);
		}
	}
}

static Vector<uint8_t> _parse_base64_uri(const String &p_uri) {
	int start = p_uri.find(",");
	ERR_FAIL_COND_V(start == -1, Vector<uint8_t>());

	CharString substr = p_uri.substr(start + 1).ascii();

	int strlen = substr.length();

	Vector<uint8_t> buf;
	buf.resize(strlen / 4 * 3 + 1 + 1);

	size_t len = 0;
	ERR_FAIL_COND_V(CryptoCore::b64_decode(buf.ptrw(), buf.size(), &len, (unsigned char *)substr.get_data(), strlen) != OK, Vector<uint8_t>());

	buf.resize(len);

	return buf;
}

Error GLTFDocument::_encode_buffer_glb(Ref<GLTFState> p_state, const String &p_path) {
	print_verbose("glTF: Total buffers: " + itos(p_state->buffers.size()));

	if (p_state->buffers.is_empty()) {
		return OK;
	}
	Array buffers;
	if (!p_state->buffers.is_empty()) {
		Vector<uint8_t> buffer_data = p_state->buffers[0];
		Dictionary gltf_buffer;

		gltf_buffer["byteLength"] = buffer_data.size();
		buffers.push_back(gltf_buffer);
	}

	for (GLTFBufferIndex i = 1; i < p_state->buffers.size() - 1; i++) {
		Vector<uint8_t> buffer_data = p_state->buffers[i];
		Dictionary gltf_buffer;
		String filename = p_path.get_basename().get_file() + itos(i) + ".bin";
		String path = p_path.get_base_dir() + "/" + filename;
		Error err;
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &err);
		if (file.is_null()) {
			return err;
		}
		if (buffer_data.size() == 0) {
			return OK;
		}
		file->create(FileAccess::ACCESS_RESOURCES);
		file->store_buffer(buffer_data.ptr(), buffer_data.size());
		gltf_buffer["uri"] = filename;
		gltf_buffer["byteLength"] = buffer_data.size();
		buffers.push_back(gltf_buffer);
	}
	p_state->json["buffers"] = buffers;

	return OK;
}

Error GLTFDocument::_encode_buffer_bins(Ref<GLTFState> p_state, const String &p_path) {
	print_verbose("glTF: Total buffers: " + itos(p_state->buffers.size()));

	if (p_state->buffers.is_empty()) {
		return OK;
	}
	Array buffers;

	for (GLTFBufferIndex i = 0; i < p_state->buffers.size(); i++) {
		Vector<uint8_t> buffer_data = p_state->buffers[i];
		Dictionary gltf_buffer;
		String filename = p_path.get_basename().get_file() + itos(i) + ".bin";
		String path = p_path.get_base_dir() + "/" + filename;
		Error err;
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &err);
		if (file.is_null()) {
			return err;
		}
		if (buffer_data.size() == 0) {
			return OK;
		}
		file->create(FileAccess::ACCESS_RESOURCES);
		file->store_buffer(buffer_data.ptr(), buffer_data.size());
		gltf_buffer["uri"] = filename;
		gltf_buffer["byteLength"] = buffer_data.size();
		buffers.push_back(gltf_buffer);
	}
	p_state->json["buffers"] = buffers;

	return OK;
}

Error GLTFDocument::_parse_buffers(Ref<GLTFState> p_state, const String &p_base_path) {
	if (!p_state->json.has("buffers")) {
		return OK;
	}

	const Array &buffers = p_state->json["buffers"];
	for (GLTFBufferIndex i = 0; i < buffers.size(); i++) {
		if (i == 0 && p_state->glb_data.size()) {
			p_state->buffers.push_back(p_state->glb_data);

		} else {
			const Dictionary &buffer = buffers[i];
			if (buffer.has("uri")) {
				Vector<uint8_t> buffer_data;
				String uri = buffer["uri"];

				if (uri.begins_with("data:")) { // Embedded data using base64.
					// Validate data MIME types and throw an error if it's one we don't know/support.
					if (!uri.begins_with("data:application/octet-stream;base64") &&
							!uri.begins_with("data:application/gltf-buffer;base64")) {
						ERR_PRINT("glTF: Got buffer with an unknown URI data type: " + uri);
					}
					buffer_data = _parse_base64_uri(uri);
				} else { // Relative path to an external image file.
					ERR_FAIL_COND_V(p_base_path.is_empty(), ERR_INVALID_PARAMETER);
					uri = uri.uri_decode();
					uri = p_base_path.path_join(uri).replace("\\", "/"); // Fix for Windows.
					ERR_FAIL_COND_V_MSG(!FileAccess::exists(uri), ERR_FILE_NOT_FOUND, "glTF: Binary file not found: " + uri);
					buffer_data = FileAccess::get_file_as_bytes(uri);
					ERR_FAIL_COND_V_MSG(buffer_data.is_empty(), ERR_PARSE_ERROR, "glTF: Couldn't load binary file as an array: " + uri);
				}

				ERR_FAIL_COND_V(!buffer.has("byteLength"), ERR_PARSE_ERROR);
				int byteLength = buffer["byteLength"];
				ERR_FAIL_COND_V(byteLength < buffer_data.size(), ERR_PARSE_ERROR);
				p_state->buffers.push_back(buffer_data);
			}
		}
	}

	print_verbose("glTF: Total buffers: " + itos(p_state->buffers.size()));

	return OK;
}

Error GLTFDocument::_encode_buffer_views(Ref<GLTFState> p_state) {
	Array buffers;
	for (GLTFBufferViewIndex i = 0; i < p_state->buffer_views.size(); i++) {
		Dictionary d;

		Ref<GLTFBufferView> buffer_view = p_state->buffer_views[i];

		d["buffer"] = buffer_view->buffer;
		d["byteLength"] = buffer_view->byte_length;

		d["byteOffset"] = buffer_view->byte_offset;

		if (buffer_view->byte_stride != -1) {
			d["byteStride"] = buffer_view->byte_stride;
		}

		if (buffer_view->indices) {
			d["target"] = GLTFDocument::ELEMENT_ARRAY_BUFFER;
		} else if (buffer_view->vertex_attributes) {
			d["target"] = GLTFDocument::ARRAY_BUFFER;
		}

		ERR_FAIL_COND_V(!d.has("buffer"), ERR_INVALID_DATA);
		ERR_FAIL_COND_V(!d.has("byteLength"), ERR_INVALID_DATA);
		buffers.push_back(d);
	}
	print_verbose("glTF: Total buffer views: " + itos(p_state->buffer_views.size()));
	if (!buffers.size()) {
		return OK;
	}
	p_state->json["bufferViews"] = buffers;
	return OK;
}

Error GLTFDocument::_parse_buffer_views(Ref<GLTFState> p_state) {
	if (!p_state->json.has("bufferViews")) {
		return OK;
	}
	const Array &buffers = p_state->json["bufferViews"];
	for (GLTFBufferViewIndex i = 0; i < buffers.size(); i++) {
		const Dictionary &d = buffers[i];

		Ref<GLTFBufferView> buffer_view;
		buffer_view.instantiate();

		ERR_FAIL_COND_V(!d.has("buffer"), ERR_PARSE_ERROR);
		buffer_view->buffer = d["buffer"];
		ERR_FAIL_COND_V(!d.has("byteLength"), ERR_PARSE_ERROR);
		buffer_view->byte_length = d["byteLength"];

		if (d.has("byteOffset")) {
			buffer_view->byte_offset = d["byteOffset"];
		}

		if (d.has("byteStride")) {
			buffer_view->byte_stride = d["byteStride"];
		}

		if (d.has("target")) {
			const int target = d["target"];
			buffer_view->indices = target == GLTFDocument::ELEMENT_ARRAY_BUFFER;
			buffer_view->vertex_attributes = target == GLTFDocument::ARRAY_BUFFER;
		}

		p_state->buffer_views.push_back(buffer_view);
	}

	print_verbose("glTF: Total buffer views: " + itos(p_state->buffer_views.size()));

	return OK;
}

Error GLTFDocument::_encode_accessors(Ref<GLTFState> p_state) {
	Array accessors;
	for (GLTFAccessorIndex i = 0; i < p_state->accessors.size(); i++) {
		Dictionary d;

		Ref<GLTFAccessor> accessor = p_state->accessors[i];
		d["componentType"] = accessor->component_type;
		d["count"] = accessor->count;
		d["type"] = _get_accessor_type_name(accessor->accessor_type);
		d["normalized"] = accessor->normalized;
		d["max"] = accessor->max;
		d["min"] = accessor->min;
		if (accessor->buffer_view != -1) {
			// bufferView may be omitted to zero-initialize the buffer. When this happens, byteOffset MUST also be omitted.
			d["byteOffset"] = accessor->byte_offset;
			d["bufferView"] = accessor->buffer_view;
		}

		if (accessor->sparse_count > 0) {
			Dictionary s;
			s["count"] = accessor->sparse_count;

			Dictionary si;
			si["bufferView"] = accessor->sparse_indices_buffer_view;
			si["componentType"] = accessor->sparse_indices_component_type;
			if (accessor->sparse_indices_byte_offset != -1) {
				si["byteOffset"] = accessor->sparse_indices_byte_offset;
			}
			ERR_FAIL_COND_V(!si.has("bufferView") || !si.has("componentType"), ERR_PARSE_ERROR);
			s["indices"] = si;

			Dictionary sv;
			sv["bufferView"] = accessor->sparse_values_buffer_view;
			if (accessor->sparse_values_byte_offset != -1) {
				sv["byteOffset"] = accessor->sparse_values_byte_offset;
			}
			ERR_FAIL_COND_V(!sv.has("bufferView"), ERR_PARSE_ERROR);
			s["values"] = sv;

			ERR_FAIL_COND_V(!s.has("count") || !s.has("indices") || !s.has("values"), ERR_PARSE_ERROR);
			d["sparse"] = s;
		}

		accessors.push_back(d);
	}

	if (!accessors.size()) {
		return OK;
	}
	p_state->json["accessors"] = accessors;
	ERR_FAIL_COND_V(!p_state->json.has("accessors"), ERR_FILE_CORRUPT);
	print_verbose("glTF: Total accessors: " + itos(p_state->accessors.size()));

	return OK;
}

String GLTFDocument::_get_accessor_type_name(const GLTFAccessor::GLTFAccessorType p_accessor_type) {
	if (p_accessor_type == GLTFAccessor::TYPE_SCALAR) {
		return "SCALAR";
	}
	if (p_accessor_type == GLTFAccessor::TYPE_VEC2) {
		return "VEC2";
	}
	if (p_accessor_type == GLTFAccessor::TYPE_VEC3) {
		return "VEC3";
	}
	if (p_accessor_type == GLTFAccessor::TYPE_VEC4) {
		return "VEC4";
	}

	if (p_accessor_type == GLTFAccessor::TYPE_MAT2) {
		return "MAT2";
	}
	if (p_accessor_type == GLTFAccessor::TYPE_MAT3) {
		return "MAT3";
	}
	if (p_accessor_type == GLTFAccessor::TYPE_MAT4) {
		return "MAT4";
	}
	ERR_FAIL_V("SCALAR");
}

GLTFAccessor::GLTFAccessorType GLTFDocument::_get_accessor_type_from_str(const String &p_string) {
	if (p_string == "SCALAR") {
		return GLTFAccessor::TYPE_SCALAR;
	}

	if (p_string == "VEC2") {
		return GLTFAccessor::TYPE_VEC2;
	}
	if (p_string == "VEC3") {
		return GLTFAccessor::TYPE_VEC3;
	}
	if (p_string == "VEC4") {
		return GLTFAccessor::TYPE_VEC4;
	}

	if (p_string == "MAT2") {
		return GLTFAccessor::TYPE_MAT2;
	}
	if (p_string == "MAT3") {
		return GLTFAccessor::TYPE_MAT3;
	}
	if (p_string == "MAT4") {
		return GLTFAccessor::TYPE_MAT4;
	}

	ERR_FAIL_V(GLTFAccessor::TYPE_SCALAR);
}

Error GLTFDocument::_parse_accessors(Ref<GLTFState> p_state) {
	if (!p_state->json.has("accessors")) {
		return OK;
	}
	const Array &accessors = p_state->json["accessors"];
	for (GLTFAccessorIndex i = 0; i < accessors.size(); i++) {
		const Dictionary &d = accessors[i];

		Ref<GLTFAccessor> accessor;
		accessor.instantiate();

		ERR_FAIL_COND_V(!d.has("componentType"), ERR_PARSE_ERROR);
		accessor->component_type = d["componentType"];
		ERR_FAIL_COND_V(!d.has("count"), ERR_PARSE_ERROR);
		accessor->count = d["count"];
		ERR_FAIL_COND_V(!d.has("type"), ERR_PARSE_ERROR);
		accessor->accessor_type = _get_accessor_type_from_str(d["type"]);

		if (d.has("bufferView")) {
			accessor->buffer_view = d["bufferView"]; //optional because it may be sparse...
		}

		if (d.has("byteOffset")) {
			accessor->byte_offset = d["byteOffset"];
		}

		if (d.has("normalized")) {
			accessor->normalized = d["normalized"];
		}

		if (d.has("max")) {
			accessor->max = d["max"];
		}

		if (d.has("min")) {
			accessor->min = d["min"];
		}

		if (d.has("sparse")) {
			const Dictionary &s = d["sparse"];

			ERR_FAIL_COND_V(!s.has("count"), ERR_PARSE_ERROR);
			accessor->sparse_count = s["count"];
			ERR_FAIL_COND_V(!s.has("indices"), ERR_PARSE_ERROR);
			const Dictionary &si = s["indices"];

			ERR_FAIL_COND_V(!si.has("bufferView"), ERR_PARSE_ERROR);
			accessor->sparse_indices_buffer_view = si["bufferView"];
			ERR_FAIL_COND_V(!si.has("componentType"), ERR_PARSE_ERROR);
			accessor->sparse_indices_component_type = si["componentType"];

			if (si.has("byteOffset")) {
				accessor->sparse_indices_byte_offset = si["byteOffset"];
			}

			ERR_FAIL_COND_V(!s.has("values"), ERR_PARSE_ERROR);
			const Dictionary &sv = s["values"];

			ERR_FAIL_COND_V(!sv.has("bufferView"), ERR_PARSE_ERROR);
			accessor->sparse_values_buffer_view = sv["bufferView"];
			if (sv.has("byteOffset")) {
				accessor->sparse_values_byte_offset = sv["byteOffset"];
			}
		}

		p_state->accessors.push_back(accessor);
	}

	print_verbose("glTF: Total accessors: " + itos(p_state->accessors.size()));

	return OK;
}

double GLTFDocument::_filter_number(double p_float) {
	if (!Math::is_finite(p_float)) {
		// 3.6.2.2. "Values of NaN, +Infinity, and -Infinity MUST NOT be present."
		return 0.0f;
	}
	return (double)(float)p_float;
}

String GLTFDocument::_get_component_type_name(const uint32_t p_component) {
	switch (p_component) {
		case GLTFDocument::COMPONENT_TYPE_BYTE:
			return "Byte";
		case GLTFDocument::COMPONENT_TYPE_UNSIGNED_BYTE:
			return "UByte";
		case GLTFDocument::COMPONENT_TYPE_SHORT:
			return "Short";
		case GLTFDocument::COMPONENT_TYPE_UNSIGNED_SHORT:
			return "UShort";
		case GLTFDocument::COMPONENT_TYPE_INT:
			return "Int";
		case GLTFDocument::COMPONENT_TYPE_FLOAT:
			return "Float";
	}

	return "<Error>";
}

Error GLTFDocument::_encode_buffer_view(Ref<GLTFState> p_state, const double *p_src, const int p_count, const GLTFAccessor::GLTFAccessorType p_accessor_type, const int p_component_type, const bool p_normalized, const int p_byte_offset, const bool p_for_vertex, GLTFBufferViewIndex &r_accessor, const bool p_for_vertex_indices) {
	const int component_count_for_type[7] = {
		1, 2, 3, 4, 4, 9, 16
	};

	const int component_count = component_count_for_type[p_accessor_type];
	const int component_size = _get_component_type_size(p_component_type);
	ERR_FAIL_COND_V(component_size == 0, FAILED);

	int skip_every = 0;
	int skip_bytes = 0;
	//special case of alignments, as described in spec
	switch (p_component_type) {
		case COMPONENT_TYPE_BYTE:
		case COMPONENT_TYPE_UNSIGNED_BYTE: {
			if (p_accessor_type == GLTFAccessor::TYPE_MAT2) {
				skip_every = 2;
				skip_bytes = 2;
			}
			if (p_accessor_type == GLTFAccessor::TYPE_MAT3) {
				skip_every = 3;
				skip_bytes = 1;
			}
		} break;
		case COMPONENT_TYPE_SHORT:
		case COMPONENT_TYPE_UNSIGNED_SHORT: {
			if (p_accessor_type == GLTFAccessor::TYPE_MAT3) {
				skip_every = 6;
				skip_bytes = 4;
			}
		} break;
		default: {
		}
	}

	Ref<GLTFBufferView> bv;
	bv.instantiate();
	const uint32_t offset = bv->byte_offset = p_byte_offset;
	Vector<uint8_t> &gltf_buffer = p_state->buffers.write[0];

	int stride = component_count * component_size;
	if (p_for_vertex && stride % 4) {
		stride += 4 - (stride % 4); //according to spec must be multiple of 4
	}
	//use to debug
	print_verbose("glTF: encoding accessor type " + _get_accessor_type_name(p_accessor_type) + " component type: " + _get_component_type_name(p_component_type) + " stride: " + itos(stride) + " amount " + itos(p_count));

	print_verbose("glTF: encoding accessor offset " + itos(p_byte_offset) + " view offset: " + itos(bv->byte_offset) + " total buffer len: " + itos(gltf_buffer.size()) + " view len " + itos(bv->byte_length));

	const int buffer_end = (stride * (p_count - 1)) + component_size;
	// TODO define bv->byte_stride
	bv->byte_offset = gltf_buffer.size();
	if (p_for_vertex_indices) {
		bv->indices = true;
	} else if (p_for_vertex) {
		bv->vertex_attributes = true;
		bv->byte_stride = stride;
	}

	switch (p_component_type) {
		case COMPONENT_TYPE_BYTE: {
			Vector<int8_t> buffer;
			buffer.resize(p_count * component_count);
			int32_t dst_i = 0;
			for (int i = 0; i < p_count; i++) {
				for (int j = 0; j < component_count; j++) {
					if (skip_every && j > 0 && (j % skip_every) == 0) {
						dst_i += skip_bytes;
					}
					double d = *p_src;
					if (p_normalized) {
						buffer.write[dst_i] = d * 128.0;
					} else {
						buffer.write[dst_i] = d;
					}
					p_src++;
					dst_i++;
				}
			}
			int64_t old_size = gltf_buffer.size();
			gltf_buffer.resize(old_size + (buffer.size() * sizeof(int8_t)));
			memcpy(gltf_buffer.ptrw() + old_size, buffer.ptrw(), buffer.size() * sizeof(int8_t));
			bv->byte_length = buffer.size() * sizeof(int8_t);
		} break;
		case COMPONENT_TYPE_UNSIGNED_BYTE: {
			Vector<uint8_t> buffer;
			buffer.resize(p_count * component_count);
			int32_t dst_i = 0;
			for (int i = 0; i < p_count; i++) {
				for (int j = 0; j < component_count; j++) {
					if (skip_every && j > 0 && (j % skip_every) == 0) {
						dst_i += skip_bytes;
					}
					double d = *p_src;
					if (p_normalized) {
						buffer.write[dst_i] = d * 255.0;
					} else {
						buffer.write[dst_i] = d;
					}
					p_src++;
					dst_i++;
				}
			}
			gltf_buffer.append_array(buffer);
			bv->byte_length = buffer.size() * sizeof(uint8_t);
		} break;
		case COMPONENT_TYPE_SHORT: {
			Vector<int16_t> buffer;
			buffer.resize(p_count * component_count);
			int32_t dst_i = 0;
			for (int i = 0; i < p_count; i++) {
				for (int j = 0; j < component_count; j++) {
					if (skip_every && j > 0 && (j % skip_every) == 0) {
						dst_i += skip_bytes;
					}
					double d = *p_src;
					if (p_normalized) {
						buffer.write[dst_i] = d * 32768.0;
					} else {
						buffer.write[dst_i] = d;
					}
					p_src++;
					dst_i++;
				}
			}
			int64_t old_size = gltf_buffer.size();
			gltf_buffer.resize(old_size + (buffer.size() * sizeof(int16_t)));
			memcpy(gltf_buffer.ptrw() + old_size, buffer.ptrw(), buffer.size() * sizeof(int16_t));
			bv->byte_length = buffer.size() * sizeof(int16_t);
		} break;
		case COMPONENT_TYPE_UNSIGNED_SHORT: {
			Vector<uint16_t> buffer;
			buffer.resize(p_count * component_count);
			int32_t dst_i = 0;
			for (int i = 0; i < p_count; i++) {
				for (int j = 0; j < component_count; j++) {
					if (skip_every && j > 0 && (j % skip_every) == 0) {
						dst_i += skip_bytes;
					}
					double d = *p_src;
					if (p_normalized) {
						buffer.write[dst_i] = d * 65535.0;
					} else {
						buffer.write[dst_i] = d;
					}
					p_src++;
					dst_i++;
				}
			}
			int64_t old_size = gltf_buffer.size();
			gltf_buffer.resize(old_size + (buffer.size() * sizeof(uint16_t)));
			memcpy(gltf_buffer.ptrw() + old_size, buffer.ptrw(), buffer.size() * sizeof(uint16_t));
			bv->byte_length = buffer.size() * sizeof(uint16_t);
		} break;
		case COMPONENT_TYPE_INT: {
			Vector<int> buffer;
			buffer.resize(p_count * component_count);
			int32_t dst_i = 0;
			for (int i = 0; i < p_count; i++) {
				for (int j = 0; j < component_count; j++) {
					if (skip_every && j > 0 && (j % skip_every) == 0) {
						dst_i += skip_bytes;
					}
					double d = *p_src;
					buffer.write[dst_i] = d;
					p_src++;
					dst_i++;
				}
			}
			int64_t old_size = gltf_buffer.size();
			gltf_buffer.resize(old_size + (buffer.size() * sizeof(int32_t)));
			memcpy(gltf_buffer.ptrw() + old_size, buffer.ptrw(), buffer.size() * sizeof(int32_t));
			bv->byte_length = buffer.size() * sizeof(int32_t);
		} break;
		case COMPONENT_TYPE_FLOAT: {
			Vector<float> buffer;
			buffer.resize(p_count * component_count);
			int32_t dst_i = 0;
			for (int i = 0; i < p_count; i++) {
				for (int j = 0; j < component_count; j++) {
					if (skip_every && j > 0 && (j % skip_every) == 0) {
						dst_i += skip_bytes;
					}
					double d = *p_src;
					buffer.write[dst_i] = d;
					p_src++;
					dst_i++;
				}
			}
			int64_t old_size = gltf_buffer.size();
			gltf_buffer.resize(old_size + (buffer.size() * sizeof(float)));
			memcpy(gltf_buffer.ptrw() + old_size, buffer.ptrw(), buffer.size() * sizeof(float));
			bv->byte_length = buffer.size() * sizeof(float);
		} break;
	}
	ERR_FAIL_COND_V(buffer_end > bv->byte_length, ERR_INVALID_DATA);

	ERR_FAIL_COND_V((int)(offset + buffer_end) > gltf_buffer.size(), ERR_INVALID_DATA);
	int pad_bytes = (4 - gltf_buffer.size()) & 3;
	for (int i = 0; i < pad_bytes; i++) {
		gltf_buffer.push_back(0);
	}

	r_accessor = bv->buffer = p_state->buffer_views.size();
	p_state->buffer_views.push_back(bv);
	return OK;
}

Error GLTFDocument::_decode_buffer_view(Ref<GLTFState> p_state, double *p_dst, const GLTFBufferViewIndex p_buffer_view, const int p_skip_every, const int p_skip_bytes, const int p_element_size, const int p_count, const GLTFAccessor::GLTFAccessorType p_accessor_type, const int p_component_count, const int p_component_type, const int p_component_size, const bool p_normalized, const int p_byte_offset, const bool p_for_vertex) {
	const Ref<GLTFBufferView> bv = p_state->buffer_views[p_buffer_view];

	int stride = p_element_size;
	if (bv->byte_stride != -1) {
		stride = bv->byte_stride;
	}
	if (p_for_vertex && stride % 4) {
		stride += 4 - (stride % 4); //according to spec must be multiple of 4
	}

	ERR_FAIL_INDEX_V(bv->buffer, p_state->buffers.size(), ERR_PARSE_ERROR);

	const uint32_t offset = bv->byte_offset + p_byte_offset;
	Vector<uint8_t> buffer = p_state->buffers[bv->buffer]; //copy on write, so no performance hit
	const uint8_t *bufptr = buffer.ptr();

	//use to debug
	print_verbose("glTF: accessor type " + _get_accessor_type_name(p_accessor_type) + " component type: " + _get_component_type_name(p_component_type) + " stride: " + itos(stride) + " amount " + itos(p_count));
	print_verbose("glTF: accessor offset " + itos(p_byte_offset) + " view offset: " + itos(bv->byte_offset) + " total buffer len: " + itos(buffer.size()) + " view len " + itos(bv->byte_length));

	const int buffer_end = (stride * (p_count - 1)) + p_element_size;
	ERR_FAIL_COND_V(buffer_end > bv->byte_length, ERR_PARSE_ERROR);

	ERR_FAIL_COND_V((int)(offset + buffer_end) > buffer.size(), ERR_PARSE_ERROR);

	//fill everything as doubles

	for (int i = 0; i < p_count; i++) {
		const uint8_t *src = &bufptr[offset + i * stride];

		for (int j = 0; j < p_component_count; j++) {
			if (p_skip_every && j > 0 && (j % p_skip_every) == 0) {
				src += p_skip_bytes;
			}

			double d = 0;

			switch (p_component_type) {
				case COMPONENT_TYPE_BYTE: {
					int8_t b = int8_t(*src);
					if (p_normalized) {
						d = (double(b) / 128.0);
					} else {
						d = double(b);
					}
				} break;
				case COMPONENT_TYPE_UNSIGNED_BYTE: {
					uint8_t b = *src;
					if (p_normalized) {
						d = (double(b) / 255.0);
					} else {
						d = double(b);
					}
				} break;
				case COMPONENT_TYPE_SHORT: {
					int16_t s = *(int16_t *)src;
					if (p_normalized) {
						d = (double(s) / 32768.0);
					} else {
						d = double(s);
					}
				} break;
				case COMPONENT_TYPE_UNSIGNED_SHORT: {
					uint16_t s = *(uint16_t *)src;
					if (p_normalized) {
						d = (double(s) / 65535.0);
					} else {
						d = double(s);
					}
				} break;
				case COMPONENT_TYPE_INT: {
					d = *(int *)src;
				} break;
				case COMPONENT_TYPE_FLOAT: {
					d = *(float *)src;
				} break;
			}

			*p_dst++ = d;
			src += p_component_size;
		}
	}

	return OK;
}

int GLTFDocument::_get_component_type_size(const int p_component_type) {
	switch (p_component_type) {
		case COMPONENT_TYPE_BYTE:
		case COMPONENT_TYPE_UNSIGNED_BYTE:
			return 1;
			break;
		case COMPONENT_TYPE_SHORT:
		case COMPONENT_TYPE_UNSIGNED_SHORT:
			return 2;
			break;
		case COMPONENT_TYPE_INT:
		case COMPONENT_TYPE_FLOAT:
			return 4;
			break;
		default: {
			ERR_FAIL_V(0);
		}
	}
	return 0;
}

Vector<double> GLTFDocument::_decode_accessor(Ref<GLTFState> p_state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {
	//spec, for reference:
	//https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#data-alignment

	ERR_FAIL_INDEX_V(p_accessor, p_state->accessors.size(), Vector<double>());

	const Ref<GLTFAccessor> a = p_state->accessors[p_accessor];

	const int component_count_for_type[7] = {
		1, 2, 3, 4, 4, 9, 16
	};

	const int component_count = component_count_for_type[a->accessor_type];
	const int component_size = _get_component_type_size(a->component_type);
	ERR_FAIL_COND_V(component_size == 0, Vector<double>());
	int element_size = component_count * component_size;

	int skip_every = 0;
	int skip_bytes = 0;
	//special case of alignments, as described in spec
	switch (a->component_type) {
		case COMPONENT_TYPE_BYTE:
		case COMPONENT_TYPE_UNSIGNED_BYTE: {
			if (a->accessor_type == GLTFAccessor::TYPE_MAT2) {
				skip_every = 2;
				skip_bytes = 2;
				element_size = 8; //override for this case
			}
			if (a->accessor_type == GLTFAccessor::TYPE_MAT3) {
				skip_every = 3;
				skip_bytes = 1;
				element_size = 12; //override for this case
			}
		} break;
		case COMPONENT_TYPE_SHORT:
		case COMPONENT_TYPE_UNSIGNED_SHORT: {
			if (a->accessor_type == GLTFAccessor::TYPE_MAT3) {
				skip_every = 6;
				skip_bytes = 4;
				element_size = 16; //override for this case
			}
		} break;
		default: {
		}
	}

	Vector<double> dst_buffer;
	dst_buffer.resize(component_count * a->count);
	double *dst = dst_buffer.ptrw();

	if (a->buffer_view >= 0) {
		ERR_FAIL_INDEX_V(a->buffer_view, p_state->buffer_views.size(), Vector<double>());

		const Error err = _decode_buffer_view(p_state, dst, a->buffer_view, skip_every, skip_bytes, element_size, a->count, a->accessor_type, component_count, a->component_type, component_size, a->normalized, a->byte_offset, p_for_vertex);
		if (err != OK) {
			return Vector<double>();
		}
	} else {
		//fill with zeros, as bufferview is not defined.
		for (int i = 0; i < (a->count * component_count); i++) {
			dst_buffer.write[i] = 0;
		}
	}

	if (a->sparse_count > 0) {
		// I could not find any file using this, so this code is so far untested
		Vector<double> indices;
		indices.resize(a->sparse_count);
		const int indices_component_size = _get_component_type_size(a->sparse_indices_component_type);

		Error err = _decode_buffer_view(p_state, indices.ptrw(), a->sparse_indices_buffer_view, 0, 0, indices_component_size, a->sparse_count, GLTFAccessor::TYPE_SCALAR, 1, a->sparse_indices_component_type, indices_component_size, false, a->sparse_indices_byte_offset, false);
		if (err != OK) {
			return Vector<double>();
		}

		Vector<double> data;
		data.resize(component_count * a->sparse_count);
		err = _decode_buffer_view(p_state, data.ptrw(), a->sparse_values_buffer_view, skip_every, skip_bytes, element_size, a->sparse_count, a->accessor_type, component_count, a->component_type, component_size, a->normalized, a->sparse_values_byte_offset, p_for_vertex);
		if (err != OK) {
			return Vector<double>();
		}

		for (int i = 0; i < indices.size(); i++) {
			const int write_offset = int(indices[i]) * component_count;

			for (int j = 0; j < component_count; j++) {
				dst[write_offset + j] = data[i * component_count + j];
			}
		}
	}

	return dst_buffer;
}

GLTFAccessorIndex GLTFDocument::_encode_accessor_as_ints(Ref<GLTFState> p_state, const Vector<int32_t> p_attribs, const bool p_for_vertex, const bool p_for_vertex_indices) {
	if (p_attribs.size() == 0) {
		return -1;
	}
	const int element_count = 1;
	const int ret_size = p_attribs.size();
	Vector<double> attribs;
	attribs.resize(ret_size);
	Vector<double> type_max;
	type_max.resize(element_count);
	Vector<double> type_min;
	type_min.resize(element_count);
	int max_index = 0;
	for (int i = 0; i < p_attribs.size(); i++) {
		attribs.write[i] = p_attribs[i];
		if (p_attribs[i] > max_index) {
			max_index = p_attribs[i];
		}
		if (i == 0) {
			for (int32_t type_i = 0; type_i < element_count; type_i++) {
				type_max.write[type_i] = attribs[(i * element_count) + type_i];
				type_min.write[type_i] = attribs[(i * element_count) + type_i];
			}
		}
		for (int32_t type_i = 0; type_i < element_count; type_i++) {
			type_max.write[type_i] = MAX(attribs[(i * element_count) + type_i], type_max[type_i]);
			type_min.write[type_i] = MIN(attribs[(i * element_count) + type_i], type_min[type_i]);
		}
	}
	ERR_FAIL_COND_V(attribs.is_empty(), -1);

	Ref<GLTFAccessor> accessor;
	accessor.instantiate();
	GLTFBufferIndex buffer_view_i;
	if (p_state->buffers.is_empty()) {
		p_state->buffers.push_back(Vector<uint8_t>());
	}
	int64_t size = p_state->buffers[0].size();
	const GLTFAccessor::GLTFAccessorType accessor_type = GLTFAccessor::TYPE_SCALAR;
	int component_type;
	if (max_index > 65535 || p_for_vertex) {
		component_type = GLTFDocument::COMPONENT_TYPE_INT;
	} else {
		component_type = GLTFDocument::COMPONENT_TYPE_UNSIGNED_SHORT;
	}

	accessor->max = type_max;
	accessor->min = type_min;
	accessor->normalized = false;
	accessor->count = ret_size;
	accessor->accessor_type = accessor_type;
	accessor->component_type = component_type;
	accessor->byte_offset = 0;
	Error err = _encode_buffer_view(p_state, attribs.ptr(), attribs.size(), accessor_type, component_type, accessor->normalized, size, p_for_vertex, buffer_view_i, p_for_vertex_indices);
	if (err != OK) {
		return -1;
	}
	accessor->buffer_view = buffer_view_i;
	p_state->accessors.push_back(accessor);
	return p_state->accessors.size() - 1;
}

Vector<int> GLTFDocument::_decode_accessor_as_ints(Ref<GLTFState> p_state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex, const Vector<int> &p_packed_vertex_ids) {
	const Vector<double> attribs = _decode_accessor(p_state, p_accessor, p_for_vertex);
	Vector<int> ret;

	if (attribs.size() == 0) {
		return ret;
	}

	const double *attribs_ptr = attribs.ptr();
	int ret_size = attribs.size();
	if (!p_packed_vertex_ids.is_empty()) {
		ERR_FAIL_COND_V(p_packed_vertex_ids[p_packed_vertex_ids.size() - 1] >= ret_size, ret);
		ret_size = p_packed_vertex_ids.size();
	}
	ret.resize(ret_size);
	for (int i = 0; i < ret_size; i++) {
		int src_i = i;
		if (!p_packed_vertex_ids.is_empty()) {
			src_i = p_packed_vertex_ids[i];
		}
		ret.write[i] = int(attribs_ptr[src_i]);
	}
	return ret;
}

Vector<float> GLTFDocument::_decode_accessor_as_floats(Ref<GLTFState> p_state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex, const Vector<int> &p_packed_vertex_ids) {
	const Vector<double> attribs = _decode_accessor(p_state, p_accessor, p_for_vertex);
	Vector<float> ret;

	if (attribs.size() == 0) {
		return ret;
	}

	const double *attribs_ptr = attribs.ptr();
	int ret_size = attribs.size();
	if (!p_packed_vertex_ids.is_empty()) {
		ERR_FAIL_COND_V(p_packed_vertex_ids[p_packed_vertex_ids.size() - 1] >= ret_size, ret);
		ret_size = p_packed_vertex_ids.size();
	}
	ret.resize(ret_size);
	for (int i = 0; i < ret_size; i++) {
		int src_i = i;
		if (!p_packed_vertex_ids.is_empty()) {
			src_i = p_packed_vertex_ids[i];
		}
		ret.write[i] = float(attribs_ptr[src_i]);
	}
	return ret;
}

void GLTFDocument::_round_min_max_components(Vector<double> &r_type_min, Vector<double> &r_type_max) {
	// 3.6.2.5: For floating-point components, JSON-stored minimum and maximum values represent single precision
	// floats and SHOULD be rounded to single precision before usage to avoid any potential boundary mismatches.
	for (int32_t type_i = 0; type_i < r_type_min.size(); type_i++) {
		r_type_min.write[type_i] = (double)(float)r_type_min[type_i];
		r_type_max.write[type_i] = (double)(float)r_type_max[type_i];
	}
}

GLTFAccessorIndex GLTFDocument::_encode_accessor_as_vec2(Ref<GLTFState> p_state, const Vector<Vector2> p_attribs, const bool p_for_vertex) {
	if (p_attribs.size() == 0) {
		return -1;
	}
	const int element_count = 2;

	const int ret_size = p_attribs.size() * element_count;
	Vector<double> attribs;
	attribs.resize(ret_size);
	Vector<double> type_max;
	type_max.resize(element_count);
	Vector<double> type_min;
	type_min.resize(element_count);

	for (int i = 0; i < p_attribs.size(); i++) {
		Vector2 attrib = p_attribs[i];
		attribs.write[(i * element_count) + 0] = _filter_number(attrib.x);
		attribs.write[(i * element_count) + 1] = _filter_number(attrib.y);
		_calc_accessor_min_max(i, element_count, type_max, attribs, type_min);
	}
	_round_min_max_components(type_min, type_max);

	ERR_FAIL_COND_V(attribs.size() % element_count != 0, -1);

	Ref<GLTFAccessor> accessor;
	accessor.instantiate();
	GLTFBufferIndex buffer_view_i;
	if (p_state->buffers.is_empty()) {
		p_state->buffers.push_back(Vector<uint8_t>());
	}
	int64_t size = p_state->buffers[0].size();
	const GLTFAccessor::GLTFAccessorType accessor_type = GLTFAccessor::TYPE_VEC2;
	const int component_type = GLTFDocument::COMPONENT_TYPE_FLOAT;

	accessor->max = type_max;
	accessor->min = type_min;
	accessor->normalized = false;
	accessor->count = p_attribs.size();
	accessor->accessor_type = accessor_type;
	accessor->component_type = component_type;
	accessor->byte_offset = 0;
	Error err = _encode_buffer_view(p_state, attribs.ptr(), p_attribs.size(), accessor_type, component_type, accessor->normalized, size, p_for_vertex, buffer_view_i);
	if (err != OK) {
		return -1;
	}
	accessor->buffer_view = buffer_view_i;
	p_state->accessors.push_back(accessor);
	return p_state->accessors.size() - 1;
}

GLTFAccessorIndex GLTFDocument::_encode_accessor_as_color(Ref<GLTFState> p_state, const Vector<Color> p_attribs, const bool p_for_vertex) {
	if (p_attribs.size() == 0) {
		return -1;
	}

	const int ret_size = p_attribs.size() * 4;
	Vector<double> attribs;
	attribs.resize(ret_size);

	const int element_count = 4;
	Vector<double> type_max;
	type_max.resize(element_count);
	Vector<double> type_min;
	type_min.resize(element_count);
	for (int i = 0; i < p_attribs.size(); i++) {
		Color attrib = p_attribs[i];
		attribs.write[(i * element_count) + 0] = _filter_number(attrib.r);
		attribs.write[(i * element_count) + 1] = _filter_number(attrib.g);
		attribs.write[(i * element_count) + 2] = _filter_number(attrib.b);
		attribs.write[(i * element_count) + 3] = _filter_number(attrib.a);

		_calc_accessor_min_max(i, element_count, type_max, attribs, type_min);
	}
	_round_min_max_components(type_min, type_max);

	ERR_FAIL_COND_V(attribs.size() % element_count != 0, -1);

	Ref<GLTFAccessor> accessor;
	accessor.instantiate();
	GLTFBufferIndex buffer_view_i;
	if (p_state->buffers.is_empty()) {
		p_state->buffers.push_back(Vector<uint8_t>());
	}
	int64_t size = p_state->buffers[0].size();
	const GLTFAccessor::GLTFAccessorType accessor_type = GLTFAccessor::TYPE_VEC4;
	const int component_type = GLTFDocument::COMPONENT_TYPE_FLOAT;

	accessor->max = type_max;
	accessor->min = type_min;
	accessor->normalized = false;
	accessor->count = p_attribs.size();
	accessor->accessor_type = accessor_type;
	accessor->component_type = component_type;
	accessor->byte_offset = 0;
	Error err = _encode_buffer_view(p_state, attribs.ptr(), p_attribs.size(), accessor_type, component_type, accessor->normalized, size, p_for_vertex, buffer_view_i);
	if (err != OK) {
		return -1;
	}
	accessor->buffer_view = buffer_view_i;
	p_state->accessors.push_back(accessor);
	return p_state->accessors.size() - 1;
}

void GLTFDocument::_calc_accessor_min_max(int p_i, const int p_element_count, Vector<double> &p_type_max, Vector<double> p_attribs, Vector<double> &p_type_min) {
	if (p_i == 0) {
		for (int32_t type_i = 0; type_i < p_element_count; type_i++) {
			p_type_max.write[type_i] = p_attribs[(p_i * p_element_count) + type_i];
			p_type_min.write[type_i] = p_attribs[(p_i * p_element_count) + type_i];
		}
	}
	for (int32_t type_i = 0; type_i < p_element_count; type_i++) {
		p_type_max.write[type_i] = MAX(p_attribs[(p_i * p_element_count) + type_i], p_type_max[type_i]);
		p_type_min.write[type_i] = MIN(p_attribs[(p_i * p_element_count) + type_i], p_type_min[type_i]);
	}
}

GLTFAccessorIndex GLTFDocument::_encode_accessor_as_weights(Ref<GLTFState> p_state, const Vector<Color> p_attribs, const bool p_for_vertex) {
	if (p_attribs.size() == 0) {
		return -1;
	}

	const int ret_size = p_attribs.size() * 4;
	Vector<double> attribs;
	attribs.resize(ret_size);

	const int element_count = 4;

	Vector<double> type_max;
	type_max.resize(element_count);
	Vector<double> type_min;
	type_min.resize(element_count);
	for (int i = 0; i < p_attribs.size(); i++) {
		Color attrib = p_attribs[i];
		attribs.write[(i * element_count) + 0] = _filter_number(attrib.r);
		attribs.write[(i * element_count) + 1] = _filter_number(attrib.g);
		attribs.write[(i * element_count) + 2] = _filter_number(attrib.b);
		attribs.write[(i * element_count) + 3] = _filter_number(attrib.a);

		_calc_accessor_min_max(i, element_count, type_max, attribs, type_min);
	}
	_round_min_max_components(type_min, type_max);

	ERR_FAIL_COND_V(attribs.size() % element_count != 0, -1);

	Ref<GLTFAccessor> accessor;
	accessor.instantiate();
	GLTFBufferIndex buffer_view_i;
	if (p_state->buffers.is_empty()) {
		p_state->buffers.push_back(Vector<uint8_t>());
	}
	int64_t size = p_state->buffers[0].size();
	const GLTFAccessor::GLTFAccessorType accessor_type = GLTFAccessor::TYPE_VEC4;
	const int component_type = GLTFDocument::COMPONENT_TYPE_FLOAT;

	accessor->max = type_max;
	accessor->min = type_min;
	accessor->normalized = false;
	accessor->count = p_attribs.size();
	accessor->accessor_type = accessor_type;
	accessor->component_type = component_type;
	accessor->byte_offset = 0;
	Error err = _encode_buffer_view(p_state, attribs.ptr(), p_attribs.size(), accessor_type, component_type, accessor->normalized, size, p_for_vertex, buffer_view_i);
	if (err != OK) {
		return -1;
	}
	accessor->buffer_view = buffer_view_i;
	p_state->accessors.push_back(accessor);
	return p_state->accessors.size() - 1;
}

GLTFAccessorIndex GLTFDocument::_encode_accessor_as_joints(Ref<GLTFState> p_state, const Vector<Color> p_attribs, const bool p_for_vertex) {
	if (p_attribs.size() == 0) {
		return -1;
	}

	const int element_count = 4;
	const int ret_size = p_attribs.size() * element_count;
	Vector<double> attribs;
	attribs.resize(ret_size);

	Vector<double> type_max;
	type_max.resize(element_count);
	Vector<double> type_min;
	type_min.resize(element_count);
	for (int i = 0; i < p_attribs.size(); i++) {
		Color attrib = p_attribs[i];
		attribs.write[(i * element_count) + 0] = _filter_number(attrib.r);
		attribs.write[(i * element_count) + 1] = _filter_number(attrib.g);
		attribs.write[(i * element_count) + 2] = _filter_number(attrib.b);
		attribs.write[(i * element_count) + 3] = _filter_number(attrib.a);
		_calc_accessor_min_max(i, element_count, type_max, attribs, type_min);
	}
	_round_min_max_components(type_min, type_max);
	ERR_FAIL_COND_V(attribs.size() % element_count != 0, -1);

	Ref<GLTFAccessor> accessor;
	accessor.instantiate();
	GLTFBufferIndex buffer_view_i;
	if (p_state->buffers.is_empty()) {
		p_state->buffers.push_back(Vector<uint8_t>());
	}
	int64_t size = p_state->buffers[0].size();
	const GLTFAccessor::GLTFAccessorType accessor_type = GLTFAccessor::TYPE_VEC4;
	const int component_type = GLTFDocument::COMPONENT_TYPE_UNSIGNED_SHORT;

	accessor->max = type_max;
	accessor->min = type_min;
	accessor->normalized = false;
	accessor->count = p_attribs.size();
	accessor->accessor_type = accessor_type;
	accessor->component_type = component_type;
	accessor->byte_offset = 0;
	Error err = _encode_buffer_view(p_state, attribs.ptr(), p_attribs.size(), accessor_type, component_type, accessor->normalized, size, p_for_vertex, buffer_view_i);
	if (err != OK) {
		return -1;
	}
	accessor->buffer_view = buffer_view_i;
	p_state->accessors.push_back(accessor);
	return p_state->accessors.size() - 1;
}

GLTFAccessorIndex GLTFDocument::_encode_accessor_as_quaternions(Ref<GLTFState> p_state, const Vector<Quaternion> p_attribs, const bool p_for_vertex) {
	if (p_attribs.size() == 0) {
		return -1;
	}
	const int element_count = 4;

	const int ret_size = p_attribs.size() * element_count;
	Vector<double> attribs;
	attribs.resize(ret_size);

	Vector<double> type_max;
	type_max.resize(element_count);
	Vector<double> type_min;
	type_min.resize(element_count);
	for (int i = 0; i < p_attribs.size(); i++) {
		Quaternion quaternion = p_attribs[i];
		attribs.write[(i * element_count) + 0] = _filter_number(quaternion.x);
		attribs.write[(i * element_count) + 1] = _filter_number(quaternion.y);
		attribs.write[(i * element_count) + 2] = _filter_number(quaternion.z);
		attribs.write[(i * element_count) + 3] = _filter_number(quaternion.w);

		_calc_accessor_min_max(i, element_count, type_max, attribs, type_min);
	}
	_round_min_max_components(type_min, type_max);

	ERR_FAIL_COND_V(attribs.size() % element_count != 0, -1);

	Ref<GLTFAccessor> accessor;
	accessor.instantiate();
	GLTFBufferIndex buffer_view_i;
	if (p_state->buffers.is_empty()) {
		p_state->buffers.push_back(Vector<uint8_t>());
	}
	int64_t size = p_state->buffers[0].size();
	const GLTFAccessor::GLTFAccessorType accessor_type = GLTFAccessor::TYPE_VEC4;
	const int component_type = GLTFDocument::COMPONENT_TYPE_FLOAT;

	accessor->max = type_max;
	accessor->min = type_min;
	accessor->normalized = false;
	accessor->count = p_attribs.size();
	accessor->accessor_type = accessor_type;
	accessor->component_type = component_type;
	accessor->byte_offset = 0;
	Error err = _encode_buffer_view(p_state, attribs.ptr(), p_attribs.size(), accessor_type, component_type, accessor->normalized, size, p_for_vertex, buffer_view_i);
	if (err != OK) {
		return -1;
	}
	accessor->buffer_view = buffer_view_i;
	p_state->accessors.push_back(accessor);
	return p_state->accessors.size() - 1;
}

Vector<Vector2> GLTFDocument::_decode_accessor_as_vec2(Ref<GLTFState> p_state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex, const Vector<int> &p_packed_vertex_ids) {
	const Vector<double> attribs = _decode_accessor(p_state, p_accessor, p_for_vertex);
	Vector<Vector2> ret;

	if (attribs.size() == 0) {
		return ret;
	}

	ERR_FAIL_COND_V(attribs.size() % 2 != 0, ret);
	const double *attribs_ptr = attribs.ptr();
	int ret_size = attribs.size() / 2;
	if (!p_packed_vertex_ids.is_empty()) {
		ERR_FAIL_COND_V(p_packed_vertex_ids[p_packed_vertex_ids.size() - 1] >= ret_size, ret);
		ret_size = p_packed_vertex_ids.size();
	}
	ret.resize(ret_size);
	for (int i = 0; i < ret_size; i++) {
		int src_i = i;
		if (!p_packed_vertex_ids.is_empty()) {
			src_i = p_packed_vertex_ids[i];
		}
		ret.write[i] = Vector2(attribs_ptr[src_i * 2 + 0], attribs_ptr[src_i * 2 + 1]);
	}
	return ret;
}

GLTFAccessorIndex GLTFDocument::_encode_accessor_as_floats(Ref<GLTFState> p_state, const Vector<real_t> p_attribs, const bool p_for_vertex) {
	if (p_attribs.size() == 0) {
		return -1;
	}
	const int element_count = 1;
	const int ret_size = p_attribs.size();
	Vector<double> attribs;
	attribs.resize(ret_size);

	Vector<double> type_max;
	type_max.resize(element_count);
	Vector<double> type_min;
	type_min.resize(element_count);

	for (int i = 0; i < p_attribs.size(); i++) {
		attribs.write[i] = _filter_number(p_attribs[i]);

		_calc_accessor_min_max(i, element_count, type_max, attribs, type_min);
	}
	_round_min_max_components(type_min, type_max);

	ERR_FAIL_COND_V(attribs.is_empty(), -1);

	Ref<GLTFAccessor> accessor;
	accessor.instantiate();
	GLTFBufferIndex buffer_view_i;
	if (p_state->buffers.is_empty()) {
		p_state->buffers.push_back(Vector<uint8_t>());
	}
	int64_t size = p_state->buffers[0].size();
	const GLTFAccessor::GLTFAccessorType accessor_type = GLTFAccessor::TYPE_SCALAR;
	const int component_type = GLTFDocument::COMPONENT_TYPE_FLOAT;

	accessor->max = type_max;
	accessor->min = type_min;
	accessor->normalized = false;
	accessor->count = ret_size;
	accessor->accessor_type = accessor_type;
	accessor->component_type = component_type;
	accessor->byte_offset = 0;
	Error err = _encode_buffer_view(p_state, attribs.ptr(), attribs.size(), accessor_type, component_type, accessor->normalized, size, p_for_vertex, buffer_view_i);
	if (err != OK) {
		return -1;
	}
	accessor->buffer_view = buffer_view_i;
	p_state->accessors.push_back(accessor);
	return p_state->accessors.size() - 1;
}

GLTFAccessorIndex GLTFDocument::_encode_accessor_as_vec3(Ref<GLTFState> p_state, const Vector<Vector3> p_attribs, const bool p_for_vertex) {
	if (p_attribs.size() == 0) {
		return -1;
	}
	const int element_count = 3;
	const int ret_size = p_attribs.size() * element_count;
	Vector<double> attribs;
	attribs.resize(ret_size);

	Vector<double> type_max;
	type_max.resize(element_count);
	Vector<double> type_min;
	type_min.resize(element_count);
	for (int i = 0; i < p_attribs.size(); i++) {
		Vector3 attrib = p_attribs[i];
		attribs.write[(i * element_count) + 0] = _filter_number(attrib.x);
		attribs.write[(i * element_count) + 1] = _filter_number(attrib.y);
		attribs.write[(i * element_count) + 2] = _filter_number(attrib.z);

		_calc_accessor_min_max(i, element_count, type_max, attribs, type_min);
	}
	_round_min_max_components(type_min, type_max);
	ERR_FAIL_COND_V(attribs.size() % element_count != 0, -1);

	Ref<GLTFAccessor> accessor;
	accessor.instantiate();
	GLTFBufferIndex buffer_view_i;
	if (p_state->buffers.is_empty()) {
		p_state->buffers.push_back(Vector<uint8_t>());
	}
	int64_t size = p_state->buffers[0].size();
	const GLTFAccessor::GLTFAccessorType accessor_type = GLTFAccessor::TYPE_VEC3;
	const int component_type = GLTFDocument::COMPONENT_TYPE_FLOAT;

	accessor->max = type_max;
	accessor->min = type_min;
	accessor->normalized = false;
	accessor->count = p_attribs.size();
	accessor->accessor_type = accessor_type;
	accessor->component_type = component_type;
	accessor->byte_offset = 0;
	Error err = _encode_buffer_view(p_state, attribs.ptr(), p_attribs.size(), accessor_type, component_type, accessor->normalized, size, p_for_vertex, buffer_view_i);
	if (err != OK) {
		return -1;
	}
	accessor->buffer_view = buffer_view_i;
	p_state->accessors.push_back(accessor);
	return p_state->accessors.size() - 1;
}

GLTFAccessorIndex GLTFDocument::_encode_sparse_accessor_as_vec3(Ref<GLTFState> p_state, const Vector<Vector3> p_attribs, const Vector<Vector3> p_reference_attribs, const float p_reference_multiplier, const bool p_for_vertex, const GLTFAccessorIndex p_reference_accessor) {
	if (p_attribs.size() == 0) {
		return -1;
	}

	const int element_count = 3;
	Vector<double> attribs;
	Vector<double> type_max;
	Vector<double> type_min;
	attribs.resize(p_attribs.size() * element_count);
	type_max.resize(element_count);
	type_min.resize(element_count);

	Vector<double> changed_indices;
	Vector<double> changed_values;
	int max_changed_index = 0;

	for (int i = 0; i < p_attribs.size(); i++) {
		Vector3 attrib = p_attribs[i];
		bool is_different = false;
		if (i < p_reference_attribs.size()) {
			is_different = !(attrib * p_reference_multiplier).is_equal_approx(p_reference_attribs[i]);
			if (!is_different) {
				attrib = p_reference_attribs[i];
			}
		} else {
			is_different = !(attrib * p_reference_multiplier).is_zero_approx();
			if (!is_different) {
				attrib = Vector3();
			}
		}
		attribs.write[(i * element_count) + 0] = _filter_number(attrib.x);
		attribs.write[(i * element_count) + 1] = _filter_number(attrib.y);
		attribs.write[(i * element_count) + 2] = _filter_number(attrib.z);
		if (is_different) {
			changed_indices.push_back(i);
			if (i > max_changed_index) {
				max_changed_index = i;
			}
			changed_values.push_back(_filter_number(attrib.x));
			changed_values.push_back(_filter_number(attrib.y));
			changed_values.push_back(_filter_number(attrib.z));
		}
		_calc_accessor_min_max(i, element_count, type_max, attribs, type_min);
	}
	_round_min_max_components(type_min, type_max);

	if (attribs.size() % element_count != 0) {
		return -1;
	}

	Ref<GLTFAccessor> sparse_accessor;
	sparse_accessor.instantiate();
	if (p_state->buffers.is_empty()) {
		p_state->buffers.push_back(Vector<uint8_t>());
	}
	int64_t size = p_state->buffers[0].size();
	const GLTFAccessor::GLTFAccessorType accessor_type = GLTFAccessor::TYPE_VEC3;
	const int component_type = GLTFDocument::COMPONENT_TYPE_FLOAT;

	sparse_accessor->normalized = false;
	sparse_accessor->count = p_attribs.size();
	sparse_accessor->accessor_type = accessor_type;
	sparse_accessor->component_type = component_type;
	if (p_reference_accessor < p_state->accessors.size() && p_reference_accessor >= 0 && p_state->accessors[p_reference_accessor].is_valid()) {
		sparse_accessor->byte_offset = p_state->accessors[p_reference_accessor]->byte_offset;
		sparse_accessor->buffer_view = p_state->accessors[p_reference_accessor]->buffer_view;
	}
	sparse_accessor->max = type_max;
	sparse_accessor->min = type_min;
	int sparse_accessor_index_stride = max_changed_index > 65535 ? 4 : 2;

	int sparse_accessor_storage_size = changed_indices.size() * (sparse_accessor_index_stride + element_count * sizeof(float));
	int conventional_storage_size = p_attribs.size() * element_count * sizeof(float);

	if (changed_indices.size() > 0 && sparse_accessor_storage_size < conventional_storage_size) {
		// It must be worthwhile to use a sparse accessor.

		GLTFBufferIndex buffer_view_i_indices = -1;
		GLTFBufferIndex buffer_view_i_values = -1;
		if (sparse_accessor_index_stride == 4) {
			sparse_accessor->sparse_indices_component_type = GLTFDocument::COMPONENT_TYPE_INT;
		} else {
			sparse_accessor->sparse_indices_component_type = GLTFDocument::COMPONENT_TYPE_UNSIGNED_SHORT;
		}
		if (_encode_buffer_view(p_state, changed_indices.ptr(), changed_indices.size(), GLTFAccessor::TYPE_SCALAR, sparse_accessor->sparse_indices_component_type, sparse_accessor->normalized, sparse_accessor->sparse_indices_byte_offset, false, buffer_view_i_indices) != OK) {
			return -1;
		}
		// We use changed_indices.size() here, because we must pass the number of vec3 values rather than the number of components.
		if (_encode_buffer_view(p_state, changed_values.ptr(), changed_indices.size(), sparse_accessor->accessor_type, sparse_accessor->component_type, sparse_accessor->normalized, sparse_accessor->sparse_values_byte_offset, false, buffer_view_i_values) != OK) {
			return -1;
		}
		sparse_accessor->sparse_indices_buffer_view = buffer_view_i_indices;
		sparse_accessor->sparse_values_buffer_view = buffer_view_i_values;
		sparse_accessor->sparse_count = changed_indices.size();
	} else if (changed_indices.size() > 0) {
		GLTFBufferIndex buffer_view_i;
		sparse_accessor->byte_offset = 0;
		Error err = _encode_buffer_view(p_state, attribs.ptr(), p_attribs.size(), accessor_type, component_type, sparse_accessor->normalized, size, p_for_vertex, buffer_view_i);
		if (err != OK) {
			return -1;
		}
		sparse_accessor->buffer_view = buffer_view_i;
	}
	p_state->accessors.push_back(sparse_accessor);

	return p_state->accessors.size() - 1;
}

GLTFAccessorIndex GLTFDocument::_encode_accessor_as_xform(Ref<GLTFState> p_state, const Vector<Transform3D> p_attribs, const bool p_for_vertex) {
	if (p_attribs.size() == 0) {
		return -1;
	}
	const int element_count = 16;
	const int ret_size = p_attribs.size() * element_count;
	Vector<double> attribs;
	attribs.resize(ret_size);

	Vector<double> type_max;
	type_max.resize(element_count);
	Vector<double> type_min;
	type_min.resize(element_count);
	for (int i = 0; i < p_attribs.size(); i++) {
		Transform3D attrib = p_attribs[i];
		Basis basis = attrib.get_basis();
		Vector3 axis_0 = basis.get_column(Vector3::AXIS_X);

		attribs.write[i * element_count + 0] = _filter_number(axis_0.x);
		attribs.write[i * element_count + 1] = _filter_number(axis_0.y);
		attribs.write[i * element_count + 2] = _filter_number(axis_0.z);
		attribs.write[i * element_count + 3] = 0.0;

		Vector3 axis_1 = basis.get_column(Vector3::AXIS_Y);
		attribs.write[i * element_count + 4] = _filter_number(axis_1.x);
		attribs.write[i * element_count + 5] = _filter_number(axis_1.y);
		attribs.write[i * element_count + 6] = _filter_number(axis_1.z);
		attribs.write[i * element_count + 7] = 0.0;

		Vector3 axis_2 = basis.get_column(Vector3::AXIS_Z);
		attribs.write[i * element_count + 8] = _filter_number(axis_2.x);
		attribs.write[i * element_count + 9] = _filter_number(axis_2.y);
		attribs.write[i * element_count + 10] = _filter_number(axis_2.z);
		attribs.write[i * element_count + 11] = 0.0;

		Vector3 origin = attrib.get_origin();
		attribs.write[i * element_count + 12] = _filter_number(origin.x);
		attribs.write[i * element_count + 13] = _filter_number(origin.y);
		attribs.write[i * element_count + 14] = _filter_number(origin.z);
		attribs.write[i * element_count + 15] = 1.0;

		_calc_accessor_min_max(i, element_count, type_max, attribs, type_min);
	}
	_round_min_max_components(type_min, type_max);
	ERR_FAIL_COND_V(attribs.size() % element_count != 0, -1);

	Ref<GLTFAccessor> accessor;
	accessor.instantiate();
	GLTFBufferIndex buffer_view_i;
	if (p_state->buffers.is_empty()) {
		p_state->buffers.push_back(Vector<uint8_t>());
	}
	int64_t size = p_state->buffers[0].size();
	const GLTFAccessor::GLTFAccessorType accessor_type = GLTFAccessor::TYPE_MAT4;
	const int component_type = GLTFDocument::COMPONENT_TYPE_FLOAT;

	accessor->max = type_max;
	accessor->min = type_min;
	accessor->normalized = false;
	accessor->count = p_attribs.size();
	accessor->accessor_type = accessor_type;
	accessor->component_type = component_type;
	accessor->byte_offset = 0;
	Error err = _encode_buffer_view(p_state, attribs.ptr(), p_attribs.size(), accessor_type, component_type, accessor->normalized, size, p_for_vertex, buffer_view_i);
	if (err != OK) {
		return -1;
	}
	accessor->buffer_view = buffer_view_i;
	p_state->accessors.push_back(accessor);
	return p_state->accessors.size() - 1;
}

Vector<Vector3> GLTFDocument::_decode_accessor_as_vec3(Ref<GLTFState> p_state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex, const Vector<int> &p_packed_vertex_ids) {
	const Vector<double> attribs = _decode_accessor(p_state, p_accessor, p_for_vertex);
	Vector<Vector3> ret;

	if (attribs.size() == 0) {
		return ret;
	}

	ERR_FAIL_COND_V(attribs.size() % 3 != 0, ret);
	const double *attribs_ptr = attribs.ptr();
	int ret_size = attribs.size() / 3;
	if (!p_packed_vertex_ids.is_empty()) {
		ERR_FAIL_COND_V(p_packed_vertex_ids[p_packed_vertex_ids.size() - 1] >= ret_size, ret);
		ret_size = p_packed_vertex_ids.size();
	}
	ret.resize(ret_size);
	for (int i = 0; i < ret_size; i++) {
		int src_i = i;
		if (!p_packed_vertex_ids.is_empty()) {
			src_i = p_packed_vertex_ids[i];
		}
		ret.write[i] = Vector3(attribs_ptr[src_i * 3 + 0], attribs_ptr[src_i * 3 + 1], attribs_ptr[src_i * 3 + 2]);
	}
	return ret;
}

Vector<Color> GLTFDocument::_decode_accessor_as_color(Ref<GLTFState> p_state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex, const Vector<int> &p_packed_vertex_ids) {
	const Vector<double> attribs = _decode_accessor(p_state, p_accessor, p_for_vertex);
	Vector<Color> ret;

	if (attribs.size() == 0) {
		return ret;
	}

	const int accessor_type = p_state->accessors[p_accessor]->accessor_type;
	ERR_FAIL_COND_V(!(accessor_type == GLTFAccessor::TYPE_VEC3 || accessor_type == GLTFAccessor::TYPE_VEC4), ret);
	int vec_len = 3;
	if (accessor_type == GLTFAccessor::TYPE_VEC4) {
		vec_len = 4;
	}

	ERR_FAIL_COND_V(attribs.size() % vec_len != 0, ret);
	const double *attribs_ptr = attribs.ptr();
	int ret_size = attribs.size() / vec_len;
	if (!p_packed_vertex_ids.is_empty()) {
		ERR_FAIL_COND_V(p_packed_vertex_ids[p_packed_vertex_ids.size() - 1] >= ret_size, ret);
		ret_size = p_packed_vertex_ids.size();
	}
	ret.resize(ret_size);
	for (int i = 0; i < ret_size; i++) {
		int src_i = i;
		if (!p_packed_vertex_ids.is_empty()) {
			src_i = p_packed_vertex_ids[i];
		}
		ret.write[i] = Color(attribs_ptr[src_i * vec_len + 0], attribs_ptr[src_i * vec_len + 1], attribs_ptr[src_i * vec_len + 2], vec_len == 4 ? attribs_ptr[src_i * 4 + 3] : 1.0);
	}
	return ret;
}
Vector<Quaternion> GLTFDocument::_decode_accessor_as_quaternion(Ref<GLTFState> p_state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {
	const Vector<double> attribs = _decode_accessor(p_state, p_accessor, p_for_vertex);
	Vector<Quaternion> ret;

	if (attribs.size() == 0) {
		return ret;
	}

	ERR_FAIL_COND_V(attribs.size() % 4 != 0, ret);
	const double *attribs_ptr = attribs.ptr();
	const int ret_size = attribs.size() / 4;
	ret.resize(ret_size);
	{
		for (int i = 0; i < ret_size; i++) {
			ret.write[i] = Quaternion(attribs_ptr[i * 4 + 0], attribs_ptr[i * 4 + 1], attribs_ptr[i * 4 + 2], attribs_ptr[i * 4 + 3]).normalized();
		}
	}
	return ret;
}
Vector<Transform2D> GLTFDocument::_decode_accessor_as_xform2d(Ref<GLTFState> p_state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {
	const Vector<double> attribs = _decode_accessor(p_state, p_accessor, p_for_vertex);
	Vector<Transform2D> ret;

	if (attribs.size() == 0) {
		return ret;
	}

	ERR_FAIL_COND_V(attribs.size() % 4 != 0, ret);
	ret.resize(attribs.size() / 4);
	for (int i = 0; i < ret.size(); i++) {
		ret.write[i][0] = Vector2(attribs[i * 4 + 0], attribs[i * 4 + 1]);
		ret.write[i][1] = Vector2(attribs[i * 4 + 2], attribs[i * 4 + 3]);
	}
	return ret;
}

Vector<Basis> GLTFDocument::_decode_accessor_as_basis(Ref<GLTFState> p_state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {
	const Vector<double> attribs = _decode_accessor(p_state, p_accessor, p_for_vertex);
	Vector<Basis> ret;

	if (attribs.size() == 0) {
		return ret;
	}

	ERR_FAIL_COND_V(attribs.size() % 9 != 0, ret);
	ret.resize(attribs.size() / 9);
	for (int i = 0; i < ret.size(); i++) {
		ret.write[i].set_column(0, Vector3(attribs[i * 9 + 0], attribs[i * 9 + 1], attribs[i * 9 + 2]));
		ret.write[i].set_column(1, Vector3(attribs[i * 9 + 3], attribs[i * 9 + 4], attribs[i * 9 + 5]));
		ret.write[i].set_column(2, Vector3(attribs[i * 9 + 6], attribs[i * 9 + 7], attribs[i * 9 + 8]));
	}
	return ret;
}

Vector<Transform3D> GLTFDocument::_decode_accessor_as_xform(Ref<GLTFState> p_state, const GLTFAccessorIndex p_accessor, const bool p_for_vertex) {
	const Vector<double> attribs = _decode_accessor(p_state, p_accessor, p_for_vertex);
	Vector<Transform3D> ret;

	if (attribs.size() == 0) {
		return ret;
	}

	ERR_FAIL_COND_V(attribs.size() % 16 != 0, ret);
	ret.resize(attribs.size() / 16);
	for (int i = 0; i < ret.size(); i++) {
		ret.write[i].basis.set_column(0, Vector3(attribs[i * 16 + 0], attribs[i * 16 + 1], attribs[i * 16 + 2]));
		ret.write[i].basis.set_column(1, Vector3(attribs[i * 16 + 4], attribs[i * 16 + 5], attribs[i * 16 + 6]));
		ret.write[i].basis.set_column(2, Vector3(attribs[i * 16 + 8], attribs[i * 16 + 9], attribs[i * 16 + 10]));
		ret.write[i].set_origin(Vector3(attribs[i * 16 + 12], attribs[i * 16 + 13], attribs[i * 16 + 14]));
	}
	return ret;
}

Error GLTFDocument::_serialize_meshes(Ref<GLTFState> p_state) {
	Array meshes;
	for (GLTFMeshIndex gltf_mesh_i = 0; gltf_mesh_i < p_state->meshes.size(); gltf_mesh_i++) {
		print_verbose("glTF: Serializing mesh: " + itos(gltf_mesh_i));
		Ref<ImporterMesh> import_mesh = p_state->meshes.write[gltf_mesh_i]->get_mesh();
		if (import_mesh.is_null()) {
			continue;
		}
		Array instance_materials = p_state->meshes.write[gltf_mesh_i]->get_instance_materials();
		Array primitives;
		Dictionary gltf_mesh;
		Array target_names;
		Array weights;
		for (int morph_i = 0; morph_i < import_mesh->get_blend_shape_count(); morph_i++) {
			target_names.push_back(import_mesh->get_blend_shape_name(morph_i));
		}
		for (int surface_i = 0; surface_i < import_mesh->get_surface_count(); surface_i++) {
			Array targets;
			Dictionary primitive;
			Mesh::PrimitiveType primitive_type = import_mesh->get_surface_primitive_type(surface_i);
			switch (primitive_type) {
				case Mesh::PRIMITIVE_POINTS: {
					primitive["mode"] = 0;
					break;
				}
				case Mesh::PRIMITIVE_LINES: {
					primitive["mode"] = 1;
					break;
				}
				// case Mesh::PRIMITIVE_LINE_LOOP: {
				// 	primitive["mode"] = 2;
				// 	break;
				// }
				case Mesh::PRIMITIVE_LINE_STRIP: {
					primitive["mode"] = 3;
					break;
				}
				case Mesh::PRIMITIVE_TRIANGLES: {
					primitive["mode"] = 4;
					break;
				}
				case Mesh::PRIMITIVE_TRIANGLE_STRIP: {
					primitive["mode"] = 5;
					break;
				}
				// case Mesh::PRIMITIVE_TRIANGLE_FAN: {
				// 	primitive["mode"] = 6;
				// 	break;
				// }
				default: {
					ERR_FAIL_V(FAILED);
				}
			}

			Array array = import_mesh->get_surface_arrays(surface_i);
			uint64_t format = import_mesh->get_surface_format(surface_i);
			int32_t vertex_num = 0;
			Dictionary attributes;
			{
				Vector<Vector3> a = array[Mesh::ARRAY_VERTEX];
				ERR_FAIL_COND_V(a.is_empty(), ERR_INVALID_DATA);
				attributes["POSITION"] = _encode_accessor_as_vec3(p_state, a, true);
				vertex_num = a.size();
			}
			{
				Vector<real_t> a = array[Mesh::ARRAY_TANGENT];
				if (a.size()) {
					const int ret_size = a.size() / 4;
					Vector<Color> attribs;
					attribs.resize(ret_size);
					for (int i = 0; i < ret_size; i++) {
						Color out;
						out.r = a[(i * 4) + 0];
						out.g = a[(i * 4) + 1];
						out.b = a[(i * 4) + 2];
						out.a = a[(i * 4) + 3];
						attribs.write[i] = out;
					}
					attributes["TANGENT"] = _encode_accessor_as_color(p_state, attribs, true);
				}
			}
			{
				Vector<Vector3> a = array[Mesh::ARRAY_NORMAL];
				if (a.size()) {
					const int ret_size = a.size();
					Vector<Vector3> attribs;
					attribs.resize(ret_size);
					for (int i = 0; i < ret_size; i++) {
						attribs.write[i] = Vector3(a[i]).normalized();
					}
					attributes["NORMAL"] = _encode_accessor_as_vec3(p_state, attribs, true);
				}
			}
			{
				Vector<Vector2> a = array[Mesh::ARRAY_TEX_UV];
				if (a.size()) {
					attributes["TEXCOORD_0"] = _encode_accessor_as_vec2(p_state, a, true);
				}
			}
			{
				Vector<Vector2> a = array[Mesh::ARRAY_TEX_UV2];
				if (a.size()) {
					attributes["TEXCOORD_1"] = _encode_accessor_as_vec2(p_state, a, true);
				}
			}
			for (int custom_i = 0; custom_i < 3; custom_i++) {
				Vector<float> a = array[Mesh::ARRAY_CUSTOM0 + custom_i];
				if (a.size()) {
					int num_channels = 4;
					int custom_shift = Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT + custom_i * Mesh::ARRAY_FORMAT_CUSTOM_BITS;
					switch ((format >> custom_shift) & Mesh::ARRAY_FORMAT_CUSTOM_MASK) {
						case Mesh::ARRAY_CUSTOM_R_FLOAT:
							num_channels = 1;
							break;
						case Mesh::ARRAY_CUSTOM_RG_FLOAT:
							num_channels = 2;
							break;
						case Mesh::ARRAY_CUSTOM_RGB_FLOAT:
							num_channels = 3;
							break;
						case Mesh::ARRAY_CUSTOM_RGBA_FLOAT:
							num_channels = 4;
							break;
					}
					int texcoord_i = 2 + 2 * custom_i;
					String gltf_texcoord_key;
					for (int prev_texcoord_i = 0; prev_texcoord_i < texcoord_i; prev_texcoord_i++) {
						gltf_texcoord_key = vformat("TEXCOORD_%d", prev_texcoord_i);
						if (!attributes.has(gltf_texcoord_key)) {
							Vector<Vector2> empty;
							empty.resize(vertex_num);
							attributes[gltf_texcoord_key] = _encode_accessor_as_vec2(p_state, empty, true);
						}
					}

					LocalVector<Vector2> first_channel;
					first_channel.resize(vertex_num);
					LocalVector<Vector2> second_channel;
					second_channel.resize(vertex_num);
					for (int32_t vert_i = 0; vert_i < vertex_num; vert_i++) {
						float u = a[vert_i * num_channels + 0];
						float v = (num_channels == 1 ? 0.0f : a[vert_i * num_channels + 1]);
						first_channel[vert_i] = Vector2(u, v);
						u = 0;
						v = 0;
						if (num_channels >= 3) {
							u = a[vert_i * num_channels + 2];
							v = (num_channels == 3 ? 0.0f : a[vert_i * num_channels + 3]);
							second_channel[vert_i] = Vector2(u, v);
						}
					}
					gltf_texcoord_key = vformat("TEXCOORD_%d", texcoord_i);
					attributes[gltf_texcoord_key] = _encode_accessor_as_vec2(p_state, first_channel, true);
					gltf_texcoord_key = vformat("TEXCOORD_%d", texcoord_i + 1);
					attributes[gltf_texcoord_key] = _encode_accessor_as_vec2(p_state, second_channel, true);
				}
			}
			{
				Vector<Color> a = array[Mesh::ARRAY_COLOR];
				if (a.size()) {
					attributes["COLOR_0"] = _encode_accessor_as_color(p_state, a, true);
				}
			}
			HashMap<int, int> joint_i_to_bone_i;
			for (GLTFNodeIndex node_i = 0; node_i < p_state->nodes.size(); node_i++) {
				GLTFSkinIndex skin_i = -1;
				if (p_state->nodes[node_i]->mesh == gltf_mesh_i) {
					skin_i = p_state->nodes[node_i]->skin;
				}
				if (skin_i != -1) {
					joint_i_to_bone_i = p_state->skins[skin_i]->joint_i_to_bone_i;
					break;
				}
			}
			{
				const Array &a = array[Mesh::ARRAY_BONES];
				const Vector<Vector3> &vertex_array = array[Mesh::ARRAY_VERTEX];
				if ((a.size() / JOINT_GROUP_SIZE) == vertex_array.size()) {
					const int ret_size = a.size() / JOINT_GROUP_SIZE;
					Vector<Color> attribs;
					attribs.resize(ret_size);
					{
						for (int array_i = 0; array_i < attribs.size(); array_i++) {
							int32_t joint_0 = a[(array_i * JOINT_GROUP_SIZE) + 0];
							int32_t joint_1 = a[(array_i * JOINT_GROUP_SIZE) + 1];
							int32_t joint_2 = a[(array_i * JOINT_GROUP_SIZE) + 2];
							int32_t joint_3 = a[(array_i * JOINT_GROUP_SIZE) + 3];
							attribs.write[array_i] = Color(joint_0, joint_1, joint_2, joint_3);
						}
					}
					attributes["JOINTS_0"] = _encode_accessor_as_joints(p_state, attribs, true);
				} else if ((a.size() / (JOINT_GROUP_SIZE * 2)) >= vertex_array.size()) {
					Vector<Color> joints_0;
					joints_0.resize(vertex_num);
					Vector<Color> joints_1;
					joints_1.resize(vertex_num);
					int32_t weights_8_count = JOINT_GROUP_SIZE * 2;
					for (int32_t vertex_i = 0; vertex_i < vertex_num; vertex_i++) {
						Color joint_0;
						joint_0.r = a[vertex_i * weights_8_count + 0];
						joint_0.g = a[vertex_i * weights_8_count + 1];
						joint_0.b = a[vertex_i * weights_8_count + 2];
						joint_0.a = a[vertex_i * weights_8_count + 3];
						joints_0.write[vertex_i] = joint_0;
						Color joint_1;
						joint_1.r = a[vertex_i * weights_8_count + 4];
						joint_1.g = a[vertex_i * weights_8_count + 5];
						joint_1.b = a[vertex_i * weights_8_count + 6];
						joint_1.a = a[vertex_i * weights_8_count + 7];
						joints_1.write[vertex_i] = joint_1;
					}
					attributes["JOINTS_0"] = _encode_accessor_as_joints(p_state, joints_0, true);
					attributes["JOINTS_1"] = _encode_accessor_as_joints(p_state, joints_1, true);
				}
			}
			{
				const Array &a = array[Mesh::ARRAY_WEIGHTS];
				const Vector<Vector3> &vertex_array = array[Mesh::ARRAY_VERTEX];
				if ((a.size() / JOINT_GROUP_SIZE) == vertex_array.size()) {
					int32_t vertex_count = vertex_array.size();
					Vector<Color> attribs;
					attribs.resize(vertex_count);
					for (int i = 0; i < vertex_count; i++) {
						Color weight_0(a[(i * JOINT_GROUP_SIZE) + 0], a[(i * JOINT_GROUP_SIZE) + 1], a[(i * JOINT_GROUP_SIZE) + 2], a[(i * JOINT_GROUP_SIZE) + 3]);
						float divisor = weight_0.r + weight_0.g + weight_0.b + weight_0.a;
						if (Math::is_zero_approx(divisor) || !Math::is_finite(divisor)) {
							divisor = 1.0;
							weight_0 = Color(1, 0, 0, 0);
						}
						attribs.write[i] = weight_0 / divisor;
					}
					attributes["WEIGHTS_0"] = _encode_accessor_as_weights(p_state, attribs, true);
				} else if ((a.size() / (JOINT_GROUP_SIZE * 2)) >= vertex_array.size()) {
					Vector<Color> weights_0;
					weights_0.resize(vertex_num);
					Vector<Color> weights_1;
					weights_1.resize(vertex_num);
					int32_t weights_8_count = JOINT_GROUP_SIZE * 2;
					for (int32_t vertex_i = 0; vertex_i < vertex_num; vertex_i++) {
						Color weight_0;
						weight_0.r = a[vertex_i * weights_8_count + 0];
						weight_0.g = a[vertex_i * weights_8_count + 1];
						weight_0.b = a[vertex_i * weights_8_count + 2];
						weight_0.a = a[vertex_i * weights_8_count + 3];
						Color weight_1;
						weight_1.r = a[vertex_i * weights_8_count + 4];
						weight_1.g = a[vertex_i * weights_8_count + 5];
						weight_1.b = a[vertex_i * weights_8_count + 6];
						weight_1.a = a[vertex_i * weights_8_count + 7];
						float divisor = weight_0.r + weight_0.g + weight_0.b + weight_0.a + weight_1.r + weight_1.g + weight_1.b + weight_1.a;
						if (Math::is_zero_approx(divisor) || !Math::is_finite(divisor)) {
							divisor = 1.0f;
							weight_0 = Color(1, 0, 0, 0);
							weight_1 = Color(0, 0, 0, 0);
						}
						weights_0.write[vertex_i] = weight_0 / divisor;
						weights_1.write[vertex_i] = weight_1 / divisor;
					}
					attributes["WEIGHTS_0"] = _encode_accessor_as_weights(p_state, weights_0, true);
					attributes["WEIGHTS_1"] = _encode_accessor_as_weights(p_state, weights_1, true);
				}
			}
			{
				Vector<int32_t> mesh_indices = array[Mesh::ARRAY_INDEX];
				if (mesh_indices.size()) {
					if (primitive_type == Mesh::PRIMITIVE_TRIANGLES) {
						// Swap around indices, convert ccw to cw for front face.
						const int is = mesh_indices.size();
						for (int k = 0; k < is; k += 3) {
							SWAP(mesh_indices.write[k + 0], mesh_indices.write[k + 2]);
						}
					}
					primitive["indices"] = _encode_accessor_as_ints(p_state, mesh_indices, false, true);
				} else {
					if (primitive_type == Mesh::PRIMITIVE_TRIANGLES) {
						// Generate indices because they need to be swapped for CW/CCW.
						const Vector<Vector3> &vertices = array[Mesh::ARRAY_VERTEX];
						Ref<SurfaceTool> st;
						st.instantiate();
						st->create_from_triangle_arrays(array);
						st->index();
						Vector<int32_t> generated_indices = st->commit_to_arrays()[Mesh::ARRAY_INDEX];
						const int vs = vertices.size();
						generated_indices.resize(vs);
						{
							for (int k = 0; k < vs; k += 3) {
								generated_indices.write[k] = k;
								generated_indices.write[k + 1] = k + 2;
								generated_indices.write[k + 2] = k + 1;
							}
						}
						primitive["indices"] = _encode_accessor_as_ints(p_state, generated_indices, false, true);
					}
				}
			}

			primitive["attributes"] = attributes;

			// Blend shapes
			print_verbose("glTF: Mesh has targets");
			if (import_mesh->get_blend_shape_count()) {
				ArrayMesh::BlendShapeMode shape_mode = import_mesh->get_blend_shape_mode();
				const float normal_tangent_sparse_rounding = 0.001;
				for (int morph_i = 0; morph_i < import_mesh->get_blend_shape_count(); morph_i++) {
					Array array_morph = import_mesh->get_surface_blend_shape_arrays(surface_i, morph_i);
					Dictionary t;
					Vector<Vector3> varr = array_morph[Mesh::ARRAY_VERTEX];
					Vector<Vector3> src_varr = array[Mesh::ARRAY_VERTEX];
					Array mesh_arrays = import_mesh->get_surface_arrays(surface_i);
					if (varr.size() && varr.size() == src_varr.size()) {
						if (shape_mode == ArrayMesh::BlendShapeMode::BLEND_SHAPE_MODE_NORMALIZED) {
							const int max_idx = src_varr.size();
							for (int blend_i = 0; blend_i < max_idx; blend_i++) {
								varr.write[blend_i] = varr[blend_i] - src_varr[blend_i];
							}
						}
						GLTFAccessorIndex position_accessor = attributes["POSITION"];
						if (position_accessor != -1) {
							int new_accessor = _encode_sparse_accessor_as_vec3(p_state, varr, Vector<Vector3>(), 1.0, true, -1);
							if (new_accessor != -1) {
								t["POSITION"] = new_accessor;
							}
						}
					}

					Vector<Vector3> narr = array_morph[Mesh::ARRAY_NORMAL];
					Vector<Vector3> src_narr = array[Mesh::ARRAY_NORMAL];
					if (narr.size() && narr.size() == src_narr.size()) {
						if (shape_mode == ArrayMesh::BlendShapeMode::BLEND_SHAPE_MODE_NORMALIZED) {
							const int max_idx = src_narr.size();
							for (int blend_i = 0; blend_i < max_idx; blend_i++) {
								narr.write[blend_i] = narr[blend_i] - src_narr[blend_i];
							}
						}
						GLTFAccessorIndex normal_accessor = attributes["NORMAL"];
						if (normal_accessor != -1) {
							int new_accessor = _encode_sparse_accessor_as_vec3(p_state, narr, Vector<Vector3>(), normal_tangent_sparse_rounding, true, -1);
							if (new_accessor != -1) {
								t["NORMAL"] = new_accessor;
							}
						}
					}
					Vector<real_t> tarr = array_morph[Mesh::ARRAY_TANGENT];
					Vector<real_t> src_tarr = array[Mesh::ARRAY_TANGENT];
					if (tarr.size() && tarr.size() == src_tarr.size()) {
						const int ret_size = tarr.size() / 4;
						Vector<Vector3> attribs;
						attribs.resize(ret_size);
						for (int i = 0; i < ret_size; i++) {
							Vector3 vec3;
							vec3.x = tarr[(i * 4) + 0] - src_tarr[(i * 4) + 0];
							vec3.y = tarr[(i * 4) + 1] - src_tarr[(i * 4) + 1];
							vec3.z = tarr[(i * 4) + 2] - src_tarr[(i * 4) + 2];
							attribs.write[i] = vec3;
						}
						GLTFAccessorIndex tangent_accessor = attributes["TANGENT"];
						if (tangent_accessor != -1) {
							int new_accessor = _encode_sparse_accessor_as_vec3(p_state, attribs, Vector<Vector3>(), normal_tangent_sparse_rounding, true, -1);
							if (new_accessor != -1) {
								t["TANGENT"] = new_accessor;
							}
						}
					}
					targets.push_back(t);
				}
			}

			Variant v;
			if (surface_i < instance_materials.size()) {
				v = instance_materials.get(surface_i);
			}
			Ref<Material> mat = v;
			if (!mat.is_valid()) {
				mat = import_mesh->get_surface_material(surface_i);
			}
			if (mat.is_valid()) {
				HashMap<Ref<Material>, GLTFMaterialIndex>::Iterator material_cache_i = p_state->material_cache.find(mat);
				if (material_cache_i && material_cache_i->value != -1) {
					primitive["material"] = material_cache_i->value;
				} else {
					GLTFMaterialIndex mat_i = p_state->materials.size();
					p_state->materials.push_back(mat);
					primitive["material"] = mat_i;
					p_state->material_cache.insert(mat, mat_i);
				}
			}

			if (targets.size()) {
				primitive["targets"] = targets;
			}

			primitives.push_back(primitive);
		}

		Dictionary e;
		e["targetNames"] = target_names;
		gltf_mesh["extras"] = e;
		_attach_meta_to_extras(import_mesh, gltf_mesh);

		weights.resize(target_names.size());
		for (int name_i = 0; name_i < target_names.size(); name_i++) {
			real_t weight = 0.0;
			if (name_i < p_state->meshes.write[gltf_mesh_i]->get_blend_weights().size()) {
				weight = p_state->meshes.write[gltf_mesh_i]->get_blend_weights()[name_i];
			}
			weights[name_i] = weight;
		}
		if (weights.size()) {
			gltf_mesh["weights"] = weights;
		}

		ERR_FAIL_COND_V(target_names.size() != weights.size(), FAILED);

		gltf_mesh["primitives"] = primitives;

		meshes.push_back(gltf_mesh);
	}

	if (!meshes.size()) {
		return OK;
	}
	p_state->json["meshes"] = meshes;
	print_verbose("glTF: Total meshes: " + itos(meshes.size()));

	return OK;
}

Error GLTFDocument::_parse_meshes(Ref<GLTFState> p_state) {
	if (!p_state->json.has("meshes")) {
		return OK;
	}

	Array meshes = p_state->json["meshes"];
	for (GLTFMeshIndex i = 0; i < meshes.size(); i++) {
		print_verbose("glTF: Parsing mesh: " + itos(i));
		Dictionary d = meshes[i];

		Ref<GLTFMesh> mesh;
		mesh.instantiate();
		bool has_vertex_color = false;

		ERR_FAIL_COND_V(!d.has("primitives"), ERR_PARSE_ERROR);

		Array primitives = d["primitives"];
		const Dictionary &extras = d.has("extras") ? (Dictionary)d["extras"] : Dictionary();
		_attach_extras_to_meta(extras, mesh);
		Ref<ImporterMesh> import_mesh;
		import_mesh.instantiate();
		String mesh_name = "mesh";
		if (d.has("name") && !String(d["name"]).is_empty()) {
			mesh_name = d["name"];
			mesh->set_original_name(mesh_name);
		}
		import_mesh->set_name(_gen_unique_name(p_state, vformat("%s_%s", p_state->scene_name, mesh_name)));
		mesh->set_name(import_mesh->get_name());

		for (int j = 0; j < primitives.size(); j++) {
			uint64_t flags = RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES;
			Dictionary p = primitives[j];

			Array array;
			array.resize(Mesh::ARRAY_MAX);

			ERR_FAIL_COND_V(!p.has("attributes"), ERR_PARSE_ERROR);

			Dictionary a = p["attributes"];

			Mesh::PrimitiveType primitive = Mesh::PRIMITIVE_TRIANGLES;
			if (p.has("mode")) {
				const int mode = p["mode"];
				ERR_FAIL_INDEX_V(mode, 7, ERR_FILE_CORRUPT);
				// Convert mesh.primitive.mode to Godot Mesh enum. See:
				// https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#_mesh_primitive_mode
				static const Mesh::PrimitiveType primitives2[7] = {
					Mesh::PRIMITIVE_POINTS, // 0 POINTS
					Mesh::PRIMITIVE_LINES, // 1 LINES
					Mesh::PRIMITIVE_LINES, // 2 LINE_LOOP; loop not supported, should be converted
					Mesh::PRIMITIVE_LINE_STRIP, // 3 LINE_STRIP
					Mesh::PRIMITIVE_TRIANGLES, // 4 TRIANGLES
					Mesh::PRIMITIVE_TRIANGLE_STRIP, // 5 TRIANGLE_STRIP
					Mesh::PRIMITIVE_TRIANGLES, // 6 TRIANGLE_FAN fan not supported, should be converted
					// TODO: Line loop and triangle fan are not supported and need to be converted to lines and triangles.
				};

				primitive = primitives2[mode];
			}

			int32_t orig_vertex_num = 0;
			ERR_FAIL_COND_V(!a.has("POSITION"), ERR_PARSE_ERROR);
			if (a.has("POSITION")) {
				PackedVector3Array vertices = _decode_accessor_as_vec3(p_state, a["POSITION"], true);
				array[Mesh::ARRAY_VERTEX] = vertices;
				orig_vertex_num = vertices.size();
			}
			int32_t vertex_num = orig_vertex_num;

			Vector<int> indices;
			Vector<int> indices_mapping;
			Vector<int> indices_rev_mapping;
			Vector<int> indices_vec4_mapping;
			if (p.has("indices")) {
				indices = _decode_accessor_as_ints(p_state, p["indices"], false);
				const int is = indices.size();

				if (primitive == Mesh::PRIMITIVE_TRIANGLES) {
					// Swap around indices, convert ccw to cw for front face.

					int *w = indices.ptrw();
					for (int k = 0; k < is; k += 3) {
						SWAP(w[k + 1], w[k + 2]);
					}
				}

				const int *indices_w = indices.ptrw();
				Vector<bool> used_indices;
				used_indices.resize_zeroed(orig_vertex_num);
				bool *used_w = used_indices.ptrw();
				for (int idx_i = 0; idx_i < is; idx_i++) {
					ERR_FAIL_INDEX_V(indices_w[idx_i], orig_vertex_num, ERR_INVALID_DATA);
					used_w[indices_w[idx_i]] = true;
				}
				indices_rev_mapping.resize_zeroed(orig_vertex_num);
				int *rev_w = indices_rev_mapping.ptrw();
				vertex_num = 0;
				for (int vert_i = 0; vert_i < orig_vertex_num; vert_i++) {
					if (used_w[vert_i]) {
						rev_w[vert_i] = indices_mapping.size();
						indices_mapping.push_back(vert_i);
						indices_vec4_mapping.push_back(vert_i * 4 + 0);
						indices_vec4_mapping.push_back(vert_i * 4 + 1);
						indices_vec4_mapping.push_back(vert_i * 4 + 2);
						indices_vec4_mapping.push_back(vert_i * 4 + 3);
						vertex_num++;
					}
				}
			}
			ERR_FAIL_COND_V(vertex_num <= 0, ERR_INVALID_DECLARATION);

			if (a.has("POSITION")) {
				PackedVector3Array vertices = _decode_accessor_as_vec3(p_state, a["POSITION"], true, indices_mapping);
				array[Mesh::ARRAY_VERTEX] = vertices;
			}
			if (a.has("NORMAL")) {
				array[Mesh::ARRAY_NORMAL] = _decode_accessor_as_vec3(p_state, a["NORMAL"], true, indices_mapping);
			}
			if (a.has("TANGENT")) {
				array[Mesh::ARRAY_TANGENT] = _decode_accessor_as_floats(p_state, a["TANGENT"], true, indices_vec4_mapping);
			}
			if (a.has("TEXCOORD_0")) {
				array[Mesh::ARRAY_TEX_UV] = _decode_accessor_as_vec2(p_state, a["TEXCOORD_0"], true, indices_mapping);
			}
			if (a.has("TEXCOORD_1")) {
				array[Mesh::ARRAY_TEX_UV2] = _decode_accessor_as_vec2(p_state, a["TEXCOORD_1"], true, indices_mapping);
			}
			for (int custom_i = 0; custom_i < 3; custom_i++) {
				Vector<float> cur_custom;
				Vector<Vector2> texcoord_first;
				Vector<Vector2> texcoord_second;

				int texcoord_i = 2 + 2 * custom_i;
				String gltf_texcoord_key = vformat("TEXCOORD_%d", texcoord_i);
				int num_channels = 0;
				if (a.has(gltf_texcoord_key)) {
					texcoord_first = _decode_accessor_as_vec2(p_state, a[gltf_texcoord_key], true, indices_mapping);
					num_channels = 2;
				}
				gltf_texcoord_key = vformat("TEXCOORD_%d", texcoord_i + 1);
				if (a.has(gltf_texcoord_key)) {
					texcoord_second = _decode_accessor_as_vec2(p_state, a[gltf_texcoord_key], true, indices_mapping);
					num_channels = 4;
				}
				if (!num_channels) {
					break;
				}
				if (num_channels == 2 || num_channels == 4) {
					cur_custom.resize(vertex_num * num_channels);
					for (int32_t uv_i = 0; uv_i < texcoord_first.size() && uv_i < vertex_num; uv_i++) {
						cur_custom.write[uv_i * num_channels + 0] = texcoord_first[uv_i].x;
						cur_custom.write[uv_i * num_channels + 1] = texcoord_first[uv_i].y;
					}
					// Vector.resize seems to not zero-initialize. Ensure all unused elements are 0:
					for (int32_t uv_i = texcoord_first.size(); uv_i < vertex_num; uv_i++) {
						cur_custom.write[uv_i * num_channels + 0] = 0;
						cur_custom.write[uv_i * num_channels + 1] = 0;
					}
				}
				if (num_channels == 4) {
					for (int32_t uv_i = 0; uv_i < texcoord_second.size() && uv_i < vertex_num; uv_i++) {
						// num_channels must be 4
						cur_custom.write[uv_i * num_channels + 2] = texcoord_second[uv_i].x;
						cur_custom.write[uv_i * num_channels + 3] = texcoord_second[uv_i].y;
					}
					// Vector.resize seems to not zero-initialize. Ensure all unused elements are 0:
					for (int32_t uv_i = texcoord_second.size(); uv_i < vertex_num; uv_i++) {
						cur_custom.write[uv_i * num_channels + 2] = 0;
						cur_custom.write[uv_i * num_channels + 3] = 0;
					}
				}
				if (cur_custom.size() > 0) {
					array[Mesh::ARRAY_CUSTOM0 + custom_i] = cur_custom;
					int custom_shift = Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT + custom_i * Mesh::ARRAY_FORMAT_CUSTOM_BITS;
					if (num_channels == 2) {
						flags |= Mesh::ARRAY_CUSTOM_RG_FLOAT << custom_shift;
					} else {
						flags |= Mesh::ARRAY_CUSTOM_RGBA_FLOAT << custom_shift;
					}
				}
			}
			if (a.has("COLOR_0")) {
				array[Mesh::ARRAY_COLOR] = _decode_accessor_as_color(p_state, a["COLOR_0"], true, indices_mapping);
				has_vertex_color = true;
			}
			if (a.has("JOINTS_0") && !a.has("JOINTS_1")) {
				PackedInt32Array joints_0 = _decode_accessor_as_ints(p_state, a["JOINTS_0"], true, indices_vec4_mapping);
				ERR_FAIL_COND_V(joints_0.size() != 4 * vertex_num, ERR_INVALID_DATA);
				array[Mesh::ARRAY_BONES] = joints_0;
			} else if (a.has("JOINTS_0") && a.has("JOINTS_1")) {
				PackedInt32Array joints_0 = _decode_accessor_as_ints(p_state, a["JOINTS_0"], true, indices_vec4_mapping);
				PackedInt32Array joints_1 = _decode_accessor_as_ints(p_state, a["JOINTS_1"], true, indices_vec4_mapping);
				ERR_FAIL_COND_V(joints_0.size() != joints_1.size(), ERR_INVALID_DATA);
				ERR_FAIL_COND_V(joints_0.size() != 4 * vertex_num, ERR_INVALID_DATA);
				int32_t weight_8_count = JOINT_GROUP_SIZE * 2;
				Vector<int> joints;
				joints.resize(vertex_num * weight_8_count);
				for (int32_t vertex_i = 0; vertex_i < vertex_num; vertex_i++) {
					joints.write[vertex_i * weight_8_count + 0] = joints_0[vertex_i * JOINT_GROUP_SIZE + 0];
					joints.write[vertex_i * weight_8_count + 1] = joints_0[vertex_i * JOINT_GROUP_SIZE + 1];
					joints.write[vertex_i * weight_8_count + 2] = joints_0[vertex_i * JOINT_GROUP_SIZE + 2];
					joints.write[vertex_i * weight_8_count + 3] = joints_0[vertex_i * JOINT_GROUP_SIZE + 3];
					joints.write[vertex_i * weight_8_count + 4] = joints_1[vertex_i * JOINT_GROUP_SIZE + 0];
					joints.write[vertex_i * weight_8_count + 5] = joints_1[vertex_i * JOINT_GROUP_SIZE + 1];
					joints.write[vertex_i * weight_8_count + 6] = joints_1[vertex_i * JOINT_GROUP_SIZE + 2];
					joints.write[vertex_i * weight_8_count + 7] = joints_1[vertex_i * JOINT_GROUP_SIZE + 3];
				}
				array[Mesh::ARRAY_BONES] = joints;
			}
			if (a.has("WEIGHTS_0") && !a.has("WEIGHTS_1")) {
				Vector<float> weights = _decode_accessor_as_floats(p_state, a["WEIGHTS_0"], true, indices_vec4_mapping);
				ERR_FAIL_COND_V(weights.size() != 4 * vertex_num, ERR_INVALID_DATA);
				{ // glTF does not seem to normalize the weights for some reason.
					int wc = weights.size();
					float *w = weights.ptrw();

					for (int k = 0; k < wc; k += 4) {
						float total = 0.0;
						total += w[k + 0];
						total += w[k + 1];
						total += w[k + 2];
						total += w[k + 3];
						if (total > 0.0) {
							w[k + 0] /= total;
							w[k + 1] /= total;
							w[k + 2] /= total;
							w[k + 3] /= total;
						}
					}
				}
				array[Mesh::ARRAY_WEIGHTS] = weights;
			} else if (a.has("WEIGHTS_0") && a.has("WEIGHTS_1")) {
				Vector<float> weights_0 = _decode_accessor_as_floats(p_state, a["WEIGHTS_0"], true, indices_vec4_mapping);
				Vector<float> weights_1 = _decode_accessor_as_floats(p_state, a["WEIGHTS_1"], true, indices_vec4_mapping);
				Vector<float> weights;
				ERR_FAIL_COND_V(weights_0.size() != weights_1.size(), ERR_INVALID_DATA);
				ERR_FAIL_COND_V(weights_0.size() != 4 * vertex_num, ERR_INVALID_DATA);
				int32_t weight_8_count = JOINT_GROUP_SIZE * 2;
				weights.resize(vertex_num * weight_8_count);
				for (int32_t vertex_i = 0; vertex_i < vertex_num; vertex_i++) {
					weights.write[vertex_i * weight_8_count + 0] = weights_0[vertex_i * JOINT_GROUP_SIZE + 0];
					weights.write[vertex_i * weight_8_count + 1] = weights_0[vertex_i * JOINT_GROUP_SIZE + 1];
					weights.write[vertex_i * weight_8_count + 2] = weights_0[vertex_i * JOINT_GROUP_SIZE + 2];
					weights.write[vertex_i * weight_8_count + 3] = weights_0[vertex_i * JOINT_GROUP_SIZE + 3];
					weights.write[vertex_i * weight_8_count + 4] = weights_1[vertex_i * JOINT_GROUP_SIZE + 0];
					weights.write[vertex_i * weight_8_count + 5] = weights_1[vertex_i * JOINT_GROUP_SIZE + 1];
					weights.write[vertex_i * weight_8_count + 6] = weights_1[vertex_i * JOINT_GROUP_SIZE + 2];
					weights.write[vertex_i * weight_8_count + 7] = weights_1[vertex_i * JOINT_GROUP_SIZE + 3];
				}
				{ // glTF does not seem to normalize the weights for some reason.
					int wc = weights.size();
					float *w = weights.ptrw();

					for (int k = 0; k < wc; k += weight_8_count) {
						float total = 0.0;
						total += w[k + 0];
						total += w[k + 1];
						total += w[k + 2];
						total += w[k + 3];
						total += w[k + 4];
						total += w[k + 5];
						total += w[k + 6];
						total += w[k + 7];
						if (total > 0.0) {
							w[k + 0] /= total;
							w[k + 1] /= total;
							w[k + 2] /= total;
							w[k + 3] /= total;
							w[k + 4] /= total;
							w[k + 5] /= total;
							w[k + 6] /= total;
							w[k + 7] /= total;
						}
					}
				}
				array[Mesh::ARRAY_WEIGHTS] = weights;
			}

			if (!indices.is_empty()) {
				int *w = indices.ptrw();
				const int is = indices.size();
				for (int ind_i = 0; ind_i < is; ind_i++) {
					w[ind_i] = indices_rev_mapping[indices[ind_i]];
				}
				array[Mesh::ARRAY_INDEX] = indices;

			} else if (primitive == Mesh::PRIMITIVE_TRIANGLES) {
				// Generate indices because they need to be swapped for CW/CCW.
				const Vector<Vector3> &vertices = array[Mesh::ARRAY_VERTEX];
				ERR_FAIL_COND_V(vertices.is_empty(), ERR_PARSE_ERROR);
				const int vs = vertices.size();
				indices.resize(vs);
				{
					int *w = indices.ptrw();
					for (int k = 0; k < vs; k += 3) {
						w[k] = k;
						w[k + 1] = k + 2;
						w[k + 2] = k + 1;
					}
				}
				array[Mesh::ARRAY_INDEX] = indices;
			}

			bool generate_tangents = p_state->force_generate_tangents && (primitive == Mesh::PRIMITIVE_TRIANGLES && !a.has("TANGENT") && a.has("NORMAL"));

			if (generate_tangents && !a.has("TEXCOORD_0")) {
				// If we don't have UVs we provide a dummy tangent array.
				Vector<float> tangents;
				tangents.resize(vertex_num * 4);
				float *tangentsw = tangents.ptrw();

				Vector<Vector3> normals = array[Mesh::ARRAY_NORMAL];
				for (int k = 0; k < vertex_num; k++) {
					Vector3 tan = Vector3(normals[k].z, -normals[k].x, normals[k].y).cross(normals[k].normalized()).normalized();
					tangentsw[k * 4 + 0] = tan.x;
					tangentsw[k * 4 + 1] = tan.y;
					tangentsw[k * 4 + 2] = tan.z;
					tangentsw[k * 4 + 3] = 1.0;
				}
				array[Mesh::ARRAY_TANGENT] = tangents;
			}

			// Disable compression if all z equals 0 (the mesh is 2D).
			const Vector<Vector3> &vertices = array[Mesh::ARRAY_VERTEX];
			bool is_mesh_2d = true;
			for (int k = 0; k < vertices.size(); k++) {
				if (!Math::is_zero_approx(vertices[k].z)) {
					is_mesh_2d = false;
					break;
				}
			}

			if (p_state->force_disable_compression || is_mesh_2d || !a.has("POSITION") || !a.has("NORMAL") || p.has("targets") || (a.has("JOINTS_0") || a.has("JOINTS_1"))) {
				flags &= ~RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES;
			}

			Ref<SurfaceTool> mesh_surface_tool;
			mesh_surface_tool.instantiate();
			mesh_surface_tool->create_from_triangle_arrays(array);
			if (a.has("JOINTS_0") && a.has("JOINTS_1")) {
				mesh_surface_tool->set_skin_weight_count(SurfaceTool::SKIN_8_WEIGHTS);
			}
			mesh_surface_tool->index();
			if (generate_tangents && a.has("TEXCOORD_0")) {
				//must generate mikktspace tangents.. ergh..
				mesh_surface_tool->generate_tangents();
			}
			array = mesh_surface_tool->commit_to_arrays();

			if ((flags & RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES) && a.has("NORMAL") && (a.has("TANGENT") || generate_tangents)) {
				// Compression is enabled, so let's validate that the normals and tangents are correct.
				Vector<Vector3> normals = array[Mesh::ARRAY_NORMAL];
				Vector<float> tangents = array[Mesh::ARRAY_TANGENT];
				for (int vert = 0; vert < normals.size(); vert++) {
					Vector3 tan = Vector3(tangents[vert * 4 + 0], tangents[vert * 4 + 1], tangents[vert * 4 + 2]);
					if (abs(tan.dot(normals[vert])) > 0.0001) {
						// Tangent is not perpendicular to the normal, so we can't use compression.
						flags &= ~RS::ARRAY_FLAG_COMPRESS_ATTRIBUTES;
					}
				}
			}

			Array morphs;
			// Blend shapes
			if (p.has("targets")) {
				print_verbose("glTF: Mesh has targets");
				const Array &targets = p["targets"];

				import_mesh->set_blend_shape_mode(Mesh::BLEND_SHAPE_MODE_NORMALIZED);

				if (j == 0) {
					const Array &target_names = extras.has("targetNames") ? (Array)extras["targetNames"] : Array();
					for (int k = 0; k < targets.size(); k++) {
						String bs_name;
						if (k < target_names.size() && ((String)target_names[k]).size() != 0) {
							bs_name = (String)target_names[k];
						} else {
							bs_name = String("morph_") + itos(k);
						}
						import_mesh->add_blend_shape(bs_name);
					}
				}

				for (int k = 0; k < targets.size(); k++) {
					const Dictionary &t = targets[k];

					Array array_copy;
					array_copy.resize(Mesh::ARRAY_MAX);

					for (int l = 0; l < Mesh::ARRAY_MAX; l++) {
						array_copy[l] = array[l];
					}

					if (t.has("POSITION")) {
						Vector<Vector3> varr = _decode_accessor_as_vec3(p_state, t["POSITION"], true, indices_mapping);
						const Vector<Vector3> src_varr = array[Mesh::ARRAY_VERTEX];
						const int size = src_varr.size();
						ERR_FAIL_COND_V(size == 0, ERR_PARSE_ERROR);
						{
							const int max_idx = varr.size();
							varr.resize(size);

							Vector3 *w_varr = varr.ptrw();
							const Vector3 *r_varr = varr.ptr();
							const Vector3 *r_src_varr = src_varr.ptr();
							for (int l = 0; l < size; l++) {
								if (l < max_idx) {
									w_varr[l] = r_varr[l] + r_src_varr[l];
								} else {
									w_varr[l] = r_src_varr[l];
								}
							}
						}
						array_copy[Mesh::ARRAY_VERTEX] = varr;
					}
					if (t.has("NORMAL")) {
						Vector<Vector3> narr = _decode_accessor_as_vec3(p_state, t["NORMAL"], true, indices_mapping);
						const Vector<Vector3> src_narr = array[Mesh::ARRAY_NORMAL];
						int size = src_narr.size();
						ERR_FAIL_COND_V(size == 0, ERR_PARSE_ERROR);
						{
							int max_idx = narr.size();
							narr.resize(size);

							Vector3 *w_narr = narr.ptrw();
							const Vector3 *r_narr = narr.ptr();
							const Vector3 *r_src_narr = src_narr.ptr();
							for (int l = 0; l < size; l++) {
								if (l < max_idx) {
									w_narr[l] = r_narr[l] + r_src_narr[l];
								} else {
									w_narr[l] = r_src_narr[l];
								}
							}
						}
						array_copy[Mesh::ARRAY_NORMAL] = narr;
					}
					if (t.has("TANGENT")) {
						const Vector<Vector3> tangents_v3 = _decode_accessor_as_vec3(p_state, t["TANGENT"], true, indices_mapping);
						const Vector<float> src_tangents = array[Mesh::ARRAY_TANGENT];
						ERR_FAIL_COND_V(src_tangents.is_empty(), ERR_PARSE_ERROR);

						Vector<float> tangents_v4;

						{
							int max_idx = tangents_v3.size();

							int size4 = src_tangents.size();
							tangents_v4.resize(size4);
							float *w4 = tangents_v4.ptrw();

							const Vector3 *r3 = tangents_v3.ptr();
							const float *r4 = src_tangents.ptr();

							for (int l = 0; l < size4 / 4; l++) {
								if (l < max_idx) {
									w4[l * 4 + 0] = r3[l].x + r4[l * 4 + 0];
									w4[l * 4 + 1] = r3[l].y + r4[l * 4 + 1];
									w4[l * 4 + 2] = r3[l].z + r4[l * 4 + 2];
								} else {
									w4[l * 4 + 0] = r4[l * 4 + 0];
									w4[l * 4 + 1] = r4[l * 4 + 1];
									w4[l * 4 + 2] = r4[l * 4 + 2];
								}
								w4[l * 4 + 3] = r4[l * 4 + 3]; //copy flip value
							}
						}

						array_copy[Mesh::ARRAY_TANGENT] = tangents_v4;
					}

					Ref<SurfaceTool> blend_surface_tool;
					blend_surface_tool.instantiate();
					blend_surface_tool->create_from_triangle_arrays(array_copy);
					if (a.has("JOINTS_0") && a.has("JOINTS_1")) {
						blend_surface_tool->set_skin_weight_count(SurfaceTool::SKIN_8_WEIGHTS);
					}
					blend_surface_tool->index();
					if (generate_tangents) {
						blend_surface_tool->generate_tangents();
					}
					array_copy = blend_surface_tool->commit_to_arrays();

					// Enforce blend shape mask array format
					for (int l = 0; l < Mesh::ARRAY_MAX; l++) {
						if (!(Mesh::ARRAY_FORMAT_BLEND_SHAPE_MASK & (1ULL << l))) {
							array_copy[l] = Variant();
						}
					}

					morphs.push_back(array_copy);
				}
			}

			Ref<Material> mat;
			String mat_name;
			if (!p_state->discard_meshes_and_materials) {
				if (p.has("material")) {
					const int material = p["material"];
					ERR_FAIL_INDEX_V(material, p_state->materials.size(), ERR_FILE_CORRUPT);
					Ref<Material> mat3d = p_state->materials[material];
					ERR_FAIL_COND_V(mat3d.is_null(), ERR_FILE_CORRUPT);

					Ref<BaseMaterial3D> base_material = mat3d;
					if (has_vertex_color && base_material.is_valid()) {
						base_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
					}
					mat = mat3d;

				} else {
					Ref<StandardMaterial3D> mat3d;
					mat3d.instantiate();
					if (has_vertex_color) {
						mat3d->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
					}
					mat = mat3d;
				}
				ERR_FAIL_COND_V(mat.is_null(), ERR_FILE_CORRUPT);
				mat_name = mat->get_name();
			}
			import_mesh->add_surface(primitive, array, morphs,
					Dictionary(), mat, mat_name, flags);
		}

		Vector<float> blend_weights;
		blend_weights.resize(import_mesh->get_blend_shape_count());
		for (int32_t weight_i = 0; weight_i < blend_weights.size(); weight_i++) {
			blend_weights.write[weight_i] = 0.0f;
		}

		if (d.has("weights")) {
			const Array &weights = d["weights"];
			for (int j = 0; j < weights.size(); j++) {
				if (j >= blend_weights.size()) {
					break;
				}
				blend_weights.write[j] = weights[j];
			}
		}
		mesh->set_blend_weights(blend_weights);
		mesh->set_mesh(import_mesh);

		p_state->meshes.push_back(mesh);
	}

	print_verbose("glTF: Total meshes: " + itos(p_state->meshes.size()));

	return OK;
}

void GLTFDocument::set_naming_version(int p_version) {
	_naming_version = p_version;
}

int GLTFDocument::get_naming_version() const {
	return _naming_version;
}

void GLTFDocument::set_image_format(const String &p_image_format) {
	_image_format = p_image_format;
}

String GLTFDocument::get_image_format() const {
	return _image_format;
}

void GLTFDocument::set_lossy_quality(float p_lossy_quality) {
	_lossy_quality = p_lossy_quality;
}

float GLTFDocument::get_lossy_quality() const {
	return _lossy_quality;
}

Error GLTFDocument::_serialize_images(Ref<GLTFState> p_state) {
	Array images;
	// Check if any extension wants to be the image saver.
	_image_save_extension = Ref<GLTFDocumentExtension>();
	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		Vector<String> image_formats = ext->get_saveable_image_formats();
		if (image_formats.has(_image_format)) {
			_image_save_extension = ext;
			break;
		}
	}
	// Serialize every image in the state's images array.
	for (int i = 0; i < p_state->images.size(); i++) {
		Dictionary image_dict;

		ERR_CONTINUE(p_state->images[i].is_null());

		Ref<Image> image = p_state->images[i]->get_image();
		ERR_CONTINUE(image.is_null());
		if (image->is_compressed()) {
			image->decompress();
			ERR_FAIL_COND_V_MSG(image->is_compressed(), ERR_INVALID_DATA, "glTF: Image was compressed, but could not be decompressed.");
		}

		if (p_state->filename.to_lower().ends_with("gltf")) {
			String img_name = p_state->images[i]->get_name();
			if (img_name.is_empty()) {
				img_name = itos(i);
			}
			img_name = _gen_unique_name(p_state, img_name);
			img_name = img_name.pad_zeros(3);
			String relative_texture_dir = "textures";
			String full_texture_dir = p_state->base_path.path_join(relative_texture_dir);
			Ref<DirAccess> da = DirAccess::open(p_state->base_path);
			ERR_FAIL_COND_V(da.is_null(), FAILED);

			if (!da->dir_exists(full_texture_dir)) {
				da->make_dir(full_texture_dir);
			}
			if (_image_save_extension.is_valid()) {
				img_name = img_name + _image_save_extension->get_image_file_extension();
				Error err = _image_save_extension->save_image_at_path(p_state, image, full_texture_dir.path_join(img_name), _image_format, _lossy_quality);
				ERR_FAIL_COND_V_MSG(err != OK, err, "glTF: Failed to save image in '" + _image_format + "' format as a separate file.");
			} else if (_image_format == "PNG") {
				img_name = img_name + ".png";
				image->save_png(full_texture_dir.path_join(img_name));
			} else if (_image_format == "JPEG") {
				img_name = img_name + ".jpg";
				image->save_jpg(full_texture_dir.path_join(img_name), _lossy_quality);
			} else {
				ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "glTF: Unknown image format '" + _image_format + "'.");
			}
			image_dict["uri"] = relative_texture_dir.path_join(img_name).uri_encode();
		} else {
			GLTFBufferViewIndex bvi;

			Ref<GLTFBufferView> bv;
			bv.instantiate();

			const GLTFBufferIndex bi = 0;
			bv->buffer = bi;
			bv->byte_offset = p_state->buffers[bi].size();
			ERR_FAIL_INDEX_V(bi, p_state->buffers.size(), ERR_PARAMETER_RANGE_ERROR);

			Vector<uint8_t> buffer;
			Ref<ImageTexture> img_tex = image;
			if (img_tex.is_valid()) {
				image = img_tex->get_image();
			}
			// Save in various image formats. Note that if the format is "None",
			// the state's images will be empty, so this code will not be reached.
			if (_image_save_extension.is_valid()) {
				buffer = _image_save_extension->serialize_image_to_bytes(p_state, image, image_dict, _image_format, _lossy_quality);
			} else if (_image_format == "PNG") {
				buffer = image->save_png_to_buffer();
				image_dict["mimeType"] = "image/png";
			} else if (_image_format == "JPEG") {
				buffer = image->save_jpg_to_buffer(_lossy_quality);
				image_dict["mimeType"] = "image/jpeg";
			} else {
				ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "glTF: Unknown image format '" + _image_format + "'.");
			}
			ERR_FAIL_COND_V_MSG(buffer.is_empty(), ERR_INVALID_DATA, "glTF: Failed to save image in '" + _image_format + "' format.");

			bv->byte_length = buffer.size();
			p_state->buffers.write[bi].resize(p_state->buffers[bi].size() + bv->byte_length);
			memcpy(&p_state->buffers.write[bi].write[bv->byte_offset], buffer.ptr(), buffer.size());
			ERR_FAIL_COND_V(bv->byte_offset + bv->byte_length > p_state->buffers[bi].size(), ERR_FILE_CORRUPT);

			p_state->buffer_views.push_back(bv);
			bvi = p_state->buffer_views.size() - 1;
			image_dict["bufferView"] = bvi;
		}
		images.push_back(image_dict);
	}

	print_verbose("Total images: " + itos(p_state->images.size()));

	if (!images.size()) {
		return OK;
	}
	p_state->json["images"] = images;

	return OK;
}

Ref<Image> GLTFDocument::_parse_image_bytes_into_image(Ref<GLTFState> p_state, const Vector<uint8_t> &p_bytes, const String &p_mime_type, int p_index, String &r_file_extension) {
	Ref<Image> r_image;
	r_image.instantiate();
	// Check if any GLTFDocumentExtensions want to import this data as an image.
	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		Error err = ext->parse_image_data(p_state, p_bytes, p_mime_type, r_image);
		ERR_CONTINUE_MSG(err != OK, "glTF: Encountered error " + itos(err) + " when parsing image " + itos(p_index) + " in file " + p_state->filename + ". Continuing.");
		if (!r_image->is_empty()) {
			r_file_extension = ext->get_image_file_extension();
			return r_image;
		}
	}
	// If no extension wanted to import this data as an image, try to load a PNG or JPEG.
	// First we honor the mime types if they were defined.
	if (p_mime_type == "image/png") { // Load buffer as PNG.
		r_image->load_png_from_buffer(p_bytes);
		r_file_extension = ".png";
	} else if (p_mime_type == "image/jpeg") { // Loader buffer as JPEG.
		r_image->load_jpg_from_buffer(p_bytes);
		r_file_extension = ".jpg";
	}
	// If we didn't pass the above tests, we attempt loading as PNG and then JPEG directly.
	// This covers URIs with base64-encoded data with application/* type but
	// no optional mimeType property, or bufferViews with a bogus mimeType
	// (e.g. `image/jpeg` but the data is actually PNG).
	// That's not *exactly* what the spec mandates but this lets us be
	// lenient with bogus glb files which do exist in production.
	if (r_image->is_empty()) { // Try PNG first.
		r_image->load_png_from_buffer(p_bytes);
	}
	if (r_image->is_empty()) { // And then JPEG.
		r_image->load_jpg_from_buffer(p_bytes);
	}
	// If it still can't be loaded, give up and insert an empty image as placeholder.
	if (r_image->is_empty()) {
		ERR_PRINT(vformat("glTF: Couldn't load image index '%d' with its given mimetype: %s.", p_index, p_mime_type));
	}
	return r_image;
}

void GLTFDocument::_parse_image_save_image(Ref<GLTFState> p_state, const Vector<uint8_t> &p_bytes, const String &p_file_extension, int p_index, Ref<Image> p_image) {
	GLTFState::GLTFHandleBinary handling = GLTFState::GLTFHandleBinary(p_state->handle_binary_image);
	if (p_image->is_empty() || handling == GLTFState::GLTFHandleBinary::HANDLE_BINARY_DISCARD_TEXTURES) {
		p_state->images.push_back(Ref<Texture2D>());
		p_state->source_images.push_back(Ref<Image>());
		return;
	}
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && handling == GLTFState::GLTFHandleBinary::HANDLE_BINARY_EXTRACT_TEXTURES) {
		if (p_state->base_path.is_empty()) {
			p_state->images.push_back(Ref<Texture2D>());
			p_state->source_images.push_back(Ref<Image>());
		} else if (p_image->get_name().is_empty()) {
			WARN_PRINT(vformat("glTF: Image index '%d' couldn't be named. Skipping it.", p_index));
			p_state->images.push_back(Ref<Texture2D>());
			p_state->source_images.push_back(Ref<Image>());
		} else {
			bool must_import = true;
			Vector<uint8_t> img_data = p_image->get_data();
			Dictionary generator_parameters;
			String file_path = p_state->get_base_path().path_join(p_state->filename.get_basename() + "_" + p_image->get_name());
			file_path += p_file_extension.is_empty() ? ".png" : p_file_extension;
			if (FileAccess::exists(file_path + ".import")) {
				Ref<ConfigFile> config;
				config.instantiate();
				config->load(file_path + ".import");
				if (config->has_section_key("remap", "generator_parameters")) {
					generator_parameters = (Dictionary)config->get_value("remap", "generator_parameters");
				}
				if (!generator_parameters.has("md5")) {
					must_import = false; // Didn't come from a gltf document; don't overwrite.
				}
			}
			if (must_import) {
				String existing_md5 = generator_parameters["md5"];
				unsigned char md5_hash[16];
				CryptoCore::md5(img_data.ptr(), img_data.size(), md5_hash);
				String new_md5 = String::hex_encode_buffer(md5_hash, 16);
				generator_parameters["md5"] = new_md5;
				if (new_md5 == existing_md5) {
					must_import = false;
				}
			}
			if (must_import) {
				Error err = OK;
				if (p_file_extension.is_empty()) {
					// If a file extension was not specified, save the image data to a PNG file.
					err = p_image->save_png(file_path);
					ERR_FAIL_COND(err != OK);
				} else {
					// If a file extension was specified, save the original bytes to a file with that extension.
					Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::WRITE, &err);
					ERR_FAIL_COND(err != OK);
					file->store_buffer(p_bytes);
					file->close();
				}
				// ResourceLoader::import will crash if not is_editor_hint(), so this case is protected above and will fall through to uncompressed.
				HashMap<StringName, Variant> custom_options;
				custom_options[SNAME("mipmaps/generate")] = true;
				// Will only use project settings defaults if custom_importer is empty.
				EditorFileSystem::get_singleton()->update_file(file_path);
				EditorFileSystem::get_singleton()->reimport_append(file_path, custom_options, String(), generator_parameters);
			}
			Ref<Texture2D> saved_image = ResourceLoader::load(file_path, "Texture2D");
			if (saved_image.is_valid()) {
				p_state->images.push_back(saved_image);
				p_state->source_images.push_back(saved_image->get_image());
			} else {
				WARN_PRINT(vformat("glTF: Image index '%d' couldn't be loaded with the name: %s. Skipping it.", p_index, p_image->get_name()));
				// Placeholder to keep count.
				p_state->images.push_back(Ref<Texture2D>());
				p_state->source_images.push_back(Ref<Image>());
			}
		}
		return;
	}
#endif // TOOLS_ENABLED
	if (handling == GLTFState::GLTFHandleBinary::HANDLE_BINARY_EMBED_AS_BASISU) {
		Ref<PortableCompressedTexture2D> tex;
		tex.instantiate();
		tex->set_name(p_image->get_name());
		tex->set_keep_compressed_buffer(true);
		tex->create_from_image(p_image, PortableCompressedTexture2D::COMPRESSION_MODE_BASIS_UNIVERSAL);
		p_state->images.push_back(tex);
		p_state->source_images.push_back(p_image);
		return;
	}
	// This handles the case of HANDLE_BINARY_EMBED_AS_UNCOMPRESSED, and it also serves
	// as a fallback for HANDLE_BINARY_EXTRACT_TEXTURES when this is not the editor.
	Ref<ImageTexture> tex;
	tex.instantiate();
	tex->set_name(p_image->get_name());
	tex->set_image(p_image);
	p_state->images.push_back(tex);
	p_state->source_images.push_back(p_image);
}

Error GLTFDocument::_parse_images(Ref<GLTFState> p_state, const String &p_base_path) {
	ERR_FAIL_COND_V(p_state.is_null(), ERR_INVALID_PARAMETER);
	if (!p_state->json.has("images")) {
		return OK;
	}

	// Ref: https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md#images

	const Array &images = p_state->json["images"];
	HashSet<String> used_names;
	for (int i = 0; i < images.size(); i++) {
		const Dictionary &dict = images[i];

		// glTF 2.0 supports PNG and JPEG types, which can be specified as (from spec):
		// "- a URI to an external file in one of the supported images formats, or
		//  - a URI with embedded base64-encoded data, or
		//  - a reference to a bufferView; in that case mimeType must be defined."
		// Since mimeType is optional for external files and base64 data, we'll have to
		// fall back on letting Godot parse the data to figure out if it's PNG or JPEG.

		// We'll assume that we use either URI or bufferView, so let's warn the user
		// if their image somehow uses both. And fail if it has neither.
		ERR_CONTINUE_MSG(!dict.has("uri") && !dict.has("bufferView"), "Invalid image definition in glTF file, it should specify an 'uri' or 'bufferView'.");
		if (dict.has("uri") && dict.has("bufferView")) {
			WARN_PRINT("Invalid image definition in glTF file using both 'uri' and 'bufferView'. 'uri' will take precedence.");
		}

		String mime_type;
		if (dict.has("mimeType")) { // Should be "image/png", "image/jpeg", or something handled by an extension.
			mime_type = dict["mimeType"];
		}

		String image_name;
		if (dict.has("name")) {
			image_name = dict["name"];
			image_name = image_name.get_file().get_basename().validate_filename();
		}
		if (image_name.is_empty()) {
			image_name = itos(i);
		}
		while (used_names.has(image_name)) {
			image_name += "_" + itos(i);
		}
		used_names.insert(image_name);
		// Load the image data. If we get a byte array, store here for later.
		Vector<uint8_t> data;
		if (dict.has("uri")) {
			// Handles the first two bullet points from the spec (embedded data, or external file).
			String uri = dict["uri"];
			if (uri.begins_with("data:")) { // Embedded data using base64.
				data = _parse_base64_uri(uri);
				// mimeType is optional, but if we have it defined in the URI, let's use it.
				if (mime_type.is_empty() && uri.contains(";")) {
					// Trim "data:" prefix which is 5 characters long, and end at ";base64".
					mime_type = uri.substr(5, uri.find(";base64") - 5);
				}
			} else { // Relative path to an external image file.
				ERR_FAIL_COND_V(p_base_path.is_empty(), ERR_INVALID_PARAMETER);
				uri = uri.uri_decode();
				uri = p_base_path.path_join(uri).replace("\\", "/"); // Fix for Windows.
				// ResourceLoader will rely on the file extension to use the relevant loader.
				// The spec says that if mimeType is defined, it should take precedence (e.g.
				// there could be a `.png` image which is actually JPEG), but there's no easy
				// API for that in Godot, so we'd have to load as a buffer (i.e. embedded in
				// the material), so we only do that only as fallback.
				Ref<Texture2D> texture = ResourceLoader::load(uri);
				if (texture.is_valid()) {
					p_state->images.push_back(texture);
					p_state->source_images.push_back(texture->get_image());
					continue;
				}
				// mimeType is optional, but if we have it in the file extension, let's use it.
				// If the mimeType does not match with the file extension, either it should be
				// specified in the file, or the GLTFDocumentExtension should handle it.
				if (mime_type.is_empty()) {
					mime_type = "image/" + uri.get_extension();
				}
				// Fallback to loading as byte array. This enables us to support the
				// spec's requirement that we honor mimetype regardless of file URI.
				data = FileAccess::get_file_as_bytes(uri);
				if (data.size() == 0) {
					WARN_PRINT(vformat("glTF: Image index '%d' couldn't be loaded as a buffer of MIME type '%s' from URI: %s because there was no data to load. Skipping it.", i, mime_type, uri));
					p_state->images.push_back(Ref<Texture2D>()); // Placeholder to keep count.
					p_state->source_images.push_back(Ref<Image>());
					continue;
				}
			}
		} else if (dict.has("bufferView")) {
			// Handles the third bullet point from the spec (bufferView).
			ERR_FAIL_COND_V_MSG(mime_type.is_empty(), ERR_FILE_CORRUPT, vformat("glTF: Image index '%d' specifies 'bufferView' but no 'mimeType', which is invalid.", i));
			const GLTFBufferViewIndex bvi = dict["bufferView"];
			ERR_FAIL_INDEX_V(bvi, p_state->buffer_views.size(), ERR_PARAMETER_RANGE_ERROR);
			Ref<GLTFBufferView> bv = p_state->buffer_views[bvi];
			const GLTFBufferIndex bi = bv->buffer;
			ERR_FAIL_INDEX_V(bi, p_state->buffers.size(), ERR_PARAMETER_RANGE_ERROR);
			ERR_FAIL_COND_V(bv->byte_offset + bv->byte_length > p_state->buffers[bi].size(), ERR_FILE_CORRUPT);
			const PackedByteArray &buffer = p_state->buffers[bi];
			data = buffer.slice(bv->byte_offset, bv->byte_offset + bv->byte_length);
		}
		// Done loading the image data bytes. Check that we actually got data to parse.
		// Note: There are paths above that return early, so this point might not be reached.
		if (data.is_empty()) {
			WARN_PRINT(vformat("glTF: Image index '%d' couldn't be loaded, no data found. Skipping it.", i));
			p_state->images.push_back(Ref<Texture2D>()); // Placeholder to keep count.
			p_state->source_images.push_back(Ref<Image>());
			continue;
		}
		// Parse the image data from bytes into an Image resource and save if needed.
		String file_extension;
		Ref<Image> img = _parse_image_bytes_into_image(p_state, data, mime_type, i, file_extension);
		img->set_name(image_name);
		_parse_image_save_image(p_state, data, file_extension, i, img);
	}

	print_verbose("glTF: Total images: " + itos(p_state->images.size()));

	return OK;
}

Error GLTFDocument::_serialize_textures(Ref<GLTFState> p_state) {
	if (!p_state->textures.size()) {
		return OK;
	}

	Array textures;
	for (int32_t i = 0; i < p_state->textures.size(); i++) {
		Dictionary texture_dict;
		Ref<GLTFTexture> gltf_texture = p_state->textures[i];
		if (_image_save_extension.is_valid()) {
			Error err = _image_save_extension->serialize_texture_json(p_state, texture_dict, gltf_texture, _image_format);
			ERR_FAIL_COND_V(err != OK, err);
		} else {
			ERR_CONTINUE(gltf_texture->get_src_image() == -1);
			texture_dict["source"] = gltf_texture->get_src_image();
		}
		GLTFTextureSamplerIndex sampler_index = gltf_texture->get_sampler();
		if (sampler_index != -1) {
			texture_dict["sampler"] = sampler_index;
		}
		textures.push_back(texture_dict);
	}
	p_state->json["textures"] = textures;

	return OK;
}

Error GLTFDocument::_parse_textures(Ref<GLTFState> p_state) {
	if (!p_state->json.has("textures")) {
		return OK;
	}

	const Array &textures = p_state->json["textures"];
	for (GLTFTextureIndex i = 0; i < textures.size(); i++) {
		const Dictionary &texture_dict = textures[i];
		Ref<GLTFTexture> gltf_texture;
		gltf_texture.instantiate();
		// Check if any GLTFDocumentExtensions want to handle this texture JSON.
		for (Ref<GLTFDocumentExtension> ext : document_extensions) {
			ERR_CONTINUE(ext.is_null());
			Error err = ext->parse_texture_json(p_state, texture_dict, gltf_texture);
			ERR_CONTINUE_MSG(err != OK, "glTF: Encountered error " + itos(err) + " when parsing texture JSON " + String(Variant(texture_dict)) + " in file " + p_state->filename + ". Continuing.");
			if (gltf_texture->get_src_image() != -1) {
				break;
			}
		}
		if (gltf_texture->get_src_image() == -1) {
			// No extensions handled it, so use the base glTF source.
			// This may be the fallback, or the only option anyway.
			ERR_FAIL_COND_V(!texture_dict.has("source"), ERR_PARSE_ERROR);
			gltf_texture->set_src_image(texture_dict["source"]);
		}
		if (gltf_texture->get_sampler() == -1 && texture_dict.has("sampler")) {
			gltf_texture->set_sampler(texture_dict["sampler"]);
		}
		p_state->textures.push_back(gltf_texture);
	}

	return OK;
}

GLTFTextureIndex GLTFDocument::_set_texture(Ref<GLTFState> p_state, Ref<Texture2D> p_texture, StandardMaterial3D::TextureFilter p_filter_mode, bool p_repeats) {
	ERR_FAIL_COND_V(p_texture.is_null(), -1);
	Ref<GLTFTexture> gltf_texture;
	gltf_texture.instantiate();
	ERR_FAIL_COND_V(p_texture->get_image().is_null(), -1);
	GLTFImageIndex gltf_src_image_i = p_state->images.size();
	p_state->images.push_back(p_texture);
	p_state->source_images.push_back(p_texture->get_image());
	gltf_texture->set_src_image(gltf_src_image_i);
	gltf_texture->set_sampler(_set_sampler_for_mode(p_state, p_filter_mode, p_repeats));
	GLTFTextureIndex gltf_texture_i = p_state->textures.size();
	p_state->textures.push_back(gltf_texture);
	return gltf_texture_i;
}

Ref<Texture2D> GLTFDocument::_get_texture(Ref<GLTFState> p_state, const GLTFTextureIndex p_texture, int p_texture_types) {
	ERR_FAIL_INDEX_V(p_texture, p_state->textures.size(), Ref<Texture2D>());
	const GLTFImageIndex image = p_state->textures[p_texture]->get_src_image();
	ERR_FAIL_INDEX_V(image, p_state->images.size(), Ref<Texture2D>());
	if (GLTFState::GLTFHandleBinary(p_state->handle_binary_image) == GLTFState::GLTFHandleBinary::HANDLE_BINARY_EMBED_AS_BASISU) {
		ERR_FAIL_INDEX_V(image, p_state->source_images.size(), Ref<Texture2D>());
		Ref<PortableCompressedTexture2D> portable_texture;
		portable_texture.instantiate();
		portable_texture->set_keep_compressed_buffer(true);
		Ref<Image> new_img = p_state->source_images[image]->duplicate();
		ERR_FAIL_COND_V(new_img.is_null(), Ref<Texture2D>());
		new_img->generate_mipmaps();
		if (p_texture_types) {
			portable_texture->create_from_image(new_img, PortableCompressedTexture2D::COMPRESSION_MODE_BASIS_UNIVERSAL, true);
		} else {
			portable_texture->create_from_image(new_img, PortableCompressedTexture2D::COMPRESSION_MODE_BASIS_UNIVERSAL, false);
		}
		p_state->images.write[image] = portable_texture;
		p_state->source_images.write[image] = new_img;
	}
	return p_state->images[image];
}

GLTFTextureSamplerIndex GLTFDocument::_set_sampler_for_mode(Ref<GLTFState> p_state, StandardMaterial3D::TextureFilter p_filter_mode, bool p_repeats) {
	for (int i = 0; i < p_state->texture_samplers.size(); ++i) {
		if (p_state->texture_samplers[i]->get_filter_mode() == p_filter_mode) {
			return i;
		}
	}

	GLTFTextureSamplerIndex gltf_sampler_i = p_state->texture_samplers.size();
	Ref<GLTFTextureSampler> gltf_sampler;
	gltf_sampler.instantiate();
	gltf_sampler->set_filter_mode(p_filter_mode);
	gltf_sampler->set_wrap_mode(p_repeats);
	p_state->texture_samplers.push_back(gltf_sampler);
	return gltf_sampler_i;
}

Ref<GLTFTextureSampler> GLTFDocument::_get_sampler_for_texture(Ref<GLTFState> p_state, const GLTFTextureIndex p_texture) {
	ERR_FAIL_INDEX_V(p_texture, p_state->textures.size(), Ref<Texture2D>());
	const GLTFTextureSamplerIndex sampler = p_state->textures[p_texture]->get_sampler();

	if (sampler == -1) {
		return p_state->default_texture_sampler;
	} else {
		ERR_FAIL_INDEX_V(sampler, p_state->texture_samplers.size(), Ref<GLTFTextureSampler>());

		return p_state->texture_samplers[sampler];
	}
}

Error GLTFDocument::_serialize_texture_samplers(Ref<GLTFState> p_state) {
	if (!p_state->texture_samplers.size()) {
		return OK;
	}

	Array samplers;
	for (int32_t i = 0; i < p_state->texture_samplers.size(); ++i) {
		Dictionary d;
		Ref<GLTFTextureSampler> s = p_state->texture_samplers[i];
		d["magFilter"] = s->get_mag_filter();
		d["minFilter"] = s->get_min_filter();
		d["wrapS"] = s->get_wrap_s();
		d["wrapT"] = s->get_wrap_t();
		samplers.push_back(d);
	}
	p_state->json["samplers"] = samplers;

	return OK;
}

Error GLTFDocument::_parse_texture_samplers(Ref<GLTFState> p_state) {
	p_state->default_texture_sampler.instantiate();
	p_state->default_texture_sampler->set_min_filter(GLTFTextureSampler::FilterMode::LINEAR_MIPMAP_LINEAR);
	p_state->default_texture_sampler->set_mag_filter(GLTFTextureSampler::FilterMode::LINEAR);
	p_state->default_texture_sampler->set_wrap_s(GLTFTextureSampler::WrapMode::REPEAT);
	p_state->default_texture_sampler->set_wrap_t(GLTFTextureSampler::WrapMode::REPEAT);

	if (!p_state->json.has("samplers")) {
		return OK;
	}

	const Array &samplers = p_state->json["samplers"];
	for (int i = 0; i < samplers.size(); ++i) {
		const Dictionary &d = samplers[i];

		Ref<GLTFTextureSampler> sampler;
		sampler.instantiate();

		if (d.has("minFilter")) {
			sampler->set_min_filter(d["minFilter"]);
		} else {
			sampler->set_min_filter(GLTFTextureSampler::FilterMode::LINEAR_MIPMAP_LINEAR);
		}
		if (d.has("magFilter")) {
			sampler->set_mag_filter(d["magFilter"]);
		} else {
			sampler->set_mag_filter(GLTFTextureSampler::FilterMode::LINEAR);
		}

		if (d.has("wrapS")) {
			sampler->set_wrap_s(d["wrapS"]);
		} else {
			sampler->set_wrap_s(GLTFTextureSampler::WrapMode::DEFAULT);
		}

		if (d.has("wrapT")) {
			sampler->set_wrap_t(d["wrapT"]);
		} else {
			sampler->set_wrap_t(GLTFTextureSampler::WrapMode::DEFAULT);
		}

		p_state->texture_samplers.push_back(sampler);
	}

	return OK;
}

Error GLTFDocument::_serialize_materials(Ref<GLTFState> p_state) {
	Array materials;
	for (int32_t i = 0; i < p_state->materials.size(); i++) {
		Dictionary d;
		Ref<Material> material = p_state->materials[i];
		if (material.is_null()) {
			materials.push_back(d);
			continue;
		}
		if (!material->get_name().is_empty()) {
			d["name"] = _gen_unique_name(p_state, material->get_name());
		}

		Ref<BaseMaterial3D> base_material = material;
		if (base_material.is_null()) {
			materials.push_back(d);
			continue;
		}

		Dictionary mr;
		{
			Array arr;
			const Color c = base_material->get_albedo().srgb_to_linear();
			arr.push_back(c.r);
			arr.push_back(c.g);
			arr.push_back(c.b);
			arr.push_back(c.a);
			mr["baseColorFactor"] = arr;
		}
		if (_image_format != "None") {
			Dictionary bct;
			Ref<Texture2D> albedo_texture = base_material->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
			GLTFTextureIndex gltf_texture_index = -1;

			if (albedo_texture.is_valid() && albedo_texture->get_image().is_valid()) {
				albedo_texture->set_name(material->get_name() + "_albedo");
				gltf_texture_index = _set_texture(p_state, albedo_texture, base_material->get_texture_filter(), base_material->get_flag(BaseMaterial3D::FLAG_USE_TEXTURE_REPEAT));
			}
			if (gltf_texture_index != -1) {
				bct["index"] = gltf_texture_index;
				Dictionary extensions = _serialize_texture_transform_uv1(material);
				if (!extensions.is_empty()) {
					bct["extensions"] = extensions;
					p_state->use_khr_texture_transform = true;
				}
				mr["baseColorTexture"] = bct;
			}
		}

		mr["metallicFactor"] = base_material->get_metallic();
		mr["roughnessFactor"] = base_material->get_roughness();
		if (_image_format != "None") {
			bool has_roughness = base_material->get_texture(BaseMaterial3D::TEXTURE_ROUGHNESS).is_valid() && base_material->get_texture(BaseMaterial3D::TEXTURE_ROUGHNESS)->get_image().is_valid();
			bool has_ao = base_material->get_feature(BaseMaterial3D::FEATURE_AMBIENT_OCCLUSION) && base_material->get_texture(BaseMaterial3D::TEXTURE_AMBIENT_OCCLUSION).is_valid();
			bool has_metalness = base_material->get_texture(BaseMaterial3D::TEXTURE_METALLIC).is_valid() && base_material->get_texture(BaseMaterial3D::TEXTURE_METALLIC)->get_image().is_valid();
			if (has_ao || has_roughness || has_metalness) {
				Dictionary mrt;
				Ref<Texture2D> roughness_texture = base_material->get_texture(BaseMaterial3D::TEXTURE_ROUGHNESS);
				BaseMaterial3D::TextureChannel roughness_channel = base_material->get_roughness_texture_channel();
				Ref<Texture2D> metallic_texture = base_material->get_texture(BaseMaterial3D::TEXTURE_METALLIC);
				BaseMaterial3D::TextureChannel metalness_channel = base_material->get_metallic_texture_channel();
				Ref<Texture2D> ao_texture = base_material->get_texture(BaseMaterial3D::TEXTURE_AMBIENT_OCCLUSION);
				BaseMaterial3D::TextureChannel ao_channel = base_material->get_ao_texture_channel();
				Ref<ImageTexture> orm_texture;
				orm_texture.instantiate();
				Ref<Image> orm_image;
				orm_image.instantiate();
				int32_t height = 0;
				int32_t width = 0;
				Ref<Image> ao_image;
				if (has_ao) {
					height = ao_texture->get_height();
					width = ao_texture->get_width();
					ao_image = ao_texture->get_image();
					Ref<ImageTexture> img_tex = ao_image;
					if (img_tex.is_valid()) {
						ao_image = img_tex->get_image();
					}
					if (ao_image->is_compressed()) {
						ao_image->decompress();
					}
				}
				Ref<Image> roughness_image;
				if (has_roughness) {
					height = roughness_texture->get_height();
					width = roughness_texture->get_width();
					roughness_image = roughness_texture->get_image();
					Ref<ImageTexture> img_tex = roughness_image;
					if (img_tex.is_valid()) {
						roughness_image = img_tex->get_image();
					}
					if (roughness_image->is_compressed()) {
						roughness_image->decompress();
					}
				}
				Ref<Image> metallness_image;
				if (has_metalness) {
					height = metallic_texture->get_height();
					width = metallic_texture->get_width();
					metallness_image = metallic_texture->get_image();
					Ref<ImageTexture> img_tex = metallness_image;
					if (img_tex.is_valid()) {
						metallness_image = img_tex->get_image();
					}
					if (metallness_image->is_compressed()) {
						metallness_image->decompress();
					}
				}
				Ref<Texture2D> albedo_texture = base_material->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
				if (albedo_texture.is_valid() && albedo_texture->get_image().is_valid()) {
					height = albedo_texture->get_height();
					width = albedo_texture->get_width();
				}
				orm_image->initialize_data(width, height, false, Image::FORMAT_RGBA8);
				if (ao_image.is_valid() && ao_image->get_size() != Vector2(width, height)) {
					ao_image->resize(width, height, Image::INTERPOLATE_LANCZOS);
				}
				if (roughness_image.is_valid() && roughness_image->get_size() != Vector2(width, height)) {
					roughness_image->resize(width, height, Image::INTERPOLATE_LANCZOS);
				}
				if (metallness_image.is_valid() && metallness_image->get_size() != Vector2(width, height)) {
					metallness_image->resize(width, height, Image::INTERPOLATE_LANCZOS);
				}
				for (int32_t h = 0; h < height; h++) {
					for (int32_t w = 0; w < width; w++) {
						Color c = Color(1.0f, 1.0f, 1.0f);
						if (has_ao) {
							if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_RED == ao_channel) {
								c.r = ao_image->get_pixel(w, h).r;
							} else if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_GREEN == ao_channel) {
								c.r = ao_image->get_pixel(w, h).g;
							} else if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_BLUE == ao_channel) {
								c.r = ao_image->get_pixel(w, h).b;
							} else if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_ALPHA == ao_channel) {
								c.r = ao_image->get_pixel(w, h).a;
							}
						}
						if (has_roughness) {
							if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_RED == roughness_channel) {
								c.g = roughness_image->get_pixel(w, h).r;
							} else if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_GREEN == roughness_channel) {
								c.g = roughness_image->get_pixel(w, h).g;
							} else if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_BLUE == roughness_channel) {
								c.g = roughness_image->get_pixel(w, h).b;
							} else if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_ALPHA == roughness_channel) {
								c.g = roughness_image->get_pixel(w, h).a;
							}
						}
						if (has_metalness) {
							if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_RED == metalness_channel) {
								c.b = metallness_image->get_pixel(w, h).r;
							} else if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_GREEN == metalness_channel) {
								c.b = metallness_image->get_pixel(w, h).g;
							} else if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_BLUE == metalness_channel) {
								c.b = metallness_image->get_pixel(w, h).b;
							} else if (BaseMaterial3D::TextureChannel::TEXTURE_CHANNEL_ALPHA == metalness_channel) {
								c.b = metallness_image->get_pixel(w, h).a;
							}
						}
						orm_image->set_pixel(w, h, c);
					}
				}
				orm_image->generate_mipmaps();
				orm_texture->set_image(orm_image);
				GLTFTextureIndex orm_texture_index = -1;
				if (has_ao || has_roughness || has_metalness) {
					orm_texture->set_name(material->get_name() + "_orm");
					orm_texture_index = _set_texture(p_state, orm_texture, base_material->get_texture_filter(), base_material->get_flag(BaseMaterial3D::FLAG_USE_TEXTURE_REPEAT));
				}
				if (has_ao) {
					Dictionary occt;
					occt["index"] = orm_texture_index;
					d["occlusionTexture"] = occt;
				}
				if (has_roughness || has_metalness) {
					mrt["index"] = orm_texture_index;
					Dictionary extensions = _serialize_texture_transform_uv1(material);
					if (!extensions.is_empty()) {
						mrt["extensions"] = extensions;
						p_state->use_khr_texture_transform = true;
					}
					mr["metallicRoughnessTexture"] = mrt;
				}
			}
		}

		d["pbrMetallicRoughness"] = mr;
		if (base_material->get_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING) && _image_format != "None") {
			Dictionary nt;
			Ref<ImageTexture> tex;
			tex.instantiate();
			{
				Ref<Texture2D> normal_texture = base_material->get_texture(BaseMaterial3D::TEXTURE_NORMAL);
				if (normal_texture.is_valid()) {
					// Code for uncompressing RG normal maps
					Ref<Image> img = normal_texture->get_image();
					if (img.is_valid()) {
						Ref<ImageTexture> img_tex = img;
						if (img_tex.is_valid()) {
							img = img_tex->get_image();
						}
						img->decompress();
						img->convert(Image::FORMAT_RGBA8);
						for (int32_t y = 0; y < img->get_height(); y++) {
							for (int32_t x = 0; x < img->get_width(); x++) {
								Color c = img->get_pixel(x, y);
								Vector2 red_green = Vector2(c.r, c.g);
								red_green = red_green * Vector2(2.0f, 2.0f) - Vector2(1.0f, 1.0f);
								float blue = 1.0f - red_green.dot(red_green);
								blue = MAX(0.0f, blue);
								c.b = Math::sqrt(blue);
								img->set_pixel(x, y, c);
							}
						}
						tex->set_image(img);
					}
				}
			}
			GLTFTextureIndex gltf_texture_index = -1;
			if (tex.is_valid() && tex->get_image().is_valid()) {
				tex->set_name(material->get_name() + "_normal");
				gltf_texture_index = _set_texture(p_state, tex, base_material->get_texture_filter(), base_material->get_flag(BaseMaterial3D::FLAG_USE_TEXTURE_REPEAT));
			}
			nt["scale"] = base_material->get_normal_scale();
			if (gltf_texture_index != -1) {
				nt["index"] = gltf_texture_index;
				d["normalTexture"] = nt;
			}
		}

		if (base_material->get_feature(BaseMaterial3D::FEATURE_EMISSION)) {
			const Color c = base_material->get_emission().linear_to_srgb();
			Array arr;
			arr.push_back(c.r);
			arr.push_back(c.g);
			arr.push_back(c.b);
			d["emissiveFactor"] = arr;
		}

		if (base_material->get_feature(BaseMaterial3D::FEATURE_EMISSION) && _image_format != "None") {
			Dictionary et;
			Ref<Texture2D> emission_texture = base_material->get_texture(BaseMaterial3D::TEXTURE_EMISSION);
			GLTFTextureIndex gltf_texture_index = -1;
			if (emission_texture.is_valid() && emission_texture->get_image().is_valid()) {
				emission_texture->set_name(material->get_name() + "_emission");
				gltf_texture_index = _set_texture(p_state, emission_texture, base_material->get_texture_filter(), base_material->get_flag(BaseMaterial3D::FLAG_USE_TEXTURE_REPEAT));
			}

			if (gltf_texture_index != -1) {
				et["index"] = gltf_texture_index;
				d["emissiveTexture"] = et;
			}
		}

		const bool ds = base_material->get_cull_mode() == BaseMaterial3D::CULL_DISABLED;
		if (ds) {
			d["doubleSided"] = ds;
		}

		if (base_material->get_transparency() == BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR) {
			d["alphaMode"] = "MASK";
			d["alphaCutoff"] = base_material->get_alpha_scissor_threshold();
		} else if (base_material->get_transparency() != BaseMaterial3D::TRANSPARENCY_DISABLED) {
			d["alphaMode"] = "BLEND";
		}

		Dictionary extensions;
		if (base_material->get_shading_mode() == BaseMaterial3D::SHADING_MODE_UNSHADED) {
			Dictionary mat_unlit;
			extensions["KHR_materials_unlit"] = mat_unlit;
			p_state->add_used_extension("KHR_materials_unlit");
		}
		if (base_material->get_feature(BaseMaterial3D::FEATURE_EMISSION) && !Math::is_equal_approx(base_material->get_emission_energy_multiplier(), 1.0f)) {
			Dictionary mat_emissive_strength;
			mat_emissive_strength["emissiveStrength"] = base_material->get_emission_energy_multiplier();
			extensions["KHR_materials_emissive_strength"] = mat_emissive_strength;
			p_state->add_used_extension("KHR_materials_emissive_strength");
		}
		d["extensions"] = extensions;

		_attach_meta_to_extras(material, d);
		materials.push_back(d);
	}
	if (!materials.size()) {
		return OK;
	}
	p_state->json["materials"] = materials;
	print_verbose("Total materials: " + itos(p_state->materials.size()));

	return OK;
}

Error GLTFDocument::_parse_materials(Ref<GLTFState> p_state) {
	if (!p_state->json.has("materials")) {
		return OK;
	}

	const Array &materials = p_state->json["materials"];
	for (GLTFMaterialIndex i = 0; i < materials.size(); i++) {
		const Dictionary &material_dict = materials[i];

		Ref<StandardMaterial3D> material;
		material.instantiate();
		if (material_dict.has("name") && !String(material_dict["name"]).is_empty()) {
			material->set_name(material_dict["name"]);
		} else {
			material->set_name(vformat("material_%s", itos(i)));
		}
		Dictionary material_extensions;
		if (material_dict.has("extensions")) {
			material_extensions = material_dict["extensions"];
		}

		if (material_extensions.has("KHR_materials_unlit")) {
			material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
		}

		if (material_extensions.has("KHR_materials_emissive_strength")) {
			Dictionary emissive_strength = material_extensions["KHR_materials_emissive_strength"];
			if (emissive_strength.has("emissiveStrength")) {
				material->set_emission_energy_multiplier(emissive_strength["emissiveStrength"]);
			}
		}

		if (material_extensions.has("KHR_materials_pbrSpecularGlossiness")) {
			WARN_PRINT("Material uses a specular and glossiness workflow. Textures will be converted to roughness and metallic workflow, which may not be 100% accurate.");
			Dictionary sgm = material_extensions["KHR_materials_pbrSpecularGlossiness"];

			Ref<GLTFSpecGloss> spec_gloss;
			spec_gloss.instantiate();
			if (sgm.has("diffuseTexture")) {
				const Dictionary &diffuse_texture_dict = sgm["diffuseTexture"];
				if (diffuse_texture_dict.has("index")) {
					Ref<GLTFTextureSampler> diffuse_sampler = _get_sampler_for_texture(p_state, diffuse_texture_dict["index"]);
					if (diffuse_sampler.is_valid()) {
						material->set_texture_filter(diffuse_sampler->get_filter_mode());
						material->set_flag(BaseMaterial3D::FLAG_USE_TEXTURE_REPEAT, diffuse_sampler->get_wrap_mode());
					}
					Ref<Texture2D> diffuse_texture = _get_texture(p_state, diffuse_texture_dict["index"], TEXTURE_TYPE_GENERIC);
					if (diffuse_texture.is_valid()) {
						spec_gloss->diffuse_img = diffuse_texture->get_image();
						material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, diffuse_texture);
					}
				}
			}
			if (sgm.has("diffuseFactor")) {
				const Array &arr = sgm["diffuseFactor"];
				ERR_FAIL_COND_V(arr.size() != 4, ERR_PARSE_ERROR);
				const Color c = Color(arr[0], arr[1], arr[2], arr[3]).linear_to_srgb();
				spec_gloss->diffuse_factor = c;
				material->set_albedo(spec_gloss->diffuse_factor);
			}

			if (sgm.has("specularFactor")) {
				const Array &arr = sgm["specularFactor"];
				ERR_FAIL_COND_V(arr.size() != 3, ERR_PARSE_ERROR);
				spec_gloss->specular_factor = Color(arr[0], arr[1], arr[2]);
			}

			if (sgm.has("glossinessFactor")) {
				spec_gloss->gloss_factor = sgm["glossinessFactor"];
				material->set_roughness(1.0f - CLAMP(spec_gloss->gloss_factor, 0.0f, 1.0f));
			}
			if (sgm.has("specularGlossinessTexture")) {
				const Dictionary &spec_gloss_texture = sgm["specularGlossinessTexture"];
				if (spec_gloss_texture.has("index")) {
					const Ref<Texture2D> orig_texture = _get_texture(p_state, spec_gloss_texture["index"], TEXTURE_TYPE_GENERIC);
					if (orig_texture.is_valid()) {
						spec_gloss->spec_gloss_img = orig_texture->get_image();
					}
				}
			}
			spec_gloss_to_rough_metal(spec_gloss, material);

		} else if (material_dict.has("pbrMetallicRoughness")) {
			const Dictionary &mr = material_dict["pbrMetallicRoughness"];
			if (mr.has("baseColorFactor")) {
				const Array &arr = mr["baseColorFactor"];
				ERR_FAIL_COND_V(arr.size() != 4, ERR_PARSE_ERROR);
				const Color c = Color(arr[0], arr[1], arr[2], arr[3]).linear_to_srgb();
				material->set_albedo(c);
			}

			if (mr.has("baseColorTexture")) {
				const Dictionary &bct = mr["baseColorTexture"];
				if (bct.has("index")) {
					Ref<GLTFTextureSampler> bct_sampler = _get_sampler_for_texture(p_state, bct["index"]);
					material->set_texture_filter(bct_sampler->get_filter_mode());
					material->set_flag(BaseMaterial3D::FLAG_USE_TEXTURE_REPEAT, bct_sampler->get_wrap_mode());
					material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, _get_texture(p_state, bct["index"], TEXTURE_TYPE_GENERIC));
				}
				if (!mr.has("baseColorFactor")) {
					material->set_albedo(Color(1, 1, 1));
				}
				_set_texture_transform_uv1(bct, material);
			}

			if (mr.has("metallicFactor")) {
				material->set_metallic(mr["metallicFactor"]);
			} else {
				material->set_metallic(1.0);
			}

			if (mr.has("roughnessFactor")) {
				material->set_roughness(mr["roughnessFactor"]);
			} else {
				material->set_roughness(1.0);
			}

			if (mr.has("metallicRoughnessTexture")) {
				const Dictionary &bct = mr["metallicRoughnessTexture"];
				if (bct.has("index")) {
					const Ref<Texture2D> t = _get_texture(p_state, bct["index"], TEXTURE_TYPE_GENERIC);
					material->set_texture(BaseMaterial3D::TEXTURE_METALLIC, t);
					material->set_metallic_texture_channel(BaseMaterial3D::TEXTURE_CHANNEL_BLUE);
					material->set_texture(BaseMaterial3D::TEXTURE_ROUGHNESS, t);
					material->set_roughness_texture_channel(BaseMaterial3D::TEXTURE_CHANNEL_GREEN);
					if (!mr.has("metallicFactor")) {
						material->set_metallic(1);
					}
					if (!mr.has("roughnessFactor")) {
						material->set_roughness(1);
					}
				}
			}
		}

		if (material_dict.has("normalTexture")) {
			const Dictionary &bct = material_dict["normalTexture"];
			if (bct.has("index")) {
				material->set_texture(BaseMaterial3D::TEXTURE_NORMAL, _get_texture(p_state, bct["index"], TEXTURE_TYPE_NORMAL));
				material->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
			}
			if (bct.has("scale")) {
				material->set_normal_scale(bct["scale"]);
			}
		}
		if (material_dict.has("occlusionTexture")) {
			const Dictionary &bct = material_dict["occlusionTexture"];
			if (bct.has("index")) {
				material->set_texture(BaseMaterial3D::TEXTURE_AMBIENT_OCCLUSION, _get_texture(p_state, bct["index"], TEXTURE_TYPE_GENERIC));
				material->set_ao_texture_channel(BaseMaterial3D::TEXTURE_CHANNEL_RED);
				material->set_feature(BaseMaterial3D::FEATURE_AMBIENT_OCCLUSION, true);
			}
		}

		if (material_dict.has("emissiveFactor")) {
			const Array &arr = material_dict["emissiveFactor"];
			ERR_FAIL_COND_V(arr.size() != 3, ERR_PARSE_ERROR);
			const Color c = Color(arr[0], arr[1], arr[2]).linear_to_srgb();
			material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);

			material->set_emission(c);
		}

		if (material_dict.has("emissiveTexture")) {
			const Dictionary &bct = material_dict["emissiveTexture"];
			if (bct.has("index")) {
				material->set_texture(BaseMaterial3D::TEXTURE_EMISSION, _get_texture(p_state, bct["index"], TEXTURE_TYPE_GENERIC));
				material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
				material->set_emission(Color(0, 0, 0));
			}
		}

		if (material_dict.has("doubleSided")) {
			const bool ds = material_dict["doubleSided"];
			if (ds) {
				material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
			}
		}
		if (material_dict.has("alphaMode")) {
			const String &am = material_dict["alphaMode"];
			if (am == "BLEND") {
				material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
			} else if (am == "MASK") {
				material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
				if (material_dict.has("alphaCutoff")) {
					material->set_alpha_scissor_threshold(material_dict["alphaCutoff"]);
				} else {
					material->set_alpha_scissor_threshold(0.5f);
				}
			}
		}

		if (material_dict.has("extras")) {
			_attach_extras_to_meta(material_dict["extras"], material);
		}
		p_state->materials.push_back(material);
	}

	print_verbose("Total materials: " + itos(p_state->materials.size()));

	return OK;
}

void GLTFDocument::_set_texture_transform_uv1(const Dictionary &p_dict, Ref<BaseMaterial3D> p_material) {
	if (p_dict.has("extensions")) {
		const Dictionary &extensions = p_dict["extensions"];
		if (extensions.has("KHR_texture_transform")) {
			if (p_material.is_valid()) {
				const Dictionary &texture_transform = extensions["KHR_texture_transform"];
				const Array &offset_arr = texture_transform["offset"];
				if (offset_arr.size() == 2) {
					const Vector3 offset_vector3 = Vector3(offset_arr[0], offset_arr[1], 0.0f);
					p_material->set_uv1_offset(offset_vector3);
				}

				const Array &scale_arr = texture_transform["scale"];
				if (scale_arr.size() == 2) {
					const Vector3 scale_vector3 = Vector3(scale_arr[0], scale_arr[1], 1.0f);
					p_material->set_uv1_scale(scale_vector3);
				}
			}
		}
	}
}

void GLTFDocument::spec_gloss_to_rough_metal(Ref<GLTFSpecGloss> r_spec_gloss, Ref<BaseMaterial3D> p_material) {
	if (r_spec_gloss.is_null()) {
		return;
	}
	if (r_spec_gloss->spec_gloss_img.is_null()) {
		return;
	}
	if (r_spec_gloss->diffuse_img.is_null()) {
		return;
	}
	if (p_material.is_null()) {
		return;
	}
	bool has_roughness = false;
	bool has_metal = false;
	p_material->set_roughness(1.0f);
	p_material->set_metallic(1.0f);
	Ref<Image> rm_img = Image::create_empty(r_spec_gloss->spec_gloss_img->get_width(), r_spec_gloss->spec_gloss_img->get_height(), false, Image::FORMAT_RGBA8);
	r_spec_gloss->spec_gloss_img->decompress();
	if (r_spec_gloss->diffuse_img.is_valid()) {
		r_spec_gloss->diffuse_img->decompress();
		r_spec_gloss->diffuse_img->resize(r_spec_gloss->spec_gloss_img->get_width(), r_spec_gloss->spec_gloss_img->get_height(), Image::INTERPOLATE_LANCZOS);
		r_spec_gloss->spec_gloss_img->resize(r_spec_gloss->diffuse_img->get_width(), r_spec_gloss->diffuse_img->get_height(), Image::INTERPOLATE_LANCZOS);
	}
	for (int32_t y = 0; y < r_spec_gloss->spec_gloss_img->get_height(); y++) {
		for (int32_t x = 0; x < r_spec_gloss->spec_gloss_img->get_width(); x++) {
			const Color specular_pixel = r_spec_gloss->spec_gloss_img->get_pixel(x, y).srgb_to_linear();
			Color specular = Color(specular_pixel.r, specular_pixel.g, specular_pixel.b);
			specular *= r_spec_gloss->specular_factor;
			Color diffuse = Color(1.0f, 1.0f, 1.0f);
			diffuse *= r_spec_gloss->diffuse_img->get_pixel(x, y).srgb_to_linear();
			float metallic = 0.0f;
			Color base_color;
			spec_gloss_to_metal_base_color(specular, diffuse, base_color, metallic);
			Color mr = Color(1.0f, 1.0f, 1.0f);
			mr.g = specular_pixel.a;
			mr.b = metallic;
			if (!Math::is_equal_approx(mr.g, 1.0f)) {
				has_roughness = true;
			}
			if (!Math::is_zero_approx(mr.b)) {
				has_metal = true;
			}
			mr.g *= r_spec_gloss->gloss_factor;
			mr.g = 1.0f - mr.g;
			rm_img->set_pixel(x, y, mr);
			if (r_spec_gloss->diffuse_img.is_valid()) {
				r_spec_gloss->diffuse_img->set_pixel(x, y, base_color.linear_to_srgb());
			}
		}
	}
	rm_img->generate_mipmaps();
	r_spec_gloss->diffuse_img->generate_mipmaps();
	p_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, ImageTexture::create_from_image(r_spec_gloss->diffuse_img));
	Ref<ImageTexture> rm_image_texture = ImageTexture::create_from_image(rm_img);
	if (has_roughness) {
		p_material->set_texture(BaseMaterial3D::TEXTURE_ROUGHNESS, rm_image_texture);
		p_material->set_roughness_texture_channel(BaseMaterial3D::TEXTURE_CHANNEL_GREEN);
	}

	if (has_metal) {
		p_material->set_texture(BaseMaterial3D::TEXTURE_METALLIC, rm_image_texture);
		p_material->set_metallic_texture_channel(BaseMaterial3D::TEXTURE_CHANNEL_BLUE);
	}
}

void GLTFDocument::spec_gloss_to_metal_base_color(const Color &p_specular_factor, const Color &p_diffuse, Color &r_base_color, float &r_metallic) {
	const Color DIELECTRIC_SPECULAR = Color(0.04f, 0.04f, 0.04f);
	Color specular = Color(p_specular_factor.r, p_specular_factor.g, p_specular_factor.b);
	const float one_minus_specular_strength = 1.0f - get_max_component(specular);
	const float dielectric_specular_red = DIELECTRIC_SPECULAR.r;
	float brightness_diffuse = get_perceived_brightness(p_diffuse);
	const float brightness_specular = get_perceived_brightness(specular);
	r_metallic = solve_metallic(dielectric_specular_red, brightness_diffuse, brightness_specular, one_minus_specular_strength);
	const float one_minus_metallic = 1.0f - r_metallic;
	const Color base_color_from_diffuse = p_diffuse * (one_minus_specular_strength / (1.0f - dielectric_specular_red) / MAX(one_minus_metallic, CMP_EPSILON));
	const Color base_color_from_specular = (specular - (DIELECTRIC_SPECULAR * (one_minus_metallic))) * (1.0f / MAX(r_metallic, CMP_EPSILON));
	r_base_color.r = Math::lerp(base_color_from_diffuse.r, base_color_from_specular.r, r_metallic * r_metallic);
	r_base_color.g = Math::lerp(base_color_from_diffuse.g, base_color_from_specular.g, r_metallic * r_metallic);
	r_base_color.b = Math::lerp(base_color_from_diffuse.b, base_color_from_specular.b, r_metallic * r_metallic);
	r_base_color.a = p_diffuse.a;
	r_base_color = r_base_color.clamp();
}
Error GLTFDocument::_parse_skins(Ref<GLTFState> p_state) {
	if (!p_state->json.has("skins")) {
		return OK;
	}

	const Array &skins = p_state->json["skins"];

	// Create the base skins, and mark nodes that are joints
	for (int i = 0; i < skins.size(); i++) {
		const Dictionary &d = skins[i];

		Ref<GLTFSkin> skin;
		skin.instantiate();

		ERR_FAIL_COND_V(!d.has("joints"), ERR_PARSE_ERROR);

		const Array &joints = d["joints"];

		if (d.has("inverseBindMatrices")) {
			skin->inverse_binds = _decode_accessor_as_xform(p_state, d["inverseBindMatrices"], false);
			ERR_FAIL_COND_V(skin->inverse_binds.size() != joints.size(), ERR_PARSE_ERROR);
		}

		for (int j = 0; j < joints.size(); j++) {
			const GLTFNodeIndex node = joints[j];
			ERR_FAIL_INDEX_V(node, p_state->nodes.size(), ERR_PARSE_ERROR);

			skin->joints.push_back(node);
			skin->joints_original.push_back(node);

			p_state->nodes.write[node]->joint = true;
		}

		if (d.has("name") && !String(d["name"]).is_empty()) {
			skin->set_name(d["name"]);
		} else {
			skin->set_name(vformat("skin_%s", itos(i)));
		}

		if (d.has("skeleton")) {
			skin->skin_root = d["skeleton"];
		}

		p_state->skins.push_back(skin);
	}

	for (GLTFSkinIndex i = 0; i < p_state->skins.size(); ++i) {
		Ref<GLTFSkin> skin = p_state->skins.write[i];

		// Expand the skin to capture all the extra non-joints that lie in between the actual joints,
		// and expand the hierarchy to ensure multi-rooted trees lie on the same height level
		ERR_FAIL_COND_V(SkinTool::_expand_skin(p_state->nodes, skin), ERR_PARSE_ERROR);
		ERR_FAIL_COND_V(SkinTool::_verify_skin(p_state->nodes, skin), ERR_PARSE_ERROR);
	}

	print_verbose("glTF: Total skins: " + itos(p_state->skins.size()));

	return OK;
}
Error GLTFDocument::_serialize_skins(Ref<GLTFState> p_state) {
	_remove_duplicate_skins(p_state);
	Array json_skins;
	for (int skin_i = 0; skin_i < p_state->skins.size(); skin_i++) {
		Ref<GLTFSkin> gltf_skin = p_state->skins[skin_i];
		Dictionary json_skin;
		json_skin["inverseBindMatrices"] = _encode_accessor_as_xform(p_state, gltf_skin->inverse_binds, false);
		json_skin["joints"] = gltf_skin->get_joints();
		json_skin["name"] = gltf_skin->get_name();
		json_skins.push_back(json_skin);
	}
	if (!p_state->skins.size()) {
		return OK;
	}

	p_state->json["skins"] = json_skins;
	return OK;
}

Error GLTFDocument::_create_skins(Ref<GLTFState> p_state) {
	for (GLTFSkinIndex skin_i = 0; skin_i < p_state->skins.size(); ++skin_i) {
		Ref<GLTFSkin> gltf_skin = p_state->skins.write[skin_i];

		Ref<Skin> skin;
		skin.instantiate();

		// Some skins don't have IBM's! What absolute monsters!
		const bool has_ibms = !gltf_skin->inverse_binds.is_empty();

		for (int joint_i = 0; joint_i < gltf_skin->joints_original.size(); ++joint_i) {
			GLTFNodeIndex node = gltf_skin->joints_original[joint_i];
			String bone_name = p_state->nodes[node]->get_name();

			Transform3D xform;
			if (has_ibms) {
				xform = gltf_skin->inverse_binds[joint_i];
			}

			if (p_state->use_named_skin_binds) {
				skin->add_named_bind(bone_name, xform);
			} else {
				int32_t bone_i = gltf_skin->joint_i_to_bone_i[joint_i];
				skin->add_bind(bone_i, xform);
			}
		}

		gltf_skin->godot_skin = skin;
	}

	// Purge the duplicates!
	_remove_duplicate_skins(p_state);

	// Create unique names now, after removing duplicates
	for (GLTFSkinIndex skin_i = 0; skin_i < p_state->skins.size(); ++skin_i) {
		Ref<Skin> skin = p_state->skins.write[skin_i]->godot_skin;
		if (skin->get_name().is_empty()) {
			// Make a unique name, no gltf node represents this skin
			skin->set_name(_gen_unique_name(p_state, "Skin"));
		}
	}

	return OK;
}

bool GLTFDocument::_skins_are_same(const Ref<Skin> p_skin_a, const Ref<Skin> p_skin_b) {
	if (p_skin_a->get_bind_count() != p_skin_b->get_bind_count()) {
		return false;
	}

	for (int i = 0; i < p_skin_a->get_bind_count(); ++i) {
		if (p_skin_a->get_bind_bone(i) != p_skin_b->get_bind_bone(i)) {
			return false;
		}
		if (p_skin_a->get_bind_name(i) != p_skin_b->get_bind_name(i)) {
			return false;
		}

		Transform3D a_xform = p_skin_a->get_bind_pose(i);
		Transform3D b_xform = p_skin_b->get_bind_pose(i);

		if (a_xform != b_xform) {
			return false;
		}
	}

	return true;
}

void GLTFDocument::_remove_duplicate_skins(Ref<GLTFState> p_state) {
	for (int i = 0; i < p_state->skins.size(); ++i) {
		for (int j = i + 1; j < p_state->skins.size(); ++j) {
			const Ref<Skin> skin_i = p_state->skins[i]->godot_skin;
			const Ref<Skin> skin_j = p_state->skins[j]->godot_skin;

			if (_skins_are_same(skin_i, skin_j)) {
				// replace it and delete the old
				p_state->skins.write[j]->godot_skin = skin_i;
			}
		}
	}
}

Error GLTFDocument::_serialize_lights(Ref<GLTFState> p_state) {
	if (p_state->lights.is_empty()) {
		return OK;
	}
	Array lights;
	for (GLTFLightIndex i = 0; i < p_state->lights.size(); i++) {
		lights.push_back(p_state->lights[i]->to_dictionary());
	}

	Dictionary extensions;
	if (p_state->json.has("extensions")) {
		extensions = p_state->json["extensions"];
	} else {
		p_state->json["extensions"] = extensions;
	}
	Dictionary lights_punctual;
	extensions["KHR_lights_punctual"] = lights_punctual;
	lights_punctual["lights"] = lights;

	print_verbose("glTF: Total lights: " + itos(p_state->lights.size()));

	return OK;
}

Error GLTFDocument::_serialize_cameras(Ref<GLTFState> p_state) {
	Array cameras;
	cameras.resize(p_state->cameras.size());
	for (GLTFCameraIndex i = 0; i < p_state->cameras.size(); i++) {
		cameras[i] = p_state->cameras[i]->to_dictionary();
	}

	if (!p_state->cameras.size()) {
		return OK;
	}

	p_state->json["cameras"] = cameras;

	print_verbose("glTF: Total cameras: " + itos(p_state->cameras.size()));

	return OK;
}

Error GLTFDocument::_parse_lights(Ref<GLTFState> p_state) {
	if (!p_state->json.has("extensions")) {
		return OK;
	}
	Dictionary extensions = p_state->json["extensions"];
	if (!extensions.has("KHR_lights_punctual")) {
		return OK;
	}
	Dictionary lights_punctual = extensions["KHR_lights_punctual"];
	if (!lights_punctual.has("lights")) {
		return OK;
	}

	const Array &lights = lights_punctual["lights"];

	for (GLTFLightIndex light_i = 0; light_i < lights.size(); light_i++) {
		Ref<GLTFLight> light = GLTFLight::from_dictionary(lights[light_i]);
		if (light.is_null()) {
			return Error::ERR_PARSE_ERROR;
		}
		p_state->lights.push_back(light);
	}

	print_verbose("glTF: Total lights: " + itos(p_state->lights.size()));

	return OK;
}

Error GLTFDocument::_parse_cameras(Ref<GLTFState> p_state) {
	if (!p_state->json.has("cameras")) {
		return OK;
	}

	const Array cameras = p_state->json["cameras"];

	for (GLTFCameraIndex i = 0; i < cameras.size(); i++) {
		p_state->cameras.push_back(GLTFCamera::from_dictionary(cameras[i]));
	}

	print_verbose("glTF: Total cameras: " + itos(p_state->cameras.size()));

	return OK;
}

String GLTFDocument::interpolation_to_string(const GLTFAnimation::Interpolation p_interp) {
	String interp = "LINEAR";
	if (p_interp == GLTFAnimation::INTERP_STEP) {
		interp = "STEP";
	} else if (p_interp == GLTFAnimation::INTERP_LINEAR) {
		interp = "LINEAR";
	} else if (p_interp == GLTFAnimation::INTERP_CATMULLROMSPLINE) {
		interp = "CATMULLROMSPLINE";
	} else if (p_interp == GLTFAnimation::INTERP_CUBIC_SPLINE) {
		interp = "CUBICSPLINE";
	}

	return interp;
}

Error GLTFDocument::_serialize_animations(Ref<GLTFState> p_state) {
	if (!p_state->animation_players.size()) {
		return OK;
	}
	for (int32_t player_i = 0; player_i < p_state->animation_players.size(); player_i++) {
		AnimationPlayer *animation_player = p_state->animation_players[player_i];
		List<StringName> animations;
		animation_player->get_animation_list(&animations);
		for (const StringName &animation_name : animations) {
			_convert_animation(p_state, animation_player, animation_name);
		}
	}
	Array animations;
	for (GLTFAnimationIndex animation_i = 0; animation_i < p_state->animations.size(); animation_i++) {
		Dictionary d;
		Ref<GLTFAnimation> gltf_animation = p_state->animations[animation_i];
		if (!gltf_animation->get_tracks().size()) {
			continue;
		}

		if (!gltf_animation->get_name().is_empty()) {
			d["name"] = gltf_animation->get_name();
		}
		Array channels;
		Array samplers;

		for (KeyValue<int, GLTFAnimation::Track> &track_i : gltf_animation->get_tracks()) {
			GLTFAnimation::Track track = track_i.value;
			if (track.position_track.times.size()) {
				Dictionary t;
				t["sampler"] = samplers.size();
				Dictionary s;

				s["interpolation"] = interpolation_to_string(track.position_track.interpolation);
				Vector<real_t> times = Variant(track.position_track.times);
				s["input"] = _encode_accessor_as_floats(p_state, times, false);
				Vector<Vector3> values = Variant(track.position_track.values);
				s["output"] = _encode_accessor_as_vec3(p_state, values, false);

				samplers.push_back(s);

				Dictionary target;
				target["path"] = "translation";
				target["node"] = track_i.key;

				t["target"] = target;
				channels.push_back(t);
			}
			if (track.rotation_track.times.size()) {
				Dictionary t;
				t["sampler"] = samplers.size();
				Dictionary s;

				s["interpolation"] = interpolation_to_string(track.rotation_track.interpolation);
				Vector<real_t> times = Variant(track.rotation_track.times);
				s["input"] = _encode_accessor_as_floats(p_state, times, false);
				Vector<Quaternion> values = track.rotation_track.values;
				s["output"] = _encode_accessor_as_quaternions(p_state, values, false);

				samplers.push_back(s);

				Dictionary target;
				target["path"] = "rotation";
				target["node"] = track_i.key;

				t["target"] = target;
				channels.push_back(t);
			}
			if (track.scale_track.times.size()) {
				Dictionary t;
				t["sampler"] = samplers.size();
				Dictionary s;

				s["interpolation"] = interpolation_to_string(track.scale_track.interpolation);
				Vector<real_t> times = Variant(track.scale_track.times);
				s["input"] = _encode_accessor_as_floats(p_state, times, false);
				Vector<Vector3> values = Variant(track.scale_track.values);
				s["output"] = _encode_accessor_as_vec3(p_state, values, false);

				samplers.push_back(s);

				Dictionary target;
				target["path"] = "scale";
				target["node"] = track_i.key;

				t["target"] = target;
				channels.push_back(t);
			}
			if (track.weight_tracks.size()) {
				double length = 0.0f;

				for (int32_t track_idx = 0; track_idx < track.weight_tracks.size(); track_idx++) {
					int32_t last_time_index = track.weight_tracks[track_idx].times.size() - 1;
					length = MAX(length, track.weight_tracks[track_idx].times[last_time_index]);
				}

				Dictionary t;
				t["sampler"] = samplers.size();
				Dictionary s;
				Vector<real_t> times;
				const double increment = 1.0 / p_state->get_bake_fps();
				{
					double time = 0.0;
					bool last = false;
					while (true) {
						times.push_back(time);
						if (last) {
							break;
						}
						time += increment;
						if (time >= length) {
							last = true;
							time = length;
						}
					}
				}

				for (int32_t track_idx = 0; track_idx < track.weight_tracks.size(); track_idx++) {
					double time = 0.0;
					bool last = false;
					Vector<real_t> weight_track;
					while (true) {
						float weight = _interpolate_track<real_t>(track.weight_tracks[track_idx].times,
								track.weight_tracks[track_idx].values,
								time,
								track.weight_tracks[track_idx].interpolation);
						weight_track.push_back(weight);
						if (last) {
							break;
						}
						time += increment;
						if (time >= length) {
							last = true;
							time = length;
						}
					}
					track.weight_tracks.write[track_idx].times = times;
					track.weight_tracks.write[track_idx].values = weight_track;
				}

				Vector<real_t> all_track_times = times;
				Vector<real_t> all_track_values;
				int32_t values_size = track.weight_tracks[0].values.size();
				int32_t weight_tracks_size = track.weight_tracks.size();
				all_track_values.resize(weight_tracks_size * values_size);
				for (int k = 0; k < track.weight_tracks.size(); k++) {
					Vector<real_t> wdata = track.weight_tracks[k].values;
					for (int l = 0; l < wdata.size(); l++) {
						int32_t index = l * weight_tracks_size + k;
						ERR_BREAK(index >= all_track_values.size());
						all_track_values.write[index] = wdata.write[l];
					}
				}

				s["interpolation"] = interpolation_to_string(track.weight_tracks[track.weight_tracks.size() - 1].interpolation);
				s["input"] = _encode_accessor_as_floats(p_state, all_track_times, false);
				s["output"] = _encode_accessor_as_floats(p_state, all_track_values, false);

				samplers.push_back(s);

				Dictionary target;
				target["path"] = "weights";
				target["node"] = track_i.key;

				t["target"] = target;
				channels.push_back(t);
			}
		}
		if (channels.size() && samplers.size()) {
			d["channels"] = channels;
			d["samplers"] = samplers;
			animations.push_back(d);
		}
	}

	if (!animations.size()) {
		return OK;
	}
	p_state->json["animations"] = animations;

	print_verbose("glTF: Total animations '" + itos(p_state->animations.size()) + "'.");

	return OK;
}

Error GLTFDocument::_parse_animations(Ref<GLTFState> p_state) {
	if (!p_state->json.has("animations")) {
		return OK;
	}

	const Array &animations = p_state->json["animations"];

	for (GLTFAnimationIndex i = 0; i < animations.size(); i++) {
		const Dictionary &d = animations[i];

		Ref<GLTFAnimation> animation;
		animation.instantiate();

		if (!d.has("channels") || !d.has("samplers")) {
			continue;
		}

		Array channels = d["channels"];
		Array samplers = d["samplers"];

		if (d.has("name")) {
			const String anim_name = d["name"];
			const String anim_name_lower = anim_name.to_lower();
			if (anim_name_lower.begins_with("loop") || anim_name_lower.ends_with("loop") || anim_name_lower.begins_with("cycle") || anim_name_lower.ends_with("cycle")) {
				animation->set_loop(true);
			}
			animation->set_original_name(anim_name);
			animation->set_name(_gen_unique_animation_name(p_state, anim_name));
		}

		for (int j = 0; j < channels.size(); j++) {
			const Dictionary &c = channels[j];
			if (!c.has("target")) {
				continue;
			}

			const Dictionary &t = c["target"];
			if (!t.has("node") || !t.has("path")) {
				continue;
			}

			ERR_FAIL_COND_V(!c.has("sampler"), ERR_PARSE_ERROR);
			const int sampler = c["sampler"];
			ERR_FAIL_INDEX_V(sampler, samplers.size(), ERR_PARSE_ERROR);

			GLTFNodeIndex node = t["node"];
			String path = t["path"];

			ERR_FAIL_INDEX_V(node, p_state->nodes.size(), ERR_PARSE_ERROR);

			GLTFAnimation::Track *track = nullptr;

			if (!animation->get_tracks().has(node)) {
				animation->get_tracks()[node] = GLTFAnimation::Track();
			}

			track = &animation->get_tracks()[node];

			const Dictionary &s = samplers[sampler];

			ERR_FAIL_COND_V(!s.has("input"), ERR_PARSE_ERROR);
			ERR_FAIL_COND_V(!s.has("output"), ERR_PARSE_ERROR);

			const int input = s["input"];
			const int output = s["output"];

			GLTFAnimation::Interpolation interp = GLTFAnimation::INTERP_LINEAR;
			int output_count = 1;
			if (s.has("interpolation")) {
				const String &in = s["interpolation"];
				if (in == "STEP") {
					interp = GLTFAnimation::INTERP_STEP;
				} else if (in == "LINEAR") {
					interp = GLTFAnimation::INTERP_LINEAR;
				} else if (in == "CATMULLROMSPLINE") {
					interp = GLTFAnimation::INTERP_CATMULLROMSPLINE;
					output_count = 3;
				} else if (in == "CUBICSPLINE") {
					interp = GLTFAnimation::INTERP_CUBIC_SPLINE;
					output_count = 3;
				}
			}

			const Vector<float> times = _decode_accessor_as_floats(p_state, input, false);
			if (path == "translation") {
				const Vector<Vector3> positions = _decode_accessor_as_vec3(p_state, output, false);
				track->position_track.interpolation = interp;
				track->position_track.times = Variant(times); //convert via variant
				track->position_track.values = Variant(positions); //convert via variant
			} else if (path == "rotation") {
				const Vector<Quaternion> rotations = _decode_accessor_as_quaternion(p_state, output, false);
				track->rotation_track.interpolation = interp;
				track->rotation_track.times = Variant(times); //convert via variant
				track->rotation_track.values = rotations;
			} else if (path == "scale") {
				const Vector<Vector3> scales = _decode_accessor_as_vec3(p_state, output, false);
				track->scale_track.interpolation = interp;
				track->scale_track.times = Variant(times); //convert via variant
				track->scale_track.values = Variant(scales); //convert via variant
			} else if (path == "weights") {
				const Vector<float> weights = _decode_accessor_as_floats(p_state, output, false);

				ERR_FAIL_INDEX_V(p_state->nodes[node]->mesh, p_state->meshes.size(), ERR_PARSE_ERROR);
				Ref<GLTFMesh> mesh = p_state->meshes[p_state->nodes[node]->mesh];
				ERR_CONTINUE(!mesh->get_blend_weights().size());
				const int wc = mesh->get_blend_weights().size();

				track->weight_tracks.resize(wc);

				const int expected_value_count = times.size() * output_count * wc;
				ERR_CONTINUE_MSG(weights.size() != expected_value_count, "Invalid weight data, expected " + itos(expected_value_count) + " weight values, got " + itos(weights.size()) + " instead.");

				const int wlen = weights.size() / wc;
				for (int k = 0; k < wc; k++) { //separate tracks, having them together is not such a good idea
					GLTFAnimation::Channel<real_t> cf;
					cf.interpolation = interp;
					cf.times = Variant(times);
					Vector<real_t> wdata;
					wdata.resize(wlen);
					for (int l = 0; l < wlen; l++) {
						wdata.write[l] = weights[l * wc + k];
					}

					cf.values = wdata;
					track->weight_tracks.write[k] = cf;
				}
			} else {
				WARN_PRINT("Invalid path '" + path + "'.");
			}
		}

		p_state->animations.push_back(animation);
	}

	print_verbose("glTF: Total animations '" + itos(p_state->animations.size()) + "'.");

	return OK;
}

void GLTFDocument::_assign_node_names(Ref<GLTFState> p_state) {
	for (int i = 0; i < p_state->nodes.size(); i++) {
		Ref<GLTFNode> gltf_node = p_state->nodes[i];
		// Any joints get unique names generated when the skeleton is made, unique to the skeleton
		if (gltf_node->skeleton >= 0) {
			continue;
		}
		String gltf_node_name = gltf_node->get_name();
		if (gltf_node_name.is_empty()) {
			if (_naming_version == 0) {
				if (gltf_node->mesh >= 0) {
					gltf_node_name = _gen_unique_name(p_state, "Mesh");
				} else if (gltf_node->camera >= 0) {
					gltf_node_name = _gen_unique_name(p_state, "Camera3D");
				} else {
					gltf_node_name = _gen_unique_name(p_state, "Node");
				}
			} else {
				if (gltf_node->mesh >= 0) {
					gltf_node_name = "Mesh";
				} else if (gltf_node->camera >= 0) {
					gltf_node_name = "Camera";
				} else {
					gltf_node_name = "Node";
				}
			}
		}
		gltf_node->set_name(_gen_unique_name(p_state, gltf_node_name));
	}
}

BoneAttachment3D *GLTFDocument::_generate_bone_attachment(Ref<GLTFState> p_state, Skeleton3D *p_skeleton, const GLTFNodeIndex p_node_index, const GLTFNodeIndex p_bone_index) {
	Ref<GLTFNode> gltf_node = p_state->nodes[p_node_index];
	Ref<GLTFNode> bone_node = p_state->nodes[p_bone_index];
	BoneAttachment3D *bone_attachment = memnew(BoneAttachment3D);
	print_verbose("glTF: Creating bone attachment for: " + gltf_node->get_name());

	ERR_FAIL_COND_V(!bone_node->joint, nullptr);

	bone_attachment->set_bone_name(bone_node->get_name());

	return bone_attachment;
}

GLTFMeshIndex GLTFDocument::_convert_mesh_to_gltf(Ref<GLTFState> p_state, MeshInstance3D *p_mesh_instance) {
	ERR_FAIL_NULL_V(p_mesh_instance, -1);
	ERR_FAIL_COND_V_MSG(p_mesh_instance->get_mesh().is_null(), -1, "glTF: Tried to export a MeshInstance3D node named " + p_mesh_instance->get_name() + ", but it has no mesh. This node will be exported without a mesh.");
	Ref<Mesh> mesh_resource = p_mesh_instance->get_mesh();
	ERR_FAIL_COND_V_MSG(mesh_resource->get_surface_count() == 0, -1, "glTF: Tried to export a MeshInstance3D node named " + p_mesh_instance->get_name() + ", but its mesh has no surfaces. This node will be exported without a mesh.");
	TypedArray<Material> instance_materials;
	for (int32_t surface_i = 0; surface_i < mesh_resource->get_surface_count(); surface_i++) {
		Ref<Material> mat = p_mesh_instance->get_active_material(surface_i);
		instance_materials.append(mat);
	}
	Ref<ImporterMesh> current_mesh = _mesh_to_importer_mesh(mesh_resource);
	Vector<float> blend_weights;
	int32_t blend_count = mesh_resource->get_blend_shape_count();
	blend_weights.resize(blend_count);
	for (int32_t blend_i = 0; blend_i < blend_count; blend_i++) {
		blend_weights.write[blend_i] = 0.0f;
	}

	Ref<GLTFMesh> gltf_mesh;
	gltf_mesh.instantiate();
	gltf_mesh->set_instance_materials(instance_materials);
	gltf_mesh->set_mesh(current_mesh);
	gltf_mesh->set_blend_weights(blend_weights);
	GLTFMeshIndex mesh_i = p_state->meshes.size();
	p_state->meshes.push_back(gltf_mesh);
	return mesh_i;
}

ImporterMeshInstance3D *GLTFDocument::_generate_mesh_instance(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index) {
	Ref<GLTFNode> gltf_node = p_state->nodes[p_node_index];

	ERR_FAIL_INDEX_V(gltf_node->mesh, p_state->meshes.size(), nullptr);

	ImporterMeshInstance3D *mi = memnew(ImporterMeshInstance3D);
	print_verbose("glTF: Creating mesh for: " + gltf_node->get_name());

	p_state->scene_mesh_instances.insert(p_node_index, mi);
	Ref<GLTFMesh> mesh = p_state->meshes.write[gltf_node->mesh];
	if (mesh.is_null()) {
		return mi;
	}
	Ref<ImporterMesh> import_mesh = mesh->get_mesh();
	if (import_mesh.is_null()) {
		return mi;
	}
	mi->set_mesh(import_mesh);
	import_mesh->merge_meta_from(*mesh);
	return mi;
}

Light3D *GLTFDocument::_generate_light(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index) {
	Ref<GLTFNode> gltf_node = p_state->nodes[p_node_index];

	ERR_FAIL_INDEX_V(gltf_node->light, p_state->lights.size(), nullptr);

	print_verbose("glTF: Creating light for: " + gltf_node->get_name());

	Ref<GLTFLight> l = p_state->lights[gltf_node->light];
	return l->to_node();
}

Camera3D *GLTFDocument::_generate_camera(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index) {
	Ref<GLTFNode> gltf_node = p_state->nodes[p_node_index];

	ERR_FAIL_INDEX_V(gltf_node->camera, p_state->cameras.size(), nullptr);

	print_verbose("glTF: Creating camera for: " + gltf_node->get_name());

	Ref<GLTFCamera> c = p_state->cameras[gltf_node->camera];
	return c->to_node();
}

GLTFCameraIndex GLTFDocument::_convert_camera(Ref<GLTFState> p_state, Camera3D *p_camera) {
	print_verbose("glTF: Converting camera: " + p_camera->get_name());

	Ref<GLTFCamera> c = GLTFCamera::from_node(p_camera);
	GLTFCameraIndex camera_index = p_state->cameras.size();
	p_state->cameras.push_back(c);
	return camera_index;
}

GLTFLightIndex GLTFDocument::_convert_light(Ref<GLTFState> p_state, Light3D *p_light) {
	print_verbose("glTF: Converting light: " + p_light->get_name());

	Ref<GLTFLight> l = GLTFLight::from_node(p_light);

	GLTFLightIndex light_index = p_state->lights.size();
	p_state->lights.push_back(l);
	return light_index;
}

void GLTFDocument::_convert_spatial(Ref<GLTFState> p_state, Node3D *p_spatial, Ref<GLTFNode> p_node) {
	p_node->transform = p_spatial->get_transform();
}

Node3D *GLTFDocument::_generate_spatial(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index) {
	Ref<GLTFNode> gltf_node = p_state->nodes[p_node_index];

	Node3D *spatial = memnew(Node3D);
	print_verbose("glTF: Converting spatial: " + gltf_node->get_name());

	return spatial;
}

void GLTFDocument::_convert_scene_node(Ref<GLTFState> p_state, Node *p_current, const GLTFNodeIndex p_gltf_parent, const GLTFNodeIndex p_gltf_root) {
	bool retflag = true;
	_check_visibility(p_current, retflag);
	if (retflag) {
		return;
	}
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && p_gltf_root != -1 && p_current->get_owner() == nullptr) {
		WARN_VERBOSE("glTF export warning: Node '" + p_current->get_name() + "' has no owner. This is likely a temporary node generated by a @tool script. This would not be saved when saving the Godot scene, therefore it will not be exported to glTF.");
		return;
	}
#endif // TOOLS_ENABLED
	Ref<GLTFNode> gltf_node;
	gltf_node.instantiate();
	gltf_node->set_original_name(p_current->get_name());
	gltf_node->set_name(_gen_unique_name(p_state, p_current->get_name()));
	gltf_node->merge_meta_from(p_current);
	if (cast_to<Node3D>(p_current)) {
		Node3D *spatial = cast_to<Node3D>(p_current);
		_convert_spatial(p_state, spatial, gltf_node);
	}
	if (cast_to<MeshInstance3D>(p_current)) {
		MeshInstance3D *mi = cast_to<MeshInstance3D>(p_current);
		_convert_mesh_instance_to_gltf(mi, p_state, gltf_node);
	} else if (cast_to<BoneAttachment3D>(p_current)) {
		BoneAttachment3D *bone = cast_to<BoneAttachment3D>(p_current);
		_convert_bone_attachment_to_gltf(bone, p_state, p_gltf_parent, p_gltf_root, gltf_node);
		return;
	} else if (cast_to<Skeleton3D>(p_current)) {
		Skeleton3D *skel = cast_to<Skeleton3D>(p_current);
		_convert_skeleton_to_gltf(skel, p_state, p_gltf_parent, p_gltf_root, gltf_node);
		// We ignore the Godot Engine node that is the skeleton.
		return;
	} else if (cast_to<MultiMeshInstance3D>(p_current)) {
		MultiMeshInstance3D *multi = cast_to<MultiMeshInstance3D>(p_current);
		_convert_multi_mesh_instance_to_gltf(multi, p_gltf_parent, p_gltf_root, gltf_node, p_state);
#ifdef MODULE_CSG_ENABLED
	} else if (cast_to<CSGShape3D>(p_current)) {
		CSGShape3D *shape = cast_to<CSGShape3D>(p_current);
		if (shape->get_parent() && shape->is_root_shape()) {
			_convert_csg_shape_to_gltf(shape, p_gltf_parent, gltf_node, p_state);
		}
#endif // MODULE_CSG_ENABLED
#ifdef MODULE_GRIDMAP_ENABLED
	} else if (cast_to<GridMap>(p_current)) {
		GridMap *gridmap = Object::cast_to<GridMap>(p_current);
		_convert_grid_map_to_gltf(gridmap, p_gltf_parent, p_gltf_root, gltf_node, p_state);
#endif // MODULE_GRIDMAP_ENABLED
	} else if (cast_to<Camera3D>(p_current)) {
		Camera3D *camera = Object::cast_to<Camera3D>(p_current);
		_convert_camera_to_gltf(camera, p_state, gltf_node);
	} else if (cast_to<Light3D>(p_current)) {
		Light3D *light = Object::cast_to<Light3D>(p_current);
		_convert_light_to_gltf(light, p_state, gltf_node);
	} else if (cast_to<AnimationPlayer>(p_current)) {
		AnimationPlayer *animation_player = Object::cast_to<AnimationPlayer>(p_current);
		_convert_animation_player_to_gltf(animation_player, p_state, p_gltf_parent, p_gltf_root, gltf_node, p_current);
	}
	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		ext->convert_scene_node(p_state, gltf_node, p_current);
	}
	GLTFNodeIndex current_node_i;
	if (gltf_node->get_parent() == -1) {
		current_node_i = p_state->append_gltf_node(gltf_node, p_current, p_gltf_parent);
	} else if (gltf_node->get_parent() < -1) {
		return;
	} else {
		current_node_i = p_state->nodes.size() - 1;
		while (gltf_node != p_state->nodes[current_node_i]) {
			current_node_i--;
		}
	}
	const GLTFNodeIndex gltf_root = (p_gltf_root == -1) ? current_node_i : p_gltf_root;
	for (int node_i = 0; node_i < p_current->get_child_count(); node_i++) {
		_convert_scene_node(p_state, p_current->get_child(node_i), current_node_i, gltf_root);
	}
}

#ifdef MODULE_CSG_ENABLED
void GLTFDocument::_convert_csg_shape_to_gltf(CSGShape3D *p_current, GLTFNodeIndex p_gltf_parent, Ref<GLTFNode> p_gltf_node, Ref<GLTFState> p_state) {
	CSGShape3D *csg = p_current;
	csg->call("_update_shape");
	Array meshes = csg->get_meshes();
	if (meshes.size() != 2) {
		return;
	}

	Ref<ImporterMesh> mesh;
	mesh.instantiate();
	{
		Ref<ArrayMesh> csg_mesh = csg->get_meshes()[1];
		for (int32_t surface_i = 0; surface_i < csg_mesh->get_surface_count(); surface_i++) {
			Array array = csg_mesh->surface_get_arrays(surface_i);

			Ref<Material> mat;

			Ref<Material> mat_override = csg->get_material_override();
			if (mat_override.is_valid()) {
				mat = mat_override;
			}

			Ref<Material> mat_surface_override = csg_mesh->surface_get_material(surface_i);
			if (mat_surface_override.is_valid() && mat.is_null()) {
				mat = mat_surface_override;
			}

			String mat_name;
			if (mat.is_valid()) {
				mat_name = mat->get_name();
			} else {
				// Assign default material when no material is assigned.
				mat = Ref<StandardMaterial3D>(memnew(StandardMaterial3D));
			}

			mesh->add_surface(csg_mesh->surface_get_primitive_type(surface_i),
					array, csg_mesh->surface_get_blend_shape_arrays(surface_i), csg_mesh->surface_get_lods(surface_i), mat,
					mat_name, csg_mesh->surface_get_format(surface_i));
		}
	}

	Ref<GLTFMesh> gltf_mesh;
	gltf_mesh.instantiate();
	gltf_mesh->set_mesh(mesh);
	gltf_mesh->set_original_name(csg->get_name());
	GLTFMeshIndex mesh_i = p_state->meshes.size();
	p_state->meshes.push_back(gltf_mesh);
	p_gltf_node->mesh = mesh_i;
	p_gltf_node->transform = csg->get_transform();
	p_gltf_node->set_original_name(csg->get_name());
	p_gltf_node->set_name(_gen_unique_name(p_state, csg->get_name()));
}
#endif // MODULE_CSG_ENABLED

void GLTFDocument::_convert_animation_player_to_gltf(AnimationPlayer *p_animation_player, Ref<GLTFState> p_state, GLTFNodeIndex p_gltf_current, GLTFNodeIndex p_gltf_root_index, Ref<GLTFNode> p_gltf_node, Node *p_scene_parent) {
	ERR_FAIL_NULL(p_animation_player);
	p_state->animation_players.push_back(p_animation_player);
	print_verbose(String("glTF: Converting animation player: ") + p_animation_player->get_name());
}

void GLTFDocument::_check_visibility(Node *p_node, bool &r_retflag) {
	r_retflag = true;
	Node3D *spatial = Object::cast_to<Node3D>(p_node);
	Node2D *node_2d = Object::cast_to<Node2D>(p_node);
	if (node_2d && !node_2d->is_visible()) {
		return;
	}
	if (spatial && !spatial->is_visible()) {
		return;
	}
	r_retflag = false;
}

void GLTFDocument::_convert_camera_to_gltf(Camera3D *camera, Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node) {
	ERR_FAIL_NULL(camera);
	GLTFCameraIndex camera_index = _convert_camera(p_state, camera);
	if (camera_index != -1) {
		p_gltf_node->camera = camera_index;
	}
}

void GLTFDocument::_convert_light_to_gltf(Light3D *light, Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node) {
	ERR_FAIL_NULL(light);
	GLTFLightIndex light_index = _convert_light(p_state, light);
	if (light_index != -1) {
		p_gltf_node->light = light_index;
	}
}

#ifdef MODULE_GRIDMAP_ENABLED
void GLTFDocument::_convert_grid_map_to_gltf(GridMap *p_grid_map, GLTFNodeIndex p_parent_node_index, GLTFNodeIndex p_root_node_index, Ref<GLTFNode> p_gltf_node, Ref<GLTFState> p_state) {
	Array cells = p_grid_map->get_used_cells();
	for (int32_t k = 0; k < cells.size(); k++) {
		GLTFNode *new_gltf_node = memnew(GLTFNode);
		p_gltf_node->children.push_back(p_state->nodes.size());
		p_state->nodes.push_back(new_gltf_node);
		Vector3 cell_location = cells[k];
		int32_t cell = p_grid_map->get_cell_item(
				Vector3(cell_location.x, cell_location.y, cell_location.z));
		Transform3D cell_xform;
		cell_xform.basis = p_grid_map->get_basis_with_orthogonal_index(
				p_grid_map->get_cell_item_orientation(
						Vector3(cell_location.x, cell_location.y, cell_location.z)));
		cell_xform.basis.scale(Vector3(p_grid_map->get_cell_scale(),
				p_grid_map->get_cell_scale(),
				p_grid_map->get_cell_scale()));
		cell_xform.set_origin(p_grid_map->map_to_local(
				Vector3(cell_location.x, cell_location.y, cell_location.z)));
		Ref<GLTFMesh> gltf_mesh;
		gltf_mesh.instantiate();
		gltf_mesh->set_mesh(_mesh_to_importer_mesh(p_grid_map->get_mesh_library()->get_item_mesh(cell)));
		gltf_mesh->set_original_name(p_grid_map->get_mesh_library()->get_item_name(cell));
		new_gltf_node->mesh = p_state->meshes.size();
		p_state->meshes.push_back(gltf_mesh);
		new_gltf_node->transform = cell_xform * p_grid_map->get_transform();
		new_gltf_node->set_original_name(p_grid_map->get_mesh_library()->get_item_name(cell));
		new_gltf_node->set_name(_gen_unique_name(p_state, p_grid_map->get_mesh_library()->get_item_name(cell)));
	}
}
#endif // MODULE_GRIDMAP_ENABLED

void GLTFDocument::_convert_multi_mesh_instance_to_gltf(
		MultiMeshInstance3D *p_multi_mesh_instance,
		GLTFNodeIndex p_parent_node_index,
		GLTFNodeIndex p_root_node_index,
		Ref<GLTFNode> p_gltf_node, Ref<GLTFState> p_state) {
	ERR_FAIL_NULL(p_multi_mesh_instance);
	Ref<MultiMesh> multi_mesh = p_multi_mesh_instance->get_multimesh();
	if (multi_mesh.is_null()) {
		return;
	}
	Ref<GLTFMesh> gltf_mesh;
	gltf_mesh.instantiate();
	Ref<Mesh> mesh = multi_mesh->get_mesh();
	if (mesh.is_null()) {
		return;
	}
	gltf_mesh->set_original_name(multi_mesh->get_name());
	gltf_mesh->set_name(multi_mesh->get_name());
	Ref<ImporterMesh> importer_mesh;
	importer_mesh.instantiate();
	Ref<ArrayMesh> array_mesh = multi_mesh->get_mesh();
	if (array_mesh.is_valid()) {
		importer_mesh->set_blend_shape_mode(array_mesh->get_blend_shape_mode());
		for (int32_t blend_i = 0; blend_i < array_mesh->get_blend_shape_count(); blend_i++) {
			importer_mesh->add_blend_shape(array_mesh->get_blend_shape_name(blend_i));
		}
	}
	for (int32_t surface_i = 0; surface_i < mesh->get_surface_count(); surface_i++) {
		Ref<Material> mat = mesh->surface_get_material(surface_i);
		String material_name;
		if (mat.is_valid()) {
			material_name = mat->get_name();
		}
		Array blend_arrays;
		if (array_mesh.is_valid()) {
			blend_arrays = array_mesh->surface_get_blend_shape_arrays(surface_i);
		}
		importer_mesh->add_surface(mesh->surface_get_primitive_type(surface_i), mesh->surface_get_arrays(surface_i),
				blend_arrays, mesh->surface_get_lods(surface_i), mat, material_name, mesh->surface_get_format(surface_i));
	}
	gltf_mesh->set_mesh(importer_mesh);
	GLTFMeshIndex mesh_index = p_state->meshes.size();
	p_state->meshes.push_back(gltf_mesh);
	for (int32_t instance_i = 0; instance_i < multi_mesh->get_instance_count();
			instance_i++) {
		Transform3D transform;
		if (multi_mesh->get_transform_format() == MultiMesh::TRANSFORM_2D) {
			Transform2D xform_2d = multi_mesh->get_instance_transform_2d(instance_i);
			transform.origin =
					Vector3(xform_2d.get_origin().x, 0, xform_2d.get_origin().y);
			real_t rotation = xform_2d.get_rotation();
			Quaternion quaternion(Vector3(0, 1, 0), rotation);
			Size2 scale = xform_2d.get_scale();
			transform.basis.set_quaternion_scale(quaternion,
					Vector3(scale.x, 0, scale.y));
			transform = p_multi_mesh_instance->get_transform() * transform;
		} else if (multi_mesh->get_transform_format() == MultiMesh::TRANSFORM_3D) {
			transform = p_multi_mesh_instance->get_transform() *
					multi_mesh->get_instance_transform(instance_i);
		}
		Ref<GLTFNode> new_gltf_node;
		new_gltf_node.instantiate();
		new_gltf_node->mesh = mesh_index;
		new_gltf_node->transform = transform;
		new_gltf_node->set_original_name(p_multi_mesh_instance->get_name());
		new_gltf_node->set_name(_gen_unique_name(p_state, p_multi_mesh_instance->get_name()));
		p_gltf_node->children.push_back(p_state->nodes.size());
		p_state->nodes.push_back(new_gltf_node);
	}
}

void GLTFDocument::_convert_skeleton_to_gltf(Skeleton3D *p_skeleton3d, Ref<GLTFState> p_state, GLTFNodeIndex p_parent_node_index, GLTFNodeIndex p_root_node_index, Ref<GLTFNode> p_gltf_node) {
	Skeleton3D *skeleton = p_skeleton3d;
	Ref<GLTFSkeleton> gltf_skeleton;
	gltf_skeleton.instantiate();
	// GLTFSkeleton is only used to hold internal p_state data. It will not be written to the document.
	//
	gltf_skeleton->godot_skeleton = skeleton;
	GLTFSkeletonIndex skeleton_i = p_state->skeletons.size();
	p_state->skeleton3d_to_gltf_skeleton[skeleton->get_instance_id()] = skeleton_i;
	p_state->skeletons.push_back(gltf_skeleton);

	BoneId bone_count = skeleton->get_bone_count();
	for (BoneId bone_i = 0; bone_i < bone_count; bone_i++) {
		Ref<GLTFNode> joint_node;
		joint_node.instantiate();
		// Note that we cannot use _gen_unique_bone_name here, because glTF spec requires all node
		// names to be unique regardless of whether or not they are used as joints.
		joint_node->set_original_name(skeleton->get_bone_name(bone_i));
		joint_node->set_name(_gen_unique_name(p_state, skeleton->get_bone_name(bone_i)));
		joint_node->transform = skeleton->get_bone_pose(bone_i);
		joint_node->joint = true;

		if (p_skeleton3d->has_bone_meta(bone_i, "extras")) {
			joint_node->set_meta("extras", p_skeleton3d->get_bone_meta(bone_i, "extras"));
		}
		GLTFNodeIndex current_node_i = p_state->nodes.size();
		p_state->scene_nodes.insert(current_node_i, skeleton);
		p_state->nodes.push_back(joint_node);

		gltf_skeleton->joints.push_back(current_node_i);
		if (skeleton->get_bone_parent(bone_i) == -1) {
			gltf_skeleton->roots.push_back(current_node_i);
		}
		gltf_skeleton->godot_bone_node.insert(bone_i, current_node_i);
	}
	for (BoneId bone_i = 0; bone_i < bone_count; bone_i++) {
		GLTFNodeIndex current_node_i = gltf_skeleton->godot_bone_node[bone_i];
		BoneId parent_bone_id = skeleton->get_bone_parent(bone_i);
		if (parent_bone_id == -1) {
			if (p_parent_node_index != -1) {
				p_state->nodes.write[current_node_i]->parent = p_parent_node_index;
				p_state->nodes.write[p_parent_node_index]->children.push_back(current_node_i);
			}
		} else {
			GLTFNodeIndex parent_node_i = gltf_skeleton->godot_bone_node[parent_bone_id];
			p_state->nodes.write[current_node_i]->parent = parent_node_i;
			p_state->nodes.write[parent_node_i]->children.push_back(current_node_i);
		}
	}
	// Remove placeholder skeleton3d node by not creating the gltf node
	// Skins are per mesh
	for (int node_i = 0; node_i < skeleton->get_child_count(); node_i++) {
		_convert_scene_node(p_state, skeleton->get_child(node_i), p_parent_node_index, p_root_node_index);
	}
}

void GLTFDocument::_convert_bone_attachment_to_gltf(BoneAttachment3D *p_bone_attachment, Ref<GLTFState> p_state, GLTFNodeIndex p_parent_node_index, GLTFNodeIndex p_root_node_index, Ref<GLTFNode> p_gltf_node) {
	Skeleton3D *skeleton;
	// Note that relative transforms to external skeletons and pose overrides are not supported.
	if (p_bone_attachment->get_use_external_skeleton()) {
		skeleton = cast_to<Skeleton3D>(p_bone_attachment->get_node_or_null(p_bone_attachment->get_external_skeleton()));
	} else {
		skeleton = cast_to<Skeleton3D>(p_bone_attachment->get_parent());
	}
	GLTFSkeletonIndex skel_gltf_i = -1;
	if (skeleton != nullptr && p_state->skeleton3d_to_gltf_skeleton.has(skeleton->get_instance_id())) {
		skel_gltf_i = p_state->skeleton3d_to_gltf_skeleton[skeleton->get_instance_id()];
	}
	int bone_idx = -1;
	if (skeleton != nullptr) {
		bone_idx = p_bone_attachment->get_bone_idx();
		if (bone_idx == -1) {
			bone_idx = skeleton->find_bone(p_bone_attachment->get_bone_name());
		}
	}
	GLTFNodeIndex par_node_index = p_parent_node_index;
	if (skeleton != nullptr && bone_idx != -1 && skel_gltf_i != -1) {
		Ref<GLTFSkeleton> gltf_skeleton = p_state->skeletons.write[skel_gltf_i];
		gltf_skeleton->bone_attachments.push_back(p_bone_attachment);
		par_node_index = gltf_skeleton->joints[bone_idx];
	}

	for (int node_i = 0; node_i < p_bone_attachment->get_child_count(); node_i++) {
		_convert_scene_node(p_state, p_bone_attachment->get_child(node_i), par_node_index, p_root_node_index);
	}
}

void GLTFDocument::_convert_mesh_instance_to_gltf(MeshInstance3D *p_scene_parent, Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node) {
	GLTFMeshIndex gltf_mesh_index = _convert_mesh_to_gltf(p_state, p_scene_parent);
	if (gltf_mesh_index != -1) {
		p_gltf_node->mesh = gltf_mesh_index;
	}
}

void GLTFDocument::_generate_scene_node(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index, Node *p_scene_parent, Node *p_scene_root) {
	Ref<GLTFNode> gltf_node = p_state->nodes[p_node_index];

	if (gltf_node->skeleton >= 0) {
		_generate_skeleton_bone_node(p_state, p_node_index, p_scene_parent, p_scene_root);
		return;
	}

	Node3D *current_node = nullptr;

	// Is our parent a skeleton
	Skeleton3D *active_skeleton = Object::cast_to<Skeleton3D>(p_scene_parent);

	const bool non_bone_parented_to_skeleton = active_skeleton;

	// skinned meshes must not be placed in a bone attachment.
	if (non_bone_parented_to_skeleton && gltf_node->skin < 0) {
		// Bone Attachment - Parent Case
		BoneAttachment3D *bone_attachment = _generate_bone_attachment(p_state, active_skeleton, p_node_index, gltf_node->parent);

		p_scene_parent->add_child(bone_attachment, true);

		// Find the correct bone_idx so we can properly serialize it.
		bone_attachment->set_bone_idx(active_skeleton->find_bone(gltf_node->get_name()));

		bone_attachment->set_owner(p_scene_root);

		// There is no gltf_node that represent this, so just directly create a unique name
		bone_attachment->set_name(gltf_node->get_name());

		// We change the scene_parent to our bone attachment now. We do not set current_node because we want to make the node
		// and attach it to the bone_attachment
		p_scene_parent = bone_attachment;
	}
	// Check if any GLTFDocumentExtension classes want to generate a node for us.
	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		current_node = ext->generate_scene_node(p_state, gltf_node, p_scene_parent);
		if (current_node) {
			break;
		}
	}
	// If none of our GLTFDocumentExtension classes generated us a node, we generate one.
	if (!current_node) {
		if (gltf_node->skin >= 0 && gltf_node->mesh >= 0 && !gltf_node->children.is_empty()) {
			// glTF specifies that skinned meshes should ignore their node transforms,
			// only being controlled by the skeleton, so Godot will reparent a skinned
			// mesh to its skeleton. However, we still need to ensure any child nodes
			// keep their place in the tree, so if there are any child nodes, the skinned
			// mesh must not be the base node, so generate an empty spatial base.
			current_node = _generate_spatial(p_state, p_node_index);
			Node3D *mesh_inst = _generate_mesh_instance(p_state, p_node_index);
			mesh_inst->set_name(gltf_node->get_name());
			current_node->add_child(mesh_inst, true);
		} else if (gltf_node->mesh >= 0) {
			current_node = _generate_mesh_instance(p_state, p_node_index);
		} else if (gltf_node->camera >= 0) {
			current_node = _generate_camera(p_state, p_node_index);
		} else if (gltf_node->light >= 0) {
			current_node = _generate_light(p_state, p_node_index);
		} else {
			current_node = _generate_spatial(p_state, p_node_index);
		}
	}
	String gltf_node_name = gltf_node->get_name();
	if (!gltf_node_name.is_empty()) {
		current_node->set_name(gltf_node_name);
	}
	// Note: p_scene_parent and p_scene_root must either both be null or both be valid.
	if (p_scene_root == nullptr) {
		// If the root node argument is null, this is the root node.
		p_scene_root = current_node;
		// If multiple nodes were generated under the root node, ensure they have the owner set.
		if (unlikely(current_node->get_child_count() > 0)) {
			Array args;
			args.append(p_scene_root);
			for (int i = 0; i < current_node->get_child_count(); i++) {
				Node *child = current_node->get_child(i);
				child->propagate_call(StringName("set_owner"), args);
			}
		}
	} else {
		// Add the node we generated and set the owner to the scene root.
		p_scene_parent->add_child(current_node, true);
		Array args;
		args.append(p_scene_root);
		current_node->propagate_call(StringName("set_owner"), args);
		current_node->set_transform(gltf_node->transform);
	}

	current_node->merge_meta_from(*gltf_node);

	p_state->scene_nodes.insert(p_node_index, current_node);
	for (int i = 0; i < gltf_node->children.size(); ++i) {
		_generate_scene_node(p_state, gltf_node->children[i], current_node, p_scene_root);
	}
}

void GLTFDocument::_generate_skeleton_bone_node(Ref<GLTFState> p_state, const GLTFNodeIndex p_node_index, Node *p_scene_parent, Node *p_scene_root) {
	Ref<GLTFNode> gltf_node = p_state->nodes[p_node_index];

	Node3D *current_node = nullptr;

	Skeleton3D *skeleton = p_state->skeletons[gltf_node->skeleton]->godot_skeleton;
	// In this case, this node is already a bone in skeleton.
	const bool is_skinned_mesh = (gltf_node->skin >= 0 && gltf_node->mesh >= 0);
	const bool requires_extra_node = (gltf_node->mesh >= 0 || gltf_node->camera >= 0 || gltf_node->light >= 0);

	Skeleton3D *active_skeleton = Object::cast_to<Skeleton3D>(p_scene_parent);
	if (active_skeleton != skeleton) {
		if (active_skeleton) {
			// Should no longer be possible.
			ERR_PRINT(vformat("glTF: Generating scene detected direct parented Skeletons at node %d", p_node_index));
			BoneAttachment3D *bone_attachment = _generate_bone_attachment(p_state, active_skeleton, p_node_index, gltf_node->parent);
			p_scene_parent->add_child(bone_attachment, true);
			bone_attachment->set_owner(p_scene_root);
			// There is no gltf_node that represent this, so just directly create a unique name
			bone_attachment->set_name(_gen_unique_name(p_state, "BoneAttachment3D"));
			// We change the scene_parent to our bone attachment now. We do not set current_node because we want to make the node
			// and attach it to the bone_attachment
			p_scene_parent = bone_attachment;
		}
		if (skeleton->get_parent() == nullptr) {
			if (p_scene_root) {
				p_scene_parent->add_child(skeleton, true);
				skeleton->set_owner(p_scene_root);
			} else {
				p_scene_parent = skeleton;
				p_scene_root = skeleton;
			}
		}
	}

	active_skeleton = skeleton;
	current_node = active_skeleton;
	if (active_skeleton) {
		p_scene_parent = active_skeleton;
	}

	if (requires_extra_node) {
		current_node = nullptr;
		// skinned meshes must not be placed in a bone attachment.
		if (!is_skinned_mesh) {
			// Bone Attachment - Same Node Case
			BoneAttachment3D *bone_attachment = _generate_bone_attachment(p_state, active_skeleton, p_node_index, p_node_index);

			p_scene_parent->add_child(bone_attachment, true);

			// Find the correct bone_idx so we can properly serialize it.
			bone_attachment->set_bone_idx(active_skeleton->find_bone(gltf_node->get_name()));

			bone_attachment->set_owner(p_scene_root);

			// There is no gltf_node that represent this, so just directly create a unique name
			bone_attachment->set_name(gltf_node->get_name());

			// We change the scene_parent to our bone attachment now. We do not set current_node because we want to make the node
			// and attach it to the bone_attachment
			p_scene_parent = bone_attachment;
		}
		// Check if any GLTFDocumentExtension classes want to generate a node for us.
		for (Ref<GLTFDocumentExtension> ext : document_extensions) {
			ERR_CONTINUE(ext.is_null());
			current_node = ext->generate_scene_node(p_state, gltf_node, p_scene_parent);
			if (current_node) {
				break;
			}
		}
		// If none of our GLTFDocumentExtension classes generated us a node, we generate one.
		if (!current_node) {
			if (gltf_node->mesh >= 0) {
				current_node = _generate_mesh_instance(p_state, p_node_index);
			} else if (gltf_node->camera >= 0) {
				current_node = _generate_camera(p_state, p_node_index);
			} else if (gltf_node->light >= 0) {
				current_node = _generate_light(p_state, p_node_index);
			} else {
				current_node = _generate_spatial(p_state, p_node_index);
			}
		}
		// Add the node we generated and set the owner to the scene root.
		p_scene_parent->add_child(current_node, true);
		if (current_node != p_scene_root) {
			Array args;
			args.append(p_scene_root);
			current_node->propagate_call(StringName("set_owner"), args);
		}
		// Do not set transform here. Transform is already applied to our bone.
		current_node->set_name(gltf_node->get_name());
	}

	p_state->scene_nodes.insert(p_node_index, current_node);

	for (int i = 0; i < gltf_node->children.size(); ++i) {
		_generate_scene_node(p_state, gltf_node->children[i], active_skeleton, p_scene_root);
	}
}

template <typename T>
struct SceneFormatImporterGLTFInterpolate {
	T lerp(const T &a, const T &b, float c) const {
		return a + (b - a) * c;
	}

	T catmull_rom(const T &p0, const T &p1, const T &p2, const T &p3, float t) {
		const float t2 = t * t;
		const float t3 = t2 * t;

		return 0.5f * ((2.0f * p1) + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
	}

	T hermite(T start, T tan_start, T end, T tan_end, float t) {
		/* Formula from the glTF 2.0 specification. */
		const real_t t2 = t * t;
		const real_t t3 = t2 * t;

		const real_t h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
		const real_t h10 = t3 - 2.0 * t2 + t;
		const real_t h01 = -2.0 * t3 + 3.0 * t2;
		const real_t h11 = t3 - t2;

		return start * h00 + tan_start * h10 + end * h01 + tan_end * h11;
	}
};

// thank you for existing, partial specialization
template <>
struct SceneFormatImporterGLTFInterpolate<Quaternion> {
	Quaternion lerp(const Quaternion &a, const Quaternion &b, const float c) const {
		ERR_FAIL_COND_V_MSG(!a.is_normalized(), Quaternion(), vformat("The quaternion \"a\" %s must be normalized.", a));
		ERR_FAIL_COND_V_MSG(!b.is_normalized(), Quaternion(), vformat("The quaternion \"b\" %s must be normalized.", b));

		return a.slerp(b, c).normalized();
	}

	Quaternion catmull_rom(const Quaternion &p0, const Quaternion &p1, const Quaternion &p2, const Quaternion &p3, const float c) {
		ERR_FAIL_COND_V_MSG(!p1.is_normalized(), Quaternion(), vformat("The quaternion \"p1\" (%s) must be normalized.", p1));
		ERR_FAIL_COND_V_MSG(!p2.is_normalized(), Quaternion(), vformat("The quaternion \"p2\" (%s) must be normalized.", p2));

		return p1.slerp(p2, c).normalized();
	}

	Quaternion hermite(const Quaternion start, const Quaternion tan_start, const Quaternion end, const Quaternion tan_end, const float t) {
		ERR_FAIL_COND_V_MSG(!start.is_normalized(), Quaternion(), vformat("The start quaternion %s must be normalized.", start));
		ERR_FAIL_COND_V_MSG(!end.is_normalized(), Quaternion(), vformat("The end quaternion %s must be normalized.", end));

		return start.slerp(end, t).normalized();
	}
};

template <typename T>
T GLTFDocument::_interpolate_track(const Vector<real_t> &p_times, const Vector<T> &p_values, const float p_time, const GLTFAnimation::Interpolation p_interp) {
	ERR_FAIL_COND_V(p_values.is_empty(), T());
	if (p_times.size() != (p_values.size() / (p_interp == GLTFAnimation::INTERP_CUBIC_SPLINE ? 3 : 1))) {
		ERR_PRINT_ONCE("The interpolated values are not corresponding to its times.");
		return p_values[0];
	}
	//could use binary search, worth it?
	int idx = -1;
	for (int i = 0; i < p_times.size(); i++) {
		if (p_times[i] > p_time) {
			break;
		}
		idx++;
	}

	SceneFormatImporterGLTFInterpolate<T> interp;

	switch (p_interp) {
		case GLTFAnimation::INTERP_LINEAR: {
			if (idx == -1) {
				return p_values[0];
			} else if (idx >= p_times.size() - 1) {
				return p_values[p_times.size() - 1];
			}

			const float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

			return interp.lerp(p_values[idx], p_values[idx + 1], c);
		} break;
		case GLTFAnimation::INTERP_STEP: {
			if (idx == -1) {
				return p_values[0];
			} else if (idx >= p_times.size() - 1) {
				return p_values[p_times.size() - 1];
			}

			return p_values[idx];
		} break;
		case GLTFAnimation::INTERP_CATMULLROMSPLINE: {
			if (idx == -1) {
				return p_values[1];
			} else if (idx >= p_times.size() - 1) {
				return p_values[1 + p_times.size() - 1];
			}

			const float c = (p_time - p_times[idx]) / (p_times[idx + 1] - p_times[idx]);

			return interp.catmull_rom(p_values[idx - 1], p_values[idx], p_values[idx + 1], p_values[idx + 3], c);
		} break;
		case GLTFAnimation::INTERP_CUBIC_SPLINE: {
			if (idx == -1) {
				return p_values[1];
			} else if (idx >= p_times.size() - 1) {
				return p_values[(p_times.size() - 1) * 3 + 1];
			}

			const float td = (p_times[idx + 1] - p_times[idx]);
			const float c = (p_time - p_times[idx]) / td;

			const T &from = p_values[idx * 3 + 1];
			const T tan_from = td * p_values[idx * 3 + 2];
			const T &to = p_values[idx * 3 + 4];
			const T tan_to = td * p_values[idx * 3 + 3];

			return interp.hermite(from, tan_from, to, tan_to, c);
		} break;
	}

	ERR_FAIL_V(p_values[0]);
}

void GLTFDocument::_import_animation(Ref<GLTFState> p_state, AnimationPlayer *p_animation_player, const GLTFAnimationIndex p_index, const bool p_trimming, const bool p_remove_immutable_tracks) {
	ERR_FAIL_COND(p_state.is_null());
	Ref<GLTFAnimation> anim = p_state->animations[p_index];

	String anim_name = anim->get_name();
	if (anim_name.is_empty()) {
		// No node represent these, and they are not in the hierarchy, so just make a unique name
		anim_name = _gen_unique_name(p_state, "Animation");
	}

	Ref<Animation> animation;
	animation.instantiate();
	animation->set_name(anim_name);
	animation->set_step(1.0 / p_state->get_bake_fps());

	if (anim->get_loop()) {
		animation->set_loop_mode(Animation::LOOP_LINEAR);
	}

	double anim_start = p_trimming ? INFINITY : 0.0;
	double anim_end = 0.0;

	for (const KeyValue<int, GLTFAnimation::Track> &track_i : anim->get_tracks()) {
		const GLTFAnimation::Track &track = track_i.value;
		//need to find the path: for skeletons, weight tracks will affect the mesh
		NodePath node_path;
		//for skeletons, transform tracks always affect bones
		NodePath transform_node_path;
		//for meshes, especially skinned meshes, there are cases where it will be added as a child
		NodePath mesh_instance_node_path;

		GLTFNodeIndex node_index = track_i.key;

		const Ref<GLTFNode> gltf_node = p_state->nodes[track_i.key];

		Node *root = p_animation_player->get_parent();
		ERR_FAIL_NULL(root);
		HashMap<GLTFNodeIndex, Node *>::Iterator node_element = p_state->scene_nodes.find(node_index);
		ERR_CONTINUE_MSG(!node_element, vformat("Unable to find node %d for animation.", node_index));
		node_path = root->get_path_to(node_element->value);
		HashMap<GLTFNodeIndex, ImporterMeshInstance3D *>::Iterator mesh_instance_element = p_state->scene_mesh_instances.find(node_index);
		if (mesh_instance_element) {
			mesh_instance_node_path = root->get_path_to(mesh_instance_element->value);
		} else {
			mesh_instance_node_path = node_path;
		}

		if (gltf_node->skeleton >= 0) {
			const Skeleton3D *sk = p_state->skeletons[gltf_node->skeleton]->godot_skeleton;
			ERR_FAIL_NULL(sk);

			const String path = p_animation_player->get_parent()->get_path_to(sk);
			const String bone = gltf_node->get_name();
			transform_node_path = path + ":" + bone;
		} else {
			transform_node_path = node_path;
		}

		if (p_trimming) {
			for (int i = 0; i < track.rotation_track.times.size(); i++) {
				anim_start = MIN(anim_start, track.rotation_track.times[i]);
				anim_end = MAX(anim_end, track.rotation_track.times[i]);
			}
			for (int i = 0; i < track.position_track.times.size(); i++) {
				anim_start = MIN(anim_start, track.position_track.times[i]);
				anim_end = MAX(anim_end, track.position_track.times[i]);
			}
			for (int i = 0; i < track.scale_track.times.size(); i++) {
				anim_start = MIN(anim_start, track.scale_track.times[i]);
				anim_end = MAX(anim_end, track.scale_track.times[i]);
			}
			for (int i = 0; i < track.weight_tracks.size(); i++) {
				for (int j = 0; j < track.weight_tracks[i].times.size(); j++) {
					anim_start = MIN(anim_start, track.weight_tracks[i].times[j]);
					anim_end = MAX(anim_end, track.weight_tracks[i].times[j]);
				}
			}
		} else {
			// If you don't use trimming and the first key time is not at 0.0, fake keys will be inserted.
			for (int i = 0; i < track.rotation_track.times.size(); i++) {
				anim_end = MAX(anim_end, track.rotation_track.times[i]);
			}
			for (int i = 0; i < track.position_track.times.size(); i++) {
				anim_end = MAX(anim_end, track.position_track.times[i]);
			}
			for (int i = 0; i < track.scale_track.times.size(); i++) {
				anim_end = MAX(anim_end, track.scale_track.times[i]);
			}
			for (int i = 0; i < track.weight_tracks.size(); i++) {
				for (int j = 0; j < track.weight_tracks[i].times.size(); j++) {
					anim_end = MAX(anim_end, track.weight_tracks[i].times[j]);
				}
			}
		}

		// Animated TRS properties will not affect a skinned mesh.
		const bool transform_affects_skinned_mesh_instance = gltf_node->skeleton < 0 && gltf_node->skin >= 0;
		if ((track.rotation_track.values.size() || track.position_track.values.size() || track.scale_track.values.size()) && !transform_affects_skinned_mesh_instance) {
			//make transform track
			int base_idx = animation->get_track_count();
			int position_idx = -1;
			int rotation_idx = -1;
			int scale_idx = -1;

			if (track.position_track.values.size()) {
				bool is_default = true; //discard the track if all it contains is default values
				if (p_remove_immutable_tracks) {
					Vector3 base_pos = gltf_node->get_position();
					for (int i = 0; i < track.position_track.times.size(); i++) {
						int value_index = track.position_track.interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE ? (1 + i * 3) : i;
						ERR_FAIL_COND_MSG(value_index >= track.position_track.values.size(), "Animation sampler output accessor with 'CUBICSPLINE' interpolation doesn't have enough elements.");
						Vector3 value = track.position_track.values[value_index];
						if (!value.is_equal_approx(base_pos)) {
							is_default = false;
							break;
						}
					}
				}
				if (!p_remove_immutable_tracks || !is_default) {
					position_idx = base_idx;
					animation->add_track(Animation::TYPE_POSITION_3D);
					animation->track_set_path(position_idx, transform_node_path);
					animation->track_set_imported(position_idx, true); //helps merging later
					if (track.position_track.interpolation == GLTFAnimation::INTERP_STEP) {
						animation->track_set_interpolation_type(position_idx, Animation::InterpolationType::INTERPOLATION_NEAREST);
					}
					base_idx++;
				}
			}
			if (track.rotation_track.values.size()) {
				bool is_default = true; //discard the track if all it contains is default values
				if (p_remove_immutable_tracks) {
					Quaternion base_rot = gltf_node->get_rotation();
					for (int i = 0; i < track.rotation_track.times.size(); i++) {
						int value_index = track.rotation_track.interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE ? (1 + i * 3) : i;
						ERR_FAIL_COND_MSG(value_index >= track.rotation_track.values.size(), "Animation sampler output accessor with 'CUBICSPLINE' interpolation doesn't have enough elements.");
						Quaternion value = track.rotation_track.values[value_index].normalized();
						if (!value.is_equal_approx(base_rot)) {
							is_default = false;
							break;
						}
					}
				}
				if (!p_remove_immutable_tracks || !is_default) {
					rotation_idx = base_idx;
					animation->add_track(Animation::TYPE_ROTATION_3D);
					animation->track_set_path(rotation_idx, transform_node_path);
					animation->track_set_imported(rotation_idx, true); //helps merging later
					if (track.rotation_track.interpolation == GLTFAnimation::INTERP_STEP) {
						animation->track_set_interpolation_type(rotation_idx, Animation::InterpolationType::INTERPOLATION_NEAREST);
					}
					base_idx++;
				}
			}
			if (track.scale_track.values.size()) {
				bool is_default = true; //discard the track if all it contains is default values
				if (p_remove_immutable_tracks) {
					Vector3 base_scale = gltf_node->get_scale();
					for (int i = 0; i < track.scale_track.times.size(); i++) {
						int value_index = track.scale_track.interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE ? (1 + i * 3) : i;
						ERR_FAIL_COND_MSG(value_index >= track.scale_track.values.size(), "Animation sampler output accessor with 'CUBICSPLINE' interpolation doesn't have enough elements.");
						Vector3 value = track.scale_track.values[value_index];
						if (!value.is_equal_approx(base_scale)) {
							is_default = false;
							break;
						}
					}
				}
				if (!p_remove_immutable_tracks || !is_default) {
					scale_idx = base_idx;
					animation->add_track(Animation::TYPE_SCALE_3D);
					animation->track_set_path(scale_idx, transform_node_path);
					animation->track_set_imported(scale_idx, true); //helps merging later
					if (track.scale_track.interpolation == GLTFAnimation::INTERP_STEP) {
						animation->track_set_interpolation_type(scale_idx, Animation::InterpolationType::INTERPOLATION_NEAREST);
					}
					base_idx++;
				}
			}

			const double increment = 1.0 / p_state->get_bake_fps();
			double time = anim_start;

			Vector3 base_pos;
			Quaternion base_rot;
			Vector3 base_scale = Vector3(1, 1, 1);

			if (rotation_idx == -1) {
				base_rot = gltf_node->get_rotation();
			}

			if (position_idx == -1) {
				base_pos = gltf_node->get_position();
			}

			if (scale_idx == -1) {
				base_scale = gltf_node->get_scale();
			}

			bool last = false;
			while (true) {
				Vector3 pos = base_pos;
				Quaternion rot = base_rot;
				Vector3 scale = base_scale;

				if (position_idx >= 0) {
					pos = _interpolate_track<Vector3>(track.position_track.times, track.position_track.values, time, track.position_track.interpolation);
					animation->position_track_insert_key(position_idx, time - anim_start, pos);
				}

				if (rotation_idx >= 0) {
					rot = _interpolate_track<Quaternion>(track.rotation_track.times, track.rotation_track.values, time, track.rotation_track.interpolation);
					animation->rotation_track_insert_key(rotation_idx, time - anim_start, rot);
				}

				if (scale_idx >= 0) {
					scale = _interpolate_track<Vector3>(track.scale_track.times, track.scale_track.values, time, track.scale_track.interpolation);
					animation->scale_track_insert_key(scale_idx, time - anim_start, scale);
				}

				if (last) {
					break;
				}
				time += increment;
				if (time >= anim_end) {
					last = true;
					time = anim_end;
				}
			}
		}

		for (int i = 0; i < track.weight_tracks.size(); i++) {
			ERR_CONTINUE(gltf_node->mesh < 0 || gltf_node->mesh >= p_state->meshes.size());
			Ref<GLTFMesh> mesh = p_state->meshes[gltf_node->mesh];
			ERR_CONTINUE(mesh.is_null());
			ERR_CONTINUE(mesh->get_mesh().is_null());
			ERR_CONTINUE(mesh->get_mesh()->get_mesh().is_null());

			const String blend_path = String(mesh_instance_node_path) + ":" + String(mesh->get_mesh()->get_blend_shape_name(i));

			const int track_idx = animation->get_track_count();
			animation->add_track(Animation::TYPE_BLEND_SHAPE);
			animation->track_set_path(track_idx, blend_path);
			animation->track_set_imported(track_idx, true); //helps merging later

			// Only LINEAR and STEP (NEAREST) can be supported out of the box by Godot's Animation,
			// the other modes have to be baked.
			GLTFAnimation::Interpolation gltf_interp = track.weight_tracks[i].interpolation;
			if (gltf_interp == GLTFAnimation::INTERP_LINEAR || gltf_interp == GLTFAnimation::INTERP_STEP) {
				animation->track_set_interpolation_type(track_idx, gltf_interp == GLTFAnimation::INTERP_STEP ? Animation::INTERPOLATION_NEAREST : Animation::INTERPOLATION_LINEAR);
				for (int j = 0; j < track.weight_tracks[i].times.size(); j++) {
					const float t = track.weight_tracks[i].times[j];
					const float attribs = track.weight_tracks[i].values[j];
					animation->blend_shape_track_insert_key(track_idx, t, attribs);
				}
			} else {
				// CATMULLROMSPLINE or CUBIC_SPLINE have to be baked, apologies.
				const double increment = 1.0 / p_state->get_bake_fps();
				double time = 0.0;
				bool last = false;
				while (true) {
					real_t blend = _interpolate_track<real_t>(track.weight_tracks[i].times, track.weight_tracks[i].values, time, gltf_interp);
					animation->blend_shape_track_insert_key(track_idx, time - anim_start, blend);
					if (last) {
						break;
					}
					time += increment;
					if (time >= anim_end) {
						last = true;
						time = anim_end;
					}
				}
			}
		}
	}

	animation->set_length(anim_end - anim_start);

	Ref<AnimationLibrary> library;
	if (!p_animation_player->has_animation_library("")) {
		library.instantiate();
		p_animation_player->add_animation_library("", library);
	} else {
		library = p_animation_player->get_animation_library("");
	}
	library->add_animation(anim_name, animation);
}

void GLTFDocument::_convert_mesh_instances(Ref<GLTFState> p_state) {
	for (GLTFNodeIndex mi_node_i = 0; mi_node_i < p_state->nodes.size(); ++mi_node_i) {
		Ref<GLTFNode> node = p_state->nodes[mi_node_i];

		if (node->mesh < 0) {
			continue;
		}
		HashMap<GLTFNodeIndex, Node *>::Iterator mi_element = p_state->scene_nodes.find(mi_node_i);
		if (!mi_element) {
			continue;
		}
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(mi_element->value);
		if (!mi) {
			continue;
		}
		node->transform = mi->get_transform();

		Node *skel_node = mi->get_node_or_null(mi->get_skeleton_path());
		Skeleton3D *godot_skeleton = Object::cast_to<Skeleton3D>(skel_node);
		if (!godot_skeleton || godot_skeleton->get_bone_count() == 0) {
			continue;
		}
		// At this point in the code, we know we have a Skeleton3D with at least one bone.
		Ref<Skin> skin = mi->get_skin();
		Ref<GLTFSkin> gltf_skin;
		gltf_skin.instantiate();
		Array json_joints;
		if (p_state->skeleton3d_to_gltf_skeleton.has(godot_skeleton->get_instance_id())) {
			// This is a skinned mesh. If the mesh has no ARRAY_WEIGHTS or ARRAY_BONES, it will be invisible.
			const GLTFSkeletonIndex skeleton_gltf_i = p_state->skeleton3d_to_gltf_skeleton[godot_skeleton->get_instance_id()];
			Ref<GLTFSkeleton> gltf_skeleton = p_state->skeletons[skeleton_gltf_i];
			int bone_cnt = godot_skeleton->get_bone_count();
			ERR_FAIL_COND(bone_cnt != gltf_skeleton->joints.size());

			ObjectID gltf_skin_key;
			if (skin.is_valid()) {
				gltf_skin_key = skin->get_instance_id();
			}
			ObjectID gltf_skel_key = godot_skeleton->get_instance_id();
			GLTFSkinIndex skin_gltf_i = -1;
			GLTFNodeIndex root_gltf_i = -1;
			if (!gltf_skeleton->roots.is_empty()) {
				root_gltf_i = gltf_skeleton->roots[0];
			}
			if (p_state->skin_and_skeleton3d_to_gltf_skin.has(gltf_skin_key) && p_state->skin_and_skeleton3d_to_gltf_skin[gltf_skin_key].has(gltf_skel_key)) {
				skin_gltf_i = p_state->skin_and_skeleton3d_to_gltf_skin[gltf_skin_key][gltf_skel_key];
			} else {
				if (skin.is_null()) {
					// Note that gltf_skin_key should remain null, so these can share a reference.
					skin = godot_skeleton->create_skin_from_rest_transforms();
				}
				gltf_skin.instantiate();
				gltf_skin->godot_skin = skin;
				gltf_skin->set_name(skin->get_name());
				gltf_skin->skeleton = skeleton_gltf_i;
				gltf_skin->skin_root = root_gltf_i;
				//gltf_state->godot_to_gltf_node[skel_node]
				HashMap<StringName, int> bone_name_to_idx;
				for (int bone_i = 0; bone_i < bone_cnt; bone_i++) {
					bone_name_to_idx[godot_skeleton->get_bone_name(bone_i)] = bone_i;
				}
				for (int bind_i = 0, cnt = skin->get_bind_count(); bind_i < cnt; bind_i++) {
					int bone_i = skin->get_bind_bone(bind_i);
					Transform3D bind_pose = skin->get_bind_pose(bind_i);
					StringName bind_name = skin->get_bind_name(bind_i);
					if (bind_name != StringName()) {
						bone_i = bone_name_to_idx[bind_name];
					}
					ERR_CONTINUE(bone_i < 0 || bone_i >= bone_cnt);
					if (bind_name == StringName()) {
						bind_name = godot_skeleton->get_bone_name(bone_i);
					}
					GLTFNodeIndex skeleton_bone_i = gltf_skeleton->joints[bone_i];
					gltf_skin->joints_original.push_back(skeleton_bone_i);
					gltf_skin->joints.push_back(skeleton_bone_i);
					gltf_skin->inverse_binds.push_back(bind_pose);
					if (godot_skeleton->get_bone_parent(bone_i) == -1) {
						gltf_skin->roots.push_back(skeleton_bone_i);
					}
					gltf_skin->joint_i_to_bone_i[bind_i] = bone_i;
					gltf_skin->joint_i_to_name[bind_i] = bind_name;
				}
				skin_gltf_i = p_state->skins.size();
				p_state->skins.push_back(gltf_skin);
				p_state->skin_and_skeleton3d_to_gltf_skin[gltf_skin_key][gltf_skel_key] = skin_gltf_i;
			}
			node->skin = skin_gltf_i;
			node->skeleton = skeleton_gltf_i;
		}
	}
}

float GLTFDocument::solve_metallic(float p_dielectric_specular, float p_diffuse, float p_specular, float p_one_minus_specular_strength) {
	if (p_specular <= p_dielectric_specular) {
		return 0.0f;
	}

	const float a = p_dielectric_specular;
	const float b = p_diffuse * p_one_minus_specular_strength / (1.0f - p_dielectric_specular) + p_specular - 2.0f * p_dielectric_specular;
	const float c = p_dielectric_specular - p_specular;
	const float D = b * b - 4.0f * a * c;
	return CLAMP((-b + Math::sqrt(D)) / (2.0f * a), 0.0f, 1.0f);
}

float GLTFDocument::get_perceived_brightness(const Color p_color) {
	const Color coeff = Color(R_BRIGHTNESS_COEFF, G_BRIGHTNESS_COEFF, B_BRIGHTNESS_COEFF);
	const Color value = coeff * (p_color * p_color);

	const float r = value.r;
	const float g = value.g;
	const float b = value.b;

	return Math::sqrt(r + g + b);
}

float GLTFDocument::get_max_component(const Color &p_color) {
	const float r = p_color.r;
	const float g = p_color.g;
	const float b = p_color.b;

	return MAX(MAX(r, g), b);
}

void GLTFDocument::_process_mesh_instances(Ref<GLTFState> p_state, Node *p_scene_root) {
	for (GLTFNodeIndex node_i = 0; node_i < p_state->nodes.size(); ++node_i) {
		Ref<GLTFNode> node = p_state->nodes[node_i];

		if (node->skin >= 0 && node->mesh >= 0) {
			const GLTFSkinIndex skin_i = node->skin;

			ImporterMeshInstance3D *mi = nullptr;
			HashMap<GLTFNodeIndex, ImporterMeshInstance3D *>::Iterator mi_element = p_state->scene_mesh_instances.find(node_i);
			if (mi_element) {
				mi = mi_element->value;
			} else {
				HashMap<GLTFNodeIndex, Node *>::Iterator si_element = p_state->scene_nodes.find(node_i);
				ERR_CONTINUE_MSG(!si_element, vformat("Unable to find node %d", node_i));
				mi = Object::cast_to<ImporterMeshInstance3D>(si_element->value);
				ERR_CONTINUE_MSG(mi == nullptr, vformat("Unable to cast node %d of type %s to ImporterMeshInstance3D", node_i, si_element->value->get_class_name()));
			}

			const GLTFSkeletonIndex skel_i = p_state->skins.write[node->skin]->skeleton;
			Ref<GLTFSkeleton> gltf_skeleton = p_state->skeletons.write[skel_i];
			Skeleton3D *skeleton = gltf_skeleton->godot_skeleton;
			ERR_CONTINUE_MSG(skeleton == nullptr, vformat("Unable to find Skeleton for node %d skin %d", node_i, skin_i));

			mi->get_parent()->remove_child(mi);
			mi->set_owner(nullptr);
			skeleton->add_child(mi, true);
			mi->set_owner(p_scene_root);

			mi->set_skin(p_state->skins.write[skin_i]->godot_skin);
			mi->set_skeleton_path(mi->get_path_to(skeleton));
			mi->set_transform(Transform3D());
		}
	}
}

GLTFAnimation::Track GLTFDocument::_convert_animation_track(Ref<GLTFState> p_state, GLTFAnimation::Track p_track, Ref<Animation> p_animation, int32_t p_track_i, GLTFNodeIndex p_node_i) {
	Animation::InterpolationType interpolation = p_animation->track_get_interpolation_type(p_track_i);

	GLTFAnimation::Interpolation gltf_interpolation = GLTFAnimation::INTERP_LINEAR;
	if (interpolation == Animation::InterpolationType::INTERPOLATION_LINEAR) {
		gltf_interpolation = GLTFAnimation::INTERP_LINEAR;
	} else if (interpolation == Animation::InterpolationType::INTERPOLATION_NEAREST) {
		gltf_interpolation = GLTFAnimation::INTERP_STEP;
	} else if (interpolation == Animation::InterpolationType::INTERPOLATION_CUBIC) {
		gltf_interpolation = GLTFAnimation::INTERP_CUBIC_SPLINE;
	}
	Animation::TrackType track_type = p_animation->track_get_type(p_track_i);
	int32_t key_count = p_animation->track_get_key_count(p_track_i);
	Vector<real_t> times;
	times.resize(key_count);
	String path = p_animation->track_get_path(p_track_i);
	for (int32_t key_i = 0; key_i < key_count; key_i++) {
		times.write[key_i] = p_animation->track_get_key_time(p_track_i, key_i);
	}
	double anim_end = p_animation->get_length();
	if (track_type == Animation::TYPE_SCALE_3D) {
		if (gltf_interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE) {
			gltf_interpolation = GLTFAnimation::INTERP_LINEAR;
			p_track.scale_track.times.clear();
			p_track.scale_track.values.clear();
			// CATMULLROMSPLINE or CUBIC_SPLINE have to be baked, apologies.
			const double increment = 1.0 / p_state->get_bake_fps();
			double time = 0.0;
			bool last = false;
			while (true) {
				Vector3 scale;
				Error err = p_animation->try_scale_track_interpolate(p_track_i, time, &scale);
				ERR_CONTINUE(err != OK);
				p_track.scale_track.values.push_back(scale);
				p_track.scale_track.times.push_back(time);
				if (last) {
					break;
				}
				time += increment;
				if (time >= anim_end) {
					last = true;
					time = anim_end;
				}
			}
		} else {
			p_track.scale_track.times = times;
			p_track.scale_track.interpolation = gltf_interpolation;
			p_track.scale_track.values.resize(key_count);
			for (int32_t key_i = 0; key_i < key_count; key_i++) {
				Vector3 scale;
				Error err = p_animation->scale_track_get_key(p_track_i, key_i, &scale);
				ERR_CONTINUE(err != OK);
				p_track.scale_track.values.write[key_i] = scale;
			}
		}
	} else if (track_type == Animation::TYPE_POSITION_3D) {
		if (gltf_interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE) {
			gltf_interpolation = GLTFAnimation::INTERP_LINEAR;
			p_track.position_track.times.clear();
			p_track.position_track.values.clear();
			// CATMULLROMSPLINE or CUBIC_SPLINE have to be baked, apologies.
			const double increment = 1.0 / p_state->get_bake_fps();
			double time = 0.0;
			bool last = false;
			while (true) {
				Vector3 scale;
				Error err = p_animation->try_position_track_interpolate(p_track_i, time, &scale);
				ERR_CONTINUE(err != OK);
				p_track.position_track.values.push_back(scale);
				p_track.position_track.times.push_back(time);
				if (last) {
					break;
				}
				time += increment;
				if (time >= anim_end) {
					last = true;
					time = anim_end;
				}
			}
		} else {
			p_track.position_track.times = times;
			p_track.position_track.values.resize(key_count);
			p_track.position_track.interpolation = gltf_interpolation;
			for (int32_t key_i = 0; key_i < key_count; key_i++) {
				Vector3 position;
				Error err = p_animation->position_track_get_key(p_track_i, key_i, &position);
				ERR_CONTINUE(err != OK);
				p_track.position_track.values.write[key_i] = position;
			}
		}
	} else if (track_type == Animation::TYPE_ROTATION_3D) {
		if (gltf_interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE) {
			gltf_interpolation = GLTFAnimation::INTERP_LINEAR;
			p_track.rotation_track.times.clear();
			p_track.rotation_track.values.clear();
			// CATMULLROMSPLINE or CUBIC_SPLINE have to be baked, apologies.
			const double increment = 1.0 / p_state->get_bake_fps();
			double time = 0.0;
			bool last = false;
			while (true) {
				Quaternion rotation;
				Error err = p_animation->try_rotation_track_interpolate(p_track_i, time, &rotation);
				ERR_CONTINUE(err != OK);
				p_track.rotation_track.values.push_back(rotation);
				p_track.rotation_track.times.push_back(time);
				if (last) {
					break;
				}
				time += increment;
				if (time >= anim_end) {
					last = true;
					time = anim_end;
				}
			}
		} else {
			p_track.rotation_track.times = times;
			p_track.rotation_track.values.resize(key_count);
			p_track.rotation_track.interpolation = gltf_interpolation;
			for (int32_t key_i = 0; key_i < key_count; key_i++) {
				Quaternion rotation;
				Error err = p_animation->rotation_track_get_key(p_track_i, key_i, &rotation);
				ERR_CONTINUE(err != OK);
				p_track.rotation_track.values.write[key_i] = rotation;
			}
		}
	} else if (track_type == Animation::TYPE_VALUE) {
		if (path.contains(":position")) {
			p_track.position_track.interpolation = gltf_interpolation;
			p_track.position_track.times = times;
			p_track.position_track.values.resize(key_count);

			if (gltf_interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE) {
				gltf_interpolation = GLTFAnimation::INTERP_LINEAR;
				p_track.position_track.times.clear();
				p_track.position_track.values.clear();
				// CATMULLROMSPLINE or CUBIC_SPLINE have to be baked, apologies.
				const double increment = 1.0 / p_state->get_bake_fps();
				double time = 0.0;
				bool last = false;
				while (true) {
					Vector3 position;
					Error err = p_animation->try_position_track_interpolate(p_track_i, time, &position);
					ERR_CONTINUE(err != OK);
					p_track.position_track.values.push_back(position);
					p_track.position_track.times.push_back(time);
					if (last) {
						break;
					}
					time += increment;
					if (time >= anim_end) {
						last = true;
						time = anim_end;
					}
				}
			} else {
				for (int32_t key_i = 0; key_i < key_count; key_i++) {
					Vector3 position = p_animation->track_get_key_value(p_track_i, key_i);
					p_track.position_track.values.write[key_i] = position;
				}
			}
		} else if (path.contains(":rotation")) {
			p_track.rotation_track.interpolation = gltf_interpolation;
			p_track.rotation_track.times = times;
			p_track.rotation_track.values.resize(key_count);
			if (gltf_interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE) {
				gltf_interpolation = GLTFAnimation::INTERP_LINEAR;
				p_track.rotation_track.times.clear();
				p_track.rotation_track.values.clear();
				// CATMULLROMSPLINE or CUBIC_SPLINE have to be baked, apologies.
				const double increment = 1.0 / p_state->get_bake_fps();
				double time = 0.0;
				bool last = false;
				while (true) {
					Quaternion rotation;
					Error err = p_animation->try_rotation_track_interpolate(p_track_i, time, &rotation);
					ERR_CONTINUE(err != OK);
					p_track.rotation_track.values.push_back(rotation);
					p_track.rotation_track.times.push_back(time);
					if (last) {
						break;
					}
					time += increment;
					if (time >= anim_end) {
						last = true;
						time = anim_end;
					}
				}
			} else {
				for (int32_t key_i = 0; key_i < key_count; key_i++) {
					Vector3 rotation_radian = p_animation->track_get_key_value(p_track_i, key_i);
					p_track.rotation_track.values.write[key_i] = Quaternion::from_euler(rotation_radian);
				}
			}
		} else if (path.contains(":scale")) {
			p_track.scale_track.times = times;
			p_track.scale_track.interpolation = gltf_interpolation;

			p_track.scale_track.values.resize(key_count);
			p_track.scale_track.interpolation = gltf_interpolation;

			if (gltf_interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE) {
				gltf_interpolation = GLTFAnimation::INTERP_LINEAR;
				p_track.scale_track.times.clear();
				p_track.scale_track.values.clear();
				// CATMULLROMSPLINE or CUBIC_SPLINE have to be baked, apologies.
				const double increment = 1.0 / p_state->get_bake_fps();
				double time = 0.0;
				bool last = false;
				while (true) {
					Vector3 scale;
					Error err = p_animation->try_scale_track_interpolate(p_track_i, time, &scale);
					ERR_CONTINUE(err != OK);
					p_track.scale_track.values.push_back(scale);
					p_track.scale_track.times.push_back(time);
					if (last) {
						break;
					}
					time += increment;
					if (time >= anim_end) {
						last = true;
						time = anim_end;
					}
				}
			} else {
				for (int32_t key_i = 0; key_i < key_count; key_i++) {
					Vector3 scale_track = p_animation->track_get_key_value(p_track_i, key_i);
					p_track.scale_track.values.write[key_i] = scale_track;
				}
			}
		}
	} else if (track_type == Animation::TYPE_BEZIER) {
		const int32_t keys = anim_end * p_state->get_bake_fps();
		if (path.contains(":scale")) {
			if (!p_track.scale_track.times.size()) {
				p_track.scale_track.interpolation = gltf_interpolation;
				Vector<real_t> new_times;
				new_times.resize(keys);
				for (int32_t key_i = 0; key_i < keys; key_i++) {
					new_times.write[key_i] = key_i / p_state->get_bake_fps();
				}
				p_track.scale_track.times = new_times;

				p_track.scale_track.values.resize(keys);

				for (int32_t key_i = 0; key_i < keys; key_i++) {
					p_track.scale_track.values.write[key_i] = Vector3(1.0f, 1.0f, 1.0f);
				}

				for (int32_t key_i = 0; key_i < keys; key_i++) {
					Vector3 bezier_track = p_track.scale_track.values[key_i];
					if (path.contains(":scale:x")) {
						bezier_track.x = p_animation->bezier_track_interpolate(p_track_i, key_i / p_state->get_bake_fps());
					} else if (path.contains(":scale:y")) {
						bezier_track.y = p_animation->bezier_track_interpolate(p_track_i, key_i / p_state->get_bake_fps());
					} else if (path.contains(":scale:z")) {
						bezier_track.z = p_animation->bezier_track_interpolate(p_track_i, key_i / p_state->get_bake_fps());
					}
					p_track.scale_track.values.write[key_i] = bezier_track;
				}
			}
		} else if (path.contains(":position")) {
			if (!p_track.position_track.times.size()) {
				p_track.position_track.interpolation = gltf_interpolation;
				Vector<real_t> new_times;
				new_times.resize(keys);
				for (int32_t key_i = 0; key_i < keys; key_i++) {
					new_times.write[key_i] = key_i / p_state->get_bake_fps();
				}
				p_track.position_track.times = new_times;

				p_track.position_track.values.resize(keys);
			}

			for (int32_t key_i = 0; key_i < keys; key_i++) {
				Vector3 bezier_track = p_track.position_track.values[key_i];
				if (path.contains(":position:x")) {
					bezier_track.x = p_animation->bezier_track_interpolate(p_track_i, key_i / p_state->get_bake_fps());
				} else if (path.contains(":position:y")) {
					bezier_track.y = p_animation->bezier_track_interpolate(p_track_i, key_i / p_state->get_bake_fps());
				} else if (path.contains(":position:z")) {
					bezier_track.z = p_animation->bezier_track_interpolate(p_track_i, key_i / p_state->get_bake_fps());
				}
				p_track.position_track.values.write[key_i] = bezier_track;
			}
		} else if (path.contains(":rotation")) {
			if (!p_track.rotation_track.times.size()) {
				p_track.rotation_track.interpolation = gltf_interpolation;
				Vector<real_t> new_times;
				new_times.resize(keys);
				for (int32_t key_i = 0; key_i < keys; key_i++) {
					new_times.write[key_i] = key_i / p_state->get_bake_fps();
				}
				p_track.rotation_track.times = new_times;

				p_track.rotation_track.values.resize(keys);
			}
			for (int32_t key_i = 0; key_i < keys; key_i++) {
				Quaternion bezier_track = p_track.rotation_track.values[key_i];
				if (path.contains(":rotation:x")) {
					bezier_track.x = p_animation->bezier_track_interpolate(p_track_i, key_i / p_state->get_bake_fps());
				} else if (path.contains(":rotation:y")) {
					bezier_track.y = p_animation->bezier_track_interpolate(p_track_i, key_i / p_state->get_bake_fps());
				} else if (path.contains(":rotation:z")) {
					bezier_track.z = p_animation->bezier_track_interpolate(p_track_i, key_i / p_state->get_bake_fps());
				} else if (path.contains(":rotation:w")) {
					bezier_track.w = p_animation->bezier_track_interpolate(p_track_i, key_i / p_state->get_bake_fps());
				}
				p_track.rotation_track.values.write[key_i] = bezier_track;
			}
		}
	}
	return p_track;
}

void GLTFDocument::_convert_animation(Ref<GLTFState> p_state, AnimationPlayer *p_animation_player, String p_animation_track_name) {
	Ref<Animation> animation = p_animation_player->get_animation(p_animation_track_name);
	Ref<GLTFAnimation> gltf_animation;
	gltf_animation.instantiate();
	gltf_animation->set_original_name(p_animation_track_name);
	gltf_animation->set_name(_gen_unique_name(p_state, p_animation_track_name));
	for (int32_t track_i = 0; track_i < animation->get_track_count(); track_i++) {
		if (!animation->track_is_enabled(track_i)) {
			continue;
		}
		String final_track_path = animation->track_get_path(track_i);
		Node *animation_base_node = p_animation_player->get_parent();
		ERR_CONTINUE_MSG(!animation_base_node, "Cannot get the parent of the animation player.");
		if (String(final_track_path).contains(":position")) {
			const Vector<String> node_suffix = String(final_track_path).split(":position");
			const NodePath path = node_suffix[0];
			const Node *node = animation_base_node->get_node_or_null(path);
			ERR_CONTINUE_MSG(!node, "Cannot get the node from a position path.");
			for (const KeyValue<GLTFNodeIndex, Node *> &position_scene_node_i : p_state->scene_nodes) {
				if (position_scene_node_i.value == node) {
					GLTFNodeIndex node_index = position_scene_node_i.key;
					HashMap<int, GLTFAnimation::Track>::Iterator position_track_i = gltf_animation->get_tracks().find(node_index);
					GLTFAnimation::Track track;
					if (position_track_i) {
						track = position_track_i->value;
					}
					track = _convert_animation_track(p_state, track, animation, track_i, node_index);
					gltf_animation->get_tracks().insert(node_index, track);
				}
			}
		} else if (String(final_track_path).contains(":rotation_degrees")) {
			const Vector<String> node_suffix = String(final_track_path).split(":rotation_degrees");
			const NodePath path = node_suffix[0];
			const Node *node = animation_base_node->get_node_or_null(path);
			ERR_CONTINUE_MSG(!node, "Cannot get the node from a rotation degrees path.");
			for (const KeyValue<GLTFNodeIndex, Node *> &rotation_degree_scene_node_i : p_state->scene_nodes) {
				if (rotation_degree_scene_node_i.value == node) {
					GLTFNodeIndex node_index = rotation_degree_scene_node_i.key;
					HashMap<int, GLTFAnimation::Track>::Iterator rotation_degree_track_i = gltf_animation->get_tracks().find(node_index);
					GLTFAnimation::Track track;
					if (rotation_degree_track_i) {
						track = rotation_degree_track_i->value;
					}
					track = _convert_animation_track(p_state, track, animation, track_i, node_index);
					gltf_animation->get_tracks().insert(node_index, track);
				}
			}
		} else if (String(final_track_path).contains(":scale")) {
			const Vector<String> node_suffix = String(final_track_path).split(":scale");
			const NodePath path = node_suffix[0];
			const Node *node = animation_base_node->get_node_or_null(path);
			ERR_CONTINUE_MSG(!node, "Cannot get the node from a scale path.");
			for (const KeyValue<GLTFNodeIndex, Node *> &scale_scene_node_i : p_state->scene_nodes) {
				if (scale_scene_node_i.value == node) {
					GLTFNodeIndex node_index = scale_scene_node_i.key;
					HashMap<int, GLTFAnimation::Track>::Iterator scale_track_i = gltf_animation->get_tracks().find(node_index);
					GLTFAnimation::Track track;
					if (scale_track_i) {
						track = scale_track_i->value;
					}
					track = _convert_animation_track(p_state, track, animation, track_i, node_index);
					gltf_animation->get_tracks().insert(node_index, track);
				}
			}
		} else if (String(final_track_path).contains(":transform")) {
			const Vector<String> node_suffix = String(final_track_path).split(":transform");
			const NodePath path = node_suffix[0];
			const Node *node = animation_base_node->get_node_or_null(path);
			ERR_CONTINUE_MSG(!node, "Cannot get the node from a transform path.");
			for (const KeyValue<GLTFNodeIndex, Node *> &transform_track_i : p_state->scene_nodes) {
				if (transform_track_i.value == node) {
					GLTFAnimation::Track track;
					track = _convert_animation_track(p_state, track, animation, track_i, transform_track_i.key);
					gltf_animation->get_tracks().insert(transform_track_i.key, track);
				}
			}
		} else if (String(final_track_path).contains(":") && animation->track_get_type(track_i) == Animation::TYPE_BLEND_SHAPE) {
			const Vector<String> node_suffix = String(final_track_path).split(":");
			const NodePath path = node_suffix[0];
			const String suffix = node_suffix[1];
			Node *node = animation_base_node->get_node_or_null(path);
			ERR_CONTINUE_MSG(!node, "Cannot get the node from a blend shape path.");
			MeshInstance3D *mi = cast_to<MeshInstance3D>(node);
			if (!mi) {
				continue;
			}
			Ref<Mesh> mesh = mi->get_mesh();
			ERR_CONTINUE(mesh.is_null());
			int32_t mesh_index = -1;
			for (const KeyValue<GLTFNodeIndex, Node *> &mesh_track_i : p_state->scene_nodes) {
				if (mesh_track_i.value == node) {
					mesh_index = mesh_track_i.key;
				}
			}
			ERR_CONTINUE(mesh_index == -1);
			HashMap<int, GLTFAnimation::Track> &tracks = gltf_animation->get_tracks();
			GLTFAnimation::Track track = gltf_animation->get_tracks().has(mesh_index) ? gltf_animation->get_tracks()[mesh_index] : GLTFAnimation::Track();
			if (!tracks.has(mesh_index)) {
				for (int32_t shape_i = 0; shape_i < mesh->get_blend_shape_count(); shape_i++) {
					String shape_name = mesh->get_blend_shape_name(shape_i);
					NodePath shape_path = String(path) + ":" + shape_name;
					int32_t shape_track_i = animation->find_track(shape_path, Animation::TYPE_BLEND_SHAPE);
					if (shape_track_i == -1) {
						GLTFAnimation::Channel<real_t> weight;
						weight.interpolation = GLTFAnimation::INTERP_LINEAR;
						weight.times.push_back(0.0f);
						weight.times.push_back(0.0f);
						weight.values.push_back(0.0f);
						weight.values.push_back(0.0f);
						track.weight_tracks.push_back(weight);
						continue;
					}
					Animation::InterpolationType interpolation = animation->track_get_interpolation_type(track_i);
					GLTFAnimation::Interpolation gltf_interpolation = GLTFAnimation::INTERP_LINEAR;
					if (interpolation == Animation::InterpolationType::INTERPOLATION_LINEAR) {
						gltf_interpolation = GLTFAnimation::INTERP_LINEAR;
					} else if (interpolation == Animation::InterpolationType::INTERPOLATION_NEAREST) {
						gltf_interpolation = GLTFAnimation::INTERP_STEP;
					} else if (interpolation == Animation::InterpolationType::INTERPOLATION_CUBIC) {
						gltf_interpolation = GLTFAnimation::INTERP_CUBIC_SPLINE;
					}
					int32_t key_count = animation->track_get_key_count(shape_track_i);
					GLTFAnimation::Channel<real_t> weight;
					weight.interpolation = gltf_interpolation;
					weight.times.resize(key_count);
					for (int32_t time_i = 0; time_i < key_count; time_i++) {
						weight.times.write[time_i] = animation->track_get_key_time(shape_track_i, time_i);
					}
					weight.values.resize(key_count);
					for (int32_t value_i = 0; value_i < key_count; value_i++) {
						weight.values.write[value_i] = animation->track_get_key_value(shape_track_i, value_i);
					}
					track.weight_tracks.push_back(weight);
				}
				tracks[mesh_index] = track;
			}
		} else if (String(final_track_path).contains(":")) {
			//Process skeleton
			const Vector<String> node_suffix = String(final_track_path).split(":");
			const String &node = node_suffix[0];
			const NodePath node_path = node;
			const String &suffix = node_suffix[1];
			Node *godot_node = animation_base_node->get_node_or_null(node_path);
			if (!godot_node) {
				continue;
			}
			Skeleton3D *skeleton = cast_to<Skeleton3D>(animation_base_node->get_node_or_null(node));
			if (!skeleton) {
				continue;
			}
			GLTFSkeletonIndex skeleton_gltf_i = -1;
			for (GLTFSkeletonIndex skeleton_i = 0; skeleton_i < p_state->skeletons.size(); skeleton_i++) {
				if (p_state->skeletons[skeleton_i]->godot_skeleton == cast_to<Skeleton3D>(godot_node)) {
					skeleton = p_state->skeletons[skeleton_i]->godot_skeleton;
					skeleton_gltf_i = skeleton_i;
					ERR_CONTINUE(!skeleton);
					Ref<GLTFSkeleton> skeleton_gltf = p_state->skeletons[skeleton_gltf_i];
					int32_t bone = skeleton->find_bone(suffix);
					ERR_CONTINUE_MSG(bone == -1, vformat("Cannot find the bone %s.", suffix));
					if (!skeleton_gltf->godot_bone_node.has(bone)) {
						continue;
					}
					GLTFNodeIndex node_i = skeleton_gltf->godot_bone_node[bone];
					HashMap<int, GLTFAnimation::Track>::Iterator property_track_i = gltf_animation->get_tracks().find(node_i);
					GLTFAnimation::Track track;
					if (property_track_i) {
						track = property_track_i->value;
					}
					track = _convert_animation_track(p_state, track, animation, track_i, node_i);
					gltf_animation->get_tracks()[node_i] = track;
				}
			}
		} else if (!String(final_track_path).contains(":")) {
			ERR_CONTINUE(!animation_base_node);
			Node *godot_node = animation_base_node->get_node_or_null(final_track_path);
			ERR_CONTINUE_MSG(!godot_node, vformat("Cannot get the node from a skeleton path %s.", final_track_path));
			for (const KeyValue<GLTFNodeIndex, Node *> &scene_node_i : p_state->scene_nodes) {
				if (scene_node_i.value == godot_node) {
					GLTFNodeIndex node_i = scene_node_i.key;
					HashMap<int, GLTFAnimation::Track>::Iterator node_track_i = gltf_animation->get_tracks().find(node_i);
					GLTFAnimation::Track track;
					if (node_track_i) {
						track = node_track_i->value;
					}
					track = _convert_animation_track(p_state, track, animation, track_i, node_i);
					gltf_animation->get_tracks()[node_i] = track;
					break;
				}
			}
		}
	}
	if (gltf_animation->get_tracks().size()) {
		p_state->animations.push_back(gltf_animation);
	}
}

Error GLTFDocument::_parse(Ref<GLTFState> p_state, String p_path, Ref<FileAccess> p_file) {
	Error err;
	if (p_file.is_null()) {
		return FAILED;
	}
	p_file->seek(0);
	uint32_t magic = p_file->get_32();
	if (magic == 0x46546C67) {
		//binary file
		//text file
		p_file->seek(0);
		err = _parse_glb(p_file, p_state);
		if (err != OK) {
			return err;
		}
	} else {
		p_file->seek(0);
		String text = p_file->get_as_utf8_string();
		JSON json;
		err = json.parse(text);
		if (err != OK) {
			_err_print_error("", "", json.get_error_line(), json.get_error_message().utf8().get_data(), false, ERR_HANDLER_SCRIPT);
		}
		ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);
		p_state->json = json.get_data();
	}

	err = _parse_asset_header(p_state);
	ERR_FAIL_COND_V(err != OK, err);

	document_extensions.clear();
	for (Ref<GLTFDocumentExtension> ext : all_document_extensions) {
		ERR_CONTINUE(ext.is_null());
		err = ext->import_preflight(p_state, p_state->json["extensionsUsed"]);
		if (err == OK) {
			document_extensions.push_back(ext);
		}
	}

	err = _parse_gltf_state(p_state, p_path);
	ERR_FAIL_COND_V(err != OK, err);

	return OK;
}

Dictionary _serialize_texture_transform_uv(Vector2 p_offset, Vector2 p_scale) {
	Dictionary texture_transform;
	bool is_offset = p_offset != Vector2(0.0, 0.0);
	if (is_offset) {
		Array offset;
		offset.resize(2);
		offset[0] = p_offset.x;
		offset[1] = p_offset.y;
		texture_transform["offset"] = offset;
	}
	bool is_scaled = p_scale != Vector2(1.0, 1.0);
	if (is_scaled) {
		Array scale;
		scale.resize(2);
		scale[0] = p_scale.x;
		scale[1] = p_scale.y;
		texture_transform["scale"] = scale;
	}
	Dictionary extension;
	// Note: Godot doesn't support texture rotation.
	if (is_offset || is_scaled) {
		extension["KHR_texture_transform"] = texture_transform;
	}
	return extension;
}

Dictionary GLTFDocument::_serialize_texture_transform_uv1(Ref<BaseMaterial3D> p_material) {
	ERR_FAIL_COND_V(p_material.is_null(), Dictionary());
	Vector3 offset = p_material->get_uv1_offset();
	Vector3 scale = p_material->get_uv1_scale();
	return _serialize_texture_transform_uv(Vector2(offset.x, offset.y), Vector2(scale.x, scale.y));
}

Dictionary GLTFDocument::_serialize_texture_transform_uv2(Ref<BaseMaterial3D> p_material) {
	ERR_FAIL_COND_V(p_material.is_null(), Dictionary());
	Vector3 offset = p_material->get_uv2_offset();
	Vector3 scale = p_material->get_uv2_scale();
	return _serialize_texture_transform_uv(Vector2(offset.x, offset.y), Vector2(scale.x, scale.y));
}

Error GLTFDocument::_serialize_asset_header(Ref<GLTFState> p_state) {
	const String version = "2.0";
	p_state->major_version = version.get_slice(".", 0).to_int();
	p_state->minor_version = version.get_slice(".", 1).to_int();
	Dictionary asset;
	asset["version"] = version;
	if (!p_state->copyright.is_empty()) {
		asset["copyright"] = p_state->copyright;
	}
	String hash = String(VERSION_HASH);
	asset["generator"] = String(VERSION_FULL_NAME) + String("@") + (hash.is_empty() ? String("unknown") : hash);
	p_state->json["asset"] = asset;
	ERR_FAIL_COND_V(!asset.has("version"), Error::FAILED);
	ERR_FAIL_COND_V(!p_state->json.has("asset"), Error::FAILED);
	return OK;
}

Error GLTFDocument::_serialize_file(Ref<GLTFState> p_state, const String p_path) {
	Error err = FAILED;
	if (p_path.to_lower().ends_with("glb")) {
		err = _encode_buffer_glb(p_state, p_path);
		ERR_FAIL_COND_V(err != OK, err);
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
		ERR_FAIL_COND_V(file.is_null(), FAILED);

		String json = Variant(p_state->json).to_json_string();

		const uint32_t magic = 0x46546C67; // GLTF
		const int32_t header_size = 12;
		const int32_t chunk_header_size = 8;
		CharString cs = json.utf8();
		const uint32_t text_data_length = cs.length();
		const uint32_t text_chunk_length = ((text_data_length + 3) & (~3));
		const uint32_t text_chunk_type = 0x4E4F534A; //JSON

		uint32_t binary_data_length = 0;
		if (p_state->buffers.size() > 0) {
			binary_data_length = p_state->buffers[0].size();
		}
		const uint32_t binary_chunk_length = ((binary_data_length + 3) & (~3));
		const uint32_t binary_chunk_type = 0x004E4942; //BIN

		file->create(FileAccess::ACCESS_RESOURCES);
		file->store_32(magic);
		file->store_32(p_state->major_version); // version
		uint32_t total_length = header_size + chunk_header_size + text_chunk_length;
		if (binary_chunk_length) {
			total_length += chunk_header_size + binary_chunk_length;
		}
		file->store_32(total_length);

		// Write the JSON text chunk.
		file->store_32(text_chunk_length);
		file->store_32(text_chunk_type);
		file->store_buffer((uint8_t *)&cs[0], cs.length());
		for (uint32_t pad_i = text_data_length; pad_i < text_chunk_length; pad_i++) {
			file->store_8(' ');
		}

		// Write a single binary chunk.
		if (binary_chunk_length) {
			file->store_32(binary_chunk_length);
			file->store_32(binary_chunk_type);
			file->store_buffer(p_state->buffers[0].ptr(), binary_data_length);
			for (uint32_t pad_i = binary_data_length; pad_i < binary_chunk_length; pad_i++) {
				file->store_8(0);
			}
		}
	} else {
		err = _encode_buffer_bins(p_state, p_path);
		ERR_FAIL_COND_V(err != OK, err);
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
		ERR_FAIL_COND_V(file.is_null(), FAILED);

		file->create(FileAccess::ACCESS_RESOURCES);
		String json = Variant(p_state->json).to_json_string();
		file->store_string(json);
	}
	return err;
}

void GLTFDocument::_bind_methods() {
	BIND_ENUM_CONSTANT(ROOT_NODE_MODE_SINGLE_ROOT);
	BIND_ENUM_CONSTANT(ROOT_NODE_MODE_KEEP_ROOT);
	BIND_ENUM_CONSTANT(ROOT_NODE_MODE_MULTI_ROOT);

	ClassDB::bind_method(D_METHOD("set_image_format", "image_format"), &GLTFDocument::set_image_format);
	ClassDB::bind_method(D_METHOD("get_image_format"), &GLTFDocument::get_image_format);
	ClassDB::bind_method(D_METHOD("set_lossy_quality", "lossy_quality"), &GLTFDocument::set_lossy_quality);
	ClassDB::bind_method(D_METHOD("get_lossy_quality"), &GLTFDocument::get_lossy_quality);
	ClassDB::bind_method(D_METHOD("set_root_node_mode", "root_node_mode"), &GLTFDocument::set_root_node_mode);
	ClassDB::bind_method(D_METHOD("get_root_node_mode"), &GLTFDocument::get_root_node_mode);
	ClassDB::bind_method(D_METHOD("append_from_file", "path", "state", "flags", "base_path"),
			&GLTFDocument::append_from_file, DEFVAL(0), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("append_from_buffer", "bytes", "base_path", "state", "flags"),
			&GLTFDocument::append_from_buffer, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("append_from_scene", "node", "state", "flags"),
			&GLTFDocument::append_from_scene, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("generate_scene", "state", "bake_fps", "trimming", "remove_immutable_tracks"),
			&GLTFDocument::generate_scene, DEFVAL(30), DEFVAL(false), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("generate_buffer", "state"),
			&GLTFDocument::generate_buffer);
	ClassDB::bind_method(D_METHOD("write_to_filesystem", "state", "path"),
			&GLTFDocument::write_to_filesystem);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "image_format"), "set_image_format", "get_image_format");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lossy_quality"), "set_lossy_quality", "get_lossy_quality");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "root_node_mode"), "set_root_node_mode", "get_root_node_mode");

	ClassDB::bind_static_method("GLTFDocument", D_METHOD("register_gltf_document_extension", "extension", "first_priority"),
			&GLTFDocument::register_gltf_document_extension, DEFVAL(false));
	ClassDB::bind_static_method("GLTFDocument", D_METHOD("unregister_gltf_document_extension", "extension"),
			&GLTFDocument::unregister_gltf_document_extension);
	ClassDB::bind_static_method("GLTFDocument", D_METHOD("get_supported_gltf_extensions"),
			&GLTFDocument::get_supported_gltf_extensions);
}

void GLTFDocument::_build_parent_hierachy(Ref<GLTFState> p_state) {
	// build the hierarchy
	for (GLTFNodeIndex node_i = 0; node_i < p_state->nodes.size(); node_i++) {
		for (int j = 0; j < p_state->nodes[node_i]->children.size(); j++) {
			GLTFNodeIndex child_i = p_state->nodes[node_i]->children[j];
			ERR_FAIL_INDEX(child_i, p_state->nodes.size());
			if (p_state->nodes.write[child_i]->parent != -1) {
				continue;
			}
			p_state->nodes.write[child_i]->parent = node_i;
		}
	}
}

Vector<Ref<GLTFDocumentExtension>> GLTFDocument::all_document_extensions;

void GLTFDocument::register_gltf_document_extension(Ref<GLTFDocumentExtension> p_extension, bool p_first_priority) {
	if (!all_document_extensions.has(p_extension)) {
		if (p_first_priority) {
			all_document_extensions.insert(0, p_extension);
		} else {
			all_document_extensions.push_back(p_extension);
		}
	}
}

void GLTFDocument::unregister_gltf_document_extension(Ref<GLTFDocumentExtension> p_extension) {
	all_document_extensions.erase(p_extension);
}

void GLTFDocument::unregister_all_gltf_document_extensions() {
	all_document_extensions.clear();
}

Vector<Ref<GLTFDocumentExtension>> GLTFDocument::get_all_gltf_document_extensions() {
	return all_document_extensions;
}

Vector<String> GLTFDocument::get_supported_gltf_extensions() {
	HashSet<String> set = get_supported_gltf_extensions_hashset();
	Vector<String> vec;
	for (const String &s : set) {
		vec.append(s);
	}
	vec.sort();
	return vec;
}

HashSet<String> GLTFDocument::get_supported_gltf_extensions_hashset() {
	HashSet<String> supported_extensions;
	// If the extension is supported directly in GLTFDocument, list it here.
	// Other built-in extensions are supported by GLTFDocumentExtension classes.
	supported_extensions.insert("GODOT_single_root");
	supported_extensions.insert("KHR_lights_punctual");
	supported_extensions.insert("KHR_materials_emissive_strength");
	supported_extensions.insert("KHR_materials_pbrSpecularGlossiness");
	supported_extensions.insert("KHR_materials_unlit");
	supported_extensions.insert("KHR_texture_transform");
	for (Ref<GLTFDocumentExtension> ext : all_document_extensions) {
		ERR_CONTINUE(ext.is_null());
		Vector<String> ext_supported_extensions = ext->get_supported_extensions();
		for (int i = 0; i < ext_supported_extensions.size(); ++i) {
			supported_extensions.insert(ext_supported_extensions[i]);
		}
	}
	return supported_extensions;
}

PackedByteArray GLTFDocument::_serialize_glb_buffer(Ref<GLTFState> p_state, Error *r_err) {
	Error err = _encode_buffer_glb(p_state, "");
	if (r_err) {
		*r_err = err;
	}
	ERR_FAIL_COND_V(err != OK, PackedByteArray());
	String json = Variant(p_state->json).to_json_string();

	const uint32_t magic = 0x46546C67; // GLTF
	const int32_t header_size = 12;
	const int32_t chunk_header_size = 8;

	int32_t padding = (chunk_header_size + json.utf8().length()) % 4;
	json += String(" ").repeat(padding);

	CharString cs = json.utf8();
	const uint32_t text_chunk_length = cs.length();

	const uint32_t text_chunk_type = 0x4E4F534A; //JSON
	int32_t binary_data_length = 0;
	if (p_state->buffers.size() > 0) {
		binary_data_length = p_state->buffers[0].size();
	}
	const int32_t binary_chunk_length = binary_data_length;
	const int32_t binary_chunk_type = 0x004E4942; //BIN

	Ref<StreamPeerBuffer> buffer;
	buffer.instantiate();
	buffer->put_32(magic);
	buffer->put_32(p_state->major_version); // version
	buffer->put_32(header_size + chunk_header_size + text_chunk_length + chunk_header_size + binary_data_length); // length
	buffer->put_32(text_chunk_length);
	buffer->put_32(text_chunk_type);
	buffer->put_data((uint8_t *)&cs[0], cs.length());
	if (binary_chunk_length) {
		buffer->put_32(binary_chunk_length);
		buffer->put_32(binary_chunk_type);
		buffer->put_data(p_state->buffers[0].ptr(), binary_data_length);
	}
	return buffer->get_data_array();
}

Node *GLTFDocument::_generate_scene_node_tree(Ref<GLTFState> p_state) {
	// Generate the skeletons and skins (if any).
	HashMap<ObjectID, SkinSkeletonIndex> skeleton_map;
	Error err = SkinTool::_create_skeletons(p_state->unique_names, p_state->skins, p_state->nodes,
			skeleton_map, p_state->skeletons, p_state->scene_nodes);
	ERR_FAIL_COND_V_MSG(err != OK, nullptr, "glTF: Failed to create skeletons.");
	err = _create_skins(p_state);
	ERR_FAIL_COND_V_MSG(err != OK, nullptr, "glTF: Failed to create skins.");
	// Run pre-generate for each extension, in case an extension needs to do something before generating the scene.
	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		err = ext->import_pre_generate(p_state);
		ERR_CONTINUE(err != OK);
	}
	// Generate the node tree.
	Node *single_root;
	if (p_state->extensions_used.has("GODOT_single_root")) {
		_generate_scene_node(p_state, 0, nullptr, nullptr);
		single_root = p_state->scene_nodes[0];
		if (single_root && single_root->get_owner() && single_root->get_owner() != single_root) {
			single_root = single_root->get_owner();
		}
	} else {
		single_root = memnew(Node3D);
		for (int32_t root_i = 0; root_i < p_state->root_nodes.size(); root_i++) {
			_generate_scene_node(p_state, p_state->root_nodes[root_i], single_root, single_root);
		}
	}
	// Assign the scene name and single root name to each other
	// if one is missing, or do nothing if both are already set.
	if (unlikely(p_state->scene_name.is_empty())) {
		p_state->scene_name = single_root->get_name();
	} else if (single_root->get_name() == StringName()) {
		if (_naming_version == 0) {
			single_root->set_name(p_state->scene_name);
		} else {
			single_root->set_name(_gen_unique_name(p_state, p_state->scene_name));
		}
	}
	return single_root;
}

Error GLTFDocument::_parse_asset_header(Ref<GLTFState> p_state) {
	if (!p_state->json.has("asset")) {
		return ERR_PARSE_ERROR;
	}
	Dictionary asset = p_state->json["asset"];
	if (!asset.has("version")) {
		return ERR_PARSE_ERROR;
	}
	String version = asset["version"];
	p_state->major_version = version.get_slice(".", 0).to_int();
	p_state->minor_version = version.get_slice(".", 1).to_int();
	if (asset.has("copyright")) {
		p_state->copyright = asset["copyright"];
	}
	return OK;
}

Error GLTFDocument::_parse_gltf_state(Ref<GLTFState> p_state, const String &p_search_path) {
	Error err;

	/* PARSE EXTENSIONS */
	err = _parse_gltf_extensions(p_state);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* PARSE SCENE */
	err = _parse_scenes(p_state);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* PARSE NODES */
	err = _parse_nodes(p_state);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* PARSE BUFFERS */
	err = _parse_buffers(p_state, p_search_path);

	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* PARSE BUFFER VIEWS */
	err = _parse_buffer_views(p_state);

	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* PARSE ACCESSORS */
	err = _parse_accessors(p_state);

	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	if (!p_state->discard_meshes_and_materials) {
		/* PARSE IMAGES */
		err = _parse_images(p_state, p_search_path);

		ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

		/* PARSE TEXTURE SAMPLERS */
		err = _parse_texture_samplers(p_state);

		ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

		/* PARSE TEXTURES */
		err = _parse_textures(p_state);

		ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

		/* PARSE TEXTURES */
		err = _parse_materials(p_state);

		ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);
	}

	/* PARSE SKINS */
	err = _parse_skins(p_state);

	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* DETERMINE SKELETONS */
	err = SkinTool::_determine_skeletons(p_state->skins, p_state->nodes, p_state->skeletons, p_state->get_import_as_skeleton_bones() ? p_state->root_nodes : Vector<GLTFNodeIndex>());
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* PARSE MESHES (we have enough info now) */
	err = _parse_meshes(p_state);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* PARSE LIGHTS */
	err = _parse_lights(p_state);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* PARSE CAMERAS */
	err = _parse_cameras(p_state);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* PARSE ANIMATIONS */
	err = _parse_animations(p_state);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* ASSIGN SCENE NAMES */
	_assign_node_names(p_state);

	return OK;
}

PackedByteArray GLTFDocument::generate_buffer(Ref<GLTFState> p_state) {
	Ref<GLTFState> state = p_state;
	ERR_FAIL_COND_V(state.is_null(), PackedByteArray());
	// For buffers, set the state filename to an empty string, but
	// don't touch the base path, in case the user set it manually.
	state->filename = "";
	Error err = _serialize(state);
	ERR_FAIL_COND_V(err != OK, PackedByteArray());
	PackedByteArray bytes = _serialize_glb_buffer(state, &err);
	return bytes;
}

Error GLTFDocument::write_to_filesystem(Ref<GLTFState> p_state, const String &p_path) {
	Ref<GLTFState> state = p_state;
	ERR_FAIL_COND_V(state.is_null(), ERR_INVALID_PARAMETER);
	state->base_path = p_path.get_base_dir();
	state->filename = p_path.get_file();
	Error err = _serialize(state);
	if (err != OK) {
		return err;
	}
	err = _serialize_file(state, p_path);
	if (err != OK) {
		return Error::FAILED;
	}
	return OK;
}

Node *GLTFDocument::generate_scene(Ref<GLTFState> p_state, float p_bake_fps, bool p_trimming, bool p_remove_immutable_tracks) {
	Ref<GLTFState> state = p_state;
	ERR_FAIL_COND_V(state.is_null(), nullptr);
	ERR_FAIL_INDEX_V(0, state->root_nodes.size(), nullptr);
	Error err = OK;
	p_state->set_bake_fps(p_bake_fps);
	Node *root = _generate_scene_node_tree(state);
	ERR_FAIL_NULL_V(root, nullptr);
	_process_mesh_instances(state, root);
	if (state->get_create_animations() && state->animations.size()) {
		AnimationPlayer *ap = memnew(AnimationPlayer);
		root->add_child(ap, true);
		ap->set_owner(root);
		for (int i = 0; i < state->animations.size(); i++) {
			_import_animation(state, ap, i, p_trimming, p_remove_immutable_tracks);
		}
	}
	for (KeyValue<GLTFNodeIndex, Node *> E : state->scene_nodes) {
		ERR_CONTINUE(!E.value);
		for (Ref<GLTFDocumentExtension> ext : document_extensions) {
			ERR_CONTINUE(ext.is_null());
			Dictionary node_json;
			if (state->json.has("nodes")) {
				Array nodes = state->json["nodes"];
				if (0 <= E.key && E.key < nodes.size()) {
					node_json = nodes[E.key];
				}
			}
			Ref<GLTFNode> gltf_node = state->nodes[E.key];
			err = ext->import_node(p_state, gltf_node, node_json, E.value);
			ERR_CONTINUE(err != OK);
		}
	}
	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		err = ext->import_post(p_state, root);
		ERR_CONTINUE(err != OK);
	}
	ERR_FAIL_NULL_V(root, nullptr);
	return root;
}

Error GLTFDocument::append_from_scene(Node *p_node, Ref<GLTFState> p_state, uint32_t p_flags) {
	ERR_FAIL_NULL_V(p_node, FAILED);
	Ref<GLTFState> state = p_state;
	ERR_FAIL_COND_V(state.is_null(), FAILED);
	state->use_named_skin_binds = p_flags & GLTF_IMPORT_USE_NAMED_SKIN_BINDS;
	state->discard_meshes_and_materials = p_flags & GLTF_IMPORT_DISCARD_MESHES_AND_MATERIALS;
	state->force_generate_tangents = p_flags & GLTF_IMPORT_GENERATE_TANGENT_ARRAYS;
	state->force_disable_compression = p_flags & GLTF_IMPORT_FORCE_DISABLE_MESH_COMPRESSION;
	if (!state->buffers.size()) {
		state->buffers.push_back(Vector<uint8_t>());
	}
	// Perform export preflight for document extensions. Only extensions that
	// return OK will be used for the rest of the export steps.
	document_extensions.clear();
	for (Ref<GLTFDocumentExtension> ext : all_document_extensions) {
		ERR_CONTINUE(ext.is_null());
		Error err = ext->export_preflight(state, p_node);
		if (err == OK) {
			document_extensions.push_back(ext);
		}
	}
	// Add the root node(s) and their descendants to the state.
	if (_root_node_mode == RootNodeMode::ROOT_NODE_MODE_MULTI_ROOT) {
		const int child_count = p_node->get_child_count();
		if (child_count > 0) {
			for (int i = 0; i < child_count; i++) {
				_convert_scene_node(state, p_node->get_child(i), -1, -1);
			}
			state->scene_name = p_node->get_name();
			return OK;
		}
	}
	if (_root_node_mode == RootNodeMode::ROOT_NODE_MODE_SINGLE_ROOT) {
		state->extensions_used.append("GODOT_single_root");
	}
	_convert_scene_node(state, p_node, -1, -1);
	// Run post-convert for each extension, in case an extension needs to do something after converting the scene.
	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		Error err = ext->export_post_convert(p_state, p_node);
		ERR_CONTINUE(err != OK);
	}
	return OK;
}

Error GLTFDocument::append_from_buffer(PackedByteArray p_bytes, String p_base_path, Ref<GLTFState> p_state, uint32_t p_flags) {
	Ref<GLTFState> state = p_state;
	ERR_FAIL_COND_V(state.is_null(), FAILED);
	// TODO Add missing texture and missing .bin file paths to r_missing_deps 2021-09-10 fire
	Error err = FAILED;
	state->use_named_skin_binds = p_flags & GLTF_IMPORT_USE_NAMED_SKIN_BINDS;
	state->discard_meshes_and_materials = p_flags & GLTF_IMPORT_DISCARD_MESHES_AND_MATERIALS;
	state->force_generate_tangents = p_flags & GLTF_IMPORT_GENERATE_TANGENT_ARRAYS;
	state->force_disable_compression = p_flags & GLTF_IMPORT_FORCE_DISABLE_MESH_COMPRESSION;

	Ref<FileAccessMemory> file_access;
	file_access.instantiate();
	file_access->open_custom(p_bytes.ptr(), p_bytes.size());
	state->base_path = p_base_path.get_base_dir();
	err = _parse(p_state, state->base_path, file_access);
	ERR_FAIL_COND_V(err != OK, err);
	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		err = ext->import_post_parse(state);
		ERR_FAIL_COND_V(err != OK, err);
	}
	return OK;
}

Error GLTFDocument::append_from_file(String p_path, Ref<GLTFState> p_state, uint32_t p_flags, String p_base_path) {
	Ref<GLTFState> state = p_state;
	// TODO Add missing texture and missing .bin file paths to r_missing_deps 2021-09-10 fire
	if (state == Ref<GLTFState>()) {
		state.instantiate();
	}
	state->filename = p_path.get_file().get_basename();
	state->use_named_skin_binds = p_flags & GLTF_IMPORT_USE_NAMED_SKIN_BINDS;
	state->discard_meshes_and_materials = p_flags & GLTF_IMPORT_DISCARD_MESHES_AND_MATERIALS;
	state->force_generate_tangents = p_flags & GLTF_IMPORT_GENERATE_TANGENT_ARRAYS;
	state->force_disable_compression = p_flags & GLTF_IMPORT_FORCE_DISABLE_MESH_COMPRESSION;

	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V(err != OK, ERR_FILE_CANT_OPEN);
	ERR_FAIL_COND_V(file.is_null(), ERR_FILE_CANT_OPEN);
	String base_path = p_base_path;
	if (base_path.is_empty()) {
		base_path = p_path.get_base_dir();
	}
	state->base_path = base_path;
	err = _parse(p_state, base_path, file);
	ERR_FAIL_COND_V(err != OK, err);
	for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		ERR_CONTINUE(ext.is_null());
		err = ext->import_post_parse(p_state);
		ERR_FAIL_COND_V(err != OK, err);
	}
	return OK;
}

Error GLTFDocument::_parse_gltf_extensions(Ref<GLTFState> p_state) {
	ERR_FAIL_COND_V(p_state.is_null(), ERR_PARSE_ERROR);
	if (p_state->json.has("extensionsUsed")) {
		Vector<String> ext_array = p_state->json["extensionsUsed"];
		p_state->extensions_used = ext_array;
	}
	if (p_state->json.has("extensionsRequired")) {
		Vector<String> ext_array = p_state->json["extensionsRequired"];
		p_state->extensions_required = ext_array;
	}
	HashSet<String> supported_extensions = get_supported_gltf_extensions_hashset();
	Error ret = OK;
	for (int i = 0; i < p_state->extensions_required.size(); i++) {
		if (!supported_extensions.has(p_state->extensions_required[i])) {
			ERR_PRINT("glTF: Can't import file '" + p_state->filename + "', required extension '" + String(p_state->extensions_required[i]) + "' is not supported. Are you missing a GLTFDocumentExtension plugin?");
			ret = ERR_UNAVAILABLE;
		}
	}
	return ret;
}

void GLTFDocument::set_root_node_mode(GLTFDocument::RootNodeMode p_root_node_mode) {
	_root_node_mode = p_root_node_mode;
}

GLTFDocument::RootNodeMode GLTFDocument::get_root_node_mode() const {
	return _root_node_mode;
}

String GLTFDocument::_gen_unique_name_static(HashSet<String> &r_unique_names, const String &p_name) {
	const String s_name = p_name.validate_node_name();

	String u_name;
	int index = 1;
	while (true) {
		u_name = s_name;

		if (index > 1) {
			u_name += itos(index);
		}
		if (!r_unique_names.has(u_name)) {
			break;
		}
		index++;
	}

	r_unique_names.insert(u_name);

	return u_name;
}
