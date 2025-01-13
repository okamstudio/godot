/**************************************************************************/
/*  voxel_gi_editor_plugin.cpp                                            */
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

#include "voxel_gi_editor_plugin.h"

#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"

void VoxelGIEditorPlugin::_bake() {
	if (voxel_gi) {
		Ref<VoxelGIData> voxel_gi_data = voxel_gi->get_probe_data();

		if (voxel_gi_data.is_null()) {
			String path = get_tree()->get_edited_scene_root()->get_scene_file_path();
			if (path.is_empty()) {
				path = "res://" + voxel_gi->get_name() + "_data.res";
			} else {
				String ext = path.get_extension();
				path = path.get_basename() + "." + voxel_gi->get_name() + "_data.res";
			}
			probe_file->set_current_path(path);
			probe_file->popup_file_dialog();
			return;
		} else {
			String path = voxel_gi_data->get_path();
			if (!path.is_resource_file()) {
				int srpos = path.find("::");
				if (srpos != -1) {
					String base = path.substr(0, srpos);
					if (ResourceLoader::get_resource_type(base) == "PackedScene") {
						if (!get_tree()->get_edited_scene_root() || get_tree()->get_edited_scene_root()->get_scene_file_path() != base) {
							EditorNode::get_singleton()->show_warning(TTR("Voxel GI data is not local to the scene."));
							return;
						}
					} else {
						if (FileAccess::exists(base + ".import")) {
							EditorNode::get_singleton()->show_warning(TTR("Voxel GI data is part of an imported resource."));
							return;
						}
					}
				}
			} else {
				if (FileAccess::exists(path + ".import")) {
					EditorNode::get_singleton()->show_warning(TTR("Voxel GI data is an imported resource."));
					return;
				}
			}
		}

		voxel_gi->bake();
	}
}

void VoxelGIEditorPlugin::edit(Object *p_object) {
	VoxelGI *s = Object::cast_to<VoxelGI>(p_object);
	if (!s) {
		return;
	}

	voxel_gi = s;
}

bool VoxelGIEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("VoxelGI");
}

void VoxelGIEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS: {
			if (!voxel_gi) {
				return;
			}

			// Set information tooltip on the Bake button. This information is useful
			// to optimize performance (video RAM size) and reduce light leaking (individual cell size).

			const Vector3i cell_size = voxel_gi->get_estimated_cell_size();

			const Vector3 half_size = voxel_gi->get_size() / 2;

			const int data_size = 4;
			const double size_mb = cell_size.x * cell_size.y * cell_size.z * data_size / (1024.0 * 1024.0);
			// Add a qualitative measurement to help the user assess whether a VoxelGI node is using a lot of VRAM.
			String size_quality;
			if (size_mb < 16.0) {
				size_quality = TTR("Low");
			} else if (size_mb < 64.0) {
				size_quality = TTR("Moderate");
			} else {
				size_quality = TTR("High");
			}

			String text;
			text += vformat(TTR("Subdivisions: %s"), vformat(U"%d × %d × %d", cell_size.x, cell_size.y, cell_size.z)) + "\n";
			text += vformat(TTR("Cell size: %s"), vformat(U"%.3f × %.3f × %.3f", half_size.x / cell_size.x, half_size.y / cell_size.y, half_size.z / cell_size.z)) + "\n";
			text += vformat(TTR("Video RAM size: %s MB (%s)"), String::num(size_mb, 2), size_quality);

			// Only update the tooltip when needed to avoid constant redrawing.
			if (bake->get_tooltip(Point2()) == text) {
				return;
			}

			bake->set_tooltip_text(text);
		} break;
	}
}

void VoxelGIEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		bake_hb->show();
		set_process(true);
	} else {
		bake_hb->hide();
		set_process(false);
	}
}

EditorProgress *VoxelGIEditorPlugin::tmp_progress = nullptr;

void VoxelGIEditorPlugin::bake_func_begin() {
	ERR_FAIL_COND(tmp_progress != nullptr);

	tmp_progress = memnew(EditorProgress("bake_gi", TTR("Bake VoxelGI"), 1000, true));
}

bool VoxelGIEditorPlugin::bake_func_step(int p_progress, const String &p_description) {
	ERR_FAIL_NULL_V(tmp_progress, false);
	return tmp_progress->step(p_description, p_progress, false);
}

void VoxelGIEditorPlugin::bake_func_end() {
	ERR_FAIL_NULL(tmp_progress);
	memdelete(tmp_progress);
	tmp_progress = nullptr;
}

void VoxelGIEditorPlugin::_voxel_gi_save_path_and_bake(const String &p_path) {
	probe_file->hide();
	if (voxel_gi) {
		voxel_gi->bake();
		// Ensure the VoxelGIData is always saved to an external resource.
		// This avoids bloating the scene file with large binary data,
		// which would be serialized as Base64 if the scene is a `.tscn` file.
		Ref<VoxelGIData> voxel_gi_data = voxel_gi->get_probe_data();
		ERR_FAIL_COND(voxel_gi_data.is_null());
		voxel_gi_data->set_path(p_path);
		ResourceSaver::save(voxel_gi_data, p_path, ResourceSaver::FLAG_CHANGE_PATH);
	}
}

VoxelGIEditorPlugin::VoxelGIEditorPlugin() {
	bake_hb = memnew(HBoxContainer);
	bake_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	bake_hb->hide();
	bake = memnew(Button);
	bake->set_theme_type_variation(SceneStringName(FlatButton));
	// TODO: Rework this as a dedicated toolbar control so we can hook into theme changes and update it
	// when the editor theme updates.
	bake->set_button_icon(EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("Bake"), EditorStringName(EditorIcons)));
	bake->set_text(TTR("Bake VoxelGI"));
	bake->connect(SceneStringName(pressed), callable_mp(this, &VoxelGIEditorPlugin::_bake));
	bake_hb->add_child(bake);

	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, bake_hb);
	voxel_gi = nullptr;
	probe_file = memnew(EditorFileDialog);
	probe_file->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	probe_file->add_filter("*.res");
	probe_file->connect("file_selected", callable_mp(this, &VoxelGIEditorPlugin::_voxel_gi_save_path_and_bake));
	EditorInterface::get_singleton()->get_base_control()->add_child(probe_file);
	probe_file->set_title(TTR("Select path for VoxelGI Data File"));

	VoxelGI::bake_begin_function = bake_func_begin;
	VoxelGI::bake_step_function = bake_func_step;
	VoxelGI::bake_end_function = bake_func_end;
}

VoxelGIEditorPlugin::~VoxelGIEditorPlugin() {
}
