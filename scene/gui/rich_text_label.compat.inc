/**************************************************************************/
/*  rich_text_label.compat.inc                                            */
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

#ifndef DISABLE_DEPRECATED

void RichTextLabel::_push_font_bind_compat_79053(const Ref<Font> &p_font, int p_size) {
	push_font(p_font, p_size);
}

void RichTextLabel::_set_table_column_expand_bind_compat_79053(int p_column, bool p_expand, int p_ratio) {
	set_table_column_expand(p_column, p_expand, p_ratio, true);
}

void RichTextLabel::_set_table_column_expand_bind_compat_101482(int p_column, bool p_expand, int p_ratio) {
	set_table_column_expand(p_column, p_expand, p_ratio, true);
}

void RichTextLabel::_push_meta_bind_compat_99481(const Variant &p_meta, MetaUnderline p_underline_mode) {
	push_meta(p_meta, p_underline_mode, String());
}

void RichTextLabel::_push_meta_bind_compat_89024(const Variant &p_meta) {
	push_meta(p_meta, RichTextLabel::MetaUnderline::META_UNDERLINE_ALWAYS, String());
}

void RichTextLabel::_add_image_bind_compat_80410(const Ref<Texture2D> &p_image, const int p_width, const int p_height, const Color &p_color, InlineAlignment p_alignment, const Rect2 &p_region) {
	add_image(p_image, p_width, p_height, p_color, p_alignment, p_region, Variant(), false, String(), false);
}

bool RichTextLabel::_remove_paragraph_bind_compat_91098(int p_paragraph) {
	return remove_paragraph(p_paragraph, false);
}

void RichTextLabel::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("push_font", "font", "font_size"), &RichTextLabel::_push_font_bind_compat_79053);
	ClassDB::bind_compatibility_method(D_METHOD("set_table_column_expand", "column", "expand", "ratio"), &RichTextLabel::_set_table_column_expand_bind_compat_79053);
	ClassDB::bind_compatibility_method(D_METHOD("set_table_column_expand", "column", "expand", "ratio"), &RichTextLabel::_set_table_column_expand_bind_compat_101482, DEFVAL(1));
	ClassDB::bind_compatibility_method(D_METHOD("push_meta", "data", "underline_mode"), &RichTextLabel::_push_meta_bind_compat_99481, DEFVAL(META_UNDERLINE_ALWAYS));
	ClassDB::bind_compatibility_method(D_METHOD("push_meta", "data"), &RichTextLabel::_push_meta_bind_compat_89024);
	ClassDB::bind_compatibility_method(D_METHOD("add_image", "image", "width", "height", "color", "inline_align", "region"), &RichTextLabel::_add_image_bind_compat_80410, DEFVAL(0), DEFVAL(0), DEFVAL(Color(1.0, 1.0, 1.0)), DEFVAL(INLINE_ALIGNMENT_CENTER), DEFVAL(Rect2()));
	ClassDB::bind_compatibility_method(D_METHOD("remove_paragraph", "paragraph"), &RichTextLabel::_remove_paragraph_bind_compat_91098);
}

#endif // DISABLE_DEPRECATED
