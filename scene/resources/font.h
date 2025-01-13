/**************************************************************************/
/*  font.h                                                                */
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

#ifndef FONT_H
#define FONT_H

#include "core/io/resource.h"
#include "core/templates/lru.h"
#include "scene/resources/texture.h"
#include "servers/text_server.h"

class TextLine;
class TextParagraph;

/*************************************************************************/
/*  Font                                                                 */
/*************************************************************************/

class Font : public Resource {
	GDCLASS(Font, Resource);

	struct ShapedTextKey {
		String text;
		int font_size = 14;
		float width = 0.f;
		BitField<TextServer::JustificationFlag> jst_flags = TextServer::JUSTIFICATION_NONE;
		BitField<TextServer::LineBreakFlag> brk_flags = TextServer::BREAK_MANDATORY;
		TextServer::Direction direction = TextServer::DIRECTION_AUTO;
		TextServer::Orientation orientation = TextServer::ORIENTATION_HORIZONTAL;

		bool operator==(const ShapedTextKey &p_b) const {
			return (font_size == p_b.font_size) && (width == p_b.width) && (jst_flags == p_b.jst_flags) && (brk_flags == p_b.brk_flags) && (direction == p_b.direction) && (orientation == p_b.orientation) && (text == p_b.text);
		}

		ShapedTextKey() {}
		ShapedTextKey(const String &p_text, int p_font_size, float p_width, BitField<TextServer::JustificationFlag> p_jst_flags, BitField<TextServer::LineBreakFlag> p_brk_flags, TextServer::Direction p_direction, TextServer::Orientation p_orientation) {
			text = p_text;
			font_size = p_font_size;
			width = p_width;
			jst_flags = p_jst_flags;
			brk_flags = p_brk_flags;
			direction = p_direction;
			orientation = p_orientation;
		}
	};

	struct ShapedTextKeyHasher {
		_FORCE_INLINE_ static uint32_t hash(const ShapedTextKey &p_a) {
			uint32_t hash = p_a.text.hash();
			hash = hash_murmur3_one_32(p_a.font_size, hash);
			hash = hash_murmur3_one_float(p_a.width, hash);
			hash = hash_murmur3_one_32(p_a.brk_flags | (p_a.jst_flags << 6) | (p_a.direction << 12) | (p_a.orientation << 15), hash);
			return hash_fmix32(hash);
		}
	};

	// Shaped string cache.
	mutable LRUCache<ShapedTextKey, Ref<TextLine>, ShapedTextKeyHasher> cache;
	mutable LRUCache<ShapedTextKey, Ref<TextParagraph>, ShapedTextKeyHasher> cache_wrap;

protected:
	// Output.
	mutable TypedArray<RID> rids;
	mutable bool dirty_rids = true;

	// Fallbacks.
	static constexpr int MAX_FALLBACK_DEPTH = 64;
	TypedArray<Font> fallbacks;

	static void _bind_methods();

	virtual void _update_rids_fb(const Font *p_f, int p_depth) const;
	virtual void _update_rids() const;
	virtual void reset_state() override;

#ifndef DISABLE_DEPRECATED
	RID _find_variation_compat_80954(const Dictionary &p_variation_coordinates, int p_face_index = 0, float p_strength = 0.0, Transform2D p_transform = Transform2D()) const;
	RID _find_variation_compat_87668(const Dictionary &p_variation_coordinates, int p_face_index = 0, float p_strength = 0.0, Transform2D p_transform = Transform2D(), int p_spacing_top = 0, int p_spacing_bottom = 0, int p_spacing_space = 0, int p_spacing_glyph = 0) const;
	static void _bind_compatibility_methods();
#endif

public:
	virtual bool _is_cyclic(const Ref<Font> &p_f, int p_depth) const;
	virtual bool _is_base_cyclic(const Ref<Font> &p_f, int p_depth) const;
	virtual void _invalidate_rids();

	static constexpr int DEFAULT_FONT_SIZE = 16;

	// Fallbacks.
	virtual void set_fallbacks(const TypedArray<Font> &p_fallbacks);
	virtual TypedArray<Font> get_fallbacks() const;

	// Output.
	virtual RID find_variation(const Dictionary &p_variation_coordinates, int p_face_index = 0, float p_strength = 0.0, Transform2D p_transform = Transform2D(), int p_spacing_top = 0, int p_spacing_bottom = 0, int p_spacing_space = 0, int p_spacing_glyph = 0, float p_baseline_offset = 0.0) const { return RID(); }
	virtual RID _get_rid() const { return RID(); }
	virtual TypedArray<RID> get_rids() const;

	// Font metrics.
	virtual real_t get_height(int p_font_size) const;
	virtual real_t get_ascent(int p_font_size) const;
	virtual real_t get_descent(int p_font_size) const;
	virtual real_t get_underline_position(int p_font_size) const;
	virtual real_t get_underline_thickness(int p_font_size) const;

	virtual String get_font_name() const;
	virtual String get_font_style_name() const;
	virtual Dictionary get_ot_name_strings() const;
	virtual BitField<TextServer::FontStyle> get_font_style() const;
	virtual int get_font_weight() const;
	virtual int get_font_stretch() const;

	virtual int get_spacing(TextServer::SpacingType p_spacing) const { return 0; }
	virtual Dictionary get_opentype_features() const;

	// Drawing string.
	virtual void set_cache_capacity(int p_single_line, int p_multi_line);

	virtual Size2 get_string_size(const String &p_text, HorizontalAlignment p_alignment = HORIZONTAL_ALIGNMENT_LEFT, float p_width = -1, int p_font_size = DEFAULT_FONT_SIZE, BitField<TextServer::JustificationFlag> p_jst_flags = TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_WORD_BOUND, TextServer::Direction p_direction = TextServer::DIRECTION_AUTO, TextServer::Orientation p_orientation = TextServer::ORIENTATION_HORIZONTAL) const;
	virtual Size2 get_multiline_string_size(const String &p_text, HorizontalAlignment p_alignment = HORIZONTAL_ALIGNMENT_LEFT, float p_width = -1, int p_font_size = DEFAULT_FONT_SIZE, int p_max_lines = -1, BitField<TextServer::LineBreakFlag> p_brk_flags = TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND, BitField<TextServer::JustificationFlag> p_jst_flags = TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_WORD_BOUND, TextServer::Direction p_direction = TextServer::DIRECTION_AUTO, TextServer::Orientation p_orientation = TextServer::ORIENTATION_HORIZONTAL) const;

	virtual void draw_string(RID p_canvas_item, const Point2 &p_pos, const String &p_text, HorizontalAlignment p_alignment = HORIZONTAL_ALIGNMENT_LEFT, float p_width = -1, int p_font_size = DEFAULT_FONT_SIZE, const Color &p_modulate = Color(1.0, 1.0, 1.0), BitField<TextServer::JustificationFlag> p_jst_flags = TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_WORD_BOUND, TextServer::Direction p_direction = TextServer::DIRECTION_AUTO, TextServer::Orientation p_orientation = TextServer::ORIENTATION_HORIZONTAL) const;
	virtual void draw_multiline_string(RID p_canvas_item, const Point2 &p_pos, const String &p_text, HorizontalAlignment p_alignment = HORIZONTAL_ALIGNMENT_LEFT, float p_width = -1, int p_font_size = DEFAULT_FONT_SIZE, int p_max_lines = -1, const Color &p_modulate = Color(1.0, 1.0, 1.0), BitField<TextServer::LineBreakFlag> p_brk_flags = TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND, BitField<TextServer::JustificationFlag> p_jst_flags = TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_WORD_BOUND, TextServer::Direction p_direction = TextServer::DIRECTION_AUTO, TextServer::Orientation p_orientation = TextServer::ORIENTATION_HORIZONTAL) const;

	virtual void draw_string_outline(RID p_canvas_item, const Point2 &p_pos, const String &p_text, HorizontalAlignment p_alignment = HORIZONTAL_ALIGNMENT_LEFT, float p_width = -1, int p_font_size = DEFAULT_FONT_SIZE, int p_size = 1, const Color &p_modulate = Color(1.0, 1.0, 1.0), BitField<TextServer::JustificationFlag> p_jst_flags = TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_WORD_BOUND, TextServer::Direction p_direction = TextServer::DIRECTION_AUTO, TextServer::Orientation p_orientation = TextServer::ORIENTATION_HORIZONTAL) const;
	virtual void draw_multiline_string_outline(RID p_canvas_item, const Point2 &p_pos, const String &p_text, HorizontalAlignment p_alignment = HORIZONTAL_ALIGNMENT_LEFT, float p_width = -1, int p_font_size = DEFAULT_FONT_SIZE, int p_max_lines = -1, int p_size = 1, const Color &p_modulate = Color(1.0, 1.0, 1.0), BitField<TextServer::LineBreakFlag> p_brk_flags = TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND, BitField<TextServer::JustificationFlag> p_jst_flags = TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_WORD_BOUND, TextServer::Direction p_direction = TextServer::DIRECTION_AUTO, TextServer::Orientation p_orientation = TextServer::ORIENTATION_HORIZONTAL) const;

	// Drawing char.
	virtual Size2 get_char_size(char32_t p_char, int p_font_size = DEFAULT_FONT_SIZE) const;
	virtual real_t draw_char(RID p_canvas_item, const Point2 &p_pos, char32_t p_char, int p_font_size = DEFAULT_FONT_SIZE, const Color &p_modulate = Color(1.0, 1.0, 1.0)) const;
	virtual real_t draw_char_outline(RID p_canvas_item, const Point2 &p_pos, char32_t p_char, int p_font_size = DEFAULT_FONT_SIZE, int p_size = 1, const Color &p_modulate = Color(1.0, 1.0, 1.0)) const;

	// Helper functions.
	virtual bool has_char(char32_t p_char) const;
	virtual String get_supported_chars() const;

	virtual bool is_language_supported(const String &p_language) const;
	virtual bool is_script_supported(const String &p_script) const;

	virtual Dictionary get_supported_feature_list() const;
	virtual Dictionary get_supported_variation_list() const;
	virtual int64_t get_face_count() const;

	Font();
	~Font();
};

/*************************************************************************/
/*  FontFile                                                             */
/*************************************************************************/

class FontFile : public Font {
	GDCLASS(FontFile, Font);
	RES_BASE_EXTENSION("fontdata");

	// Font source data.
	const uint8_t *data_ptr = nullptr;
	size_t data_size = 0;
	mutable PackedByteArray data;

	TextServer::FontAntialiasing antialiasing = TextServer::FONT_ANTIALIASING_GRAY;
	bool mipmaps = false;
	bool disable_embedded_bitmaps = true;
	bool msdf = false;
	int msdf_pixel_range = 16;
	int msdf_size = 48;
	int fixed_size = 0;
	TextServer::FixedSizeScaleMode fixed_size_scale_mode = TextServer::FIXED_SIZE_SCALE_DISABLE;
	bool force_autohinter = false;
	bool allow_system_fallback = true;
	TextServer::Hinting hinting = TextServer::HINTING_LIGHT;
	TextServer::SubpixelPositioning subpixel_positioning = TextServer::SUBPIXEL_POSITIONING_AUTO;
	bool keep_rounding_remainders = true;
	real_t oversampling = 0.f;

#ifndef DISABLE_DEPRECATED
	real_t bmp_height = 0.0;
	real_t bmp_ascent = 0.0;
#endif

	// Cache.
	mutable Vector<RID> cache;

	_FORCE_INLINE_ void _clear_cache();
	_FORCE_INLINE_ void _ensure_rid(int p_cache_index, int p_make_linked_from = -1) const;

	void _convert_packed_8bit(Ref<Image> &p_source, int p_page, int p_sz);
	void _convert_packed_4bit(Ref<Image> &p_source, int p_page, int p_sz);
	void _convert_rgba_4bit(Ref<Image> &p_source, int p_page, int p_sz);
	void _convert_mono_8bit(Ref<Image> &p_source, int p_page, int p_ch, int p_sz, int p_ol);
	void _convert_mono_4bit(Ref<Image> &p_source, int p_page, int p_ch, int p_sz, int p_ol);

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	virtual void reset_state() override;

public:
	Error _load_bitmap_font(const String &p_path, List<String> *r_image_files);

	Error load_bitmap_font(const String &p_path);
	Error load_dynamic_font(const String &p_path);

	// Font source data.
	virtual void set_data_ptr(const uint8_t *p_data, size_t p_size);
	virtual void set_data(const PackedByteArray &p_data);
	virtual PackedByteArray get_data() const;

	// Common properties.
	virtual void set_font_name(const String &p_name);
	virtual void set_font_style_name(const String &p_name);
	virtual void set_font_style(BitField<TextServer::FontStyle> p_style);
	virtual void set_font_weight(int p_weight);
	virtual void set_font_stretch(int p_stretch);

	virtual void set_antialiasing(TextServer::FontAntialiasing p_antialiasing);
	virtual TextServer::FontAntialiasing get_antialiasing() const;

	virtual void set_disable_embedded_bitmaps(bool p_disable_embedded_bitmaps);
	virtual bool get_disable_embedded_bitmaps() const;

	virtual void set_generate_mipmaps(bool p_generate_mipmaps);
	virtual bool get_generate_mipmaps() const;

	virtual void set_multichannel_signed_distance_field(bool p_msdf);
	virtual bool is_multichannel_signed_distance_field() const;

	virtual void set_msdf_pixel_range(int p_msdf_pixel_range);
	virtual int get_msdf_pixel_range() const;

	virtual void set_msdf_size(int p_msdf_size);
	virtual int get_msdf_size() const;

	virtual void set_fixed_size(int p_fixed_size);
	virtual int get_fixed_size() const;

	virtual void set_fixed_size_scale_mode(TextServer::FixedSizeScaleMode p_fixed_size_scale_mode);
	virtual TextServer::FixedSizeScaleMode get_fixed_size_scale_mode() const;

	virtual void set_allow_system_fallback(bool p_allow_system_fallback);
	virtual bool is_allow_system_fallback() const;

	virtual void set_force_autohinter(bool p_force_autohinter);
	virtual bool is_force_autohinter() const;

	virtual void set_hinting(TextServer::Hinting p_hinting);
	virtual TextServer::Hinting get_hinting() const;

	virtual void set_subpixel_positioning(TextServer::SubpixelPositioning p_subpixel);
	virtual TextServer::SubpixelPositioning get_subpixel_positioning() const;

	virtual void set_keep_rounding_remainders(bool p_keep_rounding_remainders);
	virtual bool get_keep_rounding_remainders() const;

	virtual void set_oversampling(real_t p_oversampling);
	virtual real_t get_oversampling() const;

	// Cache.
	virtual RID find_variation(const Dictionary &p_variation_coordinates, int p_face_index = 0, float p_strength = 0.0, Transform2D p_transform = Transform2D(), int p_spacing_top = 0, int p_spacing_bottom = 0, int p_spacing_space = 0, int p_spacing_glyph = 0, float p_baseline_offset = 0.0) const override;
	virtual RID _get_rid() const override;

	virtual int get_cache_count() const;
	virtual void clear_cache();
	virtual void remove_cache(int p_cache_index);

	virtual TypedArray<Vector2i> get_size_cache_list(int p_cache_index) const;
	virtual void clear_size_cache(int p_cache_index);
	virtual void remove_size_cache(int p_cache_index, const Vector2i &p_size);

	virtual void set_variation_coordinates(int p_cache_index, const Dictionary &p_variation_coordinates);
	virtual Dictionary get_variation_coordinates(int p_cache_index) const;

	virtual void set_embolden(int p_cache_index, float p_strength);
	virtual float get_embolden(int p_cache_index) const;

	virtual void set_transform(int p_cache_index, Transform2D p_transform);
	virtual Transform2D get_transform(int p_cache_index) const;

	virtual void set_extra_spacing(int p_cache_index, TextServer::SpacingType p_spacing, int64_t p_value);
	virtual int64_t get_extra_spacing(int p_cache_index, TextServer::SpacingType p_spacing) const;

	virtual float get_extra_baseline_offset(int p_cache_index) const;
	virtual void set_extra_baseline_offset(int p_cache_index, float p_baseline_offset);

	virtual void set_face_index(int p_cache_index, int64_t p_index);
	virtual int64_t get_face_index(int p_cache_index) const;

	virtual void set_cache_ascent(int p_cache_index, int p_size, real_t p_ascent);
	virtual real_t get_cache_ascent(int p_cache_index, int p_size) const;

	virtual void set_cache_descent(int p_cache_index, int p_size, real_t p_descent);
	virtual real_t get_cache_descent(int p_cache_index, int p_size) const;

	virtual void set_cache_underline_position(int p_cache_index, int p_size, real_t p_underline_position);
	virtual real_t get_cache_underline_position(int p_cache_index, int p_size) const;

	virtual void set_cache_underline_thickness(int p_cache_index, int p_size, real_t p_underline_thickness);
	virtual real_t get_cache_underline_thickness(int p_cache_index, int p_size) const;

	virtual void set_cache_scale(int p_cache_index, int p_size, real_t p_scale); // Rendering scale for bitmap fonts (e.g. emoji fonts).
	virtual real_t get_cache_scale(int p_cache_index, int p_size) const;

	virtual int get_texture_count(int p_cache_index, const Vector2i &p_size) const;
	virtual void clear_textures(int p_cache_index, const Vector2i &p_size);
	virtual void remove_texture(int p_cache_index, const Vector2i &p_size, int p_texture_index);

	virtual void set_texture_image(int p_cache_index, const Vector2i &p_size, int p_texture_index, const Ref<Image> &p_image);
	virtual Ref<Image> get_texture_image(int p_cache_index, const Vector2i &p_size, int p_texture_index) const;

	virtual void set_texture_offsets(int p_cache_index, const Vector2i &p_size, int p_texture_index, const PackedInt32Array &p_offset);
	virtual PackedInt32Array get_texture_offsets(int p_cache_index, const Vector2i &p_size, int p_texture_index) const;

	virtual PackedInt32Array get_glyph_list(int p_cache_index, const Vector2i &p_size) const;
	virtual void clear_glyphs(int p_cache_index, const Vector2i &p_size);
	virtual void remove_glyph(int p_cache_index, const Vector2i &p_size, int32_t p_glyph);

	virtual void set_glyph_advance(int p_cache_index, int p_size, int32_t p_glyph, const Vector2 &p_advance);
	virtual Vector2 get_glyph_advance(int p_cache_index, int p_size, int32_t p_glyph) const;

	virtual void set_glyph_offset(int p_cache_index, const Vector2i &p_size, int32_t p_glyph, const Vector2 &p_offset);
	virtual Vector2 get_glyph_offset(int p_cache_index, const Vector2i &p_size, int32_t p_glyph) const;

	virtual void set_glyph_size(int p_cache_index, const Vector2i &p_size, int32_t p_glyph, const Vector2 &p_gl_size);
	virtual Vector2 get_glyph_size(int p_cache_index, const Vector2i &p_size, int32_t p_glyph) const;

	virtual void set_glyph_uv_rect(int p_cache_index, const Vector2i &p_size, int32_t p_glyph, const Rect2 &p_uv_rect);
	virtual Rect2 get_glyph_uv_rect(int p_cache_index, const Vector2i &p_size, int32_t p_glyph) const;

	virtual void set_glyph_texture_idx(int p_cache_index, const Vector2i &p_size, int32_t p_glyph, int p_texture_idx);
	virtual int get_glyph_texture_idx(int p_cache_index, const Vector2i &p_size, int32_t p_glyph) const;

	virtual TypedArray<Vector2i> get_kerning_list(int p_cache_index, int p_size) const;
	virtual void clear_kerning_map(int p_cache_index, int p_size);
	virtual void remove_kerning(int p_cache_index, int p_size, const Vector2i &p_glyph_pair);

	virtual void set_kerning(int p_cache_index, int p_size, const Vector2i &p_glyph_pair, const Vector2 &p_kerning);
	virtual Vector2 get_kerning(int p_cache_index, int p_size, const Vector2i &p_glyph_pair) const;

	virtual void render_range(int p_cache_index, const Vector2i &p_size, char32_t p_start, char32_t p_end);
	virtual void render_glyph(int p_cache_index, const Vector2i &p_size, int32_t p_index);

	// Language/script support override.
	virtual void set_language_support_override(const String &p_language, bool p_supported);
	virtual bool get_language_support_override(const String &p_language) const;
	virtual void remove_language_support_override(const String &p_language);
	virtual Vector<String> get_language_support_overrides() const;

	virtual void set_script_support_override(const String &p_script, bool p_supported);
	virtual bool get_script_support_override(const String &p_script) const;
	virtual void remove_script_support_override(const String &p_script);
	virtual Vector<String> get_script_support_overrides() const;

	virtual void set_opentype_feature_overrides(const Dictionary &p_overrides);
	virtual Dictionary get_opentype_feature_overrides() const;

	// Base font properties.
	virtual int32_t get_glyph_index(int p_size, char32_t p_char, char32_t p_variation_selector = 0x0000) const;
	virtual char32_t get_char_from_glyph_index(int p_size, int32_t p_glyph_index) const;

	FontFile();
	~FontFile();
};

/*************************************************************************/
/*  FontVariation                                                        */
/*************************************************************************/

class FontVariation : public Font {
	GDCLASS(FontVariation, Font);

	struct Variation {
		Dictionary opentype;
		real_t embolden = 0.f;
		int face_index = 0;
		Transform2D transform;
	};

	mutable Ref<Font> theme_font;

	Ref<Font> base_font;

	Variation variation;
	Dictionary opentype_features;
	int extra_spacing[TextServer::SPACING_MAX];
	float baseline_offset = 0.0;

protected:
	static void _bind_methods();

	virtual void _update_rids() const override;

	virtual void reset_state() override;

public:
	virtual void set_base_font(const Ref<Font> &p_font);
	virtual Ref<Font> get_base_font() const;
	virtual Ref<Font> _get_base_font_or_default() const;

	virtual void set_variation_opentype(const Dictionary &p_coords);
	virtual Dictionary get_variation_opentype() const;

	virtual void set_variation_embolden(float p_strength);
	virtual float get_variation_embolden() const;

	virtual void set_variation_transform(Transform2D p_transform);
	virtual Transform2D get_variation_transform() const;

	virtual void set_variation_face_index(int p_face_index);
	virtual int get_variation_face_index() const;

	virtual void set_opentype_features(const Dictionary &p_features);
	virtual Dictionary get_opentype_features() const override;

	virtual void set_spacing(TextServer::SpacingType p_spacing, int p_value);
	virtual int get_spacing(TextServer::SpacingType p_spacing) const override;

	virtual float get_baseline_offset() const;
	virtual void set_baseline_offset(float p_baseline_offset);

	// Output.
	virtual RID find_variation(const Dictionary &p_variation_coordinates, int p_face_index = 0, float p_strength = 0.0, Transform2D p_transform = Transform2D(), int p_spacing_top = 0, int p_spacing_bottom = 0, int p_spacing_space = 0, int p_spacing_glyph = 0, float p_baseline_offset = 0.0) const override;
	virtual RID _get_rid() const override;

	FontVariation();
	~FontVariation();
};

/*************************************************************************/
/*  SystemFont                                                           */
/*************************************************************************/

class SystemFont : public Font {
	GDCLASS(SystemFont, Font);

	PackedStringArray names;
	bool italic = false;
	int weight = 400;
	int stretch = 100;

	mutable Ref<Font> theme_font;

	Ref<FontFile> base_font;
	Vector<int> face_indices;
	int ftr_weight = 0;
	int ftr_stretch = 0;
	int ftr_italic = 0;

	TextServer::FontAntialiasing antialiasing = TextServer::FONT_ANTIALIASING_GRAY;
	bool mipmaps = false;
	bool disable_embedded_bitmaps = true;
	bool force_autohinter = false;
	bool allow_system_fallback = true;
	TextServer::Hinting hinting = TextServer::HINTING_LIGHT;
	TextServer::SubpixelPositioning subpixel_positioning = TextServer::SUBPIXEL_POSITIONING_AUTO;
	bool keep_rounding_remainders = true;
	real_t oversampling = 0.f;
	bool msdf = false;
	int msdf_pixel_range = 16;
	int msdf_size = 48;

protected:
	static void _bind_methods();

	virtual void _update_base_font();
	virtual void _update_rids() const override;

	virtual void reset_state() override;

public:
	virtual Ref<Font> get_base_font() const { return base_font; }
	virtual Ref<Font> _get_base_font_or_default() const;

	virtual void set_antialiasing(TextServer::FontAntialiasing p_antialiasing);
	virtual TextServer::FontAntialiasing get_antialiasing() const;

	virtual void set_disable_embedded_bitmaps(bool p_disable_embedded_bitmaps);
	virtual bool get_disable_embedded_bitmaps() const;

	virtual void set_generate_mipmaps(bool p_generate_mipmaps);
	virtual bool get_generate_mipmaps() const;

	virtual void set_allow_system_fallback(bool p_allow_system_fallback);
	virtual bool is_allow_system_fallback() const;

	virtual void set_force_autohinter(bool p_force_autohinter);
	virtual bool is_force_autohinter() const;

	virtual void set_hinting(TextServer::Hinting p_hinting);
	virtual TextServer::Hinting get_hinting() const;

	virtual void set_subpixel_positioning(TextServer::SubpixelPositioning p_subpixel);
	virtual TextServer::SubpixelPositioning get_subpixel_positioning() const;

	virtual void set_keep_rounding_remainders(bool p_keep_rounding_remainders);
	virtual bool get_keep_rounding_remainders() const;

	virtual void set_oversampling(real_t p_oversampling);
	virtual real_t get_oversampling() const;

	virtual void set_multichannel_signed_distance_field(bool p_msdf);
	virtual bool is_multichannel_signed_distance_field() const;

	virtual void set_msdf_pixel_range(int p_msdf_pixel_range);
	virtual int get_msdf_pixel_range() const;

	virtual void set_msdf_size(int p_msdf_size);
	virtual int get_msdf_size() const;

	virtual void set_font_names(const PackedStringArray &p_names);
	virtual PackedStringArray get_font_names() const;

	virtual void set_font_italic(bool p_italic);
	virtual bool get_font_italic() const;

	virtual void set_font_weight(int p_weight);
	virtual int get_font_weight() const override;

	virtual void set_font_stretch(int p_stretch);
	virtual int get_font_stretch() const override;

	virtual int get_spacing(TextServer::SpacingType p_spacing) const override;

	virtual RID find_variation(const Dictionary &p_variation_coordinates, int p_face_index = 0, float p_strength = 0.0, Transform2D p_transform = Transform2D(), int p_spacing_top = 0, int p_spacing_bottom = 0, int p_spacing_space = 0, int p_spacing_glyph = 0, float p_baseline_offset = 0.0) const override;
	virtual RID _get_rid() const override;

	int64_t get_face_count() const override;

	SystemFont();
	~SystemFont();
};

#endif // FONT_H
