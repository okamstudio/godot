/**************************************************************************/
/*  resource_preloader_editor_plugin.h                                    */
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
#include "scene/gui/dialogs.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/tree.h"
#include "scene/main/resource_preloader.h"

class EditorFileDialog;

class ResourcePreloaderEditor : public PanelContainer {
	GDCLASS(ResourcePreloaderEditor, PanelContainer);

	enum {
		BUTTON_OPEN_SCENE,
		BUTTON_EDIT_RESOURCE,
		BUTTON_REMOVE
	};

	Button *load = nullptr;
	Button *paste = nullptr;
	Tree *tree = nullptr;
	bool loading_scene;

	EditorFileDialog *file = nullptr;

	AcceptDialog *dialog = nullptr;

	ResourcePreloader *preloader = nullptr;

	void _load_pressed();
	void _files_load_request(const Vector<String> &p_paths);
	void _paste_pressed();
	void _remove_resource(const String &p_to_remove);
	void _update_library();
	void _cell_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button);
	void _item_edited();

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

protected:
	void _notification(int p_what);

	static void _bind_methods();

public:
	void edit(ResourcePreloader *p_preloader);
	ResourcePreloaderEditor();
};

class ResourcePreloaderEditorPlugin : public EditorPlugin {
	GDCLASS(ResourcePreloaderEditorPlugin, EditorPlugin);

	ResourcePreloaderEditor *preloader_editor = nullptr;
	Button *button = nullptr;

public:
	virtual String get_plugin_name() const override { return "ResourcePreloader"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	ResourcePreloaderEditorPlugin();
	~ResourcePreloaderEditorPlugin();
};
