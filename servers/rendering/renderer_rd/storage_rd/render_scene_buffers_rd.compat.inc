/**************************************************************************/
/*  render_scene_buffers_rd.compat.inc                                    */
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

#ifndef DISABLE_DEPRECATED

RID RenderSceneBuffersRD::_get_color_texture_compat_80214() {
	return _get_color_texture(msaa_3d != RS::VIEWPORT_MSAA_DISABLED);
}

RID RenderSceneBuffersRD::_get_color_layer_compat_80214(const uint32_t p_layer) {
	return _get_color_layer(p_layer, msaa_3d != RS::VIEWPORT_MSAA_DISABLED);
}

RID RenderSceneBuffersRD::_get_depth_texture_compat_80214() {
	return _get_depth_texture(msaa_3d != RS::VIEWPORT_MSAA_DISABLED);
}

RID RenderSceneBuffersRD::_get_depth_layer_compat_80214(const uint32_t p_layer) {
	return _get_depth_layer(p_layer, msaa_3d != RS::VIEWPORT_MSAA_DISABLED);
}

RID RenderSceneBuffersRD::_get_velocity_texture_compat_80214() {
	return _get_velocity_texture(msaa_3d != RS::VIEWPORT_MSAA_DISABLED);
}

RID RenderSceneBuffersRD::_get_velocity_layer_compat_80214(const uint32_t p_layer) {
	return _get_velocity_layer(p_layer, msaa_3d != RS::VIEWPORT_MSAA_DISABLED);
}

RID RenderSceneBuffersRD::_create_texture_bind_compat_98670(const StringName &p_context, const StringName &p_texture_name, const RD::DataFormat p_data_format, const uint32_t p_usage_bits, const RD::TextureSamples p_texture_samples, const Size2i p_size, const uint32_t p_layers, const uint32_t p_mipmaps, bool p_unique) {
	return create_texture(p_context, p_texture_name, p_data_format, p_usage_bits, p_texture_samples, p_size, p_layers, p_mipmaps, p_unique, false);
}

void RenderSceneBuffersRD::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("get_color_texture"), &RenderSceneBuffersRD::_get_color_texture_compat_80214);
	ClassDB::bind_compatibility_method(D_METHOD("get_color_layer", "layer"), &RenderSceneBuffersRD::_get_color_layer_compat_80214);
	ClassDB::bind_compatibility_method(D_METHOD("get_depth_texture"), &RenderSceneBuffersRD::_get_depth_texture_compat_80214);
	ClassDB::bind_compatibility_method(D_METHOD("get_depth_layer", "layer"), &RenderSceneBuffersRD::_get_depth_layer_compat_80214);
	ClassDB::bind_compatibility_method(D_METHOD("get_velocity_texture"), &RenderSceneBuffersRD::_get_velocity_texture_compat_80214);
	ClassDB::bind_compatibility_method(D_METHOD("get_velocity_layer", "layer"), &RenderSceneBuffersRD::_get_velocity_layer_compat_80214);

	ClassDB::bind_compatibility_method(D_METHOD("create_texture", "context", "name", "data_format", "usage_bits", "texture_samples", "size", "layers", "mipmaps", "unique"), &RenderSceneBuffersRD::_create_texture_bind_compat_98670);
}

#endif // DISABLE_DEPRECATED
