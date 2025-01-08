/**************************************************************************/
/*  EditorWindowInfo.kt                                                   */
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

/**
 * Specifies the policy for launches.
 */
enum class LaunchPolicy {
	/**
	 * Launches happen in the same window.
	 */
	SAME,

	/**
	 * Adjacent launches are enabled.
	 */
	ADJACENT
}

/**
 * Describe the editor window to launch
 */
data class EditorWindowInfo(
	val windowClassName: String,
	val windowId: Int,
	val processNameSuffix: String,
	val launchPolicy: LaunchPolicy = LaunchPolicy.SAME
) {
	constructor(
		windowClass: Class<*>,
		windowId: Int,
		processNameSuffix: String,
		launchPolicy: LaunchPolicy = LaunchPolicy.SAME
	) : this(windowClass.name, windowId, processNameSuffix, launchPolicy)
}
