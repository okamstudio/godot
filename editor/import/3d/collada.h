/**************************************************************************/
/*  collada.h                                                             */
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

#ifndef COLLADA_H
#define COLLADA_H

#include "core/io/xml_parser.h"

class Collada {
public:
	enum ImportFlags {
		IMPORT_FLAG_SCENE = 1,
		IMPORT_FLAG_ANIMATION = 2
	};

	struct Image {
		String path;
	};

	struct Material {
		String name;
		String instance_effect;
	};

	struct Effect {
		String name;
		HashMap<String, Variant> params;

		struct Channel {
			int uv_idx = 0;
			String texture;
			Color color;
			Channel() {}
		};

		Channel diffuse, specular, emission, bump;
		float shininess = 40;
		bool found_double_sided = false;
		bool double_sided = true;
		bool unshaded = false;

		String get_texture_path(const String &p_source, Collada &p_state) const;

		Effect() {
			diffuse.color = Color(1, 1, 1, 1);
		}
	};

	struct CameraData {
		enum Mode {
			MODE_PERSPECTIVE,
			MODE_ORTHOGONAL
		};

		Mode mode = MODE_PERSPECTIVE;

		union {
			struct {
				float x_fov = 0;
				float y_fov = 0;
			} perspective;
			struct {
				float x_mag = 0;
				float y_mag = 0;
			} orthogonal;
		};

		float aspect = 1;
		float z_near = 0.05;
		float z_far = 4000;

		CameraData() {}
	};

	struct LightData {
		enum Mode {
			MODE_AMBIENT,
			MODE_DIRECTIONAL,
			MODE_OMNI,
			MODE_SPOT
		};

		Mode mode = MODE_AMBIENT;

		Color color = Color(1, 1, 1, 1);

		float constant_att = 0;
		float linear_att = 0;
		float quad_att = 0;

		float spot_angle = 45;
		float spot_exp = 1;

		LightData() {}
	};

	struct MeshData {
		String name;
		struct Source {
			Vector<float> array;
			int stride = 0;
		};

		HashMap<String, Source> sources;

		struct Vertices {
			HashMap<String, String> sources;
		};

		HashMap<String, Vertices> vertices;

		struct Primitives {
			struct SourceRef {
				String source;
				int offset = 0;
			};

			String material;
			HashMap<String, SourceRef> sources;
			Vector<float> polygons;
			Vector<float> indices;
			int count = 0;
			int vertex_size = 0;
		};

		Vector<Primitives> primitives;

		bool found_double_sided = false;
		bool double_sided = true;

		MeshData() {}
	};

	struct CurveData {
		String name;
		bool closed = false;

		struct Source {
			Vector<String> sarray;
			Vector<float> array;
			int stride = 0;
		};

		HashMap<String, Source> sources;

		HashMap<String, String> control_vertices;

		CurveData() {}
	};

	struct SkinControllerData {
		String base;
		bool use_idrefs = false;

		Transform3D bind_shape;

		struct Source {
			Vector<String> sarray; //maybe for names
			Vector<float> array;
			int stride = 1;
			Source() {}
		};

		HashMap<String, Source> sources;

		struct Joints {
			HashMap<String, String> sources;
		} joints;

		struct Weights {
			struct SourceRef {
				String source;
				int offset = 0;
			};

			String material;
			HashMap<String, SourceRef> sources;
			Vector<float> sets;
			Vector<float> indices;
			int count = 0;
		} weights;

		HashMap<String, Transform3D> bone_rest_map;

		SkinControllerData() {}
	};

	struct MorphControllerData {
		String mesh;
		String mode;

		struct Source {
			int stride = 1;
			Vector<String> sarray; //maybe for names
			Vector<float> array;
			Source() {}
		};

		HashMap<String, Source> sources;

		HashMap<String, String> targets;
		MorphControllerData() {}
	};

	struct Vertex {
		int idx = 0;
		Vector3 vertex;
		Vector3 normal;
		Vector3 uv;
		Vector3 uv2;
		Plane tangent;
		Color color;
		int uid = 0;
		struct Weight {
			int bone_idx = 0;
			float weight = 0;
			bool operator<(const Weight w) const { return weight > w.weight; } //heaviest first
		};

		Vector<Weight> weights;

		void fix_weights() {
			weights.sort();
			if (weights.size() > 4) {
				//cap to 4 and make weights add up 1
				weights.resize(4);
				float total = 0;
				for (int i = 0; i < 4; i++) {
					total += weights[i].weight;
				}
				if (total) {
					for (int i = 0; i < 4; i++) {
						weights.write[i].weight /= total;
					}
				}
			}
		}

		void fix_unit_scale(const Collada &p_state);

		bool operator<(const Vertex &p_vert) const {
			if (uid == p_vert.uid) {
				if (vertex == p_vert.vertex) {
					if (normal == p_vert.normal) {
						if (uv == p_vert.uv) {
							if (uv2 == p_vert.uv2) {
								if (!weights.is_empty() || !p_vert.weights.is_empty()) {
									if (weights.size() == p_vert.weights.size()) {
										for (int i = 0; i < weights.size(); i++) {
											if (weights[i].bone_idx != p_vert.weights[i].bone_idx) {
												return weights[i].bone_idx < p_vert.weights[i].bone_idx;
											}

											if (weights[i].weight != p_vert.weights[i].weight) {
												return weights[i].weight < p_vert.weights[i].weight;
											}
										}
									} else {
										return weights.size() < p_vert.weights.size();
									}
								}

								return (color < p_vert.color);
							} else {
								return (uv2 < p_vert.uv2);
							}
						} else {
							return (uv < p_vert.uv);
						}
					} else {
						return (normal < p_vert.normal);
					}
				} else {
					return vertex < p_vert.vertex;
				}
			} else {
				return uid < p_vert.uid;
			}
		}

		Vertex() {}
	};

	struct Node {
		enum Type {
			TYPE_NODE,
			TYPE_JOINT,
			TYPE_SKELETON, //this bone is not collada, it's added afterwards as optimization
			TYPE_LIGHT,
			TYPE_CAMERA,
			TYPE_GEOMETRY
		};

		struct XForm {
			enum Op {
				OP_ROTATE,
				OP_SCALE,
				OP_TRANSLATE,
				OP_MATRIX,
				OP_VISIBILITY
			};

			String id;
			Op op = OP_ROTATE;
			Vector<float> data;
		};

		Type type = TYPE_NODE;

		String name;
		String id;
		String empty_draw_type;
		bool noname = false;
		Vector<XForm> xform_list;
		Transform3D default_transform;
		Transform3D post_transform;
		Vector<Node *> children;

		Node *parent = nullptr;

		Transform3D compute_transform(const Collada &p_state) const;
		Transform3D get_global_transform() const;
		Transform3D get_transform() const;

		bool ignore_anim = false;

		Node() {}
		virtual ~Node() {
			for (int i = 0; i < children.size(); i++) {
				memdelete(children[i]);
			}
		}
	};

	struct NodeSkeleton : public Node {
		NodeSkeleton() { type = TYPE_SKELETON; }
	};

	struct NodeJoint : public Node {
		NodeSkeleton *owner = nullptr;
		String sid;
		NodeJoint() {
			type = TYPE_JOINT;
		}
	};

	struct NodeGeometry : public Node {
		bool controller = false;
		String source;

		struct Material {
			String target;
		};

		HashMap<String, Material> material_map;
		Vector<String> skeletons;

		NodeGeometry() { type = TYPE_GEOMETRY; }
	};

	struct NodeCamera : public Node {
		String camera;

		NodeCamera() { type = TYPE_CAMERA; }
	};

	struct NodeLight : public Node {
		String light;

		NodeLight() { type = TYPE_LIGHT; }
	};

	struct VisualScene {
		String name;
		Vector<Node *> root_nodes;

		~VisualScene() {
			for (int i = 0; i < root_nodes.size(); i++) {
				memdelete(root_nodes[i]);
			}
		}
	};

	struct AnimationClip {
		String name;
		float begin = 0;
		float end = 1;
		Vector<String> tracks;

		AnimationClip() {}
	};

	struct AnimationTrack {
		String id;
		String target;
		String param;
		String component;
		bool property = false;

		enum InterpolationType {
			INTERP_LINEAR,
			INTERP_BEZIER
		};

		struct Key {
			enum Type {
				TYPE_FLOAT,
				TYPE_MATRIX
			};

			float time = 0;
			Vector<float> data;
			Point2 in_tangent;
			Point2 out_tangent;
			InterpolationType interp_type = INTERP_LINEAR;

			Key() {}
		};

		Vector<float> get_value_at_time(float p_time) const;

		Vector<Key> keys;

		AnimationTrack() {}
	};

	/****************/
	/* IMPORT STATE */
	/****************/

	struct State {
		int import_flags = 0;

		float unit_scale = 1.0;
		Vector3::Axis up_axis = Vector3::AXIS_Y;
		bool z_up = false;

		struct Version {
			int major = 0, minor = 0, rev = 0;

			bool operator<(const Version &p_ver) const { return (major == p_ver.major) ? ((minor == p_ver.minor) ? (rev < p_ver.rev) : minor < p_ver.minor) : major < p_ver.major; }
			Version(int p_major = 0, int p_minor = 0, int p_rev = 0) {
				major = p_major;
				minor = p_minor;
				rev = p_rev;
			}
		} version;

		HashMap<String, CameraData> camera_data_map;
		HashMap<String, MeshData> mesh_data_map;
		HashMap<String, LightData> light_data_map;
		HashMap<String, CurveData> curve_data_map;

		HashMap<String, String> mesh_name_map;
		HashMap<String, String> morph_name_map;
		HashMap<String, String> morph_ownership_map;
		HashMap<String, SkinControllerData> skin_controller_data_map;
		HashMap<String, MorphControllerData> morph_controller_data_map;

		HashMap<String, Image> image_map;
		HashMap<String, Material> material_map;
		HashMap<String, Effect> effect_map;

		HashMap<String, VisualScene> visual_scene_map;
		HashMap<String, Node *> scene_map;
		HashSet<String> idref_joints;
		HashMap<String, String> sid_to_node_map;
		//RBMap<String,NodeJoint*> bone_map;

		HashMap<String, Transform3D> bone_rest_map;

		String local_path;
		String root_visual_scene;
		String root_physics_scene;

		Vector<AnimationClip> animation_clips;
		Vector<AnimationTrack> animation_tracks;
		HashMap<String, Vector<int>> referenced_tracks;
		HashMap<String, Vector<int>> by_id_tracks;

		float animation_length = 0;

		State() {}
	} state;

	Error load(const String &p_path, int p_flags = 0);

	Collada();

	Transform3D fix_transform(const Transform3D &p_transform);

	Transform3D get_root_transform() const;

	int get_uv_channel(const String &p_name);

private: // private stuff
	HashMap<String, int> channel_map;

	void _parse_asset(XMLParser &p_parser);
	void _parse_image(XMLParser &p_parser);
	void _parse_material(XMLParser &p_parser);
	void _parse_effect_material(XMLParser &p_parser, Effect &p_effect, String &p_id);
	void _parse_effect(XMLParser &p_parser);
	void _parse_camera(XMLParser &p_parser);
	void _parse_light(XMLParser &p_parser);
	void _parse_animation_clip(XMLParser &p_parser);

	void _parse_mesh_geometry(XMLParser &p_parser, const String &p_id, const String &p_name);
	void _parse_curve_geometry(XMLParser &p_parser, const String &p_id, const String &p_name);

	void _parse_skin_controller(XMLParser &p_parser, const String &p_id);
	void _parse_morph_controller(XMLParser &p_parser, const String &p_id);
	void _parse_controller(XMLParser &p_parser);

	Node *_parse_visual_instance_geometry(XMLParser &p_parser);
	Node *_parse_visual_instance_camera(XMLParser &p_parser);
	Node *_parse_visual_instance_light(XMLParser &p_parser);

	Node *_parse_visual_node_instance_data(XMLParser &p_parser);
	Node *_parse_visual_scene_node(XMLParser &p_parser);
	void _parse_visual_scene(XMLParser &p_parser);

	void _parse_animation(XMLParser &p_parser);
	void _parse_scene(XMLParser &p_parser);
	void _parse_library(XMLParser &p_parser);

	Variant _parse_param(XMLParser &p_parser);
	Vector<float> _read_float_array(XMLParser &p_parser);
	Vector<String> _read_string_array(XMLParser &p_parser);
	Transform3D _read_transform(XMLParser &p_parser);
	String _read_empty_draw_type(XMLParser &p_parser);

	void _joint_set_owner(Collada::Node *p_node, NodeSkeleton *p_owner);
	void _create_skeletons(Collada::Node **p_node, NodeSkeleton *p_skeleton = nullptr);
	void _find_morph_nodes(VisualScene *p_vscene, Node *p_node);
	bool _remove_node(Node *p_parent, Node *p_node);
	void _remove_node(VisualScene *p_vscene, Node *p_node);
	void _merge_skeletons2(VisualScene *p_vscene);
	void _merge_skeletons(VisualScene *p_vscene, Node *p_node);
	bool _optimize_skeletons(VisualScene *p_vscene, Node *p_node);

	bool _move_geometry_to_skeletons(VisualScene *p_vscene, Node *p_node, List<Node *> *p_mgeom);

	void _optimize();
};

#endif // COLLADA_H
