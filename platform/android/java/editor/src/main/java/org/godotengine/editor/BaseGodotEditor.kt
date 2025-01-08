/**************************************************************************/
/*  BaseGodotEditor.kt                                                    */
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
import android.app.ActivityManager
import android.app.ActivityOptions
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.*
import android.util.Log
import android.view.View
import android.view.WindowManager
import android.widget.Toast
import androidx.annotation.CallSuper
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.core.view.isVisible
import androidx.window.layout.WindowMetricsCalculator
import org.godotengine.editor.embed.EmbeddedGodotGame
import org.godotengine.editor.embed.GameMenuFragment
import org.godotengine.editor.utils.signApk
import org.godotengine.editor.utils.verifyApk
import org.godotengine.godot.BuildConfig
import org.godotengine.godot.GodotActivity
import org.godotengine.godot.GodotLib
import org.godotengine.godot.error.Error
import org.godotengine.godot.utils.GameMenuUtils
import org.godotengine.godot.utils.GameMenuUtils.GameEmbedMode
import org.godotengine.godot.utils.GameMenuUtils.fetchGameEmbedMode
import org.godotengine.godot.utils.PermissionsUtil
import org.godotengine.godot.utils.ProcessPhoenix
import org.godotengine.godot.utils.isNativeXRDevice
import kotlin.math.min

/**
 * Base class for the Godot Android Editor activities.
 *
 * This provides the basic templates for the activities making up this application.
 * Each derived activity runs in its own process, which enable up to have several instances of
 * the Godot engine up and running at the same time.
 */
abstract class BaseGodotEditor : GodotActivity() {

	companion object {
		private val TAG = BaseGodotEditor::class.java.simpleName

		private const val WAIT_FOR_DEBUGGER = false

		internal const val EXTRA_EDITOR_HINT = "editor_hint"
		internal const val EXTRA_PROJECT_MANAGER_HINT = "project_manager_hint"
		internal const val EXTRA_GAME_MENU_STATE = "game_menu_state"

		// Command line arguments
		private const val FULLSCREEN_ARG = "--fullscreen"
		private const val FULLSCREEN_ARG_SHORT = "-f"
		private const val EDITOR_ARG = "--editor"
		private const val EDITOR_ARG_SHORT = "-e"
		private const val EDITOR_PROJECT_MANAGER_ARG = "--project-manager"
		private const val EDITOR_PROJECT_MANAGER_ARG_SHORT = "-p"
		internal const val XR_MODE_ARG = "--xr-mode"

		// Info for the various classes used by the editor
		internal val EDITOR_MAIN_INFO = EditorWindowInfo(GodotEditor::class.java, 777, "")
		internal val RUN_GAME_INFO = EditorWindowInfo(GodotGame::class.java, 667, ":GodotGame", LaunchPolicy.ADJACENT)
		internal val EMBEDDED_RUN_GAME_INFO = EditorWindowInfo(EmbeddedGodotGame::class.java, 2667, ":EmbeddedGodotGame")
		internal val XR_RUN_GAME_INFO = EditorWindowInfo(GodotXRGame::class.java, 1667, ":GodotXRGame")

		/** Default behavior, means we check project settings **/
		private const val XR_MODE_DEFAULT = "default"

		/**
		 * Ignore project settings, OpenXR is disabled
		 */
		private const val XR_MODE_OFF = "off"

		/**
		 * Ignore project settings, OpenXR is enabled
		 */
		private const val XR_MODE_ON = "on"

		// Game menu constants
		internal const val KEY_GAME_MENU_ACTION = "key_game_menu_action"
		internal const val KEY_GAME_MENU_ACTION_PARAM1 = "key_game_menu_action_param1"

		internal const val GAME_MENU_ACTION_SET_SUSPEND = "setSuspend"
		internal const val GAME_MENU_ACTION_NEXT_FRAME = "nextFrame"
		internal const val GAME_MENU_ACTION_SET_NODE_TYPE = "setNodeType"
		internal const val GAME_MENU_ACTION_SET_SELECT_MODE = "setSelectMode"
		internal const val GAME_MENU_ACTION_SET_SELECTION_VISIBLE = "setSelectionVisible"
		internal const val GAME_MENU_ACTION_SET_CAMERA_OVERRIDE = "setCameraOverride"
		internal const val GAME_MENU_ACTION_SET_CAMERA_MANIPULATE_MODE = "setCameraManipulateMode"
		internal const val GAME_MENU_ACTION_RESET_CAMERA_2D_POSITION = "resetCamera2DPosition"
		internal const val GAME_MENU_ACTION_RESET_CAMERA_3D_POSITION = "resetCamera3DPosition"
		internal const val GAME_MENU_ACTION_EMBED_GAME_ON_PLAY = "embedGameOnPlay"
		internal const val GAME_MENU_ACTION_ALWAYS_ON_TOP = "onAlwaysOnTopUpdated"

		private const val GAME_WORKSPACE = "Game"
	}

	internal val editorMessageDispatcher = EditorMessageDispatcher(this)
	private val editorLoadingIndicator: View? by lazy { findViewById(R.id.editor_loading_indicator) }
	private val gameMenuMessageContainer: View? by lazy { findViewById<View?>(R.id.game_menu_message_container)?.apply {
		setOnClickListener {
			it.isVisible = false
		}
	} }

	private val gameMenuState = Bundle()

	override fun getGodotAppLayout() = R.layout.godot_editor_layout

	internal open fun getEditorWindowInfo() = EDITOR_MAIN_INFO

	/**
	 * Set of permissions to be excluded when requesting all permissions at startup.
	 *
	 * The permissions in this set will be requested on demand based on use cases.
	 */
	private fun getExcludedPermissions(): MutableSet<String> {
		val excludedPermissions = mutableSetOf(
			// The RECORD_AUDIO permission is requested when the "audio/driver/enable_input" project
			// setting is enabled.
			Manifest.permission.RECORD_AUDIO,
		)

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
			excludedPermissions.add(
				// The REQUEST_INSTALL_PACKAGES permission is requested the first time we attempt to
				// open an apk file.
				Manifest.permission.REQUEST_INSTALL_PACKAGES,
			)
		}

		// XR runtime permissions should only be requested when the "xr/openxr/enabled" project setting
		// is enabled.
		excludedPermissions.addAll(getXRRuntimePermissions())
		return excludedPermissions
	}

	/**
	 * Set of permissions to request when the "xr/openxr/enabled" project setting is enabled.
	 */
	@CallSuper
	protected open fun getXRRuntimePermissions(): MutableSet<String> {
		return mutableSetOf()
	}

	override fun onCreate(savedInstanceState: Bundle?) {
		installSplashScreen()

		// Prevent the editor window from showing in the display cutout
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P && getEditorWindowInfo() == EDITOR_MAIN_INFO) {
			window.attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER
		}

		// We exclude certain permissions from the set we request at startup, as they'll be
		// requested on demand based on use cases.
		PermissionsUtil.requestManifestPermissions(this, getExcludedPermissions())

		editorMessageDispatcher.parseStartIntent(packageManager, intent)

		if (BuildConfig.BUILD_TYPE == "dev" && WAIT_FOR_DEBUGGER) {
			Debug.waitForDebugger()
		}

		super.onCreate(savedInstanceState)
	}

	override fun onGodotSetupCompleted() {
		super.onGodotSetupCompleted()
		val longPressEnabled = enableLongPressGestures()
		val panScaleEnabled = enablePanAndScaleGestures()

		runOnUiThread {
			// Enable long press, panning and scaling gestures
			godotFragment?.godot?.renderView?.inputHandler?.apply {
				enableLongPress(longPressEnabled)
				enablePanningAndScalingGestures(panScaleEnabled)
			}
		}
	}

	override fun onGodotMainLoopStarted() {
		super.onGodotMainLoopStarted()
		runOnUiThread {
			// Hide the loading indicator
			editorLoadingIndicator?.visibility = View.GONE
		}
	}

	@CallSuper
	override fun updateCommandLineParams(args: Array<String>) {
		val args = if (BuildConfig.BUILD_TYPE == "dev") {
			args + "--benchmark"
		} else {
			args
		}
		super.updateCommandLineParams(args);
	}

	protected fun retrieveEditorWindowInfo(args: Array<String>, gameEmbedMode: GameEmbedMode): EditorWindowInfo {
		var hasEditor = false
		var xrMode = XR_MODE_DEFAULT

		var i = 0
		while (i < args.size) {
			when (args[i++]) {
				EDITOR_ARG, EDITOR_ARG_SHORT, EDITOR_PROJECT_MANAGER_ARG, EDITOR_PROJECT_MANAGER_ARG_SHORT -> hasEditor = true
				XR_MODE_ARG -> {
					xrMode = args[i++]
				}
			}
		}

		return if (hasEditor) {
			EDITOR_MAIN_INFO
		} else {
			// Launching a game
			val openxrEnabled = xrMode == XR_MODE_ON ||
				(xrMode == XR_MODE_DEFAULT && GodotLib.getGlobal("xr/openxr/enabled").toBoolean())
			if (openxrEnabled && isNativeXRDevice(applicationContext)) {
				XR_RUN_GAME_INFO
			} else {
				if (godot?.isProjectManagerHint() == true || isNativeXRDevice(applicationContext)) {
					RUN_GAME_INFO
				} else {
					when (gameEmbedMode) {
						GameEmbedMode.DISABLED -> RUN_GAME_INFO
						GameEmbedMode.ENABLED -> EMBEDDED_RUN_GAME_INFO
						GameEmbedMode.AUTO -> {
							val inMultiWindowMode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
								isInMultiWindowMode
							} else {
								false
							}
							if (inMultiWindowMode || isLargeScreen || isNativeXRDevice(applicationContext)) {
								RUN_GAME_INFO
							} else {
								EMBEDDED_RUN_GAME_INFO
							}
						}
					}
				}
			}
		}
	}

	private fun getEditorWindowInfoForInstanceId(instanceId: Int): EditorWindowInfo? {
		return when (instanceId) {
			RUN_GAME_INFO.windowId -> RUN_GAME_INFO
			EDITOR_MAIN_INFO.windowId -> EDITOR_MAIN_INFO
			XR_RUN_GAME_INFO.windowId -> XR_RUN_GAME_INFO
			EMBEDDED_RUN_GAME_INFO.windowId -> EMBEDDED_RUN_GAME_INFO
			else -> null
		}
	}

	protected fun getNewGodotInstanceIntent(editorWindowInfo: EditorWindowInfo, args: Array<String>): Intent {
		// If we're launching an editor window (project manager or editor) and we're in
		// fullscreen mode, we want to remain in fullscreen mode.
		// This doesn't apply to the play / game window since for that window fullscreen is
		// controlled by the game logic.
		val updatedArgs = if (editorWindowInfo == EDITOR_MAIN_INFO &&
			godot?.isInImmersiveMode() == true &&
			!args.contains(FULLSCREEN_ARG) &&
			!args.contains(FULLSCREEN_ARG_SHORT)
		) {
			args + FULLSCREEN_ARG
		} else {
			args
		}

		val newInstance = Intent()
			.setComponent(ComponentName(this, editorWindowInfo.windowClassName))
			.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
			.putExtra(EXTRA_COMMAND_LINE_PARAMS, updatedArgs)
		if (editorWindowInfo.launchPolicy == LaunchPolicy.ADJACENT) {
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
				Log.v(TAG, "Adding flag for adjacent launch")
				newInstance.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT)
			}
		}
		return newInstance
	}

	final override fun onNewGodotInstanceRequested(args: Array<String>): Int {
		val editorWindowInfo = retrieveEditorWindowInfo(args, fetchGameEmbedMode())

		// Check if this editor window is being terminated. If it's, delay the creation of a new instance until the
		// termination is complete.
		if (editorMessageDispatcher.isPendingForceQuit(editorWindowInfo)) {
			Log.v(TAG, "Scheduling new launch after termination of ${editorWindowInfo.windowId}")
			editorMessageDispatcher.runTaskAfterForceQuit(editorWindowInfo) {
				onNewGodotInstanceRequested(args)
			}
			return editorWindowInfo.windowId
		}

		val sourceView = godotFragment?.view
		val activityOptions = if (sourceView == null) {
			null
		} else {
			val startX = sourceView.width / 2
			val startY = sourceView.height / 2
			ActivityOptions.makeScaleUpAnimation(sourceView, startX, startY, 0, 0)
		}

		val newInstance = getNewGodotInstanceIntent(editorWindowInfo, args)
		newInstance.apply {
			putExtra(EXTRA_EDITOR_HINT, godot?.isEditorHint() == true)
			putExtra(EXTRA_PROJECT_MANAGER_HINT, godot?.isProjectManagerHint() == true)
			putExtra(EXTRA_GAME_MENU_STATE, gameMenuState)
		}

		if (editorWindowInfo.windowClassName == javaClass.name) {
			Log.d(TAG, "Restarting ${editorWindowInfo.windowClassName} with parameters ${args.contentToString()}")
			triggerRebirth(activityOptions?.toBundle(), newInstance)
		} else {
			Log.d(TAG, "Starting ${editorWindowInfo.windowClassName} with parameters ${args.contentToString()}")
			newInstance.putExtra(EXTRA_NEW_LAUNCH, true)
				.putExtra(EditorMessageDispatcher.EXTRA_MSG_DISPATCHER_PAYLOAD, editorMessageDispatcher.getMessageDispatcherPayload())
			startActivity(newInstance, activityOptions?.toBundle())
		}
		return editorWindowInfo.windowId
	}

	final override fun onGodotForceQuit(godotInstanceId: Int): Boolean {
		val editorWindowInfo = getEditorWindowInfoForInstanceId(godotInstanceId) ?: return super.onGodotForceQuit(godotInstanceId)

		if (editorWindowInfo.windowClassName == javaClass.name) {
			Log.d(TAG, "Force quitting ${editorWindowInfo.windowClassName}")
			ProcessPhoenix.forceQuit(this)
			return true
		}

		// Send an inter-process message to request the target editor window to force quit.
		if (editorMessageDispatcher.requestForceQuit(editorWindowInfo)) {
			return true
		}

		// Fallback to killing the target process.
		val processName = packageName + editorWindowInfo.processNameSuffix
		val activityManager = getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
		val runningProcesses = activityManager.runningAppProcesses
		for (runningProcess in runningProcesses) {
			if (runningProcess.processName == processName) {
				// Killing process directly
				Log.v(TAG, "Killing Godot process ${runningProcess.processName}")
				Process.killProcess(runningProcess.pid)
				return true
			}
		}

		return super.onGodotForceQuit(godotInstanceId)
	}

	// Get the screen's density scale
	private val isLargeScreen: Boolean
		// Get the minimum window size // Correspond to the EXPANDED window size class.
		get() {
			val metrics = WindowMetricsCalculator.getOrCreate().computeMaximumWindowMetrics(this)

			// Get the screen's density scale
			val scale = resources.displayMetrics.density

			// Get the minimum window size
			val minSize = min(metrics.bounds.width(), metrics.bounds.height()).toFloat()
			val minSizeDp = minSize / scale
			return minSizeDp >= 840f // Correspond to the EXPANDED window size class.
		}

	override fun setRequestedOrientation(requestedOrientation: Int) {
		if (!overrideOrientationRequest()) {
			super.setRequestedOrientation(requestedOrientation)
		}
	}

	/**
	 * The Godot Android Editor sets its own orientation via its AndroidManifest
	 */
	protected open fun overrideOrientationRequest() = true

	/**
	 * Enable long press gestures for the Godot Android editor.
	 */
	protected open fun enableLongPressGestures() =
		java.lang.Boolean.parseBoolean(GodotLib.getEditorSetting("interface/touchscreen/enable_long_press_as_right_click"))

	/**
	 * Enable pan and scale gestures for the Godot Android editor.
	 */
	protected open fun enablePanAndScaleGestures() =
		java.lang.Boolean.parseBoolean(GodotLib.getEditorSetting("interface/touchscreen/enable_pan_and_scale_gestures"))

	override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
		super.onActivityResult(requestCode, resultCode, data)
		// Check if we got the MANAGE_EXTERNAL_STORAGE permission
		when (requestCode) {
			PermissionsUtil.REQUEST_MANAGE_EXTERNAL_STORAGE_REQ_CODE -> {
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && !Environment.isExternalStorageManager()) {
					Toast.makeText(
						this,
						R.string.denied_storage_permission_error_msg,
						Toast.LENGTH_LONG
					).show()
				}
			}

			PermissionsUtil.REQUEST_INSTALL_PACKAGES_REQ_CODE -> {
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && !packageManager.canRequestPackageInstalls()) {
					Toast.makeText(
						this,
						R.string.denied_install_packages_permission_error_msg,
						Toast.LENGTH_LONG
					).show()
				}
			}
		}
	}

	override fun onRequestPermissionsResult(
		requestCode: Int,
		permissions: Array<String>,
		grantResults: IntArray
	) {
		super.onRequestPermissionsResult(requestCode, permissions, grantResults)
		// Check if we got access to the necessary storage permissions
		if (requestCode == PermissionsUtil.REQUEST_ALL_PERMISSION_REQ_CODE) {
			if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
				var hasReadAccess = false
				var hasWriteAccess = false
				for (i in permissions.indices) {
					if (Manifest.permission.READ_EXTERNAL_STORAGE == permissions[i] && grantResults[i] == PackageManager.PERMISSION_GRANTED) {
						hasReadAccess = true
					}
					if (Manifest.permission.WRITE_EXTERNAL_STORAGE == permissions[i] && grantResults[i] == PackageManager.PERMISSION_GRANTED) {
						hasWriteAccess = true
					}
				}
				if (!hasReadAccess || !hasWriteAccess) {
					Toast.makeText(
						this,
						R.string.denied_storage_permission_error_msg,
						Toast.LENGTH_LONG
					).show()
				}
			}
		}
	}

	override fun signApk(
		inputPath: String,
		outputPath: String,
		keystorePath: String,
		keystoreUser: String,
		keystorePassword: String
	): Error {
		val godot = godot ?: return Error.ERR_UNCONFIGURED
		return signApk(godot.fileAccessHandler, inputPath, outputPath, keystorePath, keystoreUser, keystorePassword)
	}

	override fun verifyApk(apkPath: String): Error {
		val godot = godot ?: return Error.ERR_UNCONFIGURED
		return verifyApk(godot.fileAccessHandler, apkPath)
	}

	override fun supportsFeature(featureTag: String): Boolean {
		if (featureTag == "xr_editor") {
			return isNativeXRDevice(applicationContext)
		}

		if (featureTag == "horizonos") {
			return BuildConfig.FLAVOR == "horizonos"
		}

		if (featureTag == "picoos") {
			return BuildConfig.FLAVOR == "picoos"
		}

        return false
    }

	override fun onEditorWorkspaceSelected(workspace: String) {
		if (workspace == GAME_WORKSPACE) {
			val dispatchResumeRequest = editorMessageDispatcher.bringEditorWindowToFront(RUN_GAME_INFO) ||
				editorMessageDispatcher.bringEditorWindowToFront(EMBEDDED_RUN_GAME_INFO) ||
				editorMessageDispatcher.bringEditorWindowToFront(XR_RUN_GAME_INFO)
			if (!dispatchResumeRequest) {
				runOnUiThread {
					// Show a help view
					gameMenuMessageContainer?.isVisible = true
				}
			}
		}
	}

	internal open fun bringSelfToFront() {
		runOnUiThread {
			Log.v(TAG, "Bringing self to front")
			val relaunchIntent = Intent(intent)
			// Don't restart
			relaunchIntent.putExtra(EXTRA_NEW_LAUNCH, false)
			startActivity(relaunchIntent)
		}
	}

	internal fun parseGameMenuAction(actionData: Bundle) {
		val action = actionData.getString(KEY_GAME_MENU_ACTION) ?: return
		when (action) {
			GAME_MENU_ACTION_SET_SUSPEND -> {
				val suspended = actionData.getBoolean(KEY_GAME_MENU_ACTION_PARAM1)
				suspendGame(suspended)
				gameMenuState.putBoolean(action, suspended)
			}
			GAME_MENU_ACTION_NEXT_FRAME -> {
				dispatchNextFrame()
			}
			GAME_MENU_ACTION_SET_NODE_TYPE -> {
				val nodeType = actionData.getInt(KEY_GAME_MENU_ACTION_PARAM1)
				selectRuntimeNode(nodeType)
				gameMenuState.putInt(action, nodeType)
			}
			GAME_MENU_ACTION_SET_SELECTION_VISIBLE -> {
				val enabled = actionData.getBoolean(KEY_GAME_MENU_ACTION_PARAM1)
				toggleSelectionVisibility(enabled)
				gameMenuState.putBoolean(action, enabled)
			}
			GAME_MENU_ACTION_SET_CAMERA_OVERRIDE -> {
				val enabled = actionData.getBoolean(KEY_GAME_MENU_ACTION_PARAM1)
				overrideCamera(enabled)
				gameMenuState.putBoolean(action, enabled)
			}
			GAME_MENU_ACTION_SET_SELECT_MODE -> {
				val selectMode = actionData.getInt(KEY_GAME_MENU_ACTION_PARAM1)
				selectRuntimeNodeSelectMode(selectMode)
				gameMenuState.putInt(action, selectMode)
			}
			GAME_MENU_ACTION_RESET_CAMERA_2D_POSITION -> {
				reset2DCamera()
			}
			GAME_MENU_ACTION_RESET_CAMERA_3D_POSITION -> {
				reset3DCamera()
			}
			GAME_MENU_ACTION_SET_CAMERA_MANIPULATE_MODE -> {
				val mode = actionData.getInt(KEY_GAME_MENU_ACTION_PARAM1)
				manipulateCamera(mode)
				gameMenuState.putInt(action, mode)
			}
			GAME_MENU_ACTION_EMBED_GAME_ON_PLAY -> {
				val embedded = actionData.getBoolean(KEY_GAME_MENU_ACTION_PARAM1)
				embedGameOnPlay(embedded)
				gameMenuState.putBoolean(action, embedded)
			}
			GAME_MENU_ACTION_ALWAYS_ON_TOP -> {
				gameMenuState.putBoolean(action, actionData.getBoolean(KEY_GAME_MENU_ACTION_PARAM1))
			}
		}
	}

	private fun selectRuntimeNode(nodeType: Int) {
		godot?.runOnRenderThread {
			GameMenuUtils.setNodeType(nodeType)
		}
	}

	private fun selectRuntimeNodeSelectMode(selectMode: Int) {
		godot?.runOnRenderThread {
			GameMenuUtils.setSelectMode(selectMode)
		}
	}

	private fun manipulateCamera(mode: Int) {
		godot?.runOnRenderThread {
			GameMenuUtils.setCameraManipulateMode(mode)
		}
	}

	private fun suspendGame(suspended: Boolean) {
		godot?.runOnRenderThread {
			GameMenuUtils.setSuspend(suspended)
		}
	}

	private fun dispatchNextFrame() {
		godot?.runOnRenderThread {
			GameMenuUtils.nextFrame()
		}
	}

	private fun toggleSelectionVisibility(enabled: Boolean) {
		godot?.runOnRenderThread {
			GameMenuUtils.setSelectionVisible(enabled)
		}
	}

	private fun overrideCamera(enabled: Boolean) {
		godot?.runOnRenderThread {
			GameMenuUtils.setCameraOverride(enabled)
		}
	}

	private fun selectRuntimeNode(nodeType: GameMenuFragment.GameMenuListener.RuntimeNodeType) {
		selectRuntimeNode(nodeType.ordinal)
	}

	private fun selectRuntimeNodeSelectMode(selectMode: GameMenuFragment.GameMenuListener.RuntimeNodeSelectMode) {
		selectRuntimeNodeSelectMode(selectMode.ordinal)
	}

	private fun reset2DCamera() {
		godot?.runOnRenderThread {
			GameMenuUtils.resetCamera2DPosition()
		}
	}

	private fun reset3DCamera() {
		godot?.runOnRenderThread {
			GameMenuUtils.resetCamera3DPosition()
		}
	}

	private fun manipulateCamera(mode: GameMenuFragment.GameMenuListener.CameraMode) {
		manipulateCamera(mode.ordinal)
	}

	private fun embedGameOnPlay(embedded: Boolean) {
		godot?.runOnRenderThread {
			val gameEmbedMode = if (embedded) GameEmbedMode.ENABLED else GameEmbedMode.DISABLED
			GameMenuUtils.saveGameEmbedMode(gameEmbedMode)
		}
	}
}
