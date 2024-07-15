/**************************************************************************/
/*  audio_effect_pitch_shift.cpp                                          */
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

#include "audio_effect_pitch_shift.h"

#include "core/math/math_funcs.h"
#include "servers/audio_server.h"

/* Thirdparty code, so disable clang-format with Godot style */
/* clang-format off */

/****************************************************************************
*
* NAME: smbPitchShift.cpp
* VERSION: 1.2
* HOME URL: https://blogs.zynaptiq.com/bernsee
* KNOWN BUGS: none
*
* SYNOPSIS: Routine for doing pitch shifting while maintaining
* duration using the Short Time Fourier Transform.
*
* DESCRIPTION: The routine takes a pitchShift factor value which is between 0.5
* (one octave down) and 2. (one octave up). A value of exactly 1 does not change
* the pitch. numSampsToProcess tells the routine how many samples in indata[0...
* numSampsToProcess-1] should be pitch shifted and moved to outdata[0 ...
* numSampsToProcess-1]. The two buffers can be identical (ie. it can process the
* data in-place). fftFrameSize defines the FFT frame size used for the
* processing. Typical values are 1024, 2048 and 4096. It may be any value <=
* MAX_FRAME_LENGTH but it MUST be a power of 2. osamp is the STFT
* oversampling factor which also determines the overlap between adjacent STFT
* frames. It should at least be 4 for moderate scaling ratios. A value of 32 is
* recommended for best quality. sampleRate takes the sample rate for the signal
* in unit Hz, ie. 44100 for 44.1 kHz audio. The data passed to the routine in
* indata[] should be in the range [-1.0, 1.0), which is also the output range
* for the data, make sure you scale the data accordingly (for 16bit signed integers
* you would have to divide (and multiply) by 32768).
*
* COPYRIGHT 1999-2015 Stephan M. Bernsee <s.bernsee [AT] zynaptiq [DOT] com>
*
* 						The Wide Open License (WOL)
*
* Permission to use, copy, modify, distribute and sell this software and its
* documentation for any purpose is hereby granted without fee, provided that
* the above copyright notice and this license appear in all source copies.
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF
* ANY KIND. See https://dspguru.com/wide-open-license/ for more information.
*
*****************************************************************************/

void SMBPitchShift::PitchShift(float pitchShift, long numSampsToProcess, long fftFrameSize, long osamp, float sampleRate, float *indata, float *outdata,int stride) {


	/*
		Routine smbPitchShift(). See top of file for explanation
		Purpose: doing pitch shifting while maintaining duration using the Short
		Time Fourier Transform.
		Author: (c)1999-2015 Stephan M. Bernsee <s.bernsee [AT] zynaptiq [DOT] com>
	*/

	double magn, phase, tmp, window, real, imag;
	double freqPerBin, expct;
	long i,k, qpd, index, inFifoLatency, stepSize, fftFrameSize2;

	/* set up some handy variables */
	fftFrameSize2 = fftFrameSize/2;
	stepSize = fftFrameSize/osamp;
	freqPerBin = sampleRate/(double)fftFrameSize;
	expct = 2.*Math_PI*(double)stepSize/(double)fftFrameSize;
	inFifoLatency = fftFrameSize-stepSize;
	if (gRover == 0) { gRover = inFifoLatency;
}

	/* initialize our static arrays */

	/* main processing loop */
	for (i = 0; i < numSampsToProcess; i++){
		/* As long as we have not yet collected enough data just read in */
		gInFIFO[gRover] = indata[i*stride];
		outdata[i*stride] = gOutFIFO[gRover-inFifoLatency];
		gRover++;

		/* now we have enough data for processing */
		if (gRover >= fftFrameSize) {
			gRover = inFifoLatency;

			/* do windowing and re,im interleave */
			for (k = 0; k < fftFrameSize;k++) {
				window = -.5*Math::cos(2.*Math_PI*(double)k/(double)fftFrameSize)+.5;
				gFFTworksp[2*k] = gInFIFO[k] * window;
				gFFTworksp[2*k+1] = 0.;
			}


			/* ***************** ANALYSIS ******************* */
			/* do transform */
			smbFft(gFFTworksp, fftFrameSize, -1);

			/* this is the analysis step */
			for (k = 0; k <= fftFrameSize2; k++) {
				/* de-interlace FFT buffer */
				real = gFFTworksp[2*k];
				imag = gFFTworksp[2*k+1];

				/* compute magnitude and phase */
				magn = 2.*Math::sqrt(real*real + imag*imag);
				phase = Math::atan2(imag,real);

				/* compute phase difference */
				tmp = phase - gLastPhase[k];
				gLastPhase[k] = phase;

				/* subtract expected phase difference */
				tmp -= (double)k*expct;

				/* map delta phase into +/- Pi interval */
				qpd = tmp/Math_PI;
				if (qpd >= 0) { qpd += qpd&1;
				} else { qpd -= qpd&1;
}
				tmp -= Math_PI*(double)qpd;

				/* get deviation from bin frequency from the +/- Pi interval */
				tmp = osamp*tmp/(2.*Math_PI);

				/* compute the k-th partials' true frequency */
				tmp = (double)k*freqPerBin + tmp*freqPerBin;

				/* store magnitude and true frequency in analysis arrays */
				gAnaMagn[k] = magn;
				gAnaFreq[k] = tmp;

			}

			/* ***************** PROCESSING ******************* */
			/* this does the actual pitch shifting */
			memset(gSynMagn, 0, fftFrameSize*sizeof(float));
			memset(gSynFreq, 0, fftFrameSize*sizeof(float));
			for (k = 0; k <= fftFrameSize2; k++) {
				index = k*pitchShift;
				if (index <= fftFrameSize2) {
					gSynMagn[index] += gAnaMagn[k];
					gSynFreq[index] = gAnaFreq[k] * pitchShift;
				}
			}

			/* ***************** SYNTHESIS ******************* */
			/* this is the synthesis step */
			for (k = 0; k <= fftFrameSize2; k++) {
				/* get magnitude and true frequency from synthesis arrays */
				magn = gSynMagn[k];
				tmp = gSynFreq[k];

				/* subtract bin mid frequency */
				tmp -= (double)k*freqPerBin;

				/* get bin deviation from freq deviation */
				tmp /= freqPerBin;

				/* take osamp into account */
				tmp = 2.*Math_PI*tmp/osamp;

				/* add the overlap phase advance back in */
				tmp += (double)k*expct;

				/* accumulate delta phase to get bin phase */
				gSumPhase[k] += tmp;
				phase = gSumPhase[k];

				/* get real and imag part and re-interleave */
				gFFTworksp[2*k] = magn*Math::cos(phase);
				gFFTworksp[2*k+1] = magn*Math::sin(phase);
			}

			/* zero negative frequencies */
			for (k = fftFrameSize+2; k < 2*fftFrameSize; k++) { gFFTworksp[k] = 0.;
}

			/* do inverse transform */
			smbFft(gFFTworksp, fftFrameSize, 1);

			/* do windowing and add to output accumulator */
			for(k=0; k < fftFrameSize; k++) {
				window = -.5*Math::cos(2.*Math_PI*(double)k/(double)fftFrameSize)+.5;
				gOutputAccum[k] += 2.*window*gFFTworksp[2*k]/(fftFrameSize2*osamp);
			}
			for (k = 0; k < stepSize; k++) { gOutFIFO[k] = gOutputAccum[k];
}

			/* shift accumulator */
			memmove(gOutputAccum, gOutputAccum+stepSize, fftFrameSize*sizeof(float));

			/* move input FIFO */
			for (k = 0; k < inFifoLatency; k++) { gInFIFO[k] = gInFIFO[k+stepSize];
}
		}
	}
}



void SMBPitchShift::smbFft(float *fftBuffer, long fftFrameSize, long sign)
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

	for (i = 2; i < 2*fftFrameSize-2; i += 2) {
		for (bitm = 2, j = 0; bitm < 2*fftFrameSize; bitm <<= 1) {
			if (i & bitm) { j++;
}
			j <<= 1;
		}
		if (i < j) {
			p1 = fftBuffer+i; p2 = fftBuffer+j;
			temp = *p1; *(p1++) = *p2;
			*(p2++) = temp; temp = *p1;
			*p1 = *p2; *p2 = temp;
		}
	}
	for (k = 0, le = 2; k < (long)(Math::log((double)fftFrameSize)/Math::log(2.)+.5); k++) {
		le <<= 1;
		le2 = le>>1;
		ur = 1.0;
		ui = 0.0;
		arg = Math_PI / (le2>>1);
		wr = Math::cos(arg);
		wi = sign*Math::sin(arg);
		for (j = 0; j < le2; j += 2) {
			p1r = fftBuffer+j; p1i = p1r+1;
			p2r = p1r+le2; p2i = p2r+1;
			for (i = j; i < 2*fftFrameSize; i += le) {
				tr = *p2r * ur - *p2i * ui;
				ti = *p2r * ui + *p2i * ur;
				*p2r = *p1r - tr; *p2i = *p1i - ti;
				*p1r += tr; *p1i += ti;
				p1r += le; p1i += le;
				p2r += le; p2i += le;
			}
			tr = ur*wr - ui*wi;
			ui = ur*wi + ui*wr;
			ur = tr;
		}
	}
}


/* Godot code again */
/* clang-format on */

void AudioEffectPitchShiftInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	// Avoid distortion by skipping processing if pitch_scale is 1.0.
	if (Math::is_equal_approx(base->pitch_scale, 1.0f)) {
		return;
	}

	float sample_rate = AudioServer::get_singleton()->get_mix_rate();

	float *in_l = (float *)p_src_frames;
	float *in_r = in_l + 1;

	float *out_l = (float *)p_dst_frames;
	float *out_r = out_l + 1;

	shift_l.PitchShift(base->pitch_scale, p_frame_count, fft_size, base->oversampling, sample_rate, in_l, out_l, 2);
	shift_r.PitchShift(base->pitch_scale, p_frame_count, fft_size, base->oversampling, sample_rate, in_r, out_r, 2);
}

Ref<AudioEffectInstance> AudioEffectPitchShift::instantiate() {
	Ref<AudioEffectPitchShiftInstance> ins;
	ins.instantiate();
	ins->base = Ref<AudioEffectPitchShift>(this);
	static const int fft_sizes[FFT_SIZE_MAX] = { 256, 512, 1024, 2048, 4096 };
	ins->fft_size = fft_sizes[fft_size];

	return ins;
}

void AudioEffectPitchShift::set_pitch_scale(float p_pitch_scale) {
	ERR_FAIL_COND(!(p_pitch_scale > 0.0));
	pitch_scale = p_pitch_scale;
}

float AudioEffectPitchShift::get_pitch_scale() const {
	return pitch_scale;
}

void AudioEffectPitchShift::set_oversampling(int p_oversampling) {
	ERR_FAIL_COND(p_oversampling < 4);
	oversampling = p_oversampling;
}

int AudioEffectPitchShift::get_oversampling() const {
	return oversampling;
}

void AudioEffectPitchShift::set_fft_size(FFTSize p_fft_size) {
	ERR_FAIL_INDEX(p_fft_size, FFT_SIZE_MAX);
	fft_size = p_fft_size;
}

AudioEffectPitchShift::FFTSize AudioEffectPitchShift::get_fft_size() const {
	return fft_size;
}

void AudioEffectPitchShift::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pitch_scale", "rate"), &AudioEffectPitchShift::set_pitch_scale);
	ClassDB::bind_method(D_METHOD("get_pitch_scale"), &AudioEffectPitchShift::get_pitch_scale);

	ClassDB::bind_method(D_METHOD("set_oversampling", "amount"), &AudioEffectPitchShift::set_oversampling);
	ClassDB::bind_method(D_METHOD("get_oversampling"), &AudioEffectPitchShift::get_oversampling);

	ClassDB::bind_method(D_METHOD("set_fft_size", "size"), &AudioEffectPitchShift::set_fft_size);
	ClassDB::bind_method(D_METHOD("get_fft_size"), &AudioEffectPitchShift::get_fft_size);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_scale", PROPERTY_HINT_RANGE, "0.01,16,0.01"), "set_pitch_scale", "get_pitch_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "oversampling", PROPERTY_HINT_RANGE, "4,32,1"), "set_oversampling", "get_oversampling");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fft_size", PROPERTY_HINT_ENUM, "256,512,1024,2048,4096"), "set_fft_size", "get_fft_size");

	BIND_ENUM_CONSTANT(FFT_SIZE_256);
	BIND_ENUM_CONSTANT(FFT_SIZE_512);
	BIND_ENUM_CONSTANT(FFT_SIZE_1024);
	BIND_ENUM_CONSTANT(FFT_SIZE_2048);
	BIND_ENUM_CONSTANT(FFT_SIZE_4096);
	BIND_ENUM_CONSTANT(FFT_SIZE_MAX);
}
