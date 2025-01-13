/**************************************************************************/
/*  button.cpp                                                            */
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

#include "button.h"

#include "scene/theme/theme_db.h"

Size2 Button::get_minimum_size() const {
	Ref<Texture2D> _icon = icon;
	if (_icon.is_null() && has_theme_icon(SNAME("icon"))) {
		_icon = theme_cache.icon;
	}

	return get_minimum_size_for_text_and_icon("", _icon);
}

void Button::_set_internal_margin(Side p_side, float p_value) {
	_internal_margin[p_side] = p_value;
}

void Button::_queue_update_size_cache() {
}

void Button::_update_theme_item_cache() {
	Control::_update_theme_item_cache();

	const bool rtl = is_layout_rtl();
	if (rtl && has_theme_stylebox(SNAME("normal_mirrored"))) {
		theme_cache.max_style_size = theme_cache.normal_mirrored->get_minimum_size();
		theme_cache.style_margin_left = theme_cache.normal_mirrored->get_margin(SIDE_LEFT);
		theme_cache.style_margin_right = theme_cache.normal_mirrored->get_margin(SIDE_RIGHT);
		theme_cache.style_margin_top = theme_cache.normal_mirrored->get_margin(SIDE_TOP);
		theme_cache.style_margin_bottom = theme_cache.normal_mirrored->get_margin(SIDE_BOTTOM);
	} else {
		theme_cache.max_style_size = theme_cache.normal->get_minimum_size();
		theme_cache.style_margin_left = theme_cache.normal->get_margin(SIDE_LEFT);
		theme_cache.style_margin_right = theme_cache.normal->get_margin(SIDE_RIGHT);
		theme_cache.style_margin_top = theme_cache.normal->get_margin(SIDE_TOP);
		theme_cache.style_margin_bottom = theme_cache.normal->get_margin(SIDE_BOTTOM);
	}
	if (has_theme_stylebox("hover_pressed")) {
		if (rtl && has_theme_stylebox(SNAME("hover_pressed_mirrored"))) {
			theme_cache.max_style_size = theme_cache.max_style_size.max(theme_cache.hover_pressed_mirrored->get_minimum_size());
			theme_cache.style_margin_left = MAX(theme_cache.style_margin_left, theme_cache.hover_pressed_mirrored->get_margin(SIDE_LEFT));
			theme_cache.style_margin_right = MAX(theme_cache.style_margin_right, theme_cache.hover_pressed_mirrored->get_margin(SIDE_RIGHT));
			theme_cache.style_margin_top = MAX(theme_cache.style_margin_top, theme_cache.hover_pressed_mirrored->get_margin(SIDE_TOP));
			theme_cache.style_margin_bottom = MAX(theme_cache.style_margin_bottom, theme_cache.hover_pressed_mirrored->get_margin(SIDE_BOTTOM));
		} else {
			theme_cache.max_style_size = theme_cache.max_style_size.max(theme_cache.hover_pressed->get_minimum_size());
			theme_cache.style_margin_left = MAX(theme_cache.style_margin_left, theme_cache.hover_pressed->get_margin(SIDE_LEFT));
			theme_cache.style_margin_right = MAX(theme_cache.style_margin_right, theme_cache.hover_pressed->get_margin(SIDE_RIGHT));
			theme_cache.style_margin_top = MAX(theme_cache.style_margin_top, theme_cache.hover_pressed->get_margin(SIDE_TOP));
			theme_cache.style_margin_bottom = MAX(theme_cache.style_margin_bottom, theme_cache.hover_pressed->get_margin(SIDE_BOTTOM));
		}
	}
	if (rtl && has_theme_stylebox(SNAME("pressed_mirrored"))) {
		theme_cache.max_style_size = theme_cache.max_style_size.max(theme_cache.pressed_mirrored->get_minimum_size());
		theme_cache.style_margin_left = MAX(theme_cache.style_margin_left, theme_cache.pressed_mirrored->get_margin(SIDE_LEFT));
		theme_cache.style_margin_right = MAX(theme_cache.style_margin_right, theme_cache.pressed_mirrored->get_margin(SIDE_RIGHT));
		theme_cache.style_margin_top = MAX(theme_cache.style_margin_top, theme_cache.pressed_mirrored->get_margin(SIDE_TOP));
		theme_cache.style_margin_bottom = MAX(theme_cache.style_margin_bottom, theme_cache.pressed_mirrored->get_margin(SIDE_BOTTOM));
	} else {
		theme_cache.max_style_size = theme_cache.max_style_size.max(theme_cache.pressed->get_minimum_size());
		theme_cache.style_margin_left = MAX(theme_cache.style_margin_left, theme_cache.pressed->get_margin(SIDE_LEFT));
		theme_cache.style_margin_right = MAX(theme_cache.style_margin_right, theme_cache.pressed->get_margin(SIDE_RIGHT));
		theme_cache.style_margin_top = MAX(theme_cache.style_margin_top, theme_cache.pressed->get_margin(SIDE_TOP));
		theme_cache.style_margin_bottom = MAX(theme_cache.style_margin_bottom, theme_cache.pressed->get_margin(SIDE_BOTTOM));
	}
	if (rtl && has_theme_stylebox(SNAME("hover_mirrored"))) {
		theme_cache.max_style_size = theme_cache.max_style_size.max(theme_cache.hover_mirrored->get_minimum_size());
		theme_cache.style_margin_left = MAX(theme_cache.style_margin_left, theme_cache.hover_mirrored->get_margin(SIDE_LEFT));
		theme_cache.style_margin_right = MAX(theme_cache.style_margin_right, theme_cache.hover_mirrored->get_margin(SIDE_RIGHT));
		theme_cache.style_margin_top = MAX(theme_cache.style_margin_top, theme_cache.hover_mirrored->get_margin(SIDE_TOP));
		theme_cache.style_margin_bottom = MAX(theme_cache.style_margin_bottom, theme_cache.hover_mirrored->get_margin(SIDE_BOTTOM));
	} else {
		theme_cache.max_style_size = theme_cache.max_style_size.max(theme_cache.hover->get_minimum_size());
		theme_cache.style_margin_left = MAX(theme_cache.style_margin_left, theme_cache.hover->get_margin(SIDE_LEFT));
		theme_cache.style_margin_right = MAX(theme_cache.style_margin_right, theme_cache.hover->get_margin(SIDE_RIGHT));
		theme_cache.style_margin_top = MAX(theme_cache.style_margin_top, theme_cache.hover->get_margin(SIDE_TOP));
		theme_cache.style_margin_bottom = MAX(theme_cache.style_margin_bottom, theme_cache.hover->get_margin(SIDE_BOTTOM));
	}
	if (rtl && has_theme_stylebox(SNAME("disabled_mirrored"))) {
		theme_cache.max_style_size = theme_cache.max_style_size.max(theme_cache.disabled_mirrored->get_minimum_size());
		theme_cache.style_margin_left = MAX(theme_cache.style_margin_left, theme_cache.disabled_mirrored->get_margin(SIDE_LEFT));
		theme_cache.style_margin_right = MAX(theme_cache.style_margin_right, theme_cache.disabled_mirrored->get_margin(SIDE_RIGHT));
		theme_cache.style_margin_top = MAX(theme_cache.style_margin_top, theme_cache.disabled_mirrored->get_margin(SIDE_TOP));
		theme_cache.style_margin_bottom = MAX(theme_cache.style_margin_bottom, theme_cache.disabled_mirrored->get_margin(SIDE_BOTTOM));
	} else {
		theme_cache.max_style_size = theme_cache.max_style_size.max(theme_cache.disabled->get_minimum_size());
		theme_cache.style_margin_left = MAX(theme_cache.style_margin_left, theme_cache.disabled->get_margin(SIDE_LEFT));
		theme_cache.style_margin_right = MAX(theme_cache.style_margin_right, theme_cache.disabled->get_margin(SIDE_RIGHT));
		theme_cache.style_margin_top = MAX(theme_cache.style_margin_top, theme_cache.disabled->get_margin(SIDE_TOP));
		theme_cache.style_margin_bottom = MAX(theme_cache.style_margin_bottom, theme_cache.disabled->get_margin(SIDE_BOTTOM));
	}
	theme_cache.max_style_size = theme_cache.max_style_size.max(Vector2(theme_cache.style_margin_left + theme_cache.style_margin_right, theme_cache.style_margin_top + theme_cache.style_margin_bottom));
}

Size2 Button::_get_largest_stylebox_size() const {
	return theme_cache.max_style_size;
}

Ref<StyleBox> Button::_get_current_stylebox() const {
	Ref<StyleBox> stylebox = theme_cache.normal;
	const bool rtl = is_layout_rtl();

	switch (get_draw_mode()) {
		case DRAW_NORMAL: {
			if (rtl && has_theme_stylebox(SNAME("normal_mirrored"))) {
				stylebox = theme_cache.normal_mirrored;
			} else {
				stylebox = theme_cache.normal;
			}
		} break;

		case DRAW_HOVER_PRESSED: {
			// Edge case for CheckButton and CheckBox.
			if (has_theme_stylebox("hover_pressed")) {
				if (rtl && has_theme_stylebox(SNAME("hover_pressed_mirrored"))) {
					stylebox = theme_cache.hover_pressed_mirrored;
				} else {
					stylebox = theme_cache.hover_pressed;
				}
				break;
			}
		}
			[[fallthrough]];
		case DRAW_PRESSED: {
			if (rtl && has_theme_stylebox(SNAME("pressed_mirrored"))) {
				stylebox = theme_cache.pressed_mirrored;
			} else {
				stylebox = theme_cache.pressed;
			}
		} break;

		case DRAW_HOVER: {
			if (rtl && has_theme_stylebox(SNAME("hover_mirrored"))) {
				stylebox = theme_cache.hover_mirrored;
			} else {
				stylebox = theme_cache.hover;
			}
		} break;

		case DRAW_DISABLED: {
			if (rtl && has_theme_stylebox(SNAME("disabled_mirrored"))) {
				stylebox = theme_cache.disabled_mirrored;
			} else {
				stylebox = theme_cache.disabled;
			}
		} break;
	}

	return stylebox;
}

void Button::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			queue_redraw();
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED: {
			xl_text = atr(text);
			_shape();

			update_minimum_size();
			queue_redraw();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_shape();

			update_minimum_size();
			queue_redraw();
		} break;

		case NOTIFICATION_RESIZED: {
			if (autowrap_mode != TextServer::AUTOWRAP_OFF) {
				_shape();

				update_minimum_size();
				queue_redraw();
			}
		} break;

		case NOTIFICATION_DRAW: {
			// Reshape and update size min. if text is invalidated by an external source (e.g., oversampling).
			if (text_buf.is_valid() && !TS->shaped_text_is_ready(text_buf->get_rid())) {
				_shape();

				update_minimum_size();
			}

			const RID ci = get_canvas_item();
			const Size2 size = get_size();

			Ref<StyleBox> style = _get_current_stylebox();
			// Draws the stylebox in the current state.
			if (!flat) {
				style->draw(ci, Rect2(Point2(), size));
			}

			if (has_focus()) {
				theme_cache.focus->draw(ci, Rect2(Point2(), size));
			}

			Ref<Texture2D> _icon = icon;
			if (_icon.is_null() && has_theme_icon(SNAME("icon"))) {
				_icon = theme_cache.icon;
			}

			if (xl_text.is_empty() && _icon.is_null()) {
				break;
			}

			const float style_margin_left = (theme_cache.align_to_largest_stylebox) ? theme_cache.style_margin_left : style->get_margin(SIDE_LEFT);
			const float style_margin_right = (theme_cache.align_to_largest_stylebox) ? theme_cache.style_margin_right : style->get_margin(SIDE_RIGHT);
			const float style_margin_top = (theme_cache.align_to_largest_stylebox) ? theme_cache.style_margin_top : style->get_margin(SIDE_TOP);
			const float style_margin_bottom = (theme_cache.align_to_largest_stylebox) ? theme_cache.style_margin_bottom : style->get_margin(SIDE_BOTTOM);

			Size2 drawable_size_remained = size;

			{ // The size after the stelybox is stripped.
				drawable_size_remained.width -= style_margin_left + style_margin_right;
				drawable_size_remained.height -= style_margin_top + style_margin_bottom;
			}

			const int h_separation = MAX(0, theme_cache.h_separation);

			float left_internal_margin_with_h_separation = _internal_margin[SIDE_LEFT];
			float right_internal_margin_with_h_separation = _internal_margin[SIDE_RIGHT];
			{ // The width reserved for internal element in derived classes (and h_separation if needed).

				if (_internal_margin[SIDE_LEFT] > 0.0f) {
					left_internal_margin_with_h_separation += h_separation;
				}

				if (_internal_margin[SIDE_RIGHT] > 0.0f) {
					right_internal_margin_with_h_separation += h_separation;
				}

				drawable_size_remained.width -= left_internal_margin_with_h_separation + right_internal_margin_with_h_separation; // The size after the internal element is stripped.
			}

			HorizontalAlignment icon_align_rtl_checked = horizontal_icon_alignment;
			HorizontalAlignment align_rtl_checked = alignment;
			// Swap icon and text alignment sides if right-to-left layout is set.
			if (is_layout_rtl()) {
				if (horizontal_icon_alignment == HORIZONTAL_ALIGNMENT_RIGHT) {
					icon_align_rtl_checked = HORIZONTAL_ALIGNMENT_LEFT;
				} else if (horizontal_icon_alignment == HORIZONTAL_ALIGNMENT_LEFT) {
					icon_align_rtl_checked = HORIZONTAL_ALIGNMENT_RIGHT;
				}
				if (alignment == HORIZONTAL_ALIGNMENT_RIGHT) {
					align_rtl_checked = HORIZONTAL_ALIGNMENT_LEFT;
				} else if (alignment == HORIZONTAL_ALIGNMENT_LEFT) {
					align_rtl_checked = HORIZONTAL_ALIGNMENT_RIGHT;
				}
			}

			Color font_color;
			Color icon_modulate_color(1, 1, 1, 1);
			// Get the font color and icon modulate color in the current state.
			switch (get_draw_mode()) {
				case DRAW_NORMAL: {
					// Focus colors only take precedence over normal state.
					if (has_focus()) {
						font_color = theme_cache.font_focus_color;
						if (has_theme_color(SNAME("icon_focus_color"))) {
							icon_modulate_color = theme_cache.icon_focus_color;
						}
					} else {
						font_color = theme_cache.font_color;
						if (has_theme_color(SNAME("icon_normal_color"))) {
							icon_modulate_color = theme_cache.icon_normal_color;
						}
					}
				} break;
				case DRAW_HOVER_PRESSED: {
					font_color = theme_cache.font_hover_pressed_color;
					if (has_theme_color(SNAME("icon_hover_pressed_color"))) {
						icon_modulate_color = theme_cache.icon_hover_pressed_color;
					}

				} break;
				case DRAW_PRESSED: {
					if (has_theme_color(SNAME("font_pressed_color"))) {
						font_color = theme_cache.font_pressed_color;
					} else {
						font_color = theme_cache.font_color;
					}
					if (has_theme_color(SNAME("icon_pressed_color"))) {
						icon_modulate_color = theme_cache.icon_pressed_color;
					}

				} break;
				case DRAW_HOVER: {
					font_color = theme_cache.font_hover_color;
					if (has_theme_color(SNAME("icon_hover_color"))) {
						icon_modulate_color = theme_cache.icon_hover_color;
					}

				} break;
				case DRAW_DISABLED: {
					font_color = theme_cache.font_disabled_color;
					if (has_theme_color(SNAME("icon_disabled_color"))) {
						icon_modulate_color = theme_cache.icon_disabled_color;
					} else {
						icon_modulate_color.a = 0.4;
					}

				} break;
			}

			const bool is_clipped = clip_text || overrun_behavior != TextServer::OVERRUN_NO_TRIMMING || autowrap_mode != TextServer::AUTOWRAP_OFF;
			const Size2 custom_element_size = drawable_size_remained;

			// Draw the icon.
			if (_icon.is_valid()) {
				Size2 icon_size;

				{ // Calculate the drawing size of the icon.
					icon_size = _icon->get_size();

					if (expand_icon) {
						const Size2 text_buf_size = text_buf->get_size();
						Size2 _size = custom_element_size;
						if (!is_clipped && icon_align_rtl_checked != HORIZONTAL_ALIGNMENT_CENTER && text_buf_size.width > 0.0f) {
							// If there is not enough space for icon and h_separation, h_separation will occupy the space first,
							// so the icon's width may be negative. Keep it negative to make it easier to calculate the space
							// reserved for text later.
							_size.width -= text_buf_size.width + h_separation;
						}
						if (vertical_icon_alignment != VERTICAL_ALIGNMENT_CENTER) {
							_size.height -= text_buf_size.height;
						}

						float icon_width = icon_size.width * _size.height / icon_size.height;
						float icon_height = _size.height;

						if (icon_width > _size.width) {
							icon_width = _size.width;
							icon_height = icon_size.height * icon_width / icon_size.width;
						}

						icon_size = Size2(icon_width, icon_height);
					}
					icon_size = _fit_icon_size(icon_size);
					icon_size = icon_size.round();
				}

				if (icon_size.width > 0.0f) {
					// Calculate the drawing position of the icon.
					Point2 icon_ofs;

					switch (icon_align_rtl_checked) {
						case HORIZONTAL_ALIGNMENT_CENTER: {
							icon_ofs.x = (custom_element_size.width - icon_size.width) / 2.0f;
						}
							[[fallthrough]];
						case HORIZONTAL_ALIGNMENT_FILL:
						case HORIZONTAL_ALIGNMENT_LEFT: {
							icon_ofs.x += style_margin_left;
							icon_ofs.x += left_internal_margin_with_h_separation;
						} break;

						case HORIZONTAL_ALIGNMENT_RIGHT: {
							icon_ofs.x = size.x - style_margin_right;
							icon_ofs.x -= right_internal_margin_with_h_separation;
							icon_ofs.x -= icon_size.width;
						} break;
					}

					switch (vertical_icon_alignment) {
						case VERTICAL_ALIGNMENT_CENTER: {
							icon_ofs.y = (custom_element_size.height - icon_size.height) / 2.0f;
						}
							[[fallthrough]];
						case VERTICAL_ALIGNMENT_FILL:
						case VERTICAL_ALIGNMENT_TOP: {
							icon_ofs.y += style_margin_top;
						} break;

						case VERTICAL_ALIGNMENT_BOTTOM: {
							icon_ofs.y = size.y - style_margin_bottom - icon_size.height;
						} break;
					}
					icon_ofs = icon_ofs.floor();

					Rect2 icon_region = Rect2(icon_ofs, icon_size);
					draw_texture_rect(_icon, icon_region, false, icon_modulate_color);
				}

				if (!xl_text.is_empty()) {
					// Update the size after the icon is stripped. Stripping only when the icon alignments are not center.
					if (icon_align_rtl_checked != HORIZONTAL_ALIGNMENT_CENTER) {
						// Subtract the space's width occupied by icon and h_separation together.
						drawable_size_remained.width -= icon_size.width + h_separation;
					}

					if (vertical_icon_alignment != VERTICAL_ALIGNMENT_CENTER) {
						drawable_size_remained.height -= icon_size.height;
					}
				}
			}

			// Draw the text.
			if (!xl_text.is_empty()) {
				text_buf->set_alignment(align_rtl_checked);

				float text_buf_width = Math::ceil(MAX(1.0f, drawable_size_remained.width)); // The space's width filled by the text_buf.
				if (autowrap_mode != TextServer::AUTOWRAP_OFF && !Math::is_equal_approx(text_buf_width, text_buf->get_width())) {
					update_minimum_size();
				}
				text_buf->set_width(text_buf_width);

				Point2 text_ofs;

				switch (align_rtl_checked) {
					case HORIZONTAL_ALIGNMENT_CENTER: {
						text_ofs.x = (drawable_size_remained.width - text_buf_width) / 2.0f;
					}
						[[fallthrough]];
					case HORIZONTAL_ALIGNMENT_FILL:
					case HORIZONTAL_ALIGNMENT_LEFT:
					case HORIZONTAL_ALIGNMENT_RIGHT: {
						text_ofs.x += style_margin_left;
						text_ofs.x += left_internal_margin_with_h_separation;
						if (icon_align_rtl_checked == HORIZONTAL_ALIGNMENT_LEFT) {
							// Offset by the space's width that occupied by icon and h_separation together.
							text_ofs.x += custom_element_size.width - drawable_size_remained.width;
						}
					} break;
				}

				text_ofs.y = (drawable_size_remained.height - text_buf->get_size().height) / 2.0f + style_margin_top;
				if (vertical_icon_alignment == VERTICAL_ALIGNMENT_TOP) {
					text_ofs.y += custom_element_size.height - drawable_size_remained.height; // Offset by the icon's height.
				}

				Color font_outline_color = theme_cache.font_outline_color;
				int outline_size = theme_cache.outline_size;
				if (outline_size > 0 && font_outline_color.a > 0.0f) {
					text_buf->draw_outline(ci, text_ofs, outline_size, font_outline_color);
				}
				text_buf->draw(ci, text_ofs, font_color);
			}
		} break;
	}
}

Size2 Button::_fit_icon_size(const Size2 &p_size) const {
	int max_width = theme_cache.icon_max_width;
	Size2 icon_size = p_size;

	if (max_width > 0 && icon_size.width > max_width) {
		icon_size.height = icon_size.height * max_width / icon_size.width;
		icon_size.width = max_width;
	}

	return icon_size;
}

Size2 Button::get_minimum_size_for_text_and_icon(const String &p_text, Ref<Texture2D> p_icon) const {
	// Do not include `_internal_margin`, it's already added in the `get_minimum_size` overrides.

	Ref<TextParagraph> paragraph;
	if (p_text.is_empty()) {
		paragraph = text_buf;
	} else {
		paragraph.instantiate();
		_shape(paragraph, p_text);
	}

	Size2 minsize = paragraph->get_size();
	if (clip_text || overrun_behavior != TextServer::OVERRUN_NO_TRIMMING || autowrap_mode != TextServer::AUTOWRAP_OFF) {
		minsize.width = 0;
	}

	if (!expand_icon && p_icon.is_valid()) {
		Size2 icon_size = _fit_icon_size(p_icon->get_size());
		if (vertical_icon_alignment == VERTICAL_ALIGNMENT_CENTER) {
			minsize.height = MAX(minsize.height, icon_size.height);
		} else {
			minsize.height += icon_size.height;
		}

		if (horizontal_icon_alignment != HORIZONTAL_ALIGNMENT_CENTER) {
			minsize.width += icon_size.width;
			if (!xl_text.is_empty() || !p_text.is_empty()) {
				minsize.width += MAX(0, theme_cache.h_separation);
			}
		} else {
			minsize.width = MAX(minsize.width, icon_size.width);
		}
	}

	if (!xl_text.is_empty() || !p_text.is_empty()) {
		Ref<Font> font = theme_cache.font;
		float font_height = font->get_height(theme_cache.font_size);
		if (vertical_icon_alignment == VERTICAL_ALIGNMENT_CENTER) {
			minsize.height = MAX(font_height, minsize.height);
		} else {
			minsize.height += font_height;
		}
	}

	return (theme_cache.align_to_largest_stylebox ? _get_largest_stylebox_size() : _get_current_stylebox()->get_minimum_size()) + minsize;
}

void Button::_shape(Ref<TextParagraph> p_paragraph, String p_text) const {
	if (p_paragraph.is_null()) {
		p_paragraph = text_buf;
	}

	if (p_text.is_empty()) {
		p_text = xl_text;
	}

	p_paragraph->clear();

	Ref<Font> font = theme_cache.font;
	int font_size = theme_cache.font_size;
	if (font.is_null() || font_size == 0) {
		// Can't shape without a valid font and a non-zero size.
		return;
	}

	BitField<TextServer::LineBreakFlag> autowrap_flags = TextServer::BREAK_MANDATORY;
	switch (autowrap_mode) {
		case TextServer::AUTOWRAP_WORD_SMART:
			autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_WORD:
			autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_ARBITRARY:
			autowrap_flags = TextServer::BREAK_GRAPHEME_BOUND | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_OFF:
			break;
	}
	autowrap_flags = autowrap_flags | TextServer::BREAK_TRIM_EDGE_SPACES;
	p_paragraph->set_break_flags(autowrap_flags);
	p_paragraph->set_line_spacing(theme_cache.line_spacing);

	if (text_direction == Control::TEXT_DIRECTION_INHERITED) {
		p_paragraph->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
	} else {
		p_paragraph->set_direction((TextServer::Direction)text_direction);
	}
	p_paragraph->add_string(p_text, font, font_size, language);
	p_paragraph->set_text_overrun_behavior(overrun_behavior);
}

void Button::set_text_overrun_behavior(TextServer::OverrunBehavior p_behavior) {
	if (overrun_behavior != p_behavior) {
		bool need_update_cache = overrun_behavior == TextServer::OVERRUN_NO_TRIMMING || p_behavior == TextServer::OVERRUN_NO_TRIMMING;
		overrun_behavior = p_behavior;
		_shape();

		if (need_update_cache) {
			_queue_update_size_cache();
		}
		queue_redraw();
		update_minimum_size();
	}
}

TextServer::OverrunBehavior Button::get_text_overrun_behavior() const {
	return overrun_behavior;
}

void Button::set_text(const String &p_text) {
	if (text != p_text) {
		text = p_text;
		xl_text = atr(text);
		_shape();

		queue_redraw();
		update_minimum_size();
	}
}

String Button::get_text() const {
	return text;
}

void Button::set_autowrap_mode(TextServer::AutowrapMode p_mode) {
	if (autowrap_mode != p_mode) {
		autowrap_mode = p_mode;
		_shape();
		queue_redraw();
		update_minimum_size();
	}
}

TextServer::AutowrapMode Button::get_autowrap_mode() const {
	return autowrap_mode;
}

void Button::set_text_direction(Control::TextDirection p_text_direction) {
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);
	if (text_direction != p_text_direction) {
		text_direction = p_text_direction;
		_shape();
		queue_redraw();
	}
}

Control::TextDirection Button::get_text_direction() const {
	return text_direction;
}

void Button::set_language(const String &p_language) {
	if (language != p_language) {
		language = p_language;
		_shape();
		queue_redraw();
	}
}

String Button::get_language() const {
	return language;
}

void Button::set_button_icon(const Ref<Texture2D> &p_icon) {
	if (icon == p_icon) {
		return;
	}

	if (icon.is_valid()) {
		icon->disconnect_changed(callable_mp(this, &Button::_texture_changed));
	}

	icon = p_icon;

	if (icon.is_valid()) {
		icon->connect_changed(callable_mp(this, &Button::_texture_changed));
	}

	queue_redraw();
	update_minimum_size();
}

void Button::_texture_changed() {
	queue_redraw();
	update_minimum_size();
}

Ref<Texture2D> Button::get_button_icon() const {
	return icon;
}

void Button::set_expand_icon(bool p_enabled) {
	if (expand_icon != p_enabled) {
		expand_icon = p_enabled;
		_queue_update_size_cache();
		queue_redraw();
		update_minimum_size();
	}
}

bool Button::is_expand_icon() const {
	return expand_icon;
}

void Button::set_flat(bool p_enabled) {
	if (flat != p_enabled) {
		flat = p_enabled;
		queue_redraw();
	}
}

bool Button::is_flat() const {
	return flat;
}

void Button::set_clip_text(bool p_enabled) {
	if (clip_text != p_enabled) {
		clip_text = p_enabled;

		_queue_update_size_cache();
		queue_redraw();
		update_minimum_size();
	}
}

bool Button::get_clip_text() const {
	return clip_text;
}

void Button::set_text_alignment(HorizontalAlignment p_alignment) {
	if (alignment != p_alignment) {
		alignment = p_alignment;
		queue_redraw();
	}
}

HorizontalAlignment Button::get_text_alignment() const {
	return alignment;
}

void Button::set_icon_alignment(HorizontalAlignment p_alignment) {
	if (horizontal_icon_alignment == p_alignment) {
		return;
	}

	horizontal_icon_alignment = p_alignment;
	update_minimum_size();
	queue_redraw();
}

void Button::set_vertical_icon_alignment(VerticalAlignment p_alignment) {
	if (vertical_icon_alignment == p_alignment) {
		return;
	}
	bool need_update_cache = vertical_icon_alignment == VERTICAL_ALIGNMENT_CENTER || p_alignment == VERTICAL_ALIGNMENT_CENTER;
	vertical_icon_alignment = p_alignment;

	if (need_update_cache) {
		_queue_update_size_cache();
	}
	update_minimum_size();
	queue_redraw();
}

HorizontalAlignment Button::get_icon_alignment() const {
	return horizontal_icon_alignment;
}

VerticalAlignment Button::get_vertical_icon_alignment() const {
	return vertical_icon_alignment;
}

void Button::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "text"), &Button::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &Button::get_text);
	ClassDB::bind_method(D_METHOD("set_text_overrun_behavior", "overrun_behavior"), &Button::set_text_overrun_behavior);
	ClassDB::bind_method(D_METHOD("get_text_overrun_behavior"), &Button::get_text_overrun_behavior);
	ClassDB::bind_method(D_METHOD("set_autowrap_mode", "autowrap_mode"), &Button::set_autowrap_mode);
	ClassDB::bind_method(D_METHOD("get_autowrap_mode"), &Button::get_autowrap_mode);
	ClassDB::bind_method(D_METHOD("set_text_direction", "direction"), &Button::set_text_direction);
	ClassDB::bind_method(D_METHOD("get_text_direction"), &Button::get_text_direction);
	ClassDB::bind_method(D_METHOD("set_language", "language"), &Button::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &Button::get_language);
	ClassDB::bind_method(D_METHOD("set_button_icon", "texture"), &Button::set_button_icon);
	ClassDB::bind_method(D_METHOD("get_button_icon"), &Button::get_button_icon);
	ClassDB::bind_method(D_METHOD("set_flat", "enabled"), &Button::set_flat);
	ClassDB::bind_method(D_METHOD("is_flat"), &Button::is_flat);
	ClassDB::bind_method(D_METHOD("set_clip_text", "enabled"), &Button::set_clip_text);
	ClassDB::bind_method(D_METHOD("get_clip_text"), &Button::get_clip_text);
	ClassDB::bind_method(D_METHOD("set_text_alignment", "alignment"), &Button::set_text_alignment);
	ClassDB::bind_method(D_METHOD("get_text_alignment"), &Button::get_text_alignment);
	ClassDB::bind_method(D_METHOD("set_icon_alignment", "icon_alignment"), &Button::set_icon_alignment);
	ClassDB::bind_method(D_METHOD("get_icon_alignment"), &Button::get_icon_alignment);
	ClassDB::bind_method(D_METHOD("set_vertical_icon_alignment", "vertical_icon_alignment"), &Button::set_vertical_icon_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_icon_alignment"), &Button::get_vertical_icon_alignment);
	ClassDB::bind_method(D_METHOD("set_expand_icon", "enabled"), &Button::set_expand_icon);
	ClassDB::bind_method(D_METHOD("is_expand_icon"), &Button::is_expand_icon);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_button_icon", "get_button_icon");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flat"), "set_flat", "is_flat");

	ADD_GROUP("Text Behavior", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "alignment", PROPERTY_HINT_ENUM, "Left,Center,Right"), "set_text_alignment", "get_text_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_overrun_behavior", PROPERTY_HINT_ENUM, "Trim Nothing,Trim Characters,Trim Words,Ellipsis,Word Ellipsis"), "set_text_overrun_behavior", "get_text_overrun_behavior");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "autowrap_mode", PROPERTY_HINT_ENUM, "Off,Arbitrary,Word,Word (Smart)"), "set_autowrap_mode", "get_autowrap_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "clip_text"), "set_clip_text", "get_clip_text");

	ADD_GROUP("Icon Behavior", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "icon_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right"), "set_icon_alignment", "get_icon_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertical_icon_alignment", PROPERTY_HINT_ENUM, "Top,Center,Bottom"), "set_vertical_icon_alignment", "get_vertical_icon_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "expand_icon"), "set_expand_icon", "is_expand_icon");

	ADD_GROUP("BiDi", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_direction", PROPERTY_HINT_ENUM, "Auto,Left-to-Right,Right-to-Left,Inherited"), "set_text_direction", "get_text_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID, ""), "set_language", "get_language");

	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, normal);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, normal_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, pressed);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, pressed_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, hover);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, hover_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, hover_pressed);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, hover_pressed_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, disabled);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, disabled_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, Button, focus);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, font_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, font_focus_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, font_pressed_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, font_hover_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, font_hover_pressed_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, font_disabled_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, Button, font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, Button, font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Button, outline_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, font_outline_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, icon_normal_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, icon_focus_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, icon_pressed_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, icon_hover_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, icon_hover_pressed_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, Button, icon_disabled_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, Button, icon);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Button, h_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Button, icon_max_width);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Button, align_to_largest_stylebox);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, Button, line_spacing);
}

Button::Button(const String &p_text) {
	text_buf.instantiate();
	text_buf->set_break_flags(TextServer::BREAK_MANDATORY | TextServer::BREAK_TRIM_EDGE_SPACES);
	set_mouse_filter(MOUSE_FILTER_STOP);

	set_text(p_text);
}

Button::~Button() {
}
