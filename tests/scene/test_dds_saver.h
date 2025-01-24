/**************************************************************************/
/*  test_dds_saver.h                                                      */
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

#ifndef TEST_DDS_SAVER_H
#define TEST_DDS_SAVER_H

#include "modules/dds/image_saver_dds.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/image.h"
#include "tests/core/config/test_project_settings.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace TestDDSSaver {
void init(const String &p_test, const String &p_copy_target = String()) {
	Error err;
	// Setup project settings since it's needed for the import process.
	String project_folder = TestUtils::get_temp_path(p_test.get_file().get_basename());
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(project_folder.path_join(".godot").path_join("imported"));
	// Initialize res:// to `project_folder`.
	TestProjectSettingsInternalsAccessor::resource_path() = project_folder;
	err = ProjectSettings::get_singleton()->setup(project_folder, String(), true);

	if (p_copy_target.is_empty()) {
		return;
	}

	// Copy all the necessary test data files to the res:// directory.
	da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String test_data = String("tests/data").path_join(p_test);
	da = DirAccess::open(test_data);
	CHECK_MESSAGE(da.is_valid(), "Unable to open folder.");
	da->list_dir_begin();
	for (String item = da->get_next(); !item.is_empty(); item = da->get_next()) {
		if (!FileAccess::exists(test_data.path_join(item))) {
			continue;
		}
		Ref<FileAccess> output = FileAccess::open(p_copy_target.path_join(item), FileAccess::WRITE, &err);
		CHECK_MESSAGE(err == OK, "Unable to open output file.");
		output->store_buffer(FileAccess::get_file_as_bytes(test_data.path_join(item)));
		output->close();
	}
	da->list_dir_end();
}

TEST_CASE("[SceneTree][DDSSaver] Save DDS - Save valid image with mipmap" * doctest::skip(true)) {
	init("save_dds_valid_image_with_mipmap");
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 0)); // Fill with red color
	image->generate_mipmaps();
	image->compress_from_channels(Image::COMPRESS_S3TC, Image::USED_CHANNELS_RGBA);
	Error err = save_dds("res://valid_image_with_mipmap.dds", image);
	CHECK(err == OK);

	Ref<Image> loaded_image;
	loaded_image.instantiate();
	Vector<uint8_t> buffer = FileAccess::get_file_as_bytes("res://valid_image_with_mipmap.dds", &err);
	CHECK(err == OK);
	err = loaded_image->load_dds_from_buffer(buffer);
	CHECK(err == OK);
	Dictionary metrics = image->compute_image_metrics(loaded_image, false);
	CHECK(metrics.size() > 0);
	CHECK_MESSAGE(metrics.has("root_mean_squared"), "Metrics dictionary contains 'root_mean_squared'.");
	float rms = metrics["root_mean_squared"];
	CHECK(rms == 0.0f);
}

TEST_CASE("[SceneTree][DDSSaver] Save DDS - Save valid image with BPTC and S3TC compression" * doctest::skip(true)) {
	init("save_dds_valid_image_bptc_s3tc");
	Ref<Image> image_bptc = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image_bptc->fill(Color(0, 0, 1)); // Fill with blue color
	image_bptc->compress_from_channels(Image::COMPRESS_BPTC, Image::USED_CHANNELS_RGBA);
	Error err_bptc = image_bptc->save_dds("res://valid_image_bptc.dds");
	CHECK(err_bptc == OK);

	Ref<Image> image_s3tc = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image_s3tc->fill(Color(1, 1, 1)); // Fill with white color
	image_s3tc->compress_from_channels(Image::COMPRESS_S3TC, Image::USED_CHANNELS_RGBA);
	Error err_s3tc = image_s3tc->save_dds("res://valid_image_s3tc_combined.dds");
	CHECK(err_s3tc == OK);

	// Validate BPTC image
	Ref<Image> loaded_image_bptc;
	loaded_image_bptc.instantiate();
	Vector<uint8_t> buffer_bptc = FileAccess::get_file_as_bytes("res://valid_image_bptc.dds", &err_bptc);
	CHECK(err_bptc == OK);
	err_bptc = loaded_image_bptc->load_dds_from_buffer(buffer_bptc);
	CHECK(err_bptc == OK);
	Dictionary metrics_bptc = image_bptc->compute_image_metrics(loaded_image_bptc, false);
	CHECK(metrics_bptc.size() > 0);
	CHECK_MESSAGE(metrics_bptc.has("root_mean_squared"), "Metrics dictionary contains 'root_mean_squared' for BPTC.");
	float rms_bptc = metrics_bptc["root_mean_squared"];
	CHECK(rms_bptc == 0.0f);

	// Validate S3TC image
	Ref<Image> loaded_image_s3tc;
	loaded_image_s3tc.instantiate();
	Vector<uint8_t> buffer_s3tc = FileAccess::get_file_as_bytes("res://valid_image_s3tc_combined.dds", &err_s3tc);
	CHECK(err_s3tc == OK);
	err_s3tc = loaded_image_s3tc->load_dds_from_buffer(buffer_s3tc);
	CHECK(err_s3tc == OK);
	Dictionary metrics_s3tc = image_s3tc->compute_image_metrics(loaded_image_s3tc, false);
	CHECK(metrics_s3tc.size() > 0);
	CHECK_MESSAGE(metrics_s3tc.has("root_mean_squared"), "Metrics dictionary contains 'root_mean_squared' for S3TC.");
	float rms_s3tc = metrics_s3tc["root_mean_squared"];
	CHECK(rms_s3tc == 0.0f);
}

TEST_CASE("[SceneTree][DDSSaver] Save DDS - Save valid uncompressed image") {
	init("save_dds_valid_uncompressed");
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image->fill(Color(0, 0, 1)); // Fill with blue color
	Error err = image->save_dds("res://valid_image_uncompressed.dds");
	CHECK(err == OK);
	Vector<uint8_t> buffer = FileAccess::get_file_as_bytes("res://valid_image_uncompressed.dds", &err);
	CHECK(err == OK);
	Ref<Image> loaded_image;
	loaded_image.instantiate();
	err = loaded_image->load_dds_from_buffer(buffer);
	CHECK(err == OK);
	Dictionary metrics = image->compute_image_metrics(loaded_image, false);
	CHECK(metrics.size() > 0);
	CHECK_MESSAGE(metrics.has("root_mean_squared"), "Metrics dictionary contains 'root_mean_squared' for uncompressed.");
	float rms = metrics["root_mean_squared"];
	CHECK(rms == 0.0f);
}
} //namespace TestDDSSaver

#endif // TEST_DDS_SAVER_H
