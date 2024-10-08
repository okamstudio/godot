/**************************************************************************/
/*  AndroidRuntimePlugin.kt                                               */
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

package org.godotengine.godot.plugin

import org.godotengine.godot.Godot

/**
 * Provides access to the Android runtime capabilities.
 *
 * For example, from gdscript, developers can use [getApplicationContext] to access system services
 * and check if the device supports vibration.
 *
 * var android_runtime = Engine.get_singleton("AndroidRuntime")
 * 	if android_runtime:
 * 		print("Checking if the device supports vibration")
 * 		var vibrator_service = android_runtime.getApplicationContext().getSystemService("vibrator")
 * 		if vibrator_service:
 * 			if vibrator_service.hasVibrator():
 * 				print("Vibration is supported on device!")
 * 			else:
 * 				printerr("Vibration is not supported on device")
 * 		else:
 * 			printerr("Unable to retrieve the vibrator service")
 * 	else:
 * 		printerr("Couldn't find AndroidRuntime singleton")
 */
class AndroidRuntimePlugin(godot: Godot) : GodotPlugin(godot) {
	override fun getPluginName() = "AndroidRuntime"

	@UsedByGodot
	fun getApplicationContext() = activity?.applicationContext

	@UsedByGodot
	override fun getActivity() = super.getActivity()
}
