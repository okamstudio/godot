/**************************************************************************/
/*  container.cpp                                                         */
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

#include "container.h"

void Container::_child_minsize_changed() {
	update_minimum_size();
	queue_sort();
}

void Container::add_child_notify(Node *p_child) {
	Control::add_child_notify(p_child);

	Control *control = Object::cast_to<Control>(p_child);
	if (!control) {
		return;
	}

	control->connect(SceneStringName(size_flags_changed), callable_mp(this, &Container::queue_sort));
	control->connect(SceneStringName(minimum_size_changed), callable_mp(this, &Container::_child_minsize_changed));
	control->connect(SceneStringName(visibility_changed), callable_mp(this, &Container::_child_minsize_changed));

	update_minimum_size();
	queue_sort();
}

void Container::move_child_notify(Node *p_child) {
	Control::move_child_notify(p_child);

	if (!Object::cast_to<Control>(p_child)) {
		return;
	}

	update_minimum_size();
	queue_sort();
}

void Container::remove_child_notify(Node *p_child) {
	Control::remove_child_notify(p_child);

	Control *control = Object::cast_to<Control>(p_child);
	if (!control) {
		return;
	}

	control->disconnect(SceneStringName(size_flags_changed), callable_mp(this, &Container::queue_sort));
	control->disconnect(SceneStringName(minimum_size_changed), callable_mp(this, &Container::_child_minsize_changed));
	control->disconnect(SceneStringName(visibility_changed), callable_mp(this, &Container::_child_minsize_changed));

	update_minimum_size();
	queue_sort();
}

void Container::_sort_children() {
	if (!is_inside_tree()) {
		pending_sort = false;
		return;
	}

	notification(NOTIFICATION_PRE_SORT_CHILDREN);
	emit_signal(SceneStringName(pre_sort_children));

	notification(NOTIFICATION_SORT_CHILDREN);
	emit_signal(SceneStringName(sort_children));
	pending_sort = false;
}

void Container::fit_child_in_rect(Control *p_child, const Rect2 &p_rect) {
	ERR_FAIL_NULL(p_child);
	ERR_FAIL_COND(p_child->get_parent() != this);

	bool rtl = is_layout_rtl();
	Size2 minsize = p_child->get_combined_minimum_size();
	Rect2 r = p_rect;

	if (!(p_child->get_h_size_flags().has_flag(SIZE_FILL))) {
		r.size.x = minsize.width;
		if (p_child->get_h_size_flags().has_flag(SIZE_SHRINK_END)) {
			r.position.x += rtl ? 0 : (p_rect.size.width - minsize.width);
		} else if (p_child->get_h_size_flags().has_flag(SIZE_SHRINK_CENTER)) {
			r.position.x += Math::floor((p_rect.size.x - minsize.width) / 2);
		} else {
			r.position.x += rtl ? (p_rect.size.width - minsize.width) : 0;
		}
	}

	if (!(p_child->get_v_size_flags().has_flag(SIZE_FILL))) {
		r.size.y = minsize.y;
		if (p_child->get_v_size_flags().has_flag(SIZE_SHRINK_END)) {
			r.position.y += p_rect.size.height - minsize.height;
		} else if (p_child->get_v_size_flags().has_flag(SIZE_SHRINK_CENTER)) {
			r.position.y += Math::floor((p_rect.size.y - minsize.height) / 2);
		} else {
			r.position.y += 0;
		}
	}

	p_child->set_rect(r);
	p_child->set_rotation(0);
	p_child->set_scale(Vector2(1, 1));
}

void Container::queue_sort() {
	if (!is_inside_tree()) {
		return;
	}

	if (pending_sort) {
		return;
	}

	callable_mp(this, &Container::_sort_children).call_deferred();
	pending_sort = true;
}

Control *Container::as_sortable_control(Node *p_node, SortableVisibilityMode p_visibility_mode) const {
	Control *c = Object::cast_to<Control>(p_node);
	if (!c || c->is_set_as_top_level()) {
		return nullptr;
	}
	if (p_visibility_mode == SortableVisibilityMode::VISIBLE && !c->is_visible()) {
		return nullptr;
	}
	if (p_visibility_mode == SortableVisibilityMode::VISIBLE_IN_TREE && !c->is_visible_in_tree()) {
		return nullptr;
	}
	return c;
}

Vector<int> Container::get_allowed_size_flags_horizontal() const {
	Vector<int> flags;
	if (GDVIRTUAL_CALL(_get_allowed_size_flags_horizontal, flags)) {
		return flags;
	}

	flags.append(SIZE_FILL);
	flags.append(SIZE_EXPAND);
	flags.append(SIZE_SHRINK_BEGIN);
	flags.append(SIZE_SHRINK_CENTER);
	flags.append(SIZE_SHRINK_END);
	return flags;
}

Vector<int> Container::get_allowed_size_flags_vertical() const {
	Vector<int> flags;
	if (GDVIRTUAL_CALL(_get_allowed_size_flags_vertical, flags)) {
		return flags;
	}

	flags.append(SIZE_FILL);
	flags.append(SIZE_EXPAND);
	flags.append(SIZE_SHRINK_BEGIN);
	flags.append(SIZE_SHRINK_CENTER);
	flags.append(SIZE_SHRINK_END);
	return flags;
}

void Container::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_RESIZED:
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_ENTER_TREE: {
			queue_sort();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible_in_tree()) {
				queue_sort();
			}
		} break;
	}
}

PackedStringArray Container::get_configuration_warnings() const {
	PackedStringArray warnings = Control::get_configuration_warnings();

	if (get_class() == "Container" && get_script().is_null()) {
		warnings.push_back(RTR("Container by itself serves no purpose unless a script configures its children placement behavior.\nIf you don't intend to add a script, use a plain Control node instead."));
	}

	return warnings;
}

void Container::_bind_methods() {
	ClassDB::bind_method(D_METHOD("queue_sort"), &Container::queue_sort);
	ClassDB::bind_method(D_METHOD("fit_child_in_rect", "child", "rect"), &Container::fit_child_in_rect);

	GDVIRTUAL_BIND(_get_allowed_size_flags_horizontal);
	GDVIRTUAL_BIND(_get_allowed_size_flags_vertical);

	BIND_CONSTANT(NOTIFICATION_PRE_SORT_CHILDREN);
	BIND_CONSTANT(NOTIFICATION_SORT_CHILDREN);

	ADD_SIGNAL(MethodInfo("pre_sort_children"));
	ADD_SIGNAL(MethodInfo("sort_children"));
}

Container::Container() {
	// All containers should let mouse events pass by default.
	set_mouse_filter(MOUSE_FILTER_PASS);
}
