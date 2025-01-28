/**************************************************************************/
/*  flow_container.cpp                                                    */
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

#include "flow_container.h"

#include "scene/gui/texture_rect.h"
#include "scene/theme/theme_db.h"

struct _LineData {
	int child_count = 0;
	int min_line_height = 0;
	int min_line_length = 0;
	int stretch_avail = 0;
	float stretch_ratio_total = 0;
	bool is_filled = false;
};

void FlowContainer::_resort() {
	// Avoid resorting if invisible.
	if (!is_visible_in_tree()) {
		return;
	}

	bool rtl = is_layout_rtl();

	HashMap<Control *, Size2i> children_minsize_cache;

	Vector<_LineData> lines_data;

	Vector2i ofs;
	int line_height = 0;
	int line_length = 0;
	float line_stretch_ratio_total = 0;
	int current_container_size = vertical ? get_size().y : get_size().x;
	int children_in_current_line = 0;
	Control *last_child = nullptr;

	// First pass for line wrapping and minimum size calculation.
	for (int i = 0; i < get_child_count(); i++) {
		Control *child = as_sortable_control(get_child(i));
		if (!child) {
			continue;
		}

		Size2i child_msc = child->get_combined_minimum_size();

		if (vertical) { /* VERTICAL */
			if (children_in_current_line > 0) {
				ofs.y += theme_cache.v_separation;
			}
			if (ofs.y + child_msc.y > current_container_size) {
				line_length = ofs.y - theme_cache.v_separation;
				lines_data.push_back(_LineData{ children_in_current_line, line_height, line_length, current_container_size - line_length, line_stretch_ratio_total, true });

				// Move in new column (vertical line).
				ofs.x += line_height + theme_cache.h_separation;
				ofs.y = 0;
				line_height = 0;
				line_stretch_ratio_total = 0;
				children_in_current_line = 0;
			}

			line_height = MAX(line_height, child_msc.x);
			if (child->get_v_size_flags().has_flag(SIZE_EXPAND)) {
				line_stretch_ratio_total += child->get_stretch_ratio();
			}
			ofs.y += child_msc.y;

		} else { /* HORIZONTAL */
			if (children_in_current_line > 0) {
				ofs.x += theme_cache.h_separation;
			}
			if (ofs.x + child_msc.x > current_container_size) {
				line_length = ofs.x - theme_cache.h_separation;
				lines_data.push_back(_LineData{ children_in_current_line, line_height, line_length, current_container_size - line_length, line_stretch_ratio_total, true });

				// Move in new line.
				ofs.y += line_height + theme_cache.v_separation;
				ofs.x = 0;
				line_height = 0;
				line_stretch_ratio_total = 0;
				children_in_current_line = 0;
			}

			line_height = MAX(line_height, child_msc.y);
			if (child->get_h_size_flags().has_flag(SIZE_EXPAND)) {
				line_stretch_ratio_total += child->get_stretch_ratio();
			}
			ofs.x += child_msc.x;
		}

		last_child = child;
		children_minsize_cache[child] = child_msc;
		children_in_current_line++;
	}
	line_length = vertical ? (ofs.y) : (ofs.x);
	bool is_filled = false;
	if (last_child != nullptr) {
		is_filled = vertical ? (ofs.y + last_child->get_combined_minimum_size().y > current_container_size ? true : false) : (ofs.x + last_child->get_combined_minimum_size().x > current_container_size ? true : false);
	}
	lines_data.push_back(_LineData{ children_in_current_line, line_height, line_length, current_container_size - line_length, line_stretch_ratio_total, is_filled });

	// Second pass for in-line expansion and alignment.

	int current_line_idx = 0;
	int child_idx_in_line = 0;

	ofs.x = 0;
	ofs.y = 0;

	for (int i = 0; i < get_child_count(); i++) {
		Control *child = as_sortable_control(get_child(i));
		if (!child) {
			continue;
		}
		Size2i child_size = children_minsize_cache[child];

		_LineData line_data = lines_data[current_line_idx];
		if (child_idx_in_line >= lines_data[current_line_idx].child_count) {
			current_line_idx++;
			child_idx_in_line = 0;
			if (vertical) {
				ofs.x += line_data.min_line_height + theme_cache.h_separation;
				ofs.y = 0;
			} else {
				ofs.x = 0;
				ofs.y += line_data.min_line_height + theme_cache.v_separation;
			}
			line_data = lines_data[current_line_idx];
		}

		// The first child of each line adds the offset caused by the alignment,
		// but only if the line doesn't contain a child that expands.
		if (child_idx_in_line == 0 && Math::is_equal_approx(line_data.stretch_ratio_total, 0)) {
			int alignment_ofs = 0;
			bool is_not_first_line_and_not_filled = current_line_idx != 0 && !line_data.is_filled;
			float prior_stretch_avail = is_not_first_line_and_not_filled ? lines_data[current_line_idx - 1].stretch_avail : 0.0;
			switch (alignment) {
				case ALIGNMENT_BEGIN: {
					if (last_wrap_alignment != LAST_WRAP_ALIGNMENT_INHERIT && is_not_first_line_and_not_filled) {
						if (last_wrap_alignment == LAST_WRAP_ALIGNMENT_END) {
							alignment_ofs = line_data.stretch_avail - prior_stretch_avail;
						} else if (last_wrap_alignment == LAST_WRAP_ALIGNMENT_CENTER) {
							alignment_ofs = (line_data.stretch_avail - prior_stretch_avail) * 0.5;
						}
					}
				} break;
				case ALIGNMENT_CENTER: {
					if (last_wrap_alignment != LAST_WRAP_ALIGNMENT_INHERIT && last_wrap_alignment != LAST_WRAP_ALIGNMENT_CENTER && is_not_first_line_and_not_filled) {
						if (last_wrap_alignment == LAST_WRAP_ALIGNMENT_END) {
							alignment_ofs = line_data.stretch_avail - (prior_stretch_avail * 0.5);
						} else { // Is LAST_WRAP_ALIGNMENT_BEGIN
							alignment_ofs = prior_stretch_avail * 0.5;
						}
					} else {
						alignment_ofs = line_data.stretch_avail * 0.5;
					}
				} break;
				case ALIGNMENT_END: {
					if (last_wrap_alignment != LAST_WRAP_ALIGNMENT_INHERIT && last_wrap_alignment != LAST_WRAP_ALIGNMENT_END && is_not_first_line_and_not_filled) {
						if (last_wrap_alignment == LAST_WRAP_ALIGNMENT_BEGIN) {
							alignment_ofs = prior_stretch_avail;
						} else { // Is LAST_WRAP_ALIGNMENT_CENTER
							alignment_ofs = prior_stretch_avail + (line_data.stretch_avail - prior_stretch_avail) * 0.5;
						}
					} else {
						alignment_ofs = line_data.stretch_avail;
					}
				} break;
				default:
					break;
			}
			if (vertical) { /* VERTICAL */
				ofs.y += alignment_ofs;
			} else { /* HORIZONTAL */
				ofs.x += alignment_ofs;
			}
		}

		bool is_unsupported_texture_rect = false;
		if (lines_data.size() > 1) {
			TextureRect *trect = Object::cast_to<TextureRect>(child);
			if (trect) {
				TextureRect::ExpandMode mode = trect->get_expand_mode();
				if (mode == TextureRect::EXPAND_FIT_WIDTH || mode == TextureRect::EXPAND_FIT_WIDTH_PROPORTIONAL ||
						mode == TextureRect::EXPAND_FIT_HEIGHT || mode == TextureRect::EXPAND_FIT_HEIGHT_PROPORTIONAL) {
					is_unsupported_texture_rect = true;
				}
			}
		}

		if (is_unsupported_texture_rect) {
			// Temporary fix for editor crash. Changing size of TextureRect with EXPAND_FIT_* ExpandModes can lead to infinite loop if child items are moved between lines.
			WARN_PRINT_ONCE("TextureRects with Fit Expand Modes are currently not supported inside FlowContainers with multiple lines");
			child_size = child->get_size();
		} else if (vertical) { /* VERTICAL */
			if (child->get_h_size_flags().has_flag(SIZE_FILL) || child->get_h_size_flags().has_flag(SIZE_SHRINK_CENTER) || child->get_h_size_flags().has_flag(SIZE_SHRINK_END)) {
				child_size.width = line_data.min_line_height;
			}

			if (child->get_v_size_flags().has_flag(SIZE_EXPAND)) {
				int stretch = line_data.stretch_avail * child->get_stretch_ratio() / line_data.stretch_ratio_total;
				child_size.height += stretch;
			}

		} else { /* HORIZONTAL */
			if (child->get_v_size_flags().has_flag(SIZE_FILL) || child->get_v_size_flags().has_flag(SIZE_SHRINK_CENTER) || child->get_v_size_flags().has_flag(SIZE_SHRINK_END)) {
				child_size.height = line_data.min_line_height;
			}

			if (child->get_h_size_flags().has_flag(SIZE_EXPAND)) {
				int stretch = line_data.stretch_avail * child->get_stretch_ratio() / line_data.stretch_ratio_total;
				child_size.width += stretch;
			}
		}

		Rect2 child_rect = Rect2(ofs, child_size);
		if (reverse_fill && !vertical) {
			child_rect.position.y = get_rect().size.y - child_rect.position.y - child_rect.size.height;
		}
		if ((rtl && !vertical) || ((rtl != reverse_fill) && vertical)) {
			child_rect.position.x = get_rect().size.x - child_rect.position.x - child_rect.size.width;
		}

		fit_child_in_rect(child, child_rect);

		if (vertical) { /* VERTICAL */
			ofs.y += child_size.height + theme_cache.v_separation;
		} else { /* HORIZONTAL */
			ofs.x += child_size.width + theme_cache.h_separation;
		}

		child_idx_in_line++;
	}
	cached_size = (vertical ? ofs.x : ofs.y) + line_height;
	cached_line_count = lines_data.size();
	cached_line_max_child_count = lines_data.size() > 0 ? lines_data[0].child_count : 0;
}

Size2 FlowContainer::get_minimum_size() const {
	Size2i minimum;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i), SortableVisibilityMode::VISIBLE);
		if (!c) {
			continue;
		}

		Size2i size = c->get_combined_minimum_size();

		if (vertical) { /* VERTICAL */
			minimum.height = MAX(minimum.height, size.height);
			minimum.width = cached_size;

		} else { /* HORIZONTAL */
			minimum.width = MAX(minimum.width, size.width);
			minimum.height = cached_size;
		}
	}

	return minimum;
}

Vector<int> FlowContainer::get_allowed_size_flags_horizontal() const {
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

Vector<int> FlowContainer::get_allowed_size_flags_vertical() const {
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

void FlowContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			_resort();
			update_minimum_size();
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

void FlowContainer::_validate_property(PropertyInfo &p_property) const {
	if (is_fixed && p_property.name == "vertical") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

int FlowContainer::get_line_count() const {
	return cached_line_count;
}

int FlowContainer::get_line_max_child_count() const {
	return cached_line_max_child_count;
}

void FlowContainer::set_alignment(AlignmentMode p_alignment) {
	if (alignment == p_alignment) {
		return;
	}
	alignment = p_alignment;
	_resort();
}

FlowContainer::AlignmentMode FlowContainer::get_alignment() const {
	return alignment;
}

void FlowContainer::set_last_wrap_alignment(LastWrapAlignmentMode p_last_wrap_alignment) {
	if (last_wrap_alignment == p_last_wrap_alignment) {
		return;
	}
	last_wrap_alignment = p_last_wrap_alignment;
	_resort();
}

FlowContainer::LastWrapAlignmentMode FlowContainer::get_last_wrap_alignment() const {
	return last_wrap_alignment;
}

void FlowContainer::set_vertical(bool p_vertical) {
	ERR_FAIL_COND_MSG(is_fixed, "Can't change orientation of " + get_class() + ".");
	vertical = p_vertical;
	update_minimum_size();
	_resort();
}

bool FlowContainer::is_vertical() const {
	return vertical;
}

void FlowContainer::set_reverse_fill(bool p_reverse_fill) {
	if (reverse_fill == p_reverse_fill) {
		return;
	}
	reverse_fill = p_reverse_fill;
	_resort();
}

bool FlowContainer::is_reverse_fill() const {
	return reverse_fill;
}

FlowContainer::FlowContainer(bool p_vertical) {
	vertical = p_vertical;
}

void FlowContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_line_count"), &FlowContainer::get_line_count);

	ClassDB::bind_method(D_METHOD("set_alignment", "alignment"), &FlowContainer::set_alignment);
	ClassDB::bind_method(D_METHOD("get_alignment"), &FlowContainer::get_alignment);
	ClassDB::bind_method(D_METHOD("set_last_wrap_alignment", "last_wrap_alignment"), &FlowContainer::set_last_wrap_alignment);
	ClassDB::bind_method(D_METHOD("get_last_wrap_alignment"), &FlowContainer::get_last_wrap_alignment);
	ClassDB::bind_method(D_METHOD("set_vertical", "vertical"), &FlowContainer::set_vertical);
	ClassDB::bind_method(D_METHOD("is_vertical"), &FlowContainer::is_vertical);
	ClassDB::bind_method(D_METHOD("set_reverse_fill", "reverse_fill"), &FlowContainer::set_reverse_fill);
	ClassDB::bind_method(D_METHOD("is_reverse_fill"), &FlowContainer::is_reverse_fill);

	BIND_ENUM_CONSTANT(ALIGNMENT_BEGIN);
	BIND_ENUM_CONSTANT(ALIGNMENT_CENTER);
	BIND_ENUM_CONSTANT(ALIGNMENT_END);
	BIND_ENUM_CONSTANT(LAST_WRAP_ALIGNMENT_INHERIT);
	BIND_ENUM_CONSTANT(LAST_WRAP_ALIGNMENT_BEGIN);
	BIND_ENUM_CONSTANT(LAST_WRAP_ALIGNMENT_CENTER);
	BIND_ENUM_CONSTANT(LAST_WRAP_ALIGNMENT_END);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "alignment", PROPERTY_HINT_ENUM, "Begin,Center,End"), "set_alignment", "get_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "last_wrap_alignment", PROPERTY_HINT_ENUM, "Inherit,Begin,Center,End"), "set_last_wrap_alignment", "get_last_wrap_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "vertical"), "set_vertical", "is_vertical");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "reverse_fill"), "set_reverse_fill", "is_reverse_fill");

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, FlowContainer, h_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, FlowContainer, v_separation);
}
