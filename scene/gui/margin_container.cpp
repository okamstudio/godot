/**************************************************************************/
/*  margin_container.cpp                                                  */
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

#include "margin_container.h"

#include "scene/theme/theme_db.h"

Size2 MarginContainer::get_minimum_size() const {
	Size2 max;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i), SortableVisibilityMode::VISIBLE);
		if (!c) {
			continue;
		}

		Size2 s = c->get_combined_minimum_size();
		if (s.width > max.width) {
			max.width = s.width;
		}
		if (s.height > max.height) {
			max.height = s.height;
		}
	}

	max.width += (theme_cache.margin_left + theme_cache.margin_right);
	max.height += (theme_cache.margin_top + theme_cache.margin_bottom);

	return max;
}

Vector<int> MarginContainer::get_allowed_size_flags_horizontal() const {
	Vector<int> flags;
	flags.append(SIZE_FILL);
	flags.append(SIZE_SHRINK_BEGIN);
	flags.append(SIZE_SHRINK_CENTER);
	flags.append(SIZE_SHRINK_END);
	return flags;
}

Vector<int> MarginContainer::get_allowed_size_flags_vertical() const {
	Vector<int> flags;
	flags.append(SIZE_FILL);
	flags.append(SIZE_SHRINK_BEGIN);
	flags.append(SIZE_SHRINK_CENTER);
	flags.append(SIZE_SHRINK_END);
	return flags;
}

int MarginContainer::get_margin_size(Side p_side) const {
	ERR_FAIL_INDEX_V((int)p_side, 4, 0);

	switch (p_side) {
		case SIDE_LEFT:
			return theme_cache.margin_left;
		case SIDE_RIGHT:
			return theme_cache.margin_right;
		case SIDE_TOP:
			return theme_cache.margin_top;
		case SIDE_BOTTOM:
			return theme_cache.margin_bottom;
	}

	return 0;
}

void MarginContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			Size2 s = get_size();

			for (int i = 0; i < get_child_count(); i++) {
				Control *c = as_sortable_control(get_child(i));
				if (!c) {
					continue;
				}

				int w = s.width - theme_cache.margin_left - theme_cache.margin_right;
				int h = s.height - theme_cache.margin_top - theme_cache.margin_bottom;
				fit_child_in_rect(c, Rect2(theme_cache.margin_left, theme_cache.margin_top, w, h));
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			update_minimum_size();
		} break;
	}
}

void MarginContainer::_bind_methods() {
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, MarginContainer, margin_left);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, MarginContainer, margin_top);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, MarginContainer, margin_right);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, MarginContainer, margin_bottom);
}

MarginContainer::MarginContainer() {
}
