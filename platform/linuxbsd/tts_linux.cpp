/**************************************************************************/
/*  tts_linux.cpp                                                         */
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

#include "tts_linux.h"

#include "core/config/project_settings.h"
#include "servers/text_server.h"

TTS_Linux *TTS_Linux::singleton = nullptr;

void TTS_Linux::speech_init_thread_func(void *p_userdata) {
	TTS_Linux *tts = (TTS_Linux *)p_userdata;
	if (tts) {
		MutexLock thread_safe_method(tts->_thread_safe_);
#ifdef SOWRAP_ENABLED
#ifdef DEBUG_ENABLED
		int dylibloader_verbose = 1;
#else
		int dylibloader_verbose = 0;
#endif
		if (initialize_speechd(dylibloader_verbose) != 0) {
			print_verbose("Text-to-Speech: Cannot load Speech Dispatcher library!");
		} else {
			if (!spd_open || !spd_set_notification_on || !spd_list_synthesis_voices || !free_spd_voices || !spd_set_synthesis_voice || !spd_set_volume || !spd_set_voice_pitch || !spd_set_voice_rate || !spd_set_data_mode || !spd_say || !spd_pause || !spd_resume || !spd_cancel) {
				// There's no API to check version, check if functions are available instead.
				print_verbose("Text-to-Speech: Unsupported Speech Dispatcher library version!");
				return;
			}
#else
		{
#endif
			CharString class_str;
			String config_name = GLOBAL_GET("application/config/name");
			if (config_name.length() == 0) {
				class_str = "Godot_Engine";
			} else {
				class_str = config_name.utf8();
			}
			tts->synth = spd_open(class_str, "Godot_Engine_Speech_API", "Godot_Engine", SPD_MODE_THREADED);
			if (tts->synth) {
				tts->synth->callback_end = &speech_event_callback;
				tts->synth->callback_cancel = &speech_event_callback;
				tts->synth->callback_im = &speech_event_index_mark;
				spd_set_notification_on(tts->synth, SPD_END);
				spd_set_notification_on(tts->synth, SPD_CANCEL);

				print_verbose("Text-to-Speech: Speech Dispatcher initialized.");
			} else {
				print_verbose("Text-to-Speech: Cannot initialize Speech Dispatcher synthesizer!");
			}
		}
	}
}

void TTS_Linux::speech_event_index_mark(size_t p_msg_id, size_t p_client_id, SPDNotificationType p_type, char *p_index_mark) {
	TTS_Linux *tts = TTS_Linux::get_singleton();
	if (tts) {
		callable_mp(tts, &TTS_Linux::_speech_index_mark).call_deferred((int)p_msg_id, (int)p_type, String::utf8(p_index_mark));
	}
}

void TTS_Linux::_speech_index_mark(int p_msg_id, int p_type, const String &p_index_mark) {
	_THREAD_SAFE_METHOD_

	if (ids.has(p_msg_id)) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_BOUNDARY, ids[p_msg_id], p_index_mark.to_int());
	}
}

void TTS_Linux::speech_event_callback(size_t p_msg_id, size_t p_client_id, SPDNotificationType p_type) {
	TTS_Linux *tts = TTS_Linux::get_singleton();
	if (tts) {
		callable_mp(tts, &TTS_Linux::_speech_event).call_deferred((int)p_msg_id, (int)p_type);
	}
}

void TTS_Linux::_load_voices() const {
	if (!voices_loaded) {
		SPDVoice **spd_voices = spd_list_synthesis_voices(synth);
		if (spd_voices != nullptr) {
			SPDVoice **voices_ptr = spd_voices;
			while (*voices_ptr != nullptr) {
				VoiceInfo vi;
				vi.language = String::utf8((*voices_ptr)->language);
				vi.variant = String::utf8((*voices_ptr)->variant);
				voices[String::utf8((*voices_ptr)->name)] = vi;
				voices_ptr++;
			}
			free_spd_voices(spd_voices);
		}
		voices_loaded = true;
	}
}

void TTS_Linux::_speech_event(int p_msg_id, int p_type) {
	_THREAD_SAFE_METHOD_

	if (!paused && ids.has(p_msg_id)) {
		if ((SPDNotificationType)p_type == SPD_EVENT_END) {
			DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_ENDED, ids[p_msg_id]);
			ids.erase(p_msg_id);
			last_msg_id = -1;
			speaking = false;
		} else if ((SPDNotificationType)p_type == SPD_EVENT_CANCEL) {
			DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, ids[p_msg_id]);
			ids.erase(p_msg_id);
			last_msg_id = -1;
			speaking = false;
		}
	}
	if (!speaking && queue.size() > 0) {
		DisplayServer::TTSUtterance &message = queue.front()->get();

		// Inject index mark after each word.
		String text;
		String language;

		_load_voices();
		const VoiceInfo *voice = voices.getptr(message.voice);
		if (voice) {
			language = voice->language;
		}

		PackedInt32Array breaks = TS->string_get_word_breaks(message.text, language);
		int prev_end = -1;
		for (int i = 0; i < breaks.size(); i += 2) {
			const int start = breaks[i];
			const int end = breaks[i + 1];
			if (prev_end != -1 && prev_end != start) {
				text += message.text.substr(prev_end, start - prev_end);
			}
			text += message.text.substr(start, end - start);
			text += "<mark name=\"" + String::num_int64(end, 10) + "\"/>";
			prev_end = end;
		}

		spd_set_synthesis_voice(synth, message.voice.utf8().get_data());
		spd_set_volume(synth, message.volume * 2 - 100);
		spd_set_voice_pitch(synth, (message.pitch - 1) * 100);
		float rate = 0;
		if (message.rate > 1.f) {
			rate = log10(MIN(message.rate, 2.5f)) / log10(2.5f) * 100;
		} else if (message.rate < 1.f) {
			rate = log10(MAX(message.rate, 0.5f)) / log10(0.5f) * -100;
		}
		spd_set_voice_rate(synth, rate);
		spd_set_data_mode(synth, SPD_DATA_SSML);
		last_msg_id = spd_say(synth, SPD_TEXT, text.utf8().get_data());
		ids[last_msg_id] = message.id;
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_STARTED, message.id);

		queue.pop_front();
		speaking = true;
	}
}

bool TTS_Linux::is_speaking() const {
	return speaking;
}

bool TTS_Linux::is_paused() const {
	return paused;
}

Array TTS_Linux::get_voices() const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_NULL_V(synth, Array());
	_load_voices();

	Array list;
	for (const KeyValue<String, VoiceInfo> &E : voices) {
		Dictionary voice_d;
		voice_d["name"] = E.key;
		voice_d["id"] = E.key;
		voice_d["language"] = E.value.language + "_" + E.value.variant;
		list.push_back(voice_d);
	}

	return list;
}

void TTS_Linux::speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_NULL(synth);
	if (p_interrupt) {
		stop();
	}

	if (p_text.is_empty()) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, p_utterance_id);
		return;
	}

	DisplayServer::TTSUtterance message;
	message.text = p_text;
	message.voice = p_voice;
	message.volume = CLAMP(p_volume, 0, 100);
	message.pitch = CLAMP(p_pitch, 0.f, 2.f);
	message.rate = CLAMP(p_rate, 0.1f, 10.f);
	message.id = p_utterance_id;
	queue.push_back(message);

	if (is_paused()) {
		resume();
	} else {
		_speech_event(0, (int)SPD_EVENT_BEGIN);
	}
}

void TTS_Linux::pause() {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_NULL(synth);
	if (spd_pause(synth) == 0) {
		paused = true;
	}
}

void TTS_Linux::resume() {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_NULL(synth);
	spd_resume(synth);
	paused = false;
}

void TTS_Linux::stop() {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_NULL(synth);
	for (DisplayServer::TTSUtterance &message : queue) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, message.id);
	}
	if ((last_msg_id != -1) && ids.has(last_msg_id)) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, ids[last_msg_id]);
	}
	queue.clear();
	ids.clear();
	last_msg_id = -1;
	spd_cancel(synth);
	spd_resume(synth);
	speaking = false;
	paused = false;
}

TTS_Linux *TTS_Linux::get_singleton() {
	return singleton;
}

TTS_Linux::TTS_Linux() {
	singleton = this;
	// Speech Dispatcher init can be slow, it might wait for helper process to start on background, so run it in the thread.
	init_thread.start(speech_init_thread_func, this);
}

TTS_Linux::~TTS_Linux() {
	init_thread.wait_to_finish();
	if (synth) {
		spd_close(synth);
	}

	singleton = nullptr;
}
