/**************************************************************************/
/*  animation_player_editor_plugin.cpp                                    */
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

#include "animation_player_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/os/keyboard.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/inspector_dock.h"
#include "editor/plugins/canvas_item_editor_plugin.h" // For onion skinning.
#include "editor/plugins/node_3d_editor_plugin.h" // For onion skinning.
#include "editor/scene_tree_dock.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/animation/animation_tree.h"
#include "scene/gui/separator.h"
#include "scene/main/window.h"
#include "scene/resources/animation.h"
#include "scene/resources/image_texture.h"
#include "servers/rendering_server.h"

///////////////////////////////////

void AnimationPlayerEditor::_node_removed(Node *p_node) {
	if (player && original_node == p_node) {
		if (is_dummy) {
			plugin->_clear_dummy_player();
		}

		player = nullptr;

		set_process(false);

		track_editor->set_animation(Ref<Animation>(), true);
		track_editor->set_root(nullptr);
		track_editor->show_select_node_warning(true);
		_update_player();

		_ensure_dummy_player();

		pin->set_pressed(false);
	}
}

void AnimationPlayerEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS: {
			if (!player || is_dummy) {
				track_editor->show_inactive_player_warning(false);
			} else {
				track_editor->show_inactive_player_warning(!player->is_active());
			}

			if (!player) {
				return;
			}

			updating = true;

			if (player->is_playing()) {
				{
					String animname = player->get_assigned_animation();

					if (player->has_animation(animname)) {
						Ref<Animation> anim = player->get_animation(animname);
						if (anim.is_valid()) {
							frame->set_max((double)anim->get_length());
						}
					}
				}
				frame->set_value(player->get_current_animation_position());
				track_editor->set_anim_pos(player->get_current_animation_position());
			} else if (!player->is_valid()) {
				// Reset timeline when the player has been stopped externally
				frame->set_value(0);
			} else if (last_active) {
				// Need the last frame after it stopped.
				frame->set_value(player->get_current_animation_position());
				track_editor->set_anim_pos(player->get_current_animation_position());
				stop->set_button_icon(stop_icon);
			}

			last_active = player->is_playing();

			updating = false;
		} break;

		case NOTIFICATION_ENTER_TREE: {
			tool_anim->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &AnimationPlayerEditor::_animation_tool_menu));

			onion_skinning->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &AnimationPlayerEditor::_onion_skinning_menu));

			blend_editor.next->connect(SceneStringName(item_selected), callable_mp(this, &AnimationPlayerEditor::_blend_editor_next_changed));

			get_tree()->connect(SNAME("node_removed"), callable_mp(this, &AnimationPlayerEditor::_node_removed));

			add_theme_style_override(SceneStringName(panel), EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SceneStringName(panel), SNAME("Panel")));
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorThemeManager::is_generated_theme_outdated()) {
				add_theme_style_override(SceneStringName(panel), EditorNode::get_singleton()->get_editor_theme()->get_stylebox(SceneStringName(panel), SNAME("Panel")));
			}
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			stop_icon = get_editor_theme_icon(SNAME("Stop"));
			pause_icon = get_editor_theme_icon(SNAME("Pause"));
			if (player && player->is_playing()) {
				stop->set_button_icon(pause_icon);
			} else {
				stop->set_button_icon(stop_icon);
			}

			autoplay->set_button_icon(get_editor_theme_icon(SNAME("AutoPlay")));
			play->set_button_icon(get_editor_theme_icon(SNAME("PlayStart")));
			play_from->set_button_icon(get_editor_theme_icon(SNAME("Play")));
			play_bw->set_button_icon(get_editor_theme_icon(SNAME("PlayStartBackwards")));
			play_bw_from->set_button_icon(get_editor_theme_icon(SNAME("PlayBackwards")));

			autoplay_icon = get_editor_theme_icon(SNAME("AutoPlay"));
			reset_icon = get_editor_theme_icon(SNAME("Reload"));
			{
				Ref<Image> autoplay_img = autoplay_icon->get_image();
				Ref<Image> reset_img = reset_icon->get_image();
				Size2 icon_size = autoplay_img->get_size();
				Ref<Image> autoplay_reset_img = Image::create_empty(icon_size.x * 2, icon_size.y, false, autoplay_img->get_format());
				autoplay_reset_img->blit_rect(autoplay_img, Rect2i(Point2i(), icon_size), Point2i());
				autoplay_reset_img->blit_rect(reset_img, Rect2i(Point2i(), icon_size), Point2i(icon_size.x, 0));
				autoplay_reset_icon = ImageTexture::create_from_image(autoplay_reset_img);
			}

			onion_toggle->set_button_icon(get_editor_theme_icon(SNAME("Onion")));
			onion_skinning->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));

			pin->set_button_icon(get_editor_theme_icon(SNAME("Pin")));

			tool_anim->add_theme_style_override(CoreStringName(normal), get_theme_stylebox(CoreStringName(normal), SNAME("Button")));
			track_editor->get_edit_menu()->add_theme_style_override(CoreStringName(normal), get_theme_stylebox(CoreStringName(normal), SNAME("Button")));

#define ITEM_ICON(m_item, m_icon) tool_anim->get_popup()->set_item_icon(tool_anim->get_popup()->get_item_index(m_item), get_editor_theme_icon(SNAME(m_icon)))

			ITEM_ICON(TOOL_NEW_ANIM, "New");
			ITEM_ICON(TOOL_ANIM_LIBRARY, "AnimationLibrary");
			ITEM_ICON(TOOL_DUPLICATE_ANIM, "Duplicate");
			ITEM_ICON(TOOL_RENAME_ANIM, "Rename");
			ITEM_ICON(TOOL_EDIT_TRANSITIONS, "Blend");
			ITEM_ICON(TOOL_EDIT_RESOURCE, "Edit");
			ITEM_ICON(TOOL_REMOVE_ANIM, "Remove");

			_update_animation_list_icons();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			_ensure_dummy_player();
		} break;
	}
}

void AnimationPlayerEditor::_autoplay_pressed() {
	if (updating) {
		return;
	}
	if (animation->has_selectable_items() == 0) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	String current = animation->get_item_text(animation->get_selected());
	if (player->get_autoplay() == current) {
		//unset
		undo_redo->create_action(TTR("Toggle Autoplay"));
		undo_redo->add_do_method(player, "set_autoplay", "");
		undo_redo->add_undo_method(player, "set_autoplay", player->get_autoplay());
		undo_redo->add_do_method(this, "_animation_player_changed", player);
		undo_redo->add_undo_method(this, "_animation_player_changed", player);
		undo_redo->commit_action();

	} else {
		//set
		undo_redo->create_action(TTR("Toggle Autoplay"));
		undo_redo->add_do_method(player, "set_autoplay", current);
		undo_redo->add_undo_method(player, "set_autoplay", player->get_autoplay());
		undo_redo->add_do_method(this, "_animation_player_changed", player);
		undo_redo->add_undo_method(this, "_animation_player_changed", player);
		undo_redo->commit_action();
	}
}

void AnimationPlayerEditor::_go_to_nearest_keyframe(bool p_backward) {
	if (_get_current().is_empty()) {
		return;
	}

	Ref<Animation> anim = player->get_animation(player->get_assigned_animation());

	double current_time = player->get_current_animation_position();
	// Offset the time to avoid finding the same keyframe with Animation::track_find_key().
	double time_offset = MAX(CMP_EPSILON * 2, current_time * CMP_EPSILON * 2);
	double current_time_offset = current_time + (p_backward ? -time_offset : time_offset);

	float nearest_key_time = p_backward ? 0 : anim->get_length();
	int track_count = anim->get_track_count();
	bool bezier_active = track_editor->is_bezier_editor_active();

	Node *root = get_tree()->get_edited_scene_root();
	EditorSelection *selection = EditorNode::get_singleton()->get_editor_selection();

	Vector<int> selected_tracks;
	for (int i = 0; i < track_count; ++i) {
		if (selection->is_selected(root->get_node_or_null(anim->track_get_path(i)))) {
			selected_tracks.push_back(i);
		}
	}

	// Find the nearest keyframe in selection if the scene has selected nodes
	// or the nearest keyframe in the entire animation otherwise.
	if (selected_tracks.size() > 0) {
		for (int track : selected_tracks) {
			if (bezier_active && anim->track_get_type(track) != Animation::TYPE_BEZIER) {
				continue;
			}
			int key = anim->track_find_key(track, current_time_offset, Animation::FIND_MODE_NEAREST, false, !p_backward);
			if (key == -1) {
				continue;
			}
			double key_time = anim->track_get_key_time(track, key);
			if ((p_backward && key_time > nearest_key_time) || (!p_backward && key_time < nearest_key_time)) {
				nearest_key_time = key_time;
			}
		}
	} else {
		for (int track = 0; track < track_count; ++track) {
			if (bezier_active && anim->track_get_type(track) != Animation::TYPE_BEZIER) {
				continue;
			}
			int key = anim->track_find_key(track, current_time_offset, Animation::FIND_MODE_NEAREST, false, !p_backward);
			if (key == -1) {
				continue;
			}
			double key_time = anim->track_get_key_time(track, key);
			if ((p_backward && key_time > nearest_key_time) || (!p_backward && key_time < nearest_key_time)) {
				nearest_key_time = key_time;
			}
		}
	}

	player->seek_internal(nearest_key_time, true, true, true);
	frame->set_value(nearest_key_time);
	track_editor->set_anim_pos(nearest_key_time);
}

void AnimationPlayerEditor::_play_pressed() {
	String current = _get_current();

	if (!current.is_empty()) {
		if (current == player->get_assigned_animation()) {
			player->stop(); //so it won't blend with itself
		}
		ERR_FAIL_COND_EDMSG(!_validate_tracks(player->get_animation(current)), "Animation tracks may have any invalid key, abort playing.");
		PackedStringArray markers = track_editor->get_selected_section();
		if (markers.size() == 2) {
			StringName start_marker = markers[0];
			StringName end_marker = markers[1];
			player->play_section_with_markers(current, start_marker, end_marker);
		} else {
			player->play(current);
		}
	}

	//unstop
	stop->set_button_icon(pause_icon);
}

void AnimationPlayerEditor::_play_from_pressed() {
	String current = _get_current();

	if (!current.is_empty()) {
		double time = player->get_current_animation_position();
		if (current == player->get_assigned_animation() && player->is_playing()) {
			player->clear_caches(); //so it won't blend with itself
		}
		ERR_FAIL_COND_EDMSG(!_validate_tracks(player->get_animation(current)), "Animation tracks may have any invalid key, abort playing.");
		player->seek_internal(time, true, true, true);
		PackedStringArray markers = track_editor->get_selected_section();
		if (markers.size() == 2) {
			StringName start_marker = markers[0];
			StringName end_marker = markers[1];
			player->play_section_with_markers(current, start_marker, end_marker);
		} else {
			player->play(current);
		}
	}

	//unstop
	stop->set_button_icon(pause_icon);
}

String AnimationPlayerEditor::_get_current() const {
	String current;
	if (animation->get_selected() >= 0 && animation->get_selected() < animation->get_item_count() && !animation->is_item_separator(animation->get_selected())) {
		current = animation->get_item_text(animation->get_selected());
	}
	return current;
}
void AnimationPlayerEditor::_play_bw_pressed() {
	String current = _get_current();
	if (!current.is_empty()) {
		if (current == player->get_assigned_animation()) {
			player->stop(); //so it won't blend with itself
		}
		ERR_FAIL_COND_EDMSG(!_validate_tracks(player->get_animation(current)), "Animation tracks may have any invalid key, abort playing.");
		PackedStringArray markers = track_editor->get_selected_section();
		if (markers.size() == 2) {
			StringName start_marker = markers[0];
			StringName end_marker = markers[1];
			player->play_section_with_markers_backwards(current, start_marker, end_marker);
		} else {
			player->play_backwards(current);
		}
	}

	//unstop
	stop->set_button_icon(pause_icon);
}

void AnimationPlayerEditor::_play_bw_from_pressed() {
	String current = _get_current();

	if (!current.is_empty()) {
		double time = player->get_current_animation_position();
		if (current == player->get_assigned_animation() && player->is_playing()) {
			player->clear_caches(); //so it won't blend with itself
		}
		ERR_FAIL_COND_EDMSG(!_validate_tracks(player->get_animation(current)), "Animation tracks may have any invalid key, abort playing.");
		player->seek_internal(time, true, true, true);
		PackedStringArray markers = track_editor->get_selected_section();
		if (markers.size() == 2) {
			StringName start_marker = markers[0];
			StringName end_marker = markers[1];
			player->play_section_with_markers_backwards(current, start_marker, end_marker);
		} else {
			player->play_backwards(current);
		}
	}

	//unstop
	stop->set_button_icon(pause_icon);
}

void AnimationPlayerEditor::_stop_pressed() {
	if (!player) {
		return;
	}

	if (player->is_playing()) {
		player->pause();
	} else {
		String current = _get_current();
		player->stop();
		player->set_assigned_animation(current);
		frame->set_value(0);
		track_editor->set_anim_pos(0);
	}
	stop->set_button_icon(stop_icon);
}

void AnimationPlayerEditor::_animation_selected(int p_which) {
	if (updating) {
		return;
	}

#define ITEM_CHECK_DISABLED(m_item) tool_anim->get_popup()->set_item_disabled(tool_anim->get_popup()->get_item_index(m_item), true)
	ITEM_CHECK_DISABLED(TOOL_RENAME_ANIM);
	ITEM_CHECK_DISABLED(TOOL_DUPLICATE_ANIM);
	ITEM_CHECK_DISABLED(TOOL_REMOVE_ANIM);

	ITEM_CHECK_DISABLED(TOOL_EDIT_TRANSITIONS);
	ITEM_CHECK_DISABLED(TOOL_EDIT_RESOURCE);
#undef ITEM_CHECK_DISABLED

	// when selecting an animation, the idea is that the only interesting behavior
	// ui-wise is that it should play/blend the next one if currently playing
	String current = _get_current();

	if (!current.is_empty()) {
		player->set_assigned_animation(current);

		Ref<Animation> anim = player->get_animation(current);
		ERR_FAIL_COND(anim.is_null());
		{
			bool animation_is_readonly = EditorNode::get_singleton()->is_resource_read_only(anim);

			track_editor->set_animation(anim, animation_is_readonly);
			Node *root = player->get_node_or_null(player->get_root_node());

			// Player shouldn't access parent if it's the scene root.
			if (!root || (player == get_tree()->get_edited_scene_root() && player->get_root_node() == SceneStringName(path_pp))) {
				NodePath cached_root_path = player->get_path_to(get_cached_root_node());
				if (player->get_node_or_null(cached_root_path) != nullptr) {
					player->set_root_node(cached_root_path);
				} else {
					player->set_root_node(SceneStringName(path_pp)); // No other choice, preventing crash.
				}
			} else {
				cached_root_node_id = root->get_instance_id(); // Caching as `track_editor` can lose track of player's root node.
				track_editor->set_root(root);
			}
		}
		frame->set_max((double)anim->get_length());
		autoplay->set_pressed(current == player->get_autoplay());
		player->stop();
	} else {
		track_editor->set_animation(Ref<Animation>(), true);
		track_editor->set_root(nullptr);
		autoplay->set_pressed(false);
	}

	AnimationPlayerEditor::get_singleton()->get_track_editor()->update_keying();
	_animation_key_editor_seek(timeline_position);

	emit_signal("animation_selected", current);
}

void AnimationPlayerEditor::_animation_new() {
	int count = 1;
	String base = "new_animation";
	String current_library_name = "";
	if (animation->has_selectable_items()) {
		String current_animation_name = animation->get_item_text(animation->get_selected());
		Ref<Animation> current_animation = player->get_animation(current_animation_name);
		if (current_animation.is_valid()) {
			current_library_name = player->find_animation_library(current_animation);
		}
	}
	String attempt_prefix = (current_library_name == "") ? "" : current_library_name + "/";
	while (true) {
		String attempt = base;
		if (count > 1) {
			attempt += vformat("_%d", count);
		}
		if (player->has_animation(attempt_prefix + attempt)) {
			count++;
			continue;
		}
		base = attempt;
		break;
	}

	_update_name_dialog_library_dropdown();

	name_dialog_op = TOOL_NEW_ANIM;
	name_dialog->set_title(TTR("Create New Animation"));
	name_dialog->popup_centered(Size2(300, 90));
	name_title->set_text(TTR("New Animation Name:"));
	name->set_text(base);
	name->select_all();
	name->grab_focus();
}

void AnimationPlayerEditor::_animation_rename() {
	if (!animation->has_selectable_items()) {
		return;
	}
	int selected = animation->get_selected();
	String selected_name = animation->get_item_text(selected);

	// Remove library prefix if present.
	if (selected_name.contains_char('/')) {
		selected_name = selected_name.get_slice("/", 1);
	}

	name_dialog->set_title(TTR("Rename Animation"));
	name_title->set_text(TTR("Change Animation Name:"));
	name->set_text(selected_name);
	name_dialog_op = TOOL_RENAME_ANIM;
	name_dialog->popup_centered(Size2(300, 90));
	name->select_all();
	name->grab_focus();
	library->hide();
}

void AnimationPlayerEditor::_animation_remove() {
	if (!animation->has_selectable_items()) {
		return;
	}

	String current = animation->get_item_text(animation->get_selected());

	delete_dialog->set_text(vformat(TTR("Delete Animation '%s'?"), current));
	delete_dialog->popup_centered();
}

void AnimationPlayerEditor::_animation_remove_confirmed() {
	String current = animation->get_item_text(animation->get_selected());
	Ref<Animation> anim = player->get_animation(current);

	Ref<AnimationLibrary> al = player->get_animation_library(player->find_animation_library(anim));
	ERR_FAIL_COND(al.is_null());

	// For names of form lib_name/anim_name, remove library name prefix.
	if (current.contains_char('/')) {
		current = current.get_slice("/", 1);
	}
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Remove Animation"));
	if (player->get_autoplay() == current) {
		undo_redo->add_do_method(player, "set_autoplay", "");
		undo_redo->add_undo_method(player, "set_autoplay", current);
		// Avoid having the autoplay icon linger around if there is only one animation in the player.
		undo_redo->add_do_method(this, "_animation_player_changed", player);
	}
	undo_redo->add_do_method(al.ptr(), "remove_animation", current);
	undo_redo->add_undo_method(al.ptr(), "add_animation", current, anim);
	undo_redo->add_do_method(this, "_animation_player_changed", player);
	undo_redo->add_undo_method(this, "_animation_player_changed", player);
	if (animation->has_selectable_items() && animation->get_selectable_item(false) == animation->get_selectable_item(true)) { // Last item remaining.
		undo_redo->add_do_method(this, "_stop_onion_skinning");
		undo_redo->add_undo_method(this, "_start_onion_skinning");
	}
	undo_redo->commit_action();
}

void AnimationPlayerEditor::_select_anim_by_name(const String &p_anim) {
	int idx = -1;
	for (int i = 0; i < animation->get_item_count(); i++) {
		if (animation->get_item_text(i) == p_anim) {
			idx = i;
			break;
		}
	}

	ERR_FAIL_COND(idx == -1);

	animation->select(idx);

	_animation_selected(idx);
}

float AnimationPlayerEditor::_get_editor_step() const {
	const String current = player->get_assigned_animation();
	const Ref<Animation> anim = player->get_animation(current);
	ERR_FAIL_COND_V(anim.is_null(), 0.0);

	float step = track_editor->get_snap_unit();

	// Use more precise snapping when holding Shift
	return Input::get_singleton()->is_key_pressed(Key::SHIFT) ? step * 0.25 : step;
}

void AnimationPlayerEditor::_animation_name_edited() {
	if (player->is_playing()) {
		player->stop();
	}

	String new_name = name->get_text();
	if (!AnimationLibrary::is_valid_animation_name(new_name)) {
		error_dialog->set_text(TTR("Invalid animation name!"));
		error_dialog->popup_centered();
		return;
	}

	if (name_dialog_op == TOOL_RENAME_ANIM && animation->has_selectable_items() && animation->get_item_text(animation->get_selected()) == new_name) {
		name_dialog->hide();
		return;
	}

	String test_name_prefix = "";
	if (library->is_visible() && library->get_selected_id() != -1) {
		test_name_prefix = library->get_item_metadata(library->get_selected_id());
		test_name_prefix += (test_name_prefix != "") ? "/" : "";
	}

	if (player->has_animation(test_name_prefix + new_name)) {
		error_dialog->set_text(vformat(TTR("Animation '%s' already exists!"), test_name_prefix + new_name));
		error_dialog->popup_centered();
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	switch (name_dialog_op) {
		case TOOL_RENAME_ANIM: {
			String current = animation->get_item_text(animation->get_selected());
			Ref<Animation> anim = player->get_animation(current);

			Ref<AnimationLibrary> al = player->get_animation_library(player->find_animation_library(anim));
			ERR_FAIL_COND(al.is_null());

			// Extract library prefix if present.
			String new_library_prefix = "";
			if (current.contains_char('/')) {
				new_library_prefix = current.get_slice("/", 0) + "/";
				current = current.get_slice("/", 1);
			}

			undo_redo->create_action(TTR("Rename Animation"));
			undo_redo->add_do_method(al.ptr(), "rename_animation", current, new_name);
			undo_redo->add_do_method(anim.ptr(), "set_name", new_name);
			undo_redo->add_undo_method(al.ptr(), "rename_animation", new_name, current);
			undo_redo->add_undo_method(anim.ptr(), "set_name", current);
			undo_redo->add_do_method(this, "_animation_player_changed", player);
			undo_redo->add_undo_method(this, "_animation_player_changed", player);
			undo_redo->commit_action();

			if (is_dummy) {
				plugin->_update_dummy_player(original_node);
			}
			_select_anim_by_name(new_library_prefix + new_name);
		} break;

		case TOOL_NEW_ANIM: {
			Ref<Animation> new_anim = Ref<Animation>(memnew(Animation));
			new_anim->set_name(new_name);

			if (animation->get_item_count() > 0) {
				String current = animation->get_item_text(animation->get_selected());
				Ref<Animation> current_anim = player->get_animation(current);

				if (current_anim.is_valid()) {
					new_anim->set_step(current_anim->get_step());
				}
			}

			String library_name;
			Ref<AnimationLibrary> al;
			library_name = library->get_item_metadata(library->get_selected());
			// It's possible that [Global] was selected, but doesn't exist yet.
			if (player->has_animation_library(library_name)) {
				al = player->get_animation_library(library_name);
			}

			undo_redo->create_action(TTR("Add Animation"));

			bool lib_added = false;
			if (al.is_null()) {
				al.instantiate();
				lib_added = true;
				undo_redo->add_do_method(fetch_mixer_for_library(), "add_animation_library", "", al);
				library_name = "";
			}

			undo_redo->add_do_method(al.ptr(), "add_animation", new_name, new_anim);
			undo_redo->add_undo_method(al.ptr(), "remove_animation", new_name);
			undo_redo->add_do_method(this, "_animation_player_changed", player);
			undo_redo->add_undo_method(this, "_animation_player_changed", player);
			if (!animation->has_selectable_items()) {
				undo_redo->add_do_method(this, "_start_onion_skinning");
				undo_redo->add_undo_method(this, "_stop_onion_skinning");
			}
			if (lib_added) {
				undo_redo->add_undo_method(fetch_mixer_for_library(), "remove_animation_library", "");
			}
			undo_redo->commit_action();

			if (library_name != "") {
				library_name = library_name + "/";
			}

			if (is_dummy) {
				plugin->_update_dummy_player(original_node);
			}
			_select_anim_by_name(library_name + new_name);

		} break;

		case TOOL_DUPLICATE_ANIM: {
			String current = animation->get_item_text(animation->get_selected());
			Ref<Animation> anim = player->get_animation(current);

			Ref<Animation> new_anim = _animation_clone(anim);
			new_anim->set_name(new_name);

			String library_name;
			Ref<AnimationLibrary> al;
			if (library->is_visible()) {
				library_name = library->get_item_metadata(library->get_selected());
				// It's possible that [Global] was selected, but doesn't exist yet.
				if (player->has_animation_library(library_name)) {
					al = player->get_animation_library(library_name);
				}
			} else {
				if (player->has_animation_library("")) {
					al = player->get_animation_library("");
					library_name = "";
				}
			}

			undo_redo->create_action(TTR("Duplicate Animation"));

			bool lib_added = false;
			if (al.is_null()) {
				al.instantiate();
				lib_added = true;
				undo_redo->add_do_method(player, "add_animation_library", "", al);
				library_name = "";
			}

			undo_redo->add_do_method(al.ptr(), "add_animation", new_name, new_anim);
			undo_redo->add_undo_method(al.ptr(), "remove_animation", new_name);
			undo_redo->add_do_method(this, "_animation_player_changed", player);
			undo_redo->add_undo_method(this, "_animation_player_changed", player);
			if (lib_added) {
				undo_redo->add_undo_method(player, "remove_animation_library", "");
			}
			undo_redo->commit_action();

			if (library_name != "") {
				library_name = library_name + "/";
			}

			if (is_dummy) {
				plugin->_update_dummy_player(original_node);
			}
			_select_anim_by_name(library_name + new_name);
		} break;
	}

	name_dialog->hide();
}

void AnimationPlayerEditor::_blend_editor_next_changed(const int p_idx) {
	if (!animation->has_selectable_items()) {
		return;
	}

	String current = animation->get_item_text(animation->get_selected());

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Blend Next Changed"));
	undo_redo->add_do_method(player, "animation_set_next", current, blend_editor.next->get_item_text(p_idx));
	undo_redo->add_undo_method(player, "animation_set_next", current, player->animation_get_next(current));
	undo_redo->add_do_method(this, "_animation_player_changed", player);
	undo_redo->add_undo_method(this, "_animation_player_changed", player);
	undo_redo->commit_action();
}

void AnimationPlayerEditor::_edit_animation_blend() {
	if (updating_blends || !animation->has_selectable_items()) {
		return;
	}

	blend_editor.dialog->popup_centered(Size2(400, 400) * EDSCALE);
	_update_animation_blend();
}

void AnimationPlayerEditor::_update_animation_blend() {
	if (updating_blends || !animation->has_selectable_items()) {
		return;
	}

	blend_editor.tree->clear();

	String current = animation->get_item_text(animation->get_selected());

	List<StringName> anims;
	player->get_animation_list(&anims);
	TreeItem *root = blend_editor.tree->create_item();
	updating_blends = true;

	int i = 0;
	bool anim_found = false;
	blend_editor.next->clear();
	blend_editor.next->add_item("", i);

	for (const StringName &to : anims) {
		TreeItem *blend = blend_editor.tree->create_item(root);
		blend->set_editable(0, false);
		blend->set_editable(1, true);
		blend->set_text(0, to);
		blend->set_cell_mode(1, TreeItem::CELL_MODE_RANGE);
		blend->set_range_config(1, 0, 3600, 0.001);
		blend->set_range(1, player->get_blend_time(current, to));

		i++;
		blend_editor.next->add_item(to, i);
		if (to == player->animation_get_next(current)) {
			blend_editor.next->select(i);
			anim_found = true;
		}
	}

	// make sure we reset it else it becomes out of sync and could contain a deleted animation
	if (!anim_found) {
		blend_editor.next->select(0);
		player->animation_set_next(current, blend_editor.next->get_item_text(0));
	}

	updating_blends = false;
}

void AnimationPlayerEditor::_blend_edited() {
	if (updating_blends || !animation->has_selectable_items()) {
		return;
	}

	TreeItem *selected = blend_editor.tree->get_edited();
	if (!selected) {
		return;
	}

	String current = animation->get_item_text(animation->get_selected());

	updating_blends = true;
	String to = selected->get_text(0);
	float blend_time = selected->get_range(1);
	float prev_blend_time = player->get_blend_time(current, to);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Change Blend Time"));
	undo_redo->add_do_method(player, "set_blend_time", current, to, blend_time);
	undo_redo->add_undo_method(player, "set_blend_time", current, to, prev_blend_time);
	undo_redo->add_do_method(this, "_animation_player_changed", player);
	undo_redo->add_undo_method(this, "_animation_player_changed", player);
	undo_redo->commit_action();
	updating_blends = false;
}

void AnimationPlayerEditor::ensure_visibility() {
	if (player) {
		return; // another player is pinned, don't reset
	}

	_animation_edit();
}

Dictionary AnimationPlayerEditor::get_state() const {
	Dictionary d;

	if (!is_dummy) {
		d["visible"] = is_visible_in_tree();
		if (EditorNode::get_singleton()->get_edited_scene() && is_visible_in_tree() && player) {
			d["player"] = EditorNode::get_singleton()->get_edited_scene()->get_path_to(player);
			d["animation"] = player->get_assigned_animation();
			d["track_editor_state"] = track_editor->get_state();
		}
	}

	return d;
}

void AnimationPlayerEditor::set_state(const Dictionary &p_state) {
	if (!p_state.has("visible") || !p_state["visible"]) {
		return;
	}
	if (!EditorNode::get_singleton()->get_edited_scene()) {
		return;
	}

	if (p_state.has("player")) {
		Node *n = EditorNode::get_singleton()->get_edited_scene()->get_node(p_state["player"]);
		if (Object::cast_to<AnimationPlayer>(n) && EditorNode::get_singleton()->get_editor_selection()->is_selected(n)) {
			if (player) {
				if (player->is_connected(SNAME("animation_list_changed"), callable_mp(this, &AnimationPlayerEditor::_animation_libraries_updated))) {
					player->disconnect(SNAME("animation_list_changed"), callable_mp(this, &AnimationPlayerEditor::_animation_libraries_updated));
				}
				if (player->is_connected(SNAME("current_animation_changed"), callable_mp(this, &AnimationPlayerEditor::_current_animation_changed))) {
					player->disconnect(SNAME("current_animation_changed"), callable_mp(this, &AnimationPlayerEditor::_current_animation_changed));
				}
			}
			player = Object::cast_to<AnimationPlayer>(n);
			if (player) {
				if (!player->is_connected(SNAME("animation_list_changed"), callable_mp(this, &AnimationPlayerEditor::_animation_libraries_updated))) {
					player->connect(SNAME("animation_list_changed"), callable_mp(this, &AnimationPlayerEditor::_animation_libraries_updated), CONNECT_DEFERRED);
				}
				if (!player->is_connected(SNAME("current_animation_changed"), callable_mp(this, &AnimationPlayerEditor::_current_animation_changed))) {
					player->connect(SNAME("current_animation_changed"), callable_mp(this, &AnimationPlayerEditor::_current_animation_changed));
				}
			}

			_update_player();
			EditorNode::get_bottom_panel()->make_item_visible(this);
			set_process(true);
			ensure_visibility();

			if (p_state.has("animation")) {
				String anim = p_state["animation"];
				if (!anim.is_empty() && player->has_animation(anim)) {
					_select_anim_by_name(anim);
					_animation_edit();
				}
			}
		}
	}

	if (p_state.has("track_editor_state")) {
		track_editor->set_state(p_state["track_editor_state"]);
	}
}

void AnimationPlayerEditor::_animation_resource_edit() {
	String current = _get_current();
	if (current != String()) {
		Ref<Animation> anim = player->get_animation(current);
		EditorNode::get_singleton()->edit_resource(anim);
	}
}

void AnimationPlayerEditor::_animation_edit() {
	String current = _get_current();
	if (current != String()) {
		Ref<Animation> anim = player->get_animation(current);

		track_editor->set_animation(anim, EditorNode::get_singleton()->is_resource_read_only(anim));

		Node *root = player->get_node_or_null(player->get_root_node());
		if (root) {
			track_editor->set_root(root);
		}
	} else {
		track_editor->set_animation(Ref<Animation>(), true);
		track_editor->set_root(nullptr);
	}
}

void AnimationPlayerEditor::_scale_changed(const String &p_scale) {
	player->set_speed_scale(p_scale.to_float());
}

void AnimationPlayerEditor::_update_animation() {
	// the purpose of _update_animation is to reflect the current state
	// of the animation player in the current editor..

	updating = true;

	if (player->is_playing()) {
		stop->set_button_icon(pause_icon);
	} else {
		stop->set_button_icon(stop_icon);
	}

	scale->set_text(String::num(player->get_speed_scale(), 2));
	String current = player->get_assigned_animation();

	for (int i = 0; i < animation->get_item_count(); i++) {
		if (animation->get_item_text(i) == current) {
			animation->select(i);
			break;
		}
	}

	updating = false;
}

void AnimationPlayerEditor::_update_player() {
	updating = true;

	animation->clear();

	tool_anim->set_disabled(player == nullptr);
	pin->set_disabled(player == nullptr);
	_set_controls_disabled(player == nullptr);

	if (!player) {
		AnimationPlayerEditor::get_singleton()->get_track_editor()->update_keying();
		return;
	}

	List<StringName> libraries;
	player->get_animation_library_list(&libraries);

	int active_idx = -1;
	bool no_anims_found = true;
	bool global_animation_library_is_readonly = false;
	bool all_animation_libraries_are_readonly = libraries.size() > 0;

	for (const StringName &K : libraries) {
		if (K != StringName()) {
			animation->add_separator(K);
		}

		// Check if the global library is read-only since we want to disable options for adding/remove/renaming animations if it is.
		Ref<AnimationLibrary> anim_library = player->get_animation_library(K);
		bool is_animation_library_read_only = EditorNode::get_singleton()->is_resource_read_only(anim_library);
		if (!is_animation_library_read_only) {
			all_animation_libraries_are_readonly = false;
		} else {
			if (K == "") {
				global_animation_library_is_readonly = true;
			}
		}

		List<StringName> animlist;
		anim_library->get_animation_list(&animlist);

		for (const StringName &E : animlist) {
			String path = K;
			if (path != "") {
				path += "/";
			}
			path += E;
			animation->add_item(path);
			if (player->get_assigned_animation() == path) {
				active_idx = animation->get_selectable_item(true);
			}
			no_anims_found = false;
		}
	}
#define ITEM_CHECK_DISABLED(m_item) tool_anim->get_popup()->set_item_disabled(tool_anim->get_popup()->get_item_index(m_item), all_animation_libraries_are_readonly || (no_anims_found && global_animation_library_is_readonly))
	ITEM_CHECK_DISABLED(TOOL_NEW_ANIM);
#undef ITEM_CHECK_DISABLED

	_update_animation_list_icons();

	updating = false;
	if (active_idx != -1) {
		animation->select(active_idx);
		autoplay->set_pressed(animation->get_item_text(active_idx) == player->get_autoplay());
		_animation_selected(active_idx);
	} else if (animation->has_selectable_items()) {
		int item = animation->get_selectable_item();
		animation->select(item);
		autoplay->set_pressed(animation->get_item_text(item) == player->get_autoplay());
		_animation_selected(item);
	} else {
		_animation_selected(0);
	}

	if (no_anims_found) {
		_set_controls_disabled(true);
	} else {
		String current = animation->get_item_text(animation->get_selected());
		Ref<Animation> anim = player->get_animation(current);

		bool animation_library_is_readonly = EditorNode::get_singleton()->is_resource_read_only(anim);

		track_editor->set_animation(anim, animation_library_is_readonly);
		Node *root = player->get_node_or_null(player->get_root_node());
		if (root) {
			track_editor->set_root(root);
		}
	}

	_update_animation();
}

void AnimationPlayerEditor::_set_controls_disabled(bool p_disabled) {
	frame->set_editable(!p_disabled);

	stop->set_disabled(p_disabled);
	play->set_disabled(p_disabled);
	play_bw->set_disabled(p_disabled);
	play_bw_from->set_disabled(p_disabled);
	play_from->set_disabled(p_disabled);
	animation->set_disabled(p_disabled);
	autoplay->set_disabled(p_disabled);
	onion_toggle->set_disabled(p_disabled);
	onion_skinning->set_disabled(p_disabled);
}

void AnimationPlayerEditor::_update_animation_list_icons() {
	for (int i = 0; i < animation->get_item_count(); i++) {
		String anim_name = animation->get_item_text(i);
		if (animation->is_item_disabled(i) || animation->is_item_separator(i)) {
			continue;
		}

		Ref<Texture2D> icon;
		if (anim_name == player->get_autoplay()) {
			if (anim_name == SceneStringName(RESET)) {
				icon = autoplay_reset_icon;
			} else {
				icon = autoplay_icon;
			}
		} else if (anim_name == SceneStringName(RESET)) {
			icon = reset_icon;
		}

		animation->set_item_icon(i, icon);
	}
}

void AnimationPlayerEditor::_update_name_dialog_library_dropdown() {
	StringName current_library_name;
	if (animation->has_selectable_items()) {
		String current_animation_name = animation->get_item_text(animation->get_selected());
		Ref<Animation> current_animation = player->get_animation(current_animation_name);
		if (current_animation.is_valid()) {
			current_library_name = player->find_animation_library(current_animation);
		}
	}

	List<StringName> libraries;
	player->get_animation_library_list(&libraries);
	library->clear();

	int valid_library_count = 0;

	// When [Global] isn't present, but other libraries are, add option of creating [Global].
	int index_offset = 0;
	if (!player->has_animation_library(StringName())) {
		library->add_item(String(TTR("[Global] (create)")));
		library->set_item_metadata(0, "");
		if (!libraries.is_empty()) {
			index_offset = 1;
		}
		valid_library_count++;
	}

	int current_lib_id = index_offset; // Don't default to [Global] if it doesn't exist yet.
	for (const StringName &library_name : libraries) {
		if (!EditorNode::get_singleton()->is_resource_read_only(player->get_animation_library(library_name))) {
			library->add_item((library_name == StringName()) ? String(TTR("[Global]")) : String(library_name));
			library->set_item_metadata(valid_library_count, String(library_name));
			// Default to duplicating into same library.
			if (library_name == current_library_name) {
				current_library_name = library_name;
				current_lib_id = valid_library_count;
			}
			valid_library_count++;
		}
	}

	// If our library name is empty, but we have valid libraries, we can check here to auto assign the first
	// one which isn't a read-only library.
	bool auto_assigning_non_global_library = false;
	if (current_library_name == StringName() && valid_library_count > 0) {
		for (const StringName &library_name : libraries) {
			if (!EditorNode::get_singleton()->is_resource_read_only(player->get_animation_library(library_name))) {
				current_library_name = library_name;
				current_lib_id = 0;
				if (library_name != StringName()) {
					auto_assigning_non_global_library = true;
				}
				break;
			}
		}
	}

	if (library->get_item_count() > 0) {
		library->select(current_lib_id);
		if (library->get_item_count() > 1 || auto_assigning_non_global_library) {
			library->show();
			library->set_disabled(auto_assigning_non_global_library && library->get_item_count() == 1);
		} else {
			library->hide();
		}
	}
}

void AnimationPlayerEditor::_ensure_dummy_player() {
	bool dummy_exists = is_dummy && player && original_node;
	if (dummy_exists) {
		if (is_visible()) {
			player->set_active(true);
			original_node->set_editing(true);
		} else {
			player->set_active(false);
			original_node->set_editing(false);
		}
	}

	int selected = animation->get_selected();
	autoplay->set_disabled(selected != -1 ? (animation->get_item_text(selected).is_empty() ? true : dummy_exists) : true);

	// Show warning.
	if (track_editor) {
		track_editor->show_dummy_player_warning(dummy_exists);
	}
}

void AnimationPlayerEditor::edit(AnimationMixer *p_node, AnimationPlayer *p_player, bool p_is_dummy) {
	if (player && pin->is_pressed()) {
		return; // Ignore, pinned.
	}

	if (player) {
		if (player->is_connected(SNAME("animation_list_changed"), callable_mp(this, &AnimationPlayerEditor::_animation_libraries_updated))) {
			player->disconnect(SNAME("animation_list_changed"), callable_mp(this, &AnimationPlayerEditor::_animation_libraries_updated));
		}
		if (player->is_connected(SNAME("current_animation_changed"), callable_mp(this, &AnimationPlayerEditor::_current_animation_changed))) {
			player->disconnect(SNAME("current_animation_changed"), callable_mp(this, &AnimationPlayerEditor::_current_animation_changed));
		}
	}

	AnimationTree *tree = Object::cast_to<AnimationTree>(p_node);

	if (tree) {
		if (tree->is_connected(SNAME("animation_player_changed"), callable_mp(this, &AnimationPlayerEditor::unpin))) {
			tree->disconnect(SNAME("animation_player_changed"), callable_mp(this, &AnimationPlayerEditor::unpin));
		}
	}

	original_node = p_node;
	player = p_player;
	is_dummy = p_is_dummy;

	if (tree) {
		if (!tree->is_connected(SNAME("animation_player_changed"), callable_mp(this, &AnimationPlayerEditor::unpin))) {
			tree->connect(SNAME("animation_player_changed"), callable_mp(this, &AnimationPlayerEditor::unpin));
		}
	}

	if (player) {
		if (!player->is_connected(SNAME("animation_list_changed"), callable_mp(this, &AnimationPlayerEditor::_animation_libraries_updated))) {
			player->connect(SNAME("animation_list_changed"), callable_mp(this, &AnimationPlayerEditor::_animation_libraries_updated), CONNECT_DEFERRED);
		}
		if (!player->is_connected(SNAME("current_animation_changed"), callable_mp(this, &AnimationPlayerEditor::_current_animation_changed))) {
			player->connect(SNAME("current_animation_changed"), callable_mp(this, &AnimationPlayerEditor::_current_animation_changed));
		}
		_update_player();

		if (onion.enabled) {
			if (animation->has_selectable_items()) {
				_start_onion_skinning();
			} else {
				_stop_onion_skinning();
			}
		}

		track_editor->show_select_node_warning(false);
	} else {
		if (onion.enabled) {
			_stop_onion_skinning();
		}

		track_editor->show_select_node_warning(true);
	}

	library_editor->set_animation_mixer(fetch_mixer_for_library());

	_ensure_dummy_player();
}

void AnimationPlayerEditor::forward_force_draw_over_viewport(Control *p_overlay) {
	if (!onion.can_overlay) {
		return;
	}

	// Can happen on viewport resize, at least.
	if (!_are_onion_layers_valid()) {
		return;
	}

	RID ci = p_overlay->get_canvas_item();
	Rect2 src_rect = p_overlay->get_global_rect();
	// Re-flip since captures are already flipped.
	src_rect.position.y = onion.capture_size.y - (src_rect.position.y + src_rect.size.y);
	src_rect.size.y *= -1;

	Rect2 dst_rect = Rect2(Point2(), p_overlay->get_size());

	float alpha_step = 1.0 / (onion.steps + 1);

	uint32_t capture_idx = 0;
	if (onion.past) {
		float alpha = 0.0f;
		do {
			alpha += alpha_step;

			if (onion.captures_valid[capture_idx]) {
				RS::get_singleton()->canvas_item_add_texture_rect_region(
						ci, dst_rect, RS::get_singleton()->viewport_get_texture(onion.captures[capture_idx]), src_rect, Color(1, 1, 1, alpha));
			}

			capture_idx++;
		} while (capture_idx < onion.steps);
	}
	if (onion.future) {
		float alpha = 1.0f;
		uint32_t base_cidx = capture_idx;
		do {
			alpha -= alpha_step;

			if (onion.captures_valid[capture_idx]) {
				RS::get_singleton()->canvas_item_add_texture_rect_region(
						ci, dst_rect, RS::get_singleton()->viewport_get_texture(onion.captures[capture_idx]), src_rect, Color(1, 1, 1, alpha));
			}

			capture_idx++;
		} while (capture_idx < base_cidx + onion.steps); // In case there's the present capture at the end, skip it.
	}
}

void AnimationPlayerEditor::_animation_duplicate() {
	if (!animation->has_selectable_items()) {
		return;
	}

	String current = animation->get_item_text(animation->get_selected());
	Ref<Animation> anim = player->get_animation(current);
	if (anim.is_null()) {
		return;
	}

	int count = 2;
	String new_name = current;
	PackedStringArray split = new_name.split("_");
	int last_index = split.size() - 1;
	if (last_index > 0 && split[last_index].is_valid_int() && split[last_index].to_int() >= 0) {
		count = split[last_index].to_int();
		split.remove_at(last_index);
		new_name = String("_").join(split);
	}
	while (true) {
		String attempt = new_name;
		attempt += vformat("_%d", count);
		if (player->has_animation(attempt)) {
			count++;
			continue;
		}
		new_name = attempt;
		break;
	}

	if (new_name.contains_char('/')) {
		// Discard library prefix.
		new_name = new_name.get_slice("/", 1);
	}

	_update_name_dialog_library_dropdown();

	name_dialog_op = TOOL_DUPLICATE_ANIM;
	name_dialog->set_title(TTR("Duplicate Animation"));
	// TRANSLATORS: This is a label for the new name field in the "Duplicate Animation" dialog.
	name_title->set_text(TTR("Duplicated Animation Name:"));
	name->set_text(new_name);
	name_dialog->popup_centered(Size2(300, 90));
	name->select_all();
	name->grab_focus();
}

Ref<Animation> AnimationPlayerEditor::_animation_clone(Ref<Animation> p_anim) {
	Ref<Animation> new_anim = memnew(Animation);
	List<PropertyInfo> plist;
	p_anim->get_property_list(&plist);

	for (const PropertyInfo &E : plist) {
		if (E.usage & PROPERTY_USAGE_STORAGE) {
			new_anim->set(E.name, p_anim->get(E.name));
		}
	}
	new_anim->set_path("");

	return new_anim;
}

void AnimationPlayerEditor::_seek_value_changed(float p_value, bool p_timeline_only) {
	if (updating || !player || player->is_playing()) {
		return;
	};

	updating = true;
	String current = player->get_assigned_animation();
	if (current.is_empty() || !player->has_animation(current)) {
		updating = false;
		current = "";
		return;
	};

	Ref<Animation> anim;
	anim = player->get_animation(current);

	double pos = CLAMP((double)anim->get_length() * (p_value / frame->get_max()), 0, (double)anim->get_length());
	if (track_editor->is_snap_timeline_enabled()) {
		pos = Math::snapped(pos, _get_editor_step());
	}
	pos = CLAMP(pos, 0, (double)anim->get_length() - CMP_EPSILON2); // Hack: Avoid fposmod with LOOP_LINEAR.

	if (!p_timeline_only && anim.is_valid() && (!player->is_valid() || !Math::is_equal_approx(pos, player->get_current_animation_position()))) {
		player->seek_internal(pos, true, true, false);
	}

	track_editor->set_anim_pos(pos);
}

void AnimationPlayerEditor::_animation_player_changed(Object *p_pl) {
	_update_player();

	if (blend_editor.dialog->is_visible()) {
		_update_animation_blend(); // Update.
	}

	if (library_editor->is_visible()) {
		library_editor->update_tree();
	}
}

void AnimationPlayerEditor::_animation_libraries_updated() {
	_animation_player_changed(player);
}

void AnimationPlayerEditor::_list_changed() {
	if (is_visible_in_tree()) {
		_update_player();
	}
}

void AnimationPlayerEditor::_current_animation_changed(const String &p_name) {
	if (is_visible_in_tree()) {
		if (p_name.is_empty()) {
			// Means [stop].
			frame->set_value(0);
			track_editor->set_anim_pos(0);
			_update_animation();
			return;
		}
		Ref<Animation> anim = player->get_animation(p_name);
		if (anim.is_null()) {
			return;
		}

		// Determine the read-only status of the animation's library and the libraries as a whole.
		List<StringName> libraries;
		player->get_animation_library_list(&libraries);

		bool current_animation_library_is_readonly = false;
		bool all_animation_libraries_are_readonly = true;
		for (const StringName &K : libraries) {
			Ref<AnimationLibrary> anim_library = player->get_animation_library(K);
			bool animation_library_is_readonly = EditorNode::get_singleton()->is_resource_read_only(anim_library);
			if (!animation_library_is_readonly) {
				all_animation_libraries_are_readonly = false;
			}

			List<StringName> animlist;
			anim_library->get_animation_list(&animlist);
			bool animation_found = false;
			for (const StringName &E : animlist) {
				String path = K;
				if (path != "") {
					path += "/";
				}
				path += E;
				if (p_name == path) {
					current_animation_library_is_readonly = animation_library_is_readonly;
					break;
				}
			}
			if (animation_found) {
				break;
			}
		}

		StringName library_name = player->find_animation_library(anim);

		bool animation_is_readonly = EditorNode::get_singleton()->is_resource_read_only(anim);

		track_editor->set_animation(anim, animation_is_readonly);
		_update_animation();

#define ITEM_CHECK_DISABLED(m_item) tool_anim->get_popup()->set_item_disabled(tool_anim->get_popup()->get_item_index(m_item), false)
		ITEM_CHECK_DISABLED(TOOL_EDIT_TRANSITIONS);
		ITEM_CHECK_DISABLED(TOOL_EDIT_RESOURCE);
#undef ITEM_CHECK_DISABLED

#define ITEM_CHECK_DISABLED(m_item) tool_anim->get_popup()->set_item_disabled(tool_anim->get_popup()->get_item_index(m_item), current_animation_library_is_readonly)
		ITEM_CHECK_DISABLED(TOOL_RENAME_ANIM);
		ITEM_CHECK_DISABLED(TOOL_REMOVE_ANIM);
#undef ITEM_CHECK_DISABLED

#define ITEM_CHECK_DISABLED(m_item) tool_anim->get_popup()->set_item_disabled(tool_anim->get_popup()->get_item_index(m_item), all_animation_libraries_are_readonly)
		ITEM_CHECK_DISABLED(TOOL_DUPLICATE_ANIM);
#undef ITEM_CHECK_DISABLED
	}
}

void AnimationPlayerEditor::_animation_key_editor_anim_len_changed(float p_len) {
	frame->set_max(p_len);
}
void AnimationPlayerEditor::_animation_key_editor_seek(float p_pos, bool p_timeline_only, bool p_update_position_only) {
	timeline_position = p_pos;

	if (!is_visible_in_tree() ||
			p_update_position_only ||
			!player ||
			player->is_playing() ||
			!player->has_animation(player->get_assigned_animation())) {
		return;
	}

	updating = true;
	frame->set_value(track_editor->is_snap_timeline_enabled() ? Math::snapped(p_pos, _get_editor_step()) : p_pos);
	updating = false;
	_seek_value_changed(p_pos, p_timeline_only);
}

void AnimationPlayerEditor::_animation_update_key_frame() {
	if (player) {
		player->advance(0);
	}
}

void AnimationPlayerEditor::_animation_tool_menu(int p_option) {
	String current = _get_current();

	Ref<Animation> anim;
	if (!current.is_empty()) {
		anim = player->get_animation(current);
	}

	switch (p_option) {
		case TOOL_NEW_ANIM: {
			_animation_new();
		} break;
		case TOOL_ANIM_LIBRARY: {
			library_editor->set_animation_mixer(fetch_mixer_for_library());
			library_editor->show_dialog();
		} break;
		case TOOL_DUPLICATE_ANIM: {
			_animation_duplicate();
		} break;
		case TOOL_RENAME_ANIM: {
			_animation_rename();
		} break;
		case TOOL_EDIT_TRANSITIONS: {
			_edit_animation_blend();
		} break;
		case TOOL_REMOVE_ANIM: {
			_animation_remove();
		} break;
		case TOOL_EDIT_RESOURCE: {
			if (anim.is_valid()) {
				EditorNode::get_singleton()->edit_resource(anim);
			}
		} break;
	}
}

void AnimationPlayerEditor::_onion_skinning_menu(int p_option) {
	PopupMenu *menu = onion_skinning->get_popup();
	int idx = menu->get_item_index(p_option);

	switch (p_option) {
		case ONION_SKINNING_ENABLE: {
			onion.enabled = !onion.enabled;

			if (onion.enabled) {
				if (get_player() && !get_player()->has_animation(SceneStringName(RESET))) {
					EditorNode::get_singleton()->show_warning(TTR("Onion skinning requires a RESET animation."));
				}
				_start_onion_skinning(); // It will check for RESET animation anyway.
			} else {
				_stop_onion_skinning();
			}

		} break;
		case ONION_SKINNING_PAST: {
			// Ensure at least one of past/future is checked.
			onion.past = onion.future ? !onion.past : true;
			menu->set_item_checked(idx, onion.past);
		} break;
		case ONION_SKINNING_FUTURE: {
			// Ensure at least one of past/future is checked.
			onion.future = onion.past ? !onion.future : true;
			menu->set_item_checked(idx, onion.future);
		} break;
		case ONION_SKINNING_1_STEP: // Fall-through.
		case ONION_SKINNING_2_STEPS:
		case ONION_SKINNING_3_STEPS: {
			onion.steps = (p_option - ONION_SKINNING_1_STEP) + 1;
			int one_frame_idx = menu->get_item_index(ONION_SKINNING_1_STEP);
			for (int i = 0; i <= ONION_SKINNING_LAST_STEPS_OPTION - ONION_SKINNING_1_STEP; i++) {
				menu->set_item_checked(one_frame_idx + i, (int)onion.steps == i + 1);
			}
		} break;
		case ONION_SKINNING_DIFFERENCES_ONLY: {
			onion.differences_only = !onion.differences_only;
			menu->set_item_checked(idx, onion.differences_only);
		} break;
		case ONION_SKINNING_FORCE_WHITE_MODULATE: {
			onion.force_white_modulate = !onion.force_white_modulate;
			menu->set_item_checked(idx, onion.force_white_modulate);
		} break;
		case ONION_SKINNING_INCLUDE_GIZMOS: {
			onion.include_gizmos = !onion.include_gizmos;
			menu->set_item_checked(idx, onion.include_gizmos);
		} break;
	}
}

void AnimationPlayerEditor::shortcut_input(const Ref<InputEvent> &p_ev) {
	ERR_FAIL_COND(p_ev.is_null());

	Ref<InputEventKey> k = p_ev;
	if (is_visible_in_tree() && k.is_valid() && k->is_pressed() && !k->is_echo()) {
		if (ED_IS_SHORTCUT("animation_editor/stop_animation", p_ev)) {
			_stop_pressed();
			accept_event();
		} else if (ED_IS_SHORTCUT("animation_editor/play_animation", p_ev)) {
			_play_from_pressed();
			accept_event();
		} else if (ED_IS_SHORTCUT("animation_editor/play_animation_backwards", p_ev)) {
			_play_bw_from_pressed();
			accept_event();
		} else if (ED_IS_SHORTCUT("animation_editor/play_animation_from_start", p_ev)) {
			_play_pressed();
			accept_event();
		} else if (ED_IS_SHORTCUT("animation_editor/play_animation_from_end", p_ev)) {
			_play_bw_pressed();
			accept_event();
		} else if (ED_IS_SHORTCUT("animation_editor/go_to_next_keyframe", p_ev)) {
			_go_to_nearest_keyframe(false);
			accept_event();
		} else if (ED_IS_SHORTCUT("animation_editor/go_to_previous_keyframe", p_ev)) {
			_go_to_nearest_keyframe(true);
			accept_event();
		}
	}
}

void AnimationPlayerEditor::_editor_visibility_changed() {
	if (is_visible() && animation->has_selectable_items()) {
		_start_onion_skinning();
	}
}

bool AnimationPlayerEditor::_are_onion_layers_valid() {
	ERR_FAIL_COND_V(!onion.past && !onion.future, false);

	Size2 capture_size = DisplayServer::get_singleton()->window_get_size(DisplayServer::MAIN_WINDOW_ID);
	return onion.captures.size() == onion.get_capture_count() && onion.capture_size == capture_size;
}

void AnimationPlayerEditor::_allocate_onion_layers() {
	_free_onion_layers();

	int captures = onion.get_capture_count();
	Size2 capture_size = DisplayServer::get_singleton()->window_get_size(DisplayServer::MAIN_WINDOW_ID);

	onion.captures.resize(captures);
	onion.captures_valid.resize(captures);

	for (int i = 0; i < captures; i++) {
		bool is_present = onion.differences_only && i == captures - 1;

		// Each capture is a viewport with a canvas item attached that renders a full-size rect with the contents of the main viewport.
		onion.captures[i] = RS::get_singleton()->viewport_create();

		RS::get_singleton()->viewport_set_size(onion.captures[i], capture_size.width, capture_size.height);
		RS::get_singleton()->viewport_set_update_mode(onion.captures[i], RS::VIEWPORT_UPDATE_ALWAYS);
		RS::get_singleton()->viewport_set_transparent_background(onion.captures[i], !is_present);
		RS::get_singleton()->viewport_attach_canvas(onion.captures[i], onion.capture.canvas);
	}

	// Reset the capture canvas item to the current root viewport texture (defensive).
	RS::get_singleton()->canvas_item_clear(onion.capture.canvas_item);
	RS::get_singleton()->canvas_item_add_texture_rect(onion.capture.canvas_item, Rect2(Point2(), Point2(capture_size.x, -capture_size.y)), get_tree()->get_root()->get_texture()->get_rid());

	onion.capture_size = capture_size;
}

void AnimationPlayerEditor::_free_onion_layers() {
	for (uint32_t i = 0; i < onion.captures.size(); i++) {
		if (onion.captures[i].is_valid()) {
			RS::get_singleton()->free(onion.captures[i]);
		}
	}
	onion.captures.clear();
	onion.captures_valid.clear();
}

void AnimationPlayerEditor::_prepare_onion_layers_1() {
	// This would be called per viewport and we want to act once only.
	int64_t cur_frame = get_tree()->get_frame();
	if (cur_frame == onion.last_frame) {
		return;
	}

	if (!onion.enabled || !is_visible() || !get_player() || !get_player()->has_animation(SceneStringName(RESET))) {
		_stop_onion_skinning();
		return;
	}

	onion.last_frame = cur_frame;

	// Refresh viewports with no onion layers overlaid.
	onion.can_overlay = false;
	plugin->update_overlays();

	if (player->is_playing()) {
		return;
	}

	// And go to next step afterwards.
	callable_mp(this, &AnimationPlayerEditor::_prepare_onion_layers_2_prolog).call_deferred();
}

void AnimationPlayerEditor::_prepare_onion_layers_2_prolog() {
	Ref<Animation> anim = player->get_animation(player->get_assigned_animation());
	if (anim.is_null()) {
		return;
	}

	if (!_are_onion_layers_valid()) {
		_allocate_onion_layers();
	}

	// Hide superfluous elements that would make the overlay unnecessary cluttered.
	if (Node3DEditor::get_singleton()->is_visible()) {
		// 3D
		onion.temp.spatial_edit_state = Node3DEditor::get_singleton()->get_state();
		Dictionary new_state = onion.temp.spatial_edit_state.duplicate();
		new_state["show_grid"] = false;
		new_state["show_origin"] = false;
		Array orig_vp = onion.temp.spatial_edit_state["viewports"];
		Array vp;
		vp.resize(4);
		for (int i = 0; i < vp.size(); i++) {
			Dictionary d = ((Dictionary)orig_vp[i]).duplicate();
			d["use_environment"] = false;
			d["doppler"] = false;
			d["listener"] = false;
			d["gizmos"] = onion.include_gizmos ? d["gizmos"] : Variant(false);
			d["information"] = false;
			vp[i] = d;
		}
		new_state["viewports"] = vp;
		// TODO: Save/restore only affected entries.
		Node3DEditor::get_singleton()->set_state(new_state);
	} else {
		// CanvasItemEditor.
		onion.temp.canvas_edit_state = CanvasItemEditor::get_singleton()->get_state();
		Dictionary new_state = onion.temp.canvas_edit_state.duplicate();
		new_state["show_origin"] = false;
		new_state["show_grid"] = false;
		new_state["show_rulers"] = false;
		new_state["show_guides"] = false;
		new_state["show_helpers"] = false;
		new_state["show_zoom_control"] = false;
		new_state["show_edit_locks"] = false;
		new_state["grid_visibility"] = 2; // TODO: Expose CanvasItemEditor::GRID_VISIBILITY_HIDE somehow and use it.
		new_state["show_transformation_gizmos"] = onion.include_gizmos ? new_state["gizmos"] : Variant(false);
		// TODO: Save/restore only affected entries.
		CanvasItemEditor::get_singleton()->set_state(new_state);
	}

	// Tweak the root viewport to ensure it's rendered before our target.
	RID root_vp = get_tree()->get_root()->get_viewport_rid();
	onion.temp.screen_rect = Rect2(Vector2(), DisplayServer::get_singleton()->window_get_size(DisplayServer::MAIN_WINDOW_ID));
	RS::get_singleton()->viewport_attach_to_screen(root_vp, Rect2(), DisplayServer::INVALID_WINDOW_ID);
	RS::get_singleton()->viewport_set_update_mode(root_vp, RS::VIEWPORT_UPDATE_ALWAYS);

	RID present_rid;
	if (onion.differences_only) {
		// Capture present scene as it is.
		RS::get_singleton()->canvas_item_set_material(onion.capture.canvas_item, RID());
		present_rid = onion.captures[onion.captures.size() - 1];
		RS::get_singleton()->viewport_set_active(present_rid, true);
		RS::get_singleton()->viewport_set_parent_viewport(root_vp, present_rid);
		RS::get_singleton()->draw(false);
		RS::get_singleton()->viewport_set_active(present_rid, false);
	}

	// Backup current animation state.
	onion.temp.anim_values_backup = player->make_backup();
	onion.temp.anim_player_position = player->get_current_animation_position();

	// Render every past/future step with the capture shader.

	RS::get_singleton()->canvas_item_set_material(onion.capture.canvas_item, onion.capture.material->get_rid());
	onion.capture.material->set_shader_parameter("bkg_color", GLOBAL_GET("rendering/environment/defaults/default_clear_color"));
	onion.capture.material->set_shader_parameter("differences_only", onion.differences_only);
	onion.capture.material->set_shader_parameter("present", onion.differences_only ? RS::get_singleton()->viewport_get_texture(present_rid) : RID());
	onion.capture.material->set_shader_parameter("dir_color", onion.force_white_modulate ? Color(1, 1, 1) : Color(EDITOR_GET("editors/animation/onion_layers_past_color")));

	uint32_t p_capture_idx = 0;
	int first_step_offset = onion.past ? -(int)onion.steps : 0;
	_prepare_onion_layers_2_step_prepare(first_step_offset, p_capture_idx);
}

void AnimationPlayerEditor::_prepare_onion_layers_2_step_prepare(int p_step_offset, uint32_t p_capture_idx) {
	uint32_t next_capture_idx = p_capture_idx;
	if (p_step_offset == 0) {
		// Skip present step and switch to the color of future.
		if (!onion.force_white_modulate) {
			onion.capture.material->set_shader_parameter("dir_color", EDITOR_GET("editors/animation/onion_layers_future_color"));
		}
	} else {
		Ref<Animation> anim = player->get_animation(player->get_assigned_animation());
		double pos = onion.temp.anim_player_position + p_step_offset * anim->get_step();

		bool valid = anim->get_loop_mode() != Animation::LOOP_NONE || (pos >= 0 && pos <= anim->get_length());
		onion.captures_valid[p_capture_idx] = valid;
		if (valid) {
			player->seek_internal(pos, true, true, false);
			OS::get_singleton()->get_main_loop()->process(0);
			// This is the key: process the frame and let all callbacks/updates/notifications happen
			// so everything (transforms, skeletons, etc.) is up-to-date visually.
			callable_mp(this, &AnimationPlayerEditor::_prepare_onion_layers_2_step_capture).call_deferred(p_step_offset, p_capture_idx);
			return;
		} else {
			next_capture_idx++;
		}
	}

	int last_step_offset = onion.future ? onion.steps : 0;
	if (p_step_offset < last_step_offset) {
		_prepare_onion_layers_2_step_prepare(p_step_offset + 1, next_capture_idx);
	} else {
		_prepare_onion_layers_2_epilog();
	}
}

void AnimationPlayerEditor::_prepare_onion_layers_2_step_capture(int p_step_offset, uint32_t p_capture_idx) {
	DEV_ASSERT(p_step_offset != 0);
	DEV_ASSERT(onion.captures_valid[p_capture_idx]);

	RID root_vp = get_tree()->get_root()->get_viewport_rid();
	RS::get_singleton()->viewport_set_active(onion.captures[p_capture_idx], true);
	RS::get_singleton()->viewport_set_parent_viewport(root_vp, onion.captures[p_capture_idx]);
	RS::get_singleton()->draw(false);
	RS::get_singleton()->viewport_set_active(onion.captures[p_capture_idx], false);

	int last_step_offset = onion.future ? onion.steps : 0;
	if (p_step_offset < last_step_offset) {
		_prepare_onion_layers_2_step_prepare(p_step_offset + 1, p_capture_idx + 1);
	} else {
		_prepare_onion_layers_2_epilog();
	}
}

void AnimationPlayerEditor::_prepare_onion_layers_2_epilog() {
	// Restore root viewport.
	RID root_vp = get_tree()->get_root()->get_viewport_rid();
	RS::get_singleton()->viewport_set_parent_viewport(root_vp, RID());
	RS::get_singleton()->viewport_attach_to_screen(root_vp, onion.temp.screen_rect, DisplayServer::MAIN_WINDOW_ID);
	RS::get_singleton()->viewport_set_update_mode(root_vp, RS::VIEWPORT_UPDATE_WHEN_VISIBLE);

	// Restore animation state.
	// Here we're combine the power of seeking back to the original position and
	// restoring the values backup. In most cases they will bring the same value back,
	// but there are cases handled by one that the other can't.
	// Namely:
	// - Seeking won't restore any values that may have been modified by the user
	//   in the node after the last time the AnimationPlayer updated it.
	// - Restoring the backup won't account for values that are not directly involved
	//   in the animation but a consequence of them (e.g., SkeletonModification2DLookAt).
	// FIXME: Since backup of values is based on the reset animation, only values
	//        backed by a proper reset animation will work correctly with onion
	//        skinning and the possibility to restore the values mentioned in the
	//        first point above is gone. Still good enough.
	player->seek_internal(onion.temp.anim_player_position, true, true, false);
	player->restore(onion.temp.anim_values_backup);

	// Restore state of main editors.
	if (Node3DEditor::get_singleton()->is_visible()) {
		// 3D
		Node3DEditor::get_singleton()->set_state(onion.temp.spatial_edit_state);
	} else { // CanvasItemEditor
		// 2D
		CanvasItemEditor::get_singleton()->set_state(onion.temp.canvas_edit_state);
	}

	// Update viewports with skin layers overlaid for the actual engine loop render.
	onion.can_overlay = true;
	plugin->update_overlays();
}

void AnimationPlayerEditor::_start_onion_skinning() {
	if (get_player() && !get_player()->has_animation(SceneStringName(RESET))) {
		onion.enabled = false;
		onion_toggle->set_pressed_no_signal(false);
		return;
	}
	if (!get_tree()->is_connected(SNAME("process_frame"), callable_mp(this, &AnimationPlayerEditor::_prepare_onion_layers_1))) {
		get_tree()->connect(SNAME("process_frame"), callable_mp(this, &AnimationPlayerEditor::_prepare_onion_layers_1));
	}
}

void AnimationPlayerEditor::_stop_onion_skinning() {
	if (get_tree()->is_connected(SNAME("process_frame"), callable_mp(this, &AnimationPlayerEditor::_prepare_onion_layers_1))) {
		get_tree()->disconnect(SNAME("process_frame"), callable_mp(this, &AnimationPlayerEditor::_prepare_onion_layers_1));

		_free_onion_layers();

		// Clean up.
		onion.can_overlay = false;
		plugin->update_overlays();
		onion.temp = {};
	}
}

void AnimationPlayerEditor::_pin_pressed() {
	SceneTreeDock::get_singleton()->get_tree_editor()->update_tree();
}

AnimationMixer *AnimationPlayerEditor::fetch_mixer_for_library() const {
	if (!original_node) {
		return nullptr;
	}
	// Does AnimationTree have AnimationPlayer?
	if (original_node->is_class("AnimationTree")) {
		AnimationTree *src_tree = Object::cast_to<AnimationTree>(original_node);
		Node *src_player = src_tree->get_node_or_null(src_tree->get_animation_player());
		if (src_player) {
			return Object::cast_to<AnimationMixer>(src_player);
		}
	}
	return original_node;
}

Node *AnimationPlayerEditor::get_cached_root_node() const {
	return Object::cast_to<Node>(ObjectDB::get_instance(cached_root_node_id));
}

bool AnimationPlayerEditor::_validate_tracks(const Ref<Animation> p_anim) {
	bool is_valid = true;
	if (p_anim.is_null()) {
		return true; // There is a problem outside of the animation track.
	}
	int len = p_anim->get_track_count();
	for (int i = 0; i < len; i++) {
		Animation::TrackType ttype = p_anim->track_get_type(i);
		if (ttype == Animation::TYPE_ROTATION_3D) {
			int key_len = p_anim->track_get_key_count(i);
			for (int j = 0; j < key_len; j++) {
				Quaternion q;
				p_anim->rotation_track_get_key(i, j, &q);
				ERR_BREAK_EDMSG(!q.is_normalized(), "AnimationPlayer: '" + player->get_name() + "', Animation: '" + player->get_current_animation() + "', 3D Rotation Track:  '" + p_anim->track_get_path(i) + "' contains unnormalized Quaternion key.");
			}
		} else if (ttype == Animation::TYPE_VALUE) {
			int key_len = p_anim->track_get_key_count(i);
			if (key_len == 0) {
				continue;
			}
			switch (p_anim->track_get_key_value(i, 0).get_type()) {
				case Variant::QUATERNION: {
					for (int j = 0; j < key_len; j++) {
						Quaternion q = Quaternion(p_anim->track_get_key_value(i, j));
						if (!q.is_normalized()) {
							is_valid = false;
							ERR_BREAK_EDMSG(true, "AnimationPlayer: '" + player->get_name() + "', Animation: '" + player->get_current_animation() + "', Value Track:  '" + p_anim->track_get_path(i) + "' contains unnormalized Quaternion key.");
						}
					}
				} break;
				case Variant::TRANSFORM3D: {
					for (int j = 0; j < key_len; j++) {
						Transform3D t = Transform3D(p_anim->track_get_key_value(i, j));
						if (!t.basis.orthonormalized().is_rotation()) {
							is_valid = false;
							ERR_BREAK_EDMSG(true, "AnimationPlayer: '" + player->get_name() + "', Animation: '" + player->get_current_animation() + "', Value Track:  '" + p_anim->track_get_path(i) + "' contains corrupted basis (some axes are too close other axis or scaled by zero) Transform3D key.");
						}
					}
				} break;
				default: {
				} break;
			}
		}
	}
	return is_valid;
}

void AnimationPlayerEditor::_bind_methods() {
	// Needed for UndoRedo.
	ClassDB::bind_method(D_METHOD("_animation_player_changed"), &AnimationPlayerEditor::_animation_player_changed);
	ClassDB::bind_method(D_METHOD("_animation_update_key_frame"), &AnimationPlayerEditor::_animation_update_key_frame);
	ClassDB::bind_method(D_METHOD("_start_onion_skinning"), &AnimationPlayerEditor::_start_onion_skinning);
	ClassDB::bind_method(D_METHOD("_stop_onion_skinning"), &AnimationPlayerEditor::_stop_onion_skinning);

	ADD_SIGNAL(MethodInfo("animation_selected", PropertyInfo(Variant::STRING, "name")));
}

AnimationPlayerEditor *AnimationPlayerEditor::singleton = nullptr;

AnimationPlayer *AnimationPlayerEditor::get_player() const {
	return player;
}

AnimationMixer *AnimationPlayerEditor::get_editing_node() const {
	return original_node;
}

AnimationPlayerEditor::AnimationPlayerEditor(AnimationPlayerEditorPlugin *p_plugin) {
	plugin = p_plugin;
	singleton = this;

	set_focus_mode(FOCUS_ALL);
	set_process_shortcut_input(true);

	HBoxContainer *hb = memnew(HBoxContainer);
	add_child(hb);

	HBoxContainer *playback_container = memnew(HBoxContainer);
	playback_container->set_layout_direction(LAYOUT_DIRECTION_LTR);
	hb->add_child(playback_container);

	play_bw_from = memnew(Button);
	play_bw_from->set_theme_type_variation(SceneStringName(FlatButton));
	play_bw_from->set_tooltip_text(TTR("Play Animation Backwards"));
	playback_container->add_child(play_bw_from);

	play_bw = memnew(Button);
	play_bw->set_theme_type_variation(SceneStringName(FlatButton));
	play_bw->set_tooltip_text(TTR("Play Animation Backwards from End"));
	playback_container->add_child(play_bw);

	stop = memnew(Button);
	stop->set_theme_type_variation(SceneStringName(FlatButton));
	stop->set_tooltip_text(TTR("Pause/Stop Animation"));
	playback_container->add_child(stop);

	play = memnew(Button);
	play->set_theme_type_variation(SceneStringName(FlatButton));
	play->set_tooltip_text(TTR("Play Animation from Start"));
	playback_container->add_child(play);

	play_from = memnew(Button);
	play_from->set_theme_type_variation(SceneStringName(FlatButton));
	play_from->set_tooltip_text(TTR("Play Animation"));
	playback_container->add_child(play_from);

	frame = memnew(SpinBox);
	hb->add_child(frame);
	frame->set_custom_minimum_size(Size2(80, 0) * EDSCALE);
	frame->set_stretch_ratio(2);
	frame->set_step(0.0001);
	frame->set_tooltip_text(TTR("Animation position (in seconds)."));

	hb->add_child(memnew(VSeparator));

	scale = memnew(LineEdit);
	hb->add_child(scale);
	scale->set_h_size_flags(SIZE_EXPAND_FILL);
	scale->set_stretch_ratio(1);
	scale->set_tooltip_text(TTR("Scale animation playback globally for the node."));
	scale->hide();

	delete_dialog = memnew(ConfirmationDialog);
	add_child(delete_dialog);
	delete_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AnimationPlayerEditor::_animation_remove_confirmed));

	tool_anim = memnew(MenuButton);
	tool_anim->set_shortcut_context(this);
	tool_anim->set_flat(false);
	tool_anim->set_tooltip_text(TTR("Animation Tools"));
	tool_anim->set_text(TTR("Animation"));
	tool_anim->get_popup()->add_shortcut(ED_SHORTCUT("animation_player_editor/new_animation", TTRC("New...")), TOOL_NEW_ANIM);
	tool_anim->get_popup()->add_separator();
	tool_anim->get_popup()->add_shortcut(ED_SHORTCUT("animation_player_editor/animation_libraries", TTRC("Manage Animations...")), TOOL_ANIM_LIBRARY);
	tool_anim->get_popup()->add_separator();
	tool_anim->get_popup()->add_shortcut(ED_SHORTCUT("animation_player_editor/duplicate_animation", TTRC("Duplicate...")), TOOL_DUPLICATE_ANIM);
	tool_anim->get_popup()->add_separator();
	tool_anim->get_popup()->add_shortcut(ED_SHORTCUT("animation_player_editor/rename_animation", TTRC("Rename...")), TOOL_RENAME_ANIM);
	tool_anim->get_popup()->add_shortcut(ED_SHORTCUT("animation_player_editor/edit_transitions", TTRC("Edit Transitions...")), TOOL_EDIT_TRANSITIONS);
	tool_anim->get_popup()->add_shortcut(ED_SHORTCUT("animation_player_editor/open_animation_in_inspector", TTRC("Open in Inspector")), TOOL_EDIT_RESOURCE);
	tool_anim->get_popup()->add_separator();
	tool_anim->get_popup()->add_shortcut(ED_SHORTCUT("animation_player_editor/remove_animation", TTRC("Remove")), TOOL_REMOVE_ANIM);
	tool_anim->set_disabled(true);
	hb->add_child(tool_anim);

	animation = memnew(OptionButton);
	hb->add_child(animation);
	animation->set_h_size_flags(SIZE_EXPAND_FILL);
	animation->set_tooltip_text(TTR("Display list of animations in player."));
	animation->set_clip_text(true);
	animation->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

	autoplay = memnew(Button);
	autoplay->set_theme_type_variation(SceneStringName(FlatButton));
	hb->add_child(autoplay);
	autoplay->set_tooltip_text(TTR("Autoplay on Load"));

	hb->add_child(memnew(VSeparator));

	track_editor = memnew(AnimationTrackEditor);
	hb->add_child(track_editor->get_edit_menu());

	hb->add_child(memnew(VSeparator));

	onion_toggle = memnew(Button);
	onion_toggle->set_theme_type_variation(SceneStringName(FlatButton));
	onion_toggle->set_toggle_mode(true);
	onion_toggle->set_tooltip_text(TTR("Enable Onion Skinning"));
	onion_toggle->connect(SceneStringName(pressed), callable_mp(this, &AnimationPlayerEditor::_onion_skinning_menu).bind(ONION_SKINNING_ENABLE));
	hb->add_child(onion_toggle);

	onion_skinning = memnew(MenuButton);
	onion_skinning->set_flat(false);
	onion_skinning->set_theme_type_variation("FlatMenuButton");
	onion_skinning->set_tooltip_text(TTR("Onion Skinning Options"));
	onion_skinning->get_popup()->add_separator(TTR("Directions"));
	// TRANSLATORS: Opposite of "Future", refers to a direction in animation onion skinning.
	onion_skinning->get_popup()->add_check_item(TTR("Past"), ONION_SKINNING_PAST);
	onion_skinning->get_popup()->set_item_checked(-1, true);
	// TRANSLATORS: Opposite of "Past", refers to a direction in animation onion skinning.
	onion_skinning->get_popup()->add_check_item(TTR("Future"), ONION_SKINNING_FUTURE);
	onion_skinning->get_popup()->add_separator(TTR("Depth"));
	onion_skinning->get_popup()->add_radio_check_item(TTR("1 step"), ONION_SKINNING_1_STEP);
	onion_skinning->get_popup()->set_item_checked(-1, true);
	onion_skinning->get_popup()->add_radio_check_item(TTR("2 steps"), ONION_SKINNING_2_STEPS);
	onion_skinning->get_popup()->add_radio_check_item(TTR("3 steps"), ONION_SKINNING_3_STEPS);
	onion_skinning->get_popup()->add_separator();
	onion_skinning->get_popup()->add_check_item(TTR("Differences Only"), ONION_SKINNING_DIFFERENCES_ONLY);
	onion_skinning->get_popup()->add_check_item(TTR("Force White Modulate"), ONION_SKINNING_FORCE_WHITE_MODULATE);
	onion_skinning->get_popup()->add_check_item(TTR("Include Gizmos (3D)"), ONION_SKINNING_INCLUDE_GIZMOS);
	hb->add_child(onion_skinning);

	hb->add_child(memnew(VSeparator));

	pin = memnew(Button);
	pin->set_theme_type_variation(SceneStringName(FlatButton));
	pin->set_toggle_mode(true);
	pin->set_tooltip_text(TTR("Pin AnimationPlayer"));
	hb->add_child(pin);
	pin->connect(SceneStringName(pressed), callable_mp(this, &AnimationPlayerEditor::_pin_pressed));

	file = memnew(EditorFileDialog);
	add_child(file);

	name_dialog = memnew(ConfirmationDialog);
	name_dialog->set_title(TTR("Create New Animation"));
	name_dialog->set_hide_on_ok(false);
	add_child(name_dialog);
	VBoxContainer *vb = memnew(VBoxContainer);
	name_dialog->add_child(vb);

	name_title = memnew(Label(TTR("Animation Name:")));
	vb->add_child(name_title);

	HBoxContainer *name_hb = memnew(HBoxContainer);
	name = memnew(LineEdit);
	name_hb->add_child(name);
	name->set_h_size_flags(SIZE_EXPAND_FILL);
	library = memnew(OptionButton);
	name_hb->add_child(library);
	library->hide();
	vb->add_child(name_hb);
	name_dialog->register_text_enter(name);

	error_dialog = memnew(AcceptDialog);
	error_dialog->set_ok_button_text(TTR("Close"));
	error_dialog->set_title(TTR("Error!"));
	name_dialog->add_child(error_dialog);

	name_dialog->connect(SceneStringName(confirmed), callable_mp(this, &AnimationPlayerEditor::_animation_name_edited));

	blend_editor.dialog = memnew(AcceptDialog);
	blend_editor.dialog->set_title(TTR("Cross-Animation Blend Times"));
	blend_editor.dialog->set_ok_button_text(TTR("Close"));
	blend_editor.dialog->set_hide_on_ok(true);
	add_child(blend_editor.dialog);

	VBoxContainer *blend_vb = memnew(VBoxContainer);
	blend_editor.dialog->add_child(blend_vb);

	blend_editor.tree = memnew(Tree);
	blend_editor.tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	blend_editor.tree->set_hide_root(true);
	blend_editor.tree->set_columns(2);
	blend_editor.tree->set_column_expand_ratio(0, 10);
	blend_editor.tree->set_column_clip_content(0, true);
	blend_editor.tree->set_column_expand_ratio(1, 3);
	blend_editor.tree->set_column_clip_content(1, true);
	blend_vb->add_margin_child(TTR("Blend Times:"), blend_editor.tree, true);
	blend_editor.tree->connect(SNAME("item_edited"), callable_mp(this, &AnimationPlayerEditor::_blend_edited));

	blend_editor.next = memnew(OptionButton);
	blend_editor.next->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	blend_vb->add_margin_child(TTR("Next (Auto Queue):"), blend_editor.next);

	autoplay->connect(SceneStringName(pressed), callable_mp(this, &AnimationPlayerEditor::_autoplay_pressed));
	autoplay->set_toggle_mode(true);
	play->connect(SceneStringName(pressed), callable_mp(this, &AnimationPlayerEditor::_play_pressed));
	play_from->connect(SceneStringName(pressed), callable_mp(this, &AnimationPlayerEditor::_play_from_pressed));
	play_bw->connect(SceneStringName(pressed), callable_mp(this, &AnimationPlayerEditor::_play_bw_pressed));
	play_bw_from->connect(SceneStringName(pressed), callable_mp(this, &AnimationPlayerEditor::_play_bw_from_pressed));
	stop->connect(SceneStringName(pressed), callable_mp(this, &AnimationPlayerEditor::_stop_pressed));

	animation->connect(SceneStringName(item_selected), callable_mp(this, &AnimationPlayerEditor::_animation_selected));

	frame->connect(SceneStringName(value_changed), callable_mp(this, &AnimationPlayerEditor::_seek_value_changed).bind(false));
	scale->connect(SceneStringName(text_submitted), callable_mp(this, &AnimationPlayerEditor::_scale_changed));

	add_child(track_editor);
	track_editor->set_v_size_flags(SIZE_EXPAND_FILL);
	track_editor->connect(SNAME("timeline_changed"), callable_mp(this, &AnimationPlayerEditor::_animation_key_editor_seek));
	track_editor->connect(SNAME("animation_len_changed"), callable_mp(this, &AnimationPlayerEditor::_animation_key_editor_anim_len_changed));

	_update_player();

	library_editor = memnew(AnimationLibraryEditor);
	add_child(library_editor);
	library_editor->connect(SNAME("update_editor"), callable_mp(this, &AnimationPlayerEditor::_animation_player_changed));

	// Onion skinning.

	track_editor->connect(SceneStringName(visibility_changed), callable_mp(this, &AnimationPlayerEditor::_editor_visibility_changed));

	onion.capture.canvas = RS::get_singleton()->canvas_create();
	onion.capture.canvas_item = RS::get_singleton()->canvas_item_create();
	RS::get_singleton()->canvas_item_set_parent(onion.capture.canvas_item, onion.capture.canvas);

	onion.capture.material.instantiate();

	onion.capture.shader.instantiate();
	onion.capture.shader->set_code(R"(
// Animation editor onion skinning shader.

shader_type canvas_item;

uniform vec4 bkg_color;
uniform vec4 dir_color;
uniform bool differences_only;
uniform sampler2D present;

float zero_if_equal(vec4 a, vec4 b) {
	return smoothstep(0.0, 0.005, length(a.rgb - b.rgb) / sqrt(3.0));
}

void fragment() {
	vec4 capture_samp = texture(TEXTURE, UV);
	float bkg_mask = zero_if_equal(capture_samp, bkg_color);
	float diff_mask = 1.0;
	if (differences_only) {
		// FIXME: If Y-flips across render target, canvas item, etc. was handled correctly,
		//        this would not be as convoluted in the shader.
		vec4 capture_samp2 = texture(TEXTURE, vec2(UV.x, 1.0 - UV.y));
		vec4 present_samp = texture(present, vec2(UV.x, 1.0 - UV.y));
		diff_mask = 1.0 - zero_if_equal(present_samp, bkg_color);
	}
	COLOR = vec4(capture_samp.rgb * dir_color.rgb, bkg_mask * diff_mask);
}
)");
	RS::get_singleton()->material_set_shader(onion.capture.material->get_rid(), onion.capture.shader->get_rid());

	ED_SHORTCUT("animation_editor/stop_animation", TTRC("Pause/Stop Animation"), Key::S);
	ED_SHORTCUT("animation_editor/play_animation", TTRC("Play Animation"), Key::D);
	ED_SHORTCUT("animation_editor/play_animation_backwards", TTRC("Play Animation Backwards"), Key::A);
	ED_SHORTCUT("animation_editor/play_animation_from_start", TTRC("Play Animation from Start"), KeyModifierMask::SHIFT + Key::D);
	ED_SHORTCUT("animation_editor/play_animation_from_end", TTRC("Play Animation Backwards from End"), KeyModifierMask::SHIFT + Key::A);
	ED_SHORTCUT("animation_editor/go_to_next_keyframe", TTRC("Go to Next Keyframe"), KeyModifierMask::SHIFT + KeyModifierMask::ALT + Key::D);
	ED_SHORTCUT("animation_editor/go_to_previous_keyframe", TTRC("Go to Previous Keyframe"), KeyModifierMask::SHIFT + KeyModifierMask::ALT + Key::A);
}

AnimationPlayerEditor::~AnimationPlayerEditor() {
	_free_onion_layers();
	RS::get_singleton()->free(onion.capture.canvas);
	RS::get_singleton()->free(onion.capture.canvas_item);
	onion.capture = {};
}

void AnimationPlayerEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			Node3DEditor::get_singleton()->connect(SNAME("transform_key_request"), callable_mp(this, &AnimationPlayerEditorPlugin::_transform_key_request));
			InspectorDock::get_inspector_singleton()->connect(SNAME("property_keyed"), callable_mp(this, &AnimationPlayerEditorPlugin::_property_keyed));
			anim_editor->get_track_editor()->connect(SNAME("keying_changed"), callable_mp(this, &AnimationPlayerEditorPlugin::_update_keying));
			InspectorDock::get_inspector_singleton()->connect(SNAME("edited_object_changed"), callable_mp(anim_editor->get_track_editor(), &AnimationTrackEditor::update_keying));
			set_force_draw_over_forwarding_enabled();
		} break;
	}
}

void AnimationPlayerEditorPlugin::_property_keyed(const String &p_keyed, const Variant &p_value, bool p_advance) {
	AnimationTrackEditor *te = anim_editor->get_track_editor();
	if (!te || !te->has_keying()) {
		return;
	}
	te->_clear_selection();
	te->insert_value_key(p_keyed, p_advance);
}

void AnimationPlayerEditorPlugin::_transform_key_request(Object *sp, const String &p_sub, const Transform3D &p_key) {
	if (!anim_editor->get_track_editor()->has_keying()) {
		return;
	}
	Node3D *s = Object::cast_to<Node3D>(sp);
	if (!s) {
		return;
	}
	anim_editor->get_track_editor()->insert_transform_key(s, p_sub, Animation::TYPE_POSITION_3D, p_key.origin);
	anim_editor->get_track_editor()->insert_transform_key(s, p_sub, Animation::TYPE_ROTATION_3D, p_key.basis.get_rotation_quaternion());
	anim_editor->get_track_editor()->insert_transform_key(s, p_sub, Animation::TYPE_SCALE_3D, p_key.basis.get_scale());
}

void AnimationPlayerEditorPlugin::_update_keying() {
	InspectorDock::get_inspector_singleton()->set_keying(anim_editor->get_track_editor()->has_keying());
}

void AnimationPlayerEditorPlugin::edit(Object *p_object) {
	if (player && anim_editor && anim_editor->is_pinned()) {
		return; // Ignore, pinned.
	}

	player = nullptr;
	if (!p_object) {
		return;
	}
	last_mixer = p_object->get_instance_id();

	AnimationMixer *src_node = Object::cast_to<AnimationMixer>(p_object);
	bool is_dummy = false;
	if (!p_object->is_class("AnimationPlayer")) {
		// If it needs dummy AnimationPlayer, assign original AnimationMixer to LibraryEditor.
		_update_dummy_player(src_node);

		is_dummy = true;

		if (!src_node->is_connected(SNAME("mixer_updated"), callable_mp(this, &AnimationPlayerEditorPlugin::_update_dummy_player))) {
			src_node->connect(SNAME("mixer_updated"), callable_mp(this, &AnimationPlayerEditorPlugin::_update_dummy_player).bind(src_node), CONNECT_DEFERRED);
		}
		if (!src_node->is_connected(SNAME("animation_libraries_updated"), callable_mp(this, &AnimationPlayerEditorPlugin::_update_dummy_player))) {
			src_node->connect(SNAME("animation_libraries_updated"), callable_mp(this, &AnimationPlayerEditorPlugin::_update_dummy_player).bind(src_node), CONNECT_DEFERRED);
		}
	} else {
		_clear_dummy_player();
		player = Object::cast_to<AnimationPlayer>(p_object);
	}
	player->set_dummy(is_dummy);

	anim_editor->edit(src_node, player, is_dummy);
}

void AnimationPlayerEditorPlugin::_clear_dummy_player() {
	if (!dummy_player) {
		return;
	}
	Node *parent = dummy_player->get_parent();
	if (parent) {
		callable_mp(parent, &Node::remove_child).call_deferred(dummy_player);
	}
	dummy_player->queue_free();
	dummy_player = nullptr;
}

void AnimationPlayerEditorPlugin::_update_dummy_player(AnimationMixer *p_mixer) {
	// Check current editing object.
	if (p_mixer->get_instance_id() != last_mixer && p_mixer->is_connected(SNAME("mixer_updated"), callable_mp(this, &AnimationPlayerEditorPlugin::_update_dummy_player))) {
		p_mixer->disconnect(SNAME("mixer_updated"), callable_mp(this, &AnimationPlayerEditorPlugin::_update_dummy_player));
		return;
	}

	// Add dummy player to scene.
	if (!dummy_player) {
		Node *parent = p_mixer->get_parent();
		ERR_FAIL_NULL(parent);
		dummy_player = memnew(AnimationPlayer);
		dummy_player->set_active(false); // Inactive as default, it will be activated if the AnimationPlayerEditor visibility is changed.
		parent->add_child(dummy_player);
	}
	player = dummy_player;

	// Convert AnimationTree (AnimationMixer) to AnimationPlayer.
	AnimationMixer *default_node = memnew(AnimationMixer);
	List<PropertyInfo> pinfo;
	default_node->get_property_list(&pinfo);
	for (const PropertyInfo &E : pinfo) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}
		if (E.name != "script" && E.name != "active" && E.name != "deterministic" && E.name != "root_motion_track") {
			dummy_player->set(E.name, p_mixer->get(E.name));
		}
	}
	memdelete(default_node);

	if (anim_editor) {
		anim_editor->_update_player();
	}
}

bool AnimationPlayerEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("AnimationPlayer") || p_object->is_class("AnimationTree") || p_object->is_class("AnimationMixer");
}

void AnimationPlayerEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		EditorNode::get_bottom_panel()->make_item_visible(anim_editor);
		anim_editor->set_process(true);
		anim_editor->ensure_visibility();
	}
}

AnimationPlayerEditorPlugin::AnimationPlayerEditorPlugin() {
	anim_editor = memnew(AnimationPlayerEditor(this));
	EditorNode::get_bottom_panel()->add_item(TTR("Animation"), anim_editor, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_animation_bottom_panel", TTRC("Toggle Animation Bottom Panel"), KeyModifierMask::ALT | Key::N));
}

AnimationPlayerEditorPlugin::~AnimationPlayerEditorPlugin() {
	if (dummy_player) {
		memdelete(dummy_player);
	}
}

// AnimationTrackKeyEditEditorPlugin

bool EditorInspectorPluginAnimationTrackKeyEdit::can_handle(Object *p_object) {
	return Object::cast_to<AnimationTrackKeyEdit>(p_object) != nullptr;
}

void EditorInspectorPluginAnimationTrackKeyEdit::parse_begin(Object *p_object) {
	AnimationTrackKeyEdit *atk = Object::cast_to<AnimationTrackKeyEdit>(p_object);
	ERR_FAIL_NULL(atk);

	atk_editor = memnew(AnimationTrackKeyEditEditor(atk->animation, atk->track, atk->key_ofs, atk->use_fps));
	add_custom_control(atk_editor);
}

AnimationTrackKeyEditEditorPlugin::AnimationTrackKeyEditEditorPlugin() {
	atk_plugin = memnew(EditorInspectorPluginAnimationTrackKeyEdit);
	EditorInspector::add_inspector_plugin(atk_plugin);
}

bool AnimationTrackKeyEditEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("AnimationTrackKeyEdit");
}

bool EditorInspectorPluginAnimationMarkerKeyEdit::can_handle(Object *p_object) {
	return Object::cast_to<AnimationMarkerKeyEdit>(p_object) != nullptr;
}

void EditorInspectorPluginAnimationMarkerKeyEdit::parse_begin(Object *p_object) {
	AnimationMarkerKeyEdit *amk = Object::cast_to<AnimationMarkerKeyEdit>(p_object);
	ERR_FAIL_NULL(amk);

	amk_editor = memnew(AnimationMarkerKeyEditEditor(amk->animation, amk->marker_name, amk->use_fps));
	add_custom_control(amk_editor);
}

AnimationMarkerKeyEditEditorPlugin::AnimationMarkerKeyEditEditorPlugin() {
	amk_plugin = memnew(EditorInspectorPluginAnimationMarkerKeyEdit);
	EditorInspector::add_inspector_plugin(amk_plugin);
}

bool AnimationMarkerKeyEditEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("AnimationMarkerKeyEdit");
}
