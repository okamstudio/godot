/**************************************************************************/
/*  check_box.h                                                           */
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

#include "scene/gui/button.h"

class CheckBox : public Button {
	GDCLASS(CheckBox, Button);

	struct ThemeCache {
		int h_separation = 0;
		int check_v_offset = 0;
		Ref<StyleBox> normal_style;

		Ref<Texture2D> checked;
		Ref<Texture2D> unchecked;
		Ref<Texture2D> radio_checked;
		Ref<Texture2D> radio_unchecked;
		Ref<Texture2D> checked_disabled;
		Ref<Texture2D> unchecked_disabled;
		Ref<Texture2D> radio_checked_disabled;
		Ref<Texture2D> radio_unchecked_disabled;
	} theme_cache;

protected:
	Size2 get_icon_size() const;
	Size2 get_minimum_size() const override;

	void _notification(int p_what);
	static void _bind_methods();

	bool is_radio();

public:
	CheckBox(const String &p_text = String());
	~CheckBox();
};
