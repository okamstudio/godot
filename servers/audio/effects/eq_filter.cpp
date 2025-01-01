/**************************************************************************/
/*  eq_filter.cpp                                                         */
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

#include "eq_filter.h"

#include "core/error/error_macros.h"
#include "core/math/math_defs.h"

#include <math.h>

#define POW2(v) ((v) * (v))

/* Helper */
static int solve_quadratic(double a, double b, double c, double *r1, double *r2) {
	//solves quadractic and returns number of roots

	double base = 2 * a;
	if (base == 0.0f) {
		return 0;
	}

	double squared = b * b - 4 * a * c;
	if (squared < 0.0) {
		return 0;
	}

	squared = sqrt(squared);

	*r1 = (-b + squared) / base;
	*r2 = (-b - squared) / base;

	if (*r1 == *r2) {
		return 1;
	} else {
		return 2;
	}
}

EQ::BandProcess::BandProcess() {
	c1 = c2 = c3 = history.a1 = history.a2 = history.a3 = 0;
	history.b1 = history.b2 = history.b3 = 0;
}

void EQ::recalculate_band_coefficients() {
#define BAND_LOG(m_f) (log((m_f)) / log(2.))

	for (int i = 0; i < band.size(); i++) {
		double octave_size;

		double frq = band[i].freq;

		if (i == 0) {
			octave_size = BAND_LOG(band[1].freq) - BAND_LOG(frq);
		} else if (i == (band.size() - 1)) {
			octave_size = BAND_LOG(frq) - BAND_LOG(band[i - 1].freq);
		} else {
			double next = BAND_LOG(band[i + 1].freq) - BAND_LOG(frq);
			double prev = BAND_LOG(frq) - BAND_LOG(band[i - 1].freq);
			octave_size = (next + prev) / 2.0;
		}

		double frq_l = round(frq / pow(2.0, octave_size / 2.0));

		double side_gain2 = POW2(Math_SQRT12);
		double th = Math_TAU * frq / mix_rate;
		double th_l = Math_TAU * frq_l / mix_rate;

		double c2a = side_gain2 * POW2(cos(th)) - 2.0 * side_gain2 * cos(th_l) * cos(th) + side_gain2 - POW2(sin(th_l));

		double c2b = 2.0 * side_gain2 * POW2(cos(th_l)) + side_gain2 * POW2(cos(th)) - 2.0 * side_gain2 * cos(th_l) * cos(th) - side_gain2 + POW2(sin(th_l));

		double c2c = 0.25 * side_gain2 * POW2(cos(th)) - 0.5 * side_gain2 * cos(th_l) * cos(th) + 0.25 * side_gain2 - 0.25 * POW2(sin(th_l));

		//printf("band %i, precoefs = %f,%f,%f\n",i,c2a,c2b,c2c);

		// Default initializing to silence compiler warning about potential uninitialized use.
		// Both variables are properly set in _solve_quadratic before use, or we continue if roots == 0.
		double r1 = 0, r2 = 0; //roots
		int roots = solve_quadratic(c2a, c2b, c2c, &r1, &r2);

		ERR_CONTINUE(roots == 0);

		band.write[i].c1 = 2.0 * ((0.5 - r1) / 2.0);
		band.write[i].c2 = 2.0 * r1;
		band.write[i].c3 = 2.0 * (0.5 + r1) * cos(th);
		//printf("band %i, coefs = %f,%f,%f\n",i,(float)bands[i].c1,(float)bands[i].c2,(float)bands[i].c3);
	}
}

void EQ::set_preset_band_mode(Preset p_preset) {
	band.clear();

#define PUSH_BANDS(m_bands)             \
	for (int i = 0; i < m_bands; i++) { \
		Band b;                         \
		b.freq = bands[i];              \
		b.c1 = b.c2 = b.c3 = 0;         \
		band.push_back(b);              \
	}

	switch (p_preset) {
		case PRESET_6_BANDS: {
			static const double bands[] = { 32, 100, 320, 1e3, 3200, 10e3 };
			PUSH_BANDS(6);

		} break;

		case PRESET_8_BANDS: {
			static const double bands[] = { 32, 72, 192, 512, 1200, 3000, 7500, 16e3 };

			PUSH_BANDS(8);
		} break;

		case PRESET_10_BANDS: {
			static const double bands[] = { 31.25, 62.5, 125, 250, 500, 1e3, 2e3, 4e3, 8e3, 16e3 };

			PUSH_BANDS(10);

		} break;

		case PRESET_21_BANDS: {
			static const double bands[] = { 22, 32, 44, 63, 90, 125, 175, 250, 350, 500, 700, 1e3, 1400, 2e3, 2800, 4e3, 5600, 8e3, 11e3, 16e3, 22e3 };
			PUSH_BANDS(21);

		} break;

		case PRESET_31_BANDS: {
			static const double bands[] = { 20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800, 1e3, 1250, 1600, 2e3, 2500, 3150, 4e3, 5e3, 6300, 8e3, 10e3, 12500, 16e3, 20e3 };
			PUSH_BANDS(31);
		} break;
	};

	recalculate_band_coefficients();
}

int EQ::get_band_count() const {
	return band.size();
}

float EQ::get_band_frequency(int p_band) {
	ERR_FAIL_INDEX_V(p_band, band.size(), 0);
	return band[p_band].freq;
}

void EQ::set_bands(const Vector<float> &p_bands) {
	band.resize(p_bands.size());
	for (int i = 0; i < p_bands.size(); i++) {
		band.write[i].freq = p_bands[i];
	}

	recalculate_band_coefficients();
}

void EQ::set_mix_rate(float p_mix_rate) {
	mix_rate = p_mix_rate;
	recalculate_band_coefficients();
}

EQ::BandProcess EQ::get_band_processor(int p_band) const {
	EQ::BandProcess band_proc;

	ERR_FAIL_INDEX_V(p_band, band.size(), band_proc);

	band_proc.c1 = band[p_band].c1;
	band_proc.c2 = band[p_band].c2;
	band_proc.c3 = band[p_band].c3;

	return band_proc;
}

EQ::EQ() {
	mix_rate = 44100;
}

EQ::~EQ() {
}
