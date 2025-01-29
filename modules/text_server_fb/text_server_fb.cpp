/**************************************************************************/
/*  text_server_fb.cpp                                                    */
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

#include "text_server_fb.h"

#ifdef GDEXTENSION
// Headers for building as GDExtension plug-in.

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/core/error_macros.hpp>

#define OT_TAG(m_c1, m_c2, m_c3, m_c4) ((int32_t)((((uint32_t)(m_c1) & 0xff) << 24) | (((uint32_t)(m_c2) & 0xff) << 16) | (((uint32_t)(m_c3) & 0xff) << 8) | ((uint32_t)(m_c4) & 0xff)))

using namespace godot;

#define GLOBAL_GET(m_var) ProjectSettings::get_singleton()->get_setting_with_override(m_var)

#elif defined(GODOT_MODULE)
// Headers for building as built-in module.

#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/string/print_string.h"
#include "core/string/translation_server.h"

#include "modules/modules_enabled.gen.h" // For freetype, msdfgen, svg.

#endif

// Thirdparty headers.

#ifdef MODULE_MSDFGEN_ENABLED
#ifdef _MSC_VER
#pragma warning(disable : 4458)
#endif
#include <core/EdgeHolder.h>
#include <core/ShapeDistanceFinder.h>
#include <core/contour-combiners.h>
#include <core/edge-selectors.h>
#include <msdfgen.h>
#ifdef _MSC_VER
#pragma warning(default : 4458)
#endif
#endif

#ifdef MODULE_FREETYPE_ENABLED
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#ifdef MODULE_SVG_ENABLED
#include "thorvg_svg_in_ot.h"
#endif
#endif

/*************************************************************************/

bool TextServerFallback::_has_feature(Feature p_feature) const {
	switch (p_feature) {
		case FEATURE_SIMPLE_LAYOUT:
		case FEATURE_FONT_BITMAP:
#ifdef MODULE_FREETYPE_ENABLED
		case FEATURE_FONT_DYNAMIC:
#endif
#ifdef MODULE_MSDFGEN_ENABLED
		case FEATURE_FONT_MSDF:
#endif
			return true;
		default: {
		}
	}
	return false;
}

String TextServerFallback::_get_name() const {
#ifdef GDEXTENSION
	return "Fallback (GDExtension)";
#elif defined(GODOT_MODULE)
	return "Fallback (Built-in)";
#endif
}

int64_t TextServerFallback::_get_features() const {
	int64_t interface_features = FEATURE_SIMPLE_LAYOUT | FEATURE_FONT_BITMAP;
#ifdef MODULE_FREETYPE_ENABLED
	interface_features |= FEATURE_FONT_DYNAMIC;
#endif
#ifdef MODULE_MSDFGEN_ENABLED
	interface_features |= FEATURE_FONT_MSDF;
#endif

	return interface_features;
}

void TextServerFallback::_free_rid(const RID &p_rid) {
	_THREAD_SAFE_METHOD_
	if (font_owner.owns(p_rid)) {
		MutexLock ftlock(ft_mutex);

		FontFallback *fd = font_owner.get_or_null(p_rid);
		{
			MutexLock lock(fd->mutex);
			font_owner.free(p_rid);
		}
		memdelete(fd);
	} else if (font_var_owner.owns(p_rid)) {
		MutexLock ftlock(ft_mutex);

		FontFallbackLinkedVariation *fdv = font_var_owner.get_or_null(p_rid);
		{
			font_var_owner.free(p_rid);
		}
		memdelete(fdv);
	} else if (shaped_owner.owns(p_rid)) {
		ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_rid);
		{
			MutexLock lock(sd->mutex);
			shaped_owner.free(p_rid);
		}
		memdelete(sd);
	}
}

bool TextServerFallback::_has(const RID &p_rid) {
	_THREAD_SAFE_METHOD_
	return font_owner.owns(p_rid) || shaped_owner.owns(p_rid);
}

String TextServerFallback::_get_support_data_filename() const {
	return "";
}

String TextServerFallback::_get_support_data_info() const {
	return "Not supported";
}

bool TextServerFallback::_load_support_data(const String &p_filename) {
	return false; // No extra data used.
}

bool TextServerFallback::_save_support_data(const String &p_filename) const {
	return false; // No extra data used.
}

PackedByteArray TextServerFallback::_get_support_data() const {
	return PackedByteArray(); // No extra data used.
}

bool TextServerFallback::_is_locale_right_to_left(const String &p_locale) const {
	return false; // No RTL support.
}

_FORCE_INLINE_ void TextServerFallback::_insert_feature(const StringName &p_name, int32_t p_tag) {
	feature_sets.insert(p_name, p_tag);
	feature_sets_inv.insert(p_tag, p_name);
}

void TextServerFallback::_insert_feature_sets() {
	// Registered OpenType variation tag.
	_insert_feature("italic", OT_TAG('i', 't', 'a', 'l'));
	_insert_feature("optical_size", OT_TAG('o', 'p', 's', 'z'));
	_insert_feature("slant", OT_TAG('s', 'l', 'n', 't'));
	_insert_feature("width", OT_TAG('w', 'd', 't', 'h'));
	_insert_feature("weight", OT_TAG('w', 'g', 'h', 't'));
}

_FORCE_INLINE_ int32_t ot_tag_from_string(const char *p_str, int p_len) {
	char tag[4];
	uint32_t i;

	if (!p_str || !p_len || !*p_str) {
		return OT_TAG(0, 0, 0, 0);
	}

	if (p_len < 0 || p_len > 4) {
		p_len = 4;
	}
	for (i = 0; i < (uint32_t)p_len && p_str[i]; i++) {
		tag[i] = p_str[i];
	}

	for (; i < 4; i++) {
		tag[i] = ' ';
	}

	return OT_TAG(tag[0], tag[1], tag[2], tag[3]);
}

int64_t TextServerFallback::_name_to_tag(const String &p_name) const {
	if (feature_sets.has(p_name)) {
		return feature_sets[p_name];
	}

	// No readable name, use tag string.
	return ot_tag_from_string(p_name.replace("custom_", "").ascii().get_data(), -1);
}

_FORCE_INLINE_ void ot_tag_to_string(int32_t p_tag, char *p_buf) {
	p_buf[0] = (char)(uint8_t)(p_tag >> 24);
	p_buf[1] = (char)(uint8_t)(p_tag >> 16);
	p_buf[2] = (char)(uint8_t)(p_tag >> 8);
	p_buf[3] = (char)(uint8_t)(p_tag >> 0);
}

String TextServerFallback::_tag_to_name(int64_t p_tag) const {
	if (feature_sets_inv.has(p_tag)) {
		return feature_sets_inv[p_tag];
	}

	// No readable name, use tag string.
	char name[5];
	memset(name, 0, 5);
	ot_tag_to_string(p_tag, name);
	return String("custom_") + String(name);
}

/*************************************************************************/
/* Font Glyph Rendering                                                  */
/*************************************************************************/

_FORCE_INLINE_ TextServerFallback::FontTexturePosition TextServerFallback::find_texture_pos_for_glyph(FontForSizeFallback *p_data, int p_color_size, Image::Format p_image_format, int p_width, int p_height, bool p_msdf) const {
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

void TextServerFallback::_generateMTSDF_threaded(void *p_td, uint32_t p_y) {
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

_FORCE_INLINE_ TextServerFallback::FontGlyph TextServerFallback::rasterize_msdf(FontFallback *p_font_data, FontForSizeFallback *p_data, int p_pixel_range, int p_rect_margin, FT_Outline *p_outline, const Vector2 &p_advance) const {
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

		WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_native_group_task(&TextServerFallback::_generateMTSDF_threaded, &td, h, -1, true, String("TextServerFBRenderMSDF"));
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
_FORCE_INLINE_ TextServerFallback::FontGlyph TextServerFallback::rasterize_bitmap(FontForSizeFallback *p_data, int p_rect_margin, FT_Bitmap p_bitmap, int p_yofs, int p_xofs, const Vector2 &p_advance, bool p_bgra) const {
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

_FORCE_INLINE_ bool TextServerFallback::_ensure_glyph(FontFallback *p_font_data, const Vector2i &p_size, int32_t p_glyph, FontGlyph &r_glyph) const {
	FontForSizeFallback *fd = nullptr;
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

		glyph_index = FT_Get_Char_Index(fd->face, glyph_index);

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

_FORCE_INLINE_ bool TextServerFallback::_ensure_cache_for_size(FontFallback *p_font_data, const Vector2i &p_size, FontForSizeFallback *&r_cache_for_size, bool p_silent) const {
	ERR_FAIL_COND_V(p_size.x <= 0, false);

	HashMap<Vector2i, FontForSizeFallback *>::Iterator E = p_font_data->cache.find(p_size);
	if (E) {
		r_cache_for_size = E->value;
		return true;
	}

	r_cache_for_size = nullptr;
	FontForSizeFallback *fd = memnew(FontForSizeFallback);
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
			FT_Set_Pixel_Sizes(fd->face, 0, Math::round(fd->size.x * fd->oversampling));
			if (fd->face->size->metrics.y_ppem != 0) {
				fd->scale = ((double)fd->size.x * fd->oversampling) / (double)fd->face->size->metrics.y_ppem;
			}
		}

		fd->ascent = (fd->face->size->metrics.ascender / 64.0) / fd->oversampling * fd->scale;
		fd->descent = (-fd->face->size->metrics.descender / 64.0) / fd->oversampling * fd->scale;
		fd->underline_position = (-FT_MulFix(fd->face->underline_position, fd->face->size->metrics.y_scale) / 64.0) / fd->oversampling * fd->scale;
		fd->underline_thickness = (FT_MulFix(fd->face->underline_thickness, fd->face->size->metrics.y_scale) / 64.0) / fd->oversampling * fd->scale;

		if (!p_font_data->face_init) {
			// When a font does not provide a `family_name`, FreeType tries to synthesize one based on other names.
			// FreeType automatically converts non-ASCII characters to "?" in the synthesized name.
			// To avoid that behavior, use the format-specific name directly if available.
			if (FT_IS_SFNT(fd->face)) {
				int name_count = FT_Get_Sfnt_Name_Count(fd->face);
				for (int i = 0; i < name_count; i++) {
					FT_SfntName sfnt_name;
					if (FT_Get_Sfnt_Name(fd->face, i, &sfnt_name) != 0) {
						continue;
					}
					if (sfnt_name.name_id != TT_NAME_ID_FONT_FAMILY && sfnt_name.name_id != TT_NAME_ID_TYPOGRAPHIC_FAMILY) {
						continue;
					}
					if (!p_font_data->font_name.is_empty() && sfnt_name.language_id != TT_MS_LANGID_ENGLISH_UNITED_STATES) {
						continue;
					}

					switch (sfnt_name.platform_id) {
						case TT_PLATFORM_APPLE_UNICODE: {
							p_font_data->font_name.parse_utf16((const char16_t *)sfnt_name.string, sfnt_name.string_len / 2, false);
						} break;

						case TT_PLATFORM_MICROSOFT: {
							if (sfnt_name.encoding_id == TT_MS_ID_UNICODE_CS || sfnt_name.encoding_id == TT_MS_ID_UCS_4) {
								p_font_data->font_name.parse_utf16((const char16_t *)sfnt_name.string, sfnt_name.string_len / 2, false);
							}
						} break;
					}
				}
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

			Vector<FT_Fixed> coords;
			coords.resize(amaster->num_axis);

			FT_Get_Var_Design_Coordinates(fd->face, coords.size(), coords.ptrw());

			for (FT_UInt i = 0; i < amaster->num_axis; i++) {
				// Reset to default.
				int32_t var_tag = amaster->axis[i].tag;
				double var_value = (double)amaster->axis[i].def / 65536.0;
				coords.write[i] = amaster->axis[i].def;

				if (p_font_data->variation_coordinates.has(var_tag)) {
					var_value = p_font_data->variation_coordinates[var_tag];
					coords.write[i] = CLAMP(var_value * 65536.0, amaster->axis[i].minimum, amaster->axis[i].maximum);
				}

				if (p_font_data->variation_coordinates.has(tag_to_name(var_tag))) {
					var_value = p_font_data->variation_coordinates[tag_to_name(var_tag)];
					coords.write[i] = CLAMP(var_value * 65536.0, amaster->axis[i].minimum, amaster->axis[i].maximum);
				}
			}

			FT_Set_Var_Design_Coordinates(fd->face, coords.size(), coords.ptrw());
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
	}

	p_font_data->cache.insert(p_size, fd);
	r_cache_for_size = fd;
	return true;
}

_FORCE_INLINE_ bool TextServerFallback::_font_validate(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	return _ensure_cache_for_size(fd, size, ffsd, true);
}

_FORCE_INLINE_ void TextServerFallback::_font_clear_cache(FontFallback *p_font_data) {
	MutexLock ftlock(ft_mutex);

	for (const KeyValue<Vector2i, FontForSizeFallback *> &E : p_font_data->cache) {
		memdelete(E.value);
	}

	p_font_data->cache.clear();
	p_font_data->face_init = false;
	p_font_data->supported_varaitions.clear();
}

RID TextServerFallback::_create_font() {
	_THREAD_SAFE_METHOD_

	FontFallback *fd = memnew(FontFallback);

	return font_owner.make_rid(fd);
}

RID TextServerFallback::_create_font_linked_variation(const RID &p_font_rid) {
	_THREAD_SAFE_METHOD_

	RID rid = p_font_rid;
	FontFallbackLinkedVariation *fdv = font_var_owner.get_or_null(rid);
	if (unlikely(fdv)) {
		rid = fdv->base_font;
	}
	ERR_FAIL_COND_V(!font_owner.owns(rid), RID());

	FontFallbackLinkedVariation *new_fdv = memnew(FontFallbackLinkedVariation);
	new_fdv->base_font = rid;

	return font_var_owner.make_rid(new_fdv);
}

void TextServerFallback::_font_set_data(const RID &p_font_rid, const PackedByteArray &p_data) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	_font_clear_cache(fd);
	fd->data = p_data;
	fd->data_ptr = fd->data.ptr();
	fd->data_size = fd->data.size();
}

void TextServerFallback::_font_set_data_ptr(const RID &p_font_rid, const uint8_t *p_data_ptr, int64_t p_data_size) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	_font_clear_cache(fd);
	fd->data.resize(0);
	fd->data_ptr = p_data_ptr;
	fd->data_size = p_data_size;
}

void TextServerFallback::_font_set_style(const RID &p_font_rid, BitField<FontStyle> p_style) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->style_flags = p_style;
}

void TextServerFallback::_font_set_face_index(const RID &p_font_rid, int64_t p_face_index) {
	ERR_FAIL_COND(p_face_index < 0);
	ERR_FAIL_COND(p_face_index >= 0x7FFF);

	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->face_index != p_face_index) {
		fd->face_index = p_face_index;
		_font_clear_cache(fd);
	}
}

int64_t TextServerFallback::_font_get_face_index(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	return fd->face_index;
}

int64_t TextServerFallback::_font_get_face_count(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
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

BitField<TextServer::FontStyle> TextServerFallback::_font_get_style(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0);
	return fd->style_flags;
}

void TextServerFallback::_font_set_style_name(const RID &p_font_rid, const String &p_name) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->style_name = p_name;
}

String TextServerFallback::_font_get_style_name(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, String());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), String());
	return fd->style_name;
}

void TextServerFallback::_font_set_weight(const RID &p_font_rid, int64_t p_weight) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->weight = CLAMP(p_weight, 100, 999);
}

int64_t TextServerFallback::_font_get_weight(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 400);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 400);
	return fd->weight;
}

void TextServerFallback::_font_set_stretch(const RID &p_font_rid, int64_t p_stretch) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->stretch = CLAMP(p_stretch, 50, 200);
}

int64_t TextServerFallback::_font_get_stretch(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 100);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 100);
	return fd->stretch;
}

void TextServerFallback::_font_set_name(const RID &p_font_rid, const String &p_name) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->font_name = p_name;
}

String TextServerFallback::_font_get_name(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, String());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), String());
	return fd->font_name;
}

void TextServerFallback::_font_set_antialiasing(const RID &p_font_rid, TextServer::FontAntialiasing p_antialiasing) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->antialiasing != p_antialiasing) {
		_font_clear_cache(fd);
		fd->antialiasing = p_antialiasing;
	}
}

TextServer::FontAntialiasing TextServerFallback::_font_get_antialiasing(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, TextServer::FONT_ANTIALIASING_NONE);

	MutexLock lock(fd->mutex);
	return fd->antialiasing;
}

void TextServerFallback::_font_set_disable_embedded_bitmaps(const RID &p_font_rid, bool p_disable_embedded_bitmaps) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->disable_embedded_bitmaps != p_disable_embedded_bitmaps) {
		_font_clear_cache(fd);
		fd->disable_embedded_bitmaps = p_disable_embedded_bitmaps;
	}
}

bool TextServerFallback::_font_get_disable_embedded_bitmaps(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->disable_embedded_bitmaps;
}

void TextServerFallback::_font_set_generate_mipmaps(const RID &p_font_rid, bool p_generate_mipmaps) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->mipmaps != p_generate_mipmaps) {
		for (KeyValue<Vector2i, FontForSizeFallback *> &E : fd->cache) {
			for (int i = 0; i < E.value->textures.size(); i++) {
				E.value->textures.write[i].dirty = true;
				E.value->textures.write[i].texture = Ref<ImageTexture>();
			}
		}
		fd->mipmaps = p_generate_mipmaps;
	}
}

bool TextServerFallback::_font_get_generate_mipmaps(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->mipmaps;
}

void TextServerFallback::_font_set_multichannel_signed_distance_field(const RID &p_font_rid, bool p_msdf) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->msdf != p_msdf) {
		_font_clear_cache(fd);
		fd->msdf = p_msdf;
	}
}

bool TextServerFallback::_font_is_multichannel_signed_distance_field(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->msdf;
}

void TextServerFallback::_font_set_msdf_pixel_range(const RID &p_font_rid, int64_t p_msdf_pixel_range) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->msdf_range != p_msdf_pixel_range) {
		_font_clear_cache(fd);
		fd->msdf_range = p_msdf_pixel_range;
	}
}

int64_t TextServerFallback::_font_get_msdf_pixel_range(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->msdf_range;
}

void TextServerFallback::_font_set_msdf_size(const RID &p_font_rid, int64_t p_msdf_size) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->msdf_source_size != p_msdf_size) {
		_font_clear_cache(fd);
		fd->msdf_source_size = p_msdf_size;
	}
}

int64_t TextServerFallback::_font_get_msdf_size(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	return fd->msdf_source_size;
}

void TextServerFallback::_font_set_fixed_size(const RID &p_font_rid, int64_t p_fixed_size) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->fixed_size = p_fixed_size;
}

int64_t TextServerFallback::_font_get_fixed_size(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	return fd->fixed_size;
}

void TextServerFallback::_font_set_fixed_size_scale_mode(const RID &p_font_rid, TextServer::FixedSizeScaleMode p_fixed_size_scale_mode) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->fixed_size_scale_mode = p_fixed_size_scale_mode;
}

TextServer::FixedSizeScaleMode TextServerFallback::_font_get_fixed_size_scale_mode(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, FIXED_SIZE_SCALE_DISABLE);

	MutexLock lock(fd->mutex);
	return fd->fixed_size_scale_mode;
}

void TextServerFallback::_font_set_allow_system_fallback(const RID &p_font_rid, bool p_allow_system_fallback) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->allow_system_fallback = p_allow_system_fallback;
}

bool TextServerFallback::_font_is_allow_system_fallback(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->allow_system_fallback;
}

void TextServerFallback::_font_set_force_autohinter(const RID &p_font_rid, bool p_force_autohinter) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->force_autohinter != p_force_autohinter) {
		_font_clear_cache(fd);
		fd->force_autohinter = p_force_autohinter;
	}
}

bool TextServerFallback::_font_is_force_autohinter(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->force_autohinter;
}

void TextServerFallback::_font_set_hinting(const RID &p_font_rid, TextServer::Hinting p_hinting) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->hinting != p_hinting) {
		_font_clear_cache(fd);
		fd->hinting = p_hinting;
	}
}

TextServer::Hinting TextServerFallback::_font_get_hinting(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, HINTING_NONE);

	MutexLock lock(fd->mutex);
	return fd->hinting;
}

void TextServerFallback::_font_set_subpixel_positioning(const RID &p_font_rid, TextServer::SubpixelPositioning p_subpixel) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->subpixel_positioning = p_subpixel;
}

TextServer::SubpixelPositioning TextServerFallback::_font_get_subpixel_positioning(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, SUBPIXEL_POSITIONING_DISABLED);

	MutexLock lock(fd->mutex);
	return fd->subpixel_positioning;
}

void TextServerFallback::_font_set_keep_rounding_remainders(const RID &p_font_rid, bool p_keep_rounding_remainders) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->keep_rounding_remainders = p_keep_rounding_remainders;
}

bool TextServerFallback::_font_get_keep_rounding_remainders(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->keep_rounding_remainders;
}

void TextServerFallback::_font_set_embolden(const RID &p_font_rid, double p_strength) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->embolden != p_strength) {
		_font_clear_cache(fd);
		fd->embolden = p_strength;
	}
}

double TextServerFallback::_font_get_embolden(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	return fd->embolden;
}

void TextServerFallback::_font_set_spacing(const RID &p_font_rid, SpacingType p_spacing, int64_t p_value) {
	ERR_FAIL_INDEX((int)p_spacing, 4);
	FontFallbackLinkedVariation *fdv = font_var_owner.get_or_null(p_font_rid);
	if (fdv) {
		if (fdv->extra_spacing[p_spacing] != p_value) {
			fdv->extra_spacing[p_spacing] = p_value;
		}
	} else {
		FontFallback *fd = font_owner.get_or_null(p_font_rid);
		ERR_FAIL_NULL(fd);

		MutexLock lock(fd->mutex);
		if (fd->extra_spacing[p_spacing] != p_value) {
			_font_clear_cache(fd);
			fd->extra_spacing[p_spacing] = p_value;
		}
	}
}

int64_t TextServerFallback::_font_get_spacing(const RID &p_font_rid, SpacingType p_spacing) const {
	ERR_FAIL_INDEX_V((int)p_spacing, 4, 0);
	FontFallbackLinkedVariation *fdv = font_var_owner.get_or_null(p_font_rid);
	if (fdv) {
		return fdv->extra_spacing[p_spacing];
	} else {
		FontFallback *fd = font_owner.get_or_null(p_font_rid);
		ERR_FAIL_NULL_V(fd, 0);

		MutexLock lock(fd->mutex);
		return fd->extra_spacing[p_spacing];
	}
}

void TextServerFallback::_font_set_baseline_offset(const RID &p_font_rid, double p_baseline_offset) {
	FontFallbackLinkedVariation *fdv = font_var_owner.get_or_null(p_font_rid);
	if (fdv) {
		if (fdv->baseline_offset != p_baseline_offset) {
			fdv->baseline_offset = p_baseline_offset;
		}
	} else {
		FontFallback *fd = font_owner.get_or_null(p_font_rid);
		ERR_FAIL_NULL(fd);

		MutexLock lock(fd->mutex);
		if (fd->baseline_offset != p_baseline_offset) {
			_font_clear_cache(fd);
			fd->baseline_offset = p_baseline_offset;
		}
	}
}

double TextServerFallback::_font_get_baseline_offset(const RID &p_font_rid) const {
	FontFallbackLinkedVariation *fdv = font_var_owner.get_or_null(p_font_rid);
	if (fdv) {
		return fdv->baseline_offset;
	} else {
		FontFallback *fd = font_owner.get_or_null(p_font_rid);
		ERR_FAIL_NULL_V(fd, 0.0);

		MutexLock lock(fd->mutex);
		return fd->baseline_offset;
	}
}

void TextServerFallback::_font_set_transform(const RID &p_font_rid, const Transform2D &p_transform) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->transform != p_transform) {
		_font_clear_cache(fd);
		fd->transform = p_transform;
	}
}

Transform2D TextServerFallback::_font_get_transform(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Transform2D());

	MutexLock lock(fd->mutex);
	return fd->transform;
}

void TextServerFallback::_font_set_variation_coordinates(const RID &p_font_rid, const Dictionary &p_variation_coordinates) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (!fd->variation_coordinates.recursive_equal(p_variation_coordinates, 1)) {
		_font_clear_cache(fd);
		fd->variation_coordinates = p_variation_coordinates.duplicate();
	}
}

Dictionary TextServerFallback::_font_get_variation_coordinates(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Dictionary());

	MutexLock lock(fd->mutex);
	return fd->variation_coordinates;
}

void TextServerFallback::_font_set_oversampling(const RID &p_font_rid, double p_oversampling) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	if (fd->oversampling != p_oversampling) {
		_font_clear_cache(fd);
		fd->oversampling = p_oversampling;
	}
}

double TextServerFallback::_font_get_oversampling(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	return fd->oversampling;
}

TypedArray<Vector2i> TextServerFallback::_font_get_size_cache_list(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, TypedArray<Vector2i>());

	MutexLock lock(fd->mutex);
	TypedArray<Vector2i> ret;
	for (const KeyValue<Vector2i, FontForSizeFallback *> &E : fd->cache) {
		ret.push_back(E.key);
	}
	return ret;
}

void TextServerFallback::_font_clear_size_cache(const RID &p_font_rid) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	MutexLock ftlock(ft_mutex);
	for (const KeyValue<Vector2i, FontForSizeFallback *> &E : fd->cache) {
		memdelete(E.value);
	}
	fd->cache.clear();
}

void TextServerFallback::_font_remove_size_cache(const RID &p_font_rid, const Vector2i &p_size) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	MutexLock ftlock(ft_mutex);
	if (fd->cache.has(p_size)) {
		memdelete(fd->cache[p_size]);
		fd->cache.erase(p_size);
	}
}

void TextServerFallback::_font_set_ascent(const RID &p_font_rid, int64_t p_size, double p_ascent) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->ascent = p_ascent;
}

double TextServerFallback::_font_get_ascent(const RID &p_font_rid, int64_t p_size) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_set_descent(const RID &p_font_rid, int64_t p_size, double p_descent) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->descent = p_descent;
}

double TextServerFallback::_font_get_descent(const RID &p_font_rid, int64_t p_size) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_set_underline_position(const RID &p_font_rid, int64_t p_size, double p_underline_position) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->underline_position = p_underline_position;
}

double TextServerFallback::_font_get_underline_position(const RID &p_font_rid, int64_t p_size) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_set_underline_thickness(const RID &p_font_rid, int64_t p_size, double p_underline_thickness) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->underline_thickness = p_underline_thickness;
}

double TextServerFallback::_font_get_underline_thickness(const RID &p_font_rid, int64_t p_size) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_set_scale(const RID &p_font_rid, int64_t p_size, double p_scale) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
#ifdef MODULE_FREETYPE_ENABLED
	if (ffsd->face) {
		return; // Do not override scale for dynamic fonts, it's calculated automatically.
	}
#endif
	ffsd->scale = p_scale;
}

double TextServerFallback::_font_get_scale(const RID &p_font_rid, int64_t p_size) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0.0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

int64_t TextServerFallback::_font_get_texture_count(const RID &p_font_rid, const Vector2i &p_size) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, 0);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), 0);

	return ffsd->textures.size();
}

void TextServerFallback::_font_clear_textures(const RID &p_font_rid, const Vector2i &p_size) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);
	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->textures.clear();
}

void TextServerFallback::_font_remove_texture(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ERR_FAIL_INDEX(p_texture_index, ffsd->textures.size());

	ffsd->textures.remove_at(p_texture_index);
}

void TextServerFallback::_font_set_texture_image(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index, const Ref<Image> &p_image) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);
	ERR_FAIL_COND(p_image.is_null());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
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

Ref<Image> TextServerFallback::_font_get_texture_image(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Ref<Image>());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Ref<Image>());
	ERR_FAIL_INDEX_V(p_texture_index, ffsd->textures.size(), Ref<Image>());

	const ShelfPackTexture &tex = ffsd->textures[p_texture_index];
	return tex.image;
}

void TextServerFallback::_font_set_texture_offsets(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index, const PackedInt32Array &p_offsets) {
	ERR_FAIL_COND(p_offsets.size() % 4 != 0);
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
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

PackedInt32Array TextServerFallback::_font_get_texture_offsets(const RID &p_font_rid, const Vector2i &p_size, int64_t p_texture_index) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, PackedInt32Array());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
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

PackedInt32Array TextServerFallback::_font_get_glyph_list(const RID &p_font_rid, const Vector2i &p_size) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, PackedInt32Array());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), PackedInt32Array());

	PackedInt32Array ret;
	const HashMap<int32_t, FontGlyph> &gl = ffsd->glyph_map;
	for (const KeyValue<int32_t, FontGlyph> &E : gl) {
		ret.push_back(E.key);
	}
	return ret;
}

void TextServerFallback::_font_clear_glyphs(const RID &p_font_rid, const Vector2i &p_size) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	ffsd->glyph_map.clear();
}

void TextServerFallback::_font_remove_glyph(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	ffsd->glyph_map.erase(p_glyph);
}

Vector2 TextServerFallback::_font_get_glyph_advance(const RID &p_font_rid, int64_t p_size, int64_t p_glyph) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Vector2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_set_glyph_advance(const RID &p_font_rid, int64_t p_size, int64_t p_glyph, const Vector2 &p_advance) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	FontGlyph &fgl = ffsd->glyph_map[p_glyph];

	fgl.advance = p_advance;
	fgl.found = true;
}

Vector2 TextServerFallback::_font_get_glyph_offset(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Vector2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_set_glyph_offset(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Vector2 &p_offset) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	FontGlyph &fgl = ffsd->glyph_map[p_glyph];

	fgl.rect.position = p_offset;
	fgl.found = true;
}

Vector2 TextServerFallback::_font_get_glyph_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Vector2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_set_glyph_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Vector2 &p_gl_size) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	FontGlyph &fgl = ffsd->glyph_map[p_glyph];

	fgl.rect.size = p_gl_size;
	fgl.found = true;
}

Rect2 TextServerFallback::_font_get_glyph_uv_rect(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Rect2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_set_glyph_uv_rect(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, const Rect2 &p_uv_rect) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	FontGlyph &fgl = ffsd->glyph_map[p_glyph];

	fgl.uv_rect = p_uv_rect;
	fgl.found = true;
}

int64_t TextServerFallback::_font_get_glyph_texture_idx(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, -1);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_set_glyph_texture_idx(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph, int64_t p_texture_idx) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));

	FontGlyph &fgl = ffsd->glyph_map[p_glyph];

	fgl.texture_idx = p_texture_idx;
	fgl.found = true;
}

RID TextServerFallback::_font_get_glyph_texture_rid(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, RID());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

Size2 TextServerFallback::_font_get_glyph_texture_size(const RID &p_font_rid, const Vector2i &p_size, int64_t p_glyph) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Size2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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

Dictionary TextServerFallback::_font_get_glyph_contours(const RID &p_font_rid, int64_t p_size, int64_t p_index) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Dictionary());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Dictionary());

#ifdef MODULE_FREETYPE_ENABLED
	PackedVector3Array points;
	PackedInt32Array contours;

	int32_t index = p_index & 0xffffff; // Remove subpixel shifts.

	int error = FT_Load_Glyph(ffsd->face, FT_Get_Char_Index(ffsd->face, index), FT_LOAD_NO_BITMAP | (fd->force_autohinter ? FT_LOAD_FORCE_AUTOHINT : 0));
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

TypedArray<Vector2i> TextServerFallback::_font_get_kerning_list(const RID &p_font_rid, int64_t p_size) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, TypedArray<Vector2i>());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), TypedArray<Vector2i>());

	TypedArray<Vector2i> ret;
	for (const KeyValue<Vector2i, Vector2> &E : ffsd->kerning_map) {
		ret.push_back(E.key);
	}
	return ret;
}

void TextServerFallback::_font_clear_kerning_map(const RID &p_font_rid, int64_t p_size) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->kerning_map.clear();
}

void TextServerFallback::_font_remove_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->kerning_map.erase(p_glyph_pair);
}

void TextServerFallback::_font_set_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair, const Vector2 &p_kerning) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	ffsd->kerning_map[p_glyph_pair] = p_kerning;
}

Vector2 TextServerFallback::_font_get_kerning(const RID &p_font_rid, int64_t p_size, const Vector2i &p_glyph_pair) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Vector2());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);

	FontForSizeFallback *ffsd = nullptr;
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
			int32_t glyph_a = FT_Get_Char_Index(ffsd->face, p_glyph_pair.x);
			int32_t glyph_b = FT_Get_Char_Index(ffsd->face, p_glyph_pair.y);
			FT_Get_Kerning(ffsd->face, glyph_a, glyph_b, FT_KERNING_DEFAULT, &delta);
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

int64_t TextServerFallback::_font_get_glyph_index(const RID &p_font_rid, int64_t p_size, int64_t p_char, int64_t p_variation_selector) const {
	ERR_FAIL_COND_V_MSG((p_char >= 0xd800 && p_char <= 0xdfff) || (p_char > 0x10ffff), 0, "Unicode parsing error: Invalid unicode codepoint " + String::num_int64(p_char, 16) + ".");
	return (int64_t)p_char;
}

int64_t TextServerFallback::_font_get_char_from_glyph_index(const RID &p_font_rid, int64_t p_size, int64_t p_glyph_index) const {
	return p_glyph_index;
}

bool TextServerFallback::_font_has_char(const RID &p_font_rid, int64_t p_char) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_COND_V_MSG((p_char >= 0xd800 && p_char <= 0xdfff) || (p_char > 0x10ffff), false, "Unicode parsing error: Invalid unicode codepoint " + String::num_int64(p_char, 16) + ".");
	if (!fd) {
		return false;
	}

	MutexLock lock(fd->mutex);
	FontForSizeFallback *ffsd = nullptr;
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

String TextServerFallback::_font_get_supported_chars(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, String());

	MutexLock lock(fd->mutex);
	FontForSizeFallback *ffsd = nullptr;
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

PackedInt32Array TextServerFallback::_font_get_supported_glyphs(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, PackedInt32Array());

	MutexLock lock(fd->mutex);
	FontForSizeFallback *at_size = nullptr;
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

void TextServerFallback::_font_render_range(const RID &p_font_rid, const Vector2i &p_size, int64_t p_start, int64_t p_end) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);
	ERR_FAIL_COND_MSG((p_start >= 0xd800 && p_start <= 0xdfff) || (p_start > 0x10ffff), "Unicode parsing error: Invalid unicode codepoint " + String::num_int64(p_start, 16) + ".");
	ERR_FAIL_COND_MSG((p_end >= 0xd800 && p_end <= 0xdfff) || (p_end > 0x10ffff), "Unicode parsing error: Invalid unicode codepoint " + String::num_int64(p_end, 16) + ".");

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	for (int64_t i = p_start; i <= p_end; i++) {
#ifdef MODULE_FREETYPE_ENABLED
		int32_t idx = i;
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

void TextServerFallback::_font_render_glyph(const RID &p_font_rid, const Vector2i &p_size, int64_t p_index) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_draw_glyph(const RID &p_font_rid, const RID &p_canvas, int64_t p_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color) const {
	if (p_index == 0) {
		return; // Non visual character, skip.
	}
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, p_size);
	FontForSizeFallback *ffsd = nullptr;
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

void TextServerFallback::_font_draw_glyph_outline(const RID &p_font_rid, const RID &p_canvas, int64_t p_size, int64_t p_outline_size, const Vector2 &p_pos, int64_t p_index, const Color &p_color) const {
	if (p_index == 0) {
		return; // Non visual character, skip.
	}
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size_outline(fd, Vector2i(p_size, p_outline_size));
	FontForSizeFallback *ffsd = nullptr;
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

bool TextServerFallback::_font_is_language_supported(const RID &p_font_rid, const String &p_language) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	if (fd->language_support_overrides.has(p_language)) {
		return fd->language_support_overrides[p_language];
	} else {
		return true;
	}
}

void TextServerFallback::_font_set_language_support_override(const RID &p_font_rid, const String &p_language, bool p_supported) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->language_support_overrides[p_language] = p_supported;
}

bool TextServerFallback::_font_get_language_support_override(const RID &p_font_rid, const String &p_language) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->language_support_overrides[p_language];
}

void TextServerFallback::_font_remove_language_support_override(const RID &p_font_rid, const String &p_language) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->language_support_overrides.erase(p_language);
}

PackedStringArray TextServerFallback::_font_get_language_support_overrides(const RID &p_font_rid) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, PackedStringArray());

	MutexLock lock(fd->mutex);
	PackedStringArray out;
	for (const KeyValue<String, bool> &E : fd->language_support_overrides) {
		out.push_back(E.key);
	}
	return out;
}

bool TextServerFallback::_font_is_script_supported(const RID &p_font_rid, const String &p_script) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	if (fd->script_support_overrides.has(p_script)) {
		return fd->script_support_overrides[p_script];
	} else {
		return true;
	}
}

void TextServerFallback::_font_set_script_support_override(const RID &p_font_rid, const String &p_script, bool p_supported) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	fd->script_support_overrides[p_script] = p_supported;
}

bool TextServerFallback::_font_get_script_support_override(const RID &p_font_rid, const String &p_script) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, false);

	MutexLock lock(fd->mutex);
	return fd->script_support_overrides[p_script];
}

void TextServerFallback::_font_remove_script_support_override(const RID &p_font_rid, const String &p_script) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->script_support_overrides.erase(p_script);
}

PackedStringArray TextServerFallback::_font_get_script_support_overrides(const RID &p_font_rid) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, PackedStringArray());

	MutexLock lock(fd->mutex);
	PackedStringArray out;
	for (const KeyValue<String, bool> &E : fd->script_support_overrides) {
		out.push_back(E.key);
	}
	return out;
}

void TextServerFallback::_font_set_opentype_feature_overrides(const RID &p_font_rid, const Dictionary &p_overrides) {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL(fd);

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND(!_ensure_cache_for_size(fd, size, ffsd));
	fd->feature_overrides = p_overrides;
}

Dictionary TextServerFallback::_font_get_opentype_feature_overrides(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Dictionary());

	MutexLock lock(fd->mutex);
	return fd->feature_overrides;
}

Dictionary TextServerFallback::_font_supported_feature_list(const RID &p_font_rid) const {
	return Dictionary();
}

Dictionary TextServerFallback::_font_supported_variation_list(const RID &p_font_rid) const {
	FontFallback *fd = _get_font_data(p_font_rid);
	ERR_FAIL_NULL_V(fd, Dictionary());

	MutexLock lock(fd->mutex);
	Vector2i size = _get_size(fd, 16);
	FontForSizeFallback *ffsd = nullptr;
	ERR_FAIL_COND_V(!_ensure_cache_for_size(fd, size, ffsd), Dictionary());
	return fd->supported_varaitions;
}

double TextServerFallback::_font_get_global_oversampling() const {
	return oversampling;
}

void TextServerFallback::_font_set_global_oversampling(double p_oversampling) {
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
				invalidate(shaped_owner.get_or_null(E));
			}
		}
	}
}

/*************************************************************************/
/* Shaped text buffer interface                                          */
/*************************************************************************/

void TextServerFallback::invalidate(ShapedTextDataFallback *p_shaped) {
	p_shaped->valid.clear();
	p_shaped->sort_valid = false;
	p_shaped->line_breaks_valid = false;
	p_shaped->justification_ops_valid = false;
	p_shaped->ascent = 0.0;
	p_shaped->descent = 0.0;
	p_shaped->width = 0.0;
	p_shaped->upos = 0.0;
	p_shaped->uthk = 0.0;
	p_shaped->glyphs.clear();
	p_shaped->glyphs_logical.clear();
}

void TextServerFallback::full_copy(ShapedTextDataFallback *p_shaped) {
	ShapedTextDataFallback *parent = shaped_owner.get_or_null(p_shaped->parent);

	for (const KeyValue<Variant, ShapedTextDataFallback::EmbeddedObject> &E : parent->objects) {
		if (E.value.start >= p_shaped->start && E.value.start < p_shaped->end) {
			p_shaped->objects[E.key] = E.value;
		}
	}

	for (int i = p_shaped->first_span; i <= p_shaped->last_span; i++) {
		ShapedTextDataFallback::Span span = parent->spans[i];
		span.start = MAX(p_shaped->start, span.start);
		span.end = MIN(p_shaped->end, span.end);
		p_shaped->spans.push_back(span);
	}
	p_shaped->first_span = 0;
	p_shaped->last_span = 0;

	p_shaped->parent = RID();
}

RID TextServerFallback::_create_shaped_text(TextServer::Direction p_direction, TextServer::Orientation p_orientation) {
	_THREAD_SAFE_METHOD_
	ERR_FAIL_COND_V_MSG(p_direction == DIRECTION_INHERITED, RID(), "Invalid text direction.");

	ShapedTextDataFallback *sd = memnew(ShapedTextDataFallback);
	sd->direction = p_direction;
	sd->orientation = p_orientation;

	return shaped_owner.make_rid(sd);
}

void TextServerFallback::_shaped_text_clear(const RID &p_shaped) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	sd->parent = RID();
	sd->start = 0;
	sd->end = 0;
	sd->text = String();
	sd->spans.clear();
	sd->first_span = 0;
	sd->last_span = 0;
	sd->objects.clear();
	invalidate(sd);
}

void TextServerFallback::_shaped_text_set_direction(const RID &p_shaped, TextServer::Direction p_direction) {
	ERR_FAIL_COND_MSG(p_direction == DIRECTION_INHERITED, "Invalid text direction.");
	if (p_direction == DIRECTION_RTL) {
		ERR_PRINT_ONCE("Right-to-left layout is not supported by this text server.");
	}
}

TextServer::Direction TextServerFallback::_shaped_text_get_direction(const RID &p_shaped) const {
	return TextServer::DIRECTION_LTR;
}

TextServer::Direction TextServerFallback::_shaped_text_get_inferred_direction(const RID &p_shaped) const {
	return TextServer::DIRECTION_LTR;
}

void TextServerFallback::_shaped_text_set_custom_punctuation(const RID &p_shaped, const String &p_punct) {
	_THREAD_SAFE_METHOD_
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	if (sd->custom_punct != p_punct) {
		if (sd->parent != RID()) {
			full_copy(sd);
		}
		sd->custom_punct = p_punct;
		invalidate(sd);
	}
}

String TextServerFallback::_shaped_text_get_custom_punctuation(const RID &p_shaped) const {
	_THREAD_SAFE_METHOD_
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, String());
	return sd->custom_punct;
}

void TextServerFallback::_shaped_text_set_custom_ellipsis(const RID &p_shaped, int64_t p_char) {
	_THREAD_SAFE_METHOD_
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);
	sd->el_char = p_char;
}

int64_t TextServerFallback::_shaped_text_get_custom_ellipsis(const RID &p_shaped) const {
	_THREAD_SAFE_METHOD_
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0);
	return sd->el_char;
}

void TextServerFallback::_shaped_text_set_orientation(const RID &p_shaped, TextServer::Orientation p_orientation) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	if (sd->orientation != p_orientation) {
		if (sd->parent != RID()) {
			full_copy(sd);
		}
		sd->orientation = p_orientation;
		invalidate(sd);
	}
}

void TextServerFallback::_shaped_text_set_bidi_override(const RID &p_shaped, const Array &p_override) {
	// No BiDi support, ignore.
}

TextServer::Orientation TextServerFallback::_shaped_text_get_orientation(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, TextServer::ORIENTATION_HORIZONTAL);

	MutexLock lock(sd->mutex);
	return sd->orientation;
}

void TextServerFallback::_shaped_text_set_preserve_invalid(const RID &p_shaped, bool p_enabled) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);

	MutexLock lock(sd->mutex);
	ERR_FAIL_NULL(sd);
	if (sd->preserve_invalid != p_enabled) {
		if (sd->parent != RID()) {
			full_copy(sd);
		}
		sd->preserve_invalid = p_enabled;
		invalidate(sd);
	}
}

bool TextServerFallback::_shaped_text_get_preserve_invalid(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	return sd->preserve_invalid;
}

void TextServerFallback::_shaped_text_set_preserve_control(const RID &p_shaped, bool p_enabled) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	if (sd->preserve_control != p_enabled) {
		if (sd->parent != RID()) {
			full_copy(sd);
		}
		sd->preserve_control = p_enabled;
		invalidate(sd);
	}
}

bool TextServerFallback::_shaped_text_get_preserve_control(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	return sd->preserve_control;
}

void TextServerFallback::_shaped_text_set_spacing(const RID &p_shaped, SpacingType p_spacing, int64_t p_value) {
	ERR_FAIL_INDEX((int)p_spacing, 4);
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);

	MutexLock lock(sd->mutex);
	if (sd->extra_spacing[p_spacing] != p_value) {
		if (sd->parent != RID()) {
			full_copy(sd);
		}
		sd->extra_spacing[p_spacing] = p_value;
		invalidate(sd);
	}
}

int64_t TextServerFallback::_shaped_text_get_spacing(const RID &p_shaped, SpacingType p_spacing) const {
	ERR_FAIL_INDEX_V((int)p_spacing, 4, 0);

	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0);

	MutexLock lock(sd->mutex);
	return sd->extra_spacing[p_spacing];
}

int64_t TextServerFallback::_shaped_get_span_count(const RID &p_shaped) const {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0);

	if (sd->parent != RID()) {
		return sd->last_span - sd->first_span + 1;
	} else {
		return sd->spans.size();
	}
}

Variant TextServerFallback::_shaped_get_span_meta(const RID &p_shaped, int64_t p_index) const {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Variant());
	if (sd->parent != RID()) {
		ShapedTextDataFallback *parent_sd = shaped_owner.get_or_null(sd->parent);
		ERR_FAIL_COND_V(!parent_sd->valid.is_set(), Variant());
		ERR_FAIL_INDEX_V(p_index + sd->first_span, parent_sd->spans.size(), Variant());
		return parent_sd->spans[p_index + sd->first_span].meta;
	} else {
		ERR_FAIL_INDEX_V(p_index, sd->spans.size(), Variant());
		return sd->spans[p_index].meta;
	}
}

Variant TextServerFallback::_shaped_get_span_embedded_object(const RID &p_shaped, int64_t p_index) const {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Variant());
	if (sd->parent != RID()) {
		ShapedTextDataFallback *parent_sd = shaped_owner.get_or_null(sd->parent);
		ERR_FAIL_COND_V(!parent_sd->valid.is_set(), Variant());
		ERR_FAIL_INDEX_V(p_index + sd->first_span, parent_sd->spans.size(), Variant());
		return parent_sd->spans[p_index + sd->first_span].embedded_key;
	} else {
		ERR_FAIL_INDEX_V(p_index, sd->spans.size(), Variant());
		return sd->spans[p_index].embedded_key;
	}
}

void TextServerFallback::_shaped_set_span_update_font(const RID &p_shaped, int64_t p_index, const TypedArray<RID> &p_fonts, int64_t p_size, const Dictionary &p_opentype_features) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL(sd);
	if (sd->parent != RID()) {
		full_copy(sd);
	}
	ERR_FAIL_INDEX(p_index, sd->spans.size());

	ShapedTextDataFallback::Span &span = sd->spans.ptrw()[p_index];
	span.fonts.clear();
	// Pre-sort fonts, push fonts with the language support first.
	Array fonts_no_match;
	int font_count = p_fonts.size();
	for (int i = 0; i < font_count; i++) {
		if (_font_is_language_supported(p_fonts[i], span.language)) {
			span.fonts.push_back(p_fonts[i]);
		} else {
			fonts_no_match.push_back(p_fonts[i]);
		}
	}
	span.fonts.append_array(fonts_no_match);
	span.font_size = p_size;
	span.features = p_opentype_features;

	sd->valid.clear();
}

bool TextServerFallback::_shaped_text_add_string(const RID &p_shaped, const String &p_text, const TypedArray<RID> &p_fonts, int64_t p_size, const Dictionary &p_opentype_features, const String &p_language, const Variant &p_meta) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	ERR_FAIL_COND_V(p_size <= 0, false);

	for (int i = 0; i < p_fonts.size(); i++) {
		ERR_FAIL_NULL_V(_get_font_data(p_fonts[i]), false);
	}

	if (p_text.is_empty()) {
		return true;
	}

	if (sd->parent != RID()) {
		full_copy(sd);
	}

	ShapedTextDataFallback::Span span;
	span.start = sd->text.length();
	span.end = span.start + p_text.length();

	// Pre-sort fonts, push fonts with the language support first.
	Array fonts_no_match;
	int font_count = p_fonts.size();
	if (font_count > 0) {
		span.fonts.push_back(p_fonts[0]);
	}
	for (int i = 1; i < font_count; i++) {
		if (_font_is_language_supported(p_fonts[i], p_language)) {
			span.fonts.push_back(p_fonts[i]);
		} else {
			fonts_no_match.push_back(p_fonts[i]);
		}
	}
	span.fonts.append_array(fonts_no_match);

	ERR_FAIL_COND_V(span.fonts.is_empty(), false);
	span.font_size = p_size;
	span.language = p_language;
	span.meta = p_meta;

	sd->spans.push_back(span);
	sd->text = sd->text + p_text;
	sd->end += p_text.length();
	invalidate(sd);

	return true;
}

bool TextServerFallback::_shaped_text_add_object(const RID &p_shaped, const Variant &p_key, const Size2 &p_size, InlineAlignment p_inline_align, int64_t p_length, double p_baseline) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	ERR_FAIL_COND_V(p_key == Variant(), false);
	ERR_FAIL_COND_V(sd->objects.has(p_key), false);

	if (sd->parent != RID()) {
		full_copy(sd);
	}

	ShapedTextDataFallback::Span span;
	span.start = sd->start + sd->text.length();
	span.end = span.start + p_length;
	span.embedded_key = p_key;

	ShapedTextDataFallback::EmbeddedObject obj;
	obj.inline_align = p_inline_align;
	obj.rect.size = p_size;
	obj.start = span.start;
	obj.end = span.end;
	obj.baseline = p_baseline;

	sd->spans.push_back(span);
	sd->text = sd->text + String::chr(0xfffc).repeat(p_length);
	sd->end += p_length;
	sd->objects[p_key] = obj;
	invalidate(sd);

	return true;
}

bool TextServerFallback::_shaped_text_resize_object(const RID &p_shaped, const Variant &p_key, const Size2 &p_size, InlineAlignment p_inline_align, double p_baseline) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
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

		Vector<ShapedTextDataFallback::Span> &spans = sd->spans;
		if (sd->parent != RID()) {
			ShapedTextDataFallback *parent_sd = shaped_owner.get_or_null(sd->parent);
			ERR_FAIL_COND_V(!parent_sd->valid.is_set(), false);
			spans = parent_sd->spans;
		}

		int sd_size = sd->glyphs.size();
		int span_size = spans.size();

		for (int i = 0; i < sd_size; i++) {
			Glyph gl = sd->glyphs[i];
			Variant key;
			if ((gl.flags & GRAPHEME_IS_EMBEDDED_OBJECT) == GRAPHEME_IS_EMBEDDED_OBJECT && gl.span_index + sd->first_span >= 0 && gl.span_index + sd->first_span < span_size) {
				key = spans[gl.span_index + sd->first_span].embedded_key;
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
						sd->ascent = MAX(sd->ascent, _font_get_ascent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_TOP));
						sd->descent = MAX(sd->descent, _font_get_descent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_BOTTOM));
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
		_realign(sd);
	}
	return true;
}

void TextServerFallback::_realign(ShapedTextDataFallback *p_sd) const {
	// Align embedded objects to baseline.
	double full_ascent = p_sd->ascent;
	double full_descent = p_sd->descent;
	for (KeyValue<Variant, ShapedTextDataFallback::EmbeddedObject> &E : p_sd->objects) {
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

RID TextServerFallback::_shaped_text_substr(const RID &p_shaped, int64_t p_start, int64_t p_length) const {
	_THREAD_SAFE_METHOD_

	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, RID());

	MutexLock lock(sd->mutex);
	if (sd->parent != RID()) {
		return _shaped_text_substr(sd->parent, p_start, p_length);
	}
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}
	ERR_FAIL_COND_V(p_start < 0 || p_length < 0, RID());
	ERR_FAIL_COND_V(sd->start > p_start || sd->end < p_start, RID());
	ERR_FAIL_COND_V(sd->end < p_start + p_length, RID());

	ShapedTextDataFallback *new_sd = memnew(ShapedTextDataFallback);
	new_sd->parent = p_shaped;
	new_sd->start = p_start;
	new_sd->end = p_start + p_length;

	new_sd->orientation = sd->orientation;
	new_sd->direction = sd->direction;
	new_sd->custom_punct = sd->custom_punct;
	new_sd->para_direction = sd->para_direction;
	new_sd->line_breaks_valid = sd->line_breaks_valid;
	new_sd->justification_ops_valid = sd->justification_ops_valid;
	new_sd->sort_valid = false;
	new_sd->upos = sd->upos;
	new_sd->uthk = sd->uthk;
	for (int i = 0; i < TextServer::SPACING_MAX; i++) {
		new_sd->extra_spacing[i] = sd->extra_spacing[i];
	}

	if (p_length > 0) {
		new_sd->text = sd->text.substr(p_start - sd->start, p_length);

		int span_size = sd->spans.size();

		new_sd->first_span = 0;
		new_sd->last_span = span_size - 1;
		for (int i = 0; i < span_size; i++) {
			const ShapedTextDataFallback::Span &span = sd->spans[i];
			if (span.end <= p_start) {
				new_sd->first_span = i + 1;
			} else if (span.start >= p_start + p_length) {
				new_sd->last_span = i - 1;
				break;
			}
		}

		int sd_size = sd->glyphs.size();
		const Glyph *sd_glyphs = sd->glyphs.ptr();

		for (int i = 0; i < sd_size; i++) {
			if ((sd_glyphs[i].start >= new_sd->start) && (sd_glyphs[i].end <= new_sd->end)) {
				Glyph gl = sd_glyphs[i];
				if (gl.span_index >= 0) {
					gl.span_index -= new_sd->first_span;
				}
				if (gl.end == p_start + p_length && ((gl.flags & GRAPHEME_IS_SOFT_HYPHEN) == GRAPHEME_IS_SOFT_HYPHEN)) {
					gl.index = 0x00ad;
					gl.advance = font_get_glyph_advance(gl.font_rid, gl.font_size, 0x00ad).x;
				}
				if ((gl.flags & GRAPHEME_IS_EMBEDDED_OBJECT) == GRAPHEME_IS_EMBEDDED_OBJECT && gl.span_index + new_sd->first_span >= 0 && gl.span_index + new_sd->first_span < span_size) {
					Variant key = sd->spans[gl.span_index + new_sd->first_span].embedded_key;
					if (key != Variant()) {
						ShapedTextDataFallback::EmbeddedObject obj = sd->objects[key];
						if (new_sd->orientation == ORIENTATION_HORIZONTAL) {
							obj.rect.position.x = new_sd->width;
							new_sd->width += obj.rect.size.x;
						} else {
							obj.rect.position.y = new_sd->width;
							new_sd->width += obj.rect.size.y;
						}
						new_sd->objects[key] = obj;
					}
				} else {
					if (gl.font_rid.is_valid()) {
						if (new_sd->orientation == ORIENTATION_HORIZONTAL) {
							new_sd->ascent = MAX(new_sd->ascent, _font_get_ascent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_TOP));
							new_sd->descent = MAX(new_sd->descent, _font_get_descent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_BOTTOM));
						} else {
							new_sd->ascent = MAX(new_sd->ascent, Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5));
							new_sd->descent = MAX(new_sd->descent, Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5));
						}
					} else if (new_sd->preserve_invalid || (new_sd->preserve_control && is_control(gl.index))) {
						// Glyph not found, replace with hex code box.
						if (new_sd->orientation == ORIENTATION_HORIZONTAL) {
							new_sd->ascent = MAX(new_sd->ascent, get_hex_code_box_size(gl.font_size, gl.index).y);
						} else {
							new_sd->ascent = MAX(new_sd->ascent, Math::round(get_hex_code_box_size(gl.font_size, gl.index).x * 0.5));
							new_sd->descent = MAX(new_sd->descent, Math::round(get_hex_code_box_size(gl.font_size, gl.index).x * 0.5));
						}
					}
					new_sd->width += gl.advance * gl.repeat;
				}
				new_sd->glyphs.push_back(gl);
			}
		}

		_realign(new_sd);
	}
	new_sd->valid.set();

	return shaped_owner.make_rid(new_sd);
}

RID TextServerFallback::_shaped_text_get_parent(const RID &p_shaped) const {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, RID());

	MutexLock lock(sd->mutex);
	return sd->parent;
}

double TextServerFallback::_shaped_text_fit_to_width(const RID &p_shaped, double p_width, BitField<JustificationFlag> p_jst_flags) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		_shaped_text_shape(p_shaped);
	}
	if (!sd->justification_ops_valid) {
		_shaped_text_update_justification_ops(p_shaped);
	}

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
			end_pos = sd->overrun_trim_data.trim_pos;
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
	for (int i = start_pos; i <= end_pos; i++) {
		const Glyph &gl = sd->glyphs[i];
		if (gl.count > 0) {
			if ((gl.flags & GRAPHEME_IS_SOFT_HYPHEN) != GRAPHEME_IS_SOFT_HYPHEN && (gl.flags & GRAPHEME_IS_SPACE) == GRAPHEME_IS_SPACE && (gl.flags & GRAPHEME_IS_PUNCTUATION) != GRAPHEME_IS_PUNCTUATION) {
				space_count++;
			}
		}
	}

	if ((space_count > 0) && p_jst_flags.has_flag(JUSTIFICATION_WORD_BOUND)) {
		double delta_width_per_space = (p_width - justification_width) / space_count;
		for (int i = start_pos; i <= end_pos; i++) {
			Glyph &gl = sd->glyphs.write[i];
			if (gl.count > 0) {
				if ((gl.flags & GRAPHEME_IS_SOFT_HYPHEN) != GRAPHEME_IS_SOFT_HYPHEN && (gl.flags & GRAPHEME_IS_SPACE) == GRAPHEME_IS_SPACE && (gl.flags & GRAPHEME_IS_PUNCTUATION) != GRAPHEME_IS_PUNCTUATION) {
					double old_adv = gl.advance;
					gl.advance = MAX(gl.advance + delta_width_per_space, Math::round(0.1 * gl.font_size));
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

double TextServerFallback::_shaped_text_tab_align(const RID &p_shaped, const PackedFloat32Array &p_tab_stops) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
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

bool TextServerFallback::_shaped_text_update_breaks(const RID &p_shaped) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		_shaped_text_shape(p_shaped);
	}

	if (sd->line_breaks_valid) {
		return true; // Nothing to do.
	}

	int sd_size = sd->glyphs.size();
	Glyph *sd_glyphs = sd->glyphs.ptrw();

	int c_punct_size = sd->custom_punct.length();
	const char32_t *c_punct = sd->custom_punct.ptr();

	for (int i = 0; i < sd_size; i++) {
		if (sd_glyphs[i].count > 0) {
			char32_t c = sd->text[sd_glyphs[i].start - sd->start];
			char32_t c_next = i < sd_size ? sd->text[sd_glyphs[i].start - sd->start + 1] : 0x0000;
			if (c_punct_size == 0) {
				if (is_punct(c) && c != 0x005F && c != ' ') {
					sd_glyphs[i].flags |= GRAPHEME_IS_PUNCTUATION;
				}
			} else {
				for (int j = 0; j < c_punct_size; j++) {
					if (c_punct[j] == c) {
						sd_glyphs[i].flags |= GRAPHEME_IS_PUNCTUATION;
						break;
					}
				}
			}
			if (is_underscore(c)) {
				sd->glyphs.write[i].flags |= GRAPHEME_IS_UNDERSCORE;
			}
			if (is_whitespace(c) && !is_linebreak(c)) {
				sd_glyphs[i].flags |= GRAPHEME_IS_SPACE;
				if (c != 0x00A0 && c != 0x202F && c != 0x2060 && c != 0x2007) { // Skip for non-breaking space variants.
					sd_glyphs[i].flags |= GRAPHEME_IS_BREAK_SOFT;
				}
			}
			if (is_linebreak(c)) {
				sd_glyphs[i].flags |= GRAPHEME_IS_SPACE;
				if (c != 0x000D || c_next != 0x000A) { // Skip first hard break in CR-LF pair.
					sd_glyphs[i].flags |= GRAPHEME_IS_BREAK_HARD;
				}
			}
			if (c == 0x0009 || c == 0x000b) {
				sd_glyphs[i].flags |= GRAPHEME_IS_TAB;
			}
			if (c == 0x00ad) {
				sd_glyphs[i].flags |= GRAPHEME_IS_SOFT_HYPHEN;
			}

			i += (sd_glyphs[i].count - 1);
		}
	}
	sd->line_breaks_valid = true;
	return sd->line_breaks_valid;
}

bool TextServerFallback::_shaped_text_update_justification_ops(const RID &p_shaped) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		_shaped_text_shape(p_shaped);
	}
	if (!sd->line_breaks_valid) {
		_shaped_text_update_breaks(p_shaped);
	}

	sd->justification_ops_valid = true; // Not supported by fallback server.
	return true;
}

RID TextServerFallback::_find_sys_font_for_text(const RID &p_fdef, const String &p_script_code, const String &p_language, const String &p_text) {
	RID f;
	// Try system fallback.
	if (_font_is_allow_system_fallback(p_fdef)) {
		String font_name = _font_get_name(p_fdef);
		BitField<FontStyle> font_style = _font_get_style(p_fdef);
		int font_weight = _font_get_weight(p_fdef);
		int font_stretch = _font_get_stretch(p_fdef);
		Dictionary dvar = _font_get_variation_coordinates(p_fdef);
		static int64_t wgth_tag = name_to_tag("weight");
		static int64_t wdth_tag = name_to_tag("width");
		static int64_t ital_tag = name_to_tag("italic");
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
	}
	return f;
}

void TextServerFallback::_shaped_text_overrun_trim_to_width(const RID &p_shaped_line, double p_width, BitField<TextServer::TextOverrunFlag> p_trim_flags) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped_line);
	ERR_FAIL_NULL_MSG(sd, "ShapedTextDataFallback invalid.");

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

	Vector<ShapedTextDataFallback::Span> &spans = sd->spans;
	if (sd->parent != RID()) {
		ShapedTextDataFallback *parent_sd = shaped_owner.get_or_null(sd->parent);
		ERR_FAIL_COND(!parent_sd->valid.is_set());
		spans = parent_sd->spans;
	}

	int span_size = spans.size();
	if (span_size == 0) {
		return;
	}

	int sd_size = sd->glyphs.size();
	int last_gl_font_size = sd_glyphs[sd_size - 1].font_size;
	bool found_el_char = false;

	// Find usable fonts, if fonts from the last glyph do not have required chars.
	RID dot_gl_font_rid = sd_glyphs[sd_size - 1].font_rid;
	if (add_ellipsis || enforce_ellipsis) {
		if (!_font_has_char(dot_gl_font_rid, sd->el_char)) {
			const Array &fonts = spans[span_size - 1].fonts;
			for (int i = 0; i < fonts.size(); i++) {
				if (_font_has_char(fonts[i], sd->el_char)) {
					dot_gl_font_rid = fonts[i];
					found_el_char = true;
					break;
				}
			}
			if (!found_el_char && OS::get_singleton()->has_feature("system_fonts") && fonts.size() > 0 && _font_is_allow_system_fallback(fonts[0])) {
				const char32_t u32str[] = { sd->el_char, 0 };
				RID rid = _find_sys_font_for_text(fonts[0], String(), spans[span_size - 1].language, u32str);
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
				const Array &fonts = spans[span_size - 1].fonts;
				for (int i = 0; i < fonts.size(); i++) {
					if (_font_has_char(fonts[i], '.')) {
						dot_gl_font_rid = fonts[i];
						found_dot_char = true;
						break;
					}
				}
				if (!found_dot_char && OS::get_singleton()->has_feature("system_fonts") && fonts.size() > 0 && _font_is_allow_system_fallback(fonts[0])) {
					RID rid = _find_sys_font_for_text(fonts[0], String(), spans[span_size - 1].language, ".");
					if (rid.is_valid()) {
						dot_gl_font_rid = rid;
					}
				}
			}
		}
	}
	RID whitespace_gl_font_rid = sd_glyphs[sd_size - 1].font_rid;
	if (!_font_has_char(whitespace_gl_font_rid, ' ')) {
		const Array &fonts = spans[span_size - 1].fonts;
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

	int trim_pos = 0;
	int ellipsis_pos = (enforce_ellipsis) ? 0 : -1;

	int last_valid_cut = 0;
	bool found = false;

	if (enforce_ellipsis && (width + ellipsis_width <= p_width)) {
		trim_pos = -1;
		ellipsis_pos = sd_size;
	} else {
		for (int i = sd_size - 1; i != -1; i--) {
			width -= sd_glyphs[i].advance * sd_glyphs[i].repeat;

			if (sd_glyphs[i].count > 0) {
				bool above_min_char_threshold = (i >= ell_min_characters);

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
				gl.flags = GRAPHEME_IS_SPACE | GRAPHEME_IS_BREAK_SOFT | GRAPHEME_IS_VIRTUAL;

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
				gl.flags = GRAPHEME_IS_PUNCTUATION | GRAPHEME_IS_VIRTUAL;

				sd->overrun_trim_data.ellipsis_glyph_buf.append(gl);
			}
		}

		sd->text_trimmed = true;
		sd->width_trimmed = width + ((ellipsis_pos != -1) ? ellipsis_width : 0);
	}
}

int64_t TextServerFallback::_shaped_text_get_trim_pos(const RID &p_shaped) const {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V_MSG(sd, -1, "ShapedTextDataFallback invalid.");

	MutexLock lock(sd->mutex);
	return sd->overrun_trim_data.trim_pos;
}

int64_t TextServerFallback::_shaped_text_get_ellipsis_pos(const RID &p_shaped) const {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V_MSG(sd, -1, "ShapedTextDataFallback invalid.");

	MutexLock lock(sd->mutex);
	return sd->overrun_trim_data.ellipsis_pos;
}

const Glyph *TextServerFallback::_shaped_text_get_ellipsis_glyphs(const RID &p_shaped) const {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V_MSG(sd, nullptr, "ShapedTextDataFallback invalid.");

	MutexLock lock(sd->mutex);
	return sd->overrun_trim_data.ellipsis_glyph_buf.ptr();
}

int64_t TextServerFallback::_shaped_text_get_ellipsis_glyph_count(const RID &p_shaped) const {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V_MSG(sd, 0, "ShapedTextDataFallback invalid.");

	MutexLock lock(sd->mutex);
	return sd->overrun_trim_data.ellipsis_glyph_buf.size();
}

bool TextServerFallback::_shaped_text_shape(const RID &p_shaped) {
	ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	MutexLock lock(sd->mutex);
	if (sd->valid.is_set()) {
		return true;
	}

	if (sd->parent != RID()) {
		full_copy(sd);
	}

	// Cleanup.
	sd->justification_ops_valid = false;
	sd->line_breaks_valid = false;
	sd->ascent = 0.0;
	sd->descent = 0.0;
	sd->width = 0.0;
	sd->glyphs.clear();

	if (sd->text.length() == 0) {
		sd->valid.set();
		return true;
	}

	// "Shape" string.
	for (int i = 0; i < sd->spans.size(); i++) {
		const ShapedTextDataFallback::Span &span = sd->spans[i];
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
			gl.span_index = i;
			gl.start = span.start;
			gl.end = span.end;
			gl.count = 1;
			gl.index = 0;
			gl.flags = GRAPHEME_IS_VALID | GRAPHEME_IS_EMBEDDED_OBJECT;
			if (sd->orientation == ORIENTATION_HORIZONTAL) {
				gl.advance = sd->objects[span.embedded_key].rect.size.x;
			} else {
				gl.advance = sd->objects[span.embedded_key].rect.size.y;
			}
			sd->glyphs.push_back(gl);
		} else {
			// Text span.
			RID prev_font;
			for (int j = span.start; j < span.end; j++) {
				Glyph gl;
				gl.span_index = i;
				gl.start = j;
				gl.end = j + 1;
				gl.count = 1;
				gl.font_size = span.font_size;
				gl.index = (int32_t)sd->text[j - sd->start]; // Use codepoint.
				if (gl.index == 0x0009 || gl.index == 0x000b) {
					gl.index = 0x0020;
				}
				if (!sd->preserve_control && is_control(gl.index)) {
					gl.index = 0x0020;
				}
				// Select first font which has character (font are already sorted by span language).
				for (int k = 0; k < span.fonts.size(); k++) {
					if (_font_has_char(span.fonts[k], gl.index)) {
						gl.font_rid = span.fonts[k];
						break;
					}
				}
				if (!gl.font_rid.is_valid() && prev_font.is_valid()) {
					if (_font_has_char(prev_font, gl.index)) {
						gl.font_rid = prev_font;
					}
				}
				if (!gl.font_rid.is_valid() && OS::get_singleton()->has_feature("system_fonts") && span.fonts.size() > 0) {
					// Try system fallback.
					RID fdef = span.fonts[0];
					if (_font_is_allow_system_fallback(fdef)) {
						String text = sd->text.substr(j, 1);
						gl.font_rid = _find_sys_font_for_text(fdef, String(), span.language, text);
					}
				}
				prev_font = gl.font_rid;

				if (gl.font_rid.is_valid()) {
					double scale = _font_get_scale(gl.font_rid, gl.font_size);
					bool subpos = (scale != 1.0) || (_font_get_subpixel_positioning(gl.font_rid) == SUBPIXEL_POSITIONING_ONE_HALF) || (_font_get_subpixel_positioning(gl.font_rid) == SUBPIXEL_POSITIONING_ONE_QUARTER) || (_font_get_subpixel_positioning(gl.font_rid) == SUBPIXEL_POSITIONING_AUTO && gl.font_size <= SUBPIXEL_POSITIONING_ONE_HALF_MAX_SIZE);
					if (sd->text[j - sd->start] != 0 && !is_linebreak(sd->text[j - sd->start])) {
						if (sd->orientation == ORIENTATION_HORIZONTAL) {
							gl.advance = _font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x;
							gl.x_off = 0;
							gl.y_off = _font_get_baseline_offset(gl.font_rid) * (double)(_font_get_ascent(gl.font_rid, gl.font_size) + _font_get_descent(gl.font_rid, gl.font_size));
							sd->ascent = MAX(sd->ascent, _font_get_ascent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_TOP));
							sd->descent = MAX(sd->descent, _font_get_descent(gl.font_rid, gl.font_size) + _font_get_spacing(gl.font_rid, SPACING_BOTTOM));
						} else {
							gl.advance = _font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).y;
							gl.x_off = -Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5) + _font_get_baseline_offset(gl.font_rid) * (double)(_font_get_ascent(gl.font_rid, gl.font_size) + _font_get_descent(gl.font_rid, gl.font_size));
							gl.y_off = _font_get_ascent(gl.font_rid, gl.font_size);
							sd->ascent = MAX(sd->ascent, Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5));
							sd->descent = MAX(sd->descent, Math::round(_font_get_glyph_advance(gl.font_rid, gl.font_size, gl.index).x * 0.5));
						}
					}
					if (j < sd->end - 1) {
						// Do not add extra spacing to the last glyph of the string.
						if (is_whitespace(sd->text[j - sd->start])) {
							gl.advance += sd->extra_spacing[SPACING_SPACE] + _font_get_spacing(gl.font_rid, SPACING_SPACE);
						} else {
							gl.advance += sd->extra_spacing[SPACING_GLYPH] + _font_get_spacing(gl.font_rid, SPACING_GLYPH);
						}
					}
					sd->upos = MAX(sd->upos, _font_get_underline_position(gl.font_rid, gl.font_size));
					sd->uthk = MAX(sd->uthk, _font_get_underline_thickness(gl.font_rid, gl.font_size));

					// Add kerning to previous glyph.
					if (sd->glyphs.size() > 0) {
						Glyph &prev_gl = sd->glyphs.write[sd->glyphs.size() - 1];
						if (prev_gl.font_rid == gl.font_rid && prev_gl.font_size == gl.font_size) {
							if (sd->orientation == ORIENTATION_HORIZONTAL) {
								prev_gl.advance += _font_get_kerning(gl.font_rid, gl.font_size, Vector2i(prev_gl.index, gl.index)).x;
							} else {
								prev_gl.advance += _font_get_kerning(gl.font_rid, gl.font_size, Vector2i(prev_gl.index, gl.index)).y;
							}
						}
					}
					if (sd->orientation == ORIENTATION_HORIZONTAL && !subpos) {
						gl.advance = Math::round(gl.advance);
					}
				} else if (sd->preserve_invalid || (sd->preserve_control && is_control(gl.index))) {
					// Glyph not found, replace with hex code box.
					if (sd->orientation == ORIENTATION_HORIZONTAL) {
						gl.advance = get_hex_code_box_size(gl.font_size, gl.index).x;
						sd->ascent = MAX(sd->ascent, get_hex_code_box_size(gl.font_size, gl.index).y);
					} else {
						gl.advance = get_hex_code_box_size(gl.font_size, gl.index).y;
						sd->ascent = MAX(sd->ascent, Math::round(get_hex_code_box_size(gl.font_size, gl.index).x * 0.5));
						sd->descent = MAX(sd->descent, Math::round(get_hex_code_box_size(gl.font_size, gl.index).x * 0.5));
					}
				}
				sd->width += gl.advance;
				sd->glyphs.push_back(gl);
			}
		}
	}

	// Align embedded objects to baseline.
	_realign(sd);

	sd->valid.set();
	return sd->valid.is_set();
}

bool TextServerFallback::_shaped_text_is_ready(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, false);

	return sd->valid.is_set();
}

const Glyph *TextServerFallback::_shaped_text_get_glyphs(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, nullptr);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}
	return sd->glyphs.ptr();
}

int64_t TextServerFallback::_shaped_text_get_glyph_count(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}
	return sd->glyphs.size();
}

const Glyph *TextServerFallback::_shaped_text_sort_logical(const RID &p_shaped) {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, nullptr);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		_shaped_text_shape(p_shaped);
	}

	return sd->glyphs.ptr(); // Already in the logical order, return as is.
}

Vector2i TextServerFallback::_shaped_text_get_range(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Vector2i());

	MutexLock lock(sd->mutex);
	return Vector2(sd->start, sd->end);
}

Array TextServerFallback::_shaped_text_get_objects(const RID &p_shaped) const {
	Array ret;
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, ret);

	MutexLock lock(sd->mutex);
	for (const KeyValue<Variant, ShapedTextDataFallback::EmbeddedObject> &E : sd->objects) {
		ret.push_back(E.key);
	}

	return ret;
}

Rect2 TextServerFallback::_shaped_text_get_object_rect(const RID &p_shaped, const Variant &p_key) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Rect2());

	MutexLock lock(sd->mutex);
	ERR_FAIL_COND_V(!sd->objects.has(p_key), Rect2());
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}
	return sd->objects[p_key].rect;
}

Vector2i TextServerFallback::_shaped_text_get_object_range(const RID &p_shaped, const Variant &p_key) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Vector2i());

	MutexLock lock(sd->mutex);
	ERR_FAIL_COND_V(!sd->objects.has(p_key), Vector2i());
	return Vector2i(sd->objects[p_key].start, sd->objects[p_key].end);
}

int64_t TextServerFallback::_shaped_text_get_object_glyph(const RID &p_shaped, const Variant &p_key) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, -1);

	MutexLock lock(sd->mutex);
	ERR_FAIL_COND_V(!sd->objects.has(p_key), -1);
	const ShapedTextDataFallback::EmbeddedObject &obj = sd->objects[p_key];
	int sd_size = sd->glyphs.size();
	const Glyph *sd_glyphs = sd->glyphs.ptr();
	for (int i = 0; i < sd_size; i++) {
		if (obj.start == sd_glyphs[i].start) {
			return i;
		}
	}
	return -1;
}

Size2 TextServerFallback::_shaped_text_get_size(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, Size2());

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}
	if (sd->orientation == TextServer::ORIENTATION_HORIZONTAL) {
		return Size2(sd->width, sd->ascent + sd->descent + sd->extra_spacing[SPACING_TOP] + sd->extra_spacing[SPACING_BOTTOM]).ceil();
	} else {
		return Size2(sd->ascent + sd->descent + sd->extra_spacing[SPACING_TOP] + sd->extra_spacing[SPACING_BOTTOM], sd->width).ceil();
	}
}

double TextServerFallback::_shaped_text_get_ascent(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}
	return sd->ascent + sd->extra_spacing[SPACING_TOP];
}

double TextServerFallback::_shaped_text_get_descent(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}
	return sd->descent + sd->extra_spacing[SPACING_BOTTOM];
}

double TextServerFallback::_shaped_text_get_width(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}
	return Math::ceil(sd->width);
}

double TextServerFallback::_shaped_text_get_underline_position(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}

	return sd->upos;
}

double TextServerFallback::_shaped_text_get_underline_thickness(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, 0.0);

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}

	return sd->uthk;
}

PackedInt32Array TextServerFallback::_shaped_text_get_character_breaks(const RID &p_shaped) const {
	const ShapedTextDataFallback *sd = shaped_owner.get_or_null(p_shaped);
	ERR_FAIL_NULL_V(sd, PackedInt32Array());

	MutexLock lock(sd->mutex);
	if (!sd->valid.is_set()) {
		const_cast<TextServerFallback *>(this)->_shaped_text_shape(p_shaped);
	}

	PackedInt32Array ret;
	int size = sd->end - sd->start;
	if (size > 0) {
		ret.resize(size);
		for (int i = 0; i < size; i++) {
#ifdef GDEXTENSION
			ret[i] = i + 1 + sd->start;
#else
			ret.write[i] = i + 1 + sd->start;
#endif
		}
	}
	return ret;
}

String TextServerFallback::_string_to_upper(const String &p_string, const String &p_language) const {
	return p_string.to_upper();
}

String TextServerFallback::_string_to_lower(const String &p_string, const String &p_language) const {
	return p_string.to_lower();
}

String TextServerFallback::_string_to_title(const String &p_string, const String &p_language) const {
	return p_string.capitalize();
}

PackedInt32Array TextServerFallback::_string_get_word_breaks(const String &p_string, const String &p_language, int64_t p_chars_per_line) const {
	PackedInt32Array ret;

	if (p_chars_per_line > 0) {
		int line_start = 0;
		int last_break = -1;
		int line_length = 0;

		for (int i = 0; i < p_string.length(); i++) {
			const char32_t c = p_string[i];

			bool is_lb = is_linebreak(c);
			bool is_ws = is_whitespace(c);
			bool is_p = (is_punct(c) && c != 0x005F) || is_underscore(c) || c == '\t' || c == 0xfffc;

			if (is_lb) {
				if (line_length > 0) {
					ret.push_back(line_start);
					ret.push_back(i);
				}
				line_start = i;
				line_length = 0;
				last_break = -1;
				continue;
			} else if (is_ws || is_p) {
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
			bool is_p = (is_punct(c) && c != 0x005F) || is_underscore(c) || c == '\t' || c == 0xfffc;

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
			} else if (is_ws || is_p) {
				if (word_start != -1 && word_length > 0) {
					ret.push_back(word_start);
					ret.push_back(i);
				}
				word_start = -1;
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

void TextServerFallback::_update_settings() {
	lcd_subpixel_layout.set((TextServer::FontLCDSubpixelLayout)(int)GLOBAL_GET("gui/theme/lcd_subpixel_layout"));
}

TextServerFallback::TextServerFallback() {
	_insert_feature_sets();
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &TextServerFallback::_update_settings));
}

void TextServerFallback::_cleanup() {
	for (const KeyValue<SystemFontKey, SystemFontCache> &E : system_fonts) {
		const Vector<SystemFontCacheRec> &sysf_cache = E.value.var;
		for (const SystemFontCacheRec &F : sysf_cache) {
			_free_rid(F.rid);
		}
	}
	system_fonts.clear();
	system_font_data.clear();
}

TextServerFallback::~TextServerFallback() {
#ifdef MODULE_FREETYPE_ENABLED
	if (ft_library != nullptr) {
		FT_Done_FreeType(ft_library);
	}
#endif
}
