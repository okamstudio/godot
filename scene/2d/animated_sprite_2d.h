/**************************************************************************/
/*  animated_sprite_2d.h                                                  */
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

#ifndef ANIMATED_SPRITE_2D_H
#define ANIMATED_SPRITE_2D_H

#include "scene/2d/node_2d.h"
#include "scene/resources/sprite_frames.h"

class AnimatedSprite2D : public Node2D {
	GDCLASS(AnimatedSprite2D, Node2D);

	Ref<SpriteFrames> frames;
	String autoplay;

	bool playing = false;
	StringName animation = SceneStringName(default_);
	int frame = 0;
	float speed_scale = 1.0;
	float custom_speed_scale = 1.0;

	bool centered = true;
	Point2 offset;

	real_t frame_speed_scale = 1.0;
	real_t frame_progress = 0.0;

	bool hflip = false;
	bool vflip = false;

	void _res_changed();

	double _get_frame_duration();
	void _calc_frame_speed_scale();
	void _stop_internal(bool p_reset);

	Rect2 _get_rect() const;

protected:
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
#endif // DISABLE_DEPRECATED
	static void _bind_methods();
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

public:
#ifdef TOOLS_ENABLED
	virtual Dictionary _edit_get_state() const override;
	virtual void _edit_set_state(const Dictionary &p_state) override;

	virtual void _edit_set_pivot(const Point2 &p_pivot) override;
	virtual Point2 _edit_get_pivot() const override;
	virtual bool _edit_use_pivot() const override;
#endif // TOOLS_ENABLED

#ifdef DEBUG_ENABLED
	virtual Rect2 _edit_get_rect() const override;
	virtual bool _edit_use_rect() const override;
#endif // DEBUG_ENABLED

	virtual Rect2 get_anchorable_rect() const override;

	void set_sprite_frames(const Ref<SpriteFrames> &p_frames);
	Ref<SpriteFrames> get_sprite_frames() const;

	void play(const StringName &p_name = StringName(), float p_custom_scale = 1.0, bool p_from_end = false);
	void play_backwards(const StringName &p_name = StringName());
	void pause();
	void stop();

	bool is_playing() const;

	void set_animation(const StringName &p_name);
	StringName get_animation() const;

	void set_autoplay(const String &p_name);
	String get_autoplay() const;

	void set_frame(int p_frame);
	int get_frame() const;

	void set_frame_progress(real_t p_progress);
	real_t get_frame_progress() const;

	void set_frame_and_progress(int p_frame, real_t p_progress);

	void set_speed_scale(float p_speed_scale);
	float get_speed_scale() const;
	float get_playing_speed() const;

	void set_centered(bool p_center);
	bool is_centered() const;

	void set_offset(const Point2 &p_offset);
	Point2 get_offset() const;

	void set_flip_h(bool p_flip);
	bool is_flipped_h() const;

	void set_flip_v(bool p_flip);
	bool is_flipped_v() const;

	PackedStringArray get_configuration_warnings() const override;

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const override;
#endif // TOOLS_ENABLED

	AnimatedSprite2D();
};

#endif // ANIMATED_SPRITE_2D_H
