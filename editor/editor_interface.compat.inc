/**************************************************************************/
/*  editor_interface.compat.inc                                           */
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

#include "editor_interface.h"

#ifndef DISABLE_DEPRECATED

void EditorInterface::_popup_node_selector_bind_compat_94323(const Callable &p_callback, const TypedArray<StringName> &p_valid_types) {
	popup_node_selector(p_callback, p_valid_types, nullptr);
}

void EditorInterface::_popup_property_selector_bind_compat_94323(Object *p_object, const Callable &p_callback, const PackedInt32Array &p_type_filter) {
	popup_property_selector(p_object, p_callback, p_type_filter, String());
}

void EditorInterface::_open_scene_from_path_bind_compat_90057(const String &scene_path) {
	return open_scene_from_path(scene_path, false);
}

void EditorInterface::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("popup_node_selector", "callback", "valid_types"), &EditorInterface::_popup_node_selector_bind_compat_94323, DEFVAL(TypedArray<StringName>()));
	ClassDB::bind_compatibility_method(D_METHOD("popup_property_selector", "object", "callback", "type_filter"), &EditorInterface::_popup_property_selector_bind_compat_94323, DEFVAL(PackedInt32Array()));
	ClassDB::bind_compatibility_method(D_METHOD("open_scene_from_path", "scene_path"), &EditorInterface::_open_scene_from_path_bind_compat_90057);
}

#endif
