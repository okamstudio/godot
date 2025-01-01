/**************************************************************************/
/*  animated_texture.cpp                                                  */
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

#include "animated_texture.h"

void AnimatedTexture::_update_proxy() {
	RWLockRead r(rw_lock);

	float delta;
	if (prev_ticks == 0) {
		delta = 0;
		prev_ticks = OS::get_singleton()->get_ticks_usec();
	} else {
		uint64_t ticks = OS::get_singleton()->get_ticks_usec();
		delta = float(double(ticks - prev_ticks) / 1000000.0);
		prev_ticks = ticks;
	}

	time += delta;

	float speed = speed_scale == 0 ? 0 : abs(1.0 / speed_scale);

	int iter_max = frame_count;
	while (iter_max && !pause) {
		float frame_limit = frames[current_frame].duration * speed;

		if (time > frame_limit) {
			if (speed_scale > 0.0) {
				current_frame++;
			} else {
				current_frame--;
			}
			if (current_frame >= frame_count) {
				if (one_shot) {
					current_frame = frame_count - 1;
				} else {
					current_frame = 0;
				}
			} else if (current_frame < 0) {
				if (one_shot) {
					current_frame = 0;
				} else {
					current_frame = frame_count - 1;
				}
			}
			time -= frame_limit;

		} else {
			break;
		}
		iter_max--;
	}

	if (frames[current_frame].texture.is_valid()) {
		RenderingServer::get_singleton()->texture_proxy_update(proxy, frames[current_frame].texture->get_rid());
	}
}

void AnimatedTexture::set_frames(int p_frames) {
	ERR_FAIL_COND(p_frames < 1 || p_frames > MAX_FRAMES);

	RWLockWrite r(rw_lock);

	frame_count = p_frames;
}

int AnimatedTexture::get_frames() const {
	return frame_count;
}

void AnimatedTexture::set_current_frame(int p_frame) {
	ERR_FAIL_COND(p_frame < 0 || p_frame >= frame_count);

	RWLockWrite r(rw_lock);

	current_frame = p_frame;
	time = 0;
}

int AnimatedTexture::get_current_frame() const {
	return current_frame;
}

void AnimatedTexture::set_pause(bool p_pause) {
	RWLockWrite r(rw_lock);
	pause = p_pause;
}

bool AnimatedTexture::get_pause() const {
	return pause;
}

void AnimatedTexture::set_one_shot(bool p_one_shot) {
	RWLockWrite r(rw_lock);
	one_shot = p_one_shot;
}

bool AnimatedTexture::get_one_shot() const {
	return one_shot;
}

void AnimatedTexture::set_frame_texture(int p_frame, const Ref<Texture2D> &p_texture) {
	ERR_FAIL_COND(p_texture == this);
	ERR_FAIL_INDEX(p_frame, MAX_FRAMES);

	RWLockWrite w(rw_lock);

	frames[p_frame].texture = p_texture;
}

Ref<Texture2D> AnimatedTexture::get_frame_texture(int p_frame) const {
	ERR_FAIL_INDEX_V(p_frame, MAX_FRAMES, Ref<Texture2D>());

	RWLockRead r(rw_lock);

	return frames[p_frame].texture;
}

void AnimatedTexture::set_frame_duration(int p_frame, float p_duration) {
	ERR_FAIL_INDEX(p_frame, MAX_FRAMES);

	RWLockWrite r(rw_lock);

	frames[p_frame].duration = p_duration;
}

float AnimatedTexture::get_frame_duration(int p_frame) const {
	ERR_FAIL_INDEX_V(p_frame, MAX_FRAMES, 0);

	RWLockRead r(rw_lock);

	return frames[p_frame].duration;
}

void AnimatedTexture::set_speed_scale(float p_scale) {
	ERR_FAIL_COND(p_scale < -1000 || p_scale >= 1000);

	RWLockWrite r(rw_lock);

	speed_scale = p_scale;
}

float AnimatedTexture::get_speed_scale() const {
	return speed_scale;
}

int AnimatedTexture::get_width() const {
	RWLockRead r(rw_lock);

	if (frames[current_frame].texture.is_null()) {
		return 1;
	}

	return frames[current_frame].texture->get_width();
}

int AnimatedTexture::get_height() const {
	RWLockRead r(rw_lock);

	if (frames[current_frame].texture.is_null()) {
		return 1;
	}

	return frames[current_frame].texture->get_height();
}

RID AnimatedTexture::get_rid() const {
	return proxy;
}

bool AnimatedTexture::has_alpha() const {
	RWLockRead r(rw_lock);

	if (frames[current_frame].texture.is_null()) {
		return false;
	}

	return frames[current_frame].texture->has_alpha();
}

Ref<Image> AnimatedTexture::get_image() const {
	RWLockRead r(rw_lock);

	if (frames[current_frame].texture.is_null()) {
		return Ref<Image>();
	}

	return frames[current_frame].texture->get_image();
}

bool AnimatedTexture::is_pixel_opaque(int p_x, int p_y) const {
	RWLockRead r(rw_lock);

	if (frames[current_frame].texture.is_valid()) {
		return frames[current_frame].texture->is_pixel_opaque(p_x, p_y);
	}
	return true;
}

void AnimatedTexture::_validate_property(PropertyInfo &p_property) const {
	String prop = p_property.name;
	if (prop.begins_with("frame_")) {
		int frame = prop.get_slicec('/', 0).get_slicec('_', 1).to_int();
		if (frame >= frame_count) {
			p_property.usage = PROPERTY_USAGE_NONE;
		}
	}
}

void AnimatedTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_frames", "frames"), &AnimatedTexture::set_frames);
	ClassDB::bind_method(D_METHOD("get_frames"), &AnimatedTexture::get_frames);

	ClassDB::bind_method(D_METHOD("set_current_frame", "frame"), &AnimatedTexture::set_current_frame);
	ClassDB::bind_method(D_METHOD("get_current_frame"), &AnimatedTexture::get_current_frame);

	ClassDB::bind_method(D_METHOD("set_pause", "pause"), &AnimatedTexture::set_pause);
	ClassDB::bind_method(D_METHOD("get_pause"), &AnimatedTexture::get_pause);

	ClassDB::bind_method(D_METHOD("set_one_shot", "one_shot"), &AnimatedTexture::set_one_shot);
	ClassDB::bind_method(D_METHOD("get_one_shot"), &AnimatedTexture::get_one_shot);

	ClassDB::bind_method(D_METHOD("set_speed_scale", "scale"), &AnimatedTexture::set_speed_scale);
	ClassDB::bind_method(D_METHOD("get_speed_scale"), &AnimatedTexture::get_speed_scale);

	ClassDB::bind_method(D_METHOD("set_frame_texture", "frame", "texture"), &AnimatedTexture::set_frame_texture);
	ClassDB::bind_method(D_METHOD("get_frame_texture", "frame"), &AnimatedTexture::get_frame_texture);

	ClassDB::bind_method(D_METHOD("set_frame_duration", "frame", "duration"), &AnimatedTexture::set_frame_duration);
	ClassDB::bind_method(D_METHOD("get_frame_duration", "frame"), &AnimatedTexture::get_frame_duration);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "frames", PROPERTY_HINT_RANGE, "1," + itos(MAX_FRAMES), PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_frames", "get_frames");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "current_frame", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_current_frame", "get_current_frame");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pause"), "set_pause", "get_pause");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "one_shot"), "set_one_shot", "get_one_shot");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale", PROPERTY_HINT_RANGE, "-60,60,0.1,or_less,or_greater"), "set_speed_scale", "get_speed_scale");

	for (int i = 0; i < MAX_FRAMES; i++) {
		ADD_PROPERTYI(PropertyInfo(Variant::OBJECT, "frame_" + itos(i) + "/texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_frame_texture", "get_frame_texture", i);
		ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "frame_" + itos(i) + "/duration", PROPERTY_HINT_RANGE, "0.0,16.0,0.01,or_greater,suffix:s", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_INTERNAL), "set_frame_duration", "get_frame_duration", i);
	}

	BIND_CONSTANT(MAX_FRAMES);
}

void AnimatedTexture::_finish_non_thread_safe_setup() {
	RenderingServer::get_singleton()->connect("frame_pre_draw", callable_mp(this, &AnimatedTexture::_update_proxy));
}

AnimatedTexture::AnimatedTexture() {
	//proxy = RS::get_singleton()->texture_create();
	proxy_ph = RS::get_singleton()->texture_2d_placeholder_create();
	proxy = RS::get_singleton()->texture_proxy_create(proxy_ph);

	RenderingServer::get_singleton()->texture_set_force_redraw_if_visible(proxy, true);

	MessageQueue::get_main_singleton()->push_callable(callable_mp(this, &AnimatedTexture::_finish_non_thread_safe_setup));
}

AnimatedTexture::~AnimatedTexture() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(proxy);
	RS::get_singleton()->free(proxy_ph);
}
