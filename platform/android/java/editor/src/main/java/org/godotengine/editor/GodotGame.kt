/**************************************************************************/
/*  GodotGame.kt                                                          */
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

import android.app.PictureInPictureParams
import android.content.pm.PackageManager
import android.graphics.Rect
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.view.View
import androidx.core.view.isVisible
import org.godotengine.editor.embed.GameMenuFragment
import org.godotengine.godot.utils.GameMenuUtils
import org.godotengine.godot.utils.ProcessPhoenix
import org.godotengine.godot.utils.isNativeXRDevice

/**
 * Drives the 'run project' window of the Godot Editor.
 */
open class GodotGame : BaseGodotGame(), GameMenuFragment.GameMenuListener {

	companion object {
		private val TAG = GodotGame::class.java.simpleName
	}

	private val gameViewSourceRectHint = Rect()
	private val gameMenuContainer: View? by lazy {
		findViewById(R.id.game_menu_fragment_container)
	}
	protected var gameMenuFragment : GameMenuFragment? = null

	override fun onCreate(savedInstanceState: Bundle?) {
		super.onCreate(savedInstanceState)

		val editorHint = intent.getBooleanExtra(EXTRA_EDITOR_HINT, false)

		gameMenuContainer?.isVisible = editorHint

		if (editorHint && gameMenuContainer != null) {
			// Set up the game menu fragment if we have a container for it
			var currentFragment = supportFragmentManager.findFragmentById(R.id.game_menu_fragment_container)
			if (currentFragment !is GameMenuFragment) {
				Log.v(TAG, "Creating game menu fragment instance")
				currentFragment = GameMenuFragment().apply {
					arguments = Bundle().apply {
						putBundle(EXTRA_GAME_MENU_STATE, intent.getBundleExtra(EXTRA_GAME_MENU_STATE))
					}
				}
				supportFragmentManager.beginTransaction()
					.replace(R.id.game_menu_fragment_container, currentFragment, GameMenuFragment.TAG)
					.commitNowAllowingStateLoss()
			}

			gameMenuFragment = currentFragment
		}

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
			val gameView = findViewById<View>(R.id.godot_fragment_container)
			gameView?.addOnLayoutChangeListener { v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom ->
				gameView.getGlobalVisibleRect(gameViewSourceRectHint)
			}
		}
	}

	override fun enterPiPMode() {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && hasPiPSystemFeature()) {
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
				val builder = PictureInPictureParams.Builder().setSourceRectHint(gameViewSourceRectHint)
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
					builder.setSeamlessResizeEnabled(false)
				}
				setPictureInPictureParams(builder.build())
			}

			Log.v(TAG, "Entering PiP mode")
			enterPictureInPictureMode()
		}
	}

	/**
	 * Returns true the if the device supports picture-in-picture (PiP)
	 */
	protected fun hasPiPSystemFeature(): Boolean {
		return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N &&
			packageManager.hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)
	}

	override fun onPictureInPictureModeChanged(isInPictureInPictureMode: Boolean) {
		super.onPictureInPictureModeChanged(isInPictureInPictureMode)
		Log.v(TAG, "onPictureInPictureModeChanged: $isInPictureInPictureMode")

		// Hide the game menu fragment when in PiP
		gameMenuContainer?.isVisible = !isInPictureInPictureMode
	}

	override fun onStop() {
		super.onStop()

		val isInPiPMode = Build.VERSION.SDK_INT >= Build.VERSION_CODES.N && isInPictureInPictureMode
		if (isInPiPMode && !isFinishing) {
			// We get in this state when PiP is closed, so we terminate the activity.
			finish()
		}
	}

	override fun getGodotAppLayout() = R.layout.godot_game_layout

	override fun getEditorWindowInfo() = RUN_GAME_INFO

	override fun getEditorGameEmbedMode() = GameMenuUtils.GameEmbedMode.DISABLED

	override fun overrideOrientationRequest() = false

	override fun suspendGame(suspended: Boolean) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_SUSPEND)
			putBoolean(KEY_GAME_MENU_ACTION_PARAM1, suspended)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun dispatchNextFrame() {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_NEXT_FRAME)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun toggleSelectionVisibility(enabled: Boolean) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_SELECTION_VISIBLE)
			putBoolean(KEY_GAME_MENU_ACTION_PARAM1, enabled)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun overrideCamera(enabled: Boolean) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_CAMERA_OVERRIDE)
			putBoolean(KEY_GAME_MENU_ACTION_PARAM1, enabled)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun selectRuntimeNode(nodeType: GameMenuFragment.GameMenuListener.RuntimeNodeType) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_NODE_TYPE)
			putInt(KEY_GAME_MENU_ACTION_PARAM1, nodeType.ordinal)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun selectRuntimeNodeSelectMode(selectMode: GameMenuFragment.GameMenuListener.RuntimeNodeSelectMode) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_SELECT_MODE)
			putInt(KEY_GAME_MENU_ACTION_PARAM1, selectMode.ordinal)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun reset2DCamera() {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_RESET_CAMERA_2D_POSITION)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun reset3DCamera() {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_RESET_CAMERA_3D_POSITION)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun manipulateCamera(mode: GameMenuFragment.GameMenuListener.CameraMode) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_SET_CAMERA_MANIPULATE_MODE)
			putInt(KEY_GAME_MENU_ACTION_PARAM1, mode.ordinal)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun embedGameOnPlay(embedded: Boolean) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_EMBED_GAME_ON_PLAY)
			putBoolean(KEY_GAME_MENU_ACTION_PARAM1, embedded)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun isGameEmbedded() = false

	override fun isGameEmbeddingSupported() = !isNativeXRDevice(applicationContext)

	override fun isMinimizedButtonEnabled() = !isNativeXRDevice(applicationContext)

	override fun isCloseButtonEnabled() = !isNativeXRDevice(applicationContext)

	override fun isAlwaysOnTopSupported() = false

	override fun isPiPButtonEnabled() = hasPiPSystemFeature()

	override fun onAlwaysOnTopUpdated(alwaysOnTopEnabled: Boolean) {
		val actionBundle = Bundle().apply {
			putString(KEY_GAME_MENU_ACTION, GAME_MENU_ACTION_ALWAYS_ON_TOP)
			putBoolean(KEY_GAME_MENU_ACTION_PARAM1, alwaysOnTopEnabled)
		}
		editorMessageDispatcher.dispatchGameMenuAction(EDITOR_MAIN_INFO, actionBundle)
	}

	override fun minimizeGameWindow() {
		moveTaskToBack(false)
	}

	override fun closeGameWindow() {
		ProcessPhoenix.forceQuit(this)
	}

}
