/**************************************************************************/
/*  error_list.cpp                                                        */
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

#include "error_list.h"

#include <array>

const char *error_names[] = {
	"OK", // OK
	"Failed", // FAILED
	"Unavailable", // ERR_UNAVAILABLE
	"Unconfigured", // ERR_UNCONFIGURED
	"Unauthorized", // ERR_UNAUTHORIZED
	"Parameter out of range", // ERR_PARAMETER_RANGE_ERROR
	"Out of memory", // ERR_OUT_OF_MEMORY
	"File not found", // ERR_FILE_NOT_FOUND
	"File: Bad drive", // ERR_FILE_BAD_DRIVE
	"File: Bad path", // ERR_FILE_BAD_PATH
	"File: Permission denied", // ERR_FILE_NO_PERMISSION
	"File already in use", // ERR_FILE_ALREADY_IN_USE
	"Can't open file", // ERR_FILE_CANT_OPEN
	"Can't write file", // ERR_FILE_CANT_WRITE
	"Can't read file", // ERR_FILE_CANT_READ
	"File unrecognized", // ERR_FILE_UNRECOGNIZED
	"File corrupt", // ERR_FILE_CORRUPT
	"Missing dependencies for file", // ERR_FILE_MISSING_DEPENDENCIES
	"End of file", // ERR_FILE_EOF
	"Can't open", // ERR_CANT_OPEN
	"Can't create", // ERR_CANT_CREATE
	"Query failed", // ERR_QUERY_FAILED
	"Already in use", // ERR_ALREADY_IN_USE
	"Locked", // ERR_LOCKED
	"Timeout", // ERR_TIMEOUT
	"Can't connect", // ERR_CANT_CONNECT
	"Can't resolve", // ERR_CANT_RESOLVE
	"Connection error", // ERR_CONNECTION_ERROR
	"Can't acquire resource", // ERR_CANT_ACQUIRE_RESOURCE
	"Can't fork", // ERR_CANT_FORK
	"Invalid data", // ERR_INVALID_DATA
	"Invalid parameter", // ERR_INVALID_PARAMETER
	"Already exists", // ERR_ALREADY_EXISTS
	"Does not exist", // ERR_DOES_NOT_EXIST
	"Can't read database", // ERR_DATABASE_CANT_READ
	"Can't write database", // ERR_DATABASE_CANT_WRITE
	"Compilation failed", // ERR_COMPILATION_FAILED
	"Method not found", // ERR_METHOD_NOT_FOUND
	"Link failed", // ERR_LINK_FAILED
	"Script failed", // ERR_SCRIPT_FAILED
	"Cyclic link detected", // ERR_CYCLIC_LINK
	"Invalid declaration", // ERR_INVALID_DECLARATION
	"Duplicate symbol", // ERR_DUPLICATE_SYMBOL
	"Parse error", // ERR_PARSE_ERROR
	"Busy", // ERR_BUSY
	"Skip", // ERR_SKIP
	"Help", // ERR_HELP
	"Bug", // ERR_BUG
	"Printer on fire", // ERR_PRINTER_ON_FIRE
};

static_assert(std::size(error_names) == ERR_MAX);
