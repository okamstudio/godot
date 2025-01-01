/**************************************************************************/
/*  audio_rb_resampler.cpp                                                */
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

#include "audio_rb_resampler.h"

#include "core/math/audio_frame.h"
#include "core/os/memory.h"

int AudioRBResampler::get_channel_count() const {
	if (!rb) {
		return 0;
	}

	return channels;
}

// Linear interpolation based sample rate conversion (low quality)
// Note that AudioStreamPlaybackResampled::mix has better algorithm,
// but it wasn't obvious to integrate that with VideoStreamPlayer
template <int C>
uint32_t AudioRBResampler::_resample(AudioFrame *p_dest, int p_todo, int32_t p_increment) {
	uint32_t read = offset & MIX_FRAC_MASK;

	for (int i = 0; i < p_todo; i++) {
		offset = (offset + p_increment) & (((1 << (rb_bits + MIX_FRAC_BITS)) - 1));
		read += p_increment;
		uint32_t pos = offset >> MIX_FRAC_BITS;
		float frac = float(offset & MIX_FRAC_MASK) / float(MIX_FRAC_LEN);
		ERR_FAIL_COND_V(pos >= rb_len, 0);
		uint32_t pos_next = (pos + 1) & rb_mask;

		// since this is a template with a known compile time value (C), conditionals go away when compiling.
		if constexpr (C == 1) {
			float v0 = rb[pos];
			float v0n = rb[pos_next];
			v0 += (v0n - v0) * frac;
			p_dest[i] = AudioFrame(v0, v0);
		}

		if constexpr (C == 2) {
			float v0 = rb[(pos << 1) + 0];
			float v1 = rb[(pos << 1) + 1];
			float v0n = rb[(pos_next << 1) + 0];
			float v1n = rb[(pos_next << 1) + 1];

			v0 += (v0n - v0) * frac;
			v1 += (v1n - v1) * frac;
			p_dest[i] = AudioFrame(v0, v1);
		}

		// This will probably never be used, but added anyway
		if constexpr (C == 4) {
			float v0 = rb[(pos << 2) + 0];
			float v1 = rb[(pos << 2) + 1];
			float v0n = rb[(pos_next << 2) + 0];
			float v1n = rb[(pos_next << 2) + 1];
			v0 += (v0n - v0) * frac;
			v1 += (v1n - v1) * frac;
			p_dest[i] = AudioFrame(v0, v1);
		}

		if constexpr (C == 6) {
			float v0 = rb[(pos * 6) + 0];
			float v1 = rb[(pos * 6) + 1];
			float v0n = rb[(pos_next * 6) + 0];
			float v1n = rb[(pos_next * 6) + 1];

			v0 += (v0n - v0) * frac;
			v1 += (v1n - v1) * frac;
			p_dest[i] = AudioFrame(v0, v1);
		}
	}

	return read >> MIX_FRAC_BITS; //rb_read_pos = offset >> MIX_FRAC_BITS;
}

bool AudioRBResampler::mix(AudioFrame *p_dest, int p_frames) {
	if (!rb) {
		return false;
	}

	int32_t increment = (src_mix_rate * MIX_FRAC_LEN) / target_mix_rate;
	int read_space = get_reader_space();
	int target_todo = MIN(get_num_of_ready_frames(), p_frames);

	{
		int src_read = 0;
		switch (channels) {
			case 1:
				src_read = _resample<1>(p_dest, target_todo, increment);
				break;
			case 2:
				src_read = _resample<2>(p_dest, target_todo, increment);
				break;
			case 4:
				src_read = _resample<4>(p_dest, target_todo, increment);
				break;
			case 6:
				src_read = _resample<6>(p_dest, target_todo, increment);
				break;
		}

		if (src_read > read_space) {
			src_read = read_space;
		}

		rb_read_pos.set((rb_read_pos.get() + src_read) & rb_mask);

		// Create fadeout effect for the end of stream (note that it can be because of slow writer)
		if (p_frames - target_todo > 0) {
			for (int i = 0; i < target_todo; i++) {
				p_dest[i] = p_dest[i] * float(target_todo - i) / float(target_todo);
			}
		}

		// Fill zeros (silence) for the rest of frames
		for (int i = target_todo; i < p_frames; i++) {
			p_dest[i] = AudioFrame(0, 0);
		}
	}

	return true;
}

int AudioRBResampler::get_num_of_ready_frames() {
	if (!is_ready()) {
		return 0;
	}
	int32_t increment = (src_mix_rate * MIX_FRAC_LEN) / target_mix_rate;
	int read_space = get_reader_space();
	return (int64_t(read_space) << MIX_FRAC_BITS) / increment;
}

Error AudioRBResampler::setup(int p_channels, int p_src_mix_rate, int p_target_mix_rate, int p_buffer_msec, int p_minbuff_needed) {
	ERR_FAIL_COND_V(p_channels != 1 && p_channels != 2 && p_channels != 4 && p_channels != 6, ERR_INVALID_PARAMETER);

	int desired_rb_bits = nearest_shift(MAX((p_buffer_msec / 1000.0) * p_src_mix_rate, p_minbuff_needed));

	bool recreate = !rb;

	if (rb && (uint32_t(desired_rb_bits) != rb_bits || channels != uint32_t(p_channels))) {
		memdelete_arr(rb);
		memdelete_arr(read_buf);
		recreate = true;
	}

	if (recreate) {
		channels = p_channels;
		rb_bits = desired_rb_bits;
		rb_len = (1 << rb_bits);
		rb_mask = rb_len - 1;
		const size_t array_size = rb_len * (size_t)p_channels;
		rb = memnew_arr(float, array_size);
		read_buf = memnew_arr(float, array_size);
	}

	src_mix_rate = p_src_mix_rate;
	target_mix_rate = p_target_mix_rate;
	offset = 0;
	rb_read_pos.set(0);
	rb_write_pos.set(0);

	//avoid maybe strange noises upon load
	for (unsigned int i = 0; i < (rb_len * channels); i++) {
		rb[i] = 0;
		read_buf[i] = 0;
	}

	return OK;
}

void AudioRBResampler::clear() {
	if (!rb) {
		return;
	}

	//should be stopped at this point but just in case
	memdelete_arr(rb);
	memdelete_arr(read_buf);
	rb = nullptr;
	offset = 0;
	rb_read_pos.set(0);
	rb_write_pos.set(0);
	read_buf = nullptr;
}

AudioRBResampler::AudioRBResampler() {
	rb = nullptr;
	offset = 0;
	read_buf = nullptr;
	rb_read_pos.set(0);
	rb_write_pos.set(0);

	rb_bits = 0;
	rb_len = 0;
	rb_mask = 0;
	read_buff_len = 0;
	channels = 0;
	src_mix_rate = 0;
	target_mix_rate = 0;
}

AudioRBResampler::~AudioRBResampler() {
	if (rb) {
		memdelete_arr(rb);
		memdelete_arr(read_buf);
	}
}
