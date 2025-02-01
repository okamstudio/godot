/**************************************************************************/
/*  editor_sectioned_inspector.h                                          */
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

#include "scene/gui/split_container.h"

class CheckButton;
class EditorInspector;
class LineEdit;
class SectionedInspectorFilter;
class Tree;
class TreeItem;

class SectionedInspector : public HSplitContainer {
	GDCLASS(SectionedInspector, HSplitContainer);

	ObjectID obj;

	Tree *sections = nullptr;
	SectionedInspectorFilter *filter = nullptr;

	HashMap<String, TreeItem *> section_map;
	EditorInspector *inspector = nullptr;
	LineEdit *search_box = nullptr;
	CheckButton *advanced_toggle = nullptr;

	String selected_category;

	bool restrict_to_basic = false;

	static void _bind_methods();
	void _section_selected();

	void _search_changed(const String &p_what);
	void _advanced_toggled(bool p_toggled_on);

public:
	void register_search_box(LineEdit *p_box);
	void register_advanced_toggle(CheckButton *p_toggle);

	EditorInspector *get_inspector();
	void edit(Object *p_object);
	String get_full_item_path(const String &p_item);

	void set_current_section(const String &p_section);
	String get_current_section() const;

	void update_category_list();

	SectionedInspector();
	~SectionedInspector();
};
