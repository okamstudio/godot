/**************************************************************************/
/*  animated_texture.h                                                    */
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

class AnimatedTexture : public Texture2D {
	GDCLASS(AnimatedTexture, Texture2D);

	// Use readers writers lock for this, since its far more times read than written to.
	RWLock rw_lock;

public:
	enum {
		MAX_FRAMES = 256
	};

private:
	RID proxy_ph;
	RID proxy;

	struct Frame {
		Ref<Texture2D> texture;
		float duration = 1.0;
	};

	Frame frames[MAX_FRAMES];
	int frame_count = 1.0;
	int current_frame = 0;
	bool pause = false;
	bool one_shot = false;
	float speed_scale = 1.0;

	float time = 0.0;

	uint64_t prev_ticks = 0;

	void _update_proxy();
	void _finish_non_thread_safe_setup();

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_frames(int p_frames);
	int get_frames() const;

	void set_current_frame(int p_frame);
	int get_current_frame() const;

	void set_pause(bool p_pause);
	bool get_pause() const;

	void set_one_shot(bool p_one_shot);
	bool get_one_shot() const;

	void set_frame_texture(int p_frame, const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_frame_texture(int p_frame) const;

	void set_frame_duration(int p_frame, float p_duration);
	float get_frame_duration(int p_frame) const;

	void set_speed_scale(float p_scale);
	float get_speed_scale() const;

	virtual int get_width() const override;
	virtual int get_height() const override;
	virtual RID get_rid() const override;

	virtual bool has_alpha() const override;

	virtual Ref<Image> get_image() const override;

	bool is_pixel_opaque(int p_x, int p_y) const override;

	AnimatedTexture();
	~AnimatedTexture();
};
