/**************************************************************************/
/*  audio_effect_spectrum_analyzer.h                                      */
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

class AudioEffectSpectrumAnalyzer;

class AudioEffectSpectrumAnalyzerInstance : public AudioEffectInstance {
	GDCLASS(AudioEffectSpectrumAnalyzerInstance, AudioEffectInstance);

public:
	enum MagnitudeMode {
		MAGNITUDE_AVERAGE,
		MAGNITUDE_MAX,
	};

private:
	friend class AudioEffectSpectrumAnalyzer;
	Ref<AudioEffectSpectrumAnalyzer> base;

	Vector<Vector<AudioFrame>> fft_history;
	Vector<float> temporal_fft;
	int temporal_fft_pos;
	int fft_size;
	int fft_count;
	int fft_pos;
	float mix_rate;
	uint64_t last_fft_time;

protected:
	static void _bind_methods();

public:
	virtual void process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) override;
	Vector2 get_magnitude_for_frequency_range(float p_begin, float p_end, MagnitudeMode p_mode = MAGNITUDE_MAX) const;
};

VARIANT_ENUM_CAST(AudioEffectSpectrumAnalyzerInstance::MagnitudeMode)

class AudioEffectSpectrumAnalyzer : public AudioEffect {
	GDCLASS(AudioEffectSpectrumAnalyzer, AudioEffect);

public:
	enum FFTSize {
		FFT_SIZE_256,
		FFT_SIZE_512,
		FFT_SIZE_1024,
		FFT_SIZE_2048,
		FFT_SIZE_4096,
		FFT_SIZE_MAX
	};

public:
	friend class AudioEffectSpectrumAnalyzerInstance;
	float buffer_length;
	float tapback_pos;
	FFTSize fft_size;

protected:
	static void _bind_methods();

public:
	Ref<AudioEffectInstance> instantiate() override;
	void set_buffer_length(float p_seconds);
	float get_buffer_length() const;
	void set_tap_back_pos(float p_seconds);
	float get_tap_back_pos() const;

	void set_fft_size(FFTSize);
	FFTSize get_fft_size() const;

	AudioEffectSpectrumAnalyzer();
};

VARIANT_ENUM_CAST(AudioEffectSpectrumAnalyzer::FFTSize);
