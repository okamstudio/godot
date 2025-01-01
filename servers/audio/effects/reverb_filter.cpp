/**************************************************************************/
/*  reverb_filter.cpp                                                     */
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

#include "reverb_filter.h"

#include "core/math/audio_frame.h"
#include "core/os/memory.h"

#include <math.h>

const float Reverb::comb_tunings[MAX_COMBS] = {
	//freeverb comb tunings
	0.025306122448979593f,
	0.026938775510204082f,
	0.028956916099773241f,
	0.03074829931972789f,
	0.032244897959183672f,
	0.03380952380952381f,
	0.035306122448979592f,
	0.036666666666666667f
};

const float Reverb::allpass_tunings[MAX_ALLPASS] = {
	//freeverb allpass tunings
	0.0051020408163265302f,
	0.007732426303854875f,
	0.01f,
	0.012607709750566893f
};

void Reverb::process(float *p_src, float *p_dst, int p_frames) {
	if (p_frames > INPUT_BUFFER_MAX_SIZE) {
		p_frames = INPUT_BUFFER_MAX_SIZE;
	}

	int predelay_frames = lrint((params.predelay / 1000.0) * params.mix_rate);
	if (predelay_frames < 10) {
		predelay_frames = 10;
	}
	if (predelay_frames >= echo_buffer_size) {
		predelay_frames = echo_buffer_size - 1;
	}

	for (int i = 0; i < p_frames; i++) {
		if (echo_buffer_pos >= echo_buffer_size) {
			echo_buffer_pos = 0;
		}

		int read_pos = echo_buffer_pos - predelay_frames;
		while (read_pos < 0) {
			read_pos += echo_buffer_size;
		}

		float in = undenormalize(echo_buffer[read_pos] * params.predelay_fb + p_src[i]);

		echo_buffer[echo_buffer_pos] = in;

		input_buffer[i] = in;

		p_dst[i] = 0; //take the chance and clear this

		echo_buffer_pos++;
	}

	if (params.hpf > 0) {
		float hpaux = expf(-Math_TAU * params.hpf * 6000 / params.mix_rate);
		float hp_a1 = (1.0 + hpaux) / 2.0;
		float hp_a2 = -(1.0 + hpaux) / 2.0;
		float hp_b1 = hpaux;

		for (int i = 0; i < p_frames; i++) {
			float in = input_buffer[i];
			input_buffer[i] = in * hp_a1 + hpf_h1 * hp_a2 + hpf_h2 * hp_b1;
			hpf_h2 = input_buffer[i];
			hpf_h1 = in;
		}
	}

	for (int i = 0; i < MAX_COMBS; i++) {
		Comb &c = comb[i];

		int size_limit = c.size - lrintf((float)c.extra_spread_frames * (1.0 - params.extra_spread));
		for (int j = 0; j < p_frames; j++) {
			if (c.pos >= size_limit) { //reset this now just in case
				c.pos = 0;
			}

			float out = undenormalize(c.buffer[c.pos] * c.feedback);
			out = out * (1.0 - c.damp) + c.damp_h * c.damp; //lowpass
			c.damp_h = out;
			c.buffer[c.pos] = input_buffer[j] + out;
			p_dst[j] += out;
			c.pos++;
		}
	}

	static const float allpass_feedback = 0.7;
	/* this one works, but the other version is just nicer....
	int ap_size_limit[MAX_ALLPASS];

	for (int i=0;i<MAX_ALLPASS;i++) {
		AllPass &a=allpass[i];
		ap_size_limit[i]=a.size-lrintf((float)a.extra_spread_frames*(1.0-params.extra_spread));
	}

	for (int i=0;i<p_frames;i++) {
		float sample=p_dst[i];
		float aux,in;
		float AllPass*ap;

#define PROCESS_ALLPASS(m_ap) 	\
	ap=&allpass[m_ap];	\
	if (ap->pos>=ap_size_limit[m_ap])	\
		ap->pos=0;	\
	aux=undenormalize(ap->buffer[ap->pos]);	\
	in=sample;	\
	sample=-in+aux;	\
	ap->pos++;


		PROCESS_ALLPASS(0);
		PROCESS_ALLPASS(1);
		PROCESS_ALLPASS(2);
		PROCESS_ALLPASS(3);

		p_dst[i]=sample;
	}
	*/

	for (int i = 0; i < MAX_ALLPASS; i++) {
		AllPass &a = allpass[i];
		int size_limit = a.size - lrintf((float)a.extra_spread_frames * (1.0 - params.extra_spread));

		for (int j = 0; j < p_frames; j++) {
			if (a.pos >= size_limit) {
				a.pos = 0;
			}

			float aux = a.buffer[a.pos];
			a.buffer[a.pos] = undenormalize(allpass_feedback * aux + p_dst[j]);
			p_dst[j] = aux - allpass_feedback * a.buffer[a.pos];
			a.pos++;
		}
	}

	static const float wet_scale = 0.6;

	for (int i = 0; i < p_frames; i++) {
		p_dst[i] = p_dst[i] * params.wet * wet_scale + p_src[i] * params.dry;
	}
}

void Reverb::set_room_size(float p_size) {
	params.room_size = p_size;
	update_parameters();
}

void Reverb::set_damp(float p_damp) {
	params.damp = p_damp;
	update_parameters();
}

void Reverb::set_wet(float p_wet) {
	params.wet = p_wet;
}

void Reverb::set_dry(float p_dry) {
	params.dry = p_dry;
}

void Reverb::set_predelay(float p_predelay) {
	params.predelay = p_predelay;
}

void Reverb::set_predelay_feedback(float p_predelay_fb) {
	params.predelay_fb = p_predelay_fb;
}

void Reverb::set_highpass(float p_frq) {
	if (p_frq > 1) {
		p_frq = 1;
	}
	if (p_frq < 0) {
		p_frq = 0;
	}
	params.hpf = p_frq;
}

void Reverb::set_extra_spread(float p_spread) {
	params.extra_spread = p_spread;
}

void Reverb::set_mix_rate(float p_mix_rate) {
	params.mix_rate = p_mix_rate;
	configure_buffers();
}

void Reverb::set_extra_spread_base(float p_sec) {
	params.extra_spread_base = p_sec;
	configure_buffers();
}

void Reverb::configure_buffers() {
	clear_buffers(); //clear if necessary

	for (int i = 0; i < MAX_COMBS; i++) {
		Comb &c = comb[i];

		c.extra_spread_frames = lrint(params.extra_spread_base * params.mix_rate);

		int len = lrint(comb_tunings[i] * params.mix_rate) + c.extra_spread_frames;
		if (len < 5) {
			len = 5; //may this happen?
		}

		c.buffer = memnew_arr(float, len);
		c.pos = 0;
		for (int j = 0; j < len; j++) {
			c.buffer[j] = 0;
		}
		c.size = len;
	}

	for (int i = 0; i < MAX_ALLPASS; i++) {
		AllPass &a = allpass[i];

		a.extra_spread_frames = lrint(params.extra_spread_base * params.mix_rate);

		int len = lrint(allpass_tunings[i] * params.mix_rate) + a.extra_spread_frames;
		if (len < 5) {
			len = 5; //may this happen?
		}

		a.buffer = memnew_arr(float, len);
		a.pos = 0;
		for (int j = 0; j < len; j++) {
			a.buffer[j] = 0;
		}
		a.size = len;
	}

	echo_buffer_size = (int)(((float)MAX_ECHO_MS / 1000.0) * params.mix_rate + 1.0);
	echo_buffer = memnew_arr(float, echo_buffer_size);
	for (int i = 0; i < echo_buffer_size; i++) {
		echo_buffer[i] = 0;
	}

	echo_buffer_pos = 0;
}

void Reverb::update_parameters() {
	//more freeverb derived constants
	static const float room_scale = 0.28f;
	static const float room_offset = 0.7f;

	for (int i = 0; i < MAX_COMBS; i++) {
		Comb &c = comb[i];
		c.feedback = room_offset + params.room_size * room_scale;
		if (c.feedback < room_offset) {
			c.feedback = room_offset;
		} else if (c.feedback > (room_offset + room_scale)) {
			c.feedback = (room_offset + room_scale);
		}

		float auxdmp = params.damp / 2.0 + 0.5; //only half the range (0.5 .. 1.0 is enough)
		auxdmp *= auxdmp;

		c.damp = expf(-Math_TAU * auxdmp * 10000 / params.mix_rate); // 0 .. 10khz
	}
}

void Reverb::clear_buffers() {
	if (echo_buffer) {
		memdelete_arr(echo_buffer);
	}

	for (int i = 0; i < MAX_COMBS; i++) {
		if (comb[i].buffer) {
			memdelete_arr(comb[i].buffer);
		}

		comb[i].buffer = nullptr;
	}

	for (int i = 0; i < MAX_ALLPASS; i++) {
		if (allpass[i].buffer) {
			memdelete_arr(allpass[i].buffer);
		}

		allpass[i].buffer = nullptr;
	}
}

Reverb::Reverb() {
	params.room_size = 0.8;
	params.damp = 0.5;
	params.dry = 1.0;
	params.wet = 0.0;
	params.mix_rate = 44100;
	params.extra_spread_base = 0;
	params.extra_spread = 1.0;
	params.predelay = 150;
	params.predelay_fb = 0.4;
	params.hpf = 0;

	input_buffer = memnew_arr(float, INPUT_BUFFER_MAX_SIZE);

	configure_buffers();
	update_parameters();
}

Reverb::~Reverb() {
	memdelete_arr(input_buffer);
	clear_buffers();
}
