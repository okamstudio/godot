/**************************************************************************/
/*  audio_effect_reverb.h                                                 */
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
#include "servers/audio/effects/reverb_filter.h"

class AudioEffectReverb;

class AudioEffectReverbInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectReverbInstance, AudioEffectInstance);

	Ref<AudioEffectReverb> base;

	float tmp_src[Reverb::INPUT_BUFFER_MAX_SIZE];
	float tmp_dst[Reverb::INPUT_BUFFER_MAX_SIZE];

	friend class AudioEffectReverb;

	Reverb reverb[2];

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
	AudioEffectReverbInstance();
};

class AudioEffectReverb : public AudioEffect {
	GDCLASS(AudioEffectReverb, AudioEffect);

	friend class AudioEffectReverbInstance;

	float predelay;
	float predelay_fb;
	float hpf;
	float room_size;
	float damping;
	float spread;
	float dry;
	float wet;

protected:
	static void _bind_methods();

public:
	void set_predelay_msec(float p_msec);
	void set_predelay_feedback(float p_feedback);
	void set_room_size(float p_size);
	void set_damping(float p_damping);
	void set_spread(float p_spread);
	void set_dry(float p_dry);
	void set_wet(float p_wet);
	void set_hpf(float p_hpf);

	float get_predelay_msec() const;
	float get_predelay_feedback() const;
	float get_room_size() const;
	float get_damping() const;
	float get_spread() const;
	float get_dry() const;
	float get_wet() const;
	float get_hpf() const;

	Ref<AudioEffectInstance> instantiate() override;

	AudioEffectReverb();
};
