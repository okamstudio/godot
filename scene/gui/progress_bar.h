/**************************************************************************/
/*  progress_bar.h                                                        */
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

#include "scene/gui/range.h"

class ProgressBar : public Range {
	GDCLASS(ProgressBar, Range);

	bool show_percentage = true;
	bool indeterminate = false;
	bool editor_preview_indeterminate = false;

	struct ThemeCache {
		Ref<StyleBox> background_style;
		Ref<StyleBox> fill_style;

		Ref<Font> font;
		int font_size = 0;
		Color font_color;
		int font_outline_size = 0;
		Color font_outline_color;
	} theme_cache;

protected:
	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;

	static void _bind_methods();

	double indeterminate_min_speed = 200.0;

public:
	enum FillMode {
		FILL_BEGIN_TO_END,
		FILL_END_TO_BEGIN,
		FILL_TOP_TO_BOTTOM,
		FILL_BOTTOM_TO_TOP,
		FILL_MODE_MAX
	};

	void set_fill_mode(int p_fill);
	int get_fill_mode();

	void set_show_percentage(bool p_visible);
	bool is_percentage_shown() const;

	void set_indeterminate(bool p_indeterminate);
	bool is_indeterminate() const;

	void set_editor_preview_indeterminate(bool p_indeterminate_preview);
	bool is_editor_preview_indeterminate_enabled() const;

	Size2 get_minimum_size() const override;
	ProgressBar();

private:
	float _inderminate_fill_progress = 0;

	FillMode mode = FILL_BEGIN_TO_END;
};

VARIANT_ENUM_CAST(ProgressBar::FillMode);
