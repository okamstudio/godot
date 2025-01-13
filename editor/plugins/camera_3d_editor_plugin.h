/**************************************************************************/
/*  camera_3d_editor_plugin.h                                             */
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

#ifndef CAMERA_3D_EDITOR_PLUGIN_H
#define CAMERA_3D_EDITOR_PLUGIN_H

#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/texture_editor_plugin.h"

class Camera3D;
class SubViewport;

class Camera3DEditor : public Control {
	GDCLASS(Camera3DEditor, Control);

	Panel *panel = nullptr;
	Button *preview = nullptr;
	Node *node = nullptr;

	void _pressed();

protected:
	void _node_removed(Node *p_node);

public:
	void edit(Node *p_camera);
	Camera3DEditor();
};

class Camera3DPreview : public TexturePreview {
	GDCLASS(Camera3DPreview, TexturePreview);

	Camera3D *camera = nullptr;
	SubViewport *sub_viewport = nullptr;

	void _update_sub_viewport_size();

public:
	Camera3DPreview(Camera3D *p_camera);
};

class EditorInspectorPluginCamera3DPreview : public EditorInspectorPluginTexture {
	GDCLASS(EditorInspectorPluginCamera3DPreview, EditorInspectorPluginTexture);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class Camera3DEditorPlugin : public EditorPlugin {
	GDCLASS(Camera3DEditorPlugin, EditorPlugin);

public:
	virtual String get_name() const override { return "Camera3D"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	Camera3DEditorPlugin();
	~Camera3DEditorPlugin();
};

#endif // CAMERA_3D_EDITOR_PLUGIN_H
