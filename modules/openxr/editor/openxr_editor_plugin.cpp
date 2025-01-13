/**************************************************************************/
/*  openxr_editor_plugin.cpp                                              */
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

#include "openxr_editor_plugin.h"

#include "../action_map/openxr_action_map.h"

#include "editor/editor_command_palette.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_bottom_panel.h"

void OpenXREditorPlugin::edit(Object *p_node) {
	if (Object::cast_to<OpenXRActionMap>(p_node)) {
		String path = Object::cast_to<OpenXRActionMap>(p_node)->get_path();
		if (path.is_resource_file()) {
			action_map_editor->open_action_map(path);
		}
	}
}

bool OpenXREditorPlugin::handles(Object *p_node) const {
	return (Object::cast_to<OpenXRActionMap>(p_node) != nullptr);
}

void OpenXREditorPlugin::make_visible(bool p_visible) {
}

OpenXREditorPlugin::OpenXREditorPlugin() {
	action_map_editor = memnew(OpenXRActionMapEditor);
	EditorNode::get_bottom_panel()->add_item(TTR("OpenXR Action Map"), action_map_editor, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_openxr_action_map_bottom_panel", TTR("Toggle OpenXR Action Map Bottom Panel")));

	binding_modifier_inspector_plugin = Ref<EditorInspectorPluginBindingModifier>(memnew(EditorInspectorPluginBindingModifier));
	EditorInspector::add_inspector_plugin(binding_modifier_inspector_plugin);

#ifndef ANDROID_ENABLED
	select_runtime = memnew(OpenXRSelectRuntime);
	add_control_to_container(CONTAINER_TOOLBAR, select_runtime);
#endif
}

OpenXREditorPlugin::~OpenXREditorPlugin() {
}
