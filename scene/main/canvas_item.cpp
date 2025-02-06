/**************************************************************************/
/*  canvas_item.cpp                                                       */
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

#include "canvas_item.h"
#include "canvas_item.compat.inc"

#include "scene/2d/canvas_group.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/window.h"
#include "scene/resources/atlas_texture.h"
#include "scene/resources/font.h"
#include "scene/resources/multimesh.h"
#include "scene/resources/style_box.h"
#include "scene/resources/world_2d.h"

#define ERR_DRAW_GUARD \
	ERR_FAIL_COND_MSG(!drawing, "Drawing is only allowed inside this node's `_draw()`, functions connected to its `draw` signal, or when it receives NOTIFICATION_DRAW.")

#ifdef DEBUG_ENABLED
bool CanvasItem::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {
	if (_edit_use_rect()) {
		return _edit_get_rect().has_point(p_point);
	} else {
		return p_point.length() < p_tolerance;
	}
}
#endif // DEBUG_ENABLED

#ifdef TOOLS_ENABLED
Transform2D CanvasItem::_edit_get_transform() const {
	return Transform2D(_edit_get_rotation(), _edit_get_position() + _edit_get_pivot());
}
#endif //TOOLS_ENABLED

bool CanvasItem::is_visible_in_tree() const {
	ERR_READ_THREAD_GUARD_V(false);
	return visible && parent_visible_in_tree;
}

void CanvasItem::_propagate_visibility_changed(bool p_parent_visible_in_tree) {
	parent_visible_in_tree = p_parent_visible_in_tree;
	if (!visible) {
		return;
	}

	_handle_visibility_change(p_parent_visible_in_tree);
}

void CanvasItem::set_visible(bool p_visible) {
	ERR_MAIN_THREAD_GUARD;
	if (visible == p_visible) {
		return;
	}

	visible = p_visible;

	if (!parent_visible_in_tree) {
		notification(NOTIFICATION_VISIBILITY_CHANGED);
		return;
	}

	_handle_visibility_change(p_visible);
}

void CanvasItem::_handle_visibility_change(bool p_visible) {
	RenderingServer::get_singleton()->canvas_item_set_visible(canvas_item, p_visible);
	notification(NOTIFICATION_VISIBILITY_CHANGED);

	if (p_visible) {
		queue_redraw();
	} else {
		emit_signal(SceneStringName(hidden));
	}

	_block();
	for (int i = 0; i < get_child_count(); i++) {
		CanvasItem *c = Object::cast_to<CanvasItem>(get_child(i));

		if (c) { // Should the top_levels stop propagation? I think so, but...
			c->_propagate_visibility_changed(p_visible);
		}
	}
	_unblock();
}

void CanvasItem::show() {
	ERR_MAIN_THREAD_GUARD;
	set_visible(true);
}

void CanvasItem::hide() {
	ERR_MAIN_THREAD_GUARD;
	set_visible(false);
}

bool CanvasItem::is_visible() const {
	ERR_READ_THREAD_GUARD_V(false);
	return visible;
}

CanvasItem *CanvasItem::current_item_drawn = nullptr;
CanvasItem *CanvasItem::get_current_item_drawn() {
	return current_item_drawn;
}

void CanvasItem::_redraw_callback() {
	if (!is_inside_tree()) {
		pending_update = false;
		return;
	}

	RenderingServer::get_singleton()->canvas_item_clear(get_canvas_item());
	//todo updating = true - only allow drawing here
	if (is_visible_in_tree()) {
		drawing = true;
		current_item_drawn = this;
		notification(NOTIFICATION_DRAW);
		emit_signal(SceneStringName(draw));
		GDVIRTUAL_CALL(_draw);
		current_item_drawn = nullptr;
		drawing = false;
	}
	//todo updating = false
	pending_update = false; // don't change to false until finished drawing (avoid recursive update)
}

Transform2D CanvasItem::get_global_transform_with_canvas() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	if (canvas_layer) {
		return canvas_layer->get_final_transform() * get_global_transform();
	} else if (is_inside_tree()) {
		return get_viewport()->get_canvas_transform() * get_global_transform();
	} else {
		return get_global_transform();
	}
}

Transform2D CanvasItem::get_screen_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	ERR_FAIL_COND_V(!is_inside_tree(), Transform2D());
	return get_viewport()->get_popup_base_transform() * get_global_transform_with_canvas();
}

Transform2D CanvasItem::get_global_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());

	if (_is_global_invalid()) {
		// This code can enter multiple times from threads if dirty, this is expected.
		const CanvasItem *pi = get_parent_item();
		Transform2D new_global;
		if (pi) {
			new_global = pi->get_global_transform() * get_transform();
		} else {
			new_global = get_transform();
		}

		global_transform = new_global;
		_set_global_invalid(false);
	}

	return global_transform;
}

// Same as get_global_transform() but no reset for `global_invalid`.
Transform2D CanvasItem::get_global_transform_const() const {
	if (_is_global_invalid()) {
		const CanvasItem *pi = get_parent_item();
		if (pi) {
			global_transform = pi->get_global_transform_const() * get_transform();
		} else {
			global_transform = get_transform();
		}
	}

	return global_transform;
}

void CanvasItem::_set_global_invalid(bool p_invalid) const {
	if (is_group_processing()) {
		if (p_invalid) {
			global_invalid.mt.set();
		} else {
			global_invalid.mt.clear();
		}
	} else {
		global_invalid.st = p_invalid;
	}
}

void CanvasItem::_top_level_raise_self() {
	if (!is_inside_tree()) {
		return;
	}

	if (canvas_layer) {
		RenderingServer::get_singleton()->canvas_item_set_draw_index(canvas_item, canvas_layer->get_sort_index());
	} else {
		RenderingServer::get_singleton()->canvas_item_set_draw_index(canvas_item, get_viewport()->gui_get_canvas_sort_index());
	}
}

void CanvasItem::_enter_canvas() {
	// Resolves to nullptr if the node is top_level.
	CanvasItem *parent_item = get_parent_item();

	if (get_parent()) {
		get_viewport()->canvas_parent_mark_dirty(get_parent());
	}

	if (parent_item) {
		canvas_layer = parent_item->canvas_layer;
		RenderingServer::get_singleton()->canvas_item_set_parent(canvas_item, parent_item->get_canvas_item());
		RenderingServer::get_singleton()->canvas_item_set_visibility_layer(canvas_item, visibility_layer);
	} else {
		Node *n = this;

		canvas_layer = nullptr;

		while (n) {
			canvas_layer = Object::cast_to<CanvasLayer>(n);
			if (canvas_layer) {
				break;
			}
			if (Object::cast_to<Viewport>(n)) {
				break;
			}
			n = n->get_parent();
		}

		RID canvas;
		if (canvas_layer) {
			canvas = canvas_layer->get_canvas();
		} else {
			canvas = get_viewport()->find_world_2d()->get_canvas();
		}

		RenderingServer::get_singleton()->canvas_item_set_parent(canvas_item, canvas);
		RenderingServer::get_singleton()->canvas_item_set_visibility_layer(canvas_item, visibility_layer);

		canvas_group = "_root_canvas" + itos(canvas.get_id());

		add_to_group(canvas_group);
		if (canvas_layer) {
			canvas_layer->reset_sort_index();
		} else {
			get_viewport()->gui_reset_canvas_sort_index();
		}
	}

	pending_update = false;
	queue_redraw();

	notification(NOTIFICATION_ENTER_CANVAS);
}

void CanvasItem::_exit_canvas() {
	notification(NOTIFICATION_EXIT_CANVAS, true); //reverse the notification
	RenderingServer::get_singleton()->canvas_item_set_parent(canvas_item, RID());
	canvas_layer = nullptr;
	if (canvas_group != StringName()) {
		remove_from_group(canvas_group);
		canvas_group = StringName();
	}
}

void CanvasItem::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			ERR_MAIN_THREAD_GUARD;
			ERR_FAIL_COND(!is_inside_tree());

			Node *parent = get_parent();
			if (parent) {
				CanvasItem *ci = Object::cast_to<CanvasItem>(parent);

				if (ci) {
					parent_visible_in_tree = ci->is_visible_in_tree();
					C = ci->children_items.push_back(this);
				} else {
					CanvasLayer *cl = Object::cast_to<CanvasLayer>(parent);

					if (cl) {
						parent_visible_in_tree = cl->is_visible();
					} else {
						// Look for a window.
						Viewport *viewport = nullptr;

						while (parent) {
							viewport = Object::cast_to<Viewport>(parent);
							if (viewport) {
								break;
							}
							parent = parent->get_parent();
						}

						ERR_FAIL_NULL(viewport);

						window = Object::cast_to<Window>(viewport);
						if (window) {
							window->connect(SceneStringName(visibility_changed), callable_mp(this, &CanvasItem::_window_visibility_changed));
							parent_visible_in_tree = window->is_visible();
						} else {
							parent_visible_in_tree = true;
						}
					}
				}
			}

			_set_global_invalid(true);
			_enter_canvas();

			RenderingServer::get_singleton()->canvas_item_set_visible(canvas_item, is_visible_in_tree()); // The visibility of the parent may change.
			if (is_visible_in_tree()) {
				notification(NOTIFICATION_VISIBILITY_CHANGED); // Considered invisible until entered.
			}

			_update_texture_filter_changed(false);
			_update_texture_repeat_changed(false);

			if (!block_transform_notify && !xform_change.in_list()) {
				get_tree()->xform_change_list.add(&xform_change);
			}

			if (get_viewport()) {
				get_parent()->connect(SNAME("child_order_changed"), callable_mp(get_viewport(), &Viewport::canvas_parent_mark_dirty).bind(get_parent()), CONNECT_REFERENCE_COUNTED);
			}

			// If using physics interpolation, reset for this node only,
			// as a helper, as in most cases, users will want items reset when
			// adding to the tree.
			// In cases where they move immediately after adding,
			// there will be little cost in having two resets as these are cheap,
			// and it is worth it for convenience.
			// Do not propagate to children, as each child of an added branch
			// receives its own NOTIFICATION_ENTER_TREE, and this would
			// cause unnecessary duplicate resets.
			if (is_physics_interpolated_and_enabled()) {
				notification(NOTIFICATION_RESET_PHYSICS_INTERPOLATION);
			}

		} break;
		case NOTIFICATION_EXIT_TREE: {
			ERR_MAIN_THREAD_GUARD;

			if (xform_change.in_list()) {
				get_tree()->xform_change_list.remove(&xform_change);
			}
			_exit_canvas();
			if (C) {
				Object::cast_to<CanvasItem>(get_parent())->children_items.erase(C);
				C = nullptr;
			}
			if (window) {
				window->disconnect(SceneStringName(visibility_changed), callable_mp(this, &CanvasItem::_window_visibility_changed));
				window = nullptr;
			}
			_set_global_invalid(true);
			parent_visible_in_tree = false;

			if (get_viewport()) {
				get_parent()->disconnect(SNAME("child_order_changed"), callable_mp(get_viewport(), &Viewport::canvas_parent_mark_dirty).bind(get_parent()));
			}
		} break;

		case NOTIFICATION_RESET_PHYSICS_INTERPOLATION: {
			if (is_visible_in_tree() && is_physics_interpolated()) {
				RenderingServer::get_singleton()->canvas_item_reset_physics_interpolation(canvas_item);
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			ERR_MAIN_THREAD_GUARD;

			emit_signal(SceneStringName(visibility_changed));
		} break;
		case NOTIFICATION_WORLD_2D_CHANGED: {
			ERR_MAIN_THREAD_GUARD;

			_exit_canvas();
			_enter_canvas();
		} break;
		case NOTIFICATION_PARENTED: {
			// The node is not inside the tree during this notification.
			ERR_MAIN_THREAD_GUARD;

			_notify_transform();
		} break;
	}
}

void CanvasItem::update_draw_order() {
	ERR_MAIN_THREAD_GUARD;

	if (!is_inside_tree()) {
		return;
	}

	if (canvas_group != StringName()) {
		get_tree()->call_group_flags(SceneTree::GROUP_CALL_UNIQUE | SceneTree::GROUP_CALL_DEFERRED, canvas_group, "_top_level_raise_self");
	} else {
		ERR_FAIL_NULL_MSG(get_parent_item(), "Moved child is in incorrect state (no canvas group, no canvas item parent).");
		RenderingServer::get_singleton()->canvas_item_set_draw_index(canvas_item, get_index());
	}
}

void CanvasItem::_window_visibility_changed() {
	_propagate_visibility_changed(window->is_visible());
}

void CanvasItem::queue_redraw() {
	ERR_THREAD_GUARD; // Calling from thread is safe.
	if (!is_inside_tree()) {
		return;
	}
	if (pending_update) {
		return;
	}

	pending_update = true;

	callable_mp(this, &CanvasItem::_redraw_callback).call_deferred();
}

void CanvasItem::move_to_front() {
	ERR_MAIN_THREAD_GUARD;
	if (!get_parent()) {
		return;
	}
	get_parent()->move_child(this, -1);
}

void CanvasItem::set_modulate(const Color &p_modulate) {
	ERR_THREAD_GUARD;
	if (modulate == p_modulate) {
		return;
	}

	modulate = p_modulate;
	RenderingServer::get_singleton()->canvas_item_set_modulate(canvas_item, modulate);
}

Color CanvasItem::get_modulate() const {
	ERR_READ_THREAD_GUARD_V(Color());
	return modulate;
}

Color CanvasItem::get_modulate_in_tree() const {
	ERR_READ_THREAD_GUARD_V(Color());
	Color final_modulate = modulate;
	CanvasItem *parent_item = get_parent_item();
	while (parent_item) {
		final_modulate *= parent_item->get_modulate();
		parent_item = parent_item->get_parent_item();
	}
	return final_modulate;
}

void CanvasItem::set_as_top_level(bool p_top_level) {
	ERR_MAIN_THREAD_GUARD;
	if (top_level == p_top_level) {
		return;
	}

	if (!is_inside_tree()) {
		top_level = p_top_level;
		_notify_transform();
		return;
	}

	_exit_canvas();
	top_level = p_top_level;
	_top_level_changed();
	_enter_canvas();

	_notify_transform();

	if (get_viewport()) {
		get_viewport()->canvas_item_top_level_changed();
	}
}

void CanvasItem::_top_level_changed() {
	// Inform children that top_level status has changed on a parent.
	int children = get_child_count();
	for (int i = 0; i < children; i++) {
		CanvasItem *child = Object::cast_to<CanvasItem>(get_child(i));
		if (child) {
			child->_top_level_changed_on_parent();
		}
	}
}

void CanvasItem::_top_level_changed_on_parent() {
	// Inform children that top_level status has changed on a parent.
	_top_level_changed();
}

bool CanvasItem::is_set_as_top_level() const {
	return top_level;
}

CanvasItem *CanvasItem::get_parent_item() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	if (top_level) {
		return nullptr;
	}

	return Object::cast_to<CanvasItem>(get_parent());
}

void CanvasItem::set_self_modulate(const Color &p_self_modulate) {
	ERR_THREAD_GUARD;
	if (self_modulate == p_self_modulate) {
		return;
	}

	self_modulate = p_self_modulate;
	RenderingServer::get_singleton()->canvas_item_set_self_modulate(canvas_item, self_modulate);
}

Color CanvasItem::get_self_modulate() const {
	ERR_READ_THREAD_GUARD_V(Color());
	return self_modulate;
}

void CanvasItem::set_light_mask(int p_light_mask) {
	ERR_THREAD_GUARD;
	if (light_mask == p_light_mask) {
		return;
	}

	light_mask = p_light_mask;
	RS::get_singleton()->canvas_item_set_light_mask(canvas_item, p_light_mask);
}

int CanvasItem::get_light_mask() const {
	ERR_READ_THREAD_GUARD_V(0);
	return light_mask;
}

const StringName *CanvasItem::_instance_shader_parameter_get_remap(const StringName &p_name) const {
	StringName *r = instance_shader_parameter_property_remap.getptr(p_name);
	if (!r) {
		String s = p_name;
		if (s.begins_with("instance_shader_parameters/")) {
			StringName name = s.trim_prefix("instance_shader_parameters/");
			instance_shader_parameter_property_remap[p_name] = name;
			return instance_shader_parameter_property_remap.getptr(p_name);
		}
		return nullptr;
	}
	return r;
}

bool CanvasItem::_set(const StringName &p_name, const Variant &p_value) {
	const StringName *r = _instance_shader_parameter_get_remap(p_name);
	if (r) {
		set_instance_shader_parameter(*r, p_value);
		return true;
	}
	return false;
}

bool CanvasItem::_get(const StringName &p_name, Variant &r_ret) const {
	const StringName *r = _instance_shader_parameter_get_remap(p_name);
	if (r) {
		r_ret = get_instance_shader_parameter(*r);
		return true;
	}

	return false;
}

void CanvasItem::_get_property_list(List<PropertyInfo> *p_list) const {
	List<PropertyInfo> pinfo;
	RS::get_singleton()->canvas_item_get_instance_shader_parameter_list(get_canvas_item(), &pinfo);

	for (PropertyInfo &pi : pinfo) {
		bool has_def_value = false;
		Variant def_value = RS::get_singleton()->canvas_item_get_instance_shader_parameter_default_value(get_canvas_item(), pi.name);
		if (def_value.get_type() != Variant::NIL) {
			has_def_value = true;
		}
		if (instance_shader_parameters.has(pi.name)) {
			pi.usage = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE | (has_def_value ? (PROPERTY_USAGE_CHECKABLE | PROPERTY_USAGE_CHECKED) : PROPERTY_USAGE_NONE);
		} else {
			pi.usage = PROPERTY_USAGE_EDITOR | (has_def_value ? PROPERTY_USAGE_CHECKABLE : PROPERTY_USAGE_NONE); // Do not save if not changed.
		}

		pi.name = "instance_shader_parameters/" + pi.name;
		p_list->push_back(pi);
	}
}

void CanvasItem::item_rect_changed(bool p_size_changed) {
	ERR_MAIN_THREAD_GUARD;
	if (p_size_changed) {
		queue_redraw();
	}
	emit_signal(SceneStringName(item_rect_changed));
}

void CanvasItem::set_z_index(int p_z) {
	ERR_THREAD_GUARD;
	ERR_FAIL_COND(p_z < RS::CANVAS_ITEM_Z_MIN);
	ERR_FAIL_COND(p_z > RS::CANVAS_ITEM_Z_MAX);
	z_index = p_z;
	RS::get_singleton()->canvas_item_set_z_index(canvas_item, z_index);
	update_configuration_warnings();
}

void CanvasItem::set_z_as_relative(bool p_enabled) {
	ERR_THREAD_GUARD;
	if (z_relative == p_enabled) {
		return;
	}
	z_relative = p_enabled;
	RS::get_singleton()->canvas_item_set_z_as_relative_to_parent(canvas_item, p_enabled);
}

bool CanvasItem::is_z_relative() const {
	ERR_READ_THREAD_GUARD_V(false);
	return z_relative;
}

int CanvasItem::get_z_index() const {
	ERR_READ_THREAD_GUARD_V(0);
	return z_index;
}

int CanvasItem::get_effective_z_index() const {
	ERR_READ_THREAD_GUARD_V(0);
	int effective_z_index = z_index;
	if (is_z_relative()) {
		CanvasItem *p = get_parent_item();
		if (p) {
			effective_z_index += p->get_effective_z_index();
		}
	}
	return effective_z_index;
}

void CanvasItem::set_y_sort_enabled(bool p_enabled) {
	ERR_THREAD_GUARD;
	y_sort_enabled = p_enabled;
	RS::get_singleton()->canvas_item_set_sort_children_by_y(canvas_item, y_sort_enabled);
}

bool CanvasItem::is_y_sort_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return y_sort_enabled;
}

void CanvasItem::draw_dashed_line(const Point2 &p_from, const Point2 &p_to, const Color &p_color, real_t p_width, real_t p_dash, bool p_aligned, bool p_antialiased) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;
	ERR_FAIL_COND(p_dash <= 0.0);

	float length = (p_to - p_from).length();
	Vector2 step = p_dash * (p_to - p_from).normalized();

	if (length < p_dash || step == Vector2()) {
		RenderingServer::get_singleton()->canvas_item_add_line(canvas_item, p_from, p_to, p_color, p_width, p_antialiased);
		return;
	}

	int steps = (p_aligned) ? Math::ceil(length / p_dash) : Math::floor(length / p_dash);
	if (steps % 2 == 0) {
		steps--;
	}

	Point2 off = p_from;
	if (p_aligned) {
		off += (p_to - p_from).normalized() * (length - steps * p_dash) / 2.0;
	}

	Vector<Vector2> points;
	points.resize(steps + 1);
	for (int i = 0; i < steps; i += 2) {
		points.write[i] = (i == 0) ? p_from : off;
		points.write[i + 1] = (p_aligned && i == steps - 1) ? p_to : (off + step);
		off += step * 2;
	}

	Vector<Color> colors = { p_color };

	RenderingServer::get_singleton()->canvas_item_add_multiline(canvas_item, points, colors, p_width, p_antialiased);
}

void CanvasItem::draw_line(const Point2 &p_from, const Point2 &p_to, const Color &p_color, real_t p_width, bool p_antialiased) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	RenderingServer::get_singleton()->canvas_item_add_line(canvas_item, p_from, p_to, p_color, p_width, p_antialiased);
}

void CanvasItem::draw_polyline(const Vector<Point2> &p_points, const Color &p_color, real_t p_width, bool p_antialiased) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	Vector<Color> colors = { p_color };
	RenderingServer::get_singleton()->canvas_item_add_polyline(canvas_item, p_points, colors, p_width, p_antialiased);
}

void CanvasItem::draw_polyline_colors(const Vector<Point2> &p_points, const Vector<Color> &p_colors, real_t p_width, bool p_antialiased) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	RenderingServer::get_singleton()->canvas_item_add_polyline(canvas_item, p_points, p_colors, p_width, p_antialiased);
}

void CanvasItem::draw_arc(const Vector2 &p_center, real_t p_radius, real_t p_start_angle, real_t p_end_angle, int p_point_count, const Color &p_color, real_t p_width, bool p_antialiased) {
	ERR_THREAD_GUARD;
	Vector<Point2> points;
	points.resize(p_point_count);
	Point2 *points_ptr = points.ptrw();

	// Clamp angle difference to full circle so arc won't overlap itself.
	const real_t delta_angle = CLAMP(p_end_angle - p_start_angle, -Math_TAU, Math_TAU);
	for (int i = 0; i < p_point_count; i++) {
		real_t theta = (i / (p_point_count - 1.0f)) * delta_angle + p_start_angle;
		points_ptr[i] = p_center + Vector2(Math::cos(theta), Math::sin(theta)) * p_radius;
	}

	draw_polyline(points, p_color, p_width, p_antialiased);
}

void CanvasItem::draw_multiline(const Vector<Point2> &p_points, const Color &p_color, real_t p_width, bool p_antialiased) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	Vector<Color> colors = { p_color };
	RenderingServer::get_singleton()->canvas_item_add_multiline(canvas_item, p_points, colors, p_width, p_antialiased);
}

void CanvasItem::draw_multiline_colors(const Vector<Point2> &p_points, const Vector<Color> &p_colors, real_t p_width, bool p_antialiased) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	RenderingServer::get_singleton()->canvas_item_add_multiline(canvas_item, p_points, p_colors, p_width, p_antialiased);
}

void CanvasItem::draw_rect(const Rect2 &p_rect, const Color &p_color, bool p_filled, real_t p_width, bool p_antialiased) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	Rect2 rect = p_rect.abs();

	if (p_filled) {
		if (p_width != -1.0) {
			WARN_PRINT("The draw_rect() \"width\" argument has no effect when \"filled\" is \"true\".");
		}

		RenderingServer::get_singleton()->canvas_item_add_rect(canvas_item, rect, p_color, p_antialiased);
	} else if (p_width >= rect.size.width || p_width >= rect.size.height) {
		RenderingServer::get_singleton()->canvas_item_add_rect(canvas_item, rect.grow(0.5f * p_width), p_color, p_antialiased);
	} else {
		Vector<Vector2> points;
		points.resize(5);
		points.write[0] = rect.position;
		points.write[1] = rect.position + Vector2(rect.size.x, 0);
		points.write[2] = rect.position + rect.size;
		points.write[3] = rect.position + Vector2(0, rect.size.y);
		points.write[4] = rect.position;

		Vector<Color> colors = { p_color };

		RenderingServer::get_singleton()->canvas_item_add_polyline(canvas_item, points, colors, p_width, p_antialiased);
	}
}

void CanvasItem::draw_circle(const Point2 &p_pos, real_t p_radius, const Color &p_color, bool p_filled, real_t p_width, bool p_antialiased) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	if (p_filled) {
		if (p_width != -1.0) {
			WARN_PRINT("The draw_circle() \"width\" argument has no effect when \"filled\" is \"true\".");
		}

		RenderingServer::get_singleton()->canvas_item_add_circle(canvas_item, p_pos, p_radius, p_color, p_antialiased);
	} else if (p_width >= 2.0 * p_radius) {
		RenderingServer::get_singleton()->canvas_item_add_circle(canvas_item, p_pos, p_radius + 0.5 * p_width, p_color, p_antialiased);
	} else {
		// Tessellation count is hardcoded. Keep in sync with the same variable in `RendererCanvasCull::canvas_item_add_circle()`.
		const int circle_segments = 64;

		Vector<Vector2> points;
		points.resize(circle_segments + 1);

		Vector2 *points_ptr = points.ptrw();
		const real_t circle_point_step = Math_TAU / circle_segments;

		for (int i = 0; i < circle_segments; i++) {
			float angle = i * circle_point_step;
			points_ptr[i].x = Math::cos(angle) * p_radius;
			points_ptr[i].y = Math::sin(angle) * p_radius;
			points_ptr[i] += p_pos;
		}
		points_ptr[circle_segments] = points_ptr[0];

		Vector<Color> colors = { p_color };

		RenderingServer::get_singleton()->canvas_item_add_polyline(canvas_item, points, colors, p_width, p_antialiased);
	}
}

void CanvasItem::draw_texture(const Ref<Texture2D> &p_texture, const Point2 &p_pos, const Color &p_modulate) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	ERR_FAIL_COND(p_texture.is_null());

	p_texture->draw(canvas_item, p_pos, p_modulate, false);
}

void CanvasItem::draw_texture_rect(const Ref<Texture2D> &p_texture, const Rect2 &p_rect, bool p_tile, const Color &p_modulate, bool p_transpose) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	ERR_FAIL_COND(p_texture.is_null());
	p_texture->draw_rect(canvas_item, p_rect, p_tile, p_modulate, p_transpose);
}

void CanvasItem::draw_texture_rect_region(const Ref<Texture2D> &p_texture, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, bool p_transpose, bool p_clip_uv) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;
	ERR_FAIL_COND(p_texture.is_null());
	p_texture->draw_rect_region(canvas_item, p_rect, p_src_rect, p_modulate, p_transpose, p_clip_uv);
}

void CanvasItem::draw_msdf_texture_rect_region(const Ref<Texture2D> &p_texture, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate, double p_outline, double p_pixel_range, double p_scale) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;
	ERR_FAIL_COND(p_texture.is_null());
	RenderingServer::get_singleton()->canvas_item_add_msdf_texture_rect_region(canvas_item, p_rect, p_texture->get_rid(), p_src_rect, p_modulate, p_outline, p_pixel_range, p_scale);
}

void CanvasItem::draw_lcd_texture_rect_region(const Ref<Texture2D> &p_texture, const Rect2 &p_rect, const Rect2 &p_src_rect, const Color &p_modulate) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;
	ERR_FAIL_COND(p_texture.is_null());
	RenderingServer::get_singleton()->canvas_item_add_lcd_texture_rect_region(canvas_item, p_rect, p_texture->get_rid(), p_src_rect, p_modulate);
}

void CanvasItem::draw_style_box(const Ref<StyleBox> &p_style_box, const Rect2 &p_rect) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	ERR_FAIL_COND(p_style_box.is_null());

	p_style_box->draw(canvas_item, p_rect);
}

void CanvasItem::draw_primitive(const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, Ref<Texture2D> p_texture) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	RID rid = p_texture.is_valid() ? p_texture->get_rid() : RID();
	RenderingServer::get_singleton()->canvas_item_add_primitive(canvas_item, p_points, p_colors, p_uvs, rid);
}

void CanvasItem::draw_set_transform(const Point2 &p_offset, real_t p_rot, const Size2 &p_scale) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	Transform2D xform(p_rot, p_offset);
	xform.scale_basis(p_scale);
	RenderingServer::get_singleton()->canvas_item_add_set_transform(canvas_item, xform);
}

void CanvasItem::draw_set_transform_matrix(const Transform2D &p_matrix) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	RenderingServer::get_singleton()->canvas_item_add_set_transform(canvas_item, p_matrix);
}
void CanvasItem::draw_animation_slice(double p_animation_length, double p_slice_begin, double p_slice_end, double p_offset) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	RenderingServer::get_singleton()->canvas_item_add_animation_slice(canvas_item, p_animation_length, p_slice_begin, p_slice_end, p_offset);
}

void CanvasItem::draw_end_animation() {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	RenderingServer::get_singleton()->canvas_item_add_animation_slice(canvas_item, 1, 0, 2, 0);
}

void CanvasItem::draw_polygon(const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, Ref<Texture2D> p_texture) {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;

	const Ref<AtlasTexture> atlas = p_texture;
	if (atlas.is_valid() && atlas->get_atlas().is_valid()) {
		const Ref<Texture2D> &texture = atlas->get_atlas();
		const Vector2 atlas_size = texture->get_size();

		const Vector2 remap_min = atlas->get_region().position / atlas_size;
		const Vector2 remap_max = atlas->get_region().get_end() / atlas_size;

		PackedVector2Array uvs = p_uvs;
		for (Vector2 &p : uvs) {
			p.x = Math::remap(p.x, 0, 1, remap_min.x, remap_max.x);
			p.y = Math::remap(p.y, 0, 1, remap_min.y, remap_max.y);
		}
		RenderingServer::get_singleton()->canvas_item_add_polygon(canvas_item, p_points, p_colors, uvs, texture->get_rid());
	} else {
		RID texture_rid = p_texture.is_valid() ? p_texture->get_rid() : RID();
		RenderingServer::get_singleton()->canvas_item_add_polygon(canvas_item, p_points, p_colors, p_uvs, texture_rid);
	}
}

void CanvasItem::draw_colored_polygon(const Vector<Point2> &p_points, const Color &p_color, const Vector<Point2> &p_uvs, Ref<Texture2D> p_texture) {
	draw_polygon(p_points, { p_color }, p_uvs, p_texture);
}

void CanvasItem::draw_mesh(const Ref<Mesh> &p_mesh, const Ref<Texture2D> &p_texture, const Transform2D &p_transform, const Color &p_modulate) {
	ERR_THREAD_GUARD;
	ERR_FAIL_COND(p_mesh.is_null());
	RID texture_rid = p_texture.is_valid() ? p_texture->get_rid() : RID();

	RenderingServer::get_singleton()->canvas_item_add_mesh(canvas_item, p_mesh->get_rid(), p_transform, p_modulate, texture_rid);
}

void CanvasItem::draw_multimesh(const Ref<MultiMesh> &p_multimesh, const Ref<Texture2D> &p_texture) {
	ERR_THREAD_GUARD;
	ERR_FAIL_COND(p_multimesh.is_null());
	RID texture_rid = p_texture.is_valid() ? p_texture->get_rid() : RID();
	RenderingServer::get_singleton()->canvas_item_add_multimesh(canvas_item, p_multimesh->get_rid(), texture_rid);
}

void CanvasItem::draw_string(const Ref<Font> &p_font, const Point2 &p_pos, const String &p_text, HorizontalAlignment p_alignment, float p_width, int p_font_size, const Color &p_modulate, BitField<TextServer::JustificationFlag> p_jst_flags, TextServer::Direction p_direction, TextServer::Orientation p_orientation) const {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;
	ERR_FAIL_COND(p_font.is_null());

	p_font->draw_string(canvas_item, p_pos, p_text, p_alignment, p_width, p_font_size, p_modulate, p_jst_flags, p_direction, p_orientation);
}

void CanvasItem::draw_multiline_string(const Ref<Font> &p_font, const Point2 &p_pos, const String &p_text, HorizontalAlignment p_alignment, float p_width, int p_font_size, int p_max_lines, const Color &p_modulate, BitField<TextServer::LineBreakFlag> p_brk_flags, BitField<TextServer::JustificationFlag> p_jst_flags, TextServer::Direction p_direction, TextServer::Orientation p_orientation) const {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;
	ERR_FAIL_COND(p_font.is_null());

	p_font->draw_multiline_string(canvas_item, p_pos, p_text, p_alignment, p_width, p_font_size, p_max_lines, p_modulate, p_brk_flags, p_jst_flags, p_direction, p_orientation);
}

void CanvasItem::draw_string_outline(const Ref<Font> &p_font, const Point2 &p_pos, const String &p_text, HorizontalAlignment p_alignment, float p_width, int p_font_size, int p_size, const Color &p_modulate, BitField<TextServer::JustificationFlag> p_jst_flags, TextServer::Direction p_direction, TextServer::Orientation p_orientation) const {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;
	ERR_FAIL_COND(p_font.is_null());

	p_font->draw_string_outline(canvas_item, p_pos, p_text, p_alignment, p_width, p_font_size, p_size, p_modulate, p_jst_flags, p_direction, p_orientation);
}

void CanvasItem::draw_multiline_string_outline(const Ref<Font> &p_font, const Point2 &p_pos, const String &p_text, HorizontalAlignment p_alignment, float p_width, int p_font_size, int p_max_lines, int p_size, const Color &p_modulate, BitField<TextServer::LineBreakFlag> p_brk_flags, BitField<TextServer::JustificationFlag> p_jst_flags, TextServer::Direction p_direction, TextServer::Orientation p_orientation) const {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;
	ERR_FAIL_COND(p_font.is_null());

	p_font->draw_multiline_string_outline(canvas_item, p_pos, p_text, p_alignment, p_width, p_font_size, p_max_lines, p_size, p_modulate, p_brk_flags, p_jst_flags, p_direction, p_orientation);
}

void CanvasItem::draw_char(const Ref<Font> &p_font, const Point2 &p_pos, const String &p_char, int p_font_size, const Color &p_modulate) const {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;
	ERR_FAIL_COND(p_char.length() != 1);
	ERR_FAIL_COND(p_font.is_null());

	p_font->draw_char(canvas_item, p_pos, p_char[0], p_font_size, p_modulate);
}

void CanvasItem::draw_char_outline(const Ref<Font> &p_font, const Point2 &p_pos, const String &p_char, int p_font_size, int p_size, const Color &p_modulate) const {
	ERR_THREAD_GUARD;
	ERR_DRAW_GUARD;
	ERR_FAIL_COND(p_char.length() != 1);
	ERR_FAIL_COND(p_font.is_null());

	p_font->draw_char_outline(canvas_item, p_pos, p_char[0], p_font_size, p_size, p_modulate);
}

void CanvasItem::_notify_transform_deferred() {
	if (is_inside_tree() && notify_transform && !xform_change.in_list()) {
		get_tree()->xform_change_list.add(&xform_change);
	}
}

void CanvasItem::_notify_transform(CanvasItem *p_node) {
	/* This check exists to avoid re-propagating the transform
	 * notification down the tree on dirty nodes. It provides
	 * optimization by avoiding redundancy (nodes are dirty, will get the
	 * notification anyway).
	 */

	if (/*p_node->xform_change.in_list() &&*/ p_node->_is_global_invalid()) {
		return; //nothing to do
	}

	p_node->_set_global_invalid(true);

	if (p_node->notify_transform && !p_node->xform_change.in_list()) {
		if (!p_node->block_transform_notify) {
			if (p_node->is_inside_tree()) {
				if (is_accessible_from_caller_thread()) {
					get_tree()->xform_change_list.add(&p_node->xform_change);
				} else {
					// Should be rare, but still needs to be handled.
					callable_mp(p_node, &CanvasItem::_notify_transform_deferred).call_deferred();
				}
			}
		}
	}

	for (CanvasItem *ci : p_node->children_items) {
		if (ci->top_level) {
			continue;
		}
		_notify_transform(ci);
	}
}

void CanvasItem::_physics_interpolated_changed() {
	RenderingServer::get_singleton()->canvas_item_set_interpolated(canvas_item, is_physics_interpolated());
}

void CanvasItem::set_canvas_item_use_identity_transform(bool p_enable) {
	// Prevent sending item transforms to RenderingServer when using global coords.
	_set_use_identity_transform(p_enable);

	// Let RenderingServer know not to concatenate the parent transform during the render.
	RenderingServer::get_singleton()->canvas_item_set_use_identity_transform(get_canvas_item(), p_enable);

	if (is_inside_tree()) {
		if (p_enable) {
			// Make sure item is using identity transform in server.
			RenderingServer::get_singleton()->canvas_item_set_transform(get_canvas_item(), Transform2D());
		} else {
			// Make sure item transform is up to date in server if switching identity transform off.
			RenderingServer::get_singleton()->canvas_item_set_transform(get_canvas_item(), get_transform());
		}
	}
}

Rect2 CanvasItem::get_viewport_rect() const {
	ERR_READ_THREAD_GUARD_V(Rect2());
	ERR_FAIL_COND_V(!is_inside_tree(), Rect2());
	return get_viewport()->get_visible_rect();
}

RID CanvasItem::get_canvas() const {
	ERR_READ_THREAD_GUARD_V(RID());
	ERR_FAIL_COND_V(!is_inside_tree(), RID());

	if (canvas_layer) {
		return canvas_layer->get_canvas();
	} else {
		return get_viewport()->find_world_2d()->get_canvas();
	}
}

ObjectID CanvasItem::get_canvas_layer_instance_id() const {
	ERR_READ_THREAD_GUARD_V(ObjectID());
	if (canvas_layer) {
		return canvas_layer->get_instance_id();
	} else {
		return ObjectID();
	}
}

CanvasItem *CanvasItem::get_top_level() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	CanvasItem *ci = const_cast<CanvasItem *>(this);
	while (!ci->top_level && Object::cast_to<CanvasItem>(ci->get_parent())) {
		ci = Object::cast_to<CanvasItem>(ci->get_parent());
	}

	return ci;
}

Ref<World2D> CanvasItem::get_world_2d() const {
	ERR_READ_THREAD_GUARD_V(Ref<World2D>());
	ERR_FAIL_COND_V(!is_inside_tree(), Ref<World2D>());

	CanvasItem *tl = get_top_level();

	if (tl->get_viewport()) {
		return tl->get_viewport()->find_world_2d();
	} else {
		return Ref<World2D>();
	}
}

RID CanvasItem::get_viewport_rid() const {
	ERR_READ_THREAD_GUARD_V(RID());
	ERR_FAIL_COND_V(!is_inside_tree(), RID());
	return get_viewport()->get_viewport_rid();
}

void CanvasItem::set_block_transform_notify(bool p_enable) {
	ERR_THREAD_GUARD;
	block_transform_notify = p_enable;
}

bool CanvasItem::is_block_transform_notify_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return block_transform_notify;
}

void CanvasItem::set_draw_behind_parent(bool p_enable) {
	ERR_THREAD_GUARD;
	if (behind == p_enable) {
		return;
	}
	behind = p_enable;
	RenderingServer::get_singleton()->canvas_item_set_draw_behind_parent(canvas_item, behind);
}

bool CanvasItem::is_draw_behind_parent_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return behind;
}

void CanvasItem::set_material(const Ref<Material> &p_material) {
	ERR_THREAD_GUARD;
	material = p_material;
	RID rid;
	if (material.is_valid()) {
		rid = material->get_rid();
	}
	RS::get_singleton()->canvas_item_set_material(canvas_item, rid);
	notify_property_list_changed(); //properties for material exposed
}

void CanvasItem::set_use_parent_material(bool p_use_parent_material) {
	ERR_THREAD_GUARD;
	use_parent_material = p_use_parent_material;
	RS::get_singleton()->canvas_item_set_use_parent_material(canvas_item, p_use_parent_material);
}

void CanvasItem::set_instance_shader_parameter(const StringName &p_name, const Variant &p_value) {
	if (p_value.get_type() == Variant::NIL) {
		Variant def_value = RS::get_singleton()->canvas_item_get_instance_shader_parameter_default_value(get_canvas_item(), p_name);
		RS::get_singleton()->canvas_item_set_instance_shader_parameter(get_canvas_item(), p_name, def_value);
		instance_shader_parameters.erase(p_value);
	} else {
		instance_shader_parameters[p_name] = p_value;
		if (p_value.get_type() == Variant::OBJECT) {
			RID tex_id = p_value;
			RS::get_singleton()->canvas_item_set_instance_shader_parameter(get_canvas_item(), p_name, tex_id);
		} else {
			RS::get_singleton()->canvas_item_set_instance_shader_parameter(get_canvas_item(), p_name, p_value);
		}
	}
}

Variant CanvasItem::get_instance_shader_parameter(const StringName &p_name) const {
	return RS::get_singleton()->canvas_item_get_instance_shader_parameter(get_canvas_item(), p_name);
}

bool CanvasItem::get_use_parent_material() const {
	ERR_READ_THREAD_GUARD_V(false);
	return use_parent_material;
}

Ref<Material> CanvasItem::get_material() const {
	ERR_READ_THREAD_GUARD_V(Ref<Material>());
	return material;
}

Vector2 CanvasItem::make_canvas_position_local(const Vector2 &screen_point) const {
	ERR_READ_THREAD_GUARD_V(Vector2());
	ERR_FAIL_COND_V(!is_inside_tree(), screen_point);

	Transform2D local_matrix = (get_canvas_transform() * get_global_transform()).affine_inverse();

	return local_matrix.xform(screen_point);
}

Ref<InputEvent> CanvasItem::make_input_local(const Ref<InputEvent> &p_event) const {
	ERR_READ_THREAD_GUARD_V(Ref<InputEvent>());
	ERR_FAIL_COND_V(p_event.is_null(), p_event);
	ERR_FAIL_COND_V(!is_inside_tree(), p_event);

	return p_event->xformed_by((get_canvas_transform() * get_global_transform()).affine_inverse());
}

Vector2 CanvasItem::get_global_mouse_position() const {
	ERR_READ_THREAD_GUARD_V(Vector2());
	ERR_FAIL_NULL_V(get_viewport(), Vector2());
	return get_canvas_transform().affine_inverse().xform(get_viewport()->get_mouse_position());
}

Vector2 CanvasItem::get_local_mouse_position() const {
	ERR_READ_THREAD_GUARD_V(Vector2());
	ERR_FAIL_NULL_V(get_viewport(), Vector2());

	return get_global_transform().affine_inverse().xform(get_global_mouse_position());
}

void CanvasItem::force_update_transform() {
	ERR_THREAD_GUARD;
	ERR_FAIL_COND(!is_inside_tree());
	if (!xform_change.in_list()) {
		return;
	}

	get_tree()->xform_change_list.remove(&xform_change);

	notification(NOTIFICATION_TRANSFORM_CHANGED);
}

void CanvasItem::_validate_property(PropertyInfo &p_property) const {
	if (hide_clip_children && p_property.name == "clip_children") {
		p_property.usage = PROPERTY_USAGE_NONE;
	}
}

PackedStringArray CanvasItem::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (clip_children_mode != CLIP_CHILDREN_DISABLED && is_inside_tree()) {
		bool warned_about_ancestor_clipping = false;
		bool warned_about_canvasgroup_ancestor = false;
		Node *n = get_parent();
		while (n) {
			CanvasItem *as_canvas_item = Object::cast_to<CanvasItem>(n);
			if (!warned_about_ancestor_clipping && as_canvas_item && as_canvas_item->clip_children_mode != CLIP_CHILDREN_DISABLED) {
				warnings.push_back(vformat(RTR("Ancestor \"%s\" clips its children, so this node will not be able to clip its children."), as_canvas_item->get_name()));
				warned_about_ancestor_clipping = true;
			}

			CanvasGroup *as_canvas_group = Object::cast_to<CanvasGroup>(n);
			if (!warned_about_canvasgroup_ancestor && as_canvas_group) {
				warnings.push_back(vformat(RTR("Ancestor \"%s\" is a CanvasGroup, so this node will not be able to clip its children."), as_canvas_group->get_name()));
				warned_about_canvasgroup_ancestor = true;
			}

			// Only break out early once both warnings have been triggered, so
			// that the user is aware of both possible reasons for clipping not working.
			if (warned_about_ancestor_clipping && warned_about_canvasgroup_ancestor) {
				break;
			}
			n = n->get_parent();
		}
	}

	return warnings;
}

void CanvasItem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_top_level_raise_self"), &CanvasItem::_top_level_raise_self);

#ifdef TOOLS_ENABLED
	ClassDB::bind_method(D_METHOD("_edit_set_state", "state"), &CanvasItem::_edit_set_state);
	ClassDB::bind_method(D_METHOD("_edit_get_state"), &CanvasItem::_edit_get_state);
	ClassDB::bind_method(D_METHOD("_edit_set_position", "position"), &CanvasItem::_edit_set_position);
	ClassDB::bind_method(D_METHOD("_edit_get_position"), &CanvasItem::_edit_get_position);
	ClassDB::bind_method(D_METHOD("_edit_set_scale", "scale"), &CanvasItem::_edit_set_scale);
	ClassDB::bind_method(D_METHOD("_edit_get_scale"), &CanvasItem::_edit_get_scale);
	ClassDB::bind_method(D_METHOD("_edit_set_rect", "rect"), &CanvasItem::_edit_set_rect);
	ClassDB::bind_method(D_METHOD("_edit_get_rect"), &CanvasItem::_edit_get_rect);
	ClassDB::bind_method(D_METHOD("_edit_use_rect"), &CanvasItem::_edit_use_rect);
	ClassDB::bind_method(D_METHOD("_edit_set_rotation", "degrees"), &CanvasItem::_edit_set_rotation);
	ClassDB::bind_method(D_METHOD("_edit_get_rotation"), &CanvasItem::_edit_get_rotation);
	ClassDB::bind_method(D_METHOD("_edit_use_rotation"), &CanvasItem::_edit_use_rotation);
	ClassDB::bind_method(D_METHOD("_edit_set_pivot", "pivot"), &CanvasItem::_edit_set_pivot);
	ClassDB::bind_method(D_METHOD("_edit_get_pivot"), &CanvasItem::_edit_get_pivot);
	ClassDB::bind_method(D_METHOD("_edit_use_pivot"), &CanvasItem::_edit_use_pivot);
	ClassDB::bind_method(D_METHOD("_edit_get_transform"), &CanvasItem::_edit_get_transform);
#endif //TOOLS_ENABLED

	ClassDB::bind_method(D_METHOD("get_canvas_item"), &CanvasItem::get_canvas_item);

	ClassDB::bind_method(D_METHOD("set_visible", "visible"), &CanvasItem::set_visible);
	ClassDB::bind_method(D_METHOD("is_visible"), &CanvasItem::is_visible);
	ClassDB::bind_method(D_METHOD("is_visible_in_tree"), &CanvasItem::is_visible_in_tree);
	ClassDB::bind_method(D_METHOD("show"), &CanvasItem::show);
	ClassDB::bind_method(D_METHOD("hide"), &CanvasItem::hide);

	ClassDB::bind_method(D_METHOD("queue_redraw"), &CanvasItem::queue_redraw);
	ClassDB::bind_method(D_METHOD("move_to_front"), &CanvasItem::move_to_front);

	ClassDB::bind_method(D_METHOD("set_as_top_level", "enable"), &CanvasItem::set_as_top_level);
	ClassDB::bind_method(D_METHOD("is_set_as_top_level"), &CanvasItem::is_set_as_top_level);

	ClassDB::bind_method(D_METHOD("set_light_mask", "light_mask"), &CanvasItem::set_light_mask);
	ClassDB::bind_method(D_METHOD("get_light_mask"), &CanvasItem::get_light_mask);

	ClassDB::bind_method(D_METHOD("set_modulate", "modulate"), &CanvasItem::set_modulate);
	ClassDB::bind_method(D_METHOD("get_modulate"), &CanvasItem::get_modulate);

	ClassDB::bind_method(D_METHOD("set_self_modulate", "self_modulate"), &CanvasItem::set_self_modulate);
	ClassDB::bind_method(D_METHOD("get_self_modulate"), &CanvasItem::get_self_modulate);

	ClassDB::bind_method(D_METHOD("set_z_index", "z_index"), &CanvasItem::set_z_index);
	ClassDB::bind_method(D_METHOD("get_z_index"), &CanvasItem::get_z_index);

	ClassDB::bind_method(D_METHOD("set_z_as_relative", "enable"), &CanvasItem::set_z_as_relative);
	ClassDB::bind_method(D_METHOD("is_z_relative"), &CanvasItem::is_z_relative);

	ClassDB::bind_method(D_METHOD("set_y_sort_enabled", "enabled"), &CanvasItem::set_y_sort_enabled);
	ClassDB::bind_method(D_METHOD("is_y_sort_enabled"), &CanvasItem::is_y_sort_enabled);

	ClassDB::bind_method(D_METHOD("set_draw_behind_parent", "enable"), &CanvasItem::set_draw_behind_parent);
	ClassDB::bind_method(D_METHOD("is_draw_behind_parent_enabled"), &CanvasItem::is_draw_behind_parent_enabled);

	ClassDB::bind_method(D_METHOD("draw_line", "from", "to", "color", "width", "antialiased"), &CanvasItem::draw_line, DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("draw_dashed_line", "from", "to", "color", "width", "dash", "aligned", "antialiased"), &CanvasItem::draw_dashed_line, DEFVAL(-1.0), DEFVAL(2.0), DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("draw_polyline", "points", "color", "width", "antialiased"), &CanvasItem::draw_polyline, DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("draw_polyline_colors", "points", "colors", "width", "antialiased"), &CanvasItem::draw_polyline_colors, DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("draw_arc", "center", "radius", "start_angle", "end_angle", "point_count", "color", "width", "antialiased"), &CanvasItem::draw_arc, DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("draw_multiline", "points", "color", "width", "antialiased"), &CanvasItem::draw_multiline, DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("draw_multiline_colors", "points", "colors", "width", "antialiased"), &CanvasItem::draw_multiline_colors, DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("draw_rect", "rect", "color", "filled", "width", "antialiased"), &CanvasItem::draw_rect, DEFVAL(true), DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("draw_circle", "position", "radius", "color", "filled", "width", "antialiased"), &CanvasItem::draw_circle, DEFVAL(true), DEFVAL(-1.0), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("draw_texture", "texture", "position", "modulate"), &CanvasItem::draw_texture, DEFVAL(Color(1, 1, 1, 1)));
	ClassDB::bind_method(D_METHOD("draw_texture_rect", "texture", "rect", "tile", "modulate", "transpose"), &CanvasItem::draw_texture_rect, DEFVAL(Color(1, 1, 1, 1)), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("draw_texture_rect_region", "texture", "rect", "src_rect", "modulate", "transpose", "clip_uv"), &CanvasItem::draw_texture_rect_region, DEFVAL(Color(1, 1, 1, 1)), DEFVAL(false), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("draw_msdf_texture_rect_region", "texture", "rect", "src_rect", "modulate", "outline", "pixel_range", "scale"), &CanvasItem::draw_msdf_texture_rect_region, DEFVAL(Color(1, 1, 1, 1)), DEFVAL(0.0), DEFVAL(4.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("draw_lcd_texture_rect_region", "texture", "rect", "src_rect", "modulate"), &CanvasItem::draw_lcd_texture_rect_region, DEFVAL(Color(1, 1, 1, 1)));
	ClassDB::bind_method(D_METHOD("draw_style_box", "style_box", "rect"), &CanvasItem::draw_style_box);
	ClassDB::bind_method(D_METHOD("draw_primitive", "points", "colors", "uvs", "texture"), &CanvasItem::draw_primitive, DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_method(D_METHOD("draw_polygon", "points", "colors", "uvs", "texture"), &CanvasItem::draw_polygon, DEFVAL(PackedVector2Array()), DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_method(D_METHOD("draw_colored_polygon", "points", "color", "uvs", "texture"), &CanvasItem::draw_colored_polygon, DEFVAL(PackedVector2Array()), DEFVAL(Ref<Texture2D>()));
	ClassDB::bind_method(D_METHOD("draw_string", "font", "pos", "text", "alignment", "width", "font_size", "modulate", "justification_flags", "direction", "orientation"), &CanvasItem::draw_string, DEFVAL(HORIZONTAL_ALIGNMENT_LEFT), DEFVAL(-1), DEFVAL(Font::DEFAULT_FONT_SIZE), DEFVAL(Color(1.0, 1.0, 1.0)), DEFVAL(TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_WORD_BOUND), DEFVAL(TextServer::DIRECTION_AUTO), DEFVAL(TextServer::ORIENTATION_HORIZONTAL));
	ClassDB::bind_method(D_METHOD("draw_multiline_string", "font", "pos", "text", "alignment", "width", "font_size", "max_lines", "modulate", "brk_flags", "justification_flags", "direction", "orientation"), &CanvasItem::draw_multiline_string, DEFVAL(HORIZONTAL_ALIGNMENT_LEFT), DEFVAL(-1), DEFVAL(Font::DEFAULT_FONT_SIZE), DEFVAL(-1), DEFVAL(Color(1.0, 1.0, 1.0)), DEFVAL(TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND), DEFVAL(TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_WORD_BOUND), DEFVAL(TextServer::DIRECTION_AUTO), DEFVAL(TextServer::ORIENTATION_HORIZONTAL));
	ClassDB::bind_method(D_METHOD("draw_string_outline", "font", "pos", "text", "alignment", "width", "font_size", "size", "modulate", "justification_flags", "direction", "orientation"), &CanvasItem::draw_string_outline, DEFVAL(HORIZONTAL_ALIGNMENT_LEFT), DEFVAL(-1), DEFVAL(Font::DEFAULT_FONT_SIZE), DEFVAL(1), DEFVAL(Color(1.0, 1.0, 1.0)), DEFVAL(TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_WORD_BOUND), DEFVAL(TextServer::DIRECTION_AUTO), DEFVAL(TextServer::ORIENTATION_HORIZONTAL));
	ClassDB::bind_method(D_METHOD("draw_multiline_string_outline", "font", "pos", "text", "alignment", "width", "font_size", "max_lines", "size", "modulate", "brk_flags", "justification_flags", "direction", "orientation"), &CanvasItem::draw_multiline_string_outline, DEFVAL(HORIZONTAL_ALIGNMENT_LEFT), DEFVAL(-1), DEFVAL(Font::DEFAULT_FONT_SIZE), DEFVAL(-1), DEFVAL(1), DEFVAL(Color(1.0, 1.0, 1.0)), DEFVAL(TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND), DEFVAL(TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_WORD_BOUND), DEFVAL(TextServer::DIRECTION_AUTO), DEFVAL(TextServer::ORIENTATION_HORIZONTAL));
	ClassDB::bind_method(D_METHOD("draw_char", "font", "pos", "char", "font_size", "modulate"), &CanvasItem::draw_char, DEFVAL(Font::DEFAULT_FONT_SIZE), DEFVAL(Color(1.0, 1.0, 1.0)));
	ClassDB::bind_method(D_METHOD("draw_char_outline", "font", "pos", "char", "font_size", "size", "modulate"), &CanvasItem::draw_char_outline, DEFVAL(Font::DEFAULT_FONT_SIZE), DEFVAL(-1), DEFVAL(Color(1.0, 1.0, 1.0)));
	ClassDB::bind_method(D_METHOD("draw_mesh", "mesh", "texture", "transform", "modulate"), &CanvasItem::draw_mesh, DEFVAL(Transform2D()), DEFVAL(Color(1, 1, 1, 1)));
	ClassDB::bind_method(D_METHOD("draw_multimesh", "multimesh", "texture"), &CanvasItem::draw_multimesh);
	ClassDB::bind_method(D_METHOD("draw_set_transform", "position", "rotation", "scale"), &CanvasItem::draw_set_transform, DEFVAL(0.0), DEFVAL(Size2(1.0, 1.0)));
	ClassDB::bind_method(D_METHOD("draw_set_transform_matrix", "xform"), &CanvasItem::draw_set_transform_matrix);
	ClassDB::bind_method(D_METHOD("draw_animation_slice", "animation_length", "slice_begin", "slice_end", "offset"), &CanvasItem::draw_animation_slice, DEFVAL(0.0));
	ClassDB::bind_method(D_METHOD("draw_end_animation"), &CanvasItem::draw_end_animation);
	ClassDB::bind_method(D_METHOD("get_transform"), &CanvasItem::get_transform);
	ClassDB::bind_method(D_METHOD("get_global_transform"), &CanvasItem::get_global_transform);
	ClassDB::bind_method(D_METHOD("get_global_transform_with_canvas"), &CanvasItem::get_global_transform_with_canvas);
	ClassDB::bind_method(D_METHOD("get_viewport_transform"), &CanvasItem::get_viewport_transform);
	ClassDB::bind_method(D_METHOD("get_viewport_rect"), &CanvasItem::get_viewport_rect);
	ClassDB::bind_method(D_METHOD("get_canvas_transform"), &CanvasItem::get_canvas_transform);
	ClassDB::bind_method(D_METHOD("get_screen_transform"), &CanvasItem::get_screen_transform);
	ClassDB::bind_method(D_METHOD("get_local_mouse_position"), &CanvasItem::get_local_mouse_position);
	ClassDB::bind_method(D_METHOD("get_global_mouse_position"), &CanvasItem::get_global_mouse_position);
	ClassDB::bind_method(D_METHOD("get_canvas"), &CanvasItem::get_canvas);
	ClassDB::bind_method(D_METHOD("get_canvas_layer_node"), &CanvasItem::get_canvas_layer_node);
	ClassDB::bind_method(D_METHOD("get_world_2d"), &CanvasItem::get_world_2d);
	//ClassDB::bind_method(D_METHOD("get_viewport"),&CanvasItem::get_viewport);

	ClassDB::bind_method(D_METHOD("set_material", "material"), &CanvasItem::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &CanvasItem::get_material);

	ClassDB::bind_method(D_METHOD("set_instance_shader_parameter", "name", "value"), &CanvasItem::set_instance_shader_parameter);
	ClassDB::bind_method(D_METHOD("get_instance_shader_parameter", "name"), &CanvasItem::get_instance_shader_parameter);

	ClassDB::bind_method(D_METHOD("set_use_parent_material", "enable"), &CanvasItem::set_use_parent_material);
	ClassDB::bind_method(D_METHOD("get_use_parent_material"), &CanvasItem::get_use_parent_material);

	ClassDB::bind_method(D_METHOD("set_notify_local_transform", "enable"), &CanvasItem::set_notify_local_transform);
	ClassDB::bind_method(D_METHOD("is_local_transform_notification_enabled"), &CanvasItem::is_local_transform_notification_enabled);

	ClassDB::bind_method(D_METHOD("set_notify_transform", "enable"), &CanvasItem::set_notify_transform);
	ClassDB::bind_method(D_METHOD("is_transform_notification_enabled"), &CanvasItem::is_transform_notification_enabled);

	ClassDB::bind_method(D_METHOD("force_update_transform"), &CanvasItem::force_update_transform);

	ClassDB::bind_method(D_METHOD("make_canvas_position_local", "viewport_point"), &CanvasItem::make_canvas_position_local);
	ClassDB::bind_method(D_METHOD("make_input_local", "event"), &CanvasItem::make_input_local);

	ClassDB::bind_method(D_METHOD("set_visibility_layer", "layer"), &CanvasItem::set_visibility_layer);
	ClassDB::bind_method(D_METHOD("get_visibility_layer"), &CanvasItem::get_visibility_layer);
	ClassDB::bind_method(D_METHOD("set_visibility_layer_bit", "layer", "enabled"), &CanvasItem::set_visibility_layer_bit);
	ClassDB::bind_method(D_METHOD("get_visibility_layer_bit", "layer"), &CanvasItem::get_visibility_layer_bit);

	ClassDB::bind_method(D_METHOD("set_texture_filter", "mode"), &CanvasItem::set_texture_filter);
	ClassDB::bind_method(D_METHOD("get_texture_filter"), &CanvasItem::get_texture_filter);

	ClassDB::bind_method(D_METHOD("set_texture_repeat", "mode"), &CanvasItem::set_texture_repeat);
	ClassDB::bind_method(D_METHOD("get_texture_repeat"), &CanvasItem::get_texture_repeat);

	ClassDB::bind_method(D_METHOD("set_clip_children_mode", "mode"), &CanvasItem::set_clip_children_mode);
	ClassDB::bind_method(D_METHOD("get_clip_children_mode"), &CanvasItem::get_clip_children_mode);

	GDVIRTUAL_BIND(_draw);

	ADD_GROUP("Visibility", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visible"), "set_visible", "is_visible");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "modulate"), "set_modulate", "get_modulate");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "self_modulate"), "set_self_modulate", "get_self_modulate");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_behind_parent"), "set_draw_behind_parent", "is_draw_behind_parent_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "top_level"), "set_as_top_level", "is_set_as_top_level");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "clip_children", PROPERTY_HINT_ENUM, "Disabled,Clip Only,Clip + Draw"), "set_clip_children_mode", "get_clip_children_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "light_mask", PROPERTY_HINT_LAYERS_2D_RENDER), "set_light_mask", "get_light_mask");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visibility_layer", PROPERTY_HINT_LAYERS_2D_RENDER), "set_visibility_layer", "get_visibility_layer");

	ADD_GROUP("Ordering", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "z_index", PROPERTY_HINT_RANGE, itos(RS::CANVAS_ITEM_Z_MIN) + "," + itos(RS::CANVAS_ITEM_Z_MAX) + ",1"), "set_z_index", "get_z_index");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "z_as_relative"), "set_z_as_relative", "is_z_relative");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "y_sort_enabled"), "set_y_sort_enabled", "is_y_sort_enabled");

	ADD_GROUP("Texture", "texture_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_filter", PROPERTY_HINT_ENUM, "Inherit,Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_texture_filter", "get_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_repeat", PROPERTY_HINT_ENUM, "Inherit,Disabled,Enabled,Mirror"), "set_texture_repeat", "get_texture_repeat");

	ADD_GROUP("Material", "");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "CanvasItemMaterial,ShaderMaterial"), "set_material", "get_material");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_parent_material"), "set_use_parent_material", "get_use_parent_material");
	// ADD_PROPERTY(PropertyInfo(Variant::BOOL,"transform/notify"),"set_transform_notify","is_transform_notify_enabled");

	ADD_SIGNAL(MethodInfo("draw"));
	ADD_SIGNAL(MethodInfo("visibility_changed"));
	ADD_SIGNAL(MethodInfo("hidden"));
	ADD_SIGNAL(MethodInfo("item_rect_changed"));

	BIND_CONSTANT(NOTIFICATION_TRANSFORM_CHANGED);
	BIND_CONSTANT(NOTIFICATION_LOCAL_TRANSFORM_CHANGED);
	BIND_CONSTANT(NOTIFICATION_DRAW);
	BIND_CONSTANT(NOTIFICATION_VISIBILITY_CHANGED);
	BIND_CONSTANT(NOTIFICATION_ENTER_CANVAS);
	BIND_CONSTANT(NOTIFICATION_EXIT_CANVAS);
	BIND_CONSTANT(NOTIFICATION_WORLD_2D_CHANGED);

	BIND_ENUM_CONSTANT(TEXTURE_FILTER_PARENT_NODE);
	BIND_ENUM_CONSTANT(TEXTURE_FILTER_NEAREST);
	BIND_ENUM_CONSTANT(TEXTURE_FILTER_LINEAR);
	BIND_ENUM_CONSTANT(TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	BIND_ENUM_CONSTANT(TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
	BIND_ENUM_CONSTANT(TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC);
	BIND_ENUM_CONSTANT(TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC);
	BIND_ENUM_CONSTANT(TEXTURE_FILTER_MAX);

	BIND_ENUM_CONSTANT(TEXTURE_REPEAT_PARENT_NODE);
	BIND_ENUM_CONSTANT(TEXTURE_REPEAT_DISABLED);
	BIND_ENUM_CONSTANT(TEXTURE_REPEAT_ENABLED);
	BIND_ENUM_CONSTANT(TEXTURE_REPEAT_MIRROR);
	BIND_ENUM_CONSTANT(TEXTURE_REPEAT_MAX);

	BIND_ENUM_CONSTANT(CLIP_CHILDREN_DISABLED);
	BIND_ENUM_CONSTANT(CLIP_CHILDREN_ONLY);
	BIND_ENUM_CONSTANT(CLIP_CHILDREN_AND_DRAW);
	BIND_ENUM_CONSTANT(CLIP_CHILDREN_MAX);
}

Transform2D CanvasItem::get_canvas_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	ERR_FAIL_COND_V(!is_inside_tree(), Transform2D());

	if (canvas_layer) {
		return canvas_layer->get_final_transform();
	} else if (Object::cast_to<CanvasItem>(get_parent())) {
		return Object::cast_to<CanvasItem>(get_parent())->get_canvas_transform();
	} else {
		return get_viewport()->get_canvas_transform();
	}
}

Transform2D CanvasItem::get_viewport_transform() const {
	ERR_READ_THREAD_GUARD_V(Transform2D());
	ERR_FAIL_COND_V(!is_inside_tree(), Transform2D());

	if (canvas_layer) {
		return get_viewport()->get_final_transform() * canvas_layer->get_final_transform();
	} else {
		return get_viewport()->get_final_transform() * get_viewport()->get_canvas_transform();
	}
}

void CanvasItem::set_notify_local_transform(bool p_enable) {
	ERR_THREAD_GUARD;
	notify_local_transform = p_enable;
}

bool CanvasItem::is_local_transform_notification_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return notify_local_transform;
}

void CanvasItem::set_notify_transform(bool p_enable) {
	ERR_THREAD_GUARD;
	if (notify_transform == p_enable) {
		return;
	}

	notify_transform = p_enable;

	if (notify_transform && is_inside_tree()) {
		// This ensures that invalid globals get resolved, so notifications can be received.
		_ALLOW_DISCARD_ get_global_transform();
	}
}

bool CanvasItem::is_transform_notification_enabled() const {
	ERR_READ_THREAD_GUARD_V(false);
	return notify_transform;
}

int CanvasItem::get_canvas_layer() const {
	ERR_READ_THREAD_GUARD_V(0);
	if (canvas_layer) {
		return canvas_layer->get_layer();
	} else {
		return 0;
	}
}

CanvasLayer *CanvasItem::get_canvas_layer_node() const {
	ERR_READ_THREAD_GUARD_V(nullptr);
	return canvas_layer;
}

void CanvasItem::set_visibility_layer(uint32_t p_visibility_layer) {
	ERR_THREAD_GUARD;
	visibility_layer = p_visibility_layer;
	RenderingServer::get_singleton()->canvas_item_set_visibility_layer(canvas_item, p_visibility_layer);
}

uint32_t CanvasItem::get_visibility_layer() const {
	ERR_READ_THREAD_GUARD_V(0);
	return visibility_layer;
}

void CanvasItem::set_visibility_layer_bit(uint32_t p_visibility_layer, bool p_enable) {
	ERR_THREAD_GUARD;
	ERR_FAIL_UNSIGNED_INDEX(p_visibility_layer, 32);
	if (p_enable) {
		set_visibility_layer(visibility_layer | (1 << p_visibility_layer));
	} else {
		set_visibility_layer(visibility_layer & (~(1 << p_visibility_layer)));
	}
}

bool CanvasItem::get_visibility_layer_bit(uint32_t p_visibility_layer) const {
	ERR_READ_THREAD_GUARD_V(false);
	ERR_FAIL_UNSIGNED_INDEX_V(p_visibility_layer, 32, false);
	return (visibility_layer & (1 << p_visibility_layer));
}

void CanvasItem::_refresh_texture_filter_cache() const {
	if (!is_inside_tree()) {
		return;
	}

	if (texture_filter == TEXTURE_FILTER_PARENT_NODE) {
		CanvasItem *parent_item = get_parent_item();
		if (parent_item) {
			texture_filter_cache = parent_item->texture_filter_cache;
		} else {
			texture_filter_cache = RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
		}
	} else {
		texture_filter_cache = RS::CanvasItemTextureFilter(texture_filter);
	}
}

void CanvasItem::_update_self_texture_filter(RS::CanvasItemTextureFilter p_texture_filter) {
	RS::get_singleton()->canvas_item_set_default_texture_filter(get_canvas_item(), p_texture_filter);
	queue_redraw();
}

void CanvasItem::_update_texture_filter_changed(bool p_propagate) {
	if (!is_inside_tree()) {
		return;
	}
	_refresh_texture_filter_cache();
	_update_self_texture_filter(texture_filter_cache);

	if (p_propagate) {
		for (CanvasItem *E : children_items) {
			if (!E->top_level && E->texture_filter == TEXTURE_FILTER_PARENT_NODE) {
				E->_update_texture_filter_changed(true);
			}
		}
	}
}

void CanvasItem::set_texture_filter(TextureFilter p_texture_filter) {
	ERR_MAIN_THREAD_GUARD; // Goes down in the tree, so only main thread can set.
	ERR_FAIL_INDEX(p_texture_filter, TEXTURE_FILTER_MAX);
	if (texture_filter == p_texture_filter) {
		return;
	}
	texture_filter = p_texture_filter;
	_update_texture_filter_changed(true);
	notify_property_list_changed();
}

CanvasItem::TextureFilter CanvasItem::get_texture_filter() const {
	ERR_READ_THREAD_GUARD_V(TEXTURE_FILTER_NEAREST);
	return texture_filter;
}

void CanvasItem::_refresh_texture_repeat_cache() const {
	if (!is_inside_tree()) {
		return;
	}

	if (texture_repeat == TEXTURE_REPEAT_PARENT_NODE) {
		CanvasItem *parent_item = get_parent_item();
		if (parent_item) {
			texture_repeat_cache = parent_item->texture_repeat_cache;
		} else {
			texture_repeat_cache = RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;
		}
	} else {
		texture_repeat_cache = RS::CanvasItemTextureRepeat(texture_repeat);
	}
}

void CanvasItem::_update_self_texture_repeat(RS::CanvasItemTextureRepeat p_texture_repeat) {
	RS::get_singleton()->canvas_item_set_default_texture_repeat(get_canvas_item(), p_texture_repeat);
	queue_redraw();
}

void CanvasItem::_update_texture_repeat_changed(bool p_propagate) {
	if (!is_inside_tree()) {
		return;
	}
	_refresh_texture_repeat_cache();
	_update_self_texture_repeat(texture_repeat_cache);

	if (p_propagate) {
		for (CanvasItem *E : children_items) {
			if (!E->top_level && E->texture_repeat == TEXTURE_REPEAT_PARENT_NODE) {
				E->_update_texture_repeat_changed(true);
			}
		}
	}
}

void CanvasItem::set_texture_repeat(TextureRepeat p_texture_repeat) {
	ERR_MAIN_THREAD_GUARD; // Goes down in the tree, so only main thread can set.
	ERR_FAIL_INDEX(p_texture_repeat, TEXTURE_REPEAT_MAX);
	if (texture_repeat == p_texture_repeat) {
		return;
	}
	texture_repeat = p_texture_repeat;
	_update_texture_repeat_changed(true);
	notify_property_list_changed();
}

void CanvasItem::set_clip_children_mode(ClipChildrenMode p_clip_mode) {
	ERR_THREAD_GUARD;
	ERR_FAIL_COND(p_clip_mode >= CLIP_CHILDREN_MAX);

	if (clip_children_mode == p_clip_mode) {
		return;
	}
	clip_children_mode = p_clip_mode;

	update_configuration_warnings();

	if (Object::cast_to<CanvasGroup>(this) != nullptr) {
		//avoid accidental bugs, make this not work on CanvasGroup
		return;
	}

	RS::get_singleton()->canvas_item_set_canvas_group_mode(get_canvas_item(), RS::CanvasGroupMode(clip_children_mode));
}
CanvasItem::ClipChildrenMode CanvasItem::get_clip_children_mode() const {
	ERR_READ_THREAD_GUARD_V(CLIP_CHILDREN_DISABLED);
	return clip_children_mode;
}

CanvasItem::TextureRepeat CanvasItem::get_texture_repeat() const {
	ERR_READ_THREAD_GUARD_V(TEXTURE_REPEAT_DISABLED);
	return texture_repeat;
}

CanvasItem::TextureFilter CanvasItem::get_texture_filter_in_tree() const {
	ERR_READ_THREAD_GUARD_V(TEXTURE_FILTER_NEAREST);
	_refresh_texture_filter_cache();
	return (TextureFilter)texture_filter_cache;
}

CanvasItem::TextureRepeat CanvasItem::get_texture_repeat_in_tree() const {
	ERR_READ_THREAD_GUARD_V(TEXTURE_REPEAT_DISABLED);
	_refresh_texture_repeat_cache();
	return (TextureRepeat)texture_repeat_cache;
}

CanvasItem::CanvasItem() :
		xform_change(this) {
	canvas_item = RenderingServer::get_singleton()->canvas_item_create();
}

CanvasItem::~CanvasItem() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RenderingServer::get_singleton()->free(canvas_item);
}

///////////////////////////////////////////////////////////////////

void CanvasTexture::set_diffuse_texture(const Ref<Texture2D> &p_diffuse) {
	ERR_FAIL_COND_MSG(Object::cast_to<CanvasTexture>(p_diffuse.ptr()) != nullptr, "Can't self-assign a CanvasTexture");
	if (diffuse_texture == p_diffuse) {
		return;
	}
	diffuse_texture = p_diffuse;

	RID tex_rid = diffuse_texture.is_valid() ? diffuse_texture->get_rid() : RID();
	RS::get_singleton()->canvas_texture_set_channel(canvas_texture, RS::CANVAS_TEXTURE_CHANNEL_DIFFUSE, tex_rid);
	emit_changed();
}
Ref<Texture2D> CanvasTexture::get_diffuse_texture() const {
	return diffuse_texture;
}

void CanvasTexture::set_normal_texture(const Ref<Texture2D> &p_normal) {
	ERR_FAIL_COND_MSG(Object::cast_to<CanvasTexture>(p_normal.ptr()) != nullptr, "Can't self-assign a CanvasTexture");
	if (normal_texture == p_normal) {
		return;
	}
	normal_texture = p_normal;
	RID tex_rid = normal_texture.is_valid() ? normal_texture->get_rid() : RID();
	RS::get_singleton()->canvas_texture_set_channel(canvas_texture, RS::CANVAS_TEXTURE_CHANNEL_NORMAL, tex_rid);
	emit_changed();
}
Ref<Texture2D> CanvasTexture::get_normal_texture() const {
	return normal_texture;
}

void CanvasTexture::set_specular_texture(const Ref<Texture2D> &p_specular) {
	ERR_FAIL_COND_MSG(Object::cast_to<CanvasTexture>(p_specular.ptr()) != nullptr, "Can't self-assign a CanvasTexture");
	if (specular_texture == p_specular) {
		return;
	}
	specular_texture = p_specular;
	RID tex_rid = specular_texture.is_valid() ? specular_texture->get_rid() : RID();
	RS::get_singleton()->canvas_texture_set_channel(canvas_texture, RS::CANVAS_TEXTURE_CHANNEL_SPECULAR, tex_rid);
	emit_changed();
}

Ref<Texture2D> CanvasTexture::get_specular_texture() const {
	return specular_texture;
}

void CanvasTexture::set_specular_color(const Color &p_color) {
	if (specular == p_color) {
		return;
	}
	specular = p_color;
	RS::get_singleton()->canvas_texture_set_shading_parameters(canvas_texture, specular, shininess);
	emit_changed();
}

Color CanvasTexture::get_specular_color() const {
	return specular;
}

void CanvasTexture::set_specular_shininess(real_t p_shininess) {
	if (shininess == p_shininess) {
		return;
	}
	shininess = p_shininess;
	RS::get_singleton()->canvas_texture_set_shading_parameters(canvas_texture, specular, shininess);
	emit_changed();
}

real_t CanvasTexture::get_specular_shininess() const {
	return shininess;
}

void CanvasTexture::set_texture_filter(CanvasItem::TextureFilter p_filter) {
	if (texture_filter == p_filter) {
		return;
	}
	texture_filter = p_filter;
	RS::get_singleton()->canvas_texture_set_texture_filter(canvas_texture, RS::CanvasItemTextureFilter(p_filter));
	emit_changed();
}
CanvasItem::TextureFilter CanvasTexture::get_texture_filter() const {
	return texture_filter;
}

void CanvasTexture::set_texture_repeat(CanvasItem::TextureRepeat p_repeat) {
	if (texture_repeat == p_repeat) {
		return;
	}
	texture_repeat = p_repeat;
	RS::get_singleton()->canvas_texture_set_texture_repeat(canvas_texture, RS::CanvasItemTextureRepeat(p_repeat));
	emit_changed();
}
CanvasItem::TextureRepeat CanvasTexture::get_texture_repeat() const {
	return texture_repeat;
}

int CanvasTexture::get_width() const {
	if (diffuse_texture.is_valid()) {
		return diffuse_texture->get_width();
	} else {
		return 1;
	}
}
int CanvasTexture::get_height() const {
	if (diffuse_texture.is_valid()) {
		return diffuse_texture->get_height();
	} else {
		return 1;
	}
}

bool CanvasTexture::is_pixel_opaque(int p_x, int p_y) const {
	if (diffuse_texture.is_valid()) {
		return diffuse_texture->is_pixel_opaque(p_x, p_y);
	} else {
		return false;
	}
}

bool CanvasTexture::has_alpha() const {
	if (diffuse_texture.is_valid()) {
		return diffuse_texture->has_alpha();
	} else {
		return false;
	}
}

Ref<Image> CanvasTexture::get_image() const {
	if (diffuse_texture.is_valid()) {
		return diffuse_texture->get_image();
	} else {
		return Ref<Image>();
	}
}

RID CanvasTexture::get_rid() const {
	return canvas_texture;
}

void CanvasTexture::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_diffuse_texture", "texture"), &CanvasTexture::set_diffuse_texture);
	ClassDB::bind_method(D_METHOD("get_diffuse_texture"), &CanvasTexture::get_diffuse_texture);

	ClassDB::bind_method(D_METHOD("set_normal_texture", "texture"), &CanvasTexture::set_normal_texture);
	ClassDB::bind_method(D_METHOD("get_normal_texture"), &CanvasTexture::get_normal_texture);

	ClassDB::bind_method(D_METHOD("set_specular_texture", "texture"), &CanvasTexture::set_specular_texture);
	ClassDB::bind_method(D_METHOD("get_specular_texture"), &CanvasTexture::get_specular_texture);

	ClassDB::bind_method(D_METHOD("set_specular_color", "color"), &CanvasTexture::set_specular_color);
	ClassDB::bind_method(D_METHOD("get_specular_color"), &CanvasTexture::get_specular_color);

	ClassDB::bind_method(D_METHOD("set_specular_shininess", "shininess"), &CanvasTexture::set_specular_shininess);
	ClassDB::bind_method(D_METHOD("get_specular_shininess"), &CanvasTexture::get_specular_shininess);

	ClassDB::bind_method(D_METHOD("set_texture_filter", "filter"), &CanvasTexture::set_texture_filter);
	ClassDB::bind_method(D_METHOD("get_texture_filter"), &CanvasTexture::get_texture_filter);

	ClassDB::bind_method(D_METHOD("set_texture_repeat", "repeat"), &CanvasTexture::set_texture_repeat);
	ClassDB::bind_method(D_METHOD("get_texture_repeat"), &CanvasTexture::get_texture_repeat);

	ADD_GROUP("Diffuse", "diffuse_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "diffuse_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_diffuse_texture", "get_diffuse_texture");
	ADD_GROUP("NormalMap", "normal_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "normal_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_normal_texture", "get_normal_texture");
	ADD_GROUP("Specular", "specular_");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "specular_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_specular_texture", "get_specular_texture");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "specular_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_specular_color", "get_specular_color");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "specular_shininess", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_specular_shininess", "get_specular_shininess");
	ADD_GROUP("Texture", "texture_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_filter", PROPERTY_HINT_ENUM, "Inherit,Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_texture_filter", "get_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_repeat", PROPERTY_HINT_ENUM, "Inherit,Disabled,Enabled,Mirror"), "set_texture_repeat", "get_texture_repeat");
}

CanvasTexture::CanvasTexture() {
	canvas_texture = RS::get_singleton()->canvas_texture_create();
}
CanvasTexture::~CanvasTexture() {
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RS::get_singleton()->free(canvas_texture);
}
