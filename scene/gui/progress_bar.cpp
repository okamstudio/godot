/**************************************************************************/
/*  progress_bar.cpp                                                      */
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

#include "progress_bar.h"

#include "scene/resources/text_line.h"
#include "scene/theme/theme_db.h"

Size2 ProgressBar::get_minimum_size() const {
	Size2 minimum_size = theme_cache.background_style->get_minimum_size();
	minimum_size = minimum_size.max(theme_cache.fill_style->get_minimum_size());
	if (show_percentage) {
		String txt = "100%";
		TextLine tl = TextLine(txt, theme_cache.font, theme_cache.font_size);
		minimum_size.height = MAX(minimum_size.height, theme_cache.background_style->get_minimum_size().height + tl.get_size().y);
	} else { // this is needed, else the progressbar will collapse
		minimum_size = minimum_size.maxf(1);
	}
	return minimum_size;
}

void ProgressBar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (is_visible_in_tree()) {
				_inderminate_fill_progress += get_process_delta_time() * MAX(indeterminate_min_speed, MAX(get_size().width, get_size().height) / 2);
				queue_redraw();
			}
		} break;
		case NOTIFICATION_DRAW: {
			draw_style_box(theme_cache.background_style, Rect2(Point2(), get_size()));

			if (indeterminate) {
				Size2 size = get_size();
				real_t fill_size = MIN(size.width, size.height) * 2;

				if (is_part_of_edited_scene() && !editor_preview_indeterminate) {
					// Center the filled bar when we're not previewing the animation.
					_inderminate_fill_progress = (MAX(size.width, size.height) / 2) + (fill_size / 2);
				}

				switch (mode) {
					case FILL_END_TO_BEGIN:
					case FILL_BEGIN_TO_END: {
						// Follow the RTL layout with the animation to match how the bar would fill.
						bool right_to_left = mode == (is_layout_rtl() ? FILL_BEGIN_TO_END : FILL_END_TO_BEGIN);

						if (_inderminate_fill_progress > size.width + fill_size) {
							_inderminate_fill_progress = right_to_left ? -fill_size : 0;
						}

						real_t x = right_to_left ? size.width - _inderminate_fill_progress : _inderminate_fill_progress - fill_size;
						draw_style_box(theme_cache.fill_style, Rect2(x, 0, fill_size, size.height).intersection(Rect2(Point2(), size)));
					} break;
					case FILL_TOP_TO_BOTTOM: {
						if (_inderminate_fill_progress > size.height + fill_size) {
							_inderminate_fill_progress = 0;
						}

						draw_style_box(theme_cache.fill_style, Rect2(0, _inderminate_fill_progress - fill_size, size.width, fill_size).intersection(Rect2(Point2(), size)));
					} break;
					case FILL_BOTTOM_TO_TOP: {
						if (_inderminate_fill_progress > size.height + fill_size) {
							_inderminate_fill_progress = -fill_size;
						}

						draw_style_box(theme_cache.fill_style, Rect2(0, size.height - _inderminate_fill_progress, size.width, fill_size).intersection(Rect2(Point2(), size)));
					} break;
					case FILL_MODE_MAX:
						break;
				}

				return;
			}

			float r = get_as_ratio();

			switch (mode) {
				case FILL_BEGIN_TO_END:
				case FILL_END_TO_BEGIN: {
					int mp = theme_cache.fill_style->get_minimum_size().width;
					int p = round(r * (get_size().width - mp));
					// We want FILL_BEGIN_TO_END to map to right to left when UI layout is RTL,
					// and left to right otherwise. And likewise for FILL_END_TO_BEGIN.
					bool right_to_left = mode == (is_layout_rtl() ? FILL_BEGIN_TO_END : FILL_END_TO_BEGIN);
					if (p > 0) {
						if (right_to_left) {
							int p_remaining = round((1.0 - r) * (get_size().width - mp));
							draw_style_box(theme_cache.fill_style, Rect2(Point2(p_remaining, 0), Size2(p + theme_cache.fill_style->get_minimum_size().width, get_size().height)));
						} else {
							draw_style_box(theme_cache.fill_style, Rect2(Point2(0, 0), Size2(p + theme_cache.fill_style->get_minimum_size().width, get_size().height)));
						}
					}
				} break;
				case FILL_TOP_TO_BOTTOM:
				case FILL_BOTTOM_TO_TOP: {
					int mp = theme_cache.fill_style->get_minimum_size().height;
					int p = round(r * (get_size().height - mp));

					if (p > 0) {
						if (mode == FILL_TOP_TO_BOTTOM) {
							draw_style_box(theme_cache.fill_style, Rect2(Point2(0, 0), Size2(get_size().width, p + theme_cache.fill_style->get_minimum_size().height)));
						} else {
							int p_remaining = round((1.0 - r) * (get_size().height - mp));
							draw_style_box(theme_cache.fill_style, Rect2(Point2(0, p_remaining), Size2(get_size().width, p + theme_cache.fill_style->get_minimum_size().height)));
						}
					}
				} break;
				case FILL_MODE_MAX:
					break;
			}

			if (show_percentage) {
				double ratio = 0;

				// Avoid division by zero.
				if (Math::is_equal_approx(get_max(), get_min())) {
					ratio = 1;
				} else if (is_ratio_exp() && get_min() >= 0 && get_value() >= 0) {
					double exp_min = get_min() == 0 ? 0.0 : Math::log(get_min()) / Math::log((double)2);
					double exp_max = Math::log(get_max()) / Math::log((double)2);
					double exp_value = get_value() == 0 ? 0.0 : Math::log(get_value()) / Math::log((double)2);
					double percentage = (exp_value - exp_min) / (exp_max - exp_min);

					ratio = CLAMP(percentage, is_lesser_allowed() ? percentage : 0, is_greater_allowed() ? percentage : 1);
				} else {
					double percentage = (get_value() - get_min()) / (get_max() - get_min());

					ratio = CLAMP(percentage, is_lesser_allowed() ? percentage : 0, is_greater_allowed() ? percentage : 1);
				}

				String txt = itos(int(ratio * 100));

				if (is_localizing_numeral_system()) {
					txt = TS->format_number(txt) + TS->percent_sign();
				} else {
					txt += String("%");
				}

				TextLine tl = TextLine(txt, theme_cache.font, theme_cache.font_size);
				Vector2 text_pos = (Point2(get_size().width - tl.get_size().x, get_size().height - tl.get_size().y) / 2).round();

				if (theme_cache.font_outline_size > 0 && theme_cache.font_outline_color.a > 0) {
					tl.draw_outline(get_canvas_item(), text_pos, theme_cache.font_outline_size, theme_cache.font_outline_color);
				}

				tl.draw(get_canvas_item(), text_pos, theme_cache.font_color);
			}
		} break;
	}
}

void ProgressBar::_validate_property(PropertyInfo &p_property) const {
	if (indeterminate && p_property.name == "show_percentage") {
		p_property.usage |= PROPERTY_USAGE_READ_ONLY;
	}
	if (!indeterminate && p_property.name == "editor_preview_indeterminate") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void ProgressBar::set_fill_mode(int p_fill) {
	ERR_FAIL_INDEX(p_fill, FILL_MODE_MAX);
	mode = (FillMode)p_fill;
	_inderminate_fill_progress = 0;
	queue_redraw();
}

int ProgressBar::get_fill_mode() {
	return mode;
}

void ProgressBar::set_show_percentage(bool p_visible) {
	if (show_percentage == p_visible) {
		return;
	}
	show_percentage = p_visible;
	update_minimum_size();
	queue_redraw();
}

bool ProgressBar::is_percentage_shown() const {
	return show_percentage;
}

void ProgressBar::set_indeterminate(bool p_indeterminate) {
	if (indeterminate == p_indeterminate) {
		return;
	}
	indeterminate = p_indeterminate;
	_inderminate_fill_progress = 0;

	bool should_process = !is_part_of_edited_scene() || editor_preview_indeterminate;
	set_process_internal(indeterminate && should_process);

	notify_property_list_changed();
	update_minimum_size();
	queue_redraw();
}

bool ProgressBar::is_indeterminate() const {
	return indeterminate;
}

void ProgressBar::set_editor_preview_indeterminate(bool p_preview_indeterminate) {
	if (editor_preview_indeterminate == p_preview_indeterminate) {
		return;
	}
	editor_preview_indeterminate = p_preview_indeterminate;

	if (is_part_of_edited_scene()) {
		_inderminate_fill_progress = 0;
		set_process_internal(indeterminate && editor_preview_indeterminate);
		queue_redraw();
	}
}

bool ProgressBar::is_editor_preview_indeterminate_enabled() const {
	return editor_preview_indeterminate;
}

void ProgressBar::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_fill_mode", "mode"), &ProgressBar::set_fill_mode);
	ClassDB::bind_method(D_METHOD("get_fill_mode"), &ProgressBar::get_fill_mode);
	ClassDB::bind_method(D_METHOD("set_show_percentage", "visible"), &ProgressBar::set_show_percentage);
	ClassDB::bind_method(D_METHOD("is_percentage_shown"), &ProgressBar::is_percentage_shown);
	ClassDB::bind_method(D_METHOD("set_indeterminate", "indeterminate"), &ProgressBar::set_indeterminate);
	ClassDB::bind_method(D_METHOD("is_indeterminate"), &ProgressBar::is_indeterminate);
	ClassDB::bind_method(D_METHOD("set_editor_preview_indeterminate", "preview_indeterminate"), &ProgressBar::set_editor_preview_indeterminate);
	ClassDB::bind_method(D_METHOD("is_editor_preview_indeterminate_enabled"), &ProgressBar::is_editor_preview_indeterminate_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "fill_mode", PROPERTY_HINT_ENUM, "Begin to End,End to Begin,Top to Bottom,Bottom to Top"), "set_fill_mode", "get_fill_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_percentage"), "set_show_percentage", "is_percentage_shown");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "indeterminate"), "set_indeterminate", "is_indeterminate");
	ADD_GROUP("Editor", "editor_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editor_preview_indeterminate"), "set_editor_preview_indeterminate", "is_editor_preview_indeterminate_enabled");

	BIND_ENUM_CONSTANT(FILL_BEGIN_TO_END);
	BIND_ENUM_CONSTANT(FILL_END_TO_BEGIN);
	BIND_ENUM_CONSTANT(FILL_TOP_TO_BOTTOM);
	BIND_ENUM_CONSTANT(FILL_BOTTOM_TO_TOP);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ProgressBar, background_style, "background");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, ProgressBar, fill_style, "fill");

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, ProgressBar, font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, ProgressBar, font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, ProgressBar, font_color);
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, ProgressBar, font_outline_size, "outline_size");
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, ProgressBar, font_outline_color);
}

ProgressBar::ProgressBar() {
	set_v_size_flags(0);
	set_step(0.01);
}
