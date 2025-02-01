/**************************************************************************/
/*  visible_on_screen_notifier_3d.h                                       */
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

#pragma once

#include "scene/3d/visual_instance_3d.h"

class VisibleOnScreenNotifier3D : public VisualInstance3D {
	GDCLASS(VisibleOnScreenNotifier3D, VisualInstance3D);

	AABB aabb = AABB(Vector3(-1, -1, -1), Vector3(2, 2, 2));

private:
	bool on_screen = false;
	void _visibility_enter();
	void _visibility_exit();

protected:
	virtual void _screen_enter() {}
	virtual void _screen_exit() {}

	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_aabb(const AABB &p_aabb);
	virtual AABB get_aabb() const override;
	bool is_on_screen() const;

	VisibleOnScreenNotifier3D();
	~VisibleOnScreenNotifier3D();
};

class VisibleOnScreenEnabler3D : public VisibleOnScreenNotifier3D {
	GDCLASS(VisibleOnScreenEnabler3D, VisibleOnScreenNotifier3D);

public:
	enum EnableMode {
		ENABLE_MODE_INHERIT,
		ENABLE_MODE_ALWAYS,
		ENABLE_MODE_WHEN_PAUSED,
	};

protected:
	ObjectID node_id;
	virtual void _screen_enter() override;
	virtual void _screen_exit() override;

	EnableMode enable_mode = ENABLE_MODE_INHERIT;
	NodePath enable_node_path = NodePath("..");

	void _notification(int p_what);
	static void _bind_methods();

	void _update_enable_mode(bool p_enable);

public:
	void set_enable_mode(EnableMode p_mode);
	EnableMode get_enable_mode();

	void set_enable_node_path(NodePath p_path);
	NodePath get_enable_node_path();

	VisibleOnScreenEnabler3D();
};

VARIANT_ENUM_CAST(VisibleOnScreenEnabler3D::EnableMode);
