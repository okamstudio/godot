/**************************************************************************/
/*  virtual_controller_ios.h                                              */
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

#import "core/input/virtual_controller.h"

#import <GameController/GameController.h>

class IOSVirtualController : public VirtualController {
private:
#if defined(__IPHONE_15_0)
	API_AVAILABLE(ios(15.0))
	GCVirtualController *gcv_controller;
#endif

public:
	IOSVirtualController();
	~IOSVirtualController();

	virtual void enable() override;
	virtual void disable() override;
	virtual bool is_enabled() override;
	virtual void set_enabled_left_thumbstick(bool p_enabled) override;
	virtual bool is_enabled_left_thumbstick() override;
	virtual void set_enabled_right_thumbstick(bool p_enabled) override;
	virtual bool is_enabled_right_thumbstick() override;
	virtual void set_enabled_button_a(bool p_enabled) override;
	virtual bool is_enabled_button_a() override;
	virtual void set_enabled_button_b(bool p_enabled) override;
	virtual bool is_enabled_button_b() override;
	virtual void set_enabled_button_x(bool p_enabled) override;
	virtual bool is_enabled_button_x() override;
	virtual void set_enabled_button_y(bool p_enabled) override;
	virtual bool is_enabled_button_y() override;

	void controller_connected();
	void controller_disconnected();
	void update_state();

private:
	void connect_controller();
	void disconnect_controller();
	void initialize();
	void elements_changed(GCInputElementName p_name, bool p_hidden);
	void read_project_settings();

private:
	bool enabled = false;
	bool enabled_left_thumbstick = true;
	bool enabled_right_thumbstick = true;
	bool enabled_button_a = true;
	bool enabled_button_b = true;
	bool enabled_button_x = true;
	bool enabled_button_y = true;
};
