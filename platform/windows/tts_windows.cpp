/**************************************************************************/
/*  tts_windows.cpp                                                       */
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

#include "tts_windows.h"

TTS_Windows *TTS_Windows::singleton = nullptr;

void __stdcall TTS_Windows::speech_event_callback(WPARAM wParam, LPARAM lParam) {
	TTS_Windows *tts = TTS_Windows::get_singleton();
	SPEVENT event;
	while (tts->synth->GetEvents(1, &event, nullptr) == S_OK) {
		uint32_t stream_num = (uint32_t)event.ulStreamNum;
		if (tts->ids.has(stream_num)) {
			if (event.eEventId == SPEI_START_INPUT_STREAM) {
				DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_STARTED, tts->ids[stream_num].id);
			} else if (event.eEventId == SPEI_END_INPUT_STREAM) {
				DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_ENDED, tts->ids[stream_num].id);
				tts->ids.erase(stream_num);
				tts->update_requested = true;
			} else if (event.eEventId == SPEI_WORD_BOUNDARY) {
				const Char16String &string = tts->ids[stream_num].string;
				int pos = 0;
				for (int i = 0; i < MIN(event.lParam, string.length()); i++) {
					char16_t c = string[i];
					if ((c & 0xfffffc00) == 0xd800) {
						i++;
					}
					pos++;
				}
				DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_BOUNDARY, tts->ids[stream_num].id, pos - tts->ids[stream_num].offset);
			}
		}
	}
}

void TTS_Windows::process_events() {
	if (update_requested && !paused && queue.size() > 0 && !is_speaking()) {
		DisplayServer::TTSUtterance &message = queue.front()->get();

		String text;
		DWORD flags = SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_XML;
		String pitch_tag = String("<pitch absmiddle=\"") + String::num_int64(message.pitch * 10 - 10, 10) + String("\">");
		text = pitch_tag + message.text + String("</pitch>");

		IEnumSpObjectTokens *cpEnum;
		ISpObjectToken *cpVoiceToken;
		ULONG ulCount = 0;
		ULONG stream_number = 0;
		ISpObjectTokenCategory *cpCategory;
		HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_INPROC_SERVER, IID_ISpObjectTokenCategory, (void **)&cpCategory);
		if (SUCCEEDED(hr)) {
			hr = cpCategory->SetId(SPCAT_VOICES, false);
			if (SUCCEEDED(hr)) {
				hr = cpCategory->EnumTokens(nullptr, nullptr, &cpEnum);
				if (SUCCEEDED(hr)) {
					hr = cpEnum->GetCount(&ulCount);
					while (SUCCEEDED(hr) && ulCount--) {
						wchar_t *w_id = nullptr;
						hr = cpEnum->Next(1, &cpVoiceToken, nullptr);
						cpVoiceToken->GetId(&w_id);
						if (String::utf16((const char16_t *)w_id) == message.voice) {
							synth->SetVoice(cpVoiceToken);
							cpVoiceToken->Release();
							break;
						}
						cpVoiceToken->Release();
					}
					cpEnum->Release();
				}
			}
			cpCategory->Release();
		}

		UTData ut;
		ut.string = text.utf16();
		ut.offset = pitch_tag.length(); // Subtract injected <pitch> tag offset.
		ut.id = message.id;

		synth->SetVolume(message.volume);
		synth->SetRate(10.f * log10(message.rate) / log10(3.f));
		synth->Speak((LPCWSTR)ut.string.get_data(), flags, &stream_number);

		ids[(uint32_t)stream_number] = ut;

		queue.pop_front();

		update_requested = false;
	}
}

bool TTS_Windows::is_speaking() const {
	ERR_FAIL_NULL_V(synth, false);

	SPVOICESTATUS status;
	synth->GetStatus(&status, nullptr);
	return (status.dwRunningState == SPRS_IS_SPEAKING || status.dwRunningState == 0 /* Waiting To Speak */);
}

bool TTS_Windows::is_paused() const {
	ERR_FAIL_NULL_V(synth, false);
	return paused;
}

Array TTS_Windows::get_voices() const {
	Array list;
	IEnumSpObjectTokens *cpEnum;
	ISpObjectToken *cpVoiceToken;
	ISpDataKey *cpDataKeyAttribs;
	ULONG ulCount = 0;
	ISpObjectTokenCategory *cpCategory;
	HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_INPROC_SERVER, IID_ISpObjectTokenCategory, (void **)&cpCategory);
	if (SUCCEEDED(hr)) {
		hr = cpCategory->SetId(SPCAT_VOICES, false);
		if (SUCCEEDED(hr)) {
			hr = cpCategory->EnumTokens(nullptr, nullptr, &cpEnum);
			if (SUCCEEDED(hr)) {
				hr = cpEnum->GetCount(&ulCount);
				while (SUCCEEDED(hr) && ulCount--) {
					hr = cpEnum->Next(1, &cpVoiceToken, nullptr);
					HRESULT hr_attr = cpVoiceToken->OpenKey(SPTOKENKEY_ATTRIBUTES, &cpDataKeyAttribs);
					if (SUCCEEDED(hr_attr)) {
						wchar_t *w_id = nullptr;
						wchar_t *w_lang = nullptr;
						wchar_t *w_name = nullptr;
						cpVoiceToken->GetId(&w_id);
						cpDataKeyAttribs->GetStringValue(L"Language", &w_lang);
						cpDataKeyAttribs->GetStringValue(nullptr, &w_name);
						LCID locale = wcstol(w_lang, nullptr, 16);

						int locale_chars = GetLocaleInfoW(locale, LOCALE_SISO639LANGNAME, nullptr, 0);
						int region_chars = GetLocaleInfoW(locale, LOCALE_SISO3166CTRYNAME, nullptr, 0);
						wchar_t *w_lang_code = new wchar_t[locale_chars];
						wchar_t *w_reg_code = new wchar_t[region_chars];
						GetLocaleInfoW(locale, LOCALE_SISO639LANGNAME, w_lang_code, locale_chars);
						GetLocaleInfoW(locale, LOCALE_SISO3166CTRYNAME, w_reg_code, region_chars);

						Dictionary voice_d;
						voice_d["id"] = String::utf16((const char16_t *)w_id);
						if (w_name) {
							voice_d["name"] = String::utf16((const char16_t *)w_name);
						} else {
							voice_d["name"] = voice_d["id"].operator String().replace("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech\\Voices\\Tokens\\", "");
						}
						voice_d["language"] = String::utf16((const char16_t *)w_lang_code) + "_" + String::utf16((const char16_t *)w_reg_code);
						list.push_back(voice_d);

						delete[] w_lang_code;
						delete[] w_reg_code;

						cpDataKeyAttribs->Release();
					}
					cpVoiceToken->Release();
				}
				cpEnum->Release();
			}
		}
		cpCategory->Release();
	}
	return list;
}

void TTS_Windows::speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt) {
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
		update_requested = true;
	}
}

void TTS_Windows::pause() {
	ERR_FAIL_NULL(synth);
	if (!paused) {
		if (synth->Pause() == S_OK) {
			paused = true;
		}
	}
}

void TTS_Windows::resume() {
	ERR_FAIL_NULL(synth);
	synth->Resume();
	paused = false;
}

void TTS_Windows::stop() {
	ERR_FAIL_NULL(synth);

	SPVOICESTATUS status;
	synth->GetStatus(&status, nullptr);
	uint32_t current_stream = (uint32_t)status.ulCurrentStream;
	if (ids.has(current_stream)) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, ids[current_stream].id);
		ids.erase(current_stream);
	}
	for (DisplayServer::TTSUtterance &message : queue) {
		DisplayServer::get_singleton()->tts_post_utterance_event(DisplayServer::TTS_UTTERANCE_CANCELED, message.id);
	}
	queue.clear();
	synth->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
	synth->Resume();
	paused = false;
}

TTS_Windows *TTS_Windows::get_singleton() {
	return singleton;
}

TTS_Windows::TTS_Windows() {
	singleton = this;

	if (SUCCEEDED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void **)&synth))) {
		ULONGLONG event_mask = SPFEI(SPEI_END_INPUT_STREAM) | SPFEI(SPEI_START_INPUT_STREAM) | SPFEI(SPEI_WORD_BOUNDARY);
		synth->SetInterest(event_mask, event_mask);
		synth->SetNotifyCallbackFunction(&speech_event_callback, (WPARAM)(this), 0);
		print_verbose("Text-to-Speech: SAPI initialized.");
	} else {
		print_verbose("Text-to-Speech: Cannot initialize ISpVoice!");
	}
}

TTS_Windows::~TTS_Windows() {
	if (synth) {
		synth->Release();
	}
	singleton = nullptr;
}
