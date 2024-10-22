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
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/scene_tree_dock.h"
#include "scene/2d/cpu_particles_2d.h"
#include "scene/2d/gpu_particles_2d.h"
#include "scene/3d/cpu_particles_3d.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/separator.h"
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

			menu->set_icon(menu->get_editor_theme_icon(handled_type));
			menu->set_text(handled_type);

			PopupMenu *popup = menu->get_popup();
			popup->add_shortcut(ED_SHORTCUT("particles/restart_emission", TTR("Restart Emission"), KeyModifierMask::CTRL | Key::R), MENU_RESTART);
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

void Particles2DEditorPlugin::_file_selected(const String &p_file) {
	source_emission_file = p_file;
	emission_mask->popup_centered();
}

void Particles2DEditorPlugin::_get_base_emission_mask(PackedVector2Array &r_valid_positions, PackedVector2Array &r_valid_normals, PackedByteArray &r_valid_colors, Vector2i &r_image_size) {
	Ref<Image> img;
	img.instantiate();
	Error err = ImageLoader::load_image(source_emission_file, img);
	ERR_FAIL_COND_MSG(err != OK, "Error loading image '" + source_emission_file + "'.");

	if (img->is_compressed()) {
		img->decompress();
	}
	img->convert(Image::FORMAT_RGBA8);
	ERR_FAIL_COND(img->get_format() != Image::FORMAT_RGBA8);
	Size2i s = img->get_size();
	ERR_FAIL_COND(s.width == 0 || s.height == 0);

	r_image_size = s;

	r_valid_positions.resize(s.width * s.height);

	EmissionMode emode = (EmissionMode)emission_mask_mode->get_selected();

	if (emode == EMISSION_MODE_BORDER_DIRECTED) {
		r_valid_normals.resize(s.width * s.height);
	}

	bool capture_colors = emission_colors->is_pressed();

	if (capture_colors) {
		r_valid_colors.resize(s.width * s.height * 4);
	}

	int vpc = 0;

	{
		Vector<uint8_t> img_data = img->get_data();
		const uint8_t *r = img_data.ptr();

		for (int i = 0; i < s.width; i++) {
			for (int j = 0; j < s.height; j++) {
				uint8_t a = r[(j * s.width + i) * 4 + 3];

				if (a > 128) {
					if (emode == EMISSION_MODE_SOLID) {
						if (capture_colors) {
							r_valid_colors.write[vpc * 4 + 0] = r[(j * s.width + i) * 4 + 0];
							r_valid_colors.write[vpc * 4 + 1] = r[(j * s.width + i) * 4 + 1];
							r_valid_colors.write[vpc * 4 + 2] = r[(j * s.width + i) * 4 + 2];
							r_valid_colors.write[vpc * 4 + 3] = r[(j * s.width + i) * 4 + 3];
						}
						r_valid_positions.write[vpc++] = Point2(i, j);

					} else {
						bool on_border = false;
						for (int x = i - 1; x <= i + 1; x++) {
							for (int y = j - 1; y <= j + 1; y++) {
								if (x < 0 || y < 0 || x >= s.width || y >= s.height || r[(y * s.width + x) * 4 + 3] <= 128) {
									on_border = true;
									break;
								}
							}

							if (on_border) {
								break;
							}
						}

						if (on_border) {
							r_valid_positions.write[vpc] = Point2(i, j);

							if (emode == EMISSION_MODE_BORDER_DIRECTED) {
								Vector2 normal;
								for (int x = i - 2; x <= i + 2; x++) {
									for (int y = j - 2; y <= j + 2; y++) {
										if (x == i && y == j) {
											continue;
										}

										if (x < 0 || y < 0 || x >= s.width || y >= s.height || r[(y * s.width + x) * 4 + 3] <= 128) {
											normal += Vector2(x - i, y - j).normalized();
										}
									}
								}

								normal.normalize();
								r_valid_normals.write[vpc] = normal;
							}

							if (capture_colors) {
								r_valid_colors.write[vpc * 4 + 0] = r[(j * s.width + i) * 4 + 0];
								r_valid_colors.write[vpc * 4 + 1] = r[(j * s.width + i) * 4 + 1];
								r_valid_colors.write[vpc * 4 + 2] = r[(j * s.width + i) * 4 + 2];
								r_valid_colors.write[vpc * 4 + 3] = r[(j * s.width + i) * 4 + 3];
							}

							vpc++;
						}
					}
				}
			}
		}
	}

	r_valid_positions.resize(vpc);
	if (!r_valid_normals.is_empty()) {
		r_valid_normals.resize(vpc);
	}
}

Particles2DEditorPlugin::Particles2DEditorPlugin() {
	file = memnew(EditorFileDialog);

	List<String> ext;
	ImageLoader::get_recognized_extensions(&ext);
	for (const String &E : ext) {
		file->add_filter("*." + E, E.to_upper());
	}

	file->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	EditorNode::get_singleton()->get_gui_base()->add_child(file);
	file->connect("file_selected", callable_mp(this, &Particles2DEditorPlugin::_file_selected));

	emission_mask = memnew(ConfirmationDialog);
	emission_mask->set_title(TTR("Load Emission Mask"));

	VBoxContainer *emvb = memnew(VBoxContainer);
	emission_mask->add_child(emvb);

	emission_mask_mode = memnew(OptionButton);
	emission_mask_mode->add_item(TTR("Solid Pixels"), EMISSION_MODE_SOLID);
	emission_mask_mode->add_item(TTR("Border Pixels"), EMISSION_MODE_BORDER);
	emission_mask_mode->add_item(TTR("Directed Border Pixels"), EMISSION_MODE_BORDER_DIRECTED);
	emvb->add_margin_child(TTR("Emission Mask"), emission_mask_mode);

	VBoxContainer *optionsvb = memnew(VBoxContainer);
	emvb->add_margin_child(TTR("Options"), optionsvb);

	emission_mask_centered = memnew(CheckBox(TTR("Centered")));
	optionsvb->add_child(emission_mask_centered);
	emission_colors = memnew(CheckBox(TTR("Capture Colors from Pixel")));
	optionsvb->add_child(emission_colors);

	EditorNode::get_singleton()->get_gui_base()->add_child(emission_mask);

	emission_mask->connect(SceneStringName(confirmed), callable_mp(this, &Particles2DEditorPlugin::_generate_emission_mask));
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
			EditorNode::get_singleton()->get_editor_selection()->connect("selection_changed", callable_mp(this, &GPUParticles2DEditorPlugin::_selection_changed));
		} break;
	}
}

void Particles2DEditorPlugin::_menu_callback(int p_idx) {
	if (p_idx == MENU_LOAD_EMISSION_MASK) {
		file->popup_file_dialog();
	} else {
		ParticlesEditorPlugin::_menu_callback(p_idx);
	}
}

void Particles2DEditorPlugin::_add_menu_options(PopupMenu *p_menu) {
	p_menu->add_item(TTR("Load Emission Mask"), MENU_LOAD_EMISSION_MASK);
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
	if (pm.is_null()) {
		EditorNode::get_singleton()->show_warning(TTR("Can only set point into a ParticleProcessMaterial process material"));
		return;
	}

	PackedVector2Array valid_positions;
	PackedVector2Array valid_normals;
	PackedByteArray valid_colors;
	Vector2i image_size;
	_get_base_emission_mask(valid_positions, valid_normals, valid_colors, image_size);

	ERR_FAIL_COND_MSG(valid_positions.is_empty(), "No pixels with transparency > 128 in image...");

	Vector<uint8_t> texdata;

	int vpc = valid_positions.size();
	int w = 2048;
	int h = (vpc / 2048) + 1;

	texdata.resize(w * h * 2 * sizeof(float));

	{
		Vector2 offset;
		if (emission_mask_centered->is_pressed()) {
			offset = Vector2(-image_size.width * 0.5, -image_size.height * 0.5);
		}

		uint8_t *tw = texdata.ptrw();
		float *twf = reinterpret_cast<float *>(tw);
		for (int i = 0; i < vpc; i++) {
			twf[i * 2 + 0] = valid_positions[i].x + offset.x;
			twf[i * 2 + 1] = valid_positions[i].y + offset.y;
		}
	}

	Ref<Image> img;
	img.instantiate();
	img->set_data(w, h, false, Image::FORMAT_RGF, texdata);
	pm->set_emission_point_texture(ImageTexture::create_from_image(img));
	pm->set_emission_point_count(vpc);

	if (emission_colors->is_pressed()) {
		Vector<uint8_t> colordata;
		colordata.resize(w * h * 4); //use RG texture

		{
			uint8_t *tw = colordata.ptrw();
			for (int i = 0; i < vpc * 4; i++) {
				tw[i] = valid_colors[i];
			}
		}

		img.instantiate();
		img->set_data(w, h, false, Image::FORMAT_RGBA8, colordata);
		pm->set_emission_color_texture(ImageTexture::create_from_image(img));
	}

	if (valid_normals.size()) {
		pm->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_DIRECTED_POINTS);

		Vector<uint8_t> normdata;
		normdata.resize(w * h * 2 * sizeof(float)); //use RG texture

		{
			uint8_t *tw = normdata.ptrw();
			float *twf = reinterpret_cast<float *>(tw);
			for (int i = 0; i < vpc; i++) {
				twf[i * 2 + 0] = valid_normals[i].x;
				twf[i * 2 + 1] = valid_normals[i].y;
			}
		}

		img.instantiate();
		img->set_data(w, h, false, Image::FORMAT_RGF, normdata);
		pm->set_emission_normal_texture(ImageTexture::create_from_image(img));

	} else {
		pm->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_POINTS);
	}
}

GPUParticles2DEditorPlugin::GPUParticles2DEditorPlugin() {
	handled_type = "GPUParticles2D"; // TTR("GPUParticles2D")
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
	_get_base_emission_mask(valid_positions, valid_normals, valid_colors, image_size);

	ERR_FAIL_COND_MSG(valid_positions.is_empty(), "No pixels with transparency > 128 in image...");

	int vpc = valid_positions.size();
	if (emission_colors->is_pressed()) {
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
		particles->set_emission_colors(pca);
	}

	if (valid_normals.size()) {
		particles->set_emission_shape(CPUParticles2D::EMISSION_SHAPE_DIRECTED_POINTS);
		PackedVector2Array norms;
		norms.resize(valid_normals.size());
		Vector2 *normsw = norms.ptrw();
		for (int i = 0; i < valid_normals.size(); i += 1) {
			normsw[i] = valid_normals[i];
		}
		particles->set_emission_normals(norms);
	} else {
		particles->set_emission_shape(CPUParticles2D::EMISSION_SHAPE_POINTS);
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
		particles->set_emission_points(points);
	}
}

CPUParticles2DEditorPlugin::CPUParticles2DEditorPlugin() {
	handled_type = "CPUParticles2D"; // TTR("CPUParticles2D")
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

	if (normals.size() > 0) {
		mat->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_DIRECTED_POINTS);
		mat->set_emission_point_count(point_count);
		mat->set_emission_point_texture(tex);

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
		mat->set_emission_normal_texture(ImageTexture::create_from_image(image2));
	} else {
		mat->set_emission_shape(ParticleProcessMaterial::EMISSION_SHAPE_POINTS);
		mat->set_emission_point_count(point_count);
		mat->set_emission_point_texture(tex);
	}
}

GPUParticles3DEditorPlugin::GPUParticles3DEditorPlugin() {
	handled_type = "GPUParticles3D"; // TTR("GPUParticles3D")
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

	if (normals.is_empty()) {
		particles->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_POINTS);
		particles->set_emission_points(points);
	} else {
		particles->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_DIRECTED_POINTS);
		particles->set_emission_points(points);
		particles->set_emission_normals(normals);
	}
}

CPUParticles3DEditorPlugin::CPUParticles3DEditorPlugin() {
	handled_type = "CPUParticles3D"; // TTR("CPUParticles3D")
	conversion_option_name = TTR("Convert to GPUParticles3D");
}
