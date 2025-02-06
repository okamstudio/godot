/**************************************************************************/
/*  fbx_document.cpp                                                      */
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

#include "fbx_document.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/io/file_access_memory.h"
#include "core/io/image.h"
#include "core/math/color.h"
#include "scene/3d/bone_attachment_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/material.h"
#include "scene/resources/portable_compressed_texture.h"
#include "scene/resources/surface_tool.h"

#include "modules/gltf/extensions/gltf_light.h"
#include "modules/gltf/gltf_defines.h"
#include "modules/gltf/skin_tool.h"
#include "modules/gltf/structures/gltf_animation.h"
#include "modules/gltf/structures/gltf_camera.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#endif

// FIXME: Hardcoded to avoid editor dependency.
#define FBX_IMPORT_USE_NAMED_SKIN_BINDS 16
#define FBX_IMPORT_DISCARD_MESHES_AND_MATERIALS 32
#define FBX_IMPORT_FORCE_DISABLE_MESH_COMPRESSION 64

#include <ufbx.h>

static size_t _file_access_read_fn(void *user, void *data, size_t size) {
	FileAccess *file = static_cast<FileAccess *>(user);
	return (size_t)file->get_buffer((uint8_t *)data, (uint64_t)size);
}

static bool _file_access_skip_fn(void *user, size_t size) {
	FileAccess *file = static_cast<FileAccess *>(user);
	file->seek(file->get_position() + size);
	return true;
}

static Vector2 _as_vec2(const ufbx_vec2 &p_vector) {
	return Vector2(real_t(p_vector.x), real_t(p_vector.y));
}

static Color _as_color(const ufbx_vec4 &p_vector) {
	return Color(real_t(p_vector.x), real_t(p_vector.y), real_t(p_vector.z), real_t(p_vector.w));
}

static Quaternion _as_quaternion(const ufbx_quat &p_quat) {
	return Quaternion(real_t(p_quat.x), real_t(p_quat.y), real_t(p_quat.z), real_t(p_quat.w));
}

static Transform3D _as_transform(const ufbx_transform &p_xform) {
	Transform3D result;
	result.origin = FBXDocument::_as_vec3(p_xform.translation);
	result.basis.set_quaternion_scale(_as_quaternion(p_xform.rotation), FBXDocument::_as_vec3(p_xform.scale));
	return result;
}

static real_t _relative_error(const Vector3 &p_a, const Vector3 &p_b) {
	return p_a.distance_to(p_b) / MAX(p_a.length(), p_b.length());
}

static Color _material_color(const ufbx_material_map &p_map) {
	if (p_map.value_components == 1) {
		float r = float(p_map.value_real);
		return Color(r, r, r);
	} else if (p_map.value_components == 3) {
		float r = float(p_map.value_vec3.x);
		float g = float(p_map.value_vec3.y);
		float b = float(p_map.value_vec3.z);
		return Color(r, g, b);
	} else {
		float r = float(p_map.value_vec4.x);
		float g = float(p_map.value_vec4.y);
		float b = float(p_map.value_vec4.z);
		float a = float(p_map.value_vec4.z);
		return Color(r, g, b, a);
	}
}

static Color _material_color(const ufbx_material_map &p_map, const ufbx_material_map &p_factor) {
	Color color = _material_color(p_map);
	if (p_factor.has_value) {
		float factor = float(p_factor.value_real);
		color.r *= factor;
		color.g *= factor;
		color.b *= factor;
	}
	return color;
}

static const ufbx_texture *_get_file_texture(const ufbx_texture *p_texture) {
	if (!p_texture) {
		return nullptr;
	}
	for (const ufbx_texture *texture : p_texture->file_textures) {
		if (texture->file_index != UFBX_NO_INDEX) {
			return texture;
		}
	}
	return nullptr;
}

static Ref<Image> _get_decompressed_image(Ref<Texture2D> texture) {
	if (texture.is_null()) {
		return Ref<Image>();
	}
	Ref<Image> image = texture->get_image();
	if (image.is_null()) {
		return Ref<Image>();
	}
	image = image->duplicate();
	image->decompress();
	return image;
}

static Vector<Vector2> _decode_vertex_attrib_vec2(const ufbx_vertex_vec2 &p_attrib, const Vector<uint32_t> &p_indices) {
	Vector<Vector2> ret;

	int num_indices = p_indices.size();
	ret.resize(num_indices);
	for (int i = 0; i < num_indices; i++) {
		ret.write[i] = _as_vec2(p_attrib[p_indices[i]]);
	}
	return ret;
}

static Vector<Vector3> _decode_vertex_attrib_vec3(const ufbx_vertex_vec3 &p_attrib, const Vector<uint32_t> &p_indices) {
	Vector<Vector3> ret;

	int num_indices = p_indices.size();
	ret.resize(num_indices);
	for (int i = 0; i < num_indices; i++) {
		ret.write[i] = FBXDocument::_as_vec3(p_attrib[p_indices[i]]);
	}
	return ret;
}

static Vector<float> _decode_vertex_attrib_vec3_as_tangent(const ufbx_vertex_vec3 &p_attrib, const Vector<uint32_t> &p_indices) {
	Vector<float> ret;

	int num_indices = p_indices.size();
	ret.resize(num_indices * 4);
	for (int i = 0; i < num_indices; i++) {
		Vector3 v = FBXDocument::_as_vec3(p_attrib[p_indices[i]]);
		ret.write[i * 4 + 0] = v.x;
		ret.write[i * 4 + 1] = v.y;
		ret.write[i * 4 + 2] = v.z;
		ret.write[i * 4 + 3] = 1.0f;
	}
	return ret;
}

static Vector<Color> _decode_vertex_attrib_color(const ufbx_vertex_vec4 &p_attrib, const Vector<uint32_t> &p_indices) {
	Vector<Color> ret;

	int num_indices = p_indices.size();
	ret.resize(num_indices);
	for (int i = 0; i < num_indices; i++) {
		ret.write[i] = _as_color(p_attrib[p_indices[i]]);
	}
	return ret;
}

static Vector3 _encode_vertex_index(uint32_t p_index) {
	return Vector3(real_t(p_index & 0xffff), real_t(p_index >> 16), 0.0f);
}

static uint32_t _decode_vertex_index(const Vector3 &p_vertex) {
	return uint32_t(p_vertex.x) | uint32_t(p_vertex.y) << 16;
}

static ufbx_skin_deformer *_find_skin_deformer(ufbx_skin_cluster *p_cluster) {
	for (const ufbx_connection &conn : p_cluster->element.connections_src) {
		ufbx_skin_deformer *deformer = ufbx_as_skin_deformer(conn.dst);
		if (deformer) {
			return deformer;
		}
	}
	return nullptr;
}

static String _find_element_name(ufbx_element *p_element) {
	if (p_element->name.length > 0) {
		return FBXDocument::_as_string(p_element->name);
	} else if (p_element->instances.count > 0) {
		return _find_element_name(&p_element->instances[0]->element);
	} else {
		return "";
	}
}

struct ThreadPoolFBX {
	struct Group {
		ufbx_thread_pool_context ctx = {};
		WorkerThreadPool::GroupID task_id = {};
		uint32_t start_index = 0;
	};

	WorkerThreadPool *pool = nullptr;
	Group groups[UFBX_THREAD_GROUP_COUNT] = {};
};

static void _thread_pool_task(void *user, uint32_t index) {
	ThreadPoolFBX::Group *group = (ThreadPoolFBX::Group *)user;
	ufbx_thread_pool_run_task(group->ctx, group->start_index + index);
}

static bool _thread_pool_init_fn(void *user, ufbx_thread_pool_context ctx, const ufbx_thread_pool_info *info) {
	ThreadPoolFBX *pool = (ThreadPoolFBX *)user;
	for (ThreadPoolFBX::Group &group : pool->groups) {
		group.ctx = ctx;
	}
	return true;
}

static bool _thread_pool_run_fn(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t start_index, uint32_t count) {
	ThreadPoolFBX *pool = (ThreadPoolFBX *)user;
	ThreadPoolFBX::Group &pool_group = pool->groups[group];
	pool_group.start_index = start_index;
	pool_group.task_id = pool->pool->add_native_group_task(_thread_pool_task, &pool_group, (int)count, -1, true, "ufbx");
	return true;
}

static bool _thread_pool_wait_fn(void *user, ufbx_thread_pool_context ctx, uint32_t group, uint32_t max_index) {
	ThreadPoolFBX *pool = (ThreadPoolFBX *)user;
	pool->pool->wait_for_group_task_completion(pool->groups[group].task_id);
	return true;
}

String FBXDocument::_gen_unique_name(HashSet<String> &unique_names, const String &p_name) {
	const String s_name = p_name.validate_node_name();

	String u_name;
	int index = 1;
	while (true) {
		u_name = s_name;

		if (index > 1) {
			u_name += itos(index);
		}
		if (!unique_names.has(u_name)) {
			break;
		}
		index++;
	}

	unique_names.insert(u_name);

	return u_name;
}

String FBXDocument::_sanitize_animation_name(const String &p_name) {
	String anim_name = p_name.validate_node_name();
	return AnimationLibrary::validate_library_name(anim_name);
}

String FBXDocument::_gen_unique_animation_name(Ref<FBXState> p_state, const String &p_name) {
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

Error FBXDocument::_parse_scenes(Ref<FBXState> p_state) {
	p_state->unique_names.insert("Skeleton3D"); // Reserve skeleton name.

	const ufbx_scene *fbx_scene = p_state->scene.get();

	// TODO: Multi-document support, would need test files for structure
	p_state->scene_name = "";

	// TODO: Append the root node directly if we use root-based space conversion
	for (const ufbx_node *root_node : fbx_scene->root_node->children) {
		p_state->root_nodes.push_back(int(root_node->typed_id));
	}

	return OK;
}

Error FBXDocument::_parse_nodes(Ref<FBXState> p_state) {
	const ufbx_scene *fbx_scene = p_state->scene.get();

	for (int node_i = 0; node_i < static_cast<int>(fbx_scene->nodes.count); node_i++) {
		const ufbx_node *fbx_node = fbx_scene->nodes[node_i];

		Ref<GLTFNode> node;
		node.instantiate();

		node->height = int(fbx_node->node_depth);

		if (fbx_node->name.length > 0) {
			node->set_name(_as_string(fbx_node->name));
			node->set_original_name(node->get_name());
		} else if (fbx_node->is_root) {
			node->set_name("RootNode");
		}
		if (fbx_node->camera) {
			node->camera = fbx_node->camera->typed_id;
		}
		if (fbx_node->light) {
			node->light = fbx_node->light->typed_id;
		}
		if (fbx_node->mesh) {
			node->mesh = fbx_node->mesh->typed_id;
		}

		{
			node->transform = _as_transform(fbx_node->local_transform);

			bool found_rest_xform = false;
			bool bad_rest_xform = false;
			Transform3D candidate_rest_xform;

			if (fbx_node->parent) {
				// Attempt to resolve a rest pose for bones: This uses internal FBX connections to find
				// all skin clusters connected to the bone.
				for (const ufbx_connection &child_conn : fbx_node->element.connections_src) {
					ufbx_skin_cluster *child_cluster = ufbx_as_skin_cluster(child_conn.dst);
					if (!child_cluster) {
						continue;
					}
					ufbx_skin_deformer *child_deformer = _find_skin_deformer(child_cluster);
					if (!child_deformer) {
						continue;
					}

					// Found a skin cluster: Now iterate through all the skin clusters of the parent and
					// try to find one that used by the same deformer.
					for (const ufbx_connection &parent_conn : fbx_node->parent->element.connections_src) {
						ufbx_skin_cluster *parent_cluster = ufbx_as_skin_cluster(parent_conn.dst);
						if (!parent_cluster) {
							continue;
						}
						ufbx_skin_deformer *parent_deformer = _find_skin_deformer(parent_cluster);
						if (parent_deformer != child_deformer) {
							continue;
						}

						// Success: Found two skin clusters from the same deformer, now we can resolve the
						// local bind pose from the difference between the two world-space bind poses.
						ufbx_matrix child_to_world = child_cluster->bind_to_world;
						ufbx_matrix world_to_parent = ufbx_matrix_invert(&parent_cluster->bind_to_world);
						ufbx_matrix child_to_parent = ufbx_matrix_mul(&world_to_parent, &child_to_world);
						Transform3D xform = _as_transform(ufbx_matrix_to_transform(&child_to_parent));

						if (!found_rest_xform) {
							// Found the first bind pose for the node, assume that this one is good
							found_rest_xform = true;
							candidate_rest_xform = xform;
						} else if (!bad_rest_xform) {
							// Found another: Let's hope it's similar to the previous one, if not warn and
							// use the initial pose, which is used by default if rest pose is not found.
							real_t error = 0.0f;
							error += _relative_error(candidate_rest_xform.origin, xform.origin);
							for (int i = 0; i < 3; i++) {
								error += _relative_error(candidate_rest_xform.basis.rows[i], xform.basis.rows[i]);
							}
							const real_t max_error = 0.01f;
							if (error >= max_error) {
								WARN_PRINT(vformat("FBX: Node '%s' has multiple bind poses, using initial pose as rest pose.", node->get_name()));
								bad_rest_xform = true;
							}
						}
					}
				}
			}

			Transform3D godot_rest_xform = node->transform;
			if (found_rest_xform && !bad_rest_xform) {
				godot_rest_xform = candidate_rest_xform;
			}
			node->set_additional_data("GODOT_rest_transform", godot_rest_xform);
		}

		for (const ufbx_node *child : fbx_node->children) {
			node->children.push_back(child->typed_id);
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

	return OK;
}

Error FBXDocument::_parse_meshes(Ref<FBXState> p_state) {
	ufbx_scene *fbx_scene = p_state->scene.get();

	LocalVector<int> nodes_by_mesh_id;
	nodes_by_mesh_id.reserve(fbx_scene->meshes.count);
	for (size_t i = 0; i < fbx_scene->meshes.count; i++) {
		nodes_by_mesh_id.push_back(-1);
	}
	for (int i = 0; i < p_state->nodes.size(); i++) {
		const Ref<GLTFNode> &node = p_state->nodes[i];
		if (node->mesh >= 0 && (unsigned)node->mesh < nodes_by_mesh_id.size()) {
			nodes_by_mesh_id[node->mesh] = i;
		}
	}

	for (const ufbx_mesh *fbx_mesh : fbx_scene->meshes) {
		print_verbose("FBX: Parsing mesh: " + itos(int64_t(fbx_mesh->typed_id)));

		static const Mesh::PrimitiveType primitive_types[] = {
			Mesh::PRIMITIVE_TRIANGLES,
			Mesh::PRIMITIVE_POINTS,
			Mesh::PRIMITIVE_LINES,
		};

		Ref<ImporterMesh> import_mesh;
		import_mesh.instantiate();
		String mesh_name = "mesh";
		String original_name;
		if (fbx_mesh->name.length > 0) {
			mesh_name = _as_string(fbx_mesh->name);
			original_name = mesh_name;
		} else if (fbx_mesh->typed_id < (unsigned)p_state->nodes.size() && nodes_by_mesh_id[fbx_mesh->typed_id] != -1) {
			const Ref<GLTFNode> &node = p_state->nodes[nodes_by_mesh_id[fbx_mesh->typed_id]];
			original_name = node->get_original_name();
			mesh_name = node->get_name();
		}
		import_mesh->set_name(_gen_unique_name(p_state->unique_mesh_names, mesh_name));

		bool use_blend_shapes = false;
		if (fbx_mesh->blend_deformers.count > 0) {
			use_blend_shapes = true;
		}

		Vector<float> blend_weights;
		Vector<int> blend_channels;
		if (use_blend_shapes) {
			print_verbose("FBX: Mesh has targets");

			import_mesh->set_blend_shape_mode(Mesh::BLEND_SHAPE_MODE_NORMALIZED);

			for (const ufbx_blend_deformer *fbx_deformer : fbx_mesh->blend_deformers) {
				for (const ufbx_blend_channel *fbx_channel : fbx_deformer->channels) {
					if (fbx_channel->keyframes.count == 0) {
						continue;
					}
					String bs_name;
					if (fbx_channel->name.length > 0) {
						bs_name = _as_string(fbx_channel->name);
					} else {
						bs_name = String("morph_") + itos(blend_channels.size());
					}
					import_mesh->add_blend_shape(bs_name);
					blend_weights.push_back(float(fbx_channel->weight));
					blend_channels.push_back(float(fbx_channel->typed_id));
				}
			}
		}

		for (const ufbx_mesh_part &fbx_mesh_part : fbx_mesh->material_parts) {
			for (Mesh::PrimitiveType primitive : primitive_types) {
				uint32_t num_indices = 0;
				switch (primitive) {
					case Mesh::PRIMITIVE_POINTS:
						num_indices = fbx_mesh_part.num_point_faces * 1;
						break;
					case Mesh::PRIMITIVE_LINES:
						num_indices = fbx_mesh_part.num_line_faces * 2;
						break;
					case Mesh::PRIMITIVE_TRIANGLES:
						num_indices = fbx_mesh_part.num_triangles * 3;
						break;
					case Mesh::PRIMITIVE_TRIANGLE_STRIP:
						// FIXME 2021-09-15 fire
						break;
					case Mesh::PRIMITIVE_LINE_STRIP:
						// FIXME 2021-09-15 fire
						break;
					default:
						// FIXME 2021-09-15 fire
						break;
				}
				if (num_indices == 0) {
					continue;
				}

				Vector<uint32_t> indices;
				indices.resize(num_indices);

				uint32_t offset = 0;
				for (uint32_t face_index : fbx_mesh_part.face_indices) {
					ufbx_face face = fbx_mesh->faces[face_index];
					switch (primitive) {
						case Mesh::PRIMITIVE_POINTS: {
							if (face.num_indices == 1) {
								indices.write[offset] = face.index_begin;
								offset += 1;
							}
						} break;
						case Mesh::PRIMITIVE_LINES:
							if (face.num_indices == 2) {
								indices.write[offset] = face.index_begin;
								indices.write[offset + 1] = face.index_begin + 1;
								offset += 2;
							}
							break;
						case Mesh::PRIMITIVE_TRIANGLES:
							if (face.num_indices >= 3) {
								uint32_t *dst = indices.ptrw() + offset;
								size_t space = indices.size() - offset;
								uint32_t num_triangles = ufbx_triangulate_face(dst, space, fbx_mesh, face);
								offset += num_triangles * 3;

								// Godot uses clockwise winding order!
								for (uint32_t i = 0; i < num_triangles; i++) {
									SWAP(dst[i * 3 + 0], dst[i * 3 + 2]);
								}
							}
							break;
						case Mesh::PRIMITIVE_TRIANGLE_STRIP:
							// FIXME 2021-09-15 fire
							break;
						case Mesh::PRIMITIVE_LINE_STRIP:
							// FIXME 2021-09-15 fire
							break;
						default:
							// FIXME 2021-09-15 fire
							break;
					}
				}
				ERR_CONTINUE((uint64_t)offset != (uint64_t)indices.size());

				int32_t vertex_num = indices.size();
				bool has_vertex_color = false;

				uint32_t flags = 0;

				Array array;
				array.resize(Mesh::ARRAY_MAX);

				// HACK: If we have blend shapes we cannot merge vertices at identical positions
				// if they have different indices in the file. To avoid this encode the vertex index
				// into the vertex position for the time being.
				// Ideally this would be an extra channel in the vertex but as the vertex format is
				// fixed and we already use user data for extra UV channels this'll do.
				if (use_blend_shapes) {
					Vector<Vector3> vertex_indices;
					int num_blend_shape_indices = indices.size();
					vertex_indices.resize(num_blend_shape_indices);
					for (int i = 0; i < num_blend_shape_indices; i++) {
						vertex_indices.write[i] = _encode_vertex_index(fbx_mesh->vertex_indices[indices[i]]);
					}
					array[Mesh::ARRAY_VERTEX] = vertex_indices;
				} else {
					array[Mesh::ARRAY_VERTEX] = _decode_vertex_attrib_vec3(fbx_mesh->vertex_position, indices);
				}

				// Normals always exist as they're generated if missing,
				// see `ufbx_load_opts.generate_missing_normals`.
				Vector<Vector3> normals = _decode_vertex_attrib_vec3(fbx_mesh->vertex_normal, indices);
				array[Mesh::ARRAY_NORMAL] = normals;

				if (fbx_mesh->vertex_tangent.exists) {
					Vector<float> tangents = _decode_vertex_attrib_vec3_as_tangent(fbx_mesh->vertex_tangent, indices);

					// Patch bitangent sign if available
					if (fbx_mesh->vertex_bitangent.exists) {
						for (int i = 0; i < vertex_num; i++) {
							Vector3 tangent = Vector3(tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2]);
							Vector3 bitangent = _as_vec3(fbx_mesh->vertex_bitangent[indices[i]]);
							Vector3 generated_bitangent = normals[i].cross(tangent);
							if (generated_bitangent.dot(bitangent) < 0.0f) {
								tangents.write[i * 4 + 3] = -1.0f;
							}
						}
					}

					array[Mesh::ARRAY_TANGENT] = tangents;
				}

				if (fbx_mesh->vertex_uv.exists) {
					PackedVector2Array uv_array = _decode_vertex_attrib_vec2(fbx_mesh->vertex_uv, indices);
					_process_uv_set(uv_array);
					array[Mesh::ARRAY_TEX_UV] = uv_array;
				}

				if (fbx_mesh->uv_sets.count >= 2 && fbx_mesh->uv_sets[1].vertex_uv.exists) {
					PackedVector2Array uv2_array = _decode_vertex_attrib_vec2(fbx_mesh->uv_sets[1].vertex_uv, indices);
					_process_uv_set(uv2_array);
					array[Mesh::ARRAY_TEX_UV2] = uv2_array;
				}

				for (int uv_i = 2; uv_i < 8; uv_i += 2) {
					Vector<float> cur_custom;
					Vector<Vector2> texcoord_first;
					Vector<Vector2> texcoord_second;

					int texcoord_i = uv_i;
					int texcoord_next = texcoord_i + 1;
					int num_channels = 0;
					if (texcoord_i < static_cast<int>(fbx_mesh->uv_sets.count) && fbx_mesh->uv_sets[texcoord_i].vertex_uv.exists) {
						texcoord_first = _decode_vertex_attrib_vec2(fbx_mesh->uv_sets[texcoord_i].vertex_uv, indices);
						_process_uv_set(texcoord_first);
						num_channels = 2;
					}
					if (texcoord_next < static_cast<int>(fbx_mesh->uv_sets.count) && fbx_mesh->uv_sets[texcoord_next].vertex_uv.exists) {
						texcoord_second = _decode_vertex_attrib_vec2(fbx_mesh->uv_sets[texcoord_next].vertex_uv, indices);
						_process_uv_set(texcoord_second);
						num_channels = 4;
					}
					if (!num_channels) {
						break;
					}
					cur_custom.resize(vertex_num * num_channels);
					for (int32_t uv_first_i = 0; uv_first_i < texcoord_first.size() && uv_first_i < vertex_num; uv_first_i++) {
						int index = uv_first_i * num_channels;
						cur_custom.write[index] = texcoord_first[uv_first_i].x;
						cur_custom.write[index + 1] = texcoord_first[uv_first_i].y;
					}
					if (num_channels == 4) {
						for (int32_t uv_second_i = 0; uv_second_i < texcoord_second.size() && uv_second_i < vertex_num; uv_second_i++) {
							int index = uv_second_i * num_channels;
							cur_custom.write[index + 2] = texcoord_second[uv_second_i].x;
							cur_custom.write[index + 3] = texcoord_second[uv_second_i].y;
						}
						_zero_unused_elements(cur_custom, texcoord_second.size(), vertex_num, num_channels);
					} else if (num_channels == 2) {
						_zero_unused_elements(cur_custom, texcoord_first.size(), vertex_num, num_channels);
					}
					if (!cur_custom.is_empty()) {
						array[Mesh::ARRAY_CUSTOM0 + ((uv_i - 2) / 2)] = cur_custom; // Map uv2-uv7 to custom0-custom2
						int custom_shift = Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT + ((uv_i - 2) / 2) * Mesh::ARRAY_FORMAT_CUSTOM_BITS;
						flags |= (num_channels == 2 ? Mesh::ARRAY_CUSTOM_RG_FLOAT : Mesh::ARRAY_CUSTOM_RGBA_FLOAT) << custom_shift;
					}
				}

				if (fbx_mesh->vertex_color.exists) {
					array[Mesh::ARRAY_COLOR] = _decode_vertex_attrib_color(fbx_mesh->vertex_color, indices);
					has_vertex_color = true;
				}

				int32_t num_skin_weights = 0;

				// Find the first imported skin deformer
				for (ufbx_skin_deformer *fbx_skin : fbx_mesh->skin_deformers) {
					GLTFSkinIndex skin_i = p_state->original_skin_indices[fbx_skin->typed_id];
					if (skin_i < 0) {
						continue;
					}

					// Tag all nodes to use the skin
					for (const ufbx_node *node : fbx_mesh->instances) {
						p_state->nodes[node->typed_id]->skin = skin_i;
					}

					num_skin_weights = fbx_skin->max_weights_per_vertex > 4 ? 8 : 4;

					Vector<int32_t> bones;
					Vector<float> weights;

					bones.resize(vertex_num * num_skin_weights);
					weights.resize(vertex_num * num_skin_weights);
					for (int32_t vertex_i = 0; vertex_i < vertex_num; vertex_i++) {
						uint32_t fbx_vertex_index = fbx_mesh->vertex_indices[indices[vertex_i]];
						ufbx_skin_vertex skin_vertex = fbx_skin->vertices[fbx_vertex_index];
						float total_weight = 0.0f;
						int32_t num_weights = MIN(int32_t(skin_vertex.num_weights), num_skin_weights);
						for (int32_t i = 0; i < num_weights; i++) {
							ufbx_skin_weight skin_weight = fbx_skin->weights[skin_vertex.weight_begin + i];
							int index = vertex_i * num_skin_weights + i;
							float weight = float(skin_weight.weight);
							bones.write[index] = int(skin_weight.cluster_index);
							weights.write[index] = weight;
							total_weight += weight;
						}
						if (total_weight > 0.0f) {
							for (int32_t i = 0; i < num_weights; i++) {
								int index = vertex_i * num_skin_weights + i;
								weights.write[index] /= total_weight;
							}
						}
						// Pad the rest with empty weights
						for (int32_t i = num_weights; i < num_skin_weights; i++) {
							int index = vertex_i * num_skin_weights + i;
							bones.write[index] = 0; // TODO: What should this be padded with?
							weights.write[index] = 0.0f;
						}
					}
					array[Mesh::ARRAY_BONES] = bones;
					array[Mesh::ARRAY_WEIGHTS] = weights;

					if (num_skin_weights == 8) {
						flags |= Mesh::ARRAY_FLAG_USE_8_BONE_WEIGHTS;
					}

					// Only use the first found skin
					break;
				}

				bool generate_tangents = (primitive == Mesh::PRIMITIVE_TRIANGLES && !array[Mesh::ARRAY_TANGENT] && array[Mesh::ARRAY_TEX_UV] && array[Mesh::ARRAY_NORMAL]);

				Ref<SurfaceTool> mesh_surface_tool;
				mesh_surface_tool.instantiate();
				mesh_surface_tool->create_from_triangle_arrays(array);
				mesh_surface_tool->set_skin_weight_count(num_skin_weights == 8 ? SurfaceTool::SKIN_8_WEIGHTS : SurfaceTool::SKIN_4_WEIGHTS);
				mesh_surface_tool->index();
				if (generate_tangents) {
					//must generate mikktspace tangents.. ergh..
					mesh_surface_tool->generate_tangents();
				}
				array = mesh_surface_tool->commit_to_arrays();

				Array morphs;
				//blend shapes
				if (use_blend_shapes) {
					print_verbose("FBX: Mesh has targets");

					import_mesh->set_blend_shape_mode(Mesh::BLEND_SHAPE_MODE_NORMALIZED);

					for (const ufbx_blend_deformer *fbx_deformer : fbx_mesh->blend_deformers) {
						for (const ufbx_blend_channel *fbx_channel : fbx_deformer->channels) {
							if (fbx_channel->keyframes.count == 0) {
								continue;
							}

							// Use the last shape keyframe by default
							ufbx_blend_shape *fbx_shape = fbx_channel->keyframes[fbx_channel->keyframes.count - 1].shape;

							Array array_copy;
							array_copy.resize(Mesh::ARRAY_MAX);

							for (int l = 0; l < Mesh::ARRAY_MAX; l++) {
								array_copy[l] = array[l];
							}

							Vector<Vector3> varr;
							Vector<Vector3> narr;
							const Vector<Vector3> src_varr = array[Mesh::ARRAY_VERTEX];
							const Vector<Vector3> src_narr = array[Mesh::ARRAY_NORMAL];
							const int size = src_varr.size();
							ERR_FAIL_COND_V(size == 0, ERR_PARSE_ERROR);
							{
								varr.resize(size);
								narr.resize(size);

								Vector3 *w_varr = varr.ptrw();
								Vector3 *w_narr = narr.ptrw();
								const Vector3 *r_varr = src_varr.ptr();
								const Vector3 *r_narr = src_narr.ptr();
								for (int l = 0; l < size; l++) {
									uint32_t vertex_index = _decode_vertex_index(r_varr[l]);
									uint32_t offset_index = ufbx_get_blend_shape_offset_index(fbx_shape, vertex_index);
									Vector3 position = _as_vec3(fbx_mesh->vertices[vertex_index]);
									Vector3 normal = r_narr[l];

									if (offset_index != UFBX_NO_INDEX && offset_index < fbx_shape->position_offsets.count) {
										Vector3 blend_shape_position_offset = _as_vec3(fbx_shape->position_offsets[offset_index]);
										w_varr[l] = position + blend_shape_position_offset;
									} else {
										w_varr[l] = position;
									}

									if (offset_index != UFBX_NO_INDEX && offset_index < fbx_shape->normal_offsets.count) {
										w_narr[l] = (normal.normalized() + _as_vec3(fbx_shape->normal_offsets[offset_index])).normalized();
									} else {
										w_narr[l] = normal;
									}
								}
							}
							array_copy[Mesh::ARRAY_VERTEX] = varr;
							array_copy[Mesh::ARRAY_NORMAL] = narr;

							Ref<SurfaceTool> blend_surface_tool;
							blend_surface_tool.instantiate();
							blend_surface_tool->create_from_triangle_arrays(array_copy);
							blend_surface_tool->set_skin_weight_count(num_skin_weights == 8 ? SurfaceTool::SKIN_8_WEIGHTS : SurfaceTool::SKIN_4_WEIGHTS);
							if (generate_tangents) {
								//must generate mikktspace tangents.. ergh..
								blend_surface_tool->generate_tangents();
							}
							array_copy = blend_surface_tool->commit_to_arrays();

							// Enforce blend shape mask array format
							for (int l = 0; l < Mesh::ARRAY_MAX; l++) {
								if (!(Mesh::ARRAY_FORMAT_BLEND_SHAPE_MASK & (static_cast<int64_t>(1) << l))) {
									array_copy[l] = Variant();
								}
							}

							morphs.push_back(array_copy);
						}
					}
				}

				// Decode the original vertex positions now that we're done processing blend shapes.
				if (use_blend_shapes) {
					Vector<Vector3> varr = array[Mesh::ARRAY_VERTEX];
					Vector3 *w_varr = varr.ptrw();
					const int size = varr.size();
					for (int i = 0; i < size; i++) {
						uint32_t vertex_index = _decode_vertex_index(w_varr[i]);
						w_varr[i] = _as_vec3(fbx_mesh->vertices[vertex_index]);
					}
					array[Mesh::ARRAY_VERTEX] = varr;
				}

				Ref<Material> mat;
				String mat_name;
				if (!p_state->discard_meshes_and_materials) {
					ufbx_material *fbx_material = nullptr;
					if (fbx_mesh_part.index < fbx_mesh->materials.count) {
						fbx_material = fbx_mesh->materials[fbx_mesh_part.index];
					}
					if (fbx_material) {
						const int material = int(fbx_material->typed_id);
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
		}

		Ref<GLTFMesh> mesh;
		mesh.instantiate();
		Dictionary additional_data;
		additional_data["blend_channels"] = blend_channels;
		mesh->set_additional_data("GODOT_mesh_blend_channels", additional_data);
		mesh->set_blend_weights(blend_weights);
		mesh->set_mesh(import_mesh);
		mesh->set_name(import_mesh->get_name());
		mesh->set_original_name(original_name);

		p_state->meshes.push_back(mesh);
	}

	print_verbose("FBX: Total meshes: " + itos(p_state->meshes.size()));

	return OK;
}

Ref<Image> FBXDocument::_parse_image_bytes_into_image(Ref<FBXState> p_state, const Vector<uint8_t> &p_bytes, const String &p_filename, int p_index) {
	Ref<Image> r_image;
	r_image.instantiate();
	// Try to import first based on filename.
	String filename_lower = p_filename.to_lower();
	if (filename_lower.ends_with(".png")) {
		r_image->load_png_from_buffer(p_bytes);
	} else if (filename_lower.ends_with(".jpg")) {
		r_image->load_jpg_from_buffer(p_bytes);
	} else if (filename_lower.ends_with(".tga")) {
		r_image->load_tga_from_buffer(p_bytes);
	}
	// If we didn't pass the above tests, try loading as each option.
	if (r_image->is_empty()) { // Try PNG first.
		r_image->load_png_from_buffer(p_bytes);
	}
	if (r_image->is_empty()) { // And then JPEG.
		r_image->load_jpg_from_buffer(p_bytes);
	}
	if (r_image->is_empty()) { // And then TGA.
		r_image->load_jpg_from_buffer(p_bytes);
	}
	// If it still can't be loaded, give up and insert an empty image as placeholder.
	if (r_image->is_empty()) {
		ERR_PRINT(vformat("FBX: Couldn't load image index '%d'", p_index));
	}
	return r_image;
}

GLTFImageIndex FBXDocument::_parse_image_save_image(Ref<FBXState> p_state, const Vector<uint8_t> &p_bytes, const String &p_file_extension, int p_index, Ref<Image> p_image) {
	FBXState::GLTFHandleBinary handling = FBXState::GLTFHandleBinary(p_state->handle_binary_image);
	if (p_image->is_empty() || handling == FBXState::GLTFHandleBinary::HANDLE_BINARY_DISCARD_TEXTURES) {
		if (p_index < 0) {
			return -1;
		}
		p_state->images.push_back(Ref<Texture2D>());
		p_state->source_images.push_back(Ref<Image>());
		return p_state->images.size() - 1;
	}
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && handling == FBXState::GLTFHandleBinary::HANDLE_BINARY_EXTRACT_TEXTURES) {
		if (p_state->base_path.is_empty()) {
			if (p_index < 0) {
				return -1;
			}
			p_state->images.push_back(Ref<Texture2D>());
			p_state->source_images.push_back(Ref<Image>());
		} else if (p_image->get_name().is_empty()) {
			if (p_index < 0) {
				return -1;
			}
			WARN_PRINT(vformat("FBX: Image index '%d' couldn't be named. Skipping it.", p_index));
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
					ERR_FAIL_COND_V(err != OK, -1);
				} else {
					// If a file extension was specified, save the original bytes to a file with that extension.
					Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::WRITE, &err);
					ERR_FAIL_COND_V(err != OK, -1);
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
			Ref<Texture2D> saved_image = ResourceLoader::load(_get_texture_path(p_state->get_base_path(), file_path), "Texture2D");
			if (saved_image.is_valid()) {
				p_state->images.push_back(saved_image);
				p_state->source_images.push_back(saved_image->get_image());
			} else if (p_index < 0) {
				return -1;
			} else {
				WARN_PRINT(vformat("FBX: Image index '%d' couldn't be loaded with the name: %s. Skipping it.", p_index, p_image->get_name()));
				// Placeholder to keep count.
				p_state->images.push_back(Ref<Texture2D>());
				p_state->source_images.push_back(Ref<Image>());
			}
		}
		return p_state->images.size() - 1;
	}
#endif // TOOLS_ENABLED
	if (handling == FBXState::HANDLE_BINARY_EMBED_AS_BASISU) {
		Ref<PortableCompressedTexture2D> tex;
		tex.instantiate();
		tex->set_name(p_image->get_name());
		tex->set_keep_compressed_buffer(true);
		tex->create_from_image(p_image, PortableCompressedTexture2D::COMPRESSION_MODE_BASIS_UNIVERSAL);
		p_state->images.push_back(tex);
		p_state->source_images.push_back(p_image);
		return p_state->images.size() - 1;
	}
	// This handles the case of HANDLE_BINARY_EMBED_AS_UNCOMPRESSED, and it also serves
	// as a fallback for HANDLE_BINARY_EXTRACT_TEXTURES when this is not the editor.
	Ref<ImageTexture> tex;
	tex.instantiate();
	tex->set_name(p_image->get_name());
	tex->set_image(p_image);
	p_state->images.push_back(tex);
	p_state->source_images.push_back(p_image);
	return p_state->images.size() - 1;
}

Error FBXDocument::_parse_images(Ref<FBXState> p_state, const String &p_base_path) {
	ERR_FAIL_COND_V(p_state.is_null(), ERR_INVALID_PARAMETER);

	const ufbx_scene *fbx_scene = p_state->scene.get();
	for (int texture_i = 0; texture_i < static_cast<int>(fbx_scene->texture_files.count); texture_i++) {
		const ufbx_texture_file &fbx_texture_file = fbx_scene->texture_files[texture_i];
		String path = _as_string(fbx_texture_file.filename);
		// Use only filename for absolute paths to avoid portability issues.
		if (path.is_absolute_path()) {
			path = path.get_file();
		}
		if (!p_base_path.is_empty()) {
			path = p_base_path.path_join(path);
		}
		path = path.simplify_path();
		Vector<uint8_t> data;
		if (fbx_texture_file.content.size > 0 && fbx_texture_file.content.size <= INT_MAX) {
			data.resize(int(fbx_texture_file.content.size));
			memcpy(data.ptrw(), fbx_texture_file.content.data, fbx_texture_file.content.size);
		} else {
			String base_dir = p_state->get_base_path();
			Ref<Texture2D> texture = ResourceLoader::load(_get_texture_path(base_dir, path), "Texture2D");
			if (texture.is_valid()) {
				p_state->images.push_back(texture);
				p_state->source_images.push_back(texture->get_image());
				continue;
			}
			// Fallback to loading as byte array.
			data = FileAccess::get_file_as_bytes(path);
			if (data.size() == 0) {
				WARN_PRINT(vformat("FBX: Image index '%d' couldn't be loaded from path: %s because there was no data to load. Skipping it.", texture_i, path));
				p_state->images.push_back(Ref<Texture2D>()); // Placeholder to keep count.
				p_state->source_images.push_back(Ref<Image>());
				continue;
			}
		}

		// Parse the image data from bytes into an Image resource and save if needed.
		String file_extension;
		Ref<Image> img = _parse_image_bytes_into_image(p_state, data, path, texture_i);
		img->set_name(itos(texture_i));
		_parse_image_save_image(p_state, data, file_extension, texture_i, img);
	}

	// Create a texture for each file texture.
	for (int texture_file_i = 0; texture_file_i < static_cast<int>(fbx_scene->texture_files.count); texture_file_i++) {
		Ref<GLTFTexture> texture;
		texture.instantiate();
		texture->set_src_image(GLTFImageIndex(texture_file_i));
		p_state->textures.push_back(texture);
	}

	print_verbose("FBX: Total images: " + itos(p_state->images.size()));

	return OK;
}

Ref<Texture2D> FBXDocument::_get_texture(Ref<FBXState> p_state, const GLTFTextureIndex p_texture, int p_texture_types) {
	ERR_FAIL_INDEX_V(p_texture, p_state->textures.size(), Ref<Texture2D>());
	const GLTFImageIndex image = p_state->textures[p_texture]->get_src_image();
	ERR_FAIL_INDEX_V(image, p_state->images.size(), Ref<Texture2D>());
	if (FBXState::GLTFHandleBinary(p_state->handle_binary_image) == FBXState::HANDLE_BINARY_EMBED_AS_BASISU) {
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

Error FBXDocument::_parse_materials(Ref<FBXState> p_state) {
	const ufbx_scene *fbx_scene = p_state->scene.get();
	for (GLTFMaterialIndex material_i = 0; material_i < static_cast<GLTFMaterialIndex>(fbx_scene->materials.count); material_i++) {
		const ufbx_material *fbx_material = fbx_scene->materials[material_i];

		Ref<StandardMaterial3D> material;
		material.instantiate();
		if (fbx_material->name.length > 0) {
			material->set_name(_as_string(fbx_material->name));
		} else {
			material->set_name(vformat("material_%s", itos(material_i)));
		}
		material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		Dictionary material_extensions;

		if (fbx_material->pbr.base_color.has_value) {
			Color albedo = _material_color(fbx_material->pbr.base_color, fbx_material->pbr.base_factor);
			material->set_albedo(albedo.linear_to_srgb());
		}

		if (fbx_material->features.double_sided.enabled) {
			material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		}

		const ufbx_texture *base_texture = _get_file_texture(fbx_material->pbr.base_color.texture);
		if (base_texture) {
			material->set_u_repeat_mode(base_texture->wrap_u == UFBX_WRAP_REPEAT ? BaseMaterial3D::REPEAT_MODE_REPEAT : BaseMaterial3D::REPEAT_MODE_CLAMP_TO_EDGE);
			material->set_v_repeat_mode(base_texture->wrap_v == UFBX_WRAP_REPEAT ? BaseMaterial3D::REPEAT_MODE_REPEAT : BaseMaterial3D::REPEAT_MODE_CLAMP_TO_EDGE);

			Ref<Texture2D> albedo_texture = _get_texture(p_state, GLTFTextureIndex(base_texture->file_index), TEXTURE_TYPE_GENERIC);

			// Search for transparency map.
			Ref<Texture2D> transparency_texture;
			const ufbx_texture *transparency_sources[] = {
				fbx_material->pbr.opacity.texture,
				fbx_material->fbx.transparency_color.texture,
			};
			for (const ufbx_texture *transparency_source : transparency_sources) {
				const ufbx_texture *fbx_transparency_texture = _get_file_texture(transparency_source);
				if (fbx_transparency_texture) {
					transparency_texture = _get_texture(p_state, GLTFTextureIndex(fbx_transparency_texture->file_index), TEXTURE_TYPE_GENERIC);
					if (transparency_texture.is_valid()) {
						break;
					}
				}
			}

			// Multiply the albedo alpha with the transparency texture if necessary.
			if (albedo_texture.is_valid() && transparency_texture.is_valid() && albedo_texture != transparency_texture) {
				Pair<uint64_t, uint64_t> key = { albedo_texture->get_rid().get_id(), transparency_texture->get_rid().get_id() };
				GLTFTextureIndex *texture_index_ptr = p_state->albedo_transparency_textures.getptr(key);
				if (texture_index_ptr != nullptr) {
					if (*texture_index_ptr >= 0) {
						albedo_texture = _get_texture(p_state, *texture_index_ptr, TEXTURE_TYPE_GENERIC);
					}
				} else {
					Ref<Image> albedo_image = _get_decompressed_image(albedo_texture);
					Ref<Image> transparency_image = _get_decompressed_image(transparency_texture);

					if (albedo_image.is_valid() && transparency_image.is_valid()) {
						albedo_image->convert(Image::Format::FORMAT_RGBA8);
						transparency_image->resize(albedo_texture->get_width(), albedo_texture->get_height(), Image::INTERPOLATE_LANCZOS);
						for (int y = 0; y < albedo_image->get_height(); y++) {
							for (int x = 0; x < albedo_image->get_width(); x++) {
								Color albedo_pixel = albedo_image->get_pixel(x, y);
								Color transparency_pixel = transparency_image->get_pixel(x, y);
								albedo_pixel.a *= transparency_pixel.r;
								albedo_image->set_pixel(x, y, albedo_pixel);
							}
						}

						albedo_image->clear_mipmaps();
						albedo_image->generate_mipmaps();

						albedo_image->set_name(vformat("alpha_%d", p_state->albedo_transparency_textures.size()));

						GLTFImageIndex new_image = _parse_image_save_image(p_state, PackedByteArray(), "", -1, albedo_image);
						if (new_image >= 0) {
							Ref<GLTFTexture> new_texture;
							new_texture.instantiate();
							new_texture->set_src_image(GLTFImageIndex(new_image));
							p_state->textures.push_back(new_texture);

							GLTFTextureIndex texture_index = p_state->textures.size() - 1;
							p_state->albedo_transparency_textures[key] = texture_index;

							albedo_texture = _get_texture(p_state, texture_index, TEXTURE_TYPE_GENERIC);
						} else {
							WARN_PRINT(vformat("FBX: Could not save modified albedo texture from RID (%d, %d).", key.first, key.second));
							p_state->albedo_transparency_textures[key] = -1;
						}
					}
				}
			}

			Image::AlphaMode alpha_mode;
			if (albedo_texture.is_valid()) {
				Image::AlphaMode *alpha_mode_ptr = p_state->alpha_mode_cache.getptr(albedo_texture->get_rid().get_id());
				if (alpha_mode_ptr != nullptr) {
					alpha_mode = *alpha_mode_ptr;
				} else {
					Ref<Image> albedo_image = _get_decompressed_image(albedo_texture);
					alpha_mode = albedo_image->detect_alpha();
					p_state->alpha_mode_cache[albedo_texture->get_rid().get_id()] = alpha_mode;
				}

				if (alpha_mode == Image::ALPHA_BLEND) {
					material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
				} else if (alpha_mode == Image::ALPHA_BIT) {
					material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
				}
				material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, albedo_texture);
			}

			// Combined textures and factors are very unreliable in FBX
			Color albedo_factor = Color(1, 1, 1);
			if (fbx_material->pbr.base_factor.has_value) {
				albedo_factor *= (float)fbx_material->pbr.base_factor.value_real;
			}
			material->set_albedo(albedo_factor.linear_to_srgb());

			// TODO: Does not support rotation, could be inverted?
			material->set_uv1_offset(_as_vec3(base_texture->uv_transform.translation));
			Vector3 scale = _as_vec3(base_texture->uv_transform.scale);
			material->set_uv1_scale(scale);
		}

		if (fbx_material->features.pbr.enabled) {
			if (fbx_material->pbr.metalness.has_value) {
				material->set_metallic(float(fbx_material->pbr.metalness.value_real));
			} else {
				material->set_metallic(1.0);
			}

			if (fbx_material->pbr.roughness.has_value) {
				material->set_roughness(float(fbx_material->pbr.roughness.value_real));
			} else {
				material->set_roughness(1.0);
			}

			const ufbx_texture *metalness_texture = _get_file_texture(fbx_material->pbr.metalness.texture);
			if (metalness_texture) {
				material->set_texture(BaseMaterial3D::TEXTURE_METALLIC, _get_texture(p_state, GLTFTextureIndex(metalness_texture->file_index), TEXTURE_TYPE_GENERIC));
				material->set_metallic_texture_channel(BaseMaterial3D::TEXTURE_CHANNEL_RED);
				material->set_metallic(1.0);
			}

			const ufbx_texture *roughness_texture = _get_file_texture(fbx_material->pbr.roughness.texture);
			if (roughness_texture) {
				material->set_texture(BaseMaterial3D::TEXTURE_ROUGHNESS, _get_texture(p_state, GLTFTextureIndex(roughness_texture->file_index), TEXTURE_TYPE_GENERIC));
				material->set_roughness_texture_channel(BaseMaterial3D::TEXTURE_CHANNEL_RED);
				material->set_roughness(1.0);
			}
		}

		const ufbx_texture *normal_texture = _get_file_texture(fbx_material->pbr.normal_map.texture);
		if (normal_texture) {
			material->set_texture(BaseMaterial3D::TEXTURE_NORMAL, _get_texture(p_state, GLTFTextureIndex(normal_texture->file_index), TEXTURE_TYPE_NORMAL));
			material->set_feature(BaseMaterial3D::FEATURE_NORMAL_MAPPING, true);
			if (fbx_material->pbr.normal_map.has_value) {
				material->set_normal_scale(fbx_material->pbr.normal_map.value_real);
			}
		}

		const ufbx_texture *occlusion_texture = _get_file_texture(fbx_material->pbr.ambient_occlusion.texture);
		if (occlusion_texture) {
			material->set_texture(BaseMaterial3D::TEXTURE_AMBIENT_OCCLUSION, _get_texture(p_state, GLTFTextureIndex(occlusion_texture->file_index), TEXTURE_TYPE_GENERIC));
			material->set_ao_texture_channel(BaseMaterial3D::TEXTURE_CHANNEL_RED);
			material->set_feature(BaseMaterial3D::FEATURE_AMBIENT_OCCLUSION, true);
		}

		if (fbx_material->pbr.emission_color.has_value) {
			material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
			material->set_emission(_material_color(fbx_material->pbr.emission_color).linear_to_srgb());
			material->set_emission_energy_multiplier(float(fbx_material->pbr.emission_factor.value_real));
		}

		const ufbx_texture *emission_texture = _get_file_texture(fbx_material->pbr.emission_color.texture);
		if (emission_texture) {
			material->set_texture(BaseMaterial3D::TEXTURE_EMISSION, _get_texture(p_state, GLTFTextureIndex(emission_texture->file_index), TEXTURE_TYPE_GENERIC));
			material->set_feature(BaseMaterial3D::FEATURE_EMISSION, true);
			material->set_emission(Color(0, 0, 0));
		}

		if (fbx_material->features.double_sided.enabled && fbx_material->features.double_sided.is_explicit) {
			material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		}
		p_state->materials.push_back(material);
	}

	print_verbose("Total materials: " + itos(p_state->materials.size()));

	return OK;
}
Error FBXDocument::_parse_cameras(Ref<FBXState> p_state) {
	const ufbx_scene *fbx_scene = p_state->scene.get();
	for (GLTFCameraIndex i = 0; i < static_cast<GLTFCameraIndex>(fbx_scene->cameras.count); i++) {
		const ufbx_camera *fbx_camera = fbx_scene->cameras[i];

		Ref<GLTFCamera> camera;
		camera.instantiate();
		camera->set_name(_as_string(fbx_camera->name));
		if (fbx_camera->projection_mode == UFBX_PROJECTION_MODE_PERSPECTIVE) {
			camera->set_perspective(true);
			camera->set_fov(Math::deg_to_rad(real_t(fbx_camera->field_of_view_deg.y)));
		} else {
			camera->set_perspective(false);
			camera->set_size_mag(real_t(fbx_camera->orthographic_size.y * 0.5f));
		}
		if (fbx_camera->near_plane != 0.0f) {
			camera->set_depth_near(fbx_camera->near_plane);
		}
		if (fbx_camera->far_plane != 0.0f) {
			camera->set_depth_far(fbx_camera->far_plane);
		}
		p_state->cameras.push_back(camera);
	}

	print_verbose("FBX: Total cameras: " + itos(p_state->cameras.size()));

	return OK;
}

Error FBXDocument::_parse_animations(Ref<FBXState> p_state) {
	const ufbx_scene *fbx_scene = p_state->scene.get();
	for (GLTFAnimationIndex animation_i = 0; animation_i < static_cast<GLTFAnimationIndex>(fbx_scene->anim_stacks.count); animation_i++) {
		const ufbx_anim_stack *fbx_anim_stack = fbx_scene->anim_stacks[animation_i];

		Ref<GLTFAnimation> animation;
		animation.instantiate();

		if (fbx_anim_stack->name.length > 0) {
			const String anim_name = _as_string(fbx_anim_stack->name);
			const String anim_name_lower = anim_name.to_lower();
			if (anim_name_lower.begins_with("loop") || anim_name_lower.ends_with("loop") || anim_name_lower.begins_with("cycle") || anim_name_lower.ends_with("cycle")) {
				animation->set_loop(true);
			}
			animation->set_original_name(anim_name);
			animation->set_name(_gen_unique_animation_name(p_state, anim_name));
		}

		Dictionary additional_data;
		additional_data["time_begin"] = fbx_anim_stack->time_begin;
		additional_data["time_end"] = fbx_anim_stack->time_end;
		animation->set_additional_data("GODOT_animation_time_begin_time_end", additional_data);
		ufbx_bake_opts opts = {};
		opts.resample_rate = p_state->get_bake_fps();
		opts.minimum_sample_rate = p_state->get_bake_fps();
		opts.max_keyframe_segments = 1024;

		ufbx_error error;
		ufbx_unique_ptr<ufbx_baked_anim> fbx_baked_anim{ ufbx_bake_anim(fbx_scene, fbx_anim_stack->anim, &opts, &error) };
		if (!fbx_baked_anim) {
			char err_buf[512];
			ufbx_format_error(err_buf, sizeof(err_buf), &error);
			ERR_FAIL_V_MSG(FAILED, err_buf);
		}

		for (const ufbx_baked_node &fbx_baked_node : fbx_baked_anim->nodes) {
			const GLTFNodeIndex node = fbx_baked_node.typed_id;
			GLTFAnimation::NodeTrack &track = animation->get_node_tracks()[node];

			for (const ufbx_baked_vec3 &key : fbx_baked_node.translation_keys) {
				track.position_track.times.push_back(float(key.time));
				track.position_track.values.push_back(_as_vec3(key.value));
			}

			for (const ufbx_baked_quat &key : fbx_baked_node.rotation_keys) {
				track.rotation_track.times.push_back(float(key.time));
				track.rotation_track.values.push_back(_as_quaternion(key.value));
			}

			for (const ufbx_baked_vec3 &key : fbx_baked_node.scale_keys) {
				track.scale_track.times.push_back(float(key.time));
				track.scale_track.values.push_back(_as_vec3(key.value));
			}
		}

		Dictionary blend_shape_animations;

		for (const ufbx_baked_element &fbx_baked_element : fbx_baked_anim->elements) {
			const ufbx_element *fbx_element = fbx_scene->elements[fbx_baked_element.element_id];

			for (const ufbx_baked_prop &fbx_baked_prop : fbx_baked_element.props) {
				String prop_name = _as_string(fbx_baked_prop.name);

				if (fbx_element->type == UFBX_ELEMENT_BLEND_CHANNEL && prop_name == UFBX_DeformPercent) {
					const ufbx_blend_channel *fbx_blend_channel = ufbx_as_blend_channel(fbx_element);

					int blend_i = fbx_blend_channel->typed_id;
					Vector<real_t> track_times;
					Vector<real_t> track_values;

					for (const ufbx_baked_vec3 &key : fbx_baked_prop.keys) {
						track_times.push_back(float(key.time));
						track_values.push_back(real_t(key.value.x / 100.0));
					}

					Dictionary track;
					track["times"] = track_times;
					track["values"] = track_values;
					blend_shape_animations[blend_i] = track;
				}
			}
		}

		animation->set_additional_data("GODOT_blend_shape_animations", blend_shape_animations);

		p_state->animations.push_back(animation);
	}

	print_verbose("FBX: Total animations '" + itos(p_state->animations.size()) + "'.");

	return OK;
}

void FBXDocument::_assign_node_names(Ref<FBXState> p_state) {
	for (int i = 0; i < p_state->nodes.size(); i++) {
		Ref<GLTFNode> fbx_node = p_state->nodes[i];

		// Any joints get unique names generated when the skeleton is made, unique to the skeleton
		if (fbx_node->skeleton >= 0) {
			continue;
		}

		if (fbx_node->get_name().is_empty()) {
			if (fbx_node->mesh >= 0) {
				fbx_node->set_name(_gen_unique_name(p_state->unique_names, "Mesh"));
			} else if (fbx_node->camera >= 0) {
				fbx_node->set_name(_gen_unique_name(p_state->unique_names, "Camera3D"));
			} else {
				fbx_node->set_name(_gen_unique_name(p_state->unique_names, "Node"));
			}
		}

		fbx_node->set_name(_gen_unique_name(p_state->unique_names, fbx_node->get_name()));
	}
}

BoneAttachment3D *FBXDocument::_generate_bone_attachment(Ref<FBXState> p_state, Skeleton3D *p_skeleton, const GLTFNodeIndex p_node_index, const GLTFNodeIndex p_bone_index) {
	Ref<GLTFNode> fbx_node = p_state->nodes[p_node_index];
	Ref<GLTFNode> bone_node = p_state->nodes[p_bone_index];
	BoneAttachment3D *bone_attachment = memnew(BoneAttachment3D);
	print_verbose("FBX: Creating bone attachment for: " + fbx_node->get_name());

	ERR_FAIL_COND_V(!bone_node->joint, nullptr);

	bone_attachment->set_bone_name(bone_node->get_name());

	return bone_attachment;
}

ImporterMeshInstance3D *FBXDocument::_generate_mesh_instance(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index) {
	Ref<GLTFNode> fbx_node = p_state->nodes[p_node_index];

	ERR_FAIL_INDEX_V(fbx_node->mesh, p_state->meshes.size(), nullptr);

	ImporterMeshInstance3D *mi = memnew(ImporterMeshInstance3D);
	print_verbose("FBX: Creating mesh for: " + fbx_node->get_name());

	p_state->scene_mesh_instances.insert(p_node_index, mi);
	Ref<GLTFMesh> mesh = p_state->meshes.write[fbx_node->mesh];
	if (mesh.is_null()) {
		return mi;
	}
	Ref<ImporterMesh> import_mesh = mesh->get_mesh();
	if (import_mesh.is_null()) {
		return mi;
	}
	mi->set_mesh(import_mesh);
	return mi;
}

Camera3D *FBXDocument::_generate_camera(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index) {
	Ref<GLTFNode> fbx_node = p_state->nodes[p_node_index];

	ERR_FAIL_INDEX_V(fbx_node->camera, p_state->cameras.size(), nullptr);

	print_verbose("FBX: Creating camera for: " + fbx_node->get_name());

	Ref<GLTFCamera> c = p_state->cameras[fbx_node->camera];
	return c->to_node();
}

Light3D *FBXDocument::_generate_light(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index) {
	Ref<GLTFNode> fbx_node = p_state->nodes[p_node_index];

	ERR_FAIL_INDEX_V(fbx_node->light, p_state->lights.size(), nullptr);

	print_verbose("FBX: Creating light for: " + fbx_node->get_name());

	Ref<GLTFLight> l = p_state->lights[fbx_node->light];
	Light3D *light = nullptr;

	if (l->get_light_type() == "point") {
		light = memnew(OmniLight3D);
	} else if (l->get_light_type() == "directional") {
		light = memnew(DirectionalLight3D);
	} else if (l->get_light_type() == "spot") {
		light = memnew(SpotLight3D);
	} else {
		ERR_FAIL_NULL_V(light, nullptr);
	}

	if (light) {
		light->set_name(l->get_name());
		light->set_color(l->get_color());
		light->set_param(Light3D::PARAM_ENERGY, l->get_intensity());
		Dictionary additional_data = l->get_additional_data("GODOT_fbx_light");
		if (additional_data.has("castShadows")) {
			light->set_shadow(additional_data["castShadows"]);
		}
		if (additional_data.has("castLight")) {
			light->set_visible(additional_data["castLight"]);
		}

		Transform3D transform;
		DirectionalLight3D *dir_light = Object::cast_to<DirectionalLight3D>(light);
		SpotLight3D *spot_light = Object::cast_to<SpotLight3D>(light);
		OmniLight3D *omni_light = Object::cast_to<OmniLight3D>(light);
		if (dir_light) {
			dir_light->set_transform(transform);
		} else if (spot_light) {
			spot_light->set_transform(transform);
			spot_light->set_param(SpotLight3D::PARAM_SPOT_ANGLE, l->get_outer_cone_angle() / 2.0f);
		}
		if (omni_light || spot_light) {
			light->set_param(OmniLight3D::PARAM_RANGE, 4096);
		}

// This is "correct", but FBX files may have unexpected decay modes.
// Also does not match with what FBX2glTF does, so it might be better to not do any of this..
#if 0
		if (omni_light || spot_light) {
			float attenuation = 1.0f;
			if (additional_data.has("decay")) {
				String decay_type = additional_data["decay"];
				if (decay_type == "none") {
					attenuation = 0.001f;
				} else if (decay_type == "linear") {
					attenuation = 1.0f;
				} else if (decay_type == "quadratic") {
					attenuation = 2.0f;
				} else if (decay_type == "cubic") {
					attenuation = 3.0f;
				}
			}
			light->set_param(Light3D::PARAM_ATTENUATION, attenuation);
		}
#endif

		if (spot_light) {
			// Line of best fit derived from guessing, see https://www.desmos.com/calculator/biiflubp8b
			// The points in desmos are not exact, except for (1, infinity).
			float angle_ratio = l->get_inner_cone_angle() / l->get_outer_cone_angle();
			float angle_attenuation = 0.2 / (1 - angle_ratio) - 0.1;
			light->set_param(SpotLight3D::PARAM_SPOT_ATTENUATION, angle_attenuation);
		}
	}

	return light;
}

Node3D *FBXDocument::_generate_spatial(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index) {
	Ref<GLTFNode> fbx_node = p_state->nodes[p_node_index];

	Node3D *spatial = memnew(Node3D);
	print_verbose("FBX: Converting spatial: " + fbx_node->get_name());

	return spatial;
}

void FBXDocument::_generate_scene_node(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index, Node *p_scene_parent, Node *p_scene_root) {
	Ref<GLTFNode> fbx_node = p_state->nodes[p_node_index];

	if (fbx_node->skeleton >= 0) {
		_generate_skeleton_bone_node(p_state, p_node_index, p_scene_parent, p_scene_root);
		return;
	}

	Node3D *current_node = nullptr;

	// Is our parent a skeleton
	Skeleton3D *active_skeleton = Object::cast_to<Skeleton3D>(p_scene_parent);

	const bool non_bone_parented_to_skeleton = active_skeleton;

	// skinned meshes must not be placed in a bone attachment.
	if (non_bone_parented_to_skeleton && fbx_node->skin < 0) {
		// Bone Attachment - Parent Case
		BoneAttachment3D *bone_attachment = _generate_bone_attachment(p_state, active_skeleton, p_node_index, fbx_node->parent);

		p_scene_parent->add_child(bone_attachment, true);
		bone_attachment->set_owner(p_scene_root);

		// There is no fbx_node that represent this, so just directly create a unique name
		bone_attachment->set_name(fbx_node->get_name());

		// We change the scene_parent to our bone attachment now. We do not set current_node because we want to make the node
		// and attach it to the bone_attachment
		p_scene_parent = bone_attachment;
	}
	if (!current_node) {
		if (fbx_node->skin >= 0 && fbx_node->mesh >= 0 && !fbx_node->children.is_empty()) {
			current_node = _generate_spatial(p_state, p_node_index);
			Node3D *mesh_inst = _generate_mesh_instance(p_state, p_node_index);
			mesh_inst->set_name(fbx_node->get_name());

			current_node->add_child(mesh_inst, true);
		} else if (fbx_node->mesh >= 0) {
			current_node = _generate_mesh_instance(p_state, p_node_index);
		} else if (fbx_node->camera >= 0) {
			current_node = _generate_camera(p_state, p_node_index);
		} else if (fbx_node->light >= 0) {
			current_node = _generate_light(p_state, p_node_index);
		} else {
			current_node = _generate_spatial(p_state, p_node_index);
		}
	}

	ERR_FAIL_NULL(current_node);

	// Add the node we generated and set the owner to the scene root.
	p_scene_parent->add_child(current_node, true);
	if (current_node != p_scene_root) {
		Array args;
		args.append(p_scene_root);
		current_node->propagate_call(StringName("set_owner"), args);
	}
	current_node->set_transform(fbx_node->transform);
	current_node->set_name(fbx_node->get_name());

	p_state->scene_nodes.insert(p_node_index, current_node);
	for (int i = 0; i < fbx_node->children.size(); ++i) {
		_generate_scene_node(p_state, fbx_node->children[i], current_node, p_scene_root);
	}
}

void FBXDocument::_generate_skeleton_bone_node(Ref<FBXState> p_state, const GLTFNodeIndex p_node_index, Node *p_scene_parent, Node *p_scene_root) {
	Ref<GLTFNode> fbx_node = p_state->nodes[p_node_index];

	Node3D *current_node = nullptr;

	Skeleton3D *skeleton = p_state->skeletons[fbx_node->skeleton]->godot_skeleton;
	// In this case, this node is already a bone in skeleton.
	const bool is_skinned_mesh = (fbx_node->skin >= 0 && fbx_node->mesh >= 0);
	const bool requires_extra_node = (fbx_node->mesh >= 0 || fbx_node->camera >= 0 || fbx_node->light >= 0);

	Skeleton3D *active_skeleton = Object::cast_to<Skeleton3D>(p_scene_parent);
	if (active_skeleton != skeleton) {
		if (active_skeleton) {
			// Should no longer be possible.
			ERR_PRINT(vformat("FBX: Generating scene detected direct parented Skeletons at node %d", p_node_index));
			BoneAttachment3D *bone_attachment = _generate_bone_attachment(p_state, active_skeleton, p_node_index, fbx_node->parent);
			p_scene_parent->add_child(bone_attachment, true);
			bone_attachment->set_owner(p_scene_root);
			// There is no fbx_node that represent this, so just directly create a unique name
			bone_attachment->set_name(_gen_unique_name(p_state->unique_names, "BoneAttachment3D"));
			// We change the scene_parent to our bone attachment now. We do not set current_node because we want to make the node
			// and attach it to the bone_attachment
			p_scene_parent = bone_attachment;
		}
		if (skeleton->get_parent() == nullptr) {
			p_scene_parent->add_child(skeleton, true);
			skeleton->set_owner(p_scene_root);
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
			bone_attachment->set_owner(p_scene_root);

			// There is no fbx_node that represent this, so just directly create a unique name
			bone_attachment->set_name(fbx_node->get_name());

			// We change the scene_parent to our bone attachment now. We do not set current_node because we want to make the node
			// and attach it to the bone_attachment
			p_scene_parent = bone_attachment;
		}
		// TODO: 20240118 // fire
		// // Check if any GLTFDocumentExtension classes want to generate a node for us.
		// for (Ref<GLTFDocumentExtension> ext : document_extensions) {
		// 	ERR_CONTINUE(ext.is_null());
		// 	current_node = ext->generate_scene_node(p_state, fbx_node, p_scene_parent);
		// 	if (current_node) {
		// 		break;
		// 	}
		// }
		// If none of our GLTFDocumentExtension classes generated us a node, we generate one.
		if (!current_node) {
			if (fbx_node->mesh >= 0) {
				current_node = _generate_mesh_instance(p_state, p_node_index);
			} else if (fbx_node->camera >= 0) {
				current_node = _generate_camera(p_state, p_node_index);
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
		current_node->set_name(fbx_node->get_name());
	}

	p_state->scene_nodes.insert(p_node_index, current_node);

	for (int i = 0; i < fbx_node->children.size(); ++i) {
		_generate_scene_node(p_state, fbx_node->children[i], active_skeleton, p_scene_root);
	}
}

void FBXDocument::_import_animation(Ref<FBXState> p_state, AnimationPlayer *p_animation_player, const GLTFAnimationIndex p_index, const bool p_trimming, const bool p_remove_immutable_tracks) {
	Ref<GLTFAnimation> anim = p_state->animations[p_index];

	String anim_name = anim->get_name();
	if (anim_name.is_empty()) {
		// No node represent these, and they are not in the hierarchy, so just make a unique name
		anim_name = _gen_unique_name(p_state->unique_names, "Animation");
	}

	Ref<Animation> animation;
	animation.instantiate();
	animation->set_name(anim_name);
	animation->set_step(1.0 / p_state->get_bake_fps());

	if (anim->get_loop()) {
		animation->set_loop_mode(Animation::LOOP_LINEAR);
	}

	Dictionary additional_animation_data = anim->get_additional_data("GODOT_animation_time_begin_time_end");

	double anim_start_offset = p_trimming ? double(additional_animation_data["time_begin"]) : 0.0;

	for (const KeyValue<int, GLTFAnimation::NodeTrack> &track_i : anim->get_node_tracks()) {
		const GLTFAnimation::NodeTrack &track = track_i.value;
		//need to find the path: for skeletons, weight tracks will affect the mesh
		NodePath node_path;
		//for skeletons, transform tracks always affect bones
		NodePath transform_node_path;
		GLTFNodeIndex node_index = track_i.key;
		Node *root = p_animation_player->get_parent();
		ERR_FAIL_NULL(root);
		HashMap<GLTFNodeIndex, Node *>::Iterator node_element = p_state->scene_nodes.find(node_index);
		ERR_CONTINUE_MSG(!node_element, vformat("Unable to find node %d for animation.", node_index));
		node_path = root->get_path_to(node_element->value);

		const Ref<GLTFNode> fbx_node = p_state->nodes[track_i.key];

		if (fbx_node->skeleton >= 0) {
			const Skeleton3D *sk = p_state->skeletons[fbx_node->skeleton]->godot_skeleton;
			ERR_FAIL_NULL(sk);

			const String path = p_animation_player->get_parent()->get_path_to(sk);
			const String bone = fbx_node->get_name();
			transform_node_path = path + ":" + bone;
		} else {
			transform_node_path = node_path;
		}

		// Animated TRS properties will not affect a skinned mesh.
		const bool transform_affects_skinned_mesh_instance = fbx_node->skeleton < 0 && fbx_node->skin >= 0;
		if ((track.rotation_track.values.size() || track.position_track.values.size() || track.scale_track.values.size()) && !transform_affects_skinned_mesh_instance) {
			// Make a transform track.
			int base_idx = animation->get_track_count();
			int position_idx = -1;
			int rotation_idx = -1;
			int scale_idx = -1;

			if (track.position_track.values.size()) {
				bool is_default = true; // Discard the track if all it contains is default values.
				if (p_remove_immutable_tracks) {
					Vector3 base_pos = p_state->nodes[track_i.key]->transform.origin;
					for (int i = 0; i < track.position_track.times.size(); i++) {
						Vector3 value = track.position_track.values[track.position_track.interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE ? (1 + i * 3) : i];
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
					animation->track_set_imported(position_idx, true); // Helps merging positions later.
					base_idx++;
				}
			}
			if (track.rotation_track.values.size()) {
				bool is_default = true; // Discard the track if all the track contains is the default values.
				if (p_remove_immutable_tracks) {
					Quaternion base_rot = p_state->nodes[track_i.key]->transform.basis.get_rotation_quaternion();
					for (int i = 0; i < track.rotation_track.times.size(); i++) {
						Quaternion value = track.rotation_track.values[track.rotation_track.interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE ? (1 + i * 3) : i].normalized();
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
					base_idx++;
				}
			}
			if (track.scale_track.values.size()) {
				bool is_default = true; // Discard the track if all the track contains is the default values.
				if (p_remove_immutable_tracks) {
					Vector3 base_scale = p_state->nodes[track_i.key]->transform.basis.get_scale();
					for (int i = 0; i < track.scale_track.times.size(); i++) {
						Vector3 value = track.scale_track.values[track.scale_track.interpolation == GLTFAnimation::INTERP_CUBIC_SPLINE ? (1 + i * 3) : i];
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
					base_idx++;
				}
			}

			if (position_idx != -1) {
				animation->track_set_interpolation_type(position_idx, Animation::INTERPOLATION_LINEAR);
				for (int j = 0; j < track.position_track.times.size(); j++) {
					const float t = track.position_track.times[j] - anim_start_offset;
					const Vector3 value = track.position_track.values[j];
					animation->position_track_insert_key(position_idx, t, value);
				}
			}

			if (rotation_idx != -1) {
				animation->track_set_interpolation_type(rotation_idx, Animation::INTERPOLATION_LINEAR);
				for (int j = 0; j < track.rotation_track.times.size(); j++) {
					const float t = track.rotation_track.times[j] - anim_start_offset;
					const Quaternion value = track.rotation_track.values[j];
					animation->rotation_track_insert_key(rotation_idx, t, value);
				}
			}

			if (scale_idx != -1) {
				animation->track_set_interpolation_type(scale_idx, Animation::INTERPOLATION_LINEAR);
				for (int j = 0; j < track.scale_track.times.size(); j++) {
					const float t = track.scale_track.times[j] - anim_start_offset;
					const Vector3 value = track.scale_track.values[j];
					animation->scale_track_insert_key(scale_idx, t, value);
				}
			}
		}
	}

	Dictionary blend_shape_animations = anim->get_additional_data("GODOT_blend_shape_animations");

	for (GLTFNodeIndex node_index = 0; node_index < p_state->nodes.size(); node_index++) {
		Ref<GLTFNode> node = p_state->nodes[node_index];
		if (node->mesh < 0) {
			continue;
		}

		// For meshes, especially skinned meshes, there are cases where it will be added as a child.
		NodePath mesh_instance_node_path;

		Node *root = p_animation_player->get_parent();
		ERR_FAIL_NULL(root);
		HashMap<GLTFNodeIndex, Node *>::Iterator node_element = p_state->scene_nodes.find(node_index);
		ERR_CONTINUE_MSG(!node_element, vformat("Unable to find node %d for animation.", node_index));
		NodePath node_path = root->get_path_to(node_element->value);
		HashMap<GLTFNodeIndex, ImporterMeshInstance3D *>::Iterator mesh_instance_element = p_state->scene_mesh_instances.find(node_index);
		if (mesh_instance_element) {
			mesh_instance_node_path = root->get_path_to(mesh_instance_element->value);
		} else {
			mesh_instance_node_path = node_path;
		}

		Ref<GLTFMesh> mesh = p_state->meshes[node->mesh];
		ERR_CONTINUE(mesh.is_null());
		ERR_CONTINUE(mesh->get_mesh().is_null());
		ERR_CONTINUE(mesh->get_mesh()->get_mesh().is_null());

		Dictionary mesh_additional_data = mesh->get_additional_data("GODOT_mesh_blend_channels");
		Vector<int> blend_channels = mesh_additional_data["blend_channels"];

		for (int i = 0; i < blend_channels.size(); i++) {
			int blend_i = blend_channels[i];
			if (!blend_shape_animations.has(blend_i)) {
				continue;
			}
			Dictionary blend_track = blend_shape_animations[blend_i];

			GLTFAnimation::Channel<real_t> weights;
			weights.interpolation = GLTFAnimation::INTERP_LINEAR;
			weights.times = blend_track["times"];
			weights.values = blend_track["values"];

			const String blend_path = String(mesh_instance_node_path) + ":" + String(mesh->get_mesh()->get_blend_shape_name(i));
			const int track_idx = animation->get_track_count();
			animation->add_track(Animation::TYPE_BLEND_SHAPE);
			animation->track_set_path(track_idx, blend_path);
			animation->track_set_imported(track_idx, true); // Helps merging later.

			animation->track_set_interpolation_type(track_idx, Animation::INTERPOLATION_LINEAR);
			for (int j = 0; j < weights.times.size(); j++) {
				const double t = weights.times[j] - anim_start_offset;
				const real_t attribs = weights.values[j];
				animation->blend_shape_track_insert_key(track_idx, t, attribs);
			}
		}
	}
	double time_begin = additional_animation_data["time_begin"];
	double time_end = additional_animation_data["time_end"];
	double length = p_trimming ? time_end - time_begin : time_end;
	animation->set_length(length);

	Ref<AnimationLibrary> library;
	if (!p_animation_player->has_animation_library("")) {
		library.instantiate();
		p_animation_player->add_animation_library("", library);
	} else {
		library = p_animation_player->get_animation_library("");
	}
	library->add_animation(anim_name, animation);
}

void FBXDocument::_process_mesh_instances(Ref<FBXState> p_state, Node *p_scene_root) {
	for (GLTFNodeIndex node_i = 0; node_i < p_state->nodes.size(); ++node_i) {
		Ref<GLTFNode> node = p_state->nodes[node_i];

		if (node.is_null() || !(node->skin >= 0 && node->mesh >= 0)) {
			continue;
		}

		const GLTFSkinIndex skin_i = node->skin;

		ImporterMeshInstance3D *mi = nullptr;
		HashMap<GLTFNodeIndex, ImporterMeshInstance3D *>::Iterator mi_element = p_state->scene_mesh_instances.find(node_i);
		if (!mi_element) {
			HashMap<GLTFNodeIndex, Node *>::Iterator si_element = p_state->scene_nodes.find(node_i);
			ERR_CONTINUE_MSG(!si_element, vformat("Unable to find node %d", node_i));
			mi = Object::cast_to<ImporterMeshInstance3D>(si_element->value);
			ERR_CONTINUE_MSG(mi == nullptr, vformat("Unable to cast node %d of type %s to ImporterMeshInstance3D", node_i, si_element->value->get_class_name()));
		} else {
			mi = mi_element->value;
		}

		bool is_skin_valid = node->skin >= 0;
		bool is_skin_accessible = is_skin_valid && node->skin < p_state->skins.size();
		bool is_valid = is_skin_accessible && p_state->skins.write[node->skin]->skeleton >= 0;

		if (!is_valid) {
			continue;
		}

		const GLTFSkeletonIndex skel_i = p_state->skins.write[node->skin]->skeleton;
		Ref<GLTFSkeleton> fbx_skeleton = p_state->skeletons.write[skel_i];
		Skeleton3D *skeleton = fbx_skeleton->godot_skeleton;
		ERR_CONTINUE_MSG(skeleton == nullptr, vformat("Unable to find Skeleton for node %d skin %d", node_i, skin_i));

		mi->get_parent()->remove_child(mi);
		mi->set_owner(nullptr);
		skeleton->add_child(mi, true);
		mi->set_owner(skeleton->get_owner());

		mi->set_skin(p_state->skins.write[skin_i]->godot_skin);
		mi->set_skeleton_path(mi->get_path_to(skeleton));
		mi->set_transform(Transform3D());
	}
}

Error FBXDocument::_parse(Ref<FBXState> p_state, String p_path, Ref<FileAccess> p_file) {
	p_state->scene.reset();

	Error err = ERR_INVALID_DATA;
	if (p_file.is_null()) {
		return FAILED;
	}

	ufbx_load_opts opts = {};
	opts.target_axes = ufbx_axes_right_handed_y_up;
	opts.target_unit_meters = 1.0f;
	opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
	if (!p_state->get_allow_geometry_helper_nodes()) {
		opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY_NO_FALLBACK;
		opts.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_COMPENSATE_NO_FALLBACK;
	} else {
		opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
		opts.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_COMPENSATE;
	}
	opts.pivot_handling = UFBX_PIVOT_HANDLING_ADJUST_TO_PIVOT;
	opts.geometry_transform_helper_name.data = "GeometryTransformHelper";
	opts.geometry_transform_helper_name.length = SIZE_MAX;
	opts.scale_helper_name.data = "ScaleHelper";
	opts.scale_helper_name.length = SIZE_MAX;
	opts.node_depth_limit = 512;
	opts.target_camera_axes = ufbx_axes_right_handed_y_up;
	opts.target_light_axes = ufbx_axes_right_handed_y_up;
	opts.clean_skin_weights = true;
	if (p_state->discard_meshes_and_materials) {
		opts.ignore_geometry = true;
		opts.ignore_embedded = true;
	}
	opts.generate_missing_normals = true;

	ThreadPoolFBX thread_pool;
	thread_pool.pool = WorkerThreadPool::get_singleton();

	opts.thread_opts.pool.init_fn = &_thread_pool_init_fn;
	opts.thread_opts.pool.run_fn = &_thread_pool_run_fn;
	opts.thread_opts.pool.wait_fn = &_thread_pool_wait_fn;
	opts.thread_opts.pool.user = &thread_pool;
	opts.thread_opts.memory_limit = 64 * 1024 * 1024;

	ufbx_error error;
	ufbx_stream file_stream = {};
	file_stream.read_fn = &_file_access_read_fn;
	file_stream.skip_fn = &_file_access_skip_fn;
	file_stream.user = p_file.ptr();
	p_state->scene.reset(ufbx_load_stream(&file_stream, &opts, &error));

	if (!p_state->scene.get()) {
		char err_buf[512];
		ufbx_format_error(err_buf, sizeof(err_buf), &error);
		ERR_FAIL_V_MSG(ERR_PARSE_ERROR, err_buf);
	}

	const int max_warning_count = 10;
	int warning_count[UFBX_WARNING_TYPE_COUNT] = {};
	int ignored_warning_count = 0;
	for (const ufbx_warning &warning : p_state->scene->metadata.warnings) {
		if (warning_count[warning.type]++ < max_warning_count) {
			if (warning.count > 1) {
				WARN_PRINT(vformat("FBX: ufbx warning: %s (x%d)", _as_string(warning.description), (int)warning.count));
			} else {
				String element_name;
				if (warning.element_id != UFBX_NO_INDEX) {
					element_name = _find_element_name(p_state->scene->elements[warning.element_id]);
				}
				if (!element_name.is_empty()) {
					WARN_PRINT(vformat("FBX: ufbx warning in '%s': %s", element_name, _as_string(warning.description)));
				} else {
					WARN_PRINT(vformat("FBX: ufbx warning: %s", _as_string(warning.description)));
				}
			}
		} else {
			ignored_warning_count++;
		}
	}
	if (ignored_warning_count > 0) {
		WARN_PRINT(vformat("FBX: ignored %d further ufbx warnings", ignored_warning_count));
	}

	err = _parse_fbx_state(p_state, p_path);
	ERR_FAIL_COND_V(err != OK, err);

	return OK;
}

Node *FBXDocument::generate_scene(Ref<GLTFState> p_state, float p_bake_fps, bool p_trimming, bool p_remove_immutable_tracks) {
	Ref<FBXState> state = p_state;
	ERR_FAIL_COND_V(state.is_null(), nullptr);
	ERR_FAIL_INDEX_V(0, state->root_nodes.size(), nullptr);
	p_state->set_bake_fps(p_bake_fps);
	GLTFNodeIndex fbx_root = state->root_nodes.write[0];
	Node *fbx_root_node = state->get_scene_node(fbx_root);
	Node *root = fbx_root_node;
	if (root && root->get_owner() && root->get_owner() != root) {
		root = root->get_owner();
	}
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
	ERR_FAIL_NULL_V(root, nullptr);
	return root;
}

Error FBXDocument::append_from_buffer(PackedByteArray p_bytes, String p_base_path, Ref<GLTFState> p_state, uint32_t p_flags) {
	Ref<FBXState> state = p_state;
	ERR_FAIL_COND_V(state.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_NULL_V(p_bytes.ptr(), ERR_INVALID_DATA);
	Error err = FAILED;
	state->use_named_skin_binds = p_flags & FBX_IMPORT_USE_NAMED_SKIN_BINDS;
	state->discard_meshes_and_materials = p_flags & FBX_IMPORT_DISCARD_MESHES_AND_MATERIALS;

	Ref<FileAccessMemory> file_access;
	file_access.instantiate();
	file_access->open_custom(p_bytes.ptr(), p_bytes.size());
	state->base_path = p_base_path.get_base_dir();
	err = _parse(state, state->base_path, file_access);
	ERR_FAIL_COND_V(err != OK, err);
	// TODO: 202040118 // fire
	// for (Ref<GLTFDocumentExtension> ext : get_all_gltf_document_extensions()) {
	// 	ERR_CONTINUE(ext.is_null());
	// 	err = ext->import_post_parse(state);
	// 	ERR_FAIL_COND_V(err != OK, err);
	// }
	return OK;
}

Error FBXDocument::_parse_fbx_state(Ref<FBXState> p_state, const String &p_search_path) {
	Error err;

	// Abort parsing if the scene is not loaded.
	ERR_FAIL_NULL_V(p_state->scene.get(), ERR_PARSE_ERROR);

	/* PARSE SCENE */
	err = _parse_scenes(p_state);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* PARSE NODES */
	err = _parse_nodes(p_state);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	if (!p_state->discard_meshes_and_materials) {
		/* PARSE IMAGES */
		err = _parse_images(p_state, p_search_path);

		ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

		/* PARSE MATERIALS */
		err = _parse_materials(p_state);

		ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);
	}

	/* PARSE SKINS */
	err = _parse_skins(p_state);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* DETERMINE SKELETONS */
	err = SkinTool::_determine_skeletons(p_state->skins, p_state->nodes, p_state->skeletons, p_state->get_import_as_skeleton_bones() ? p_state->root_nodes : Vector<GLTFNodeIndex>());
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* CREATE SKELETONS */
	err = SkinTool::_create_skeletons(p_state->unique_names, p_state->skins, p_state->nodes, p_state->skeleton3d_to_fbx_skeleton, p_state->skeletons, p_state->scene_nodes);
	ERR_FAIL_COND_V(err != OK, ERR_PARSE_ERROR);

	/* CREATE SKINS */
	err = SkinTool::_create_skins(p_state->skins, p_state->nodes, p_state->use_named_skin_binds, p_state->unique_names);
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

	Node3D *root = memnew(Node3D);
	for (int32_t root_i = 0; root_i < p_state->root_nodes.size(); root_i++) {
		_generate_scene_node(p_state, p_state->root_nodes[root_i], root, root);
	}

	return OK;
}

Error FBXDocument::append_from_file(String p_path, Ref<GLTFState> p_state, uint32_t p_flags, String p_base_path) {
	Ref<FBXState> state = p_state;
	ERR_FAIL_COND_V(state.is_null(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_path.is_empty(), ERR_FILE_NOT_FOUND);
	if (p_state == Ref<FBXState>()) {
		p_state.instantiate();
	}
	state->filename = p_path.get_file().get_basename();
	state->use_named_skin_binds = p_flags & FBX_IMPORT_USE_NAMED_SKIN_BINDS;
	state->discard_meshes_and_materials = p_flags & FBX_IMPORT_DISCARD_MESHES_AND_MATERIALS;
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
	// TODO: 20240118 // fire
	// for (Ref<GLTFDocumentExtension> ext : document_extensions) {
	// 	ERR_CONTINUE(ext.is_null());
	// 	err = ext->import_post_parse(p_state);
	// 	ERR_FAIL_COND_V(err != OK, err);
	// }
	return OK;
}

void FBXDocument::_process_uv_set(PackedVector2Array &uv_array) {
	int uv_size = uv_array.size();
	for (int uv_i = 0; uv_i < uv_size; uv_i++) {
		Vector2 &uv = uv_array.write[uv_i];
		uv.y = 1.0 - uv.y;
	}
}

void FBXDocument::_zero_unused_elements(Vector<float> &cur_custom, int start, int end, int num_channels) {
	for (int32_t uv_i = start; uv_i < end; uv_i++) {
		int index = uv_i * num_channels;
		for (int channel = 0; channel < num_channels; channel++) {
			cur_custom.write[index + channel] = 0;
		}
	}
}

Error FBXDocument::_parse_lights(Ref<FBXState> p_state) {
	const ufbx_scene *fbx_scene = p_state->scene.get();
	for (size_t i = 0; i < fbx_scene->lights.count; i++) {
		const ufbx_light *fbx_light = fbx_scene->lights.data[i];
		Ref<GLTFLight> light;
		light.instantiate();
		light->set_name(_as_string(fbx_light->name));
		light->set_color(Color(fbx_light->color.x, fbx_light->color.y, fbx_light->color.z));
		light->set_intensity(fbx_light->intensity);
		switch (fbx_light->type) {
			case UFBX_LIGHT_POINT:
				light->set_light_type("point");
				break;
			case UFBX_LIGHT_DIRECTIONAL:
				light->set_light_type("directional");
				break;
			case UFBX_LIGHT_SPOT:
				light->set_light_type("spot");
				break;
			case UFBX_LIGHT_AREA:
				light->set_light_type("area");
				break;
			case UFBX_LIGHT_VOLUME:
				light->set_light_type("volume");
				break;
			default:
				light->set_light_type("unknown");
				break;
		}

		Dictionary additional_data;
		additional_data["shadow"] = fbx_light->cast_shadows;
		if (fbx_light->decay == UFBX_LIGHT_DECAY_NONE) {
			additional_data["decay"] = "none";

		} else if (fbx_light->decay == UFBX_LIGHT_DECAY_LINEAR) {
			additional_data["decay"] = "linear";

		} else if (fbx_light->decay == UFBX_LIGHT_DECAY_QUADRATIC) {
			additional_data["decay"] = "quadratic";

		} else if (fbx_light->decay == UFBX_LIGHT_DECAY_CUBIC) {
			additional_data["decay"] = "cubic";
		}

		if (fbx_light->area_shape == UFBX_LIGHT_AREA_SHAPE_RECTANGLE) {
			additional_data["areaShape"] = "rectangle";
		} else if (fbx_light->area_shape == UFBX_LIGHT_AREA_SHAPE_SPHERE) {
			additional_data["areaShape"] = "sphere";
		}

		light->set_inner_cone_angle(fbx_light->inner_angle);
		light->set_outer_cone_angle(fbx_light->outer_angle);

		additional_data["castLight"] = fbx_light->cast_light;
		additional_data["castShadows"] = fbx_light->cast_shadows;
		light->set_additional_data("GODOT_fbx_light", additional_data);
		p_state->lights.push_back(light);
	}
	print_verbose("FBX: Total lights: " + itos(p_state->lights.size()));
	return OK;
}

String FBXDocument::_get_texture_path(const String &p_base_dir, const String &p_source_file_path) const {
	// Check if the original path exists first.
	if (FileAccess::exists(p_source_file_path)) {
		return p_source_file_path.strip_edges();
	}
	const String tex_file_name = p_source_file_path.get_file();
	const Vector<String> subdirs = {
		"", "textures/", "Textures/", "images/",
		"Images/", "materials/", "Materials/",
		"maps/", "Maps/", "tex/", "Tex/"
	};
	String base_dir = p_base_dir;
	const String source_file_name = tex_file_name;
	while (!base_dir.is_empty()) {
		String old_base_dir = base_dir;
		for (int i = 0; i < subdirs.size(); ++i) {
			String full_path = base_dir.path_join(subdirs[i] + source_file_name);
			if (FileAccess::exists(full_path)) {
				return full_path.strip_edges();
			}
		}
		base_dir = base_dir.get_base_dir();
		if (base_dir == old_base_dir) {
			break;
		}
	}
	return String();
}

Error FBXDocument::_parse_skins(Ref<FBXState> p_state) {
	const ufbx_scene *fbx_scene = p_state->scene.get();
	HashMap<GLTFNodeIndex, bool> joint_mapping;

	for (const ufbx_skin_deformer *fbx_skin : fbx_scene->skin_deformers) {
		if (fbx_skin->clusters.count == 0 || fbx_skin->weights.count == 0) {
			p_state->skin_indices.push_back(-1);
			continue;
		}

		Ref<GLTFSkin> skin;
		skin.instantiate();

		skin->inverse_binds.resize(fbx_skin->clusters.count);
		for (int skin_i = 0; skin_i < static_cast<int>(fbx_skin->clusters.count); skin_i++) {
			const ufbx_skin_cluster *fbx_cluster = fbx_skin->clusters[skin_i];
			skin->inverse_binds.write[skin_i] = FBXDocument::_as_xform(fbx_cluster->geometry_to_bone);
			const GLTFNodeIndex node = fbx_cluster->bone_node->typed_id;

			skin->joints.push_back(node);
			skin->joints_original.push_back(node);
			p_state->nodes.write[node]->joint = true;
		}

		if (fbx_skin->name.length > 0) {
			skin->set_name(FBXDocument::_as_string(fbx_skin->name));
		} else {
			skin->set_name(vformat("skin_%s", itos(fbx_skin->typed_id)));
		}
		p_state->skin_indices.push_back(p_state->skins.size());
		p_state->skins.push_back(skin);
	}

	for (const ufbx_bone *fbx_bone : fbx_scene->bones) {
		for (const ufbx_node *fbx_node : fbx_bone->instances) {
			const GLTFNodeIndex node = fbx_node->typed_id;
			if (!p_state->nodes.write[node]->joint) {
				p_state->nodes.write[node]->joint = true;

				if (!(fbx_node->parent && fbx_node->parent->attrib_type == UFBX_ELEMENT_BONE)) {
					Ref<GLTFSkin> skin;
					skin.instantiate();
					skin->joints.push_back(node);
					skin->joints_original.push_back(node);
					skin->set_name(vformat("skin_%s", itos(p_state->skins.size())));
					p_state->skin_indices.push_back(p_state->skins.size());
					p_state->skins.push_back(skin);
				}
			}
		}
	}
	p_state->original_skin_indices = p_state->skin_indices.duplicate();
	Error err = SkinTool::_asset_parse_skins(
			p_state->original_skin_indices,
			p_state->skins.duplicate(),
			p_state->nodes.duplicate(),
			p_state->skin_indices,
			p_state->skins,
			joint_mapping);
	if (err != OK) {
		return err;
	}
	for (int i = 0; i < p_state->skins.size(); ++i) {
		Ref<GLTFSkin> skin = p_state->skins.write[i];
		ERR_FAIL_COND_V(skin.is_null(), ERR_PARSE_ERROR);
		// Expand and verify the skin
		ERR_FAIL_COND_V(SkinTool::_expand_skin(p_state->nodes, skin), ERR_PARSE_ERROR);
		ERR_FAIL_COND_V(SkinTool::_verify_skin(p_state->nodes, skin), ERR_PARSE_ERROR);
	}

	print_verbose("FBX: Total skins: " + itos(p_state->skins.size()));

	for (HashMap<GLTFNodeIndex, bool>::Iterator it = joint_mapping.begin(); it != joint_mapping.end(); ++it) {
		GLTFNodeIndex node_index = it->key;
		bool is_joint = it->value;
		if (is_joint) {
			if (p_state->nodes.size() > node_index) {
				p_state->nodes.write[node_index]->joint = true;
			}
		}
	}

	return OK;
}

PackedByteArray FBXDocument::generate_buffer(Ref<GLTFState> p_state) {
	return PackedByteArray();
}

Error FBXDocument::write_to_filesystem(Ref<GLTFState> p_state, const String &p_path) {
	return ERR_UNAVAILABLE;
}

Error FBXDocument::append_from_scene(Node *p_node, Ref<GLTFState> p_state, uint32_t p_flags) {
	return ERR_UNAVAILABLE;
}

Vector3 FBXDocument::_as_vec3(const ufbx_vec3 &p_vector) {
	return Vector3(real_t(p_vector.x), real_t(p_vector.y), real_t(p_vector.z));
}

String FBXDocument::_as_string(const ufbx_string &p_string) {
	return String::utf8(p_string.data, (int)p_string.length);
}

Transform3D FBXDocument::_as_xform(const ufbx_matrix &p_mat) {
	Transform3D xform;
	xform.basis.set_column(Vector3::AXIS_X, _as_vec3(p_mat.cols[0]));
	xform.basis.set_column(Vector3::AXIS_Y, _as_vec3(p_mat.cols[1]));
	xform.basis.set_column(Vector3::AXIS_Z, _as_vec3(p_mat.cols[2]));
	xform.set_origin(_as_vec3(p_mat.cols[3]));
	return xform;
}
