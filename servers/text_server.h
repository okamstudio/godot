/**************************************************************************/
/*  text_server.h                                                         */
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

#ifndef TEXT_SERVER_H
#define TEXT_SERVER_H

#include "core/io/image.h"
#include "core/object/ref_counted.h"
#include "core/templates/rid.h"
#include "core/variant/native_ptr.h"
#include "core/variant/variant.h"

template <typename T>
class TypedArray;

struct Glyph;
struct CaretInfo;

#define OT_TAG(m_c1, m_c2, m_c3, m_c4) ((int32_t)((((uint32_t)(m_c1) & 0xff) << 24) | (((uint32_t)(m_c2) & 0xff) << 16) | (((uint32_t)(m_c3) & 0xff) << 8) | ((uint32_t)(m_c4) & 0xff)))

class TextServer : public RefCounted {
	GDCLASS(TextServer, RefCounted);

public:
	enum FontAntialiasing {
		FONT_ANTIALIASING_NONE,
		FONT_ANTIALIASING_GRAY,
		FONT_ANTIALIASING_LCD,
	};

	enum FontLCDSubpixelLayout {
		FONT_LCD_SUBPIXEL_LAYOUT_NONE,
		FONT_LCD_SUBPIXEL_LAYOUT_HRGB,
		FONT_LCD_SUBPIXEL_LAYOUT_HBGR,
		FONT_LCD_SUBPIXEL_LAYOUT_VRGB,
		FONT_LCD_SUBPIXEL_LAYOUT_VBGR,
		FONT_LCD_SUBPIXEL_LAYOUT_MAX,
	};

	enum Direction {
		DIRECTION_AUTO,
		DIRECTION_LTR,
		DIRECTION_RTL,
		DIRECTION_INHERITED,
	};

	enum Orientation {
		ORIENTATION_HORIZONTAL,
		ORIENTATION_VERTICAL
	};

	enum JustificationFlag {
		JUSTIFICATION_NONE = 0,
		JUSTIFICATION_KASHIDA = 1 << 0,
		JUSTIFICATION_WORD_BOUND = 1 << 1,
		JUSTIFICATION_TRIM_EDGE_SPACES = 1 << 2,
		JUSTIFICATION_AFTER_LAST_TAB = 1 << 3,
		JUSTIFICATION_CONSTRAIN_ELLIPSIS = 1 << 4,
		JUSTIFICATION_SKIP_LAST_LINE = 1 << 5,
		JUSTIFICATION_SKIP_LAST_LINE_WITH_VISIBLE_CHARS = 1 << 6,
		JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE = 1 << 7,
	};

	enum VisibleCharactersBehavior {
		VC_CHARS_BEFORE_SHAPING,
		VC_CHARS_AFTER_SHAPING,
		VC_GLYPHS_AUTO,
		VC_GLYPHS_LTR,
		VC_GLYPHS_RTL,
	};

	enum AutowrapMode {
		AUTOWRAP_OFF,
		AUTOWRAP_ARBITRARY,
		AUTOWRAP_WORD,
		AUTOWRAP_WORD_SMART
	};

	enum LineBreakFlag {
		BREAK_NONE = 0,
		BREAK_MANDATORY = 1 << 0,
		BREAK_WORD_BOUND = 1 << 1,
		BREAK_GRAPHEME_BOUND = 1 << 2,
		BREAK_ADAPTIVE = 1 << 3,
		BREAK_TRIM_EDGE_SPACES = 1 << 4,
		BREAK_TRIM_INDENT = 1 << 5,
	};

	enum OverrunBehavior {
		OVERRUN_NO_TRIMMING,
		OVERRUN_TRIM_CHAR,
		OVERRUN_TRIM_WORD,
		OVERRUN_TRIM_ELLIPSIS,
		OVERRUN_TRIM_WORD_ELLIPSIS,
	};

	enum TextOverrunFlag {
		OVERRUN_NO_TRIM = 0,
		OVERRUN_TRIM = 1 << 0,
		OVERRUN_TRIM_WORD_ONLY = 1 << 1,
		OVERRUN_ADD_ELLIPSIS = 1 << 2,
		OVERRUN_ENFORCE_ELLIPSIS = 1 << 3,
		OVERRUN_JUSTIFICATION_AWARE = 1 << 4,
	};

	enum GraphemeFlag {
		GRAPHEME_IS_VALID = 1 << 0, // Grapheme is valid.
		GRAPHEME_IS_RTL = 1 << 1, // Grapheme is right-to-left.
		GRAPHEME_IS_VIRTUAL = 1 << 2, // Grapheme is not part of source string (added by fit_to_width function, do not affect caret movement).
		GRAPHEME_IS_SPACE = 1 << 3, // Is whitespace (for justification and word breaks).
		GRAPHEME_IS_BREAK_HARD = 1 << 4, // Is line break (mandatory break, e.g. "\n").
		GRAPHEME_IS_BREAK_SOFT = 1 << 5, // Is line break (optional break, e.g. space).
		GRAPHEME_IS_TAB = 1 << 6, // Is tab or vertical tab.
		GRAPHEME_IS_ELONGATION = 1 << 7, // Elongation (e.g. kashida), grapheme can be duplicated or truncated to fit line to width.
		GRAPHEME_IS_PUNCTUATION = 1 << 8, // Punctuation, except underscore (can be used as word break, but not line break or justifiction).
		GRAPHEME_IS_UNDERSCORE = 1 << 9, // Underscore (can be used as word break).
		GRAPHEME_IS_CONNECTED = 1 << 10, // Connected to previous grapheme.
		GRAPHEME_IS_SAFE_TO_INSERT_TATWEEL = 1 << 11, // It is safe to insert a U+0640 before this grapheme for elongation.
		GRAPHEME_IS_EMBEDDED_OBJECT = 1 << 12, // Grapheme is an object replacement character for the embedded object.
		GRAPHEME_IS_SOFT_HYPHEN = 1 << 13, // Grapheme is a soft hyphen.
	};

	enum Hinting {
		HINTING_NONE,
		HINTING_LIGHT,
		HINTING_NORMAL
	};

	enum SubpixelPositioning {
		SUBPIXEL_POSITIONING_DISABLED = 0,
		SUBPIXEL_POSITIONING_AUTO = 1,
		SUBPIXEL_POSITIONING_ONE_HALF = 2,
		SUBPIXEL_POSITIONING_ONE_QUARTER = 3,

		SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE = 20,
		SUBPIXEL_POSITIONING_ONE_QUARTER_MAX_SIZE = 16,
	};

	enum Feature {
		FEATURE_SIMPLE_LAYOUT = 1 << 0,
		FEATURE_BIDI_LAYOUT = 1 << 1,
		FEATURE_VERTICAL_LAYOUT = 1 << 2,
		FEATURE_SHAPING = 1 << 3,
		FEATURE_KASHIDA_JUSTIFICATION = 1 << 4,
		FEATURE_BREAK_ITERATORS = 1 << 5,
		FEATURE_FONT_BITMAP = 1 << 6,
		FEATURE_FONT_DYNAMIC = 1 << 7,
		FEATURE_FONT_MSDF = 1 << 8,
		FEATURE_FONT_SYSTEM = 1 << 9,
		FEATURE_FONT_VARIABLE = 1 << 10,
		FEATURE_CONTEXT_SENSITIVE_CASE_CONVERSION = 1 << 11,
		FEATURE_USE_SUPPORT_DATA = 1 << 12,
		FEATURE_UNICODE_IDENTIFIERS = 1 << 13,
		FEATURE_UNICODE_SECURITY = 1 << 14,
	};

	enum ContourPointTag {
		CONTOUR_CURVE_TAG_ON = 0x01,
		CONTOUR_CURVE_TAG_OFF_CONIC = 0x00,
		CONTOUR_CURVE_TAG_OFF_CUBIC = 0x02
	};

	enum SpacingType {
		SPACING_GLYPH,
		SPACING_SPACE,
		SPACING_TOP,
		SPACING_BOTTOM,
		SPACING_MAX,
	};

	enum FontStyle {
		FONT_BOLD = 1 << 0,
		FONT_ITALIC = 1 << 1,
		FONT_FIXED_WIDTH = 1 << 2,
	};

	enum StructuredTextParser {
		STRUCTURED_TEXT_DEFAULT,
		STRUCTURED_TEXT_URI,
		STRUCTURED_TEXT_FILE,
		STRUCTURED_TEXT_EMAIL,
		STRUCTURED_TEXT_LIST,
		STRUCTURED_TEXT_GDSCRIPT,
		STRUCTURED_TEXT_CUSTOM
	};

	enum FixedSizeScaleMode {
		FIXED_SIZE_SCALE_DISABLE,
		FIXED_SIZE_SCALE_INTEGER_ONLY,
		FIXED_SIZE_SCALE_ENABLED,
	};

	void _draw_hex_code_box_number(const RID &p_canvas, int64_t p_size, const Vector2 &p_pos, uint8_t p_index, const Color &p_color) const;

protected:
	HashMap<char32_t, char32_t> diacritics_map;
	void _diacritics_map_add(const String &p_from, char32_t p_to);
	void _init_diacritics_map();

	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	PackedInt32Array _shaped_text_get_word_breaks_bind_compat_90732(const RID &p_shaped, BitField<TextServer::GraphemeFlag> p_grapheme_flags = GRAPHEME_IS_SPACE | GRAPHEME_IS_PUNCTUATION) const;
	static void _bind_compatibility_methods();
#endif

public:
	virtual bool has_feature(Feature p_feature) const = 0;
	virtual String get_name() const = 0;
	virtual int64_t get_features() const = 0;

	virtual void free_rid(const RID &p_rid) = 0;
	virtual bool has(const RID &p_rid) = 0;
	virtual bool load_support_data(const String &p_filename) = 0;

	virtual String get_support_data_filename() const = 0;
	virtual String get_support_data_info() const = 0;
	virtual bool save_support_data(const String &p_filename) const = 0;
	virtual PackedByteArray get_support_data() const = 0;

	virtual bool is_locale_right_to_left(const String &p_locale) const = 0;

	virtual int64_t name_to_tag(const String &p_name) const;
	virtual String tag_to_name(int64_t p_tag) const;

	/* Font interface */

	virtual RID create_font() = 0;
	virtual RID create_font_linked_variation(const RID &p_font_rid) = 0;

	virtual void font_set_data(const RID &p_font_rid, const PackedByteArray &p_data) = 0;
	virtual void font_set_data_ptr(const RID &p_font_rid, const uint8_t *p_data_ptr, int64_t p_data_size) = 0;

	virtual void font_set_face_index(const RID &p_font_rid, int64_t p_index) = 0;
	virtual int64_t font_get_face_index(const RID &p_font_rid) const = 0;

	virtual int64_t font_get_face_count(const RID &p_font_rid) const = 0;

	virtual void font_set_style(const RID &p_font_rid, BitField<FontStyle> p_style) = 0;
	virtual BitField<FontStyle> font_get_style(const RID &p_font_rid) const = 0;

	virtual void font_set_name(const RID &p_font_rid, const String &p_name) = 0;
	virtual String font_get_name(const RID &p_font_rid) const = 0;
	virtual Dictionary font_get_ot_name_strings(const RID &p_font_rid) const { return Dictionary(); }

	virtual void font_set_style_name(const RID &p_font_rid, const String &p_name) = 0;
	virtual String font_get_style_name(const RID &p_font_rid) const = 0;

	virtual void font_set_weight(const RID &p_font_rid, int64_t p_weight) = 0;
	virtual int64_t font_get_weight(const RID &p_font_rid) const = 0;

	virtual void font_set_stretch(const RID &p_font_rid, int64_t p_stretch) = 0;
	virtual int64_t font_get_stretch(const RID &p_font_rid) const = 0;

	virtual void font_set_antialiasing(const RID &p_font_rid, FontAntialiasing p_antialiasing) = 0;
	virtual FontAntialiasing font_get_antialiasing(const RID &p_font_rid) const = 0;

	virtual void font_set_disable_embedded_bitmaps(const RID &p_font_rid, bool p_disable_embedded_bitmaps) = 0;
	virtual bool font_get_disable_embedded_bitmaps(const RID &p_font_rid) const = 0;

	virtual void font_set_generate_mipmaps(const RID &p_font_rid, bool p_generate_mipmaps) = 0;
	virtual bool font_get_generate_mipmaps(const RID &p_font_rid) const = 0;

	virtual void font_set_multichannel_signed_distance_field(const RID &p_font_rid, bool p_msdf) = 0;
	virtual bool font_is_multichannel_signed_distance_field(const RID &p_font_rid) const = 0;

	virtual void font_set_msdf_pixel_range(const RID &p_font_rid, int64_t p_msdf_pixel_range) = 0;
	virtual int64_t font_get_msdf_pixel_range(const RID &p_font_rid) const = 0;

	virtual void font_set_msdf_size(const RID &p_font_rid, int64_t p_msdf_size) = 0;
	virtual int64_t font_get_msdf_size(const RID &p_font_rid) const = 0;

	virtual void font_set_fixed_size(const RID &p_font_rid, int64_t p_fixed_size) = 0;
	virtual int64_t font_get_fixed_size(const RID &p_font_rid) const = 0;

	virtual void font_set_fixed_size_scale_mode(const RID &p_font_rid, FixedSizeScaleMode p_fixed_size_scale) = 0;
	virtual FixedSizeScaleMode font_get_fixed_size_scale_mode(const RID &p_font_rid) const = 0;

	virtual void font_set_allow_system_fallback(const RID &p_font_rid, bool p_allow_system_fallback) = 0;
	virtual bool font_is_allow_system_fallback(const RID &p_font_rid) const = 0;

	virtual void font_set_force_autohinter(const RID &p_font_rid, bool p_force_autohinter) = 0;
	virtual bool font_is_force_autohinter(const RID &p_font_rid) const = 0;

	virtual void font_set_hinting(const RID &p_font_rid, Hinting p_hinting) = 0;
	virtual Hinting font_get_hinting(const RID &p_font_rid) const = 0;

	virtual void font_set_subpixel_positioning(const RID &p_font_rid, SubpixelPositioning p_subpixel) = 0;
	virtual SubpixelPositioning font_get_subpixel_positioning(const RID &p_font_rid) const = 0;

	virtual void font_set_keep_rounding_remainders(const RID &p_font_rid, bool p_keep_rounding_remainders) = 0;
	virtual bool font_get_keep_rounding_remainders(const RID &p_font_rid) const = 0;

	virtual void font_set_embolden(const RID &p_font_rid, double p_strength) = 0;
	virtual double font_get_embolden(const RID &p_font_rid) const = 0;

	virtual void font_set_spacing(const RID &p_font_rid, SpacingType p_spacing, int64_t p_value) = 0;
	virtual int64_t font_get_spacing(const RID &p_font_rid, SpacingType p_spacing) const = 0;

	virtual void font_set_baseline_offset(const RID &p_font_rid, double p_baseline_offset) = 0;
	virtual double font_get_baseline_offset(const RID &p_font_rid) const = 0;

	virtual void font_set_transform(const RID &p_font_rid, const Transform2D &p_transform) = 0;
	virtual Transform2D font_get_transform(const RID &p_font_rid) const = 0;

	virtual void font_set_variation_coordinates(const RID &p_font_rid, const Dictionary &p_variation_coordinates) = 0;
	virtual Dictionary font_get_variation_coordinates(const RID &p_font_rid) const = 0;

	virtual void font_set_oversampling(const RID &p_font_rid, double p_oversampling) = 0;
	virtual double font_get_oversampling(const RID &p_font_rid) const = 0;

	virtual TypedArray<Vector2i> font_get_size_cache_list(const RID &p_font_rid) const = 0;
	virtual void font_clear_size_cache(const RID &p_font_rid) = 0;
	virtual void font_remove_size_cache(const RID &p_font_rid, const Vector2i &p_size) = 0;

	virtual void font_set_ascent(const RID &p_font_rid, int64_t p_size, double p_ascent) = 0;
	virtual double font_get_ascent(const RID &p_font_rid, int64_t p_size) const = 0;

	virtual void font_set_descent(const RID &p_font_rid, int64_t p_size, double p_descent) = 0;
	virtual double font_get_descent(const RID &p_font_rid, int64_t p_size) const = 0;

	virtual void font_set_underline_position(const RID &p_font_rid, int64_t p_size, double p_underline_position) = 0;
	virtual double font_get_underline_position(const RID &p_font_rid, int64_t p_size) const = 0;

	virtual void font_set_underline_thickness(const RID &p_font_rid, int64_t p_size, double p_underline_thickness) = 0;
	virtual double font_get_underline_thickness(const RID &p_font_rid, int64_t p_size) const = 0;

	virtual void font_set_scale(const RID &p_font_rid, int64_t p_size, double p_scale) = 0;
	virtual double font_get_scale(const RID &p_font_rid, int64_t p_size) const = 0;

	virtual int64_t font_get_texture_count(const RID &p_font_rid, const Vector2i &p_size) const = 0;
	virtual void font_clear_textures(const RID &p_font_rid, const Vector2i &p_size) = 0;
	virtual void font_remove_texture(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) = 0;

	virtual void font_set_texture_image(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index, const Ref<Image> &p_image) = 0;
	virtual Ref<Image> font_get_texture_image(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) const = 0;

	virtual void font_set_texture_offsets(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index, const PackedInt32Array &p_offset) = 0;
	virtual PackedInt32Array font_get_texture_offsets(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) const = 0;

	virtual PackedInt32Array font_get_glyph_list(const RID &p_font_rid, const Vector2i &p_size) const = 0;
	virtual void font_clear_glyphs(const RID &p_font_rid, const Vector2i &p_size) = 0;
	virtual void font_remove_glyph(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) = 0;

	virtual Vector2 font_get_glyph_advance(const RID &p_font_rid, int64_t p_size, int64_t p_glyph) const = 0;
	virtual void font_set_glyph_advance(const RID &p_font_rid, int64_t p_size, int64_t p_glyph, const Vector2 &p_advance) = 0;

	virtual Vector2 font_get_glyph_offset(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const = 0;
	virtual void font_set_glyph_offset(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Vector2 &p_offset) = 0;

	virtual Vector2 font_get_glyph_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const = 0;
	virtual void font_set_glyph_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Vector2 &p_gl_size) = 0;

	virtual Rect2 font_get_glyph_uv_rect(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const = 0;
	virtual void font_set_glyph_uv_rect(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Rect2 &p_uv_rect) = 0;

	virtual int64_t font_get_glyph_texture_idx(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const = 0;
	virtual void font_set_glyph_texture_idx(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, int64_t p_texture_idx) = 0;
	virtual RID font_get_glyph_texture_rid(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const = 0;
	virtual Size2 font_get_glyph_texture_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const = 0;

	virtual Dictionary font_get_glyph_contours(const RID &p_font, int64_t p_size, int64_t p_index) const = 0;

	virtual TypedArray<Vector2i> font_get_kerning_list(const RID &p_font_rid, int64_t p_size) const = 0;
	virtual void font_clear_kerning_map(const RID &p_font_rid, int64_t p_size) = 0;
	virtual void font_remove_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair) = 0;

	virtual void font_set_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair, const Vector2 &p_kerning) = 0;
	virtual Vector2 font_get_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair) const = 0;

	virtual int64_t font_get_glyph_index(const RID &p_font_rid, int64_t p_size, int64_t p_char, int64_t p_variation_selector) const = 0;
	virtual int64_t font_get_char_from_glyph_index(const RID &p_font_rid, int64_t p_size, int64_t p_glyph_index) const = 0;

	virtual bool font_has_char(const RID &p_font_rid, int64_t p_char) const = 0;
	virtual String font_get_supported_chars(const RID &p_font_rid) const = 0;
	virtual PackedInt32Array font_get_supported_glyphs(const RID &p_font_rid) const = 0;

	virtual void font_render_range(const RID &p_font, const Vector2i &p_size, int64_t p_start, int64_t p_end) = 0;
	virtual void font_render_glyph(const RID &p_font_rid, const Vector2i &p_size, int64_t p_index) = 0;

	virtual void font_draw_glyph(const RID &p_font, const RID &p_canvas, int64_t p_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color = Color(1, 1, 1)) const = 0;
	virtual void font_draw_glyph_outline(const RID &p_font, const RID &p_canvas, int64_t p_size, int64_t p_outline_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color = Color(1, 1, 1)) const = 0;

	virtual bool font_is_language_supported(const RID &p_font_rid, const String &p_language) const = 0;
	virtual void font_set_language_support_override(const RID &p_font_rid, const String &p_language, bool p_supported) = 0;
	virtual bool font_get_language_support_override(const RID &p_font_rid, const String &p_language) = 0;
	virtual void font_remove_language_support_override(const RID &p_font_rid, const String &p_language) = 0;
	virtual PackedStringArray font_get_language_support_overrides(const RID &p_font_rid) = 0;

	virtual bool font_is_script_supported(const RID &p_font_rid, const String &p_script) const = 0;
	virtual void font_set_script_support_override(const RID &p_font_rid, const String &p_script, bool p_supported) = 0;
	virtual bool font_get_script_support_override(const RID &p_font_rid, const String &p_script) = 0;
	virtual void font_remove_script_support_override(const RID &p_font_rid, const String &p_script) = 0;
	virtual PackedStringArray font_get_script_support_overrides(const RID &p_font_rid) = 0;

	virtual void font_set_opentype_feature_overrides(const RID &p_font_rid, const Dictionary &p_overrides) = 0;
	virtual Dictionary font_get_opentype_feature_overrides(const RID &p_font_rid) const = 0;

	virtual Dictionary font_supported_feature_list(const RID &p_font_rid) const = 0;
	virtual Dictionary font_supported_variation_list(const RID &p_font_rid) const = 0;

	virtual double font_get_global_oversampling() const = 0;
	virtual void font_set_global_oversampling(double p_oversampling) = 0;

	virtual Vector2 get_hex_code_box_size(int64_t p_size, int64_t p_index) const;
	virtual void draw_hex_code_box(const RID &p_canvas, int64_t p_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color) const;

	/* Shaped text buffer interface */

	virtual RID create_shaped_text(Direction p_direction = DIRECTION_AUTO, Orientation p_orientation = ORIENTATION_HORIZONTAL) = 0;

	virtual void shaped_text_clear(const RID &p_shaped) = 0;

	virtual void shaped_text_set_direction(const RID &p_shaped, Direction p_direction = DIRECTION_AUTO) = 0;
	virtual Direction shaped_text_get_direction(const RID &p_shaped) const = 0;
	virtual Direction shaped_text_get_inferred_direction(const RID &p_shaped) const = 0;

	virtual void shaped_text_set_bidi_override(const RID &p_shaped, const Array &p_override) = 0;

	virtual void shaped_text_set_custom_punctuation(const RID &p_shaped, const String &p_punct) = 0;
	virtual String shaped_text_get_custom_punctuation(const RID &p_shaped) const = 0;

	virtual void shaped_text_set_custom_ellipsis(const RID &p_shaped, int64_t p_char) = 0;
	virtual int64_t shaped_text_get_custom_ellipsis(const RID &p_shaped) const = 0;

	virtual void shaped_text_set_orientation(const RID &p_shaped, Orientation p_orientation = ORIENTATION_HORIZONTAL) = 0;
	virtual Orientation shaped_text_get_orientation(const RID &p_shaped) const = 0;

	virtual void shaped_text_set_preserve_invalid(const RID &p_shaped, bool p_enabled) = 0;
	virtual bool shaped_text_get_preserve_invalid(const RID &p_shaped) const = 0;

	virtual void shaped_text_set_preserve_control(const RID &p_shaped, bool p_enabled) = 0;
	virtual bool shaped_text_get_preserve_control(const RID &p_shaped) const = 0;

	virtual void shaped_text_set_spacing(const RID &p_shaped, SpacingType p_spacing, int64_t p_value) = 0;
	virtual int64_t shaped_text_get_spacing(const RID &p_shaped, SpacingType p_spacing) const = 0;

	virtual bool shaped_text_add_string(const RID &p_shaped, const String &p_text, const TypedArray<RID> &p_fonts, int64_t p_size, const Dictionary &p_opentype_features = Dictionary(), const String &p_language = "", const Variant &p_meta = Variant()) = 0;
	virtual bool shaped_text_add_object(const RID &p_shaped, const Variant &p_key, const Size2 &p_size, InlineAlignment p_inline_align = INLINE_ALIGNMENT_CENTER, int64_t p_length = 1, double p_baseline = 0.0) = 0;
	virtual bool shaped_text_resize_object(const RID &p_shaped, const Variant &p_key, const Size2 &p_size, InlineAlignment p_inline_align = INLINE_ALIGNMENT_CENTER, double p_baseline = 0.0) = 0;

	virtual int64_t shaped_get_span_count(const RID &p_shaped) const = 0;
	virtual Variant shaped_get_span_meta(const RID &p_shaped, int64_t p_index) const = 0;
	virtual void shaped_set_span_update_font(const RID &p_shaped, int64_t p_index, const TypedArray<RID> &p_fonts, int64_t p_size, const Dictionary &p_opentype_features = Dictionary()) = 0;

	virtual RID shaped_text_substr(const RID &p_shaped, int64_t p_start, int64_t p_length) const = 0; // Copy shaped substring (e.g. line break) without reshaping, but correctly reordered, preservers range.
	virtual RID shaped_text_get_parent(const RID &p_shaped) const = 0;

	virtual double shaped_text_fit_to_width(const RID &p_shaped, double p_width, BitField<TextServer::JustificationFlag> p_jst_flags = JUSTIFICATION_WORD_BOUND | JUSTIFICATION_KASHIDA) = 0;
	virtual double shaped_text_tab_align(const RID &p_shaped, const PackedFloat32Array &p_tab_stops) = 0;

	virtual bool shaped_text_shape(const RID &p_shaped) = 0;
	virtual bool shaped_text_update_breaks(const RID &p_shaped) = 0;
	virtual bool shaped_text_update_justification_ops(const RID &p_shaped) = 0;

	virtual bool shaped_text_is_ready(const RID &p_shaped) const = 0;
	bool shaped_text_has_visible_chars(const RID &p_shaped) const;

	virtual const Glyph *shaped_text_get_glyphs(const RID &p_shaped) const = 0;
	TypedArray<Dictionary> _shaped_text_get_glyphs_wrapper(const RID &p_shaped) const;
	virtual const Glyph *shaped_text_sort_logical(const RID &p_shaped) = 0;
	TypedArray<Dictionary> _shaped_text_sort_logical_wrapper(const RID &p_shaped);
	virtual int64_t shaped_text_get_glyph_count(const RID &p_shaped) const = 0;

	virtual Vector2i shaped_text_get_range(const RID &p_shaped) const = 0;

	virtual PackedInt32Array shaped_text_get_line_breaks_adv(const RID &p_shaped, const PackedFloat32Array &p_width, int64_t p_start = 0, bool p_once = true, BitField<TextServer::LineBreakFlag> p_break_flags = BREAK_MANDATORY | BREAK_WORD_BOUND) const;
	virtual PackedInt32Array shaped_text_get_line_breaks(const RID &p_shaped, double p_width, int64_t p_start = 0, BitField<TextServer::LineBreakFlag> p_break_flags = BREAK_MANDATORY | BREAK_WORD_BOUND) const;
	virtual PackedInt32Array shaped_text_get_word_breaks(const RID &p_shaped, BitField<TextServer::GraphemeFlag> p_grapheme_flags = GRAPHEME_IS_SPACE | GRAPHEME_IS_PUNCTUATION, BitField<TextServer::GraphemeFlag> p_skip_grapheme_flags = GRAPHEME_IS_VIRTUAL) const;

	virtual int64_t shaped_text_get_trim_pos(const RID &p_shaped) const = 0;
	virtual int64_t shaped_text_get_ellipsis_pos(const RID &p_shaped) const = 0;
	virtual const Glyph *shaped_text_get_ellipsis_glyphs(const RID &p_shaped) const = 0;
	TypedArray<Dictionary> _shaped_text_get_ellipsis_glyphs_wrapper(const RID &p_shaped) const;
	virtual int64_t shaped_text_get_ellipsis_glyph_count(const RID &p_shaped) const = 0;

	virtual void shaped_text_overrun_trim_to_width(const RID &p_shaped, double p_width, BitField<TextServer::TextOverrunFlag> p_trim_flags) = 0;

	virtual Array shaped_text_get_objects(const RID &p_shaped) const = 0;
	virtual Rect2 shaped_text_get_object_rect(const RID &p_shaped, const Variant &p_key) const = 0;
	virtual Vector2i shaped_text_get_object_range(const RID &p_shaped, const Variant &p_key) const = 0;
	virtual int64_t shaped_text_get_object_glyph(const RID &p_shaped, const Variant &p_key) const = 0;

	virtual Size2 shaped_text_get_size(const RID &p_shaped) const = 0;
	virtual double shaped_text_get_ascent(const RID &p_shaped) const = 0;
	virtual double shaped_text_get_descent(const RID &p_shaped) const = 0;
	virtual double shaped_text_get_width(const RID &p_shaped) const = 0;
	virtual double shaped_text_get_underline_position(const RID &p_shaped) const = 0;
	virtual double shaped_text_get_underline_thickness(const RID &p_shaped) const = 0;

	virtual Direction shaped_text_get_dominant_direction_in_range(const RID &p_shaped, int64_t p_start, int64_t p_end) const;

	virtual CaretInfo shaped_text_get_carets(const RID &p_shaped, int64_t p_position) const;
	Dictionary _shaped_text_get_carets_wrapper(const RID &p_shaped, int64_t p_position) const;

	virtual Vector<Vector2> shaped_text_get_selection(const RID &p_shaped, int64_t p_start, int64_t p_end) const;

	virtual int64_t shaped_text_hit_test_grapheme(const RID &p_shaped, double p_coords) const; // Return grapheme index.
	virtual int64_t shaped_text_hit_test_position(const RID &p_shaped, double p_coords) const; // Return caret/selection position.

	virtual Vector2 shaped_text_get_grapheme_bounds(const RID &p_shaped, int64_t p_pos) const;
	virtual int64_t shaped_text_next_grapheme_pos(const RID &p_shaped, int64_t p_pos) const;
	virtual int64_t shaped_text_prev_grapheme_pos(const RID &p_shaped, int64_t p_pos) const;

	virtual PackedInt32Array shaped_text_get_character_breaks(const RID &p_shaped) const = 0;
	virtual int64_t shaped_text_next_character_pos(const RID &p_shaped, int64_t p_pos) const;
	virtual int64_t shaped_text_prev_character_pos(const RID &p_shaped, int64_t p_pos) const;
	virtual int64_t shaped_text_closest_character_pos(const RID &p_shaped, int64_t p_pos) const;

	// The pen position is always placed on the baseline and moveing left to right.
	virtual void shaped_text_draw(const RID &p_shaped, const RID &p_canvas, const Vector2 &p_pos, double p_clip_l = -1.0, double p_clip_r = -1.0, const Color &p_color = Color(1, 1, 1)) const;
	virtual void shaped_text_draw_outline(const RID &p_shaped, const RID &p_canvas, const Vector2 &p_pos, double p_clip_l = -1.0, double p_clip_r = -1.0, int64_t p_outline_size = 1, const Color &p_color = Color(1, 1, 1)) const;

#ifdef DEBUG_ENABLED
	void debug_print_glyph(int p_idx, const Glyph &p_glyph) const;
	void shaped_text_debug_print(const RID &p_shaped) const;
#endif

	// Number conversion.
	virtual String format_number(const String &p_string, const String &p_language = "") const = 0;
	virtual String parse_number(const String &p_string, const String &p_language = "") const = 0;
	virtual String percent_sign(const String &p_language = "") const = 0;

	// String functions.
	virtual PackedInt32Array string_get_word_breaks(const String &p_string, const String &p_language = "", int64_t p_chars_per_line = 0) const = 0;
	virtual PackedInt32Array string_get_character_breaks(const String &p_string, const String &p_language = "") const;

	virtual int64_t is_confusable(const String &p_string, const PackedStringArray &p_dict) const { return -1; }
	virtual bool spoof_check(const String &p_string) const { return false; }

	virtual String strip_diacritics(const String &p_string) const;
	virtual bool is_valid_identifier(const String &p_string) const;
	virtual bool is_valid_letter(uint64_t p_unicode) const;

	// Other string operations.
	virtual String string_to_upper(const String &p_string, const String &p_language = "") const = 0;
	virtual String string_to_lower(const String &p_string, const String &p_language = "") const = 0;
	virtual String string_to_title(const String &p_string, const String &p_language = "") const = 0;

	TypedArray<Vector3i> parse_structured_text(StructuredTextParser p_parser_type, const Array &p_args, const String &p_text) const;

	virtual void cleanup() {}

	TextServer();
	~TextServer();
};

/*************************************************************************/

struct Glyph {
	int start = -1; // Start offset in the source string.
	int end = -1; // End offset in the source string.

	uint8_t count = 0; // Number of glyphs in the grapheme, set in the first glyph only.
	uint8_t repeat = 1; // Draw multiple times in the row.
	uint16_t flags = 0; // Grapheme flags (valid, rtl, virtual), set in the first glyph only.

	float x_off = 0.f; // Offset from the origin of the glyph on baseline.
	float y_off = 0.f;
	float advance = 0.f; // Advance to the next glyph along baseline(x for horizontal layout, y for vertical).

	RID font_rid; // Font resource.
	int font_size = 0; // Font size;
	int32_t index = 0; // Glyph index (font specific) or UTF-32 codepoint (for the invalid glyphs).

	bool operator==(const Glyph &p_a) const;
	bool operator!=(const Glyph &p_a) const;

	bool operator<(const Glyph &p_a) const;
	bool operator>(const Glyph &p_a) const;
};

struct CaretInfo {
	Rect2 l_caret;
	Rect2 t_caret;
	TextServer::Direction l_dir;
	TextServer::Direction t_dir;
};

/*************************************************************************/

class TextServerManager : public Object {
	GDCLASS(TextServerManager, Object);

protected:
	static void _bind_methods();

private:
	static TextServerManager *singleton;

	Ref<TextServer> primary_interface;
	Vector<Ref<TextServer>> interfaces;

public:
	_FORCE_INLINE_ static TextServerManager *get_singleton() {
		return singleton;
	}

	void add_interface(const Ref<TextServer> &p_interface);
	void remove_interface(const Ref<TextServer> &p_interface);
	int get_interface_count() const;
	Ref<TextServer> get_interface(int p_index) const;
	Ref<TextServer> find_interface(const String &p_name) const;
	TypedArray<Dictionary> get_interfaces() const;

	_FORCE_INLINE_ Ref<TextServer> get_primary_interface() const {
		return primary_interface;
	}
	void set_primary_interface(const Ref<TextServer> &p_primary_interface);

	TextServerManager();
	~TextServerManager();
};

/*************************************************************************/

#define TS TextServerManager::get_singleton()->get_primary_interface()

VARIANT_ENUM_CAST(TextServer::VisibleCharactersBehavior);
VARIANT_ENUM_CAST(TextServer::AutowrapMode);
VARIANT_ENUM_CAST(TextServer::OverrunBehavior);
VARIANT_ENUM_CAST(TextServer::Direction);
VARIANT_ENUM_CAST(TextServer::Orientation);
VARIANT_BITFIELD_CAST(TextServer::JustificationFlag);
VARIANT_BITFIELD_CAST(TextServer::LineBreakFlag);
VARIANT_BITFIELD_CAST(TextServer::TextOverrunFlag);
VARIANT_BITFIELD_CAST(TextServer::GraphemeFlag);
VARIANT_ENUM_CAST(TextServer::Hinting);
VARIANT_ENUM_CAST(TextServer::SubpixelPositioning);
VARIANT_ENUM_CAST(TextServer::Feature);
VARIANT_ENUM_CAST(TextServer::ContourPointTag);
VARIANT_ENUM_CAST(TextServer::SpacingType);
VARIANT_BITFIELD_CAST(TextServer::FontStyle);
VARIANT_ENUM_CAST(TextServer::StructuredTextParser);
VARIANT_ENUM_CAST(TextServer::FontAntialiasing);
VARIANT_ENUM_CAST(TextServer::FontLCDSubpixelLayout);
VARIANT_ENUM_CAST(TextServer::FixedSizeScaleMode);

GDVIRTUAL_NATIVE_PTR(Glyph);
GDVIRTUAL_NATIVE_PTR(CaretInfo);

#endif // TEXT_SERVER_H
