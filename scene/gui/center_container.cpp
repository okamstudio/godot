/**************************************************************************/
/*  center_container.cpp                                                  */
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

#include "center_container.h"

Size2 CenterContainer::get_minimum_size() const {
	if (use_top_left) {
		return Size2();
	}
	Size2 ms;
	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i), SortableVisibilityMode::VISIBLE);
		if (!c) {
			continue;
		}
		Size2 minsize = c->get_combined_minimum_size();
		ms = ms.max(minsize);
	}

	return ms;
}

void CenterContainer::set_use_top_left(bool p_enable) {
	if (use_top_left == p_enable) {
		return;
	}

	use_top_left = p_enable;

	update_minimum_size();
	queue_sort();
}

bool CenterContainer::is_using_top_left() const {
	return use_top_left;
}

Vector<int> CenterContainer::get_allowed_size_flags_horizontal() const {
	return Vector<int>();
}

Vector<int> CenterContainer::get_allowed_size_flags_vertical() const {
	return Vector<int>();
}

void CenterContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			Size2 size = get_size();
			for (int i = 0; i < get_child_count(); i++) {
				Control *c = as_sortable_control(get_child(i));
				if (!c) {
					continue;
				}
				Size2 minsize = c->get_combined_minimum_size();
				Point2 ofs = use_top_left ? (-minsize * 0.5).floor() : ((size - minsize) / 2.0).floor();
				fit_child_in_rect(c, Rect2(ofs, minsize));
			}
		} break;
	}
}

void CenterContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_use_top_left", "enable"), &CenterContainer::set_use_top_left);
	ClassDB::bind_method(D_METHOD("is_using_top_left"), &CenterContainer::is_using_top_left);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_top_left"), "set_use_top_left", "is_using_top_left");
}

CenterContainer::CenterContainer() {}
