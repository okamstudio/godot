/**************************************************************************/
/*  camera_feed.h                                                         */
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

#include "core/io/image.h"
#include "core/math/transform_2d.h"
#include "servers/camera_server.h"

/**
	The camera server is a singleton object that gives access to the various
	camera feeds that can be used as the background for our environment.
**/

class CameraFeed : public RefCounted {
	GDCLASS(CameraFeed, RefCounted);

public:
	enum FeedDataType {
		FEED_NOIMAGE, // we don't have an image yet
		FEED_RGB, // our texture will contain a normal RGB texture that can be used directly
		FEED_YCBCR, // our texture will contain a YCbCr texture that needs to be converted to RGB before output
		FEED_YCBCR_SEP, // our camera is split into two textures, first plane contains Y data, second plane contains CbCr data
		FEED_EXTERNAL, // specific for android atm, camera feed is managed externally, assumed RGB for now
	};

	enum FeedPosition {
		FEED_UNSPECIFIED, // we have no idea
		FEED_FRONT, // this is a camera on the front of the device
		FEED_BACK // this is a camera on the back of the device
	};

private:
	int id; // unique id for this, for internal use in case feeds are removed

protected:
	struct FeedFormat {
		int width = 0;
		int height = 0;
		String format;
		int frame_numerator = 0;
		int frame_denominator = 0;
		uint32_t pixel_format = 0;
	};

	String name; // name of our camera feed
	FeedDataType datatype; // type of texture data stored
	FeedPosition position; // position of camera on the device
	Transform2D transform; // display transform
	int base_width = 0;
	int base_height = 0;
	Vector<FeedFormat> formats;
	Dictionary parameters;
	int selected_format = -1;

	bool active; // only when active do we actually update the camera texture each frame
	RID texture[CameraServer::FEED_IMAGES]; // texture images needed for this

	static void _bind_methods();

public:
	int get_id() const;
	bool is_active() const;
	void set_active(bool p_is_active);

	String get_name() const;
	void set_name(String p_name);

	int get_base_width() const;
	int get_base_height() const;

	FeedPosition get_position() const;
	void set_position(FeedPosition p_position);

	Transform2D get_transform() const;
	void set_transform(const Transform2D &p_transform);

	RID get_texture(CameraServer::FeedImage p_which);
	uint64_t get_texture_tex_id(CameraServer::FeedImage p_which);

	CameraFeed();
	CameraFeed(String p_name, FeedPosition p_position = CameraFeed::FEED_UNSPECIFIED);
	virtual ~CameraFeed();

	FeedDataType get_datatype() const;
	void set_rgb_image(const Ref<Image> &p_rgb_img);
	void set_ycbcr_image(const Ref<Image> &p_ycbcr_img);
	void set_ycbcr_images(const Ref<Image> &p_y_img, const Ref<Image> &p_cbcr_img);
	void set_external(int p_width, int p_height);

	virtual bool set_format(int p_index, const Dictionary &p_parameters);
	virtual Array get_formats() const;
	virtual FeedFormat get_format() const;

	virtual bool activate_feed();
	virtual void deactivate_feed();

	GDVIRTUAL0R(bool, _activate_feed)
	GDVIRTUAL0(_deactivate_feed)
};

VARIANT_ENUM_CAST(CameraFeed::FeedDataType);
VARIANT_ENUM_CAST(CameraFeed::FeedPosition);
