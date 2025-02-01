/**************************************************************************/
/*  audio_stream_wav.h                                                    */
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

#include "servers/audio/audio_stream.h"

#include "thirdparty/misc/qoa.h"

class AudioStreamWAV;

class AudioStreamPlaybackWAV : public AudioStreamPlayback {
	GDCLASS(AudioStreamPlaybackWAV, AudioStreamPlayback);
	enum {
		MIX_FRAC_BITS = 13,
		MIX_FRAC_LEN = (1 << MIX_FRAC_BITS),
		MIX_FRAC_MASK = MIX_FRAC_LEN - 1,
	};

	struct IMA_ADPCM_State {
		int16_t step_index = 0;
		int32_t predictor = 0;
		/* values at loop point */
		int16_t loop_step_index = 0;
		int32_t loop_predictor = 0;
		int32_t last_nibble = 0;
		int32_t loop_pos = 0;
		int32_t window_ofs = 0;
	} ima_adpcm[2];

	struct QOA_State {
		qoa_desc desc = {};
		uint32_t data_ofs = 0;
		uint32_t frame_len = 0;
		LocalVector<int16_t> dec;
		uint32_t dec_len = 0;
		int64_t cache_pos = -1;
		int16_t cache[2] = { 0, 0 };
		int16_t cache_r[2] = { 0, 0 };
	} qoa;

	int64_t offset = 0;
	int sign = 1;
	bool active = false;
	friend class AudioStreamWAV;
	Ref<AudioStreamWAV> base;

	template <typename Depth, bool is_stereo, bool is_ima_adpcm, bool is_qoa>
	void do_resample(const Depth *p_src, AudioFrame *p_dst, int64_t &p_offset, int32_t &p_increment, uint32_t p_amount, IMA_ADPCM_State *p_ima_adpcm, QOA_State *p_qoa);

	bool _is_sample = false;
	Ref<AudioSamplePlayback> sample_playback;

public:
	virtual void start(double p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;

	virtual int get_loop_count() const override; //times it looped

	virtual double get_playback_position() const override;
	virtual void seek(double p_time) override;

	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;

	virtual void tag_used_streams() override;

	virtual void set_is_sample(bool p_is_sample) override;
	virtual bool get_is_sample() const override;
	virtual Ref<AudioSamplePlayback> get_sample_playback() const override;
	virtual void set_sample_playback(const Ref<AudioSamplePlayback> &p_playback) override;

	AudioStreamPlaybackWAV();
	~AudioStreamPlaybackWAV();
};

class AudioStreamWAV : public AudioStream {
	GDCLASS(AudioStreamWAV, AudioStream);
	RES_BASE_EXTENSION("sample")

public:
	enum Format {
		FORMAT_8_BITS,
		FORMAT_16_BITS,
		FORMAT_IMA_ADPCM,
		FORMAT_QOA,
	};

	// Keep the ResourceImporterWAV `edit/loop_mode` enum hint in sync with these options.
	enum LoopMode {
		LOOP_DISABLED,
		LOOP_FORWARD,
		LOOP_PINGPONG,
		LOOP_BACKWARD
	};

private:
	friend class AudioStreamPlaybackWAV;

	enum {
		DATA_PAD = 16 //padding for interpolation
	};

	Format format = FORMAT_8_BITS;
	LoopMode loop_mode = LOOP_DISABLED;
	bool stereo = false;
	int loop_begin = 0;
	int loop_end = 0;
	int mix_rate = 44100;
	LocalVector<uint8_t> data;
	uint32_t data_bytes = 0;

protected:
	static void _bind_methods();

public:
	static Ref<AudioStreamWAV> load_from_buffer(const Vector<uint8_t> &p_stream_data, const Dictionary &p_options);
	static Ref<AudioStreamWAV> load_from_file(const String &p_path, const Dictionary &p_options);

	void set_format(Format p_format);
	Format get_format() const;

	void set_loop_mode(LoopMode p_loop_mode);
	LoopMode get_loop_mode() const;

	void set_loop_begin(int p_frame);
	int get_loop_begin() const;

	void set_loop_end(int p_frame);
	int get_loop_end() const;

	void set_mix_rate(int p_hz);
	int get_mix_rate() const;

	void set_stereo(bool p_enable);
	bool is_stereo() const;

	virtual double get_length() const override; //if supported, otherwise return 0

	virtual bool is_monophonic() const override;

	void set_data(const Vector<uint8_t> &p_data);
	Vector<uint8_t> get_data() const;

	Error save_to_wav(const String &p_path);

	virtual Ref<AudioStreamPlayback> instantiate_playback() override;
	virtual String get_stream_name() const override;

	virtual bool can_be_sampled() const override {
		return true;
	}
	virtual Ref<AudioSample> generate_sample() const override;

	static void _compress_ima_adpcm(const Vector<float> &p_data, Vector<uint8_t> &r_dst_data) {
		static const int16_t _ima_adpcm_step_table[89] = {
			7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
			19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
			50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
			130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
			337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
			876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
			2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
			5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
			15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
		};

		static const int8_t _ima_adpcm_index_table[16] = {
			-1, -1, -1, -1, 2, 4, 6, 8,
			-1, -1, -1, -1, 2, 4, 6, 8
		};

		int datalen = p_data.size();
		int datamax = datalen;
		if (datalen & 1) {
			datalen++;
		}

		r_dst_data.resize(datalen / 2 + 4);
		uint8_t *w = r_dst_data.ptrw();

		int i, step_idx = 0, prev = 0;
		uint8_t *out = w;
		const float *in = p_data.ptr();

		// Initial value is zero.
		*(out++) = 0;
		*(out++) = 0;
		// Table index initial value.
		*(out++) = 0;
		// Unused.
		*(out++) = 0;

		for (i = 0; i < datalen; i++) {
			int step, diff, vpdiff, mask;
			uint8_t nibble;
			int16_t xm_sample;

			if (i >= datamax) {
				xm_sample = 0;
			} else {
				xm_sample = CLAMP(in[i] * 32767.0, -32768, 32767);
			}

			diff = (int)xm_sample - prev;

			nibble = 0;
			step = _ima_adpcm_step_table[step_idx];
			vpdiff = step >> 3;
			if (diff < 0) {
				nibble = 8;
				diff = -diff;
			}
			mask = 4;
			while (mask) {
				if (diff >= step) {
					nibble |= mask;
					diff -= step;
					vpdiff += step;
				}

				step >>= 1;
				mask >>= 1;
			}

			if (nibble & 8) {
				prev -= vpdiff;
			} else {
				prev += vpdiff;
			}

			prev = CLAMP(prev, -32768, 32767);

			step_idx += _ima_adpcm_index_table[nibble];
			step_idx = CLAMP(step_idx, 0, 88);

			if (i & 1) {
				*out |= nibble << 4;
				out++;
			} else {
				*out = nibble;
			}
		}
	}

	static void _compress_qoa(const Vector<float> &p_data, Vector<uint8_t> &dst_data, qoa_desc *p_desc) {
		uint32_t frames_len = (p_desc->samples + QOA_FRAME_LEN - 1) / QOA_FRAME_LEN * (QOA_LMS_LEN * 4 * p_desc->channels + 8);
		uint32_t slices_len = (p_desc->samples + QOA_SLICE_LEN - 1) / QOA_SLICE_LEN * 8 * p_desc->channels;
		dst_data.resize(8 + frames_len + slices_len);

		for (uint32_t c = 0; c < p_desc->channels; c++) {
			memset(p_desc->lms[c].history, 0, sizeof(p_desc->lms[c].history));
			memset(p_desc->lms[c].weights, 0, sizeof(p_desc->lms[c].weights));
			p_desc->lms[c].weights[2] = -(1 << 13);
			p_desc->lms[c].weights[3] = (1 << 14);
		}

		LocalVector<int16_t> data16;
		data16.resize(QOA_FRAME_LEN * p_desc->channels);

		uint8_t *dst_ptr = dst_data.ptrw();
		dst_ptr += qoa_encode_header(p_desc, dst_data.ptrw());

		uint32_t frame_len = QOA_FRAME_LEN;
		for (uint32_t s = 0; s < p_desc->samples; s += frame_len) {
			frame_len = MIN(frame_len, p_desc->samples - s);
			for (uint32_t i = 0; i < frame_len * p_desc->channels; i++) {
				data16[i] = CLAMP(p_data[s * p_desc->channels + i] * 32767.0, -32768, 32767);
			}
			dst_ptr += qoa_encode_frame(data16.ptr(), p_desc, frame_len, dst_ptr);
		}
	}

	AudioStreamWAV();
	~AudioStreamWAV();
};

VARIANT_ENUM_CAST(AudioStreamWAV::Format)
VARIANT_ENUM_CAST(AudioStreamWAV::LoopMode)
