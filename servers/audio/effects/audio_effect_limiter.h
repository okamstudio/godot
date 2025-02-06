/**************************************************************************/
/*  audio_effect_limiter.h                                                */
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

class AudioEffectLimiter;

class AudioEffectLimiterInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectLimiterInstance, AudioEffectInstance);
	friend class AudioEffectLimiter;
	Ref<AudioEffectLimiter> base;

	float mix_volume_db;

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
};

class AudioEffectLimiter : public AudioEffect {
	GDCLASS(AudioEffectLimiter, AudioEffect);

	friend class AudioEffectLimiterInstance;
	float threshold;
	float ceiling;
	float soft_clip;
	float soft_clip_ratio;

protected:
	static void _bind_methods();

public:
	void set_threshold_db(float p_threshold);
	float get_threshold_db() const;

	void set_ceiling_db(float p_ceiling);
	float get_ceiling_db() const;

	void set_soft_clip_db(float p_soft_clip);
	float get_soft_clip_db() const;

	void set_soft_clip_ratio(float p_soft_clip);
	float get_soft_clip_ratio() const;

	Ref<AudioEffectInstance> instantiate() override;

	AudioEffectLimiter();
};
