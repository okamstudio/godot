/**************************************************************************/
/*  particles_editor_plugin.h                                             */
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

#ifndef PARTICLES_EDITOR_PLUGIN_H
#define PARTICLES_EDITOR_PLUGIN_H

#include "editor/plugins/editor_plugin.h"

class MarginContainer;
class LineEdit;
class CheckBox;
class ConfirmationDialog;
class EditorFileDialog;
class GPUParticles2D;
class HBoxContainer;
class MenuButton;
class OptionButton;
class SceneTreeDialog;
class SpinBox;

class ParticlesEditorPlugin : public EditorPlugin {
	GDCLASS(ParticlesEditorPlugin, EditorPlugin);

private:
	enum {
		MENU_OPTION_CONVERT,
		MENU_RESTART
	};

	HBoxContainer *toolbar = nullptr;
	MenuButton *menu = nullptr;

protected:
	String handled_type;
	String conversion_option_name;

	Node *edited_node = nullptr;

	void _notification(int p_what);

	bool need_show_lifetime_dialog(SpinBox *p_seconds);
	virtual void _menu_callback(int p_idx);

	virtual void _add_menu_options(PopupMenu *p_menu) {}
	virtual Node *_convert_particles() = 0;

public:
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	ParticlesEditorPlugin();
};

// 2D /////////////////////////////////////////////

class Particles2DEditorPlugin : public ParticlesEditorPlugin {
	GDCLASS(Particles2DEditorPlugin, ParticlesEditorPlugin);

protected:
	enum {
		MENU_LOAD_EMISSION_MASK = 100,
	};

	enum MaskMode {
		MASK_MODE_SOLID,
		MASK_MODE_BORDER,
	};

	enum DirectionMode {
		DIRECTION_MODE_NONE,
		DIRECTION_MODE_GENERATE,
		DIRECTION_MODE_TEXTURE,
	};

	enum TextureType {
		TEXTURE_TYPE_MASK,
		TEXTURE_TYPE_DIRECTION,
	};

	EditorFileDialog *file_dialog = nullptr;
	ConfirmationDialog *emission_mask_dialog = nullptr;
	OptionButton *emission_mask_mode = nullptr;
	OptionButton *emission_direction_mode = nullptr;
	CheckBox *emission_mask_centered = nullptr;
	CheckBox *emission_mask_colors = nullptr;
	LineEdit *mask_img_path_line_edit = nullptr;
	LineEdit *direction_img_path_line_edit = nullptr;
	HBoxContainer *direction_img_hbox = nullptr;
	Label *direction_img_label = nullptr;
	Button *mask_browse_button = nullptr;
	Button *direction_browse_button = nullptr;
	Label *error_message = nullptr;
	TextureType browsing_texture_type;

	virtual void _menu_callback(int p_idx) override;
	virtual void _add_menu_options(PopupMenu *p_menu) override;

	void _validate_textures();
	void _mask_img_path_line_edit_text_changed(const String &p_text);
	void _direction_img_path_line_edit_text_changed(const String &p_text);
	void _emission_mask_mode_item_changed(int p_idx) const;
	void _emission_direction_mode_item_changed(int p_idx);
	void _browse_mask_texture_pressed();
	void _browse_direction_texture_pressed();
	void _file_selected(const String &p_file);
	void _process_emission_masks(PackedVector2Array &r_valid_positions, PackedVector2Array &r_valid_normals, PackedByteArray &r_valid_colors, Vector2i &r_image_size);
	virtual void _generate_emission_mask() = 0;

public:
	Particles2DEditorPlugin();
};

class GPUParticles2DEditorPlugin : public Particles2DEditorPlugin {
	GDCLASS(GPUParticles2DEditorPlugin, Particles2DEditorPlugin);

	enum {
		MENU_GENERATE_VISIBILITY_RECT = 200,
	};

	List<GPUParticles2D *> selected_particles;

	ConfirmationDialog *generate_visibility_rect = nullptr;
	SpinBox *generate_seconds = nullptr;

	void _selection_changed();
	void _generate_visibility_rect();

protected:
	void _notification(int p_what);

	void _menu_callback(int p_idx) override;
	void _add_menu_options(PopupMenu *p_menu) override;

	Node *_convert_particles() override;

	void _generate_emission_mask() override;

public:
	GPUParticles2DEditorPlugin();
};

class CPUParticles2DEditorPlugin : public Particles2DEditorPlugin {
	GDCLASS(CPUParticles2DEditorPlugin, Particles2DEditorPlugin);

protected:
	void _notification(int p_what);
	Node *_convert_particles() override;

	void _generate_emission_mask() override;

public:
	CPUParticles2DEditorPlugin();
};

// 3D /////////////////////////////////////////////

class Particles3DEditorPlugin : public ParticlesEditorPlugin {
	GDCLASS(Particles3DEditorPlugin, ParticlesEditorPlugin);

	enum {
		MENU_OPTION_GENERATE_AABB = 300,
		MENU_OPTION_CREATE_EMISSION_VOLUME_FROM_NODE,
	};

	ConfirmationDialog *generate_aabb = nullptr;
	SpinBox *generate_seconds = nullptr;

	SceneTreeDialog *emission_tree_dialog = nullptr;
	ConfirmationDialog *emission_dialog = nullptr;
	SpinBox *emission_amount = nullptr;
	OptionButton *emission_fill = nullptr;

	void _generate_aabb();
	void _node_selected(const NodePath &p_path);

protected:
	Vector<Face3> geometry;

	virtual void _menu_callback(int p_idx) override;
	virtual void _add_menu_options(PopupMenu *p_menu) override;

	bool _generate(Vector<Vector3> &r_points, Vector<Vector3> &r_normals);
	virtual bool _can_generate_points() const = 0;
	virtual void _generate_emission_points() = 0;

public:
	Particles3DEditorPlugin();
};

class GPUParticles3DEditorPlugin : public Particles3DEditorPlugin {
	GDCLASS(GPUParticles3DEditorPlugin, Particles3DEditorPlugin);

protected:
	Node *_convert_particles() override;

	bool _can_generate_points() const override;
	void _generate_emission_points() override;

public:
	GPUParticles3DEditorPlugin();
};

class CPUParticles3DEditorPlugin : public Particles3DEditorPlugin {
	GDCLASS(CPUParticles3DEditorPlugin, Particles3DEditorPlugin);

protected:
	Node *_convert_particles() override;

	bool _can_generate_points() const override { return true; }
	void _generate_emission_points() override;

public:
	CPUParticles3DEditorPlugin();
};

#endif // PARTICLES_EDITOR_PLUGIN_H
