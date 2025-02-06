/**************************************************************************/
/*  GodotGestureHandler.kt                                                */
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

package org.godotengine.godot.input

import android.os.Build
import android.view.GestureDetector.SimpleOnGestureListener
import android.view.InputDevice
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.ScaleGestureDetector.OnScaleGestureListener
import org.godotengine.godot.GodotLib

/**
 * Handles regular and scale gesture input related events for the [GodotView] view.
 *
 * @See https://developer.android.com/reference/android/view/GestureDetector.SimpleOnGestureListener
 * @See https://developer.android.com/reference/android/view/ScaleGestureDetector.OnScaleGestureListener
 */
internal class GodotGestureHandler(private val inputHandler: GodotInputHandler) : SimpleOnGestureListener(), OnScaleGestureListener {

	companion object {
		private val TAG = GodotGestureHandler::class.java.simpleName
	}

	/**
	 * Enable pan and scale gestures
	 */
	var panningAndScalingEnabled = false

	private var nextDownIsDoubleTap = false
	private var dragInProgress = false
	private var scaleInProgress = false
	private var contextClickInProgress = false
	private var pointerCaptureInProgress = false

	private var lastDragX: Float = 0.0f
	private var lastDragY: Float = 0.0f
	private var disableOsScrollDeadzone = false //TODO: project setting and documentation

	override fun onDown(event: MotionEvent): Boolean {
		inputHandler.handleMotionEvent(event, MotionEvent.ACTION_DOWN, nextDownIsDoubleTap)
		nextDownIsDoubleTap = false
		return true
	}

	override fun onSingleTapUp(event: MotionEvent): Boolean {
		inputHandler.handleMotionEvent(event)
		return true
	}

	override fun onLongPress(event: MotionEvent) {
		val toolType = GodotInputHandler.getEventToolType(event)
		if (toolType != MotionEvent.TOOL_TYPE_MOUSE) {
			contextClickRouter(event)
		}
	}

	private fun contextClickRouter(event: MotionEvent) {
		if (scaleInProgress || nextDownIsDoubleTap) {
			return
		}

		// Cancel the previous down event
		inputHandler.handleMotionEvent(event, MotionEvent.ACTION_CANCEL)

		// Turn a context click into a single tap right mouse button click.
		inputHandler.handleMouseEvent(
			event,
			MotionEvent.ACTION_DOWN,
			MotionEvent.BUTTON_SECONDARY,
			false
		)
		contextClickInProgress = true
	}

	fun onPointerCaptureChange(hasCapture: Boolean) {
		if (pointerCaptureInProgress == hasCapture) {
			return
		}

		if (!hasCapture) {
			// Dispatch a mouse relative ACTION_UP event to signal the end of the capture
			inputHandler.handleMouseEvent(MotionEvent.ACTION_UP, true)
		}
		pointerCaptureInProgress = hasCapture
	}

	fun onMotionEvent(event: MotionEvent): Boolean {
		return when (event.actionMasked) {
			MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL, MotionEvent.ACTION_BUTTON_RELEASE -> {
				onActionUp(event)
			}
			MotionEvent.ACTION_MOVE -> {
				onActionMove(event)
			}
			else -> false
		}
	}

	private fun onActionUp(event: MotionEvent): Boolean {
		if (event.actionMasked == MotionEvent.ACTION_CANCEL && pointerCaptureInProgress) {
			// Don't dispatch the ACTION_CANCEL while a capture is in progress
			return true
		}

		if (pointerCaptureInProgress || dragInProgress || contextClickInProgress) {
			if (contextClickInProgress || GodotInputHandler.isMouseEvent(event)) {
				// This may be an ACTION_BUTTON_RELEASE event which we don't handle,
				// so we convert it to an ACTION_UP event.
				inputHandler.handleMouseEvent(event, MotionEvent.ACTION_UP)
			} else {
				inputHandler.handleTouchEvent(event)
			}
			pointerCaptureInProgress = false
			dragInProgress = false
			contextClickInProgress = false
			lastDragX = 0.0f
			lastDragY = 0.0f
			return true
		}

		return false
	}

	private fun onActionMove(event: MotionEvent): Boolean {
		if (contextClickInProgress) {
			inputHandler.handleMouseEvent(event, event.actionMasked, MotionEvent.BUTTON_SECONDARY, false)
			return true
		} else if (disableOsScrollDeadzone && !scaleInProgress) {
			// The 'onScroll' event is triggered with a long delay.
			// Force the 'InputEventScreenDrag' event earlier here.
			// We don't toggle 'dragInProgress' here so that the scaling logic can override the drag operation if needed.
			// Once the 'onScroll' event kicks-in, 'dragInProgress' will be properly set.
			if (lastDragX != event.getX(0) || lastDragY != event.getY(0)) {
				lastDragX = event.getX(0)
				lastDragY = event.getY(0)
				inputHandler.handleMotionEvent(event)
				return true
			}
		}
		return false
	}

	override fun onDoubleTapEvent(event: MotionEvent): Boolean {
		if (event.actionMasked == MotionEvent.ACTION_UP) {
			nextDownIsDoubleTap = false
			inputHandler.handleMotionEvent(event)
		} else if (event.actionMasked == MotionEvent.ACTION_MOVE && !panningAndScalingEnabled) {
			inputHandler.handleMotionEvent(event)
		}

		return true
	}

	override fun onDoubleTap(event: MotionEvent): Boolean {
		nextDownIsDoubleTap = true
		return true
	}

	override fun onScroll(
		originEvent: MotionEvent?,
		terminusEvent: MotionEvent,
		distanceX: Float,
		distanceY: Float
	): Boolean {
		if (scaleInProgress) {
			if (dragInProgress || (disableOsScrollDeadzone && (lastDragX != 0.0f || lastDragY != 0.0f))) {
				if (originEvent != null) {
					// Cancel the drag
					inputHandler.handleMotionEvent(originEvent, MotionEvent.ACTION_CANCEL)
				}
				dragInProgress = false
				lastDragX = 0.0f
				lastDragY = 0.0f
			}
		}

		val x = terminusEvent.x
		val y = terminusEvent.y
		if (terminusEvent.pointerCount >= 2 && panningAndScalingEnabled && !pointerCaptureInProgress && !dragInProgress) {
			inputHandler.handlePanEvent(x, y, distanceX / 5f, distanceY / 5f)
		} else if (!scaleInProgress) {
			dragInProgress = true
			lastDragX = terminusEvent.getX(0)
			lastDragY = terminusEvent.getY(0)
			inputHandler.handleMotionEvent(terminusEvent)
		}
		return true
	}

	override fun onScale(detector: ScaleGestureDetector): Boolean {
		if (!panningAndScalingEnabled || pointerCaptureInProgress || dragInProgress) {
			return false
		}

		if (detector.scaleFactor >= 0.8f && detector.scaleFactor != 1f && detector.scaleFactor <= 1.2f) {
			inputHandler.handleMagnifyEvent(detector.focusX, detector.focusY, detector.scaleFactor)
		}
		return true
	}

	override fun onScaleBegin(detector: ScaleGestureDetector): Boolean {
		if (!panningAndScalingEnabled || pointerCaptureInProgress || dragInProgress) {
			return false
		}
		scaleInProgress = true
		return true
	}

	override fun onScaleEnd(detector: ScaleGestureDetector) {
		scaleInProgress = false
	}
}
