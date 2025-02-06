/**************************************************************************/
/*  render_scene_data.h                                                   */
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

#include "core/object/class_db.h"
#include "core/object/gdvirtual.gen.inc"
#include "core/object/object.h"
#include "core/object/script_language.h"

class RenderSceneData : public Object {
	GDCLASS(RenderSceneData, Object);

protected:
	static void _bind_methods();

public:
	virtual Transform3D get_cam_transform() const = 0;
	virtual Projection get_cam_projection() const = 0;

	virtual uint32_t get_view_count() const = 0;
	virtual Vector3 get_view_eye_offset(uint32_t p_view) const = 0;
	virtual Projection get_view_projection(uint32_t p_view) const = 0;

	virtual RID get_uniform_buffer() const = 0;
};

class RenderSceneDataExtension : public RenderSceneData {
	GDCLASS(RenderSceneDataExtension, RenderSceneData);

protected:
	static void _bind_methods();

public:
	virtual Transform3D get_cam_transform() const override;
	virtual Projection get_cam_projection() const override;

	virtual uint32_t get_view_count() const override;
	virtual Vector3 get_view_eye_offset(uint32_t p_view) const override;
	virtual Projection get_view_projection(uint32_t p_view) const override;

	virtual RID get_uniform_buffer() const override;

	GDVIRTUAL0RC(Transform3D, _get_cam_transform)
	GDVIRTUAL0RC(Projection, _get_cam_projection)

	GDVIRTUAL0RC(uint32_t, _get_view_count)
	GDVIRTUAL1RC(Vector3, _get_view_eye_offset, uint32_t)
	GDVIRTUAL1RC(Projection, _get_view_projection, uint32_t)

	GDVIRTUAL0RC(RID, _get_uniform_buffer)
};
