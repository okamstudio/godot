/**************************************************************************/
/*  test_text_server.h                                                    */
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

#ifndef TEST_TEXT_SERVER_H
#define TEST_TEXT_SERVER_H

#ifdef TOOLS_ENABLED

#include "editor/themes/builtin_fonts.gen.h"
#include "servers/text_server.h"
#include "tests/test_macros.h"

namespace TestTextServer {

TEST_SUITE("[TextServer]") {
	TEST_CASE("[TextServer] Init, font loading and shaping") {
		SUBCASE("[TextServer] Loading fonts") {
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(i);
				CHECK_FALSE_MESSAGE(ts.is_null(), "Invalid TS interface.");

				if (!ts->has_feature(TextServer::FEATURE_FONT_DYNAMIC)) {
					continue;
				}

				RID font = ts->create_font();
				ts->font_set_data_ptr(font, _font_NotoSans_Regular, _font_NotoSans_Regular_size);
				CHECK_FALSE_MESSAGE(font == RID(), "Loading font failed.");
				ts->free_rid(font);
			}
		}

		SUBCASE("[TextServer] Text layout: Font fallback") {
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(i);
				CHECK_FALSE_MESSAGE(ts.is_null(), "Invalid TS interface.");

				if (!ts->has_feature(TextServer::FEATURE_FONT_DYNAMIC) || !ts->has_feature(TextServer::FEATURE_SIMPLE_LAYOUT)) {
					continue;
				}

				RID font1 = ts->create_font();
				ts->font_set_data_ptr(font1, _font_NotoSans_Regular, _font_NotoSans_Regular_size);
				ts->font_set_allow_system_fallback(font1, false);
				RID font2 = ts->create_font();
				ts->font_set_data_ptr(font2, _font_NotoSansThai_Regular, _font_NotoSansThai_Regular_size);
				ts->font_set_allow_system_fallback(font2, false);

				Array font;
				font.push_back(font1);
				font.push_back(font2);

				String test = U"คนอ้วน khon uan ראה";
				//                 6^       17^

				RID ctx = ts->create_shaped_text();
				CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
				bool ok = ts->shaped_text_add_string(ctx, test, font, 16);
				CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");

				const Glyph *glyphs = ts->shaped_text_get_glyphs(ctx);
				int gl_size = ts->shaped_text_get_glyph_count(ctx);
				CHECK_FALSE_MESSAGE(gl_size == 0, "Shaping failed");
				for (int j = 0; j < gl_size; j++) {
					if (glyphs[j].start < 6) {
						CHECK_FALSE_MESSAGE(glyphs[j].font_rid != font[1], "Incorrect font selected.");
					}
					if ((glyphs[j].start > 6) && (glyphs[j].start < 16)) {
						CHECK_FALSE_MESSAGE(glyphs[j].font_rid != font[0], "Incorrect font selected.");
					}
					if (glyphs[j].start > 16) {
						CHECK_FALSE_MESSAGE(glyphs[j].font_rid != RID(), "Incorrect font selected.");
						CHECK_FALSE_MESSAGE(glyphs[j].index != test[glyphs[j].start], "Incorrect glyph index.");
					}
					CHECK_FALSE_MESSAGE((glyphs[j].start < 0 || glyphs[j].end > test.length()), "Incorrect glyph range.");
					CHECK_FALSE_MESSAGE(glyphs[j].font_size != 16, "Incorrect glyph font size.");
				}

				ts->free_rid(ctx);

				for (int j = 0; j < font.size(); j++) {
					ts->free_rid(font[j]);
				}
				font.clear();
			}
		}

		SUBCASE("[TextServer] Text layout: BiDi") {
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(i);
				CHECK_FALSE_MESSAGE(ts.is_null(), "Invalid TS interface.");

				if (!ts->has_feature(TextServer::FEATURE_FONT_DYNAMIC) || !ts->has_feature(TextServer::FEATURE_BIDI_LAYOUT)) {
					continue;
				}

				RID font1 = ts->create_font();
				ts->font_set_data_ptr(font1, _font_NotoSans_Regular, _font_NotoSans_Regular_size);
				RID font2 = ts->create_font();
				ts->font_set_data_ptr(font2, _font_Vazirmatn_Regular, _font_Vazirmatn_Regular_size);

				Array font;
				font.push_back(font1);
				font.push_back(font2);

				String test = U"Arabic (اَلْعَرَبِيَّةُ, al-ʿarabiyyah)";
				//                    7^      26^

				RID ctx = ts->create_shaped_text();
				CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
				bool ok = ts->shaped_text_add_string(ctx, test, font, 16);
				CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");

				const Glyph *glyphs = ts->shaped_text_get_glyphs(ctx);
				int gl_size = ts->shaped_text_get_glyph_count(ctx);
				CHECK_FALSE_MESSAGE(gl_size == 0, "Shaping failed");
				for (int j = 0; j < gl_size; j++) {
					if (glyphs[j].count > 0) {
						if (glyphs[j].start < 7) {
							CHECK_FALSE_MESSAGE(((glyphs[j].flags & TextServer::GRAPHEME_IS_RTL) == TextServer::GRAPHEME_IS_RTL), "Incorrect direction.");
						}
						if ((glyphs[j].start > 8) && (glyphs[j].start < 23)) {
							CHECK_FALSE_MESSAGE(((glyphs[j].flags & TextServer::GRAPHEME_IS_RTL) != TextServer::GRAPHEME_IS_RTL), "Incorrect direction.");
						}
						if (glyphs[j].start > 26) {
							CHECK_FALSE_MESSAGE(((glyphs[j].flags & TextServer::GRAPHEME_IS_RTL) == TextServer::GRAPHEME_IS_RTL), "Incorrect direction.");
						}
					}
				}

				ts->free_rid(ctx);

				for (int j = 0; j < font.size(); j++) {
					ts->free_rid(font[j]);
				}
				font.clear();
			}
		}

		SUBCASE("[TextServer] Text layout: Line break and align points") {
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(i);
				CHECK_FALSE_MESSAGE(ts.is_null(), "Invalid TS interface.");

				if (!ts->has_feature(TextServer::FEATURE_FONT_DYNAMIC) || !ts->has_feature(TextServer::FEATURE_SIMPLE_LAYOUT)) {
					continue;
				}

				RID font1 = ts->create_font();
				ts->font_set_data_ptr(font1, _font_NotoSans_Regular, _font_NotoSans_Regular_size);
				ts->font_set_allow_system_fallback(font1, false);
				RID font2 = ts->create_font();
				ts->font_set_data_ptr(font2, _font_NotoSansThai_Regular, _font_NotoSansThai_Regular_size);
				ts->font_set_allow_system_fallback(font2, false);
				RID font3 = ts->create_font();
				ts->font_set_data_ptr(font3, _font_Vazirmatn_Regular, _font_Vazirmatn_Regular_size);
				ts->font_set_allow_system_fallback(font3, false);

				Array font;
				font.push_back(font1);
				font.push_back(font2);
				font.push_back(font3);

				{
					RID ctx = ts->create_shaped_text();
					CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
					ts->shaped_text_add_string(ctx, U"Xtest", font, 10);
					ts->shaped_text_add_string(ctx, U"xs", font, 10);
					RID sctx = ts->shaped_text_substr(ctx, 1, 5);
					CHECK_FALSE_MESSAGE(sctx == RID(), "Creating substring text buffer failed.");
					PackedInt32Array sbrk = ts->shaped_text_get_character_breaks(sctx);
					CHECK_FALSE_MESSAGE(sbrk.size() != 5, "Invalid substring char breaks number.");
					if (sbrk.size() == 5) {
						CHECK_FALSE_MESSAGE(sbrk[0] != 2, "Invalid substring char break position.");
						CHECK_FALSE_MESSAGE(sbrk[1] != 3, "Invalid substring char break position.");
						CHECK_FALSE_MESSAGE(sbrk[2] != 4, "Invalid substring char break position.");
						CHECK_FALSE_MESSAGE(sbrk[3] != 5, "Invalid substring char break position.");
						CHECK_FALSE_MESSAGE(sbrk[4] != 6, "Invalid substring char break position.");
					}
					PackedInt32Array fbrk = ts->shaped_text_get_character_breaks(ctx);
					CHECK_FALSE_MESSAGE(fbrk.size() != 7, "Invalid char breaks number.");
					if (fbrk.size() == 7) {
						CHECK_FALSE_MESSAGE(fbrk[0] != 1, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(fbrk[1] != 2, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(fbrk[2] != 3, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(fbrk[3] != 4, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(fbrk[4] != 5, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(fbrk[5] != 6, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(fbrk[6] != 7, "Invalid char break position.");
					}
					PackedInt32Array rbrk = ts->string_get_character_breaks(U"Xtestxs");
					CHECK_FALSE_MESSAGE(rbrk.size() != 7, "Invalid char breaks number.");
					if (rbrk.size() == 7) {
						CHECK_FALSE_MESSAGE(rbrk[0] != 1, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(rbrk[1] != 2, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(rbrk[2] != 3, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(rbrk[3] != 4, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(rbrk[4] != 5, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(rbrk[5] != 6, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(rbrk[6] != 7, "Invalid char break position.");
					}

					ts->free_rid(sctx);
					ts->free_rid(ctx);
				}

				if (ts->has_feature(TextServer::FEATURE_BREAK_ITERATORS)) {
					RID ctx = ts->create_shaped_text();
					CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
					ts->shaped_text_add_string(ctx, U"X❤️‍🔥", font, 10);
					ts->shaped_text_add_string(ctx, U"xs", font, 10);
					RID sctx = ts->shaped_text_substr(ctx, 1, 5);
					CHECK_FALSE_MESSAGE(sctx == RID(), "Creating substring text buffer failed.");
					PackedInt32Array sbrk = ts->shaped_text_get_character_breaks(sctx);
					CHECK_FALSE_MESSAGE(sbrk.size() != 2, "Invalid substring char breaks number.");
					if (sbrk.size() == 2) {
						CHECK_FALSE_MESSAGE(sbrk[0] != 5, "Invalid substring char break position.");
						CHECK_FALSE_MESSAGE(sbrk[1] != 6, "Invalid substring char break position.");
					}
					PackedInt32Array fbrk = ts->shaped_text_get_character_breaks(ctx);
					CHECK_FALSE_MESSAGE(fbrk.size() != 4, "Invalid char breaks number.");
					if (fbrk.size() == 4) {
						CHECK_FALSE_MESSAGE(fbrk[0] != 1, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(fbrk[1] != 5, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(fbrk[2] != 6, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(fbrk[3] != 7, "Invalid char break position.");
					}
					PackedInt32Array rbrk = ts->string_get_character_breaks(U"X❤️‍🔥xs");
					CHECK_FALSE_MESSAGE(rbrk.size() != 4, "Invalid char breaks number.");
					if (rbrk.size() == 4) {
						CHECK_FALSE_MESSAGE(rbrk[0] != 1, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(rbrk[1] != 5, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(rbrk[2] != 6, "Invalid char break position.");
						CHECK_FALSE_MESSAGE(rbrk[3] != 7, "Invalid char break position.");
					}

					ts->free_rid(sctx);
					ts->free_rid(ctx);
				}

				{
					String test = U"Test test long text long text\n";
					RID ctx = ts->create_shaped_text();
					CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
					bool ok = ts->shaped_text_add_string(ctx, test, font, 16);
					CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");
					ts->shaped_text_update_breaks(ctx);
					ts->shaped_text_update_justification_ops(ctx);

					const Glyph *glyphs = ts->shaped_text_get_glyphs(ctx);
					int gl_size = ts->shaped_text_get_glyph_count(ctx);

					CHECK_FALSE_MESSAGE(gl_size != 30, "Invalid glyph count.");
					for (int j = 0; j < gl_size; j++) {
						bool hard = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_HARD) == TextServer::GRAPHEME_IS_BREAK_HARD;
						bool soft = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_SOFT) == TextServer::GRAPHEME_IS_BREAK_SOFT;
						bool space = (glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE;
						bool virt = (glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) == TextServer::GRAPHEME_IS_VIRTUAL;
						bool elo = (glyphs[j].flags & TextServer::GRAPHEME_IS_ELONGATION) == TextServer::GRAPHEME_IS_ELONGATION;
						if (j == 4 || j == 9 || j == 14 || j == 19 || j == 24) {
							CHECK_FALSE_MESSAGE((!soft || !space || hard || virt || elo), "Invalid glyph flags.");
						} else if (j == 29) {
							CHECK_FALSE_MESSAGE((soft || !space || !hard || virt || elo), "Invalid glyph flags.");
						} else {
							CHECK_FALSE_MESSAGE((soft || space || hard || virt || elo), "Invalid glyph flags.");
						}
					}
					ts->free_rid(ctx);
				}

				{
					String test = U"الحمـد";
					RID ctx = ts->create_shaped_text();
					CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
					bool ok = ts->shaped_text_add_string(ctx, test, font, 16);
					CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");
					ts->shaped_text_update_breaks(ctx);

					const Glyph *glyphs = ts->shaped_text_get_glyphs(ctx);
					int gl_size = ts->shaped_text_get_glyph_count(ctx);
					CHECK_FALSE_MESSAGE(gl_size != 6, "Invalid glyph count.");
					for (int j = 0; j < gl_size; j++) {
						bool hard = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_HARD) == TextServer::GRAPHEME_IS_BREAK_HARD;
						bool soft = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_SOFT) == TextServer::GRAPHEME_IS_BREAK_SOFT;
						bool space = (glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE;
						bool virt = (glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) == TextServer::GRAPHEME_IS_VIRTUAL;
						bool elo = (glyphs[j].flags & TextServer::GRAPHEME_IS_ELONGATION) == TextServer::GRAPHEME_IS_ELONGATION;
						CHECK_FALSE_MESSAGE((soft || space || hard || virt || elo), "Invalid glyph flags.");
					}
					if (ts->has_feature(TextServer::FEATURE_KASHIDA_JUSTIFICATION)) {
						ts->shaped_text_update_justification_ops(ctx);

						glyphs = ts->shaped_text_get_glyphs(ctx);
						gl_size = ts->shaped_text_get_glyph_count(ctx);

						CHECK_FALSE_MESSAGE(gl_size != 6, "Invalid glyph count.");
						for (int j = 0; j < gl_size; j++) {
							bool hard = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_HARD) == TextServer::GRAPHEME_IS_BREAK_HARD;
							bool soft = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_SOFT) == TextServer::GRAPHEME_IS_BREAK_SOFT;
							bool space = (glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE;
							bool virt = (glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) == TextServer::GRAPHEME_IS_VIRTUAL;
							bool elo = (glyphs[j].flags & TextServer::GRAPHEME_IS_ELONGATION) == TextServer::GRAPHEME_IS_ELONGATION;
							if (j == 1) {
								CHECK_FALSE_MESSAGE((soft || space || hard || virt || !elo), "Invalid glyph flags.");
							} else {
								CHECK_FALSE_MESSAGE((soft || space || hard || virt || elo), "Invalid glyph flags.");
							}
						}
					}
					ts->free_rid(ctx);
				}

				{
					String test = U"الحمد";
					RID ctx = ts->create_shaped_text();
					CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
					bool ok = ts->shaped_text_add_string(ctx, test, font, 16);
					CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");
					ts->shaped_text_update_breaks(ctx);

					const Glyph *glyphs = ts->shaped_text_get_glyphs(ctx);
					int gl_size = ts->shaped_text_get_glyph_count(ctx);
					CHECK_FALSE_MESSAGE(gl_size != 5, "Invalid glyph count.");
					for (int j = 0; j < gl_size; j++) {
						bool hard = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_HARD) == TextServer::GRAPHEME_IS_BREAK_HARD;
						bool soft = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_SOFT) == TextServer::GRAPHEME_IS_BREAK_SOFT;
						bool space = (glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE;
						bool virt = (glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) == TextServer::GRAPHEME_IS_VIRTUAL;
						bool elo = (glyphs[j].flags & TextServer::GRAPHEME_IS_ELONGATION) == TextServer::GRAPHEME_IS_ELONGATION;
						CHECK_FALSE_MESSAGE((soft || space || hard || virt || elo), "Invalid glyph flags.");
					}

					if (ts->has_feature(TextServer::FEATURE_KASHIDA_JUSTIFICATION)) {
						ts->shaped_text_update_justification_ops(ctx);

						glyphs = ts->shaped_text_get_glyphs(ctx);
						gl_size = ts->shaped_text_get_glyph_count(ctx);

						CHECK_FALSE_MESSAGE(gl_size != 6, "Invalid glyph count.");
						for (int j = 0; j < gl_size; j++) {
							bool hard = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_HARD) == TextServer::GRAPHEME_IS_BREAK_HARD;
							bool soft = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_SOFT) == TextServer::GRAPHEME_IS_BREAK_SOFT;
							bool space = (glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE;
							bool virt = (glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) == TextServer::GRAPHEME_IS_VIRTUAL;
							bool elo = (glyphs[j].flags & TextServer::GRAPHEME_IS_ELONGATION) == TextServer::GRAPHEME_IS_ELONGATION;
							if (j == 1) {
								CHECK_FALSE_MESSAGE((soft || space || hard || !virt || !elo), "Invalid glyph flags.");
							} else {
								CHECK_FALSE_MESSAGE((soft || space || hard || virt || elo), "Invalid glyph flags.");
							}
						}
					}
					ts->free_rid(ctx);
				}

				{
					String test = U"الحمـد الرياضي العربي";
					RID ctx = ts->create_shaped_text();
					CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
					bool ok = ts->shaped_text_add_string(ctx, test, font, 16);
					CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");
					ts->shaped_text_update_breaks(ctx);

					const Glyph *glyphs = ts->shaped_text_get_glyphs(ctx);
					int gl_size = ts->shaped_text_get_glyph_count(ctx);

					CHECK_FALSE_MESSAGE(gl_size != 21, "Invalid glyph count.");
					for (int j = 0; j < gl_size; j++) {
						bool hard = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_HARD) == TextServer::GRAPHEME_IS_BREAK_HARD;
						bool soft = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_SOFT) == TextServer::GRAPHEME_IS_BREAK_SOFT;
						bool space = (glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE;
						bool virt = (glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) == TextServer::GRAPHEME_IS_VIRTUAL;
						bool elo = (glyphs[j].flags & TextServer::GRAPHEME_IS_ELONGATION) == TextServer::GRAPHEME_IS_ELONGATION;
						if (j == 6 || j == 14) {
							CHECK_FALSE_MESSAGE((!soft || !space || hard || virt || elo), "Invalid glyph flags.");
						} else {
							CHECK_FALSE_MESSAGE((soft || space || hard || virt || elo), "Invalid glyph flags.");
						}
					}

					if (ts->has_feature(TextServer::FEATURE_KASHIDA_JUSTIFICATION)) {
						ts->shaped_text_update_justification_ops(ctx);

						glyphs = ts->shaped_text_get_glyphs(ctx);
						gl_size = ts->shaped_text_get_glyph_count(ctx);

						CHECK_FALSE_MESSAGE(gl_size != 23, "Invalid glyph count.");
						for (int j = 0; j < gl_size; j++) {
							bool hard = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_HARD) == TextServer::GRAPHEME_IS_BREAK_HARD;
							bool soft = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_SOFT) == TextServer::GRAPHEME_IS_BREAK_SOFT;
							bool space = (glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE;
							bool virt = (glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) == TextServer::GRAPHEME_IS_VIRTUAL;
							bool elo = (glyphs[j].flags & TextServer::GRAPHEME_IS_ELONGATION) == TextServer::GRAPHEME_IS_ELONGATION;
							if (j == 7 || j == 16) {
								CHECK_FALSE_MESSAGE((!soft || !space || hard || virt || elo), "Invalid glyph flags.");
							} else if (j == 3 || j == 9) {
								CHECK_FALSE_MESSAGE((soft || space || hard || !virt || !elo), "Invalid glyph flags.");
							} else if (j == 18) {
								CHECK_FALSE_MESSAGE((soft || space || hard || virt || !elo), "Invalid glyph flags.");
							} else {
								CHECK_FALSE_MESSAGE((soft || space || hard || virt || elo), "Invalid glyph flags.");
							}
						}
					}

					ts->free_rid(ctx);
				}

				{
					String test = U"เป็น ภาษา ราชการ และ ภาษา";
					RID ctx = ts->create_shaped_text();
					CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
					bool ok = ts->shaped_text_add_string(ctx, test, font, 16);
					CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");
					ts->shaped_text_update_breaks(ctx);
					ts->shaped_text_update_justification_ops(ctx);

					const Glyph *glyphs = ts->shaped_text_get_glyphs(ctx);
					int gl_size = ts->shaped_text_get_glyph_count(ctx);

					CHECK_FALSE_MESSAGE(gl_size != 25, "Invalid glyph count.");
					for (int j = 0; j < gl_size; j++) {
						bool hard = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_HARD) == TextServer::GRAPHEME_IS_BREAK_HARD;
						bool soft = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_SOFT) == TextServer::GRAPHEME_IS_BREAK_SOFT;
						bool space = (glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE;
						bool virt = (glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) == TextServer::GRAPHEME_IS_VIRTUAL;
						bool elo = (glyphs[j].flags & TextServer::GRAPHEME_IS_ELONGATION) == TextServer::GRAPHEME_IS_ELONGATION;
						if (j == 4 || j == 9 || j == 16 || j == 20) {
							CHECK_FALSE_MESSAGE((!soft || !space || hard || virt || elo), "Invalid glyph flags.");
						} else {
							CHECK_FALSE_MESSAGE((soft || space || hard || virt || elo), "Invalid glyph flags.");
						}
					}
					ts->free_rid(ctx);
				}

				if (ts->has_feature(TextServer::FEATURE_BREAK_ITERATORS)) { // Line breaking opportunities.
					String test = U"เป็นภาษาราชการและภาษา";
					RID ctx = ts->create_shaped_text();
					CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
					bool ok = ts->shaped_text_add_string(ctx, test, font, 16);
					CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");
					ts->shaped_text_update_breaks(ctx);
					ts->shaped_text_update_justification_ops(ctx);

					const Glyph *glyphs = ts->shaped_text_get_glyphs(ctx);
					int gl_size = ts->shaped_text_get_glyph_count(ctx);

					CHECK_FALSE_MESSAGE(gl_size != 25, "Invalid glyph count.");
					for (int j = 0; j < gl_size; j++) {
						bool hard = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_HARD) == TextServer::GRAPHEME_IS_BREAK_HARD;
						bool soft = (glyphs[j].flags & TextServer::GRAPHEME_IS_BREAK_SOFT) == TextServer::GRAPHEME_IS_BREAK_SOFT;
						bool space = (glyphs[j].flags & TextServer::GRAPHEME_IS_SPACE) == TextServer::GRAPHEME_IS_SPACE;
						bool virt = (glyphs[j].flags & TextServer::GRAPHEME_IS_VIRTUAL) == TextServer::GRAPHEME_IS_VIRTUAL;
						bool elo = (glyphs[j].flags & TextServer::GRAPHEME_IS_ELONGATION) == TextServer::GRAPHEME_IS_ELONGATION;
						if (j == 4 || j == 9 || j == 16 || j == 20) {
							CHECK_FALSE_MESSAGE((!soft || !space || hard || !virt || elo), "Invalid glyph flags.");
						} else {
							CHECK_FALSE_MESSAGE((soft || space || hard || virt || elo), "Invalid glyph flags.");
						}
					}
					ts->free_rid(ctx);
				}

				if (ts->has_feature(TextServer::FEATURE_BREAK_ITERATORS)) { // Break line.
					struct TestCase {
						String text;
						PackedInt32Array breaks;
					};
					TestCase cases[] = {
						{ U"            เมาส์ตัวนี้", { 0, 17, 17, 23 } },
						{ U"              กู้ไฟล์", { 0, 17, 17, 21 } },
						{ U"             ไม่มีคำ", { 0, 18, 18, 20 } },
						{ U"             ไม่มีคำพูด", { 0, 18, 18, 23 } },
						{ U"            ไม่มีคำ", { 0, 17, 17, 19 } },
						{ U"         มีอุปกรณ์\nนี้", { 0, 11, 11, 19, 19, 22 } },
						{ U"الحمدا لحمدا لحمـــد", { 0, 13, 13, 20 } },
						{ U"         الحمد test", { 0, 15, 15, 19 } },
						{ U"الحمـد الرياضي العربي", { 0, 7, 7, 15, 15, 21 } },
						{ U"test \rtest", { 0, 6, 6, 10 } },
						{ U"test\r test", { 0, 5, 5, 10 } },
						{ U"test\r test \r test", { 0, 5, 5, 12, 12, 17 } },
					};
					for (size_t j = 0; j < sizeof(cases) / sizeof(TestCase); j++) {
						RID ctx = ts->create_shaped_text();
						CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
						bool ok = ts->shaped_text_add_string(ctx, cases[j].text, font, 16);
						CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");

						PackedInt32Array breaks = ts->shaped_text_get_line_breaks(ctx, 90.0);
						CHECK_FALSE_MESSAGE(breaks != cases[j].breaks, "Invalid break points.");

						breaks = ts->shaped_text_get_line_breaks_adv(ctx, { 90.0 }, 0, false);
						CHECK_FALSE_MESSAGE(breaks != cases[j].breaks, "Invalid break points.");

						ts->free_rid(ctx);
					}
				}

				if (ts->has_feature(TextServer::FEATURE_BREAK_ITERATORS)) { // Break line and trim spaces.
					struct TestCase {
						String text;
						PackedInt32Array breaks;
					};
					TestCase cases[] = {
						{ U"test \rtest", { 0, 4, 6, 10 } },
						{ U"test\r test", { 0, 4, 6, 10 } },
						{ U"test\r test \r test", { 0, 4, 6, 10, 13, 17 } },
					};
					for (size_t j = 0; j < sizeof(cases) / sizeof(TestCase); j++) {
						RID ctx = ts->create_shaped_text();
						CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
						bool ok = ts->shaped_text_add_string(ctx, cases[j].text, font, 16);
						CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");

						PackedInt32Array breaks = ts->shaped_text_get_line_breaks(ctx, 90.0, 0, TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND | TextServer::BREAK_TRIM_EDGE_SPACES);
						CHECK_FALSE_MESSAGE(breaks != cases[j].breaks, "Invalid break points.");

						breaks = ts->shaped_text_get_line_breaks_adv(ctx, { 90.0 }, 0, false, TextServer::BREAK_MANDATORY | TextServer::BREAK_WORD_BOUND | TextServer::BREAK_TRIM_EDGE_SPACES);
						CHECK_FALSE_MESSAGE(breaks != cases[j].breaks, "Invalid break points.");

						ts->free_rid(ctx);
					}
				}

				for (int j = 0; j < font.size(); j++) {
					ts->free_rid(font[j]);
				}
				font.clear();
			}
		}

		SUBCASE("[TextServer] Text layout: Line breaking") {
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(i);
				CHECK_FALSE_MESSAGE(ts.is_null(), "Invalid TS interface.");

				if (!ts->has_feature(TextServer::FEATURE_FONT_DYNAMIC) || !ts->has_feature(TextServer::FEATURE_SIMPLE_LAYOUT)) {
					continue;
				}

				String test_1 = U"test test test";
				//                   5^  10^

				RID font1 = ts->create_font();
				ts->font_set_data_ptr(font1, _font_NotoSans_Regular, _font_NotoSans_Regular_size);
				RID font2 = ts->create_font();
				ts->font_set_data_ptr(font2, _font_NotoSansThai_Regular, _font_NotoSansThai_Regular_size);

				Array font;
				font.push_back(font1);
				font.push_back(font2);

				RID ctx = ts->create_shaped_text();
				CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
				bool ok = ts->shaped_text_add_string(ctx, test_1, font, 16);
				CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");

				PackedInt32Array brks = ts->shaped_text_get_line_breaks(ctx, 1);
				CHECK_FALSE_MESSAGE(brks.size() != 6, "Invalid line breaks number.");
				if (brks.size() == 6) {
					CHECK_FALSE_MESSAGE(brks[0] != 0, "Invalid line break position.");
					CHECK_FALSE_MESSAGE(brks[1] != 5, "Invalid line break position.");

					CHECK_FALSE_MESSAGE(brks[2] != 5, "Invalid line break position.");
					CHECK_FALSE_MESSAGE(brks[3] != 10, "Invalid line break position.");

					CHECK_FALSE_MESSAGE(brks[4] != 10, "Invalid line break position.");
					CHECK_FALSE_MESSAGE(brks[5] != 14, "Invalid line break position.");
				}

				ts->free_rid(ctx);

				for (int j = 0; j < font.size(); j++) {
					ts->free_rid(font[j]);
				}
				font.clear();
			}
		}

		SUBCASE("[TextServer] Text layout: Justification") {
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(i);
				CHECK_FALSE_MESSAGE(ts.is_null(), "Invalid TS interface.");

				if (!ts->has_feature(TextServer::FEATURE_FONT_DYNAMIC) || !ts->has_feature(TextServer::FEATURE_SIMPLE_LAYOUT)) {
					continue;
				}

				RID font1 = ts->create_font();
				ts->font_set_data_ptr(font1, _font_NotoSans_Regular, _font_NotoSans_Regular_size);
				RID font2 = ts->create_font();
				ts->font_set_data_ptr(font2, _font_Vazirmatn_Regular, _font_Vazirmatn_Regular_size);

				Array font;
				font.push_back(font1);
				font.push_back(font2);

				String test_1 = U"الحمد";
				String test_2 = U"الحمد test";
				String test_3 = U"test test";
				//                    7^      26^

				RID ctx;
				bool ok;
				float width_old, width;
				if (ts->has_feature(TextServer::FEATURE_KASHIDA_JUSTIFICATION)) {
					ctx = ts->create_shaped_text();
					CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
					ok = ts->shaped_text_add_string(ctx, test_1, font, 16);
					CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");

					width_old = ts->shaped_text_get_width(ctx);
					width = ts->shaped_text_fit_to_width(ctx, 100, TextServer::JUSTIFICATION_WORD_BOUND);
					CHECK_FALSE_MESSAGE((width != width_old), "Invalid fill width.");
					width = ts->shaped_text_fit_to_width(ctx, 100, TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA);
					CHECK_FALSE_MESSAGE((width <= width_old || width > 100), "Invalid fill width.");

					ts->free_rid(ctx);

					ctx = ts->create_shaped_text();
					CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
					ok = ts->shaped_text_add_string(ctx, test_2, font, 16);
					CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");

					width_old = ts->shaped_text_get_width(ctx);
					width = ts->shaped_text_fit_to_width(ctx, 100, TextServer::JUSTIFICATION_WORD_BOUND);
					CHECK_FALSE_MESSAGE((width <= width_old || width > 100), "Invalid fill width.");
					width = ts->shaped_text_fit_to_width(ctx, 100, TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA);
					CHECK_FALSE_MESSAGE((width <= width_old || width > 100), "Invalid fill width.");

					ts->free_rid(ctx);
				}

				ctx = ts->create_shaped_text();
				CHECK_FALSE_MESSAGE(ctx == RID(), "Creating text buffer failed.");
				ok = ts->shaped_text_add_string(ctx, test_3, font, 16);
				CHECK_FALSE_MESSAGE(!ok, "Adding text to the buffer failed.");

				width_old = ts->shaped_text_get_width(ctx);
				width = ts->shaped_text_fit_to_width(ctx, 100, TextServer::JUSTIFICATION_WORD_BOUND);
				CHECK_FALSE_MESSAGE((width <= width_old || width > 100), "Invalid fill width.");

				ts->free_rid(ctx);

				for (int j = 0; j < font.size(); j++) {
					ts->free_rid(font[j]);
				}
				font.clear();
			}
		}

		SUBCASE("[TextServer] Unicode identifiers") {
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(i);
				CHECK_FALSE_MESSAGE(ts.is_null(), "Invalid TS interface.");

				static const char32_t *data[19] = { U"-30", U"100", U"10.1", U"10,1", U"1e2", U"1e-2", U"1e2e3", U"0xAB", U"AB", U"Test1", U"1Test", U"Test*1", U"test_testeT", U"test_tes teT", U"عَلَيْكُمْ", U"عَلَيْكُمْTest", U"ӒӖӚӜ", U"_test", U"ÂÃÄÅĀĂĄÇĆĈĊ" };
				static bool isid[19] = { false, false, false, false, false, false, false, false, true, true, false, false, true, false, true, true, true, true, true };
				for (int j = 0; j < 19; j++) {
					String s = String(data[j]);
					CHECK(ts->is_valid_identifier(s) == isid[j]);
				}

				if (ts->has_feature(TextServer::FEATURE_UNICODE_IDENTIFIERS)) {
					// Test UAX 3.2 ZW(N)J usage.
					CHECK(ts->is_valid_identifier(U"\u0646\u0627\u0645\u0647\u200C\u0627\u06CC"));
					CHECK(ts->is_valid_identifier(U"\u0D26\u0D43\u0D15\u0D4D\u200C\u0D38\u0D3E\u0D15\u0D4D\u0D37\u0D3F"));
					CHECK(ts->is_valid_identifier(U"\u0DC1\u0DCA\u200D\u0DBB\u0DD3"));
				}
			}
		}

		SUBCASE("[TextServer] Unicode letters") {
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(i);
				CHECK_FALSE_MESSAGE(ts.is_null(), "Invalid TS interface.");

				struct ul_testcase {
					int fail_index = -1; // Expecting failure at given index.
					char32_t text[10]; // Using 0 as the terminator.
				};
				ul_testcase cases[14] = {
					{
							0,
							{ 0x2D, 0x33, 0x30, 0, 0, 0, 0, 0, 0, 0 }, // "-30"
					},
					{
							1,
							{ 0x61, 0x2E, 0x31, 0, 0, 0, 0, 0, 0, 0 }, // "a.1"
					},
					{
							1,
							{ 0x61, 0x2C, 0x31, 0, 0, 0, 0, 0, 0, 0 }, // "a,1"
					},
					{
							0,
							{ 0x31, 0x65, 0x2D, 0x32, 0, 0, 0, 0, 0, 0 }, // "1e-2"
					},
					{
							0,
							{ 0xAB, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, // "Left-Pointing Double Angle Quotation Mark"
					},
					{
							-1,
							{ 0x41, 0x42, 0, 0, 0, 0, 0, 0, 0, 0 }, // "AB"
					},
					{
							4,
							{ 0x54, 0x65, 0x73, 0x74, 0x31, 0, 0, 0, 0, 0 }, // "Test1"
					},
					{
							2,
							{ 0x54, 0x65, 0x2A, 0x73, 0x74, 0, 0, 0, 0, 0 }, // "Te*st"
					},
					{
							4,
							{ 0x74, 0x65, 0x73, 0x74, 0x5F, 0x74, 0x65, 0x73, 0x74, 0x65 }, // "test_teste"
					},
					{
							4,
							{ 0x74, 0x65, 0x73, 0x74, 0x20, 0x74, 0x65, 0x73, 0x74, 0 }, // "test test"
					},
					{
							-1,
							{ 0x643, 0x402, 0x716, 0xB05, 0, 0, 0, 0, 0, 0 }, // "كЂܖଅ" (arabic letters),
					},
					{
							-1,
							{ 0x643, 0x402, 0x716, 0xB05, 0x54, 0x65, 0x73, 0x74, 0x30AA, 0x4E21 }, // 0-3 arabic letters, 4-7 latin letters, 8-9 CJK letters
					},
					{
							-1,
							{ 0x4D2, 0x4D6, 0x4DA, 0x4DC, 0, 0, 0, 0, 0, 0 }, // "ӒӖӚӜ" cyrillic letters
					},
					{
							-1,
							{ 0xC2, 0xC3, 0xC4, 0xC5, 0x100, 0x102, 0x104, 0xC7, 0x106, 0x108 }, // "ÂÃÄÅĀĂĄÇĆĈ" rarer latin letters
					},
				};

				for (int j = 0; j < 14; j++) {
					ul_testcase test = cases[j];
					int failed_on_index = -1;
					for (int k = 0; k < 10; k++) {
						char32_t character = test.text[k];
						if (character == 0) {
							break;
						}
						if (!ts->is_valid_letter(character)) {
							failed_on_index = k;
							break;
						}
					}

					if (test.fail_index == -1) {
						CHECK_MESSAGE(test.fail_index == failed_on_index, "In interface ", ts->get_name() + ": In test case ", j, ", the character at index ", failed_on_index, " should have been a letter.");
					} else {
						CHECK_MESSAGE(test.fail_index == failed_on_index, "In interface ", ts->get_name() + ": In test case ", j, ", expected first non-letter at index ", test.fail_index, ", but found at index ", failed_on_index);
					}
				}
			}
		}

		SUBCASE("[TextServer] Strip Diacritics") {
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(i);
				CHECK_FALSE_MESSAGE(ts.is_null(), "Invalid TS interface.");

				if (ts->has_feature(TextServer::FEATURE_SHAPING)) {
					CHECK(ts->strip_diacritics(U"ٱلسَّلَامُ عَلَيْكُمْ") == U"ٱلسلام عليكم");
				}

				CHECK(ts->strip_diacritics(U"pêches épinards tomates fraises") == U"peches epinards tomates fraises");
				CHECK(ts->strip_diacritics(U"ΆΈΉΊΌΎΏΪΫϓϔ") == U"ΑΕΗΙΟΥΩΙΥΥΥ");
				CHECK(ts->strip_diacritics(U"άέήίΐϊΰϋόύώ") == U"αεηιιιυυουω");
				CHECK(ts->strip_diacritics(U"ЀЁЃ ЇЌЍӢӤЙ ЎӮӰӲ ӐӒӖӚӜӞ ӦӪ Ӭ Ӵ Ӹ") == U"ЕЕГ ІКИИИИ УУУУ ААЕӘЖЗ ОӨ Э Ч Ы");
				CHECK(ts->strip_diacritics(U"ѐёѓ їќѝӣӥй ўӯӱӳ ӑӓӗӛӝӟ ӧӫ ӭ ӵ ӹ") == U"еег ікииии уууу ааеәжз оө э ч ы");
				CHECK(ts->strip_diacritics(U"ÀÁÂÃÄÅĀĂĄÇĆĈĊČĎÈÉÊËĒĔĖĘĚĜĞĠĢĤÌÍÎÏĨĪĬĮİĴĶĹĻĽÑŃŅŇŊÒÓÔÕÖØŌŎŐƠŔŖŘŚŜŞŠŢŤÙÚÛÜŨŪŬŮŰŲƯŴÝŶŹŻŽ") == U"AAAAAAAAACCCCCDEEEEEEEEEGGGGHIIIIIIIIIJKLLLNNNNŊOOOOOØOOOORRRSSSSTTUUUUUUUUUUUWYYZZZ");
				CHECK(ts->strip_diacritics(U"àáâãäåāăąçćĉċčďèéêëēĕėęěĝğġģĥìíîïĩīĭįĵķĺļľñńņňŋòóôõöøōŏőơŕŗřśŝşšţťùúûüũūŭůűųưŵýÿŷźżž") == U"aaaaaaaaacccccdeeeeeeeeegggghiiiiiiiijklllnnnnŋoooooøoooorrrssssttuuuuuuuuuuuwyyyzzz");
				CHECK(ts->strip_diacritics(U"ǍǏȈǑǪǬȌȎȪȬȮȰǓǕǗǙǛȔȖǞǠǺȀȂȦǢǼǦǴǨǸȆȐȒȘȚȞȨ Ḁ ḂḄḆ Ḉ ḊḌḎḐḒ ḔḖḘḚḜ Ḟ Ḡ ḢḤḦḨḪ ḬḮ ḰḲḴ ḶḸḺḼ ḾṀṂ ṄṆṈṊ ṌṎṐṒ ṔṖ ṘṚṜṞ ṠṢṤṦṨ ṪṬṮṰ ṲṴṶṸṺ") == U"AIIOOOOOOOOOUUUUUUUAAAAAAÆÆGGKNERRSTHE A BBB C DDDDD EEEEE F G HHHHH II KKK LLLL MMM NNNN OOOO PP RRRR SSSSS TTTT UUUUU");
				CHECK(ts->strip_diacritics(U"ǎǐȉȋǒǫǭȍȏȫȭȯȱǔǖǘǚǜȕȗǟǡǻȁȃȧǣǽǧǵǩǹȇȑȓșțȟȩ ḁ ḃḅḇ ḉ ḋḍḏḑḓ ḟ ḡ ḭḯ ḱḳḵ ḷḹḻḽ ḿṁṃ ṅṇṉṋ ṍṏṑṓ ṗṕ ṙṛṝṟ ṡṣṥṧṩ ṫṭṯṱ ṳṵṷṹṻ") == U"aiiiooooooooouuuuuuuaaaaaaææggknerrsthe a bbb c ddddd f g ii kkk llll mmm nnnn oooo pp rrrr sssss tttt uuuuu");
				CHECK(ts->strip_diacritics(U"ṼṾ ẀẂẄẆẈ ẊẌ Ẏ ẐẒẔ") == U"VV WWWWW XX Y ZZZ");
				CHECK(ts->strip_diacritics(U"ṽṿ ẁẃẅẇẉ ẋẍ ẏ ẑẓẕ ẖ ẗẘẙẛ") == U"vv wwwww xx y zzz h twys");
			}
		}

		SUBCASE("[TextServer] Word break") {
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(i);

				if (!ts->has_feature(TextServer::FEATURE_SIMPLE_LAYOUT)) {
					continue;
				}

				CHECK_FALSE_MESSAGE(ts.is_null(), "Invalid TS interface.");
				{
					String text1 = U"linguistically similar and effectively form";
					//                           14^     22^ 26^         38^
					PackedInt32Array breaks = ts->string_get_word_breaks(text1, "en");
					CHECK(breaks.size() == 10);
					if (breaks.size() == 10) {
						CHECK(breaks[0] == 0);
						CHECK(breaks[1] == 14);
						CHECK(breaks[2] == 15);
						CHECK(breaks[3] == 22);
						CHECK(breaks[4] == 23);
						CHECK(breaks[5] == 26);
						CHECK(breaks[6] == 27);
						CHECK(breaks[7] == 38);
						CHECK(breaks[8] == 39);
						CHECK(breaks[9] == 43);
					}
				}

				if (ts->has_feature(TextServer::FEATURE_BREAK_ITERATORS)) {
					String text2 = U"เป็นภาษาราชการและภาษาประจำชาติของประเทศไทย";
					//				 เป็น ภาษา ราชการ และ ภาษา ประจำ ชาติ ของ ประเทศไทย
					//                 3^   7^    13^ 16^  20^   25^ 29^ 32^

					PackedInt32Array breaks = ts->string_get_word_breaks(text2, "th");
					CHECK(breaks.size() == 18);
					if (breaks.size() == 18) {
						CHECK(breaks[0] == 0);
						CHECK(breaks[1] == 4);
						CHECK(breaks[2] == 4);
						CHECK(breaks[3] == 8);
						CHECK(breaks[4] == 8);
						CHECK(breaks[5] == 14);
						CHECK(breaks[6] == 14);
						CHECK(breaks[7] == 17);
						CHECK(breaks[8] == 17);
						CHECK(breaks[9] == 21);
						CHECK(breaks[10] == 21);
						CHECK(breaks[11] == 26);
						CHECK(breaks[12] == 26);
						CHECK(breaks[13] == 30);
						CHECK(breaks[14] == 30);
						CHECK(breaks[15] == 33);
						CHECK(breaks[16] == 33);
						CHECK(breaks[17] == 42);
					}
				}

				if (ts->has_feature(TextServer::FEATURE_BREAK_ITERATORS)) {
					String text2 = U"U+2764 U+FE0F U+200D U+1F525 ; 13.1 # ❤️‍🔥";

					PackedInt32Array breaks = ts->string_get_character_breaks(text2, "en");
					CHECK(breaks.size() == 39);
					if (breaks.size() == 39) {
						CHECK(breaks[0] == 1);
						CHECK(breaks[1] == 2);
						CHECK(breaks[2] == 3);
						CHECK(breaks[3] == 4);
						CHECK(breaks[4] == 5);
						CHECK(breaks[5] == 6);
						CHECK(breaks[6] == 7);
						CHECK(breaks[7] == 8);
						CHECK(breaks[8] == 9);
						CHECK(breaks[9] == 10);
						CHECK(breaks[10] == 11);
						CHECK(breaks[11] == 12);
						CHECK(breaks[12] == 13);
						CHECK(breaks[13] == 14);
						CHECK(breaks[14] == 15);
						CHECK(breaks[15] == 16);
						CHECK(breaks[16] == 17);
						CHECK(breaks[17] == 18);
						CHECK(breaks[18] == 19);
						CHECK(breaks[19] == 20);
						CHECK(breaks[20] == 21);
						CHECK(breaks[21] == 22);
						CHECK(breaks[22] == 23);
						CHECK(breaks[23] == 24);
						CHECK(breaks[24] == 25);
						CHECK(breaks[25] == 26);
						CHECK(breaks[26] == 27);
						CHECK(breaks[27] == 28);
						CHECK(breaks[28] == 29);
						CHECK(breaks[29] == 30);
						CHECK(breaks[30] == 31);
						CHECK(breaks[31] == 32);
						CHECK(breaks[32] == 33);
						CHECK(breaks[33] == 34);
						CHECK(breaks[34] == 35);
						CHECK(breaks[35] == 36);
						CHECK(breaks[36] == 37);
						CHECK(breaks[37] == 38);
						CHECK(breaks[38] == 42);
					}
				}
			}
		}
	}
}
}; // namespace TestTextServer

#endif // TOOLS_ENABLED

#endif // TEST_TEXT_SERVER_H
