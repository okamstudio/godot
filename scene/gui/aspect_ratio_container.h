/**************************************************************************/
/*  aspect_ratio_container.h                                              */
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

#include "scene/gui/container.h"

class AspectRatioContainer : public Container {
	GDCLASS(AspectRatioContainer, Container);

protected:
	void _notification(int p_what);
	static void _bind_methods();
	virtual Size2 get_minimum_size() const override;

public:
	enum StretchMode {
		STRETCH_WIDTH_CONTROLS_HEIGHT,
		STRETCH_HEIGHT_CONTROLS_WIDTH,
		STRETCH_FIT,
		STRETCH_COVER,
	};
	enum AlignmentMode {
		ALIGNMENT_BEGIN,
		ALIGNMENT_CENTER,
		ALIGNMENT_END,
	};

private:
	float ratio = 1.0;
	StretchMode stretch_mode = STRETCH_FIT;
	AlignmentMode alignment_horizontal = ALIGNMENT_CENTER;
	AlignmentMode alignment_vertical = ALIGNMENT_CENTER;

public:
	void set_ratio(float p_ratio);
	float get_ratio() const { return ratio; }

	void set_stretch_mode(StretchMode p_mode);
	StretchMode get_stretch_mode() const { return stretch_mode; }

	void set_alignment_horizontal(AlignmentMode p_alignment_horizontal);
	AlignmentMode get_alignment_horizontal() const { return alignment_horizontal; }

	void set_alignment_vertical(AlignmentMode p_alignment_vertical);
	AlignmentMode get_alignment_vertical() const { return alignment_vertical; }

	virtual Vector<int> get_allowed_size_flags_horizontal() const override;
	virtual Vector<int> get_allowed_size_flags_vertical() const override;
};

VARIANT_ENUM_CAST(AspectRatioContainer::StretchMode);
VARIANT_ENUM_CAST(AspectRatioContainer::AlignmentMode);
