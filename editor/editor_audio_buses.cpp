/**************************************************************************/
/*  editor_audio_buses.cpp                                                */
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

#include "editor_audio_buses.h"

#include "core/io/resource_saver.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "filesystem_dock.h"
#include "scene/resources/font.h"
#include "servers/audio_server.h"

void EditorAudioBus::_update_visible_channels() {
	int i = 0;
	for (; i < cc; i++) {
		if (!channel[i].vu_l->is_visible()) {
			channel[i].vu_l->show();
		}
		if (!channel[i].vu_r->is_visible()) {
			channel[i].vu_r->show();
		}
	}

	for (; i < CHANNELS_MAX; i++) {
		if (channel[i].vu_l->is_visible()) {
			channel[i].vu_l->hide();
		}
		if (channel[i].vu_r->is_visible()) {
			channel[i].vu_r->hide();
		}
	}
}

void EditorAudioBus::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			for (int i = 0; i < CHANNELS_MAX; i++) {
				channel[i].vu_l->set_under_texture(get_icon("BusVuEmpty", "EditorIcons"));
				channel[i].vu_l->set_progress_texture(get_icon("BusVuFull", "EditorIcons"));
				channel[i].vu_r->set_under_texture(get_icon("BusVuEmpty", "EditorIcons"));
				channel[i].vu_r->set_progress_texture(get_icon("BusVuFull", "EditorIcons"));
				channel[i].prev_active = true;
			}

			disabled_vu = get_icon("BusVuFrozen", "EditorIcons");

			Color solo_color = EditorSettings::get_singleton()->is_dark_theme() ? Color(1.0, 0.89, 0.22) : Color(1.0, 0.92, 0.44);
			Color mute_color = EditorSettings::get_singleton()->is_dark_theme() ? Color(1.0, 0.16, 0.16) : Color(1.0, 0.44, 0.44);
			Color bypass_color = EditorSettings::get_singleton()->is_dark_theme() ? Color(0.13, 0.8, 1.0) : Color(0.44, 0.87, 1.0);

			solo->set_icon(get_icon("AudioBusSolo", "EditorIcons"));
			solo->add_color_override("icon_color_pressed", solo_color);
			mute->set_icon(get_icon("AudioBusMute", "EditorIcons"));
			mute->add_color_override("icon_color_pressed", mute_color);
			bypass->set_icon(get_icon("AudioBusBypass", "EditorIcons"));
			bypass->add_color_override("icon_color_pressed", bypass_color);

			bus_options->set_icon(get_icon("GuiTabMenuHl", "EditorIcons"));

			audio_value_preview_label->add_color_override("font_color", get_color("font_color", "TooltipLabel"));
			audio_value_preview_label->add_color_override("font_color_shadow", get_color("font_color_shadow", "TooltipLabel"));
			audio_value_preview_box->add_style_override("panel", get_stylebox("panel", "TooltipPanel"));

			for (int i = 0; i < effect_options->get_item_count(); i++) {
				String class_name = effect_options->get_item_metadata(i);
				Ref<Texture> icon = EditorNode::get_singleton()->get_class_icon(class_name);
				effect_options->set_item_icon(i, icon);
			}
		} break;

		case NOTIFICATION_READY: {
			update_bus();
			set_process(true);
		} break;
		case NOTIFICATION_DRAW: {
			if (is_master) {
				draw_style_box(get_stylebox("disabled", "Button"), Rect2(Vector2(), get_size()));
			} else if (has_focus()) {
				draw_style_box(get_stylebox("focus", "Button"), Rect2(Vector2(), get_size()));
			} else {
				draw_style_box(get_stylebox("panel", "TabContainer"), Rect2(Vector2(), get_size()));
			}

			if (get_index() != 0 && hovering_drop) {
				Color accent = get_color("accent_color", "Editor");
				accent.a *= 0.7;
				draw_rect(Rect2(Point2(), get_size()), accent, false);
			}
		} break;
		case NOTIFICATION_PROCESS: {
			if (cc != AudioServer::get_singleton()->get_bus_channels(get_index())) {
				cc = AudioServer::get_singleton()->get_bus_channels(get_index());
				_update_visible_channels();
			}

			for (int i = 0; i < cc; i++) {
				float real_peak[2] = { -100, -100 };
				bool activity_found = false;

				if (AudioServer::get_singleton()->is_bus_channel_active(get_index(), i)) {
					activity_found = true;
					real_peak[0] = MAX(real_peak[0], AudioServer::get_singleton()->get_bus_peak_volume_left_db(get_index(), i));
					real_peak[1] = MAX(real_peak[1], AudioServer::get_singleton()->get_bus_peak_volume_right_db(get_index(), i));
				}

				if (real_peak[0] > channel[i].peak_l) {
					channel[i].peak_l = real_peak[0];
				} else {
					channel[i].peak_l -= get_process_delta_time() * 60.0;
				}

				if (real_peak[1] > channel[i].peak_r) {
					channel[i].peak_r = real_peak[1];
				} else {
					channel[i].peak_r -= get_process_delta_time() * 60.0;
				}

				channel[i].vu_l->set_value(channel[i].peak_l);
				channel[i].vu_r->set_value(channel[i].peak_r);

				if (activity_found != channel[i].prev_active) {
					if (activity_found) {
						channel[i].vu_l->set_over_texture(Ref<Texture>());
						channel[i].vu_r->set_over_texture(Ref<Texture>());
					} else {
						channel[i].vu_l->set_over_texture(disabled_vu);
						channel[i].vu_r->set_over_texture(disabled_vu);
					}

					channel[i].prev_active = activity_found;
				}
			}
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			for (int i = 0; i < CHANNELS_MAX; i++) {
				channel[i].peak_l = -100;
				channel[i].peak_r = -100;
				channel[i].prev_active = true;
			}

			set_process(is_visible_in_tree());
		} break;

		case NOTIFICATION_MOUSE_EXIT:
		case NOTIFICATION_DRAG_END: {
			if (hovering_drop) {
				hovering_drop = false;
				update();
			}
		} break;
	}
}

void EditorAudioBus::update_send() {
	send->clear();
	if (is_master) {
		send->set_disabled(true);
		send->set_text(TTR("Speakers"));
	} else {
		send->set_disabled(false);
		StringName current_send = AudioServer::get_singleton()->get_bus_send(get_index());
		int current_send_index = 0; //by default to master

		for (int i = 0; i < get_index(); i++) {
			StringName send_name = AudioServer::get_singleton()->get_bus_name(i);
			send->add_item(send_name);
			if (send_name == current_send) {
				current_send_index = i;
			}
		}

		send->select(current_send_index);
	}
}

void EditorAudioBus::update_bus() {
	if (updating_bus) {
		return;
	}

	updating_bus = true;

	int index = get_index();

	float db_value = AudioServer::get_singleton()->get_bus_volume_db(index);
	slider->set_value(_scaled_db_to_normalized_volume(db_value));
	track_name->set_text(AudioServer::get_singleton()->get_bus_name(index));
	if (is_master) {
		track_name->set_editable(false);
	}

	solo->set_pressed(AudioServer::get_singleton()->is_bus_solo(index));
	mute->set_pressed(AudioServer::get_singleton()->is_bus_mute(index));
	bypass->set_pressed(AudioServer::get_singleton()->is_bus_bypassing_effects(index));
	// effects..
	effects->clear();

	TreeItem *root = effects->create_item();
	for (int i = 0; i < AudioServer::get_singleton()->get_bus_effect_count(index); i++) {
		Ref<AudioEffect> afx = AudioServer::get_singleton()->get_bus_effect(index, i);

		TreeItem *fx = effects->create_item(root);
		fx->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		fx->set_editable(0, true);
		fx->set_checked(0, AudioServer::get_singleton()->is_bus_effect_enabled(index, i));
		fx->set_text(0, afx->get_name());
		fx->set_metadata(0, i);
	}

	TreeItem *add = effects->create_item(root);
	add->set_cell_mode(0, TreeItem::CELL_MODE_CUSTOM);
	add->set_editable(0, true);
	add->set_selectable(0, false);
	add->set_text(0, TTR("Add Effect"));

	update_send();

	updating_bus = false;
}

void EditorAudioBus::_name_changed(const String &p_new_name) {
	if (p_new_name == AudioServer::get_singleton()->get_bus_name(get_index())) {
		return;
	}

	String attempt = p_new_name;
	int attempts = 1;

	while (true) {
		bool name_free = true;
		for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
			if (AudioServer::get_singleton()->get_bus_name(i) == attempt) {
				name_free = false;
				break;
			}
		}

		if (name_free) {
			break;
		}

		attempts++;
		attempt = p_new_name + " " + itos(attempts);
	}
	updating_bus = true;

	UndoRedo *ur = EditorNode::get_undo_redo();

	StringName current = AudioServer::get_singleton()->get_bus_name(get_index());
	ur->create_action(TTR("Rename Audio Bus"));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_name", get_index(), attempt);
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_name", get_index(), current);

	for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
		if (AudioServer::get_singleton()->get_bus_send(i) == current) {
			ur->add_do_method(AudioServer::get_singleton(), "set_bus_send", i, attempt);
			ur->add_undo_method(AudioServer::get_singleton(), "set_bus_send", i, current);
		}
	}

	ur->add_do_method(buses, "_update_bus", get_index());
	ur->add_undo_method(buses, "_update_bus", get_index());

	ur->add_do_method(buses, "_update_sends");
	ur->add_undo_method(buses, "_update_sends");
	ur->commit_action();

	updating_bus = false;

	track_name->release_focus();
}

void EditorAudioBus::_volume_changed(float p_normalized) {
	if (updating_bus) {
		return;
	}

	updating_bus = true;

	const float p_db = this->_normalized_volume_to_scaled_db(p_normalized);

	if (Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
		// Snap the value when holding Ctrl for easier editing.
		// To do so, it needs to be converted back to normalized volume (as the slider uses that unit).
		slider->set_value(_scaled_db_to_normalized_volume(Math::round(p_db)));
	}

	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Change Audio Bus Volume"), UndoRedo::MERGE_ENDS);
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_volume_db", get_index(), p_db);
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_volume_db", get_index(), AudioServer::get_singleton()->get_bus_volume_db(get_index()));
	ur->add_do_method(buses, "_update_bus", get_index());
	ur->add_undo_method(buses, "_update_bus", get_index());
	ur->commit_action();

	updating_bus = false;
}

float EditorAudioBus::_normalized_volume_to_scaled_db(float normalized) {
	/* There are three different formulas for the conversion from normalized
	 * values to relative decibal values.
	 * One formula is an exponential graph which intends to counteract
	 * the logarithmic nature of human hearing. This is an approximation
	 * of the behaviour of a 'logarithmic potentiometer' found on most
	 * musical instruments and also emulated in popular software.
	 * The other two equations are hand-tuned linear tapers that intend to
	 * try to ease the exponential equation in areas where it makes sense.*/

	if (normalized > 0.6f) {
		return 22.22f * normalized - 16.2f;
	} else if (normalized < 0.05f) {
		return 830.72 * normalized - 80.0f;
	} else {
		return 45.0f * Math::pow(normalized - 1.0, 3);
	}
}

float EditorAudioBus::_scaled_db_to_normalized_volume(float db) {
	/* Inversion of equations found in _normalized_volume_to_scaled_db.
	 * IMPORTANT: If one function changes, the other much change to reflect it. */
	if (db > -2.88) {
		return (db + 16.2f) / 22.22f;
	} else if (db < -38.602f) {
		return (db + 80.00f) / 830.72f;
	} else {
		if (db < 0.0) {
			/* To accommodate for NaN on negative numbers for root, we will mirror the
			 * results of the positive db range in order to get the desired numerical
			 * value on the negative side. */
			float positive_x = Math::pow(Math::abs(db) / 45.0f, 1.0f / 3.0f) + 1.0f;
			Vector2 translation = Vector2(1.0f, 0.0f) - Vector2(positive_x, Math::abs(db));
			Vector2 reflected_position = Vector2(1.0, 0.0f) + translation;
			return reflected_position.x;
		} else {
			return Math::pow(db / 45.0f, 1.0f / 3.0f) + 1.0f;
		}
	}
}

void EditorAudioBus::_show_value(float slider_value) {
	float db;
	if (Input::get_singleton()->is_key_pressed(KEY_CONTROL)) {
		// Display the correct (snapped) value when holding Ctrl
		db = Math::round(_normalized_volume_to_scaled_db(slider_value));
	} else {
		db = _normalized_volume_to_scaled_db(slider_value);
	}

	String text;
	if (Math::is_zero_approx(Math::stepify(db, 0.1))) {
		// Prevent displaying `-0.0 dB` and show ` 0.0 dB` instead.
		// The leading space makes the text visually line up with its positive/negative counterparts.
		text = " 0.0 dB";
	} else {
		// Show an explicit `+` sign if positive.
		text = vformat("%+.1f dB", db);
	}

	// Also set the preview text as a standard Control tooltip.
	// This way, it can be seen when the slider is merely hovered (instead of dragged).
	slider->set_tooltip(text);
	audio_value_preview_label->set_text(text);
	const Vector2 slider_size = slider->get_size();
	const Vector2 slider_position = slider->get_global_position();
	const float vert_padding = 10.0f;
	const Vector2 box_position = Vector2(slider_size.x, (slider_size.y - vert_padding) * (1.0f - slider->get_value()) - vert_padding);
	audio_value_preview_box->set_position(slider_position + box_position);
	audio_value_preview_box->set_size(audio_value_preview_label->get_size());
	if (slider->has_focus() && !audio_value_preview_box->is_visible()) {
		audio_value_preview_box->show();
	}
	preview_timer->start();
}

void EditorAudioBus::_hide_value_preview() {
	audio_value_preview_box->hide();
}

void EditorAudioBus::_solo_toggled() {
	updating_bus = true;

	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Toggle Audio Bus Solo"));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_solo", get_index(), solo->is_pressed());
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_solo", get_index(), AudioServer::get_singleton()->is_bus_solo(get_index()));
	ur->add_do_method(buses, "_update_bus", get_index());
	ur->add_undo_method(buses, "_update_bus", get_index());
	ur->commit_action();

	updating_bus = false;
}
void EditorAudioBus::_mute_toggled() {
	updating_bus = true;

	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Toggle Audio Bus Mute"));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_mute", get_index(), mute->is_pressed());
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_mute", get_index(), AudioServer::get_singleton()->is_bus_mute(get_index()));
	ur->add_do_method(buses, "_update_bus", get_index());
	ur->add_undo_method(buses, "_update_bus", get_index());
	ur->commit_action();

	updating_bus = false;
}
void EditorAudioBus::_bypass_toggled() {
	updating_bus = true;

	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Toggle Audio Bus Bypass Effects"));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_bypass_effects", get_index(), bypass->is_pressed());
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_bypass_effects", get_index(), AudioServer::get_singleton()->is_bus_bypassing_effects(get_index()));
	ur->add_do_method(buses, "_update_bus", get_index());
	ur->add_undo_method(buses, "_update_bus", get_index());
	ur->commit_action();

	updating_bus = false;
}

void EditorAudioBus::_send_selected(int p_which) {
	updating_bus = true;

	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Select Audio Bus Send"));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_send", get_index(), send->get_item_text(p_which));
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_send", get_index(), AudioServer::get_singleton()->get_bus_send(get_index()));
	ur->add_do_method(buses, "_update_bus", get_index());
	ur->add_undo_method(buses, "_update_bus", get_index());
	ur->commit_action();

	updating_bus = false;
}

void EditorAudioBus::_effect_selected() {
	TreeItem *effect = effects->get_selected();
	if (!effect) {
		return;
	}
	updating_bus = true;

	if (effect->get_metadata(0) != Variant()) {
		int index = effect->get_metadata(0);
		Ref<AudioEffect> effect2 = AudioServer::get_singleton()->get_bus_effect(get_index(), index);
		if (effect2.is_valid()) {
			EditorNode::get_singleton()->push_item(effect2.ptr());
		}
	}

	updating_bus = false;
}

void EditorAudioBus::_effect_edited() {
	if (updating_bus) {
		return;
	}

	TreeItem *effect = effects->get_edited();
	if (!effect) {
		return;
	}

	if (effect->get_metadata(0) == Variant()) {
		Rect2 area = effects->get_item_rect(effect);

		effect_options->set_position(effects->get_global_position() + area.position + Vector2(0, area.size.y));
		effect_options->popup();
		//add effect
	} else {
		int index = effect->get_metadata(0);
		updating_bus = true;

		UndoRedo *ur = EditorNode::get_undo_redo();
		ur->create_action(TTR("Select Audio Bus Send"));
		ur->add_do_method(AudioServer::get_singleton(), "set_bus_effect_enabled", get_index(), index, effect->is_checked(0));
		ur->add_undo_method(AudioServer::get_singleton(), "set_bus_effect_enabled", get_index(), index, AudioServer::get_singleton()->is_bus_effect_enabled(get_index(), index));
		ur->add_do_method(buses, "_update_bus", get_index());
		ur->add_undo_method(buses, "_update_bus", get_index());
		ur->commit_action();

		updating_bus = false;
	}
}

void EditorAudioBus::_effect_add(int p_which) {
	if (updating_bus) {
		return;
	}

	StringName name = effect_options->get_item_metadata(p_which);

	Object *fx = ClassDB::instance(name);
	ERR_FAIL_COND(!fx);
	AudioEffect *afx = Object::cast_to<AudioEffect>(fx);
	ERR_FAIL_COND(!afx);
	Ref<AudioEffect> afxr = Ref<AudioEffect>(afx);

	afxr->set_name(effect_options->get_item_text(p_which));

	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Add Audio Bus Effect"));
	ur->add_do_method(AudioServer::get_singleton(), "add_bus_effect", get_index(), afxr, -1);
	ur->add_undo_method(AudioServer::get_singleton(), "remove_bus_effect", get_index(), AudioServer::get_singleton()->get_bus_effect_count(get_index()));
	ur->add_do_method(buses, "_update_bus", get_index());
	ur->add_undo_method(buses, "_update_bus", get_index());
	ur->commit_action();
}

void EditorAudioBus::_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && k->get_scancode() == KEY_DELETE && !k->is_echo()) {
		accept_event();
		emit_signal("delete_request");
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == 2 && mb->is_pressed()) {
		Vector2 pos = Vector2(mb->get_position().x, mb->get_position().y);
		bus_popup->set_position(get_global_position() + pos);
		bus_popup->popup();
	}
}

void EditorAudioBus::_unhandled_key_input(Ref<InputEvent> p_event) {
	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo() && k->get_scancode() == KEY_DELETE) {
		TreeItem *current_effect = effects->get_selected();
		if (current_effect && current_effect->get_metadata(0).get_type() == Variant::INT) {
			_delete_effect_pressed(0);
			accept_event();
		}
	}
}

void EditorAudioBus::_bus_popup_pressed(int p_option) {
	if (p_option == 2) {
		// Reset volume
		emit_signal("vol_reset_request");
	} else if (p_option == 1) {
		emit_signal("delete_request");
	} else if (p_option == 0) {
		//duplicate
		emit_signal("duplicate_request", get_index());
	}
}

Variant EditorAudioBus::get_drag_data(const Point2 &p_point) {
	if (get_index() == 0) {
		return Variant();
	}

	Control *c = memnew(Control);
	Panel *p = memnew(Panel);
	c->add_child(p);
	p->set_modulate(Color(1, 1, 1, 0.7));
	p->add_style_override("panel", get_stylebox("focus", "Button"));
	p->set_size(get_size());
	p->set_position(-p_point);
	set_drag_preview(c);
	Dictionary d;
	d["type"] = "move_audio_bus";
	d["index"] = get_index();

	if (get_index() < AudioServer::get_singleton()->get_bus_count() - 1) {
		emit_signal("drop_end_request");
	}

	return d;
}

bool EditorAudioBus::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	if (get_index() == 0) {
		return false;
	}

	Dictionary d = p_data;
	if (d.has("type") && String(d["type"]) == "move_audio_bus" && (int)d["index"] != get_index()) {
		hovering_drop = true;
		return true;
	}

	return false;
}

void EditorAudioBus::drop_data(const Point2 &p_point, const Variant &p_data) {
	Dictionary d = p_data;
	emit_signal("dropped", d["index"], get_index());
}

Variant EditorAudioBus::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	TreeItem *item = effects->get_item_at_position(p_point);
	if (!item) {
		return Variant();
	}

	Variant md = item->get_metadata(0);
	if (md.get_type() == Variant::INT) {
		Dictionary fxd;
		fxd["type"] = "audio_bus_effect";
		fxd["bus"] = get_index();
		fxd["effect"] = md;

		Label *l = memnew(Label);
		l->set_text(item->get_text(0));
		effects->set_drag_preview(l);

		return fxd;
	}

	return Variant();
}

bool EditorAudioBus::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	Dictionary d = p_data;
	if (!d.has("type") || String(d["type"]) != "audio_bus_effect") {
		return false;
	}

	TreeItem *item = effects->get_item_at_position(p_point);
	if (!item) {
		return false;
	}

	effects->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);

	return true;
}

void EditorAudioBus::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	Dictionary d = p_data;

	TreeItem *item = effects->get_item_at_position(p_point);
	if (!item) {
		return;
	}
	int pos = effects->get_drop_section_at_position(p_point);
	Variant md = item->get_metadata(0);

	int paste_at;
	int bus = d["bus"];
	int effect = d["effect"];

	if (md.get_type() == Variant::INT) {
		paste_at = md;
		if (pos > 0) {
			paste_at++;
		}

		if (bus == get_index() && paste_at > effect) {
			paste_at--;
		}
	} else {
		paste_at = -1;
	}

	bool enabled = AudioServer::get_singleton()->is_bus_effect_enabled(bus, effect);

	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Move Bus Effect"));
	ur->add_do_method(AudioServer::get_singleton(), "remove_bus_effect", bus, effect);
	ur->add_do_method(AudioServer::get_singleton(), "add_bus_effect", get_index(), AudioServer::get_singleton()->get_bus_effect(bus, effect), paste_at);

	if (paste_at == -1) {
		paste_at = AudioServer::get_singleton()->get_bus_effect_count(get_index());
		if (bus == get_index()) {
			paste_at--;
		}
	}
	if (!enabled) {
		ur->add_do_method(AudioServer::get_singleton(), "set_bus_effect_enabled", get_index(), paste_at, false);
	}

	ur->add_undo_method(AudioServer::get_singleton(), "remove_bus_effect", get_index(), paste_at);
	ur->add_undo_method(AudioServer::get_singleton(), "add_bus_effect", bus, AudioServer::get_singleton()->get_bus_effect(bus, effect), effect);
	if (!enabled) {
		ur->add_undo_method(AudioServer::get_singleton(), "set_bus_effect_enabled", bus, effect, false);
	}

	ur->add_do_method(buses, "_update_bus", get_index());
	ur->add_undo_method(buses, "_update_bus", get_index());
	if (get_index() != bus) {
		ur->add_do_method(buses, "_update_bus", bus);
		ur->add_undo_method(buses, "_update_bus", bus);
	}
	ur->commit_action();
}

void EditorAudioBus::_delete_effect_pressed(int p_option) {
	TreeItem *item = effects->get_selected();
	if (!item) {
		return;
	}

	if (item->get_metadata(0).get_type() != Variant::INT) {
		return;
	}

	int index = item->get_metadata(0);

	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Delete Bus Effect"));
	ur->add_do_method(AudioServer::get_singleton(), "remove_bus_effect", get_index(), index);
	ur->add_undo_method(AudioServer::get_singleton(), "add_bus_effect", get_index(), AudioServer::get_singleton()->get_bus_effect(get_index(), index), index);
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_effect_enabled", get_index(), index, AudioServer::get_singleton()->is_bus_effect_enabled(get_index(), index));
	ur->add_do_method(buses, "_update_bus", get_index());
	ur->add_undo_method(buses, "_update_bus", get_index());
	ur->commit_action();
}

void EditorAudioBus::_effect_rmb(const Vector2 &p_pos) {
	TreeItem *item = effects->get_selected();
	if (!item) {
		return;
	}

	if (item->get_metadata(0).get_type() != Variant::INT) {
		return;
	}

	delete_effect_popup->set_position(get_global_mouse_position());
	delete_effect_popup->popup();
}

void EditorAudioBus::_bind_methods() {
	ClassDB::bind_method("update_bus", &EditorAudioBus::update_bus);
	ClassDB::bind_method("update_send", &EditorAudioBus::update_send);
	ClassDB::bind_method("_name_changed", &EditorAudioBus::_name_changed);
	ClassDB::bind_method("_volume_changed", &EditorAudioBus::_volume_changed);
	ClassDB::bind_method("_show_value", &EditorAudioBus::_show_value);
	ClassDB::bind_method("_hide_value_preview", &EditorAudioBus::_hide_value_preview);
	ClassDB::bind_method("_solo_toggled", &EditorAudioBus::_solo_toggled);
	ClassDB::bind_method("_mute_toggled", &EditorAudioBus::_mute_toggled);
	ClassDB::bind_method("_bypass_toggled", &EditorAudioBus::_bypass_toggled);
	ClassDB::bind_method("_name_focus_exit", &EditorAudioBus::_name_focus_exit);
	ClassDB::bind_method("_send_selected", &EditorAudioBus::_send_selected);
	ClassDB::bind_method("_effect_edited", &EditorAudioBus::_effect_edited);
	ClassDB::bind_method("_effect_selected", &EditorAudioBus::_effect_selected);
	ClassDB::bind_method("_effect_add", &EditorAudioBus::_effect_add);
	ClassDB::bind_method("_gui_input", &EditorAudioBus::_gui_input);
	ClassDB::bind_method("_unhandled_key_input", &EditorAudioBus::_unhandled_key_input);
	ClassDB::bind_method("_bus_popup_pressed", &EditorAudioBus::_bus_popup_pressed);
	ClassDB::bind_method("get_drag_data_fw", &EditorAudioBus::get_drag_data_fw);
	ClassDB::bind_method("can_drop_data_fw", &EditorAudioBus::can_drop_data_fw);
	ClassDB::bind_method("drop_data_fw", &EditorAudioBus::drop_data_fw);
	ClassDB::bind_method("_delete_effect_pressed", &EditorAudioBus::_delete_effect_pressed);
	ClassDB::bind_method("_effect_rmb", &EditorAudioBus::_effect_rmb);

	ADD_SIGNAL(MethodInfo("duplicate_request"));
	ADD_SIGNAL(MethodInfo("delete_request"));
	ADD_SIGNAL(MethodInfo("vol_reset_request"));
	ADD_SIGNAL(MethodInfo("drop_end_request"));
	ADD_SIGNAL(MethodInfo("dropped"));
}

EditorAudioBus::EditorAudioBus(EditorAudioBuses *p_buses, bool p_is_master) {
	buses = p_buses;
	updating_bus = false;
	is_master = p_is_master;
	hovering_drop = false;

	set_tooltip(TTR("Drag & drop to rearrange."));

	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	set_v_size_flags(SIZE_EXPAND_FILL);
	set_process_unhandled_key_input(true);

	track_name = memnew(LineEdit);
	track_name->connect("text_entered", this, "_name_changed");
	track_name->connect("focus_exited", this, "_name_focus_exit");
	vb->add_child(track_name);

	HBoxContainer *hbc = memnew(HBoxContainer);
	vb->add_child(hbc);
	solo = memnew(ToolButton);
	solo->set_toggle_mode(true);
	solo->set_tooltip(TTR("Solo"));
	solo->set_focus_mode(FOCUS_NONE);
	solo->connect("pressed", this, "_solo_toggled");
	hbc->add_child(solo);
	mute = memnew(ToolButton);
	mute->set_toggle_mode(true);
	mute->set_tooltip(TTR("Mute"));
	mute->set_focus_mode(FOCUS_NONE);
	mute->connect("pressed", this, "_mute_toggled");
	hbc->add_child(mute);
	bypass = memnew(ToolButton);
	bypass->set_toggle_mode(true);
	bypass->set_tooltip(TTR("Bypass"));
	bypass->set_focus_mode(FOCUS_NONE);
	bypass->connect("pressed", this, "_bypass_toggled");
	hbc->add_child(bypass);
	hbc->add_spacer();

	bus_options = memnew(MenuButton);
	bus_options->set_h_size_flags(SIZE_SHRINK_END);
	bus_options->set_anchor(MARGIN_RIGHT, 0.0);
	bus_options->set_tooltip(TTR("Bus Options"));
	hbc->add_child(bus_options);

	Ref<StyleBoxEmpty> sbempty = memnew(StyleBoxEmpty);
	for (int i = 0; i < hbc->get_child_count(); i++) {
		Control *child = Object::cast_to<Control>(hbc->get_child(i));
		child->add_style_override("normal", sbempty);
		child->add_style_override("hover", sbempty);
		child->add_style_override("focus", sbempty);
		child->add_style_override("pressed", sbempty);
	}

	HSeparator *separator = memnew(HSeparator);
	separator->set_mouse_filter(MOUSE_FILTER_PASS);
	vb->add_child(separator);

	HBoxContainer *hb = memnew(HBoxContainer);
	vb->add_child(hb);
	slider = memnew(VSlider);
	slider->set_min(0.0);
	slider->set_max(1.0);
	slider->set_step(0.0001);
	slider->set_clip_contents(false);

	audio_value_preview_box = memnew(Panel);
	slider->add_child(audio_value_preview_box);
	audio_value_preview_box->set_as_toplevel(true);
	audio_value_preview_box->set_mouse_filter(MOUSE_FILTER_PASS);
	audio_value_preview_box->hide();

	HBoxContainer *audioprev_hbc = memnew(HBoxContainer);
	audioprev_hbc->set_v_size_flags(SIZE_EXPAND_FILL);
	audioprev_hbc->set_h_size_flags(SIZE_EXPAND_FILL);
	audioprev_hbc->set_mouse_filter(MOUSE_FILTER_PASS);
	audio_value_preview_box->add_child(audioprev_hbc);

	audio_value_preview_label = memnew(Label);
	audio_value_preview_label->set_v_size_flags(SIZE_EXPAND_FILL);
	audio_value_preview_label->set_h_size_flags(SIZE_EXPAND_FILL);
	audio_value_preview_label->set_mouse_filter(MOUSE_FILTER_PASS);
	audioprev_hbc->add_child(audio_value_preview_label);

	preview_timer = memnew(Timer);
	preview_timer->set_wait_time(0.8f);
	preview_timer->set_one_shot(true);
	add_child(preview_timer);

	slider->connect("value_changed", this, "_volume_changed");
	slider->connect("value_changed", this, "_show_value");
	preview_timer->connect("timeout", this, "_hide_value_preview");
	hb->add_child(slider);

	cc = 0;
	for (int i = 0; i < CHANNELS_MAX; i++) {
		channel[i].vu_l = memnew(TextureProgress);
		channel[i].vu_l->set_fill_mode(TextureProgress::FILL_BOTTOM_TO_TOP);
		hb->add_child(channel[i].vu_l);
		channel[i].vu_l->set_min(-80);
		channel[i].vu_l->set_max(24);
		channel[i].vu_l->set_step(0.1);

		channel[i].vu_r = memnew(TextureProgress);
		channel[i].vu_r->set_fill_mode(TextureProgress::FILL_BOTTOM_TO_TOP);
		hb->add_child(channel[i].vu_r);
		channel[i].vu_r->set_min(-80);
		channel[i].vu_r->set_max(24);
		channel[i].vu_r->set_step(0.1);

		channel[i].peak_l = 0.0f;
		channel[i].peak_r = 0.0f;
	}

	EditorAudioMeterNotches *scale = memnew(EditorAudioMeterNotches);

	for (float db = 6.0f; db >= -80.0f; db -= 6.0f) {
		bool renderNotch = (db >= -6.0f || db == -24.0f || db == -72.0f);
		scale->add_notch(_scaled_db_to_normalized_volume(db), db, renderNotch);
	}
	scale->set_mouse_filter(MOUSE_FILTER_PASS);
	hb->add_child(scale);

	effects = memnew(Tree);
	effects->set_hide_root(true);
	effects->set_custom_minimum_size(Size2(0, 80) * EDSCALE);
	effects->set_hide_folding(true);
	effects->set_v_size_flags(SIZE_EXPAND_FILL);
	vb->add_child(effects);
	effects->connect("item_edited", this, "_effect_edited");
	effects->connect("cell_selected", this, "_effect_selected");
	effects->set_edit_checkbox_cell_only_when_checkbox_is_pressed(true);
	effects->set_drag_forwarding(this);
	effects->connect("item_rmb_selected", this, "_effect_rmb");
	effects->set_allow_rmb_select(true);
	effects->set_focus_mode(FOCUS_CLICK);
	effects->set_allow_reselect(true);

	send = memnew(OptionButton);
	send->set_clip_text(true);
	send->connect("item_selected", this, "_send_selected");
	vb->add_child(send);

	set_focus_mode(FOCUS_CLICK);

	effect_options = memnew(PopupMenu);
	effect_options->connect("index_pressed", this, "_effect_add");
	add_child(effect_options);
	List<StringName> effects;
	ClassDB::get_inheriters_from_class("AudioEffect", &effects);
	effects.sort_custom<StringName::AlphCompare>();
	for (List<StringName>::Element *E = effects.front(); E; E = E->next()) {
		if (!ClassDB::can_instance(E->get())) {
			continue;
		}

		String name = E->get().operator String().replace("AudioEffect", "");
		effect_options->add_item(name);
		effect_options->set_item_metadata(effect_options->get_item_count() - 1, E->get());
	}

	bus_popup = bus_options->get_popup();
	bus_popup->add_item(TTR("Duplicate"));
	bus_popup->add_item(TTR("Delete"));
	bus_popup->set_item_disabled(1, is_master);
	bus_popup->add_item(TTR("Reset Volume"));
	bus_popup->connect("index_pressed", this, "_bus_popup_pressed");

	delete_effect_popup = memnew(PopupMenu);
	delete_effect_popup->add_item(TTR("Delete Effect"));
	add_child(delete_effect_popup);
	delete_effect_popup->connect("index_pressed", this, "_delete_effect_pressed");
}

void EditorAudioBusDrop::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			draw_style_box(get_stylebox("normal", "Button"), Rect2(Vector2(), get_size()));

			if (hovering_drop) {
				Color accent = get_color("accent_color", "Editor");
				accent.a *= 0.7;
				draw_rect(Rect2(Point2(), get_size()), accent, false);
			}
		} break;
		case NOTIFICATION_MOUSE_ENTER: {
			if (!hovering_drop) {
				hovering_drop = true;
				update();
			}
		} break;
		case NOTIFICATION_MOUSE_EXIT:
		case NOTIFICATION_DRAG_END: {
			if (hovering_drop) {
				hovering_drop = false;
				update();
			}
		} break;
	}
}

bool EditorAudioBusDrop::can_drop_data(const Point2 &p_point, const Variant &p_data) const {
	Dictionary d = p_data;
	return (d.has("type") && String(d["type"]) == "move_audio_bus");
}

void EditorAudioBusDrop::drop_data(const Point2 &p_point, const Variant &p_data) {
	Dictionary d = p_data;
	emit_signal("dropped", d["index"], AudioServer::get_singleton()->get_bus_count());
}

void EditorAudioBusDrop::_bind_methods() {
	ADD_SIGNAL(MethodInfo("dropped"));
}

EditorAudioBusDrop::EditorAudioBusDrop() {
	hovering_drop = false;
}

void EditorAudioBuses::_update_buses() {
	while (bus_hb->get_child_count() > 0) {
		memdelete(bus_hb->get_child(0));
	}

	drop_end = nullptr;

	for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
		bool is_master = (i == 0);
		EditorAudioBus *audio_bus = memnew(EditorAudioBus(this, is_master));
		bus_hb->add_child(audio_bus);
		audio_bus->connect("delete_request", this, "_delete_bus", varray(audio_bus), CONNECT_DEFERRED);
		audio_bus->connect("duplicate_request", this, "_duplicate_bus", varray(), CONNECT_DEFERRED);
		audio_bus->connect("vol_reset_request", this, "_reset_bus_volume", varray(audio_bus), CONNECT_DEFERRED);
		audio_bus->connect("drop_end_request", this, "_request_drop_end");
		audio_bus->connect("dropped", this, "_drop_at_index", varray(), CONNECT_DEFERRED);
	}
}

EditorAudioBuses *EditorAudioBuses::register_editor() {
	EditorAudioBuses *audio_buses = memnew(EditorAudioBuses);
	EditorNode::get_singleton()->add_bottom_panel_item(TTR("Audio"), audio_buses);
	return audio_buses;
}

void EditorAudioBuses::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			bus_scroll->add_style_override("bg", get_stylebox("bg", "Tree"));
		} break;
		case NOTIFICATION_READY: {
			_update_buses();
		} break;
		case NOTIFICATION_DRAG_END: {
			if (drop_end) {
				drop_end->queue_delete();
				drop_end = nullptr;
			}
		} break;
		case NOTIFICATION_PROCESS: {
			// Check if anything was edited.
			bool edited = AudioServer::get_singleton()->is_edited();
			for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
				for (int j = 0; j < AudioServer::get_singleton()->get_bus_effect_count(i); j++) {
					Ref<AudioEffect> effect = AudioServer::get_singleton()->get_bus_effect(i, j);
					if (effect->is_edited()) {
						edited = true;
						effect->set_edited(false);
					}
				}
			}

			AudioServer::get_singleton()->set_edited(false);

			if (edited) {
				save_timer->start();
			}
		} break;
	}
}

void EditorAudioBuses::_add_bus() {
	UndoRedo *ur = EditorNode::get_undo_redo();

	ur->create_action(TTR("Add Audio Bus"));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_count", AudioServer::get_singleton()->get_bus_count() + 1);
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_count", AudioServer::get_singleton()->get_bus_count());
	ur->add_do_method(this, "_update_buses");
	ur->add_undo_method(this, "_update_buses");
	ur->commit_action();
}

void EditorAudioBuses::_update_bus(int p_index) {
	if (p_index >= bus_hb->get_child_count()) {
		return;
	}

	bus_hb->get_child(p_index)->call("update_bus");
}

void EditorAudioBuses::_update_sends() {
	for (int i = 0; i < bus_hb->get_child_count(); i++) {
		bus_hb->get_child(i)->call("update_send");
	}
}

void EditorAudioBuses::_delete_bus(Object *p_which) {
	EditorAudioBus *bus = Object::cast_to<EditorAudioBus>(p_which);
	int index = bus->get_index();
	if (index == 0) {
		EditorNode::get_singleton()->show_warning(TTR("Master bus can't be deleted!"));
		return;
	}

	UndoRedo *ur = EditorNode::get_undo_redo();

	ur->create_action(TTR("Delete Audio Bus"));
	ur->add_do_method(AudioServer::get_singleton(), "remove_bus", index);
	ur->add_undo_method(AudioServer::get_singleton(), "add_bus", index);
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_name", index, AudioServer::get_singleton()->get_bus_name(index));
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_volume_db", index, AudioServer::get_singleton()->get_bus_volume_db(index));
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_send", index, AudioServer::get_singleton()->get_bus_send(index));
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_solo", index, AudioServer::get_singleton()->is_bus_solo(index));
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_mute", index, AudioServer::get_singleton()->is_bus_mute(index));
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_bypass_effects", index, AudioServer::get_singleton()->is_bus_bypassing_effects(index));
	for (int i = 0; i < AudioServer::get_singleton()->get_bus_effect_count(index); i++) {
		ur->add_undo_method(AudioServer::get_singleton(), "add_bus_effect", index, AudioServer::get_singleton()->get_bus_effect(index, i));
		ur->add_undo_method(AudioServer::get_singleton(), "set_bus_effect_enabled", index, i, AudioServer::get_singleton()->is_bus_effect_enabled(index, i));
	}
	ur->add_do_method(this, "_update_buses");
	ur->add_undo_method(this, "_update_buses");
	ur->commit_action();
}

void EditorAudioBuses::_duplicate_bus(int p_which) {
	int add_at_pos = p_which + 1;
	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Duplicate Audio Bus"));
	ur->add_do_method(AudioServer::get_singleton(), "add_bus", add_at_pos);
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_name", add_at_pos, AudioServer::get_singleton()->get_bus_name(p_which) + " Copy");
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_volume_db", add_at_pos, AudioServer::get_singleton()->get_bus_volume_db(p_which));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_send", add_at_pos, AudioServer::get_singleton()->get_bus_send(p_which));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_solo", add_at_pos, AudioServer::get_singleton()->is_bus_solo(p_which));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_mute", add_at_pos, AudioServer::get_singleton()->is_bus_mute(p_which));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_bypass_effects", add_at_pos, AudioServer::get_singleton()->is_bus_bypassing_effects(p_which));
	for (int i = 0; i < AudioServer::get_singleton()->get_bus_effect_count(p_which); i++) {
		ur->add_do_method(AudioServer::get_singleton(), "add_bus_effect", add_at_pos, AudioServer::get_singleton()->get_bus_effect(p_which, i));
		ur->add_do_method(AudioServer::get_singleton(), "set_bus_effect_enabled", add_at_pos, i, AudioServer::get_singleton()->is_bus_effect_enabled(p_which, i));
	}
	ur->add_undo_method(AudioServer::get_singleton(), "remove_bus", add_at_pos);
	ur->add_do_method(this, "_update_buses");
	ur->add_undo_method(this, "_update_buses");
	ur->commit_action();
}

void EditorAudioBuses::_reset_bus_volume(Object *p_which) {
	EditorAudioBus *bus = Object::cast_to<EditorAudioBus>(p_which);
	int index = bus->get_index();

	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Reset Bus Volume"));
	ur->add_do_method(AudioServer::get_singleton(), "set_bus_volume_db", index, 0.f);
	ur->add_undo_method(AudioServer::get_singleton(), "set_bus_volume_db", index, AudioServer::get_singleton()->get_bus_volume_db(index));
	ur->add_do_method(this, "_update_buses");
	ur->add_undo_method(this, "_update_buses");
	ur->commit_action();
}

void EditorAudioBuses::_request_drop_end() {
	if (!drop_end && bus_hb->get_child_count()) {
		drop_end = memnew(EditorAudioBusDrop);

		bus_hb->add_child(drop_end);
		drop_end->set_custom_minimum_size(Object::cast_to<Control>(bus_hb->get_child(0))->get_size());
		drop_end->connect("dropped", this, "_drop_at_index", varray(), CONNECT_DEFERRED);
	}
}

void EditorAudioBuses::_drop_at_index(int p_bus, int p_index) {
	UndoRedo *ur = EditorNode::get_undo_redo();
	ur->create_action(TTR("Move Audio Bus"));

	ur->add_do_method(AudioServer::get_singleton(), "move_bus", p_bus, p_index);
	int real_bus = p_index > p_bus ? p_bus : p_bus + 1;
	int real_index = p_index > p_bus ? p_index - 1 : p_index;
	ur->add_undo_method(AudioServer::get_singleton(), "move_bus", real_index, real_bus);

	ur->add_do_method(this, "_update_buses");
	ur->add_undo_method(this, "_update_buses");
	ur->commit_action();
}

void EditorAudioBuses::_server_save() {
	Ref<AudioBusLayout> state = AudioServer::get_singleton()->generate_bus_layout();
	ResourceSaver::save(edited_path, state);
}

void EditorAudioBuses::_select_layout() {
	EditorNode::get_singleton()->get_filesystem_dock()->select_file(edited_path);
}

void EditorAudioBuses::_save_as_layout() {
	file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	file_dialog->set_title(TTR("Save Audio Bus Layout As..."));
	file_dialog->set_current_path(edited_path);
	file_dialog->popup_centered_ratio();
	new_layout = false;
}

void EditorAudioBuses::_new_layout() {
	file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	file_dialog->set_title(TTR("Location for New Layout..."));
	file_dialog->set_current_path(edited_path);
	file_dialog->popup_centered_ratio();
	new_layout = true;
}

void EditorAudioBuses::_load_layout() {
	file_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	file_dialog->set_title(TTR("Open Audio Bus Layout"));
	file_dialog->set_current_path(edited_path);
	file_dialog->popup_centered_ratio();
	new_layout = false;
}

void EditorAudioBuses::_load_default_layout() {
	String layout_path = ProjectSettings::get_singleton()->get("audio/default_bus_layout");

	Ref<AudioBusLayout> state;
	if (ResourceLoader::exists(layout_path)) {
		state = ResourceLoader::load(layout_path, "", true);
	}
	if (state.is_null()) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("There is no '%s' file."), layout_path));
		return;
	}

	edited_path = layout_path;
	file->set_text(String(TTR("Layout:")) + " " + layout_path.get_file());
	AudioServer::get_singleton()->set_bus_layout(state);
	_update_buses();
	EditorNode::get_singleton()->get_undo_redo()->clear_history();
	call_deferred("_select_layout");
}

void EditorAudioBuses::_file_dialog_callback(const String &p_string) {
	if (file_dialog->get_mode() == EditorFileDialog::MODE_OPEN_FILE) {
		Ref<AudioBusLayout> state = ResourceLoader::load(p_string, "", true);
		if (state.is_null()) {
			EditorNode::get_singleton()->show_warning(TTR("Invalid file, not an audio bus layout."));
			return;
		}

		edited_path = p_string;
		file->set_text(String(TTR("Layout:")) + " " + p_string.get_file());
		AudioServer::get_singleton()->set_bus_layout(state);
		_update_buses();
		EditorNode::get_singleton()->get_undo_redo()->clear_history();
		call_deferred("_select_layout");

	} else if (file_dialog->get_mode() == EditorFileDialog::MODE_SAVE_FILE) {
		if (new_layout) {
			Ref<AudioBusLayout> empty_state;
			empty_state.instance();
			AudioServer::get_singleton()->set_bus_layout(empty_state);
		}

		Error err = ResourceSaver::save(p_string, AudioServer::get_singleton()->generate_bus_layout());

		if (err != OK) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("Error saving file: %s"), p_string));
			return;
		}

		edited_path = p_string;
		file->set_text(String(TTR("Layout:")) + " " + p_string.get_file());
		_update_buses();
		EditorNode::get_singleton()->get_undo_redo()->clear_history();
		call_deferred("_select_layout");
	}
}

void EditorAudioBuses::_bind_methods() {
	ClassDB::bind_method("_add_bus", &EditorAudioBuses::_add_bus);
	ClassDB::bind_method("_update_buses", &EditorAudioBuses::_update_buses);
	ClassDB::bind_method("_update_bus", &EditorAudioBuses::_update_bus);
	ClassDB::bind_method("_update_sends", &EditorAudioBuses::_update_sends);
	ClassDB::bind_method("_delete_bus", &EditorAudioBuses::_delete_bus);
	ClassDB::bind_method("_request_drop_end", &EditorAudioBuses::_request_drop_end);
	ClassDB::bind_method("_drop_at_index", &EditorAudioBuses::_drop_at_index);
	ClassDB::bind_method("_server_save", &EditorAudioBuses::_server_save);
	ClassDB::bind_method("_select_layout", &EditorAudioBuses::_select_layout);
	ClassDB::bind_method("_save_as_layout", &EditorAudioBuses::_save_as_layout);
	ClassDB::bind_method("_load_layout", &EditorAudioBuses::_load_layout);
	ClassDB::bind_method("_load_default_layout", &EditorAudioBuses::_load_default_layout);
	ClassDB::bind_method("_new_layout", &EditorAudioBuses::_new_layout);
	ClassDB::bind_method("_duplicate_bus", &EditorAudioBuses::_duplicate_bus);
	ClassDB::bind_method("_reset_bus_volume", &EditorAudioBuses::_reset_bus_volume);

	ClassDB::bind_method("_file_dialog_callback", &EditorAudioBuses::_file_dialog_callback);
}

EditorAudioBuses::EditorAudioBuses() {
	drop_end = nullptr;
	top_hb = memnew(HBoxContainer);
	add_child(top_hb);

	file = memnew(Label);
	String layout_path = ProjectSettings::get_singleton()->get("audio/default_bus_layout");
	file->set_text(String(TTR("Layout:")) + " " + layout_path.get_file());
	file->set_clip_text(true);
	file->set_h_size_flags(SIZE_EXPAND_FILL);
	top_hb->add_child(file);

	add = memnew(Button);
	top_hb->add_child(add);
	add->set_text(TTR("Add Bus"));
	add->set_tooltip(TTR("Add a new Audio Bus to this layout."));
	add->connect("pressed", this, "_add_bus");

	VSeparator *separator = memnew(VSeparator);
	top_hb->add_child(separator);

	load = memnew(Button);
	load->set_text(TTR("Load"));
	load->set_tooltip(TTR("Load an existing Bus Layout."));
	top_hb->add_child(load);
	load->connect("pressed", this, "_load_layout");

	save_as = memnew(Button);
	save_as->set_text(TTR("Save As"));
	save_as->set_tooltip(TTR("Save this Bus Layout to a file."));
	top_hb->add_child(save_as);
	save_as->connect("pressed", this, "_save_as_layout");

	_default = memnew(Button);
	_default->set_text(TTR("Load Default"));
	_default->set_tooltip(TTR("Load the default Bus Layout."));
	top_hb->add_child(_default);
	_default->connect("pressed", this, "_load_default_layout");

	_new = memnew(Button);
	_new->set_text(TTR("Create"));
	_new->set_tooltip(TTR("Create a new Bus Layout."));
	top_hb->add_child(_new);
	_new->connect("pressed", this, "_new_layout");

	bus_scroll = memnew(ScrollContainer);
	bus_scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	bus_scroll->set_enable_h_scroll(true);
	bus_scroll->set_enable_v_scroll(false);
	add_child(bus_scroll);
	bus_hb = memnew(HBoxContainer);
	bus_hb->set_v_size_flags(SIZE_EXPAND_FILL);
	bus_scroll->add_child(bus_hb);

	save_timer = memnew(Timer);
	save_timer->set_wait_time(0.8);
	save_timer->set_one_shot(true);
	add_child(save_timer);
	save_timer->connect("timeout", this, "_server_save");

	set_v_size_flags(SIZE_EXPAND_FILL);

	edited_path = ProjectSettings::get_singleton()->get("audio/default_bus_layout");

	file_dialog = memnew(EditorFileDialog);
	List<String> ext;
	ResourceLoader::get_recognized_extensions_for_type("AudioBusLayout", &ext);
	for (List<String>::Element *E = ext.front(); E; E = E->next()) {
		file_dialog->add_filter("*." + E->get(), TTR("Audio Bus Layout"));
	}
	add_child(file_dialog);
	file_dialog->connect("file_selected", this, "_file_dialog_callback");

	set_process(true);
}

void EditorAudioBuses::open_layout(const String &p_path) {
	EditorNode::get_singleton()->make_bottom_panel_item_visible(this);

	Ref<AudioBusLayout> state = ResourceLoader::load(p_path, "", true);
	if (state.is_null()) {
		EditorNode::get_singleton()->show_warning(TTR("Invalid file, not an audio bus layout."));
		return;
	}

	edited_path = p_path;
	file->set_text(p_path.get_file());
	AudioServer::get_singleton()->set_bus_layout(state);
	_update_buses();
	EditorNode::get_singleton()->get_undo_redo()->clear_history();
	call_deferred("_select_layout");
}

void AudioBusesEditorPlugin::edit(Object *p_node) {
	if (Object::cast_to<AudioBusLayout>(p_node)) {
		String path = Object::cast_to<AudioBusLayout>(p_node)->get_path();
		if (path.is_resource_file()) {
			audio_bus_editor->open_layout(path);
		}
	}
}

bool AudioBusesEditorPlugin::handles(Object *p_node) const {
	return (Object::cast_to<AudioBusLayout>(p_node) != nullptr);
}

void AudioBusesEditorPlugin::make_visible(bool p_visible) {
}

AudioBusesEditorPlugin::AudioBusesEditorPlugin(EditorAudioBuses *p_node) {
	audio_bus_editor = p_node;
}

AudioBusesEditorPlugin::~AudioBusesEditorPlugin() {
}

void EditorAudioMeterNotches::add_notch(float p_normalized_offset, float p_db_value, bool p_render_value) {
	notches.push_back(AudioNotch(p_normalized_offset, p_db_value, p_render_value));
}

Size2 EditorAudioMeterNotches::get_minimum_size() const {
	Ref<Font> font = get_font("font", "Label");
	float font_height = font->get_height();

	float width = 0;
	float height = top_padding + btm_padding;

	for (int i = 0; i < notches.size(); i++) {
		if (notches[i].render_db_value) {
			width = MAX(width, font->get_string_size(String::num(Math::abs(notches[i].db_value)) + "dB").x);
			height += font_height;
		}
	}
	width += line_length + label_space;

	return Size2(width, height);
}

void EditorAudioMeterNotches::_bind_methods() {
	ClassDB::bind_method("add_notch", &EditorAudioMeterNotches::add_notch);
	ClassDB::bind_method("_draw_audio_notches", &EditorAudioMeterNotches::_draw_audio_notches);
}

void EditorAudioMeterNotches::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			notch_color = get_color("font_color", "Editor");
		} break;
		case NOTIFICATION_DRAW: {
			_draw_audio_notches();
		} break;
	}
}

void EditorAudioMeterNotches::_draw_audio_notches() {
	Ref<Font> font = get_font("font", "Label");
	float font_height = font->get_height();

	for (int i = 0; i < notches.size(); i++) {
		AudioNotch n = notches[i];
		draw_line(Vector2(0, (1.0f - n.relative_position) * (get_size().y - btm_padding - top_padding) + top_padding),
				Vector2(line_length * EDSCALE, (1.0f - n.relative_position) * (get_size().y - btm_padding - top_padding) + top_padding),
				notch_color,
				Math::round(EDSCALE));

		if (n.render_db_value) {
			draw_string(font,
					Vector2((line_length + label_space) * EDSCALE,
							(1.0f - n.relative_position) * (get_size().y - btm_padding - top_padding) + (font_height / 4) + top_padding),
					String::num(Math::abs(n.db_value)) + "dB",
					notch_color);
		}
	}
}

EditorAudioMeterNotches::EditorAudioMeterNotches() {
	notch_color = get_color("font_color", "Editor");
}
