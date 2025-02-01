/**************************************************************************/
/*  audio_effect_distortion.h                                             */
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

class AudioEffectDistortion;

class AudioEffectDistortionInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectDistortionInstance, AudioEffectInstance);
	friend class AudioEffectDistortion;
	Ref<AudioEffectDistortion> base;
	float h[2];

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
};

class AudioEffectDistortion : public AudioEffect {
	GDCLASS(AudioEffectDistortion, AudioEffect);

public:
	enum Mode {
		MODE_CLIP,
		MODE_ATAN,
		MODE_LOFI,
		MODE_OVERDRIVE,
		MODE_WAVESHAPE,
	};

	friend class AudioEffectDistortionInstance;
	Mode mode;
	float pre_gain;
	float post_gain;
	float keep_hf_hz;
	float drive;

protected:
	static void _bind_methods();

public:
	Ref<AudioEffectInstance> instantiate() override;

	void set_mode(Mode p_mode);
	Mode get_mode() const;

	void set_pre_gain(float p_pre_gain);
	float get_pre_gain() const;

	void set_keep_hf_hz(float p_keep_hf_hz);
	float get_keep_hf_hz() const;

	void set_drive(float p_drive);
	float get_drive() const;

	void set_post_gain(float p_post_gain);
	float get_post_gain() const;

	AudioEffectDistortion();
};

VARIANT_ENUM_CAST(AudioEffectDistortion::Mode)
