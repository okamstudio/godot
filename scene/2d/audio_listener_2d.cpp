/**************************************************************************/
/*  audio_listener_2d.cpp                                                 */
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

#include "audio_listener_2d.h"

#include "scene/main/viewport.h"

bool AudioListener2D::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "current") {
		if (p_value.operator bool()) {
			make_current();
		} else {
			clear_current();
		}
	} else {
		return false;
	}
	return true;
}

bool AudioListener2D::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "current") {
		if (is_part_of_edited_scene()) {
			r_ret = current;
		} else {
			r_ret = is_current();
		}
	} else {
		return false;
	}
	return true;
}

void AudioListener2D::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::BOOL, PNAME("current")));
}

void AudioListener2D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!is_part_of_edited_scene() && current) {
				make_current();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (!is_part_of_edited_scene()) {
				if (is_current()) {
					clear_current();
					current = true; // Keep it true.
				} else {
					current = false;
				}
			}
		} break;
	}
}

void AudioListener2D::make_current() {
	current = true;
	if (!is_inside_tree()) {
		return;
	}
	get_viewport()->_audio_listener_2d_set(this);
}

void AudioListener2D::clear_current() {
	current = false;
	if (!is_inside_tree()) {
		return;
	}
	get_viewport()->_audio_listener_2d_remove(this);
}

bool AudioListener2D::is_current() const {
	if (is_inside_tree() && !is_part_of_edited_scene()) {
		return get_viewport()->get_audio_listener_2d() == this;
	} else {
		return current;
	}
}

void AudioListener2D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("make_current"), &AudioListener2D::make_current);
	ClassDB::bind_method(D_METHOD("clear_current"), &AudioListener2D::clear_current);
	ClassDB::bind_method(D_METHOD("is_current"), &AudioListener2D::is_current);
}

AudioListener2D::AudioListener2D() {
	set_hide_clip_children(true);
}
