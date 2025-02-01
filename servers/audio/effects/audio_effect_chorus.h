/**************************************************************************/
/*  audio_effect_chorus.h                                                 */
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

#include "servers/audio/audio_effect.h"

class AudioEffectChorus;

class AudioEffectChorusInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectChorusInstance, AudioEffectInstance);
	friend class AudioEffectChorus;
	Ref<AudioEffectChorus> base;

	Vector<AudioFrame> audio_buffer;
	unsigned int buffer_pos;
	unsigned int buffer_mask;

	AudioFrame filter_h[4];
	uint64_t cycles[4];

	void _process_chunk(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count);

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
};

class AudioEffectChorus : public AudioEffect {
	GDCLASS(AudioEffectChorus, AudioEffect);

	friend class AudioEffectChorusInstance;

public:
	enum {
		MAX_DELAY_MS = 50,
		MAX_DEPTH_MS = 20,
		MAX_WIDTH_MS = 50,
		MAX_VOICES = 4,
		CYCLES_FRAC = 16,
		CYCLES_MASK = (1 << CYCLES_FRAC) - 1,
		MAX_CHANNELS = 4,
		MS_CUTOFF_MAX = 16000
	};

private:
	struct Voice {
		float delay;
		float rate;
		float depth;
		float level;
		float cutoff;
		float pan;

		Voice() {
			delay = 12.0;
			rate = 1;
			depth = 0;
			level = 0;
			cutoff = MS_CUTOFF_MAX;
			pan = 0;
		}

	} voice[MAX_VOICES];

	int voice_count;

	float wet;
	float dry;

protected:
	void _validate_property(PropertyInfo &p_property) const;

	static void _bind_methods();

public:
	void set_voice_count(int p_voices);
	int get_voice_count() const;

	void set_voice_delay_ms(int p_voice, float p_delay_ms);
	float get_voice_delay_ms(int p_voice) const;

	void set_voice_rate_hz(int p_voice, float p_rate_hz);
	float get_voice_rate_hz(int p_voice) const;

	void set_voice_depth_ms(int p_voice, float p_depth_ms);
	float get_voice_depth_ms(int p_voice) const;

	void set_voice_level_db(int p_voice, float p_level_db);
	float get_voice_level_db(int p_voice) const;

	void set_voice_cutoff_hz(int p_voice, float p_cutoff_hz);
	float get_voice_cutoff_hz(int p_voice) const;

	void set_voice_pan(int p_voice, float p_pan);
	float get_voice_pan(int p_voice) const;

	void set_wet(float amount);
	float get_wet() const;

	void set_dry(float amount);
	float get_dry() const;

	Ref<AudioEffectInstance> instantiate() override;

	AudioEffectChorus();
};
