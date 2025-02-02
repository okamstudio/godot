/**************************************************************************/
/*  foldable_container.cpp                                                */
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

#include "foldable_container.h"

#include "scene/theme/theme_db.h"

Size2 FoldableContainer::get_minimum_size() const {
	Size2 title_ms = _get_title_panel_min_size();

	if (folded) {
		return title_ms;
	}
	Size2 ms;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i));
		if (!c) {
			continue;
		}
		ms = ms.max(c->get_combined_minimum_size());
	}
	ms += theme_cache.panel_style->get_minimum_size();

	return Size2(MAX(ms.width, title_ms.width), ms.height + title_ms.height);
}

void FoldableContainer::fold() {
	set_folded(true);
	emit_signal(SNAME("folding_changed"), folded);
}

void FoldableContainer::expand() {
	set_folded(false);
	emit_signal(SNAME("folding_changed"), folded);
}

void FoldableContainer::set_folded(bool p_folded) {
	if (folded != p_folded) {
		if (!changing_group && foldable_group.is_valid()) {
			if (!p_folded) {
				_update_group();
				foldable_group->emit_signal(SNAME("expanded"), this);
			} else if (!foldable_group->updating_group && foldable_group->get_expanded_container() == this && !foldable_group->is_allow_folding_all()) {
				return;
			}
		}
		folded = p_folded;

		update_minimum_size();
		queue_sort();
		queue_redraw();
	}
}

bool FoldableContainer::is_folded() const {
	return folded;
}

void FoldableContainer::set_foldable_group(const Ref<FoldableGroup> &p_group) {
	if (foldable_group.is_valid()) {
		foldable_group->containers.erase(this);
	}

	foldable_group = p_group;

	if (foldable_group.is_valid()) {
		changing_group = true;
		if (folded && !foldable_group->get_expanded_container() && !foldable_group->is_allow_folding_all()) {
			set_folded(false);
		} else if (!folded && foldable_group->get_expanded_container()) {
			set_folded(true);
		}
		foldable_group->containers.insert(this);
		changing_group = false;
	}

	queue_redraw();
}

Ref<FoldableGroup> FoldableContainer::get_foldable_group() const {
	return foldable_group;
}

void FoldableContainer::set_text(const String &p_text) {
	if (text == p_text) {
		return;
	}
	text = p_text;
	_shape();
	update_minimum_size();
	queue_redraw();
}

String FoldableContainer::get_text() const {
	return text;
}

void FoldableContainer::set_text_alignment(HorizontalAlignment p_alignment) {
	ERR_FAIL_INDEX((int)p_alignment, 3);
	text_alignment = p_alignment;

	if (_get_actual_alignment() != text_buf->get_horizontal_alignment()) {
		_shape();
		queue_redraw();
	}
}

HorizontalAlignment FoldableContainer::get_text_alignment() const {
	return text_alignment;
}

void FoldableContainer::set_language(const String &p_language) {
	if (language == p_language) {
		return;
	}
	language = p_language;
	_shape();
	update_minimum_size();
	queue_redraw();
}

String FoldableContainer::get_language() const {
	return language;
}

void FoldableContainer::set_text_direction(Control::TextDirection p_text_direction) {
	ERR_FAIL_INDEX(int(p_text_direction), 4);
	if (text_direction == p_text_direction) {
		return;
	}
	text_direction = p_text_direction;
	_shape();
	queue_redraw();
}

Control::TextDirection FoldableContainer::get_text_direction() const {
	return text_direction;
}

void FoldableContainer::set_text_overrun_behavior(TextServer::OverrunBehavior p_overrun_behavior) {
	if (overrun_behavior == p_overrun_behavior) {
		return;
	}
	overrun_behavior = p_overrun_behavior;
	_shape();
	update_minimum_size();
	queue_redraw();
}

TextServer::OverrunBehavior FoldableContainer::get_text_overrun_behavior() const {
	return overrun_behavior;
}

void FoldableContainer::set_title_position(FoldableContainer::TitlePosition p_title_position) {
	ERR_FAIL_INDEX(p_title_position, POSITION_MAX);
	if (title_position == p_title_position) {
		return;
	}
	title_position = p_title_position;
	queue_redraw();
	queue_sort();
}

FoldableContainer::TitlePosition FoldableContainer::get_title_position() const {
	return title_position;
}

void FoldableContainer::add_title_bar_control(Control *p_control) {
	ERR_FAIL_NULL(p_control);
	if (p_control->get_parent()) {
		p_control->get_parent()->remove_child(p_control);
		ERR_FAIL_COND_MSG(p_control->get_parent() != nullptr, "Failed to remove control from parent.");
	}
	add_child(p_control, false, INTERNAL_MODE_FRONT);
	title_controls.push_back(p_control);
}

void FoldableContainer::remove_title_bar_control(Control *p_control) {
	ERR_FAIL_NULL(p_control);

	int64_t index = title_controls.find(p_control);
	ERR_FAIL_COND_MSG(index == -1, "Can't remove control from title bar.");

	title_controls.remove_at(index);
	remove_child(p_control);
}

void FoldableContainer::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseMotion> m = p_event;
	if (m.is_valid()) {
		Rect2 title_rect = Rect2(0, (title_position == POSITION_TOP) ? 0 : get_size().height - title_panel_height, get_size().width, title_panel_height);
		if (title_rect.has_point(m->get_position())) {
			if (!is_hovering) {
				is_hovering = true;
				queue_redraw();
			}
		} else if (is_hovering) {
			is_hovering = false;
			queue_redraw();
		}
		return;
	}

	if (p_event->is_action_pressed(SNAME("ui_accept"), false, true)) {
		set_folded(!folded);
		emit_signal(SNAME("folding_changed"), folded);
		accept_event();
		return;
	}

	Ref<InputEventMouseButton> b = p_event;
	if (b.is_valid()) {
		Rect2 title_rect = Rect2(0, (title_position == POSITION_TOP) ? 0 : get_size().height - title_panel_height, get_size().width, title_panel_height);
		if (b->get_button_index() == MouseButton::LEFT && b->is_pressed() && title_rect.has_point(b->get_position())) {
			set_folded(!folded);
			emit_signal(SNAME("folding_changed"), folded);
			accept_event();
		}
	}
}

String FoldableContainer::get_tooltip(const Point2 &p_pos) const {
	if (Rect2(0, (title_position == POSITION_TOP) ? 0 : get_size().height - title_panel_height, get_size().width, title_panel_height).has_point(p_pos)) {
		return Control::get_tooltip(p_pos);
	}
	return String();
}

void FoldableContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			RID ci = get_canvas_item();
			Size2 size = get_size();
			title_panel_height = _get_title_panel_min_size().height;
			Ref<StyleBox> title_style = _get_title_style();
			real_t title_controls_width = _get_title_controls_width();

			Point2 title_ofs = (title_position == POSITION_TOP) ? Point2() : Point2(0, size.height - title_panel_height);
			Rect2 title_rect = Rect2(-title_ofs, Size2(size.width, title_panel_height));
			if (title_position == POSITION_BOTTOM) {
				draw_set_transform(Point2(0.0, title_style->get_draw_rect(title_rect).size.height), 0.0, Size2(1.0, -1.0));
			}
			title_style->draw(ci, title_rect);
			if (title_position == POSITION_BOTTOM) {
				draw_set_transform(Point2(), 0.0, Size2(1.0, 1.0));
			}

			Size2 title_ms = title_style->get_minimum_size();
			int title_width = size.width - title_ms.width;

			int h_separation = MAX(theme_cache.h_separation, 0);
			int title_style_ofs = (title_position == POSITION_TOP) ? title_style->get_margin(SIDE_TOP) : title_style->get_margin(SIDE_BOTTOM);
			Point2 title_pos;
			title_pos.y = MAX((title_panel_height - title_ms.height - text_buf->get_size().height) * 0.5, 0) + title_style_ofs;

			Ref<Texture2D> icon = _get_title_icon();

			title_width -= icon->get_width() + h_separation;
			Point2 icon_pos;
			icon_pos.x = title_style->get_margin(SIDE_LEFT);
			icon_pos.y = MAX((title_panel_height - title_ms.height - icon->get_height()) * 0.5, 0) + title_style_ofs;

			bool rtl = is_layout_rtl();
			if (rtl) {
				icon_pos.x = size.width - icon_pos.x - icon->get_width();
			} else {
				title_pos.x = title_style->get_margin(SIDE_LEFT) + icon->get_width() + h_separation;
			}
			icon->draw(ci, title_ofs + icon_pos);

			int offset = 0;
			title_width -= offset;
			if (rtl) {
				title_pos.x = offset + h_separation + title_controls_width;
			}

			Color font_color = folded ? theme_cache.title_collapsed_font_color : theme_cache.title_font_color;
			if (is_hovering) {
				font_color = theme_cache.title_hovered_font_color;
			}
			text_buf->set_width(title_width - title_controls_width);

			if (title_width > 0) {
				if (theme_cache.title_font_outline_size > 0 && theme_cache.title_font_outline_color.a > 0) {
					text_buf->draw_outline(ci, title_ofs + title_pos, theme_cache.title_font_outline_size, theme_cache.title_font_outline_color);
				}
				text_buf->draw(ci, title_ofs + title_pos, font_color);
			}

			if (!folded) {
				Rect2 panel_rect = Rect2(Point2(0, (title_position == POSITION_TOP) ? title_panel_height : 0), Size2(size.width, size.height - title_panel_height));
				if (title_position == POSITION_BOTTOM) {
					draw_set_transform(Point2(0.0, title_style->get_draw_rect(panel_rect).size.height), 0.0, Size2(1.0, -1.0));
				}
				theme_cache.panel_style->draw(ci, panel_rect);
				if (title_position == POSITION_BOTTOM) {
					draw_set_transform(Point2(), 0.0, Size2(1.0, 1.0));
				}
			}

			if (has_focus()) {
				Rect2 focus_rect = folded ? Rect2(-title_ofs, Size2(size.width, title_panel_height)) : Rect2(Point2(), size);
				if (title_position == POSITION_BOTTOM) {
					draw_set_transform(Point2(0.0, title_style->get_draw_rect(focus_rect).size.height), 0.0, Size2(1.0, -1.0));
				}
				theme_cache.focus_style->draw(ci, focus_rect);
				if (title_position == POSITION_BOTTOM) {
					draw_set_transform(Point2(), 0.0, Size2(1.0, 1.0));
				}
			}
		} break;

		case NOTIFICATION_SORT_CHILDREN: {
			bool rtl = is_layout_rtl();
			const Vector2 size = get_size();
			const Vector2 title_size = _get_title_panel_min_size();
			const Ref<StyleBox> title_style = _get_title_style();

			uint32_t title_count = title_controls.size();
			if (title_count > 0) {
				real_t offset = 0.0;
				if (rtl) {
					offset = title_style->get_margin(SIDE_LEFT);
				} else {
					offset = _get_title_controls_width();
					offset = size.x - title_style->get_margin(SIDE_RIGHT) - offset;
				}

				real_t v_center = title_size.y * 0.5;
				for (uint32_t i = 0; i < title_count; i++) {
					Control *control = title_controls[rtl ? title_count - i - 1 : i];
					if (!control->is_visible()) {
						continue;
					}
					Vector2 title_ms;
					Rect2 rect = Rect2(Vector2(), control->get_combined_minimum_size());
					rect.position.x = offset;
					rect.position.y = v_center - rect.size.y * 0.5;
					fit_child_in_rect(control, rect);

					offset += rect.size.x + theme_cache.h_separation;
				}
			}

			Rect2 inner_rect;
			inner_rect.position.x = rtl ? theme_cache.panel_style->get_margin(SIDE_RIGHT) : theme_cache.panel_style->get_margin(SIDE_LEFT);
			inner_rect.size.x = size.x - theme_cache.panel_style->get_margin(SIDE_LEFT) - theme_cache.panel_style->get_margin(SIDE_RIGHT);
			inner_rect.position.y = theme_cache.panel_style->get_margin(SIDE_TOP);

			inner_rect.size.y = size.y - theme_cache.panel_style->get_margin(SIDE_TOP) - theme_cache.panel_style->get_margin(SIDE_BOTTOM) - title_size.y;
			if (title_position == POSITION_TOP) {
				inner_rect.position.y += title_size.y;
			}

			for (int i = 0; i < get_child_count(false); i++) {
				Control *c = as_sortable_control(get_child(i, false), SortableVisibilityMode::IGNORE);
				if (!c) {
					continue;
				}
				c->set_visible(!folded);

				if (!folded) {
					fit_child_in_rect(c, inner_rect);
				}
			}
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			if (is_hovering) {
				is_hovering = false;
				queue_redraw();
			}
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			_shape();
			update_minimum_size();
			queue_redraw();
		} break;
	}
}

Size2 FoldableContainer::_get_title_panel_min_size() const {
	Ref<StyleBox> title_style = folded ? theme_cache.title_collapsed_style : theme_cache.title_style;
	Ref<Texture2D> icon = _get_title_icon();
	Size2 title_ms = title_style->get_minimum_size();

	Size2 final_size = title_ms;
	final_size.width += icon->get_width();

	if (!text.is_empty()) {
		final_size.width += MAX(0, theme_cache.h_separation);
		Size2 text_size = text_buf->get_size();
		final_size.height += MAX(text_size.height, icon->get_height());
		if (overrun_behavior == TextServer::OverrunBehavior::OVERRUN_NO_TRIMMING) {
			final_size.width += text_size.width;
		}
	} else {
		final_size.height += icon->get_height();
	}

	if (!title_controls.is_empty()) {
		real_t controls_height = 0;
		int visible_controls = 0;

		for (const Control *control : title_controls) {
			if (!control->is_visible()) {
				continue;
			}
			Vector2 size = control->get_combined_minimum_size();
			final_size.width += size.width;
			controls_height = MAX(controls_height, size.height);
			visible_controls++;
		}
		if (visible_controls > 0) {
			final_size.width += theme_cache.h_separation * visible_controls - 1;
		}
		final_size.height = MAX(final_size.height, title_ms.height + controls_height);
	}

	return final_size;
}

real_t FoldableContainer::_get_title_controls_width() const {
	real_t offset = 0.0;
	int visible_controls = 0;
	for (const Control *control : title_controls) {
		if (control->is_visible()) {
			offset += control->get_combined_minimum_size().x;
			visible_controls++;
		}
	}
	if (visible_controls > 0) {
		offset += theme_cache.h_separation * visible_controls - 1;
	}
	return offset;
}

Ref<StyleBox> FoldableContainer::_get_title_style() const {
	if (is_hovering) {
		return folded ? theme_cache.title_collapsed_hover_style : theme_cache.title_hover_style;
	}
	return folded ? theme_cache.title_collapsed_style : theme_cache.title_style;
}

Ref<Texture2D> FoldableContainer::_get_title_icon() const {
	if (!folded) {
		return (title_position == POSITION_TOP) ? theme_cache.expanded_arrow : theme_cache.expanded_arrow_mirrored;
	} else if (is_layout_rtl()) {
		return theme_cache.folded_arrow_mirrored;
	}
	return theme_cache.folded_arrow;
}

void FoldableContainer::_shape() {
	Ref<Font> font = theme_cache.title_font;
	int font_size = theme_cache.title_font_size;
	if (font.is_null() || font_size == 0) {
		return;
	}

	text_buf->clear();
	text_buf->set_width(-1);

	if (text_direction == TEXT_DIRECTION_INHERITED) {
		text_buf->set_direction(is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR);
	} else {
		text_buf->set_direction((TextServer::Direction)text_direction);
	}
	text_buf->set_horizontal_alignment(_get_actual_alignment());
	text_buf->set_text_overrun_behavior(overrun_behavior);
	text_buf->add_string(atr(text), font, font_size, language);
}

HorizontalAlignment FoldableContainer::_get_actual_alignment() const {
	if (is_layout_rtl()) {
		if (text_alignment == HORIZONTAL_ALIGNMENT_RIGHT) {
			return HORIZONTAL_ALIGNMENT_LEFT;
		} else if (text_alignment == HORIZONTAL_ALIGNMENT_LEFT) {
			return HORIZONTAL_ALIGNMENT_RIGHT;
		}
	}
	return text_alignment;
}

void FoldableContainer::_update_group() {
	foldable_group->updating_group = true;
	for (FoldableContainer *E : foldable_group->containers) {
		if (E == this) {
			continue;
		}

		E->set_folded(true);
	}
	foldable_group->updating_group = false;
}

void FoldableContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("fold"), &FoldableContainer::fold);
	ClassDB::bind_method(D_METHOD("expand"), &FoldableContainer::expand);
	ClassDB::bind_method(D_METHOD("set_folded", "folded"), &FoldableContainer::set_folded);
	ClassDB::bind_method(D_METHOD("is_folded"), &FoldableContainer::is_folded);
	ClassDB::bind_method(D_METHOD("set_foldable_group", "button_group"), &FoldableContainer::set_foldable_group);
	ClassDB::bind_method(D_METHOD("get_foldable_group"), &FoldableContainer::get_foldable_group);
	ClassDB::bind_method(D_METHOD("set_text", "text"), &FoldableContainer::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &FoldableContainer::get_text);
	ClassDB::bind_method(D_METHOD("set_title_alignment", "alignment"), &FoldableContainer::set_text_alignment);
	ClassDB::bind_method(D_METHOD("get_title_alignment"), &FoldableContainer::get_text_alignment);
	ClassDB::bind_method(D_METHOD("set_language", "language"), &FoldableContainer::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &FoldableContainer::get_language);
	ClassDB::bind_method(D_METHOD("set_text_direction", "text_direction"), &FoldableContainer::set_text_direction);
	ClassDB::bind_method(D_METHOD("get_text_direction"), &FoldableContainer::get_text_direction);
	ClassDB::bind_method(D_METHOD("set_text_overrun_behavior", "overrun_behavior"), &FoldableContainer::set_text_overrun_behavior);
	ClassDB::bind_method(D_METHOD("get_text_overrun_behavior"), &FoldableContainer::get_text_overrun_behavior);
	ClassDB::bind_method(D_METHOD("set_title_position", "title_position"), &FoldableContainer::set_title_position);
	ClassDB::bind_method(D_METHOD("get_title_position"), &FoldableContainer::get_title_position);
	ClassDB::bind_method(D_METHOD("add_title_bar_control", "control"), &FoldableContainer::add_title_bar_control);
	ClassDB::bind_method(D_METHOD("remove_title_bar_control", "control"), &FoldableContainer::remove_title_bar_control);

	ADD_SIGNAL(MethodInfo("folding_changed", PropertyInfo(Variant::BOOL, "is_folded")));

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "folded"), "set_folded", "is_folded");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text"), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "title_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right"), "set_title_alignment", "get_title_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "title_position", PROPERTY_HINT_ENUM, "Top,Bottom"), "set_title_position", "get_title_position");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_overrun_behavior", PROPERTY_HINT_ENUM, "Trim Nothing,Trim Characters,Trim Words,Ellipsis,Word Ellipsis"), "set_text_overrun_behavior", "get_text_overrun_behavior");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "foldable_group", PROPERTY_HINT_RESOURCE_TYPE, "FoldableGroup"), "set_foldable_group", "get_foldable_group");

	ADD_GROUP("BiDi", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_direction", PROPERTY_HINT_ENUM, "Auto,Left-to-Right,Right-to-Left,Inherited"), "set_text_direction", "get_text_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID), "set_language", "get_language");

	BIND_ENUM_CONSTANT(POSITION_TOP);
	BIND_ENUM_CONSTANT(POSITION_BOTTOM);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, FoldableContainer, title_style, "title_panel");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, FoldableContainer, title_hover_style, "title_hover_panel");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, FoldableContainer, title_collapsed_style, "title_collapsed_panel");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, FoldableContainer, title_collapsed_hover_style, "title_collapsed_hover_panel");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, FoldableContainer, focus_style, "focus");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, FoldableContainer, panel_style, "panel");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_FONT, FoldableContainer, title_font, "font");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_FONT_SIZE, FoldableContainer, title_font_size, "font_size");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_CONSTANT, FoldableContainer, title_font_outline_size, "outline_size");

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, FoldableContainer, title_font_color, "font_color");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, FoldableContainer, title_hovered_font_color, "hover_font_color");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, FoldableContainer, title_collapsed_font_color, "collapsed_font_color");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_COLOR, FoldableContainer, title_font_outline_color, "font_outline_color");

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, FoldableContainer, expanded_arrow);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, FoldableContainer, expanded_arrow_mirrored);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, FoldableContainer, folded_arrow);
	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, FoldableContainer, folded_arrow_mirrored);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, FoldableContainer, h_separation);
}

FoldableContainer::FoldableContainer(const String &p_text) {
	text_buf.instantiate();
	set_text(p_text);
	set_focus_mode(FOCUS_ALL);
	set_mouse_filter(MOUSE_FILTER_STOP);
}

FoldableContainer::~FoldableContainer() {
	if (foldable_group.is_valid()) {
		foldable_group->containers.erase(this);
	}
}

FoldableContainer *FoldableGroup::get_expanded_container() {
	for (FoldableContainer *container : containers) {
		if (!container->is_folded()) {
			return container;
		}
	}

	return nullptr;
}

void FoldableGroup::set_allow_folding_all(bool p_enabled) {
	allow_folding_all = p_enabled;
	if (!allow_folding_all && !get_expanded_container() && containers.size() > 0) {
		updating_group = true;
		for (FoldableContainer *E : containers) {
			E->set_folded(false);
			break;
		}
		updating_group = false;
	}
}

bool FoldableGroup::is_allow_folding_all() {
	return allow_folding_all;
}

void FoldableGroup::get_containers(List<FoldableContainer *> *r_containers) {
	for (FoldableContainer *E : containers) {
		r_containers->push_back(E);
	}
}

TypedArray<FoldableContainer> FoldableGroup::_get_containers() {
	TypedArray<FoldableContainer> foldable_containers;
	for (const FoldableContainer *E : containers) {
		foldable_containers.push_back(E);
	}

	return foldable_containers;
}

void FoldableGroup::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_expanded_container"), &FoldableGroup::get_expanded_container);
	ClassDB::bind_method(D_METHOD("get_containers"), &FoldableGroup::_get_containers);
	ClassDB::bind_method(D_METHOD("set_allow_folding_all", "enabled"), &FoldableGroup::set_allow_folding_all);
	ClassDB::bind_method(D_METHOD("is_allow_folding_all"), &FoldableGroup::is_allow_folding_all);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "allow_folding_all"), "set_allow_folding_all", "is_allow_folding_all");

	ADD_SIGNAL(MethodInfo("expanded", PropertyInfo(Variant::OBJECT, "container", PROPERTY_HINT_RESOURCE_TYPE, "FoldableContainer")));
}

FoldableGroup::FoldableGroup() {
	set_local_to_scene(true);
}
