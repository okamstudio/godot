/**************************************************************************/
/*  test_theme.h                                                          */
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

#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/theme.h"
#include "tests/test_tools.h"

#include "thirdparty/doctest/doctest.h"

namespace TestTheme {

class Fixture {
public:
	struct DataEntry {
		Theme::DataType type;
		Variant value;
	} const valid_data[Theme::DATA_TYPE_MAX] = {
		{ Theme::DATA_TYPE_COLOR, Color() },
		{ Theme::DATA_TYPE_CONSTANT, 42 },
		{ Theme::DATA_TYPE_FONT, Ref<FontFile>(memnew(FontFile)) },
		{ Theme::DATA_TYPE_FONT_SIZE, 42 },
		{ Theme::DATA_TYPE_ICON, Ref<Texture>(memnew(ImageTexture)) },
		{ Theme::DATA_TYPE_STYLEBOX, Ref<StyleBox>(memnew(StyleBoxFlat)) },
	};

	const StringName valid_item_name = "valid_item_name";
	const StringName valid_type_name = "ValidTypeName";
};

TEST_CASE_FIXTURE(Fixture, "[Theme] Good theme type names") {
	StringName names[] = {
		"", // Empty name.
		"CapitalizedName",
		"snake_cased_name",
		"42",
		"_Underscore_",
	};

	SUBCASE("add_type") {
		for (const StringName &name : names) {
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->add_type(name);
			CHECK_FALSE(ed.has_error);
		}
	}

	SUBCASE("set_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->set_theme_item(entry.type, valid_item_name, name, entry.value);
				CHECK_FALSE(ed.has_error);
			}
		}
	}

	SUBCASE("add_theme_item_type") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->add_theme_item_type(entry.type, name);
				CHECK_FALSE(ed.has_error);
			}
		}
	}

	SUBCASE("set_type_variation") {
		for (const StringName &name : names) {
			if (name == StringName()) { // Skip empty here, not allowed.
				continue;
			}
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->set_type_variation(valid_type_name, name);
			CHECK_FALSE(ed.has_error);
		}
		for (const StringName &name : names) {
			if (name == StringName()) { // Skip empty here, not allowed.
				continue;
			}
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->set_type_variation(name, valid_type_name);
			CHECK_FALSE(ed.has_error);
		}
	}
}

TEST_CASE_FIXTURE(Fixture, "[Theme] Bad theme type names") {
	StringName names[] = {
		"With/Slash",
		"With Space",
		"With@various$symbols!",
		String::utf8("contains_汉字"),
	};

	ERR_PRINT_OFF; // All these rightfully print errors.

	SUBCASE("add_type") {
		for (const StringName &name : names) {
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->add_type(name);
			CHECK(ed.has_error);
		}
	}

	SUBCASE("set_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->set_theme_item(entry.type, valid_item_name, name, entry.value);
				CHECK(ed.has_error);
			}
		}
	}

	SUBCASE("add_theme_item_type") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->add_theme_item_type(entry.type, name);
				CHECK(ed.has_error);
			}
		}
	}

	SUBCASE("set_type_variation") {
		for (const StringName &name : names) {
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->set_type_variation(valid_type_name, name);
			CHECK(ed.has_error);
		}
		for (const StringName &name : names) {
			Ref<Theme> theme = memnew(Theme);

			ErrorDetector ed;
			theme->set_type_variation(name, valid_type_name);
			CHECK(ed.has_error);
		}
	}

	ERR_PRINT_ON;
}

TEST_CASE_FIXTURE(Fixture, "[Theme] Good theme item names") {
	StringName names[] = {
		"CapitalizedName",
		"snake_cased_name",
		"42",
		"_Underscore_",
	};

	SUBCASE("set_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->set_theme_item(entry.type, name, valid_type_name, entry.value);
				CHECK_FALSE(ed.has_error);
				CHECK(theme->has_theme_item(entry.type, name, valid_type_name));
			}
		}
	}

	SUBCASE("rename_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);
				theme->set_theme_item(entry.type, valid_item_name, valid_type_name, entry.value);

				ErrorDetector ed;
				theme->rename_theme_item(entry.type, valid_item_name, name, valid_type_name);
				CHECK_FALSE(ed.has_error);
				CHECK_FALSE(theme->has_theme_item(entry.type, valid_item_name, valid_type_name));
				CHECK(theme->has_theme_item(entry.type, name, valid_type_name));
			}
		}
	}
}

TEST_CASE_FIXTURE(Fixture, "[Theme] Bad theme item names") {
	StringName names[] = {
		"", // Empty name.
		"With/Slash",
		"With Space",
		"With@various$symbols!",
		String::utf8("contains_汉字"),
	};

	ERR_PRINT_OFF; // All these rightfully print errors.

	SUBCASE("set_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);

				ErrorDetector ed;
				theme->set_theme_item(entry.type, name, valid_type_name, entry.value);
				CHECK(ed.has_error);
				CHECK_FALSE(theme->has_theme_item(entry.type, name, valid_type_name));
			}
		}
	}

	SUBCASE("rename_theme_item") {
		for (const StringName &name : names) {
			for (const DataEntry &entry : valid_data) {
				Ref<Theme> theme = memnew(Theme);
				theme->set_theme_item(entry.type, valid_item_name, valid_type_name, entry.value);

				ErrorDetector ed;
				theme->rename_theme_item(entry.type, valid_item_name, name, valid_type_name);
				CHECK(ed.has_error);
				CHECK(theme->has_theme_item(entry.type, valid_item_name, valid_type_name));
				CHECK_FALSE(theme->has_theme_item(entry.type, name, valid_type_name));
			}
		}
	}

	ERR_PRINT_ON;
}

} // namespace TestTheme
