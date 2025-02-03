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

#include "core/bind/core_bind.h"
#include "core/class_db.h"
#include "core/io/config_file.h"
#include "core/io/image_loader.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/io/stream_peer_ssl.h"
#include "core/message_queue.h"
#include "core/os/file_access.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/path_remap.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/translation.h"
#include "core/version.h"
#include "main/input_default.h"
#include "main/main.h"
#include "scene/gui/center_container.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/link_button.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/tabs.h"
#include "scene/gui/texture_progress.h"
#include "scene/gui/tool_button.h"
#include "scene/resources/packed_scene.h"
#include "servers/navigation_2d_server.h"
#include "servers/navigation_server.h"
#include "servers/physics_2d_server.h"

#include "editor/audio_stream_preview.h"
#include "editor/dependency_editor.h"
#include "editor/editor_about.h"
#include "editor/editor_audio_buses.h"
#include "editor/editor_export.h"
#include "editor/editor_feature_profile.h"
#include "editor/editor_file_system.h"
#include "editor/editor_help.h"
#include "editor/editor_inspector.h"
#include "editor/editor_layouts_dialog.h"
#include "editor/editor_log.h"
#include "editor/editor_plugin.h"
#include "editor/editor_properties.h"
#include "editor/editor_property_name_processor.h"
#include "editor/editor_quick_open.h"
#include "editor/editor_resource_picker.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_run_native.h"
#include "editor/editor_run_script.h"
#include "editor/editor_scale.h"
#include "editor/editor_settings.h"
#include "editor/editor_spin_slider.h"
#include "editor/editor_themes.h"
#include "editor/export_template_manager.h"
#include "editor/fileserver/editor_file_server.h"
#include "editor/filesystem_dock.h"
#include "editor/import/editor_import_collada.h"
#include "editor/import/resource_importer_bitmask.h"
#include "editor/import/resource_importer_csv_translation.h"
#include "editor/import/resource_importer_image.h"
#include "editor/import/resource_importer_layered_texture.h"
#include "editor/import/resource_importer_obj.h"
#include "editor/import/resource_importer_scene.h"
#include "editor/import/resource_importer_texture.h"
#include "editor/import/resource_importer_texture_atlas.h"
#include "editor/import/resource_importer_wav.h"
#include "editor/import_dock.h"
#include "editor/multi_node_edit.h"
#include "editor/node_dock.h"
#include "editor/pane_drag.h"
#include "editor/plugin_config_dialog.h"
#include "editor/plugins/animation_blend_space_1d_editor.h"
#include "editor/plugins/animation_blend_space_2d_editor.h"
#include "editor/plugins/animation_blend_tree_editor_plugin.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "editor/plugins/animation_state_machine_editor.h"
#include "editor/plugins/animation_tree_editor_plugin.h"
#include "editor/plugins/animation_tree_player_editor_plugin.h"
#include "editor/plugins/asset_library_editor_plugin.h"
#include "editor/plugins/audio_stream_editor_plugin.h"
#include "editor/plugins/baked_lightmap_editor_plugin.h"
#include "editor/plugins/bit_map_editor_plugin.h"
#include "editor/plugins/camera_editor_plugin.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/plugins/collision_polygon_2d_editor_plugin.h"
#include "editor/plugins/collision_polygon_editor_plugin.h"
#include "editor/plugins/collision_shape_2d_editor_plugin.h"
#include "editor/plugins/cpu_particles_2d_editor_plugin.h"
#include "editor/plugins/cpu_particles_editor_plugin.h"
#include "editor/plugins/curve_editor_plugin.h"
#include "editor/plugins/editor_preview_plugins.h"
#include "editor/plugins/gi_probe_editor_plugin.h"
#include "editor/plugins/gradient_editor_plugin.h"
#include "editor/plugins/gradient_texture_2d_editor_plugin.h"
#include "editor/plugins/item_list_editor_plugin.h"
#include "editor/plugins/light_occluder_2d_editor_plugin.h"
#include "editor/plugins/line_2d_editor_plugin.h"
#include "editor/plugins/material_editor_plugin.h"
#include "editor/plugins/merge_group_editor_plugin.h"
#include "editor/plugins/mesh_editor_plugin.h"
#include "editor/plugins/mesh_instance_editor_plugin.h"
#include "editor/plugins/mesh_library_editor_plugin.h"
#include "editor/plugins/multimesh_editor_plugin.h"
#include "editor/plugins/navigation_polygon_editor_plugin.h"
#include "editor/plugins/packed_scene_editor_plugin.h"
#include "editor/plugins/particles_2d_editor_plugin.h"
#include "editor/plugins/particles_editor_plugin.h"
#include "editor/plugins/path_2d_editor_plugin.h"
#include "editor/plugins/path_editor_plugin.h"
#include "editor/plugins/physical_bone_plugin.h"
#include "editor/plugins/polygon_2d_editor_plugin.h"
#include "editor/plugins/ray_cast_2d_editor_plugin.h"
#include "editor/plugins/resource_preloader_editor_plugin.h"
#include "editor/plugins/room_manager_editor_plugin.h"
#include "editor/plugins/root_motion_editor_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/plugins/script_text_editor.h"
#include "editor/plugins/shader_editor_plugin.h"
#include "editor/plugins/skeleton_2d_editor_plugin.h"
#include "editor/plugins/skeleton_editor_plugin.h"
#include "editor/plugins/skeleton_ik_editor_plugin.h"
#include "editor/plugins/spatial_editor_plugin.h"
#include "editor/plugins/sprite_editor_plugin.h"
#include "editor/plugins/sprite_frames_editor_plugin.h"
#include "editor/plugins/style_box_editor_plugin.h"
#include "editor/plugins/text_editor.h"
#include "editor/plugins/texture_editor_plugin.h"
#include "editor/plugins/texture_region_editor_plugin.h"
#include "editor/plugins/theme_editor_plugin.h"
#include "editor/plugins/tile_map_editor_plugin.h"
#include "editor/plugins/tile_set_editor_plugin.h"
#include "editor/plugins/version_control_editor_plugin.h"
#include "editor/plugins/viewport_preview_editor_plugin.h"
#include "editor/plugins/visual_shader_editor_plugin.h"
#include "editor/progress_dialog.h"
#include "editor/project_export.h"
#include "editor/project_settings_editor.h"
#include "editor/register_exporters.h"
#include "editor/run_settings_dialog.h"
#include "editor/script_editor_debugger.h"
#include "editor/settings_config_dialog.h"

#include <stdio.h>
#include <stdlib.h>

EditorNode *EditorNode::singleton = nullptr;

// The metadata key used to store and retrieve the version text to copy to the clipboard.
static const String META_TEXT_TO_COPY = "text_to_copy";

void EditorNode::disambiguate_filenames(const Vector<String> p_full_paths, Vector<String> &r_filenames) {
	// Keep track of a list of "index sets," i.e. sets of indices
	// within disambiguated_scene_names which contain the same name.
	Vector<Set<int>> index_sets;
	Map<String, int> scene_name_to_set_index;
	for (int i = 0; i < r_filenames.size(); i++) {
		String scene_name = r_filenames[i];
		if (!scene_name_to_set_index.has(scene_name)) {
			index_sets.push_back(Set<int>());
			scene_name_to_set_index.insert(r_filenames[i], index_sets.size() - 1);
		}
		index_sets.write[scene_name_to_set_index[scene_name]].insert(i);
	}

	// For each index set with a size > 1, we need to disambiguate
	for (int i = 0; i < index_sets.size(); i++) {
		Set<int> iset = index_sets[i];
		while (iset.size() > 1) {
			// Append the parent folder to each scene name
			for (Set<int>::Element *E = iset.front(); E; E = E->next()) {
				int set_idx = E->get();
				String scene_name = r_filenames[set_idx];
				String full_path = p_full_paths[set_idx];

				// Get rid of file extensions and res:// prefixes
				if (scene_name.rfind(".") >= 0) {
					scene_name = scene_name.substr(0, scene_name.rfind("."));
				}
				if (full_path.begins_with("res://")) {
					full_path = full_path.substr(6);
				}
				if (full_path.rfind(".") >= 0) {
					full_path = full_path.substr(0, full_path.rfind("."));
				}

				int scene_name_size = scene_name.size();
				int full_path_size = full_path.size();
				int difference = full_path_size - scene_name_size;

				// Find just the parent folder of the current path and append it.
				// If the current name is foo.tscn, and the full path is /some/folder/foo.tscn
				// then slash_idx is the second '/', so that we select just "folder", and
				// append that to yield "folder/foo.tscn".
				if (difference > 0) {
					String parent = full_path.substr(0, difference);
					int slash_idx = parent.rfind("/");
					slash_idx = parent.rfind("/", slash_idx - 1);
					parent = slash_idx >= 0 ? parent.substr(slash_idx + 1) : parent;
					r_filenames.write[set_idx] = parent + r_filenames[set_idx];
				}
			}

			// Loop back through scene names and remove non-ambiguous names
			bool can_proceed = false;
			Set<int>::Element *E = iset.front();
			while (E) {
				String scene_name = r_filenames[E->get()];
				bool duplicate_found = false;
				for (Set<int>::Element *F = iset.front(); F; F = F->next()) {
					if (E->get() == F->get()) {
						continue;
					}
					String other_scene_name = r_filenames[F->get()];
					if (other_scene_name == scene_name) {
						duplicate_found = true;
						break;
					}
				}

				Set<int>::Element *to_erase = duplicate_found ? nullptr : E;

				// We need to check that we could actually append anymore names
				// if we wanted to for disambiguation. If we can't, then we have
				// to abort even with ambiguous names. We clean the full path
				// and the scene name first to remove extensions so that this
				// comparison actually works.
				String path = p_full_paths[E->get()];
				if (path.begins_with("res://")) {
					path = path.substr(6);
				}
				if (path.rfind(".") >= 0) {
					path = path.substr(0, path.rfind("."));
				}
				if (scene_name.rfind(".") >= 0) {
					scene_name = scene_name.substr(0, scene_name.rfind("."));
				}

				// We can proceed iff the full path is longer than the scene name,
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

void EditorNode::_update_scene_tabs() {
	bool show_rb = EditorSettings::get_singleton()->get("interface/scene_tabs/show_script_button");

	OS::get_singleton()->global_menu_clear("_dock");

	// Get all scene names, which may be ambiguous
	Vector<String> disambiguated_scene_names;
	Vector<String> full_path_names;
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		disambiguated_scene_names.push_back(editor_data.get_scene_title(i));
		full_path_names.push_back(editor_data.get_scene_path(i));
	}

	disambiguate_filenames(full_path_names, disambiguated_scene_names);

	scene_tabs->clear_tabs();
	Ref<Texture> script_icon = gui_base->get_icon("Script", "EditorIcons");
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Node *type_node = editor_data.get_edited_scene_root(i);
		Ref<Texture> icon;
		if (type_node) {
			icon = EditorNode::get_singleton()->get_object_icon(type_node, "Node");
		}

		int current = editor_data.get_edited_scene();
		bool unsaved = (i == current) ? saved_version != editor_data.get_undo_redo().get_version() : editor_data.get_scene_version(i) != 0;
		scene_tabs->add_tab(disambiguated_scene_names[i] + (unsaved ? "(*)" : ""), icon);

		OS::get_singleton()->global_menu_add_item("_dock", editor_data.get_scene_title(i) + (unsaved ? "(*)" : ""), GLOBAL_SCENE, i);

		if (show_rb && editor_data.get_scene_root_script(i).is_valid()) {
			scene_tabs->set_tab_right_button(i, script_icon);
		}
	}

	OS::get_singleton()->global_menu_add_separator("_dock");
	OS::get_singleton()->global_menu_add_item("_dock", TTR("New Window"), GLOBAL_NEW_WINDOW, Variant());

	scene_tabs->set_current_tab(editor_data.get_edited_scene());

	if (scene_tabs->get_offset_buttons_visible()) {
		// move add button to fixed position on the tabbar
		if (scene_tab_add->get_parent() == scene_tabs) {
			scene_tab_add->set_position(Point2(0, 0));
			scene_tabs->remove_child(scene_tab_add);
			tabbar_container->add_child(scene_tab_add);
			tabbar_container->move_child(scene_tab_add, 1);
		}
	} else {
		// move add button to after last tab
		if (scene_tab_add->get_parent() == tabbar_container) {
			tabbar_container->remove_child(scene_tab_add);
			scene_tabs->add_child(scene_tab_add);
		}
		Rect2 last_tab = Rect2();
		if (scene_tabs->get_tab_count() != 0) {
			last_tab = scene_tabs->get_tab_rect(scene_tabs->get_tab_count() - 1);
		}
		scene_tab_add->set_position(Point2(last_tab.get_position().x + last_tab.get_size().x + 3, last_tab.get_position().y));
	}
}

void EditorNode::_version_control_menu_option(int p_idx) {
	switch (vcs_actions_menu->get_item_id(p_idx)) {
		case RUN_VCS_SETTINGS: {
			VersionControlEditorPlugin::get_singleton()->popup_vcs_set_up_dialog(gui_base);
		} break;
		case RUN_VCS_SHUT_DOWN: {
			VersionControlEditorPlugin::get_singleton()->shut_down();
		} break;
	}
}

void EditorNode::_update_title() {
	const String appname = ProjectSettings::get_singleton()->get("application/config/name");
	String title = (appname.empty() ? TTR("Unnamed Project") : appname) + String(" - ") + VERSION_NAME;
	const String edited = editor_data.get_edited_scene_root() ? editor_data.get_edited_scene_root()->get_filename() : String();
	if (!edited.empty()) {
		// Display the edited scene name before the program name so that it can be seen in the OS task bar.
		title = vformat("%s - %s", edited.get_file(), title);
	}
	if (unsaved_cache) {
		// Display the "modified" mark before anything else so that it can always be seen in the OS task bar.
		title = vformat("(*) %s", title);
	}

	OS::get_singleton()->set_window_title(title);
}

void EditorNode::_unhandled_input(const Ref<InputEvent> &p_event) {
	if (Node::get_viewport()->get_modal_stack_top()) {
		return; //ignore because of modal window
	}

	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo() && !gui_base->get_viewport()->gui_has_modal_stack()) {
		EditorPlugin *old_editor = editor_plugin_screen;

		if (ED_IS_SHORTCUT("editor/next_tab", p_event)) {
			int next_tab = editor_data.get_edited_scene() + 1;
			next_tab %= editor_data.get_edited_scene_count();
			_scene_tab_changed(next_tab);
		}
		if (ED_IS_SHORTCUT("editor/prev_tab", p_event)) {
			int next_tab = editor_data.get_edited_scene() - 1;
			next_tab = next_tab >= 0 ? next_tab : editor_data.get_edited_scene_count() - 1;
			_scene_tab_changed(next_tab);
		}
		if (ED_IS_SHORTCUT("editor/filter_files", p_event)) {
			filesystem_dock->focus_on_filter();
		}

		if (ED_IS_SHORTCUT("editor/editor_2d", p_event)) {
			_editor_select(EDITOR_2D);
		} else if (ED_IS_SHORTCUT("editor/editor_3d", p_event)) {
			_editor_select(EDITOR_3D);
		} else if (ED_IS_SHORTCUT("editor/editor_script", p_event)) {
			_editor_select(EDITOR_SCRIPT);
		} else if (ED_IS_SHORTCUT("editor/editor_help", p_event)) {
			emit_signal("request_help_search", "");
		} else if (ED_IS_SHORTCUT("editor/editor_assetlib", p_event) && AssetLibraryEditorPlugin::is_available()) {
			_editor_select(EDITOR_ASSETLIB);
		} else if (ED_IS_SHORTCUT("editor/editor_next", p_event)) {
			_editor_select_next();
		} else if (ED_IS_SHORTCUT("editor/editor_prev", p_event)) {
			_editor_select_prev();
		}

		if (old_editor != editor_plugin_screen) {
			get_tree()->set_input_as_handled();
		}
	}
}

void EditorNode::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS: {
			if (opening_prev && !confirmation->is_visible()) {
				opening_prev = false;
			}

			if (unsaved_cache != (saved_version != editor_data.get_undo_redo().get_version())) {
				unsaved_cache = (saved_version != editor_data.get_undo_redo().get_version());
				_update_title();
			}

			if (last_checked_version != editor_data.get_undo_redo().get_version()) {
				_update_scene_tabs();
				last_checked_version = editor_data.get_undo_redo().get_version();
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
				if (EditorSettings::get_singleton()->get("interface/editor/show_update_spinner")) {
					update_spinner->set_icon(gui_base->get_icon("Progress" + itos(update_spinner_step + 1), "EditorIcons"));
				}
			}

			editor_selection->update();

			scene_root->set_size_override(true, Size2(ProjectSettings::get_singleton()->get("display/window/size/width"), ProjectSettings::get_singleton()->get("display/window/size/height")));

			ResourceImporterTexture::get_singleton()->update_imports();
		} break;

		case NOTIFICATION_ENTER_TREE: {
			Engine::get_singleton()->set_editor_hint(true);

			OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(int(EDITOR_GET("interface/editor/low_processor_mode_sleep_usec")));
			get_tree()->get_root()->set_usage(Viewport::USAGE_2D_NO_SAMPLING); //reduce memory usage
			get_tree()->get_root()->set_disable_3d(true);
			get_tree()->get_root()->set_as_audio_listener(false);
			get_tree()->get_root()->set_as_audio_listener_2d(false);
			get_tree()->set_auto_accept_quit(false);
#ifdef ANDROID_ENABLED
			get_tree()->set_quit_on_go_back(false);
#endif
			get_tree()->connect("files_dropped", this, "_dropped_files");
			get_tree()->connect("global_menu_action", this, "_global_menu_action");

			/* DO NOT LOAD SCENES HERE, WAIT FOR FILE SCANNING AND REIMPORT TO COMPLETE */
		} break;

		case NOTIFICATION_EXIT_TREE: {
			editor_data.save_editor_external_data();
			FileAccess::set_file_close_fail_notify_callback(nullptr);
			log->deinit(); // do not get messages anymore
			editor_data.clear_edited_scenes();
		} break;

		case NOTIFICATION_READY: {
			{
				_initializing_addons = true;
				Vector<String> addons;
				if (ProjectSettings::get_singleton()->has_setting("editor_plugins/enabled")) {
					addons = ProjectSettings::get_singleton()->get("editor_plugins/enabled");
				}

				for (int i = 0; i < addons.size(); i++) {
					set_addon_plugin_enabled(addons[i], true);
				}
				_initializing_addons = false;
			}

			VisualServer::get_singleton()->viewport_set_hide_scenario(get_scene_root()->get_viewport_rid(), true);
			VisualServer::get_singleton()->viewport_set_hide_canvas(get_scene_root()->get_viewport_rid(), true);
			VisualServer::get_singleton()->viewport_set_disable_environment(get_viewport()->get_viewport_rid(), true);

			feature_profile_manager->notify_changed();

			if (!main_editor_buttons[EDITOR_3D]->is_visible()) { //may be hidden due to feature profile
				_editor_select(EDITOR_2D);
			} else {
				_editor_select(EDITOR_3D);
			}

			_update_debug_options();

			// Save the project after opening to mark it as last modified, except in headless mode.
			if (OS::get_singleton()->can_draw() && !OS::get_singleton()->is_no_window_mode_enabled()) {
				ProjectSettings::get_singleton()->save();
			}

			/* DO NOT LOAD SCENES HERE, WAIT FOR FILE SCANNING AND REIMPORT TO COMPLETE */
		} break;

		case MainLoop::NOTIFICATION_WM_FOCUS_IN: {
			// Restore the original FPS cap after focusing back on the editor.
			OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(int(EDITOR_GET("interface/editor/low_processor_mode_sleep_usec")));

			EditorFileSystem::get_singleton()->scan_changes();
			_scan_external_changes();
		} break;

		case MainLoop::NOTIFICATION_WM_FOCUS_OUT: {
			// Save on focus loss before applying the FPS limit to avoid slowing down the saving process.
			if (EDITOR_GET("interface/editor/save_on_focus_loss")) {
				_menu_option_confirm(FILE_SAVE_SCENE, false);
			}

			// Set a low FPS cap to decrease CPU/GPU usage while the editor is unfocused.
			OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(int(EDITOR_GET("interface/editor/unfocused_low_processor_mode_sleep_usec")));
		} break;

		case MainLoop::NOTIFICATION_WM_ABOUT: {
			show_about();
		} break;

		case MainLoop::NOTIFICATION_WM_QUIT_REQUEST: {
			_menu_option_confirm(FILE_QUIT, false);
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			scene_tabs->set_tab_close_display_policy((bool(EDITOR_GET("interface/scene_tabs/always_show_close_button")) ? Tabs::CLOSE_BUTTON_SHOW_ALWAYS : Tabs::CLOSE_BUTTON_SHOW_ACTIVE_ONLY));
			FileDialog::set_default_show_hidden_files(EditorSettings::get_singleton()->get("filesystem/file_dialog/show_hidden_files"));
			EditorFileDialog::set_default_show_hidden_files(EditorSettings::get_singleton()->get("filesystem/file_dialog/show_hidden_files"));
			EditorFileDialog::set_default_display_mode((EditorFileDialog::DisplayMode)EditorSettings::get_singleton()->get("filesystem/file_dialog/display_mode").operator int());

			theme = create_custom_theme(theme_base->get_theme());

			theme_base->set_theme(theme);
			gui_base->set_theme(theme);

			gui_base->add_style_override("panel", gui_base->get_stylebox("Background", "EditorStyles"));
			scene_root_parent->add_style_override("panel", gui_base->get_stylebox("Content", "EditorStyles"));
			bottom_panel->add_style_override("panel", gui_base->get_stylebox("panel", "TabContainer"));
			scene_tabs->add_style_override("tab_fg", gui_base->get_stylebox("SceneTabFG", "EditorStyles"));
			scene_tabs->add_style_override("tab_bg", gui_base->get_stylebox("SceneTabBG", "EditorStyles"));

			file_menu->add_style_override("hover", gui_base->get_stylebox("MenuHover", "EditorStyles"));
			project_menu->add_style_override("hover", gui_base->get_stylebox("MenuHover", "EditorStyles"));
			debug_menu->add_style_override("hover", gui_base->get_stylebox("MenuHover", "EditorStyles"));
			settings_menu->add_style_override("hover", gui_base->get_stylebox("MenuHover", "EditorStyles"));
			help_menu->add_style_override("hover", gui_base->get_stylebox("MenuHover", "EditorStyles"));

			if (EDITOR_GET("interface/scene_tabs/resize_if_many_tabs")) {
				scene_tabs->set_min_width(int(EDITOR_GET("interface/scene_tabs/minimum_width")) * EDSCALE);
			} else {
				scene_tabs->set_min_width(0);
			}
			_update_scene_tabs();

			recent_scenes->set_as_minsize();

			// debugger area
			if (ScriptEditor::get_singleton()->get_debugger()->is_visible()) {
				bottom_panel->add_style_override("panel", gui_base->get_stylebox("BottomPanelDebuggerOverride", "EditorStyles"));
			}

			// update_icons
			for (int i = 0; i < singleton->main_editor_buttons.size(); i++) {
				ToolButton *tb = singleton->main_editor_buttons[i];
				EditorPlugin *p_editor = singleton->editor_table[i];
				Ref<Texture> icon = p_editor->get_icon();

				if (icon.is_valid()) {
					tb->set_icon(icon);
				} else if (singleton->gui_base->has_icon(p_editor->get_name(), "EditorIcons")) {
					tb->set_icon(singleton->gui_base->get_icon(p_editor->get_name(), "EditorIcons"));
				}
			}

			_build_icon_type_cache();

			play_button->set_icon(gui_base->get_icon("MainPlay", "EditorIcons"));
			play_scene_button->set_icon(gui_base->get_icon("PlayScene", "EditorIcons"));
			play_custom_scene_button->set_icon(gui_base->get_icon("PlayCustom", "EditorIcons"));
			pause_button->set_icon(gui_base->get_icon("Pause", "EditorIcons"));
			stop_button->set_icon(gui_base->get_icon("Stop", "EditorIcons"));

			prev_scene->set_icon(gui_base->get_icon("PrevScene", "EditorIcons"));
			distraction_free->set_icon(gui_base->get_icon("DistractionFree", "EditorIcons"));
			scene_tab_add->set_icon(gui_base->get_icon("Add", "EditorIcons"));

			bottom_panel_raise->set_icon(gui_base->get_icon("ExpandBottomDock", "EditorIcons"));

			// clear_button->set_icon(gui_base->get_icon("Close", "EditorIcons")); don't have access to that node. needs to become a class property
			dock_tab_move_left->set_icon(theme->get_icon("Back", "EditorIcons"));
			dock_tab_move_right->set_icon(theme->get_icon("Forward", "EditorIcons"));

			PopupMenu *p = help_menu->get_popup();
			p->set_item_icon(p->get_item_index(HELP_SEARCH), gui_base->get_icon("HelpSearch", "EditorIcons"));
			p->set_item_icon(p->get_item_index(HELP_DOCS), gui_base->get_icon("ExternalLink", "EditorIcons"));
			p->set_item_icon(p->get_item_index(HELP_QA), gui_base->get_icon("ExternalLink", "EditorIcons"));
			p->set_item_icon(p->get_item_index(HELP_REPORT_A_BUG), gui_base->get_icon("ExternalLink", "EditorIcons"));
			p->set_item_icon(p->get_item_index(HELP_SUGGEST_A_FEATURE), gui_base->get_icon("ExternalLink", "EditorIcons"));
			p->set_item_icon(p->get_item_index(HELP_SEND_DOCS_FEEDBACK), gui_base->get_icon("ExternalLink", "EditorIcons"));
			p->set_item_icon(p->get_item_index(HELP_COMMUNITY), gui_base->get_icon("ExternalLink", "EditorIcons"));
			p->set_item_icon(p->get_item_index(HELP_ABOUT), gui_base->get_icon("Godot", "EditorIcons"));
			p->set_item_icon(p->get_item_index(HELP_SUPPORT_GODOT_DEVELOPMENT), gui_base->get_icon("Heart", "EditorIcons"));
			_update_update_spinner();
		} break;

		case Control::NOTIFICATION_RESIZED: {
			_update_scene_tabs();
		} break;
	}
}

void EditorNode::_update_update_spinner() {
	update_spinner->set_visible(EditorSettings::get_singleton()->get("interface/editor/show_update_spinner"));

	const bool update_continuously = EditorSettings::get_singleton()->get("interface/editor/update_continuously");
	const bool vital_only = EditorSettings::get_singleton()->get("interface/editor/update_vital_only");
	PopupMenu *update_popup = update_spinner->get_popup();
	update_popup->set_item_checked(update_popup->get_item_index(SETTINGS_UPDATE_CONTINUOUSLY), update_continuously);

	if (update_continuously) {
		update_popup->set_item_checked(update_popup->get_item_index(SETTINGS_UPDATE_WHEN_CHANGED), false);
		update_popup->set_item_checked(update_popup->get_item_index(SETTINGS_UPDATE_VITAL_ONLY), false);
	} else {
		update_popup->set_item_checked(update_popup->get_item_index(SETTINGS_UPDATE_WHEN_CHANGED), !vital_only);
		update_popup->set_item_checked(update_popup->get_item_index(SETTINGS_UPDATE_VITAL_ONLY), vital_only);
	}

	if (update_continuously) {
		update_spinner->set_tooltip(TTR("Spins when the editor window redraws.\nUpdate Continuously is enabled, which can increase power usage. Click to disable it."));

		// Use a different color for the update spinner when Update Continuously is enabled,
		// as this feature should only be enabled for troubleshooting purposes.
		// Make the icon modulate color overbright because icons are not completely white on a dark theme.
		// On a light theme, icons are dark, so we need to modulate them with an even brighter color.
		const bool dark_theme = EditorSettings::get_singleton()->is_dark_theme();
		update_spinner->set_self_modulate(
				gui_base->get_color("error_color", "Editor") * (dark_theme ? Color(1.1, 1.1, 1.1) : Color(4.25, 4.25, 4.25)));
	} else {
		update_spinner->set_tooltip(TTR("Spins when the editor window redraws."));
		update_spinner->set_self_modulate(Color(1, 1, 1));
	}

	OS::get_singleton()->set_low_processor_usage_mode(!update_continuously);

	// Only set low priority redraws to false in the editor.
	// When we run the project in the editor, we don't want it to prevent
	// rendering any frames.
	OS::get_singleton()->set_update_vital_only(vital_only && !update_continuously);
}

void EditorNode::_on_plugin_ready(Object *p_script, const String &p_activate_name) {
	Ref<Script> script = Object::cast_to<Script>(p_script);
	if (script.is_null()) {
		return;
	}
	if (p_activate_name.length()) {
		set_addon_plugin_enabled(p_activate_name, true);
	}
	project_settings->update_plugins();
	project_settings->hide();
	push_item(script.operator->());
}

void EditorNode::_remove_plugin_from_enabled(const String &p_name) {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	PoolStringArray enabled_plugins = ps->get("editor_plugins/enabled");
	for (int i = 0; i < enabled_plugins.size(); ++i) {
		if (enabled_plugins.get(i) == p_name) {
			enabled_plugins.remove(i);
			break;
		}
	}
	ps->set("editor_plugins/enabled", enabled_plugins);
}

void EditorNode::_resources_changed(const PoolVector<String> &p_resources) {
	List<Ref<Resource>> changed;

	int rc = p_resources.size();
	for (int i = 0; i < rc; i++) {
		Ref<Resource> res(ResourceCache::get(p_resources.get(i)));
		if (res.is_null()) {
			continue;
		}

		if (!res->editor_can_reload_from_file()) {
			continue;
		}
		if (!res->get_path().is_resource_file() && !res->get_path().is_abs_path()) {
			continue;
		}
		if (!FileAccess::exists(res->get_path())) {
			continue;
		}

		if (res->get_import_path() != String()) {
			//this is an imported resource, will be reloaded if reimported via the _resources_reimported() callback
			continue;
		}

		changed.push_back(res);
	}

	if (changed.size()) {
		for (List<Ref<Resource>>::Element *E = changed.front(); E; E = E->next()) {
			E->get()->reload_from_file();
		}
	}
}

void EditorNode::_fs_changed() {
	for (Set<FileDialog *>::Element *E = file_dialogs.front(); E; E = E->next()) {
		E->get()->invalidate();
	}

	for (Set<EditorFileDialog *>::Element *E = editor_file_dialogs.front(); E; E = E->next()) {
		E->get()->invalidate();
	}

	_mark_unsaved_scenes();

	// FIXME: Move this to a cleaner location, it's hacky to do this is _fs_changed.
	String export_error;
	Error err = OK;
	if (export_defer.preset != "" && !EditorFileSystem::get_singleton()->is_scanning()) {
		String preset_name = export_defer.preset;
		// Ensures export_project does not loop infinitely, because notifications may
		// come during the export.
		export_defer.preset = "";
		Ref<EditorExportPreset> preset;
		for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); ++i) {
			preset = EditorExport::get_singleton()->get_export_preset(i);
			if (preset->get_name() == preset_name) {
				break;
			}
			preset.unref();
		}

		if (preset.is_null()) {
			DirAccessRef da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
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
			Ref<EditorExportPlatform> platform = preset->get_platform();
			const String export_path = export_defer.path.empty() ? preset->get_export_path() : export_defer.path;
			if (export_path.empty()) {
				err = FAILED;
				export_error = vformat("Export preset \"%s\" doesn't have a default export path, and none was specified.", preset_name);
			} else if (platform.is_null()) {
				err = FAILED;
				export_error = vformat("Export preset \"%s\" doesn't have a matching platform.", preset_name);
			} else {
				if (export_defer.pack_only) { // Only export .pck or .zip data pack.
					if (export_path.ends_with(".zip")) {
						err = platform->export_zip(preset, export_defer.debug, export_path);
					} else if (export_path.ends_with(".pck")) {
						err = platform->export_pack(preset, export_defer.debug, export_path);
					}
				} else { // Normal project export.
					String config_error;
					bool missing_templates;
					if (!platform->can_export(preset, config_error, missing_templates)) {
						ERR_PRINT(vformat("Cannot export project with preset \"%s\" due to configuration errors:\n%s", preset_name, config_error));
						err = missing_templates ? ERR_FILE_NOT_FOUND : ERR_UNCONFIGURED;
					} else {
						platform->clear_messages();
						err = platform->export_project(preset, export_defer.debug, export_path);
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
			OS::get_singleton()->set_exit_code(EXIT_FAILURE);
		} else if (!export_error.empty()) {
			WARN_PRINT(export_error);
		}
		_exit_editor();
	}
}

void EditorNode::_resources_reimported(const Vector<String> &p_resources) {
	List<String> scenes; //will load later
	int current_tab = scene_tabs->get_current_tab();

	for (int i = 0; i < p_resources.size(); i++) {
		String file_type = ResourceLoader::get_resource_type(p_resources[i]);
		if (file_type == "PackedScene") {
			scenes.push_back(p_resources[i]);
			//reload later if needed, first go with normal resources
			continue;
		}

		if (!ResourceCache::has(p_resources[i])) {
			continue; //not loaded, no need to reload
		}
		//reload normally
		Resource *resource = ResourceCache::get(p_resources[i]);
		if (resource) {
			resource->reload_from_file();
		}
	}

	for (List<String>::Element *E = scenes.front(); E; E = E->next()) {
		reload_scene(E->get());
	}

	scene_tabs->set_current_tab(current_tab);
}

void EditorNode::_sources_changed(bool p_exist) {
	if (waiting_for_first_scan) {
		waiting_for_first_scan = false;

		// Start preview thread now that it's safe.
		if (!singleton->cmdline_export_mode) {
			EditorResourcePreview::get_singleton()->start();
		}

		_load_docks();

		if (defer_load_scene != "") {
			OS::get_singleton()->benchmark_begin_measure("editor_load_scene");
			load_scene(defer_load_scene);
			defer_load_scene = "";
			OS::get_singleton()->benchmark_end_measure("editor_load_scene");
			OS::get_singleton()->benchmark_dump();
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
		DirAccessRef da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
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

	String project_settings_path = ProjectSettings::get_singleton()->get_resource_path().plus_file("project.godot");
	if (FileAccess::get_modified_time(project_settings_path) > ProjectSettings::get_singleton()->get_last_saved_time()) {
		TreeItem *ti = disk_changed_list->create_item(r);
		ti->set_text(0, "project.godot");
		need_reload = true;
	}

	if (need_reload) {
		disk_changed->call_deferred("popup_centered_ratio", 0.5);
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

			Error err = load_scene(filename, false, false, true, false, true);
			if (err != OK) {
				ERR_PRINT(vformat("Failed to load scene: %s", filename));
			}
			editor_data.move_edited_scene_to_index(i);
		}
	}

	get_undo_redo()->clear_history(false);
	set_current_scene(current_idx);
	_update_scene_tabs();
	disk_changed->hide();
}

void EditorNode::_reload_project_settings() {
	ProjectSettings::get_singleton()->setup(ProjectSettings::get_singleton()->get_resource_path(), String(), true);
}

void EditorNode::_vp_resized() {
}

void EditorNode::_version_button_pressed() {
	OS::get_singleton()->set_clipboard(version_btn->get_meta(META_TEXT_TO_COPY));
}

void EditorNode::_node_renamed() {
	if (get_inspector()) {
		get_inspector()->update_tree();
	}
}

void EditorNode::_editor_select_next() {
	int editor = _get_current_main_editor();

	do {
		if (editor == editor_table.size() - 1) {
			editor = 0;
		} else {
			editor++;
		}
	} while (!main_editor_buttons[editor]->is_visible());

	_editor_select(editor);
}

void EditorNode::_editor_select_prev() {
	int editor = _get_current_main_editor();

	do {
		if (editor == 0) {
			editor = editor_table.size() - 1;
		} else {
			editor--;
		}
	} while (!main_editor_buttons[editor]->is_visible());

	_editor_select(editor);
}

Error EditorNode::load_resource(const String &p_resource, bool p_ignore_broken_deps) {
	dependency_errors.clear();

	Error err;
	RES res = ResourceLoader::load(p_resource, "", false, &err);
	ERR_FAIL_COND_V(!res.is_valid(), ERR_CANT_OPEN);

	if (!p_ignore_broken_deps && dependency_errors.has(p_resource)) {
		//current_option = -1;
		Vector<String> errors;
		for (Set<String>::Element *E = dependency_errors[p_resource].front(); E; E = E->next()) {
			errors.push_back(E->get());
		}
		dependency_error->show(DependencyErrorDialog::MODE_RESOURCE, p_resource, errors);
		dependency_errors.erase(p_resource);

		return ERR_FILE_MISSING_DEPENDENCIES;
	}

	inspector_dock->edit_resource(res);
	return OK;
}

void EditorNode::edit_node(Node *p_node) {
	push_item(p_node);
}

void EditorNode::save_resource_in_path(const Ref<Resource> &p_resource, const String &p_path) {
	editor_data.apply_changes_in_editors();
	int flg = 0;
	if (EditorSettings::get_singleton()->get("filesystem/on_save/compress_binary_resources")) {
		flg |= ResourceSaver::FLAG_COMPRESS;
	}

	String path = ProjectSettings::get_singleton()->localize_path(p_path);
	Error err = ResourceSaver::save(path, p_resource, flg | ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS);

	if (err != OK) {
		if (ResourceLoader::is_imported(p_resource->get_path())) {
			show_accept(TTR("Imported resources can't be saved."), TTR("OK"));
		} else {
			show_accept(TTR("Error saving resource!"), TTR("OK"));
		}
		return;
	}

	((Resource *)p_resource.ptr())->set_path(path);
	emit_signal("resource_saved", p_resource);
	editor_data.notify_resource_saved(p_resource);
}

void EditorNode::save_resource(const Ref<Resource> &p_resource) {
	if (p_resource->get_path().is_resource_file()) {
		save_resource_in_path(p_resource, p_resource->get_path());
	} else {
		save_resource_as(p_resource);
	}
}

void EditorNode::save_resource_as(const Ref<Resource> &p_resource, const String &p_at_path) {
	{
		String path = p_resource->get_path();
		int srpos = path.find("::");
		if (srpos != -1) {
			String base = path.substr(0, srpos);
			if (!get_edited_scene() || get_edited_scene()->get_filename() != base) {
				show_warning(TTR("This resource can't be saved because it does not belong to the edited scene. Make it unique first."));
				return;
			}
		}
	}

	file->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	saving_resource = p_resource;

	current_option = RESOURCE_SAVE_AS;
	List<String> extensions;
	Ref<PackedScene> sd = memnew(PackedScene);
	ResourceSaver::get_recognized_extensions(p_resource, &extensions);
	file->clear_filters();

	List<String> preferred;
	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		if (p_resource->is_class("Script") && (E->get() == "tres" || E->get() == "res")) {
			//this serves no purpose and confused people
			continue;
		}
		file->add_filter("*." + E->get() + " ; " + E->get().to_upper());
		preferred.push_back(E->get());
	}
	// Lowest priority extension
	List<String>::Element *res_element = preferred.find("res");
	if (res_element) {
		preferred.move_to_back(res_element);
	}
	// Highest priority extension
	List<String>::Element *tres_element = preferred.find("tres");
	if (tres_element) {
		preferred.move_to_front(tres_element);
	}

	if (p_at_path != String()) {
		file->set_current_dir(p_at_path);
		if (p_resource->get_path().is_resource_file()) {
			file->set_current_file(p_resource->get_path().get_file());
		} else {
			if (extensions.size()) {
				file->set_current_file("new_" + p_resource->get_class().to_lower() + "." + preferred.front()->get().to_lower());
			} else {
				file->set_current_file(String());
			}
		}
	} else if (p_resource->get_path() != "") {
		file->set_current_path(p_resource->get_path());
		if (extensions.size()) {
			String ext = p_resource->get_path().get_extension().to_lower();
			if (extensions.find(ext) == nullptr) {
				file->set_current_path(p_resource->get_path().replacen("." + ext, "." + extensions.front()->get()));
			}
		}
	} else if (preferred.size()) {
		String existing;
		if (extensions.size()) {
			existing = "new_" + p_resource->get_class().to_lower() + "." + preferred.front()->get().to_lower();
		}
		file->set_current_path(existing);
	}
	file->popup_centered_ratio();
	file->set_title(TTR("Save Resource As..."));
}

void EditorNode::_menu_option(int p_option) {
	_menu_option_confirm(p_option, false);
}

void EditorNode::_menu_confirm_current() {
	_menu_option_confirm(current_option, true);
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
				show_accept(vformat(TTR("Can't open '%s'. The file could have been moved or deleted."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_PARSE_ERROR: {
				show_accept(vformat(TTR("Error while parsing '%s'."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_CORRUPT: {
				show_accept(vformat(TTR("Unexpected end of file '%s'."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_NOT_FOUND: {
				show_accept(vformat(TTR("Missing '%s' or its dependencies."), p_file.get_file()), TTR("OK"));
			} break;
			default: {
				show_accept(vformat(TTR("Error while loading '%s'."), p_file.get_file()), TTR("OK"));
			} break;
		}
	}
}

void EditorNode::_get_scene_metadata(const String &p_file) {
	Node *scene = editor_data.get_edited_scene_root();

	if (!scene) {
		return;
	}

	String path = EditorSettings::get_singleton()->get_project_settings_dir().plus_file(p_file.get_file() + "-editstate-" + p_file.md5_text() + ".cfg");

	Ref<ConfigFile> cf;
	cf.instance();

	Error err = cf->load(path);
	if (err != OK || !cf->has_section("editor_states")) {
		return; //must not exist
	}

	List<String> esl;
	cf->get_section_keys("editor_states", &esl);

	Dictionary md;
	for (List<String>::Element *E = esl.front(); E; E = E->next()) {
		Variant st = cf->get_value("editor_states", E->get());
		if (st.get_type() != Variant::NIL) {
			md[E->get()] = st;
		}
	}

	editor_data.set_editor_states(md);
}

void EditorNode::_set_scene_metadata(const String &p_file, int p_idx) {
	Node *scene = editor_data.get_edited_scene_root(p_idx);

	if (!scene) {
		return;
	}

	scene->set_meta("__editor_run_settings__", Variant()); //clear it (no point in keeping it)
	scene->set_meta("__editor_plugin_states__", Variant());

	String path = EditorSettings::get_singleton()->get_project_settings_dir().plus_file(p_file.get_file() + "-editstate-" + p_file.md5_text() + ".cfg");

	Ref<ConfigFile> cf;
	cf.instance();

	Dictionary md;

	if (p_idx < 0 || editor_data.get_edited_scene() == p_idx) {
		md = editor_data.get_editor_states();
	} else {
		md = editor_data.get_scene_editor_states(p_idx);
	}

	List<Variant> keys;
	md.get_key_list(&keys);

	for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
		cf->set_value("editor_states", E->get(), md[E->get()]);
	}

	Error err = cf->save(path);
	ERR_FAIL_COND_MSG(err != OK, "Cannot save config file to '" + path + "'.");
}

bool EditorNode::_find_and_save_resource(RES p_res, Map<RES, bool> &processed, int32_t flags) {
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
			//save
			ResourceSaver::save(p_res->get_path(), p_res, flags);
		}
		processed[p_res] = false; //because it's a file
		return false;
	} else {
		processed[p_res] = changed;
		return changed;
	}
}

bool EditorNode::_find_and_save_edited_subresources(Object *obj, Map<RES, bool> &processed, int32_t flags) {
	bool ret_changed = false;
	List<PropertyInfo> pi;
	obj->get_property_list(&pi);
	for (List<PropertyInfo>::Element *E = pi.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		switch (E->get().type) {
			case Variant::OBJECT: {
				RES res = obj->get(E->get().name);

				if (_find_and_save_resource(res, processed, flags)) {
					ret_changed = true;
				}

			} break;
			case Variant::ARRAY: {
				Array varray = obj->get(E->get().name);
				int len = varray.size();
				for (int i = 0; i < len; i++) {
					const Variant &v = varray.get(i);
					RES res = v;
					if (_find_and_save_resource(res, processed, flags)) {
						ret_changed = true;
					}
				}

			} break;
			case Variant::DICTIONARY: {
				Dictionary d = obj->get(E->get().name);
				List<Variant> keys;
				d.get_key_list(&keys);
				for (List<Variant>::Element *F = keys.front(); F; F = F->next()) {
					Variant v = d[F->get()];
					RES res = v;
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

void EditorNode::_save_edited_subresources(Node *scene, Map<RES, bool> &processed, int32_t flags) {
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
	} else if (p_node->is_class("Spatial")) {
		count_3d++;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_find_node_types(p_node->get_child(i), count_2d, count_3d);
	}
}

void EditorNode::_save_scene_with_preview(String p_file, int p_idx) {
	EditorProgress save("save", TTR("Saving Scene"), 4);

	Ref<World> edited_world;

	if (editor_data.get_edited_scene_root() != nullptr) {
		// Allow a generic mechanism for the engine to make changes prior, and after saving.
		if (editor_data.get_edited_scene_root()->get_tree() && editor_data.get_edited_scene_root()->get_tree()->get_root()) {
			edited_world = editor_data.get_edited_scene_root()->get_tree()->get_root()->get_world();
			if (edited_world.is_valid()) {
				edited_world->notify_saving(true);
			}
		}

		save.step(TTR("Analyzing"), 0);

		int c2d = 0;
		int c3d = 0;

		_find_node_types(editor_data.get_edited_scene_root(), c2d, c3d);

		save.step(TTR("Creating Thumbnail"), 1);
		//current view?

		Ref<Image> img;
		// If neither 3D or 2D nodes are present, make a 1x1 black texture.
		// We cannot fallback on the 2D editor, because it may not have been used yet,
		// which would result in an invalid texture.
		if (c3d == 0 && c2d == 0) {
			img.instance();
			img->create(1, 1, false, Image::FORMAT_RGB8);
		} else if (c3d < c2d) {
			Ref<ViewportTexture> viewport_texture = scene_root->get_texture();
			if (viewport_texture->get_width() > 0 && viewport_texture->get_height() > 0) {
				img = viewport_texture->get_data();
			}
		} else {
			// The 3D editor may be disabled as a feature, but scenes can still be opened.
			// This check prevents the preview from regenerating in case those scenes are then saved.
			// The preview will be generated if no feature profile is set (as the 3D editor is enabled by default).
			Ref<EditorFeatureProfile> profile = feature_profile_manager->get_current_profile();
			if (!profile.is_valid() || !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_3D)) {
				img = SpatialEditor::get_singleton()->get_editor_viewport(0)->get_viewport_node()->get_texture()->get_data();
			}
		}

		if (img.is_valid() && img->get_width() > 0 && img->get_height() > 0) {
			img = img->duplicate();

			save.step(TTR("Creating Thumbnail"), 2);
			save.step(TTR("Creating Thumbnail"), 3);

			int preview_size = EditorSettings::get_singleton()->get("filesystem/file_dialog/thumbnail_size");
			preview_size *= EDSCALE;

			// consider a square region
			int vp_size = MIN(img->get_width(), img->get_height());
			int x = (img->get_width() - vp_size) / 2;
			int y = (img->get_height() - vp_size) / 2;

			if (vp_size < preview_size) {
				// just square it.
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

			img->flip_y();

			//save thumbnail directly, as thumbnailer may not update due to actual scene not changing md5
			String temp_path = EditorSettings::get_singleton()->get_cache_dir();
			String cache_base = ProjectSettings::get_singleton()->globalize_path(p_file).md5_text();
			cache_base = temp_path.plus_file("resthumb-" + cache_base);

			//does not have it, try to load a cached thumbnail

			String file = cache_base + ".png";

			post_process_preview(img);
			img->save_png(file);
		}
	}

	save.step(TTR("Saving Scene"), 4);
	_save_scene(p_file, p_idx);

	if (edited_world.is_valid()) {
		edited_world->notify_saving(false);
	}

	if (!singleton->cmdline_export_mode) {
		EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);
	}
}

bool EditorNode::_validate_scene_recursive(const String &p_filename, Node *p_node) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		if (child->get_filename() == p_filename) {
			return true;
		}

		if (_validate_scene_recursive(p_filename, child)) {
			return true;
		}
	}

	return false;
}

int EditorNode::_save_external_resources() {
	//save external resources and its subresources if any was modified

	int flg = 0;
	if (EditorSettings::get_singleton()->get("filesystem/on_save/compress_binary_resources")) {
		flg |= ResourceSaver::FLAG_COMPRESS;
	}
	flg |= ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;

	Set<String> edited_resources;
	int saved = 0;
	List<Ref<Resource>> cached;
	ResourceCache::get_cached_resources(&cached);
	for (List<Ref<Resource>>::Element *E = cached.front(); E; E = E->next()) {
		Ref<Resource> res = E->get();
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

	for (Set<String>::Element *E = edited_resources.front(); E; E = E->next()) {
		Ref<Resource> res = Ref<Resource>(ResourceCache::get(E->get()));
		if (!res.is_valid()) {
			continue; // Maybe it was erased in a thread, who knows.
		}
		Ref<PackedScene> ps = res;
		if (ps.is_valid()) {
			continue; // Do not save PackedScenes, this will mess up the editor.
		}
		ResourceSaver::save(res->get_path(), res, flg);
		saved++;
	}

	return saved;
}

static void _reset_animation_players(Node *p_node, List<Ref<AnimatedValuesBackup>> *r_anim_backups) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		AnimationPlayer *player = Object::cast_to<AnimationPlayer>(p_node->get_child(i));
		if (player && player->is_reset_on_save_enabled() && player->can_apply_reset()) {
			Ref<AnimatedValuesBackup> old_values = player->apply_reset();
			if (old_values.is_valid()) {
				r_anim_backups->push_back(old_values);
			}
		}
		_reset_animation_players(p_node->get_child(i), r_anim_backups);
	}
}

void EditorNode::_save_scene(String p_file, int idx) {
	Node *scene = editor_data.get_edited_scene_root(idx);

	if (!scene) {
		show_accept(TTR("This operation can't be done without a tree root."), TTR("OK"));
		return;
	}

	if (scene->get_filename() != String() && _validate_scene_recursive(scene->get_filename(), scene)) {
		show_accept(TTR("This scene can't be saved because there is a cyclic instancing inclusion.\nPlease resolve it and then attempt to save again."), TTR("OK"));
		return;
	}

	editor_data.apply_changes_in_editors();
	List<Ref<AnimatedValuesBackup>> anim_backups;
	_reset_animation_players(scene, &anim_backups);
	_save_default_environment();

	_set_scene_metadata(p_file, idx);

	Ref<PackedScene> sdata;

	if (ResourceCache::has(p_file)) {
		// something may be referencing this resource and we are good with that.
		// we must update it, but also let the previous scene state go, as
		// old version still work for referencing changes in instanced or inherited scenes

		sdata = Ref<PackedScene>(Object::cast_to<PackedScene>(ResourceCache::get(p_file)));
		if (sdata.is_valid()) {
			sdata->recreate_state();
		} else {
			sdata.instance();
		}
	} else {
		sdata.instance();
	}
	Error err = sdata->pack(scene);

	if (err != OK) {
		show_accept(TTR("Couldn't save scene. Likely dependencies (instances or inheritance) couldn't be satisfied."), TTR("OK"));
		return;
	}

	int flg = 0;
	if (EditorSettings::get_singleton()->get("filesystem/on_save/compress_binary_resources")) {
		flg |= ResourceSaver::FLAG_COMPRESS;
	}
	flg |= ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;

	err = ResourceSaver::save(p_file, sdata, flg);

	// This needs to be emitted before saving external resources.
	emit_signal("scene_saved", p_file);
	editor_data.notify_scene_saved(p_file);

	_save_external_resources();
	editor_data.save_editor_external_data();

	for (List<Ref<AnimatedValuesBackup>>::Element *E = anim_backups.front(); E; E = E->next()) {
		E->get()->restore();
	}

	if (err == OK) {
		scene->set_filename(ProjectSettings::get_singleton()->localize_path(p_file));
		if (idx < 0 || idx == editor_data.get_edited_scene()) {
			set_current_version(editor_data.get_undo_redo().get_version());
		} else {
			editor_data.set_edited_scene_version(0, idx);
		}
		editor_data.set_scene_modified_time(idx, FileAccess::get_modified_time(p_file));

		editor_folding.save_scene_folding(scene, p_file);

		_update_title();
		_update_scene_tabs();
	} else {
		_dialog_display_save_error(p_file, err);
	}
}

void EditorNode::save_all_scenes() {
	_menu_option_confirm(RUN_STOP, true);
	_save_all_scenes();
}

void EditorNode::save_scene_list(Vector<String> p_scene_filenames) {
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Node *scene = editor_data.get_edited_scene_root(i);

		if (scene && (p_scene_filenames.find(scene->get_filename()) >= 0)) {
			_save_scene(scene->get_filename(), i);
		}
	}
}

void EditorNode::restart_editor() {
	exiting = true;

	if (editor_run.get_status() != EditorRun::STATUS_STOP) {
		editor_run.stop();
	}

	String to_reopen;
	if (get_tree()->get_edited_scene_root()) {
		to_reopen = get_tree()->get_edited_scene_root()->get_filename();
	}

	_exit_editor();

	List<String> args;

	const Vector<String> &forwardable_args = Main::get_forwardable_cli_arguments(Main::CLI_SCOPE_TOOL);
	for (int i = 0; i < forwardable_args.size(); i++) {
		args.push_back(forwardable_args[i]);
	}

	args.push_back("--path");
	args.push_back(ProjectSettings::get_singleton()->get_resource_path());

	args.push_back("-e");

	if (to_reopen != String()) {
		args.push_back(to_reopen);
	}

	OS::get_singleton()->set_restart_on_exit(true, args);
}

void EditorNode::_save_all_scenes() {
	bool all_saved = true;
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Node *scene = editor_data.get_edited_scene_root(i);
		if (scene) {
			if (scene->get_filename() != "" && DirAccess::exists(scene->get_filename().get_base_dir())) {
				if (i != editor_data.get_edited_scene()) {
					_save_scene(scene->get_filename(), i);
				} else {
					_save_scene_with_preview(scene->get_filename());
				}
			} else if (scene->get_filename() != "") {
				all_saved = false;
			}
		}
	}

	if (!all_saved) {
		show_warning(TTR("Could not save one or more scenes!"), TTR("Save All Scenes"));
	}
	_save_default_environment();
}

void EditorNode::_mark_unsaved_scenes() {
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		Node *node = editor_data.get_edited_scene_root(i);
		if (!node) {
			continue;
		}

		String path = node->get_filename();
		if (!(path == String() || FileAccess::exists(path))) {
			if (i == editor_data.get_edited_scene()) {
				set_current_version(-1);
			} else {
				editor_data.set_edited_scene_version(-1, i);
			}
		}
	}

	_update_title();
	_update_scene_tabs();
}

void EditorNode::_dialog_action(String p_file) {
	switch (current_option) {
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
			ProjectSettings::get_singleton()->set("application/run/main_scene", p_file);
			ProjectSettings::get_singleton()->save();
			//would be nice to show the project manager opened with the highlighted field..

			if (pick_main_scene->has_meta("from_native") && (bool)pick_main_scene->get_meta("from_native")) {
				run_native->resume_run_native();
			} else {
				_run(false, ""); // automatically run the project
			}
		} break;
		case FILE_CLOSE:
		case FILE_CLOSE_ALL_AND_QUIT:
		case FILE_CLOSE_ALL_AND_RUN_PROJECT_MANAGER:
		case FILE_CLOSE_ALL_AND_RELOAD_CURRENT_PROJECT:
		case SCENE_TAB_CLOSE:
		case FILE_SAVE_SCENE:
		case FILE_SAVE_AS_SCENE: {
			int scene_idx = (current_option == FILE_SAVE_SCENE || current_option == FILE_SAVE_AS_SCENE) ? -1 : tab_closing;

			if (file->get_mode() == EditorFileDialog::MODE_SAVE_FILE) {
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

				_save_default_environment();
				_save_scene_with_preview(p_file, scene_idx);
				_add_to_recent_scenes(p_file);
				save_layout();

				if (scene_idx != -1) {
					_discard_changes();
				}
			}

		} break;

		case FILE_SAVE_AND_RUN: {
			if (file->get_mode() == EditorFileDialog::MODE_SAVE_FILE) {
				_save_default_environment();
				_save_scene_with_preview(p_file);
				_run(false, p_file);
			}
		} break;

		case FILE_EXPORT_MESH_LIBRARY: {
			Ref<MeshLibrary> ml;
			if (file_export_lib_merge->is_pressed() && FileAccess::exists(p_file)) {
				ml = ResourceLoader::load(p_file, "MeshLibrary");

				if (ml.is_null()) {
					show_accept(TTR("Can't load MeshLibrary for merging!"), TTR("OK"));
					return;
				}
			}

			if (ml.is_null()) {
				ml = Ref<MeshLibrary>(memnew(MeshLibrary));
			}

			MeshLibraryEditor::update_library_file(editor_data.get_edited_scene_root(), ml, true, file_export_lib_apply_xforms->is_pressed());

			Error err = ResourceSaver::save(p_file, ml);
			if (err) {
				show_accept(TTR("Error saving MeshLibrary!"), TTR("OK"));
				return;
			}

		} break;
		case FILE_EXPORT_TILESET: {
			Ref<TileSet> tileset;
			if (FileAccess::exists(p_file) && file_export_lib_merge->is_pressed()) {
				tileset = ResourceLoader::load(p_file, "TileSet");

				if (tileset.is_null()) {
					show_accept(TTR("Can't load TileSet for merging!"), TTR("OK"));
					return;
				}

			} else {
				tileset = Ref<TileSet>(memnew(TileSet));
			}

			TileSetEditor::update_library_file(editor_data.get_edited_scene_root(), tileset, true);

			Error err = ResourceSaver::save(p_file, tileset);
			if (err) {
				show_accept(TTR("Error saving TileSet!"), TTR("OK"));
				return;
			}
		} break;

		case RESOURCE_SAVE:
		case RESOURCE_SAVE_AS: {
			ERR_FAIL_COND(saving_resource.is_null());
			if (p_file.get_file().begins_with(".")) {
				show_accept(TTR("Could not use a name with a leading dot."), TTR("OK"));
				return;
			}

			save_resource_in_path(saving_resource, p_file);
			saving_resource = Ref<Resource>();
			ObjectID current = editor_history.get_current();
			Object *current_obj = current > 0 ? ObjectDB::get_instance(current) : nullptr;
			ERR_FAIL_COND(!current_obj);
			current_obj->_change_notify();
		} break;
		case SETTINGS_LAYOUT_SAVE: {
			if (p_file.empty()) {
				return;
			}

			Ref<ConfigFile> config;
			config.instance();
			Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());

			if (err == ERR_FILE_CANT_OPEN || err == ERR_FILE_NOT_FOUND) {
				config.instance(); // new config
			} else if (err != OK) {
				show_warning(TTR("An error occurred while trying to save the editor layout.\nMake sure the editor's user data path is writable."));
				return;
			}

			_save_docks_to_config(config, p_file);

			config->save(EditorSettings::get_singleton()->get_editor_layouts_config());

			layout_dialog->hide();
			_update_layouts_menu();

			if (p_file == "Default") {
				show_warning(TTR("Default editor layout overridden.\nTo restore the Default layout to its base settings, use the Delete Layout option and delete the Default layout."));
			}

		} break;
		case SETTINGS_LAYOUT_DELETE: {
			if (p_file.empty()) {
				return;
			}

			Ref<ConfigFile> config;
			config.instance();
			Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());

			if (err != OK || !config->has_section(p_file)) {
				show_warning(TTR("Layout name not found!"));
				return;
			}

			// erase
			List<String> keys;
			config->get_section_keys(p_file, &keys);
			for (List<String>::Element *E = keys.front(); E; E = E->next()) {
				config->set_value(p_file, E->get(), Variant());
			}

			config->save(EditorSettings::get_singleton()->get_editor_layouts_config());

			layout_dialog->hide();
			_update_layouts_menu();

			if (p_file == "Default") {
				show_warning(TTR("Restored the Default layout to its base settings."));
			}

		} break;
		default: { //save scene?

			if (file->get_mode() == EditorFileDialog::MODE_SAVE_FILE) {
				_save_scene_with_preview(p_file);
			}

		} break;
	}
}

bool EditorNode::item_has_editor(Object *p_object) {
	if (_is_class_editor_disabled_by_feature_profile(p_object->get_class())) {
		return false;
	}

	return editor_data.get_subeditors(p_object).size() > 0;
}

void EditorNode::edit_item_resource(RES p_resource) {
	edit_item(p_resource.ptr());
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

void EditorNode::edit_item(Object *p_object) {
	Vector<EditorPlugin *> sub_plugins;

	if (p_object) {
		if (_is_class_editor_disabled_by_feature_profile(p_object->get_class())) {
			return;
		}
		sub_plugins = editor_data.get_subeditors(p_object);
	}

	if (!sub_plugins.empty()) {
		bool same = true;
		if (sub_plugins.size() == editor_plugins_over->get_plugins_list().size()) {
			for (int i = 0; i < sub_plugins.size(); i++) {
				if (sub_plugins[i] != editor_plugins_over->get_plugins_list()[i]) {
					same = false;
				}
			}
		} else {
			same = false;
		}
		if (!same) {
			_display_top_editors(false);
			_set_top_editors(sub_plugins);
		}
		_set_editing_top_editors(p_object);
		_display_top_editors(true);
	} else {
		hide_top_editors();
	}
}

void EditorNode::push_item(Object *p_object, const String &p_property, bool p_inspector_only) {
	if (!p_object) {
		get_inspector()->edit(nullptr);
		node_dock->set_node(nullptr);
		scene_tree_dock->set_selected(nullptr);
		inspector_dock->update(nullptr);
		_display_top_editors(false);
		return;
	}

	uint32_t id = p_object->get_instance_id();
	if (id != editor_history.get_current()) {
		if (p_inspector_only) {
			editor_history.add_object_inspector_only(id);
		} else if (p_property == "") {
			editor_history.add_object(id);
		} else {
			editor_history.add_object(id, p_property);
		}
	}

	_edit_current();
}

void EditorNode::_save_default_environment() {
	Ref<Environment> fallback = get_tree()->get_root()->get_world()->get_fallback_environment();

	if (fallback.is_valid() && fallback->get_path().is_resource_file()) {
		Map<RES, bool> processed;
		_find_and_save_edited_subresources(fallback.ptr(), processed, 0);
		save_resource_in_path(fallback, fallback->get_path());
	}
}

void EditorNode::hide_top_editors() {
	_display_top_editors(false);

	editor_plugins_over->clear();
}

void EditorNode::_display_top_editors(bool p_display) {
	editor_plugins_over->make_visible(p_display);
}

void EditorNode::_set_top_editors(Vector<EditorPlugin *> p_editor_plugins_over) {
	editor_plugins_over->set_plugins_list(p_editor_plugins_over);
}

void EditorNode::_set_editing_top_editors(Object *p_current_object) {
	editor_plugins_over->edit(p_current_object);
}

static bool overrides_external_editor(Object *p_object) {
	Script *script = Object::cast_to<Script>(p_object);

	if (!script) {
		return false;
	}

	return script->get_language()->overrides_external_editor();
}

void EditorNode::_edit_current(bool p_skip_foreign) {
	uint32_t current = editor_history.get_current();
	Object *current_obj = current > 0 ? ObjectDB::get_instance(current) : nullptr;

	RES res = Object::cast_to<Resource>(current_obj);
	if (p_skip_foreign && res.is_valid()) {
		if (res->get_path().find("::") > -1 && res->get_path().get_slice("::", 0) != editor_data.get_scene_path(get_current_tab())) {
			// Trying to edit resource that belongs to another scene; abort.
			current_obj = nullptr;
		}
	}

	bool inspector_only = editor_history.is_current_inspector_only();
	this->current = current_obj;

	if (!current_obj) {
		scene_tree_dock->set_selected(nullptr);
		get_inspector()->edit(nullptr);
		node_dock->set_node(nullptr);
		inspector_dock->update(nullptr);

		_display_top_editors(false);

		return;
	}

	Object *prev_inspected_object = get_inspector()->get_edited_object();

	bool disable_folding = bool(EDITOR_GET("interface/inspector/disable_folding"));
	bool is_resource = current_obj->is_class("Resource");
	bool is_node = current_obj->is_class("Node");
	bool stay_in_script_editor_on_node_selected = bool(EDITOR_GET("text_editor/navigation/stay_in_script_editor_on_node_selected"));
	bool skip_main_plugin = false;

	String editable_warning; //none by default

	if (is_resource) {
		Resource *current_res = Object::cast_to<Resource>(current_obj);
		ERR_FAIL_COND(!current_res);
		get_inspector()->edit(current_res);
		scene_tree_dock->set_selected(nullptr);
		node_dock->set_node(nullptr);
		inspector_dock->update(nullptr);
		EditorNode::get_singleton()->get_import_dock()->set_edit_path(current_res->get_path());

		int subr_idx = current_res->get_path().find("::");
		if (subr_idx != -1) {
			String base_path = current_res->get_path().substr(0, subr_idx);
			if (FileAccess::exists(base_path + ".import")) {
				editable_warning = TTR("This resource belongs to a scene that was imported, so it's not editable.\nPlease read the documentation relevant to importing scenes to better understand this workflow.");
			} else {
				if ((!get_edited_scene() || get_edited_scene()->get_filename() != base_path) && ResourceLoader::get_resource_type(base_path) == "PackedScene") {
					editable_warning = TTR("This resource belongs to a scene that was instanced or inherited.\nChanges to it won't be kept when saving the current scene.");
				}
			}
		} else if (current_res->get_path().is_resource_file()) {
			if (FileAccess::exists(current_res->get_path() + ".import")) {
				editable_warning = TTR("This resource was imported, so it's not editable. Change its settings in the import panel and then re-import.");
			}
		}
	} else if (is_node) {
		Node *current_node = Object::cast_to<Node>(current_obj);
		ERR_FAIL_COND(!current_node);

		get_inspector()->edit(current_node);
		if (current_node->is_inside_tree()) {
			node_dock->set_node(current_node);
			scene_tree_dock->set_selected(current_node);
			inspector_dock->update(current_node);
			if (!inspector_only && !skip_main_plugin) {
				skip_main_plugin = stay_in_script_editor_on_node_selected && ScriptEditor::get_singleton()->is_visible_in_tree();
			}
		} else {
			node_dock->set_node(nullptr);
			scene_tree_dock->set_selected(nullptr);
			inspector_dock->update(nullptr);
		}

		if (get_edited_scene() && get_edited_scene()->get_filename() != String()) {
			String source_scene = get_edited_scene()->get_filename();
			if (FileAccess::exists(source_scene + ".import")) {
				editable_warning = TTR("This scene was imported, so changes to it won't be kept.\nInstancing it or inheriting will allow making changes to it.\nPlease read the documentation relevant to importing scenes to better understand this workflow.");
			}
		}

	} else {
		Node *selected_node = nullptr;

		if (current_obj->is_class("ScriptEditorDebuggerInspectedObject")) {
			editable_warning = TTR("This is a remote object, so changes to it won't be kept.\nPlease read the documentation relevant to debugging to better understand this workflow.");
			disable_folding = true;
		} else if (current_obj->is_class("MultiNodeEdit")) {
			Node *scene = get_edited_scene();
			if (scene) {
				MultiNodeEdit *multi_node_edit = Object::cast_to<MultiNodeEdit>(current_obj);
				int node_count = multi_node_edit->get_node_count();
				if (node_count > 0) {
					List<Node *> multi_nodes;
					for (int node_index = 0; node_index < node_count; ++node_index) {
						Node *node = scene->get_node(multi_node_edit->get_node(node_index));
						if (node) {
							multi_nodes.push_back(node);
						}
					}
					if (!multi_nodes.empty()) {
						// Pick the top-most node
						multi_nodes.sort_custom<Node::Comparator>();
						selected_node = multi_nodes.front()->get();
					}
				}
			}
		}

		get_inspector()->edit(current_obj);
		node_dock->set_node(nullptr);
		scene_tree_dock->set_selected(selected_node);
		inspector_dock->update(nullptr);
	}

	if (current_obj == prev_inspected_object) {
		// Make sure inspected properties are restored.
		get_inspector()->update_tree();
	}

	inspector_dock->set_warning(editable_warning);

	if (get_inspector()->is_using_folding() == disable_folding) {
		get_inspector()->set_use_folding(!disable_folding);
	}

	/* Take care of PLUGIN EDITOR */

	if (!inspector_only) {
		EditorPlugin *main_plugin = editor_data.get_editor(current_obj);

		int plugin_index = 0;
		for (; plugin_index < editor_table.size(); plugin_index++) {
			if (editor_table[plugin_index] == main_plugin) {
				if (!main_editor_buttons[plugin_index]->is_visible()) {
					main_plugin = nullptr; //if button is not visible, then no plugin active
				}

				break;
			}
		}

		if (main_plugin && !skip_main_plugin) {
			// special case if use of external editor is true
			Resource *current_res = Object::cast_to<Resource>(current_obj);
			if (main_plugin->get_name() == "Script" && current_obj->get_class_name() != StringName("VisualScript") && current_res && !current_res->get_path().empty() && current_res->get_path().find("::") == -1 && (bool(EditorSettings::get_singleton()->get("text_editor/external/use_external_editor")) || overrides_external_editor(current_obj))) {
				if (!changing_scene) {
					main_plugin->edit(current_obj);
				}
			}

			else if (main_plugin != editor_plugin_screen && (!ScriptEditor::get_singleton() || !ScriptEditor::get_singleton()->is_visible_in_tree() || ScriptEditor::get_singleton()->can_take_away_focus())) {
				// update screen main_plugin
				_editor_select(plugin_index);
				main_plugin->edit(current_obj);
			} else {
				editor_plugin_screen->edit(current_obj);
			}
		}

		Vector<EditorPlugin *> sub_plugins;

		if (!_is_class_editor_disabled_by_feature_profile(current_obj->get_class())) {
			sub_plugins = editor_data.get_subeditors(current_obj);
		}

		if (!sub_plugins.empty()) {
			_display_top_editors(false);

			_set_top_editors(sub_plugins);
			_set_editing_top_editors(current_obj);
			_display_top_editors(true);
		} else if (!editor_plugins_over->get_plugins_list().empty()) {
			hide_top_editors();
		}
	}

	inspector_dock->update(current_obj);
	inspector_dock->update_keying();
}

void EditorNode::_run(bool p_current, const String &p_custom) {
	if (editor_run.get_status() == EditorRun::STATUS_PLAY) {
		play_button->set_pressed(!_playing_edited);
		play_scene_button->set_pressed(_playing_edited);
		return;
	}

	play_button->set_pressed(false);
	play_button->set_icon(gui_base->get_icon("MainPlay", "EditorIcons"));
	play_scene_button->set_pressed(false);
	play_scene_button->set_icon(gui_base->get_icon("PlayScene", "EditorIcons"));
	play_custom_scene_button->set_pressed(false);
	play_custom_scene_button->set_icon(gui_base->get_icon("PlayCustom", "EditorIcons"));

	String run_filename;
	String args;
	bool skip_breakpoints;

	if (p_current || (editor_data.get_edited_scene_root() && p_custom != String() && p_custom == editor_data.get_edited_scene_root()->get_filename())) {
		Node *scene = editor_data.get_edited_scene_root();

		if (!scene) {
			show_accept(TTR("There is no defined scene to run."), TTR("OK"));
			return;
		}

		if (scene->get_filename() == "") {
			current_option = -1;
			_menu_option(FILE_SAVE_AS_SCENE);
			// Set the option to save and run so when the dialog is accepted, the scene runs.
			current_option = FILE_SAVE_AND_RUN;
			file->set_title(TTR("Save scene before running..."));
			return;
		}

		run_filename = scene->get_filename();
	} else if (p_custom != "") {
		run_filename = p_custom;
	}

	if (run_filename == "") {
		//evidently, run the scene
		if (!ensure_main_scene(false)) {
			return;
		}
	}

	if (bool(EDITOR_GET("run/auto_save/save_before_running"))) {
		if (unsaved_cache) {
			Node *scene = editor_data.get_edited_scene_root();

			if (scene && scene->get_filename() != "") { // Only autosave if there is a scene and if it has a path.
				_save_scene_with_preview(scene->get_filename());
			}
		}
		_menu_option(FILE_SAVE_ALL_SCENES);
		editor_data.save_editor_external_data();
	}

	if (!call_build()) {
		return;
	}

	if (bool(EDITOR_GET("run/output/always_clear_output_on_play"))) {
		log->clear();
	}

	if (bool(EDITOR_GET("run/output/always_open_output_on_play"))) {
		make_bottom_panel_item_visible(log);
	}

	List<String> breakpoints;
	editor_data.get_editor_breakpoints(&breakpoints);

	args = ProjectSettings::get_singleton()->get("editor/main_run_args");
	skip_breakpoints = ScriptEditor::get_singleton()->get_debugger()->is_skip_breakpoints();
	emit_signal("play_pressed");

	Error error = editor_run.run(run_filename, args, breakpoints, skip_breakpoints);

	if (error != OK) {
		emit_signal("stop_pressed");
		show_accept(TTR("Could not start subprocess!"), TTR("OK"));
		return;
	}

	if (p_current) {
		play_scene_button->set_pressed(true);
		play_scene_button->set_icon(gui_base->get_icon("Reload", "EditorIcons"));
	} else if (p_custom != "") {
		run_custom_filename = p_custom;
		play_custom_scene_button->set_pressed(true);
		play_custom_scene_button->set_icon(gui_base->get_icon("Reload", "EditorIcons"));
	} else {
		play_button->set_pressed(true);
		play_button->set_icon(gui_base->get_icon("Reload", "EditorIcons"));
	}
	stop_button->set_disabled(false);

	_playing_edited = p_current;
}

void EditorNode::_android_build_source_selected(const String &p_file) {
	export_template_manager->install_android_template_from_file(p_file);
}

void EditorNode::_menu_option_confirm(int p_option, bool p_confirmed) {
	if (!p_confirmed) { //this may be a hack..
		current_option = (MenuOptions)p_option;
	}

	switch (p_option) {
		case FILE_NEW_SCENE: {
			new_scene();

		} break;
		case FILE_NEW_INHERITED_SCENE:
		case FILE_OPEN_SCENE: {
			file->set_mode(EditorFileDialog::MODE_OPEN_FILE);
			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);
			file->clear_filters();
			for (int i = 0; i < extensions.size(); i++) {
				file->add_filter("*." + extensions[i] + " ; " + extensions[i].to_upper());
			}

			Node *scene = editor_data.get_edited_scene_root();
			if (scene) {
				file->set_current_path(scene->get_filename());
			};
			file->set_title(p_option == FILE_OPEN_SCENE ? TTR("Open Scene") : TTR("Open Base Scene"));
			file->popup_centered_ratio();

		} break;
		case FILE_QUICK_OPEN: {
			quick_open->popup_dialog("Resource", true);
			quick_open->set_title(TTR("Quick Open..."));

		} break;
		case FILE_QUICK_OPEN_SCENE: {
			quick_open->popup_dialog("PackedScene", true);
			quick_open->set_title(TTR("Quick Open Scene..."));

		} break;
		case FILE_QUICK_OPEN_SCRIPT: {
			quick_open->popup_dialog("Script", true);
			quick_open->set_title(TTR("Quick Open Script..."));

		} break;
		case FILE_OPEN_PREV: {
			if (previous_scenes.empty()) {
				break;
			}
			opening_prev = true;
			open_request(previous_scenes.back()->get());
			previous_scenes.pop_back();

		} break;
		case FILE_CLOSE_OTHERS:
		case FILE_CLOSE_RIGHT:
		case FILE_CLOSE_ALL: {
			if (editor_data.get_edited_scene_count() > 1 && (current_option != FILE_CLOSE_RIGHT || editor_data.get_edited_scene() < editor_data.get_edited_scene_count() - 1)) {
				int next_tab = editor_data.get_edited_scene() + 1;
				next_tab %= editor_data.get_edited_scene_count();
				_scene_tab_closed(next_tab, current_option);
			} else {
				if (current_option != FILE_CLOSE_ALL) {
					current_option = -1;
				} else {
					_scene_tab_closed(editor_data.get_edited_scene());
				}
			}

			if (p_confirmed) {
				_menu_option_confirm(SCENE_TAB_CLOSE, true);
			}

		} break;
		case FILE_CLOSE_ALL_AND_QUIT:
		case FILE_CLOSE_ALL_AND_RUN_PROJECT_MANAGER:
		case FILE_CLOSE_ALL_AND_RELOAD_CURRENT_PROJECT:
		case FILE_CLOSE: {
			if (!p_confirmed) {
				tab_closing = p_option == FILE_CLOSE ? editor_data.get_edited_scene() : _next_unsaved_scene(false);
				_scene_tab_changed(tab_closing);

				if (unsaved_cache || p_option == FILE_CLOSE_ALL_AND_QUIT || p_option == FILE_CLOSE_ALL_AND_RUN_PROJECT_MANAGER || p_option == FILE_CLOSE_ALL_AND_RELOAD_CURRENT_PROJECT) {
					Node *scene_root = editor_data.get_edited_scene_root(tab_closing);
					if (scene_root) {
						String scene_filename = scene_root->get_filename();
						if (p_option == FILE_CLOSE_ALL_AND_RELOAD_CURRENT_PROJECT) {
							save_confirmation->get_ok()->set_text(TTR("Save & Reload"));
							save_confirmation->set_text(vformat(TTR("Save changes to '%s' before reloading?"), scene_filename != "" ? scene_filename : "unsaved scene"));
						} else {
							save_confirmation->get_ok()->set_text(TTR("Save & Close"));
							save_confirmation->set_text(vformat(TTR("Save changes to '%s' before closing?"), scene_filename != "" ? scene_filename : "unsaved scene"));
						}
						save_confirmation->popup_centered_minsize();
						break;
					}
				}
			} else if (p_option == FILE_CLOSE) {
				tab_closing = editor_data.get_edited_scene();
			}
			if (!editor_data.get_edited_scene_root(tab_closing)) {
				// empty tab
				_scene_tab_closed(tab_closing);
				break;
			}

			FALLTHROUGH;
		}
		case SCENE_TAB_CLOSE:
		case FILE_SAVE_SCENE: {
			int scene_idx = (p_option == FILE_SAVE_SCENE) ? -1 : tab_closing;
			Node *scene = editor_data.get_edited_scene_root(scene_idx);
			if (scene && scene->get_filename() != "") {
				if (DirAccess::exists(scene->get_filename().get_base_dir())) {
					if (scene_idx != editor_data.get_edited_scene()) {
						_save_scene_with_preview(scene->get_filename(), scene_idx);
					} else {
						_save_scene_with_preview(scene->get_filename());
					}

					if (scene_idx != -1) {
						_discard_changes();
					}
					save_layout();
				} else {
					show_save_accept(vformat(TTR("%s no longer exists! Please specify a new save location."), scene->get_filename().get_base_dir()), TTR("OK"));
				}
				break;
			}
			FALLTHROUGH;
		}
		case FILE_SAVE_AS_SCENE: {
			int scene_idx = (p_option == FILE_SAVE_SCENE || p_option == FILE_SAVE_AS_SCENE) ? -1 : tab_closing;

			Node *scene = editor_data.get_edited_scene_root(scene_idx);

			if (!scene) {
				if (p_option == FILE_SAVE_SCENE) {
					// Pressing Ctrl + S saves the current script if a scene is currently open, but it won't if the scene has no root node.
					// Work around this by explicitly saving the script in this case (similar to pressing Ctrl + Alt + S).
					ScriptEditor::get_singleton()->save_current_script();
				}

				const int saved = _save_external_resources();
				if (saved > 0) {
					show_accept(
							vformat(TTR("The current scene has no root node, but %d modified external resource(s) were saved anyway."), saved),
							TTR("OK"));
				} else if (p_option == FILE_SAVE_AS_SCENE) {
					// Don't show this dialog when pressing Ctrl + S to avoid interfering with script saving.
					show_accept(
							TTR("A root node is required to save the scene. You can add a root node using the Scene tree dock."),
							TTR("OK"));
				}

				break;
			}

			file->set_mode(EditorFileDialog::MODE_SAVE_FILE);

			List<String> extensions;
			Ref<PackedScene> sd = memnew(PackedScene);
			ResourceSaver::get_recognized_extensions(sd, &extensions);
			file->clear_filters();
			for (int i = 0; i < extensions.size(); i++) {
				file->add_filter("*." + extensions[i] + " ; " + extensions[i].to_upper());
			}

			if (scene->get_filename() != "") {
				String path = scene->get_filename();
				file->set_current_path(path);
				if (extensions.size()) {
					String ext = path.get_extension().to_lower();
					if (extensions.find(ext) == nullptr) {
						file->set_current_path(path.replacen("." + ext, "." + extensions.front()->get()));
					}
				}
			} else if (extensions.size()) {
				String root_name = scene->get_name();
				// Very similar to node naming logic.
				switch (ProjectSettings::get_singleton()->get("editor/scene_naming").operator int()) {
					case SCENE_NAME_CASING_AUTO:
						// Use casing of the root node.
						break;
					case SCENE_NAME_CASING_PASCAL_CASE: {
						root_name = root_name.capitalize().replace(" ", "");
					} break;
					case SCENE_NAME_CASING_SNAKE_CASE:
						root_name = root_name.capitalize().replace(" ", "").replace("-", "_").camelcase_to_underscore();
						break;
				}
				file->set_current_path(root_name + "." + extensions.front()->get().to_lower());
			}
			file->popup_centered_ratio();
			file->set_title(TTR("Save Scene As..."));

		} break;

		case FILE_SAVE_ALL_SCENES: {
			_save_all_scenes();
		} break;

		case FILE_EXPORT_PROJECT: {
			project_export->popup_export();
		} break;

		case FILE_EXPORT_MESH_LIBRARY: {
			if (!editor_data.get_edited_scene_root()) {
				show_accept(TTR("This operation can't be done without a scene."), TTR("OK"));
				break;
			}

			List<String> extensions;
			Ref<MeshLibrary> ml(memnew(MeshLibrary));
			ResourceSaver::get_recognized_extensions(ml, &extensions);
			file_export_lib->clear_filters();
			for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
				file_export_lib->add_filter("*." + E->get());
			}

			file_export_lib->popup_centered_ratio();
			file_export_lib->set_title(TTR("Export Mesh Library"));

		} break;
		case FILE_EXPORT_TILESET: {
			//Make sure that the scene has a root before trying to convert to tileset
			if (!editor_data.get_edited_scene_root()) {
				show_accept(TTR("This operation can't be done without a root node."), TTR("OK"));
				break;
			}

			List<String> extensions;
			Ref<TileSet> ml(memnew(TileSet));
			ResourceSaver::get_recognized_extensions(ml, &extensions);
			file_export_lib->clear_filters();
			for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
				file_export_lib->add_filter("*." + E->get());
			}

			file_export_lib->popup_centered_ratio();
			file_export_lib->set_title(TTR("Export Tile Set"));

		} break;

		case FILE_IMPORT_SUBSCENE: {
			if (!editor_data.get_edited_scene_root()) {
				show_accept(TTR("This operation can't be done without a selected node."), TTR("OK"));
				break;
			}

			scene_tree_dock->import_subscene();

		} break;

		case FILE_EXTERNAL_OPEN_SCENE: {
			if (unsaved_cache && !p_confirmed) {
				confirmation->get_ok()->set_text(TTR("Open"));
				confirmation->set_text(TTR("Current scene not saved. Open anyway?"));
				confirmation->popup_centered_minsize();
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
			if (Input::get_singleton()->get_mouse_button_mask() & 0x7) {
				log->add_message(TTR("Can't undo while mouse buttons are pressed."), EditorLog::MSG_TYPE_EDITOR);
			} else {
				String action = editor_data.get_undo_redo().get_current_action_name();

				if (!editor_data.get_undo_redo().undo()) {
					log->add_message(TTR("Nothing to undo."), EditorLog::MSG_TYPE_EDITOR);
				} else if (action != "") {
					log->add_message(vformat(TTR("Undo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
				}
			}
		} break;
		case EDIT_REDO: {
			if (Input::get_singleton()->get_mouse_button_mask() & 0x7) {
				log->add_message(TTR("Can't redo while mouse buttons are pressed."), EditorLog::MSG_TYPE_EDITOR);
			} else {
				if (!editor_data.get_undo_redo().redo()) {
					log->add_message(TTR("Nothing to redo."), EditorLog::MSG_TYPE_EDITOR);
				} else {
					String action = editor_data.get_undo_redo().get_current_action_name();
					log->add_message(vformat(TTR("Redo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
				}
			}
		} break;

		case EDIT_RELOAD_SAVED_SCENE: {
			Node *scene = get_edited_scene();

			if (!scene) {
				break;
			}

			String filename = scene->get_filename();

			if (filename == String()) {
				show_warning(TTR("Can't reload a scene that was never saved."));
				break;
			}

			if (unsaved_cache && !p_confirmed) {
				confirmation->get_ok()->set_text(TTR("Reload Saved Scene"));
				confirmation->set_text(
						TTR("The current scene has unsaved changes.\nReload the saved scene anyway? This action cannot be undone."));
				confirmation->popup_centered_minsize();
				break;
			}

			int cur_idx = editor_data.get_edited_scene();
			_remove_edited_scene();
			Error err = load_scene(filename);
			if (err != OK)
				ERR_PRINT("Failed to load scene");
			editor_data.move_edited_scene_to_index(cur_idx);
			get_undo_redo()->clear_history(false);
			scene_tabs->set_current_tab(cur_idx);

		} break;
		case RUN_PLAY: {
			run_play();

		} break;
		case RUN_PLAY_CUSTOM_SCENE: {
			if (run_custom_filename.empty() || editor_run.get_status() == EditorRun::STATUS_STOP) {
				_menu_option_confirm(RUN_STOP, true);
				quick_run->popup_dialog("PackedScene", true);
				quick_run->set_title(TTR("Quick Run Scene..."));
				play_custom_scene_button->set_pressed(false);
			} else {
				String last_custom_scene = run_custom_filename;
				run_play_custom(last_custom_scene);
			}

		} break;
		case RUN_STOP: {
			if (editor_run.get_status() == EditorRun::STATUS_STOP) {
				break;
			}

			editor_run.stop();
			run_custom_filename.clear();
			play_button->set_pressed(false);
			play_button->set_icon(gui_base->get_icon("MainPlay", "EditorIcons"));
			play_scene_button->set_pressed(false);
			play_scene_button->set_icon(gui_base->get_icon("PlayScene", "EditorIcons"));
			play_custom_scene_button->set_pressed(false);
			play_custom_scene_button->set_icon(gui_base->get_icon("PlayCustom", "EditorIcons"));
			stop_button->set_disabled(true);

			if (bool(EDITOR_GET("run/output/always_close_output_on_stop"))) {
				for (int i = 0; i < bottom_panel_items.size(); i++) {
					if (bottom_panel_items[i].control == log) {
						_bottom_panel_switch(false, i);
						break;
					}
				}
			}
			emit_signal("stop_pressed");

		} break;

		case FILE_SHOW_IN_FILESYSTEM: {
			String path = editor_data.get_scene_path(editor_data.get_edited_scene());
			if (path != String()) {
				filesystem_dock->navigate_to_path(path);
			}
		} break;

		case RUN_PLAY_SCENE: {
			run_play_current();

		} break;
		case RUN_PLAY_NATIVE: {
			bool autosave = EDITOR_GET("run/auto_save/save_before_running");
			if (autosave) {
				_menu_option_confirm(FILE_SAVE_ALL_SCENES, false);
			}
			if (run_native->is_deploy_debug_remote_enabled()) {
				_menu_option_confirm(RUN_STOP, true);

				if (!call_build()) {
					break; // build failed
				}

				emit_signal("play_pressed");
				editor_run.run_native_notify();
			}
		} break;
		case RUN_SCENE_SETTINGS: {
			run_settings_dialog->popup_run_settings();
		} break;
		case RUN_SETTINGS: {
			project_settings->popup_project_settings();
		} break;
		case FILE_INSTALL_ANDROID_SOURCE: {
			if (p_confirmed) {
				export_template_manager->install_android_template();
			} else {
				if (DirAccess::exists("res://android/build")) {
					remove_android_build_template->popup_centered_minsize();
				} else if (export_template_manager->can_install_android_template()) {
					install_android_build_template->popup_centered_minsize();
				} else {
					custom_build_manage_templates->popup_centered_minsize();
				}
			}
		} break;
		case RUN_USER_DATA_FOLDER: {
			// ensure_user_data_dir() to prevent the edge case: "Open User Data Folder" won't work after the project was renamed in ProjectSettingsEditor unless the project is saved
			OS::get_singleton()->ensure_user_data_dir();
			OS::get_singleton()->shell_open(String("file://") + OS::get_singleton()->get_user_data_dir());
		} break;
		case FILE_EXPLORE_ANDROID_BUILD_TEMPLATES: {
			OS::get_singleton()->shell_open("file://" + ProjectSettings::get_singleton()->get_resource_path().plus_file("android"));
		} break;
		case FILE_QUIT:
		case RUN_PROJECT_MANAGER:
		case RELOAD_CURRENT_PROJECT: {
			if (!p_confirmed) {
				bool save_each = EDITOR_GET("interface/editor/save_each_scene_on_quit");
				if (_next_unsaved_scene(!save_each) == -1) {
					bool confirm = (p_option != RELOAD_CURRENT_PROJECT) && EDITOR_GET("interface/editor/quit_confirmation");
					if (confirm) {
						confirmation->get_ok()->set_text(p_option == FILE_QUIT ? TTR("Quit") : TTR("Yes"));
						confirmation->set_text(p_option == FILE_QUIT ? TTR("Exit the editor?") : TTR("Open Project Manager?"));
						confirmation->popup_centered_minsize();
					} else {
						_discard_changes();
						break;
					}
				} else {
					if (save_each) {
						if (p_option == RELOAD_CURRENT_PROJECT) {
							_menu_option_confirm(FILE_CLOSE_ALL_AND_RELOAD_CURRENT_PROJECT, false);
						} else if (p_option == FILE_QUIT) {
							_menu_option_confirm(FILE_CLOSE_ALL_AND_QUIT, false);
						} else {
							_menu_option_confirm(FILE_CLOSE_ALL_AND_RUN_PROJECT_MANAGER, false);
						}
					} else {
						String unsaved_scenes;
						int i = _next_unsaved_scene(true, 0);
						while (i != -1) {
							unsaved_scenes += "\n            " + editor_data.get_edited_scene_root(i)->get_filename();
							i = _next_unsaved_scene(true, ++i);
						}
						if (p_option == RELOAD_CURRENT_PROJECT) {
							save_confirmation->get_ok()->set_text(TTR("Save & Reload"));
							save_confirmation->set_text(TTR("Save changes to the following scene(s) before reloading?") + unsaved_scenes);
						} else {
							save_confirmation->get_ok()->set_text(TTR("Save & Quit"));
							save_confirmation->set_text((p_option == FILE_QUIT ? TTR("Save changes to the following scene(s) before quitting?") : TTR("Save changes to the following scene(s) before opening Project Manager?")) + unsaved_scenes);
						}
						save_confirmation->popup_centered_minsize();
					}
				}

				OS::get_singleton()->request_attention();
				break;
			}

			if (_next_unsaved_scene(true) != -1) {
				_save_all_scenes();
			}
			_discard_changes();
		} break;
		case RUN_FILE_SERVER: {
			bool ischecked = debug_menu->get_popup()->is_item_checked(debug_menu->get_popup()->get_item_index(RUN_FILE_SERVER));

			if (ischecked) {
				file_server->stop();
				run_native->set_deploy_dumb(false);
			} else {
				file_server->start();
				run_native->set_deploy_dumb(true);
			}

			debug_menu->get_popup()->set_item_checked(debug_menu->get_popup()->get_item_index(RUN_FILE_SERVER), !ischecked);
			EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_file_server", !ischecked);
		} break;
		case RUN_LIVE_DEBUG: {
			bool ischecked = debug_menu->get_popup()->is_item_checked(debug_menu->get_popup()->get_item_index(RUN_LIVE_DEBUG));

			debug_menu->get_popup()->set_item_checked(debug_menu->get_popup()->get_item_index(RUN_LIVE_DEBUG), !ischecked);
			ScriptEditor::get_singleton()->get_debugger()->set_live_debugging(!ischecked);
			EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_live_debug", !ischecked);

		} break;
		case RUN_DEPLOY_REMOTE_DEBUG: {
			bool ischecked = debug_menu->get_popup()->is_item_checked(debug_menu->get_popup()->get_item_index(RUN_DEPLOY_REMOTE_DEBUG));
			debug_menu->get_popup()->set_item_checked(debug_menu->get_popup()->get_item_index(RUN_DEPLOY_REMOTE_DEBUG), !ischecked);
			run_native->set_deploy_debug_remote(!ischecked);
			EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_deploy_remote_debug", !ischecked);

		} break;
		case RUN_DEBUG_COLLISONS: {
			bool ischecked = debug_menu->get_popup()->is_item_checked(debug_menu->get_popup()->get_item_index(RUN_DEBUG_COLLISONS));
			debug_menu->get_popup()->set_item_checked(debug_menu->get_popup()->get_item_index(RUN_DEBUG_COLLISONS), !ischecked);
			run_native->set_debug_collisions(!ischecked);
			editor_run.set_debug_collisions(!ischecked);
			EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_debug_collisons", !ischecked);

		} break;
		case RUN_DEBUG_NAVIGATION: {
			bool ischecked = debug_menu->get_popup()->is_item_checked(debug_menu->get_popup()->get_item_index(RUN_DEBUG_NAVIGATION));
			debug_menu->get_popup()->set_item_checked(debug_menu->get_popup()->get_item_index(RUN_DEBUG_NAVIGATION), !ischecked);
			run_native->set_debug_navigation(!ischecked);
			editor_run.set_debug_navigation(!ischecked);
			EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_debug_navigation", !ischecked);

		} break;
		case RUN_DEBUG_SHADER_FALLBACKS: {
			bool ischecked = debug_menu->get_popup()->is_item_checked(debug_menu->get_popup()->get_item_index(RUN_DEBUG_SHADER_FALLBACKS));
			debug_menu->get_popup()->set_item_checked(debug_menu->get_popup()->get_item_index(RUN_DEBUG_SHADER_FALLBACKS), !ischecked);
			run_native->set_debug_shader_fallbacks(!ischecked);
			editor_run.set_debug_shader_fallbacks(!ischecked);
			EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_debug_shader_fallbacks", !ischecked);
		} break;
		case RUN_RELOAD_SCRIPTS: {
			bool ischecked = debug_menu->get_popup()->is_item_checked(debug_menu->get_popup()->get_item_index(RUN_RELOAD_SCRIPTS));
			debug_menu->get_popup()->set_item_checked(debug_menu->get_popup()->get_item_index(RUN_RELOAD_SCRIPTS), !ischecked);

			ScriptEditor::get_singleton()->set_live_auto_reload_running_scripts(!ischecked);
			EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_reload_scripts", !ischecked);

		} break;
		case SETTINGS_UPDATE_CONTINUOUSLY: {
			EditorSettings::get_singleton()->set("interface/editor/update_continuously", true);
			_update_update_spinner();
			show_accept(TTR("This option is deprecated. Situations where refresh must be forced are now considered a bug. Please report."), TTR("OK"));
		} break;
		case SETTINGS_UPDATE_WHEN_CHANGED: {
			EditorSettings::get_singleton()->set("interface/editor/update_continuously", false);
			EditorSettings::get_singleton()->set("interface/editor/update_vital_only", false);
			_update_update_spinner();
		} break;
		case SETTINGS_UPDATE_VITAL_ONLY: {
			EditorSettings::get_singleton()->set("interface/editor/update_continuously", false);
			EditorSettings::get_singleton()->set("interface/editor/update_vital_only", true);
			_update_update_spinner();
		} break;
		case SETTINGS_UPDATE_SPINNER_HIDE: {
			EditorSettings::get_singleton()->set("interface/editor/show_update_spinner", false);
			_update_update_spinner();
		} break;
		case SETTINGS_PREFERENCES: {
			settings_config_dialog->popup_edit_settings();
		} break;
		case SETTINGS_EDITOR_DATA_FOLDER: {
			OS::get_singleton()->shell_open(String("file://") + EditorSettings::get_singleton()->get_data_dir());
		} break;
		case SETTINGS_EDITOR_CONFIG_FOLDER: {
			OS::get_singleton()->shell_open(String("file://") + EditorSettings::get_singleton()->get_settings_dir());
		} break;
		case SETTINGS_MANAGE_EXPORT_TEMPLATES: {
			export_template_manager->popup_manager();

		} break;
		case SETTINGS_INSTALL_ANDROID_BUILD_TEMPLATE: {
			custom_build_manage_templates->hide();
			file_android_build_source->popup_centered_ratio();
		} break;
		case SETTINGS_MANAGE_FEATURE_PROFILES: {
			feature_profile_manager->popup_centered_clamped(Size2(900, 800) * EDSCALE, 0.8);
		} break;
		case SETTINGS_TOGGLE_FULLSCREEN: {
			OS::get_singleton()->set_window_fullscreen(!OS::get_singleton()->is_window_fullscreen());

		} break;
		case EDITOR_SCREENSHOT: {
			screenshot_timer->start();
		} break;
		case SETTINGS_PICK_MAIN_SCENE: {
			file->set_mode(EditorFileDialog::MODE_OPEN_FILE);
			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);
			file->clear_filters();
			for (int i = 0; i < extensions.size(); i++) {
				file->add_filter("*." + extensions[i] + " ; " + extensions[i].to_upper());
			}

			Node *scene = editor_data.get_edited_scene_root();
			if (scene) {
				file->set_current_path(scene->get_filename());
			};
			file->set_title(TTR("Pick a Main Scene"));
			file->popup_centered_ratio();

		} break;
		case HELP_SEARCH: {
			emit_signal("request_help_search", "");
		} break;
		case HELP_DOCS: {
			OS::get_singleton()->shell_open(VERSION_DOCS_URL "/");
		} break;
		case HELP_QA: {
			OS::get_singleton()->shell_open("https://godotengine.org/qa/");
		} break;
		case HELP_REPORT_A_BUG: {
			OS::get_singleton()->shell_open("https://github.com/godotengine/godot/issues");
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
			about->popup_centered_minsize(Size2(780, 500) * EDSCALE);
		} break;
		case HELP_SUPPORT_GODOT_DEVELOPMENT: {
			OS::get_singleton()->shell_open("https://godotengine.org/donate");
		} break;

		case SET_VIDEO_DRIVER_SAVE_AND_RESTART: {
			ProjectSettings::get_singleton()->set("rendering/quality/driver/driver_name", video_driver_request);
			ProjectSettings::get_singleton()->save();

			save_all_scenes();
			restart_editor();
		} break;
	}
}

void EditorNode::_request_screenshot() {
	_screenshot();
}

void EditorNode::_screenshot(bool p_use_utc) {
	String name = "editor_screenshot_" + OS::get_singleton()->get_iso_date_time(p_use_utc).replace(":", "") + ".png";
	NodePath path = String("user://") + name;
	_save_screenshot(path);
	if (EditorSettings::get_singleton()->get("interface/editor/automatically_open_screenshots")) {
		OS::get_singleton()->shell_open(String("file://") + ProjectSettings::get_singleton()->globalize_path(path));
	}
}

void EditorNode::_save_screenshot(NodePath p_path) {
	Viewport *viewport = EditorInterface::get_singleton()->get_editor_viewport()->get_viewport();
	viewport->set_clear_mode(Viewport::CLEAR_MODE_ONLY_NEXT_FRAME);
	Ref<Image> img = viewport->get_texture()->get_data();
	img->flip_y();
	viewport->set_clear_mode(Viewport::CLEAR_MODE_ALWAYS);
	Error error = img->save_png(p_path);
	ERR_FAIL_COND_MSG(error != OK, "Cannot save screenshot to file '" + p_path + "'.");
}

void EditorNode::_tool_menu_option(int p_idx) {
	switch (tool_menu->get_item_id(p_idx)) {
		case TOOLS_ORPHAN_RESOURCES: {
			orphan_resources->show();
		} break;
		case TOOLS_CUSTOM: {
			if (tool_menu->get_item_submenu(p_idx) == "") {
				Array params = tool_menu->get_item_metadata(p_idx);

				Object *handler = ObjectDB::get_instance(params[0]);
				String callback = params[1];
				Variant *ud = &params[2];
				Variant::CallError ce;

				handler->call(callback, (const Variant **)&ud, 1, ce);
				if (ce.error != Variant::CallError::CALL_OK) {
					String err = Variant::get_call_error_text(handler, callback, (const Variant **)&ud, 1, ce);
					ERR_PRINT("Error calling function from tool menu: " + err);
				}
			} // else it's a submenu so don't do anything.
		} break;
	}
}

int EditorNode::_next_unsaved_scene(bool p_valid_filename, int p_start) {
	for (int i = p_start; i < editor_data.get_edited_scene_count(); i++) {
		if (!editor_data.get_edited_scene_root(i)) {
			continue;
		}
		int current = editor_data.get_edited_scene();
		bool unsaved = (i == current) ? saved_version != editor_data.get_undo_redo().get_version() : editor_data.get_scene_version(i) != 0;
		if (unsaved) {
			String scene_filename = editor_data.get_edited_scene_root(i)->get_filename();
			if (p_valid_filename && scene_filename.length() == 0) {
				continue;
			}
			return i;
		}
	}
	return -1;
}

void EditorNode::_exit_editor() {
	exiting = true;
	resource_preview->stop(); //stop early to avoid crashes
	_save_docks();

	// Dim the editor window while it's quitting to make it clearer that it's busy
	dim_editor(true, true);

	get_tree()->quit();
}

void EditorNode::_discard_changes(const String &p_str) {
	switch (current_option) {
		case FILE_CLOSE_ALL_AND_QUIT:
		case FILE_CLOSE_ALL_AND_RUN_PROJECT_MANAGER:
		case FILE_CLOSE_ALL_AND_RELOAD_CURRENT_PROJECT:
		case FILE_CLOSE:
		case FILE_CLOSE_OTHERS:
		case FILE_CLOSE_RIGHT:
		case FILE_CLOSE_ALL:
		case SCENE_TAB_CLOSE: {
			Node *scene = editor_data.get_edited_scene_root(tab_closing);
			if (scene != nullptr) {
				String scene_filename = scene->get_filename();
				if (scene_filename != "") {
					previous_scenes.push_back(scene_filename);
				}
			}

			_remove_scene(tab_closing);
			_update_scene_tabs();

			if (current_option == FILE_CLOSE_ALL_AND_QUIT || current_option == FILE_CLOSE_ALL_AND_RUN_PROJECT_MANAGER || current_option == FILE_CLOSE_ALL_AND_RELOAD_CURRENT_PROJECT) {
				// If restore tabs is enabled, reopen the scene that has just been closed, so it's remembered properly.
				if (bool(EDITOR_GET("interface/scene_tabs/restore_scenes_on_load"))) {
					_menu_option_confirm(FILE_OPEN_PREV, true);
				}
				if (_next_unsaved_scene(false) == -1) {
					if (current_option == FILE_CLOSE_ALL_AND_RELOAD_CURRENT_PROJECT) {
						current_option = RELOAD_CURRENT_PROJECT;
					} else if (current_option == FILE_CLOSE_ALL_AND_QUIT) {
						current_option = FILE_QUIT;
					} else {
						current_option = RUN_PROJECT_MANAGER;
					}
					_discard_changes();
				} else {
					_menu_option_confirm(current_option, false);
				}
			} else if (current_option == FILE_CLOSE_OTHERS || current_option == FILE_CLOSE_RIGHT) {
				if (editor_data.get_edited_scene_count() == 1 || (current_option == FILE_CLOSE_RIGHT && editor_data.get_edited_scene_count() <= editor_data.get_edited_scene() + 1)) {
					current_option = -1;
					save_confirmation->hide();
				} else {
					_menu_option_confirm(current_option, false);
				}
			} else if (current_option == FILE_CLOSE_ALL && editor_data.get_edited_scene_count() > 0) {
				_menu_option_confirm(current_option, false);
			} else {
				current_option = -1;
				save_confirmation->hide();
			}
		} break;
		case FILE_QUIT: {
			_menu_option_confirm(RUN_STOP, true);
			_exit_editor();

		} break;
		case RUN_PROJECT_MANAGER: {
			_menu_option_confirm(RUN_STOP, true);
			_exit_editor();
			String exec = OS::get_singleton()->get_executable_path();

			List<String> args;
			const Vector<String> &forwardable_args = Main::get_forwardable_cli_arguments(Main::CLI_SCOPE_TOOL);
			for (int i = 0; i < forwardable_args.size(); i++) {
				args.push_back(forwardable_args[i]);
			}

			String exec_base_dir = exec.get_base_dir();
			if (!exec_base_dir.empty()) {
				args.push_back("--path");
				args.push_back(exec_base_dir);
			}
			args.push_back("--project-manager");

			OS::ProcessID pid = 0;
			Error err = OS::get_singleton()->execute(exec, args, false, &pid);
			ERR_FAIL_COND(err);
		} break;
		case RELOAD_CURRENT_PROJECT: {
			restart_editor();
		} break;
	}
}

void EditorNode::_update_debug_options() {
	bool check_deploy_remote = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_deploy_remote_debug", false);
	bool check_file_server = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_file_server", false);
	bool check_debug_collisons = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_debug_collisons", false);
	bool check_debug_navigation = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_debug_navigation", false);
	bool check_debug_shader_fallbacks = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_debug_shader_fallbacks", false);
	bool check_live_debug = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_live_debug", true);
	bool check_reload_scripts = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_reload_scripts", true);

	if (check_deploy_remote) {
		_menu_option_confirm(RUN_DEPLOY_REMOTE_DEBUG, true);
	}
	if (check_file_server) {
		_menu_option_confirm(RUN_FILE_SERVER, true);
	}
	if (check_debug_collisons) {
		_menu_option_confirm(RUN_DEBUG_COLLISONS, true);
	}
	if (check_debug_navigation) {
		_menu_option_confirm(RUN_DEBUG_NAVIGATION, true);
	}
	if (check_debug_shader_fallbacks) {
		_menu_option_confirm(RUN_DEBUG_SHADER_FALLBACKS, true);
	}
	if (check_live_debug) {
		_menu_option_confirm(RUN_LIVE_DEBUG, true);
	}
	if (check_reload_scripts) {
		_menu_option_confirm(RUN_RELOAD_SCRIPTS, true);
	}
}

void EditorNode::_update_file_menu_opened() {
	Ref<ShortCut> close_scene_sc = ED_GET_SHORTCUT("editor/close_scene");
	close_scene_sc->set_name(TTR("Close Scene"));
	Ref<ShortCut> reopen_closed_scene_sc = ED_GET_SHORTCUT("editor/reopen_closed_scene");
	reopen_closed_scene_sc->set_name(TTR("Reopen Closed Scene"));

	PopupMenu *pop = file_menu->get_popup();
	pop->set_item_disabled(pop->get_item_index(FILE_OPEN_PREV), previous_scenes.empty());

	const UndoRedo &undo_redo = editor_data.get_undo_redo();
	pop->set_item_disabled(pop->get_item_index(EDIT_UNDO), !undo_redo.has_undo());
	pop->set_item_disabled(pop->get_item_index(EDIT_REDO), !undo_redo.has_redo());
}

void EditorNode::_update_file_menu_closed() {
	PopupMenu *pop = file_menu->get_popup();
	pop->set_item_disabled(pop->get_item_index(FILE_OPEN_PREV), false);
}

Control *EditorNode::get_viewport() {
	return viewport;
}

void EditorNode::_editor_select(int p_which) {
	static bool selecting = false;
	if (selecting || changing_scene) {
		return;
	}

	ERR_FAIL_INDEX(p_which, editor_table.size());

	if (!main_editor_buttons[p_which]->is_visible()) { //button hidden, no editor
		return;
	}

	selecting = true;

	for (int i = 0; i < main_editor_buttons.size(); i++) {
		main_editor_buttons[i]->set_pressed(i == p_which);
	}

	selecting = false;

	EditorPlugin *new_editor = editor_table[p_which];
	ERR_FAIL_COND(!new_editor);

	if (editor_plugin_screen == new_editor) {
		return;
	}

	if (editor_plugin_screen) {
		editor_plugin_screen->make_visible(false);
	}

	editor_plugin_screen = new_editor;
	editor_plugin_screen->make_visible(true);
	editor_plugin_screen->selected_notify();

	int plugin_count = editor_data.get_editor_plugin_count();
	for (int i = 0; i < plugin_count; i++) {
		editor_data.get_editor_plugin(i)->notify_main_screen_changed(editor_plugin_screen->get_name());
	}

	if (EditorSettings::get_singleton()->get("interface/editor/separate_distraction_mode")) {
		if (p_which == EDITOR_SCRIPT) {
			set_distraction_free_mode(script_distraction);
		} else {
			set_distraction_free_mode(scene_distraction);
		}
	}
}

void EditorNode::select_editor_by_name(const String &p_name) {
	ERR_FAIL_COND(p_name == "");

	for (int i = 0; i < main_editor_buttons.size(); i++) {
		if (main_editor_buttons[i]->get_text() == p_name) {
			_editor_select(i);
			return;
		}
	}

	ERR_FAIL_MSG("The editor name '" + p_name + "' was not found.");
}

void EditorNode::add_editor_plugin(EditorPlugin *p_editor, bool p_config_changed) {
	if (p_editor->has_main_screen()) {
		ToolButton *tb = memnew(ToolButton);
		tb->set_toggle_mode(true);
		tb->connect("pressed", singleton, "_editor_select", varray(singleton->main_editor_buttons.size()));
		tb->set_text(p_editor->get_name());
		Ref<Texture> icon = p_editor->get_icon();

		if (icon.is_valid()) {
			tb->set_icon(icon);
		} else if (singleton->gui_base->has_icon(p_editor->get_name(), "EditorIcons")) {
			tb->set_icon(singleton->gui_base->get_icon(p_editor->get_name(), "EditorIcons"));
		}

		tb->set_name(p_editor->get_name());
		singleton->main_editor_buttons.push_back(tb);
		singleton->main_editor_button_vb->add_child(tb);
		singleton->editor_table.push_back(p_editor);

		singleton->distraction_free->raise();
	}
	singleton->editor_data.add_editor_plugin(p_editor);
	singleton->add_child(p_editor);
	if (p_config_changed) {
		p_editor->enable_plugin();
	}
}

void EditorNode::remove_editor_plugin(EditorPlugin *p_editor, bool p_config_changed) {
	if (p_editor->has_main_screen()) {
		for (int i = 0; i < singleton->main_editor_buttons.size(); i++) {
			if (p_editor->get_name() == singleton->main_editor_buttons[i]->get_text()) {
				if (singleton->main_editor_buttons[i]->is_pressed()) {
					singleton->_editor_select(EDITOR_SCRIPT);
				}

				memdelete(singleton->main_editor_buttons[i]);
				singleton->main_editor_buttons.remove(i);

				break;
			}
		}

		singleton->editor_table.erase(p_editor);
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
}

void EditorNode::_update_addon_config() {
	if (_initializing_addons) {
		return;
	}

	Vector<String> enabled_addons;

	for (Map<String, EditorPlugin *>::Element *E = plugin_addons.front(); E; E = E->next()) {
		enabled_addons.push_back(E->key());
	}

	if (enabled_addons.size() == 0) {
		ProjectSettings::get_singleton()->set("editor_plugins/enabled", Variant());
	} else {
		ProjectSettings::get_singleton()->set("editor_plugins/enabled", enabled_addons);
	}

	project_settings->queue_save();
}

void EditorNode::set_addon_plugin_enabled(String p_addon, bool p_enabled, bool p_config_changed) {
	if (!p_addon.begins_with("res://")) {
		p_addon = _to_absolute_plugin_path(p_addon);
	}

	ERR_FAIL_COND(p_enabled && plugin_addons.has(p_addon));
	ERR_FAIL_COND(!p_enabled && !plugin_addons.has(p_addon));

	if (!p_enabled) {
		EditorPlugin *addon = plugin_addons[p_addon];
		remove_editor_plugin(addon, p_config_changed);
		memdelete(addon); //bye
		plugin_addons.erase(p_addon);
		_update_addon_config();
		return;
	}

	Ref<ConfigFile> cf;
	cf.instance();
	if (!DirAccess::exists(p_addon.get_base_dir())) {
		_remove_plugin_from_enabled(p_addon);
		WARN_PRINT("Addon '" + p_addon + "' failed to load. No directory found. Removing from enabled plugins.");
		return;
	}
	Error err = cf->load(p_addon);
	if (err != OK) {
		show_warning(vformat(TTR("Unable to enable addon plugin at: '%s' parsing of config failed."), p_addon));
		return;
	}

	if (!cf->has_section_key("plugin", "script")) {
		show_warning(vformat(TTR("Unable to find script field for addon plugin at: '%s'."), p_addon));
		return;
	}

	String script_path = cf->get_value("plugin", "script");
	Ref<Script> script; // We need to save it for creating "ep" below.

	// Only try to load the script if it has a name. Else, the plugin has no init script.
	if (script_path.length() > 0) {
		script_path = p_addon.get_base_dir().plus_file(script_path);
		script = ResourceLoader::load(script_path);

		if (script.is_null()) {
			show_warning(vformat(TTR("Unable to load addon script from path: '%s'."), script_path));
			return;
		}

		// Errors in the script cause the base_type to be an empty string.
		if (String(script->get_instance_base_type()) == "") {
			show_warning(vformat(TTR("Unable to load addon script from path: '%s'. This might be due to a code error in that script.\nDisabling the addon at '%s' to prevent further errors."), script_path, p_addon));
			_remove_plugin_from_enabled(p_addon);
			return;
		}

		// Plugin init scripts must inherit from EditorPlugin and be tools.
		if (String(script->get_instance_base_type()) != "EditorPlugin") {
			show_warning(vformat(TTR("Unable to load addon script from path: '%s' Base type is not EditorPlugin."), script_path));
			return;
		}

		if (!script->is_tool()) {
			show_warning(vformat(TTR("Unable to load addon script from path: '%s' Script is not in tool mode."), script_path));
			return;
		}
	}

	EditorPlugin *ep = memnew(EditorPlugin);
	ep->set_script(script.get_ref_ptr());
	plugin_addons[p_addon] = ep;
	add_editor_plugin(ep, p_config_changed);

	_update_addon_config();
}

bool EditorNode::is_addon_plugin_enabled(const String &p_addon) const {
	if (p_addon.begins_with("res://")) {
		return plugin_addons.has(p_addon);
	}

	return plugin_addons.has(_to_absolute_plugin_path(p_addon));
}

void EditorNode::_remove_edited_scene(bool p_change_tab) {
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
		_scene_tab_changed(new_index);
	}
	editor_data.remove_scene(old_index);
	editor_data.get_undo_redo().clear_history(false);
	_update_title();
	_update_scene_tabs();
}

void EditorNode::_remove_scene(int index, bool p_change_tab) {
	// Clear icon cache in case some scripts are no longer needed.
	script_icon_cache.clear();

	if (editor_data.get_edited_scene() == index) {
		//Scene to remove is current scene
		_remove_edited_scene(p_change_tab);
	} else {
		//Scene to remove is not active scene
		editor_data.remove_scene(index);
	}
}

void EditorNode::set_edited_scene(Node *p_scene) {
	if (get_editor_data().get_edited_scene_root()) {
		if (get_editor_data().get_edited_scene_root()->get_parent() == scene_root) {
			scene_root->remove_child(get_editor_data().get_edited_scene_root());
		}
	}
	get_editor_data().set_edited_scene_root(p_scene);

	if (Object::cast_to<Popup>(p_scene)) {
		Object::cast_to<Popup>(p_scene)->show(); //show popups
	}
	scene_tree_dock->set_edited_scene(p_scene);
	if (get_tree()) {
		get_tree()->set_edited_scene_root(p_scene);
	}

	if (p_scene) {
		if (p_scene->get_parent() != scene_root) {
			scene_root->add_child(p_scene);
		}
	}
}

int EditorNode::_get_current_main_editor() {
	for (int i = 0; i < editor_table.size(); i++) {
		if (editor_table[i] == editor_plugin_screen) {
			return i;
		}
	}

	return 0;
}

Dictionary EditorNode::_get_main_scene_state() {
	Dictionary state;
	state["main_tab"] = _get_current_main_editor();
	state["scene_tree_offset"] = scene_tree_dock->get_tree_editor()->get_scene_tree()->get_vscroll_bar()->get_value();
	state["property_edit_offset"] = get_inspector()->get_scroll_offset();
	state["saved_version"] = saved_version;
	state["node_filter"] = scene_tree_dock->get_filter();
	return state;
}

void EditorNode::_set_main_scene_state(Dictionary p_state, Node *p_for_scene) {
	if (get_edited_scene() != p_for_scene && p_for_scene != nullptr) {
		return; //not for this scene
	}

	changing_scene = false;

	int current = -1;
	for (int i = 0; i < editor_table.size(); i++) {
		if (editor_plugin_screen == editor_table[i]) {
			current = i;
			break;
		}
	}

	if (p_state.has("editor_index")) {
		int index = p_state["editor_index"];
		if (current < 2) { //if currently in spatial/2d, only switch to spatial/2d. if currently in script, stay there
			if (index < 2 || !get_edited_scene()) {
				_editor_select(index);
			}
		}
	}

	if (get_edited_scene()) {
		if (current < 2) {
			//use heuristic instead
			int n2d = 0, n3d = 0;
			_find_node_types(get_edited_scene(), n2d, n3d);
			if (n2d > n3d) {
				_editor_select(EDITOR_2D);
			} else if (n3d > n2d) {
				_editor_select(EDITOR_3D);
			}
		}
	}

	if (p_state.has("scene_tree_offset")) {
		scene_tree_dock->get_tree_editor()->get_scene_tree()->get_vscroll_bar()->set_value(p_state["scene_tree_offset"]);
	}
	if (p_state.has("property_edit_offset")) {
		get_inspector()->set_scroll_offset(p_state["property_edit_offset"]);
	}

	if (p_state.has("node_filter")) {
		scene_tree_dock->set_filter(p_state["node_filter"]);
	}

	//this should only happen at the very end

	ScriptEditor::get_singleton()->get_debugger()->update_live_edit_root();
	ScriptEditor::get_singleton()->set_scene_root_script(editor_data.get_scene_root_script(editor_data.get_edited_scene()));
	editor_data.notify_edited_scene_changed();
}

void EditorNode::set_current_version(uint64_t p_version) {
	saved_version = p_version;
	editor_data.set_edited_scene_version(p_version);
}

bool EditorNode::is_changing_scene() const {
	return changing_scene;
}

void EditorNode::_clear_undo_history() {
	get_undo_redo()->clear_history(false);
}

void EditorNode::set_current_scene(int p_idx) {
	//Save the folding in case the scene gets reloaded.
	if (editor_data.get_scene_path(p_idx) != "" && editor_data.get_edited_scene_root(p_idx)) {
		editor_folding.save_scene_folding(editor_data.get_edited_scene_root(p_idx), editor_data.get_scene_path(p_idx));
	}

	if (editor_data.check_and_update_scene(p_idx)) {
		if (editor_data.get_scene_path(p_idx) != "") {
			editor_folding.load_scene_folding(editor_data.get_edited_scene_root(p_idx), editor_data.get_scene_path(p_idx));
		}

		call_deferred("_clear_undo_history");
	}

	changing_scene = true;
	editor_data.save_edited_scene_state(editor_selection, &editor_history, _get_main_scene_state());

	if (get_editor_data().get_edited_scene_root()) {
		if (get_editor_data().get_edited_scene_root()->get_parent() == scene_root) {
			scene_root->remove_child(get_editor_data().get_edited_scene_root());
		}
	}

	editor_selection->clear();
	editor_data.set_edited_scene(p_idx);

	Node *new_scene = editor_data.get_edited_scene_root();

	if (Object::cast_to<Popup>(new_scene)) {
		Object::cast_to<Popup>(new_scene)->show(); //show popups
	}

	scene_tree_dock->set_edited_scene(new_scene);
	if (get_tree()) {
		get_tree()->set_edited_scene_root(new_scene);
	}

	if (new_scene) {
		if (new_scene->get_parent() != scene_root) {
			scene_root->add_child(new_scene);
		}
	}

	Dictionary state = editor_data.restore_edited_scene_state(editor_selection, &editor_history);
	_edit_current(true);

	_update_title();

	call_deferred("_set_main_scene_state", state, get_edited_scene()); //do after everything else is done setting up
}

bool EditorNode::is_scene_open(const String &p_path) {
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		if (editor_data.get_scene_path(i) == p_path) {
			return true;
		}
	}

	return false;
}

void EditorNode::fix_dependencies(const String &p_for_file) {
	dependency_fixer->edit(p_for_file);
}

int EditorNode::new_scene() {
	int idx = editor_data.add_edited_scene(-1);
	_scene_tab_changed(idx);
	editor_data.clear_editor_states();
	_update_scene_tabs();
	return idx;
}

Error EditorNode::load_scene(const String &p_scene, bool p_ignore_broken_deps, bool p_set_inherited, bool p_clear_errors, bool p_force_open_imported, bool p_silent_change_tab) {
	if (!is_inside_tree()) {
		defer_load_scene = p_scene;
		return OK;
	}

	if (!p_set_inherited) {
		for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
			if (editor_data.get_scene_path(i) == p_scene) {
				_scene_tab_changed(i);
				return OK;
			}
		}

		if (!p_force_open_imported && FileAccess::exists(p_scene + ".import")) {
			open_imported->set_text(vformat(TTR("Scene '%s' was automatically imported, so it can't be modified.\nTo make changes to it, a new inherited scene can be created."), p_scene.get_file()));
			open_imported->popup_centered_minsize();
			new_inherited_button->grab_focus();
			open_import_request = p_scene;
			return OK;
		}
	}

	if (p_clear_errors) {
		load_errors->clear();
	}

	String lpath = ProjectSettings::get_singleton()->localize_path(p_scene);

	if (!lpath.begins_with("res://")) {
		show_accept(TTR("Error loading scene, it must be inside the project path. Use 'Import' to open the scene, then save it inside the project path."), TTR("OK"));
		opening_prev = false;
		return ERR_FILE_NOT_FOUND;
	}

	int prev = editor_data.get_edited_scene();
	int idx = editor_data.add_edited_scene(-1);

	if (!editor_data.get_edited_scene_root() && editor_data.get_edited_scene_count() == 2) {
		_remove_edited_scene();
	} else if (!p_silent_change_tab) {
		_scene_tab_changed(idx);
	} else {
		set_current_scene(idx);
	}

	dependency_errors.clear();

	Error err;
	Ref<PackedScene> sdata = ResourceLoader::load(lpath, "", true, &err);
	if (!sdata.is_valid()) {
		_dialog_display_load_error(lpath, err);
		opening_prev = false;

		if (prev != -1) {
			set_current_scene(prev);
			editor_data.remove_scene(idx);
		}
		return ERR_FILE_NOT_FOUND;
	}

	if (!p_ignore_broken_deps && dependency_errors.has(lpath)) {
		current_option = -1;
		Vector<String> errors;
		for (Set<String>::Element *E = dependency_errors[lpath].front(); E; E = E->next()) {
			errors.push_back(E->get());
		}
		dependency_error->show(DependencyErrorDialog::MODE_SCENE, lpath, errors);
		opening_prev = false;

		if (prev != -1) {
			set_current_scene(prev);
			editor_data.remove_scene(idx);
		}
		return ERR_FILE_MISSING_DEPENDENCIES;
	}

	dependency_errors.erase(lpath); //at least not self path

	for (Map<String, Set<String>>::Element *E = dependency_errors.front(); E; E = E->next()) {
		String txt = vformat(TTR("Scene '%s' has broken dependencies:"), E->key()) + "\n";
		for (Set<String>::Element *F = E->get().front(); F; F = F->next()) {
			txt += "\t" + F->get() + "\n";
		}
		add_io_error(txt);
	}

	if (ResourceCache::has(lpath)) {
		//used from somewhere else? no problem! update state and replace sdata
		Ref<PackedScene> ps = Ref<PackedScene>(Object::cast_to<PackedScene>(ResourceCache::get(lpath)));
		if (ps.is_valid()) {
			ps->replace_state(sdata->get_state());
			ps->set_last_modified_time(sdata->get_last_modified_time());
			sdata = ps;
		}

	} else {
		sdata->set_path(lpath, true); //take over path
	}

	Node *new_scene = sdata->instance(p_set_inherited ? PackedScene::GEN_EDIT_STATE_MAIN_INHERITED : PackedScene::GEN_EDIT_STATE_MAIN);

	if (!new_scene) {
		sdata.unref();
		_dialog_display_load_error(lpath, ERR_FILE_CORRUPT);
		opening_prev = false;
		if (prev != -1) {
			set_current_scene(prev);
			editor_data.remove_scene(idx);
		}
		return ERR_FILE_CORRUPT;
	}

	if (p_set_inherited) {
		Ref<SceneState> state = sdata->get_state();
		state->set_path(lpath);
		new_scene->set_scene_inherited_state(state);
		new_scene->set_filename(String());
	}

	new_scene->set_scene_instance_state(Ref<SceneState>());

	set_edited_scene(new_scene);
	_get_scene_metadata(p_scene);

	saved_version = editor_data.get_undo_redo().get_version();
	_update_title();
	_update_scene_tabs();
	_add_to_recent_scenes(lpath);

	if (editor_folding.has_folding_data(lpath)) {
		editor_folding.load_scene_folding(new_scene, lpath);
	} else if (EDITOR_GET("interface/inspector/auto_unfold_foreign_scenes")) {
		editor_folding.unfold_scene(new_scene);
		editor_folding.save_scene_folding(new_scene, lpath);
	}

	prev_scene->set_disabled(previous_scenes.size() == 0);
	opening_prev = false;
	scene_tree_dock->set_selected(new_scene);

	ScriptEditor::get_singleton()->get_debugger()->update_live_edit_root();

	push_item(new_scene);

	if (!restoring_scenes) {
		save_layout();
	}

	return OK;
}

void EditorNode::open_request(const String &p_path) {
	if (!opening_prev) {
		List<String>::Element *prev_scene = previous_scenes.find(p_path);
		if (prev_scene != nullptr) {
			prev_scene->erase();
		}
	}

	load_scene(p_path); // as it will be opened in separate tab
}

void EditorNode::request_instance_scene(const String &p_path) {
	scene_tree_dock->instance(p_path);
}

void EditorNode::request_instance_scenes(const Vector<String> &p_files) {
	scene_tree_dock->instance_scenes(p_files);
}

ImportDock *EditorNode::get_import_dock() {
	return import_dock;
}

FileSystemDock *EditorNode::get_filesystem_dock() {
	return filesystem_dock;
}
SceneTreeDock *EditorNode::get_scene_tree_dock() {
	return scene_tree_dock;
}
InspectorDock *EditorNode::get_inspector_dock() {
	return inspector_dock;
}

void EditorNode::_inherit_request(String p_file) {
	current_option = FILE_NEW_INHERITED_SCENE;
	_dialog_action(p_file);
}

void EditorNode::_instance_request(const Vector<String> &p_files) {
	request_instance_scenes(p_files);
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
	if (rc.find(p_scene) != -1) {
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
		call_deferred("_update_recent_scenes");
	} else {
		Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scenes", Array());
		ERR_FAIL_INDEX(p_idx, rc.size());

		if (load_scene(rc[p_idx]) != OK) {
			rc.remove(p_idx);
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
	recent_scenes->add_shortcut(ED_SHORTCUT("editor/clear_recent", TTR("Clear Recent Scenes")));
	recent_scenes->set_as_minsize();
}

void EditorNode::_quick_opened() {
	Vector<String> files = quick_open->get_selected_files();

	bool open_scene_dialog = quick_open->get_base_type() == "PackedScene";
	for (int i = 0; i < files.size(); i++) {
		String res_path = files[i];

		List<String> scene_extensions;
		ResourceLoader::get_recognized_extensions_for_type("PackedScene", &scene_extensions);

		if (open_scene_dialog || scene_extensions.find(files[i].get_extension().to_lower())) {
			open_request(res_path);
		} else {
			load_resource(res_path);
		}
	}
}

void EditorNode::_quick_run() {
	_run(false, quick_run->get_selected());
}

void EditorNode::notify_child_process_exited() {
	_menu_option_confirm(RUN_STOP, false);
	stop_button->set_pressed(false);
	editor_run.stop();
}

void EditorNode::add_io_error(const String &p_error) {
	_load_error_notify(singleton, p_error);
}

void EditorNode::_load_error_notify(void *p_ud, const String &p_text) {
	EditorNode *en = (EditorNode *)p_ud;
	en->load_errors->add_image(en->gui_base->get_icon("Error", "EditorIcons"));
	en->load_errors->add_text(p_text + "\n");
	en->load_error_dialog->popup_centered_ratio(0.5);
}

bool EditorNode::_find_scene_in_use(Node *p_node, const String &p_path) const {
	if (p_node->get_filename() == p_path) {
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

void EditorNode::register_editor_types() {
	OS::get_singleton()->benchmark_begin_measure("register_editor_types");

	ResourceLoader::set_timestamp_on_load(true);
	ResourceSaver::set_timestamp_on_save(true);

	ClassDB::register_class<EditorPlugin>();
	ClassDB::register_class<EditorImportPlugin>();
	ClassDB::register_class<EditorScript>();
	ClassDB::register_class<EditorSelection>();
	ClassDB::register_class<EditorFileDialog>();
	ClassDB::register_virtual_class<EditorSettings>();
	ClassDB::register_class<EditorSpatialGizmo>();
	ClassDB::register_class<EditorSpatialGizmoPlugin>();
	ClassDB::register_virtual_class<EditorResourcePreview>();
	ClassDB::register_class<EditorResourcePreviewGenerator>();
	ClassDB::register_virtual_class<EditorFileSystem>();
	ClassDB::register_class<EditorFileSystemDirectory>();
	ClassDB::register_class<EditorVCSInterface>();
	ClassDB::register_virtual_class<ScriptEditor>();
	ClassDB::register_virtual_class<EditorInterface>();
	ClassDB::register_class<EditorExportPlugin>();
	ClassDB::register_class<EditorResourceConversionPlugin>();
	ClassDB::register_class<EditorSceneImporter>();
	ClassDB::register_class<EditorInspector>();
	ClassDB::register_class<EditorInspectorPlugin>();
	ClassDB::register_class<EditorProperty>();
	ClassDB::register_class<AnimationTrackEditPlugin>();
	ClassDB::register_class<ScriptCreateDialog>();
	ClassDB::register_class<EditorFeatureProfile>();
	ClassDB::register_class<EditorSpinSlider>();
	ClassDB::register_class<EditorResourcePicker>();
	ClassDB::register_class<EditorScriptPicker>();
	ClassDB::register_virtual_class<FileSystemDock>();

	// FIXME: Is this stuff obsolete, or should it be ported to new APIs?
	ClassDB::register_class<EditorScenePostImport>();
	//ClassDB::register_type<EditorImportExport>();

	OS::get_singleton()->benchmark_end_measure("register_editor_types");
}

void EditorNode::unregister_editor_types() {
	OS::get_singleton()->benchmark_begin_measure("unregister_editor_types");

	_init_callbacks.clear();

	EditorResourcePicker::clear_caches();

	OS::get_singleton()->benchmark_end_measure("unregister_editor_types");
}

void EditorNode::stop_child_process() {
	_menu_option_confirm(RUN_STOP, false);
}

Ref<Script> EditorNode::get_object_custom_type_base(const Object *p_object) const {
	ERR_FAIL_COND_V(!p_object, nullptr);

	Ref<Script> script = p_object->get_script();

	if (script.is_valid()) {
		// Uncommenting would break things! Consider adding a parameter if you need it.
		// StringName name = EditorNode::get_editor_data().script_class_get_name(base_script->get_path());
		// if (name != StringName())
		// 	return name;

		// should probably be deprecated in 4.x
		StringName base = script->get_instance_base_type();
		if (base != StringName() && EditorNode::get_editor_data().get_custom_types().has(base)) {
			const Vector<EditorData::CustomType> &types = EditorNode::get_editor_data().get_custom_types()[base];

			Ref<Script> base_script = script;
			while (base_script.is_valid()) {
				for (int i = 0; i < types.size(); ++i) {
					if (types[i].script == base_script) {
						return types[i].script;
					}
				}
				base_script = base_script->get_base_script();
			}
		}
	}

	return nullptr;
}

StringName EditorNode::get_object_custom_type_name(const Object *p_object) const {
	ERR_FAIL_COND_V(!p_object, StringName());

	Ref<Script> script = p_object->get_script();
	if (script.is_null() && p_object->is_class("Script")) {
		script = p_object;
	}

	if (script.is_valid()) {
		Ref<Script> base_script = script;
		while (base_script.is_valid()) {
			StringName name = EditorNode::get_editor_data().script_class_get_name(base_script->get_path());
			if (name != StringName()) {
				return name;
			}

			// should probably be deprecated in 4.x
			StringName base = base_script->get_instance_base_type();
			if (base != StringName() && EditorNode::get_editor_data().get_custom_types().has(base)) {
				const Vector<EditorData::CustomType> &types = EditorNode::get_editor_data().get_custom_types()[base];
				for (int i = 0; i < types.size(); ++i) {
					if (types[i].script == base_script) {
						return types[i].name;
					}
				}
			}
			base_script = base_script->get_base_script();
		}
	}

	return StringName();
}

Ref<ImageTexture> EditorNode::_load_custom_class_icon(const String &p_path) const {
	if (p_path.length()) {
		Ref<Image> img = memnew(Image);
		Error err = ImageLoader::load_image(p_path, img);
		if (err == OK) {
			Ref<ImageTexture> icon = memnew(ImageTexture);
			img->resize(16 * EDSCALE, 16 * EDSCALE, Image::INTERPOLATE_LANCZOS);
			icon->create_from_image(img);
			return icon;
		}
	}
	return nullptr;
}

String EditorNode::_to_absolute_plugin_path(const String &p_plugin_name) {
	return "res://addons/" + p_plugin_name + "/plugin.cfg";
}

void EditorNode::_pick_main_scene_custom_action(const String &p_custom_action_name) {
	if (p_custom_action_name == "select_current") {
		Node *scene = editor_data.get_edited_scene_root();

		if (!scene) {
			show_accept(TTR("There is no defined scene to run."), TTR("OK"));
			return;
		}

		pick_main_scene->hide();
		current_option = SETTINGS_PICK_MAIN_SCENE;
		_dialog_action(scene->get_filename());
	}
}

Ref<Texture> EditorNode::get_object_icon(const Object *p_object, const String &p_fallback) {
	ERR_FAIL_COND_V(!p_object || !gui_base, nullptr);

	Ref<Script> script = p_object->get_script();
	if (script.is_null() && p_object->is_class("Script")) {
		script = p_object;
	}

	if (script.is_valid() && !script_icon_cache.has(script)) {
		Ref<Script> base_script = script;
		while (base_script.is_valid()) {
			StringName name = EditorNode::get_editor_data().script_class_get_name(base_script->get_path());
			String icon_path = EditorNode::get_editor_data().script_class_get_icon_path(name);
			Ref<ImageTexture> icon = _load_custom_class_icon(icon_path);
			if (icon.is_valid()) {
				script_icon_cache[script] = icon;
				return icon;
			}

			// should probably be deprecated in 4.x
			StringName base = base_script->get_instance_base_type();
			if (base != StringName() && EditorNode::get_editor_data().get_custom_types().has(base)) {
				const Vector<EditorData::CustomType> &types = EditorNode::get_editor_data().get_custom_types()[base];
				for (int i = 0; i < types.size(); ++i) {
					if (types[i].script == base_script && types[i].icon.is_valid()) {
						script_icon_cache[script] = types[i].icon;
						return types[i].icon;
					}
				}
			}
			base_script = base_script->get_base_script();
		}

		// If no icon found, cache it as null.
		script_icon_cache[script] = Ref<Texture>();
	} else if (script.is_valid() && script_icon_cache.has(script) && script_icon_cache[script].is_valid()) {
		return script_icon_cache[script];
	}

	// should probably be deprecated in 4.x
	if (p_object->has_meta("_editor_icon")) {
		return p_object->get_meta("_editor_icon");
	}

	if (gui_base->has_icon(p_object->get_class(), "EditorIcons")) {
		return gui_base->get_icon(p_object->get_class(), "EditorIcons");
	}

	if (p_fallback.length()) {
		return gui_base->get_icon(p_fallback, "EditorIcons");
	}

	return nullptr;
}

Ref<Texture> EditorNode::get_class_icon(const String &p_class, const String &p_fallback) const {
	ERR_FAIL_COND_V_MSG(p_class.empty(), nullptr, "Class name cannot be empty.");

	if (ScriptServer::is_global_class(p_class)) {
		String class_name = p_class;
		Ref<Script> script = EditorNode::get_editor_data().script_class_load_script(class_name);

		while (true) {
			String icon_path = EditorNode::get_editor_data().script_class_get_icon_path(class_name);
			Ref<Texture> icon = _load_custom_class_icon(icon_path);
			if (icon.is_valid()) {
				return icon; // Current global class has icon.
			}

			// Find next global class along the inheritance chain.
			do {
				Ref<Script> base_script = script->get_base_script();
				if (base_script.is_null()) {
					// We've reached a native class, use its icon.
					String base_type;
					script->get_language()->get_global_class_name(script->get_path(), &base_type);
					if (gui_base->has_icon(base_type, "EditorIcons")) {
						return gui_base->get_icon(base_type, "EditorIcons");
					}
					return gui_base->get_icon(p_fallback, "EditorIcons");
				}
				script = base_script;
				class_name = EditorNode::get_editor_data().script_class_get_name(script->get_path());
			} while (class_name.empty());
		}
	}

	const Map<String, Vector<EditorData::CustomType>> &p_map = EditorNode::get_editor_data().get_custom_types();
	for (const Map<String, Vector<EditorData::CustomType>>::Element *E = p_map.front(); E; E = E->next()) {
		const Vector<EditorData::CustomType> &ct = E->value();
		for (int i = 0; i < ct.size(); ++i) {
			if (ct[i].name == p_class) {
				if (ct[i].icon.is_valid()) {
					return ct[i].icon;
				}
			}
		}
	}

	if (gui_base->has_icon(p_class, "EditorIcons")) {
		return gui_base->get_icon(p_class, "EditorIcons");
	}

	if (p_fallback.length() && gui_base->has_icon(p_fallback, "EditorIcons")) {
		return gui_base->get_icon(p_fallback, "EditorIcons");
	}

	return nullptr;
}

void EditorNode::progress_add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel) {
	if (singleton->cmdline_export_mode) {
		print_line(p_task + ": begin: " + p_label + " steps: " + itos(p_steps));
	} else {
		singleton->progress_dialog->add_task(p_task, p_label, p_steps, p_can_cancel);
	}
}

bool EditorNode::progress_task_step(const String &p_task, const String &p_state, int p_step, bool p_force_refresh) {
	if (singleton->cmdline_export_mode) {
		print_line("\t" + p_task + ": step " + itos(p_step) + ": " + p_state);
		return false;
	} else {
		return singleton->progress_dialog->task_step(p_task, p_state, p_step, p_force_refresh);
	}
}

void EditorNode::progress_end_task(const String &p_task) {
	if (singleton->cmdline_export_mode) {
		print_line(p_task + ": end");
	} else {
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

Ref<Texture> EditorNode::_file_dialog_get_icon(const String &p_path) {
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
	StringName ei = "EditorIcons";
	theme_base->get_theme()->get_icon_list(ei, &tl);
	for (List<StringName>::Element *E = tl.front(); E; E = E->next()) {
		if (!ClassDB::class_exists(E->get())) {
			continue;
		}
		icon_type_cache[E->get()] = theme_base->get_theme()->get_icon(E->get(), ei);
	}
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

Error EditorNode::export_preset(const String &p_preset, const String &p_path, bool p_debug, bool p_pack_only) {
	export_defer.preset = p_preset;
	export_defer.path = p_path;
	export_defer.debug = p_debug;
	export_defer.pack_only = p_pack_only;
	cmdline_export_mode = true;
	return OK;
}

void EditorNode::show_accept(const String &p_text, const String &p_title) {
	current_option = -1;
	accept->get_ok()->set_text(p_title);
	accept->set_text(p_text);
	accept->popup_centered_minsize();
}

void EditorNode::show_save_accept(const String &p_text, const String &p_title) {
	current_option = -1;
	save_accept->get_ok()->set_text(p_title);
	save_accept->set_text(p_text);
	save_accept->popup_centered();
}

void EditorNode::show_warning(const String &p_text, const String &p_title) {
	if (warning->is_inside_tree()) {
		warning->set_text(p_text);
		warning->set_title(p_title);
		warning->popup_centered_minsize();
	} else {
		WARN_PRINT(p_title + " " + p_text);
	}
}

void EditorNode::_copy_warning(const String &p_str) {
	OS::get_singleton()->set_clipboard(warning->get_text());
}

void EditorNode::_dock_select_input(const Ref<InputEvent> &p_input) {
	Ref<InputEventMouse> me = p_input;

	if (me.is_valid()) {
		Vector2 point = me->get_position();

		int nrect = -1;
		for (int i = 0; i < DOCK_SLOT_MAX; i++) {
			if (dock_select_rect[i].has_point(point)) {
				nrect = i;
				break;
			}
		}

		if (nrect != dock_select_rect_over) {
			dock_select->update();
			dock_select_rect_over = nrect;
		}

		if (nrect == -1) {
			return;
		}

		Ref<InputEventMouseButton> mb = me;

		if (mb.is_valid() && mb->get_button_index() == 1 && mb->is_pressed() && dock_popup_selected != nrect) {
			Control *dock = dock_slot[dock_popup_selected]->get_current_tab_control();
			if (dock) {
				dock_slot[dock_popup_selected]->remove_child(dock);
			}
			if (dock_slot[dock_popup_selected]->get_tab_count() == 0) {
				dock_slot[dock_popup_selected]->hide();

			} else {
				dock_slot[dock_popup_selected]->set_current_tab(0);
			}

			dock_slot[nrect]->add_child(dock);
			dock_popup_selected = nrect;
			dock_slot[nrect]->set_current_tab(dock_slot[nrect]->get_tab_count() - 1);
			dock_slot[nrect]->show();
			dock_select->update();

			for (int i = 0; i < vsplits.size(); i++) {
				bool in_use = dock_slot[i * 2 + 0]->get_tab_count() || dock_slot[i * 2 + 1]->get_tab_count();
				if (in_use) {
					vsplits[i]->show();
				} else {
					vsplits[i]->hide();
				}
			}

			if (right_l_vsplit->is_visible() || right_r_vsplit->is_visible()) {
				right_hsplit->show();
			} else {
				right_hsplit->hide();
			}

			_edit_current();
			_save_docks();
		}
	}
}

void EditorNode::_dock_popup_exit() {
	dock_select_rect_over = -1;
	dock_select->update();
}

void EditorNode::_dock_pre_popup(int p_which) {
	dock_popup_selected = p_which;
}

void EditorNode::_dock_move_left() {
	if (dock_popup_selected < 0 || dock_popup_selected >= DOCK_SLOT_MAX) {
		return;
	}
	Control *current = dock_slot[dock_popup_selected]->get_tab_control(dock_slot[dock_popup_selected]->get_current_tab());
	Control *prev = dock_slot[dock_popup_selected]->get_tab_control(dock_slot[dock_popup_selected]->get_current_tab() - 1);
	if (!current || !prev) {
		return;
	}
	dock_slot[dock_popup_selected]->move_child(current, prev->get_index());
	dock_slot[dock_popup_selected]->set_current_tab(dock_slot[dock_popup_selected]->get_current_tab() - 1);
	dock_select->update();
	_edit_current();
	_save_docks();
}

void EditorNode::_dock_move_right() {
	Control *current = dock_slot[dock_popup_selected]->get_tab_control(dock_slot[dock_popup_selected]->get_current_tab());
	Control *next = dock_slot[dock_popup_selected]->get_tab_control(dock_slot[dock_popup_selected]->get_current_tab() + 1);
	if (!current || !next) {
		return;
	}
	dock_slot[dock_popup_selected]->move_child(next, current->get_index());
	dock_slot[dock_popup_selected]->set_current_tab(dock_slot[dock_popup_selected]->get_current_tab() + 1);
	dock_select->update();
	_edit_current();
	_save_docks();
}

void EditorNode::_dock_select_draw() {
	Size2 s = dock_select->get_size();
	s.y /= 2.0;
	s.x /= 6.0;

	Color used = Color(0.6, 0.6, 0.6, 0.8);
	Color used_selected = Color(0.8, 0.8, 0.8, 0.8);
	Color tab_selected = theme_base->get_color("mono_color", "Editor");
	Color unused = used;
	unused.a = 0.4;
	Color unusable = unused;
	unusable.a = 0.1;

	Rect2 unr(s.x * 2, 0, s.x * 2, s.y * 2);
	unr.position += Vector2(2, 5);
	unr.size -= Vector2(4, 7);

	dock_select->draw_rect(unr, unusable);

	dock_tab_move_left->set_disabled(true);
	dock_tab_move_right->set_disabled(true);

	if (dock_popup_selected != -1 && dock_slot[dock_popup_selected]->get_tab_count()) {
		dock_tab_move_left->set_disabled(dock_slot[dock_popup_selected]->get_current_tab() == 0);
		dock_tab_move_right->set_disabled(dock_slot[dock_popup_selected]->get_current_tab() >= dock_slot[dock_popup_selected]->get_tab_count() - 1);
	}

	for (int i = 0; i < DOCK_SLOT_MAX; i++) {
		Vector2 ofs;

		switch (i) {
			case DOCK_SLOT_LEFT_UL: {
			} break;
			case DOCK_SLOT_LEFT_BL: {
				ofs.y += s.y;
			} break;
			case DOCK_SLOT_LEFT_UR: {
				ofs.x += s.x;
			} break;
			case DOCK_SLOT_LEFT_BR: {
				ofs += s;
			} break;
			case DOCK_SLOT_RIGHT_UL: {
				ofs.x += s.x * 4;
			} break;
			case DOCK_SLOT_RIGHT_BL: {
				ofs.x += s.x * 4;
				ofs.y += s.y;

			} break;
			case DOCK_SLOT_RIGHT_UR: {
				ofs.x += s.x * 4;
				ofs.x += s.x;

			} break;
			case DOCK_SLOT_RIGHT_BR: {
				ofs.x += s.x * 4;
				ofs += s;

			} break;
		}

		Rect2 r(ofs, s);
		dock_select_rect[i] = r;
		r.position += Vector2(2, 5);
		r.size -= Vector2(4, 7);

		if (i == dock_select_rect_over) {
			dock_select->draw_rect(r, used_selected);
		} else if (dock_slot[i]->get_child_count() == 0) {
			dock_select->draw_rect(r, unused);
		} else {
			dock_select->draw_rect(r, used);
		}

		for (int j = 0; j < MIN(3, dock_slot[i]->get_child_count()); j++) {
			int xofs = (r.size.width / 3) * j;
			Color c = used;
			if (i == dock_popup_selected && (dock_slot[i]->get_current_tab() > 3 || dock_slot[i]->get_current_tab() == j)) {
				c = tab_selected;
			}
			dock_select->draw_rect(Rect2(2 + ofs.x + xofs, ofs.y, r.size.width / 3 - 1, 3), c);
		}
	}
}

void EditorNode::_save_docks() {
	if (waiting_for_first_scan) {
		return; //scanning, do not touch docks
	}
	Ref<ConfigFile> config;
	config.instance();

	_save_docks_to_config(config, "docks");
	_save_open_scenes_to_config(config, "EditorNode");
	editor_data.get_plugin_window_layout(config);

	config->save(EditorSettings::get_singleton()->get_project_settings_dir().plus_file("editor_layout.cfg"));
}

void EditorNode::_save_docks_to_config(Ref<ConfigFile> p_layout, const String &p_section) {
	for (int i = 0; i < DOCK_SLOT_MAX; i++) {
		String names;
		for (int j = 0; j < dock_slot[i]->get_tab_count(); j++) {
			String name = dock_slot[i]->get_tab_control(j)->get_name();
			if (names != "") {
				names += ",";
			}
			names += name;
		}

		if (names != "") {
			p_layout->set_value(p_section, "dock_" + itos(i + 1), names);
		}
	}

	p_layout->set_value(p_section, "dock_filesystem_split", filesystem_dock->get_split_offset());
	p_layout->set_value(p_section, "dock_filesystem_display_mode", filesystem_dock->get_display_mode());
	p_layout->set_value(p_section, "dock_filesystem_file_list_display_mode", filesystem_dock->get_file_list_display_mode());

	for (int i = 0; i < vsplits.size(); i++) {
		if (vsplits[i]->is_visible_in_tree()) {
			p_layout->set_value(p_section, "dock_split_" + itos(i + 1), vsplits[i]->get_split_offset());
		}
	}

	for (int i = 0; i < hsplits.size(); i++) {
		p_layout->set_value(p_section, "dock_hsplit_" + itos(i + 1), hsplits[i]->get_split_offset());
	}
}

void EditorNode::_save_open_scenes_to_config(Ref<ConfigFile> p_layout, const String &p_section) {
	Array scenes;
	for (int i = 0; i < editor_data.get_edited_scene_count(); i++) {
		String path = editor_data.get_scene_path(i);
		if (path == "") {
			continue;
		}
		scenes.push_back(path);
	}
	p_layout->set_value(p_section, "open_scenes", scenes);
}

void EditorNode::save_layout() {
	dock_drag_timer->start();
}

void EditorNode::_dock_split_dragged(int ofs) {
	dock_drag_timer->start();
}

void EditorNode::_load_docks() {
	Ref<ConfigFile> config;
	config.instance();
	Error err = config->load(EditorSettings::get_singleton()->get_project_settings_dir().plus_file("editor_layout.cfg"));
	if (err != OK) {
		//no config
		if (overridden_default_layout >= 0) {
			_layout_menu_option(overridden_default_layout);
		}
		return;
	}

	_load_docks_from_config(config, "docks");
	_load_open_scenes_from_config(config, "EditorNode");

	editor_data.set_plugin_window_layout(config);
}

void EditorNode::_update_dock_slots_visibility(bool p_keep_selected_tabs) {
	if (!docks_visible) {
		for (int i = 0; i < DOCK_SLOT_MAX; i++) {
			dock_slot[i]->hide();
		}

		for (int i = 0; i < vsplits.size(); i++) {
			vsplits[i]->hide();
		}

		right_hsplit->hide();
	} else {
		for (int i = 0; i < DOCK_SLOT_MAX; i++) {
			int tabs_visible = 0;
			for (int j = 0; j < dock_slot[i]->get_tab_count(); j++) {
				if (!dock_slot[i]->get_tab_hidden(j)) {
					tabs_visible++;
				}
			}
			if (tabs_visible) {
				dock_slot[i]->show();
			} else {
				dock_slot[i]->hide();
			}
		}

		for (int i = 0; i < vsplits.size(); i++) {
			bool in_use = dock_slot[i * 2 + 0]->get_tab_count() || dock_slot[i * 2 + 1]->get_tab_count();
			if (in_use) {
				vsplits[i]->show();
			} else {
				vsplits[i]->hide();
			}
		}

		if (!p_keep_selected_tabs) {
			for (int i = 0; i < DOCK_SLOT_MAX; i++) {
				if (dock_slot[i]->is_visible() && dock_slot[i]->get_tab_count()) {
					dock_slot[i]->set_current_tab(0);
				}
			}
		}

		if (right_l_vsplit->is_visible() || right_r_vsplit->is_visible()) {
			right_hsplit->show();
		} else {
			right_hsplit->hide();
		}
	}
}

void EditorNode::_dock_tab_changed(int p_tab) {
	// update visibility but don't set current tab

	if (!docks_visible) {
		for (int i = 0; i < DOCK_SLOT_MAX; i++) {
			dock_slot[i]->hide();
		}

		for (int i = 0; i < vsplits.size(); i++) {
			vsplits[i]->hide();
		}

		right_hsplit->hide();
		bottom_panel->hide();
	} else {
		for (int i = 0; i < DOCK_SLOT_MAX; i++) {
			if (dock_slot[i]->get_tab_count()) {
				dock_slot[i]->show();
			} else {
				dock_slot[i]->hide();
			}
		}

		for (int i = 0; i < vsplits.size(); i++) {
			bool in_use = dock_slot[i * 2 + 0]->get_tab_count() || dock_slot[i * 2 + 1]->get_tab_count();
			if (in_use) {
				vsplits[i]->show();
			} else {
				vsplits[i]->hide();
			}
		}
		bottom_panel->show();

		if (right_l_vsplit->is_visible() || right_r_vsplit->is_visible()) {
			right_hsplit->show();
		} else {
			right_hsplit->hide();
		}
	}
}

void EditorNode::_load_docks_from_config(Ref<ConfigFile> p_layout, const String &p_section) {
	for (int i = 0; i < DOCK_SLOT_MAX; i++) {
		if (!p_layout->has_section_key(p_section, "dock_" + itos(i + 1))) {
			continue;
		}

		Vector<String> names = String(p_layout->get_value(p_section, "dock_" + itos(i + 1))).split(",");

		for (int j = 0; j < names.size(); j++) {
			String name = names[j];
			//find it, in a horribly inefficient way
			int atidx = -1;
			Control *node = nullptr;
			for (int k = 0; k < DOCK_SLOT_MAX; k++) {
				if (!dock_slot[k]->has_node(name)) {
					continue;
				}
				node = Object::cast_to<Control>(dock_slot[k]->get_node(name));
				if (!node) {
					continue;
				}
				atidx = k;
				break;
			}
			if (atidx == -1) { //well, it's not anywhere
				continue;
			}

			if (atidx == i) {
				node->raise();
				continue;
			}

			dock_slot[atidx]->remove_child(node);

			if (dock_slot[atidx]->get_tab_count() == 0) {
				dock_slot[atidx]->hide();
			}
			dock_slot[i]->add_child(node);
			dock_slot[i]->show();
		}
	}

	if (p_layout->has_section_key(p_section, "dock_filesystem_split")) {
		int fs_split_ofs = p_layout->get_value(p_section, "dock_filesystem_split");
		filesystem_dock->set_split_offset(fs_split_ofs);
	}

	if (p_layout->has_section_key(p_section, "dock_filesystem_display_mode")) {
		FileSystemDock::DisplayMode dock_filesystem_display_mode = FileSystemDock::DisplayMode(int(p_layout->get_value(p_section, "dock_filesystem_display_mode")));
		filesystem_dock->set_display_mode(dock_filesystem_display_mode);
	}

	if (p_layout->has_section_key(p_section, "dock_filesystem_file_list_display_mode")) {
		FileSystemDock::FileListDisplayMode dock_filesystem_file_list_display_mode = FileSystemDock::FileListDisplayMode(int(p_layout->get_value(p_section, "dock_filesystem_file_list_display_mode")));
		filesystem_dock->set_file_list_display_mode(dock_filesystem_file_list_display_mode);
	}

	for (int i = 0; i < vsplits.size(); i++) {
		if (!p_layout->has_section_key(p_section, "dock_split_" + itos(i + 1))) {
			continue;
		}

		int ofs = p_layout->get_value(p_section, "dock_split_" + itos(i + 1));
		vsplits[i]->set_split_offset(ofs);
	}

	for (int i = 0; i < hsplits.size(); i++) {
		if (!p_layout->has_section_key(p_section, "dock_hsplit_" + itos(i + 1))) {
			continue;
		}
		int ofs = p_layout->get_value(p_section, "dock_hsplit_" + itos(i + 1));
		hsplits[i]->set_split_offset(ofs);
	}

	for (int i = 0; i < vsplits.size(); i++) {
		bool in_use = dock_slot[i * 2 + 0]->get_tab_count() || dock_slot[i * 2 + 1]->get_tab_count();
		if (in_use) {
			vsplits[i]->show();
		} else {
			vsplits[i]->hide();
		}
	}

	if (right_l_vsplit->is_visible() || right_r_vsplit->is_visible()) {
		right_hsplit->show();
	} else {
		right_hsplit->hide();
	}

	for (int i = 0; i < DOCK_SLOT_MAX; i++) {
		if (dock_slot[i]->is_visible() && dock_slot[i]->get_tab_count()) {
			dock_slot[i]->set_current_tab(0);
		}
	}
}

void EditorNode::_load_open_scenes_from_config(Ref<ConfigFile> p_layout, const String &p_section) {
	if (!bool(EDITOR_GET("interface/scene_tabs/restore_scenes_on_load"))) {
		return;
	}

	if (!p_layout->has_section(p_section) || !p_layout->has_section_key(p_section, "open_scenes")) {
		return;
	}

	restoring_scenes = true;

	Array scenes = p_layout->get_value(p_section, "open_scenes");
	for (int i = 0; i < scenes.size(); i++) {
		load_scene(scenes[i]);
	}
	save_layout();

	restoring_scenes = false;
}

bool EditorNode::has_scenes_in_session() {
	if (!bool(EDITOR_GET("interface/scene_tabs/restore_scenes_on_load"))) {
		return false;
	}
	Ref<ConfigFile> config;
	config.instance();
	Error err = config->load(EditorSettings::get_singleton()->get_project_settings_dir().plus_file("editor_layout.cfg"));
	if (err != OK) {
		return false;
	}
	if (!config->has_section("EditorNode") || !config->has_section_key("EditorNode", "open_scenes")) {
		return false;
	}
	Array scenes = config->get_value("EditorNode", "open_scenes");
	return !scenes.empty();
}

bool EditorNode::ensure_main_scene(bool p_from_native) {
	pick_main_scene->set_meta("from_native", p_from_native); //whether from play button or native run
	String main_scene = GLOBAL_DEF("application/run/main_scene", "");

	if (main_scene == "") {
		current_option = -1;
		pick_main_scene->set_text(TTR("No main scene has ever been defined, select one?\nYou can change it later in \"Project Settings\" under the 'application' category."));
		pick_main_scene->popup_centered_minsize();

		if (editor_data.get_edited_scene_root()) {
			select_current_scene_button->set_disabled(false);
			select_current_scene_button->grab_focus();
		} else {
			select_current_scene_button->set_disabled(true);
		}

		return false;
	}

	if (!FileAccess::exists(main_scene)) {
		current_option = -1;
		pick_main_scene->set_text(vformat(TTR("Selected scene '%s' does not exist, select a valid one?\nYou can change it later in \"Project Settings\" under the 'application' category."), main_scene));
		pick_main_scene->popup_centered_minsize();
		return false;
	}

	if (ResourceLoader::get_resource_type(main_scene) != "PackedScene") {
		current_option = -1;
		pick_main_scene->set_text(vformat(TTR("Selected scene '%s' is not a scene file, select a valid one?\nYou can change it later in \"Project Settings\" under the 'application' category."), main_scene));
		pick_main_scene->popup_centered_minsize();
		return false;
	}

	return true;
}

void EditorNode::run_play() {
	_menu_option_confirm(RUN_STOP, true);
	_run(false);
}

void EditorNode::run_play_current() {
	_save_default_environment();
	_menu_option_confirm(RUN_STOP, true);
	_run(true);
}

void EditorNode::run_play_custom(const String &p_custom) {
	_menu_option_confirm(RUN_STOP, true);
	_run(false, p_custom);
}

void EditorNode::run_stop() {
	_menu_option_confirm(RUN_STOP, false);
}

bool EditorNode::is_run_playing() const {
	EditorRun::Status status = editor_run.get_status();
	return (status == EditorRun::STATUS_PLAY || status == EditorRun::STATUS_PAUSED);
}

String EditorNode::get_run_playing_scene() const {
	String run_filename = editor_run.get_running_scene();
	if (run_filename == "" && is_run_playing()) {
		run_filename = GLOBAL_DEF("application/run/main_scene", ""); // Must be the main scene then.
	}

	return run_filename;
}

int EditorNode::get_current_tab() {
	return scene_tabs->get_current_tab();
}

void EditorNode::set_current_tab(int p_tab) {
	scene_tabs->set_current_tab(p_tab);
}

void EditorNode::_update_layouts_menu() {
	editor_layouts->clear();
	overridden_default_layout = -1;

	editor_layouts->set_size(Vector2());
	editor_layouts->add_shortcut(ED_SHORTCUT("layout/save", TTR("Save Layout")), SETTINGS_LAYOUT_SAVE);
	editor_layouts->add_shortcut(ED_SHORTCUT("layout/delete", TTR("Delete Layout")), SETTINGS_LAYOUT_DELETE);
	editor_layouts->add_separator();
	editor_layouts->add_shortcut(ED_SHORTCUT("layout/default", TTR("Default")), SETTINGS_LAYOUT_DEFAULT);

	Ref<ConfigFile> config;
	config.instance();
	Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());
	if (err != OK) {
		return; //no config
	}

	List<String> layouts;
	config.ptr()->get_sections(&layouts);

	for (List<String>::Element *E = layouts.front(); E; E = E->next()) {
		String layout = E->get();

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
			current_option = p_id;
			layout_dialog->set_title(TTR("Save Layout"));
			layout_dialog->get_ok()->set_text(TTR("Save"));
			layout_dialog->popup_centered();
			layout_dialog->set_name_line_enabled(true);
		} break;
		case SETTINGS_LAYOUT_DELETE: {
			current_option = p_id;
			layout_dialog->set_title(TTR("Delete Layout"));
			layout_dialog->get_ok()->set_text(TTR("Delete"));
			layout_dialog->popup_centered();
			layout_dialog->set_name_line_enabled(false);
		} break;
		case SETTINGS_LAYOUT_DEFAULT: {
			_load_docks_from_config(default_layout, "docks");
			_save_docks();
		} break;
		default: {
			Ref<ConfigFile> config;
			config.instance();
			Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());
			if (err != OK) {
				return; //no config
			}

			_load_docks_from_config(config, editor_layouts->get_item_text(p_id));
			_save_docks();
		}
	}
}

void EditorNode::_scene_tab_script_edited(int p_tab) {
	Ref<Script> script = editor_data.get_scene_root_script(p_tab);
	if (script.is_valid()) {
		inspector_dock->edit_resource(script);
	}
}

void EditorNode::_scene_tab_closed(int p_tab, int option) {
	current_option = option;
	tab_closing = p_tab;
	Node *scene = editor_data.get_edited_scene_root(p_tab);
	if (!scene) {
		_discard_changes();
		return;
	}

	bool unsaved = (p_tab == editor_data.get_edited_scene())
			? saved_version != editor_data.get_undo_redo().get_version()
			: editor_data.get_scene_version(p_tab) != 0;
	if (unsaved) {
		save_confirmation->get_ok()->set_text(TTR("Save & Close"));
		save_confirmation->set_text(vformat(TTR("Save changes to '%s' before closing?"), scene->get_filename() != "" ? scene->get_filename() : "unsaved scene"));
		save_confirmation->popup_centered_minsize();
	} else {
		_discard_changes();
	}

	save_layout();
	_update_scene_tabs();
}

void EditorNode::_scene_tab_hover(int p_tab) {
	if (!bool(EDITOR_GET("interface/scene_tabs/show_thumbnail_on_hover"))) {
		return;
	}
	int current_tab = scene_tabs->get_current_tab();

	if (p_tab == current_tab || p_tab < 0) {
		tab_preview_panel->hide();
	} else {
		String path = editor_data.get_scene_path(p_tab);
		if (path != String()) {
			EditorResourcePreview::get_singleton()->queue_resource_preview(path, this, "_thumbnail_done", p_tab);
		}
	}
}

void EditorNode::_scene_tab_exit() {
	tab_preview_panel->hide();
}

void EditorNode::_scene_tab_input(const Ref<InputEvent> &p_input) {
	Ref<InputEventMouseButton> mb = p_input;

	if (mb.is_valid()) {
		if (scene_tabs->get_hovered_tab() >= 0) {
			if (mb->get_button_index() == BUTTON_MIDDLE && mb->is_pressed()) {
				_scene_tab_closed(scene_tabs->get_hovered_tab());
			}
		} else {
			if ((mb->get_button_index() == BUTTON_LEFT && mb->is_doubleclick()) || (mb->get_button_index() == BUTTON_MIDDLE && mb->is_pressed())) {
				_menu_option_confirm(FILE_NEW_SCENE, true);
			}
		}
		if (mb->get_button_index() == BUTTON_RIGHT && mb->is_pressed()) {
			// context menu
			scene_tabs_context_menu->clear();
			scene_tabs_context_menu->set_size(Size2(1, 1));

			scene_tabs_context_menu->add_shortcut(ED_GET_SHORTCUT("editor/new_scene"), FILE_NEW_SCENE);
			if (scene_tabs->get_hovered_tab() >= 0) {
				scene_tabs_context_menu->add_shortcut(ED_GET_SHORTCUT("editor/save_scene"), FILE_SAVE_SCENE);
				scene_tabs_context_menu->add_shortcut(ED_GET_SHORTCUT("editor/save_scene_as"), FILE_SAVE_AS_SCENE);
			}
			scene_tabs_context_menu->add_shortcut(ED_GET_SHORTCUT("editor/save_all_scenes"), FILE_SAVE_ALL_SCENES);
			if (scene_tabs->get_hovered_tab() >= 0) {
				scene_tabs_context_menu->add_separator();
				scene_tabs_context_menu->add_item(TTR("Show in FileSystem"), FILE_SHOW_IN_FILESYSTEM);
				scene_tabs_context_menu->add_item(TTR("Play This Scene"), RUN_PLAY_SCENE);

				scene_tabs_context_menu->add_separator();
				Ref<ShortCut> close_tab_sc = ED_GET_SHORTCUT("editor/close_scene");
				close_tab_sc->set_name(TTR("Close Tab"));
				scene_tabs_context_menu->add_shortcut(close_tab_sc, FILE_CLOSE);
				Ref<ShortCut> undo_close_tab_sc = ED_GET_SHORTCUT("editor/reopen_closed_scene");
				undo_close_tab_sc->set_name(TTR("Undo Close Tab"));
				scene_tabs_context_menu->add_shortcut(undo_close_tab_sc, FILE_OPEN_PREV);
				if (previous_scenes.empty()) {
					scene_tabs_context_menu->set_item_disabled(scene_tabs_context_menu->get_item_index(FILE_OPEN_PREV), true);
				}
				scene_tabs_context_menu->add_item(TTR("Close Other Tabs"), FILE_CLOSE_OTHERS);
				scene_tabs_context_menu->add_item(TTR("Close Tabs to the Right"), FILE_CLOSE_RIGHT);
				scene_tabs_context_menu->add_item(TTR("Close All Tabs"), FILE_CLOSE_ALL);
			}
			scene_tabs_context_menu->set_position(mb->get_global_position());
			scene_tabs_context_menu->popup();
		}
		if (mb->get_button_index() == BUTTON_WHEEL_UP && mb->is_pressed()) {
			int previous_tab = editor_data.get_edited_scene() - 1;
			previous_tab = previous_tab >= 0 ? previous_tab : editor_data.get_edited_scene_count() - 1;
			_scene_tab_changed(previous_tab);
		}
		if (mb->get_button_index() == BUTTON_WHEEL_DOWN && mb->is_pressed()) {
			int next_tab = editor_data.get_edited_scene() + 1;
			next_tab %= editor_data.get_edited_scene_count();
			_scene_tab_changed(next_tab);
		}
	}
}

void EditorNode::_reposition_active_tab(int idx_to) {
	editor_data.move_edited_scene_to_index(idx_to);
	_update_scene_tabs();
}

void EditorNode::_thumbnail_done(const String &p_path, const Ref<Texture> &p_preview, const Ref<Texture> &p_small_preview, const Variant &p_udata) {
	int p_tab = p_udata.operator signed int();
	if (p_preview.is_valid()) {
		Rect2 rect = scene_tabs->get_tab_rect(p_tab);
		rect.position += scene_tabs->get_global_position();
		tab_preview->set_texture(p_preview);
		tab_preview_panel->set_position(rect.position + Vector2(0, rect.size.height));
		tab_preview_panel->show();
	}
}

void EditorNode::_scene_tab_changed(int p_tab) {
	tab_preview_panel->hide();

	bool unsaved = (saved_version != editor_data.get_undo_redo().get_version());

	if (p_tab == editor_data.get_edited_scene()) {
		return; //pointless
	}

	uint64_t next_scene_version = editor_data.get_scene_version(p_tab);

	editor_data.get_undo_redo().create_action(TTR("Switch Scene Tab"));
	editor_data.get_undo_redo().add_do_method(this, "set_current_version", unsaved ? saved_version : 0);
	editor_data.get_undo_redo().add_do_method(this, "set_current_scene", p_tab);
	editor_data.get_undo_redo().add_do_method(this, "set_current_version", next_scene_version == 0 ? editor_data.get_undo_redo().get_version() + 1 : next_scene_version);

	editor_data.get_undo_redo().add_undo_method(this, "set_current_version", next_scene_version);
	editor_data.get_undo_redo().add_undo_method(this, "set_current_scene", editor_data.get_edited_scene());
	editor_data.get_undo_redo().add_undo_method(this, "set_current_version", saved_version);
	editor_data.get_undo_redo().commit_action();
}

ToolButton *EditorNode::add_bottom_panel_item(String p_text, Control *p_item) {
	ToolButton *tb = memnew(ToolButton);
	tb->connect("toggled", this, "_bottom_panel_switch", varray(bottom_panel_items.size()));
	tb->set_text(p_text);
	tb->set_toggle_mode(true);
	tb->set_focus_mode(Control::FOCUS_NONE);
	bottom_panel_vb->add_child(p_item);
	bottom_panel_hb->raise();
	bottom_panel_hb_editors->add_child(tb);
	p_item->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	p_item->hide();
	BottomPanelItem bpi;
	bpi.button = tb;
	bpi.control = p_item;
	bpi.name = p_text;
	bottom_panel_items.push_back(bpi);

	return tb;
}

bool EditorNode::are_bottom_panels_hidden() const {
	for (int i = 0; i < bottom_panel_items.size(); i++) {
		if (bottom_panel_items[i].button->is_pressed()) {
			return false;
		}
	}

	return true;
}

void EditorNode::hide_bottom_panel() {
	for (int i = 0; i < bottom_panel_items.size(); i++) {
		if (bottom_panel_items[i].control->is_visible()) {
			_bottom_panel_switch(false, i);
			break;
		}
	}
}

void EditorNode::make_bottom_panel_item_visible(Control *p_item) {
	for (int i = 0; i < bottom_panel_items.size(); i++) {
		if (bottom_panel_items[i].control == p_item) {
			_bottom_panel_switch(true, i);
			break;
		}
	}
}

void EditorNode::raise_bottom_panel_item(Control *p_item) {
	for (int i = 0; i < bottom_panel_items.size(); i++) {
		if (bottom_panel_items[i].control == p_item) {
			bottom_panel_items[i].button->raise();
			SWAP(bottom_panel_items.write[i], bottom_panel_items.write[bottom_panel_items.size() - 1]);
			break;
		}
	}

	for (int i = 0; i < bottom_panel_items.size(); i++) {
		bottom_panel_items[i].button->disconnect("toggled", this, "_bottom_panel_switch");
		bottom_panel_items[i].button->connect("toggled", this, "_bottom_panel_switch", varray(i));
	}
}

void EditorNode::remove_bottom_panel_item(Control *p_item) {
	for (int i = 0; i < bottom_panel_items.size(); i++) {
		if (bottom_panel_items[i].control == p_item) {
			if (p_item->is_visible_in_tree()) {
				_bottom_panel_switch(false, i);
			}
			bottom_panel_vb->remove_child(bottom_panel_items[i].control);
			bottom_panel_hb_editors->remove_child(bottom_panel_items[i].button);
			memdelete(bottom_panel_items[i].button);
			bottom_panel_items.remove(i);
			break;
		}
	}

	for (int i = 0; i < bottom_panel_items.size(); i++) {
		bottom_panel_items[i].button->disconnect("toggled", this, "_bottom_panel_switch");
		bottom_panel_items[i].button->connect("toggled", this, "_bottom_panel_switch", varray(i));
	}
}

void EditorNode::_bottom_panel_switch(bool p_enable, int p_idx) {
	ERR_FAIL_INDEX(p_idx, bottom_panel_items.size());

	if (bottom_panel_items[p_idx].control->is_visible() == p_enable) {
		return;
	}

	if (p_enable) {
		for (int i = 0; i < bottom_panel_items.size(); i++) {
			bottom_panel_items[i].button->set_pressed(i == p_idx);
			bottom_panel_items[i].control->set_visible(i == p_idx);
		}
		if (ScriptEditor::get_singleton()->get_debugger() == bottom_panel_items[p_idx].control) { // this is the debug panel which uses tabs, so the top section should be smaller
			bottom_panel->add_style_override("panel", gui_base->get_stylebox("BottomPanelDebuggerOverride", "EditorStyles"));
		} else {
			bottom_panel->add_style_override("panel", gui_base->get_stylebox("panel", "TabContainer"));
		}
		center_split->set_dragger_visibility(SplitContainer::DRAGGER_VISIBLE);
		center_split->set_collapsed(false);
		if (bottom_panel_raise->is_pressed()) {
			top_split->hide();
		}
		bottom_panel_raise->show();

	} else {
		bottom_panel->add_style_override("panel", gui_base->get_stylebox("panel", "TabContainer"));
		bottom_panel_items[p_idx].button->set_pressed(false);
		bottom_panel_items[p_idx].control->set_visible(false);
		center_split->set_dragger_visibility(SplitContainer::DRAGGER_HIDDEN);
		center_split->set_collapsed(true);
		bottom_panel_raise->hide();
		if (bottom_panel_raise->is_pressed()) {
			top_split->show();
		}
	}
}

void EditorNode::set_docks_visible(bool p_show) {
	docks_visible = p_show;
	_update_dock_slots_visibility(true);
}

bool EditorNode::get_docks_visible() const {
	return docks_visible;
}

void EditorNode::_toggle_distraction_free_mode() {
	if (EditorSettings::get_singleton()->get("interface/editor/separate_distraction_mode")) {
		int screen = -1;
		for (int i = 0; i < editor_table.size(); i++) {
			if (editor_plugin_screen == editor_table[i]) {
				screen = i;
				break;
			}
		}

		if (screen == EDITOR_SCRIPT) {
			script_distraction = !script_distraction;
			set_distraction_free_mode(script_distraction);
		} else {
			scene_distraction = !scene_distraction;
			set_distraction_free_mode(scene_distraction);
		}
	} else {
		set_distraction_free_mode(distraction_free->is_pressed());
	}
}

void EditorNode::set_distraction_free_mode(bool p_enter) {
	distraction_free->set_pressed(p_enter);

	if (p_enter) {
		if (docks_visible) {
			set_docks_visible(false);
		}
	} else {
		set_docks_visible(true);
	}
}

bool EditorNode::is_distraction_free_mode_enabled() const {
	return distraction_free->is_pressed();
}

void EditorNode::add_control_to_dock(DockSlot p_slot, Control *p_control) {
	ERR_FAIL_INDEX(p_slot, DOCK_SLOT_MAX);
	dock_slot[p_slot]->add_child(p_control);
	_update_dock_slots_visibility();
}

void EditorNode::remove_control_from_dock(Control *p_control) {
	Control *dock = nullptr;
	for (int i = 0; i < DOCK_SLOT_MAX; i++) {
		if (p_control->get_parent() == dock_slot[i]) {
			dock = dock_slot[i];
			break;
		}
	}

	ERR_FAIL_COND_MSG(!dock, "Control was not in dock.");

	dock->remove_child(p_control);
	_update_dock_slots_visibility();
}

Variant EditorNode::drag_resource(const Ref<Resource> &p_res, Control *p_from) {
	Control *drag_control = memnew(Control);
	TextureRect *drag_preview = memnew(TextureRect);
	Label *label = memnew(Label);

	Ref<Texture> preview;

	{
		//todo make proper previews
		Ref<ImageTexture> pic = gui_base->get_icon("FileBigThumb", "EditorIcons");
		Ref<Image> img = pic->get_data();
		img = img->duplicate();
		img->resize(48, 48); //meh
		Ref<ImageTexture> resized_pic = Ref<ImageTexture>(memnew(ImageTexture));
		resized_pic->create_from_image(img);
		preview = resized_pic;
	}

	drag_preview->set_texture(preview);
	drag_control->add_child(drag_preview);
	if (p_res->get_path().is_resource_file()) {
		label->set_text(p_res->get_path().get_file());
	} else if (p_res->get_name() != "") {
		label->set_text(p_res->get_name());
	} else {
		label->set_text(p_res->get_class());
	}

	drag_control->add_child(label);

	p_from->set_drag_preview(drag_control); //wait until it enters scene

	label->set_position(Point2((preview->get_width() - label->get_minimum_size().width) / 2, preview->get_height()));

	Dictionary drag_data;
	drag_data["type"] = "resource";
	drag_data["resource"] = p_res;
	drag_data["from"] = p_from;

	return drag_data;
}

Variant EditorNode::drag_files_and_dirs(const Vector<String> &p_paths, Control *p_from) {
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

		if (p_paths[i].ends_with("/")) {
			label->set_text(p_paths[i].substr(0, p_paths[i].length() - 1).get_file());
			icon->set_texture(gui_base->get_icon("Folder", "EditorIcons"));
		} else {
			label->set_text(p_paths[i].get_file());
			icon->set_texture(gui_base->get_icon("File", "EditorIcons"));
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
	p_from->set_drag_preview(vbox); //wait until it enters scene

	Dictionary drag_data;
	drag_data["type"] = has_folder ? "files_and_dirs" : "files";
	drag_data["files"] = p_paths;
	drag_data["from"] = p_from;
	return drag_data;
}

void EditorNode::add_tool_menu_item(const String &p_name, Object *p_handler, const String &p_callback, const Variant &p_ud) {
	ERR_FAIL_NULL(p_handler);
	int idx = tool_menu->get_item_count();
	tool_menu->add_item(p_name, TOOLS_CUSTOM);

	Array parameters;
	parameters.push_back(p_handler->get_instance_id());
	parameters.push_back(p_callback);
	parameters.push_back(p_ud);

	tool_menu->set_item_metadata(idx, parameters);
}

void EditorNode::add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu) {
	ERR_FAIL_NULL(p_submenu);
	ERR_FAIL_COND(p_submenu->get_parent() != nullptr);

	tool_menu->add_child(p_submenu);
	tool_menu->add_submenu_item(p_name, p_submenu->get_name(), TOOLS_CUSTOM);
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
			tool_menu->set_as_minsize();
			return;
		}
	}
}

void EditorNode::_global_menu_action(const Variant &p_id, const Variant &p_meta) {
	int id = (int)p_id;
	if (id == GLOBAL_NEW_WINDOW) {
		if (OS::get_singleton()->get_main_loop()) {
			List<String> args;
			args.push_back("-p");
			String exec = OS::get_singleton()->get_executable_path();

			OS::ProcessID pid = 0;
			OS::get_singleton()->execute(exec, args, false, &pid);
		}
	} else if (id == GLOBAL_SCENE) {
		int idx = (int)p_meta;
		scene_tabs->set_current_tab(idx);
	}
}

void EditorNode::_dropped_files(const Vector<String> &p_files, int p_screen) {
	String to_path = ProjectSettings::get_singleton()->globalize_path(get_filesystem_dock()->get_selected_path());

	_add_dropped_files_recursive(p_files, to_path);

	EditorFileSystem::get_singleton()->scan_changes();
}

void EditorNode::_add_dropped_files_recursive(const Vector<String> &p_files, String to_path) {
	DirAccessRef dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	for (int i = 0; i < p_files.size(); i++) {
		String from = p_files[i];
		String to = to_path.plus_file(from.get_file());

		if (dir->dir_exists(from)) {
			Vector<String> sub_files;

			DirAccessRef sub_dir = DirAccess::open(from);
			sub_dir->list_dir_begin();

			String next_file = sub_dir->get_next();
			while (next_file != "") {
				if (next_file == "." || next_file == "..") {
					next_file = sub_dir->get_next();
					continue;
				}

				sub_files.push_back(from.plus_file(next_file));
				next_file = sub_dir->get_next();
			}

			if (!sub_files.empty()) {
				dir->make_dir(to);
				_add_dropped_files_recursive(sub_files, to);
			}

			continue;
		}

		dir->copy(from, to);
	}
}

void EditorNode::_file_access_close_error_notify(const String &p_str) {
	add_io_error(vformat(TTR("Unable to write to file '%s', file in use, locked or lacking permissions."), p_str));
}

void EditorNode::reload_scene(const String &p_path) {
	//first of all, reload internal textures, materials, meshes, etc. as they might have changed on disk

	List<Ref<Resource>> cached;
	ResourceCache::get_cached_resources(&cached);
	List<Ref<Resource>> to_clear; //clear internal resources from previous scene from being used
	for (List<Ref<Resource>>::Element *E = cached.front(); E; E = E->next()) {
		if (E->get()->get_path().begins_with(p_path + "::")) { //subresources of existing scene
			to_clear.push_back(E->get());
		}
	}

	//so reload reloads everything, clear subresources of previous scene
	while (to_clear.front()) {
		to_clear.front()->get()->set_path("");
		to_clear.pop_front();
	}

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
			//scene is not open, so at it might be instanced. We'll refresh the whole scene later.
			editor_data.get_undo_redo().clear_history();
		}
		return;
	}

	if (current_tab == scene_idx) {
		editor_data.apply_changes_in_editors();
		_set_scene_metadata(p_path);
	}

	//remove scene
	_remove_scene(scene_idx, false);

	//reload scene
	load_scene(p_path, true, false, true, true);

	//adjust index so tab is back a the previous position
	editor_data.move_edited_scene_to_index(scene_idx);
	get_undo_redo()->clear_history();

	//recover the tab
	scene_tabs->set_current_tab(current_tab);
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
	load_scene(open_import_request, true, false, true, true);
}

void EditorNode::dim_editor(bool p_dimming, bool p_force_dim) {
	// Dimming can be forced regardless of the editor setting, which is useful when quitting the editor.
	if ((p_force_dim || EditorSettings::get_singleton()->get("interface/editor/dim_editor_on_dialog_popup")) && p_dimming) {
		dimmed = true;
		gui_base->set_modulate(Color(0.5, 0.5, 0.5));
	} else {
		dimmed = false;
		gui_base->set_modulate(Color(1, 1, 1));
	}
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

Vector<Ref<EditorResourceConversionPlugin>> EditorNode::find_resource_conversion_plugin(const Ref<Resource> &p_for_resource) {
	Vector<Ref<EditorResourceConversionPlugin>> ret;

	for (int i = 0; i < resource_conversion_plugins.size(); i++) {
		if (resource_conversion_plugins[i].is_valid() && resource_conversion_plugins[i]->handles(p_for_resource)) {
			ret.push_back(resource_conversion_plugins[i]);
		}
	}

	return ret;
}

void EditorNode::_bottom_panel_raise_toggled(bool p_pressed) {
	top_split->set_visible(!p_pressed);
}

void EditorNode::_update_video_driver_color() {
	// TODO: Probably should de-hardcode this and add to editor settings.
	if (video_driver->get_text() == "GLES2") {
		video_driver->add_color_override("font_color", Color::hex(0x5586a4ff));
	} else if (video_driver->get_text() == "GLES3") {
		video_driver->add_color_override("font_color", Color::hex(0xa5557dff));
	}
}

void EditorNode::_video_driver_selected(int p_which) {
	String driver = video_driver->get_item_metadata(p_which);

	String current = OS::get_singleton()->get_video_driver_name(OS::get_singleton()->get_current_video_driver());

	if (driver == current) {
		return;
	}

	video_driver_request = driver;
	video_restart_dialog->popup_centered_minsize();
	video_driver->select(video_driver_current);
	_update_video_driver_color();
}

void EditorNode::_resource_saved(RES p_resource, const String &p_path) {
	if (EditorFileSystem::get_singleton()) {
		EditorFileSystem::get_singleton()->update_file(p_path);
	}

	singleton->editor_folding.save_resource_folding(p_resource, p_path);
}

void EditorNode::_resource_loaded(RES p_resource, const String &p_path) {
	singleton->editor_folding.load_resource_folding(p_resource, p_path);
}

void EditorNode::_project_settings_changed() {
	SceneTree *tree = get_tree();
	tree->set_debug_collisions_color(GLOBAL_GET("debug/shapes/collision/shape_color"));
	tree->set_debug_collision_contact_color(GLOBAL_GET("debug/shapes/collision/contact_color"));
	tree->set_debug_navigation_color(GLOBAL_GET("debug/shapes/navigation/geometry_color"));
	tree->set_debug_navigation_disabled_color(GLOBAL_GET("debug/shapes/navigation/disabled_geometry_color"));

	_update_title();
}

void EditorNode::_feature_profile_changed() {
	Ref<EditorFeatureProfile> profile = feature_profile_manager->get_current_profile();
	TabContainer *import_tabs = cast_to<TabContainer>(import_dock->get_parent());
	TabContainer *node_tabs = cast_to<TabContainer>(node_dock->get_parent());
	TabContainer *fs_tabs = cast_to<TabContainer>(filesystem_dock->get_parent());
	if (profile.is_valid()) {
		node_tabs->set_tab_hidden(node_dock->get_index(), profile->is_feature_disabled(EditorFeatureProfile::FEATURE_NODE_DOCK));
		// The Import dock is useless without the FileSystem dock. Ensure the configuration is valid.
		bool fs_dock_disabled = profile->is_feature_disabled(EditorFeatureProfile::FEATURE_FILESYSTEM_DOCK);
		fs_tabs->set_tab_hidden(filesystem_dock->get_index(), fs_dock_disabled);
		import_tabs->set_tab_hidden(import_dock->get_index(), fs_dock_disabled || profile->is_feature_disabled(EditorFeatureProfile::FEATURE_IMPORT_DOCK));

		main_editor_buttons[EDITOR_3D]->set_visible(!profile->is_feature_disabled(EditorFeatureProfile::FEATURE_3D));
		main_editor_buttons[EDITOR_SCRIPT]->set_visible(!profile->is_feature_disabled(EditorFeatureProfile::FEATURE_SCRIPT));
		if (AssetLibraryEditorPlugin::is_available()) {
			main_editor_buttons[EDITOR_ASSETLIB]->set_visible(!profile->is_feature_disabled(EditorFeatureProfile::FEATURE_ASSET_LIB));
		}
		if ((profile->is_feature_disabled(EditorFeatureProfile::FEATURE_3D) && singleton->main_editor_buttons[EDITOR_3D]->is_pressed()) ||
				(profile->is_feature_disabled(EditorFeatureProfile::FEATURE_SCRIPT) && singleton->main_editor_buttons[EDITOR_SCRIPT]->is_pressed()) ||
				(AssetLibraryEditorPlugin::is_available() && profile->is_feature_disabled(EditorFeatureProfile::FEATURE_ASSET_LIB) && singleton->main_editor_buttons[EDITOR_ASSETLIB]->is_pressed())) {
			_editor_select(EDITOR_2D);
		}
	} else {
		import_tabs->set_tab_hidden(import_dock->get_index(), false);
		node_tabs->set_tab_hidden(node_dock->get_index(), false);
		fs_tabs->set_tab_hidden(filesystem_dock->get_index(), false);
		import_dock->set_visible(true);
		node_dock->set_visible(true);
		filesystem_dock->set_visible(true);
		main_editor_buttons[EDITOR_3D]->set_visible(true);
		main_editor_buttons[EDITOR_SCRIPT]->set_visible(true);
		if (AssetLibraryEditorPlugin::is_available()) {
			main_editor_buttons[EDITOR_ASSETLIB]->set_visible(true);
		}
	}

	_update_dock_slots_visibility();
}

void EditorNode::_bind_methods() {
	ClassDB::bind_method("_menu_option", &EditorNode::_menu_option);
	ClassDB::bind_method("_tool_menu_option", &EditorNode::_tool_menu_option);
	ClassDB::bind_method("_menu_confirm_current", &EditorNode::_menu_confirm_current);
	ClassDB::bind_method("_dialog_action", &EditorNode::_dialog_action);
	ClassDB::bind_method("_editor_select", &EditorNode::_editor_select);
	ClassDB::bind_method("_node_renamed", &EditorNode::_node_renamed);
	ClassDB::bind_method("edit_node", &EditorNode::edit_node);
	ClassDB::bind_method("_unhandled_input", &EditorNode::_unhandled_input);
	ClassDB::bind_method("_update_file_menu_opened", &EditorNode::_update_file_menu_opened);
	ClassDB::bind_method("_update_file_menu_closed", &EditorNode::_update_file_menu_closed);
	ClassDB::bind_method("_android_build_source_selected", &EditorNode::_android_build_source_selected);

	ClassDB::bind_method(D_METHOD("push_item", "object", "property", "inspector_only"), &EditorNode::push_item, DEFVAL(""), DEFVAL(false));

	ClassDB::bind_method("_get_scene_metadata", &EditorNode::_get_scene_metadata);
	ClassDB::bind_method("set_edited_scene", &EditorNode::set_edited_scene);
	ClassDB::bind_method("open_request", &EditorNode::open_request);
	ClassDB::bind_method("_inherit_request", &EditorNode::_inherit_request);
	ClassDB::bind_method("_instance_request", &EditorNode::_instance_request);
	ClassDB::bind_method("_close_messages", &EditorNode::_close_messages);
	ClassDB::bind_method("_show_messages", &EditorNode::_show_messages);
	ClassDB::bind_method("_vp_resized", &EditorNode::_vp_resized);
	ClassDB::bind_method("_version_button_pressed", &EditorNode::_version_button_pressed);
	ClassDB::bind_method("_quick_opened", &EditorNode::_quick_opened);
	ClassDB::bind_method("_quick_run", &EditorNode::_quick_run);

	ClassDB::bind_method("_open_recent_scene", &EditorNode::_open_recent_scene);

	ClassDB::bind_method("stop_child_process", &EditorNode::stop_child_process);

	ClassDB::bind_method("get_script_create_dialog", &EditorNode::get_script_create_dialog);

	ClassDB::bind_method("_sources_changed", &EditorNode::_sources_changed);
	ClassDB::bind_method("_fs_changed", &EditorNode::_fs_changed);
	ClassDB::bind_method("_dock_select_draw", &EditorNode::_dock_select_draw);
	ClassDB::bind_method("_dock_select_input", &EditorNode::_dock_select_input);
	ClassDB::bind_method("_dock_pre_popup", &EditorNode::_dock_pre_popup);
	ClassDB::bind_method("_dock_split_dragged", &EditorNode::_dock_split_dragged);
	ClassDB::bind_method("_save_docks", &EditorNode::_save_docks);
	ClassDB::bind_method("_dock_popup_exit", &EditorNode::_dock_popup_exit);
	ClassDB::bind_method("_dock_move_left", &EditorNode::_dock_move_left);
	ClassDB::bind_method("_dock_move_right", &EditorNode::_dock_move_right);
	ClassDB::bind_method("_dock_tab_changed", &EditorNode::_dock_tab_changed);

	ClassDB::bind_method("_layout_menu_option", &EditorNode::_layout_menu_option);

	ClassDB::bind_method("set_current_scene", &EditorNode::set_current_scene);
	ClassDB::bind_method("set_current_version", &EditorNode::set_current_version);
	ClassDB::bind_method("_scene_tab_changed", &EditorNode::_scene_tab_changed);
	ClassDB::bind_method("_scene_tab_closed", &EditorNode::_scene_tab_closed);
	ClassDB::bind_method("_scene_tab_hover", &EditorNode::_scene_tab_hover);
	ClassDB::bind_method("_scene_tab_exit", &EditorNode::_scene_tab_exit);
	ClassDB::bind_method("_scene_tab_input", &EditorNode::_scene_tab_input);
	ClassDB::bind_method("_reposition_active_tab", &EditorNode::_reposition_active_tab);
	ClassDB::bind_method("_thumbnail_done", &EditorNode::_thumbnail_done);
	ClassDB::bind_method("_scene_tab_script_edited", &EditorNode::_scene_tab_script_edited);
	ClassDB::bind_method("_set_main_scene_state", &EditorNode::_set_main_scene_state);
	ClassDB::bind_method("_update_scene_tabs", &EditorNode::_update_scene_tabs);
	ClassDB::bind_method("_discard_changes", &EditorNode::_discard_changes);
	ClassDB::bind_method("_update_recent_scenes", &EditorNode::_update_recent_scenes);
	ClassDB::bind_method("_pick_main_scene_custom_action", &EditorNode::_pick_main_scene_custom_action);

	ClassDB::bind_method("_clear_undo_history", &EditorNode::_clear_undo_history);
	ClassDB::bind_method("_dropped_files", &EditorNode::_dropped_files);
	ClassDB::bind_method(D_METHOD("_global_menu_action"), &EditorNode::_global_menu_action, DEFVAL(Variant()));
	ClassDB::bind_method("_toggle_distraction_free_mode", &EditorNode::_toggle_distraction_free_mode);
	ClassDB::bind_method("_version_control_menu_option", &EditorNode::_version_control_menu_option);
	ClassDB::bind_method("edit_item_resource", &EditorNode::edit_item_resource);

	ClassDB::bind_method(D_METHOD("get_gui_base"), &EditorNode::get_gui_base);
	ClassDB::bind_method(D_METHOD("_bottom_panel_switch"), &EditorNode::_bottom_panel_switch);

	ClassDB::bind_method(D_METHOD("_open_imported"), &EditorNode::_open_imported);
	ClassDB::bind_method(D_METHOD("_inherit_imported"), &EditorNode::_inherit_imported);

	ClassDB::bind_method("_copy_warning", &EditorNode::_copy_warning);

	ClassDB::bind_method(D_METHOD("_resources_reimported"), &EditorNode::_resources_reimported);
	ClassDB::bind_method(D_METHOD("_bottom_panel_raise_toggled"), &EditorNode::_bottom_panel_raise_toggled);

	ClassDB::bind_method(D_METHOD("_on_plugin_ready"), &EditorNode::_on_plugin_ready);

	ClassDB::bind_method(D_METHOD("_video_driver_selected"), &EditorNode::_video_driver_selected);

	ClassDB::bind_method(D_METHOD("_resources_changed"), &EditorNode::_resources_changed);
	ClassDB::bind_method(D_METHOD("_reload_modified_scenes"), &EditorNode::_reload_modified_scenes);
	ClassDB::bind_method(D_METHOD("_reload_project_settings"), &EditorNode::_reload_project_settings);
	ClassDB::bind_method(D_METHOD("_resave_scenes"), &EditorNode::_resave_scenes);
	ClassDB::bind_method(D_METHOD("_project_settings_changed"), &EditorNode::_project_settings_changed);
	ClassDB::bind_method(D_METHOD("_feature_profile_changed"), &EditorNode::_feature_profile_changed);

	ClassDB::bind_method("_screenshot", &EditorNode::_screenshot);
	ClassDB::bind_method("_request_screenshot", &EditorNode::_request_screenshot);
	ClassDB::bind_method("_save_screenshot", &EditorNode::_save_screenshot);

	ADD_SIGNAL(MethodInfo("play_pressed"));
	ADD_SIGNAL(MethodInfo("pause_pressed"));
	ADD_SIGNAL(MethodInfo("stop_pressed"));
	ADD_SIGNAL(MethodInfo("request_help_search"));
	ADD_SIGNAL(MethodInfo("script_add_function_request", PropertyInfo(Variant::OBJECT, "obj"), PropertyInfo(Variant::STRING, "function"), PropertyInfo(Variant::POOL_STRING_ARRAY, "args")));
	ADD_SIGNAL(MethodInfo("resource_saved", PropertyInfo(Variant::OBJECT, "obj")));
	ADD_SIGNAL(MethodInfo("scene_saved", PropertyInfo(Variant::STRING, "path")));
}

static Node *_resource_get_edited_scene() {
	return EditorNode::get_singleton()->get_edited_scene();
}

void EditorNode::_print_handler(void *p_this, const String &p_string, bool p_error) {
	EditorNode *en = (EditorNode *)p_this;
	en->log->add_message(p_string, p_error ? EditorLog::MSG_TYPE_ERROR : EditorLog::MSG_TYPE_STD);
}

static void _execute_thread(void *p_ud) {
	EditorNode::ExecuteThreadArgs *eta = (EditorNode::ExecuteThreadArgs *)p_ud;
	Error err = OS::get_singleton()->execute(eta->path, eta->args, true, nullptr, &eta->output, &eta->exitcode, true, &eta->execute_output_mutex);
	print_verbose("Thread exit status: " + itos(eta->exitcode));
	if (err != OK) {
		eta->exitcode = err;
	}

	eta->done.set();
}

int EditorNode::execute_and_show_output(const String &p_title, const String &p_path, const List<String> &p_arguments, bool p_close_on_ok, bool p_close_on_errors) {
	execute_output_dialog->set_title(p_title);
	execute_output_dialog->get_ok()->set_disabled(true);
	execute_outputs->clear();
	execute_outputs->set_scroll_follow(true);
	execute_output_dialog->popup_centered_ratio();

	ExecuteThreadArgs eta;
	eta.path = p_path;
	eta.args = p_arguments;
	eta.exitcode = 255;

	int prev_len = 0;

	eta.execute_output_thread.start(_execute_thread, &eta);

	while (!eta.done.is_set()) {
		eta.execute_output_mutex.lock();
		if (prev_len != eta.output.length()) {
			String to_add = eta.output.substr(prev_len, eta.output.length());
			prev_len = eta.output.length();
			execute_outputs->add_text(to_add);
			Main::iteration();
		}
		eta.execute_output_mutex.unlock();
		OS::get_singleton()->delay_usec(1000);
	}

	eta.execute_output_thread.wait_to_finish();
	execute_outputs->add_text("\nExit Code: " + itos(eta.exitcode));

	if (p_close_on_errors && eta.exitcode != 0) {
		execute_output_dialog->hide();
	}
	if (p_close_on_ok && eta.exitcode == 0) {
		execute_output_dialog->hide();
	}

	execute_output_dialog->get_ok()->set_disabled(false);

	return eta.exitcode;
}

EditorNode::EditorNode() {
	OS::get_singleton()->benchmark_begin_measure("editor");
	EditorPropertyNameProcessor *epnp = memnew(EditorPropertyNameProcessor);
	add_child(epnp);

	Input::get_singleton()->set_use_accumulated_input(true);
	Resource::_get_local_scene_func = _resource_get_edited_scene;

	VisualServer::get_singleton()->textures_keep_original(true);
	VisualServer::get_singleton()->set_debug_generate_wireframes(true);

	NavigationServer::get_singleton()->set_active(false); // no nav by default if editor

	PhysicsServer::get_singleton()->set_active(false); // no physics by default if editor
	Physics2DServer::get_singleton()->set_active(false); // no physics by default if editor
	ScriptServer::set_scripting_enabled(false); // no scripting by default if editor

	EditorHelp::generate_doc(); //before any editor classes are created
	SceneState::set_disable_placeholders(true);
	ResourceLoader::clear_translation_remaps(); //no remaps using during editor
	ResourceLoader::clear_path_remaps();

	InputDefault *id = Object::cast_to<InputDefault>(Input::get_singleton());

	if (id) {
		if (!OS::get_singleton()->has_touchscreen_ui_hint() && Input::get_singleton()) {
			//only if no touchscreen ui hint, set emulation
			id->set_emulate_touch_from_mouse(false); //just disable just in case
		}
		id->set_custom_mouse_cursor(RES());
	}

	singleton = this;
	exiting = false;
	dimmed = false;
	last_checked_version = 0;
	changing_scene = false;
	_initializing_addons = false;
	docks_visible = true;
	restoring_scenes = false;
	cmdline_export_mode = false;
	scene_distraction = false;
	script_distraction = false;

	TranslationServer::get_singleton()->set_enabled(false);
	// load settings
	if (!EditorSettings::get_singleton()) {
		EditorSettings::create();
	}

	FileAccess::set_backup_save(EDITOR_GET("filesystem/on_save/safe_save_on_backup_then_rename"));

	{
		int display_scale = EditorSettings::get_singleton()->get("interface/editor/display_scale");

		switch (display_scale) {
			case 0:
				// Try applying a suitable display scale automatically.
				editor_set_scale(EditorSettings::get_singleton()->get_auto_display_scale());
				break;
			case 1:
				editor_set_scale(0.75);
				break;
			case 2:
				editor_set_scale(1.0);
				break;
			case 3:
				editor_set_scale(1.25);
				break;
			case 4:
				editor_set_scale(1.5);
				break;
			case 5:
				editor_set_scale(1.75);
				break;
			case 6:
				editor_set_scale(2.0);
				break;
			default:
				editor_set_scale(EditorSettings::get_singleton()->get("interface/editor/custom_display_scale"));
				break;
		}
	}

	// Define a minimum window size to prevent UI elements from overlapping or being cut off
	OS::get_singleton()->set_min_window_size(Size2(1024, 600) * EDSCALE);

	FileDialog::set_default_show_hidden_files(EditorSettings::get_singleton()->get("filesystem/file_dialog/show_hidden_files"));
	EditorFileDialog::set_default_show_hidden_files(EditorSettings::get_singleton()->get("filesystem/file_dialog/show_hidden_files"));
	EditorFileDialog::set_default_display_mode((EditorFileDialog::DisplayMode)EditorSettings::get_singleton()->get("filesystem/file_dialog/display_mode").operator int());

	int swap_cancel_ok = EDITOR_GET("interface/editor/accept_dialog_cancel_ok_buttons");
	if (swap_cancel_ok != 0) { // 0 is auto, set in register_scene based on OS.
		// Swap on means OK first.
		AcceptDialog::set_swap_ok_cancel(swap_cancel_ok == 2);
	}

	ResourceLoader::set_abort_on_missing_resources(false);
	ResourceLoader::set_error_notify_func(this, _load_error_notify);
	ResourceLoader::set_dependency_error_notify_func(this, _dependency_error_report);

	{ //register importers at the beginning, so dialogs are created with the right extensions
		Ref<ResourceImporterTexture> import_texture;
		import_texture.instance();
		ResourceFormatImporter::get_singleton()->add_importer(import_texture);

		Ref<ResourceImporterLayeredTexture> import_3d;
		import_3d.instance();
		import_3d->set_3d(true);
		ResourceFormatImporter::get_singleton()->add_importer(import_3d);

		Ref<ResourceImporterLayeredTexture> import_array;
		import_array.instance();
		import_array->set_3d(false);
		ResourceFormatImporter::get_singleton()->add_importer(import_array);

		Ref<ResourceImporterImage> import_image;
		import_image.instance();
		ResourceFormatImporter::get_singleton()->add_importer(import_image);

		Ref<ResourceImporterTextureAtlas> import_texture_atlas;
		import_texture_atlas.instance();
		ResourceFormatImporter::get_singleton()->add_importer(import_texture_atlas);

		Ref<ResourceImporterCSVTranslation> import_csv_translation;
		import_csv_translation.instance();
		ResourceFormatImporter::get_singleton()->add_importer(import_csv_translation);

		Ref<ResourceImporterWAV> import_wav;
		import_wav.instance();
		ResourceFormatImporter::get_singleton()->add_importer(import_wav);

		Ref<ResourceImporterOBJ> import_obj;
		import_obj.instance();
		ResourceFormatImporter::get_singleton()->add_importer(import_obj);

		Ref<ResourceImporterScene> import_scene;
		import_scene.instance();
		ResourceFormatImporter::get_singleton()->add_importer(import_scene);

		{
			Ref<EditorSceneImporterCollada> import_collada;
			import_collada.instance();
			import_scene->add_importer(import_collada);

			Ref<EditorOBJImporter> import_obj2;
			import_obj2.instance();
			import_scene->add_importer(import_obj2);

			Ref<EditorSceneImporterESCN> import_escn;
			import_escn.instance();
			import_scene->add_importer(import_escn);
		}

		Ref<ResourceImporterBitMap> import_bitmap;
		import_bitmap.instance();
		ResourceFormatImporter::get_singleton()->add_importer(import_bitmap);
	}

	{
		Ref<EditorInspectorDefaultPlugin> eidp;
		eidp.instance();
		EditorInspector::add_inspector_plugin(eidp);

		Ref<EditorInspectorRootMotionPlugin> rmp;
		rmp.instance();
		EditorInspector::add_inspector_plugin(rmp);

		Ref<EditorInspectorShaderModePlugin> smp;
		smp.instance();
		EditorInspector::add_inspector_plugin(smp);
	}

	editor_selection = memnew(EditorSelection);

	EditorFileSystem *efs = memnew(EditorFileSystem);
	add_child(efs);

	//used for previews
	FileDialog::get_icon_func = _file_dialog_get_icon;
	FileDialog::register_func = _file_dialog_register;
	FileDialog::unregister_func = _file_dialog_unregister;

	EditorFileDialog::get_icon_func = _file_dialog_get_icon;
	EditorFileDialog::register_func = _editor_file_dialog_register;
	EditorFileDialog::unregister_func = _editor_file_dialog_unregister;

	editor_export = memnew(EditorExport);
	add_child(editor_export);

	// Exporters might need the theme
	theme = create_custom_theme();

	register_exporters();

	ClassDB::set_class_enabled("RootMotionView", true);

	//defs here, use EDITOR_GET in logic
	EDITOR_DEF_RST("interface/scene_tabs/always_show_close_button", false);
	EDITOR_DEF_RST("interface/scene_tabs/resize_if_many_tabs", true);
	EDITOR_DEF_RST("interface/scene_tabs/minimum_width", 50);
	EDITOR_DEF("run/output/always_clear_output_on_play", true);
	EDITOR_DEF("run/output/always_open_output_on_play", true);
	EDITOR_DEF("run/output/always_close_output_on_stop", true);
	EDITOR_DEF("interface/editor/save_on_focus_loss", false);
	EDITOR_DEF_RST("interface/editor/save_each_scene_on_quit", true);
	EDITOR_DEF("interface/editor/quit_confirmation", true);
	EDITOR_DEF("interface/editor/update_continuously", false);
#if defined(ANDROID_ENABLED) || defined(JAVASCRIPT_ENABLED)
	EDITOR_DEF("interface/editor/show_update_spinner", true);
	EDITOR_DEF("interface/editor/update_vital_only", true);
#else
	EDITOR_DEF("interface/editor/show_update_spinner", false);
	EDITOR_DEF("interface/editor/update_vital_only", false);
#endif
	EDITOR_DEF("interface/editor/localize_settings", true);

	AudioServer::get_singleton()->set_enabled(!EDITOR_DEF_RST("interface/audio/muting/mute_driver", false));
	AudioDriverManager::set_mute_sensitivity(AudioDriverManager::MUTE_FLAG_SILENCE, EDITOR_DEF_RST("interface/audio/muting/mute_on_silence", true));
	AudioDriverManager::set_mute_sensitivity(AudioDriverManager::MUTE_FLAG_PAUSED, EDITOR_DEF_RST("interface/audio/muting/mute_on_pause", true));
	AudioDriverManager::set_mute_sensitivity(AudioDriverManager::MUTE_FLAG_FOCUS_LOSS, EDITOR_DEF_RST("interface/audio/muting/mute_on_focus_loss", true));

	EDITOR_DEF_RST("interface/scene_tabs/restore_scenes_on_load", false);
	EDITOR_DEF_RST("interface/scene_tabs/show_thumbnail_on_hover", true);
	EDITOR_DEF_RST("interface/inspector/default_property_name_style", EditorPropertyNameProcessor::STYLE_CAPITALIZED);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "interface/inspector/default_property_name_style", PROPERTY_HINT_ENUM, "Raw,Capitalized,Localized"));
	EDITOR_DEF_RST("interface/inspector/default_float_step", 0.001);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::REAL, "interface/inspector/default_float_step", PROPERTY_HINT_RANGE, "0,1,0"));
	EDITOR_DEF_RST("interface/inspector/disable_folding", false);
	EDITOR_DEF_RST("interface/inspector/auto_unfold_foreign_scenes", true);
	EDITOR_DEF("interface/inspector/horizontal_vector2_editing", false);
	EDITOR_DEF("interface/inspector/horizontal_vector_types_editing", true);
	EDITOR_DEF("interface/inspector/open_resources_in_current_inspector", true);
	EDITOR_DEF("interface/inspector/resources_to_open_in_new_inspector", "Script,MeshLibrary,TileSet");
	EDITOR_DEF("interface/inspector/default_color_picker_mode", 0);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "interface/inspector/default_color_picker_mode", PROPERTY_HINT_ENUM, "RGB,HSV,RAW", PROPERTY_USAGE_DEFAULT));
	EDITOR_DEF("version_control/username", "");
	EDITOR_DEF("version_control/ssh_public_key_path", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "version_control/ssh_public_key_path", PROPERTY_HINT_GLOBAL_FILE));
	EDITOR_DEF("version_control/ssh_private_key_path", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "version_control/ssh_private_key_path", PROPERTY_HINT_GLOBAL_FILE));

	theme_base = memnew(Control);
	add_child(theme_base);
	theme_base->set_anchors_and_margins_preset(Control::PRESET_WIDE);

	gui_base = memnew(Panel);
	theme_base->add_child(gui_base);
	gui_base->set_anchors_and_margins_preset(Control::PRESET_WIDE);

	theme_base->set_theme(theme);
	gui_base->set_theme(theme);
	gui_base->add_style_override("panel", gui_base->get_stylebox("Background", "EditorStyles"));

	resource_preview = memnew(EditorResourcePreview);
	add_child(resource_preview);
	progress_dialog = memnew(ProgressDialog);
	gui_base->add_child(progress_dialog);

	// take up all screen
	gui_base->set_anchor(MARGIN_RIGHT, Control::ANCHOR_END);
	gui_base->set_anchor(MARGIN_BOTTOM, Control::ANCHOR_END);
	gui_base->set_end(Point2(0, 0));

	main_vbox = memnew(VBoxContainer);
	gui_base->add_child(main_vbox);
	main_vbox->set_anchors_and_margins_preset(Control::PRESET_WIDE, Control::PRESET_MODE_MINSIZE, 8);
	main_vbox->add_constant_override("separation", 8 * EDSCALE);

	menu_hb = memnew(HBoxContainer);
	main_vbox->add_child(menu_hb);

	left_l_hsplit = memnew(HSplitContainer);
	main_vbox->add_child(left_l_hsplit);

	left_l_hsplit->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	left_l_vsplit = memnew(VSplitContainer);
	left_l_hsplit->add_child(left_l_vsplit);
	dock_slot[DOCK_SLOT_LEFT_UL] = memnew(TabContainer);
	left_l_vsplit->add_child(dock_slot[DOCK_SLOT_LEFT_UL]);
	dock_slot[DOCK_SLOT_LEFT_BL] = memnew(TabContainer);
	left_l_vsplit->add_child(dock_slot[DOCK_SLOT_LEFT_BL]);

	left_r_hsplit = memnew(HSplitContainer);
	left_l_hsplit->add_child(left_r_hsplit);
	left_r_vsplit = memnew(VSplitContainer);
	left_r_hsplit->add_child(left_r_vsplit);
	dock_slot[DOCK_SLOT_LEFT_UR] = memnew(TabContainer);
	left_r_vsplit->add_child(dock_slot[DOCK_SLOT_LEFT_UR]);
	dock_slot[DOCK_SLOT_LEFT_BR] = memnew(TabContainer);
	left_r_vsplit->add_child(dock_slot[DOCK_SLOT_LEFT_BR]);

	main_hsplit = memnew(HSplitContainer);
	left_r_hsplit->add_child(main_hsplit);
	VBoxContainer *center_vb = memnew(VBoxContainer);
	main_hsplit->add_child(center_vb);
	center_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	center_split = memnew(VSplitContainer);
	center_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	center_split->set_collapsed(false);
	center_vb->add_child(center_split);

	right_hsplit = memnew(HSplitContainer);
	main_hsplit->add_child(right_hsplit);

	right_l_vsplit = memnew(VSplitContainer);
	right_hsplit->add_child(right_l_vsplit);
	dock_slot[DOCK_SLOT_RIGHT_UL] = memnew(TabContainer);
	right_l_vsplit->add_child(dock_slot[DOCK_SLOT_RIGHT_UL]);
	dock_slot[DOCK_SLOT_RIGHT_BL] = memnew(TabContainer);
	right_l_vsplit->add_child(dock_slot[DOCK_SLOT_RIGHT_BL]);

	right_r_vsplit = memnew(VSplitContainer);
	right_hsplit->add_child(right_r_vsplit);
	dock_slot[DOCK_SLOT_RIGHT_UR] = memnew(TabContainer);
	right_r_vsplit->add_child(dock_slot[DOCK_SLOT_RIGHT_UR]);
	dock_slot[DOCK_SLOT_RIGHT_BR] = memnew(TabContainer);
	right_r_vsplit->add_child(dock_slot[DOCK_SLOT_RIGHT_BR]);

	// Store them for easier access
	vsplits.push_back(left_l_vsplit);
	vsplits.push_back(left_r_vsplit);
	vsplits.push_back(right_l_vsplit);
	vsplits.push_back(right_r_vsplit);

	hsplits.push_back(left_l_hsplit);
	hsplits.push_back(left_r_hsplit);
	hsplits.push_back(main_hsplit);
	hsplits.push_back(right_hsplit);

	for (int i = 0; i < vsplits.size(); i++) {
		vsplits[i]->connect("dragged", this, "_dock_split_dragged");
		hsplits[i]->connect("dragged", this, "_dock_split_dragged");
	}

	dock_select_popup = memnew(PopupPanel);
	gui_base->add_child(dock_select_popup);
	VBoxContainer *dock_vb = memnew(VBoxContainer);
	dock_select_popup->add_child(dock_vb);

	HBoxContainer *dock_hb = memnew(HBoxContainer);
	dock_tab_move_left = memnew(ToolButton);
	dock_tab_move_left->set_icon(theme->get_icon("Back", "EditorIcons"));
	dock_tab_move_left->set_focus_mode(Control::FOCUS_NONE);
	dock_tab_move_left->connect("pressed", this, "_dock_move_left");
	dock_hb->add_child(dock_tab_move_left);

	Label *dock_label = memnew(Label);
	dock_label->set_text(TTR("Dock Position"));
	dock_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dock_label->set_align(Label::ALIGN_CENTER);
	dock_hb->add_child(dock_label);

	dock_tab_move_right = memnew(ToolButton);
	dock_tab_move_right->set_icon(theme->get_icon("Forward", "EditorIcons"));
	dock_tab_move_right->set_focus_mode(Control::FOCUS_NONE);
	dock_tab_move_right->connect("pressed", this, "_dock_move_right");

	dock_hb->add_child(dock_tab_move_right);
	dock_vb->add_child(dock_hb);

	dock_select = memnew(Control);
	dock_select->set_custom_minimum_size(Size2(128, 64) * EDSCALE);
	dock_select->connect("gui_input", this, "_dock_select_input");
	dock_select->connect("draw", this, "_dock_select_draw");
	dock_select->connect("mouse_exited", this, "_dock_popup_exit");
	dock_select->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	dock_vb->add_child(dock_select);

	dock_select_popup->set_as_minsize();
	dock_select_rect_over = -1;
	dock_popup_selected = -1;
	for (int i = 0; i < DOCK_SLOT_MAX; i++) {
		dock_slot[i]->set_custom_minimum_size(Size2(170, 0) * EDSCALE);
		dock_slot[i]->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		dock_slot[i]->set_popup(dock_select_popup);
		dock_slot[i]->connect("pre_popup_pressed", this, "_dock_pre_popup", varray(i));
		dock_slot[i]->set_tab_align(TabContainer::ALIGN_LEFT);
		dock_slot[i]->set_drag_to_rearrange_enabled(true);
		dock_slot[i]->set_tabs_rearrange_group(1);
		dock_slot[i]->connect("tab_changed", this, "_dock_tab_changed");
		dock_slot[i]->set_use_hidden_tabs_for_min_size(true);
	}

	dock_drag_timer = memnew(Timer);
	add_child(dock_drag_timer);
	dock_drag_timer->set_wait_time(0.5);
	dock_drag_timer->set_one_shot(true);
	dock_drag_timer->connect("timeout", this, "_save_docks");

	top_split = memnew(VSplitContainer);
	center_split->add_child(top_split);
	top_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	top_split->set_collapsed(true);

	VBoxContainer *srt = memnew(VBoxContainer);
	srt->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	top_split->add_child(srt);
	srt->add_constant_override("separation", 0);

	tab_preview_panel = memnew(Panel);
	tab_preview_panel->set_size(Size2(100, 100) * EDSCALE);
	tab_preview_panel->hide();
	tab_preview_panel->set_self_modulate(Color(1, 1, 1, 0.7));
	gui_base->add_child(tab_preview_panel);

	tab_preview = memnew(TextureRect);
	tab_preview->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	tab_preview->set_size(Size2(96, 96) * EDSCALE);
	tab_preview->set_position(Point2(2, 2) * EDSCALE);
	tab_preview_panel->add_child(tab_preview);

	scene_tabs = memnew(Tabs);
	scene_tabs->add_style_override("tab_fg", gui_base->get_stylebox("SceneTabFG", "EditorStyles"));
	scene_tabs->add_style_override("tab_bg", gui_base->get_stylebox("SceneTabBG", "EditorStyles"));
	scene_tabs->set_select_with_rmb(true);
	scene_tabs->add_tab("unsaved");
	scene_tabs->set_tab_align(Tabs::ALIGN_LEFT);
	scene_tabs->set_tab_close_display_policy((bool(EDITOR_DEF("interface/scene_tabs/always_show_close_button", false)) ? Tabs::CLOSE_BUTTON_SHOW_ALWAYS : Tabs::CLOSE_BUTTON_SHOW_ACTIVE_ONLY));
	scene_tabs->set_min_width(int(EDITOR_DEF("interface/scene_tabs/minimum_width", 50)) * EDSCALE);
	scene_tabs->set_drag_to_rearrange_enabled(true);
	scene_tabs->connect("tab_changed", this, "_scene_tab_changed");
	scene_tabs->connect("right_button_pressed", this, "_scene_tab_script_edited");
	scene_tabs->connect("tab_close", this, "_scene_tab_closed", varray(SCENE_TAB_CLOSE));
	scene_tabs->connect("tab_hover", this, "_scene_tab_hover");
	scene_tabs->connect("mouse_exited", this, "_scene_tab_exit");
	scene_tabs->connect("gui_input", this, "_scene_tab_input");
	scene_tabs->connect("reposition_active_tab_request", this, "_reposition_active_tab");
	scene_tabs->connect("resized", this, "_update_scene_tabs");

	tabbar_container = memnew(HBoxContainer);
	scene_tabs->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	scene_tabs_context_menu = memnew(PopupMenu);
	tabbar_container->add_child(scene_tabs_context_menu);
	scene_tabs_context_menu->connect("id_pressed", this, "_menu_option");
	scene_tabs_context_menu->set_hide_on_window_lose_focus(true);

	srt->add_child(tabbar_container);
	tabbar_container->add_child(scene_tabs);
	distraction_free = memnew(ToolButton);
#ifdef OSX_ENABLED
	distraction_free->set_shortcut(ED_SHORTCUT("editor/distraction_free_mode", TTR("Distraction Free Mode"), KEY_MASK_CMD | KEY_MASK_CTRL | KEY_D));
#else
	distraction_free->set_shortcut(ED_SHORTCUT("editor/distraction_free_mode", TTR("Distraction Free Mode"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_F11));
#endif
	distraction_free->set_tooltip(TTR("Toggle distraction-free mode."));
	distraction_free->connect("pressed", this, "_toggle_distraction_free_mode");
	distraction_free->set_icon(gui_base->get_icon("DistractionFree", "EditorIcons"));
	distraction_free->set_toggle_mode(true);

	scene_tab_add = memnew(ToolButton);
	tabbar_container->add_child(scene_tab_add);
	tabbar_container->add_child(distraction_free);
	scene_tab_add->set_tooltip(TTR("Add a new scene."));
	scene_tab_add->set_icon(gui_base->get_icon("Add", "EditorIcons"));
	scene_tab_add->add_color_override("icon_color_normal", Color(0.6f, 0.6f, 0.6f, 0.8f));
	scene_tab_add->connect("pressed", this, "_menu_option", make_binds(FILE_NEW_SCENE));

	scene_root_parent = memnew(PanelContainer);
	scene_root_parent->set_custom_minimum_size(Size2(0, 80) * EDSCALE);
	scene_root_parent->add_style_override("panel", gui_base->get_stylebox("Content", "EditorStyles"));
	scene_root_parent->set_draw_behind_parent(true);
	srt->add_child(scene_root_parent);
	scene_root_parent->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	scene_root = memnew(Viewport);
	//scene_root->set_usage(Viewport::USAGE_2D); canvas BG mode prevents usage of this as 2D
	scene_root->set_disable_3d(true);

	VisualServer::get_singleton()->viewport_set_hide_scenario(scene_root->get_viewport_rid(), true);
	scene_root->set_disable_input(true);
	scene_root->set_as_audio_listener_2d(true);

	viewport = memnew(VBoxContainer);
	viewport->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	viewport->add_constant_override("separation", 0);
	scene_root_parent->add_child(viewport);

	HBoxContainer *left_menu_hb = memnew(HBoxContainer);
	menu_hb->add_child(left_menu_hb);

	file_menu = memnew(MenuButton);
	file_menu->set_flat(false);
	file_menu->set_switch_on_hover(true);
	file_menu->set_text(TTR("Scene"));
	file_menu->add_style_override("hover", gui_base->get_stylebox("MenuHover", "EditorStyles"));
	left_menu_hb->add_child(file_menu);

	prev_scene = memnew(ToolButton);
	prev_scene->set_icon(gui_base->get_icon("PrevScene", "EditorIcons"));
	prev_scene->set_tooltip(TTR("Go to previously opened scene."));
	prev_scene->set_disabled(true);
	prev_scene->connect("pressed", this, "_menu_option", make_binds(FILE_OPEN_PREV));
	gui_base->add_child(prev_scene);
	prev_scene->set_position(Point2(3, 24));
	prev_scene->hide();

	accept = memnew(AcceptDialog);
	gui_base->add_child(accept);
	accept->connect("confirmed", this, "_menu_confirm_current");

	save_accept = memnew(AcceptDialog);
	gui_base->add_child(save_accept);
	save_accept->connect("confirmed", this, "_menu_option", make_binds((int)MenuOptions::FILE_SAVE_AS_SCENE));

	project_export = memnew(ProjectExportDialog);
	gui_base->add_child(project_export);

	dependency_error = memnew(DependencyErrorDialog);
	gui_base->add_child(dependency_error);

	dependency_fixer = memnew(DependencyEditor);
	gui_base->add_child(dependency_fixer);

	settings_config_dialog = memnew(EditorSettingsDialog);
	gui_base->add_child(settings_config_dialog);

	project_settings = memnew(ProjectSettingsEditor(&editor_data));
	gui_base->add_child(project_settings);
	ProjectSettings::get_singleton()->connect("project_settings_changed", this, "_project_settings_changed");

	run_settings_dialog = memnew(RunSettingsDialog);
	gui_base->add_child(run_settings_dialog);

	export_template_manager = memnew(ExportTemplateManager);
	gui_base->add_child(export_template_manager);

	feature_profile_manager = memnew(EditorFeatureProfileManager);
	gui_base->add_child(feature_profile_manager);
	about = memnew(EditorAbout);
	gui_base->add_child(about);
	feature_profile_manager->connect("current_feature_profile_changed", this, "_feature_profile_changed");

	warning = memnew(AcceptDialog);
	warning->add_button(TTR("Copy Text"), true, "copy");
	gui_base->add_child(warning);
	warning->connect("custom_action", this, "_copy_warning");

	ED_SHORTCUT("editor/next_tab", TTR("Next tab"), KEY_MASK_CMD + KEY_TAB);
	ED_SHORTCUT("editor/prev_tab", TTR("Previous tab"), KEY_MASK_CMD + KEY_MASK_SHIFT + KEY_TAB);
	ED_SHORTCUT("editor/filter_files", TTR("Filter Files..."), KEY_MASK_ALT + KEY_MASK_CMD + KEY_P);
	PopupMenu *p;

	file_menu->set_tooltip(TTR("Operations with scene files."));

	p = file_menu->get_popup();
	p->set_hide_on_window_lose_focus(true);
	p->add_shortcut(ED_SHORTCUT("editor/new_scene", TTR("New Scene"), KEY_MASK_CMD + KEY_N), FILE_NEW_SCENE);
	p->add_shortcut(ED_SHORTCUT("editor/new_inherited_scene", TTR("New Inherited Scene..."), KEY_MASK_CMD + KEY_MASK_SHIFT + KEY_N), FILE_NEW_INHERITED_SCENE);
	p->add_shortcut(ED_SHORTCUT("editor/open_scene", TTR("Open Scene..."), KEY_MASK_CMD + KEY_O), FILE_OPEN_SCENE);
	p->add_shortcut(ED_SHORTCUT("editor/reopen_closed_scene", TTR("Reopen Closed Scene"), KEY_MASK_CMD + KEY_MASK_SHIFT + KEY_T), FILE_OPEN_PREV);
	p->add_submenu_item(TTR("Open Recent"), "RecentScenes", FILE_OPEN_RECENT);

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("editor/save_scene", TTR("Save Scene"), KEY_MASK_CMD + KEY_S), FILE_SAVE_SCENE);
	p->add_shortcut(ED_SHORTCUT("editor/save_scene_as", TTR("Save Scene As..."), KEY_MASK_SHIFT + KEY_MASK_CMD + KEY_S), FILE_SAVE_AS_SCENE);
	p->add_shortcut(ED_SHORTCUT("editor/save_all_scenes", TTR("Save All Scenes"), KEY_MASK_ALT + KEY_MASK_SHIFT + KEY_MASK_CMD + KEY_S), FILE_SAVE_ALL_SCENES);

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("editor/quick_open", TTR("Quick Open..."), KEY_MASK_SHIFT + KEY_MASK_ALT + KEY_O), FILE_QUICK_OPEN);
	p->add_shortcut(ED_SHORTCUT("editor/quick_open_scene", TTR("Quick Open Scene..."), KEY_MASK_SHIFT + KEY_MASK_CMD + KEY_O), FILE_QUICK_OPEN_SCENE);
	p->add_shortcut(ED_SHORTCUT("editor/quick_open_script", TTR("Quick Open Script..."), KEY_MASK_ALT + KEY_MASK_CMD + KEY_O), FILE_QUICK_OPEN_SCRIPT);

	p->add_separator();
	PopupMenu *pm_export = memnew(PopupMenu);
	pm_export->set_name("Export");
	p->add_child(pm_export);
	p->add_submenu_item(TTR("Convert To..."), "Export");
	pm_export->add_shortcut(ED_SHORTCUT("editor/convert_to_MeshLibrary", TTR("MeshLibrary...")), FILE_EXPORT_MESH_LIBRARY);
	pm_export->add_shortcut(ED_SHORTCUT("editor/convert_to_TileSet", TTR("TileSet...")), FILE_EXPORT_TILESET);
	pm_export->connect("id_pressed", this, "_menu_option");

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("editor/undo", TTR("Undo"), KEY_MASK_CMD + KEY_Z), EDIT_UNDO, true);
	p->add_shortcut(ED_SHORTCUT("editor/redo", TTR("Redo"), KEY_MASK_SHIFT + KEY_MASK_CMD + KEY_Z), EDIT_REDO, true);

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("editor/reload_saved_scene", TTR("Reload Saved Scene")), EDIT_RELOAD_SAVED_SCENE);
	p->add_shortcut(ED_SHORTCUT("editor/close_scene", TTR("Close Scene"), KEY_MASK_SHIFT + KEY_MASK_CMD + KEY_W), FILE_CLOSE);

	recent_scenes = memnew(PopupMenu);
	recent_scenes->set_name("RecentScenes");
	p->add_child(recent_scenes);
	recent_scenes->connect("id_pressed", this, "_open_recent_scene");

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("editor/file_quit", TTR("Quit"), KEY_MASK_CMD + KEY_Q), FILE_QUIT, true);

	project_menu = memnew(MenuButton);
	project_menu->set_flat(false);
	project_menu->set_switch_on_hover(true);
	project_menu->set_tooltip(TTR("Miscellaneous project or scene-wide tools."));
	project_menu->set_text(TTR("Project"));
	project_menu->add_style_override("hover", gui_base->get_stylebox("MenuHover", "EditorStyles"));
	left_menu_hb->add_child(project_menu);

	p = project_menu->get_popup();
	p->set_hide_on_window_lose_focus(true);
	p->add_shortcut(ED_SHORTCUT("editor/project_settings", TTR("Project Settings...")), RUN_SETTINGS);
	p->connect("id_pressed", this, "_menu_option");

	vcs_actions_menu = VersionControlEditorPlugin::get_singleton()->get_version_control_actions_panel();
	vcs_actions_menu->set_name("Version Control");
	vcs_actions_menu->connect("index_pressed", this, "_version_control_menu_option");
	p->add_separator();
	p->add_child(vcs_actions_menu);
	p->add_submenu_item(TTR("Version Control"), "Version Control");
	vcs_actions_menu->add_item(TTR("Set Up Version Control"), RUN_VCS_SETTINGS);
	vcs_actions_menu->add_item(TTR("Shut Down Version Control"), RUN_VCS_SHUT_DOWN);

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("editor/export", TTR("Export...")), FILE_EXPORT_PROJECT);
#ifndef ANDROID_ENABLED
	p->add_item(TTR("Install Android Build Template..."), FILE_INSTALL_ANDROID_SOURCE);
	p->add_item(TTR("Open User Data Folder"), RUN_USER_DATA_FOLDER);
#endif

	plugin_config_dialog = memnew(PluginConfigDialog);
	plugin_config_dialog->connect("plugin_ready", this, "_on_plugin_ready");
	gui_base->add_child(plugin_config_dialog);

	tool_menu = memnew(PopupMenu);
	tool_menu->set_name("Tools");
	tool_menu->connect("index_pressed", this, "_tool_menu_option");
	p->add_child(tool_menu);
	p->add_submenu_item(TTR("Tools"), "Tools");
	tool_menu->add_item(TTR("Orphan Resource Explorer..."), TOOLS_ORPHAN_RESOURCES);

	p->add_separator();
	p->add_shortcut(ED_SHORTCUT("editor/reload_current_project", TTR("Reload Current Project")), RELOAD_CURRENT_PROJECT);
#ifdef OSX_ENABLED
	p->add_shortcut(ED_SHORTCUT("editor/quit_to_project_list", TTR("Quit to Project List"), KEY_MASK_SHIFT + KEY_MASK_ALT + KEY_Q), RUN_PROJECT_MANAGER, true);
#else
	p->add_shortcut(ED_SHORTCUT("editor/quit_to_project_list", TTR("Quit to Project List"), KEY_MASK_SHIFT + KEY_MASK_CMD + KEY_Q), RUN_PROJECT_MANAGER, true);
#endif

	menu_hb->add_spacer();

	main_editor_button_vb = memnew(HBoxContainer);
	menu_hb->add_child(main_editor_button_vb);

	debug_menu = memnew(MenuButton);
	debug_menu->set_flat(false);
	debug_menu->set_switch_on_hover(true);
	debug_menu->set_text(TTR("Debug"));
	debug_menu->add_style_override("hover", gui_base->get_stylebox("MenuHover", "EditorStyles"));
	left_menu_hb->add_child(debug_menu);

	p = debug_menu->get_popup();
	p->set_hide_on_window_lose_focus(true);
	p->set_hide_on_checkable_item_selection(false);

	p->add_check_shortcut(ED_SHORTCUT("editor/deploy_with_remote_debug", TTR("Deploy with Remote Debug")), RUN_DEPLOY_REMOTE_DEBUG);
	p->set_item_tooltip(
			p->get_item_count() - 1,
			TTR("When this option is enabled, using one-click deploy will make the executable attempt to connect to this computer's IP so the running project can be debugged.\nThis option is intended to be used for remote debugging (typically with a mobile device).\nYou don't need to enable it to use the GDScript debugger locally."));

	p->add_check_shortcut(ED_SHORTCUT("editor/small_deploy_with_network_fs", TTR("Small Deploy with Network Filesystem")), RUN_FILE_SERVER);
	p->set_item_tooltip(
			p->get_item_count() - 1,
			TTR("When this option is enabled, using one-click deploy for Android will only export an executable without the project data.\nThe filesystem will be provided from the project by the editor over the network.\nOn Android, deploying will use the USB cable for faster performance. This option speeds up testing for projects with large assets."));

	p->add_separator();

	p->add_check_shortcut(ED_SHORTCUT("editor/visible_collision_shapes", TTR("Visible Collision Shapes")), RUN_DEBUG_COLLISONS);
	p->set_item_tooltip(
			p->get_item_count() - 1,
			TTR("When this option is enabled, collision shapes and raycast nodes (for 2D and 3D) will be visible in the running project."));

	p->add_check_shortcut(ED_SHORTCUT("editor/visible_navigation", TTR("Visible Navigation")), RUN_DEBUG_NAVIGATION);
	p->set_item_tooltip(
			p->get_item_count() - 1,
			TTR("When this option is enabled, navigation meshes and polygons will be visible in the running project."));

	if (GLOBAL_GET("rendering/quality/driver/driver_name") == "GLES3") {
		p->add_separator();

		p->add_check_shortcut(ED_SHORTCUT("editor/use_shader_fallbacks", TTR("Force Shader Fallbacks")), RUN_DEBUG_SHADER_FALLBACKS);
		p->set_item_tooltip(
				p->get_item_count() - 1,
				TTR("When this option is enabled, shaders will be used in their fallback form (either visible via an ubershader or hidden) during all the run time.\nThis is useful for verifying the look and performance of fallbacks, which are normally displayed briefly.\nAsynchronous shader compilation must be enabled in the project settings for this option to make a difference."));
	}

	p->add_separator();

	p->add_check_shortcut(ED_SHORTCUT("editor/sync_scene_changes", TTR("Synchronize Scene Changes")), RUN_LIVE_DEBUG);
	p->set_item_tooltip(
			p->get_item_count() - 1,
			TTR("When this option is enabled, any changes made to the scene in the editor will be replicated in the running project.\nWhen used remotely on a device, this is more efficient when the network filesystem option is enabled."));

	p->add_check_shortcut(ED_SHORTCUT("editor/sync_script_changes", TTR("Synchronize Script Changes")), RUN_RELOAD_SCRIPTS);
	p->set_item_tooltip(
			p->get_item_count() - 1,
			TTR("When this option is enabled, any script that is saved will be reloaded in the running project.\nWhen used remotely on a device, this is more efficient when the network filesystem option is enabled."));

	p->connect("id_pressed", this, "_menu_option");

	menu_hb->add_spacer();

	settings_menu = memnew(MenuButton);
	settings_menu->set_flat(false);
	settings_menu->set_switch_on_hover(true);
	settings_menu->set_text(TTR("Editor"));
	settings_menu->add_style_override("hover", gui_base->get_stylebox("MenuHover", "EditorStyles"));
	left_menu_hb->add_child(settings_menu);

	p = settings_menu->get_popup();
	p->set_hide_on_window_lose_focus(true);
#ifdef OSX_ENABLED
	p->add_shortcut(ED_SHORTCUT("editor/editor_settings", TTR("Editor Settings..."), KEY_MASK_CMD + KEY_COMMA), SETTINGS_PREFERENCES);
#else
	p->add_shortcut(ED_SHORTCUT("editor/editor_settings", TTR("Editor Settings...")), SETTINGS_PREFERENCES);
#endif
	p->add_separator();

	editor_layouts = memnew(PopupMenu);
	editor_layouts->set_name("Layouts");
	p->add_child(editor_layouts);
	editor_layouts->connect("id_pressed", this, "_layout_menu_option");
	p->add_submenu_item(TTR("Editor Layout"), "Layouts");
	p->add_separator();
#ifdef OSX_ENABLED
	p->add_shortcut(ED_SHORTCUT("editor/take_screenshot", TTR("Take Screenshot"), KEY_MASK_CMD | KEY_F12), EDITOR_SCREENSHOT);
#else
	p->add_shortcut(ED_SHORTCUT("editor/take_screenshot", TTR("Take Screenshot"), KEY_MASK_CTRL | KEY_F12), EDITOR_SCREENSHOT);
#endif
	p->set_item_tooltip(p->get_item_count() - 1, TTR("Screenshots are stored in the Editor Data/Settings Folder."));
#ifndef ANDROID_ENABLED
#ifdef OSX_ENABLED
	p->add_shortcut(ED_SHORTCUT("editor/fullscreen_mode", TTR("Toggle Fullscreen"), KEY_MASK_CMD | KEY_MASK_CTRL | KEY_F), SETTINGS_TOGGLE_FULLSCREEN);
#else
	p->add_shortcut(ED_SHORTCUT("editor/fullscreen_mode", TTR("Toggle Fullscreen"), KEY_MASK_SHIFT | KEY_F11), SETTINGS_TOGGLE_FULLSCREEN);
#endif
#endif
	p->add_separator();

#ifndef ANDROID_ENABLED
	if (OS::get_singleton()->get_data_path() == OS::get_singleton()->get_config_path()) {
		// Configuration and data folders are located in the same place (Windows/macOS)
		p->add_item(TTR("Open Editor Data/Settings Folder"), SETTINGS_EDITOR_DATA_FOLDER);
	} else {
		// Separate configuration and data folders (Linux)
		p->add_item(TTR("Open Editor Data Folder"), SETTINGS_EDITOR_DATA_FOLDER);
		p->add_item(TTR("Open Editor Settings Folder"), SETTINGS_EDITOR_CONFIG_FOLDER);
	}
	p->add_separator();
#endif

	p->add_item(TTR("Manage Editor Features..."), SETTINGS_MANAGE_FEATURE_PROFILES);
#ifndef ANDROID_ENABLED
	p->add_item(TTR("Manage Export Templates..."), SETTINGS_MANAGE_EXPORT_TEMPLATES);
#endif

	// Help Menu
	help_menu = memnew(MenuButton);
	help_menu->set_flat(false);
	help_menu->set_switch_on_hover(true);
	help_menu->set_text(TTR("Help"));
	help_menu->add_style_override("hover", gui_base->get_stylebox("MenuHover", "EditorStyles"));
	left_menu_hb->add_child(help_menu);

	p = help_menu->get_popup();
	p->set_hide_on_window_lose_focus(true);
	p->connect("id_pressed", this, "_menu_option");
#ifdef OSX_ENABLED
	p->add_icon_shortcut(gui_base->get_icon("HelpSearch", "EditorIcons"), ED_SHORTCUT("editor/editor_help", TTR("Search Help"), KEY_MASK_ALT | KEY_SPACE), HELP_SEARCH);
#else
	p->add_icon_shortcut(gui_base->get_icon("HelpSearch", "EditorIcons"), ED_SHORTCUT("editor/editor_help", TTR("Search Help"), KEY_F1), HELP_SEARCH);
#endif
	p->add_separator();
	p->add_icon_shortcut(gui_base->get_icon("ExternalLink", "EditorIcons"), ED_SHORTCUT("editor/online_docs", TTR("Online Documentation")), HELP_DOCS);
	p->add_icon_shortcut(gui_base->get_icon("ExternalLink", "EditorIcons"), ED_SHORTCUT("editor/q&a", TTR("Questions & Answers")), HELP_QA);
	p->add_icon_shortcut(gui_base->get_icon("ExternalLink", "EditorIcons"), ED_SHORTCUT("editor/report_a_bug", TTR("Report a Bug")), HELP_REPORT_A_BUG);
	p->add_icon_shortcut(gui_base->get_icon("ExternalLink", "EditorIcons"), ED_SHORTCUT("editor/suggest_a_feature", TTR("Suggest a Feature")), HELP_SUGGEST_A_FEATURE);
	p->add_icon_shortcut(gui_base->get_icon("ExternalLink", "EditorIcons"), ED_SHORTCUT("editor/send_docs_feedback", TTR("Send Docs Feedback")), HELP_SEND_DOCS_FEEDBACK);
	p->add_icon_shortcut(gui_base->get_icon("ExternalLink", "EditorIcons"), ED_SHORTCUT("editor/community", TTR("Community")), HELP_COMMUNITY);
	p->add_separator();
	p->add_icon_shortcut(gui_base->get_icon("Godot", "EditorIcons"), ED_SHORTCUT("editor/about", TTR("About Godot")), HELP_ABOUT);
	p->add_icon_shortcut(gui_base->get_icon("Heart", "EditorIcons"), ED_SHORTCUT("editor/support_development", TTR("Support Godot Development")), HELP_SUPPORT_GODOT_DEVELOPMENT);

	HBoxContainer *play_hb = memnew(HBoxContainer);
	menu_hb->add_child(play_hb);

	play_button = memnew(ToolButton);
	play_hb->add_child(play_button);
	play_button->set_toggle_mode(true);
	play_button->set_icon(gui_base->get_icon("MainPlay", "EditorIcons"));
	play_button->set_focus_mode(Control::FOCUS_NONE);
	play_button->connect("pressed", this, "_menu_option", make_binds(RUN_PLAY));
	play_button->set_tooltip(TTR("Play the project."));
#ifdef OSX_ENABLED
	play_button->set_shortcut(ED_SHORTCUT("editor/play", TTR("Play"), KEY_MASK_CMD | KEY_B));
#else
	play_button->set_shortcut(ED_SHORTCUT("editor/play", TTR("Play"), KEY_F5));
#endif

	pause_button = memnew(ToolButton);
	pause_button->set_toggle_mode(true);
	pause_button->set_icon(gui_base->get_icon("Pause", "EditorIcons"));
	pause_button->set_focus_mode(Control::FOCUS_NONE);
	pause_button->set_tooltip(TTR("Pause the scene execution for debugging."));
	pause_button->set_disabled(true);
	play_hb->add_child(pause_button);
#ifdef OSX_ENABLED
	pause_button->set_shortcut(ED_SHORTCUT("editor/pause_scene", TTR("Pause Scene"), KEY_MASK_CMD | KEY_MASK_CTRL | KEY_Y));
#else
	pause_button->set_shortcut(ED_SHORTCUT("editor/pause_scene", TTR("Pause Scene"), KEY_F7));
#endif

	stop_button = memnew(ToolButton);
	play_hb->add_child(stop_button);
	stop_button->set_focus_mode(Control::FOCUS_NONE);
	stop_button->set_icon(gui_base->get_icon("Stop", "EditorIcons"));
	stop_button->connect("pressed", this, "_menu_option", make_binds(RUN_STOP));
	stop_button->set_tooltip(TTR("Stop the scene."));
	stop_button->set_disabled(true);
#ifdef OSX_ENABLED
	stop_button->set_shortcut(ED_SHORTCUT("editor/stop", TTR("Stop"), KEY_MASK_CMD | KEY_PERIOD));
#else
	stop_button->set_shortcut(ED_SHORTCUT("editor/stop", TTR("Stop"), KEY_F8));
#endif

	run_native = memnew(EditorRunNative);
	play_hb->add_child(run_native);
	run_native->connect("native_run", this, "_menu_option", varray(RUN_PLAY_NATIVE));

	play_scene_button = memnew(ToolButton);
	play_hb->add_child(play_scene_button);
	play_scene_button->set_toggle_mode(true);
	play_scene_button->set_focus_mode(Control::FOCUS_NONE);
	play_scene_button->set_icon(gui_base->get_icon("PlayScene", "EditorIcons"));
	play_scene_button->connect("pressed", this, "_menu_option", make_binds(RUN_PLAY_SCENE));
	play_scene_button->set_tooltip(TTR("Play the edited scene."));
#ifdef OSX_ENABLED
	play_scene_button->set_shortcut(ED_SHORTCUT("editor/play_scene", TTR("Play Scene"), KEY_MASK_CMD | KEY_R));
#else
	play_scene_button->set_shortcut(ED_SHORTCUT("editor/play_scene", TTR("Play Scene"), KEY_F6));
#endif

	play_custom_scene_button = memnew(ToolButton);
	play_hb->add_child(play_custom_scene_button);
	play_custom_scene_button->set_toggle_mode(true);
	play_custom_scene_button->set_focus_mode(Control::FOCUS_NONE);
	play_custom_scene_button->set_icon(gui_base->get_icon("PlayCustom", "EditorIcons"));
	play_custom_scene_button->connect("pressed", this, "_menu_option", make_binds(RUN_PLAY_CUSTOM_SCENE));
	play_custom_scene_button->set_tooltip(TTR("Play custom scene"));
#ifdef OSX_ENABLED
	play_custom_scene_button->set_shortcut(ED_SHORTCUT("editor/play_custom_scene", TTR("Play Custom Scene"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_R));
#else
	play_custom_scene_button->set_shortcut(ED_SHORTCUT("editor/play_custom_scene", TTR("Play Custom Scene"), KEY_MASK_CMD | KEY_MASK_SHIFT | KEY_F5));
#endif

	HBoxContainer *right_menu_hb = memnew(HBoxContainer);
	menu_hb->add_child(right_menu_hb);

	// Toggle for video driver
	video_driver = memnew(OptionButton);
	video_driver->set_flat(true);
	video_driver->set_focus_mode(Control::FOCUS_NONE);
	video_driver->connect("item_selected", this, "_video_driver_selected");
	video_driver->add_font_override("font", gui_base->get_font("bold", "EditorFonts"));
	right_menu_hb->add_child(video_driver);

	String video_drivers = ProjectSettings::get_singleton()->get_custom_property_info()["rendering/quality/driver/driver_name"].hint_string;
	String current_video_driver = OS::get_singleton()->get_video_driver_name(OS::get_singleton()->get_current_video_driver());
	video_driver_current = 0;
	for (int i = 0; i < video_drivers.get_slice_count(","); i++) {
		String driver = video_drivers.get_slice(",", i);
		video_driver->add_item(driver);
		video_driver->set_item_metadata(i, driver);

		if (current_video_driver == driver) {
			video_driver->select(i);
			video_driver_current = i;
		}
	}

	_update_video_driver_color();

	video_restart_dialog = memnew(ConfirmationDialog);
	video_restart_dialog->set_text(TTR("Changing the video driver requires restarting the editor."));
	video_restart_dialog->get_ok()->set_text(TTR("Save & Restart"));
	video_restart_dialog->connect("confirmed", this, "_menu_option", varray(SET_VIDEO_DRIVER_SAVE_AND_RESTART));
	gui_base->add_child(video_restart_dialog);

	progress_hb = memnew(BackgroundProgress);

	layout_dialog = memnew(EditorLayoutsDialog);
	gui_base->add_child(layout_dialog);
	layout_dialog->set_hide_on_ok(false);
	layout_dialog->set_size(Size2(225, 270) * EDSCALE);
	layout_dialog->connect("name_confirmed", this, "_dialog_action");

	update_spinner = memnew(MenuButton);
	right_menu_hb->add_child(update_spinner);
	update_spinner->set_icon(gui_base->get_icon("Progress1", "EditorIcons"));
	update_spinner->get_popup()->connect("id_pressed", this, "_menu_option");
	p = update_spinner->get_popup();
	p->add_radio_check_item(TTR("Update Continuously"), SETTINGS_UPDATE_CONTINUOUSLY);
	p->add_radio_check_item(TTR("Update All Changes"), SETTINGS_UPDATE_WHEN_CHANGED);
	p->add_radio_check_item(TTR("Update Vital Changes"), SETTINGS_UPDATE_VITAL_ONLY);
	p->add_separator();
	p->add_item(TTR("Hide Update Spinner"), SETTINGS_UPDATE_SPINNER_HIDE);
	_update_update_spinner();

	// Instantiate and place editor docks

	scene_tree_dock = memnew(SceneTreeDock(this, scene_root, editor_selection, editor_data));
	inspector_dock = memnew(InspectorDock(this, editor_data));
	import_dock = memnew(ImportDock);
	node_dock = memnew(NodeDock);

	filesystem_dock = memnew(FileSystemDock(this));
	filesystem_dock->connect("inherit", this, "_inherit_request");
	filesystem_dock->connect("instance", this, "_instance_request");
	filesystem_dock->connect("display_mode_changed", this, "_save_docks");

	// Scene: Top left
	dock_slot[DOCK_SLOT_LEFT_UR]->add_child(scene_tree_dock);
	dock_slot[DOCK_SLOT_LEFT_UR]->set_tab_title(scene_tree_dock->get_index(), TTR("Scene"));

	// Import: Top left, behind Scene
	dock_slot[DOCK_SLOT_LEFT_UR]->add_child(import_dock);
	dock_slot[DOCK_SLOT_LEFT_UR]->set_tab_title(import_dock->get_index(), TTR("Import"));

	// FileSystem: Bottom left
	dock_slot[DOCK_SLOT_LEFT_BR]->add_child(filesystem_dock);
	dock_slot[DOCK_SLOT_LEFT_BR]->set_tab_title(filesystem_dock->get_index(), TTR("FileSystem"));

	// Inspector: Full height right
	dock_slot[DOCK_SLOT_RIGHT_UL]->add_child(inspector_dock);
	dock_slot[DOCK_SLOT_RIGHT_UL]->set_tab_title(inspector_dock->get_index(), TTR("Inspector"));

	// Node: Full height right, behind Inspector
	dock_slot[DOCK_SLOT_RIGHT_UL]->add_child(node_dock);
	dock_slot[DOCK_SLOT_RIGHT_UL]->set_tab_title(node_dock->get_index(), TTR("Node"));

	// Hide unused dock slots and vsplits
	dock_slot[DOCK_SLOT_LEFT_UL]->hide();
	dock_slot[DOCK_SLOT_LEFT_BL]->hide();
	dock_slot[DOCK_SLOT_RIGHT_BL]->hide();
	dock_slot[DOCK_SLOT_RIGHT_UR]->hide();
	dock_slot[DOCK_SLOT_RIGHT_BR]->hide();
	left_l_vsplit->hide();
	right_r_vsplit->hide();

	// Add some offsets to left_r and main hsplits to make LEFT_R and RIGHT_L docks wider than minsize
	left_r_hsplit->set_split_offset(70 * EDSCALE);
	main_hsplit->set_split_offset(-70 * EDSCALE);

	// Define corresponding default layout

	const String docks_section = "docks";
	overridden_default_layout = -1;
	default_layout.instance();
	// Dock numbers are based on DockSlot enum value + 1
	default_layout->set_value(docks_section, "dock_3", "Scene,Import");
	default_layout->set_value(docks_section, "dock_4", "FileSystem");
	default_layout->set_value(docks_section, "dock_5", "Inspector,Node");

	for (int i = 0; i < vsplits.size(); i++) {
		default_layout->set_value(docks_section, "dock_split_" + itos(i + 1), 0);
	}
	default_layout->set_value(docks_section, "dock_hsplit_1", 0);
	default_layout->set_value(docks_section, "dock_hsplit_2", 70 * EDSCALE);
	default_layout->set_value(docks_section, "dock_hsplit_3", -70 * EDSCALE);
	default_layout->set_value(docks_section, "dock_hsplit_4", 0);

	_update_layouts_menu();

	// Bottom panels

	bottom_panel = memnew(PanelContainer);
	bottom_panel->add_style_override("panel", gui_base->get_stylebox("panel", "TabContainer"));
	center_split->add_child(bottom_panel);
	center_split->set_dragger_visibility(SplitContainer::DRAGGER_HIDDEN);

	bottom_panel_vb = memnew(VBoxContainer);
	bottom_panel->add_child(bottom_panel_vb);

	bottom_panel_hb = memnew(HBoxContainer);
	bottom_panel_hb->set_custom_minimum_size(Size2(0, 24 * EDSCALE)); // Adjust for the height of the "Expand Bottom Dock" icon.
	bottom_panel_vb->add_child(bottom_panel_hb);

	bottom_panel_hb_editors = memnew(HBoxContainer);
	bottom_panel_hb_editors->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	bottom_panel_hb->add_child(bottom_panel_hb_editors);

	VBoxContainer *version_info_vbc = memnew(VBoxContainer);
	bottom_panel_hb->add_child(version_info_vbc);

	// Add a dummy control node for vertical spacing.
	Control *v_spacer = memnew(Control);
	version_info_vbc->add_child(v_spacer);

	version_btn = memnew(LinkButton);
	version_btn->set_text(VERSION_FULL_CONFIG);
	String hash = String(VERSION_HASH);
	if (hash.length() != 0) {
		hash = " " + vformat("[%s]", hash.left(9));
	}
	// Set the text to copy in metadata as it slightly differs from the button's text.
	version_btn->set_meta(META_TEXT_TO_COPY, "v" VERSION_FULL_BUILD + hash);
	// Fade out the version label to be less prominent, but still readable
	version_btn->set_self_modulate(Color(1, 1, 1, 0.65));
	version_btn->set_underline_mode(LinkButton::UNDERLINE_MODE_ON_HOVER);
	version_btn->set_tooltip(TTR("Click to copy."));
	version_btn->connect("pressed", this, "_version_button_pressed");
	version_info_vbc->add_child(version_btn);

	// Add a dummy control node for horizontal spacing.
	Control *h_spacer = memnew(Control);
	bottom_panel_hb->add_child(h_spacer);

	bottom_panel_raise = memnew(ToolButton);
	bottom_panel_raise->set_icon(gui_base->get_icon("ExpandBottomDock", "EditorIcons"));

	bottom_panel_raise->set_shortcut(ED_SHORTCUT("editor/bottom_panel_expand", TTR("Expand Bottom Panel"), KEY_MASK_SHIFT | KEY_F12));

	bottom_panel_hb->add_child(bottom_panel_raise);
	bottom_panel_raise->hide();
	bottom_panel_raise->set_toggle_mode(true);
	bottom_panel_raise->connect("toggled", this, "_bottom_panel_raise_toggled");

	log = memnew(EditorLog);
	ToolButton *output_button = add_bottom_panel_item(TTR("Output"), log);
	log->set_tool_button(output_button);

	old_split_ofs = 0;

	center_split->connect("resized", this, "_vp_resized");

	orphan_resources = memnew(OrphanResourcesDialog);
	gui_base->add_child(orphan_resources);

	confirmation = memnew(ConfirmationDialog);
	gui_base->add_child(confirmation);
	confirmation->connect("confirmed", this, "_menu_confirm_current");

	save_confirmation = memnew(ConfirmationDialog);
	save_confirmation->add_button(TTR("Don't Save"), OS::get_singleton()->get_swap_ok_cancel(), "discard");
	gui_base->add_child(save_confirmation);
	save_confirmation->connect("confirmed", this, "_menu_confirm_current");
	save_confirmation->connect("custom_action", this, "_discard_changes");

	custom_build_manage_templates = memnew(ConfirmationDialog);
	custom_build_manage_templates->set_text(TTR("Android build template is missing, please install relevant templates."));
	custom_build_manage_templates->get_ok()->set_text(TTR("Manage Templates"));
	custom_build_manage_templates->add_button(TTR("Install from file"))->connect("pressed", this, "_menu_option", varray(SETTINGS_INSTALL_ANDROID_BUILD_TEMPLATE));
	custom_build_manage_templates->connect("confirmed", this, "_menu_option", varray(SETTINGS_MANAGE_EXPORT_TEMPLATES));
	gui_base->add_child(custom_build_manage_templates);

	file_android_build_source = memnew(EditorFileDialog);
	file_android_build_source->set_title(TTR("Select android sources file"));
	file_android_build_source->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_android_build_source->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	file_android_build_source->add_filter("*.zip");
	file_android_build_source->connect("file_selected", this, "_android_build_source_selected");
	gui_base->add_child(file_android_build_source);

	install_android_build_template = memnew(ConfirmationDialog);
	install_android_build_template->set_text(TTR("This will set up your project for custom Android builds by installing the source template to \"res://android/build\".\nYou can then apply modifications and build your own custom APK on export (adding modules, changing the AndroidManifest.xml, etc.).\nNote that in order to make custom builds instead of using pre-built APKs, the \"Use Custom Build\" option should be enabled in the Android export preset."));
	install_android_build_template->get_ok()->set_text(TTR("Install"));
	install_android_build_template->connect("confirmed", this, "_menu_confirm_current");
	gui_base->add_child(install_android_build_template);

	remove_android_build_template = memnew(ConfirmationDialog);
	remove_android_build_template->set_text(TTR("The Android build template is already installed in this project and it won't be overwritten.\nRemove the \"res://android/build\" directory manually before attempting this operation again."));
	remove_android_build_template->get_ok()->set_text(TTR("Show in File Manager"));
	remove_android_build_template->connect("confirmed", this, "_menu_option", varray(FILE_EXPLORE_ANDROID_BUILD_TEMPLATES));
	gui_base->add_child(remove_android_build_template);

	file_templates = memnew(EditorFileDialog);
	file_templates->set_title(TTR("Import Templates From ZIP File"));

	gui_base->add_child(file_templates);
	file_templates->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	file_templates->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_templates->clear_filters();
	file_templates->add_filter("*.tpz ; " + TTR("Template Package"));

	file = memnew(EditorFileDialog);
	gui_base->add_child(file);
	file->set_current_dir("res://");

	file_export_lib = memnew(EditorFileDialog);
	file_export_lib->set_title(TTR("Export Library"));
	file_export_lib->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	file_export_lib->connect("file_selected", this, "_dialog_action");
	file_export_lib_merge = memnew(CheckBox);
	file_export_lib_merge->set_text(TTR("Merge With Existing"));
	file_export_lib_merge->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	file_export_lib_merge->set_pressed(true);
	file_export_lib->get_vbox()->add_child(file_export_lib_merge);
	file_export_lib_apply_xforms = memnew(CheckBox);
	file_export_lib_apply_xforms->set_text(TTR("Apply MeshInstance Transforms"));
	file_export_lib_apply_xforms->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	file_export_lib_apply_xforms->set_pressed(false);
	file_export_lib->get_vbox()->add_child(file_export_lib_apply_xforms);
	gui_base->add_child(file_export_lib);

	file_script = memnew(EditorFileDialog);
	file_script->set_title(TTR("Open & Run a Script"));
	file_script->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_script->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	List<String> sexts;
	ResourceLoader::get_recognized_extensions_for_type("Script", &sexts);
	for (List<String>::Element *E = sexts.front(); E; E = E->next()) {
		file_script->add_filter("*." + E->get());
	}
	gui_base->add_child(file_script);
	file_script->connect("file_selected", this, "_dialog_action");

	file_menu->get_popup()->connect("id_pressed", this, "_menu_option");
	file_menu->connect("about_to_show", this, "_update_file_menu_opened");
	file_menu->get_popup()->connect("popup_hide", this, "_update_file_menu_closed");

	settings_menu->get_popup()->connect("id_pressed", this, "_menu_option");

	file->connect("file_selected", this, "_dialog_action");
	file_templates->connect("file_selected", this, "_dialog_action");

	preview_gen = memnew(AudioStreamPreviewGenerator);
	add_child(preview_gen);
	//plugin stuff

	file_server = memnew(EditorFileServer);

	disk_changed = memnew(ConfirmationDialog);
	{
		VBoxContainer *vbc = memnew(VBoxContainer);
		disk_changed->add_child(vbc);

		Label *dl = memnew(Label);
		dl->set_text(TTR("The following files are newer on disk.\nWhat action should be taken?"));
		vbc->add_child(dl);

		disk_changed_list = memnew(Tree);
		vbc->add_child(disk_changed_list);
		disk_changed_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);

		disk_changed->connect("confirmed", this, "_reload_modified_scenes");
		disk_changed->connect("confirmed", this, "_reload_project_settings");
		disk_changed->get_ok()->set_text(TTR("Reload"));

		disk_changed->add_button(TTR("Resave"), !OS::get_singleton()->get_swap_ok_cancel(), "resave");
		disk_changed->connect("custom_action", this, "_resave_scenes");
	}

	gui_base->add_child(disk_changed);

	add_editor_plugin(memnew(AnimationPlayerEditorPlugin(this)));
	add_editor_plugin(memnew(CanvasItemEditorPlugin(this)));
	add_editor_plugin(memnew(SpatialEditorPlugin(this)));
	add_editor_plugin(memnew(ScriptEditorPlugin(this)));

	EditorAudioBuses *audio_bus_editor = EditorAudioBuses::register_editor();

	ScriptTextEditor::register_editor(); //register one for text scripts
	TextEditor::register_editor();

	if (AssetLibraryEditorPlugin::is_available()) {
		add_editor_plugin(memnew(AssetLibraryEditorPlugin(this)));
	} else {
		print_verbose("Asset Library not available (due to using Web editor, or SSL support disabled).");
	}

	//add interface before adding plugins

	editor_interface = memnew(EditorInterface);
	add_child(editor_interface);

	//more visually meaningful to have this later
	raise_bottom_panel_item(AnimationPlayerEditor::singleton);

	add_editor_plugin(VersionControlEditorPlugin::get_singleton());
	add_editor_plugin(memnew(ShaderEditorPlugin(this)));
	add_editor_plugin(memnew(VisualShaderEditorPlugin(this)));

	add_editor_plugin(memnew(CameraEditorPlugin(this)));
	add_editor_plugin(memnew(ThemeEditorPlugin(this)));
	add_editor_plugin(memnew(MultiMeshEditorPlugin(this)));
	add_editor_plugin(memnew(MeshInstanceEditorPlugin(this)));
	add_editor_plugin(memnew(AnimationTreeEditorPlugin(this)));
	add_editor_plugin(memnew(AnimationTreePlayerEditorPlugin(this)));
	add_editor_plugin(memnew(MeshLibraryEditorPlugin(this)));
	add_editor_plugin(memnew(StyleBoxEditorPlugin(this)));
	add_editor_plugin(memnew(SpriteEditorPlugin(this)));
	add_editor_plugin(memnew(Skeleton2DEditorPlugin(this)));
	add_editor_plugin(memnew(ParticlesEditorPlugin(this)));
	add_editor_plugin(memnew(CPUParticles2DEditorPlugin(this)));
	add_editor_plugin(memnew(CPUParticlesEditorPlugin(this)));
	add_editor_plugin(memnew(ResourcePreloaderEditorPlugin(this)));
	add_editor_plugin(memnew(ItemListEditorPlugin(this)));
	add_editor_plugin(memnew(Polygon3DEditorPlugin(this)));
	add_editor_plugin(memnew(CollisionPolygon2DEditorPlugin(this)));
	add_editor_plugin(memnew(TileSetEditorPlugin(this)));
	add_editor_plugin(memnew(TileMapEditorPlugin(this)));
	add_editor_plugin(memnew(SpriteFramesEditorPlugin(this)));
	add_editor_plugin(memnew(TextureRegionEditorPlugin(this)));
	add_editor_plugin(memnew(Particles2DEditorPlugin(this)));
	add_editor_plugin(memnew(GIProbeEditorPlugin(this)));
	add_editor_plugin(memnew(BakedLightmapEditorPlugin(this)));
	add_editor_plugin(memnew(RoomManagerEditorPlugin(this)));
	add_editor_plugin(memnew(RoomEditorPlugin(this)));
	add_editor_plugin(memnew(OccluderEditorPlugin(this)));
	add_editor_plugin(memnew(PortalEditorPlugin(this)));
	add_editor_plugin(memnew(PackedSceneEditorPlugin(this)));
	add_editor_plugin(memnew(MergeGroupEditorPlugin(this)));
	add_editor_plugin(memnew(Path2DEditorPlugin(this)));
	add_editor_plugin(memnew(PathEditorPlugin(this)));
	add_editor_plugin(memnew(Line2DEditorPlugin(this)));
	add_editor_plugin(memnew(Polygon2DEditorPlugin(this)));
	add_editor_plugin(memnew(LightOccluder2DEditorPlugin(this)));
	add_editor_plugin(memnew(NavigationPolygonEditorPlugin(this)));
	add_editor_plugin(memnew(GradientEditorPlugin(this)));
	add_editor_plugin(memnew(CollisionShape2DEditorPlugin(this)));
	add_editor_plugin(memnew(CurveEditorPlugin(this)));
	add_editor_plugin(memnew(TextureEditorPlugin(this)));
	add_editor_plugin(memnew(AudioStreamEditorPlugin(this)));
	add_editor_plugin(memnew(AudioBusesEditorPlugin(audio_bus_editor)));
	add_editor_plugin(memnew(SkeletonEditorPlugin(this)));
	add_editor_plugin(memnew(SkeletonIKEditorPlugin(this)));
	add_editor_plugin(memnew(PhysicalBonePlugin(this)));
	add_editor_plugin(memnew(MeshEditorPlugin(this)));
	add_editor_plugin(memnew(MaterialEditorPlugin(this)));
	add_editor_plugin(memnew(ViewportPreviewEditorPlugin(this)));
	add_editor_plugin(memnew(GradientTexture2DEditorPlugin(this)));
	add_editor_plugin(memnew(RayCast2DEditorPlugin(this)));
	add_editor_plugin(memnew(BitMapEditorPlugin(this)));

	for (int i = 0; i < EditorPlugins::get_plugin_count(); i++) {
		add_editor_plugin(EditorPlugins::create(i, this));
	}

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
		Ref<SpatialMaterialConversionPlugin> spatial_mat_convert;
		spatial_mat_convert.instance();
		resource_conversion_plugins.push_back(spatial_mat_convert);

		Ref<CanvasItemMaterialConversionPlugin> canvas_item_mat_convert;
		canvas_item_mat_convert.instance();
		resource_conversion_plugins.push_back(canvas_item_mat_convert);

		Ref<ParticlesMaterialConversionPlugin> particles_mat_convert;
		particles_mat_convert.instance();
		resource_conversion_plugins.push_back(particles_mat_convert);

		Ref<VisualShaderConversionPlugin> vshader_convert;
		vshader_convert.instance();
		resource_conversion_plugins.push_back(vshader_convert);
	}
	update_spinner_step_msec = OS::get_singleton()->get_ticks_msec();
	update_spinner_step_frame = Engine::get_singleton()->get_frames_drawn();
	update_spinner_step = 0;

	editor_plugin_screen = nullptr;
	editor_plugins_over = memnew(EditorPluginList);
	editor_plugins_force_over = memnew(EditorPluginList);
	editor_plugins_force_input_forwarding = memnew(EditorPluginList);

	Ref<EditorExportTextSceneToBinaryPlugin> export_text_to_binary_plugin;
	export_text_to_binary_plugin.instance();

	EditorExport::get_singleton()->add_export_plugin(export_text_to_binary_plugin);

	_edit_current();
	current = nullptr;
	saving_resource = Ref<Resource>();

	reference_resource_mem = true;
	save_external_resources_mem = true;

	set_process(true);

	open_imported = memnew(ConfirmationDialog);
	open_imported->get_ok()->set_text(TTR("Open Anyway"));
	new_inherited_button = open_imported->add_button(TTR("New Inherited"), !OS::get_singleton()->get_swap_ok_cancel(), "inherit");
	open_imported->connect("confirmed", this, "_open_imported");
	open_imported->connect("custom_action", this, "_inherit_imported");
	gui_base->add_child(open_imported);

	saved_version = 1;
	unsaved_cache = true;
	_last_instanced_scene = nullptr;

	quick_open = memnew(EditorQuickOpen);
	gui_base->add_child(quick_open);
	quick_open->connect("quick_open", this, "_quick_opened");

	quick_run = memnew(EditorQuickOpen);
	gui_base->add_child(quick_run);
	quick_run->connect("quick_open", this, "_quick_run");

	_update_recent_scenes();

	editor_data.restore_editor_global_states();
	convert_old = false;
	opening_prev = false;
	set_process_unhandled_input(true);
	_playing_edited = false;

	load_errors = memnew(RichTextLabel);
	load_error_dialog = memnew(AcceptDialog);
	load_error_dialog->add_child(load_errors);
	load_error_dialog->set_title(TTR("Load Errors"));
	gui_base->add_child(load_error_dialog);

	execute_outputs = memnew(RichTextLabel);
	execute_outputs->set_selection_enabled(true);
	execute_output_dialog = memnew(AcceptDialog);
	execute_output_dialog->add_child(execute_outputs);
	execute_output_dialog->set_title("");
	// Prevent closing too fast and missing important information.
	execute_output_dialog->set_exclusive(true);
	gui_base->add_child(execute_output_dialog);

	EditorFileSystem::get_singleton()->connect("sources_changed", this, "_sources_changed");
	EditorFileSystem::get_singleton()->connect("filesystem_changed", this, "_fs_changed");
	EditorFileSystem::get_singleton()->connect("resources_reimported", this, "_resources_reimported");
	EditorFileSystem::get_singleton()->connect("resources_reload", this, "_resources_changed");

	_build_icon_type_cache();

	Node::set_human_readable_collision_renaming(true);

	pick_main_scene = memnew(ConfirmationDialog);
	gui_base->add_child(pick_main_scene);
	pick_main_scene->get_ok()->set_text(TTR("Select"));
	pick_main_scene->connect("confirmed", this, "_menu_option", varray(SETTINGS_PICK_MAIN_SCENE));
	select_current_scene_button = pick_main_scene->add_button(TTR("Select Current"), true, "select_current");
	pick_main_scene->connect("custom_action", this, "_pick_main_scene_custom_action");

	for (int i = 0; i < _init_callbacks.size(); i++) {
		_init_callbacks[i]();
	}

	editor_data.add_edited_scene(-1);
	editor_data.set_edited_scene(0);
	_update_scene_tabs();

	import_dock->initialize_import_options();

	FileAccess::set_file_close_fail_notify_callback(_file_access_close_error_notify);

	waiting_for_first_scan = true;

	print_handler.printfunc = _print_handler;
	print_handler.userdata = this;
	add_print_handler(&print_handler);

	ResourceSaver::set_save_callback(_resource_saved);
	ResourceLoader::set_load_callback(_resource_loaded);

#ifdef OSX_ENABLED
	ED_SHORTCUT("editor/editor_2d", TTR("Open 2D Editor"), KEY_MASK_ALT | KEY_1);
	ED_SHORTCUT("editor/editor_3d", TTR("Open 3D Editor"), KEY_MASK_ALT | KEY_2);
	ED_SHORTCUT("editor/editor_script", TTR("Open Script Editor"), KEY_MASK_ALT | KEY_3);
	ED_SHORTCUT("editor/editor_assetlib", TTR("Open Asset Library"), KEY_MASK_ALT | KEY_4);
#else
	// Use the Ctrl modifier so F2 can be used to rename nodes in the scene tree dock.
	ED_SHORTCUT("editor/editor_2d", TTR("Open 2D Editor"), KEY_MASK_CTRL | KEY_F1);
	ED_SHORTCUT("editor/editor_3d", TTR("Open 3D Editor"), KEY_MASK_CTRL | KEY_F2);
	ED_SHORTCUT("editor/editor_script", TTR("Open Script Editor"), KEY_MASK_CTRL | KEY_F3);
	ED_SHORTCUT("editor/editor_assetlib", TTR("Open Asset Library"), KEY_MASK_CTRL | KEY_F4);
#endif
	ED_SHORTCUT("editor/editor_next", TTR("Open the next Editor"));
	ED_SHORTCUT("editor/editor_prev", TTR("Open the previous Editor"));

	screenshot_timer = memnew(Timer);
	screenshot_timer->set_one_shot(true);
	screenshot_timer->set_wait_time(settings_menu->get_popup()->get_submenu_popup_delay() + 0.1f);
	screenshot_timer->connect("timeout", this, "_request_screenshot");
	add_child(screenshot_timer);
	screenshot_timer->set_owner(get_owner());

	String exec = OS::get_singleton()->get_executable_path();
	EditorSettings::get_singleton()->set_project_metadata("editor_metadata", "executable_path", exec); // Save editor executable path for third-party tools

	OS::get_singleton()->benchmark_end_measure("editor");
}

EditorNode::~EditorNode() {
	EditorInspector::cleanup_plugins();

	remove_print_handler(&print_handler);
	memdelete(EditorHelp::get_doc_data());
	memdelete(editor_selection);
	memdelete(editor_plugins_over);
	memdelete(editor_plugins_force_over);
	memdelete(editor_plugins_force_input_forwarding);
	memdelete(file_server);
	memdelete(progress_hb);

	EditorSettings::destroy();
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

bool EditorPluginList::forward_spatial_gui_input(Camera *p_camera, const Ref<InputEvent> &p_event, bool serve_when_force_input_enabled) {
	bool discard = false;

	for (int i = 0; i < plugins_list.size(); i++) {
		if ((!serve_when_force_input_enabled) && plugins_list[i]->is_input_event_forwarding_always_enabled()) {
			continue;
		}

		if (plugins_list[i]->forward_spatial_gui_input(p_camera, p_event)) {
			discard = true;
		}
	}

	return discard;
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

void EditorPluginList::forward_spatial_draw_over_viewport(Control *p_overlay) {
	for (int i = 0; i < plugins_list.size(); i++) {
		plugins_list[i]->forward_spatial_draw_over_viewport(p_overlay);
	}
}

void EditorPluginList::forward_spatial_force_draw_over_viewport(Control *p_overlay) {
	for (int i = 0; i < plugins_list.size(); i++) {
		plugins_list[i]->forward_spatial_force_draw_over_viewport(p_overlay);
	}
}

void EditorPluginList::add_plugin(EditorPlugin *p_plugin) {
	plugins_list.push_back(p_plugin);
}

void EditorPluginList::remove_plugin(EditorPlugin *p_plugin) {
	plugins_list.erase(p_plugin);
}

bool EditorPluginList::empty() {
	return plugins_list.empty();
}

void EditorPluginList::clear() {
	plugins_list.clear();
}

EditorPluginList::EditorPluginList() {
}

EditorPluginList::~EditorPluginList() {
}
