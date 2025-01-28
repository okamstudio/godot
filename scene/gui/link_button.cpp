/**************************************************************************/
/*  link_button.cpp                                                       */
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

#include "link_button.h"

#include "scene/theme/theme_db.h"

void LinkButton::_shape() {
	Ref<Font> font = theme_cache.font;
	int font_size = theme_cache.font_size;

	text_buf->clear();
	if (text_direction == Control::TEXT_DIRECTION_INHERITED) {
		text_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
	} else {
		text_buf->set_direction((TextServer::Direction)text_direction);
	}
	TS->shaped_text_set_bidi_override(text_buf->get_rid(), structured_text_parser(st_parser, st_args, xl_text));
	text_buf->add_string(xl_text, font, font_size, language);
}

void LinkButton::set_text(const String &p_text) {
	if (text == p_text) {
		return;
	}
	text = p_text;
	xl_text = atr(text);
	_shape();
	update_minimum_size();
	queue_redraw();
}

String LinkButton::get_text() const {
	return text;
}

void LinkButton::set_structured_text_bidi_override(TextServer::StructuredTextParser p_parser) {
	if (st_parser != p_parser) {
		st_parser = p_parser;
		_shape();
		queue_redraw();
	}
}

TextServer::StructuredTextParser LinkButton::get_structured_text_bidi_override() const {
	return st_parser;
}

void LinkButton::set_structured_text_bidi_override_options(Array p_args) {
	st_args = p_args;
	_shape();
	queue_redraw();
}

Array LinkButton::get_structured_text_bidi_override_options() const {
	return st_args;
}

void LinkButton::set_text_direction(Control::TextDirection p_text_direction) {
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);
	if (text_direction != p_text_direction) {
		text_direction = p_text_direction;
		_shape();
		queue_redraw();
	}
}

Control::TextDirection LinkButton::get_text_direction() const {
	return text_direction;
}

void LinkButton::set_language(const String &p_language) {
	if (language != p_language) {
		language = p_language;
		_shape();
		queue_redraw();
	}
}

String LinkButton::get_language() const {
	return language;
}

void LinkButton::set_uri(const String &p_uri) {
	uri = p_uri;
}

String LinkButton::get_uri() const {
	return uri;
}

void LinkButton::set_underline_mode(UnderlineMode p_underline_mode) {
	if (underline_mode == p_underline_mode) {
		return;
	}

	underline_mode = p_underline_mode;
	queue_redraw();
}

LinkButton::UnderlineMode LinkButton::get_underline_mode() const {
	return underline_mode;
}

Ref<Font> LinkButton::get_button_font() const {
	return theme_cache.font;
}

void LinkButton::pressed() {
	if (uri.is_empty()) {
		return;
	}

	OS::get_singleton()->shell_open(uri);
}

Size2 LinkButton::get_minimum_size() const {
	return text_buf->get_size();
}

void LinkButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_TRANSLATION_CHANGED: {
			xl_text = atr(text);
			_shape();
			update_minimum_size();
			queue_redraw();
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			queue_redraw();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_shape();
			update_minimum_size();
			queue_redraw();
		} break;

		case NOTIFICATION_DRAW: {
			RID ci = get_canvas_item();
			Size2 size = get_size();
			Color color;
			bool do_underline = false;

			switch (get_draw_mode()) {
				case DRAW_NORMAL: {
					if (has_focus()) {
						color = theme_cache.font_focus_color;
					} else {
						color = theme_cache.font_color;
					}

					do_underline = underline_mode == UNDERLINE_MODE_ALWAYS;
				} break;
				case DRAW_HOVER_PRESSED:
				case DRAW_PRESSED: {
					if (has_theme_color(SNAME("font_pressed_color"))) {
						color = theme_cache.font_pressed_color;
					} else {
						color = theme_cache.font_color;
					}

					do_underline = underline_mode != UNDERLINE_MODE_NEVER;

				} break;
				case DRAW_HOVER: {
					color = theme_cache.font_hover_color;
					do_underline = underline_mode != UNDERLINE_MODE_NEVER;

				} break;
				case DRAW_DISABLED: {
					color = theme_cache.font_disabled_color;
					do_underline = underline_mode == UNDERLINE_MODE_ALWAYS;

				} break;
			}

			if (has_focus()) {
				Ref<StyleBox> style = theme_cache.focus;
				style->draw(ci, Rect2(Point2(), size));
			}

			int width = text_buf->get_line_width();

			Color font_outline_color = theme_cache.font_outline_color;
			int outline_size = theme_cache.outline_size;
			if (is_layout_rtl()) {
				if (outline_size > 0 && font_outline_color.a > 0) {
					text_buf->draw_outline(get_canvas_item(), Vector2(size.width - width, 0), outline_size, font_outline_color);
				}
				text_buf->draw(get_canvas_item(), Vector2(size.width - width, 0), color);
			} else {
				if (outline_size > 0 && font_outline_color.a > 0) {
					text_buf->draw_outline(get_canvas_item(), Vector2(0, 0), outline_size, font_outline_color);
				}
				text_buf->draw(get_canvas_item(), Vector2(0, 0), color);
			}

			if (do_underline) {
				int underline_spacing = theme_cache.underline_spacing + text_buf->get_line_underline_position();
				int y = text_buf->get_line_ascent() + underline_spacing;
				int underline_thickness = MAX(1, text_buf->get_line_underline_thickness());

				if (is_layout_rtl()) {
					draw_line(Vector2(size.width - width, y), Vector2(size.width, y), color, underline_thickness);
				} else {
					draw_line(Vector2(0, y), Vector2(width, y), color, underline_thickness);
				}
			}
		} break;
	}
}

void LinkButton::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "text"), &LinkButton::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &LinkButton::get_text);
	ClassDB::bind_method(D_METHOD("set_text_direction", "direction"), &LinkButton::set_text_direction);
	ClassDB::bind_method(D_METHOD("get_text_direction"), &LinkButton::get_text_direction);
	ClassDB::bind_method(D_METHOD("set_language", "language"), &LinkButton::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &LinkButton::get_language);
	ClassDB::bind_method(D_METHOD("set_uri", "uri"), &LinkButton::set_uri);
	ClassDB::bind_method(D_METHOD("get_uri"), &LinkButton::get_uri);
	ClassDB::bind_method(D_METHOD("set_underline_mode", "underline_mode"), &LinkButton::set_underline_mode);
	ClassDB::bind_method(D_METHOD("get_underline_mode"), &LinkButton::get_underline_mode);
	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override", "parser"), &LinkButton::set_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override"), &LinkButton::get_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override_options", "args"), &LinkButton::set_structured_text_bidi_override_options);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override_options"), &LinkButton::get_structured_text_bidi_override_options);

	BIND_ENUM_CONSTANT(UNDERLINE_MODE_ALWAYS);
	BIND_ENUM_CONSTANT(UNDERLINE_MODE_ON_HOVER);
	BIND_ENUM_CONSTANT(UNDERLINE_MODE_NEVER);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "underline", PROPERTY_HINT_ENUM, "Always,On Hover,Never"), "set_underline_mode", "get_underline_mode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "uri"), "set_uri", "get_uri");

	ADD_GROUP("BiDi", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_direction", PROPERTY_HINT_ENUM, "Auto,Left-to-Right,Right-to-Left,Inherited"), "set_text_direction", "get_text_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID, ""), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "structured_text_bidi_override", PROPERTY_HINT_ENUM, "Default,URI,File,Email,List,None,Custom"), "set_structured_text_bidi_override", "get_structured_text_bidi_override");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "structured_text_bidi_override_options"), "set_structured_text_bidi_override_options", "get_structured_text_bidi_override_options");

	BIND_THEME_ITEM(Theme::DATA_TYPE_STYLEBOX, LinkButton, focus);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, LinkButton, font_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, LinkButton, font_focus_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, LinkButton, font_pressed_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, LinkButton, font_hover_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, LinkButton, font_hover_pressed_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, LinkButton, font_disabled_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, LinkButton, font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, LinkButton, font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, LinkButton, outline_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, LinkButton, font_outline_color);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, LinkButton, underline_spacing);
}

LinkButton::LinkButton(const String &p_text) {
	text_buf.instantiate();
	set_focus_mode(FOCUS_NONE);
	set_default_cursor_shape(CURSOR_POINTING_HAND);

	set_text(p_text);
}
