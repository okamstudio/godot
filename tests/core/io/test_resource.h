/**************************************************************************/
/*  test_resource.h                                                       */
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

#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"

#include "thirdparty/doctest/doctest.h"

#include "tests/test_macros.h"

namespace TestResource {

TEST_CASE("[Resource] Duplication") {
	Ref<Resource> resource = memnew(Resource);
	resource->set_name("Hello world");
	Ref<Resource> child_resource = memnew(Resource);
	child_resource->set_name("I'm a child resource");
	resource->set_meta("other_resource", child_resource);

	Ref<Resource> resource_dupe = resource->duplicate();
	const Ref<Resource> &resource_dupe_reference = resource_dupe;
	resource_dupe->set_name("Changed name");
	child_resource->set_name("My name was changed too");

	CHECK_MESSAGE(
			resource_dupe->get_name() == "Changed name",
			"Duplicated resource should have the new name.");
	CHECK_MESSAGE(
			resource_dupe_reference->get_name() == "Changed name",
			"Reference to the duplicated resource should have the new name.");
	CHECK_MESSAGE(
			resource->get_name() == "Hello world",
			"Original resource name should not be affected after editing the duplicate's name.");
	CHECK_MESSAGE(
			Ref<Resource>(resource_dupe->get_meta("other_resource"))->get_name() == "My name was changed too",
			"Duplicated resource should share its child resource with the original.");
}

TEST_CASE("[Resource] Saving and loading") {
	Ref<Resource> resource = memnew(Resource);
	resource->set_name("Hello world");
	resource->set_meta("ExampleMetadata", Vector2i(40, 80));
	resource->set_meta("string", "The\nstring\nwith\nunnecessary\nline\n\t\\\nbreaks");
	Ref<Resource> child_resource = memnew(Resource);
	child_resource->set_name("I'm a child resource");
	resource->set_meta("other_resource", child_resource);
	const String save_path_binary = TestUtils::get_temp_path("resource.res");
	const String save_path_text = TestUtils::get_temp_path("resource.tres");
	ResourceSaver::save(resource, save_path_binary);
	ResourceSaver::save(resource, save_path_text);

	const Ref<Resource> &loaded_resource_binary = ResourceLoader::load(save_path_binary);
	CHECK_MESSAGE(
			loaded_resource_binary->get_name() == "Hello world",
			"The loaded resource name should be equal to the expected value.");
	CHECK_MESSAGE(
			loaded_resource_binary->get_meta("ExampleMetadata") == Vector2i(40, 80),
			"The loaded resource metadata should be equal to the expected value.");
	CHECK_MESSAGE(
			loaded_resource_binary->get_meta("string") == "The\nstring\nwith\nunnecessary\nline\n\t\\\nbreaks",
			"The loaded resource metadata should be equal to the expected value.");
	const Ref<Resource> &loaded_child_resource_binary = loaded_resource_binary->get_meta("other_resource");
	CHECK_MESSAGE(
			loaded_child_resource_binary->get_name() == "I'm a child resource",
			"The loaded child resource name should be equal to the expected value.");

	const Ref<Resource> &loaded_resource_text = ResourceLoader::load(save_path_text);
	CHECK_MESSAGE(
			loaded_resource_text->get_name() == "Hello world",
			"The loaded resource name should be equal to the expected value.");
	CHECK_MESSAGE(
			loaded_resource_text->get_meta("ExampleMetadata") == Vector2i(40, 80),
			"The loaded resource metadata should be equal to the expected value.");
	CHECK_MESSAGE(
			loaded_resource_text->get_meta("string") == "The\nstring\nwith\nunnecessary\nline\n\t\\\nbreaks",
			"The loaded resource metadata should be equal to the expected value.");
	const Ref<Resource> &loaded_child_resource_text = loaded_resource_text->get_meta("other_resource");
	CHECK_MESSAGE(
			loaded_child_resource_text->get_name() == "I'm a child resource",
			"The loaded child resource name should be equal to the expected value.");
}

TEST_CASE("[Resource] Breaking circular references on save") {
	Ref<Resource> resource_a = memnew(Resource);
	resource_a->set_name("A");
	Ref<Resource> resource_b = memnew(Resource);
	resource_b->set_name("B");
	Ref<Resource> resource_c = memnew(Resource);
	resource_c->set_name("C");
	resource_a->set_meta("next", resource_b);
	resource_b->set_meta("next", resource_c);
	resource_c->set_meta("next", resource_b);

	const String save_path_binary = TestUtils::get_temp_path("resource.res");
	const String save_path_text = TestUtils::get_temp_path("resource.tres");
	ResourceSaver::save(resource_a, save_path_binary);
	// Suppress expected errors caused by the resources above being uncached.
	ERR_PRINT_OFF;
	ResourceSaver::save(resource_a, save_path_text);

	const Ref<Resource> &loaded_resource_a_binary = ResourceLoader::load(save_path_binary);
	ERR_PRINT_ON;
	CHECK_MESSAGE(
			loaded_resource_a_binary->get_name() == "A",
			"The loaded resource name should be equal to the expected value.");
	const Ref<Resource> &loaded_resource_b_binary = loaded_resource_a_binary->get_meta("next");
	CHECK_MESSAGE(
			loaded_resource_b_binary->get_name() == "B",
			"The loaded child resource name should be equal to the expected value.");
	const Ref<Resource> &loaded_resource_c_binary = loaded_resource_b_binary->get_meta("next");
	CHECK_MESSAGE(
			loaded_resource_c_binary->get_name() == "C",
			"The loaded child resource name should be equal to the expected value.");
	CHECK_MESSAGE(
			!loaded_resource_c_binary->has_meta("next"),
			"The loaded child resource circular reference should be NULL.");

	const Ref<Resource> &loaded_resource_a_text = ResourceLoader::load(save_path_text);
	CHECK_MESSAGE(
			loaded_resource_a_text->get_name() == "A",
			"The loaded resource name should be equal to the expected value.");
	const Ref<Resource> &loaded_resource_b_text = loaded_resource_a_text->get_meta("next");
	CHECK_MESSAGE(
			loaded_resource_b_text->get_name() == "B",
			"The loaded child resource name should be equal to the expected value.");
	const Ref<Resource> &loaded_resource_c_text = loaded_resource_b_text->get_meta("next");
	CHECK_MESSAGE(
			loaded_resource_c_text->get_name() == "C",
			"The loaded child resource name should be equal to the expected value.");
	CHECK_MESSAGE(
			!loaded_resource_c_text->has_meta("next"),
			"The loaded child resource circular reference should be NULL.");

	// Break circular reference to avoid memory leak
	resource_c->remove_meta("next");
}
} // namespace TestResource
