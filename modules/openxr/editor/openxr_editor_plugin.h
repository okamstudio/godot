/**************************************************************************/
/*  openxr_editor_plugin.h                                                */
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

#ifndef OPENXR_EDITOR_PLUGIN_H
#define OPENXR_EDITOR_PLUGIN_H

#include "openxr_action_map_editor.h"
#include "openxr_binding_modifier_editor.h"
#include "openxr_select_runtime.h"

#include "editor/plugins/editor_plugin.h"

class OpenXREditorPlugin : public EditorPlugin {
	GDCLASS(OpenXREditorPlugin, EditorPlugin);

	OpenXRActionMapEditor *action_map_editor = nullptr;
	Ref<EditorInspectorPluginBindingModifier> binding_modifier_inspector_plugin = nullptr;
#ifndef ANDROID_ENABLED
	OpenXRSelectRuntime *select_runtime = nullptr;
#endif

public:
	virtual String get_plugin_name() const override { return "OpenXRPlugin"; }
	bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_node) override;
	virtual bool handles(Object *p_node) const override;
	virtual void make_visible(bool p_visible) override;

	OpenXREditorPlugin();
	~OpenXREditorPlugin();
};

#endif // OPENXR_EDITOR_PLUGIN_H
