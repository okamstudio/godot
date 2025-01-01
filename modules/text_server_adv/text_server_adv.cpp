/**************************************************************************/
/*  text_server_adv.cpp                                                   */
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

#include "text_server_adv.h"

#ifdef GDEXTENSION
// Headers for building as GDExtension plug-in.

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/core/error_macros.hpp>

using namespace godot;

#define GLOBAL_GET(m_var) ProjectSettings::get_singleton()->get_setting_with_override(m_var)

#elif defined(GODOT_MODULE)
// Headers for building as built-in module.

#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/object/worker_thread_pool.h"
#include "core/string/translation_server.h"
#include "scene/resources/image_texture.h"

#include "modules/modules_enabled.gen.h" // For freetype, msdfgen, svg.

#endif

// Built-in ICU data.

#ifdef ICU_STATIC_DATA
#include "icudata.gen.h"
#endif

// Thirdparty headers.

#ifdef MODULE_MSDFGEN_ENABLED
#ifdef _MSC_VER
#pragma warning(disable : 4458)
#endif
#include <core/ShapeDistanceFinder.h>
#include <core/contour-combiners.h>
#include <core/edge-selectors.h>
#include <msdfgen.h>
#ifdef _MSC_VER
#pragma warning(default : 4458)
#endif
#endif

#ifdef MODULE_SVG_ENABLED
#ifdef MODULE_FREETYPE_ENABLED
#include "thorvg_svg_in_ot.h"
#endif
#endif

/*************************************************************************/
/*  bmp_font_t HarfBuzz Bitmap font interface                            */
/*************************************************************************/

hb_font_funcs_t *TextServerAdvanced::funcs = nullptr;

TextServerAdvanced::bmp_font_t *TextServerAdvanced::_bmp_font_create(TextServerAdvanced::FontForSizeAdvanced *p_face, bool p_unref) {
	bmp_font_t *bm_font = memnew(bmp_font_t);

	if (!bm_font) {
		return nullptr;
	}

	bm_font->face = p_face;
	bm_font->unref = p_unref;

	return bm_font;
}

void TextServerAdvanced::_bmp_font_destroy(void *p_data) {
	bmp_font_t *bm_font = static_cast<bmp_font_t *>(p_data);
	memdelete(bm_font);
}

hb_bool_t TextServerAdvanced::_bmp_get_nominal_glyph(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_unicode, hb_codepoint_t *r_glyph, void *p_user_data) {
	const bmp_font_t *bm_font = static_cast<const bmp_font_t *>(p_font_data);

	if (!bm_font->face) {
		return false;
	}

	if (!bm_font->face->glyph_map.has(p_unicode)) {
		if (bm_font->face->glyph_map.has(0xf000u + p_unicode)) {
			*r_glyph = 0xf000u + p_unicode;
			return true;
		} else {
			return false;
		}
	}

	*r_glyph = p_unicode;
	return true;
}

hb_position_t TextServerAdvanced::_bmp_get_glyph_h_advance(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_glyph, void *p_user_data) {
	const bmp_font_t *bm_font = static_cast<const bmp_font_t *>(p_font_data);

	if (!bm_font->face) {
		return 0;
	}

	HashMap<int32_t, FontGlyph>::Iterator E = bm_font->face->glyph_map.find(p_glyph);
	if (!E) {
		return 0;
	}

	return E->value.advance.x * 64;
}

hb_position_t TextServerAdvanced::_bmp_get_glyph_v_advance(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_glyph, void *p_user_data) {
	const bmp_font_t *bm_font = static_cast<const bmp_font_t *>(p_font_data);

	if (!bm_font->face) {
		return 0;
	}

	HashMap<int32_t, FontGlyph>::Iterator E = bm_font->face->glyph_map.find(p_glyph);
	if (!E) {
		return 0;
	}

	return -E->value.advance.y * 64;
}

hb_position_t TextServerAdvanced::_bmp_get_glyph_h_kerning(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_left_glyph, hb_codepoint_t p_right_glyph, void *p_user_data) {
	const bmp_font_t *bm_font = static_cast<const bmp_font_t *>(p_font_data);

	if (!bm_font->face) {
		return 0;
	}

	if (!bm_font->face->kerning_map.has(Vector2i(p_left_glyph, p_right_glyph))) {
		return 0;
	}

	return bm_font->face->kerning_map[Vector2i(p_left_glyph, p_right_glyph)].x * 64;
}

hb_bool_t TextServerAdvanced::_bmp_get_glyph_v_origin(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_glyph, hb_position_t *r_x, hb_position_t *r_y, void *p_user_data) {
	const bmp_font_t *bm_font = static_cast<const bmp_font_t *>(p_font_data);

	if (!bm_font->face) {
		return false;
	}

	HashMap<int32_t, FontGlyph>::Iterator E = bm_font->face->glyph_map.find(p_glyph);
	if (!E) {
		return false;
	}

	*r_x = E->value.advance.x * 32;
	*r_y = -bm_font->face->ascent * 64;

	return true;
}

hb_bool_t TextServerAdvanced::_bmp_get_glyph_extents(hb_font_t *p_font, void *p_font_data, hb_codepoint_t p_glyph, hb_glyph_extents_t *r_extents, void *p_user_data) {
	const bmp_font_t *bm_font = static_cast<const bmp_font_t *>(p_font_data);

	if (!bm_font->face) {
		return false;
	}

	HashMap<int32_t, FontGlyph>::Iterator E = bm_font->face->glyph_map.find(p_glyph);
	if (!E) {
		return false;
	}

	r_extents->x_bearing = 0;
	r_extents->y_bearing = 0;
	r_extents->width = E->value.rect.size.x * 64;
	r_extents->height = E->value.rect.size.y * 64;

	return true;
}

hb_bool_t TextServerAdvanced::_bmp_get_font_h_extents(hb_font_t *p_font, void *p_font_data, hb_font_extents_t *r_metrics, void *p_user_data) {
	const bmp_font_t *bm_font = static_cast<const bmp_font_t *>(p_font_data);

	if (!bm_font->face) {
		return false;
	}

	r_metrics->ascender = bm_font->face->ascent;
	r_metrics->descender = bm_font->face->descent;
	r_metrics->line_gap = 0;

	return true;
}

void TextServerAdvanced::_bmp_create_font_funcs() {
	if (funcs == nullptr) {
		funcs = hb_font_funcs_create();

		hb_font_funcs_set_font_h_extents_func(funcs, _bmp_get_font_h_extents, nullptr, nullptr);
		hb_font_funcs_set_nominal_glyph_func(funcs, _bmp_get_nominal_glyph, nullptr, nullptr);
		hb_font_funcs_set_glyph_h_advance_func(funcs, _bmp_get_glyph_h_advance, nullptr, nullptr);
		hb_font_funcs_set_glyph_v_advance_func(funcs, _bmp_get_glyph_v_advance, nullptr, nullptr);
		hb_font_funcs_set_glyph_v_origin_func(funcs, _bmp_get_glyph_v_origin, nullptr, nullptr);
		hb_font_funcs_set_glyph_h_kerning_func(funcs, _bmp_get_glyph_h_kerning, nullptr, nullptr);
		hb_font_funcs_set_glyph_extents_func(funcs, _bmp_get_glyph_extents, nullptr, nullptr);

		hb_font_funcs_make_immutable(funcs);
	}
}

void TextServerAdvanced::_bmp_free_font_funcs() {
	if (funcs != nullptr) {
		hb_font_funcs_destroy(funcs);
		funcs = nullptr;
	}
}

void TextServerAdvanced::_bmp_font_set_funcs(hb_font_t *p_font, TextServerAdvanced::FontForSizeAdvanced *p_face, bool p_unref) {
	hb_font_set_funcs(p_font, funcs, _bmp_font_create(p_face, p_unref), _bmp_font_destroy);
}

hb_font_t *TextServerAdvanced::_bmp_font_create(TextServerAdvanced::FontForSizeAdvanced *p_face, hb_destroy_func_t p_destroy) {
	hb_font_t *font;
	hb_face_t *face = hb_face_create(nullptr, 0);

	font = hb_font_create(face);
	hb_face_destroy(face);
	_bmp_font_set_funcs(font, p_face, false);
	return font;
}

/*************************************************************************/
/*  Character properties.                                                */
/*************************************************************************/

_FORCE_INLINE_ bool is_ain(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_AIN;
}

_FORCE_INLINE_ bool is_alef(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_ALEF;
}

_FORCE_INLINE_ bool is_beh(char32_t p_chr) {
	int32_t prop = u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP);
	return (prop == U_JG_BEH) || (prop == U_JG_NOON) || (prop == U_JG_AFRICAN_NOON) || (prop == U_JG_NYA) || (prop == U_JG_YEH) || (prop == U_JG_FARSI_YEH);
}

_FORCE_INLINE_ bool is_dal(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_DAL;
}

_FORCE_INLINE_ bool is_feh(char32_t p_chr) {
	return (u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_FEH) || (u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_AFRICAN_FEH);
}

_FORCE_INLINE_ bool is_gaf(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_GAF;
}

_FORCE_INLINE_ bool is_heh(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_HEH;
}

_FORCE_INLINE_ bool is_kaf(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_KAF;
}

_FORCE_INLINE_ bool is_lam(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_LAM;
}

_FORCE_INLINE_ bool is_qaf(char32_t p_chr) {
	return (u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_QAF) || (u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_AFRICAN_QAF);
}

_FORCE_INLINE_ bool is_reh(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_REH;
}

_FORCE_INLINE_ bool is_seen_sad(char32_t p_chr) {
	return (u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_SAD) || (u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_SEEN);
}

_FORCE_INLINE_ bool is_tah(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_TAH;
}

_FORCE_INLINE_ bool is_teh_marbuta(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_TEH_MARBUTA;
}

_FORCE_INLINE_ bool is_yeh(char32_t p_chr) {
	int32_t prop = u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP);
	return (prop == U_JG_YEH) || (prop == U_JG_FARSI_YEH) || (prop == U_JG_YEH_BARREE) || (prop == U_JG_BURUSHASKI_YEH_BARREE) || (prop == U_JG_YEH_WITH_TAIL);
}

_FORCE_INLINE_ bool is_waw(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_GROUP) == U_JG_WAW;
}

_FORCE_INLINE_ bool is_transparent(char32_t p_chr) {
	return u_getIntPropertyValue(p_chr, UCHAR_JOINING_TYPE) == U_JT_TRANSPARENT;
}

_FORCE_INLINE_ bool is_ligature(char32_t p_chr, char32_t p_nchr) {
	return (is_lam(p_chr) && is_alef(p_nchr));
}

_FORCE_INLINE_ bool is_connected_to_prev(char32_t p_chr, char32_t p_pchr) {
	int32_t prop = u_getIntPropertyValue(p_pchr, UCHAR_JOINING_TYPE);
	return (prop != U_JT_RIGHT_JOINING) && (prop != U_JT_NON_JOINING) ? !is_ligature(p_pchr, p_chr) : false;
}

/*************************************************************************/

bool TextServerAdvanced::icu_data_loaded = false;
PackedByteArray TextServerAdvanced::icu_data;

bool TextServerAdvanced::_has_feature(Feature p_feature) const {
	switch (p_feature) {
		case FEATURE_SIMPLE_LAYOUT:
		case FEATURE_BIDI_LAYOUT:
		case FEATURE_VERTICAL_LAYOUT:
		case FEATURE_SHAPING:
		case FEATURE_KASHIDA_JUSTIFICATION:
		case FEATURE_BREAK_ITERATORS:
		case FEATURE_FONT_BITMAP:
#ifdef MODULE_FREETYPE_ENABLED
		case FEATURE_FONT_DYNAMIC:
#endif
#ifdef MODULE_MSDFGEN_ENABLED
		case FEATURE_FONT_MSDF:
#endif
		case FEATURE_FONT_VARIABLE:
		case FEATURE_CONTEXT_SENSITIVE_CASE_CONVERSION:
		case FEATURE_USE_SUPPORT_DATA:
		case FEATURE_UNICODE_IDENTIFIERS:
		case FEATURE_UNICODE_SECURITY:
			return true;
		default: {
		}
	}
	return false;
}

String TextServerAdvanced::_get_name() const {
#ifdef GDEXTENSION
	return "ICU / HarfBuzz / Graphite (GDExtension)";
#elif defined(GODOT_MODULE)
	return "ICU / HarfBuzz / Graphite (Built-in)";
#endif
}

int64_t TextServerAdvanced::_get_features() const {
	int64_t interface_features = FEATURE_SIMPLE_LAYOUT | FEATURE_BIDI_LAYOUT | FEATURE_VERTICAL_LAYOUT | FEATURE_SHAPING | FEATURE_KASHIDA_JUSTIFICATION | FEATURE_BREAK_ITERATORS | FEATURE_FONT_BITMAP | FEATURE_FONT_VARIABLE | FEATURE_CONTEXT_SENSITIVE_CASE_CONVERSION | FEATURE_USE_SUPPORT_DATA;
#ifdef MODULE_FREETYPE_ENABLED
	interface_features |= FEATURE_FONT_DYNAMIC;
#endif
#ifdef MODULE_MSDFGEN_ENABLED
	interface_features |= FEATURE_FONT_MSDF;
#endif

	return interface_features;
}

void TextServerAdvanced::_free_rid(const RID &p_rid) {
	_THREAD_SAFE_METHOD_
	if (font_owner.owns(p_rid)) {
		MutexLock ftlock(ft_mutex);

		FontAdvanced *fd = font_owner.get_or_null(p_rid);
		{
			MutexLock lock(fd->mutex);
			font_owner.free(p_rid);
		}
		memdelete(fd);
	} else if (font_var_owner.owns(p_rid)) {
		MutexLock ftlock(ft_mutex);

		FontAdvancedLinkedVariation *fdv = font_var_owner.get_or_null(p_rid);
		{
			font_var_owner.free(p_rid);
		}
		memdelete(fdv);
	} else if (shaped_owner.owns(p_rid)) {
		ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_rid);
		{
			MutexLock lock(sd->mutex);
			shaped_owner.free(p_rid);
		}
		memdelete(sd);
	}
}

bool TextServerAdvanced::_has(const RID &p_rid) {
	_THREAD_SAFE_METHOD_
	return font_owner.owns(p_rid) || font_var_owner.owns(p_rid) || shaped_owner.owns(p_rid);
}

bool TextServerAdvanced::_load_support_data(const String &p_filename) {
	_THREAD_SAFE_METHOD_

#ifdef ICU_STATIC_DATA
	if (!icu_data_loaded) {
		UErrorCode err = U_ZERO_ERROR;
		u_init(&err); // Do not check for errors, since we only load part of the data.
		icu_data_loaded = true;
	}
#else
	if (!icu_data_loaded) {
		UErrorCode err = U_ZERO_ERROR;
#ifdef ICU_DATA_NAME
		String filename = (p_filename.is_empty()) ? String("res://") + _MKSTR(ICU_DATA_NAME) : p_filename;

		Ref<FileAccess> f = FileAccess::open(filename, FileAccess::READ);
		if (f.is_null()) {
			return false;
		}
		uint64_t len = f->get_length();
		icu_data = f->get_buffer(len);

		udata_setCommonData(icu_data.ptr(), &err);
		if (U_FAILURE(err)) {
			ERR_FAIL_V_MSG(false, u_errorName(err));
		}

		err = U_ZERO_ERROR;
#endif
		u_init(&err);
		if (U_FAILURE(err)) {
			ERR_FAIL_V_MSG(false, u_errorName(err));
		}
		icu_data_loaded = true;
	}
#endif
	return true;
}

String TextServerAdvanced::_get_support_data_filename() const {
	return _MKSTR(ICU_DATA_NAME);
}

String TextServerAdvanced::_get_support_data_info() const {
	return String("ICU break iteration data (") + _MKSTR(ICU_DATA_NAME) + String(").");
}

bool TextServerAdvanced::_save_support_data(const String &p_filename) const {
	_THREAD_SAFE_METHOD_
#ifdef ICU_STATIC_DATA

	// Store data to the res file if it's available.

	Ref<FileAccess> f = FileAccess::open(p_filename, FileAccess::WRITE);
	if (f.is_null()) {
		return false;
	}

	PackedByteArray icu_data_static;
	icu_data_static.resize(U_ICUDATA_SIZE);
	memcpy(icu_data_static.ptrw(), U_ICUDATA_ENTRY_POINT, U_ICUDATA_SIZE);
	f->store_buffer(icu_data_static);

	return true;
#else
	return false;
#endif
}

PackedByteArray TextServerAdvanced::_get_support_data() const {
	_THREAD_SAFE_METHOD_
#ifdef ICU_STATIC_DATA

	PackedByteArray icu_data_static;
	icu_data_static.resize(U_ICUDATA_SIZE);
	memcpy(icu_data_static.ptrw(), U_ICUDATA_ENTRY_POINT, U_ICUDATA_SIZE);

	return icu_data_static;
#else
	return icu_data;
#endif
}

bool TextServerAdvanced::_is_locale_right_to_left(const String &p_locale) const {
	String l = p_locale.get_slicec('_', 0);
	if ((l == "ar") || (l == "dv") || (l == "he") || (l == "fa") || (l == "ff") || (l == "ku") || (l == "ur")) {
		return true;
	} else {
		return false;
	}
}

_FORCE_INLINE_ void TextServerAdvanced::_insert_feature(const StringName &p_name, int32_t p_tag, Variant::Type p_vtype, bool p_hidden) {
	FeatureInfo fi;
	fi.name = p_name;
	fi.vtype = p_vtype;
	fi.hidden = p_hidden;

	feature_sets.insert(p_name, p_tag);
	feature_sets_inv.insert(p_tag, fi);
}

void TextServerAdvanced::_insert_feature_sets() {
	// Registered OpenType feature tags.
	// Name, Tag, Data Type, Hidden
	_insert_feature("access_all_alternates", HB_TAG('a', 'a', 'l', 't'), Variant::Type::INT, false);
	_insert_feature("above_base_forms", HB_TAG('a', 'b', 'v', 'f'), Variant::Type::INT, true);
	_insert_feature("above_base_mark_positioning", HB_TAG('a', 'b', 'v', 'm'), Variant::Type::INT, true);
	_insert_feature("above_base_substitutions", HB_TAG('a', 'b', 'v', 's'), Variant::Type::INT, true);
	_insert_feature("alternative_fractions", HB_TAG('a', 'f', 'r', 'c'), Variant::Type::INT, false);
	_insert_feature("akhands", HB_TAG('a', 'k', 'h', 'n'), Variant::Type::INT, true);
	_insert_feature("below_base_forms", HB_TAG('b', 'l', 'w', 'f'), Variant::Type::INT, true);
	_insert_feature("below_base_mark_positioning", HB_TAG('b', 'l', 'w', 'm'), Variant::Type::INT, true);
	_insert_feature("below_base_substitutions", HB_TAG('b', 'l', 'w', 's'), Variant::Type::INT, true);
	_insert_feature("contextual_alternates", HB_TAG('c', 'a', 'l', 't'), Variant::Type::BOOL, false);
	_insert_feature("case_sensitive_forms", HB_TAG('c', 'a', 's', 'e'), Variant::Type::BOOL, false);
	_insert_feature("glyph_composition", HB_TAG('c', 'c', 'm', 'p'), Variant::Type::INT, true);
	_insert_feature("conjunct_form_after_ro", HB_TAG('c', 'f', 'a', 'r'), Variant::Type::INT, true);
	_insert_feature("contextual_half_width_spacing", HB_TAG('c', 'h', 'w', 's'), Variant::Type::INT, true);
	_insert_feature("conjunct_forms", HB_TAG('c', 'j', 'c', 't'), Variant::Type::INT, true);
	_insert_feature("contextual_ligatures", HB_TAG('c', 'l', 'i', 'g'), Variant::Type::BOOL, false);
	_insert_feature("centered_cjk_punctuation", HB_TAG('c', 'p', 'c', 't'), Variant::Type::BOOL, false);
	_insert_feature("capital_spacing", HB_TAG('c', 'p', 's', 'p'), Variant::Type::BOOL, false);
	_insert_feature("contextual_swash", HB_TAG('c', 's', 'w', 'h'), Variant::Type::INT, false);
	_insert_feature("cursive_positioning", HB_TAG('c', 'u', 'r', 's'), Variant::Type::INT, true);
	_insert_feature("character_variant_01", HB_TAG('c', 'v', '0', '1'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_02", HB_TAG('c', 'v', '0', '2'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_03", HB_TAG('c', 'v', '0', '3'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_04", HB_TAG('c', 'v', '0', '4'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_05", HB_TAG('c', 'v', '0', '5'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_06", HB_TAG('c', 'v', '0', '6'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_07", HB_TAG('c', 'v', '0', '7'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_08", HB_TAG('c', 'v', '0', '8'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_09", HB_TAG('c', 'v', '0', '9'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_10", HB_TAG('c', 'v', '1', '0'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_11", HB_TAG('c', 'v', '1', '1'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_12", HB_TAG('c', 'v', '1', '2'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_13", HB_TAG('c', 'v', '1', '3'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_14", HB_TAG('c', 'v', '1', '4'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_15", HB_TAG('c', 'v', '1', '5'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_16", HB_TAG('c', 'v', '1', '6'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_17", HB_TAG('c', 'v', '1', '7'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_18", HB_TAG('c', 'v', '1', '8'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_19", HB_TAG('c', 'v', '1', '9'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_20", HB_TAG('c', 'v', '2', '0'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_21", HB_TAG('c', 'v', '2', '1'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_22", HB_TAG('c', 'v', '2', '2'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_23", HB_TAG('c', 'v', '2', '3'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_24", HB_TAG('c', 'v', '2', '4'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_25", HB_TAG('c', 'v', '2', '5'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_26", HB_TAG('c', 'v', '2', '6'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_27", HB_TAG('c', 'v', '2', '7'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_28", HB_TAG('c', 'v', '2', '8'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_29", HB_TAG('c', 'v', '2', '9'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_30", HB_TAG('c', 'v', '3', '0'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_31", HB_TAG('c', 'v', '3', '1'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_32", HB_TAG('c', 'v', '3', '2'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_33", HB_TAG('c', 'v', '3', '3'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_34", HB_TAG('c', 'v', '3', '4'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_35", HB_TAG('c', 'v', '3', '5'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_36", HB_TAG('c', 'v', '3', '6'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_37", HB_TAG('c', 'v', '3', '7'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_38", HB_TAG('c', 'v', '3', '8'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_39", HB_TAG('c', 'v', '3', '9'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_40", HB_TAG('c', 'v', '4', '0'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_41", HB_TAG('c', 'v', '4', '1'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_42", HB_TAG('c', 'v', '4', '2'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_43", HB_TAG('c', 'v', '4', '3'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_44", HB_TAG('c', 'v', '4', '4'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_45", HB_TAG('c', 'v', '4', '5'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_46", HB_TAG('c', 'v', '4', '6'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_47", HB_TAG('c', 'v', '4', '7'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_48", HB_TAG('c', 'v', '4', '8'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_49", HB_TAG('c', 'v', '4', '9'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_50", HB_TAG('c', 'v', '5', '0'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_51", HB_TAG('c', 'v', '5', '1'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_52", HB_TAG('c', 'v', '5', '2'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_53", HB_TAG('c', 'v', '5', '3'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_54", HB_TAG('c', 'v', '5', '4'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_55", HB_TAG('c', 'v', '5', '5'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_56", HB_TAG('c', 'v', '5', '6'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_57", HB_TAG('c', 'v', '5', '7'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_58", HB_TAG('c', 'v', '5', '8'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_59", HB_TAG('c', 'v', '5', '9'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_60", HB_TAG('c', 'v', '6', '0'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_61", HB_TAG('c', 'v', '6', '1'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_62", HB_TAG('c', 'v', '6', '2'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_63", HB_TAG('c', 'v', '6', '3'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_64", HB_TAG('c', 'v', '6', '4'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_65", HB_TAG('c', 'v', '6', '5'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_66", HB_TAG('c', 'v', '6', '6'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_67", HB_TAG('c', 'v', '6', '7'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_68", HB_TAG('c', 'v', '6', '8'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_69", HB_TAG('c', 'v', '6', '9'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_70", HB_TAG('c', 'v', '7', '0'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_71", HB_TAG('c', 'v', '7', '1'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_72", HB_TAG('c', 'v', '7', '2'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_73", HB_TAG('c', 'v', '7', '3'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_74", HB_TAG('c', 'v', '7', '4'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_75", HB_TAG('c', 'v', '7', '5'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_76", HB_TAG('c', 'v', '7', '6'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_77", HB_TAG('c', 'v', '7', '7'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_78", HB_TAG('c', 'v', '7', '8'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_79", HB_TAG('c', 'v', '7', '9'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_80", HB_TAG('c', 'v', '8', '0'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_81", HB_TAG('c', 'v', '8', '1'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_82", HB_TAG('c', 'v', '8', '2'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_83", HB_TAG('c', 'v', '8', '3'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_84", HB_TAG('c', 'v', '8', '4'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_85", HB_TAG('c', 'v', '8', '5'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_86", HB_TAG('c', 'v', '8', '6'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_87", HB_TAG('c', 'v', '8', '7'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_88", HB_TAG('c', 'v', '8', '8'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_89", HB_TAG('c', 'v', '8', '9'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_90", HB_TAG('c', 'v', '9', '0'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_91", HB_TAG('c', 'v', '9', '1'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_92", HB_TAG('c', 'v', '9', '2'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_93", HB_TAG('c', 'v', '9', '3'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_94", HB_TAG('c', 'v', '9', '4'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_95", HB_TAG('c', 'v', '9', '5'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_96", HB_TAG('c', 'v', '9', '6'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_97", HB_TAG('c', 'v', '9', '7'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_98", HB_TAG('c', 'v', '9', '8'), Variant::Type::BOOL, false);
	_insert_feature("character_variant_99", HB_TAG('c', 'v', '9', '9'), Variant::Type::BOOL, false);
	_insert_feature("petite_capitals_from_capitals", HB_TAG('c', '2', 'p', 'c'), Variant::Type::BOOL, false);
	_insert_feature("small_capitals_from_capitals", HB_TAG('c', '2', 's', 'c'), Variant::Type::BOOL, false);
	_insert_feature("distances", HB_TAG('d', 'i', 's', 't'), Variant::Type::INT, true);
	_insert_feature("discretionary_ligatures", HB_TAG('d', 'l', 'i', 'g'), Variant::Type::BOOL, false);
	_insert_feature("denominators", HB_TAG('d', 'n', 'o', 'm'), Variant::Type::BOOL, false);
	_insert_feature("dotless_forms", HB_TAG('d', 't', 'l', 's'), Variant::Type::INT, true);
	_insert_feature("expert_forms", HB_TAG('e', 'x', 'p', 't'), Variant::Type::BOOL, true);
	_insert_feature("final_glyph_on_line_alternates", HB_TAG('f', 'a', 'l', 't'), Variant::Type::INT, false);
	_insert_feature("terminal_forms_2", HB_TAG('f', 'i', 'n', '2'), Variant::Type::INT, true);
	_insert_feature("terminal_forms_3", HB_TAG('f', 'i', 'n', '3'), Variant::Type::INT, true);
	_insert_feature("terminal_forms", HB_TAG('f', 'i', 'n', 'a'), Variant::Type::INT, true);
	_insert_feature("flattened_accent_forms", HB_TAG('f', 'l', 'a', 'c'), Variant::Type::INT, true);
	_insert_feature("fractions", HB_TAG('f', 'r', 'a', 'c'), Variant::Type::BOOL, false);
	_insert_feature("full_widths", HB_TAG('f', 'w', 'i', 'd'), Variant::Type::BOOL, false);
	_insert_feature("half_forms", HB_TAG('h', 'a', 'l', 'f'), Variant::Type::INT, true);
	_insert_feature("halant_forms", HB_TAG('h', 'a', 'l', 'n'), Variant::Type::INT, true);
	_insert_feature("alternate_half_widths", HB_TAG('h', 'a', 'l', 't'), Variant::Type::BOOL, false);
	_insert_feature("historical_forms", HB_TAG('h', 'i', 's', 't'), Variant::Type::INT, false);
	_insert_feature("horizontal_kana_alternates", HB_TAG('h', 'k', 'n', 'a'), Variant::Type::BOOL, false);
	_insert_feature("historical_ligatures", HB_TAG('h', 'l', 'i', 'g'), Variant::Type::BOOL, false);
	_insert_feature("hangul", HB_TAG('h', 'n', 'g', 'l'), Variant::Type::INT, false);
	_insert_feature("hojo_kanji_forms", HB_TAG('h', 'o', 'j', 'o'), Variant::Type::INT, false);
	_insert_feature("half_widths", HB_TAG('h', 'w', 'i', 'd'), Variant::Type::BOOL, false);
	_insert_feature("initial_forms", HB_TAG('i', 'n', 'i', 't'), Variant::Type::INT, true);
	_insert_feature("isolated_forms", HB_TAG('i', 's', 'o', 'l'), Variant::Type::INT, true);
	_insert_feature("italics", HB_TAG('i', 't', 'a', 'l'), Variant::Type::INT, false);
	_insert_feature("justification_alternates", HB_TAG('j', 'a', 'l', 't'), Variant::Type::INT, false);
	_insert_feature("jis78_forms", HB_TAG('j', 'p', '7', '8'), Variant::Type::INT, false);
	_insert_feature("jis83_forms", HB_TAG('j', 'p', '8', '3'), Variant::Type::INT, false);
	_insert_feature("jis90_forms", HB_TAG('j', 'p', '9', '0'), Variant::Type::INT, false);
	_insert_feature("jis2004_forms", HB_TAG('j', 'p', '0', '4'), Variant::Type::INT, false);
	_insert_feature("kerning", HB_TAG('k', 'e', 'r', 'n'), Variant::Type::BOOL, false);
	_insert_feature("left_bounds", HB_TAG('l', 'f', 'b', 'd'), Variant::Type::INT, false);
	_insert_feature("standard_ligatures", HB_TAG('l', 'i', 'g', 'a'), Variant::Type::BOOL, false);
	_insert_feature("leading_jamo_forms", HB_TAG('l', 'j', 'm', 'o'), Variant::Type::INT, true);
	_insert_feature("lining_figures", HB_TAG('l', 'n', 'u', 'm'), Variant::Type::INT, false);
	_insert_feature("localized_forms", HB_TAG('l', 'o', 'c', 'l'), Variant::Type::INT, true);
	_insert_feature("left_to_right_alternates", HB_TAG('l', 't', 'r', 'a'), Variant::Type::INT, true);
	_insert_feature("left_to_right_mirrored_forms", HB_TAG('l', 't', 'r', 'm'), Variant::Type::INT, true);
	_insert_feature("mark_positioning", HB_TAG('m', 'a', 'r', 'k'), Variant::Type::INT, true);
	_insert_feature("medial_forms_2", HB_TAG('m', 'e', 'd', '2'), Variant::Type::INT, true);
	_insert_feature("medial_forms", HB_TAG('m', 'e', 'd', 'i'), Variant::Type::INT, true);
	_insert_feature("mathematical_greek", HB_TAG('m', 'g', 'r', 'k'), Variant::Type::BOOL, false);
	_insert_feature("mark_to_mark_positioning", HB_TAG('m', 'k', 'm', 'k'), Variant::Type::INT, true);
	_insert_feature("mark_positioning_via_substitution", HB_TAG('m', 's', 'e', 't'), Variant::Type::INT, true);
	_insert_feature("alternate_annotation_forms", HB_TAG('n', 'a', 'l', 't'), Variant::Type::INT, false);
	_insert_feature("nlc_kanji_forms", HB_TAG('n', 'l', 'c', 'k'), Variant::Type::INT, false);
	_insert_feature("nukta_forms", HB_TAG('n', 'u', 'k', 't'), Variant::Type::INT, true);
	_insert_feature("numerators", HB_TAG('n', 'u', 'm', 'r'), Variant::Type::BOOL, false);
	_insert_feature("oldstyle_figures", HB_TAG('o', 'n', 'u', 'm'), Variant::Type::INT, false);
	_insert_feature("optical_bounds", HB_TAG('o', 'p', 'b', 'd'), Variant::Type::INT, true);
	_insert_feature("ordinals", HB_TAG('o', 'r', 'd', 'n'), Variant::Type::BOOL, false);
	_insert_feature("ornaments", HB_TAG('o', 'r', 'n', 'm'), Variant::Type::INT, false);
	_insert_feature("proportional_alternate_widths", HB_TAG('p', 'a', 'l', 't'), Variant::Type::BOOL, false);
	_insert_feature("petite_capitals", HB_TAG('p', 'c', 'a', 'p'), Variant::Type::BOOL, false);
	_insert_feature("proportional_kana", HB_TAG('p', 'k', 'n', 'a'), Variant::Type::BOOL, false);
	_insert_feature("proportional_figures", HB_TAG('p', 'n', 'u', 'm'), Variant::Type::BOOL, false);
	_insert_feature("pre_base_forms", HB_TAG('p', 'r', 'e', 'f'), Variant::Type::INT, true);
	_insert_feature("pre_base_substitutions", HB_TAG('p', 'r', 'e', 's'), Variant::Type::INT, true);
	_insert_feature("post_base_forms", HB_TAG('p', 's', 't', 'f'), Variant::Type::INT, true);
	_insert_feature("post_base_substitutions", HB_TAG('p', 's', 't', 's'), Variant::Type::INT, true);
	_insert_feature("proportional_widths", HB_TAG('p', 'w', 'i', 'd'), Variant::Type::BOOL, false);
	_insert_feature("quarter_widths", HB_TAG('q', 'w', 'i', 'd'), Variant::Type::BOOL, false);
	_insert_feature("randomize", HB_TAG('r', 'a', 'n', 'd'), Variant::Type::INT, false);
	_insert_feature("required_contextual_alternates", HB_TAG('r', 'c', 'l', 't'), Variant::Type::BOOL, true);
	_insert_feature("rakar_forms", HB_TAG('r', 'k', 'r', 'f'), Variant::Type::INT, true);
	_insert_feature("required_ligatures", HB_TAG('r', 'l', 'i', 'g'), Variant::Type::BOOL, true);
	_insert_feature("reph_forms", HB_TAG('r', 'p', 'h', 'f'), Variant::Type::INT, true);
	_insert_feature("right_bounds", HB_TAG('r', 't', 'b', 'd'), Variant::Type::INT, false);
	_insert_feature("right_to_left_alternates", HB_TAG('r', 't', 'l', 'a'), Variant::Type::INT, true);
	_insert_feature("right_to_left_mirrored_forms", HB_TAG('r', 't', 'l', 'm'), Variant::Type::INT, true);
	_insert_feature("ruby_notation_forms", HB_TAG('r', 'u', 'b', 'y'), Variant::Type::INT, false);
	_insert_feature("required_variation_alternates", HB_TAG('r', 'v', 'r', 'n'), Variant::Type::INT, true);
	_insert_feature("stylistic_alternates", HB_TAG('s', 'a', 'l', 't'), Variant::Type::INT, false);
	_insert_feature("scientific_inferiors", HB_TAG('s', 'i', 'n', 'f'), Variant::Type::BOOL, false);
	_insert_feature("optical_size", HB_TAG('s', 'i', 'z', 'e'), Variant::Type::INT, false);
	_insert_feature("small_capitals", HB_TAG('s', 'm', 'c', 'p'), Variant::Type::BOOL, false);
	_insert_feature("simplified_forms", HB_TAG('s', 'm', 'p', 'l'), Variant::Type::INT, false);
	_insert_feature("stylistic_set_01", HB_TAG('s', 's', '0', '1'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_02", HB_TAG('s', 's', '0', '2'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_03", HB_TAG('s', 's', '0', '3'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_04", HB_TAG('s', 's', '0', '4'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_05", HB_TAG('s', 's', '0', '5'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_06", HB_TAG('s', 's', '0', '6'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_07", HB_TAG('s', 's', '0', '7'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_08", HB_TAG('s', 's', '0', '8'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_09", HB_TAG('s', 's', '0', '9'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_10", HB_TAG('s', 's', '1', '0'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_11", HB_TAG('s', 's', '1', '1'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_12", HB_TAG('s', 's', '1', '2'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_13", HB_TAG('s', 's', '1', '3'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_14", HB_TAG('s', 's', '1', '4'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_15", HB_TAG('s', 's', '1', '5'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_16", HB_TAG('s', 's', '1', '6'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_17", HB_TAG('s', 's', '1', '7'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_18", HB_TAG('s', 's', '1', '8'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_19", HB_TAG('s', 's', '1', '9'), Variant::Type::BOOL, false);
	_insert_feature("stylistic_set_20", HB_TAG('s', 's', '2', '0'), Variant::Type::BOOL, false);
	_insert_feature("math_script_style_alternates", HB_TAG('s', 's', 't', 'y'), Variant::Type::INT, true);
	_insert_feature("stretching_glyph_decomposition", HB_TAG('s', 't', 'c', 'h'), Variant::Type::INT, true);
	_insert_feature("subscript", HB_TAG('s', 'u', 'b', 's'), Variant::Type::BOOL, false);
	_insert_feature("superscript", HB_TAG('s', 'u', 'p', 's'), Variant::Type::BOOL, false);
	_insert_feature("swash", HB_TAG('s', 'w', 's', 'h'), Variant::Type::INT, false);
	_insert_feature("titling", HB_TAG('t', 'i', 't', 'l'), Variant::Type::BOOL, false);
	_insert_feature("trailing_jamo_forms", HB_TAG('t', 'j', 'm', 'o'), Variant::Type::INT, true);
	_insert_feature("traditional_name_forms", HB_TAG('t', 'n', 'a', 'm'), Variant::Type::INT, false);
	_insert_feature("tabular_figures", HB_TAG('t', 'n', 'u', 'm'), Variant::Type::BOOL, false);
	_insert_feature("traditional_forms", HB_TAG('t', 'r', 'a', 'd'), Variant::Type::INT, false);
	_insert_feature("third_widths", HB_TAG('t', 'w', 'i', 'd'), Variant::Type::BOOL, false);
	_insert_feature("unicase", HB_TAG('u', 'n', 'i', 'c'), Variant::Type::BOOL, false);
	_insert_feature("alternate_vertical_metrics", HB_TAG('v', 'a', 'l', 't'), Variant::Type::INT, false);
	_insert_feature("vattu_variants", HB_TAG('v', 'a', 't', 'u'), Variant::Type::INT, true);
	_insert_feature("vertical_contextual_half_width_spacing", HB_TAG('v', 'c', 'h', 'w'), Variant::Type::BOOL, false);
	_insert_feature("vertical_alternates", HB_TAG('v', 'e', 'r', 't'), Variant::Type::INT, false);
	_insert_feature("alternate_vertical_half_metrics", HB_TAG('v', 'h', 'a', 'l'), Variant::Type::BOOL, false);
	_insert_feature("vowel_jamo_forms", HB_TAG('v', 'j', 'm', 'o'), Variant::Type::INT, true);
	_insert_feature("vertical_kana_alternates", HB_TAG('v', 'k', 'n', 'a'), Variant::Type::INT, false);
	_insert_feature("vertical_kerning", HB_TAG('v', 'k', 'r', 'n'), Variant::Type::BOOL, false);
	_insert_feature("proportional_alternate_vertical_metrics", HB_TAG('v', 'p', 'a', 'l'), Variant::Type::BOOL, false);
	_insert_feature("vertical_alternates_and_rotation", HB_TAG('v', 'r', 't', '2'), Variant::Type::INT, false);
	_insert_feature("vertical_alternates_for_rotation", HB_TAG('v', 'r', 't', 'r'), Variant::Type::INT, false);
	_insert_feature("slashed_zero", HB_TAG('z', 'e', 'r', 'o'), Variant::Type::BOOL, false);

	// Registered OpenType variation tag.
	_insert_feature("italic", HB_TAG('i', 't', 'a', 'l'), Variant::Type::INT, false);
	_insert_feature("optical_size", HB_TAG('o', 'p', 's', 'z'), Variant::Type::INT, false);
	_insert_feature("slant", HB_TAG('s', 'l', 'n', 't'), Variant::Type::INT, false);
	_insert_feature("width", HB_TAG('w', 'd', 't', 'h'), Variant::Type::INT, false);
	_insert_feature("weight", HB_TAG('w', 'g', 'h', 't'), Variant::Type::INT, false);
}

int64_t TextServerAdvanced::_name_to_tag(const String &p_name) const {
	if (feature_sets.has(p_name)) {
		return feature_sets[p_name];
	}

	// No readable name, use tag string.
	return hb_tag_from_string(p_name.replace("custom_", "").ascii().get_data(), -1);
}

Variant::Type TextServerAdvanced::_get_tag_type(int64_t p_tag) const {
	if (feature_sets_inv.has(p_tag)) {
		return feature_sets_inv[p_tag].vtype;
	}
	return Variant::Type::INT;
}

bool TextServerAdvanced::_get_tag_hidden(int64_t p_tag) const {
	if (feature_sets_inv.has(p_tag)) {
		return feature_sets_inv[p_tag].hidden;
	}
	return false;
}

String TextServerAdvanced::_tag_to_name(int64_t p_tag) const {
	if (feature_sets_inv.has(p_tag)) {
		return feature_sets_inv[p_tag].name;
	}

	// No readable name, use tag string.
	char name[5];
	memset(name, 0, 5);
	hb_tag_to_string(p_tag, name);
	return String("custom_") + String(name);
}

/*************************************************************************/
/* Font Glyph Rendering                                                  */
/*************************************************************************/

_FORCE_INLINE_ TextServerAdvanced::FontTexturePosition TextServerAdvanced::find_texture_pos_for_glyph(FontForSizeAdvanced *p_data, int p_color_size, Image::Format p_image_format, int p_width, int p_height, bool p_msdf) const {
	FontTexturePosition ret;

	int mw = p_width;
	int mh = p_height;

	ShelfPackTexture *ct = p_data->textures.ptrw();
	for (int32_t i = 0; i < p_data->textures.size(); i++) {
		if (ct[i].image.is_null()) {
			continue;
		}
		if (p_image_format != ct[i].image->get_format()) {
			continue;
		}
		if (mw > ct[i].texture_w || mh > ct[i].texture_h) { // Too big for this texture.
			continue;
		}

		ret = ct[i].pack_rect(i, mh, mw);
		if (ret.index != -1) {
			break;
		}
	}

	if (ret.index == -1) {
		// Could not find texture to fit, create one.
		int texsize = MAX(p_data->size.x * p_data->oversampling * 8, 256);

		texsize = next_power_of_2(texsize);
		if (p_msdf) {
			texsize = MIN(texsize, 2048);
		} else {
			texsize = MIN(texsize, 1024);
		}
		if (mw > texsize) { // Special case, adapt to it?
			texsize = next_power_of_2(mw);
		}
		if (mh > texsize) { // Special case, adapt to it?
			texsize = next_power_of_2(mh);
		}

		ShelfPackTexture tex = ShelfPackTexture(texsize, texsize);
		tex.image = Image::create_empty(texsize, texsize, false, p_image_format);
		{
			// Zero texture.
			uint8_t *w = tex.image->ptrw();
			ERR_FAIL_COND_V(texsize * texsize * p_color_size > tex.image->get_data_size(), ret);
			// Initialize the texture to all-white pixels to prevent artifacts when the
			// font is displayed at a non-default scale with filtering enabled.
			if (p_color_size == 2) {
				for (int i = 0; i < texsize * texsize * p_color_size; i += 2) { // FORMAT_LA8, BW font.
					w[i + 0] = 255;
					w[i + 1] = 0;
				}
			} else if (p_color_size == 4) {
				for (int i = 0; i < texsize * texsize * p_color_size; i += 4) { // FORMAT_RGBA8, Color font, Multichannel(+True) SDF.
					w[i + 0] = 255;
					w[i + 1] = 255;
					w[i + 2] = 255;
					w[i + 3] = 0;
				}
			} else {
				ERR_FAIL_V(ret);
			}
		}
		p_data->textures.push_back(tex);

		int32_t idx = p_data->textures.size() - 1;
		ret = p_data->textures.write[idx].pack_rect(idx, mh, mw);
	}

	return ret;
}

#ifdef MODULE_MSDFGEN_ENABLED

struct MSContext {
	msdfgen::Point2 position;
	msdfgen::Shape *shape = nullptr;
	msdfgen::Contour *contour = nullptr;
};

class DistancePixelConversion {
	double invRange;

public:
	_FORCE_INLINE_ explicit DistancePixelConversion(double range) :
			invRange(1 / range) {}
	_FORCE_INLINE_ void operator()(float *pixels, const msdfgen::MultiAndTrueDistance &distance) const {
		pixels[0] = float(invRange * distance.r + .5);
		pixels[1] = float(invRange * distance.g + .5);
		pixels[2] = float(invRange * distance.b + .5);
		pixels[3] = float(invRange * distance.a + .5);
	}
};

struct MSDFThreadData {
	msdfgen::Bitmap<float, 4> *output;
	msdfgen::Shape *shape;
	msdfgen::Projection *projection;
	DistancePixelConversion *distancePixelConversion;
};

static msdfgen::Point2 ft_point2(const FT_Vector &vector) {
	return msdfgen::Point2(vector.x / 60.0f, vector.y / 60.0f);
}

static int ft_move_to(const FT_Vector *to, void *user) {
	MSContext *context = static_cast<MSContext *>(user);
	if (!(context->contour && context->contour->edges.empty())) {
		context->contour = &context->shape->addContour();
	}
	context->position = ft_point2(*to);
	return 0;
}

static int ft_line_to(const FT_Vector *to, void *user) {
	MSContext *context = static_cast<MSContext *>(user);
	msdfgen::Point2 endpoint = ft_point2(*to);
	if (endpoint != context->position) {
		context->contour->addEdge(new msdfgen::LinearSegment(context->position, endpoint));
		context->position = endpoint;
	}
	return 0;
}

static int ft_conic_to(const FT_Vector *control, const FT_Vector *to, void *user) {
	MSContext *context = static_cast<MSContext *>(user);
	context->contour->addEdge(new msdfgen::QuadraticSegment(context->position, ft_point2(*control), ft_point2(*to)));
	context->position = ft_point2(*to);
	return 0;
}

static int ft_cubic_to(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user) {
	MSContext *context = static_cast<MSContext *>(user);
	context->contour->addEdge(new msdfgen::CubicSegment(context->position, ft_point2(*control1), ft_point2(*control2), ft_point2(*to)));
	context->position = ft_point2(*to);
	return 0;
}

void TextServerAdvanced::_generateMTSDF_threaded(void *p_td, uint32_t p_y) {
	MSDFThreadData *td = static_cast<MSDFThreadData *>(p_td);

	msdfgen::ShapeDistanceFinder<msdfgen::OverlappingContourCombiner<msdfgen::MultiAndTrueDistanceSelector>> distanceFinder(*td->shape);
	int row = td->shape->inverseYAxis ? td->output->height() - p_y - 1 : p_y;
	for (int col = 0; col < td->output->width(); ++col) {
		int x = (p_y % 2) ? td->output->width() - col - 1 : col;
		msdfgen::Point2 p = td->projection->unproject(msdfgen::Point2(x + .5, p_y + .5));
		msdfgen::MultiAndTrueDistance distance = distanceFinder.distance(p);
		td->distancePixelConversion->operator()(td->output->operator()(x, row), distance);
	}
}

_FORCE_INLINE_ TextServerAdvanced::FontGlyph TextServerAdvanced::rasterize_msdf(FontAdvanced *p_font_data, FontForSizeAdvanced *p_data, int p_pixel_range, int p_rect_margin, FT_Outline *p_outline, const Vector2 &p_advance) const {
	msdfgen::Shape shape;

	shape.contours.clear();
	shape.inverseYAxis = false;

	MSContext context = {};
	context.shape = &shape;
	FT_Outline_Funcs ft_functions;
	ft_functions.move_to = &ft_move_to;
	ft_functions.line_to = &ft_line_to;
	ft_functions.conic_to = &ft_conic_to;
	ft_functions.cubic_to = &ft_cubic_to;
	ft_functions.shift = 0;
	ft_functions.delta = 0;

	int error = FT_Outline_Decompose(p_outline, &ft_functions, &context);
	ERR_FAIL_COND_V_MSG(error, FontGlyph(), "FreeType: Outline decomposition error: '" + String(FT_Error_String(error)) + "'.");
	if (!shape.contours.empty() && shape.contours.back().edges.empty()) {
		shape.contours.pop_back();
	}

	if (FT_Outline_Get_Orientation(p_outline) == 1) {
		for (int i = 0; i < (int)shape.contours.size(); ++i) {
			shape.contours[i].reverse();
		}
	}

	shape.inverseYAxis = true;
	shape.normalize();

	msdfgen::Shape::Bounds bounds = shape.getBounds(p_pixel_range);

	FontGlyph chr;
	chr.found = true;
	chr.advance = p_advance;

	if (shape.validate() && shape.contours.size() > 0) {
		int w = (bounds.r - bounds.l);
		int h = (bounds.t - bounds.b);

		if (w == 0 || h == 0) {
			chr.texture_idx = -1;
			chr.uv_rect = Rect2();
			chr.rect = Rect2();
			return chr;
		}

		int mw = w + p_rect_margin * 4;
		int mh = h + p_rect_margin * 4;

		ERR_FAIL_COND_V(mw > 4096, FontGlyph());
		ERR_FAIL_COND_V(mh > 4096, FontGlyph());

		FontTexturePosition tex_pos = find_texture_pos_for_glyph(p_data, 4, Image::FORMAT_RGBA8, mw, mh, true);
		ERR_FAIL_COND_V(tex_pos.index < 0, FontGlyph());
		ShelfPackTexture &tex = p_data->textures.write[tex_pos.index];

		edgeColoringSimple(shape, 3.0); // Max. angle.
		msdfgen::Bitmap<float, 4> image(w, h); // Texture size.

		DistancePixelConversion distancePixelConversion(p_pixel_range);
		msdfgen::Projection projection(msdfgen::Vector2(1.0, 1.0), msdfgen::Vector2(-bounds.l, -bounds.b));
		msdfgen::MSDFGeneratorConfig config(true, msdfgen::ErrorCorrectionConfig());

		MSDFThreadData td;
		td.output = &image;
		td.shape = &shape;
		td.projection = &projection;
		td.distancePixelConversion = &distancePixelConversion;

		WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_native_group_task(&TextServerAdvanced::_generateMTSDF_threaded, &td, h, -1, true, String("FontServerRasterizeMSDF"));
		WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);

		msdfgen::msdfErrorCorrection(image, shape, projection, p_pixel_range, config);

		{
			uint8_t *wr = tex.image->ptrw();

			for (int i = 0; i < h; i++) {
				for (int j = 0; j < w; j++) {
					int ofs = ((i + tex_pos.y + p_rect_margin * 2) * tex.texture_w + j + tex_pos.x + p_rect_margin * 2) * 4;
					ERR_FAIL_COND_V(ofs >= tex.image->get_data_size(), FontGlyph());
					wr[ofs + 0] = (uint8_t)(CLAMP(image(j, i)[0] * 256.f, 0.f, 255.f));
					wr[ofs + 1] = (uint8_t)(CLAMP(image(j, i)[1] * 256.f, 0.f, 255.f));
					wr[ofs + 2] = (uint8_t)(CLAMP(image(j, i)[2] * 256.f, 0.f, 255.f));
					wr[ofs + 3] = (uint8_t)(CLAMP(image(j, i)[3] * 256.f, 0.f, 255.f));
				}
			}
		}

		tex.dirty = true;

		chr.texture_idx = tex_pos.index;

		chr.uv_rect = Rect2(tex_pos.x + p_rect_margin, tex_pos.y + p_rect_margin, w + p_rect_margin * 2, h + p_rect_margin * 2);
		chr.rect.position = Vector2(bounds.l - p_rect_margin, -bounds.t - p_rect_margin);

		chr.rect.size = chr.uv_rect.size;
	}
	return chr;
}
#endif

#ifdef MODULE_FREETYPE_ENABLED
_FORCE_INLINE_ TextServerAdvanced::FontGlyph TextServerAdvanced::rasterize_bitmap(FontForSizeAdvanced *p_data, int p_rect_margin, FT_Bitmap p_bitmap, int p_yofs, int p_xofs, const Vector2 &p_advance, bool p_bgra) const {
	FontGlyph chr;
	chr.advance = p_advance * p_data->scale / p_data->oversampling;
	chr.found = true;

	int w = p_bitmap.width;
	int h = p_bitmap.rows;

	if (w == 0 || h == 0) {
		chr.texture_idx = -1;
		chr.uv_rect = Rect2();
		chr.rect = Rect2();
		return chr;
	}

	int color_size = 2;

	switch (p_bitmap.pixel_mode) {
		case FT_PIXEL_MODE_MONO:
		case FT_PIXEL_MODE_GRAY: {
			color_size = 2;
		} break;
		case FT_PIXEL_MODE_BGRA: {
			color_size = 4;
		} break;
		case FT_PIXEL_MODE_LCD: {
			color_size = 4;
			w /= 3;
		} break;
		case FT_PIXEL_MODE_LCD_V: {
			color_size = 4;
			h /= 3;
		} break;
	}

	int mw = w + p_rect_margin * 4;
	int mh = h + p_rect_margin * 4;

	ERR_FAIL_COND_V(mw > 4096, FontGlyph());
	ERR_FAIL_COND_V(mh > 4096, FontGlyph());

	Image::Format require_format = color_size == 4 ? Image::FORMAT_RGBA8 : Image::FORMAT_LA8;

	FontTexturePosition tex_pos = find_texture_pos_for_glyph(p_data, color_size, require_format, mw, mh, false);
	ERR_FAIL_COND_V(tex_pos.index < 0, FontGlyph());

	// Fit character in char texture.
	ShelfPackTexture &tex = p_data->textures.write[tex_pos.index];

	{
		uint8_t *wr = tex.image->ptrw();

		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				int ofs = ((i + tex_pos.y + p_rect_margin * 2) * tex.texture_w + j + tex_pos.x + p_rect_margin * 2) * color_size;
				ERR_FAIL_COND_V(ofs >= tex.image->get_data_size(), FontGlyph());
				switch (p_bitmap.pixel_mode) {
					case FT_PIXEL_MODE_MONO: {
						int byte = i * p_bitmap.pitch + (j >> 3);
						int bit = 1 << (7 - (j % 8));
						wr[ofs + 0] = 255; // grayscale as 1
						wr[ofs + 1] = (p_bitmap.buffer[byte] & bit) ? 255 : 0;
					} break;
					case FT_PIXEL_MODE_GRAY:
						wr[ofs + 0] = 255; // grayscale as 1
						wr[ofs + 1] = p_bitmap.buffer[i * p_bitmap.pitch + j];
						break;
					case FT_PIXEL_MODE_BGRA: {
						int ofs_color = i * p_bitmap.pitch + (j << 2);
						wr[ofs + 2] = p_bitmap.buffer[ofs_color + 0];
						wr[ofs + 1] = p_bitmap.buffer[ofs_color + 1];
						wr[ofs + 0] = p_bitmap.buffer[ofs_color + 2];
						wr[ofs + 3] = p_bitmap.buffer[ofs_color + 3];
					} break;
					case FT_PIXEL_MODE_LCD: {
						int ofs_color = i * p_bitmap.pitch + (j * 3);
						if (p_bgra) {
							wr[ofs + 0] = p_bitmap.buffer[ofs_color + 2];
							wr[ofs + 1] = p_bitmap.buffer[ofs_color + 1];
							wr[ofs + 2] = p_bitmap.buffer[ofs_color + 0];
							wr[ofs + 3] = 255;
						} else {
							wr[ofs + 0] = p_bitmap.buffer[ofs_color + 0];
							wr[ofs + 1] = p_bitmap.buffer[ofs_color + 1];
							wr[ofs + 2] = p_bitmap.buffer[ofs_color + 2];
							wr[ofs + 3] = 255;
						}
					} break;
					case FT_PIXEL_MODE_LCD_V: {
						int ofs_color = i * p_bitmap.pitch * 3 + j;
						if (p_bgra) {
							wr[ofs + 0] = p_bitmap.buffer[ofs_color + p_bitmap.pitch * 2];
							wr[ofs + 1] = p_bitmap.buffer[ofs_color + p_bitmap.pitch];
							wr[ofs + 2] = p_bitmap.buffer[ofs_color + 0];
							wr[ofs + 3] = 255;
						} else {
							wr[ofs + 0] = p_bitmap.buffer[ofs_color + 0];
							wr[ofs + 1] = p_bitmap.buffer[ofs_color + p_bitmap.pitch];
							wr[ofs + 2] = p_bitmap.buffer[ofs_color + p_bitmap.pitch * 2];
							wr[ofs + 3] = 255;
						}
					} break;
					default:
						ERR_FAIL_V_MSG(FontGlyph(), "Font uses unsupported pixel format: " + String::num_int64(p_bitmap.pixel_mode) + ".");
						break;
				}
			}
		}
	}

	tex.dirty = true;

	chr.texture_idx = tex_pos.index;

	chr.uv_rect = Rect2(tex_pos.x + p_rect_margin, tex_pos.y + p_rect_margin, w + p_rect_margin * 2, h + p_rect_margin * 2);
	chr.rect.position = Vector2(p_xofs - p_rect_margin, -p_yofs - p_rect_margin) * p_data->scale / p_data->oversampling;
	chr.rect.size = chr.uv_rect.size * p_data->scale / p_data->oversampling;
	return chr;
}
#endif

/*************************************************************************/
/* Font Cache                                                            */
/*************************************************************************/

_FORCE_INLINE_ bool TextServerAdvanced::_ensure_glyph(FontAdvanced *p_font_data, const Vector2i &p_size, int32_t p_glyph, FontGlyph &r_glyph) const {
	FontForSizeAdvanced *fd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(p_font_data, p_size, fd), false);

	int32_t glyph_index = p_glyph & 0xffffff; // Remove subpixel shifts.

	HashMap<int32_t, FontGlyph>::Iterator E = fd->glyph_map.find(p_glyph);
	if (E) {
		r_glyph = E->value;
		return E->value.found;
	}

	if (glyph_index == 0) { // Non graphical or invalid glyph, do not render.
		E = fd->glyph_map.insert(p_glyph, FontGlyph());
		r_glyph = E->value;
		return true;
	}

#ifdef MODULE_FREETYPE_ENABLED
	FontGlyph gl;
	if (fd->face) {
		FT_Int32 flags = FT_LOAD_DEFAULT;

		bool outline = p_size.y > 0;
		switch (p_font_data->hinting) {
			case TextServer::HINTING_NONE:
				flags |= FT_LOAD_NO_HINTING;
				break;
			case TextServer::HINTING_LIGHT:
				flags |= FT_LOAD_TARGET_LIGHT;
				break;
			default:
				flags |= FT_LOAD_TARGET_NORMAL;
				break;
		}
		if (p_font_data->force_autohinter) {
			flags |= FT_LOAD_FORCE_AUTOHINT;
		}
		if (outline || (p_font_data->disable_embedded_bitmaps && !FT_HAS_COLOR(fd->face))) {
			flags |= FT_LOAD_NO_BITMAP;
		} else if (FT_HAS_COLOR(fd->face)) {
			flags |= FT_LOAD_COLOR;
		}

		FT_Fixed v, h;
		FT_Get_Advance(fd->face, glyph_index, flags, &h);
		FT_Get_Advance(fd->face, glyph_index, flags | FT_LOAD_VERTICAL_LAYOUT, &v);

		int error = FT_Load_Glyph(fd->face, glyph_index, flags);
		if (error) {
			E = fd->glyph_map.insert(p_glyph, FontGlyph());
			r_glyph = E->value;
			return false;
		}

		if (!p_font_data->msdf) {
			if ((p_font_data->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_QUARTER) || (p_font_data->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && p_size.x <= SUBPIXEL_POSITIONING_ONE_QUARTER_MAX_SIZE)) {
				FT_Pos xshift = (int)((p_glyph >> 27) & 3) << 4;
				FT_Outline_Translate(&fd->face->glyph->outline, xshift, 0);
			} else if ((p_font_data->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_HALF) || (p_font_data->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && p_size.x <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE)) {
				FT_Pos xshift = (int)((p_glyph >> 27) & 3) << 5;
				FT_Outline_Translate(&fd->face->glyph->outline, xshift, 0);
			}
		}

		if (p_font_data->embolden != 0.f) {
			FT_Pos strength = p_font_data->embolden * p_size.x * fd->oversampling * 4; // 26.6 fractional units (1 / 64).
			FT_Outline_Embolden(&fd->face->glyph->outline, strength);
		}

		if (p_font_data->transform != Transform2D()) {
			FT_Matrix mat = { FT_Fixed(p_font_data->transform[0][0] * 65536), FT_Fixed(p_font_data->transform[0][1] * 65536), FT_Fixed(p_font_data->transform[1][0] * 65536), FT_Fixed(p_font_data->transform[1][1] * 65536) }; // 16.16 fractional units (1 / 65536).
			FT_Outline_Transform(&fd->face->glyph->outline, &mat);
		}

		FT_Render_Mode aa_mode = FT_RENDER_MODE_NORMAL;
		bool bgra = false;
		switch (p_font_data->antialiasing) {
			case FONT_ANTIALIASING_NONE: {
				aa_mode = FT_RENDER_MODE_MONO;
			} break;
			case FONT_ANTIALIASING_GRAY: {
				aa_mode = FT_RENDER_MODE_NORMAL;
			} break;
			case FONT_ANTIALIASING_LCD: {
				int aa_layout = (int)((p_glyph >> 24) & 7);
				switch (aa_layout) {
					case FONT_LCD_SUBPIXEL_LAYOUT_HRGB: {
						aa_mode = FT_RENDER_MODE_LCD;
						bgra = false;
					} break;
					case FONT_LCD_SUBPIXEL_LAYOUT_HBGR: {
						aa_mode = FT_RENDER_MODE_LCD;
						bgra = true;
					} break;
					case FONT_LCD_SUBPIXEL_LAYOUT_VRGB: {
						aa_mode = FT_RENDER_MODE_LCD_V;
						bgra = false;
					} break;
					case FONT_LCD_SUBPIXEL_LAYOUT_VBGR: {
						aa_mode = FT_RENDER_MODE_LCD_V;
						bgra = true;
					} break;
					default: {
						aa_mode = FT_RENDER_MODE_NORMAL;
					} break;
				}
			} break;
		}

		if (!outline) {
			if (!p_font_data->msdf) {
				error = FT_Render_Glyph(fd->face->glyph, aa_mode);
			}
			FT_GlyphSlot slot = fd->face->glyph;
			if (!error) {
				if (p_font_data->msdf) {
#ifdef MODULE_MSDFGEN_ENABLED
					gl = rasterize_msdf(p_font_data, fd, p_font_data->msdf_range, rect_range, &slot->outline, Vector2((h + (1 << 9)) >> 10, (v + (1 << 9)) >> 10) / 64.0);
#else
					fd->glyph_map[p_glyph] = FontGlyph();
					ERR_FAIL_V_MSG(false, "Compiled without MSDFGEN support!");
#endif
				} else {
					gl = rasterize_bitmap(fd, rect_range, slot->bitmap, slot->bitmap_top, slot->bitmap_left, Vector2((h + (1 << 9)) >> 10, (v + (1 << 9)) >> 10) / 64.0, bgra);
				}
			}
		} else {
			FT_Stroker stroker;
			if (FT_Stroker_New(ft_library, &stroker) != 0) {
				fd->glyph_map[p_glyph] = FontGlyph();
				ERR_FAIL_V_MSG(false, "FreeType: Failed to load glyph stroker.");
			}

			FT_Stroker_Set(stroker, (int)(fd->size.y * fd->oversampling * 16.0), FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 0);
			FT_Glyph glyph;
			FT_BitmapGlyph glyph_bitmap;

			if (FT_Get_Glyph(fd->face->glyph, &glyph) != 0) {
				goto cleanup_stroker;
			}
			if (FT_Glyph_Stroke(&glyph, stroker, 1) != 0) {
				goto cleanup_glyph;
			}
			if (FT_Glyph_To_Bitmap(&glyph, aa_mode, nullptr, 1) != 0) {
				goto cleanup_glyph;
			}
			glyph_bitmap = (FT_BitmapGlyph)glyph;
			gl = rasterize_bitmap(fd, rect_range, glyph_bitmap->bitmap, glyph_bitmap->top, glyph_bitmap->left, Vector2(), bgra);

		cleanup_glyph:
			FT_Done_Glyph(glyph);
		cleanup_stroker:
			FT_Stroker_Done(stroker);
		}
		E = fd->glyph_map.insert(p_glyph, gl);
		r_glyph = E->value;
		return gl.found;
	}
#endif
	E = fd->glyph_map.insert(p_glyph, FontGlyph());
	r_glyph = E->value;
	return false;
}

_FORCE_INLINE_ bool TextServerAdvanced::_ensure_cache_for_size(FontAdvanced *p_font_data, const Vector2i &p_size, FontForSizeAdvanced *&r_cache_for_size, bool p_silent) const {
	ERR_FAIL_COND_V(p_size.x <= 0, false);

	HashMap<Vector2i, FontForSizeAdvanced *>::Iterator E = p_font_data->cache.find(p_size);
	if (E) {
		r_cache_for_size = E->value;
		return true;
	}

	FontForSizeAdvanced *fd = memnew(FontForSizeAdvanced);
	fd->size = p_size;
	if (p_font_data->data_ptr && (p_font_data->data_size > 0)) {
		// Init dynamic font.
#ifdef MODULE_FREETYPE_ENABLED
		int error = 0;
		{
			MutexLock ftlock(ft_mutex);
			if (!ft_library) {
				error = FT_Init_FreeType(&ft_library);
				if (error != 0) {
					memdelete(fd);
					if (p_silent) {
						return false;
					} else {
						ERR_FAIL_V_MSG(false, "FreeType: Error initializing library: '" + String(FT_Error_String(error)) + "'.");
					}
				}
#ifdef MODULE_SVG_ENABLED
				FT_Property_Set(ft_library, "ot-svg", "svg-hooks", get_tvg_svg_in_ot_hooks());
#endif
			}

			memset(&fd->stream, 0, sizeof(FT_StreamRec));
			fd->stream.base = (unsigned char *)p_font_data->data_ptr;
			fd->stream.size = p_font_data->data_size;
			fd->stream.pos = 0;

			FT_Open_Args fargs;
			memset(&fargs, 0, sizeof(FT_Open_Args));
			fargs.memory_base = (unsigned char *)p_font_data->data_ptr;
			fargs.memory_size = p_font_data->data_size;
			fargs.flags = FT_OPEN_MEMORY;
			fargs.stream = &fd->stream;

			int max_index = 0;
			FT_Face tmp_face = nullptr;
			error = FT_Open_Face(ft_library, &fargs, -1, &tmp_face);
			if (tmp_face && error == 0) {
				max_index = tmp_face->num_faces - 1;
			}
			if (tmp_face) {
				FT_Done_Face(tmp_face);
			}

			error = FT_Open_Face(ft_library, &fargs, CLAMP(p_font_data->face_index, 0, max_index), &fd->face);
			if (error) {
				FT_Done_Face(fd->face);
				fd->face = nullptr;
				memdelete(fd);
				if (p_silent) {
					return false;
				} else {
					ERR_FAIL_V_MSG(false, "FreeType: Error loading font: '" + String(FT_Error_String(error)) + "'.");
				}
			}
		}

		if (p_font_data->msdf) {
			fd->oversampling = 1.0;
			fd->size.x = p_font_data->msdf_source_size;
		} else if (p_font_data->oversampling <= 0.0) {
			fd->oversampling = _font_get_global_oversampling();
		} else {
			fd->oversampling = p_font_data->oversampling;
		}

		if (FT_HAS_COLOR(fd->face) && fd->face->num_fixed_sizes > 0) {
			int best_match = 0;
			int diff = ABS(fd->size.x - ((int64_t)fd->face->available_sizes[0].width));
			fd->scale = double(fd->size.x * fd->oversampling) / fd->face->available_sizes[0].width;
			for (int i = 1; i < fd->face->num_fixed_sizes; i++) {
				int ndiff = ABS(fd->size.x - ((int64_t)fd->face->available_sizes[i].width));
				if (ndiff < diff) {
					best_match = i;
					diff = ndiff;
					fd->scale = double(fd->size.x * fd->oversampling) / fd->face->available_sizes[i].width;
				}
			}
			FT_Select_Size(fd->face, best_match);
		} else {
			FT_Set_Pixel_Sizes(fd->face, 0, double(fd->size.x * fd->oversampling));
			if (fd->face->size->metrics.y_ppem != 0) {
				fd->scale = ((double)fd->size.x * fd->oversampling) / (double)fd->face->size->metrics.y_ppem;
			}
		}

		fd->hb_handle = hb_ft_font_create(fd->face, nullptr);

		fd->ascent = (fd->face->size->metrics.ascender / 64.0) / fd->oversampling * fd->scale;
		fd->descent = (-fd->face->size->metrics.descender / 64.0) / fd->oversampling * fd->scale;
		fd->underline_position = (-FT_MulFix(fd->face->underline_position, fd->face->size->metrics.y_scale) / 64.0) / fd->oversampling * fd->scale;
		fd->underline_thickness = (FT_MulFix(fd->face->underline_thickness, fd->face->size->metrics.y_scale) / 64.0) / fd->oversampling * fd->scale;

#if HB_VERSION_ATLEAST(3, 3, 0)
		hb_font_set_synthetic_slant(fd->hb_handle, p_font_data->transform[0][1]);
#else
#ifndef _MSC_VER
#warning Building with HarfBuzz < 3.3.0, synthetic slant offset correction disabled.
#endif
#endif

		if (!p_font_data->face_init) {
			// When a font does not provide a `family_name`, FreeType tries to synthesize one based on other names.
			// FreeType automatically converts non-ASCII characters to "?" in the synthesized name.
			// To avoid that behavior, use the format-specific name directly if available.
			hb_face_t *hb_face = hb_font_get_face(fd->hb_handle);
			unsigned int num_entries = 0;
			const hb_ot_name_entry_t *names = hb_ot_name_list_names(hb_face, &num_entries);
			const hb_language_t english = hb_language_from_string("en", -1);
			for (unsigned int i = 0; i < num_entries; i++) {
				if (names[i].name_id != HB_OT_NAME_ID_FONT_FAMILY) {
					continue;
				}
				if (!p_font_data->font_name.is_empty() && names[i].language != english) {
					continue;
				}
				unsigned int text_size = hb_ot_name_get_utf32(hb_face, names[i].name_id, names[i].language, nullptr, nullptr) + 1;
				p_font_data->font_name.resize(text_size);
				hb_ot_name_get_utf32(hb_face, names[i].name_id, names[i].language, &text_size, (uint32_t *)p_font_data->font_name.ptrw());
			}
			if (p_font_data->font_name.is_empty() && fd->face->family_name != nullptr) {
				p_font_data->font_name = String::utf8((const char *)fd->face->family_name);
			}
			if (fd->face->style_name != nullptr) {
				p_font_data->style_name = String::utf8((const char *)fd->face->style_name);
			}
			p_font_data->weight = _font_get_weight_by_name(p_font_data->style_name.to_lower());
			p_font_data->stretch = _font_get_stretch_by_name(p_font_data->style_name.to_lower());
			p_font_data->style_flags = 0;
			if ((fd->face->style_flags & FT_STYLE_FLAG_BOLD) || p_font_data->weight >= 700) {
				p_font_data->style_flags.set_flag(FONT_BOLD);
			}
			if ((fd->face->style_flags & FT_STYLE_FLAG_ITALIC) || _is_ital_style(p_font_data->style_name.to_lower())) {
				p_font_data->style_flags.set_flag(FONT_ITALIC);
			}
			if (fd->face->face_flags & FT_FACE_FLAG_FIXED_WIDTH) {
				p_font_data->style_flags.set_flag(FONT_FIXED_WIDTH);
			}

			// Get supported scripts from OpenType font data.
			p_font_data->supported_scripts.clear();
			unsigned int count = hb_ot_layout_table_get_script_tags(hb_face, HB_OT_TAG_GSUB, 0, nullptr, nullptr);
			if (count != 0) {
				hb_tag_t *script_tags = (hb_tag_t *)memalloc(count * sizeof(hb_tag_t));
				hb_ot_layout_table_get_script_tags(hb_face, HB_OT_TAG_GSUB, 0, &count, script_tags);
				for (unsigned int i = 0; i < count; i++) {
					p_font_data->supported_scripts.insert(script_tags[i]);
				}
				memfree(script_tags);
			}
			count = hb_ot_layout_table_get_script_tags(hb_face, HB_OT_TAG_GPOS, 0, nullptr, nullptr);
			if (count != 0) {
				hb_tag_t *script_tags = (hb_tag_t *)memalloc(count * sizeof(hb_tag_t));
				hb_ot_layout_table_get_script_tags(hb_face, HB_OT_TAG_GPOS, 0, &count, script_tags);
				for (unsigned int i = 0; i < count; i++) {
					p_font_data->supported_scripts.insert(script_tags[i]);
				}
				memfree(script_tags);
			}

			// Get supported scripts from OS2 table.
			TT_OS2 *os2 = (TT_OS2 *)FT_Get_Sfnt_Table(fd->face, FT_SFNT_OS2);
			if (os2) {
				if ((os2->ulUnicodeRange1 & 1L << 4) || (os2->ulUnicodeRange1 & 1L << 5) || (os2->ulUnicodeRange1 & 1L << 6) || (os2->ulUnicodeRange1 & 1L << 31) || (os2->ulUnicodeRange2 & 1L << 0) || (os2->ulUnicodeRange2 & 1L << 1) || (os2->ulUnicodeRange2 & 1L << 2) || (os2->ulUnicodeRange2 & 1L << 3) || (os2->ulUnicodeRange2 & 1L << 4) || (os2->ulUnicodeRange2 & 1L << 5) || (os2->ulUnicodeRange2 & 1L << 6) || (os2->ulUnicodeRange2 & 1L << 7) || (os2->ulUnicodeRange2 & 1L << 8) || (os2->ulUnicodeRange2 & 1L << 9) || (os2->ulUnicodeRange2 & 1L << 10) || (os2->ulUnicodeRange2 & 1L << 11) || (os2->ulUnicodeRange2 & 1L << 12) || (os2->ulUnicodeRange2 & 1L << 13) || (os2->ulUnicodeRange2 & 1L << 14) || (os2->ulUnicodeRange2 & 1L << 15) || (os2->ulUnicodeRange2 & 1L << 30) || (os2->ulUnicodeRange3 & 1L << 0) || (os2->ulUnicodeRange3 & 1L << 1) || (os2->ulUnicodeRange3 & 1L << 2) || (os2->ulUnicodeRange3 & 1L << 4) || (os2->ulUnicodeRange3 & 1L << 5) || (os2->ulUnicodeRange3 & 1L << 18) || (os2->ulUnicodeRange3 & 1L << 24) || (os2->ulUnicodeRange3 & 1L << 25) || (os2->ulUnicodeRange3 & 1L << 26) || (os2->ulUnicodeRange3 & 1L << 27) || (os2->ulUnicodeRange3 & 1L << 28) || (os2->ulUnicodeRange4 & 1L << 3) || (os2->ulUnicodeRange4 & 1L << 6) || (os2->ulUnicodeRange4 & 1L << 15) || (os2->ulUnicodeRange4 & 1L << 23) || (os2->ulUnicodeRange4 & 1L << 24) || (os2->ulUnicodeRange4 & 1L << 26)) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_COMMON);
				}
				if ((os2->ulUnicodeRange1 & 1L << 0) || (os2->ulUnicodeRange1 & 1L << 1) || (os2->ulUnicodeRange1 & 1L << 2) || (os2->ulUnicodeRange1 & 1L << 3) || (os2->ulUnicodeRange1 & 1L << 29)) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_LATIN);
				}
				if ((os2->ulUnicodeRange1 & 1L << 7) || (os2->ulUnicodeRange1 & 1L << 30)) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_GREEK);
				}
				if (os2->ulUnicodeRange1 & 1L << 8) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_COPTIC);
				}
				if (os2->ulUnicodeRange1 & 1L << 9) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_CYRILLIC);
				}
				if (os2->ulUnicodeRange1 & 1L << 10) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_ARMENIAN);
				}
				if (os2->ulUnicodeRange1 & 1L << 11) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_HEBREW);
				}
				if (os2->ulUnicodeRange1 & 1L << 12) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_VAI);
				}
				if ((os2->ulUnicodeRange1 & 1L << 13) || (os2->ulUnicodeRange2 & 1L << 31) || (os2->ulUnicodeRange3 & 1L << 3)) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_ARABIC);
				}
				if (os2->ulUnicodeRange1 & 1L << 14) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_NKO);
				}
				if (os2->ulUnicodeRange1 & 1L << 15) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_DEVANAGARI);
				}
				if (os2->ulUnicodeRange1 & 1L << 16) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_BENGALI);
				}
				if (os2->ulUnicodeRange1 & 1L << 17) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_GURMUKHI);
				}
				if (os2->ulUnicodeRange1 & 1L << 18) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_GUJARATI);
				}
				if (os2->ulUnicodeRange1 & 1L << 19) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_ORIYA);
				}
				if (os2->ulUnicodeRange1 & 1L << 20) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_TAMIL);
				}
				if (os2->ulUnicodeRange1 & 1L << 21) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_TELUGU);
				}
				if (os2->ulUnicodeRange1 & 1L << 22) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_KANNADA);
				}
				if (os2->ulUnicodeRange1 & 1L << 23) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_MALAYALAM);
				}
				if (os2->ulUnicodeRange1 & 1L << 24) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_THAI);
				}
				if (os2->ulUnicodeRange1 & 1L << 25) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_LAO);
				}
				if (os2->ulUnicodeRange1 & 1L << 26) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_GEORGIAN);
				}
				if (os2->ulUnicodeRange1 & 1L << 27) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_BALINESE);
				}
				if ((os2->ulUnicodeRange1 & 1L << 28) || (os2->ulUnicodeRange2 & 1L << 20) || (os2->ulUnicodeRange2 & 1L << 24)) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_HANGUL);
				}
				if ((os2->ulUnicodeRange2 & 1L << 21) || (os2->ulUnicodeRange2 & 1L << 22) || (os2->ulUnicodeRange2 & 1L << 23) || (os2->ulUnicodeRange2 & 1L << 26) || (os2->ulUnicodeRange2 & 1L << 27) || (os2->ulUnicodeRange2 & 1L << 29)) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_HAN);
				}
				if (os2->ulUnicodeRange2 & 1L << 17) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_HIRAGANA);
				}
				if (os2->ulUnicodeRange2 & 1L << 18) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_KATAKANA);
				}
				if (os2->ulUnicodeRange2 & 1L << 19) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_BOPOMOFO);
				}
				if (os2->ulUnicodeRange3 & 1L << 6) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_TIBETAN);
				}
				if (os2->ulUnicodeRange3 & 1L << 7) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_SYRIAC);
				}
				if (os2->ulUnicodeRange3 & 1L << 8) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_THAANA);
				}
				if (os2->ulUnicodeRange3 & 1L << 9) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_SINHALA);
				}
				if (os2->ulUnicodeRange3 & 1L << 10) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_MYANMAR);
				}
				if (os2->ulUnicodeRange3 & 1L << 11) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_ETHIOPIC);
				}
				if (os2->ulUnicodeRange3 & 1L << 12) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_CHEROKEE);
				}
				if (os2->ulUnicodeRange3 & 1L << 13) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_CANADIAN_SYLLABICS);
				}
				if (os2->ulUnicodeRange3 & 1L << 14) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_OGHAM);
				}
				if (os2->ulUnicodeRange3 & 1L << 15) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_RUNIC);
				}
				if (os2->ulUnicodeRange3 & 1L << 16) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_KHMER);
				}
				if (os2->ulUnicodeRange3 & 1L << 17) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_MONGOLIAN);
				}
				if (os2->ulUnicodeRange3 & 1L << 19) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_YI);
				}
				if (os2->ulUnicodeRange3 & 1L << 20) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_HANUNOO);
					p_font_data->supported_scripts.insert(HB_SCRIPT_TAGBANWA);
					p_font_data->supported_scripts.insert(HB_SCRIPT_BUHID);
					p_font_data->supported_scripts.insert(HB_SCRIPT_TAGALOG);
				}
				if (os2->ulUnicodeRange3 & 1L << 21) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_OLD_ITALIC);
				}
				if (os2->ulUnicodeRange3 & 1L << 22) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_GOTHIC);
				}
				if (os2->ulUnicodeRange3 & 1L << 23) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_DESERET);
				}
				if (os2->ulUnicodeRange3 & 1L << 29) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_LIMBU);
				}
				if (os2->ulUnicodeRange3 & 1L << 30) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_TAI_LE);
				}
				if (os2->ulUnicodeRange3 & 1L << 31) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_NEW_TAI_LUE);
				}
				if (os2->ulUnicodeRange4 & 1L << 0) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_BUGINESE);
				}
				if (os2->ulUnicodeRange4 & 1L << 1) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_GLAGOLITIC);
				}
				if (os2->ulUnicodeRange4 & 1L << 2) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_TIFINAGH);
				}
				if (os2->ulUnicodeRange4 & 1L << 4) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_SYLOTI_NAGRI);
				}
				if (os2->ulUnicodeRange4 & 1L << 5) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_LINEAR_B);
				}
				if (os2->ulUnicodeRange4 & 1L << 7) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_UGARITIC);
				}
				if (os2->ulUnicodeRange4 & 1L << 8) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_OLD_PERSIAN);
				}
				if (os2->ulUnicodeRange4 & 1L << 9) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_SHAVIAN);
				}
				if (os2->ulUnicodeRange4 & 1L << 10) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_OSMANYA);
				}
				if (os2->ulUnicodeRange4 & 1L << 11) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_CYPRIOT);
				}
				if (os2->ulUnicodeRange4 & 1L << 12) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_KHAROSHTHI);
				}
				if (os2->ulUnicodeRange4 & 1L << 13) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_TAI_VIET);
				}
				if (os2->ulUnicodeRange4 & 1L << 14) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_CUNEIFORM);
				}
				if (os2->ulUnicodeRange4 & 1L << 16) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_SUNDANESE);
				}
				if (os2->ulUnicodeRange4 & 1L << 17) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_LEPCHA);
				}
				if (os2->ulUnicodeRange4 & 1L << 18) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_OL_CHIKI);
				}
				if (os2->ulUnicodeRange4 & 1L << 19) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_SAURASHTRA);
				}
				if (os2->ulUnicodeRange4 & 1L << 20) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_KAYAH_LI);
				}
				if (os2->ulUnicodeRange4 & 1L << 21) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_REJANG);
				}
				if (os2->ulUnicodeRange4 & 1L << 22) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_CHAM);
				}
				if (os2->ulUnicodeRange4 & 1L << 25) {
					p_font_data->supported_scripts.insert(HB_SCRIPT_ANATOLIAN_HIEROGLYPHS);
				}
			}

			// Read OpenType feature tags.
			p_font_data->supported_features.clear();
			count = hb_ot_layout_table_get_feature_tags(hb_face, HB_OT_TAG_GSUB, 0, nullptr, nullptr);
			if (count != 0) {
				hb_tag_t *feature_tags = (hb_tag_t *)memalloc(count * sizeof(hb_tag_t));
				hb_ot_layout_table_get_feature_tags(hb_face, HB_OT_TAG_GSUB, 0, &count, feature_tags);
				for (unsigned int i = 0; i < count; i++) {
					Dictionary ftr;

#if HB_VERSION_ATLEAST(2, 1, 0)
					hb_ot_name_id_t lbl_id;
					if (hb_ot_layout_feature_get_name_ids(hb_face, HB_OT_TAG_GSUB, i, &lbl_id, nullptr, nullptr, nullptr, nullptr)) {
						PackedInt32Array lbl;
						unsigned int text_size = hb_ot_name_get_utf32(hb_face, lbl_id, hb_language_from_string(TranslationServer::get_singleton()->get_tool_locale().ascii().get_data(), -1), nullptr, nullptr) + 1;
						lbl.resize(text_size);
						memset((uint32_t *)lbl.ptrw(), 0, sizeof(uint32_t) * text_size);
						hb_ot_name_get_utf32(hb_face, lbl_id, hb_language_from_string(TranslationServer::get_singleton()->get_tool_locale().ascii().get_data(), -1), &text_size, (uint32_t *)lbl.ptrw());
						ftr["label"] = String((const char32_t *)lbl.ptr());
					}
#else
#ifndef _MSC_VER
#warning Building with HarfBuzz < 2.1.0, readable OpenType feature names disabled.
#endif
#endif
					ftr["type"] = _get_tag_type(feature_tags[i]);
					ftr["hidden"] = _get_tag_hidden(feature_tags[i]);

					p_font_data->supported_features[feature_tags[i]] = ftr;
				}
				memfree(feature_tags);
			}
			count = hb_ot_layout_table_get_feature_tags(hb_face, HB_OT_TAG_GPOS, 0, nullptr, nullptr);
			if (count != 0) {
				hb_tag_t *feature_tags = (hb_tag_t *)memalloc(count * sizeof(hb_tag_t));
				hb_ot_layout_table_get_feature_tags(hb_face, HB_OT_TAG_GPOS, 0, &count, feature_tags);
				for (unsigned int i = 0; i < count; i++) {
					Dictionary ftr;

#if HB_VERSION_ATLEAST(2, 1, 0)
					hb_ot_name_id_t lbl_id;
					if (hb_ot_layout_feature_get_name_ids(hb_face, HB_OT_TAG_GPOS, i, &lbl_id, nullptr, nullptr, nullptr, nullptr)) {
						PackedInt32Array lbl;
						unsigned int text_size = hb_ot_name_get_utf32(hb_face, lbl_id, hb_language_from_string(TranslationServer::get_singleton()->get_tool_locale().ascii().get_data(), -1), nullptr, nullptr) + 1;
						lbl.resize(text_size);
						memset((uint32_t *)lbl.ptrw(), 0, sizeof(uint32_t) * text_size);
						hb_ot_name_get_utf32(hb_face, lbl_id, hb_language_from_string(TranslationServer::get_singleton()->get_tool_locale().ascii().get_data(), -1), &text_size, (uint32_t *)lbl.ptrw());
						ftr["label"] = String((const char32_t *)lbl.ptr());
					}
#else
#ifndef _MSC_VER
#warning Building with HarfBuzz < 2.1.0, readable OpenType feature names disabled.
#endif
#endif
					ftr["type"] = _get_tag_type(feature_tags[i]);
					ftr["hidden"] = _get_tag_hidden(feature_tags[i]);

					p_font_data->supported_features[feature_tags[i]] = ftr;
				}
				memfree(feature_tags);
			}

			// Read OpenType variations.
			p_font_data->supported_varaitions.clear();
			if (fd->face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS) {
				FT_MM_Var *amaster;
				FT_Get_MM_Var(fd->face, &amaster);
				for (FT_UInt i = 0; i < amaster->num_axis; i++) {
					p_font_data->supported_varaitions[(int32_t)amaster->axis[i].tag] = Vector3i(amaster->axis[i].minimum / 65536, amaster->axis[i].maximum / 65536, amaster->axis[i].def / 65536);
				}
				FT_Done_MM_Var(ft_library, amaster);
			}
			p_font_data->face_init = true;
		}

		// Write variations.
		if (fd->face->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS) {
			FT_MM_Var *amaster;

			FT_Get_MM_Var(fd->face, &amaster);

			Vector<hb_variation_t> hb_vars;
			Vector<FT_Fixed> coords;
			coords.resize(amaster->num_axis);

			FT_Get_Var_Design_Coordinates(fd->face, coords.size(), coords.ptrw());

			for (FT_UInt i = 0; i < amaster->num_axis; i++) {
				hb_variation_t var;

				// Reset to default.
				var.tag = amaster->axis[i].tag;
				var.value = (double)amaster->axis[i].def / 65536.0;
				coords.write[i] = amaster->axis[i].def;

				if (p_font_data->variation_coordinates.has(var.tag)) {
					var.value = p_font_data->variation_coordinates[var.tag];
					coords.write[i] = CLAMP(var.value * 65536.0, amaster->axis[i].minimum, amaster->axis[i].maximum);
				}

				if (p_font_data->variation_coordinates.has(_tag_to_name(var.tag))) {
					var.value = p_font_data->variation_coordinates[_tag_to_name(var.tag)];
					coords.write[i] = CLAMP(var.value * 65536.0, amaster->axis[i].minimum, amaster->axis[i].maximum);
				}

				hb_vars.push_back(var);
			}

			FT_Set_Var_Design_Coordinates(fd->face, coords.size(), coords.ptrw());
			hb_font_set_variations(fd->hb_handle, hb_vars.is_empty() ? nullptr : &hb_vars[0], hb_vars.size());
			FT_Done_MM_Var(ft_library, amaster);
		}
#else
		memdelete(fd);
		if (p_silent) {
			return false;
		} else {
			ERR_FAIL_V_MSG(false, "FreeType: Can't load dynamic font, engine is compiled without FreeType support!");
		}
#endif
	} else {
		// Init bitmap font.
		fd->hb_handle = _bmp_font_create(fd, nullptr);
	}
	p_font_data->cache.insert(p_size, fd);
	r_cache_for_size = fd;
	return true;
}

_FORCE_INLINE_ bool TextServerAdvanced::_font_validate(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	return _ensure_cache_for_size(fd, size, ffsd, true);
}

_FORCE_INLINE_ void TextServerAdvanced::_font_clear_cache(FontAdvanced *p_font_data) {
	MutexLock ftlock(ft_mutex);

	for (const KeyValue<Vector2i, FontForSizeAdvanced *> &E : p_font_data->cache) {
		memdelete(E.value);
	}
	p_font_data->cache.clear();
	p_font_data->face_init = false;
	p_font_data->supported_features.clear();
	p_font_data->supported_varaitions.clear();
	p_font_data->supported_scripts.clear();
}

hb_font_t *TextServerAdvanced::_font_get_hb_handle(const RID &p_font_rid, int64_t p_size) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, nullptr);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), nullptr);

	return ffsd->hb_handle;
}

RID TextServerAdvanced::_create_font() {
	_THREAD_SAFE_METHOD_

	FontAdvanced *fd = memnew(FontAdvanced);

	return font_owner.make_rid(fd);
}

RID TextServerAdvanced::_create_font_linked_variation(const RID &p_font_rid) {
	_THREAD_SAFE_METHOD_

	RID rid = p_font_rid;
	FontAdvancedLinkedVariation *fdv = font_var_owner.get_or_null(rid);
	if (unlikely(fdv)) {
		rid = fdv->base_font;
	}
	ERR_FAIL_COND_V(!font_owner.owns(rid), RID());

	FontAdvancedLinkedVariation *new_fdv = memnew(FontAdvancedLinkedVariation);
	new_fdv->base_font = rid;

	return font_var_owner.make_rid(new_fdv);
}

void TextServerAdvanced::_font_set_data(const RID &p_font_rid, const PackedByteArray &p_data) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	_font_clear_cache(fd);
	fd->data = p_data;
	fd->data_ptr = fd->data.ptr();
	fd->data_size = fd->data.size();
}

void TextServerAdvanced::_font_set_data_ptr(const RID &p_font_rid, const uint8_t *p_data_ptr, int64_t p_data_size) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	_font_clear_cache(fd);
	fd->data.resize(0);
	fd->data_ptr = p_data_ptr;
	fd->data_size = p_data_size;
}

void TextServerAdvanced::_font_set_face_index(const RID &p_font_rid, int64_t p_face_index) {
	ERR_FAIL_COND(p_face_index < 0);
	ERR_FAIL_COND(p_face_index >= 0x7FFF);

	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->face_index != p_face_index) {
		fd->face_index = p_face_index;
		_font_clear_cache(fd);
	}
}

int64_t TextServerAdvanced::_font_get_face_index(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	return fd->face_index;
}

int64_t TextServerAdvanced::_font_get_face_count(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	int face_count = 0;

	if (fd->data_ptr && (fd->data_size > 0)) {
		// Init dynamic font.
#ifdef MODULE_FREETYPE_ENABLED
		int error = 0;
		if (!ft_library) {
			error = FT_Init_FreeType(&ft_library);
			ERR_FAIL_COND_V_MSG(error != 0, false, "FreeType: Error initializing library: '" + String(FT_Error_String(error)) + "'.");
#ifdef MODULE_SVG_ENABLED
			FT_Property_Set(ft_library, "ot-svg", "svg-hooks", get_tvg_svg_in_ot_hooks());
#endif
		}

		FT_StreamRec stream;
		memset(&stream, 0, sizeof(FT_StreamRec));
		stream.base = (unsigned char *)fd->data_ptr;
		stream.size = fd->data_size;
		stream.pos = 0;

		FT_Open_Args fargs;
		memset(&fargs, 0, sizeof(FT_Open_Args));
		fargs.memory_base = (unsigned char *)fd->data_ptr;
		fargs.memory_size = fd->data_size;
		fargs.flags = FT_OPEN_MEMORY;
		fargs.stream = &stream;

		MutexLock ftlock(ft_mutex);

		FT_Face tmp_face = nullptr;
		error = FT_Open_Face(ft_library, &fargs, -1, &tmp_face);
		if (error == 0) {
			face_count = tmp_face->num_faces;
			FT_Done_Face(tmp_face);
		}
#endif
	}

	return face_count;
}

void TextServerAdvanced::_font_set_style(const RID &p_font_rid, BitField<FontStyle> p_style) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->style_flags = p_style;
}

BitField<TextServer::FontStyle> TextServerAdvanced::_font_get_style(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0);
	return fd->style_flags;
}

void TextServerAdvanced::_font_set_style_name(const RID &p_font_rid, const String &p_name) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->style_name = p_name;
}

String TextServerAdvanced::_font_get_style_name(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, String());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), String());
	return fd->style_name;
}

void TextServerAdvanced::_font_set_weight(const RID &p_font_rid, int64_t p_weight) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->weight = CLAMP(p_weight, 100, 999);
}

int64_t TextServerAdvanced::_font_get_weight(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 400);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 400);
	return fd->weight;
}

void TextServerAdvanced::_font_set_stretch(const RID &p_font_rid, int64_t p_stretch) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->stretch = CLAMP(p_stretch, 50, 200);
}

int64_t TextServerAdvanced::_font_get_stretch(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 100);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 100);
	return fd->stretch;
}

void TextServerAdvanced::_font_set_name(const RID &p_font_rid, const String &p_name) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->font_name = p_name;
}

String TextServerAdvanced::_font_get_name(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, String());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), String());
	return fd->font_name;
}

Dictionary TextServerAdvanced::_font_get_ot_name_strings(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Dictionary());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Dictionary());

	hb_face_t *hb_face = hb_font_get_face(ffsd->hb_handle);

	unsigned int num_entries = 0;
	const hb_ot_name_entry_t *names = hb_ot_name_list_names(hb_face, &num_entries);
	HashMap<String, Dictionary> names_for_lang;
	for (unsigned int i = 0; i < num_entries; i++) {
		String name;
		switch (names[i].name_id) {
			case HB_OT_NAME_ID_COPYRIGHT: {
				name = "copyright";
			} break;
			case HB_OT_NAME_ID_FONT_FAMILY: {
				name = "family_name";
			} break;
			case HB_OT_NAME_ID_FONT_SUBFAMILY: {
				name = "subfamily_name";
			} break;
			case HB_OT_NAME_ID_UNIQUE_ID: {
				name = "unique_identifier";
			} break;
			case HB_OT_NAME_ID_FULL_NAME: {
				name = "full_name";
			} break;
			case HB_OT_NAME_ID_VERSION_STRING: {
				name = "version";
			} break;
			case HB_OT_NAME_ID_POSTSCRIPT_NAME: {
				name = "postscript_name";
			} break;
			case HB_OT_NAME_ID_TRADEMARK: {
				name = "trademark";
			} break;
			case HB_OT_NAME_ID_MANUFACTURER: {
				name = "manufacturer";
			} break;
			case HB_OT_NAME_ID_DESIGNER: {
				name = "designer";
			} break;
			case HB_OT_NAME_ID_DESCRIPTION: {
				name = "description";
			} break;
			case HB_OT_NAME_ID_VENDOR_URL: {
				name = "vendor_url";
			} break;
			case HB_OT_NAME_ID_DESIGNER_URL: {
				name = "designer_url";
			} break;
			case HB_OT_NAME_ID_LICENSE: {
				name = "license";
			} break;
			case HB_OT_NAME_ID_LICENSE_URL: {
				name = "license_url";
			} break;
			case HB_OT_NAME_ID_TYPOGRAPHIC_FAMILY: {
				name = "typographic_family_name";
			} break;
			case HB_OT_NAME_ID_TYPOGRAPHIC_SUBFAMILY: {
				name = "typographic_subfamily_name";
			} break;
			case HB_OT_NAME_ID_MAC_FULL_NAME: {
				name = "full_name_macos";
			} break;
			case HB_OT_NAME_ID_SAMPLE_TEXT: {
				name = "sample_text";
			} break;
			case HB_OT_NAME_ID_CID_FINDFONT_NAME: {
				name = "cid_findfont_name";
			} break;
			case HB_OT_NAME_ID_WWS_FAMILY: {
				name = "weight_width_slope_family_name";
			} break;
			case HB_OT_NAME_ID_WWS_SUBFAMILY: {
				name = "weight_width_slope_subfamily_name";
			} break;
			case HB_OT_NAME_ID_LIGHT_BACKGROUND: {
				name = "light_background_palette";
			} break;
			case HB_OT_NAME_ID_DARK_BACKGROUND: {
				name = "dark_background_palette";
			} break;
			case HB_OT_NAME_ID_VARIATIONS_PS_PREFIX: {
				name = "postscript_name_prefix";
			} break;
			default: {
				name = vformat("unknown_%d", names[i].name_id);
			} break;
		}
		String text;
		unsigned int text_size = hb_ot_name_get_utf32(hb_face, names[i].name_id, names[i].language, nullptr, nullptr) + 1;
		text.resize(text_size);
		hb_ot_name_get_utf32(hb_face, names[i].name_id, names[i].language, &text_size, (uint32_t *)text.ptrw());
		if (!text.is_empty()) {
			Dictionary &id_string = names_for_lang[String(hb_language_to_string(names[i].language))];
			id_string[name] = text;
		}
	}

	Dictionary out;
	for (const KeyValue<String, Dictionary> &E : names_for_lang) {
		out[E.key] = E.value;
	}

	return out;
}

void TextServerAdvanced::_font_set_antialiasing(const RID &p_font_rid, TextServer::FontAntialiasing p_antialiasing) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->antialiasing != p_antialiasing) {
		_font_clear_cache(fd);
		fd->antialiasing = p_antialiasing;
	}
}

TextServer::FontAntialiasing TextServerAdvanced::_font_get_antialiasing(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, TextServer::FONT_ANTIALIASING_NONE);

	MutexLock lock(fd->mutex);
	return fd->antialiasing;
}

void TextServerAdvanced::_font_set_disable_embedded_bitmaps(const RID &p_font_rid, bool p_disable_embedded_bitmaps) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->disable_embedded_bitmaps != p_disable_embedded_bitmaps) {
		_font_clear_cache(fd);
		fd->disable_embedded_bitmaps = p_disable_embedded_bitmaps;
	}
}

bool TextServerAdvanced::_font_get_disable_embedded_bitmaps(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->disable_embedded_bitmaps;
}

void TextServerAdvanced::_font_set_generate_mipmaps(const RID &p_font_rid, bool p_generate_mipmaps) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->mipmaps != p_generate_mipmaps) {
		for (KeyValue<Vector2i, FontForSizeAdvanced *> &E : fd->cache) {
			for (int i = 0; i < E.value->textures.size(); i++) {
				E.value->textures.write[i].dirty = true;
				E.value->textures.write[i].texture = Ref<ImageTexture>();
			}
		}
		fd->mipmaps = p_generate_mipmaps;
	}
}

bool TextServerAdvanced::_font_get_generate_mipmaps(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->mipmaps;
}

void TextServerAdvanced::_font_set_multichannel_signed_distance_field(const RID &p_font_rid, bool p_msdf) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->msdf != p_msdf) {
		_font_clear_cache(fd);
		fd->msdf = p_msdf;
	}
}

bool TextServerAdvanced::_font_is_multichannel_signed_distance_field(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->msdf;
}

void TextServerAdvanced::_font_set_msdf_pixel_range(const RID &p_font_rid, int64_t p_msdf_pixel_range) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->msdf_range != p_msdf_pixel_range) {
		_font_clear_cache(fd);
		fd->msdf_range = p_msdf_pixel_range;
	}
}

int64_t TextServerAdvanced::_font_get_msdf_pixel_range(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->msdf_range;
}

void TextServerAdvanced::_font_set_msdf_size(const RID &p_font_rid, int64_t p_msdf_size) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->msdf_source_size != p_msdf_size) {
		_font_clear_cache(fd);
		fd->msdf_source_size = p_msdf_size;
	}
}

int64_t TextServerAdvanced::_font_get_msdf_size(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	return fd->msdf_source_size;
}

void TextServerAdvanced::_font_set_fixed_size(const RID &p_font_rid, int64_t p_fixed_size) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->fixed_size = p_fixed_size;
}

int64_t TextServerAdvanced::_font_get_fixed_size(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	return fd->fixed_size;
}

void TextServerAdvanced::_font_set_fixed_size_scale_mode(const RID &p_font_rid, TextServer::FixedSizeScaleMode p_fixed_size_scale_mode) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->fixed_size_scale_mode = p_fixed_size_scale_mode;
}

TextServer::FixedSizeScaleMode TextServerAdvanced::_font_get_fixed_size_scale_mode(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, FIXED_SIZE_SCALE_DISABLE);

	MutexLock lock(fd->mutex);
	return fd->fixed_size_scale_mode;
}

void TextServerAdvanced::_font_set_allow_system_fallback(const RID &p_font_rid, bool p_allow_system_fallback) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->allow_system_fallback = p_allow_system_fallback;
}

bool TextServerAdvanced::_font_is_allow_system_fallback(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->allow_system_fallback;
}

void TextServerAdvanced::_font_set_force_autohinter(const RID &p_font_rid, bool p_force_autohinter) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->force_autohinter != p_force_autohinter) {
		_font_clear_cache(fd);
		fd->force_autohinter = p_force_autohinter;
	}
}

bool TextServerAdvanced::_font_is_force_autohinter(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->force_autohinter;
}

void TextServerAdvanced::_font_set_hinting(const RID &p_font_rid, TextServer::Hinting p_hinting) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->hinting != p_hinting) {
		_font_clear_cache(fd);
		fd->hinting = p_hinting;
	}
}

TextServer::Hinting TextServerAdvanced::_font_get_hinting(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, HINTING_NONE);

	MutexLock lock(fd->mutex);
	return fd->hinting;
}

void TextServerAdvanced::_font_set_subpixel_positioning(const RID &p_font_rid, TextServer::SubpixelPositioning p_subpixel) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->subpixel_positioning = p_subpixel;
}

TextServer::SubpixelPositioning TextServerAdvanced::_font_get_subpixel_positioning(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, SUBPIXEL_POSITIONING_DISABLED);

	MutexLock lock(fd->mutex);
	return fd->subpixel_positioning;
}

void TextServerAdvanced::_font_set_keep_rounding_remainders(const RID &p_font_rid, bool p_keep_rounding_remainders) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->keep_rounding_remainders = p_keep_rounding_remainders;
}

bool TextServerAdvanced::_font_get_keep_rounding_remainders(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->keep_rounding_remainders;
}

void TextServerAdvanced::_font_set_embolden(const RID &p_font_rid, double p_strength) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->embolden != p_strength) {
		_font_clear_cache(fd);
		fd->embolden = p_strength;
	}
}

double TextServerAdvanced::_font_get_embolden(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	return fd->embolden;
}

void TextServerAdvanced::_font_set_spacing(const RID &p_font_rid, SpacingType p_spacing, int64_t p_value) {
	ERR_FAIL_INDEX((int)p_spacing, 4);
	FontAdvancedLinkedVariation *fdv = font_var_owner.get_or_null(p_font_rid);
	if (fdv) {
		if (fdv->extra_spacing[p_spacing] != p_value) {
			fdv->extra_spacing[p_spacing] = p_value;
		}
	} else {
		FontAdvanced *fd = font_owner.get_or_null(p_font_rid);
		ERR_FAIL_NULL(fd);

		MutexLock lock(fd->mutex);
		if (fd->extra_spacing[p_spacing] != p_value) {
			fd->extra_spacing[p_spacing] = p_value;
		}
	}
}

int64_t TextServerAdvanced::_font_get_spacing(const RID &p_font_rid, SpacingType p_spacing) const {
	ERR_FAIL_INDEX_V((int)p_spacing, 4, 0);
	FontAdvancedLinkedVariation *fdv = font_var_owner.get_or_null(p_font_rid);
	if (fdv) {
		return fdv->extra_spacing[p_spacing];
	} else {
		FontAdvanced *fd = font_owner.get_or_null(p_font_rid);
		ERR_FAIL_NULL_V(fd, 0);

		MutexLock lock(fd->mutex);
		return fd->extra_spacing[p_spacing];
	}
}

void TextServerAdvanced::_font_set_baseline_offset(const RID &p_font_rid, double p_baseline_offset) {
	FontAdvancedLinkedVariation *fdv = font_var_owner.get_or_null(p_font_rid);
	if (fdv) {
		if (fdv->baseline_offset != p_baseline_offset) {
			fdv->baseline_offset = p_baseline_offset;
		}
	} else {
		FontAdvanced *fd = font_owner.get_or_null(p_font_rid);
		ERR_FAIL_NULL(fd);

		MutexLock lock(fd->mutex);
		if (fd->baseline_offset != p_baseline_offset) {
			_font_clear_cache(fd);
			fd->baseline_offset = p_baseline_offset;
		}
	}
}

double TextServerAdvanced::_font_get_baseline_offset(const RID &p_font_rid) const {
	FontAdvancedLinkedVariation *fdv = font_var_owner.get_or_null(p_font_rid);
	if (fdv) {
		return fdv->baseline_offset;
	} else {
		FontAdvanced *fd = font_owner.get_or_null(p_font_rid);
		ERR_FAIL_NULL_V(fd, 0.0);

		MutexLock lock(fd->mutex);
		return fd->baseline_offset;
	}
}

void TextServerAdvanced::_font_set_transform(const RID &p_font_rid, const Transform2D &p_transform) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->transform != p_transform) {
		_font_clear_cache(fd);
		fd->transform = p_transform;
	}
}

Transform2D TextServerAdvanced::_font_get_transform(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Transform2D());

	MutexLock lock(fd->mutex);
	return fd->transform;
}

void TextServerAdvanced::_font_set_variation_coordinates(const RID &p_font_rid, const Dictionary &p_variation_coordinates) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (!fd->variation_coordinates.recursive_equal(p_variation_coordinates, 1)) {
		_font_clear_cache(fd);
		fd->variation_coordinates = p_variation_coordinates.duplicate();
	}
}

Dictionary TextServerAdvanced::_font_get_variation_coordinates(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Dictionary());

	MutexLock lock(fd->mutex);
	return fd->variation_coordinates;
}

void TextServerAdvanced::_font_set_oversampling(const RID &p_font_rid, double p_oversampling) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->oversampling != p_oversampling) {
		_font_clear_cache(fd);
		fd->oversampling = p_oversampling;
	}
}

double TextServerAdvanced::_font_get_oversampling(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	return fd->oversampling;
}

TypedArray<Vector2i> TextServerAdvanced::_font_get_size_cache_list(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, TypedArray<Vector2i>());

	MutexLock lock(fd->mutex);
	TypedArray<Vector2i> ret;
	for (const KeyValue<Vector2i, FontForSizeAdvanced *> &E : fd->cache) {
		ret.push_back(E.key);
	}
	return ret;
}

void TextServerAdvanced::_font_clear_size_cache(const RID &p_font_rid) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	MutexLock ftlock(ft_mutex);
	for (const KeyValue<Vector2i, FontForSizeAdvanced *> &E : fd->cache) {
		memdelete(E.value);
	}
	fd->cache.clear();
}

void TextServerAdvanced::_font_remove_size_cache(const RID &p_font_rid, const Vector2i &p_size) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	MutexLock ftlock(ft_mutex);
	if (fd->cache.has(p_size)) {
		memdelete(fd->cache[p_size]);
		fd->cache.erase(p_size);
	}
}

void TextServerAdvanced::_font_set_ascent(const RID &p_font_rid, int64_t p_size, double p_ascent) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->ascent = p_ascent;
}

double TextServerAdvanced::_font_get_ascent(const RID &p_font_rid, int64_t p_size) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0.0);

	if (fd->msdf) {
		return ffsd->ascent * (double)p_size / (double)fd->msdf_source_size;
	} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
		if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
			return ffsd->ascent * (double)p_size / (double)fd->fixed_size;
		} else {
			return ffsd->ascent * Math::round((double)p_size / (double)fd->fixed_size);
		}
	} else {
		return ffsd->ascent;
	}
}

void TextServerAdvanced::_font_set_descent(const RID &p_font_rid, int64_t p_size, double p_descent) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->descent = p_descent;
}

double TextServerAdvanced::_font_get_descent(const RID &p_font_rid, int64_t p_size) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0.0);

	if (fd->msdf) {
		return ffsd->descent * (double)p_size / (double)fd->msdf_source_size;
	} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
		if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
			return ffsd->descent * (double)p_size / (double)fd->fixed_size;
		} else {
			return ffsd->descent * Math::round((double)p_size / (double)fd->fixed_size);
		}
	} else {
		return ffsd->descent;
	}
}

void TextServerAdvanced::_font_set_underline_position(const RID &p_font_rid, int64_t p_size, double p_underline_position) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->underline_position = p_underline_position;
}

double TextServerAdvanced::_font_get_underline_position(const RID &p_font_rid, int64_t p_size) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0.0);

	if (fd->msdf) {
		return ffsd->underline_position * (double)p_size / (double)fd->msdf_source_size;
	} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
		if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
			return ffsd->underline_position * (double)p_size / (double)fd->fixed_size;
		} else {
			return ffsd->underline_position * Math::round((double)p_size / (double)fd->fixed_size);
		}
	} else {
		return ffsd->underline_position;
	}
}

void TextServerAdvanced::_font_set_underline_thickness(const RID &p_font_rid, int64_t p_size, double p_underline_thickness) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->underline_thickness = p_underline_thickness;
}

double TextServerAdvanced::_font_get_underline_thickness(const RID &p_font_rid, int64_t p_size) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0.0);

	if (fd->msdf) {
		return ffsd->underline_thickness * (double)p_size / (double)fd->msdf_source_size;
	} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
		if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
			return ffsd->underline_thickness * (double)p_size / (double)fd->fixed_size;
		} else {
			return ffsd->underline_thickness * Math::round((double)p_size / (double)fd->fixed_size);
		}
	} else {
		return ffsd->underline_thickness;
	}
}

void TextServerAdvanced::_font_set_scale(const RID &p_font_rid, int64_t p_size, double p_scale) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

#ifdef MODULE_FREETYPE_ENABLED
	if (ffsd->face) {
		return; // Do not override scale for dynamic fonts, it's calculated automatically.
	}
#endif
	ffsd->scale = p_scale;
}

double TextServerAdvanced::_font_get_scale(const RID &p_font_rid, int64_t p_size) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0.0);

	if (fd->msdf) {
		return ffsd->scale * (double)p_size / (double)fd->msdf_source_size;
	} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
		if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
			return ffsd->scale * (double)p_size / (double)fd->fixed_size;
		} else {
			return ffsd->scale * Math::round((double)p_size / (double)fd->fixed_size);
		}
	} else {
		return ffsd->scale / ffsd->oversampling;
	}
}

int64_t TextServerAdvanced::_font_get_texture_count(const RID &p_font_rid, const Vector2i &p_size) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0);

	return ffsd->textures.size();
}

void TextServerAdvanced::_font_clear_textures(const RID &p_font_rid, const Vector2i &p_size) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);
	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->textures.clear();
}

void TextServerAdvanced::_font_remove_texture(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ERR_FAIL_INDEX(p_texture_index, ffsd->textures.size());

	ffsd->textures.remove_at(p_texture_index);
}

void TextServerAdvanced::_font_set_texture_image(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index, const Ref<Image> &p_image) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);
	ERR_FAIL_COND(p_image.is_null());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ERR_FAIL_COND(p_texture_index < 0);
	if (p_texture_index >= ffsd->textures.size()) {
		ffsd->textures.resize(p_texture_index + 1);
	}

	ShelfPackTexture &tex = ffsd->textures.write[p_texture_index];

	tex.image = p_image;
	tex.texture_w = p_image->get_width();
	tex.texture_h = p_image->get_height();

	Ref<Image> img = p_image;
	if (fd->mipmaps && !img->has_mipmaps()) {
		img = p_image->duplicate();
		img->generate_mipmaps();
	}
	tex.texture = ImageTexture::create_from_image(img);
	tex.dirty = false;
}

Ref<Image> TextServerAdvanced::_font_get_texture_image(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Ref<Image>());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Ref<Image>());
	ERR_FAIL_INDEX_V(p_texture_index, ffsd->textures.size(), Ref<Image>());

	const ShelfPackTexture &tex = ffsd->textures[p_texture_index];
	return tex.image;
}

void TextServerAdvanced::_font_set_texture_offsets(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index, const PackedInt32Array &p_offsets) {
	ERR_FAIL_COND(p_offsets.size() % 4 != 0);
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ERR_FAIL_COND(p_texture_index < 0);
	if (p_texture_index >= ffsd->textures.size()) {
		ffsd->textures.resize(p_texture_index + 1);
	}

	ShelfPackTexture &tex = ffsd->textures.write[p_texture_index];
	tex.shelves.clear();
	for (int32_t i = 0; i < p_offsets.size(); i += 4) {
		tex.shelves.push_back(Shelf(p_offsets[i], p_offsets[i + 1], p_offsets[i + 2], p_offsets[i + 3]));
	}
}

PackedInt32Array TextServerAdvanced::_font_get_texture_offsets(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, PackedInt32Array());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), PackedInt32Array());
	ERR_FAIL_INDEX_V(p_texture_index, ffsd->textures.size(), PackedInt32Array());

	const ShelfPackTexture &tex = ffsd->textures[p_texture_index];
	PackedInt32Array ret;
	ret.resize(tex.shelves.size() * 4);

	int32_t *wr = ret.ptrw();
	int32_t i = 0;
	for (const Shelf &E : tex.shelves) {
		wr[i * 4] = E.x;
		wr[i * 4 + 1] = E.y;
		wr[i * 4 + 2] = E.w;
		wr[i * 4 + 3] = E.h;
		i++;
	}
	return ret;
}

PackedInt32Array TextServerAdvanced::_font_get_glyph_list(const RID &p_font_rid, const Vector2i &p_size) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, PackedInt32Array());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), PackedInt32Array());

	PackedInt32Array ret;
	const HashMap<int32_t, FontGlyph> &gl = ffsd->glyph_map;
	for (const KeyValue<int32_t, FontGlyph> &E : gl) {
		ret.push_back(E.key);
	}
	return ret;
}

void TextServerAdvanced::_font_clear_glyphs(const RID &p_font_rid, const Vector2i &p_size) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	ffsd->glyph_map.clear();
}

void TextServerAdvanced::_font_remove_glyph(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	ffsd->glyph_map.erase(p_glyph);
}

double TextServerAdvanced::_get_extra_advance(RID p_font_rid, int p_font_size) const {
	const FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_font_size);

	if (fd->embolden != 0.0) {
		return fd->embolden * double(size.x) / 64.0;
	} else {
		return 0.0;
	}
}

Vector2 TextServerAdvanced::_font_get_glyph_advance(const RID &p_font_rid, int64_t p_size, int64_t p_glyph) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Vector2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Vector2());

	int mod = 0;
	if (fd->antialiasing == FONT_ANTIALIASING_LCD) {
		TextServer::FontLCDSubpixelLayout layout = lcd_subpixel_layout.get();
		if (layout != FONT_LCD_SUBPIXEL_LAYOUT_NONE) {
			mod = (layout << 24);
		}
	}

	FontGlyph fgl;
	if (!_ensure_glyph(fd, size, p_glyph | mod, fgl)) {
		return Vector2(); // Invalid or non graphicl glyph, do not display errors.
	}

	Vector2 ea;
	if (fd->embolden != 0.0) {
		ea.x = fd->embolden * double(size.x) / 64.0;
	}

	double scale = _font_get_scale(p_font_rid, p_size);
	if (fd->msdf) {
		return (fgl.advance + ea) * (double)p_size / (double)fd->msdf_source_size;
	} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
		if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
			return (fgl.advance + ea) * (double)p_size / (double)fd->fixed_size;
		} else {
			return (fgl.advance + ea) * Math::round((double)p_size / (double)fd->fixed_size);
		}
	} else if ((scale == 1.0) && ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_DISABLED) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x > SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE))) {
		return (fgl.advance + ea).round();
	} else {
		return fgl.advance + ea;
	}
}

void TextServerAdvanced::_font_set_glyph_advance(const RID &p_font_rid, int64_t p_size, int64_t p_glyph, const Vector2 &p_advance) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	FontGlyph &fgl = ffsd->glyph_map[p_glyph];

	fgl.advance = p_advance;
	fgl.found = true;
}

Vector2 TextServerAdvanced::_font_get_glyph_offset(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Vector2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Vector2());

	int mod = 0;
	if (fd->antialiasing == FONT_ANTIALIASING_LCD) {
		TextServer::FontLCDSubpixelLayout layout = lcd_subpixel_layout.get();
		if (layout != FONT_LCD_SUBPIXEL_LAYOUT_NONE) {
			mod = (layout << 24);
		}
	}

	FontGlyph fgl;
	if (!_ensure_glyph(fd, size, p_glyph | mod, fgl)) {
		return Vector2(); // Invalid or non graphicl glyph, do not display errors.
	}

	if (fd->msdf) {
		return fgl.rect.position * (double)p_size.x / (double)fd->msdf_source_size;
	} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size.x) {
		if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
			return fgl.rect.position * (double)p_size.x / (double)fd->fixed_size;
		} else {
			return fgl.rect.position * Math::round((double)p_size.x / (double)fd->fixed_size);
		}
	} else {
		return fgl.rect.position;
	}
}

void TextServerAdvanced::_font_set_glyph_offset(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Vector2 &p_offset) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	FontGlyph &fgl = ffsd->glyph_map[p_glyph];

	fgl.rect.position = p_offset;
	fgl.found = true;
}

Vector2 TextServerAdvanced::_font_get_glyph_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Vector2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Vector2());

	int mod = 0;
	if (fd->antialiasing == FONT_ANTIALIASING_LCD) {
		TextServer::FontLCDSubpixelLayout layout = lcd_subpixel_layout.get();
		if (layout != FONT_LCD_SUBPIXEL_LAYOUT_NONE) {
			mod = (layout << 24);
		}
	}

	FontGlyph fgl;
	if (!_ensure_glyph(fd, size, p_glyph | mod, fgl)) {
		return Vector2(); // Invalid or non graphicl glyph, do not display errors.
	}

	if (fd->msdf) {
		return fgl.rect.size * (double)p_size.x / (double)fd->msdf_source_size;
	} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size.x) {
		if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
			return fgl.rect.size * (double)p_size.x / (double)fd->fixed_size;
		} else {
			return fgl.rect.size * Math::round((double)p_size.x / (double)fd->fixed_size);
		}
	} else {
		return fgl.rect.size;
	}
}

void TextServerAdvanced::_font_set_glyph_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Vector2 &p_gl_size) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	FontGlyph &fgl = ffsd->glyph_map[p_glyph];

	fgl.rect.size = p_gl_size;
	fgl.found = true;
}

Rect2 TextServerAdvanced::_font_get_glyph_uv_rect(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Rect2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Rect2());

	int mod = 0;
	if (fd->antialiasing == FONT_ANTIALIASING_LCD) {
		TextServer::FontLCDSubpixelLayout layout = lcd_subpixel_layout.get();
		if (layout != FONT_LCD_SUBPIXEL_LAYOUT_NONE) {
			mod = (layout << 24);
		}
	}

	FontGlyph fgl;
	if (!_ensure_glyph(fd, size, p_glyph | mod, fgl)) {
		return Rect2(); // Invalid or non graphicl glyph, do not display errors.
	}

	return fgl.uv_rect;
}

void TextServerAdvanced::_font_set_glyph_uv_rect(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Rect2 &p_uv_rect) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	FontGlyph &fgl = ffsd->glyph_map[p_glyph];

	fgl.uv_rect = p_uv_rect;
	fgl.found = true;
}

int64_t TextServerAdvanced::_font_get_glyph_texture_idx(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, -1);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), -1);

	int mod = 0;
	if (fd->antialiasing == FONT_ANTIALIASING_LCD) {
		TextServer::FontLCDSubpixelLayout layout = lcd_subpixel_layout.get();
		if (layout != FONT_LCD_SUBPIXEL_LAYOUT_NONE) {
			mod = (layout << 24);
		}
	}

	FontGlyph fgl;
	if (!_ensure_glyph(fd, size, p_glyph | mod, fgl)) {
		return -1; // Invalid or non graphicl glyph, do not display errors.
	}

	return fgl.texture_idx;
}

void TextServerAdvanced::_font_set_glyph_texture_idx(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, int64_t p_texture_idx) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	FontGlyph &fgl = ffsd->glyph_map[p_glyph];

	fgl.texture_idx = p_texture_idx;
	fgl.found = true;
}

RID TextServerAdvanced::_font_get_glyph_texture_rid(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, RID());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), RID());

	int mod = 0;
	if (fd->antialiasing == FONT_ANTIALIASING_LCD) {
		TextServer::FontLCDSubpixelLayout layout = lcd_subpixel_layout.get();
		if (layout != FONT_LCD_SUBPIXEL_LAYOUT_NONE) {
			mod = (layout << 24);
		}
	}

	FontGlyph fgl;
	if (!_ensure_glyph(fd, size, p_glyph | mod, fgl)) {
		return RID(); // Invalid or non graphicl glyph, do not display errors.
	}

	ERR_FAIL_COND_V(fgl.texture_idx < -1 || fgl.texture_idx >= ffsd->textures.size(), RID());

	if (RenderingServer::get_singleton() != nullptr) {
		if (fgl.texture_idx != -1) {
			if (ffsd->textures[fgl.texture_idx].dirty) {
				ShelfPackTexture &tex = ffsd->textures.write[fgl.texture_idx];
				Ref<Image> img = tex.image;
				if (fd->mipmaps && !img->has_mipmaps()) {
					img = tex.image->duplicate();
					img->generate_mipmaps();
				}
				if (tex.texture.is_null()) {
					tex.texture = ImageTexture::create_from_image(img);
				} else {
					tex.texture->update(img);
				}
				tex.dirty = false;
			}
			return ffsd->textures[fgl.texture_idx].texture->get_rid();
		}
	}

	return RID();
}

Size2 TextServerAdvanced::_font_get_glyph_texture_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Size2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Size2());

	int mod = 0;
	if (fd->antialiasing == FONT_ANTIALIASING_LCD) {
		TextServer::FontLCDSubpixelLayout layout = lcd_subpixel_layout.get();
		if (layout != FONT_LCD_SUBPIXEL_LAYOUT_NONE) {
			mod = (layout << 24);
		}
	}

	FontGlyph fgl;
	if (!_ensure_glyph(fd, size, p_glyph | mod, fgl)) {
		return Size2(); // Invalid or non graphicl glyph, do not display errors.
	}

	ERR_FAIL_COND_V(fgl.texture_idx < -1 || fgl.texture_idx >= ffsd->textures.size(), Size2());

	if (RenderingServer::get_singleton() != nullptr) {
		if (fgl.texture_idx != -1) {
			if (ffsd->textures[fgl.texture_idx].dirty) {
				ShelfPackTexture &tex = ffsd->textures.write[fgl.texture_idx];
				Ref<Image> img = tex.image;
				if (fd->mipmaps && !img->has_mipmaps()) {
					img = tex.image->duplicate();
					img->generate_mipmaps();
				}
				if (tex.texture.is_null()) {
					tex.texture = ImageTexture::create_from_image(img);
				} else {
					tex.texture->update(img);
				}
				tex.dirty = false;
			}
			return ffsd->textures[fgl.texture_idx].texture->get_size();
		}
	}

	return Size2();
}

Dictionary TextServerAdvanced::_font_get_glyph_contours(const RID &p_font_rid, int64_t p_size, int64_t p_index) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Dictionary());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Dictionary());

#ifdef MODULE_FREETYPE_ENABLED
	PackedVector3Array points;
	PackedInt32Array contours;

	int32_t index = p_index & 0xffffff; // Remove subpixel shifts.

	int error = FT_Load_Glyph(ffsd->face, index, FT_LOAD_NO_BITMAP | (fd->force_autohinter ? FT_LOAD_FORCE_AUTOHINT : 0));
	ERR_FAIL_COND_V(error, Dictionary());

	if (fd->embolden != 0.f) {
		FT_Pos strength = fd->embolden * p_size * 4; // 26.6 fractional units (1 / 64).
		FT_Outline_Embolden(&ffsd->face->glyph->outline, strength);
	}

	if (fd->transform != Transform2D()) {
		FT_Matrix mat = { FT_Fixed(fd->transform[0][0] * 65536), FT_Fixed(fd->transform[0][1] * 65536), FT_Fixed(fd->transform[1][0] * 65536), FT_Fixed(fd->transform[1][1] * 65536) }; // 16.16 fractional units (1 / 65536).
		FT_Outline_Transform(&ffsd->face->glyph->outline, &mat);
	}

	double scale = (1.0 / 64.0) / ffsd->oversampling * ffsd->scale;
	if (fd->msdf) {
		scale = scale * (double)p_size / (double)fd->msdf_source_size;
	} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
		if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
			scale = scale * (double)p_size / (double)fd->fixed_size;
		} else {
			scale = scale * Math::round((double)p_size / (double)fd->fixed_size);
		}
	}
	for (short i = 0; i < ffsd->face->glyph->outline.n_points; i++) {
		points.push_back(Vector3(ffsd->face->glyph->outline.points[i].x * scale, -ffsd->face->glyph->outline.points[i].y * scale, FT_CURVE_TAG(ffsd->face->glyph->outline.tags[i])));
	}
	for (short i = 0; i < ffsd->face->glyph->outline.n_contours; i++) {
		contours.push_back(ffsd->face->glyph->outline.contours[i]);
	}
	bool orientation = (FT_Outline_Get_Orientation(&ffsd->face->glyph->outline) == FT_ORIENTATION_FILL_RIGHT);

	Dictionary out;
	out["points"] = points;
	out["contours"] = contours;
	out["orientation"] = orientation;
	return out;
#else
	return Dictionary();
#endif
}

TypedArray<Vector2i> TextServerAdvanced::_font_get_kerning_list(const RID &p_font_rid, int64_t p_size) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, TypedArray<Vector2i>());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), TypedArray<Vector2i>());

	TypedArray<Vector2i> ret;
	for (const KeyValue<Vector2i, Vector2> &E : fd->cache[size]->kerning_map) {
		ret.push_back(E.key);
	}
	return ret;
}

void TextServerAdvanced::_font_clear_kerning_map(const RID &p_font_rid, int64_t p_size) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->kerning_map.clear();
}

void TextServerAdvanced::_font_remove_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->kerning_map.erase(p_glyph_pair);
}

void TextServerAdvanced::_font_set_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair, const Vector2 &p_kerning) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->kerning_map[p_glyph_pair] = p_kerning;
}

Vector2 TextServerAdvanced::_font_get_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Vector2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Vector2());

	const HashMap<Vector2i, Vector2> &kern = ffsd->kerning_map;

	if (kern.has(p_glyph_pair)) {
		if (fd->msdf) {
			return kern[p_glyph_pair] * (double)p_size / (double)fd->msdf_source_size;
		} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
			if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
				return kern[p_glyph_pair] * (double)p_size / (double)fd->fixed_size;
			} else {
				return kern[p_glyph_pair] * Math::round((double)p_size / (double)fd->fixed_size);
			}
		} else {
			return kern[p_glyph_pair];
		}
	} else {
#ifdef MODULE_FREETYPE_ENABLED
		if (ffsd->face) {
			FT_Vector delta;
			FT_Get_Kerning(ffsd->face, p_glyph_pair.x, p_glyph_pair.y, FT_KERNING_DEFAULT, &delta);
			if (fd->msdf) {
				return Vector2(delta.x, delta.y) * (double)p_size / (double)fd->msdf_source_size;
			} else if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
				if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
					return Vector2(delta.x, delta.y) * (double)p_size / (double)fd->fixed_size;
				} else {
					return Vector2(delta.x, delta.y) * Math::round((double)p_size / (double)fd->fixed_size);
				}
			} else {
				return Vector2(delta.x, delta.y);
			}
		}
#endif
	}
	return Vector2();
}

int64_t TextServerAdvanced::_font_get_glyph_index(const RID &p_font_rid, int64_t p_size, int64_t p_char, int64_t p_variation_selector) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);
	ERR_FAIL_COND_V_MSG((p_char >= 0xd800 && p_char <= 0xdfff) || (p_char > 0x10ffff), 0, "Unicode parsing error: Invalid unicode codepoint " + String::num_int64(p_char, 16) + ".");
	ERR_FAIL_COND_V_MSG((p_variation_selector >= 0xd800 && p_variation_selector <= 0xdfff) || (p_variation_selector > 0x10ffff), 0, "Unicode parsing error: Invalid unicode codepoint " + String::num_int64(p_variation_selector, 16) + ".");

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0);

#ifdef MODULE_FREETYPE_ENABLED
	if (ffsd->face) {
		if (p_variation_selector) {
			return FT_Face_GetCharVariantIndex(ffsd->face, p_char, p_variation_selector);
		} else {
			return FT_Get_Char_Index(ffsd->face, p_char);
		}
	} else {
		return (int64_t)p_char;
	}
#else
	return (int64_t)p_char;
#endif
}

int64_t TextServerAdvanced::_font_get_char_from_glyph_index(const RID &p_font_rid, int64_t p_size, int64_t p_glyph_index) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0);

#ifdef MODULE_FREETYPE_ENABLED
	if (ffsd->inv_glyph_map.is_empty()) {
		FT_Face face = ffsd->face;
		FT_UInt gindex;
		FT_ULong charcode = FT_Get_First_Char(face, &gindex);
		while (gindex != 0) {
			if (charcode != 0) {
				ffsd->inv_glyph_map[gindex] = charcode;
			}
			charcode = FT_Get_Next_Char(face, charcode, &gindex);
		}
	}

	if (ffsd->inv_glyph_map.has(p_glyph_index)) {
		return ffsd->inv_glyph_map[p_glyph_index];
	} else {
		return 0;
	}
#else
	return p_glyph_index;
#endif
}

bool TextServerAdvanced::_font_has_char(const RID &p_font_rid, int64_t p_char) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_COND_V_MSG((p_char >= 0xd800 && p_char <= 0xdfff) || (p_char > 0x10ffff), false, "Unicode parsing error: Invalid unicode codepoint " + String::num_int64(p_char, 16) + ".");
	if (!fd) {
		return false;
	}

	MutexLock lock(fd->mutex);
	FontForSizeAdvanced *ffsd = nullptr;
	if (fd->cache.is_empty()) {
		ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, fd->msdf ? Vector2i(fd->msdf_source_size, 0) : Vector2i(16, 0), ffsd), false);
	} else {
		ffsd = fd->cache.begin()->value;
	}

#ifdef MODULE_FREETYPE_ENABLED
	if (ffsd->face) {
		return FT_Get_Char_Index(ffsd->face, p_char) != 0;
	}
#endif
	return ffsd->glyph_map.has((int32_t)p_char);
}

String TextServerAdvanced::_font_get_supported_chars(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, String());

	MutexLock lock(fd->mutex);
	FontForSizeAdvanced *ffsd = nullptr;
	if (fd->cache.is_empty()) {
		ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, fd->msdf ? Vector2i(fd->msdf_source_size, 0) : Vector2i(16, 0), ffsd), String());
	} else {
		ffsd = fd->cache.begin()->value;
	}

	String chars;
#ifdef MODULE_FREETYPE_ENABLED
	if (ffsd->face) {
		FT_UInt gindex;
		FT_ULong charcode = FT_Get_First_Char(ffsd->face, &gindex);
		while (gindex != 0) {
			if (charcode != 0) {
				chars = chars + String::chr(charcode);
			}
			charcode = FT_Get_Next_Char(ffsd->face, charcode, &gindex);
		}
		return chars;
	}
#endif
	const HashMap<int32_t, FontGlyph> &gl = ffsd->glyph_map;
	for (const KeyValue<int32_t, FontGlyph> &E : gl) {
		chars = chars + String::chr(E.key);
	}
	return chars;
}

PackedInt32Array TextServerAdvanced::_font_get_supported_glyphs(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, PackedInt32Array());

	MutexLock lock(fd->mutex);
	FontForSizeAdvanced *at_size = nullptr;
	if (fd->cache.is_empty()) {
		ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, fd->msdf ? Vector2i(fd->msdf_source_size, 0) : Vector2i(16, 0), at_size), PackedInt32Array());
	} else {
		at_size = fd->cache.begin()->value;
	}

	PackedInt32Array glyphs;
#ifdef MODULE_FREETYPE_ENABLED
	if (at_size && at_size->face) {
		FT_UInt gindex;
		FT_ULong charcode = FT_Get_First_Char(at_size->face, &gindex);
		while (gindex != 0) {
			glyphs.push_back(gindex);
			charcode = FT_Get_Next_Char(at_size->face, charcode, &gindex);
		}
		return glyphs;
	}
#endif
	if (at_size) {
		const HashMap<int32_t, FontGlyph> &gl = at_size->glyph_map;
		for (const KeyValue<int32_t, FontGlyph> &E : gl) {
			glyphs.push_back(E.key);
		}
	}
	return glyphs;
}

void TextServerAdvanced::_font_render_range(const RID &p_font_rid, const Vector2i &p_size, int64_t p_start, int64_t p_end) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);
	ERR_FAIL_COND_MSG((p_start >= 0xd800 && p_start <= 0xdfff) || (p_start > 0x10ffff), "Unicode parsing error: Invalid unicode codepoint " + String::num_int64(p_start, 16) + ".");
	ERR_FAIL_COND_MSG((p_end >= 0xd800 && p_end <= 0xdfff) || (p_end > 0x10ffff), "Unicode parsing error: Invalid unicode codepoint " + String::num_int64(p_end, 16) + ".");

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	for (int64_t i = p_start; i <= p_end; i++) {
#ifdef MODULE_FREETYPE_ENABLED
		int32_t idx = FT_Get_Char_Index(ffsd->face, i);
		if (ffsd->face) {
			FontGlyph fgl;
			if (fd->msdf) {
				_ensure_glyph(fd, size, (int32_t)idx, fgl);
			} else {
				for (int aa = 0; aa < ((fd->antialiasing == FONT_ANTIALIASING_LCD) ? FONT_LCD_SUBPIXEL_LAYOUT_MAX : 1); aa++) {
					if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_QUARTER) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_QUARTER_MAX_SIZE)) {
						_ensure_glyph(fd, size, (int32_t)idx | (0 << 27) | (aa << 24), fgl);
						_ensure_glyph(fd, size, (int32_t)idx | (1 << 27) | (aa << 24), fgl);
						_ensure_glyph(fd, size, (int32_t)idx | (2 << 27) | (aa << 24), fgl);
						_ensure_glyph(fd, size, (int32_t)idx | (3 << 27) | (aa << 24), fgl);
					} else if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_HALF) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE)) {
						_ensure_glyph(fd, size, (int32_t)idx | (1 << 27) | (aa << 24), fgl);
						_ensure_glyph(fd, size, (int32_t)idx | (0 << 27) | (aa << 24), fgl);
					} else {
						_ensure_glyph(fd, size, (int32_t)idx | (aa << 24), fgl);
					}
				}
			}
		}
#endif
	}
}

void TextServerAdvanced::_font_render_glyph(const RID &p_font_rid, const Vector2i &p_size, int64_t p_index) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
#ifdef MODULE_FREETYPE_ENABLED
	int32_t idx = p_index & 0xffffff; // Remove subpixel shifts.
	if (ffsd->face) {
		FontGlyph fgl;
		if (fd->msdf) {
			_ensure_glyph(fd, size, (int32_t)idx, fgl);
		} else {
			for (int aa = 0; aa < ((fd->antialiasing == FONT_ANTIALIASING_LCD) ? FONT_LCD_SUBPIXEL_LAYOUT_MAX : 1); aa++) {
				if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_QUARTER) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_QUARTER_MAX_SIZE)) {
					_ensure_glyph(fd, size, (int32_t)idx | (0 << 27) | (aa << 24), fgl);
					_ensure_glyph(fd, size, (int32_t)idx | (1 << 27) | (aa << 24), fgl);
					_ensure_glyph(fd, size, (int32_t)idx | (2 << 27) | (aa << 24), fgl);
					_ensure_glyph(fd, size, (int32_t)idx | (3 << 27) | (aa << 24), fgl);
				} else if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_HALF) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE)) {
					_ensure_glyph(fd, size, (int32_t)idx | (1 << 27) | (aa << 24), fgl);
					_ensure_glyph(fd, size, (int32_t)idx | (0 << 27) | (aa << 24), fgl);
				} else {
					_ensure_glyph(fd, size, (int32_t)idx | (aa << 24), fgl);
				}
			}
		}
	}
#endif
}

void TextServerAdvanced::_font_draw_glyph(const RID &p_font_rid, const RID &p_canvas, int64_t p_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color) const {
	if (p_index == 0) {
		return; // Non visual character, skip.
	}
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	int32_t index = p_index & 0xffffff; // Remove subpixel shifts.
	bool lcd_aa = false;

#ifdef MODULE_FREETYPE_ENABLED
	if (!fd->msdf && ffsd->face) {
		// LCD layout, bits 24, 25, 26
		if (fd->antialiasing == FONT_ANTIALIASING_LCD) {
			TextServer::FontLCDSubpixelLayout layout = lcd_subpixel_layout.get();
			if (layout != FONT_LCD_SUBPIXEL_LAYOUT_NONE) {
				lcd_aa = true;
				index = index | (layout << 24);
			}
		}
		// Subpixel X-shift, bits 27, 28
		if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_QUARTER) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_QUARTER_MAX_SIZE)) {
			int xshift = (int)(Math::floor(4 * (p_pos.x + 0.125)) - 4 * Math::floor(p_pos.x + 0.125));
			index = index | (xshift << 27);
		} else if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_HALF) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE)) {
			int xshift = (int)(Math::floor(2 * (p_pos.x + 0.25)) - 2 * Math::floor(p_pos.x + 0.25));
			index = index | (xshift << 27);
		}
	}
#endif

	FontGlyph fgl;
	if (!_ensure_glyph(fd, size, index, fgl)) {
		return; // Invalid or non-graphical glyph, do not display errors, nothing to draw.
	}

	if (fgl.found) {
		ERR_FAIL_COND(fgl.texture_idx < -1 || fgl.texture_idx >= ffsd->textures.size());

		if (fgl.texture_idx != -1) {
			Color modulate = p_color;
#ifdef MODULE_FREETYPE_ENABLED
			if (ffsd->face && ffsd->textures[fgl.texture_idx].image.is_valid() && (ffsd->textures[fgl.texture_idx].image->get_format() == Image::FORMAT_RGBA8) && !lcd_aa && !fd->msdf) {
				modulate.r = modulate.g = modulate.b = 1.0;
			}
#endif
			if (RenderingServer::get_singleton() != nullptr) {
				if (ffsd->textures[fgl.texture_idx].dirty) {
					ShelfPackTexture &tex = ffsd->textures.write[fgl.texture_idx];
					Ref<Image> img = tex.image;
					if (fd->mipmaps && !img->has_mipmaps()) {
						img = tex.image->duplicate();
						img->generate_mipmaps();
					}
					if (tex.texture.is_null()) {
						tex.texture = ImageTexture::create_from_image(img);
					} else {
						tex.texture->update(img);
					}
					tex.dirty = false;
				}
				RID texture = ffsd->textures[fgl.texture_idx].texture->get_rid();
				if (fd->msdf) {
					Point2 cpos = p_pos;
					cpos += fgl.rect.position * (double)p_size / (double)fd->msdf_source_size;
					Size2 csize = fgl.rect.size * (double)p_size / (double)fd->msdf_source_size;
					RenderingServer::get_singleton()->canvas_item_add_msdf_texture_rect_region(p_canvas, Rect2(cpos, csize), texture, fgl.uv_rect, modulate, 0, fd->msdf_range, (double)p_size / (double)fd->msdf_source_size);
				} else {
					double scale = _font_get_scale(p_font_rid, p_size);
					Point2 cpos = p_pos;
					if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_QUARTER) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_QUARTER_MAX_SIZE)) {
						cpos.x = cpos.x + 0.125;
					} else if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_HALF) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE)) {
						cpos.x = cpos.x + 0.25;
					}
					if (scale == 1.0) {
						cpos.y = Math::floor(cpos.y);
						cpos.x = Math::floor(cpos.x);
					}
					Vector2 gpos = fgl.rect.position;
					Size2 csize = fgl.rect.size;
					if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
						if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
							double gl_scale = (double)p_size / (double)fd->fixed_size;
							gpos *= gl_scale;
							csize *= gl_scale;
						} else {
							double gl_scale = Math::round((double)p_size / (double)fd->fixed_size);
							gpos *= gl_scale;
							csize *= gl_scale;
						}
					}
					cpos += gpos;
					if (lcd_aa) {
						RenderingServer::get_singleton()->canvas_item_add_lcd_texture_rect_region(p_canvas, Rect2(cpos, csize), texture, fgl.uv_rect, modulate);
					} else {
						RenderingServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas, Rect2(cpos, csize), texture, fgl.uv_rect, modulate, false, false);
					}
				}
			}
		}
	}
}

void TextServerAdvanced::_font_draw_glyph_outline(const RID &p_font_rid, const RID &p_canvas, int64_t p_size, int64_t p_outline_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color) const {
	if (p_index == 0) {
		return; // Non visual character, skip.
	}
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, Vector2i(p_size, p_outline_size));
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	int32_t index = p_index & 0xffffff; // Remove subpixel shifts.
	bool lcd_aa = false;

#ifdef MODULE_FREETYPE_ENABLED
	if (!fd->msdf && ffsd->face) {
		// LCD layout, bits 24, 25, 26
		if (fd->antialiasing == FONT_ANTIALIASING_LCD) {
			TextServer::FontLCDSubpixelLayout layout = lcd_subpixel_layout.get();
			if (layout != FONT_LCD_SUBPIXEL_LAYOUT_NONE) {
				lcd_aa = true;
				index = index | (layout << 24);
			}
		}
		// Subpixel X-shift, bits 27, 28
		if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_QUARTER) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_QUARTER_MAX_SIZE)) {
			int xshift = (int)(Math::floor(4 * (p_pos.x + 0.125)) - 4 * Math::floor(p_pos.x + 0.125));
			index = index | (xshift << 27);
		} else if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_HALF) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE)) {
			int xshift = (int)(Math::floor(2 * (p_pos.x + 0.25)) - 2 * Math::floor(p_pos.x + 0.25));
			index = index | (xshift << 27);
		}
	}
#endif

	FontGlyph fgl;
	if (!_ensure_glyph(fd, size, index, fgl)) {
		return; // Invalid or non-graphical glyph, do not display errors, nothing to draw.
	}

	if (fgl.found) {
		ERR_FAIL_COND(fgl.texture_idx < -1 || fgl.texture_idx >= ffsd->textures.size());

		if (fgl.texture_idx != -1) {
			Color modulate = p_color;
#ifdef MODULE_FREETYPE_ENABLED
			if (ffsd->face && fd->cache[size]->textures[fgl.texture_idx].image.is_valid() && (ffsd->textures[fgl.texture_idx].image->get_format() == Image::FORMAT_RGBA8) && !lcd_aa && !fd->msdf) {
				modulate.r = modulate.g = modulate.b = 1.0;
			}
#endif
			if (RenderingServer::get_singleton() != nullptr) {
				if (ffsd->textures[fgl.texture_idx].dirty) {
					ShelfPackTexture &tex = ffsd->textures.write[fgl.texture_idx];
					Ref<Image> img = tex.image;
					if (fd->mipmaps && !img->has_mipmaps()) {
						img = tex.image->duplicate();
						img->generate_mipmaps();
					}
					if (tex.texture.is_null()) {
						tex.texture = ImageTexture::create_from_image(img);
					} else {
						tex.texture->update(img);
					}
					tex.dirty = false;
				}
				RID texture = ffsd->textures[fgl.texture_idx].texture->get_rid();
				if (fd->msdf) {
					Point2 cpos = p_pos;
					cpos += fgl.rect.position * (double)p_size / (double)fd->msdf_source_size;
					Size2 csize = fgl.rect.size * (double)p_size / (double)fd->msdf_source_size;
					RenderingServer::get_singleton()->canvas_item_add_msdf_texture_rect_region(p_canvas, Rect2(cpos, csize), texture, fgl.uv_rect, modulate, p_outline_size, fd->msdf_range, (double)p_size / (double)fd->msdf_source_size);
				} else {
					Point2 cpos = p_pos;
					double scale = _font_get_scale(p_font_rid, p_size);
					if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_QUARTER) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_QUARTER_MAX_SIZE)) {
						cpos.x = cpos.x + 0.125;
					} else if ((fd->subpixel_positioning == SUBPIXEL_POSITIONING_ONE_HALF) || (fd->subpixel_positioning == SUBPIXEL_POSITIONING_AUTO && size.x <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE)) {
						cpos.x = cpos.x + 0.25;
					}
					if (scale == 1.0) {
						cpos.y = Math::floor(cpos.y);
						cpos.x = Math::floor(cpos.x);
					}
					Vector2 gpos = fgl.rect.position;
					Size2 csize = fgl.rect.size;
					if (fd->fixed_size > 0 && fd->fixed_size_scale_mode != FIXED_SIZE_SCALE_DISABLE && size.x != p_size) {
						if (fd->fixed_size_scale_mode == FIXED_SIZE_SCALE_ENABLED) {
							double gl_scale = (double)p_size / (double)fd->fixed_size;
							gpos *= gl_scale;
							csize *= gl_scale;
						} else {
							double gl_scale = Math::round((double)p_size / (double)fd->fixed_size);
							gpos *= gl_scale;
							csize *= gl_scale;
						}
					}
					cpos += gpos;
					if (lcd_aa) {
						RenderingServer::get_singleton()->canvas_item_add_lcd_texture_rect_region(p_canvas, Rect2(cpos, csize), texture, fgl.uv_rect, modulate);
					} else {
						RenderingServer::get_singleton()->canvas_item_add_texture_rect_region(p_canvas, Rect2(cpos, csize), texture, fgl.uv_rect, modulate, false, false);
					}
				}
			}
		}
	}
}

bool TextServerAdvanced::_font_is_language_supported(const RID &p_font_rid, const String &p_language) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	if (fd->language_support_overrides.has(p_language)) {
		return fd->language_support_overrides[p_language];
	} else {
		return true;
	}
}

void TextServerAdvanced::_font_set_language_support_override(const RID &p_font_rid, const String &p_language, bool p_supported) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->language_support_overrides[p_language] = p_supported;
}

bool TextServerAdvanced::_font_get_language_support_override(const RID &p_font_rid, const String &p_language) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->language_support_overrides[p_language];
}

void TextServerAdvanced::_font_remove_language_support_override(const RID &p_font_rid, const String &p_language) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->language_support_overrides.erase(p_language);
}

PackedStringArray TextServerAdvanced::_font_get_language_support_overrides(const RID &p_font_rid) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, PackedStringArray());

	MutexLock lock(fd->mutex);
	PackedStringArray out;
	for (const KeyValue<String, bool> &E : fd->language_support_overrides) {
		out.push_back(E.key);
	}
	return out;
}

bool TextServerAdvanced::_font_is_script_supported(const RID &p_font_rid, const String &p_script) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	if (fd->script_support_overrides.has(p_script)) {
		return fd->script_support_overrides[p_script];
	} else {
		Vector2i size = _get_size(fd, 16);
		FontForSizeAdvanced *ffsd = nullptr;
		ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), false);
		return fd->supported_scripts.has(hb_tag_from_string(p_script.ascii().get_data(), -1));
	}
}

void TextServerAdvanced::_font_set_script_support_override(const RID &p_font_rid, const String &p_script, bool p_supported) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->script_support_overrides[p_script] = p_supported;
}

bool TextServerAdvanced::_font_get_script_support_override(const RID &p_font_rid, const String &p_script) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->script_support_overrides[p_script];
}

void TextServerAdvanced::_font_remove_script_support_override(const RID &p_font_rid, const String &p_script) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->script_support_overrides.erase(p_script);
}

PackedStringArray TextServerAdvanced::_font_get_script_support_overrides(const RID &p_font_rid) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, PackedStringArray());

	MutexLock lock(fd->mutex);
	PackedStringArray out;
	for (const KeyValue<String, bool> &E : fd->script_support_overrides) {
		out.push_back(E.key);
	}
	return out;
}

void TextServerAdvanced::_font_set_opentype_feature_overrides(const RID &p_font_rid, const Dictionary &p_overrides) {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->feature_overrides = p_overrides;
}

Dictionary TextServerAdvanced::_font_get_opentype_feature_overrides(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Dictionary());

	MutexLock lock(fd->mutex);
	return fd->feature_overrides;
}

Dictionary TextServerAdvanced::_font_supported_feature_list(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Dictionary());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Dictionary());
	return fd->supported_features;
}

Dictionary TextServerAdvanced::_font_supported_variation_list(const RID &p_font_rid) const {
	FontAdvanced *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Dictionary());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeAdvanced *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Dictionary());
	return fd->supported_varaitions;
}

double TextServerAdvanced::_font_get_global_oversampling() const {
	return oversampling;
}

void TextServerAdvanced::_font_set_global_oversampling(double p_oversampling) {
	_THREAD_SAFE_METHOD_
	if (oversampling != p_oversampling) {
		oversampling = p_oversampling;
		List<RID> fonts;
		font_owner.get_owned_list(&fonts);
		bool font_cleared = false;
		for (const RID &E : fonts) {
			if (!_font_is_multichannel_signed_distance_field(E) && _font_get_oversampling(E) <= 0) {
				_font_clear_size_cache(E);
				font_cleared = true;
			}
		}

		if (font_cleared) {
			List<RID> text_bufs;
			shaped_owner.get_owned_list(&text_bufs);
			for (const RID &E : text_bufs) {
				invalidate(shaped_owner.get_or_null(E), false);
			}
		}
	}
}

/*************************************************************************/
/* Shaped text buffer interface                                          */
/*************************************************************************/

int64_t TextServerAdvanced::_convert_pos(const String &p_utf32, const Char16String &p_utf16, int64_t p_pos) const {
	int64_t limit = p_pos;
	if (p_utf32.length() != p_utf16.length()) {
		const UChar *data = p_utf16.get_data();
		for (int i = 0; i < p_pos; i++) {
			if (U16_IS_LEAD(data[i])) {
				limit--;
			}
		}
	}
	return limit;
}

int64_t TextServerAdvanced::_convert_pos(const ShapedTextDataAdvanced *p_sd, int64_t p_pos) const {
	int64_t limit = p_pos;
	if (p_sd->text.length() != p_sd->utf16.length()) {
		const UChar *data = p_sd->utf16.get_data();
		for (int i = 0; i < p_pos; i++) {
			if (U16_IS_LEAD(data[i])) {
				limit--;
			}
		}
	}
	return limit;
}

int64_t TextServerAdvanced::_convert_pos_inv(const ShapedTextDataAdvanced *p_sd, int64_t p_pos) const {
	int64_t limit = p_pos;
	if (p_sd->text.length() != p_sd->utf16.length()) {
		for (int i = 0; i < p_pos; i++) {
			if (p_sd->text[i] > 0xffff) {
				limit++;
			}
		}
	}
	return limit;
}

void TextServerAdvanced::invalidate(TextServerAdvanced::ShapedTextDataAdvanced *p_shaped, bool p_text) {
	p_shaped->valid.clear();
	p_shaped->sort_valid = false;
	p_shaped->line_breaks_valid = false;
	p_shaped->justification_ops_valid = false;
	p_shaped->text_trimmed = false;
	p_shaped->ascent = 0.0;
	p_shaped->descent = 0.0;
	p_shaped->width = 0.0;
	p_shaped->upos = 0.0;
	p_shaped->uthk = 0.0;
	p_shaped->glyphs.clear();
	p_shaped->glyphs_logical.clear();
	p_shaped->overrun_trim_data = TrimData();
	p_shaped->utf16 = Char16String();
	for (int i = 0; i < p_shaped->bidi_iter.size(); i++) {
		ubidi_close(p_shaped->bidi_iter[i]);
	}
	p_shaped->bidi_iter.clear();

	if (p_text) {
		if (p_shaped->script_iter != nullptr) {
			memdelete(p_shaped->script_iter);
			p_shaped->script_iter = nullptr;
		}
		p_shaped->break_ops_valid = false;
		p_shaped->chars_valid = false;
		p_shaped->js_ops_valid = false;
	}
}

void TextServerAdvanced::full_copy(ShapedTextDataAdvanced *p_shaped) {
	ShapedTextDataAdvanced *parent = shaped_owner.get_or_null(p_shaped->parent);

	for (const KeyValue<Variant, ShapedTextDataAdvanced::EmbeddedObject> &E : parent->objects) {
		if (E.value.start >= p_shaped->start && E.value.start < p_shaped->end) {
			p_shaped->objects[E.key] = E.value;
		}
	}

	for (int i = 0; i < parent->spans.size(); i++) {
		ShapedTextDataAdvanced::Span span = parent->spans[i];
		if (span.start >= p_shaped->end || span.end <= p_shaped->start) {
			continue;
		}
		span.start = MAX(p_shaped->start, span.start);
		span.end = MIN(p_shaped->end, span.end);
		p_shaped->spans.push_back(span);
	}

	p_shaped->parent = RID();
}

RID TextServerAdvanced::_create_shaped_text(TextServer::Direction p_direction, TextServer::Orientation p_orientation) {
	_THREAD_SAFE_METHOD_
	ERR_FAIL_COND_V_MSG(p_direction == DIRECTION_INHERITED, RID(), "Invalid text direction.");

	ShapedTextDataAdvanced *sd = memnew(ShapedTextDataAdvanced);
	sd->hb_buffer = hb_buffer_create();
	sd->direction = p_direction;
	sd->orientation = p_orientation;
	return shaped_owner.make_rid(sd);
}

void TextServerAdvanced::_shaped_text_clear(const RID &p_shaped) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	sd->parent = RID();
	sd->start = 0;
	sd->end = 0;
	sd->text = String();
	sd->spans.clear();
	sd->objects.clear();
	sd->bidi_override.clear();
	invalidate(sd, true);
}

void TextServerAdvanced::_shaped_text_set_direction(const RID &p_shaped, TextServer::Direction p_direction) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_COND_MSG(p_direction == DIRECTION_INHERITED, "Invalid text direction.");
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	if (sd->direction != p_direction) {
		if (sd->parent != RID()) {
			full_copy(sd);
		}
		sd->direction = p_direction;
		invalidate(sd, false);
	}
}

TextServer::Direction TextServerAdvanced::_shaped_text_get_direction(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, TextServer::DIRECTION_LTR);

	MutexLock lock(sd->mutex);
	return sd->direction;
}

TextServer::Direction TextServerAdvanced::_shaped_text_get_inferred_direction(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, TextServer::DIRECTION_LTR);

	MutexLock lock(sd->mutex);
	return sd->para_direction;
}

void TextServerAdvanced::_shaped_text_set_custom_punctuation(const RID &p_shaped, const String &p_punct) {
	_THREAD_SAFE_METHOD_
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	if (sd->custom_punct != p_punct) {
		if (sd->parent != RID()) {
			full_copy(sd);
		}
		sd->custom_punct = p_punct;
		invalidate(sd, false);
	}
}

String TextServerAdvanced::_shaped_text_get_custom_punctuation(const RID &p_shaped) const {
	_THREAD_SAFE_METHOD_
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, String());
	return sd->custom_punct;
}

void TextServerAdvanced::_shaped_text_set_custom_ellipsis(const RID &p_shaped, int64_t p_char) {
	_THREAD_SAFE_METHOD_
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);
	sd->el_char = p_char;
}

int64_t TextServerAdvanced::_shaped_text_get_custom_ellipsis(const RID &p_shaped) const {
	_THREAD_SAFE_METHOD_
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0);
	return sd->el_char;
}

void TextServerAdvanced::_shaped_text_set_bidi_override(const RID &p_shaped, const Array &p_override) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	if (sd->parent != RID()) {
		full_copy(sd);
	}
	sd->bidi_override.clear();
	for (int i = 0; i < p_override.size(); i++) {
		if (p_override[i].get_type() == Variant::VECTOR3I) {
			const Vector3i &r = p_override[i];
			sd->bidi_override.push_back(r);
		} else if (p_override[i].get_type() == Variant::VECTOR2I) {
			const Vector2i &r = p_override[i];
			sd->bidi_override.push_back(Vector3i(r.x, r.y, DIRECTION_INHERITED));
		}
	}
	invalidate(sd, false);
}

void TextServerAdvanced::_shaped_text_set_orientation(const RID &p_shaped, TextServer::Orientation p_orientation) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	if (sd->orientation != p_orientation) {
		if (sd->parent != RID()) {
			full_copy(sd);
		}
		sd->orientation = p_orientation;
		invalidate(sd, false);
	}
}

void TextServerAdvanced::_shaped_text_set_preserve_invalid(const RID &p_shaped, bool p_enabled) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	ERR_FAIL_COND(sd->parent != RID());
	if (sd->preserve_invalid != p_enabled) {
		sd->preserve_invalid = p_enabled;
		invalidate(sd, false);
	}
}

bool TextServerAdvanced::_shaped_text_get_preserve_invalid(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	return sd->preserve_invalid;
}

void TextServerAdvanced::_shaped_text_set_preserve_control(const RID &p_shaped, bool p_enabled) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	if (sd->preserve_control != p_enabled) {
		if (sd->parent != RID()) {
			full_copy(sd);
		}
		sd->preserve_control = p_enabled;
		invalidate(sd, false);
	}
}

bool TextServerAdvanced::_shaped_text_get_preserve_control(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	return sd->preserve_control;
}

void TextServerAdvanced::_shaped_text_set_spacing(const RID &p_shaped, SpacingType p_spacing, int64_t p_value) {
	ERR_FAIL_INDEX((int)p_spacing, 4);
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	if (sd->extra_spacing[p_spacing] != p_value) {
		if (sd->parent != RID()) {
			full_copy(sd);
		}
		sd->extra_spacing[p_spacing] = p_value;
		invalidate(sd, false);
	}
}

int64_t TextServerAdvanced::_shaped_text_get_spacing(const RID &p_shaped, SpacingType p_spacing) const {
	ERR_FAIL_INDEX_V((int)p_spacing, 4, 0);

	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0);

	MutexLock lock(sd->mutex);
	return sd->extra_spacing[p_spacing];
}

TextServer::Orientation TextServerAdvanced::_shaped_text_get_orientation(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, TextServer::ORIENTATION_HORIZONTAL);

	MutexLock lock(sd->mutex);
	return sd->orientation;
}

int64_t TextServerAdvanced::_shaped_get_span_count(const RID &p_shaped) const {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0);
	return sd->spans.size();
}

Variant TextServerAdvanced::_shaped_get_span_meta(const RID &p_shaped, int64_t p_index) const {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Variant());
	ERR_FAIL_INDEX_V(p_index, sd->spans.size(), Variant());
	return sd->spans[p_index].meta;
}

void TextServerAdvanced::_shaped_set_span_update_font(const RID &p_shaped, int64_t p_index, const TypedArray<RID> &p_fonts, int64_t p_size, const Dictionary &p_opentype_features) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);
	ERR_FAIL_INDEX(p_index, sd->spans.size());

	ShapedTextDataAdvanced::Span &span = sd->spans.ptrw()[p_index];
	span.fonts = p_fonts;
	span.font_size = p_size;
	span.features = p_opentype_features;

	invalidate(sd, false);
}

bool TextServerAdvanced::_shaped_text_add_string(const RID &p_shaped, const String &p_text, const TypedArray<RID> &p_fonts, int64_t p_size, const Dictionary &p_opentype_features, const String &p_language, const Variant &p_meta) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);
	ERR_FAIL_COND_V(p_size <= 0, false);

	MutexLock lock(sd->mutex);
	for (int i = 0; i < p_fonts.size(); i++) {
		ERR_FAIL_NULL_V(_get_font_data(p_fonts[i]), false);
	}

	if (p_text.is_empty()) {
		return true;
	}

	if (sd->parent != RID()) {
		full_copy(sd);
	}

	ShapedTextDataAdvanced::Span span;
	span.start = sd->text.length();
	span.end = span.start + p_text.length();
	span.fonts = p_fonts; // Do not pre-sort, spans will be divided to subruns later.
	span.font_size = p_size;
	span.language = p_language;
	span.features = p_opentype_features;
	span.meta = p_meta;

	sd->spans.push_back(span);
	sd->text = sd->text + p_text;
	sd->end += p_text.length();
	invalidate(sd, true);

	return true;
}

bool TextServerAdvanced::_shaped_text_add_object(const RID &p_shaped, const Variant &p_key, const Size2 &p_size, InlineAlignment p_inline_align, int64_t p_length, double p_baseline) {
	_THREAD_SAFE_METHOD_
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);
	ERR_FAIL_COND_V(p_key == Variant(), false);
	ERR_FAIL_COND_V(sd->objects.has(p_key), false);

	if (sd->parent != RID()) {
		full_copy(sd);
	}

	ShapedTextDataAdvanced::Span span;
	span.start = sd->start + sd->text.length();
	span.end = span.start + p_length;
	span.embedded_key = p_key;

	ShapedTextDataAdvanced::EmbeddedObject obj;
	obj.inline_align = p_inline_align;
	obj.rect.size = p_size;
	obj.start = span.start;
	obj.end = span.end;
	obj.baseline = p_baseline;

	sd->spans.push_back(span);
	sd->text = sd->text + String::chr(0xfffc).repeat(p_length);
	sd->end += p_length;
	sd->objects[p_key] = obj;
	invalidate(sd, true);

	return true;
}

bool TextServerAdvanced::_shaped_text_resize_object(const RID &p_shaped, const Variant &p_key, const Size2 &p_size, InlineAlignment p_inline_align, double p_baseline) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	ERR_FAIL_COND_V(!sd->objects.has(p_key), false);
	sd->objects[p_key].rect.size = p_size;
	sd->objects[p_key].inline_align = p_inline_align;
	sd->objects[p_key].baseline = p_baseline;
	if (sd->valid.is_set()) {
		// Recalc string metrics.
		sd->ascent = 0;
		sd->descent = 0;
		sd->width = 0;
		sd->upos = 0;
		sd->uthk = 0;
		int sd_size = sd->glyphs.size();

		for (int i = 0; i < sd_size; i++) {
			Glyph gl = sd->glyphs[i];
			Variant key;
			if (gl.count == 1) {
				for (const KeyValue<Variant, ShapedTextDataAdvanced::EmbeddedObject> &E : sd->objects) {
					if (E.value.start == gl.start) {
						key = E.key;
						break;
					}
				}
			}
			if (key != Variant()) {
				if (sd->orientation == ORIENTATION_HORIZONTAL) {
					sd->objects[key].rect.position.x = sd->width;
					sd->width += sd->objects[key].rect.size.x;
					sd->glyphs.write[i].advance = sd->objects[key].rect.size.x;
				} else {
					sd->objects[key].rect.position.y = sd->width;
					sd->width += sd->objects[key].rect.size.y;
					sd->glyphs.write[i].advance = sd->objects[key].rect.size.y;
				}
			} else {
				if (gl.font_rid.is_valid()) {
					if (sd->orientation == ORIENTATION_HORIZONTAL) {
						sd->ascent = MAX(sd->ascent, MAX(_font_get_ascent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_TOP), -gl.y_off));
						sd->descent = MAX(sd->descent, MAX(_font_get_descent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_BOTTOM), gl.y_off));
					} else {
						sd->ascent = MAX(sd->ascent, Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5));
						sd->descent = MAX(sd->descent, Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5));
					}
					sd->upos = MAX(sd->upos, _font_get_underline_position(gl.font_rid, gl.font_size));
					sd->uthk = MAX(sd->uthk, _font_get_underline_thickness(gl.font_rid, gl.font_size));
				} else if (sd->preserve_invalid || (sd->preserve_control && is_control(gl.index))) {
					// Glyph not found, replace with hex code box.
					if (sd->orientation == ORIENTATION_HORIZONTAL) {
						sd->ascent = MAX(sd->ascent, get_hex_code_box_size(gl.font_size, gl.index).y);
					} else {
						sd->ascent = MAX(sd->ascent, Math::round(get_hex_code_box_size(gl.font_size, gl.index).x * 0.5));
						sd->descent = MAX(sd->descent, Math::round(get_hex_code_box_size(gl.font_size, gl.index).x * 0.5));
					}
				}
				sd->width += gl.advance * gl.repeat;
			}
		}
		sd->sort_valid = false;
		sd->glyphs_logical.clear();
		_realign(sd);
	}
	return true;
}

void TextServerAdvanced::_realign(ShapedTextDataAdvanced *p_sd) const {
	// Align embedded objects to baseline.
	double full_ascent = p_sd->ascent;
	double full_descent = p_sd->descent;
	for (KeyValue<Variant, ShapedTextDataAdvanced::EmbeddedObject> &E : p_sd->objects) {
		if ((E.value.start >= p_sd->start) && (E.value.start < p_sd->end)) {
			if (p_sd->orientation == ORIENTATION_HORIZONTAL) {
				switch (E.value.inline_align & INLINE_ALIGNMENT_TEXT_MASK) {
					case INLINE_ALIGNMENT_TO_TOP: {
						E.value.rect.position.y = -p_sd->ascent;
					} break;
					case INLINE_ALIGNMENT_TO_CENTER: {
						E.value.rect.position.y = (-p_sd->ascent + p_sd->descent) / 2;
					} break;
					case INLINE_ALIGNMENT_TO_BASELINE: {
						E.value.rect.position.y = 0;
					} break;
					case INLINE_ALIGNMENT_TO_BOTTOM: {
						E.value.rect.position.y = p_sd->descent;
					} break;
				}
				switch (E.value.inline_align & INLINE_ALIGNMENT_IMAGE_MASK) {
					case INLINE_ALIGNMENT_BOTTOM_TO: {
						E.value.rect.position.y -= E.value.rect.size.y;
					} break;
					case INLINE_ALIGNMENT_CENTER_TO: {
						E.value.rect.position.y -= E.value.rect.size.y / 2;
					} break;
					case INLINE_ALIGNMENT_BASELINE_TO: {
						E.value.rect.position.y -= E.value.baseline;
					} break;
					case INLINE_ALIGNMENT_TOP_TO: {
						// NOP
					} break;
				}
				full_ascent = MAX(full_ascent, -E.value.rect.position.y);
				full_descent = MAX(full_descent, E.value.rect.position.y + E.value.rect.size.y);
			} else {
				switch (E.value.inline_align & INLINE_ALIGNMENT_TEXT_MASK) {
					case INLINE_ALIGNMENT_TO_TOP: {
						E.value.rect.position.x = -p_sd->ascent;
					} break;
					case INLINE_ALIGNMENT_TO_CENTER: {
						E.value.rect.position.x = (-p_sd->ascent + p_sd->descent) / 2;
					} break;
					case INLINE_ALIGNMENT_TO_BASELINE: {
						E.value.rect.position.x = 0;
					} break;
					case INLINE_ALIGNMENT_TO_BOTTOM: {
						E.value.rect.position.x = p_sd->descent;
					} break;
				}
				switch (E.value.inline_align & INLINE_ALIGNMENT_IMAGE_MASK) {
					case INLINE_ALIGNMENT_BOTTOM_TO: {
						E.value.rect.position.x -= E.value.rect.size.x;
					} break;
					case INLINE_ALIGNMENT_CENTER_TO: {
						E.value.rect.position.x -= E.value.rect.size.x / 2;
					} break;
					case INLINE_ALIGNMENT_BASELINE_TO: {
						E.value.rect.position.x -= E.value.baseline;
					} break;
					case INLINE_ALIGNMENT_TOP_TO: {
						// NOP
					} break;
				}
				full_ascent = MAX(full_ascent, -E.value.rect.position.x);
				full_descent = MAX(full_descent, E.value.rect.position.x + E.value.rect.size.x);
			}
		}
	}
	p_sd->ascent = full_ascent;
	p_sd->descent = full_descent;
}

RID TextServerAdvanced::_shaped_text_substr(const RID &p_shaped, int64_t p_start, int64_t p_length) const {
	_THREAD_SAFE_METHOD_
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, RID());

	MutexLock lock(sd->mutex);
	if (sd->parent != RID()) {
		return _shaped_text_substr(sd->parent, p_start, p_length);
	}
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}
	ERR_FAIL_COND_V(p_start < 0 || p_length < 0, RID());
	ERR_FAIL_COND_V(sd->start > p_start || sd->end < p_start, RID());
	ERR_FAIL_COND_V(sd->end < p_start + p_length, RID());

	ShapedTextDataAdvanced *new_sd = memnew(ShapedTextDataAdvanced);
	new_sd->parent = p_shaped;
	new_sd->start = p_start;
	new_sd->end = p_start + p_length;
	new_sd->orientation = sd->orientation;
	new_sd->direction = sd->direction;
	new_sd->custom_punct = sd->custom_punct;
	new_sd->para_direction = sd->para_direction;
	new_sd->base_para_direction = sd->base_para_direction;
	for (int i = 0; i < TextServer::SPACING_MAX; i++) {
		new_sd->extra_spacing[i] = sd->extra_spacing[i];
	}

	if (!_shape_substr(new_sd, sd, p_start, p_length)) {
		memdelete(new_sd);
		return RID();
	}
	return shaped_owner.make_rid(new_sd);
}

bool TextServerAdvanced::_shape_substr(ShapedTextDataAdvanced *p_new_sd, const ShapedTextDataAdvanced *p_sd, int64_t p_start, int64_t p_length) const {
	if (p_new_sd->valid.is_set()) {
		return true;
	}

	p_new_sd->hb_buffer = hb_buffer_create();

	p_new_sd->line_breaks_valid = p_sd->line_breaks_valid;
	p_new_sd->justification_ops_valid = p_sd->justification_ops_valid;
	p_new_sd->sort_valid = false;
	p_new_sd->upos = p_sd->upos;
	p_new_sd->uthk = p_sd->uthk;

	if (p_length > 0) {
		p_new_sd->text = p_sd->text.substr(p_start - p_sd->start, p_length);
		p_new_sd->utf16 = p_new_sd->text.utf16();
		p_new_sd->script_iter = memnew(ScriptIterator(p_new_sd->text, 0, p_new_sd->text.length()));

		int sd_size = p_sd->glyphs.size();
		const Glyph *sd_glyphs = p_sd->glyphs.ptr();
		for (int ov = 0; ov < p_sd->bidi_override.size(); ov++) {
			UErrorCode err = U_ZERO_ERROR;

			if (p_sd->bidi_override[ov].x >= p_start + p_length || p_sd->bidi_override[ov].y <= p_start) {
				continue;
			}
			int ov_start = _convert_pos_inv(p_sd, p_sd->bidi_override[ov].x);
			int start = MAX(0, _convert_pos_inv(p_sd, p_start) - ov_start);
			int end = MIN(_convert_pos_inv(p_sd, p_start + p_length), _convert_pos_inv(p_sd, p_sd->bidi_override[ov].y)) - ov_start;

			ERR_FAIL_COND_V_MSG((start < 0 || end - start > p_new_sd->utf16.length()), false, "Invalid BiDi override range.");

			// Create temporary line bidi & shape.
			UBiDi *bidi_iter = nullptr;
			if (p_sd->bidi_iter[ov]) {
				bidi_iter = ubidi_openSized(end - start, 0, &err);
				if (U_SUCCESS(err)) {
					ubidi_setLine(p_sd->bidi_iter[ov], start, end, bidi_iter, &err);
					if (U_FAILURE(err)) {
						// Line BiDi failed (string contains incompatible control characters), try full paragraph BiDi instead.
						err = U_ZERO_ERROR;
						const UChar *data = p_sd->utf16.get_data();
						switch (static_cast<TextServer::Direction>(p_sd->bidi_override[ov].z)) {
							case DIRECTION_LTR: {
								ubidi_setPara(bidi_iter, data + start, end - start, UBIDI_LTR, nullptr, &err);
							} break;
							case DIRECTION_RTL: {
								ubidi_setPara(bidi_iter, data + start, end - start, UBIDI_RTL, nullptr, &err);
							} break;
							case DIRECTION_INHERITED: {
								ubidi_setPara(bidi_iter, data + start, end - start, p_sd->base_para_direction, nullptr, &err);
							} break;
							case DIRECTION_AUTO: {
								UBiDiDirection direction = ubidi_getBaseDirection(data + start, end - start);
								if (direction != UBIDI_NEUTRAL) {
									ubidi_setPara(bidi_iter, data + start, end - start, direction, nullptr, &err);
								} else {
									ubidi_setPara(bidi_iter, data + start, end - start, p_sd->base_para_direction, nullptr, &err);
								}
							} break;
						}
						if (U_FAILURE(err)) {
							ubidi_close(bidi_iter);
							bidi_iter = nullptr;
							ERR_PRINT(vformat("BiDi reordering for the line failed: %s", u_errorName(err)));
						}
					}
				} else {
					bidi_iter = nullptr;
					ERR_PRINT(vformat("BiDi iterator allocation for the line failed: %s", u_errorName(err)));
				}
			}
			p_new_sd->bidi_iter.push_back(bidi_iter);

			err = U_ZERO_ERROR;
			int bidi_run_count = 1;
			if (bidi_iter) {
				bidi_run_count = ubidi_countRuns(bidi_iter, &err);
				if (U_FAILURE(err)) {
					ERR_PRINT(u_errorName(err));
				}
			}
			for (int i = 0; i < bidi_run_count; i++) {
				int32_t _bidi_run_start = 0;
				int32_t _bidi_run_length = end - start;
				if (bidi_iter) {
					ubidi_getVisualRun(bidi_iter, i, &_bidi_run_start, &_bidi_run_length);
				}

				int32_t bidi_run_start = _convert_pos(p_sd, ov_start + start + _bidi_run_start);
				int32_t bidi_run_end = _convert_pos(p_sd, ov_start + start + _bidi_run_start + _bidi_run_length);

				for (int j = 0; j < sd_size; j++) {
					if ((sd_glyphs[j].start >= bidi_run_start) && (sd_glyphs[j].end <= bidi_run_end)) {
						// Copy glyphs.
						Glyph gl = sd_glyphs[j];
						if (gl.end == p_start + p_length && ((gl.flags & GRAPHEME_IS_SOFT_HYPHEN) == GRAPHEME_IS_SOFT_HYPHEN)) {
							uint32_t index = font_get_glyph_index(gl.font_rid, gl.font_size, 0x00ad, 0);
							float w = font_get_glyph_advance(gl.font_rid, gl.font_size, index)[(p_new_sd->orientation == ORIENTATION_HORIZONTAL) ? 0 : 1];
							gl.index = index;
							gl.advance = w;
						}
						Variant key;
						bool find_embedded = false;
						if (gl.count == 1) {
							for (const KeyValue<Variant, ShapedTextDataAdvanced::EmbeddedObject> &E : p_sd->objects) {
								if (E.value.start == gl.start) {
									find_embedded = true;
									key = E.key;
									p_new_sd->objects[key] = E.value;
									break;
								}
							}
						}
						if (find_embedded) {
							if (p_new_sd->orientation == ORIENTATION_HORIZONTAL) {
								p_new_sd->objects[key].rect.position.x = p_new_sd->width;
								p_new_sd->width += p_new_sd->objects[key].rect.size.x;
							} else {
								p_new_sd->objects[key].rect.position.y = p_new_sd->width;
								p_new_sd->width += p_new_sd->objects[key].rect.size.y;
							}
						} else {
							if (gl.font_rid.is_valid()) {
								if (p_new_sd->orientation == ORIENTATION_HORIZONTAL) {
									p_new_sd->ascent = MAX(p_new_sd->ascent, MAX(_font_get_ascent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_TOP), -gl.y_off));
									p_new_sd->descent = MAX(p_new_sd->descent, MAX(_font_get_descent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_BOTTOM), gl.y_off));
								} else {
									p_new_sd->ascent = MAX(p_new_sd->ascent, Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5));
									p_new_sd->descent = MAX(p_new_sd->descent, Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5));
								}
							} else if (p_new_sd->preserve_invalid || (p_new_sd->preserve_control && is_control(gl.index))) {
								// Glyph not found, replace with hex code box.
								if (p_new_sd->orientation == ORIENTATION_HORIZONTAL) {
									p_new_sd->ascent = MAX(p_new_sd->ascent, get_hex_code_box_size(gl.font_size, gl.index).y);
								} else {
									p_new_sd->ascent = MAX(p_new_sd->ascent, Math::round(get_hex_code_box_size(gl.font_size, gl.index).x * 0.5));
									p_new_sd->descent = MAX(p_new_sd->descent, Math::round(get_hex_code_box_size(gl.font_size, gl.index).x * 0.5));
								}
							}
							p_new_sd->width += gl.advance * gl.repeat;
						}
						p_new_sd->glyphs.push_back(gl);
					}
				}
			}
		}

		_realign(p_new_sd);
	}
	p_new_sd->valid.set();

	return true;
}

RID TextServerAdvanced::_shaped_text_get_parent(const RID &p_shaped) const {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, RID());

	MutexLock lock(sd->mutex);
	return sd->parent;
}

double TextServerAdvanced::_shaped_text_fit_to_width(const RID &p_shaped, double p_width, BitField<TextServer::JustificationFlag> p_jst_flags) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		_shaped_text_shape(p_shaped);
	}
	if (!sd->justification_ops_valid) {
		_shaped_text_update_justification_ops(p_shaped);
	}

	sd->fit_width_minimum_reached = false;
	int start_pos = 0;
	int end_pos = sd->glyphs.size() - 1;

	if (p_jst_flags.has_flag(JUSTIFICATION_AFTER_LAST_TAB)) {
		int start, end, delta;
		if (sd->para_direction == DIRECTION_LTR) {
			start = sd->glyphs.size() - 1;
			end = -1;
			delta = -1;
		} else {
			start = 0;
			end = sd->glyphs.size();
			delta = +1;
		}

		for (int i = start; i != end; i += delta) {
			if ((sd->glyphs[i].flags & GRAPHEME_IS_TAB) == GRAPHEME_IS_TAB) {
				if (sd->para_direction == DIRECTION_LTR) {
					start_pos = i;
					break;
				} else {
					end_pos = i;
					break;
				}
			}
		}
	}

	double justification_width;
	if (p_jst_flags.has_flag(JUSTIFICATION_CONSTRAIN_ELLIPSIS)) {
		if (sd->overrun_trim_data.trim_pos >= 0) {
			if (sd->para_direction == DIRECTION_RTL) {
				start_pos = sd->overrun_trim_data.trim_pos;
			} else {
				end_pos = sd->overrun_trim_data.trim_pos;
			}
			justification_width = sd->width_trimmed;
		} else {
			return Math::ceil(sd->width);
		}
	} else {
		justification_width = sd->width;
	}

	if (p_jst_flags.has_flag(JUSTIFICATION_TRIM_EDGE_SPACES)) {
		// Trim spaces.
		while ((start_pos < end_pos) && ((sd->glyphs[start_pos].flags & GRAPHEME_IS_SOFT_HYPHEN) != GRAPHEME_IS_SOFT_HYPHEN) && ((sd->glyphs[start_pos].flags & GRAPHEME_IS_SPACE) == GRAPHEME_IS_SPACE || (sd->glyphs[start_pos].flags & GRAPHEME_IS_BREAK_HARD) == GRAPHEME_IS_BREAK_HARD || (sd->glyphs[start_pos].flags & GRAPHEME_IS_BREAK_SOFT) == GRAPHEME_IS_BREAK_SOFT)) {
			justification_width -= sd->glyphs[start_pos].advance * sd->glyphs[start_pos].repeat;
			sd->glyphs.write[start_pos].advance = 0;
			start_pos += sd->glyphs[start_pos].count;
		}
		while ((start_pos < end_pos) && ((sd->glyphs[end_pos].flags & GRAPHEME_IS_SOFT_HYPHEN) != GRAPHEME_IS_SOFT_HYPHEN) && ((sd->glyphs[end_pos].flags & GRAPHEME_IS_SPACE) == GRAPHEME_IS_SPACE || (sd->glyphs[end_pos].flags & GRAPHEME_IS_BREAK_HARD) == GRAPHEME_IS_BREAK_HARD || (sd->glyphs[end_pos].flags & GRAPHEME_IS_BREAK_SOFT) == GRAPHEME_IS_BREAK_SOFT)) {
			justification_width -= sd->glyphs[end_pos].advance * sd->glyphs[end_pos].repeat;
			sd->glyphs.write[end_pos].advance = 0;
			end_pos -= sd->glyphs[end_pos].count;
		}
	} else {
		// Skip breaks, but do not reset size.
		while ((start_pos < end_pos) && ((sd->glyphs[start_pos].flags & GRAPHEME_IS_SOFT_HYPHEN) != GRAPHEME_IS_SOFT_HYPHEN) && ((sd->glyphs[start_pos].flags & GRAPHEME_IS_BREAK_HARD) == GRAPHEME_IS_BREAK_HARD || (sd->glyphs[start_pos].flags & GRAPHEME_IS_BREAK_SOFT) == GRAPHEME_IS_BREAK_SOFT)) {
			start_pos += sd->glyphs[start_pos].count;
		}
		while ((start_pos < end_pos) && ((sd->glyphs[end_pos].flags & GRAPHEME_IS_SOFT_HYPHEN) != GRAPHEME_IS_SOFT_HYPHEN) && ((sd->glyphs[end_pos].flags & GRAPHEME_IS_BREAK_HARD) == GRAPHEME_IS_BREAK_HARD || (sd->glyphs[end_pos].flags & GRAPHEME_IS_BREAK_SOFT) == GRAPHEME_IS_BREAK_SOFT)) {
			end_pos -= sd->glyphs[end_pos].count;
		}
	}

	int space_count = 0;
	int elongation_count = 0;
	for (int i = start_pos; i <= end_pos; i++) {
		const Glyph &gl = sd->glyphs[i];
		if (gl.count > 0) {
			if ((gl.flags & GRAPHEME_IS_ELONGATION) == GRAPHEME_IS_ELONGATION) {
				if ((i > 0) && ((sd->glyphs[i - 1].flags & GRAPHEME_IS_ELONGATION) != GRAPHEME_IS_ELONGATION)) {
					// Expand once per elongation sequence.
					elongation_count++;
				}
			}
			if ((gl.flags & GRAPHEME_IS_SOFT_HYPHEN) != GRAPHEME_IS_SOFT_HYPHEN && (gl.flags & GRAPHEME_IS_SPACE) == GRAPHEME_IS_SPACE && (gl.flags & GRAPHEME_IS_PUNCTUATION) != GRAPHEME_IS_PUNCTUATION) {
				space_count++;
			}
		}
	}

	if ((elongation_count > 0) && p_jst_flags.has_flag(JUSTIFICATION_KASHIDA)) {
		double delta_width_per_kashida = (p_width - justification_width) / elongation_count;
		for (int i = start_pos; i <= end_pos; i++) {
			Glyph &gl = sd->glyphs.write[i];
			if (gl.count > 0) {
				if (((gl.flags & GRAPHEME_IS_ELONGATION) == GRAPHEME_IS_ELONGATION) && (gl.advance > 0)) {
					if ((i > 0) && ((sd->glyphs[i - 1].flags & GRAPHEME_IS_ELONGATION) != GRAPHEME_IS_ELONGATION)) {
						// Expand once per elongation sequence.
						int count = delta_width_per_kashida / gl.advance;
						int prev_count = gl.repeat;
						if ((gl.flags & GRAPHEME_IS_VIRTUAL) == GRAPHEME_IS_VIRTUAL) {
							gl.repeat = CLAMP(count, 0, 255);
						} else {
							gl.repeat = CLAMP(count + 1, 1, 255);
						}
						justification_width += (gl.repeat - prev_count) * gl.advance;
					}
				}
			}
		}
	}
	if ((space_count > 0) && p_jst_flags.has_flag(JUSTIFICATION_WORD_BOUND)) {
		double delta_width_per_space = (p_width - justification_width) / space_count;
		double adv_remain = 0;
		for (int i = start_pos; i <= end_pos; i++) {
			Glyph &gl = sd->glyphs.write[i];
			if (gl.count > 0) {
				if ((gl.flags & GRAPHEME_IS_SOFT_HYPHEN) != GRAPHEME_IS_SOFT_HYPHEN && (gl.flags & GRAPHEME_IS_SPACE) == GRAPHEME_IS_SPACE && (gl.flags & GRAPHEME_IS_PUNCTUATION) != GRAPHEME_IS_PUNCTUATION) {
					double old_adv = gl.advance;
					double new_advance;
					if ((gl.flags & GRAPHEME_IS_VIRTUAL) == GRAPHEME_IS_VIRTUAL) {
						new_advance = MAX(gl.advance + delta_width_per_space, 0.0);
					} else {
						new_advance = MAX(gl.advance + delta_width_per_space, 0.1 * gl.font_size);
					}
					gl.advance = new_advance;
					adv_remain += (new_advance - gl.advance);
					if (adv_remain >= 1.0) {
						gl.advance++;
						adv_remain -= 1.0;
					} else if (adv_remain <= -1.0) {
						gl.advance = MAX(gl.advance - 1, 0);
						adv_remain -= 1.0;
					}
					justification_width += (gl.advance - old_adv);
				}
			}
		}
	}

	if (Math::floor(p_width) < Math::floor(justification_width)) {
		sd->fit_width_minimum_reached = true;
	}

	if (!p_jst_flags.has_flag(JUSTIFICATION_CONSTRAIN_ELLIPSIS)) {
		sd->width = justification_width;
	}

	return Math::ceil(justification_width);
}

double TextServerAdvanced::_shaped_text_tab_align(const RID &p_shaped, const PackedFloat32Array &p_tab_stops) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		_shaped_text_shape(p_shaped);
	}
	if (!sd->line_breaks_valid) {
		_shaped_text_update_breaks(p_shaped);
	}

	for (int i = 0; i < p_tab_stops.size(); i++) {
		if (p_tab_stops[i] <= 0) {
			return 0.0;
		}
	}

	int tab_index = 0;
	double off = 0.0;

	int start, end, delta;
	if (sd->para_direction == DIRECTION_LTR) {
		start = 0;
		end = sd->glyphs.size();
		delta = +1;
	} else {
		start = sd->glyphs.size() - 1;
		end = -1;
		delta = -1;
	}

	Glyph *gl = sd->glyphs.ptrw();

	for (int i = start; i != end; i += delta) {
		if ((gl[i].flags & GRAPHEME_IS_TAB) == GRAPHEME_IS_TAB) {
			double tab_off = 0.0;
			while (tab_off <= off) {
				tab_off += p_tab_stops[tab_index];
				tab_index++;
				if (tab_index >= p_tab_stops.size()) {
					tab_index = 0;
				}
			}
			double old_adv = gl[i].advance;
			gl[i].advance = tab_off - off;
			sd->width += gl[i].advance - old_adv;
			off = 0;
			continue;
		}
		off += gl[i].advance * gl[i].repeat;
	}

	return 0.0;
}

RID TextServerAdvanced::_find_sys_font_for_text(const RID &p_fdef, const String &p_script_code, const String &p_language, const String &p_text) {
	RID f;
	// Try system fallback.
	String font_name = _font_get_name(p_fdef);
	BitField<FontStyle> font_style = _font_get_style(p_fdef);
	int font_weight = _font_get_weight(p_fdef);
	int font_stretch = _font_get_stretch(p_fdef);
	Dictionary dvar = _font_get_variation_coordinates(p_fdef);
	static int64_t wgth_tag = _name_to_tag("weight");
	static int64_t wdth_tag = _name_to_tag("width");
	static int64_t ital_tag = _name_to_tag("italic");
	if (dvar.has(wgth_tag)) {
		font_weight = dvar[wgth_tag].operator int();
	}
	if (dvar.has(wdth_tag)) {
		font_stretch = dvar[wdth_tag].operator int();
	}
	if (dvar.has(ital_tag) && dvar[ital_tag].operator int() == 1) {
		font_style.set_flag(TextServer::FONT_ITALIC);
	}

	String locale = (p_language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale() : p_language;
	PackedStringArray fallback_font_name = OS::get_singleton()->get_system_font_path_for_text(font_name, p_text, locale, p_script_code, font_weight, font_stretch, font_style & TextServer::FONT_ITALIC);
#ifdef GDEXTENSION
	for (int fb = 0; fb < fallback_font_name.size(); fb++) {
		const String &E = fallback_font_name[fb];
#elif defined(GODOT_MODULE)
	for (const String &E : fallback_font_name) {
#endif
		SystemFontKey key = SystemFontKey(E, font_style & TextServer::FONT_ITALIC, font_weight, font_stretch, p_fdef, this);
		if (system_fonts.has(key)) {
			const SystemFontCache &sysf_cache = system_fonts[key];
			int best_score = 0;
			int best_match = -1;
			for (int face_idx = 0; face_idx < sysf_cache.var.size(); face_idx++) {
				const SystemFontCacheRec &F = sysf_cache.var[face_idx];
				if (unlikely(!_font_has_char(F.rid, p_text[0]))) {
					continue;
				}
				BitField<FontStyle> style = _font_get_style(F.rid);
				int weight = _font_get_weight(F.rid);
				int stretch = _font_get_stretch(F.rid);
				int score = (20 - Math::abs(weight - font_weight) / 50);
				score += (20 - Math::abs(stretch - font_stretch) / 10);
				if (bool(style & TextServer::FONT_ITALIC) == bool(font_style & TextServer::FONT_ITALIC)) {
					score += 30;
				}
				if (score >= best_score) {
					best_score = score;
					best_match = face_idx;
				}
				if (best_score == 70) {
					break;
				}
			}
			if (best_match != -1) {
				f = sysf_cache.var[best_match].rid;
			}
		}
		if (!f.is_valid()) {
			if (system_fonts.has(key)) {
				const SystemFontCache &sysf_cache = system_fonts[key];
				if (sysf_cache.max_var == sysf_cache.var.size()) {
					// All subfonts already tested, skip.
					continue;
				}
			}

			if (!system_font_data.has(E)) {
				system_font_data[E] = FileAccess::get_file_as_bytes(E);
			}

			const PackedByteArray &font_data = system_font_data[E];

			SystemFontCacheRec sysf;
			sysf.rid = _create_font();
			_font_set_data_ptr(sysf.rid, font_data.ptr(), font_data.size());
			if (!_font_validate(sysf.rid)) {
				_free_rid(sysf.rid);
				continue;
			}

			Dictionary var = dvar;
			// Select matching style from collection.
			int best_score = 0;
			int best_match = -1;
			for (int face_idx = 0; face_idx < _font_get_face_count(sysf.rid); face_idx++) {
				_font_set_face_index(sysf.rid, face_idx);
				if (unlikely(!_font_has_char(sysf.rid, p_text[0]))) {
					continue;
				}
				BitField<FontStyle> style = _font_get_style(sysf.rid);
				int weight = _font_get_weight(sysf.rid);
				int stretch = _font_get_stretch(sysf.rid);
				int score = (20 - Math::abs(weight - font_weight) / 50);
				score += (20 - Math::abs(stretch - font_stretch) / 10);
				if (bool(style & TextServer::FONT_ITALIC) == bool(font_style & TextServer::FONT_ITALIC)) {
					score += 30;
				}
				if (score >= best_score) {
					best_score = score;
					best_match = face_idx;
				}
				if (best_score == 70) {
					break;
				}
			}
			if (best_match == -1) {
				_free_rid(sysf.rid);
				continue;
			} else {
				_font_set_face_index(sysf.rid, best_match);
			}
			sysf.index = best_match;

			// If it's a variable font, apply weight, stretch and italic coordinates to match requested style.
			if (best_score != 70) {
				Dictionary ftr = _font_supported_variation_list(sysf.rid);
				if (ftr.has(wdth_tag)) {
					var[wdth_tag] = font_stretch;
					_font_set_stretch(sysf.rid, font_stretch);
				}
				if (ftr.has(wgth_tag)) {
					var[wgth_tag] = font_weight;
					_font_set_weight(sysf.rid, font_weight);
				}
				if ((font_style & TextServer::FONT_ITALIC) && ftr.has(ital_tag)) {
					var[ital_tag] = 1;
					_font_set_style(sysf.rid, _font_get_style(sysf.rid) | TextServer::FONT_ITALIC);
				}
			}

			_font_set_antialiasing(sysf.rid, key.antialiasing);
			_font_set_disable_embedded_bitmaps(sysf.rid, key.disable_embedded_bitmaps);
			_font_set_generate_mipmaps(sysf.rid, key.mipmaps);
			_font_set_multichannel_signed_distance_field(sysf.rid, key.msdf);
			_font_set_msdf_pixel_range(sysf.rid, key.msdf_range);
			_font_set_msdf_size(sysf.rid, key.msdf_source_size);
			_font_set_fixed_size(sysf.rid, key.fixed_size);
			_font_set_force_autohinter(sysf.rid, key.force_autohinter);
			_font_set_hinting(sysf.rid, key.hinting);
			_font_set_subpixel_positioning(sysf.rid, key.subpixel_positioning);
			_font_set_keep_rounding_remainders(sysf.rid, key.keep_rounding_remainders);
			_font_set_variation_coordinates(sysf.rid, var);
			_font_set_oversampling(sysf.rid, key.oversampling);
			_font_set_embolden(sysf.rid, key.embolden);
			_font_set_transform(sysf.rid, key.transform);
			_font_set_spacing(sysf.rid, SPACING_TOP, key.extra_spacing[SPACING_TOP]);
			_font_set_spacing(sysf.rid, SPACING_BOTTOM, key.extra_spacing[SPACING_BOTTOM]);
			_font_set_spacing(sysf.rid, SPACING_SPACE, key.extra_spacing[SPACING_SPACE]);
			_font_set_spacing(sysf.rid, SPACING_GLYPH, key.extra_spacing[SPACING_GLYPH]);

			if (system_fonts.has(key)) {
				system_fonts[key].var.push_back(sysf);
			} else {
				SystemFontCache &sysf_cache = system_fonts[key];
				sysf_cache.max_var = _font_get_face_count(sysf.rid);
				sysf_cache.var.push_back(sysf);
			}
			f = sysf.rid;
		}
		break;
	}
	return f;
}

void TextServerAdvanced::_shaped_text_overrun_trim_to_width(const RID &p_shaped_line, double p_width, BitField<TextServer::TextOverrunFlag> p_trim_flags) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped_line);
	ERR_FAIL_NULL_MSG(sd, "ShapedTextDataAdvanced invalid.");

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		_shaped_text_shape(p_shaped_line);
	}

	sd->text_trimmed = false;
	sd->overrun_trim_data.ellipsis_glyph_buf.clear();

	bool add_ellipsis = p_trim_flags.has_flag(OVERRUN_ADD_ELLIPSIS);
	bool cut_per_word = p_trim_flags.has_flag(OVERRUN_TRIM_WORD_ONLY);
	bool enforce_ellipsis = p_trim_flags.has_flag(OVERRUN_ENFORCE_ELLIPSIS);
	bool justification_aware = p_trim_flags.has_flag(OVERRUN_JUSTIFICATION_AWARE);

	Glyph *sd_glyphs = sd->glyphs.ptrw();

	if ((p_trim_flags & OVERRUN_TRIM) == OVERRUN_NO_TRIM || sd_glyphs == nullptr || p_width <= 0 || !(sd->width > p_width || enforce_ellipsis)) {
		sd->overrun_trim_data.trim_pos = -1;
		sd->overrun_trim_data.ellipsis_pos = -1;
		return;
	}

	if (justification_aware && !sd->fit_width_minimum_reached) {
		return;
	}

	Vector<ShapedTextDataAdvanced::Span> &spans = sd->spans;
	if (sd->parent != RID()) {
		ShapedTextDataAdvanced *parent_sd = shaped_owner.get_or_null(sd->parent);
		ERR_FAIL_COND(!parent_sd->valid.is_set());
		spans = parent_sd->spans;
	}

	if (spans.size() == 0) {
		return;
	}

	int sd_size = sd->glyphs.size();
	int last_gl_font_size = sd_glyphs[sd_size - 1].font_size;
	bool found_el_char = false;

	// Find usable fonts, if fonts from the last glyph do not have required chars.
	RID dot_gl_font_rid = sd_glyphs[sd_size - 1].font_rid;
	if (add_ellipsis || enforce_ellipsis) {
		if (!_font_has_char(dot_gl_font_rid, sd->el_char)) {
			const Array &fonts = spans[spans.size() - 1].fonts;
			for (int i = 0; i < fonts.size(); i++) {
				if (_font_has_char(fonts[i], sd->el_char)) {
					dot_gl_font_rid = fonts[i];
					found_el_char = true;
					break;
				}
			}
			if (!found_el_char && OS::get_singleton()->has_feature("system_fonts") && fonts.size() > 0 && _font_is_allow_system_fallback(fonts[0])) {
				const char32_t u32str[] = { sd->el_char, 0 };
				RID rid = _find_sys_font_for_text(fonts[0], String(), spans[spans.size() - 1].language, u32str);
				if (rid.is_valid()) {
					dot_gl_font_rid = rid;
					found_el_char = true;
				}
			}
		} else {
			found_el_char = true;
		}
		if (!found_el_char) {
			bool found_dot_char = false;
			dot_gl_font_rid = sd_glyphs[sd_size - 1].font_rid;
			if (!_font_has_char(dot_gl_font_rid, '.')) {
				const Array &fonts = spans[spans.size() - 1].fonts;
				for (int i = 0; i < fonts.size(); i++) {
					if (_font_has_char(fonts[i], '.')) {
						dot_gl_font_rid = fonts[i];
						found_dot_char = true;
						break;
					}
				}
				if (!found_dot_char && OS::get_singleton()->has_feature("system_fonts") && fonts.size() > 0 && _font_is_allow_system_fallback(fonts[0])) {
					RID rid = _find_sys_font_for_text(fonts[0], String(), spans[spans.size() - 1].language, ".");
					if (rid.is_valid()) {
						dot_gl_font_rid = rid;
					}
				}
			}
		}
	}
	RID whitespace_gl_font_rid = sd_glyphs[sd_size - 1].font_rid;
	if (!_font_has_char(whitespace_gl_font_rid, ' ')) {
		const Array &fonts = spans[spans.size() - 1].fonts;
		for (int i = 0; i < fonts.size(); i++) {
			if (_font_has_char(fonts[i], ' ')) {
				whitespace_gl_font_rid = fonts[i];
				break;
			}
		}
	}

	int32_t dot_gl_idx = ((add_ellipsis || enforce_ellipsis) && dot_gl_font_rid.is_valid()) ? _font_get_glyph_index(dot_gl_font_rid, last_gl_font_size, (found_el_char ? sd->el_char : '.'), 0) : -1;
	Vector2 dot_adv = ((add_ellipsis || enforce_ellipsis) && dot_gl_font_rid.is_valid()) ? _font_get_glyph_advance(dot_gl_font_rid, last_gl_font_size, dot_gl_idx) : Vector2();
	int32_t whitespace_gl_idx = whitespace_gl_font_rid.is_valid() ? _font_get_glyph_index(whitespace_gl_font_rid, last_gl_font_size, ' ', 0) : -1;
	Vector2 whitespace_adv = whitespace_gl_font_rid.is_valid() ? _font_get_glyph_advance(whitespace_gl_font_rid, last_gl_font_size, whitespace_gl_idx) : Vector2();

	int ellipsis_width = 0;
	if (add_ellipsis && whitespace_gl_font_rid.is_valid()) {
		ellipsis_width = (found_el_char ? 1 : 3) * dot_adv.x + sd->extra_spacing[SPACING_GLYPH] + _font_get_spacing(dot_gl_font_rid, SPACING_GLYPH) + (cut_per_word ? whitespace_adv.x : 0);
	}

	int ell_min_characters = 6;
	double width = sd->width;

	bool is_rtl = sd->para_direction == DIRECTION_RTL;

	int trim_pos = (is_rtl) ? sd_size : 0;
	int ellipsis_pos = (enforce_ellipsis) ? 0 : -1;

	int last_valid_cut = 0;
	bool found = false;

	int glyphs_from = (is_rtl) ? 0 : sd_size - 1;
	int glyphs_to = (is_rtl) ? sd_size - 1 : -1;
	int glyphs_delta = (is_rtl) ? +1 : -1;

	if (enforce_ellipsis && (width + ellipsis_width <= p_width)) {
		trim_pos = -1;
		ellipsis_pos = (is_rtl) ? 0 : sd_size;
	} else {
		for (int i = glyphs_from; i != glyphs_to; i += glyphs_delta) {
			if (!is_rtl) {
				width -= sd_glyphs[i].advance * sd_glyphs[i].repeat;
			}
			if (sd_glyphs[i].count > 0) {
				bool above_min_char_threshold = ((is_rtl) ? sd_size - 1 - i : i) >= ell_min_characters;

				if (width + (((above_min_char_threshold && add_ellipsis) || enforce_ellipsis) ? ellipsis_width : 0) <= p_width) {
					if (cut_per_word && above_min_char_threshold) {
						if ((sd_glyphs[i].flags & GRAPHEME_IS_BREAK_SOFT) == GRAPHEME_IS_BREAK_SOFT) {
							last_valid_cut = i;
							found = true;
						}
					} else {
						last_valid_cut = i;
						found = true;
					}
					if (found) {
						trim_pos = last_valid_cut;

						if (add_ellipsis && (above_min_char_threshold || enforce_ellipsis) && width - ellipsis_width <= p_width) {
							ellipsis_pos = trim_pos;
						}
						break;
					}
				}
			}
			if (is_rtl) {
				width -= sd_glyphs[i].advance * sd_glyphs[i].repeat;
			}
		}
	}

	sd->overrun_trim_data.trim_pos = trim_pos;
	sd->overrun_trim_data.ellipsis_pos = ellipsis_pos;
	if (trim_pos == 0 && enforce_ellipsis && add_ellipsis) {
		sd->overrun_trim_data.ellipsis_pos = 0;
	}

	if ((trim_pos >= 0 && sd->width > p_width) || enforce_ellipsis) {
		if (add_ellipsis && (ellipsis_pos > 0 || enforce_ellipsis)) {
			// Insert an additional space when cutting word bound for aesthetics.
			if (cut_per_word && (ellipsis_pos > 0)) {
				Glyph gl;
				gl.count = 1;
				gl.advance = whitespace_adv.x;
				gl.index = whitespace_gl_idx;
				gl.font_rid = whitespace_gl_font_rid;
				gl.font_size = last_gl_font_size;
				gl.flags = GRAPHEME_IS_SPACE | GRAPHEME_IS_BREAK_SOFT | GRAPHEME_IS_VIRTUAL | (is_rtl ? GRAPHEME_IS_RTL : 0);

				sd->overrun_trim_data.ellipsis_glyph_buf.append(gl);
			}
			// Add ellipsis dots.
			if (dot_gl_idx != 0) {
				Glyph gl;
				gl.count = 1;
				gl.repeat = (found_el_char ? 1 : 3);
				gl.advance = dot_adv.x;
				gl.index = dot_gl_idx;
				gl.font_rid = dot_gl_font_rid;
				gl.font_size = last_gl_font_size;
				gl.flags = GRAPHEME_IS_PUNCTUATION | GRAPHEME_IS_VIRTUAL | (is_rtl ? GRAPHEME_IS_RTL : 0);

				sd->overrun_trim_data.ellipsis_glyph_buf.append(gl);
			}
		}

		sd->text_trimmed = true;
		sd->width_trimmed = width + ((ellipsis_pos != -1) ? ellipsis_width : 0);
	}
}

int64_t TextServerAdvanced::_shaped_text_get_trim_pos(const RID &p_shaped) const {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V_MSG(sd, -1, "ShapedTextDataAdvanced invalid.");

	MutexLock lock(sd->mutex);
	return sd->overrun_trim_data.trim_pos;
}

int64_t TextServerAdvanced::_shaped_text_get_ellipsis_pos(const RID &p_shaped) const {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V_MSG(sd, -1, "ShapedTextDataAdvanced invalid.");

	MutexLock lock(sd->mutex);
	return sd->overrun_trim_data.ellipsis_pos;
}

const Glyph *TextServerAdvanced::_shaped_text_get_ellipsis_glyphs(const RID &p_shaped) const {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V_MSG(sd, nullptr, "ShapedTextDataAdvanced invalid.");

	MutexLock lock(sd->mutex);
	return sd->overrun_trim_data.ellipsis_glyph_buf.ptr();
}

int64_t TextServerAdvanced::_shaped_text_get_ellipsis_glyph_count(const RID &p_shaped) const {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V_MSG(sd, 0, "ShapedTextDataAdvanced invalid.");

	MutexLock lock(sd->mutex);
	return sd->overrun_trim_data.ellipsis_glyph_buf.size();
}

void TextServerAdvanced::_update_chars(ShapedTextDataAdvanced *p_sd) const {
	if (!p_sd->chars_valid) {
		p_sd->chars.clear();

		const UChar *data = p_sd->utf16.get_data();
		UErrorCode err = U_ZERO_ERROR;
		int prev = -1;
		int i = 0;

		Vector<ShapedTextDataAdvanced::Span> &spans = p_sd->spans;
		if (p_sd->parent != RID()) {
			ShapedTextDataAdvanced *parent_sd = shaped_owner.get_or_null(p_sd->parent);
			ERR_FAIL_COND(!parent_sd->valid.is_set());
			spans = parent_sd->spans;
		}

		while (i < spans.size()) {
			if (spans[i].start > p_sd->end) {
				break;
			}
			if (spans[i].end < p_sd->start) {
				i++;
				continue;
			}

			int r_start = MAX(0, spans[i].start - p_sd->start);
			String language = spans[i].language;
			while (i + 1 < spans.size() && language == spans[i + 1].language) {
				i++;
			}
			int r_end = MIN(spans[i].end - p_sd->start, p_sd->text.length());
			UBreakIterator *bi = ubrk_open(UBRK_CHARACTER, (language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale().ascii().get_data() : language.ascii().get_data(), data + _convert_pos_inv(p_sd, r_start), _convert_pos_inv(p_sd, r_end - r_start), &err);
			if (U_SUCCESS(err)) {
				while (ubrk_next(bi) != UBRK_DONE) {
					int pos = _convert_pos(p_sd, ubrk_current(bi)) + r_start + p_sd->start;
					if (prev != pos) {
						p_sd->chars.push_back(pos);
					}
					prev = pos;
				}
				ubrk_close(bi);
			} else {
				for (int j = r_start; j < r_end; j++) {
					if (prev != j) {
						p_sd->chars.push_back(j + 1 + p_sd->start);
					}
					prev = j;
				}
			}
			i++;
		}
		p_sd->chars_valid = true;
	}
}

PackedInt32Array TextServerAdvanced::_shaped_text_get_character_breaks(const RID &p_shaped) const {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, PackedInt32Array());

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}

	_update_chars(sd);

	return sd->chars;
}

bool TextServerAdvanced::_shaped_text_update_breaks(const RID &p_shaped) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		_shaped_text_shape(p_shaped);
	}

	if (sd->line_breaks_valid) {
		return true; // Nothing to do.
	}

	const UChar *data = sd->utf16.get_data();

	if (!sd->break_ops_valid) {
		sd->breaks.clear();
		sd->break_inserts = 0;
		UErrorCode err = U_ZERO_ERROR;
		int i = 0;
		while (i < sd->spans.size()) {
			String language = sd->spans[i].language;
			int r_start = sd->spans[i].start;
			while (i + 1 < sd->spans.size() && language == sd->spans[i + 1].language) {
				i++;
			}
			int r_end = sd->spans[i].end;
			UBreakIterator *bi = ubrk_open(UBRK_LINE, (language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale().ascii().get_data() : language.ascii().get_data(), data + _convert_pos_inv(sd, r_start), _convert_pos_inv(sd, r_end - r_start), &err);
			if (U_FAILURE(err)) {
				// No data loaded - use fallback.
				for (int j = r_start; j < r_end; j++) {
					char32_t c = sd->text[j - sd->start];
					char32_t c_next = (j < r_end) ? sd->text[j - sd->start + 1] : 0x0000;
					if (is_whitespace(c)) {
						sd->breaks[j + 1] = false;
					}
					if (is_linebreak(c)) {
						if (c != 0x000D || c_next != 0x000A) { // Skip first hard break in CR-LF pair.
							sd->breaks[j + 1] = true;
						}
					}
				}
			} else {
				while (ubrk_next(bi) != UBRK_DONE) {
					int pos = _convert_pos(sd, ubrk_current(bi)) + r_start;
					if ((ubrk_getRuleStatus(bi) >= UBRK_LINE_HARD) && (ubrk_getRuleStatus(bi) < UBRK_LINE_HARD_LIMIT)) {
						sd->breaks[pos] = true;
					} else if ((ubrk_getRuleStatus(bi) >= UBRK_LINE_SOFT) && (ubrk_getRuleStatus(bi) < UBRK_LINE_SOFT_LIMIT)) {
						sd->breaks[pos] = false;
					}
					int pos_p = pos - 1 - sd->start;
					char32_t c = sd->text[pos_p];
					if (pos - sd->start != sd->end && !is_whitespace(c) && (c != 0xfffc)) {
						sd->break_inserts++;
					}
				}
			}
			ubrk_close(bi);
			i++;
		}
		sd->break_ops_valid = true;
	}

	Vector<Glyph> glyphs_new;

	bool rewrite = false;
	int sd_shift = 0;
	int sd_size = sd->glyphs.size();
	Glyph *sd_glyphs = sd->glyphs.ptrw();
	Glyph *sd_glyphs_new = nullptr;

	if (sd->break_inserts > 0) {
		glyphs_new.resize(sd->glyphs.size() + sd->break_inserts);
		sd_glyphs_new = glyphs_new.ptrw();
		rewrite = true;
	} else {
		sd_glyphs_new = sd_glyphs;
	}

	sd->sort_valid = false;
	sd->glyphs_logical.clear();
	const char32_t *ch = sd->text.ptr();

	int c_punct_size = sd->custom_punct.length();
	const char32_t *c_punct = sd->custom_punct.ptr();

	for (int i = 0; i < sd_size; i++) {
		if (rewrite) {
			for (int j = 0; j < sd_glyphs[i].count; j++) {
				sd_glyphs_new[sd_shift + i + j] = sd_glyphs[i + j];
			}
		}
		if (sd_glyphs[i].count > 0) {
			char32_t c = ch[sd_glyphs[i].start - sd->start];
			if (c == 0xfffc) {
				i += (sd_glyphs[i].count - 1);
				continue;
			}
			if (c == 0x0009 || c == 0x000b) {
				sd_glyphs_new[sd_shift + i].flags |= GRAPHEME_IS_TAB;
			}
			if (c == 0x00ad) {
				sd_glyphs_new[sd_shift + i].flags |= GRAPHEME_IS_SOFT_HYPHEN;
			}
			if (is_whitespace(c)) {
				sd_glyphs_new[sd_shift + i].flags |= GRAPHEME_IS_SPACE;
			}
			if (c_punct_size == 0) {
				if (u_ispunct(c) && c != 0x005f) {
					sd_glyphs_new[sd_shift + i].flags |= GRAPHEME_IS_PUNCTUATION;
				}
			} else {
				for (int j = 0; j < c_punct_size; j++) {
					if (c_punct[j] == c) {
						sd_glyphs_new[sd_shift + i].flags |= GRAPHEME_IS_PUNCTUATION;
						break;
					}
				}
			}
			if (is_underscore(c)) {
				sd_glyphs_new[sd_shift + i].flags |= GRAPHEME_IS_UNDERSCORE;
			}
			if (sd->breaks.has(sd_glyphs[i].end)) {
				if (sd->breaks[sd_glyphs[i].end] && (is_linebreak(c))) {
					sd_glyphs_new[sd_shift + i].flags |= GRAPHEME_IS_BREAK_HARD;
				} else if (is_whitespace(c) || c == 0x00ad) {
					sd_glyphs_new[sd_shift + i].flags |= GRAPHEME_IS_BREAK_SOFT;
				} else {
					int count = sd_glyphs[i].count;
					// Do not add extra space at the end of the line.
					if (sd_glyphs[i].end == sd->end) {
						i += (sd_glyphs[i].count - 1);
						continue;
					}
					// Do not add extra space after existing space.
					if (sd_glyphs[i].flags & GRAPHEME_IS_RTL) {
						if ((i + count < sd_size - 1) && ((sd_glyphs[i + count].flags & (GRAPHEME_IS_SPACE | GRAPHEME_IS_BREAK_SOFT)) == (GRAPHEME_IS_SPACE | GRAPHEME_IS_BREAK_SOFT))) {
							i += (sd_glyphs[i].count - 1);
							continue;
						}
					} else {
						if ((sd_glyphs[i].flags & (GRAPHEME_IS_SPACE | GRAPHEME_IS_BREAK_SOFT)) == (GRAPHEME_IS_SPACE | GRAPHEME_IS_BREAK_SOFT)) {
							i += (sd_glyphs[i].count - 1);
							continue;
						}
					}
					Glyph gl;
					gl.start = sd_glyphs[i].start;
					gl.end = sd_glyphs[i].end;
					gl.count = 1;
					gl.font_rid = sd_glyphs[i].font_rid;
					gl.font_size = sd_glyphs[i].font_size;
					gl.flags = GRAPHEME_IS_BREAK_SOFT | GRAPHEME_IS_VIRTUAL | GRAPHEME_IS_SPACE;
					// Mark virtual space after punctuation as punctuation to avoid justification at this point.
					if (c_punct_size == 0) {
						if (u_ispunct(c) && c != 0x005f) {
							gl.flags |= GRAPHEME_IS_PUNCTUATION;
						}
					} else {
						for (int j = 0; j < c_punct_size; j++) {
							if (c_punct[j] == c) {
								gl.flags |= GRAPHEME_IS_PUNCTUATION;
								break;
							}
						}
					}
					if (sd_glyphs[i].flags & GRAPHEME_IS_RTL) {
						gl.flags |= GRAPHEME_IS_RTL;
						for (int j = sd_glyphs[i].count - 1; j >= 0; j--) {
							sd_glyphs_new[sd_shift + i + j + 1] = sd_glyphs_new[sd_shift + i + j];
						}
						sd_glyphs_new[sd_shift + i] = gl;
					} else {
						sd_glyphs_new[sd_shift + i + count] = gl;
					}
					sd_shift++;
					ERR_FAIL_COND_V_MSG(sd_shift > sd->break_inserts, false, "Invalid break insert count!");
				}
			}
			i += (sd_glyphs[i].count - 1);
		}
	}
	if (sd_shift < sd->break_inserts) {
		// Note: should not happen with a normal text, but might be a case with special fonts that substitute a long string (with breaks opportunities in it) with a single glyph (like Font Awesome).
		glyphs_new.resize(sd->glyphs.size() + sd_shift);
	}

	if (sd->break_inserts > 0) {
		sd->glyphs = glyphs_new;
	}

	sd->line_breaks_valid = true;

	return sd->line_breaks_valid;
}

_FORCE_INLINE_ int64_t _generate_kashida_justification_opportunies(const String &p_data, int64_t p_start, int64_t p_end) {
	int64_t kashida_pos = -1;
	int8_t priority = 100;
	int64_t i = p_start;

	char32_t pc = 0;

	while ((p_end > p_start) && is_transparent(p_data[p_end - 1])) {
		p_end--;
	}

	while (i < p_end) {
		uint32_t c = p_data[i];

		if (c == 0x0640) {
			kashida_pos = i;
			priority = 0;
		}
		if (priority >= 1 && i < p_end - 1) {
			if (is_seen_sad(c) && (p_data[i + 1] != 0x200c)) {
				kashida_pos = i;
				priority = 1;
			}
		}
		if (priority >= 2 && i > p_start) {
			if (is_teh_marbuta(c) || is_dal(c) || (is_heh(c) && i == p_end - 1)) {
				if (is_connected_to_prev(c, pc)) {
					kashida_pos = i - 1;
					priority = 2;
				}
			}
		}
		if (priority >= 3 && i > p_start) {
			if (is_alef(c) || ((is_lam(c) || is_tah(c) || is_kaf(c) || is_gaf(c)) && i == p_end - 1)) {
				if (is_connected_to_prev(c, pc)) {
					kashida_pos = i - 1;
					priority = 3;
				}
			}
		}
		if (priority >= 4 && i > p_start && i < p_end - 1) {
			if (is_beh(c)) {
				if (is_reh(p_data[i + 1]) || is_yeh(p_data[i + 1])) {
					if (is_connected_to_prev(c, pc)) {
						kashida_pos = i - 1;
						priority = 4;
					}
				}
			}
		}
		if (priority >= 5 && i > p_start) {
			if (is_waw(c) || ((is_ain(c) || is_qaf(c) || is_feh(c)) && i == p_end - 1)) {
				if (is_connected_to_prev(c, pc)) {
					kashida_pos = i - 1;
					priority = 5;
				}
			}
		}
		if (priority >= 6 && i > p_start) {
			if (is_reh(c)) {
				if (is_connected_to_prev(c, pc)) {
					kashida_pos = i - 1;
					priority = 6;
				}
			}
		}
		if (!is_transparent(c)) {
			pc = c;
		}
		i++;
	}

	return kashida_pos;
}

bool TextServerAdvanced::_shaped_text_update_justification_ops(const RID &p_shaped) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		_shaped_text_shape(p_shaped);
	}
	if (!sd->line_breaks_valid) {
		_shaped_text_update_breaks(p_shaped);
	}

	if (sd->justification_ops_valid) {
		return true; // Nothing to do.
	}

	const UChar *data = sd->utf16.get_data();
	int data_size = sd->utf16.length();

	if (!sd->js_ops_valid) {
		sd->jstops.clear();

		// Use ICU word iterator and custom kashida detection.
		UErrorCode err = U_ZERO_ERROR;
		UBreakIterator *bi = ubrk_open(UBRK_WORD, "", data, data_size, &err);
		if (U_FAILURE(err)) {
			// No data - use fallback.
			int limit = 0;
			for (int i = 0; i < sd->text.length(); i++) {
				if (is_whitespace(data[i])) {
					int ks = _generate_kashida_justification_opportunies(sd->text, limit, i) + sd->start;
					if (ks != -1) {
						sd->jstops[ks] = true;
					}
					limit = i + 1;
				}
			}
			int ks = _generate_kashida_justification_opportunies(sd->text, limit, sd->text.length()) + sd->start;
			if (ks != -1) {
				sd->jstops[ks] = true;
			}
		} else {
			int limit = 0;
			while (ubrk_next(bi) != UBRK_DONE) {
				if (ubrk_getRuleStatus(bi) != UBRK_WORD_NONE) {
					int i = _convert_pos(sd, ubrk_current(bi));
					sd->jstops[i + sd->start] = false;
					int ks = _generate_kashida_justification_opportunies(sd->text, limit, i);
					if (ks != -1) {
						sd->jstops[ks + sd->start] = true;
					}
					limit = i;
				}
			}
			ubrk_close(bi);
		}

		sd->js_ops_valid = true;
	}

	sd->sort_valid = false;
	sd->glyphs_logical.clear();

	Glyph *sd_glyphs = sd->glyphs.ptrw();
	int sd_size = sd->glyphs.size();
	if (!sd->jstops.is_empty()) {
		for (int i = 0; i < sd_size; i++) {
			if (sd_glyphs[i].count > 0) {
				char32_t c = sd->text[sd_glyphs[i].start - sd->start];
				if (c == 0x0640 && sd_glyphs[i].start == sd_glyphs[i].end - 1) {
					sd_glyphs[i].flags |= GRAPHEME_IS_ELONGATION;
				}
				if (sd->jstops.has(sd_glyphs[i].start)) {
					if (c == 0xfffc || c == 0x00ad) {
						continue;
					}
					if (sd->jstops[sd_glyphs[i].start]) {
						if (c != 0x0640) {
							if (sd_glyphs[i].font_rid != RID()) {
								Glyph gl = _shape_single_glyph(sd, 0x0640, HB_SCRIPT_ARABIC, HB_DIRECTION_RTL, sd->glyphs[i].font_rid, sd->glyphs[i].font_size);
								if ((sd_glyphs[i].flags & GRAPHEME_IS_VALID) == GRAPHEME_IS_VALID) {
#if HB_VERSION_ATLEAST(5, 1, 0)
									if ((i > 0) && ((sd_glyphs[i - 1].flags & GRAPHEME_IS_SAFE_TO_INSERT_TATWEEL) != GRAPHEME_IS_SAFE_TO_INSERT_TATWEEL)) {
										continue;
									}
#endif
									gl.start = sd_glyphs[i].start;
									gl.end = sd_glyphs[i].end;
									gl.repeat = 0;
									gl.count = 1;
									if (sd->orientation == ORIENTATION_HORIZONTAL) {
										gl.y_off = sd_glyphs[i].y_off;
									} else {
										gl.x_off = sd_glyphs[i].x_off;
									}
									gl.flags |= GRAPHEME_IS_ELONGATION | GRAPHEME_IS_VIRTUAL;
									sd->glyphs.insert(i, gl);
									i++;

									// Update write pointer and size.
									sd_size = sd->glyphs.size();
									sd_glyphs = sd->glyphs.ptrw();
									continue;
								}
							}
						}
					} else if ((sd_glyphs[i].flags & GRAPHEME_IS_SPACE) != GRAPHEME_IS_SPACE && (sd_glyphs[i].flags & GRAPHEME_IS_PUNCTUATION) != GRAPHEME_IS_PUNCTUATION) {
						int count = sd_glyphs[i].count;
						// Do not add extra spaces at the end of the line.
						if (sd_glyphs[i].end == sd->end) {
							continue;
						}
						// Do not add extra space after existing space.
						if (sd_glyphs[i].flags & GRAPHEME_IS_RTL) {
							if ((i + count < sd_size - 1) && ((sd_glyphs[i + count].flags & (GRAPHEME_IS_SPACE | GRAPHEME_IS_BREAK_SOFT)) == (GRAPHEME_IS_SPACE | GRAPHEME_IS_BREAK_SOFT))) {
								continue;
							}
						} else {
							if ((i > 0) && ((sd_glyphs[i - 1].flags & (GRAPHEME_IS_SPACE | GRAPHEME_IS_BREAK_SOFT)) == (GRAPHEME_IS_SPACE | GRAPHEME_IS_BREAK_SOFT))) {
								continue;
							}
						}
						// Inject virtual space for alignment.
						Glyph gl;
						gl.start = sd_glyphs[i].start;
						gl.end = sd_glyphs[i].end;
						gl.count = 1;
						gl.font_rid = sd_glyphs[i].font_rid;
						gl.font_size = sd_glyphs[i].font_size;
						gl.flags = GRAPHEME_IS_SPACE | GRAPHEME_IS_VIRTUAL;
						if (sd_glyphs[i].flags & GRAPHEME_IS_RTL) {
							gl.flags |= GRAPHEME_IS_RTL;
							sd->glyphs.insert(i, gl); // Insert before.
						} else {
							sd->glyphs.insert(i + count, gl); // Insert after.
						}
						i += count;

						// Update write pointer and size.
						sd_size = sd->glyphs.size();
						sd_glyphs = sd->glyphs.ptrw();
						continue;
					}
				}
			}
		}
	}

	sd->justification_ops_valid = true;
	return sd->justification_ops_valid;
}

Glyph TextServerAdvanced::_shape_single_glyph(ShapedTextDataAdvanced *p_sd, char32_t p_char, hb_script_t p_script, hb_direction_t p_direction, const RID &p_font, int64_t p_font_size) {
	hb_font_t *hb_font = _font_get_hb_handle(p_font, p_font_size);
	double scale = _font_get_scale(p_font, p_font_size);
	bool subpos = (scale != 1.0) || (_font_get_subpixel_positioning(p_font) == SUBPIXEL_POSITIONING_ONE_HALF) || (_font_get_subpixel_positioning(p_font) == SUBPIXEL_POSITIONING_ONE_QUARTER) || (_font_get_subpixel_positioning(p_font) == SUBPIXEL_POSITIONING_AUTO && p_font_size <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE);
	ERR_FAIL_NULL_V(hb_font, Glyph());

	hb_buffer_clear_contents(p_sd->hb_buffer);
	hb_buffer_set_direction(p_sd->hb_buffer, p_direction);
	hb_buffer_set_flags(p_sd->hb_buffer, (hb_buffer_flags_t)(HB_BUFFER_FLAG_DEFAULT));
	hb_buffer_set_script(p_sd->hb_buffer, p_script);
	hb_buffer_add_utf32(p_sd->hb_buffer, (const uint32_t *)&p_char, 1, 0, 1);

	hb_shape(hb_font, p_sd->hb_buffer, nullptr, 0);

	unsigned int glyph_count = 0;
	hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(p_sd->hb_buffer, &glyph_count);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(p_sd->hb_buffer, &glyph_count);

	// Process glyphs.
	Glyph gl;

	if (p_direction == HB_DIRECTION_RTL || p_direction == HB_DIRECTION_BTT) {
		gl.flags |= TextServer::GRAPHEME_IS_RTL;
	}

	gl.font_rid = p_font;
	gl.font_size = p_font_size;

	if (glyph_count > 0) {
		if (p_sd->orientation == ORIENTATION_HORIZONTAL) {
			if (subpos) {
				gl.advance = (double)glyph_pos[0].x_advance / (64.0 / scale) + _get_extra_advance(p_font, p_font_size);
			} else {
				gl.advance = Math::round((double)glyph_pos[0].x_advance / (64.0 / scale) + _get_extra_advance(p_font, p_font_size));
			}
		} else {
			gl.advance = -Math::round((double)glyph_pos[0].y_advance / (64.0 / scale));
		}
		gl.count = 1;

		gl.index = glyph_info[0].codepoint;
		if (subpos) {
			gl.x_off = (double)glyph_pos[0].x_offset / (64.0 / scale);
		} else {
			gl.x_off = Math::round((double)glyph_pos[0].x_offset / (64.0 / scale));
		}
		gl.y_off = -Math::round((double)glyph_pos[0].y_offset / (64.0 / scale));
		if (p_sd->orientation == ORIENTATION_HORIZONTAL) {
			gl.y_off += _font_get_baseline_offset(gl.font_rid) * (double)(_font_get_ascent(gl.font_rid, gl.font_size) + _font_get_descent(gl.font_rid, gl.font_size));
		} else {
			gl.x_off += _font_get_baseline_offset(gl.font_rid) * (double)(_font_get_ascent(gl.font_rid, gl.font_size) + _font_get_descent(gl.font_rid, gl.font_size));
		}

		if ((glyph_info[0].codepoint != 0) || !u_isgraph(p_char)) {
			gl.flags |= GRAPHEME_IS_VALID;
		}
	}
	return gl;
}

_FORCE_INLINE_ void TextServerAdvanced::_add_featuers(const Dictionary &p_source, Vector<hb_feature_t> &r_ftrs) {
	Array keys = p_source.keys();
	Array values = p_source.values();
	for (int i = 0; i < keys.size(); i++) {
		int32_t value = values[i];
		if (value >= 0) {
			hb_feature_t feature;
			if (keys[i].is_string()) {
				feature.tag = _name_to_tag(keys[i]);
			} else {
				feature.tag = keys[i];
			}
			feature.value = value;
			feature.start = 0;
			feature.end = -1;
			r_ftrs.push_back(feature);
		}
	}
}

void TextServerAdvanced::_shape_run(ShapedTextDataAdvanced *p_sd, int64_t p_start, int64_t p_end, hb_script_t p_script, hb_direction_t p_direction, TypedArray<RID> p_fonts, int64_t p_span, int64_t p_fb_index, int64_t p_prev_start, int64_t p_prev_end, RID p_prev_font) {
	RID f;
	int fs = p_sd->spans[p_span].font_size;

	if (p_fb_index >= 0 && p_fb_index < p_fonts.size()) {
		// Try font from list.
		f = p_fonts[p_fb_index];
	} else if (OS::get_singleton()->has_feature("system_fonts") && p_fonts.size() > 0 && ((p_fb_index == p_fonts.size()) || (p_fb_index > p_fonts.size() && p_start != p_prev_start))) {
		// Try system fallback.
		RID fdef = p_fonts[0];
		if (_font_is_allow_system_fallback(fdef)) {
			_update_chars(p_sd);

			int64_t next = p_end;
			for (const int32_t &E : p_sd->chars) {
				if (E > p_start) {
					next = E;
					break;
				}
			}
			char scr_buffer[5] = { 0, 0, 0, 0, 0 };
			hb_tag_to_string(hb_script_to_iso15924_tag(p_script), scr_buffer);
			String script_code = String(scr_buffer);

			String text = p_sd->text.substr(p_start, next - p_start);
			f = _find_sys_font_for_text(fdef, script_code, p_sd->spans[p_span].language, text);
		}
	}

	if (!f.is_valid()) {
		// Shaping failed, try looking up raw characters or use fallback hex code boxes.
		int fb_from = (p_direction != HB_DIRECTION_RTL) ? p_start : p_end - 1;
		int fb_to = (p_direction != HB_DIRECTION_RTL) ? p_end : p_start - 1;
		int fb_delta = (p_direction != HB_DIRECTION_RTL) ? +1 : -1;

		for (int i = fb_from; i != fb_to; i += fb_delta) {
			if (p_sd->preserve_invalid || (p_sd->preserve_control && is_control(p_sd->text[i]))) {
				Glyph gl;
				gl.start = i + p_sd->start;
				gl.end = i + 1 + p_sd->start;
				gl.count = 1;
				gl.font_size = fs;
				if (p_direction == HB_DIRECTION_RTL || p_direction == HB_DIRECTION_BTT) {
					gl.flags |= TextServer::GRAPHEME_IS_RTL;
				}

				bool found = false;
				for (int j = 0; j <= p_fonts.size(); j++) {
					RID f_rid;
					if (j == p_fonts.size()) {
						f_rid = p_prev_font;
					} else {
						f_rid = p_fonts[j];
					}
					if (f_rid.is_valid() && _font_has_char(f_rid, p_sd->text[i])) {
						gl.font_rid = f_rid;
						gl.index = _font_get_glyph_index(gl.font_rid, fs, p_sd->text[i], 0);
						if (p_sd->orientation == ORIENTATION_HORIZONTAL) {
							gl.advance = _font_get_glyph_advance(gl.font_rid, fs, gl.index).x;
							gl.x_off = 0;
							gl.y_off = _font_get_baseline_offset(gl.font_rid) * (double)(_font_get_ascent(gl.font_rid, gl.font_size) + _font_get_descent(gl.font_rid, gl.font_size));
							p_sd->ascent = MAX(p_sd->ascent, _font_get_ascent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_TOP));
							p_sd->descent = MAX(p_sd->descent, _font_get_descent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_BOTTOM));
						} else {
							gl.advance = _font_get_glyph_advance(gl.font_rid, fs, gl.index).y;
							gl.x_off = -Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5) + _font_get_baseline_offset(gl.font_rid) * (double)(_font_get_ascent(gl.font_rid, gl.font_size) + _font_get_descent(gl.font_rid, gl.font_size));
							gl.y_off = _font_get_ascent(gl.font_rid, gl.font_size);
							p_sd->ascent = MAX(p_sd->ascent, Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5));
							p_sd->descent = MAX(p_sd->descent, Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5));
						}
						double scale = _font_get_scale(gl.font_rid, fs);
						bool subpos = (scale != 1.0) || (_font_get_subpixel_positioning(gl.font_rid) == SUBPIXEL_POSITIONING_ONE_HALF) || (_font_get_subpixel_positioning(gl.font_rid) == SUBPIXEL_POSITIONING_ONE_QUARTER) || (_font_get_subpixel_positioning(gl.font_rid) == SUBPIXEL_POSITIONING_AUTO && fs <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE);
						if (!subpos) {
							gl.advance = Math::round(gl.advance);
							gl.x_off = Math::round(gl.x_off);
						}
						found = true;
						break;
					}
				}
				if (!found) {
					gl.font_rid = RID();
					gl.index = p_sd->text[i];
					if (p_sd->orientation == ORIENTATION_HORIZONTAL) {
						gl.advance = get_hex_code_box_size(fs, gl.index).x;
						p_sd->ascent = MAX(p_sd->ascent, get_hex_code_box_size(fs, gl.index).y);
					} else {
						gl.advance = get_hex_code_box_size(fs, gl.index).y;
						gl.y_off = get_hex_code_box_size(fs, gl.index).y;
						gl.x_off = -Math::round(get_hex_code_box_size(fs, gl.index).x * 0.5);
						p_sd->ascent = MAX(p_sd->ascent, Math::round(get_hex_code_box_size(fs, gl.index).x * 0.5));
						p_sd->descent = MAX(p_sd->descent, Math::round(get_hex_code_box_size(fs, gl.index).x * 0.5));
					}
				}

				p_sd->width += gl.advance;

				p_sd->glyphs.push_back(gl);
			}
		}
		return;
	}

	FontAdvanced *fd = _get_font_data(f);
	ERR_FAIL_NULL(fd);
	MutexLock lock(fd->mutex);

	Vector2i fss = _get_size(fd, fs);
	hb_font_t *hb_font = _font_get_hb_handle(f, fs);
	double scale = _font_get_scale(f, fs);
	double sp_sp = p_sd->extra_spacing[SPACING_SPACE] + _font_get_spacing(f, SPACING_SPACE);
	double sp_gl = p_sd->extra_spacing[SPACING_GLYPH] + _font_get_spacing(f, SPACING_GLYPH);
	bool last_run = (p_sd->end == p_end);
	double ea = _get_extra_advance(f, fs);
	bool subpos = (scale != 1.0) || (_font_get_subpixel_positioning(f) == SUBPIXEL_POSITIONING_ONE_HALF) || (_font_get_subpixel_positioning(f) == SUBPIXEL_POSITIONING_ONE_QUARTER) || (_font_get_subpixel_positioning(f) == SUBPIXEL_POSITIONING_AUTO && fs <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE);
	ERR_FAIL_NULL(hb_font);

	hb_buffer_clear_contents(p_sd->hb_buffer);
	hb_buffer_set_direction(p_sd->hb_buffer, p_direction);
	int flags = (p_start == 0 ? HB_BUFFER_FLAG_BOT : 0) | (p_end == p_sd->text.length() ? HB_BUFFER_FLAG_EOT : 0);
	if (p_sd->preserve_control) {
		flags |= HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES;
	} else {
		flags |= HB_BUFFER_FLAG_DEFAULT;
	}
#if HB_VERSION_ATLEAST(5, 1, 0)
	flags |= HB_BUFFER_FLAG_PRODUCE_SAFE_TO_INSERT_TATWEEL;
#endif
	hb_buffer_set_flags(p_sd->hb_buffer, (hb_buffer_flags_t)flags);
	hb_buffer_set_script(p_sd->hb_buffer, p_script);

	if (p_sd->spans[p_span].language.is_empty()) {
		hb_language_t lang = hb_language_from_string(TranslationServer::get_singleton()->get_tool_locale().ascii().get_data(), -1);
		hb_buffer_set_language(p_sd->hb_buffer, lang);
	} else {
		hb_language_t lang = hb_language_from_string(p_sd->spans[p_span].language.ascii().get_data(), -1);
		hb_buffer_set_language(p_sd->hb_buffer, lang);
	}

	hb_buffer_add_utf32(p_sd->hb_buffer, (const uint32_t *)p_sd->text.ptr(), p_sd->text.length(), p_start, p_end - p_start);

	Vector<hb_feature_t> ftrs;
	_add_featuers(_font_get_opentype_feature_overrides(f), ftrs);
	_add_featuers(p_sd->spans[p_span].features, ftrs);

	hb_shape(hb_font, p_sd->hb_buffer, ftrs.is_empty() ? nullptr : &ftrs[0], ftrs.size());

	unsigned int glyph_count = 0;
	hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(p_sd->hb_buffer, &glyph_count);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(p_sd->hb_buffer, &glyph_count);

	int mod = 0;
	if (fd->antialiasing == FONT_ANTIALIASING_LCD) {
		TextServer::FontLCDSubpixelLayout layout = lcd_subpixel_layout.get();
		if (layout != FONT_LCD_SUBPIXEL_LAYOUT_NONE) {
			mod = (layout << 24);
		}
	}

	// Process glyphs.
	if (glyph_count > 0) {
		Glyph *w = (Glyph *)memalloc(glyph_count * sizeof(Glyph));

		int end = (p_direction == HB_DIRECTION_RTL || p_direction == HB_DIRECTION_BTT) ? p_end : 0;
		uint32_t last_cluster_id = UINT32_MAX;
		unsigned int last_cluster_index = 0;
		bool last_cluster_valid = true;

		double adv_rem = 0.0;
		for (unsigned int i = 0; i < glyph_count; i++) {
			if ((i > 0) && (last_cluster_id != glyph_info[i].cluster)) {
				if (p_direction == HB_DIRECTION_RTL || p_direction == HB_DIRECTION_BTT) {
					end = w[last_cluster_index].start;
				} else {
					for (unsigned int j = last_cluster_index; j < i; j++) {
						w[j].end = glyph_info[i].cluster;
					}
				}
				if (p_direction == HB_DIRECTION_RTL || p_direction == HB_DIRECTION_BTT) {
					w[last_cluster_index].flags |= GRAPHEME_IS_RTL;
				}
				if (last_cluster_valid) {
					w[last_cluster_index].flags |= GRAPHEME_IS_VALID;
				}
				w[last_cluster_index].count = i - last_cluster_index;
				last_cluster_index = i;
				last_cluster_valid = true;
			}

			last_cluster_id = glyph_info[i].cluster;

			Glyph &gl = w[i];
			gl = Glyph();

			gl.start = glyph_info[i].cluster;
			gl.end = end;
			gl.count = 0;

			gl.font_rid = f;
			gl.font_size = fs;

			if (glyph_info[i].mask & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) {
				gl.flags |= GRAPHEME_IS_CONNECTED;
			}

#if HB_VERSION_ATLEAST(5, 1, 0)
			if (glyph_info[i].mask & HB_GLYPH_FLAG_SAFE_TO_INSERT_TATWEEL) {
				gl.flags |= GRAPHEME_IS_SAFE_TO_INSERT_TATWEEL;
			}
#endif

			gl.index = glyph_info[i].codepoint;
			if ((p_sd->text[glyph_info[i].cluster] == 0x0009) || u_isblank(p_sd->text[glyph_info[i].cluster]) || is_linebreak(p_sd->text[glyph_info[i].cluster])) {
				adv_rem = 0.0; // Reset on blank.
			}
			if (gl.index != 0) {
				FontGlyph fgl;
				_ensure_glyph(fd, fss, gl.index | mod, fgl);
				if (subpos) {
					gl.x_off = (double)glyph_pos[i].x_offset / (64.0 / scale);
				} else if (p_sd->orientation == ORIENTATION_HORIZONTAL) {
					gl.x_off = Math::round(adv_rem + ((double)glyph_pos[i].x_offset / (64.0 / scale)));
				} else {
					gl.x_off = Math::round((double)glyph_pos[i].x_offset / (64.0 / scale));
				}
				if (p_sd->orientation == ORIENTATION_HORIZONTAL) {
					gl.y_off = -Math::round((double)glyph_pos[i].y_offset / (64.0 / scale));
				} else {
					gl.y_off = -Math::round(adv_rem + ((double)glyph_pos[i].y_offset / (64.0 / scale)));
				}
				if (p_sd->orientation == ORIENTATION_HORIZONTAL) {
					if (subpos) {
						gl.advance = (double)glyph_pos[i].x_advance / (64.0 / scale) + ea;
					} else {
						double full_adv = adv_rem + ((double)glyph_pos[i].x_advance / (64.0 / scale) + ea);
						gl.advance = Math::round(full_adv);
						if (fd->keep_rounding_remainders) {
							adv_rem = full_adv - gl.advance;
						}
					}
				} else {
					double full_adv = adv_rem + ((double)glyph_pos[i].y_advance / (64.0 / scale));
					gl.advance = -Math::round(full_adv);
					if (fd->keep_rounding_remainders) {
						adv_rem = full_adv + gl.advance;
					}
				}
				if (p_sd->orientation == ORIENTATION_HORIZONTAL) {
					gl.y_off += _font_get_baseline_offset(gl.font_rid) * (double)(_font_get_ascent(gl.font_rid, gl.font_size) + _font_get_descent(gl.font_rid, gl.font_size));
				} else {
					gl.x_off += _font_get_baseline_offset(gl.font_rid) * (double)(_font_get_ascent(gl.font_rid, gl.font_size) + _font_get_descent(gl.font_rid, gl.font_size));
				}
			}
			if (!last_run || i < glyph_count - 1) {
				// Do not add extra spacing to the last glyph of the string.
				if (sp_sp && is_whitespace(p_sd->text[glyph_info[i].cluster])) {
					gl.advance += sp_sp;
				} else {
					gl.advance += sp_gl;
				}
			}

			if (p_sd->preserve_control) {
				last_cluster_valid = last_cluster_valid && ((glyph_info[i].codepoint != 0) || (p_sd->text[glyph_info[i].cluster] == 0x0009) || (u_isblank(p_sd->text[glyph_info[i].cluster]) && (gl.advance != 0)) || (!u_isblank(p_sd->text[glyph_info[i].cluster]) && is_linebreak(p_sd->text[glyph_info[i].cluster])));
			} else {
				last_cluster_valid = last_cluster_valid && ((glyph_info[i].codepoint != 0) || (p_sd->text[glyph_info[i].cluster] == 0x0009) || (u_isblank(p_sd->text[glyph_info[i].cluster]) && (gl.advance != 0)) || (!u_isblank(p_sd->text[glyph_info[i].cluster]) && !u_isgraph(p_sd->text[glyph_info[i].cluster])));
			}
		}
		if (p_direction == HB_DIRECTION_LTR || p_direction == HB_DIRECTION_TTB) {
			for (unsigned int j = last_cluster_index; j < glyph_count; j++) {
				w[j].end = p_end;
			}
		}
		w[last_cluster_index].count = glyph_count - last_cluster_index;
		if (p_direction == HB_DIRECTION_RTL || p_direction == HB_DIRECTION_BTT) {
			w[last_cluster_index].flags |= GRAPHEME_IS_RTL;
		}
		if (last_cluster_valid) {
			w[last_cluster_index].flags |= GRAPHEME_IS_VALID;
		}

		// Fallback.
		int failed_subrun_start = p_end + 1;
		int failed_subrun_end = p_start;

		for (unsigned int i = 0; i < glyph_count; i++) {
			if ((w[i].flags & GRAPHEME_IS_VALID) == GRAPHEME_IS_VALID) {
				if (failed_subrun_start != p_end + 1) {
					_shape_run(p_sd, failed_subrun_start, failed_subrun_end, p_script, p_direction, p_fonts, p_span, p_fb_index + 1, p_start, p_end, (p_fb_index >= p_fonts.size()) ? f : RID());
					failed_subrun_start = p_end + 1;
					failed_subrun_end = p_start;
				}
				for (int j = 0; j < w[i].count; j++) {
					if (p_sd->orientation == ORIENTATION_HORIZONTAL) {
						p_sd->ascent = MAX(p_sd->ascent, -w[i + j].y_off);
						p_sd->descent = MAX(p_sd->descent, w[i + j].y_off);
					} else {
						double gla = Math::round(_font_get_glyph_advance(f, fs, w[i + j].index).x * 0.5);
						p_sd->ascent = MAX(p_sd->ascent, gla);
						p_sd->descent = MAX(p_sd->descent, gla);
					}
					p_sd->width += w[i + j].advance;
					w[i + j].start += p_sd->start;
					w[i + j].end += p_sd->start;
					p_sd->glyphs.push_back(w[i + j]);
				}
			} else {
				if (failed_subrun_start >= w[i].start) {
					failed_subrun_start = w[i].start;
				}
				if (failed_subrun_end <= w[i].end) {
					failed_subrun_end = w[i].end;
				}
			}
			i += w[i].count - 1;
		}
		memfree(w);
		if (failed_subrun_start != p_end + 1) {
			_shape_run(p_sd, failed_subrun_start, failed_subrun_end, p_script, p_direction, p_fonts, p_span, p_fb_index + 1, p_start, p_end, (p_fb_index >= p_fonts.size()) ? f : RID());
		}
		p_sd->ascent = MAX(p_sd->ascent, _font_get_ascent(f, fs) + _font_get_spacing(f, SPACING_TOP));
		p_sd->descent = MAX(p_sd->descent, _font_get_descent(f, fs) + _font_get_spacing(f, SPACING_BOTTOM));
		p_sd->upos = MAX(p_sd->upos, _font_get_underline_position(f, fs));
		p_sd->uthk = MAX(p_sd->uthk, _font_get_underline_thickness(f, fs));
	}
}

bool TextServerAdvanced::_shaped_text_shape(const RID &p_shaped) {
	_THREAD_SAFE_METHOD_
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	if (sd->valid.is_set()) {
		return true;
	}

	invalidate(sd, false);
	if (sd->parent != RID()) {
		_shaped_text_shape(sd->parent);
		ShapedTextDataAdvanced *parent_sd = shaped_owner.get_or_null(sd->parent);
		ERR_FAIL_COND_V(!parent_sd->valid.is_set(), false);
		ERR_FAIL_COND_V(!_shape_substr(sd, parent_sd, sd->start, sd->end - sd->start), false);
		return true;
	}

	if (sd->text.length() == 0) {
		sd->valid.set();
		return true;
	}

	sd->utf16 = sd->text.utf16();
	const UChar *data = sd->utf16.get_data();

	// Create script iterator.
	if (sd->script_iter == nullptr) {
		sd->script_iter = memnew(ScriptIterator(sd->text, 0, sd->text.length()));
	}

	sd->base_para_direction = UBIDI_DEFAULT_LTR;
	switch (sd->direction) {
		case DIRECTION_LTR: {
			sd->para_direction = DIRECTION_LTR;
			sd->base_para_direction = UBIDI_LTR;
		} break;
		case DIRECTION_RTL: {
			sd->para_direction = DIRECTION_RTL;
			sd->base_para_direction = UBIDI_RTL;
		} break;
		case DIRECTION_INHERITED:
		case DIRECTION_AUTO: {
			UBiDiDirection direction = ubidi_getBaseDirection(data, sd->utf16.length());
			if (direction != UBIDI_NEUTRAL) {
				sd->para_direction = (direction == UBIDI_RTL) ? DIRECTION_RTL : DIRECTION_LTR;
				sd->base_para_direction = direction;
			} else {
				const String &lang = (sd->spans.is_empty() || sd->spans[0].language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale() : sd->spans[0].language;
				bool lang_rtl = _is_locale_right_to_left(lang);

				sd->para_direction = lang_rtl ? DIRECTION_RTL : DIRECTION_LTR;
				sd->base_para_direction = lang_rtl ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR;
			}
		} break;
	}

	if (sd->bidi_override.is_empty()) {
		sd->bidi_override.push_back(Vector3i(sd->start, sd->end, DIRECTION_INHERITED));
	}

	for (int ov = 0; ov < sd->bidi_override.size(); ov++) {
		// Create BiDi iterator.
		int start = _convert_pos_inv(sd, sd->bidi_override[ov].x - sd->start);
		int end = _convert_pos_inv(sd, sd->bidi_override[ov].y - sd->start);

		if (start < 0 || end - start > sd->utf16.length()) {
			continue;
		}

		UErrorCode err = U_ZERO_ERROR;
		UBiDi *bidi_iter = ubidi_openSized(end - start, 0, &err);
		if (U_SUCCESS(err)) {
			switch (static_cast<TextServer::Direction>(sd->bidi_override[ov].z)) {
				case DIRECTION_LTR: {
					ubidi_setPara(bidi_iter, data + start, end - start, UBIDI_LTR, nullptr, &err);
				} break;
				case DIRECTION_RTL: {
					ubidi_setPara(bidi_iter, data + start, end - start, UBIDI_RTL, nullptr, &err);
				} break;
				case DIRECTION_INHERITED: {
					ubidi_setPara(bidi_iter, data + start, end - start, sd->base_para_direction, nullptr, &err);
				} break;
				case DIRECTION_AUTO: {
					UBiDiDirection direction = ubidi_getBaseDirection(data + start, end - start);
					if (direction != UBIDI_NEUTRAL) {
						ubidi_setPara(bidi_iter, data + start, end - start, direction, nullptr, &err);
					} else {
						ubidi_setPara(bidi_iter, data + start, end - start, sd->base_para_direction, nullptr, &err);
					}
				} break;
			}
			if (U_FAILURE(err)) {
				ubidi_close(bidi_iter);
				bidi_iter = nullptr;
				ERR_PRINT(vformat("BiDi reordering for the paragraph failed: %s", u_errorName(err)));
			}
		} else {
			bidi_iter = nullptr;
			ERR_PRINT(vformat("BiDi iterator allocation for the paragraph failed: %s", u_errorName(err)));
		}
		sd->bidi_iter.push_back(bidi_iter);

		err = U_ZERO_ERROR;
		int bidi_run_count = 1;
		if (bidi_iter) {
			bidi_run_count = ubidi_countRuns(bidi_iter, &err);
			if (U_FAILURE(err)) {
				ERR_PRINT(u_errorName(err));
			}
		}
		for (int i = 0; i < bidi_run_count; i++) {
			int32_t _bidi_run_start = 0;
			int32_t _bidi_run_length = end - start;
			bool is_ltr = false;
			hb_direction_t bidi_run_direction = HB_DIRECTION_INVALID;
			if (bidi_iter) {
				is_ltr = (ubidi_getVisualRun(bidi_iter, i, &_bidi_run_start, &_bidi_run_length) == UBIDI_LTR);
			}
			switch (sd->orientation) {
				case ORIENTATION_HORIZONTAL: {
					if (is_ltr) {
						bidi_run_direction = HB_DIRECTION_LTR;
					} else {
						bidi_run_direction = HB_DIRECTION_RTL;
					}
				} break;
				case ORIENTATION_VERTICAL: {
					if (is_ltr) {
						bidi_run_direction = HB_DIRECTION_TTB;
					} else {
						bidi_run_direction = HB_DIRECTION_BTT;
					}
				}
			}

			int32_t bidi_run_start = _convert_pos(sd, start + _bidi_run_start);
			int32_t bidi_run_end = _convert_pos(sd, start + _bidi_run_start + _bidi_run_length);

			// Shape runs.

			int scr_from = (is_ltr) ? 0 : sd->script_iter->script_ranges.size() - 1;
			int scr_to = (is_ltr) ? sd->script_iter->script_ranges.size() : -1;
			int scr_delta = (is_ltr) ? +1 : -1;

			for (int j = scr_from; j != scr_to; j += scr_delta) {
				if ((sd->script_iter->script_ranges[j].start < bidi_run_end) && (sd->script_iter->script_ranges[j].end > bidi_run_start)) {
					int32_t script_run_start = MAX(sd->script_iter->script_ranges[j].start, bidi_run_start);
					int32_t script_run_end = MIN(sd->script_iter->script_ranges[j].end, bidi_run_end);
					char scr_buffer[5] = { 0, 0, 0, 0, 0 };
					hb_tag_to_string(hb_script_to_iso15924_tag(sd->script_iter->script_ranges[j].script), scr_buffer);
					String script_code = String(scr_buffer);

					int spn_from = (is_ltr) ? 0 : sd->spans.size() - 1;
					int spn_to = (is_ltr) ? sd->spans.size() : -1;
					int spn_delta = (is_ltr) ? +1 : -1;

					for (int k = spn_from; k != spn_to; k += spn_delta) {
						const ShapedTextDataAdvanced::Span &span = sd->spans[k];
						if (span.start - sd->start >= script_run_end || span.end - sd->start <= script_run_start) {
							continue;
						}
						if (span.embedded_key != Variant()) {
							// Embedded object.
							if (sd->orientation == ORIENTATION_HORIZONTAL) {
								sd->objects[span.embedded_key].rect.position.x = sd->width;
								sd->width += sd->objects[span.embedded_key].rect.size.x;
							} else {
								sd->objects[span.embedded_key].rect.position.y = sd->width;
								sd->width += sd->objects[span.embedded_key].rect.size.y;
							}
							Glyph gl;
							gl.start = span.start;
							gl.end = span.end;
							gl.count = 1;
							gl.flags = GRAPHEME_IS_VALID | GRAPHEME_IS_EMBEDDED_OBJECT;
							if (sd->orientation == ORIENTATION_HORIZONTAL) {
								gl.advance = sd->objects[span.embedded_key].rect.size.x;
							} else {
								gl.advance = sd->objects[span.embedded_key].rect.size.y;
							}
							sd->glyphs.push_back(gl);
						} else {
							Array fonts;
							Array fonts_scr_only;
							Array fonts_no_match;
							int font_count = span.fonts.size();
							if (font_count > 0) {
								fonts.push_back(sd->spans[k].fonts[0]);
							}
							for (int l = 1; l < font_count; l++) {
								if (_font_is_script_supported(span.fonts[l], script_code)) {
									if (_font_is_language_supported(span.fonts[l], span.language)) {
										fonts.push_back(sd->spans[k].fonts[l]);
									} else {
										fonts_scr_only.push_back(sd->spans[k].fonts[l]);
									}
								} else {
									fonts_no_match.push_back(sd->spans[k].fonts[l]);
								}
							}
							fonts.append_array(fonts_scr_only);
							fonts.append_array(fonts_no_match);
							_shape_run(sd, MAX(sd->spans[k].start - sd->start, script_run_start), MIN(sd->spans[k].end - sd->start, script_run_end), sd->script_iter->script_ranges[j].script, bidi_run_direction, fonts, k, 0, 0, 0, RID());
						}
					}
				}
			}
		}
	}

	_realign(sd);
	sd->valid.set();
	return sd->valid.is_set();
}

bool TextServerAdvanced::_shaped_text_is_ready(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	// Atomic read is safe and faster.
	return sd->valid.is_set();
}

const Glyph *TextServerAdvanced::_shaped_text_get_glyphs(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, nullptr);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}
	return sd->glyphs.ptr();
}

int64_t TextServerAdvanced::_shaped_text_get_glyph_count(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}
	return sd->glyphs.size();
}

const Glyph *TextServerAdvanced::_shaped_text_sort_logical(const RID &p_shaped) {
	ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, nullptr);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		_shaped_text_shape(p_shaped);
	}

	if (!sd->sort_valid) {
		sd->glyphs_logical = sd->glyphs;
		sd->glyphs_logical.sort_custom<GlyphCompare>();
		sd->sort_valid = true;
	}

	return sd->glyphs_logical.ptr();
}

Vector2i TextServerAdvanced::_shaped_text_get_range(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Vector2i());

	MutexLock lock(sd->mutex);
	return Vector2(sd->start, sd->end);
}

Array TextServerAdvanced::_shaped_text_get_objects(const RID &p_shaped) const {
	Array ret;
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, ret);

	MutexLock lock(sd->mutex);
	for (const KeyValue<Variant, ShapedTextDataAdvanced::EmbeddedObject> &E : sd->objects) {
		ret.push_back(E.key);
	}

	return ret;
}

Rect2 TextServerAdvanced::_shaped_text_get_object_rect(const RID &p_shaped, const Variant &p_key) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Rect2());

	MutexLock lock(sd->mutex);
	ERR_FAIL_COND_V(!sd->objects.has(p_key), Rect2());
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}
	return sd->objects[p_key].rect;
}

Vector2i TextServerAdvanced::_shaped_text_get_object_range(const RID &p_shaped, const Variant &p_key) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Vector2i());

	MutexLock lock(sd->mutex);
	ERR_FAIL_COND_V(!sd->objects.has(p_key), Vector2i());
	return Vector2i(sd->objects[p_key].start, sd->objects[p_key].end);
}

int64_t TextServerAdvanced::_shaped_text_get_object_glyph(const RID &p_shaped, const Variant &p_key) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, -1);

	MutexLock lock(sd->mutex);
	ERR_FAIL_COND_V(!sd->objects.has(p_key), -1);
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}
	const ShapedTextDataAdvanced::EmbeddedObject &obj = sd->objects[p_key];
	int sd_size = sd->glyphs.size();
	const Glyph *sd_glyphs = sd->glyphs.ptr();
	for (int i = 0; i < sd_size; i++) {
		if (obj.start == sd_glyphs[i].start) {
			return i;
		}
	}
	return -1;
}

Size2 TextServerAdvanced::_shaped_text_get_size(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Size2());

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}
	if (sd->orientation == TextServer::ORIENTATION_HORIZONTAL) {
		return Size2((sd->text_trimmed ? sd->width_trimmed : sd->width), sd->ascent + sd->descent + sd->extra_spacing[SPACING_TOP] + sd->extra_spacing[SPACING_BOTTOM]).ceil();
	} else {
		return Size2(sd->ascent + sd->descent + sd->extra_spacing[SPACING_TOP] + sd->extra_spacing[SPACING_BOTTOM], (sd->text_trimmed ? sd->width_trimmed : sd->width)).ceil();
	}
}

double TextServerAdvanced::_shaped_text_get_ascent(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}
	return sd->ascent + sd->extra_spacing[SPACING_TOP];
}

double TextServerAdvanced::_shaped_text_get_descent(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}
	return sd->descent + sd->extra_spacing[SPACING_BOTTOM];
}

double TextServerAdvanced::_shaped_text_get_width(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}
	return Math::ceil(sd->text_trimmed ? sd->width_trimmed : sd->width);
}

double TextServerAdvanced::_shaped_text_get_underline_position(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}

	return sd->upos;
}

double TextServerAdvanced::_shaped_text_get_underline_thickness(const RID &p_shaped) const {
	const ShapedTextDataAdvanced *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerAdvanced *>(this)->_shaped_text_shape(p_shaped);
	}

	return sd->uthk;
}

void TextServerAdvanced::_insert_num_systems_lang() {
	// Eastern Arabic numerals.
	{
		NumSystemData ar;
		ar.lang.insert(StringName("ar")); // Arabic
		ar.lang.insert(StringName("ar_AE"));
		ar.lang.insert(StringName("ar_BH"));
		ar.lang.insert(StringName("ar_DJ"));
		ar.lang.insert(StringName("ar_EG"));
		ar.lang.insert(StringName("ar_ER"));
		ar.lang.insert(StringName("ar_IL"));
		ar.lang.insert(StringName("ar_IQ"));
		ar.lang.insert(StringName("ar_JO"));
		ar.lang.insert(StringName("ar_KM"));
		ar.lang.insert(StringName("ar_KW"));
		ar.lang.insert(StringName("ar_LB"));
		ar.lang.insert(StringName("ar_MR"));
		ar.lang.insert(StringName("ar_OM"));
		ar.lang.insert(StringName("ar_PS"));
		ar.lang.insert(StringName("ar_QA"));
		ar.lang.insert(StringName("ar_SA"));
		ar.lang.insert(StringName("ar_SD"));
		ar.lang.insert(StringName("ar_SO"));
		ar.lang.insert(StringName("ar_SS"));
		ar.lang.insert(StringName("ar_SY"));
		ar.lang.insert(StringName("ar_TD"));
		ar.lang.insert(StringName("ar_YE"));
		ar.lang.insert(StringName("ckb")); // Central Kurdish
		ar.lang.insert(StringName("ckb_IQ"));
		ar.lang.insert(StringName("ckb_IR"));
		ar.lang.insert(StringName("sd")); // Sindhi
		ar.lang.insert(StringName("sd_PK"));
		ar.lang.insert(StringName("sd_Arab"));
		ar.lang.insert(StringName("sd_Arab_PK"));
		ar.digits = U"٠١٢٣٤٥٦٧٨٩٫";
		ar.percent_sign = U"٪";
		ar.exp = U"اس";
		num_systems.push_back(ar);
	}

	// Persian and Urdu numerals.
	{
		NumSystemData pr;
		pr.lang.insert(StringName("fa")); // Persian
		pr.lang.insert(StringName("fa_AF"));
		pr.lang.insert(StringName("fa_IR"));
		pr.lang.insert(StringName("ks")); // Kashmiri
		pr.lang.insert(StringName("ks_IN"));
		pr.lang.insert(StringName("ks_Arab"));
		pr.lang.insert(StringName("ks_Arab_IN"));
		pr.lang.insert(StringName("lrc")); // Northern Luri
		pr.lang.insert(StringName("lrc_IQ"));
		pr.lang.insert(StringName("lrc_IR"));
		pr.lang.insert(StringName("mzn")); // Mazanderani
		pr.lang.insert(StringName("mzn_IR"));
		pr.lang.insert(StringName("pa_PK")); // Panjabi
		pr.lang.insert(StringName("pa_Arab"));
		pr.lang.insert(StringName("pa_Arab_PK"));
		pr.lang.insert(StringName("ps")); // Pushto
		pr.lang.insert(StringName("ps_AF"));
		pr.lang.insert(StringName("ps_PK"));
		pr.lang.insert(StringName("ur_IN")); // Urdu
		pr.lang.insert(StringName("uz_AF")); // Uzbek
		pr.lang.insert(StringName("uz_Arab"));
		pr.lang.insert(StringName("uz_Arab_AF"));
		pr.digits = U"۰۱۲۳۴۵۶۷۸۹٫";
		pr.percent_sign = U"٪";
		pr.exp = U"اس";
		num_systems.push_back(pr);
	}

	// Bengali numerals.
	{
		NumSystemData bn;
		bn.lang.insert(StringName("as")); // Assamese
		bn.lang.insert(StringName("as_IN"));
		bn.lang.insert(StringName("bn")); // Bengali
		bn.lang.insert(StringName("bn_BD"));
		bn.lang.insert(StringName("bn_IN"));
		bn.lang.insert(StringName("mni")); // Manipuri
		bn.lang.insert(StringName("mni_IN"));
		bn.lang.insert(StringName("mni_Beng"));
		bn.lang.insert(StringName("mni_Beng_IN"));
		bn.digits = U"০১২৩৪৫৬৭৮৯.";
		bn.percent_sign = U"%";
		bn.exp = U"e";
		num_systems.push_back(bn);
	}

	// Devanagari numerals.
	{
		NumSystemData mr;
		mr.lang.insert(StringName("mr")); // Marathi
		mr.lang.insert(StringName("mr_IN"));
		mr.lang.insert(StringName("ne")); // Nepali
		mr.lang.insert(StringName("ne_IN"));
		mr.lang.insert(StringName("ne_NP"));
		mr.lang.insert(StringName("sa")); // Sanskrit
		mr.lang.insert(StringName("sa_IN"));
		mr.digits = U"०१२३४५६७८९.";
		mr.percent_sign = U"%";
		mr.exp = U"e";
		num_systems.push_back(mr);
	}

	// Dzongkha numerals.
	{
		NumSystemData dz;
		dz.lang.insert(StringName("dz")); // Dzongkha
		dz.lang.insert(StringName("dz_BT"));
		dz.digits = U"༠༡༢༣༤༥༦༧༨༩.";
		dz.percent_sign = U"%";
		dz.exp = U"e";
		num_systems.push_back(dz);
	}

	// Santali numerals.
	{
		NumSystemData sat;
		sat.lang.insert(StringName("sat")); // Santali
		sat.lang.insert(StringName("sat_IN"));
		sat.lang.insert(StringName("sat_Olck"));
		sat.lang.insert(StringName("sat_Olck_IN"));
		sat.digits = U"᱐᱑᱒᱓᱔᱕᱖᱗᱘᱙.";
		sat.percent_sign = U"%";
		sat.exp = U"e";
		num_systems.push_back(sat);
	}

	// Burmese numerals.
	{
		NumSystemData my;
		my.lang.insert(StringName("my")); // Burmese
		my.lang.insert(StringName("my_MM"));
		my.digits = U"၀၁၂၃၄၅၆၇၈၉.";
		my.percent_sign = U"%";
		my.exp = U"e";
		num_systems.push_back(my);
	}

	// Chakma numerals.
	{
		NumSystemData ccp;
		ccp.lang.insert(StringName("ccp")); // Chakma
		ccp.lang.insert(StringName("ccp_BD"));
		ccp.lang.insert(StringName("ccp_IN"));
		ccp.digits = U"𑄶𑄷𑄸𑄹𑄺𑄻𑄼𑄽𑄾𑄿.";
		ccp.percent_sign = U"%";
		ccp.exp = U"e";
		num_systems.push_back(ccp);
	}

	// Adlam numerals.
	{
		NumSystemData ff;
		ff.lang.insert(StringName("ff")); // Fulah
		ff.lang.insert(StringName("ff_Adlm_BF"));
		ff.lang.insert(StringName("ff_Adlm_CM"));
		ff.lang.insert(StringName("ff_Adlm_GH"));
		ff.lang.insert(StringName("ff_Adlm_GM"));
		ff.lang.insert(StringName("ff_Adlm_GN"));
		ff.lang.insert(StringName("ff_Adlm_GW"));
		ff.lang.insert(StringName("ff_Adlm_LR"));
		ff.lang.insert(StringName("ff_Adlm_MR"));
		ff.lang.insert(StringName("ff_Adlm_NE"));
		ff.lang.insert(StringName("ff_Adlm_NG"));
		ff.lang.insert(StringName("ff_Adlm_SL"));
		ff.lang.insert(StringName("ff_Adlm_SN"));
		ff.digits = U"𞥐𞥑𞥒𞥓𞥔𞥕𞥖𞥗𞥘𞥙.";
		ff.percent_sign = U"%";
		ff.exp = U"e";
		num_systems.push_back(ff);
	}
}

String TextServerAdvanced::_format_number(const String &p_string, const String &p_language) const {
	const StringName lang = (p_language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale() : p_language;

	String res = p_string;
	for (int i = 0; i < num_systems.size(); i++) {
		if (num_systems[i].lang.has(lang)) {
			if (num_systems[i].digits.is_empty()) {
				return p_string;
			}
			res.replace("e", num_systems[i].exp);
			res.replace("E", num_systems[i].exp);
			char32_t *data = res.ptrw();
			for (int j = 0; j < res.length(); j++) {
				if (data[j] >= 0x30 && data[j] <= 0x39) {
					data[j] = num_systems[i].digits[data[j] - 0x30];
				} else if (data[j] == '.' || data[j] == ',') {
					data[j] = num_systems[i].digits[10];
				}
			}
			break;
		}
	}
	return res;
}

String TextServerAdvanced::_parse_number(const String &p_string, const String &p_language) const {
	const StringName lang = (p_language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale() : p_language;

	String res = p_string;
	for (int i = 0; i < num_systems.size(); i++) {
		if (num_systems[i].lang.has(lang)) {
			if (num_systems[i].digits.is_empty()) {
				return p_string;
			}
			res.replace(num_systems[i].exp, "e");
			char32_t *data = res.ptrw();
			for (int j = 0; j < res.length(); j++) {
				if (data[j] == num_systems[i].digits[10]) {
					data[j] = '.';
				} else {
					for (int k = 0; k < 10; k++) {
						if (data[j] == num_systems[i].digits[k]) {
							data[j] = 0x30 + k;
						}
					}
				}
			}
			break;
		}
	}
	return res;
}

String TextServerAdvanced::_percent_sign(const String &p_language) const {
	const StringName lang = (p_language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale() : p_language;

	for (int i = 0; i < num_systems.size(); i++) {
		if (num_systems[i].lang.has(lang)) {
			if (num_systems[i].percent_sign.is_empty()) {
				return "%";
			}
			return num_systems[i].percent_sign;
		}
	}
	return "%";
}

int64_t TextServerAdvanced::_is_confusable(const String &p_string, const PackedStringArray &p_dict) const {
#ifndef ICU_STATIC_DATA
	if (!icu_data_loaded) {
		return -1;
	}
#endif
	UErrorCode status = U_ZERO_ERROR;
	int64_t match_index = -1;

	Char16String utf16 = p_string.utf16();
	Vector<UChar *> skeletons;
	skeletons.resize(p_dict.size());

	if (sc_conf == nullptr) {
		sc_conf = uspoof_open(&status);
		uspoof_setChecks(sc_conf, USPOOF_CONFUSABLE, &status);
	}
	for (int i = 0; i < p_dict.size(); i++) {
		Char16String word = p_dict[i].utf16();
		int32_t len = uspoof_getSkeleton(sc_conf, 0, word.get_data(), -1, nullptr, 0, &status);
		skeletons.write[i] = (UChar *)memalloc(++len * sizeof(UChar));
		status = U_ZERO_ERROR;
		uspoof_getSkeleton(sc_conf, 0, word.get_data(), -1, skeletons.write[i], len, &status);
	}

	int32_t len = uspoof_getSkeleton(sc_conf, 0, utf16.get_data(), -1, nullptr, 0, &status);
	UChar *skel = (UChar *)memalloc(++len * sizeof(UChar));
	status = U_ZERO_ERROR;
	uspoof_getSkeleton(sc_conf, 0, utf16.get_data(), -1, skel, len, &status);
	for (int i = 0; i < skeletons.size(); i++) {
		if (u_strcmp(skel, skeletons[i]) == 0) {
			match_index = i;
			break;
		}
	}
	memfree(skel);

	for (int i = 0; i < skeletons.size(); i++) {
		memfree(skeletons.write[i]);
	}

	ERR_FAIL_COND_V_MSG(U_FAILURE(status), -1, u_errorName(status));

	return match_index;
}

bool TextServerAdvanced::_spoof_check(const String &p_string) const {
#ifndef ICU_STATIC_DATA
	if (!icu_data_loaded) {
		return false;
	}
#endif
	UErrorCode status = U_ZERO_ERROR;
	Char16String utf16 = p_string.utf16();

	if (allowed == nullptr) {
		allowed = uset_openEmpty();
		uset_addAll(allowed, uspoof_getRecommendedSet(&status));
		uset_addAll(allowed, uspoof_getInclusionSet(&status));
	}
	if (sc_spoof == nullptr) {
		sc_spoof = uspoof_open(&status);
		uspoof_setAllowedChars(sc_spoof, allowed, &status);
		uspoof_setRestrictionLevel(sc_spoof, USPOOF_MODERATELY_RESTRICTIVE);
	}

	int32_t bitmask = uspoof_check(sc_spoof, utf16.get_data(), -1, nullptr, &status);
	ERR_FAIL_COND_V_MSG(U_FAILURE(status), false, u_errorName(status));

	return (bitmask != 0);
}

String TextServerAdvanced::_strip_diacritics(const String &p_string) const {
#ifndef ICU_STATIC_DATA
	if (!icu_data_loaded) {
		return TextServer::strip_diacritics(p_string);
	}
#endif
	UErrorCode err = U_ZERO_ERROR;

	// Get NFKD normalizer singleton.
	const UNormalizer2 *unorm = unorm2_getNFKDInstance(&err);
	ERR_FAIL_COND_V_MSG(U_FAILURE(err), TextServer::strip_diacritics(p_string), u_errorName(err));

	// Convert to UTF-16.
	Char16String utf16 = p_string.utf16();

	// Normalize.
	Vector<char16_t> normalized;
	err = U_ZERO_ERROR;
	int32_t len = unorm2_normalize(unorm, utf16.get_data(), -1, nullptr, 0, &err);
	ERR_FAIL_COND_V_MSG(err != U_BUFFER_OVERFLOW_ERROR, TextServer::strip_diacritics(p_string), u_errorName(err));
	normalized.resize(len);
	err = U_ZERO_ERROR;
	unorm2_normalize(unorm, utf16.get_data(), -1, normalized.ptrw(), len, &err);
	ERR_FAIL_COND_V_MSG(U_FAILURE(err), TextServer::strip_diacritics(p_string), u_errorName(err));

	// Convert back to UTF-32.
	String normalized_string = String::utf16(normalized.ptr(), len);

	// Strip combining characters.
	String result;
	for (int i = 0; i < normalized_string.length(); i++) {
		if (u_getCombiningClass(normalized_string[i]) == 0) {
#ifdef GDEXTENSION
			result = result + String::chr(normalized_string[i]);
#elif defined(GODOT_MODULE)
			result = result + normalized_string[i];
#endif
		}
	}
	return result;
}

String TextServerAdvanced::_string_to_upper(const String &p_string, const String &p_language) const {
#ifndef ICU_STATIC_DATA
	if (!icu_data_loaded) {
		return p_string.to_upper();
	}
#endif

	if (p_string.is_empty()) {
		return p_string;
	}
	const String lang = (p_language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale() : p_language;

	// Convert to UTF-16.
	Char16String utf16 = p_string.utf16();

	Vector<char16_t> upper;
	UErrorCode err = U_ZERO_ERROR;
	int32_t len = u_strToUpper(nullptr, 0, utf16.get_data(), -1, lang.ascii().get_data(), &err);
	ERR_FAIL_COND_V_MSG(err != U_BUFFER_OVERFLOW_ERROR, p_string, u_errorName(err));
	upper.resize(len);
	err = U_ZERO_ERROR;
	u_strToUpper(upper.ptrw(), len, utf16.get_data(), -1, lang.ascii().get_data(), &err);
	ERR_FAIL_COND_V_MSG(U_FAILURE(err), p_string, u_errorName(err));

	// Convert back to UTF-32.
	return String::utf16(upper.ptr(), len);
}

String TextServerAdvanced::_string_to_lower(const String &p_string, const String &p_language) const {
#ifndef ICU_STATIC_DATA
	if (!icu_data_loaded) {
		return p_string.to_lower();
	}
#endif

	if (p_string.is_empty()) {
		return p_string;
	}
	const String lang = (p_language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale() : p_language;
	// Convert to UTF-16.
	Char16String utf16 = p_string.utf16();

	Vector<char16_t> lower;
	UErrorCode err = U_ZERO_ERROR;
	int32_t len = u_strToLower(nullptr, 0, utf16.get_data(), -1, lang.ascii().get_data(), &err);
	ERR_FAIL_COND_V_MSG(err != U_BUFFER_OVERFLOW_ERROR, p_string, u_errorName(err));
	lower.resize(len);
	err = U_ZERO_ERROR;
	u_strToLower(lower.ptrw(), len, utf16.get_data(), -1, lang.ascii().get_data(), &err);
	ERR_FAIL_COND_V_MSG(U_FAILURE(err), p_string, u_errorName(err));

	// Convert back to UTF-32.
	return String::utf16(lower.ptr(), len);
}

String TextServerAdvanced::_string_to_title(const String &p_string, const String &p_language) const {
#ifndef ICU_STATIC_DATA
	if (!icu_data_loaded) {
		return p_string.capitalize();
	}
#endif

	if (p_string.is_empty()) {
		return p_string;
	}
	const String lang = (p_language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale() : p_language;

	// Convert to UTF-16.
	Char16String utf16 = p_string.utf16();

	Vector<char16_t> upper;
	UErrorCode err = U_ZERO_ERROR;
	int32_t len = u_strToTitle(nullptr, 0, utf16.get_data(), -1, nullptr, lang.ascii().get_data(), &err);
	ERR_FAIL_COND_V_MSG(err != U_BUFFER_OVERFLOW_ERROR, p_string, u_errorName(err));
	upper.resize(len);
	err = U_ZERO_ERROR;
	u_strToTitle(upper.ptrw(), len, utf16.get_data(), -1, nullptr, lang.ascii().get_data(), &err);
	ERR_FAIL_COND_V_MSG(U_FAILURE(err), p_string, u_errorName(err));

	// Convert back to UTF-32.
	return String::utf16(upper.ptr(), len);
}

PackedInt32Array TextServerAdvanced::_string_get_word_breaks(const String &p_string, const String &p_language, int64_t p_chars_per_line) const {
	const String lang = (p_language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale() : p_language;
	// Convert to UTF-16.
	Char16String utf16 = p_string.utf16();

	HashSet<int> breaks;
	UErrorCode err = U_ZERO_ERROR;
	UBreakIterator *bi = ubrk_open(UBRK_WORD, lang.ascii().get_data(), (const UChar *)utf16.get_data(), utf16.length(), &err);
	if (U_SUCCESS(err)) {
		while (ubrk_next(bi) != UBRK_DONE) {
			int pos = _convert_pos(p_string, utf16, ubrk_current(bi));
			if (pos != p_string.length() - 1) {
				breaks.insert(pos);
			}
		}
		ubrk_close(bi);
	}

	PackedInt32Array ret;

	if (p_chars_per_line > 0) {
		int line_start = 0;
		int last_break = -1;
		int line_length = 0;

		for (int i = 0; i < p_string.length(); i++) {
			const char32_t c = p_string[i];

			bool is_lb = is_linebreak(c);
			bool is_ws = is_whitespace(c);
			bool is_p = (u_ispunct(c) && c != 0x005F) || is_underscore(c) || c == '\t' || c == 0xfffc;

			if (is_lb) {
				if (line_length > 0) {
					ret.push_back(line_start);
					ret.push_back(i);
				}
				line_start = i;
				line_length = 0;
				last_break = -1;
				continue;
			} else if (breaks.has(i) || is_ws || is_p) {
				last_break = i;
			}

			if (line_length == p_chars_per_line) {
				if (last_break != -1) {
					int last_break_w_spaces = last_break;
					while (last_break > line_start && is_whitespace(p_string[last_break - 1])) {
						last_break--;
					}
					if (line_start != last_break) {
						ret.push_back(line_start);
						ret.push_back(last_break);
					}
					while (last_break_w_spaces < p_string.length() && is_whitespace(p_string[last_break_w_spaces])) {
						last_break_w_spaces++;
					}
					line_start = last_break_w_spaces;
					if (last_break_w_spaces < i) {
						line_length = i - last_break_w_spaces;
					} else {
						i = last_break_w_spaces;
						line_length = 0;
					}
				} else {
					ret.push_back(line_start);
					ret.push_back(i);
					line_start = i;
					line_length = 0;
				}
				last_break = -1;
			}
			line_length++;
		}
		if (line_length > 0) {
			ret.push_back(line_start);
			ret.push_back(p_string.length());
		}
	} else {
		int word_start = 0; // -1 if no word encountered. Leading spaces are part of a word.
		int word_length = 0;

		for (int i = 0; i < p_string.length(); i++) {
			const char32_t c = p_string[i];

			bool is_lb = is_linebreak(c);
			bool is_ws = is_whitespace(c);
			bool is_p = (u_ispunct(c) && c != 0x005F) || is_underscore(c) || c == '\t' || c == 0xfffc;

			if (word_start == -1) {
				if (!is_lb && !is_ws && !is_p) {
					word_start = i;
				}
				continue;
			}

			if (is_lb) {
				if (word_start != -1 && word_length > 0) {
					ret.push_back(word_start);
					ret.push_back(i);
				}
				word_start = -1;
				word_length = 0;
			} else if (breaks.has(i) || is_ws || is_p) {
				if (word_start != -1 && word_length > 0) {
					ret.push_back(word_start);
					ret.push_back(i);
				}
				if (is_ws || is_p) {
					word_start = -1;
				} else {
					word_start = i;
				}
				word_length = 0;
			}

			word_length++;
		}
		if (word_start != -1 && word_length > 0) {
			ret.push_back(word_start);
			ret.push_back(p_string.length());
		}
	}

	return ret;
}

PackedInt32Array TextServerAdvanced::_string_get_character_breaks(const String &p_string, const String &p_language) const {
	const String lang = (p_language.is_empty()) ? TranslationServer::get_singleton()->get_tool_locale() : p_language;
	// Convert to UTF-16.
	Char16String utf16 = p_string.utf16();

	PackedInt32Array ret;

	UErrorCode err = U_ZERO_ERROR;
	UBreakIterator *bi = ubrk_open(UBRK_CHARACTER, lang.ascii().get_data(), (const UChar *)utf16.get_data(), utf16.length(), &err);
	if (U_SUCCESS(err)) {
		while (ubrk_next(bi) != UBRK_DONE) {
			int pos = _convert_pos(p_string, utf16, ubrk_current(bi));
			ret.push_back(pos);
		}
		ubrk_close(bi);
	} else {
		return TextServer::string_get_character_breaks(p_string, p_language);
	}

	return ret;
}

bool TextServerAdvanced::_is_valid_identifier(const String &p_string) const {
#ifndef ICU_STATIC_DATA
	if (!icu_data_loaded) {
		WARN_PRINT_ONCE("ICU data is not loaded, Unicode security and spoofing detection disabled.");
		return TextServer::is_valid_identifier(p_string);
	}
#endif

	enum UAX31SequenceStatus {
		SEQ_NOT_STARTED,
		SEQ_STARTED,
		SEQ_STARTED_VIR,
		SEQ_NEAR_END,
	};

	const char32_t *str = p_string.ptr();
	int len = p_string.length();

	if (len == 0) {
		return false; // Empty string.
	}

	UErrorCode err = U_ZERO_ERROR;
	Char16String utf16 = p_string.utf16();
	const UNormalizer2 *norm_c = unorm2_getNFCInstance(&err);
	if (U_FAILURE(err)) {
		return false; // Failed to load normalizer.
	}
	bool isnurom = unorm2_isNormalized(norm_c, utf16.get_data(), utf16.length(), &err);
	if (U_FAILURE(err) || !isnurom) {
		return false; // Do not conform to Normalization Form C.
	}

	UAX31SequenceStatus A1_sequence_status = SEQ_NOT_STARTED;
	UScriptCode A1_scr = USCRIPT_INHERITED;
	UAX31SequenceStatus A2_sequence_status = SEQ_NOT_STARTED;
	UScriptCode A2_scr = USCRIPT_INHERITED;
	UAX31SequenceStatus B_sequence_status = SEQ_NOT_STARTED;
	UScriptCode B_scr = USCRIPT_INHERITED;

	for (int i = 0; i < len; i++) {
		err = U_ZERO_ERROR;
		UScriptCode scr = uscript_getScript(str[i], &err);
		if (U_FAILURE(err)) {
			return false; // Invalid script.
		}
		if (uscript_getUsage(scr) != USCRIPT_USAGE_RECOMMENDED) {
			return false; // Not a recommended script.
		}
		uint8_t cat = u_charType(str[i]);
		int32_t jt = u_getIntPropertyValue(str[i], UCHAR_JOINING_TYPE);

		// UAX #31 section 2.3 subsections A1, A2 and B, check ZWNJ and ZWJ usage.
		switch (A1_sequence_status) {
			case SEQ_NEAR_END: {
				if ((A1_scr > USCRIPT_INHERITED) && (scr > USCRIPT_INHERITED) && (scr != A1_scr)) {
					return false; // Mixed script.
				}
				if (jt == U_JT_RIGHT_JOINING || jt == U_JT_DUAL_JOINING) {
					A1_sequence_status = SEQ_NOT_STARTED; // Valid end of sequence, reset.
				} else if (jt != U_JT_TRANSPARENT) {
					return false; // Invalid end of sequence.
				}
			} break;
			case SEQ_STARTED: {
				if ((A1_scr > USCRIPT_INHERITED) && (scr > USCRIPT_INHERITED) && (scr != A1_scr)) {
					A1_sequence_status = SEQ_NOT_STARTED; // Reset.
				} else {
					if (jt != U_JT_TRANSPARENT) {
						if (str[i] == 0x200C /*ZWNJ*/) {
							A1_sequence_status = SEQ_NEAR_END;
							continue;
						} else {
							A1_sequence_status = SEQ_NOT_STARTED; // Reset.
						}
					}
				}
			} break;
			default:
				break;
		}
		if (A1_sequence_status == SEQ_NOT_STARTED) {
			if (jt == U_JT_LEFT_JOINING || jt == U_JT_DUAL_JOINING) {
				A1_sequence_status = SEQ_STARTED;
				A1_scr = scr;
			}
		};

		switch (A2_sequence_status) {
			case SEQ_NEAR_END: {
				if ((A2_scr > USCRIPT_INHERITED) && (scr > USCRIPT_INHERITED) && (scr != A2_scr)) {
					return false; // Mixed script.
				}
				if (cat == U_UPPERCASE_LETTER || cat == U_LOWERCASE_LETTER || cat == U_TITLECASE_LETTER || cat == U_MODIFIER_LETTER || cat == U_OTHER_LETTER) {
					A2_sequence_status = SEQ_NOT_STARTED; // Valid end of sequence, reset.
				} else if (cat != U_MODIFIER_LETTER || u_getCombiningClass(str[i]) == 0) {
					return false; // Invalid end of sequence.
				}
			} break;
			case SEQ_STARTED_VIR: {
				if ((A2_scr > USCRIPT_INHERITED) && (scr > USCRIPT_INHERITED) && (scr != A2_scr)) {
					A2_sequence_status = SEQ_NOT_STARTED; // Reset.
				} else {
					if (str[i] == 0x200C /*ZWNJ*/) {
						A2_sequence_status = SEQ_NEAR_END;
						continue;
					} else if (cat != U_MODIFIER_LETTER || u_getCombiningClass(str[i]) == 0) {
						A2_sequence_status = SEQ_NOT_STARTED; // Reset.
					}
				}
			} break;
			case SEQ_STARTED: {
				if ((A2_scr > USCRIPT_INHERITED) && (scr > USCRIPT_INHERITED) && (scr != A2_scr)) {
					A2_sequence_status = SEQ_NOT_STARTED; // Reset.
				} else {
					if (u_getCombiningClass(str[i]) == 9 /*Virama Combining Class*/) {
						A2_sequence_status = SEQ_STARTED_VIR;
					} else if (cat != U_MODIFIER_LETTER) {
						A2_sequence_status = SEQ_NOT_STARTED; // Reset.
					}
				}
			} break;
			default:
				break;
		}
		if (A2_sequence_status == SEQ_NOT_STARTED) {
			if (cat == U_UPPERCASE_LETTER || cat == U_LOWERCASE_LETTER || cat == U_TITLECASE_LETTER || cat == U_MODIFIER_LETTER || cat == U_OTHER_LETTER) {
				A2_sequence_status = SEQ_STARTED;
				A2_scr = scr;
			}
		}

		switch (B_sequence_status) {
			case SEQ_NEAR_END: {
				if ((B_scr > USCRIPT_INHERITED) && (scr > USCRIPT_INHERITED) && (scr != B_scr)) {
					return false; // Mixed script.
				}
				if (u_getIntPropertyValue(str[i], UCHAR_INDIC_SYLLABIC_CATEGORY) != U_INSC_VOWEL_DEPENDENT) {
					B_sequence_status = SEQ_NOT_STARTED; // Valid end of sequence, reset.
				} else {
					return false; // Invalid end of sequence.
				}
			} break;
			case SEQ_STARTED_VIR: {
				if ((B_scr > USCRIPT_INHERITED) && (scr > USCRIPT_INHERITED) && (scr != B_scr)) {
					B_sequence_status = SEQ_NOT_STARTED; // Reset.
				} else {
					if (str[i] == 0x200D /*ZWJ*/) {
						B_sequence_status = SEQ_NEAR_END;
						continue;
					} else if (cat != U_MODIFIER_LETTER || u_getCombiningClass(str[i]) == 0) {
						B_sequence_status = SEQ_NOT_STARTED; // Reset.
					}
				}
			} break;
			case SEQ_STARTED: {
				if ((B_scr > USCRIPT_INHERITED) && (scr > USCRIPT_INHERITED) && (scr != B_scr)) {
					B_sequence_status = SEQ_NOT_STARTED; // Reset.
				} else {
					if (u_getCombiningClass(str[i]) == 9 /*Virama Combining Class*/) {
						B_sequence_status = SEQ_STARTED_VIR;
					} else if (cat != U_MODIFIER_LETTER) {
						B_sequence_status = SEQ_NOT_STARTED; // Reset.
					}
				}
			} break;
			default:
				break;
		}
		if (B_sequence_status == SEQ_NOT_STARTED) {
			if (cat == U_UPPERCASE_LETTER || cat == U_LOWERCASE_LETTER || cat == U_TITLECASE_LETTER || cat == U_MODIFIER_LETTER || cat == U_OTHER_LETTER) {
				B_sequence_status = SEQ_STARTED;
				B_scr = scr;
			}
		}

		if (u_hasBinaryProperty(str[i], UCHAR_PATTERN_SYNTAX) || u_hasBinaryProperty(str[i], UCHAR_PATTERN_WHITE_SPACE) || u_hasBinaryProperty(str[i], UCHAR_NONCHARACTER_CODE_POINT)) {
			return false; // Not a XID_Start or XID_Continue character.
		}
		if (i == 0) {
			if (!(cat == U_LOWERCASE_LETTER || cat == U_UPPERCASE_LETTER || cat == U_TITLECASE_LETTER || cat == U_OTHER_LETTER || cat == U_MODIFIER_LETTER || cat == U_LETTER_NUMBER || str[0] == 0x2118 || str[0] == 0x212E || str[0] == 0x309B || str[0] == 0x309C || str[0] == 0x005F)) {
				return false; // Not a XID_Start character.
			}
		} else {
			if (!(cat == U_LOWERCASE_LETTER || cat == U_UPPERCASE_LETTER || cat == U_TITLECASE_LETTER || cat == U_OTHER_LETTER || cat == U_MODIFIER_LETTER || cat == U_LETTER_NUMBER || cat == U_NON_SPACING_MARK || cat == U_COMBINING_SPACING_MARK || cat == U_DECIMAL_DIGIT_NUMBER || cat == U_CONNECTOR_PUNCTUATION || str[i] == 0x2118 || str[i] == 0x212E || str[i] == 0x309B || str[i] == 0x309C || str[i] == 0x1369 || str[i] == 0x1371 || str[i] == 0x00B7 || str[i] == 0x0387 || str[i] == 0x19DA || str[i] == 0x0E33 || str[i] == 0x0EB3 || str[i] == 0xFF9E || str[i] == 0xFF9F)) {
				return false; // Not a XID_Continue character.
			}
		}
	}
	return true;
}

bool TextServerAdvanced::_is_valid_letter(uint64_t p_unicode) const {
#ifndef ICU_STATIC_DATA
	if (!icu_data_loaded) {
		return TextServer::is_valid_letter(p_unicode);
	}
#endif

	return u_isalpha(p_unicode);
}

void TextServerAdvanced::_update_settings() {
	lcd_subpixel_layout.set((TextServer::FontLCDSubpixelLayout)(int)GLOBAL_GET("gui/theme/lcd_subpixel_layout"));
}

TextServerAdvanced::TextServerAdvanced() {
	_insert_num_systems_lang();
	_insert_feature_sets();
	_bmp_create_font_funcs();
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &TextServerAdvanced::_update_settings));
}

void TextServerAdvanced::_cleanup() {
	_THREAD_SAFE_METHOD_
	for (const KeyValue<SystemFontKey, SystemFontCache> &E : system_fonts) {
		const Vector<SystemFontCacheRec> &sysf_cache = E.value.var;
		for (const SystemFontCacheRec &F : sysf_cache) {
			_free_rid(F.rid);
		}
	}
	system_fonts.clear();
	system_font_data.clear();
}

TextServerAdvanced::~TextServerAdvanced() {
	_bmp_free_font_funcs();
#ifdef MODULE_FREETYPE_ENABLED
	if (ft_library != nullptr) {
		FT_Done_FreeType(ft_library);
	}
#endif
	if (sc_spoof != nullptr) {
		uspoof_close(sc_spoof);
		sc_spoof = nullptr;
	}
	if (sc_conf != nullptr) {
		uspoof_close(sc_conf);
		sc_conf = nullptr;
	}
	if (allowed != nullptr) {
		uset_close(allowed);
		allowed = nullptr;
	}

	std::atexit(u_cleanup);
}
