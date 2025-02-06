/**************************************************************************/
/*  world_environment.h                                                   */
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

#include "scene/main/node.h"
#include "scene/resources/camera_attributes.h"
#include "scene/resources/compositor.h"
#include "scene/resources/environment.h"

class WorldEnvironment : public Node {
	GDCLASS(WorldEnvironment, Node);

	Ref<Environment> environment;
	Ref<CameraAttributes> camera_attributes;
	Ref<Compositor> compositor;

	void _update_current_environment();
	void _update_current_camera_attributes();
	void _update_current_compositor();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_environment(const Ref<Environment> &p_environment);
	Ref<Environment> get_environment() const;

	void set_camera_attributes(const Ref<CameraAttributes> &p_camera_attributes);
	Ref<CameraAttributes> get_camera_attributes() const;

	void set_compositor(const Ref<Compositor> &p_compositor);
	Ref<Compositor> get_compositor() const;

	PackedStringArray get_configuration_warnings() const override;

	WorldEnvironment();
};
