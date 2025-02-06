/**************************************************************************/
/*  text_server_extension.h                                               */
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

#include "core/object/gdvirtual.gen.inc"
#include "core/os/thread_safe.h"
#include "core/variant/native_ptr.h"
#include "core/variant/typed_array.h"
#include "servers/text_server.h"

class TextServerExtension : public TextServer {
	GDCLASS(TextServerExtension, TextServer);

protected:
	_THREAD_SAFE_CLASS_

	static void _bind_methods();

public:
	virtual bool has_feature(Feature p_feature) const override;
	virtual String get_name() const override;
	virtual int64_t get_features() const override;
	GDVIRTUAL1RC_REQUIRED(bool, _has_feature, Feature);
	GDVIRTUAL0RC_REQUIRED(String, _get_name);
	GDVIRTUAL0RC_REQUIRED(int64_t, _get_features);

	virtual void free_rid(const RID &p_rid) override;
	virtual bool has(const RID &p_rid) override;
	virtual bool load_support_data(const String &p_filename) override;
	GDVIRTUAL1_REQUIRED(_free_rid, RID);
	GDVIRTUAL1R_REQUIRED(bool, _has, RID);
	GDVIRTUAL1R(bool, _load_support_data, const String &);

	virtual String get_support_data_filename() const override;
	virtual String get_support_data_info() const override;
	virtual bool save_support_data(const String &p_filename) const override;
	virtual PackedByteArray get_support_data() const override;
	GDVIRTUAL0RC(String, _get_support_data_filename);
	GDVIRTUAL0RC(String, _get_support_data_info);
	GDVIRTUAL1RC(bool, _save_support_data, const String &);
	GDVIRTUAL0RC(PackedByteArray, _get_support_data);

	virtual bool is_locale_right_to_left(const String &p_locale) const override;
	GDVIRTUAL1RC(bool, _is_locale_right_to_left, const String &);

	virtual int64_t name_to_tag(const String &p_name) const override;
	virtual String tag_to_name(int64_t p_tag) const override;
	GDVIRTUAL1RC(int64_t, _name_to_tag, const String &);
	GDVIRTUAL1RC(String, _tag_to_name, int64_t);

	/* Font interface */

	virtual RID create_font() override;
	GDVIRTUAL0R_REQUIRED(RID, _create_font);

	virtual RID create_font_linked_variation(const RID &p_font_rid) override;
	GDVIRTUAL1R(RID, _create_font_linked_variation, RID);

	virtual void font_set_data(const RID &p_font_rid, const PackedByteArray &p_data) override;
	virtual void font_set_data_ptr(const RID &p_font_rid, const uint8_t *p_data_ptr, int64_t p_data_size) override;
	GDVIRTUAL2(_font_set_data, RID, const PackedByteArray &);
	GDVIRTUAL3(_font_set_data_ptr, RID, GDExtensionConstPtr<const uint8_t>, int64_t);

	virtual void font_set_face_index(const RID &p_font_rid, int64_t p_index) override;
	virtual int64_t font_get_face_index(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_face_index, RID, int64_t);
	GDVIRTUAL1RC(int64_t, _font_get_face_index, RID);

	virtual int64_t font_get_face_count(const RID &p_font_rid) const override;
	GDVIRTUAL1RC(int64_t, _font_get_face_count, RID);

	virtual void font_set_style(const RID &p_font_rid, BitField<FontStyle> p_style) override;
	virtual BitField<FontStyle> font_get_style(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_style, RID, BitField<FontStyle>);
	GDVIRTUAL1RC(BitField<FontStyle>, _font_get_style, RID);

	virtual void font_set_name(const RID &p_font_rid, const String &p_name) override;
	virtual String font_get_name(const RID &p_font_rid) const override;
	virtual Dictionary font_get_ot_name_strings(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_name, RID, const String &);
	GDVIRTUAL1RC(String, _font_get_name, RID);
	GDVIRTUAL1RC(Dictionary, _font_get_ot_name_strings, RID);

	virtual void font_set_style_name(const RID &p_font_rid, const String &p_name) override;
	virtual String font_get_style_name(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_style_name, RID, const String &);
	GDVIRTUAL1RC(String, _font_get_style_name, RID);

	virtual void font_set_weight(const RID &p_font_rid, int64_t p_weight) override;
	virtual int64_t font_get_weight(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_weight, RID, int64_t);
	GDVIRTUAL1RC(int64_t, _font_get_weight, RID);

	virtual void font_set_stretch(const RID &p_font_rid, int64_t p_stretch) override;
	virtual int64_t font_get_stretch(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_stretch, RID, int64_t);
	GDVIRTUAL1RC(int64_t, _font_get_stretch, RID);

	virtual void font_set_antialiasing(const RID &p_font_rid, TextServer::FontAntialiasing p_antialiasing) override;
	virtual TextServer::FontAntialiasing font_get_antialiasing(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_antialiasing, RID, TextServer::FontAntialiasing);
	GDVIRTUAL1RC(TextServer::FontAntialiasing, _font_get_antialiasing, RID);

	virtual void font_set_disable_embedded_bitmaps(const RID &p_font_rid, bool p_disable_embedded_bitmaps) override;
	virtual bool font_get_disable_embedded_bitmaps(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_disable_embedded_bitmaps, RID, bool);
	GDVIRTUAL1RC(bool, _font_get_disable_embedded_bitmaps, RID);

	virtual void font_set_generate_mipmaps(const RID &p_font_rid, bool p_generate_mipmaps) override;
	virtual bool font_get_generate_mipmaps(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_generate_mipmaps, RID, bool);
	GDVIRTUAL1RC(bool, _font_get_generate_mipmaps, RID);

	virtual void font_set_multichannel_signed_distance_field(const RID &p_font_rid, bool p_msdf) override;
	virtual bool font_is_multichannel_signed_distance_field(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_multichannel_signed_distance_field, RID, bool);
	GDVIRTUAL1RC(bool, _font_is_multichannel_signed_distance_field, RID);

	virtual void font_set_msdf_pixel_range(const RID &p_font_rid, int64_t p_msdf_pixel_range) override;
	virtual int64_t font_get_msdf_pixel_range(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_msdf_pixel_range, RID, int64_t);
	GDVIRTUAL1RC(int64_t, _font_get_msdf_pixel_range, RID);

	virtual void font_set_msdf_size(const RID &p_font_rid, int64_t p_msdf_size) override;
	virtual int64_t font_get_msdf_size(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_msdf_size, RID, int64_t);
	GDVIRTUAL1RC(int64_t, _font_get_msdf_size, RID);

	virtual void font_set_fixed_size(const RID &p_font_rid, int64_t p_fixed_size) override;
	virtual int64_t font_get_fixed_size(const RID &p_font_rid) const override;
	GDVIRTUAL2_REQUIRED(_font_set_fixed_size, RID, int64_t);
	GDVIRTUAL1RC_REQUIRED(int64_t, _font_get_fixed_size, RID);

	virtual void font_set_fixed_size_scale_mode(const RID &p_font_rid, FixedSizeScaleMode p_fixed_size_scale) override;
	virtual FixedSizeScaleMode font_get_fixed_size_scale_mode(const RID &p_font_rid) const override;
	GDVIRTUAL2_REQUIRED(_font_set_fixed_size_scale_mode, RID, FixedSizeScaleMode);
	GDVIRTUAL1RC_REQUIRED(FixedSizeScaleMode, _font_get_fixed_size_scale_mode, RID);

	virtual void font_set_subpixel_positioning(const RID &p_font_rid, SubpixelPositioning p_subpixel) override;
	virtual SubpixelPositioning font_get_subpixel_positioning(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_subpixel_positioning, RID, SubpixelPositioning);
	GDVIRTUAL1RC(SubpixelPositioning, _font_get_subpixel_positioning, RID);

	virtual void font_set_keep_rounding_remainders(const RID &p_font_rid, bool p_keep_rounding_remainders) override;
	virtual bool font_get_keep_rounding_remainders(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_keep_rounding_remainders, RID, bool);
	GDVIRTUAL1RC(bool, _font_get_keep_rounding_remainders, RID);

	virtual void font_set_embolden(const RID &p_font_rid, double p_strength) override;
	virtual double font_get_embolden(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_embolden, RID, double);
	GDVIRTUAL1RC(double, _font_get_embolden, RID);

	virtual void font_set_spacing(const RID &p_font_rid, SpacingType p_spacing, int64_t p_value) override;
	virtual int64_t font_get_spacing(const RID &p_font_rid, SpacingType p_spacing) const override;
	GDVIRTUAL3(_font_set_spacing, const RID &, SpacingType, int64_t);
	GDVIRTUAL2RC(int64_t, _font_get_spacing, const RID &, SpacingType);

	virtual void font_set_baseline_offset(const RID &p_font_rid, double p_baseline_offset) override;
	virtual double font_get_baseline_offset(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_baseline_offset, const RID &, double);
	GDVIRTUAL1RC(double, _font_get_baseline_offset, const RID &);

	virtual void font_set_transform(const RID &p_font_rid, const Transform2D &p_transform) override;
	virtual Transform2D font_get_transform(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_transform, RID, Transform2D);
	GDVIRTUAL1RC(Transform2D, _font_get_transform, RID);

	virtual void font_set_allow_system_fallback(const RID &p_font_rid, bool p_allow_system_fallback) override;
	virtual bool font_is_allow_system_fallback(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_allow_system_fallback, RID, bool);
	GDVIRTUAL1RC(bool, _font_is_allow_system_fallback, RID);

	virtual void font_set_force_autohinter(const RID &p_font_rid, bool p_force_autohinter) override;
	virtual bool font_is_force_autohinter(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_force_autohinter, RID, bool);
	GDVIRTUAL1RC(bool, _font_is_force_autohinter, RID);

	virtual void font_set_hinting(const RID &p_font_rid, Hinting p_hinting) override;
	virtual Hinting font_get_hinting(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_hinting, RID, Hinting);
	GDVIRTUAL1RC(Hinting, _font_get_hinting, RID);

	virtual void font_set_variation_coordinates(const RID &p_font_rid, const Dictionary &p_variation_coordinates) override;
	virtual Dictionary font_get_variation_coordinates(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_variation_coordinates, RID, Dictionary);
	GDVIRTUAL1RC(Dictionary, _font_get_variation_coordinates, RID);

	virtual void font_set_oversampling(const RID &p_font_rid, double p_oversampling) override;
	virtual double font_get_oversampling(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_oversampling, RID, double);
	GDVIRTUAL1RC(double, _font_get_oversampling, RID);

	virtual TypedArray<Vector2i> font_get_size_cache_list(const RID &p_font_rid) const override;
	virtual void font_clear_size_cache(const RID &p_font_rid) override;
	virtual void font_remove_size_cache(const RID &p_font_rid, const Vector2i &p_size) override;
	GDVIRTUAL1RC_REQUIRED(TypedArray<Vector2i>, _font_get_size_cache_list, RID);
	GDVIRTUAL1_REQUIRED(_font_clear_size_cache, RID);
	GDVIRTUAL2_REQUIRED(_font_remove_size_cache, RID, const Vector2i &);

	virtual void font_set_ascent(const RID &p_font_rid, int64_t p_size, double p_ascent) override;
	virtual double font_get_ascent(const RID &p_font_rid, int64_t p_size) const override;
	GDVIRTUAL3_REQUIRED(_font_set_ascent, RID, int64_t, double);
	GDVIRTUAL2RC_REQUIRED(double, _font_get_ascent, RID, int64_t);

	virtual void font_set_descent(const RID &p_font_rid, int64_t p_size, double p_descent) override;
	virtual double font_get_descent(const RID &p_font_rid, int64_t p_size) const override;
	GDVIRTUAL3_REQUIRED(_font_set_descent, RID, int64_t, double);
	GDVIRTUAL2RC_REQUIRED(double, _font_get_descent, RID, int64_t);

	virtual void font_set_underline_position(const RID &p_font_rid, int64_t p_size, double p_underline_position) override;
	virtual double font_get_underline_position(const RID &p_font_rid, int64_t p_size) const override;
	GDVIRTUAL3_REQUIRED(_font_set_underline_position, RID, int64_t, double);
	GDVIRTUAL2RC_REQUIRED(double, _font_get_underline_position, RID, int64_t);

	virtual void font_set_underline_thickness(const RID &p_font_rid, int64_t p_size, double p_underline_thickness) override;
	virtual double font_get_underline_thickness(const RID &p_font_rid, int64_t p_size) const override;
	GDVIRTUAL3_REQUIRED(_font_set_underline_thickness, RID, int64_t, double);
	GDVIRTUAL2RC_REQUIRED(double, _font_get_underline_thickness, RID, int64_t);

	virtual void font_set_scale(const RID &p_font_rid, int64_t p_size, double p_scale) override;
	virtual double font_get_scale(const RID &p_font_rid, int64_t p_size) const override;
	GDVIRTUAL3_REQUIRED(_font_set_scale, RID, int64_t, double);
	GDVIRTUAL2RC_REQUIRED(double, _font_get_scale, RID, int64_t);

	virtual int64_t font_get_texture_count(const RID &p_font_rid, const Vector2i &p_size) const override;
	virtual void font_clear_textures(const RID &p_font_rid, const Vector2i &p_size) override;
	virtual void font_remove_texture(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) override;
	GDVIRTUAL2RC_REQUIRED(int64_t, _font_get_texture_count, RID, const Vector2i &);
	GDVIRTUAL2_REQUIRED(_font_clear_textures, RID, const Vector2i &);
	GDVIRTUAL3_REQUIRED(_font_remove_texture, RID, const Vector2i &, int64_t);

	virtual void font_set_texture_image(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index, const Ref<Image> &p_image) override;
	virtual Ref<Image> font_get_texture_image(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) const override;
	GDVIRTUAL4_REQUIRED(_font_set_texture_image, RID, const Vector2i &, int64_t, const Ref<Image> &);
	GDVIRTUAL3RC_REQUIRED(Ref<Image>, _font_get_texture_image, RID, const Vector2i &, int64_t);

	virtual void font_set_texture_offsets(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index, const PackedInt32Array &p_offset) override;
	virtual PackedInt32Array font_get_texture_offsets(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) const override;
	GDVIRTUAL4(_font_set_texture_offsets, RID, const Vector2i &, int64_t, const PackedInt32Array &);
	GDVIRTUAL3RC(PackedInt32Array, _font_get_texture_offsets, RID, const Vector2i &, int64_t);

	virtual PackedInt32Array font_get_glyph_list(const RID &p_font_rid, const Vector2i &p_size) const override;
	virtual void font_clear_glyphs(const RID &p_font_rid, const Vector2i &p_size) override;
	virtual void font_remove_glyph(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) override;
	GDVIRTUAL2RC_REQUIRED(PackedInt32Array, _font_get_glyph_list, RID, const Vector2i &);
	GDVIRTUAL2_REQUIRED(_font_clear_glyphs, RID, const Vector2i &);
	GDVIRTUAL3_REQUIRED(_font_remove_glyph, RID, const Vector2i &, int64_t);

	virtual Vector2 font_get_glyph_advance(const RID &p_font_rid, int64_t p_size, int64_t p_glyph) const override;
	virtual void font_set_glyph_advance(const RID &p_font_rid, int64_t p_size, int64_t p_glyph, const Vector2 &p_advance) override;
	GDVIRTUAL3RC_REQUIRED(Vector2, _font_get_glyph_advance, RID, int64_t, int64_t);
	GDVIRTUAL4_REQUIRED(_font_set_glyph_advance, RID, int64_t, int64_t, const Vector2 &);

	virtual Vector2 font_get_glyph_offset(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override;
	virtual void font_set_glyph_offset(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Vector2 &p_offset) override;
	GDVIRTUAL3RC_REQUIRED(Vector2, _font_get_glyph_offset, RID, const Vector2i &, int64_t);
	GDVIRTUAL4_REQUIRED(_font_set_glyph_offset, RID, const Vector2i &, int64_t, const Vector2 &);

	virtual Vector2 font_get_glyph_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override;
	virtual void font_set_glyph_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Vector2 &p_gl_size) override;
	GDVIRTUAL3RC_REQUIRED(Vector2, _font_get_glyph_size, RID, const Vector2i &, int64_t);
	GDVIRTUAL4_REQUIRED(_font_set_glyph_size, RID, const Vector2i &, int64_t, const Vector2 &);

	virtual Rect2 font_get_glyph_uv_rect(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override;
	virtual void font_set_glyph_uv_rect(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Rect2 &p_uv_rect) override;
	GDVIRTUAL3RC_REQUIRED(Rect2, _font_get_glyph_uv_rect, RID, const Vector2i &, int64_t);
	GDVIRTUAL4_REQUIRED(_font_set_glyph_uv_rect, RID, const Vector2i &, int64_t, const Rect2 &);

	virtual int64_t font_get_glyph_texture_idx(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override;
	virtual void font_set_glyph_texture_idx(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, int64_t p_texture_idx) override;
	GDVIRTUAL3RC_REQUIRED(int64_t, _font_get_glyph_texture_idx, RID, const Vector2i &, int64_t);
	GDVIRTUAL4_REQUIRED(_font_set_glyph_texture_idx, RID, const Vector2i &, int64_t, int64_t);

	virtual RID font_get_glyph_texture_rid(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override;
	GDVIRTUAL3RC_REQUIRED(RID, _font_get_glyph_texture_rid, RID, const Vector2i &, int64_t);

	virtual Size2 font_get_glyph_texture_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const override;
	GDVIRTUAL3RC_REQUIRED(Size2, _font_get_glyph_texture_size, RID, const Vector2i &, int64_t);

	virtual Dictionary font_get_glyph_contours(const RID &p_font, int64_t p_size, int64_t p_index) const override;
	GDVIRTUAL3RC(Dictionary, _font_get_glyph_contours, RID, int64_t, int64_t);

	virtual TypedArray<Vector2i> font_get_kerning_list(const RID &p_font_rid, int64_t p_size) const override;
	virtual void font_clear_kerning_map(const RID &p_font_rid, int64_t p_size) override;
	virtual void font_remove_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair) override;
	GDVIRTUAL2RC(TypedArray<Vector2i>, _font_get_kerning_list, RID, int64_t);
	GDVIRTUAL2(_font_clear_kerning_map, RID, int64_t);
	GDVIRTUAL3(_font_remove_kerning, RID, int64_t, const Vector2i &);

	virtual void font_set_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair, const Vector2 &p_kerning) override;
	virtual Vector2 font_get_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair) const override;
	GDVIRTUAL4(_font_set_kerning, RID, int64_t, const Vector2i &, const Vector2 &);
	GDVIRTUAL3RC(Vector2, _font_get_kerning, RID, int64_t, const Vector2i &);

	virtual int64_t font_get_glyph_index(const RID &p_font_rid, int64_t p_size, int64_t p_char, int64_t p_variation_selector = 0) const override;
	GDVIRTUAL4RC_REQUIRED(int64_t, _font_get_glyph_index, RID, int64_t, int64_t, int64_t);

	virtual int64_t font_get_char_from_glyph_index(const RID &p_font_rid, int64_t p_size, int64_t p_glyph_index) const override;
	GDVIRTUAL3RC_REQUIRED(int64_t, _font_get_char_from_glyph_index, RID, int64_t, int64_t);

	virtual bool font_has_char(const RID &p_font_rid, int64_t p_char) const override;
	virtual String font_get_supported_chars(const RID &p_font_rid) const override;
	virtual PackedInt32Array font_get_supported_glyphs(const RID &p_font_rid) const override;
	GDVIRTUAL2RC_REQUIRED(bool, _font_has_char, RID, int64_t);
	GDVIRTUAL1RC_REQUIRED(String, _font_get_supported_chars, RID);
	GDVIRTUAL1RC_REQUIRED(PackedInt32Array, _font_get_supported_glyphs, RID);

	virtual void font_render_range(const RID &p_font, const Vector2i &p_size, int64_t p_start, int64_t p_end) override;
	virtual void font_render_glyph(const RID &p_font_rid, const Vector2i &p_size, int64_t p_index) override;
	GDVIRTUAL4(_font_render_range, RID, const Vector2i &, int64_t, int64_t);
	GDVIRTUAL3(_font_render_glyph, RID, const Vector2i &, int64_t);

	virtual void font_draw_glyph(const RID &p_font, const RID &p_canvas, int64_t p_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color = Color(1, 1, 1)) const override;
	virtual void font_draw_glyph_outline(const RID &p_font, const RID &p_canvas, int64_t p_size, int64_t p_outline_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color = Color(1, 1, 1)) const override;
	GDVIRTUAL6C_REQUIRED(_font_draw_glyph, RID, RID, int64_t, const Vector2 &, int64_t, const Color &);
	GDVIRTUAL7C_REQUIRED(_font_draw_glyph_outline, RID, RID, int64_t, int64_t, const Vector2 &, int64_t, const Color &);

	virtual bool font_is_language_supported(const RID &p_font_rid, const String &p_language) const override;
	virtual void font_set_language_support_override(const RID &p_font_rid, const String &p_language, bool p_supported) override;
	virtual bool font_get_language_support_override(const RID &p_font_rid, const String &p_language) override;
	virtual void font_remove_language_support_override(const RID &p_font_rid, const String &p_language) override;
	virtual PackedStringArray font_get_language_support_overrides(const RID &p_font_rid) override;
	GDVIRTUAL2RC(bool, _font_is_language_supported, RID, const String &);
	GDVIRTUAL3(_font_set_language_support_override, RID, const String &, bool);
	GDVIRTUAL2R(bool, _font_get_language_support_override, RID, const String &);
	GDVIRTUAL2(_font_remove_language_support_override, RID, const String &);
	GDVIRTUAL1R(PackedStringArray, _font_get_language_support_overrides, RID);

	virtual bool font_is_script_supported(const RID &p_font_rid, const String &p_script) const override;
	virtual void font_set_script_support_override(const RID &p_font_rid, const String &p_script, bool p_supported) override;
	virtual bool font_get_script_support_override(const RID &p_font_rid, const String &p_script) override;
	virtual void font_remove_script_support_override(const RID &p_font_rid, const String &p_script) override;
	virtual PackedStringArray font_get_script_support_overrides(const RID &p_font_rid) override;
	GDVIRTUAL2RC(bool, _font_is_script_supported, RID, const String &);
	GDVIRTUAL3(_font_set_script_support_override, RID, const String &, bool);
	GDVIRTUAL2R(bool, _font_get_script_support_override, RID, const String &);
	GDVIRTUAL2(_font_remove_script_support_override, RID, const String &);
	GDVIRTUAL1R(PackedStringArray, _font_get_script_support_overrides, RID);

	virtual void font_set_opentype_feature_overrides(const RID &p_font_rid, const Dictionary &p_overrides) override;
	virtual Dictionary font_get_opentype_feature_overrides(const RID &p_font_rid) const override;
	GDVIRTUAL2(_font_set_opentype_feature_overrides, RID, const Dictionary &);
	GDVIRTUAL1RC(Dictionary, _font_get_opentype_feature_overrides, RID);

	virtual Dictionary font_supported_feature_list(const RID &p_font_rid) const override;
	virtual Dictionary font_supported_variation_list(const RID &p_font_rid) const override;
	GDVIRTUAL1RC(Dictionary, _font_supported_feature_list, RID);
	GDVIRTUAL1RC(Dictionary, _font_supported_variation_list, RID);

	virtual double font_get_global_oversampling() const override;
	virtual void font_set_global_oversampling(double p_oversampling) override;
	GDVIRTUAL0RC(double, _font_get_global_oversampling);
	GDVIRTUAL1(_font_set_global_oversampling, double);

	virtual Vector2 get_hex_code_box_size(int64_t p_size, int64_t p_index) const override;
	virtual void draw_hex_code_box(const RID &p_canvas, int64_t p_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color) const override;
	GDVIRTUAL2RC(Vector2, _get_hex_code_box_size, int64_t, int64_t);
	GDVIRTUAL5C(_draw_hex_code_box, RID, int64_t, const Vector2 &, int64_t, const Color &);

	/* Shaped text buffer interface */

	virtual RID create_shaped_text(Direction p_direction = DIRECTION_AUTO, Orientation p_orientation = ORIENTATION_HORIZONTAL) override;
	GDVIRTUAL2R_REQUIRED(RID, _create_shaped_text, Direction, Orientation);

	virtual void shaped_text_clear(const RID &p_shaped) override;
	GDVIRTUAL1_REQUIRED(_shaped_text_clear, RID);

	virtual void shaped_text_set_direction(const RID &p_shaped, Direction p_direction = DIRECTION_AUTO) override;
	virtual Direction shaped_text_get_direction(const RID &p_shaped) const override;
	virtual Direction shaped_text_get_inferred_direction(const RID &p_shaped) const override;
	GDVIRTUAL2(_shaped_text_set_direction, RID, Direction);
	GDVIRTUAL1RC(Direction, _shaped_text_get_direction, RID);
	GDVIRTUAL1RC(Direction, _shaped_text_get_inferred_direction, RID);

	virtual void shaped_text_set_bidi_override(const RID &p_shaped, const Array &p_override) override;
	GDVIRTUAL2(_shaped_text_set_bidi_override, RID, const Array &);

	virtual void shaped_text_set_custom_punctuation(const RID &p_shaped, const String &p_punct) override;
	virtual String shaped_text_get_custom_punctuation(const RID &p_shaped) const override;
	GDVIRTUAL2(_shaped_text_set_custom_punctuation, RID, String);
	GDVIRTUAL1RC(String, _shaped_text_get_custom_punctuation, RID);

	virtual void shaped_text_set_custom_ellipsis(const RID &p_shaped, int64_t p_char) override;
	virtual int64_t shaped_text_get_custom_ellipsis(const RID &p_shaped) const override;
	GDVIRTUAL2(_shaped_text_set_custom_ellipsis, RID, int64_t);
	GDVIRTUAL1RC(int64_t, _shaped_text_get_custom_ellipsis, RID);

	virtual void shaped_text_set_orientation(const RID &p_shaped, Orientation p_orientation = ORIENTATION_HORIZONTAL) override;
	virtual Orientation shaped_text_get_orientation(const RID &p_shaped) const override;
	GDVIRTUAL2(_shaped_text_set_orientation, RID, Orientation);
	GDVIRTUAL1RC(Orientation, _shaped_text_get_orientation, RID);

	virtual void shaped_text_set_preserve_invalid(const RID &p_shaped, bool p_enabled) override;
	virtual bool shaped_text_get_preserve_invalid(const RID &p_shaped) const override;
	GDVIRTUAL2(_shaped_text_set_preserve_invalid, RID, bool);
	GDVIRTUAL1RC(bool, _shaped_text_get_preserve_invalid, RID);

	virtual void shaped_text_set_preserve_control(const RID &p_shaped, bool p_enabled) override;
	virtual bool shaped_text_get_preserve_control(const RID &p_shaped) const override;
	GDVIRTUAL2(_shaped_text_set_preserve_control, RID, bool);
	GDVIRTUAL1RC(bool, _shaped_text_get_preserve_control, RID);

	virtual void shaped_text_set_spacing(const RID &p_shaped, SpacingType p_spacing, int64_t p_value) override;
	virtual int64_t shaped_text_get_spacing(const RID &p_shaped, SpacingType p_spacing) const override;
	GDVIRTUAL3(_shaped_text_set_spacing, RID, SpacingType, int64_t);
	GDVIRTUAL2RC(int64_t, _shaped_text_get_spacing, RID, SpacingType);

	virtual bool shaped_text_add_string(const RID &p_shaped, const String &p_text, const TypedArray<RID> &p_fonts, int64_t p_size, const Dictionary &p_opentype_features = Dictionary(), const String &p_language = "", const Variant &p_meta = Variant()) override;
	virtual bool shaped_text_add_object(const RID &p_shaped, const Variant &p_key, const Size2 &p_size, InlineAlignment p_inline_align = INLINE_ALIGNMENT_CENTER, int64_t p_length = 1, double p_baseline = 0.0) override;
	virtual bool shaped_text_resize_object(const RID &p_shaped, const Variant &p_key, const Size2 &p_size, InlineAlignment p_inline_align = INLINE_ALIGNMENT_CENTER, double p_baseline = 0.0) override;
	GDVIRTUAL7R_REQUIRED(bool, _shaped_text_add_string, RID, const String &, const TypedArray<RID> &, int64_t, const Dictionary &, const String &, const Variant &);
	GDVIRTUAL6R_REQUIRED(bool, _shaped_text_add_object, RID, const Variant &, const Size2 &, InlineAlignment, int64_t, double);
	GDVIRTUAL5R_REQUIRED(bool, _shaped_text_resize_object, RID, const Variant &, const Size2 &, InlineAlignment, double);

	virtual int64_t shaped_get_span_count(const RID &p_shaped) const override;
	virtual Variant shaped_get_span_meta(const RID &p_shaped, int64_t p_index) const override;
	virtual Variant shaped_get_span_embedded_object(const RID &p_shaped, int64_t p_index) const override;
	virtual void shaped_set_span_update_font(const RID &p_shaped, int64_t p_index, const TypedArray<RID> &p_fonts, int64_t p_size, const Dictionary &p_opentype_features = Dictionary()) override;
	GDVIRTUAL1RC_REQUIRED(int64_t, _shaped_get_span_count, RID);
	GDVIRTUAL2RC_REQUIRED(Variant, _shaped_get_span_meta, RID, int64_t);
	GDVIRTUAL2RC_REQUIRED(Variant, _shaped_get_span_embedded_object, RID, int64_t);
	GDVIRTUAL5_REQUIRED(_shaped_set_span_update_font, RID, int64_t, const TypedArray<RID> &, int64_t, const Dictionary &);

	virtual RID shaped_text_substr(const RID &p_shaped, int64_t p_start, int64_t p_length) const override;
	virtual RID shaped_text_get_parent(const RID &p_shaped) const override;
	GDVIRTUAL3RC_REQUIRED(RID, _shaped_text_substr, RID, int64_t, int64_t);
	GDVIRTUAL1RC_REQUIRED(RID, _shaped_text_get_parent, RID);

	virtual double shaped_text_fit_to_width(const RID &p_shaped, double p_width, BitField<TextServer::JustificationFlag> p_jst_flags = JUSTIFICATION_WORD_BOUND | JUSTIFICATION_KASHIDA) override;
	virtual double shaped_text_tab_align(const RID &p_shaped, const PackedFloat32Array &p_tab_stops) override;
	GDVIRTUAL3R(double, _shaped_text_fit_to_width, RID, double, BitField<TextServer::JustificationFlag>);
	GDVIRTUAL2R(double, _shaped_text_tab_align, RID, const PackedFloat32Array &);

	virtual bool shaped_text_shape(const RID &p_shaped) override;
	virtual bool shaped_text_update_breaks(const RID &p_shaped) override;
	virtual bool shaped_text_update_justification_ops(const RID &p_shaped) override;
	GDVIRTUAL1R_REQUIRED(bool, _shaped_text_shape, RID);
	GDVIRTUAL1R(bool, _shaped_text_update_breaks, RID);
	GDVIRTUAL1R(bool, _shaped_text_update_justification_ops, RID);

	virtual bool shaped_text_is_ready(const RID &p_shaped) const override;
	GDVIRTUAL1RC_REQUIRED(bool, _shaped_text_is_ready, RID);

	virtual const Glyph *shaped_text_get_glyphs(const RID &p_shaped) const override;
	virtual const Glyph *shaped_text_sort_logical(const RID &p_shaped) override;
	virtual int64_t shaped_text_get_glyph_count(const RID &p_shaped) const override;
	GDVIRTUAL1RC_REQUIRED(GDExtensionConstPtr<const Glyph>, _shaped_text_get_glyphs, RID);
	GDVIRTUAL1R_REQUIRED(GDExtensionConstPtr<const Glyph>, _shaped_text_sort_logical, RID);
	GDVIRTUAL1RC_REQUIRED(int64_t, _shaped_text_get_glyph_count, RID);

	virtual Vector2i shaped_text_get_range(const RID &p_shaped) const override;
	GDVIRTUAL1RC_REQUIRED(Vector2i, _shaped_text_get_range, RID);

	virtual PackedInt32Array shaped_text_get_line_breaks_adv(const RID &p_shaped, const PackedFloat32Array &p_width, int64_t p_start = 0, bool p_once = true, BitField<TextServer::LineBreakFlag> p_break_flags = BREAK_MANDATORY | BREAK_WORD_BOUND) const override;
	virtual PackedInt32Array shaped_text_get_line_breaks(const RID &p_shaped, double p_width, int64_t p_start = 0, BitField<TextServer::LineBreakFlag> p_break_flags = BREAK_MANDATORY | BREAK_WORD_BOUND) const override;
	virtual PackedInt32Array shaped_text_get_word_breaks(const RID &p_shaped, BitField<TextServer::GraphemeFlag> p_grapheme_flags = GRAPHEME_IS_SPACE | GRAPHEME_IS_PUNCTUATION, BitField<TextServer::GraphemeFlag> p_skip_grapheme_flags = GRAPHEME_IS_VIRTUAL) const override;
	GDVIRTUAL5RC(PackedInt32Array, _shaped_text_get_line_breaks_adv, RID, const PackedFloat32Array &, int64_t, bool, BitField<TextServer::LineBreakFlag>);
	GDVIRTUAL4RC(PackedInt32Array, _shaped_text_get_line_breaks, RID, double, int64_t, BitField<TextServer::LineBreakFlag>);
	GDVIRTUAL3RC(PackedInt32Array, _shaped_text_get_word_breaks, RID, BitField<TextServer::GraphemeFlag>, BitField<TextServer::GraphemeFlag>);

	virtual int64_t shaped_text_get_trim_pos(const RID &p_shaped) const override;
	virtual int64_t shaped_text_get_ellipsis_pos(const RID &p_shaped) const override;
	virtual const Glyph *shaped_text_get_ellipsis_glyphs(const RID &p_shaped) const override;
	virtual int64_t shaped_text_get_ellipsis_glyph_count(const RID &p_shaped) const override;
	GDVIRTUAL1RC_REQUIRED(int64_t, _shaped_text_get_trim_pos, RID);
	GDVIRTUAL1RC_REQUIRED(int64_t, _shaped_text_get_ellipsis_pos, RID);
	GDVIRTUAL1RC_REQUIRED(GDExtensionConstPtr<const Glyph>, _shaped_text_get_ellipsis_glyphs, RID);
	GDVIRTUAL1RC_REQUIRED(int64_t, _shaped_text_get_ellipsis_glyph_count, RID);

	virtual void shaped_text_overrun_trim_to_width(const RID &p_shaped, double p_width, BitField<TextServer::TextOverrunFlag> p_trim_flags) override;
	GDVIRTUAL3(_shaped_text_overrun_trim_to_width, RID, double, BitField<TextServer::TextOverrunFlag>);

	virtual Array shaped_text_get_objects(const RID &p_shaped) const override;
	virtual Rect2 shaped_text_get_object_rect(const RID &p_shaped, const Variant &p_key) const override;
	virtual Vector2i shaped_text_get_object_range(const RID &p_shaped, const Variant &p_key) const override;
	virtual int64_t shaped_text_get_object_glyph(const RID &p_shaped, const Variant &p_key) const override;
	GDVIRTUAL1RC_REQUIRED(Array, _shaped_text_get_objects, RID);
	GDVIRTUAL2RC_REQUIRED(Rect2, _shaped_text_get_object_rect, RID, const Variant &);
	GDVIRTUAL2RC_REQUIRED(Vector2i, _shaped_text_get_object_range, RID, const Variant &);
	GDVIRTUAL2RC_REQUIRED(int64_t, _shaped_text_get_object_glyph, RID, const Variant &);

	virtual Size2 shaped_text_get_size(const RID &p_shaped) const override;
	virtual double shaped_text_get_ascent(const RID &p_shaped) const override;
	virtual double shaped_text_get_descent(const RID &p_shaped) const override;
	virtual double shaped_text_get_width(const RID &p_shaped) const override;
	virtual double shaped_text_get_underline_position(const RID &p_shaped) const override;
	virtual double shaped_text_get_underline_thickness(const RID &p_shaped) const override;
	GDVIRTUAL1RC_REQUIRED(Size2, _shaped_text_get_size, RID);
	GDVIRTUAL1RC_REQUIRED(double, _shaped_text_get_ascent, RID);
	GDVIRTUAL1RC_REQUIRED(double, _shaped_text_get_descent, RID);
	GDVIRTUAL1RC_REQUIRED(double, _shaped_text_get_width, RID);
	GDVIRTUAL1RC_REQUIRED(double, _shaped_text_get_underline_position, RID);
	GDVIRTUAL1RC_REQUIRED(double, _shaped_text_get_underline_thickness, RID);

	virtual Direction shaped_text_get_dominant_direction_in_range(const RID &p_shaped, int64_t p_start, int64_t p_end) const override;
	GDVIRTUAL3RC(int64_t, _shaped_text_get_dominant_direction_in_range, RID, int64_t, int64_t);

	virtual CaretInfo shaped_text_get_carets(const RID &p_shaped, int64_t p_position) const override;
	virtual Vector<Vector2> shaped_text_get_selection(const RID &p_shaped, int64_t p_start, int64_t p_end) const override;
	GDVIRTUAL3C(_shaped_text_get_carets, RID, int64_t, GDExtensionPtr<CaretInfo>);
	GDVIRTUAL3RC(Vector<Vector2>, _shaped_text_get_selection, RID, int64_t, int64_t);

	virtual int64_t shaped_text_hit_test_grapheme(const RID &p_shaped, double p_coords) const override;
	virtual int64_t shaped_text_hit_test_position(const RID &p_shaped, double p_coords) const override;
	GDVIRTUAL2RC(int64_t, _shaped_text_hit_test_grapheme, RID, double);
	GDVIRTUAL2RC(int64_t, _shaped_text_hit_test_position, RID, double);

	virtual void shaped_text_draw(const RID &p_shaped, const RID &p_canvas, const Vector2 &p_pos, double p_clip_l = -1.0, double p_clip_r = -1.0, const Color &p_color = Color(1, 1, 1)) const override;
	virtual void shaped_text_draw_outline(const RID &p_shaped, const RID &p_canvas, const Vector2 &p_pos, double p_clip_l = -1.0, double p_clip_r = -1.0, int64_t p_outline_size = 1, const Color &p_color = Color(1, 1, 1)) const override;
	GDVIRTUAL6C(_shaped_text_draw, RID, RID, const Vector2 &, double, double, const Color &);
	GDVIRTUAL7C(_shaped_text_draw_outline, RID, RID, const Vector2 &, double, double, int64_t, const Color &);

	virtual Vector2 shaped_text_get_grapheme_bounds(const RID &p_shaped, int64_t p_pos) const override;
	virtual int64_t shaped_text_next_grapheme_pos(const RID &p_shaped, int64_t p_pos) const override;
	virtual int64_t shaped_text_prev_grapheme_pos(const RID &p_shaped, int64_t p_pos) const override;
	GDVIRTUAL2RC(Vector2, _shaped_text_get_grapheme_bounds, RID, int64_t);
	GDVIRTUAL2RC(int64_t, _shaped_text_next_grapheme_pos, RID, int64_t);
	GDVIRTUAL2RC(int64_t, _shaped_text_prev_grapheme_pos, RID, int64_t);

	virtual PackedInt32Array shaped_text_get_character_breaks(const RID &p_shaped) const override;
	virtual int64_t shaped_text_next_character_pos(const RID &p_shaped, int64_t p_pos) const override;
	virtual int64_t shaped_text_prev_character_pos(const RID &p_shaped, int64_t p_pos) const override;
	virtual int64_t shaped_text_closest_character_pos(const RID &p_shaped, int64_t p_pos) const override;
	GDVIRTUAL1RC(PackedInt32Array, _shaped_text_get_character_breaks, RID);
	GDVIRTUAL2RC(int64_t, _shaped_text_next_character_pos, RID, int64_t);
	GDVIRTUAL2RC(int64_t, _shaped_text_prev_character_pos, RID, int64_t);
	GDVIRTUAL2RC(int64_t, _shaped_text_closest_character_pos, RID, int64_t);

	virtual String format_number(const String &p_string, const String &p_language = "") const override;
	virtual String parse_number(const String &p_string, const String &p_language = "") const override;
	virtual String percent_sign(const String &p_language = "") const override;
	GDVIRTUAL2RC(String, _format_number, const String &, const String &);
	GDVIRTUAL2RC(String, _parse_number, const String &, const String &);
	GDVIRTUAL1RC(String, _percent_sign, const String &);

	virtual String strip_diacritics(const String &p_string) const override;
	GDVIRTUAL1RC(String, _strip_diacritics, const String &);

	virtual PackedInt32Array string_get_word_breaks(const String &p_string, const String &p_language = "", int64_t p_chars_per_line = 0) const override;
	GDVIRTUAL3RC(PackedInt32Array, _string_get_word_breaks, const String &, const String &, int64_t);

	virtual PackedInt32Array string_get_character_breaks(const String &p_string, const String &p_language = "") const override;
	GDVIRTUAL2RC(PackedInt32Array, _string_get_character_breaks, const String &, const String &);

	virtual bool is_valid_identifier(const String &p_string) const override;
	GDVIRTUAL1RC(bool, _is_valid_identifier, const String &);
	virtual bool is_valid_letter(uint64_t p_unicode) const override;
	GDVIRTUAL1RC(bool, _is_valid_letter, uint64_t);

	virtual String string_to_upper(const String &p_string, const String &p_language = "") const override;
	virtual String string_to_lower(const String &p_string, const String &p_language = "") const override;
	virtual String string_to_title(const String &p_string, const String &p_language = "") const override;
	GDVIRTUAL2RC(String, _string_to_upper, const String &, const String &);
	GDVIRTUAL2RC(String, _string_to_lower, const String &, const String &);
	GDVIRTUAL2RC(String, _string_to_title, const String &, const String &);

	TypedArray<Vector3i> parse_structured_text(StructuredTextParser p_parser_type, const Array &p_args, const String &p_text) const;
	GDVIRTUAL3RC(TypedArray<Vector3i>, _parse_structured_text, StructuredTextParser, const Array &, const String &);

	virtual int64_t is_confusable(const String &p_string, const PackedStringArray &p_dict) const override;
	virtual bool spoof_check(const String &p_string) const override;
	GDVIRTUAL2RC(int64_t, _is_confusable, const String &, const PackedStringArray &);
	GDVIRTUAL1RC(bool, _spoof_check, const String &);

	virtual void cleanup() override;
	GDVIRTUAL0(_cleanup);

	TextServerExtension();
	~TextServerExtension();
};
