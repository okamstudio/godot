/**************************************************************************/
/*  audio_effect_eq.h                                                     */
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
#include "servers/audio/effects/eq_filter.h"

class AudioEffectEQ;

class AudioEffectEQInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectEQInstance, AudioEffectInstance);
	friend class AudioEffectEQ;
	Ref<AudioEffectEQ> base;

	Vector<EQ::BandProcess> bands[2];
	Vector<float> gains;

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
};

class AudioEffectEQ : public AudioEffect {
	GDCLASS(AudioEffectEQ, AudioEffect);

	friend class AudioEffectEQInstance;

	EQ eq;
	Vector<float> gain;
	HashMap<StringName, int> prop_band_map;
	Vector<String> band_names;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

public:
	Ref<AudioEffectInstance> instantiate() override;
	void set_band_gain_db(int p_band, float p_volume);
	float get_band_gain_db(int p_band) const;
	int get_band_count() const;

	AudioEffectEQ(EQ::Preset p_preset = EQ::PRESET_6_BANDS);
};

class AudioEffectEQ6 : public AudioEffectEQ {
	GDCLASS(AudioEffectEQ6, AudioEffectEQ);

public:
	AudioEffectEQ6() :
			AudioEffectEQ(EQ::PRESET_6_BANDS) {}
};

class AudioEffectEQ10 : public AudioEffectEQ {
	GDCLASS(AudioEffectEQ10, AudioEffectEQ);

public:
	AudioEffectEQ10() :
			AudioEffectEQ(EQ::PRESET_10_BANDS) {}
};

class AudioEffectEQ21 : public AudioEffectEQ {
	GDCLASS(AudioEffectEQ21, AudioEffectEQ);

public:
	AudioEffectEQ21() :
			AudioEffectEQ(EQ::PRESET_21_BANDS) {}
};
