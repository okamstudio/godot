/**************************************************************************/
/*  animated_sprite_2d.cpp                                                */
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

#include "animated_sprite_2d.h"

#include "scene/main/viewport.h"

#ifdef TOOLS_ENABLED
Dictionary AnimatedSprite2D::_edit_get_state() const {
	Dictionary state = Node2D::_edit_get_state();
	state["offset"] = offset;
	return state;
}

void AnimatedSprite2D::_edit_set_state(const Dictionary &p_state) {
	Node2D::_edit_set_state(p_state);
	set_offset(p_state["offset"]);
}

void AnimatedSprite2D::_edit_set_pivot(const Point2 &p_pivot) {
	set_offset(get_offset() - p_pivot);
	set_position(get_transform().xform(p_pivot));
}

Point2 AnimatedSprite2D::_edit_get_pivot() const {
	return Vector2();
}

bool AnimatedSprite2D::_edit_use_pivot() const {
	return true;
}
#endif // TOOLS_ENABLED

#ifdef DEBUG_ENABLED
Rect2 AnimatedSprite2D::_edit_get_rect() const {
	return _get_rect();
}

bool AnimatedSprite2D::_edit_use_rect() const {
	if (frames.is_null() || !frames->has_animation(animation)) {
		return false;
	}
	if (frame < 0 || frame >= frames->get_frame_count(animation)) {
		return false;
	}

	Ref<Texture2D> t;
	if (animation) {
		t = frames->get_frame_texture(animation, frame);
	}
	return t.is_valid();
}
#endif // DEBUG_ENABLED

Rect2 AnimatedSprite2D::get_anchorable_rect() const {
	return _get_rect();
}

Rect2 AnimatedSprite2D::_get_rect() const {
	if (frames.is_null() || !frames->has_animation(animation)) {
		return Rect2();
	}
	if (frame < 0 || frame >= frames->get_frame_count(animation)) {
		return Rect2();
	}

	Ref<Texture2D> t;
	if (animation) {
		t = frames->get_frame_texture(animation, frame);
	}
	if (t.is_null()) {
		return Rect2();
	}
	Size2 s = t->get_size();

	Point2 ofs = offset;
	if (centered) {
		ofs -= s / 2;
	}

	if (s == Size2(0, 0)) {
		s = Size2(1, 1);
	}

	return Rect2(ofs, s);
}

void AnimatedSprite2D::_validate_property(PropertyInfo &p_property) const {
	if (frames.is_null()) {
		return;
	}

	if (p_property.name == "animation") {
		List<StringName> names;
		frames->get_animation_list(&names);
		names.sort_custom<StringName::AlphCompare>();

		bool current_found = false;
		bool is_first_element = true;

		for (const StringName &E : names) {
			if (!is_first_element) {
				p_property.hint_string += ",";
			} else {
				is_first_element = false;
			}

			p_property.hint_string += String(E);
			if (animation == E) {
				current_found = true;
			}
		}

		if (!current_found) {
			if (p_property.hint_string.is_empty()) {
				p_property.hint_string = String(animation);
			} else {
				p_property.hint_string = String(animation) + "," + p_property.hint_string;
			}
		}
		return;
	}

	if (p_property.name == "frame") {
		if (playing) {
			p_property.usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY;
			return;
		}

		p_property.hint = PROPERTY_HINT_RANGE;
		if (frames->has_animation(animation) && frames->get_frame_count(animation) > 0) {
			p_property.hint_string = "0," + itos(frames->get_frame_count(animation) - 1) + ",1";
		} else {
			// Avoid an error, `hint_string` is required for `PROPERTY_HINT_RANGE`.
			p_property.hint_string = "0,0,1";
		}
		p_property.usage |= PROPERTY_USAGE_KEYING_INCREMENTS;
	}
}

void AnimatedSprite2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (!Engine::get_singleton()->is_editor_hint() && frames.is_valid() && frames->has_animation(autoplay)) {
				play(autoplay);
			}
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (frames.is_null() || !frames->has_animation(animation)) {
				return;
			}

			double remaining = get_process_delta_time();
			int i = 0;
			while (remaining) {
				// Animation speed may be changed by animation_finished or frame_changed signals.
				double speed = frames->get_animation_speed(animation) * speed_scale * custom_speed_scale * frame_speed_scale;
				double abs_speed = Math::abs(speed);

				if (speed == 0) {
					return; // Do nothing.
				}

				// Frame count may be changed by animation_finished or frame_changed signals.
				int fc = frames->get_frame_count(animation);

				int last_frame = fc - 1;
				if (!signbit(speed)) {
					// Forwards.
					if (frame_progress >= 1.0) {
						if (frame >= last_frame) {
							if (frames->get_animation_loop(animation)) {
								frame = 0;
								emit_signal("animation_looped");
							} else {
								frame = last_frame;
								pause();
								emit_signal(SceneStringName(animation_finished));
								return;
							}
						} else {
							frame++;
						}
						_calc_frame_speed_scale();
						frame_progress = 0.0;
						queue_redraw();
						emit_signal(SceneStringName(frame_changed));
					}
					double to_process = MIN((1.0 - frame_progress) / abs_speed, remaining);
					frame_progress += to_process * abs_speed;
					remaining -= to_process;
				} else {
					// Backwards.
					if (frame_progress <= 0) {
						if (frame <= 0) {
							if (frames->get_animation_loop(animation)) {
								frame = last_frame;
								emit_signal("animation_looped");
							} else {
								frame = 0;
								pause();
								emit_signal(SceneStringName(animation_finished));
								return;
							}
						} else {
							frame--;
						}
						_calc_frame_speed_scale();
						frame_progress = 1.0;
						queue_redraw();
						emit_signal(SceneStringName(frame_changed));
					}
					double to_process = MIN(frame_progress / abs_speed, remaining);
					frame_progress -= to_process * abs_speed;
					remaining -= to_process;
				}

				i++;
				if (i > fc) {
					return; // Prevents freezing if to_process is each time much less than remaining.
				}
			}
		} break;

		case NOTIFICATION_DRAW: {
			if (frames.is_null() || !frames->has_animation(animation)) {
				return;
			}

			Ref<Texture2D> texture = frames->get_frame_texture(animation, frame);
			if (texture.is_null()) {
				return;
			}

			RID ci = get_canvas_item();

			Size2 s = texture->get_size();
			Point2 ofs = offset;
			if (centered) {
				ofs -= s / 2;
			}

			if (get_viewport() && get_viewport()->is_snap_2d_transforms_to_pixel_enabled()) {
				ofs = (ofs + Point2(0.5, 0.5)).floor();
			}

			Rect2 dst_rect(ofs, s);

			if (hflip) {
				dst_rect.size.x = -dst_rect.size.x;
			}
			if (vflip) {
				dst_rect.size.y = -dst_rect.size.y;
			}

			texture->draw_rect_region(ci, dst_rect, Rect2(Vector2(), texture->get_size()), Color(1, 1, 1), false);
		} break;
	}
}

void AnimatedSprite2D::set_sprite_frames(const Ref<SpriteFrames> &p_frames) {
	if (frames == p_frames) {
		return;
	}

	if (frames.is_valid()) {
		frames->disconnect(CoreStringName(changed), callable_mp(this, &AnimatedSprite2D::_res_changed));
	}
	stop();
	frames = p_frames;
	if (frames.is_valid()) {
		frames->connect(CoreStringName(changed), callable_mp(this, &AnimatedSprite2D::_res_changed));

		List<StringName> al;
		frames->get_animation_list(&al);
		if (al.size() == 0) {
			set_animation(StringName());
			autoplay = String();
		} else {
			if (!frames->has_animation(animation)) {
				set_animation(al.front()->get());
			}
			if (!frames->has_animation(autoplay)) {
				autoplay = String();
			}
		}
	}

	notify_property_list_changed();
	queue_redraw();
	update_configuration_warnings();
	emit_signal("sprite_frames_changed");
}

Ref<SpriteFrames> AnimatedSprite2D::get_sprite_frames() const {
	return frames;
}

void AnimatedSprite2D::set_frame(int p_frame) {
	set_frame_and_progress(p_frame, signbit(get_playing_speed()) ? 1.0 : 0.0);
}

int AnimatedSprite2D::get_frame() const {
	return frame;
}

void AnimatedSprite2D::set_frame_progress(real_t p_progress) {
	frame_progress = p_progress;
}

real_t AnimatedSprite2D::get_frame_progress() const {
	return frame_progress;
}

void AnimatedSprite2D::set_frame_and_progress(int p_frame, real_t p_progress) {
	if (frames.is_null()) {
		return;
	}

	bool has_animation = frames->has_animation(animation);
	int end_frame = has_animation ? MAX(0, frames->get_frame_count(animation) - 1) : 0;
	bool is_changed = frame != p_frame;

	if (p_frame < 0) {
		frame = 0;
	} else if (has_animation && p_frame > end_frame) {
		frame = end_frame;
	} else {
		frame = p_frame;
	}

	_calc_frame_speed_scale();
	frame_progress = p_progress;

	if (!is_changed) {
		return; // No change, don't redraw.
	}
	queue_redraw();
	emit_signal(SceneStringName(frame_changed));
}

void AnimatedSprite2D::set_speed_scale(float p_speed_scale) {
	speed_scale = p_speed_scale;
}

float AnimatedSprite2D::get_speed_scale() const {
	return speed_scale;
}

float AnimatedSprite2D::get_playing_speed() const {
	if (!playing) {
		return 0;
	}
	return speed_scale * custom_speed_scale;
}

void AnimatedSprite2D::set_centered(bool p_center) {
	if (centered == p_center) {
		return;
	}

	centered = p_center;
	queue_redraw();
	item_rect_changed();
}

bool AnimatedSprite2D::is_centered() const {
	return centered;
}

void AnimatedSprite2D::set_offset(const Point2 &p_offset) {
	if (offset == p_offset) {
		return;
	}

	offset = p_offset;
	queue_redraw();
	item_rect_changed();
}

Point2 AnimatedSprite2D::get_offset() const {
	return offset;
}

void AnimatedSprite2D::set_flip_h(bool p_flip) {
	if (hflip == p_flip) {
		return;
	}

	hflip = p_flip;
	queue_redraw();
}

bool AnimatedSprite2D::is_flipped_h() const {
	return hflip;
}

void AnimatedSprite2D::set_flip_v(bool p_flip) {
	if (vflip == p_flip) {
		return;
	}

	vflip = p_flip;
	queue_redraw();
}

bool AnimatedSprite2D::is_flipped_v() const {
	return vflip;
}

void AnimatedSprite2D::_res_changed() {
	set_frame_and_progress(frame, frame_progress);
	queue_redraw();
	notify_property_list_changed();
}

bool AnimatedSprite2D::is_playing() const {
	return playing;
}

void AnimatedSprite2D::set_autoplay(const String &p_name) {
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		WARN_PRINT("Setting autoplay after the node has been added to the scene has no effect.");
	}

	autoplay = p_name;
}

String AnimatedSprite2D::get_autoplay() const {
	return autoplay;
}

void AnimatedSprite2D::play(const StringName &p_name, float p_custom_scale, bool p_from_end) {
	StringName name = p_name;

	if (name == StringName()) {
		name = animation;
	}

	ERR_FAIL_COND_MSG(frames.is_null(), vformat("There is no animation with name '%s'.", name));
	ERR_FAIL_COND_MSG(!frames->get_animation_names().has(name), vformat("There is no animation with name '%s'.", name));

	if (frames->get_frame_count(name) == 0) {
		return;
	}

	playing = true;
	custom_speed_scale = p_custom_scale;

	if (name != animation) {
		animation = name;
		int end_frame = MAX(0, frames->get_frame_count(animation) - 1);

		if (p_from_end) {
			set_frame_and_progress(end_frame, 1.0);
		} else {
			set_frame_and_progress(0, 0.0);
		}
		emit_signal(SceneStringName(animation_changed));
	} else {
		int end_frame = MAX(0, frames->get_frame_count(animation) - 1);
		bool is_backward = signbit(speed_scale * custom_speed_scale);

		if (p_from_end && is_backward && frame == 0 && frame_progress <= 0.0) {
			set_frame_and_progress(end_frame, 1.0);
		} else if (!p_from_end && !is_backward && frame == end_frame && frame_progress >= 1.0) {
			set_frame_and_progress(0, 0.0);
		}
	}

	set_process_internal(true);
	notify_property_list_changed();
	queue_redraw();
}

void AnimatedSprite2D::play_backwards(const StringName &p_name) {
	play(p_name, -1, true);
}

void AnimatedSprite2D::_stop_internal(bool p_reset) {
	playing = false;
	if (p_reset) {
		custom_speed_scale = 1.0;
		set_frame_and_progress(0, 0.0);
	}
	notify_property_list_changed();
	set_process_internal(false);
}

void AnimatedSprite2D::pause() {
	_stop_internal(false);
}

void AnimatedSprite2D::stop() {
	_stop_internal(true);
}

double AnimatedSprite2D::_get_frame_duration() {
	if (frames.is_valid() && frames->has_animation(animation)) {
		return frames->get_frame_duration(animation, frame);
	}
	return 1.0;
}

void AnimatedSprite2D::_calc_frame_speed_scale() {
	frame_speed_scale = 1.0 / _get_frame_duration();
}

void AnimatedSprite2D::set_animation(const StringName &p_name) {
	if (animation == p_name) {
		return;
	}

	animation = p_name;

	emit_signal(SceneStringName(animation_changed));

	if (frames.is_null()) {
		animation = StringName();
		stop();
		ERR_FAIL_MSG(vformat("There is no animation with name '%s'.", p_name));
	}

	int frame_count = frames->get_frame_count(animation);
	if (animation == StringName() || frame_count == 0) {
		stop();
		return;
	} else if (!frames->get_animation_names().has(animation)) {
		animation = StringName();
		stop();
		ERR_FAIL_MSG(vformat("There is no animation with name '%s'.", p_name));
	}

	if (signbit(get_playing_speed())) {
		set_frame_and_progress(frame_count - 1, 1.0);
	} else {
		set_frame_and_progress(0, 0.0);
	}

	notify_property_list_changed();
	queue_redraw();
}

StringName AnimatedSprite2D::get_animation() const {
	return animation;
}

PackedStringArray AnimatedSprite2D::get_configuration_warnings() const {
	PackedStringArray warnings = Node2D::get_configuration_warnings();
	if (frames.is_null()) {
		warnings.push_back(RTR("A SpriteFrames resource must be created or set in the \"Sprite Frames\" property in order for AnimatedSprite2D to display frames."));
	}
	return warnings;
}

#ifdef TOOLS_ENABLED
void AnimatedSprite2D::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0 && frames.is_valid()) {
		if (pf == "play" || pf == "play_backwards" || pf == "set_animation" || pf == "set_autoplay") {
			List<StringName> al;
			frames->get_animation_list(&al);
			for (const StringName &name : al) {
				r_options->push_back(String(name).quote());
			}
		}
	}
	Node2D::get_argument_options(p_function, p_idx, r_options);
}
#endif // TOOLS_ENABLED

#ifndef DISABLE_DEPRECATED
bool AnimatedSprite2D::_set(const StringName &p_name, const Variant &p_value) {
	if ((p_name == SNAME("frames"))) {
		set_sprite_frames(p_value);
		return true;
	}
	return false;
}
#endif
void AnimatedSprite2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_sprite_frames", "sprite_frames"), &AnimatedSprite2D::set_sprite_frames);
	ClassDB::bind_method(D_METHOD("get_sprite_frames"), &AnimatedSprite2D::get_sprite_frames);

	ClassDB::bind_method(D_METHOD("set_animation", "name"), &AnimatedSprite2D::set_animation);
	ClassDB::bind_method(D_METHOD("get_animation"), &AnimatedSprite2D::get_animation);

	ClassDB::bind_method(D_METHOD("set_autoplay", "name"), &AnimatedSprite2D::set_autoplay);
	ClassDB::bind_method(D_METHOD("get_autoplay"), &AnimatedSprite2D::get_autoplay);

	ClassDB::bind_method(D_METHOD("is_playing"), &AnimatedSprite2D::is_playing);

	ClassDB::bind_method(D_METHOD("play", "name", "custom_speed", "from_end"), &AnimatedSprite2D::play, DEFVAL(StringName()), DEFVAL(1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("play_backwards", "name"), &AnimatedSprite2D::play_backwards, DEFVAL(StringName()));
	ClassDB::bind_method(D_METHOD("pause"), &AnimatedSprite2D::pause);
	ClassDB::bind_method(D_METHOD("stop"), &AnimatedSprite2D::stop);

	ClassDB::bind_method(D_METHOD("set_centered", "centered"), &AnimatedSprite2D::set_centered);
	ClassDB::bind_method(D_METHOD("is_centered"), &AnimatedSprite2D::is_centered);

	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &AnimatedSprite2D::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &AnimatedSprite2D::get_offset);

	ClassDB::bind_method(D_METHOD("set_flip_h", "flip_h"), &AnimatedSprite2D::set_flip_h);
	ClassDB::bind_method(D_METHOD("is_flipped_h"), &AnimatedSprite2D::is_flipped_h);

	ClassDB::bind_method(D_METHOD("set_flip_v", "flip_v"), &AnimatedSprite2D::set_flip_v);
	ClassDB::bind_method(D_METHOD("is_flipped_v"), &AnimatedSprite2D::is_flipped_v);

	ClassDB::bind_method(D_METHOD("set_frame", "frame"), &AnimatedSprite2D::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &AnimatedSprite2D::get_frame);

	ClassDB::bind_method(D_METHOD("set_frame_progress", "progress"), &AnimatedSprite2D::set_frame_progress);
	ClassDB::bind_method(D_METHOD("get_frame_progress"), &AnimatedSprite2D::get_frame_progress);

	ClassDB::bind_method(D_METHOD("set_frame_and_progress", "frame", "progress"), &AnimatedSprite2D::set_frame_and_progress);

	ClassDB::bind_method(D_METHOD("set_speed_scale", "speed_scale"), &AnimatedSprite2D::set_speed_scale);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &AnimatedSprite2D::get_speed_scale);
	ClassDB::bind_method(D_METHOD("get_playing_speed"), &AnimatedSprite2D::get_playing_speed);

	ADD_SIGNAL(MethodInfo("sprite_frames_changed"));
	ADD_SIGNAL(MethodInfo("animation_changed"));
	ADD_SIGNAL(MethodInfo("frame_changed"));
	ADD_SIGNAL(MethodInfo("animation_looped"));
	ADD_SIGNAL(MethodInfo("animation_finished"));

	ADD_GROUP("Animation", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "sprite_frames", PROPERTY_HINT_RESOURCE_TYPE, "SpriteFrames"), "set_sprite_frames", "get_sprite_frames");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "animation", PROPERTY_HINT_ENUM, ""), "set_animation", "get_animation");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "autoplay", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_autoplay", "get_autoplay");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "frame"), "set_frame", "get_frame");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "frame_progress", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_frame_progress", "get_frame_progress");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale"), "set_speed_scale", "get_speed_scale");
	ADD_GROUP("Offset", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "centered"), "set_centered", "is_centered");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset", PROPERTY_HINT_NONE, "suffix:px"), "set_offset", "get_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_h"), "set_flip_h", "is_flipped_h");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_v"), "set_flip_v", "is_flipped_v");
}

AnimatedSprite2D::AnimatedSprite2D() {
}
