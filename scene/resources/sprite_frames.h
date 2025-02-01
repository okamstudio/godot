/**************************************************************************/
/*  sprite_frames.h                                                       */
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

#include "scene/resources/texture.h"

static const float SPRITE_FRAME_MINIMUM_DURATION = 0.01;

class SpriteFrames : public Resource {
	GDCLASS(SpriteFrames, Resource);

	struct Frame {
		Ref<Texture2D> texture;
		float duration = 1.0;
	};

	struct Anim {
		double speed = 5.0;
		bool loop = true;
		Vector<Frame> frames;
	};

	HashMap<StringName, Anim> animations;

	Array _get_animations() const;
	void _set_animations(const Array &p_animations);

protected:
	static void _bind_methods();

public:
	void add_animation(const StringName &p_anim);
	bool has_animation(const StringName &p_anim) const;
	void duplicate_animation(const StringName &p_from, const StringName &p_to);
	void remove_animation(const StringName &p_anim);
	void rename_animation(const StringName &p_prev, const StringName &p_next);

	void get_animation_list(List<StringName> *r_animations) const;
	Vector<String> get_animation_names() const;

	void set_animation_speed(const StringName &p_anim, double p_fps);
	double get_animation_speed(const StringName &p_anim) const;

	void set_animation_loop(const StringName &p_anim, bool p_loop);
	bool get_animation_loop(const StringName &p_anim) const;

	void add_frame(const StringName &p_anim, const Ref<Texture2D> &p_texture, float p_duration = 1.0, int p_at_pos = -1);
	void set_frame(const StringName &p_anim, int p_idx, const Ref<Texture2D> &p_texture, float p_duration = 1.0);
	void remove_frame(const StringName &p_anim, int p_idx);

	int get_frame_count(const StringName &p_anim) const;

	_FORCE_INLINE_ Ref<Texture2D> get_frame_texture(const StringName &p_anim, int p_idx) const {
		HashMap<StringName, Anim>::ConstIterator E = animations.find(p_anim);
		ERR_FAIL_COND_V_MSG(!E, Ref<Texture2D>(), "Animation '" + String(p_anim) + "' doesn't exist.");
		ERR_FAIL_COND_V(p_idx < 0, Ref<Texture2D>());
		if (p_idx >= E->value.frames.size()) {
			return Ref<Texture2D>();
		}

		return E->value.frames[p_idx].texture;
	}

	_FORCE_INLINE_ float get_frame_duration(const StringName &p_anim, int p_idx) const {
		HashMap<StringName, Anim>::ConstIterator E = animations.find(p_anim);
		ERR_FAIL_COND_V_MSG(!E, 1.0, "Animation '" + String(p_anim) + "' doesn't exist.");
		ERR_FAIL_COND_V(p_idx < 0, 1.0);
		if (p_idx >= E->value.frames.size()) {
			return 1.0;
		}

		return E->value.frames[p_idx].duration;
	}

	void clear(const StringName &p_anim);
	void clear_all();

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif

	SpriteFrames();
};
