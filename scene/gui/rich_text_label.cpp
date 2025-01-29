/**************************************************************************/
/*  rich_text_label.cpp                                                   */
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

#include "rich_text_label.h"
#include "rich_text_label.compat.inc"

#include "core/input/input_map.h"
#include "core/math/math_defs.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "scene/gui/label.h"
#include "scene/gui/rich_text_effect.h"
#include "scene/resources/atlas_texture.h"
#include "scene/theme/theme_db.h"
#include "servers/display_server.h"

#include "modules/modules_enabled.gen.h" // For regex.
#ifdef MODULE_REGEX_ENABLED
#include "modules/regex/regex.h"
#endif

RichTextLabel::ItemCustomFX::ItemCustomFX() {
	type = ITEM_CUSTOMFX;
	char_fx_transform.instantiate();
}

RichTextLabel::ItemCustomFX::~ItemCustomFX() {
	_clear_children();

	char_fx_transform.unref();
	custom_effect.unref();
}

RichTextLabel::Item *RichTextLabel::_get_next_item(Item *p_item, bool p_free) const {
	if (p_free) {
		if (p_item->subitems.size()) {
			return p_item->subitems.front()->get();
		} else if (!p_item->parent) {
			return nullptr;
		} else if (p_item->E->next()) {
			return p_item->E->next()->get();
		} else {
			// Go up until something with a next is found.
			while (p_item->parent && !p_item->E->next()) {
				p_item = p_item->parent;
			}

			if (p_item->parent) {
				return p_item->E->next()->get();
			} else {
				return nullptr;
			}
		}

	} else {
		if (p_item->subitems.size() && p_item->type != ITEM_TABLE) {
			return p_item->subitems.front()->get();
		} else if (p_item->type == ITEM_FRAME) {
			return nullptr;
		} else if (p_item->E->next()) {
			return p_item->E->next()->get();
		} else {
			// Go up until something with a next is found.
			while (p_item->type != ITEM_FRAME && !p_item->E->next()) {
				p_item = p_item->parent;
			}

			if (p_item->type != ITEM_FRAME) {
				return p_item->E->next()->get();
			} else {
				return nullptr;
			}
		}
	}
}

RichTextLabel::Item *RichTextLabel::_get_prev_item(Item *p_item, bool p_free) const {
	if (p_free) {
		if (p_item->subitems.size()) {
			return p_item->subitems.back()->get();
		} else if (!p_item->parent) {
			return nullptr;
		} else if (p_item->E->prev()) {
			return p_item->E->prev()->get();
		} else {
			// Go back until something with a prev is found.
			while (p_item->parent && !p_item->E->prev()) {
				p_item = p_item->parent;
			}

			if (p_item->parent) {
				return p_item->E->prev()->get();
			} else {
				return nullptr;
			}
		}

	} else {
		if (p_item->subitems.size() && p_item->type != ITEM_TABLE) {
			return p_item->subitems.back()->get();
		} else if (p_item->type == ITEM_FRAME) {
			return nullptr;
		} else if (p_item->E->prev()) {
			return p_item->E->prev()->get();
		} else {
			// Go back until something with a prev is found.
			while (p_item->type != ITEM_FRAME && !p_item->E->prev()) {
				p_item = p_item->parent;
			}

			if (p_item->type != ITEM_FRAME) {
				return p_item->E->prev()->get();
			} else {
				return nullptr;
			}
		}
	}
}

Rect2 RichTextLabel::_get_text_rect() {
	return Rect2(theme_cache.normal_style->get_offset(), get_size() - theme_cache.normal_style->get_minimum_size());
}

RichTextLabel::Item *RichTextLabel::_get_item_at_pos(RichTextLabel::Item *p_item_from, RichTextLabel::Item *p_item_to, int p_position) {
	int offset = 0;
	for (Item *it = p_item_from; it && it != p_item_to; it = _get_next_item(it)) {
		switch (it->type) {
			case ITEM_TEXT: {
				ItemText *t = static_cast<ItemText *>(it);
				offset += t->text.length();
				if (offset > p_position) {
					return it;
				}
			} break;
			case ITEM_NEWLINE: {
				offset += 1;
				if (offset == p_position) {
					return it;
				}
			} break;
			case ITEM_IMAGE: {
				offset += 1;
				if (offset > p_position) {
					return it;
				}
			} break;
			case ITEM_TABLE: {
				offset += 1;
			} break;
			default:
				break;
		}
	}
	return p_item_from;
}

String RichTextLabel::_roman(int p_num, bool p_capitalize) const {
	if (p_num > 3999) {
		return "ERR";
	};
	String s;
	if (p_capitalize) {
		const String roman_M[] = { "", "M", "MM", "MMM" };
		const String roman_C[] = { "", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM" };
		const String roman_X[] = { "", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC" };
		const String roman_I[] = { "", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX" };
		s = roman_M[p_num / 1000] + roman_C[(p_num % 1000) / 100] + roman_X[(p_num % 100) / 10] + roman_I[p_num % 10];
	} else {
		const String roman_M[] = { "", "m", "mm", "mmm" };
		const String roman_C[] = { "", "c", "cc", "ccc", "cd", "d", "dc", "dcc", "dccc", "cm" };
		const String roman_X[] = { "", "x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc" };
		const String roman_I[] = { "", "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix" };
		s = roman_M[p_num / 1000] + roman_C[(p_num % 1000) / 100] + roman_X[(p_num % 100) / 10] + roman_I[p_num % 10];
	}
	return s;
}

String RichTextLabel::_letters(int p_num, bool p_capitalize) const {
	int64_t n = p_num;

	int chars = 0;
	do {
		n /= 24;
		chars++;
	} while (n);

	String s;
	s.resize(chars + 1);
	char32_t *c = s.ptrw();
	c[chars] = 0;
	n = p_num;
	do {
		int mod = ABS(n % 24);
		char a = (p_capitalize ? 'A' : 'a');
		c[--chars] = a + mod - 1;

		n /= 24;
	} while (n);

	return s;
}

String RichTextLabel::_get_prefix(Item *p_item, const Vector<int> &p_list_index, const Vector<ItemList *> &p_list_items) {
	String prefix;
	int segments = 0;
	for (int i = 0; i < p_list_index.size(); i++) {
		String segment;
		if (p_list_items[i]->list_type == LIST_DOTS) {
			if (segments == 0) {
				prefix = p_list_items[i]->bullet;
			}
			break;
		}
		prefix = "." + prefix;
		if (p_list_items[i]->list_type == LIST_NUMBERS) {
			segment = itos(p_list_index[i]);
			if (is_localizing_numeral_system()) {
				segment = TS->format_number(segment, _find_language(p_item));
			}
			segments++;
		} else if (p_list_items[i]->list_type == LIST_LETTERS) {
			segment = _letters(p_list_index[i], p_list_items[i]->capitalize);
			segments++;
		} else if (p_list_items[i]->list_type == LIST_ROMAN) {
			segment = _roman(p_list_index[i], p_list_items[i]->capitalize);
			segments++;
		}
		prefix = segment + prefix;
	}
	return prefix;
}

void RichTextLabel::_update_line_font(ItemFrame *p_frame, int p_line, const Ref<Font> &p_base_font, int p_base_font_size) {
	ERR_FAIL_NULL(p_frame);
	ERR_FAIL_COND(p_line < 0 || p_line >= (int)p_frame->lines.size());

	Line &l = p_frame->lines[p_line];
	MutexLock lock(l.text_buf->get_mutex());

	// Prefix.
	Vector<int> list_index;
	Vector<int> list_count;
	Vector<ItemList *> list_items;
	_find_list(l.from, list_index, list_count, list_items);

	if (list_items.size() > 0) {
		if (list_index[0] == 1) {
			// List level start, shape all prefixes for this level and compute max. prefix width.
			list_items[0]->max_width = 0;
			int index = 0;
			for (int i = p_line; i < (int)p_frame->lines.size(); i++) {
				Line &list_l = p_frame->lines[i];
				if (_find_list_item(list_l.from) == list_items[0]) {
					index++;

					Ref<Font> font = theme_cache.normal_font;
					int font_size = theme_cache.normal_font_size;

					ItemFont *font_it = _find_font(list_l.from);
					if (font_it) {
						if (font_it->font.is_valid()) {
							font = font_it->font;
						}
						if (font_it->font_size > 0) {
							font_size = font_it->font_size;
						}
					}
					ItemFontSize *font_size_it = _find_font_size(list_l.from);
					if (font_size_it && font_size_it->font_size > 0) {
						font_size = font_size_it->font_size;
					}

					list_index.write[0] = index;
					String prefix = _get_prefix(list_l.from, list_index, list_items);
					list_l.text_prefix.instantiate();
					list_l.text_prefix->set_direction(_find_direction(list_l.from));
					list_l.text_prefix->add_string(prefix, font, font_size);
					list_items.write[0]->max_width = MAX(list_items[0]->max_width, list_l.text_prefix->get_size().x);
				}
			}
		}
		l.prefix_width = list_items[0]->max_width;
	}

	RID t = l.text_buf->get_rid();
	int spans = TS->shaped_get_span_count(t);
	for (int i = 0; i < spans; i++) {
		Item *it_span = items.get_or_null(TS->shaped_get_span_meta(t, i));
		ItemText *it = reinterpret_cast<ItemText *>(it_span);
		if (it) {
			Ref<Font> font = p_base_font;
			int font_size = p_base_font_size;

			ItemFont *font_it = _find_font(it);
			if (font_it) {
				if (font_it->font.is_valid()) {
					font = font_it->font;
				}
				if (font_it->font_size > 0) {
					font_size = font_it->font_size;
				}
			}
			ItemFontSize *font_size_it = _find_font_size(it);
			if (font_size_it && font_size_it->font_size > 0) {
				font_size = font_size_it->font_size;
			}
			TS->shaped_set_span_update_font(t, i, font->get_rids(), font_size, font->get_opentype_features());
		} else {
			TS->shaped_set_span_update_font(t, i, p_base_font->get_rids(), p_base_font_size, p_base_font->get_opentype_features());
		}
	}

	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	for (Item *it = l.from; it && it != it_to; it = _get_next_item(it)) {
		switch (it->type) {
			case ITEM_TABLE: {
				ItemTable *table = static_cast<ItemTable *>(it);
				for (Item *E : table->subitems) {
					ERR_CONTINUE(E->type != ITEM_FRAME); // Children should all be frames.
					ItemFrame *frame = static_cast<ItemFrame *>(E);
					for (int i = 0; i < (int)frame->lines.size(); i++) {
						_update_line_font(frame, i, p_base_font, p_base_font_size);
					}
				}
			} break;
			default:
				break;
		}
	}
}

float RichTextLabel::_resize_line(ItemFrame *p_frame, int p_line, const Ref<Font> &p_base_font, int p_base_font_size, int p_width, float p_h) {
	ERR_FAIL_NULL_V(p_frame, p_h);
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)p_frame->lines.size(), p_h);

	Line &l = p_frame->lines[p_line];
	MutexLock lock(l.text_buf->get_mutex());

	l.indent = _find_margin(l.from, p_base_font, p_base_font_size) + l.prefix_width;
	l.offset.x = l.indent;
	l.text_buf->set_width(p_width - l.offset.x);

	PackedFloat32Array tab_stops = _find_tab_stops(l.from);
	if (!tab_stops.is_empty()) {
		l.text_buf->tab_align(tab_stops);
	} else if (tab_size > 0) { // Align inline tabs.
		Vector<float> tabs;
		tabs.push_back(tab_size * p_base_font->get_char_size(' ', p_base_font_size).width);
		l.text_buf->tab_align(tabs);
	}

	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	for (Item *it = l.from; it && it != it_to; it = _get_next_item(it)) {
		switch (it->type) {
			case ITEM_IMAGE: {
				ItemImage *img = static_cast<ItemImage *>(it);
				Size2 img_size = img->size;
				if (img->size_in_percent) {
					img_size = _get_image_size(img->image, p_width * img->rq_size.width / 100.f, p_width * img->rq_size.height / 100.f, img->region);
					l.text_buf->resize_object(it->rid, img_size, img->inline_align);
				}
			} break;
			case ITEM_TABLE: {
				ItemTable *table = static_cast<ItemTable *>(it);
				int col_count = table->columns.size();

				for (int i = 0; i < col_count; i++) {
					table->columns[i].width = 0;
				}

				for (Item *E : table->subitems) {
					ERR_CONTINUE(E->type != ITEM_FRAME); // Children should all be frames.
					ItemFrame *frame = static_cast<ItemFrame *>(E);
					float prev_h = 0;
					for (int i = 0; i < (int)frame->lines.size(); i++) {
						MutexLock sub_lock(frame->lines[i].text_buf->get_mutex());
						int w = _find_margin(frame->lines[i].from, p_base_font, p_base_font_size) + 1;
						prev_h = _resize_line(frame, i, p_base_font, p_base_font_size, w, prev_h);
					}
				}

				const int available_width = p_width - theme_cache.table_h_separation * (col_count - 1);
				_set_table_size(table, available_width);

				int row_idx = (table->align_to_row < 0) ? table->rows_baseline.size() - 1 : table->align_to_row;
				if (table->rows_baseline.size() != 0 && row_idx < (int)table->rows_baseline.size()) {
					l.text_buf->resize_object(it->rid, Size2(table->total_width, table->total_height), table->inline_align, Math::round(table->rows_baseline[row_idx]));
				} else {
					l.text_buf->resize_object(it->rid, Size2(table->total_width, table->total_height), table->inline_align);
				}
			} break;
			default:
				break;
		}
	}

	l.offset.y = p_h;
	return _calculate_line_vertical_offset(l);
}

float RichTextLabel::_shape_line(ItemFrame *p_frame, int p_line, const Ref<Font> &p_base_font, int p_base_font_size, int p_width, float p_h, int *r_char_offset) {
	ERR_FAIL_NULL_V(p_frame, p_h);
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)p_frame->lines.size(), p_h);

	Line &l = p_frame->lines[p_line];
	MutexLock lock(l.text_buf->get_mutex());

	BitField<TextServer::LineBreakFlag> autowrap_flags = TextServer::BREAK_MANDATORY;
	switch (autowrap_mode) {
		case TextServer::AUTOWRAP_WORD_SMART:
			autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_ADAPTIVE | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_WORD:
			autowrap_flags = TextServer::BREAK_WORD_BOUND | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_ARBITRARY:
			autowrap_flags = TextServer::BREAK_GRAPHEME_BOUND | TextServer::BREAK_MANDATORY;
			break;
		case TextServer::AUTOWRAP_OFF:
			break;
	}
	autowrap_flags = autowrap_flags | TextServer::BREAK_TRIM_EDGE_SPACES;

	// Clear cache.
	l.text_buf->clear();
	l.text_buf->set_break_flags(autowrap_flags);
	l.text_buf->set_justification_flags(_find_jst_flags(l.from));
	l.char_offset = *r_char_offset;
	l.char_count = 0;

	// List prefix.
	Vector<int> list_index;
	Vector<int> list_count;
	Vector<ItemList *> list_items;
	_find_list(l.from, list_index, list_count, list_items);

	if (list_items.size() > 0) {
		if (list_index[0] == 1) {
			// List level start, shape all prefixes for this level and compute max. prefix width.
			list_items[0]->max_width = 0;
			int index = 0;
			for (int i = p_line; i < (int)p_frame->lines.size(); i++) {
				Line &list_l = p_frame->lines[i];
				if (_find_list_item(list_l.from) == list_items[0]) {
					index++;

					Ref<Font> font = theme_cache.normal_font;
					int font_size = theme_cache.normal_font_size;

					ItemFont *font_it = _find_font(list_l.from);
					if (font_it) {
						if (font_it->font.is_valid()) {
							font = font_it->font;
						}
						if (font_it->font_size > 0) {
							font_size = font_it->font_size;
						}
					}
					ItemFontSize *font_size_it = _find_font_size(list_l.from);
					if (font_size_it && font_size_it->font_size > 0) {
						font_size = font_size_it->font_size;
					}

					list_index.write[0] = index;
					String prefix = _get_prefix(list_l.from, list_index, list_items);
					list_l.text_prefix.instantiate();
					list_l.text_prefix->set_direction(_find_direction(list_l.from));
					list_l.text_prefix->add_string(prefix, font, font_size);
					list_items.write[0]->max_width = MAX(list_items[0]->max_width, list_l.text_prefix->get_size().x);
				}
			}
		}
		l.prefix_width = list_items[0]->max_width;
	}

	// Add indent.
	l.indent = _find_margin(l.from, p_base_font, p_base_font_size) + l.prefix_width;
	l.offset.x = l.indent;
	l.text_buf->set_width(p_width - l.offset.x);
	l.text_buf->set_alignment(_find_alignment(l.from));
	l.text_buf->set_direction(_find_direction(l.from));

	PackedFloat32Array tab_stops = _find_tab_stops(l.from);
	if (!tab_stops.is_empty()) {
		l.text_buf->tab_align(tab_stops);
	} else if (tab_size > 0) { // Align inline tabs.
		Vector<float> tabs;
		tabs.push_back(tab_size * p_base_font->get_char_size(' ', p_base_font_size).width);
		l.text_buf->tab_align(tabs);
	}

	// Shape current paragraph.
	String txt;
	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	int remaining_characters = visible_characters - l.char_offset;
	for (Item *it = l.from; it && it != it_to; it = _get_next_item(it)) {
		if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING && visible_characters >= 0 && remaining_characters <= 0) {
			break;
		}
		switch (it->type) {
			case ITEM_DROPCAP: {
				// Add dropcap.
				const ItemDropcap *dc = static_cast<ItemDropcap *>(it);
				l.text_buf->set_dropcap(dc->text, dc->font, dc->font_size, dc->dropcap_margins);
				l.dc_color = dc->color;
				l.dc_ol_size = dc->ol_size;
				l.dc_ol_color = dc->ol_color;
			} break;
			case ITEM_NEWLINE: {
				Ref<Font> font = p_base_font;
				int font_size = p_base_font_size;

				ItemFont *font_it = _find_font(it);
				if (font_it) {
					if (font_it->font.is_valid()) {
						font = font_it->font;
					}
					if (font_it->font_size > 0) {
						font_size = font_it->font_size;
					}
				}
				ItemFontSize *font_size_it = _find_font_size(it);
				if (font_size_it && font_size_it->font_size > 0) {
					font_size = font_size_it->font_size;
				}
				l.text_buf->add_string(String::chr(0x200B), font, font_size, String(), it->rid);
				txt += "\n";
				l.char_count++;
				remaining_characters--;
			} break;
			case ITEM_TEXT: {
				ItemText *t = static_cast<ItemText *>(it);
				Ref<Font> font = p_base_font;
				int font_size = p_base_font_size;

				ItemFont *font_it = _find_font(it);
				if (font_it) {
					if (font_it->font.is_valid()) {
						font = font_it->font;
					}
					if (font_it->font_size > 0) {
						font_size = font_it->font_size;
					}
				}
				ItemFontSize *font_size_it = _find_font_size(it);
				if (font_size_it && font_size_it->font_size > 0) {
					font_size = font_size_it->font_size;
				}
				String lang = _find_language(it);
				String tx = t->text;
				if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING && visible_characters >= 0 && remaining_characters >= 0) {
					tx = tx.substr(0, remaining_characters);
				}
				remaining_characters -= tx.length();

				l.text_buf->add_string(tx, font, font_size, lang, it->rid);
				txt += tx;
				l.char_count += tx.length();
			} break;
			case ITEM_IMAGE: {
				ItemImage *img = static_cast<ItemImage *>(it);
				Size2 img_size = img->size;
				if (img->size_in_percent) {
					img_size = _get_image_size(img->image, p_width * img->rq_size.width / 100.f, p_width * img->rq_size.height / 100.f, img->region);
				}
				l.text_buf->add_object(it->rid, img_size, img->inline_align, 1);
				txt += String::chr(0xfffc);
				l.char_count++;
				remaining_characters--;
			} break;
			case ITEM_TABLE: {
				ItemTable *table = static_cast<ItemTable *>(it);
				int col_count = table->columns.size();
				int t_char_count = 0;
				// Set minimums to zero.
				for (int i = 0; i < col_count; i++) {
					table->columns[i].min_width = 0;
					table->columns[i].max_width = 0;
					table->columns[i].width = 0;
				}
				// Compute minimum width for each cell.
				const int available_width = p_width - theme_cache.table_h_separation * (col_count - 1);

				int idx = 0;
				for (Item *E : table->subitems) {
					ERR_CONTINUE(E->type != ITEM_FRAME); // Children should all be frames.
					ItemFrame *frame = static_cast<ItemFrame *>(E);

					int column = idx % col_count;
					float prev_h = 0;
					for (int i = 0; i < (int)frame->lines.size(); i++) {
						MutexLock sub_lock(frame->lines[i].text_buf->get_mutex());

						int char_offset = l.char_offset + l.char_count;
						int w = _find_margin(frame->lines[i].from, p_base_font, p_base_font_size) + 1;
						prev_h = _shape_line(frame, i, p_base_font, p_base_font_size, w, prev_h, &char_offset);
						int cell_ch = (char_offset - (l.char_offset + l.char_count));
						l.char_count += cell_ch;
						t_char_count += cell_ch;
						remaining_characters -= cell_ch;

						table->columns[column].min_width = MAX(table->columns[column].min_width, frame->lines[i].indent + ceil(frame->lines[i].text_buf->get_size().x));
						table->columns[column].max_width = MAX(table->columns[column].max_width, frame->lines[i].indent + ceil(frame->lines[i].text_buf->get_non_wrapped_size().x));
					}
					idx++;
				}

				_set_table_size(table, available_width);

				int row_idx = (table->align_to_row < 0) ? table->rows_baseline.size() - 1 : table->align_to_row;
				if (table->rows_baseline.size() != 0 && row_idx < (int)table->rows_baseline.size()) {
					l.text_buf->add_object(it->rid, Size2(table->total_width, table->total_height), table->inline_align, t_char_count, Math::round(table->rows_baseline[row_idx]));
				} else {
					l.text_buf->add_object(it->rid, Size2(table->total_width, table->total_height), table->inline_align, t_char_count);
				}
				txt += String::chr(0xfffc).repeat(t_char_count);
			} break;
			default:
				break;
		}
	}

	// Apply BiDi override.
	l.text_buf->set_bidi_override(structured_text_parser(_find_stt(l.from), st_args, txt));

	*r_char_offset = l.char_offset + l.char_count;

	l.offset.y = p_h;
	return _calculate_line_vertical_offset(l);
}

void RichTextLabel::_set_table_size(ItemTable *p_table, int p_available_width) {
	int col_count = p_table->columns.size();

	// Compute available width and total ratio (for expanders).
	int total_ratio = 0;
	int remaining_width = p_available_width;
	p_table->total_width = theme_cache.table_h_separation;

	for (int i = 0; i < col_count; i++) {
		remaining_width -= p_table->columns[i].min_width;
		if (p_table->columns[i].max_width > p_table->columns[i].min_width) {
			p_table->columns[i].expand = true;
		}
		if (p_table->columns[i].expand) {
			total_ratio += p_table->columns[i].expand_ratio;
		}
	}

	// Assign actual widths.
	for (int i = 0; i < col_count; i++) {
		p_table->columns[i].width = p_table->columns[i].min_width;
		if (p_table->columns[i].expand && total_ratio > 0 && remaining_width > 0) {
			p_table->columns[i].width += p_table->columns[i].expand_ratio * remaining_width / total_ratio;
		}
		if (i != col_count - 1) {
			p_table->total_width += p_table->columns[i].width + theme_cache.table_h_separation;
		} else {
			p_table->total_width += p_table->columns[i].width;
		}
	}

	// Resize to max_width if needed and distribute the remaining space.
	bool table_need_fit = true;
	while (table_need_fit) {
		table_need_fit = false;
		// Fit slim.
		for (int i = 0; i < col_count; i++) {
			if (!p_table->columns[i].expand || !p_table->columns[i].shrink) {
				continue;
			}
			int dif = p_table->columns[i].width - p_table->columns[i].max_width;
			if (dif > 0) {
				table_need_fit = true;
				p_table->columns[i].width = p_table->columns[i].max_width;
				p_table->total_width -= dif;
				total_ratio -= p_table->columns[i].expand_ratio;
			}
		}
		// Grow.
		remaining_width = p_available_width - p_table->total_width;
		if (remaining_width > 0 && total_ratio > 0) {
			for (int i = 0; i < col_count; i++) {
				if (p_table->columns[i].expand) {
					int dif = p_table->columns[i].max_width - p_table->columns[i].width;
					if (dif > 0) {
						int slice = p_table->columns[i].expand_ratio * remaining_width / total_ratio;
						int incr = MIN(dif, slice);
						p_table->columns[i].width += incr;
						p_table->total_width += incr;
					}
				}
			}
		}
	}

	// Update line width and get total height.
	int idx = 0;
	p_table->total_height = 0;
	p_table->rows.clear();
	p_table->rows_baseline.clear();

	Vector2 offset = Vector2(theme_cache.table_h_separation * 0.5, theme_cache.table_v_separation * 0.5).floor();
	float row_height = 0.0;

	for (const List<Item *>::Element *E = p_table->subitems.front(); E; E = E->next()) {
		ERR_CONTINUE(E->get()->type != ITEM_FRAME); // Children should all be frames.
		ItemFrame *frame = static_cast<ItemFrame *>(E->get());

		int column = idx % col_count;

		offset.x += frame->padding.position.x;
		float yofs = frame->padding.position.y;
		float prev_h = 0;
		float row_baseline = 0.0;
		for (int i = 0; i < (int)frame->lines.size(); i++) {
			MutexLock sub_lock(frame->lines[i].text_buf->get_mutex());

			frame->lines[i].text_buf->set_width(p_table->columns[column].width);
			p_table->columns[column].width = MAX(p_table->columns[column].width, ceil(frame->lines[i].text_buf->get_size().x));

			frame->lines[i].offset.y = prev_h;

			float h = frame->lines[i].text_buf->get_size().y + (frame->lines[i].text_buf->get_line_count() - 1) * theme_cache.line_separation;
			if (i > 0) {
				h += theme_cache.line_separation;
			}
			if (frame->min_size_over.y > 0) {
				h = MAX(h, frame->min_size_over.y);
			}
			if (frame->max_size_over.y > 0) {
				h = MIN(h, frame->max_size_over.y);
			}
			yofs += h;
			prev_h = frame->lines[i].offset.y + frame->lines[i].text_buf->get_size().y + frame->lines[i].text_buf->get_line_count() * theme_cache.line_separation;

			frame->lines[i].offset += offset;
			row_baseline = MAX(row_baseline, frame->lines[i].text_buf->get_line_ascent(frame->lines[i].text_buf->get_line_count() - 1));
		}
		yofs += frame->padding.size.y;
		offset.x += p_table->columns[column].width + theme_cache.table_h_separation + frame->padding.size.x;

		row_height = MAX(yofs, row_height);
		// Add row height after last column of the row or last cell of the table.
		if (column == col_count - 1 || E->next() == nullptr) {
			offset.x = Math::floor(theme_cache.table_h_separation * 0.5);
			row_height += theme_cache.table_v_separation;
			p_table->total_height += row_height;
			offset.y += row_height;
			p_table->rows.push_back(row_height);
			p_table->rows_baseline.push_back(p_table->total_height - row_height + row_baseline + Math::floor(theme_cache.table_v_separation * 0.5));
			row_height = 0;
		}
		idx++;
	}
}

int RichTextLabel::_draw_line(ItemFrame *p_frame, int p_line, const Vector2 &p_ofs, int p_width, float p_vsep, const Color &p_base_color, int p_outline_size, const Color &p_outline_color, const Color &p_font_shadow_color, int p_shadow_outline_size, const Point2 &p_shadow_ofs, int &r_processed_glyphs) {
	ERR_FAIL_NULL_V(p_frame, 0);
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)p_frame->lines.size(), 0);

	Vector2 off;

	Line &l = p_frame->lines[p_line];
	MutexLock lock(l.text_buf->get_mutex());

	Item *it_from = l.from;

	if (it_from == nullptr) {
		return 0;
	}

	RID ci = get_canvas_item();
	bool rtl = (l.text_buf->get_direction() == TextServer::DIRECTION_RTL);
	bool lrtl = is_layout_rtl();

	bool trim_chars = (visible_characters >= 0) && (visible_chars_behavior == TextServer::VC_CHARS_AFTER_SHAPING);
	bool trim_glyphs_ltr = (visible_characters >= 0) && ((visible_chars_behavior == TextServer::VC_GLYPHS_LTR) || ((visible_chars_behavior == TextServer::VC_GLYPHS_AUTO) && !lrtl));
	bool trim_glyphs_rtl = (visible_characters >= 0) && ((visible_chars_behavior == TextServer::VC_GLYPHS_RTL) || ((visible_chars_behavior == TextServer::VC_GLYPHS_AUTO) && lrtl));
	int total_glyphs = (trim_glyphs_ltr || trim_glyphs_rtl) ? get_total_glyph_count() : 0;
	int visible_glyphs = total_glyphs * visible_ratio;

	// Draw dropcap.
	int dc_lines = l.text_buf->get_dropcap_lines();
	float h_off = l.text_buf->get_dropcap_size().x;
	bool skip_dc = (trim_chars && l.char_offset > visible_characters) || (trim_glyphs_ltr && (r_processed_glyphs >= visible_glyphs)) || (trim_glyphs_rtl && (r_processed_glyphs < total_glyphs - visible_glyphs));
	if (!skip_dc) {
		if (l.dc_ol_size > 0) {
			l.text_buf->draw_dropcap_outline(ci, p_ofs + ((rtl) ? Vector2() : Vector2(l.offset.x, 0)), l.dc_ol_size, l.dc_ol_color);
		}
		l.text_buf->draw_dropcap(ci, p_ofs + ((rtl) ? Vector2() : Vector2(l.offset.x, 0)), l.dc_color);
	}

	int line_count = 0;
	bool has_visible_chars = false;
	// Bottom margin for text clipping.
	float v_limit = theme_cache.normal_style->get_margin(SIDE_BOTTOM);
	Size2 ctrl_size = get_size();
	// Draw text.
	for (int line = 0; line < l.text_buf->get_line_count(); line++) {
		if (line > 0) {
			off.y += (theme_cache.line_separation + p_vsep);
		}

		if (p_ofs.y + off.y >= ctrl_size.height - v_limit) {
			break;
		}

		double l_height = l.text_buf->get_line_ascent(line) + l.text_buf->get_line_descent(line);
		if (p_ofs.y + off.y + l_height <= 0) {
			off.y += l_height;
			continue;
		}

		float width = l.text_buf->get_width();
		float length = l.text_buf->get_line_size(line).x;

		// Draw line.
		if (rtl) {
			off.x = p_width - l.offset.x - width;
			if (!lrtl && p_frame == main) { // Skip Scrollbar.
				off.x -= scroll_w;
			}
		} else {
			off.x = l.offset.x;
			if (lrtl && p_frame == main) { // Skip Scrollbar.
				off.x += scroll_w;
			}
		}

		// Draw text.
		switch (l.text_buf->get_alignment()) {
			case HORIZONTAL_ALIGNMENT_FILL:
			case HORIZONTAL_ALIGNMENT_LEFT: {
				if (rtl) {
					off.x += width - length;
				}
			} break;
			case HORIZONTAL_ALIGNMENT_CENTER: {
				off.x += Math::floor((width - length) / 2.0);
			} break;
			case HORIZONTAL_ALIGNMENT_RIGHT: {
				if (!rtl) {
					off.x += width - length;
				}
			} break;
		}

		bool skip_prefix = (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING && l.char_offset == visible_characters) || (trim_chars && l.char_offset > visible_characters) || (trim_glyphs_ltr && (r_processed_glyphs >= visible_glyphs)) || (trim_glyphs_rtl && (r_processed_glyphs < total_glyphs - visible_glyphs));
		if (l.text_prefix.is_valid() && line == 0 && !skip_prefix) {
			Color font_color = _find_color(l.from, p_base_color);
			int outline_size = _find_outline_size(l.from, p_outline_size);
			Color font_outline_color = _find_outline_color(l.from, p_outline_color);
			Color font_shadow_color = p_font_shadow_color * Color(1, 1, 1, font_color.a);
			if (rtl) {
				if (p_shadow_outline_size > 0 && font_shadow_color.a != 0.0) {
					l.text_prefix->draw_outline(ci, p_ofs + Vector2(off.x + length, 0) + p_shadow_ofs, p_shadow_outline_size, font_shadow_color);
				}
				if (outline_size > 0 && font_outline_color.a != 0.0) {
					l.text_prefix->draw_outline(ci, p_ofs + Vector2(off.x + length, 0), outline_size, font_outline_color);
				}
				l.text_prefix->draw(ci, p_ofs + Vector2(off.x + length, 0), font_color);
			} else {
				if (p_shadow_outline_size > 0 && font_shadow_color.a != 0.0) {
					l.text_prefix->draw_outline(ci, p_ofs + Vector2(off.x - l.text_prefix->get_size().x, 0) + p_shadow_ofs, p_shadow_outline_size, font_shadow_color);
				}
				if (outline_size > 0 && font_outline_color.a != 0.0) {
					l.text_prefix->draw_outline(ci, p_ofs + Vector2(off.x - l.text_prefix->get_size().x, 0), outline_size, font_outline_color);
				}
				l.text_prefix->draw(ci, p_ofs + Vector2(off.x - l.text_prefix->get_size().x, 0), font_color);
			}
		}

		if (line <= dc_lines) {
			if (rtl) {
				off.x -= h_off;
			} else {
				off.x += h_off;
			}
		}

		RID rid = l.text_buf->get_line_rid(line);
		double l_ascent = TS->shaped_text_get_ascent(rid);
		Size2 l_size = TS->shaped_text_get_size(rid);
		double upos = TS->shaped_text_get_underline_position(rid);
		double uth = TS->shaped_text_get_underline_thickness(rid);

		off.y += l_ascent;

		const Glyph *glyphs = TS->shaped_text_get_glyphs(rid);
		int gl_size = TS->shaped_text_get_glyph_count(rid);
		Vector2i chr_range = TS->shaped_text_get_range(rid);

		int sel_start = -1;
		int sel_end = -1;

		if (selection.active && (selection.from_frame->lines[selection.from_line].char_offset + selection.from_char) <= (l.char_offset + chr_range.y) && (selection.to_frame->lines[selection.to_line].char_offset + selection.to_char) >= (l.char_offset + chr_range.x)) {
			sel_start = MAX(chr_range.x, (selection.from_frame->lines[selection.from_line].char_offset + selection.from_char) - l.char_offset);
			sel_end = MIN(chr_range.y, (selection.to_frame->lines[selection.to_line].char_offset + selection.to_char) - l.char_offset);
		}

		int processed_glyphs_step = 0;
		for (int step = DRAW_STEP_BACKGROUND; step < DRAW_STEP_MAX; step++) {
			if (step == DRAW_STEP_TEXT) {
				// Draw inlined objects.
				Array objects = TS->shaped_text_get_objects(rid);
				for (int i = 0; i < objects.size(); i++) {
					Item *it = items.get_or_null(objects[i]);
					if (it != nullptr) {
						Vector2i obj_range = TS->shaped_text_get_object_range(rid, objects[i]);
						if (trim_chars && l.char_offset + obj_range.y > visible_characters) {
							continue;
						}
						if (trim_glyphs_ltr || trim_glyphs_rtl) {
							int obj_glyph = r_processed_glyphs + TS->shaped_text_get_object_glyph(rid, objects[i]);
							if ((trim_glyphs_ltr && (obj_glyph >= visible_glyphs)) || (trim_glyphs_rtl && (obj_glyph < total_glyphs - visible_glyphs))) {
								continue;
							}
						}
						Rect2 rect = TS->shaped_text_get_object_rect(rid, objects[i]);
						switch (it->type) {
							case ITEM_IMAGE: {
								ItemImage *img = static_cast<ItemImage *>(it);
								if (img->pad) {
									Size2 pad_size = rect.size.min(img->image->get_size());
									Vector2 pad_off = (rect.size - pad_size) / 2;
									img->image->draw_rect(ci, Rect2(p_ofs + rect.position + off + pad_off, pad_size), false, img->color);
								} else {
									img->image->draw_rect(ci, Rect2(p_ofs + rect.position + off, rect.size), false, img->color);
								}
							} break;
							case ITEM_TABLE: {
								ItemTable *table = static_cast<ItemTable *>(it);
								Color odd_row_bg = theme_cache.table_odd_row_bg;
								Color even_row_bg = theme_cache.table_even_row_bg;
								Color border = theme_cache.table_border;
								float h_separation = theme_cache.table_h_separation;
								float v_separation = theme_cache.table_v_separation;

								int col_count = table->columns.size();
								int row_count = table->rows.size();

								int idx = 0;
								for (Item *E : table->subitems) {
									ItemFrame *frame = static_cast<ItemFrame *>(E);

									int col = idx % col_count;
									int row = idx / col_count;

									if (frame->lines.size() != 0 && row < row_count) {
										Vector2 coff = frame->lines[0].offset;
										coff.x -= frame->lines[0].indent;
										if (rtl) {
											coff.x = rect.size.width - table->columns[col].width - coff.x;
										}
										if (row % 2 == 0) {
											Color c = frame->odd_row_bg != Color(0, 0, 0, 0) ? frame->odd_row_bg : odd_row_bg;
											if (c.a > 0.0) {
												draw_rect(Rect2(p_ofs + rect.position + off + coff - frame->padding.position - Vector2(h_separation * 0.5, v_separation * 0.5).floor(), Size2(table->columns[col].width + h_separation + frame->padding.position.x + frame->padding.size.x, table->rows[row])), c, true);
											}
										} else {
											Color c = frame->even_row_bg != Color(0, 0, 0, 0) ? frame->even_row_bg : even_row_bg;
											if (c.a > 0.0) {
												draw_rect(Rect2(p_ofs + rect.position + off + coff - frame->padding.position - Vector2(h_separation * 0.5, v_separation * 0.5).floor(), Size2(table->columns[col].width + h_separation + frame->padding.position.x + frame->padding.size.x, table->rows[row])), c, true);
											}
										}
										Color bc = frame->border != Color(0, 0, 0, 0) ? frame->border : border;
										if (bc.a > 0.0) {
											draw_rect(Rect2(p_ofs + rect.position + off + coff - frame->padding.position - Vector2(h_separation * 0.5, v_separation * 0.5).floor(), Size2(table->columns[col].width + h_separation + frame->padding.position.x + frame->padding.size.x, table->rows[row])), bc, false);
										}
									}

									for (int j = 0; j < (int)frame->lines.size(); j++) {
										_draw_line(frame, j, p_ofs + rect.position + off + Vector2(0, frame->lines[j].offset.y), rect.size.x, 0, p_base_color, p_outline_size, p_outline_color, p_font_shadow_color, p_shadow_outline_size, p_shadow_ofs, r_processed_glyphs);
									}
									idx++;
								}
							} break;
							default:
								break;
						}
					}
				}
			}
			Vector2 off_step = off;
			processed_glyphs_step = r_processed_glyphs;

			Vector2 ul_start;
			bool ul_started = false;
			Color ul_color_prev;
			Color ul_color;

			Vector2 dot_ul_start;
			bool dot_ul_started = false;
			Color dot_ul_color_prev;
			Color dot_ul_color;

			Vector2 st_start;
			bool st_started = false;
			Color st_color_prev;
			Color st_color;

			float box_start = 0.0;
			Color last_color = Color(0, 0, 0, 0);

			Item *it = it_from;
			int span = -1;
			for (int i = 0; i < gl_size; i++) {
				bool selected = selection.active && (sel_start != -1) && (glyphs[i].start >= sel_start) && (glyphs[i].end <= sel_end);
				if (glyphs[i].span_index != span) {
					span = glyphs[i].span_index;
					if (span >= 0) {
						if ((glyphs[i].flags & TextServer::GRAPHEME_IS_EMBEDDED_OBJECT) == TextServer::GRAPHEME_IS_EMBEDDED_OBJECT) {
							Item *new_it = items.get_or_null(TS->shaped_get_span_embedded_object(rid, span));
							if (new_it) {
								it = new_it;
							}
						} else {
							Item *new_it = items.get_or_null(TS->shaped_get_span_meta(rid, span));
							if (new_it) {
								it = new_it;
							}
						}
					}
				}

				Color font_color = (step == DRAW_STEP_SHADOW || step == DRAW_STEP_OUTLINE || step == DRAW_STEP_TEXT) ? _find_color(it, p_base_color) : Color();
				int outline_size = (step == DRAW_STEP_OUTLINE) ? _find_outline_size(it, p_outline_size) : 0;
				Color font_outline_color = (step == DRAW_STEP_OUTLINE) ? _find_outline_color(it, p_outline_color) : Color();
				Color font_shadow_color = p_font_shadow_color;
				bool txt_visible = false;
				if (step == DRAW_STEP_OUTLINE) {
					txt_visible = (font_outline_color.a != 0 && outline_size > 0);
				} else if (step == DRAW_STEP_SHADOW) {
					txt_visible = (font_shadow_color.a != 0);
				} else if (step == DRAW_STEP_TEXT) {
					txt_visible = (font_color.a != 0);
					bool has_ul = _find_underline(it);
					if (!has_ul && underline_meta) {
						ItemMeta *meta = nullptr;
						if (_find_meta(it, nullptr, &meta) && meta) {
							switch (meta->underline) {
								case META_UNDERLINE_ALWAYS: {
									has_ul = true;
								} break;
								case META_UNDERLINE_NEVER: {
									has_ul = false;
								} break;
								case META_UNDERLINE_ON_HOVER: {
									has_ul = (meta == meta_hovering);
								} break;
							}
						}
					}
					if (has_ul) {
						if (ul_started && font_color != ul_color_prev) {
							float y_off = upos;
							float underline_width = MAX(1.0, uth * theme_cache.base_scale);
							draw_line(ul_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), ul_color, underline_width);
							ul_start = p_ofs + Vector2(off_step.x, off_step.y);
							ul_color_prev = font_color;
							ul_color = font_color;
							ul_color.a *= 0.5;
						} else if (!ul_started) {
							ul_started = true;
							ul_start = p_ofs + Vector2(off_step.x, off_step.y);
							ul_color_prev = font_color;
							ul_color = font_color;
							ul_color.a *= 0.5;
						}
					} else if (ul_started) {
						ul_started = false;
						float y_off = upos;
						float underline_width = MAX(1.0, uth * theme_cache.base_scale);
						draw_line(ul_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), ul_color, underline_width);
					}
					if (_find_hint(it, nullptr) && underline_hint) {
						if (dot_ul_started && font_color != dot_ul_color_prev) {
							float y_off = upos;
							float underline_width = MAX(1.0, uth * theme_cache.base_scale);
							draw_dashed_line(dot_ul_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), dot_ul_color, underline_width, MAX(2.0, underline_width * 2));
							dot_ul_start = p_ofs + Vector2(off_step.x, off_step.y);
							dot_ul_color_prev = font_color;
							dot_ul_color = font_color;
							dot_ul_color.a *= 0.5;
						} else if (!dot_ul_started) {
							dot_ul_started = true;
							dot_ul_start = p_ofs + Vector2(off_step.x, off_step.y);
							dot_ul_color_prev = font_color;
							dot_ul_color = font_color;
							dot_ul_color.a *= 0.5;
						}
					} else if (dot_ul_started) {
						dot_ul_started = false;
						float y_off = upos;
						float underline_width = MAX(1.0, uth * theme_cache.base_scale);
						draw_dashed_line(dot_ul_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), dot_ul_color, underline_width, MAX(2.0, underline_width * 2));
					}
					if (_find_strikethrough(it)) {
						if (st_started && font_color != st_color_prev) {
							float y_off = -l_ascent + l_size.y / 2;
							float underline_width = MAX(1.0, uth * theme_cache.base_scale);
							draw_line(st_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), st_color, underline_width);
							st_start = p_ofs + Vector2(off_step.x, off_step.y);
							st_color_prev = font_color;
							st_color = font_color;
							st_color.a *= 0.5;
						} else if (!st_started) {
							st_started = true;
							st_start = p_ofs + Vector2(off_step.x, off_step.y);
							st_color_prev = font_color;
							st_color = font_color;
							st_color.a *= 0.5;
						}
					} else if (st_started) {
						st_started = false;
						float y_off = -l_ascent + l_size.y / 2;
						float underline_width = MAX(1.0, uth * theme_cache.base_scale);
						draw_line(st_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), st_color, underline_width);
					}
				}
				if (step == DRAW_STEP_SHADOW || step == DRAW_STEP_OUTLINE || step == DRAW_STEP_TEXT) {
					ItemFade *fade = nullptr;
					Item *fade_item = it;
					while (fade_item) {
						if (fade_item->type == ITEM_FADE) {
							fade = static_cast<ItemFade *>(fade_item);
							break;
						}
						fade_item = fade_item->parent;
					}

					Vector<ItemFX *> fx_stack;
					_fetch_item_fx_stack(it, fx_stack);
					bool custom_fx_ok = true;

					Point2 fx_offset = Vector2(glyphs[i].x_off, glyphs[i].y_off);
					RID frid = glyphs[i].font_rid;
					uint32_t gl = glyphs[i].index;
					uint16_t gl_fl = glyphs[i].flags;
					uint8_t gl_cn = glyphs[i].count;
					bool cprev_cluster = false;
					bool cprev_conn = false;
					if (gl_cn == 0) { // Parts of the same grapheme cluster, always connected.
						cprev_cluster = true;
					}
					if (gl_fl & TextServer::GRAPHEME_IS_RTL) { // Check if previous grapheme cluster is connected.
						if (i > 0 && (glyphs[i - 1].flags & TextServer::GRAPHEME_IS_CONNECTED)) {
							cprev_conn = true;
						}
					} else {
						if (glyphs[i].flags & TextServer::GRAPHEME_IS_CONNECTED) {
							cprev_conn = true;
						}
					}

					//Apply fx.
					if (fade) {
						float faded_visibility = 1.0f;
						if (l.char_offset + glyphs[i].start >= fade->char_ofs + fade->starting_index) {
							faded_visibility -= (float)((l.char_offset + glyphs[i].start) - (fade->char_ofs + fade->starting_index)) / (float)fade->length;
							faded_visibility = faded_visibility < 0.0f ? 0.0f : faded_visibility;
						}
						font_color.a = faded_visibility;
					}

					Transform2D char_xform;
					char_xform.set_origin(p_ofs + off_step);

					for (int j = 0; j < fx_stack.size(); j++) {
						ItemFX *item_fx = fx_stack[j];
						bool cn = cprev_cluster || (cprev_conn && item_fx->connected);

						if (item_fx->type == ITEM_CUSTOMFX && custom_fx_ok) {
							ItemCustomFX *item_custom = static_cast<ItemCustomFX *>(item_fx);

							Ref<CharFXTransform> charfx = item_custom->char_fx_transform;
							Ref<RichTextEffect> custom_effect = item_custom->custom_effect;

							if (custom_effect.is_valid()) {
								charfx->elapsed_time = item_custom->elapsed_time;
								charfx->range = Vector2i(l.char_offset + glyphs[i].start, l.char_offset + glyphs[i].end);
								charfx->relative_index = l.char_offset + glyphs[i].start - item_fx->char_ofs;
								charfx->visibility = txt_visible;
								charfx->outline = (step == DRAW_STEP_SHADOW) || (step == DRAW_STEP_OUTLINE);
								charfx->font = frid;
								charfx->glyph_index = gl;
								charfx->glyph_flags = gl_fl;
								charfx->glyph_count = gl_cn;
								charfx->offset = fx_offset;
								charfx->color = font_color;
								charfx->transform = char_xform;

								bool effect_status = custom_effect->_process_effect_impl(charfx);
								custom_fx_ok = effect_status;

								char_xform = charfx->transform;
								fx_offset = charfx->offset;
								font_color = charfx->color;
								frid = charfx->font;
								gl = charfx->glyph_index;
								txt_visible &= charfx->visibility;
							}
						} else if (item_fx->type == ITEM_SHAKE) {
							ItemShake *item_shake = static_cast<ItemShake *>(item_fx);

							if (!cn) {
								uint64_t char_current_rand = item_shake->offset_random(glyphs[i].start);
								uint64_t char_previous_rand = item_shake->offset_previous_random(glyphs[i].start);
								uint64_t max_rand = 2147483647;
								double current_offset = Math::remap(char_current_rand % max_rand, 0, max_rand, 0.0f, 2.f * (float)Math_PI);
								double previous_offset = Math::remap(char_previous_rand % max_rand, 0, max_rand, 0.0f, 2.f * (float)Math_PI);
								double n_time = (double)(item_shake->elapsed_time / (0.5f / item_shake->rate));
								n_time = (n_time > 1.0) ? 1.0 : n_time;
								item_shake->prev_off = Point2(Math::lerp(Math::sin(previous_offset), Math::sin(current_offset), n_time), Math::lerp(Math::cos(previous_offset), Math::cos(current_offset), n_time)) * (float)item_shake->strength / 10.0f;
							}
							fx_offset += item_shake->prev_off;
						} else if (item_fx->type == ITEM_WAVE) {
							ItemWave *item_wave = static_cast<ItemWave *>(item_fx);

							if (!cn) {
								double value = Math::sin(item_wave->frequency * item_wave->elapsed_time + ((p_ofs.x + off_step.x) / 50)) * (item_wave->amplitude / 10.0f);
								item_wave->prev_off = Point2(0, 1) * value;
							}
							fx_offset += item_wave->prev_off;
						} else if (item_fx->type == ITEM_TORNADO) {
							ItemTornado *item_tornado = static_cast<ItemTornado *>(item_fx);

							if (!cn) {
								double torn_x = Math::sin(item_tornado->frequency * item_tornado->elapsed_time + ((p_ofs.x + off_step.x) / 50)) * (item_tornado->radius);
								double torn_y = Math::cos(item_tornado->frequency * item_tornado->elapsed_time + ((p_ofs.x + off_step.x) / 50)) * (item_tornado->radius);
								item_tornado->prev_off = Point2(torn_x, torn_y);
							}
							fx_offset += item_tornado->prev_off;
						} else if (item_fx->type == ITEM_RAINBOW) {
							ItemRainbow *item_rainbow = static_cast<ItemRainbow *>(item_fx);

							font_color = font_color.from_hsv(MAX(item_rainbow->frequency, 0) * ABS(item_rainbow->elapsed_time * item_rainbow->speed + ((p_ofs.x + off_step.x) / 50)), item_rainbow->saturation, item_rainbow->value, font_color.a);
						} else if (item_fx->type == ITEM_PULSE) {
							ItemPulse *item_pulse = static_cast<ItemPulse *>(item_fx);

							const float sined_time = (Math::ease(Math::pingpong(item_pulse->elapsed_time, 1.0 / item_pulse->frequency) * item_pulse->frequency, item_pulse->ease));
							font_color = font_color.lerp(font_color * item_pulse->color, sined_time);
						}
					}

					if (is_inside_tree() && get_viewport()->is_snap_2d_transforms_to_pixel_enabled()) {
						fx_offset = (fx_offset + Point2(0.5, 0.5)).floor();
					}

					Vector2 char_off = char_xform.get_origin();
					Transform2D char_reverse_xform;
					if (step == DRAW_STEP_TEXT) {
						if (selected && use_selected_font_color) {
							font_color = theme_cache.font_selected_color;
						}

						char_reverse_xform.set_origin(-char_off);
						Transform2D char_final_xform = char_xform * char_reverse_xform;
						draw_set_transform_matrix(char_final_xform);
					} else if (step == DRAW_STEP_SHADOW) {
						font_color = font_shadow_color * Color(1, 1, 1, font_color.a);

						char_reverse_xform.set_origin(-char_off - p_shadow_ofs);
						Transform2D char_final_xform = char_xform * char_reverse_xform;
						char_final_xform.columns[2] += p_shadow_ofs;
						draw_set_transform_matrix(char_final_xform);
					} else if (step == DRAW_STEP_OUTLINE) {
						font_color = font_outline_color * Color(1, 1, 1, font_color.a);

						char_reverse_xform.set_origin(-char_off);
						Transform2D char_final_xform = char_xform * char_reverse_xform;
						draw_set_transform_matrix(char_final_xform);
					}

					// Draw glyphs.
					for (int j = 0; j < glyphs[i].repeat; j++) {
						bool skip = (trim_chars && l.char_offset + glyphs[i].end > visible_characters) || (trim_glyphs_ltr && (processed_glyphs_step >= visible_glyphs)) || (trim_glyphs_rtl && (processed_glyphs_step < total_glyphs - visible_glyphs));
						if (!skip) {
							if (txt_visible) {
								has_visible_chars = true;
								if (step == DRAW_STEP_TEXT) {
									if (frid != RID()) {
										TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color);
									} else if (((glyphs[i].flags & TextServer::GRAPHEME_IS_VIRTUAL) != TextServer::GRAPHEME_IS_VIRTUAL) && ((glyphs[i].flags & TextServer::GRAPHEME_IS_EMBEDDED_OBJECT) != TextServer::GRAPHEME_IS_EMBEDDED_OBJECT)) {
										TS->draw_hex_code_box(ci, glyphs[i].font_size, fx_offset + char_off, gl, font_color);
									}
								} else if (step == DRAW_STEP_SHADOW && frid != RID()) {
									TS->font_draw_glyph(frid, ci, glyphs[i].font_size, fx_offset + char_off + p_shadow_ofs, gl, font_color);
									if (p_shadow_outline_size > 0) {
										TS->font_draw_glyph_outline(frid, ci, glyphs[i].font_size, p_shadow_outline_size, fx_offset + char_off + p_shadow_ofs, gl, font_color);
									}
								} else if (step == DRAW_STEP_OUTLINE && frid != RID() && outline_size > 0) {
									TS->font_draw_glyph_outline(frid, ci, glyphs[i].font_size, outline_size, fx_offset + char_off, gl, font_color);
								}
							}
							processed_glyphs_step++;
						}
						if (step == DRAW_STEP_TEXT && skip) {
							// Finish underline/overline/strikethrough is previous glyph is skipped.
							if (ul_started) {
								ul_started = false;
								float y_off = upos;
								float underline_width = MAX(1.0, uth * theme_cache.base_scale);
								draw_line(ul_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), ul_color, underline_width);
							}
							if (dot_ul_started) {
								dot_ul_started = false;
								float y_off = upos;
								float underline_width = MAX(1.0, uth * theme_cache.base_scale);
								draw_dashed_line(dot_ul_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), dot_ul_color, underline_width, MAX(2.0, underline_width * 2));
							}
							if (st_started) {
								st_started = false;
								float y_off = -l_ascent + l_size.y / 2;
								float underline_width = MAX(1.0, uth * theme_cache.base_scale);
								draw_line(st_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), st_color, underline_width);
							}
						}
						off_step.x += glyphs[i].advance;
					}
					draw_set_transform_matrix(Transform2D());
				}
				// Draw boxes.
				if (step == DRAW_STEP_BACKGROUND || step == DRAW_STEP_FOREGROUND) {
					for (int j = 0; j < glyphs[i].repeat; j++) {
						bool skip = (trim_chars && l.char_offset + glyphs[i].end > visible_characters) || (trim_glyphs_ltr && (processed_glyphs_step >= visible_glyphs)) || (trim_glyphs_rtl && (processed_glyphs_step < total_glyphs - visible_glyphs));
						if (!skip) {
							Color color;
							if (step == DRAW_STEP_BACKGROUND) {
								color = _find_bgcolor(it);
							} else if (step == DRAW_STEP_FOREGROUND) {
								color = _find_fgcolor(it);
							}
							if (color != last_color) {
								if (last_color.a > 0.0) {
									Vector2 rect_off = p_ofs + Vector2(box_start - theme_cache.text_highlight_h_padding, off_step.y - l_ascent - theme_cache.text_highlight_v_padding);
									Vector2 rect_size = Vector2(off_step.x - box_start + 2 * theme_cache.text_highlight_h_padding, l_size.y + 2 * theme_cache.text_highlight_v_padding);
									RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(rect_off, rect_size), last_color);
								}
								if (color.a > 0.0) {
									box_start = off_step.x;
								}
							}
							last_color = color;
							processed_glyphs_step++;
						} else {
							// Finish box is previous glyph is skipped.
							if (last_color.a > 0.0) {
								Vector2 rect_off = p_ofs + Vector2(box_start - theme_cache.text_highlight_h_padding, off_step.y - l_ascent - theme_cache.text_highlight_v_padding);
								Vector2 rect_size = Vector2(off_step.x - box_start + 2 * theme_cache.text_highlight_h_padding, l_size.y + 2 * theme_cache.text_highlight_v_padding);
								RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(rect_off, rect_size), last_color);
							}
							last_color = Color(0, 0, 0, 0);
						}
						off_step.x += glyphs[i].advance;
					}
				}
			}
			// Finish lines and boxes.
			if (step == DRAW_STEP_BACKGROUND || step == DRAW_STEP_FOREGROUND) {
				if (last_color.a > 0.0) {
					Vector2 rect_off = p_ofs + Vector2(box_start - theme_cache.text_highlight_h_padding, off_step.y - l_ascent - theme_cache.text_highlight_v_padding);
					Vector2 rect_size = Vector2(off_step.x - box_start + 2 * theme_cache.text_highlight_h_padding, l_size.y + 2 * theme_cache.text_highlight_v_padding);
					RenderingServer::get_singleton()->canvas_item_add_rect(ci, Rect2(rect_off, rect_size), last_color);
				}
			}
			if (step == DRAW_STEP_BACKGROUND) {
				if (sel_start != -1) {
					Color selection_bg = theme_cache.selection_color;
					Vector<Vector2> sel = TS->shaped_text_get_selection(rid, sel_start, sel_end);
					for (int i = 0; i < sel.size(); i++) {
						Rect2 rect = Rect2(sel[i].x + p_ofs.x + off.x, p_ofs.y + off.y - l_ascent, sel[i].y - sel[i].x, l_size.y); // Note: use "off" not "off_step", selection is relative to the line start.
						RenderingServer::get_singleton()->canvas_item_add_rect(ci, rect, selection_bg);
					}
				}
			}
			if (step == DRAW_STEP_TEXT) {
				if (ul_started) {
					ul_started = false;
					float y_off = upos;
					float underline_width = MAX(1.0, uth * theme_cache.base_scale);
					draw_line(ul_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), ul_color, underline_width);
				}
				if (dot_ul_started) {
					dot_ul_started = false;
					float y_off = upos;
					float underline_width = MAX(1.0, uth * theme_cache.base_scale);
					draw_dashed_line(dot_ul_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), dot_ul_color, underline_width, MAX(2.0, underline_width * 2));
				}
				if (st_started) {
					st_started = false;
					float y_off = -l_ascent + l_size.y / 2;
					float underline_width = MAX(1.0, uth * theme_cache.base_scale);
					draw_line(st_start + Vector2(0, y_off), p_ofs + Vector2(off_step.x, off_step.y + y_off), st_color, underline_width);
				}
			}
		}

		r_processed_glyphs = processed_glyphs_step;
		off.y += TS->shaped_text_get_descent(rid);
		if (has_visible_chars) {
			line_count++;
			has_visible_chars = false;
		}
	}

	return line_count;
}

void RichTextLabel::_find_click(ItemFrame *p_frame, const Point2i &p_click, ItemFrame **r_click_frame, int *r_click_line, Item **r_click_item, int *r_click_char, bool *r_outside, bool p_meta) {
	if (r_click_item) {
		*r_click_item = nullptr;
	}
	if (r_click_char != nullptr) {
		*r_click_char = 0;
	}
	if (r_outside != nullptr) {
		*r_outside = true;
	}

	Size2 size = get_size();
	Rect2 text_rect = _get_text_rect();

	int vofs = vscroll->get_value();

	// Search for the first line.
	int to_line = main->first_invalid_line.load();
	int from_line = _find_first_line(0, to_line, vofs);

	int total_height = INT32_MAX;
	if (to_line && vertical_alignment != VERTICAL_ALIGNMENT_TOP) {
		MutexLock lock(main->lines[to_line - 1].text_buf->get_mutex());
		if (theme_cache.line_separation < 0) {
			// Do not apply to the last line to avoid cutting text.
			total_height = main->lines[to_line - 1].offset.y + main->lines[to_line - 1].text_buf->get_size().y + (main->lines[to_line - 1].text_buf->get_line_count() - 1) * theme_cache.line_separation;
		} else {
			total_height = main->lines[to_line - 1].offset.y + main->lines[to_line - 1].text_buf->get_size().y + main->lines[to_line - 1].text_buf->get_line_count() * theme_cache.line_separation;
		}
	}
	float vbegin = 0, vsep = 0;
	if (text_rect.size.y > total_height) {
		switch (vertical_alignment) {
			case VERTICAL_ALIGNMENT_TOP: {
				// Nothing.
			} break;
			case VERTICAL_ALIGNMENT_CENTER: {
				vbegin = (text_rect.size.y - total_height) / 2;
			} break;
			case VERTICAL_ALIGNMENT_BOTTOM: {
				vbegin = text_rect.size.y - total_height;
			} break;
			case VERTICAL_ALIGNMENT_FILL: {
				int lines = 0;
				for (int l = from_line; l < to_line; l++) {
					MutexLock lock(main->lines[l].text_buf->get_mutex());
					lines += main->lines[l].text_buf->get_line_count();
				}
				if (lines > 1) {
					vsep = (text_rect.size.y - total_height) / (lines - 1);
				}
			} break;
		}
	}

	Point2 ofs = text_rect.get_position() + Vector2(0, vbegin + main->lines[from_line].offset.y - vofs);
	while (ofs.y < size.height && from_line < to_line) {
		MutexLock lock(main->lines[from_line].text_buf->get_mutex());
		_find_click_in_line(p_frame, from_line, ofs, text_rect.size.x, vsep, p_click, r_click_frame, r_click_line, r_click_item, r_click_char, false, p_meta);
		ofs.y += main->lines[from_line].text_buf->get_size().y + main->lines[from_line].text_buf->get_line_count() * (theme_cache.line_separation + vsep);
		if (((r_click_item != nullptr) && ((*r_click_item) != nullptr)) || ((r_click_frame != nullptr) && ((*r_click_frame) != nullptr))) {
			if (r_outside != nullptr) {
				*r_outside = false;
			}
			return;
		}
		from_line++;
	}
}

float RichTextLabel::_find_click_in_line(ItemFrame *p_frame, int p_line, const Vector2 &p_ofs, int p_width, float p_vsep, const Point2i &p_click, ItemFrame **r_click_frame, int *r_click_line, Item **r_click_item, int *r_click_char, bool p_table, bool p_meta) {
	Vector2 off;

	bool line_clicked = false;
	float text_rect_begin = 0.0;
	int char_pos = -1;
	bool char_clicked = false;
	Line &l = p_frame->lines[p_line];
	MutexLock lock(l.text_buf->get_mutex());

	bool rtl = (l.text_buf->get_direction() == TextServer::DIRECTION_RTL);
	bool lrtl = is_layout_rtl();

	// Table hit test results.
	bool table_hit = false;
	Vector2i table_range;
	float table_offy = 0.f;
	ItemFrame *table_click_frame = nullptr;
	int table_click_line = -1;
	Item *table_click_item = nullptr;
	int table_click_char = -1;

	for (int line = 0; line < l.text_buf->get_line_count(); line++) {
		RID rid = l.text_buf->get_line_rid(line);

		float width = l.text_buf->get_width();
		float length = TS->shaped_text_get_width(rid);

		if (rtl) {
			off.x = p_width - l.offset.x - width;
			if (!lrtl && p_frame == main) { // Skip Scrollbar.
				off.x -= scroll_w;
			}
		} else {
			off.x = l.offset.x;
			if (lrtl && p_frame == main) { // Skip Scrollbar.
				off.x += scroll_w;
			}
		}

		switch (l.text_buf->get_alignment()) {
			case HORIZONTAL_ALIGNMENT_FILL:
			case HORIZONTAL_ALIGNMENT_LEFT: {
				if (rtl) {
					off.x += width - length;
				}
			} break;
			case HORIZONTAL_ALIGNMENT_CENTER: {
				off.x += Math::floor((width - length) / 2.0);
			} break;
			case HORIZONTAL_ALIGNMENT_RIGHT: {
				if (!rtl) {
					off.x += width - length;
				}
			} break;
		}
		// Adjust for dropcap.
		int dc_lines = l.text_buf->get_dropcap_lines();
		float h_off = l.text_buf->get_dropcap_size().x;
		if (line <= dc_lines) {
			if (rtl) {
				off.x -= h_off;
			} else {
				off.x += h_off;
			}
		}
		off.y += TS->shaped_text_get_ascent(rid);

		Array objects = TS->shaped_text_get_objects(rid);
		for (int i = 0; i < objects.size(); i++) {
			Item *it = items.get_or_null(objects[i]);
			if (it != nullptr) {
				Rect2 rect = TS->shaped_text_get_object_rect(rid, objects[i]);
				rect.position += p_ofs + off;
				if (p_click.y >= rect.position.y && p_click.y <= rect.position.y + rect.size.y) {
					switch (it->type) {
						case ITEM_TABLE: {
							ItemTable *table = static_cast<ItemTable *>(it);

							int idx = 0;
							int col_count = table->columns.size();
							int row_count = table->rows.size();

							for (Item *E : table->subitems) {
								ItemFrame *frame = static_cast<ItemFrame *>(E);

								int col = idx % col_count;
								int row = idx / col_count;

								if (frame->lines.size() != 0 && row < row_count) {
									Vector2 coff = frame->lines[0].offset;
									if (rtl) {
										coff.x = rect.size.width - table->columns[col].width - coff.x;
									}
									Rect2 crect = Rect2(rect.position + coff - frame->padding.position - Vector2(theme_cache.table_h_separation * 0.5, theme_cache.table_h_separation * 0.5).floor(), Size2(table->columns[col].width + theme_cache.table_h_separation, table->rows[row] + theme_cache.table_v_separation) + frame->padding.position + frame->padding.size);
									if (col == col_count - 1) {
										if (rtl) {
											crect.size.x = crect.position.x + crect.size.x;
											crect.position.x = 0;
										} else {
											crect.size.x = get_size().x;
										}
									}
									if (crect.has_point(p_click)) {
										for (int j = 0; j < (int)frame->lines.size(); j++) {
											_find_click_in_line(frame, j, rect.position + Vector2(frame->padding.position.x, frame->lines[j].offset.y), rect.size.x, 0, p_click, &table_click_frame, &table_click_line, &table_click_item, &table_click_char, true, p_meta);
											if (table_click_frame && table_click_item) {
												// Save cell detected cell hit data.
												table_range = Vector2i(INT32_MAX, 0);
												for (Item *F : table->subitems) {
													ItemFrame *sub_frame = static_cast<ItemFrame *>(F);
													for (int k = 0; k < (int)sub_frame->lines.size(); k++) {
														table_range.x = MIN(table_range.x, sub_frame->lines[k].char_offset);
														table_range.y = MAX(table_range.y, sub_frame->lines[k].char_offset + sub_frame->lines[k].char_count);
													}
												}
												table_offy = off.y;
												table_hit = true;
											}
										}
									}
								}
								idx++;
							}
						} break;
						default:
							break;
					}
				}
			}
		}
		Rect2 rect = Rect2(p_ofs + off - Vector2(0, TS->shaped_text_get_ascent(rid)) - p_frame->padding.position, TS->shaped_text_get_size(rid) + p_frame->padding.position + p_frame->padding.size);
		if (p_table) {
			rect.size.y += theme_cache.table_v_separation;
		}

		if (p_click.y >= rect.position.y && p_click.y <= rect.position.y + rect.size.y) {
			if (!p_meta) {
				char_pos = rtl ? TS->shaped_text_get_range(rid).y : TS->shaped_text_get_range(rid).x;
			}
			if ((!rtl && p_click.x >= rect.position.x) || (rtl && p_click.x <= rect.position.x + rect.size.x)) {
				if (p_meta) {
					int64_t glyph_idx = TS->shaped_text_hit_test_grapheme(rid, p_click.x - rect.position.x);
					if (glyph_idx >= 0) {
						float baseline_y = rect.position.y + TS->shaped_text_get_ascent(rid);
						const Glyph *glyphs = TS->shaped_text_get_glyphs(rid);
						if (glyphs[glyph_idx].flags & TextServer::GRAPHEME_IS_EMBEDDED_OBJECT) {
							// Emebedded object.
							for (int i = 0; i < objects.size(); i++) {
								if (TS->shaped_text_get_object_glyph(rid, objects[i]) == glyph_idx) {
									Rect2 obj_rect = TS->shaped_text_get_object_rect(rid, objects[i]);
									obj_rect.position.y += baseline_y;
									if (p_click.y >= obj_rect.position.y && p_click.y <= obj_rect.position.y + obj_rect.size.y) {
										char_pos = glyphs[glyph_idx].start;
										char_clicked = true;
									}
									break;
								}
							}
						} else if (glyphs[glyph_idx].font_rid != RID()) {
							// Normal glyph.
							float fa = TS->font_get_ascent(glyphs[glyph_idx].font_rid, glyphs[glyph_idx].font_size);
							float fd = TS->font_get_descent(glyphs[glyph_idx].font_rid, glyphs[glyph_idx].font_size);
							if (p_click.y >= baseline_y - fa && p_click.y <= baseline_y + fd) {
								char_pos = glyphs[glyph_idx].start;
								char_clicked = true;
							}
						} else if (!(glyphs[glyph_idx].flags & TextServer::GRAPHEME_IS_VIRTUAL)) {
							// Hex code box.
							Vector2 gl_size = TS->get_hex_code_box_size(glyphs[glyph_idx].font_size, glyphs[glyph_idx].index);
							if (p_click.y >= baseline_y - gl_size.y * 0.9 && p_click.y <= baseline_y + gl_size.y * 0.2) {
								char_pos = glyphs[glyph_idx].start;
								char_clicked = true;
							}
						}
					}
				} else {
					char_pos = TS->shaped_text_hit_test_position(rid, p_click.x - rect.position.x);
					char_pos = TS->shaped_text_closest_character_pos(rid, char_pos);
					char_clicked = true;
				}
			}
			line_clicked = true;
			text_rect_begin = rtl ? rect.position.x + rect.size.x : rect.position.x;
		}

		// If table hit was detected, and line hit is in the table bounds use table hit.
		if (table_hit && (((char_pos + p_frame->lines[p_line].char_offset) >= table_range.x && (char_pos + p_frame->lines[p_line].char_offset) <= table_range.y) || !char_clicked)) {
			if (r_click_frame != nullptr) {
				*r_click_frame = table_click_frame;
			}

			if (r_click_line != nullptr) {
				*r_click_line = table_click_line;
			}

			if (r_click_item != nullptr) {
				*r_click_item = table_click_item;
			}

			if (r_click_char != nullptr) {
				*r_click_char = table_click_char;
			}
			return table_offy;
		}

		off.y += TS->shaped_text_get_descent(rid) + theme_cache.line_separation + p_vsep;
	}

	// Text line hit.
	if (line_clicked) {
		// Find item.
		if (r_click_item != nullptr) {
			Item *it = p_frame->lines[p_line].from;
			Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
			if (char_pos >= 0) {
				*r_click_item = _get_item_at_pos(it, it_to, char_pos);
			} else {
				int stop = text_rect_begin;
				*r_click_item = _find_indentable(it);
				while (*r_click_item) {
					Ref<Font> font = theme_cache.normal_font;
					int font_size = theme_cache.normal_font_size;
					ItemFont *font_it = _find_font(*r_click_item);
					if (font_it) {
						if (font_it->font.is_valid()) {
							font = font_it->font;
						}
						if (font_it->font_size > 0) {
							font_size = font_it->font_size;
						}
					}
					ItemFontSize *font_size_it = _find_font_size(*r_click_item);
					if (font_size_it && font_size_it->font_size > 0) {
						font_size = font_size_it->font_size;
					}
					if (rtl) {
						stop += tab_size * font->get_char_size(' ', font_size).width;
						if (stop > p_click.x) {
							break;
						}
					} else {
						stop -= tab_size * font->get_char_size(' ', font_size).width;
						if (stop < p_click.x) {
							break;
						}
					}
					*r_click_item = _find_indentable((*r_click_item)->parent);
				}
			}
		}

		if (r_click_frame != nullptr) {
			*r_click_frame = p_frame;
		}

		if (r_click_line != nullptr) {
			*r_click_line = p_line;
		}

		if (r_click_char != nullptr) {
			*r_click_char = char_pos;
		}
	}

	return off.y;
}

void RichTextLabel::_scroll_changed(double) {
	if (updating_scroll) {
		return;
	}

	if (scroll_follow && vscroll->get_value() > (vscroll->get_max() - vscroll->get_page() - 1)) {
		scroll_following = true;
	} else {
		scroll_following = false;
	}

	scroll_updated = true;

	queue_redraw();
}

void RichTextLabel::_update_fx(RichTextLabel::ItemFrame *p_frame, double p_delta_time) {
	Item *it = p_frame;
	while (it) {
		ItemFX *ifx = nullptr;

		if (it->type == ITEM_CUSTOMFX || it->type == ITEM_SHAKE || it->type == ITEM_WAVE || it->type == ITEM_TORNADO || it->type == ITEM_RAINBOW || it->type == ITEM_PULSE) {
			ifx = static_cast<ItemFX *>(it);
		}

		if (!ifx) {
			it = _get_next_item(it, true);
			continue;
		}

		ifx->elapsed_time += p_delta_time;

		ItemShake *shake = nullptr;

		if (it->type == ITEM_SHAKE) {
			shake = static_cast<ItemShake *>(it);
		}

		if (shake) {
			bool cycle = (shake->elapsed_time > (1.0f / shake->rate));
			if (cycle) {
				shake->elapsed_time -= (1.0f / shake->rate);
				shake->reroll_random();
			}
		}

		it = _get_next_item(it, true);
	}
}

int RichTextLabel::_find_first_line(int p_from, int p_to, int p_vofs) const {
	int l = p_from;
	int r = p_to;
	while (l < r) {
		int m = Math::floor(double(l + r) / 2.0);
		MutexLock lock(main->lines[m].text_buf->get_mutex());
		int ofs = _calculate_line_vertical_offset(main->lines[m]);
		if (ofs < p_vofs) {
			l = m + 1;
		} else {
			r = m;
		}
	}
	return MIN(l, (int)main->lines.size() - 1);
}

_FORCE_INLINE_ float RichTextLabel::_calculate_line_vertical_offset(const RichTextLabel::Line &line) const {
	return line.get_height(theme_cache.line_separation);
}

void RichTextLabel::_update_theme_item_cache() {
	Control::_update_theme_item_cache();

	theme_cache.base_scale = get_theme_default_base_scale();
	use_selected_font_color = theme_cache.font_selected_color != Color(0, 0, 0, 0);
}

void RichTextLabel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_MOUSE_EXIT: {
			if (meta_hovering) {
				meta_hovering = nullptr;
				emit_signal(SNAME("meta_hover_ended"), current_meta);
				current_meta = false;
				queue_redraw();
			}
		} break;

		case NOTIFICATION_RESIZED: {
			_stop_thread();
			main->first_resized_line.store(0); // Invalidate all lines.
			queue_redraw();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_stop_thread();
			main->first_invalid_font_line.store(0); // Invalidate all lines.
			queue_redraw();
		} break;

		case NOTIFICATION_ENTER_TREE: {
			_stop_thread();
			if (!text.is_empty()) {
				set_text(text);
			}

			main->first_invalid_line.store(0); // Invalidate all lines.
			queue_redraw();
		} break;

		case NOTIFICATION_PREDELETE:
		case NOTIFICATION_EXIT_TREE: {
			_stop_thread();
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED: {
			if (!stack_externally_modified) {
				_apply_translation();
			}

			queue_redraw();
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (is_visible_in_tree()) {
				queue_redraw();
			}
		} break;

		case NOTIFICATION_DRAW: {
			RID ci = get_canvas_item();
			Size2 size = get_size();

			draw_style_box(theme_cache.normal_style, Rect2(Point2(), size));

			if (has_focus()) {
				RenderingServer::get_singleton()->canvas_item_add_clip_ignore(ci, true);
				draw_style_box(theme_cache.focus_style, Rect2(Point2(), size));
				RenderingServer::get_singleton()->canvas_item_add_clip_ignore(ci, false);
			}

			// Start text shaping.
			if (_validate_line_caches()) {
				set_physics_process_internal(false); // Disable auto refresh, if text is fully processed.
			} else {
				// Draw loading progress bar.
				if ((progress_delay > 0) && (OS::get_singleton()->get_ticks_msec() - loading_started >= (uint64_t)progress_delay)) {
					Vector2 p_size = Vector2(size.width - (theme_cache.normal_style->get_offset().x + vscroll->get_combined_minimum_size().width) * 2, vscroll->get_combined_minimum_size().width);
					Vector2 p_pos = Vector2(theme_cache.normal_style->get_offset().x, size.height - theme_cache.normal_style->get_offset().y - vscroll->get_combined_minimum_size().width);

					draw_style_box(theme_cache.progress_bg_style, Rect2(p_pos, p_size));

					bool right_to_left = is_layout_rtl();
					double r = loaded.load();
					int mp = theme_cache.progress_fg_style->get_minimum_size().width;
					int p = round(r * (p_size.width - mp));
					if (right_to_left) {
						int p_remaining = round((1.0 - r) * (p_size.width - mp));
						draw_style_box(theme_cache.progress_fg_style, Rect2(p_pos + Point2(p_remaining, 0), Size2(p + theme_cache.progress_fg_style->get_minimum_size().width, p_size.height)));
					} else {
						draw_style_box(theme_cache.progress_fg_style, Rect2(p_pos, Size2(p + theme_cache.progress_fg_style->get_minimum_size().width, p_size.height)));
					}
				}
			}

			// Draw main text.
			Rect2 text_rect = _get_text_rect();
			float vofs = vscroll->get_value();

			// Search for the first line.
			int to_line = main->first_invalid_line.load();
			int from_line = _find_first_line(0, to_line, vofs);

			// Bottom margin for text clipping.
			float v_limit = theme_cache.normal_style->get_margin(SIDE_BOTTOM);

			int total_height = INT32_MAX;
			if (to_line && vertical_alignment != VERTICAL_ALIGNMENT_TOP) {
				MutexLock lock(main->lines[to_line - 1].text_buf->get_mutex());
				if (theme_cache.line_separation < 0) {
					// Do not apply to the last line to avoid cutting text.
					total_height = main->lines[to_line - 1].offset.y + main->lines[to_line - 1].text_buf->get_size().y + (main->lines[to_line - 1].text_buf->get_line_count() - 1) * theme_cache.line_separation;
				} else {
					total_height = main->lines[to_line - 1].offset.y + main->lines[to_line - 1].text_buf->get_size().y + main->lines[to_line - 1].text_buf->get_line_count() * theme_cache.line_separation;
				}
			}
			float vbegin = 0, vsep = 0;
			if (text_rect.size.y > total_height) {
				switch (vertical_alignment) {
					case VERTICAL_ALIGNMENT_TOP: {
						// Nothing.
					} break;
					case VERTICAL_ALIGNMENT_CENTER: {
						vbegin = (text_rect.size.y - total_height) / 2;
					} break;
					case VERTICAL_ALIGNMENT_BOTTOM: {
						vbegin = text_rect.size.y - total_height;
					} break;
					case VERTICAL_ALIGNMENT_FILL: {
						int lines = 0;
						for (int l = from_line; l < to_line; l++) {
							MutexLock lock(main->lines[l].text_buf->get_mutex());
							lines += main->lines[l].text_buf->get_line_count();
						}
						if (lines > 1) {
							vsep = (text_rect.size.y - total_height) / (lines - 1);
						}
					} break;
				}
			}

			Point2 shadow_ofs(theme_cache.shadow_offset_x, theme_cache.shadow_offset_y);

			visible_paragraph_count = 0;
			visible_line_count = 0;

			// New cache draw.
			Point2 ofs = text_rect.get_position() + Vector2(0, vbegin + main->lines[from_line].offset.y - vofs);
			int processed_glyphs = 0;
			while (ofs.y < size.height - v_limit && from_line < to_line) {
				MutexLock lock(main->lines[from_line].text_buf->get_mutex());

				visible_paragraph_count++;
				visible_line_count += _draw_line(main, from_line, ofs, text_rect.size.x, vsep, theme_cache.default_color, theme_cache.outline_size, theme_cache.font_outline_color, theme_cache.font_shadow_color, theme_cache.shadow_outline_size, shadow_ofs, processed_glyphs);
				ofs.y += main->lines[from_line].text_buf->get_size().y + main->lines[from_line].text_buf->get_line_count() * (theme_cache.line_separation + vsep);
				from_line++;
			}
		} break;

		case NOTIFICATION_INTERNAL_PROCESS: {
			if (is_visible_in_tree()) {
				if (!is_finished()) {
					return;
				}
				double dt = get_process_delta_time();
				_update_fx(main, dt);
				queue_redraw();
			}
		} break;

		case NOTIFICATION_FOCUS_EXIT: {
			if (deselect_on_focus_loss_enabled) {
				deselect();
			}
		} break;

		case NOTIFICATION_DRAG_END: {
			selection.drag_attempt = false;
		} break;
	}
}

Control::CursorShape RichTextLabel::get_cursor_shape(const Point2 &p_pos) const {
	if (selection.click_item) {
		return CURSOR_IBEAM;
	}

	Item *item = nullptr;
	bool outside = true;
	const_cast<RichTextLabel *>(this)->_find_click(main, p_pos, nullptr, nullptr, &item, nullptr, &outside, true);

	if (item && !outside && const_cast<RichTextLabel *>(this)->_find_meta(item, nullptr)) {
		return CURSOR_POINTING_HAND;
	}
	return get_default_cursor_shape();
}

void RichTextLabel::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseButton> b = p_event;

	if (b.is_valid()) {
		if (b->get_button_index() == MouseButton::LEFT) {
			if (b->is_pressed() && !b->is_double_click()) {
				scroll_updated = false;
				ItemFrame *c_frame = nullptr;
				int c_line = 0;
				Item *c_item = nullptr;
				int c_index = 0;
				bool outside;

				selection.double_click = false;
				selection.drag_attempt = false;

				_find_click(main, b->get_position(), &c_frame, &c_line, &c_item, &c_index, &outside, false);
				if (c_item != nullptr) {
					if (selection.enabled) {
						selection.click_frame = c_frame;
						selection.click_item = c_item;
						selection.click_line = c_line;
						selection.click_char = c_index;

						// Erase previous selection.
						if (selection.active) {
							if (drag_and_drop_selection_enabled && _is_click_inside_selection()) {
								selection.drag_attempt = true;
								selection.click_item = nullptr;
							} else {
								selection.from_frame = nullptr;
								selection.from_line = 0;
								selection.from_item = nullptr;
								selection.from_char = 0;
								selection.to_frame = nullptr;
								selection.to_line = 0;
								selection.to_item = nullptr;
								selection.to_char = 0;
								deselect();
							}
						}
					}
				}
			} else if (b->is_pressed() && b->is_double_click() && selection.enabled) {
				//double_click: select word

				ItemFrame *c_frame = nullptr;
				int c_line = 0;
				Item *c_item = nullptr;
				int c_index = 0;
				bool outside;

				selection.drag_attempt = false;

				_find_click(main, b->get_position(), &c_frame, &c_line, &c_item, &c_index, &outside, false);

				if (c_frame) {
					const Line &l = c_frame->lines[c_line];
					MutexLock lock(l.text_buf->get_mutex());
					PackedInt32Array words = TS->shaped_text_get_word_breaks(l.text_buf->get_rid());
					for (int i = 0; i < words.size(); i = i + 2) {
						if (c_index >= words[i] && c_index < words[i + 1]) {
							selection.from_frame = c_frame;
							selection.from_line = c_line;
							selection.from_item = c_item;
							selection.from_char = words[i];

							selection.to_frame = c_frame;
							selection.to_line = c_line;
							selection.to_item = c_item;
							selection.to_char = words[i + 1];

							selection.active = true;
							if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CLIPBOARD_PRIMARY)) {
								DisplayServer::get_singleton()->clipboard_set_primary(get_selected_text());
							}
							queue_redraw();
							break;
						}
					}

					selection.click_frame = c_frame;
					selection.click_item = c_item;
					selection.click_line = c_line;
					selection.click_char = c_index;

					selection.double_click = true;
				}
			} else if (!b->is_pressed()) {
				if (selection.enabled && DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_CLIPBOARD_PRIMARY)) {
					DisplayServer::get_singleton()->clipboard_set_primary(get_selected_text());
				}
				selection.click_item = nullptr;
				if (selection.drag_attempt) {
					selection.drag_attempt = false;
					if (_is_click_inside_selection()) {
						selection.from_frame = nullptr;
						selection.from_line = 0;
						selection.from_item = nullptr;
						selection.from_char = 0;
						selection.to_frame = nullptr;
						selection.to_line = 0;
						selection.to_item = nullptr;
						selection.to_char = 0;
						deselect();
					}
				}
				if (!b->is_double_click() && !scroll_updated && !selection.active) {
					Item *c_item = nullptr;

					bool outside = true;
					_find_click(main, b->get_position(), nullptr, nullptr, &c_item, nullptr, &outside, true);

					if (c_item) {
						Variant meta;
						if (!outside && _find_meta(c_item, &meta)) {
							//meta clicked
							emit_signal(SNAME("meta_clicked"), meta);
						}
					}
				}
			}
		}

		bool scroll_value_modified = false;
		double prev_scroll = vscroll->get_value();

		if (b->get_button_index() == MouseButton::WHEEL_UP) {
			if (scroll_active) {
				vscroll->scroll(-vscroll->get_page() * b->get_factor() * 0.5 / 8);
				scroll_value_modified = true;
			}
		}
		if (b->get_button_index() == MouseButton::WHEEL_DOWN) {
			if (scroll_active) {
				vscroll->scroll(vscroll->get_page() * b->get_factor() * 0.5 / 8);
				scroll_value_modified = true;
			}
		}

		if (scroll_value_modified && vscroll->get_value() != prev_scroll) {
			accept_event();
			return;
		}

		if (b->get_button_index() == MouseButton::RIGHT && context_menu_enabled) {
			_update_context_menu();
			menu->set_position(get_screen_position() + b->get_position());
			menu->reset_size();
			menu->popup();
			grab_focus();
		}
	}

	Ref<InputEventPanGesture> pan_gesture = p_event;
	if (pan_gesture.is_valid()) {
		if (scroll_active) {
			vscroll->scroll(vscroll->get_page() * pan_gesture->get_delta().y * 0.5 / 8);
		}

		return;
	}

	Ref<InputEventKey> k = p_event;

	if (k.is_valid()) {
		if (k->is_pressed()) {
			bool handled = false;

			if (k->is_action("ui_page_up", true) && vscroll->is_visible_in_tree()) {
				vscroll->scroll(-vscroll->get_page());
				handled = true;
			}
			if (k->is_action("ui_page_down", true) && vscroll->is_visible_in_tree()) {
				vscroll->scroll(vscroll->get_page());
				handled = true;
			}
			if (k->is_action("ui_up", true) && vscroll->is_visible_in_tree()) {
				vscroll->scroll(-theme_cache.normal_font->get_height(theme_cache.normal_font_size));
				handled = true;
			}
			if (k->is_action("ui_down", true) && vscroll->is_visible_in_tree()) {
				vscroll->scroll(theme_cache.normal_font->get_height(theme_cache.normal_font_size));
				handled = true;
			}
			if (k->is_action("ui_home", true) && vscroll->is_visible_in_tree()) {
				vscroll->scroll_to(0);
				handled = true;
			}
			if (k->is_action("ui_end", true) && vscroll->is_visible_in_tree()) {
				vscroll->scroll_to(vscroll->get_max());
				handled = true;
			}
			if (is_shortcut_keys_enabled()) {
				if (k->is_action("ui_text_select_all", true)) {
					select_all();
					handled = true;
				}
				if (k->is_action("ui_copy", true)) {
					selection_copy();
					handled = true;
				}
			}
			if (k->is_action("ui_menu", true)) {
				if (context_menu_enabled) {
					_update_context_menu();
					menu->set_position(get_screen_position());
					menu->reset_size();
					menu->popup();
					menu->grab_focus();
				}
				handled = true;
			}

			if (handled) {
				accept_event();
			}
		}
	}

	Ref<InputEventMouseMotion> m = p_event;
	if (m.is_valid()) {
		ItemFrame *c_frame = nullptr;
		int c_line = 0;
		Item *c_item = nullptr;
		int c_index = 0;
		bool outside;

		_find_click(main, m->get_position(), &c_frame, &c_line, &c_item, &c_index, &outside, false);
		if (selection.click_item && c_item) {
			selection.from_frame = selection.click_frame;
			selection.from_line = selection.click_line;
			selection.from_item = selection.click_item;
			selection.from_char = selection.click_char;

			selection.to_frame = c_frame;
			selection.to_line = c_line;
			selection.to_item = c_item;
			selection.to_char = c_index;

			bool swap = false;
			if (selection.click_frame && c_frame) {
				const Line &l1 = c_frame->lines[c_line];
				const Line &l2 = selection.click_frame->lines[selection.click_line];
				if (l1.char_offset + c_index < l2.char_offset + selection.click_char) {
					swap = true;
				} else if (l1.char_offset + c_index == l2.char_offset + selection.click_char && !selection.double_click) {
					deselect();
					return;
				}
			}

			if (swap) {
				SWAP(selection.from_frame, selection.to_frame);
				SWAP(selection.from_line, selection.to_line);
				SWAP(selection.from_item, selection.to_item);
				SWAP(selection.from_char, selection.to_char);
			}

			if (selection.double_click && c_frame) {
				// Expand the selection to word edges.

				Line *l = &selection.from_frame->lines[selection.from_line];
				MutexLock lock(l->text_buf->get_mutex());
				PackedInt32Array words = TS->shaped_text_get_word_breaks(l->text_buf->get_rid());
				for (int i = 0; i < words.size(); i = i + 2) {
					if (selection.from_char > words[i] && selection.from_char < words[i + 1]) {
						selection.from_char = words[i];
						break;
					}
				}
				l = &selection.to_frame->lines[selection.to_line];
				lock = MutexLock(l->text_buf->get_mutex());
				words = TS->shaped_text_get_word_breaks(l->text_buf->get_rid());
				for (int i = 0; i < words.size(); i = i + 2) {
					if (selection.to_char > words[i] && selection.to_char < words[i + 1]) {
						selection.to_char = words[i + 1];
						break;
					}
				}
			}

			selection.active = true;
			queue_redraw();
		}

		_find_click(main, m->get_position(), nullptr, nullptr, &c_item, nullptr, &outside, true);
		Variant meta;
		ItemMeta *item_meta;
		ItemMeta *prev_meta = meta_hovering;
		if (c_item && !outside && _find_meta(c_item, &meta, &item_meta)) {
			if (meta_hovering != item_meta) {
				if (meta_hovering) {
					emit_signal(SNAME("meta_hover_ended"), current_meta);
				}
				meta_hovering = item_meta;
				current_meta = meta;
				emit_signal(SNAME("meta_hover_started"), meta);
				if ((item_meta && item_meta->underline == META_UNDERLINE_ON_HOVER) || (prev_meta && prev_meta->underline == META_UNDERLINE_ON_HOVER)) {
					queue_redraw();
				}
			}
		} else if (meta_hovering) {
			meta_hovering = nullptr;
			emit_signal(SNAME("meta_hover_ended"), current_meta);
			current_meta = false;
			if (prev_meta->underline == META_UNDERLINE_ON_HOVER) {
				queue_redraw();
			}
		}
	}
}

String RichTextLabel::get_tooltip(const Point2 &p_pos) const {
	Item *c_item = nullptr;
	bool outside;

	const_cast<RichTextLabel *>(this)->_find_click(main, p_pos, nullptr, nullptr, &c_item, nullptr, &outside, true);

	String description;
	if (c_item && !outside) {
		ItemMeta *meta = nullptr;
		if (const_cast<RichTextLabel *>(this)->_find_hint(c_item, &description)) {
			return description;
		} else if (c_item->type == ITEM_IMAGE && !static_cast<ItemImage *>(c_item)->tooltip.is_empty()) {
			return static_cast<ItemImage *>(c_item)->tooltip;
		} else if (const_cast<RichTextLabel *>(this)->_find_meta(c_item, nullptr, &meta) && meta && !meta->tooltip.is_empty()) {
			return meta->tooltip;
		}
	}

	return Control::get_tooltip(p_pos);
}

void RichTextLabel::_find_frame(Item *p_item, ItemFrame **r_frame, int *r_line) {
	if (r_frame != nullptr) {
		*r_frame = nullptr;
	}
	if (r_line != nullptr) {
		*r_line = 0;
	}

	Item *item = p_item;

	while (item) {
		if (item->parent != nullptr && item->parent->type == ITEM_FRAME) {
			if (r_frame != nullptr) {
				*r_frame = static_cast<ItemFrame *>(item->parent);
			}
			if (r_line != nullptr) {
				*r_line = item->line;
			}
			return;
		}

		item = item->parent;
	}
}

RichTextLabel::Item *RichTextLabel::_find_indentable(Item *p_item) {
	Item *indentable = p_item;

	while (indentable) {
		if (indentable->type == ITEM_INDENT || indentable->type == ITEM_LIST) {
			return indentable;
		}
		indentable = indentable->parent;
	}

	return indentable;
}

RichTextLabel::ItemFont *RichTextLabel::_find_font(Item *p_item) {
	Item *fontitem = p_item;

	while (fontitem) {
		if (fontitem->type == ITEM_FONT) {
			ItemFont *fi = static_cast<ItemFont *>(fontitem);
			switch (fi->def_font) {
				case NORMAL_FONT: {
					if (fi->variation) {
						Ref<FontVariation> fc = fi->font;
						if (fc.is_valid()) {
							fc->set_base_font(theme_cache.normal_font);
						}
					} else {
						fi->font = theme_cache.normal_font;
					}
					if (fi->def_size) {
						fi->font_size = theme_cache.normal_font_size;
					}
				} break;
				case BOLD_FONT: {
					if (fi->variation) {
						Ref<FontVariation> fc = fi->font;
						if (fc.is_valid()) {
							fc->set_base_font(theme_cache.bold_font);
						}
					} else {
						fi->font = theme_cache.bold_font;
					}
					if (fi->def_size) {
						fi->font_size = theme_cache.bold_font_size;
					}
				} break;
				case ITALICS_FONT: {
					if (fi->variation) {
						Ref<FontVariation> fc = fi->font;
						if (fc.is_valid()) {
							fc->set_base_font(theme_cache.italics_font);
						}
					} else {
						fi->font = theme_cache.italics_font;
					}
					if (fi->def_size) {
						fi->font_size = theme_cache.italics_font_size;
					}
				} break;
				case BOLD_ITALICS_FONT: {
					if (fi->variation) {
						Ref<FontVariation> fc = fi->font;
						if (fc.is_valid()) {
							fc->set_base_font(theme_cache.bold_italics_font);
						}
					} else {
						fi->font = theme_cache.bold_italics_font;
					}
					if (fi->def_size) {
						fi->font_size = theme_cache.bold_italics_font_size;
					}
				} break;
				case MONO_FONT: {
					if (fi->variation) {
						Ref<FontVariation> fc = fi->font;
						if (fc.is_valid()) {
							fc->set_base_font(theme_cache.mono_font);
						}
					} else {
						fi->font = theme_cache.mono_font;
					}
					if (fi->def_size) {
						fi->font_size = theme_cache.mono_font_size;
					}
				} break;
				default: {
				} break;
			}
			return fi;
		}

		fontitem = fontitem->parent;
	}

	return nullptr;
}

RichTextLabel::ItemFontSize *RichTextLabel::_find_font_size(Item *p_item) {
	Item *sizeitem = p_item;

	while (sizeitem) {
		if (sizeitem->type == ITEM_FONT_SIZE) {
			ItemFontSize *fi = static_cast<ItemFontSize *>(sizeitem);
			return fi;
		}

		sizeitem = sizeitem->parent;
	}

	return nullptr;
}

int RichTextLabel::_find_outline_size(Item *p_item, int p_default) {
	Item *sizeitem = p_item;

	while (sizeitem) {
		if (sizeitem->type == ITEM_OUTLINE_SIZE) {
			ItemOutlineSize *fi = static_cast<ItemOutlineSize *>(sizeitem);
			return fi->outline_size;
		}

		sizeitem = sizeitem->parent;
	}

	return p_default;
}

RichTextLabel::ItemDropcap *RichTextLabel::_find_dc_item(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_DROPCAP) {
			return static_cast<ItemDropcap *>(item);
		}
		item = item->parent;
	}

	return nullptr;
}

RichTextLabel::ItemList *RichTextLabel::_find_list_item(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_LIST) {
			return static_cast<ItemList *>(item);
		}
		item = item->parent;
	}

	return nullptr;
}

int RichTextLabel::_find_list(Item *p_item, Vector<int> &r_index, Vector<int> &r_count, Vector<ItemList *> &r_list) {
	Item *item = p_item;
	Item *prev_item = p_item;

	int level = 0;

	while (item) {
		if (item->type == ITEM_LIST) {
			ItemList *list = static_cast<ItemList *>(item);

			ItemFrame *frame = nullptr;
			int line = -1;
			_find_frame(list, &frame, &line);

			int index = 1;
			int count = 1;
			if (frame != nullptr) {
				for (int i = list->line + 1; i < (int)frame->lines.size(); i++) {
					if (_find_list_item(frame->lines[i].from) == list) {
						if (i <= prev_item->line) {
							index++;
						}
						count++;
					}
				}
			}

			r_index.push_back(index);
			r_count.push_back(count);
			r_list.push_back(list);

			prev_item = item;
		}
		level++;
		item = item->parent;
	}

	return level;
}

int RichTextLabel::_find_margin(Item *p_item, const Ref<Font> &p_base_font, int p_base_font_size) {
	Item *item = p_item;

	float margin = 0.0;

	while (item) {
		if (item->type == ITEM_FRAME) {
			break;
		}

		if (item->type == ITEM_INDENT) {
			Ref<Font> font = p_base_font;
			int font_size = p_base_font_size;

			ItemFont *font_it = _find_font(item);
			if (font_it) {
				if (font_it->font.is_valid()) {
					font = font_it->font;
				}
				if (font_it->font_size > 0) {
					font_size = font_it->font_size;
				}
			}
			ItemFontSize *font_size_it = _find_font_size(item);
			if (font_size_it && font_size_it->font_size > 0) {
				font_size = font_size_it->font_size;
			}
			margin += tab_size * font->get_char_size(' ', font_size).width;

		} else if (item->type == ITEM_LIST) {
			Ref<Font> font = p_base_font;
			int font_size = p_base_font_size;

			ItemFont *font_it = _find_font(item);
			if (font_it) {
				if (font_it->font.is_valid()) {
					font = font_it->font;
				}
				if (font_it->font_size > 0) {
					font_size = font_it->font_size;
				}
			}
			ItemFontSize *font_size_it = _find_font_size(item);
			if (font_size_it && font_size_it->font_size > 0) {
				font_size = font_size_it->font_size;
			}
			margin += tab_size * font->get_char_size(' ', font_size).width;
		}

		item = item->parent;
	}

	return margin;
}

BitField<TextServer::JustificationFlag> RichTextLabel::_find_jst_flags(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			return p->jst_flags;
		}

		item = item->parent;
	}

	return default_jst_flags;
}

PackedFloat32Array RichTextLabel::_find_tab_stops(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			return p->tab_stops;
		}

		item = item->parent;
	}

	return default_tab_stops;
}

HorizontalAlignment RichTextLabel::_find_alignment(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			return p->alignment;
		}

		item = item->parent;
	}

	return default_alignment;
}

TextServer::Direction RichTextLabel::_find_direction(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			if (p->direction != Control::TEXT_DIRECTION_INHERITED) {
				return (TextServer::Direction)p->direction;
			}
		}

		item = item->parent;
	}

	if (text_direction == Control::TEXT_DIRECTION_INHERITED) {
		return is_layout_rtl() ? TextServer::DIRECTION_RTL : TextServer::DIRECTION_LTR;
	} else {
		return (TextServer::Direction)text_direction;
	}
}

TextServer::StructuredTextParser RichTextLabel::_find_stt(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			return p->st_parser;
		}

		item = item->parent;
	}

	return st_parser;
}

String RichTextLabel::_find_language(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_LANGUAGE) {
			ItemLanguage *p = static_cast<ItemLanguage *>(item);
			return p->language;
		} else if (item->type == ITEM_PARAGRAPH) {
			ItemParagraph *p = static_cast<ItemParagraph *>(item);
			return p->language;
		}

		item = item->parent;
	}

	return language;
}

Color RichTextLabel::_find_color(Item *p_item, const Color &p_default_color) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_COLOR) {
			ItemColor *color = static_cast<ItemColor *>(item);
			return color->color;
		}

		item = item->parent;
	}

	return p_default_color;
}

Color RichTextLabel::_find_outline_color(Item *p_item, const Color &p_default_color) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_OUTLINE_COLOR) {
			ItemOutlineColor *color = static_cast<ItemOutlineColor *>(item);
			return color->color;
		}

		item = item->parent;
	}

	return p_default_color;
}

bool RichTextLabel::_find_underline(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_UNDERLINE) {
			return true;
		}

		item = item->parent;
	}

	return false;
}

bool RichTextLabel::_find_strikethrough(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_STRIKETHROUGH) {
			return true;
		}

		item = item->parent;
	}

	return false;
}

void RichTextLabel::_fetch_item_fx_stack(Item *p_item, Vector<ItemFX *> &r_stack) {
	Item *item = p_item;
	while (item) {
		if (item->type == ITEM_CUSTOMFX || item->type == ITEM_SHAKE || item->type == ITEM_WAVE || item->type == ITEM_TORNADO || item->type == ITEM_RAINBOW || item->type == ITEM_PULSE) {
			r_stack.push_back(static_cast<ItemFX *>(item));
		}

		item = item->parent;
	}
}

void RichTextLabel::_normalize_subtags(Vector<String> &subtags) {
	for (String &subtag : subtags) {
		subtag = subtag.unquote();
	}
}

bool RichTextLabel::_find_meta(Item *p_item, Variant *r_meta, ItemMeta **r_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_META) {
			ItemMeta *meta = static_cast<ItemMeta *>(item);
			if (r_meta) {
				*r_meta = meta->meta;
			}
			if (r_item) {
				*r_item = meta;
			}
			return true;
		}

		item = item->parent;
	}

	return false;
}

bool RichTextLabel::_find_hint(Item *p_item, String *r_description) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_HINT) {
			ItemHint *hint = static_cast<ItemHint *>(item);
			if (r_description) {
				*r_description = hint->description;
			}
			return true;
		}

		item = item->parent;
	}

	return false;
}

Color RichTextLabel::_find_bgcolor(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_BGCOLOR) {
			ItemBGColor *color = static_cast<ItemBGColor *>(item);
			return color->color;
		}

		item = item->parent;
	}

	return Color(0, 0, 0, 0);
}

Color RichTextLabel::_find_fgcolor(Item *p_item) {
	Item *item = p_item;

	while (item) {
		if (item->type == ITEM_FGCOLOR) {
			ItemFGColor *color = static_cast<ItemFGColor *>(item);
			return color->color;
		}

		item = item->parent;
	}

	return Color(0, 0, 0, 0);
}

bool RichTextLabel::_find_layout_subitem(Item *from, Item *to) {
	if (from && from != to) {
		if (from->type != ITEM_FONT && from->type != ITEM_COLOR && from->type != ITEM_UNDERLINE && from->type != ITEM_STRIKETHROUGH) {
			return true;
		}

		for (Item *E : from->subitems) {
			bool layout = _find_layout_subitem(E, to);

			if (layout) {
				return true;
			}
		}
	}

	return false;
}

void RichTextLabel::_thread_function(void *p_userdata) {
	set_current_thread_safe_for_nodes(true);
	_process_line_caches();
	updating.store(false);
	callable_mp(this, &RichTextLabel::_thread_end).call_deferred();
}

void RichTextLabel::_thread_end() {
	set_physics_process_internal(false);
	if (!scroll_visible) {
		vscroll->hide();
	}
	if (is_visible_in_tree()) {
		queue_redraw();
	}
}

void RichTextLabel::_stop_thread() {
	if (threaded) {
		stop_thread.store(true);
		wait_until_finished();
	}
}

int RichTextLabel::get_pending_paragraphs() const {
	int to_line = main->first_invalid_line.load();
	int lines = main->lines.size();

	return lines - to_line;
}

bool RichTextLabel::is_finished() const {
	const_cast<RichTextLabel *>(this)->_validate_line_caches();

	if (updating.load()) {
		return false;
	}
	return (main->first_invalid_line.load() == (int)main->lines.size() && main->first_resized_line.load() == (int)main->lines.size() && main->first_invalid_font_line.load() == (int)main->lines.size());
}

bool RichTextLabel::is_updating() const {
	return updating.load() || validating.load();
}

void RichTextLabel::wait_until_finished() {
	if (task != WorkerThreadPool::INVALID_TASK_ID) {
		WorkerThreadPool::get_singleton()->wait_for_task_completion(task);
		task = WorkerThreadPool::INVALID_TASK_ID;
	}
}

void RichTextLabel::set_threaded(bool p_threaded) {
	if (threaded != p_threaded) {
		_stop_thread();
		threaded = p_threaded;
		queue_redraw();
	}
}

bool RichTextLabel::is_threaded() const {
	return threaded;
}

void RichTextLabel::set_progress_bar_delay(int p_delay_ms) {
	progress_delay = p_delay_ms;
}

int RichTextLabel::get_progress_bar_delay() const {
	return progress_delay;
}

_FORCE_INLINE_ float RichTextLabel::_update_scroll_exceeds(float p_total_height, float p_ctrl_height, float p_width, int p_idx, float p_old_scroll, float p_text_rect_height) {
	updating_scroll = true;

	float total_height = p_total_height;
	bool exceeds = p_total_height > p_ctrl_height && scroll_active;
	if (exceeds != scroll_visible) {
		if (exceeds) {
			scroll_visible = true;
			scroll_w = vscroll->get_combined_minimum_size().width;
			vscroll->show();
			vscroll->set_anchor_and_offset(SIDE_LEFT, ANCHOR_END, -scroll_w);
		} else {
			scroll_visible = false;
			scroll_w = 0;
		}

		main->first_resized_line.store(0);

		total_height = 0;
		for (int j = 0; j <= p_idx; j++) {
			total_height = _resize_line(main, j, theme_cache.normal_font, theme_cache.normal_font_size, p_width - scroll_w, total_height);

			main->first_resized_line.store(j);
		}
	}
	vscroll->set_max(total_height);
	vscroll->set_page(p_text_rect_height);
	if (scroll_follow && scroll_following) {
		vscroll->set_value(total_height);
	} else {
		vscroll->set_value(p_old_scroll);
	}
	updating_scroll = false;

	return total_height;
}

bool RichTextLabel::_validate_line_caches() {
	if (updating.load()) {
		return false;
	}
	validating.store(true);
	if (main->first_invalid_line.load() == (int)main->lines.size()) {
		MutexLock data_lock(data_mutex);
		Rect2 text_rect = _get_text_rect();

		float ctrl_height = get_size().height;

		// Update fonts.
		float old_scroll = vscroll->get_value();
		if (main->first_invalid_font_line.load() != (int)main->lines.size()) {
			for (int i = main->first_invalid_font_line.load(); i < (int)main->lines.size(); i++) {
				_update_line_font(main, i, theme_cache.normal_font, theme_cache.normal_font_size);
			}
			main->first_resized_line.store(main->first_invalid_font_line.load());
			main->first_invalid_font_line.store(main->lines.size());
		}

		if (main->first_resized_line.load() == (int)main->lines.size()) {
			vscroll->set_value(old_scroll);
			validating.store(false);
			if (!scroll_visible) {
				vscroll->hide();
			}
			return true;
		}

		// Resize lines without reshaping.
		int fi = main->first_resized_line.load();

		float total_height = (fi == 0) ? 0 : _calculate_line_vertical_offset(main->lines[fi - 1]);
		for (int i = fi; i < (int)main->lines.size(); i++) {
			total_height = _resize_line(main, i, theme_cache.normal_font, theme_cache.normal_font_size, text_rect.get_size().width - scroll_w, total_height);
			total_height = _update_scroll_exceeds(total_height, ctrl_height, text_rect.get_size().width, i, old_scroll, text_rect.size.height);
			main->first_resized_line.store(i);
		}

		main->first_resized_line.store(main->lines.size());

		if (fit_content) {
			update_minimum_size();
		}
		validating.store(false);
		if (!scroll_visible) {
			vscroll->hide();
		}
		return true;
	}
	validating.store(false);
	stop_thread.store(false);
	if (threaded) {
		updating.store(true);
		loaded.store(true);
		task = WorkerThreadPool::get_singleton()->add_template_task(this, &RichTextLabel::_thread_function, nullptr, true, vformat("RichTextLabelShape:%x", (int64_t)get_instance_id()));
		set_physics_process_internal(true);
		loading_started = OS::get_singleton()->get_ticks_msec();
		return false;
	} else {
		updating.store(true);
		_process_line_caches();
		updating.store(false);
		if (!scroll_visible) {
			vscroll->hide();
		}
		queue_redraw();
		return true;
	}
}

void RichTextLabel::_process_line_caches() {
	// Shape invalid lines.
	if (!is_inside_tree()) {
		return;
	}

	MutexLock data_lock(data_mutex);
	Rect2 text_rect = _get_text_rect();

	float ctrl_height = get_size().height;
	int fi = main->first_invalid_line.load();
	int total_chars = main->lines[fi].char_offset;
	float old_scroll = vscroll->get_value();

	float total_height = 0;
	if (fi != 0) {
		int sr = MIN(main->first_invalid_font_line.load(), main->first_resized_line.load());

		// Update fonts.
		for (int i = main->first_invalid_font_line.load(); i < fi; i++) {
			_update_line_font(main, i, theme_cache.normal_font, theme_cache.normal_font_size);

			main->first_invalid_font_line.store(i);

			if (stop_thread.load()) {
				return;
			}
		}

		// Resize lines without reshaping.
		if (sr != 0) {
			total_height = _calculate_line_vertical_offset(main->lines[sr - 1]);
		}

		for (int i = sr; i < fi; i++) {
			total_height = _resize_line(main, i, theme_cache.normal_font, theme_cache.normal_font_size, text_rect.get_size().width - scroll_w, total_height);
			total_height = _update_scroll_exceeds(total_height, ctrl_height, text_rect.get_size().width, i, old_scroll, text_rect.size.height);

			main->first_resized_line.store(i);

			if (stop_thread.load()) {
				return;
			}
		}
	}

	total_height = (fi == 0) ? 0 : _calculate_line_vertical_offset(main->lines[fi - 1]);
	for (int i = fi; i < (int)main->lines.size(); i++) {
		total_height = _shape_line(main, i, theme_cache.normal_font, theme_cache.normal_font_size, text_rect.get_size().width - scroll_w, total_height, &total_chars);
		total_height = _update_scroll_exceeds(total_height, ctrl_height, text_rect.get_size().width, i, old_scroll, text_rect.size.height);

		main->first_invalid_line.store(i);
		main->first_resized_line.store(i);
		main->first_invalid_font_line.store(i);

		if (stop_thread.load()) {
			return;
		}
		loaded.store(double(i) / double(main->lines.size()));
	}

	main->first_invalid_line.store(main->lines.size());
	main->first_resized_line.store(main->lines.size());
	main->first_invalid_font_line.store(main->lines.size());

	if (fit_content) {
		update_minimum_size();
	}
	emit_signal(SceneStringName(finished));
}

void RichTextLabel::_invalidate_current_line(ItemFrame *p_frame) {
	if ((int)p_frame->lines.size() - 1 <= p_frame->first_invalid_line) {
		p_frame->first_invalid_line = (int)p_frame->lines.size() - 1;
	}
}

void RichTextLabel::_texture_changed(RID p_item) {
	Item *it = items.get_or_null(p_item);
	if (it && it->type == ITEM_IMAGE) {
		ItemImage *img = reinterpret_cast<ItemImage *>(it);
		Size2 new_size = _get_image_size(img->image, img->rq_size.width, img->rq_size.height, img->region);
		if (img->size != new_size) {
			main->first_invalid_line.store(0);
			img->size = new_size;
		}
	}
	queue_redraw();
}

void RichTextLabel::add_text(const String &p_text) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	if (current->type == ITEM_TABLE) {
		return; //can't add anything here
	}

	int pos = 0;
	String t = p_text.replace("\r\n", "\n");

	while (pos < t.length()) {
		int end = t.find_char('\n', pos);
		String line;
		bool eol = false;
		if (end == -1) {
			end = t.length();
		} else {
			eol = true;
		}

		if (pos == 0 && end == t.length()) {
			line = t;
		} else {
			line = t.substr(pos, end - pos);
		}

		if (line.length() > 0) {
			if (current->subitems.size() && current->subitems.back()->get()->type == ITEM_TEXT) {
				//append text condition!
				ItemText *ti = static_cast<ItemText *>(current->subitems.back()->get());
				ti->text += line;
				_invalidate_current_line(main);

			} else {
				//append item condition
				ItemText *item = memnew(ItemText);
				item->owner = get_instance_id();
				item->rid = items.make_rid(item);
				item->text = line;
				_add_item(item, false);
			}
		}

		if (eol) {
			ItemNewline *item = memnew(ItemNewline);
			item->owner = get_instance_id();
			item->rid = items.make_rid(item);
			item->line = current_frame->lines.size();
			_add_item(item, false);
			current_frame->lines.resize(current_frame->lines.size() + 1);
			if (item->type != ITEM_NEWLINE) {
				current_frame->lines[current_frame->lines.size() - 1].from = item;
			}
			_invalidate_current_line(current_frame);
		}

		pos = end + 1;
	}
	queue_redraw();
}

void RichTextLabel::_add_item(Item *p_item, bool p_enter, bool p_ensure_newline) {
	if (!internal_stack_editing) {
		stack_externally_modified = true;
	}

	if (p_enter && !parsing_bbcode.load() && !tag_stack.is_empty()) {
		tag_stack.push_back(U"?");
	}

	p_item->parent = current;
	p_item->E = current->subitems.push_back(p_item);
	p_item->index = current_idx++;
	p_item->char_ofs = current_char_ofs;
	if (p_item->type == ITEM_TEXT) {
		ItemText *t = static_cast<ItemText *>(p_item);
		current_char_ofs += t->text.length();
	} else if (p_item->type == ITEM_IMAGE) {
		current_char_ofs++;
	} else if (p_item->type == ITEM_NEWLINE) {
		current_char_ofs++;
	}

	if (p_enter) {
		current = p_item;
	}

	if (p_ensure_newline) {
		Item *from = current_frame->lines[current_frame->lines.size() - 1].from;
		// only create a new line for Item types that generate content/layout, ignore those that represent formatting/styling
		if (_find_layout_subitem(from, p_item)) {
			_invalidate_current_line(current_frame);
			current_frame->lines.resize(current_frame->lines.size() + 1);
		}
	}

	if (current_frame->lines[current_frame->lines.size() - 1].from == nullptr) {
		current_frame->lines[current_frame->lines.size() - 1].from = p_item;
	}
	p_item->line = current_frame->lines.size() - 1;

	_invalidate_current_line(current_frame);

	if (fit_content) {
		update_minimum_size();
	}
	queue_redraw();
}

Size2 RichTextLabel::_get_image_size(const Ref<Texture2D> &p_image, int p_width, int p_height, const Rect2 &p_region) {
	Size2 ret;
	if (p_width > 0) {
		// custom width
		ret.width = p_width;
		if (p_height > 0) {
			// custom height
			ret.height = p_height;
		} else {
			// calculate height to keep aspect ratio
			if (p_region.has_area()) {
				ret.height = p_region.get_size().height * p_width / p_region.get_size().width;
			} else {
				ret.height = p_image->get_height() * p_width / p_image->get_width();
			}
		}
	} else {
		if (p_height > 0) {
			// custom height
			ret.height = p_height;
			// calculate width to keep aspect ratio
			if (p_region.has_area()) {
				ret.width = p_region.get_size().width * p_height / p_region.get_size().height;
			} else {
				ret.width = p_image->get_width() * p_height / p_image->get_height();
			}
		} else {
			if (p_region.has_area()) {
				// if the image has a region, keep the region size
				ret = p_region.get_size();
			} else {
				// keep original width and height
				ret = p_image->get_size();
			}
		}
	}
	return ret;
}

void RichTextLabel::add_image(const Ref<Texture2D> &p_image, int p_width, int p_height, const Color &p_color, InlineAlignment p_alignment, const Rect2 &p_region, const Variant &p_key, bool p_pad, const String &p_tooltip, bool p_size_in_percent) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	if (current->type == ITEM_TABLE) {
		return;
	}

	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND(p_image->get_width() == 0);
	ERR_FAIL_COND(p_image->get_height() == 0);
	ERR_FAIL_COND(p_width < 0);
	ERR_FAIL_COND(p_height < 0);

	ItemImage *item = memnew(ItemImage);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);

	if (p_region.has_area()) {
		Ref<AtlasTexture> atlas_tex = memnew(AtlasTexture);
		atlas_tex->set_atlas(p_image);
		atlas_tex->set_region(p_region);
		item->image = atlas_tex;
	} else {
		item->image = p_image;
	}
	item->color = p_color;
	item->inline_align = p_alignment;
	item->rq_size = Size2(p_width, p_height);
	item->region = p_region;
	item->size = _get_image_size(p_image, p_width, p_height, p_region);
	item->size_in_percent = p_size_in_percent;
	item->pad = p_pad;
	item->key = p_key;
	item->tooltip = p_tooltip;

	item->image->connect_changed(callable_mp(this, &RichTextLabel::_texture_changed).bind(item->rid), CONNECT_REFERENCE_COUNTED);

	_add_item(item, false);
}

void RichTextLabel::update_image(const Variant &p_key, BitField<ImageUpdateMask> p_mask, const Ref<Texture2D> &p_image, int p_width, int p_height, const Color &p_color, InlineAlignment p_alignment, const Rect2 &p_region, bool p_pad, const String &p_tooltip, bool p_size_in_percent) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	if (p_mask & UPDATE_TEXTURE) {
		ERR_FAIL_COND(p_image.is_null());
		ERR_FAIL_COND(p_image->get_width() == 0);
		ERR_FAIL_COND(p_image->get_height() == 0);
	}

	ERR_FAIL_COND(p_width < 0);
	ERR_FAIL_COND(p_height < 0);

	bool reshape = false;

	Item *it = main;
	while (it) {
		if (it->type == ITEM_IMAGE) {
			ItemImage *it_img = static_cast<ItemImage *>(it);
			if (it_img->key == p_key) {
				ItemImage *item = it_img;
				if (p_mask & UPDATE_REGION) {
					item->region = p_region;
				}
				if (p_mask & UPDATE_TEXTURE) {
					if (item->image.is_valid()) {
						item->image->disconnect_changed(callable_mp(this, &RichTextLabel::_texture_changed));
					}
					if (item->region.has_area()) {
						Ref<AtlasTexture> atlas_tex = memnew(AtlasTexture);
						atlas_tex->set_atlas(p_image);
						atlas_tex->set_region(item->region);
						item->image = atlas_tex;
					} else {
						item->image = p_image;
					}
					item->image->connect_changed(callable_mp(this, &RichTextLabel::_texture_changed).bind(item->rid), CONNECT_REFERENCE_COUNTED);
				}
				if (p_mask & UPDATE_COLOR) {
					item->color = p_color;
				}
				if (p_mask & UPDATE_TOOLTIP) {
					item->tooltip = p_tooltip;
				}
				if (p_mask & UPDATE_PAD) {
					item->pad = p_pad;
				}
				if (p_mask & UPDATE_ALIGNMENT) {
					if (item->inline_align != p_alignment) {
						reshape = true;
						item->inline_align = p_alignment;
					}
				}
				if (p_mask & UPDATE_WIDTH_IN_PERCENT) {
					if (item->size_in_percent != p_size_in_percent) {
						reshape = true;
						item->size_in_percent = p_size_in_percent;
					}
				}
				if (p_mask & UPDATE_SIZE) {
					if (p_width > 0) {
						item->rq_size.width = p_width;
					}
					if (p_height > 0) {
						item->rq_size.height = p_height;
					}
				}
				if ((p_mask & UPDATE_SIZE) || (p_mask & UPDATE_REGION) || (p_mask & UPDATE_TEXTURE)) {
					ERR_FAIL_COND(item->image.is_null());
					ERR_FAIL_COND(item->image->get_width() == 0);
					ERR_FAIL_COND(item->image->get_height() == 0);
					Size2 new_size = _get_image_size(item->image, item->rq_size.width, item->rq_size.height, item->region);
					if (item->size != new_size) {
						reshape = true;
						item->size = new_size;
					}
				}
			}
		}
		it = _get_next_item(it, true);
	}

	if (reshape) {
		main->first_invalid_line.store(0);
	}
	queue_redraw();
}

void RichTextLabel::add_newline() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	if (current->type == ITEM_TABLE) {
		return;
	}
	ItemNewline *item = memnew(ItemNewline);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->line = current_frame->lines.size();
	_add_item(item, false);
	current_frame->lines.resize(current_frame->lines.size() + 1);
	_invalidate_current_line(current_frame);
	queue_redraw();
}

void RichTextLabel::_remove_frame(HashSet<Item *> &r_erase_list, ItemFrame *p_frame, int p_line, bool p_erase, int p_char_offset, int p_line_offset) {
	Line &l = p_frame->lines[p_line];
	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	if (!p_erase) {
		l.char_offset -= p_char_offset;
	}

	for (Item *it = l.from; it && it != it_to;) {
		Item *next_it = _get_next_item(it);
		it->line -= p_line_offset;
		if (!p_erase) {
			while (r_erase_list.has(it->parent)) {
				it->E->erase();
				it->parent = it->parent->parent;
				it->E = it->parent->subitems.push_back(it);
			}
		}
		if (it->type == ITEM_TABLE) {
			ItemTable *table = static_cast<ItemTable *>(it);
			for (List<Item *>::Element *sub_it = table->subitems.front(); sub_it; sub_it = sub_it->next()) {
				ERR_CONTINUE(sub_it->get()->type != ITEM_FRAME); // Children should all be frames.
				ItemFrame *frame = static_cast<ItemFrame *>(sub_it->get());
				for (int i = 0; i < (int)frame->lines.size(); i++) {
					_remove_frame(r_erase_list, frame, i, p_erase, p_char_offset, 0);
				}
				if (p_erase) {
					r_erase_list.insert(frame);
				} else {
					frame->char_ofs -= p_char_offset;
				}
			}
		}
		if (p_erase) {
			r_erase_list.insert(it);
		} else {
			it->char_ofs -= p_char_offset;
		}
		it = next_it;
	}
}

bool RichTextLabel::remove_paragraph(int p_paragraph, bool p_no_invalidate) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	if (p_paragraph >= (int)main->lines.size() || p_paragraph < 0) {
		return false;
	}

	stack_externally_modified = true;

	if (main->lines.size() == 1) {
		// Clear all.
		main->_clear_children();
		current = main;
		current_frame = main;
		main->lines.clear();
		main->lines.resize(1);

		current_char_ofs = 0;
	} else {
		HashSet<Item *> erase_list;
		Line &l = main->lines[p_paragraph];
		int off = l.char_count;
		for (int i = p_paragraph; i < (int)main->lines.size(); i++) {
			if (i == p_paragraph) {
				_remove_frame(erase_list, main, i, true, off, 0);
			} else {
				_remove_frame(erase_list, main, i, false, off, 1);
			}
		}
		for (HashSet<Item *>::Iterator E = erase_list.begin(); E; ++E) {
			Item *it = *E;
			if (current_frame == it) {
				current_frame = main;
			}
			if (current == it) {
				current = main;
			}
			if (!erase_list.has(it->parent)) {
				it->E->erase();
			}
			items.free(it->rid);
			it->subitems.clear();
			memdelete(it);
		}
		main->lines.remove_at(p_paragraph);
		current_char_ofs -= off;
	}

	selection.click_frame = nullptr;
	selection.click_item = nullptr;
	selection.active = false;

	if (is_processing_internal()) {
		bool process_enabled = false;
		Item *it = main;
		while (it) {
			Vector<ItemFX *> fx_stack;
			_fetch_item_fx_stack(it, fx_stack);
			if (fx_stack.size()) {
				process_enabled = true;
				break;
			}
			it = _get_next_item(it, true);
		}
		set_process_internal(process_enabled);
	}

	if (p_no_invalidate) {
		// Do not invalidate cache, only update vertical offsets of the paragraphs after deleted one and scrollbar.
		int to_line = main->first_invalid_line.load() - 1;
		float total_height = (p_paragraph == 0) ? 0 : _calculate_line_vertical_offset(main->lines[p_paragraph - 1]);
		for (int i = p_paragraph; i < to_line; i++) {
			MutexLock lock(main->lines[to_line - 1].text_buf->get_mutex());
			main->lines[i].offset.y = total_height;
			total_height = _calculate_line_vertical_offset(main->lines[i]);
		}
		updating_scroll = true;
		vscroll->set_max(total_height);
		updating_scroll = false;

		main->first_invalid_line.store(MAX(main->first_invalid_line.load() - 1, 0));
		main->first_resized_line.store(MAX(main->first_resized_line.load() - 1, 0));
		main->first_invalid_font_line.store(MAX(main->first_invalid_font_line.load() - 1, 0));
	} else {
		// Invalidate cache after the deleted paragraph.
		main->first_invalid_line.store(MIN(main->first_invalid_line.load(), p_paragraph));
		main->first_resized_line.store(MIN(main->first_resized_line.load(), p_paragraph));
		main->first_invalid_font_line.store(MIN(main->first_invalid_font_line.load(), p_paragraph));
	}
	queue_redraw();

	return true;
}

bool RichTextLabel::invalidate_paragraph(int p_paragraph) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	if (p_paragraph >= (int)main->lines.size() || p_paragraph < 0) {
		return false;
	}

	// Invalidate cache.
	main->first_invalid_line.store(MIN(main->first_invalid_line.load(), p_paragraph));
	main->first_resized_line.store(MIN(main->first_resized_line.load(), p_paragraph));
	main->first_invalid_font_line.store(MIN(main->first_invalid_font_line.load(), p_paragraph));
	queue_redraw();

	return true;
}

void RichTextLabel::push_dropcap(const String &p_string, const Ref<Font> &p_font, int p_size, const Rect2 &p_dropcap_margins, const Color &p_color, int p_ol_size, const Color &p_ol_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ERR_FAIL_COND(p_string.is_empty());
	ERR_FAIL_COND(p_font.is_null());
	ERR_FAIL_COND(p_size <= 0);

	ItemDropcap *item = memnew(ItemDropcap);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->text = p_string.replace("\r\n", "\n");
	item->font = p_font;
	item->font_size = p_size;
	item->color = p_color;
	item->ol_size = p_ol_size;
	item->ol_color = p_ol_color;
	item->dropcap_margins = p_dropcap_margins;
	_add_item(item, false);
}

void RichTextLabel::_push_def_font_var(DefaultFont p_def_font, const Ref<Font> &p_font, int p_size) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemFont *item = memnew(ItemFont);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->def_font = p_def_font;
	item->variation = true;
	item->font = p_font;
	item->font_size = p_size;
	item->def_size = (p_size <= 0);
	_add_item(item, true);
}

void RichTextLabel::_push_def_font(DefaultFont p_def_font) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemFont *item = memnew(ItemFont);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->def_font = p_def_font;
	item->def_size = true;
	_add_item(item, true);
}

void RichTextLabel::push_font(const Ref<Font> &p_font, int p_size) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ERR_FAIL_COND(p_font.is_null());
	ItemFont *item = memnew(ItemFont);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->font = p_font;
	item->font_size = p_size;
	_add_item(item, true);
}

void RichTextLabel::push_normal() {
	ERR_FAIL_COND(theme_cache.normal_font.is_null());

	_push_def_font(NORMAL_FONT);
}

void RichTextLabel::push_bold() {
	ERR_FAIL_COND(theme_cache.bold_font.is_null());

	ItemFont *item_font = _find_font(current);
	_push_def_font((item_font && item_font->def_font == ITALICS_FONT) ? BOLD_ITALICS_FONT : BOLD_FONT);
}

void RichTextLabel::push_bold_italics() {
	ERR_FAIL_COND(theme_cache.bold_italics_font.is_null());

	_push_def_font(BOLD_ITALICS_FONT);
}

void RichTextLabel::push_italics() {
	ERR_FAIL_COND(theme_cache.italics_font.is_null());

	ItemFont *item_font = _find_font(current);
	_push_def_font((item_font && item_font->def_font == BOLD_FONT) ? BOLD_ITALICS_FONT : ITALICS_FONT);
}

void RichTextLabel::push_mono() {
	ERR_FAIL_COND(theme_cache.mono_font.is_null());

	_push_def_font(MONO_FONT);
}

void RichTextLabel::push_font_size(int p_font_size) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemFontSize *item = memnew(ItemFontSize);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->font_size = p_font_size;
	_add_item(item, true);
}

void RichTextLabel::push_outline_size(int p_ol_size) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemOutlineSize *item = memnew(ItemOutlineSize);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->outline_size = p_ol_size;
	_add_item(item, true);
}

void RichTextLabel::push_color(const Color &p_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemColor *item = memnew(ItemColor);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->color = p_color;
	_add_item(item, true);
}

void RichTextLabel::push_outline_color(const Color &p_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemOutlineColor *item = memnew(ItemOutlineColor);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->color = p_color;
	_add_item(item, true);
}

void RichTextLabel::push_underline() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemUnderline *item = memnew(ItemUnderline);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);

	_add_item(item, true);
}

void RichTextLabel::push_strikethrough() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemStrikethrough *item = memnew(ItemStrikethrough);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);

	_add_item(item, true);
}

void RichTextLabel::push_paragraph(HorizontalAlignment p_alignment, Control::TextDirection p_direction, const String &p_language, TextServer::StructuredTextParser p_st_parser, BitField<TextServer::JustificationFlag> p_jst_flags, const PackedFloat32Array &p_tab_stops) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);

	ItemParagraph *item = memnew(ItemParagraph);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->alignment = p_alignment;
	item->direction = p_direction;
	item->language = p_language;
	item->st_parser = p_st_parser;
	item->jst_flags = p_jst_flags;
	item->tab_stops = p_tab_stops;
	_add_item(item, true, true);
}

void RichTextLabel::push_indent(int p_level) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ERR_FAIL_COND(p_level < 0);

	ItemIndent *item = memnew(ItemIndent);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->level = p_level;
	_add_item(item, true, true);
}

void RichTextLabel::push_list(int p_level, ListType p_list, bool p_capitalize, const String &p_bullet) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ERR_FAIL_COND(p_level < 0);

	ItemList *item = memnew(ItemList);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->list_type = p_list;
	item->level = p_level;
	item->capitalize = p_capitalize;
	item->bullet = p_bullet;
	_add_item(item, true, true);
}

void RichTextLabel::push_meta(const Variant &p_meta, MetaUnderline p_underline_mode, const String &p_tooltip) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemMeta *item = memnew(ItemMeta);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->meta = p_meta;
	item->underline = p_underline_mode;
	item->tooltip = p_tooltip;
	_add_item(item, true);
}

void RichTextLabel::push_language(const String &p_language) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemLanguage *item = memnew(ItemLanguage);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->language = p_language;
	_add_item(item, true);
}

void RichTextLabel::push_hint(const String &p_string) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemHint *item = memnew(ItemHint);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->description = p_string;
	_add_item(item, true);
}

void RichTextLabel::push_table(int p_columns, InlineAlignment p_alignment, int p_align_to_row) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ERR_FAIL_COND(p_columns < 1);
	ItemTable *item = memnew(ItemTable);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->columns.resize(p_columns);
	item->total_width = 0;
	item->inline_align = p_alignment;
	item->align_to_row = p_align_to_row;
	for (int i = 0; i < (int)item->columns.size(); i++) {
		item->columns[i].expand = false;
		item->columns[i].shrink = true;
		item->columns[i].expand_ratio = 1;
	}
	_add_item(item, true, false);
}

void RichTextLabel::push_fade(int p_start_index, int p_length) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemFade *item = memnew(ItemFade);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->starting_index = p_start_index;
	item->length = p_length;
	_add_item(item, true);
}

void RichTextLabel::push_shake(int p_strength = 10, float p_rate = 24.0f, bool p_connected = true) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemShake *item = memnew(ItemShake);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->strength = p_strength;
	item->rate = p_rate;
	item->connected = p_connected;
	_add_item(item, true);
}

void RichTextLabel::push_wave(float p_frequency = 1.0f, float p_amplitude = 10.0f, bool p_connected = true) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemWave *item = memnew(ItemWave);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->frequency = p_frequency;
	item->amplitude = p_amplitude;
	item->connected = p_connected;
	_add_item(item, true);
}

void RichTextLabel::push_tornado(float p_frequency = 1.0f, float p_radius = 10.0f, bool p_connected = true) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemTornado *item = memnew(ItemTornado);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->frequency = p_frequency;
	item->radius = p_radius;
	item->connected = p_connected;
	_add_item(item, true);
}

void RichTextLabel::push_rainbow(float p_saturation, float p_value, float p_frequency, float p_speed) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemRainbow *item = memnew(ItemRainbow);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->speed = p_speed;
	item->frequency = p_frequency;
	item->saturation = p_saturation;
	item->value = p_value;
	_add_item(item, true);
}

void RichTextLabel::push_pulse(const Color &p_color, float p_frequency, float p_ease) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ItemPulse *item = memnew(ItemPulse);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->color = p_color;
	item->frequency = p_frequency;
	item->ease = p_ease;
	_add_item(item, true);
}

void RichTextLabel::push_bgcolor(const Color &p_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemBGColor *item = memnew(ItemBGColor);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->color = p_color;
	_add_item(item, true);
}

void RichTextLabel::push_fgcolor(const Color &p_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemFGColor *item = memnew(ItemFGColor);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->color = p_color;
	_add_item(item, true);
}

void RichTextLabel::push_customfx(Ref<RichTextEffect> p_custom_effect, Dictionary p_environment) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemCustomFX *item = memnew(ItemCustomFX);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->custom_effect = p_custom_effect;
	item->char_fx_transform->environment = p_environment;
	_add_item(item, true);

	set_process_internal(true);
}

void RichTextLabel::push_context() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type == ITEM_TABLE);
	ItemContext *item = memnew(ItemContext);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	_add_item(item, true);
}

void RichTextLabel::set_table_column_expand(int p_column, bool p_expand, int p_ratio, bool p_shrink) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_TABLE);

	ItemTable *table = static_cast<ItemTable *>(current);
	ERR_FAIL_INDEX(p_column, (int)table->columns.size());
	table->columns[p_column].expand = p_expand;
	table->columns[p_column].shrink = p_shrink;
	table->columns[p_column].expand_ratio = p_ratio;
}

void RichTextLabel::set_cell_row_background_color(const Color &p_odd_row_bg, const Color &p_even_row_bg) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_FRAME);

	ItemFrame *cell = static_cast<ItemFrame *>(current);
	ERR_FAIL_COND(!cell->cell);
	cell->odd_row_bg = p_odd_row_bg;
	cell->even_row_bg = p_even_row_bg;
}

void RichTextLabel::set_cell_border_color(const Color &p_color) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_FRAME);

	ItemFrame *cell = static_cast<ItemFrame *>(current);
	ERR_FAIL_COND(!cell->cell);
	cell->border = p_color;
}

void RichTextLabel::set_cell_size_override(const Size2 &p_min_size, const Size2 &p_max_size) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_FRAME);

	ItemFrame *cell = static_cast<ItemFrame *>(current);
	ERR_FAIL_COND(!cell->cell);
	cell->min_size_over = p_min_size;
	cell->max_size_over = p_max_size;
}

void RichTextLabel::set_cell_padding(const Rect2 &p_padding) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_FRAME);

	ItemFrame *cell = static_cast<ItemFrame *>(current);
	ERR_FAIL_COND(!cell->cell);
	cell->padding = p_padding;
}

void RichTextLabel::push_cell() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_COND(current->type != ITEM_TABLE);

	ItemFrame *item = memnew(ItemFrame);
	item->owner = get_instance_id();
	item->rid = items.make_rid(item);
	item->parent_frame = current_frame;
	_add_item(item, true);
	current_frame = item;
	item->cell = true;
	item->lines.resize(1);
	item->lines[0].from = nullptr;
	item->first_invalid_line.store(0); // parent frame last line ???
}

int RichTextLabel::get_current_table_column() const {
	ERR_FAIL_COND_V(current->type != ITEM_TABLE, -1);

	ItemTable *table = static_cast<ItemTable *>(current);
	return table->subitems.size() % table->columns.size();
}

void RichTextLabel::pop() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_NULL(current->parent);

	if (current->type == ITEM_FRAME) {
		current_frame = static_cast<ItemFrame *>(current)->parent_frame;
	}
	current = current->parent;
	if (!parsing_bbcode.load() && !tag_stack.is_empty()) {
		tag_stack.pop_back();
	}
}

void RichTextLabel::pop_context() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	ERR_FAIL_NULL(current->parent);

	while (current->parent && current != main) {
		if (current->type == ITEM_FRAME) {
			current_frame = static_cast<ItemFrame *>(current)->parent_frame;
		} else if (current->type == ITEM_CONTEXT) {
			if (!parsing_bbcode.load() && !tag_stack.is_empty()) {
				tag_stack.pop_back();
			}
			current = current->parent;
			return;
		}
		if (!parsing_bbcode.load() && !tag_stack.is_empty()) {
			tag_stack.pop_back();
		}
		current = current->parent;
	}
}

void RichTextLabel::pop_all() {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	current = main;
	current_frame = main;
}

void RichTextLabel::clear() {
	_stop_thread();
	set_process_internal(false);
	MutexLock data_lock(data_mutex);

	stack_externally_modified = false;

	tag_stack.clear();
	main->_clear_children();
	current = main;
	current_frame = main;
	main->lines.clear();
	main->lines.resize(1);
	main->first_invalid_line.store(0);

	selection.click_frame = nullptr;
	selection.click_item = nullptr;
	deselect();

	current_idx = 1;
	current_char_ofs = 0;
	if (scroll_follow) {
		scroll_following = true;
	}

	if (fit_content) {
		update_minimum_size();
	}
}

void RichTextLabel::set_tab_size(int p_spaces) {
	if (tab_size == p_spaces) {
		return;
	}

	_stop_thread();

	tab_size = p_spaces;
	main->first_resized_line.store(0);
	queue_redraw();
}

int RichTextLabel::get_tab_size() const {
	return tab_size;
}

void RichTextLabel::set_fit_content(bool p_enabled) {
	if (p_enabled == fit_content) {
		return;
	}

	fit_content = p_enabled;
	update_minimum_size();
}

bool RichTextLabel::is_fit_content_enabled() const {
	return fit_content;
}

void RichTextLabel::set_meta_underline(bool p_underline) {
	if (underline_meta == p_underline) {
		return;
	}

	underline_meta = p_underline;
	queue_redraw();
}

bool RichTextLabel::is_meta_underlined() const {
	return underline_meta;
}

void RichTextLabel::set_hint_underline(bool p_underline) {
	underline_hint = p_underline;
	queue_redraw();
}

bool RichTextLabel::is_hint_underlined() const {
	return underline_hint;
}

void RichTextLabel::set_offset(int p_pixel) {
	vscroll->set_value(p_pixel);
}

void RichTextLabel::set_scroll_active(bool p_active) {
	if (scroll_active == p_active) {
		return;
	}

	scroll_active = p_active;
	vscroll->set_drag_node_enabled(p_active);
	queue_redraw();
}

bool RichTextLabel::is_scroll_active() const {
	return scroll_active;
}

void RichTextLabel::set_scroll_follow(bool p_follow) {
	scroll_follow = p_follow;
	if (!vscroll->is_visible_in_tree() || vscroll->get_value() > (vscroll->get_max() - vscroll->get_page() - 1)) {
		scroll_following = true;
	}
}

bool RichTextLabel::is_scroll_following() const {
	return scroll_follow;
}

void RichTextLabel::parse_bbcode(const String &p_bbcode) {
	clear();
	append_text(p_bbcode);
}

String RichTextLabel::_get_tag_value(const String &p_tag) {
	return p_tag.substr(p_tag.find_char('=') + 1);
}

int RichTextLabel::_find_unquoted(const String &p_src, char32_t p_chr, int p_from) {
	if (p_from < 0) {
		return -1;
	}

	const int len = p_src.length();
	if (len == 0) {
		return -1;
	}

	const char32_t *src = p_src.get_data();
	bool in_single_quote = false;
	bool in_double_quote = false;
	for (int i = p_from; i < len; i++) {
		if (in_double_quote) {
			if (src[i] == '"') {
				in_double_quote = false;
			}
		} else if (in_single_quote) {
			if (src[i] == '\'') {
				in_single_quote = false;
			}
		} else {
			if (src[i] == '"') {
				in_double_quote = true;
			} else if (src[i] == '\'') {
				in_single_quote = true;
			} else if (src[i] == p_chr) {
				return i;
			}
		}
	}

	return -1;
}

Vector<String> RichTextLabel::_split_unquoted(const String &p_src, char32_t p_splitter) {
	Vector<String> ret;

	if (p_src.is_empty()) {
		return ret;
	}

	int from = 0;
	int len = p_src.length();

	while (true) {
		int end = _find_unquoted(p_src, p_splitter, from);
		if (end < 0) {
			end = len;
		}
		if (end > from) {
			ret.push_back(p_src.substr(from, end - from));
		}
		if (end == len) {
			break;
		}

		from = end + 1;
	}

	return ret;
}

void RichTextLabel::append_text(const String &p_bbcode) {
	_stop_thread();
	MutexLock data_lock(data_mutex);

	parsing_bbcode.store(true);

	int pos = 0;

	bool in_bold = false;
	bool in_italics = false;
	bool after_list_open_tag = false;
	bool after_list_close_tag = false;

	String bbcode = p_bbcode.replace("\r\n", "\n");

	while (pos <= bbcode.length()) {
		int brk_pos = bbcode.find_char('[', pos);

		if (brk_pos < 0) {
			brk_pos = bbcode.length();
		}

		String txt = brk_pos > pos ? bbcode.substr(pos, brk_pos - pos) : "";

		// Trim the first newline character, it may be added later as needed.
		if (after_list_close_tag || after_list_open_tag) {
			txt = txt.trim_prefix("\n");
		}

		if (brk_pos == bbcode.length()) {
			// For tags that are not properly closed.
			if (txt.is_empty() && after_list_open_tag) {
				txt = "\n";
			}

			if (!txt.is_empty()) {
				add_text(txt);
			}
			break; //nothing else to add
		}

		int brk_end = _find_unquoted(bbcode, ']', brk_pos + 1);

		if (brk_end == -1) {
			//no close, add the rest
			txt += bbcode.substr(brk_pos);
			add_text(txt);
			break;
		}

		String tag = bbcode.substr(brk_pos + 1, brk_end - brk_pos - 1);
		Vector<String> split_tag_block = _split_unquoted(tag, ' ');

		// Find optional parameters.
		String bbcode_name;
		typedef HashMap<String, String> OptionMap;
		OptionMap bbcode_options;
		if (!split_tag_block.is_empty()) {
			bbcode_name = split_tag_block[0];
			for (int i = 1; i < split_tag_block.size(); i++) {
				const String &expr = split_tag_block[i];
				int value_pos = expr.find_char('=');
				if (value_pos > -1) {
					bbcode_options[expr.substr(0, value_pos)] = expr.substr(value_pos + 1).unquote();
				}
			}
		} else {
			bbcode_name = tag;
		}

		// Find main parameter.
		String bbcode_value;
		int main_value_pos = bbcode_name.find_char('=');
		if (main_value_pos > -1) {
			bbcode_value = bbcode_name.substr(main_value_pos + 1);
			bbcode_name = bbcode_name.substr(0, main_value_pos);
		}

		if (tag.begins_with("/") && tag_stack.size()) {
			bool tag_ok = tag_stack.size() && tag_stack.front()->get() == tag.substr(1, tag.length());

			if (tag_stack.front()->get() == "b") {
				in_bold = false;
			}
			if (tag_stack.front()->get() == "i") {
				in_italics = false;
			}
			if ((tag_stack.front()->get() == "indent") || (tag_stack.front()->get() == "ol") || (tag_stack.front()->get() == "ul")) {
				current_frame->indent_level--;
			}

			if (!tag_ok) {
				txt += "[" + tag;
				add_text(txt);
				after_list_open_tag = false;
				after_list_close_tag = false;
				pos = brk_end;
				continue;
			}

			if (txt.is_empty() && after_list_open_tag) {
				txt = "\n"; // Make empty list have at least one item.
			}
			after_list_open_tag = false;

			if (tag == "/ol" || tag == "/ul") {
				if (!txt.is_empty()) {
					// Make sure text ends with a newline character, that is, the last item
					// will wrap at the end of block.
					if (!txt.ends_with("\n")) {
						txt += "\n";
					}
				} else if (!after_list_close_tag) {
					txt = "\n"; // Make the innermost list item wrap at the end of lists.
				}
				after_list_close_tag = true;
			} else {
				after_list_close_tag = false;
			}

			if (!txt.is_empty()) {
				add_text(txt);
			}

			tag_stack.pop_front();
			pos = brk_end + 1;
			if (tag != "/img" && tag != "/dropcap") {
				pop();
			}
			continue;
		}

		if (tag == "ol" || tag.begins_with("ol ") || tag == "ul" || tag.begins_with("ul ")) {
			if (txt.is_empty() && after_list_open_tag) {
				txt = "\n"; // Make each list have at least one item at the beginning.
			}
			after_list_open_tag = true;
		} else {
			after_list_open_tag = false;
		}
		if (!txt.is_empty()) {
			add_text(txt);
		}
		after_list_close_tag = false;

		if (tag == "b") {
			//use bold font
			in_bold = true;
			if (in_italics) {
				_push_def_font(BOLD_ITALICS_FONT);
			} else {
				_push_def_font(BOLD_FONT);
			}
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "i") {
			//use italics font
			in_italics = true;
			if (in_bold) {
				_push_def_font(BOLD_ITALICS_FONT);
			} else {
				_push_def_font(ITALICS_FONT);
			}
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "code") {
			//use monospace font
			_push_def_font(MONO_FONT);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("table=")) {
			Vector<String> subtag = _split_unquoted(_get_tag_value(tag), U',');
			_normalize_subtags(subtag);

			int columns = (subtag.is_empty()) ? 1 : subtag[0].to_int();
			if (columns < 1) {
				columns = 1;
			}

			int alignment = INLINE_ALIGNMENT_TOP;
			if (subtag.size() > 2) {
				if (subtag[1] == "top" || subtag[1] == "t") {
					alignment = INLINE_ALIGNMENT_TOP_TO;
				} else if (subtag[1] == "center" || subtag[1] == "c") {
					alignment = INLINE_ALIGNMENT_CENTER_TO;
				} else if (subtag[1] == "baseline" || subtag[1] == "l") {
					alignment = INLINE_ALIGNMENT_BASELINE_TO;
				} else if (subtag[1] == "bottom" || subtag[1] == "b") {
					alignment = INLINE_ALIGNMENT_BOTTOM_TO;
				}
				if (subtag[2] == "top" || subtag[2] == "t") {
					alignment |= INLINE_ALIGNMENT_TO_TOP;
				} else if (subtag[2] == "center" || subtag[2] == "c") {
					alignment |= INLINE_ALIGNMENT_TO_CENTER;
				} else if (subtag[2] == "baseline" || subtag[2] == "l") {
					alignment |= INLINE_ALIGNMENT_TO_BASELINE;
				} else if (subtag[2] == "bottom" || subtag[2] == "b") {
					alignment |= INLINE_ALIGNMENT_TO_BOTTOM;
				}
			} else if (subtag.size() > 1) {
				if (subtag[1] == "top" || subtag[1] == "t") {
					alignment = INLINE_ALIGNMENT_TOP;
				} else if (subtag[1] == "center" || subtag[1] == "c") {
					alignment = INLINE_ALIGNMENT_CENTER;
				} else if (subtag[1] == "bottom" || subtag[1] == "b") {
					alignment = INLINE_ALIGNMENT_BOTTOM;
				}
			}
			int row = -1;
			if (subtag.size() > 3) {
				row = subtag[3].to_int();
			}

			push_table(columns, (InlineAlignment)alignment, row);
			pos = brk_end + 1;
			tag_stack.push_front("table");
		} else if (tag == "cell") {
			push_cell();
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("cell=")) {
			int ratio = _get_tag_value(tag).to_int();
			if (ratio < 1) {
				ratio = 1;
			}

			set_table_column_expand(get_current_table_column(), true, ratio);
			push_cell();

			pos = brk_end + 1;
			tag_stack.push_front("cell");
		} else if (tag.begins_with("cell ")) {
			bool shrink = true;
			OptionMap::Iterator shrink_option = bbcode_options.find("shrink");
			if (shrink_option) {
				shrink = (shrink_option->value == "true");
			}

			OptionMap::Iterator expand_option = bbcode_options.find("expand");
			if (expand_option) {
				int ratio = expand_option->value.to_int();
				if (ratio < 1) {
					ratio = 1;
				}
				set_table_column_expand(get_current_table_column(), true, ratio, shrink);
			}

			push_cell();
			const Color fallback_color = Color(0, 0, 0, 0);

			OptionMap::Iterator border_option = bbcode_options.find("border");
			if (border_option) {
				Color color = Color::from_string(border_option->value, fallback_color);
				set_cell_border_color(color);
			}
			OptionMap::Iterator bg_option = bbcode_options.find("bg");
			if (bg_option) {
				Vector<String> subtag_b = _split_unquoted(bg_option->value, U',');
				_normalize_subtags(subtag_b);

				if (subtag_b.size() == 2) {
					Color color1 = Color::from_string(subtag_b[0], fallback_color);
					Color color2 = Color::from_string(subtag_b[1], fallback_color);
					set_cell_row_background_color(color1, color2);
				}
				if (subtag_b.size() == 1) {
					Color color1 = Color::from_string(bg_option->value, fallback_color);
					set_cell_row_background_color(color1, color1);
				}
			}
			OptionMap::Iterator padding_option = bbcode_options.find("padding");
			if (padding_option) {
				Vector<String> subtag_b = _split_unquoted(padding_option->value, U',');
				_normalize_subtags(subtag_b);

				if (subtag_b.size() == 4) {
					set_cell_padding(Rect2(subtag_b[0].to_float(), subtag_b[1].to_float(), subtag_b[2].to_float(), subtag_b[3].to_float()));
				}
			}

			pos = brk_end + 1;
			tag_stack.push_front("cell");
		} else if (tag == "u") {
			//use underline
			push_underline();
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "s") {
			//use strikethrough
			push_strikethrough();
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("char=")) {
			int32_t char_code = _get_tag_value(tag).hex_to_int();
			add_text(String::chr(char_code));
			pos = brk_end + 1;
		} else if (tag == "lb") {
			add_text("[");
			pos = brk_end + 1;
		} else if (tag == "rb") {
			add_text("]");
			pos = brk_end + 1;
		} else if (tag == "lrm") {
			add_text(String::chr(0x200E));
			pos = brk_end + 1;
		} else if (tag == "rlm") {
			add_text(String::chr(0x200F));
			pos = brk_end + 1;
		} else if (tag == "lre") {
			add_text(String::chr(0x202A));
			pos = brk_end + 1;
		} else if (tag == "rle") {
			add_text(String::chr(0x202B));
			pos = brk_end + 1;
		} else if (tag == "lro") {
			add_text(String::chr(0x202D));
			pos = brk_end + 1;
		} else if (tag == "rlo") {
			add_text(String::chr(0x202E));
			pos = brk_end + 1;
		} else if (tag == "pdf") {
			add_text(String::chr(0x202C));
			pos = brk_end + 1;
		} else if (tag == "alm") {
			add_text(String::chr(0x061c));
			pos = brk_end + 1;
		} else if (tag == "lri") {
			add_text(String::chr(0x2066));
			pos = brk_end + 1;
		} else if (tag == "rli") {
			add_text(String::chr(0x2027));
			pos = brk_end + 1;
		} else if (tag == "fsi") {
			add_text(String::chr(0x2068));
			pos = brk_end + 1;
		} else if (tag == "pdi") {
			add_text(String::chr(0x2069));
			pos = brk_end + 1;
		} else if (tag == "zwj") {
			add_text(String::chr(0x200D));
			pos = brk_end + 1;
		} else if (tag == "zwnj") {
			add_text(String::chr(0x200C));
			pos = brk_end + 1;
		} else if (tag == "wj") {
			add_text(String::chr(0x2060));
			pos = brk_end + 1;
		} else if (tag == "shy") {
			add_text(String::chr(0x00AD));
			pos = brk_end + 1;
		} else if (tag == "center") {
			push_paragraph(HORIZONTAL_ALIGNMENT_CENTER, text_direction, language, st_parser, default_jst_flags, default_tab_stops);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "fill") {
			push_paragraph(HORIZONTAL_ALIGNMENT_FILL, text_direction, language, st_parser, default_jst_flags, default_tab_stops);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "left") {
			push_paragraph(HORIZONTAL_ALIGNMENT_LEFT, text_direction, language, st_parser, default_jst_flags, default_tab_stops);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "right") {
			push_paragraph(HORIZONTAL_ALIGNMENT_RIGHT, text_direction, language, st_parser, default_jst_flags, default_tab_stops);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "ul") {
			current_frame->indent_level++;
			push_list(current_frame->indent_level, LIST_DOTS, false);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("ul bullet=")) {
			String bullet = _get_tag_value(tag);
			current_frame->indent_level++;
			push_list(current_frame->indent_level, LIST_DOTS, false, bullet);
			pos = brk_end + 1;
			tag_stack.push_front("ul");
		} else if ((tag == "ol") || (tag == "ol type=1")) {
			current_frame->indent_level++;
			push_list(current_frame->indent_level, LIST_NUMBERS, false);
			pos = brk_end + 1;
			tag_stack.push_front("ol");
		} else if (tag == "ol type=a") {
			current_frame->indent_level++;
			push_list(current_frame->indent_level, LIST_LETTERS, false);
			pos = brk_end + 1;
			tag_stack.push_front("ol");
		} else if (tag == "ol type=A") {
			current_frame->indent_level++;
			push_list(current_frame->indent_level, LIST_LETTERS, true);
			pos = brk_end + 1;
			tag_stack.push_front("ol");
		} else if (tag == "ol type=i") {
			current_frame->indent_level++;
			push_list(current_frame->indent_level, LIST_ROMAN, false);
			pos = brk_end + 1;
			tag_stack.push_front("ol");
		} else if (tag == "ol type=I") {
			current_frame->indent_level++;
			push_list(current_frame->indent_level, LIST_ROMAN, true);
			pos = brk_end + 1;
			tag_stack.push_front("ol");
		} else if (tag == "indent") {
			current_frame->indent_level++;
			push_indent(current_frame->indent_level);
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("lang=")) {
			String lang = _get_tag_value(tag).unquote();
			push_language(lang);
			pos = brk_end + 1;
			tag_stack.push_front("lang");
		} else if (tag == "p") {
			push_paragraph(HORIZONTAL_ALIGNMENT_LEFT);
			pos = brk_end + 1;
			tag_stack.push_front("p");
		} else if (tag.begins_with("p ")) {
			HorizontalAlignment alignment = HORIZONTAL_ALIGNMENT_LEFT;
			Control::TextDirection dir = Control::TEXT_DIRECTION_INHERITED;
			String lang = language;
			PackedFloat32Array tab_stops = default_tab_stops;
			TextServer::StructuredTextParser st_parser_type = TextServer::STRUCTURED_TEXT_DEFAULT;
			BitField<TextServer::JustificationFlag> jst_flags = default_jst_flags;

			OptionMap::Iterator justification_flags_option = bbcode_options.find("justification_flags");
			if (!justification_flags_option) {
				justification_flags_option = bbcode_options.find("jst");
			}
			if (justification_flags_option) {
				Vector<String> subtag_b = _split_unquoted(justification_flags_option->value, U',');
				jst_flags = 0; // Clear flags.
				for (const String &E : subtag_b) {
					if (E == "kashida" || E == "k") {
						jst_flags.set_flag(TextServer::JUSTIFICATION_KASHIDA);
					} else if (E == "word" || E == "w") {
						jst_flags.set_flag(TextServer::JUSTIFICATION_WORD_BOUND);
					} else if (E == "trim" || E == "tr") {
						jst_flags.set_flag(TextServer::JUSTIFICATION_TRIM_EDGE_SPACES);
					} else if (E == "after_last_tab" || E == "lt") {
						jst_flags.set_flag(TextServer::JUSTIFICATION_AFTER_LAST_TAB);
					} else if (E == "skip_last" || E == "sl") {
						jst_flags.set_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE);
					} else if (E == "skip_last_with_chars" || E == "sv") {
						jst_flags.set_flag(TextServer::JUSTIFICATION_SKIP_LAST_LINE_WITH_VISIBLE_CHARS);
					} else if (E == "do_not_skip_single" || E == "ns") {
						jst_flags.set_flag(TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE);
					}
				}
			}
			OptionMap::Iterator tab_stops_option = bbcode_options.find("tab_stops");
			if (tab_stops_option) {
				Vector<String> splitters;
				splitters.push_back(",");
				splitters.push_back(";");
				tab_stops = tab_stops_option->value.split_floats_mk(splitters);
			}
			OptionMap::Iterator align_option = bbcode_options.find("align");
			if (align_option) {
				if (align_option->value == "l" || align_option->value == "left") {
					alignment = HORIZONTAL_ALIGNMENT_LEFT;
				} else if (align_option->value == "c" || align_option->value == "center") {
					alignment = HORIZONTAL_ALIGNMENT_CENTER;
				} else if (align_option->value == "r" || align_option->value == "right") {
					alignment = HORIZONTAL_ALIGNMENT_RIGHT;
				} else if (align_option->value == "f" || align_option->value == "fill") {
					alignment = HORIZONTAL_ALIGNMENT_FILL;
				}
			}
			OptionMap::Iterator direction_option = bbcode_options.find("direction");
			if (!direction_option) {
				direction_option = bbcode_options.find("dir");
			}
			if (direction_option) {
				if (direction_option->value == "a" || direction_option->value == "auto") {
					dir = Control::TEXT_DIRECTION_AUTO;
				} else if (direction_option->value == "l" || direction_option->value == "ltr") {
					dir = Control::TEXT_DIRECTION_LTR;
				} else if (direction_option->value == "r" || direction_option->value == "rtl") {
					dir = Control::TEXT_DIRECTION_RTL;
				}
			}
			OptionMap::Iterator language_option = bbcode_options.find("language");
			if (!language_option) {
				language_option = bbcode_options.find("lang");
			}
			if (language_option) {
				lang = language_option->value;
			}
			OptionMap::Iterator bidi_override_option = bbcode_options.find("bidi_override");
			if (!bidi_override_option) {
				bidi_override_option = bbcode_options.find("st");
			}
			if (bidi_override_option) {
				if (bidi_override_option->value == "d" || bidi_override_option->value == "default") {
					st_parser_type = TextServer::STRUCTURED_TEXT_DEFAULT;
				} else if (bidi_override_option->value == "u" || bidi_override_option->value == "uri") {
					st_parser_type = TextServer::STRUCTURED_TEXT_URI;
				} else if (bidi_override_option->value == "f" || bidi_override_option->value == "file") {
					st_parser_type = TextServer::STRUCTURED_TEXT_FILE;
				} else if (bidi_override_option->value == "e" || bidi_override_option->value == "email") {
					st_parser_type = TextServer::STRUCTURED_TEXT_EMAIL;
				} else if (bidi_override_option->value == "l" || bidi_override_option->value == "list") {
					st_parser_type = TextServer::STRUCTURED_TEXT_LIST;
				} else if (bidi_override_option->value == "n" || bidi_override_option->value == "gdscript") {
					st_parser_type = TextServer::STRUCTURED_TEXT_GDSCRIPT;
				} else if (bidi_override_option->value == "c" || bidi_override_option->value == "custom") {
					st_parser_type = TextServer::STRUCTURED_TEXT_CUSTOM;
				}
			}

			push_paragraph(alignment, dir, lang, st_parser_type, jst_flags, tab_stops);
			pos = brk_end + 1;
			tag_stack.push_front("p");
		} else if (tag == "url") {
			int end = bbcode.find_char('[', brk_end);
			if (end == -1) {
				end = bbcode.length();
			}
			String url = bbcode.substr(brk_end + 1, end - brk_end - 1).unquote();
			push_meta(url, META_UNDERLINE_ALWAYS);

			pos = brk_end + 1;
			tag_stack.push_front(tag);

		} else if (tag.begins_with("url ")) {
			String url;
			MetaUnderline underline = META_UNDERLINE_ALWAYS;
			String tooltip;

			OptionMap::Iterator underline_option = bbcode_options.find("underline");
			if (underline_option) {
				if (underline_option->value == "never") {
					underline = META_UNDERLINE_NEVER;
				} else if (underline_option->value == "always") {
					underline = META_UNDERLINE_ALWAYS;
				} else if (underline_option->value == "hover") {
					underline = META_UNDERLINE_ON_HOVER;
				}
			}
			OptionMap::Iterator tooltip_option = bbcode_options.find("tooltip");
			if (tooltip_option) {
				tooltip = tooltip_option->value;
			}
			OptionMap::Iterator href_option = bbcode_options.find("href");
			if (href_option) {
				url = href_option->value;
			}

			push_meta(url, underline, tooltip);

			pos = brk_end + 1;
			tag_stack.push_front("url");
		} else if (tag.begins_with("url=")) {
			String url = _get_tag_value(tag).unquote();
			push_meta(url, META_UNDERLINE_ALWAYS);
			pos = brk_end + 1;
			tag_stack.push_front("url");
		} else if (tag.begins_with("hint=")) {
			String description = _get_tag_value(tag).unquote();
			push_hint(description);
			pos = brk_end + 1;
			tag_stack.push_front("hint");
		} else if (tag.begins_with("dropcap")) {
			int fs = theme_cache.normal_font_size * 3;
			Ref<Font> f = theme_cache.normal_font;
			Color color = theme_cache.default_color;
			Color outline_color = theme_cache.font_outline_color;
			int outline_size = theme_cache.outline_size;
			Rect2 dropcap_margins;

			OptionMap::Iterator font_option = bbcode_options.find("font");
			if (!font_option) {
				font_option = bbcode_options.find("f");
			}
			if (font_option) {
				const String &fnt = font_option->value;
				Ref<Font> font = ResourceLoader::load(fnt, "Font");
				if (font.is_valid()) {
					f = font;
				}
			}
			OptionMap::Iterator font_size_option = bbcode_options.find("font_size");
			if (font_size_option) {
				fs = font_size_option->value.to_int();
			}
			OptionMap::Iterator margins_option = bbcode_options.find("margins");
			if (margins_option) {
				Vector<String> subtag_b = _split_unquoted(margins_option->value, U',');
				_normalize_subtags(subtag_b);

				if (subtag_b.size() == 4) {
					dropcap_margins.position.x = subtag_b[0].to_float();
					dropcap_margins.position.y = subtag_b[1].to_float();
					dropcap_margins.size.x = subtag_b[2].to_float();
					dropcap_margins.size.y = subtag_b[3].to_float();
				}
			}
			OptionMap::Iterator outline_size_option = bbcode_options.find("outline_size");
			if (outline_size_option) {
				outline_size = outline_size_option->value.to_int();
			}
			OptionMap::Iterator color_option = bbcode_options.find("color");
			if (color_option) {
				color = Color::from_string(color_option->value, color);
			}
			OptionMap::Iterator outline_color_option = bbcode_options.find("outline_color");
			if (outline_color_option) {
				outline_color = Color::from_string(outline_color_option->value, outline_color);
			}

			int end = bbcode.find_char('[', brk_end);
			if (end == -1) {
				end = bbcode.length();
			}

			String dc_txt = bbcode.substr(brk_end + 1, end - brk_end - 1);

			push_dropcap(dc_txt, f, fs, dropcap_margins, color, outline_size, outline_color);

			pos = end;
			tag_stack.push_front(bbcode_name);
		} else if (tag.begins_with("img")) {
			int alignment = INLINE_ALIGNMENT_CENTER;
			if (tag.begins_with("img=")) {
				Vector<String> subtag = _split_unquoted(_get_tag_value(tag), U',');
				_normalize_subtags(subtag);

				if (subtag.size() > 1) {
					if (subtag[0] == "top" || subtag[0] == "t") {
						alignment = INLINE_ALIGNMENT_TOP_TO;
					} else if (subtag[0] == "center" || subtag[0] == "c") {
						alignment = INLINE_ALIGNMENT_CENTER_TO;
					} else if (subtag[0] == "bottom" || subtag[0] == "b") {
						alignment = INLINE_ALIGNMENT_BOTTOM_TO;
					}
					if (subtag[1] == "top" || subtag[1] == "t") {
						alignment |= INLINE_ALIGNMENT_TO_TOP;
					} else if (subtag[1] == "center" || subtag[1] == "c") {
						alignment |= INLINE_ALIGNMENT_TO_CENTER;
					} else if (subtag[1] == "baseline" || subtag[1] == "l") {
						alignment |= INLINE_ALIGNMENT_TO_BASELINE;
					} else if (subtag[1] == "bottom" || subtag[1] == "b") {
						alignment |= INLINE_ALIGNMENT_TO_BOTTOM;
					}
				} else if (subtag.size() > 0) {
					if (subtag[0] == "top" || subtag[0] == "t") {
						alignment = INLINE_ALIGNMENT_TOP;
					} else if (subtag[0] == "center" || subtag[0] == "c") {
						alignment = INLINE_ALIGNMENT_CENTER;
					} else if (subtag[0] == "bottom" || subtag[0] == "b") {
						alignment = INLINE_ALIGNMENT_BOTTOM;
					}
				}
			}

			int end = bbcode.find_char('[', brk_end);
			if (end == -1) {
				end = bbcode.length();
			}

			String image = bbcode.substr(brk_end + 1, end - brk_end - 1);

			Ref<Texture2D> texture = ResourceLoader::load(image, "Texture2D");
			if (texture.is_valid()) {
				Rect2 region;
				OptionMap::Iterator region_option = bbcode_options.find("region");
				if (region_option) {
					Vector<String> region_values = _split_unquoted(region_option->value, U',');
					if (region_values.size() == 4) {
						region.position.x = region_values[0].to_float();
						region.position.y = region_values[1].to_float();
						region.size.x = region_values[2].to_float();
						region.size.y = region_values[3].to_float();
					}
				}

				Color color = Color(1.0, 1.0, 1.0);
				OptionMap::Iterator color_option = bbcode_options.find("color");
				if (color_option) {
					color = Color::from_string(color_option->value, color);
				}

				int width = 0;
				int height = 0;
				bool pad = false;
				String tooltip;
				bool size_in_percent = false;
				if (!bbcode_value.is_empty()) {
					int sep = bbcode_value.find_char('x');
					if (sep == -1) {
						width = bbcode_value.to_int();
					} else {
						width = bbcode_value.substr(0, sep).to_int();
						height = bbcode_value.substr(sep + 1).to_int();
					}
				} else {
					OptionMap::Iterator align_option = bbcode_options.find("align");
					if (align_option) {
						Vector<String> subtag = _split_unquoted(align_option->value, U',');
						_normalize_subtags(subtag);

						if (subtag.size() > 1) {
							if (subtag[0] == "top" || subtag[0] == "t") {
								alignment = INLINE_ALIGNMENT_TOP_TO;
							} else if (subtag[0] == "center" || subtag[0] == "c") {
								alignment = INLINE_ALIGNMENT_CENTER_TO;
							} else if (subtag[0] == "bottom" || subtag[0] == "b") {
								alignment = INLINE_ALIGNMENT_BOTTOM_TO;
							}
							if (subtag[1] == "top" || subtag[1] == "t") {
								alignment |= INLINE_ALIGNMENT_TO_TOP;
							} else if (subtag[1] == "center" || subtag[1] == "c") {
								alignment |= INLINE_ALIGNMENT_TO_CENTER;
							} else if (subtag[1] == "baseline" || subtag[1] == "l") {
								alignment |= INLINE_ALIGNMENT_TO_BASELINE;
							} else if (subtag[1] == "bottom" || subtag[1] == "b") {
								alignment |= INLINE_ALIGNMENT_TO_BOTTOM;
							}
						} else if (subtag.size() > 0) {
							if (subtag[0] == "top" || subtag[0] == "t") {
								alignment = INLINE_ALIGNMENT_TOP;
							} else if (subtag[0] == "center" || subtag[0] == "c") {
								alignment = INLINE_ALIGNMENT_CENTER;
							} else if (subtag[0] == "bottom" || subtag[0] == "b") {
								alignment = INLINE_ALIGNMENT_BOTTOM;
							}
						}
					}
					OptionMap::Iterator width_option = bbcode_options.find("width");
					if (width_option) {
						width = width_option->value.to_int();
						if (width_option->value.ends_with("%")) {
							size_in_percent = true;
						}
					}

					OptionMap::Iterator height_option = bbcode_options.find("height");
					if (height_option) {
						height = height_option->value.to_int();
						if (height_option->value.ends_with("%")) {
							size_in_percent = true;
						}
					}

					OptionMap::Iterator tooltip_option = bbcode_options.find("tooltip");
					if (tooltip_option) {
						tooltip = tooltip_option->value;
					}

					OptionMap::Iterator pad_option = bbcode_options.find("pad");
					if (pad_option) {
						pad = (pad_option->value == "true");
					}
				}

				add_image(texture, width, height, color, (InlineAlignment)alignment, region, Variant(), pad, tooltip, size_in_percent);
			}

			pos = end;
			tag_stack.push_front(bbcode_name);
		} else if (tag.begins_with("color=")) {
			String color_str = _get_tag_value(tag).unquote();
			Color color = Color::from_string(color_str, theme_cache.default_color);
			push_color(color);
			pos = brk_end + 1;
			tag_stack.push_front("color");

		} else if (tag.begins_with("outline_color=")) {
			String color_str = _get_tag_value(tag).unquote();
			Color color = Color::from_string(color_str, theme_cache.default_color);
			push_outline_color(color);
			pos = brk_end + 1;
			tag_stack.push_front("outline_color");

		} else if (tag.begins_with("font_size=")) {
			int fnt_size = _get_tag_value(tag).to_int();
			push_font_size(fnt_size);
			pos = brk_end + 1;
			tag_stack.push_front("font_size");

		} else if (tag.begins_with("opentype_features=") || tag.begins_with("otf=")) {
			int value_pos = tag.find_char('=');
			String fnt_ftr = tag.substr(value_pos + 1);
			Vector<String> subtag = fnt_ftr.split(",");
			_normalize_subtags(subtag);

			if (subtag.size() > 0) {
				Ref<Font> font = theme_cache.normal_font;
				DefaultFont def_font = NORMAL_FONT;

				ItemFont *font_it = _find_font(current);
				if (font_it) {
					if (font_it->font.is_valid()) {
						font = font_it->font;
						def_font = font_it->def_font;
					}
				}
				Dictionary features;
				for (int i = 0; i < subtag.size(); i++) {
					Vector<String> subtag_a = subtag[i].split("=");
					_normalize_subtags(subtag_a);

					if (subtag_a.size() == 2) {
						features[TS->name_to_tag(subtag_a[0])] = subtag_a[1].to_int();
					} else if (subtag_a.size() == 1) {
						features[TS->name_to_tag(subtag_a[0])] = 1;
					}
				}

				Ref<FontVariation> fc;
				fc.instantiate();

				fc->set_base_font(font);
				fc->set_opentype_features(features);

				if (def_font != CUSTOM_FONT) {
					_push_def_font_var(def_font, fc);
				} else {
					push_font(fc);
				}
			}
			pos = brk_end + 1;
			tag_stack.push_front(tag.substr(0, value_pos));

		} else if (tag.begins_with("font=")) {
			String fnt = _get_tag_value(tag).unquote();

			Ref<Font> fc = ResourceLoader::load(fnt, "Font");
			if (fc.is_valid()) {
				push_font(fc);
			}

			pos = brk_end + 1;
			tag_stack.push_front("font");

		} else if (tag.begins_with("font ")) {
			Ref<Font> font = theme_cache.normal_font;
			DefaultFont def_font = NORMAL_FONT;
			int fnt_size = -1;

			ItemFont *font_it = _find_font(current);
			if (font_it) {
				if (font_it->font.is_valid()) {
					font = font_it->font;
					def_font = font_it->def_font;
				}
			}

			Ref<FontVariation> fc;
			fc.instantiate();

			OptionMap::Iterator name_option = bbcode_options.find("name");
			if (!name_option) {
				name_option = bbcode_options.find("n");
			}
			if (name_option) {
				const String &fnt = name_option->value;
				Ref<Font> font_data = ResourceLoader::load(fnt, "Font");
				if (font_data.is_valid()) {
					font = font_data;
					def_font = CUSTOM_FONT;
				}
			}
			OptionMap::Iterator size_option = bbcode_options.find("size");
			if (!size_option) {
				size_option = bbcode_options.find("s");
			}
			if (size_option) {
				fnt_size = size_option->value.to_int();
			}
			OptionMap::Iterator glyph_spacing_option = bbcode_options.find("glyph_spacing");
			if (!glyph_spacing_option) {
				glyph_spacing_option = bbcode_options.find("gl");
			}
			if (glyph_spacing_option) {
				int spacing = glyph_spacing_option->value.to_int();
				fc->set_spacing(TextServer::SPACING_GLYPH, spacing);
			}
			OptionMap::Iterator space_spacing_option = bbcode_options.find("space_spacing");
			if (!space_spacing_option) {
				space_spacing_option = bbcode_options.find("sp");
			}
			if (space_spacing_option) {
				int spacing = space_spacing_option->value.to_int();
				fc->set_spacing(TextServer::SPACING_SPACE, spacing);
			}
			OptionMap::Iterator top_spacing_option = bbcode_options.find("top_spacing");
			if (!top_spacing_option) {
				top_spacing_option = bbcode_options.find("top");
			}
			if (top_spacing_option) {
				int spacing = top_spacing_option->value.to_int();
				fc->set_spacing(TextServer::SPACING_TOP, spacing);
			}
			OptionMap::Iterator bottom_spacing_option = bbcode_options.find("bottom_spacing");
			if (!bottom_spacing_option) {
				bottom_spacing_option = bbcode_options.find("bt");
			}
			if (bottom_spacing_option) {
				int spacing = bottom_spacing_option->value.to_int();
				fc->set_spacing(TextServer::SPACING_BOTTOM, spacing);
			}
			OptionMap::Iterator embolden_option = bbcode_options.find("embolden");
			if (!embolden_option) {
				embolden_option = bbcode_options.find("emb");
			}
			if (embolden_option) {
				float emb = embolden_option->value.to_float();
				fc->set_variation_embolden(emb);
			}
			OptionMap::Iterator face_index_option = bbcode_options.find("face_index");
			if (!face_index_option) {
				face_index_option = bbcode_options.find("fi");
			}
			if (face_index_option) {
				int fi = face_index_option->value.to_int();
				fc->set_variation_face_index(fi);
			}
			OptionMap::Iterator slant_option = bbcode_options.find("slant");
			if (!slant_option) {
				slant_option = bbcode_options.find("sln");
			}
			if (slant_option) {
				float slant = slant_option->value.to_float();
				fc->set_variation_transform(Transform2D(1.0, slant, 0.0, 1.0, 0.0, 0.0));
			}
			OptionMap::Iterator opentype_variation_option = bbcode_options.find("opentype_variation");
			if (!opentype_variation_option) {
				opentype_variation_option = bbcode_options.find("otv");
			}
			if (opentype_variation_option) {
				Dictionary variations;
				if (!opentype_variation_option->value.is_empty()) {
					Vector<String> variation_tags = opentype_variation_option->value.split(",");
					for (int j = 0; j < variation_tags.size(); j++) {
						Vector<String> subtag_b = variation_tags[j].split("=");
						_normalize_subtags(subtag_b);

						if (subtag_b.size() == 2) {
							variations[TS->name_to_tag(subtag_b[0])] = subtag_b[1].to_float();
						}
					}
					fc->set_variation_opentype(variations);
				}
			}
			OptionMap::Iterator opentype_features_option = bbcode_options.find("opentype_features");
			if (!opentype_features_option) {
				opentype_features_option = bbcode_options.find("otf");
			}
			if (opentype_features_option) {
				Dictionary features;
				if (!opentype_features_option->value.is_empty()) {
					Vector<String> feature_tags = opentype_features_option->value.split(",");
					for (int j = 0; j < feature_tags.size(); j++) {
						Vector<String> subtag_b = feature_tags[j].split("=");
						_normalize_subtags(subtag_b);

						if (subtag_b.size() == 2) {
							features[TS->name_to_tag(subtag_b[0])] = subtag_b[1].to_float();
						} else if (subtag_b.size() == 1) {
							features[TS->name_to_tag(subtag_b[0])] = 1;
						}
					}
					fc->set_opentype_features(features);
				}
			}

			fc->set_base_font(font);

			if (def_font != CUSTOM_FONT) {
				_push_def_font_var(def_font, fc, fnt_size);
			} else {
				push_font(fc, fnt_size);
			}

			pos = brk_end + 1;
			tag_stack.push_front("font");

		} else if (tag.begins_with("outline_size=")) {
			int fnt_size = _get_tag_value(tag).to_int();
			if (fnt_size > 0) {
				push_outline_size(fnt_size);
			}
			pos = brk_end + 1;
			tag_stack.push_front("outline_size");

		} else if (bbcode_name == "fade") {
			int start_index = 0;
			OptionMap::Iterator start_option = bbcode_options.find("start");
			if (start_option) {
				start_index = start_option->value.to_int();
			}

			int length = 10;
			OptionMap::Iterator length_option = bbcode_options.find("length");
			if (length_option) {
				length = length_option->value.to_int();
			}

			push_fade(start_index, length);
			pos = brk_end + 1;
			tag_stack.push_front("fade");
		} else if (bbcode_name == "shake") {
			int strength = 5;
			OptionMap::Iterator strength_option = bbcode_options.find("level");
			if (strength_option) {
				strength = strength_option->value.to_int();
			}

			float rate = 20.0f;
			OptionMap::Iterator rate_option = bbcode_options.find("rate");
			if (rate_option) {
				rate = rate_option->value.to_float();
			}

			bool connected = true;
			OptionMap::Iterator connected_option = bbcode_options.find("connected");
			if (connected_option) {
				connected = connected_option->value.to_int();
			}

			push_shake(strength, rate, connected);
			pos = brk_end + 1;
			tag_stack.push_front("shake");
			set_process_internal(true);
		} else if (bbcode_name == "wave") {
			float amplitude = 20.0f;
			OptionMap::Iterator amplitude_option = bbcode_options.find("amp");
			if (amplitude_option) {
				amplitude = amplitude_option->value.to_float();
			}

			float period = 5.0f;
			OptionMap::Iterator period_option = bbcode_options.find("freq");
			if (period_option) {
				period = period_option->value.to_float();
			}

			bool connected = true;
			OptionMap::Iterator connected_option = bbcode_options.find("connected");
			if (connected_option) {
				connected = connected_option->value.to_int();
			}

			push_wave(period, amplitude, connected);
			pos = brk_end + 1;
			tag_stack.push_front("wave");
			set_process_internal(true);
		} else if (bbcode_name == "tornado") {
			float radius = 10.0f;
			OptionMap::Iterator radius_option = bbcode_options.find("radius");
			if (radius_option) {
				radius = radius_option->value.to_float();
			}

			float frequency = 1.0f;
			OptionMap::Iterator frequency_option = bbcode_options.find("freq");
			if (frequency_option) {
				frequency = frequency_option->value.to_float();
			}

			bool connected = true;
			OptionMap::Iterator connected_option = bbcode_options.find("connected");
			if (connected_option) {
				connected = connected_option->value.to_int();
			}

			push_tornado(frequency, radius, connected);
			pos = brk_end + 1;
			tag_stack.push_front("tornado");
			set_process_internal(true);
		} else if (bbcode_name == "rainbow") {
			float saturation = 0.8f;
			OptionMap::Iterator saturation_option = bbcode_options.find("sat");
			if (saturation_option) {
				saturation = saturation_option->value.to_float();
			}

			float value = 0.8f;
			OptionMap::Iterator value_option = bbcode_options.find("val");
			if (value_option) {
				value = value_option->value.to_float();
			}

			float frequency = 1.0f;
			OptionMap::Iterator frequency_option = bbcode_options.find("freq");
			if (frequency_option) {
				frequency = frequency_option->value.to_float();
			}

			float speed = 1.0f;
			OptionMap::Iterator speed_option = bbcode_options.find("speed");
			if (speed_option) {
				speed = speed_option->value.to_float();
			}

			push_rainbow(saturation, value, frequency, speed);
			pos = brk_end + 1;
			tag_stack.push_front("rainbow");
			set_process_internal(true);
		} else if (bbcode_name == "pulse") {
			Color color = Color(1, 1, 1, 0.25);
			OptionMap::Iterator color_option = bbcode_options.find("color");
			if (color_option) {
				color = Color::from_string(color_option->value, color);
			}

			float frequency = 1.0;
			OptionMap::Iterator freq_option = bbcode_options.find("freq");
			if (freq_option) {
				frequency = freq_option->value.to_float();
			}

			float ease = -2.0;
			OptionMap::Iterator ease_option = bbcode_options.find("ease");
			if (ease_option) {
				ease = ease_option->value.to_float();
			}

			push_pulse(color, frequency, ease);
			pos = brk_end + 1;
			tag_stack.push_front("pulse");
			set_process_internal(true);
		} else if (tag.begins_with("bgcolor=")) {
			String color_str = _get_tag_value(tag).unquote();
			Color color = Color::from_string(color_str, theme_cache.default_color);

			push_bgcolor(color);
			pos = brk_end + 1;
			tag_stack.push_front("bgcolor");

		} else if (tag.begins_with("fgcolor=")) {
			String color_str = _get_tag_value(tag).unquote();
			Color color = Color::from_string(color_str, theme_cache.default_color);

			push_fgcolor(color);
			pos = brk_end + 1;
			tag_stack.push_front("fgcolor");

		} else {
			Vector<String> &expr = split_tag_block;
			if (expr.size() < 1) {
				add_text("[");
				pos = brk_pos + 1;
			} else {
				String identifier = expr[0];
				expr.remove_at(0);
				Dictionary properties = parse_expressions_for_values(expr);
				Ref<RichTextEffect> effect = _get_custom_effect_by_code(identifier);

				if (effect.is_valid()) {
					push_customfx(effect, properties);
					pos = brk_end + 1;
					tag_stack.push_front(identifier);
				} else {
					add_text("["); //ignore
					pos = brk_pos + 1;
				}
			}
		}
	}

	parsing_bbcode.store(false);
}

void RichTextLabel::scroll_to_selection() {
	float line_offset = get_selection_line_offset();
	if (line_offset != -1.0) {
		vscroll->set_value(line_offset);
	}
}

void RichTextLabel::scroll_to_paragraph(int p_paragraph) {
	_validate_line_caches();

	if (p_paragraph <= 0) {
		vscroll->set_value(0);
	} else if (p_paragraph >= main->first_invalid_line.load()) {
		vscroll->set_value(vscroll->get_max());
	} else {
		vscroll->set_value(main->lines[p_paragraph].offset.y);
	}
}

int RichTextLabel::get_paragraph_count() const {
	return main->lines.size();
}

int RichTextLabel::get_visible_paragraph_count() const {
	if (!is_visible()) {
		return 0;
	}

	const_cast<RichTextLabel *>(this)->_validate_line_caches();
	return visible_paragraph_count;
}

void RichTextLabel::scroll_to_line(int p_line) {
	if (p_line <= 0) {
		vscroll->set_value(0);
		return;
	}
	_validate_line_caches();

	int line_count = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		if ((line_count <= p_line) && (line_count + main->lines[i].text_buf->get_line_count() >= p_line)) {
			float line_offset = 0.f;
			for (int j = 0; j < p_line - line_count; j++) {
				line_offset += main->lines[i].text_buf->get_line_ascent(j) + main->lines[i].text_buf->get_line_descent(j) + theme_cache.line_separation;
			}
			vscroll->set_value(main->lines[i].offset.y + line_offset);
			return;
		}
		line_count += main->lines[i].text_buf->get_line_count();
	}
	vscroll->set_value(vscroll->get_max());
}

float RichTextLabel::get_line_offset(int p_line) {
	_validate_line_caches();

	int line_count = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		if ((line_count <= p_line) && (p_line <= line_count + main->lines[i].text_buf->get_line_count())) {
			float line_offset = 0.f;
			for (int j = 0; j < p_line - line_count; j++) {
				line_offset += main->lines[i].text_buf->get_line_ascent(j) + main->lines[i].text_buf->get_line_descent(j) + theme_cache.line_separation;
			}
			return main->lines[i].offset.y + line_offset;
		}
		line_count += main->lines[i].text_buf->get_line_count();
	}
	return 0;
}

float RichTextLabel::get_paragraph_offset(int p_paragraph) {
	_validate_line_caches();

	int to_line = main->first_invalid_line.load();
	if (0 <= p_paragraph && p_paragraph < to_line) {
		return main->lines[p_paragraph].offset.y;
	}
	return 0;
}

int RichTextLabel::get_line_count() const {
	const_cast<RichTextLabel *>(this)->_validate_line_caches();

	int line_count = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		line_count += main->lines[i].text_buf->get_line_count();
	}
	return line_count;
}

Vector2i RichTextLabel::get_line_range(int p_line) {
	const_cast<RichTextLabel *>(this)->_validate_line_caches();

	int line_count = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		int lc = main->lines[i].text_buf->get_line_count();

		if (p_line < line_count + lc) {
			Vector2i char_offset = Vector2i(main->lines[i].char_offset, main->lines[i].char_offset);
			Vector2i line_range = main->lines[i].text_buf->get_line_range(p_line - line_count);
			return char_offset + line_range;
		}

		line_count += lc;
	}
	return Vector2i();
}

int RichTextLabel::get_visible_line_count() const {
	if (!is_visible()) {
		return 0;
	}
	const_cast<RichTextLabel *>(this)->_validate_line_caches();

	return visible_line_count;
}

void RichTextLabel::set_selection_enabled(bool p_enabled) {
	if (selection.enabled == p_enabled) {
		return;
	}

	selection.enabled = p_enabled;
	if (!p_enabled) {
		if (selection.active) {
			deselect();
		}
		set_focus_mode(FOCUS_NONE);
	} else {
		set_focus_mode(FOCUS_ALL);
	}
}

void RichTextLabel::set_deselect_on_focus_loss_enabled(const bool p_enabled) {
	if (deselect_on_focus_loss_enabled == p_enabled) {
		return;
	}

	deselect_on_focus_loss_enabled = p_enabled;
	if (p_enabled && selection.active && !has_focus()) {
		deselect();
	}
}

Variant RichTextLabel::get_drag_data(const Point2 &p_point) {
	Variant ret = Control::get_drag_data(p_point);
	if (ret != Variant()) {
		return ret;
	}

	if (selection.drag_attempt && selection.enabled) {
		String t = get_selected_text();
		Label *l = memnew(Label);
		l->set_text(t);
		l->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED); // Text is already translated.
		set_drag_preview(l);
		return t;
	}

	return Variant();
}

bool RichTextLabel::_is_click_inside_selection() const {
	if (selection.active && selection.enabled && selection.click_frame && selection.from_frame && selection.to_frame) {
		const Line &l_click = selection.click_frame->lines[selection.click_line];
		const Line &l_from = selection.from_frame->lines[selection.from_line];
		const Line &l_to = selection.to_frame->lines[selection.to_line];
		return (l_click.char_offset + selection.click_char >= l_from.char_offset + selection.from_char) && (l_click.char_offset + selection.click_char <= l_to.char_offset + selection.to_char);
	} else {
		return false;
	}
}

bool RichTextLabel::_search_table(ItemTable *p_table, List<Item *>::Element *p_from, const String &p_string, bool p_reverse_search) {
	List<Item *>::Element *E = p_from;
	while (E != nullptr) {
		ERR_CONTINUE(E->get()->type != ITEM_FRAME); // Children should all be frames.
		ItemFrame *frame = static_cast<ItemFrame *>(E->get());
		if (p_reverse_search) {
			for (int i = (int)frame->lines.size() - 1; i >= 0; i--) {
				if (_search_line(frame, i, p_string, -1, p_reverse_search)) {
					return true;
				}
			}
		} else {
			for (int i = 0; i < (int)frame->lines.size(); i++) {
				if (_search_line(frame, i, p_string, 0, p_reverse_search)) {
					return true;
				}
			}
		}
		E = p_reverse_search ? E->prev() : E->next();
	}
	return false;
}

bool RichTextLabel::_search_line(ItemFrame *p_frame, int p_line, const String &p_string, int p_char_idx, bool p_reverse_search) {
	ERR_FAIL_NULL_V(p_frame, false);
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)p_frame->lines.size(), false);

	Line &l = p_frame->lines[p_line];

	String txt;
	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	for (Item *it = l.from; it && it != it_to; it = _get_next_item(it)) {
		switch (it->type) {
			case ITEM_NEWLINE: {
				txt += "\n";
			} break;
			case ITEM_TEXT: {
				ItemText *t = static_cast<ItemText *>(it);
				txt += t->text;
			} break;
			case ITEM_IMAGE: {
				txt += " ";
			} break;
			case ITEM_TABLE: {
				ItemTable *table = static_cast<ItemTable *>(it);
				List<Item *>::Element *E = p_reverse_search ? table->subitems.back() : table->subitems.front();
				if (_search_table(table, E, p_string, p_reverse_search)) {
					return true;
				}
			} break;
			default:
				break;
		}
	}

	int sp = -1;
	if (p_reverse_search) {
		sp = txt.rfindn(p_string, p_char_idx);
	} else {
		sp = txt.findn(p_string, p_char_idx);
	}

	if (sp != -1) {
		selection.from_frame = p_frame;
		selection.from_line = p_line;
		selection.from_item = _get_item_at_pos(l.from, it_to, sp);
		selection.from_char = sp;
		selection.to_frame = p_frame;
		selection.to_line = p_line;
		selection.to_item = _get_item_at_pos(l.from, it_to, sp + p_string.length());
		selection.to_char = sp + p_string.length();
		selection.active = true;
		return true;
	}

	return false;
}

bool RichTextLabel::search(const String &p_string, bool p_from_selection, bool p_search_previous) {
	ERR_FAIL_COND_V(!selection.enabled, false);

	if (p_string.size() == 0) {
		selection.active = false;
		return false;
	}

	int char_idx = p_search_previous ? -1 : 0;
	int current_line = 0;
	int to_line = main->first_invalid_line.load();
	int ending_line = to_line - 1;
	if (p_from_selection && selection.active) {
		// First check to see if other results exist in current line
		char_idx = p_search_previous ? selection.from_char - 1 : selection.to_char;
		if (!(p_search_previous && char_idx < 0) &&
				_search_line(selection.from_frame, selection.from_line, p_string, char_idx, p_search_previous)) {
			scroll_to_selection();
			queue_redraw();
			return true;
		}
		char_idx = p_search_previous ? -1 : 0;

		// Next, check to see if the current search result is in a table
		if (selection.from_frame->parent != nullptr && selection.from_frame->parent->type == ITEM_TABLE) {
			// Find last search result in table
			ItemTable *parent_table = static_cast<ItemTable *>(selection.from_frame->parent);
			List<Item *>::Element *parent_element = p_search_previous ? parent_table->subitems.back() : parent_table->subitems.front();

			while (parent_element->get() != selection.from_frame) {
				parent_element = p_search_previous ? parent_element->prev() : parent_element->next();
				ERR_FAIL_NULL_V(parent_element, false);
			}

			// Search remainder of table
			if (!(p_search_previous && parent_element == parent_table->subitems.front()) &&
					parent_element != parent_table->subitems.back()) {
				parent_element = p_search_previous ? parent_element->prev() : parent_element->next(); // Don't want to search current item
				ERR_FAIL_NULL_V(parent_element, false);

				// Search for next element
				if (_search_table(parent_table, parent_element, p_string, p_search_previous)) {
					scroll_to_selection();
					queue_redraw();
					return true;
				}
			}
		}

		ending_line = selection.from_frame->line + selection.from_line;
		current_line = p_search_previous ? ending_line - 1 : ending_line + 1;
	} else if (p_search_previous) {
		current_line = ending_line;
		ending_line = 0;
	}

	// Search remainder of the file
	while (current_line != ending_line) {
		// Wrap around
		if (current_line < 0) {
			current_line = to_line - 1;
		} else if (current_line >= to_line) {
			current_line = 0;
		}

		if (_search_line(main, current_line, p_string, char_idx, p_search_previous)) {
			scroll_to_selection();
			queue_redraw();
			return true;
		}

		if (current_line != ending_line) {
			p_search_previous ? current_line-- : current_line++;
		}
	}

	if (p_from_selection && selection.active) {
		// Check contents of selection
		return _search_line(main, current_line, p_string, char_idx, p_search_previous);
	} else {
		return false;
	}
}

String RichTextLabel::_get_line_text(ItemFrame *p_frame, int p_line, Selection p_selection) const {
	String txt;

	ERR_FAIL_NULL_V(p_frame, txt);
	ERR_FAIL_COND_V(p_line < 0 || p_line >= (int)p_frame->lines.size(), txt);

	Line &l = p_frame->lines[p_line];

	Item *it_to = (p_line + 1 < (int)p_frame->lines.size()) ? p_frame->lines[p_line + 1].from : nullptr;
	int end_idx = 0;
	if (it_to != nullptr) {
		end_idx = it_to->index;
	} else {
		for (Item *it = l.from; it; it = _get_next_item(it)) {
			end_idx = it->index + 1;
		}
	}
	for (Item *it = l.from; it && it != it_to; it = _get_next_item(it)) {
		if (it->type == ITEM_TABLE) {
			ItemTable *table = static_cast<ItemTable *>(it);
			for (Item *E : table->subitems) {
				ERR_CONTINUE(E->type != ITEM_FRAME); // Children should all be frames.
				ItemFrame *frame = static_cast<ItemFrame *>(E);
				for (int i = 0; i < (int)frame->lines.size(); i++) {
					txt += _get_line_text(frame, i, p_selection);
				}
			}
		}
		if ((p_selection.to_item != nullptr) && (p_selection.to_item->index < l.from->index)) {
			continue;
		}
		if ((p_selection.from_item != nullptr) && (p_selection.from_item->index >= end_idx)) {
			continue;
		}
		if (it->type == ITEM_DROPCAP) {
			const ItemDropcap *dc = static_cast<ItemDropcap *>(it);
			txt += dc->text;
		} else if (it->type == ITEM_TEXT) {
			const ItemText *t = static_cast<ItemText *>(it);
			txt += t->text;
		} else if (it->type == ITEM_NEWLINE) {
			txt += "\n";
		} else if (it->type == ITEM_IMAGE) {
			txt += " ";
		}
	}
	if ((l.from != nullptr) && (p_frame == p_selection.to_frame) && (p_selection.to_item != nullptr) && (p_selection.to_item->index >= l.from->index) && (p_selection.to_item->index < end_idx)) {
		txt = txt.substr(0, p_selection.to_char);
	}
	if ((l.from != nullptr) && (p_frame == p_selection.from_frame) && (p_selection.from_item != nullptr) && (p_selection.from_item->index >= l.from->index) && (p_selection.from_item->index < end_idx)) {
		txt = txt.substr(p_selection.from_char, -1);
	}
	return txt;
}

void RichTextLabel::set_context_menu_enabled(bool p_enabled) {
	context_menu_enabled = p_enabled;
}

bool RichTextLabel::is_context_menu_enabled() const {
	return context_menu_enabled;
}

void RichTextLabel::set_shortcut_keys_enabled(bool p_enabled) {
	shortcut_keys_enabled = p_enabled;
}

bool RichTextLabel::is_shortcut_keys_enabled() const {
	return shortcut_keys_enabled;
}

// Context menu.
PopupMenu *RichTextLabel::get_menu() const {
	if (!menu) {
		const_cast<RichTextLabel *>(this)->_generate_context_menu();
	}
	return menu;
}

bool RichTextLabel::is_menu_visible() const {
	return menu && menu->is_visible();
}

String RichTextLabel::get_selected_text() const {
	if (!selection.active || !selection.enabled) {
		return "";
	}

	String txt;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		txt += _get_line_text(main, i, selection);
	}

	if (selection_modifier.is_valid()) {
		txt = selection_modifier.call(txt);
	}

	return txt;
}

void RichTextLabel::deselect() {
	selection.active = false;
	queue_redraw();
}

void RichTextLabel::selection_copy() {
	String txt = get_selected_text();

	if (!txt.is_empty()) {
		DisplayServer::get_singleton()->clipboard_set(txt);
	}
}

void RichTextLabel::select_all() {
	_validate_line_caches();

	if (!selection.enabled) {
		return;
	}

	Item *it = main;
	Item *from_item = nullptr;
	Item *to_item = nullptr;

	while (it) {
		if (it->type != ITEM_FRAME) {
			if (!from_item) {
				from_item = it;
			}
			to_item = it;
		}
		it = _get_next_item(it, true);
	}
	if (!from_item) {
		return;
	}

	ItemFrame *from_frame = nullptr;
	int from_line = 0;
	_find_frame(from_item, &from_frame, &from_line);
	if (!from_frame) {
		return;
	}
	ItemFrame *to_frame = nullptr;
	int to_line = 0;
	_find_frame(to_item, &to_frame, &to_line);
	if (!to_frame) {
		return;
	}
	selection.from_line = from_line;
	selection.from_frame = from_frame;
	selection.from_char = 0;
	selection.from_item = from_item;
	selection.to_line = to_line;
	selection.to_frame = to_frame;
	selection.to_char = to_frame->lines[to_line].char_count;
	selection.to_item = to_item;
	selection.active = true;
	queue_redraw();
}

bool RichTextLabel::is_selection_enabled() const {
	return selection.enabled;
}

bool RichTextLabel::is_deselect_on_focus_loss_enabled() const {
	return deselect_on_focus_loss_enabled;
}

void RichTextLabel::set_drag_and_drop_selection_enabled(const bool p_enabled) {
	drag_and_drop_selection_enabled = p_enabled;
}

bool RichTextLabel::is_drag_and_drop_selection_enabled() const {
	return drag_and_drop_selection_enabled;
}

int RichTextLabel::get_selection_from() const {
	if (!selection.active || !selection.enabled) {
		return -1;
	}

	return selection.from_frame->lines[selection.from_line].char_offset + selection.from_char;
}

int RichTextLabel::get_selection_to() const {
	if (!selection.active || !selection.enabled) {
		return -1;
	}

	return selection.to_frame->lines[selection.to_line].char_offset + selection.to_char - 1;
}

float RichTextLabel::get_selection_line_offset() const {
	if (selection.active && selection.from_frame && selection.from_line >= 0 && selection.from_line < (int)selection.from_frame->lines.size()) {
		// Selected frame paragraph offset.
		float line_offset = selection.from_frame->lines[selection.from_line].offset.y;

		// Add wrapped line offset.
		for (int i = 0; i < selection.from_frame->lines[selection.from_line].text_buf->get_line_count(); i++) {
			Vector2i range = selection.from_frame->lines[selection.from_line].text_buf->get_line_range(i);
			if (range.x <= selection.from_char && range.y >= selection.from_char) {
				break;
			}
			line_offset += selection.from_frame->lines[selection.from_line].text_buf->get_line_ascent(i) + selection.from_frame->lines[selection.from_line].text_buf->get_line_descent(i) + theme_cache.line_separation;
		}

		// Add nested frame (e.g. table cell) offset.
		ItemFrame *it = selection.from_frame;
		while (it->parent_frame != nullptr) {
			line_offset += it->parent_frame->lines[it->line].offset.y;
			it = it->parent_frame;
		}
		return line_offset;
	}

	return -1.0;
}

void RichTextLabel::set_text(const String &p_bbcode) {
	// Allow clearing the tag stack.
	if (!p_bbcode.is_empty() && text == p_bbcode) {
		return;
	}

	stack_externally_modified = false;

	text = p_bbcode;
	if (text.is_empty()) {
		clear();
	} else {
		_apply_translation();
	}
}

void RichTextLabel::_apply_translation() {
	if (text.is_empty()) {
		return;
	}

	internal_stack_editing = true;

	String xl_text = atr(text);
	if (use_bbcode) {
		parse_bbcode(xl_text);
	} else { // Raw text.
		clear();
		add_text(xl_text);
	}

	internal_stack_editing = false;
}

String RichTextLabel::get_text() const {
	return text;
}

void RichTextLabel::set_use_bbcode(bool p_enable) {
	if (use_bbcode == p_enable) {
		return;
	}
	use_bbcode = p_enable;
	notify_property_list_changed();

	if (!stack_externally_modified) {
		_apply_translation();
	}
}

bool RichTextLabel::is_using_bbcode() const {
	return use_bbcode;
}

String RichTextLabel::get_parsed_text() const {
	String txt;
	Item *it = main;
	while (it) {
		if (it->type == ITEM_DROPCAP) {
			ItemDropcap *dc = static_cast<ItemDropcap *>(it);
			txt += dc->text;
		} else if (it->type == ITEM_TEXT) {
			ItemText *t = static_cast<ItemText *>(it);
			txt += t->text;
		} else if (it->type == ITEM_NEWLINE) {
			txt += "\n";
		} else if (it->type == ITEM_IMAGE) {
			txt += " ";
		} else if (it->type == ITEM_INDENT || it->type == ITEM_LIST) {
			txt += "\t";
		}
		it = _get_next_item(it, true);
	}
	return txt;
}

void RichTextLabel::set_text_direction(Control::TextDirection p_text_direction) {
	ERR_FAIL_COND((int)p_text_direction < -1 || (int)p_text_direction > 3);
	_stop_thread();

	if (text_direction != p_text_direction) {
		text_direction = p_text_direction;
		if (!stack_externally_modified) {
			_apply_translation();
		} else {
			main->first_invalid_line.store(0); // Invalidate all lines.
			_validate_line_caches();
		}
		queue_redraw();
	}
}

Control::TextDirection RichTextLabel::get_text_direction() const {
	return text_direction;
}

void RichTextLabel::set_horizontal_alignment(HorizontalAlignment p_alignment) {
	ERR_FAIL_INDEX((int)p_alignment, 4);
	_stop_thread();

	if (default_alignment != p_alignment) {
		default_alignment = p_alignment;
		if (!stack_externally_modified) {
			_apply_translation();
		} else {
			main->first_invalid_line.store(0); // Invalidate all lines.
			_validate_line_caches();
		}
		queue_redraw();
	}
}

HorizontalAlignment RichTextLabel::get_horizontal_alignment() const {
	return default_alignment;
}

void RichTextLabel::set_vertical_alignment(VerticalAlignment p_alignment) {
	ERR_FAIL_INDEX((int)p_alignment, 4);

	if (vertical_alignment == p_alignment) {
		return;
	}

	vertical_alignment = p_alignment;
	queue_redraw();
}

VerticalAlignment RichTextLabel::get_vertical_alignment() const {
	return vertical_alignment;
}

void RichTextLabel::set_justification_flags(BitField<TextServer::JustificationFlag> p_flags) {
	_stop_thread();

	if (default_jst_flags != p_flags) {
		default_jst_flags = p_flags;
		if (!stack_externally_modified) {
			_apply_translation();
		} else {
			main->first_invalid_line.store(0); // Invalidate all lines.
			_validate_line_caches();
		}
		queue_redraw();
	}
}

BitField<TextServer::JustificationFlag> RichTextLabel::get_justification_flags() const {
	return default_jst_flags;
}

void RichTextLabel::set_tab_stops(const PackedFloat32Array &p_tab_stops) {
	_stop_thread();

	if (default_tab_stops != p_tab_stops) {
		default_tab_stops = p_tab_stops;
		if (!stack_externally_modified) {
			_apply_translation();
		} else {
			main->first_invalid_line.store(0); // Invalidate all lines.
			_validate_line_caches();
		}
		queue_redraw();
	}
}

PackedFloat32Array RichTextLabel::get_tab_stops() const {
	return default_tab_stops;
}

void RichTextLabel::set_structured_text_bidi_override(TextServer::StructuredTextParser p_parser) {
	if (st_parser != p_parser) {
		_stop_thread();

		st_parser = p_parser;
		if (!stack_externally_modified) {
			_apply_translation();
		} else {
			main->first_invalid_line.store(0); // Invalidate all lines.
			_validate_line_caches();
		}
		queue_redraw();
	}
}

TextServer::StructuredTextParser RichTextLabel::get_structured_text_bidi_override() const {
	return st_parser;
}

void RichTextLabel::set_structured_text_bidi_override_options(Array p_args) {
	if (st_args != p_args) {
		_stop_thread();

		st_args = p_args;
		main->first_invalid_line.store(0); // Invalidate all lines.
		_validate_line_caches();
		queue_redraw();
	}
}

Array RichTextLabel::get_structured_text_bidi_override_options() const {
	return st_args;
}

void RichTextLabel::set_language(const String &p_language) {
	if (language != p_language) {
		_stop_thread();

		language = p_language;
		if (!stack_externally_modified) {
			_apply_translation();
		} else {
			main->first_invalid_line.store(0); // Invalidate all lines.
			_validate_line_caches();
		}
		queue_redraw();
	}
}

String RichTextLabel::get_language() const {
	return language;
}

void RichTextLabel::set_autowrap_mode(TextServer::AutowrapMode p_mode) {
	if (autowrap_mode != p_mode) {
		_stop_thread();

		autowrap_mode = p_mode;
		main->first_invalid_line = 0; // Invalidate all lines.
		_validate_line_caches();
		queue_redraw();
	}
}

TextServer::AutowrapMode RichTextLabel::get_autowrap_mode() const {
	return autowrap_mode;
}

void RichTextLabel::set_visible_ratio(float p_ratio) {
	if (visible_ratio != p_ratio) {
		_stop_thread();

		if (p_ratio >= 1.0) {
			visible_characters = -1;
			visible_ratio = 1.0;
		} else if (p_ratio < 0.0) {
			visible_characters = 0;
			visible_ratio = 0.0;
		} else {
			visible_characters = get_total_character_count() * p_ratio;
			visible_ratio = p_ratio;
		}

		if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING) {
			main->first_invalid_line.store(0); //  Invalidate all lines..
			_validate_line_caches();
		}
		queue_redraw();
	}
}

float RichTextLabel::get_visible_ratio() const {
	return visible_ratio;
}

void RichTextLabel::set_effects(Array p_effects) {
	custom_effects = p_effects;
	if (!stack_externally_modified && use_bbcode) {
		parse_bbcode(atr(text));
	}
}

Array RichTextLabel::get_effects() {
	return custom_effects;
}

void RichTextLabel::install_effect(const Variant effect) {
	Ref<RichTextEffect> rteffect;
	rteffect = effect;

	ERR_FAIL_COND_MSG(rteffect.is_null(), "Invalid RichTextEffect resource.");
	custom_effects.push_back(effect);
	if (!stack_externally_modified && use_bbcode) {
		parse_bbcode(atr(text));
	}
}

int RichTextLabel::get_content_height() const {
	const_cast<RichTextLabel *>(this)->_validate_line_caches();

	int total_height = 0;
	int to_line = main->first_invalid_line.load();
	if (to_line) {
		MutexLock lock(main->lines[to_line - 1].text_buf->get_mutex());
		if (theme_cache.line_separation < 0) {
			// Do not apply to the last line to avoid cutting text.
			total_height = main->lines[to_line - 1].offset.y + main->lines[to_line - 1].text_buf->get_size().y + (main->lines[to_line - 1].text_buf->get_line_count() - 1) * theme_cache.line_separation;
		} else {
			total_height = main->lines[to_line - 1].offset.y + main->lines[to_line - 1].text_buf->get_size().y + main->lines[to_line - 1].text_buf->get_line_count() * theme_cache.line_separation;
		}
	}
	return total_height;
}

int RichTextLabel::get_content_width() const {
	const_cast<RichTextLabel *>(this)->_validate_line_caches();

	int total_width = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		total_width = MAX(total_width, main->lines[i].offset.x + main->lines[i].text_buf->get_size().x);
	}
	return total_width;
}

#ifndef DISABLE_DEPRECATED
// People will be very angry, if their texts get erased, because of #39148. (3.x -> 4.0)
// Although some people may not used bbcode_text, so we only overwrite, if bbcode_text is not empty.
bool RichTextLabel::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "bbcode_text" && !((String)p_value).is_empty()) {
		set_text(p_value);
		return true;
	}
	return false;
}
#endif

void RichTextLabel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_parsed_text"), &RichTextLabel::get_parsed_text);
	ClassDB::bind_method(D_METHOD("add_text", "text"), &RichTextLabel::add_text);
	ClassDB::bind_method(D_METHOD("set_text", "text"), &RichTextLabel::set_text);
	ClassDB::bind_method(D_METHOD("add_image", "image", "width", "height", "color", "inline_align", "region", "key", "pad", "tooltip", "size_in_percent"), &RichTextLabel::add_image, DEFVAL(0), DEFVAL(0), DEFVAL(Color(1.0, 1.0, 1.0)), DEFVAL(INLINE_ALIGNMENT_CENTER), DEFVAL(Rect2()), DEFVAL(Variant()), DEFVAL(false), DEFVAL(String()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("update_image", "key", "mask", "image", "width", "height", "color", "inline_align", "region", "pad", "tooltip", "size_in_percent"), &RichTextLabel::update_image, DEFVAL(0), DEFVAL(0), DEFVAL(Color(1.0, 1.0, 1.0)), DEFVAL(INLINE_ALIGNMENT_CENTER), DEFVAL(Rect2()), DEFVAL(false), DEFVAL(String()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("newline"), &RichTextLabel::add_newline);
	ClassDB::bind_method(D_METHOD("remove_paragraph", "paragraph", "no_invalidate"), &RichTextLabel::remove_paragraph, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("invalidate_paragraph", "paragraph"), &RichTextLabel::invalidate_paragraph);
	ClassDB::bind_method(D_METHOD("push_font", "font", "font_size"), &RichTextLabel::push_font, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("push_font_size", "font_size"), &RichTextLabel::push_font_size);
	ClassDB::bind_method(D_METHOD("push_normal"), &RichTextLabel::push_normal);
	ClassDB::bind_method(D_METHOD("push_bold"), &RichTextLabel::push_bold);
	ClassDB::bind_method(D_METHOD("push_bold_italics"), &RichTextLabel::push_bold_italics);
	ClassDB::bind_method(D_METHOD("push_italics"), &RichTextLabel::push_italics);
	ClassDB::bind_method(D_METHOD("push_mono"), &RichTextLabel::push_mono);
	ClassDB::bind_method(D_METHOD("push_color", "color"), &RichTextLabel::push_color);
	ClassDB::bind_method(D_METHOD("push_outline_size", "outline_size"), &RichTextLabel::push_outline_size);
	ClassDB::bind_method(D_METHOD("push_outline_color", "color"), &RichTextLabel::push_outline_color);
	ClassDB::bind_method(D_METHOD("push_paragraph", "alignment", "base_direction", "language", "st_parser", "justification_flags", "tab_stops"), &RichTextLabel::push_paragraph, DEFVAL(TextServer::DIRECTION_AUTO), DEFVAL(""), DEFVAL(TextServer::STRUCTURED_TEXT_DEFAULT), DEFVAL(TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_SKIP_LAST_LINE | TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE), DEFVAL(PackedFloat32Array()));
	ClassDB::bind_method(D_METHOD("push_indent", "level"), &RichTextLabel::push_indent);
	ClassDB::bind_method(D_METHOD("push_list", "level", "type", "capitalize", "bullet"), &RichTextLabel::push_list, DEFVAL(String::utf8("•")));
	ClassDB::bind_method(D_METHOD("push_meta", "data", "underline_mode", "tooltip"), &RichTextLabel::push_meta, DEFVAL(META_UNDERLINE_ALWAYS), DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("push_hint", "description"), &RichTextLabel::push_hint);
	ClassDB::bind_method(D_METHOD("push_language", "language"), &RichTextLabel::push_language);
	ClassDB::bind_method(D_METHOD("push_underline"), &RichTextLabel::push_underline);
	ClassDB::bind_method(D_METHOD("push_strikethrough"), &RichTextLabel::push_strikethrough);
	ClassDB::bind_method(D_METHOD("push_table", "columns", "inline_align", "align_to_row"), &RichTextLabel::push_table, DEFVAL(INLINE_ALIGNMENT_TOP), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("push_dropcap", "string", "font", "size", "dropcap_margins", "color", "outline_size", "outline_color"), &RichTextLabel::push_dropcap, DEFVAL(Rect2()), DEFVAL(Color(1, 1, 1)), DEFVAL(0), DEFVAL(Color(0, 0, 0, 0)));
	ClassDB::bind_method(D_METHOD("set_table_column_expand", "column", "expand", "ratio", "shrink"), &RichTextLabel::set_table_column_expand, DEFVAL(1), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("set_cell_row_background_color", "odd_row_bg", "even_row_bg"), &RichTextLabel::set_cell_row_background_color);
	ClassDB::bind_method(D_METHOD("set_cell_border_color", "color"), &RichTextLabel::set_cell_border_color);
	ClassDB::bind_method(D_METHOD("set_cell_size_override", "min_size", "max_size"), &RichTextLabel::set_cell_size_override);
	ClassDB::bind_method(D_METHOD("set_cell_padding", "padding"), &RichTextLabel::set_cell_padding);
	ClassDB::bind_method(D_METHOD("push_cell"), &RichTextLabel::push_cell);
	ClassDB::bind_method(D_METHOD("push_fgcolor", "fgcolor"), &RichTextLabel::push_fgcolor);
	ClassDB::bind_method(D_METHOD("push_bgcolor", "bgcolor"), &RichTextLabel::push_bgcolor);
	ClassDB::bind_method(D_METHOD("push_customfx", "effect", "env"), &RichTextLabel::push_customfx);
	ClassDB::bind_method(D_METHOD("push_context"), &RichTextLabel::push_context);
	ClassDB::bind_method(D_METHOD("pop_context"), &RichTextLabel::pop_context);
	ClassDB::bind_method(D_METHOD("pop"), &RichTextLabel::pop);
	ClassDB::bind_method(D_METHOD("pop_all"), &RichTextLabel::pop_all);

	ClassDB::bind_method(D_METHOD("clear"), &RichTextLabel::clear);

	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override", "parser"), &RichTextLabel::set_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override"), &RichTextLabel::get_structured_text_bidi_override);
	ClassDB::bind_method(D_METHOD("set_structured_text_bidi_override_options", "args"), &RichTextLabel::set_structured_text_bidi_override_options);
	ClassDB::bind_method(D_METHOD("get_structured_text_bidi_override_options"), &RichTextLabel::get_structured_text_bidi_override_options);
	ClassDB::bind_method(D_METHOD("set_text_direction", "direction"), &RichTextLabel::set_text_direction);
	ClassDB::bind_method(D_METHOD("get_text_direction"), &RichTextLabel::get_text_direction);
	ClassDB::bind_method(D_METHOD("set_language", "language"), &RichTextLabel::set_language);
	ClassDB::bind_method(D_METHOD("get_language"), &RichTextLabel::get_language);

	ClassDB::bind_method(D_METHOD("set_horizontal_alignment", "alignment"), &RichTextLabel::set_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("get_horizontal_alignment"), &RichTextLabel::get_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("set_vertical_alignment", "alignment"), &RichTextLabel::set_vertical_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_alignment"), &RichTextLabel::get_vertical_alignment);
	ClassDB::bind_method(D_METHOD("set_justification_flags", "justification_flags"), &RichTextLabel::set_justification_flags);
	ClassDB::bind_method(D_METHOD("get_justification_flags"), &RichTextLabel::get_justification_flags);
	ClassDB::bind_method(D_METHOD("set_tab_stops", "tab_stops"), &RichTextLabel::set_tab_stops);
	ClassDB::bind_method(D_METHOD("get_tab_stops"), &RichTextLabel::get_tab_stops);

	ClassDB::bind_method(D_METHOD("set_autowrap_mode", "autowrap_mode"), &RichTextLabel::set_autowrap_mode);
	ClassDB::bind_method(D_METHOD("get_autowrap_mode"), &RichTextLabel::get_autowrap_mode);

	ClassDB::bind_method(D_METHOD("set_meta_underline", "enable"), &RichTextLabel::set_meta_underline);
	ClassDB::bind_method(D_METHOD("is_meta_underlined"), &RichTextLabel::is_meta_underlined);

	ClassDB::bind_method(D_METHOD("set_hint_underline", "enable"), &RichTextLabel::set_hint_underline);
	ClassDB::bind_method(D_METHOD("is_hint_underlined"), &RichTextLabel::is_hint_underlined);

	ClassDB::bind_method(D_METHOD("set_scroll_active", "active"), &RichTextLabel::set_scroll_active);
	ClassDB::bind_method(D_METHOD("is_scroll_active"), &RichTextLabel::is_scroll_active);

	ClassDB::bind_method(D_METHOD("set_scroll_follow", "follow"), &RichTextLabel::set_scroll_follow);
	ClassDB::bind_method(D_METHOD("is_scroll_following"), &RichTextLabel::is_scroll_following);

	ClassDB::bind_method(D_METHOD("get_v_scroll_bar"), &RichTextLabel::get_v_scroll_bar);

	ClassDB::bind_method(D_METHOD("scroll_to_line", "line"), &RichTextLabel::scroll_to_line);
	ClassDB::bind_method(D_METHOD("scroll_to_paragraph", "paragraph"), &RichTextLabel::scroll_to_paragraph);
	ClassDB::bind_method(D_METHOD("scroll_to_selection"), &RichTextLabel::scroll_to_selection);

	ClassDB::bind_method(D_METHOD("set_tab_size", "spaces"), &RichTextLabel::set_tab_size);
	ClassDB::bind_method(D_METHOD("get_tab_size"), &RichTextLabel::get_tab_size);

	ClassDB::bind_method(D_METHOD("set_fit_content", "enabled"), &RichTextLabel::set_fit_content);
	ClassDB::bind_method(D_METHOD("is_fit_content_enabled"), &RichTextLabel::is_fit_content_enabled);

	ClassDB::bind_method(D_METHOD("set_selection_enabled", "enabled"), &RichTextLabel::set_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_selection_enabled"), &RichTextLabel::is_selection_enabled);

	ClassDB::bind_method(D_METHOD("set_context_menu_enabled", "enabled"), &RichTextLabel::set_context_menu_enabled);
	ClassDB::bind_method(D_METHOD("is_context_menu_enabled"), &RichTextLabel::is_context_menu_enabled);

	ClassDB::bind_method(D_METHOD("set_shortcut_keys_enabled", "enabled"), &RichTextLabel::set_shortcut_keys_enabled);
	ClassDB::bind_method(D_METHOD("is_shortcut_keys_enabled"), &RichTextLabel::is_shortcut_keys_enabled);

	ClassDB::bind_method(D_METHOD("set_deselect_on_focus_loss_enabled", "enable"), &RichTextLabel::set_deselect_on_focus_loss_enabled);
	ClassDB::bind_method(D_METHOD("is_deselect_on_focus_loss_enabled"), &RichTextLabel::is_deselect_on_focus_loss_enabled);

	ClassDB::bind_method(D_METHOD("set_drag_and_drop_selection_enabled", "enable"), &RichTextLabel::set_drag_and_drop_selection_enabled);
	ClassDB::bind_method(D_METHOD("is_drag_and_drop_selection_enabled"), &RichTextLabel::is_drag_and_drop_selection_enabled);

	ClassDB::bind_method(D_METHOD("get_selection_from"), &RichTextLabel::get_selection_from);
	ClassDB::bind_method(D_METHOD("get_selection_to"), &RichTextLabel::get_selection_to);
	ClassDB::bind_method(D_METHOD("get_selection_line_offset"), &RichTextLabel::get_selection_line_offset);

	ClassDB::bind_method(D_METHOD("select_all"), &RichTextLabel::select_all);
	ClassDB::bind_method(D_METHOD("get_selected_text"), &RichTextLabel::get_selected_text);
	ClassDB::bind_method(D_METHOD("deselect"), &RichTextLabel::deselect);

	ClassDB::bind_method(D_METHOD("parse_bbcode", "bbcode"), &RichTextLabel::parse_bbcode);
	ClassDB::bind_method(D_METHOD("append_text", "bbcode"), &RichTextLabel::append_text);

	ClassDB::bind_method(D_METHOD("get_text"), &RichTextLabel::get_text);

#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("is_ready"), &RichTextLabel::is_finished);
#endif // DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("is_finished"), &RichTextLabel::is_finished);

	ClassDB::bind_method(D_METHOD("set_threaded", "threaded"), &RichTextLabel::set_threaded);
	ClassDB::bind_method(D_METHOD("is_threaded"), &RichTextLabel::is_threaded);

	ClassDB::bind_method(D_METHOD("set_progress_bar_delay", "delay_ms"), &RichTextLabel::set_progress_bar_delay);
	ClassDB::bind_method(D_METHOD("get_progress_bar_delay"), &RichTextLabel::get_progress_bar_delay);

	ClassDB::bind_method(D_METHOD("set_visible_characters", "amount"), &RichTextLabel::set_visible_characters);
	ClassDB::bind_method(D_METHOD("get_visible_characters"), &RichTextLabel::get_visible_characters);

	ClassDB::bind_method(D_METHOD("get_visible_characters_behavior"), &RichTextLabel::get_visible_characters_behavior);
	ClassDB::bind_method(D_METHOD("set_visible_characters_behavior", "behavior"), &RichTextLabel::set_visible_characters_behavior);

	ClassDB::bind_method(D_METHOD("set_visible_ratio", "ratio"), &RichTextLabel::set_visible_ratio);
	ClassDB::bind_method(D_METHOD("get_visible_ratio"), &RichTextLabel::get_visible_ratio);

	ClassDB::bind_method(D_METHOD("get_character_line", "character"), &RichTextLabel::get_character_line);
	ClassDB::bind_method(D_METHOD("get_character_paragraph", "character"), &RichTextLabel::get_character_paragraph);
	ClassDB::bind_method(D_METHOD("get_total_character_count"), &RichTextLabel::get_total_character_count);

	ClassDB::bind_method(D_METHOD("set_use_bbcode", "enable"), &RichTextLabel::set_use_bbcode);
	ClassDB::bind_method(D_METHOD("is_using_bbcode"), &RichTextLabel::is_using_bbcode);

	ClassDB::bind_method(D_METHOD("get_line_count"), &RichTextLabel::get_line_count);
	ClassDB::bind_method(D_METHOD("get_line_range", "line"), &RichTextLabel::get_line_range);
	ClassDB::bind_method(D_METHOD("get_visible_line_count"), &RichTextLabel::get_visible_line_count);

	ClassDB::bind_method(D_METHOD("get_paragraph_count"), &RichTextLabel::get_paragraph_count);
	ClassDB::bind_method(D_METHOD("get_visible_paragraph_count"), &RichTextLabel::get_visible_paragraph_count);

	ClassDB::bind_method(D_METHOD("get_content_height"), &RichTextLabel::get_content_height);
	ClassDB::bind_method(D_METHOD("get_content_width"), &RichTextLabel::get_content_width);

	ClassDB::bind_method(D_METHOD("get_line_offset", "line"), &RichTextLabel::get_line_offset);
	ClassDB::bind_method(D_METHOD("get_paragraph_offset", "paragraph"), &RichTextLabel::get_paragraph_offset);

	ClassDB::bind_method(D_METHOD("parse_expressions_for_values", "expressions"), &RichTextLabel::parse_expressions_for_values);

	ClassDB::bind_method(D_METHOD("set_effects", "effects"), &RichTextLabel::set_effects);
	ClassDB::bind_method(D_METHOD("get_effects"), &RichTextLabel::get_effects);
	ClassDB::bind_method(D_METHOD("install_effect", "effect"), &RichTextLabel::install_effect);

	ClassDB::bind_method(D_METHOD("get_menu"), &RichTextLabel::get_menu);
	ClassDB::bind_method(D_METHOD("is_menu_visible"), &RichTextLabel::is_menu_visible);
	ClassDB::bind_method(D_METHOD("menu_option", "option"), &RichTextLabel::menu_option);

	// Note: set "bbcode_enabled" first, to avoid unnecessary "text" resets.
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bbcode_enabled"), "set_use_bbcode", "is_using_bbcode");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fit_content"), "set_fit_content", "is_fit_content_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_active"), "set_scroll_active", "is_scroll_active");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "scroll_following"), "set_scroll_follow", "is_scroll_following");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "autowrap_mode", PROPERTY_HINT_ENUM, "Off,Arbitrary,Word,Word (Smart)"), "set_autowrap_mode", "get_autowrap_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tab_size", PROPERTY_HINT_RANGE, "0,24,1"), "set_tab_size", "get_tab_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "context_menu_enabled"), "set_context_menu_enabled", "is_context_menu_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "shortcut_keys_enabled"), "set_shortcut_keys_enabled", "is_shortcut_keys_enabled");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "horizontal_alignment", PROPERTY_HINT_ENUM, "Left,Center,Right,Fill"), "set_horizontal_alignment", "get_horizontal_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "vertical_alignment", PROPERTY_HINT_ENUM, "Top,Center,Bottom,Fill"), "set_vertical_alignment", "get_vertical_alignment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "justification_flags", PROPERTY_HINT_FLAGS, "Kashida Justification:1,Word Justification:2,Justify Only After Last Tab:8,Skip Last Line:32,Skip Last Line With Visible Characters:64,Do Not Skip Single Line:128"), "set_justification_flags", "get_justification_flags");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "tab_stops"), "set_tab_stops", "get_tab_stops");

	ADD_GROUP("Markup", "");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "custom_effects", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("RichTextEffect"), (PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE)), "set_effects", "get_effects");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "meta_underlined"), "set_meta_underline", "is_meta_underlined");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "hint_underlined"), "set_hint_underline", "is_hint_underlined");

	ADD_GROUP("Threading", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "threaded"), "set_threaded", "is_threaded");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "progress_bar_delay", PROPERTY_HINT_NONE, "suffix:ms"), "set_progress_bar_delay", "get_progress_bar_delay");

	ADD_GROUP("Text Selection", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selection_enabled"), "set_selection_enabled", "is_selection_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "deselect_on_focus_loss_enabled"), "set_deselect_on_focus_loss_enabled", "is_deselect_on_focus_loss_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drag_and_drop_selection_enabled"), "set_drag_and_drop_selection_enabled", "is_drag_and_drop_selection_enabled");

	ADD_GROUP("Displayed Text", "");
	// Note: "visible_characters" and "visible_ratio" should be set after "text" to be correctly applied.
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visible_characters", PROPERTY_HINT_RANGE, "-1,128000,1"), "set_visible_characters", "get_visible_characters");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "visible_characters_behavior", PROPERTY_HINT_ENUM, "Characters Before Shaping,Characters After Shaping,Glyphs (Layout Direction),Glyphs (Left-to-Right),Glyphs (Right-to-Left)"), "set_visible_characters_behavior", "get_visible_characters_behavior");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "visible_ratio", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_visible_ratio", "get_visible_ratio");

	ADD_GROUP("BiDi", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "text_direction", PROPERTY_HINT_ENUM, "Auto,Left-to-Right,Right-to-Left,Inherited"), "set_text_direction", "get_text_direction");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID, ""), "set_language", "get_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "structured_text_bidi_override", PROPERTY_HINT_ENUM, "Default,URI,File,Email,List,None,Custom"), "set_structured_text_bidi_override", "get_structured_text_bidi_override");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "structured_text_bidi_override_options"), "set_structured_text_bidi_override_options", "get_structured_text_bidi_override_options");

	ADD_SIGNAL(MethodInfo("meta_clicked", PropertyInfo(Variant::NIL, "meta", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("meta_hover_started", PropertyInfo(Variant::NIL, "meta", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("meta_hover_ended", PropertyInfo(Variant::NIL, "meta", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));

	ADD_SIGNAL(MethodInfo("finished"));

	BIND_ENUM_CONSTANT(LIST_NUMBERS);
	BIND_ENUM_CONSTANT(LIST_LETTERS);
	BIND_ENUM_CONSTANT(LIST_ROMAN);
	BIND_ENUM_CONSTANT(LIST_DOTS);

	BIND_ENUM_CONSTANT(MENU_COPY);
	BIND_ENUM_CONSTANT(MENU_SELECT_ALL);
	BIND_ENUM_CONSTANT(MENU_MAX);

	BIND_ENUM_CONSTANT(META_UNDERLINE_NEVER);
	BIND_ENUM_CONSTANT(META_UNDERLINE_ALWAYS);
	BIND_ENUM_CONSTANT(META_UNDERLINE_ON_HOVER);

	BIND_BITFIELD_FLAG(UPDATE_TEXTURE);
	BIND_BITFIELD_FLAG(UPDATE_SIZE);
	BIND_BITFIELD_FLAG(UPDATE_COLOR);
	BIND_BITFIELD_FLAG(UPDATE_ALIGNMENT);
	BIND_BITFIELD_FLAG(UPDATE_REGION);
	BIND_BITFIELD_FLAG(UPDATE_PAD);
	BIND_BITFIELD_FLAG(UPDATE_TOOLTIP);
	BIND_BITFIELD_FLAG(UPDATE_WIDTH_IN_PERCENT);

	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, RichTextLabel, normal_style, "normal");
	BIND_THEME_ITEM_CUSTOM(Theme::DATA_TYPE_STYLEBOX, RichTextLabel, focus_style, "focus");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, RichTextLabel, progress_bg_style, "background", "ProgressBar");
	BIND_THEME_ITEM_EXT(Theme::DATA_TYPE_STYLEBOX, RichTextLabel, progress_fg_style, "fill", "ProgressBar");

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, RichTextLabel, line_separation);

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, RichTextLabel, normal_font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, RichTextLabel, normal_font_size);

	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, RichTextLabel, default_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, RichTextLabel, font_selected_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, RichTextLabel, selection_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, RichTextLabel, font_outline_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, RichTextLabel, font_shadow_color);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, RichTextLabel, shadow_outline_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, RichTextLabel, shadow_offset_x);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, RichTextLabel, shadow_offset_y);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, RichTextLabel, outline_size);

	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, RichTextLabel, bold_font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, RichTextLabel, bold_font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, RichTextLabel, bold_italics_font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, RichTextLabel, bold_italics_font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, RichTextLabel, italics_font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, RichTextLabel, italics_font_size);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT, RichTextLabel, mono_font);
	BIND_THEME_ITEM(Theme::DATA_TYPE_FONT_SIZE, RichTextLabel, mono_font_size);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, RichTextLabel, text_highlight_h_padding);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, RichTextLabel, text_highlight_v_padding);

	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, RichTextLabel, table_h_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_CONSTANT, RichTextLabel, table_v_separation);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, RichTextLabel, table_odd_row_bg);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, RichTextLabel, table_even_row_bg);
	BIND_THEME_ITEM(Theme::DATA_TYPE_COLOR, RichTextLabel, table_border);
}

TextServer::VisibleCharactersBehavior RichTextLabel::get_visible_characters_behavior() const {
	return visible_chars_behavior;
}

void RichTextLabel::set_visible_characters_behavior(TextServer::VisibleCharactersBehavior p_behavior) {
	if (visible_chars_behavior != p_behavior) {
		_stop_thread();

		visible_chars_behavior = p_behavior;
		main->first_invalid_line.store(0); // Invalidate all lines.
		_validate_line_caches();
		queue_redraw();
	}
}

void RichTextLabel::set_visible_characters(int p_visible) {
	if (visible_characters != p_visible) {
		_stop_thread();

		visible_characters = p_visible;
		if (p_visible == -1) {
			visible_ratio = 1;
		} else {
			int total_char_count = get_total_character_count();
			if (total_char_count > 0) {
				visible_ratio = (float)p_visible / (float)total_char_count;
			}
		}
		if (visible_chars_behavior == TextServer::VC_CHARS_BEFORE_SHAPING) {
			main->first_invalid_line.store(0); // Invalidate all lines.
			_validate_line_caches();
		}
		queue_redraw();
	}
}

int RichTextLabel::get_visible_characters() const {
	return visible_characters;
}

int RichTextLabel::get_character_line(int p_char) {
	_validate_line_caches();

	int line_count = 0;
	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		MutexLock lock(main->lines[i].text_buf->get_mutex());
		int char_offset = main->lines[i].char_offset;
		int char_count = main->lines[i].char_count;
		if (char_offset <= p_char && p_char < char_offset + char_count) {
			int lc = main->lines[i].text_buf->get_line_count();
			for (int j = 0; j < lc; j++) {
				Vector2i range = main->lines[i].text_buf->get_line_range(j);
				if (char_offset + range.x <= p_char && p_char < char_offset + range.y) {
					break;
				}
				if (char_offset + range.x > p_char && line_count > 0) {
					line_count--; // Character is not rendered and is between the lines (e.g., edge space).
					break;
				}
				if (j != lc - 1) {
					line_count++;
				}
			}
			return line_count;
		} else {
			line_count += main->lines[i].text_buf->get_line_count();
		}
	}
	return -1;
}

int RichTextLabel::get_character_paragraph(int p_char) {
	_validate_line_caches();

	int to_line = main->first_invalid_line.load();
	for (int i = 0; i < to_line; i++) {
		int char_offset = main->lines[i].char_offset;
		if (char_offset <= p_char && p_char < char_offset + main->lines[i].char_count) {
			return i;
		}
	}
	return -1;
}

int RichTextLabel::get_total_character_count() const {
	// Note: Do not use line buffer "char_count", it includes only visible characters.
	int tc = 0;
	Item *it = main;
	while (it) {
		if (it->type == ITEM_TEXT) {
			ItemText *t = static_cast<ItemText *>(it);
			tc += t->text.length();
		} else if (it->type == ITEM_NEWLINE) {
			tc++;
		} else if (it->type == ITEM_IMAGE) {
			tc++;
		}
		it = _get_next_item(it, true);
	}
	return tc;
}

int RichTextLabel::get_total_glyph_count() const {
	const_cast<RichTextLabel *>(this)->_validate_line_caches();

	int tg = 0;
	Item *it = main;
	while (it) {
		if (it->type == ITEM_FRAME) {
			ItemFrame *f = static_cast<ItemFrame *>(it);
			for (int i = 0; i < (int)f->lines.size(); i++) {
				MutexLock lock(f->lines[i].text_buf->get_mutex());
				tg += TS->shaped_text_get_glyph_count(f->lines[i].text_buf->get_rid());
			}
		}
		it = _get_next_item(it, true);
	}

	return tg;
}

Size2 RichTextLabel::get_minimum_size() const {
	Size2 sb_min_size = theme_cache.normal_style->get_minimum_size();
	Size2 min_size;

	if (fit_content) {
		min_size.x = get_content_width();
		min_size.y = get_content_height();
	}

	return sb_min_size +
			((autowrap_mode != TextServer::AUTOWRAP_OFF) ? Size2(1, min_size.height) : min_size);
}

// Context menu.
void RichTextLabel::_generate_context_menu() {
	menu = memnew(PopupMenu);
	add_child(menu, false, INTERNAL_MODE_FRONT);
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &RichTextLabel::menu_option));

	menu->add_item(ETR("Copy"), MENU_COPY);
	menu->add_item(ETR("Select All"), MENU_SELECT_ALL);
}

void RichTextLabel::_update_context_menu() {
	if (!menu) {
		_generate_context_menu();
	}

	int idx = -1;

#define MENU_ITEM_ACTION_DISABLED(m_menu, m_id, m_action, m_disabled)                                                  \
	idx = m_menu->get_item_index(m_id);                                                                                \
	if (idx >= 0) {                                                                                                    \
		m_menu->set_item_accelerator(idx, shortcut_keys_enabled ? _get_menu_action_accelerator(m_action) : Key::NONE); \
		m_menu->set_item_disabled(idx, m_disabled);                                                                    \
	}

	MENU_ITEM_ACTION_DISABLED(menu, MENU_COPY, "ui_copy", !selection.enabled)
	MENU_ITEM_ACTION_DISABLED(menu, MENU_SELECT_ALL, "ui_text_select_all", !selection.enabled)

#undef MENU_ITEM_ACTION_DISABLED
}

Key RichTextLabel::_get_menu_action_accelerator(const String &p_action) {
	const List<Ref<InputEvent>> *events = InputMap::get_singleton()->action_get_events(p_action);
	if (!events) {
		return Key::NONE;
	}

	// Use first event in the list for the accelerator.
	const List<Ref<InputEvent>>::Element *first_event = events->front();
	if (!first_event) {
		return Key::NONE;
	}

	const Ref<InputEventKey> event = first_event->get();
	if (event.is_null()) {
		return Key::NONE;
	}

	// Use physical keycode if non-zero
	if (event->get_physical_keycode() != Key::NONE) {
		return event->get_physical_keycode_with_modifiers();
	} else {
		return event->get_keycode_with_modifiers();
	}
}

void RichTextLabel::menu_option(int p_option) {
	switch (p_option) {
		case MENU_COPY: {
			selection_copy();
		} break;
		case MENU_SELECT_ALL: {
			select_all();
		} break;
	}
}

Ref<RichTextEffect> RichTextLabel::_get_custom_effect_by_code(String p_bbcode_identifier) {
	for (int i = 0; i < custom_effects.size(); i++) {
		Ref<RichTextEffect> effect = custom_effects[i];
		if (effect.is_null()) {
			continue;
		}

		if (effect->get_bbcode() == p_bbcode_identifier) {
			return effect;
		}
	}

	return Ref<RichTextEffect>();
}

Dictionary RichTextLabel::parse_expressions_for_values(Vector<String> p_expressions) {
	Dictionary d;
	for (int i = 0; i < p_expressions.size(); i++) {
		Array a;
		Vector<String> parts = p_expressions[i].split("=", true);
		const String &key = parts[0];
		if (parts.size() != 2) {
			return d;
		}

		Vector<String> values = parts[1].split(",", false);

#ifdef MODULE_REGEX_ENABLED
		RegEx color = RegEx();
		color.compile("^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$");
		RegEx nodepath = RegEx();
		nodepath.compile("^\\$");
		RegEx boolean = RegEx();
		boolean.compile("^(true|false)$");
		RegEx decimal = RegEx();
		decimal.compile("^-?^.?\\d+(\\.\\d+?)?$");
		RegEx numerical = RegEx();
		numerical.compile("^\\d+$");

		for (int j = 0; j < values.size(); j++) {
			if (color.search(values[j]).is_valid()) {
				a.append(Color::html(values[j]));
			} else if (nodepath.search(values[j]).is_valid()) {
				if (values[j].begins_with("$")) {
					String v = values[j].substr(1, values[j].length());
					a.append(NodePath(v));
				}
			} else if (boolean.search(values[j]).is_valid()) {
				if (values[j] == "true") {
					a.append(true);
				} else if (values[j] == "false") {
					a.append(false);
				}
			} else if (decimal.search(values[j]).is_valid()) {
				a.append(values[j].to_float());
			} else if (numerical.search(values[j]).is_valid()) {
				a.append(values[j].to_int());
			} else {
				a.append(values[j]);
			}
		}
#endif

		if (values.size() > 1) {
			d[key] = a;
		} else if (values.size() == 1) {
			d[key] = a[0];
		}
	}
	return d;
}

RichTextLabel::RichTextLabel(const String &p_text) {
	main = memnew(ItemFrame);
	main->owner = get_instance_id();
	main->rid = items.make_rid(main);
	main->index = 0;
	current = main;
	main->lines.resize(1);
	main->lines[0].from = main;
	main->first_invalid_line.store(0);
	main->first_resized_line.store(0);
	main->first_invalid_font_line.store(0);
	current_frame = main;

	vscroll = memnew(VScrollBar);
	add_child(vscroll, false, INTERNAL_MODE_FRONT);
	vscroll->set_drag_node(String(".."));
	vscroll->set_step(1);
	vscroll->set_anchor_and_offset(SIDE_TOP, ANCHOR_BEGIN, 0);
	vscroll->set_anchor_and_offset(SIDE_BOTTOM, ANCHOR_END, 0);
	vscroll->set_anchor_and_offset(SIDE_RIGHT, ANCHOR_END, 0);
	vscroll->connect(SceneStringName(value_changed), callable_mp(this, &RichTextLabel::_scroll_changed));
	vscroll->set_step(1);
	vscroll->hide();

	set_text(p_text);
	updating.store(false);
	validating.store(false);
	stop_thread.store(false);
	parsing_bbcode.store(false);

	set_clip_contents(true);
}

RichTextLabel::~RichTextLabel() {
	_stop_thread();
	items.free(main->rid);
	memdelete(main);
}
