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
		if (quad_instance->is_visible()) {
			// Clamp rotation so the quad is always visible.
			const real_t limit = Math::deg_to_rad(80.0);
			rot = rot.clampf(-limit, limit);
		} else {
			rot.x = CLAMP(rot.x, -Math_PI / 2, Math_PI / 2);
		}
		_update_rotation();
		_store_rotation_metadata();
	}
}

void MaterialEditor::_update_theme_item_cache() {
	Control::_update_theme_item_cache();

	theme_cache.light_1_icon = get_editor_theme_icon(SNAME("MaterialPreviewLight1"));
	theme_cache.light_2_icon = get_editor_theme_icon(SNAME("MaterialPreviewLight2"));

	theme_cache.sphere_icon = get_editor_theme_icon(SNAME("MaterialPreviewSphere"));
	theme_cache.box_icon = get_editor_theme_icon(SNAME("MaterialPreviewCube"));
	theme_cache.quad_icon = get_editor_theme_icon(SNAME("MaterialPreviewQuad"));

	theme_cache.checkerboard = get_editor_theme_icon(SNAME("Checkerboard"));
}

void MaterialEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			light_1_switch->set_button_icon(theme_cache.light_1_icon);
			light_2_switch->set_button_icon(theme_cache.light_2_icon);

			sphere_switch->set_button_icon(theme_cache.sphere_icon);
			box_switch->set_button_icon(theme_cache.box_icon);
			quad_switch->set_button_icon(theme_cache.quad_icon);

			error_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		} break;

		case NOTIFICATION_DRAW: {
			if (!is_unsupported_shader_mode) {
				Size2 size = get_size();
				draw_texture_rect(theme_cache.checkerboard, Rect2(Point2(), size), true);
			}
		} break;
	}
}

void MaterialEditor::_set_rotation(real_t p_x_degrees, real_t p_y_degrees) {
	rot.x = Math::deg_to_rad(p_x_degrees);
	rot.y = Math::deg_to_rad(p_y_degrees);
	_update_rotation();
}

// Store the rotation so it can persist when switching between materials.
void MaterialEditor::_store_rotation_metadata() {
	Vector2 rotation_degrees = Vector2(Math::rad_to_deg(rot.x), Math::rad_to_deg(rot.y));
	EditorSettings::get_singleton()->set_project_metadata("inspector_options", "material_preview_rotation", rotation_degrees);
}

void MaterialEditor::_update_rotation() {
	Transform3D t;
	t.basis.rotate(Vector3(0, 1, 0), -rot.y);
	t.basis.rotate(Vector3(1, 0, 0), -rot.x);
	rotation->set_transform(t);
}

void MaterialEditor::edit(Ref<Material> p_material, const Ref<Environment> &p_env) {
	material = p_material;
	camera->set_environment(p_env);

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

void MaterialEditor::_on_light_1_switch_pressed() {
	light1->set_visible(light_1_switch->is_pressed());
}

void MaterialEditor::_on_light_2_switch_pressed() {
	light2->set_visible(light_2_switch->is_pressed());
}

void MaterialEditor::_on_sphere_switch_pressed() {
	sphere_instance->show();
	box_instance->hide();
	quad_instance->hide();
	box_switch->set_pressed(false);
	quad_switch->set_pressed(false);
	_set_rotation(-15.0, 30.0);
	_store_rotation_metadata();
	EditorSettings::get_singleton()->set_project_metadata("inspector_options", "material_preview_mesh", "sphere");
}

void MaterialEditor::_on_box_switch_pressed() {
	sphere_instance->hide();
	box_instance->show();
	quad_instance->hide();
	sphere_switch->set_pressed(false);
	quad_switch->set_pressed(false);
	_set_rotation(-15.0, 30.0);
	_store_rotation_metadata();
	EditorSettings::get_singleton()->set_project_metadata("inspector_options", "material_preview_mesh", "box");
}

void MaterialEditor::_on_quad_switch_pressed() {
	sphere_instance->hide();
	box_instance->hide();
	quad_instance->show();
	sphere_switch->set_pressed(false);
	box_switch->set_pressed(false);
	_set_rotation(0.0, 0.0);
	_store_rotation_metadata();
	EditorSettings::get_singleton()->set_project_metadata("inspector_options", "material_preview_mesh", "quad");
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
	viewport->set_transparent_background(true);
	viewport->set_msaa_3d(Viewport::MSAA_4X);

	camera = memnew(Camera3D);
	camera->set_transform(Transform3D(Basis(), Vector3(0, 0, 1.1)));
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
	light1->set_transform(Transform3D().looking_at(Vector3(-1, -1, -1), Vector3(0, 1, 0)));
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

	sphere_instance->set_transform(Transform3D() * 0.375);
	box_instance->set_transform(Transform3D() * 0.25);
	quad_instance->set_transform(Transform3D() * 0.375);

	sphere_mesh.instantiate();
	sphere_instance->set_mesh(sphere_mesh);
	box_mesh.instantiate();
	box_instance->set_mesh(box_mesh);
	quad_mesh.instantiate();
	quad_instance->set_mesh(quad_mesh);

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
	sphere_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_sphere_switch_pressed));

	box_switch = memnew(Button);
	box_switch->set_theme_type_variation("PreviewLightButton");
	box_switch->set_toggle_mode(true);
	vb_shape->add_child(box_switch);
	box_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_box_switch_pressed));

	quad_switch = memnew(Button);
	quad_switch->set_theme_type_variation("PreviewLightButton");
	quad_switch->set_toggle_mode(true);
	vb_shape->add_child(quad_switch);
	quad_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_quad_switch_pressed));

	layout_3d->add_spacer();

	VBoxContainer *vb_light = memnew(VBoxContainer);
	layout_3d->add_child(vb_light);

	light_1_switch = memnew(Button);
	light_1_switch->set_theme_type_variation("PreviewLightButton");
	light_1_switch->set_toggle_mode(true);
	light_1_switch->set_pressed(true);
	vb_light->add_child(light_1_switch);
	light_1_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_light_1_switch_pressed));

	light_2_switch = memnew(Button);
	light_2_switch->set_theme_type_variation("PreviewLightButton");
	light_2_switch->set_toggle_mode(true);
	light_2_switch->set_pressed(true);
	vb_light->add_child(light_2_switch);
	light_2_switch->connect(SceneStringName(pressed), callable_mp(this, &MaterialEditor::_on_light_2_switch_pressed));

	String shape = EditorSettings::get_singleton()->get_project_metadata("inspector_options", "material_preview_mesh", "sphere");
	if (shape == "sphere") {
		box_instance->hide();
		quad_instance->hide();
		sphere_switch->set_pressed_no_signal(true);
	} else if (shape == "box") {
		sphere_instance->hide();
		quad_instance->hide();
		box_switch->set_pressed_no_signal(true);
	} else {
		sphere_instance->hide();
		box_instance->hide();
		quad_switch->set_pressed_no_signal(true);
	}

	Vector2 stored_rot = EditorSettings::get_singleton()->get_project_metadata("inspector_options", "material_preview_rotation", Vector2());
	_set_rotation(stored_rot.x, stored_rot.y);
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
	editor->edit(m, env);
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
	env.instantiate();
	Ref<Sky> sky = memnew(Sky());
	env->set_sky(sky);
	env->set_background(Environment::BG_COLOR);
	env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
	env->set_reflection_source(Environment::REFLECTION_SOURCE_SKY);

	EditorNode::get_editor_data().add_undo_redo_inspector_hook_callback(callable_mp(this, &EditorInspectorPluginMaterial::_undo_redo_inspector_callback));
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
