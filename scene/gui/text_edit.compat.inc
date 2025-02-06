/**************************************************************************/
/*  text_edit.compat.inc                                                  */
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

void TextEdit::_set_selection_mode_compat_86978(SelectionMode p_mode, int p_line, int p_column, int p_caret) {
	set_selection_mode(p_mode);
}

Point2i TextEdit::_get_line_column_at_pos_bind_compat_100913(const Point2i &p_pos, bool p_allow_out_of_bounds) const {
	return get_line_column_at_pos(p_pos, p_allow_out_of_bounds, true);
}

void TextEdit::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("set_selection_mode", "mode", "line", "column", "caret_index"), &TextEdit::_set_selection_mode_compat_86978, DEFVAL(-1), DEFVAL(-1), DEFVAL(0));
	ClassDB::bind_compatibility_method(D_METHOD("get_line_column_at_pos", "position", "allow_out_of_bounds"), &TextEdit::_get_line_column_at_pos_bind_compat_100913, DEFVAL(true));
}

#endif
