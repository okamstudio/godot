/**************************************************************************/
/*  audio_stream_player_3d.cpp                                            */
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

#include "audio_stream_player_3d.h"
#include "audio_stream_player_3d.compat.inc"

#include "core/config/project_settings.h"
#include "scene/3d/audio_listener_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/physics/area_3d.h"
#include "scene/3d/velocity_tracker_3d.h"
#include "scene/audio/audio_stream_player_internal.h"
#include "scene/main/viewport.h"
#include "servers/audio/audio_stream.h"

// Based on "A Novel Multichannel Panning Method for Standard and Arbitrary Loudspeaker Configurations" by Ramy Sadek and Chris Kyriakakis (2004)
// Speaker-Placement Correction Amplitude Panning (SPCAP)
class Spcap {
private:
	struct Speaker {
		Vector3 direction;
		real_t effective_number_of_speakers = 0; // precalculated
		mutable real_t squared_gain = 0; // temporary
	};

	Vector<Speaker> speakers;

public:
	Spcap(unsigned int speaker_count, const Vector3 *speaker_directions) {
		speakers.resize(speaker_count);
		Speaker *w = speakers.ptrw();
		for (unsigned int speaker_num = 0; speaker_num < speaker_count; speaker_num++) {
			w[speaker_num].direction = speaker_directions[speaker_num];
			w[speaker_num].squared_gain = 0.0;
			w[speaker_num].effective_number_of_speakers = 0.0;
			for (unsigned int other_speaker_num = 0; other_speaker_num < speaker_count; other_speaker_num++) {
				w[speaker_num].effective_number_of_speakers += 0.5 * (1.0 + w[speaker_num].direction.dot(w[other_speaker_num].direction));
			}
		}
	}

	unsigned int get_speaker_count() const {
		return (unsigned int)speakers.size();
	}

	Vector3 get_speaker_direction(unsigned int index) const {
		return speakers.ptr()[index].direction;
	}

	void calculate(const Vector3 &source_direction, real_t tightness, unsigned int volume_count, real_t *volumes) const {
		const Speaker *r = speakers.ptr();
		real_t sum_squared_gains = 0.0;
		for (unsigned int speaker_num = 0; speaker_num < (unsigned int)speakers.size(); speaker_num++) {
			real_t initial_gain = 0.5 * powf(1.0 + r[speaker_num].direction.dot(source_direction), tightness) / r[speaker_num].effective_number_of_speakers;
			r[speaker_num].squared_gain = initial_gain * initial_gain;
			sum_squared_gains += r[speaker_num].squared_gain;
		}

		for (unsigned int speaker_num = 0; speaker_num < MIN(volume_count, (unsigned int)speakers.size()); speaker_num++) {
			volumes[speaker_num] = sqrtf(r[speaker_num].squared_gain / sum_squared_gains);
		}
	}
};

//TODO: hardcoded main speaker directions for 2, 3.1, 5.1 and 7.1 setups - these are simplified and could also be made configurable
static const Vector3 speaker_directions[7] = {
	Vector3(-1.0, 0.0, -1.0).normalized(), // front-left
	Vector3(1.0, 0.0, -1.0).normalized(), // front-right
	Vector3(0.0, 0.0, -1.0).normalized(), // center
	Vector3(-1.0, 0.0, 1.0).normalized(), // rear-left
	Vector3(1.0, 0.0, 1.0).normalized(), // rear-right
	Vector3(-1.0, 0.0, 0.0).normalized(), // side-left
	Vector3(1.0, 0.0, 0.0).normalized(), // side-right
};

void AudioStreamPlayer3D::_calc_output_vol(const Vector3 &source_dir, real_t tightness, Vector<AudioFrame> &output) {
	unsigned int speaker_count = 0; // only main speakers (no LFE)
	switch (AudioServer::get_singleton()->get_speaker_mode()) {
		case AudioServer::SPEAKER_MODE_STEREO:
			speaker_count = 2;
			break;
		case AudioServer::SPEAKER_SURROUND_31:
			speaker_count = 3;
			break;
		case AudioServer::SPEAKER_SURROUND_51:
			speaker_count = 5;
			break;
		case AudioServer::SPEAKER_SURROUND_71:
			speaker_count = 7;
			break;
	}

	Spcap spcap(speaker_count, speaker_directions); //TODO: should only be created/recreated once the speaker mode / speaker positions changes
	real_t volumes[7];
	spcap.calculate(source_dir, tightness, speaker_count, volumes);

	switch (AudioServer::get_singleton()->get_speaker_mode()) {
		case AudioServer::SPEAKER_SURROUND_71:
			output.write[3].left = volumes[5]; // side-left
			output.write[3].right = volumes[6]; // side-right
			[[fallthrough]];
		case AudioServer::SPEAKER_SURROUND_51:
			output.write[2].left = volumes[3]; // rear-left
			output.write[2].right = volumes[4]; // rear-right
			[[fallthrough]];
		case AudioServer::SPEAKER_SURROUND_31:
			output.write[1].right = 1.0; // LFE - always full power
			output.write[1].left = volumes[2]; // center
			[[fallthrough]];
		case AudioServer::SPEAKER_MODE_STEREO:
			output.write[0].right = volumes[1]; // front-right
			output.write[0].left = volumes[0]; // front-left
			break;
	}
}

void AudioStreamPlayer3D::_calc_reverb_vol(Area3D *area, Vector3 listener_area_pos, Vector<AudioFrame> direct_path_vol, Vector<AudioFrame> &reverb_vol) {
	reverb_vol.resize(4);
	reverb_vol.write[0] = AudioFrame(0, 0);
	reverb_vol.write[1] = AudioFrame(0, 0);
	reverb_vol.write[2] = AudioFrame(0, 0);
	reverb_vol.write[3] = AudioFrame(0, 0);

	float uniformity = area->get_reverb_uniformity();
	float area_send = area->get_reverb_amount();

	if (uniformity > 0.0) {
		float distance = listener_area_pos.length();
		float attenuation = Math::db_to_linear(_get_attenuation_db(distance));

		// Determine the fraction of sound that would come from each speaker if they were all driven uniformly.
		float center_val[3] = { 0.5f, 0.25f, 0.16666f };
		int channel_count = AudioServer::get_singleton()->get_channel_count();
		AudioFrame center_frame(center_val[channel_count - 1], center_val[channel_count - 1]);

		if (attenuation < 1.0) {
			//pan the uniform sound
			Vector3 rev_pos = listener_area_pos;
			rev_pos.y = 0;
			rev_pos.normalize();

			// Stereo pair.
			float c = rev_pos.x * 0.5 + 0.5;
			reverb_vol.write[0].left = 1.0 - c;
			reverb_vol.write[0].right = c;

			if (channel_count >= 3) {
				// Center pair + Side pair
				float xl = Vector3(-1, 0, -1).normalized().dot(rev_pos) * 0.5 + 0.5;
				float xr = Vector3(1, 0, -1).normalized().dot(rev_pos) * 0.5 + 0.5;

				reverb_vol.write[1].left = xl;
				reverb_vol.write[1].right = xr;
				reverb_vol.write[2].left = 1.0 - xr;
				reverb_vol.write[2].right = 1.0 - xl;
			}

			if (channel_count >= 4) {
				// Rear pair
				// FIXME: Not sure what math should be done here
				reverb_vol.write[3].left = 1.0 - c;
				reverb_vol.write[3].right = c;
			}

			for (int i = 0; i < channel_count; i++) {
				reverb_vol.write[i] = reverb_vol[i].lerp(center_frame, attenuation);
			}
		} else {
			for (int i = 0; i < channel_count; i++) {
				reverb_vol.write[i] = center_frame;
			}
		}

		for (int i = 0; i < channel_count; i++) {
			reverb_vol.write[i] = direct_path_vol[i].lerp(reverb_vol[i] * attenuation, uniformity);
			reverb_vol.write[i] *= area_send;
		}

	} else {
		for (int i = 0; i < 4; i++) {
			reverb_vol.write[i] = direct_path_vol[i] * area_send;
		}
	}
}

float AudioStreamPlayer3D::_get_attenuation_db(float p_distance) const {
	float att = 0;
	switch (attenuation_model) {
		case ATTENUATION_INVERSE_DISTANCE: {
			att = Math::linear_to_db(1.0 / ((p_distance / unit_size) + CMP_EPSILON));
		} break;
		case ATTENUATION_INVERSE_SQUARE_DISTANCE: {
			float d = (p_distance / unit_size);
			d *= d;
			att = Math::linear_to_db(1.0 / (d + CMP_EPSILON));
		} break;
		case ATTENUATION_LOGARITHMIC: {
			att = -20 * Math::log(p_distance / unit_size + CMP_EPSILON);
		} break;
		case ATTENUATION_DISABLED:
			break;
		default: {
			ERR_PRINT("Unknown attenuation type");
			break;
		}
	}

	att += internal->volume_db;
	if (att > max_db) {
		att = max_db;
	}

	return att;
}

void AudioStreamPlayer3D::_notification(int p_what) {
	internal->notification(p_what);
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			velocity_tracker->reset(get_global_transform().origin);
			AudioServer::get_singleton()->add_listener_changed_callback(_listener_changed_cb, this);
		} break;

		case NOTIFICATION_EXIT_TREE: {
			AudioServer::get_singleton()->remove_listener_changed_callback(_listener_changed_cb, this);
		} break;

		case NOTIFICATION_TRANSFORM_CHANGED: {
			if (doppler_tracking != DOPPLER_TRACKING_DISABLED) {
				velocity_tracker->update_position(get_global_transform().origin);
			}
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			// Update anything related to position first, if possible of course.
			Vector<AudioFrame> volume_vector;
			if (setplay.get() > 0 || (internal->active.is_set() && last_mix_count != AudioServer::get_singleton()->get_mix_count()) || force_update_panning) {
				force_update_panning = false;
				volume_vector = _update_panning();
			}

			if (setplayback.is_valid() && setplay.get() >= 0) {
				internal->active.set();
				HashMap<StringName, Vector<AudioFrame>> bus_map;
				bus_map[_get_actual_bus()] = volume_vector;
				AudioServer::get_singleton()->start_playback_stream(setplayback, bus_map, setplay.get(), actual_pitch_scale, linear_attenuation, attenuation_filter_cutoff_hz);
				setplayback.unref();
				setplay.set(-1);
			}

			if (!internal->stream_playbacks.is_empty() && internal->active.is_set()) {
				internal->process();
			}
			internal->ensure_playback_limit();
		} break;
	}
}

// Interacts with PhysicsServer3D, so can only be called during _physics_process
Area3D *AudioStreamPlayer3D::_get_overriding_area() {
	//check if any area is diverting sound into a bus
	Ref<World3D> world_3d = get_world_3d();
	ERR_FAIL_COND_V(world_3d.is_null(), nullptr);

	Vector3 global_pos = get_global_transform().origin;

	PhysicsDirectSpaceState3D *space_state = PhysicsServer3D::get_singleton()->space_get_direct_state(world_3d->get_space());

	PhysicsDirectSpaceState3D::ShapeResult sr[MAX_INTERSECT_AREAS];

	PhysicsDirectSpaceState3D::PointParameters point_params;
	point_params.position = global_pos;
	point_params.collision_mask = area_mask;
	point_params.collide_with_bodies = false;
	point_params.collide_with_areas = true;

	int areas = space_state->intersect_point(point_params, sr, MAX_INTERSECT_AREAS);

	for (int i = 0; i < areas; i++) {
		if (!sr[i].collider) {
			continue;
		}

		Area3D *tarea = Object::cast_to<Area3D>(sr[i].collider);
		if (!tarea) {
			continue;
		}

		if (!tarea->is_overriding_audio_bus() && !tarea->is_using_reverb_bus()) {
			continue;
		}

		return tarea;
	}
	return nullptr;
}

// Interacts with PhysicsServer3D, so can only be called during _physics_process.
StringName AudioStreamPlayer3D::_get_actual_bus() {
	Area3D *overriding_area = _get_overriding_area();
	if (overriding_area && overriding_area->is_overriding_audio_bus() && !overriding_area->is_using_reverb_bus()) {
		return overriding_area->get_audio_bus_name();
	}
	return internal->bus;
}

// Interacts with PhysicsServer3D, so can only be called during _physics_process.
Vector<AudioFrame> AudioStreamPlayer3D::_update_panning() {
	Vector<AudioFrame> output_volume_vector;
	output_volume_vector.resize(4);
	for (AudioFrame &frame : output_volume_vector) {
		frame = AudioFrame(0, 0);
	}

	if (!internal->active.is_set() || internal->stream.is_null()) {
		return output_volume_vector;
	}

	Vector3 linear_velocity;

	//compute linear velocity for doppler
	if (doppler_tracking != DOPPLER_TRACKING_DISABLED) {
		linear_velocity = velocity_tracker->get_tracked_linear_velocity();
	}

	Vector3 global_pos = get_global_transform().origin;

	Ref<World3D> world_3d = get_world_3d();
	ERR_FAIL_COND_V(world_3d.is_null(), output_volume_vector);

	HashSet<Camera3D *> cameras = world_3d->get_cameras();
	cameras.insert(get_viewport()->get_camera_3d());

	PhysicsDirectSpaceState3D *space_state = PhysicsServer3D::get_singleton()->space_get_direct_state(world_3d->get_space());

	for (Camera3D *camera : cameras) {
		if (!camera) {
			continue;
		}
		Viewport *vp = camera->get_viewport();
		if (!vp) {
			continue;
		}
		if (!vp->is_audio_listener_3d()) {
			continue;
		}

		bool listener_is_camera = true;
		Node3D *listener_node = camera;

		AudioListener3D *listener = vp->get_audio_listener_3d();
		if (listener) {
			listener_node = listener;
			listener_is_camera = false;
		}

		Vector3 local_pos = listener_node->get_global_transform().orthonormalized().affine_inverse().xform(global_pos);

		float dist = local_pos.length();

		Vector3 area_sound_pos;
		Vector3 listener_area_pos;

		Area3D *area = _get_overriding_area();

		if (area && area->is_using_reverb_bus() && area->get_reverb_uniformity() > 0) {
			area_sound_pos = space_state->get_closest_point_to_object_volume(area->get_rid(), listener_node->get_global_transform().origin);
			listener_area_pos = listener_node->get_global_transform().affine_inverse().xform(area_sound_pos);
		}

		if (max_distance > 0) {
			float total_max = max_distance;

			if (area && area->is_using_reverb_bus() && area->get_reverb_uniformity() > 0) {
				total_max = MAX(total_max, listener_area_pos.length());
			}
			if (dist > total_max || total_max > max_distance) {
				if (!was_further_than_max_distance_last_frame) {
					HashMap<StringName, Vector<AudioFrame>> bus_volumes;
					for (Ref<AudioStreamPlayback> &playback : internal->stream_playbacks) {
						// So the player gets muted and mostly stops mixing when out of range.
						AudioServer::get_singleton()->set_playback_bus_volumes_linear(playback, bus_volumes);
					}
					was_further_than_max_distance_last_frame = true; // Cache so we don't set the volume over and over.
				}
				continue; //can't hear this sound in this listener
			}
		}
		was_further_than_max_distance_last_frame = false;

		float multiplier = Math::db_to_linear(_get_attenuation_db(dist));
		if (max_distance > 0) {
			multiplier *= MAX(0, 1.0 - (dist / max_distance));
		}

		float db_att = (1.0 - MIN(1.0, multiplier)) * attenuation_filter_db;

		if (emission_angle_enabled) {
			Vector3 listenertopos = global_pos - listener_node->get_global_transform().origin;
			float c = listenertopos.normalized().dot(get_global_transform().basis.get_column(2).normalized()); //it's z negative
			float angle = Math::rad_to_deg(Math::acos(c));
			if (angle > emission_angle) {
				db_att -= -emission_angle_filter_attenuation_db;
			}
		}

		linear_attenuation = Math::db_to_linear(db_att);
		for (Ref<AudioStreamPlayback> &playback : internal->stream_playbacks) {
			AudioServer::get_singleton()->set_playback_highshelf_params(playback, linear_attenuation, attenuation_filter_cutoff_hz);
		}
		// Bake in a constant factor here to allow the project setting defaults for 2d and 3d to be normalized to 1.0.
		float tightness = cached_global_panning_strength * 2.0f;
		tightness *= panning_strength;
		_calc_output_vol(local_pos.normalized(), tightness, output_volume_vector);

		for (unsigned int k = 0; k < 4; k++) {
			output_volume_vector.write[k] = multiplier * output_volume_vector[k];
		}

		HashMap<StringName, Vector<AudioFrame>> bus_volumes;
		if (area) {
			if (area->is_overriding_audio_bus()) {
				//override audio bus
				bus_volumes[area->get_audio_bus_name()] = output_volume_vector;
			}

			if (area->is_using_reverb_bus()) {
				StringName reverb_bus_name = area->get_reverb_bus_name();
				Vector<AudioFrame> reverb_vol;
				_calc_reverb_vol(area, listener_area_pos, output_volume_vector, reverb_vol);
				bus_volumes[reverb_bus_name] = reverb_vol;
			}
		} else {
			bus_volumes[internal->bus] = output_volume_vector;
		}

		for (Ref<AudioStreamPlayback> &playback : internal->stream_playbacks) {
			AudioServer::get_singleton()->set_playback_bus_volumes_linear(playback, bus_volumes);
		}

		if (doppler_tracking != DOPPLER_TRACKING_DISABLED) {
			Vector3 listener_velocity;

			if (listener_is_camera) {
				listener_velocity = camera->get_doppler_tracked_velocity();
			}

			Vector3 local_velocity = listener_node->get_global_transform().orthonormalized().basis.xform_inv(linear_velocity - listener_velocity);

			if (local_velocity != Vector3()) {
				float approaching = local_pos.normalized().dot(local_velocity.normalized());
				float velocity = local_velocity.length();
				float speed_of_sound = 343.0;

				float doppler_pitch_scale = internal->pitch_scale * speed_of_sound / (speed_of_sound + velocity * approaching);
				doppler_pitch_scale = CLAMP(doppler_pitch_scale, (1 / 8.0), 8.0); //avoid crazy stuff

				actual_pitch_scale = doppler_pitch_scale;
			} else {
				actual_pitch_scale = internal->pitch_scale;
			}
		} else {
			actual_pitch_scale = internal->pitch_scale;
		}
		for (Ref<AudioStreamPlayback> &playback : internal->stream_playbacks) {
			AudioServer::get_singleton()->set_playback_pitch_scale(playback, actual_pitch_scale);
			if (playback->get_is_sample()) {
				Ref<AudioSamplePlayback> sample_playback = playback->get_sample_playback();
				if (sample_playback.is_valid()) {
					AudioServer::get_singleton()->update_sample_playback_pitch_scale(sample_playback, actual_pitch_scale);
				}
			}
		}
	}
	return output_volume_vector;
}

void AudioStreamPlayer3D::set_stream(Ref<AudioStream> p_stream) {
	internal->set_stream(p_stream);
}

Ref<AudioStream> AudioStreamPlayer3D::get_stream() const {
	return internal->stream;
}

void AudioStreamPlayer3D::set_volume_db(float p_volume) {
	ERR_FAIL_COND_MSG(Math::is_nan(p_volume), "Volume can't be set to NaN.");
	internal->volume_db = p_volume;
}

float AudioStreamPlayer3D::get_volume_db() const {
	return internal->volume_db;
}

void AudioStreamPlayer3D::set_volume_linear(float p_volume) {
	set_volume_db(Math::linear_to_db(p_volume));
}

float AudioStreamPlayer3D::get_volume_linear() const {
	return Math::db_to_linear(get_volume_db());
}

void AudioStreamPlayer3D::set_unit_size(float p_volume) {
	unit_size = p_volume;
	update_gizmos();
}

float AudioStreamPlayer3D::get_unit_size() const {
	return unit_size;
}

void AudioStreamPlayer3D::set_max_db(float p_boost) {
	max_db = p_boost;
}

float AudioStreamPlayer3D::get_max_db() const {
	return max_db;
}

void AudioStreamPlayer3D::set_pitch_scale(float p_pitch_scale) {
	internal->set_pitch_scale(p_pitch_scale);
}

float AudioStreamPlayer3D::get_pitch_scale() const {
	return internal->pitch_scale;
}

void AudioStreamPlayer3D::play(float p_from_pos) {
	Ref<AudioStreamPlayback> stream_playback = internal->play_basic();
	if (stream_playback.is_null()) {
		return;
	}
	setplayback = stream_playback;
	setplay.set(p_from_pos);

	// Sample handling.
	if (stream_playback->get_is_sample() && stream_playback->get_sample_playback().is_valid()) {
		Ref<AudioSamplePlayback> sample_playback = stream_playback->get_sample_playback();
		sample_playback->offset = p_from_pos;
		sample_playback->bus = _get_actual_bus();

		AudioServer::get_singleton()->start_sample_playback(sample_playback);
	}
}

void AudioStreamPlayer3D::seek(float p_seconds) {
	internal->seek(p_seconds);
}

void AudioStreamPlayer3D::stop() {
	setplay.set(-1);
	internal->stop_basic();
}

bool AudioStreamPlayer3D::is_playing() const {
	if (setplay.get() >= 0) {
		return true; // play() has been called this frame, but no playback exists just yet.
	}
	return internal->is_playing();
}

float AudioStreamPlayer3D::get_playback_position() {
	if (setplay.get() >= 0) {
		return setplay.get(); // play() has been called this frame, but no playback exists just yet.
	}
	return internal->get_playback_position();
}

void AudioStreamPlayer3D::set_bus(const StringName &p_bus) {
	internal->bus = p_bus; // This will be pushed to the audio server during the next physics timestep, which is fast enough.
}

StringName AudioStreamPlayer3D::get_bus() const {
	return internal->get_bus();
}

void AudioStreamPlayer3D::set_autoplay(bool p_enable) {
	internal->autoplay = p_enable;
}

bool AudioStreamPlayer3D::is_autoplay_enabled() const {
	return internal->autoplay;
}

void AudioStreamPlayer3D::_set_playing(bool p_enable) {
	internal->set_playing(p_enable);
}

void AudioStreamPlayer3D::_validate_property(PropertyInfo &p_property) const {
	internal->validate_property(p_property);
}

void AudioStreamPlayer3D::set_max_distance(float p_metres) {
	ERR_FAIL_COND(p_metres < 0.0);
	max_distance = p_metres;
	update_gizmos();
}

float AudioStreamPlayer3D::get_max_distance() const {
	return max_distance;
}

void AudioStreamPlayer3D::set_area_mask(uint32_t p_mask) {
	area_mask = p_mask;
}

uint32_t AudioStreamPlayer3D::get_area_mask() const {
	return area_mask;
}

void AudioStreamPlayer3D::set_emission_angle_enabled(bool p_enable) {
	emission_angle_enabled = p_enable;
	update_gizmos();
}

bool AudioStreamPlayer3D::is_emission_angle_enabled() const {
	return emission_angle_enabled;
}

void AudioStreamPlayer3D::set_emission_angle(float p_angle) {
	ERR_FAIL_COND(p_angle < 0 || p_angle > 90);
	emission_angle = p_angle;
	update_gizmos();
}

float AudioStreamPlayer3D::get_emission_angle() const {
	return emission_angle;
}

void AudioStreamPlayer3D::set_emission_angle_filter_attenuation_db(float p_angle_attenuation_db) {
	emission_angle_filter_attenuation_db = p_angle_attenuation_db;
}

float AudioStreamPlayer3D::get_emission_angle_filter_attenuation_db() const {
	return emission_angle_filter_attenuation_db;
}

void AudioStreamPlayer3D::set_attenuation_filter_cutoff_hz(float p_hz) {
	attenuation_filter_cutoff_hz = p_hz;
}

float AudioStreamPlayer3D::get_attenuation_filter_cutoff_hz() const {
	return attenuation_filter_cutoff_hz;
}

void AudioStreamPlayer3D::set_attenuation_filter_db(float p_db) {
	attenuation_filter_db = p_db;
}

float AudioStreamPlayer3D::get_attenuation_filter_db() const {
	return attenuation_filter_db;
}

void AudioStreamPlayer3D::set_attenuation_model(AttenuationModel p_model) {
	ERR_FAIL_INDEX((int)p_model, 4);
	attenuation_model = p_model;
	update_gizmos();
}

AudioStreamPlayer3D::AttenuationModel AudioStreamPlayer3D::get_attenuation_model() const {
	return attenuation_model;
}

void AudioStreamPlayer3D::set_doppler_tracking(DopplerTracking p_tracking) {
	if (doppler_tracking == p_tracking) {
		return;
	}

	doppler_tracking = p_tracking;

	if (doppler_tracking != DOPPLER_TRACKING_DISABLED) {
		set_notify_transform(true);
		velocity_tracker->set_track_physics_step(doppler_tracking == DOPPLER_TRACKING_PHYSICS_STEP);
		if (is_inside_tree()) {
			velocity_tracker->reset(get_global_transform().origin);
		}
	} else {
		set_notify_transform(false);
	}
}

AudioStreamPlayer3D::DopplerTracking AudioStreamPlayer3D::get_doppler_tracking() const {
	return doppler_tracking;
}

void AudioStreamPlayer3D::set_stream_paused(bool p_pause) {
	internal->set_stream_paused(p_pause);
}

bool AudioStreamPlayer3D::get_stream_paused() const {
	return internal->get_stream_paused();
}

bool AudioStreamPlayer3D::has_stream_playback() {
	return internal->has_stream_playback();
}

Ref<AudioStreamPlayback> AudioStreamPlayer3D::get_stream_playback() {
	return internal->get_stream_playback();
}

void AudioStreamPlayer3D::set_max_polyphony(int p_max_polyphony) {
	internal->set_max_polyphony(p_max_polyphony);
}

int AudioStreamPlayer3D::get_max_polyphony() const {
	return internal->max_polyphony;
}

void AudioStreamPlayer3D::set_panning_strength(float p_panning_strength) {
	ERR_FAIL_COND_MSG(p_panning_strength < 0, "Panning strength must be a positive number.");
	panning_strength = p_panning_strength;
}

float AudioStreamPlayer3D::get_panning_strength() const {
	return panning_strength;
}

AudioServer::PlaybackType AudioStreamPlayer3D::get_playback_type() const {
	return internal->get_playback_type();
}

void AudioStreamPlayer3D::set_playback_type(AudioServer::PlaybackType p_playback_type) {
	internal->set_playback_type(p_playback_type);
}

bool AudioStreamPlayer3D::_set(const StringName &p_name, const Variant &p_value) {
	return internal->set(p_name, p_value);
}

bool AudioStreamPlayer3D::_get(const StringName &p_name, Variant &r_ret) const {
	return internal->get(p_name, r_ret);
}

void AudioStreamPlayer3D::_get_property_list(List<PropertyInfo> *p_list) const {
	internal->get_property_list(p_list);
}

void AudioStreamPlayer3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_stream", "stream"), &AudioStreamPlayer3D::set_stream);
	ClassDB::bind_method(D_METHOD("get_stream"), &AudioStreamPlayer3D::get_stream);

	ClassDB::bind_method(D_METHOD("set_volume_db", "volume_db"), &AudioStreamPlayer3D::set_volume_db);
	ClassDB::bind_method(D_METHOD("get_volume_db"), &AudioStreamPlayer3D::get_volume_db);

	ClassDB::bind_method(D_METHOD("set_volume_linear", "volume_linear"), &AudioStreamPlayer3D::set_volume_linear);
	ClassDB::bind_method(D_METHOD("get_volume_linear"), &AudioStreamPlayer3D::get_volume_linear);

	ClassDB::bind_method(D_METHOD("set_unit_size", "unit_size"), &AudioStreamPlayer3D::set_unit_size);
	ClassDB::bind_method(D_METHOD("get_unit_size"), &AudioStreamPlayer3D::get_unit_size);

	ClassDB::bind_method(D_METHOD("set_max_db", "max_db"), &AudioStreamPlayer3D::set_max_db);
	ClassDB::bind_method(D_METHOD("get_max_db"), &AudioStreamPlayer3D::get_max_db);

	ClassDB::bind_method(D_METHOD("set_pitch_scale", "pitch_scale"), &AudioStreamPlayer3D::set_pitch_scale);
	ClassDB::bind_method(D_METHOD("get_pitch_scale"), &AudioStreamPlayer3D::get_pitch_scale);

	ClassDB::bind_method(D_METHOD("play", "from_position"), &AudioStreamPlayer3D::play, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("seek", "to_position"), &AudioStreamPlayer3D::seek);
	ClassDB::bind_method(D_METHOD("stop"), &AudioStreamPlayer3D::stop);

	ClassDB::bind_method(D_METHOD("is_playing"), &AudioStreamPlayer3D::is_playing);
	ClassDB::bind_method(D_METHOD("get_playback_position"), &AudioStreamPlayer3D::get_playback_position);

	ClassDB::bind_method(D_METHOD("set_bus", "bus"), &AudioStreamPlayer3D::set_bus);
	ClassDB::bind_method(D_METHOD("get_bus"), &AudioStreamPlayer3D::get_bus);

	ClassDB::bind_method(D_METHOD("set_autoplay", "enable"), &AudioStreamPlayer3D::set_autoplay);
	ClassDB::bind_method(D_METHOD("is_autoplay_enabled"), &AudioStreamPlayer3D::is_autoplay_enabled);

	ClassDB::bind_method(D_METHOD("set_playing", "enable"), &AudioStreamPlayer3D::_set_playing);

	ClassDB::bind_method(D_METHOD("set_max_distance", "meters"), &AudioStreamPlayer3D::set_max_distance);
	ClassDB::bind_method(D_METHOD("get_max_distance"), &AudioStreamPlayer3D::get_max_distance);

	ClassDB::bind_method(D_METHOD("set_area_mask", "mask"), &AudioStreamPlayer3D::set_area_mask);
	ClassDB::bind_method(D_METHOD("get_area_mask"), &AudioStreamPlayer3D::get_area_mask);

	ClassDB::bind_method(D_METHOD("set_emission_angle", "degrees"), &AudioStreamPlayer3D::set_emission_angle);
	ClassDB::bind_method(D_METHOD("get_emission_angle"), &AudioStreamPlayer3D::get_emission_angle);

	ClassDB::bind_method(D_METHOD("set_emission_angle_enabled", "enabled"), &AudioStreamPlayer3D::set_emission_angle_enabled);
	ClassDB::bind_method(D_METHOD("is_emission_angle_enabled"), &AudioStreamPlayer3D::is_emission_angle_enabled);

	ClassDB::bind_method(D_METHOD("set_emission_angle_filter_attenuation_db", "db"), &AudioStreamPlayer3D::set_emission_angle_filter_attenuation_db);
	ClassDB::bind_method(D_METHOD("get_emission_angle_filter_attenuation_db"), &AudioStreamPlayer3D::get_emission_angle_filter_attenuation_db);

	ClassDB::bind_method(D_METHOD("set_attenuation_filter_cutoff_hz", "degrees"), &AudioStreamPlayer3D::set_attenuation_filter_cutoff_hz);
	ClassDB::bind_method(D_METHOD("get_attenuation_filter_cutoff_hz"), &AudioStreamPlayer3D::get_attenuation_filter_cutoff_hz);

	ClassDB::bind_method(D_METHOD("set_attenuation_filter_db", "db"), &AudioStreamPlayer3D::set_attenuation_filter_db);
	ClassDB::bind_method(D_METHOD("get_attenuation_filter_db"), &AudioStreamPlayer3D::get_attenuation_filter_db);

	ClassDB::bind_method(D_METHOD("set_attenuation_model", "model"), &AudioStreamPlayer3D::set_attenuation_model);
	ClassDB::bind_method(D_METHOD("get_attenuation_model"), &AudioStreamPlayer3D::get_attenuation_model);

	ClassDB::bind_method(D_METHOD("set_doppler_tracking", "mode"), &AudioStreamPlayer3D::set_doppler_tracking);
	ClassDB::bind_method(D_METHOD("get_doppler_tracking"), &AudioStreamPlayer3D::get_doppler_tracking);

	ClassDB::bind_method(D_METHOD("set_stream_paused", "pause"), &AudioStreamPlayer3D::set_stream_paused);
	ClassDB::bind_method(D_METHOD("get_stream_paused"), &AudioStreamPlayer3D::get_stream_paused);

	ClassDB::bind_method(D_METHOD("set_max_polyphony", "max_polyphony"), &AudioStreamPlayer3D::set_max_polyphony);
	ClassDB::bind_method(D_METHOD("get_max_polyphony"), &AudioStreamPlayer3D::get_max_polyphony);

	ClassDB::bind_method(D_METHOD("set_panning_strength", "panning_strength"), &AudioStreamPlayer3D::set_panning_strength);
	ClassDB::bind_method(D_METHOD("get_panning_strength"), &AudioStreamPlayer3D::get_panning_strength);

	ClassDB::bind_method(D_METHOD("has_stream_playback"), &AudioStreamPlayer3D::has_stream_playback);
	ClassDB::bind_method(D_METHOD("get_stream_playback"), &AudioStreamPlayer3D::get_stream_playback);

	ClassDB::bind_method(D_METHOD("set_playback_type", "playback_type"), &AudioStreamPlayer3D::set_playback_type);
	ClassDB::bind_method(D_METHOD("get_playback_type"), &AudioStreamPlayer3D::get_playback_type);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "stream", PROPERTY_HINT_RESOURCE_TYPE, "AudioStream"), "set_stream", "get_stream");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "attenuation_model", PROPERTY_HINT_ENUM, "Inverse,Inverse Square,Logarithmic,Disabled"), "set_attenuation_model", "get_attenuation_model");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_db", PROPERTY_HINT_RANGE, "-80,80,suffix:dB"), "set_volume_db", "get_volume_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume_linear", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_volume_linear", "get_volume_linear");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_size", PROPERTY_HINT_RANGE, "0.1,100,0.01,or_greater"), "set_unit_size", "get_unit_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_db", PROPERTY_HINT_RANGE, "-24,6,suffix:dB"), "set_max_db", "get_max_db");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch_scale", PROPERTY_HINT_RANGE, "0.01,4,0.01,or_greater"), "set_pitch_scale", "get_pitch_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing", PROPERTY_HINT_ONESHOT, "", PROPERTY_USAGE_EDITOR), "set_playing", "is_playing");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoplay"), "set_autoplay", "is_autoplay_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "stream_paused", PROPERTY_HINT_NONE, ""), "set_stream_paused", "get_stream_paused");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance", PROPERTY_HINT_RANGE, "0,4096,0.01,or_greater,suffix:m"), "set_max_distance", "get_max_distance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_polyphony", PROPERTY_HINT_NONE, ""), "set_max_polyphony", "get_max_polyphony");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "panning_strength", PROPERTY_HINT_RANGE, "0,3,0.01,or_greater"), "set_panning_strength", "get_panning_strength");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "bus", PROPERTY_HINT_ENUM, ""), "set_bus", "get_bus");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "area_mask", PROPERTY_HINT_LAYERS_2D_PHYSICS), "set_area_mask", "get_area_mask");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "playback_type", PROPERTY_HINT_ENUM, "Default,Stream,Sample"), "set_playback_type", "get_playback_type");
	ADD_GROUP("Emission Angle", "emission_angle");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emission_angle_enabled"), "set_emission_angle_enabled", "is_emission_angle_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_angle_degrees", PROPERTY_HINT_RANGE, "0.1,90,0.1,degrees"), "set_emission_angle", "get_emission_angle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_angle_filter_attenuation_db", PROPERTY_HINT_RANGE, "-80,0,0.1,suffix:dB"), "set_emission_angle_filter_attenuation_db", "get_emission_angle_filter_attenuation_db");
	ADD_GROUP("Attenuation Filter", "attenuation_filter_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attenuation_filter_cutoff_hz", PROPERTY_HINT_RANGE, "1,20500,1,suffix:Hz"), "set_attenuation_filter_cutoff_hz", "get_attenuation_filter_cutoff_hz");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attenuation_filter_db", PROPERTY_HINT_RANGE, "-80,0,0.1,suffix:dB"), "set_attenuation_filter_db", "get_attenuation_filter_db");
	ADD_GROUP("Doppler", "doppler_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "doppler_tracking", PROPERTY_HINT_ENUM, "Disabled,Idle,Physics"), "set_doppler_tracking", "get_doppler_tracking");

	BIND_ENUM_CONSTANT(ATTENUATION_INVERSE_DISTANCE);
	BIND_ENUM_CONSTANT(ATTENUATION_INVERSE_SQUARE_DISTANCE);
	BIND_ENUM_CONSTANT(ATTENUATION_LOGARITHMIC);
	BIND_ENUM_CONSTANT(ATTENUATION_DISABLED);

	BIND_ENUM_CONSTANT(DOPPLER_TRACKING_DISABLED);
	BIND_ENUM_CONSTANT(DOPPLER_TRACKING_IDLE_STEP);
	BIND_ENUM_CONSTANT(DOPPLER_TRACKING_PHYSICS_STEP);

	ADD_SIGNAL(MethodInfo("finished"));
}

AudioStreamPlayer3D::AudioStreamPlayer3D() {
	internal = memnew(AudioStreamPlayerInternal(this, callable_mp(this, &AudioStreamPlayer3D::play), callable_mp(this, &AudioStreamPlayer3D::stop), true));
	velocity_tracker.instantiate();
	set_disable_scale(true);
	cached_global_panning_strength = GLOBAL_GET("audio/general/3d_panning_strength");
}

AudioStreamPlayer3D::~AudioStreamPlayer3D() {
	memdelete(internal);
}
