/**************************************************************************/
/*  xr_tracker.h                                                          */
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

#include "core/os/thread_safe.h"
#include "servers/xr_server.h"

/**
	The XR tracker object is a common base for all different types of XR trackers.
*/

class XRTracker : public RefCounted {
	GDCLASS(XRTracker, RefCounted);
	_THREAD_SAFE_CLASS_

protected:
	XRServer::TrackerType type = XRServer::TRACKER_UNKNOWN; // type of tracker
	StringName name = "Unknown"; // (unique) name of the tracker
	String description; // description of the tracker

	static void _bind_methods();

public:
	virtual void set_tracker_type(XRServer::TrackerType p_type);
	XRServer::TrackerType get_tracker_type() const;
	void set_tracker_name(const StringName &p_name);
	StringName get_tracker_name() const;
	void set_tracker_desc(const String &p_desc);
	String get_tracker_desc() const;
};
