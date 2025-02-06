/**************************************************************************/
/*  display_server.compat.inc                                             */
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

Error DisplayServer::_file_dialog_show_bind_compat_98194(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback) {
	return file_dialog_show(p_title, p_current_directory, p_filename, p_show_hidden, p_mode, p_filters, p_callback, MAIN_WINDOW_ID);
}

Error DisplayServer::_file_dialog_with_options_show_bind_compat_98194(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback) {
	return file_dialog_with_options_show(p_title, p_current_directory, p_root, p_filename, p_show_hidden, p_mode, p_filters, p_options, p_callback, MAIN_WINDOW_ID);
}

void DisplayServer::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("file_dialog_show", "title", "current_directory", "filename", "show_hidden", "mode", "filters", "callback"), &DisplayServer::_file_dialog_show_bind_compat_98194);
	ClassDB::bind_compatibility_method(D_METHOD("file_dialog_with_options_show", "title", "current_directory", "root", "filename", "show_hidden", "mode", "filters", "options", "callback"), &DisplayServer::_file_dialog_with_options_show_bind_compat_98194);
}

#endif
