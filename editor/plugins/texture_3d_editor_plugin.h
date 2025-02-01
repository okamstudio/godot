/**************************************************************************/
/*  texture_3d_editor_plugin.h                                            */
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
#include "scene/gui/spin_box.h"
#include "scene/resources/shader.h"
#include "scene/resources/texture.h"

class ColorChannelSelector;

class Texture3DEditor : public Control {
	GDCLASS(Texture3DEditor, Control);

	SpinBox *layer = nullptr;
	Label *info = nullptr;
	Ref<Texture3D> texture;

	Ref<Shader> shader;
	Ref<ShaderMaterial> material;

	Control *texture_rect = nullptr;

	ColorChannelSelector *channel_selector = nullptr;

	bool setting = false;

	void _make_shaders();

	void _layer_changed(double) {
		if (!setting) {
			_update_material(false);
		}
	}

	void _texture_changed();

	void _texture_rect_update_area();
	void _texture_rect_draw();

	void _update_material(bool p_texture_changed);
	void _update_gui();

	void on_selected_channels_changed();

protected:
	void _notification(int p_what);

public:
	void edit(Ref<Texture3D> p_texture);

	Texture3DEditor();
	~Texture3DEditor();
};

class EditorInspectorPlugin3DTexture : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPlugin3DTexture, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class Texture3DEditorPlugin : public EditorPlugin {
	GDCLASS(Texture3DEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "Texture3D"; }

	Texture3DEditorPlugin();
};
