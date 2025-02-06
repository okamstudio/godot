/**************************************************************************/
/*  gradle_export_util.h                                                  */
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

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/zip_io.h"
#include "core/os/os.h"
#include "editor/export/editor_export.h"

const String GODOT_PROJECT_NAME_XML_STRING = R"(<?xml version="1.0" encoding="utf-8"?>
<!--WARNING: THIS FILE WILL BE OVERWRITTEN AT BUILD TIME-->
<resources>
	<string name="godot_project_name_string">%s</string>
</resources>
)";

// Application category.
// See https://developer.android.com/guide/topics/manifest/application-element#appCategory for standards
static const int APP_CATEGORY_ACCESSIBILITY = 0;
static const int APP_CATEGORY_AUDIO = 1;
static const int APP_CATEGORY_GAME = 2;
static const int APP_CATEGORY_IMAGE = 3;
static const int APP_CATEGORY_MAPS = 4;
static const int APP_CATEGORY_NEWS = 5;
static const int APP_CATEGORY_PRODUCTIVITY = 6;
static const int APP_CATEGORY_SOCIAL = 7;
static const int APP_CATEGORY_VIDEO = 8;
static const int APP_CATEGORY_UNDEFINED = 9;

// Supported XR modes.
// This should match the entries in 'platform/android/java/lib/src/org/godotengine/godot/xr/XRMode.java'
static const int XR_MODE_REGULAR = 0;
static const int XR_MODE_OPENXR = 1;

struct CustomExportData {
	String assets_directory;
	String libs_directory;
	bool debug;
	Vector<String> libs;
};

int _get_android_orientation_value(DisplayServer::ScreenOrientation screen_orientation);

String _get_android_orientation_label(DisplayServer::ScreenOrientation screen_orientation);

int _get_app_category_value(int category_index);

String _get_app_category_label(int category_index);

// Utility method used to create a directory.
Error create_directory(const String &p_dir);

// Writes p_data into a file at p_path, creating directories if necessary.
// Note: this will overwrite the file at p_path if it already exists.
Error store_file_at_path(const String &p_path, const Vector<uint8_t> &p_data);

// Writes string p_data into a file at p_path, creating directories if necessary.
// Note: this will overwrite the file at p_path if it already exists.
Error store_string_at_path(const String &p_path, const String &p_data);

// Implementation of EditorExportSaveFunction.
// This method will only be called as an input to export_project_files.
// It is used by the export_project_files method to save all the asset files into the gradle project.
// It's functionality mirrors that of the method save_apk_file.
// This method will be called ONLY when gradle build is enabled.
Error rename_and_store_file_in_gradle_project(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key, uint64_t p_seed);

// Creates strings.xml files inside the gradle project for different locales.
Error _create_project_name_strings_files(const Ref<EditorExportPreset> &p_preset, const String &project_name, const String &p_gradle_build_dir);

String bool_to_string(bool v);

String _get_gles_tag();

String _get_screen_sizes_tag(const Ref<EditorExportPreset> &p_preset);

String _get_activity_tag(const Ref<EditorExportPlatform> &p_export_platform, const Ref<EditorExportPreset> &p_preset, bool p_debug);

String _get_application_tag(const Ref<EditorExportPlatform> &p_export_platform, const Ref<EditorExportPreset> &p_preset, bool p_has_read_write_storage_permission, bool p_debug);
