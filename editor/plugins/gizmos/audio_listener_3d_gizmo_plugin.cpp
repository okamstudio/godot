/**************************************************************************/
/*  audio_listener_3d_gizmo_plugin.cpp                                    */
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

#include "audio_listener_3d_gizmo_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "scene/3d/audio_listener_3d.h"

AudioListener3DGizmoPlugin::AudioListener3DGizmoPlugin() {
	create_icon_material("audio_listener_3d_icon", EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("GizmoAudioListener3D"), EditorStringName(EditorIcons)));
}

bool AudioListener3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<AudioListener3D>(p_spatial) != nullptr;
}

String AudioListener3DGizmoPlugin::get_gizmo_name() const {
	return "AudioListener3D";
}

int AudioListener3DGizmoPlugin::get_priority() const {
	return -1;
}

void AudioListener3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	const Ref<Material> icon = get_material("audio_listener_3d_icon", p_gizmo);
	p_gizmo->add_unscaled_billboard(icon, 0.05);
}
