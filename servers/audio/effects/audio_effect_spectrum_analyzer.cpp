/**************************************************************************/
/*  audio_effect_spectrum_analyzer.cpp                                    */
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

#include "audio_effect_spectrum_analyzer.h"
#include "servers/audio_server.h"

static void smbFft(float *fftBuffer, long fftFrameSize, long sign)
/*
	FFT routine, (C)1996 S.M.Bernsee. Sign = -1 is FFT, 1 is iFFT (inverse)
	Fills fftBuffer[0...2*fftFrameSize-1] with the Fourier transform of the
	time domain data in fftBuffer[0...2*fftFrameSize-1]. The FFT array takes
	and returns the cosine and sine parts in an interleaved manner, ie.
	fftBuffer[0] = cosPart[0], fftBuffer[1] = sinPart[0], asf. fftFrameSize
	must be a power of 2. It expects a complex input signal (see footnote 2),
	ie. when working with 'common' audio signals our input signal has to be
	passed as {in[0],0.,in[1],0.,in[2],0.,...} asf. In that case, the transform
	of the frequencies of interest is in fftBuffer[0...fftFrameSize].
*/
{
	float wr, wi, arg, *p1, *p2, temp;
	float tr, ti, ur, ui, *p1r, *p1i, *p2r, *p2i;
	long i, bitm, j, le, le2, k;

	for (i = 2; i < 2 * fftFrameSize - 2; i += 2) {
		for (bitm = 2, j = 0; bitm < 2 * fftFrameSize; bitm <<= 1) {
			if (i & bitm) {
				j++;
			}
			j <<= 1;
		}
		if (i < j) {
			p1 = fftBuffer + i;
			p2 = fftBuffer + j;
			temp = *p1;
			*(p1++) = *p2;
			*(p2++) = temp;
			temp = *p1;
			*p1 = *p2;
			*p2 = temp;
		}
	}
	for (k = 0, le = 2; k < (long)(log((double)fftFrameSize) / log(2.) + .5); k++) {
		le <<= 1;
		le2 = le >> 1;
		ur = 1.0;
		ui = 0.0;
		arg = Math_PI / (le2 >> 1);
		wr = cos(arg);
		wi = sign * sin(arg);
		for (j = 0; j < le2; j += 2) {
			p1r = fftBuffer + j;
			p1i = p1r + 1;
			p2r = p1r + le2;
			p2i = p2r + 1;
			for (i = j; i < 2 * fftFrameSize; i += le) {
				tr = *p2r * ur - *p2i * ui;
				ti = *p2r * ui + *p2i * ur;
				*p2r = *p1r - tr;
				*p2i = *p1i - ti;
				*p1r += tr;
				*p1i += ti;
				p1r += le;
				p1i += le;
				p2r += le;
				p2i += le;
			}
			tr = ur * wr - ui * wi;
			ui = ur * wi + ui * wr;
			ur = tr;
		}
	}
}

void AudioEffectSpectrumAnalyzerInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	uint64_t time = OS::get_singleton()->get_ticks_usec();

	//copy everything over first, since this only really does capture
	for (int i = 0; i < p_frame_count; i++) {
		p_dst_frames[i] = p_src_frames[i];
	}

	//capture spectrum
	while (p_frame_count) {
		int to_fill = fft_size * 2 - temporal_fft_pos;
		to_fill = MIN(to_fill, p_frame_count);
		const double to_fill_step = Math_TAU / (double)fft_size;

		float *fftw = temporal_fft.ptrw();
		for (int i = 0; i < to_fill; i++) { //left and right buffers
			float window = -0.5 * Math::cos(to_fill_step * (double)temporal_fft_pos) + 0.5;
			fftw[temporal_fft_pos * 2] = window * p_src_frames->left;
			fftw[temporal_fft_pos * 2 + 1] = 0;
			fftw[(temporal_fft_pos + fft_size * 2) * 2] = window * p_src_frames->right;
			fftw[(temporal_fft_pos + fft_size * 2) * 2 + 1] = 0;
			++p_src_frames;
			++temporal_fft_pos;
		}

		p_frame_count -= to_fill;

		if (temporal_fft_pos == fft_size * 2) {
			//time to do a FFT
			smbFft(fftw, fft_size * 2, -1);
			smbFft(fftw + fft_size * 4, fft_size * 2, -1);
			int next = (fft_pos + 1) % fft_count;

			AudioFrame *hw = (AudioFrame *)fft_history[next].ptr(); //do not use write, avoid cow

			for (int i = 0; i < fft_size; i++) {
				//abs(vec)/fft_size normalizes each frequency
				hw[i].left = Vector2(fftw[i * 2], fftw[i * 2 + 1]).length() / float(fft_size);
				hw[i].right = Vector2(fftw[fft_size * 4 + i * 2], fftw[fft_size * 4 + i * 2 + 1]).length() / float(fft_size);
			}

			fft_pos = next; //swap
			temporal_fft_pos = 0;
		}
	}

	//determine time of capture
	double remainder_sec = (temporal_fft_pos / mix_rate); //subtract remainder from mix time
	last_fft_time = time - uint64_t(remainder_sec * 1000000.0);
}

void AudioEffectSpectrumAnalyzerInstance::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_magnitude_for_frequency_range", "from_hz", "to_hz", "mode"), &AudioEffectSpectrumAnalyzerInstance::get_magnitude_for_frequency_range, DEFVAL(MAGNITUDE_MAX));
	BIND_ENUM_CONSTANT(MAGNITUDE_AVERAGE);
	BIND_ENUM_CONSTANT(MAGNITUDE_MAX);
}

Vector2 AudioEffectSpectrumAnalyzerInstance::get_magnitude_for_frequency_range(float p_begin, float p_end, MagnitudeMode p_mode) const {
	if (last_fft_time == 0) {
		return Vector2();
	}
	uint64_t time = OS::get_singleton()->get_ticks_usec();
	float diff = double(time - last_fft_time) / 1000000.0 + base->get_tap_back_pos();
	diff -= AudioServer::get_singleton()->get_output_latency();
	float fft_time_size = float(fft_size) / mix_rate;

	int fft_index = fft_pos;

	while (diff > fft_time_size) {
		diff -= fft_time_size;
		fft_index -= 1;
		if (fft_index < 0) {
			fft_index = fft_count - 1;
		}
	}

	int begin_pos = p_begin * fft_size / (mix_rate * 0.5);
	int end_pos = p_end * fft_size / (mix_rate * 0.5);

	begin_pos = CLAMP(begin_pos, 0, fft_size - 1);
	end_pos = CLAMP(end_pos, 0, fft_size - 1);

	if (begin_pos > end_pos) {
		SWAP(begin_pos, end_pos);
	}
	const AudioFrame *r = fft_history[fft_index].ptr();

	if (p_mode == MAGNITUDE_AVERAGE) {
		Vector2 avg;

		for (int i = begin_pos; i <= end_pos; i++) {
			avg += Vector2(r[i]);
		}

		avg /= float(end_pos - begin_pos + 1);

		return avg;
	} else {
		Vector2 max;

		for (int i = begin_pos; i <= end_pos; i++) {
			max.x = MAX(max.x, r[i].left);
			max.y = MAX(max.y, r[i].right);
		}

		return max;
	}
}

Ref<AudioEffectInstance> AudioEffectSpectrumAnalyzer::instantiate() {
	Ref<AudioEffectSpectrumAnalyzerInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectSpectrumAnalyzer>(this);
	static const int fft_sizes[FFT_SIZE_MAX] = { 256, 512, 1024, 2048, 4096 };
	ins->fft_size = fft_sizes[fft_size];
	ins->mix_rate = AudioServer::get_singleton()->get_mix_rate();
	ins->fft_count = (buffer_length / (float(ins->fft_size) / ins->mix_rate)) + 1;
	ins->fft_pos = 0;
	ins->last_fft_time = 0;
	ins->fft_history.resize(ins->fft_count);
	ins->temporal_fft.resize(ins->fft_size * 8); //x2 stereo, x2 amount of samples for freqs, x2 for input
	ins->temporal_fft_pos = 0;
	for (int i = 0; i < ins->fft_count; i++) {
		ins->fft_history.write[i].resize(ins->fft_size); //only magnitude matters
		for (int j = 0; j < ins->fft_size; j++) {
			ins->fft_history.write[i].write[j] = AudioFrame(0, 0);
		}
	}
	return ins;
}

void AudioEffectSpectrumAnalyzer::set_buffer_length(float p_seconds) {
	buffer_length = p_seconds;
}

float AudioEffectSpectrumAnalyzer::get_buffer_length() const {
	return buffer_length;
}

void AudioEffectSpectrumAnalyzer::set_tap_back_pos(float p_seconds) {
	tapback_pos = p_seconds;
}

float AudioEffectSpectrumAnalyzer::get_tap_back_pos() const {
	return tapback_pos;
}

void AudioEffectSpectrumAnalyzer::set_fft_size(FFTSize p_fft_size) {
	ERR_FAIL_INDEX(p_fft_size, FFT_SIZE_MAX);
	fft_size = p_fft_size;
}

AudioEffectSpectrumAnalyzer::FFTSize AudioEffectSpectrumAnalyzer::get_fft_size() const {
	return fft_size;
}

void AudioEffectSpectrumAnalyzer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_buffer_length", "seconds"), &AudioEffectSpectrumAnalyzer::set_buffer_length);
	ClassDB::bind_method(D_METHOD("get_buffer_length"), &AudioEffectSpectrumAnalyzer::get_buffer_length);

	ClassDB::bind_method(D_METHOD("set_tap_back_pos", "seconds"), &AudioEffectSpectrumAnalyzer::set_tap_back_pos);
	ClassDB::bind_method(D_METHOD("get_tap_back_pos"), &AudioEffectSpectrumAnalyzer::get_tap_back_pos);

	ClassDB::bind_method(D_METHOD("set_fft_size", "size"), &AudioEffectSpectrumAnalyzer::set_fft_size);
	ClassDB::bind_method(D_METHOD("get_fft_size"), &AudioEffectSpectrumAnalyzer::get_fft_size);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "buffer_length", PROPERTY_HINT_RANGE, "0.1,4,0.1,suffix:s"), "set_buffer_length", "get_buffer_length");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tap_back_pos", PROPERTY_HINT_RANGE, "0.1,4,0.1"), "set_tap_back_pos", "get_tap_back_pos");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fft_size", PROPERTY_HINT_ENUM, "256,512,1024,2048,4096"), "set_fft_size", "get_fft_size");

	BIND_ENUM_CONSTANT(FFT_SIZE_256);
	BIND_ENUM_CONSTANT(FFT_SIZE_512);
	BIND_ENUM_CONSTANT(FFT_SIZE_1024);
	BIND_ENUM_CONSTANT(FFT_SIZE_2048);
	BIND_ENUM_CONSTANT(FFT_SIZE_4096);
	BIND_ENUM_CONSTANT(FFT_SIZE_MAX);
}

AudioEffectSpectrumAnalyzer::AudioEffectSpectrumAnalyzer() {
	buffer_length = 2;
	tapback_pos = 0.01;
	fft_size = FFT_SIZE_1024;
}
