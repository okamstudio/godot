/**************************************************************************/
/*  audio_stream_interactive.h                                            */
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

#ifndef AUDIO_STREAM_INTERACTIVE_H
#define AUDIO_STREAM_INTERACTIVE_H

#include "servers/audio/audio_stream.h"

class AudioStreamPlaybackInteractive;

class AudioStreamInteractive : public AudioStream {
	GDCLASS(AudioStreamInteractive, AudioStream)
	OBJ_SAVE_TYPE(AudioStream)
public:
	enum TransitionFromTime {
		TRANSITION_FROM_TIME_IMMEDIATE,
		TRANSITION_FROM_TIME_NEXT_BEAT,
		TRANSITION_FROM_TIME_NEXT_BAR,
		TRANSITION_FROM_TIME_END,
		TRANSITION_FROM_TIME_MAX
	};

	enum TransitionToTime {
		TRANSITION_TO_TIME_SAME_POSITION,
		TRANSITION_TO_TIME_START,
		TRANSITION_TO_TIME_PREVIOUS_POSITION,
		TRANSITION_TO_TIME_MAX,
	};

	enum FadeMode {
		FADE_DISABLED,
		FADE_IN,
		FADE_OUT,
		FADE_CROSS,
		FADE_AUTOMATIC,
		FADE_MAX
	};

	enum AutoAdvanceMode {
		AUTO_ADVANCE_DISABLED,
		AUTO_ADVANCE_ENABLED,
		AUTO_ADVANCE_RETURN_TO_HOLD,
	};

	enum {
		CLIP_ANY = -1
	};

private:
	friend class AudioStreamPlaybackInteractive;
	int sample_rate = 44100;
	bool stereo = true;
	int initial_clip = 0;

	double time = 0;

	enum {
		MAX_CLIPS = 63, // Because we use bitmasks for transition matching.
		MAX_TRANSITIONS = 63,
	};

	struct Clip {
		StringName name;
		Ref<AudioStream> stream;

		AutoAdvanceMode auto_advance = AUTO_ADVANCE_DISABLED;
		int auto_advance_next_clip = 0;
	};

	Clip clips[MAX_CLIPS];

	struct Transition {
		TransitionFromTime from_time = TRANSITION_FROM_TIME_NEXT_BEAT;
		TransitionToTime to_time = TRANSITION_TO_TIME_START;
		FadeMode fade_mode = FADE_AUTOMATIC;
		float fade_beats = 1;
		bool use_filler_clip = false;
		int filler_clip = 0;
		bool hold_previous = false;
	};

	struct TransitionKey {
		uint32_t from_clip;
		uint32_t to_clip;
		bool operator==(const TransitionKey &p_key) const {
			return from_clip == p_key.from_clip && to_clip == p_key.to_clip;
		}
		TransitionKey(uint32_t p_from_clip = 0, uint32_t p_to_clip = 0) {
			from_clip = p_from_clip;
			to_clip = p_to_clip;
		}
	};

	struct TransitionKeyHasher {
		static _FORCE_INLINE_ uint32_t hash(const TransitionKey &p_key) {
			uint32_t h = hash_murmur3_one_32(p_key.from_clip);
			return hash_murmur3_one_32(p_key.to_clip, h);
		}
	};

	HashMap<TransitionKey, Transition, TransitionKeyHasher> transition_map;

	uint64_t version = 1; // Used to stop playback instances for incompatibility.
	int clip_count = 0;

	HashSet<AudioStreamPlaybackInteractive *> playbacks;

#ifdef TOOLS_ENABLED

	mutable String stream_name_cache;
	String _get_streams_hint() const;
	PackedStringArray _get_linked_undo_properties(const String &p_property, const Variant &p_new_value) const;
	void _inspector_array_swap_clip(uint32_t p_item_a, uint32_t p_item_b);

#endif

	void _set_transitions(const Dictionary &p_transitions);
	Dictionary _get_transitions() const;

public:
	// CLIPS
	void set_clip_count(int p_count);
	int get_clip_count() const;

	void set_initial_clip(int p_clip);
	int get_initial_clip() const;

	void set_clip_name(int p_clip, const StringName &p_name);
	StringName get_clip_name(int p_clip) const;

	void set_clip_stream(int p_clip, const Ref<AudioStream> &p_stream);
	Ref<AudioStream> get_clip_stream(int p_clip) const;

	void set_clip_auto_advance(int p_clip, AutoAdvanceMode p_mode);
	AutoAdvanceMode get_clip_auto_advance(int p_clip) const;

	void set_clip_auto_advance_next_clip(int p_clip, int p_index);
	int get_clip_auto_advance_next_clip(int p_clip) const;

	// TRANSITIONS

	void add_transition(int p_from_clip, int p_to_clip, TransitionFromTime p_from_time, TransitionToTime p_to_time, FadeMode p_fade_mode, float p_fade_beats, bool p_use_filler_flip = false, int p_filler_clip = -1, bool p_hold_previous = false);
	TransitionFromTime get_transition_from_time(int p_from_clip, int p_to_clip) const;
	TransitionToTime get_transition_to_time(int p_from_clip, int p_to_clip) const;
	FadeMode get_transition_fade_mode(int p_from_clip, int p_to_clip) const;
	float get_transition_fade_beats(int p_from_clip, int p_to_clip) const;
	bool is_transition_using_filler_clip(int p_from_clip, int p_to_clip) const;
	int get_transition_filler_clip(int p_from_clip, int p_to_clip) const;
	bool is_transition_holding_previous(int p_from_clip, int p_to_clip) const;

	bool has_transition(int p_from_clip, int p_to_clip) const;
	void erase_transition(int p_from_clip, int p_to_clip);

	PackedInt32Array get_transition_list() const;

	virtual Ref<AudioStreamPlayback> instantiate_playback() override;
	virtual String get_stream_name() const override;
	virtual double get_length() const override { return 0; }
	AudioStreamInteractive();

protected:
	virtual void get_parameter_list(List<Parameter> *r_parameters) override;

	static void _bind_methods();
	void _validate_property(PropertyInfo &r_property) const;
};

VARIANT_ENUM_CAST(AudioStreamInteractive::TransitionFromTime)
VARIANT_ENUM_CAST(AudioStreamInteractive::TransitionToTime)
VARIANT_ENUM_CAST(AudioStreamInteractive::AutoAdvanceMode)
VARIANT_ENUM_CAST(AudioStreamInteractive::FadeMode)

class AudioStreamPlaybackInteractive : public AudioStreamPlayback {
	GDCLASS(AudioStreamPlaybackInteractive, AudioStreamPlayback)
	friend class AudioStreamInteractive;

private:
	Ref<AudioStreamInteractive> stream;
	uint64_t version = 0;

	enum {
		BUFFER_SIZE = 1024
	};

	AudioFrame mix_buffer[BUFFER_SIZE];
	AudioFrame temp_buffer[BUFFER_SIZE];

	struct State {
		Ref<AudioStream> stream;
		Ref<AudioStreamPlayback> playback;
		bool active = false;
		double fade_wait = 0; // Time to wait until fade kicks-in
		double fade_volume = 1.0;
		double fade_speed = 0; // Fade speed, negative or positive
		int auto_advance = -1;
		bool first_mix = true;
		double previous_position = 0;

		void reset_fade() {
			fade_wait = 0;
			fade_volume = 1.0;
			fade_speed = 0;
		}
	};

	State states[AudioStreamInteractive::MAX_CLIPS];
	int playback_current = -1;

	bool active = false;
	int return_memory = -1;

	void _mix_internal(int p_frames);
	void _mix_internal_state(int p_state_idx, int p_frames);

	void _queue(int p_to_clip_index, bool p_is_auto_advance);

	int switch_request = -1;

protected:
	static void _bind_methods();

public:
	virtual void start(double p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;
	virtual int get_loop_count() const override; // times it looped
	virtual double get_playback_position() const override;
	virtual void seek(double p_time) override;
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;

	virtual void tag_used_streams() override;

	void switch_to_clip_by_name(const StringName &p_name);
	void switch_to_clip(int p_index);
	int get_current_clip_index() const;

	virtual void set_parameter(const StringName &p_name, const Variant &p_value) override;
	virtual Variant get_parameter(const StringName &p_name) const override;

	AudioStreamPlaybackInteractive();
	~AudioStreamPlaybackInteractive();
};

#endif // AUDIO_STREAM_INTERACTIVE_H
