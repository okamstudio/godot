/**************************************************************************/
/*  game_menu_utils_jni.cpp                                               */
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

#include "game_menu_utils_jni.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/plugins/game_view_plugin.h"
#endif

extern "C" {

JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setSuspend(JNIEnv *env, jclass clazz, jboolean enabled) {
#ifdef TOOLS_ENABLED
	GameViewPlugin *game_view_plugin = Object::cast_to<GameViewPlugin>(EditorNode::get_singleton()->get_editor_main_screen()->get_plugin_by_name("Game"));
	if (game_view_plugin != nullptr && game_view_plugin->get_debugger().is_valid()) {
		game_view_plugin->get_debugger()->set_suspend(enabled);
	}
#endif
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_nextFrame(JNIEnv *env, jclass clazz) {
#ifdef TOOLS_ENABLED
	GameViewPlugin *game_view_plugin = Object::cast_to<GameViewPlugin>(EditorNode::get_singleton()->get_editor_main_screen()->get_plugin_by_name("Game"));
	if (game_view_plugin != nullptr && game_view_plugin->get_debugger().is_valid()) {
		game_view_plugin->get_debugger()->next_frame();
	}
#endif
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setNodeType(JNIEnv *env, jclass clazz, jint type) {
#ifdef TOOLS_ENABLED
	GameViewPlugin *game_view_plugin = Object::cast_to<GameViewPlugin>(EditorNode::get_singleton()->get_editor_main_screen()->get_plugin_by_name("Game"));
	if (game_view_plugin != nullptr && game_view_plugin->get_debugger().is_valid()) {
		game_view_plugin->get_debugger()->set_node_type(type);
	}
#endif
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setSelectMode(JNIEnv *env, jclass clazz, jint mode) {
#ifdef TOOLS_ENABLED
	GameViewPlugin *game_view_plugin = Object::cast_to<GameViewPlugin>(EditorNode::get_singleton()->get_editor_main_screen()->get_plugin_by_name("Game"));
	if (game_view_plugin != nullptr && game_view_plugin->get_debugger().is_valid()) {
		game_view_plugin->get_debugger()->set_select_mode(mode);
	}
#endif
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setSelectionVisible(JNIEnv *env, jclass clazz, jboolean visible) {
#ifdef TOOLS_ENABLED
	GameViewPlugin *game_view_plugin = Object::cast_to<GameViewPlugin>(EditorNode::get_singleton()->get_editor_main_screen()->get_plugin_by_name("Game"));
	if (game_view_plugin != nullptr && game_view_plugin->get_debugger().is_valid()) {
		game_view_plugin->get_debugger()->set_selection_visible(visible);
	}
#endif
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setCameraOverride(JNIEnv *env, jclass clazz, jboolean enabled) {
#ifdef TOOLS_ENABLED
	GameViewPlugin *game_view_plugin = Object::cast_to<GameViewPlugin>(EditorNode::get_singleton()->get_editor_main_screen()->get_plugin_by_name("Game"));
	if (game_view_plugin != nullptr && game_view_plugin->get_debugger().is_valid()) {
		game_view_plugin->get_debugger()->set_camera_override(enabled);
	}
#endif
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setCameraManipulateMode(JNIEnv *env, jclass clazz, jint mode) {
#ifdef TOOLS_ENABLED
	GameViewPlugin *game_view_plugin = Object::cast_to<GameViewPlugin>(EditorNode::get_singleton()->get_editor_main_screen()->get_plugin_by_name("Game"));
	if (game_view_plugin != nullptr && game_view_plugin->get_debugger().is_valid()) {
		game_view_plugin->get_debugger()->set_camera_manipulate_mode(static_cast<EditorDebuggerNode::CameraOverride>(mode));
	}
#endif
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_resetCamera2DPosition(JNIEnv *env, jclass clazz) {
#ifdef TOOLS_ENABLED
	GameViewPlugin *game_view_plugin = Object::cast_to<GameViewPlugin>(EditorNode::get_singleton()->get_editor_main_screen()->get_plugin_by_name("Game"));
	if (game_view_plugin != nullptr && game_view_plugin->get_debugger().is_valid()) {
		game_view_plugin->get_debugger()->reset_camera_2d_position();
	}
#endif
}

JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_resetCamera3DPosition(JNIEnv *env, jclass clazz) {
#ifdef TOOLS_ENABLED
	GameViewPlugin *game_view_plugin = Object::cast_to<GameViewPlugin>(EditorNode::get_singleton()->get_editor_main_screen()->get_plugin_by_name("Game"));
	if (game_view_plugin != nullptr && game_view_plugin->get_debugger().is_valid()) {
		game_view_plugin->get_debugger()->reset_camera_3d_position();
	}
#endif
}
}
