/**************************************************************************/
/*  skeleton_3d_editor_plugin.h                                           */
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

#ifndef SKELETON_3D_EDITOR_PLUGIN_H
#define SKELETON_3D_EDITOR_PLUGIN_H

#include "editor/add_metadata_dialog.h"
#include "editor/editor_properties.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/resources/immediate_mesh.h"

class EditorInspectorPluginSkeleton;
class EditorPropertyVector3;
class Joint;
class PhysicalBone3D;
class Skeleton3DEditorPlugin;
class Button;
class Tree;
class TreeItem;
class VSeparator;

class BonePropertiesEditor : public VBoxContainer {
	GDCLASS(BonePropertiesEditor, VBoxContainer);

	EditorInspectorSection *section = nullptr;

	EditorPropertyCheck *enabled_checkbox = nullptr;
	EditorPropertyVector3 *position_property = nullptr;
	EditorPropertyQuaternion *rotation_property = nullptr;
	EditorPropertyVector3 *scale_property = nullptr;

	EditorInspectorSection *rest_section = nullptr;
	EditorPropertyTransform3D *rest_matrix = nullptr;

	EditorInspectorSection *meta_section = nullptr;
	AddMetadataDialog *add_meta_dialog = nullptr;
	Button *add_metadata_button = nullptr;

	Rect2 background_rects[5];

	Skeleton3D *skeleton = nullptr;
	// String property;

	bool toggle_enabled = false;
	bool updating = false;

	String label;

	void create_editors();

	void _value_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing);

	void _property_keyed(const String &p_path, bool p_advance);

	void _meta_changed(const String &p_property, const Variant &p_value, const String &p_name, bool p_changing);
	void _meta_deleted(const String &p_property);
	void _show_add_meta_dialog();
	void _add_meta_confirm();

	HashMap<StringName, EditorProperty *> meta_editors;

protected:
	void _notification(int p_what);

public:
	BonePropertiesEditor(Skeleton3D *p_skeleton);

	// Which transform target to modify.
	void set_target(const String &p_prop);
	void set_label(const String &p_label) { label = p_label; }
	void set_keyable(const bool p_keyable);

	void _update_properties();
};

class Skeleton3DEditor : public VBoxContainer {
	GDCLASS(Skeleton3DEditor, VBoxContainer);

	static void _bind_methods();

	friend class Skeleton3DEditorPlugin;

	enum SkeletonOption {
		SKELETON_OPTION_RESET_ALL_POSES,
		SKELETON_OPTION_RESET_SELECTED_POSES,
		SKELETON_OPTION_ALL_POSES_TO_RESTS,
		SKELETON_OPTION_SELECTED_POSES_TO_RESTS,
		SKELETON_OPTION_CREATE_PHYSICAL_SKELETON,
		SKELETON_OPTION_EXPORT_SKELETON_PROFILE,
	};

	struct BoneInfo {
		PhysicalBone3D *physical_bone = nullptr;
		Transform3D relative_rest; // Relative to skeleton node.
	};

	EditorInspectorPluginSkeleton *editor_plugin = nullptr;

	Skeleton3D *skeleton = nullptr;

	enum {
		JOINT_BUTTON_REVERT = 0,
	};

	Tree *joint_tree = nullptr;
	BonePropertiesEditor *rest_editor = nullptr;
	BonePropertiesEditor *pose_editor = nullptr;

	HBoxContainer *topmenu_bar = nullptr;
	MenuButton *skeleton_options = nullptr;
	Button *edit_mode_button = nullptr;

	bool edit_mode = false;

	HBoxContainer *animation_hb = nullptr;
	Button *key_loc_button = nullptr;
	Button *key_rot_button = nullptr;
	Button *key_scale_button = nullptr;
	Button *key_insert_button = nullptr;
	Button *key_insert_all_button = nullptr;

	EditorInspectorSection *bones_section = nullptr;

	EditorFileDialog *file_dialog = nullptr;

	bool keyable = false;

	static Skeleton3DEditor *singleton;

	void _on_click_skeleton_option(int p_skeleton_option);
	void _file_selected(const String &p_file);
	TreeItem *_find(TreeItem *p_node, const NodePath &p_path);
	void edit_mode_toggled(const bool pressed);

	EditorFileDialog *file_export_lib = nullptr;

	void update_joint_tree();
	void update_all();

	void create_editors();

	void reset_pose(const bool p_all_bones);
	void pose_to_rest(const bool p_all_bones);

	void insert_keys(const bool p_all_bones);

	void create_physical_skeleton();
	PhysicalBone3D *create_physical_bone(int bone_id, int bone_child_id, const Vector<BoneInfo> &bones_infos);

	void export_skeleton_profile();

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	void set_keyable(const bool p_keyable);
	void set_bone_options_enabled(const bool p_bone_options_enabled);

	// Handle.
	MeshInstance3D *handles_mesh_instance = nullptr;
	Ref<ImmediateMesh> handles_mesh;
	Ref<ShaderMaterial> handle_material;
	Ref<Shader> handle_shader;

	Vector3 bone_original_position;
	Quaternion bone_original_rotation;
	Vector3 bone_original_scale;

	void _update_gizmo_visible();
	void _bone_enabled_changed(const int p_bone_id);

	void _hide_handles();

	void _draw_gizmo();
	void _draw_handles();

	void _joint_tree_selection_changed();
	void _joint_tree_rmb_select(const Vector2 &p_pos, MouseButton p_button);
	void _joint_tree_button_clicked(Object *p_item, int p_column, int p_id, MouseButton p_button);
	void _update_properties();

	void _subgizmo_selection_change();

	int selected_bone = -1;

protected:
	void _notification(int p_what);
	void _node_removed(Node *p_node);

public:
	static Skeleton3DEditor *get_singleton() { return singleton; }

	void select_bone(int p_idx);

	int get_selected_bone() const;

	void move_skeleton_bone(NodePath p_skeleton_path, int32_t p_selected_boneidx, int32_t p_target_boneidx);

	Skeleton3D *get_skeleton() const { return skeleton; }

	bool is_edit_mode() const { return edit_mode; }

	void update_bone_original();
	Vector3 get_bone_original_position() const { return bone_original_position; }
	Quaternion get_bone_original_rotation() const { return bone_original_rotation; }
	Vector3 get_bone_original_scale() const { return bone_original_scale; }

	Skeleton3DEditor(EditorInspectorPluginSkeleton *e_plugin, Skeleton3D *skeleton);
	~Skeleton3DEditor();
};

class EditorInspectorPluginSkeleton : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginSkeleton, EditorInspectorPlugin);

	friend class Skeleton3DEditorPlugin;

	Skeleton3DEditor *skel_editor = nullptr;

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class Skeleton3DEditorPlugin : public EditorPlugin {
	GDCLASS(Skeleton3DEditorPlugin, EditorPlugin);

	EditorInspectorPluginSkeleton *skeleton_plugin = nullptr;

public:
	virtual EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;

	bool has_main_screen() const override { return false; }
	virtual bool handles(Object *p_object) const override;

	virtual String get_plugin_name() const override { return "Skeleton3D"; }

	Skeleton3DEditorPlugin();
};

class Skeleton3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(Skeleton3DGizmoPlugin, EditorNode3DGizmoPlugin);

	struct SelectionMaterials {
		Ref<StandardMaterial3D> unselected_mat;
		Ref<ShaderMaterial> selected_mat;
	};
	static SelectionMaterials selection_materials;

public:
	static Ref<ArrayMesh> get_bones_mesh(Skeleton3D *p_skeleton, int p_selected, bool p_is_selected);

	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;

	int subgizmos_intersect_ray(const EditorNode3DGizmo *p_gizmo, Camera3D *p_camera, const Vector2 &p_point) const override;
	Transform3D get_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id) const override;
	void set_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id, Transform3D p_transform) override;
	void commit_subgizmos(const EditorNode3DGizmo *p_gizmo, const Vector<int> &p_ids, const Vector<Transform3D> &p_restore, bool p_cancel) override;

	void redraw(EditorNode3DGizmo *p_gizmo) override;

	Skeleton3DGizmoPlugin();
	~Skeleton3DGizmoPlugin();
};

#endif // SKELETON_3D_EDITOR_PLUGIN_H
