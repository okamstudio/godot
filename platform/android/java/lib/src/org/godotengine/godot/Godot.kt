/**************************************************************************/
/*  Godot.kt                                                              */
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

package org.godotengine.godot

import android.annotation.SuppressLint
import android.app.Activity
import android.app.AlertDialog
import android.content.*
import android.content.pm.PackageManager
import android.content.res.Configuration
import android.content.res.Resources
import android.graphics.Color
import android.hardware.Sensor
import android.hardware.SensorManager
import android.os.*
import android.util.Log
import android.util.TypedValue
import android.view.*
import android.widget.EditText
import android.widget.FrameLayout
import androidx.annotation.Keep
import androidx.annotation.StringRes
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsAnimationCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.google.android.vending.expansion.downloader.*
import org.godotengine.godot.error.Error
import org.godotengine.godot.input.GodotEditText
import org.godotengine.godot.input.GodotInputHandler
import org.godotengine.godot.io.FilePicker
import org.godotengine.godot.io.directory.DirectoryAccessHandler
import org.godotengine.godot.io.file.FileAccessHandler
import org.godotengine.godot.plugin.AndroidRuntimePlugin
import org.godotengine.godot.plugin.GodotPlugin
import org.godotengine.godot.plugin.GodotPluginRegistry
import org.godotengine.godot.tts.GodotTTS
import org.godotengine.godot.utils.CommandLineFileParser
import org.godotengine.godot.utils.GodotNetUtils
import org.godotengine.godot.utils.PermissionsUtil
import org.godotengine.godot.utils.PermissionsUtil.requestPermission
import org.godotengine.godot.utils.beginBenchmarkMeasure
import org.godotengine.godot.utils.benchmarkFile
import org.godotengine.godot.utils.dumpBenchmark
import org.godotengine.godot.utils.endBenchmarkMeasure
import org.godotengine.godot.utils.useBenchmark
import org.godotengine.godot.xr.XRMode
import java.io.File
import java.io.FileInputStream
import java.io.InputStream
import java.lang.Exception
import java.security.MessageDigest
import java.util.*
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference


/**
 * Core component used to interface with the native layer of the engine.
 *
 * Can be hosted by [Activity], [Fragment] or [Service] android components, so long as its
 * lifecycle methods are properly invoked.
 */
class Godot(private val context: Context) {

	internal companion object {
		private val TAG = Godot::class.java.simpleName

		// Supported build flavors
		const val EDITOR_FLAVOR = "editor"
		const val TEMPLATE_FLAVOR = "template"

		/**
		 * @return true if this is an editor build, false if this is a template build
		 */
		fun isEditorBuild() = BuildConfig.FLAVOR == EDITOR_FLAVOR
	}

	private val mSensorManager: SensorManager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
	private val mClipboard: ClipboardManager = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
	private val vibratorService: Vibrator = context.getSystemService(Context.VIBRATOR_SERVICE) as Vibrator

	private val pluginRegistry: GodotPluginRegistry by lazy {
		GodotPluginRegistry.getPluginRegistry()
	}

	private val accelerometerEnabled = AtomicBoolean(false)
	private val mAccelerometer: Sensor? by lazy {
		mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
	}

	private val gravityEnabled = AtomicBoolean(false)
	private val mGravity: Sensor? by lazy {
		mSensorManager.getDefaultSensor(Sensor.TYPE_GRAVITY)
	}

	private val magnetometerEnabled = AtomicBoolean(false)
	private val mMagnetometer: Sensor? by lazy {
		mSensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD)
	}

	private val gyroscopeEnabled = AtomicBoolean(false)
	private val mGyroscope: Sensor? by lazy {
		mSensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
	}

	val tts = GodotTTS(context)
	val directoryAccessHandler = DirectoryAccessHandler(context)
	val fileAccessHandler = FileAccessHandler(context)
	val netUtils = GodotNetUtils(context)
	private val commandLineFileParser = CommandLineFileParser()
	private val godotInputHandler = GodotInputHandler(context, this)

	/**
	 * Task to run when the engine terminates.
	 */
	private val runOnTerminate = AtomicReference<Runnable>()

	/**
	 * Tracks whether [onCreate] was completed successfully.
	 */
	private var initializationStarted = false

	/**
	 * Tracks whether [GodotLib.initialize] was completed successfully.
	 */
	private var nativeLayerInitializeCompleted = false

	/**
	 * Tracks whether [GodotLib.setup] was completed successfully.
	 */
	private var nativeLayerSetupCompleted = false

	/**
	 * Tracks whether [onInitRenderView] was completed successfully.
	 */
	private var renderViewInitialized = false
	private var primaryHost: GodotHost? = null

	/**
	 * Tracks whether we're in the RESUMED lifecycle state.
	 * See [onResume] and [onPause]
	 */
	private var resumed = false

	/**
	 * Tracks whether [onGodotSetupCompleted] fired.
	 */
	private val godotMainLoopStarted = AtomicBoolean(false)

	var io: GodotIO? = null

	private var commandLine : MutableList<String> = ArrayList<String>()
	private var xrMode = XRMode.REGULAR
	private var expansionPackPath: String = ""
	private var useApkExpansion = false
	private val useImmersive = AtomicBoolean(false)
	private var useDebugOpengl = false
	private var darkMode = false

	private var containerLayout: FrameLayout? = null
	var renderView: GodotRenderView? = null

	/**
	 * Returns true if the native engine has been initialized through [onInitNativeLayer], false otherwise.
	 */
	private fun isNativeInitialized() = nativeLayerInitializeCompleted && nativeLayerSetupCompleted

	/**
	 * Returns true if the engine has been initialized, false otherwise.
	 */
	fun isInitialized() = initializationStarted && isNativeInitialized() && renderViewInitialized

	/**
	 * Provides access to the primary host [Activity]
	 */
	fun getActivity() = primaryHost?.activity
	private fun requireActivity() = getActivity() ?: throw IllegalStateException("Host activity must be non-null")

	/**
	 * Start initialization of the Godot engine.
	 *
	 * This must be followed by [onInitNativeLayer] and [onInitRenderView] in that order to complete
	 * initialization of the engine.
	 *
	 * @throws IllegalArgumentException exception if the specified expansion pack (if any)
	 * is invalid.
	 */
	fun onCreate(primaryHost: GodotHost) {
		if (this.primaryHost != null || initializationStarted) {
			Log.d(TAG, "OnCreate already invoked")
			return
		}

		Log.v(TAG, "OnCreate: $primaryHost")

		darkMode = context.resources?.configuration?.uiMode?.and(Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES

		beginBenchmarkMeasure("Startup", "Godot::onCreate")
		try {
			this.primaryHost = primaryHost
			val activity = requireActivity()
			val window = activity.window
			window.addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON)

			Log.v(TAG, "Initializing Godot plugin registry")
			val runtimePlugins = mutableSetOf<GodotPlugin>(AndroidRuntimePlugin(this))
			runtimePlugins.addAll(primaryHost.getHostPlugins(this))
			GodotPluginRegistry.initializePluginRegistry(this, runtimePlugins)
			if (io == null) {
				io = GodotIO(activity)
			}

			// check for apk expansion API
			commandLine = getCommandLine()
			var mainPackMd5: String? = null
			var mainPackKey: String? = null
			val newArgs: MutableList<String> = ArrayList()
			var i = 0
			while (i < commandLine.size) {
				val hasExtra: Boolean = i < commandLine.size - 1
				if (commandLine[i] == XRMode.REGULAR.cmdLineArg) {
					xrMode = XRMode.REGULAR
				} else if (commandLine[i] == XRMode.OPENXR.cmdLineArg) {
					xrMode = XRMode.OPENXR
				} else if (commandLine[i] == "--debug_opengl") {
					useDebugOpengl = true
				} else if (commandLine[i] == "--fullscreen") {
					useImmersive.set(true)
					newArgs.add(commandLine[i])
				} else if (commandLine[i] == "--use_apk_expansion") {
					useApkExpansion = true
				} else if (hasExtra && commandLine[i] == "--apk_expansion_md5") {
					mainPackMd5 = commandLine[i + 1]
					i++
				} else if (hasExtra && commandLine[i] == "--apk_expansion_key") {
					mainPackKey = commandLine[i + 1]
					val prefs = activity.getSharedPreferences(
							"app_data_keys",
							Context.MODE_PRIVATE
					)
					val editor = prefs.edit()
					editor.putString("store_public_key", mainPackKey)
					editor.apply()
					i++
				} else if (commandLine[i] == "--benchmark") {
					useBenchmark = true
					newArgs.add(commandLine[i])
				} else if (hasExtra && commandLine[i] == "--benchmark-file") {
					useBenchmark = true
					newArgs.add(commandLine[i])

					// Retrieve the filepath
					benchmarkFile = commandLine[i + 1]
					newArgs.add(commandLine[i + 1])

					i++
				} else if (commandLine[i].trim().isNotEmpty()) {
					newArgs.add(commandLine[i])
				}
				i++
			}
			commandLine = if (newArgs.isEmpty()) { mutableListOf() } else { newArgs }
			if (useApkExpansion && mainPackMd5 != null && mainPackKey != null) {
				// Build the full path to the app's expansion files
				try {
					expansionPackPath = Helpers.getSaveFilePath(context)
					expansionPackPath += "/main." + activity.packageManager.getPackageInfo(
							activity.packageName,
							0
					).versionCode + "." + activity.packageName + ".obb"
				} catch (e: java.lang.Exception) {
					Log.e(TAG, "Unable to build full path to the app's expansion files", e)
				}
				val f = File(expansionPackPath)
				var packValid = true
				if (!f.exists()) {
					packValid = false
				} else if (obbIsCorrupted(expansionPackPath, mainPackMd5)) {
					packValid = false
					try {
						f.delete()
					} catch (_: java.lang.Exception) {
					}
				}
				if (!packValid) {
					// Aborting engine initialization
					throw IllegalArgumentException("Invalid expansion pack")
				}
			}

			initializationStarted = true
		} catch (e: java.lang.Exception) {
			// Clear the primary host and rethrow
			this.primaryHost = null
			initializationStarted = false
			throw e
		} finally {
			endBenchmarkMeasure("Startup", "Godot::onCreate")
		}
	}

	/**
	 * Toggle immersive mode.
	 * Must be called from the UI thread.
	 */
	private fun enableImmersiveMode(enabled: Boolean, override: Boolean = false) {
		val activity = getActivity() ?: return
		val window = activity.window ?: return

		if (!useImmersive.compareAndSet(!enabled, enabled) && !override) {
			return
		}

		WindowCompat.setDecorFitsSystemWindows(window, !enabled)
		val controller = WindowInsetsControllerCompat(window, window.decorView)
		if (enabled) {
			controller.hide(WindowInsetsCompat.Type.systemBars())
			controller.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
		} else {
			val fullScreenThemeValue = TypedValue()
			val hasStatusBar = if (activity.theme.resolveAttribute(android.R.attr.windowFullscreen, fullScreenThemeValue, true) && fullScreenThemeValue.type == TypedValue.TYPE_INT_BOOLEAN) {
				fullScreenThemeValue.data == 0
			} else {
				// Fallback to checking the editor build
				!isEditorBuild()
			}

			val types = if (hasStatusBar) {
				WindowInsetsCompat.Type.navigationBars() or WindowInsetsCompat.Type.statusBars()
			} else {
				WindowInsetsCompat.Type.navigationBars()
			}
			controller.show(types)
		}
	}

	/**
	 * Invoked from the render thread to toggle the immersive mode.
	 */
	@Keep
	private fun nativeEnableImmersiveMode(enabled: Boolean) {
		runOnUiThread {
			enableImmersiveMode(enabled)
		}
	}

	@Keep
	fun isInImmersiveMode() = useImmersive.get()

	/**
	 * Initializes the native layer of the Godot engine.
	 *
	 * This must be preceded by [onCreate] and followed by [onInitRenderView] to complete
	 * initialization of the engine.
	 *
	 * @return false if initialization of the native layer fails, true otherwise.
	 *
	 * @throws IllegalStateException if [onCreate] has not been called.
	 */
	fun onInitNativeLayer(host: GodotHost): Boolean {
		if (!initializationStarted) {
			throw IllegalStateException("OnCreate must be invoked successfully prior to initializing the native layer")
		}
		if (isNativeInitialized()) {
			Log.d(TAG, "OnInitNativeLayer already invoked")
			return true
		}
		if (host != primaryHost) {
			Log.e(TAG, "Native initialization is only supported for the primary host")
			return false
		}

		Log.v(TAG, "OnInitNativeLayer: $host")

		beginBenchmarkMeasure("Startup", "Godot::onInitNativeLayer")
		try {
			if (expansionPackPath.isNotEmpty()) {
				commandLine.add("--main-pack")
				commandLine.add(expansionPackPath)
			}
			val activity = requireActivity()
			if (!nativeLayerInitializeCompleted) {
				nativeLayerInitializeCompleted = GodotLib.initialize(
					activity,
					this,
					activity.assets,
					io,
					netUtils,
					directoryAccessHandler,
					fileAccessHandler,
					useApkExpansion,
				)
				Log.v(TAG, "Godot native layer initialization completed: $nativeLayerInitializeCompleted")
			}

			if (nativeLayerInitializeCompleted && !nativeLayerSetupCompleted) {
				nativeLayerSetupCompleted = GodotLib.setup(commandLine.toTypedArray(), tts)
				if (!nativeLayerSetupCompleted) {
					throw IllegalStateException("Unable to setup the Godot engine! Aborting...")
				} else {
					Log.v(TAG, "Godot native layer setup completed")
				}
			}
		} finally {
			endBenchmarkMeasure("Startup", "Godot::onInitNativeLayer")
		}
		return isNativeInitialized()
	}

	/**
	 * Used to complete initialization of the view used by the engine for rendering.
	 *
	 * This must be preceded by [onCreate] and [onInitNativeLayer] in that order to properly
	 * initialize the engine.
	 *
	 * @param host The [GodotHost] that's initializing the render views
	 * @param providedContainerLayout Optional argument; if provided, this is reused to host the Godot's render views
	 *
	 * @return A [FrameLayout] instance containing Godot's render views if initialization is successful, null otherwise.
	 *
	 * @throws IllegalStateException if [onInitNativeLayer] has not been called
	 */
	@JvmOverloads
	fun onInitRenderView(host: GodotHost, providedContainerLayout: FrameLayout = FrameLayout(host.activity)): FrameLayout? {
		if (!isNativeInitialized()) {
			throw IllegalStateException("onInitNativeLayer() must be invoked successfully prior to initializing the render view")
		}

		Log.v(TAG, "OnInitRenderView: $host")

		beginBenchmarkMeasure("Startup", "Godot::onInitRenderView")
		try {
			val activity: Activity = host.activity
			containerLayout = providedContainerLayout
			containerLayout?.removeAllViews()
			containerLayout?.layoutParams = ViewGroup.LayoutParams(
					ViewGroup.LayoutParams.MATCH_PARENT,
					ViewGroup.LayoutParams.MATCH_PARENT
			)

			// GodotEditText layout
			val editText = GodotEditText(activity)
			editText.layoutParams =
					ViewGroup.LayoutParams(
							ViewGroup.LayoutParams.MATCH_PARENT,
							activity.resources.getDimension(R.dimen.text_edit_height).toInt()
					)
			// Prevent GodotEditText from showing on splash screen on devices with Android 14 or newer.
			editText.setBackgroundColor(Color.TRANSPARENT)
			// ...add to FrameLayout
			containerLayout?.addView(editText)
			renderView = if (usesVulkan()) {
				if (meetsVulkanRequirements(activity.packageManager)) {
					GodotVulkanRenderView(host, this, godotInputHandler)
				} else if (canFallbackToOpenGL()) {
					// Fallback to OpenGl.
					GodotGLRenderView(host, this, godotInputHandler, xrMode, useDebugOpengl)
				} else {
					throw IllegalStateException(activity.getString(R.string.error_missing_vulkan_requirements_message))
				}

			} else {
				// Fallback to OpenGl.
				GodotGLRenderView(host, this, godotInputHandler, xrMode, useDebugOpengl)
			}

			if (host == primaryHost) {
				renderView?.startRenderer()
			}

			renderView?.let {
				containerLayout?.addView(
					it.view,
					ViewGroup.LayoutParams(
							ViewGroup.LayoutParams.MATCH_PARENT,
							ViewGroup.LayoutParams.MATCH_PARENT
					)
				)
			}

			editText.setView(renderView)
			io?.setEdit(editText)

			// Listeners for keyboard height.
			val decorView = activity.window.decorView
			// Report the height of virtual keyboard as it changes during the animation.
			ViewCompat.setWindowInsetsAnimationCallback(decorView, object : WindowInsetsAnimationCompat.Callback(DISPATCH_MODE_STOP) {
				var startBottom = 0
				var endBottom = 0
				override fun onPrepare(animation: WindowInsetsAnimationCompat) {
					startBottom = ViewCompat.getRootWindowInsets(decorView)?.getInsets(WindowInsetsCompat.Type.ime())?.bottom ?: 0
				}

				override fun onStart(animation: WindowInsetsAnimationCompat, bounds: WindowInsetsAnimationCompat.BoundsCompat): WindowInsetsAnimationCompat.BoundsCompat {
					endBottom = ViewCompat.getRootWindowInsets(decorView)?.getInsets(WindowInsetsCompat.Type.ime())?.bottom ?: 0
					return bounds
				}

				override fun onProgress(windowInsets: WindowInsetsCompat, animationsList: List<WindowInsetsAnimationCompat>): WindowInsetsCompat {
					// Find the IME animation.
					var imeAnimation: WindowInsetsAnimationCompat? = null
					for (animation in animationsList) {
						if (animation.typeMask and WindowInsetsCompat.Type.ime() != 0) {
							imeAnimation = animation
							break
						}
					}

					// Update keyboard height based on IME animation.
					if (imeAnimation != null) {
						val interpolatedFraction = imeAnimation.interpolatedFraction
						// Linear interpolation between start and end values.
						val keyboardHeight = startBottom * (1.0f - interpolatedFraction) + endBottom * interpolatedFraction
						GodotLib.setVirtualKeyboardHeight(keyboardHeight.toInt())
					}
					return windowInsets
				}

				override fun onEnd(animation: WindowInsetsAnimationCompat) {}
			})

			if (host == primaryHost) {
				renderView?.queueOnRenderThread {
					for (plugin in pluginRegistry.allPlugins) {
						plugin.onRegisterPluginWithGodotNative()
					}
					setKeepScreenOn(java.lang.Boolean.parseBoolean(GodotLib.getGlobal("display/window/energy_saving/keep_screen_on")))
				}

				// Include the returned non-null views in the Godot view hierarchy.
				for (plugin in pluginRegistry.allPlugins) {
					val pluginView = plugin.onMainCreate(activity)
					if (pluginView != null) {
						if (plugin.shouldBeOnTop()) {
							containerLayout?.addView(pluginView)
						} else {
							containerLayout?.addView(pluginView, 0)
						}
					}
				}
			}
			renderViewInitialized = true
		} finally {
			if (!renderViewInitialized) {
				containerLayout?.removeAllViews()
				containerLayout = null
			}

			endBenchmarkMeasure("Startup", "Godot::onInitRenderView")
		}
		return containerLayout
	}

	fun onStart(host: GodotHost) {
		Log.v(TAG, "OnStart: $host")
		if (host != primaryHost) {
			return
		}

		renderView?.onActivityStarted()
	}

	fun onResume(host: GodotHost) {
		Log.v(TAG, "OnResume: $host")
		resumed = true
		if (host != primaryHost) {
			return
		}

		renderView?.onActivityResumed()
		registerSensorsIfNeeded()
		enableImmersiveMode(useImmersive.get(), true)
		for (plugin in pluginRegistry.allPlugins) {
			plugin.onMainResume()
		}
	}

	private fun registerSensorsIfNeeded() {
		if (!resumed || !godotMainLoopStarted.get()) {
			return
		}

		if (accelerometerEnabled.get() && mAccelerometer != null) {
			mSensorManager.registerListener(godotInputHandler, mAccelerometer, SensorManager.SENSOR_DELAY_GAME)
		}
		if (gravityEnabled.get() && mGravity != null) {
			mSensorManager.registerListener(godotInputHandler, mGravity, SensorManager.SENSOR_DELAY_GAME)
		}
		if (magnetometerEnabled.get() && mMagnetometer != null) {
			mSensorManager.registerListener(godotInputHandler, mMagnetometer, SensorManager.SENSOR_DELAY_GAME)
		}
		if (gyroscopeEnabled.get() && mGyroscope != null) {
			mSensorManager.registerListener(godotInputHandler, mGyroscope, SensorManager.SENSOR_DELAY_GAME)
		}
	}

	fun onPause(host: GodotHost) {
		Log.v(TAG, "OnPause: $host")
		resumed = false
		if (host != primaryHost) {
			return
		}

		renderView?.onActivityPaused()
		mSensorManager.unregisterListener(godotInputHandler)
		for (plugin in pluginRegistry.allPlugins) {
			plugin.onMainPause()
		}
	}

	fun onStop(host: GodotHost) {
		Log.v(TAG, "OnStop: $host")
		if (host != primaryHost) {
			return
		}

		renderView?.onActivityStopped()
	}

	fun onDestroy(primaryHost: GodotHost) {
		Log.v(TAG, "OnDestroy: $primaryHost")
		if (this.primaryHost != primaryHost) {
			return
		}

		for (plugin in pluginRegistry.allPlugins) {
			plugin.onMainDestroy()
		}

		renderView?.onActivityDestroyed()
	}

	/**
	 * Configuration change callback
	*/
	fun onConfigurationChanged(newConfig: Configuration) {
		val newDarkMode = newConfig.uiMode.and(Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES
		if (darkMode != newDarkMode) {
			darkMode = newDarkMode
			GodotLib.onNightModeChanged()
		}
	}

	/**
	 * Activity result callback
	 */
	fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
		for (plugin in pluginRegistry.allPlugins) {
			plugin.onMainActivityResult(requestCode, resultCode, data)
		}
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
			FilePicker.handleActivityResult(context, requestCode, resultCode, data)
		}
	}

	/**
	 * Permissions request callback
	 */
	fun onRequestPermissionsResult(
		requestCode: Int,
		permissions: Array<String?>,
		grantResults: IntArray
	) {
		for (plugin in pluginRegistry.allPlugins) {
			plugin.onMainRequestPermissionsResult(requestCode, permissions, grantResults)
		}
		for (i in permissions.indices) {
			GodotLib.requestPermissionResult(
				permissions[i],
				grantResults[i] == PackageManager.PERMISSION_GRANTED
			)
		}
	}

	/**
	 * Invoked on the render thread when the Godot setup is complete.
	 */
	private fun onGodotSetupCompleted() {
		Log.v(TAG, "OnGodotSetupCompleted")

		// These properties are defined after Godot setup completion, so we retrieve them here.
		val longPressEnabled = java.lang.Boolean.parseBoolean(GodotLib.getGlobal("input_devices/pointing/android/enable_long_press_as_right_click"))
		val panScaleEnabled = java.lang.Boolean.parseBoolean(GodotLib.getGlobal("input_devices/pointing/android/enable_pan_and_scale_gestures"))
		val rotaryInputAxisValue = GodotLib.getGlobal("input_devices/pointing/android/rotary_input_scroll_axis")

		runOnUiThread {
			renderView?.inputHandler?.apply {
				enableLongPress(longPressEnabled)
				enablePanningAndScalingGestures(panScaleEnabled)
				try {
					setRotaryInputAxis(Integer.parseInt(rotaryInputAxisValue))
				} catch (e: NumberFormatException) {
					Log.w(TAG, e)
				}
			}
		}

		for (plugin in pluginRegistry.allPlugins) {
			plugin.onGodotSetupCompleted()
		}
		primaryHost?.onGodotSetupCompleted()
	}

	/**
	 * Invoked on the render thread when the Godot main loop has started.
	 */
	private fun onGodotMainLoopStarted() {
		Log.v(TAG, "OnGodotMainLoopStarted")
		godotMainLoopStarted.set(true)

		accelerometerEnabled.set(java.lang.Boolean.parseBoolean(GodotLib.getGlobal("input_devices/sensors/enable_accelerometer")))
		gravityEnabled.set(java.lang.Boolean.parseBoolean(GodotLib.getGlobal("input_devices/sensors/enable_gravity")))
		gyroscopeEnabled.set(java.lang.Boolean.parseBoolean(GodotLib.getGlobal("input_devices/sensors/enable_gyroscope")))
		magnetometerEnabled.set(java.lang.Boolean.parseBoolean(GodotLib.getGlobal("input_devices/sensors/enable_magnetometer")))

		runOnUiThread {
			registerSensorsIfNeeded()
			enableImmersiveMode(useImmersive.get(), true)
		}

		for (plugin in pluginRegistry.allPlugins) {
			plugin.onGodotMainLoopStarted()
		}
		primaryHost?.onGodotMainLoopStarted()
	}

	/**
	 * Invoked on the render thread when the engine is about to terminate.
	 */
	@Keep
	private fun onGodotTerminating() {
		Log.v(TAG, "OnGodotTerminating")
		runOnTerminate.get()?.run()
	}

	private fun restart() {
		primaryHost?.onGodotRestartRequested(this)
	}

	fun alert(
		@StringRes messageResId: Int,
		@StringRes titleResId: Int,
		okCallback: Runnable?
	) {
		val res: Resources = getActivity()?.resources ?: return
		alert(res.getString(messageResId), res.getString(titleResId), okCallback)
	}

	@JvmOverloads
	@Keep
	fun alert(message: String, title: String, okCallback: Runnable? = null) {
		val activity: Activity = getActivity() ?: return
		runOnUiThread {
			val builder = AlertDialog.Builder(activity)
			builder.setMessage(message).setTitle(title)
			builder.setPositiveButton(
				R.string.dialog_ok
			) { dialog: DialogInterface, id: Int ->
				okCallback?.run()
				dialog.cancel()
			}
			val dialog = builder.create()
			dialog.show()
		}
	}

	/**
	 * Queue a runnable to be run on the render thread.
	 *
	 * This must be called after the render thread has started.
	 */
	fun runOnRenderThread(action: Runnable) {
		renderView?.queueOnRenderThread(action)
	}

	/**
	 * Runs the specified action on the UI thread.
	 * If the current thread is the UI thread, then the action is executed immediately.
	 * If the current thread is not the UI thread, the action is posted to the event queue
	 * of the UI thread.
	 */
	fun runOnUiThread(action: Runnable) {
		val activity: Activity = getActivity() ?: return
		activity.runOnUiThread(action)
	}

	/**
	 * Returns true if the call is being made on the Ui thread.
	 */
	private fun isOnUiThread() = Looper.myLooper() == Looper.getMainLooper()

	/**
	 * Returns true if `Vulkan` is used for rendering.
	 */
	private fun usesVulkan(): Boolean {
		val renderer = GodotLib.getGlobal("rendering/renderer/rendering_method")
		val renderingDevice = GodotLib.getGlobal("rendering/rendering_device/driver")
		return ("forward_plus" == renderer || "mobile" == renderer) && "vulkan" == renderingDevice
	}

	/**
	 * Returns true if can fallback to OpenGL.
	 */
	private fun canFallbackToOpenGL(): Boolean {
		return java.lang.Boolean.parseBoolean(GodotLib.getGlobal("rendering/rendering_device/fallback_to_opengl3"))
	}

	/**
	 * Returns true if the device meets the base requirements for Vulkan support, false otherwise.
	 */
	private fun meetsVulkanRequirements(packageManager: PackageManager?): Boolean {
		if (packageManager == null) {
			return false
		}
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
			if (!packageManager.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_LEVEL, 1)) {
				// Optional requirements.. log as warning if missing
				Log.w(TAG, "The vulkan hardware level does not meet the minimum requirement: 1")
			}

			// Check for api version 1.0
			return packageManager.hasSystemFeature(PackageManager.FEATURE_VULKAN_HARDWARE_VERSION, 0x400003)
		}
		return false
	}

	private fun setKeepScreenOn(enabled: Boolean) {
		runOnUiThread {
			if (enabled) {
				getActivity()?.window?.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
			} else {
				getActivity()?.window?.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
			}
		}
	}

	/**
	 * Returns true if dark mode is supported, false otherwise.
	 */
	@Keep
	private fun isDarkModeSupported(): Boolean {
		return context.resources?.configuration?.uiMode?.and(Configuration.UI_MODE_NIGHT_MASK) != Configuration.UI_MODE_NIGHT_UNDEFINED
	}

	/**
	 * Returns true if dark mode is supported and enabled, false otherwise.
	 */
	@Keep
	private fun isDarkMode(): Boolean {
		return darkMode
	}

	fun hasClipboard(): Boolean {
		return mClipboard.hasPrimaryClip()
	}

	fun getClipboard(): String {
		val clipData = mClipboard.primaryClip ?: return ""
		val text = clipData.getItemAt(0).text ?: return ""
		return text.toString()
	}

	fun setClipboard(text: String?) {
		val clip = ClipData.newPlainText("myLabel", text)
		mClipboard.setPrimaryClip(clip)
	}

	@Keep
	private fun showFilePicker(currentDirectory: String, filename: String, fileMode: Int, filters: Array<String>) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
			FilePicker.showFilePicker(context, getActivity(), currentDirectory, filename, fileMode, filters)
		}
	}

	/**
	 * Popup a dialog to input text.
	 */
	@Keep
	private fun showInputDialog(title: String, message: String, existingText: String) {
		val activity: Activity = getActivity() ?: return
		val inputField = EditText(activity)
		val paddingHorizontal = activity.resources.getDimensionPixelSize(R.dimen.input_dialog_padding_horizontal)
		val paddingVertical = activity.resources.getDimensionPixelSize(R.dimen.input_dialog_padding_vertical)
		inputField.setPadding(paddingHorizontal, paddingVertical, paddingHorizontal, paddingVertical)
		inputField.setText(existingText)
		runOnUiThread {
			val builder = AlertDialog.Builder(activity)
			builder.setMessage(message).setTitle(title).setView(inputField)
			builder.setPositiveButton(R.string.dialog_ok) {
				dialog: DialogInterface, id: Int ->
				GodotLib.inputDialogCallback(inputField.text.toString())
				dialog.dismiss()
			}
			val dialog = builder.create()
			dialog.show()
		}
	}

	@Keep
	private fun getAccentColor(): Int {
		val value = TypedValue()
		context.theme.resolveAttribute(android.R.attr.colorAccent, value, true)
		return value.data
	}

	@Keep
	private fun getBaseColor(): Int {
		val value = TypedValue()
		context.theme.resolveAttribute(android.R.attr.colorBackground, value, true)
		return value.data
	}

	/**
	 * Destroys the Godot Engine and kill the process it's running in.
	 */
	@JvmOverloads
	fun destroyAndKillProcess(destroyRunnable: Runnable? = null) {
		val host = primaryHost
		val activity = host?.activity
		if (host == null || activity == null) {
			// Run the destroyRunnable right away as we are about to force quit.
			destroyRunnable?.run()

			// Fallback to force quit
			forceQuit(0)
			return
		}

		// Store the destroyRunnable so it can be run when the engine is terminating
		runOnTerminate.set(destroyRunnable)

		runOnUiThread {
			onDestroy(host)
		}
	}

	@Keep
	private fun forceQuit(instanceId: Int): Boolean {
		primaryHost?.let {
			if (instanceId == 0) {
				it.onGodotForceQuit(this)
				return true
			} else {
				return it.onGodotForceQuit(instanceId)
			}
		} ?: return false
	}

	fun onBackPressed() {
		for (plugin in pluginRegistry.allPlugins) {
			plugin.onMainBackPressed()
		}
		renderView?.queueOnRenderThread { GodotLib.back() }
	}

	/**
	 * Used by the native code (java_godot_wrapper.h) to vibrate the device.
	 * @param durationMs
	 */
	@SuppressLint("MissingPermission")
	@Keep
	private fun vibrate(durationMs: Int, amplitude: Int) {
		if (durationMs > 0 && requestPermission("VIBRATE")) {
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
				if (amplitude <= -1) {
					vibratorService.vibrate(
						VibrationEffect.createOneShot(
							durationMs.toLong(),
							VibrationEffect.DEFAULT_AMPLITUDE
						)
					)
				} else {
					vibratorService.vibrate(
						VibrationEffect.createOneShot(
							durationMs.toLong(),
							amplitude
						)
					)
				}
			} else {
				// deprecated in API 26
				vibratorService.vibrate(durationMs.toLong())
			}
		}
	}

	private fun getCommandLine(): MutableList<String> {
		val commandLine = try {
			commandLineFileParser.parseCommandLine(requireActivity().assets.open("_cl_"))
		} catch (ignored: Exception) {
			mutableListOf()
		}

		val hostCommandLine = primaryHost?.commandLine
		if (!hostCommandLine.isNullOrEmpty()) {
			commandLine.addAll(hostCommandLine)
		}

		return commandLine
	}

	/**
	 * Used by the native code (java_godot_wrapper.h) to access the input fallback mapping.
	 * @return The input fallback mapping for the current XR mode.
	 */
	@Keep
	private fun getInputFallbackMapping(): String? {
		return xrMode.inputFallbackMapping
	}

	fun requestPermission(name: String?): Boolean {
		val activity = getActivity() ?: return false
		return requestPermission(name, activity)
	}

	fun requestPermissions(): Boolean {
		return PermissionsUtil.requestManifestPermissions(getActivity())
	}

	fun getGrantedPermissions(): Array<String?>? {
		return PermissionsUtil.getGrantedPermissions(getActivity())
	}

	/**
	 * Return true if the given feature is supported.
	 */
	@Keep
	private fun hasFeature(feature: String): Boolean {
		if (primaryHost?.supportsFeature(feature) ?: false) {
			return true;
		}

		for (plugin in pluginRegistry.allPlugins) {
			if (plugin.supportsFeature(feature)) {
				return true
			}
		}
		return false
	}

	/**
	 * Get the list of gdextension modules to register.
	 */
	@Keep
	private fun getGDExtensionConfigFiles(): Array<String> {
		val configFiles = mutableSetOf<String>()
		for (plugin in pluginRegistry.allPlugins) {
			configFiles.addAll(plugin.pluginGDExtensionLibrariesPaths)
		}

		return configFiles.toTypedArray()
	}

	@Keep
	private fun getCACertificates(): String {
		return GodotNetUtils.getCACertificates()
	}

	private fun obbIsCorrupted(f: String, mainPackMd5: String): Boolean {
		return try {
			val fis: InputStream = FileInputStream(f)

			// Create MD5 Hash
			val buffer = ByteArray(16384)
			val complete = MessageDigest.getInstance("MD5")
			var numRead: Int
			do {
				numRead = fis.read(buffer)
				if (numRead > 0) {
					complete.update(buffer, 0, numRead)
				}
			} while (numRead != -1)
			fis.close()
			val messageDigest = complete.digest()

			// Create Hex String
			val hexString = StringBuilder()
			for (b in messageDigest) {
				var s = Integer.toHexString(0xFF and b.toInt())
				if (s.length == 1) {
					s = "0$s"
				}
				hexString.append(s)
			}
			val md5str = hexString.toString()
			md5str != mainPackMd5
		} catch (e: java.lang.Exception) {
			e.printStackTrace()
			true
		}
	}

	@Keep
	private fun initInputDevices() {
		godotInputHandler.initInputDevices()
	}

	@Keep
	private fun createNewGodotInstance(args: Array<String>): Int {
		return primaryHost?.onNewGodotInstanceRequested(args) ?: -1
	}

	@Keep
	private fun nativeBeginBenchmarkMeasure(scope: String, label: String) {
		beginBenchmarkMeasure(scope, label)
	}

	@Keep
	private fun nativeEndBenchmarkMeasure(scope: String, label: String) {
		endBenchmarkMeasure(scope, label)
	}

	@Keep
	private fun nativeDumpBenchmark(benchmarkFile: String) {
		dumpBenchmark(fileAccessHandler, benchmarkFile)
	}

	@Keep
	private fun nativeSignApk(inputPath: String,
							  outputPath: String,
							  keystorePath: String,
							  keystoreUser: String,
							  keystorePassword: String): Int {
		val signResult = primaryHost?.signApk(inputPath, outputPath, keystorePath, keystoreUser, keystorePassword) ?: Error.ERR_UNAVAILABLE
		return signResult.toNativeValue()
	}

	@Keep
	private fun nativeVerifyApk(apkPath: String): Int {
		val verifyResult = primaryHost?.verifyApk(apkPath) ?: Error.ERR_UNAVAILABLE
		return verifyResult.toNativeValue()
	}
}
