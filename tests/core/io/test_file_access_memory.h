/**************************************************************************/
/*  test_file_access_memory.h                                             */
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

#ifndef TEST_FILE_ACCESS_MEMORY_H
#define TEST_FILE_ACCESS_MEMORY_H

#include "core/io/file_access.h"
#include "core/io/file_access_memory.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestFileAccessMemory {

TEST_CASE("[Editor][FileAccessMemory] Read/write string") {
	Ref<FileAccessMemory> f = FileAccessMemory::open("res://string_test", FileAccessMemory::READ_WRITE);
	REQUIRE(!f.is_null());

	f->store_string("test");
	f->store_string(" ");
	f->store_string("string");

	f->seek(0);
	CHECK(f->get_as_text() == "test string");
}

TEST_CASE("[Editor][FileAccessMemory] Read/write buffer") {
	Ref<FileAccessMemory> f = FileAccessMemory::open("res://buffer_test", FileAccessMemory::READ_WRITE);
	REQUIRE(!f.is_null());

	f->seek(0);
	f->store_64(123456);
	f->store_32(654321);
	f->store_16(65000);
	f->store_8(123);
	f->seek(0);

	// Read forwards
	CHECK(f->get_64() == 123456);
	CHECK(f->get_32() == 654321);
	CHECK(f->get_16() == 65000);
	CHECK(f->get_8() == 123);

	// Seek backwards
	f->seek_end(-1);
	CHECK(f->get_8() == 123);
	f->seek_end(-1 - 2);
	CHECK(f->get_16() == 65000);
	f->seek_end(-1 - 2 - 4);
	CHECK(f->get_32() == 654321);
	f->seek_end(-1 - 2 - 4 - 8);
	CHECK(f->get_64() == 123456);
}

TEST_CASE("[Editor][FileAccessMemory] Testing in memory dirs") {
	Ref<DirAccessMemory> da = DirAccessMemory::create(DirAccess::ACCESS_RESOURCES);

	// Creating subfolder to non-existent folders shouldn't work.
	CHECK(da->make_dir("test/subfolder") == ERR_CANT_CREATE);

	CHECK(da->make_dir("test") == OK);
	CHECK(da->dir_exists("test"));
	CHECK(da->dir_exists("res://test"));
	CHECK_FALSE(da->dir_exists("non_existing"));
	CHECK_FALSE(da->dir_exists("res://non_existing"));

	CHECK(da->make_dir_recursive("test/subfolder\\subsub") == OK);
	CHECK(da->dir_exists("res://test"));
	CHECK(da->dir_exists("res://test/"));
	CHECK(da->dir_exists("res://test/subfolder"));
	CHECK(da->dir_exists("res://test/subfolder/"));
	CHECK(da->dir_exists("res://test/subfolder/subsub"));
	CHECK(da->dir_exists("res://test/subfolder/subsub/"));
	CHECK(da->change_dir("test") == OK);
	CHECK(da->change_dir("..") == OK);
	CHECK(da->get_current_dir() == "res://");
	CHECK(da->change_dir("test/subfolder//") == OK);

	da->list_dir_begin();
	CHECK(da->get_next() == "subsub");
	CHECK(da->get_next().is_empty());
	da->list_dir_end();
}

} // namespace TestFileAccessMemory

#endif // TEST_FILE_ACCESS_MEMORY_H
