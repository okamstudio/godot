/**************************************************************************/
/*  camera_feed.cpp                                                       */
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

#include "camera_feed.h"

#include "servers/rendering_server.h"

void CameraFeed::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_id"), &CameraFeed::get_id);

	ClassDB::bind_method(D_METHOD("is_active"), &CameraFeed::is_active);
	ClassDB::bind_method(D_METHOD("set_active", "active"), &CameraFeed::set_active);

	ClassDB::bind_method(D_METHOD("get_name"), &CameraFeed::get_name);
	ClassDB::bind_method(D_METHOD("set_name", "name"), &CameraFeed::set_name);

	ClassDB::bind_method(D_METHOD("get_position"), &CameraFeed::get_position);
	ClassDB::bind_method(D_METHOD("set_position", "position"), &CameraFeed::set_position);

	// Note, for transform some feeds may override what the user sets (such as ARKit)
	ClassDB::bind_method(D_METHOD("get_transform"), &CameraFeed::get_transform);
	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &CameraFeed::set_transform);

	ClassDB::bind_method(D_METHOD("set_rgb_image", "rgb_image"), &CameraFeed::set_rgb_image);
	ClassDB::bind_method(D_METHOD("set_ycbcr_image", "ycbcr_image"), &CameraFeed::set_ycbcr_image);
	ClassDB::bind_method(D_METHOD("set_external", "width", "height"), &CameraFeed::set_external);
	ClassDB::bind_method(D_METHOD("get_texture_tex_id", "feed_image_type"), &CameraFeed::get_texture_tex_id);

	ClassDB::bind_method(D_METHOD("get_datatype"), &CameraFeed::get_datatype);

	ClassDB::bind_method(D_METHOD("get_formats"), &CameraFeed::get_formats);
	ClassDB::bind_method(D_METHOD("set_format", "index", "parameters"), &CameraFeed::set_format);

	GDVIRTUAL_BIND(_activate_feed);
	GDVIRTUAL_BIND(_deactivate_feed);

	ADD_SIGNAL(MethodInfo("frame_changed"));
	ADD_SIGNAL(MethodInfo("format_changed"));

	ADD_GROUP("Feed", "feed_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "feed_is_active"), "set_active", "is_active");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "feed_transform"), "set_transform", "get_transform");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "formats"), "", "get_formats");

	BIND_ENUM_CONSTANT(FEED_NOIMAGE);
	BIND_ENUM_CONSTANT(FEED_RGB);
	BIND_ENUM_CONSTANT(FEED_YCBCR);
	BIND_ENUM_CONSTANT(FEED_YCBCR_SEP);
	BIND_ENUM_CONSTANT(FEED_EXTERNAL);

	BIND_ENUM_CONSTANT(FEED_UNSPECIFIED);
	BIND_ENUM_CONSTANT(FEED_FRONT);
	BIND_ENUM_CONSTANT(FEED_BACK);
}

int CameraFeed::get_id() const {
	return id;
}

bool CameraFeed::is_active() const {
	return active;
}

void CameraFeed::set_active(bool p_is_active) {
	if (p_is_active == active) {
		// all good
	} else if (p_is_active) {
		// attempt to activate this feed
		if (activate_feed()) {
			active = true;
		}
	} else {
		// just deactivate it
		deactivate_feed();
		active = false;
	}
}

String CameraFeed::get_name() const {
	return name;
}

void CameraFeed::set_name(String p_name) {
	name = p_name;
}

int CameraFeed::get_base_width() const {
	return base_width;
}

int CameraFeed::get_base_height() const {
	return base_height;
}

CameraFeed::FeedDataType CameraFeed::get_datatype() const {
	return datatype;
}

CameraFeed::FeedPosition CameraFeed::get_position() const {
	return position;
}

void CameraFeed::set_position(CameraFeed::FeedPosition p_position) {
	position = p_position;
}

Transform2D CameraFeed::get_transform() const {
	return transform;
}

void CameraFeed::set_transform(const Transform2D &p_transform) {
	transform = p_transform;
}

RID CameraFeed::get_texture(CameraServer::FeedImage p_which) {
	return texture[p_which];
}

uint64_t CameraFeed::get_texture_tex_id(CameraServer::FeedImage p_which) {
	return RenderingServer::get_singleton()->texture_get_native_handle(texture[p_which]);
}

CameraFeed::CameraFeed() {
	// initialize our feed
	id = CameraServer::get_singleton()->get_free_id();
	base_width = 0;
	base_height = 0;
	name = "???";
	active = false;
	datatype = CameraFeed::FEED_RGB;
	position = CameraFeed::FEED_UNSPECIFIED;
	transform = Transform2D(1.0, 0.0, 0.0, -1.0, 0.0, 1.0);
	texture[CameraServer::FEED_Y_IMAGE] = RenderingServer::get_singleton()->texture_2d_placeholder_create();
	texture[CameraServer::FEED_CBCR_IMAGE] = RenderingServer::get_singleton()->texture_2d_placeholder_create();
}

CameraFeed::CameraFeed(String p_name, FeedPosition p_position) {
	// initialize our feed
	id = CameraServer::get_singleton()->get_free_id();
	base_width = 0;
	base_height = 0;
	name = p_name;
	active = false;
	datatype = CameraFeed::FEED_NOIMAGE;
	position = p_position;
	transform = Transform2D(1.0, 0.0, 0.0, -1.0, 0.0, 1.0);
	texture[CameraServer::FEED_Y_IMAGE] = RenderingServer::get_singleton()->texture_2d_placeholder_create();
	texture[CameraServer::FEED_CBCR_IMAGE] = RenderingServer::get_singleton()->texture_2d_placeholder_create();
}

CameraFeed::~CameraFeed() {
	// Free our textures
	ERR_FAIL_NULL(RenderingServer::get_singleton());
	RenderingServer::get_singleton()->free(texture[CameraServer::FEED_Y_IMAGE]);
	RenderingServer::get_singleton()->free(texture[CameraServer::FEED_CBCR_IMAGE]);
}

void CameraFeed::set_rgb_image(const Ref<Image> &p_rgb_img) {
	ERR_FAIL_COND(p_rgb_img.is_null());
	if (active) {
		int new_width = p_rgb_img->get_width();
		int new_height = p_rgb_img->get_height();

		if ((base_width != new_width) || (base_height != new_height)) {
			// We're assuming here that our camera image doesn't change around formats etc, allocate the whole lot...
			base_width = new_width;
			base_height = new_height;

			RID new_texture = RenderingServer::get_singleton()->texture_2d_create(p_rgb_img);
			RenderingServer::get_singleton()->texture_replace(texture[CameraServer::FEED_RGBA_IMAGE], new_texture);

			emit_signal(SNAME("format_changed"));
		} else {
			RenderingServer::get_singleton()->texture_2d_update(texture[CameraServer::FEED_RGBA_IMAGE], p_rgb_img);
		}

		datatype = CameraFeed::FEED_RGB;
	}
}

void CameraFeed::set_ycbcr_image(const Ref<Image> &p_ycbcr_img) {
	ERR_FAIL_COND(p_ycbcr_img.is_null());
	if (active) {
		int new_width = p_ycbcr_img->get_width();
		int new_height = p_ycbcr_img->get_height();

		if ((base_width != new_width) || (base_height != new_height)) {
			// We're assuming here that our camera image doesn't change around formats etc, allocate the whole lot...
			base_width = new_width;
			base_height = new_height;

			RID new_texture = RenderingServer::get_singleton()->texture_2d_create(p_ycbcr_img);
			RenderingServer::get_singleton()->texture_replace(texture[CameraServer::FEED_RGBA_IMAGE], new_texture);

			emit_signal(SNAME("format_changed"));
		} else {
			RenderingServer::get_singleton()->texture_2d_update(texture[CameraServer::FEED_RGBA_IMAGE], p_ycbcr_img);
		}

		datatype = CameraFeed::FEED_YCBCR;
	}
}

void CameraFeed::set_ycbcr_images(const Ref<Image> &p_y_img, const Ref<Image> &p_cbcr_img) {
	ERR_FAIL_COND(p_y_img.is_null());
	ERR_FAIL_COND(p_cbcr_img.is_null());
	if (active) {
		///@TODO investigate whether we can use thirdparty/misc/yuv2rgb.h here to convert our YUV data to RGB, our shader approach is potentially faster though..
		// Wondering about including that into multiple projects, may cause issues.
		// That said, if we convert to RGB, we could enable using texture resources again...

		int new_y_width = p_y_img->get_width();
		int new_y_height = p_y_img->get_height();

		if ((base_width != new_y_width) || (base_height != new_y_height)) {
			// We're assuming here that our camera image doesn't change around formats etc, allocate the whole lot...
			base_width = new_y_width;
			base_height = new_y_height;
			{
				RID new_texture = RenderingServer::get_singleton()->texture_2d_create(p_y_img);
				RenderingServer::get_singleton()->texture_replace(texture[CameraServer::FEED_Y_IMAGE], new_texture);
			}
			{
				RID new_texture = RenderingServer::get_singleton()->texture_2d_create(p_cbcr_img);
				RenderingServer::get_singleton()->texture_replace(texture[CameraServer::FEED_CBCR_IMAGE], new_texture);
			}

			emit_signal(SNAME("format_changed"));
		} else {
			RenderingServer::get_singleton()->texture_2d_update(texture[CameraServer::FEED_Y_IMAGE], p_y_img);
			RenderingServer::get_singleton()->texture_2d_update(texture[CameraServer::FEED_CBCR_IMAGE], p_cbcr_img);
		}

		datatype = CameraFeed::FEED_YCBCR_SEP;
	}
}

void CameraFeed::set_external(int p_width, int p_height) {
	if ((base_width != p_width) || (base_height != p_height)) {
		// We're assuming here that our camera image doesn't change around formats etc, allocate the whole lot...
		base_width = p_width;
		base_height = p_height;

		auto new_texture = RenderingServer::get_singleton()->texture_external_create(p_width, p_height, 0);
		RenderingServer::get_singleton()->texture_replace(texture[CameraServer::FEED_YCBCR_IMAGE], new_texture);
	}

	datatype = CameraFeed::FEED_EXTERNAL;
}

bool CameraFeed::activate_feed() {
	bool ret = true;
	GDVIRTUAL_CALL(_activate_feed, ret);
	return ret;
}

void CameraFeed::deactivate_feed() {
	GDVIRTUAL_CALL(_deactivate_feed);
}

bool CameraFeed::set_format(int p_index, const Dictionary &p_parameters) {
	return false;
}

Array CameraFeed::get_formats() const {
	return Array();
}

CameraFeed::FeedFormat CameraFeed::get_format() const {
	FeedFormat feed_format = {};
	return feed_format;
}
