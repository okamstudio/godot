/**************************************************************************/
/*  tts_android.cpp                                                       */
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

#include "tts_android.h"

#include "java_godot_wrapper.h"
#include "os_android.h"
#include "thread_jandroid.h"

bool TTS_Android::initialized = false;
jobject TTS_Android::tts = nullptr;
jclass TTS_Android::cls = nullptr;

jmethodID TTS_Android::_init = nullptr;
jmethodID TTS_Android::_is_speaking = nullptr;
jmethodID TTS_Android::_is_paused = nullptr;
jmethodID TTS_Android::_get_voices = nullptr;
jmethodID TTS_Android::_speak = nullptr;
jmethodID TTS_Android::_pause_speaking = nullptr;
jmethodID TTS_Android::_resume_speaking = nullptr;
jmethodID TTS_Android::_stop_speaking = nullptr;

HashMap<int, Char16String> TTS_Android::ids;

void TTS_Android::setup(jobject p_tts) {
	bool tts_enabled = GLOBAL_GET("audio/general/text_to_speech");
	if (tts_enabled) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);

		tts = env->NewGlobalRef(p_tts);

		jclass c = env->GetObjectClass(tts);
		cls = (jclass)env->NewGlobalRef(c);

		_init = env->GetMethodID(cls, "init", "()V");
		_is_speaking = env->GetMethodID(cls, "isSpeaking", "()Z");
		_is_paused = env->GetMethodID(cls, "isPaused", "()Z");
		_get_voices = env->GetMethodID(cls, "getVoices", "()[Ljava/lang/String;");
		_speak = env->GetMethodID(cls, "speak", "(Ljava/lang/String;Ljava/lang/String;IFFIZ)V");
		_pause_speaking = env->GetMethodID(cls, "pauseSpeaking", "()V");
		_resume_speaking = env->GetMethodID(cls, "resumeSpeaking", "()V");
		_stop_speaking = env->GetMethodID(cls, "stopSpeaking", "()V");

		if (_init) {
			env->CallVoidMethod(tts, _init);
			initialized = true;
		}
	}
}

void TTS_Android::terminate() {
	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL(env);

	env->DeleteGlobalRef(cls);
	env->DeleteGlobalRef(tts);
}

void TTS_Android::_java_utterance_callback(int p_event, int p_id, int p_pos) {
	ERR_FAIL_COND_MSG(!initialized, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	if (ids.has(p_id)) {
		int pos = 0;
		if ((DisplayServer::TTSUtteranceEvent)p_event == DisplayServer::TTS_UTTERANCE_BOUNDARY) {
			// Convert position from UTF-16 to UTF-32.
			const Char16String &string = ids[p_id];
			for (int i = 0; i < MIN(p_pos, string.length()); i++) {
				char16_t c = string[i];
				if ((c & 0xfffffc00) == 0xd800) {
					i++;
				}
				pos++;
			}
		} else if ((DisplayServer::TTSUtteranceEvent)p_event != DisplayServer::TTS_UTTERANCE_STARTED) {
			ids.erase(p_id);
		}
		DisplayServer::get_singleton()->tts_post_utterance_event((DisplayServer::TTSUtteranceEvent)p_event, p_id, pos);
	}
}

bool TTS_Android::is_speaking() {
	ERR_FAIL_COND_V_MSG(!initialized, false, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	if (_is_speaking) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_NULL_V(env, false);
		return env->CallBooleanMethod(tts, _is_speaking);
	} else {
		return false;
	}
}

bool TTS_Android::is_paused() {
	ERR_FAIL_COND_V_MSG(!initialized, false, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	if (_is_paused) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_NULL_V(env, false);
		return env->CallBooleanMethod(tts, _is_paused);
	} else {
		return false;
	}
}

Array TTS_Android::get_voices() {
	ERR_FAIL_COND_V_MSG(!initialized, Array(), "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	Array list;
	if (_get_voices) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, list);

		jobject voices_object = env->CallObjectMethod(tts, _get_voices);
		jobjectArray *arr = reinterpret_cast<jobjectArray *>(&voices_object);

		jsize len = env->GetArrayLength(*arr);
		for (int i = 0; i < len; i++) {
			jstring jStr = (jstring)env->GetObjectArrayElement(*arr, i);
			String str = jstring_to_string(jStr, env);
			Vector<String> tokens = str.split(";", true, 2);
			if (tokens.size() == 2) {
				Dictionary voice_d;
				voice_d["name"] = tokens[1];
				voice_d["id"] = tokens[1];
				voice_d["language"] = tokens[0];
				list.push_back(voice_d);
			}
			env->DeleteLocalRef(jStr);
		}
	}
	return list;
}

void TTS_Android::speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt) {
	ERR_FAIL_COND_MSG(!initialized, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	if (p_interrupt) {
		stop();
	}

	if (p_text.is_empty()) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, p_utterance_id);
		return;
	}

	ids[p_utterance_id] = p_text.utf16();

	if (_speak) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);

		jstring jStrT = env->NewStringUTF(p_text.utf8().get_data());
		jstring jStrV = env->NewStringUTF(p_voice.utf8().get_data());
		env->CallVoidMethod(tts, _speak, jStrT, jStrV, CLAMP(p_volume, 0, 100), CLAMP(p_pitch, 0.f, 2.f), CLAMP(p_rate, 0.1f, 10.f), p_utterance_id, p_interrupt);
		env->DeleteLocalRef(jStrT);
		env->DeleteLocalRef(jStrV);
	}
}

void TTS_Android::pause() {
	ERR_FAIL_COND_MSG(!initialized, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	if (_pause_speaking) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_NULL(env);
		env->CallVoidMethod(tts, _pause_speaking);
	}
}

void TTS_Android::resume() {
	ERR_FAIL_COND_MSG(!initialized, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	if (_resume_speaking) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_NULL(env);
		env->CallVoidMethod(tts, _resume_speaking);
	}
}

void TTS_Android::stop() {
	ERR_FAIL_COND_MSG(!initialized, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	for (const KeyValue<int, Char16String> &E : ids) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, E.key);
	}
	ids.clear();

	if (_stop_speaking) {
		JNIEnv *env = get_jni_env();

		ERR_FAIL_NULL(env);
		env->CallVoidMethod(tts, _stop_speaking);
	}
}
