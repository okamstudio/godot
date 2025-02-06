/**************************************************************************/
/*  audio_effect_gate.h                                                   */
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

#ifndef AUDIO_EFFECT_GATE_H
#define AUDIO_EFFECT_GATE_H

#include "servers/audio/audio_effect.h"

class AudioEffectGate;

class AudioEffectGateInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectGateInstance, AudioEffectInstance);
	friend class AudioEffectGate;

	Ref<AudioEffectGate> base;

	enum GateState {
		GATE_CLOSED,
		GATE_ATTACK,
		GATE_OPEN,
		GATE_HOLD,
		GATE_RELEASE
	};

	GateState gate_state = GATE_CLOSED;
	float last_envelope_value = 0.f;
	int samples_since_below_threshold = 0.f;
	bool below_threshold = true; // Start with silence

public:
	float next_envelope_value(float p_sample_rate);

	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
};

class AudioEffectGate : public AudioEffect {
	GDCLASS(AudioEffectGate, AudioEffect);
	friend class AudioEffectGateInstance;

protected:
	static void _bind_methods();

	float attack_ms = 5.f;
	float hold_ms = 100.f;
	float release_ms = 500.f;

	float threshold_db = -30.f;

public:
	virtual Ref<AudioEffectInstance> instantiate() override;

	void set_attack_ms(float p_attack_ms);
	float get_attack_ms();

	void set_hold_ms(float p_hold_ms);
	float get_hold_ms();

	void set_release_ms(float p_release_ms);
	float get_release_ms();

	void set_threshold_db(float p_threshold_db);
	float get_threshold_db() const;
};

#endif // AUDIO_EFFECT_GATE_H
