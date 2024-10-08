/**************************************************************************/
/*  text_server_adv.h                                                     */
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

#ifndef TEXT_SERVER_ADV_H
#define TEXT_SERVER_ADV_H

/*************************************************************************/
/* ICU/HarfBuzz/Graphite backed Text Server implementation with BiDi,    */
/* shaping and advanced font features support.                           */
/*************************************************************************/

#include "script_iterator.h"

#ifdef GDEXTENSION
// Headers for building as GDExtension plug-in.

#include <godot_cpp/godot.hpp>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/ext_wrappers.gen.inc>
#include <godot_cpp/core/mutex_lock.hpp>

#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>

#include <godot_cpp/classes/text_server.hpp>
#include <godot_cpp/classes/text_server_extension.hpp>
#include <godot_cpp/classes/text_server_manager.hpp>

#include <godot_cpp/classes/caret_info.hpp>
#include <godot_cpp/classes/global_constants_binds.hpp>
#include <godot_cpp/classes/glyph.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/worker_thread_pool.hpp>

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/templates/rid_owner.hpp>
#include <godot_cpp/templates/safe_refcount.hpp>
#include <godot_cpp/templates/vector.hpp>

using namespace godot;

#elif defined(GODOT_MODULE)
// Headers for building as built-in module.

#include "core/extension/ext_wrappers.gen.inc"
#include "core/object/worker_thread_pool.h"
#include "core/templates/hash_map.h"
#include "core/templates/rid_owner.h"
#include "core/templates/safe_refcount.h"
#include "scene/resources/image_texture.h"
#include "servers/text/text_server_extension.h"

#include "modules/modules_enabled.gen.h" // For freetype, msdfgen, svg.

#endif

// Thirdparty headers.

#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/uchar.h>
#include <unicode/uclean.h>
#include <unicode/udata.h>
#include <unicode/uiter.h>
#include <unicode/uloc.h>
#include <unicode/unorm2.h>
#include <unicode/uscript.h>
#include <unicode/uspoof.h>
#include <unicode/ustring.h>
#include <unicode/utypes.h>

#ifdef MODULE_FREETYPE_ENABLED
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_STROKER_H
#include FT_ADVANCES_H
#include FT_MULTIPLE_MASTERS_H
#include FT_BBOX_H
#include FT_MODULE_H
#include FT_CONFIG_OPTIONS_H
#if !defined(FT_CONFIG_OPTION_USE_BROTLI) && !defined(_MSC_VER)
#warning FreeType is configured without Brotli support, built-in fonts will not be available.
#endif
#include <hb-ft.h>
#include <hb-ot.h>
#endif

#include <hb-icu.h>
#include <hb.h>

/*************************************************************************/

class TextServerAdvanced : public TextServerExtension {
	GDCLASS(TextServerAdvanced, TextServerExtension);
	_THREAD_SAFE_CLASS_

	struct NumSystemData {
		HashSet<StringName> lang;
		String digits;
		String percent_sign;
		String exp;
	};

	Vector<NumSystemData> num_systems;

	struct FeatureInfo {
		StringName name;
		Variant::Type vtype = Variant::INT;
		bool hidden = false;
	};

	HashMap<StringName, int32_t> feature_sets;
	HashMap<int32_t, FeatureInfo> feature_sets_inv;

	SafeNumeric<TextServer::FontLCDSubpixelLayout> lcd_subpixel_layout{ TextServer::FontLCDSubpixelLayout::FONT_LCD_SUBPIXEL_LAYOUT_NONE };
	void _update_settings();

	void _insert_num_systems_lang();
	void _insert_feature_sets();
	_FORCE_INLINE_ void _insert_feature(const StringName &p_name, int32_t p_tag, Variant::Type p_vtype = Variant::INT, bool p_hidden = false);

	// ICU support data.

	static bool icu_data_loaded;
	static PackedByteArray icu_data;
	mutable USet *allowed = nullptr;
	mutable USpoofChecker *sc_spoof = nullptr;
	mutable USpoofChecker *sc_conf = nullptr;

	// Font cache data.

#ifdef MODULE_FREETYPE_ENABLED
	mutable FT_Library ft_library = nullptr;
#endif

	const int rect_range = 1;

	struct FontTexturePosition {
		int32_t index = -1;
		int32_t x = 0;
		int32_t y = 0;

		FontTexturePosition() {}
		FontTexturePosition(int32_t p_id, int32_t p_x, int32_t p_y) :
				index(p_id), x(p_x), y(p_y) {}
	};

	struct Shelf {
		int32_t x = 0;
		int32_t y = 0;
		int32_t w = 0;
		int32_t h = 0;

		FontTexturePosition alloc_shelf(int32_t p_id, int32_t p_w, int32_t p_h) {
			if (p_w > w || p_h > h) {
				return FontTexturePosition(-1, 0, 0);
			}
			int32_t xx = x;
			x += p_w;
			w -= p_w;
			return FontTexturePosition(p_id, xx, y);
		}

		Shelf() {}
		Shelf(int32_t p_x, int32_t p_y, int32_t p_w, int32_t p_h) :
				x(p_x), y(p_y), w(p_w), h(p_h) {}
	};

	struct ShelfPackTexture {
		int32_t texture_w = 1024;
		int32_t texture_h = 1024;

		Ref<Image> image;
		Ref<ImageTexture> texture;
		bool dirty = true;

		List<Shelf> shelves;

		FontTexturePosition pack_rect(int32_t p_id, int32_t p_h, int32_t p_w) {
			int32_t y = 0;
			int32_t waste = 0;
			Shelf *best_shelf = nullptr;
			int32_t best_waste = std::numeric_limits<std::int32_t>::max();

			for (Shelf &E : shelves) {
				y += E.h;
				if (p_w > E.w) {
					continue;
				}
				if (p_h == E.h) {
					return E.alloc_shelf(p_id, p_w, p_h);
				}
				if (p_h > E.h) {
					continue;
				}
				if (p_h < E.h) {
					waste = (E.h - p_h) * p_w;
					if (waste < best_waste) {
						best_waste = waste;
						best_shelf = &E;
					}
				}
			}
			if (best_shelf) {
				return best_shelf->alloc_shelf(p_id, p_w, p_h);
			}
			if (p_h <= (texture_h - y) && p_w <= texture_w) {
				List<Shelf>::Element *E = shelves.push_back(Shelf(0, y, texture_w, p_h));
				return E->get().alloc_shelf(p_id, p_w, p_h);
			}
			return FontTexturePosition(-1, 0, 0);
		}

		ShelfPackTexture() {}
		ShelfPackTexture(int32_t p_w, int32_t p_h) :
				texture_w(p_w), texture_h(p_h) {}
	};

	struct FontGlyph {
		bool found = false;
		int texture_idx = -1;
		Rect2 rect;
		Rect2 uv_rect;
		Vector2 advance;
	};

	struct FontForSizeAdvanced {
		double ascent = 0.0;
		double descent = 0.0;
		double underline_position = 0.0;
		double underline_thickness = 0.0;
		double scale = 1.0;
		double oversampling = 1.0;

		Vector2i size;

		Vector<ShelfPackTexture> textures;
		HashMap<int64_t, int64_t> inv_glyph_map;
		HashMap<int32_t, FontGlyph> glyph_map;
		HashMap<Vector2i, Vector2> kerning_map;
		hb_font_t *hb_handle = nullptr;

#ifdef MODULE_FREETYPE_ENABLED
		FT_Face face = nullptr;
		FT_StreamRec stream;
#endif

		~FontForSizeAdvanced() {
			if (hb_handle != nullptr) {
				hb_font_destroy(hb_handle);
			}
#ifdef MODULE_FREETYPE_ENABLED
			if (face != nullptr) {
				FT_Done_Face(face);
			}
#endif
		}
	};

	struct FontAdvancedLinkedVariation {
		RID base_font;
		int extra_spacing[4] = { 0, 0, 0, 0 };
		double baseline_offset = 0.0;
	};

	struct FontAdvanced {
		Mutex mutex;

		TextServer::FontAntialiasing antialiasing = TextServer::FONT_ANTIALIASING_GRAY;
		bool disable_embedded_bitmaps = true;
		bool mipmaps = false;
		bool msdf = false;
		int msdf_range = 14;
		FixedSizeScaleMode fixed_size_scale_mode = FIXED_SIZE_SCALE_DISABLE;
		int msdf_source_size = 48;
		int fixed_size = 0;
		bool allow_system_fallback = true;
		bool force_autohinter = false;
		TextServer::Hinting hinting = TextServer::HINTING_LIGHT;
		TextServer::SubpixelPositioning subpixel_positioning = TextServer::SUBPIXEL_POSITIONING_AUTO;
		Dictionary variation_coordinates;
		double oversampling = 0.0;
		double embolden = 0.0;
		Transform2D transform;

		BitField<TextServer::FontStyle> style_flags = 0;
		String font_name;
		String style_name;
		int weight = 400;
		int stretch = 100;
		int extra_spacing[4] = { 0, 0, 0, 0 };
		double baseline_offset = 0.0;

		HashMap<Vector2i, FontForSizeAdvanced *> cache;

		bool face_init = false;
		HashSet<uint32_t> supported_scripts;
		Dictionary supported_features;
		Dictionary supported_varaitions;
		Dictionary feature_overrides;

		// Language/script support override.
		HashMap<String, bool> language_support_overrides;
		HashMap<String, bool> script_support_overrides;

		PackedByteArray data;
		const uint8_t *data_ptr;
		size_t data_size;
		int face_index = 0;

		~FontAdvanced() {
			for (const KeyValue<Vector2i, FontForSizeAdvanced *> &E : cache) {
				memdelete(E.value);
			}
			cache.clear();
		}
	};

	_FORCE_INLINE_ FontTexturePosition find_texture_pos_for_glyph(FontForSizeAdvanced *p_data, int p_color_size, Image::Format p_image_format, int p_width, int p_height, bool p_msdf) const;
#ifdef MODULE_MSDFGEN_ENABLED
	_FORCE_INLINE_ FontGlyph rasterize_msdf(FontAdvanced *p_font_data, FontForSizeAdvanced *p_data, int p_pixel_range, int p_rect_margin, FT_Outline *p_outline, const Vector2 &p_advance) const;
#endif
#ifdef MODULE_FREETYPE_ENABLED
	_FORCE_INLINE_ FontGlyph rasterize_bitmap(FontForSizeAdvanced *p_data, int p_rect_margin, FT_Bitmap p_bitmap, int p_yofs, int p_xofs, const Vector2 &p_advance, bool p_bgra) const;
#endif
	_FORCE_INLINE_ bool _ensure_glyph(FontAdvanced *p_font_data, const Vector2i &p_size, int32_t p_glyph, FontGlyph &r_glyph) const;
	_FORCE_INLINE_ bool _ensure_cache_for_size(FontAdvanced *p_font_data, const Vector2i &p_size, FontForSizeAdvanced *&r_cache_for_size, bool p_silent = false) const;
	_FORCE_INLINE_ bool _font_validate(const RID &p_font_rid) const;
	_FORCE_INLINE_ void _font_clear_cache(FontAdvanced *p_font_data);
	static void _generateMTSDF_threaded(void *p_td, uint32_t p_y);

	_FORCE_INLINE_ Vector2i _get_size(const FontAdvanced *p_font_data, int p_size) const {
		if (p_font_data->msdf) {
			return Vector2i(p_font_data->msdf_source_size, 0);
		} else if (p_font_data->fixed_size > 0) {
			return Vector2i(p_font_data->fixed_size, 0);
		} else {
			return Vector2i(p_size, 0);
		}
	}

	_FORCE_INLINE_ Vector2i _get_size_outline(const FontAdvanced *p_font_data, const Vector2i &p_size) const {
		if (p_font_data->msdf) {
			return Vector2i(p_font_data->msdf_source_size, 0);
		} else if (p_font_data->fixed_size > 0) {
			return Vector2i(p_font_data->fixed_size, MIN(p_size.y, 1));
		} else {
			return p_size;
		}
	}

	_FORCE_INLINE_ double _get_extra_advance(RID p_font_rid, int p_font_size) const;
	_FORCE_INLINE_ Variant::Type _get_tag_type(int64_t p_tag) const;
	_FORCE_INLINE_ bool _get_tag_hidden(int64_t p_tag) const;
	_FORCE_INLINE_ int _font_get_weight_by_name(const String &p_sty_name) const {
		String sty_name = p_sty_name.replace(" ", "").replace("-", "");
		if (sty_name.contains("thin") || sty_name.contains("hairline")) {
			return 100;
		} else if (sty_name.contains("extralight") || sty_name.contains("ultralight")) {
			return 200;
		} else if (sty_name.contains("light")) {
			return 300;
		} else if (sty_name.contains("semilight")) {
			return 350;
		} else if (sty_name.contains("regular")) {
			return 400;
		} else if (sty_name.contains("medium")) {
			return 500;
		} else if (sty_name.contains("semibold") || sty_name.contains("demibold")) {
			return 600;
		} else if (sty_name.contains("bold")) {
			return 700;
		} else if (sty_name.contains("extrabold") || sty_name.contains("ultrabold")) {
			return 800;
		} else if (sty_name.contains("black") || sty_name.contains("heavy")) {
			return 900;
		} else if (sty_name.contains("extrablack") || sty_name.contains("ultrablack")) {
			return 950;
		}
		return 400;
	}
	_FORCE_INLINE_ int _font_get_stretch_by_name(const String &p_sty_name) const {
		String sty_name = p_sty_name.replace(" ", "").replace("-", "");
		if (sty_name.contains("ultracondensed")) {
			return 50;
		} else if (sty_name.contains("extracondensed")) {
			return 63;
		} else if (sty_name.contains("condensed")) {
			return 75;
		} else if (sty_name.contains("semicondensed")) {
			return 87;
		} else if (sty_name.contains("semiexpanded")) {
			return 113;
		} else if (sty_name.contains("expanded")) {
			return 125;
		} else if (sty_name.contains("extraexpanded")) {
			return 150;
		} else if (sty_name.contains("ultraexpanded")) {
			return 200;
		}
		return 100;
	}
	_FORCE_INLINE_ bool _is_ital_style(const String &p_sty_name) const {
		return p_sty_name.contains("italic") || p_sty_name.contains("oblique");
	}

	// Shaped text cache data.
	struct TrimData {
		int trim_pos = -1;
		int ellipsis_pos = -1;
		Vector<Glyph> ellipsis_glyph_buf;
	};

	struct ShapedTextDataAdvanced {
		Mutex mutex;

		/* Source data */
		RID parent; // Substring parent ShapedTextData.

		int start = 0; // Substring start offset in the parent string.
		int end = 0; // Substring end offset in the parent string.

		String text;
		String custom_punct;
		TextServer::Direction direction = DIRECTION_LTR; // Desired text direction.
		TextServer::Orientation orientation = ORIENTATION_HORIZONTAL;

		struct Span {
			int start = -1;
			int end = -1;

			Array fonts;
			int font_size = 0;

			Variant embedded_key;

			String language;
			Dictionary features;
			Variant meta;
		};
		Vector<Span> spans;

		struct EmbeddedObject {
			int start = -1;
			int end = -1;
			InlineAlignment inline_align = INLINE_ALIGNMENT_CENTER;
			Rect2 rect;
			double baseline = 0;
		};
		HashMap<Variant, EmbeddedObject, VariantHasher, VariantComparator> objects;

		/* Shaped data */
		TextServer::Direction para_direction = DIRECTION_LTR; // Detected text direction.
		int base_para_direction = UBIDI_DEFAULT_LTR;
		SafeFlag valid{ false }; // String is shaped.
		bool line_breaks_valid = false; // Line and word break flags are populated (and virtual zero width spaces inserted).
		bool justification_ops_valid = false; // Virtual elongation glyphs are added to the string.
		bool sort_valid = false;
		bool text_trimmed = false;

		bool preserve_invalid = true; // Draw hex code box instead of missing characters.
		bool preserve_control = false; // Draw control characters.

		double ascent = 0.0; // Ascent for horizontal layout, 1/2 of width for vertical.
		double descent = 0.0; // Descent for horizontal layout, 1/2 of width for vertical.
		double width = 0.0; // Width for horizontal layout, height for vertical.
		double width_trimmed = 0.0;
		int extra_spacing[4] = { 0, 0, 0, 0 };

		double upos = 0.0;
		double uthk = 0.0;

		char32_t el_char = 0x2026;
		TrimData overrun_trim_data;
		bool fit_width_minimum_reached = false;

		Vector<Glyph> glyphs;
		Vector<Glyph> glyphs_logical;

		/* Intermediate data */
		Char16String utf16;
		Vector<UBiDi *> bidi_iter;
		Vector<Vector3i> bidi_override;
		ScriptIterator *script_iter = nullptr;
		hb_buffer_t *hb_buffer = nullptr;

		HashMap<int, bool> jstops;
		HashMap<int, bool> breaks;
		PackedInt32Array chars;
		int break_inserts = 0;
		bool break_ops_valid = false;
		bool js_ops_valid = false;
		bool chars_valid = false;

		~ShapedTextDataAdvanced() {
			for (int i = 0; i < bidi_iter.size(); i++) {
				if (bidi_iter[i]) {
					ubidi_close(bidi_iter[i]);
				}
			}
			if (script_iter) {
				memdelete(script_iter);
			}
			if (hb_buffer) {
				hb_buffer_destroy(hb_buffer);
			}
		}
	};

	// Common data.

	double oversampling = 1.0;
	mutable RID_PtrOwner<FontAdvancedLinkedVariation> font_var_owner;
	mutable RID_PtrOwner<FontAdvanced> font_owner;
	mutable RID_PtrOwner<ShapedTextDataAdvanced> shaped_owner;

	_FORCE_INLINE_ FontAdvanced *_get_font_data(const RID &p_font_rid) const {
		RID rid = p_font_rid;
		FontAdvancedLinkedVariation *fdv = font_var_owner.get_or_null(rid);
		if (unlikely(fdv)) {
			rid = fdv->base_font;
		}
		return font_owner.get_or_null(rid);
	}

	struct SystemFontKey {
		String font_name;
		TextServer::FontAntialiasing antialiasing = TextServer::FONT_ANTIALIASING_GRAY;
		bool disable_embedded_bitmaps = true;
		bool italic = false;
		bool mipmaps = false;
		bool msdf = false;
		bool force_autohinter = false;
		int weight = 400;
		int stretch = 100;
		int msdf_range = 14;
		int msdf_source_size = 48;
		int fixed_size = 0;
		TextServer::Hinting hinting = TextServer::HINTING_LIGHT;
		TextServer::SubpixelPositioning subpixel_positioning = TextServer::SUBPIXEL_POSITIONING_AUTO;
		Dictionary variation_coordinates;
		double oversampling = 0.0;
		double embolden = 0.0;
		Transform2D transform;
		int extra_spacing[4] = { 0, 0, 0, 0 };
		double baseline_offset = 0.0;

		bool operator==(const SystemFontKey &p_b) const {
			return (font_name == p_b.font_name) && (antialiasing == p_b.antialiasing) && (italic == p_b.italic) && (disable_embedded_bitmaps == p_b.disable_embedded_bitmaps) && (mipmaps == p_b.mipmaps) && (msdf == p_b.msdf) && (force_autohinter == p_b.force_autohinter) && (weight == p_b.weight) && (stretch == p_b.stretch) && (msdf_range == p_b.msdf_range) && (msdf_source_size == p_b.msdf_source_size) && (fixed_size == p_b.fixed_size) && (hinting == p_b.hinting) && (subpixel_positioning == p_b.subpixel_positioning) && (variation_coordinates == p_b.variation_coordinates) && (oversampling == p_b.oversampling) && (embolden == p_b.embolden) && (transform == p_b.transform) && (extra_spacing[SPACING_TOP] == p_b.extra_spacing[SPACING_TOP]) && (extra_spacing[SPACING_BOTTOM] == p_b.extra_spacing[SPACING_BOTTOM]) && (extra_spacing[SPACING_SPACE] == p_b.extra_spacing[SPACING_SPACE]) && (extra_spacing[SPACING_GLYPH] == p_b.extra_spacing[SPACING_GLYPH]) && (baseline_offset == p_b.baseline_offset);
		}

		SystemFontKey(const String &p_font_name, bool p_italic, int p_weight, int p_stretch, RID p_font, const TextServerAdvanced *p_fb) {
			font_name = p_font_name;
			italic = p_italic;
			weight = p_weight;
			stretch = p_stretch;
			antialiasing = p_fb->_font_get_antialiasing(p_font);
			disable_embedded_bitmaps = p_fb->_font_get_disable_embedded_bitmaps(p_font);
			mipmaps = p_fb->_font_get_generate_mipmaps(p_font);
			msdf = p_fb->_font_is_multichannel_signed_distance_field(p_font);
			msdf_range = p_fb->_font_get_msdf_pixel_range(p_font);
			msdf_source_size = p_fb->_font_get_msdf_size(p_font);
			fixed_size = p_fb->_font_get_fixed_size(p_font);
			force_autohinter = p_fb->_font_is_force_autohinter(p_font);
			hinting = p_fb->_font_get_hinting(p_font);
			subpixel_positioning = p_fb->_font_get_subpixel_positioning(p_font);
			variation_coordinates = p_fb->_font_get_variation_coordinates(p_font);
			oversampling = p_fb->_font_get_oversampling(p_font);
			embolden = p_fb->_font_get_embolden(p_font);
			transform = p_fb->_font_get_transform(p_font);
			extra_spacing[SPACING_TOP] = p_fb->_font_get_spacing(p_font, SPACING_TOP);
			extra_spacing[SPACING_BOTTOM] = p_fb->_font_get_spacing(p_font, SPACING_BOTTOM);
			extra_spacing[SPACING_SPACE] = p_fb->_font_get_spacing(p_font, SPACING_SPACE);
			extra_spacing[SPACING_GLYPH] = p_fb->_font_get_spacing(p_font, SPACING_GLYPH);
			baseline_offset = p_fb->_font_get_baseline_offset(p_font);
		}
	};

	struct SystemFontCacheRec {
		RID rid;
		int index = 0;
	};

	struct SystemFontCache {
		Vector<SystemFontCacheRec> var;
		int max_var = 0;
	};

	struct SystemFontKeyHasher {
		_FORCE_INLINE_ static uint32_t hash(const SystemFontKey &p_a) {
			uint32_t hash = p_a.font_name.hash();
			hash = hash_murmur3_one_32(p_a.variation_coordinates.hash(), hash);
			hash = hash_murmur3_one_32(p_a.weight, hash);
			hash = hash_murmur3_one_32(p_a.stretch, hash);
			hash = hash_murmur3_one_32(p_a.msdf_range, hash);
			hash = hash_murmur3_one_32(p_a.msdf_source_size, hash);
			hash = hash_murmur3_one_32(p_a.fixed_size, hash);
			hash = hash_murmur3_one_double(p_a.oversampling, hash);
			hash = hash_murmur3_one_double(p_a.embolden, hash);
			hash = hash_murmur3_one_real(p_a.transform[0].x, hash);
			hash = hash_murmur3_one_real(p_a.transform[0].y, hash);
			hash = hash_murmur3_one_real(p_a.transform[1].x, hash);
			hash = hash_murmur3_one_real(p_a.transform[1].y, hash);
			hash = hash_murmur3_one_32(p_a.extra_spacing[SPACING_TOP], hash);
			hash = hash_murmur3_one_32(p_a.extra_spacing[SPACING_BOTTOM], hash);
			hash = hash_murmur3_one_32(p_a.extra_spacing[SPACING_SPACE], hash);
			hash = hash_murmur3_one_32(p_a.extra_spacing[SPACING_GLYPH], hash);
			hash = hash_murmur3_one_double(p_a.baseline_offset, hash);
			return hash_fmix32(hash_murmur3_one_32(((int)p_a.mipmaps) | ((int)p_a.msdf << 1) | ((int)p_a.italic << 2) | ((int)p_a.force_autohinter << 3) | ((int)p_a.hinting << 4) | ((int)p_a.subpixel_positioning << 8) | ((int)p_a.antialiasing << 12) | ((int)p_a.disable_embedded_bitmaps << 14), hash));
		}
	};
	mutable HashMap<SystemFontKey, SystemFontCache, SystemFontKeyHasher> system_fonts;
	mutable HashMap<String, PackedByteArray> system_font_data;

	void _update_chars(ShapedTextDataAdvanced *p_sd) const;
	void _realign(ShapedTextDataAdvanced *p_sd) const;
	int64_t _convert_pos(const String &p_utf32, const Char16String &p_utf16, int64_t p_pos) const;
	int64_t _convert_pos(const ShapedTextDataAdvanced *p_sd, int64_t p_pos) const;
	int64_t _convert_pos_inv(const ShapedTextDataAdvanced *p_sd, int64_t p_pos) const;
	bool _shape_substr(ShapedTextDataAdvanced *p_new_sd, const ShapedTextDataAdvanced *p_sd, int64_t p_start, int64_t p_length) const;
	void _shape_run(ShapedTextDataAdvanced *p_sd, int64_t p_start, int64_t p_end, hb_script_t p_script, hb_direction_t p_direction, TypedArray<RID> p_fonts, int64_t p_span, int64_t p_fb_index, int64_t p_prev_start, int64_t p_prev_end, RID p_prev_font);
	Glyph _shape_single_glyph(ShapedTextDataAdvanced *p_sd, char32_t p_char, hb_script_t p_script, hb_direction_t p_direction, const RID &p_font, int64_t p_font_size);
	_FORCE_INLINE_ RID _find_sys_font_for_text(const RID &p_fdef, const String &p_script_code, const String &p_language, const String &p_text);

	_FORCE_INLINE_ void _add_featuers(const Dictionary &p_source, Vector<hb_feature_t> &r_ftrs);

	Mutex ft_mutex;

	// HarfBuzz bitmap font interface.

	static hb_font_funcs_t *funcs;

	struct bmp_font_t {
		TextServerAdvanced::FontForSizeAdvanced *face = nullptr;
		bool unref = false; /* Whether to destroy bm_face when done. */
	};

	static bmp_font_t *_bmp_font_create(TextServerAdvanced::FontForSizeAdvanced *p_face, bool p_unref);
	static void _bmp_font_destroy(void *p_data);
	static hb_bool_t _bmp_get_nominal_glyph(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_unicode, hb_codepoint_t *r_glyph, void *p_user_data);
	static hb_position_t _bmp_get_glyph_h_advance(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_glyph, void *p_user_data);
	static hb_position_t _bmp_get_glyph_v_advance(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_glyph, void *p_user_data);
	static hb_position_t _bmp_get_glyph_h_kerning(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_left_glyph, hb_codepoint_t p_right_glyph, void *p_user_data);
	static hb_bool_t _bmp_get_glyph_v_origin(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_glyph, hb_position_t *r_x, hb_position_t *r_y, void *p_user_data);
	static hb_bool_t _bmp_get_glyph_extents(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_glyph, hb_glyph_extents_t *r_extents, void *p_user_data);
	static hb_bool_t _bmp_get_font_h_extents(hb_font_t *p_font, void *p_font_data, hb_font_extents_t *r_metrics, void *p_user_data);
	static void _bmp_create_font_funcs();
	static void _bmp_free_font_funcs();
	static void _bmp_font_set_funcs(hb_font_t *p_font, TextServerAdvanced::FontForSizeAdvanced *p_face, bool p_unref);
	static hb_font_t *_bmp_font_create(TextServerAdvanced::FontForSizeAdvanced *p_face, hb_destroy_func_t p_destroy);

	hb_font_t *_font_get_hb_handle(const RID &p_font, int64_t p_font_size) const;

	struct GlyphCompare { // For line breaking reordering.
		_FORCE_INLINE_ bool operator()(const Glyph &l, const Glyph &r) const {
			if (l.start == r.start) {
				if (l.count == r.count) {
					return (l.flags & TextServer::GRAPHEME_IS_VIRTUAL) < (r.flags & TextServer::GRAPHEME_IS_VIRTUAL);
				}
				return l.count > r.count; // Sort first glyph with count & flags, order of the rest are irrelevant.
			} else {
				return l.start < r.start;
			}
		}
	};

protected:
	static void _bind_methods() {}

	void full_copy(ShapedTextDataAdvanced *p_shaped);
	void invalidate(ShapedTextDataAdvanced *p_shaped, bool p_text = false);

public:
	MODBIND1RC(bool, has_feature, Feature);
	MODBIND0RC(String, get_name);
	MODBIND0RC(int64_t, get_features);

	MODBIND1(free_rid, const RID &);
	MODBIND1R(bool, has, const RID &);
	MODBIND1R(bool, load_support_data, const String &);

	MODBIND0RC(String, get_support_data_filename);
	MODBIND0RC(String, get_support_data_info);
	MODBIND1RC(bool, save_support_data, const String &);

	MODBIND1RC(bool, is_locale_right_to_left, const String &);

	MODBIND1RC(int64_t, name_to_tag, const String &);
	MODBIND1RC(String, tag_to_name, int64_t);

	/* Font interface */

	MODBIND0R(RID, create_font);
	MODBIND1R(RID, create_font_linked_variation, const RID &);

	MODBIND2(font_set_data, const RID &, const PackedByteArray &);
	MODBIND3(font_set_data_ptr, const RID &, const uint8_t *, int64_t);

	MODBIND2(font_set_face_index, const RID &, int64_t);
	MODBIND1RC(int64_t, font_get_face_index, const RID &);

	MODBIND1RC(int64_t, font_get_face_count, const RID &);

	MODBIND2(font_set_style, const RID &, BitField<FontStyle>);
	MODBIND1RC(BitField<FontStyle>, font_get_style, const RID &);

	MODBIND2(font_set_style_name, const RID &, const String &);
	MODBIND1RC(String, font_get_style_name, const RID &);

	MODBIND2(font_set_weight, const RID &, int64_t);
	MODBIND1RC(int64_t, font_get_weight, const RID &);

	MODBIND2(font_set_stretch, const RID &, int64_t);
	MODBIND1RC(int64_t, font_get_stretch, const RID &);

	MODBIND2(font_set_name, const RID &, const String &);
	MODBIND1RC(String, font_get_name, const RID &);
	MODBIND1RC(Dictionary, font_get_ot_name_strings, const RID &);

	MODBIND2(font_set_antialiasing, const RID &, TextServer::FontAntialiasing);
	MODBIND1RC(TextServer::FontAntialiasing, font_get_antialiasing, const RID &);

	MODBIND2(font_set_disable_embedded_bitmaps, const RID &, bool);
	MODBIND1RC(bool, font_get_disable_embedded_bitmaps, const RID &);

	MODBIND2(font_set_generate_mipmaps, const RID &, bool);
	MODBIND1RC(bool, font_get_generate_mipmaps, const RID &);

	MODBIND2(font_set_multichannel_signed_distance_field, const RID &, bool);
	MODBIND1RC(bool, font_is_multichannel_signed_distance_field, const RID &);

	MODBIND2(font_set_msdf_pixel_range, const RID &, int64_t);
	MODBIND1RC(int64_t, font_get_msdf_pixel_range, const RID &);

	MODBIND2(font_set_msdf_size, const RID &, int64_t);
	MODBIND1RC(int64_t, font_get_msdf_size, const RID &);

	MODBIND2(font_set_fixed_size, const RID &, int64_t);
	MODBIND1RC(int64_t, font_get_fixed_size, const RID &);

	MODBIND2(font_set_fixed_size_scale_mode, const RID &, FixedSizeScaleMode);
	MODBIND1RC(FixedSizeScaleMode, font_get_fixed_size_scale_mode, const RID &);

	MODBIND2(font_set_allow_system_fallback, const RID &, bool);
	MODBIND1RC(bool, font_is_allow_system_fallback, const RID &);

	MODBIND2(font_set_force_autohinter, const RID &, bool);
	MODBIND1RC(bool, font_is_force_autohinter, const RID &);

	MODBIND2(font_set_subpixel_positioning, const RID &, SubpixelPositioning);
	MODBIND1RC(SubpixelPositioning, font_get_subpixel_positioning, const RID &);

	MODBIND2(font_set_embolden, const RID &, double);
	MODBIND1RC(double, font_get_embolden, const RID &);

	MODBIND3(font_set_spacing, const RID &, SpacingType, int64_t);
	MODBIND2RC(int64_t, font_get_spacing, const RID &, SpacingType);

	MODBIND2(font_set_baseline_offset, const RID &, double);
	MODBIND1RC(double, font_get_baseline_offset, const RID &);

	MODBIND2(font_set_transform, const RID &, const Transform2D &);
	MODBIND1RC(Transform2D, font_get_transform, const RID &);

	MODBIND2(font_set_variation_coordinates, const RID &, const Dictionary &);
	MODBIND1RC(Dictionary, font_get_variation_coordinates, const RID &);

	MODBIND2(font_set_hinting, const RID &, TextServer::Hinting);
	MODBIND1RC(TextServer::Hinting, font_get_hinting, const RID &);

	MODBIND2(font_set_oversampling, const RID &, double);
	MODBIND1RC(double, font_get_oversampling, const RID &);

	MODBIND1RC(TypedArray<Vector2i>, font_get_size_cache_list, const RID &);
	MODBIND1(font_clear_size_cache, const RID &);
	MODBIND2(font_remove_size_cache, const RID &, const Vector2i &);

	MODBIND3(font_set_ascent, const RID &, int64_t, double);
	MODBIND2RC(double, font_get_ascent, const RID &, int64_t);

	MODBIND3(font_set_descent, const RID &, int64_t, double);
	MODBIND2RC(double, font_get_descent, const RID &, int64_t);

	MODBIND3(font_set_underline_position, const RID &, int64_t, double);
	MODBIND2RC(double, font_get_underline_position, const RID &, int64_t);

	MODBIND3(font_set_underline_thickness, const RID &, int64_t, double);
	MODBIND2RC(double, font_get_underline_thickness, const RID &, int64_t);

	MODBIND3(font_set_scale, const RID &, int64_t, double);
	MODBIND2RC(double, font_get_scale, const RID &, int64_t);

	MODBIND2RC(int64_t, font_get_texture_count, const RID &, const Vector2i &);
	MODBIND2(font_clear_textures, const RID &, const Vector2i &);
	MODBIND3(font_remove_texture, const RID &, const Vector2i &, int64_t);

	MODBIND4(font_set_texture_image, const RID &, const Vector2i &, int64_t, const Ref<Image> &);
	MODBIND3RC(Ref<Image>, font_get_texture_image, const RID &, const Vector2i &, int64_t);

	MODBIND4(font_set_texture_offsets, const RID &, const Vector2i &, int64_t, const PackedInt32Array &);
	MODBIND3RC(PackedInt32Array, font_get_texture_offsets, const RID &, const Vector2i &, int64_t);

	MODBIND2RC(PackedInt32Array, font_get_glyph_list, const RID &, const Vector2i &);
	MODBIND2(font_clear_glyphs, const RID &, const Vector2i &);
	MODBIND3(font_remove_glyph, const RID &, const Vector2i &, int64_t);

	MODBIND3RC(Vector2, font_get_glyph_advance, const RID &, int64_t, int64_t);
	MODBIND4(font_set_glyph_advance, const RID &, int64_t, int64_t, const Vector2 &);

	MODBIND3RC(Vector2, font_get_glyph_offset, const RID &, const Vector2i &, int64_t);
	MODBIND4(font_set_glyph_offset, const RID &, const Vector2i &, int64_t, const Vector2 &);

	MODBIND3RC(Vector2, font_get_glyph_size, const RID &, const Vector2i &, int64_t);
	MODBIND4(font_set_glyph_size, const RID &, const Vector2i &, int64_t, const Vector2 &);

	MODBIND3RC(Rect2, font_get_glyph_uv_rect, const RID &, const Vector2i &, int64_t);
	MODBIND4(font_set_glyph_uv_rect, const RID &, const Vector2i &, int64_t, const Rect2 &);

	MODBIND3RC(int64_t, font_get_glyph_texture_idx, const RID &, const Vector2i &, int64_t);
	MODBIND4(font_set_glyph_texture_idx, const RID &, const Vector2i &, int64_t, int64_t);

	MODBIND3RC(RID, font_get_glyph_texture_rid, const RID &, const Vector2i &, int64_t);
	MODBIND3RC(Size2, font_get_glyph_texture_size, const RID &, const Vector2i &, int64_t);

	MODBIND3RC(Dictionary, font_get_glyph_contours, const RID &, int64_t, int64_t);

	MODBIND2RC(TypedArray<Vector2i>, font_get_kerning_list, const RID &, int64_t);
	MODBIND2(font_clear_kerning_map, const RID &, int64_t);
	MODBIND3(font_remove_kerning, const RID &, int64_t, const Vector2i &);

	MODBIND4(font_set_kerning, const RID &, int64_t, const Vector2i &, const Vector2 &);
	MODBIND3RC(Vector2, font_get_kerning, const RID &, int64_t, const Vector2i &);

	MODBIND4RC(int64_t, font_get_glyph_index, const RID &, int64_t, int64_t, int64_t);
	MODBIND3RC(int64_t, font_get_char_from_glyph_index, const RID &, int64_t, int64_t);

	MODBIND2RC(bool, font_has_char, const RID &, int64_t);
	MODBIND1RC(String, font_get_supported_chars, const RID &);
	MODBIND1RC(PackedInt32Array, font_get_supported_glyphs, const RID &);

	MODBIND4(font_render_range, const RID &, const Vector2i &, int64_t, int64_t);
	MODBIND3(font_render_glyph, const RID &, const Vector2i &, int64_t);

	MODBIND6C(font_draw_glyph, const RID &, const RID &, int64_t, const Vector2 &, int64_t, const Color &);
	MODBIND7C(font_draw_glyph_outline, const RID &, const RID &, int64_t, int64_t, const Vector2 &, int64_t, const Color &);

	MODBIND2RC(bool, font_is_language_supported, const RID &, const String &);
	MODBIND3(font_set_language_support_override, const RID &, const String &, bool);
	MODBIND2R(bool, font_get_language_support_override, const RID &, const String &);
	MODBIND2(font_remove_language_support_override, const RID &, const String &);
	MODBIND1R(PackedStringArray, font_get_language_support_overrides, const RID &);

	MODBIND2RC(bool, font_is_script_supported, const RID &, const String &);
	MODBIND3(font_set_script_support_override, const RID &, const String &, bool);
	MODBIND2R(bool, font_get_script_support_override, const RID &, const String &);
	MODBIND2(font_remove_script_support_override, const RID &, const String &);
	MODBIND1R(PackedStringArray, font_get_script_support_overrides, const RID &);

	MODBIND2(font_set_opentype_feature_overrides, const RID &, const Dictionary &);
	MODBIND1RC(Dictionary, font_get_opentype_feature_overrides, const RID &);

	MODBIND1RC(Dictionary, font_supported_feature_list, const RID &);
	MODBIND1RC(Dictionary, font_supported_variation_list, const RID &);

	MODBIND0RC(double, font_get_global_oversampling);
	MODBIND1(font_set_global_oversampling, double);

	/* Shaped text buffer interface */

	MODBIND2R(RID, create_shaped_text, Direction, Orientation);

	MODBIND1(shaped_text_clear, const RID &);

	MODBIND2(shaped_text_set_direction, const RID &, Direction);
	MODBIND1RC(Direction, shaped_text_get_direction, const RID &);
	MODBIND1RC(Direction, shaped_text_get_inferred_direction, const RID &);

	MODBIND2(shaped_text_set_bidi_override, const RID &, const Array &);

	MODBIND2(shaped_text_set_custom_punctuation, const RID &, const String &);
	MODBIND1RC(String, shaped_text_get_custom_punctuation, const RID &);

	MODBIND2(shaped_text_set_custom_ellipsis, const RID &, int64_t);
	MODBIND1RC(int64_t, shaped_text_get_custom_ellipsis, const RID &);

	MODBIND2(shaped_text_set_orientation, const RID &, Orientation);
	MODBIND1RC(Orientation, shaped_text_get_orientation, const RID &);

	MODBIND2(shaped_text_set_preserve_invalid, const RID &, bool);
	MODBIND1RC(bool, shaped_text_get_preserve_invalid, const RID &);

	MODBIND2(shaped_text_set_preserve_control, const RID &, bool);
	MODBIND1RC(bool, shaped_text_get_preserve_control, const RID &);

	MODBIND3(shaped_text_set_spacing, const RID &, SpacingType, int64_t);
	MODBIND2RC(int64_t, shaped_text_get_spacing, const RID &, SpacingType);

	MODBIND7R(bool, shaped_text_add_string, const RID &, const String &, const TypedArray<RID> &, int64_t, const Dictionary &, const String &, const Variant &);
	MODBIND6R(bool, shaped_text_add_object, const RID &, const Variant &, const Size2 &, InlineAlignment, int64_t, double);
	MODBIND5R(bool, shaped_text_resize_object, const RID &, const Variant &, const Size2 &, InlineAlignment, double);

	MODBIND1RC(int64_t, shaped_get_span_count, const RID &);
	MODBIND2RC(Variant, shaped_get_span_meta, const RID &, int64_t);
	MODBIND5(shaped_set_span_update_font, const RID &, int64_t, const TypedArray<RID> &, int64_t, const Dictionary &);

	MODBIND3RC(RID, shaped_text_substr, const RID &, int64_t, int64_t);
	MODBIND1RC(RID, shaped_text_get_parent, const RID &);

	MODBIND3R(double, shaped_text_fit_to_width, const RID &, double, BitField<TextServer::JustificationFlag>);
	MODBIND2R(double, shaped_text_tab_align, const RID &, const PackedFloat32Array &);

	MODBIND1R(bool, shaped_text_shape, const RID &);
	MODBIND1R(bool, shaped_text_update_breaks, const RID &);
	MODBIND1R(bool, shaped_text_update_justification_ops, const RID &);

	MODBIND1RC(int64_t, shaped_text_get_trim_pos, const RID &);
	MODBIND1RC(int64_t, shaped_text_get_ellipsis_pos, const RID &);
	MODBIND1RC(const Glyph *, shaped_text_get_ellipsis_glyphs, const RID &);
	MODBIND1RC(int64_t, shaped_text_get_ellipsis_glyph_count, const RID &);

	MODBIND3(shaped_text_overrun_trim_to_width, const RID &, double, BitField<TextServer::TextOverrunFlag>);

	MODBIND1RC(bool, shaped_text_is_ready, const RID &);

	MODBIND1RC(const Glyph *, shaped_text_get_glyphs, const RID &);
	MODBIND1R(const Glyph *, shaped_text_sort_logical, const RID &);
	MODBIND1RC(int64_t, shaped_text_get_glyph_count, const RID &);

	MODBIND1RC(Vector2i, shaped_text_get_range, const RID &);

	MODBIND1RC(Array, shaped_text_get_objects, const RID &);
	MODBIND2RC(Rect2, shaped_text_get_object_rect, const RID &, const Variant &);
	MODBIND2RC(Vector2i, shaped_text_get_object_range, const RID &, const Variant &);
	MODBIND2RC(int64_t, shaped_text_get_object_glyph, const RID &, const Variant &);

	MODBIND1RC(Size2, shaped_text_get_size, const RID &);
	MODBIND1RC(double, shaped_text_get_ascent, const RID &);
	MODBIND1RC(double, shaped_text_get_descent, const RID &);
	MODBIND1RC(double, shaped_text_get_width, const RID &);
	MODBIND1RC(double, shaped_text_get_underline_position, const RID &);
	MODBIND1RC(double, shaped_text_get_underline_thickness, const RID &);

	MODBIND1RC(PackedInt32Array, shaped_text_get_character_breaks, const RID &);

	MODBIND2RC(String, format_number, const String &, const String &);
	MODBIND2RC(String, parse_number, const String &, const String &);
	MODBIND1RC(String, percent_sign, const String &);

	MODBIND3RC(PackedInt32Array, string_get_word_breaks, const String &, const String &, int64_t);
	MODBIND2RC(PackedInt32Array, string_get_character_breaks, const String &, const String &);

	MODBIND2RC(int64_t, is_confusable, const String &, const PackedStringArray &);
	MODBIND1RC(bool, spoof_check, const String &);

	MODBIND1RC(String, strip_diacritics, const String &);
	MODBIND1RC(bool, is_valid_identifier, const String &);
	MODBIND1RC(bool, is_valid_letter, uint64_t);

	MODBIND2RC(String, string_to_upper, const String &, const String &);
	MODBIND2RC(String, string_to_lower, const String &, const String &);
	MODBIND2RC(String, string_to_title, const String &, const String &);

	MODBIND0(cleanup);

	TextServerAdvanced();
	~TextServerAdvanced();
};

#endif // TEXT_SERVER_ADV_H
