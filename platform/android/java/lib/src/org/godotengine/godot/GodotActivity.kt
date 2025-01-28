/**************************************************************************/
/*  GodotActivity.kt                                                      */
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

import android.app.Activity
import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.util.Log
import androidx.annotation.CallSuper
import androidx.annotation.LayoutRes
import androidx.fragment.app.FragmentActivity
import org.godotengine.godot.utils.PermissionsUtil
import org.godotengine.godot.utils.ProcessPhoenix

/**
 * Base abstract activity for Android apps intending to use Godot as the primary screen.
 *
 * Also a reference implementation for how to setup and use the [GodotFragment] fragment
 * within an Android app.
 */
abstract class GodotActivity : FragmentActivity(), GodotHost {

	companion object {
		private val TAG = GodotActivity::class.java.simpleName

		@JvmStatic
		protected val EXTRA_COMMAND_LINE_PARAMS = "command_line_params"

		@JvmStatic
		protected val EXTRA_NEW_LAUNCH = "new_launch_requested"

		// This window must not match those in BaseGodotEditor.RUN_GAME_INFO etc
		@JvmStatic
		private final val DEFAULT_WINDOW_ID = 664;
	}

	private val commandLineParams = ArrayList<String>()
	/**
	 * Interaction with the [Godot] object is delegated to the [GodotFragment] class.
	 */
	protected var godotFragment: GodotFragment? = null
		private set

	@CallSuper
	override fun onCreate(savedInstanceState: Bundle?) {
		val params = intent.getStringArrayExtra(EXTRA_COMMAND_LINE_PARAMS)
		Log.d(TAG, "Starting intent $intent with parameters ${params.contentToString()}")
		updateCommandLineParams(params ?: emptyArray())

		super.onCreate(savedInstanceState)

		setContentView(getGodotAppLayout())

		handleStartIntent(intent, true)

		val currentFragment = supportFragmentManager.findFragmentById(R.id.godot_fragment_container)
		if (currentFragment is GodotFragment) {
			Log.v(TAG, "Reusing existing Godot fragment instance.")
			godotFragment = currentFragment
		} else {
			Log.v(TAG, "Creating new Godot fragment instance.")
			godotFragment = initGodotInstance()
			supportFragmentManager.beginTransaction().replace(R.id.godot_fragment_container, godotFragment!!).setPrimaryNavigationFragment(godotFragment).commitNowAllowingStateLoss()
		}
	}

	override fun onNewGodotInstanceRequested(args: Array<String>): Int {
		Log.d(TAG, "Restarting with parameters ${args.contentToString()}")
		val intent = Intent()
			.setComponent(ComponentName(this, javaClass.name))
			.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
			.putExtra(EXTRA_COMMAND_LINE_PARAMS, args)
		triggerRebirth(null, intent)
		// fake 'process' id returned by create_instance() etc
		return DEFAULT_WINDOW_ID;
	}

	protected fun triggerRebirth(bundle: Bundle?, intent: Intent) {
		// Launch a new activity
		val godot = godot
		if (godot != null) {
			godot.destroyAndKillProcess {
				ProcessPhoenix.triggerRebirth(this, bundle, intent)
			}
		} else {
			ProcessPhoenix.triggerRebirth(this, bundle, intent)
		}
	}

	@LayoutRes
	protected open fun getGodotAppLayout() = R.layout.godot_app_layout

	override fun onDestroy() {
		Log.v(TAG, "Destroying GodotActivity $this...")
		super.onDestroy()
	}

	override fun onGodotForceQuit(instance: Godot) {
		runOnUiThread { terminateGodotInstance(instance) }
	}

	private fun terminateGodotInstance(instance: Godot) {
		godotFragment?.let {
			if (instance === it.godot) {
				Log.v(TAG, "Force quitting Godot instance")
				ProcessPhoenix.forceQuit(this)
			}
		}
	}

	override fun onGodotRestartRequested(instance: Godot) {
		runOnUiThread {
			godotFragment?.let {
				if (instance === it.godot) {
					// It's very hard to properly de-initialize Godot on Android to restart the game
					// from scratch. Therefore, we need to kill the whole app process and relaunch it.
					//
					// Restarting only the activity, wouldn't be enough unless it did proper cleanup (including
					// releasing and reloading native libs or resetting their state somehow and clearing static data).
					Log.v(TAG, "Restarting Godot instance...")
					ProcessPhoenix.triggerRebirth(this)
				}
			}
		}
	}

	override fun onNewIntent(newIntent: Intent) {
		super.onNewIntent(newIntent)
		intent = newIntent

		handleStartIntent(newIntent, false)

		godotFragment?.onNewIntent(newIntent)
	}

	private fun handleStartIntent(intent: Intent, newLaunch: Boolean) {
		if (!newLaunch) {
			val newLaunchRequested = intent.getBooleanExtra(EXTRA_NEW_LAUNCH, false)
			if (newLaunchRequested) {
				Log.d(TAG, "New launch requested, restarting..")
				val restartIntent = Intent(intent).putExtra(EXTRA_NEW_LAUNCH, false)
				ProcessPhoenix.triggerRebirth(this, restartIntent)
				return
			}
		}
	}

	@CallSuper
	override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
		super.onActivityResult(requestCode, resultCode, data)
		godotFragment?.onActivityResult(requestCode, resultCode, data)
	}

	@CallSuper
	override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
		super.onRequestPermissionsResult(requestCode, permissions, grantResults)
		godotFragment?.onRequestPermissionsResult(requestCode, permissions, grantResults)

		// Logging the result of permission requests
		if (requestCode == PermissionsUtil.REQUEST_ALL_PERMISSION_REQ_CODE || requestCode == PermissionsUtil.REQUEST_SINGLE_PERMISSION_REQ_CODE) {
			Log.d(TAG, "Received permissions request result..")
			for (i in permissions.indices) {
				val permissionGranted = grantResults[i] == PackageManager.PERMISSION_GRANTED
				Log.d(TAG, "Permission ${permissions[i]} ${if (permissionGranted) { "granted"} else { "denied" }}")
			}
		}
	}

	override fun onBackPressed() {
		godotFragment?.onBackPressed() ?: super.onBackPressed()
	}

	override fun getActivity(): Activity? {
		return this
	}

	override fun getGodot(): Godot? {
		return godotFragment?.godot
	}

	/**
	 * Used to initialize the Godot fragment instance in [onCreate].
	 */
	protected open fun initGodotInstance(): GodotFragment {
		return GodotFragment()
	}

	@CallSuper
	protected open fun updateCommandLineParams(args: Array<String>) {
		// Update the list of command line params with the new args
		commandLineParams.clear()
		if (args.isNotEmpty()) {
			commandLineParams.addAll(args)
		}
	}

	final override fun getCommandLine() = commandLineParams
}
