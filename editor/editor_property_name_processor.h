/**************************************************************************/
/*  editor_property_name_processor.h                                      */
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

#include "scene/main/node.h"

class EditorPropertyNameProcessor : public Node {
	GDCLASS(EditorPropertyNameProcessor, Node);

	static EditorPropertyNameProcessor *singleton;

	mutable HashMap<String, String> capitalize_string_cache;
	HashMap<String, String> capitalize_string_remaps;
	LocalVector<String> stop_words; // Exceptions that shouldn't be capitalized.

	HashMap<String, HashMap<String, StringName>> translation_contexts;

	// Capitalizes property path segments.
	String _capitalize_name(const String &p_name) const;

	// Returns the translation context for the given name.
	StringName _get_context(const String &p_name, const String &p_property, const StringName &p_class) const;

public:
	// Matches `interface/inspector/default_property_name_style` editor setting.
	enum Style {
		STYLE_RAW,
		STYLE_CAPITALIZED,
		STYLE_LOCALIZED,
	};

	static EditorPropertyNameProcessor *get_singleton() { return singleton; }

	static Style get_default_inspector_style();
	static Style get_settings_style();
	static Style get_tooltip_style(Style p_style);

	static bool is_localization_available();

	// Turns property path segment into the given style.
	// `p_class` and `p_property` are only used for `STYLE_LOCALIZED`, associating the name with a translation context.
	String process_name(const String &p_name, Style p_style, const String &p_property = "", const StringName &p_class = "") const;

	// Translate plain text group names.
	String translate_group_name(const String &p_name) const;

	EditorPropertyNameProcessor();
	~EditorPropertyNameProcessor();
};
