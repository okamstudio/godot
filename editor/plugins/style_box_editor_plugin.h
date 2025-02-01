/**************************************************************************/
/*  style_box_editor_plugin.h                                             */
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

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/texture_rect.h"

class Button;
class StyleBox;

class StyleBoxPreview : public TextureRect {
	GDCLASS(StyleBoxPreview, TextureRect);

	Button *grid_preview = nullptr;
	Ref<StyleBox> stylebox;

	void _sb_changed();
	void _redraw();
	static bool grid_preview_enabled;
	void _grid_preview_toggled(bool p_active);

protected:
	void _notification(int p_what);

public:
	void edit(const Ref<StyleBox> &p_stylebox);

	StyleBoxPreview();
};

class EditorInspectorPluginStyleBox : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginStyleBox, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class StyleBoxEditorPlugin : public EditorPlugin {
	GDCLASS(StyleBoxEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "StyleBox"; }

	StyleBoxEditorPlugin();
};
