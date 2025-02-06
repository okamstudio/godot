/**************************************************************************/
/*  nine_patch_rect.h                                                     */
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

#include "scene/gui/control.h"

class NinePatchRect : public Control {
	GDCLASS(NinePatchRect, Control);

public:
	enum AxisStretchMode {
		AXIS_STRETCH_MODE_STRETCH,
		AXIS_STRETCH_MODE_TILE,
		AXIS_STRETCH_MODE_TILE_FIT,
	};

	bool draw_center = true;
	int margin[4] = {};
	Rect2 region_rect;
	Ref<Texture2D> texture;

	AxisStretchMode axis_h = AXIS_STRETCH_MODE_STRETCH;
	AxisStretchMode axis_v = AXIS_STRETCH_MODE_STRETCH;

	void _texture_changed();

protected:
	void _notification(int p_what);
	virtual Size2 get_minimum_size() const override;
	static void _bind_methods();

public:
	void set_texture(const Ref<Texture2D> &p_tex);
	Ref<Texture2D> get_texture() const;

	void set_patch_margin(Side p_side, int p_size);
	int get_patch_margin(Side p_side) const;

	void set_region_rect(const Rect2 &p_region_rect);
	Rect2 get_region_rect() const;

	void set_draw_center(bool p_enabled);
	bool is_draw_center_enabled() const;

	void set_h_axis_stretch_mode(AxisStretchMode p_mode);
	AxisStretchMode get_h_axis_stretch_mode() const;

	void set_v_axis_stretch_mode(AxisStretchMode p_mode);
	AxisStretchMode get_v_axis_stretch_mode() const;

	NinePatchRect();
	~NinePatchRect();
};

VARIANT_ENUM_CAST(NinePatchRect::AxisStretchMode)
