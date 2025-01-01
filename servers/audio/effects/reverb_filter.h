/**************************************************************************/
/*  reverb_filter.h                                                       */
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

#ifndef REVERB_FILTER_H
#define REVERB_FILTER_H

class Reverb {
public:
	enum {
		INPUT_BUFFER_MAX_SIZE = 1024,

	};

private:
	enum {
		MAX_COMBS = 8,
		MAX_ALLPASS = 4,
		MAX_ECHO_MS = 500

	};

	static const float comb_tunings[MAX_COMBS];
	static const float allpass_tunings[MAX_ALLPASS];

	struct Comb {
		int size = 0;
		float *buffer = nullptr;
		float feedback = 0;
		float damp = 0; //lowpass
		float damp_h = 0; //history
		int pos = 0;
		int extra_spread_frames = 0;

		Comb() {}
	};

	struct AllPass {
		int size = 0;
		float *buffer = nullptr;
		int pos = 0;
		int extra_spread_frames = 0;
		AllPass() {}
	};

	Comb comb[MAX_COMBS];
	AllPass allpass[MAX_ALLPASS];
	float *input_buffer = nullptr;
	float *echo_buffer = nullptr;
	int echo_buffer_size = 0;
	int echo_buffer_pos = 0;

	float hpf_h1 = 0.0f;
	float hpf_h2 = 0.0f;

	struct Parameters {
		float room_size;
		float damp;
		float wet;
		float dry;
		float mix_rate;
		float extra_spread_base;
		float extra_spread;
		float predelay;
		float predelay_fb;
		float hpf;
	} params;

	void configure_buffers();
	void update_parameters();
	void clear_buffers();

public:
	void set_room_size(float p_size);
	void set_damp(float p_damp);
	void set_wet(float p_wet);
	void set_dry(float p_dry);
	void set_predelay(float p_predelay); // in ms
	void set_predelay_feedback(float p_predelay_fb); // in ms
	void set_highpass(float p_frq);
	void set_mix_rate(float p_mix_rate);
	void set_extra_spread(float p_spread);
	void set_extra_spread_base(float p_sec);

	void process(float *p_src, float *p_dst, int p_frames);

	Reverb();

	~Reverb();
};

#endif // REVERB_FILTER_H
