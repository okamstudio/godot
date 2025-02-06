/**************************************************************************/
/*  render_data.h                                                         */
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

#include "core/object/object.h"
#include "render_scene_buffers.h"
#include "render_scene_data.h"

class RenderData : public Object {
	GDCLASS(RenderData, Object);

protected:
	static void _bind_methods();

public:
	virtual Ref<RenderSceneBuffers> get_render_scene_buffers() const = 0;
	virtual RenderSceneData *get_render_scene_data() const = 0;

	virtual RID get_environment() const = 0;
	virtual RID get_camera_attributes() const = 0;
};

class RenderDataExtension : public RenderData {
	GDCLASS(RenderDataExtension, RenderData);

protected:
	static void _bind_methods();

	virtual Ref<RenderSceneBuffers> get_render_scene_buffers() const override;
	virtual RenderSceneData *get_render_scene_data() const override;

	virtual RID get_environment() const override;
	virtual RID get_camera_attributes() const override;

	GDVIRTUAL0RC(Ref<RenderSceneBuffers>, _get_render_scene_buffers)
	GDVIRTUAL0RC(RenderSceneData *, _get_render_scene_data)
	GDVIRTUAL0RC(RID, _get_environment)
	GDVIRTUAL0RC(RID, _get_camera_attributes)
};
