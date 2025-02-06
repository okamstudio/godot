/**************************************************************************/
/*  test_logger.h                                                         */
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

#ifndef TEST_LOGGER_H
#define TEST_LOGGER_H

#include "core/io/dir_access.h"
#include "core/io/logger.h"
#include "modules/regex/regex.h"
#include "tests/test_macros.h"

namespace TestLogger {

constexpr int sleep_duration = 1200000;

void initialize_logs() {
	ProjectSettings::get_singleton()->set_setting("application/config/name", "godot_tests");
	DirAccess::make_dir_recursive_absolute(OS::get_singleton()->get_user_data_dir().path_join("logs"));
}

void cleanup_logs() {
	ProjectSettings::get_singleton()->set_setting("application/config/name", "godot_tests");
	Ref<DirAccess> dir = DirAccess::open("user://logs");
	dir->list_dir_begin();
	String file = dir->get_next();
	while (file != "") {
		if (file.match("*.log")) {
			dir->remove(file);
		}
		file = dir->get_next();
	}
	DirAccess::remove_absolute(OS::get_singleton()->get_user_data_dir().path_join("logs"));
	DirAccess::remove_absolute(OS::get_singleton()->get_user_data_dir());
}

TEST_CASE("[Logger][RotatedFileLogger] Creates the first log file and logs on it") {
	initialize_logs();

	String waiting_for_godot = "Waiting for Godot";
	RotatedFileLogger logger("user://logs/godot.log");
	logger.logf("%s", "Waiting for Godot");

	Error err = Error::OK;
	Ref<FileAccess> log = FileAccess::open("user://logs/godot.log", FileAccess::READ, &err);
	CHECK_EQ(err, Error::OK);
	CHECK_EQ(log->get_as_text(), waiting_for_godot);

	cleanup_logs();
}

void get_log_files(Vector<String> &log_files) {
	Ref<DirAccess> dir = DirAccess::open("user://logs");
	dir->list_dir_begin();
	String file = dir->get_next();
	while (file != "") {
		// Filtering godot.log because ordered_insert will put it first and should be the last.
		if (file.match("*.log") && file != "godot.log") {
			log_files.ordered_insert(file);
		}
		file = dir->get_next();
	}
	if (FileAccess::exists("user://logs/godot.log")) {
		log_files.push_back("godot.log");
	}
}

// All things related to log file rotation are in the same test because testing it require some sleeps.
TEST_CASE("[Logger][RotatedFileLogger] Rotates logs files") {
	initialize_logs();

	Vector<String> all_waiting_for_godot;

	const int number_of_files = 3;
	for (int i = 0; i < number_of_files; i++) {
		String waiting_for_godot = "Waiting for Godot " + itos(i);
		RotatedFileLogger logger("user://logs/godot.log", number_of_files);
		logger.logf("%s", waiting_for_godot.ascii().get_data());
		all_waiting_for_godot.push_back(waiting_for_godot);

		// Required to ensure the rotation of the log file.
		OS::get_singleton()->delay_usec(sleep_duration);
	}

	Vector<String> log_files;
	get_log_files(log_files);
	CHECK_MESSAGE(log_files.size() == number_of_files, "Did not rotate all files");

	for (int i = 0; i < log_files.size(); i++) {
		Error err = Error::OK;
		Ref<FileAccess> log_file = FileAccess::open("user://logs/" + log_files[i], FileAccess::READ, &err);
		REQUIRE_EQ(err, Error::OK);
		CHECK_EQ(log_file->get_as_text(), all_waiting_for_godot[i]);
	}

	// Required to ensure the rotation of the log file.
	OS::get_singleton()->delay_usec(sleep_duration);

	// This time the oldest log must be removed and godot.log updated.
	String new_waiting_for_godot = "Waiting for Godot " + itos(number_of_files);
	all_waiting_for_godot = all_waiting_for_godot.slice(1, all_waiting_for_godot.size());
	all_waiting_for_godot.push_back(new_waiting_for_godot);
	RotatedFileLogger logger("user://logs/godot.log", number_of_files);
	logger.logf("%s", new_waiting_for_godot.ascii().get_data());

	log_files.clear();
	get_log_files(log_files);
	CHECK_MESSAGE(log_files.size() == number_of_files, "Did not remove old log file");

	for (int i = 0; i < log_files.size(); i++) {
		Error err = Error::OK;
		Ref<FileAccess> log_file = FileAccess::open("user://logs/" + log_files[i], FileAccess::READ, &err);
		REQUIRE_EQ(err, Error::OK);
		CHECK_EQ(log_file->get_as_text(), all_waiting_for_godot[i]);
	}

	cleanup_logs();
}

TEST_CASE("[Logger][CompositeLogger] Logs the same into multiple loggers") {
	initialize_logs();

	Vector<Logger *> all_loggers;
	all_loggers.push_back(memnew(RotatedFileLogger("user://logs/godot_logger_1.log", 1)));
	all_loggers.push_back(memnew(RotatedFileLogger("user://logs/godot_logger_2.log", 1)));

	String waiting_for_godot = "Waiting for Godot";
	CompositeLogger logger(all_loggers);
	logger.logf("%s", "Waiting for Godot");

	Error err = Error::OK;
	Ref<FileAccess> log = FileAccess::open("user://logs/godot_logger_1.log", FileAccess::READ, &err);
	CHECK_EQ(err, Error::OK);
	CHECK_EQ(log->get_as_text(), waiting_for_godot);
	log = FileAccess::open("user://logs/godot_logger_2.log", FileAccess::READ, &err);
	CHECK_EQ(err, Error::OK);
	CHECK_EQ(log->get_as_text(), waiting_for_godot);

	cleanup_logs();
}

} // namespace TestLogger

#endif // TEST_LOGGER_H
