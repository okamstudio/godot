/**************************************************************************/
/*  FileAccessHandler.kt                                                  */
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

package org.godotengine.godot.io.file

import android.content.Context
import android.util.Log
import android.util.SparseArray
import org.godotengine.godot.error.Error
import org.godotengine.godot.io.StorageScope
import java.io.FileNotFoundException
import java.io.InputStream
import java.lang.UnsupportedOperationException
import java.nio.ByteBuffer

/**
 * Handles regular and media store file access and interactions.
 */
class FileAccessHandler(val context: Context) {

	companion object {
		private val TAG = FileAccessHandler::class.java.simpleName

		private const val INVALID_FILE_ID = 0
		private const val STARTING_FILE_ID = 1
		private val FILE_OPEN_FAILED = Pair(Error.FAILED, INVALID_FILE_ID)

		internal fun getInputStream(context: Context, storageScopeIdentifier: StorageScope.Identifier, path: String?): InputStream? {
			val storageScope = storageScopeIdentifier.identifyStorageScope(path)
			return try {
				path?.let {
					DataAccess.getInputStream(storageScope, context, path)
				}
			} catch (e: Exception) {
				null
			}
		}

		internal fun fileExists(context: Context, storageScopeIdentifier: StorageScope.Identifier, path: String?): Boolean {
			val storageScope = storageScopeIdentifier.identifyStorageScope(path)
			if (storageScope == StorageScope.UNKNOWN) {
				return false
			}

			return try {
				path?.let {
					DataAccess.fileExists(storageScope, context, it)
				} ?: false
			} catch (e: SecurityException) {
				false
			}
		}

		internal fun removeFile(context: Context, storageScopeIdentifier: StorageScope.Identifier, path: String?): Boolean {
			val storageScope = storageScopeIdentifier.identifyStorageScope(path)
			if (storageScope == StorageScope.UNKNOWN) {
				return false
			}

			return try {
				path?.let {
					DataAccess.removeFile(storageScope, context, it)
				} ?: false
			} catch (e: Exception) {
				false
			}
		}

		internal fun renameFile(context: Context, storageScopeIdentifier: StorageScope.Identifier, from: String, to: String): Boolean {
			val storageScope = storageScopeIdentifier.identifyStorageScope(from)
			if (storageScope == StorageScope.UNKNOWN) {
				return false
			}

			return try {
				DataAccess.renameFile(storageScope, context, from, to)
			} catch (e: Exception) {
				false
			}
		}
	}

	internal val storageScopeIdentifier = StorageScope.Identifier(context)
	private val files = SparseArray<DataAccess>()
	private var lastFileId = STARTING_FILE_ID

	private fun hasFileId(fileId: Int) = files.indexOfKey(fileId) >= 0

	fun canAccess(filePath: String?): Boolean {
		return storageScopeIdentifier.canAccess(filePath)
	}

	/**
	 * Returns a positive (> 0) file id when the operation succeeds.
	 * Otherwise, returns a negative value of [Error].
	 */
	fun fileOpen(path: String?, modeFlags: Int): Int {
		val (fileError, fileId) = fileOpen(path, FileAccessFlags.fromNativeModeFlags(modeFlags))
		return if (fileError == Error.OK) {
			fileId
		} else {
			// Return the negative of the [Error#toNativeValue()] value to differentiate from the
			// positive file id.
			-fileError.toNativeValue()
		}
	}

	internal fun fileOpen(path: String?, accessFlag: FileAccessFlags?): Pair<Error, Int> {
		if (accessFlag == null) {
			return FILE_OPEN_FAILED
		}

		val storageScope = storageScopeIdentifier.identifyStorageScope(path)
		if (storageScope == StorageScope.UNKNOWN) {
			return FILE_OPEN_FAILED
		}

		return try {
			path?.let {
				val dataAccess = DataAccess.generateDataAccess(storageScope, context, it, accessFlag) ?: return FILE_OPEN_FAILED

				files.put(++lastFileId, dataAccess)
				Pair(Error.OK, lastFileId)
			} ?: FILE_OPEN_FAILED
		} catch (e: FileNotFoundException) {
			Pair(Error.ERR_FILE_NOT_FOUND, INVALID_FILE_ID)
		} catch (e: UnsupportedOperationException) {
			Pair(Error.ERR_UNAVAILABLE, INVALID_FILE_ID)
		} catch (e: Exception) {
			Log.w(TAG, "Error while opening $path", e)
			FILE_OPEN_FAILED
		}
	}

	fun fileGetSize(fileId: Int): Long {
		if (!hasFileId(fileId)) {
			return 0L
		}

		return files[fileId].size()
	}

	fun fileSeek(fileId: Int, position: Long) {
		if (!hasFileId(fileId)) {
			return
		}

		files[fileId].seek(position)
	}

	fun fileSeekFromEnd(fileId: Int, position: Long) {
		if (!hasFileId(fileId)) {
			return
		}

		files[fileId].seekFromEnd(position)
	}

	fun fileRead(fileId: Int, byteBuffer: ByteBuffer?): Int {
		if (!hasFileId(fileId) || byteBuffer == null) {
			return 0
		}

		return files[fileId].read(byteBuffer)
	}

	fun fileWrite(fileId: Int, byteBuffer: ByteBuffer?): Boolean {
		if (!hasFileId(fileId) || byteBuffer == null) {
			return false
		}

		return files[fileId].write(byteBuffer)
	}

	fun fileFlush(fileId: Int) {
		if (!hasFileId(fileId)) {
			return
		}

		files[fileId].flush()
	}

	fun getInputStream(path: String?) = Companion.getInputStream(context, storageScopeIdentifier, path)

	fun renameFile(from: String, to: String) = Companion.renameFile(context, storageScopeIdentifier, from, to)

	fun fileExists(path: String?) = Companion.fileExists(context, storageScopeIdentifier, path)

	fun fileLastModified(filepath: String?): Long {
		val storageScope = storageScopeIdentifier.identifyStorageScope(filepath)
		if (storageScope == StorageScope.UNKNOWN) {
			return 0L
		}

		return try {
			filepath?.let {
				DataAccess.fileLastModified(storageScope, context, it)
			} ?: 0L
		} catch (e: SecurityException) {
			0L
		}
	}

	fun fileResize(fileId: Int, length: Long): Int {
		if (!hasFileId(fileId)) {
			return Error.FAILED.toNativeValue()
		}

		return files[fileId].resize(length).toNativeValue()
	}

	fun fileGetPosition(fileId: Int): Long {
		if (!hasFileId(fileId)) {
			return 0L
		}

		return files[fileId].position()
	}

	fun isFileEof(fileId: Int): Boolean {
		if (!hasFileId(fileId)) {
			return false
		}

		return files[fileId].endOfFile
	}

	fun setFileEof(fileId: Int, eof: Boolean) {
		val file = files[fileId] ?: return
		file.endOfFile = eof
	}

	fun fileClose(fileId: Int) {
		if (hasFileId(fileId)) {
			files[fileId].close()
			files.remove(fileId)
		}
	}
}
