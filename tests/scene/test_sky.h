/**************************************************************************/
/*  test_sky.h                                                            */
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

#ifndef TEST_SKY_H
#define TEST_SKY_H

#include "scene/resources/sky.h"

#include "tests/test_macros.h"

namespace TestSky {

TEST_CASE("[SceneTree][Sky] Constructor") {
	Ref<Sky> test_sky;
	test_sky.instantiate();

	CHECK(test_sky->get_process_mode() == Sky::PROCESS_MODE_AUTOMATIC);
	CHECK(test_sky->get_radiance_size() == Sky::RADIANCE_SIZE_256);
	CHECK(test_sky->get_material().is_null());
}

TEST_CASE("[SceneTree][Sky] Radiance size setter and getter") {
	Ref<Sky> test_sky;
	test_sky.instantiate();

	// Check default.
	CHECK(test_sky->get_radiance_size() == Sky::RADIANCE_SIZE_256);

	test_sky->set_radiance_size(Sky::RADIANCE_SIZE_1024);
	CHECK(test_sky->get_radiance_size() == Sky::RADIANCE_SIZE_1024);

	ERR_PRINT_OFF;
	// Check setting invalid radiance size.
	test_sky->set_radiance_size(Sky::RADIANCE_SIZE_MAX);
	ERR_PRINT_ON;

	CHECK(test_sky->get_radiance_size() == Sky::RADIANCE_SIZE_1024);
}

TEST_CASE("[SceneTree][Sky] Process mode setter and getter") {
	Ref<Sky> test_sky;
	test_sky.instantiate();

	// Check default.
	CHECK(test_sky->get_process_mode() == Sky::PROCESS_MODE_AUTOMATIC);

	test_sky->set_process_mode(Sky::PROCESS_MODE_INCREMENTAL);
	CHECK(test_sky->get_process_mode() == Sky::PROCESS_MODE_INCREMENTAL);
}

TEST_CASE("[SceneTree][Sky] Material setter and getter") {
	Ref<Sky> test_sky;
	test_sky.instantiate();

	Ref<Material> material;
	material.instantiate();

	SUBCASE("Material passed to the class should remain the same") {
		test_sky->set_material(material);
		CHECK(test_sky->get_material() == material);
	}
	SUBCASE("Material passed many times to the class should remain the same") {
		test_sky->set_material(material);
		test_sky->set_material(material);
		test_sky->set_material(material);
		CHECK(test_sky->get_material() == material);
	}
	SUBCASE("Material rewrite testing") {
		Ref<Material> material1;
		Ref<Material> material2;
		material1.instantiate();
		material2.instantiate();

		test_sky->set_material(material1);
		test_sky->set_material(material2);
		CHECK_MESSAGE(test_sky->get_material() != material1,
				"After rewrite, second material should be in class.");
		CHECK_MESSAGE(test_sky->get_material() == material2,
				"After rewrite, second material should be in class.");
	}

	SUBCASE("Assign same material to two skys") {
		Ref<Sky> sky2;
		sky2.instantiate();

		test_sky->set_material(material);
		sky2->set_material(material);
		CHECK_MESSAGE(test_sky->get_material() == sky2->get_material(),
				"Both skys should have the same material.");
	}

	SUBCASE("Swapping materials between two skys") {
		Ref<Sky> sky2;
		sky2.instantiate();
		Ref<Material> material1;
		Ref<Material> material2;
		material1.instantiate();
		material2.instantiate();

		test_sky->set_material(material1);
		sky2->set_material(material2);
		CHECK(test_sky->get_material() == material1);
		CHECK(sky2->get_material() == material2);

		// Do the swap.
		Ref<Material> temp = test_sky->get_material();
		test_sky->set_material(sky2->get_material());
		sky2->set_material(temp);

		CHECK(test_sky->get_material() == material2);
		CHECK(sky2->get_material() == material1);
	}
}

TEST_CASE("[SceneTree][Sky] Invalid radiance size handling") {
	Ref<Sky> test_sky;
	test_sky.instantiate();

	// Attempt to set an invalid radiance size.
	ERR_PRINT_OFF;
	test_sky->set_radiance_size(Sky::RADIANCE_SIZE_MAX);
	ERR_PRINT_ON;

	// Verify that the radiance size remains unchanged.
	CHECK(test_sky->get_radiance_size() == Sky::RADIANCE_SIZE_256);
}

TEST_CASE("[SceneTree][Sky] Process mode variations") {
	Ref<Sky> test_sky;
	test_sky.instantiate();

	// Test all process modes.
	const Sky::ProcessMode process_modes[] = {
		Sky::PROCESS_MODE_AUTOMATIC,
		Sky::PROCESS_MODE_QUALITY,
		Sky::PROCESS_MODE_INCREMENTAL,
		Sky::PROCESS_MODE_REALTIME
	};

	for (Sky::ProcessMode mode : process_modes) {
		test_sky->set_process_mode(mode);
		CHECK(test_sky->get_process_mode() == mode);
	}
}

TEST_CASE("[SceneTree][Sky] Radiance size variations") {
	Ref<Sky> test_sky;
	test_sky.instantiate();

	// Test all radiance sizes except MAX.
	const Sky::RadianceSize radiance_sizes[] = {
		Sky::RADIANCE_SIZE_32,
		Sky::RADIANCE_SIZE_64,
		Sky::RADIANCE_SIZE_128,
		Sky::RADIANCE_SIZE_256,
		Sky::RADIANCE_SIZE_512,
		Sky::RADIANCE_SIZE_1024,
		Sky::RADIANCE_SIZE_2048
	};

	for (Sky::RadianceSize size : radiance_sizes) {
		test_sky->set_radiance_size(size);
		CHECK(test_sky->get_radiance_size() == size);
	}
}

TEST_CASE("[SceneTree][Sky] Null material handling") {
	Ref<Sky> test_sky;
	test_sky.instantiate();

	SUBCASE("Setting null material") {
		test_sky->set_material(Ref<Material>());
		CHECK(test_sky->get_material().is_null());
	}

	SUBCASE("Overwriting existing material with null") {
		Ref<Material> material;
		material.instantiate();
		test_sky->set_material(material);
		test_sky->set_material(Ref<Material>());

		CHECK(test_sky->get_material().is_null());
	}
}

TEST_CASE("[SceneTree][Sky] RID generation") {
	Ref<Sky> test_sky;
	test_sky.instantiate();
	// Check validity.
	CHECK(!test_sky->get_rid().is_valid());
}

} // namespace TestSky

#endif // TEST_SKY_H
