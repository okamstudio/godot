/**************************************************************************/
/*  eq_filter.h                                                           */
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

#ifndef EQ_FILTER_H
#define EQ_FILTER_H

#include "core/templates/vector.h"

class EQ {
public:
	enum Preset {
		PRESET_6_BANDS,
		PRESET_8_BANDS,
		PRESET_10_BANDS,
		PRESET_21_BANDS,
		PRESET_31_BANDS
	};

	class BandProcess {
		friend class EQ;
		float c1, c2, c3;
		struct History {
			float a1, a2, a3;
			float b1, b2, b3;

		} history;

	public:
		inline void process_one(float &p_data);

		BandProcess();
	};

private:
	struct Band {
		float freq;
		float c1, c2, c3;
	};

	Vector<Band> band;

	float mix_rate;

	void recalculate_band_coefficients();

public:
	void set_mix_rate(float p_mix_rate);

	int get_band_count() const;
	void set_preset_band_mode(Preset p_preset);
	void set_bands(const Vector<float> &p_bands);
	BandProcess get_band_processor(int p_band) const;
	float get_band_frequency(int p_band);

	EQ();
	~EQ();
};

/* Inline Function */

inline void EQ::BandProcess::process_one(float &p_data) {
	history.a1 = p_data;

	history.b1 = c1 * (history.a1 - history.a3) + c3 * history.b2 - c2 * history.b3;

	p_data = history.b1;

	history.a3 = history.a2;
	history.a2 = history.a1;
	history.b3 = history.b2;
	history.b2 = history.b1;
}

#endif // EQ_FILTER_H
