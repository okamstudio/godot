/**************************************************************************/
/*  gradient_editor_plugin.h                                              */
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

#ifndef GRADIENT_EDITOR_PLUGIN_H
#define GRADIENT_EDITOR_PLUGIN_H

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"

class EditorSpinSlider;
class ColorPicker;
class PopupPanel;
class GradientTexture1D;

class GradientEdit : public Control {
	GDCLASS(GradientEdit, Control);

	Ref<Gradient> gradient;
	Ref<GradientTexture1D> preview_texture;

	PopupPanel *popup = nullptr;
	ColorPicker *picker = nullptr;

	bool snap_enabled = false;
	int snap_count = 10;

	enum GrabMode {
		GRAB_NONE,
		GRAB_ADD,
		GRAB_MOVE
	};

	GrabMode grabbing = GRAB_NONE;
	float pre_grab_offset = 0.5;
	int pre_grab_index = -1;
	int selected_index = -1;
	int hovered_index = -1;

	// Make sure to use the scaled values below.
	const int BASE_SPACING = 4;
	const int BASE_HANDLE_WIDTH = 8;

	int draw_spacing = BASE_SPACING;
	int handle_width = BASE_HANDLE_WIDTH;

	int _get_gradient_rect_width() const;

	void _color_changed(const Color &p_color);
	void _redraw();

	int _get_point_at(int p_xpos) const;
	int _predict_insertion_index(float p_offset);
	void _show_color_picker();

protected:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_gradient(const Ref<Gradient> &p_gradient);
	const Ref<Gradient> &get_gradient() const;

	ColorPicker *get_picker() const;
	PopupPanel *get_popup() const;

	void set_selected_index(int p_index);

	void add_point(float p_offset, const Color &p_color);
	void remove_point(int p_index);
	void set_offset(int p_index, float p_offset);
	void set_color(int p_index, const Color &p_color);
	void reverse_gradient();

	void set_snap_enabled(bool p_enabled);
	void set_snap_count(int p_count);

	GradientEdit();
};

class GradientEditor : public VBoxContainer {
	GDCLASS(GradientEditor, VBoxContainer);

	Button *reverse_button = nullptr;
	Button *snap_button = nullptr;
	EditorSpinSlider *snap_count_edit = nullptr;
	GradientEdit *gradient_editor_rect = nullptr;

	void _set_snap_enabled(bool p_enabled);
	void _set_snap_count(int p_count);

protected:
	void _notification(int p_what);

public:
	static const int DEFAULT_SNAP;
	void set_gradient(const Ref<Gradient> &p_gradient);

	GradientEditor();
};

class EditorInspectorPluginGradient : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginGradient, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class GradientEditorPlugin : public EditorPlugin {
	GDCLASS(GradientEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "Gradient"; }

	GradientEditorPlugin();
};

#endif // GRADIENT_EDITOR_PLUGIN_H
