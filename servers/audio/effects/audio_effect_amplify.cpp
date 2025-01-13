/**************************************************************************/
/*  audio_effect_amplify.cpp                                              */
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

#include "audio_effect_amplify.h"

void AudioEffectAmplifyInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	//multiply volume interpolating to avoid clicks if this changes
	float volume_db = base->volume_db;
	float vol = Math::db_to_linear(mix_volume_db);
	float vol_inc = (Math::db_to_linear(volume_db) - vol) / float(p_frame_count);

	for (int i = 0; i < p_frame_count; i++) {
		p_dst_frames[i] = p_src_frames[i] * vol;
		vol += vol_inc;
	}
	//set volume for next mix
	mix_volume_db = volume_db;
}

Ref<AudioEffectInstance> AudioEffectAmplify::instantiate() {
	Ref<AudioEffectAmplifyInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectAmplify>(this);
	ins->mix_volume_db = volume_db;
	return ins;
}

void AudioEffectAmplify::set_volume_db(float p_volume) {
	volume_db = p_volume;
}

float AudioEffectAmplify::get_volume_db() const {
	return volume_db;
}

void AudioEffectAmplify::set_volume_linear(float p_volume) {
	set_volume_db(Math::linear_to_db(p_volume));
}

float AudioEffectAmplify::get_volume_linear() const {
	return Math::db_to_linear(get_volume_db());
}

void AudioEffectAmplify::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_volume_db", "volume"), &AudioEffectAmplify::set_volume_db);
	ClassDB::bind_method(D_METHOD("get_volume_db"), &AudioEffectAmplify::get_volume_db);
	ClassDB::bind_method(D_METHOD("set_volume_linear", "volume"), &AudioEffectAmplify::set_volume_linear);
	ClassDB::bind_method(D_METHOD("get_volume_linear"), &AudioEffectAmplify::get_volume_linear);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_db", PROPERTY_HINT_RANGE, "-80,24,0.01,suffix:dB"), "set_volume_db", "get_volume_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_linear", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_volume_linear", "get_volume_linear");
}

AudioEffectAmplify::AudioEffectAmplify() {
	volume_db = 0;
}
