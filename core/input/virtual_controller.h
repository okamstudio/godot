/**************************************************************************/
/*  virtual_controller.h                                                  */
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

#ifndef VIRTUAL_CONTROLLER_H
#define VIRTUAL_CONTROLLER_H

#include "core/object/class_db.h"

class VirtualController : public Object {
	GDCLASS(VirtualController, Object);

protected:
	static void _bind_methods();

public:
	virtual void enable() = 0;
	virtual void disable() = 0;
	virtual bool is_enabled() = 0;
	virtual void set_enabled_left_thumbstick(bool p_enabled) = 0;
	virtual bool is_enabled_left_thumbstick() = 0;
	virtual void set_enabled_right_thumbstick(bool p_enabled) = 0;
	virtual bool is_enabled_right_thumbstick() = 0;
	virtual void set_enabled_button_a(bool p_enabled) = 0;
	virtual bool is_enabled_button_a() = 0;
	virtual void set_enabled_button_b(bool p_enabled) = 0;
	virtual bool is_enabled_button_b() = 0;
	virtual void set_enabled_button_x(bool p_enabled) = 0;
	virtual bool is_enabled_button_x() = 0;
	virtual void set_enabled_button_y(bool p_enabled) = 0;
	virtual bool is_enabled_button_y() = 0;
};

#endif // VIRTUAL_CONTROLLER_H
