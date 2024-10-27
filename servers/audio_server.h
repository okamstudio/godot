/**************************************************************************/
/*  audio_server.h                                                        */
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

#ifndef AUDIO_SERVER_H
#define AUDIO_SERVER_H

#include "core/math/audio_frame.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/templates/safe_list.h"
#include "core/variant/variant.h"
#include "servers/audio/audio_effect.h"
#include "servers/audio/audio_filter_sw.h"

#include <atomic>

class AudioDriverDummy;
class AudioSample;
class AudioStream;
class AudioStreamWAV;
class AudioStreamPlayback;
class AudioSamplePlayback;

class AudioDriver {
	static AudioDriver *singleton;
	uint64_t _last_mix_time = 0;
	uint64_t _last_mix_frames = 0;

#ifdef DEBUG_ENABLED
	SafeNumeric<uint64_t> prof_ticks;
	SafeNumeric<uint64_t> prof_time;
#endif

protected:
	Vector<int32_t> input_buffer;
	unsigned int input_position = 0;
	unsigned int input_size = 0;

	void audio_server_process(int p_frames, int32_t *p_buffer, bool p_update_mix_time = true);
	void update_mix_time(int p_frames);
	void input_buffer_init(int driver_buffer_frames);
	void input_buffer_write(int32_t sample);

	int _get_configured_mix_rate();

#ifdef DEBUG_ENABLED
	_FORCE_INLINE_ void start_counting_ticks() { prof_ticks.set(OS::get_singleton()->get_ticks_usec()); }
	_FORCE_INLINE_ void stop_counting_ticks() { prof_time.add(OS::get_singleton()->get_ticks_usec() - prof_ticks.get()); }
#else
	_FORCE_INLINE_ void start_counting_ticks() {}
	_FORCE_INLINE_ void stop_counting_ticks() {}
#endif

public:
	double get_time_since_last_mix(); //useful for video -> audio sync
	double get_time_to_next_mix();

	enum SpeakerMode {
		SPEAKER_MODE_STEREO,
		SPEAKER_SURROUND_31,
		SPEAKER_SURROUND_51,
		SPEAKER_SURROUND_71,
	};

	static AudioDriver *get_singleton();
	void set_singleton();

	// Virtual API to implement.

	virtual const char *get_name() const = 0;

	virtual Error init() = 0;
	virtual void start() = 0;
	virtual int get_mix_rate() const = 0;
	virtual SpeakerMode get_speaker_mode() const = 0;
	virtual float get_latency() { return 0; }

	virtual void lock() = 0;
	virtual void unlock() = 0;
	virtual void finish() = 0;

	virtual PackedStringArray get_output_device_list();
	virtual String get_output_device();
	virtual void set_output_device(const String &p_name) {}

	virtual Error input_start() { return FAILED; }
	virtual Error input_stop() { return FAILED; }

	virtual PackedStringArray get_input_device_list();
	virtual String get_input_device() { return "Default"; }
	virtual void set_input_device(const String &p_name) {}

	//

	SpeakerMode get_speaker_mode_by_total_channels(int p_channels) const;
	int get_total_channels_by_speaker_mode(SpeakerMode) const;

	Vector<int32_t> get_input_buffer() { return input_buffer; }
	unsigned int get_input_position() { return input_position; }
	unsigned int get_input_size() { return input_size; }

#ifdef DEBUG_ENABLED
	uint64_t get_profiling_time() const { return prof_time.get(); }
	void reset_profiling_time() { prof_time.set(0); }
#endif

	// Samples handling.
	virtual bool is_stream_registered_as_sample(const Ref<AudioStream> &p_stream) const {
		return false;
	}
	virtual void register_sample(const Ref<AudioSample> &p_sample) {}
	virtual void unregister_sample(const Ref<AudioSample> &p_sample) {}
	virtual void start_sample_playback(const Ref<AudioSamplePlayback> &p_playback);
	virtual void stop_sample_playback(const Ref<AudioSamplePlayback> &p_playback) {}
	virtual void set_sample_playback_pause(const Ref<AudioSamplePlayback> &p_playback, bool p_paused) {}
	virtual bool is_sample_playback_active(const Ref<AudioSamplePlayback> &p_playback) { return false; }
	virtual double get_sample_playback_position(const Ref<AudioSamplePlayback> &p_playback) { return false; }
	virtual void update_sample_playback_pitch_scale(const Ref<AudioSamplePlayback> &p_playback, float p_pitch_scale = 0.0f) {}
	virtual void set_sample_playback_bus_volumes_linear(const Ref<AudioSamplePlayback> &p_playback, const HashMap<StringName, Vector<AudioFrame>> &p_bus_volumes) {}

	virtual void set_sample_bus_count(int p_count) {}
	virtual void remove_sample_bus(int p_bus) {}
	virtual void add_sample_bus(int p_at_pos = -1) {}
	virtual void move_sample_bus(int p_bus, int p_to_pos) {}
	virtual void set_sample_bus_send(int p_bus, const StringName &p_send) {}
	virtual void set_sample_bus_volume_db(int p_bus, float p_volume_db) {}
	virtual void set_sample_bus_solo(int p_bus, bool p_enable) {}
	virtual void set_sample_bus_mute(int p_bus, bool p_enable) {}

	AudioDriver() {}
	virtual ~AudioDriver() {}
};

class AudioDriverManager {
	enum {
		MAX_DRIVERS = 10
	};

	static AudioDriver *drivers[MAX_DRIVERS];
	static int driver_count;

	static AudioDriverDummy dummy_driver;

public:
	static const int DEFAULT_MIX_RATE = 44100;

	static void add_driver(AudioDriver *p_driver);
	static void initialize(int p_driver);
	static int get_driver_count();
	static AudioDriver *get_driver(int p_driver);
};

class AudioBusLayout;

class AudioServer : public Object {
	GDCLASS(AudioServer, Object);

public:
	//re-expose this here, as AudioDriver is not exposed to script
	enum SpeakerMode {
		SPEAKER_MODE_STEREO,
		SPEAKER_SURROUND_31,
		SPEAKER_SURROUND_51,
		SPEAKER_SURROUND_71,
	};

	enum PlaybackType {
		PLAYBACK_TYPE_DEFAULT,
		PLAYBACK_TYPE_STREAM,
		PLAYBACK_TYPE_SAMPLE,
		PLAYBACK_TYPE_MAX
	};

	enum {
		AUDIO_DATA_INVALID_ID = -1,
		MAX_CHANNELS_PER_BUS = 4,
		MAX_BUSES_PER_PLAYBACK = 6,
		LOOKAHEAD_BUFFER_SIZE = 64,
	};

	typedef void (*AudioCallback)(void *p_userdata);

private:
	uint64_t mix_time = 0;
	int mix_size = 0;

	uint32_t buffer_size = 0;
	uint64_t mix_count = 0;
	uint64_t mix_frames = 0;
#ifdef DEBUG_ENABLED
	SafeNumeric<uint64_t> prof_time;
#endif

	float channel_disable_threshold_db = 0.0f;
	uint32_t channel_disable_frames = 0;

	int channel_count = 0;
	int to_mix = 0;

	float playback_speed_scale = 1.0f;

	bool tag_used_audio_streams = false;

	struct Bus {
		StringName name;
		bool solo = false;
		bool mute = false;
		bool bypass = false;

		bool soloed = false;

		// Each channel is a stereo pair.
		struct Channel {
			bool used = false;
			bool active = false;
			AudioFrame peak_volume = AudioFrame(AUDIO_MIN_PEAK_DB, AUDIO_MIN_PEAK_DB);
			Vector<AudioFrame> buffer;
			Vector<Ref<AudioEffectInstance>> effect_instances;
			uint64_t last_mix_with_audio = 0;
			Channel() {}
		};

		Vector<Channel> channels;

		struct Effect {
			Ref<AudioEffect> effect;
			bool enabled = false;
#ifdef DEBUG_ENABLED
			uint64_t prof_time = 0;
#endif
		};

		Vector<Effect> effects;
		float volume_db = 0.0f;
		StringName send;
		int index_cache = 0;
	};

	struct AudioStreamPlaybackBusDetails {
		bool bus_active[MAX_BUSES_PER_PLAYBACK] = {};
		StringName bus[MAX_BUSES_PER_PLAYBACK];
		AudioFrame volume[MAX_BUSES_PER_PLAYBACK][MAX_CHANNELS_PER_BUS];
	};

	struct AudioStreamPlaybackListNode {
		enum PlaybackState {
			PAUSED = 0, // Paused. Keep this stream playback around though so it can be restarted.
			PLAYING = 1, // Playing. Fading may still be necessary if volume changes!
			FADE_OUT_TO_PAUSE = 2, // About to pause.
			FADE_OUT_TO_DELETION = 3, // About to stop.
			AWAITING_DELETION = 4,
		};
		// If zero or positive, a place in the stream to seek to during the next mix.
		SafeNumeric<float> setseek;
		SafeNumeric<float> pitch_scale;
		SafeNumeric<float> highshelf_gain;
		SafeNumeric<float> attenuation_filter_cutoff_hz; // This isn't used unless highshelf_gain is nonzero.
		AudioFilterSW::Processor filter_process[8];
		// Updating this ref after the list node is created breaks consistency guarantees, don't do it!
		Ref<AudioStreamPlayback> stream_playback;
		// Playback state determines the fate of a particular AudioStreamListNode during the mix step. Must be atomically replaced.
		std::atomic<PlaybackState> state = AWAITING_DELETION;
		// This data should only ever be modified by an atomic replacement of the pointer.
		std::atomic<AudioStreamPlaybackBusDetails *> bus_details = nullptr;
		// Previous bus details should only be accessed on the audio thread.
		AudioStreamPlaybackBusDetails *prev_bus_details = nullptr;
		// The next few samples are stored here so we have some time to fade audio out if it ends abruptly at the beginning of the next mix.
		AudioFrame lookahead[LOOKAHEAD_BUFFER_SIZE];
	};

	SafeList<AudioStreamPlaybackListNode *> playback_list;
	SafeList<AudioStreamPlaybackBusDetails *> bus_details_graveyard;
	void _delete_stream_playback(Ref<AudioStreamPlayback> p_playback);
	void _delete_stream_playback_list_node(AudioStreamPlaybackListNode *p_node);

	// TODO document if this is necessary.
	SafeList<AudioStreamPlaybackBusDetails *> bus_details_graveyard_frame_old;

	Vector<Vector<AudioFrame>> temp_buffer; //temp_buffer for each level
	Vector<AudioFrame> mix_buffer;
	Vector<Bus *> buses;
	HashMap<StringName, Bus *> bus_map;

	void _update_bus_effects(int p_bus);

	static AudioServer *singleton;

	void init_channels_and_buffers();

	void _mix_step();
	void _mix_step_for_channel(AudioFrame *p_out_buf, AudioFrame *p_source_buf, AudioFrame p_vol_start, AudioFrame p_vol_final, float p_attenuation_filter_cutoff_hz, float p_highshelf_gain, AudioFilterSW::Processor *p_processor_l, AudioFilterSW::Processor *p_processor_r);

	// Should only be called on the main thread.
	AudioStreamPlaybackListNode *_find_playback_list_node(Ref<AudioStreamPlayback> p_playback);

	struct CallbackItem {
		AudioCallback callback;
		void *userdata = nullptr;
	};

	SafeList<CallbackItem *> update_callback_list;
	SafeList<CallbackItem *> mix_callback_list;
	SafeList<CallbackItem *> listener_changed_callback_list;

	friend class AudioDriver;
	void _driver_process(int p_frames, int32_t *p_buffer);

	LocalVector<Ref<AudioSamplePlayback>> sample_playback_list;

protected:
	static void _bind_methods();

public:
	_FORCE_INLINE_ int get_channel_count() const {
		switch (get_speaker_mode()) {
			case SPEAKER_MODE_STEREO:
				return 1;
			case SPEAKER_SURROUND_31:
				return 2;
			case SPEAKER_SURROUND_51:
				return 3;
			case SPEAKER_SURROUND_71:
				return 4;
		}
		ERR_FAIL_V(1);
	}

	// Do not use from outside audio thread.
	bool thread_has_channel_mix_buffer(int p_bus, int p_buffer) const;
	AudioFrame *thread_get_channel_mix_buffer(int p_bus, int p_buffer);
	int thread_get_mix_buffer_size() const;
	int thread_find_bus_index(const StringName &p_name);

	void set_bus_count(int p_count);
	int get_bus_count() const;

	void remove_bus(int p_index);
	void add_bus(int p_at_pos = -1);

	void move_bus(int p_bus, int p_to_pos);

	void set_bus_name(int p_bus, const String &p_name);
	String get_bus_name(int p_bus) const;
	int get_bus_index(const StringName &p_bus_name) const;

	int get_bus_channels(int p_bus) const;

	void set_bus_volume_db(int p_bus, float p_volume_db);
	float get_bus_volume_db(int p_bus) const;

	void set_bus_send(int p_bus, const StringName &p_send);
	StringName get_bus_send(int p_bus) const;

	void set_bus_solo(int p_bus, bool p_enable);
	bool is_bus_solo(int p_bus) const;

	void set_bus_mute(int p_bus, bool p_enable);
	bool is_bus_mute(int p_bus) const;

	void set_bus_bypass_effects(int p_bus, bool p_enable);
	bool is_bus_bypassing_effects(int p_bus) const;

	void add_bus_effect(int p_bus, const Ref<AudioEffect> &p_effect, int p_at_pos = -1);
	void remove_bus_effect(int p_bus, int p_effect);

	int get_bus_effect_count(int p_bus);
	Ref<AudioEffect> get_bus_effect(int p_bus, int p_effect);
	Ref<AudioEffectInstance> get_bus_effect_instance(int p_bus, int p_effect, int p_channel = 0);

	void swap_bus_effects(int p_bus, int p_effect, int p_by_effect);

	void set_bus_effect_enabled(int p_bus, int p_effect, bool p_enabled);
	bool is_bus_effect_enabled(int p_bus, int p_effect) const;

	float get_bus_peak_volume_left_db(int p_bus, int p_channel) const;
	float get_bus_peak_volume_right_db(int p_bus, int p_channel) const;

	bool is_bus_channel_active(int p_bus, int p_channel) const;

	void set_playback_speed_scale(float p_scale);
	float get_playback_speed_scale() const;

	// Convenience method.
	void start_playback_stream(Ref<AudioStreamPlayback> p_playback, const StringName &p_bus, Vector<AudioFrame> p_volume_db_vector, float p_start_time = 0, float p_pitch_scale = 1);
	// Expose all parameters.
	void start_playback_stream(Ref<AudioStreamPlayback> p_playback, const HashMap<StringName, Vector<AudioFrame>> &p_bus_volumes, float p_start_time = 0, float p_pitch_scale = 1, float p_highshelf_gain = 0, float p_attenuation_cutoff_hz = 0);
	void stop_playback_stream(Ref<AudioStreamPlayback> p_playback);

	void set_playback_bus_exclusive(Ref<AudioStreamPlayback> p_playback, const StringName &p_bus, Vector<AudioFrame> p_volumes);
	void set_playback_bus_volumes_linear(Ref<AudioStreamPlayback> p_playback, const HashMap<StringName, Vector<AudioFrame>> &p_bus_volumes);
	void set_playback_all_bus_volumes_linear(Ref<AudioStreamPlayback> p_playback, Vector<AudioFrame> p_volumes);
	void set_playback_pitch_scale(Ref<AudioStreamPlayback> p_playback, float p_pitch_scale);
	void set_playback_paused(Ref<AudioStreamPlayback> p_playback, bool p_paused);
	void set_playback_highshelf_params(Ref<AudioStreamPlayback> p_playback, float p_gain, float p_attenuation_cutoff_hz);

	bool is_playback_active(Ref<AudioStreamPlayback> p_playback);
	float get_playback_position(Ref<AudioStreamPlayback> p_playback);
	bool is_playback_paused(Ref<AudioStreamPlayback> p_playback);

	uint64_t get_mix_count() const;
	uint64_t get_mixed_frames() const;

	String get_driver_name() const;

	void notify_listener_changed();

	virtual void init();
	virtual void finish();
	virtual void update();
	virtual void load_default_bus_layout();

	/* MISC config */

	virtual void lock();
	virtual void unlock();

	virtual SpeakerMode get_speaker_mode() const;
	virtual float get_mix_rate() const;

	virtual float read_output_peak_db() const;

	static AudioServer *get_singleton();

	virtual double get_output_latency() const;
	virtual double get_time_to_next_mix() const;
	virtual double get_time_since_last_mix() const;

	void add_listener_changed_callback(AudioCallback p_callback, void *p_userdata);
	void remove_listener_changed_callback(AudioCallback p_callback, void *p_userdata);

	void add_update_callback(AudioCallback p_callback, void *p_userdata);
	void remove_update_callback(AudioCallback p_callback, void *p_userdata);

	void add_mix_callback(AudioCallback p_callback, void *p_userdata);
	void remove_mix_callback(AudioCallback p_callback, void *p_userdata);

	void set_bus_layout(const Ref<AudioBusLayout> &p_bus_layout);
	Ref<AudioBusLayout> generate_bus_layout() const;

	PackedStringArray get_output_device_list();
	String get_output_device();
	void set_output_device(const String &p_name);

	PackedStringArray get_input_device_list();
	String get_input_device();
	void set_input_device(const String &p_name);

	void set_enable_tagging_used_audio_streams(bool p_enable);

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	PlaybackType get_default_playback_type() const;

	bool is_stream_registered_as_sample(const Ref<AudioStream> &p_stream);
	void register_stream_as_sample(const Ref<AudioStream> &p_stream);
	void unregister_stream_as_sample(const Ref<AudioStream> &p_stream);
	void register_sample(const Ref<AudioSample> &p_sample);
	void unregister_sample(const Ref<AudioSample> &p_sample);
	void start_sample_playback(const Ref<AudioSamplePlayback> &p_playback);
	void stop_sample_playback(const Ref<AudioSamplePlayback> &p_playback);
	void set_sample_playback_pause(const Ref<AudioSamplePlayback> &p_playback, bool p_paused);
	bool is_sample_playback_active(const Ref<AudioSamplePlayback> &p_playback);
	double get_sample_playback_position(const Ref<AudioSamplePlayback> &p_playback);
	void update_sample_playback_pitch_scale(const Ref<AudioSamplePlayback> &p_playback, float p_pitch_scale = 0.0f);

	AudioServer();
	virtual ~AudioServer();
};

VARIANT_ENUM_CAST(AudioServer::SpeakerMode)
VARIANT_ENUM_CAST(AudioServer::PlaybackType)

class AudioBusLayout : public Resource {
	GDCLASS(AudioBusLayout, Resource);

	friend class AudioServer;

	struct Bus {
		StringName name;
		bool solo = false;
		bool mute = false;
		bool bypass = false;

		struct Effect {
			Ref<AudioEffect> effect;
			bool enabled = false;
		};

		Vector<Effect> effects;

		float volume_db = 0.0f;
		StringName send;

		Bus() {}
	};

	Vector<Bus> buses;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	AudioBusLayout();
};

typedef AudioServer AS;

#endif // AUDIO_SERVER_H
