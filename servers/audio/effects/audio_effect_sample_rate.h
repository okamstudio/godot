/*************************************************************************/
/*  audio_effect_sample_rate.h                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef AUDIO_EFFECT_SAMPLE_RATE_H
#define AUDIO_EFFECT_SAMPLE_RATE_H

#include "servers/audio/audio_effect.h"

class AudioEffectSampleRate;

class AudioEffectSampleRateInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectSampleRateInstance, AudioEffectInstance);
	friend class AudioEffectSampleRate;
	Ref<AudioEffectSampleRate> base;

	float processed_frames = 0;
	AudioFrame last_sampled_frame;

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
};

class AudioEffectSampleRate : public AudioEffect {
	GDCLASS(AudioEffectSampleRate, AudioEffect);

	friend class AudioEffectSampleRateInstance;

protected:
	static void _bind_methods();

public:
	float rate;
	float mix;

	Ref<AudioEffectInstance> instantiate() override;
	void set_rate(float p_rate);
	float get_rate() const;
	void set_mix(float p_mix);
	float get_mix() const;

	AudioEffectSampleRate();
};

#endif // AUDIO_EFFECT_SAMPLE_RATE_H
