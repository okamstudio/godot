/**************************************************************************/
/*  texture_button.h                                                      */
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

#include "scene/gui/base_button.h"
#include "scene/resources/bit_map.h"
class TextureButton : public BaseButton {
	GDCLASS(TextureButton, BaseButton);

public:
	enum StretchMode {
		STRETCH_SCALE,
		STRETCH_TILE,
		STRETCH_KEEP,
		STRETCH_KEEP_CENTERED,
		STRETCH_KEEP_ASPECT,
		STRETCH_KEEP_ASPECT_CENTERED,
		STRETCH_KEEP_ASPECT_COVERED,
	};

private:
	Ref<Texture2D> normal;
	Ref<Texture2D> pressed;
	Ref<Texture2D> hover;
	Ref<Texture2D> disabled;
	Ref<Texture2D> focused;
	Ref<BitMap> click_mask;
	bool ignore_texture_size = false;
	StretchMode stretch_mode = STRETCH_KEEP;

	Rect2 _texture_region;
	Rect2 _position_rect;
	bool _tile = false;

	bool hflip = false;
	bool vflip = false;

	void _set_texture(Ref<Texture2D> *p_destination, const Ref<Texture2D> &p_texture);
	void _texture_changed();

protected:
	virtual Size2 get_minimum_size() const override;
	virtual bool has_point(const Point2 &p_point) const override;
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_texture_normal(const Ref<Texture2D> &p_normal);
	void set_texture_pressed(const Ref<Texture2D> &p_pressed);
	void set_texture_hover(const Ref<Texture2D> &p_hover);
	void set_texture_disabled(const Ref<Texture2D> &p_disabled);
	void set_texture_focused(const Ref<Texture2D> &p_focused);
	void set_click_mask(const Ref<BitMap> &p_click_mask);

	Ref<Texture2D> get_texture_normal() const;
	Ref<Texture2D> get_texture_pressed() const;
	Ref<Texture2D> get_texture_hover() const;
	Ref<Texture2D> get_texture_disabled() const;
	Ref<Texture2D> get_texture_focused() const;
	Ref<BitMap> get_click_mask() const;

	bool get_ignore_texture_size() const;
	void set_ignore_texture_size(bool p_ignore);

	void set_stretch_mode(StretchMode p_stretch_mode);
	StretchMode get_stretch_mode() const;

	void set_flip_h(bool p_flip);
	bool is_flipped_h() const;

	void set_flip_v(bool p_flip);
	bool is_flipped_v() const;

	TextureButton();
};

VARIANT_ENUM_CAST(TextureButton::StretchMode);
