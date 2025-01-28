/**************************************************************************/
/*  text_server_dummy.h                                                   */
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

#ifndef TEXT_SERVER_DUMMY_H
#define TEXT_SERVER_DUMMY_H

#include "servers/text/text_server_extension.h"

/*************************************************************************/

class TextServerDummy : public TextServerExtension {
	GDCLASS(TextServerDummy, TextServerExtension);
	_THREAD_SAFE_CLASS_

public:
	virtual bool has_feature(Feature p_feature) const override { return false; }
	virtual String get_name() const override { return "Dummy"; }
	virtual int64_t get_features() const override { return 0; }
	virtual void free_rid(const RID &p_rid) override {}
	virtual bool has(const RID &p_rid) override { return false; }

	virtual RID create_font() override { return RID(); }
	virtual void font_set_fixed_size(const RID &p_font_rid, int64_t p_fixed_size) override {}
	virtual int64_t font_get_fixed_size(const RID &p_font_rid) const override { return 0; }
	virtual void font_set_fixed_size_scale_mode(const RID &p_font_rid, TextServer::FixedSizeScaleMode p_fixed_size_scale_mode) override {}
	virtual TextServer::FixedSizeScaleMode font_get_fixed_size_scale_mode(const RID &p_font_rid) const override { return FIXED_SIZE_SCALE_DISABLE; }
	virtual TypedArray<Vector2i> font_get_size_cache_list(const RID &p_font_rid) const override { return TypedArray<Vector2i>(); }
	virtual void font_clear_size_cache(const RID &p_font_rid) override {}
	virtual void font_remove_size_cache(const RID &p_font_rid, const Vector2i &p_size) override {}
	virtual void font_set_ascent(const RID &p_font_rid, int64_t p_size, double p_ascent) override {}
	virtual double font_get_ascent(const RID &p_font_rid, int64_t p_size) const override { return 0; }
	virtual void font_set_descent(const RID &p_font_rid, int64_t p_size, double p_descent) override {}
	virtual double font_get_descent(const RID &p_font_rid, int64_t p_size) const override { return 0; }
	virtual void font_set_underline_position(const RID &p_font_rid, int64_t p_size, double p_underline_position) override {}
	virtual double font_get_underline_position(const RID &p_font_rid, int64_t p_size) const override { return 0; }
	virtual void font_set_underline_thickness(const RID &p_font_rid, int64_t p_size, double p_underline_thickness) override {}
	virtual double font_get_underline_thickness(const RID &p_font_rid, int64_t p_size) const override { return 0; }
	virtual void font_set_scale(const RID &p_font_rid, int64_t p_size, double p_scale) override {}
	virtual double font_get_scale(const RID &p_font_rid, int64_t p_size) const override { return 0; }
	virtual int64_t font_get_texture_count(const RID &p_font_rid, const Vector2i &p_size) const override { return 0; }
	virtual void font_clear_textures(const RID &p_font_rid, const Vector2i &p_size) override {}
	virtual void font_remove_texture(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) override {}
	virtual void font_set_texture_image(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index, const Ref<Image> &p_image) override {}
	virtual Ref<Image> font_get_texture_image(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) const override { return Ref<Image>(); }
	virtual PackedInt32Array font_get_glyph_list(const RID &p_font_rid, const Vector2i &p_size) const override { return PackedInt32Array(); }
	virtual void font_clear_glyphs(const RID &p_font_rid, const Vector2i &p_size) override {}
	virtual void font_remove_glyph(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) override {}
	virtual Vector2 font_get_glyph_advance(const RID &p_font_rid, int64_t p_size, int64_t p_glyph) const override { return Vector2(); }
	virtual void font_set_glyph_advance(const RID &p_font_rid, int64_t p_size, int64_t p_glyph, const Vector2 &p_advance) override {}
	virtual Vector2 font_get_glyph_offset(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override { return Vector2(); }
	virtual void font_set_glyph_offset(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Vector2 &p_offset) override {}
	virtual Vector2 font_get_glyph_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override { return Vector2(); }
	virtual void font_set_glyph_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Vector2 &p_gl_size) override {}
	virtual Rect2 font_get_glyph_uv_rect(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override { return Rect2(); }
	virtual void font_set_glyph_uv_rect(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Rect2 &p_uv_rect) override {}
	virtual int64_t font_get_glyph_texture_idx(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override { return 0; }
	virtual void font_set_glyph_texture_idx(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, int64_t p_texture_idx) override {}
	virtual RID font_get_glyph_texture_rid(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override { return RID(); }
	virtual Size2 font_get_glyph_texture_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override { return Size2(); }
	virtual int64_t font_get_glyph_index(const RID &p_font_rid, int64_t p_size, int64_t p_char, int64_t p_variation_selector) const override { return 0; }
	virtual int64_t font_get_char_from_glyph_index(const RID &p_font_rid, int64_t p_size, int64_t p_glyph_index) const override { return 0; }
	virtual bool font_has_char(const RID &p_font_rid, int64_t p_char) const override { return false; }
	virtual String font_get_supported_chars(const RID &p_font_rid) const override { return String(); }
	virtual PackedInt32Array font_get_supported_glyphs(const RID &p_font_rid) const override { return PackedInt32Array(); }
	virtual void font_draw_glyph(const RID &p_font_rid, const RID &p_canvas, int64_t p_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color) const override {}
	virtual void font_draw_glyph_outline(const RID &p_font_rid, const RID &p_canvas, int64_t p_size, int64_t p_outline_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color) const override {}

	virtual RID create_shaped_text(TextServer::Direction p_direction, TextServer::Orientation p_orientation) override { return RID(); }
	virtual void shaped_text_clear(const RID &p_shaped) override {}
	virtual bool shaped_text_add_string(const RID &p_shaped, const String &p_text, const TypedArray<RID> &p_fonts, int64_t p_size, const Dictionary &p_opentype_features, const String &p_language, const Variant &p_meta) override { return false; }
	virtual bool shaped_text_add_object(const RID &p_shaped, const Variant &p_key, const Size2 &p_size, InlineAlignment p_inline_align, int64_t p_length, double p_baseline) override { return false; }
	virtual bool shaped_text_resize_object(const RID &p_shaped, const Variant &p_key, const Size2 &p_size, InlineAlignment p_inline_align, double p_baseline) override { return false; }
	virtual int64_t shaped_get_span_count(const RID &p_shaped) const override { return 0; }
	virtual Variant shaped_get_span_meta(const RID &p_shaped, int64_t p_index) const override { return Variant(); }
	virtual Variant shaped_get_span_embedded_object(const RID &p_shaped, int64_t p_index) const override { return Variant(); }
	virtual void shaped_set_span_update_font(const RID &p_shaped, int64_t p_index, const TypedArray<RID> &p_fonts, int64_t p_size, const Dictionary &p_opentype_features) override {}
	virtual RID shaped_text_substr(const RID &p_shaped, int64_t p_start, int64_t p_length) const override { return RID(); }
	virtual RID shaped_text_get_parent(const RID &p_shaped) const override { return RID(); }
	virtual bool shaped_text_shape(const RID &p_shaped) override { return false; }
	virtual bool shaped_text_is_ready(const RID &p_shaped) const override { return false; }
	virtual const Glyph *shaped_text_get_glyphs(const RID &p_shaped) const override { return nullptr; }
	virtual const Glyph *shaped_text_sort_logical(const RID &p_shaped) override { return nullptr; }
	virtual int64_t shaped_text_get_glyph_count(const RID &p_shaped) const override { return 0; }
	virtual Vector2i shaped_text_get_range(const RID &p_shaped) const override { return Vector2i(); }
	virtual int64_t shaped_text_get_trim_pos(const RID &p_shaped) const override { return -1; }
	virtual int64_t shaped_text_get_ellipsis_pos(const RID &p_shaped) const override { return -1; }
	virtual const Glyph *shaped_text_get_ellipsis_glyphs(const RID &p_shaped) const override { return nullptr; }
	virtual int64_t shaped_text_get_ellipsis_glyph_count(const RID &p_shaped) const override { return -1; }
	virtual Array shaped_text_get_objects(const RID &p_shaped) const override { return Array(); }
	virtual Rect2 shaped_text_get_object_rect(const RID &p_shaped, const Variant &p_key) const override { return Rect2(); }
	virtual Vector2i shaped_text_get_object_range(const RID &p_shaped, const Variant &p_key) const override { return Vector2i(); }
	virtual int64_t shaped_text_get_object_glyph(const RID &p_shaped, const Variant &p_key) const override { return -1; }
	virtual Size2 shaped_text_get_size(const RID &p_shaped) const override { return Size2(); }
	virtual double shaped_text_get_ascent(const RID &p_shaped) const override { return 0; }
	virtual double shaped_text_get_descent(const RID &p_shaped) const override { return 0; }
	virtual double shaped_text_get_width(const RID &p_shaped) const override { return 0; }
	virtual double shaped_text_get_underline_position(const RID &p_shaped) const override { return 0; }
	virtual double shaped_text_get_underline_thickness(const RID &p_shaped) const override { return 0; }
};

#endif // TEXT_SERVER_DUMMY_H
