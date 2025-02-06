/**************************************************************************/
/*  test_config_file.h                                                    */
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

#include "core/io/config_file.h"
#include "core/os/os.h"

#include "tests/test_macros.h"

namespace TestConfigFile {

TEST_CASE("[ConfigFile] Parsing well-formatted files") {
	ConfigFile config_file;
	// Formatting is intentionally hand-edited to see how human-friendly the parser is.
	const Error error = config_file.parse(R"(
[player]

name = "Unnamed Player"
tagline="Waiting
for
Godot"

color =Color(   0, 0.5,1, 1) ; Inline comment
position= Vector2(
	3,
	4
)

[graphics]

antialiasing = true

; Testing comments and case-sensitivity...
antiAliasing = false
)");

	CHECK_MESSAGE(error == OK, "The configuration file should parse successfully.");
	CHECK_MESSAGE(
			String(config_file.get_value("player", "name")) == "Unnamed Player",
			"Reading `player/name` should return the expected value.");
	CHECK_MESSAGE(
			String(config_file.get_value("player", "tagline")) == "Waiting\nfor\nGodot",
			"Reading `player/tagline` should return the expected value.");
	CHECK_MESSAGE(
			Color(config_file.get_value("player", "color")).is_equal_approx(Color(0, 0.5, 1)),
			"Reading `player/color` should return the expected value.");
	CHECK_MESSAGE(
			Vector2(config_file.get_value("player", "position")).is_equal_approx(Vector2(3, 4)),
			"Reading `player/position` should return the expected value.");
	CHECK_MESSAGE(
			bool(config_file.get_value("graphics", "antialiasing")),
			"Reading `graphics/antialiasing` should return `true`.");
	CHECK_MESSAGE(
			bool(config_file.get_value("graphics", "antiAliasing")) == false,
			"Reading `graphics/antiAliasing` should return `false`.");

	// An empty ConfigFile is valid.
	const Error error_empty = config_file.parse("");
	CHECK_MESSAGE(error_empty == OK,
			"An empty configuration file should parse successfully.");
}

TEST_CASE("[ConfigFile] Parsing malformatted file") {
	ConfigFile config_file;
	ERR_PRINT_OFF;
	const Error error = config_file.parse(R"(
[player]

name = "Unnamed Player"" ; Extraneous closing quote.
tagline = "Waiting\nfor\nGodot"

color = Color(0, 0.5, 1) ; Missing 4th parameter.
position = Vector2(
	3,,
	4
) ; Extraneous comma.

[graphics]

antialiasing = true
antialiasing = false ; Duplicate key.
)");
	ERR_PRINT_ON;

	CHECK_MESSAGE(error == ERR_PARSE_ERROR,
			"The configuration file shouldn't parse successfully.");
}

TEST_CASE("[ConfigFile] Saving file") {
	ConfigFile config_file;
	config_file.set_value("player", "name", "Unnamed Player");
	config_file.set_value("player", "tagline", "Waiting\nfor\nGodot");
	config_file.set_value("player", "color", Color(0, 0.5, 1));
	config_file.set_value("player", "position", Vector2(3, 4));
	config_file.set_value("graphics", "antialiasing", true);
	config_file.set_value("graphics", "antiAliasing", false);
	config_file.set_value("quoted", String::utf8("静音"), 42);
	config_file.set_value("quoted", "a=b", 7);

#ifdef WINDOWS_ENABLED
	const String config_path = OS::get_singleton()->get_environment("TEMP").path_join("config.ini");
#else
	const String config_path = "/tmp/config.ini";
#endif

	config_file.save(config_path);

	// Expected contents of the saved ConfigFile.
	const String contents = String::utf8(R"([player]

name="Unnamed Player"
tagline="Waiting
for
Godot"
color=Color(0, 0.5, 1, 1)
position=Vector2(3, 4)

[graphics]

antialiasing=true
antiAliasing=false

[quoted]

"静音"=42
"a=b"=7
)");

	Ref<FileAccess> file = FileAccess::open(config_path, FileAccess::READ);
	CHECK_MESSAGE(file->get_as_utf8_string() == contents,
			"The saved configuration file should match the expected format.");
}
} // namespace TestConfigFile
