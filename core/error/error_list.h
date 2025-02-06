/**************************************************************************/
/*  error_list.h                                                          */
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

#pragma once

/** Error List. Please never compare an error against FAILED
 * Either do result != OK , or !result. This way, Error fail
 * values can be more detailed in the future.
 *
 * This is a generic error list, mainly for organizing a language of returning errors.
 *
 * Errors:
 * - Are added to the Error enum in core/error/error_list.h
 * - Have a description added to error_names in core/error/error_list.cpp
 * - Are bound with BIND_CORE_ENUM_CONSTANT() in core/core_constants.cpp
 * - Have a matching Android version in platform/android/java/lib/src/org/godotengine/godot/error/Error.kt
 */

enum Error {
	OK, // (0)
	FAILED, ///< Generic fail error
	ERR_UNAVAILABLE, ///< What is requested is unsupported/unavailable
	ERR_UNCONFIGURED, ///< The object being used hasn't been properly set up yet
	ERR_UNAUTHORIZED, ///< Missing credentials for requested resource
	ERR_PARAMETER_RANGE_ERROR, ///< Parameter given out of range (5)
	ERR_OUT_OF_MEMORY, ///< Out of memory
	ERR_FILE_NOT_FOUND,
	ERR_FILE_BAD_DRIVE,
	ERR_FILE_BAD_PATH,
	ERR_FILE_NO_PERMISSION, // (10)
	ERR_FILE_ALREADY_IN_USE,
	ERR_FILE_CANT_OPEN,
	ERR_FILE_CANT_WRITE,
	ERR_FILE_CANT_READ,
	ERR_FILE_UNRECOGNIZED, // (15)
	ERR_FILE_CORRUPT,
	ERR_FILE_MISSING_DEPENDENCIES,
	ERR_FILE_EOF,
	ERR_CANT_OPEN, ///< Can't open a resource/socket/file
	ERR_CANT_CREATE, // (20)
	ERR_QUERY_FAILED,
	ERR_ALREADY_IN_USE,
	ERR_LOCKED, ///< resource is locked
	ERR_TIMEOUT,
	ERR_CANT_CONNECT, // (25)
	ERR_CANT_RESOLVE,
	ERR_CONNECTION_ERROR,
	ERR_CANT_ACQUIRE_RESOURCE,
	ERR_CANT_FORK,
	ERR_INVALID_DATA, ///< Data passed is invalid (30)
	ERR_INVALID_PARAMETER, ///< Parameter passed is invalid
	ERR_ALREADY_EXISTS, ///< When adding, item already exists
	ERR_DOES_NOT_EXIST, ///< When retrieving/erasing, if item does not exist
	ERR_DATABASE_CANT_READ, ///< database is full
	ERR_DATABASE_CANT_WRITE, ///< database is full (35)
	ERR_COMPILATION_FAILED,
	ERR_METHOD_NOT_FOUND,
	ERR_LINK_FAILED,
	ERR_SCRIPT_FAILED,
	ERR_CYCLIC_LINK, // (40)
	ERR_INVALID_DECLARATION,
	ERR_DUPLICATE_SYMBOL,
	ERR_PARSE_ERROR,
	ERR_BUSY,
	ERR_SKIP, // (45)
	ERR_HELP, ///< user requested help!!
	ERR_BUG, ///< a bug in the software certainly happened, due to a double check failing or unexpected behavior.
	ERR_PRINTER_ON_FIRE, /// the parallel port printer is engulfed in flames
	ERR_MAX, // Not being returned, value represents the number of errors
};

extern const char *error_names[];
