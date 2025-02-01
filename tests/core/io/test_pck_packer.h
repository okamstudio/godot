/**************************************************************************/
/*  test_pck_packer.h                                                     */
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

#include "core/io/file_access_pack.h"
#include "core/io/pck_packer.h"
#include "core/os/os.h"

#include "tests/test_utils.h"
#include "thirdparty/doctest/doctest.h"

namespace TestPCKPacker {

TEST_CASE("[PCKPacker] Pack an empty PCK file") {
	PCKPacker pck_packer;
	const String output_pck_path = TestUtils::get_temp_path("output_empty.pck");
	CHECK_MESSAGE(
			pck_packer.pck_start(output_pck_path) == OK,
			"Starting a PCK file should return an OK error code.");

	CHECK_MESSAGE(
			pck_packer.flush() == OK,
			"Flushing the PCK should return an OK error code.");

	Error err;
	Ref<FileAccess> f = FileAccess::open(output_pck_path, FileAccess::READ, &err);
	CHECK_MESSAGE(
			err == OK,
			"The generated empty PCK file should be opened successfully.");
	CHECK_MESSAGE(
			f->get_length() >= 100,
			"The generated empty PCK file shouldn't be too small (it should have the PCK header).");
	CHECK_MESSAGE(
			f->get_length() <= 500,
			"The generated empty PCK file shouldn't be too large.");
}

TEST_CASE("[PCKPacker] Pack empty with zero alignment invalid") {
	PCKPacker pck_packer;
	const String output_pck_path = TestUtils::get_temp_path("output_empty.pck");
	ERR_PRINT_OFF;
	CHECK_MESSAGE(pck_packer.pck_start(output_pck_path, 0) != OK, "PCK with zero alignment should fail.");
	ERR_PRINT_ON;
}

TEST_CASE("[PCKPacker] Pack empty with invalid key") {
	PCKPacker pck_packer;
	const String output_pck_path = TestUtils::get_temp_path("output_empty.pck");
	ERR_PRINT_OFF;
	CHECK_MESSAGE(pck_packer.pck_start(output_pck_path, 32, "") != OK, "PCK with invalid key should fail.");
	ERR_PRINT_ON;
}

TEST_CASE("[PCKPacker] Pack a PCK file with some files and directories") {
	PCKPacker pck_packer;
	const String output_pck_path = TestUtils::get_temp_path("output_with_files.pck");
	CHECK_MESSAGE(
			pck_packer.pck_start(output_pck_path) == OK,
			"Starting a PCK file should return an OK error code.");

	const String base_dir = OS::get_singleton()->get_executable_path().get_base_dir();

	CHECK_MESSAGE(
			pck_packer.add_file("version.py", base_dir.path_join("../version.py"), "version.py") == OK,
			"Adding a file to the PCK should return an OK error code.");
	CHECK_MESSAGE(
			pck_packer.add_file("some/directories with spaces/to/create/icon.png", base_dir.path_join("../icon.png")) == OK,
			"Adding a file to a new subdirectory in the PCK should return an OK error code.");
	CHECK_MESSAGE(
			pck_packer.add_file("some/directories with spaces/to/create/icon.svg", base_dir.path_join("../icon.svg")) == OK,
			"Adding a file to an existing subdirectory in the PCK should return an OK error code.");
	CHECK_MESSAGE(
			pck_packer.add_file("some/directories with spaces/to/create/icon.png", base_dir.path_join("../logo.png")) == OK,
			"Overriding a non-flushed file to an existing subdirectory in the PCK should return an OK error code.");
	CHECK_MESSAGE(
			pck_packer.flush() == OK,
			"Flushing the PCK should return an OK error code.");

	Error err;
	Ref<FileAccess> f = FileAccess::open(output_pck_path, FileAccess::READ, &err);
	CHECK_MESSAGE(
			err == OK,
			"The generated non-empty PCK file should be opened successfully.");
	CHECK_MESSAGE(
			f->get_length() >= 18000,
			"The generated non-empty PCK file should be large enough to actually hold the contents specified above.");
	CHECK_MESSAGE(
			f->get_length() <= 27000,
			"The generated non-empty PCK file shouldn't be too large.");
}
} // namespace TestPCKPacker
