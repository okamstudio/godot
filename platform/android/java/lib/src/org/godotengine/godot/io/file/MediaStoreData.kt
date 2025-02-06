/**************************************************************************/
/*  MediaStoreData.kt                                                     */
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

import android.content.ContentUris
import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import android.util.Log
import androidx.annotation.RequiresApi

import java.io.File
import java.io.FileInputStream
import java.io.FileNotFoundException
import java.io.FileOutputStream
import java.nio.channels.FileChannel


/**
 * Implementation of [DataAccess] which handles access and interactions with file and data
 * under scoped storage via the MediaStore API.
 */
@RequiresApi(Build.VERSION_CODES.Q)
internal class MediaStoreData(context: Context, filePath: String, accessFlag: FileAccessFlags) :
	DataAccess.FileChannelDataAccess(filePath) {

	private data class DataItem(
		val id: Long,
		val uri: Uri,
		val displayName: String,
		val relativePath: String,
		val size: Int,
		val dateModified: Int,
		val mediaType: Int
	)

	companion object {
		private val TAG = MediaStoreData::class.java.simpleName

		private val COLLECTION = MediaStore.Files.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)

		private val PROJECTION = arrayOf(
			MediaStore.Files.FileColumns._ID,
			MediaStore.Files.FileColumns.DISPLAY_NAME,
			MediaStore.Files.FileColumns.RELATIVE_PATH,
			MediaStore.Files.FileColumns.SIZE,
			MediaStore.Files.FileColumns.DATE_MODIFIED,
			MediaStore.Files.FileColumns.MEDIA_TYPE,
		)

		private const val SELECTION_BY_PATH = "${MediaStore.Files.FileColumns.DISPLAY_NAME} = ? " +
			" AND ${MediaStore.Files.FileColumns.RELATIVE_PATH} = ?"

		private const val AUTHORITY_MEDIA_DOCUMENTS = "com.android.providers.media.documents"
		private const val AUTHORITY_EXTERNAL_STORAGE_DOCUMENTS = "com.android.externalstorage.documents"
		private const val AUTHORITY_DOWNLOADS_DOCUMENTS = "com.android.providers.downloads.documents"

		private fun getSelectionByPathArguments(path: String): Array<String> {
			return arrayOf(getMediaStoreDisplayName(path), getMediaStoreRelativePath(path))
		}

		private const val SELECTION_BY_ID = "${MediaStore.Files.FileColumns._ID} = ? "

		private fun getSelectionByIdArgument(id: Long) = arrayOf(id.toString())

		private fun getMediaStoreDisplayName(path: String) = File(path).name

		private fun getMediaStoreRelativePath(path: String): String {
			val pathFile = File(path)
			val environmentDir = Environment.getExternalStorageDirectory()
			var relativePath = (pathFile.parent?.replace(environmentDir.absolutePath, "") ?: "").trim('/')
			if (relativePath.isNotBlank()) {
				relativePath += "/"
			}
			return relativePath
		}

		private fun queryById(context: Context, id: Long): List<DataItem> {
			val query = context.contentResolver.query(
				COLLECTION,
				PROJECTION,
				SELECTION_BY_ID,
				getSelectionByIdArgument(id),
				null
			)
			return dataItemFromCursor(query)
		}

		private fun queryByPath(context: Context, path: String): List<DataItem> {
			val query = context.contentResolver.query(
				COLLECTION,
				PROJECTION,
				SELECTION_BY_PATH,
				getSelectionByPathArguments(path),
				null
			)
			return dataItemFromCursor(query)
		}

		private fun dataItemFromCursor(query: Cursor?): List<DataItem> {
			query?.use { cursor ->
				cursor.count
				if (cursor.count == 0) {
					return emptyList()
				}
				val idColumn = cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns._ID)
				val displayNameColumn =
					cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns.DISPLAY_NAME)
				val relativePathColumn =
					cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns.RELATIVE_PATH)
				val sizeColumn = cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns.SIZE)
				val dateModifiedColumn =
					cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns.DATE_MODIFIED)
				val mediaTypeColumn = cursor.getColumnIndexOrThrow(MediaStore.Files.FileColumns.MEDIA_TYPE)

				val result = ArrayList<DataItem>()
				while (cursor.moveToNext()) {
					val id = cursor.getLong(idColumn)
					result.add(
						DataItem(
							id,
							ContentUris.withAppendedId(COLLECTION, id),
							cursor.getString(displayNameColumn),
							cursor.getString(relativePathColumn),
							cursor.getInt(sizeColumn),
							cursor.getInt(dateModifiedColumn),
							cursor.getInt(mediaTypeColumn)
						)
					)
				}
				return result
			}
			return emptyList()
		}

		private fun addFile(context: Context, path: String): DataItem? {
			val fileDetails = ContentValues().apply {
				put(MediaStore.Files.FileColumns._ID, 0)
				put(MediaStore.Files.FileColumns.DISPLAY_NAME, getMediaStoreDisplayName(path))
				put(MediaStore.Files.FileColumns.RELATIVE_PATH, getMediaStoreRelativePath(path))
			}

			context.contentResolver.insert(COLLECTION, fileDetails) ?: return null

			// File was successfully added, let's retrieve its info
			val infos = queryByPath(context, path)
			if (infos.isEmpty()) {
				return null
			}

			return infos[0]
		}

		fun delete(context: Context, path: String): Boolean {
			val itemsToDelete = queryByPath(context, path)
			if (itemsToDelete.isEmpty()) {
				return false
			}

			val resolver = context.contentResolver
			var itemsDeleted = 0
			for (item in itemsToDelete) {
				itemsDeleted += resolver.delete(item.uri, null, null)
			}

			return itemsDeleted > 0
		}

		fun fileExists(context: Context, path: String): Boolean {
			return queryByPath(context, path).isNotEmpty()
		}

		fun fileLastModified(context: Context, path: String): Long {
			val result = queryByPath(context, path)
			if (result.isEmpty()) {
				return 0L
			}

			val dataItem = result[0]
			return dataItem.dateModified.toLong() / 1000L
		}

		fun fileLastAccessed(@Suppress("UNUSED_PARAMETER") context: Context, @Suppress("UNUSED_PARAMETER") path: String): Long {
			return 0L
		}

		fun fileSize(context: Context, path: String): Long {
			val result = queryByPath(context, path)
			if (result.isEmpty()) {
				return -1L
			}

			val dataItem = result[0]
			return dataItem.size.toLong()
		}

		fun rename(context: Context, from: String, to: String): Boolean {
			// Ensure the source exists.
			val sources = queryByPath(context, from)
			if (sources.isEmpty()) {
				return false
			}

			// Take the first source
			val source = sources[0]

			// Set up the updated values
			val updatedDetails = ContentValues().apply {
				put(MediaStore.Files.FileColumns.DISPLAY_NAME, getMediaStoreDisplayName(to))
				put(MediaStore.Files.FileColumns.RELATIVE_PATH, getMediaStoreRelativePath(to))
			}

			val updated = context.contentResolver.update(
				source.uri,
				updatedDetails,
				SELECTION_BY_ID,
				getSelectionByIdArgument(source.id)
			)
			return updated > 0
		}

		fun getUriFromDirectoryPath(context: Context, directoryPath: String): Uri? {
			if (!directoryExists(directoryPath)) {
				return null
			}
			// Check if the path is under external storage.
			val externalStorageRoot = Environment.getExternalStorageDirectory().absolutePath
			if (directoryPath.startsWith(externalStorageRoot)) {
				val relativePath = directoryPath.replaceFirst(externalStorageRoot, "").trim('/')
				val uri = Uri.Builder()
					.scheme("content")
					.authority(AUTHORITY_EXTERNAL_STORAGE_DOCUMENTS)
					.appendPath("document")
					.appendPath("primary:$relativePath")
					.build()
				return uri
			}
			return null
		}

		fun getFilePathFromUri(context: Context, uri: Uri): String? {
			// Converts content uri to filepath.
			val id = getIdFromUri(uri) ?: return null

			if (uri.authority == AUTHORITY_EXTERNAL_STORAGE_DOCUMENTS) {
				val split = id.split(":")
				val fileName = split.last()
				val relativePath = split.dropLast(1).joinToString("/")
				val fullPath = File(Environment.getExternalStorageDirectory(), "$relativePath/$fileName").absolutePath
				return fullPath
			} else {
				val id = id.toLongOrNull() ?: return null
				val dataItems = queryById(context, id)
				return if (dataItems.isNotEmpty()) {
					val dataItem = dataItems[0]
					File(Environment.getExternalStorageDirectory(), File(dataItem.relativePath, dataItem.displayName).toString()).absolutePath
				} else {
					null
				}
			}
		}

		private fun getIdFromUri(uri: Uri): String? {
			return try {
				if (uri.authority == AUTHORITY_EXTERNAL_STORAGE_DOCUMENTS || uri.authority == AUTHORITY_MEDIA_DOCUMENTS || uri.authority == AUTHORITY_DOWNLOADS_DOCUMENTS) {
					val documentId = uri.lastPathSegment ?: throw IllegalArgumentException("Invalid URI: $uri")
					documentId.substringAfter(":")
				} else {
					throw IllegalArgumentException("Unsupported URI format: $uri")
				}
			} catch (e: Exception) {
				Log.d(TAG, "Failed to parse ID from URI: $uri", e)
				null
			}
		}

		private fun directoryExists(path: String): Boolean {
			return try {
				val file = File(path)
				file.isDirectory && file.exists()
			} catch (e: SecurityException) {
				Log.d(TAG, "Failed to check directoryExists: $path", e)
				false
			}
		}

	}

	private val id: Long
	private val uri: Uri
	override val fileChannel: FileChannel

	init {
		val contentResolver = context.contentResolver
		val dataItems = queryByPath(context, filePath)

		val dataItem = when (accessFlag) {
			FileAccessFlags.READ -> {
				// The file should already exist
				if (dataItems.isEmpty()) {
					throw FileNotFoundException("Unable to access file $filePath")
				}

				val dataItem = dataItems[0]
				dataItem
			}

			FileAccessFlags.WRITE, FileAccessFlags.READ_WRITE, FileAccessFlags.WRITE_READ -> {
				// Create the file if it doesn't exist
				val dataItem = if (dataItems.isEmpty()) {
					addFile(context, filePath)
				} else {
					dataItems[0]
				}

				if (dataItem == null) {
					throw FileNotFoundException("Unable to access file $filePath")
				}
				dataItem
			}
		}

		id = dataItem.id
		uri = dataItem.uri

		val parcelFileDescriptor = contentResolver.openFileDescriptor(uri, accessFlag.getMode())
			?: throw IllegalStateException("Unable to access file descriptor")
		fileChannel = if (accessFlag == FileAccessFlags.READ) {
			FileInputStream(parcelFileDescriptor.fileDescriptor).channel
		} else {
			FileOutputStream(parcelFileDescriptor.fileDescriptor).channel
		}

		if (accessFlag.shouldTruncate()) {
			fileChannel.truncate(0)
		}
	}
}
