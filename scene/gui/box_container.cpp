/**************************************************************************/
/*  box_container.cpp                                                     */
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

#include "box_container.h"

#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/theme/theme_db.h"

struct _MinSizeCache {
	int min_size = 0;
	bool will_stretch = false;
	int final_size = 0;
};

void BoxContainer::_resort() {
	/** First pass, determine minimum size AND amount of stretchable elements */

	Size2i new_size = get_size();

	bool rtl = is_layout_rtl();

	bool first = true;
	int children_count = 0;
	int stretch_min = 0;
	int stretch_avail = 0;
	float stretch_ratio_total = 0.0;
	HashMap<Control *, _MinSizeCache> min_size_cache;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i));
		if (!c) {
			continue;
		}

		Size2i size = c->get_combined_minimum_size();
		_MinSizeCache msc;

		if (vertical) { /* VERTICAL */
			stretch_min += size.height;
			msc.min_size = size.height;
			msc.will_stretch = c->get_v_size_flags().has_flag(SIZE_EXPAND);

		} else { /* HORIZONTAL */
			stretch_min += size.width;
			msc.min_size = size.width;
			msc.will_stretch = c->get_h_size_flags().has_flag(SIZE_EXPAND);
		}

		if (msc.will_stretch) {
			stretch_avail += msc.min_size;
			stretch_ratio_total += c->get_stretch_ratio();
		}
		msc.final_size = msc.min_size;
		min_size_cache[c] = msc;
		children_count++;
	}

	if (children_count == 0) {
		return;
	}

	int stretch_max = (vertical ? new_size.height : new_size.width) - (children_count - 1) * theme_cache.separation;
	int stretch_diff = stretch_max - stretch_min;
	if (stretch_diff < 0) {
		//avoid negative stretch space
		stretch_diff = 0;
	}

	stretch_avail += stretch_diff; //available stretch space.
	/** Second, pass successively to discard elements that can't be stretched, this will run while stretchable
		elements exist */

	bool has_stretched = false;
	while (stretch_ratio_total > 0) { // first of all, don't even be here if no stretchable objects exist

		has_stretched = true;
		bool refit_successful = true; //assume refit-test will go well
		float error = 0.0; // Keep track of accumulated error in pixels

		for (int i = 0; i < get_child_count(); i++) {
			Control *c = as_sortable_control(get_child(i));
			if (!c) {
				continue;
			}

			ERR_FAIL_COND(!min_size_cache.has(c));
			_MinSizeCache &msc = min_size_cache[c];

			if (msc.will_stretch) { //wants to stretch
				//let's see if it can really stretch
				float final_pixel_size = stretch_avail * c->get_stretch_ratio() / stretch_ratio_total;
				// Add leftover fractional pixels to error accumulator
				error += final_pixel_size - (int)final_pixel_size;
				if (final_pixel_size < msc.min_size) {
					//if available stretching area is too small for widget,
					//then remove it from stretching area
					msc.will_stretch = false;
					stretch_ratio_total -= c->get_stretch_ratio();
					refit_successful = false;
					stretch_avail -= msc.min_size;
					msc.final_size = msc.min_size;
					break;
				} else {
					msc.final_size = final_pixel_size;
					// Dump accumulated error if one pixel or more
					if (error >= 1) {
						msc.final_size += 1;
						error -= 1;
					}
				}
			}
		}

		if (refit_successful) { //uf refit went well, break
			break;
		}
	}

	/** Final pass, draw and stretch elements **/

	int ofs = 0;
	if (!has_stretched) {
		if (!vertical) {
			switch (alignment) {
				case ALIGNMENT_BEGIN:
					if (rtl) {
						ofs = stretch_diff;
					}
					break;
				case ALIGNMENT_CENTER:
					ofs = stretch_diff / 2;
					break;
				case ALIGNMENT_END:
					if (!rtl) {
						ofs = stretch_diff;
					}
					break;
			}
		} else {
			switch (alignment) {
				case ALIGNMENT_BEGIN:
					break;
				case ALIGNMENT_CENTER:
					ofs = stretch_diff / 2;
					break;
				case ALIGNMENT_END:
					ofs = stretch_diff;
					break;
			}
		}
	}

	first = true;
	int idx = 0;

	int start;
	int end;
	int delta;
	if (!rtl || vertical) {
		start = 0;
		end = get_child_count();
		delta = +1;
	} else {
		start = get_child_count() - 1;
		end = -1;
		delta = -1;
	}

	for (int i = start; i != end; i += delta) {
		Control *c = as_sortable_control(get_child(i));
		if (!c) {
			continue;
		}

		_MinSizeCache &msc = min_size_cache[c];

		if (first) {
			first = false;
		} else {
			ofs += theme_cache.separation;
		}

		int from = ofs;
		int to = ofs + msc.final_size;

		if (msc.will_stretch && idx == children_count - 1) {
			//adjust so the last one always fits perfect
			//compensating for numerical imprecision

			to = vertical ? new_size.height : new_size.width;
		}

		int size = to - from;

		Rect2 rect;

		if (vertical) {
			rect = Rect2(0, from, new_size.width, size);
		} else {
			rect = Rect2(from, 0, size, new_size.height);
		}

		fit_child_in_rect(c, rect);

		ofs = to;
		idx++;
	}
}

Size2 BoxContainer::get_minimum_size() const {
	/* Calculate MINIMUM SIZE */

	Size2i minimum;

	bool first = true;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i), SortableVisibilityMode::VISIBLE);
		if (!c) {
			continue;
		}

		Size2i size = c->get_combined_minimum_size();

		if (vertical) { /* VERTICAL */

			if (size.width > minimum.width) {
				minimum.width = size.width;
			}

			minimum.height += size.height + (first ? 0 : theme_cache.separation);

		} else { /* HORIZONTAL */

			if (size.height > minimum.height) {
				minimum.height = size.height;
			}

			minimum.width += size.width + (first ? 0 : theme_cache.separation);
		}

		first = false;
	}

	return minimum;
}

void BoxContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			_resort();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			update_minimum_size();
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			queue_sort();
		} break;
	}
}

void BoxContainer::_validate_property(PropertyInfo &p_property) const {
	if (is_fixed && p_property.name == "vertical") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

void BoxContainer::set_alignment(AlignmentMode p_alignment) {
	if (alignment == p_alignment) {
		return;
	}
	alignment = p_alignment;
	_resort();
}

BoxContainer::AlignmentMode BoxContainer::get_alignment() const {
	return alignment;
}

void BoxContainer::set_vertical(bool p_vertical) {
	ERR_FAIL_COND_MSG(is_fixed, "Can't change orientation of " + get_class() + ".");
	vertical = p_vertical;
	update_minimum_size();
	_resort();
}

bool BoxContainer::is_vertical() const {
	return vertical;
}

Control *BoxContainer::add_spacer(bool p_begin) {
	Control *c = memnew(Control);
	c->set_mouse_filter(MOUSE_FILTER_PASS); //allow spacer to pass mouse events

	if (vertical) {
		c->set_v_size_flags(SIZE_EXPAND_FILL);
	} else {
		c->set_h_size_flags(SIZE_EXPAND_FILL);
	}

	add_child(c);
	if (p_begin) {
		move_child(c, 0);
	}

	return c;
}

Vector<int> BoxContainer::get_allowed_size_flags_horizontal() const {
	Vector<int> flags;
	flags.append(SIZE_FILL);
	if (!vertical) {
		flags.append(SIZE_EXPAND);
	}
	flags.append(SIZE_SHRINK_BEGIN);
	flags.append(SIZE_SHRINK_CENTER);
	flags.append(SIZE_SHRINK_END);
	return flags;
}

Vector<int> BoxContainer::get_allowed_size_flags_vertical() const {
	Vector<int> flags;
	flags.append(SIZE_FILL);
	if (vertical) {
		flags.append(SIZE_EXPAND);
	}
	flags.append(SIZE_SHRINK_BEGIN);
	flags.append(SIZE_SHRINK_CENTER);
	flags.append(SIZE_SHRINK_END);
	return flags;
}

BoxContainer::BoxContainer(bool p_vertical) {
	vertical = p_vertical;
}

void BoxContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_spacer", "begin"), &BoxContainer::add_spacer);
	ClassDB::bind_method(D_METHOD("set_alignment", "alignment"), &BoxContainer::set_alignment);
	ClassDB::bind_method(D_METHOD("get_alignment"), &BoxContainer::get_alignment);
	ClassDB::bind_method(D_METHOD("set_vertical", "vertical"), &BoxContainer::set_vertical);
	ClassDB::bind_method(D_METHOD("is_vertical"), &BoxContainer::is_vertical);

	BIND_ENUM_CONSTANT(ALIGNMENT_BEGIN);
	BIND_ENUM_CONSTANT(ALIGNMENT_CENTER);
	BIND_ENUM_CONSTANT(ALIGNMENT_END);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "alignment", PROPERTY_HINT_ENUM, "Begin,Center,End"), "set_alignment", "get_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "vertical"), "set_vertical", "is_vertical");

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, BoxContainer, separation);
}

MarginContainer *VBoxContainer::add_margin_child(const String &p_label, Control *p_control, bool p_expand) {
	Label *l = memnew(Label);
	l->set_theme_type_variation("HeaderSmall");
	l->set_text(p_label);
	add_child(l);
	MarginContainer *mc = memnew(MarginContainer);
	mc->add_theme_constant_override("margin_left", 0);
	mc->add_child(p_control, true);
	add_child(mc);
	if (p_expand) {
		mc->set_v_size_flags(SIZE_EXPAND_FILL);
	}

	return mc;
}
