/**************************************************************************/
/*  audio_stream_ogg_vorbis.cpp                                           */
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

#include "audio_stream_ogg_vorbis.h"

#include "modules/vorbis/resource_importer_ogg_vorbis.h"
#include <ogg/ogg.h>

int AudioStreamPlaybackOggVorbis::_mix_internal(AudioFrame *p_buffer, int p_frames) {
	ERR_FAIL_COND_V(!ready, 0);

	if (!active) {
		return 0;
	}

	int todo = p_frames;

	int beat_length_frames = -1;
	bool use_loop = looping_override ? looping : vorbis_stream->loop;

	if (use_loop && vorbis_stream->get_bpm() > 0 && vorbis_stream->get_beat_count() > 0) {
		beat_length_frames = vorbis_stream->get_beat_count() * vorbis_data->get_sampling_rate() * 60 / vorbis_stream->get_bpm();
	}

	while (todo > 0 && active) {
		AudioFrame *buffer = p_buffer;
		buffer += p_frames - todo;

		int to_mix = todo;
		if (beat_length_frames >= 0 && (beat_length_frames - (int)frames_mixed) < to_mix) {
			to_mix = MAX(0, beat_length_frames - (int)frames_mixed);
		}

		int mixed = _mix_frames_vorbis(buffer, to_mix);
		ERR_FAIL_COND_V(mixed < 0, 0);
		todo -= mixed;
		frames_mixed += mixed;

		if (loop_fade_remaining < FADE_SIZE) {
			int to_fade = loop_fade_remaining + MIN(FADE_SIZE - loop_fade_remaining, mixed);
			for (int i = loop_fade_remaining; i < to_fade; i++) {
				buffer[i - loop_fade_remaining] += loop_fade[i] * (float(FADE_SIZE - i) / float(FADE_SIZE));
			}
			loop_fade_remaining = to_fade;
		}

		if (beat_length_frames >= 0) {
			/**
			 * Length determined by beat length
			 * This code is commented out because, in practice, it is preferred that the fade
			 * is done by the transitioner and this stream just goes on until it ends while fading out.
			 *
			 * End fade implementation is left here for reference in case at some point this feature
			 * is desired.

			if (!beat_loop && (int)frames_mixed > beat_length_frames - FADE_SIZE) {
				print_line("beat length fade/after mix?");
				//No loop, just fade and finish
				for (int i = 0; i < mixed; i++) {
					int idx = frames_mixed + i - mixed;
					buffer[i] *= 1.0 - float(MAX(0, (idx - (beat_length_frames - FADE_SIZE)))) / float(FADE_SIZE);
				}
				if ((int)frames_mixed == beat_length_frames) {
					for (int i = p_frames - todo; i < p_frames; i++) {
						p_buffer[i] = AudioFrame(0, 0);
					}
					active = false;
					break;
				}
			} else
			**/

			if (use_loop && beat_length_frames <= (int)frames_mixed) {
				// End of file when doing beat-based looping. <= used instead of == because importer editing
				if (!have_packets_left && !have_samples_left) {
					//Nothing remaining, so do nothing.
					loop_fade_remaining = FADE_SIZE;
				} else {
					// Add some loop fade;
					int faded_mix = _mix_frames_vorbis(loop_fade, FADE_SIZE);

					for (int i = faded_mix; i < FADE_SIZE; i++) {
						// In case lesss was mixed, pad with zeros
						loop_fade[i] = AudioFrame(0, 0);
					}
					loop_fade_remaining = 0;
				}

				seek(vorbis_stream->loop_offset);
				loops++;
				// We still have buffer to fill, start from this element in the next iteration.
				continue;
			}
		}

		if (!have_packets_left && !have_samples_left) {
			// Actual end of file!
			bool is_not_empty = mixed > 0 || vorbis_stream->get_length() > 0;
			if (use_loop && is_not_empty) {
				//loop

				seek(vorbis_stream->loop_offset);
				loops++;
				// We still have buffer to fill, start from this element in the next iteration.

			} else {
				for (int i = p_frames - todo; i < p_frames; i++) {
					p_buffer[i] = AudioFrame(0, 0);
				}
				active = false;
			}
		}
	}
	return p_frames - todo;
}

int AudioStreamPlaybackOggVorbis::_mix_frames_vorbis(AudioFrame *p_buffer, int p_frames) {
	ERR_FAIL_COND_V(!ready, p_frames);
	if (!have_samples_left) {
		ogg_packet *packet = nullptr;
		int err;

		if (!vorbis_data_playback->next_ogg_packet(&packet)) {
			have_packets_left = false;
			WARN_PRINT("ran out of packets in stream");
			return -1;
		}

		err = vorbis_synthesis(&block, packet);
		ERR_FAIL_COND_V_MSG(err != 0, p_frames, "Error during vorbis synthesis " + itos(err));

		err = vorbis_synthesis_blockin(&dsp_state, &block);
		ERR_FAIL_COND_V_MSG(err != 0, p_frames, "Error during vorbis block processing " + itos(err));

		have_packets_left = !packet->e_o_s;
	}

	float **pcm; // Accessed with pcm[channel_idx][sample_idx].

	int frames = vorbis_synthesis_pcmout(&dsp_state, &pcm);
	if (frames > p_frames) {
		frames = p_frames;
		have_samples_left = true;
	} else {
		have_samples_left = false;
	}

	if (info.channels > 1) {
		for (int frame = 0; frame < frames; frame++) {
			p_buffer[frame].left = pcm[0][frame];
			p_buffer[frame].right = pcm[1][frame];
		}
	} else {
		for (int frame = 0; frame < frames; frame++) {
			p_buffer[frame].left = pcm[0][frame];
			p_buffer[frame].right = pcm[0][frame];
		}
	}
	vorbis_synthesis_read(&dsp_state, frames);
	return frames;
}

float AudioStreamPlaybackOggVorbis::get_stream_sampling_rate() {
	return vorbis_data->get_sampling_rate();
}

bool AudioStreamPlaybackOggVorbis::_alloc_vorbis() {
	vorbis_info_init(&info);
	info_is_allocated = true;
	vorbis_comment_init(&comment);
	comment_is_allocated = true;

	ERR_FAIL_COND_V(vorbis_data.is_null(), false);
	vorbis_data_playback = vorbis_data->instantiate_playback();

	ogg_packet *packet;
	int err;

	for (int i = 0; i < 3; i++) {
		if (!vorbis_data_playback->next_ogg_packet(&packet)) {
			WARN_PRINT("Not enough packets to parse header");
			return false;
		}

		err = vorbis_synthesis_headerin(&info, &comment, packet);
		ERR_FAIL_COND_V_MSG(err != 0, false, "Error parsing header");
	}

	err = vorbis_synthesis_init(&dsp_state, &info);
	ERR_FAIL_COND_V_MSG(err != 0, false, "Error initializing dsp state");
	dsp_state_is_allocated = true;

	err = vorbis_block_init(&dsp_state, &block);
	ERR_FAIL_COND_V_MSG(err != 0, false, "Error initializing block");
	block_is_allocated = true;

	ready = true;

	return true;
}

void AudioStreamPlaybackOggVorbis::start(double p_from_pos) {
	ERR_FAIL_COND(!ready);
	loop_fade_remaining = FADE_SIZE;
	active = true;
	seek(p_from_pos);
	loops = 0;
	begin_resample();
}

void AudioStreamPlaybackOggVorbis::stop() {
	active = false;
}

bool AudioStreamPlaybackOggVorbis::is_playing() const {
	return active;
}

int AudioStreamPlaybackOggVorbis::get_loop_count() const {
	return loops;
}

double AudioStreamPlaybackOggVorbis::get_playback_position() const {
	return double(frames_mixed) / (double)vorbis_data->get_sampling_rate();
}

void AudioStreamPlaybackOggVorbis::tag_used_streams() {
	vorbis_stream->tag_used(get_playback_position());
}

void AudioStreamPlaybackOggVorbis::set_parameter(const StringName &p_name, const Variant &p_value) {
	if (p_name == SNAME("looping")) {
		if (p_value == Variant()) {
			looping_override = false;
			looping = false;
		} else {
			looping_override = true;
			looping = p_value;
		}
	}
}

Variant AudioStreamPlaybackOggVorbis::get_parameter(const StringName &p_name) const {
	if (looping_override && p_name == SNAME("looping")) {
		return looping;
	}
	return Variant();
}

void AudioStreamPlaybackOggVorbis::seek(double p_time) {
	ERR_FAIL_COND(!ready);
	ERR_FAIL_COND(vorbis_stream.is_null());
	if (!active) {
		return;
	}

	if (p_time >= vorbis_stream->get_length()) {
		p_time = 0;
	}

	frames_mixed = uint32_t(vorbis_data->get_sampling_rate() * p_time);

	const int64_t desired_sample = p_time * get_stream_sampling_rate();

	if (!vorbis_data_playback->seek_page(desired_sample)) {
		WARN_PRINT("seek failed");
		return;
	}

	// We want to start decoding before the page that we expect the sample to be in (the sample may
	// be part of a partial packet across page boundaries). Otherwise, the decoder may not have
	// synchronized before reaching the sample.
	int64_t start_page_number = vorbis_data_playback->get_page_number() - 1;
	if (start_page_number < 0) {
		start_page_number = 0;
	}

	while (true) {
		ogg_packet *packet;
		int err;

		// We start at an unknown granule position.
		int64_t granule_pos = -1;

		// Decode data until we get to the desired sample or notice that we have read past it.
		vorbis_data_playback->set_page_number(start_page_number);
		vorbis_synthesis_restart(&dsp_state);

		while (true) {
			if (!vorbis_data_playback->next_ogg_packet(&packet)) {
				WARN_PRINT_ONCE("Seeking beyond limits");
				return;
			}

			err = vorbis_synthesis(&block, packet);
			if (err != OV_ENOTAUDIO) {
				ERR_FAIL_COND_MSG(err != 0, "Error during vorbis synthesis " + itos(err) + ".");

				err = vorbis_synthesis_blockin(&dsp_state, &block);
				ERR_FAIL_COND_MSG(err != 0, "Error during vorbis block processing " + itos(err) + ".");

				int samples_out = vorbis_synthesis_pcmout(&dsp_state, nullptr);

				if (granule_pos < 0) {
					// We don't know where we are yet, so just keep on decoding.
					err = vorbis_synthesis_read(&dsp_state, samples_out);
					ERR_FAIL_COND_MSG(err != 0, "Error during vorbis read updating " + itos(err) + ".");
				} else if (granule_pos + samples_out >= desired_sample) {
					// Our sample is in this block. Skip the beginning of the block up to the sample, then
					// return.
					int skip_samples = (int)(desired_sample - granule_pos);
					err = vorbis_synthesis_read(&dsp_state, skip_samples);
					ERR_FAIL_COND_MSG(err != 0, "Error during vorbis read updating " + itos(err) + ".");
					have_samples_left = skip_samples < samples_out;
					have_packets_left = !packet->e_o_s;
					return;
				} else {
					// Our sample is not in this block. Skip it.
					err = vorbis_synthesis_read(&dsp_state, samples_out);
					ERR_FAIL_COND_MSG(err != 0, "Error during vorbis read updating " + itos(err) + ".");
					granule_pos += samples_out;
				}
			}
			if (packet->granulepos != -1) {
				// We found an update to our granule position.
				granule_pos = packet->granulepos;
				if (granule_pos > desired_sample) {
					// We've read past our sample. We need to start on an earlier page.
					if (start_page_number == 0) {
						// We didn't find the sample even reading from the beginning.
						have_samples_left = false;
						have_packets_left = !packet->e_o_s;
						return;
					}
					start_page_number--;
					break;
				}
			}
			if (packet->e_o_s) {
				// We've reached the end of the stream and didn't find our sample.
				have_samples_left = false;
				have_packets_left = false;
				return;
			}
		}
	}
}

void AudioStreamPlaybackOggVorbis::set_is_sample(bool p_is_sample) {
	_is_sample = p_is_sample;
}

bool AudioStreamPlaybackOggVorbis::get_is_sample() const {
	return _is_sample;
}

Ref<AudioSamplePlayback> AudioStreamPlaybackOggVorbis::get_sample_playback() const {
	return sample_playback;
}

void AudioStreamPlaybackOggVorbis::set_sample_playback(const Ref<AudioSamplePlayback> &p_playback) {
	sample_playback = p_playback;
	if (sample_playback.is_valid()) {
		sample_playback->stream_playback = Ref<AudioStreamPlayback>(this);
	}
}

AudioStreamPlaybackOggVorbis::~AudioStreamPlaybackOggVorbis() {
	if (block_is_allocated) {
		vorbis_block_clear(&block);
	}
	if (dsp_state_is_allocated) {
		vorbis_dsp_clear(&dsp_state);
	}
	if (comment_is_allocated) {
		vorbis_comment_clear(&comment);
	}
	if (info_is_allocated) {
		vorbis_info_clear(&info);
	}
}

Ref<AudioStreamPlayback> AudioStreamOggVorbis::instantiate_playback() {
	Ref<AudioStreamPlaybackOggVorbis> ovs;

	ERR_FAIL_COND_V(packet_sequence.is_null(), nullptr);

	ovs.instantiate();
	ovs->vorbis_stream = Ref<AudioStreamOggVorbis>(this);
	ovs->vorbis_data = packet_sequence;
	ovs->frames_mixed = 0;
	ovs->active = false;
	ovs->loops = 0;
	if (ovs->_alloc_vorbis()) {
		return ovs;
	}
	// Failed to allocate data structures.
	return nullptr;
}

String AudioStreamOggVorbis::get_stream_name() const {
	return ""; //return stream_name;
}

void AudioStreamOggVorbis::maybe_update_info() {
	ERR_FAIL_COND(packet_sequence.is_null());

	vorbis_info info;
	vorbis_comment comment;
	int err;

	vorbis_info_init(&info);
	vorbis_comment_init(&comment);

	Ref<OggPacketSequencePlayback> packet_sequence_playback = packet_sequence->instantiate_playback();

	for (int i = 0; i < 3; i++) {
		ogg_packet *packet;
		if (!packet_sequence_playback->next_ogg_packet(&packet)) {
			WARN_PRINT("Failed to get header packet");
			break;
		}
		if (i == 0) {
			packet->b_o_s = 1;

			ERR_FAIL_COND(!vorbis_synthesis_idheader(packet));
		}

		err = vorbis_synthesis_headerin(&info, &comment, packet);
		ERR_FAIL_COND_MSG(err != 0, "Error parsing header packet " + itos(i) + ": " + itos(err));
	}

	packet_sequence->set_sampling_rate(info.rate);

	vorbis_comment_clear(&comment);
	vorbis_info_clear(&info);
}

void AudioStreamOggVorbis::set_packet_sequence(Ref<OggPacketSequence> p_packet_sequence) {
	packet_sequence = p_packet_sequence;
	if (packet_sequence.is_valid()) {
		maybe_update_info();
	}
}

Ref<OggPacketSequence> AudioStreamOggVorbis::get_packet_sequence() const {
	return packet_sequence;
}

void AudioStreamOggVorbis::set_loop(bool p_enable) {
	loop = p_enable;
}

bool AudioStreamOggVorbis::has_loop() const {
	return loop;
}

void AudioStreamOggVorbis::set_loop_offset(double p_seconds) {
	loop_offset = p_seconds;
}

double AudioStreamOggVorbis::get_loop_offset() const {
	return loop_offset;
}

double AudioStreamOggVorbis::get_length() const {
	ERR_FAIL_COND_V(packet_sequence.is_null(), 0);
	return packet_sequence->get_length();
}

void AudioStreamOggVorbis::set_bpm(double p_bpm) {
	ERR_FAIL_COND(p_bpm < 0);
	bpm = p_bpm;
	emit_changed();
}

double AudioStreamOggVorbis::get_bpm() const {
	return bpm;
}

void AudioStreamOggVorbis::set_beat_count(int p_beat_count) {
	ERR_FAIL_COND(p_beat_count < 0);
	beat_count = p_beat_count;
	emit_changed();
}

int AudioStreamOggVorbis::get_beat_count() const {
	return beat_count;
}

void AudioStreamOggVorbis::set_bar_beats(int p_bar_beats) {
	ERR_FAIL_COND(p_bar_beats < 2);
	bar_beats = p_bar_beats;
	emit_changed();
}

int AudioStreamOggVorbis::get_bar_beats() const {
	return bar_beats;
}

bool AudioStreamOggVorbis::is_monophonic() const {
	return false;
}

void AudioStreamOggVorbis::get_parameter_list(List<Parameter> *r_parameters) {
	r_parameters->push_back(Parameter(PropertyInfo(Variant::BOOL, "looping", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_CHECKABLE), Variant()));
}

Ref<AudioSample> AudioStreamOggVorbis::generate_sample() const {
	Ref<AudioSample> sample;
	sample.instantiate();
	sample->stream = this;
	sample->loop_mode = loop
			? AudioSample::LoopMode::LOOP_FORWARD
			: AudioSample::LoopMode::LOOP_DISABLED;
	sample->loop_begin = loop_offset;
	sample->loop_end = 0;
	return sample;
}

void AudioStreamOggVorbis::_bind_methods() {
	ClassDB::bind_static_method("AudioStreamOggVorbis", D_METHOD("load_from_buffer", "buffer"), &AudioStreamOggVorbis::load_from_buffer);
	ClassDB::bind_static_method("AudioStreamOggVorbis", D_METHOD("load_from_file", "path"), &AudioStreamOggVorbis::load_from_file);

	ClassDB::bind_method(D_METHOD("set_packet_sequence", "packet_sequence"), &AudioStreamOggVorbis::set_packet_sequence);
	ClassDB::bind_method(D_METHOD("get_packet_sequence"), &AudioStreamOggVorbis::get_packet_sequence);

	ClassDB::bind_method(D_METHOD("set_loop", "enable"), &AudioStreamOggVorbis::set_loop);
	ClassDB::bind_method(D_METHOD("has_loop"), &AudioStreamOggVorbis::has_loop);

	ClassDB::bind_method(D_METHOD("set_loop_offset", "seconds"), &AudioStreamOggVorbis::set_loop_offset);
	ClassDB::bind_method(D_METHOD("get_loop_offset"), &AudioStreamOggVorbis::get_loop_offset);

	ClassDB::bind_method(D_METHOD("set_bpm", "bpm"), &AudioStreamOggVorbis::set_bpm);
	ClassDB::bind_method(D_METHOD("get_bpm"), &AudioStreamOggVorbis::get_bpm);

	ClassDB::bind_method(D_METHOD("set_beat_count", "count"), &AudioStreamOggVorbis::set_beat_count);
	ClassDB::bind_method(D_METHOD("get_beat_count"), &AudioStreamOggVorbis::get_beat_count);

	ClassDB::bind_method(D_METHOD("set_bar_beats", "count"), &AudioStreamOggVorbis::set_bar_beats);
	ClassDB::bind_method(D_METHOD("get_bar_beats"), &AudioStreamOggVorbis::get_bar_beats);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "packet_sequence", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_packet_sequence", "get_packet_sequence");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bpm", PROPERTY_HINT_RANGE, "0,400,0.01,or_greater"), "set_bpm", "get_bpm");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "beat_count", PROPERTY_HINT_RANGE, "0,512,1,or_greater"), "set_beat_count", "get_beat_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bar_beats", PROPERTY_HINT_RANGE, "2,32,1,or_greater"), "set_bar_beats", "get_bar_beats");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "has_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "loop_offset"), "set_loop_offset", "get_loop_offset");
}

AudioStreamOggVorbis::AudioStreamOggVorbis() {}

AudioStreamOggVorbis::~AudioStreamOggVorbis() {}

Ref<AudioStreamOggVorbis> AudioStreamOggVorbis::load_from_buffer(const Vector<uint8_t> &file_data) {
	return ResourceImporterOggVorbis::load_from_buffer(file_data);
}

Ref<AudioStreamOggVorbis> AudioStreamOggVorbis::load_from_file(const String &p_path) {
	return ResourceImporterOggVorbis::load_from_file(p_path);
}
