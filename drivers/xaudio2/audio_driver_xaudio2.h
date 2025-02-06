/**************************************************************************/
/*  audio_driver_xaudio2.h                                                */
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

#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"
#include "servers/audio_server.h"

#include <mmsystem.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl/client.h>
#include <xaudio2.h>

class AudioDriverXAudio2 : public AudioDriver {
	enum {
		AUDIO_BUFFERS = 2
	};

	struct XAudio2DriverVoiceCallback : public IXAudio2VoiceCallback {
		HANDLE buffer_end_event;
		XAudio2DriverVoiceCallback() :
				buffer_end_event(CreateEvent(nullptr, FALSE, FALSE, nullptr)) {}
		void STDMETHODCALLTYPE OnBufferEnd(void *pBufferContext) {
			SetEvent(buffer_end_event);
		}

		//Unused methods are stubs
		void STDMETHODCALLTYPE OnStreamEnd() {}
		void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() {}
		void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32 SamplesRequired) {}
		void STDMETHODCALLTYPE OnBufferStart(void *pBufferContext) {}
		void STDMETHODCALLTYPE OnLoopEnd(void *pBufferContext) {}
		void STDMETHODCALLTYPE OnVoiceError(void *pBufferContext, HRESULT Error) {}
	};

	Thread thread;
	Mutex mutex;

	int32_t *samples_in = nullptr;
	int16_t *samples_out[AUDIO_BUFFERS];

	static void thread_func(void *p_udata);
	int buffer_size = 0;

	unsigned int mix_rate = 0;
	SpeakerMode speaker_mode = SpeakerMode::SPEAKER_MODE_STEREO;

	int channels = 0;

	SafeFlag active;
	SafeFlag exit_thread;
	bool pcm_open = false;

	WAVEFORMATEX wave_format = { 0 };
	Microsoft::WRL::ComPtr<IXAudio2> xaudio;
	int current_buffer = 0;
	IXAudio2MasteringVoice *mastering_voice = nullptr;
	XAUDIO2_BUFFER xaudio_buffer[AUDIO_BUFFERS];
	IXAudio2SourceVoice *source_voice = nullptr;
	XAudio2DriverVoiceCallback voice_callback;

public:
	virtual const char *get_name() const override {
		return "XAudio2";
	}

	virtual Error init() override;
	virtual void start() override;
	virtual int get_mix_rate() const override;
	virtual SpeakerMode get_speaker_mode() const override;
	virtual float get_latency() override;

	virtual void lock() override;
	virtual void unlock() override;
	virtual void finish() override;

	AudioDriverXAudio2();
	~AudioDriverXAudio2() {}
};
