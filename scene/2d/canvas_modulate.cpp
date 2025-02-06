/**************************************************************************/
/*  canvas_modulate.cpp                                                   */
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

#include "canvas_modulate.h"

void CanvasModulate::_on_in_canvas_visibility_changed(bool p_new_visibility) {
	RID canvas = get_canvas();
	StringName group_name = "_canvas_modulate_" + itos(canvas.get_id());

	ERR_FAIL_COND_MSG(p_new_visibility == is_in_group(group_name), vformat("CanvasModulate becoming %s in the canvas already %s in the modulate group. Buggy logic, please report.", p_new_visibility ? "visible" : "invisible", p_new_visibility ? "was" : "was not"));

	if (p_new_visibility) {
		bool has_active_canvas_modulate = get_tree()->has_group(group_name); // Group would be removed if empty; otherwise one CanvasModulate within must be active.
		add_to_group(group_name);
		if (!has_active_canvas_modulate) {
			is_active = true;
			RS::get_singleton()->canvas_set_modulate(canvas, color);
		}
	} else {
		remove_from_group(group_name);
		if (is_active) {
			is_active = false;
			CanvasModulate *new_active = Object::cast_to<CanvasModulate>(get_tree()->get_first_node_in_group(group_name));
			if (new_active) {
				new_active->is_active = true;
				RS::get_singleton()->canvas_set_modulate(canvas, new_active->color);
			} else {
				RS::get_singleton()->canvas_set_modulate(canvas, Color(1, 1, 1, 1));
			}
		}
	}

	update_configuration_info();
}

void CanvasModulate::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_CANVAS: {
			is_in_canvas = true;
			bool visible_in_tree = is_visible_in_tree();
			if (visible_in_tree) {
				_on_in_canvas_visibility_changed(true);
			}
			was_visible_in_tree = visible_in_tree;
		} break;

		case NOTIFICATION_EXIT_CANVAS: {
			is_in_canvas = false;
			if (was_visible_in_tree) {
				_on_in_canvas_visibility_changed(false);
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_in_canvas) {
				return;
			}

			bool visible_in_tree = is_visible_in_tree();
			if (visible_in_tree == was_visible_in_tree) {
				return;
			}

			_on_in_canvas_visibility_changed(visible_in_tree);

			was_visible_in_tree = visible_in_tree;
		} break;
	}
}

void CanvasModulate::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_color", "color"), &CanvasModulate::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &CanvasModulate::get_color);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
}

void CanvasModulate::set_color(const Color &p_color) {
	color = p_color;
	if (is_active) {
		RS::get_singleton()->canvas_set_modulate(get_canvas(), color);
	}
}

Color CanvasModulate::get_color() const {
	return color;
}

#ifdef TOOLS_ENABLED
Vector<ConfigurationInfo> CanvasModulate::get_configuration_info() const {
	Vector<ConfigurationInfo> infos = Node2D::get_configuration_info();

	if (is_in_canvas && is_visible_in_tree()) {
		List<Node *> nodes;
		get_tree()->get_nodes_in_group("_canvas_modulate_" + itos(get_canvas().get_id()), &nodes);

		if (nodes.size() > 1) {
			CONFIG_WARNING(RTR("Only one visible CanvasModulate is allowed per canvas.\nWhen there are more than one, only one of them will be active. Which one is undefined."));
		}
	}

	return infos;
}
#endif

CanvasModulate::CanvasModulate() {
}

CanvasModulate::~CanvasModulate() {
}
