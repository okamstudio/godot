/*************************************************************************/
/*  GodotXRGame.kt                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

package org.godotengine.editor

import org.godotengine.godot.GodotLib
import org.godotengine.godot.xr.XRMode

/**
 * Provide support for running XR apps / games from the editor window.
 */
open class GodotXRGame: GodotGame() {

	override fun overrideOrientationRequest() = true

	override fun updateCommandLineParams(args: Array<String>) {
		val updatedArgs = ArrayList<String>()
		if (!args.contains(XRMode.OPENXR.cmdLineArg)) {
			updatedArgs.add(XRMode.OPENXR.cmdLineArg)
		}
		if (!args.contains(XR_MODE_ARG)) {
			updatedArgs.add(XR_MODE_ARG)
			updatedArgs.add("on")
		}
		updatedArgs.addAll(args)

		super.updateCommandLineParams(updatedArgs.toTypedArray())
	}

	override fun getEditorWindowInfo() = XR_RUN_GAME_INFO

	override fun getProjectPermissionsToEnable(): MutableList<String> {
		val permissionsToEnable = super.getProjectPermissionsToEnable()

		val xrRuntimePermission = getXRRuntimePermissions()
		if (xrRuntimePermission.isNotEmpty() && GodotLib.getGlobal("xr/openxr/enabled").toBoolean()) {
			// We only request permissions when the `automatically_request_runtime_permissions`
			// project setting is enabled.
			// If the project setting is not defined, we fall-back to the default behavior which is
			// to automatically request permissions.
			val automaticallyRequestPermissionsSetting = GodotLib.getGlobal("xr/openxr/extensions/automatically_request_runtime_permissions")
			val automaticPermissionsRequestEnabled = automaticallyRequestPermissionsSetting.isNullOrEmpty() ||
				automaticallyRequestPermissionsSetting.toBoolean()
			if (automaticPermissionsRequestEnabled) {
				permissionsToEnable.addAll(xrRuntimePermission)
			}
		}

		return permissionsToEnable
	}
}
