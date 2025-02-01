/**************************************************************************/
/*  audio_effect_phaser.h                                                 */
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

class AudioEffectPhaser;

class AudioEffectPhaserInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectPhaserInstance, AudioEffectInstance);
	friend class AudioEffectPhaser;
	Ref<AudioEffectPhaser> base;

	float phase;
	AudioFrame h;

	class AllpassDelay {
		float a, h;

	public:
		_ALWAYS_INLINE_ void delay(float d) {
			a = (1.f - d) / (1.f + d);
		}

		_ALWAYS_INLINE_ float update(float s) {
			float y = s * -a + h;
			h = y * a + s;
			return y;
		}

		AllpassDelay() {
			a = 0;
			h = 0;
		}
	};

	AllpassDelay allpass[2][6];

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
};

class AudioEffectPhaser : public AudioEffect {
	GDCLASS(AudioEffectPhaser, AudioEffect);

	friend class AudioEffectPhaserInstance;
	float range_min;
	float range_max;
	float rate;
	float feedback;
	float depth;

protected:
	static void _bind_methods();

public:
	Ref<AudioEffectInstance> instantiate() override;

	void set_range_min_hz(float p_hz);
	float get_range_min_hz() const;

	void set_range_max_hz(float p_hz);
	float get_range_max_hz() const;

	void set_rate_hz(float p_hz);
	float get_rate_hz() const;

	void set_feedback(float p_fbk);
	float get_feedback() const;

	void set_depth(float p_depth);
	float get_depth() const;

	AudioEffectPhaser();
};
