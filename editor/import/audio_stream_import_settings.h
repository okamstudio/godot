/**************************************************************************/
/*  audio_stream_import_settings.h                                        */
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

#pragma once

#include "editor/plugins/editor_plugin.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/spin_box.h"
#include "scene/resources/texture.h"

class CheckBox;

class AudioStreamImportSettingsDialog : public ConfirmationDialog {
	GDCLASS(AudioStreamImportSettingsDialog, ConfirmationDialog);

	CheckBox *bpm_enabled = nullptr;
	SpinBox *bpm_edit = nullptr;
	CheckBox *beats_enabled = nullptr;
	SpinBox *beats_edit = nullptr;
	Label *bar_beats_label = nullptr;
	SpinBox *bar_beats_edit = nullptr;
	CheckBox *loop = nullptr;
	SpinBox *loop_offset = nullptr;
	ColorRect *color_rect = nullptr;
	Ref<AudioStream> stream;
	AudioStreamPlayer *_player = nullptr;
	ColorRect *_preview = nullptr;
	Control *_indicator = nullptr;
	Label *_current_label = nullptr;
	Label *_duration_label = nullptr;

	HScrollBar *zoom_bar = nullptr;
	Button *zoom_in = nullptr;
	Button *zoom_reset = nullptr;
	Button *zoom_out = nullptr;

	Button *_play_button = nullptr;
	Button *_stop_button = nullptr;

	bool updating_settings = false;

	float _current = 0;
	bool _dragging = false;
	bool _beat_len_dragging = false;
	bool _pausing = false;
	int _hovering_beat = -1;

	HashMap<StringName, Variant> params;
	String importer;
	String path;

	void _audio_changed();

	static AudioStreamImportSettingsDialog *singleton;

	void _settings_changed();

	void _reimport();

protected:
	void _notification(int p_what);
	void _preview_changed(ObjectID p_which);
	void _preview_zoom_in();
	void _preview_zoom_out();
	void _preview_zoom_reset();
	void _preview_zoom_offset_changed(double);

	void _play();
	void _stop();
	void _on_finished();
	void _draw_preview();
	void _draw_indicator();
	void _on_input_indicator(Ref<InputEvent> p_event);
	void _seek_to(real_t p_x);
	void _set_beat_len_to(real_t p_x);
	void _on_indicator_mouse_exited();
	int _get_beat_at_pos(real_t p_x);

public:
	void edit(const String &p_path, const String &p_importer, const Ref<AudioStream> &p_stream);

	static AudioStreamImportSettingsDialog *get_singleton() { return singleton; }

	AudioStreamImportSettingsDialog();
};
