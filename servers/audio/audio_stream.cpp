/**************************************************************************/
/*  audio_stream.cpp                                                      */
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

#include "audio_stream.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"

void AudioStreamPlayback::start(double p_from_pos) {
	if (GDVIRTUAL_CALL(_start, p_from_pos)) {
		return;
	}
	ERR_FAIL_MSG("AudioStreamPlayback::start unimplemented!");
}
void AudioStreamPlayback::stop() {
	if (GDVIRTUAL_CALL(_stop)) {
		return;
	}
	ERR_FAIL_MSG("AudioStreamPlayback::stop unimplemented!");
}
bool AudioStreamPlayback::is_playing() const {
	bool ret;
	if (GDVIRTUAL_CALL(_is_playing, ret)) {
		return ret;
	}
	ERR_FAIL_V_MSG(false, "AudioStreamPlayback::is_playing unimplemented!");
}

int AudioStreamPlayback::get_loop_count() const {
	int ret = 0;
	GDVIRTUAL_CALL(_get_loop_count, ret);
	return ret;
}

double AudioStreamPlayback::get_playback_position() const {
	double ret;
	if (GDVIRTUAL_CALL(_get_playback_position, ret)) {
		return ret;
	}
	ERR_FAIL_V_MSG(0, "AudioStreamPlayback::get_playback_position unimplemented!");
}
void AudioStreamPlayback::seek(double p_time) {
	GDVIRTUAL_CALL(_seek, p_time);
}

int AudioStreamPlayback::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	int ret = 0;
	GDVIRTUAL_CALL(_mix, p_buffer, p_rate_scale, p_frames, ret);
	return ret;
}

PackedVector2Array AudioStreamPlayback::_mix_audio_bind(float p_rate_scale, int p_frames) {
	Vector<AudioFrame> frames = mix_audio(p_rate_scale, p_frames);

	PackedVector2Array res;
	res.resize(frames.size());

	Vector2 *res_ptrw = res.ptrw();
	for (int i = 0; i < frames.size(); i++) {
		res_ptrw[i] = Vector2(frames[i].left, frames[i].right);
	}

	return res;
}

Vector<AudioFrame> AudioStreamPlayback::mix_audio(float p_rate_scale, int p_frames) {
	Vector<AudioFrame> res;
	res.resize(p_frames);

	int frames = mix(res.ptrw(), p_rate_scale, p_frames);
	res.resize(frames);

	return res;
}

void AudioStreamPlayback::start_playback(double p_from_pos) {
	start(p_from_pos);
}

void AudioStreamPlayback::stop_playback() {
	stop();
}

void AudioStreamPlayback::seek_playback(double p_time) {
	seek(p_time);
}

void AudioStreamPlayback::tag_used_streams() {
	GDVIRTUAL_CALL(_tag_used_streams);
}

void AudioStreamPlayback::set_parameter(const StringName &p_name, const Variant &p_value) {
	GDVIRTUAL_CALL(_set_parameter, p_name, p_value);
}

Variant AudioStreamPlayback::get_parameter(const StringName &p_name) const {
	Variant ret;
	GDVIRTUAL_CALL(_get_parameter, p_name, ret);
	return ret;
}

Ref<AudioSamplePlayback> AudioStreamPlayback::get_sample_playback() const {
	return nullptr;
}

void AudioStreamPlayback::_bind_methods() {
	GDVIRTUAL_BIND(_start, "from_pos")
	GDVIRTUAL_BIND(_stop)
	GDVIRTUAL_BIND(_is_playing)
	GDVIRTUAL_BIND(_get_loop_count)
	GDVIRTUAL_BIND(_get_playback_position)
	GDVIRTUAL_BIND(_seek, "position")
	GDVIRTUAL_BIND(_mix, "buffer", "rate_scale", "frames");
	GDVIRTUAL_BIND(_tag_used_streams);
	GDVIRTUAL_BIND(_set_parameter, "name", "value");
	GDVIRTUAL_BIND(_get_parameter, "name");

	ClassDB::bind_method(D_METHOD("set_sample_playback", "playback_sample"), &AudioStreamPlayback::set_sample_playback);
	ClassDB::bind_method(D_METHOD("get_sample_playback"), &AudioStreamPlayback::get_sample_playback);
	ClassDB::bind_method(D_METHOD("mix_audio", "rate_scale", "frames"), &AudioStreamPlayback::_mix_audio_bind);
	ClassDB::bind_method(D_METHOD("start", "from_pos"), &AudioStreamPlayback::start_playback, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("seek", "time"), &AudioStreamPlayback::seek_playback, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("stop"), &AudioStreamPlayback::stop_playback);
	ClassDB::bind_method(D_METHOD("get_loop_count"), &AudioStreamPlayback::get_loop_count);
	ClassDB::bind_method(D_METHOD("get_playback_position"), &AudioStreamPlayback::get_playback_position);
	ClassDB::bind_method(D_METHOD("is_playing"), &AudioStreamPlayback::is_playing);
}

AudioStreamPlayback::AudioStreamPlayback() {}

AudioStreamPlayback::~AudioStreamPlayback() {
	if (get_sample_playback().is_valid() && likely(AudioServer::get_singleton() != nullptr)) {
		AudioServer::get_singleton()->stop_sample_playback(get_sample_playback());
	}
}
//////////////////////////////

void AudioStreamPlaybackResampled::begin_resample() {
	//clear cubic interpolation history
	internal_buffer[0] = AudioFrame(0.0, 0.0);
	internal_buffer[1] = AudioFrame(0.0, 0.0);
	internal_buffer[2] = AudioFrame(0.0, 0.0);
	internal_buffer[3] = AudioFrame(0.0, 0.0);
	//mix buffer
	_mix_internal(internal_buffer + 4, INTERNAL_BUFFER_LEN);
	mix_offset = 0;
}

int AudioStreamPlaybackResampled::_mix_internal(AudioFrame *p_buffer, int p_frames) {
	int ret = 0;
	GDVIRTUAL_CALL(_mix_resampled, p_buffer, p_frames, ret);
	return ret;
}
float AudioStreamPlaybackResampled::get_stream_sampling_rate() {
	float ret = 0;
	GDVIRTUAL_CALL(_get_stream_sampling_rate, ret);
	return ret;
}

void AudioStreamPlaybackResampled::_bind_methods() {
	ClassDB::bind_method(D_METHOD("begin_resample"), &AudioStreamPlaybackResampled::begin_resample);

	GDVIRTUAL_BIND(_mix_resampled, "dst_buffer", "frame_count");
	GDVIRTUAL_BIND(_get_stream_sampling_rate);
}

int AudioStreamPlaybackResampled::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	float target_rate = AudioServer::get_singleton()->get_mix_rate();
	float playback_speed_scale = AudioServer::get_singleton()->get_playback_speed_scale();

	uint64_t mix_increment = uint64_t(((get_stream_sampling_rate() * p_rate_scale * playback_speed_scale) / double(target_rate)) * double(FP_LEN));

	int mixed_frames_total = -1;

	int i;
	for (i = 0; i < p_frames; i++) {
		uint32_t idx = CUBIC_INTERP_HISTORY + uint32_t(mix_offset >> FP_BITS);
		//standard cubic interpolation (great quality/performance ratio)
		//this used to be moved to a LUT for greater performance, but nowadays CPU speed is generally faster than memory.
		float mu = (mix_offset & FP_MASK) / float(FP_LEN);
		AudioFrame y0 = internal_buffer[idx - 3];
		AudioFrame y1 = internal_buffer[idx - 2];
		AudioFrame y2 = internal_buffer[idx - 1];
		AudioFrame y3 = internal_buffer[idx - 0];

		if (idx >= internal_buffer_end && mixed_frames_total == -1) {
			// The internal buffer ends somewhere in this range, and we haven't yet recorded the number of good frames we have.
			mixed_frames_total = i;
		}

		float mu2 = mu * mu;
		float h11 = mu2 * (mu - 1);
		float z = mu2 - h11;
		float h01 = z - h11;
		float h10 = mu - z;

		p_buffer[i] = y1 + (y2 - y1) * h01 + ((y2 - y0) * h10 + (y3 - y1) * h11) * 0.5;

		mix_offset += mix_increment;

		while ((mix_offset >> FP_BITS) >= INTERNAL_BUFFER_LEN) {
			internal_buffer[0] = internal_buffer[INTERNAL_BUFFER_LEN + 0];
			internal_buffer[1] = internal_buffer[INTERNAL_BUFFER_LEN + 1];
			internal_buffer[2] = internal_buffer[INTERNAL_BUFFER_LEN + 2];
			internal_buffer[3] = internal_buffer[INTERNAL_BUFFER_LEN + 3];
			int mixed_frames = _mix_internal(internal_buffer + 4, INTERNAL_BUFFER_LEN);
			if (mixed_frames != INTERNAL_BUFFER_LEN) {
				// internal_buffer[mixed_frames] is the first frame of silence.
				internal_buffer_end = mixed_frames;
			} else {
				// The internal buffer does not contain the first frame of silence.
				internal_buffer_end = -1;
			}
			mix_offset -= (INTERNAL_BUFFER_LEN << FP_BITS);
		}
	}
	if (mixed_frames_total == -1 && i == p_frames) {
		mixed_frames_total = p_frames;
	}
	return mixed_frames_total;
}

////////////////////////////////

Ref<AudioStreamPlayback> AudioStream::instantiate_playback() {
	Ref<AudioStreamPlayback> ret;
	if (GDVIRTUAL_CALL(_instantiate_playback, ret)) {
		return ret;
	}
	ERR_FAIL_V_MSG(Ref<AudioStreamPlayback>(), "Method must be implemented!");
}
String AudioStream::get_stream_name() const {
	String ret;
	GDVIRTUAL_CALL(_get_stream_name, ret);
	return ret;
}

double AudioStream::get_length() const {
	double ret = 0;
	GDVIRTUAL_CALL(_get_length, ret);
	return ret;
}

bool AudioStream::is_monophonic() const {
	bool ret = true;
	GDVIRTUAL_CALL(_is_monophonic, ret);
	return ret;
}

double AudioStream::get_bpm() const {
	double ret = 0;
	GDVIRTUAL_CALL(_get_bpm, ret);
	return ret;
}

bool AudioStream::has_loop() const {
	bool ret = 0;
	GDVIRTUAL_CALL(_has_loop, ret);
	return ret;
}

int AudioStream::get_bar_beats() const {
	int ret = 0;
	GDVIRTUAL_CALL(_get_bar_beats, ret);
	return ret;
}

int AudioStream::get_beat_count() const {
	int ret = 0;
	GDVIRTUAL_CALL(_get_beat_count, ret);
	return ret;
}

void AudioStream::tag_used(float p_offset) {
	if (tagged_frame != AudioServer::get_singleton()->get_mixed_frames()) {
		offset_count = 0;
		tagged_frame = AudioServer::get_singleton()->get_mixed_frames();
	}
	if (offset_count < MAX_TAGGED_OFFSETS) {
		tagged_offsets[offset_count++] = p_offset;
	}
}

uint64_t AudioStream::get_tagged_frame() const {
	return tagged_frame;
}
uint32_t AudioStream::get_tagged_frame_count() const {
	return offset_count;
}
float AudioStream::get_tagged_frame_offset(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, MAX_TAGGED_OFFSETS, 0);
	return tagged_offsets[p_index];
}

void AudioStream::get_parameter_list(List<Parameter> *r_parameters) {
	TypedArray<Dictionary> ret;
	GDVIRTUAL_CALL(_get_parameter_list, ret);
	for (int i = 0; i < ret.size(); i++) {
		Dictionary d = ret[i];
		ERR_CONTINUE(!d.has("default_value"));
		r_parameters->push_back(Parameter(PropertyInfo::from_dict(d), d["default_value"]));
	}
}

Ref<AudioSample> AudioStream::generate_sample() const {
	ERR_FAIL_COND_V_MSG(!can_be_sampled(), nullptr, "Cannot generate a sample for a stream that cannot be sampled.");
	Ref<AudioSample> sample;
	sample.instantiate();
	sample->stream = this;
	return sample;
}

void AudioStream::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_length"), &AudioStream::get_length);
	ClassDB::bind_method(D_METHOD("is_monophonic"), &AudioStream::is_monophonic);
	ClassDB::bind_method(D_METHOD("instantiate_playback"), &AudioStream::instantiate_playback);
	ClassDB::bind_method(D_METHOD("can_be_sampled"), &AudioStream::can_be_sampled);
	ClassDB::bind_method(D_METHOD("generate_sample"), &AudioStream::generate_sample);
	ClassDB::bind_method(D_METHOD("is_meta_stream"), &AudioStream::is_meta_stream);

	GDVIRTUAL_BIND(_instantiate_playback);
	GDVIRTUAL_BIND(_get_stream_name);
	GDVIRTUAL_BIND(_get_length);
	GDVIRTUAL_BIND(_is_monophonic);
	GDVIRTUAL_BIND(_get_bpm)
	GDVIRTUAL_BIND(_get_beat_count)
	GDVIRTUAL_BIND(_get_parameter_list)

	ADD_SIGNAL(MethodInfo("parameter_list_changed"));
}

////////////////////////////////

Ref<AudioStreamPlayback> AudioStreamMicrophone::instantiate_playback() {
	Ref<AudioStreamPlaybackMicrophone> playback;
	playback.instantiate();

	playbacks.insert(playback.ptr());

	playback->microphone = Ref<AudioStreamMicrophone>((AudioStreamMicrophone *)this);
	playback->active = false;

	return playback;
}

String AudioStreamMicrophone::get_stream_name() const {
	//if (audio_stream.is_valid()) {
	//return "Random: " + audio_stream->get_name();
	//}
	return "Microphone";
}

double AudioStreamMicrophone::get_length() const {
	return 0;
}

bool AudioStreamMicrophone::is_monophonic() const {
	return true;
}

int AudioStreamPlaybackMicrophone::_mix_internal(AudioFrame *p_buffer, int p_frames) {
	AudioDriver::get_singleton()->lock();

	Vector<int32_t> buf = AudioDriver::get_singleton()->get_input_buffer();
	unsigned int input_size = AudioDriver::get_singleton()->get_input_size();
	int mix_rate = AudioDriver::get_singleton()->get_mix_rate();
	unsigned int playback_delay = MIN(((50 * mix_rate) / 1000) * 2, buf.size() >> 1);
#ifdef DEBUG_ENABLED
	unsigned int input_position = AudioDriver::get_singleton()->get_input_position();
#endif

	int mixed_frames = p_frames;

	if (playback_delay > input_size) {
		for (int i = 0; i < p_frames; i++) {
			p_buffer[i] = AudioFrame(0.0f, 0.0f);
		}
		input_ofs = 0;
	} else {
		for (int i = 0; i < p_frames; i++) {
			if (input_size > input_ofs && (int)input_ofs < buf.size()) {
				float l = (buf[input_ofs++] >> 16) / 32768.f;
				if ((int)input_ofs >= buf.size()) {
					input_ofs = 0;
				}
				float r = (buf[input_ofs++] >> 16) / 32768.f;
				if ((int)input_ofs >= buf.size()) {
					input_ofs = 0;
				}

				p_buffer[i] = AudioFrame(l, r);
			} else {
				if (mixed_frames == p_frames) {
					mixed_frames = i;
				}
				p_buffer[i] = AudioFrame(0.0f, 0.0f);
			}
		}
	}

#ifdef DEBUG_ENABLED
	if (input_ofs > input_position && (int)(input_ofs - input_position) < (p_frames * 2)) {
		print_verbose(String(get_class_name()) + " buffer underrun: input_position=" + itos(input_position) + " input_ofs=" + itos(input_ofs) + " input_size=" + itos(input_size));
	}
#endif

	AudioDriver::get_singleton()->unlock();

	return mixed_frames;
}

int AudioStreamPlaybackMicrophone::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	return AudioStreamPlaybackResampled::mix(p_buffer, p_rate_scale, p_frames);
}

float AudioStreamPlaybackMicrophone::get_stream_sampling_rate() {
	return AudioDriver::get_singleton()->get_mix_rate();
}

void AudioStreamPlaybackMicrophone::start(double p_from_pos) {
	if (active) {
		return;
	}

	if (!GLOBAL_GET("audio/driver/enable_input")) {
		WARN_PRINT("You must enable the project setting \"audio/driver/enable_input\" to use audio capture.");
		return;
	}

	input_ofs = 0;

	if (AudioDriver::get_singleton()->input_start() == OK) {
		active = true;
		begin_resample();
	}
}

void AudioStreamPlaybackMicrophone::stop() {
	if (active) {
		AudioDriver::get_singleton()->input_stop();
		active = false;
	}
}

bool AudioStreamPlaybackMicrophone::is_playing() const {
	return active;
}

int AudioStreamPlaybackMicrophone::get_loop_count() const {
	return 0;
}

double AudioStreamPlaybackMicrophone::get_playback_position() const {
	return 0;
}

void AudioStreamPlaybackMicrophone::seek(double p_time) {
	// Can't seek a microphone input
}

void AudioStreamPlaybackMicrophone::tag_used_streams() {
	microphone->tag_used(0);
}

AudioStreamPlaybackMicrophone::~AudioStreamPlaybackMicrophone() {
	microphone->playbacks.erase(this);
	stop();
}

AudioStreamPlaybackMicrophone::AudioStreamPlaybackMicrophone() {
}

////////////////////////////////

void AudioStreamRandomizer::add_stream(int p_index, Ref<AudioStream> p_stream, float p_weight) {
	if (p_index < 0) {
		p_index = audio_stream_pool.size();
	}
	ERR_FAIL_COND(p_index > audio_stream_pool.size());
	PoolEntry entry{ p_stream, p_weight };
	audio_stream_pool.insert(p_index, entry);
	emit_signal(CoreStringName(changed));
	notify_property_list_changed();
}

// p_index_to is relative to the array prior to the removal of from.
// Example: [0, 1, 2, 3], move(1, 3) => [0, 2, 1, 3]
void AudioStreamRandomizer::move_stream(int p_index_from, int p_index_to) {
	ERR_FAIL_INDEX(p_index_from, audio_stream_pool.size());
	// p_index_to == audio_stream_pool.size() is valid (move to end).
	ERR_FAIL_COND(p_index_to < 0);
	ERR_FAIL_COND(p_index_to > audio_stream_pool.size());
	audio_stream_pool.insert(p_index_to, audio_stream_pool[p_index_from]);
	// If 'from' is strictly after 'to' we need to increment the index by one because of the insertion.
	if (p_index_from > p_index_to) {
		p_index_from++;
	}
	audio_stream_pool.remove_at(p_index_from);
	emit_signal(CoreStringName(changed));
	notify_property_list_changed();
}

void AudioStreamRandomizer::remove_stream(int p_index) {
	ERR_FAIL_INDEX(p_index, audio_stream_pool.size());
	audio_stream_pool.remove_at(p_index);
	emit_signal(CoreStringName(changed));
	notify_property_list_changed();
}

void AudioStreamRandomizer::set_stream(int p_index, Ref<AudioStream> p_stream) {
	ERR_FAIL_INDEX(p_index, audio_stream_pool.size());
	audio_stream_pool.write[p_index].stream = p_stream;
	emit_signal(CoreStringName(changed));
}

Ref<AudioStream> AudioStreamRandomizer::get_stream(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, audio_stream_pool.size(), nullptr);
	return audio_stream_pool[p_index].stream;
}

void AudioStreamRandomizer::set_stream_probability_weight(int p_index, float p_weight) {
	ERR_FAIL_INDEX(p_index, audio_stream_pool.size());
	audio_stream_pool.write[p_index].weight = p_weight;
	emit_signal(CoreStringName(changed));
}

float AudioStreamRandomizer::get_stream_probability_weight(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, audio_stream_pool.size(), 0);
	return audio_stream_pool[p_index].weight;
}

void AudioStreamRandomizer::set_streams_count(int p_count) {
	audio_stream_pool.resize(p_count);
}

int AudioStreamRandomizer::get_streams_count() const {
	return audio_stream_pool.size();
}

void AudioStreamRandomizer::set_random_pitch(float p_pitch) {
	if (p_pitch < 1) {
		p_pitch = 1;
	}
	random_pitch_scale = p_pitch;
}

float AudioStreamRandomizer::get_random_pitch() const {
	return random_pitch_scale;
}

void AudioStreamRandomizer::set_random_volume_offset_db(float p_volume_offset_db) {
	if (p_volume_offset_db < 0) {
		p_volume_offset_db = 0;
	}
	random_volume_offset_db = p_volume_offset_db;
}

float AudioStreamRandomizer::get_random_volume_offset_db() const {
	return random_volume_offset_db;
}

void AudioStreamRandomizer::set_playback_mode(PlaybackMode p_playback_mode) {
	playback_mode = p_playback_mode;
}

AudioStreamRandomizer::PlaybackMode AudioStreamRandomizer::get_playback_mode() const {
	return playback_mode;
}

Ref<AudioStreamPlayback> AudioStreamRandomizer::instance_playback_random() {
	Ref<AudioStreamPlaybackRandomizer> playback;
	playback.instantiate();
	playbacks.insert(playback.ptr());
	playback->randomizer = Ref<AudioStreamRandomizer>((AudioStreamRandomizer *)this);

	double total_weight = 0;
	Vector<PoolEntry> local_pool;
	for (const PoolEntry &entry : audio_stream_pool) {
		if (entry.stream.is_valid() && entry.weight > 0) {
			local_pool.push_back(entry);
			total_weight += entry.weight;
		}
	}
	if (local_pool.is_empty()) {
		return playback;
	}
	double chosen_cumulative_weight = Math::random(0.0, total_weight);
	double cumulative_weight = 0;
	for (PoolEntry &entry : local_pool) {
		cumulative_weight += entry.weight;
		if (cumulative_weight > chosen_cumulative_weight) {
			playback->playback = entry.stream->instantiate_playback();
			last_playback = entry.stream;
			break;
		}
	}
	if (playback->playback.is_null()) {
		// This indicates a floating point error. Take the last element.
		last_playback = local_pool[local_pool.size() - 1].stream;
		playback->playback = local_pool.write[local_pool.size() - 1].stream->instantiate_playback();
	}
	return playback;
}

Ref<AudioStreamPlayback> AudioStreamRandomizer::instance_playback_no_repeats() {
	Ref<AudioStreamPlaybackRandomizer> playback;

	double total_weight = 0;
	Vector<PoolEntry> local_pool;
	for (const PoolEntry &entry : audio_stream_pool) {
		if (entry.stream == last_playback) {
			continue;
		}
		if (entry.stream.is_valid() && entry.weight > 0) {
			local_pool.push_back(entry);
			total_weight += entry.weight;
		}
	}
	if (local_pool.is_empty()) {
		// There is only one sound to choose from.
		// Always play a random sound while allowing repeats (which always plays the same sound).
		playback = instance_playback_random();
		return playback;
	}

	playback.instantiate();
	playbacks.insert(playback.ptr());
	playback->randomizer = Ref<AudioStreamRandomizer>((AudioStreamRandomizer *)this);
	double chosen_cumulative_weight = Math::random(0.0, total_weight);
	double cumulative_weight = 0;
	for (PoolEntry &entry : local_pool) {
		cumulative_weight += entry.weight;
		if (cumulative_weight > chosen_cumulative_weight) {
			last_playback = entry.stream;
			playback->playback = entry.stream->instantiate_playback();
			break;
		}
	}
	if (playback->playback.is_null()) {
		// This indicates a floating point error. Take the last element.
		last_playback = local_pool[local_pool.size() - 1].stream;
		playback->playback = local_pool.write[local_pool.size() - 1].stream->instantiate_playback();
	}
	return playback;
}

Ref<AudioStreamPlayback> AudioStreamRandomizer::instance_playback_sequential() {
	Ref<AudioStreamPlaybackRandomizer> playback;
	playback.instantiate();
	playbacks.insert(playback.ptr());
	playback->randomizer = Ref<AudioStreamRandomizer>((AudioStreamRandomizer *)this);

	Vector<Ref<AudioStream>> local_pool;
	for (const PoolEntry &entry : audio_stream_pool) {
		if (entry.stream.is_null()) {
			continue;
		}
		if (local_pool.has(entry.stream)) {
			WARN_PRINT("Duplicate stream in sequential playback pool");
			continue;
		}
		local_pool.push_back(entry.stream);
	}
	if (local_pool.is_empty()) {
		return playback;
	}
	bool found_last_stream = false;
	for (Ref<AudioStream> &entry : local_pool) {
		if (found_last_stream) {
			last_playback = entry;
			playback->playback = entry->instantiate_playback();
			break;
		}
		if (entry == last_playback) {
			found_last_stream = true;
		}
	}
	if (playback->playback.is_null()) {
		// Wrap around
		last_playback = local_pool[0];
		playback->playback = local_pool.write[0]->instantiate_playback();
	}
	return playback;
}

Ref<AudioStreamPlayback> AudioStreamRandomizer::instantiate_playback() {
	switch (playback_mode) {
		case PLAYBACK_RANDOM:
			return instance_playback_random();
		case PLAYBACK_RANDOM_NO_REPEATS:
			return instance_playback_no_repeats();
		case PLAYBACK_SEQUENTIAL:
			return instance_playback_sequential();
		default:
			ERR_FAIL_V_MSG(nullptr, "Unhandled playback mode.");
	}
}

String AudioStreamRandomizer::get_stream_name() const {
	return "Randomizer";
}

double AudioStreamRandomizer::get_length() const {
	return 0;
}

bool AudioStreamRandomizer::is_monophonic() const {
	for (const PoolEntry &entry : audio_stream_pool) {
		if (entry.stream.is_valid() && entry.stream->is_monophonic()) {
			return true;
		}
	}
	return false;
}

void AudioStreamRandomizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_stream", "index", "stream", "weight"), &AudioStreamRandomizer::add_stream, DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("move_stream", "index_from", "index_to"), &AudioStreamRandomizer::move_stream);
	ClassDB::bind_method(D_METHOD("remove_stream", "index"), &AudioStreamRandomizer::remove_stream);

	ClassDB::bind_method(D_METHOD("set_stream", "index", "stream"), &AudioStreamRandomizer::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream", "index"), &AudioStreamRandomizer::get_stream);
	ClassDB::bind_method(D_METHOD("set_stream_probability_weight", "index", "weight"), &AudioStreamRandomizer::set_stream_probability_weight);
	ClassDB::bind_method(D_METHOD("get_stream_probability_weight", "index"), &AudioStreamRandomizer::get_stream_probability_weight);

	ClassDB::bind_method(D_METHOD("set_streams_count", "count"), &AudioStreamRandomizer::set_streams_count);
	ClassDB::bind_method(D_METHOD("get_streams_count"), &AudioStreamRandomizer::get_streams_count);

	ClassDB::bind_method(D_METHOD("set_random_pitch", "scale"), &AudioStreamRandomizer::set_random_pitch);
	ClassDB::bind_method(D_METHOD("get_random_pitch"), &AudioStreamRandomizer::get_random_pitch);

	ClassDB::bind_method(D_METHOD("set_random_volume_offset_db", "db_offset"), &AudioStreamRandomizer::set_random_volume_offset_db);
	ClassDB::bind_method(D_METHOD("get_random_volume_offset_db"), &AudioStreamRandomizer::get_random_volume_offset_db);

	ClassDB::bind_method(D_METHOD("set_playback_mode", "mode"), &AudioStreamRandomizer::set_playback_mode);
	ClassDB::bind_method(D_METHOD("get_playback_mode"), &AudioStreamRandomizer::get_playback_mode);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "playback_mode", PROPERTY_HINT_ENUM, "Random (Avoid Repeats),Random,Sequential"), "set_playback_mode", "get_playback_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "random_pitch", PROPERTY_HINT_RANGE, "1,16,0.01"), "set_random_pitch", "get_random_pitch");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "random_volume_offset_db", PROPERTY_HINT_RANGE, "0,40,0.01,suffix:dB"), "set_random_volume_offset_db", "get_random_volume_offset_db");
	ADD_ARRAY("streams", "stream_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "streams_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_streams_count", "get_streams_count");

	BIND_ENUM_CONSTANT(PLAYBACK_RANDOM_NO_REPEATS);
	BIND_ENUM_CONSTANT(PLAYBACK_RANDOM);
	BIND_ENUM_CONSTANT(PLAYBACK_SEQUENTIAL);

	PoolEntry defaults;

	base_property_helper.set_prefix("stream_");
	base_property_helper.set_array_length_getter(&AudioStreamRandomizer::get_streams_count);
	base_property_helper.register_property(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "AudioStream"), defaults.stream, &AudioStreamRandomizer::set_stream, &AudioStreamRandomizer::get_stream);
	base_property_helper.register_property(PropertyInfo(Variant::FLOAT, "weight", PROPERTY_HINT_RANGE, "0,100,0.001,or_greater"), defaults.weight, &AudioStreamRandomizer::set_stream_probability_weight, &AudioStreamRandomizer::get_stream_probability_weight);
	PropertyListHelper::register_base_helper(&base_property_helper);
}

AudioStreamRandomizer::AudioStreamRandomizer() {
	property_helper.setup_for_instance(base_property_helper, this);
}

void AudioStreamPlaybackRandomizer::start(double p_from_pos) {
	playing = playback;
	{
		float range_from = 1.0 / randomizer->random_pitch_scale;
		float range_to = randomizer->random_pitch_scale;

		pitch_scale = range_from + Math::randf() * (range_to - range_from);
	}
	{
		float range_from = -randomizer->random_volume_offset_db;
		float range_to = randomizer->random_volume_offset_db;

		float volume_offset_db = range_from + Math::randf() * (range_to - range_from);
		volume_scale = Math::db_to_linear(volume_offset_db);
	}

	if (playing.is_valid()) {
		playing->start(p_from_pos);
	}
}

void AudioStreamPlaybackRandomizer::stop() {
	if (playing.is_valid()) {
		playing->stop();
	}
}

bool AudioStreamPlaybackRandomizer::is_playing() const {
	if (playing.is_valid()) {
		return playing->is_playing();
	}

	return false;
}

int AudioStreamPlaybackRandomizer::get_loop_count() const {
	if (playing.is_valid()) {
		return playing->get_loop_count();
	}

	return 0;
}

double AudioStreamPlaybackRandomizer::get_playback_position() const {
	if (playing.is_valid()) {
		return playing->get_playback_position();
	}

	return 0;
}

void AudioStreamPlaybackRandomizer::seek(double p_time) {
	if (playing.is_valid()) {
		playing->seek(p_time);
	}
}

void AudioStreamPlaybackRandomizer::tag_used_streams() {
	Ref<AudioStreamPlayback> p = playing; // Thread safety
	if (p.is_valid()) {
		p->tag_used_streams();
	}
	randomizer->tag_used(0);
}

int AudioStreamPlaybackRandomizer::mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) {
	if (playing.is_valid()) {
		int mixed_samples = playing->mix(p_buffer, p_rate_scale * pitch_scale, p_frames);
		for (int samp = 0; samp < mixed_samples; samp++) {
			p_buffer[samp] *= volume_scale;
		}
		return mixed_samples;
	} else {
		for (int i = 0; i < p_frames; i++) {
			p_buffer[i] = AudioFrame(0, 0);
		}
		return p_frames;
	}
}

AudioStreamPlaybackRandomizer::~AudioStreamPlaybackRandomizer() {
	randomizer->playbacks.erase(this);
}
/////////////////////////////////////////////
