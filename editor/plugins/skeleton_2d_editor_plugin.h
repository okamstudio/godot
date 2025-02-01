/**************************************************************************/
/*  skeleton_2d_editor_plugin.h                                           */
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

#include "editor/plugins/editor_plugin.h"
#include "scene/2d/skeleton_2d.h"

class AcceptDialog;
class MenuButton;

class Skeleton2DEditor : public Control {
	GDCLASS(Skeleton2DEditor, Control);

	enum Menu {
		MENU_OPTION_SET_REST,
		MENU_OPTION_MAKE_REST,
	};

	Skeleton2D *node = nullptr;

	MenuButton *options = nullptr;
	AcceptDialog *err_dialog = nullptr;

	void _menu_option(int p_option);

	//void _create_uv_lines();
	friend class Skeleton2DEditorPlugin;

protected:
	void _node_removed(Node *p_node);

public:
	void edit(Skeleton2D *p_sprite);
	Skeleton2DEditor();
};

class Skeleton2DEditorPlugin : public EditorPlugin {
	GDCLASS(Skeleton2DEditorPlugin, EditorPlugin);

	Skeleton2DEditor *sprite_editor = nullptr;

public:
	virtual String get_plugin_name() const override { return "Skeleton2D"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	Skeleton2DEditorPlugin();
	~Skeleton2DEditorPlugin();
};
