/**************************************************************************/
/*  audio_effect_filter.h                                                 */
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
#include "servers/audio/audio_filter_sw.h"

class AudioEffectFilter;

class AudioEffectFilterInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectFilterInstance, AudioEffectInstance);
	friend class AudioEffectFilter;

	Ref<AudioEffectFilter> base;

	AudioFilterSW filter;
	AudioFilterSW::Processor filter_process[2][4];

	template <int S>
	void _process_filter(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count);

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;

	AudioEffectFilterInstance();
};

class AudioEffectFilter : public AudioEffect {
	GDCLASS(AudioEffectFilter, AudioEffect);

public:
	enum FilterDB {
		FILTER_6DB,
		FILTER_12DB,
		FILTER_18DB,
		FILTER_24DB,
	};
	friend class AudioEffectFilterInstance;

	AudioFilterSW::Mode mode;
	float cutoff;
	float resonance;
	float gain;
	FilterDB db;

protected:
	static void _bind_methods();

public:
	void set_cutoff(float p_freq);
	float get_cutoff() const;

	void set_resonance(float p_amount);
	float get_resonance() const;

	void set_gain(float p_amount);
	float get_gain() const;

	void set_db(FilterDB p_db);
	FilterDB get_db() const;

	Ref<AudioEffectInstance> instantiate() override;

	AudioEffectFilter(AudioFilterSW::Mode p_mode = AudioFilterSW::LOWPASS);
};

VARIANT_ENUM_CAST(AudioEffectFilter::FilterDB)

class AudioEffectLowPassFilter : public AudioEffectFilter {
	GDCLASS(AudioEffectLowPassFilter, AudioEffectFilter);

	void _validate_property(PropertyInfo &p_property) const {
		if (p_property.name == "gain") {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	}

public:
	AudioEffectLowPassFilter() :
			AudioEffectFilter(AudioFilterSW::LOWPASS) {}
};

class AudioEffectHighPassFilter : public AudioEffectFilter {
	GDCLASS(AudioEffectHighPassFilter, AudioEffectFilter);
	void _validate_property(PropertyInfo &p_property) const {
		if (p_property.name == "gain") {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	}

public:
	AudioEffectHighPassFilter() :
			AudioEffectFilter(AudioFilterSW::HIGHPASS) {}
};

class AudioEffectBandPassFilter : public AudioEffectFilter {
	GDCLASS(AudioEffectBandPassFilter, AudioEffectFilter);
	void _validate_property(PropertyInfo &p_property) const {
		if (p_property.name == "gain") {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	}

public:
	AudioEffectBandPassFilter() :
			AudioEffectFilter(AudioFilterSW::BANDPASS) {}
};

class AudioEffectNotchFilter : public AudioEffectFilter {
	GDCLASS(AudioEffectNotchFilter, AudioEffectFilter);

public:
	AudioEffectNotchFilter() :
			AudioEffectFilter(AudioFilterSW::NOTCH) {}
};

class AudioEffectBandLimitFilter : public AudioEffectFilter {
	GDCLASS(AudioEffectBandLimitFilter, AudioEffectFilter);

public:
	AudioEffectBandLimitFilter() :
			AudioEffectFilter(AudioFilterSW::BANDLIMIT) {}
};

class AudioEffectLowShelfFilter : public AudioEffectFilter {
	GDCLASS(AudioEffectLowShelfFilter, AudioEffectFilter);

public:
	AudioEffectLowShelfFilter() :
			AudioEffectFilter(AudioFilterSW::LOWSHELF) {}
};

class AudioEffectHighShelfFilter : public AudioEffectFilter {
	GDCLASS(AudioEffectHighShelfFilter, AudioEffectFilter);

public:
	AudioEffectHighShelfFilter() :
			AudioEffectFilter(AudioFilterSW::HIGHSHELF) {}
};
