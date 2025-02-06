/**************************************************************************/
/*  audio_stream_editor_plugin.h                                          */
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

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"

class AudioStreamEditor : public ColorRect {
	GDCLASS(AudioStreamEditor, ColorRect);

	Ref<AudioStream> stream;

	AudioStreamPlayer *_player = nullptr;
	ColorRect *_preview = nullptr;
	Control *_indicator = nullptr;
	Label *_current_label = nullptr;
	Label *_duration_label = nullptr;

	Button *_play_button = nullptr;
	Button *_stop_button = nullptr;

	float _current = 0;
	bool _dragging = false;
	bool _pausing = false;

protected:
	void _notification(int p_what);
	void _preview_changed(ObjectID p_which);
	void _play();
	void _stop();
	void _on_finished();
	void _draw_preview();
	void _draw_indicator();
	void _on_input_indicator(Ref<InputEvent> p_event);
	void _seek_to(real_t p_x);
	void _stream_changed();

public:
	void set_stream(const Ref<AudioStream> &p_stream);

	AudioStreamEditor();
};

class EditorInspectorPluginAudioStream : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginAudioStream, EditorInspectorPlugin);
	AudioStreamEditor *editor = nullptr;

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class AudioStreamEditorPlugin : public EditorPlugin {
	GDCLASS(AudioStreamEditorPlugin, EditorPlugin);

public:
	AudioStreamEditorPlugin();
};
