/**************************************************************************/
/*  subviewport_container.h                                               */
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

#ifndef SUBVIEWPORT_CONTAINER_H
#define SUBVIEWPORT_CONTAINER_H

#include "scene/gui/container.h"

class SubViewportContainer : public Container {
	GDCLASS(SubViewportContainer, Container);

	bool stretch = false;
	int shrink = 1;
	bool mouse_target = false;

	void _notify_viewports(int p_notification);
	bool _is_propagated_in_gui_input(const Ref<InputEvent> &p_event);
	void _send_event_to_viewports(const Ref<InputEvent> &p_event);
	void _propagate_nonpositional_event(const Ref<InputEvent> &p_event);

protected:
	void _notification(int p_what);
	static void _bind_methods();

	virtual void add_child_notify(Node *p_child) override;
	virtual void remove_child_notify(Node *p_child) override;

	GDVIRTUAL1RC(bool, _propagate_input_event, Ref<InputEvent>);

public:
	void set_stretch(bool p_enable);
	bool is_stretch_enabled() const;

	virtual void input(const Ref<InputEvent> &p_event) override;
	virtual void unhandled_input(const Ref<InputEvent> &p_event) override;
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	void set_stretch_shrink(int p_shrink);
	int get_stretch_shrink() const;
	void recalc_force_viewport_sizes();

	void set_mouse_target(bool p_enable);
	bool is_mouse_target_enabled();

	virtual Size2 get_minimum_size() const override;

	virtual Vector<int> get_allowed_size_flags_horizontal() const override;
	virtual Vector<int> get_allowed_size_flags_vertical() const override;

	PackedStringArray get_configuration_warnings() const override;

	SubViewportContainer();
};

#endif // SUBVIEWPORT_CONTAINER_H
