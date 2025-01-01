/**************************************************************************/
/*  Error.kt                                                              */
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

package org.godotengine.godot.error

/**
 * Godot error list.
 *
 * This enum MUST match its native counterpart in 'core/error/error_list.h'
 */
enum class Error(private val description: String) {
	OK("OK"), // (0)
	FAILED("Failed"), ///< Generic fail error
	ERR_UNAVAILABLE("Unavailable"), ///< What is requested is unsupported/unavailable
	ERR_UNCONFIGURED("Unconfigured"), ///< The object being used hasn't been properly set up yet
	ERR_UNAUTHORIZED("Unauthorized"), ///< Missing credentials for requested resource
	ERR_PARAMETER_RANGE_ERROR("Parameter out of range"), ///< Parameter given out of range (5)
	ERR_OUT_OF_MEMORY("Out of memory"), ///< Out of memory
	ERR_FILE_NOT_FOUND("File not found"),
	ERR_FILE_BAD_DRIVE("File: Bad drive"),
	ERR_FILE_BAD_PATH("File: Bad path"),
	ERR_FILE_NO_PERMISSION("File: Permission denied"), // (10)
	ERR_FILE_ALREADY_IN_USE("File already in use"),
	ERR_FILE_CANT_OPEN("Can't open file"),
	ERR_FILE_CANT_WRITE("Can't write file"),
	ERR_FILE_CANT_READ("Can't read file"),
	ERR_FILE_UNRECOGNIZED("File unrecognized"), // (15)
	ERR_FILE_CORRUPT("File corrupt"),
	ERR_FILE_MISSING_DEPENDENCIES("Missing dependencies for file"),
	ERR_FILE_EOF("End of file"),
	ERR_CANT_OPEN("Can't open"), ///< Can't open a resource/socket/file
	ERR_CANT_CREATE("Can't create"), // (20)
	ERR_QUERY_FAILED("Query failed"),
	ERR_ALREADY_IN_USE("Already in use"),
	ERR_LOCKED("Locked"), ///< resource is locked
	ERR_TIMEOUT("Timeout"),
	ERR_CANT_CONNECT("Can't connect"), // (25)
	ERR_CANT_RESOLVE("Can't resolve"),
	ERR_CONNECTION_ERROR("Connection error"),
	ERR_CANT_ACQUIRE_RESOURCE("Can't acquire resource"),
	ERR_CANT_FORK("Can't fork"),
	ERR_INVALID_DATA("Invalid data"), ///< Data passed is invalid (30)
	ERR_INVALID_PARAMETER("Invalid parameter"), ///< Parameter passed is invalid
	ERR_ALREADY_EXISTS("Already exists"), ///< When adding, item already exists
	ERR_DOES_NOT_EXIST("Does not exist"), ///< When retrieving/erasing, if item does not exist
	ERR_DATABASE_CANT_READ("Can't read database"), ///< database is full
	ERR_DATABASE_CANT_WRITE("Can't write database"), ///< database is full (35)
	ERR_COMPILATION_FAILED("Compilation failed"),
	ERR_METHOD_NOT_FOUND("Method not found"),
	ERR_LINK_FAILED("Link failed"),
	ERR_SCRIPT_FAILED("Script failed"),
	ERR_CYCLIC_LINK("Cyclic link detected"), // (40)
	ERR_INVALID_DECLARATION("Invalid declaration"),
	ERR_DUPLICATE_SYMBOL("Duplicate symbol"),
	ERR_PARSE_ERROR("Parse error"),
	ERR_BUSY("Busy"),
	ERR_SKIP("Skip"), // (45)
	ERR_HELP("Help"), ///< user requested help!!
	ERR_BUG("Bug"), ///< a bug in the software certainly happened, due to a double check failing or unexpected behavior.
	ERR_PRINTER_ON_FIRE("Printer on fire"); /// the parallel port printer is engulfed in flames

	companion object {
		internal fun fromNativeValue(nativeValue: Int): Error? {
			return Error.entries.getOrNull(nativeValue)
		}
	}

	fun toNativeValue(): Int = this.ordinal

	override fun toString(): String {
		return description
	}
}
