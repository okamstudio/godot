/**************************************************************************/
/*  test_viewport.h                                                       */
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

#ifndef TEST_VIEWPORT_H
#define TEST_VIEWPORT_H

#include "scene/2d/physics/area_2d.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/gui/control.h"
#include "scene/gui/subviewport_container.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/window.h"
#include "scene/resources/2d/rectangle_shape_2d.h"
#include "servers/physics_server_2d_dummy.h"

#include "tests/test_macros.h"

namespace TestViewport {

class NotificationControlViewport : public Control {
	GDCLASS(NotificationControlViewport, Control);

protected:
	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_MOUSE_ENTER: {
				if (mouse_over) {
					invalid_order = true;
				}
				mouse_over = true;
			} break;

			case NOTIFICATION_MOUSE_EXIT: {
				if (!mouse_over) {
					invalid_order = true;
				}
				mouse_over = false;
			} break;

			case NOTIFICATION_MOUSE_ENTER_SELF: {
				if (mouse_over_self) {
					invalid_order = true;
				}
				mouse_over_self = true;
			} break;

			case NOTIFICATION_MOUSE_EXIT_SELF: {
				if (!mouse_over_self) {
					invalid_order = true;
				}
				mouse_over_self = false;
			} break;
		}
	}

public:
	bool mouse_over = false;
	bool mouse_over_self = false;
	bool invalid_order = false;
};

// `NotificationControlViewport`-derived class that additionally
// - allows start Dragging
// - stores mouse information of last event
class DragStart : public NotificationControlViewport {
	GDCLASS(DragStart, NotificationControlViewport);

public:
	MouseButton last_mouse_button;
	Point2i last_mouse_move_position;
	StringName drag_data_name = SNAME("Drag Data");

	virtual Variant get_drag_data(const Point2 &p_point) override {
		return drag_data_name;
	}

	virtual void gui_input(const Ref<InputEvent> &p_event) override {
		Ref<InputEventMouseButton> mb = p_event;
		if (mb.is_valid()) {
			last_mouse_button = mb->get_button_index();
			return;
		}

		Ref<InputEventMouseMotion> mm = p_event;
		if (mm.is_valid()) {
			last_mouse_move_position = mm->get_position();
			return;
		}
	}
};

// `NotificationControlViewport`-derived class that acts as a Drag and Drop target.
class DragTarget : public NotificationControlViewport {
	GDCLASS(DragTarget, NotificationControlViewport);

protected:
	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_DRAG_BEGIN: {
				during_drag = true;
			} break;

			case NOTIFICATION_DRAG_END: {
				during_drag = false;
			} break;
		}
	}

public:
	Variant drag_data;
	bool valid_drop = false;
	bool during_drag = false;
	virtual bool can_drop_data(const Point2 &p_point, const Variant &p_data) const override {
		StringName string_data = p_data;
		// Verify drag data is compatible.
		if (string_data != SNAME("Drag Data")) {
			return false;
		}
		// Only the left half is droppable area.
		if (p_point.x * 2 > get_size().x) {
			return false;
		}
		return true;
	}

	virtual void drop_data(const Point2 &p_point, const Variant &p_data) override {
		drag_data = p_data;
		valid_drop = true;
	}
};

TEST_CASE("[SceneTree][Viewport] Controls and InputEvent handling") {
	DragStart *node_a = memnew(DragStart);
	NotificationControlViewport *node_b = memnew(NotificationControlViewport);
	Node2D *node_c = memnew(Node2D);
	DragTarget *node_d = memnew(DragTarget);
	NotificationControlViewport *node_e = memnew(NotificationControlViewport);
	Node *node_f = memnew(Node);
	NotificationControlViewport *node_g = memnew(NotificationControlViewport);
	NotificationControlViewport *node_h = memnew(NotificationControlViewport);
	NotificationControlViewport *node_i = memnew(NotificationControlViewport);
	NotificationControlViewport *node_j = memnew(NotificationControlViewport);

	node_a->set_name(SNAME("NodeA"));
	node_b->set_name(SNAME("NodeB"));
	node_c->set_name(SNAME("NodeC"));
	node_d->set_name(SNAME("NodeD"));
	node_e->set_name(SNAME("NodeE"));
	node_f->set_name(SNAME("NodeF"));
	node_g->set_name(SNAME("NodeG"));
	node_h->set_name(SNAME("NodeH"));
	node_i->set_name(SNAME("NodeI"));
	node_j->set_name(SNAME("NodeJ"));

	node_a->set_position(Point2i(0, 0));
	node_b->set_position(Point2i(10, 10));
	node_c->set_position(Point2i(0, 0));
	node_d->set_position(Point2i(10, 10));
	node_e->set_position(Point2i(10, 100));
	node_g->set_position(Point2i(10, 100));
	node_h->set_position(Point2i(10, 120));
	node_i->set_position(Point2i(2, 0));
	node_j->set_position(Point2i(2, 0));
	node_a->set_size(Point2i(30, 30));
	node_b->set_size(Point2i(30, 30));
	node_d->set_size(Point2i(30, 30));
	node_e->set_size(Point2i(10, 10));
	node_g->set_size(Point2i(10, 10));
	node_h->set_size(Point2i(10, 10));
	node_i->set_size(Point2i(10, 10));
	node_j->set_size(Point2i(10, 10));
	node_a->set_focus_mode(Control::FOCUS_CLICK);
	node_b->set_focus_mode(Control::FOCUS_CLICK);
	node_d->set_focus_mode(Control::FOCUS_CLICK);
	node_e->set_focus_mode(Control::FOCUS_CLICK);
	node_g->set_focus_mode(Control::FOCUS_CLICK);
	node_h->set_focus_mode(Control::FOCUS_CLICK);
	node_i->set_focus_mode(Control::FOCUS_CLICK);
	node_j->set_focus_mode(Control::FOCUS_CLICK);
	Window *root = SceneTree::get_singleton()->get_root();
	DisplayServerMock *DS = (DisplayServerMock *)(DisplayServer::get_singleton());

	// Scene tree:
	// - root
	//   - a (Control)
	//   - b (Control)
	//     - c (Node2D)
	//       - d (Control)
	//   - e (Control)
	//     - f (Node)
	//       - g (Control)
	//   - h (Control)
	//     - i (Control)
	//       - j (Control)
	root->add_child(node_a);
	root->add_child(node_b);
	node_b->add_child(node_c);
	node_c->add_child(node_d);
	root->add_child(node_e);
	node_e->add_child(node_f);
	node_f->add_child(node_g);
	root->add_child(node_h);
	node_h->add_child(node_i);
	node_i->add_child(node_j);

	Point2i on_a = Point2i(5, 5);
	Point2i on_b = Point2i(15, 15);
	Point2i on_d = Point2i(25, 25);
	Point2i on_e = Point2i(15, 105);
	Point2i on_g = Point2i(15, 105);
	Point2i on_i = Point2i(13, 125);
	Point2i on_j = Point2i(15, 125);
	Point2i on_background = Point2i(500, 500);
	Point2i on_outside = Point2i(-1, -1);

	// Unit tests for Viewport::gui_find_control and Viewport::_gui_find_control_at_pos
	SUBCASE("[VIEWPORT][GuiFindControl] Finding Controls at a Viewport-position") {
		// FIXME: It is extremely difficult to create a situation where the Control has a zero determinant.
		// Leaving that if-branch untested.

		SUBCASE("[VIEWPORT][GuiFindControl] Basic position tests") {
			CHECK(root->gui_find_control(on_a) == node_a);
			CHECK(root->gui_find_control(on_b) == node_b);
			CHECK(root->gui_find_control(on_d) == node_d);
			CHECK(root->gui_find_control(on_e) == node_g); // Node F makes G a Root Control at the same position as E
			CHECK(root->gui_find_control(on_g) == node_g);
			CHECK_FALSE(root->gui_find_control(on_background));
		}

		SUBCASE("[VIEWPORT][GuiFindControl] Invisible nodes are not considered as results.") {
			// Non-Root Control
			node_d->hide();
			CHECK(root->gui_find_control(on_d) == node_b);
			// Root Control
			node_b->hide();
			CHECK(root->gui_find_control(on_b) == node_a);
		}

		SUBCASE("[VIEWPORT][GuiFindControl] Root Control with CanvasItem as parent is affected by parent's transform.") {
			node_b->remove_child(node_c);
			node_c->set_position(Point2i(50, 50));
			root->add_child(node_c);
			CHECK(root->gui_find_control(Point2i(65, 65)) == node_d);
		}

		SUBCASE("[VIEWPORT][GuiFindControl] Control Contents Clipping clips accessible position of children.") {
			CHECK_FALSE(node_b->is_clipping_contents());
			CHECK(root->gui_find_control(on_d + Point2i(20, 20)) == node_d);
			node_b->set_clip_contents(true);
			CHECK(root->gui_find_control(on_d) == node_d);
			CHECK_FALSE(root->gui_find_control(on_d + Point2i(20, 20)));
		}

		SUBCASE("[VIEWPORT][GuiFindControl] Top Level Control as descendant of CanvasItem isn't affected by parent's transform.") {
			CHECK(root->gui_find_control(on_d + Point2i(20, 20)) == node_d);
			node_d->set_as_top_level(true);
			CHECK_FALSE(root->gui_find_control(on_d + Point2i(20, 20)));
			CHECK(root->gui_find_control(on_b) == node_d);
		}
	}

	SUBCASE("[Viewport][GuiInputEvent] nullptr as argument doesn't lead to a crash.") {
		ERR_PRINT_OFF;
		root->push_input(nullptr);
		ERR_PRINT_ON;
	}

	// Unit tests for Viewport::_gui_input_event (Mouse Buttons)
	SUBCASE("[Viewport][GuiInputEvent] Mouse Button Down/Up.") {
		SUBCASE("[Viewport][GuiInputEvent] Mouse Button Control Focus Change.") {
			SUBCASE("[Viewport][GuiInputEvent] Grab Focus while no Control has focus.") {
				CHECK_FALSE(root->gui_get_focus_owner());

				// Click on A
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK(node_a->has_focus());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			}

			SUBCASE("[Viewport][GuiInputEvent] Grab Focus from other Control.") {
				node_a->grab_focus();
				CHECK(node_a->has_focus());

				// Click on D
				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK(node_d->has_focus());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			}

			SUBCASE("[Viewport][GuiInputEvent] Non-CanvasItem breaks Transform hierarchy.") {
				CHECK_FALSE(root->gui_get_focus_owner());

				// Click on G absolute coordinates
				SEND_GUI_MOUSE_BUTTON_EVENT(Point2i(15, 105), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK(node_g->has_focus());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(Point2i(15, 105), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			}

			SUBCASE("[Viewport][GuiInputEvent] No Focus change when clicking in background.") {
				CHECK_FALSE(root->gui_get_focus_owner());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_get_focus_owner());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);

				node_a->grab_focus();
				CHECK(node_a->has_focus());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_a->has_focus());
			}

			SUBCASE("[Viewport][GuiInputEvent] Mouse Button No Focus Steal while other Mouse Button is pressed.") {
				CHECK_FALSE(root->gui_get_focus_owner());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK(node_a->has_focus());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_b, MouseButton::RIGHT, (int)MouseButtonMask::LEFT | (int)MouseButtonMask::RIGHT, Key::NONE);
				CHECK(node_a->has_focus());

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_b, MouseButton::RIGHT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_b, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_a->has_focus());
			}

			SUBCASE("[Viewport][GuiInputEvent] Allow Focus Steal with LMB while other Mouse Button is held down and was initially pressed without being over a Control.") {
				// TODO: Not sure, if this is intended behavior, but this is an edge case.
				CHECK_FALSE(root->gui_get_focus_owner());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_background, MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
				CHECK_FALSE(root->gui_get_focus_owner());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, (int)MouseButtonMask::LEFT | (int)MouseButtonMask::RIGHT, Key::NONE);
				CHECK(node_a->has_focus());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::RIGHT, Key::NONE);
				CHECK(node_a->has_focus());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_b, MouseButton::LEFT, (int)MouseButtonMask::LEFT | (int)MouseButtonMask::RIGHT, Key::NONE);
				CHECK(node_b->has_focus());

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::RIGHT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::RIGHT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_b->has_focus());
			}

			SUBCASE("[Viewport][GuiInputEvent] Ignore Focus from Mouse Buttons when mouse-filter is set to ignore.") {
				node_d->grab_focus();
				node_d->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
				CHECK(node_d->has_focus());

				// Click on overlapping area B&D.
				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK(node_b->has_focus());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			}

			SUBCASE("[Viewport][GuiInputEvent] RMB doesn't grab focus.") {
				node_a->grab_focus();
				CHECK(node_a->has_focus());

				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::RIGHT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_a->has_focus());
			}

			SUBCASE("[Viewport][GuiInputEvent] LMB on unfocusable Control doesn't grab focus.") {
				CHECK_FALSE(node_g->has_focus());
				node_g->set_focus_mode(Control::FOCUS_NONE);

				SEND_GUI_MOUSE_BUTTON_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(node_g->has_focus());

				// Now verify the opposite with FOCUS_CLICK
				node_g->set_focus_mode(Control::FOCUS_CLICK);
				SEND_GUI_MOUSE_BUTTON_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_g->has_focus());
				node_g->set_focus_mode(Control::FOCUS_CLICK);
			}

			SUBCASE("[Viewport][GuiInputEvent] Signal 'gui_focus_changed' is only emitted if a previously unfocused Control grabs focus.") {
				SIGNAL_WATCH(root, SNAME("gui_focus_changed"));
				Array node_array;
				node_array.push_back(node_a);
				Array signal_args;
				signal_args.push_back(node_array);

				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				SIGNAL_CHECK(SNAME("gui_focus_changed"), signal_args);

				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_a->has_focus());
				SIGNAL_CHECK_FALSE(SNAME("gui_focus_changed"));

				SIGNAL_UNWATCH(root, SNAME("gui_focus_changed"));
			}

			SUBCASE("[Viewport][GuiInputEvent] Focus Propagation to parent items.") {
				SUBCASE("[Viewport][GuiInputEvent] Unfocusable Control with MOUSE_FILTER_PASS propagates focus to parent CanvasItem.") {
					node_d->set_focus_mode(Control::FOCUS_NONE);
					node_d->set_mouse_filter(Control::MOUSE_FILTER_PASS);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_d + Point2i(20, 20), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					CHECK(node_b->has_focus());
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d + Point2i(20, 20), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);

					// Verify break condition for Root Control.
					node_a->set_focus_mode(Control::FOCUS_NONE);
					node_a->set_mouse_filter(Control::MOUSE_FILTER_PASS);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK(node_b->has_focus());
				}

				SUBCASE("[Viewport][GuiInputEvent] Top Level CanvasItem stops focus propagation.") {
					node_d->set_focus_mode(Control::FOCUS_NONE);
					node_d->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					node_c->set_as_top_level(true);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_b, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_b, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK_FALSE(root->gui_get_focus_owner());

					node_d->set_focus_mode(Control::FOCUS_CLICK);
					SEND_GUI_MOUSE_BUTTON_EVENT(on_b, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_b, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK(node_d->has_focus());
				}
			}
		}

		SUBCASE("[Viewport][GuiInputEvent] Process-Mode affects, if GUI Mouse Button Events are processed.") {
			node_a->last_mouse_button = MouseButton::NONE;
			node_a->set_process_mode(Node::PROCESS_MODE_DISABLED);
			SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->last_mouse_button == MouseButton::NONE);

			// Now verify that with allowed processing the event is processed.
			node_a->set_process_mode(Node::PROCESS_MODE_ALWAYS);
			SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
			SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->last_mouse_button == MouseButton::LEFT);
		}
	}

	// Unit tests for Viewport::_gui_input_event (Mouse Motion)
	SUBCASE("[Viewport][GuiInputEvent] Mouse Motion") {
		// FIXME: Tooltips are not yet tested. They likely require an internal clock.

		SUBCASE("[Viewport][GuiInputEvent] Mouse Motion changes the Control that it is over.") {
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_a->mouse_over);
			CHECK_FALSE(node_a->mouse_over_self);

			// Move over Control.
			SEND_GUI_MOUSE_MOTION_EVENT(on_a, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->mouse_over);
			CHECK(node_a->mouse_over_self);

			// No change.
			SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(1, 1), MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->mouse_over);
			CHECK(node_a->mouse_over_self);

			// Move over other Control.
			SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_a->mouse_over);
			CHECK_FALSE(node_a->mouse_over_self);
			CHECK(node_d->mouse_over);
			CHECK(node_d->mouse_over_self);

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_d->mouse_over);
			CHECK_FALSE(node_d->mouse_over_self);

			CHECK_FALSE(node_a->invalid_order);
			CHECK_FALSE(node_d->invalid_order);
		}

		SUBCASE("[Viewport][GuiInputEvent] Mouse Enter/Exit notification propagation.") {
			node_d->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			node_g->set_mouse_filter(Control::MOUSE_FILTER_PASS);

			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_b->mouse_over);
			CHECK_FALSE(node_b->mouse_over_self);
			CHECK_FALSE(node_d->mouse_over);
			CHECK_FALSE(node_d->mouse_over_self);

			// Move to Control node_d. node_b receives mouse over since it is only separated by a CanvasItem.
			SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_b->mouse_over);
			CHECK_FALSE(node_b->mouse_over_self);
			CHECK(node_d->mouse_over);
			CHECK(node_d->mouse_over_self);

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_b->mouse_over);
			CHECK_FALSE(node_b->mouse_over_self);
			CHECK_FALSE(node_d->mouse_over);
			CHECK_FALSE(node_d->mouse_over_self);

			CHECK_FALSE(node_e->mouse_over);
			CHECK_FALSE(node_e->mouse_over_self);
			CHECK_FALSE(node_g->mouse_over);
			CHECK_FALSE(node_g->mouse_over_self);

			// Move to Control node_g. node_g receives mouse over but node_e does not since it is separated by a non-CanvasItem.
			SEND_GUI_MOUSE_MOTION_EVENT(on_g, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_e->mouse_over);
			CHECK_FALSE(node_e->mouse_over_self);
			CHECK(node_g->mouse_over);
			CHECK(node_g->mouse_over_self);

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_e->mouse_over);
			CHECK_FALSE(node_e->mouse_over_self);
			CHECK_FALSE(node_g->mouse_over);
			CHECK_FALSE(node_g->mouse_over_self);

			CHECK_FALSE(node_b->invalid_order);
			CHECK_FALSE(node_d->invalid_order);
			CHECK_FALSE(node_e->invalid_order);
			CHECK_FALSE(node_g->invalid_order);

			node_d->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_g->set_mouse_filter(Control::MOUSE_FILTER_STOP);
		}

		SUBCASE("[Viewport][GuiInputEvent] Mouse Enter/Exit notification propagation when moving into child.") {
			SIGNAL_WATCH(node_i, SceneStringName(mouse_entered));
			SIGNAL_WATCH(node_i, SceneStringName(mouse_exited));
			Array signal_args;
			signal_args.push_back(Array());

			node_j->set_mouse_filter(Control::MOUSE_FILTER_PASS);

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);

			// Move to Control node_i.
			SEND_GUI_MOUSE_MOTION_EVENT(on_i, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_i->mouse_over);
			CHECK(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);
			SIGNAL_CHECK(SceneStringName(mouse_entered), signal_args);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Move to child Control node_j. node_i should not receive any new Mouse Enter signals.
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Move to parent Control node_i. node_i should not receive any new Mouse Enter signals.
			SEND_GUI_MOUSE_MOTION_EVENT(on_i, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_i->mouse_over);
			CHECK(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK(SceneStringName(mouse_exited), signal_args);

			CHECK_FALSE(node_i->invalid_order);
			CHECK_FALSE(node_j->invalid_order);

			node_j->set_mouse_filter(Control::MOUSE_FILTER_STOP);

			SIGNAL_UNWATCH(node_i, SceneStringName(mouse_entered));
			SIGNAL_UNWATCH(node_i, SceneStringName(mouse_exited));
		}

		SUBCASE("[Viewport][GuiInputEvent] Mouse Enter/Exit notification propagation with top level.") {
			node_c->set_as_top_level(true);
			node_i->set_as_top_level(true);
			node_c->set_position(node_b->get_global_position());
			node_i->set_position(node_h->get_global_position());
			node_d->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			node_i->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_PASS);

			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_b->mouse_over);
			CHECK_FALSE(node_b->mouse_over_self);
			CHECK_FALSE(node_d->mouse_over);
			CHECK_FALSE(node_d->mouse_over_self);

			// Move to Control node_d. node_b does not receive mouse over since node_c is top level.
			SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_b->mouse_over);
			CHECK_FALSE(node_b->mouse_over_self);
			CHECK(node_d->mouse_over);
			CHECK(node_d->mouse_over_self);

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_b->mouse_over);
			CHECK_FALSE(node_b->mouse_over_self);
			CHECK_FALSE(node_d->mouse_over);
			CHECK_FALSE(node_d->mouse_over_self);

			CHECK_FALSE(node_g->mouse_over);
			CHECK_FALSE(node_g->mouse_over_self);
			CHECK_FALSE(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);

			// Move to Control node_j. node_h does not receive mouse over since node_i is top level.
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);

			CHECK_FALSE(node_b->invalid_order);
			CHECK_FALSE(node_d->invalid_order);
			CHECK_FALSE(node_e->invalid_order);
			CHECK_FALSE(node_h->invalid_order);
			CHECK_FALSE(node_i->invalid_order);
			CHECK_FALSE(node_j->invalid_order);

			node_c->set_as_top_level(false);
			node_i->set_as_top_level(false);
			node_c->set_position(Point2i(0, 0));
			node_i->set_position(Point2i(0, 0));
			node_d->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_i->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_STOP);
		}

		SUBCASE("[Viewport][GuiInputEvent] Mouse Enter/Exit notification propagation with mouse filter stop.") {
			node_i->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_PASS);

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);

			// Move to Control node_j. node_h does not receive mouse over since node_i is MOUSE_FILTER_STOP.
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);

			CHECK_FALSE(node_h->invalid_order);
			CHECK_FALSE(node_i->invalid_order);
			CHECK_FALSE(node_j->invalid_order);

			node_i->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_STOP);
		}

		SUBCASE("[Viewport][GuiInputEvent] Mouse Enter/Exit notification propagation with mouse filter ignore.") {
			node_i->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_PASS);

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);

			// Move to Control node_j. node_i does not receive mouse over since node_i is MOUSE_FILTER_IGNORE.
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);

			// Move to background.
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			CHECK_FALSE(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);

			CHECK_FALSE(node_h->invalid_order);
			CHECK_FALSE(node_i->invalid_order);
			CHECK_FALSE(node_j->invalid_order);

			node_i->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_STOP);
		}

		SUBCASE("[Viewport][GuiInputEvent] Mouse Enter/Exit notification when changing top level.") {
			SIGNAL_WATCH(node_i, SceneStringName(mouse_entered));
			SIGNAL_WATCH(node_i, SceneStringName(mouse_exited));
			Array signal_args;
			signal_args.push_back(Array());

			node_d->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			node_i->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_PASS);

			// Move to Control node_d.
			SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_b->mouse_over);
			CHECK_FALSE(node_b->mouse_over_self);
			CHECK(node_d->mouse_over);
			CHECK(node_d->mouse_over_self);

			// Change node_c to be top level. node_b should receive Mouse Exit.
			node_c->set_as_top_level(true);
			CHECK_FALSE(node_b->mouse_over);
			CHECK_FALSE(node_b->mouse_over_self);
			CHECK(node_d->mouse_over);
			CHECK(node_d->mouse_over_self);

			// Change node_c to be not top level. node_b should receive Mouse Enter.
			node_c->set_as_top_level(false);
			CHECK(node_b->mouse_over);
			CHECK_FALSE(node_b->mouse_over_self);
			CHECK(node_d->mouse_over);
			CHECK(node_d->mouse_over_self);

			// Move to Control node_j.
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK(SceneStringName(mouse_entered), signal_args);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Change node_i to top level. node_h should receive Mouse Exit. node_i should not receive any new signals.
			node_i->set_as_top_level(true);
			CHECK_FALSE(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Change node_i to not top level. node_h should receive Mouse Enter. node_i should not receive any new signals.
			node_i->set_as_top_level(false);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			CHECK_FALSE(node_b->invalid_order);
			CHECK_FALSE(node_d->invalid_order);
			CHECK_FALSE(node_e->invalid_order);
			CHECK_FALSE(node_h->invalid_order);
			CHECK_FALSE(node_i->invalid_order);
			CHECK_FALSE(node_j->invalid_order);

			node_d->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_i->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_STOP);

			SIGNAL_UNWATCH(node_i, SceneStringName(mouse_entered));
			SIGNAL_UNWATCH(node_i, SceneStringName(mouse_exited));
		}

		SUBCASE("[Viewport][GuiInputEvent] Mouse Enter/Exit notification when changing the mouse filter to stop.") {
			SIGNAL_WATCH(node_i, SceneStringName(mouse_entered));
			SIGNAL_WATCH(node_i, SceneStringName(mouse_exited));
			Array signal_args;
			signal_args.push_back(Array());

			node_i->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_PASS);

			// Move to Control node_j.
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK(SceneStringName(mouse_entered), signal_args);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Change node_i to MOUSE_FILTER_STOP. node_h should receive Mouse Exit. node_i should not receive any new signals.
			node_i->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			CHECK_FALSE(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Change node_i to MOUSE_FILTER_PASS. node_h should receive Mouse Enter. node_i should not receive any new signals.
			node_i->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			CHECK_FALSE(node_h->invalid_order);
			CHECK_FALSE(node_i->invalid_order);
			CHECK_FALSE(node_j->invalid_order);

			node_i->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_STOP);

			SIGNAL_UNWATCH(node_i, SceneStringName(mouse_entered));
			SIGNAL_UNWATCH(node_i, SceneStringName(mouse_exited));
		}

		SUBCASE("[Viewport][GuiInputEvent] Mouse Enter/Exit notification when changing the mouse filter to ignore.") {
			SIGNAL_WATCH(node_i, SceneStringName(mouse_entered));
			SIGNAL_WATCH(node_i, SceneStringName(mouse_exited));
			Array signal_args;
			signal_args.push_back(Array());

			node_i->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_PASS);

			// Move to Control node_j.
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK(SceneStringName(mouse_entered), signal_args);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Change node_i to MOUSE_FILTER_IGNORE. node_i should receive Mouse Exit.
			node_i->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK(SceneStringName(mouse_exited), signal_args);

			// Change node_i to MOUSE_FILTER_PASS. node_i should receive Mouse Enter.
			node_i->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK(SceneStringName(mouse_entered), signal_args);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Change node_j to MOUSE_FILTER_IGNORE. After updating the mouse motion, node_i should now have mouse_over_self.
			node_j->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Change node_j to MOUSE_FILTER_PASS. After updating the mouse motion, node_j should now have mouse_over_self.
			node_j->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			CHECK_FALSE(node_h->invalid_order);
			CHECK_FALSE(node_i->invalid_order);
			CHECK_FALSE(node_j->invalid_order);

			node_i->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_STOP);

			SIGNAL_UNWATCH(node_i, SceneStringName(mouse_entered));
			SIGNAL_UNWATCH(node_i, SceneStringName(mouse_exited));
		}

		SUBCASE("[Viewport][GuiInputEvent] Mouse Enter/Exit notification when removing the hovered Control.") {
			SIGNAL_WATCH(node_h, SceneStringName(mouse_entered));
			SIGNAL_WATCH(node_h, SceneStringName(mouse_exited));
			Array signal_args;
			signal_args.push_back(Array());

			node_i->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_PASS);

			// Move to Control node_j.
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK(SceneStringName(mouse_entered), signal_args);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Remove node_i from the tree. node_i and node_j should receive Mouse Exit. node_h should not receive any new signals.
			node_h->remove_child(node_i);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Add node_i to the tree and update the mouse. node_i and node_j should receive Mouse Enter. node_h should not receive any new signals.
			node_h->add_child(node_i);
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			CHECK_FALSE(node_h->invalid_order);
			CHECK_FALSE(node_i->invalid_order);
			CHECK_FALSE(node_j->invalid_order);

			node_i->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_STOP);

			SIGNAL_UNWATCH(node_h, SceneStringName(mouse_entered));
			SIGNAL_UNWATCH(node_h, SceneStringName(mouse_exited));
		}

		SUBCASE("[Viewport][GuiInputEvent] Mouse Enter/Exit notification when hiding the hovered Control.") {
			SIGNAL_WATCH(node_h, SceneStringName(mouse_entered));
			SIGNAL_WATCH(node_h, SceneStringName(mouse_exited));
			Array signal_args;
			signal_args.push_back(Array());

			node_i->set_mouse_filter(Control::MOUSE_FILTER_PASS);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_PASS);

			// Move to Control node_j.
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK(SceneStringName(mouse_entered), signal_args);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Hide node_i. node_i and node_j should receive Mouse Exit. node_h should not receive any new signals.
			node_i->hide();
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK_FALSE(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK_FALSE(node_j->mouse_over);
			CHECK_FALSE(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			// Show node_i and update the mouse. node_i and node_j should receive Mouse Enter. node_h should not receive any new signals.
			node_i->show();
			SEND_GUI_MOUSE_MOTION_EVENT(on_j, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_h->mouse_over);
			CHECK_FALSE(node_h->mouse_over_self);
			CHECK(node_i->mouse_over);
			CHECK_FALSE(node_i->mouse_over_self);
			CHECK(node_j->mouse_over);
			CHECK(node_j->mouse_over_self);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			CHECK_FALSE(node_h->invalid_order);
			CHECK_FALSE(node_i->invalid_order);
			CHECK_FALSE(node_j->invalid_order);

			node_i->set_mouse_filter(Control::MOUSE_FILTER_STOP);
			node_j->set_mouse_filter(Control::MOUSE_FILTER_STOP);

			SIGNAL_UNWATCH(node_h, SceneStringName(mouse_entered));
			SIGNAL_UNWATCH(node_h, SceneStringName(mouse_exited));
		}

		SUBCASE("[Viewport][GuiInputEvent] Window Mouse Enter/Exit signals.") {
			SIGNAL_WATCH(root, SceneStringName(mouse_entered));
			SIGNAL_WATCH(root, SceneStringName(mouse_exited));
			Array signal_args;
			signal_args.push_back(Array());

			SEND_GUI_MOUSE_MOTION_EVENT(on_outside, MouseButtonMask::NONE, Key::NONE);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
			SIGNAL_CHECK(SceneStringName(mouse_exited), signal_args);

			SEND_GUI_MOUSE_MOTION_EVENT(on_a, MouseButtonMask::NONE, Key::NONE);
			SIGNAL_CHECK(SceneStringName(mouse_entered), signal_args);
			SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));

			SIGNAL_UNWATCH(root, SceneStringName(mouse_entered));
			SIGNAL_UNWATCH(root, SceneStringName(mouse_exited));
		}

		SUBCASE("[Viewport][GuiInputEvent] Process-Mode affects, if GUI Mouse Motion Events are processed.") {
			node_a->last_mouse_move_position = on_outside;
			node_a->set_process_mode(Node::PROCESS_MODE_DISABLED);
			SEND_GUI_MOUSE_MOTION_EVENT(on_a, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->last_mouse_move_position == on_outside);

			// Now verify that with allowed processing the event is processed.
			node_a->set_process_mode(Node::PROCESS_MODE_ALWAYS);
			SEND_GUI_MOUSE_MOTION_EVENT(on_a, MouseButtonMask::NONE, Key::NONE);
			CHECK(node_a->last_mouse_move_position == on_a);
		}
	}

	// Unit tests for Viewport::_gui_input_event (Drag and Drop)
	SUBCASE("[Viewport][GuiInputEvent] Drag and Drop") {
		// FIXME: Drag-Preview will likely change. Tests for this part would have to be rewritten anyway.
		// See https://github.com/godotengine/godot/pull/67531#issuecomment-1385353430 for details.
		// Note: Testing Drag and Drop with non-embedded windows would require DisplayServerMock additions.
		int min_grab_movement = 11;
		SUBCASE("[Viewport][GuiInputEvent][DnD] Drag from one Control to another in the same viewport.") {
			SUBCASE("[Viewport][GuiInputEvent][DnD] Perform successful Drag and Drop on a different Control.") {
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());

				// Move above a Control, that is a Drop target and allows dropping at this point.
				SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_CAN_DROP);

				CHECK(root->gui_is_dragging());
				CHECK_FALSE(root->gui_is_drag_successful());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK(root->gui_is_drag_successful());
				CHECK((StringName)node_d->drag_data == SNAME("Drag Data"));
			}

			SUBCASE("[Viewport][GuiInputEvent][DnD] Perform unsuccessful drop on Control.") {
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				// Move, but don't trigger DnD yet.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(0, min_grab_movement - 1), MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				// Move and trigger DnD.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());

				// Move above a Control, that is not a Drop target.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_FORBIDDEN);

				// Move above a Control, that is a Drop target, but has disallowed this point.
				SEND_GUI_MOUSE_MOTION_EVENT(on_d + Point2i(20, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_FORBIDDEN);
				CHECK(root->gui_is_dragging());

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d + Point2i(20, 0), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK_FALSE(root->gui_is_drag_successful());
			}

			SUBCASE("[Viewport][GuiInputEvent][DnD] Perform unsuccessful drop on No-Control.") {
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				// Move, but don't trigger DnD yet.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement - 1, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				// Move and trigger DnD.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());

				// Move away from Controls.
				SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_ARROW);

				CHECK(root->gui_is_dragging());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK_FALSE(root->gui_is_drag_successful());
			}

			SUBCASE("[Viewport][GuiInputEvent][DnD] Perform unsuccessful drop outside of window.") {
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				// Move and trigger DnD.
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());

				SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_CAN_DROP);

				// Move outside of window.
				SEND_GUI_MOUSE_MOTION_EVENT(on_outside, MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_outside, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK_FALSE(root->gui_is_drag_successful());
			}

			SUBCASE("[Viewport][GuiInputEvent][DnD] Drag and Drop doesn't work with other Mouse Buttons than LMB.") {
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::MIDDLE, MouseButtonMask::MIDDLE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());

				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement, 0), MouseButtonMask::MIDDLE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::MIDDLE, MouseButtonMask::NONE, Key::NONE);
			}

			SUBCASE("[Viewport][GuiInputEvent][DnD] Drag and Drop parent propagation.") {
				Node2D *node_aa = memnew(Node2D);
				Control *node_aaa = memnew(Control);
				Node2D *node_dd = memnew(Node2D);
				Control *node_ddd = memnew(Control);
				node_aaa->set_size(Size2i(10, 10));
				node_aaa->set_position(Point2i(0, 5));
				node_ddd->set_size(Size2i(10, 10));
				node_ddd->set_position(Point2i(0, 5));
				node_a->add_child(node_aa);
				node_aa->add_child(node_aaa);
				node_d->add_child(node_dd);
				node_dd->add_child(node_ddd);
				Point2i on_aaa = on_a + Point2i(-2, 2);
				Point2i on_ddd = on_d + Point2i(-2, 2);

				SUBCASE("[Viewport][GuiInputEvent] Drag and Drop propagation to parent Controls.") {
					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					node_ddd->set_mouse_filter(Control::MOUSE_FILTER_PASS);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_aaa, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(on_aaa + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
					CHECK(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(on_ddd, MouseButtonMask::LEFT, Key::NONE);

					CHECK(root->gui_is_dragging());
					CHECK_FALSE(root->gui_is_drag_successful());
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_ddd, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());
					CHECK(root->gui_is_drag_successful());

					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_STOP);
					node_ddd->set_mouse_filter(Control::MOUSE_FILTER_STOP);
				}

				SUBCASE("[Viewport][GuiInputEvent] Drag and Drop grab-propagation stopped by Top Level.") {
					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					node_aaa->set_as_top_level(true);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_aaa, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(on_aaa + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					node_aaa->set_as_top_level(false);
					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_STOP);
				}

				SUBCASE("[Viewport][GuiInputEvent] Drag and Drop target-propagation stopped by Top Level.") {
					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					node_ddd->set_mouse_filter(Control::MOUSE_FILTER_PASS);
					node_ddd->set_as_top_level(true);
					node_ddd->set_position(Point2i(30, 100));

					SEND_GUI_MOUSE_BUTTON_EVENT(on_aaa, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(on_aaa + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
					CHECK(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(Point2i(35, 105), MouseButtonMask::LEFT, Key::NONE);

					CHECK(root->gui_is_dragging());
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(Point2i(35, 105), MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());
					CHECK_FALSE(root->gui_is_drag_successful());

					node_ddd->set_position(Point2i(0, 5));
					node_ddd->set_as_top_level(false);
					node_aaa->set_mouse_filter(Control::MOUSE_FILTER_STOP);
					node_ddd->set_mouse_filter(Control::MOUSE_FILTER_STOP);
				}

				SUBCASE("[Viewport][GuiInputEvent] Drag and Drop grab-propagation stopped by non-CanvasItem.") {
					node_g->set_mouse_filter(Control::MOUSE_FILTER_PASS);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
					SEND_GUI_MOUSE_MOTION_EVENT(on_g + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_background, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					node_g->set_mouse_filter(Control::MOUSE_FILTER_STOP);
				}

				SUBCASE("[Viewport][GuiInputEvent] Drag and Drop target-propagation stopped by non-CanvasItem.") {
					node_g->set_mouse_filter(Control::MOUSE_FILTER_PASS);

					SEND_GUI_MOUSE_BUTTON_EVENT(on_a - Point2i(1, 1), MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE); // Offset for node_aaa.
					SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(0, min_grab_movement), MouseButtonMask::LEFT, Key::NONE);
					CHECK(root->gui_is_dragging());

					SEND_GUI_MOUSE_MOTION_EVENT(on_g, MouseButtonMask::LEFT, Key::NONE);
					SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_g, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
					CHECK_FALSE(root->gui_is_dragging());

					node_g->set_mouse_filter(Control::MOUSE_FILTER_STOP);
				}

				memdelete(node_ddd);
				memdelete(node_dd);
				memdelete(node_aaa);
				memdelete(node_aa);
			}

			SUBCASE("[Viewport][GuiInputEvent][DnD] Force Drag and Drop.") {
				SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				node_a->force_drag(SNAME("Drag Data"), nullptr);
				CHECK(root->gui_is_dragging());

				SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::NONE, Key::NONE);

				// Force Drop doesn't get triggered by mouse Buttons other than LMB.
				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::MIDDLE, MouseButtonMask::MIDDLE, Key::NONE);
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::MIDDLE, MouseButtonMask::NONE, Key::NONE);
				CHECK(root->gui_is_dragging());

				// Force Drop with LMB-Down.
				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK(root->gui_is_drag_successful());

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);

				node_a->force_drag(SNAME("Drag Data"), nullptr);
				CHECK(root->gui_is_dragging());

				// Cancel with RMB.
				SEND_GUI_MOUSE_BUTTON_EVENT(on_d, MouseButton::RIGHT, MouseButtonMask::RIGHT, Key::NONE);
				CHECK_FALSE(root->gui_is_dragging());
				CHECK_FALSE(root->gui_is_drag_successful());
				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_a, MouseButton::RIGHT, MouseButtonMask::NONE, Key::NONE);
			}
		}

		SUBCASE("[Viewport][GuiInputEvent][DnD] Drag to a different Viewport.") {
			SubViewportContainer *svc = memnew(SubViewportContainer);
			svc->set_size(Size2(100, 100));
			svc->set_position(Point2(200, 50));
			root->add_child(svc);

			SubViewport *sv = memnew(SubViewport);
			sv->set_embedding_subwindows(true);
			sv->set_size(Size2i(100, 100));
			svc->add_child(sv);

			DragStart *sv_a = memnew(DragStart);
			sv_a->set_position(Point2(10, 10));
			sv_a->set_size(Size2(10, 10));
			sv->add_child(sv_a);
			Point2i on_sva = Point2i(215, 65);

			DragTarget *sv_b = memnew(DragTarget);
			sv_b->set_position(Point2(30, 30));
			sv_b->set_size(Size2(20, 20));
			sv->add_child(sv_b);
			Point2i on_svb = Point2i(235, 85);

			Window *ew = memnew(Window);
			ew->set_position(Point2(50, 200));
			ew->set_size(Size2(100, 100));
			root->add_child(ew);

			DragStart *ew_a = memnew(DragStart);
			ew_a->set_position(Point2(10, 10));
			ew_a->set_size(Size2(10, 10));
			ew->add_child(ew_a);
			Point2i on_ewa = Point2i(65, 215);

			DragTarget *ew_b = memnew(DragTarget);
			ew_b->set_position(Point2(30, 30));
			ew_b->set_size(Size2(20, 20));
			ew->add_child(ew_b);
			Point2i on_ewb = Point2i(85, 235);

			SUBCASE("[Viewport][GuiInputEvent][DnD] Drag to SubViewport") {
				sv_b->valid_drop = false;
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());
				CHECK(sv_b->during_drag);
				SEND_GUI_MOUSE_MOTION_EVENT(on_svb, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_CAN_DROP);

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_svb, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(sv_b->valid_drop);
				CHECK(!sv_b->during_drag);
			}

			SUBCASE("[Viewport][GuiInputEvent][DnD] Drag from SubViewport") {
				node_d->valid_drop = false;
				SEND_GUI_MOUSE_BUTTON_EVENT(on_sva, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_MOTION_EVENT(on_sva + Point2i(min_grab_movement, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(sv->gui_is_dragging());
				CHECK(node_d->during_drag);
				SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_CAN_DROP);

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_d->valid_drop);
				CHECK(!node_d->during_drag);
			}

			SUBCASE("[Viewport][GuiInputEvent][DnD] Drag to embedded Window") {
				ew_b->valid_drop = false;
				SEND_GUI_MOUSE_BUTTON_EVENT(on_a, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_MOTION_EVENT(on_a + Point2i(min_grab_movement, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(root->gui_is_dragging());
				CHECK(ew_b->during_drag);
				SEND_GUI_MOUSE_MOTION_EVENT(on_ewb, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_CAN_DROP);

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_ewb, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(ew_b->valid_drop);
				CHECK(!ew_b->during_drag);
			}

			SUBCASE("[Viewport][GuiInputEvent][DnD] Drag from embedded Window") {
				node_d->valid_drop = false;
				SEND_GUI_MOUSE_BUTTON_EVENT(on_ewa, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
				SEND_GUI_MOUSE_MOTION_EVENT(on_ewa + Point2i(min_grab_movement, 0), MouseButtonMask::LEFT, Key::NONE);
				CHECK(ew->gui_is_dragging());
				CHECK(node_d->during_drag);
				SEND_GUI_MOUSE_MOTION_EVENT(on_d, MouseButtonMask::LEFT, Key::NONE);
				CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_CAN_DROP);

				SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_d, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
				CHECK(node_d->valid_drop);
				CHECK(!node_d->during_drag);
			}

			memdelete(ew_a);
			memdelete(ew_b);
			memdelete(ew);
			memdelete(sv_a);
			memdelete(sv_b);
			memdelete(sv);
			memdelete(svc);
		}
	}

	memdelete(node_j);
	memdelete(node_i);
	memdelete(node_h);
	memdelete(node_g);
	memdelete(node_f);
	memdelete(node_e);
	memdelete(node_d);
	memdelete(node_c);
	memdelete(node_b);
	memdelete(node_a);
}

TEST_CASE("[SceneTree][Viewport] Control mouse cursor shape") {
	SUBCASE("[Viewport][CursorShape] Mouse cursor is not overridden by SubViewportContainer") {
		SubViewportContainer *node_a = memnew(SubViewportContainer);
		SubViewport *node_b = memnew(SubViewport);
		Control *node_c = memnew(Control);

		node_a->set_name("SubViewportContainer");
		node_b->set_name("SubViewport");
		node_c->set_name("Control");
		node_a->set_position(Point2i(0, 0));
		node_c->set_position(Point2i(0, 0));
		node_a->set_size(Point2i(100, 100));
		node_b->set_size(Point2i(100, 100));
		node_c->set_size(Point2i(100, 100));
		node_a->set_default_cursor_shape(Control::CURSOR_ARROW);
		node_c->set_default_cursor_shape(Control::CURSOR_FORBIDDEN);
		Window *root = SceneTree::get_singleton()->get_root();
		DisplayServerMock *DS = (DisplayServerMock *)(DisplayServer::get_singleton());

		// Scene tree:
		// - root
		//   - node_a (SubViewportContainer)
		//     - node_b (SubViewport)
		//       - node_c (Control)

		root->add_child(node_a);
		node_a->add_child(node_b);
		node_b->add_child(node_c);

		Point2i on_c = Point2i(5, 5);

		SEND_GUI_MOUSE_MOTION_EVENT(on_c, MouseButtonMask::NONE, Key::NONE);
		CHECK(DS->get_cursor_shape() == DisplayServer::CURSOR_FORBIDDEN); // GH-74805

		memdelete(node_c);
		memdelete(node_b);
		memdelete(node_a);
	}
}

class TestArea2D : public Area2D {
	GDCLASS(TestArea2D, Area2D);

	void _on_mouse_entered() {
		enter_id = ++TestArea2D::counter; // > 0, if activated.
	}

	void _on_mouse_exited() {
		exit_id = ++TestArea2D::counter; // > 0, if activated.
	}

	void _on_input_event(Node *p_vp, Ref<InputEvent> p_ev, int p_shape) {
		last_input_event = p_ev;
	}

public:
	static int counter;
	int enter_id = 0;
	int exit_id = 0;
	Ref<InputEvent> last_input_event;

	void init_signals() {
		connect(SceneStringName(mouse_entered), callable_mp(this, &TestArea2D::_on_mouse_entered));
		connect(SceneStringName(mouse_exited), callable_mp(this, &TestArea2D::_on_mouse_exited));
		connect(SceneStringName(input_event), callable_mp(this, &TestArea2D::_on_input_event));
	}

	void test_reset() {
		enter_id = 0;
		exit_id = 0;
		last_input_event.unref();
	}
};

int TestArea2D::counter = 0;

TEST_CASE("[SceneTree][Viewport] Physics Picking 2D") {
	// FIXME: MOUSE_MODE_CAPTURED if-conditions are not testable, because DisplayServerMock doesn't support it.

	// NOTE: This test requires a real physics server.
	PhysicsServer2DDummy *physics_server_2d_dummy = Object::cast_to<PhysicsServer2DDummy>(PhysicsServer2D::get_singleton());
	if (physics_server_2d_dummy) {
		return;
	}

	struct PickingCollider {
		TestArea2D *a;
		CollisionShape2D *c;
		Ref<RectangleShape2D> r;
	};

	SceneTree *tree = SceneTree::get_singleton();
	Window *root = tree->get_root();
	root->set_physics_object_picking(true);

	Point2i on_background = Point2i(800, 800);
	Point2i on_outside = Point2i(-1, -1);
	SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
	tree->physics_process(1);

	Vector<PickingCollider> v;
	for (int i = 0; i < 4; i++) {
		PickingCollider pc;
		pc.a = memnew(TestArea2D);
		pc.c = memnew(CollisionShape2D);
		pc.r.instantiate();
		pc.r->set_size(Size2(150, 150));
		pc.c->set_shape(pc.r);
		pc.a->add_child(pc.c);
		pc.a->set_name("A" + itos(i));
		pc.c->set_name("C" + itos(i));
		v.push_back(pc);
		SIGNAL_WATCH(pc.a, SceneStringName(mouse_entered));
		SIGNAL_WATCH(pc.a, SceneStringName(mouse_exited));
	}

	Node2D *node_a = memnew(Node2D);
	node_a->set_position(Point2i(0, 0));
	v[0].a->set_position(Point2i(0, 0));
	v[1].a->set_position(Point2i(0, 100));
	node_a->add_child(v[0].a);
	node_a->add_child(v[1].a);
	Node2D *node_b = memnew(Node2D);
	node_b->set_position(Point2i(100, 0));
	v[2].a->set_position(Point2i(0, 0));
	v[3].a->set_position(Point2i(0, 100));
	node_b->add_child(v[2].a);
	node_b->add_child(v[3].a);
	root->add_child(node_a);
	root->add_child(node_b);
	Point2i on_all = Point2i(50, 50);
	Point2i on_0 = Point2i(10, 10);
	Point2i on_01 = Point2i(10, 50);
	Point2i on_02 = Point2i(50, 10);

	Array empty_signal_args_2;
	empty_signal_args_2.push_back(Array());
	empty_signal_args_2.push_back(Array());

	Array empty_signal_args_4;
	empty_signal_args_4.push_back(Array());
	empty_signal_args_4.push_back(Array());
	empty_signal_args_4.push_back(Array());
	empty_signal_args_4.push_back(Array());

	for (PickingCollider E : v) {
		E.a->init_signals();
	}

	SUBCASE("[Viewport][Picking2D] Mouse Motion") {
		SEND_GUI_MOUSE_MOTION_EVENT(on_all, MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);
		SIGNAL_CHECK(SceneStringName(mouse_entered), empty_signal_args_4);
		SIGNAL_CHECK_FALSE(SceneStringName(mouse_exited));
		for (PickingCollider E : v) {
			CHECK(E.a->enter_id);
			CHECK_FALSE(E.a->exit_id);
			E.a->test_reset();
		}

		SEND_GUI_MOUSE_MOTION_EVENT(on_01, MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);
		SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
		SIGNAL_CHECK(SceneStringName(mouse_exited), empty_signal_args_2);

		for (int i = 0; i < v.size(); i++) {
			CHECK_FALSE(v[i].a->enter_id);
			if (i < 2) {
				CHECK_FALSE(v[i].a->exit_id);
			} else {
				CHECK(v[i].a->exit_id);
			}
			v[i].a->test_reset();
		}

		SEND_GUI_MOUSE_MOTION_EVENT(on_outside, MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);
		SIGNAL_CHECK_FALSE(SceneStringName(mouse_entered));
		SIGNAL_CHECK(SceneStringName(mouse_exited), empty_signal_args_2);
		for (int i = 0; i < v.size(); i++) {
			CHECK_FALSE(v[i].a->enter_id);
			if (i < 2) {
				CHECK(v[i].a->exit_id);
			} else {
				CHECK_FALSE(v[i].a->exit_id);
			}
			v[i].a->test_reset();
		}
	}

	SUBCASE("[Viewport][Picking2D] Object moved / passive hovering") {
		SEND_GUI_MOUSE_MOTION_EVENT(on_all, MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);
		for (int i = 0; i < v.size(); i++) {
			CHECK(v[i].a->enter_id);
			CHECK_FALSE(v[i].a->exit_id);
			v[i].a->test_reset();
		}

		node_b->set_position(Point2i(200, 0));
		tree->physics_process(1);
		for (int i = 0; i < v.size(); i++) {
			CHECK_FALSE(v[i].a->enter_id);
			if (i < 2) {
				CHECK_FALSE(v[i].a->exit_id);
			} else {
				CHECK(v[i].a->exit_id);
			}
			v[i].a->test_reset();
		}

		node_b->set_position(Point2i(100, 0));
		tree->physics_process(1);
		for (int i = 0; i < v.size(); i++) {
			if (i < 2) {
				CHECK_FALSE(v[i].a->enter_id);
			} else {
				CHECK(v[i].a->enter_id);
			}
			CHECK_FALSE(v[i].a->exit_id);
			v[i].a->test_reset();
		}
	}

	SUBCASE("[Viewport][Picking2D] No Processing") {
		SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);
		for (PickingCollider E : v) {
			E.a->test_reset();
		}

		v[0].a->set_process_mode(Node::PROCESS_MODE_DISABLED);
		v[0].c->set_process_mode(Node::PROCESS_MODE_DISABLED);
		SEND_GUI_MOUSE_MOTION_EVENT(on_02, MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);
		CHECK_FALSE(v[0].a->enter_id);
		CHECK_FALSE(v[0].a->exit_id);
		CHECK(v[2].a->enter_id);
		CHECK_FALSE(v[2].a->exit_id);
		for (PickingCollider E : v) {
			E.a->test_reset();
		}

		SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);
		CHECK_FALSE(v[0].a->enter_id);
		CHECK_FALSE(v[0].a->exit_id);
		CHECK_FALSE(v[2].a->enter_id);
		CHECK(v[2].a->exit_id);

		for (PickingCollider E : v) {
			E.a->test_reset();
		}
		v[0].a->set_process_mode(Node::PROCESS_MODE_ALWAYS);
		v[0].c->set_process_mode(Node::PROCESS_MODE_ALWAYS);
	}

	SUBCASE("[Viewport][Picking2D] Multiple events in series") {
		SEND_GUI_MOUSE_MOTION_EVENT(on_0, MouseButtonMask::NONE, Key::NONE);
		SEND_GUI_MOUSE_MOTION_EVENT(on_0 + Point2i(10, 0), MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);

		for (int i = 0; i < v.size(); i++) {
			if (i < 1) {
				CHECK(v[i].a->enter_id);
			} else {
				CHECK_FALSE(v[i].a->enter_id);
			}
			CHECK_FALSE(v[i].a->exit_id);
			v[i].a->test_reset();
		}

		SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
		SEND_GUI_MOUSE_MOTION_EVENT(on_background + Point2i(10, 10), MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);

		for (int i = 0; i < v.size(); i++) {
			CHECK_FALSE(v[i].a->enter_id);
			if (i < 1) {
				CHECK(v[i].a->exit_id);
			} else {
				CHECK_FALSE(v[i].a->exit_id);
			}
			v[i].a->test_reset();
		}
	}

	SUBCASE("[Viewport][Picking2D] Disable Picking") {
		SEND_GUI_MOUSE_MOTION_EVENT(on_02, MouseButtonMask::NONE, Key::NONE);

		root->set_physics_object_picking(false);
		CHECK_FALSE(root->get_physics_object_picking());

		tree->physics_process(1);

		for (int i = 0; i < v.size(); i++) {
			CHECK_FALSE(v[i].a->enter_id);
			v[i].a->test_reset();
		}

		root->set_physics_object_picking(true);
		CHECK(root->get_physics_object_picking());
	}

	SUBCASE("[Viewport][Picking2D] CollisionObject in CanvasLayer") {
		CanvasLayer *node_c = memnew(CanvasLayer);
		node_c->set_rotation(Math_PI);
		node_c->set_offset(Point2i(100, 100));
		root->add_child(node_c);

		v[2].a->reparent(node_c, false);
		v[3].a->reparent(node_c, false);

		SEND_GUI_MOUSE_MOTION_EVENT(on_02, MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);

		for (int i = 0; i < v.size(); i++) {
			if (i == 0 || i == 3) {
				CHECK(v[i].a->enter_id);
			} else {
				CHECK_FALSE(v[i].a->enter_id);
			}
			v[i].a->test_reset();
		}

		v[2].a->reparent(node_b, false);
		v[3].a->reparent(node_b, false);
		root->remove_child(node_c);
		memdelete(node_c);
	}

	SUBCASE("[Viewport][Picking2D] Picking Sort") {
		root->set_physics_object_picking_sort(true);
		CHECK(root->get_physics_object_picking_sort());

		SUBCASE("[Viewport][Picking2D] Picking Sort Z-Index") {
			node_a->set_z_index(10);
			v[0].a->set_z_index(0);
			v[1].a->set_z_index(2);
			node_b->set_z_index(5);
			v[2].a->set_z_index(8);
			v[3].a->set_z_index(11);
			v[3].a->set_z_as_relative(false);

			TestArea2D::counter = 0;
			SEND_GUI_MOUSE_MOTION_EVENT(on_all, MouseButtonMask::NONE, Key::NONE);
			tree->physics_process(1);

			CHECK(v[0].a->enter_id == 4);
			CHECK(v[1].a->enter_id == 2);
			CHECK(v[2].a->enter_id == 1);
			CHECK(v[3].a->enter_id == 3);
			for (int i = 0; i < v.size(); i++) {
				CHECK_FALSE(v[i].a->exit_id);
				v[i].a->test_reset();
			}

			TestArea2D::counter = 0;
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			tree->physics_process(1);

			CHECK(v[0].a->exit_id == 4);
			CHECK(v[1].a->exit_id == 2);
			CHECK(v[2].a->exit_id == 1);
			CHECK(v[3].a->exit_id == 3);
			for (int i = 0; i < v.size(); i++) {
				CHECK_FALSE(v[i].a->enter_id);
				v[i].a->set_z_as_relative(true);
				v[i].a->set_z_index(0);
				v[i].a->test_reset();
			}

			node_a->set_z_index(0);
			node_b->set_z_index(0);
		}

		SUBCASE("[Viewport][Picking2D] Picking Sort Scene Tree Location") {
			TestArea2D::counter = 0;
			SEND_GUI_MOUSE_MOTION_EVENT(on_all, MouseButtonMask::NONE, Key::NONE);
			tree->physics_process(1);

			for (int i = 0; i < v.size(); i++) {
				CHECK(v[i].a->enter_id == 4 - i);
				CHECK_FALSE(v[i].a->exit_id);
				v[i].a->test_reset();
			}

			TestArea2D::counter = 0;
			SEND_GUI_MOUSE_MOTION_EVENT(on_background, MouseButtonMask::NONE, Key::NONE);
			tree->physics_process(1);

			for (int i = 0; i < v.size(); i++) {
				CHECK_FALSE(v[i].a->enter_id);
				CHECK(v[i].a->exit_id == 4 - i);
				v[i].a->test_reset();
			}
		}

		root->set_physics_object_picking_sort(false);
		CHECK_FALSE(root->get_physics_object_picking_sort());
	}

	SUBCASE("[Viewport][Picking2D] Mouse Button") {
		SEND_GUI_MOUSE_BUTTON_EVENT(on_0, MouseButton::LEFT, MouseButtonMask::LEFT, Key::NONE);
		tree->physics_process(1);

		for (int i = 0; i < v.size(); i++) {
			if (i == 0) {
				CHECK(v[i].a->enter_id);
			} else {
				CHECK_FALSE(v[i].a->enter_id);
			}
			CHECK_FALSE(v[i].a->exit_id);
			v[i].a->test_reset();
		}

		SEND_GUI_MOUSE_BUTTON_RELEASED_EVENT(on_0, MouseButton::LEFT, MouseButtonMask::NONE, Key::NONE);
		tree->physics_process(1);

		for (int i = 0; i < v.size(); i++) {
			CHECK_FALSE(v[i].a->enter_id);
			CHECK_FALSE(v[i].a->exit_id);
			v[i].a->test_reset();
		}
	}

	SUBCASE("[Viewport][Picking2D] Screen Touch") {
		SEND_GUI_TOUCH_EVENT(on_01, true, false);
		tree->physics_process(1);
		for (int i = 0; i < v.size(); i++) {
			if (i < 2) {
				Ref<InputEventScreenTouch> st = v[i].a->last_input_event;
				CHECK(st.is_valid());
			} else {
				CHECK(v[i].a->last_input_event.is_null());
			}
			v[i].a->test_reset();
		}
	}

	for (PickingCollider E : v) {
		SIGNAL_UNWATCH(E.a, SceneStringName(mouse_entered));
		SIGNAL_UNWATCH(E.a, SceneStringName(mouse_exited));
		memdelete(E.c);
		memdelete(E.a);
	}
}

TEST_CASE("[SceneTree][Viewport] Embedded Windows") {
	Window *root = SceneTree::get_singleton()->get_root();
	Window *w = memnew(Window);

	SUBCASE("[Viewport] Safe-rect of embedded Window") {
		root->add_child(w);
		root->subwindow_set_popup_safe_rect(w, Rect2i(10, 10, 10, 10));
		CHECK_EQ(root->subwindow_get_popup_safe_rect(w), Rect2i(10, 10, 10, 10));
		root->remove_child(w);
		CHECK_EQ(root->subwindow_get_popup_safe_rect(w), Rect2i());
	}

	memdelete(w);
}

} // namespace TestViewport

#endif // TEST_VIEWPORT_H
