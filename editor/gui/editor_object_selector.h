/**************************************************************************/
/*  editor_object_selector.h                                              */
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

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/texture_rect.h"

class EditorSelectionHistory;

class EditorObjectSelector : public Button {
	GDCLASS(EditorObjectSelector, Button);

	EditorSelectionHistory *history = nullptr;

	TextureRect *current_object_icon = nullptr;
	Label *current_object_label = nullptr;
	TextureRect *sub_objects_icon = nullptr;
	PopupMenu *sub_objects_menu = nullptr;

	Vector<ObjectID> objects;

	void _show_popup();
	void _id_pressed(int p_idx);
	void _about_to_show();
	void _add_children_to_popup(Object *p_obj, int p_depth = 0);

protected:
	void _notification(int p_what);

public:
	virtual Size2 get_minimum_size() const override;

	void update_path();
	void clear_path();
	void enable_path();

	EditorObjectSelector(EditorSelectionHistory *p_history);
};
