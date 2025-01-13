/**************************************************************************/
/*  Callable.kt                                                           */
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

package org.godotengine.godot.variant

import androidx.annotation.Keep

/**
 * Android version of a Godot built-in Callable type representing a method or a standalone function.
 */
@Keep
class Callable private constructor(private val nativeCallablePointer: Long) {

	companion object {
		/**
		 * Invoke method [methodName] on the Godot object specified by [godotObjectId]
		 */
		@JvmStatic
		fun call(godotObjectId: Long, methodName: String, vararg methodParameters: Any): Any? {
			return nativeCallObject(godotObjectId, methodName, methodParameters)
		}

		/**
		 * Invoke method [methodName] on the Godot object specified by [godotObjectId] during idle time.
		 */
		@JvmStatic
		fun callDeferred(godotObjectId: Long, methodName: String, vararg methodParameters: Any) {
			nativeCallObjectDeferred(godotObjectId, methodName, methodParameters)
		}

		@JvmStatic
		private external fun nativeCall(pointer: Long, params: Array<out Any>): Any?

		@JvmStatic
		private external fun nativeCallObject(godotObjectId: Long, methodName: String, params: Array<out Any>): Any?

		@JvmStatic
		private external fun nativeCallObjectDeferred(godotObjectId: Long, methodName: String, params: Array<out Any>)

		@JvmStatic
		private external fun releaseNativePointer(nativePointer: Long)
	}

	/**
	 * Calls the method represented by this [Callable]. Arguments can be passed and should match the method's signature.
	 */
	internal fun call(vararg params: Any): Any? {
		if (nativeCallablePointer == 0L) {
			return null
		}

		return nativeCall(nativeCallablePointer, params)
	}

	/**
	 * Used to provide access to the native callable pointer to the native logic.
	 */
	private fun getNativePointer() = nativeCallablePointer

	/** Note that [finalize] is deprecated and shouldn't be used, unfortunately its replacement,
	 * [java.lang.ref.Cleaner], is only available on Android api 33 and higher.
	 * So we resort to using it for the time being until our min api catches up to api 33.
	 **/
	protected fun finalize() {
		releaseNativePointer(nativeCallablePointer)
	}
}
