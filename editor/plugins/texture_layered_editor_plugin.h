/**************************************************************************/
/*  texture_layered_editor_plugin.h                                       */
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

#ifndef TEXTURE_LAYERED_EDITOR_PLUGIN_H
#define TEXTURE_LAYERED_EDITOR_PLUGIN_H

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/spin_box.h"
#include "scene/resources/shader.h"
#include "scene/resources/texture.h"

class TextureLayeredEditor : public Control {
	GDCLASS(TextureLayeredEditor, Control);

	SpinBox *layer = nullptr;
	Label *info = nullptr;
	Ref<TextureLayered> texture;

	Ref<Shader> shaders[3];
	Ref<ShaderMaterial> materials[3];

	float x_rot = 0;
	float y_rot = 0;
	Control *texture_rect = nullptr;

	bool setting = false;

	void _make_shaders();
	void _update_material(bool p_texture_changed);

	void _layer_changed(double) {
		if (!setting) {
			_update_material(false);
		}
	}

	void _texture_changed();

	void _texture_rect_update_area();
	void _texture_rect_draw();

	void _update_gui();

protected:
	void _notification(int p_what);
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	void edit(Ref<TextureLayered> p_texture);

	TextureLayeredEditor();
	~TextureLayeredEditor();
};

class EditorInspectorPluginLayeredTexture : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginLayeredTexture, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class TextureLayeredEditorPlugin : public EditorPlugin {
	GDCLASS(TextureLayeredEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "TextureLayered"; }

	TextureLayeredEditorPlugin();
};

#endif // TEXTURE_LAYERED_EDITOR_PLUGIN_H
