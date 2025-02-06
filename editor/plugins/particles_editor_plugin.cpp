/**************************************************************************/
/*  particles_editor_plugin.cpp                                           */
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

#include "particles_editor_plugin.h"

#include "canvas_item_editor_plugin.h"
#include "core/io/image_loader.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/scene_tree_dock.h"
#include "scene/2d/cpu_particles_2d.h"
#include "scene/2d/gpu_particles_2d.h"
#include "scene/3d/cpu_particles_3d.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/spin_box.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/particle_process_material.h"

void ParticlesEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (handled_type.ends_with("2D")) {
				add_control_to_container(CONTAINER_CANVAS_EDITOR_MENU, toolbar);
			} else if (handled_type.ends_with("3D")) {
				add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, toolbar);
			} else {
				DEV_ASSERT(false);
			}

			menu->set_button_icon(menu->get_editor_theme_icon(handled_type));
			menu->set_text(handled_type);

			PopupMenu *popup = menu->get_popup();
			popup->add_shortcut(ED_SHORTCUT("particles/restart_emission", TTRC("Restart Emission"), KeyModifierMask::CTRL | Key::R), MENU_RESTART);
			_add_menu_options(popup);
			popup->add_item(conversion_option_name, MENU_OPTION_CONVERT);
		} break;
	}
}

bool ParticlesEditorPlugin::need_show_lifetime_dialog(SpinBox *p_seconds) {
	// Add one second to the default generation lifetime, since the progress is updated every second.
	p_seconds->set_value(MAX(1.0, trunc(edited_node->get("lifetime").operator double()) + 1.0));

	if (p_seconds->get_value() >= 11.0 + CMP_EPSILON) {
		// Only pop up the time dialog if the particle's lifetime is long enough to warrant shortening it.
		return true;
	} else {
		// Generate the visibility rect/AABB immediately.
		return false;
	}
}

void ParticlesEditorPlugin::_menu_callback(int p_idx) {
	switch (p_idx) {
		case MENU_OPTION_CONVERT: {
			Node *converted_node = _convert_particles();

			EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
			ur->create_action(conversion_option_name, UndoRedo::MERGE_DISABLE, edited_node);
			SceneTreeDock::get_singleton()->replace_node(edited_node, converted_node);
			ur->commit_action(false);
		} break;

		case MENU_RESTART: {
			edited_node->call("restart");
		}
	}
}

void ParticlesEditorPlugin::edit(Object *p_object) {
	edited_node = Object::cast_to<Node>(p_object);
}

bool ParticlesEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class(handled_type);
}

void ParticlesEditorPlugin::make_visible(bool p_visible) {
	toolbar->set_visible(p_visible);
}

ParticlesEditorPlugin::ParticlesEditorPlugin() {
	toolbar = memnew(HBoxContainer);
	toolbar->hide();

	menu = memnew(MenuButton);
	menu->set_switch_on_hover(true);
	toolbar->add_child(menu);
	menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &ParticlesEditorPlugin::_menu_callback));
}

// 2D /////////////////////////////////////////////

void GPUParticles2DEditorPlugin::_menu_callback(int p_idx) {
	if (p_idx == MENU_GENERATE_VISIBILITY_RECT) {
		if (need_show_lifetime_dialog(generate_seconds)) {
			generate_visibility_rect->popup_centered();
		} else {
			_generate_visibility_rect();
		}
	} else {
		Particles2DEditorPlugin::_menu_callback(p_idx);
	}
}

void GPUParticles2DEditorPlugin::_add_menu_options(PopupMenu *p_menu) {
	Particles2DEditorPlugin::_add_menu_options(p_menu);
	p_menu->add_item(TTR("Generate Visibility Rect"), MENU_GENERATE_VISIBILITY_RECT);
}

void Particles2DEditorPlugin::_browse_mask_texture_pressed() {
	browsing_texture_type = TEXTURE_TYPE_MASK;
	file_dialog->popup_file_dialog();
}

void Particles2DEditorPlugin::_browse_direction_texture_pressed() {
	browsing_texture_type = TEXTURE_TYPE_DIRECTION;
	file_dialog->popup_centered();
}

void Particles2DEditorPlugin::_file_selected(const String &p_file) {
	switch (browsing_texture_type) {
		case TEXTURE_TYPE_MASK: {
			mask_img_path_line_edit->set_text(p_file);
			break;
		}
		case TEXTURE_TYPE_DIRECTION: {
			direction_img_path_line_edit->set_text(p_file);
			break;
		}
	}

	_validate_textures();
}

void Particles2DEditorPlugin::_process_emission_masks(PackedVector2Array &r_valid_positions, PackedVector2Array &r_valid_normals, PackedByteArray &r_valid_colors, Vector2i &r_image_size) {
	Ref<Image> mask_img;
	mask_img.instantiate();
	Error err = ImageLoader::load_image(mask_img_path_line_edit->get_text(), mask_img);
	ERR_FAIL_COND_MSG(err != OK, vformat("Error loading image '%s'.", mask_img_path_line_edit->get_text()));

	if (mask_img->is_compressed()) {
		mask_img->decompress();
	}
	mask_img->convert(Image::FORMAT_RGBA8);
	ERR_FAIL_COND(mask_img->get_format() != Image::FORMAT_RGBA8);
	Size2i mask_img_size = mask_img->get_size();
	ERR_FAIL_COND(mask_img_size.width == 0 || mask_img_size.height == 0);

	r_image_size = mask_img_size;

	r_valid_positions.resize(mask_img_size.width * mask_img_size.height);

	MaskMode emission_mode = static_cast<MaskMode>(emission_mask_mode->get_selected());
	DirectionMode direction_mode = static_cast<DirectionMode>(emission_direction_mode->get_selected());

	if (direction_mode != DIRECTION_MODE_NONE) {
		r_valid_normals.resize(mask_img_size.width * mask_img_size.height);
	}

	bool capture_colors = emission_mask_colors->is_pressed();

	if (capture_colors) {
		r_valid_colors.resize(mask_img_size.width * mask_img_size.height * 4);
	}

	int valid_point_count = 0;

	{
		Vector<uint8_t> mask_img_data = mask_img->get_data();
		const uint8_t *mask_img_ptr = mask_img_data.ptr();

		for (int mask_img_x = 0; mask_img_x < mask_img_size.width; mask_img_x++) {
			for (int mask_img_y = 0; mask_img_y < mask_img_size.height; mask_img_y++) {
				uint8_t mask_alpha = mask_img_ptr[(mask_img_y * mask_img_size.width + mask_img_x) * 4 + 3];

				if (mask_alpha <= 128) {
					continue;
				}

				if (emission_mode == MASK_MODE_SOLID) {
					r_valid_positions.write[valid_point_count++] = Point2(mask_img_x, mask_img_y);
				} else {
					bool pixel_is_on_border = false;
					for (int x = mask_img_x - 1; x <= mask_img_x + 1; x++) {
						for (int y = mask_img_y - 1; y <= mask_img_y + 1; y++) {
							if (x < 0 || y < 0 || x >= mask_img_size.width || y >= mask_img_size.height || mask_img_ptr[(y * mask_img_size.width + x) * 4 + 3] <= 128) {
								pixel_is_on_border = true;
								break;
							}
						}

						if (pixel_is_on_border) {
							break;
						}
					}

					if (!pixel_is_on_border) {
						continue;
					}

					r_valid_positions.write[valid_point_count] = Point2(mask_img_x, mask_img_y);

					if (direction_mode == DIRECTION_MODE_GENERATE) {
						Vector2 normal;
						for (int x = mask_img_x - 2; x <= mask_img_x + 2; x++) {
							for (int y = mask_img_y - 2; y <= mask_img_y + 2; y++) {
								if (x == mask_img_x && y == mask_img_y) {
									continue;
								}

								if (x < 0 || y < 0 || x >= mask_img_size.width || y >= mask_img_size.height || mask_img_ptr[(y * mask_img_size.width + x) * 4 + 3] <= 128) {
									normal += Vector2(x - mask_img_x, y - mask_img_y).normalized();
								}
							}
						}

						normal.normalize();
						r_valid_normals.write[valid_point_count] = normal;
					}

					valid_point_count++;
				}
			}
		}

		if (capture_colors) {
			for (int i = 0; i < valid_point_count; ++i) {
				const Point2i point = r_valid_positions.get(i);
				r_valid_colors.write[i * 4 + 0] = mask_img_ptr[(point.y * mask_img_size.width + point.x) * 4 + 0];
				r_valid_colors.write[i * 4 + 1] = mask_img_ptr[(point.y * mask_img_size.width + point.x) * 4 + 1];
				r_valid_colors.write[i * 4 + 2] = mask_img_ptr[(point.y * mask_img_size.width + point.x) * 4 + 2];
				r_valid_colors.write[i * 4 + 3] = mask_img_ptr[(point.y * mask_img_size.width + point.x) * 4 + 3];
			}
		}
	}

	if (direction_mode == DIRECTION_MODE_TEXTURE) {
		Ref<Image> normal_img;
		normal_img.instantiate();
		err = ImageLoader::load_image(direction_img_path_line_edit->get_text(), normal_img);
		ERR_FAIL_COND_MSG(err != OK, vformat("Error loading image '%s'.", direction_img_path_line_edit->get_text()));

		if (normal_img->is_compressed()) {
			normal_img->decompress();
		}
		normal_img->convert(Image::FORMAT_RGB8);
		ERR_FAIL_COND(normal_img->get_format() != Image::FORMAT_RGB8);
		Size2i normal_img_size = normal_img->get_size();
		ERR_FAIL_COND(normal_img_size.width == 0 || normal_img_size.height == 0);
		ERR_FAIL_COND_MSG(normal_img_size != mask_img_size, "Mask and Normal texture must have the same size.");

		Vector<uint8_t> normal_img_data = normal_img->get_data();
		const uint8_t *normal_img_ptr = normal_img_data.ptr();

		for (int i = 0; i < valid_point_count; ++i) {
			const Point2i point = r_valid_positions.get(i);
			const uint8_t normal_r = normal_img_ptr[(point.y * normal_img_size.width + point.x) * 3 + 0];
			const uint8_t normal_g = normal_img_ptr[(point.y * normal_img_size.width + point.x) * 3 + 1];

			Vector2 normal;
			normal.x = static_cast<float>(normal_r) / 255.0f - 0.5f;
			normal.y = static_cast<float>(normal_g) / 255.0f - 0.5f;

			normal.normalize();

			r_valid_normals.write[i] = normal;
		}
	}

	r_valid_positions.resize(valid_point_count);
	if (!r_valid_normals.is_empty()) {
		r_valid_normals.resize(valid_point_count);
	}
}

Particles2DEditorPlugin::Particles2DEditorPlugin() {
	file_dialog = memnew(EditorFileDialog);

	List<String> ext;
	ImageLoader::get_recognized_extensions(&ext);
	for (const String &E : ext) {
		file_dialog->add_filter("*." + E, E.to_upper());
	}

	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	file_dialog->connect("file_selected", callable_mp(this, &Particles2DEditorPlugin::_file_selected));

	emission_mask_dialog = memnew(ConfirmationDialog);
	emission_mask_dialog->set_title(TTR("Load Emission Mask"));
	emission_mask_dialog->add_child(file_dialog);
	emission_mask_dialog->get_ok_button()->set_disabled(true);

	VBoxContainer *emvb = memnew(VBoxContainer);
	emission_mask_dialog->add_child(emvb);

	HBoxContainer *mask_img_hbox = memnew(HBoxContainer);

	mask_img_path_line_edit = memnew(LineEdit);
	mask_img_hbox->add_child(mask_img_path_line_edit);
	mask_img_path_line_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	mask_img_path_line_edit->set_editable(false);
	mask_img_path_line_edit->set_placeholder(vformat(TTR("Mask texture path")));
	mask_img_path_line_edit->connect(SceneStringName(text_changed), callable_mp(this, &Particles2DEditorPlugin::_mask_img_path_line_edit_text_changed));

	mask_browse_button = memnew(Button);
	mask_img_hbox->add_child(mask_browse_button);
	mask_browse_button->connect(SceneStringName(pressed), callable_mp(this, &Particles2DEditorPlugin::_browse_mask_texture_pressed));
	emvb->add_margin_child(TTR("Mask Texture"), mask_img_hbox);

	emission_mask_mode = memnew(OptionButton);
	emission_mask_mode->add_item(TTR("Solid Pixels"), MASK_MODE_SOLID);
	emission_mask_mode->add_item(TTR("Border Pixels"), MASK_MODE_BORDER);
	emission_mask_mode->connect(SceneStringName(item_selected), callable_mp(this, &Particles2DEditorPlugin::_emission_mask_mode_item_changed));
	emvb->add_margin_child(TTR("Mask Mode"), emission_mask_mode);

	emission_direction_mode = memnew(OptionButton);
	emission_direction_mode->add_item(TTR("None"), DIRECTION_MODE_NONE);
	emission_direction_mode->add_item(TTR("Generate"), DIRECTION_MODE_GENERATE);
	emission_direction_mode->add_item(TTR("Texture"), DIRECTION_MODE_TEXTURE);
	emission_direction_mode->connect(SceneStringName(item_selected), callable_mp(this, &Particles2DEditorPlugin::_emission_direction_mode_item_changed));
	emission_direction_mode->set_item_disabled(DIRECTION_MODE_GENERATE, true);
	emvb->add_margin_child(TTR("Direction Mode"), emission_direction_mode);

	direction_img_label = memnew(Label);
	direction_img_label->set_text(TTR("Direction Texture"));
	direction_img_label->set_theme_type_variation("HeaderSmall");
	emvb->add_child(direction_img_label);
	direction_img_label->hide();

	direction_img_hbox = memnew(HBoxContainer);
	direction_img_path_line_edit = memnew(LineEdit);
	direction_img_hbox->add_child(direction_img_path_line_edit);
	direction_img_path_line_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	direction_img_path_line_edit->set_editable(false);
	direction_img_path_line_edit->set_placeholder(vformat(TTR("Direction texture path")));
	direction_img_path_line_edit->connect(SceneStringName(text_changed), callable_mp(this, &Particles2DEditorPlugin::_direction_img_path_line_edit_text_changed));

	direction_browse_button = memnew(Button);
	direction_img_hbox->add_child(direction_browse_button);
	direction_browse_button->connect(SceneStringName(pressed), callable_mp(this, &Particles2DEditorPlugin::_browse_direction_texture_pressed));
	emvb->add_child(direction_img_hbox);
	direction_img_hbox->hide();

	VBoxContainer *optionsvb = memnew(VBoxContainer);
	emvb->add_margin_child(TTR("Options"), optionsvb);

	emission_mask_centered = memnew(CheckBox(TTR("Centered")));
	emission_mask_centered->set_pressed(true);
	optionsvb->add_child(emission_mask_centered);
	emission_mask_colors = memnew(CheckBox(TTR("Copy Color from Mask Texture")));
	optionsvb->add_child(emission_mask_colors);

	error_message = memnew(Label);
	error_message->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	error_message->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	error_message->add_theme_color_override(SceneStringName(font_color), EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("error_color"), EditorStringName(Editor)));
	emvb->add_child(error_message);

	EditorNode::get_singleton()->get_gui_base()->add_child(emission_mask_dialog);

	emission_mask_dialog->connect(SceneStringName(confirmed), callable_mp(this, &Particles2DEditorPlugin::_generate_emission_mask));
}

void GPUParticles2DEditorPlugin::_selection_changed() {
	List<Node *> selected_nodes = EditorNode::get_singleton()->get_editor_selection()->get_selected_node_list();
	if (selected_particles.is_empty() && selected_nodes.is_empty()) {
		return;
	}

	for (GPUParticles2D *particles : selected_particles) {
		particles->set_show_visibility_rect(false);
	}
	selected_particles.clear();

	for (Node *node : selected_nodes) {
		GPUParticles2D *selected_particle = Object::cast_to<GPUParticles2D>(node);
		if (selected_particle) {
			selected_particle->set_show_visibility_rect(true);
			selected_particles.push_back(selected_particle);
		}
	}
}

void GPUParticles2DEditorPlugin::_generate_visibility_rect() {
	GPUParticles2D *particles = Object::cast_to<GPUParticles2D>(edited_node);

	double time = generate_seconds->get_value();

	float running = 0.0;

	EditorProgress ep("gen_vrect", TTR("Generating Visibility Rect (Waiting for Particle Simulation)"), int(time));

	bool was_emitting = particles->is_emitting();
	if (!was_emitting) {
		particles->set_emitting(true);
		OS::get_singleton()->delay_usec(1000);
	}

	Rect2 rect;
	while (running < time) {
		uint64_t ticks = OS::get_singleton()->get_ticks_usec();
		ep.step(TTR("Generating..."), int(running), true);
		OS::get_singleton()->delay_usec(1000);

		Rect2 capture = particles->capture_rect();
		if (rect == Rect2()) {
			rect = capture;
		} else {
			rect = rect.merge(capture);
		}

		running += (OS::get_singleton()->get_ticks_usec() - ticks) / 1000000.0;
	}

	if (!was_emitting) {
		particles->set_emitting(false);
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Generate Visibility Rect"));
	undo_redo->add_do_method(particles, "set_visibility_rect", rect);
	undo_redo->add_undo_method(particles, "set_visibility_rect", particles->get_visibility_rect());
	undo_redo->commit_action();
}

void GPUParticles2DEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			mask_browse_button->set_button_icon(mask_browse_button->get_editor_theme_icon(SNAME("Folder")));
			direction_browse_button->set_button_icon(direction_browse_button->get_editor_theme_icon(SNAME("Folder")));
			EditorNode::get_singleton()->get_editor_selection()->connect("selection_changed", callable_mp(this, &GPUParticles2DEditorPlugin::_selection_changed));
		} break;
	}
}

void Particles2DEditorPlugin::_menu_callback(int p_idx) {
	if (p_idx == MENU_LOAD_EMISSION_MASK) {
		GPUParticles2D *particles = Object::cast_to<GPUParticles2D>(edited_node);
		if (particles && particles->get_process_material().is_null()) {
			EditorNode::get_singleton()->show_warning(TTR("Loading emission mask requires ParticleProcessMaterial."));
			return;
		}

		emission_mask_dialog->popup_centered();
	} else {
		ParticlesEditorPlugin::_menu_callback(p_idx);
	}
}

void Particles2DEditorPlugin::_add_menu_options(PopupMenu *p_menu) {
	p_menu->add_item(TTR("Load Emission Mask"), MENU_LOAD_EMISSION_MASK);
}

void Particles2DEditorPlugin::_validate_textures() {
	DirectionMode direction_mode = static_cast<DirectionMode>(emission_direction_mode->get_selected());
	direction_img_label->set_visible(direction_mode == DIRECTION_MODE_TEXTURE);
	direction_img_hbox->set_visible(direction_mode == DIRECTION_MODE_TEXTURE);

	error_message->hide();
	emission_mask_dialog->get_ok_button()->set_disabled(true);

	if (mask_img_path_line_edit->get_text().is_empty()) {
		return;
	}

	Ref<Image> mask_img;
	mask_img.instantiate();
	Error err = ImageLoader::load_image(mask_img_path_line_edit->get_text(), mask_img);
	if (err != OK) {
		error_message->show();
		error_message->set_text(TTR("Failed to load mask texture."));
		return;
	}

	if (mask_img->is_compressed()) {
		mask_img->decompress();
	}
	mask_img->convert(Image::FORMAT_RGBA8);

	if (mask_img->get_format() != Image::FORMAT_RGBA8) {
		error_message->show();
		error_message->set_text(TTR("Failed to convert mask texture to RGBA8."));
		return;
	}

	Size2i mask_img_size = mask_img->get_size();
	if (mask_img_size.width == 0 || mask_img_size.height == 0) {
		error_message->show();
		error_message->set_text(TTR("Mask texture has an invalid size."));
		return;
	}

	if (direction_mode == DIRECTION_MODE_TEXTURE) {
		if (direction_img_path_line_edit->get_text().is_empty()) {
			return;
		}

		Ref<Image> direction_img;
		direction_img.instantiate();
		err = ImageLoader::load_image(direction_img_path_line_edit->get_text(), direction_img);

		if (err != OK) {
			error_message->show();
			error_message->set_text(TTR("Failed to load direction texture."));
			return;
		}

		if (direction_img->is_compressed()) {
			direction_img->decompress();
		}
		direction_img->convert(Image::FORMAT_RGBA8);

		if (direction_img->get_format() != Image::FORMAT_RGBA8) {
			error_message->show();
			error_message->set_text(TTR("Failed to convert direction texture to RGBA8."));
			return;
		}

		Size2i direction_img_size = direction_img->get_size();

		if (direction_img_size.width == 0 || direction_img_size.height == 0 || direction_img_size != mask_img_size) {
			error_message->show();
			error_message->set_text(TTR("Direction texture has an invalid size. It must have the same size as the mask texture."));
			return;
		}
	}

	emission_mask_dialog->get_ok_button()->set_disabled(false);
}

void Particles2DEditorPlugin::_mask_img_path_line_edit_text_changed(const String &p_text) {
	_validate_textures();
}

void Particles2DEditorPlugin::_direction_img_path_line_edit_text_changed(const String &p_text) {
	_validate_textures();
}

void Particles2DEditorPlugin::_emission_mask_mode_item_changed(int p_idx) const {
	emission_direction_mode->set_item_disabled(DIRECTION_MODE_GENERATE, p_idx == static_cast<int>(MASK_MODE_SOLID));

	if (emission_direction_mode->get_selected() == DIRECTION_MODE_GENERATE) {
		emission_direction_mode->select(DIRECTION_MODE_NONE);
	}
}

void Particles2DEditorPlugin::_emission_direction_mode_item_changed(int p_idx) {
	_validate_textures();
}

Node *GPUParticles2DEditorPlugin::_convert_particles() {
	GPUParticles2D *particles = Object::cast_to<GPUParticles2D>(edited_node);

	CPUParticles2D *cpu_particles = memnew(CPUParticles2D);
	cpu_particles->convert_from_particles(particles);
	cpu_particles->set_name(particles->get_name());
	cpu_particles->set_transform(particles->get_transform());
	cpu_particles->set_visible(particles->is_visible());
	cpu_particles->set_process_mode(particles->get_process_mode());
	cpu_particles->set_z_index(particles->get_z_index());
	return cpu_particles;
}

void GPUParticles2DEditorPlugin::_generate_emission_mask() {
	GPUParticles2D *particles = Object::cast_to<GPUParticles2D>(edited_node);
	Ref<ParticleProcessMaterial> pm = particles->get_process_material();
	ERR_FAIL_COND(pm.is_null());

	PackedVector2Array emission_positions;
	PackedVector2Array emission_normals;
	PackedByteArray emission_colors;
	Vector2i texture_size;
	_process_emission_masks(emission_positions, emission_normals, emission_colors, texture_size);

	ERR_FAIL_COND_MSG(emission_positions.is_empty(), "No pixels with transparency > 128 in image...");

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Load Emission Mask"));
	ParticleProcessMaterial *pmptr = pm.ptr();

	Vector<uint8_t> mask_texture_data;

	int valid_positions_count = emission_positions.size();
	int w = 2048;
	int h = (valid_positions_count / 2048) + 1;

	mask_texture_data.resize_zeroed(w * h * 2 * sizeof(float));

	{
		Vector2 offset;
		if (emission_mask_centered->is_pressed()) {
			offset = Vector2(-texture_size.width * 0.5, -texture_size.height * 0.5);
		}

		uint8_t *tw = mask_texture_data.ptrw();
		float *twf = reinterpret_cast<float *>(tw);
		for (int i = 0; i < valid_positions_count; i++) {
			twf[i * 2 + 0] = emission_positions[i].x + offset.x;
			twf[i * 2 + 1] = emission_positions[i].y + offset.y;
		}
	}

	Ref<Image> img;
	img.instantiate();
	img->set_data(w, h, false, Image::FORMAT_RGF, mask_texture_data);
	undo_redo->add_do_property(pmptr, "emission_point_texture", ImageTexture::create_from_image(img));
	undo_redo->add_undo_property(pmptr, "emission_point_texture", pm->get_emission_point_texture());
	undo_redo->add_do_property(pmptr, "emission_point_count", valid_positions_count);
	undo_redo->add_undo_property(pmptr, "emission_point_count", pm->get_emission_point_count());

	if (emission_mask_colors->is_pressed()) {
		Vector<uint8_t> color_texture_data;
		color_texture_data.resize_zeroed(w * h * 4);

		{
			uint8_t *tw = color_texture_data.ptrw();
			for (int i = 0; i < valid_positions_count * 4; i++) {
				tw[i] = emission_colors[i];
			}
		}

		img.instantiate();
		img->set_data(w, h, false, Image::FORMAT_RGBA8, color_texture_data);
		undo_redo->add_do_property(pmptr, "emission_color_texture", ImageTexture::create_from_image(img));
		undo_redo->add_undo_property(pmptr, "emission_color_texture", pm->get_emission_color_texture());
	}

	if (emission_normals.size()) {
		undo_redo->add_do_property(pmptr, "emission_shape", ParticleProcessMaterial::EMISSION_SHAPE_DIRECTED_POINTS);
		undo_redo->add_undo_property(pmptr, "emission_shape", pm->get_emission_shape());
		pm->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_DIRECTED_POINTS);

		Vector<uint8_t> normal_texture_data;
		normal_texture_data.resize_zeroed(w * h * 2 * sizeof(float));

		{
			uint8_t *tw = normal_texture_data.ptrw();
			float *twf = reinterpret_cast<float *>(tw);
			for (int i = 0; i < valid_positions_count; i++) {
				twf[i * 2 + 0] = emission_normals[i].x;
				twf[i * 2 + 1] = emission_normals[i].y;
			}
		}

		img.instantiate();
		img->set_data(w, h, false, Image::FORMAT_RGF, normal_texture_data);
		undo_redo->add_do_property(pmptr, "emission_normal_texture", ImageTexture::create_from_image(img));
		undo_redo->add_undo_property(pmptr, "emission_normal_texture", pm->get_emission_normal_texture());
	} else {
		undo_redo->add_do_property(pmptr, "emission_shape", ParticleProcessMaterial::EMISSION_SHAPE_POINTS);
		undo_redo->add_undo_property(pmptr, "emission_shape", pm->get_emission_shape());
	}
	undo_redo->commit_action();
}

GPUParticles2DEditorPlugin::GPUParticles2DEditorPlugin() {
	handled_type = TTRC("GPUParticles2D");
	conversion_option_name = TTR("Convert to CPUParticles2D");

	generate_visibility_rect = memnew(ConfirmationDialog);
	generate_visibility_rect->set_title(TTR("Generate Visibility Rect"));

	VBoxContainer *genvb = memnew(VBoxContainer);
	generate_visibility_rect->add_child(genvb);

	generate_seconds = memnew(SpinBox);
	generate_seconds->set_min(0.1);
	generate_seconds->set_max(25);
	generate_seconds->set_value(2);
	genvb->add_margin_child(TTR("Generation Time (sec):"), generate_seconds);

	EditorNode::get_singleton()->get_gui_base()->add_child(generate_visibility_rect);

	generate_visibility_rect->connect(SceneStringName(confirmed), callable_mp(this, &GPUParticles2DEditorPlugin::_generate_visibility_rect));
}

void CPUParticles2DEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			mask_browse_button->set_button_icon(mask_browse_button->get_editor_theme_icon(SNAME("Folder")));
			direction_browse_button->set_button_icon(direction_browse_button->get_editor_theme_icon(SNAME("Folder")));
		} break;
	}
}

Node *CPUParticles2DEditorPlugin::_convert_particles() {
	CPUParticles2D *particles = Object::cast_to<CPUParticles2D>(edited_node);

	GPUParticles2D *gpu_particles = memnew(GPUParticles2D);
	gpu_particles->convert_from_particles(particles);
	gpu_particles->set_name(particles->get_name());
	gpu_particles->set_transform(particles->get_transform());
	gpu_particles->set_visible(particles->is_visible());
	gpu_particles->set_process_mode(particles->get_process_mode());
	return gpu_particles;
}

void CPUParticles2DEditorPlugin::_generate_emission_mask() {
	CPUParticles2D *particles = Object::cast_to<CPUParticles2D>(edited_node);

	PackedVector2Array valid_positions;
	PackedVector2Array valid_normals;
	PackedByteArray valid_colors;
	Vector2i image_size;
	_process_emission_masks(valid_positions, valid_normals, valid_colors, image_size);

	ERR_FAIL_COND_MSG(valid_positions.is_empty(), "No pixels with transparency > 128 in image...");

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Load Emission Mask"));

	int vpc = valid_positions.size();
	if (emission_mask_colors->is_pressed()) {
		PackedColorArray pca;
		pca.resize(vpc);
		Color *pcaw = pca.ptrw();
		for (int i = 0; i < vpc; i += 1) {
			Color color;
			color.r = valid_colors[i * 4 + 0] / 255.0f;
			color.g = valid_colors[i * 4 + 1] / 255.0f;
			color.b = valid_colors[i * 4 + 2] / 255.0f;
			color.a = valid_colors[i * 4 + 3] / 255.0f;
			pcaw[i] = color;
		}
		undo_redo->add_do_property(particles, "emission_colors", pca);
		undo_redo->add_undo_property(particles, "emission_colors", particles->get_emission_colors());
	}

	if (valid_normals.size()) {
		undo_redo->add_do_property(particles, "emission_shape", CPUParticles2D::EMISSION_SHAPE_DIRECTED_POINTS);
		undo_redo->add_undo_property(particles, "emission_shape", particles->get_emission_shape());
		PackedVector2Array norms;
		norms.resize(valid_normals.size());
		Vector2 *normsw = norms.ptrw();
		for (int i = 0; i < valid_normals.size(); i += 1) {
			normsw[i] = valid_normals[i];
		}
		undo_redo->add_do_property(particles, "emission_normals", norms);
		undo_redo->add_undo_property(particles, "emission_normals", particles->get_emission_normals());
	} else {
		undo_redo->add_do_property(particles, "emission_shape", CPUParticles2D::EMISSION_SHAPE_POINTS);
		undo_redo->add_undo_property(particles, "emission_shape", particles->get_emission_shape());
	}

	{
		Vector2 offset;
		if (emission_mask_centered->is_pressed()) {
			offset = Vector2(-image_size.width * 0.5, -image_size.height * 0.5);
		}

		PackedVector2Array points;
		points.resize(valid_positions.size());
		Vector2 *pointsw = points.ptrw();
		for (int i = 0; i < valid_positions.size(); i += 1) {
			pointsw[i] = valid_positions[i] + offset;
		}
		undo_redo->add_do_property(particles, "emission_points", points);
		undo_redo->add_undo_property(particles, "emission_shape", particles->get_emission_points());
	}
	undo_redo->commit_action();
}

CPUParticles2DEditorPlugin::CPUParticles2DEditorPlugin() {
	handled_type = TTRC("CPUParticles2D");
	conversion_option_name = TTR("Convert to GPUParticles2D");
}

// 3D /////////////////////////////////////////////

void Particles3DEditorPlugin::_generate_aabb() {
	double time = generate_seconds->get_value();

	double running = 0.0;

	EditorProgress ep("gen_aabb", TTR("Generating Visibility AABB (Waiting for Particle Simulation)"), int(time));

	bool was_emitting = edited_node->get("emitting");
	if (!was_emitting) {
		edited_node->set("emitting", true);
		OS::get_singleton()->delay_usec(1000);
	}

	AABB rect;
	Callable capture_aabb = Callable(edited_node, "capture_aabb");

	while (running < time) {
		uint64_t ticks = OS::get_singleton()->get_ticks_usec();
		ep.step(TTR("Generating..."), int(running), true);
		OS::get_singleton()->delay_usec(1000);

		AABB capture = capture_aabb.call();
		if (rect == AABB()) {
			rect = capture;
		} else {
			rect.merge_with(capture);
		}

		running += (OS::get_singleton()->get_ticks_usec() - ticks) / 1000000.0;
	}

	if (!was_emitting) {
		edited_node->set("emitting", false);
	}

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Generate Visibility AABB"));
	ur->add_do_property(edited_node, "visibility_aabb", rect);
	ur->add_undo_property(edited_node, "visibility_aabb", edited_node->get("visibility_aabb"));
	ur->commit_action();
}

void Particles3DEditorPlugin::_node_selected(const NodePath &p_path) {
	Node *sel = get_node(p_path);
	if (!sel) {
		return;
	}

	if (!sel->is_class("Node3D")) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("\"%s\" doesn't inherit from Node3D."), sel->get_name()));
		return;
	}

	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(sel);
	if (!mi || mi->get_mesh().is_null()) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("\"%s\" doesn't contain geometry."), sel->get_name()));
		return;
	}

	geometry = mi->get_mesh()->get_faces();
	if (geometry.size() == 0) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("\"%s\" doesn't contain face geometry."), sel->get_name()));
		return;
	}

	Transform3D geom_xform = edited_node->get("global_transform");
	geom_xform = geom_xform.affine_inverse() * mi->get_global_transform();
	int gc = geometry.size();
	Face3 *w = geometry.ptrw();

	for (int i = 0; i < gc; i++) {
		for (int j = 0; j < 3; j++) {
			w[i].vertex[j] = geom_xform.xform(w[i].vertex[j]);
		}
	}
	emission_dialog->popup_centered(Size2(300, 130));
}

void Particles3DEditorPlugin::_menu_callback(int p_idx) {
	switch (p_idx) {
		case MENU_OPTION_GENERATE_AABB: {
			if (need_show_lifetime_dialog(generate_seconds)) {
				generate_aabb->popup_centered();
			} else {
				_generate_aabb();
			}
		} break;

		case MENU_OPTION_CREATE_EMISSION_VOLUME_FROM_NODE: {
			if (_can_generate_points()) {
				emission_tree_dialog->popup_scenetree_dialog();
			}
		} break;

		default: {
			ParticlesEditorPlugin::_menu_callback(p_idx);
		}
	}
}

void Particles3DEditorPlugin::_add_menu_options(PopupMenu *p_menu) {
	p_menu->add_item(TTR("Generate AABB"), MENU_OPTION_GENERATE_AABB);
	p_menu->add_item(TTR("Create Emission Points From Node"), MENU_OPTION_CREATE_EMISSION_VOLUME_FROM_NODE);
}

bool Particles3DEditorPlugin::_generate(Vector<Vector3> &r_points, Vector<Vector3> &r_normals) {
	bool use_normals = emission_fill->get_selected() == 1;

	if (emission_fill->get_selected() < 2) {
		float area_accum = 0;
		RBMap<float, int> triangle_area_map;

		for (int i = 0; i < geometry.size(); i++) {
			float area = geometry[i].get_area();
			if (area < CMP_EPSILON) {
				continue;
			}
			triangle_area_map[area_accum] = i;
			area_accum += area;
		}

		if (!triangle_area_map.size() || area_accum == 0) {
			EditorNode::get_singleton()->show_warning(TTR("The geometry's faces don't contain any area."));
			return false;
		}

		int emissor_count = emission_amount->get_value();

		for (int i = 0; i < emissor_count; i++) {
			float areapos = Math::random(0.0f, area_accum);

			RBMap<float, int>::Iterator E = triangle_area_map.find_closest(areapos);
			ERR_FAIL_COND_V(!E, false);
			int index = E->value;
			ERR_FAIL_INDEX_V(index, geometry.size(), false);

			// ok FINALLY get face
			Face3 face = geometry[index];
			//now compute some position inside the face...

			Vector3 pos = face.get_random_point_inside();

			r_points.push_back(pos);

			if (use_normals) {
				Vector3 normal = face.get_plane().normal;
				r_normals.push_back(normal);
			}
		}
	} else {
		int gcount = geometry.size();

		if (gcount == 0) {
			EditorNode::get_singleton()->show_warning(TTR("The geometry doesn't contain any faces."));
			return false;
		}

		const Face3 *r = geometry.ptr();

		AABB aabb;

		for (int i = 0; i < gcount; i++) {
			for (int j = 0; j < 3; j++) {
				if (i == 0 && j == 0) {
					aabb.position = r[i].vertex[j];
				} else {
					aabb.expand_to(r[i].vertex[j]);
				}
			}
		}

		int emissor_count = emission_amount->get_value();

		for (int i = 0; i < emissor_count; i++) {
			int attempts = 5;

			for (int j = 0; j < attempts; j++) {
				Vector3 dir;
				dir[Math::rand() % 3] = 1.0;
				Vector3 ofs = (Vector3(1, 1, 1) - dir) * Vector3(Math::randf(), Math::randf(), Math::randf()) * aabb.size + aabb.position;

				Vector3 ofsv = ofs + aabb.size * dir;

				//space it a little
				ofs -= dir;
				ofsv += dir;

				float max = -1e7, min = 1e7;

				for (int k = 0; k < gcount; k++) {
					const Face3 &f3 = r[k];

					Vector3 res;
					if (f3.intersects_segment(ofs, ofsv, &res)) {
						res -= ofs;
						float d = dir.dot(res);

						if (d < min) {
							min = d;
						}
						if (d > max) {
							max = d;
						}
					}
				}

				if (max < min) {
					continue; //lost attempt
				}

				float val = min + (max - min) * Math::randf();

				Vector3 point = ofs + dir * val;

				r_points.push_back(point);
				break;
			}
		}
	}
	return true;
}

Particles3DEditorPlugin::Particles3DEditorPlugin() {
	generate_aabb = memnew(ConfirmationDialog);
	generate_aabb->set_title(TTR("Generate Visibility AABB"));

	VBoxContainer *genvb = memnew(VBoxContainer);
	generate_aabb->add_child(genvb);

	generate_seconds = memnew(SpinBox);
	generate_seconds->set_min(0.1);
	generate_seconds->set_max(25);
	generate_seconds->set_value(2);
	genvb->add_margin_child(TTR("Generation Time (sec):"), generate_seconds);

	EditorNode::get_singleton()->get_gui_base()->add_child(generate_aabb);

	generate_aabb->connect(SceneStringName(confirmed), callable_mp(this, &Particles3DEditorPlugin::_generate_aabb));

	emission_tree_dialog = memnew(SceneTreeDialog);
	Vector<StringName> valid_types;
	valid_types.push_back("MeshInstance3D");
	emission_tree_dialog->set_valid_types(valid_types);
	EditorNode::get_singleton()->get_gui_base()->add_child(emission_tree_dialog);
	emission_tree_dialog->connect("selected", callable_mp(this, &Particles3DEditorPlugin::_node_selected));

	emission_dialog = memnew(ConfirmationDialog);
	emission_dialog->set_title(TTR("Create Emitter"));
	EditorNode::get_singleton()->get_gui_base()->add_child(emission_dialog);

	VBoxContainer *emd_vb = memnew(VBoxContainer);
	emission_dialog->add_child(emd_vb);

	emission_amount = memnew(SpinBox);
	emission_amount->set_min(1);
	emission_amount->set_max(100000);
	emission_amount->set_value(512);
	emd_vb->add_margin_child(TTR("Emission Points:"), emission_amount);

	emission_fill = memnew(OptionButton);
	emission_fill->add_item(TTR("Surface Points"));
	emission_fill->add_item(TTR("Surface Points+Normal (Directed)"));
	emission_fill->add_item(TTR("Volume"));
	emd_vb->add_margin_child(TTR("Emission Source:"), emission_fill);

	emission_dialog->set_ok_button_text(TTR("Create"));
	emission_dialog->get_ok_button()->set_disabled(true);
	emission_dialog->connect(SceneStringName(confirmed), callable_mp(this, &Particles3DEditorPlugin::_generate_emission_points));
}

Node *GPUParticles3DEditorPlugin::_convert_particles() {
	GPUParticles3D *particles = Object::cast_to<GPUParticles3D>(edited_node);

	CPUParticles3D *cpu_particles = memnew(CPUParticles3D);
	cpu_particles->convert_from_particles(particles);
	cpu_particles->set_name(particles->get_name());
	cpu_particles->set_transform(particles->get_transform());
	cpu_particles->set_visible(particles->is_visible());
	cpu_particles->set_process_mode(particles->get_process_mode());
	return cpu_particles;
}

bool GPUParticles3DEditorPlugin::_can_generate_points() const {
	GPUParticles3D *particles = Object::cast_to<GPUParticles3D>(edited_node);
	Ref<ParticleProcessMaterial> mat = particles->get_process_material();
	if (mat.is_null()) {
		EditorNode::get_singleton()->show_warning(TTR("A processor material of type 'ParticleProcessMaterial' is required."));
		return false;
	}
	return true;
}

void GPUParticles3DEditorPlugin::_generate_emission_points() {
	GPUParticles3D *particles = Object::cast_to<GPUParticles3D>(edited_node);

	/// hacer codigo aca
	Vector<Vector3> points;
	Vector<Vector3> normals;

	if (!_generate(points, normals)) {
		return;
	}

	int point_count = points.size();

	int w = 2048;
	int h = (point_count / 2048) + 1;

	Vector<uint8_t> point_img;
	point_img.resize(w * h * 3 * sizeof(float));

	{
		uint8_t *iw = point_img.ptrw();
		memset(iw, 0, w * h * 3 * sizeof(float));
		const Vector3 *r = points.ptr();
		float *wf = reinterpret_cast<float *>(iw);
		for (int i = 0; i < point_count; i++) {
			wf[i * 3 + 0] = r[i].x;
			wf[i * 3 + 1] = r[i].y;
			wf[i * 3 + 2] = r[i].z;
		}
	}

	Ref<Image> image = memnew(Image(w, h, false, Image::FORMAT_RGBF, point_img));
	Ref<ImageTexture> tex = ImageTexture::create_from_image(image);

	Ref<ParticleProcessMaterial> mat = particles->get_process_material();
	ERR_FAIL_COND(mat.is_null());

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Create Emission Points"));
	ParticleProcessMaterial *matptr = mat.ptr();

	if (normals.size() > 0) {
		undo_redo->add_do_property(matptr, "emission_shape", ParticleProcessMaterial::EMISSION_SHAPE_DIRECTED_POINTS);
		undo_redo->add_undo_property(matptr, "emission_shape", matptr->get_emission_shape());

		Vector<uint8_t> point_img2;
		point_img2.resize(w * h * 3 * sizeof(float));

		{
			uint8_t *iw = point_img2.ptrw();
			memset(iw, 0, w * h * 3 * sizeof(float));
			const Vector3 *r = normals.ptr();
			float *wf = reinterpret_cast<float *>(iw);
			for (int i = 0; i < point_count; i++) {
				wf[i * 3 + 0] = r[i].x;
				wf[i * 3 + 1] = r[i].y;
				wf[i * 3 + 2] = r[i].z;
			}
		}

		Ref<Image> image2 = memnew(Image(w, h, false, Image::FORMAT_RGBF, point_img2));
		undo_redo->add_do_property(matptr, "emission_normal_texture", image2);
		undo_redo->add_undo_property(matptr, "emission_normal_texture", matptr->get_emission_normal_texture());
	} else {
		undo_redo->add_do_property(matptr, "emission_shape", ParticleProcessMaterial::EMISSION_SHAPE_POINTS);
		undo_redo->add_undo_property(matptr, "emission_shape", matptr->get_emission_shape());
	}
	undo_redo->add_do_property(matptr, "emission_point_count", point_count);
	undo_redo->add_undo_property(matptr, "emission_point_count", matptr->get_emission_point_count());
	undo_redo->add_do_property(matptr, "emission_point_texture", tex);
	undo_redo->add_undo_property(matptr, "emission_point_texture", matptr->get_emission_point_texture());
	undo_redo->commit_action();
}

GPUParticles3DEditorPlugin::GPUParticles3DEditorPlugin() {
	handled_type = TTRC("GPUParticles3D");
	conversion_option_name = TTR("Convert to CPUParticles3D");
}

Node *CPUParticles3DEditorPlugin::_convert_particles() {
	CPUParticles3D *particles = Object::cast_to<CPUParticles3D>(edited_node);

	GPUParticles3D *gpu_particles = memnew(GPUParticles3D);
	gpu_particles->convert_from_particles(particles);
	gpu_particles->set_name(particles->get_name());
	gpu_particles->set_transform(particles->get_transform());
	gpu_particles->set_visible(particles->is_visible());
	gpu_particles->set_process_mode(particles->get_process_mode());
	return gpu_particles;
}

void CPUParticles3DEditorPlugin::_generate_emission_points() {
	CPUParticles3D *particles = Object::cast_to<CPUParticles3D>(edited_node);

	/// hacer codigo aca
	Vector<Vector3> points;
	Vector<Vector3> normals;

	if (!_generate(points, normals)) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Create Emission Points"));

	if (normals.is_empty()) {
		undo_redo->add_do_property(particles, "emission_shape", ParticleProcessMaterial::EMISSION_SHAPE_POINTS);
		undo_redo->add_undo_property(particles, "emission_shape", particles->get_emission_shape());
	} else {
		undo_redo->add_do_property(particles, "emission_shape", ParticleProcessMaterial::EMISSION_SHAPE_DIRECTED_POINTS);
		undo_redo->add_undo_property(particles, "emission_shape", particles->get_emission_shape());
		undo_redo->add_do_property(particles, "emission_normals", normals);
		undo_redo->add_undo_property(particles, "emission_normals", particles->get_emission_normals());
	}
	undo_redo->add_do_property(particles, "emission_points", points);
	undo_redo->add_undo_property(particles, "emission_points", particles->get_emission_points());
	undo_redo->commit_action();
}

CPUParticles3DEditorPlugin::CPUParticles3DEditorPlugin() {
	handled_type = TTRC("CPUParticles3D");
	conversion_option_name = TTR("Convert to GPUParticles3D");
}
