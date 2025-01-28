/**************************************************************************/
/*  audio_stream_import_settings.cpp                                      */
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

#include "audio_stream_import_settings.h"
#include "editor/audio_stream_preview.h"
#include "editor/editor_file_system.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_box.h"

AudioStreamImportSettingsDialog *AudioStreamImportSettingsDialog::singleton = nullptr;

void AudioStreamImportSettingsDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			AudioStreamPreviewGenerator::get_singleton()->connect("preview_updated", callable_mp(this, &AudioStreamImportSettingsDialog::_preview_changed));
			connect(SceneStringName(confirmed), callable_mp(this, &AudioStreamImportSettingsDialog::_reimport));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
			_stop_button->set_button_icon(get_editor_theme_icon(SNAME("Stop")));

			_preview->set_color(get_theme_color(SNAME("dark_color_2"), EditorStringName(Editor)));
			color_rect->set_color(get_theme_color(SNAME("dark_color_1"), EditorStringName(Editor)));

			_current_label->begin_bulk_theme_override();
			_current_label->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("status_source"), EditorStringName(EditorFonts)));
			_current_label->add_theme_font_size_override(SceneStringName(font_size), get_theme_font_size(SNAME("status_source_size"), EditorStringName(EditorFonts)));
			_current_label->end_bulk_theme_override();

			_duration_label->begin_bulk_theme_override();
			_duration_label->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("status_source"), EditorStringName(EditorFonts)));
			_duration_label->add_theme_font_size_override(SceneStringName(font_size), get_theme_font_size(SNAME("status_source_size"), EditorStringName(EditorFonts)));
			_duration_label->end_bulk_theme_override();

			zoom_in->set_button_icon(get_editor_theme_icon(SNAME("ZoomMore")));
			zoom_out->set_button_icon(get_editor_theme_icon(SNAME("ZoomLess")));
			zoom_reset->set_button_icon(get_editor_theme_icon(SNAME("ZoomReset")));

			_indicator->queue_redraw();
			_preview->queue_redraw();
		} break;

		case NOTIFICATION_PROCESS: {
			_current = _player->get_playback_position();
			_indicator->queue_redraw();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				_stop();
			}
		} break;
	}
}

void AudioStreamImportSettingsDialog::_draw_preview() {
	Rect2 rect = _preview->get_rect();
	Size2 rect_size = rect.size;
	int width = rect_size.width;

	Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);
	float preview_offset = zoom_bar->get_value();
	float preview_len = zoom_bar->get_page();

	Ref<Font> beat_font = get_theme_font(SNAME("main"), EditorStringName(EditorFonts));
	int main_size = get_theme_font_size(SNAME("main_size"), EditorStringName(EditorFonts));
	Vector<Vector2> points;
	points.resize(width * 2);
	Color color_active = get_theme_color(SNAME("contrast_color_2"), EditorStringName(Editor));
	Color color_inactive = color_active;
	color_inactive.a *= 0.5;
	Vector<Color> colors;
	colors.resize(width);

	float inactive_from = 1e20;
	float beat_size = 0;
	int last_beat = 0;
	if (stream->get_bpm() > 0) {
		beat_size = 60 / float(stream->get_bpm());
		int y_ofs = beat_font->get_height(main_size) + 4 * EDSCALE;
		rect.position.y += y_ofs;
		rect.size.y -= y_ofs;

		if (stream->get_beat_count() > 0) {
			last_beat = stream->get_beat_count();
			inactive_from = last_beat * beat_size;
		}
	}

	for (int i = 0; i < width; i++) {
		float ofs = preview_offset + i * preview_len / rect_size.width;
		float ofs_n = preview_offset + (i + 1) * preview_len / rect_size.width;
		float max = preview->get_max(ofs, ofs_n) * 0.5 + 0.5;
		float min = preview->get_min(ofs, ofs_n) * 0.5 + 0.5;

		int idx = i;
		points.write[idx * 2 + 0] = Vector2(i + 1, rect.position.y + min * rect.size.y);
		points.write[idx * 2 + 1] = Vector2(i + 1, rect.position.y + max * rect.size.y);

		colors.write[idx] = ofs > inactive_from ? color_inactive : color_active;
	}

	if (!points.is_empty()) {
		RS::get_singleton()->canvas_item_add_multiline(_preview->get_canvas_item(), points, colors);
	}

	if (beat_size) {
		Color beat_color = Color(1, 1, 1, 1);
		Color final_beat_color = beat_color;
		Color bar_color = beat_color;
		beat_color.a *= 0.4;
		bar_color.a *= 0.6;

		int prev_beat = 0; // Do not draw beat zero
		Color color_bg = color_active;
		color_bg.a *= 0.2;
		_preview->draw_rect(Rect2(0, 0, rect.size.width, rect.position.y), color_bg);
		int bar_beats = stream->get_bar_beats();

		int last_text_end_x = 0;
		for (int i = 0; i < width; i++) {
			float ofs = preview_offset + i * preview_len / rect_size.width;
			int beat = int(ofs / beat_size);
			if (beat != prev_beat) {
				String text = itos(beat);
				int text_w = beat_font->get_string_size(text).width;
				if (i - text_w / 2 > last_text_end_x + 2 * EDSCALE) {
					int x_ofs = i - text_w / 2;
					_preview->draw_string(beat_font, Point2(x_ofs, 2 * EDSCALE + beat_font->get_ascent(main_size)), text, HORIZONTAL_ALIGNMENT_LEFT, rect.size.width - x_ofs, Font::DEFAULT_FONT_SIZE, color_active);
					last_text_end_x = i + text_w / 2;
				}

				if (beat == last_beat) {
					_preview->draw_rect(Rect2i(i, rect.position.y, 2, rect.size.height), final_beat_color);
					// Darken subsequent beats
					beat_color.a *= 0.3;
					color_active.a *= 0.3;
				} else {
					_preview->draw_rect(Rect2i(i, rect.position.y, 1, rect.size.height), (beat % bar_beats) == 0 ? bar_color : beat_color);
				}
				prev_beat = beat;
			}
		}
	}
}

void AudioStreamImportSettingsDialog::_preview_changed(ObjectID p_which) {
	if (stream.is_valid() && stream->get_instance_id() == p_which) {
		_preview->queue_redraw();
	}
}

void AudioStreamImportSettingsDialog::_preview_zoom_in() {
	if (stream.is_null()) {
		return;
	}
	float page_size = zoom_bar->get_page();
	zoom_bar->set_page(page_size * 0.5);
	zoom_bar->set_value(zoom_bar->get_value() + page_size * 0.25);

	_preview->queue_redraw();
	_indicator->queue_redraw();
}

void AudioStreamImportSettingsDialog::_preview_zoom_out() {
	if (stream.is_null()) {
		return;
	}
	float page_size = zoom_bar->get_page();
	zoom_bar->set_page(MIN(zoom_bar->get_max(), page_size * 2.0));
	zoom_bar->set_value(zoom_bar->get_value() - page_size * 0.5);

	_preview->queue_redraw();
	_indicator->queue_redraw();
}

void AudioStreamImportSettingsDialog::_preview_zoom_reset() {
	if (stream.is_null()) {
		return;
	}
	zoom_bar->set_max(stream->get_length());
	zoom_bar->set_page(zoom_bar->get_max());
	zoom_bar->set_value(0);
	_preview->queue_redraw();
	_indicator->queue_redraw();
}

void AudioStreamImportSettingsDialog::_preview_zoom_offset_changed(double) {
	_preview->queue_redraw();
	_indicator->queue_redraw();
}

void AudioStreamImportSettingsDialog::_audio_changed() {
	if (!is_visible()) {
		return;
	}
	_preview->queue_redraw();
	_indicator->queue_redraw();
	color_rect->queue_redraw();
}

void AudioStreamImportSettingsDialog::_play() {
	if (_player->is_playing()) {
		// '_pausing' variable indicates that we want to pause the audio player, not stop it. See '_on_finished()'.
		_pausing = true;
		_player->stop();
		_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
		set_process(false);
	} else {
		_player->play(_current);
		_play_button->set_button_icon(get_editor_theme_icon(SNAME("Pause")));
		set_process(true);
	}
}

void AudioStreamImportSettingsDialog::_stop() {
	_player->stop();
	_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
	_current = 0;
	_indicator->queue_redraw();
	set_process(false);
}

void AudioStreamImportSettingsDialog::_on_finished() {
	_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
	if (!_pausing) {
		_current = 0;
		_indicator->queue_redraw();
	} else {
		_pausing = false;
	}
	set_process(false);
}

void AudioStreamImportSettingsDialog::_draw_indicator() {
	if (stream.is_null()) {
		return;
	}

	Rect2 rect = _preview->get_rect();

	Ref<Font> beat_font = get_theme_font(SNAME("main"), EditorStringName(EditorFonts));
	int main_size = get_theme_font_size(SNAME("main_size"), EditorStringName(EditorFonts));

	if (stream->get_bpm() > 0) {
		int y_ofs = beat_font->get_height(main_size) + 4 * EDSCALE;
		rect.position.y += y_ofs;
		rect.size.height -= y_ofs;
	}

	_current_label->set_text(String::num(_current, 2).pad_decimals(2) + " /");

	float ofs_x = (_current - zoom_bar->get_value()) * rect.size.width / zoom_bar->get_page();
	if (ofs_x < 0 || ofs_x >= rect.size.width) {
		return;
	}

	const Color color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	_indicator->draw_line(Point2(ofs_x, rect.position.y), Point2(ofs_x, rect.position.y + rect.size.height), color, Math::round(2 * EDSCALE));
	_indicator->draw_texture(
			get_editor_theme_icon(SNAME("TimelineIndicator")),
			Point2(ofs_x - get_editor_theme_icon(SNAME("TimelineIndicator"))->get_width() * 0.5, rect.position.y),
			color);

	if (stream->get_bpm() > 0 && _hovering_beat != -1) {
		// Draw hovered beat.
		float preview_offset = zoom_bar->get_value();
		float preview_len = zoom_bar->get_page();
		float beat_size = 60 / float(stream->get_bpm());
		int prev_beat = 0;
		for (int i = 0; i < rect.size.width; i++) {
			float ofs = preview_offset + i * preview_len / rect.size.width;
			int beat = int(ofs / beat_size);
			if (beat != prev_beat) {
				String text = itos(beat);
				int text_w = beat_font->get_string_size(text).width;
				if (i - text_w / 2 > 2 * EDSCALE && beat == _hovering_beat) {
					int x_ofs = i - text_w / 2;
					_indicator->draw_string(beat_font, Point2(x_ofs, 2 * EDSCALE + beat_font->get_ascent(main_size)), text, HORIZONTAL_ALIGNMENT_LEFT, rect.size.width - x_ofs, Font::DEFAULT_FONT_SIZE, color);
					break;
				}
				prev_beat = beat;
			}
		}
	}
}

void AudioStreamImportSettingsDialog::_on_indicator_mouse_exited() {
	_hovering_beat = -1;
	_indicator->queue_redraw();
}

void AudioStreamImportSettingsDialog::_on_input_indicator(Ref<InputEvent> p_event) {
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		if (stream->get_bpm() > 0) {
			int main_size = get_theme_font_size(SNAME("main_size"), EditorStringName(EditorFonts));
			Ref<Font> beat_font = get_theme_font(SNAME("main"), EditorStringName(EditorFonts));
			int y_ofs = beat_font->get_height(main_size) + 4 * EDSCALE;
			if ((!_dragging && mb->get_position().y < y_ofs) || _beat_len_dragging) {
				if (mb->is_pressed()) {
					_set_beat_len_to(mb->get_position().x);
					_beat_len_dragging = true;
				} else {
					_beat_len_dragging = false;
				}
				return;
			}
		}

		if (mb->is_pressed()) {
			_seek_to(mb->get_position().x);
		}
		_dragging = mb->is_pressed();
	}

	const Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (_dragging) {
			_seek_to(mm->get_position().x);
		}
		if (_beat_len_dragging) {
			_set_beat_len_to(mm->get_position().x);
		}
		if (stream->get_bpm() > 0) {
			int main_size = get_theme_font_size(SNAME("main_size"), EditorStringName(EditorFonts));
			Ref<Font> beat_font = get_theme_font(SNAME("main"), EditorStringName(EditorFonts));
			int y_ofs = beat_font->get_height(main_size) + 4 * EDSCALE;
			if (mm->get_position().y < y_ofs) {
				int new_hovering_beat = _get_beat_at_pos(mm->get_position().x);
				if (new_hovering_beat != _hovering_beat) {
					_hovering_beat = new_hovering_beat;
					_indicator->queue_redraw();
				}
			} else if (_hovering_beat != -1) {
				_hovering_beat = -1;
				_indicator->queue_redraw();
			}
		}
	}
}

int AudioStreamImportSettingsDialog::_get_beat_at_pos(real_t p_x) {
	float ofs_sec = zoom_bar->get_value() + p_x * zoom_bar->get_page() / _preview->get_size().width;
	ofs_sec = CLAMP(ofs_sec, 0, stream->get_length());
	float beat_size = 60 / float(stream->get_bpm());
	int beat = int(ofs_sec / beat_size + 0.5);

	if (beat * beat_size > stream->get_length() + 0.001) { // Stream may end few audio frames before but may still want to use full loop.
		beat--;
	}
	return beat;
}

void AudioStreamImportSettingsDialog::_set_beat_len_to(real_t p_x) {
	int beat = _get_beat_at_pos(p_x);
	if (beat < 1) {
		beat = 1; // Because 0 is disable.
	}
	updating_settings = true;
	beats_enabled->set_pressed(true);
	beats_edit->set_value(beat);
	updating_settings = false;
	_settings_changed();
}

void AudioStreamImportSettingsDialog::_seek_to(real_t p_x) {
	_current = zoom_bar->get_value() + p_x / _preview->get_rect().size.x * zoom_bar->get_page();
	_current = CLAMP(_current, 0, stream->get_length());
	_player->seek(_current);
	_indicator->queue_redraw();
}

void AudioStreamImportSettingsDialog::edit(const String &p_path, const String &p_importer, const Ref<AudioStream> &p_stream) {
	if (stream.is_valid()) {
		stream->disconnect_changed(callable_mp(this, &AudioStreamImportSettingsDialog::_audio_changed));
	}

	importer = p_importer;
	path = p_path;

	stream = p_stream;
	_player->set_stream(stream);
	_current = 0;
	String text = String::num(stream->get_length(), 2).pad_decimals(2) + "s";
	_duration_label->set_text(text);

	if (stream.is_valid()) {
		stream->connect_changed(callable_mp(this, &AudioStreamImportSettingsDialog::_audio_changed));
		_preview->queue_redraw();
		_indicator->queue_redraw();
		color_rect->queue_redraw();
	} else {
		hide();
	}
	params.clear();

	if (stream.is_valid()) {
		Ref<ConfigFile> config_file;
		config_file.instantiate();
		Error err = config_file->load(p_path + ".import");
		updating_settings = true;
		if (err == OK) {
			double bpm = config_file->get_value("params", "bpm", 0);
			int beats = config_file->get_value("params", "beat_count", 0);
			bpm_edit->set_value(bpm > 0 ? bpm : 120);
			bpm_enabled->set_pressed(bpm > 0);
			beats_edit->set_value(beats);
			beats_enabled->set_pressed(beats > 0);
			loop->set_pressed(config_file->get_value("params", "loop", false));
			loop_offset->set_value(config_file->get_value("params", "loop_offset", 0));
			bar_beats_edit->set_value(config_file->get_value("params", "bar_beats", 4));

			List<String> keys;
			config_file->get_section_keys("params", &keys);
			for (const String &K : keys) {
				params[K] = config_file->get_value("params", K);
			}
		} else {
			bpm_edit->set_value(false);
			bpm_enabled->set_pressed(false);
			beats_edit->set_value(0);
			beats_enabled->set_pressed(false);
			bar_beats_edit->set_value(4);
			loop->set_pressed(false);
			loop_offset->set_value(0);
		}

		_preview_zoom_reset();
		updating_settings = false;
		_settings_changed();

		set_title(vformat(TTR("Audio Stream Importer: %s"), p_path.get_file()));
		popup_centered();
	}
}

void AudioStreamImportSettingsDialog::_settings_changed() {
	if (updating_settings) {
		return;
	}

	updating_settings = true;
	stream->call("set_loop", loop->is_pressed());
	stream->call("set_loop_offset", loop_offset->get_value());
	if (loop->is_pressed()) {
		loop_offset->set_editable(true);
	} else {
		loop_offset->set_editable(false);
	}

	if (bpm_enabled->is_pressed()) {
		stream->call("set_bpm", bpm_edit->get_value());
		beats_enabled->set_disabled(false);
		beats_edit->set_editable(true);
		bar_beats_edit->set_editable(true);
		double bpm = bpm_edit->get_value();
		if (bpm > 0) {
			float beat_size = 60 / float(bpm);
			int beat_max = int((stream->get_length() + 0.001) / beat_size);
			int current_beat = beats_edit->get_value();
			beats_edit->set_max(beat_max);
			if (current_beat > beat_max) {
				beats_edit->set_value(beat_max);
				stream->call("set_beat_count", beat_max);
			}
		}
		stream->call("set_bar_beats", bar_beats_edit->get_value());
	} else {
		stream->call("set_bpm", 0);
		stream->call("set_bar_beats", 4);
		beats_enabled->set_disabled(true);
		beats_edit->set_editable(false);
		bar_beats_edit->set_editable(false);
	}
	if (bpm_enabled->is_pressed() && beats_enabled->is_pressed()) {
		stream->call("set_beat_count", beats_edit->get_value());
	} else {
		stream->call("set_beat_count", 0);
	}

	updating_settings = false;

	_preview->queue_redraw();
	_indicator->queue_redraw();
	color_rect->queue_redraw();
}

void AudioStreamImportSettingsDialog::_reimport() {
	params["loop"] = loop->is_pressed();
	params["loop_offset"] = loop_offset->get_value();
	params["bpm"] = bpm_enabled->is_pressed() ? double(bpm_edit->get_value()) : double(0);
	params["beat_count"] = (bpm_enabled->is_pressed() && beats_enabled->is_pressed()) ? int(beats_edit->get_value()) : int(0);
	params["bar_beats"] = (bpm_enabled->is_pressed()) ? int(bar_beats_edit->get_value()) : int(4);

	EditorFileSystem::get_singleton()->reimport_file_with_custom_parameters(path, importer, params);
}

AudioStreamImportSettingsDialog::AudioStreamImportSettingsDialog() {
	get_ok_button()->set_text(TTR("Reimport"));
	get_cancel_button()->set_text(TTR("Close"));

	VBoxContainer *main_vbox = memnew(VBoxContainer);
	add_child(main_vbox);

	HBoxContainer *loop_hb = memnew(HBoxContainer);
	loop_hb->add_theme_constant_override("separation", 4 * EDSCALE);
	loop = memnew(CheckBox);
	loop->set_text(TTR("Enable"));
	loop->set_tooltip_text(TTR("Enable looping."));
	loop->connect(SceneStringName(toggled), callable_mp(this, &AudioStreamImportSettingsDialog::_settings_changed).unbind(1));
	loop_hb->add_child(loop);
	loop_hb->add_spacer();
	loop_hb->add_child(memnew(Label(TTR("Offset:"))));
	loop_offset = memnew(SpinBox);
	loop_offset->set_max(10000);
	loop_offset->set_step(0.001);
	loop_offset->set_suffix("sec");
	loop_offset->set_tooltip_text(TTR("Loop offset (from beginning). Note that if BPM is set, this setting will be ignored."));
	loop_offset->connect(SceneStringName(value_changed), callable_mp(this, &AudioStreamImportSettingsDialog::_settings_changed).unbind(1));
	loop_hb->add_child(loop_offset);
	main_vbox->add_margin_child(TTR("Loop:"), loop_hb);

	HBoxContainer *interactive_hb = memnew(HBoxContainer);
	interactive_hb->add_theme_constant_override("separation", 4 * EDSCALE);
	bpm_enabled = memnew(CheckBox);
	bpm_enabled->set_text((TTR("BPM:")));
	bpm_enabled->connect(SceneStringName(toggled), callable_mp(this, &AudioStreamImportSettingsDialog::_settings_changed).unbind(1));
	interactive_hb->add_child(bpm_enabled);
	bpm_edit = memnew(SpinBox);
	bpm_edit->set_max(400);
	bpm_edit->set_step(0.01);
	bpm_edit->set_tooltip_text(TTR("Configure the Beats Per Measure (tempo) used for the interactive streams.\nThis is required in order to configure beat information."));
	bpm_edit->connect(SceneStringName(value_changed), callable_mp(this, &AudioStreamImportSettingsDialog::_settings_changed).unbind(1));
	interactive_hb->add_child(bpm_edit);
	interactive_hb->add_spacer();
	beats_enabled = memnew(CheckBox);
	beats_enabled->set_text(TTR("Beat Count:"));
	beats_enabled->connect(SceneStringName(toggled), callable_mp(this, &AudioStreamImportSettingsDialog::_settings_changed).unbind(1));
	interactive_hb->add_child(beats_enabled);
	beats_edit = memnew(SpinBox);
	beats_edit->set_tooltip_text(TTR("Configure the amount of Beats used for music-aware looping. If zero, it will be autodetected from the length.\nIt is recommended to set this value (either manually or by clicking on a beat number in the preview) to ensure looping works properly."));
	beats_edit->set_max(99999);
	beats_edit->connect(SceneStringName(value_changed), callable_mp(this, &AudioStreamImportSettingsDialog::_settings_changed).unbind(1));
	interactive_hb->add_child(beats_edit);
	bar_beats_label = memnew(Label(TTR("Bar Beats:")));
	interactive_hb->add_child(bar_beats_label);
	bar_beats_edit = memnew(SpinBox);
	bar_beats_edit->set_tooltip_text(TTR("Configure the Beats Per Bar. This used for music-aware transitions between AudioStreams."));
	bar_beats_edit->set_min(2);
	bar_beats_edit->set_max(32);
	bar_beats_edit->connect(SceneStringName(value_changed), callable_mp(this, &AudioStreamImportSettingsDialog::_settings_changed).unbind(1));
	interactive_hb->add_child(bar_beats_edit);
	main_vbox->add_margin_child(TTR("Music Playback:"), interactive_hb);

	color_rect = memnew(ColorRect);
	main_vbox->add_margin_child(TTR("Preview:"), color_rect, true);
	color_rect->set_custom_minimum_size(Size2(600, 200) * EDSCALE);
	color_rect->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	_player = memnew(AudioStreamPlayer);
	_player->connect(SceneStringName(finished), callable_mp(this, &AudioStreamImportSettingsDialog::_on_finished));
	color_rect->add_child(_player);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, 0);
	color_rect->add_child(vbox);
	vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	_preview = memnew(ColorRect);
	_preview->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_preview->connect(SceneStringName(draw), callable_mp(this, &AudioStreamImportSettingsDialog::_draw_preview));
	_preview->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->add_child(_preview);

	HBoxContainer *zoom_hbox = memnew(HBoxContainer);
	zoom_bar = memnew(HScrollBar);
	zoom_in = memnew(Button);
	zoom_in->set_flat(true);
	zoom_reset = memnew(Button);
	zoom_reset->set_flat(true);
	zoom_out = memnew(Button);
	zoom_out->set_flat(true);
	zoom_hbox->add_child(zoom_bar);
	zoom_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	zoom_bar->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	zoom_hbox->add_child(zoom_out);
	zoom_hbox->add_child(zoom_reset);
	zoom_hbox->add_child(zoom_in);
	zoom_in->connect(SceneStringName(pressed), callable_mp(this, &AudioStreamImportSettingsDialog::_preview_zoom_in));
	zoom_reset->connect(SceneStringName(pressed), callable_mp(this, &AudioStreamImportSettingsDialog::_preview_zoom_reset));
	zoom_out->connect(SceneStringName(pressed), callable_mp(this, &AudioStreamImportSettingsDialog::_preview_zoom_out));
	zoom_bar->connect(SceneStringName(value_changed), callable_mp(this, &AudioStreamImportSettingsDialog::_preview_zoom_offset_changed));
	vbox->add_child(zoom_hbox);

	_indicator = memnew(Control);
	_indicator->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	_indicator->connect(SceneStringName(draw), callable_mp(this, &AudioStreamImportSettingsDialog::_draw_indicator));
	_indicator->connect(SceneStringName(gui_input), callable_mp(this, &AudioStreamImportSettingsDialog::_on_input_indicator));
	_indicator->connect(SceneStringName(mouse_exited), callable_mp(this, &AudioStreamImportSettingsDialog::_on_indicator_mouse_exited));
	_preview->add_child(_indicator);

	HBoxContainer *hbox = memnew(HBoxContainer);
	hbox->add_theme_constant_override("separation", 0);
	vbox->add_child(hbox);

	_play_button = memnew(Button);
	_play_button->set_flat(true);
	hbox->add_child(_play_button);
	_play_button->set_focus_mode(Control::FOCUS_NONE);
	_play_button->connect(SceneStringName(pressed), callable_mp(this, &AudioStreamImportSettingsDialog::_play));

	_stop_button = memnew(Button);
	_stop_button->set_flat(true);
	hbox->add_child(_stop_button);
	_stop_button->set_focus_mode(Control::FOCUS_NONE);
	_stop_button->connect(SceneStringName(pressed), callable_mp(this, &AudioStreamImportSettingsDialog::_stop));

	_current_label = memnew(Label);
	_current_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	_current_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_current_label->set_modulate(Color(1, 1, 1, 0.5));
	hbox->add_child(_current_label);

	_duration_label = memnew(Label);
	hbox->add_child(_duration_label);

	singleton = this;
}
