/**************************************************************************/
/*  scene_import_settings.cpp                                             */
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

#include "scene_import_settings.h"

#include "core/config/project_settings.h"
#include "editor/editor_file_system.h"
#include "editor/editor_inspector.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/plugins/skeleton_3d_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/subviewport_container.h"
#include "scene/resources/3d/importer_mesh.h"
#include "scene/resources/surface_tool.h"

class SceneImportSettingsData : public Object {
	GDCLASS(SceneImportSettingsData, Object)
	friend class SceneImportSettingsDialog;
	HashMap<StringName, Variant> *settings = nullptr;
	HashMap<StringName, Variant> current;
	HashMap<StringName, Variant> defaults;
	List<ResourceImporter::ImportOption> options;
	Vector<String> animation_list;

	bool hide_options = false;
	String path;

	ResourceImporterScene::InternalImportCategory category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MAX;

	bool _set(const StringName &p_name, const Variant &p_value) {
		if (settings) {
			if (defaults.has(p_name) && defaults[p_name] == p_value) {
				settings->erase(p_name);
			} else {
				(*settings)[p_name] = p_value;
			}

			current[p_name] = p_value;

			// SceneImportSettings must decide if a new collider should be generated or not.
			if (category == ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MESH_3D_NODE) {
				SceneImportSettingsDialog::get_singleton()->request_generate_collider();
			}

			if (SceneImportSettingsDialog::get_singleton()->is_editing_animation()) {
				if (category == ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MAX) {
					if (ResourceImporterScene::get_animation_singleton()->get_option_visibility(path, p_name, current)) {
						SceneImportSettingsDialog::get_singleton()->update_view();
					}
				} else {
					if (ResourceImporterScene::get_animation_singleton()->get_internal_option_update_view_required(category, p_name, current)) {
						SceneImportSettingsDialog::get_singleton()->update_view();
					}
				}
			} else {
				if (category == ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MAX) {
					if (ResourceImporterScene::get_scene_singleton()->get_option_visibility(path, p_name, current)) {
						SceneImportSettingsDialog::get_singleton()->update_view();
					}
				} else {
					if (ResourceImporterScene::get_scene_singleton()->get_internal_option_update_view_required(category, p_name, current)) {
						SceneImportSettingsDialog::get_singleton()->update_view();
					}
				}
			}

			return true;
		}
		return false;
	}

	bool _get(const StringName &p_name, Variant &r_ret) const {
		if (settings) {
			if (settings->has(p_name)) {
				r_ret = (*settings)[p_name];
				return true;
			}
		}
		if (defaults.has(p_name)) {
			r_ret = defaults[p_name];
			return true;
		}
		return false;
	}

	void handle_special_properties(PropertyInfo &r_option) const {
		ERR_FAIL_NULL(settings);
		if (r_option.name == "rest_pose/load_pose") {
			if (!settings->has("rest_pose/load_pose") || int((*settings)["rest_pose/load_pose"]) != 2) {
				if (settings->has("rest_pose/external_animation_library")) {
					(*settings)["rest_pose/external_animation_library"] = Variant();
				}
			}
		}
		if (r_option.name == "rest_pose/selected_animation") {
			if (!settings->has("rest_pose/load_pose")) {
				return;
			}
			String hint_string;

			switch (int((*settings)["rest_pose/load_pose"])) {
				case 1: {
					hint_string = String(",").join(animation_list);
					if (animation_list.size() == 1) {
						(*settings)["rest_pose/selected_animation"] = animation_list[0];
					}
				} break;
				case 2: {
					Object *res = nullptr;
					if (settings->has("rest_pose/external_animation_library")) {
						res = (*settings)["rest_pose/external_animation_library"];
					}
					Ref<Animation> anim(res);
					Ref<AnimationLibrary> library(res);
					if (anim.is_valid()) {
						hint_string = anim->get_name();
					}
					if (library.is_valid()) {
						List<StringName> anim_names;
						library->get_animation_list(&anim_names);
						if (anim_names.size() == 1) {
							(*settings)["rest_pose/selected_animation"] = String(anim_names.front()->get());
						}
						for (StringName anim_name : anim_names) {
							hint_string += "," + anim_name; // Include preceding, as a catch-all.
						}
					}
				} break;
				default:
					break;
			}
			r_option.hint = PROPERTY_HINT_ENUM;
			r_option.hint_string = hint_string;
		}
	}

	void _get_property_list(List<PropertyInfo> *r_list) const {
		if (hide_options) {
			return;
		}
		for (const ResourceImporter::ImportOption &E : options) {
			PropertyInfo option = E.option;
			if (SceneImportSettingsDialog::get_singleton()->is_editing_animation()) {
				if (category == ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MAX) {
					if (ResourceImporterScene::get_animation_singleton()->get_option_visibility(path, E.option.name, current)) {
						handle_special_properties(option);
						r_list->push_back(option);
					}
				} else {
					if (ResourceImporterScene::get_animation_singleton()->get_internal_option_visibility(category, E.option.name, current)) {
						handle_special_properties(option);
						r_list->push_back(option);
					}
				}
			} else {
				if (category == ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MAX) {
					if (ResourceImporterScene::get_scene_singleton()->get_option_visibility(path, E.option.name, current)) {
						handle_special_properties(option);
						r_list->push_back(option);
					}
				} else {
					if (ResourceImporterScene::get_scene_singleton()->get_internal_option_visibility(category, E.option.name, current)) {
						handle_special_properties(option);
						r_list->push_back(option);
					}
				}
			}
		}
	}
};

void SceneImportSettingsDialog::_fill_material(Tree *p_tree, const Ref<Material> &p_material, TreeItem *p_parent) {
	String import_id;
	bool has_import_id = false;

	if (p_material->has_meta("import_id")) {
		import_id = p_material->get_meta("import_id");
		has_import_id = true;
	} else if (!p_material->get_name().is_empty()) {
		import_id = p_material->get_name();
		has_import_id = true;
	} else if (unnamed_material_name_map.has(p_material)) {
		import_id = unnamed_material_name_map[p_material];
	} else {
		import_id = "@MATERIAL:" + itos(material_map.size());
		unnamed_material_name_map[p_material] = import_id;
	}

	bool created = false;
	if (!material_map.has(import_id)) {
		MaterialData md;
		created = true;
		md.has_import_id = has_import_id;
		md.material = p_material;

		_load_default_subresource_settings(md.settings, "materials", import_id, ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MATERIAL);

		material_map[import_id] = md;
	}

	MaterialData &material_data = material_map[import_id];
	ERR_FAIL_COND(p_material != material_data.material);

	Ref<Texture2D> icon = get_editor_theme_icon(SNAME("StandardMaterial3D"));

	TreeItem *item = p_tree->create_item(p_parent);
	if (p_material->get_name().is_empty()) {
		item->set_text(0, TTR("<Unnamed Material>"));
	} else {
		item->set_text(0, p_material->get_name());
	}
	item->set_icon(0, icon);

	item->set_meta("type", "Material");
	item->set_meta("import_id", import_id);
	item->set_tooltip_text(0, vformat(TTR("Import ID: %s"), import_id));
	item->set_selectable(0, true);

	if (p_tree == scene_tree) {
		material_data.scene_node = item;
	} else if (p_tree == mesh_tree) {
		material_data.mesh_node = item;
	} else {
		material_data.material_node = item;
	}

	if (created) {
		_fill_material(material_tree, p_material, material_tree->get_root());
	}
}

void SceneImportSettingsDialog::_fill_mesh(Tree *p_tree, const Ref<Mesh> &p_mesh, TreeItem *p_parent) {
	String import_id;

	bool has_import_id = false;
	if (p_mesh->has_meta("import_id")) {
		import_id = p_mesh->get_meta("import_id");
		has_import_id = true;
	} else if (!p_mesh->get_name().is_empty()) {
		import_id = p_mesh->get_name();
		has_import_id = true;
	} else {
		import_id = "@MESH:" + itos(mesh_set.size());
	}

	if (!mesh_map.has(import_id)) {
		MeshData md;
		md.has_import_id = has_import_id;
		md.mesh = p_mesh;

		_load_default_subresource_settings(md.settings, "meshes", import_id, ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MESH);

		mesh_map[import_id] = md;
	}

	MeshData &mesh_data = mesh_map[import_id];

	Ref<Texture2D> icon = get_editor_theme_icon(SNAME("MeshItem"));

	TreeItem *item = p_tree->create_item(p_parent);
	item->set_text(0, p_mesh->get_name());
	item->set_icon(0, icon);

	bool created = false;
	if (!mesh_set.has(p_mesh)) {
		mesh_set.insert(p_mesh);
		created = true;
	}

	item->set_meta("type", "Mesh");
	item->set_meta("import_id", import_id);
	item->set_tooltip_text(0, vformat(TTR("Import ID: %s"), import_id));

	item->set_selectable(0, true);

	if (p_tree == scene_tree) {
		mesh_data.scene_node = item;
	} else {
		mesh_data.mesh_node = item;
	}

	item->set_collapsed(true);

	for (int i = 0; i < p_mesh->get_surface_count(); i++) {
		Ref<Material> mat = p_mesh->surface_get_material(i);
		if (mat.is_valid()) {
			_fill_material(p_tree, mat, item);
		}
	}

	if (created) {
		_fill_mesh(mesh_tree, p_mesh, mesh_tree->get_root());
	}
}

void SceneImportSettingsDialog::_fill_animation(Tree *p_tree, const Ref<Animation> &p_anim, const String &p_name, TreeItem *p_parent) {
	if (!animation_map.has(p_name)) {
		AnimationData ad;
		ad.animation = p_anim;

		_load_default_subresource_settings(ad.settings, "animations", p_name, ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_ANIMATION);

		animation_map[p_name] = ad;
	}

	AnimationData &animation_data = animation_map[p_name];

	Ref<Texture2D> icon = get_editor_theme_icon(SNAME("Animation"));

	TreeItem *item = p_tree->create_item(p_parent);
	item->set_text(0, p_name);
	item->set_icon(0, icon);

	item->set_meta("type", "Animation");
	item->set_meta("import_id", p_name);

	item->set_selectable(0, true);

	animation_data.scene_node = item;
}

void SceneImportSettingsDialog::_fill_scene(Node *p_node, TreeItem *p_parent_item) {
	String import_id;

	if (p_node->has_meta("import_id")) {
		import_id = p_node->get_meta("import_id");
	} else {
		import_id = "PATH:" + String(scene->get_path_to(p_node));
		p_node->set_meta("import_id", import_id);
	}

	ImporterMeshInstance3D *src_mesh_node = Object::cast_to<ImporterMeshInstance3D>(p_node);

	if (src_mesh_node) {
		MeshInstance3D *mesh_node = memnew(MeshInstance3D);
		mesh_node->set_name(src_mesh_node->get_name());
		mesh_node->set_transform(src_mesh_node->get_transform());
		mesh_node->set_skin(src_mesh_node->get_skin());
		mesh_node->set_skeleton_path(src_mesh_node->get_skeleton_path());
		mesh_node->set_visible(src_mesh_node->is_visible());
		if (src_mesh_node->get_mesh().is_valid()) {
			Ref<ImporterMesh> editor_mesh = src_mesh_node->get_mesh();
			mesh_node->set_mesh(editor_mesh->get_mesh());
		}
		// Replace the original mesh node in the scene tree with the new one.
		if (unlikely(p_node == scene)) {
			scene = mesh_node;
		}
		p_node->replace_by(mesh_node);
		memdelete(p_node);
		p_node = mesh_node;
	}

	String type = p_node->get_class();

	if (!has_theme_icon(type, EditorStringName(EditorIcons))) {
		type = "Node3D";
	}

	Ref<Texture2D> icon = get_editor_theme_icon(type);

	TreeItem *item = scene_tree->create_item(p_parent_item);
	item->set_text(0, p_node->get_name());

	if (p_node == scene) {
		icon = get_editor_theme_icon(SNAME("PackedScene"));
		item->set_text(0, TTR("Scene"));
	}

	item->set_icon(0, icon);

	item->set_meta("type", "Node");
	item->set_meta("class", type);
	item->set_meta("import_id", import_id);
	item->set_tooltip_text(0, vformat(TTR("Type: %s\nImport ID: %s"), type, import_id));

	item->set_selectable(0, true);

	if (!node_map.has(import_id)) {
		NodeData nd;

		if (p_node != scene) {
			ResourceImporterScene::InternalImportCategory category;
			if (src_mesh_node) {
				category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MESH_3D_NODE;
			} else if (Object::cast_to<AnimationPlayer>(p_node)) {
				category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_ANIMATION_NODE;

				animation_player = Object::cast_to<AnimationPlayer>(p_node);
				animation_player->connect(SceneStringName(animation_finished), callable_mp(this, &SceneImportSettingsDialog::_animation_finished));
			} else if (Object::cast_to<Skeleton3D>(p_node)) {
				category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_SKELETON_3D_NODE;
				Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(p_node);
				skeleton->connect(SceneStringName(tree_entered), callable_mp(this, &SceneImportSettingsDialog::_skeleton_tree_entered).bind(skeleton));
				skeletons.push_back(skeleton);
			} else {
				category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_NODE;
			}

			_load_default_subresource_settings(nd.settings, "nodes", import_id, category);
		}

		node_map[import_id] = nd;
	}
	NodeData &node_data = node_map[import_id];

	node_data.node = p_node;
	node_data.scene_node = item;

	AnimationPlayer *anim_node = Object::cast_to<AnimationPlayer>(p_node);
	if (anim_node) {
		Vector<String> animation_list;
		List<StringName> animations;
		anim_node->get_animation_list(&animations);
		for (const StringName &E : animations) {
			_fill_animation(scene_tree, anim_node->get_animation(E), E, item);
			animation_list.append(E);
		}
		if (scene_import_settings_data != nullptr) {
			scene_import_settings_data->animation_list = animation_list;
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_fill_scene(p_node->get_child(i), item);
	}
	MeshInstance3D *mesh_node = Object::cast_to<MeshInstance3D>(p_node);
	if (mesh_node && mesh_node->get_mesh().is_valid()) {
		if (!editing_animation) {
			_fill_mesh(scene_tree, mesh_node->get_mesh(), item);
		}

		// Add the collider view.
		MeshInstance3D *collider_view = memnew(MeshInstance3D);
		collider_view->set_name("collider_view");
		collider_view->set_visible(false);
		mesh_node->add_child(collider_view, true);
		collider_view->set_owner(mesh_node);

		Transform3D accum_xform;
		Node3D *base = mesh_node;
		while (base) {
			accum_xform = base->get_transform() * accum_xform;
			base = Object::cast_to<Node3D>(base->get_parent());
		}

		AABB aabb = accum_xform.xform(mesh_node->get_mesh()->get_aabb());

		if (first_aabb) {
			contents_aabb = aabb;
			first_aabb = false;
		} else {
			contents_aabb.merge_with(aabb);
		}
	}

	Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(p_node);
	if (skeleton) {
		Ref<ArrayMesh> bones_mesh = Skeleton3DGizmoPlugin::get_bones_mesh(skeleton, -1, true);

		bones_mesh_preview->set_mesh(bones_mesh);

		Transform3D accum_xform;
		Node3D *base = skeleton;
		while (base) {
			accum_xform = base->get_transform() * accum_xform;
			base = Object::cast_to<Node3D>(base->get_parent());
		}

		bones_mesh_preview->set_transform(accum_xform * skeleton->get_transform());

		AABB aabb = accum_xform.xform(bones_mesh->get_aabb());

		if (first_aabb) {
			contents_aabb = aabb;
			first_aabb = false;
		} else {
			contents_aabb.merge_with(aabb);
		}
	}
}

void SceneImportSettingsDialog::_update_scene() {
	scene_tree->clear();
	material_tree->clear();
	mesh_tree->clear();

	// Hidden roots.
	material_tree->create_item();
	mesh_tree->create_item();

	_fill_scene(scene, nullptr);
}

void SceneImportSettingsDialog::_update_view_gizmos() {
	if (!is_visible()) {
		return;
	}
	const HashMap<StringName, Variant> &main_settings = scene_import_settings_data->current;
	bool reshow_settings = false;
	if (main_settings.has("nodes/import_as_skeleton_bones")) {
		bool new_import_as_skeleton = main_settings["nodes/import_as_skeleton_bones"];
		reshow_settings = reshow_settings || (new_import_as_skeleton != previous_import_as_skeleton);
		previous_import_as_skeleton = new_import_as_skeleton;
	}
	if (main_settings.has("animation/import_rest_as_RESET")) {
		bool new_rest_as_reset = main_settings["animation/import_rest_as_RESET"];
		reshow_settings = reshow_settings || (new_rest_as_reset != previous_rest_as_reset);
		previous_rest_as_reset = new_rest_as_reset;
	}
	if (reshow_settings) {
		_re_import();
		open_settings(base_path);
		return;
	}
	for (const KeyValue<String, NodeData> &e : node_map) {
		// Skip import nodes that aren't MeshInstance3D.
		const MeshInstance3D *mesh_node = Object::cast_to<MeshInstance3D>(e.value.node);
		if (mesh_node == nullptr || mesh_node->get_mesh().is_null()) {
			continue;
		}

		// Determine if the mesh collider should be visible.
		bool show_collider_view = false;
		if (e.value.settings.has(SNAME("generate/physics"))) {
			show_collider_view = e.value.settings[SNAME("generate/physics")];
		}

		// Get the collider_view MeshInstance3D.
		TypedArray<Node> descendants = mesh_node->find_children("collider_view", "MeshInstance3D");
		CRASH_COND_MSG(descendants.is_empty(), "This is unreachable, since the collider view is always created even when the collision is not used! If this is triggered there is a bug on the function `_fill_scene`.");
		MeshInstance3D *collider_view = Object::cast_to<MeshInstance3D>(descendants[0].operator Object *());

		// Regenerate the physics collider for this MeshInstance3D if either:
		// - A regeneration is requested for the selected import node.
		// - The collider is being made visible.
		if ((generate_collider && e.key == selected_id) || (show_collider_view && !collider_view->is_visible())) {
			// This collider_view doesn't have a mesh so we need to generate a new one.
			Ref<ImporterMesh> mesh;
			mesh.instantiate();
			// ResourceImporterScene::get_collision_shapes() expects ImporterMesh, not Mesh.
			// TODO: Duplicate code with EditorSceneFormatImporterESCN::import_scene()
			// Consider making a utility function to convert from Mesh to ImporterMesh.
			Ref<Mesh> mesh_3d_mesh = mesh_node->get_mesh();
			Ref<ArrayMesh> array_mesh_3d_mesh = mesh_3d_mesh;
			if (array_mesh_3d_mesh.is_valid()) {
				// For the MeshInstance3D nodes, we need to convert the ArrayMesh to an ImporterMesh specially.
				mesh->set_name(array_mesh_3d_mesh->get_name());
				for (int32_t blend_i = 0; blend_i < array_mesh_3d_mesh->get_blend_shape_count(); blend_i++) {
					mesh->add_blend_shape(array_mesh_3d_mesh->get_blend_shape_name(blend_i));
				}
				for (int32_t surface_i = 0; surface_i < array_mesh_3d_mesh->get_surface_count(); surface_i++) {
					mesh->add_surface(array_mesh_3d_mesh->surface_get_primitive_type(surface_i),
							array_mesh_3d_mesh->surface_get_arrays(surface_i),
							array_mesh_3d_mesh->surface_get_blend_shape_arrays(surface_i),
							array_mesh_3d_mesh->surface_get_lods(surface_i),
							array_mesh_3d_mesh->surface_get_material(surface_i),
							array_mesh_3d_mesh->surface_get_name(surface_i),
							array_mesh_3d_mesh->surface_get_format(surface_i));
				}
				mesh->set_blend_shape_mode(array_mesh_3d_mesh->get_blend_shape_mode());
			} else if (mesh_3d_mesh.is_valid()) {
				// For the MeshInstance3D nodes, we need to convert the Mesh to an ImporterMesh specially.
				mesh->set_name(mesh_3d_mesh->get_name());
				for (int32_t surface_i = 0; surface_i < mesh_3d_mesh->get_surface_count(); surface_i++) {
					mesh->add_surface(mesh_3d_mesh->surface_get_primitive_type(surface_i),
							mesh_3d_mesh->surface_get_arrays(surface_i),
							Array(),
							mesh_3d_mesh->surface_get_lods(surface_i),
							mesh_3d_mesh->surface_get_material(surface_i),
							mesh_3d_mesh->surface_get_material(surface_i).is_valid() ? mesh_3d_mesh->surface_get_material(surface_i)->get_name() : String(),
							mesh_3d_mesh->surface_get_format(surface_i));
				}
			}

			// Generate the mesh collider.
			Vector<Ref<Shape3D>> shapes = ResourceImporterScene::get_collision_shapes(mesh, e.value.settings, 1.0);
			const Transform3D transform = ResourceImporterScene::get_collision_shapes_transform(e.value.settings);

			Ref<ArrayMesh> collider_view_mesh;
			collider_view_mesh.instantiate();
			for (Ref<Shape3D> shape : shapes) {
				Ref<ArrayMesh> debug_shape_mesh;
				if (shape.is_valid()) {
					debug_shape_mesh = shape->get_debug_mesh();
				}
				if (debug_shape_mesh.is_valid()) {
					collider_view_mesh->add_surface_from_arrays(
							debug_shape_mesh->surface_get_primitive_type(0),
							debug_shape_mesh->surface_get_arrays(0));

					collider_view_mesh->surface_set_material(
							collider_view_mesh->get_surface_count() - 1,
							collider_mat);
				}
			}

			collider_view->set_mesh(collider_view_mesh);
			collider_view->set_transform(transform);
		}

		// Set the collider visibility.
		collider_view->set_visible(show_collider_view);
	}

	generate_collider = false;
}

void SceneImportSettingsDialog::_update_camera() {
	AABB camera_aabb;

	float rot_x = cam_rot_x;
	float rot_y = cam_rot_y;
	float zoom = cam_zoom;

	if (selected_type == "Node" || selected_type == "Animation" || selected_type.is_empty()) {
		camera_aabb = contents_aabb;
	} else {
		if (mesh_preview->get_mesh().is_valid()) {
			camera_aabb = mesh_preview->get_transform().xform(mesh_preview->get_mesh()->get_aabb());
		} else {
			camera_aabb = AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));
		}
		if (selected_type == "Mesh" && mesh_map.has(selected_id)) {
			const MeshData &md = mesh_map[selected_id];
			rot_x = md.cam_rot_x;
			rot_y = md.cam_rot_y;
			zoom = md.cam_zoom;
		} else if (selected_type == "Material" && material_map.has(selected_id)) {
			const MaterialData &md = material_map[selected_id];
			rot_x = md.cam_rot_x;
			rot_y = md.cam_rot_y;
			zoom = md.cam_zoom;
		}
	}

	Vector3 center = camera_aabb.get_center();
	float camera_size = camera_aabb.get_longest_axis_size();

	camera->set_orthogonal(camera_size * zoom, 0.0001, camera_size * 2);

	Transform3D xf;
	xf.basis = Basis(Vector3(0, 1, 0), rot_y) * Basis(Vector3(1, 0, 0), rot_x);
	xf.origin = center;
	xf.translate_local(0, 0, camera_size);

	camera->set_transform(xf);
}

void SceneImportSettingsDialog::_load_default_subresource_settings(HashMap<StringName, Variant> &settings, const String &p_type, const String &p_import_id, ResourceImporterScene::InternalImportCategory p_category) {
	if (base_subresource_settings.has(p_type)) {
		Dictionary d = base_subresource_settings[p_type];
		if (d.has(p_import_id)) {
			d = d[p_import_id];
			List<ResourceImporterScene::ImportOption> options;
			if (editing_animation) {
				ResourceImporterScene::get_animation_singleton()->get_internal_import_options(p_category, &options);
			} else {
				ResourceImporterScene::get_scene_singleton()->get_internal_import_options(p_category, &options);
			}
			for (const ResourceImporterScene::ImportOption &E : options) {
				String key = E.option.name;
				if (d.has(key)) {
					settings[key] = d[key];
				}
			}
		}
	}
}

void SceneImportSettingsDialog::request_generate_collider() {
	generate_collider = true;
}

void SceneImportSettingsDialog::update_view() {
	update_view_timer->start();
}

void SceneImportSettingsDialog::open_settings(const String &p_path, const String &p_scene_import_type) {
	if (scene) {
		_cleanup();
		memdelete(scene);
		scene = nullptr;
	}

	editing_animation = p_scene_import_type == "AnimationLibrary";
	scene_import_settings_data->settings = nullptr;
	scene_import_settings_data->path = p_path;

	// Visibility.
	data_mode->set_tab_hidden(1, editing_animation);
	data_mode->set_tab_hidden(2, editing_animation);
	if (editing_animation) {
		data_mode->set_current_tab(0);
	}

	action_menu->get_popup()->set_item_disabled(action_menu->get_popup()->get_item_id(ACTION_EXTRACT_MATERIALS), editing_animation);
	action_menu->get_popup()->set_item_disabled(action_menu->get_popup()->get_item_id(ACTION_CHOOSE_MESH_SAVE_PATHS), editing_animation);

	base_path = p_path;

	mesh_set.clear();
	animation_map.clear();
	material_map.clear();
	unnamed_material_name_map.clear();
	mesh_map.clear();
	node_map.clear();
	defaults.clear();

	mesh_preview->hide();

	selected_id = "";
	selected_type = "";

	cam_rot_x = -Math_PI / 4;
	cam_rot_y = -Math_PI / 4;
	cam_zoom = 1;

	{
		base_subresource_settings.clear();

		Ref<ConfigFile> config;
		config.instantiate();
		Error err = config->load(p_path + ".import");
		if (err == OK) {
			List<String> keys;
			config->get_section_keys("params", &keys);
			for (const String &E : keys) {
				Variant value = config->get_value("params", E);
				if (E == "_subresources") {
					base_subresource_settings = value;
				} else {
					defaults[E] = value;
				}
			}
		}
	}

	scene = ResourceImporterScene::get_scene_singleton()->pre_import(p_path, defaults); // Use the scene singleton here because we want to see the full thing.
	if (scene == nullptr) {
		EditorNode::get_singleton()->show_warning(TTR("Error opening scene"));
		return;
	}

	first_aabb = true;

	_update_scene();

	base_viewport->add_child(scene);

	inspector->edit(nullptr);

	if (first_aabb) {
		contents_aabb = AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));
		first_aabb = false;
	}

	const HashMap<StringName, Variant> &main_settings = scene_import_settings_data->current;
	if (main_settings.has("nodes/import_as_skeleton_bones")) {
		previous_import_as_skeleton = main_settings["nodes/import_as_skeleton_bones"];
	}
	if (main_settings.has("animation/import_rest_as_RESET")) {
		previous_rest_as_reset = main_settings["animation/import_rest_as_RESET"];
	}
	popup_centered_ratio();
	_update_view_gizmos();
	_update_camera();

	// Start with the root item (Scene) selected.
	scene_tree->get_root()->select(0);

	if (editing_animation) {
		set_title(vformat(TTR("Advanced Import Settings for AnimationLibrary '%s'"), base_path.get_file()));
	} else {
		set_title(vformat(TTR("Advanced Import Settings for Scene '%s'"), base_path.get_file()));
	}
}

SceneImportSettingsDialog *SceneImportSettingsDialog::singleton = nullptr;

SceneImportSettingsDialog *SceneImportSettingsDialog::get_singleton() {
	return singleton;
}

Node *SceneImportSettingsDialog::get_selected_node() {
	if (selected_id == "") {
		return nullptr;
	}
	return node_map[selected_id].node;
}

void SceneImportSettingsDialog::_select(Tree *p_from, const String &p_type, const String &p_id) {
	selecting = true;
	scene_import_settings_data->hide_options = false;

	bones_mesh_preview->hide();
	if (p_type == "Node") {
		node_selected->hide(); // Always hide just in case.
		mesh_preview->hide();
		_reset_animation();

		if (Object::cast_to<Node3D>(scene)) {
			Object::cast_to<Node3D>(scene)->show();
		}
		material_tree->deselect_all();
		mesh_tree->deselect_all();
		NodeData &nd = node_map[p_id];

		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(nd.node);
		if (mi) {
			Ref<Mesh> base_mesh = mi->get_mesh();
			if (base_mesh.is_valid()) {
				AABB aabb = base_mesh->get_aabb();
				Transform3D aabb_xf;
				aabb_xf.basis.scale(aabb.size);
				aabb_xf.origin = aabb.position;

				aabb_xf = mi->get_global_transform() * aabb_xf;
				node_selected->set_transform(aabb_xf);
				node_selected->show();
			}
		}

		if (nd.node == scene) {
			scene_import_settings_data->settings = &defaults;
			scene_import_settings_data->category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MAX;
		} else {
			scene_import_settings_data->settings = &nd.settings;
			if (mi) {
				scene_import_settings_data->category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MESH_3D_NODE;
				scene_import_settings_data->hide_options = editing_animation;
			} else if (Object::cast_to<AnimationPlayer>(nd.node)) {
				scene_import_settings_data->category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_ANIMATION_NODE;
			} else if (Object::cast_to<Skeleton3D>(nd.node)) {
				scene_import_settings_data->category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_SKELETON_3D_NODE;
				bones_mesh_preview->show();
			} else {
				scene_import_settings_data->category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_NODE;
				scene_import_settings_data->hide_options = editing_animation;
			}
		}
	} else if (p_type == "Animation") {
		node_selected->hide(); // Always hide just in case.
		mesh_preview->hide();
		_reset_animation(p_id);

		if (Object::cast_to<Node3D>(scene)) {
			Object::cast_to<Node3D>(scene)->show();
		}
		material_tree->deselect_all();
		mesh_tree->deselect_all();
		AnimationData &ad = animation_map[p_id];

		scene_import_settings_data->settings = &ad.settings;
		scene_import_settings_data->category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_ANIMATION;

		_animation_update_skeleton_visibility();
	} else if (p_type == "Mesh") {
		node_selected->hide();
		if (Object::cast_to<Node3D>(scene)) {
			Object::cast_to<Node3D>(scene)->hide();
		}

		MeshData &md = mesh_map[p_id];
		if (md.mesh_node != nullptr) {
			if (p_from != mesh_tree) {
				md.mesh_node->uncollapse_tree();
				md.mesh_node->select(0);
				mesh_tree->ensure_cursor_is_visible();
			}
			if (p_from != scene_tree) {
				md.scene_node->uncollapse_tree();
				md.scene_node->select(0);
				scene_tree->ensure_cursor_is_visible();
			}
		}

		mesh_preview->set_mesh(md.mesh);
		mesh_preview->show();
		_reset_animation();

		material_tree->deselect_all();

		scene_import_settings_data->settings = &md.settings;
		scene_import_settings_data->category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MESH;
	} else if (p_type == "Material") {
		node_selected->hide();
		if (Object::cast_to<Node3D>(scene)) {
			Object::cast_to<Node3D>(scene)->hide();
		}

		mesh_preview->show();
		_reset_animation();

		MaterialData &md = material_map[p_id];

		material_preview->set_material(md.material);
		mesh_preview->set_mesh(material_preview);

		if (p_from != mesh_tree) {
			md.mesh_node->uncollapse_tree();
			md.mesh_node->select(0);
			mesh_tree->ensure_cursor_is_visible();
		}
		if (p_from != scene_tree) {
			md.scene_node->uncollapse_tree();
			md.scene_node->select(0);
			scene_tree->ensure_cursor_is_visible();
		}
		if (p_from != material_tree) {
			md.material_node->uncollapse_tree();
			md.material_node->select(0);
			material_tree->ensure_cursor_is_visible();
		}

		scene_import_settings_data->settings = &md.settings;
		scene_import_settings_data->category = ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MATERIAL;
	}

	selected_type = p_type;
	selected_id = p_id;

	selecting = false;

	_update_camera();

	List<ResourceImporter::ImportOption> options;

	if (editing_animation) {
		if (scene_import_settings_data->category == ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MAX) {
			ResourceImporterScene::get_animation_singleton()->get_import_options(base_path, &options);
		} else {
			ResourceImporterScene::get_animation_singleton()->get_internal_import_options(scene_import_settings_data->category, &options);
		}

	} else {
		if (scene_import_settings_data->category == ResourceImporterScene::INTERNAL_IMPORT_CATEGORY_MAX) {
			ResourceImporterScene::get_scene_singleton()->get_import_options(base_path, &options);
		} else {
			ResourceImporterScene::get_scene_singleton()->get_internal_import_options(scene_import_settings_data->category, &options);
		}
	}

	scene_import_settings_data->defaults.clear();
	scene_import_settings_data->current.clear();

	if (scene_import_settings_data->settings) {
		for (const ResourceImporter::ImportOption &E : options) {
			scene_import_settings_data->defaults[E.option.name] = E.default_value;
			// Needed for visibility toggling (fails if something is missing).
			if (scene_import_settings_data->settings->has(E.option.name)) {
				scene_import_settings_data->current[E.option.name] = (*scene_import_settings_data->settings)[E.option.name];
			} else {
				scene_import_settings_data->current[E.option.name] = E.default_value;
			}
		}
	}

	scene_import_settings_data->options = options;
	inspector->edit(scene_import_settings_data);
	scene_import_settings_data->notify_property_list_changed();
}

void SceneImportSettingsDialog::_inspector_property_edited(const String &p_name) {
	if (p_name == "settings/loop_mode") {
		if (!animation_map.has(selected_id)) {
			return;
		}
		HashMap<StringName, Variant> settings = animation_map[selected_id].settings;
		if (settings.has(p_name)) {
			animation_loop_mode = static_cast<Animation::LoopMode>((int)settings[p_name]);
		} else {
			animation_loop_mode = Animation::LoopMode::LOOP_NONE;
		}
	}
}

void SceneImportSettingsDialog::_reset_bone_transforms() {
	for (Skeleton3D *skeleton : skeletons) {
		skeleton->reset_bone_poses();
	}
}

void SceneImportSettingsDialog::_play_animation() {
	if (animation_player == nullptr) {
		return;
	}
	StringName id = StringName(selected_id);
	if (animation_player->has_animation(id)) {
		if (animation_player->is_playing()) {
			animation_player->pause();
			animation_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
			set_process(false);
		} else {
			animation_player->play(id);
			animation_play_button->set_button_icon(get_editor_theme_icon(SNAME("Pause")));
			set_process(true);
		}
	}
}

void SceneImportSettingsDialog::_stop_current_animation() {
	animation_pingpong = false;
	animation_player->stop();
	animation_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
	animation_slider->set_value_no_signal(0.0);
	set_process(false);
}

void SceneImportSettingsDialog::_reset_animation(const String &p_animation_name) {
	if (p_animation_name.is_empty()) {
		animation_preview->hide();

		if (animation_player != nullptr && animation_player->is_playing()) {
			animation_player->stop();
		}
		animation_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));

		_reset_bone_transforms();
		set_process(false);
	} else {
		_reset_bone_transforms();
		animation_preview->show();

		animation_loop_mode = Animation::LoopMode::LOOP_NONE;
		animation_pingpong = false;

		if (animation_map.has(p_animation_name)) {
			HashMap<StringName, Variant> settings = animation_map[p_animation_name].settings;
			if (settings.has("settings/loop_mode")) {
				animation_loop_mode = static_cast<Animation::LoopMode>((int)settings["settings/loop_mode"]);
			}
		}

		if (animation_player->is_playing() && animation_loop_mode != Animation::LoopMode::LOOP_NONE) {
			animation_player->play(p_animation_name);
		} else {
			animation_player->stop(true);
			animation_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
			animation_player->set_assigned_animation(p_animation_name);
			animation_player->seek(0.0, true);
			animation_slider->set_value_no_signal(0.0);
			set_process(false);
		}
	}
}

void SceneImportSettingsDialog::_animation_slider_value_changed(double p_value) {
	if (animation_player == nullptr || !animation_map.has(selected_id) || animation_map[selected_id].animation.is_null()) {
		return;
	}
	if (animation_player->is_playing()) {
		animation_player->stop();
		animation_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
		set_process(false);
	}
	animation_player->seek(p_value * animation_map[selected_id].animation->get_length(), true);
}

void SceneImportSettingsDialog::_skeleton_tree_entered(Skeleton3D *p_skeleton) {
	bones_mesh_preview->set_skeleton_path(p_skeleton->get_path());
	bones_mesh_preview->set_skin(p_skeleton->register_skin(p_skeleton->create_skin_from_rest_transforms()));
}

void SceneImportSettingsDialog::_animation_finished(const StringName &p_name) {
	Animation::LoopMode loop_mode = animation_loop_mode;

	switch (loop_mode) {
		case Animation::LOOP_NONE: {
			animation_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
			animation_slider->set_value_no_signal(1.0);
			set_process(false);
		} break;
		case Animation::LOOP_LINEAR: {
			animation_player->play(p_name);
		} break;
		case Animation::LOOP_PINGPONG: {
			if (animation_pingpong) {
				animation_player->play(p_name);
			} else {
				animation_player->play_backwards(p_name);
			}
			animation_pingpong = !animation_pingpong;
		} break;
		default: {
		} break;
	}
}

void SceneImportSettingsDialog::_animation_update_skeleton_visibility() {
	if (animation_toggle_skeleton_visibility->is_pressed()) {
		bones_mesh_preview->show();
	} else {
		bones_mesh_preview->hide();
	}
}

void SceneImportSettingsDialog::_material_tree_selected() {
	if (selecting) {
		return;
	}
	TreeItem *item = material_tree->get_selected();
	String type = item->get_meta("type");
	String import_id = item->get_meta("import_id");

	_select(material_tree, type, import_id);
}

void SceneImportSettingsDialog::_mesh_tree_selected() {
	if (selecting) {
		return;
	}

	TreeItem *item = mesh_tree->get_selected();
	String type = item->get_meta("type");
	String import_id = item->get_meta("import_id");

	_select(mesh_tree, type, import_id);
}

void SceneImportSettingsDialog::_scene_tree_selected() {
	if (selecting) {
		return;
	}
	TreeItem *item = scene_tree->get_selected();
	String type = item->get_meta("type");
	String import_id = item->get_meta("import_id");

	_select(scene_tree, type, import_id);
}

void SceneImportSettingsDialog::_cleanup() {
	skeletons.clear();
	if (animation_player != nullptr) {
		animation_player->disconnect(SceneStringName(animation_finished), callable_mp(this, &SceneImportSettingsDialog::_animation_finished));
		animation_player = nullptr;
	}
	set_process(false);
}

void SceneImportSettingsDialog::_on_light_1_switch_pressed() {
	light1->set_visible(light_1_switch->is_pressed());
}

void SceneImportSettingsDialog::_on_light_2_switch_pressed() {
	light2->set_visible(light_2_switch->is_pressed());
}

void SceneImportSettingsDialog::_on_light_rotate_switch_pressed() {
	bool light_top_level = !light_rotate_switch->is_pressed();
	light1->set_as_top_level_keep_local(light_top_level);
	light2->set_as_top_level_keep_local(light_top_level);
}

void SceneImportSettingsDialog::_viewport_input(const Ref<InputEvent> &p_input) {
	float *rot_x = &cam_rot_x;
	float *rot_y = &cam_rot_y;
	float *zoom = &cam_zoom;

	if (selected_type == "Mesh" && mesh_map.has(selected_id)) {
		MeshData &md = mesh_map[selected_id];
		rot_x = &md.cam_rot_x;
		rot_y = &md.cam_rot_y;
		zoom = &md.cam_zoom;
	} else if (selected_type == "Material" && material_map.has(selected_id)) {
		MaterialData &md = material_map[selected_id];
		rot_x = &md.cam_rot_x;
		rot_y = &md.cam_rot_y;
		zoom = &md.cam_zoom;
	}
	Ref<InputEventMouseMotion> mm = p_input;
	if (mm.is_valid() && (mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
		(*rot_x) -= mm->get_relative().y * 0.01 * EDSCALE;
		(*rot_y) -= mm->get_relative().x * 0.01 * EDSCALE;
		(*rot_x) = CLAMP((*rot_x), -Math_PI / 2, Math_PI / 2);
		_update_camera();
	}
	if (mm.is_valid() && DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CURSOR_SHAPE)) {
		DisplayServer::get_singleton()->cursor_set_shape(DisplayServer::CursorShape::CURSOR_ARROW);
	}
	Ref<InputEventMouseButton> mb = p_input;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::WHEEL_DOWN) {
		(*zoom) *= 1.1;
		if ((*zoom) > 10.0) {
			(*zoom) = 10.0;
		}
		_update_camera();
	}
	if (mb.is_valid() && mb->get_button_index() == MouseButton::WHEEL_UP) {
		(*zoom) /= 1.1;
		if ((*zoom) < 0.1) {
			(*zoom) = 0.1;
		}
		_update_camera();
	}
	Ref<InputEventMagnifyGesture> mg = p_input;
	if (mg.is_valid()) {
		real_t mg_factor = mg->get_factor();
		if (mg_factor == 0.0) {
			mg_factor = 1.0;
		}
		(*zoom) /= mg_factor;
		if ((*zoom) < 0.1) {
			(*zoom) = 0.1;
		} else if ((*zoom) > 10.0) {
			(*zoom) = 10.0;
		}
		_update_camera();
	}
}

void SceneImportSettingsDialog::_re_import() {
	HashMap<StringName, Variant> main_settings;

	main_settings = scene_import_settings_data->current;
	main_settings.erase("_subresources");
	Dictionary nodes;
	Dictionary materials;
	Dictionary meshes;
	Dictionary animations;

	Dictionary subresources;

	for (KeyValue<String, NodeData> &E : node_map) {
		if (E.value.settings.size()) {
			Dictionary d;
			for (const KeyValue<StringName, Variant> &F : E.value.settings) {
				d[String(F.key)] = F.value;
			}
			nodes[E.key] = d;
		}
	}
	if (nodes.size()) {
		subresources["nodes"] = nodes;
	}

	for (KeyValue<String, MaterialData> &E : material_map) {
		if (E.value.settings.size()) {
			Dictionary d;
			for (const KeyValue<StringName, Variant> &F : E.value.settings) {
				d[String(F.key)] = F.value;
			}
			materials[E.key] = d;
		}
	}
	if (materials.size()) {
		subresources["materials"] = materials;
	}

	for (KeyValue<String, MeshData> &E : mesh_map) {
		if (E.value.settings.size()) {
			Dictionary d;
			for (const KeyValue<StringName, Variant> &F : E.value.settings) {
				d[String(F.key)] = F.value;
			}
			meshes[E.key] = d;
		}
	}
	if (meshes.size()) {
		subresources["meshes"] = meshes;
	}

	for (KeyValue<String, AnimationData> &E : animation_map) {
		if (E.value.settings.size()) {
			Dictionary d;
			for (const KeyValue<StringName, Variant> &F : E.value.settings) {
				d[String(F.key)] = F.value;
			}
			animations[E.key] = d;
		}
	}
	if (animations.size()) {
		subresources["animations"] = animations;
	}

	if (subresources.size()) {
		main_settings["_subresources"] = subresources;
	}

	_cleanup(); // Prevent skeletons and other pointers from pointing to dangling references.
	EditorFileSystem::get_singleton()->reimport_file_with_custom_parameters(base_path, editing_animation ? "animation_library" : "scene", main_settings);
}

void SceneImportSettingsDialog::_update_theme_item_cache() {
	ConfirmationDialog::_update_theme_item_cache();
	theme_cache.light_1_icon = get_editor_theme_icon(SNAME("MaterialPreviewLight1"));
	theme_cache.light_2_icon = get_editor_theme_icon(SNAME("MaterialPreviewLight2"));
	theme_cache.rotate_icon = get_editor_theme_icon(SNAME("PreviewRotate"));
}

void SceneImportSettingsDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			connect(SceneStringName(confirmed), callable_mp(this, &SceneImportSettingsDialog::_re_import));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			action_menu->begin_bulk_theme_override();
			action_menu->add_theme_style_override(CoreStringName(normal), get_theme_stylebox(CoreStringName(normal), "Button"));
			action_menu->add_theme_style_override(SceneStringName(hover), get_theme_stylebox(SceneStringName(hover), "Button"));
			action_menu->add_theme_style_override(SceneStringName(pressed), get_theme_stylebox(SceneStringName(pressed), "Button"));
			action_menu->end_bulk_theme_override();

			if (animation_player != nullptr && animation_player->is_playing()) {
				animation_play_button->set_button_icon(get_editor_theme_icon(SNAME("Pause")));
			} else {
				animation_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
			}
			animation_stop_button->set_button_icon(get_editor_theme_icon(SNAME("Stop")));

			light_1_switch->set_button_icon(theme_cache.light_1_icon);
			light_2_switch->set_button_icon(theme_cache.light_2_icon);
			light_rotate_switch->set_button_icon(theme_cache.rotate_icon);

			animation_toggle_skeleton_visibility->set_button_icon(get_editor_theme_icon(SNAME("SkeletonPreview")));
		} break;

		case NOTIFICATION_PROCESS: {
			if (animation_player != nullptr) {
				animation_slider->set_value_no_signal(animation_player->get_current_animation_position() / animation_player->get_current_animation_length());
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				_cleanup();
			}
		} break;
	}
}

void SceneImportSettingsDialog::_menu_callback(int p_id) {
	switch (p_id) {
		case ACTION_EXTRACT_MATERIALS: {
			save_path->set_title(TTR("Select folder to extract material resources"));
			external_extension_type->select(0);
		} break;
		case ACTION_CHOOSE_MESH_SAVE_PATHS: {
			save_path->set_title(TTR("Select folder where mesh resources will save on import"));
			external_extension_type->select(1);
		} break;
		case ACTION_CHOOSE_ANIMATION_SAVE_PATHS: {
			save_path->set_title(TTR("Select folder where animations will save on import"));
			external_extension_type->select(1);
		} break;
	}

	save_path->set_current_dir(base_path.get_base_dir());
	current_action = p_id;
	save_path->popup_centered_ratio();
}

void SceneImportSettingsDialog::_save_path_changed(const String &p_path) {
	save_path_item->set_text(1, p_path);

	if (FileAccess::exists(p_path)) {
		save_path_item->set_text(2, TTR("Warning: File exists"));
		save_path_item->set_tooltip_text(2, TTR("Existing file with the same name will be replaced."));
		save_path_item->set_icon(2, get_editor_theme_icon(SNAME("StatusWarning")));

	} else {
		save_path_item->set_text(2, TTR("Will create new file"));
		save_path_item->set_icon(2, get_editor_theme_icon(SNAME("StatusSuccess")));
	}
}

void SceneImportSettingsDialog::_browse_save_callback(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}

	TreeItem *item = Object::cast_to<TreeItem>(p_item);

	String path = item->get_text(1);

	item_save_path->set_current_file(path);
	save_path_item = item;

	item_save_path->popup_centered_ratio();
}

void SceneImportSettingsDialog::_save_dir_callback(const String &p_path) {
	external_path_tree->clear();
	TreeItem *root = external_path_tree->create_item();
	save_path_items.clear();

	switch (current_action) {
		case ACTION_EXTRACT_MATERIALS: {
			for (const KeyValue<String, MaterialData> &E : material_map) {
				MaterialData &md = material_map[E.key];

				TreeItem *item = external_path_tree->create_item(root);

				String name = md.material_node->get_text(0);

				item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				item->set_icon(0, get_editor_theme_icon(SNAME("StandardMaterial3D")));
				item->set_text(0, name);

				if (md.has_import_id) {
					if (md.settings.has("use_external/enabled") && bool(md.settings["use_external/enabled"])) {
						item->set_text(2, TTR("Already External"));
						item->set_tooltip_text(2, TTR("This material already references an external file, no action will be taken.\nDisable the external property for it to be extracted again."));
					} else {
						item->set_metadata(0, E.key);
						item->set_editable(0, true);
						item->set_checked(0, true);
						name = name.validate_filename();
						String path = p_path.path_join(name);
						if (external_extension_type->get_selected() == 0) {
							path += ".tres";
						} else {
							path += ".res";
						}

						item->set_text(1, path);
						if (FileAccess::exists(path)) {
							item->set_text(2, TTR("Warning: File exists"));
							item->set_tooltip_text(2, TTR("Existing file with the same name will be replaced."));
							item->set_icon(2, get_editor_theme_icon(SNAME("StatusWarning")));

						} else {
							item->set_text(2, TTR("Will create new file"));
							item->set_icon(2, get_editor_theme_icon(SNAME("StatusSuccess")));
						}

						item->add_button(1, get_editor_theme_icon(SNAME("Folder")));
					}

				} else {
					item->set_text(2, TTR("No import ID"));
					item->set_tooltip_text(2, TTR("Material has no name nor any other way to identify on re-import.\nPlease name it or ensure it is exported with an unique ID."));
					item->set_icon(2, get_editor_theme_icon(SNAME("StatusError")));
				}

				save_path_items.push_back(item);
			}

			external_paths->set_title(TTR("Extract Materials to Resource Files"));
			external_paths->set_ok_button_text(TTR("Extract"));
		} break;
		case ACTION_CHOOSE_MESH_SAVE_PATHS: {
			for (const KeyValue<String, MeshData> &E : mesh_map) {
				MeshData &md = mesh_map[E.key];

				TreeItem *item = external_path_tree->create_item(root);

				String name = md.mesh_node->get_text(0);

				item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				item->set_icon(0, get_editor_theme_icon(SNAME("MeshItem")));
				item->set_text(0, name);

				if (md.has_import_id) {
					if (md.settings.has("save_to_file/enabled") && bool(md.settings["save_to_file/enabled"])) {
						item->set_text(2, TTR("Already Saving"));
						item->set_tooltip_text(2, TTR("This mesh already saves to an external resource, no action will be taken."));
					} else {
						item->set_metadata(0, E.key);
						item->set_editable(0, true);
						item->set_checked(0, true);
						name = name.validate_filename();
						String path = p_path.path_join(name);
						if (external_extension_type->get_selected() == 0) {
							path += ".tres";
						} else {
							path += ".res";
						}

						item->set_text(1, path);
						if (FileAccess::exists(path)) {
							item->set_text(2, TTR("Warning: File exists"));
							item->set_tooltip_text(2, TTR("Existing file with the same name will be replaced on import."));
							item->set_icon(2, get_editor_theme_icon(SNAME("StatusWarning")));

						} else {
							item->set_text(2, TTR("Will save to new file"));
							item->set_icon(2, get_editor_theme_icon(SNAME("StatusSuccess")));
						}

						item->add_button(1, get_editor_theme_icon(SNAME("Folder")));
					}

				} else {
					item->set_text(2, TTR("No import ID"));
					item->set_tooltip_text(2, TTR("Mesh has no name nor any other way to identify on re-import.\nPlease name it or ensure it is exported with an unique ID."));
					item->set_icon(2, get_editor_theme_icon(SNAME("StatusError")));
				}

				save_path_items.push_back(item);
			}

			external_paths->set_title(TTR("Set paths to save meshes as resource files on Reimport"));
			external_paths->set_ok_button_text(TTR("Set Paths"));
		} break;
		case ACTION_CHOOSE_ANIMATION_SAVE_PATHS: {
			for (const KeyValue<String, AnimationData> &E : animation_map) {
				AnimationData &ad = animation_map[E.key];

				TreeItem *item = external_path_tree->create_item(root);

				String name = ad.scene_node->get_text(0);

				item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				item->set_icon(0, get_editor_theme_icon(SNAME("Animation")));
				item->set_text(0, name);

				if (ad.settings.has("save_to_file/enabled") && bool(ad.settings["save_to_file/enabled"])) {
					item->set_text(2, TTR("Already Saving"));
					item->set_tooltip_text(2, TTR("This animation already saves to an external resource, no action will be taken."));
				} else {
					item->set_metadata(0, E.key);
					item->set_editable(0, true);
					item->set_checked(0, true);
					name = name.validate_filename();
					String path = p_path.path_join(name);
					if (external_extension_type->get_selected() == 0) {
						path += ".tres";
					} else {
						path += ".res";
					}

					item->set_text(1, path);
					if (FileAccess::exists(path)) {
						item->set_text(2, TTR("Warning: File exists"));
						item->set_tooltip_text(2, TTR("Existing file with the same name will be replaced on import."));
						item->set_icon(2, get_editor_theme_icon(SNAME("StatusWarning")));

					} else {
						item->set_text(2, TTR("Will save to new file"));
						item->set_icon(2, get_editor_theme_icon(SNAME("StatusSuccess")));
					}

					item->add_button(1, get_editor_theme_icon(SNAME("Folder")));
				}

				save_path_items.push_back(item);
			}

			external_paths->set_title(TTR("Set paths to save animations as resource files on Reimport"));
			external_paths->set_ok_button_text(TTR("Set Paths"));

		} break;
	}

	external_paths->popup_centered_ratio();
}

void SceneImportSettingsDialog::_save_dir_confirm() {
	for (int i = 0; i < save_path_items.size(); i++) {
		TreeItem *item = save_path_items[i];
		if (!item->is_checked(0)) {
			continue; //ignore
		}
		String path = item->get_text(1);
		if (!path.is_resource_file()) {
			continue;
		}

		String id = item->get_metadata(0);

		switch (current_action) {
			case ACTION_EXTRACT_MATERIALS: {
				ERR_CONTINUE(!material_map.has(id));
				MaterialData &md = material_map[id];

				Error err = ResourceSaver::save(md.material, path);
				if (err != OK) {
					EditorNode::get_singleton()->add_io_error(TTR("Can't make material external to file, write error:") + "\n\t" + path);
					continue;
				}

				md.settings["use_external/enabled"] = true;
				md.settings["use_external/path"] = path;

			} break;
			case ACTION_CHOOSE_MESH_SAVE_PATHS: {
				ERR_CONTINUE(!mesh_map.has(id));
				MeshData &md = mesh_map[id];

				md.settings["save_to_file/enabled"] = true;
				md.settings["save_to_file/path"] = path;
			} break;
			case ACTION_CHOOSE_ANIMATION_SAVE_PATHS: {
				ERR_CONTINUE(!animation_map.has(id));
				AnimationData &ad = animation_map[id];

				ad.settings["save_to_file/enabled"] = true;
				ad.settings["save_to_file/path"] = path;

			} break;
		}
	}

	if (current_action == ACTION_EXTRACT_MATERIALS) {
		//as this happens right now, the scene needs to be saved and reimported.
		_re_import();
		open_settings(base_path);
	} else {
		scene_import_settings_data->notify_property_list_changed();
	}
}

SceneImportSettingsDialog::SceneImportSettingsDialog() {
	singleton = this;

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);
	HBoxContainer *menu_hb = memnew(HBoxContainer);
	main_vb->add_child(menu_hb);

	action_menu = memnew(MenuButton);
	action_menu->set_text(TTR("Actions..."));
	// Style the MenuButton like a regular Button to make it more noticeable.
	action_menu->set_flat(false);
	action_menu->set_focus_mode(Control::FOCUS_ALL);
	menu_hb->add_child(action_menu);

	action_menu->get_popup()->add_item(TTR("Extract Materials"), ACTION_EXTRACT_MATERIALS);
	action_menu->get_popup()->add_separator();
	action_menu->get_popup()->add_item(TTR("Set Animation Save Paths"), ACTION_CHOOSE_ANIMATION_SAVE_PATHS);
	action_menu->get_popup()->add_item(TTR("Set Mesh Save Paths"), ACTION_CHOOSE_MESH_SAVE_PATHS);

	action_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &SceneImportSettingsDialog::_menu_callback));

	tree_split = memnew(HSplitContainer);
	main_vb->add_child(tree_split);
	tree_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	data_mode = memnew(TabContainer);
	tree_split->add_child(data_mode);
	data_mode->set_custom_minimum_size(Size2(300 * EDSCALE, 0));
	data_mode->set_theme_type_variation("TabContainerOdd");

	property_split = memnew(HSplitContainer);
	tree_split->add_child(property_split);
	property_split->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	scene_tree = memnew(Tree);
	scene_tree->set_name(TTR("Scene"));
	scene_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	data_mode->add_child(scene_tree);
	scene_tree->connect("cell_selected", callable_mp(this, &SceneImportSettingsDialog::_scene_tree_selected));

	mesh_tree = memnew(Tree);
	mesh_tree->set_name(TTR("Meshes"));
	mesh_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	data_mode->add_child(mesh_tree);
	mesh_tree->set_hide_root(true);
	mesh_tree->connect("cell_selected", callable_mp(this, &SceneImportSettingsDialog::_mesh_tree_selected));

	material_tree = memnew(Tree);
	material_tree->set_name(TTR("Materials"));
	material_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	data_mode->add_child(material_tree);
	material_tree->connect("cell_selected", callable_mp(this, &SceneImportSettingsDialog::_material_tree_selected));

	material_tree->set_hide_root(true);

	VBoxContainer *vp_vb = memnew(VBoxContainer);
	vp_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vp_vb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vp_vb->set_anchors_and_offsets_preset(Control::LayoutPreset::PRESET_FULL_RECT);
	property_split->add_child(vp_vb);

	SubViewportContainer *vp_container = memnew(SubViewportContainer);
	vp_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vp_container->set_custom_minimum_size(Size2(10, 10));
	vp_container->set_stretch(true);
	vp_container->connect(SceneStringName(gui_input), callable_mp(this, &SceneImportSettingsDialog::_viewport_input));
	vp_vb->add_child(vp_container);

	base_viewport = memnew(SubViewport);
	vp_container->add_child(base_viewport);

	animation_preview = memnew(PanelContainer);
	animation_preview->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vp_vb->add_child(animation_preview);
	animation_preview->hide();

	HBoxContainer *animation_hbox = memnew(HBoxContainer);
	animation_preview->add_child(animation_hbox);

	animation_play_button = memnew(Button);
	animation_hbox->add_child(animation_play_button);
	animation_play_button->set_flat(true);
	animation_play_button->set_focus_mode(Control::FOCUS_NONE);
	animation_play_button->set_shortcut(ED_SHORTCUT("scene_import_settings/play_selected_animation", TTRC("Selected Animation Play/Pause"), Key::SPACE));
	animation_play_button->connect(SceneStringName(pressed), callable_mp(this, &SceneImportSettingsDialog::_play_animation));

	animation_stop_button = memnew(Button);
	animation_hbox->add_child(animation_stop_button);
	animation_stop_button->set_flat(true);
	animation_stop_button->set_focus_mode(Control::FOCUS_NONE);
	animation_stop_button->set_tooltip_text(TTR("Selected Animation Stop"));
	animation_stop_button->connect(SceneStringName(pressed), callable_mp(this, &SceneImportSettingsDialog::_stop_current_animation));

	animation_slider = memnew(HSlider);
	animation_hbox->add_child(animation_slider);
	animation_slider->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	animation_slider->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	animation_slider->set_max(1.0);
	animation_slider->set_step(1.0 / 100.0);
	animation_slider->set_value_no_signal(0.0);
	animation_slider->set_focus_mode(Control::FOCUS_NONE);
	animation_slider->connect(SceneStringName(value_changed), callable_mp(this, &SceneImportSettingsDialog::_animation_slider_value_changed));

	animation_toggle_skeleton_visibility = memnew(Button);
	animation_hbox->add_child(animation_toggle_skeleton_visibility);
	animation_toggle_skeleton_visibility->set_toggle_mode(true);
	animation_toggle_skeleton_visibility->set_theme_type_variation("FlatButton");
	animation_toggle_skeleton_visibility->set_focus_mode(Control::FOCUS_NONE);
	animation_toggle_skeleton_visibility->set_tooltip_text(TTR("Toggle Animation Skeleton Visibility"));

	animation_toggle_skeleton_visibility->connect(SceneStringName(pressed), callable_mp(this, &SceneImportSettingsDialog::_animation_update_skeleton_visibility));

	base_viewport->set_use_own_world_3d(true);

	HBoxContainer *viewport_hbox = memnew(HBoxContainer);
	vp_container->add_child(viewport_hbox);
	viewport_hbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 2);

	viewport_hbox->add_spacer();

	VBoxContainer *vb_light = memnew(VBoxContainer);
	vb_light->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	viewport_hbox->add_child(vb_light);

	light_rotate_switch = memnew(Button);
	light_rotate_switch->set_theme_type_variation("PreviewLightButton");
	light_rotate_switch->set_toggle_mode(true);
	light_rotate_switch->set_pressed(true);
	light_rotate_switch->set_tooltip_text(TTR("Rotate Lights With Model"));
	light_rotate_switch->connect(SceneStringName(pressed), callable_mp(this, &SceneImportSettingsDialog::_on_light_rotate_switch_pressed));
	vb_light->add_child(light_rotate_switch);

	light_1_switch = memnew(Button);
	light_1_switch->set_theme_type_variation("PreviewLightButton");
	light_1_switch->set_toggle_mode(true);
	light_1_switch->set_pressed(true);
	light_1_switch->set_tooltip_text(TTR("Primary Light"));
	light_1_switch->connect(SceneStringName(pressed), callable_mp(this, &SceneImportSettingsDialog::_on_light_1_switch_pressed));
	vb_light->add_child(light_1_switch);

	light_2_switch = memnew(Button);
	light_2_switch->set_theme_type_variation("PreviewLightButton");
	light_2_switch->set_toggle_mode(true);
	light_2_switch->set_pressed(true);
	light_2_switch->set_tooltip_text(TTR("Secondary Light"));
	light_2_switch->connect(SceneStringName(pressed), callable_mp(this, &SceneImportSettingsDialog::_on_light_2_switch_pressed));
	vb_light->add_child(light_2_switch);

	camera = memnew(Camera3D);
	base_viewport->add_child(camera);
	camera->make_current();

	if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		camera_attributes.instantiate();
		camera->set_attributes(camera_attributes);
	}

	// Use a grayscale gradient sky to avoid skewing the preview towards a specific color,
	// but still allow shaded areas to be easily distinguished (using the ambient and reflected light).
	// This also helps the user orient themselves in the preview, since the bottom of the sky is black
	// and the top of the sky is white.
	procedural_sky_material.instantiate();
	procedural_sky_material->set_sky_top_color(Color(1, 1, 1));
	procedural_sky_material->set_sky_horizon_color(Color(0.5, 0.5, 0.5));
	procedural_sky_material->set_ground_horizon_color(Color(0.5, 0.5, 0.5));
	procedural_sky_material->set_ground_bottom_color(Color(0, 0, 0));
	procedural_sky_material->set_sky_curve(2.0);
	procedural_sky_material->set_ground_curve(0.5);
	// Hide the sun from the sky.
	procedural_sky_material->set_sun_angle_max(0.0);
	sky.instantiate();
	sky->set_material(procedural_sky_material);
	environment.instantiate();
	environment->set_background(Environment::BG_SKY);
	environment->set_sky(sky);
	// A custom FOV must be specified, as an orthogonal camera is used for the preview.
	environment->set_sky_custom_fov(50.0);
	camera->set_environment(environment);

	light1 = memnew(DirectionalLight3D);
	light1->set_transform(Transform3D(Basis::looking_at(Vector3(-1, -1, -1))));
	light1->set_shadow(true);
	camera->add_child(light1);

	light2 = memnew(DirectionalLight3D);
	light2->set_transform(Transform3D(Basis::looking_at(Vector3(0, 1, 0), Vector3(0, 0, 1))));
	light2->set_color(Color(0.5f, 0.5f, 0.5f));
	camera->add_child(light2);

	{
		Ref<StandardMaterial3D> selection_mat;
		selection_mat.instantiate();
		selection_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		selection_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
		selection_mat->set_albedo(Color(1, 0.8, 1.0));

		Ref<SurfaceTool> st;
		st.instantiate();
		st->begin(Mesh::PRIMITIVE_LINES);

		AABB base_aabb;
		base_aabb.size = Vector3(1, 1, 1);

		for (int i = 0; i < 12; i++) {
			Vector3 a, b;
			base_aabb.get_edge(i, a, b);

			st->add_vertex(a);
			st->add_vertex(a.lerp(b, 0.2));
			st->add_vertex(b);
			st->add_vertex(b.lerp(a, 0.2));
		}

		selection_mesh.instantiate();
		st->commit(selection_mesh);
		selection_mesh->surface_set_material(0, selection_mat);

		node_selected = memnew(MeshInstance3D);
		node_selected->set_mesh(selection_mesh);
		node_selected->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		base_viewport->add_child(node_selected);
		node_selected->hide();
	}

	{
		mesh_preview = memnew(MeshInstance3D);
		base_viewport->add_child(mesh_preview);
		mesh_preview->hide();

		material_preview.instantiate();
	}

	{
		collider_mat.instantiate();
		collider_mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		collider_mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
		collider_mat->set_albedo(Color(0.5, 0.5, 1.0));
	}

	{
		bones_mesh_preview = memnew(MeshInstance3D);
		bones_mesh_preview->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		bones_mesh_preview->set_skeleton_path(NodePath());
		base_viewport->add_child(bones_mesh_preview);
	}

	inspector = memnew(EditorInspector);
	inspector->set_custom_minimum_size(Size2(300 * EDSCALE, 0));
	inspector->connect(SNAME("property_edited"), callable_mp(this, &SceneImportSettingsDialog::_inspector_property_edited));

	property_split->add_child(inspector);

	scene_import_settings_data = memnew(SceneImportSettingsData);

	set_ok_button_text(TTR("Reimport"));
	set_cancel_button_text(TTR("Close"));

	external_paths = memnew(ConfirmationDialog);
	add_child(external_paths);
	external_path_tree = memnew(Tree);
	external_paths->add_child(external_path_tree);
	external_path_tree->connect("button_clicked", callable_mp(this, &SceneImportSettingsDialog::_browse_save_callback));
	external_paths->connect(SceneStringName(confirmed), callable_mp(this, &SceneImportSettingsDialog::_save_dir_confirm));
	external_path_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	external_path_tree->set_columns(3);
	external_path_tree->set_column_titles_visible(true);
	external_path_tree->set_column_expand(0, true);
	external_path_tree->set_column_custom_minimum_width(0, 100 * EDSCALE);
	external_path_tree->set_column_title(0, TTR("Resource"));
	external_path_tree->set_column_expand(1, true);
	external_path_tree->set_column_custom_minimum_width(1, 100 * EDSCALE);
	external_path_tree->set_column_title(1, TTR("Path"));
	external_path_tree->set_column_expand(2, false);
	external_path_tree->set_column_custom_minimum_width(2, 200 * EDSCALE);
	external_path_tree->set_column_title(2, TTR("Status"));
	save_path = memnew(EditorFileDialog);
	save_path->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
	HBoxContainer *extension_hb = memnew(HBoxContainer);
	save_path->get_vbox()->add_child(extension_hb);
	extension_hb->add_spacer();
	extension_hb->add_child(memnew(Label(TTR("Save Extension:"))));
	external_extension_type = memnew(OptionButton);
	extension_hb->add_child(external_extension_type);
	external_extension_type->add_item(TTR("Text: *.tres"));
	external_extension_type->add_item(TTR("Binary: *.res"));
	external_path_tree->set_hide_root(true);
	add_child(save_path);

	item_save_path = memnew(EditorFileDialog);
	item_save_path->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	item_save_path->add_filter("*.tres", TTR("Text Resource"));
	item_save_path->add_filter("*.res", TTR("Binary Resource"));
	add_child(item_save_path);
	item_save_path->connect("file_selected", callable_mp(this, &SceneImportSettingsDialog::_save_path_changed));

	save_path->connect("dir_selected", callable_mp(this, &SceneImportSettingsDialog::_save_dir_callback));

	update_view_timer = memnew(Timer);
	update_view_timer->set_wait_time(0.2);
	update_view_timer->connect("timeout", callable_mp(this, &SceneImportSettingsDialog::_update_view_gizmos));
	add_child(update_view_timer);
}

SceneImportSettingsDialog::~SceneImportSettingsDialog() {
	memdelete(scene_import_settings_data);
}
