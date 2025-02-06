/**************************************************************************/
/*  layout_container.h                                                    */
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

#ifndef LAYOUT_CONTAINER_H
#define LAYOUT_CONTAINER_H

#include "core/math/math_defs.h"
#include "scene/gui/container.h"

class LayoutContainer : public Container {
	GDCLASS(LayoutContainer, Container);

public:
	enum LayoutContainerPreset {
		PRESET_TOP_LEFT = 0,
		PRESET_TOP_RIGHT,
		PRESET_BOTTOM_LEFT,
		PRESET_BOTTOM_RIGHT,
		PRESET_CENTER_LEFT,
		PRESET_CENTER_TOP,
		PRESET_CENTER_RIGHT,
		PRESET_CENTER_BOTTOM,
		PRESET_CENTER,
		PRESET_LEFT_WIDE,
		PRESET_TOP_WIDE,
		PRESET_RIGHT_WIDE,
		PRESET_BOTTOM_WIDE,
		PRESET_VCENTER_WIDE,
		PRESET_HCENTER_WIDE,
		PRESET_FULL_RECT,
		PRESET_CUSTOM,
		PRESET_MAX,
	};

	inline static const real_t relative_margins_presets[PRESET_CUSTOM][4] = {
		// LEFT, TOP, RIGHT, BOTTOM.
		{ 0.0, 0.0, 1.0, 1.0 }, // PRESET_TOP_LEFT
		{ 1.0, 0.0, 0.0, 1.0 }, // PRESET_TOP_RIGHT
		{ 0.0, 1.0, 1.0, 0.0 }, // PRESET_BOTTOM_LEFT
		{ 1.0, 1.0, 0.0, 0.0 }, // PRESET_BOTTOM_RIGHT
		{ 0.0, 0.5, 1.0, 0.5 }, // PRESET_CENTER_LEFT
		{ 0.5, 0.0, 0.5, 1.0 }, // PRESET_CENTER_TOP
		{ 1.0, 0.5, 0.0, 0.5 }, // PRESET_CENTER_RIGHT
		{ 0.5, 1.0, 0.5, 0.0 }, // PRESET_CENTER_BOTTOM
		{ 0.5, 0.5, 0.5, 0.5 }, // PRESET_CENTER
		{ 0.0, 0.0, 1.0, 0.0 }, // PRESET_LEFT_WIDE
		{ 0.0, 0.0, 0.0, 1.0 }, // PRESET_TOP_WIDE
		{ 1.0, 0.0, 0.0, 0.0 }, // PRESET_RIGHT_WIDE
		{ 0.0, 1.0, 0.0, 0.0 }, // PRESET_BOTTOM_WIDE
		{ 0.5, 0.0, 0.5, 0.0 }, // PRESET_VCENTER_WIDE
		{ 0.0, 0.5, 0.0, 0.5 }, // PRESET_HCENTER_WIDE
		{ 0.0, 0.0, 0.0, 0.0 }, // PRESET_FULL_RECT
	};

private:
	LayoutContainerPreset preset = PRESET_TOP_LEFT;
	real_t custom_relative_margins[4] = { 0, 0, 0, 0 };
	real_t margins[4] = { 0.0, 0.0, 0.0, 0.0 };

	struct ThemeCache {
		int margin_left = 0;
		int margin_top = 0;
		int margin_right = 0;
		int margin_bottom = 0;
	} theme_cache;

	void _resort();

	void _set_custom_relative_margin(Side p_side, real_t p_relative_margin);

protected:
	void _notification(int p_what);
	static void _bind_methods();
	virtual Size2 get_minimum_size() const override;
	void _validate_property(PropertyInfo &p_property) const;

public:
	virtual Vector<int> get_allowed_size_flags_horizontal() const override;
	virtual Vector<int> get_allowed_size_flags_vertical() const override;

	void set_margin_layout_preset(LayoutContainerPreset p_preset);
	LayoutContainerPreset get_margin_layout_preset() const;

	void set_custom_relative_margin(Side p_side, real_t p_relative_margin, bool p_push_opposite_margin = true);
	real_t get_custom_relative_margin(Side p_side) const;

	void set_margin(Side p_side, real_t p_value);
	real_t get_margin(Side p_side) const;

	void set_custom_relative_margin_and_offset(Side p_side, real_t p_relative_margin, real_t p_pos, bool p_push_opposite_margin = true);
};

VARIANT_ENUM_CAST(LayoutContainer::LayoutContainerPreset);

#endif // LAYOUT_CONTAINER_H
