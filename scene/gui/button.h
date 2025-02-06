/**************************************************************************/
/*  button.h                                                              */
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
#include "scene/resources/text_paragraph.h"

class Button : public BaseButton {
	GDCLASS(Button, BaseButton);

private:
	bool flat = false;
	String text;
	String xl_text;
	Ref<TextParagraph> text_buf;

	String language;
	TextDirection text_direction = TEXT_DIRECTION_AUTO;
	TextServer::AutowrapMode autowrap_mode = TextServer::AUTOWRAP_OFF;
	TextServer::OverrunBehavior overrun_behavior = TextServer::OVERRUN_NO_TRIMMING;

	Ref<Texture2D> icon;
	bool expand_icon = false;
	bool clip_text = false;
	HorizontalAlignment alignment = HORIZONTAL_ALIGNMENT_CENTER;
	HorizontalAlignment horizontal_icon_alignment = HORIZONTAL_ALIGNMENT_LEFT;
	VerticalAlignment vertical_icon_alignment = VERTICAL_ALIGNMENT_CENTER;
	float _internal_margin[4] = {};

	struct ThemeCache {
		Ref<StyleBox> normal;
		Ref<StyleBox> normal_mirrored;
		Ref<StyleBox> pressed;
		Ref<StyleBox> pressed_mirrored;
		Ref<StyleBox> hover;
		Ref<StyleBox> hover_mirrored;
		Ref<StyleBox> hover_pressed;
		Ref<StyleBox> hover_pressed_mirrored;
		Ref<StyleBox> disabled;
		Ref<StyleBox> disabled_mirrored;
		Ref<StyleBox> focus;

		Size2 max_style_size;
		float style_margin_left = 0;
		float style_margin_right = 0;
		float style_margin_top = 0;
		float style_margin_bottom = 0;

		bool align_to_largest_stylebox = false;

		Color font_color;
		Color font_focus_color;
		Color font_pressed_color;
		Color font_hover_color;
		Color font_hover_pressed_color;
		Color font_disabled_color;

		Ref<Font> font;
		int font_size = 0;
		int outline_size = 0;
		Color font_outline_color;

		Color icon_normal_color;
		Color icon_focus_color;
		Color icon_pressed_color;
		Color icon_hover_color;
		Color icon_hover_pressed_color;
		Color icon_disabled_color;

		Ref<Texture2D> icon;

		int h_separation = 0;
		int icon_max_width = 0;
		int line_spacing = 0;
	} theme_cache;

	void _shape(Ref<TextParagraph> p_paragraph = Ref<TextParagraph>(), String p_text = "") const;
	void _texture_changed();

protected:
	virtual void _update_theme_item_cache() override;

	void _set_internal_margin(Side p_side, float p_value);
	virtual void _queue_update_size_cache();

	Size2 _fit_icon_size(const Size2 &p_size) const;
	Ref<StyleBox> _get_current_stylebox() const;
	Size2 _get_largest_stylebox_size() const;
	void _notification(int p_what);
	static void _bind_methods();

public:
	virtual Size2 get_minimum_size() const override;

	Size2 get_minimum_size_for_text_and_icon(const String &p_text, Ref<Texture2D> p_icon) const;

	void set_text(const String &p_text);
	String get_text() const;

	void set_text_overrun_behavior(TextServer::OverrunBehavior p_behavior);
	TextServer::OverrunBehavior get_text_overrun_behavior() const;

	void set_autowrap_mode(TextServer::AutowrapMode p_mode);
	TextServer::AutowrapMode get_autowrap_mode() const;

	void set_text_direction(TextDirection p_text_direction);
	TextDirection get_text_direction() const;

	void set_language(const String &p_language);
	String get_language() const;

	void set_button_icon(const Ref<Texture2D> &p_icon);
	Ref<Texture2D> get_button_icon() const;

	void set_expand_icon(bool p_enabled);
	bool is_expand_icon() const;

	void set_flat(bool p_enabled);
	bool is_flat() const;

	void set_clip_text(bool p_enabled);
	bool get_clip_text() const;

	void set_text_alignment(HorizontalAlignment p_alignment);
	HorizontalAlignment get_text_alignment() const;

	void set_icon_alignment(HorizontalAlignment p_alignment);
	void set_vertical_icon_alignment(VerticalAlignment p_alignment);
	HorizontalAlignment get_icon_alignment() const;
	VerticalAlignment get_vertical_icon_alignment() const;

	Button(const String &p_text = String());
	~Button();
};
