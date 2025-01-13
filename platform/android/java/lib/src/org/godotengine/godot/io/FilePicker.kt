/**************************************************************************/
/*  FilePicker.kt                                                         */
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

package org.godotengine.godot.io

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.DocumentsContract
import android.util.Log
import android.webkit.MimeTypeMap
import androidx.annotation.RequiresApi
import org.godotengine.godot.GodotLib
import org.godotengine.godot.io.file.MediaStoreData

/**
 * Utility class for managing file selection and file picker activities.
 *
 * It provides methods to launch a file picker and handle the result, supporting various file modes,
 * including opening files, directories, and saving files.
 */
internal class FilePicker {
	companion object {
		private const val FILE_PICKER_REQUEST = 1000
		private val TAG = FilePicker::class.java.simpleName

		// Constants for fileMode values
		private const val FILE_MODE_OPEN_FILE = 0
		private const val FILE_MODE_OPEN_FILES = 1
		private const val FILE_MODE_OPEN_DIR = 2
		private const val FILE_MODE_OPEN_ANY = 3
		private const val FILE_MODE_SAVE_FILE = 4

		/**
		 * Handles the result from a file picker activity and processes the selected file(s) or directory.
		 *
		 * @param context The context from which the file picker was launched.
		 * @param requestCode The request code used when starting the file picker activity.
		 * @param resultCode The result code returned by the activity.
		 * @param data The intent data containing the selected file(s) or directory.
		 */
		@RequiresApi(Build.VERSION_CODES.Q)
		fun handleActivityResult(context: Context, requestCode: Int, resultCode: Int, data: Intent?) {
			if (requestCode == FILE_PICKER_REQUEST) {
				if (resultCode == Activity.RESULT_CANCELED) {
					Log.d(TAG, "File picker canceled")
					GodotLib.filePickerCallback(false, emptyArray())
					return
				}
				if (resultCode == Activity.RESULT_OK) {
					val selectedPaths: MutableList<String> = mutableListOf()
					// Handle multiple file selection.
					val clipData = data?.clipData
					if (clipData != null) {
						for (i in 0 until clipData.itemCount) {
							val uri = clipData.getItemAt(i).uri
							uri?.let {
								val filepath = MediaStoreData.getFilePathFromUri(context, uri)
								if (filepath != null) {
									selectedPaths.add(filepath)
								} else {
									Log.d(TAG, "null filepath URI: $it")
								}
							}
						}
					} else {
						val uri: Uri? = data?.data
						uri?.let {
							val filepath = MediaStoreData.getFilePathFromUri(context, uri)
							if (filepath != null) {
								selectedPaths.add(filepath)
							} else {
								Log.d(TAG, "null filepath URI: $it")
							}
						}
					}

					if (selectedPaths.isNotEmpty()) {
						GodotLib.filePickerCallback(true, selectedPaths.toTypedArray())
					} else {
						GodotLib.filePickerCallback(false, emptyArray())
					}
				}
			}
		}

		/**
		 * Launches a file picker activity with specified settings based on the mode, initial directory,
		 * file type filters, and other parameters.
		 *
		 * @param context The context from which to start the file picker.
		 * @param activity The activity instance used to initiate the picker. Required for activity results.
		 * @param currentDirectory The directory path to start the file picker in.
		 * @param filename The name of the file when using save mode.
		 * @param fileMode The mode to operate in, specifying open, save, or directory select.
		 * @param filters Array of MIME types to filter file selection.
		 */
		@RequiresApi(Build.VERSION_CODES.Q)
		fun showFilePicker(context: Context, activity: Activity?, currentDirectory: String, filename: String, fileMode: Int, filters: Array<String>) {
			val intent = when (fileMode) {
				FILE_MODE_OPEN_DIR -> Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
				FILE_MODE_SAVE_FILE -> Intent(Intent.ACTION_CREATE_DOCUMENT)
				else -> Intent(Intent.ACTION_OPEN_DOCUMENT)
			}
			val initialDirectory = MediaStoreData.getUriFromDirectoryPath(context, currentDirectory)
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && initialDirectory != null) {
				intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI, initialDirectory)
			} else {
				Log.d(TAG, "Error cannot set initial directory")
			}
			if (fileMode == FILE_MODE_OPEN_FILES) {
				intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true) // Set multi select for FILE_MODE_OPEN_FILES
			} else if (fileMode == FILE_MODE_SAVE_FILE) {
				intent.putExtra(Intent.EXTRA_TITLE, filename) // Set filename for FILE_MODE_SAVE_FILE
			}
			// ACTION_OPEN_DOCUMENT_TREE does not support intent type
			if (fileMode != FILE_MODE_OPEN_DIR) {
				intent.type = "*/*"
				if (filters.isNotEmpty()) {
					val resolvedFilters = filters.map { resolveMimeType(it) }.distinct()
					if (resolvedFilters.size == 1) {
						intent.type = resolvedFilters[0]
					} else {
						intent.putExtra(Intent.EXTRA_MIME_TYPES, resolvedFilters.toTypedArray())
					}
				}
				intent.addCategory(Intent.CATEGORY_OPENABLE)
			}
			intent.putExtra(Intent.EXTRA_LOCAL_ONLY, true)
			activity?.startActivityForResult(intent, FILE_PICKER_REQUEST)
		}

		/**
		 * Retrieves the MIME type for a given file extension.
		 *
		 * @param ext the extension whose MIME type is to be determined.
		 * @return the MIME type as a string, or "application/octet-stream" if the type is unknown.
		 */
		private fun resolveMimeType(ext: String): String {
			val mimeTypeMap = MimeTypeMap.getSingleton()
			var input = ext

			// Fix for extensions like "*.txt" or ".txt".
			if (ext.contains(".")) {
				input = ext.substring(ext.indexOf(".") + 1);
			}

			// Check if the input is already a valid MIME type.
			if (mimeTypeMap.hasMimeType(input)) {
				return input
			}

			val resolvedMimeType = mimeTypeMap.getMimeTypeFromExtension(input)
			if (resolvedMimeType != null) {
				return resolvedMimeType
			}
			// Check for wildcard MIME types like "image/*".
			if (input.contains("/*")) {
				val category = input.substringBefore("/*")
				return when (category) {
					"image" -> "image/*"
					"video" -> "video/*"
					"audio" -> "audio/*"
					else -> "application/octet-stream"
				}
			}
			// Fallback to a generic MIME type if the input is neither a valid extension nor MIME type.
			return "application/octet-stream"
		}
	}
}
