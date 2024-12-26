/**************************************************************************/
/*  code_edit.compat.inc                                                  */
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

String CodeEdit::_get_text_for_symbol_lookup_bind_compat_73196() {
	return get_text_for_symbol_lookup();
}

void CodeEdit::_add_code_completion_option_compat_84906(CodeCompletionKind p_type, const String &p_display_text, const String &p_insert_text, const Color &p_text_color, const Ref<Resource> &p_icon, const Variant &p_value, int p_location) {
	add_code_completion_option(p_type, p_display_text, p_insert_text, p_text_color, p_icon, p_value, p_location);
}

void CodeEdit::_add_code_completion_option_compat_100019(CodeCompletionKind p_type, const String &p_display_text, const String &p_insert_text, const Color &p_text_color, const Ref<Resource> &p_icon, const Variant &p_value, int p_location) {
	add_code_completion_option(p_type, p_display_text, p_insert_text, p_text_color, p_icon, p_value, p_location, false);
}

void CodeEdit::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("get_text_for_symbol_lookup"), &CodeEdit::_get_text_for_symbol_lookup_bind_compat_73196);

	ClassDB::bind_compatibility_method(D_METHOD("add_code_completion_option", "type", "display_text", "insert_text", "text_color", "icon", "value", "location"), &CodeEdit::_add_code_completion_option_compat_84906, DEFVAL(Color(1, 1, 1)), DEFVAL(Ref<Resource>()), DEFVAL(Variant::NIL), DEFVAL(LOCATION_OTHER));
	ClassDB::bind_compatibility_method(D_METHOD("add_code_completion_option", "type", "display_text", "insert_text", "text_color", "icon", "value", "location"), &CodeEdit::_add_code_completion_option_compat_100019, DEFVAL(Color(1, 1, 1)), DEFVAL(Ref<Resource>()), DEFVAL(Variant()), DEFVAL(LOCATION_OTHER));
}

#endif
