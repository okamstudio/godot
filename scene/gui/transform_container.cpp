/**************************************************************************/
/*  transform_container.cpp                                               */
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

#include "transform_container.h"

void TransformContainer::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			const Rect2 rect(Vector2(), get_size());

			for (int i = 0; i < get_child_count(); i++) {
				Control *c = as_sortable_control(get_child(i));
				if (!c) {
					continue;
				}
				fit_child_in_rect(c, rect);

				Vector2 absolute_visual_offset = visual_offset;
				Vector2 absolute_visual_pivot = visual_pivot;

				if (visual_offset_relative_to_size) {
					absolute_visual_offset *= c->get_size();
				}

				if (visual_pivot_relative_to_size) {
					absolute_visual_pivot *= c->get_size();
				}

				const Transform2D xform = c->get_transform()
												  .translated(-absolute_visual_pivot)
												  .rotated(visual_rotation)
												  .scaled(visual_scale)
												  .translated(absolute_visual_pivot)
												  .translated(absolute_visual_offset);

				RenderingServer::get_singleton()->canvas_item_set_transform(c->get_canvas_item(), xform);
			}
		}
	}
}

void TransformContainer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_visual_offset", "offset"), &TransformContainer::set_visual_offset);
	ClassDB::bind_method(D_METHOD("get_visual_offset"), &TransformContainer::get_visual_offset);

	ClassDB::bind_method(D_METHOD("set_visual_offset_relative_to_size", "relative"), &TransformContainer::set_visual_offset_relative_to_size);
	ClassDB::bind_method(D_METHOD("is_visual_offset_relative_to_size"), &TransformContainer::is_visual_offset_relative_to_size);

	ClassDB::bind_method(D_METHOD("set_visual_scale", "scale"), &TransformContainer::set_visual_scale);
	ClassDB::bind_method(D_METHOD("get_visual_scale"), &TransformContainer::get_visual_scale);

	ClassDB::bind_method(D_METHOD("set_visual_rotation", "rotation"), &TransformContainer::set_visual_rotation);
	ClassDB::bind_method(D_METHOD("get_visual_rotation"), &TransformContainer::get_visual_rotation);

	ClassDB::bind_method(D_METHOD("set_visual_pivot", "pivot"), &TransformContainer::set_visual_pivot);
	ClassDB::bind_method(D_METHOD("get_visual_pivot"), &TransformContainer::get_visual_pivot);

	ClassDB::bind_method(D_METHOD("set_visual_pivot_relative_to_size", "relative"), &TransformContainer::set_visual_pivot_relative_to_size);
	ClassDB::bind_method(D_METHOD("is_visual_pivot_relative_to_size"), &TransformContainer::is_visual_pivot_relative_to_size);

	ADD_GROUP("Visual", "visual_");

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "visual_offset"), "set_visual_offset", "get_visual_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visual_offset_relative_to_size"), "set_visual_offset_relative_to_size", "is_visual_offset_relative_to_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "visual_scale", PROPERTY_HINT_LINK), "set_visual_scale", "get_visual_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "visual_rotation", PROPERTY_HINT_RANGE, "-360,360,0.1,or_less,or_greater,radians_as_degrees"), "set_visual_rotation", "get_visual_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "visual_pivot"), "set_visual_pivot", "get_visual_pivot");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visual_pivot_relative_to_size"), "set_visual_pivot_relative_to_size", "is_visual_pivot_relative_to_size");
}

Size2 TransformContainer::get_minimum_size() const {
	Size2 ms;
	for (int i = 0; i < get_child_count(); i++) {
		Control *c = as_sortable_control(get_child(i), SortableVisbilityMode::VISIBLE);
		if (!c) {
			continue;
		}
		Size2 minsize = c->get_combined_minimum_size();
		ms = ms.max(minsize);
	}
	return ms;
}

Vector<int> TransformContainer::get_allowed_size_flags_horizontal() const {
	return {
		SIZE_FILL,
		SIZE_SHRINK_BEGIN,
		SIZE_SHRINK_CENTER,
		SIZE_SHRINK_END,
	};
}

Vector<int> TransformContainer::get_allowed_size_flags_vertical() const {
	return {
		SIZE_FILL,
		SIZE_SHRINK_BEGIN,
		SIZE_SHRINK_CENTER,
		SIZE_SHRINK_END,
	};
}

void TransformContainer::set_visual_offset(const Vector2 &p_offset) {
	if (visual_offset == p_offset) {
		return;
	}
	visual_offset = p_offset;
	queue_sort();
}

Vector2 TransformContainer::get_visual_offset() const {
	return visual_offset;
}

void TransformContainer::set_visual_offset_relative_to_size(bool p_relative) {
	if (visual_offset_relative_to_size == p_relative) {
		return;
	}
	visual_offset_relative_to_size = p_relative;
	queue_sort();
}

bool TransformContainer::is_visual_offset_relative_to_size() const {
	return visual_offset_relative_to_size;
}

void TransformContainer::set_visual_scale(const Vector2 &p_scale) {
	if (visual_scale == p_scale) {
		return;
	}
	visual_scale = p_scale;
	queue_sort();
}

Vector2 TransformContainer::get_visual_scale() const {
	return visual_scale;
}

void TransformContainer::set_visual_rotation(real_t p_rotation) {
	if (visual_rotation == p_rotation) {
		return;
	}
	visual_rotation = p_rotation;
	queue_sort();
}

real_t TransformContainer::get_visual_rotation() const {
	return visual_rotation;
}

void TransformContainer::set_visual_pivot(const Vector2 &p_pivot) {
	if (visual_pivot == p_pivot) {
		return;
	}
	visual_pivot = p_pivot;
	queue_sort();
}

Vector2 TransformContainer::get_visual_pivot() const {
	return visual_pivot;
}

void TransformContainer::set_visual_pivot_relative_to_size(bool p_relative) {
	if (visual_pivot_relative_to_size == p_relative) {
		return;
	}
	visual_pivot_relative_to_size = p_relative;
	queue_sort();
}

bool TransformContainer::is_visual_pivot_relative_to_size() const {
	return visual_pivot_relative_to_size;
}
