/**************************************************************************/
/*  rendering_device.compat.inc                                           */
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

RID RenderingDevice::_shader_create_from_bytecode_bind_compat_79606(const Vector<uint8_t> &p_shader_binary) {
	return shader_create_from_bytecode(p_shader_binary, RID());
}

void RenderingDevice::_draw_list_end_bind_compat_81356(BitField<BarrierMask> p_post_barrier) {
	draw_list_end();
}

void RenderingDevice::_compute_list_end_bind_compat_81356(BitField<BarrierMask> p_post_barrier) {
	compute_list_end();
}

void RenderingDevice::_barrier_bind_compat_81356(BitField<BarrierMask> p_from, BitField<BarrierMask> p_to) {
	// Does nothing.
}

void RenderingDevice::_draw_list_end_bind_compat_84976(BitField<BarrierMask> p_post_barrier) {
	draw_list_end();
}

void RenderingDevice::_compute_list_end_bind_compat_84976(BitField<BarrierMask> p_post_barrier) {
	compute_list_end();
}

RenderingDevice::InitialAction RenderingDevice::_convert_initial_action_84976(InitialAction p_old_initial_action) {
	switch (uint32_t(p_old_initial_action)) {
		case 0: // INITIAL_ACTION_CLEAR
			return INITIAL_ACTION_CLEAR;
		case 1: // INITIAL_ACTION_CLEAR_REGION
		case 2: // INITIAL_ACTION_CLEAR_REGION_CONTINUE
			return INITIAL_ACTION_CLEAR;
		case 3: // INITIAL_ACTION_KEEP
			return INITIAL_ACTION_LOAD;
		case 4: // INITIAL_ACTION_DROP
			return INITIAL_ACTION_DISCARD;
		case 5: // INITIAL_ACTION_CONTINUE
			return INITIAL_ACTION_LOAD;
		default:
			return INITIAL_ACTION_LOAD;
	}
}

RenderingDevice::FinalAction RenderingDevice::_convert_final_action_84976(FinalAction p_old_final_action) {
	switch (uint32_t(p_old_final_action)) {
		case 0: // FINAL_ACTION_READ
			return FINAL_ACTION_STORE;
		case 1: // FINAL_ACTION_DISCARD
			return FINAL_ACTION_DISCARD;
		case 2: // FINAL_ACTION_CONTINUE
			return FINAL_ACTION_STORE;
		default:
			return FINAL_ACTION_STORE;
	}
}

RenderingDevice::DrawListID RenderingDevice::_draw_list_begin_bind_compat_84976(RID p_framebuffer, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_color_values, float p_clear_depth, uint32_t p_clear_stencil, const Rect2 &p_region, const TypedArray<RID> &p_storage_textures) {
	return _draw_list_begin_bind_compat_98670(p_framebuffer, _convert_initial_action_84976(p_initial_color_action), _convert_final_action_84976(p_final_color_action), _convert_initial_action_84976(p_initial_depth_action), _convert_final_action_84976(p_final_depth_action), p_clear_color_values, p_clear_depth, p_clear_stencil, p_region, RDD::BreadcrumbMarker::NONE);
}

RenderingDevice::DrawListID RenderingDevice::_draw_list_begin_bind_compat_90993(RID p_framebuffer, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_color_values, float p_clear_depth, uint32_t p_clear_stencil, const Rect2 &p_region) {
	return _draw_list_begin_bind_compat_98670(p_framebuffer, p_initial_color_action, p_final_color_action, p_initial_depth_action, p_final_depth_action, p_clear_color_values, p_clear_depth, p_clear_stencil, p_region, RDD::BreadcrumbMarker::NONE);
}

RenderingDevice::DrawListID RenderingDevice::_draw_list_begin_bind_compat_98670(RID p_framebuffer, InitialAction p_initial_color_action, FinalAction p_final_color_action, InitialAction p_initial_depth_action, FinalAction p_final_depth_action, const Vector<Color> &p_clear_color_values, float p_clear_depth, uint32_t p_clear_stencil, const Rect2 &p_region, uint32_t p_breadcrumb) {
	// Deduce flags from initial and final actions.
	BitField<DrawFlags> draw_flags = RD::DRAW_DEFAULT_ALL;
	if (p_initial_color_action == RD::INITIAL_ACTION_CLEAR) {
		draw_flags.set_flag(DrawFlags(RD::DRAW_CLEAR_COLOR_ALL));
	} else if (p_initial_color_action == RD::INITIAL_ACTION_DISCARD) {
		draw_flags.set_flag(DrawFlags(RD::DRAW_IGNORE_COLOR_ALL));
	}

	if (p_initial_depth_action == RD::INITIAL_ACTION_CLEAR) {
		draw_flags.set_flag(DrawFlags(RD::DRAW_CLEAR_DEPTH | RD::DRAW_CLEAR_STENCIL));
	} else if (p_initial_depth_action == RD::INITIAL_ACTION_DISCARD) {
		draw_flags.set_flag(DrawFlags(RD::DRAW_IGNORE_DEPTH | RD::DRAW_IGNORE_STENCIL));
	}

	return draw_list_begin(p_framebuffer, draw_flags, p_clear_color_values, p_clear_depth, p_clear_stencil, p_region, p_breadcrumb);
}

RenderingDevice::ComputeListID RenderingDevice::_compute_list_begin_bind_compat_84976(bool p_allow_draw_overlap) {
	return compute_list_begin();
}

Error RenderingDevice::_buffer_update_bind_compat_84976(RID p_buffer, uint32_t p_offset, uint32_t p_size, const Vector<uint8_t> &p_data, BitField<BarrierMask> p_post_barrier) {
	return _buffer_update_bind(p_buffer, p_offset, p_size, p_data);
}

Error RenderingDevice::_buffer_clear_bind_compat_84976(RID p_buffer, uint32_t p_offset, uint32_t p_size, BitField<BarrierMask> p_post_barrier) {
	return buffer_clear(p_buffer, p_offset, p_size);
}

Error RenderingDevice::_texture_update_bind_compat_84976(RID p_texture, uint32_t p_layer, const Vector<uint8_t> &p_data, BitField<BarrierMask> p_post_barrier) {
	return texture_update(p_texture, p_layer, p_data);
}

Error RenderingDevice::_texture_copy_bind_compat_84976(RID p_from_texture, RID p_to_texture, const Vector3 &p_from, const Vector3 &p_to, const Vector3 &p_size, uint32_t p_src_mipmap, uint32_t p_dst_mipmap, uint32_t p_src_layer, uint32_t p_dst_layer, BitField<BarrierMask> p_post_barrier) {
	return texture_copy(p_from_texture, p_to_texture, p_from, p_to, p_size, p_src_mipmap, p_dst_mipmap, p_src_layer, p_dst_layer);
}

Error RenderingDevice::_texture_clear_bind_compat_84976(RID p_texture, const Color &p_color, uint32_t p_base_mipmap, uint32_t p_mipmaps, uint32_t p_base_layer, uint32_t p_layers, BitField<BarrierMask> p_post_barrier) {
	return texture_clear(p_texture, p_color, p_base_mipmap, p_mipmaps, p_base_layer, p_layers);
}

Error RenderingDevice::_texture_resolve_multisample_bind_compat_84976(RID p_from_texture, RID p_to_texture, BitField<BarrierMask> p_post_barrier) {
	return texture_resolve_multisample(p_from_texture, p_to_texture);
}

RenderingDevice::FramebufferFormatID RenderingDevice::_screen_get_framebuffer_format_bind_compat_87340() const {
	return screen_get_framebuffer_format(DisplayServer::MAIN_WINDOW_ID);
}

void RenderingDevice::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("shader_create_from_bytecode", "binary_data"), &RenderingDevice::_shader_create_from_bytecode_bind_compat_79606);

	ClassDB::bind_compatibility_method(D_METHOD("draw_list_end", "post_barrier"), &RenderingDevice::_draw_list_end_bind_compat_81356, DEFVAL(7));
	ClassDB::bind_compatibility_method(D_METHOD("compute_list_end", "post_barrier"), &RenderingDevice::_compute_list_end_bind_compat_81356, DEFVAL(7));
	ClassDB::bind_compatibility_method(D_METHOD("barrier", "from", "to"), &RenderingDevice::_barrier_bind_compat_81356, DEFVAL(7), DEFVAL(7));

	ClassDB::bind_compatibility_method(D_METHOD("draw_list_end", "post_barrier"), &RenderingDevice::_draw_list_end_bind_compat_84976, DEFVAL(0x7FFF));
	ClassDB::bind_compatibility_method(D_METHOD("compute_list_end", "post_barrier"), &RenderingDevice::_compute_list_end_bind_compat_84976, DEFVAL(0x7FFF));
	ClassDB::bind_compatibility_method(D_METHOD("draw_list_begin", "framebuffer", "initial_color_action", "final_color_action", "initial_depth_action", "final_depth_action", "clear_color_values", "clear_depth", "clear_stencil", "region", "storage_textures"), &RenderingDevice::_draw_list_begin_bind_compat_84976, DEFVAL(Vector<Color>()), DEFVAL(1.0), DEFVAL(0), DEFVAL(Rect2()), DEFVAL(TypedArray<RID>()));
	ClassDB::bind_compatibility_method(D_METHOD("compute_list_begin", "allow_draw_overlap"), &RenderingDevice::_compute_list_begin_bind_compat_84976, DEFVAL(false));
	ClassDB::bind_compatibility_method(D_METHOD("buffer_update", "buffer", "offset", "size_bytes", "data", "post_barrier"), &RenderingDevice::_buffer_update_bind_compat_84976, DEFVAL(0x7FFF));
	ClassDB::bind_compatibility_method(D_METHOD("buffer_clear", "buffer", "offset", "size_bytes", "post_barrier"), &RenderingDevice::_buffer_clear_bind_compat_84976, DEFVAL(0x7FFF));
	ClassDB::bind_compatibility_method(D_METHOD("texture_update", "texture", "layer", "data", "post_barrier"), &RenderingDevice::_texture_update_bind_compat_84976, DEFVAL(0x7FFF));
	ClassDB::bind_compatibility_method(D_METHOD("texture_copy", "from_texture", "to_texture", "from_pos", "to_pos", "size", "src_mipmap", "dst_mipmap", "src_layer", "dst_layer", "post_barrier"), &RenderingDevice::_texture_copy_bind_compat_84976, DEFVAL(0x7FFF));
	ClassDB::bind_compatibility_method(D_METHOD("texture_clear", "texture", "color", "base_mipmap", "mipmap_count", "base_layer", "layer_count", "post_barrier"), &RenderingDevice::_texture_clear_bind_compat_84976, DEFVAL(0x7FFF));
	ClassDB::bind_compatibility_method(D_METHOD("texture_resolve_multisample", "from_texture", "to_texture", "post_barrier"), &RenderingDevice::_texture_resolve_multisample_bind_compat_84976, DEFVAL(0x7FFF));

	ClassDB::bind_compatibility_method(D_METHOD("screen_get_framebuffer_format"), &RenderingDevice::_screen_get_framebuffer_format_bind_compat_87340);

	ClassDB::bind_compatibility_method(D_METHOD("draw_list_begin", "framebuffer", "initial_color_action", "final_color_action", "initial_depth_action", "final_depth_action", "clear_color_values", "clear_depth", "clear_stencil", "region"), &RenderingDevice::_draw_list_begin_bind_compat_90993, DEFVAL(Vector<Color>()), DEFVAL(1.0), DEFVAL(0), DEFVAL(Rect2()));

	ClassDB::bind_compatibility_method(D_METHOD("draw_list_begin", "framebuffer", "initial_color_action", "final_color_action", "initial_depth_action", "final_depth_action", "clear_color_values", "clear_depth", "clear_stencil", "region", "breadcrumb"), &RenderingDevice::_draw_list_begin_bind_compat_98670, DEFVAL(Vector<Color>()), DEFVAL(1.0), DEFVAL(0), DEFVAL(Rect2()), DEFVAL(0));
}

#endif
