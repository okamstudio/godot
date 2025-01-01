/**************************************************************************/
/*  audio_stream_wav.cpp                                                  */
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

#include "audio_stream_wav.h"

#include "core/io/file_access_memory.h"
#include "core/io/marshalls.h"

const float TRIM_DB_LIMIT = -50;
const int TRIM_FADE_OUT_FRAMES = 500;

void AudioStreamPlaybackWAV::start(double p_from_pos) {
	if (base->format == AudioStreamWAV::FORMAT_IMA_ADPCM) {
		//no seeking in IMA_ADPCM
		for (int i = 0; i < 2; i++) {
			ima_adpcm[i].step_index = 0;
			ima_adpcm[i].predictor = 0;
			ima_adpcm[i].loop_step_index = 0;
			ima_adpcm[i].loop_predictor = 0;
			ima_adpcm[i].last_nibble = -1;
			ima_adpcm[i].loop_pos = 0x7FFFFFFF;
			ima_adpcm[i].window_ofs = 0;
		}

		offset = 0;
	} else {
		seek(p_from_pos);
	}

	sign = 1;
	active = true;
}

void AudioStreamPlaybackWAV::stop() {
	active = false;
}

bool AudioStreamPlaybackWAV::is_playing() const {
	return active;
}

int AudioStreamPlaybackWAV::get_loop_count() const {
	return 0;
}

double AudioStreamPlaybackWAV::get_playback_position() const {
	return float(offset >> MIX_FRAC_BITS) / base->mix_rate;
}

void AudioStreamPlaybackWAV::seek(double p_time) {
	if (base->format == AudioStreamWAV::FORMAT_IMA_ADPCM) {
		return; //no seeking in ima-adpcm
	}

	double max = base->get_length();
	if (p_time < 0) {
		p_time = 0;
	} else if (p_time >= max) {
		p_time = max - 0.001;
	}

	offset = uint64_t(p_time * base->mix_rate) << MIX_FRAC_BITS;
}

template <typename Depth, bool is_stereo, bool is_ima_adpcm, bool is_qoa>
void AudioStreamPlaybackWAV::do_resample(const Depth *p_src, AudioFrame *p_dst, int64_t &p_offset, int32_t &p_increment, uint32_t p_amount, IMA_ADPCM_State *p_ima_adpcm, QOA_State *p_qoa) {
	// this function will be compiled branchless by any decent compiler

	int32_t final = 0, final_r = 0, next = 0, next_r = 0;
	while (p_amount) {
		p_amount--;
		int64_t pos = p_offset >> MIX_FRAC_BITS;
		if (is_stereo && !is_ima_adpcm && !is_qoa) {
			pos <<= 1;
		}

		if (is_ima_adpcm) {
			int64_t sample_pos = pos + p_ima_adpcm[0].window_ofs;

			while (sample_pos > p_ima_adpcm[0].last_nibble) {
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

				for (int i = 0; i < (is_stereo ? 2 : 1); i++) {
					int16_t nibble, diff, step;

					p_ima_adpcm[i].last_nibble++;

					uint8_t nbb = p_src[(p_ima_adpcm[i].last_nibble >> 1) * (is_stereo ? 2 : 1) + i];
					nibble = (p_ima_adpcm[i].last_nibble & 1) ? (nbb >> 4) : (nbb & 0xF);
					step = _ima_adpcm_step_table[p_ima_adpcm[i].step_index];

					p_ima_adpcm[i].step_index += _ima_adpcm_index_table[nibble];
					if (p_ima_adpcm[i].step_index < 0) {
						p_ima_adpcm[i].step_index = 0;
					}
					if (p_ima_adpcm[i].step_index > 88) {
						p_ima_adpcm[i].step_index = 88;
					}

					diff = step >> 3;
					if (nibble & 1) {
						diff += step >> 2;
					}
					if (nibble & 2) {
						diff += step >> 1;
					}
					if (nibble & 4) {
						diff += step;
					}
					if (nibble & 8) {
						diff = -diff;
					}

					p_ima_adpcm[i].predictor += diff;
					if (p_ima_adpcm[i].predictor < -0x8000) {
						p_ima_adpcm[i].predictor = -0x8000;
					} else if (p_ima_adpcm[i].predictor > 0x7FFF) {
						p_ima_adpcm[i].predictor = 0x7FFF;
					}

					/* store loop if there */
					if (p_ima_adpcm[i].last_nibble == p_ima_adpcm[i].loop_pos) {
						p_ima_adpcm[i].loop_step_index = p_ima_adpcm[i].step_index;
						p_ima_adpcm[i].loop_predictor = p_ima_adpcm[i].predictor;
					}

					//printf("%i - %i - pred %i\n",int(p_ima_adpcm[i].last_nibble),int(nibble),int(p_ima_adpcm[i].predictor));
				}
			}

			final = p_ima_adpcm[0].predictor;
			if (is_stereo) {
				final_r = p_ima_adpcm[1].predictor;
			}

		} else {
			if (is_qoa) {
				if (pos != p_qoa->cache_pos) { // Prevents triple decoding on lower mix rates.
					for (int i = 0; i < 2; i++) {
						// Sign operations prevent triple decoding on backward loops, maxing prevents pop.
						uint32_t interp_pos = MIN(pos + (i * sign) + (sign < 0), p_qoa->desc.samples - 1);
						uint32_t new_data_ofs = 8 + interp_pos / QOA_FRAME_LEN * p_qoa->frame_len;

						if (p_qoa->data_ofs != new_data_ofs) {
							p_qoa->data_ofs = new_data_ofs;
							const uint8_t *ofs_src = (uint8_t *)p_src + p_qoa->data_ofs;
							qoa_decode_frame(ofs_src, p_qoa->frame_len, &p_qoa->desc, p_qoa->dec.ptr(), &p_qoa->dec_len);
						}

						uint32_t dec_idx = (interp_pos % QOA_FRAME_LEN) * p_qoa->desc.channels;

						if ((sign > 0 && i == 0) || (sign < 0 && i == 1)) {
							final = p_qoa->dec[dec_idx];
							p_qoa->cache[0] = final;
							if (is_stereo) {
								final_r = p_qoa->dec[dec_idx + 1];
								p_qoa->cache_r[0] = final_r;
							}
						} else {
							next = p_qoa->dec[dec_idx];
							p_qoa->cache[1] = next;
							if (is_stereo) {
								next_r = p_qoa->dec[dec_idx + 1];
								p_qoa->cache_r[1] = next_r;
							}
						}
					}
					p_qoa->cache_pos = pos;
				} else {
					final = p_qoa->cache[0];
					if (is_stereo) {
						final_r = p_qoa->cache_r[0];
					}

					next = p_qoa->cache[1];
					if (is_stereo) {
						next_r = p_qoa->cache_r[1];
					}
				}
			} else {
				final = p_src[pos];
				if (is_stereo) {
					final_r = p_src[pos + 1];
				}

				if constexpr (sizeof(Depth) == 1) { /* conditions will not exist anymore when compiled! */
					final <<= 8;
					if (is_stereo) {
						final_r <<= 8;
					}
				}

				if (is_stereo) {
					next = p_src[pos + 2];
					next_r = p_src[pos + 3];
				} else {
					next = p_src[pos + 1];
				}

				if constexpr (sizeof(Depth) == 1) {
					next <<= 8;
					if (is_stereo) {
						next_r <<= 8;
					}
				}
			}
			int32_t frac = int64_t(p_offset & MIX_FRAC_MASK);

			final = final + ((next - final) * frac >> MIX_FRAC_BITS);
			if (is_stereo) {
				final_r = final_r + ((next_r - final_r) * frac >> MIX_FRAC_BITS);
			}
		}

		if (!is_stereo) {
			final_r = final; //copy to right channel if stereo
		}

		p_dst->left = final / 32767.0;
		p_dst->right = final_r / 32767.0;
		p_dst++;

		p_offset += p_increment;
	}
}

int AudioStreamPlaybackWAV::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (base->data.is_empty() || !active) {
		for (int i = 0; i < p_frames; i++) {
			p_buffer[i] = AudioFrame(0, 0);
		}
		return 0;
	}

	int len = base->data_bytes;
	switch (base->format) {
		case AudioStreamWAV::FORMAT_8_BITS:
			len /= 1;
			break;
		case AudioStreamWAV::FORMAT_16_BITS:
			len /= 2;
			break;
		case AudioStreamWAV::FORMAT_IMA_ADPCM:
			len *= 2;
			break;
		case AudioStreamWAV::FORMAT_QOA:
			len = qoa.desc.samples * qoa.desc.channels;
			break;
	}

	if (base->stereo) {
		len /= 2;
	}

	/* some 64-bit fixed point precaches */

	int64_t loop_begin_fp = ((int64_t)base->loop_begin << MIX_FRAC_BITS);
	int64_t loop_end_fp = ((int64_t)base->loop_end << MIX_FRAC_BITS);
	int64_t length_fp = ((int64_t)len << MIX_FRAC_BITS);
	int64_t begin_limit = (base->loop_mode != AudioStreamWAV::LOOP_DISABLED) ? loop_begin_fp : 0;
	int64_t end_limit = (base->loop_mode != AudioStreamWAV::LOOP_DISABLED) ? loop_end_fp : length_fp - MIX_FRAC_LEN;
	bool is_stereo = base->stereo;

	int32_t todo = p_frames;

	if (base->loop_mode == AudioStreamWAV::LOOP_BACKWARD) {
		sign = -1;
	}

	float base_rate = AudioServer::get_singleton()->get_mix_rate();
	float srate = base->mix_rate;
	srate *= p_rate_scale;
	float playback_speed_scale = AudioServer::get_singleton()->get_playback_speed_scale();
	float fincrement = (srate * playback_speed_scale) / base_rate;
	int32_t increment = int32_t(MAX(fincrement * MIX_FRAC_LEN, 1));
	increment *= sign;

	//looping

	AudioStreamWAV::LoopMode loop_format = base->loop_mode;
	AudioStreamWAV::Format format = base->format;

	/* audio data */

	const uint8_t *data = base->data.ptr() + AudioStreamWAV::DATA_PAD;
	AudioFrame *dst_buff = p_buffer;

	if (format == AudioStreamWAV::FORMAT_IMA_ADPCM) {
		if (loop_format != AudioStreamWAV::LOOP_DISABLED) {
			ima_adpcm[0].loop_pos = loop_begin_fp >> MIX_FRAC_BITS;
			ima_adpcm[1].loop_pos = loop_begin_fp >> MIX_FRAC_BITS;
			loop_format = AudioStreamWAV::LOOP_FORWARD;
		}
	}

	while (todo > 0) {
		int64_t limit = 0;
		int32_t target = 0, aux = 0;

		/** LOOP CHECKING **/

		if (increment < 0) {
			/* going backwards */

			if (loop_format != AudioStreamWAV::LOOP_DISABLED && offset < loop_begin_fp) {
				/* loopstart reached */
				if (loop_format == AudioStreamWAV::LOOP_PINGPONG) {
					/* bounce ping pong */
					offset = loop_begin_fp + (loop_begin_fp - offset);
					increment = -increment;
					sign *= -1;
				} else {
					/* go to loop-end */
					offset = loop_end_fp - (loop_begin_fp - offset);
				}
			} else {
				/* check for sample not reaching beginning */
				if (offset < 0) {
					active = false;
					break;
				}
			}
		} else {
			/* going forward */
			if (loop_format != AudioStreamWAV::LOOP_DISABLED && offset >= loop_end_fp) {
				/* loopend reached */

				if (loop_format == AudioStreamWAV::LOOP_PINGPONG) {
					/* bounce ping pong */
					offset = loop_end_fp - (offset - loop_end_fp);
					increment = -increment;
					sign *= -1;
				} else {
					/* go to loop-begin */

					if (format == AudioStreamWAV::FORMAT_IMA_ADPCM) {
						for (int i = 0; i < 2; i++) {
							ima_adpcm[i].step_index = ima_adpcm[i].loop_step_index;
							ima_adpcm[i].predictor = ima_adpcm[i].loop_predictor;
							ima_adpcm[i].last_nibble = loop_begin_fp >> MIX_FRAC_BITS;
						}
						offset = loop_begin_fp;
					} else {
						offset = loop_begin_fp + (offset - loop_end_fp);
					}
				}
			} else {
				/* no loop, check for end of sample */
				if (offset >= length_fp) {
					active = false;
					break;
				}
			}
		}

		/** MIXCOUNT COMPUTING **/

		/* next possible limit (looppoints or sample begin/end */
		limit = (increment < 0) ? begin_limit : end_limit;

		/* compute what is shorter, the todo or the limit? */
		aux = (limit - offset) / increment + 1;
		target = (aux < todo) ? aux : todo; /* mix target is the shorter buffer */

		/* check just in case */
		if (target <= 0) {
			active = false;
			break;
		}

		todo -= target;

		switch (base->format) {
			case AudioStreamWAV::FORMAT_8_BITS: {
				if (is_stereo) {
					do_resample<int8_t, true, false, false>((int8_t *)data, dst_buff, offset, increment, target, ima_adpcm, &qoa);
				} else {
					do_resample<int8_t, false, false, false>((int8_t *)data, dst_buff, offset, increment, target, ima_adpcm, &qoa);
				}
			} break;
			case AudioStreamWAV::FORMAT_16_BITS: {
				if (is_stereo) {
					do_resample<int16_t, true, false, false>((int16_t *)data, dst_buff, offset, increment, target, ima_adpcm, &qoa);
				} else {
					do_resample<int16_t, false, false, false>((int16_t *)data, dst_buff, offset, increment, target, ima_adpcm, &qoa);
				}

			} break;
			case AudioStreamWAV::FORMAT_IMA_ADPCM: {
				if (is_stereo) {
					do_resample<int8_t, true, true, false>((int8_t *)data, dst_buff, offset, increment, target, ima_adpcm, &qoa);
				} else {
					do_resample<int8_t, false, true, false>((int8_t *)data, dst_buff, offset, increment, target, ima_adpcm, &qoa);
				}

			} break;
			case AudioStreamWAV::FORMAT_QOA: {
				if (is_stereo) {
					do_resample<uint8_t, true, false, true>((uint8_t *)data, dst_buff, offset, increment, target, ima_adpcm, &qoa);
				} else {
					do_resample<uint8_t, false, false, true>((uint8_t *)data, dst_buff, offset, increment, target, ima_adpcm, &qoa);
				}
			} break;
		}

		dst_buff += target;
	}

	if (todo) {
		int mixed_frames = p_frames - todo;
		//bit was missing from mix
		int todo_ofs = p_frames - todo;
		for (int i = todo_ofs; i < p_frames; i++) {
			p_buffer[i] = AudioFrame(0, 0);
		}
		return mixed_frames;
	}
	return p_frames;
}

void AudioStreamPlaybackWAV::tag_used_streams() {
	base->tag_used(get_playback_position());
}

void AudioStreamPlaybackWAV::set_is_sample(bool p_is_sample) {
	_is_sample = p_is_sample;
}

bool AudioStreamPlaybackWAV::get_is_sample() const {
	return _is_sample;
}

Ref<AudioSamplePlayback> AudioStreamPlaybackWAV::get_sample_playback() const {
	return sample_playback;
}

void AudioStreamPlaybackWAV::set_sample_playback(const Ref<AudioSamplePlayback> &p_playback) {
	sample_playback = p_playback;
	if (sample_playback.is_valid()) {
		sample_playback->stream_playback = Ref<AudioStreamPlayback>(this);
	}
}

AudioStreamPlaybackWAV::AudioStreamPlaybackWAV() {}

AudioStreamPlaybackWAV::~AudioStreamPlaybackWAV() {}

/////////////////////

void AudioStreamWAV::set_format(Format p_format) {
	format = p_format;
}

AudioStreamWAV::Format AudioStreamWAV::get_format() const {
	return format;
}

void AudioStreamWAV::set_loop_mode(LoopMode p_loop_mode) {
	loop_mode = p_loop_mode;
}

AudioStreamWAV::LoopMode AudioStreamWAV::get_loop_mode() const {
	return loop_mode;
}

void AudioStreamWAV::set_loop_begin(int p_frame) {
	loop_begin = p_frame;
}

int AudioStreamWAV::get_loop_begin() const {
	return loop_begin;
}

void AudioStreamWAV::set_loop_end(int p_frame) {
	loop_end = p_frame;
}

int AudioStreamWAV::get_loop_end() const {
	return loop_end;
}

void AudioStreamWAV::set_mix_rate(int p_hz) {
	ERR_FAIL_COND(p_hz == 0);
	mix_rate = p_hz;
}

int AudioStreamWAV::get_mix_rate() const {
	return mix_rate;
}

void AudioStreamWAV::set_stereo(bool p_enable) {
	stereo = p_enable;
}

bool AudioStreamWAV::is_stereo() const {
	return stereo;
}

double AudioStreamWAV::get_length() const {
	int len = data_bytes;
	switch (format) {
		case AudioStreamWAV::FORMAT_8_BITS:
			len /= 1;
			break;
		case AudioStreamWAV::FORMAT_16_BITS:
			len /= 2;
			break;
		case AudioStreamWAV::FORMAT_IMA_ADPCM:
			len *= 2;
			break;
		case AudioStreamWAV::FORMAT_QOA:
			qoa_desc desc = {};
			qoa_decode_header(data.ptr() + DATA_PAD, data_bytes, &desc);
			len = desc.samples * desc.channels;
			break;
	}

	if (stereo) {
		len /= 2;
	}

	return double(len) / mix_rate;
}

bool AudioStreamWAV::is_monophonic() const {
	return false;
}

void AudioStreamWAV::set_data(const Vector<uint8_t> &p_data) {
	AudioServer::get_singleton()->lock();

	int src_data_len = p_data.size();

	data.clear();

	int alloc_len = src_data_len + DATA_PAD * 2;
	data.resize(alloc_len);
	memset(data.ptr(), 0, alloc_len);
	memcpy(data.ptr() + DATA_PAD, p_data.ptr(), src_data_len);
	data_bytes = src_data_len;

	AudioServer::get_singleton()->unlock();
}

Vector<uint8_t> AudioStreamWAV::get_data() const {
	Vector<uint8_t> pv;

	if (data_bytes) {
		pv.resize(data_bytes);
		memcpy(pv.ptrw(), data.ptr() + DATA_PAD, data_bytes);
	}

	return pv;
}

Error AudioStreamWAV::save_to_wav(const String &p_path) {
	if (format == AudioStreamWAV::FORMAT_IMA_ADPCM || format == AudioStreamWAV::FORMAT_QOA) {
		WARN_PRINT("Saving IMA_ADPCM and QOA samples is not supported yet");
		return ERR_UNAVAILABLE;
	}

	int sub_chunk_2_size = data_bytes; //Subchunk2Size = Size of data in bytes

	// Format code
	// 1:PCM format (for 8 or 16 bit)
	// 3:IEEE float format
	int format_code = (format == FORMAT_IMA_ADPCM) ? 3 : 1;

	int n_channels = stereo ? 2 : 1;

	long sample_rate = mix_rate;

	int byte_pr_sample = 0;
	switch (format) {
		case AudioStreamWAV::FORMAT_8_BITS:
			byte_pr_sample = 1;
			break;
		case AudioStreamWAV::FORMAT_16_BITS:
		case AudioStreamWAV::FORMAT_QOA:
			byte_pr_sample = 2;
			break;
		case AudioStreamWAV::FORMAT_IMA_ADPCM:
			byte_pr_sample = 4;
			break;
	}

	String file_path = p_path;
	if (file_path.substr(file_path.length() - 4, 4).to_lower() != ".wav") {
		file_path += ".wav";
	}

	Ref<FileAccess> file = FileAccess::open(file_path, FileAccess::WRITE); //Overrides existing file if present

	ERR_FAIL_COND_V(file.is_null(), ERR_FILE_CANT_WRITE);

	// Create WAV Header
	file->store_string("RIFF"); //ChunkID
	file->store_32(sub_chunk_2_size + 36); //ChunkSize = 36 + SubChunk2Size (size of entire file minus the 8 bits for this and previous header)
	file->store_string("WAVE"); //Format
	file->store_string("fmt "); //Subchunk1ID
	file->store_32(16); //Subchunk1Size = 16
	file->store_16(format_code); //AudioFormat
	file->store_16(n_channels); //Number of Channels
	file->store_32(sample_rate); //SampleRate
	file->store_32(sample_rate * n_channels * byte_pr_sample); //ByteRate
	file->store_16(n_channels * byte_pr_sample); //BlockAlign = NumChannels * BytePrSample
	file->store_16(byte_pr_sample * 8); //BitsPerSample
	file->store_string("data"); //Subchunk2ID
	file->store_32(sub_chunk_2_size); //Subchunk2Size

	// Add data
	Vector<uint8_t> stream_data = get_data();
	const uint8_t *read_data = stream_data.ptr();
	switch (format) {
		case AudioStreamWAV::FORMAT_8_BITS:
			for (unsigned int i = 0; i < data_bytes; i++) {
				uint8_t data_point = (read_data[i] + 128);
				file->store_8(data_point);
			}
			break;
		case AudioStreamWAV::FORMAT_16_BITS:
		case AudioStreamWAV::FORMAT_QOA:
			for (unsigned int i = 0; i < data_bytes / 2; i++) {
				uint16_t data_point = decode_uint16(&read_data[i * 2]);
				file->store_16(data_point);
			}
			break;
		case AudioStreamWAV::FORMAT_IMA_ADPCM:
			//Unimplemented
			break;
	}

	return OK;
}

Ref<AudioStreamPlayback> AudioStreamWAV::instantiate_playback() {
	Ref<AudioStreamPlaybackWAV> sample;
	sample.instantiate();
	sample->base = Ref<AudioStreamWAV>(this);

	if (format == AudioStreamWAV::FORMAT_QOA) {
		uint32_t ffp = qoa_decode_header(data.ptr() + DATA_PAD, data_bytes, &sample->qoa.desc);
		ERR_FAIL_COND_V(ffp != 8, Ref<AudioStreamPlaybackWAV>());
		sample->qoa.frame_len = qoa_max_frame_size(&sample->qoa.desc);
		int samples_len = (sample->qoa.desc.samples > QOA_FRAME_LEN ? QOA_FRAME_LEN : sample->qoa.desc.samples);
		int dec_len = sample->qoa.desc.channels * samples_len;
		sample->qoa.dec.resize(dec_len);
	}

	return sample;
}

String AudioStreamWAV::get_stream_name() const {
	return "";
}

Ref<AudioSample> AudioStreamWAV::generate_sample() const {
	Ref<AudioSample> sample;
	sample.instantiate();
	sample->stream = this;
	switch (loop_mode) {
		case AudioStreamWAV::LoopMode::LOOP_DISABLED: {
			sample->loop_mode = AudioSample::LoopMode::LOOP_DISABLED;
		} break;

		case AudioStreamWAV::LoopMode::LOOP_FORWARD: {
			sample->loop_mode = AudioSample::LoopMode::LOOP_FORWARD;
		} break;

		case AudioStreamWAV::LoopMode::LOOP_PINGPONG: {
			sample->loop_mode = AudioSample::LoopMode::LOOP_PINGPONG;
		} break;

		case AudioStreamWAV::LoopMode::LOOP_BACKWARD: {
			sample->loop_mode = AudioSample::LoopMode::LOOP_BACKWARD;
		} break;
	}
	sample->loop_begin = loop_begin;
	sample->loop_end = loop_end;
	sample->sample_rate = mix_rate;
	return sample;
}

void AudioStreamWAV::_bind_methods() {
	ClassDB::bind_static_method("AudioStreamWAV", D_METHOD("load_from_file", "path", "options"), &AudioStreamWAV::load_from_file, DEFVAL(Dictionary()));
	ClassDB::bind_static_method("AudioStreamWAV", D_METHOD("load_from_buffer", "buffer", "options"), &AudioStreamWAV::load_from_buffer, DEFVAL(Dictionary()));

	ClassDB::bind_method(D_METHOD("set_data", "data"), &AudioStreamWAV::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &AudioStreamWAV::get_data);

	ClassDB::bind_method(D_METHOD("set_format", "format"), &AudioStreamWAV::set_format);
	ClassDB::bind_method(D_METHOD("get_format"), &AudioStreamWAV::get_format);

	ClassDB::bind_method(D_METHOD("set_loop_mode", "loop_mode"), &AudioStreamWAV::set_loop_mode);
	ClassDB::bind_method(D_METHOD("get_loop_mode"), &AudioStreamWAV::get_loop_mode);

	ClassDB::bind_method(D_METHOD("set_loop_begin", "loop_begin"), &AudioStreamWAV::set_loop_begin);
	ClassDB::bind_method(D_METHOD("get_loop_begin"), &AudioStreamWAV::get_loop_begin);

	ClassDB::bind_method(D_METHOD("set_loop_end", "loop_end"), &AudioStreamWAV::set_loop_end);
	ClassDB::bind_method(D_METHOD("get_loop_end"), &AudioStreamWAV::get_loop_end);

	ClassDB::bind_method(D_METHOD("set_mix_rate", "mix_rate"), &AudioStreamWAV::set_mix_rate);
	ClassDB::bind_method(D_METHOD("get_mix_rate"), &AudioStreamWAV::get_mix_rate);

	ClassDB::bind_method(D_METHOD("set_stereo", "stereo"), &AudioStreamWAV::set_stereo);
	ClassDB::bind_method(D_METHOD("is_stereo"), &AudioStreamWAV::is_stereo);

	ClassDB::bind_method(D_METHOD("save_to_wav", "path"), &AudioStreamWAV::save_to_wav);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_data", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "format", PROPERTY_HINT_ENUM, "8-Bit,16-Bit,IMA ADPCM,Quite OK Audio"), "set_format", "get_format");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loop_mode", PROPERTY_HINT_ENUM, "Disabled,Forward,Ping-Pong,Backward"), "set_loop_mode", "get_loop_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loop_begin"), "set_loop_begin", "get_loop_begin");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loop_end"), "set_loop_end", "get_loop_end");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mix_rate"), "set_mix_rate", "get_mix_rate");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stereo"), "set_stereo", "is_stereo");

	BIND_ENUM_CONSTANT(FORMAT_8_BITS);
	BIND_ENUM_CONSTANT(FORMAT_16_BITS);
	BIND_ENUM_CONSTANT(FORMAT_IMA_ADPCM);
	BIND_ENUM_CONSTANT(FORMAT_QOA);

	BIND_ENUM_CONSTANT(LOOP_DISABLED);
	BIND_ENUM_CONSTANT(LOOP_FORWARD);
	BIND_ENUM_CONSTANT(LOOP_PINGPONG);
	BIND_ENUM_CONSTANT(LOOP_BACKWARD);
}

Ref<AudioStreamWAV> AudioStreamWAV::load_from_buffer(const Vector<uint8_t> &p_file_data, const Dictionary &p_options) {
	// /* STEP 1, READ WAVE FILE */

	Ref<FileAccessMemory> file;
	file.instantiate();
	Error err = file->open_custom(p_file_data.ptr(), p_file_data.size());
	ERR_FAIL_COND_V_MSG(err != OK, Ref<AudioStreamWAV>(), "Cannot create memfile for WAV file buffer.");

	/* CHECK RIFF */
	char riff[5];
	riff[4] = 0;
	file->get_buffer((uint8_t *)&riff, 4); //RIFF

	if (riff[0] != 'R' || riff[1] != 'I' || riff[2] != 'F' || riff[3] != 'F') {
		ERR_FAIL_V_MSG(Ref<AudioStreamWAV>(), vformat("Not a WAV file. File should start with 'RIFF', but found '%s', in file of size %d bytes", riff, file->get_length()));
	}

	/* GET FILESIZE */

	// The file size in header is 8 bytes less than the actual size.
	// See https://docs.fileformat.com/audio/wav/
	const int FILE_SIZE_HEADER_OFFSET = 8;
	uint32_t file_size_header = file->get_32() + FILE_SIZE_HEADER_OFFSET;
	uint64_t file_size = file->get_length();
	if (file_size != file_size_header) {
		WARN_PRINT(vformat("File size %d is %s than the expected size %d.", file_size, file_size > file_size_header ? "larger" : "smaller", file_size_header));
	}

	/* CHECK WAVE */

	char wave[5];
	wave[4] = 0;
	file->get_buffer((uint8_t *)&wave, 4); //WAVE

	if (wave[0] != 'W' || wave[1] != 'A' || wave[2] != 'V' || wave[3] != 'E') {
		ERR_FAIL_V_MSG(Ref<AudioStreamWAV>(), vformat("Not a WAV file. Header should contain 'WAVE', but found '%s', in file of size %d bytes", wave, file->get_length()));
	}

	// Let users override potential loop points from the WAV.
	// We parse the WAV loop points only with "Detect From WAV" (0).
	int import_loop_mode = p_options["edit/loop_mode"];

	int format_bits = 0;
	int format_channels = 0;

	AudioStreamWAV::LoopMode loop_mode = AudioStreamWAV::LOOP_DISABLED;
	uint16_t compression_code = 1;
	bool format_found = false;
	bool data_found = false;
	int format_freq = 0;
	int loop_begin = 0;
	int loop_end = 0;
	int frames = 0;

	Vector<float> data;

	while (!file->eof_reached()) {
		/* chunk */
		char chunk_id[4];
		file->get_buffer((uint8_t *)&chunk_id, 4); //RIFF

		/* chunk size */
		uint32_t chunksize = file->get_32();
		uint32_t file_pos = file->get_position(); //save file pos, so we can skip to next chunk safely

		if (file->eof_reached()) {
			//ERR_PRINT("EOF REACH");
			break;
		}

		if (chunk_id[0] == 'f' && chunk_id[1] == 'm' && chunk_id[2] == 't' && chunk_id[3] == ' ' && !format_found) {
			/* IS FORMAT CHUNK */

			//Issue: #7755 : Not a bug - usage of other formats (format codes) are unsupported in current importer version.
			//Consider revision for engine version 3.0
			compression_code = file->get_16();
			if (compression_code != 1 && compression_code != 3) {
				ERR_FAIL_V_MSG(Ref<AudioStreamWAV>(), "Format not supported for WAVE file (not PCM). Save WAVE files as uncompressed PCM or IEEE float instead.");
			}

			format_channels = file->get_16();
			if (format_channels != 1 && format_channels != 2) {
				ERR_FAIL_V_MSG(Ref<AudioStreamWAV>(), "Format not supported for WAVE file (not stereo or mono).");
			}

			format_freq = file->get_32(); //sampling rate

			file->get_32(); // average bits/second (unused)
			file->get_16(); // block align (unused)
			format_bits = file->get_16(); // bits per sample

			if (format_bits % 8 || format_bits == 0) {
				ERR_FAIL_V_MSG(Ref<AudioStreamWAV>(), "Invalid amount of bits in the sample (should be one of 8, 16, 24 or 32).");
			}

			if (compression_code == 3 && format_bits % 32) {
				ERR_FAIL_V_MSG(Ref<AudioStreamWAV>(), "Invalid amount of bits in the IEEE float sample (should be 32 or 64).");
			}

			/* Don't need anything else, continue */
			format_found = true;
		}

		if (chunk_id[0] == 'd' && chunk_id[1] == 'a' && chunk_id[2] == 't' && chunk_id[3] == 'a' && !data_found) {
			/* IS DATA CHUNK */
			data_found = true;

			if (!format_found) {
				ERR_PRINT("'data' chunk before 'format' chunk found.");
				break;
			}

			uint64_t remaining_bytes = file_size - file_pos;
			frames = chunksize;
			if (remaining_bytes < chunksize) {
				WARN_PRINT("Data chunk size is smaller than expected. Proceeding with actual data size.");
				frames = remaining_bytes;
			}

			ERR_FAIL_COND_V(format_channels == 0, Ref<AudioStreamWAV>());
			frames /= format_channels;
			frames /= (format_bits >> 3);

			/*print_line("chunksize: "+itos(chunksize));
			print_line("channels: "+itos(format_channels));
			print_line("bits: "+itos(format_bits));
			*/

			data.resize(frames * format_channels);

			if (compression_code == 1) {
				if (format_bits == 8) {
					for (int i = 0; i < frames * format_channels; i++) {
						// 8 bit samples are UNSIGNED

						data.write[i] = int8_t(file->get_8() - 128) / 128.f;
					}
				} else if (format_bits == 16) {
					for (int i = 0; i < frames * format_channels; i++) {
						//16 bit SIGNED

						data.write[i] = int16_t(file->get_16()) / 32768.f;
					}
				} else {
					for (int i = 0; i < frames * format_channels; i++) {
						//16+ bits samples are SIGNED
						// if sample is > 16 bits, just read extra bytes

						uint32_t s = 0;
						for (int b = 0; b < (format_bits >> 3); b++) {
							s |= ((uint32_t)file->get_8()) << (b * 8);
						}
						s <<= (32 - format_bits);

						data.write[i] = (int32_t(s) >> 16) / 32768.f;
					}
				}
			} else if (compression_code == 3) {
				if (format_bits == 32) {
					for (int i = 0; i < frames * format_channels; i++) {
						//32 bit IEEE Float

						data.write[i] = file->get_float();
					}
				} else if (format_bits == 64) {
					for (int i = 0; i < frames * format_channels; i++) {
						//64 bit IEEE Float

						data.write[i] = file->get_double();
					}
				}
			}

			// This is commented out due to some weird edge case seemingly in FileAccessMemory, doesn't seem to have any side effects though.
			// if (file->eof_reached()) {
			// 	ERR_FAIL_V_MSG(Ref<AudioStreamWAV>(), "Premature end of file.");
			// }
		}

		if (import_loop_mode == 0 && chunk_id[0] == 's' && chunk_id[1] == 'm' && chunk_id[2] == 'p' && chunk_id[3] == 'l') {
			// Loop point info!

			/**
			 *	Consider exploring next document:
			 *		http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/RIFFNEW.pdf
			 *	Especially on page:
			 *		16 - 17
			 *	Timestamp:
			 *		22:38 06.07.2017 GMT
			 **/

			for (int i = 0; i < 10; i++) {
				file->get_32(); // i wish to know why should i do this... no doc!
			}

			// only read 0x00 (loop forward), 0x01 (loop ping-pong) and 0x02 (loop backward)
			// Skip anything else because it's not supported, reserved for future uses or sampler specific
			// from https://sites.google.com/site/musicgapi/technical-documents/wav-file-format#smpl (loop type values table)
			int loop_type = file->get_32();
			if (loop_type == 0x00 || loop_type == 0x01 || loop_type == 0x02) {
				if (loop_type == 0x00) {
					loop_mode = AudioStreamWAV::LOOP_FORWARD;
				} else if (loop_type == 0x01) {
					loop_mode = AudioStreamWAV::LOOP_PINGPONG;
				} else if (loop_type == 0x02) {
					loop_mode = AudioStreamWAV::LOOP_BACKWARD;
				}
				loop_begin = file->get_32();
				loop_end = file->get_32();
			}
		}
		// Move to the start of the next chunk. Note that RIFF requires a padding byte for odd
		// chunk sizes.
		file->seek(file_pos + chunksize + (chunksize & 1));
	}

	// STEP 2, APPLY CONVERSIONS

	bool is16 = format_bits != 8;
	int rate = format_freq;

	/*
	print_line("Input Sample: ");
	print_line("\tframes: " + itos(frames));
	print_line("\tformat_channels: " + itos(format_channels));
	print_line("\t16bits: " + itos(is16));
	print_line("\trate: " + itos(rate));
	print_line("\tloop: " + itos(loop));
	print_line("\tloop begin: " + itos(loop_begin));
	print_line("\tloop end: " + itos(loop_end));
	*/

	//apply frequency limit

	bool limit_rate = p_options["force/max_rate"];
	int limit_rate_hz = p_options["force/max_rate_hz"];
	if (limit_rate && rate > limit_rate_hz && rate > 0 && frames > 0) {
		// resample!
		int new_data_frames = (int)(frames * (float)limit_rate_hz / (float)rate);

		Vector<float> new_data;
		new_data.resize(new_data_frames * format_channels);
		for (int c = 0; c < format_channels; c++) {
			float frac = 0.0;
			int ipos = 0;

			for (int i = 0; i < new_data_frames; i++) {
				// Cubic interpolation should be enough.

				float y0 = data[MAX(0, ipos - 1) * format_channels + c];
				float y1 = data[ipos * format_channels + c];
				float y2 = data[MIN(frames - 1, ipos + 1) * format_channels + c];
				float y3 = data[MIN(frames - 1, ipos + 2) * format_channels + c];

				new_data.write[i * format_channels + c] = Math::cubic_interpolate(y1, y2, y0, y3, frac);

				// update position and always keep fractional part within ]0...1]
				// in order to avoid 32bit floating point precision errors

				frac += (float)rate / (float)limit_rate_hz;
				int tpos = (int)Math::floor(frac);
				ipos += tpos;
				frac -= tpos;
			}
		}

		if (loop_mode) {
			loop_begin = (int)(loop_begin * (float)new_data_frames / (float)frames);
			loop_end = (int)(loop_end * (float)new_data_frames / (float)frames);
		}

		data = new_data;
		rate = limit_rate_hz;
		frames = new_data_frames;
	}

	bool normalize = p_options["edit/normalize"];

	if (normalize) {
		float max = 0.0;
		for (int i = 0; i < data.size(); i++) {
			float amp = Math::abs(data[i]);
			if (amp > max) {
				max = amp;
			}
		}

		if (max > 0) {
			float mult = 1.0 / max;
			for (int i = 0; i < data.size(); i++) {
				data.write[i] *= mult;
			}
		}
	}

	bool trim = p_options["edit/trim"];

	if (trim && (loop_mode == AudioStreamWAV::LOOP_DISABLED) && format_channels > 0) {
		int first = 0;
		int last = (frames / format_channels) - 1;
		bool found = false;
		float limit = Math::db_to_linear(TRIM_DB_LIMIT);

		for (int i = 0; i < data.size() / format_channels; i++) {
			float amp_channel_sum = 0.0;
			for (int j = 0; j < format_channels; j++) {
				amp_channel_sum += Math::abs(data[(i * format_channels) + j]);
			}

			float amp = Math::abs(amp_channel_sum / (float)format_channels);

			if (!found && amp > limit) {
				first = i;
				found = true;
			}

			if (found && amp > limit) {
				last = i;
			}
		}

		if (first < last) {
			Vector<float> new_data;
			new_data.resize((last - first) * format_channels);
			for (int i = first; i < last; i++) {
				float fade_out_mult = 1.0;

				if (last - i < TRIM_FADE_OUT_FRAMES) {
					fade_out_mult = ((float)(last - i - 1) / (float)TRIM_FADE_OUT_FRAMES);
				}

				for (int j = 0; j < format_channels; j++) {
					new_data.write[((i - first) * format_channels) + j] = data[(i * format_channels) + j] * fade_out_mult;
				}
			}

			data = new_data;
			frames = data.size() / format_channels;
		}
	}

	if (import_loop_mode >= 2) {
		loop_mode = (AudioStreamWAV::LoopMode)(import_loop_mode - 1);
		loop_begin = p_options["edit/loop_begin"];
		loop_end = p_options["edit/loop_end"];
		// Wrap around to max frames, so `-1` can be used to select the end, etc.
		if (loop_begin < 0) {
			loop_begin = CLAMP(loop_begin + frames, 0, frames - 1);
		}
		if (loop_end < 0) {
			loop_end = CLAMP(loop_end + frames, 0, frames - 1);
		}
	}

	int compression = p_options["compress/mode"];
	bool force_mono = p_options["force/mono"];

	if (force_mono && format_channels == 2) {
		Vector<float> new_data;
		new_data.resize(data.size() / 2);
		for (int i = 0; i < frames; i++) {
			new_data.write[i] = (data[i * 2 + 0] + data[i * 2 + 1]) / 2.0;
		}

		data = new_data;
		format_channels = 1;
	}

	bool force_8_bit = p_options["force/8_bit"];
	if (force_8_bit) {
		is16 = false;
	}

	Vector<uint8_t> dst_data;
	AudioStreamWAV::Format dst_format;

	if (compression == 1) {
		dst_format = AudioStreamWAV::FORMAT_IMA_ADPCM;
		if (format_channels == 1) {
			_compress_ima_adpcm(data, dst_data);
		} else {
			//byte interleave
			Vector<float> left;
			Vector<float> right;

			int tframes = data.size() / 2;
			left.resize(tframes);
			right.resize(tframes);

			for (int i = 0; i < tframes; i++) {
				left.write[i] = data[i * 2 + 0];
				right.write[i] = data[i * 2 + 1];
			}

			Vector<uint8_t> bleft;
			Vector<uint8_t> bright;

			_compress_ima_adpcm(left, bleft);
			_compress_ima_adpcm(right, bright);

			int dl = bleft.size();
			dst_data.resize(dl * 2);

			uint8_t *w = dst_data.ptrw();
			const uint8_t *rl = bleft.ptr();
			const uint8_t *rr = bright.ptr();

			for (int i = 0; i < dl; i++) {
				w[i * 2 + 0] = rl[i];
				w[i * 2 + 1] = rr[i];
			}
		}

	} else if (compression == 2) {
		dst_format = AudioStreamWAV::FORMAT_QOA;

		qoa_desc desc = {};
		desc.samplerate = rate;
		desc.samples = frames;
		desc.channels = format_channels;

		_compress_qoa(data, dst_data, &desc);
	} else {
		dst_format = is16 ? AudioStreamWAV::FORMAT_16_BITS : AudioStreamWAV::FORMAT_8_BITS;
		dst_data.resize(data.size() * (is16 ? 2 : 1));
		{
			uint8_t *w = dst_data.ptrw();

			int ds = data.size();
			for (int i = 0; i < ds; i++) {
				if (is16) {
					int16_t v = CLAMP(data[i] * 32768, -32768, 32767);
					encode_uint16(v, &w[i * 2]);
				} else {
					int8_t v = CLAMP(data[i] * 128, -128, 127);
					w[i] = v;
				}
			}
		}
	}

	Ref<AudioStreamWAV> sample;
	sample.instantiate();
	sample->set_data(dst_data);
	sample->set_format(dst_format);
	sample->set_mix_rate(rate);
	sample->set_loop_mode(loop_mode);
	sample->set_loop_begin(loop_begin);
	sample->set_loop_end(loop_end);
	sample->set_stereo(format_channels == 2);
	return sample;
}

Ref<AudioStreamWAV> AudioStreamWAV::load_from_file(const String &p_path, const Dictionary &p_options) {
	Vector<uint8_t> file_data = FileAccess::get_file_as_bytes(p_path);
	ERR_FAIL_COND_V_MSG(file_data.is_empty(), Ref<AudioStreamWAV>(), vformat("Cannot open file '%s'.", p_path));
	return load_from_buffer(file_data, p_options);
}

AudioStreamWAV::AudioStreamWAV() {}

AudioStreamWAV::~AudioStreamWAV() {}
