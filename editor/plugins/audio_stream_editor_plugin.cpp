/**************************************************************************/
/*  audio_stream_editor_plugin.cpp                                        */
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

#include "audio_stream_editor_plugin.h"

#include "editor/audio_stream_preview.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/audio_stream_wav.h"

// AudioStreamEditor

void AudioStreamEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			AudioStreamPreviewGenerator::get_singleton()->connect(SNAME("preview_updated"), callable_mp(this, &AudioStreamEditor::_preview_changed));
		} break;
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_ENTER_TREE: {
			Ref<Font> font = get_theme_font(SNAME("status_source"), EditorStringName(EditorFonts));

			_current_label->add_theme_font_override(SceneStringName(font), font);
			_duration_label->add_theme_font_override(SceneStringName(font), font);

			_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
			_stop_button->set_button_icon(get_editor_theme_icon(SNAME("Stop")));
			_preview->set_color(get_theme_color(SNAME("dark_color_2"), EditorStringName(Editor)));

			set_color(get_theme_color(SNAME("dark_color_1"), EditorStringName(Editor)));

			_indicator->queue_redraw();
			_preview->queue_redraw();
		} break;
		case NOTIFICATION_PROCESS: {
			_current = _player->get_playback_position();
			_indicator->queue_redraw();
		} break;
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible_in_tree()) {
				_stop();
			}
		} break;
		default: {
		} break;
	}
}

void AudioStreamEditor::_draw_preview() {
	Size2 size = get_size();
	int width = size.width;
	if (width <= 0) {
		return; // No points to draw.
	}

	Rect2 rect = _preview->get_rect();

	Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(stream);
	float preview_len = preview->get_length();

	Vector<Vector2> points;
	points.resize(width * 2);

	for (int i = 0; i < width; i++) {
		float ofs = i * preview_len / size.width;
		float ofs_n = (i + 1) * preview_len / size.width;
		float max = preview->get_max(ofs, ofs_n) * 0.5 + 0.5;
		float min = preview->get_min(ofs, ofs_n) * 0.5 + 0.5;

		int idx = i;
		points.write[idx * 2 + 0] = Vector2(i + 1, rect.position.y + min * rect.size.y);
		points.write[idx * 2 + 1] = Vector2(i + 1, rect.position.y + max * rect.size.y);
	}

	Vector<Color> colors = { get_theme_color(SNAME("contrast_color_2"), EditorStringName(Editor)) };

	RS::get_singleton()->canvas_item_add_multiline(_preview->get_canvas_item(), points, colors);
}

void AudioStreamEditor::_preview_changed(ObjectID p_which) {
	if (stream.is_valid() && stream->get_instance_id() == p_which) {
		_preview->queue_redraw();
	}
}

void AudioStreamEditor::_stream_changed() {
	if (!is_visible()) {
		return;
	}
	queue_redraw();
}

void AudioStreamEditor::_play() {
	if (_player->is_playing()) {
		_pausing = true;
		_player->stop();
		_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
		set_process(false);
	} else {
		_pausing = false;
		_player->play(_current);
		_play_button->set_button_icon(get_editor_theme_icon(SNAME("Pause")));
		set_process(true);
	}
}

void AudioStreamEditor::_stop() {
	_player->stop();
	_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
	_current = 0;
	_indicator->queue_redraw();
	set_process(false);
}

void AudioStreamEditor::_on_finished() {
	_play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
	if (!_pausing) {
		_current = 0;
		_indicator->queue_redraw();
	} else {
		_pausing = false;
	}
	set_process(false);
}

void AudioStreamEditor::_draw_indicator() {
	if (stream.is_null()) {
		return;
	}

	Rect2 rect = _preview->get_rect();
	float len = stream->get_length();
	float ofs_x = _current / len * rect.size.width;
	const Color col = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	Ref<Texture2D> icon = get_editor_theme_icon(SNAME("TimelineIndicator"));
	_indicator->draw_line(Point2(ofs_x, 0), Point2(ofs_x, rect.size.height), col, Math::round(2 * EDSCALE));
	_indicator->draw_texture(
			icon,
			Point2(ofs_x - icon->get_width() * 0.5, 0),
			col);

	_current_label->set_text(String::num(_current, 2).pad_decimals(2) + " /");
}

void AudioStreamEditor::_on_input_indicator(Ref<InputEvent> p_event) {
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
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
	}
}

void AudioStreamEditor::_seek_to(real_t p_x) {
	_current = p_x / _preview->get_rect().size.x * stream->get_length();
	_current = CLAMP(_current, 0, stream->get_length());
	_player->seek(_current);
	_indicator->queue_redraw();
}

void AudioStreamEditor::set_stream(const Ref<AudioStream> &p_stream) {
	if (stream.is_valid()) {
		stream->disconnect_changed(callable_mp(this, &AudioStreamEditor::_stream_changed));
	}

	stream = p_stream;
	if (stream.is_null()) {
		hide();
		return;
	}
	stream->connect_changed(callable_mp(this, &AudioStreamEditor::_stream_changed));

	_player->set_stream(stream);
	_current = 0;

	String text = String::num(stream->get_length(), 2).pad_decimals(2) + "s";
	_duration_label->set_text(text);

	queue_redraw();
}

AudioStreamEditor::AudioStreamEditor() {
	set_custom_minimum_size(Size2(1, 100) * EDSCALE);

	_player = memnew(AudioStreamPlayer);
	_player->connect(SceneStringName(finished), callable_mp(this, &AudioStreamEditor::_on_finished));
	add_child(_player);

	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	add_child(vbox);

	_preview = memnew(ColorRect);
	_preview->set_v_size_flags(SIZE_EXPAND_FILL);
	_preview->connect(SceneStringName(draw), callable_mp(this, &AudioStreamEditor::_draw_preview));
	vbox->add_child(_preview);

	_indicator = memnew(Control);
	_indicator->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	_indicator->connect(SceneStringName(draw), callable_mp(this, &AudioStreamEditor::_draw_indicator));
	_indicator->connect(SceneStringName(gui_input), callable_mp(this, &AudioStreamEditor::_on_input_indicator));
	_preview->add_child(_indicator);

	HBoxContainer *hbox = memnew(HBoxContainer);
	hbox->add_theme_constant_override("separation", 0);
	vbox->add_child(hbox);

	_play_button = memnew(Button);
	hbox->add_child(_play_button);
	_play_button->set_flat(true);
	_play_button->set_focus_mode(Control::FOCUS_NONE);
	_play_button->connect(SceneStringName(pressed), callable_mp(this, &AudioStreamEditor::_play));
	_play_button->set_shortcut(ED_SHORTCUT("audio_stream_editor/audio_preview_play_pause", TTRC("Audio Preview Play/Pause"), Key::SPACE));

	_stop_button = memnew(Button);
	hbox->add_child(_stop_button);
	_stop_button->set_flat(true);
	_stop_button->set_focus_mode(Control::FOCUS_NONE);
	_stop_button->connect(SceneStringName(pressed), callable_mp(this, &AudioStreamEditor::_stop));

	_current_label = memnew(Label);
	_current_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	_current_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_current_label->set_modulate(Color(1, 1, 1, 0.5));
	hbox->add_child(_current_label);

	_duration_label = memnew(Label);
	hbox->add_child(_duration_label);
}

// EditorInspectorPluginAudioStream

bool EditorInspectorPluginAudioStream::can_handle(Object *p_object) {
	return Object::cast_to<AudioStreamWAV>(p_object) != nullptr;
}

void EditorInspectorPluginAudioStream::parse_begin(Object *p_object) {
	AudioStream *stream = Object::cast_to<AudioStream>(p_object);

	editor = memnew(AudioStreamEditor);
	editor->set_stream(Ref<AudioStream>(stream));

	add_custom_control(editor);
}

// AudioStreamEditorPlugin

AudioStreamEditorPlugin::AudioStreamEditorPlugin() {
	Ref<EditorInspectorPluginAudioStream> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
