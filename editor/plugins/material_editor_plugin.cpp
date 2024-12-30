/**************************************************************************/
/*  material_editor_plugin.cpp                                            */
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

#include "material_editor_plugin.h"

#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/themes/editor_scale.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/subviewport_container.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/fog_material.h"
#include "scene/resources/3d/sky_material.h"
#include "scene/resources/canvas_item_material.h"
#include "scene/resources/particle_process_material.h"

void MaterialEditor::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && (mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
		rot.x -= mm->get_relative().y * 0.01;
		rot.y -= mm->get_relative().x * 0.01;
		rot.x = CLAMP(rot.x, -Math_PI / 2, Math_PI / 2);
		_update_camera();
		_store_camera_metadata();
	}
}

void MaterialEditor::_update_theme_item_cache() {
	Control::_update_theme_item_cache();

	theme_cache.light_1_icon = get_editor_theme_icon(SNAME("MaterialPreviewLight1"));
	theme_cache.light_2_icon = get_editor_theme_icon(SNAME("MaterialPreviewLight2"));
	theme_cache.floor_icon = get_editor_theme_icon(SNAME("GuiMiniCheckerboard"));

	theme_cache.sphere_icon = get_editor_theme_icon(SNAME("MaterialPreviewSphere"));
	theme_cache.box_icon = get_editor_theme_icon(SNAME("MaterialPreviewCube"));
	theme_cache.quad_icon = get_editor_theme_icon(SNAME("MaterialPreviewQuad"));

	theme_cache.checkerboard = get_editor_theme_icon(SNAME("Checkerboard"));
}

void MaterialEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &MaterialEditor::_update_environment));
			_update_environment();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			light_1_switch->set_button_icon(theme_cache.light_1_icon);
			light_2_switch->set_button_icon(theme_cache.light_2_icon);
			floor_switch->set_button_icon(theme_cache.floor_icon);

			sphere_switch->set_button_icon(theme_cache.sphere_icon);
			box_switch->set_button_icon(theme_cache.box_icon);
			quad_switch->set_button_icon(theme_cache.quad_icon);

			error_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));

			default_floor_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, get_editor_theme_icon(SNAME("GuiMiniCheckerboard")));
		} break;

		case NOTIFICATION_DRAW: {
			if (!is_unsupported_shader_mode) {
				Size2 size = get_size();
				draw_texture_rect(theme_cache.checkerboard, Rect2(Point2(), size), true);
			}
		} break;
	}
}

// Store the rotation so it can persist when switching between materials.
void MaterialEditor::_store_camera_metadata() {
	Vector2 rotation_degrees = Vector2(Math::rad_to_deg(rot.x), Math::rad_to_deg(rot.y));
	EditorSettings::get_singleton()->set_project_metadata("inspector_options", "material_preview_rotation", rotation_degrees);
	EditorSettings::get_singleton()->set_project_metadata("inspector_options", "material_preview_zoom", cam_zoom);
	EditorSettings::get_singleton()->set_project_metadata("inspector_options", "material_preview_mesh", shape);
}

void MaterialEditor::_update_camera() {
	Transform3D xf;
	Vector3 center = contents_aabb.get_center();
	xf.basis = Basis(Vector3(0, 1, 0), rot.y) * Basis(Vector3(1, 0, 0), rot.x);
	xf.origin = center;
	xf.translate_local(0, 0, cam_zoom);
	camera->set_transform(xf);
}

void MaterialEditor::edit(Ref<Material> p_material, const Ref<Environment> &p_env) {
	material = p_material;
	viewport->get_world_3d()->set_fallback_environment(p_env);

	is_unsupported_shader_mode = false;
	if (material.is_valid()) {
		Shader::Mode mode = p_material->get_shader_mode();
		switch (mode) {
			case Shader::MODE_CANVAS_ITEM:
				layout_error->hide();
				layout_3d->hide();
				layout_2d->show();
				vc->hide();
				rect_instance->set_material(material);
				break;
			case Shader::MODE_SPATIAL:
				layout_error->hide();
				layout_2d->hide();
				layout_3d->show();
				vc->show();
				sphere_instance->set_material_override(material);
				box_instance->set_material_override(material);
				quad_instance->set_material_override(material);
				break;
			default:
				layout_error->show();
				layout_2d->hide();
				layout_3d->hide();
				vc->hide();
				is_unsupported_shader_mode = true;
				break;
		}
	} else {
		hide();
	}
}

void MaterialEditor::_on_visibility_switch_pressed(const int &p_shape) {
	switch ((Switch)p_shape) {
		case LIGHT_1: {
			light1->set_visible(light_1_switch->is_pressed());
			EditorSettings::get_singleton()->set_project_metadata("inspector_options", "material_preview_light1", light1->is_visible());
		} break;

		case LIGHT_2: {
			light2->set_visible(light_2_switch->is_pressed());
			EditorSettings::get_singleton()->set_project_metadata("inspector_options", "material_preview_light2", light2->is_visible());
		} break;

		case FLOOR: {
			bool is_visible = !floor_instance->is_visible();
			floor_instance->set_visible(is_visible);
			floor_switch->set_pressed(is_visible);
			EditorSettings::get_singleton()->set_project_metadata("inspector_options", "material_preview_floor", is_visible);
		} break;
	}
}

void MaterialEditor::_on_shape_switch_pressed(const int &p_shape) {
	sphere_instance->hide();
	box_instance->hide();
	quad_instance->hide();
	sphere_switch->set_pressed_no_signal(false);
	box_switch->set_pressed_no_signal(false);
	quad_switch->set_pressed_no_signal(false);

	float visible_height = 1.02;
	switch ((Shape)p_shape) {
		case SPHERE: {
			if (shape == SPHERE) {
				rot = Vector2(Math::deg_to_rad(-30.0), Math::deg_to_rad(0.0));
			}
			sphere_instance->show();
			sphere_switch->set_pressed_no_signal(true);
		} break;

		case BOX: {
			if (shape == BOX) {
				rot = Vector2(Math::deg_to_rad(-30.0), Math::deg_to_rad(20.0));
			}
			box_instance->show();
			box_switch->set_pressed_no_signal(true);
			visible_height = 1.55;
		} break;

		case QUAD: {
			if (shape == QUAD) {
				rot = Vector2(0.0, 0.0);
			}
			quad_instance->show();
			quad_switch->set_pressed_no_signal(true);
		}
	}

	shape = (Shape)p_shape;
	// FOV independent camera framing based on view height.
	cam_zoom = visible_height / Math::sin(Math::deg_to_rad(camera->get_fov()));
	_update_camera();
	_store_camera_metadata();
}

MaterialEditor::MaterialEditor() {
	// Canvas item

	vc_2d = memnew(SubViewportContainer);
	vc_2d->set_stretch(true);
	add_child(vc_2d);
	vc_2d->set_anchors_and_offsets_preset(PRESET_FULL_RECT);

	viewport_2d = memnew(SubViewport);
	vc_2d->add_child(viewport_2d);
	viewport_2d->set_disable_input(true);
	viewport_2d->set_transparent_background(true);

	layout_2d = memnew(HBoxContainer);
	layout_2d->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	viewport_2d->add_child(layout_2d);
	layout_2d->set_anchors_and_offsets_preset(PRESET_FULL_RECT);

	rect_instance = memnew(ColorRect);
	layout_2d->add_child(rect_instance);
	rect_instance->set_custom_minimum_size(Size2(150, 150) * EDSCALE);

	layout_2d->set_visible(false);

	layout_error = memnew(VBoxContainer);
	layout_error->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	layout_error->set_anchors_and_offsets_preset(PRESET_FULL_RECT);

	error_label = memnew(Label);
	error_label->set_text(TTR("Preview is not available for this shader mode."));
	error_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	error_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	error_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);

	layout_error->add_child(error_label);
	layout_error->hide();
	add_child(layout_error);

	// Spatial

	vc = memnew(SubViewportContainer);
	vc->set_stretch(true);
	add_child(vc);
	vc->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	viewport = memnew(SubViewport);
	Ref<World3D> world_3d;
	world_3d.instantiate();
	viewport->set_world_3d(world_3d); // Use own world.
	vc->add_child(viewport);
	viewport->set_disable_input(true);
	viewport->set_transparent_background(false);
	viewport->set_msaa_3d(Viewport::MSAA_4X);

	camera = memnew(Camera3D);
	camera->set_transform(Transform3D(Basis(), Vector3(0, 0.5, cam_zoom)));
	// Use low field of view so the sphere/box/quad is fully encompassed within the preview,
	// without much distortion.
	camera->set_perspective(20, 0.1, 10);
	camera->make_current();
	if (GLOBAL_GET("rendering/lights_and_shadows/use_physical_light_units")) {
		camera_attributes.instantiate();
		camera->set_attributes(camera_attributes);
	}
	viewport->add_child(camera);

	light1 = memnew(DirectionalLight3D);
	light1->set_transform(Transform3D().looking_at(Vector3(1, -1, -1), Vector3(0, 1, 0)));
	light1->set_shadow(true);
	viewport->add_child(light1);

	light2 = memnew(DirectionalLight3D);
	light2->set_transform(Transform3D().looking_at(Vector3(0, 1, 0), Vector3(0, 0, 1)));
	light2->set_color(Color(0.7, 0.7, 0.7));
	viewport->add_child(light2);

	rotation = memnew(Node3D);
	viewport->add_child(rotation);

	sphere_instance = memnew(MeshInstance3D);
	rotation->add_child(sphere_instance);

	box_instance = memnew(MeshInstance3D);
	rotation->add_child(box_instance);

	quad_instance = memnew(MeshInstance3D);
	rotation->add_child(quad_instance);

	floor_instance = memnew(MeshInstance3D);
	rotation->add_child(floor_instance);

	Transform3D transform = Transform3D();
	transform.origin = Vector3(0, 0.5, 0);
	sphere_instance->set_transform(transform);
	box_instance->set_transform(transform);
	quad_instance->set_transform(transform);

	sphere_mesh.instantiate();
	sphere_instance->set_mesh(sphere_mesh);
	box_mesh.instantiate();
	box_instance->set_mesh(box_mesh);
	quad_mesh.instantiate();
	quad_instance->set_mesh(quad_mesh);
	floor_mesh.instantiate();
	floor_mesh->set_size(Size2(10, 10));
	floor_instance->set_mesh(floor_mesh);

	contents_aabb = sphere_instance->get_transform().xform(sphere_mesh->get_aabb());

	default_floor_material = memnew(StandardMaterial3D);
	default_floor_material->set_uv1_scale(Vector3(20, 20, 20));
	default_floor_material->set_texture_filter(StandardMaterial3D::TEXTURE_FILTER_NEAREST);
	default_floor_material->set_albedo(Color::hex(0x454545ff));
	floor_mesh->set_material(default_floor_material);

	probe = memnew(ReflectionProbe);
	probe->set_size(Vector3(10, 1.5, 10));
	rotation->add_child(probe);
	probe->set_update_mode(ReflectionProbe::UPDATE_ALWAYS);
	probe->set_position(Vector3(0, 0.5, 0));

	set_custom_minimum_size(Size2(1, 150) * EDSCALE);

	layout_3d = memnew(HBoxContainer);
	add_child(layout_3d);
	layout_3d->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 2);

	VBoxContainer *vb_shape = memnew(VBoxContainer);
	layout_3d->add_child(vb_shape);

	sphere_switch = memnew(Button);
	sphere_switch->set_theme_type_variation("PreviewLightButton");
	sphere_switch->set_toggle_mode(true);
	vb_shape->add_child(sphere_switch);
	sphere_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_shape_switch_pressed).bind(SPHERE));

	box_switch = memnew(Button);
	box_switch->set_theme_type_variation("PreviewLightButton");
	box_switch->set_toggle_mode(true);
	vb_shape->add_child(box_switch);
	box_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_shape_switch_pressed).bind(BOX));

	quad_switch = memnew(Button);
	quad_switch->set_theme_type_variation("PreviewLightButton");
	quad_switch->set_toggle_mode(true);
	vb_shape->add_child(quad_switch);
	quad_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_shape_switch_pressed).bind(QUAD));

	layout_3d->add_spacer();

	VBoxContainer *vb_light = memnew(VBoxContainer);
	layout_3d->add_child(vb_light);

	light_1_switch = memnew(Button);
	light_1_switch->set_theme_type_variation("PreviewLightButton");
	light_1_switch->set_toggle_mode(true);
	light_1_switch->set_pressed(true);
	vb_light->add_child(light_1_switch);
	light_1_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_visibility_switch_pressed).bind(LIGHT_1));

	light_2_switch = memnew(Button);
	light_2_switch->set_theme_type_variation("PreviewLightButton");
	light_2_switch->set_toggle_mode(true);
	light_2_switch->set_pressed(true);
	vb_light->add_child(light_2_switch);
	light_2_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_visibility_switch_pressed).bind(LIGHT_2));

	floor_switch = memnew(Button);
	floor_switch->set_theme_type_variation("PreviewLightButton");
	floor_switch->set_toggle_mode(true);
	floor_switch->set_pressed(false);
	vb_light->add_child(floor_switch);
	floor_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_visibility_switch_pressed).bind(FLOOR));

	Vector2 stored_rot = EditorSettings::get_singleton()->get_project_metadata("inspector_options", "material_preview_rotation", Vector2(-30, 0));
	rot.x = Math::deg_to_rad(stored_rot.x);
	rot.y = Math::deg_to_rad(stored_rot.y);
	cam_zoom = EditorSettings::get_singleton()->get_project_metadata("inspector_options", "material_preview_zoom", 3.0);

	shape = (Shape)(int)EditorSettings::get_singleton()->get_project_metadata("inspector_options", "material_preview_mesh", SPHERE);
	_on_shape_switch_pressed(shape);

	light1->set_visible(EditorSettings::get_singleton()->get_project_metadata("inspector_options", "material_preview_light1", true));
	light_1_switch->set_pressed_no_signal(light1->is_visible());
	light2->set_visible(EditorSettings::get_singleton()->get_project_metadata("inspector_options", "material_preview_light2", false));
	light_2_switch->set_pressed_no_signal(light2->is_visible());

	bool floor_visible = EditorSettings::get_singleton()->get_project_metadata("inspector_options", "material_preview_floor", false);
	floor_instance->set_visible(floor_visible);
	floor_switch->set_pressed(floor_visible);

	_update_camera();
}

///////////////////////

bool EditorInspectorPluginMaterial::can_handle(Object *p_object) {
	Material *material = Object::cast_to<Material>(p_object);
	if (!material) {
		return false;
	}
	Shader::Mode mode = material->get_shader_mode();
	return mode == Shader::MODE_SPATIAL || mode == Shader::MODE_CANVAS_ITEM;
}

void EditorInspectorPluginMaterial::parse_begin(Object *p_object) {
	Material *material = Object::cast_to<Material>(p_object);
	if (!material) {
		return;
	}
	Ref<Material> m(material);

	MaterialEditor *editor = memnew(MaterialEditor);
	editor->edit(m, default_environment);
	add_custom_control(editor);
}

void EditorInspectorPluginMaterial::_undo_redo_inspector_callback(Object *p_undo_redo, Object *p_edited, const String &p_property, const Variant &p_new_value) {
	EditorUndoRedoManager *undo_redo = Object::cast_to<EditorUndoRedoManager>(p_undo_redo);
	ERR_FAIL_NULL(undo_redo);

	// For BaseMaterial3D, if a roughness or metallic textures is being assigned to an empty slot,
	// set the respective metallic or roughness factor to 1.0 as a convenience feature
	BaseMaterial3D *base_material = Object::cast_to<StandardMaterial3D>(p_edited);
	if (base_material) {
		Texture2D *texture = Object::cast_to<Texture2D>(p_new_value);
		if (texture) {
			if (p_property == "roughness_texture") {
				if (base_material->get_texture(StandardMaterial3D::TEXTURE_ROUGHNESS).is_null()) {
					undo_redo->add_do_property(p_edited, "roughness", 1.0);

					bool valid = false;
					Variant value = p_edited->get("roughness", &valid);
					if (valid) {
						undo_redo->add_undo_property(p_edited, "roughness", value);
					}
				}
			} else if (p_property == "metallic_texture") {
				if (base_material->get_texture(StandardMaterial3D::TEXTURE_METALLIC).is_null()) {
					undo_redo->add_do_property(p_edited, "metallic", 1.0);

					bool valid = false;
					Variant value = p_edited->get("metallic", &valid);
					if (valid) {
						undo_redo->add_undo_property(p_edited, "metallic", value);
					}
				}
			}
		}
	}
}

EditorInspectorPluginMaterial::EditorInspectorPluginMaterial() {
	default_environment.instantiate();
	Ref<Sky> sky = memnew(Sky());
	Ref<ProceduralSkyMaterial> sky_material;
	sky_material.instantiate();
	sky->set_material(sky_material);

	default_environment->set_sky(sky);
	default_environment->set_background(Environment::BG_SKY);
	default_environment->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
	default_environment->set_reflection_source(Environment::REFLECTION_SOURCE_SKY);
	default_environment->set_tonemapper(Environment::TONE_MAPPER_FILMIC);
	default_environment->set_glow_enabled(true);

	EditorNode::get_editor_data().add_undo_redo_inspector_hook_callback(callable_mp(this, &EditorInspectorPluginMaterial::_undo_redo_inspector_callback));
}

void MaterialEditor::_update_environment() {
	{
		Ref<Environment> env = camera->get_environment();
		String env_path = GLOBAL_GET("rendering/environment/material_preview/environment");
		env_path = ResourceUID::get_singleton()->ensure_path(env_path.strip_edges());

		if (!env_path.is_empty()) {
			String type = ResourceLoader::get_resource_type(env_path);
			if (!ClassDB::is_parent_class(type, "Environment")) {
				// Wrong type, set as empty.
				ProjectSettings::get_singleton()->set("rendering/environment/material_preview/environment", "");
			}
		}

		String cpath;
		Ref<Environment> fallback = viewport->get_world_3d()->get_fallback_environment();
		if (fallback.is_valid()) {
			cpath = fallback->get_path();
		}
		if (cpath != env_path) {
			if (!env_path.is_empty()) {
				fallback = ResourceLoader::load(env_path);
				if (fallback.is_null()) {
					// Could not load fallback, set as empty.
					ProjectSettings::get_singleton()->set("rendering/environment/material_preview/environment", "");
				}
			} else {
				fallback.unref();
			}
		}
		viewport->get_world_3d()->set_environment(fallback);
	}

	{
		String mat_path = GLOBAL_GET("rendering/environment/material_preview/floor_material");
		mat_path = ResourceUID::get_singleton()->ensure_path(mat_path.strip_edges());

		if (!mat_path.is_empty()) {
			String type = ResourceLoader::get_resource_type(mat_path);
			if (!ClassDB::is_parent_class(type, "BaseMaterial3D")) {
				// Wrong type, set as empty.
				ProjectSettings::get_singleton()->set("rendering/environment/material_preview/floor_material", "");
			}
		}

		String cpath;
		Ref<BaseMaterial3D> fallback_mat = floor_mesh->get_material();
		if (fallback_mat.is_valid()) {
			cpath = fallback_mat->get_path();
		}
		if (cpath != mat_path) {
			if (!mat_path.is_empty()) {
				fallback_mat = ResourceLoader::load(mat_path);
				if (fallback_mat.is_null()) {
					// Could not load fallback, set as empty.
					ProjectSettings::get_singleton()->set("rendering/environment/material_preview/floor_material", "");
					fallback_mat = default_floor_material;
				}
			} else {
				fallback_mat = default_floor_material;
			}
		}
		floor_mesh->set_material(fallback_mat);
	}
}

MaterialEditorPlugin::MaterialEditorPlugin() {
	Ref<EditorInspectorPluginMaterial> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}

String StandardMaterial3DConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool StandardMaterial3DConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<StandardMaterial3D> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> StandardMaterial3DConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	Ref<StandardMaterial3D> mat = p_resource;
	ERR_FAIL_COND_V(mat.is_null(), Ref<Resource>());

	Ref<ShaderMaterial> smat;
	smat.instantiate();

	Ref<Shader> shader;
	shader.instantiate();

	String code = RS::get_singleton()->shader_get_code(mat->get_shader_rid());

	shader->set_code(code);

	smat->set_shader(shader);

	List<PropertyInfo> params;
	RS::get_singleton()->get_shader_parameter_list(mat->get_shader_rid(), &params);

	for (const PropertyInfo &E : params) {
		// Texture parameter has to be treated specially since StandardMaterial3D saved it
		// as RID but ShaderMaterial needs Texture itself
		Ref<Texture2D> texture = mat->get_texture_by_name(E.name);
		if (texture.is_valid()) {
			smat->set_shader_parameter(E.name, texture);
		} else {
			Variant value = RS::get_singleton()->material_get_param(mat->get_rid(), E.name);
			smat->set_shader_parameter(E.name, value);
		}
	}

	smat->set_render_priority(mat->get_render_priority());
	smat->set_local_to_scene(mat->is_local_to_scene());
	smat->set_name(mat->get_name());
	return smat;
}

String ORMMaterial3DConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool ORMMaterial3DConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<ORMMaterial3D> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> ORMMaterial3DConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	Ref<ORMMaterial3D> mat = p_resource;
	ERR_FAIL_COND_V(mat.is_null(), Ref<Resource>());

	Ref<ShaderMaterial> smat;
	smat.instantiate();

	Ref<Shader> shader;
	shader.instantiate();

	String code = RS::get_singleton()->shader_get_code(mat->get_shader_rid());

	shader->set_code(code);

	smat->set_shader(shader);

	List<PropertyInfo> params;
	RS::get_singleton()->get_shader_parameter_list(mat->get_shader_rid(), &params);

	for (const PropertyInfo &E : params) {
		// Texture parameter has to be treated specially since ORMMaterial3D saved it
		// as RID but ShaderMaterial needs Texture itself
		Ref<Texture2D> texture = mat->get_texture_by_name(E.name);
		if (texture.is_valid()) {
			smat->set_shader_parameter(E.name, texture);
		} else {
			Variant value = RS::get_singleton()->material_get_param(mat->get_rid(), E.name);
			smat->set_shader_parameter(E.name, value);
		}
	}

	smat->set_render_priority(mat->get_render_priority());
	smat->set_local_to_scene(mat->is_local_to_scene());
	smat->set_name(mat->get_name());
	return smat;
}

String ParticleProcessMaterialConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool ParticleProcessMaterialConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<ParticleProcessMaterial> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> ParticleProcessMaterialConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	Ref<ParticleProcessMaterial> mat = p_resource;
	ERR_FAIL_COND_V(mat.is_null(), Ref<Resource>());

	Ref<ShaderMaterial> smat;
	smat.instantiate();

	Ref<Shader> shader;
	shader.instantiate();

	String code = RS::get_singleton()->shader_get_code(mat->get_shader_rid());

	shader->set_code(code);

	smat->set_shader(shader);

	List<PropertyInfo> params;
	RS::get_singleton()->get_shader_parameter_list(mat->get_shader_rid(), &params);

	for (const PropertyInfo &E : params) {
		Variant value = RS::get_singleton()->material_get_param(mat->get_rid(), E.name);
		smat->set_shader_parameter(E.name, value);
	}

	smat->set_render_priority(mat->get_render_priority());
	smat->set_local_to_scene(mat->is_local_to_scene());
	smat->set_name(mat->get_name());
	return smat;
}

String CanvasItemMaterialConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool CanvasItemMaterialConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<CanvasItemMaterial> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> CanvasItemMaterialConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	Ref<CanvasItemMaterial> mat = p_resource;
	ERR_FAIL_COND_V(mat.is_null(), Ref<Resource>());

	Ref<ShaderMaterial> smat;
	smat.instantiate();

	Ref<Shader> shader;
	shader.instantiate();

	String code = RS::get_singleton()->shader_get_code(mat->get_shader_rid());

	shader->set_code(code);

	smat->set_shader(shader);

	List<PropertyInfo> params;
	RS::get_singleton()->get_shader_parameter_list(mat->get_shader_rid(), &params);

	for (const PropertyInfo &E : params) {
		Variant value = RS::get_singleton()->material_get_param(mat->get_rid(), E.name);
		smat->set_shader_parameter(E.name, value);
	}

	smat->set_render_priority(mat->get_render_priority());
	smat->set_local_to_scene(mat->is_local_to_scene());
	smat->set_name(mat->get_name());
	return smat;
}

String ProceduralSkyMaterialConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool ProceduralSkyMaterialConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<ProceduralSkyMaterial> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> ProceduralSkyMaterialConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	Ref<ProceduralSkyMaterial> mat = p_resource;
	ERR_FAIL_COND_V(mat.is_null(), Ref<Resource>());

	Ref<ShaderMaterial> smat;
	smat.instantiate();

	Ref<Shader> shader;
	shader.instantiate();

	String code = RS::get_singleton()->shader_get_code(mat->get_shader_rid());

	shader->set_code(code);

	smat->set_shader(shader);

	List<PropertyInfo> params;
	RS::get_singleton()->get_shader_parameter_list(mat->get_shader_rid(), &params);

	for (const PropertyInfo &E : params) {
		Variant value = RS::get_singleton()->material_get_param(mat->get_rid(), E.name);
		smat->set_shader_parameter(E.name, value);
	}

	smat->set_render_priority(mat->get_render_priority());
	smat->set_local_to_scene(mat->is_local_to_scene());
	smat->set_name(mat->get_name());
	return smat;
}

String PanoramaSkyMaterialConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool PanoramaSkyMaterialConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<PanoramaSkyMaterial> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> PanoramaSkyMaterialConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	Ref<PanoramaSkyMaterial> mat = p_resource;
	ERR_FAIL_COND_V(mat.is_null(), Ref<Resource>());

	Ref<ShaderMaterial> smat;
	smat.instantiate();

	Ref<Shader> shader;
	shader.instantiate();

	String code = RS::get_singleton()->shader_get_code(mat->get_shader_rid());

	shader->set_code(code);

	smat->set_shader(shader);

	List<PropertyInfo> params;
	RS::get_singleton()->get_shader_parameter_list(mat->get_shader_rid(), &params);

	for (const PropertyInfo &E : params) {
		Variant value = RS::get_singleton()->material_get_param(mat->get_rid(), E.name);
		smat->set_shader_parameter(E.name, value);
	}

	smat->set_render_priority(mat->get_render_priority());
	smat->set_local_to_scene(mat->is_local_to_scene());
	smat->set_name(mat->get_name());
	return smat;
}

String PhysicalSkyMaterialConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool PhysicalSkyMaterialConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<PhysicalSkyMaterial> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> PhysicalSkyMaterialConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	Ref<PhysicalSkyMaterial> mat = p_resource;
	ERR_FAIL_COND_V(mat.is_null(), Ref<Resource>());

	Ref<ShaderMaterial> smat;
	smat.instantiate();

	Ref<Shader> shader;
	shader.instantiate();

	String code = RS::get_singleton()->shader_get_code(mat->get_shader_rid());

	shader->set_code(code);

	smat->set_shader(shader);

	List<PropertyInfo> params;
	RS::get_singleton()->get_shader_parameter_list(mat->get_shader_rid(), &params);

	for (const PropertyInfo &E : params) {
		Variant value = RS::get_singleton()->material_get_param(mat->get_rid(), E.name);
		smat->set_shader_parameter(E.name, value);
	}

	smat->set_render_priority(mat->get_render_priority());
	smat->set_local_to_scene(mat->is_local_to_scene());
	smat->set_name(mat->get_name());
	return smat;
}

String FogMaterialConversionPlugin::converts_to() const {
	return "ShaderMaterial";
}

bool FogMaterialConversionPlugin::handles(const Ref<Resource> &p_resource) const {
	Ref<FogMaterial> mat = p_resource;
	return mat.is_valid();
}

Ref<Resource> FogMaterialConversionPlugin::convert(const Ref<Resource> &p_resource) const {
	Ref<FogMaterial> mat = p_resource;
	ERR_FAIL_COND_V(mat.is_null(), Ref<Resource>());

	Ref<ShaderMaterial> smat;
	smat.instantiate();

	Ref<Shader> shader;
	shader.instantiate();

	String code = RS::get_singleton()->shader_get_code(mat->get_shader_rid());

	shader->set_code(code);

	smat->set_shader(shader);

	List<PropertyInfo> params;
	RS::get_singleton()->get_shader_parameter_list(mat->get_shader_rid(), &params);

	for (const PropertyInfo &E : params) {
		Variant value = RS::get_singleton()->material_get_param(mat->get_rid(), E.name);
		smat->set_shader_parameter(E.name, value);
	}

	smat->set_render_priority(mat->get_render_priority());
	return smat;
}
