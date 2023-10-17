/**************************************************************************/
/*  FileData.kt                                                           */
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

import android.os.*
import java.io.File
import java.io.FileOutputStream
import java.io.RandomAccessFile
import java.nio.channels.FileChannel
import java.nio.file.attribute.BasicFileAttributes
import java.nio.file.Files
import java.nio.file.FileSystems
import java.nio.file.Path
import java.util.concurrent.TimeUnit

/**
 * Implementation of [DataAccess] which handles regular (not scoped) file access and interactions.
 */
internal class FileData(filePath: String, accessFlag: FileAccessFlags) : DataAccess.FileChannelDataAccess(filePath) {

	companion object {
		private val TAG = FileData::class.java.simpleName

		fun fileExists(path: String): Boolean {
			return try {
				File(path).isFile
			} catch (e: SecurityException) {
				false
			}
		}

		fun fileLastModified(filepath: String): Long {
			return try {
				File(filepath).lastModified() / 1000L
			} catch (e: SecurityException) {
				0L
			}
		}

		fun fileLastAccessed(filepath: String): Long {
			return try {
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
					Files.readAttributes<BasicFileAttributes>(FileSystems.getDefault().getPath(filepath), BasicFileAttributes::class.java).lastAccessTime().to(TimeUnit.SECONDS)
				} else {
					0L
				}
			} catch (e: SecurityException) {
				0L
			}
		}

		fun fileSize(filepath: String): Long {
			return try {
				if (File(filepath).isFile) {
					File(filepath).length()
				} else {
					-1L
				}
			} catch (e: SecurityException) {
				-1L
			}
		}

		fun delete(filepath: String): Boolean {
			return try {
				File(filepath).delete()
			} catch (e: Exception) {
				false
			}
		}

		fun rename(from: String, to: String): Boolean {
			return try {
				val fromFile = File(from)
				fromFile.renameTo(File(to))
			} catch (e: Exception) {
				false
			}
		}
	}

	override val fileChannel: FileChannel

	init {
		fileChannel = if (accessFlag == FileAccessFlags.WRITE) {
			// Create parent directory is necessary
			val parentDir = File(filePath).parentFile
			if (parentDir != null && !parentDir.exists()) {
				parentDir.mkdirs()
			}

			FileOutputStream(filePath, !accessFlag.shouldTruncate()).channel
		} else {
			RandomAccessFile(filePath, accessFlag.getMode()).channel
		}

		if (accessFlag.shouldTruncate()) {
			fileChannel.truncate(0)
		}
	}
}
