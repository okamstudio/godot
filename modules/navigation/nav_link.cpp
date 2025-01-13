/**************************************************************************/
/*  nav_link.cpp                                                          */
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

#include "nav_link.h"

#include "nav_map.h"

void NavLink::set_map(NavMap *p_map) {
	if (map == p_map) {
		return;
	}

	cancel_sync_request();

	if (map) {
		map->remove_link(this);
	}

	map = p_map;
	link_dirty = true;

	if (map) {
		map->add_link(this);
		request_sync();
	}
}

void NavLink::set_enabled(bool p_enabled) {
	if (enabled == p_enabled) {
		return;
	}
	enabled = p_enabled;

	// TODO: This should not require a full rebuild as the link has not really changed.
	link_dirty = true;

	request_sync();
}

void NavLink::set_bidirectional(bool p_bidirectional) {
	if (bidirectional == p_bidirectional) {
		return;
	}
	bidirectional = p_bidirectional;
	link_dirty = true;

	request_sync();
}

void NavLink::set_start_position(const Vector3 p_position) {
	if (start_position == p_position) {
		return;
	}
	start_position = p_position;
	link_dirty = true;

	request_sync();
}

void NavLink::set_end_position(const Vector3 p_position) {
	if (end_position == p_position) {
		return;
	}
	end_position = p_position;
	link_dirty = true;

	request_sync();
}

bool NavLink::is_dirty() const {
	return link_dirty;
}

void NavLink::sync() {
	link_dirty = false;
}

void NavLink::request_sync() {
	if (map && !sync_dirty_request_list_element.in_list()) {
		map->add_link_sync_dirty_request(&sync_dirty_request_list_element);
	}
}

void NavLink::cancel_sync_request() {
	if (map && sync_dirty_request_list_element.in_list()) {
		map->remove_link_sync_dirty_request(&sync_dirty_request_list_element);
	}
}

NavLink::NavLink() :
		sync_dirty_request_list_element(this) {
	type = NavigationUtilities::PathSegmentType::PATH_SEGMENT_TYPE_LINK;
}

NavLink::~NavLink() {
	cancel_sync_request();
}
