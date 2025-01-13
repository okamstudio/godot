/**************************************************************************/
/*  DataAccess.kt                                                         */
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
import android.os.Build
import android.util.Log
import org.godotengine.godot.error.Error
import org.godotengine.godot.io.StorageScope
import java.io.FileNotFoundException
import java.io.IOException
import java.io.InputStream
import java.nio.ByteBuffer
import java.nio.channels.Channels
import java.nio.channels.ClosedChannelException
import java.nio.channels.FileChannel
import java.nio.channels.NonWritableChannelException
import kotlin.jvm.Throws
import kotlin.math.max

/**
 * Base class for file IO operations.
 *
 * Its derived instances provide concrete implementations to handle regular file access, as well
 * as file access through the media store API on versions of Android were scoped storage is enabled.
 */
internal abstract class DataAccess {

	companion object {
		private val TAG = DataAccess::class.java.simpleName

		@Throws(java.lang.Exception::class, FileNotFoundException::class)
		fun getInputStream(storageScope: StorageScope, context: Context, filePath: String): InputStream? {
			return when(storageScope) {
				StorageScope.ASSETS -> {
					val assetData = AssetData(context, filePath, FileAccessFlags.READ)
					Channels.newInputStream(assetData.readChannel)
				}

				StorageScope.APP -> {
					val fileData = FileData(filePath, FileAccessFlags.READ)
					Channels.newInputStream(fileData.fileChannel)
				}
				StorageScope.SHARED -> {
					if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
						val mediaStoreData = MediaStoreData(context, filePath, FileAccessFlags.READ)
						Channels.newInputStream(mediaStoreData.fileChannel)
					} else {
						null
					}
				}

				StorageScope.UNKNOWN -> null
			}
		}

		@Throws(java.lang.Exception::class, FileNotFoundException::class)
		fun generateDataAccess(
			storageScope: StorageScope,
			context: Context,
			filePath: String,
			accessFlag: FileAccessFlags
		): DataAccess? {
			return when (storageScope) {
				StorageScope.APP -> FileData(filePath, accessFlag)

				StorageScope.ASSETS -> AssetData(context, filePath, accessFlag)

				StorageScope.SHARED -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
					MediaStoreData(context, filePath, accessFlag)
				} else {
					null
				}

				StorageScope.UNKNOWN -> null
			}
		}

		fun fileExists(storageScope: StorageScope, context: Context, path: String): Boolean {
			return when(storageScope) {
				StorageScope.APP -> FileData.fileExists(path)
				StorageScope.ASSETS -> AssetData.fileExists(context, path)
				StorageScope.SHARED -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
					MediaStoreData.fileExists(context, path)
				} else {
					false
				}

				StorageScope.UNKNOWN -> false
			}
		}

		fun fileLastModified(storageScope: StorageScope, context: Context, path: String): Long {
			return when(storageScope) {
				StorageScope.APP -> FileData.fileLastModified(path)
				StorageScope.ASSETS -> AssetData.fileLastModified(path)
				StorageScope.SHARED -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
					MediaStoreData.fileLastModified(context, path)
				} else {
					0L
				}

				StorageScope.UNKNOWN -> 0L
			}
		}

		fun removeFile(storageScope: StorageScope, context: Context, path: String): Boolean {
			return when(storageScope) {
				StorageScope.APP -> FileData.delete(path)
				StorageScope.ASSETS -> AssetData.delete(path)
				StorageScope.SHARED -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
					MediaStoreData.delete(context, path)
				} else {
					false
				}

				StorageScope.UNKNOWN -> false
			}
		}

		fun renameFile(storageScope: StorageScope, context: Context, from: String, to: String): Boolean {
			return when(storageScope) {
				StorageScope.APP -> FileData.rename(from, to)
				StorageScope.ASSETS -> AssetData.rename(from, to)
				StorageScope.SHARED -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
					MediaStoreData.rename(context, from, to)
				} else {
					false
				}

				StorageScope.UNKNOWN -> false
			}
		}
	}

	internal var endOfFile = false
	abstract fun close()
	abstract fun flush()
	abstract fun seek(position: Long)
	abstract fun resize(length: Long): Error
	abstract fun position(): Long
	abstract fun size(): Long
	abstract fun read(buffer: ByteBuffer): Int
	abstract fun write(buffer: ByteBuffer): Boolean

	fun seekFromEnd(positionFromEnd: Long) {
		val positionFromBeginning = max(0, size() - positionFromEnd)
		seek(positionFromBeginning)
	}

	abstract class FileChannelDataAccess(private val filePath: String) : DataAccess() {
		internal abstract val fileChannel: FileChannel

		override fun close() {
			try {
				fileChannel.close()
			} catch (e: IOException) {
				Log.w(TAG, "Exception when closing file $filePath.", e)
			}
		}

		override fun flush() {
			try {
				fileChannel.force(false)
			} catch (e: IOException) {
				Log.w(TAG, "Exception when flushing file $filePath.", e)
			}
		}

		override fun seek(position: Long) {
			try {
				fileChannel.position(position)
				endOfFile = position >= fileChannel.size()
			} catch (e: Exception) {
				Log.w(TAG, "Exception when seeking file $filePath.", e)
			}
		}

		override fun resize(length: Long): Error {
			return try {
				fileChannel.truncate(length)
				Error.OK
			} catch (e: NonWritableChannelException) {
				Error.ERR_FILE_CANT_OPEN
			} catch (e: ClosedChannelException) {
				Error.ERR_FILE_CANT_OPEN
			} catch (e: IllegalArgumentException) {
				Error.ERR_INVALID_PARAMETER
			} catch (e: IOException) {
				Error.FAILED
			}
		}

		override fun position(): Long {
			return try {
				fileChannel.position()
			} catch (e: IOException) {
				Log.w(
					TAG,
					"Exception when retrieving position for file $filePath.",
					e
				)
				0L
			}
		}

		override fun size() = try {
			fileChannel.size()
		} catch (e: IOException) {
			Log.w(TAG, "Exception when retrieving size for file $filePath.", e)
			0L
		}

		override fun read(buffer: ByteBuffer): Int {
			return try {
				val readBytes = fileChannel.read(buffer)
				endOfFile = readBytes == -1 || (fileChannel.position() >= fileChannel.size())
				if (readBytes == -1) {
					0
				} else {
					readBytes
				}
			} catch (e: IOException) {
				Log.w(TAG, "Exception while reading from file $filePath.", e)
				0
			}
		}

		override fun write(buffer: ByteBuffer): Boolean {
			try {
				val writtenBytes = fileChannel.write(buffer)
				if (writtenBytes > 0) {
					endOfFile = false
				}
				return true
			} catch (e: IOException) {
				Log.w(TAG, "Exception while writing to file $filePath.", e)
				return false
			}
		}
	}
}
