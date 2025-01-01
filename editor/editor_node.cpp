/**************************************************************************/
/*  editor_node.cpp                                                       */
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

#include "editor_node.h"

#include "core/config/project_settings.h"
#include "core/extension/gdextension_manager.h"
#include "core/input/input.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/string/print_string.h"
#include "core/string/translation_server.h"
#include "core/version.h"
#include "editor/editor_string_names.h"
#include "editor/plugins/editor_context_menu_plugin.h"
#include "main/main.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/bone_attachment_3d.h"
#include "scene/animation/animation_tree.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/menu_bar.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/popup.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/main/window.h"
#include "scene/property_utils.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/portable_compressed_texture.h"
#include "scene/theme/theme_db.h"
#include "servers/display_server.h"
#include "servers/navigation_server_3d.h"
#include "servers/physics_server_2d.h"
#include "servers/rendering_server.h"

#include "editor/audio_stream_preview.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/dependency_editor.h"
#include "editor/editor_about.h"
#include "editor/editor_audio_buses.h"
#include "editor/editor_build_profile.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_data.h"
#include "editor/editor_dock_manager.h"
#include "editor/editor_feature_profile.h"
#include "editor/editor_folding.h"
#include "editor/editor_help.h"
#include "editor/editor_inspector.h"
#include "editor/editor_interface.h"
#include "editor/editor_layouts_dialog.h"
#include "editor/editor_log.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_native_shader_source_visualizer.h"
#include "editor/editor_paths.h"
#include "editor/editor_properties.h"
#include "editor/editor_property_name_processor.h"
#include "editor/editor_resource_picker.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/editor_settings_dialog.h"
#include "editor/editor_translation_parser.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/export/editor_export.h"
#include "editor/export/export_template_manager.h"
#include "editor/export/project_export.h"
#include "editor/fbx_importer_manager.h"
#include "editor/filesystem_dock.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_quick_open_dialog.h"
#include "editor/gui/editor_run_bar.h"
#include "editor/gui/editor_scene_tabs.h"
#include "editor/gui/editor_title_bar.h"
#include "editor/gui/editor_toaster.h"
#include "editor/history_dock.h"
#include "editor/import/3d/editor_import_collada.h"
#include "editor/import/3d/resource_importer_obj.h"
#include "editor/import/3d/resource_importer_scene.h"
#include "editor/import/3d/scene_import_settings.h"
#include "editor/import/audio_stream_import_settings.h"
#include "editor/import/dynamic_font_import_settings.h"
#include "editor/import/resource_importer_bitmask.h"
#include "editor/import/resource_importer_bmfont.h"
#include "editor/import/resource_importer_csv_translation.h"
#include "editor/import/resource_importer_dynamic_font.h"
#include "editor/import/resource_importer_image.h"
#include "editor/import/resource_importer_imagefont.h"
#include "editor/import/resource_importer_layered_texture.h"
#include "editor/import/resource_importer_shader_file.h"
#include "editor/import/resource_importer_texture.h"
#include "editor/import/resource_importer_texture_atlas.h"
#include "editor/import/resource_importer_wav.h"
#include "editor/import_dock.h"
#include "editor/inspector_dock.h"
#include "editor/multi_node_edit.h"
#include "editor/node_dock.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "editor/plugins/asset_library_editor_plugin.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/plugins/debugger_editor_plugin.h"
#include "editor/plugins/dedicated_server_export_plugin.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/editor_preview_plugins.h"
#include "editor/plugins/editor_resource_conversion_plugin.h"
#include "editor/plugins/game_view_plugin.h"
#include "editor/plugins/gdextension_export_plugin.h"
#include "editor/plugins/material_editor_plugin.h"
#include "editor/plugins/mesh_library_editor_plugin.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "editor/plugins/packed_scene_translation_parser_plugin.h"
#include "editor/plugins/particle_process_material_editor_plugin.h"
#include "editor/plugins/plugin_config_dialog.h"
#include "editor/plugins/root_motion_editor_plugin.h"
#include "editor/plugins/script_text_editor.h"
#include "editor/plugins/text_editor.h"
#include "editor/plugins/version_control_editor_plugin.h"
#include "editor/plugins/visual_shader_editor_plugin.h"
#include "editor/progress_dialog.h"
#include "editor/project_settings_editor.h"
#include "editor/register_exporters.h"
#include "editor/scene_tree_dock.h"
#include "editor/surface_upgrade_tool.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "editor/window_wrapper.h"

#include <stdlib.h>

#include "modules/modules_enabled.gen.h" // For gdscript, mono.

EditorNode *EditorNode::singleton = nullptr;

static const String EDITOR_NODE_CONFIG_SECTION = "EditorNode";

static const String REMOVE_ANDROID_BUILD_TEMPLATE_MESSAGE = "The Android build template is already installed in this project and it won't be overwritten.\nRemove the \"%s\" directory manually before attempting this operation again.";
static const String INSTALL_ANDROID_BUILD_TEMPLATE_MESSAGE = "This will set up your project for gradle Android builds by installing the source template to \"%s\".\nNote that in order to make gradle builds instead of using pre-built APKs, the \"Use Gradle Build\" option should be enabled in the Android export preset.";

bool EditorProgress::step(const String &p_state, int p_step, bool p_force_refresh) {
	if (!force_background && Thread::is_main_thread()) {
		return EditorNode::progress_task_step(task, p_state, p_step, p_force_refresh);
	} else {
		EditorNode::progress_task_step_bg(task, p_step);
		return false;
	}
}

EditorProgress::EditorProgress(const String &p_task, const String &p_label, int p_amount, bool p_can_cancel, bool p_force_background) {
	if (!p_force_background && Thread::is_main_thread()) {
		EditorNode::progress_add_task(p_task, p_label, p_amount, p_can_cancel);
	} else {
		EditorNode::progress_add_task_bg(p_task, p_label, p_amount);
	}
	task = p_task;
	force_background = p_force_background;
}

EditorProgress::~EditorProgress() {
	if (!force_background && Thread::is_main_thread()) {
		EditorNode::progress_end_task(task);
	} else {
		EditorNode::progress_end_task_bg(task);
	}
}

void EditorNode::disambiguate_filenames(const Vector<String> p_full_paths, Vector<String> &r_filenames) {
	ERR_FAIL_COND_MSG(p_full_paths.size() != r_filenames.size(), vformat("disambiguate_filenames requires two string vectors of same length (%d != %d).", p_full_paths.size(), r_filenames.size()));

	// Keep track of a list of "index sets," i.e. sets of indices
	// within disambiguated_scene_names which contain the same name.
	Vector<RBSet<int>> index_sets;
	HashMap<String, int> scene_name_to_set_index;
	for (int i = 0; i < r_filenames.size(); i++) {
		const String &scene_name = r_filenames[i];
		if (!scene_name_to_set_index.has(scene_name)) {
			index_sets.append(RBSet<int>());
			scene_name_to_set_index.insert(r_filenames[i], index_sets.size() - 1);
		}
		index_sets.write[scene_name_to_set_index[scene_name]].insert(i);
	}

	// For each index set with a size > 1, we need to disambiguate.
	for (int i = 0; i < index_sets.size(); i++) {
		RBSet<int> iset = index_sets[i];
		while (iset.size() > 1) {
			// Append the parent folder to each scene name.
			for (const int &E : iset) {
				int set_idx = E;
				String scene_name = r_filenames[set_idx];
				String full_path = p_full_paths[set_idx];

				// Get rid of file extensions and res:// prefixes.
				scene_name = scene_name.get_basename();
				if (full_path.begins_with("res://")) {
					full_path = full_path.substr(6);
				}
				full_path = full_path.get_basename();

				// Normalize trailing slashes when normalizing directory names.
				scene_name = scene_name.trim_suffix("/");
				full_path = full_path.trim_suffix("/");

				int scene_name_size = scene_name.size();
				int full_path_size = full_path.size();
				int difference = full_path_size - scene_name_size;

				// Find just the parent folder of the current path and append it.
				// If the current name is foo.tscn, and the full path is /some/folder/foo.tscn
				// then slash_idx is the second '/', so that we select just "folder", and
				// append that to yield "folder/foo.tscn".
				if (difference > 0) {
					String parent = full_path.substr(0, difference);
					int slash_idx = parent.rfind_char('/');
					slash_idx = parent.rfind_char('/', slash_idx - 1);
					parent = (slash_idx >= 0 && parent.length() > 1) ? parent.substr(slash_idx + 1) : parent;
					r_filenames.write[set_idx] = parent + r_filenames[set_idx];
				}
			}

			// Loop back through scene names and remove non-ambiguous names.
			bool can_proceed = false;
			RBSet<int>::Element *E = iset.front();
			while (E) {
				String scene_name = r_filenames[E->get()];
				bool duplicate_found = false;
				for (const int &F : iset) {
					if (E->get() == F) {
						continue;
					}
					const String &other_scene_name = r_filenames[F];
					if (other_scene_name == scene_name) {
						duplicate_found = true;
						break;
					}
				}

				RBSet<int>::Element *to_erase = duplicate_found ? nullptr : E;

				// We need to check that we could actually append anymore names
				// if we wanted to for disambiguation. If we can't, then we have
				// to abort even with ambiguous names. We clean the full path
				// and the scene name first to remove extensions so that this
				// comparison actually works.
				String path = p_full_paths[E->get()];

				// Get rid of file extensions and res:// prefixes.
				scene_name = scene_name.get_basename();
				if (path.begins_with("res://")) {
					path = path.substr(6);
				}
				path = path.get_basename();

				// Normalize trailing slashes when normalizing directory names.
				scene_name = scene_name.trim_suffix("/");
				path = path.trim_suffix("/");

				// We can proceed if the full path is longer than the scene name,
				// meaning that there is at least one more parent folder we can
				// tack onto the name.
				can_proceed = can_proceed || (path.size() - scene_name.size()) >= 1;

				E = E->next();
				if (to_erase) {
					iset.erase(to_erase);
				}
			}

			if (!can_proceed) {
				break;
			}
		}
	}
}

void EditorNode::_version_control_menu_option(int p_idx) {
	switch (vcs_actions_menu->get_item_id(p_idx)) {
		case RUN_VCS_METADATA: {
			VersionControlEditorPlugin::get_singleton()->popup_vcs_metadata_dialog();
		} break;
		case RUN_VCS_SETTINGS: {
			VersionControlEditorPlugin::get_singleton()->popup_vcs_set_up_dialog(gui_base);
		} break;
	}
}

void EditorNode::_update_title() {
	const String appname = GLOBAL_GET("application/config/name");
	String title = (appname.is_empty() ? TTR("Unnamed Project") : appname);
	const String edited = editor_data.get_edited_scene_root() ? editor_data.get_edited_scene_root()->get_scene_file_path() : String();
	if (!edited.is_empty()) {
		// Display the edited scene name before the program name so that it can be seen in the OS task bar.
		title = vformat("%s - %s", edited.get_file(), title);
	}
	if (unsaved_cache) {
		// Display the "modified" mark before anything else so that it can always be seen in the OS task bar.
		title = vformat("(*) %s", title);
	}
	DisplayServer::get_singleton()->window_set_title(title + String(" - ") + VERSION_NAME);
	if (project_title) {
		project_title->set_text(title);
	}
}

void EditorNode::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventKey> k = p_event;
	if ((k.is_valid() && k->is_pressed() && !k->is_echo()) || Object::cast_to<InputEventShortcut>(*p_event)) {
		bool is_handled = true;
		if (ED_IS_SHORTCUT("editor/filter_files", p_event)) {
			FileSystemDock::get_singleton()->focus_on_filter();
		} else if (ED_IS_SHORTCUT("editor/editor_2d", p_event)) {
			editor_main_screen->select(EditorMainScreen::EDITOR_2D);
		} else if (ED_IS_SHORTCUT("editor/editor_3d", p_event)) {
			editor_main_screen->select(EditorMainScreen::EDITOR_3D);
		} else if (ED_IS_SHORTCUT("editor/editor_script", p_event)) {
			editor_main_screen->select(EditorMainScreen::EDITOR_SCRIPT);
		} else if (ED_IS_SHORTCUT("editor/editor_game", p_event)) {
			editor_main_screen->select(EditorMainScreen::EDITOR_GAME);
		} else if (ED_IS_SHORTCUT("editor/editor_help", p_event)) {
			emit_signal(SNAME("request_help_search"), "");
		} else if (ED_IS_SHORTCUT("editor/editor_assetlib", p_event) && AssetLibraryEditorPlugin::is_available()) {
			editor_main_screen->select(EditorMainScreen::EDITOR_ASSETLIB);
		} else if (ED_IS_SHORTCUT("editor/editor_next", p_event)) {
			editor_main_screen->select_next();
		} else if (ED_IS_SHORTCUT("editor/editor_prev", p_event)) {
			editor_main_screen->select_prev();
		} else if (ED_IS_SHORTCUT("editor/command_palette", p_event)) {
			_open_command_palette();
		} else if (ED_IS_SHORTCUT("editor/toggle_last_opened_bottom_panel", p_event)) {
			bottom_panel->toggle_last_opened_bottom_panel();
		} else {
			is_handled = false;
		}

		if (is_handled) {
			get_tree()->get_root()->set_input_as_handled();
		}
	}
}

void EditorNode::_update_vsync_mode() {
	const DisplayServer::VSyncMode window_vsync_mode = DisplayServer::VSyncMode(int(EDITOR_GET("interface/editor/vsync_mode")));
	DisplayServer::get_singleton()->window_set_vsync_mode(window_vsync_mode);
}

void EditorNode::_update_from_settings() {
	_update_title();

	int current_filter = GLOBAL_GET("rendering/textures/canvas_textures/default_texture_filter");
	if (current_filter != scene_root->get_default_canvas_item_texture_filter()) {
		Viewport::DefaultCanvasItemTextureFilter tf = (Viewport::DefaultCanvasItemTextureFilter)current_filter;
		scene_root->set_default_canvas_item_texture_filter(tf);
	}
	int current_repeat = GLOBAL_GET("rendering/textures/canvas_textures/default_texture_repeat");
	if (current_repeat != scene_root->get_default_canvas_item_texture_repeat()) {
		Viewport::DefaultCanvasItemTextureRepeat tr = (Viewport::DefaultCanvasItemTextureRepeat)current_repeat;
		scene_root->set_default_canvas_item_texture_repeat(tr);
	}

	RS::DOFBokehShape dof_shape = RS::DOFBokehShape(int(GLOBAL_GET("rendering/camera/depth_of_field/depth_of_field_bokeh_shape")));
	RS::get_singleton()->camera_attributes_set_dof_blur_bokeh_shape(dof_shape);
	RS::DOFBlurQuality dof_quality = RS::DOFBlurQuality(int(GLOBAL_GET("rendering/camera/depth_of_field/depth_of_field_bokeh_quality")));
	bool dof_jitter = GLOBAL_GET("rendering/camera/depth_of_field/depth_of_field_use_jitter");
	RS::get_singleton()->camera_attributes_set_dof_blur_quality(dof_quality, dof_jitter);
	RS::get_singleton()->environment_set_ssao_quality(RS::EnvironmentSSAOQuality(int(GLOBAL_GET("rendering/environment/ssao/quality"))), GLOBAL_GET("rendering/environment/ssao/half_size"), GLOBAL_GET("rendering/environment/ssao/adaptive_target"), GLOBAL_GET("rendering/environment/ssao/blur_passes"), GLOBAL_GET("rendering/environment/ssao/fadeout_from"), GLOBAL_GET("rendering/environment/ssao/fadeout_to"));
	RS::get_singleton()->screen_space_roughness_limiter_set_active(GLOBAL_GET("rendering/anti_aliasing/screen_space_roughness_limiter/enabled"), GLOBAL_GET("rendering/anti_aliasing/screen_space_roughness_limiter/amount"), GLOBAL_GET("rendering/anti_aliasing/screen_space_roughness_limiter/limit"));
	bool glow_bicubic = int(GLOBAL_GET("rendering/environment/glow/upscale_mode")) > 0;
	RS::get_singleton()->environment_set_ssil_quality(RS::EnvironmentSSILQuality(int(GLOBAL_GET("rendering/environment/ssil/quality"))), GLOBAL_GET("rendering/environment/ssil/half_size"), GLOBAL_GET("rendering/environment/ssil/adaptive_target"), GLOBAL_GET("rendering/environment/ssil/blur_passes"), GLOBAL_GET("rendering/environment/ssil/fadeout_from"), GLOBAL_GET("rendering/environment/ssil/fadeout_to"));
	RS::get_singleton()->environment_glow_set_use_bicubic_upscale(glow_bicubic);
	RS::EnvironmentSSRRoughnessQuality ssr_roughness_quality = RS::EnvironmentSSRRoughnessQuality(int(GLOBAL_GET("rendering/environment/screen_space_reflection/roughness_quality")));
	RS::get_singleton()->environment_set_ssr_roughness_quality(ssr_roughness_quality);
	RS::SubSurfaceScatteringQuality sss_quality = RS::SubSurfaceScatteringQuality(int(GLOBAL_GET("rendering/environment/subsurface_scattering/subsurface_scattering_quality")));
	RS::get_singleton()->sub_surface_scattering_set_quality(sss_quality);
	float sss_scale = GLOBAL_GET("rendering/environment/subsurface_scattering/subsurface_scattering_scale");
	float sss_depth_scale = GLOBAL_GET("rendering/environment/subsurface_scattering/subsurface_scattering_depth_scale");
	RS::get_singleton()->sub_surface_scattering_set_scale(sss_scale, sss_depth_scale);

	uint32_t directional_shadow_size = GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/size");
	uint32_t directional_shadow_16_bits = GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/16_bits");
	RS::get_singleton()->directional_shadow_atlas_set_size(directional_shadow_size, directional_shadow_16_bits);

	RS::ShadowQuality shadows_quality = RS::ShadowQuality(int(GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality")));
	RS::get_singleton()->positional_soft_shadow_filter_set_quality(shadows_quality);
	RS::ShadowQuality directional_shadow_quality = RS::ShadowQuality(int(GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality")));
	RS::get_singleton()->directional_soft_shadow_filter_set_quality(directional_shadow_quality);
	float probe_update_speed = GLOBAL_GET("rendering/lightmapping/probe_capture/update_speed");
	RS::get_singleton()->lightmap_set_probe_capture_update_speed(probe_update_speed);
	RS::EnvironmentSDFGIFramesToConverge frames_to_converge = RS::EnvironmentSDFGIFramesToConverge(int(GLOBAL_GET("rendering/global_illumination/sdfgi/frames_to_converge")));
	RS::get_singleton()->environment_set_sdfgi_frames_to_converge(frames_to_converge);
	RS::EnvironmentSDFGIRayCount ray_count = RS::EnvironmentSDFGIRayCount(int(GLOBAL_GET("rendering/global_illumination/sdfgi/probe_ray_count")));
	RS::get_singleton()->environment_set_sdfgi_ray_count(ray_count);
	RS::VoxelGIQuality voxel_gi_quality = RS::VoxelGIQuality(int(GLOBAL_GET("rendering/global_illumination/voxel_gi/quality")));
	RS::get_singleton()->voxel_gi_set_quality(voxel_gi_quality);
	RS::get_singleton()->environment_set_volumetric_fog_volume_size(GLOBAL_GET("rendering/environment/volumetric_fog/volume_size"), GLOBAL_GET("rendering/environment/volumetric_fog/volume_depth"));
	RS::get_singleton()->environment_set_volumetric_fog_filter_active(bool(GLOBAL_GET("rendering/environment/volumetric_fog/use_filter")));
	RS::get_singleton()->canvas_set_shadow_texture_size(GLOBAL_GET("rendering/2d/shadow_atlas/size"));

	bool use_half_res_gi = GLOBAL_GET("rendering/global_illumination/gi/use_half_resolution");
	RS::get_singleton()->gi_set_use_half_resolution(use_half_res_gi);

	bool snap_2d_transforms = GLOBAL_GET("rendering/2d/snap/snap_2d_transforms_to_pixel");
	scene_root->set_snap_2d_transforms_to_pixel(snap_2d_transforms);
	bool snap_2d_vertices = GLOBAL_GET("rendering/2d/snap/snap_2d_vertices_to_pixel");
	scene_root->set_snap_2d_vertices_to_pixel(snap_2d_vertices);

	Viewport::SDFOversize sdf_oversize = Viewport::SDFOversize(int(GLOBAL_GET("rendering/2d/sdf/oversize")));
	scene_root->set_sdf_oversize(sdf_oversize);
	Viewport::SDFScale sdf_scale = Viewport::SDFScale(int(GLOBAL_GET("rendering/2d/sdf/scale")));
	scene_root->set_sdf_scale(sdf_scale);

	Viewport::MSAA msaa = Viewport::MSAA(int(GLOBAL_GET("rendering/anti_aliasing/quality/msaa_2d")));
	scene_root->set_msaa_2d(msaa);

	bool use_hdr_2d = GLOBAL_GET("rendering/viewport/hdr_2d");
	scene_root->set_use_hdr_2d(use_hdr_2d);

	float mesh_lod_threshold = GLOBAL_GET("rendering/mesh_lod/lod_change/threshold_pixels");
	scene_root->set_mesh_lod_threshold(mesh_lod_threshold);

	RS::get_singleton()->decals_set_filter(RS::DecalFilter(int(GLOBAL_GET("rendering/textures/decals/filter"))));
	RS::get_singleton()->light_projectors_set_filter(RS::LightProjectorFilter(int(GLOBAL_GET("rendering/textures/light_projectors/filter"))));
	RS::get_singleton()->lightmaps_set_bicubic_filter(GLOBAL_GET("rendering/lightmapping/lightmap_gi/use_bicubic_filter"));

	SceneTree *tree = get_tree();
	tree->set_debug_collisions_color(GLOBAL_GET("debug/shapes/collision/shape_color"));
	tree->set_debug_collision_contact_color(GLOBAL_GET("debug/shapes/collision/contact_color"));

	ResourceImporterTexture::get_singleton()->update_imports();

#ifdef DEBUG_ENABLED
	NavigationServer3D::get_singleton()->set_debug_navigation_edge_connection_color(GLOBAL_GET("debug/shapes/navigation/edge_connection_color"));
	NavigationServer3D::get_singleton()->set_debug_navigation_geometry_edge_color(GLOBAL_GET("debug/shapes/navigation/geometry_edge_color"));
	NavigationServer3D::get_singleton()->set_debug_navigation_geometry_face_color(GLOBAL_GET("debug/shapes/navigation/geometry_face_color"));
	NavigationServer3D::get_singleton()->set_debug_navigation_geometry_edge_disabled_color(GLOBAL_GET("debug/shapes/navigation/geometry_edge_disabled_color"));
	NavigationServer3D::get_singleton()->set_debug_navigation_geometry_face_disabled_color(GLOBAL_GET("debug/shapes/navigation/geometry_face_disabled_color"));
	NavigationServer3D::get_singleton()->set_debug_navigation_enable_edge_connections(GLOBAL_GET("debug/shapes/navigation/enable_edge_connections"));
	NavigationServer3D::get_singleton()->set_debug_navigation_enable_edge_connections_xray(GLOBAL_GET("debug/shapes/navigation/enable_edge_connections_xray"));
	NavigationServer3D::get_singleton()->set_debug_navigation_enable_edge_lines(GLOBAL_GET("debug/shapes/navigation/enable_edge_lines"));
	NavigationServer3D::get_singleton()->set_debug_navigation_enable_edge_lines_xray(GLOBAL_GET("debug/shapes/navigation/enable_edge_lines_xray"));
	NavigationServer3D::get_singleton()->set_debug_navigation_enable_geometry_face_random_color(GLOBAL_GET("debug/shapes/navigation/enable_geometry_face_random_color"));
#endif // DEBUG_ENABLED
}

void EditorNode::_gdextensions_reloaded() {
	// In case the developer is inspecting an object that will be changed by the reload.
	InspectorDock::get_inspector_singleton()->update_tree();

	// Reload script editor to revalidate GDScript if classes are added or removed.
	ScriptEditor::get_singleton()->reload_scripts(true);

	// Regenerate documentation.
	EditorHelp::generate_doc();
}

void EditorNode::_update_theme(bool p_skip_creation) {
	if (!p_skip_creation) {
		theme = EditorThemeManager::generate_theme(theme);
		DisplayServer::set_early_window_clear_color_override(true, theme->get_color(SNAME("background"), EditorStringName(Editor)));
	}

	Vector<Ref<Theme>> editor_themes;
	editor_themes.push_back(theme);
	editor_themes.push_back(ThemeDB::get_singleton()->get_default_theme());

	ThemeContext *node_tc = ThemeDB::get_singleton()->get_theme_context(this);
	if (node_tc) {
		node_tc->set_themes(editor_themes);
	} else {
		ThemeDB::get_singleton()->create_theme_context(this, editor_themes);
	}

	Window *window = get_window();
	if (window) {
		ThemeContext *window_tc = ThemeDB::get_singleton()->get_theme_context(window);
		if (window_tc) {
			window_tc->set_themes(editor_themes);
		} else {
			ThemeDB::get_singleton()->create_theme_context(window, editor_themes);
		}
	}

	if (CanvasItemEditor::get_singleton()->get_theme_preview() == CanvasItemEditor::THEME_PREVIEW_EDITOR) {
		update_preview_themes(CanvasItemEditor::THEME_PREVIEW_EDITOR);
	}

	// Update styles.
	{
		gui_base->add_theme_style_override(SceneStringName(panel), theme->get_stylebox(SNAME("Background"), EditorStringName(EditorStyles)));
		main_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, theme->get_constant(SNAME("window_border_margin"), EditorStringName(Editor)));
		main_vbox->add_theme_constant_override("separation", theme->get_constant(SNAME("top_bar_separation"), EditorStringName(Editor)));

		editor_main_screen->add_theme_style_override(SceneStringName(panel), theme->get_stylebox(SNAME("Content"), EditorStringName(EditorStyles)));
		bottom_panel->add_theme_style_override(SceneStringName(panel), theme->get_stylebox(SNAME("BottomPanel"), EditorStringName(EditorStyles)));
		distraction_free->set_button_icon(theme->get_icon(SNAME("DistractionFree"), EditorStringName(EditorIcons)));
		distraction_free->add_theme_style_override(SceneStringName(pressed), theme->get_stylebox(CoreStringName(normal), "FlatMenuButton"));

		help_menu->set_item_icon(help_menu->get_item_index(HELP_SEARCH), theme->get_icon(SNAME("HelpSearch"), EditorStringName(EditorIcons)));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_COPY_SYSTEM_INFO), theme->get_icon(SNAME("ActionCopy"), EditorStringName(EditorIcons)));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_ABOUT), theme->get_icon(SNAME("Godot"), EditorStringName(EditorIcons)));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_SUPPORT_GODOT_DEVELOPMENT), theme->get_icon(SNAME("Heart"), EditorStringName(EditorIcons)));

		if (EditorDebuggerNode::get_singleton()->is_visible()) {
			bottom_panel->add_theme_style_override(SceneStringName(panel), theme->get_stylebox(SNAME("BottomPanelDebuggerOverride"), EditorStringName(EditorStyles)));
		}

		_update_renderer_color();
	}

	editor_dock_manager->update_tab_styles();
	editor_dock_manager->set_tab_icon_max_width(theme->get_constant(SNAME("class_icon_size"), EditorStringName(Editor)));
}

void EditorNode::update_preview_themes(int p_mode) {
	if (!scene_root->is_inside_tree()) {
		return; // Too early.
	}

	Vector<Ref<Theme>> preview_themes;

	switch (p_mode) {
		case CanvasItemEditor::THEME_PREVIEW_PROJECT:
			preview_themes.push_back(ThemeDB::get_singleton()->get_project_theme());
			break;

		case CanvasItemEditor::THEME_PREVIEW_EDITOR:
			preview_themes.push_back(get_editor_theme());
			break;

		default:
			break;
	}

	preview_themes.push_back(ThemeDB::get_singleton()->get_default_theme());

	ThemeContext *preview_context = ThemeDB::get_singleton()->get_theme_context(scene_root);
	if (preview_context) {
		preview_context->set_themes(preview_themes);
	} else {
		ThemeDB::get_singleton()->create_theme_context(scene_root, preview_themes);
	}
}

void EditorNode::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			EditorHelp::generate_doc();
#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
			EditorHelpHighlighter::create_singleton();
#endif
		} break;

		case NOTIFICATION_PROCESS: {
			if (opening_prev && !confirmation->is_visible()) {
				opening_prev = false;
			}

			bool global_unsaved = EditorUndoRedoManager::get_singleton()->is_history_unsaved(EditorUndoRedoManager::GLOBAL_HISTORY);
			bool scene_or_global_unsaved = global_unsaved || EditorUndoRedoManager::get_singleton()->is_history_unsaved(editor_data.get_current_edited_scene_history_id());
			if (unsaved_cache != scene_or_global_unsaved) {
				unsaved_cache = scene_or_global_unsaved;
				_update_title();
			}

			if (editor_data.is_scene_changed(-1)) {
				scene_tabs->update_scene_tabs();
			}

			// Update the animation frame of the update spinner.
			uint64_t frame = Engine::get_singleton()->get_frames_drawn();
			uint64_t tick = OS::get_singleton()->get_ticks_msec();

			if (frame != update_spinner_step_frame && (tick - update_spinner_step_msec) > (1000 / 8)) {
				update_spinner_step++;
				if (update_spinner_step >= 8) {
					update_spinner_step = 0;
				}

				update_spinner_step_msec = tick;
				update_spinner_step_frame = frame + 1;

				// Update the icon itself only when the spinner is visible.
				if (_should_display_update_spinner()) {
					update_spinner->set_button_icon(theme->get_icon("Progress" + itos(update_spinner_step + 1), EditorStringName(EditorIcons)));
				}
			}

			editor_selection->update();

			ResourceImporterTexture::get_singleton()->update_imports();

			if (requested_first_scan) {
				requested_first_scan = false;

				OS::get_singleton()->benchmark_begin_measure("Editor", "First Scan");

				if (run_surface_upgrade_tool) {
					run_surface_upgrade_tool = false;
					SurfaceUpgradeTool::get_singleton()->connect("upgrade_finished", callable_mp(EditorFileSystem::get_singleton(), &EditorFileSystem::scan), CONNECT_ONE_SHOT);
					SurfaceUpgradeTool::get_singleton()->finish_upgrade();
				} else {
					EditorFileSystem::get_singleton()->scan();
				}
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			get_tree()->set_disable_node_threading(true); // No node threading while running editor.

			Engine::get_singleton()->set_editor_hint(true);

			Window *window = get_window();
			if (window) {
				// Handle macOS fullscreen and extend-to-title changes.
				window->connect("titlebar_changed", callable_mp(this, &EditorNode::_titlebar_resized));
			}

			// Theme has already been created in the constructor, so we can skip that step.
			_update_theme(true);

			OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(int(EDITOR_GET("interface/editor/low_processor_mode_sleep_usec")));
			get_tree()->get_root()->set_as_audio_listener_3d(false);
			get_tree()->get_root()->set_as_audio_listener_2d(false);
			get_tree()->get_root()->set_snap_2d_transforms_to_pixel(false);
			get_tree()->get_root()->set_snap_2d_vertices_to_pixel(false);
			get_tree()->set_auto_accept_quit(false);
#ifdef ANDROID_ENABLED
			get_tree()->set_quit_on_go_back(false);
#endif
			get_tree()->get_root()->connect("files_dropped", callable_mp(this, &EditorNode::_dropped_files));

			command_palette->register_shortcuts_as_command();

			callable_mp(this, &EditorNode::_begin_first_scan).call_deferred();

			last_dark_mode_state = DisplayServer::get_singleton()->is_dark_mode();
			last_system_accent_color = DisplayServer::get_singleton()->get_accent_color();
			last_system_base_color = DisplayServer::get_singleton()->get_base_color();
			DisplayServer::get_singleton()->set_system_theme_change_callback(callable_mp(this, &EditorNode::_check_system_theme_changed));

			get_viewport()->connect("size_changed", callable_mp(this, &EditorNode::_viewport_resized));

			/* DO NOT LOAD SCENES HERE, WAIT FOR FILE SCANNING AND REIMPORT TO COMPLETE */
		} break;

		case NOTIFICATION_EXIT_TREE: {
			singleton->active_plugins.clear();

			if (progress_dialog) {
				progress_dialog->queue_free();
			}
			if (load_error_dialog) {
				load_error_dialog->queue_free();
			}
			if (execute_output_dialog) {
				execute_output_dialog->queue_free();
			}
			if (warning) {
				warning->queue_free();
			}
			if (accept) {
				accept->queue_free();
			}
			if (save_accept) {
				save_accept->queue_free();
			}
			editor_data.save_editor_external_data();
			FileAccess::set_file_close_fail_notify_callback(nullptr);
			log->deinit(); // Do not get messages anymore.
			editor_data.clear_edited_scenes();
			get_viewport()->disconnect("size_changed", callable_mp(this, &EditorNode::_viewport_resized));
		} break;

		case NOTIFICATION_READY: {
			started_timestamp = Time::get_singleton()->get_unix_time_from_system();

			RenderingServer::get_singleton()->viewport_set_disable_2d(get_scene_root()->get_viewport_rid(), true);
			RenderingServer::get_singleton()->viewport_set_environment_mode(get_viewport()->get_viewport_rid(), RenderingServer::VIEWPORT_ENVIRONMENT_DISABLED);
			DisplayServer::get_singleton()->screen_set_keep_on(EDITOR_GET("interface/editor/keep_screen_on"));

			feature_profile_manager->notify_changed();

			// Save the project after opening to mark it as last modified, except in headless mode.
			if (DisplayServer::get_singleton()->window_can_draw()) {
				ProjectSettings::get_singleton()->save();
			}

			_titlebar_resized();

			// Set up a theme context for the 2D preview viewport using the stored preview theme.
			CanvasItemEditor::ThemePreviewMode theme_preview_mode = (CanvasItemEditor::ThemePreviewMode)(int)EditorSettings::get_singleton()->get_project_metadata("2d_editor", "theme_preview", CanvasItemEditor::THEME_PREVIEW_PROJECT);
			update_preview_themes(theme_preview_mode);

			/* DO NOT LOAD SCENES HERE, WAIT FOR FILE SCANNING AND REIMPORT TO COMPLETE */
		} break;

		case NOTIFICATION_APPLICATION_FOCUS_IN: {
			// Restore the original FPS cap after focusing back on the editor.
			OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(int(EDITOR_GET("interface/editor/low_processor_mode_sleep_usec")));

			EditorFileSystem::get_singleton()->scan_changes();
			_scan_external_changes();

			GDExtensionManager *gdextension_manager = GDExtensionManager::get_singleton();
			callable_mp(gdextension_manager, &GDExtensionManager::reload_extensions).call_deferred();
		} break;

		case NOTIFICATION_APPLICATION_FOCUS_OUT: {
			// Save on focus loss before applying the FPS limit to avoid slowing down the saving process.
			if (EDITOR_GET("interface/editor/save_on_focus_loss")) {
				_menu_option_confirm(FILE_SAVE_SCENE_SILENTLY, false);
			}

			// Set a low FPS cap to decrease CPU/GPU usage while the editor is unfocused.
			if (unfocused_low_processor_usage_mode_enabled) {
				OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(int(EDITOR_GET("interface/editor/unfocused_low_processor_mode_sleep_usec")));
			}
		} break;

		case NOTIFICATION_WM_ABOUT: {
			show_about();
		} break;

		case NOTIFICATION_WM_CLOSE_REQUEST: {
			_menu_option_confirm(FILE_QUIT, false);
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("filesystem/file_dialog")) {
				FileDialog::set_default_show_hidden_files(EDITOR_GET("filesystem/file_dialog/show_hidden_files"));
				EditorFileDialog::set_default_show_hidden_files(EDITOR_GET("filesystem/file_dialog/show_hidden_files"));
				EditorFileDialog::set_default_display_mode((EditorFileDialog::DisplayMode)EDITOR_GET("filesystem/file_dialog/display_mode").operator int());
			}

			if (EDITOR_GET("interface/editor/import_resources_when_unfocused")) {
				scan_changes_timer->start();
			} else {
				scan_changes_timer->stop();
			}

			follow_system_theme = EDITOR_GET("interface/theme/follow_system_theme");
			use_system_accent_color = EDITOR_GET("interface/theme/use_system_accent_color");

			if (EditorThemeManager::is_generated_theme_outdated()) {
				_update_theme();
				_build_icon_type_cache();
				recent_scenes->reset_size();
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/dock_tab_style")) {
				editor_dock_manager->update_tab_styles();
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/scene_tabs")) {
				scene_tabs->update_scene_tabs();
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("docks/filesystem")) {
				HashSet<String> updated_textfile_extensions;
				HashSet<String> updated_other_file_extensions;
				bool extensions_match = true;
				const Vector<String> textfile_ext = ((String)(EDITOR_GET("docks/filesystem/textfile_extensions"))).split(",", false);
				for (const String &E : textfile_ext) {
					updated_textfile_extensions.insert(E);
					if (extensions_match && !textfile_extensions.has(E)) {
						extensions_match = false;
					}
				}
				const Vector<String> other_file_ext = ((String)(EDITOR_GET("docks/filesystem/other_file_extensions"))).split(",", false);
				for (const String &E : other_file_ext) {
					updated_other_file_extensions.insert(E);
					if (extensions_match && !other_file_extensions.has(E)) {
						extensions_match = false;
					}
				}

				if (!extensions_match || updated_textfile_extensions.size() < textfile_extensions.size() || updated_other_file_extensions.size() < other_file_extensions.size()) {
					textfile_extensions = updated_textfile_extensions;
					other_file_extensions = updated_other_file_extensions;
					EditorFileSystem::get_singleton()->scan();
				}
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor")) {
				_update_update_spinner();
				_update_vsync_mode();
				DisplayServer::get_singleton()->screen_set_keep_on(EDITOR_GET("interface/editor/keep_screen_on"));
			}

#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("text_editor/theme/highlighting")) {
				EditorHelpHighlighter::get_singleton()->reset_cache();
			}
#endif
		} break;
	}
}

void EditorNode::_update_update_spinner() {
	update_spinner->set_visible(!RenderingServer::get_singleton()->canvas_item_get_debug_redraw() && _should_display_update_spinner());

	const bool update_continuously = EDITOR_GET("interface/editor/update_continuously");
	PopupMenu *update_popup = update_spinner->get_popup();
	update_popup->set_item_checked(update_popup->get_item_index(SETTINGS_UPDATE_CONTINUOUSLY), update_continuously);
	update_popup->set_item_checked(update_popup->get_item_index(SETTINGS_UPDATE_WHEN_CHANGED), !update_continuously);

	if (update_continuously) {
		update_spinner->set_tooltip_text(TTR("Spins when the editor window redraws.\nUpdate Continuously is enabled, which can increase power usage. Click to disable it."));

		// Use a different color for the update spinner when Update Continuously is enabled,
		// as this feature should only be enabled for troubleshooting purposes.
		// Make the icon modulate color overbright because icons are not completely white on a dark theme.
		// On a light theme, icons are dark, so we need to modulate them with an even brighter color.
		const bool dark_theme = EditorThemeManager::is_dark_theme();
		update_spinner->set_self_modulate(theme->get_color(SNAME("error_color"), EditorStringName(Editor)) * (dark_theme ? Color(1.1, 1.1, 1.1) : Color(4.25, 4.25, 4.25)));
	} else {
		update_spinner->set_tooltip_text(TTR("Spins when the editor window redraws."));
		update_spinner->set_self_modulate(Color(1, 1, 1));
	}

	OS::get_singleton()->set_low_processor_usage_mode(!update_continuously);
}

void EditorNode::init_plugins() {
	_initializing_plugins = true;
	Vector<String> addons;
	if (ProjectSettings::get_singleton()->has_setting("editor_plugins/enabled")) {
		addons = GLOBAL_GET("editor_plugins/enabled");
	}

	for (const String &addon : addons) {
		set_addon_plugin_enabled(addon, true);
	}
	_initializing_plugins = false;

	if (!pending_addons.is_empty()) {
		EditorFileSystem::get_singleton()->connect("script_classes_updated", callable_mp(this, &EditorNode::_enable_pending_addons), CONNECT_ONE_SHOT);
	}
}

void EditorNode::_on_plugin_ready(Object *p_script, const String &p_activate_name) {
	Ref<Script> scr = Object::cast_to<Script>(p_script);
	if (scr.is_null()) {
		return;
	}
	project_settings_editor->update_plugins();
	project_settings_editor->hide();
	push_item(scr.operator->());
	if (p_activate_name.length()) {
		set_addon_plugin_enabled(p_activate_name, true);
	}
}

void EditorNode::_remove_plugin_from_enabled(const String &p_name) {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	PackedStringArray enabled_plugins = ps->get("editor_plugins/enabled");
	for (int i = 0; i < enabled_plugins.size(); ++i) {
		if (enabled_plugins.get(i) == p_name) {
			enabled_plugins.remove_at(i);
			break;
		}
	}
	ps->set("editor_plugins/enabled", enabled_plugins);
}

void EditorNode::_plugin_over_edit(EditorPlugin *p_plugin, Object *p_object) {
	if (p_object) {
		editor_plugins_over->add_plugin(p_plugin);
		p_plugin->make_visible(true);
		p_plugin->edit(p_object);
	} else {
		editor_plugins_over->remove_plugin(p_plugin);
		p_plugin->make_visible(false);
		p_plugin->edit(nullptr);
	}
}

void EditorNode::_plugin_over_self_own(EditorPlugin *p_plugin) {
	active_plugins[p_plugin->get_instance_id()].insert(p_plugin);
}

void EditorNode::_resources_changed(const Vector<String> &p_resources) {
	List<Ref<Resource>> changed;

	int rc = p_resources.size();
	for (int i = 0; i < rc; i++) {
		Ref<Resource> res = ResourceCache::get_ref(p_resources.get(i));
		if (res.is_null()) {
			continue;
		}

		if (!res->editor_can_reload_from_file()) {
			continue;
		}
		if (!res->get_path().is_resource_file() && !res->get_path().is_absolute_path()) {
			continue;
		}
		if (!FileAccess::exists(res->get_path())) {
			continue;
		}

		if (!res->get_import_path().is_empty()) {
			// This is an imported resource, will be reloaded if reimported via the _resources_reimported() callback.
			continue;
		}

		changed.push_back(res);
	}

	if (changed.size()) {
		for (Ref<Resource> &res : changed) {
			res->reload_from_file();
		}
	}
}

void EditorNode::_fs_changed() {
	for (FileDialog *E : file_dialogs) {
		E->invalidate();
	}

	for (EditorFileDialog *E : editor_file_dialogs) {
		E->invalidate();
	}

	_mark_unsaved_scenes();

	// FIXME: Move this to a cleaner location, it's hacky to do this in _fs_changed.
	String export_error;
	Error err = OK;
	// It's important to wait for the first scan to finish; otherwise, scripts or resources might not be imported.
	if (!export_defer.preset.is_empty() && !EditorFileSystem::get_singleton()->is_scanning()) {
		String preset_name = export_defer.preset;
		// Ensures export_project does not loop infinitely, because notifications may
		// come during the export.
		export_defer.preset = "";
		Ref<EditorExportPreset> export_preset;
		for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); ++i) {
			export_preset = EditorExport::get_singleton()->get_export_preset(i);
			if (export_preset->get_name() == preset_name) {
				break;
			}
			export_preset.unref();
		}

		if (export_preset.is_null()) {
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
			if (da->file_exists("res://export_presets.cfg")) {
				err = FAILED;
				export_error = vformat(
						"Invalid export preset name: %s.\nThe following presets were detected in this project's `export_presets.cfg`:\n\n",
						preset_name);
				for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); ++i) {
					// Write the preset name between double quotes since it needs to be written between quotes on the command line if it contains spaces.
					export_error += vformat("        \"%s\"\n", EditorExport::get_singleton()->get_export_preset(i)->get_name());
				}
			} else {
				err = FAILED;
				export_error = "This project doesn't have an `export_presets.cfg` file at its root.\nCreate an export preset from the \"Project > Export\" dialog and try again.";
			}
		} else {
			Ref<EditorExportPlatform> platform = export_preset->get_platform();
			const String export_path = export_defer.path.is_empty() ? export_preset->get_export_path() : export_defer.path;
			if (export_path.is_empty()) {
				err = FAILED;
				export_error = vformat("Export preset \"%s\" doesn't have a default export path, and none was specified.", preset_name);
			} else if (platform.is_null()) {
				err = FAILED;
				export_error = vformat("Export preset \"%s\" doesn't have a matching platform.", preset_name);
			} else {
				export_preset->update_value_overrides();
				if (export_defer.pack_only) { // Only export .pck or .zip data pack.
					if (export_path.ends_with(".zip")) {
						if (export_defer.patch) {
							err = platform->export_zip_patch(export_preset, export_defer.debug, export_path, export_defer.patches);
						} else {
							err = platform->export_zip(export_preset, export_defer.debug, export_path);
						}
					} else if (export_path.ends_with(".pck")) {
						if (export_defer.patch) {
							err = platform->export_pack_patch(export_preset, export_defer.debug, export_path, export_defer.patches);
						} else {
							err = platform->export_pack(export_preset, export_defer.debug, export_path);
						}
					} else {
						ERR_PRINT(vformat("Export path \"%s\" doesn't end with a supported extension.", export_path));
						err = FAILED;
					}
				} else { // Normal project export.
					String config_error;
					bool missing_templates;
					if (export_defer.android_build_template) {
						export_template_manager->install_android_template(export_preset);
					}
					if (!platform->can_export(export_preset, config_error, missing_templates, export_defer.debug)) {
						ERR_PRINT(vformat("Cannot export project with preset \"%s\" due to configuration errors:\n%s", preset_name, config_error));
						err = missing_templates ? ERR_FILE_NOT_FOUND : ERR_UNCONFIGURED;
					} else {
						platform->clear_messages();
						err = platform->export_project(export_preset, export_defer.debug, export_path);
					}
				}
				if (err != OK) {
					export_error = vformat("Project export for preset \"%s\" failed.", preset_name);
				} else if (platform->get_worst_message_type() >= EditorExportPlatform::EXPORT_MESSAGE_WARNING) {
					export_error = vformat("Project export for preset \"%s\" completed with warnings.", preset_name);
				}
			}
		}

		if (err != OK) {
			ERR_PRINT(export_error);
			_exit_editor(EXIT_FAILURE);
			return;
		}
		if (!export_error.is_empty()) {
			WARN_PRINT(export_error);
		}
		_exit_editor(EXIT_SUCCESS);
	}
}

void EditorNode::_resources_reimporting(const Vector<String> &p_resources) {
	// This will copy all the modified properties of the nodes into 'scenes_modification_table'
	// before they are actually reimported. It's important to do this before the reimportation
	// because if a mesh is present in an inherited scene, the resource will be modified in
	// the inherited scene. Then, get_modified_properties_for_node will return the mesh property,
	// which will trigger a recopy of the previous mesh, preventing the reload.
	scenes_modification_table.clear();
	scenes_reimported.clear();
	resources_reimported.clear();
	EditorFileSystem *editor_file_system = EditorFileSystem::get_singleton();
	for (const String &res_path : p_resources) {
		// It's faster to use EditorFileSystem::get_file_type than fetching the resource type from disk.
		// This makes a big difference when reimporting many resources.
		String file_type = editor_file_system->get_file_type(res_path);
		if (file_type.is_empty()) {
			file_type = ResourceLoader::get_resource_type(res_path);
		}
		if (file_type == "PackedScene") {
			scenes_reimported.push_back(res_path);
		} else {
			resources_reimported.push_back(res_path);
		}
	}

	if (scenes_reimported.size() > 0) {
		preload_reimporting_with_path_in_edited_scenes(scenes_reimported);
	}
}

void EditorNode::_resources_reimported(const Vector<String> &p_resources) {
	int current_tab = scene_tabs->get_current_tab();

	for (const String &res_path : resources_reimported) {
		if (!ResourceCache::has(res_path)) {
			// Not loaded, no need to reload.
			continue;
		}
		// Reload normally.
		Ref<Resource> resource = ResourceCache::get_ref(res_path);
		if (resource.is_valid()) {
			resource->reload_from_file();
		}
	}

	// Editor may crash when related animation is playing while re-importing GLTF scene, stop it in advance.
	AnimationPlayer *ap = AnimationPlayerEditor::get_singleton()->get_player();
	if (ap && scenes_reimported.size() > 0) {
		ap->stop(true);
	}

	for (const String &E : scenes_reimported) {
		reload_scene(E);
	}

	reload_instances_with_path_in_edited_scenes();

	scenes_modification_table.clear();
	scenes_reimported.clear();
	resources_reimported.clear();

	_set_current_scene_nocheck(current_tab);
}

void EditorNode::_sources_changed(bool p_exist) {
	if (waiting_for_first_scan) {
		waiting_for_first_scan = false;

		OS::get_singleton()->benchmark_end_measure("Editor", "First Scan");

		// Reload the global shader variables, but this time
		// loading textures, as they are now properly imported.
		RenderingServer::get_singleton()->global_shader_parameters_load_settings(true);

		_load_editor_layout();

		if (!defer_load_scene.is_empty()) {
			OS::get_singleton()->benchmark_begin_measure("Editor", "Load Scene");

			load_scene(defer_load_scene);
			defer_load_scene = "";

			OS::get_singleton()->benchmark_end_measure("Editor", "Load Scene");
			OS::get_singleton()->benchmark_dump();
		}

		if (SurfaceUpgradeTool::get_singleton()->is_show_requested()) {
			SurfaceUpgradeTool::get_singleton()->show_popup();
		}

		// Start preview thread now that it's safe.
		if (!singleton->cmdline_export_mode) {
			EditorResourcePreview::get_singleton()->start();
		}
	}
}

void EditorNode::_scan_external_changes() {
	disk_changed_list->clear();
	TreeItem *r = disk_changed_list->create_item();
	disk_changed_list->set_hide_root(true);
	bool need_reload = false;

	// Check if any edited scene has changed.

	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (editor_data.get_scene_path(i) == "" || !da->file_exists(editor_data.get_scene_path(i))) {
			continue;
		}

		uint64_t last_date = editor_data.get_scene_modified_time(i);
		uint64_t date = FileAccess::get_modified_time(editor_data.get_scene_path(i));

		if (date > last_date) {
			TreeItem *ti = disk_changed_list->create_item(r);
			ti->set_text(0, editor_data.get_scene_path(i).get_file());
			need_reload = true;
		}
	}

	String project_settings_path = ProjectSettings::get_singleton()->get_resource_path().path_join("project.godot");
	if (FileAccess::get_modified_time(project_settings_path) > ProjectSettings::get_singleton()->get_last_saved_time()) {
		TreeItem *ti = disk_changed_list->create_item(r);
		ti->set_text(0, "project.godot");
		need_reload = true;
	}

	if (need_reload) {
		callable_mp((Window *)disk_changed, &Window::popup_centered_ratio).call_deferred(0.3);
	}
}

void EditorNode::_resave_scenes(String p_str) {
	save_all_scenes();
	ProjectSettings::get_singleton()->save();
	disk_changed->hide();
}

void EditorNode::_reload_modified_scenes() {
	int current_idx = editor_data.get_edited_scene();

	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		if (editor_data.get_scene_path(i) == "") {
			continue;
		}

		uint64_t last_date = editor_data.get_scene_modified_time(i);
		uint64_t date = FileAccess::get_modified_time(editor_data.get_scene_path(i));

		if (date > last_date) {
			String filename = editor_data.get_scene_path(i);
			editor_data.set_edited_scene(i);
			_remove_edited_scene(false);

			Error err = load_scene(filename, false, false, false, true);
			if (err != OK) {
				ERR_PRINT(vformat("Failed to load scene: %s", filename));
			}
			editor_data.move_edited_scene_to_index(i);
		}
	}

	_set_current_scene(current_idx);
	scene_tabs->update_scene_tabs();
	disk_changed->hide();
}

void EditorNode::_reload_project_settings() {
	ProjectSettings::get_singleton()->setup(ProjectSettings::get_singleton()->get_resource_path(), String(), true, true);
}

void EditorNode::_vp_resized() {
}

void EditorNode::_viewport_resized() {
	Window *w = get_window();
	if (w) {
		was_window_windowed_last = w->get_mode() == Window::MODE_WINDOWED;
	}
}

void EditorNode::_titlebar_resized() {
	DisplayServer::get_singleton()->window_set_window_buttons_offset(Vector2i(title_bar->get_global_position().y + title_bar->get_size().y / 2, title_bar->get_global_position().y + title_bar->get_size().y / 2), DisplayServer::MAIN_WINDOW_ID);
	const Vector3i &margin = DisplayServer::get_singleton()->window_get_safe_title_margins(DisplayServer::MAIN_WINDOW_ID);
	if (left_menu_spacer) {
		int w = (gui_base->is_layout_rtl()) ? margin.y : margin.x;
		left_menu_spacer->set_custom_minimum_size(Size2(w, 0));
	}
	if (right_menu_spacer) {
		int w = (gui_base->is_layout_rtl()) ? margin.x : margin.y;
		right_menu_spacer->set_custom_minimum_size(Size2(w, 0));
	}
	if (title_bar) {
		title_bar->set_custom_minimum_size(Size2(0, margin.z - title_bar->get_global_position().y));
	}
}

void EditorNode::_update_undo_redo_allowed() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	file_menu->set_item_disabled(file_menu->get_item_index(EDIT_UNDO), !undo_redo->has_undo());
	file_menu->set_item_disabled(file_menu->get_item_index(EDIT_REDO), !undo_redo->has_redo());
}

void EditorNode::_node_renamed() {
	if (InspectorDock::get_inspector_singleton()) {
		InspectorDock::get_inspector_singleton()->update_tree();
	}
}

void EditorNode::_open_command_palette() {
	command_palette->open_popup();
}

Error EditorNode::load_resource(const String &p_resource, bool p_ignore_broken_deps) {
	dependency_errors.clear();

	Error err;

	Ref<Resource> res;
	if (ResourceLoader::exists(p_resource, "")) {
		res = ResourceLoader::load(p_resource, "", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	} else if (textfile_extensions.has(p_resource.get_extension())) {
		res = ScriptEditor::get_singleton()->open_file(p_resource);
	} else if (other_file_extensions.has(p_resource.get_extension())) {
		OS::get_singleton()->shell_open(ProjectSettings::get_singleton()->globalize_path(p_resource));
		return OK;
	}
	ERR_FAIL_COND_V(res.is_null(), ERR_CANT_OPEN);

	if (!p_ignore_broken_deps && dependency_errors.has(p_resource)) {
		Vector<String> errors;
		for (const String &E : dependency_errors[p_resource]) {
			errors.push_back(E);
		}
		dependency_error->show(DependencyErrorDialog::MODE_RESOURCE, p_resource, errors);
		dependency_errors.erase(p_resource);

		return ERR_FILE_MISSING_DEPENDENCIES;
	}

	InspectorDock::get_singleton()->edit_resource(res);
	return OK;
}

void EditorNode::edit_node(Node *p_node) {
	push_item(p_node);
}

void EditorNode::edit_resource(const Ref<Resource> &p_resource) {
	InspectorDock::get_singleton()->edit_resource(p_resource);
}

void EditorNode::save_resource_in_path(const Ref<Resource> &p_resource, const String &p_path) {
	editor_data.apply_changes_in_editors();

	if (saving_resources_in_path.has(p_resource)) {
		return;
	}
	saving_resources_in_path.insert(p_resource);

	int flg = 0;
	if (EDITOR_GET("filesystem/on_save/compress_binary_resources")) {
		flg |= ResourceSaver::FLAG_COMPRESS;
	}

	String path = ProjectSettings::get_singleton()->localize_path(p_path);
	Error err = ResourceSaver::save(p_resource, path, flg | ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS);

	if (err != OK) {
		if (ResourceLoader::is_imported(p_resource->get_path())) {
			show_accept(TTR("Imported resources can't be saved."), TTR("OK"));
		} else {
			show_accept(TTR("Error saving resource!"), TTR("OK"));
		}

		saving_resources_in_path.erase(p_resource);
		return;
	}

	((Resource *)p_resource.ptr())->set_path(path);
	saving_resources_in_path.erase(p_resource);

	_resource_saved(p_resource, path);

	emit_signal(SNAME("resource_saved"), p_resource);
	editor_data.notify_resource_saved(p_resource);
}

void EditorNode::save_resource(const Ref<Resource> &p_resource) {
	// If built-in resource, save the scene instead.
	if (p_resource->is_built_in()) {
		const String scene_path = p_resource->get_path().get_slice("::", 0);
		if (!scene_path.is_empty()) {
			if (ResourceLoader::exists(scene_path) && ResourceLoader::get_resource_type(scene_path) == "PackedScene") {
				save_scene_if_open(scene_path);
			} else {
				// Not a packed scene, so save it as regular resource.
				Ref<Resource> parent_resource = ResourceCache::get_ref(scene_path);
				ERR_FAIL_COND_MSG(parent_resource.is_null(), "Parent resource not loaded, can't save.");
				save_resource(parent_resource);
			}
			return;
		}
	}

	// If the resource has been imported, ask the user to use a different path in order to save it.
	String path = p_resource->get_path();
	if (path.is_resource_file() && !FileAccess::exists(path + ".import")) {
		save_resource_in_path(p_resource, p_resource->get_path());
	} else {
		save_resource_as(p_resource);
	}
}

void EditorNode::save_resource_as(const Ref<Resource> &p_resource, const String &p_at_path) {
	{
		String path = p_resource->get_path();
		if (!path.is_resource_file()) {
			int srpos = path.find("::");
			if (srpos != -1) {
				String base = path.substr(0, srpos);
				if (!get_edited_scene() || get_edited_scene()->get_scene_file_path() != base) {
					show_warning(TTR("This resource can't be saved because it does not belong to the edited scene. Make it unique first."));
					return;
				}
			}
		} else if (FileAccess::exists(path + ".import")) {
			show_warning(TTR("This resource can't be saved because it was imported from another file. Make it unique first."));
			return;
		}
	}

	file->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	saving_resource = p_resource;

	current_menu_option = RESOURCE_SAVE_AS;
	List<String> extensions;
	Ref<PackedScene> sd = memnew(PackedScene);
	ResourceSaver::get_recognized_extensions(p_resource, &extensions);
	file->clear_filters();

	List<String> preferred;
	for (const String &E : extensions) {
		if (p_resource->is_class("Script") && (E == "tres" || E == "res")) {
			// This serves no purpose and confused people.
			continue;
		}
		file->add_filter("*." + E, E.to_upper());
		preferred.push_back(E);
	}
	// Lowest priority extension.
	List<String>::Element *res_element = preferred.find("res");
	if (res_element) {
		preferred.move_to_back(res_element);
	}
	// Highest priority extension.
	List<String>::Element *tres_element = preferred.find("tres");
	if (tres_element) {
		preferred.move_to_front(tres_element);
	}

	if (!p_at_path.is_empty()) {
		file->set_current_dir(p_at_path);
		if (p_resource->get_path().is_resource_file()) {
			file->set_current_file(p_resource->get_path().get_file());
		} else {
			if (!preferred.is_empty()) {
				String resource_name_snake_case = p_resource->get_class().to_snake_case();
				file->set_current_file("new_" + resource_name_snake_case + "." + preferred.front()->get().to_lower());
			} else {
				file->set_current_file(String());
			}
		}
	} else if (!p_resource->get_path().is_empty()) {
		file->set_current_path(p_resource->get_path());
		if (!extensions.is_empty()) {
			const String ext = p_resource->get_path().get_extension().to_lower();
			if (extensions.find(ext) == nullptr) {
				file->set_current_path(p_resource->get_path().replacen("." + ext, "." + extensions.front()->get()));
			}
		}
	} else if (!preferred.is_empty()) {
		const String resource_name_snake_case = p_resource->get_class().to_snake_case();
		const String existing = "new_" + resource_name_snake_case + "." + preferred.front()->get().to_lower();
		file->set_current_path(existing);
	}
	file->set_title(TTR("Save Resource As..."));
	file->popup_file_dialog();
}

void EditorNode::ensure_uid_file(const String &p_new_resource_path) {
	if (ResourceLoader::exists(p_new_resource_path) && !ResourceLoader::has_custom_uid_support(p_new_resource_path) && !FileAccess::exists(p_new_resource_path + ".uid")) {
		Ref<FileAccess> f = FileAccess::open(p_new_resource_path + ".uid", FileAccess::WRITE);
		if (f.is_valid()) {
			const ResourceUID::ID id = ResourceUID::get_singleton()->create_id();
			f->store_line(ResourceUID::get_singleton()->id_to_text(id));
		}
	}
}

void EditorNode::_menu_option(int p_option) {
	_menu_option_confirm(p_option, false);
}

void EditorNode::_menu_confirm_current() {
	_menu_option_confirm(current_menu_option, true);
}

void EditorNode::trigger_menu_option(int p_option, bool p_confirmed) {
	_menu_option_confirm(p_option, p_confirmed);
}

void EditorNode::_dialog_display_save_error(String p_file, Error p_error) {
	if (p_error) {
		switch (p_error) {
			case ERR_FILE_CANT_WRITE: {
				show_accept(TTR("Can't open file for writing:") + " " + p_file.get_extension(), TTR("OK"));
			} break;
			case ERR_FILE_UNRECOGNIZED: {
				show_accept(TTR("Requested file format unknown:") + " " + p_file.get_extension(), TTR("OK"));
			} break;
			default: {
				show_accept(TTR("Error while saving."), TTR("OK"));
			} break;
		}
	}
}

void EditorNode::_dialog_display_load_error(String p_file, Error p_error) {
	if (p_error) {
		switch (p_error) {
			case ERR_CANT_OPEN: {
				show_accept(vformat(TTR("Can't open file '%s'. The file could have been moved or deleted."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_PARSE_ERROR: {
				show_accept(vformat(TTR("Error while parsing file '%s'."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_CORRUPT: {
				show_accept(vformat(TTR("Scene file '%s' appears to be invalid/corrupt."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_NOT_FOUND: {
				show_accept(vformat(TTR("Missing file '%s' or one of its dependencies."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_UNRECOGNIZED: {
				show_accept(vformat(TTR("File '%s' is saved in a format that is newer than the formats supported by this version of Godot, so it can't be opened."), p_file.get_file()), TTR("OK"));
			} break;
			default: {
				show_accept(vformat(TTR("Error while loading file '%s'."), p_file.get_file()), TTR("OK"));
			} break;
		}
	}
}

void EditorNode::_load_editor_plugin_states_from_config(const Ref<ConfigFile> &p_config_file) {
	Node *scene = editor_data.get_edited_scene_root();

	if (!scene) {
		return;
	}

	List<String> esl;
	p_config_file->get_section_keys("editor_states", &esl);

	Dictionary md;
	for (const String &E : esl) {
		Variant st = p_config_file->get_value("editor_states", E);
		if (st.get_type() != Variant::NIL) {
			md[E] = st;
		}
	}

	editor_data.set_editor_plugin_states(md);
}

void EditorNode::_save_editor_states(const String &p_file, int p_idx) {
	Node *scene = editor_data.get_edited_scene_root(p_idx);

	if (!scene) {
		return;
	}

	String path = EditorPaths::get_singleton()->get_project_settings_dir().path_join(p_file.get_file() + "-editstate-" + p_file.md5_text() + ".cfg");

	Ref<ConfigFile> cf;
	cf.instantiate();

	Dictionary md;
	if (p_idx < 0 || editor_data.get_edited_scene() == p_idx) {
		md = editor_data.get_editor_plugin_states();
	} else {
		md = editor_data.get_scene_editor_states(p_idx);
	}

	List<Variant> keys;
	md.get_key_list(&keys);
	for (const Variant &E : keys) {
		cf->set_value("editor_states", E, md[E]);
	}

	// Save the currently selected nodes.

	List<Node *> selection = editor_selection->get_full_selected_node_list();
	TypedArray<NodePath> selection_paths;
	for (Node *selected_node : selection) {
		selection_paths.push_back(selected_node->get_path());
	}
	cf->set_value("editor_states", "selected_nodes", selection_paths);

	Error err = cf->save(path);
	ERR_FAIL_COND_MSG(err != OK, "Cannot save config file to '" + path + "'.");
}

bool EditorNode::_find_and_save_resource(Ref<Resource> p_res, HashMap<Ref<Resource>, bool> &processed, int32_t flags) {
	if (p_res.is_null()) {
		return false;
	}

	if (processed.has(p_res)) {
		return processed[p_res];
	}

	bool changed = p_res->is_edited();
	p_res->set_edited(false);

	bool subchanged = _find_and_save_edited_subresources(p_res.ptr(), processed, flags);

	if (p_res->get_path().is_resource_file()) {
		if (changed || subchanged) {
			ResourceSaver::save(p_res, p_res->get_path(), flags);
		}
		processed[p_res] = false; // Because it's a file.
		return false;
	} else {
		processed[p_res] = changed;
		return changed;
	}
}

bool EditorNode::_find_and_save_edited_subresources(Object *obj, HashMap<Ref<Resource>, bool> &processed, int32_t flags) {
	bool ret_changed = false;
	List<PropertyInfo> pi;
	obj->get_property_list(&pi);
	for (const PropertyInfo &E : pi) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		switch (E.type) {
			case Variant::OBJECT: {
				Ref<Resource> res = obj->get(E.name);

				if (_find_and_save_resource(res, processed, flags)) {
					ret_changed = true;
				}

			} break;
			case Variant::ARRAY: {
				Array varray = obj->get(E.name);
				int len = varray.size();
				for (int i = 0; i < len; i++) {
					const Variant &v = varray.get(i);
					Ref<Resource> res = v;
					if (_find_and_save_resource(res, processed, flags)) {
						ret_changed = true;
					}
				}

			} break;
			case Variant::DICTIONARY: {
				Dictionary d = obj->get(E.name);
				List<Variant> keys;
				d.get_key_list(&keys);
				for (const Variant &F : keys) {
					Variant v = d[F];
					Ref<Resource> res = v;
					if (_find_and_save_resource(res, processed, flags)) {
						ret_changed = true;
					}
				}
			} break;
			default: {
			}
		}
	}

	return ret_changed;
}

void EditorNode::_save_edited_subresources(Node *scene, HashMap<Ref<Resource>, bool> &processed, int32_t flags) {
	_find_and_save_edited_subresources(scene, processed, flags);

	for (int i = 0; i < scene->get_child_count(); i++) {
		Node *n = scene->get_child(i);
		if (n->get_owner() != editor_data.get_edited_scene_root()) {
			continue;
		}
		_save_edited_subresources(n, processed, flags);
	}
}

void EditorNode::_find_node_types(Node *p_node, int &count_2d, int &count_3d) {
	if (p_node->is_class("Viewport") || (p_node != editor_data.get_edited_scene_root() && p_node->get_owner() != editor_data.get_edited_scene_root())) {
		return;
	}

	if (p_node->is_class("CanvasItem")) {
		count_2d++;
	} else if (p_node->is_class("Node3D")) {
		count_3d++;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_find_node_types(p_node->get_child(i), count_2d, count_3d);
	}
}

void EditorNode::_save_scene_with_preview(String p_file, int p_idx) {
	save_scene_progress = memnew(EditorProgress("save", TTR("Saving Scene"), 4));

	if (editor_data.get_edited_scene_root() != nullptr) {
		save_scene_progress->step(TTR("Analyzing"), 0);

		int c2d = 0;
		int c3d = 0;

		_find_node_types(editor_data.get_edited_scene_root(), c2d, c3d);

		save_scene_progress->step(TTR("Creating Thumbnail"), 1);
		// Current view?

		Ref<Image> img;
		// If neither 3D or 2D nodes are present, make a 1x1 black texture.
		// We cannot fallback on the 2D editor, because it may not have been used yet,
		// which would result in an invalid texture.
		if (c3d == 0 && c2d == 0) {
			img.instantiate();
			img->initialize_data(1, 1, false, Image::FORMAT_RGB8);
		} else if (c3d < c2d) {
			Ref<ViewportTexture> viewport_texture = scene_root->get_texture();
			if (viewport_texture->get_width() > 0 && viewport_texture->get_height() > 0) {
				img = viewport_texture->get_image();
			}
		} else {
			// The 3D editor may be disabled as a feature, but scenes can still be opened.
			// This check prevents the preview from regenerating in case those scenes are then saved.
			// The preview will be generated if no feature profile is set (as the 3D editor is enabled by default).
			Ref<EditorFeatureProfile> profile = feature_profile_manager->get_current_profile();
			if (profile.is_null() || !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_3D)) {
				img = Node3DEditor::get_singleton()->get_editor_viewport(0)->get_viewport_node()->get_texture()->get_image();
			}
		}

		if (img.is_valid() && img->get_width() > 0 && img->get_height() > 0) {
			img = img->duplicate();

			save_scene_progress->step(TTR("Creating Thumbnail"), 3);

			int preview_size = EDITOR_GET("filesystem/file_dialog/thumbnail_size");
			preview_size *= EDSCALE;

			// Consider a square region.
			int vp_size = MIN(img->get_width(), img->get_height());
			int x = (img->get_width() - vp_size) / 2;
			int y = (img->get_height() - vp_size) / 2;

			if (vp_size < preview_size) {
				// Just square it.
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

			// Save thumbnail directly, as thumbnailer may not update due to actual scene not changing md5.
			String temp_path = EditorPaths::get_singleton()->get_cache_dir();
			String cache_base = ProjectSettings::get_singleton()->globalize_path(p_file).md5_text();
			cache_base = temp_path.path_join("resthumb-" + cache_base);

			// Does not have it, try to load a cached thumbnail.
			post_process_preview(img);
			img->save_png(cache_base + ".png");
		}
	}

	save_scene_progress->step(TTR("Saving Scene"), 4);
	_save_scene(p_file, p_idx);

	if (!singleton->cmdline_export_mode) {
		EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);
	}

	_close_save_scene_progress();
}

void EditorNode::_close_save_scene_progress() {
	memdelete_notnull(save_scene_progress);
	save_scene_progress = nullptr;
}

bool EditorNode::_validate_scene_recursive(const String &p_filename, Node *p_node) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		if (child->get_scene_file_path() == p_filename) {
			return true;
		}

		if (_validate_scene_recursive(p_filename, child)) {
			return true;
		}
	}

	return false;
}

int EditorNode::_save_external_resources(bool p_also_save_external_data) {
	// Save external resources and its subresources if any was modified.

	int flg = 0;
	if (EDITOR_GET("filesystem/on_save/compress_binary_resources")) {
		flg |= ResourceSaver::FLAG_COMPRESS;
	}
	flg |= ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;

	HashSet<String> edited_resources;
	int saved = 0;
	List<Ref<Resource>> cached;
	ResourceCache::get_cached_resources(&cached);

	for (Ref<Resource> res : cached) {
		if (!res->is_edited()) {
			continue;
		}

		String path = res->get_path();
		if (path.begins_with("res://")) {
			int subres_pos = path.find("::");
			if (subres_pos == -1) {
				// Actual resource.
				edited_resources.insert(path);
			} else {
				edited_resources.insert(path.substr(0, subres_pos));
			}
		}

		res->set_edited(false);
	}

	for (const String &E : edited_resources) {
		Ref<Resource> res = ResourceCache::get_ref(E);
		if (res.is_null()) {
			continue; // Maybe it was erased in a thread, who knows.
		}
		Ref<PackedScene> ps = res;
		if (ps.is_valid()) {
			continue; // Do not save PackedScenes, this will mess up the editor.
		}
		ResourceSaver::save(res, res->get_path(), flg);
		saved++;
	}

	if (p_also_save_external_data) {
		for (int i = 0; i < editor_data.get_editor_plugin_count(); i++) {
			EditorPlugin *plugin = editor_data.get_editor_plugin(i);
			if (!plugin->get_unsaved_status().is_empty()) {
				plugin->save_external_data();
				saved++;
			}
		}
	}

	EditorUndoRedoManager::get_singleton()->set_history_as_saved(EditorUndoRedoManager::GLOBAL_HISTORY);

	return saved;
}

static void _reset_animation_mixers(Node *p_node, List<Pair<AnimationMixer *, Ref<AnimatedValuesBackup>>> *r_anim_backups) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		AnimationMixer *mixer = Object::cast_to<AnimationMixer>(p_node->get_child(i));
		if (mixer && mixer->is_active() && mixer->is_reset_on_save_enabled() && mixer->can_apply_reset()) {
			AnimationTree *tree = Object::cast_to<AnimationTree>(p_node->get_child(i));
			if (tree) {
				AnimationPlayer *player = Object::cast_to<AnimationPlayer>(tree->get_node_or_null(tree->get_animation_player()));
				if (player && player->is_active() && player->is_reset_on_save_enabled() && player->can_apply_reset()) {
					continue; // Avoid to process reset/restore many times.
				}
			}
			Ref<AnimatedValuesBackup> backup = mixer->apply_reset();
			if (backup.is_valid()) {
				Pair<AnimationMixer *, Ref<AnimatedValuesBackup>> pair;
				pair.first = mixer;
				pair.second = backup;
				r_anim_backups->push_back(pair);
			}
		}
		_reset_animation_mixers(p_node->get_child(i), r_anim_backups);
	}
}

void EditorNode::_save_scene(String p_file, int idx) {
	ERR_FAIL_COND_MSG(!saving_scene.is_empty() && saving_scene == p_file, "Scene saved while already being saved!");

	Node *scene = editor_data.get_edited_scene_root(idx);

	if (!scene) {
		show_accept(TTR("This operation can't be done without a tree root."), TTR("OK"));
		return;
	}

	if (!scene->get_scene_file_path().is_empty() && _validate_scene_recursive(scene->get_scene_file_path(), scene)) {
		show_accept(TTR("This scene can't be saved because there is a cyclic instance inclusion.\nPlease resolve it and then attempt to save again."), TTR("OK"));
		return;
	}

	scene->propagate_notification(NOTIFICATION_EDITOR_PRE_SAVE);

	editor_data.apply_changes_in_editors();
	List<Pair<AnimationMixer *, Ref<AnimatedValuesBackup>>> anim_backups;
	_reset_animation_mixers(scene, &anim_backups);
	save_default_environment();

	_save_editor_states(p_file, idx);

	Ref<PackedScene> sdata;

	if (ResourceCache::has(p_file)) {
		// Something may be referencing this resource and we are good with that.
		// We must update it, but also let the previous scene state go, as
		// old version still work for referencing changes in instantiated or inherited scenes.

		sdata = ResourceCache::get_ref(p_file);
		if (sdata.is_valid()) {
			sdata->recreate_state();
		} else {
			sdata.instantiate();
		}
	} else {
		sdata.instantiate();
	}
	Error err = sdata->pack(scene);

	if (err != OK) {
		show_accept(TTR("Couldn't save scene. Likely dependencies (instances or inheritance) couldn't be satisfied."), TTR("OK"));
		return;
	}

	int flg = 0;
	if (EDITOR_GET("filesystem/on_save/compress_binary_resources")) {
		flg |= ResourceSaver::FLAG_COMPRESS;
	}
	flg |= ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;

	err = ResourceSaver::save(sdata, p_file, flg);

	// This needs to be emitted before saving external resources.
	emit_signal(SNAME("scene_saved"), p_file);
	editor_data.notify_scene_saved(p_file);

	_save_external_resources();
	saving_scene = p_file; // Some editors may save scenes of built-in resources as external data, so avoid saving this scene again.
	editor_data.save_editor_external_data();
	saving_scene = "";

	for (Pair<AnimationMixer *, Ref<AnimatedValuesBackup>> &E : anim_backups) {
		E.first->restore(E.second);
	}

	if (err == OK) {
		scene->set_scene_file_path(ProjectSettings::get_singleton()->localize_path(p_file));
		editor_data.set_scene_as_saved(idx);
		editor_data.set_scene_modified_time(idx, FileAccess::get_modified_time(p_file));

		editor_folding.save_scene_folding(scene, p_file);

		_update_title();
		scene_tabs->update_scene_tabs();
	} else {
		_dialog_display_save_error(p_file, err);
	}

	scene->propagate_notification(NOTIFICATION_EDITOR_POST_SAVE);
}

void EditorNode::save_all_scenes() {
	project_run_bar->stop_playing();
	_save_all_scenes();
}

void EditorNode::save_scene_if_open(const String &p_scene_path) {
	int idx = editor_data.get_edited_scene_from_path(p_scene_path);
	if (idx >= 0) {
		_save_scene(p_scene_path, idx);
	}
}

void EditorNode::save_scene_list(const HashSet<String> &p_scene_paths) {
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Node *scene = editor_data.get_edited_scene_root(i);

		if (scene && p_scene_paths.has(scene->get_scene_file_path())) {
			_save_scene(scene->get_scene_file_path(), i);
		}
	}
}

void EditorNode::save_before_run() {
	current_menu_option = FILE_SAVE_AND_RUN;
	_menu_option_confirm(FILE_SAVE_AS_SCENE, true);
	file->set_title(TTR("Save scene before running..."));
}

void EditorNode::try_autosave() {
	if (!bool(EDITOR_GET("run/auto_save/save_before_running"))) {
		return;
	}

	if (unsaved_cache) {
		Node *scene = editor_data.get_edited_scene_root();

		if (scene && !scene->get_scene_file_path().is_empty()) { // Only autosave if there is a scene and if it has a path.
			_save_scene_with_preview(scene->get_scene_file_path());
		}
	}
	_menu_option(FILE_SAVE_ALL_SCENES);
	editor_data.save_editor_external_data();
}

void EditorNode::restart_editor(bool p_goto_project_manager) {
	exiting = true;

	if (project_run_bar->is_playing()) {
		project_run_bar->stop_playing();
	}

	String to_reopen;
	if (!p_goto_project_manager && get_tree()->get_edited_scene_root()) {
		to_reopen = get_tree()->get_edited_scene_root()->get_scene_file_path();
	}

	_exit_editor(EXIT_SUCCESS);

	List<String> args;
	for (const String &a : Main::get_forwardable_cli_arguments(Main::CLI_SCOPE_TOOL)) {
		args.push_back(a);
	}

	if (p_goto_project_manager) {
		args.push_back("--project-manager");
	} else {
		args.push_back("--path");
		args.push_back(ProjectSettings::get_singleton()->get_resource_path());

		args.push_back("-e");
	}

	if (!to_reopen.is_empty()) {
		args.push_back(to_reopen);
	}

	OS::get_singleton()->set_restart_on_exit(true, args);
}

void EditorNode::_save_all_scenes() {
	bool all_saved = true;
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Node *scene = editor_data.get_edited_scene_root(i);
		if (scene) {
			if (!scene->get_scene_file_path().is_empty() && DirAccess::exists(scene->get_scene_file_path().get_base_dir())) {
				if (i != editor_data.get_edited_scene()) {
					_save_scene(scene->get_scene_file_path(), i);
				} else {
					_save_scene_with_preview(scene->get_scene_file_path());
				}
			} else if (!scene->get_scene_file_path().is_empty()) {
				all_saved = false;
			}
		}
	}

	if (!all_saved) {
		show_warning(TTR("Could not save one or more scenes!"), TTR("Save All Scenes"));
	}
	save_default_environment();
}

void EditorNode::_mark_unsaved_scenes() {
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Node *node = editor_data.get_edited_scene_root(i);
		if (!node) {
			continue;
		}

		String path = node->get_scene_file_path();
		if (!path.is_empty() && !FileAccess::exists(path)) {
			// Mark scene tab as unsaved if the file is gone.
			EditorUndoRedoManager::get_singleton()->set_history_as_unsaved(editor_data.get_scene_history_id(i));
		}
	}

	_update_title();
	scene_tabs->update_scene_tabs();
}

void EditorNode::_dialog_action(String p_file) {
	switch (current_menu_option) {
		case FILE_NEW_INHERITED_SCENE: {
			Node *scene = editor_data.get_edited_scene_root();
			// If the previous scene is rootless, just close it in favor of the new one.
			if (!scene) {
				_menu_option_confirm(FILE_CLOSE, true);
			}

			load_scene(p_file, false, true);
		} break;
		case FILE_OPEN_SCENE: {
			load_scene(p_file);
		} break;
		case SETTINGS_PICK_MAIN_SCENE: {
			ProjectSettings::get_singleton()->set("application/run/main_scene", ResourceUID::path_to_uid(p_file));
			ProjectSettings::get_singleton()->save();
			// TODO: Would be nice to show the project manager opened with the highlighted field.

			project_run_bar->play_main_scene((bool)pick_main_scene->get_meta("from_native", false));
		} break;
		case FILE_CLOSE:
		case SCENE_TAB_CLOSE:
		case FILE_SAVE_SCENE:
		case FILE_SAVE_AS_SCENE: {
			int scene_idx = (current_menu_option == FILE_SAVE_SCENE || current_menu_option == FILE_SAVE_AS_SCENE) ? -1 : tab_closing_idx;

			if (file->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				bool same_open_scene = false;
				for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
					if (editor_data.get_scene_path(i) == p_file && i != scene_idx) {
						same_open_scene = true;
					}
				}

				if (same_open_scene) {
					show_warning(TTR("Can't overwrite scene that is still open!"));
					return;
				}

				save_default_environment();
				_save_scene_with_preview(p_file, scene_idx);
				_add_to_recent_scenes(p_file);
				save_editor_layout_delayed();

				if (scene_idx != -1) {
					_discard_changes();
				} else {
					// Update the path of the edited scene to ensure later do/undo action history matches.
					editor_data.set_scene_path(editor_data.get_edited_scene(), p_file);
				}
			}

		} break;

		case FILE_SAVE_AND_RUN: {
			if (file->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				save_default_environment();
				_save_scene_with_preview(p_file);
				project_run_bar->play_custom_scene(p_file);
			}
		} break;

		case FILE_SAVE_AND_RUN_MAIN_SCENE: {
			ProjectSettings::get_singleton()->set("application/run/main_scene", ResourceUID::path_to_uid(p_file));
			ProjectSettings::get_singleton()->save();

			if (file->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				save_default_environment();
				_save_scene_with_preview(p_file);
				project_run_bar->play_main_scene((bool)pick_main_scene->get_meta("from_native", false));
			}
		} break;

		case FILE_EXPORT_MESH_LIBRARY: {
			const Dictionary &fd_options = file_export_lib->get_selected_options();
			bool merge_with_existing_library = fd_options.get(TTR("Merge With Existing"), true);
			bool apply_mesh_instance_transforms = fd_options.get(TTR("Apply MeshInstance Transforms"), false);

			Ref<MeshLibrary> ml;
			if (merge_with_existing_library && FileAccess::exists(p_file)) {
				ml = ResourceLoader::load(p_file, "MeshLibrary");

				if (ml.is_null()) {
					show_accept(TTR("Can't load MeshLibrary for merging!"), TTR("OK"));
					return;
				}
			}

			if (ml.is_null()) {
				ml.instantiate();
			}

			MeshLibraryEditor::update_library_file(editor_data.get_edited_scene_root(), ml, merge_with_existing_library, apply_mesh_instance_transforms);

			Error err = ResourceSaver::save(ml, p_file);
			if (err) {
				show_accept(TTR("Error saving MeshLibrary!"), TTR("OK"));
				return;
			} else if (ResourceCache::has(p_file)) {
				// Make sure MeshLibrary is updated in the editor.
				ResourceLoader::load(p_file)->reload_from_file();
			}

		} break;

		case RESOURCE_SAVE:
		case RESOURCE_SAVE_AS: {
			ERR_FAIL_COND(saving_resource.is_null());
			save_resource_in_path(saving_resource, p_file);

			if (current_menu_option == RESOURCE_SAVE_AS) {
				// Create .uid file when making new Resource.
				ensure_uid_file(p_file);
			}

			saving_resource = Ref<Resource>();
			ObjectID current_id = editor_history.get_current();
			Object *current_obj = current_id.is_valid() ? ObjectDB::get_instance(current_id) : nullptr;
			ERR_FAIL_NULL(current_obj);
			current_obj->notify_property_list_changed();
		} break;
		case SETTINGS_LAYOUT_SAVE: {
			if (p_file.is_empty()) {
				return;
			}

			Ref<ConfigFile> config;
			config.instantiate();
			Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());

			if (err == ERR_FILE_CANT_OPEN || err == ERR_FILE_NOT_FOUND) {
				config.instantiate();
			} else if (err != OK) {
				show_warning(TTR("An error occurred while trying to save the editor layout.\nMake sure the editor's user data path is writable."));
				return;
			}

			editor_dock_manager->save_docks_to_config(config, p_file);

			config->save(EditorSettings::get_singleton()->get_editor_layouts_config());

			layout_dialog->hide();
			_update_layouts_menu();

			if (p_file == "Default") {
				show_warning(TTR("Default editor layout overridden.\nTo restore the Default layout to its base settings, use the Delete Layout option and delete the Default layout."));
			}

		} break;
		case SETTINGS_LAYOUT_DELETE: {
			Ref<ConfigFile> config;
			config.instantiate();
			Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());

			if (err != OK || !config->has_section(p_file)) {
				show_warning(TTR("Layout name not found!"));
				return;
			}

			// Erase key values.
			List<String> keys;
			config->get_section_keys(p_file, &keys);
			for (const String &key : keys) {
				config->set_value(p_file, key, Variant());
			}

			config->save(EditorSettings::get_singleton()->get_editor_layouts_config());

			layout_dialog->hide();
			_update_layouts_menu();

			if (p_file == "Default") {
				show_warning(TTR("Restored the Default layout to its base settings."));
			}

		} break;
		default: {
			// Save scene?
			if (file->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				_save_scene_with_preview(p_file);
			}

		} break;
	}
}

bool EditorNode::_is_class_editor_disabled_by_feature_profile(const StringName &p_class) {
	Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
	if (profile.is_null()) {
		return false;
	}

	StringName class_name = p_class;

	while (class_name != StringName()) {
		if (profile->is_class_disabled(class_name)) {
			return true;
		}
		if (profile->is_class_editor_disabled(class_name)) {
			return true;
		}
		class_name = ClassDB::get_parent_class(class_name);
	}

	return false;
}

void EditorNode::edit_item(Object *p_object, Object *p_editing_owner) {
	ERR_FAIL_NULL(p_editing_owner);

	// Editing for this type of object may be disabled by user's feature profile.
	if (!p_object || _is_class_editor_disabled_by_feature_profile(p_object->get_class())) {
		// Nothing to edit, clean up the owner context and return.
		hide_unused_editors(p_editing_owner);
		return;
	}

	// Get a list of editor plugins that can handle this type of object.
	Vector<EditorPlugin *> available_plugins = editor_data.get_handling_sub_editors(p_object);
	if (available_plugins.is_empty()) {
		// None, clean up the owner context and return.
		hide_unused_editors(p_editing_owner);
		return;
	}

	ObjectID owner_id = p_editing_owner->get_instance_id();

	// Remove editor plugins no longer used by this editing owner. Keep the ones that can
	// still be reused by the new edited object.

	List<EditorPlugin *> to_remove;
	for (EditorPlugin *plugin : active_plugins[owner_id]) {
		if (!available_plugins.has(plugin)) {
			to_remove.push_back(plugin);
			if (plugin->can_auto_hide()) {
				_plugin_over_edit(plugin, nullptr);
			} else {
				// If plugin can't be hidden, make it own itself and become responsible for closing.
				_plugin_over_self_own(plugin);
			}
		}
	}

	for (EditorPlugin *plugin : to_remove) {
		active_plugins[owner_id].erase(plugin);
	}

	LocalVector<EditorPlugin *> to_over_edit;

	// Send the edited object to the plugins.
	for (EditorPlugin *plugin : available_plugins) {
		if (active_plugins[owner_id].has(plugin)) {
			// Plugin was already active, just change the object and ensure it's visible.
			plugin->make_visible(true);
			plugin->edit(p_object);
			continue;
		}

		if (active_plugins.has(plugin->get_instance_id())) {
			// Plugin is already active, but as self-owning, so it needs a separate check.
			plugin->make_visible(true);
			plugin->edit(p_object);
			continue;
		}

		// If plugin is already associated with another owner, remove it from there first.
		for (KeyValue<ObjectID, HashSet<EditorPlugin *>> &kv : active_plugins) {
			if (kv.key != owner_id) {
				EditorPropertyResource *epres = Object::cast_to<EditorPropertyResource>(ObjectDB::get_instance(kv.key));
				if (epres && kv.value.has(plugin)) {
					// If it's resource property editing the same resource type, fold it.
					epres->fold_resource();
				}
				kv.value.erase(plugin);
			}
		}

		// Activate previously inactive plugin and edit the object.
		active_plugins[owner_id].insert(plugin);
		// TODO: Call the function directly once a proper priority system is implemented.
		to_over_edit.push_back(plugin);
	}

	for (EditorPlugin *plugin : to_over_edit) {
		_plugin_over_edit(plugin, p_object);
	}
}

void EditorNode::push_node_item(Node *p_node) {
	if (p_node || Object::cast_to<Node>(InspectorDock::get_inspector_singleton()->get_edited_object()) || Object::cast_to<MultiNodeEdit>(InspectorDock::get_inspector_singleton()->get_edited_object())) {
		// Don't push null if the currently edited object is not a Node.
		push_item(p_node);
	}
}

void EditorNode::push_item(Object *p_object, const String &p_property, bool p_inspector_only) {
	if (!p_object) {
		InspectorDock::get_inspector_singleton()->edit(nullptr);
		NodeDock::get_singleton()->set_node(nullptr);
		SceneTreeDock::get_singleton()->set_selected(nullptr);
		InspectorDock::get_singleton()->update(nullptr);
		hide_unused_editors();
		return;
	}
	_add_to_history(p_object, p_property, p_inspector_only);
	_edit_current();
}

void EditorNode::edit_previous_item() {
	if (editor_history.previous()) {
		_edit_current();
	}
}

void EditorNode::push_item_no_inspector(Object *p_object) {
	_add_to_history(p_object, "", false);
	_edit_current(false, true);
}

void EditorNode::save_default_environment() {
	Ref<Environment> fallback = get_tree()->get_root()->get_world_3d()->get_fallback_environment();

	if (fallback.is_valid() && fallback->get_path().is_resource_file()) {
		HashMap<Ref<Resource>, bool> processed;
		_find_and_save_edited_subresources(fallback.ptr(), processed, 0);
		save_resource_in_path(fallback, fallback->get_path());
	}
}

void EditorNode::hide_unused_editors(const Object *p_editing_owner) {
	if (p_editing_owner) {
		const ObjectID id = p_editing_owner->get_instance_id();
		for (EditorPlugin *plugin : active_plugins[id]) {
			if (plugin->can_auto_hide()) {
				_plugin_over_edit(plugin, nullptr);
			} else {
				_plugin_over_self_own(plugin);
			}
		}
		active_plugins.erase(id);
	} else {
		// If no editing owner is provided, this method will go over all owners and check if they are valid.
		// This is to sweep properties that were removed from the inspector.
		List<ObjectID> to_remove;
		for (KeyValue<ObjectID, HashSet<EditorPlugin *>> &kv : active_plugins) {
			Object *context = ObjectDB::get_instance(kv.key);
			if (context) {
				// In case of self-owning plugins, they are disabled here if they can auto hide.
				const EditorPlugin *self_owning = Object::cast_to<EditorPlugin>(context);
				if (self_owning && self_owning->can_auto_hide()) {
					context = nullptr;
				}
			}

			if (!context || context->call(SNAME("_should_stop_editing"))) {
				to_remove.push_back(kv.key);
				for (EditorPlugin *plugin : kv.value) {
					if (plugin->can_auto_hide()) {
						_plugin_over_edit(plugin, nullptr);
					} else {
						_plugin_over_self_own(plugin);
					}
				}
			}
		}

		for (const ObjectID &id : to_remove) {
			active_plugins.erase(id);
		}
	}
}

static bool overrides_external_editor(Object *p_object) {
	Script *script = Object::cast_to<Script>(p_object);

	if (!script) {
		return false;
	}

	return script->get_language()->overrides_external_editor();
}

void EditorNode::_add_to_history(const Object *p_object, const String &p_property, bool p_inspector_only) {
	ObjectID id = p_object->get_instance_id();
	ObjectID history_id = editor_history.get_current();
	if (id != history_id) {
		const MultiNodeEdit *multi_node_edit = Object::cast_to<const MultiNodeEdit>(p_object);
		const MultiNodeEdit *history_multi_node_edit = Object::cast_to<const MultiNodeEdit>(ObjectDB::get_instance(history_id));
		if (multi_node_edit && history_multi_node_edit && multi_node_edit->is_same_selection(history_multi_node_edit)) {
			return;
		}
		if (p_inspector_only) {
			editor_history.add_object(id, String(), true);
		} else if (p_property.is_empty()) {
			editor_history.add_object(id);
		} else {
			editor_history.add_object(id, p_property);
		}
	}
}

void EditorNode::_edit_current(bool p_skip_foreign, bool p_skip_inspector_update) {
	ObjectID current_id = editor_history.get_current();
	Object *current_obj = current_id.is_valid() ? ObjectDB::get_instance(current_id) : nullptr;

	Ref<Resource> res = Object::cast_to<Resource>(current_obj);
	if (p_skip_foreign && res.is_valid()) {
		const int current_tab = scene_tabs->get_current_tab();
		if (res->get_path().contains("::") && res->get_path().get_slice("::", 0) != editor_data.get_scene_path(current_tab)) {
			// Trying to edit resource that belongs to another scene; abort.
			current_obj = nullptr;
		}
	}

	bool inspector_only = editor_history.is_current_inspector_only();
	current = current_obj;

	if (!current_obj) {
		SceneTreeDock::get_singleton()->set_selected(nullptr);
		InspectorDock::get_inspector_singleton()->edit(nullptr);
		NodeDock::get_singleton()->set_node(nullptr);
		InspectorDock::get_singleton()->update(nullptr);
		hide_unused_editors();
		return;
	}

	// Update the use folding setting and state.
	bool disable_folding = bool(EDITOR_GET("interface/inspector/disable_folding")) || current_obj->is_class("EditorDebuggerRemoteObject");
	if (InspectorDock::get_inspector_singleton()->is_using_folding() == disable_folding) {
		InspectorDock::get_inspector_singleton()->set_use_folding(!disable_folding, false);
	}

	bool is_resource = current_obj->is_class("Resource");
	bool is_node = current_obj->is_class("Node");
	bool stay_in_script_editor_on_node_selected = bool(EDITOR_GET("text_editor/behavior/navigation/stay_in_script_editor_on_node_selected"));
	bool skip_main_plugin = false;

	String editable_info; // None by default.
	bool info_is_warning = false;

	if (current_obj->has_method("_is_read_only")) {
		if (current_obj->call("_is_read_only")) {
			editable_info = TTR("This object is marked as read-only, so it's not editable.");
		}
	}

	if (is_resource) {
		Resource *current_res = Object::cast_to<Resource>(current_obj);
		ERR_FAIL_NULL(current_res);

		if (!p_skip_inspector_update) {
			InspectorDock::get_inspector_singleton()->edit(current_res);
			SceneTreeDock::get_singleton()->set_selected(nullptr);
			NodeDock::get_singleton()->set_node(nullptr);
			InspectorDock::get_singleton()->update(nullptr);
			ImportDock::get_singleton()->set_edit_path(current_res->get_path());
		}

		int subr_idx = current_res->get_path().find("::");
		if (subr_idx != -1) {
			String base_path = current_res->get_path().substr(0, subr_idx);
			if (FileAccess::exists(base_path + ".import")) {
				if (!base_path.is_resource_file()) {
					if (get_edited_scene() && get_edited_scene()->get_scene_file_path() == base_path) {
						info_is_warning = true;
					}
				}
				editable_info = TTR("This resource belongs to a scene that was imported, so it's not editable.\nPlease read the documentation relevant to importing scenes to better understand this workflow.");
			} else if ((!get_edited_scene() || get_edited_scene()->get_scene_file_path() != base_path) && ResourceLoader::get_resource_type(base_path) == "PackedScene") {
				editable_info = TTR("This resource belongs to a scene that was instantiated or inherited.\nChanges to it must be made inside the original scene.");
			}
		} else if (current_res->get_path().is_resource_file()) {
			if (FileAccess::exists(current_res->get_path() + ".import")) {
				editable_info = TTR("This resource was imported, so it's not editable. Change its settings in the import panel and then re-import.");
			}
		}
	} else if (is_node) {
		Node *current_node = Object::cast_to<Node>(current_obj);
		ERR_FAIL_NULL(current_node);

		InspectorDock::get_inspector_singleton()->edit(current_node);
		if (current_node->is_inside_tree()) {
			NodeDock::get_singleton()->set_node(current_node);
			SceneTreeDock::get_singleton()->set_selected(current_node);
			SceneTreeDock::get_singleton()->set_selection({ current_node });
			InspectorDock::get_singleton()->update(current_node);
			if (!inspector_only && !skip_main_plugin) {
				skip_main_plugin = stay_in_script_editor_on_node_selected && !ScriptEditor::get_singleton()->is_editor_floating() && ScriptEditor::get_singleton()->is_visible_in_tree();
			}
		} else {
			NodeDock::get_singleton()->set_node(nullptr);
			SceneTreeDock::get_singleton()->set_selected(nullptr);
			InspectorDock::get_singleton()->update(nullptr);
		}

		if (get_edited_scene() && !get_edited_scene()->get_scene_file_path().is_empty()) {
			String source_scene = get_edited_scene()->get_scene_file_path();
			if (FileAccess::exists(source_scene + ".import")) {
				editable_info = TTR("This scene was imported, so changes to it won't be kept.\nInstantiating or inheriting it will allow you to make changes to it.\nPlease read the documentation relevant to importing scenes to better understand this workflow.");
				info_is_warning = true;
			}
		}

	} else {
		Node *selected_node = nullptr;

		Vector<Node *> multi_nodes;
		if (current_obj->is_class("MultiNodeEdit")) {
			Node *scene = get_edited_scene();
			if (scene) {
				MultiNodeEdit *multi_node_edit = Object::cast_to<MultiNodeEdit>(current_obj);
				int node_count = multi_node_edit->get_node_count();
				if (node_count > 0) {
					for (int node_index = 0; node_index < node_count; ++node_index) {
						Node *node = scene->get_node(multi_node_edit->get_node(node_index));
						if (node) {
							multi_nodes.push_back(node);
						}
					}
					if (!multi_nodes.is_empty()) {
						// Pick the top-most node.
						multi_nodes.sort_custom<Node::Comparator>();
						selected_node = multi_nodes[0];
					}
				}
			}
		}

		InspectorDock::get_inspector_singleton()->edit(current_obj);
		NodeDock::get_singleton()->set_node(nullptr);
		SceneTreeDock::get_singleton()->set_selected(selected_node);
		SceneTreeDock::get_singleton()->set_selection(multi_nodes);
		InspectorDock::get_singleton()->update(nullptr);
	}

	InspectorDock::get_singleton()->set_info(
			info_is_warning ? TTR("Changes may be lost!") : TTR("This object is read-only."),
			editable_info,
			info_is_warning);

	Object *editor_owner = (is_node || current_obj->is_class("MultiNodeEdit")) ? (Object *)SceneTreeDock::get_singleton() : is_resource ? (Object *)InspectorDock::get_inspector_singleton()
																																		: (Object *)this;

	// Take care of the main editor plugin.

	if (!inspector_only) {
		EditorPlugin *main_plugin = editor_data.get_handling_main_editor(current_obj);

		int plugin_index = editor_main_screen->get_plugin_index(main_plugin);
		if (main_plugin && plugin_index >= 0 && !editor_main_screen->is_button_enabled(plugin_index)) {
			main_plugin = nullptr;
		}
		EditorPlugin *editor_plugin_screen = editor_main_screen->get_selected_plugin();

		ObjectID editor_owner_id = editor_owner->get_instance_id();
		if (main_plugin && !skip_main_plugin) {
			// Special case if use of external editor is true.
			Resource *current_res = Object::cast_to<Resource>(current_obj);
			if (main_plugin->get_plugin_name() == "Script" && current_res && !current_res->is_built_in() && (bool(EDITOR_GET("text_editor/external/use_external_editor")) || overrides_external_editor(current_obj))) {
				if (!changing_scene) {
					main_plugin->edit(current_obj);
				}
			}

			else if (main_plugin != editor_plugin_screen && (!ScriptEditor::get_singleton() || !ScriptEditor::get_singleton()->is_visible_in_tree() || ScriptEditor::get_singleton()->can_take_away_focus())) {
				// Unedit previous plugin.
				editor_plugin_screen->edit(nullptr);
				active_plugins[editor_owner_id].erase(editor_plugin_screen);
				// Update screen main_plugin.
				editor_main_screen->select(plugin_index);
				main_plugin->edit(current_obj);
			} else {
				editor_plugin_screen->edit(current_obj);
			}
			is_main_screen_editing = true;
		} else if (!main_plugin && editor_plugin_screen && is_main_screen_editing) {
			editor_plugin_screen->edit(nullptr);
			is_main_screen_editing = false;
		}

		edit_item(current_obj, editor_owner);
	}

	InspectorDock::get_singleton()->update(current_obj);
}

void EditorNode::_android_build_source_selected(const String &p_file) {
	export_template_manager->install_android_template_from_file(p_file, android_export_preset);
}

void EditorNode::_android_export_preset_selected(int p_index) {
	if (p_index >= 0) {
		android_export_preset = EditorExport::get_singleton()->get_export_preset(choose_android_export_profile->get_item_id(p_index));
	} else {
		android_export_preset.unref();
	}
	install_android_build_template_message->set_text(vformat(TTR(INSTALL_ANDROID_BUILD_TEMPLATE_MESSAGE), export_template_manager->get_android_build_directory(android_export_preset)));
}

void EditorNode::_menu_option_confirm(int p_option, bool p_confirmed) {
	if (!p_confirmed) { // FIXME: this may be a hack.
		current_menu_option = (MenuOptions)p_option;
	}

	switch (p_option) {
		case FILE_NEW_SCENE: {
			new_scene();

		} break;
		case FILE_NEW_INHERITED_SCENE:
		case FILE_OPEN_SCENE: {
			file->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);
			file->clear_filters();
			for (const String &extension : extensions) {
				file->add_filter("*." + extension, extension.to_upper());
			}

			Node *scene = editor_data.get_edited_scene_root();
			if (scene) {
				file->set_current_path(scene->get_scene_file_path());
			};
			file->set_title(p_option == FILE_OPEN_SCENE ? TTR("Open Scene") : TTR("Open Base Scene"));
			file->popup_file_dialog();

		} break;
		case FILE_QUICK_OPEN: {
			quick_open_dialog->popup_dialog({ "Resource" }, callable_mp(this, &EditorNode::_quick_opened));
		} break;
		case FILE_QUICK_OPEN_SCENE: {
			quick_open_dialog->popup_dialog({ "PackedScene" }, callable_mp(this, &EditorNode::_quick_opened));
		} break;
		case FILE_QUICK_OPEN_SCRIPT: {
			quick_open_dialog->popup_dialog({ "Script" }, callable_mp(this, &EditorNode::_quick_opened));
		} break;
		case FILE_OPEN_PREV: {
			if (previous_scenes.is_empty()) {
				break;
			}
			opening_prev = true;
			open_request(previous_scenes.back()->get());
			previous_scenes.pop_back();

		} break;
		case FILE_CLOSE_OTHERS: {
			tab_closing_menu_option = -1;
			for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
				if (i == editor_data.get_edited_scene()) {
					continue;
				}
				tabs_to_close.push_back(editor_data.get_scene_path(i));
			}
			_proceed_closing_scene_tabs();
		} break;
		case FILE_CLOSE_RIGHT: {
			tab_closing_menu_option = -1;
			for (int i = editor_data.get_edited_scene() + 1; i < editor_data.get_edited_scene_count(); i++) {
				tabs_to_close.push_back(editor_data.get_scene_path(i));
			}
			_proceed_closing_scene_tabs();
		} break;
		case FILE_CLOSE_ALL: {
			tab_closing_menu_option = -1;
			for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
				tabs_to_close.push_back(editor_data.get_scene_path(i));
			}
			_proceed_closing_scene_tabs();
		} break;
		case FILE_CLOSE: {
			_scene_tab_closed(editor_data.get_edited_scene());
		} break;
		case FILE_SAVE_SCENE_SILENTLY: {
			// Save scene without displaying progress dialog. Used to work around
			// errors about parent node being busy setting up children
			// when Save on Focus Loss kicks in.
			Node *scene = editor_data.get_edited_scene_root();
			if (scene && !scene->get_scene_file_path().is_empty() && DirAccess::exists(scene->get_scene_file_path().get_base_dir())) {
				_save_scene(scene->get_scene_file_path());
				save_editor_layout_delayed();
			}
		} break;
		case SCENE_TAB_CLOSE:
		case FILE_SAVE_SCENE: {
			int scene_idx = (p_option == FILE_SAVE_SCENE) ? -1 : tab_closing_idx;
			Node *scene = editor_data.get_edited_scene_root(scene_idx);
			if (scene && !scene->get_scene_file_path().is_empty()) {
				if (DirAccess::exists(scene->get_scene_file_path().get_base_dir())) {
					if (scene_idx != editor_data.get_edited_scene()) {
						_save_scene_with_preview(scene->get_scene_file_path(), scene_idx);
					} else {
						_save_scene_with_preview(scene->get_scene_file_path());
					}

					if (scene_idx != -1) {
						_discard_changes();
					}
					save_editor_layout_delayed();
				} else {
					show_save_accept(vformat(TTR("%s no longer exists! Please specify a new save location."), scene->get_scene_file_path().get_base_dir()), TTR("OK"));
				}
				break;
			}
			[[fallthrough]];
		}
		case FILE_SAVE_AS_SCENE: {
			int scene_idx = (p_option == FILE_SAVE_SCENE || p_option == FILE_SAVE_AS_SCENE) ? -1 : tab_closing_idx;

			Node *scene = editor_data.get_edited_scene_root(scene_idx);

			if (!scene) {
				if (p_option == FILE_SAVE_SCENE) {
					// Pressing Ctrl + S saves the current script if a scene is currently open, but it won't if the scene has no root node.
					// Work around this by explicitly saving the script in this case (similar to pressing Ctrl + Alt + S).
					ScriptEditor::get_singleton()->save_current_script();
				}

				const int saved = _save_external_resources(true);
				if (saved > 0) {
					EditorToaster::get_singleton()->popup_str(vformat(TTR("The current scene has no root node, but %d modified external resource(s) and/or plugin data were saved anyway."), saved), EditorToaster::SEVERITY_INFO);
				} else if (p_option == FILE_SAVE_AS_SCENE) {
					// Don't show this dialog when pressing Ctrl + S to avoid interfering with script saving.
					show_accept(
							TTR("A root node is required to save the scene. You can add a root node using the Scene tree dock."),
							TTR("OK"));
				}

				break;
			}

			file->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);

			List<String> extensions;
			Ref<PackedScene> sd = memnew(PackedScene);
			ResourceSaver::get_recognized_extensions(sd, &extensions);
			file->clear_filters();
			for (const String &extension : extensions) {
				file->add_filter("*." + extension, extension.to_upper());
			}

			if (!scene->get_scene_file_path().is_empty()) {
				String path = scene->get_scene_file_path();
				file->set_current_path(path);
				if (extensions.size()) {
					String ext = path.get_extension().to_lower();
					if (extensions.find(ext) == nullptr) {
						file->set_current_path(path.replacen("." + ext, "." + extensions.front()->get()));
					}
				}
			} else if (extensions.size()) {
				String root_name = scene->get_name();
				root_name = EditorNode::adjust_scene_name_casing(root_name);
				file->set_current_path(root_name + "." + extensions.front()->get().to_lower());
			}
			file->set_title(TTR("Save Scene As..."));
			file->popup_file_dialog();

		} break;

		case FILE_SAVE_ALL_SCENES: {
			_save_all_scenes();
		} break;

		case FILE_RUN_SCENE: {
			project_run_bar->play_current_scene();
		} break;

		case FILE_EXPORT_PROJECT: {
			project_export->popup_export();
		} break;

		case FILE_EXTERNAL_OPEN_SCENE: {
			if (unsaved_cache && !p_confirmed) {
				confirmation->set_ok_button_text(TTR("Open"));
				confirmation->set_text(TTR("Current scene not saved. Open anyway?"));
				confirmation->popup_centered();
				break;
			}

			bool oprev = opening_prev;
			Error err = load_scene(external_file);
			if (err == OK && oprev) {
				previous_scenes.pop_back();
				opening_prev = false;
			}

		} break;

		case EDIT_UNDO: {
			if ((int)Input::get_singleton()->get_mouse_button_mask() & 0x7) {
				log->add_message(TTR("Can't undo while mouse buttons are pressed."), EditorLog::MSG_TYPE_EDITOR);
			} else {
				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
				String action = undo_redo->get_current_action_name();
				int id = undo_redo->get_current_action_history_id();
				if (!undo_redo->undo()) {
					log->add_message(TTR("Nothing to undo."), EditorLog::MSG_TYPE_EDITOR);
				} else if (!action.is_empty()) {
					switch (id) {
						case EditorUndoRedoManager::GLOBAL_HISTORY:
							log->add_message(vformat(TTR("Global Undo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						case EditorUndoRedoManager::REMOTE_HISTORY:
							log->add_message(vformat(TTR("Remote Undo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						default:
							log->add_message(vformat(TTR("Scene Undo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
					}
				}
			}
		} break;
		case EDIT_REDO: {
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			if ((int)Input::get_singleton()->get_mouse_button_mask() & 0x7) {
				log->add_message(TTR("Can't redo while mouse buttons are pressed."), EditorLog::MSG_TYPE_EDITOR);
			} else {
				if (!undo_redo->redo()) {
					log->add_message(TTR("Nothing to redo."), EditorLog::MSG_TYPE_EDITOR);
				} else {
					String action = undo_redo->get_current_action_name();
					if (action.is_empty()) {
						break;
					}

					switch (undo_redo->get_current_action_history_id()) {
						case EditorUndoRedoManager::GLOBAL_HISTORY:
							log->add_message(vformat(TTR("Global Redo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						case EditorUndoRedoManager::REMOTE_HISTORY:
							log->add_message(vformat(TTR("Remote Redo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						default:
							log->add_message(vformat(TTR("Scene Redo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
					}
				}
			}
		} break;

		case EDIT_RELOAD_SAVED_SCENE: {
			Node *scene = get_edited_scene();

			if (!scene) {
				break;
			}

			String filename = scene->get_scene_file_path();

			if (filename.is_empty()) {
				show_warning(TTR("Can't reload a scene that was never saved."));
				break;
			}

			if (unsaved_cache && !p_confirmed) {
				confirmation->set_ok_button_text(TTR("Reload Saved Scene"));
				confirmation->set_text(
						TTR("The current scene has unsaved changes.\nReload the saved scene anyway? This action cannot be undone."));
				confirmation->popup_centered();
				break;
			}

			int cur_idx = editor_data.get_edited_scene();
			_remove_edited_scene();
			Error err = load_scene(filename);
			if (err != OK) {
				ERR_PRINT("Failed to load scene");
			}
			editor_data.move_edited_scene_to_index(cur_idx);
			EditorUndoRedoManager::get_singleton()->clear_history(editor_data.get_current_edited_scene_history_id(), false);
			scene_tabs->set_current_tab(cur_idx);

		} break;

		case FILE_SHOW_IN_FILESYSTEM: {
			String path = editor_data.get_scene_path(editor_data.get_edited_scene());
			if (!path.is_empty()) {
				FileSystemDock::get_singleton()->navigate_to_path(path);
			}
		} break;

		case RUN_SETTINGS: {
			project_settings_editor->popup_project_settings();
		} break;
		case FILE_INSTALL_ANDROID_SOURCE: {
			if (p_confirmed) {
				if (export_template_manager->is_android_template_installed(android_export_preset)) {
					remove_android_build_template->set_text(vformat(TTR(REMOVE_ANDROID_BUILD_TEMPLATE_MESSAGE), export_template_manager->get_android_build_directory(android_export_preset)));
					remove_android_build_template->popup_centered();
				} else if (!export_template_manager->can_install_android_template(android_export_preset)) {
					gradle_build_manage_templates->popup_centered();
				} else {
					export_template_manager->install_android_template(android_export_preset);
				}
			} else {
				bool has_custom_gradle_build = false;
				choose_android_export_profile->clear();
				for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
					Ref<EditorExportPreset> export_preset = EditorExport::get_singleton()->get_export_preset(i);
					if (export_preset->get_platform()->get_class_name() == "EditorExportPlatformAndroid" && (bool)export_preset->get("gradle_build/use_gradle_build")) {
						choose_android_export_profile->add_item(export_preset->get_name(), i);
						String gradle_build_directory = export_preset->get("gradle_build/gradle_build_directory");
						String android_source_template = export_preset->get("gradle_build/android_source_template");
						if (!android_source_template.is_empty() || (gradle_build_directory != "" && gradle_build_directory != "res://android")) {
							has_custom_gradle_build = true;
						}
					}
				}
				_android_export_preset_selected(choose_android_export_profile->get_item_count() >= 1 ? 0 : -1);

				if (choose_android_export_profile->get_item_count() > 1 && has_custom_gradle_build) {
					// If there's multiple options and at least one of them uses a custom gradle build then prompt the user to choose.
					choose_android_export_profile->show();
					install_android_build_template->popup_centered();
				} else {
					choose_android_export_profile->hide();

					if (export_template_manager->is_android_template_installed(android_export_preset)) {
						remove_android_build_template->set_text(vformat(TTR(REMOVE_ANDROID_BUILD_TEMPLATE_MESSAGE), export_template_manager->get_android_build_directory(android_export_preset)));
						remove_android_build_template->popup_centered();
					} else if (export_template_manager->can_install_android_template(android_export_preset)) {
						install_android_build_template->popup_centered();
					} else {
						gradle_build_manage_templates->popup_centered();
					}
				}
			}
		} break;
		case RUN_USER_DATA_FOLDER: {
			// Ensure_user_data_dir() to prevent the edge case: "Open User Data Folder" won't work after the project was renamed in ProjectSettingsEditor unless the project is saved.
			OS::get_singleton()->ensure_user_data_dir();
			OS::get_singleton()->shell_show_in_file_manager(OS::get_singleton()->get_user_data_dir(), true);
		} break;
		case FILE_EXPLORE_ANDROID_BUILD_TEMPLATES: {
			OS::get_singleton()->shell_show_in_file_manager(ProjectSettings::get_singleton()->globalize_path(export_template_manager->get_android_build_directory(android_export_preset).get_base_dir()), true);
		} break;
		case FILE_QUIT:
		case RUN_PROJECT_MANAGER:
		case RELOAD_CURRENT_PROJECT: {
			if (p_confirmed && plugin_to_save) {
				plugin_to_save->save_external_data();
				p_confirmed = false;
			}

			if (!p_confirmed) {
				bool save_each = EDITOR_GET("interface/editor/save_each_scene_on_quit");
				if (_next_unsaved_scene(!save_each) == -1) {
					if (EditorUndoRedoManager::get_singleton()->is_history_unsaved(EditorUndoRedoManager::GLOBAL_HISTORY)) {
						if (p_option == RELOAD_CURRENT_PROJECT) {
							save_confirmation->set_ok_button_text(TTR("Save & Reload"));
							save_confirmation->set_text(TTR("Save modified resources before reloading?"));
						} else {
							save_confirmation->set_ok_button_text(TTR("Save & Quit"));
							save_confirmation->set_text(TTR("Save modified resources before closing?"));
						}
						save_confirmation->reset_size();
						save_confirmation->popup_centered();
						break;
					}

					plugin_to_save = nullptr;
					for (int i = 0; i < editor_data.get_editor_plugin_count(); i++) {
						const String unsaved_status = editor_data.get_editor_plugin(i)->get_unsaved_status();
						if (!unsaved_status.is_empty()) {
							if (p_option == RELOAD_CURRENT_PROJECT) {
								save_confirmation->set_ok_button_text(TTR("Save & Reload"));
								save_confirmation->set_text(unsaved_status);
							} else {
								save_confirmation->set_ok_button_text(TTR("Save & Quit"));
								save_confirmation->set_text(unsaved_status);
							}
							save_confirmation->reset_size();
							save_confirmation->popup_centered();
							plugin_to_save = editor_data.get_editor_plugin(i);
							break;
						}
					}

					if (plugin_to_save) {
						break;
					}

					_discard_changes();
					break;
				}

				if (save_each) {
					tab_closing_menu_option = current_menu_option;
					for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
						tabs_to_close.push_back(editor_data.get_scene_path(i));
					}
					_proceed_closing_scene_tabs();
				} else {
					String unsaved_scenes;
					int i = _next_unsaved_scene(true, 0);
					while (i != -1) {
						unsaved_scenes += "\n            " + editor_data.get_edited_scene_root(i)->get_scene_file_path();
						i = _next_unsaved_scene(true, ++i);
					}
					if (p_option == RELOAD_CURRENT_PROJECT) {
						save_confirmation->set_ok_button_text(TTR("Save & Reload"));
						save_confirmation->set_text(TTR("Save changes to the following scene(s) before reloading?") + unsaved_scenes);
					} else {
						save_confirmation->set_ok_button_text(TTR("Save & Quit"));
						save_confirmation->set_text((p_option == FILE_QUIT ? TTR("Save changes to the following scene(s) before quitting?") : TTR("Save changes to the following scene(s) before opening Project Manager?")) + unsaved_scenes);
					}
					save_confirmation->reset_size();
					save_confirmation->popup_centered();
				}

				DisplayServer::get_singleton()->window_request_attention();
				break;
			}
			_save_external_resources();
			_discard_changes();
		} break;
		case SETTINGS_UPDATE_CONTINUOUSLY: {
			EditorSettings::get_singleton()->set("interface/editor/update_continuously", true);
			_update_update_spinner();
			show_accept(TTR("This option is deprecated. Situations where refresh must be forced are now considered a bug. Please report."), TTR("OK"));
		} break;
		case SETTINGS_UPDATE_WHEN_CHANGED: {
			EditorSettings::get_singleton()->set("interface/editor/update_continuously", false);
			_update_update_spinner();
		} break;
		case SETTINGS_UPDATE_SPINNER_HIDE: {
			EditorSettings::get_singleton()->set("interface/editor/show_update_spinner", 2); // Disabled
			_update_update_spinner();
		} break;
		case SETTINGS_PREFERENCES: {
			editor_settings_dialog->popup_edit_settings();
		} break;
		case SETTINGS_EDITOR_DATA_FOLDER: {
			OS::get_singleton()->shell_show_in_file_manager(EditorPaths::get_singleton()->get_data_dir(), true);
		} break;
		case SETTINGS_EDITOR_CONFIG_FOLDER: {
			OS::get_singleton()->shell_show_in_file_manager(EditorPaths::get_singleton()->get_config_dir(), true);
		} break;
		case SETTINGS_MANAGE_EXPORT_TEMPLATES: {
			export_template_manager->popup_manager();
		} break;
		case SETTINGS_MANAGE_FBX_IMPORTER: {
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
			fbx_importer_manager->show_dialog();
#endif
		} break;
		case SETTINGS_INSTALL_ANDROID_BUILD_TEMPLATE: {
			gradle_build_manage_templates->hide();
			file_android_build_source->popup_centered_ratio();
		} break;
		case SETTINGS_MANAGE_FEATURE_PROFILES: {
			feature_profile_manager->popup_centered_clamped(Size2(900, 800) * EDSCALE, 0.8);
		} break;
		case SETTINGS_TOGGLE_FULLSCREEN: {
			DisplayServer::WindowMode mode = DisplayServer::get_singleton()->window_get_mode();
			if (mode == DisplayServer::WINDOW_MODE_FULLSCREEN || mode == DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
				DisplayServer::get_singleton()->window_set_mode(prev_mode);
			} else {
				prev_mode = mode;
				DisplayServer::get_singleton()->window_set_mode(DisplayServer::WINDOW_MODE_FULLSCREEN);
			}
		} break;
		case EDITOR_SCREENSHOT: {
			screenshot_timer->start();
		} break;
		case SETTINGS_PICK_MAIN_SCENE: {
			file->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);
			file->clear_filters();
			for (const String &extension : extensions) {
				file->add_filter("*." + extension, extension.to_upper());
			}

			Node *scene = editor_data.get_edited_scene_root();
			if (scene) {
				file->set_current_path(scene->get_scene_file_path());
			}
			file->set_title(TTR("Pick a Main Scene"));
			file->popup_file_dialog();

		} break;
		case HELP_SEARCH: {
			emit_signal(SNAME("request_help_search"), "");
		} break;
		case HELP_COMMAND_PALETTE: {
			command_palette->open_popup();
		} break;
		case HELP_DOCS: {
			OS::get_singleton()->shell_open(VERSION_DOCS_URL "/");
		} break;
		case HELP_FORUM: {
			OS::get_singleton()->shell_open("https://forum.godotengine.org/");
		} break;
		case HELP_REPORT_A_BUG: {
			OS::get_singleton()->shell_open("https://github.com/godotengine/godot/issues");
		} break;
		case HELP_COPY_SYSTEM_INFO: {
			String info = _get_system_info();
			DisplayServer::get_singleton()->clipboard_set(info);
		} break;
		case HELP_SUGGEST_A_FEATURE: {
			OS::get_singleton()->shell_open("https://github.com/godotengine/godot-proposals#readme");
		} break;
		case HELP_SEND_DOCS_FEEDBACK: {
			OS::get_singleton()->shell_open("https://github.com/godotengine/godot-docs/issues");
		} break;
		case HELP_COMMUNITY: {
			OS::get_singleton()->shell_open("https://godotengine.org/community");
		} break;
		case HELP_ABOUT: {
			about->popup_centered(Size2(780, 500) * EDSCALE);
		} break;
		case HELP_SUPPORT_GODOT_DEVELOPMENT: {
			OS::get_singleton()->shell_open("https://godotengine.org/donate");
		} break;
		case SET_RENDERER_NAME_SAVE_AND_RESTART: {
			ProjectSettings::get_singleton()->set("rendering/renderer/rendering_method", renderer_request);
			if (renderer_request == "mobile" || renderer_request == "gl_compatibility") {
				// Also change the mobile override if changing to a compatible rendering method.
				// This prevents visual discrepancies between desktop and mobile platforms.
				ProjectSettings::get_singleton()->set("rendering/renderer/rendering_method.mobile", renderer_request);
			} else if (renderer_request == "forward_plus") {
				// Use the equivalent mobile rendering method. This prevents the rendering method from staying
				// on its old choice if moving from `gl_compatibility` to `forward_plus`.
				ProjectSettings::get_singleton()->set("rendering/renderer/rendering_method.mobile", "mobile");
			}

			ProjectSettings::get_singleton()->save();

			save_all_scenes();
			restart_editor();
		} break;
	}
}

String EditorNode::adjust_scene_name_casing(const String &p_root_name) {
	switch (GLOBAL_GET("editor/naming/scene_name_casing").operator int()) {
		case SCENE_NAME_CASING_AUTO:
			// Use casing of the root node.
			break;
		case SCENE_NAME_CASING_PASCAL_CASE:
			return p_root_name.replace("-", "_").to_pascal_case();
		case SCENE_NAME_CASING_SNAKE_CASE:
			return p_root_name.replace("-", "_").to_snake_case();
		case SCENE_NAME_CASING_KEBAB_CASE:
			return p_root_name.to_snake_case().replace("_", "-");
	}
	return p_root_name;
}

String EditorNode::adjust_script_name_casing(const String &p_file_name, ScriptLanguage::ScriptNameCasing p_auto_casing) {
	int editor_casing = GLOBAL_GET("editor/naming/script_name_casing");
	if (editor_casing == ScriptLanguage::SCRIPT_NAME_CASING_AUTO) {
		// Use the script language's preferred casing.
		editor_casing = p_auto_casing;
	}

	switch (editor_casing) {
		case ScriptLanguage::SCRIPT_NAME_CASING_AUTO:
			// Script language has no preference, so do not adjust.
			break;
		case ScriptLanguage::SCRIPT_NAME_CASING_PASCAL_CASE:
			return p_file_name.replace("-", "_").to_pascal_case();
		case ScriptLanguage::SCRIPT_NAME_CASING_SNAKE_CASE:
			return p_file_name.replace("-", "_").to_snake_case();
		case ScriptLanguage::SCRIPT_NAME_CASING_KEBAB_CASE:
			return p_file_name.to_snake_case().replace("_", "-");
	}
	return p_file_name;
}

void EditorNode::_request_screenshot() {
	_screenshot();
}

void EditorNode::_screenshot(bool p_use_utc) {
	String name = "editor_screenshot_" + Time::get_singleton()->get_datetime_string_from_system(p_use_utc).replace(":", "") + ".png";
	NodePath path = String("user://") + name;
	_save_screenshot(path);
	if (EDITOR_GET("interface/editor/automatically_open_screenshots")) {
		OS::get_singleton()->shell_show_in_file_manager(ProjectSettings::get_singleton()->globalize_path(path), true);
	}
}

void EditorNode::_save_screenshot(NodePath p_path) {
	Control *main_screen_control = editor_main_screen->get_control();
	ERR_FAIL_NULL_MSG(main_screen_control, "Cannot get the editor main screen control.");
	Viewport *viewport = main_screen_control->get_viewport();
	ERR_FAIL_NULL_MSG(viewport, "Cannot get a viewport from the editor main screen.");
	Ref<ViewportTexture> texture = viewport->get_texture();
	ERR_FAIL_COND_MSG(texture.is_null(), "Cannot get a viewport texture from the editor main screen.");
	Ref<Image> img = texture->get_image();
	ERR_FAIL_COND_MSG(img.is_null(), "Cannot get an image from a viewport texture of the editor main screen.");
	Error error = img->save_png(p_path);
	ERR_FAIL_COND_MSG(error != OK, "Cannot save screenshot to file '" + p_path + "'.");
}

void EditorNode::_check_system_theme_changed() {
	DisplayServer *display_server = DisplayServer::get_singleton();

	bool system_theme_changed = false;

	if (follow_system_theme) {
		if (display_server->get_base_color() != last_system_base_color) {
			system_theme_changed = true;
			last_system_base_color = display_server->get_base_color();
		}

		if (display_server->is_dark_mode_supported() && display_server->is_dark_mode() != last_dark_mode_state) {
			system_theme_changed = true;
			last_dark_mode_state = display_server->is_dark_mode();
		}
	}

	if (use_system_accent_color) {
		if (display_server->get_accent_color() != last_system_accent_color) {
			system_theme_changed = true;
			last_system_accent_color = display_server->get_accent_color();
		}
	}

	if (system_theme_changed) {
		_update_theme();
	}
}

void EditorNode::_tool_menu_option(int p_idx) {
	switch (tool_menu->get_item_id(p_idx)) {
		case TOOLS_ORPHAN_RESOURCES: {
			orphan_resources->show();
		} break;
		case TOOLS_BUILD_PROFILE_MANAGER: {
			build_profile_manager->popup_centered_clamped(Size2(700, 800) * EDSCALE, 0.8);
		} break;
		case TOOLS_SURFACE_UPGRADE: {
			surface_upgrade_dialog->popup_on_demand();
		} break;
		case TOOLS_CUSTOM: {
			if (tool_menu->get_item_submenu(p_idx) == "") {
				Callable callback = tool_menu->get_item_metadata(p_idx);
				Callable::CallError ce;
				Variant result;
				callback.callp(nullptr, 0, result, ce);

				if (ce.error != Callable::CallError::CALL_OK) {
					String err = Variant::get_callable_error_text(callback, nullptr, 0, ce);
					ERR_PRINT("Error calling function from tool menu: " + err);
				}
			} // Else it's a submenu so don't do anything.
		} break;
	}
}

void EditorNode::_export_as_menu_option(int p_idx) {
	if (p_idx == 0) { // MeshLibrary
		current_menu_option = FILE_EXPORT_MESH_LIBRARY;

		if (!editor_data.get_edited_scene_root()) {
			show_accept(TTR("This operation can't be done without a scene."), TTR("OK"));
			return;
		}

		List<String> extensions;
		Ref<MeshLibrary> ml(memnew(MeshLibrary));
		ResourceSaver::get_recognized_extensions(ml, &extensions);
		file_export_lib->clear_filters();
		for (const String &E : extensions) {
			file_export_lib->add_filter("*." + E);
		}

		file_export_lib->set_title(TTR("Export Mesh Library"));
		file_export_lib->popup_file_dialog();
	} else { // Custom menu options added by plugins
		if (export_as_menu->get_item_submenu(p_idx).is_empty()) { // If not a submenu
			Callable callback = export_as_menu->get_item_metadata(p_idx);
			Callable::CallError ce;
			Variant result;
			callback.callp(nullptr, 0, result, ce);

			if (ce.error != Callable::CallError::CALL_OK) {
				String err = Variant::get_callable_error_text(callback, nullptr, 0, ce);
				ERR_PRINT("Error calling function from export_as menu: " + err);
			}
		}
	}
}

int EditorNode::_next_unsaved_scene(bool p_valid_filename, int p_start) {
	for (int i = p_start; i < editor_data.get_edited_scene_count(); i++) {
		if (!editor_data.get_edited_scene_root(i)) {
			continue;
		}

		String scene_filename = editor_data.get_edited_scene_root(i)->get_scene_file_path();
		if (p_valid_filename && scene_filename.is_empty()) {
			continue;
		}

		bool unsaved = EditorUndoRedoManager::get_singleton()->is_history_unsaved(editor_data.get_scene_history_id(i));
		if (unsaved) {
			return i;
		} else {
			for (int j = 0; j < editor_data.get_editor_plugin_count(); j++) {
				if (!editor_data.get_editor_plugin(j)->get_unsaved_status(scene_filename).is_empty()) {
					return i;
				}
			}
		}
	}
	return -1;
}

void EditorNode::_exit_editor(int p_exit_code) {
	exiting = true;
	resource_preview->stop(); // Stop early to avoid crashes.
	_save_editor_layout();

	// Dim the editor window while it's quitting to make it clearer that it's busy.
	dim_editor(true);

	// Unload addons before quitting to allow cleanup.
	unload_editor_addons();

	get_tree()->quit(p_exit_code);
}

void EditorNode::unload_editor_addons() {
	for (const KeyValue<String, EditorPlugin *> &E : addon_name_to_plugin) {
		print_verbose(vformat("Unloading addon: %s", E.key));
		remove_editor_plugin(E.value, false);
		memdelete(E.value);
	}

	addon_name_to_plugin.clear();
}

void EditorNode::_discard_changes(const String &p_str) {
	switch (current_menu_option) {
		case FILE_CLOSE:
		case SCENE_TAB_CLOSE: {
			Node *scene = editor_data.get_edited_scene_root(tab_closing_idx);
			if (scene != nullptr) {
				String scene_filename = scene->get_scene_file_path();
				if (!scene_filename.is_empty()) {
					previous_scenes.push_back(scene_filename);
				}
			}

			// Don't close tabs when exiting the editor (required for "restore_scenes_on_load" setting).
			if (!_is_closing_editor()) {
				_remove_scene(tab_closing_idx);
				scene_tabs->update_scene_tabs();
			}
			_proceed_closing_scene_tabs();
		} break;
		case FILE_QUIT: {
			project_run_bar->stop_playing();
			_exit_editor(EXIT_SUCCESS);

		} break;
		case RUN_PROJECT_MANAGER: {
			restart_editor(true);
		} break;
		case RELOAD_CURRENT_PROJECT: {
			restart_editor();
		} break;
	}
}

void EditorNode::_update_file_menu_opened() {
	file_menu->set_item_disabled(file_menu->get_item_index(FILE_OPEN_PREV), previous_scenes.is_empty());
	_update_undo_redo_allowed();
}

void EditorNode::_update_file_menu_closed() {
	file_menu->set_item_disabled(file_menu->get_item_index(FILE_OPEN_PREV), false);
}

void EditorNode::_palette_quick_open_dialog() {
	quick_open_color_palette->popup_dialog({ "ColorPalette" }, palette_file_selected_callback);
	quick_open_color_palette->set_title(TTR("Quick Open Color Palette..."));
}

void EditorNode::replace_resources_in_object(Object *p_object, const Vector<Ref<Resource>> &p_source_resources, const Vector<Ref<Resource>> &p_target_resource) {
	List<PropertyInfo> pi;
	p_object->get_property_list(&pi);

	for (const PropertyInfo &E : pi) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		switch (E.type) {
			case Variant::OBJECT: {
				if (E.hint == PROPERTY_HINT_RESOURCE_TYPE) {
					const Variant &v = p_object->get(E.name);
					Ref<Resource> res = v;

					if (res.is_valid()) {
						int res_idx = p_source_resources.find(res);
						if (res_idx != -1) {
							p_object->set(E.name, p_target_resource.get(res_idx));
						} else {
							replace_resources_in_object(v, p_source_resources, p_target_resource);
						}
					}
				}
			} break;
			case Variant::ARRAY: {
				Array varray = p_object->get(E.name);
				int len = varray.size();
				bool array_requires_updating = false;
				for (int i = 0; i < len; i++) {
					const Variant &v = varray.get(i);
					Ref<Resource> res = v;

					if (res.is_valid()) {
						int res_idx = p_source_resources.find(res);
						if (res_idx != -1) {
							varray.set(i, p_target_resource.get(res_idx));
							array_requires_updating = true;
						} else {
							replace_resources_in_object(v, p_source_resources, p_target_resource);
						}
					}
				}
				if (array_requires_updating) {
					p_object->set(E.name, varray);
				}
			} break;
			case Variant::DICTIONARY: {
				Dictionary d = p_object->get(E.name);
				List<Variant> keys;
				bool dictionary_requires_updating = false;
				d.get_key_list(&keys);
				for (const Variant &F : keys) {
					Variant v = d[F];
					Ref<Resource> res = v;

					if (res.is_valid()) {
						int res_idx = p_source_resources.find(res);
						if (res_idx != -1) {
							d[F] = p_target_resource.get(res_idx);
							dictionary_requires_updating = true;
						} else {
							replace_resources_in_object(v, p_source_resources, p_target_resource);
						}
					}
				}
				if (dictionary_requires_updating) {
					p_object->set(E.name, d);
				}
			} break;
			default: {
			}
		}
	}

	Node *n = Object::cast_to<Node>(p_object);
	if (n) {
		for (int i = 0; i < n->get_child_count(); i++) {
			replace_resources_in_object(n->get_child(i), p_source_resources, p_target_resource);
		}
	}
}

void EditorNode::replace_resources_in_scenes(const Vector<Ref<Resource>> &p_source_resources, const Vector<Ref<Resource>> &p_target_resource) {
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Node *edited_scene_root = editor_data.get_edited_scene_root(i);
		if (edited_scene_root) {
			replace_resources_in_object(edited_scene_root, p_source_resources, p_target_resource);
		}
	}
}

void EditorNode::add_editor_plugin(EditorPlugin *p_editor, bool p_config_changed) {
	if (p_editor->has_main_screen()) {
		singleton->editor_main_screen->add_main_plugin(p_editor);
	}
	singleton->editor_data.add_editor_plugin(p_editor);
	singleton->add_child(p_editor);
	if (p_config_changed) {
		p_editor->enable_plugin();
	}
}

void EditorNode::remove_editor_plugin(EditorPlugin *p_editor, bool p_config_changed) {
	if (p_editor->has_main_screen()) {
		singleton->editor_main_screen->remove_main_plugin(p_editor);
	}
	p_editor->make_visible(false);
	p_editor->clear();
	if (p_config_changed) {
		p_editor->disable_plugin();
	}
	singleton->editor_plugins_over->remove_plugin(p_editor);
	singleton->editor_plugins_force_over->remove_plugin(p_editor);
	singleton->editor_plugins_force_input_forwarding->remove_plugin(p_editor);
	singleton->remove_child(p_editor);
	singleton->editor_data.remove_editor_plugin(p_editor);

	for (KeyValue<ObjectID, HashSet<EditorPlugin *>> &kv : singleton->active_plugins) {
		kv.value.erase(p_editor);
	}
}

void EditorNode::add_extension_editor_plugin(const StringName &p_class_name) {
	ERR_FAIL_COND_MSG(!ClassDB::class_exists(p_class_name), vformat("No such editor plugin registered: %s", p_class_name));
	ERR_FAIL_COND_MSG(!ClassDB::is_parent_class(p_class_name, SNAME("EditorPlugin")), vformat("Class is not an editor plugin: %s", p_class_name));
	ERR_FAIL_COND_MSG(singleton->editor_data.has_extension_editor_plugin(p_class_name), vformat("Editor plugin already added for class: %s", p_class_name));

	EditorPlugin *plugin = Object::cast_to<EditorPlugin>(ClassDB::instantiate(p_class_name));
	singleton->editor_data.add_extension_editor_plugin(p_class_name, plugin);
	add_editor_plugin(plugin);
}

void EditorNode::remove_extension_editor_plugin(const StringName &p_class_name) {
	// If we're exiting, the editor plugins will get cleaned up anyway, so don't do anything.
	if (!singleton || singleton->exiting) {
		return;
	}

	ERR_FAIL_COND_MSG(!singleton->editor_data.has_extension_editor_plugin(p_class_name), vformat("No editor plugin added for class: %s", p_class_name));

	EditorPlugin *plugin = singleton->editor_data.get_extension_editor_plugin(p_class_name);
	remove_editor_plugin(plugin);
	memdelete(plugin);
	singleton->editor_data.remove_extension_editor_plugin(p_class_name);
}

void EditorNode::_update_addon_config() {
	if (_initializing_plugins) {
		return;
	}

	Vector<String> enabled_addons;

	for (const KeyValue<String, EditorPlugin *> &E : addon_name_to_plugin) {
		enabled_addons.push_back(E.key);
	}

	if (enabled_addons.size() == 0) {
		ProjectSettings::get_singleton()->set("editor_plugins/enabled", Variant());
	} else {
		enabled_addons.sort();
		ProjectSettings::get_singleton()->set("editor_plugins/enabled", enabled_addons);
	}

	project_settings_editor->queue_save();
}

void EditorNode::set_addon_plugin_enabled(const String &p_addon, bool p_enabled, bool p_config_changed) {
	String addon_path = p_addon;

	if (!addon_path.begins_with("res://")) {
		addon_path = "res://addons/" + addon_path + "/plugin.cfg";
	}

	ERR_FAIL_COND(p_enabled && addon_name_to_plugin.has(addon_path));
	ERR_FAIL_COND(!p_enabled && !addon_name_to_plugin.has(addon_path));

	if (!p_enabled) {
		EditorPlugin *addon = addon_name_to_plugin[addon_path];
		remove_editor_plugin(addon, p_config_changed);
		memdelete(addon);
		addon_name_to_plugin.erase(addon_path);
		_update_addon_config();
		return;
	}

	Ref<ConfigFile> cf;
	cf.instantiate();
	if (!DirAccess::exists(addon_path.get_base_dir())) {
		_remove_plugin_from_enabled(addon_path);
		WARN_PRINT("Addon '" + addon_path + "' failed to load. No directory found. Removing from enabled plugins.");
		return;
	}
	Error err = cf->load(addon_path);
	if (err != OK) {
		show_warning(vformat(TTR("Unable to enable addon plugin at: '%s' parsing of config failed."), addon_path));
		return;
	}

	String plugin_version;
	if (cf->has_section_key("plugin", "version")) {
		plugin_version = cf->get_value("plugin", "version");
	}

	if (!cf->has_section_key("plugin", "script")) {
		show_warning(vformat(TTR("Unable to find script field for addon plugin at: '%s'."), addon_path));
		return;
	}

	String script_path = cf->get_value("plugin", "script");
	Ref<Script> scr; // We need to save it for creating "ep" below.

	// Only try to load the script if it has a name. Else, the plugin has no init script.
	if (script_path.length() > 0) {
		script_path = addon_path.get_base_dir().path_join(script_path);
		scr = ResourceLoader::load(script_path, "Script", ResourceFormatLoader::CACHE_MODE_IGNORE);

		if (scr.is_null()) {
			show_warning(vformat(TTR("Unable to load addon script from path: '%s'."), script_path));
			return;
		}

		// Errors in the script cause the base_type to be an empty StringName.
		if (scr->get_instance_base_type() == StringName()) {
			if (_initializing_plugins) {
				// However, if it happens during initialization, waiting for file scan might help.
				pending_addons.push_back(p_addon);
				return;
			}

			show_warning(vformat(TTR("Unable to load addon script from path: '%s'. This might be due to a code error in that script.\nDisabling the addon at '%s' to prevent further errors."), script_path, addon_path));
			_remove_plugin_from_enabled(addon_path);
			return;
		}

		// Plugin init scripts must inherit from EditorPlugin and be tools.
		if (!ClassDB::is_parent_class(scr->get_instance_base_type(), "EditorPlugin")) {
			show_warning(vformat(TTR("Unable to load addon script from path: '%s'. Base type is not 'EditorPlugin'."), script_path));
			return;
		}

		if (!scr->is_tool()) {
			show_warning(vformat(TTR("Unable to load addon script from path: '%s'. Script is not in tool mode."), script_path));
			return;
		}
	}

	EditorPlugin *ep = memnew(EditorPlugin);
	ep->set_script(scr);
	ep->set_plugin_version(plugin_version);
	addon_name_to_plugin[addon_path] = ep;
	add_editor_plugin(ep, p_config_changed);

	_update_addon_config();
}

bool EditorNode::is_addon_plugin_enabled(const String &p_addon) const {
	if (p_addon.begins_with("res://")) {
		return addon_name_to_plugin.has(p_addon);
	}

	return addon_name_to_plugin.has("res://addons/" + p_addon + "/plugin.cfg");
}

void EditorNode::_remove_edited_scene(bool p_change_tab) {
	// When scene gets closed no node is edited anymore, so make sure the editors are notified before nodes are freed.
	hide_unused_editors(SceneTreeDock::get_singleton());

	int new_index = editor_data.get_edited_scene();
	int old_index = new_index;

	if (new_index > 0) {
		new_index = new_index - 1;
	} else if (editor_data.get_edited_scene_count() > 1) {
		new_index = 1;
	} else {
		editor_data.add_edited_scene(-1);
		new_index = 1;
	}

	if (p_change_tab) {
		_set_current_scene(new_index);
	}
	editor_data.remove_scene(old_index);
	_update_title();
	scene_tabs->update_scene_tabs();
}

void EditorNode::_remove_scene(int index, bool p_change_tab) {
	// Clear icon cache in case some scripts are no longer needed.
	// FIXME: Ideally the cache should never be cleared and only updated on per-script basis, when an icon changes.
	editor_data.clear_script_icon_cache();

	if (editor_data.get_edited_scene() == index) {
		// Scene to remove is current scene.
		_remove_edited_scene(p_change_tab);
	} else {
		// Scene to remove is not active scene.
		editor_data.remove_scene(index);
	}
}

void EditorNode::set_edited_scene(Node *p_scene) {
	set_edited_scene_root(p_scene, true);
}

void EditorNode::set_edited_scene_root(Node *p_scene, bool p_auto_add) {
	Node *old_edited_scene_root = get_editor_data().get_edited_scene_root();
	ERR_FAIL_COND_MSG(p_scene && p_scene != old_edited_scene_root && p_scene->get_parent(), "Non-null nodes that are set as edited scene should not have a parent node.");

	if (p_auto_add && old_edited_scene_root && old_edited_scene_root->get_parent() == scene_root) {
		scene_root->remove_child(old_edited_scene_root);
	}
	get_editor_data().set_edited_scene_root(p_scene);

	if (Object::cast_to<Popup>(p_scene)) {
		Object::cast_to<Popup>(p_scene)->show();
	}
	SceneTreeDock::get_singleton()->set_edited_scene(p_scene);
	if (get_tree()) {
		get_tree()->set_edited_scene_root(p_scene);
	}

	if (p_auto_add && p_scene) {
		scene_root->add_child(p_scene, true);
	}
}

Dictionary EditorNode::_get_main_scene_state() {
	Dictionary state;
	state["scene_tree_offset"] = SceneTreeDock::get_singleton()->get_tree_editor()->get_scene_tree()->get_vscroll_bar()->get_value();
	state["property_edit_offset"] = InspectorDock::get_inspector_singleton()->get_scroll_offset();
	state["node_filter"] = SceneTreeDock::get_singleton()->get_filter();
	return state;
}

void EditorNode::_set_main_scene_state(Dictionary p_state, Node *p_for_scene) {
	if (get_edited_scene() != p_for_scene && p_for_scene != nullptr) {
		return; // Not for this scene.
	}

	changing_scene = false;

	if (get_edited_scene()) {
		int current_tab = editor_main_screen->get_selected_index();
		if (current_tab < 2) {
			// Switch between 2D and 3D if currently in 2D or 3D.
			Node *selected_node = SceneTreeDock::get_singleton()->get_tree_editor()->get_selected();
			if (!selected_node) {
				selected_node = get_edited_scene();
			}

			if (Object::cast_to<CanvasItem>(selected_node)) {
				editor_main_screen->select(EditorMainScreen::EDITOR_2D);
			} else if (Object::cast_to<Node3D>(selected_node)) {
				editor_main_screen->select(EditorMainScreen::EDITOR_3D);
			}
		}
	}

	if (p_state.has("scene_tree_offset")) {
		SceneTreeDock::get_singleton()->get_tree_editor()->get_scene_tree()->get_vscroll_bar()->set_value(p_state["scene_tree_offset"]);
	}
	if (p_state.has("property_edit_offset")) {
		InspectorDock::get_inspector_singleton()->set_scroll_offset(p_state["property_edit_offset"]);
	}

	if (p_state.has("node_filter")) {
		SceneTreeDock::get_singleton()->set_filter(p_state["node_filter"]);
	}

	// This should only happen at the very end.

	EditorDebuggerNode::get_singleton()->update_live_edit_root();
	ScriptEditor::get_singleton()->set_scene_root_script(editor_data.get_scene_root_script(editor_data.get_edited_scene()));
	editor_data.notify_edited_scene_changed();
	emit_signal(SNAME("scene_changed"));

	// Reset SDFGI after everything else so that any last-second scene modifications will be processed.
	RenderingServer::get_singleton()->sdfgi_reset();
}

bool EditorNode::is_changing_scene() const {
	return changing_scene;
}

void EditorNode::_set_current_scene(int p_idx) {
	if (p_idx == editor_data.get_edited_scene()) {
		return; // Pointless.
	}

	_set_current_scene_nocheck(p_idx);
}

void EditorNode::_set_current_scene_nocheck(int p_idx) {
	// Save the folding in case the scene gets reloaded.
	if (editor_data.get_scene_path(p_idx) != "" && editor_data.get_edited_scene_root(p_idx)) {
		editor_folding.save_scene_folding(editor_data.get_edited_scene_root(p_idx), editor_data.get_scene_path(p_idx));
	}

	changing_scene = true;
	editor_data.save_edited_scene_state(editor_selection, &editor_history, _get_main_scene_state());

	Node *old_scene = get_editor_data().get_edited_scene_root();

	editor_selection->clear();
	editor_data.set_edited_scene(p_idx);

	Node *new_scene = editor_data.get_edited_scene_root();

	// Remove the scene only if it's a new scene, preventing performance issues when adding and removing scenes.
	if (old_scene && new_scene != old_scene && old_scene->get_parent() == scene_root) {
		scene_root->remove_child(old_scene);
	}

	if (Popup *p = Object::cast_to<Popup>(new_scene)) {
		p->show();
	}

	SceneTreeDock::get_singleton()->set_edited_scene(new_scene);
	if (get_tree()) {
		get_tree()->set_edited_scene_root(new_scene);
	}

	if (new_scene) {
		if (new_scene->get_parent() != scene_root) {
			scene_root->add_child(new_scene, true);
		}
	}

	if (editor_data.check_and_update_scene(p_idx)) {
		if (!editor_data.get_scene_path(p_idx).is_empty()) {
			editor_folding.load_scene_folding(editor_data.get_edited_scene_root(p_idx), editor_data.get_scene_path(p_idx));
		}

		EditorUndoRedoManager::get_singleton()->clear_history(editor_data.get_scene_history_id(p_idx), false);
	}

	Dictionary state = editor_data.restore_edited_scene_state(editor_selection, &editor_history);
	_edit_current(true);

	_update_title();
	callable_mp(scene_tabs, &EditorSceneTabs::update_scene_tabs).call_deferred();

	if (tabs_to_close.is_empty()) {
		callable_mp(this, &EditorNode::_set_main_scene_state).call_deferred(state, get_edited_scene()); // Do after everything else is done setting up.
	}

	_update_undo_redo_allowed();
}

void EditorNode::setup_color_picker(ColorPicker *p_picker) {
	p_picker->set_editor_settings(EditorSettings::get_singleton());
	int default_color_mode = EditorSettings::get_singleton()->get_project_metadata("color_picker", "color_mode", EDITOR_GET("interface/inspector/default_color_picker_mode"));
	int picker_shape = EditorSettings::get_singleton()->get_project_metadata("color_picker", "picker_shape", EDITOR_GET("interface/inspector/default_color_picker_shape"));

	p_picker->set_color_mode((ColorPicker::ColorModeType)default_color_mode);
	p_picker->set_picker_shape((ColorPicker::PickerShapeType)picker_shape);

	p_picker->set_quick_open_callback(callable_mp(this, &EditorNode::_palette_quick_open_dialog));
	p_picker->set_palette_saved_callback(callable_mp(EditorFileSystem::get_singleton(), &EditorFileSystem::update_file));
	palette_file_selected_callback = callable_mp(p_picker, &ColorPicker::_quick_open_palette_file_selected);
}

bool EditorNode::is_scene_open(const String &p_path) {
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		if (editor_data.get_scene_path(i) == p_path) {
			return true;
		}
	}

	return false;
}

bool EditorNode::is_multi_window_enabled() const {
	return !SceneTree::get_singleton()->get_root()->is_embedding_subwindows() && !EDITOR_GET("interface/editor/single_window_mode") && EDITOR_GET("interface/multi_window/enable");
}

void EditorNode::fix_dependencies(const String &p_for_file) {
	dependency_fixer->edit(p_for_file);
}

int EditorNode::new_scene() {
	int idx = editor_data.add_edited_scene(-1);
	_set_current_scene(idx); // Before trying to remove an empty scene, set the current tab index to the newly added tab index.

	// Remove placeholder empty scene.
	if (editor_data.get_edited_scene_count() > 1) {
		for (int i = 0; i < editor_data.get_edited_scene_count() - 1; i++) {
			bool unsaved = EditorUndoRedoManager::get_singleton()->is_history_unsaved(editor_data.get_scene_history_id(i));
			if (!unsaved && editor_data.get_scene_path(i).is_empty() && editor_data.get_edited_scene_root(i) == nullptr) {
				editor_data.remove_scene(i);
				idx--;
			}
		}
	}

	editor_data.clear_editor_states();
	scene_tabs->update_scene_tabs();
	return idx;
}

Error EditorNode::load_scene(const String &p_scene, bool p_ignore_broken_deps, bool p_set_inherited, bool p_force_open_imported, bool p_silent_change_tab) {
	if (!is_inside_tree()) {
		defer_load_scene = p_scene;
		return OK;
	}

	String lpath = ResourceUID::ensure_path(p_scene);
	if (!p_set_inherited) {
		for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
			if (editor_data.get_scene_path(i) == lpath) {
				_set_current_scene(i);
				return OK;
			}
		}

		if (!p_force_open_imported && FileAccess::exists(lpath + ".import")) {
			open_imported->set_text(vformat(TTR("Scene '%s' was automatically imported, so it can't be modified.\nTo make changes to it, a new inherited scene can be created."), lpath.get_file()));
			open_imported->popup_centered();
			new_inherited_button->grab_focus();
			open_import_request = lpath;
			return OK;
		}
	}

	lpath = ProjectSettings::get_singleton()->localize_path(lpath);
	if (!lpath.begins_with("res://")) {
		show_accept(TTR("Error loading scene, it must be inside the project path. Use 'Import' to open the scene, then save it inside the project path."), TTR("OK"));
		opening_prev = false;
		return ERR_FILE_NOT_FOUND;
	}

	int prev = editor_data.get_edited_scene();
	int idx = prev;

	if (prev == -1 || editor_data.get_edited_scene_root() || !editor_data.get_scene_path(prev).is_empty()) {
		idx = editor_data.add_edited_scene(-1);

		if (p_silent_change_tab) {
			_set_current_scene_nocheck(idx);
		} else {
			_set_current_scene(idx);
		}
	} else {
		EditorUndoRedoManager::get_singleton()->clear_history(editor_data.get_current_edited_scene_history_id(), false);
	}

	dependency_errors.clear();

	Error err;
	Ref<PackedScene> sdata = ResourceLoader::load(lpath, "", ResourceFormatLoader::CACHE_MODE_REPLACE, &err);

	if (!p_ignore_broken_deps && dependency_errors.has(lpath)) {
		current_menu_option = -1;
		Vector<String> errors;
		for (const String &E : dependency_errors[lpath]) {
			errors.push_back(E);
		}
		dependency_error->show(DependencyErrorDialog::MODE_SCENE, lpath, errors);
		opening_prev = false;

		if (prev != -1 && prev != idx) {
			_set_current_scene(prev);
			editor_data.remove_scene(idx);
		}
		return ERR_FILE_MISSING_DEPENDENCIES;
	}

	if (sdata.is_null()) {
		_dialog_display_load_error(lpath, err);
		opening_prev = false;

		if (prev != -1 && prev != idx) {
			_set_current_scene(prev);
			editor_data.remove_scene(idx);
		}
		return ERR_FILE_NOT_FOUND;
	}

	dependency_errors.erase(lpath); // At least not self path.

	for (KeyValue<String, HashSet<String>> &E : dependency_errors) {
		String txt = vformat(TTR("Scene '%s' has broken dependencies:"), E.key) + "\n";
		for (const String &F : E.value) {
			txt += "\t" + F + "\n";
		}
		add_io_error(txt);
	}

	if (ResourceCache::has(lpath)) {
		// Used from somewhere else? No problem! Update state and replace sdata.
		Ref<PackedScene> ps = ResourceCache::get_ref(lpath);
		if (ps.is_valid()) {
			ps->replace_state(sdata->get_state());
			ps->set_last_modified_time(sdata->get_last_modified_time());
			sdata = ps;
		}

	} else {
		sdata->set_path(lpath, true); // Take over path.
	}

	Node *new_scene = sdata->instantiate(p_set_inherited ? PackedScene::GEN_EDIT_STATE_MAIN_INHERITED : PackedScene::GEN_EDIT_STATE_MAIN);

	if (!new_scene) {
		sdata.unref();
		_dialog_display_load_error(lpath, ERR_FILE_CORRUPT);
		opening_prev = false;
		if (prev != -1 && prev != idx) {
			_set_current_scene(prev);
			editor_data.remove_scene(idx);
		}
		return ERR_FILE_CORRUPT;
	}

	if (p_set_inherited) {
		Ref<SceneState> state = sdata->get_state();
		state->set_path(lpath);
		new_scene->set_scene_inherited_state(state);
		new_scene->set_scene_file_path(String());
	}

	new_scene->set_scene_instance_state(Ref<SceneState>());

	set_edited_scene(new_scene);

	String config_file_path = EditorPaths::get_singleton()->get_project_settings_dir().path_join(lpath.get_file() + "-editstate-" + lpath.md5_text() + ".cfg");
	Ref<ConfigFile> editor_state_cf;
	editor_state_cf.instantiate();
	Error editor_state_cf_err = editor_state_cf->load(config_file_path);
	if (editor_state_cf_err == OK || editor_state_cf->has_section("editor_states")) {
		_load_editor_plugin_states_from_config(editor_state_cf);
	}

	if (editor_folding.has_folding_data(lpath)) {
		editor_folding.load_scene_folding(new_scene, lpath);
	} else if (EDITOR_GET("interface/inspector/auto_unfold_foreign_scenes")) {
		editor_folding.unfold_scene(new_scene);
		editor_folding.save_scene_folding(new_scene, lpath);
	}

	opening_prev = false;

	EditorDebuggerNode::get_singleton()->update_live_edit_root();

	if (restoring_scenes) {
		// Initialize history for restored scenes.
		ObjectID id = new_scene->get_instance_id();
		if (id != editor_history.get_current()) {
			editor_history.add_object(id);
		}
	}

	// Load the selected nodes.
	if (editor_state_cf->has_section_key("editor_states", "selected_nodes")) {
		TypedArray<NodePath> selected_node_list = editor_state_cf->get_value("editor_states", "selected_nodes", TypedArray<String>());

		for (int i = 0; i < selected_node_list.size(); i++) {
			Node *selected_node = new_scene->get_node_or_null(selected_node_list[i]);
			if (selected_node) {
				editor_selection->add_node(selected_node);
			}
		}
	}

	if (!restoring_scenes) {
		save_editor_layout_delayed();
	}

	if (p_set_inherited) {
		EditorUndoRedoManager::get_singleton()->set_history_as_unsaved(editor_data.get_current_edited_scene_history_id());
	}

	_update_title();
	scene_tabs->update_scene_tabs();
	if (!restoring_scenes) {
		_add_to_recent_scenes(lpath);
	}

	return OK;
}

HashMap<StringName, Variant> EditorNode::get_modified_properties_for_node(Node *p_node, bool p_node_references_only) {
	HashMap<StringName, Variant> modified_property_map;

	List<PropertyInfo> pinfo;
	p_node->get_property_list(&pinfo);
	for (const PropertyInfo &E : pinfo) {
		if (E.usage & PROPERTY_USAGE_STORAGE) {
			bool node_reference = (E.type == Variant::OBJECT && E.hint == PROPERTY_HINT_NODE_TYPE);
			if (p_node_references_only && !node_reference) {
				continue;
			}
			bool is_valid_revert = false;
			Variant revert_value = EditorPropertyRevert::get_property_revert_value(p_node, E.name, &is_valid_revert);
			Variant current_value = p_node->get(E.name);
			if (is_valid_revert) {
				if (PropertyUtils::is_property_value_different(p_node, current_value, revert_value)) {
					// If this property is a direct node reference, save a NodePath instead to prevent corrupted references.
					if (node_reference) {
						Node *target_node = Object::cast_to<Node>(current_value);
						if (target_node) {
							modified_property_map[E.name] = p_node->get_path_to(target_node);
						}
					} else {
						modified_property_map[E.name] = current_value;
					}
				}
			}
		}
	}

	return modified_property_map;
}

HashMap<StringName, Variant> EditorNode::get_modified_properties_reference_to_nodes(Node *p_node, List<Node *> &p_nodes_referenced_by) {
	HashMap<StringName, Variant> modified_property_map;

	List<PropertyInfo> pinfo;
	p_node->get_property_list(&pinfo);
	for (const PropertyInfo &E : pinfo) {
		if (E.usage & PROPERTY_USAGE_STORAGE) {
			if (E.type != Variant::OBJECT || E.hint != PROPERTY_HINT_NODE_TYPE) {
				continue;
			}
			Variant current_value = p_node->get(E.name);
			Node *target_node = Object::cast_to<Node>(current_value);
			if (target_node && p_nodes_referenced_by.find(target_node)) {
				modified_property_map[E.name] = p_node->get_path_to(target_node);
			}
		}
	}

	return modified_property_map;
}

void EditorNode::update_node_from_node_modification_entry(Node *p_node, ModificationNodeEntry &p_node_modification) {
	if (p_node) {
		// First, attempt to restore the script property since it may affect the get_property_list method.
		Variant *script_property_table_entry = p_node_modification.property_table.getptr(CoreStringName(script));
		if (script_property_table_entry) {
			p_node->set_script(*script_property_table_entry);
		}

		// Get properties for this node.
		List<PropertyInfo> pinfo;
		p_node->get_property_list(&pinfo);

		// Get names of all valid property names.
		HashMap<StringName, bool> property_node_reference_table;
		for (const PropertyInfo &E : pinfo) {
			if (E.usage & PROPERTY_USAGE_STORAGE) {
				if (E.type == Variant::OBJECT && E.hint == PROPERTY_HINT_NODE_TYPE) {
					property_node_reference_table[E.name] = true;
				} else {
					property_node_reference_table[E.name] = false;
				}
			}
		}

		// Restore the modified properties for this node.
		for (const KeyValue<StringName, Variant> &E : p_node_modification.property_table) {
			bool *property_node_reference_table_entry = property_node_reference_table.getptr(E.key);
			if (property_node_reference_table_entry) {
				// If the property is a node reference, attempt to restore from the node path instead.
				bool is_node_reference = *property_node_reference_table_entry;
				if (is_node_reference) {
					if (E.value.get_type() == Variant::NODE_PATH) {
						p_node->set(E.key, p_node->get_node_or_null(E.value));
					}
				} else {
					p_node->set(E.key, E.value);
				}
			}
		}

		// Restore the connections to other nodes.
		for (const ConnectionWithNodePath &E : p_node_modification.connections_to) {
			Connection conn = E.connection;

			// Get the node the callable is targeting.
			Node *target_node = Object::cast_to<Node>(conn.callable.get_object());

			// If the callable object no longer exists or is marked for deletion,
			// attempt to reaccquire the closest match by using the node path
			// we saved earlier.
			if (!target_node || !target_node->is_queued_for_deletion()) {
				target_node = p_node->get_node_or_null(E.node_path);
			}

			if (target_node) {
				// Reconstruct the callable.
				Callable new_callable = Callable(target_node, conn.callable.get_method());

				if (!p_node->is_connected(conn.signal.get_name(), new_callable)) {
					ERR_FAIL_COND(p_node->connect(conn.signal.get_name(), new_callable, conn.flags) != OK);
				}
			}
		}

		// Restore the connections from other nodes.
		for (const Connection &E : p_node_modification.connections_from) {
			Connection conn = E;

			bool valid = p_node->has_method(conn.callable.get_method()) || Ref<Script>(p_node->get_script()).is_null() || Ref<Script>(p_node->get_script())->has_method(conn.callable.get_method());
			ERR_CONTINUE_MSG(!valid, vformat("Attempt to connect signal '%s.%s' to nonexistent method '%s.%s'.", conn.signal.get_object()->get_class(), conn.signal.get_name(), conn.callable.get_object()->get_class(), conn.callable.get_method()));

			// Get the object which the signal is connected from.
			Object *source_object = conn.signal.get_object();

			if (source_object) {
				ERR_FAIL_COND(source_object->connect(conn.signal.get_name(), Callable(p_node, conn.callable.get_method()), conn.flags) != OK);
			}
		}

		// Re-add the groups.
		for (const Node::GroupInfo &E : p_node_modification.groups) {
			p_node->add_to_group(E.name, E.persistent);
		}
	}
}

bool EditorNode::is_additional_node_in_scene(Node *p_edited_scene, Node *p_reimported_root, Node *p_node) {
	if (p_node == p_reimported_root) {
		return false;
	}

	bool node_part_of_subscene = p_node != p_edited_scene &&
			p_edited_scene->get_scene_inherited_state().is_valid() &&
			p_edited_scene->get_scene_inherited_state()->find_node_by_path(p_edited_scene->get_path_to(p_node)) >= 0 &&
			// It's important to process added nodes from the base scene in the inherited scene as
			// additional nodes to ensure they do not disappear on reload.
			// When p_reimported_root == p_edited_scene that means the edited scene
			// is the reimported scene, in that case the node is in the root base scene,
			// so it's not an addition, otherwise, the node would be added twice on reload.
			(p_node->get_owner() != p_edited_scene || p_reimported_root == p_edited_scene);

	if (node_part_of_subscene) {
		return false;
	}

	// Loop through the owners until either we reach the root node or nullptr
	Node *valid_node_owner = p_node->get_owner();
	while (valid_node_owner) {
		if (valid_node_owner == p_reimported_root) {
			break;
		}
		valid_node_owner = valid_node_owner->get_owner();
	}

	// When the owner is the imported scene and the owner is also the edited scene,
	// that means the node was added in the current edited scene.
	// We can be sure here because if the node that the node does not come from
	// the base scene because we checked just over with 'get_scene_inherited_state()->find_node_by_path'.
	if (valid_node_owner == p_reimported_root && p_reimported_root != p_edited_scene) {
		return false;
	}

	return true;
}

void EditorNode::get_scene_editor_data_for_node(Node *p_root, Node *p_node, HashMap<NodePath, SceneEditorDataEntry> &p_table) {
	SceneEditorDataEntry new_entry;
	new_entry.is_display_folded = p_node->is_displayed_folded();

	if (p_root != p_node) {
		new_entry.is_editable = p_root->is_editable_instance(p_node);
	}

	p_table.insert(p_root->get_path_to(p_node), new_entry);

	for (int i = 0; i < p_node->get_child_count(); i++) {
		get_scene_editor_data_for_node(p_root, p_node->get_child(i), p_table);
	}
}

void EditorNode::get_preload_scene_modification_table(
		Node *p_edited_scene,
		Node *p_reimported_root,
		Node *p_node, InstanceModificationsEntry &p_instance_modifications) {
	if (is_additional_node_in_scene(p_edited_scene, p_reimported_root, p_node)) {
		// Only save additional nodes which have an owner since this was causing issues transient ownerless nodes
		// which get recreated upon scene tree entry.
		// For now instead, assume all ownerless nodes are transient and will have to be recreated.
		if (p_node->get_owner()) {
			HashMap<StringName, Variant> modified_properties = get_modified_properties_for_node(p_node, true);
			if (p_node->get_owner() == p_edited_scene) {
				AdditiveNodeEntry new_additive_node_entry;
				new_additive_node_entry.node = p_node;
				new_additive_node_entry.parent = p_reimported_root->get_path_to(p_node->get_parent());
				new_additive_node_entry.owner = p_node->get_owner();
				new_additive_node_entry.index = p_node->get_index();

				Node2D *node_2d = Object::cast_to<Node2D>(p_node);
				if (node_2d) {
					new_additive_node_entry.transform_2d = node_2d->get_transform();
				}
				Node3D *node_3d = Object::cast_to<Node3D>(p_node);
				if (node_3d) {
					new_additive_node_entry.transform_3d = node_3d->get_transform();
				}

				p_instance_modifications.addition_list.push_back(new_additive_node_entry);
			}
			if (!modified_properties.is_empty()) {
				ModificationNodeEntry modification_node_entry;
				modification_node_entry.property_table = modified_properties;

				p_instance_modifications.modifications[p_reimported_root->get_path_to(p_node)] = modification_node_entry;
			}
		}
	} else {
		HashMap<StringName, Variant> modified_properties = get_modified_properties_for_node(p_node, false);

		// Find all valid connections to other nodes.
		List<Connection> connections_to;
		p_node->get_all_signal_connections(&connections_to);

		List<ConnectionWithNodePath> valid_connections_to;
		for (const Connection &c : connections_to) {
			Node *connection_target_node = Object::cast_to<Node>(c.callable.get_object());
			if (connection_target_node) {
				// TODO: add support for reinstating custom callables
				if (!c.callable.is_custom()) {
					ConnectionWithNodePath connection_to;
					connection_to.connection = c;
					connection_to.node_path = p_node->get_path_to(connection_target_node);
					valid_connections_to.push_back(connection_to);
				}
			}
		}

		// Find all valid connections from other nodes.
		List<Connection> connections_from;
		p_node->get_signals_connected_to_this(&connections_from);

		List<Connection> valid_connections_from;
		for (const Connection &c : connections_from) {
			Node *source_node = Object::cast_to<Node>(c.signal.get_object());

			Node *valid_source_owner = nullptr;
			if (source_node) {
				valid_source_owner = source_node->get_owner();
				while (valid_source_owner) {
					if (valid_source_owner == p_reimported_root) {
						break;
					}
					valid_source_owner = valid_source_owner->get_owner();
				}
			}

			if (!source_node || valid_source_owner == nullptr) {
				// TODO: add support for reinstating custom callables
				if (!c.callable.is_custom()) {
					valid_connections_from.push_back(c);
				}
			}
		}

		// Find all node groups.
		List<Node::GroupInfo> groups;
		p_node->get_groups(&groups);

		if (!modified_properties.is_empty() || !valid_connections_to.is_empty() || !valid_connections_from.is_empty() || !groups.is_empty()) {
			ModificationNodeEntry modification_node_entry;
			modification_node_entry.property_table = modified_properties;
			modification_node_entry.connections_to = valid_connections_to;
			modification_node_entry.connections_from = valid_connections_from;
			modification_node_entry.groups = groups;

			p_instance_modifications.modifications[p_reimported_root->get_path_to(p_node)] = modification_node_entry;
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		get_preload_scene_modification_table(p_edited_scene, p_reimported_root, p_node->get_child(i), p_instance_modifications);
	}
}

void EditorNode::get_preload_modifications_reference_to_nodes(
		Node *p_root,
		Node *p_node,
		HashSet<Node *> &p_excluded_nodes,
		List<Node *> &p_instance_list_with_children,
		HashMap<NodePath, ModificationNodeEntry> &p_modification_table) {
	if (!p_excluded_nodes.find(p_node)) {
		HashMap<StringName, Variant> modified_properties = get_modified_properties_reference_to_nodes(p_node, p_instance_list_with_children);

		if (!modified_properties.is_empty()) {
			ModificationNodeEntry modification_node_entry;
			modification_node_entry.property_table = modified_properties;

			p_modification_table[p_root->get_path_to(p_node)] = modification_node_entry;
		}

		for (int i = 0; i < p_node->get_child_count(); i++) {
			get_preload_modifications_reference_to_nodes(p_root, p_node->get_child(i), p_excluded_nodes, p_instance_list_with_children, p_modification_table);
		}
	}
}

void EditorNode::get_children_nodes(Node *p_node, List<Node *> &p_nodes) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		p_nodes.push_back(child);
		get_children_nodes(child, p_nodes);
	}
}

void EditorNode::replace_history_reimported_nodes(Node *p_original_root_node, Node *p_new_root_node, Node *p_node) {
	NodePath scene_path_to_node = p_original_root_node->get_path_to(p_node);
	Node *new_node = p_new_root_node->get_node_or_null(scene_path_to_node);
	if (new_node) {
		editor_history.replace_object(p_node->get_instance_id(), new_node->get_instance_id());
	} else {
		editor_history.replace_object(p_node->get_instance_id(), ObjectID());
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		replace_history_reimported_nodes(p_original_root_node, p_new_root_node, p_node->get_child(i));
	}
}

void EditorNode::open_request(const String &p_path, bool p_set_inherited) {
	if (!opening_prev) {
		List<String>::Element *prev_scene_item = previous_scenes.find(p_path);
		if (prev_scene_item != nullptr) {
			prev_scene_item->erase();
		}
	}

	load_scene(p_path, false, p_set_inherited); // As it will be opened in separate tab.
}

bool EditorNode::has_previous_scenes() const {
	return !previous_scenes.is_empty();
}

void EditorNode::edit_foreign_resource(Ref<Resource> p_resource) {
	load_scene(p_resource->get_path().get_slice("::", 0));
	callable_mp(InspectorDock::get_singleton(), &InspectorDock::edit_resource).call_deferred(p_resource);
}

bool EditorNode::is_resource_read_only(Ref<Resource> p_resource, bool p_foreign_resources_are_writable) {
	ERR_FAIL_COND_V(p_resource.is_null(), false);

	String path = p_resource->get_path();
	if (!path.is_resource_file()) {
		// If the resource name contains '::', that means it is a subresource embedded in another resource.
		int srpos = path.find("::");
		if (srpos != -1) {
			String base = path.substr(0, srpos);
			// If the base resource is a packed scene, we treat it as read-only if it is not the currently edited scene.
			if (ResourceLoader::get_resource_type(base) == "PackedScene") {
				if (!get_tree()->get_edited_scene_root() || get_tree()->get_edited_scene_root()->get_scene_file_path() != base) {
					// If we have not flagged foreign resources as writable or the base scene the resource is
					// part was imported, it can be considered read-only.
					if (!p_foreign_resources_are_writable || FileAccess::exists(base + ".import")) {
						return true;
					}
				}
			} else {
				// If a corresponding .import file exists for the base file, we assume it to be imported and should therefore treated as read-only.
				if (FileAccess::exists(base + ".import")) {
					return true;
				}
			}
		}
	} else if (FileAccess::exists(path + ".import")) {
		// The resource is not a subresource, but if it has an .import file, it's imported so treat it as read only.
		return true;
	}

	return false;
}

void EditorNode::request_instantiate_scene(const String &p_path) {
	SceneTreeDock::get_singleton()->instantiate(p_path);
}

void EditorNode::request_instantiate_scenes(const Vector<String> &p_files) {
	SceneTreeDock::get_singleton()->instantiate_scenes(p_files);
}

String EditorNode::get_multiwindow_support_tooltip_text() const {
	if (SceneTree::get_singleton()->get_root()->is_embedding_subwindows()) {
		if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_SUBWINDOWS)) {
			return TTR("Multi-window support is not available because the `--single-window` command line argument was used to start the editor.");
		} else {
			return TTR("Multi-window support is not available because the current platform doesn't support multiple windows.");
		}
	} else if (EDITOR_GET("interface/editor/single_window_mode")) {
		return TTR("Multi-window support is not available because Interface > Editor > Single Window Mode is enabled in the editor settings.");
	}

	return TTR("Multi-window support is not available because Interface > Multi Window > Enable is disabled in the editor settings.");
}

void EditorNode::_inherit_request(String p_file) {
	current_menu_option = FILE_NEW_INHERITED_SCENE;
	_dialog_action(p_file);
}

void EditorNode::_instantiate_request(const Vector<String> &p_files) {
	request_instantiate_scenes(p_files);
}

void EditorNode::_close_messages() {
	old_split_ofs = center_split->get_split_offset();
	center_split->set_split_offset(0);
}

void EditorNode::_show_messages() {
	center_split->set_split_offset(old_split_ofs);
}

void EditorNode::_add_to_recent_scenes(const String &p_scene) {
	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scenes", Array());
	if (rc.has(p_scene)) {
		rc.erase(p_scene);
	}
	rc.push_front(p_scene);
	if (rc.size() > 10) {
		rc.resize(10);
	}

	EditorSettings::get_singleton()->set_project_metadata("recent_files", "scenes", rc);
	_update_recent_scenes();
}

void EditorNode::_open_recent_scene(int p_idx) {
	if (p_idx == recent_scenes->get_item_count() - 1) {
		EditorSettings::get_singleton()->set_project_metadata("recent_files", "scenes", Array());
		callable_mp(this, &EditorNode::_update_recent_scenes).call_deferred();
	} else {
		Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scenes", Array());
		ERR_FAIL_INDEX(p_idx, rc.size());

		if (load_scene(rc[p_idx]) != OK) {
			rc.remove_at(p_idx);
			EditorSettings::get_singleton()->set_project_metadata("recent_files", "scenes", rc);
			_update_recent_scenes();
		}
	}
}

void EditorNode::_update_recent_scenes() {
	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scenes", Array());
	recent_scenes->clear();

	String path;
	for (int i = 0; i < rc.size(); i++) {
		path = rc[i];
		recent_scenes->add_item(path.replace("res://", ""), i);
	}

	recent_scenes->add_separator();
	recent_scenes->add_shortcut(ED_SHORTCUT("editor/clear_recent", TTRC("Clear Recent Scenes")));
	recent_scenes->reset_size();
}

void EditorNode::_quick_opened(const String &p_file_path) {
	if (ClassDB::is_parent_class(ResourceLoader::get_resource_type(p_file_path), "PackedScene")) {
		open_request(p_file_path);
	} else {
		load_resource(p_file_path);
	}
}

void EditorNode::_project_run_started() {
	if (bool(EDITOR_GET("run/output/always_clear_output_on_play"))) {
		log->clear();
	}

	int action_on_play = EDITOR_GET("run/bottom_panel/action_on_play");
	if (action_on_play == ACTION_ON_PLAY_OPEN_OUTPUT) {
		bottom_panel->make_item_visible(log);
	} else if (action_on_play == ACTION_ON_PLAY_OPEN_DEBUGGER) {
		bottom_panel->make_item_visible(EditorDebuggerNode::get_singleton());
	}
}

void EditorNode::_project_run_stopped() {
	int action_on_stop = EDITOR_GET("run/bottom_panel/action_on_stop");
	if (action_on_stop == ACTION_ON_STOP_CLOSE_BUTTOM_PANEL) {
		bottom_panel->hide_bottom_panel();
	}
}

void EditorNode::notify_all_debug_sessions_exited() {
	project_run_bar->stop_playing();
}

void EditorNode::add_io_error(const String &p_error) {
	DEV_ASSERT(Thread::get_caller_id() == Thread::get_main_id());
	singleton->load_errors->add_image(singleton->theme->get_icon(SNAME("Error"), EditorStringName(EditorIcons)));
	singleton->load_errors->add_text(p_error + "\n");
	// When a progress dialog is displayed, we will wait for it ot close before displaying
	// the io errors to prevent the io popup to set it's parent to the progress dialog.
	if (singleton->progress_dialog->is_visible()) {
		singleton->load_errors_queued_to_display = true;
	} else {
		EditorInterface::get_singleton()->popup_dialog_centered_ratio(singleton->load_error_dialog, 0.5);
	}
}

void EditorNode::add_io_warning(const String &p_warning) {
	DEV_ASSERT(Thread::get_caller_id() == Thread::get_main_id());
	singleton->load_errors->add_image(singleton->theme->get_icon(SNAME("Warning"), EditorStringName(EditorIcons)));
	singleton->load_errors->add_text(p_warning + "\n");
	// When a progress dialog is displayed, we will wait for it ot close before displaying
	// the io errors to prevent the io popup to set it's parent to the progress dialog.
	if (singleton->progress_dialog->is_visible()) {
		singleton->load_errors_queued_to_display = true;
	} else {
		EditorInterface::get_singleton()->popup_dialog_centered_ratio(singleton->load_error_dialog, 0.5);
	}
}

bool EditorNode::_find_scene_in_use(Node *p_node, const String &p_path) const {
	if (p_node->get_scene_file_path() == p_path) {
		return true;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		if (_find_scene_in_use(p_node->get_child(i), p_path)) {
			return true;
		}
	}

	return false;
}

bool EditorNode::is_scene_in_use(const String &p_path) {
	Node *es = get_edited_scene();
	if (es) {
		return _find_scene_in_use(es, p_path);
	}
	return false;
}

OS::ProcessID EditorNode::has_child_process(OS::ProcessID p_pid) const {
	return project_run_bar->has_child_process(p_pid);
}

void EditorNode::stop_child_process(OS::ProcessID p_pid) {
	project_run_bar->stop_child_process(p_pid);
}

Ref<Script> EditorNode::get_object_custom_type_base(const Object *p_object) const {
	ERR_FAIL_NULL_V(p_object, nullptr);

	const Node *node = Object::cast_to<const Node>(p_object);
	if (node && node->has_meta(SceneStringName(_custom_type_script))) {
		return node->get_meta(SceneStringName(_custom_type_script));
	}

	Ref<Script> scr = p_object->get_script();

	if (scr.is_valid()) {
		// Uncommenting would break things! Consider adding a parameter if you need it.
		// StringName name = EditorNode::get_editor_data().script_class_get_name(base_script->get_path());
		// if (name != StringName()) {
		// 	return name;
		// }

		// TODO: Should probably be deprecated in 4.x
		StringName base = scr->get_instance_base_type();
		if (base != StringName() && EditorNode::get_editor_data().get_custom_types().has(base)) {
			const Vector<EditorData::CustomType> &types = EditorNode::get_editor_data().get_custom_types()[base];

			Ref<Script> base_scr = scr;
			while (base_scr.is_valid()) {
				for (int i = 0; i < types.size(); ++i) {
					if (types[i].script == base_scr) {
						return types[i].script;
					}
				}
				base_scr = base_scr->get_base_script();
			}
		}
	}

	return nullptr;
}

StringName EditorNode::get_object_custom_type_name(const Object *p_object) const {
	ERR_FAIL_NULL_V(p_object, StringName());

	Ref<Script> scr = p_object->get_script();
	if (scr.is_null() && Object::cast_to<Script>(p_object)) {
		scr = p_object;
	}

	if (scr.is_valid()) {
		Ref<Script> base_scr = scr;
		while (base_scr.is_valid()) {
			StringName name = EditorNode::get_editor_data().script_class_get_name(base_scr->get_path());
			if (name != StringName()) {
				return name;
			}

			// TODO: Should probably be deprecated in 4.x.
			StringName base = base_scr->get_instance_base_type();
			if (base != StringName() && EditorNode::get_editor_data().get_custom_types().has(base)) {
				const Vector<EditorData::CustomType> &types = EditorNode::get_editor_data().get_custom_types()[base];
				for (int i = 0; i < types.size(); ++i) {
					if (types[i].script == base_scr) {
						return types[i].name;
					}
				}
			}
			base_scr = base_scr->get_base_script();
		}
	}

	return StringName();
}

void EditorNode::_pick_main_scene_custom_action(const String &p_custom_action_name) {
	if (p_custom_action_name == "select_current") {
		Node *scene = editor_data.get_edited_scene_root();

		if (!scene) {
			show_accept(TTR("There is no defined scene to run."), TTR("OK"));
			return;
		}

		pick_main_scene->hide();

		if (!FileAccess::exists(scene->get_scene_file_path())) {
			current_menu_option = FILE_SAVE_AND_RUN_MAIN_SCENE;
			_menu_option_confirm(FILE_SAVE_AS_SCENE, true);
			file->set_title(TTR("Save scene before running..."));
		} else {
			current_menu_option = SETTINGS_PICK_MAIN_SCENE;
			_dialog_action(scene->get_scene_file_path());
		}
	}
}

Ref<Texture2D> EditorNode::_get_class_or_script_icon(const String &p_class, const Ref<Script> &p_script, const String &p_fallback, bool p_fallback_script_to_theme) {
	ERR_FAIL_COND_V_MSG(p_class.is_empty(), nullptr, "Class name cannot be empty.");
	EditorData &ed = EditorNode::get_editor_data();

	// Check for a script icon first.
	if (p_script.is_valid()) {
		Ref<Texture2D> script_icon = ed.get_script_icon(p_script);
		if (script_icon.is_valid()) {
			return script_icon;
		}

		if (p_fallback_script_to_theme) {
			// Look for the native base type in the editor theme. This is relevant for
			// scripts extending other scripts and for built-in classes.
			String script_class_name = p_script->get_language()->get_global_class_name(p_script->get_path());
			String base_type;
			if (script_class_name.is_empty()) {
				base_type = p_script->get_instance_base_type();
			} else {
				base_type = ScriptServer::get_global_class_native_base(script_class_name);
			}

			if (theme.is_valid() && theme->has_icon(base_type, EditorStringName(EditorIcons))) {
				return theme->get_icon(base_type, EditorStringName(EditorIcons));
			}
		}
	}

	// Script was not valid or didn't yield any useful values, try the class name
	// directly.

	// Check if the class name is an extension-defined type.
	Ref<Texture2D> ext_icon = ed.extension_class_get_icon(p_class);
	if (ext_icon.is_valid()) {
		return ext_icon;
	}

	// Check if the class name is a custom type.
	// TODO: Should probably be deprecated in 4.x
	const EditorData::CustomType *ctype = ed.get_custom_type_by_name(p_class);
	if (ctype && ctype->icon.is_valid()) {
		return ctype->icon;
	}

	// Look up the class name or the fallback name in the editor theme.
	// This is only relevant for built-in classes.
	if (theme.is_valid()) {
		if (theme->has_icon(p_class, EditorStringName(EditorIcons))) {
			return theme->get_icon(p_class, EditorStringName(EditorIcons));
		}

		if (!p_fallback.is_empty() && theme->has_icon(p_fallback, EditorStringName(EditorIcons))) {
			return theme->get_icon(p_fallback, EditorStringName(EditorIcons));
		}

		// If the fallback is empty or wasn't found, use the default fallback.
		if (ClassDB::class_exists(p_class)) {
			bool instantiable = !ClassDB::is_virtual(p_class) && ClassDB::can_instantiate(p_class);
			if (ClassDB::is_parent_class(p_class, SNAME("Node"))) {
				return theme->get_icon(instantiable ? "Node" : "NodeDisabled", EditorStringName(EditorIcons));
			} else {
				return theme->get_icon(instantiable ? "Object" : "ObjectDisabled", EditorStringName(EditorIcons));
			}
		}
	}

	return nullptr;
}

Ref<Texture2D> EditorNode::get_object_icon(const Object *p_object, const String &p_fallback) {
	ERR_FAIL_NULL_V_MSG(p_object, nullptr, "Object cannot be null.");

	Ref<Script> scr = p_object->get_script();
	if (scr.is_null() && p_object->is_class("Script")) {
		scr = p_object;
	}

	return _get_class_or_script_icon(p_object->get_class(), scr, p_fallback);
}

Ref<Texture2D> EditorNode::get_class_icon(const String &p_class, const String &p_fallback) {
	ERR_FAIL_COND_V_MSG(p_class.is_empty(), nullptr, "Class name cannot be empty.");

	Ref<Script> scr;
	if (ScriptServer::is_global_class(p_class)) {
		scr = EditorNode::get_editor_data().script_class_load_script(p_class);
	} else if (ResourceLoader::exists(p_class)) { // If the script is not a class_name we check if the script resource exists.
		scr = ResourceLoader::load(p_class);
	}

	return _get_class_or_script_icon(p_class, scr, p_fallback, true);
}

bool EditorNode::is_object_of_custom_type(const Object *p_object, const StringName &p_class) {
	ERR_FAIL_NULL_V(p_object, false);

	Ref<Script> scr = p_object->get_script();
	if (scr.is_null() && Object::cast_to<Script>(p_object)) {
		scr = p_object;
	}

	if (scr.is_valid()) {
		Ref<Script> base_script = scr;
		while (base_script.is_valid()) {
			StringName name = EditorNode::get_editor_data().script_class_get_name(base_script->get_path());
			if (name == p_class) {
				return true;
			}
			base_script = base_script->get_base_script();
		}
	}
	return false;
}

void EditorNode::progress_add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel) {
	if (!singleton) {
		return;
	} else if (singleton->cmdline_export_mode) {
		print_line(p_task + ": begin: " + p_label + " steps: " + itos(p_steps));
	} else if (singleton->progress_dialog) {
		singleton->progress_dialog->add_task(p_task, p_label, p_steps, p_can_cancel);
	}
}

bool EditorNode::progress_task_step(const String &p_task, const String &p_state, int p_step, bool p_force_refresh) {
	if (!singleton) {
		return false;
	} else if (singleton->cmdline_export_mode) {
		print_line("\t" + p_task + ": step " + itos(p_step) + ": " + p_state);
		return false;
	} else if (singleton->progress_dialog) {
		return singleton->progress_dialog->task_step(p_task, p_state, p_step, p_force_refresh);
	} else {
		return false;
	}
}

void EditorNode::progress_end_task(const String &p_task) {
	if (!singleton) {
		return;
	} else if (singleton->cmdline_export_mode) {
		print_line(p_task + ": end");
	} else if (singleton->progress_dialog) {
		singleton->progress_dialog->end_task(p_task);
	}
}

void EditorNode::progress_add_task_bg(const String &p_task, const String &p_label, int p_steps) {
	singleton->progress_hb->add_task(p_task, p_label, p_steps);
}

void EditorNode::progress_task_step_bg(const String &p_task, int p_step) {
	singleton->progress_hb->task_step(p_task, p_step);
}

void EditorNode::progress_end_task_bg(const String &p_task) {
	singleton->progress_hb->end_task(p_task);
}

void EditorNode::_progress_dialog_visibility_changed() {
	// Open the io errors after the progress dialog is closed.
	if (load_errors_queued_to_display && !progress_dialog->is_visible()) {
		EditorInterface::get_singleton()->popup_dialog_centered_ratio(singleton->load_error_dialog, 0.5);
		load_errors_queued_to_display = false;
	}
}

void EditorNode::_load_error_dialog_visibility_changed() {
	if (!load_error_dialog->is_visible()) {
		load_errors->clear();
	}
}

String EditorNode::_get_system_info() const {
	String distribution_name = OS::get_singleton()->get_distribution_name();
	if (distribution_name.is_empty()) {
		distribution_name = OS::get_singleton()->get_name();
	}
	if (distribution_name.is_empty()) {
		distribution_name = "Other";
	}
	const String distribution_version = OS::get_singleton()->get_version_alias();

	String godot_version = "Godot v" + String(VERSION_FULL_CONFIG);
	if (String(VERSION_BUILD) != "official") {
		String hash = String(VERSION_HASH);
		hash = hash.is_empty() ? String("unknown") : vformat("(%s)", hash.left(9));
		godot_version += " " + hash;
	}

	String display_session_type;
#ifdef LINUXBSD_ENABLED
	// `replace` is necessary, because `capitalize` introduces a whitespace between "x" and "11".
	display_session_type = OS::get_singleton()->get_environment("XDG_SESSION_TYPE").capitalize().replace(" ", "");
#endif // LINUXBSD_ENABLED
	String driver_name = OS::get_singleton()->get_current_rendering_driver_name().to_lower();
	String rendering_method = OS::get_singleton()->get_current_rendering_method().to_lower();

	const String rendering_device_name = RenderingServer::get_singleton()->get_video_adapter_name();

	RenderingDevice::DeviceType device_type = RenderingServer::get_singleton()->get_video_adapter_type();
	String device_type_string;
	switch (device_type) {
		case RenderingDevice::DeviceType::DEVICE_TYPE_INTEGRATED_GPU:
			device_type_string = "integrated";
			break;
		case RenderingDevice::DeviceType::DEVICE_TYPE_DISCRETE_GPU:
			device_type_string = "dedicated";
			break;
		case RenderingDevice::DeviceType::DEVICE_TYPE_VIRTUAL_GPU:
			device_type_string = "virtual";
			break;
		case RenderingDevice::DeviceType::DEVICE_TYPE_CPU:
			device_type_string = "(software emulation on CPU)";
			break;
		case RenderingDevice::DeviceType::DEVICE_TYPE_OTHER:
		case RenderingDevice::DeviceType::DEVICE_TYPE_MAX:
			break; // Can't happen, but silences warning for DEVICE_TYPE_MAX
	}

	const Vector<String> video_adapter_driver_info = OS::get_singleton()->get_video_adapter_driver_info();

	const String processor_name = OS::get_singleton()->get_processor_name();
	const int processor_count = OS::get_singleton()->get_processor_count();

	// Prettify
	if (rendering_method == "forward_plus") {
		rendering_method = "Forward+";
	} else if (rendering_method == "mobile") {
		rendering_method = "Mobile";
	} else if (rendering_method == "gl_compatibility") {
		rendering_method = "Compatibility";
	}
	if (driver_name == "vulkan") {
		driver_name = "Vulkan";
	} else if (driver_name == "d3d12") {
		driver_name = "Direct3D 12";
	} else if (driver_name == "opengl3_angle") {
		driver_name = "OpenGL ES 3/ANGLE";
	} else if (driver_name == "opengl3_es") {
		driver_name = "OpenGL ES 3";
	} else if (driver_name == "opengl3") {
		if (OS::get_singleton()->get_gles_over_gl()) {
			driver_name = "OpenGL 3";
		} else {
			driver_name = "OpenGL ES 3";
		}
	} else if (driver_name == "metal") {
		driver_name = "Metal";
	}

	// Join info.
	Vector<String> info;
	info.push_back(godot_version);
	String distribution_display_session_type = distribution_name;
	if (!distribution_version.is_empty()) {
		distribution_display_session_type += " " + distribution_version;
	}
	if (!display_session_type.is_empty()) {
		distribution_display_session_type += " on " + display_session_type;
	}
	info.push_back(distribution_display_session_type);

	String display_driver_window_mode;
#ifdef LINUXBSD_ENABLED
	// `replace` is necessary, because `capitalize` introduces a whitespace between "x" and "11".
	display_driver_window_mode = DisplayServer::get_singleton()->get_name().capitalize().replace(" ", "") + " display driver";
#endif // LINUXBSD_ENABLED
	if (!display_driver_window_mode.is_empty()) {
		display_driver_window_mode += ", ";
	}
	display_driver_window_mode += get_viewport()->is_embedding_subwindows() ? "Single-window" : "Multi-window";

	if (DisplayServer::get_singleton()->get_screen_count() == 1) {
		display_driver_window_mode += ", " + itos(DisplayServer::get_singleton()->get_screen_count()) + " monitor";
	} else {
		display_driver_window_mode += ", " + itos(DisplayServer::get_singleton()->get_screen_count()) + " monitors";
	}

	info.push_back(display_driver_window_mode);

	info.push_back(vformat("%s (%s)", driver_name, rendering_method));

	String graphics;
	if (!device_type_string.is_empty()) {
		graphics = device_type_string + " ";
	}
	graphics += rendering_device_name;
	if (video_adapter_driver_info.size() == 2) { // This vector is always either of length 0 or 2.
		const String &vad_name = video_adapter_driver_info[0];
		const String &vad_version = video_adapter_driver_info[1]; // Version could be potentially empty on Linux/BSD.
		if (!vad_version.is_empty()) {
			graphics += vformat(" (%s; %s)", vad_name, vad_version);
		} else if (!vad_name.is_empty()) {
			graphics += vformat(" (%s)", vad_name);
		}
	}
	info.push_back(graphics);

	info.push_back(vformat("%s (%d threads)", processor_name, processor_count));

	return String(" - ").join(info);
}

bool EditorNode::_should_display_update_spinner() const {
#ifdef DEV_ENABLED
	const bool in_dev = true;
#else
	const bool in_dev = false;
#endif
	const int show_update_spinner_setting = EDITOR_GET("interface/editor/show_update_spinner");
	return (show_update_spinner_setting == 0 && in_dev) || show_update_spinner_setting == 1;
}

Ref<Texture2D> EditorNode::_file_dialog_get_icon(const String &p_path) {
	EditorFileSystemDirectory *efsd = EditorFileSystem::get_singleton()->get_filesystem_path(p_path.get_base_dir());
	if (efsd) {
		String file = p_path.get_file();
		for (int i = 0; i < efsd->get_file_count(); i++) {
			if (efsd->get_file(i) == file) {
				String type = efsd->get_file_type(i);

				if (singleton->icon_type_cache.has(type)) {
					return singleton->icon_type_cache[type];
				} else {
					return singleton->icon_type_cache["Object"];
				}
			}
		}
	}

	return singleton->icon_type_cache["Object"];
}

void EditorNode::_build_icon_type_cache() {
	List<StringName> tl;
	theme->get_icon_list(EditorStringName(EditorIcons), &tl);
	for (const StringName &E : tl) {
		if (!ClassDB::class_exists(E)) {
			continue;
		}
		icon_type_cache[E] = theme->get_icon(E, EditorStringName(EditorIcons));
	}
}

void EditorNode::_enable_pending_addons() {
	for (uint32_t i = 0; i < pending_addons.size(); i++) {
		set_addon_plugin_enabled(pending_addons[i], true);
	}
	pending_addons.clear();
}

void EditorNode::_file_dialog_register(FileDialog *p_dialog) {
	singleton->file_dialogs.insert(p_dialog);
}

void EditorNode::_file_dialog_unregister(FileDialog *p_dialog) {
	singleton->file_dialogs.erase(p_dialog);
}

void EditorNode::_editor_file_dialog_register(EditorFileDialog *p_dialog) {
	singleton->editor_file_dialogs.insert(p_dialog);
}

void EditorNode::_editor_file_dialog_unregister(EditorFileDialog *p_dialog) {
	singleton->editor_file_dialogs.erase(p_dialog);
}

Vector<EditorNodeInitCallback> EditorNode::_init_callbacks;

void EditorNode::_begin_first_scan() {
	// In headless mode, scan right away.
	// This allows users to continue using `godot --headless --editor --quit` to prepare a project.
	if (!DisplayServer::get_singleton()->window_can_draw()) {
		OS::get_singleton()->benchmark_begin_measure("Editor", "First Scan");
		EditorFileSystem::get_singleton()->scan();
		return;
	}

	if (!waiting_for_first_scan) {
		return;
	}
	requested_first_scan = true;
}

Error EditorNode::export_preset(const String &p_preset, const String &p_path, bool p_debug, bool p_pack_only, bool p_android_build_template, bool p_patch, const Vector<String> &p_patches) {
	export_defer.preset = p_preset;
	export_defer.path = p_path;
	export_defer.debug = p_debug;
	export_defer.pack_only = p_pack_only;
	export_defer.android_build_template = p_android_build_template;
	export_defer.patch = p_patch;
	export_defer.patches = p_patches;
	cmdline_export_mode = true;
	return OK;
}

bool EditorNode::is_project_exporting() const {
	return project_export && project_export->is_exporting();
}

void EditorNode::show_accept(const String &p_text, const String &p_title) {
	current_menu_option = -1;
	if (accept) {
		_close_save_scene_progress();
		accept->set_ok_button_text(p_title);
		accept->set_text(p_text);
		accept->reset_size();
		EditorInterface::get_singleton()->popup_dialog_centered_clamped(accept, Size2i(), 0.0);
	}
}

void EditorNode::show_save_accept(const String &p_text, const String &p_title) {
	current_menu_option = -1;
	if (save_accept) {
		_close_save_scene_progress();
		save_accept->set_ok_button_text(p_title);
		save_accept->set_text(p_text);
		save_accept->reset_size();
		EditorInterface::get_singleton()->popup_dialog_centered_clamped(save_accept, Size2i(), 0.0);
	}
}

void EditorNode::show_warning(const String &p_text, const String &p_title) {
	if (warning) {
		_close_save_scene_progress();
		warning->set_text(p_text);
		warning->set_title(p_title);
		warning->reset_size();
		EditorInterface::get_singleton()->popup_dialog_centered_clamped(warning, Size2i(), 0.0);
	} else {
		WARN_PRINT(p_title + " " + p_text);
	}
}

void EditorNode::_copy_warning(const String &p_str) {
	DisplayServer::get_singleton()->clipboard_set(warning->get_text());
}

void EditorNode::_save_editor_layout() {
	if (!load_editor_layout_done) {
		return;
	}
	Ref<ConfigFile> config;
	config.instantiate();
	// Load and amend existing config if it exists.
	config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));

	editor_dock_manager->save_docks_to_config(config, "docks");
	_save_open_scenes_to_config(config);
	_save_central_editor_layout_to_config(config);
	_save_window_settings_to_config(config, "EditorWindow");
	editor_data.get_plugin_window_layout(config);

	config->save(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));
}

void EditorNode::_save_open_scenes_to_config(Ref<ConfigFile> p_layout) {
	PackedStringArray scenes;
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		String path = editor_data.get_scene_path(i);
		if (path.is_empty()) {
			continue;
		}
		scenes.push_back(path);
	}
	p_layout->set_value(EDITOR_NODE_CONFIG_SECTION, "open_scenes", scenes);

	String currently_edited_scene_path = editor_data.get_scene_path(editor_data.get_edited_scene());
	p_layout->set_value(EDITOR_NODE_CONFIG_SECTION, "current_scene", currently_edited_scene_path);
}

void EditorNode::save_editor_layout_delayed() {
	editor_layout_save_delay_timer->start();
}

void EditorNode::_load_editor_layout() {
	EditorProgress ep("loading_editor_layout", TTR("Loading editor"), 5);
	ep.step(TTR("Loading editor layout..."), 0, true);
	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));
	if (err != OK) { // No config.
		// If config is not found, expand the res:// folder and favorites by default.
		TreeItem *root = FileSystemDock::get_singleton()->get_tree_control()->get_item_with_metadata("res://", 0);
		if (root) {
			root->set_collapsed(false);
		}

		TreeItem *favorites = FileSystemDock::get_singleton()->get_tree_control()->get_item_with_metadata("Favorites", 0);
		if (favorites) {
			favorites->set_collapsed(false);
		}

		if (overridden_default_layout >= 0) {
			_layout_menu_option(overridden_default_layout);
		}
	} else {
		ep.step(TTR("Loading docks..."), 1, true);
		editor_dock_manager->load_docks_from_config(config, "docks", true);

		ep.step(TTR("Reopening scenes..."), 2, true);
		_load_open_scenes_from_config(config);

		ep.step(TTR("Loading central editor layout..."), 3, true);
		_load_central_editor_layout_from_config(config);

		ep.step(TTR("Loading plugin window layout..."), 4, true);
		editor_data.set_plugin_window_layout(config);

		ep.step(TTR("Editor layout ready."), 5, true);
	}
	load_editor_layout_done = true;
}

void EditorNode::_save_central_editor_layout_to_config(Ref<ConfigFile> p_config_file) {
	// Bottom panel.

	int center_split_offset = center_split->get_split_offset();
	p_config_file->set_value(EDITOR_NODE_CONFIG_SECTION, "center_split_offset", center_split_offset);

	bottom_panel->save_layout_to_config(p_config_file, EDITOR_NODE_CONFIG_SECTION);

	// Debugger tab.

	int selected_default_debugger_tab_idx = EditorDebuggerNode::get_singleton()->get_default_debugger()->get_current_debugger_tab();
	p_config_file->set_value(EDITOR_NODE_CONFIG_SECTION, "selected_default_debugger_tab_idx", selected_default_debugger_tab_idx);

	// Main editor (plugin).

	editor_main_screen->save_layout_to_config(p_config_file, EDITOR_NODE_CONFIG_SECTION);
}

void EditorNode::_load_central_editor_layout_from_config(Ref<ConfigFile> p_config_file) {
	// Bottom panel.

	bottom_panel->load_layout_from_config(p_config_file, EDITOR_NODE_CONFIG_SECTION);

	if (p_config_file->has_section_key(EDITOR_NODE_CONFIG_SECTION, "center_split_offset")) {
		int center_split_offset = p_config_file->get_value(EDITOR_NODE_CONFIG_SECTION, "center_split_offset");
		center_split->set_split_offset(center_split_offset);
	}

	// Debugger tab.

	if (p_config_file->has_section_key(EDITOR_NODE_CONFIG_SECTION, "selected_default_debugger_tab_idx")) {
		int selected_default_debugger_tab_idx = p_config_file->get_value(EDITOR_NODE_CONFIG_SECTION, "selected_default_debugger_tab_idx");
		EditorDebuggerNode::get_singleton()->get_default_debugger()->switch_to_debugger(selected_default_debugger_tab_idx);
	}

	// Main editor (plugin).

	editor_main_screen->load_layout_from_config(p_config_file, EDITOR_NODE_CONFIG_SECTION);
}

void EditorNode::_save_window_settings_to_config(Ref<ConfigFile> p_layout, const String &p_section) {
	Window *w = get_window();
	if (w) {
		p_layout->set_value(p_section, "screen", w->get_current_screen());

		Window::Mode mode = w->get_mode();
		switch (mode) {
			case Window::MODE_WINDOWED:
				p_layout->set_value(p_section, "mode", "windowed");
				p_layout->set_value(p_section, "size", w->get_size());
				break;
			case Window::MODE_FULLSCREEN:
			case Window::MODE_EXCLUSIVE_FULLSCREEN:
				p_layout->set_value(p_section, "mode", "fullscreen");
				break;
			case Window::MODE_MINIMIZED:
				if (was_window_windowed_last) {
					p_layout->set_value(p_section, "mode", "windowed");
					p_layout->set_value(p_section, "size", w->get_size());
				} else {
					p_layout->set_value(p_section, "mode", "maximized");
				}
				break;
			default:
				p_layout->set_value(p_section, "mode", "maximized");
				break;
		}

		p_layout->set_value(p_section, "position", w->get_position());
	}
}

void EditorNode::_load_open_scenes_from_config(Ref<ConfigFile> p_layout) {
	if (!bool(EDITOR_GET("interface/scene_tabs/restore_scenes_on_load"))) {
		return;
	}

	if (!p_layout->has_section(EDITOR_NODE_CONFIG_SECTION) ||
			!p_layout->has_section_key(EDITOR_NODE_CONFIG_SECTION, "open_scenes")) {
		return;
	}

	restoring_scenes = true;

	PackedStringArray scenes = p_layout->get_value(EDITOR_NODE_CONFIG_SECTION, "open_scenes");
	for (int i = 0; i < scenes.size(); i++) {
		if (FileAccess::exists(scenes[i])) {
			load_scene(scenes[i]);
		}
	}

	if (p_layout->has_section_key(EDITOR_NODE_CONFIG_SECTION, "current_scene")) {
		String current_scene = p_layout->get_value(EDITOR_NODE_CONFIG_SECTION, "current_scene");
		for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
			if (editor_data.get_scene_path(i) == current_scene) {
				_set_current_scene(i);
				break;
			}
		}
	}

	save_editor_layout_delayed();

	restoring_scenes = false;
}

bool EditorNode::has_scenes_in_session() {
	if (!bool(EDITOR_GET("interface/scene_tabs/restore_scenes_on_load"))) {
		return false;
	}
	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));
	if (err != OK) {
		return false;
	}
	if (!config->has_section(EDITOR_NODE_CONFIG_SECTION) || !config->has_section_key(EDITOR_NODE_CONFIG_SECTION, "open_scenes")) {
		return false;
	}
	Array scenes = config->get_value(EDITOR_NODE_CONFIG_SECTION, "open_scenes");
	return !scenes.is_empty();
}

void EditorNode::undo() {
	trigger_menu_option(EDIT_UNDO, true);
}

void EditorNode::redo() {
	trigger_menu_option(EDIT_REDO, true);
}

bool EditorNode::ensure_main_scene(bool p_from_native) {
	pick_main_scene->set_meta("from_native", p_from_native); // Whether from play button or native run.
	String main_scene = GLOBAL_GET("application/run/main_scene");

	if (main_scene.is_empty()) {
		current_menu_option = -1;
		pick_main_scene->set_text(TTR("No main scene has ever been defined. Select one?\nYou can change it later in \"Project Settings\" under the 'application' category."));
		pick_main_scene->popup_centered();

		if (editor_data.get_edited_scene_root()) {
			select_current_scene_button->set_disabled(false);
			select_current_scene_button->grab_focus();
		} else {
			select_current_scene_button->set_disabled(true);
		}

		return false;
	}

	if (!FileAccess::exists(main_scene)) {
		current_menu_option = -1;
		pick_main_scene->set_text(vformat(TTR("Selected scene '%s' does not exist. Select a valid one?\nYou can change it later in \"Project Settings\" under the 'application' category."), main_scene));
		pick_main_scene->popup_centered();
		return false;
	}

	if (ResourceLoader::get_resource_type(main_scene) != "PackedScene") {
		current_menu_option = -1;
		pick_main_scene->set_text(vformat(TTR("Selected scene '%s' is not a scene file. Select a valid one?\nYou can change it later in \"Project Settings\" under the 'application' category."), main_scene));
		pick_main_scene->popup_centered();
		return false;
	}

	return true;
}

void EditorNode::_immediate_dialog_confirmed() {
	immediate_dialog_confirmed = true;
}
bool EditorNode::immediate_confirmation_dialog(const String &p_text, const String &p_ok_text, const String &p_cancel_text, uint32_t p_wrap_width) {
	ConfirmationDialog *cd = memnew(ConfirmationDialog);
	cd->set_text(p_text);
	cd->set_ok_button_text(p_ok_text);
	cd->set_cancel_button_text(p_cancel_text);
	if (p_wrap_width > 0) {
		cd->set_autowrap(true);
		cd->get_label()->set_custom_minimum_size(Size2(p_wrap_width, 0) * EDSCALE);
	}

	cd->connect(SceneStringName(confirmed), callable_mp(singleton, &EditorNode::_immediate_dialog_confirmed));
	singleton->gui_base->add_child(cd);

	cd->popup_centered();

	while (true) {
		DisplayServer::get_singleton()->process_events();
		Main::iteration();
		if (singleton->immediate_dialog_confirmed || !cd->is_visible()) {
			break;
		}
	}

	memdelete(cd);
	return singleton->immediate_dialog_confirmed;
}

void EditorNode::cleanup() {
	_init_callbacks.clear();
}

void EditorNode::_update_layouts_menu() {
	editor_layouts->clear();
	overridden_default_layout = -1;

	editor_layouts->reset_size();
	editor_layouts->add_shortcut(ED_SHORTCUT("layout/save", TTRC("Save Layout...")), SETTINGS_LAYOUT_SAVE);
	editor_layouts->add_shortcut(ED_SHORTCUT("layout/delete", TTRC("Delete Layout...")), SETTINGS_LAYOUT_DELETE);
	editor_layouts->add_separator();
	editor_layouts->add_shortcut(ED_SHORTCUT("layout/default", TTRC("Default")), SETTINGS_LAYOUT_DEFAULT);

	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());
	if (err != OK) {
		return; // No config.
	}

	List<String> layouts;
	config.ptr()->get_sections(&layouts);

	for (const String &layout : layouts) {
		if (layout == TTR("Default")) {
			editor_layouts->remove_item(editor_layouts->get_item_index(SETTINGS_LAYOUT_DEFAULT));
			overridden_default_layout = editor_layouts->get_item_count();
		}

		editor_layouts->add_item(layout);
	}
}

void EditorNode::_layout_menu_option(int p_id) {
	switch (p_id) {
		case SETTINGS_LAYOUT_SAVE: {
			current_menu_option = p_id;
			layout_dialog->set_title(TTR("Save Layout"));
			layout_dialog->set_ok_button_text(TTR("Save"));
			layout_dialog->set_name_line_enabled(true);
			layout_dialog->popup_centered();
		} break;
		case SETTINGS_LAYOUT_DELETE: {
			current_menu_option = p_id;
			layout_dialog->set_title(TTR("Delete Layout"));
			layout_dialog->set_ok_button_text(TTR("Delete"));
			layout_dialog->set_name_line_enabled(false);
			layout_dialog->popup_centered();
		} break;
		case SETTINGS_LAYOUT_DEFAULT: {
			editor_dock_manager->load_docks_from_config(default_layout, "docks");
			_save_editor_layout();
		} break;
		default: {
			Ref<ConfigFile> config;
			config.instantiate();
			Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());
			if (err != OK) {
				return; // No config.
			}

			editor_dock_manager->load_docks_from_config(config, editor_layouts->get_item_text(p_id));
			_save_editor_layout();
		}
	}
}

void EditorNode::_proceed_closing_scene_tabs() {
	List<String>::Element *E = tabs_to_close.front();
	if (!E) {
		if (_is_closing_editor()) {
			current_menu_option = tab_closing_menu_option;
			_menu_option_confirm(tab_closing_menu_option, true);
		} else {
			current_menu_option = -1;
			save_confirmation->hide();
		}
		return;
	}
	String scene_to_close = E->get();
	tabs_to_close.pop_front();

	int tab_idx = -1;
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		if (editor_data.get_scene_path(i) == scene_to_close) {
			tab_idx = i;
			break;
		}
	}
	ERR_FAIL_COND(tab_idx < 0);

	_scene_tab_closed(tab_idx);
}

bool EditorNode::_is_closing_editor() const {
	return tab_closing_menu_option == FILE_QUIT || tab_closing_menu_option == RUN_PROJECT_MANAGER || tab_closing_menu_option == RELOAD_CURRENT_PROJECT;
}

void EditorNode::_scene_tab_closed(int p_tab) {
	current_menu_option = SCENE_TAB_CLOSE;
	tab_closing_idx = p_tab;
	Node *scene = editor_data.get_edited_scene_root(p_tab);
	if (!scene) {
		_discard_changes();
		return;
	}

	String scene_filename = scene->get_scene_file_path();
	String unsaved_message;

	if (EditorUndoRedoManager::get_singleton()->is_history_unsaved(editor_data.get_scene_history_id(p_tab))) {
		if (scene_filename.is_empty()) {
			unsaved_message = TTR("This scene was never saved.");
		} else {
			// Consider editor startup to be a point of saving, so that when you
			// close and reopen the editor, you don't get an excessively long
			// "modified X hours ago".
			const uint64_t last_modified_seconds = Time::get_singleton()->get_unix_time_from_system() - MAX(started_timestamp, FileAccess::get_modified_time(scene->get_scene_file_path()));
			String last_modified_string;
			if (last_modified_seconds < 120) {
				last_modified_string = vformat(TTRN("%d second ago", "%d seconds ago", last_modified_seconds), last_modified_seconds);
			} else if (last_modified_seconds < 7200) {
				last_modified_string = vformat(TTRN("%d minute ago", "%d minutes ago", last_modified_seconds / 60), last_modified_seconds / 60);
			} else {
				last_modified_string = vformat(TTRN("%d hour ago", "%d hours ago", last_modified_seconds / 3600), last_modified_seconds / 3600);
			}
			unsaved_message = vformat(TTR("Scene \"%s\" has unsaved changes.\nLast saved: %s."), scene_filename, last_modified_string);
		}
	} else {
		// Check if any plugin has unsaved changes in that scene.
		for (int i = 0; i < editor_data.get_editor_plugin_count(); i++) {
			unsaved_message = editor_data.get_editor_plugin(i)->get_unsaved_status(scene_filename);
			if (!unsaved_message.is_empty()) {
				break;
			}
		}
	}

	if (!unsaved_message.is_empty()) {
		if (scene_tabs->get_current_tab() != p_tab) {
			_set_current_scene(p_tab);
		}

		save_confirmation->set_ok_button_text(TTR("Save & Close"));
		save_confirmation->set_text(unsaved_message + "\n\n" + TTR("Save before closing?"));
		save_confirmation->reset_size();
		save_confirmation->popup_centered();
	} else {
		_discard_changes();
	}

	save_editor_layout_delayed();
	scene_tabs->update_scene_tabs();
}

void EditorNode::_cancel_close_scene_tab() {
	if (_is_closing_editor()) {
		tab_closing_menu_option = -1;
	}
}

void EditorNode::_toggle_distraction_free_mode() {
	if (EDITOR_GET("interface/editor/separate_distraction_mode")) {
		int screen = editor_main_screen->get_selected_index();

		if (screen == EditorMainScreen::EDITOR_SCRIPT) {
			script_distraction_free = !script_distraction_free;
			set_distraction_free_mode(script_distraction_free);
		} else {
			scene_distraction_free = !scene_distraction_free;
			set_distraction_free_mode(scene_distraction_free);
		}
	} else {
		set_distraction_free_mode(distraction_free->is_pressed());
	}
}

void EditorNode::update_distraction_free_mode() {
	if (!EDITOR_GET("interface/editor/separate_distraction_mode")) {
		return;
	}
	int screen = editor_main_screen->get_selected_index();
	if (screen == EditorMainScreen::EDITOR_SCRIPT) {
		set_distraction_free_mode(script_distraction_free);
	} else {
		set_distraction_free_mode(scene_distraction_free);
	}
}

void EditorNode::set_distraction_free_mode(bool p_enter) {
	distraction_free->set_pressed(p_enter);

	if (p_enter) {
		if (editor_dock_manager->are_docks_visible()) {
			editor_dock_manager->set_docks_visible(false);
		}
	} else {
		editor_dock_manager->set_docks_visible(true);
	}
}

bool EditorNode::is_distraction_free_mode_enabled() const {
	return distraction_free->is_pressed();
}

Dictionary EditorNode::drag_resource(const Ref<Resource> &p_res, Control *p_from) {
	Control *drag_control = memnew(Control);
	TextureRect *drag_preview = memnew(TextureRect);
	Label *label = memnew(Label);
	label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

	Ref<Texture2D> preview;

	{
		// TODO: make proper previews
		Ref<ImageTexture> texture = theme->get_icon(SNAME("FileBigThumb"), EditorStringName(EditorIcons));
		Ref<Image> img = texture->get_image();
		img = img->duplicate();
		img->resize(48, 48); // meh
		preview = ImageTexture::create_from_image(img);
	}

	drag_preview->set_texture(preview);
	drag_control->add_child(drag_preview);
	if (p_res->get_path().is_resource_file()) {
		label->set_text(p_res->get_path().get_file());
	} else if (!p_res->get_name().is_empty()) {
		label->set_text(p_res->get_name());
	} else {
		label->set_text(p_res->get_class());
	}

	drag_control->add_child(label);

	p_from->set_drag_preview(drag_control); // Wait until it enters scene.

	label->set_position(Point2((preview->get_width() - label->get_minimum_size().width) / 2, preview->get_height()));

	Dictionary drag_data;
	drag_data["type"] = "resource";
	drag_data["resource"] = p_res;
	drag_data["from"] = p_from;

	return drag_data;
}

Dictionary EditorNode::drag_files_and_dirs(const Vector<String> &p_paths, Control *p_from) {
	bool has_folder = false;
	bool has_file = false;
	for (int i = 0; i < p_paths.size(); i++) {
		bool is_folder = p_paths[i].ends_with("/");
		has_folder |= is_folder;
		has_file |= !is_folder;
	}

	int max_rows = 6;
	int num_rows = p_paths.size() > max_rows ? max_rows - 1 : p_paths.size(); // Don't waste a row to say "1 more file" - list it instead.
	VBoxContainer *vbox = memnew(VBoxContainer);
	for (int i = 0; i < num_rows; i++) {
		HBoxContainer *hbox = memnew(HBoxContainer);
		TextureRect *icon = memnew(TextureRect);
		Label *label = memnew(Label);
		label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

		if (p_paths[i].ends_with("/")) {
			label->set_text(p_paths[i].substr(0, p_paths[i].length() - 1).get_file());
			icon->set_texture(theme->get_icon(SNAME("Folder"), EditorStringName(EditorIcons)));
		} else {
			label->set_text(p_paths[i].get_file());
			icon->set_texture(theme->get_icon(SNAME("File"), EditorStringName(EditorIcons)));
		}
		icon->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
		icon->set_size(Size2(16, 16));
		hbox->add_child(icon);
		hbox->add_child(label);
		vbox->add_child(hbox);
	}

	if (p_paths.size() > num_rows) {
		Label *label = memnew(Label);
		if (has_file && has_folder) {
			label->set_text(vformat(TTR("%d more files or folders"), p_paths.size() - num_rows));
		} else if (has_folder) {
			label->set_text(vformat(TTR("%d more folders"), p_paths.size() - num_rows));
		} else {
			label->set_text(vformat(TTR("%d more files"), p_paths.size() - num_rows));
		}
		vbox->add_child(label);
	}
	p_from->set_drag_preview(vbox); // Wait until it enters scene.

	Dictionary drag_data;
	drag_data["type"] = has_folder ? "files_and_dirs" : "files";
	drag_data["files"] = p_paths;
	drag_data["from"] = p_from;
	return drag_data;
}

void EditorNode::add_tool_menu_item(const String &p_name, const Callable &p_callback) {
	int idx = tool_menu->get_item_count();
	tool_menu->add_item(p_name, TOOLS_CUSTOM);
	tool_menu->set_item_metadata(idx, p_callback);
}

void EditorNode::add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu) {
	ERR_FAIL_NULL(p_submenu);
	ERR_FAIL_COND(p_submenu->get_parent() != nullptr);
	tool_menu->add_submenu_node_item(p_name, p_submenu, TOOLS_CUSTOM);
}

void EditorNode::remove_tool_menu_item(const String &p_name) {
	for (int i = 0; i < tool_menu->get_item_count(); i++) {
		if (tool_menu->get_item_id(i) != TOOLS_CUSTOM) {
			continue;
		}

		if (tool_menu->get_item_text(i) == p_name) {
			if (tool_menu->get_item_submenu(i) != "") {
				Node *n = tool_menu->get_node(tool_menu->get_item_submenu(i));
				tool_menu->remove_child(n);
				memdelete(n);
			}
			tool_menu->remove_item(i);
			tool_menu->reset_size();
			return;
		}
	}
}

PopupMenu *EditorNode::get_export_as_menu() {
	return export_as_menu;
}

void EditorNode::_dropped_files(const Vector<String> &p_files) {
	String to_path = FileSystemDock::get_singleton()->get_folder_path_at_mouse_position();
	if (to_path.is_empty()) {
		to_path = FileSystemDock::get_singleton()->get_current_directory();
	}
	to_path = ProjectSettings::get_singleton()->globalize_path(to_path);

	_add_dropped_files_recursive(p_files, to_path);

	EditorFileSystem::get_singleton()->scan_changes();
}

void EditorNode::_add_dropped_files_recursive(const Vector<String> &p_files, String to_path) {
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND(dir.is_null());

	for (int i = 0; i < p_files.size(); i++) {
		const String &from = p_files[i];
		String to = to_path.path_join(from.get_file());

		if (dir->dir_exists(from)) {
			Vector<String> sub_files;

			Ref<DirAccess> sub_dir = DirAccess::open(from);
			ERR_FAIL_COND(sub_dir.is_null());

			sub_dir->list_dir_begin();

			String next_file = sub_dir->get_next();
			while (!next_file.is_empty()) {
				if (next_file == "." || next_file == "..") {
					next_file = sub_dir->get_next();
					continue;
				}

				sub_files.push_back(from.path_join(next_file));
				next_file = sub_dir->get_next();
			}

			if (!sub_files.is_empty()) {
				dir->make_dir(to);
				_add_dropped_files_recursive(sub_files, to);
			}

			continue;
		}

		dir->copy(from, to);
	}
}

void EditorNode::_file_access_close_error_notify(const String &p_str) {
	callable_mp_static(&EditorNode::_file_access_close_error_notify_impl).call_deferred(p_str);
}

void EditorNode::_file_access_close_error_notify_impl(const String &p_str) {
	add_io_error(vformat(TTR("Unable to write to file '%s', file in use, locked or lacking permissions."), p_str));
}

// Recursive function to inform nodes that an array of nodes have had their scene reimported.
// It will attempt to call a method named '_nodes_scene_reimported' on every node in the
// tree so that editor scripts which create transient nodes will have the opportunity
// to recreate them.
void EditorNode::_notify_nodes_scene_reimported(Node *p_node, Array p_reimported_nodes) {
	Skeleton3D *skel_3d = Object::cast_to<Skeleton3D>(p_node);
	if (skel_3d) {
		skel_3d->reset_bone_poses();
	} else {
		BoneAttachment3D *attachment = Object::cast_to<BoneAttachment3D>(p_node);
		if (attachment) {
			attachment->notify_rebind_required();
		}
	}

	if (p_node->has_method("_nodes_scene_reimported")) {
		p_node->call("_nodes_scene_reimported", p_reimported_nodes);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_notify_nodes_scene_reimported(p_node->get_child(i), p_reimported_nodes);
	}
}

void EditorNode::reload_scene(const String &p_path) {
	int scene_idx = -1;
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		if (editor_data.get_scene_path(i) == p_path) {
			scene_idx = i;
			break;
		}
	}

	int current_tab = editor_data.get_edited_scene();

	if (scene_idx == -1) {
		if (get_edited_scene()) {
			int current_history_id = editor_data.get_current_edited_scene_history_id();
			bool is_unsaved = EditorUndoRedoManager::get_singleton()->is_history_unsaved(current_history_id);

			// Scene is not open, so at it might be instantiated. We'll refresh the whole scene later.
			EditorUndoRedoManager::get_singleton()->clear_history(current_history_id, false);
			if (is_unsaved) {
				EditorUndoRedoManager::get_singleton()->set_history_as_unsaved(current_history_id);
			}
		}
		return;
	}

	if (current_tab == scene_idx) {
		editor_data.apply_changes_in_editors();
		_save_editor_states(p_path);
	}

	// Reload scene.
	_remove_scene(scene_idx, false);
	load_scene(p_path, true, false, true);

	// Adjust index so tab is back a the previous position.
	editor_data.move_edited_scene_to_index(scene_idx);
	EditorUndoRedoManager::get_singleton()->clear_history(editor_data.get_scene_history_id(scene_idx), false);

	// Recover the tab.
	scene_tabs->set_current_tab(current_tab);
}

void EditorNode::find_all_instances_inheriting_path_in_node(Node *p_root, Node *p_node, const String &p_instance_path, HashSet<Node *> &p_instance_list) {
	String scene_file_path = p_node->get_scene_file_path();

	bool valid_instance_found = false;

	// Attempt to find all the instances matching path we're going to reload.
	if (p_node->get_scene_file_path() == p_instance_path) {
		valid_instance_found = true;
	} else {
		Node *current_node = p_node;

		Ref<SceneState> inherited_state = current_node->get_scene_inherited_state();
		while (inherited_state.is_valid()) {
			String inherited_path = inherited_state->get_path();
			if (inherited_path == p_instance_path) {
				valid_instance_found = true;
				break;
			}

			inherited_state = inherited_state->get_base_scene_state();
		}
	}

	// Instead of adding this instance directly, if its not owned by the scene, walk its ancestors
	// and find the first node still owned by the scene. This is what we will reloading instead.
	if (valid_instance_found) {
		Node *current_node = p_node;
		while (true) {
			if (current_node->get_owner() == p_root || current_node->get_owner() == nullptr) {
				p_instance_list.insert(current_node);
				break;
			}
			current_node = current_node->get_parent();
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		find_all_instances_inheriting_path_in_node(p_root, p_node->get_child(i), p_instance_path, p_instance_list);
	}
}

void EditorNode::preload_reimporting_with_path_in_edited_scenes(const List<String> &p_scenes) {
	EditorProgress progress("preload_reimporting_scene", TTR("Preparing scenes for reload"), editor_data.get_edited_scene_count());

	int original_edited_scene_idx = editor_data.get_edited_scene();

	// Walk through each opened scene to get a global list of all instances which match
	// the current reimported scenes.
	for (int current_scene_idx = 0; current_scene_idx < editor_data.get_edited_scene_count(); current_scene_idx++) {
		progress.step(vformat(TTR("Analyzing scene %s"), editor_data.get_scene_title(current_scene_idx)), current_scene_idx);

		Node *edited_scene_root = editor_data.get_edited_scene_root(current_scene_idx);

		if (edited_scene_root) {
			SceneModificationsEntry scene_modifications;

			for (const String &instance_path : p_scenes) {
				if (editor_data.get_scene_path(current_scene_idx) == instance_path) {
					continue;
				}

				HashSet<Node *> instances_to_reimport;
				find_all_instances_inheriting_path_in_node(edited_scene_root, edited_scene_root, instance_path, instances_to_reimport);
				if (instances_to_reimport.size() > 0) {
					editor_data.set_edited_scene(current_scene_idx);

					List<Node *> instance_list_with_children;
					for (Node *original_node : instances_to_reimport) {
						InstanceModificationsEntry instance_modifications;

						// Fetching all the modified properties of the nodes reimported scene.
						get_preload_scene_modification_table(edited_scene_root, original_node, original_node, instance_modifications);

						instance_modifications.original_node = original_node;
						instance_modifications.instance_path = instance_path;
						scene_modifications.instance_list.push_back(instance_modifications);

						instance_list_with_children.push_back(original_node);
						get_children_nodes(original_node, instance_list_with_children);
					}

					// Search the scene to find nodes that references the nodes will be recreated.
					get_preload_modifications_reference_to_nodes(edited_scene_root, edited_scene_root, instances_to_reimport, instance_list_with_children, scene_modifications.other_instances_modifications);
				}
			}

			if (scene_modifications.instance_list.size() > 0) {
				scenes_modification_table[current_scene_idx] = scene_modifications;
			}
		}
	}

	editor_data.set_edited_scene(original_edited_scene_idx);

	progress.step(TTR("Preparation done."), editor_data.get_edited_scene_count());
}

void EditorNode::reload_instances_with_path_in_edited_scenes() {
	if (scenes_modification_table.size() == 0) {
		return;
	}
	EditorProgress progress("reloading_scene", TTR("Scenes reloading"), editor_data.get_edited_scene_count());
	progress.step(TTR("Reloading..."), 0, true);

	Error err;
	Array replaced_nodes;
	HashMap<String, Ref<PackedScene>> local_scene_cache;

	// Reload the new instances.
	for (KeyValue<int, SceneModificationsEntry> &scene_modifications_elem : scenes_modification_table) {
		for (InstanceModificationsEntry instance_modifications : scene_modifications_elem.value.instance_list) {
			if (!local_scene_cache.has(instance_modifications.instance_path)) {
				Ref<PackedScene> instance_scene_packed_scene = ResourceLoader::load(instance_modifications.instance_path, "", ResourceFormatLoader::CACHE_MODE_REPLACE, &err);

				ERR_FAIL_COND(err != OK);
				ERR_FAIL_COND(instance_scene_packed_scene.is_null());

				local_scene_cache[instance_modifications.instance_path] = instance_scene_packed_scene;
			}
		}
	}

	// Save the current scene state/selection in case of lost.
	Dictionary editor_state = _get_main_scene_state();
	editor_data.save_edited_scene_state(editor_selection, &editor_history, editor_state);
	editor_selection->clear();

	int original_edited_scene_idx = editor_data.get_edited_scene();

	for (KeyValue<int, SceneModificationsEntry> &scene_modifications_elem : scenes_modification_table) {
		// Set the current scene.
		int current_scene_idx = scene_modifications_elem.key;
		SceneModificationsEntry *scene_modifications = &scene_modifications_elem.value;

		editor_data.set_edited_scene(current_scene_idx);
		Node *current_edited_scene = editor_data.get_edited_scene_root(current_scene_idx);

		// Make sure the node is in the tree so that editor_selection can add node smoothly.
		if (original_edited_scene_idx != current_scene_idx) {
			// Prevent scene roots with the same name from being in the tree at the same time.
			Node *original_edited_scene_root = editor_data.get_edited_scene_root(original_edited_scene_idx);
			if (original_edited_scene_root && original_edited_scene_root->get_name() == current_edited_scene->get_name()) {
				scene_root->remove_child(original_edited_scene_root);
			}
			scene_root->add_child(current_edited_scene);
		}

		// Restore the state so that the selection can be updated.
		editor_state = editor_data.restore_edited_scene_state(editor_selection, &editor_history);

		int current_history_id = editor_data.get_current_edited_scene_history_id();
		bool is_unsaved = EditorUndoRedoManager::get_singleton()->is_history_unsaved(current_history_id);

		// Clear the history for this affected tab.
		EditorUndoRedoManager::get_singleton()->clear_history(current_history_id, false);

		// Update the version
		editor_data.is_scene_changed(current_scene_idx);

		for (InstanceModificationsEntry instance_modifications : scene_modifications->instance_list) {
			Node *original_node = instance_modifications.original_node;
			String original_node_file_path = original_node->get_scene_file_path();
			Ref<PackedScene> instance_scene_packed_scene = local_scene_cache[instance_modifications.instance_path];

			// Load a replacement scene for the node.
			Ref<PackedScene> current_packed_scene;
			Ref<PackedScene> base_packed_scene;
			if (original_node_file_path == instance_modifications.instance_path) {
				// If the node file name directly matches the scene we're replacing,
				// just load it since we already cached it.
				current_packed_scene = instance_scene_packed_scene;
			} else {
				// Otherwise, check the inheritance chain, reloading and caching any scenes
				// we require along the way.
				List<String> required_load_paths;

				// Do we need to check if the paths are empty?
				if (!original_node_file_path.is_empty()) {
					required_load_paths.push_front(original_node_file_path);
				}
				Ref<SceneState> inherited_state = original_node->get_scene_inherited_state();
				while (inherited_state.is_valid()) {
					String inherited_path = inherited_state->get_path();
					// Do we need to check if the paths are empty?
					if (!inherited_path.is_empty()) {
						required_load_paths.push_front(inherited_path);
					}
					inherited_state = inherited_state->get_base_scene_state();
				}

				// Ensure the inheritance chain is loaded in the correct order so that cache can
				// be properly updated.
				for (String path : required_load_paths) {
					if (current_packed_scene.is_valid()) {
						base_packed_scene = current_packed_scene;
					}
					if (!local_scene_cache.find(path)) {
						current_packed_scene = ResourceLoader::load(path, "", ResourceFormatLoader::CACHE_MODE_REPLACE, &err);
						local_scene_cache[path] = current_packed_scene;
					} else {
						current_packed_scene = local_scene_cache[path];
					}
				}
			}

			ERR_FAIL_COND(current_packed_scene.is_null());

			// Instantiate early so that caches cleared on load in SceneState can be rebuilt early.
			Node *instantiated_node = nullptr;

			// If we are in a inherited scene, it's easier to create a new base scene and
			// grab the node from there.
			// When scene_path_to_node is '.' and we have scene_inherited_state, it's because
			// it's a multi-level inheritance scene. We should use
			NodePath scene_path_to_node = current_edited_scene->get_path_to(original_node);
			Ref<SceneState> scene_state = current_edited_scene->get_scene_inherited_state();
			if (scene_path_to_node != "." && scene_state.is_valid() && scene_state->get_path() != instance_modifications.instance_path && scene_state->find_node_by_path(scene_path_to_node) >= 0) {
				Node *root_node = scene_state->instantiate(SceneState::GenEditState::GEN_EDIT_STATE_INSTANCE);
				instantiated_node = root_node->get_node(scene_path_to_node);

				if (instantiated_node) {
					if (instantiated_node->get_parent()) {
						// Remove from the root so we can delete it from memory.
						instantiated_node->get_parent()->remove_child(instantiated_node);
						// No need of the additional children that could have been added to the node
						// in the base scene. That will be managed by the 'addition_list' later.
						_remove_all_not_owned_children(instantiated_node, instantiated_node);
						memdelete(root_node);
					}
				} else {
					// Should not happen because we checked with find_node_by_path before, just in case.
					memdelete(root_node);
				}
			}

			if (!instantiated_node) {
				// If no base scene was found to create the node, we will use the reimported packed scene directly.
				// But, when the current edited scene is the reimported scene, it's because it's an inherited scene
				// derived from the reimported scene. In that case, we will not instantiate current_packed_scene, because
				// we would reinstantiate ourself. Using the base scene is better.
				if (current_edited_scene == original_node) {
					if (base_packed_scene.is_valid()) {
						instantiated_node = base_packed_scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
					} else {
						instantiated_node = instance_scene_packed_scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
					}
				} else {
					instantiated_node = current_packed_scene->instantiate(PackedScene::GEN_EDIT_STATE_INSTANCE);
				}
			}
			ERR_FAIL_NULL(instantiated_node);

			// Disconnect all relevant connections, all connections from and persistent connections to.
			for (const KeyValue<NodePath, ModificationNodeEntry> &modification_table_entry : instance_modifications.modifications) {
				for (Connection conn : modification_table_entry.value.connections_from) {
					conn.signal.get_object()->disconnect(conn.signal.get_name(), conn.callable);
				}
				for (ConnectionWithNodePath cwnp : modification_table_entry.value.connections_to) {
					Connection conn = cwnp.connection;
					if (conn.flags & CONNECT_PERSIST) {
						conn.signal.get_object()->disconnect(conn.signal.get_name(), conn.callable);
					}
				}
			}

			// Store all the paths for any selected nodes which are ancestors of the node we're replacing.
			List<NodePath> selected_node_paths;
			for (Node *selected_node : editor_selection->get_selected_node_list()) {
				if (selected_node == original_node || original_node->is_ancestor_of(selected_node)) {
					selected_node_paths.push_back(original_node->get_path_to(selected_node));
					editor_selection->remove_node(selected_node);
				}
			}

			// Remove all nodes which were added as additional elements (they will be restored later).
			for (AdditiveNodeEntry additive_node_entry : instance_modifications.addition_list) {
				Node *addition_node = additive_node_entry.node;
				addition_node->get_parent()->remove_child(addition_node);
			}

			// Clear ownership of the nodes (kind of hack to workaround an issue with
			// replace_by when called on nodes in other tabs).
			List<Node *> nodes_owned_by_original_node;
			original_node->get_owned_by(original_node, &nodes_owned_by_original_node);
			for (Node *owned_node : nodes_owned_by_original_node) {
				owned_node->set_owner(nullptr);
			}

			// Replace the old nodes in the history with the new ones.
			// Otherwise, the history will contain old nodes, and some could still be
			// instantiated if used elsewhere, causing the "current edited item" to be
			// linked to a node that will be destroyed later. This caused the editor to
			// crash when reimporting scenes with animations when "Editable children" was enabled.
			replace_history_reimported_nodes(original_node, instantiated_node, original_node);

			// Reset the editable instance state.
			HashMap<NodePath, SceneEditorDataEntry> scene_editor_data_table;
			Node *owner = original_node->get_owner();
			if (!owner) {
				owner = original_node;
			}

			get_scene_editor_data_for_node(owner, original_node, scene_editor_data_table);

			bool original_node_scene_instance_load_placeholder = original_node->get_scene_instance_load_placeholder();

			// Delete all the remaining node children.
			while (original_node->get_child_count()) {
				Node *child = original_node->get_child(0);

				original_node->remove_child(child);
				child->queue_free();
			}

			// Update the name to match
			instantiated_node->set_name(original_node->get_name());

			// Is this replacing the edited root node?

			if (current_edited_scene == original_node) {
				// Set the instance as un inherited scene of itself.
				instantiated_node->set_scene_inherited_state(instantiated_node->get_scene_instance_state());
				instantiated_node->set_scene_instance_state(nullptr);
				instantiated_node->set_scene_file_path(original_node_file_path);
				current_edited_scene = instantiated_node;
				editor_data.set_edited_scene_root(current_edited_scene);

				if (original_edited_scene_idx == current_scene_idx) {
					// How that the editor executes a redraw while destroying or progressing the EditorProgress,
					// it crashes when the root scene has been replaced because the edited scene
					// was freed and no longer in the scene tree.
					SceneTreeDock::get_singleton()->set_edited_scene(current_edited_scene);
					if (get_tree()) {
						get_tree()->set_edited_scene_root(current_edited_scene);
					}
				}
			}

			// Replace the original node with the instantiated version.
			original_node->replace_by(instantiated_node, false);

			// Mark the old node for deletion.
			original_node->queue_free();

			// Restore the placeholder state from the original node.
			instantiated_node->set_scene_instance_load_placeholder(original_node_scene_instance_load_placeholder);

			// Attempt to re-add all the additional nodes.
			for (AdditiveNodeEntry additive_node_entry : instance_modifications.addition_list) {
				Node *parent_node = instantiated_node->get_node_or_null(additive_node_entry.parent);

				if (!parent_node) {
					parent_node = current_edited_scene;
				}

				parent_node->add_child(additive_node_entry.node);
				parent_node->move_child(additive_node_entry.node, additive_node_entry.index);
				// If the additive node's owner was the node which got replaced, update it.
				if (additive_node_entry.owner == original_node) {
					additive_node_entry.owner = instantiated_node;
				}

				additive_node_entry.node->set_owner(additive_node_entry.owner);

				// If the parent node was lost, attempt to restore the original global transform.
				{
					Node2D *node_2d = Object::cast_to<Node2D>(additive_node_entry.node);
					if (node_2d) {
						node_2d->set_transform(additive_node_entry.transform_2d);
					}

					Node3D *node_3d = Object::cast_to<Node3D>(additive_node_entry.node);
					if (node_3d) {
						node_3d->set_transform(additive_node_entry.transform_3d);
					}
				}
			}

			// Restore the scene's editable instance and folded states.
			for (HashMap<NodePath, SceneEditorDataEntry>::Iterator I = scene_editor_data_table.begin(); I; ++I) {
				Node *node = owner->get_node_or_null(I->key);
				if (node) {
					if (owner != node) {
						owner->set_editable_instance(node, I->value.is_editable);
					}
					node->set_display_folded(I->value.is_display_folded);
				}
			}

			// Restore the selection.
			if (selected_node_paths.size()) {
				for (NodePath selected_node_path : selected_node_paths) {
					Node *selected_node = instantiated_node->get_node_or_null(selected_node_path);
					if (selected_node) {
						editor_selection->add_node(selected_node);
					}
				}
				editor_selection->update();
			}

			// Attempt to restore the modified properties and signals for the instantitated node and all its owned children.
			for (KeyValue<NodePath, ModificationNodeEntry> &E : instance_modifications.modifications) {
				NodePath new_current_path = E.key;
				Node *modifiable_node = instantiated_node->get_node_or_null(new_current_path);

				update_node_from_node_modification_entry(modifiable_node, E.value);
			}
			// Add the newly instantiated node to the edited scene's replaced node list.
			replaced_nodes.push_back(instantiated_node);
		}

		// Attempt to restore the modified properties and signals for the instantitated node and all its owned children.
		for (KeyValue<NodePath, ModificationNodeEntry> &E : scene_modifications->other_instances_modifications) {
			NodePath new_current_path = E.key;
			Node *modifiable_node = current_edited_scene->get_node_or_null(new_current_path);

			if (modifiable_node) {
				update_node_from_node_modification_entry(modifiable_node, E.value);
			}
		}

		if (is_unsaved) {
			EditorUndoRedoManager::get_singleton()->set_history_as_unsaved(current_history_id);
		}

		// Save the current handled scene state.
		editor_data.save_edited_scene_state(editor_selection, &editor_history, editor_state);
		editor_selection->clear();

		// Cleanup the history of the changes.
		editor_history.cleanup_history();

		if (original_edited_scene_idx != current_scene_idx) {
			scene_root->remove_child(current_edited_scene);

			// Ensure the current edited scene is re-added if removed earlier because it has the same name
			// as the reimported scene. The editor could crash when reloading SceneTreeDock if the current
			// edited scene is not in the scene tree.
			Node *original_edited_scene_root = editor_data.get_edited_scene_root(original_edited_scene_idx);
			if (original_edited_scene_root && !original_edited_scene_root->get_parent()) {
				scene_root->add_child(original_edited_scene_root);
			}
		}
	}

	// For the whole editor, call the _notify_nodes_scene_reimported with a list of replaced nodes.
	// To inform anything that depends on them that they should update as appropriate.
	_notify_nodes_scene_reimported(this, replaced_nodes);

	editor_data.set_edited_scene(original_edited_scene_idx);

	editor_data.restore_edited_scene_state(editor_selection, &editor_history);

	progress.step(TTR("Reloading done."), editor_data.get_edited_scene_count());
}

void EditorNode::_remove_all_not_owned_children(Node *p_node, Node *p_owner) {
	Vector<Node *> nodes_to_remove;
	if (p_node != p_owner && p_node->get_owner() != p_owner) {
		nodes_to_remove.push_back(p_node);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child_node = p_node->get_child(i);
		_remove_all_not_owned_children(child_node, p_owner);
	}

	for (Node *node : nodes_to_remove) {
		node->get_parent()->remove_child(node);
		node->queue_free();
	}
}

int EditorNode::plugin_init_callback_count = 0;

void EditorNode::add_plugin_init_callback(EditorPluginInitializeCallback p_callback) {
	ERR_FAIL_COND(plugin_init_callback_count == MAX_INIT_CALLBACKS);

	plugin_init_callbacks[plugin_init_callback_count++] = p_callback;
}

EditorPluginInitializeCallback EditorNode::plugin_init_callbacks[EditorNode::MAX_INIT_CALLBACKS];

int EditorNode::build_callback_count = 0;

void EditorNode::add_build_callback(EditorBuildCallback p_callback) {
	ERR_FAIL_COND(build_callback_count == MAX_INIT_CALLBACKS);

	build_callbacks[build_callback_count++] = p_callback;
}

EditorBuildCallback EditorNode::build_callbacks[EditorNode::MAX_BUILD_CALLBACKS];

bool EditorNode::call_build() {
	bool builds_successful = true;

	for (int i = 0; i < build_callback_count && builds_successful; i++) {
		if (!build_callbacks[i]()) {
			ERR_PRINT("A Godot Engine build callback failed.");
			builds_successful = false;
		}
	}

	if (builds_successful && !editor_data.call_build()) {
		ERR_PRINT("An EditorPlugin build callback failed.");
		builds_successful = false;
	}

	return builds_successful;
}

void EditorNode::_inherit_imported(const String &p_action) {
	open_imported->hide();
	load_scene(open_import_request, true, true);
}

void EditorNode::_open_imported() {
	load_scene(open_import_request, true, false, true);
}

void EditorNode::dim_editor(bool p_dimming) {
	dimmed = p_dimming;
	gui_base->set_modulate(p_dimming ? Color(0.5, 0.5, 0.5) : Color(1, 1, 1));
}

bool EditorNode::is_editor_dimmed() const {
	return dimmed;
}

void EditorNode::open_export_template_manager() {
	export_template_manager->popup_manager();
}

void EditorNode::add_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin) {
	resource_conversion_plugins.push_back(p_plugin);
}

void EditorNode::remove_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin) {
	resource_conversion_plugins.erase(p_plugin);
}

Vector<Ref<EditorResourceConversionPlugin>> EditorNode::find_resource_conversion_plugin_for_resource(const Ref<Resource> &p_for_resource) {
	if (p_for_resource.is_null()) {
		return Vector<Ref<EditorResourceConversionPlugin>>();
	}

	Vector<Ref<EditorResourceConversionPlugin>> ret;
	for (Ref<EditorResourceConversionPlugin> resource_conversion_plugin : resource_conversion_plugins) {
		if (resource_conversion_plugin.is_valid() && resource_conversion_plugin->handles(p_for_resource)) {
			ret.push_back(resource_conversion_plugin);
		}
	}

	return ret;
}

Vector<Ref<EditorResourceConversionPlugin>> EditorNode::find_resource_conversion_plugin_for_type_name(const String &p_type) {
	Vector<Ref<EditorResourceConversionPlugin>> ret;

	if (ClassDB::can_instantiate(p_type)) {
		Ref<Resource> temp = Object::cast_to<Resource>(ClassDB::instantiate(p_type));
		if (temp.is_valid()) {
			for (Ref<EditorResourceConversionPlugin> resource_conversion_plugin : resource_conversion_plugins) {
				if (resource_conversion_plugin.is_valid() && resource_conversion_plugin->handles(temp)) {
					ret.push_back(resource_conversion_plugin);
				}
			}
		}
	}

	return ret;
}

void EditorNode::_update_renderer_color() {
	String rendering_method = renderer->get_selected_metadata();

	if (rendering_method == "forward_plus") {
		renderer->add_theme_color_override(SceneStringName(font_color), theme->get_color(SNAME("forward_plus_color"), EditorStringName(Editor)));
	} else if (rendering_method == "mobile") {
		renderer->add_theme_color_override(SceneStringName(font_color), theme->get_color(SNAME("mobile_color"), EditorStringName(Editor)));
	} else if (rendering_method == "gl_compatibility") {
		renderer->add_theme_color_override(SceneStringName(font_color), theme->get_color(SNAME("gl_compatibility_color"), EditorStringName(Editor)));
	}
}

void EditorNode::_renderer_selected(int p_which) {
	String rendering_method = renderer->get_item_metadata(p_which);

	String current_renderer = GLOBAL_GET("rendering/renderer/rendering_method");

	if (rendering_method == current_renderer) {
		return;
	}

	renderer_request = rendering_method;
	video_restart_dialog->set_text(
			vformat(TTR("Changing the renderer requires restarting the editor.\n\nChoosing Save & Restart will change the rendering method to:\n- Desktop platforms: %s\n- Mobile platforms: %s\n- Web platform: gl_compatibility"),
					renderer_request, renderer_request.replace("forward_plus", "mobile")));
	video_restart_dialog->popup_centered();
	renderer->select(renderer_current);
	_update_renderer_color();
}

void EditorNode::_add_renderer_entry(const String &p_renderer_name, bool p_mark_overridden) {
	String item_text;
	if (p_renderer_name == "forward_plus") {
		item_text = TTR("Forward+");
	}
	if (p_renderer_name == "mobile") {
		item_text = TTR("Mobile");
	}
	if (p_renderer_name == "gl_compatibility") {
		item_text = TTR("Compatibility");
	}
	if (p_mark_overridden) {
		item_text += " " + TTR("(Overridden)");
	}
	renderer->add_item(item_text);
}

void EditorNode::_resource_saved(Ref<Resource> p_resource, const String &p_path) {
	if (singleton->saving_resources_in_path.has(p_resource)) {
		// This is going to be handled by save_resource_in_path when the time is right.
		return;
	}

	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(p_path);
	}

	singleton->editor_folding.save_resource_folding(p_resource, p_path);
}

void EditorNode::_resource_loaded(Ref<Resource> p_resource, const String &p_path) {
	singleton->editor_folding.load_resource_folding(p_resource, p_path);
}

void EditorNode::_feature_profile_changed() {
	Ref<EditorFeatureProfile> profile = feature_profile_manager->get_current_profile();
	if (profile.is_valid()) {
		editor_dock_manager->set_dock_enabled(NodeDock::get_singleton(), !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_NODE_DOCK));
		// The Import dock is useless without the FileSystem dock. Ensure the configuration is valid.
		bool fs_dock_disabled = profile->is_feature_disabled(EditorFeatureProfile::FEATURE_FILESYSTEM_DOCK);
		editor_dock_manager->set_dock_enabled(FileSystemDock::get_singleton(), !fs_dock_disabled);
		editor_dock_manager->set_dock_enabled(ImportDock::get_singleton(), !fs_dock_disabled && !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_IMPORT_DOCK));
		editor_dock_manager->set_dock_enabled(history_dock, !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_HISTORY_DOCK));

		editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_3D, !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_3D));
		editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_SCRIPT, !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_SCRIPT));
		editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_GAME, !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_GAME));
		if (AssetLibraryEditorPlugin::is_available()) {
			editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_ASSETLIB, !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_ASSET_LIB));
		}
	} else {
		editor_dock_manager->set_dock_enabled(ImportDock::get_singleton(), true);
		editor_dock_manager->set_dock_enabled(NodeDock::get_singleton(), true);
		editor_dock_manager->set_dock_enabled(FileSystemDock::get_singleton(), true);
		editor_dock_manager->set_dock_enabled(history_dock, true);
		editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_3D, true);
		editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_SCRIPT, true);
		editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_GAME, true);
		if (AssetLibraryEditorPlugin::is_available()) {
			editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_ASSETLIB, true);
		}
	}
}

void EditorNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("push_item", "object", "property", "inspector_only"), &EditorNode::push_item, DEFVAL(""), DEFVAL(false));

	ClassDB::bind_method("set_edited_scene", &EditorNode::set_edited_scene);

	ClassDB::bind_method("stop_child_process", &EditorNode::stop_child_process);

	ADD_SIGNAL(MethodInfo("request_help_search"));
	ADD_SIGNAL(MethodInfo("script_add_function_request", PropertyInfo(Variant::OBJECT, "obj"), PropertyInfo(Variant::STRING, "function"), PropertyInfo(Variant::PACKED_STRING_ARRAY, "args")));
	ADD_SIGNAL(MethodInfo("resource_saved", PropertyInfo(Variant::OBJECT, "obj")));
	ADD_SIGNAL(MethodInfo("scene_saved", PropertyInfo(Variant::STRING, "path")));
	ADD_SIGNAL(MethodInfo("scene_changed"));
	ADD_SIGNAL(MethodInfo("scene_closed", PropertyInfo(Variant::STRING, "path")));
}

static Node *_resource_get_edited_scene() {
	return EditorNode::get_singleton()->get_edited_scene();
}

void EditorNode::_print_handler(void *p_this, const String &p_string, bool p_error, bool p_rich) {
	callable_mp_static(&EditorNode::_print_handler_impl).call_deferred(p_string, p_error, p_rich);
}

void EditorNode::_print_handler_impl(const String &p_string, bool p_error, bool p_rich) {
	if (!singleton) {
		return;
	}
	if (p_error) {
		singleton->log->add_message(p_string, EditorLog::MSG_TYPE_ERROR);
	} else if (p_rich) {
		singleton->log->add_message(p_string, EditorLog::MSG_TYPE_STD_RICH);
	} else {
		singleton->log->add_message(p_string, EditorLog::MSG_TYPE_STD);
	}
}

static void _execute_thread(void *p_ud) {
	EditorNode::ExecuteThreadArgs *eta = (EditorNode::ExecuteThreadArgs *)p_ud;
	Error err = OS::get_singleton()->execute(eta->path, eta->args, &eta->output, &eta->exitcode, true, &eta->execute_output_mutex);
	print_verbose("Thread exit status: " + itos(eta->exitcode));
	if (err != OK) {
		eta->exitcode = err;
	}

	eta->done.set();
}

int EditorNode::execute_and_show_output(const String &p_title, const String &p_path, const List<String> &p_arguments, bool p_close_on_ok, bool p_close_on_errors, String *r_output) {
	if (execute_output_dialog) {
		execute_output_dialog->set_title(p_title);
		execute_output_dialog->get_ok_button()->set_disabled(true);
		execute_outputs->clear();
		execute_outputs->set_scroll_follow(true);
		EditorInterface::get_singleton()->popup_dialog_centered_ratio(execute_output_dialog);
	}

	ExecuteThreadArgs eta;
	eta.path = p_path;
	eta.args = p_arguments;
	eta.exitcode = 255;

	int prev_len = 0;

	eta.execute_output_thread.start(_execute_thread, &eta);

	while (!eta.done.is_set()) {
		{
			MutexLock lock(eta.execute_output_mutex);
			if (prev_len != eta.output.length()) {
				String to_add = eta.output.substr(prev_len, eta.output.length());
				prev_len = eta.output.length();
				execute_outputs->add_text(to_add);
				DisplayServer::get_singleton()->process_events(); // Get rid of pending events.
				Main::iteration();
			}
		}
		OS::get_singleton()->delay_usec(1000);
	}

	eta.execute_output_thread.wait_to_finish();
	execute_outputs->add_text("\nExit Code: " + itos(eta.exitcode));

	if (execute_output_dialog) {
		if (p_close_on_errors && eta.exitcode != 0) {
			execute_output_dialog->hide();
		}
		if (p_close_on_ok && eta.exitcode == 0) {
			execute_output_dialog->hide();
		}

		execute_output_dialog->get_ok_button()->set_disabled(false);
	}

	if (r_output) {
		*r_output = eta.output;
	}
	return eta.exitcode;
}

void EditorNode::set_unfocused_low_processor_usage_mode_enabled(bool p_enabled) {
	unfocused_low_processor_usage_mode_enabled = p_enabled;
}

EditorNode::EditorNode() {
	DEV_ASSERT(!singleton);
	singleton = this;

	set_translation_domain("godot.editor");

	Resource::_get_local_scene_func = _resource_get_edited_scene;

	{
		PortableCompressedTexture2D::set_keep_all_compressed_buffers(true);
		RenderingServer::get_singleton()->set_debug_generate_wireframes(true);

		AudioServer::get_singleton()->set_enable_tagging_used_audio_streams(true);

		// No navigation by default if in editor.
		if (NavigationServer3D::get_singleton()->get_debug_enabled()) {
			NavigationServer3D::get_singleton()->set_active(true);
		} else {
			NavigationServer3D::get_singleton()->set_active(false);
		}

		// No physics by default if in editor.
		PhysicsServer3D::get_singleton()->set_active(false);
		PhysicsServer2D::get_singleton()->set_active(false);

		// No scripting by default if in editor (except for tool).
		ScriptServer::set_scripting_enabled(false);

		if (!DisplayServer::get_singleton()->is_touchscreen_available()) {
			// Only if no touchscreen ui hint, disable emulation just in case.
			Input::get_singleton()->set_emulate_touch_from_mouse(false);
		}
		if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CUSTOM_CURSOR_SHAPE)) {
			DisplayServer::get_singleton()->cursor_set_custom_image(Ref<Resource>());
		}
	}

	SceneState::set_disable_placeholders(true);
	ResourceLoader::clear_translation_remaps(); // Using no remaps if in editor.
	ResourceLoader::clear_path_remaps();
	ResourceLoader::set_create_missing_resources_if_class_unavailable(true);

	EditorPropertyNameProcessor *epnp = memnew(EditorPropertyNameProcessor);
	add_child(epnp);

	EditorUndoRedoManager::get_singleton()->connect("version_changed", callable_mp(this, &EditorNode::_update_undo_redo_allowed));
	EditorUndoRedoManager::get_singleton()->connect("history_changed", callable_mp(this, &EditorNode::_update_undo_redo_allowed));
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &EditorNode::_update_from_settings));
	GDExtensionManager::get_singleton()->connect("extensions_reloaded", callable_mp(this, &EditorNode::_gdextensions_reloaded));

	TranslationServer::get_singleton()->set_enabled(false);
	// Load settings.
	if (!EditorSettings::get_singleton()) {
		EditorSettings::create();
	}

	ED_SHORTCUT("editor/lock_selected_nodes", TTRC("Lock Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | Key::L);
	ED_SHORTCUT("editor/unlock_selected_nodes", TTRC("Unlock Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::L);
	ED_SHORTCUT("editor/group_selected_nodes", TTRC("Group Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | Key::G);
	ED_SHORTCUT("editor/ungroup_selected_nodes", TTRC("Ungroup Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::G);

	FileAccess::set_backup_save(EDITOR_GET("filesystem/on_save/safe_save_on_backup_then_rename"));

	_update_vsync_mode();

	// Warm up the surface upgrade tool as early as possible.
	surface_upgrade_tool = memnew(SurfaceUpgradeTool);
	run_surface_upgrade_tool = EditorSettings::get_singleton()->get_project_metadata("surface_upgrade_tool", "run_on_restart", false);
	if (run_surface_upgrade_tool) {
		SurfaceUpgradeTool::get_singleton()->begin_upgrade();
	}

	{
		bool agile_input_event_flushing = EDITOR_GET("input/buffering/agile_event_flushing");
		bool use_accumulated_input = EDITOR_GET("input/buffering/use_accumulated_input");

		Input::get_singleton()->set_agile_input_event_flushing(agile_input_event_flushing);
		Input::get_singleton()->set_use_accumulated_input(use_accumulated_input);
	}

	{
		int display_scale = EDITOR_GET("interface/editor/display_scale");

		switch (display_scale) {
			case 0:
				// Try applying a suitable display scale automatically.
				EditorScale::set_scale(EditorSettings::get_singleton()->get_auto_display_scale());
				break;
			case 1:
				EditorScale::set_scale(0.75);
				break;
			case 2:
				EditorScale::set_scale(1.0);
				break;
			case 3:
				EditorScale::set_scale(1.25);
				break;
			case 4:
				EditorScale::set_scale(1.5);
				break;
			case 5:
				EditorScale::set_scale(1.75);
				break;
			case 6:
				EditorScale::set_scale(2.0);
				break;
			default:
				EditorScale::set_scale(EDITOR_GET("interface/editor/custom_display_scale"));
				break;
		}
	}

	// Define a minimum window size to prevent UI elements from overlapping or being cut off.
	Window *w = Object::cast_to<Window>(SceneTree::get_singleton()->get_root());
	if (w) {
		const Size2 minimum_size = Size2(1024, 600) * EDSCALE;
		w->set_min_size(minimum_size); // Calling it this early doesn't sync the property with DS.
		DisplayServer::get_singleton()->window_set_min_size(minimum_size);
	}

	EditorFileDialog::set_default_show_hidden_files(EDITOR_GET("filesystem/file_dialog/show_hidden_files"));
	EditorFileDialog::set_default_display_mode((EditorFileDialog::DisplayMode)EDITOR_GET("filesystem/file_dialog/display_mode").operator int());

	int swap_cancel_ok = EDITOR_GET("interface/editor/accept_dialog_cancel_ok_buttons");
	if (swap_cancel_ok != 0) { // 0 is auto, set in register_scene based on DisplayServer.
		// Swap on means OK first.
		AcceptDialog::set_swap_cancel_ok(swap_cancel_ok == 2);
	}

	int ed_root_dir = EDITOR_GET("interface/editor/ui_layout_direction");
	Control::set_root_layout_direction(ed_root_dir);
	Window::set_root_layout_direction(ed_root_dir);

	ResourceLoader::set_abort_on_missing_resources(false);
	ResourceLoader::set_error_notify_func(&EditorNode::add_io_error);
	ResourceLoader::set_dependency_error_notify_func(&EditorNode::_dependency_error_report);

	SceneState::set_instantiation_warning_notify_func([](const String &p_warning) {
		add_io_warning(p_warning);
		callable_mp(EditorInterface::get_singleton(), &EditorInterface::mark_scene_as_unsaved).call_deferred();
	});

	{
		// Register importers at the beginning, so dialogs are created with the right extensions.
		Ref<ResourceImporterTexture> import_texture = memnew(ResourceImporterTexture(true));
		ResourceFormatImporter::get_singleton()->add_importer(import_texture);

		Ref<ResourceImporterLayeredTexture> import_cubemap;
		import_cubemap.instantiate();
		import_cubemap->set_mode(ResourceImporterLayeredTexture::MODE_CUBEMAP);
		ResourceFormatImporter::get_singleton()->add_importer(import_cubemap);

		Ref<ResourceImporterLayeredTexture> import_array;
		import_array.instantiate();
		import_array->set_mode(ResourceImporterLayeredTexture::MODE_2D_ARRAY);
		ResourceFormatImporter::get_singleton()->add_importer(import_array);

		Ref<ResourceImporterLayeredTexture> import_cubemap_array;
		import_cubemap_array.instantiate();
		import_cubemap_array->set_mode(ResourceImporterLayeredTexture::MODE_CUBEMAP_ARRAY);
		ResourceFormatImporter::get_singleton()->add_importer(import_cubemap_array);

		Ref<ResourceImporterLayeredTexture> import_3d = memnew(ResourceImporterLayeredTexture(true));
		import_3d->set_mode(ResourceImporterLayeredTexture::MODE_3D);
		ResourceFormatImporter::get_singleton()->add_importer(import_3d);

		Ref<ResourceImporterImage> import_image;
		import_image.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_image);

		Ref<ResourceImporterTextureAtlas> import_texture_atlas;
		import_texture_atlas.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_texture_atlas);

		Ref<ResourceImporterDynamicFont> import_font_data_dynamic;
		import_font_data_dynamic.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_font_data_dynamic);

		Ref<ResourceImporterBMFont> import_font_data_bmfont;
		import_font_data_bmfont.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_font_data_bmfont);

		Ref<ResourceImporterImageFont> import_font_data_image;
		import_font_data_image.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_font_data_image);

		Ref<ResourceImporterCSVTranslation> import_csv_translation;
		import_csv_translation.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_csv_translation);

		Ref<ResourceImporterWAV> import_wav;
		import_wav.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_wav);

		Ref<ResourceImporterOBJ> import_obj;
		import_obj.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_obj);

		Ref<ResourceImporterShaderFile> import_shader_file;
		import_shader_file.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_shader_file);

		Ref<ResourceImporterScene> import_scene = memnew(ResourceImporterScene("PackedScene", true));
		ResourceFormatImporter::get_singleton()->add_importer(import_scene);

		Ref<ResourceImporterScene> import_animation = memnew(ResourceImporterScene("AnimationLibrary", true));
		ResourceFormatImporter::get_singleton()->add_importer(import_animation);

		{
			Ref<EditorSceneFormatImporterCollada> import_collada;
			import_collada.instantiate();
			ResourceImporterScene::add_scene_importer(import_collada);

			Ref<EditorOBJImporter> import_obj2;
			import_obj2.instantiate();
			ResourceImporterScene::add_scene_importer(import_obj2);

			Ref<EditorSceneFormatImporterESCN> import_escn;
			import_escn.instantiate();
			ResourceImporterScene::add_scene_importer(import_escn);
		}

		Ref<ResourceImporterBitMap> import_bitmap;
		import_bitmap.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_bitmap);
	}

	{
		Ref<EditorInspectorDefaultPlugin> eidp;
		eidp.instantiate();
		EditorInspector::add_inspector_plugin(eidp);

		Ref<EditorInspectorRootMotionPlugin> rmp;
		rmp.instantiate();
		EditorInspector::add_inspector_plugin(rmp);

		Ref<EditorInspectorVisualShaderModePlugin> smp;
		smp.instantiate();
		EditorInspector::add_inspector_plugin(smp);

		Ref<EditorInspectorParticleProcessMaterialPlugin> ppm;
		ppm.instantiate();
		EditorInspector::add_inspector_plugin(ppm);
	}

	editor_selection = memnew(EditorSelection);

	EditorFileSystem *efs = memnew(EditorFileSystem);
	add_child(efs);

	EditorContextMenuPluginManager::create();

	// Used for previews.
	FileDialog::get_icon_func = _file_dialog_get_icon;
	FileDialog::register_func = _file_dialog_register;
	FileDialog::unregister_func = _file_dialog_unregister;

	EditorFileDialog::get_icon_func = _file_dialog_get_icon;
	EditorFileDialog::register_func = _editor_file_dialog_register;
	EditorFileDialog::unregister_func = _editor_file_dialog_unregister;

	editor_export = memnew(EditorExport);
	add_child(editor_export);

	// Exporters might need the theme.
	EditorThemeManager::initialize();
	theme = EditorThemeManager::generate_theme();
	DisplayServer::set_early_window_clear_color_override(true, theme->get_color(SNAME("background"), EditorStringName(Editor)));

	register_exporters();

	ED_SHORTCUT("canvas_item_editor/pan_view", TTRC("Pan View"), Key::SPACE);

	const Vector<String> textfile_ext = ((String)(EDITOR_GET("docks/filesystem/textfile_extensions"))).split(",", false);
	for (const String &E : textfile_ext) {
		textfile_extensions.insert(E);
	}
	const Vector<String> other_file_ext = ((String)(EDITOR_GET("docks/filesystem/other_file_extensions"))).split(",", false);
	for (const String &E : other_file_ext) {
		other_file_extensions.insert(E);
	}

	resource_preview = memnew(EditorResourcePreview);
	add_child(resource_preview);
	progress_dialog = memnew(ProgressDialog);
	progress_dialog->set_unparent_when_invisible(true);
	progress_dialog->connect(SceneStringName(visibility_changed), callable_mp(this, &EditorNode::_progress_dialog_visibility_changed));

	gui_base = memnew(Panel);
	add_child(gui_base);

	// Take up all screen.
	gui_base->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	gui_base->set_anchor(SIDE_RIGHT, Control::ANCHOR_END);
	gui_base->set_anchor(SIDE_BOTTOM, Control::ANCHOR_END);
	gui_base->set_end(Point2(0, 0));

	main_vbox = memnew(VBoxContainer);
	gui_base->add_child(main_vbox);

	title_bar = memnew(EditorTitleBar);
	main_vbox->add_child(title_bar);

	left_l_hsplit = memnew(DockSplitContainer);
	left_l_hsplit->set_name("DockHSplitLeftL");
	main_vbox->add_child(left_l_hsplit);

	left_l_hsplit->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	left_l_vsplit = memnew(DockSplitContainer);
	left_l_vsplit->set_name("DockVSplitLeftL");
	left_l_vsplit->set_vertical(true);
	left_l_hsplit->add_child(left_l_vsplit);

	TabContainer *dock_slot[EditorDockManager::DOCK_SLOT_MAX];
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UL] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UL]->set_name("DockSlotLeftUL");
	left_l_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UL]);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BL] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BL]->set_name("DockSlotLeftBL");
	left_l_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BL]);

	left_r_hsplit = memnew(DockSplitContainer);
	left_r_hsplit->set_name("DockHSplitLeftR");
	left_l_hsplit->add_child(left_r_hsplit);
	left_r_vsplit = memnew(DockSplitContainer);
	left_r_vsplit->set_name("DockVSplitLeftR");
	left_r_vsplit->set_vertical(true);
	left_r_hsplit->add_child(left_r_vsplit);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UR] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UR]->set_name("DockSlotLeftUR");
	left_r_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UR]);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BR] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BR]->set_name("DockSlotLeftBR");
	left_r_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BR]);

	main_hsplit = memnew(DockSplitContainer);
	main_hsplit->set_name("DockHSplitMain");
	left_r_hsplit->add_child(main_hsplit);
	VBoxContainer *center_vb = memnew(VBoxContainer);
	main_hsplit->add_child(center_vb);

	center_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	center_split = memnew(DockSplitContainer);
	center_split->set_name("DockVSplitCenter");
	center_split->set_vertical(true);
	center_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	center_split->set_collapsed(false);
	center_vb->add_child(center_split);

	right_hsplit = memnew(DockSplitContainer);
	right_hsplit->set_name("DockHSplitRight");
	main_hsplit->add_child(right_hsplit);

	right_l_vsplit = memnew(DockSplitContainer);
	right_l_vsplit->set_name("DockVSplitRightL");
	right_l_vsplit->set_vertical(true);
	right_hsplit->add_child(right_l_vsplit);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UL] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UL]->set_name("DockSlotRightUL");
	right_l_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UL]);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BL] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BL]->set_name("DockSlotRightBL");
	right_l_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BL]);

	right_r_vsplit = memnew(DockSplitContainer);
	right_r_vsplit->set_name("DockVSplitRightR");
	right_r_vsplit->set_vertical(true);
	right_hsplit->add_child(right_r_vsplit);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UR] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UR]->set_name("DockSlotRightUR");
	right_r_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UR]);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BR] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BR]->set_name("DockSlotRightBR");
	right_r_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BR]);

	editor_dock_manager = memnew(EditorDockManager);

	// Save the splits for easier access.
	editor_dock_manager->add_vsplit(left_l_vsplit);
	editor_dock_manager->add_vsplit(left_r_vsplit);
	editor_dock_manager->add_vsplit(right_l_vsplit);
	editor_dock_manager->add_vsplit(right_r_vsplit);

	editor_dock_manager->add_hsplit(left_l_hsplit);
	editor_dock_manager->add_hsplit(left_r_hsplit);
	editor_dock_manager->add_hsplit(main_hsplit);
	editor_dock_manager->add_hsplit(right_hsplit);

	for (int i = 0; i < EditorDockManager::DOCK_SLOT_MAX; i++) {
		editor_dock_manager->register_dock_slot((EditorDockManager::DockSlot)i, dock_slot[i]);
	}

	editor_layout_save_delay_timer = memnew(Timer);
	add_child(editor_layout_save_delay_timer);
	editor_layout_save_delay_timer->set_wait_time(0.5);
	editor_layout_save_delay_timer->set_one_shot(true);
	editor_layout_save_delay_timer->connect("timeout", callable_mp(this, &EditorNode::_save_editor_layout));

	scan_changes_timer = memnew(Timer);
	scan_changes_timer->set_wait_time(0.5);
	scan_changes_timer->set_autostart(EDITOR_GET("interface/editor/import_resources_when_unfocused"));
	scan_changes_timer->connect("timeout", callable_mp(EditorFileSystem::get_singleton(), &EditorFileSystem::scan_changes));
	add_child(scan_changes_timer);

	top_split = memnew(VSplitContainer);
	center_split->add_child(top_split);
	top_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	top_split->set_collapsed(true);

	VBoxContainer *srt = memnew(VBoxContainer);
	srt->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	srt->add_theme_constant_override("separation", 0);
	top_split->add_child(srt);

	scene_tabs = memnew(EditorSceneTabs);
	srt->add_child(scene_tabs);
	scene_tabs->connect("tab_changed", callable_mp(this, &EditorNode::_set_current_scene));
	scene_tabs->connect("tab_closed", callable_mp(this, &EditorNode::_scene_tab_closed));

	distraction_free = memnew(Button);
	distraction_free->set_theme_type_variation("FlatMenuButton");
	ED_SHORTCUT_AND_COMMAND("editor/distraction_free_mode", TTRC("Distraction Free Mode"), KeyModifierMask::CTRL | KeyModifierMask::SHIFT | Key::F11);
	ED_SHORTCUT_OVERRIDE("editor/distraction_free_mode", "macos", KeyModifierMask::META | KeyModifierMask::SHIFT | Key::D);
	ED_SHORTCUT_AND_COMMAND("editor/toggle_last_opened_bottom_panel", TTRC("Toggle Last Opened Bottom Panel"), KeyModifierMask::CMD_OR_CTRL | Key::J);
	distraction_free->set_shortcut(ED_GET_SHORTCUT("editor/distraction_free_mode"));
	distraction_free->set_tooltip_text(TTRC("Toggle distraction-free mode."));
	distraction_free->set_toggle_mode(true);
	scene_tabs->add_extra_button(distraction_free);
	distraction_free->connect(SceneStringName(pressed), callable_mp(this, &EditorNode::_toggle_distraction_free_mode));

	editor_main_screen = memnew(EditorMainScreen);
	editor_main_screen->set_custom_minimum_size(Size2(0, 80) * EDSCALE);
	editor_main_screen->set_draw_behind_parent(true);
	srt->add_child(editor_main_screen);
	editor_main_screen->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	scene_root = memnew(SubViewport);
	scene_root->set_embedding_subwindows(true);
	scene_root->set_disable_3d(true);
	scene_root->set_disable_input(true);
	scene_root->set_as_audio_listener_2d(true);

	bool global_menu = !bool(EDITOR_GET("interface/editor/use_embedded_menu")) && NativeMenu::get_singleton()->has_feature(NativeMenu::FEATURE_GLOBAL_MENU);
	bool can_expand = bool(EDITOR_GET("interface/editor/expand_to_title")) && DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_EXTEND_TO_TITLE);

	if (can_expand) {
		// Add spacer to avoid other controls under window minimize/maximize/close buttons (left side).
		left_menu_spacer = memnew(Control);
		left_menu_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
		title_bar->add_child(left_menu_spacer);
	}

	main_menu = memnew(MenuBar);
	main_menu->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	title_bar->add_child(main_menu);
	main_menu->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	main_menu->set_theme_type_variation("MainMenuBar");
	main_menu->set_start_index(0); // Main menu, add to the start of global menu.
	main_menu->set_prefer_global_menu(global_menu);
	main_menu->set_switch_on_hover(true);

	file_menu = memnew(PopupMenu);
	file_menu->set_name(TTR("Scene"));
	main_menu->add_child(file_menu);
	main_menu->set_menu_tooltip(0, TTR("Operations with scene files."));

	accept = memnew(AcceptDialog);
	accept->set_autowrap(true);
	accept->set_min_size(Vector2i(600, 0));
	accept->set_unparent_when_invisible(true);

	save_accept = memnew(AcceptDialog);
	save_accept->set_unparent_when_invisible(true);
	save_accept->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_option).bind((int)MenuOptions::FILE_SAVE_AS_SCENE));

	project_export = memnew(ProjectExportDialog);
	gui_base->add_child(project_export);

	dependency_error = memnew(DependencyErrorDialog);
	gui_base->add_child(dependency_error);

	dependency_fixer = memnew(DependencyEditor);
	gui_base->add_child(dependency_fixer);

	editor_settings_dialog = memnew(EditorSettingsDialog);
	gui_base->add_child(editor_settings_dialog);

	project_settings_editor = memnew(ProjectSettingsEditor(&editor_data));
	gui_base->add_child(project_settings_editor);

	scene_import_settings = memnew(SceneImportSettingsDialog);
	gui_base->add_child(scene_import_settings);

	audio_stream_import_settings = memnew(AudioStreamImportSettingsDialog);
	gui_base->add_child(audio_stream_import_settings);

	fontdata_import_settings = memnew(DynamicFontImportSettingsDialog);
	gui_base->add_child(fontdata_import_settings);

	export_template_manager = memnew(ExportTemplateManager);
	gui_base->add_child(export_template_manager);

	feature_profile_manager = memnew(EditorFeatureProfileManager);
	gui_base->add_child(feature_profile_manager);

	build_profile_manager = memnew(EditorBuildProfileManager);
	gui_base->add_child(build_profile_manager);

	about = memnew(EditorAbout);
	gui_base->add_child(about);
	feature_profile_manager->connect("current_feature_profile_changed", callable_mp(this, &EditorNode::_feature_profile_changed));

#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	fbx_importer_manager = memnew(FBXImporterManager);
	gui_base->add_child(fbx_importer_manager);
#endif

	warning = memnew(AcceptDialog);
	warning->set_unparent_when_invisible(true);
	warning->add_button(TTR("Copy Text"), true, "copy");
	warning->connect("custom_action", callable_mp(this, &EditorNode::_copy_warning));

	ED_SHORTCUT("editor/next_tab", TTRC("Next Scene Tab"), KeyModifierMask::CTRL + Key::TAB);
	ED_SHORTCUT("editor/prev_tab", TTRC("Previous Scene Tab"), KeyModifierMask::CTRL + KeyModifierMask::SHIFT + Key::TAB);
	ED_SHORTCUT("editor/filter_files", TTRC("Focus FileSystem Filter"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::ALT + Key::P);

	command_palette = EditorCommandPalette::get_singleton();
	command_palette->set_title(TTR("Command Palette"));
	gui_base->add_child(command_palette);

	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/new_scene", TTRC("New Scene"), KeyModifierMask::CMD_OR_CTRL + Key::N), FILE_NEW_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/new_inherited_scene", TTRC("New Inherited Scene..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::N), FILE_NEW_INHERITED_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/open_scene", TTRC("Open Scene..."), KeyModifierMask::CMD_OR_CTRL + Key::O), FILE_OPEN_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/reopen_closed_scene", TTRC("Reopen Closed Scene"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::T), FILE_OPEN_PREV);

	recent_scenes = memnew(PopupMenu);
	file_menu->add_submenu_node_item(TTR("Open Recent"), recent_scenes, FILE_OPEN_RECENT);
	recent_scenes->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_open_recent_scene));

	file_menu->add_separator();
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/save_scene", TTRC("Save Scene"), KeyModifierMask::CMD_OR_CTRL + Key::S), FILE_SAVE_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/save_scene_as", TTRC("Save Scene As..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::S), FILE_SAVE_AS_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/save_all_scenes", TTRC("Save All Scenes"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + KeyModifierMask::ALT + Key::S), FILE_SAVE_ALL_SCENES);

	file_menu->add_separator();

	file_menu->add_shortcut(ED_SHORTCUT_ARRAY_AND_COMMAND("editor/quick_open", TTRC("Quick Open..."), { int32_t(KeyModifierMask::SHIFT + KeyModifierMask::ALT + Key::O), int32_t(KeyModifierMask::CMD_OR_CTRL + Key::P) }), FILE_QUICK_OPEN);
	ED_SHORTCUT_OVERRIDE_ARRAY("editor/quick_open", "macos", { int32_t(KeyModifierMask::META + KeyModifierMask::CTRL + Key::O), int32_t(KeyModifierMask::CMD_OR_CTRL + Key::P) });
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/quick_open_scene", TTRC("Quick Open Scene..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::O), FILE_QUICK_OPEN_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/quick_open_script", TTRC("Quick Open Script..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::ALT + Key::O), FILE_QUICK_OPEN_SCRIPT);

	file_menu->add_separator();
	export_as_menu = memnew(PopupMenu);
	file_menu->add_submenu_node_item(TTR("Export As..."), export_as_menu);
	export_as_menu->add_shortcut(ED_SHORTCUT("editor/export_as_mesh_library", TTRC("MeshLibrary...")), FILE_EXPORT_MESH_LIBRARY);
	export_as_menu->connect("index_pressed", callable_mp(this, &EditorNode::_export_as_menu_option));

	file_menu->add_separator();
	file_menu->add_shortcut(ED_GET_SHORTCUT("ui_undo"), EDIT_UNDO, false, true);
	file_menu->add_shortcut(ED_GET_SHORTCUT("ui_redo"), EDIT_REDO, false, true);

	file_menu->add_separator();
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/reload_saved_scene", TTRC("Reload Saved Scene")), EDIT_RELOAD_SAVED_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/close_scene", TTRC("Close Scene"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::W), FILE_CLOSE);
	ED_SHORTCUT_OVERRIDE("editor/close_scene", "macos", KeyModifierMask::CMD_OR_CTRL + Key::W);

	if (!global_menu || !OS::get_singleton()->has_feature("macos")) {
		// On macOS  "Quit" and "About" options are in the "app" menu.
		file_menu->add_separator();
		file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/file_quit", TTRC("Quit"), KeyModifierMask::CMD_OR_CTRL + Key::Q), FILE_QUIT, true);
	}

	ED_SHORTCUT_AND_COMMAND("editor/editor_settings", TTRC("Editor Settings..."));
	ED_SHORTCUT_OVERRIDE("editor/editor_settings", "macos", KeyModifierMask::META + Key::COMMA);
#ifdef MACOS_ENABLED
	if (global_menu && NativeMenu::get_singleton()->has_system_menu(NativeMenu::APPLICATION_MENU_ID)) {
		apple_menu = memnew(PopupMenu);
		apple_menu->set_system_menu(NativeMenu::APPLICATION_MENU_ID);
		main_menu->add_child(apple_menu);

		apple_menu->add_shortcut(ED_GET_SHORTCUT("editor/editor_settings"), SETTINGS_PREFERENCES);
		apple_menu->add_separator();
		apple_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));
	}
#endif

	project_menu = memnew(PopupMenu);
	project_menu->set_name(TTR("Project"));
	main_menu->add_child(project_menu);

	project_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/project_settings", TTRC("Project Settings..."), Key::NONE, TTRC("Project Settings")), RUN_SETTINGS);
	project_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));

	project_menu->add_separator();
	project_menu->add_item(TTR("Version Control"), VCS_MENU);

	project_menu->add_separator();
	project_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/export", TTRC("Export..."), Key::NONE, TTRC("Export")), FILE_EXPORT_PROJECT);
#ifndef ANDROID_ENABLED
	project_menu->add_item(TTR("Install Android Build Template..."), FILE_INSTALL_ANDROID_SOURCE);
	project_menu->add_item(TTR("Open User Data Folder"), RUN_USER_DATA_FOLDER);
#endif

	project_menu->add_separator();

	tool_menu = memnew(PopupMenu);
	tool_menu->connect("index_pressed", callable_mp(this, &EditorNode::_tool_menu_option));
	project_menu->add_submenu_node_item(TTR("Tools"), tool_menu);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/orphan_resource_explorer", TTRC("Orphan Resource Explorer...")), TOOLS_ORPHAN_RESOURCES);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/engine_compilation_configuration_editor", TTRC("Engine Compilation Configuration Editor...")), TOOLS_BUILD_PROFILE_MANAGER);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/upgrade_mesh_surfaces", TTRC("Upgrade Mesh Surfaces...")), TOOLS_SURFACE_UPGRADE);

	project_menu->add_separator();
	project_menu->add_shortcut(ED_SHORTCUT("editor/reload_current_project", TTRC("Reload Current Project")), RELOAD_CURRENT_PROJECT);
	ED_SHORTCUT_AND_COMMAND("editor/quit_to_project_list", TTRC("Quit to Project List"), KeyModifierMask::CTRL + KeyModifierMask::SHIFT + Key::Q);
	ED_SHORTCUT_OVERRIDE("editor/quit_to_project_list", "macos", KeyModifierMask::META + KeyModifierMask::CTRL + KeyModifierMask::ALT + Key::Q);
	project_menu->add_shortcut(ED_GET_SHORTCUT("editor/quit_to_project_list"), RUN_PROJECT_MANAGER, true);

	// Spacer to center 2D / 3D / Script buttons.
	HBoxContainer *left_spacer = memnew(HBoxContainer);
	left_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	left_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_bar->add_child(left_spacer);

	if (can_expand && global_menu) {
		project_title = memnew(Label);
		project_title->add_theme_font_override(SceneStringName(font), theme->get_font(SNAME("bold"), EditorStringName(EditorFonts)));
		project_title->add_theme_font_size_override(SceneStringName(font_size), theme->get_font_size(SNAME("bold_size"), EditorStringName(EditorFonts)));
		project_title->set_focus_mode(Control::FOCUS_NONE);
		project_title->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		project_title->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		project_title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		project_title->set_mouse_filter(Control::MOUSE_FILTER_PASS);
		left_spacer->add_child(project_title);
	}

	HBoxContainer *main_editor_button_hb = memnew(HBoxContainer);
	main_editor_button_hb->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	editor_main_screen->set_button_container(main_editor_button_hb);
	title_bar->add_child(main_editor_button_hb);

	// Options are added and handled by DebuggerEditorPlugin.
	debug_menu = memnew(PopupMenu);
	debug_menu->set_name(TTR("Debug"));
	main_menu->add_child(debug_menu);

	settings_menu = memnew(PopupMenu);
	settings_menu->set_name(TTR("Editor"));
	main_menu->add_child(settings_menu);

#ifdef MACOS_ENABLED
	if (!global_menu) {
		settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/editor_settings"), SETTINGS_PREFERENCES);
	}
#else
	settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/editor_settings"), SETTINGS_PREFERENCES);
#endif
	settings_menu->add_shortcut(ED_SHORTCUT("editor/command_palette", TTRC("Command Palette..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::P), HELP_COMMAND_PALETTE);
	settings_menu->add_separator();

	settings_menu->add_submenu_node_item(TTR("Editor Docks"), editor_dock_manager->get_docks_menu());

	editor_layouts = memnew(PopupMenu);
	editor_layouts->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	settings_menu->add_submenu_node_item(TTR("Editor Layout"), editor_layouts);
	editor_layouts->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_layout_menu_option));
	settings_menu->add_separator();

	ED_SHORTCUT_AND_COMMAND("editor/take_screenshot", TTRC("Take Screenshot"), KeyModifierMask::CTRL | Key::F12);
	ED_SHORTCUT_OVERRIDE("editor/take_screenshot", "macos", KeyModifierMask::META | Key::F12);
	settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/take_screenshot"), EDITOR_SCREENSHOT);

	settings_menu->set_item_tooltip(-1, TTR("Screenshots are stored in the user data folder (\"user://\")."));

	ED_SHORTCUT_AND_COMMAND("editor/fullscreen_mode", TTRC("Toggle Fullscreen"), KeyModifierMask::SHIFT | Key::F11);
	ED_SHORTCUT_OVERRIDE("editor/fullscreen_mode", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::F);
	settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/fullscreen_mode"), SETTINGS_TOGGLE_FULLSCREEN);
	settings_menu->add_separator();

#ifndef ANDROID_ENABLED
	if (OS::get_singleton()->get_data_path() == OS::get_singleton()->get_config_path()) {
		// Configuration and data folders are located in the same place (Windows/macOS).
		settings_menu->add_item(TTR("Open Editor Data/Settings Folder"), SETTINGS_EDITOR_DATA_FOLDER);
	} else {
		// Separate configuration and data folders (Linux).
		settings_menu->add_item(TTR("Open Editor Data Folder"), SETTINGS_EDITOR_DATA_FOLDER);
		settings_menu->add_item(TTR("Open Editor Settings Folder"), SETTINGS_EDITOR_CONFIG_FOLDER);
	}
	settings_menu->add_separator();
#endif

	settings_menu->add_item(TTR("Manage Editor Features..."), SETTINGS_MANAGE_FEATURE_PROFILES);
	settings_menu->add_item(TTR("Manage Export Templates..."), SETTINGS_MANAGE_EXPORT_TEMPLATES);
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	settings_menu->add_item(TTR("Configure FBX Importer..."), SETTINGS_MANAGE_FBX_IMPORTER);
#endif

	help_menu = memnew(PopupMenu);
	help_menu->set_name(TTR("Help"));
	if (global_menu && NativeMenu::get_singleton()->has_system_menu(NativeMenu::HELP_MENU_ID)) {
		help_menu->set_system_menu(NativeMenu::HELP_MENU_ID);
	}
	main_menu->add_child(help_menu);

	help_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));

	ED_SHORTCUT_AND_COMMAND("editor/editor_help", TTRC("Search Help..."), Key::F1);
	ED_SHORTCUT_OVERRIDE("editor/editor_help", "macos", KeyModifierMask::ALT | Key::SPACE);
	help_menu->add_icon_shortcut(theme->get_icon(SNAME("HelpSearch"), EditorStringName(EditorIcons)), ED_GET_SHORTCUT("editor/editor_help"), HELP_SEARCH);
	help_menu->add_separator();
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/online_docs", TTRC("Online Documentation")), HELP_DOCS);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/forum", TTRC("Forum")), HELP_FORUM);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/community", TTRC("Community")), HELP_COMMUNITY);
	help_menu->add_separator();
	help_menu->add_icon_shortcut(theme->get_icon(SNAME("ActionCopy"), EditorStringName(EditorIcons)), ED_SHORTCUT_AND_COMMAND("editor/copy_system_info", TTRC("Copy System Info")), HELP_COPY_SYSTEM_INFO);
	help_menu->set_item_tooltip(-1, TTR("Copies the system info as a single-line text into the clipboard."));
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/report_a_bug", TTRC("Report a Bug")), HELP_REPORT_A_BUG);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/suggest_a_feature", TTRC("Suggest a Feature")), HELP_SUGGEST_A_FEATURE);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/send_docs_feedback", TTRC("Send Docs Feedback")), HELP_SEND_DOCS_FEEDBACK);
	help_menu->add_separator();
	if (!global_menu || !OS::get_singleton()->has_feature("macos")) {
		// On macOS  "Quit" and "About" options are in the "app" menu.
		help_menu->add_icon_shortcut(theme->get_icon(SNAME("Godot"), EditorStringName(EditorIcons)), ED_SHORTCUT_AND_COMMAND("editor/about", TTRC("About Godot...")), HELP_ABOUT);
	}
	help_menu->add_icon_shortcut(theme->get_icon(SNAME("Heart"), EditorStringName(EditorIcons)), ED_SHORTCUT_AND_COMMAND("editor/support_development", TTRC("Support Godot Development")), HELP_SUPPORT_GODOT_DEVELOPMENT);

	// Spacer to center 2D / 3D / Script buttons.
	Control *right_spacer = memnew(Control);
	right_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	right_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_bar->add_child(right_spacer);

	project_run_bar = memnew(EditorRunBar);
	project_run_bar->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	title_bar->add_child(project_run_bar);
	project_run_bar->connect("play_pressed", callable_mp(this, &EditorNode::_project_run_started));
	project_run_bar->connect("stop_pressed", callable_mp(this, &EditorNode::_project_run_stopped));

	HBoxContainer *right_menu_hb = memnew(HBoxContainer);
	right_menu_hb->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	title_bar->add_child(right_menu_hb);

	renderer = memnew(OptionButton);
	renderer->set_visible(true);
	renderer->set_flat(true);
	renderer->set_theme_type_variation("TopBarOptionButton");
	renderer->set_fit_to_longest_item(false);
	renderer->set_focus_mode(Control::FOCUS_NONE);
	renderer->set_tooltip_text(TTR("Choose a rendering method.\n\nNotes:\n- On mobile platforms, the Mobile rendering method is used if Forward+ is selected here.\n- On the web platform, the Compatibility rendering method is always used."));

	right_menu_hb->add_child(renderer);

	if (can_expand) {
		// Add spacer to avoid other controls under the window minimize/maximize/close buttons (right side).
		right_menu_spacer = memnew(Control);
		right_menu_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
		title_bar->add_child(right_menu_spacer);
	}

	String current_renderer_ps = GLOBAL_GET("rendering/renderer/rendering_method");
	current_renderer_ps = current_renderer_ps.to_lower();
	String current_renderer_os = OS::get_singleton()->get_current_rendering_method().to_lower();

	// Add the renderers name to the UI.
	if (current_renderer_ps == current_renderer_os) {
		renderer->connect(SceneStringName(item_selected), callable_mp(this, &EditorNode::_renderer_selected));
		// As we are doing string comparisons, keep in standard case to prevent problems with capitals
		// "vulkan" in particular uses lowercase "v" in the code, and uppercase in the UI.
		PackedStringArray renderers = ProjectSettings::get_singleton()->get_custom_property_info().get(StringName("rendering/renderer/rendering_method")).hint_string.split(",", false);
		for (int i = 0; i < renderers.size(); i++) {
			String rendering_method = renderers[i];
			_add_renderer_entry(rendering_method, false);
			renderer->set_item_metadata(i, rendering_method);
			// Lowercase for standard comparison.
			rendering_method = rendering_method.to_lower();
			if (current_renderer_ps == rendering_method) {
				renderer->select(i);
				renderer_current = i;
			}
		}
	} else {
		// It's an CLI-overridden rendering method.
		_add_renderer_entry(current_renderer_os, true);
		renderer->set_item_metadata(0, current_renderer_os);
		renderer->select(0);
		renderer_current = 0;
	}
	_update_renderer_color();

	video_restart_dialog = memnew(ConfirmationDialog);
	video_restart_dialog->set_ok_button_text(TTR("Save & Restart"));
	video_restart_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_option).bind(SET_RENDERER_NAME_SAVE_AND_RESTART));
	gui_base->add_child(video_restart_dialog);

	progress_hb = memnew(BackgroundProgress);

	layout_dialog = memnew(EditorLayoutsDialog);
	gui_base->add_child(layout_dialog);
	layout_dialog->set_hide_on_ok(false);
	layout_dialog->set_size(Size2(225, 270) * EDSCALE);
	layout_dialog->connect("name_confirmed", callable_mp(this, &EditorNode::_dialog_action));

	update_spinner = memnew(MenuButton);
	right_menu_hb->add_child(update_spinner);
	update_spinner->set_button_icon(theme->get_icon(SNAME("Progress1"), EditorStringName(EditorIcons)));
	update_spinner->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));
	PopupMenu *p = update_spinner->get_popup();
	p->add_radio_check_item(TTR("Update Continuously"), SETTINGS_UPDATE_CONTINUOUSLY);
	p->add_radio_check_item(TTR("Update When Changed"), SETTINGS_UPDATE_WHEN_CHANGED);
	p->add_separator();
	p->add_item(TTR("Hide Update Spinner"), SETTINGS_UPDATE_SPINNER_HIDE);
	_update_update_spinner();

	// Instantiate and place editor docks.

	memnew(SceneTreeDock(scene_root, editor_selection, editor_data));
	memnew(InspectorDock(editor_data));
	memnew(ImportDock);
	memnew(NodeDock);

	FileSystemDock *filesystem_dock = memnew(FileSystemDock);
	filesystem_dock->connect("inherit", callable_mp(this, &EditorNode::_inherit_request));
	filesystem_dock->connect("instantiate", callable_mp(this, &EditorNode::_instantiate_request));
	filesystem_dock->connect("display_mode_changed", callable_mp(this, &EditorNode::_save_editor_layout));
	get_project_settings()->connect_filesystem_dock_signals(filesystem_dock);

	history_dock = memnew(HistoryDock);

	// Scene: Top left.
	editor_dock_manager->add_dock(SceneTreeDock::get_singleton(), TTR("Scene"), EditorDockManager::DOCK_SLOT_LEFT_UR, nullptr, "PackedScene");

	// Import: Top left, behind Scene.
	editor_dock_manager->add_dock(ImportDock::get_singleton(), TTR("Import"), EditorDockManager::DOCK_SLOT_LEFT_UR, nullptr, "FileAccess");

	// FileSystem: Bottom left.
	editor_dock_manager->add_dock(FileSystemDock::get_singleton(), TTR("FileSystem"), EditorDockManager::DOCK_SLOT_LEFT_BR, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_filesystem_bottom_panel", TTRC("Toggle FileSystem Bottom Panel"), KeyModifierMask::ALT | Key::F), "Folder");

	// Inspector: Full height right.
	editor_dock_manager->add_dock(InspectorDock::get_singleton(), TTR("Inspector"), EditorDockManager::DOCK_SLOT_RIGHT_UL, nullptr, "AnimationTrackList");

	// Node: Full height right, behind Inspector.
	editor_dock_manager->add_dock(NodeDock::get_singleton(), TTR("Node"), EditorDockManager::DOCK_SLOT_RIGHT_UL, nullptr, "Object");

	// History: Full height right, behind Node.
	editor_dock_manager->add_dock(history_dock, TTR("History"), EditorDockManager::DOCK_SLOT_RIGHT_UL, nullptr, "History");

	// Add some offsets to left_r and main hsplits to make LEFT_R and RIGHT_L docks wider than minsize.
	left_r_hsplit->set_split_offset(270 * EDSCALE);
	main_hsplit->set_split_offset(-270 * EDSCALE);

	// Define corresponding default layout.

	const String docks_section = "docks";
	default_layout.instantiate();
	// Dock numbers are based on DockSlot enum value + 1.
	default_layout->set_value(docks_section, "dock_3", "Scene,Import");
	default_layout->set_value(docks_section, "dock_4", "FileSystem");
	default_layout->set_value(docks_section, "dock_5", "Inspector,Node,History");

	// There are 4 vsplits and 4 hsplits.
	for (int i = 0; i < editor_dock_manager->get_vsplit_count(); i++) {
		default_layout->set_value(docks_section, "dock_split_" + itos(i + 1), 0);
	}
	default_layout->set_value(docks_section, "dock_hsplit_1", 0);
	default_layout->set_value(docks_section, "dock_hsplit_2", 270);
	default_layout->set_value(docks_section, "dock_hsplit_3", -270);
	default_layout->set_value(docks_section, "dock_hsplit_4", 0);

	_update_layouts_menu();

	// Bottom panels.

	bottom_panel = memnew(EditorBottomPanel);
	center_split->add_child(bottom_panel);
	center_split->set_dragger_visibility(SplitContainer::DRAGGER_HIDDEN);

	log = memnew(EditorLog);
	Button *output_button = bottom_panel->add_item(TTR("Output"), log, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_output_bottom_panel", TTRC("Toggle Output Bottom Panel"), KeyModifierMask::ALT | Key::O));
	log->set_tool_button(output_button);

	center_split->connect(SceneStringName(resized), callable_mp(this, &EditorNode::_vp_resized));

	native_shader_source_visualizer = memnew(EditorNativeShaderSourceVisualizer);
	gui_base->add_child(native_shader_source_visualizer);

	orphan_resources = memnew(OrphanResourcesDialog);
	gui_base->add_child(orphan_resources);

	surface_upgrade_dialog = memnew(SurfaceUpgradeDialog);
	gui_base->add_child(surface_upgrade_dialog);

	confirmation = memnew(ConfirmationDialog);
	gui_base->add_child(confirmation);
	confirmation->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_confirm_current));

	save_confirmation = memnew(ConfirmationDialog);
	save_confirmation->add_button(TTR("Don't Save"), DisplayServer::get_singleton()->get_swap_cancel_ok(), "discard");
	gui_base->add_child(save_confirmation);
	save_confirmation->set_min_size(Vector2(450.0 * EDSCALE, 0));
	save_confirmation->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_confirm_current));
	save_confirmation->connect("custom_action", callable_mp(this, &EditorNode::_discard_changes));
	save_confirmation->connect("canceled", callable_mp(this, &EditorNode::_cancel_close_scene_tab));

	gradle_build_manage_templates = memnew(ConfirmationDialog);
	gradle_build_manage_templates->set_text(TTR("Android build template is missing, please install relevant templates."));
	gradle_build_manage_templates->set_ok_button_text(TTR("Manage Templates"));
	gradle_build_manage_templates->add_button(TTR("Install from file"))->connect(SceneStringName(pressed), callable_mp(this, &EditorNode::_menu_option).bind(SETTINGS_INSTALL_ANDROID_BUILD_TEMPLATE));
	gradle_build_manage_templates->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_option).bind(SETTINGS_MANAGE_EXPORT_TEMPLATES));
	gui_base->add_child(gradle_build_manage_templates);

	file_android_build_source = memnew(EditorFileDialog);
	file_android_build_source->set_title(TTR("Select Android sources file"));
	file_android_build_source->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_android_build_source->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	file_android_build_source->add_filter("*.zip");
	file_android_build_source->connect("file_selected", callable_mp(this, &EditorNode::_android_build_source_selected));
	gui_base->add_child(file_android_build_source);

	{
		VBoxContainer *vbox = memnew(VBoxContainer);
		install_android_build_template_message = memnew(Label);
		install_android_build_template_message->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		install_android_build_template_message->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
		vbox->add_child(install_android_build_template_message);

		choose_android_export_profile = memnew(OptionButton);
		choose_android_export_profile->connect(SceneStringName(item_selected), callable_mp(this, &EditorNode::_android_export_preset_selected));
		vbox->add_child(choose_android_export_profile);

		install_android_build_template = memnew(ConfirmationDialog);
		install_android_build_template->set_ok_button_text(TTR("Install"));
		install_android_build_template->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_confirm_current));
		install_android_build_template->add_child(vbox);
		install_android_build_template->set_min_size(Vector2(500.0 * EDSCALE, 0));
		gui_base->add_child(install_android_build_template);
	}

	remove_android_build_template = memnew(ConfirmationDialog);
	remove_android_build_template->set_ok_button_text(TTR("Show in File Manager"));
	remove_android_build_template->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_option).bind(FILE_EXPLORE_ANDROID_BUILD_TEMPLATES));
	gui_base->add_child(remove_android_build_template);

	file_templates = memnew(EditorFileDialog);
	file_templates->set_title(TTR("Import Templates From ZIP File"));

	gui_base->add_child(file_templates);
	file_templates->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	file_templates->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_templates->clear_filters();
	file_templates->add_filter("*.tpz", TTR("Template Package"));

	file = memnew(EditorFileDialog);
	gui_base->add_child(file);
	file->set_current_dir("res://");
	file->set_transient_to_focused(true);

	file_export_lib = memnew(EditorFileDialog);
	file_export_lib->set_title(TTR("Export Library"));
	file_export_lib->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_export_lib->connect("file_selected", callable_mp(this, &EditorNode::_dialog_action));
	file_export_lib->add_option(TTR("Merge With Existing"), Vector<String>(), true);
	file_export_lib->add_option(TTR("Apply MeshInstance Transforms"), Vector<String>(), false);
	gui_base->add_child(file_export_lib);

	file_script = memnew(EditorFileDialog);
	file_script->set_title(TTR("Open & Run a Script"));
	file_script->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_script->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	List<String> sexts;
	ResourceLoader::get_recognized_extensions_for_type("Script", &sexts);
	for (const String &E : sexts) {
		file_script->add_filter("*." + E);
	}
	gui_base->add_child(file_script);
	file_script->connect("file_selected", callable_mp(this, &EditorNode::_dialog_action));

	file_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));
	file_menu->connect("about_to_popup", callable_mp(this, &EditorNode::_update_file_menu_opened));
	file_menu->connect("popup_hide", callable_mp(this, &EditorNode::_update_file_menu_closed));

	settings_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));

	file->connect("file_selected", callable_mp(this, &EditorNode::_dialog_action));
	file_templates->connect("file_selected", callable_mp(this, &EditorNode::_dialog_action));

	audio_preview_gen = memnew(AudioStreamPreviewGenerator);
	add_child(audio_preview_gen);

	add_editor_plugin(memnew(DebuggerEditorPlugin(debug_menu)));

	disk_changed = memnew(ConfirmationDialog);
	{
		disk_changed->set_title(TTR("Files have been modified outside Godot"));

		VBoxContainer *vbc = memnew(VBoxContainer);
		disk_changed->add_child(vbc);

		Label *dl = memnew(Label);
		dl->set_text(TTR("The following files are newer on disk:"));
		vbc->add_child(dl);

		disk_changed_list = memnew(Tree);
		vbc->add_child(disk_changed_list);
		disk_changed_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);

		Label *what_action_label = memnew(Label);
		what_action_label->set_text(TTR("What action should be taken?"));
		vbc->add_child(what_action_label);

		disk_changed->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_reload_modified_scenes));
		disk_changed->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_reload_project_settings));
		disk_changed->set_ok_button_text(TTR("Reload from disk"));

		disk_changed->add_button(TTR("Ignore external changes"), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "resave");
		disk_changed->connect("custom_action", callable_mp(this, &EditorNode::_resave_scenes));
	}

	gui_base->add_child(disk_changed);

	add_editor_plugin(memnew(AnimationPlayerEditorPlugin));
	add_editor_plugin(memnew(AnimationTrackKeyEditEditorPlugin));
	add_editor_plugin(memnew(AnimationMarkerKeyEditEditorPlugin));
	add_editor_plugin(memnew(CanvasItemEditorPlugin));
	add_editor_plugin(memnew(Node3DEditorPlugin));
	add_editor_plugin(memnew(ScriptEditorPlugin));
	add_editor_plugin(memnew(GameViewPlugin));

	EditorAudioBuses *audio_bus_editor = EditorAudioBuses::register_editor();

	ScriptTextEditor::register_editor(); // Register one for text scripts.
	TextEditor::register_editor();

	if (AssetLibraryEditorPlugin::is_available()) {
		add_editor_plugin(memnew(AssetLibraryEditorPlugin));
	} else {
		print_verbose("Asset Library not available (due to using Web editor, or SSL support disabled).");
	}

	// More visually meaningful to have this later.
	bottom_panel->move_item_to_end(AnimationPlayerEditor::get_singleton());

	add_editor_plugin(VersionControlEditorPlugin::get_singleton());

	vcs_actions_menu = VersionControlEditorPlugin::get_singleton()->get_version_control_actions_panel();
	vcs_actions_menu->connect("index_pressed", callable_mp(this, &EditorNode::_version_control_menu_option));
	vcs_actions_menu->add_item(TTR("Create/Override Version Control Metadata..."), RUN_VCS_METADATA);
	vcs_actions_menu->add_item(TTR("Version Control Settings..."), RUN_VCS_SETTINGS);
	project_menu->set_item_submenu_node(project_menu->get_item_index(VCS_MENU), vcs_actions_menu);

	add_editor_plugin(memnew(AudioBusesEditorPlugin(audio_bus_editor)));

	for (int i = 0; i < EditorPlugins::get_plugin_count(); i++) {
		add_editor_plugin(EditorPlugins::create(i));
	}

	for (const StringName &extension_class_name : GDExtensionEditorPlugins::get_extension_classes()) {
		add_extension_editor_plugin(extension_class_name);
	}
	GDExtensionEditorPlugins::editor_node_add_plugin = &EditorNode::add_extension_editor_plugin;
	GDExtensionEditorPlugins::editor_node_remove_plugin = &EditorNode::remove_extension_editor_plugin;

	for (int i = 0; i < plugin_init_callback_count; i++) {
		plugin_init_callbacks[i]();
	}

	resource_preview->add_preview_generator(Ref<EditorTexturePreviewPlugin>(memnew(EditorTexturePreviewPlugin)));
	resource_preview->add_preview_generator(Ref<EditorImagePreviewPlugin>(memnew(EditorImagePreviewPlugin)));
	resource_preview->add_preview_generator(Ref<EditorPackedScenePreviewPlugin>(memnew(EditorPackedScenePreviewPlugin)));
	resource_preview->add_preview_generator(Ref<EditorMaterialPreviewPlugin>(memnew(EditorMaterialPreviewPlugin)));
	resource_preview->add_preview_generator(Ref<EditorScriptPreviewPlugin>(memnew(EditorScriptPreviewPlugin)));
	resource_preview->add_preview_generator(Ref<EditorAudioStreamPreviewPlugin>(memnew(EditorAudioStreamPreviewPlugin)));
	resource_preview->add_preview_generator(Ref<EditorMeshPreviewPlugin>(memnew(EditorMeshPreviewPlugin)));
	resource_preview->add_preview_generator(Ref<EditorBitmapPreviewPlugin>(memnew(EditorBitmapPreviewPlugin)));
	resource_preview->add_preview_generator(Ref<EditorFontPreviewPlugin>(memnew(EditorFontPreviewPlugin)));
	resource_preview->add_preview_generator(Ref<EditorGradientPreviewPlugin>(memnew(EditorGradientPreviewPlugin)));

	{
		Ref<StandardMaterial3DConversionPlugin> spatial_mat_convert;
		spatial_mat_convert.instantiate();
		resource_conversion_plugins.push_back(spatial_mat_convert);

		Ref<ORMMaterial3DConversionPlugin> orm_mat_convert;
		orm_mat_convert.instantiate();
		resource_conversion_plugins.push_back(orm_mat_convert);

		Ref<CanvasItemMaterialConversionPlugin> canvas_item_mat_convert;
		canvas_item_mat_convert.instantiate();
		resource_conversion_plugins.push_back(canvas_item_mat_convert);

		Ref<ParticleProcessMaterialConversionPlugin> particles_mat_convert;
		particles_mat_convert.instantiate();
		resource_conversion_plugins.push_back(particles_mat_convert);

		Ref<ProceduralSkyMaterialConversionPlugin> procedural_sky_mat_convert;
		procedural_sky_mat_convert.instantiate();
		resource_conversion_plugins.push_back(procedural_sky_mat_convert);

		Ref<PanoramaSkyMaterialConversionPlugin> panorama_sky_mat_convert;
		panorama_sky_mat_convert.instantiate();
		resource_conversion_plugins.push_back(panorama_sky_mat_convert);

		Ref<PhysicalSkyMaterialConversionPlugin> physical_sky_mat_convert;
		physical_sky_mat_convert.instantiate();
		resource_conversion_plugins.push_back(physical_sky_mat_convert);

		Ref<FogMaterialConversionPlugin> fog_mat_convert;
		fog_mat_convert.instantiate();
		resource_conversion_plugins.push_back(fog_mat_convert);

		Ref<VisualShaderConversionPlugin> vshader_convert;
		vshader_convert.instantiate();
		resource_conversion_plugins.push_back(vshader_convert);
	}

	update_spinner_step_msec = OS::get_singleton()->get_ticks_msec();
	update_spinner_step_frame = Engine::get_singleton()->get_frames_drawn();

	editor_plugins_over = memnew(EditorPluginList);
	editor_plugins_force_over = memnew(EditorPluginList);
	editor_plugins_force_input_forwarding = memnew(EditorPluginList);

	Ref<GDExtensionExportPlugin> gdextension_export_plugin;
	gdextension_export_plugin.instantiate();

	EditorExport::get_singleton()->add_export_plugin(gdextension_export_plugin);

	Ref<DedicatedServerExportPlugin> dedicated_server_export_plugin;
	dedicated_server_export_plugin.instantiate();

	EditorExport::get_singleton()->add_export_plugin(dedicated_server_export_plugin);

	Ref<PackedSceneEditorTranslationParserPlugin> packed_scene_translation_parser_plugin;
	packed_scene_translation_parser_plugin.instantiate();
	EditorTranslationParser::get_singleton()->add_parser(packed_scene_translation_parser_plugin, EditorTranslationParser::STANDARD);

	_edit_current();
	current = nullptr;
	saving_resource = Ref<Resource>();

	set_process(true);

	open_imported = memnew(ConfirmationDialog);
	open_imported->set_ok_button_text(TTR("Open Anyway"));
	new_inherited_button = open_imported->add_button(TTR("New Inherited"), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "inherit");
	open_imported->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_open_imported));
	open_imported->connect("custom_action", callable_mp(this, &EditorNode::_inherit_imported));
	gui_base->add_child(open_imported);

	quick_open_dialog = memnew(EditorQuickOpenDialog);
	gui_base->add_child(quick_open_dialog);

	quick_open_color_palette = memnew(EditorQuickOpenDialog);
	gui_base->add_child(quick_open_color_palette);

	_update_recent_scenes();

	set_process_shortcut_input(true);

	load_errors = memnew(RichTextLabel);
	load_error_dialog = memnew(AcceptDialog);
	load_error_dialog->set_unparent_when_invisible(true);
	load_error_dialog->add_child(load_errors);
	load_error_dialog->set_title(TTR("Load Errors"));
	load_error_dialog->connect(SceneStringName(visibility_changed), callable_mp(this, &EditorNode::_load_error_dialog_visibility_changed));

	execute_outputs = memnew(RichTextLabel);
	execute_outputs->set_selection_enabled(true);
	execute_outputs->set_context_menu_enabled(true);
	execute_output_dialog = memnew(AcceptDialog);
	execute_output_dialog->set_unparent_when_invisible(true);
	execute_output_dialog->add_child(execute_outputs);
	execute_output_dialog->set_title("");

	EditorFileSystem::get_singleton()->connect("sources_changed", callable_mp(this, &EditorNode::_sources_changed));
	EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &EditorNode::_fs_changed));
	EditorFileSystem::get_singleton()->connect("resources_reimporting", callable_mp(this, &EditorNode::_resources_reimporting));
	EditorFileSystem::get_singleton()->connect("resources_reimported", callable_mp(this, &EditorNode::_resources_reimported));
	EditorFileSystem::get_singleton()->connect("resources_reload", callable_mp(this, &EditorNode::_resources_changed));

	_build_icon_type_cache();

	pick_main_scene = memnew(ConfirmationDialog);
	gui_base->add_child(pick_main_scene);
	pick_main_scene->set_ok_button_text(TTR("Select"));
	pick_main_scene->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_option).bind(SETTINGS_PICK_MAIN_SCENE));
	select_current_scene_button = pick_main_scene->add_button(TTR("Select Current"), true, "select_current");
	pick_main_scene->connect("custom_action", callable_mp(this, &EditorNode::_pick_main_scene_custom_action));

	for (int i = 0; i < _init_callbacks.size(); i++) {
		_init_callbacks[i]();
	}

	editor_data.add_edited_scene(-1);
	editor_data.set_edited_scene(0);
	scene_tabs->update_scene_tabs();

	ImportDock::get_singleton()->initialize_import_options();

	FileAccess::set_file_close_fail_notify_callback(_file_access_close_error_notify);

	print_handler.printfunc = _print_handler;
	print_handler.userdata = this;
	add_print_handler(&print_handler);

	ResourceSaver::set_save_callback(_resource_saved);
	ResourceLoader::set_load_callback(_resource_loaded);

	// Use the Ctrl modifier so F2 can be used to rename nodes in the scene tree dock.
	ED_SHORTCUT_AND_COMMAND("editor/editor_2d", TTRC("Open 2D Editor"), KeyModifierMask::CTRL | Key::F1);
	ED_SHORTCUT_AND_COMMAND("editor/editor_3d", TTRC("Open 3D Editor"), KeyModifierMask::CTRL | Key::F2);
	ED_SHORTCUT_AND_COMMAND("editor/editor_script", TTRC("Open Script Editor"), KeyModifierMask::CTRL | Key::F3);
	ED_SHORTCUT_AND_COMMAND("editor/editor_game", TTRC("Open Game View"), KeyModifierMask::CTRL | Key::F4);
	ED_SHORTCUT_AND_COMMAND("editor/editor_assetlib", TTRC("Open Asset Library"), KeyModifierMask::CTRL | Key::F5);

	ED_SHORTCUT_OVERRIDE("editor/editor_2d", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::KEY_1);
	ED_SHORTCUT_OVERRIDE("editor/editor_3d", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::KEY_2);
	ED_SHORTCUT_OVERRIDE("editor/editor_script", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::KEY_3);
	ED_SHORTCUT_OVERRIDE("editor/editor_game", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::KEY_4);
	ED_SHORTCUT_OVERRIDE("editor/editor_assetlib", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::KEY_5);

	ED_SHORTCUT_AND_COMMAND("editor/editor_next", TTRC("Open the next Editor"));
	ED_SHORTCUT_AND_COMMAND("editor/editor_prev", TTRC("Open the previous Editor"));

	// Apply setting presets in case the editor_settings file is missing values.
	EditorSettingsDialog::update_navigation_preset();

	screenshot_timer = memnew(Timer);
	screenshot_timer->set_one_shot(true);
	screenshot_timer->set_wait_time(settings_menu->get_submenu_popup_delay() + 0.1f);
	screenshot_timer->connect("timeout", callable_mp(this, &EditorNode::_request_screenshot));
	add_child(screenshot_timer);
	screenshot_timer->set_owner(get_owner());

	// Adjust spacers to center 2D / 3D / Script buttons.
	int max_w = MAX(project_run_bar->get_minimum_size().x + right_menu_hb->get_minimum_size().x, main_menu->get_minimum_size().x);
	left_spacer->set_custom_minimum_size(Size2(MAX(0, max_w - main_menu->get_minimum_size().x), 0));
	right_spacer->set_custom_minimum_size(Size2(MAX(0, max_w - project_run_bar->get_minimum_size().x - right_menu_hb->get_minimum_size().x), 0));

	// Extend menu bar to window title.
	if (can_expand) {
		DisplayServer::get_singleton()->process_events();
		DisplayServer::get_singleton()->window_set_flag(DisplayServer::WINDOW_FLAG_EXTEND_TO_TITLE, true, DisplayServer::MAIN_WINDOW_ID);
		title_bar->set_can_move_window(true);
	}

	{
		const String exec = OS::get_singleton()->get_executable_path();
		const String old_exec = EditorSettings::get_singleton()->get_project_metadata("editor_metadata", "executable_path", "");
		// Save editor executable path for third-party tools.
		if (exec != old_exec) {
			EditorSettings::get_singleton()->set_project_metadata("editor_metadata", "executable_path", exec);
		}
	}

	follow_system_theme = EDITOR_GET("interface/theme/follow_system_theme");
	use_system_accent_color = EDITOR_GET("interface/theme/use_system_accent_color");
	system_theme_timer = memnew(Timer);
	system_theme_timer->set_wait_time(1.0);
	system_theme_timer->connect("timeout", callable_mp(this, &EditorNode::_check_system_theme_changed));
	add_child(system_theme_timer);
	system_theme_timer->set_owner(get_owner());
	system_theme_timer->set_autostart(true);
}

EditorNode::~EditorNode() {
	EditorInspector::cleanup_plugins();
	EditorTranslationParser::get_singleton()->clean_parsers();
	ResourceImporterScene::clean_up_importer_plugins();
	EditorContextMenuPluginManager::cleanup();

	remove_print_handler(&print_handler);
	EditorHelp::cleanup_doc();
#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
	EditorHelpHighlighter::free_singleton();
#endif
	memdelete(editor_selection);
	memdelete(editor_plugins_over);
	memdelete(editor_plugins_force_over);
	memdelete(editor_plugins_force_input_forwarding);
	memdelete(progress_hb);
	memdelete(surface_upgrade_tool);
	memdelete(editor_dock_manager);

	EditorSettings::destroy();
	EditorThemeManager::finalize();

	GDExtensionEditorPlugins::editor_node_add_plugin = nullptr;
	GDExtensionEditorPlugins::editor_node_remove_plugin = nullptr;

	FileDialog::get_icon_func = nullptr;
	FileDialog::register_func = nullptr;
	FileDialog::unregister_func = nullptr;

	EditorFileDialog::get_icon_func = nullptr;
	EditorFileDialog::register_func = nullptr;
	EditorFileDialog::unregister_func = nullptr;

	file_dialogs.clear();
	editor_file_dialogs.clear();

	singleton = nullptr;
}

/*
 * EDITOR PLUGIN LIST
 */

void EditorPluginList::make_visible(bool p_visible) {
	for (int i = 0; i < plugins_list.size(); i++) {
		plugins_list[i]->make_visible(p_visible);
	}
}

void EditorPluginList::edit(Object *p_object) {
	for (int i = 0; i < plugins_list.size(); i++) {
		plugins_list[i]->edit(p_object);
	}
}

bool EditorPluginList::forward_gui_input(const Ref<InputEvent> &p_event) {
	bool discard = false;

	for (int i = 0; i < plugins_list.size(); i++) {
		if (plugins_list[i]->forward_canvas_gui_input(p_event)) {
			discard = true;
		}
	}

	return discard;
}

EditorPlugin::AfterGUIInput EditorPluginList::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event, bool serve_when_force_input_enabled) {
	EditorPlugin::AfterGUIInput after = EditorPlugin::AFTER_GUI_INPUT_PASS;

	for (int i = 0; i < plugins_list.size(); i++) {
		if ((!serve_when_force_input_enabled) && plugins_list[i]->is_input_event_forwarding_always_enabled()) {
			continue;
		}

		EditorPlugin::AfterGUIInput current_after = plugins_list[i]->forward_3d_gui_input(p_camera, p_event);
		if (current_after == EditorPlugin::AFTER_GUI_INPUT_STOP) {
			after = EditorPlugin::AFTER_GUI_INPUT_STOP;
		}
		if (after != EditorPlugin::AFTER_GUI_INPUT_STOP && current_after == EditorPlugin::AFTER_GUI_INPUT_CUSTOM) {
			after = EditorPlugin::AFTER_GUI_INPUT_CUSTOM;
		}
	}

	return after;
}

void EditorPluginList::forward_canvas_draw_over_viewport(Control *p_overlay) {
	for (int i = 0; i < plugins_list.size(); i++) {
		plugins_list[i]->forward_canvas_draw_over_viewport(p_overlay);
	}
}

void EditorPluginList::forward_canvas_force_draw_over_viewport(Control *p_overlay) {
	for (int i = 0; i < plugins_list.size(); i++) {
		plugins_list[i]->forward_canvas_force_draw_over_viewport(p_overlay);
	}
}

void EditorPluginList::forward_3d_draw_over_viewport(Control *p_overlay) {
	for (int i = 0; i < plugins_list.size(); i++) {
		plugins_list[i]->forward_3d_draw_over_viewport(p_overlay);
	}
}

void EditorPluginList::forward_3d_force_draw_over_viewport(Control *p_overlay) {
	for (int i = 0; i < plugins_list.size(); i++) {
		plugins_list[i]->forward_3d_force_draw_over_viewport(p_overlay);
	}
}

void EditorPluginList::add_plugin(EditorPlugin *p_plugin) {
	ERR_FAIL_COND(plugins_list.has(p_plugin));
	plugins_list.push_back(p_plugin);
}

void EditorPluginList::remove_plugin(EditorPlugin *p_plugin) {
	plugins_list.erase(p_plugin);
}

bool EditorPluginList::is_empty() {
	return plugins_list.is_empty();
}

void EditorPluginList::clear() {
	plugins_list.clear();
}

EditorPluginList::EditorPluginList() {
}

EditorPluginList::~EditorPluginList() {
}
