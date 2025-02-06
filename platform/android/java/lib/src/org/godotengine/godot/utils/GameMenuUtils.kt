/**************************************************************************/
/*  GameMenuUtils.kt                                                      */
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

package org.godotengine.godot.utils

import android.util.Log
import org.godotengine.godot.GodotLib

/**
 * Utility class for accessing and using game menu APIs.
 */
object GameMenuUtils {
	private val TAG = GameMenuUtils::class.java.simpleName

	/**
	 * Enum representing the "run/window_placement/game_embed_mode" editor settings
 	 */
	enum class GameEmbedMode(internal val nativeValue: Int) {
		DISABLED(-1), AUTO(0), ENABLED(1);

		companion object {
			internal const val SETTING_KEY = "run/window_placement/game_embed_mode"

			@JvmStatic
			internal fun fromNativeValue(nativeValue: Int): GameEmbedMode? {
				return when (nativeValue) {
					DISABLED.nativeValue -> DISABLED
					AUTO.nativeValue -> AUTO
					ENABLED.nativeValue -> ENABLED
					else -> null
				}
			}
		}
	}

	@JvmStatic
	external fun setSuspend(enabled: Boolean)

	@JvmStatic
	external fun nextFrame()

	@JvmStatic
	external fun setNodeType(type: Int)

	@JvmStatic
	external fun setSelectMode(mode: Int)

	@JvmStatic
	external fun setSelectionVisible(visible: Boolean)

	@JvmStatic
	external fun setCameraOverride(enabled: Boolean)

	@JvmStatic
	external fun setCameraManipulateMode(mode: Int)

	@JvmStatic
	external fun resetCamera2DPosition()

	@JvmStatic
	external fun resetCamera3DPosition()

	/**
	 * Returns true if the game should be embedded in the editor on play, false otherwise.
	 *
	 * Must be called on the render thread.
	 */
	fun fetchGameEmbedMode(): GameEmbedMode {
		try {
			val gameEmbedModeValue = Integer.parseInt(GodotLib.getEditorSetting(GameEmbedMode.SETTING_KEY))
			val gameEmbedMode = GameEmbedMode.fromNativeValue(gameEmbedModeValue) ?: GameEmbedMode.AUTO
			return gameEmbedMode
		} catch (e: Exception) {
			Log.w(TAG, "Unable to retrieve game embed mode", e)
			return GameEmbedMode.AUTO
		}
	}

	/**
	 * Update the 'game_embed_mode' editor setting.
	 *
	 * Must be called on the render thread.
	 */
	fun saveGameEmbedMode(gameEmbedMode: GameEmbedMode) {
		GodotLib.setEditorSetting(GameEmbedMode.SETTING_KEY, gameEmbedMode.nativeValue)
	}
}
