/**************************************************************************/
/*  material_editor_plugin.h                                              */
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

#ifndef MATERIAL_EDITOR_PLUGIN_H
#define MATERIAL_EDITOR_PLUGIN_H

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/editor_resource_conversion_plugin.h"
#include "scene/3d/reflection_probe.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/material.h"

class Camera3D;
class ColorRect;
class DirectionalLight3D;
class HBoxContainer;
class MeshInstance3D;
class SubViewport;
class SubViewportContainer;
class Button;
class Label;

class MaterialEditor : public Control {
	GDCLASS(MaterialEditor, Control);

	enum Shape {
		SPHERE,
		BOX,
		QUAD,
	};

	enum Switch {
		LIGHT_1,
		LIGHT_2,
		FLOOR,
	};

	SubViewportContainer *vc_2d = nullptr;
	SubViewport *viewport_2d = nullptr;
	HBoxContainer *layout_2d = nullptr;
	ColorRect *rect_instance = nullptr;

	SubViewportContainer *vc = nullptr;
	SubViewport *viewport = nullptr;
	Node3D *rotation = nullptr;
	MeshInstance3D *sphere_instance = nullptr;
	MeshInstance3D *box_instance = nullptr;
	MeshInstance3D *quad_instance = nullptr;
	MeshInstance3D *floor_instance = nullptr;
	DirectionalLight3D *light1 = nullptr;
	DirectionalLight3D *light2 = nullptr;
	ReflectionProbe *probe = nullptr;
	Camera3D *camera = nullptr;
	Ref<CameraAttributesPractical> camera_attributes;

	Ref<SphereMesh> sphere_mesh;
	Ref<BoxMesh> box_mesh;
	Ref<QuadMesh> quad_mesh;
	Ref<PlaneMesh> floor_mesh;

	VBoxContainer *layout_error = nullptr;
	Label *error_label = nullptr;
	bool is_unsupported_shader_mode = false;

	HBoxContainer *layout_3d = nullptr;

	Ref<Material> material;

	Button *sphere_switch = nullptr;
	Button *box_switch = nullptr;
	Button *quad_switch = nullptr;
	Button *light_1_switch = nullptr;
	Button *light_2_switch = nullptr;
	Button *floor_switch = nullptr;

	Shape shape;

	Vector2 rot;
	float cam_zoom = 3.0f;
	AABB contents_aabb;

	Ref<BaseMaterial3D> default_floor_material;

	struct ThemeCache {
		Ref<Texture2D> light_1_icon;
		Ref<Texture2D> light_2_icon;
		Ref<Texture2D> floor_icon;
		Ref<Texture2D> sphere_icon;
		Ref<Texture2D> box_icon;
		Ref<Texture2D> quad_icon;
		Ref<Texture2D> checkerboard;
	} theme_cache;

	void _on_visibility_switch_pressed(const int &p_shape);
	void _on_shape_switch_pressed(const int &p_shape);

	void _update_environment();

protected:
	virtual void _update_theme_item_cache() override;
	void _notification(int p_what);
	void gui_input(const Ref<InputEvent> &p_event) override;
	void _store_camera_metadata();
	void _update_camera();

public:
	void edit(Ref<Material> p_material, const Ref<Environment> &p_env);
	MaterialEditor();
};

class EditorInspectorPluginMaterial : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginMaterial, EditorInspectorPlugin);
	Ref<Environment> default_environment;

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;

	void _undo_redo_inspector_callback(Object *p_undo_redo, Object *p_edited, const String &p_property, const Variant &p_new_value);

	EditorInspectorPluginMaterial();
};

class MaterialEditorPlugin : public EditorPlugin {
	GDCLASS(MaterialEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "Material"; }

	MaterialEditorPlugin();
};

class StandardMaterial3DConversionPlugin : public EditorResourceConversionPlugin {
	GDCLASS(StandardMaterial3DConversionPlugin, EditorResourceConversionPlugin);

public:
	virtual String converts_to() const override;
	virtual bool handles(const Ref<Resource> &p_resource) const override;
	virtual Ref<Resource> convert(const Ref<Resource> &p_resource) const override;
};

class ORMMaterial3DConversionPlugin : public EditorResourceConversionPlugin {
	GDCLASS(ORMMaterial3DConversionPlugin, EditorResourceConversionPlugin);

public:
	virtual String converts_to() const override;
	virtual bool handles(const Ref<Resource> &p_resource) const override;
	virtual Ref<Resource> convert(const Ref<Resource> &p_resource) const override;
};

class ParticleProcessMaterialConversionPlugin : public EditorResourceConversionPlugin {
	GDCLASS(ParticleProcessMaterialConversionPlugin, EditorResourceConversionPlugin);

public:
	virtual String converts_to() const override;
	virtual bool handles(const Ref<Resource> &p_resource) const override;
	virtual Ref<Resource> convert(const Ref<Resource> &p_resource) const override;
};

class CanvasItemMaterialConversionPlugin : public EditorResourceConversionPlugin {
	GDCLASS(CanvasItemMaterialConversionPlugin, EditorResourceConversionPlugin);

public:
	virtual String converts_to() const override;
	virtual bool handles(const Ref<Resource> &p_resource) const override;
	virtual Ref<Resource> convert(const Ref<Resource> &p_resource) const override;
};

class ProceduralSkyMaterialConversionPlugin : public EditorResourceConversionPlugin {
	GDCLASS(ProceduralSkyMaterialConversionPlugin, EditorResourceConversionPlugin);

public:
	virtual String converts_to() const override;
	virtual bool handles(const Ref<Resource> &p_resource) const override;
	virtual Ref<Resource> convert(const Ref<Resource> &p_resource) const override;
};

class PanoramaSkyMaterialConversionPlugin : public EditorResourceConversionPlugin {
	GDCLASS(PanoramaSkyMaterialConversionPlugin, EditorResourceConversionPlugin);

public:
	virtual String converts_to() const override;
	virtual bool handles(const Ref<Resource> &p_resource) const override;
	virtual Ref<Resource> convert(const Ref<Resource> &p_resource) const override;
};

class PhysicalSkyMaterialConversionPlugin : public EditorResourceConversionPlugin {
	GDCLASS(PhysicalSkyMaterialConversionPlugin, EditorResourceConversionPlugin);

public:
	virtual String converts_to() const override;
	virtual bool handles(const Ref<Resource> &p_resource) const override;
	virtual Ref<Resource> convert(const Ref<Resource> &p_resource) const override;
};

class FogMaterialConversionPlugin : public EditorResourceConversionPlugin {
	GDCLASS(FogMaterialConversionPlugin, EditorResourceConversionPlugin);

public:
	virtual String converts_to() const override;
	virtual bool handles(const Ref<Resource> &p_resource) const override;
	virtual Ref<Resource> convert(const Ref<Resource> &p_resource) const override;
};

#endif // MATERIAL_EDITOR_PLUGIN_H
