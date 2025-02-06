/**************************************************************************/
/*  BaseGodotGame.kt                                                      */
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

package org.godotengine.editor

import android.Manifest
import android.util.Log
import androidx.annotation.CallSuper
import org.godotengine.godot.GodotLib
import org.godotengine.godot.utils.GameMenuUtils
import org.godotengine.godot.utils.PermissionsUtil
import org.godotengine.godot.utils.ProcessPhoenix

/**
 * Base class for the Godot play windows.
 */
abstract class BaseGodotGame: GodotEditor() {
	companion object {
		private val TAG = BaseGodotGame::class.java.simpleName
	}

	override fun enableLongPressGestures() = java.lang.Boolean.parseBoolean(GodotLib.getGlobal("input_devices/pointing/android/enable_long_press_as_right_click"))

	override fun enablePanAndScaleGestures() = java.lang.Boolean.parseBoolean(GodotLib.getGlobal("input_devices/pointing/android/enable_pan_and_scale_gestures"))

	override fun onGodotSetupCompleted() {
		super.onGodotSetupCompleted()
		Log.v(TAG, "OnGodotSetupCompleted")

		// Check if we should be running in XR instead (if available) as it's possible we were
		// launched from the project manager which doesn't have that information.
		val launchingArgs = intent.getStringArrayExtra(EXTRA_COMMAND_LINE_PARAMS)
		if (launchingArgs != null) {
			val editorWindowInfo = retrieveEditorWindowInfo(launchingArgs, getEditorGameEmbedMode())
			if (editorWindowInfo != getEditorWindowInfo()) {
				val relaunchIntent = getNewGodotInstanceIntent(editorWindowInfo, launchingArgs)
				relaunchIntent.putExtra(EXTRA_NEW_LAUNCH, true)
					.putExtra(EditorMessageDispatcher.EXTRA_MSG_DISPATCHER_PAYLOAD, intent.getBundleExtra(EditorMessageDispatcher.EXTRA_MSG_DISPATCHER_PAYLOAD))

				Log.d(TAG, "Relaunching XR project using ${editorWindowInfo.windowClassName} with parameters ${launchingArgs.contentToString()}")
				val godot = godot
				if (godot != null) {
					godot.destroyAndKillProcess {
						ProcessPhoenix.triggerRebirth(this, relaunchIntent)
					}
				} else {
					ProcessPhoenix.triggerRebirth(this, relaunchIntent)
				}
				return
			}
		}

		// Request project runtime permissions if necessary
		val permissionsToEnable = getProjectPermissionsToEnable()
		if (permissionsToEnable.isNotEmpty()) {
			PermissionsUtil.requestPermissions(this, permissionsToEnable)
		}
	}

	/**
	 * Check for project permissions to enable
	 */
	@CallSuper
	protected open fun getProjectPermissionsToEnable(): MutableList<String> {
		val permissionsToEnable = mutableListOf<String>()

		// Check for RECORD_AUDIO permission
		val audioInputEnabled = java.lang.Boolean.parseBoolean(GodotLib.getGlobal("audio/driver/enable_input"))
		if (audioInputEnabled) {
			permissionsToEnable.add(Manifest.permission.RECORD_AUDIO)
		}

		return permissionsToEnable
	}

	protected open fun getEditorGameEmbedMode() = GameMenuUtils.GameEmbedMode.AUTO
}
