/**************************************************************************/
/*  audio_driver_opensl.cpp                                               */
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

#include "audio_driver_opensl.h"

#include <string.h>

#define MAX_NUMBER_INTERFACES 3
#define MAX_NUMBER_OUTPUT_DEVICES 6

/* Structure for passing information to callback function */

void AudioDriverOpenSL::_buffer_callback(
		SLAndroidSimpleBufferQueueItf queueItf) {
	bool mix = true;

	if (pause) {
		mix = false;
	} else {
		mix = mutex.try_lock();
	}

	if (mix) {
		audio_server_process(buffer_size, mixdown_buffer);
	} else {
		int32_t *src_buff = mixdown_buffer;
		for (unsigned int i = 0; i < buffer_size * 2; i++) {
			src_buff[i] = 0;
		}
	}

	if (mix) {
		mutex.unlock();
	}

	const int32_t *src_buff = mixdown_buffer;

	int16_t *ptr = (int16_t *)buffers[last_free];
	last_free = (last_free + 1) % BUFFER_COUNT;

	for (unsigned int i = 0; i < buffer_size * 2; i++) {
		ptr[i] = src_buff[i] >> 16;
	}

	(*queueItf)->Enqueue(queueItf, ptr, 4 * buffer_size);
}

void AudioDriverOpenSL::_buffer_callbacks(
		SLAndroidSimpleBufferQueueItf queueItf,
		void *pContext) {
	AudioDriverOpenSL *ad = static_cast<AudioDriverOpenSL *>(pContext);

	ad->_buffer_callback(queueItf);
}

Error AudioDriverOpenSL::init() {
	SLresult res;
	SLEngineOption EngineOption[] = {
		{ (SLuint32)SL_ENGINEOPTION_THREADSAFE, (SLuint32)SL_BOOLEAN_TRUE }
	};
	res = slCreateEngine(&sl, 1, EngineOption, 0, nullptr, nullptr);
	ERR_FAIL_COND_V_MSG(res != SL_RESULT_SUCCESS, ERR_INVALID_PARAMETER, "Could not initialize OpenSL.");

	res = (*sl)->Realize(sl, SL_BOOLEAN_FALSE);
	ERR_FAIL_COND_V_MSG(res != SL_RESULT_SUCCESS, ERR_INVALID_PARAMETER, "Could not realize OpenSL.");

	return OK;
}

void AudioDriverOpenSL::start() {
	active = false;

	SLresult res;

	buffer_size = 1024;

	for (int i = 0; i < BUFFER_COUNT; i++) {
		buffers[i] = memnew_arr(int16_t, buffer_size * 2);
		memset(buffers[i], 0, buffer_size * 4);
	}

	mixdown_buffer = memnew_arr(int32_t, buffer_size * 2);

	/* Callback context for the buffer queue callback function */

	/* Get the SL Engine Interface which is implicit */
	res = (*sl)->GetInterface(sl, SL_IID_ENGINE, (void *)&EngineItf);

	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);

	{
		const SLInterfaceID ids[1] = { SL_IID_ENVIRONMENTALREVERB };
		const SLboolean req[1] = { SL_BOOLEAN_FALSE };
		res = (*EngineItf)->CreateOutputMix(EngineItf, &OutputMix, 0, ids, req);
	}

	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
	// Realizing the Output Mix object in synchronous mode.
	res = (*OutputMix)->Realize(OutputMix, SL_BOOLEAN_FALSE);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);

	SLDataLocator_AndroidSimpleBufferQueue loc_bufq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, BUFFER_COUNT };
	/* Setup the format of the content in the buffer queue */
	pcm.formatType = SL_DATAFORMAT_PCM;
	pcm.numChannels = 2;
	pcm.samplesPerSec = SL_SAMPLINGRATE_44_1;
	pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
	pcm.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
	pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
#ifdef BIG_ENDIAN_ENABLED
	pcm.endianness = SL_BYTEORDER_BIGENDIAN;
#else
	pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
#endif
	audioSource.pFormat = (void *)&pcm;
	audioSource.pLocator = (void *)&loc_bufq;

	/* Setup the data sink structure */
	locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
	locator_outputmix.outputMix = OutputMix;
	audioSink.pLocator = (void *)&locator_outputmix;
	audioSink.pFormat = nullptr;

	/* Create the music player */
	{
		const SLInterfaceID ids[2] = { SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND };
		const SLboolean req[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

		res = (*EngineItf)->CreateAudioPlayer(EngineItf, &player, &audioSource, &audioSink, 1, ids, req);
		ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
	}
	/* Realizing the player in synchronous mode. */
	res = (*player)->Realize(player, SL_BOOLEAN_FALSE);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
	/* Get seek and play interfaces */
	res = (*player)->GetInterface(player, SL_IID_PLAY, (void *)&playItf);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
	res = (*player)->GetInterface(player, SL_IID_BUFFERQUEUE,
			(void *)&bufferQueueItf);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
	/* Setup to receive buffer queue event callbacks */
	res = (*bufferQueueItf)->RegisterCallback(bufferQueueItf, _buffer_callbacks, this);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);

	last_free = 0;

	//fill up buffers
	for (int i = 0; i < BUFFER_COUNT; i++) {
		/* Enqueue a few buffers to get the ball rolling */
		res = (*bufferQueueItf)->Enqueue(bufferQueueItf, buffers[i], 4 * buffer_size); /* Size given in */
	}

	res = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);

	active = true;
}

void AudioDriverOpenSL::_record_buffer_callback(SLAndroidSimpleBufferQueueItf queueItf) {
	for (int i = 0; i < rec_buffer.size(); i++) {
		int32_t sample = rec_buffer[i] << 16;
		input_buffer_write(sample);
		input_buffer_write(sample); // call twice to convert to Stereo
	}

	SLresult res = (*recordBufferQueueItf)->Enqueue(recordBufferQueueItf, rec_buffer.ptrw(), rec_buffer.size() * sizeof(int16_t));
	ERR_FAIL_COND(res != SL_RESULT_SUCCESS);
}

void AudioDriverOpenSL::_record_buffer_callbacks(SLAndroidSimpleBufferQueueItf queueItf, void *pContext) {
	AudioDriverOpenSL *ad = static_cast<AudioDriverOpenSL *>(pContext);

	ad->_record_buffer_callback(queueItf);
}

Error AudioDriverOpenSL::init_input_device() {
	SLDataLocator_IODevice loc_dev = {
		SL_DATALOCATOR_IODEVICE,
		SL_IODEVICE_AUDIOINPUT,
		SL_DEFAULTDEVICEID_AUDIOINPUT,
		nullptr
	};
	SLDataSource recSource = { &loc_dev, nullptr };

	SLDataLocator_AndroidSimpleBufferQueue loc_bq = {
		SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
		1,
	};
	SLDataFormat_PCM format_pcm = {
		SL_DATAFORMAT_PCM,
		1,
		SL_SAMPLINGRATE_44_1,
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_PCMSAMPLEFORMAT_FIXED_16,
		SL_SPEAKER_FRONT_CENTER,
#ifdef BIG_ENDIAN_ENABLED
		SL_BYTEORDER_BIGENDIAN,
#else
		SL_BYTEORDER_LITTLEENDIAN,
#endif
	};
	SLDataSink recSnk = { &loc_bq, &format_pcm };

	const SLInterfaceID ids[2] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE, SL_IID_ANDROIDCONFIGURATION };
	const SLboolean req[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

	SLresult res = (*EngineItf)->CreateAudioRecorder(EngineItf, &recorder, &recSource, &recSnk, 2, ids, req);
	ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

	res = (*recorder)->Realize(recorder, SL_BOOLEAN_FALSE);
	ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

	res = (*recorder)->GetInterface(recorder, SL_IID_RECORD, (void *)&recordItf);
	ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

	res = (*recorder)->GetInterface(recorder, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, (void *)&recordBufferQueueItf);
	ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

	res = (*recordBufferQueueItf)->RegisterCallback(recordBufferQueueItf, _record_buffer_callbacks, this);
	ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

	SLuint32 state;
	res = (*recordItf)->GetRecordState(recordItf, &state);
	ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

	if (state != SL_RECORDSTATE_STOPPED) {
		res = (*recordItf)->SetRecordState(recordItf, SL_RECORDSTATE_STOPPED);
		ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

		res = (*recordBufferQueueItf)->Clear(recordBufferQueueItf);
		ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);
	}

	const int rec_buffer_frames = 2048;
	rec_buffer.resize(rec_buffer_frames);
	input_buffer_init(rec_buffer_frames);

	res = (*recordBufferQueueItf)->Enqueue(recordBufferQueueItf, rec_buffer.ptrw(), rec_buffer.size() * sizeof(int16_t));
	ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

	res = (*recordItf)->SetRecordState(recordItf, SL_RECORDSTATE_RECORDING);
	ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

	return OK;
}

Error AudioDriverOpenSL::input_start() {
	if (recordItf || recordBufferQueueItf) {
		return ERR_ALREADY_IN_USE;
	}

	if (OS::get_singleton()->request_permission("RECORD_AUDIO")) {
		return init_input_device();
	}

	WARN_PRINT("Unable to start audio capture - No RECORD_AUDIO permission");
	return ERR_UNAUTHORIZED;
}

Error AudioDriverOpenSL::input_stop() {
	if (!recordItf || !recordBufferQueueItf) {
		return ERR_CANT_OPEN;
	}

	SLuint32 state;
	SLresult res = (*recordItf)->GetRecordState(recordItf, &state);
	ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

	if (state != SL_RECORDSTATE_STOPPED) {
		res = (*recordItf)->SetRecordState(recordItf, SL_RECORDSTATE_STOPPED);
		ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);

		res = (*recordBufferQueueItf)->Clear(recordBufferQueueItf);
		ERR_FAIL_COND_V(res != SL_RESULT_SUCCESS, ERR_CANT_OPEN);
	}

	return OK;
}

int AudioDriverOpenSL::get_mix_rate() const {
	return 44100; // hardcoded for Android, as selected by SL_SAMPLINGRATE_44_1
}

AudioDriver::SpeakerMode AudioDriverOpenSL::get_speaker_mode() const {
	return SPEAKER_MODE_STEREO;
}

void AudioDriverOpenSL::lock() {
	if (active) {
		mutex.lock();
	}
}

void AudioDriverOpenSL::unlock() {
	if (active) {
		mutex.unlock();
	}
}

void AudioDriverOpenSL::finish() {
	if (recordItf) {
		(*recordItf)->SetRecordState(recordItf, SL_RECORDSTATE_STOPPED);
		recordItf = nullptr;
	}
	if (recorder) {
		(*recorder)->Destroy(recorder);
		recorder = nullptr;
	}
	if (playItf) {
		(*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);
		playItf = nullptr;
	}
	if (player) {
		(*player)->Destroy(player);
		player = nullptr;
	}
	if (OutputMix) {
		(*OutputMix)->Destroy(OutputMix);
		OutputMix = nullptr;
	}
	if (sl) {
		(*sl)->Destroy(sl);
		sl = nullptr;
	}
}

void AudioDriverOpenSL::set_pause(bool p_pause) {
	pause = p_pause;

	if (active && playItf) {
		if (pause) {
			(*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PAUSED);
		} else {
			(*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
		}
	}
}

AudioDriverOpenSL::AudioDriverOpenSL() {
}
