/**************************************************************************/
/*  audio_stream_interactive_editor_plugin.cpp                            */
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

#include "audio_stream_interactive_editor_plugin.h"

#include "../audio_stream_interactive.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_box.h"
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tree.h"

void AudioStreamInteractiveTransitionEditor::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY || p_what == NOTIFICATION_THEME_CHANGED) {
		fade_mode->clear();
		fade_mode->add_icon_item(get_editor_theme_icon(SNAME("FadeDisabled")), TTR("Disabled"), AudioStreamInteractive::FADE_DISABLED);
		fade_mode->add_icon_item(get_editor_theme_icon(SNAME("FadeIn")), TTR("Fade-In"), AudioStreamInteractive::FADE_IN);
		fade_mode->add_icon_item(get_editor_theme_icon(SNAME("FadeOut")), TTR("Fade-Out"), AudioStreamInteractive::FADE_OUT);
		fade_mode->add_icon_item(get_editor_theme_icon(SNAME("FadeCross")), TTR("Cross-Fade"), AudioStreamInteractive::FADE_CROSS);
		fade_mode->add_icon_item(get_editor_theme_icon(SNAME("AutoPlay")), TTR("Automatic"), AudioStreamInteractive::FADE_AUTOMATIC);

		fade_length_unit->clear();
		fade_length_unit->add_item(TTR("Beats"), AudioStreamInteractive::UNIT_BEATS);
		fade_length_unit->add_item(TTR("Bars"), AudioStreamInteractive::UNIT_BARS);
		fade_length_unit->add_item(TTR("Seconds"), AudioStreamInteractive::UNIT_SECONDS);
	}
}

void AudioStreamInteractiveTransitionEditor::_bind_methods() {
	ClassDB::bind_method("_update_transitions", &AudioStreamInteractiveTransitionEditor::_update_transitions);
}

void AudioStreamInteractiveTransitionEditor::_edited() {
	if (updating) {
		return;
	}

	bool enabled = transition_enabled->is_pressed();
	AudioStreamInteractive::TransitionFromTime from = AudioStreamInteractive::TransitionFromTime(transition_from->get_selected());
	AudioStreamInteractive::TransitionToTime to = AudioStreamInteractive::TransitionToTime(transition_to->get_selected());
	AudioStreamInteractive::FadeMode fade = AudioStreamInteractive::FadeMode(fade_mode->get_selected());
	AudioStreamInteractive::FadeLengthUnit fade_unit = AudioStreamInteractive::FadeLengthUnit(fade_length_unit->get_selected());
	float length = fade_length->get_value();
	bool use_filler = filler_clip->get_selected() > 0;
	int filler = use_filler ? filler_clip->get_selected() - 1 : 0;
	bool hold = hold_previous->is_pressed();

	EditorUndoRedoManager::get_singleton()->create_action(TTR("Edit Transitions"));
	for (int i = 0; i < selected.size(); i++) {
		if (!enabled) {
			if (audio_stream_interactive->has_transition(selected[i].x, selected[i].y)) {
				EditorUndoRedoManager::get_singleton()->add_do_method(audio_stream_interactive, "erase_transition", selected[i].x, selected[i].y);
			}
		} else {
			EditorUndoRedoManager::get_singleton()->add_do_method(audio_stream_interactive, "add_transition_with_unit", selected[i].x, selected[i].y, from, to, fade, length, fade_unit, use_filler, filler, hold);
		}
	}
	EditorUndoRedoManager::get_singleton()->add_undo_property(audio_stream_interactive, "_transitions", audio_stream_interactive->get("_transitions"));
	EditorUndoRedoManager::get_singleton()->add_do_method(this, "_update_transitions");
	EditorUndoRedoManager::get_singleton()->add_undo_method(this, "_update_transitions");
	EditorUndoRedoManager::get_singleton()->commit_action();

	_update_options_enabled_state();
}

void AudioStreamInteractiveTransitionEditor::_update_options_enabled_state() const {
	transition_enabled->set_disabled(selected.is_empty());

	bool options_disabled = !transition_enabled->is_pressed() || selected.is_empty();
	transition_from->set_disabled(options_disabled);
	transition_to->set_disabled(options_disabled);
	fade_mode->set_disabled(options_disabled);
	fade_length->set_editable(!options_disabled);
	fade_length_unit->set_disabled(options_disabled);
	filler_clip->set_disabled(options_disabled);
	hold_previous->set_disabled(options_disabled);
}

void AudioStreamInteractiveTransitionEditor::_update_selection() {
	updating_selection = false;
	int clip_count = audio_stream_interactive->get_clip_count();
	selected.clear();
	Vector2i editing;
	int editing_order = -1;
	for (int i = 0; i <= clip_count; i++) {
		for (int j = 0; j <= clip_count; j++) {
			if (rows[i]->is_selected(j)) {
				Vector2i meta = rows[i]->get_metadata(j);
				if (selection_order.has(meta)) {
					int order = selection_order[meta];
					if (order > editing_order) {
						editing = meta;
					}
				}
				selected.push_back(meta);
			}
		}
	}

	if (selected.is_empty()) {
		options_vbox->set_visible(false);
		return;
	}

	options_vbox->set_visible(true);

	updating = true;
	if (!audio_stream_interactive->has_transition(editing.x, editing.y)) {
		transition_enabled->set_pressed(false);
		transition_from->select(0);
		transition_to->select(0);
		fade_mode->select(AudioStreamInteractive::FADE_AUTOMATIC);
		fade_length->set_value(1.0);
		fade_length_unit->select(AudioStreamInteractive::UNIT_BEATS);
		filler_clip->select(0);
		hold_previous->set_pressed(false);
	} else {
		transition_enabled->set_pressed(true);
		transition_from->select(audio_stream_interactive->get_transition_from_time(editing.x, editing.y));
		transition_to->select(audio_stream_interactive->get_transition_to_time(editing.x, editing.y));
		fade_mode->select(audio_stream_interactive->get_transition_fade_mode(editing.x, editing.y));
		fade_length_unit->select(audio_stream_interactive->get_transition_fade_length_unit(editing.x, editing.y));
		fade_length->set_value(audio_stream_interactive->get_transition_fade_length(editing.x, editing.y));
		if (audio_stream_interactive->is_transition_using_filler_clip(editing.x, editing.y)) {
			filler_clip->select(audio_stream_interactive->get_transition_filler_clip(editing.x, editing.y) + 1);
		} else {
			filler_clip->select(0);
		}
		hold_previous->set_pressed(audio_stream_interactive->is_transition_holding_previous(editing.x, editing.y));
	}

	_update_options_enabled_state();
	updating = false;
}

void AudioStreamInteractiveTransitionEditor::_cell_selected(TreeItem *p_item, int p_column, bool p_selected) {
	int to = p_item->get_meta("to");
	int from = p_column == audio_stream_interactive->get_clip_count() ? AudioStreamInteractive::CLIP_ANY : p_column;
	if (p_selected) {
		selection_order[Vector2i(from, to)] = order_counter++;
	}

	if (!updating_selection) {
		MessageQueue::get_singleton()->push_callable(callable_mp(this, &AudioStreamInteractiveTransitionEditor::_update_selection));
		updating_selection = true;
	}
}

void AudioStreamInteractiveTransitionEditor::_update_transitions() {
	if (!is_visible()) {
		return;
	}
	int clip_count = audio_stream_interactive->get_clip_count();
	Color font_color = tree->get_theme_color(SceneStringName(font_color), "Tree");
	Color font_color_default = font_color;
	font_color_default.a *= 0.5;
	Ref<Texture> fade_icons[5] = {
		get_editor_theme_icon(SNAME("FadeDisabled")),
		get_editor_theme_icon(SNAME("FadeIn")),
		get_editor_theme_icon(SNAME("FadeOut")),
		get_editor_theme_icon(SNAME("FadeCross")),
		get_editor_theme_icon(SNAME("AutoPlay"))
	};
	for (int i = 0; i <= clip_count; i++) {
		for (int j = 0; j <= clip_count; j++) {
			int from = i == clip_count ? AudioStreamInteractive::CLIP_ANY : i;
			int to = j == clip_count ? AudioStreamInteractive::CLIP_ANY : j;

			bool exists = audio_stream_interactive->has_transition(from, to);
			String tooltip;
			Ref<Texture> icon;
			if (!exists) {
				if (audio_stream_interactive->has_transition(AudioStreamInteractive::CLIP_ANY, to)) {
					from = AudioStreamInteractive::CLIP_ANY;
					tooltip = vformat(TTR("Using Any Clip -> %s."), audio_stream_interactive->get_clip_name(to));
				} else if (audio_stream_interactive->has_transition(from, AudioStreamInteractive::CLIP_ANY)) {
					to = AudioStreamInteractive::CLIP_ANY;
					tooltip = vformat(TTR("Using %s -> Any Clip."), audio_stream_interactive->get_clip_name(from));
				} else if (audio_stream_interactive->has_transition(AudioStreamInteractive::CLIP_ANY, AudioStreamInteractive::CLIP_ANY)) {
					from = to = AudioStreamInteractive::CLIP_ANY;
					tooltip = TTR("Using All Clips -> Any Clip.");
				} else {
					tooltip = TTR("No transition available.");
				}
			}

			String from_time;
			String to_time;
			if (audio_stream_interactive->has_transition(from, to)) {
				icon = fade_icons[audio_stream_interactive->get_transition_fade_mode(from, to)];
				switch (audio_stream_interactive->get_transition_from_time(from, to)) {
					case AudioStreamInteractive::TRANSITION_FROM_TIME_IMMEDIATE: {
						from_time = TTR("Immediate");
					} break;
					case AudioStreamInteractive::TRANSITION_FROM_TIME_NEXT_BEAT: {
						from_time = TTR("Next Beat");
					} break;
					case AudioStreamInteractive::TRANSITION_FROM_TIME_NEXT_BAR: {
						from_time = TTR("Next Bar");
					} break;
					case AudioStreamInteractive::TRANSITION_FROM_TIME_END: {
						from_time = TTR("Clip End");
					} break;
					default: {
					}
				}

				switch (audio_stream_interactive->get_transition_to_time(from, to)) {
					case AudioStreamInteractive::TRANSITION_TO_TIME_SAME_POSITION: {
						to_time = TTR("Same", "Transition Time Position");
					} break;
					case AudioStreamInteractive::TRANSITION_TO_TIME_START: {
						to_time = TTR("Start", "Transition Time Position");
					} break;
					case AudioStreamInteractive::TRANSITION_TO_TIME_PREVIOUS_POSITION: {
						to_time = TTR("Prev", "Transition Time Position");
					} break;
					default: {
					}
				}
			}

			rows[j]->set_icon(i, icon);
			rows[j]->set_text(i, to_time.is_empty() ? from_time : vformat(U"%s ⮕ %s", from_time, to_time));
			rows[j]->set_tooltip_text(i, tooltip);
			if (exists) {
				rows[j]->set_custom_color(i, font_color);
				rows[j]->set_icon_modulate(i, Color(1, 1, 1, 1));
			} else {
				rows[j]->set_custom_color(i, font_color_default);
				rows[j]->set_icon_modulate(i, Color(1, 1, 1, 0.5));
			}
		}
	}
}

void AudioStreamInteractiveTransitionEditor::edit(Object *p_obj) {
	audio_stream_interactive = Object::cast_to<AudioStreamInteractive>(p_obj);
	if (!audio_stream_interactive) {
		return;
	}

	Ref<Font> header_font = get_theme_font("bold", EditorStringName(EditorFonts));
	int header_font_size = get_theme_font_size("bold_size", EditorStringName(EditorFonts));

	tree->clear();
	rows.clear();
	selection_order.clear();
	selected.clear();

	int clip_count = audio_stream_interactive->get_clip_count();
	tree->set_columns(clip_count + 2);
	TreeItem *root = tree->create_item();
	TreeItem *header = tree->create_item(root); // Header
	int header_index = clip_count + 1;
	header->set_text(header_index, TTR("From / To"));
	header->set_selectable(header_index, false);

	filler_clip->clear();
	filler_clip->add_item(TTR("Disabled"), -1);

	Color header_color = get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor));

	int max_w = 0;

	updating = true;
	for (int i = 0; i <= clip_count; i++) {
		int cell_index = i;
		int clip_i = i == clip_count ? AudioStreamInteractive::CLIP_ANY : i;
		header->set_selectable(cell_index, false);
		header->set_custom_font(cell_index, header_font);
		header->set_custom_font_size(cell_index, header_font_size);
		header->set_custom_bg_color(cell_index, header_color);

		String name;
		if (i == clip_count) {
			name = TTR("Any Clip");
		} else {
			name = audio_stream_interactive->get_clip_name(i);
		}

		int min_w = header_font->get_string_size(name + "XX").width;
		tree->set_column_custom_minimum_width(cell_index, min_w);
		max_w = MAX(max_w, min_w);

		header->set_text(cell_index, name);

		TreeItem *row = tree->create_item(root);
		row->set_text(header_index, name);
		row->set_selectable(header_index, false);
		row->set_custom_font(header_index, header_font);
		row->set_custom_font_size(header_index, header_font_size);
		row->set_custom_bg_color(header_index, header_color);
		row->set_meta("to", clip_i);
		for (int j = 0; j <= clip_count; j++) {
			int clip_j = j == clip_count ? AudioStreamInteractive::CLIP_ANY : j;
			row->set_metadata(j, Vector2i(clip_j, clip_i));
		}
		rows.push_back(row);

		if (i < clip_count) {
			filler_clip->add_item(name, i);
		}
	}

	tree->set_column_custom_minimum_width(header_index, max_w);
	selection_order.clear();
	_update_selection();
	popup_centered_clamped(Size2(900, 550) * EDSCALE);
	updating = false;
	_update_transitions();
}

AudioStreamInteractiveTransitionEditor::AudioStreamInteractiveTransitionEditor() {
	set_title(TTR("AudioStreamInteractive Transition Editor"));
	split = memnew(HSplitContainer);
	add_child(split);
	tree = memnew(Tree);
	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_hide_root(true);
	tree->add_theme_constant_override("draw_guides", 1);
	tree->set_select_mode(Tree::SELECT_MULTI);
	tree->set_custom_minimum_size(Size2(400, 0) * EDSCALE);
	split->add_child(tree);

	tree->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tree->connect("multi_selected", callable_mp(this, &AudioStreamInteractiveTransitionEditor::_cell_selected));
	options_vbox = memnew(VBoxContainer);
	split->add_child(options_vbox);

	transition_enabled = memnew(CheckBox);
	transition_enabled->set_text(TTR("Enabled"));
	options_vbox->add_margin_child(TTR("Use Transition:"), transition_enabled);
	transition_enabled->connect(SceneStringName(pressed), callable_mp(this, &AudioStreamInteractiveTransitionEditor::_edited));

	transition_from = memnew(OptionButton);
	options_vbox->add_margin_child(TTR("Transition From:"), transition_from);
	transition_from->add_item(TTR("Immediate"), AudioStreamInteractive::TRANSITION_FROM_TIME_IMMEDIATE);
	transition_from->add_item(TTR("Next Beat"), AudioStreamInteractive::TRANSITION_FROM_TIME_NEXT_BEAT);
	transition_from->add_item(TTR("Next Bar"), AudioStreamInteractive::TRANSITION_FROM_TIME_NEXT_BAR);
	transition_from->add_item(TTR("Clip End"), AudioStreamInteractive::TRANSITION_FROM_TIME_END);

	transition_from->connect(SceneStringName(item_selected), callable_mp(this, &AudioStreamInteractiveTransitionEditor::_edited).unbind(1));

	transition_to = memnew(OptionButton);
	options_vbox->add_margin_child(TTR("Transition To:"), transition_to);
	transition_to->add_item(TTR("Same Position"), AudioStreamInteractive::TRANSITION_TO_TIME_SAME_POSITION);
	transition_to->add_item(TTR("Clip Start"), AudioStreamInteractive::TRANSITION_TO_TIME_START);
	transition_to->add_item(TTR("Prev Position"), AudioStreamInteractive::TRANSITION_TO_TIME_PREVIOUS_POSITION);
	transition_to->connect(SceneStringName(item_selected), callable_mp(this, &AudioStreamInteractiveTransitionEditor::_edited).unbind(1));

	fade_mode = memnew(OptionButton);
	options_vbox->add_margin_child(TTR("Fade Mode:"), fade_mode);
	fade_mode->connect(SceneStringName(item_selected), callable_mp(this, &AudioStreamInteractiveTransitionEditor::_edited).unbind(1));

	HBoxContainer *fade_length_hb = memnew(HBoxContainer);
	fade_length = memnew(SpinBox);
	fade_length->set_max(16);
	fade_length->set_step(0.1);
	fade_length->connect(SceneStringName(value_changed), callable_mp(this, &AudioStreamInteractiveTransitionEditor::_edited).unbind(1));
	fade_length_unit = memnew(OptionButton);
	fade_length_unit->connect(SceneStringName(item_selected), callable_mp(this, &AudioStreamInteractiveTransitionEditor::_edited).unbind(1));
	fade_length_hb->add_child(fade_length);
	fade_length_hb->add_child(fade_length_unit);
	options_vbox->add_margin_child(TTR("Fade Length:"), fade_length_hb);

	filler_clip = memnew(OptionButton);
	options_vbox->add_margin_child(TTR("Filler Clip:"), filler_clip);
	filler_clip->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	filler_clip->connect(SceneStringName(item_selected), callable_mp(this, &AudioStreamInteractiveTransitionEditor::_edited).unbind(1));

	hold_previous = memnew(CheckBox);
	hold_previous->set_text(TTR("Enabled"));
	hold_previous->connect(SceneStringName(pressed), callable_mp(this, &AudioStreamInteractiveTransitionEditor::_edited));
	options_vbox->add_margin_child(TTR("Hold Previous:"), hold_previous);

	options_vbox->set_visible(false);

	set_exclusive(true);
}

////////////////////////

bool EditorInspectorPluginAudioStreamInteractive::can_handle(Object *p_object) {
	return Object::cast_to<AudioStreamInteractive>(p_object);
}

void EditorInspectorPluginAudioStreamInteractive::_edit(Object *p_object) {
	audio_stream_interactive_transition_editor->edit(p_object);
}

void EditorInspectorPluginAudioStreamInteractive::parse_end(Object *p_object) {
	if (Object::cast_to<AudioStreamInteractive>(p_object)) {
		Button *button = EditorInspector::create_inspector_action_button(TTR("Edit Transitions"));
		button->set_button_icon(audio_stream_interactive_transition_editor->get_editor_theme_icon(SNAME("Blend")));
		button->connect(SceneStringName(pressed), callable_mp(this, &EditorInspectorPluginAudioStreamInteractive::_edit).bind(p_object));
		add_custom_control(button);
	}
}

EditorInspectorPluginAudioStreamInteractive::EditorInspectorPluginAudioStreamInteractive() {
	audio_stream_interactive_transition_editor = memnew(AudioStreamInteractiveTransitionEditor);
	EditorNode::get_singleton()->get_gui_base()->add_child(audio_stream_interactive_transition_editor);
}

AudioStreamInteractiveEditorPlugin::AudioStreamInteractiveEditorPlugin() {
	Ref<EditorInspectorPluginAudioStreamInteractive> inspector_plugin;
	inspector_plugin.instantiate();
	add_inspector_plugin(inspector_plugin);
}
