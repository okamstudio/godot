/**************************************************************************/
/*  xr_server.compat.inc                                                  */
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

#define _COMPAT_HEADER_XRServer                                                        \
	void _add_tracker_bind_compat_90645(const Ref<XRPositionalTracker> &p_tracker);    \
	void _remove_tracker_bind_compat_90645(const Ref<XRPositionalTracker> &p_tracker); \
	Ref<XRPositionalTracker> _get_tracker_bind_compat_90645(const StringName &p_name) const;

#define _COMPAT_SOURCE_XRServer                                                                                                  \
	void XRServer::_add_tracker_bind_compat_90645(const Ref<XRPositionalTracker> &p_tracker) {                                   \
		add_tracker(p_tracker);                                                                                                  \
	}                                                                                                                            \
                                                                                                                                 \
	void XRServer::_remove_tracker_bind_compat_90645(const Ref<XRPositionalTracker> &p_tracker) {                                \
		remove_tracker(p_tracker);                                                                                               \
	}                                                                                                                            \
                                                                                                                                 \
	Ref<XRPositionalTracker> XRServer::_get_tracker_bind_compat_90645(const StringName &p_name) const {                          \
		return get_tracker(p_name);                                                                                              \
	}                                                                                                                            \
                                                                                                                                 \
	void XRServer::_bind_compatibility_methods() {                                                                               \
		ClassDB::bind_compatibility_method(D_METHOD("add_tracker", "tracker"), &XRServer::_add_tracker_bind_compat_90645);       \
		ClassDB::bind_compatibility_method(D_METHOD("remove_tracker", "tracker"), &XRServer::_remove_tracker_bind_compat_90645); \
		ClassDB::bind_compatibility_method(D_METHOD("get_tracker", "name"), &XRServer::_get_tracker_bind_compat_90645);          \
	}
