/**************************************************************************/
/*  audio_effect_compressor.h                                             */
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

class AudioEffectCompressor;

class AudioEffectCompressorInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectCompressorInstance, AudioEffectInstance);
	friend class AudioEffectCompressor;
	Ref<AudioEffectCompressor> base;

	float rundb, averatio, runratio, runmax, maxover, gr_meter;
	int current_channel;

public:
	void set_current_channel(int p_channel) { current_channel = p_channel; }
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
};

class AudioEffectCompressor : public AudioEffect {
	GDCLASS(AudioEffectCompressor, AudioEffect);

	friend class AudioEffectCompressorInstance;
	float threshold;
	float ratio;
	float gain;
	float attack_us;
	float release_ms;
	float mix;
	StringName sidechain;

protected:
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

public:
	Ref<AudioEffectInstance> instantiate() override;

	void set_threshold(float p_threshold);
	float get_threshold() const;

	void set_ratio(float p_ratio);
	float get_ratio() const;

	void set_gain(float p_gain);
	float get_gain() const;

	void set_attack_us(float p_attack_us);
	float get_attack_us() const;

	void set_release_ms(float p_release_ms);
	float get_release_ms() const;

	void set_mix(float p_mix);
	float get_mix() const;

	void set_sidechain(const StringName &p_sidechain);
	StringName get_sidechain() const;

	AudioEffectCompressor();
};
