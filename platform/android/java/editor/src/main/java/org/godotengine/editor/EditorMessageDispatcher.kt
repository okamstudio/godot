/**************************************************************************/
/*  EditorMessageDispatcher.kt                                            */
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

import android.annotation.SuppressLint
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Handler
import android.os.Message
import android.os.Messenger
import android.os.RemoteException
import android.util.Log
import java.util.concurrent.ConcurrentHashMap

/**
 * Used by the [BaseGodotEditor] classes to dispatch messages across processes.
 */
internal class EditorMessageDispatcher(private val editor: BaseGodotEditor) {

	companion object {
		private val TAG = EditorMessageDispatcher::class.java.simpleName

		/**
		 * Extra used to pass the message dispatcher payload through an [Intent]
		 */
		const val EXTRA_MSG_DISPATCHER_PAYLOAD = "message_dispatcher_payload"

		/**
		 * Key used to pass the editor id through a [Bundle]
		 */
		private const val KEY_EDITOR_ID = "editor_id"

		/**
		 * Key used to pass the editor messenger through a [Bundle]
		 */
		private const val KEY_EDITOR_MESSENGER = "editor_messenger"

		/**
		 * Requests the recipient to quit right away.
		 */
		private const val MSG_FORCE_QUIT = 0

		/**
		 * Requests the recipient to store the passed [android.os.Messenger] instance.
		 */
		private const val MSG_REGISTER_MESSENGER = 1

		/**
		 * Requests the recipient to dispatch the given game menu action.
		 */
		private const val MSG_DISPATCH_GAME_MENU_ACTION = 2

		/**
		 * Requests the recipient resumes itself / brings itself to front.
		 */
		private const val MSG_BRING_SELF_TO_FRONT = 3
	}

	private data class RecipientInfo(
		val messenger: Messenger,
		var pendingForceQuit: Boolean = false,
		val scheduledTasksPendingForceQuit: HashSet<Runnable> = HashSet()
	)
	private val recipientsInfos = ConcurrentHashMap<Int, RecipientInfo>()

	@SuppressLint("HandlerLeak")
	private val dispatcherHandler = object : Handler() {
		override fun handleMessage(msg: Message) {
			when (msg.what) {
				MSG_FORCE_QUIT -> {
					Log.v(TAG, "Force quitting ${editor.getEditorWindowInfo().windowId}")
					editor.finishAndRemoveTask()
				}

				MSG_REGISTER_MESSENGER -> {
					val editorId = msg.arg1
					val messenger = msg.replyTo
					registerMessenger(editorId, messenger)
				}

				MSG_DISPATCH_GAME_MENU_ACTION -> {
					val actionData = msg.data
					if (actionData != null) {
						editor.parseGameMenuAction(actionData)
					}
				}

				MSG_BRING_SELF_TO_FRONT -> editor.bringSelfToFront()

				else -> super.handleMessage(msg)
			}
		}
	}

	/**
	 * Request the window with the given [editorWindow] to force quit.
	 */
	fun requestForceQuit(editorWindow: EditorWindowInfo): Boolean {
		val editorId = editorWindow.windowId
		val info = recipientsInfos[editorId] ?: return false
		if (info.pendingForceQuit) {
			return true
		}

		val messenger = info.messenger
		return try {
			Log.v(TAG, "Requesting 'forceQuit' for $editorId")
			val msg = Message.obtain(null, MSG_FORCE_QUIT)
			messenger.send(msg)
			info.pendingForceQuit = true

			true
		} catch (e: RemoteException) {
			Log.e(TAG, "Error requesting 'forceQuit' to $editorId", e)
			cleanRecipientInfo(editorId)
			false
		}
	}

	internal fun isPendingForceQuit(editorWindow: EditorWindowInfo): Boolean {
		return recipientsInfos[editorWindow.windowId]?.pendingForceQuit == true
	}

	internal fun runTaskAfterForceQuit(editorWindow: EditorWindowInfo, task: Runnable) {
		val recipientInfo = recipientsInfos[editorWindow.windowId]
		if (recipientInfo == null || !recipientInfo.pendingForceQuit) {
			task.run()
		} else {
			recipientInfo.scheduledTasksPendingForceQuit.add(task)
		}
	}

	/**
	 * Request the given [editorWindow] to bring itself to front / resume itself.
	 *
	 * Returns true if the request was successfully dispatched, false otherwise.
	 */
	fun bringEditorWindowToFront(editorWindow: EditorWindowInfo): Boolean {
		val editorId = editorWindow.windowId
		val info = recipientsInfos[editorId] ?: return false
		val messenger = info.messenger
		return try {
			Log.v(TAG, "Requesting 'bringSelfToFront' for $editorId")
			val msg = Message.obtain(null, MSG_BRING_SELF_TO_FRONT)
			messenger.send(msg)
			true
		} catch (e: RemoteException) {
			Log.e(TAG, "Error requesting 'bringSelfToFront' to $editorId", e)
			cleanRecipientInfo(editorId)
			false
		}
	}

	/**
	 * Dispatch a game menu action to another editor instance.
	 */
	fun dispatchGameMenuAction(editorWindow: EditorWindowInfo, actionData: Bundle) {
		val editorId = editorWindow.windowId
		val info = recipientsInfos[editorId] ?: return
		val messenger = info.messenger
		try {
			Log.d(TAG, "Dispatch game menu action to $editorId")
			val msg = Message.obtain(null, MSG_DISPATCH_GAME_MENU_ACTION).apply {
				data = actionData
			}
			messenger.send(msg)
		} catch (e: RemoteException) {
			Log.e(TAG, "Error dispatching game menu action to $editorId", e)
			cleanRecipientInfo(editorId)
		}
	}

	/**
	 * Utility method to register a receiver messenger.
	 */
	private fun registerMessenger(editorId: Int, messenger: Messenger?, messengerDeathCallback: Runnable? = null) {
		try {
			if (messenger == null) {
				Log.w(TAG, "Invalid 'replyTo' payload")
			} else if (messenger.binder.isBinderAlive) {
				messenger.binder.linkToDeath({
					Log.v(TAG, "Removing messenger for $editorId")
					cleanRecipientInfo(editorId)
					messengerDeathCallback?.run()
				}, 0)
				recipientsInfos[editorId] = RecipientInfo(messenger)
			}
		} catch (e: RemoteException) {
			Log.e(TAG, "Unable to register messenger from $editorId", e)
			cleanRecipientInfo(editorId)
		}
	}

	private fun cleanRecipientInfo(editorId: Int) {
		val recipientInfo = recipientsInfos.remove(editorId) ?: return
		Log.v(TAG, "Cleaning info for recipient $editorId")
		for (task in recipientInfo.scheduledTasksPendingForceQuit) {
			task.run()
		}
	}

	/**
	 * Utility method to register a [Messenger] attached to this handler with a host.
	 *
	 * This is done so that the host can send request to the editor instance attached to this handle.
	 *
	 * Note that this is only done when the editor instance is internal (not exported) to prevent
	 * arbitrary apps from having the ability to send requests.
	 */
	private fun registerSelfTo(pm: PackageManager, host: Messenger?, selfId: Int) {
		try {
			if (host == null || !host.binder.isBinderAlive) {
				Log.v(TAG, "Host is unavailable")
				return
			}

			val activityInfo = pm.getActivityInfo(editor.componentName, 0)
			if (activityInfo.exported) {
				Log.v(TAG, "Not registering self to host as we're exported")
				return
			}

			Log.v(TAG, "Registering self $selfId to host")
			val msg = Message.obtain(null, MSG_REGISTER_MESSENGER)
			msg.arg1 = selfId
			msg.replyTo = Messenger(dispatcherHandler)
			host.send(msg)
		} catch (e: RemoteException) {
			Log.e(TAG, "Unable to register self with host", e)
		}
	}

	/**
	 * Parses the starting intent and retrieve an editor messenger if available
	 */
	fun parseStartIntent(pm: PackageManager, intent: Intent) {
		val messengerBundle = intent.getBundleExtra(EXTRA_MSG_DISPATCHER_PAYLOAD) ?: return

		// Retrieve the sender messenger payload and store it. This can be used to communicate back
		// to the sender.
		val senderId = messengerBundle.getInt(KEY_EDITOR_ID)
		val senderMessenger: Messenger? = messengerBundle.getParcelable(KEY_EDITOR_MESSENGER)
		registerMessenger(senderId, senderMessenger) {
			// Terminate current instance when parent is no longer available.
			Log.d(TAG, "Terminating current editor instance because parent is no longer available")
			editor.finish()
		}

		// Register ourselves to the sender so that it can communicate with us.
		registerSelfTo(pm, senderMessenger, editor.getEditorWindowInfo().windowId)
	}

	/**
	 * Returns the payload used by the [EditorMessageDispatcher] class to establish an IPC bridge
	 * across editor instances.
	 */
	fun getMessageDispatcherPayload(): Bundle {
		return Bundle().apply {
			putInt(KEY_EDITOR_ID, editor.getEditorWindowInfo().windowId)
			putParcelable(KEY_EDITOR_MESSENGER, Messenger(dispatcherHandler))
		}
	}
}
