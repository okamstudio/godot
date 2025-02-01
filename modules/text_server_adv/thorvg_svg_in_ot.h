/**************************************************************************/
/*  thorvg_svg_in_ot.h                                                    */
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

#ifdef GDEXTENSION
// Headers for building as GDExtension plug-in.

#include <godot_cpp/core/mutex_lock.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/templates/hash_map.hpp>

using namespace godot;

#elif defined(GODOT_MODULE)
// Headers for building as built-in module.

#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/typedefs.h"

#include "modules/modules_enabled.gen.h" // For svg, freetype.
#endif

#ifdef MODULE_SVG_ENABLED
#ifdef MODULE_FREETYPE_ENABLED

#include <freetype/freetype.h>
#include <freetype/otsvg.h>
#include <ft2build.h>
#include <thorvg.h>

struct GL_State {
	bool ready = false;
	float x = 0;
	float y = 0;
	float w = 0;
	float h = 0;
	CharString xml_code;
	tvg::Matrix m;
};

struct TVG_NodeCache {
	uint64_t document_offset;
	uint64_t body_offset;
};

struct TVG_DocumentCache {
	String xml_body;
	double embox_x;
	double embox_y;
	HashMap<int64_t, Vector<TVG_NodeCache>> node_caches;
};

struct TVG_State {
	Mutex mutex;
	HashMap<uint32_t, GL_State> glyph_map;
	HashMap<FT_Byte *, TVG_DocumentCache> document_map;
};

FT_Error tvg_svg_in_ot_init(FT_Pointer *p_state);
void tvg_svg_in_ot_free(FT_Pointer *p_state);
FT_Error tvg_svg_in_ot_preset_slot(FT_GlyphSlot p_slot, FT_Bool p_cache, FT_Pointer *p_state);
FT_Error tvg_svg_in_ot_render(FT_GlyphSlot p_slot, FT_Pointer *p_state);

SVG_RendererHooks *get_tvg_svg_in_ot_hooks();

#endif // MODULE_FREETYPE_ENABLED
#endif // MODULE_SVG_ENABLED
