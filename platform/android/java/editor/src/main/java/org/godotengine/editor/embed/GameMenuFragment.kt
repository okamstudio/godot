/**************************************************************************/
/*  GameMenuFragment.kt                                                   */
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

package org.godotengine.editor.embed

import android.content.Context
import android.os.Build
import android.os.Bundle
import android.view.LayoutInflater
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.PopupMenu
import android.widget.RadioButton
import android.widget.Toast
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import org.godotengine.editor.BaseGodotEditor
import org.godotengine.editor.R

/**
 * Implements the game menu interface for the Android editor.
 */
class GameMenuFragment : Fragment(), PopupMenu.OnMenuItemClickListener {

	companion object {
		val TAG = GameMenuFragment::class.java.simpleName
	}

	/**
	 * Used to be notified of events fired when interacting with the game menu
	 */
	interface GameMenuListener {

		/**
		 * Kotlin representation of the RuntimeNodeSelect::SelectMode enum in 'scene/debugger/scene_debugger.h'
		 */
		enum class RuntimeNodeSelectMode {
			SINGLE,
			LIST
		}

		/**
		 * Kotlin representation of the RuntimeNodeSelect::NodeType enum in 'scene/debugger/scene_debugger.h'
		 */
		enum class RuntimeNodeType {
			NONE,
			TYPE_2D,
			TYPE_3D
		}

		/**
		 * Kotlin representation of the EditorDebuggerNode::CameraOverride in 'editor/debugger/editor_debugger_node.h'
		 */
		enum class CameraMode {
			NONE,
			IN_GAME,
			EDITORS
		}

		fun suspendGame(suspended: Boolean)
		fun dispatchNextFrame()
		fun toggleSelectionVisibility(enabled: Boolean)
		fun overrideCamera(enabled: Boolean)
		fun selectRuntimeNode(nodeType: RuntimeNodeType)
		fun selectRuntimeNodeSelectMode(selectMode: RuntimeNodeSelectMode)
		fun reset2DCamera()
		fun reset3DCamera()
		fun manipulateCamera(mode: CameraMode)

		fun isGameEmbeddingSupported(): Boolean
		fun embedGameOnPlay(embedded: Boolean)
		fun isGameEmbedded(): Boolean

		fun enterPiPMode()
		fun minimizeGameWindow()
		fun closeGameWindow()

		fun isMinimizedButtonEnabled() = false
		fun isFullScreenButtonEnabled() = false
		fun isCloseButtonEnabled() = false
		fun isPiPButtonEnabled() = false

		fun isAlwaysOnTopSupported(): Boolean
		fun onAlwaysOnTopUpdated(alwaysOnTopEnabled: Boolean)

		fun onFullScreenUpdated(enabled: Boolean) {}
	}

	private val pauseButton: View? by lazy {
		view?.findViewById(R.id.game_menu_pause_button)
	}
	private val nextFrameButton: View? by lazy {
		view?.findViewById(R.id.game_menu_next_frame_button)
	}
	private val unselectNodesButton: RadioButton? by lazy {
		view?.findViewById(R.id.game_menu_unselect_nodes_button)
	}
	private val select2DNodesButton: RadioButton? by lazy {
		view?.findViewById(R.id.game_menu_select_2d_nodes_button)
	}
	private val select3DNodesButton: RadioButton? by lazy {
		view?.findViewById(R.id.game_menu_select_3d_nodes_button)
	}
	private val guiVisibilityButton: View? by lazy {
		view?.findViewById(R.id.game_menu_gui_visibility_button)
	}
	private val toolSelectButton: RadioButton? by lazy {
		view?.findViewById(R.id.game_menu_tool_select_button)
	}
	private val listSelectButton: RadioButton? by lazy {
		view?.findViewById(R.id.game_menu_list_select_button)
	}
	private val optionsButton: View? by lazy {
		view?.findViewById(R.id.game_menu_options_button)
	}
	private val minimizeButton: View? by lazy {
		view?.findViewById(R.id.game_menu_minimize_button)
	}
	private val pipButton: View? by lazy {
		view?.findViewById(R.id.game_menu_pip_button)
	}
	private val fullscreenButton: View? by lazy {
		view?.findViewById(R.id.game_menu_fullscreen_button)
	}
	private val closeButton: View? by lazy {
		view?.findViewById(R.id.game_menu_close_button)
	}

	private val popupMenu: PopupMenu by lazy {
		PopupMenu(context, optionsButton).apply {
			setOnMenuItemClickListener(this@GameMenuFragment)
			inflate(R.menu.options_menu)

			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
				menu.setGroupDividerEnabled(true)
			}
			if (menuListener?.isGameEmbeddingSupported() == false) {
				menu.setGroupEnabled(R.id.group_menu_embed_options, false)
				menu.setGroupVisible(R.id.group_menu_embed_options, false)
			} else {
				val isGameEmbedded = menuListener?.isGameEmbedded() == true
				menu.findItem(R.id.menu_embed_game_on_play)?.isChecked = isGameEmbedded

				val keepOnTopMenuItem = menu.findItem(R.id.menu_embed_game_keep_on_top)
				if (menuListener?.isAlwaysOnTopSupported() == false) {
					keepOnTopMenuItem?.isVisible = false
				} else {
					keepOnTopMenuItem?.isEnabled = isGameEmbedded
				}
			}
		}
	}

	private val menuItemActionView: View by lazy {
		View(context)
	}
	private val menuItemActionExpandListener = object: MenuItem.OnActionExpandListener {
		override fun onMenuItemActionExpand(item: MenuItem): Boolean {
			return false
		}

		override fun onMenuItemActionCollapse(item: MenuItem): Boolean {
			return false
		}
	}

	private var menuListener: GameMenuListener? = null
	private var alwaysOnTopChecked = false

	override fun onAttach(context: Context) {
		super.onAttach(context)
		val parentActivity = activity
		if (parentActivity is GameMenuListener) {
			menuListener = parentActivity
		} else {
			val parentFragment = parentFragment
			if (parentFragment is GameMenuListener) {
				menuListener = parentFragment
			}
		}
	}

	override fun onDetach() {
		super.onDetach()
		menuListener = null
	}

	override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, bundle: Bundle?): View? {
		return inflater.inflate(R.layout.game_menu_fragment_layout, container, false)
	}

	override fun onViewCreated(view: View, bundle: Bundle?) {
		super.onViewCreated(view, bundle)

		val gameMenuState = arguments?.getBundle(BaseGodotEditor.EXTRA_GAME_MENU_STATE) ?: Bundle()
		alwaysOnTopChecked = gameMenuState.getBoolean(BaseGodotEditor.GAME_MENU_ACTION_ALWAYS_ON_TOP, false)

		val isMinimizeButtonEnabled = menuListener?.isMinimizedButtonEnabled() == true
		val isFullScreenButtonEnabled = menuListener?.isFullScreenButtonEnabled() == true
		val isCloseButtonEnabled = menuListener?.isCloseButtonEnabled() == true
		val isPiPButtonEnabled = menuListener?.isPiPButtonEnabled() == true

		// Show the divider if any of the window controls is visible
		view.findViewById<View>(R.id.game_menu_window_controls_divider)?.isVisible =
			isMinimizeButtonEnabled ||
				isFullScreenButtonEnabled ||
				isCloseButtonEnabled ||
				isPiPButtonEnabled

		fullscreenButton?.apply{
			isVisible = isFullScreenButtonEnabled
			setOnClickListener {
				it.isActivated = !it.isActivated
				menuListener?.onFullScreenUpdated(it.isActivated)
			}
		}
		pipButton?.apply {
			isVisible = isPiPButtonEnabled
			setOnClickListener {
				menuListener?.enterPiPMode()
			}
		}
		minimizeButton?.apply {
			isVisible = isMinimizeButtonEnabled
			setOnClickListener {
				menuListener?.minimizeGameWindow()
			}
		}
		closeButton?.apply{
			isVisible = isCloseButtonEnabled
			setOnClickListener {
				menuListener?.closeGameWindow()
			}
		}
		pauseButton?.apply {
			setOnClickListener {
				val isActivated = !it.isActivated
				menuListener?.suspendGame(isActivated)
				it.isActivated = isActivated
			}
		}
		nextFrameButton?.setOnClickListener {
			menuListener?.dispatchNextFrame()
		}

		val nodeTypeOrdinal = gameMenuState.getInt(BaseGodotEditor.GAME_MENU_ACTION_SET_NODE_TYPE, GameMenuListener.RuntimeNodeType.NONE.ordinal)
		val nodeType = GameMenuListener.RuntimeNodeType.entries[nodeTypeOrdinal]
		unselectNodesButton?.apply{
			isChecked = nodeType == GameMenuListener.RuntimeNodeType.NONE
			setOnCheckedChangeListener { buttonView, isChecked ->
				if (isChecked) {
					menuListener?.selectRuntimeNode(GameMenuListener.RuntimeNodeType.NONE)
				}
			}
		}
		select2DNodesButton?.apply{
			isChecked = nodeType == GameMenuListener.RuntimeNodeType.TYPE_2D
			setOnCheckedChangeListener { buttonView, isChecked ->
				if (isChecked) {
					menuListener?.selectRuntimeNode(GameMenuListener.RuntimeNodeType.TYPE_2D)
				}
			}
		}
		select3DNodesButton?.apply{
			isChecked = nodeType == GameMenuListener.RuntimeNodeType.TYPE_3D
			setOnCheckedChangeListener { buttonView, isChecked ->
				if (isChecked) {
					menuListener?.selectRuntimeNode(GameMenuListener.RuntimeNodeType.TYPE_3D)
				}
			}
		}
		guiVisibilityButton?.apply{
			isActivated = !gameMenuState.getBoolean(BaseGodotEditor.GAME_MENU_ACTION_SET_SELECTION_VISIBLE, true)
			setOnClickListener {
				val isActivated = !it.isActivated
				menuListener?.toggleSelectionVisibility(!isActivated)
				it.isActivated = isActivated
			}
		}

		val selectModeOrdinal = gameMenuState.getInt(BaseGodotEditor.GAME_MENU_ACTION_SET_SELECT_MODE, GameMenuListener.RuntimeNodeSelectMode.SINGLE.ordinal)
		val selectMode = GameMenuListener.RuntimeNodeSelectMode.entries[selectModeOrdinal]
		toolSelectButton?.apply{
			isChecked = selectMode == GameMenuListener.RuntimeNodeSelectMode.SINGLE
			setOnCheckedChangeListener { buttonView, isChecked ->
				if (isChecked) {
					menuListener?.selectRuntimeNodeSelectMode(GameMenuListener.RuntimeNodeSelectMode.SINGLE)
				}
			}
		}
		listSelectButton?.apply{
			isChecked = selectMode == GameMenuListener.RuntimeNodeSelectMode.LIST
			setOnCheckedChangeListener { buttonView, isChecked ->
				if (isChecked) {
					menuListener?.selectRuntimeNodeSelectMode(GameMenuListener.RuntimeNodeSelectMode.LIST)
				}
			}
		}
		optionsButton?.setOnClickListener {
			popupMenu.show()
		}

		popupMenu.menu.apply {
			findItem(R.id.menu_embed_game_keep_on_top)?.isChecked = alwaysOnTopChecked

			val cameraModeOrdinal = gameMenuState.getInt(BaseGodotEditor.GAME_MENU_ACTION_SET_CAMERA_MANIPULATE_MODE, GameMenuListener.CameraMode.IN_GAME.ordinal)
			val cameraMode = GameMenuListener.CameraMode.entries[cameraModeOrdinal]
			if (cameraMode == GameMenuListener.CameraMode.IN_GAME || cameraMode == GameMenuListener.CameraMode.NONE) {
				findItem(R.id.menu_manipulate_camera_in_game)?.isChecked = true
			} else {
				findItem(R.id.menu_manipulate_camera_from_editors)?.isChecked = true
			}
		}
	}

	internal fun isAlwaysOnTop() = menuListener?.isGameEmbedded() == true && alwaysOnTopChecked

	private fun preventMenuItemCollapse(item: MenuItem) {
		item.setShowAsAction(MenuItem.SHOW_AS_ACTION_COLLAPSE_ACTION_VIEW)
		item.setActionView(menuItemActionView)
		item.setOnActionExpandListener(menuItemActionExpandListener)
	}

	override fun onMenuItemClick(item: MenuItem): Boolean {
		if (!item.hasSubMenu()) {
			preventMenuItemCollapse(item)
		}

		when(item.itemId) {
			R.id.menu_embed_game_on_play -> {
				item.isChecked = !item.isChecked
				menuListener?.embedGameOnPlay(item.isChecked)

				if (item.isChecked != menuListener?.isGameEmbedded()) {
					Toast.makeText(
						context,
						if (item.isChecked) "Restart game to embed" else "Restart Game to disable embedding",
						Toast.LENGTH_LONG
					).show()
				}
			}

			R.id.menu_embed_game_keep_on_top -> {
				item.isChecked = !item.isChecked
				alwaysOnTopChecked = item.isChecked
				menuListener?.onAlwaysOnTopUpdated(alwaysOnTopChecked)
			}

			R.id.menu_camera_override -> {
				item.isChecked = !item.isChecked
				menuListener?.overrideCamera(item.isChecked)

				popupMenu.menu.findItem(R.id.menu_camera_options)?.isEnabled = item.isChecked
			}

			R.id.menu_reset_2d_camera -> {
				menuListener?.reset2DCamera()
			}

			R.id.menu_reset_3d_camera -> {
				menuListener?.reset3DCamera()
			}

			R.id.menu_manipulate_camera_in_game -> {
				if (!item.isChecked) {
					item.isChecked = true
					menuListener?.manipulateCamera(GameMenuListener.CameraMode.IN_GAME)
				}
			}

			R.id.menu_manipulate_camera_from_editors -> {
				if (!item.isChecked) {
					item.isChecked = true
					menuListener?.manipulateCamera(GameMenuListener.CameraMode.EDITORS)
				}
			}
		}
		return false
	}
}
