/**************************************************************************/
/*  camera_server.cpp                                                     */
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

#include "camera_server.h"
#include "core/variant/typed_array.h"
#include "rendering_server.h"
#include "servers/camera/camera_feed.h"

////////////////////////////////////////////////////////
// CameraServer

CameraServer::CreateFunc CameraServer::create_func = nullptr;

void CameraServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_feed", "index"), &CameraServer::get_feed);
	ClassDB::bind_method(D_METHOD("get_feed_count"), &CameraServer::get_feed_count);
	ClassDB::bind_method(D_METHOD("feeds"), &CameraServer::get_feeds);

	ClassDB::bind_method(D_METHOD("add_feed", "feed"), &CameraServer::add_feed);
	ClassDB::bind_method(D_METHOD("remove_feed", "feed"), &CameraServer::remove_feed);

	ADD_SIGNAL(MethodInfo("camera_feed_added", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("camera_feed_removed", PropertyInfo(Variant::INT, "id")));

	BIND_ENUM_CONSTANT(FEED_RGBA_IMAGE);
	BIND_ENUM_CONSTANT(FEED_YCBCR_IMAGE);
	BIND_ENUM_CONSTANT(FEED_Y_IMAGE);
	BIND_ENUM_CONSTANT(FEED_CBCR_IMAGE);
}

CameraServer *CameraServer::singleton = nullptr;

CameraServer *CameraServer::get_singleton() {
	return singleton;
}

int CameraServer::get_free_id() {
	bool id_exists = true;
	int newid = 0;

	// find a free id
	while (id_exists) {
		newid++;
		id_exists = false;
		for (int i = 0; i < feeds.size() && !id_exists; i++) {
			if (feeds[i]->get_id() == newid) {
				id_exists = true;
			};
		};
	};

	return newid;
}

int CameraServer::get_feed_index(int p_id) {
	for (int i = 0; i < feeds.size(); i++) {
		if (feeds[i]->get_id() == p_id) {
			return i;
		};
	};

	return -1;
}

Ref<CameraFeed> CameraServer::get_feed_by_id(int p_id) {
	int index = get_feed_index(p_id);

	if (index == -1) {
		return nullptr;
	} else {
		return feeds[index];
	}
}

void CameraServer::add_feed(const Ref<CameraFeed> &p_feed) {
	ERR_FAIL_COND(p_feed.is_null());

	// add our feed
	feeds.push_back(p_feed);

	PRINT_VERBOSE("CameraServer: Registered camera " + p_feed->get_name() + " with ID " + itos(p_feed->get_id()) + " and position " + itos(p_feed->get_position()) + " at index " + itos(feeds.size() - 1));

	// let whomever is interested know
	emit_signal(SNAME("camera_feed_added"), p_feed->get_id());
}

void CameraServer::remove_feed(const Ref<CameraFeed> &p_feed) {
	for (int i = 0; i < feeds.size(); i++) {
		if (feeds[i] == p_feed) {
			int feed_id = p_feed->get_id();

			PRINT_VERBOSE("CameraServer: Removed camera " + p_feed->get_name() + " with ID " + itos(feed_id) + " and position " + itos(p_feed->get_position()));

			// remove it from our array, if this results in our feed being unreferenced it will be destroyed
			feeds.remove_at(i);

			// let whomever is interested know
			emit_signal(SNAME("camera_feed_removed"), feed_id);
			return;
		};
	};
}

Ref<CameraFeed> CameraServer::get_feed(int p_index) {
	ERR_FAIL_INDEX_V(p_index, feeds.size(), nullptr);

	return feeds[p_index];
}

int CameraServer::get_feed_count() {
	return feeds.size();
}

TypedArray<CameraFeed> CameraServer::get_feeds() {
	TypedArray<CameraFeed> return_feeds;
	int cc = get_feed_count();
	return_feeds.resize(cc);

	for (int i = 0; i < feeds.size(); i++) {
		return_feeds[i] = get_feed(i);
	};

	return return_feeds;
}

RID CameraServer::feed_texture(int p_id, CameraServer::FeedImage p_texture) {
	int index = get_feed_index(p_id);
	ERR_FAIL_COND_V(index == -1, RID());

	Ref<CameraFeed> feed = get_feed(index);

	return feed->get_texture(p_texture);
}

CameraServer::CameraServer() {
	singleton = this;
}

CameraServer::~CameraServer() {
	singleton = nullptr;
}
