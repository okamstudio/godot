/**************************************************************************/
/*  lightmap_gi_editor_plugin.cpp                                         */
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

#include "lightmap_gi_editor_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"

#include "modules/modules_enabled.gen.h" // For lightmapper_rd.

void LightmapGIEditorPlugin::_bake_select_file(const String &p_file) {
	if (lightmap) {
		LightmapGI::BakeError err = LightmapGI::BAKE_ERROR_OK;
		const uint64_t time_started = OS::get_singleton()->get_ticks_msec();
		if (get_tree()->get_edited_scene_root()) {
			Ref<LightmapGIData> lightmapGIData = lightmap->get_light_data();

			if (lightmapGIData.is_valid()) {
				String path = lightmapGIData->get_path();
				if (!path.is_resource_file()) {
					int srpos = path.find("::");
					if (srpos != -1) {
						String base = path.substr(0, srpos);
						if (ResourceLoader::get_resource_type(base) == "PackedScene") {
							if (!get_tree()->get_edited_scene_root() || get_tree()->get_edited_scene_root()->get_scene_file_path() != base) {
								err = LightmapGI::BAKE_ERROR_FOREIGN_DATA;
							}
						} else {
							if (FileAccess::exists(base + ".import")) {
								err = LightmapGI::BAKE_ERROR_FOREIGN_DATA;
							}
						}
					}
				} else {
					if (FileAccess::exists(path + ".import")) {
						err = LightmapGI::BAKE_ERROR_FOREIGN_DATA;
					}
				}
			}

			if (err == LightmapGI::BAKE_ERROR_OK) {
				if (get_tree()->get_edited_scene_root() == lightmap) {
					err = lightmap->bake(lightmap, p_file, bake_func_step);
				} else {
					err = lightmap->bake(lightmap->get_parent(), p_file, bake_func_step);
				}
			}
		} else {
			err = LightmapGI::BAKE_ERROR_NO_SCENE_ROOT;
		}

		bake_func_end(time_started);

		switch (err) {
			case LightmapGI::BAKE_ERROR_NO_SAVE_PATH: {
				String scene_path = lightmap->get_scene_file_path();
				if (scene_path.is_empty() && lightmap->get_owner()) {
					scene_path = lightmap->get_owner()->get_scene_file_path();
				}
				if (scene_path.is_empty()) {
					EditorNode::get_singleton()->show_warning(TTR("Can't determine a save path for lightmap images.\nSave your scene and try again."));
					break;
				}
				scene_path = scene_path.get_basename() + ".lmbake";

				file_dialog->set_current_path(scene_path);
				file_dialog->popup_file_dialog();
			} break;
			case LightmapGI::BAKE_ERROR_NO_MESHES: {
				EditorNode::get_singleton()->show_warning(
						TTR("No meshes with lightmapping support to bake. Make sure they contain UV2 data and their Global Illumination property is set to Static.") +
						String::utf8("\n\n•  ") + TTR("To import a scene with lightmapping support, set Meshes > Light Baking to Static Lightmaps in the Import dock.") +
						String::utf8("\n•  ") + TTR("To enable lightmapping support on a primitive mesh, edit the PrimitiveMesh resource in the inspector and check Add UV2.") +
						String::utf8("\n•  ") + TTR("To enable lightmapping support on a CSG mesh, select the root CSG node and choose CSG > Bake Mesh Instance at the top of the 3D editor viewport.\nSelect the generated MeshInstance3D node and choose Mesh > Unwrap UV2 for Lightmap/AO at the top of the 3D editor viewport."));
			} break;
			case LightmapGI::BAKE_ERROR_CANT_CREATE_IMAGE: {
				EditorNode::get_singleton()->show_warning(TTR("Failed creating lightmap images. Make sure the lightmap destination path is writable."));
			} break;
			case LightmapGI::BAKE_ERROR_NO_SCENE_ROOT: {
				EditorNode::get_singleton()->show_warning(TTR("No editor scene root found."));
			} break;
			case LightmapGI::BAKE_ERROR_FOREIGN_DATA: {
				EditorNode::get_singleton()->show_warning(TTR("Lightmap data is not local to the scene."));
			} break;
			case LightmapGI::BAKE_ERROR_TEXTURE_SIZE_TOO_SMALL: {
				EditorNode::get_singleton()->show_warning(TTR("Maximum texture size is too small for the lightmap images.\nWhile this can be fixed by increasing the maximum texture size, it is recommended you split the scene into more objects instead."));
			} break;
			case LightmapGI::BAKE_ERROR_LIGHTMAP_TOO_SMALL: {
				EditorNode::get_singleton()->show_warning(TTR("Failed creating lightmap images. Make sure all meshes to bake have the Lightmap Size Hint property set high enough, and the LightmapGI's Texel Scale value is not too low."));
			} break;
			case LightmapGI::BAKE_ERROR_ATLAS_TOO_SMALL: {
				EditorNode::get_singleton()->show_warning(TTR("Failed fitting a lightmap image into an atlas. This should never happen and should be reported."));
			} break;
			default: {
			} break;
		}
	}
}

void LightmapGIEditorPlugin::_bake() {
	_bake_select_file("");
}

void LightmapGIEditorPlugin::edit(Object *p_object) {
	LightmapGI *s = Object::cast_to<LightmapGI>(p_object);
	if (!s) {
		return;
	}

	lightmap = s;
}

bool LightmapGIEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("LightmapGI");
}

void LightmapGIEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		bake->show();
	} else {
		bake->hide();
	}
}

EditorProgress *LightmapGIEditorPlugin::tmp_progress = nullptr;

bool LightmapGIEditorPlugin::bake_func_step(float p_progress, const String &p_description, void *, bool p_refresh) {
	if (!tmp_progress) {
		tmp_progress = memnew(EditorProgress("bake_lightmaps", TTR("Bake Lightmaps"), 1000, true));
		ERR_FAIL_NULL_V(tmp_progress, false);
	}
	return tmp_progress->step(p_description, p_progress * 1000, p_refresh);
}

void LightmapGIEditorPlugin::bake_func_end(uint64_t p_time_started) {
	if (tmp_progress != nullptr) {
		memdelete(tmp_progress);
		tmp_progress = nullptr;
	}

	const int time_taken = (OS::get_singleton()->get_ticks_msec() - p_time_started) * 0.001;
	print_line(vformat("Done baking lightmaps in %02d:%02d:%02d.", time_taken / 3600, (time_taken % 3600) / 60, time_taken % 60));
	// Request attention in case the user was doing something else.
	// Baking lightmaps is likely the editor task that can take the most time,
	// so only request the attention for baking lightmaps.
	DisplayServer::get_singleton()->window_request_attention();
}

void LightmapGIEditorPlugin::_bind_methods() {
	ClassDB::bind_method("_bake", &LightmapGIEditorPlugin::_bake);
}

LightmapGIEditorPlugin::LightmapGIEditorPlugin() {
	bake = memnew(Button);
	bake->set_theme_type_variation(SceneStringName(FlatButton));
	// TODO: Rework this as a dedicated toolbar control so we can hook into theme changes and update it
	// when the editor theme updates.
	bake->set_button_icon(EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("Bake"), EditorStringName(EditorIcons)));
	bake->set_text(TTR("Bake Lightmaps"));

#ifdef MODULE_LIGHTMAPPER_RD_ENABLED
	// Disable lightmap baking if not supported on the current GPU.
	if (!DisplayServer::get_singleton()->can_create_rendering_device()) {
		bake->set_disabled(true);
		bake->set_tooltip_text(vformat(TTR("Lightmap baking is not supported on this GPU (%s)."), RenderingServer::get_singleton()->get_video_adapter_name()));
	}
#else
	// Disable lightmap baking if the module is disabled at compile-time.
	bake->set_disabled(true);
#if defined(ANDROID_ENABLED) || defined(IOS_ENABLED)
	bake->set_tooltip_text(vformat(TTR("Lightmaps cannot be baked on %s."), OS::get_singleton()->get_name()));
#else
	bake->set_tooltip_text(TTR("Lightmaps cannot be baked, as the `lightmapper_rd` module was disabled at compile-time."));
#endif
#endif // MODULE_LIGHTMAPPER_RD_ENABLED

	bake->hide();
	bake->connect(SceneStringName(pressed), Callable(this, "_bake"));
	add_control_to_container(CONTAINER_SPATIAL_EDITOR_MENU, bake);
	lightmap = nullptr;

	file_dialog = memnew(EditorFileDialog);
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_dialog->add_filter("*.lmbake", TTR("LightMap Bake"));
	file_dialog->set_title(TTR("Select lightmap bake file:"));
	file_dialog->connect("file_selected", callable_mp(this, &LightmapGIEditorPlugin::_bake_select_file));
	bake->add_child(file_dialog);
}

LightmapGIEditorPlugin::~LightmapGIEditorPlugin() {
}
