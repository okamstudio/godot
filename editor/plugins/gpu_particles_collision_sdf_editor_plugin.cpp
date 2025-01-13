/**************************************************************************/
/*  gpu_particles_collision_sdf_editor_plugin.cpp                         */
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

#include "gpu_particles_collision_sdf_editor_plugin.h"

#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"

void GPUParticlesCollisionSDF3DEditorPlugin::_bake() {
	if (col_sdf) {
		if (col_sdf->get_texture().is_null() || !col_sdf->get_texture()->get_path().is_resource_file()) {
			String path = get_tree()->get_edited_scene_root()->get_scene_file_path();
			if (path.is_empty()) {
				path = "res://" + col_sdf->get_name() + "_data.exr";
			} else {
				String ext = path.get_extension();
				path = path.get_basename() + "." + col_sdf->get_name() + "_data.exr";
			}
			probe_file->set_current_path(path);
			probe_file->popup_file_dialog();
			return;
		}

		_sdf_save_path_and_bake(col_sdf->get_texture()->get_path());
	}
}

void GPUParticlesCollisionSDF3DEditorPlugin::edit(Object *p_object) {
	GPUParticlesCollisionSDF3D *s = Object::cast_to<GPUParticlesCollisionSDF3D>(p_object);
	if (!s) {
		return;
	}

	col_sdf = s;
}

bool GPUParticlesCollisionSDF3DEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("GPUParticlesCollisionSDF3D");
}

void GPUParticlesCollisionSDF3DEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS: {
			if (!col_sdf) {
				return;
			}

			// Set information tooltip on the Bake button. This information is useful
			// to optimize performance (video RAM size) and reduce collision tunneling (individual cell size).

			const Vector3i size = col_sdf->get_estimated_cell_size();

			const Vector3 extents = col_sdf->get_size() / 2;

			int data_size = 2;
			const double size_mb = size.x * size.y * size.z * data_size / (1024.0 * 1024.0);
			// Add a qualitative measurement to help the user assess whether a GPUParticlesCollisionSDF3D node is using a lot of VRAM.
			String size_quality;
			if (size_mb < 8.0) {
				size_quality = TTR("Low");
			} else if (size_mb < 32.0) {
				size_quality = TTR("Moderate");
			} else {
				size_quality = TTR("High");
			}

			String text;
			text += vformat(TTR("Subdivisions: %s"), vformat(U"%d × %d × %d", size.x, size.y, size.z)) + "\n";
			text += vformat(TTR("Cell size: %s"), vformat(U"%.3f × %.3f × %.3f", extents.x / size.x, extents.y / size.y, extents.z / size.z)) + "\n";
			text += vformat(TTR("Video RAM size: %s MB (%s)"), String::num(size_mb, 2), size_quality);

			// Only update the tooltip when needed to avoid constant redrawing.
			if (bake->get_tooltip(Point2()) == text) {
				return;
			}

			bake->set_tooltip_text(text);
		} break;
	}
}

void GPUParticlesCollisionSDF3DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		bake_hb->show();
		set_process(true);
	} else {
		bake_hb->hide();
		set_process(false);
	}
}

EditorProgress *GPUParticlesCollisionSDF3DEditorPlugin::tmp_progress = nullptr;

void GPUParticlesCollisionSDF3DEditorPlugin::bake_func_begin(int p_steps) {
	ERR_FAIL_COND(tmp_progress != nullptr);

	tmp_progress = memnew(EditorProgress("bake_sdf", TTR("Bake SDF"), p_steps));
}

void GPUParticlesCollisionSDF3DEditorPlugin::bake_func_step(int p_step, const String &p_description) {
	ERR_FAIL_NULL(tmp_progress);
	tmp_progress->step(p_description, p_step, false);
}

void GPUParticlesCollisionSDF3DEditorPlugin::bake_func_end() {
	ERR_FAIL_NULL(tmp_progress);
	memdelete(tmp_progress);
	tmp_progress = nullptr;
}

void GPUParticlesCollisionSDF3DEditorPlugin::_sdf_save_path_and_bake(const String &p_path) {
	probe_file->hide();
	if (col_sdf) {
		Ref<Image> bake_img = col_sdf->bake();
		if (bake_img.is_null()) {
			EditorNode::get_singleton()->show_warning(TTR("No faces detected during GPUParticlesCollisionSDF3D bake.\nCheck whether there are visible meshes matching the bake mask within its extents."));
			return;
		}

		Ref<ConfigFile> config;

		config.instantiate();
		if (FileAccess::exists(p_path + ".import")) {
			config->load(p_path + ".import");
		}

		config->set_value("remap", "importer", "3d_texture");
		config->set_value("remap", "type", "CompressedTexture3D");
		if (!config->has_section_key("params", "compress/mode")) {
			config->set_value("params", "compress/mode", 3); //user may want another compression, so leave it be
		}
		config->set_value("params", "compress/channel_pack", 1);
		config->set_value("params", "mipmaps/generate", false);
		config->set_value("params", "slices/horizontal", 1);
		config->set_value("params", "slices/vertical", bake_img->get_meta("depth"));

		config->save(p_path + ".import");

		Error err = bake_img->save_exr(p_path, false);
		ERR_FAIL_COND(err);
		ResourceLoader::import(p_path);
		Ref<Texture> t = ResourceLoader::load(p_path); //if already loaded, it will be updated on refocus?
		ERR_FAIL_COND(t.is_null());

		col_sdf->set_texture(t);
	}
}

GPUParticlesCollisionSDF3DEditorPlugin::GPUParticlesCollisionSDF3DEditorPlugin() {
	bake_hb = memnew(HBoxContainer);
	bake_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	bake_hb->hide();
	bake = memnew(Button);
	bake->set_theme_type_variation(SceneStringName(FlatButton));
	bake->set_button_icon(EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("Bake"), EditorStringName(EditorIcons)));
	bake->set_text(TTR("Bake SDF"));
	bake->connect(SceneStringName(pressed), callable_mp(this, &GPUParticlesCollisionSDF3DEditorPlugin::_bake));
	bake_hb->add_child(bake);

	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, bake_hb);
	col_sdf = nullptr;
	probe_file = memnew(EditorFileDialog);
	probe_file->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	probe_file->add_filter("*.exr");
	probe_file->connect("file_selected", callable_mp(this, &GPUParticlesCollisionSDF3DEditorPlugin::_sdf_save_path_and_bake));
	EditorInterface::get_singleton()->get_base_control()->add_child(probe_file);
	probe_file->set_title(TTR("Select path for SDF Texture"));

	GPUParticlesCollisionSDF3D::bake_begin_function = bake_func_begin;
	GPUParticlesCollisionSDF3D::bake_step_function = bake_func_step;
	GPUParticlesCollisionSDF3D::bake_end_function = bake_func_end;
}

GPUParticlesCollisionSDF3DEditorPlugin::~GPUParticlesCollisionSDF3DEditorPlugin() {
}
