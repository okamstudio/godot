/**************************************************************************/
/*  tts_linux.h                                                           */
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

#include "core/os/thread.h"
#include "core/os/thread_safe.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/variant/array.h"
#include "servers/display_server.h"

#ifdef SOWRAP_ENABLED
#include "speechd-so_wrap.h"
#else
#include <libspeechd.h>
#endif

class TTS_Linux : public Object {
	_THREAD_SAFE_CLASS_

	List<DisplayServer::TTSUtterance> queue;
	SPDConnection *synth = nullptr;
	bool speaking = false;
	bool paused = false;
	int last_msg_id = -1;
	HashMap<int, int> ids;

	struct VoiceInfo {
		String language;
		String variant;
	};
	mutable bool voices_loaded = false;
	mutable HashMap<String, VoiceInfo> voices;

	Thread init_thread;

	static void speech_init_thread_func(void *p_userdata);
	static void speech_event_callback(size_t p_msg_id, size_t p_client_id, SPDNotificationType p_type);
	static void speech_event_index_mark(size_t p_msg_id, size_t p_client_id, SPDNotificationType p_type, char *p_index_mark);

	static TTS_Linux *singleton;

protected:
	void _load_voices() const;
	void _speech_event(int p_msg_id, int p_type);
	void _speech_index_mark(int p_msg_id, int p_type, const String &p_index_mark);

public:
	static TTS_Linux *get_singleton();

	bool is_speaking() const;
	bool is_paused() const;
	Array get_voices() const;

	void speak(const String &p_text, const String &p_voice, int p_volume = 50, float p_pitch = 1.f, float p_rate = 1.f, int p_utterance_id = 0, bool p_interrupt = false);
	void pause();
	void resume();
	void stop();

	TTS_Linux();
	~TTS_Linux();
};
