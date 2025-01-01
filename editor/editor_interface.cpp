/**************************************************************************/
/*  editor_interface.cpp                                                  */
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

#include "editor_interface.h"
#include "editor_interface.compat.inc"

#include "core/config/project_settings.h"
#include "editor/create_dialog.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_feature_profile.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/filesystem_dock.h"
#include "editor/gui/editor_quick_open_dialog.h"
#include "editor/gui/editor_run_bar.h"
#include "editor/gui/editor_scene_tabs.h"
#include "editor/gui/editor_toaster.h"
#include "editor/gui/scene_tree_editor.h"
#include "editor/inspector_dock.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "editor/property_selector.h"
#include "editor/themes/editor_scale.h"
#include "main/main.h"
#include "plugins/editor_preview_plugins.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/control.h"
#include "scene/main/window.h"
#include "scene/resources/theme.h"

EditorInterface *EditorInterface::singleton = nullptr;

void EditorInterface::restart_editor(bool p_save) {
	if (p_save) {
		EditorNode::get_singleton()->save_all_scenes();
	}
	EditorNode::get_singleton()->restart_editor();
}

// Editor tools.

EditorCommandPalette *EditorInterface::get_command_palette() const {
	return EditorCommandPalette::get_singleton();
}

EditorFileSystem *EditorInterface::get_resource_file_system() const {
	return EditorFileSystem::get_singleton();
}

EditorPaths *EditorInterface::get_editor_paths() const {
	return EditorPaths::get_singleton();
}

EditorResourcePreview *EditorInterface::get_resource_previewer() const {
	return EditorResourcePreview::get_singleton();
}

EditorSelection *EditorInterface::get_selection() const {
	return EditorNode::get_singleton()->get_editor_selection();
}

Ref<EditorSettings> EditorInterface::get_editor_settings() const {
	return EditorSettings::get_singleton();
}

EditorToaster *EditorInterface::get_editor_toaster() const {
	return EditorToaster::get_singleton();
}

EditorUndoRedoManager *EditorInterface::get_editor_undo_redo() const {
	return EditorUndoRedoManager::get_singleton();
}

AABB EditorInterface::_calculate_aabb_for_scene(Node *p_node, AABB &p_scene_aabb) {
	MeshInstance3D *mesh_node = Object::cast_to<MeshInstance3D>(p_node);
	if (mesh_node && mesh_node->get_mesh().is_valid()) {
		Transform3D accum_xform;
		Node3D *base = mesh_node;
		while (base) {
			accum_xform = base->get_transform() * accum_xform;
			base = Object::cast_to<Node3D>(base->get_parent());
		}

		AABB aabb = accum_xform.xform(mesh_node->get_mesh()->get_aabb());
		p_scene_aabb.merge_with(aabb);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		p_scene_aabb = _calculate_aabb_for_scene(p_node->get_child(i), p_scene_aabb);
	}

	return p_scene_aabb;
}

TypedArray<Texture2D> EditorInterface::_make_mesh_previews(const TypedArray<Mesh> &p_meshes, int p_preview_size) {
	Vector<Ref<Mesh>> meshes;

	for (int i = 0; i < p_meshes.size(); i++) {
		meshes.push_back(p_meshes[i]);
	}

	Vector<Ref<Texture2D>> textures = make_mesh_previews(meshes, nullptr, p_preview_size);
	TypedArray<Texture2D> ret;
	for (int i = 0; i < textures.size(); i++) {
		ret.push_back(textures[i]);
	}

	return ret;
}

Vector<Ref<Texture2D>> EditorInterface::make_mesh_previews(const Vector<Ref<Mesh>> &p_meshes, Vector<Transform3D> *p_transforms, int p_preview_size) {
	int size = p_preview_size;

	RID scenario = RS::get_singleton()->scenario_create();

	RID viewport = RS::get_singleton()->viewport_create();
	RS::get_singleton()->viewport_set_update_mode(viewport, RS::VIEWPORT_UPDATE_ALWAYS);
	RS::get_singleton()->viewport_set_scenario(viewport, scenario);
	RS::get_singleton()->viewport_set_size(viewport, size, size);
	RS::get_singleton()->viewport_set_transparent_background(viewport, true);
	RS::get_singleton()->viewport_set_active(viewport, true);
	RID viewport_texture = RS::get_singleton()->viewport_get_texture(viewport);

	RID camera = RS::get_singleton()->camera_create();
	RS::get_singleton()->viewport_attach_camera(viewport, camera);

	RID light = RS::get_singleton()->directional_light_create();
	RID light_instance = RS::get_singleton()->instance_create2(light, scenario);

	RID light2 = RS::get_singleton()->directional_light_create();
	RS::get_singleton()->light_set_color(light2, Color(0.7, 0.7, 0.7));
	RID light_instance2 = RS::get_singleton()->instance_create2(light2, scenario);

	EditorProgress ep("mlib", TTR("Creating Mesh Previews"), p_meshes.size());

	Vector<Ref<Texture2D>> textures;

	for (int i = 0; i < p_meshes.size(); i++) {
		const Ref<Mesh> &mesh = p_meshes[i];
		if (mesh.is_null()) {
			textures.push_back(Ref<Texture2D>());
			continue;
		}

		Transform3D mesh_xform;
		if (p_transforms != nullptr) {
			mesh_xform = (*p_transforms)[i];
		}

		RID inst = RS::get_singleton()->instance_create2(mesh->get_rid(), scenario);
		RS::get_singleton()->instance_set_transform(inst, mesh_xform);

		AABB aabb = mesh->get_aabb();
		Vector3 ofs = aabb.get_center();
		aabb.position -= ofs;
		Transform3D xform;
		xform.basis = Basis().rotated(Vector3(0, 1, 0), -Math_PI / 6);
		xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 6) * xform.basis;
		AABB rot_aabb = xform.xform(aabb);
		float m = MAX(rot_aabb.size.x, rot_aabb.size.y) * 0.5;
		if (m == 0) {
			textures.push_back(Ref<Texture2D>());
			continue;
		}
		xform.origin = -xform.basis.xform(ofs); //-ofs*m;
		xform.origin.z -= rot_aabb.size.z * 2;
		xform.invert();
		xform = mesh_xform * xform;

		RS::get_singleton()->camera_set_transform(camera, xform * Transform3D(Basis(), Vector3(0, 0, 3)));
		RS::get_singleton()->camera_set_orthogonal(camera, m * 2, 0.01, 1000.0);

		RS::get_singleton()->instance_set_transform(light_instance, xform * Transform3D().looking_at(Vector3(-2, -1, -1), Vector3(0, 1, 0)));
		RS::get_singleton()->instance_set_transform(light_instance2, xform * Transform3D().looking_at(Vector3(+1, -1, -2), Vector3(0, 1, 0)));

		ep.step(TTR("Thumbnail..."), i);
		DisplayServer::get_singleton()->process_events();
		Main::iteration();
		Main::iteration();
		Ref<Image> img = RS::get_singleton()->texture_2d_get(viewport_texture);
		ERR_CONTINUE(img.is_null() || img->is_empty());
		Ref<ImageTexture> it = ImageTexture::create_from_image(img);

		RS::get_singleton()->free(inst);

		textures.push_back(it);
	}

	RS::get_singleton()->free(viewport);
	RS::get_singleton()->free(light);
	RS::get_singleton()->free(light_instance);
	RS::get_singleton()->free(light2);
	RS::get_singleton()->free(light_instance2);
	RS::get_singleton()->free(camera);
	RS::get_singleton()->free(scenario);

	return textures;
}

void EditorInterface::make_scene_preview(const String &p_path, Node *p_scene, int p_preview_size) {
	ERR_FAIL_NULL_MSG(p_scene, "The provided scene is null.");
	ERR_FAIL_COND_MSG(p_scene->is_inside_tree(), "The scene must not be inside the tree.");
	ERR_FAIL_COND_MSG(!Engine::get_singleton()->is_editor_hint(), "This function can only be called from the editor.");
	ERR_FAIL_NULL_MSG(EditorNode::get_singleton(), "EditorNode doesn't exist.");

	SubViewport *sub_viewport_node = memnew(SubViewport);
	AABB scene_aabb;
	scene_aabb = _calculate_aabb_for_scene(p_scene, scene_aabb);

	sub_viewport_node->set_update_mode(SubViewport::UPDATE_ALWAYS);
	sub_viewport_node->set_size(Vector2i(p_preview_size, p_preview_size));
	sub_viewport_node->set_transparent_background(false);
	Ref<World3D> world;
	world.instantiate();
	sub_viewport_node->set_world_3d(world);

	EditorNode::get_singleton()->add_child(sub_viewport_node);
	Ref<Environment> env;
	env.instantiate();
	env->set_background(Environment::BG_CLEAR_COLOR);

	Ref<CameraAttributesPractical> camera_attributes;
	camera_attributes.instantiate();

	Node3D *root = memnew(Node3D);
	root->set_name("Root");
	sub_viewport_node->add_child(root);

	Camera3D *camera = memnew(Camera3D);
	camera->set_environment(env);
	camera->set_attributes(camera_attributes);
	camera->set_name("Camera3D");
	root->add_child(camera);
	camera->set_current(true);

	camera->set_position(Vector3(0.0, 0.0, 3.0));

	DirectionalLight3D *light = memnew(DirectionalLight3D);
	light->set_name("Light");
	DirectionalLight3D *light2 = memnew(DirectionalLight3D);
	light2->set_name("Light2");
	light2->set_color(Color(0.7, 0.7, 0.7, 1.0));

	root->add_child(light);
	root->add_child(light2);

	sub_viewport_node->add_child(p_scene);

	// Calculate the camera and lighting position based on the size of the scene.
	Vector3 center = scene_aabb.get_center();
	float camera_size = scene_aabb.get_longest_axis_size();

	const float cam_rot_x = -Math_PI / 4;
	const float cam_rot_y = -Math_PI / 4;

	camera->set_orthogonal(camera_size * 2.0, 0.0001, camera_size * 2.0);

	Transform3D xf;
	xf.basis = Basis(Vector3(0, 1, 0), cam_rot_y) * Basis(Vector3(1, 0, 0), cam_rot_x);
	xf.origin = center;
	xf.translate_local(0, 0, camera_size);

	camera->set_transform(xf);

	Transform3D xform;
	xform.basis = Basis().rotated(Vector3(0, 1, 0), -Math_PI / 6);
	xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 6) * xform.basis;

	light->set_transform(xform * Transform3D().looking_at(Vector3(-2, -1, -1), Vector3(0, 1, 0)));
	light2->set_transform(xform * Transform3D().looking_at(Vector3(+1, -1, -2), Vector3(0, 1, 0)));

	// Update the renderer to get the screenshot.
	DisplayServer::get_singleton()->process_events();
	Main::iteration();
	Main::iteration();

	// Get the texture.
	Ref<Texture2D> texture = sub_viewport_node->get_texture();
	ERR_FAIL_COND_MSG(texture.is_null(), "Failed to get texture from sub_viewport_node.");

	// Remove the initial scene node.
	sub_viewport_node->remove_child(p_scene);

	// Cleanup the viewport.
	if (sub_viewport_node) {
		if (sub_viewport_node->get_parent()) {
			sub_viewport_node->get_parent()->remove_child(sub_viewport_node);
		}
		sub_viewport_node->queue_free();
		sub_viewport_node = nullptr;
	}

	// Now generate the cache image.
	Ref<Image> img = texture->get_image();
	if (img.is_valid() && img->get_width() > 0 && img->get_height() > 0) {
		img = img->duplicate();

		int preview_size = EDITOR_GET("filesystem/file_dialog/thumbnail_size");
		preview_size *= EDSCALE;

		int vp_size = MIN(img->get_width(), img->get_height());
		int x = (img->get_width() - vp_size) / 2;
		int y = (img->get_height() - vp_size) / 2;

		if (vp_size < preview_size) {
			img->crop_from_point(x, y, vp_size, vp_size);
		} else {
			int ratio = vp_size / preview_size;
			int size = preview_size * MAX(1, ratio / 2);

			x = (img->get_width() - size) / 2;
			y = (img->get_height() - size) / 2;

			img->crop_from_point(x, y, size, size);
			img->resize(preview_size, preview_size, Image::INTERPOLATE_LANCZOS);
		}
		img->convert(Image::FORMAT_RGB8);

		String temp_path = EditorPaths::get_singleton()->get_cache_dir();
		String cache_base = ProjectSettings::get_singleton()->globalize_path(p_path).md5_text();
		cache_base = temp_path.path_join("resthumb-" + cache_base);

		post_process_preview(img);
		img->save_png(cache_base + ".png");
	}

	EditorResourcePreview::get_singleton()->check_for_invalidation(p_path);
	EditorFileSystem::get_singleton()->emit_signal(SNAME("filesystem_changed"));
}

void EditorInterface::set_plugin_enabled(const String &p_plugin, bool p_enabled) {
	EditorNode::get_singleton()->set_addon_plugin_enabled(p_plugin, p_enabled, true);
}

bool EditorInterface::is_plugin_enabled(const String &p_plugin) const {
	return EditorNode::get_singleton()->is_addon_plugin_enabled(p_plugin);
}

// Editor GUI.

Ref<Theme> EditorInterface::get_editor_theme() const {
	return EditorNode::get_singleton()->get_editor_theme();
}

Control *EditorInterface::get_base_control() const {
	return EditorNode::get_singleton()->get_gui_base();
}

VBoxContainer *EditorInterface::get_editor_main_screen() const {
	return EditorNode::get_singleton()->get_editor_main_screen()->get_control();
}

ScriptEditor *EditorInterface::get_script_editor() const {
	return ScriptEditor::get_singleton();
}

SubViewport *EditorInterface::get_editor_viewport_2d() const {
	return EditorNode::get_singleton()->get_scene_root();
}

SubViewport *EditorInterface::get_editor_viewport_3d(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, static_cast<int>(Node3DEditor::VIEWPORTS_COUNT), nullptr);
	return Node3DEditor::get_singleton()->get_editor_viewport(p_idx)->get_viewport_node();
}

void EditorInterface::set_main_screen_editor(const String &p_name) {
	EditorNode::get_singleton()->get_editor_main_screen()->select_by_name(p_name);
}

void EditorInterface::set_distraction_free_mode(bool p_enter) {
	EditorNode::get_singleton()->set_distraction_free_mode(p_enter);
}

bool EditorInterface::is_distraction_free_mode_enabled() const {
	return EditorNode::get_singleton()->is_distraction_free_mode_enabled();
}

bool EditorInterface::is_multi_window_enabled() const {
	return EditorNode::get_singleton()->is_multi_window_enabled();
}

float EditorInterface::get_editor_scale() const {
	return EDSCALE;
}

void EditorInterface::popup_dialog(Window *p_dialog, const Rect2i &p_screen_rect) {
	p_dialog->popup_exclusive(EditorNode::get_singleton(), p_screen_rect);
}

void EditorInterface::popup_dialog_centered(Window *p_dialog, const Size2i &p_minsize) {
	p_dialog->popup_exclusive_centered(EditorNode::get_singleton(), p_minsize);
}

void EditorInterface::popup_dialog_centered_ratio(Window *p_dialog, float p_ratio) {
	p_dialog->popup_exclusive_centered_ratio(EditorNode::get_singleton(), p_ratio);
}

void EditorInterface::popup_dialog_centered_clamped(Window *p_dialog, const Size2i &p_size, float p_fallback_ratio) {
	p_dialog->popup_exclusive_centered_clamped(EditorNode::get_singleton(), p_size, p_fallback_ratio);
}

String EditorInterface::get_current_feature_profile() const {
	return EditorFeatureProfileManager::get_singleton()->get_current_profile_name();
}

void EditorInterface::set_current_feature_profile(const String &p_profile_name) {
	EditorFeatureProfileManager::get_singleton()->set_current_profile(p_profile_name, true);
}

// Editor dialogs.

void EditorInterface::popup_node_selector(const Callable &p_callback, const TypedArray<StringName> &p_valid_types, Node *p_current_value) {
	if (!node_selector) {
		node_selector = memnew(SceneTreeDialog);
		get_base_control()->add_child(node_selector);
	}

	Vector<StringName> valid_types;
	int length = p_valid_types.size();
	valid_types.resize(length);
	for (int i = 0; i < length; i++) {
		valid_types.write[i] = p_valid_types[i];
	}
	node_selector->set_valid_types(valid_types);
	node_selector->popup_scenetree_dialog(p_current_value);

	const Callable callback = callable_mp(this, &EditorInterface::_node_selected);
	node_selector->connect(SNAME("selected"), callback.bind(p_callback), CONNECT_DEFERRED);
	node_selector->connect(SNAME("canceled"), callback.bind(NodePath(), p_callback), CONNECT_DEFERRED);
}

void EditorInterface::popup_property_selector(Object *p_object, const Callable &p_callback, const PackedInt32Array &p_type_filter, const String &p_current_value) {
	if (!property_selector) {
		property_selector = memnew(PropertySelector);
		get_base_control()->add_child(property_selector);
	}

	Vector<Variant::Type> type_filter;
	int length = p_type_filter.size();
	type_filter.resize(length);
	for (int i = 0; i < length; i++) {
		type_filter.write[i] = (Variant::Type)p_type_filter[i];
	}
	property_selector->set_type_filter(type_filter);
	property_selector->select_property_from_instance(p_object, p_current_value);

	const Callable callback = callable_mp(this, &EditorInterface::_property_selected);
	property_selector->connect(SNAME("selected"), callback.bind(p_callback), CONNECT_DEFERRED);
	property_selector->connect(SNAME("canceled"), callback.bind(String(), p_callback), CONNECT_DEFERRED);
}

void EditorInterface::popup_method_selector(Object *p_object, const Callable &p_callback, const String &p_current_value) {
	if (!method_selector) {
		method_selector = memnew(PropertySelector);
		get_base_control()->add_child(method_selector);
	}

	method_selector->select_method_from_instance(p_object, p_current_value);

	const Callable callback = callable_mp(this, &EditorInterface::_method_selected);
	method_selector->connect(SNAME("selected"), callback.bind(p_callback), CONNECT_DEFERRED);
	method_selector->connect(SNAME("canceled"), callback.bind(String(), p_callback), CONNECT_DEFERRED);
}

void EditorInterface::popup_quick_open(const Callable &p_callback, const TypedArray<StringName> &p_base_types) {
	StringName required_type = SNAME("Resource");
	Vector<StringName> base_types;
	if (p_base_types.is_empty()) {
		base_types.append(required_type);
	} else {
		for (int i = 0; i < p_base_types.size(); i++) {
			StringName type = p_base_types[i];
			ERR_FAIL_COND_MSG(!(ClassDB::is_parent_class(type, required_type) || EditorNode::get_editor_data().script_class_is_parent(type, required_type)), "Only types deriving from Resource are supported in the quick open dialog.");
			base_types.append(type);
		}
	}

	EditorQuickOpenDialog *quick_open = EditorNode::get_singleton()->get_quick_open_dialog();
	quick_open->connect(SNAME("canceled"), callable_mp(this, &EditorInterface::_quick_open).bind(String(), p_callback));
	quick_open->popup_dialog(base_types, callable_mp(this, &EditorInterface::_quick_open).bind(p_callback));
}

void EditorInterface::popup_create_dialog(const Callable &p_callback, const StringName &p_base_type, const String &p_current_type, const String &p_dialog_title, const TypedArray<StringName> &p_custom_type_blocklist, const Dictionary &p_custom_suffix) {
	if (!create_dialog) {
		create_dialog = memnew(CreateDialog);
		get_base_control()->add_child(create_dialog);
	}

	HashSet<StringName> blocklist;
	for (const Variant &E : p_custom_type_blocklist) {
		blocklist.insert(E);
	}
	create_dialog->set_type_blocklist(blocklist);

	HashMap<StringName, String> suffix_map;
	List<Variant> keys;
	p_custom_suffix.get_key_list(&keys);
	for (Variant &k : keys) {
		const StringName key = k;
		if (key.is_empty()) {
			continue;
		}
		suffix_map.insert(key, p_custom_suffix[key]);
	}
	create_dialog->set_type_suffixes(suffix_map);

	String safe_base_type = p_base_type;
	if (p_base_type.is_empty() || (!ClassDB::class_exists(p_base_type) && !ScriptServer::is_global_class(p_base_type))) {
		ERR_PRINT(vformat("Invalid base type '%s'. The base type has fallen back to 'Object'.", p_base_type));
		safe_base_type = "Object";
	}

	create_dialog->set_base_type(safe_base_type);
	create_dialog->popup_create(false, true, p_current_type, "");
	create_dialog->set_title(p_dialog_title.is_empty() ? vformat(TTR("Create New %s"), p_base_type) : p_dialog_title);

	const Callable callback = callable_mp(this, &EditorInterface::_create_dialog_item_selected);
	create_dialog->connect(SNAME("create"), callback.bind(false, p_callback), CONNECT_DEFERRED);
	create_dialog->connect(SNAME("canceled"), callback.bind(true, p_callback), CONNECT_DEFERRED);
}

void EditorInterface::_node_selected(const NodePath &p_node_path, const Callable &p_callback) {
	const Callable callback = callable_mp(this, &EditorInterface::_node_selected);
	node_selector->disconnect(SNAME("selected"), callback);
	node_selector->disconnect(SNAME("canceled"), callback);

	if (p_node_path.is_empty()) {
		_call_dialog_callback(p_callback, NodePath(), "node selection canceled");
	} else {
		const NodePath path = get_edited_scene_root()->get_path().rel_path_to(p_node_path);
		_call_dialog_callback(p_callback, path, "node selected");
	}
}

void EditorInterface::_property_selected(const String &p_property_name, const Callable &p_callback) {
	const Callable callback = callable_mp(this, &EditorInterface::_property_selected);
	property_selector->disconnect(SNAME("selected"), callback);
	property_selector->disconnect(SNAME("canceled"), callback);

	if (p_property_name.is_empty()) {
		_call_dialog_callback(p_callback, NodePath(p_property_name).get_as_property_path(), "property selection canceled");
	} else {
		_call_dialog_callback(p_callback, NodePath(p_property_name).get_as_property_path(), "property selected");
	}
}

void EditorInterface::_method_selected(const String &p_method_name, const Callable &p_callback) {
	const Callable callback = callable_mp(this, &EditorInterface::_method_selected);
	method_selector->disconnect(SNAME("selected"), callback);
	method_selector->disconnect(SNAME("canceled"), callback);

	if (p_method_name.is_empty()) {
		_call_dialog_callback(p_callback, p_method_name, "method selection canceled");
	} else {
		_call_dialog_callback(p_callback, p_method_name, "method selected");
	}
}

void EditorInterface::_quick_open(const String &p_file_path, const Callable &p_callback) {
	EditorQuickOpenDialog *quick_open = EditorNode::get_singleton()->get_quick_open_dialog();
	quick_open->disconnect(SNAME("canceled"), callable_mp(this, &EditorInterface::_quick_open));
	_call_dialog_callback(p_callback, p_file_path, "quick open");
}

void EditorInterface::_create_dialog_item_selected(bool p_is_canceled, const Callable &p_callback) {
	const Callable callback = callable_mp(this, &EditorInterface::_create_dialog_item_selected);
	create_dialog->disconnect(SNAME("create"), callback);
	create_dialog->disconnect(SNAME("canceled"), callback);
	_call_dialog_callback(p_callback, p_is_canceled ? "" : create_dialog->get_selected_type(), "create dialog");
}

void EditorInterface::_call_dialog_callback(const Callable &p_callback, const Variant &p_selected, const String &p_context) {
	Callable::CallError ce;
	Variant ret;
	const Variant *args[1] = { &p_selected };
	p_callback.callp(args, 1, ret, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		ERR_PRINT(vformat("Error calling %s callback: %s", p_context, Variant::get_callable_error_text(p_callback, args, 1, ce)));
	}
}

// Editor docks.

FileSystemDock *EditorInterface::get_file_system_dock() const {
	return FileSystemDock::get_singleton();
}

void EditorInterface::select_file(const String &p_file) {
	FileSystemDock::get_singleton()->select_file(p_file);
}

Vector<String> EditorInterface::get_selected_paths() const {
	return FileSystemDock::get_singleton()->get_selected_paths();
}

String EditorInterface::get_current_path() const {
	return FileSystemDock::get_singleton()->get_current_path();
}

String EditorInterface::get_current_directory() const {
	return FileSystemDock::get_singleton()->get_current_directory();
}

EditorInspector *EditorInterface::get_inspector() const {
	return InspectorDock::get_inspector_singleton();
}

// Object/Resource/Node editing.

void EditorInterface::inspect_object(Object *p_obj, const String &p_for_property, bool p_inspector_only) {
	EditorNode::get_singleton()->push_item(p_obj, p_for_property, p_inspector_only);
}

void EditorInterface::edit_resource(const Ref<Resource> &p_resource) {
	EditorNode::get_singleton()->edit_resource(p_resource);
}

void EditorInterface::edit_node(Node *p_node) {
	EditorNode::get_singleton()->edit_node(p_node);
}

void EditorInterface::edit_script(const Ref<Script> &p_script, int p_line, int p_col, bool p_grab_focus) {
	ScriptEditor::get_singleton()->edit(p_script, p_line - 1, p_col - 1, p_grab_focus);
}

void EditorInterface::open_scene_from_path(const String &scene_path, bool p_set_inherited) {
	if (EditorNode::get_singleton()->is_changing_scene()) {
		return;
	}

	EditorNode::get_singleton()->open_request(scene_path, p_set_inherited);
}

void EditorInterface::reload_scene_from_path(const String &scene_path) {
	if (EditorNode::get_singleton()->is_changing_scene()) {
		return;
	}

	EditorNode::get_singleton()->reload_scene(scene_path);
}

Node *EditorInterface::get_edited_scene_root() const {
	return EditorNode::get_singleton()->get_edited_scene();
}

PackedStringArray EditorInterface::get_open_scenes() const {
	PackedStringArray ret;
	Vector<EditorData::EditedScene> scenes = EditorNode::get_editor_data().get_edited_scenes();

	int scns_amount = scenes.size();
	for (int idx_scn = 0; idx_scn < scns_amount; idx_scn++) {
		if (scenes[idx_scn].root == nullptr) {
			continue;
		}
		ret.push_back(scenes[idx_scn].root->get_scene_file_path());
	}
	return ret;
}

Error EditorInterface::save_scene() {
	if (!get_edited_scene_root()) {
		return ERR_CANT_CREATE;
	}
	if (get_edited_scene_root()->get_scene_file_path().is_empty()) {
		return ERR_CANT_CREATE;
	}

	save_scene_as(get_edited_scene_root()->get_scene_file_path());
	return OK;
}

void EditorInterface::save_scene_as(const String &p_scene, bool p_with_preview) {
	EditorNode::get_singleton()->save_scene_to_path(p_scene, p_with_preview);
}

void EditorInterface::mark_scene_as_unsaved() {
	EditorUndoRedoManager::get_singleton()->set_history_as_unsaved(EditorNode::get_editor_data().get_current_edited_scene_history_id());
	EditorSceneTabs::get_singleton()->update_scene_tabs();
}

void EditorInterface::save_all_scenes() {
	EditorNode::get_singleton()->save_all_scenes();
}

// Scene playback.

void EditorInterface::play_main_scene() {
	EditorRunBar::get_singleton()->play_main_scene();
}

void EditorInterface::play_current_scene() {
	EditorRunBar::get_singleton()->play_current_scene();
}

void EditorInterface::play_custom_scene(const String &scene_path) {
	EditorRunBar::get_singleton()->play_custom_scene(scene_path);
}

void EditorInterface::stop_playing_scene() {
	EditorRunBar::get_singleton()->stop_playing();
}

bool EditorInterface::is_playing_scene() const {
	return EditorRunBar::get_singleton()->is_playing();
}

String EditorInterface::get_playing_scene() const {
	return EditorRunBar::get_singleton()->get_playing_scene();
}

void EditorInterface::set_movie_maker_enabled(bool p_enabled) {
	EditorRunBar::get_singleton()->set_movie_maker_enabled(p_enabled);
}

bool EditorInterface::is_movie_maker_enabled() const {
	return EditorRunBar::get_singleton()->is_movie_maker_enabled();
}

#ifdef TOOLS_ENABLED
void EditorInterface::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0) {
		if (pf == "set_main_screen_editor") {
			for (String E : { "\"2D\"", "\"3D\"", "\"Script\"", "\"AssetLib\"" }) {
				r_options->push_back(E);
			}
		} else if (pf == "get_editor_viewport_3d") {
			for (uint32_t i = 0; i < Node3DEditor::VIEWPORTS_COUNT; i++) {
				r_options->push_back(String::num_int64(i));
			}
		}
	}
	Object::get_argument_options(p_function, p_idx, r_options);
}
#endif

// Base.

void EditorInterface::_bind_methods() {
	ClassDB::bind_method(D_METHOD("restart_editor", "save"), &EditorInterface::restart_editor, DEFVAL(true));

	// Editor tools.

	ClassDB::bind_method(D_METHOD("get_command_palette"), &EditorInterface::get_command_palette);
	ClassDB::bind_method(D_METHOD("get_resource_filesystem"), &EditorInterface::get_resource_file_system);
	ClassDB::bind_method(D_METHOD("get_editor_paths"), &EditorInterface::get_editor_paths);
	ClassDB::bind_method(D_METHOD("get_resource_previewer"), &EditorInterface::get_resource_previewer);
	ClassDB::bind_method(D_METHOD("get_selection"), &EditorInterface::get_selection);
	ClassDB::bind_method(D_METHOD("get_editor_settings"), &EditorInterface::get_editor_settings);
	ClassDB::bind_method(D_METHOD("get_editor_toaster"), &EditorInterface::get_editor_toaster);
	ClassDB::bind_method(D_METHOD("get_editor_undo_redo"), &EditorInterface::get_editor_undo_redo);

	ClassDB::bind_method(D_METHOD("make_mesh_previews", "meshes", "preview_size"), &EditorInterface::_make_mesh_previews);

	ClassDB::bind_method(D_METHOD("set_plugin_enabled", "plugin", "enabled"), &EditorInterface::set_plugin_enabled);
	ClassDB::bind_method(D_METHOD("is_plugin_enabled", "plugin"), &EditorInterface::is_plugin_enabled);

	// Editor GUI.

	ClassDB::bind_method(D_METHOD("get_editor_theme"), &EditorInterface::get_editor_theme);
	ClassDB::bind_method(D_METHOD("get_base_control"), &EditorInterface::get_base_control);
	ClassDB::bind_method(D_METHOD("get_editor_main_screen"), &EditorInterface::get_editor_main_screen);
	ClassDB::bind_method(D_METHOD("get_script_editor"), &EditorInterface::get_script_editor);
	ClassDB::bind_method(D_METHOD("get_editor_viewport_2d"), &EditorInterface::get_editor_viewport_2d);
	ClassDB::bind_method(D_METHOD("get_editor_viewport_3d", "idx"), &EditorInterface::get_editor_viewport_3d, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("set_main_screen_editor", "name"), &EditorInterface::set_main_screen_editor);
	ClassDB::bind_method(D_METHOD("set_distraction_free_mode", "enter"), &EditorInterface::set_distraction_free_mode);
	ClassDB::bind_method(D_METHOD("is_distraction_free_mode_enabled"), &EditorInterface::is_distraction_free_mode_enabled);
	ClassDB::bind_method(D_METHOD("is_multi_window_enabled"), &EditorInterface::is_multi_window_enabled);

	ClassDB::bind_method(D_METHOD("get_editor_scale"), &EditorInterface::get_editor_scale);

	ClassDB::bind_method(D_METHOD("popup_dialog", "dialog", "rect"), &EditorInterface::popup_dialog, DEFVAL(Rect2i()));
	ClassDB::bind_method(D_METHOD("popup_dialog_centered", "dialog", "minsize"), &EditorInterface::popup_dialog_centered, DEFVAL(Size2i()));
	ClassDB::bind_method(D_METHOD("popup_dialog_centered_ratio", "dialog", "ratio"), &EditorInterface::popup_dialog_centered_ratio, DEFVAL(0.8));
	ClassDB::bind_method(D_METHOD("popup_dialog_centered_clamped", "dialog", "minsize", "fallback_ratio"), &EditorInterface::popup_dialog_centered_clamped, DEFVAL(Size2i()), DEFVAL(0.75));

	ClassDB::bind_method(D_METHOD("get_current_feature_profile"), &EditorInterface::get_current_feature_profile);
	ClassDB::bind_method(D_METHOD("set_current_feature_profile", "profile_name"), &EditorInterface::set_current_feature_profile);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "distraction_free_mode"), "set_distraction_free_mode", "is_distraction_free_mode_enabled");

	// Editor dialogs.

	ClassDB::bind_method(D_METHOD("popup_node_selector", "callback", "valid_types", "current_value"), &EditorInterface::popup_node_selector, DEFVAL(TypedArray<StringName>()), DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("popup_property_selector", "object", "callback", "type_filter", "current_value"), &EditorInterface::popup_property_selector, DEFVAL(PackedInt32Array()), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("popup_method_selector", "object", "callback", "current_value"), &EditorInterface::popup_method_selector, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("popup_quick_open", "callback", "base_types"), &EditorInterface::popup_quick_open, DEFVAL(TypedArray<StringName>()));
	ClassDB::bind_method(D_METHOD("popup_create_dialog", "callback", "base_type", "current_type", "dialog_title", "type_blocklist", "type_suffixes"), &EditorInterface::popup_create_dialog, DEFVAL(""), DEFVAL(""), DEFVAL(""), DEFVAL(TypedArray<StringName>()), DEFVAL(Dictionary()));

	// Editor docks.

	ClassDB::bind_method(D_METHOD("get_file_system_dock"), &EditorInterface::get_file_system_dock);
	ClassDB::bind_method(D_METHOD("select_file", "file"), &EditorInterface::select_file);
	ClassDB::bind_method(D_METHOD("get_selected_paths"), &EditorInterface::get_selected_paths);
	ClassDB::bind_method(D_METHOD("get_current_path"), &EditorInterface::get_current_path);
	ClassDB::bind_method(D_METHOD("get_current_directory"), &EditorInterface::get_current_directory);

	ClassDB::bind_method(D_METHOD("get_inspector"), &EditorInterface::get_inspector);

	// Object/Resource/Node editing.

	ClassDB::bind_method(D_METHOD("inspect_object", "object", "for_property", "inspector_only"), &EditorInterface::inspect_object, DEFVAL(String()), DEFVAL(false));

	ClassDB::bind_method(D_METHOD("edit_resource", "resource"), &EditorInterface::edit_resource);
	ClassDB::bind_method(D_METHOD("edit_node", "node"), &EditorInterface::edit_node);
	ClassDB::bind_method(D_METHOD("edit_script", "script", "line", "column", "grab_focus"), &EditorInterface::edit_script, DEFVAL(-1), DEFVAL(0), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("open_scene_from_path", "scene_filepath", "set_inherited"), &EditorInterface::open_scene_from_path, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("reload_scene_from_path", "scene_filepath"), &EditorInterface::reload_scene_from_path);

	ClassDB::bind_method(D_METHOD("get_open_scenes"), &EditorInterface::get_open_scenes);
	ClassDB::bind_method(D_METHOD("get_edited_scene_root"), &EditorInterface::get_edited_scene_root);

	ClassDB::bind_method(D_METHOD("save_scene"), &EditorInterface::save_scene);
	ClassDB::bind_method(D_METHOD("save_scene_as", "path", "with_preview"), &EditorInterface::save_scene_as, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("save_all_scenes"), &EditorInterface::save_all_scenes);

	ClassDB::bind_method(D_METHOD("mark_scene_as_unsaved"), &EditorInterface::mark_scene_as_unsaved);

	// Scene playback.

	ClassDB::bind_method(D_METHOD("play_main_scene"), &EditorInterface::play_main_scene);
	ClassDB::bind_method(D_METHOD("play_current_scene"), &EditorInterface::play_current_scene);
	ClassDB::bind_method(D_METHOD("play_custom_scene", "scene_filepath"), &EditorInterface::play_custom_scene);
	ClassDB::bind_method(D_METHOD("stop_playing_scene"), &EditorInterface::stop_playing_scene);
	ClassDB::bind_method(D_METHOD("is_playing_scene"), &EditorInterface::is_playing_scene);
	ClassDB::bind_method(D_METHOD("get_playing_scene"), &EditorInterface::get_playing_scene);

	ClassDB::bind_method(D_METHOD("set_movie_maker_enabled", "enabled"), &EditorInterface::set_movie_maker_enabled);
	ClassDB::bind_method(D_METHOD("is_movie_maker_enabled"), &EditorInterface::is_movie_maker_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "movie_maker_enabled"), "set_movie_maker_enabled", "is_movie_maker_enabled");
}

void EditorInterface::create() {
	memnew(EditorInterface);
}

void EditorInterface::free() {
	ERR_FAIL_NULL(singleton);
	memdelete(singleton);
}

EditorInterface::EditorInterface() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
}
