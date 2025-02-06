/**************************************************************************/
/*  thorvg_svg_in_ot.cpp                                                  */
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

#include "thorvg_svg_in_ot.h"

#ifdef GDEXTENSION
// Headers for building as GDExtension plug-in.

#include <godot_cpp/classes/xml_parser.hpp>
#include <godot_cpp/core/mutex_lock.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/templates/vector.hpp>

using namespace godot;

#elif defined(GODOT_MODULE)
// Headers for building as built-in module.

#include "core/error/error_macros.h"
#include "core/io/xml_parser.h"
#include "core/os/memory.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"

#include "modules/modules_enabled.gen.h" // For svg, freetype.
#endif

#ifdef MODULE_SVG_ENABLED
#ifdef MODULE_FREETYPE_ENABLED

#include <freetype/otsvg.h>
#include <ft2build.h>

#include <math.h>
#include <stdlib.h>

FT_Error tvg_svg_in_ot_init(FT_Pointer *p_state) {
	*p_state = memnew(TVG_State);

	return FT_Err_Ok;
}

void tvg_svg_in_ot_free(FT_Pointer *p_state) {
	TVG_State *state = *reinterpret_cast<TVG_State **>(p_state);
	memdelete(state);
}

static void construct_xml(Ref<XMLParser> &parser, double &r_embox_x, double &r_embox_y, String *p_xml, int64_t &r_tag_count) {
	if (parser->get_node_type() == XMLParser::NODE_ELEMENT) {
		*p_xml += vformat("<%s", parser->get_node_name());
		bool is_svg_tag = parser->get_node_name() == "svg";
		for (int i = 0; i < parser->get_attribute_count(); i++) {
			String aname = parser->get_attribute_name(i);
			String value = parser->get_attribute_value(i);
			if (is_svg_tag && aname == "viewBox") {
				PackedStringArray vb = value.split(" ");
				if (vb.size() == 4) {
					r_embox_x = vb[2].to_float();
					r_embox_y = vb[3].to_float();
				}
			} else if (is_svg_tag && aname == "width") {
				r_embox_x = value.to_float();
			} else if (is_svg_tag && aname == "height") {
				r_embox_y = value.to_float();
			} else {
				*p_xml += vformat(" %s=\"%s\"", aname, value);
			}
		}

		if (parser->is_empty()) {
			*p_xml += "/>";
		} else {
			*p_xml += ">";
			if (r_tag_count >= 0) {
				r_tag_count++;
			}
		}
	} else if (parser->get_node_type() == XMLParser::NODE_TEXT) {
		*p_xml += parser->get_node_data();
	} else if (parser->get_node_type() == XMLParser::NODE_ELEMENT_END) {
		*p_xml += vformat("</%s>", parser->get_node_name());
		if (r_tag_count > 0) {
			r_tag_count--;
		}
	}
}

FT_Error tvg_svg_in_ot_preset_slot(FT_GlyphSlot p_slot, FT_Bool p_cache, FT_Pointer *p_state) {
	TVG_State *state = *reinterpret_cast<TVG_State **>(p_state);
	if (!state) {
		ERR_FAIL_V_MSG(FT_Err_Invalid_SVG_Document, "SVG in OT state not initialized.");
	}
	MutexLock lock(state->mutex);

	FT_SVG_Document document = (FT_SVG_Document)p_slot->other;
	FT_Size_Metrics metrics = document->metrics;

	GL_State &gl_state = state->glyph_map[p_slot->glyph_index];
	if (!gl_state.ready) {
		Ref<XMLParser> parser;
		parser.instantiate();
		parser->_open_buffer((const uint8_t *)document->svg_document, document->svg_document_length);

		String xml_body;

		double embox_x = document->units_per_EM;
		double embox_y = document->units_per_EM;

		TVG_DocumentCache &cache = state->document_map[document->svg_document];

		if (!cache.xml_body.is_empty()) {
			// If we have a cached document, that means we have already parsed it.
			// All node cache should be available.

			xml_body = cache.xml_body;
			embox_x = cache.embox_x;
			embox_y = cache.embox_y;

			ERR_FAIL_COND_V(!cache.node_caches.has(p_slot->glyph_index), FT_Err_Invalid_SVG_Document);
			Vector<TVG_NodeCache> &ncs = cache.node_caches[p_slot->glyph_index];

			uint64_t offset = 0;
			for (TVG_NodeCache &nc : ncs) {
				// Seek will call read() internally.
				if (parser->seek(nc.document_offset) == OK) {
					int64_t tag_count = 0;
					String xml_node;

					// We only parse the glyph node.
					do {
						construct_xml(parser, embox_x, embox_y, &xml_node, tag_count);
					} while (tag_count != 0 && parser->read() == OK);

					xml_body = xml_body.insert(nc.body_offset + offset, xml_node);
					offset += xml_node.length();
				}
			}
		} else {
			String xml_node;
			String xml_body_temp;

			String *p_xml = &xml_body_temp;
			int64_t tag_count = -1;

			while (parser->read() == OK) {
				if (parser->has_attribute("id")) {
					const String &gl_name = parser->get_named_attribute_value("id");
					if (gl_name.begins_with("glyph")) {
#ifdef GDEXTENSION
						int dot_pos = gl_name.find(".");
#else
						int dot_pos = gl_name.find_char('.');
#endif // GDEXTENSION
						int64_t gl_idx = gl_name.substr(5, (dot_pos > 0) ? dot_pos - 5 : -1).to_int();

						TVG_NodeCache node_cache = TVG_NodeCache();
						node_cache.document_offset = parser->get_node_offset(),
						node_cache.body_offset = (uint64_t)cache.xml_body.length();
						cache.node_caches[gl_idx].push_back(node_cache);

						if (p_slot->glyph_index != gl_idx) {
							parser->skip_section();
							continue;
						}
						tag_count = 0;
						xml_node = "";
						p_xml = &xml_node;
					}
				}

				xml_body_temp = "";
				construct_xml(parser, embox_x, embox_y, p_xml, tag_count);

				if (xml_body_temp.length() > 0) {
					xml_body += xml_body_temp;
					cache.xml_body += xml_body_temp;
					continue;
				}

				if (tag_count == 0) {
					p_xml = &xml_body_temp;
					tag_count = -1;
					xml_body += xml_node;
				}
			}

			cache.embox_x = embox_x;
			cache.embox_y = embox_y;
		}

		std::unique_ptr<tvg::Picture> picture = tvg::Picture::gen();
		gl_state.xml_code = xml_body.utf8();

		tvg::Result result = picture->load(gl_state.xml_code.get_data(), gl_state.xml_code.length(), "svg+xml", false);
		if (result != tvg::Result::Success) {
			ERR_FAIL_V_MSG(FT_Err_Invalid_SVG_Document, "Failed to load SVG document (glyph metrics).");
		}

		float svg_width, svg_height;
		picture->size(&svg_width, &svg_height);
		double aspect = svg_width / svg_height;

		result = picture->size(embox_x * aspect, embox_y);
		if (result != tvg::Result::Success) {
			ERR_FAIL_V_MSG(FT_Err_Invalid_SVG_Document, "Failed to resize SVG document.");
		}

		double x_svg_to_out = (double)metrics.x_ppem / embox_x;
		double y_svg_to_out = (double)metrics.y_ppem / embox_y;

		gl_state.m.e11 = (double)document->transform.xx / (1 << 16);
		gl_state.m.e12 = -(double)document->transform.xy / (1 << 16);
		gl_state.m.e21 = -(double)document->transform.yx / (1 << 16);
		gl_state.m.e22 = (double)document->transform.yy / (1 << 16);
		gl_state.m.e13 = (double)document->delta.x / 64 * embox_x / metrics.x_ppem;
		gl_state.m.e23 = -(double)document->delta.y / 64 * embox_y / metrics.y_ppem;
		gl_state.m.e31 = 0;
		gl_state.m.e32 = 0;
		gl_state.m.e33 = 1;

		result = picture->size(embox_x * aspect * x_svg_to_out, embox_y * y_svg_to_out);
		if (result != tvg::Result::Success) {
			ERR_FAIL_V_MSG(FT_Err_Invalid_SVG_Document, "Failed to resize SVG document.");
		}

		result = picture->transform(gl_state.m);
		if (result != tvg::Result::Success) {
			ERR_FAIL_V_MSG(FT_Err_Invalid_SVG_Document, "Failed to apply transform to SVG document.");
		}

		picture->size(&gl_state.w, &gl_state.h);
		gl_state.x = (gl_state.h - gl_state.w) / 2.0;
		gl_state.y = -gl_state.h;

		gl_state.ready = true;
	}

	p_slot->bitmap_left = (FT_Int)gl_state.x;
	p_slot->bitmap_top = (FT_Int)-gl_state.y;

	double tmpd = Math::ceil(gl_state.h);
	p_slot->bitmap.rows = (unsigned int)tmpd;
	tmpd = Math::ceil(gl_state.w);
	p_slot->bitmap.width = (unsigned int)tmpd;
	p_slot->bitmap.pitch = (int)p_slot->bitmap.width * 4;

	p_slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;

	float metrics_width = (float)gl_state.w;
	float metrics_height = (float)gl_state.h;

	float horiBearingX = (float)gl_state.x;
	float horiBearingY = (float)-gl_state.y;

	float vertBearingX = p_slot->metrics.horiBearingX / 64.0f - p_slot->metrics.horiAdvance / 64.0f / 2;
	float vertBearingY = (p_slot->metrics.vertAdvance / 64.0f - p_slot->metrics.height / 64.0f) / 2;

	float tmpf = Math::round(metrics_width * 64);
	p_slot->metrics.width = (FT_Pos)tmpf;
	tmpf = Math::round(metrics_height * 64);
	p_slot->metrics.height = (FT_Pos)tmpf;

	p_slot->metrics.horiBearingX = (FT_Pos)(horiBearingX * 64);
	p_slot->metrics.horiBearingY = (FT_Pos)(horiBearingY * 64);
	p_slot->metrics.vertBearingX = (FT_Pos)(vertBearingX * 64);
	p_slot->metrics.vertBearingY = (FT_Pos)(vertBearingY * 64);

	if (p_slot->metrics.vertAdvance == 0) {
		p_slot->metrics.vertAdvance = (FT_Pos)(metrics_height * 1.2f * 64);
	}

	return FT_Err_Ok;
}

FT_Error tvg_svg_in_ot_render(FT_GlyphSlot p_slot, FT_Pointer *p_state) {
	TVG_State *state = *reinterpret_cast<TVG_State **>(p_state);
	if (!state) {
		ERR_FAIL_V_MSG(FT_Err_Invalid_SVG_Document, "SVG in OT state not initialized.");
	}
	MutexLock lock(state->mutex);

	if (!state->glyph_map.has(p_slot->glyph_index)) {
		ERR_FAIL_V_MSG(FT_Err_Invalid_SVG_Document, "SVG glyph not loaded.");
	}

	GL_State &gl_state = state->glyph_map[p_slot->glyph_index];
	ERR_FAIL_COND_V_MSG(!gl_state.ready, FT_Err_Invalid_SVG_Document, "SVG glyph not ready.");

	std::unique_ptr<tvg::Picture> picture = tvg::Picture::gen();
	tvg::Result res = picture->load(gl_state.xml_code.get_data(), gl_state.xml_code.length(), "svg+xml", false);
	if (res != tvg::Result::Success) {
		ERR_FAIL_V_MSG(FT_Err_Invalid_SVG_Document, "Failed to load SVG document (glyph rendering).");
	}
	res = picture->size(gl_state.w, gl_state.h);
	if (res != tvg::Result::Success) {
		ERR_FAIL_V_MSG(FT_Err_Invalid_SVG_Document, "Failed to resize SVG document.");
	}
	res = picture->transform(gl_state.m);
	if (res != tvg::Result::Success) {
		ERR_FAIL_V_MSG(FT_Err_Invalid_SVG_Document, "Failed to apply transform to SVG document.");
	}

	std::unique_ptr<tvg::SwCanvas> sw_canvas = tvg::SwCanvas::gen();
	res = sw_canvas->target((uint32_t *)p_slot->bitmap.buffer, (int)p_slot->bitmap.width, (int)p_slot->bitmap.width, (int)p_slot->bitmap.rows, tvg::SwCanvas::ARGB8888S);
	if (res != tvg::Result::Success) {
		ERR_FAIL_V_MSG(FT_Err_Invalid_Outline, "Failed to create SVG canvas.");
	}
	res = sw_canvas->push(std::move(picture));
	if (res != tvg::Result::Success) {
		ERR_FAIL_V_MSG(FT_Err_Invalid_Outline, "Failed to set SVG canvas source.");
	}
	res = sw_canvas->draw();
	if (res != tvg::Result::Success) {
		ERR_FAIL_V_MSG(FT_Err_Invalid_Outline, "Failed to draw to SVG canvas.");
	}
	res = sw_canvas->sync();
	if (res != tvg::Result::Success) {
		ERR_FAIL_V_MSG(FT_Err_Invalid_Outline, "Failed to sync SVG canvas.");
	}

	state->glyph_map.erase(p_slot->glyph_index);

	p_slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
	p_slot->bitmap.num_grays = 256;
	p_slot->format = FT_GLYPH_FORMAT_BITMAP;

	return FT_Err_Ok;
}

SVG_RendererHooks tvg_svg_in_ot_hooks = {
	(SVG_Lib_Init_Func)tvg_svg_in_ot_init,
	(SVG_Lib_Free_Func)tvg_svg_in_ot_free,
	(SVG_Lib_Render_Func)tvg_svg_in_ot_render,
	(SVG_Lib_Preset_Slot_Func)tvg_svg_in_ot_preset_slot,
};

SVG_RendererHooks *get_tvg_svg_in_ot_hooks() {
	return &tvg_svg_in_ot_hooks;
}

#endif // MODULE_FREETYPE_ENABLED
#endif // MODULE_SVG_ENABLED
