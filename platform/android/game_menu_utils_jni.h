/**************************************************************************/
/*  game_menu_utils_jni.h                                                 */
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

#ifndef GAME_MENU_UTILS_JNI_H
#define GAME_MENU_UTILS_JNI_H

#include <jni.h>

extern "C" {
JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setSuspend(JNIEnv *env, jclass clazz, jboolean enabled);
JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_nextFrame(JNIEnv *env, jclass clazz);
JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setNodeType(JNIEnv *env, jclass clazz, jint type);
JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setSelectMode(JNIEnv *env, jclass clazz, jint mode);
JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setSelectionVisible(JNIEnv *env, jclass clazz, jboolean visible);
JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setCameraOverride(JNIEnv *env, jclass clazz, jboolean enabled);
JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_setCameraManipulateMode(JNIEnv *env, jclass clazz, jint mode);
JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_resetCamera2DPosition(JNIEnv *env, jclass clazz);
JNIEXPORT void JNICALL Java_org_godotengine_godot_utils_GameMenuUtils_resetCamera3DPosition(JNIEnv *env, jclass clazz);
}

#endif // GAME_MENU_UTILS_JNI_H
