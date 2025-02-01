/**************************************************************************/
/*  tts_android.h                                                         */
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

#include "core/config/project_settings.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "servers/display_server.h"

#include <jni.h>

class TTS_Android {
	static bool initialized;
	static jobject tts;
	static jclass cls;

	static jmethodID _init;
	static jmethodID _is_speaking;
	static jmethodID _is_paused;
	static jmethodID _get_voices;
	static jmethodID _speak;
	static jmethodID _pause_speaking;
	static jmethodID _resume_speaking;
	static jmethodID _stop_speaking;

	static HashMap<int, Char16String> ids;

public:
	static void setup(jobject p_tts);
	static void terminate();
	static void _java_utterance_callback(int p_event, int p_id, int p_pos);

	static bool is_speaking();
	static bool is_paused();
	static Array get_voices();
	static void speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt);
	static void pause();
	static void resume();
	static void stop();
};
