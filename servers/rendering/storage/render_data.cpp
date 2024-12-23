/**************************************************************************/
/*  render_data.cpp                                                       */
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

#include "render_data.h"

void RenderData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_render_scene_buffers"), &RenderData::get_render_scene_buffers);
	ClassDB::bind_method(D_METHOD("get_render_scene_data"), &RenderData::get_render_scene_data);
	ClassDB::bind_method(D_METHOD("get_environment"), &RenderData::get_environment);
	ClassDB::bind_method(D_METHOD("get_camera_attributes"), &RenderData::get_camera_attributes);
	ClassDB::bind_method(D_METHOD("get_light_storage"), &RenderData::get_light_storage);
	ClassDB::bind_method(D_METHOD("get_texture_storage"), &RenderData::get_texture_storage);
}

void RenderDataExtension::_bind_methods() {
	GDVIRTUAL_BIND(_get_render_scene_buffers);
	GDVIRTUAL_BIND(_get_render_scene_data)
	GDVIRTUAL_BIND(_get_light_storage)
	GDVIRTUAL_BIND(_get_texture_storage)
	GDVIRTUAL_BIND(_get_environment)
	GDVIRTUAL_BIND(_get_camera_attributes)
}

RendererLightStorage *RenderDataExtension::get_light_storage() const {
	RendererLightStorage *ret = nullptr;
	GDVIRTUAL_CALL(_get_light_storage, ret);
	return ret;
}

RendererTextureStorage *RenderDataExtension::get_texture_storage() const {
	RendererTextureStorage *ret = nullptr;
	GDVIRTUAL_CALL(_get_texture_storage, ret);
	return ret;
}

Ref<RenderSceneBuffers> RenderDataExtension::get_render_scene_buffers() const {
	Ref<RenderSceneBuffers> ret;
	GDVIRTUAL_CALL(_get_render_scene_buffers, ret);
	return ret;
}

RenderSceneData *RenderDataExtension::get_render_scene_data() const {
	RenderSceneData *ret = nullptr;
	GDVIRTUAL_CALL(_get_render_scene_data, ret);
	return ret;
}

RID RenderDataExtension::get_environment() const {
	RID ret;
	GDVIRTUAL_CALL(_get_environment, ret);
	return ret;
}

RID RenderDataExtension::get_camera_attributes() const {
	RID ret;
	GDVIRTUAL_CALL(_get_camera_attributes, ret);
	return ret;
}
