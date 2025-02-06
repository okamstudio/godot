/**************************************************************************/
/*  audio_filter_sw.h                                                     */
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

#ifndef AUDIO_FILTER_SW_H
#define AUDIO_FILTER_SW_H

#include "core/typedefs.h"

class AudioFilterSW {
public:
	struct Coeffs {
		double a1 = 0.0;
		double a2 = 0.0;
		double b0 = 0.0;
		double b1 = 0.0;
		double b2 = 0.0;
	};

	enum Mode {
		BANDPASS,
		HIGHPASS,
		LOWPASS,
		NOTCH,
		PEAK,
		BANDLIMIT,
		LOWSHELF,
		HIGHSHELF
	};

	class Processor { // Simple filter processor.
		AudioFilterSW *filter = nullptr;
		Coeffs coeffs;
		// History.
		float ha1 = 0.0f;
		float ha2 = 0.0f;
		float hb1 = 0.0f;
		float hb2 = 0.0f;
		Coeffs incr_coeffs;

	public:
		void set_filter(AudioFilterSW *p_filter, bool p_clear_history = true);
		void process(float *p_samples, int p_amount, int p_stride = 1, bool p_interpolate = false);
		void update_coeffs(int p_interp_buffer_len = 0);
		_ALWAYS_INLINE_ void process_one(float &p_sample);
		_ALWAYS_INLINE_ void process_one_interp(float &p_sample);

		Processor();
	};

private:
	float cutoff = 5000.0f;
	float resonance = 0.5f;
	float gain = 1.0f;
	float sampling_rate = 44100.0f;
	int stages = 1;
	Mode mode = LOWPASS;

public:
	float get_response(float p_freq, Coeffs *p_coeffs);

	void set_mode(Mode p_mode);
	void set_cutoff(float p_cutoff);
	void set_resonance(float p_resonance);
	void set_gain(float p_gain);
	void set_sampling_rate(float p_srate);
	void set_stages(int p_stages); //adjust for multiple stages

	void prepare_coefficients(Coeffs *p_coeffs);

	AudioFilterSW() {}
};

/* inline methods */

void AudioFilterSW::Processor::process_one(float &p_sample) {
	float pre = p_sample;
	p_sample = (p_sample * coeffs.b0 + hb1 * coeffs.b1 + hb2 * coeffs.b2 + ha1 * coeffs.a1 + ha2 * coeffs.a2);
	ha2 = ha1;
	hb2 = hb1;
	hb1 = pre;
	ha1 = p_sample;
}

void AudioFilterSW::Processor::process_one_interp(float &p_sample) {
	float pre = p_sample;
	p_sample = (p_sample * coeffs.b0 + hb1 * coeffs.b1 + hb2 * coeffs.b2 + ha1 * coeffs.a1 + ha2 * coeffs.a2);
	ha2 = ha1;
	hb2 = hb1;
	hb1 = pre;
	ha1 = p_sample;

	coeffs.b0 += incr_coeffs.b0;
	coeffs.b1 += incr_coeffs.b1;
	coeffs.b2 += incr_coeffs.b2;
	coeffs.a1 += incr_coeffs.a1;
	coeffs.a2 += incr_coeffs.a2;
}

#endif // AUDIO_FILTER_SW_H
