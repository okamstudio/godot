/**************************************************************************/
/*  project_converter_3_to_4.cpp                                          */
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

#include "project_converter_3_to_4.h"

#ifndef DISABLE_DEPRECATED

#include "modules/modules_enabled.gen.h" // For regex.

#ifdef MODULE_REGEX_ENABLED

#include "core/error/error_macros.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/ref_counted.h"
#include "core/os/time.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "editor/renames_map_3_to_4.h"
#include "modules/regex/regex.h"

// Find "OS.set_property(x)", capturing x into $1.
static String make_regex_gds_os_property_set(const String &name_set) {
	return String("\\bOS\\.") + name_set + "\\s*\\((.*)\\)";
}
// Find "OS.property = x", capturing x into $1 or $2.
static String make_regex_gds_os_property_assign(const String &name) {
	return String("\\bOS\\.") + name + "\\s*=\\s*([^#]+)";
}
// Find "OS.property" OR "OS.get_property()" / "OS.is_property()".
static String make_regex_gds_os_property_get(const String &name, const String &get) {
	return String("\\bOS\\.(") + get + "_)?" + name + "(\\s*\\(\\s*\\))?";
}

class ProjectConverter3To4::RegExContainer {
public:
	// Custom GDScript.
	RegEx reg_is_empty = RegEx("\\bempty\\(");
	RegEx reg_super = RegEx("([\t ])\\.([a-zA-Z_])");
	RegEx reg_json_to = RegEx("\\bto_json\\b");
	RegEx reg_json_parse = RegEx("([\t ]{0,})([^\n]+)parse_json\\(([^\n]+)");
	RegEx reg_json_non_new = RegEx("([\t ]{0,})([^\n]+)JSON\\.parse\\(([^\n]+)");
	RegEx reg_json_print = RegEx("\\bJSON\\b\\.print\\(");
	RegEx reg_export_simple = RegEx("export[ ]*\\(([a-zA-Z0-9_]+)\\)[ ]*var[ ]+([a-zA-Z0-9_]+)");
	RegEx reg_export_typed = RegEx("export[ ]*\\(([a-zA-Z0-9_]+)\\)[ ]*var[ ]+([a-zA-Z0-9_]+)[ ]*:[ ]*[a-zA-Z0-9_]+");
	RegEx reg_export_inferred_type = RegEx("export[ ]*\\([a-zA-Z0-9_]+\\)[ ]*var[ ]+([a-zA-Z0-9_]+)[ ]*:[ ]*=");
	RegEx reg_export_advanced = RegEx("export[ ]*\\(([^)^\n]+)\\)[ ]*var[ ]+([a-zA-Z0-9_]+)([^\n]+)");
	RegEx reg_setget_setget = RegEx("var[ ]+([a-zA-Z0-9_]+)([^\n]+?)[ \t]*setget[ \t]+([a-zA-Z0-9_]+)[ \t]*,[ \t]*([a-zA-Z0-9_]+)");
	RegEx reg_setget_set = RegEx("var[ ]+([a-zA-Z0-9_]+)([^\n]+?)[ \t]*setget[ \t]+([a-zA-Z0-9_]+)[ \t]*[,]*[^\n]*$");
	RegEx reg_setget_get = RegEx("var[ ]+([a-zA-Z0-9_]+)([^\n]+?)[ \t]*setget[ \t]+,[ \t]*([a-zA-Z0-9_]+)[ \t]*$");
	RegEx reg_join = RegEx("([\\(\\)a-zA-Z0-9_]+)\\.join\\(([^\n^\\)]+)\\)");
	RegEx reg_image_lock = RegEx("([a-zA-Z0-9_\\.]+)\\.lock\\(\\)");
	RegEx reg_image_unlock = RegEx("([a-zA-Z0-9_\\.]+)\\.unlock\\(\\)");
	RegEx reg_instantiate = RegEx("\\.instance\\(([^\\)]*)\\)");
	// Simple OS properties with getters/setters.
	RegEx reg_os_current_screen = RegEx("\\bOS\\.((set_|get_)?)current_screen\\b");
	RegEx reg_os_min_window_size = RegEx("\\bOS\\.((set_|get_)?)min_window_size\\b");
	RegEx reg_os_max_window_size = RegEx("\\bOS\\.((set_|get_)?)max_window_size\\b");
	RegEx reg_os_window_position = RegEx("\\bOS\\.((set_|get_)?)window_position\\b");
	RegEx reg_os_window_size = RegEx("\\bOS\\.((set_|get_)?)window_size\\b");
	RegEx reg_os_getset_screen_orient = RegEx("\\bOS\\.(s|g)et_screen_orientation\\b");
	// OS property getters/setters for non trivial replacements.
	RegEx reg_os_set_window_resizable = RegEx(make_regex_gds_os_property_set("set_window_resizable"));
	RegEx reg_os_assign_window_resizable = RegEx(make_regex_gds_os_property_assign("window_resizable"));
	RegEx reg_os_is_window_resizable = RegEx(make_regex_gds_os_property_get("window_resizable", "is"));
	RegEx reg_os_set_fullscreen = RegEx(make_regex_gds_os_property_set("set_window_fullscreen"));
	RegEx reg_os_assign_fullscreen = RegEx(make_regex_gds_os_property_assign("window_fullscreen"));
	RegEx reg_os_is_fullscreen = RegEx(make_regex_gds_os_property_get("window_fullscreen", "is"));
	RegEx reg_os_set_maximized = RegEx(make_regex_gds_os_property_set("set_window_maximized"));
	RegEx reg_os_assign_maximized = RegEx(make_regex_gds_os_property_assign("window_maximized"));
	RegEx reg_os_is_maximized = RegEx(make_regex_gds_os_property_get("window_maximized", "is"));
	RegEx reg_os_set_minimized = RegEx(make_regex_gds_os_property_set("set_window_minimized"));
	RegEx reg_os_assign_minimized = RegEx(make_regex_gds_os_property_assign("window_minimized"));
	RegEx reg_os_is_minimized = RegEx(make_regex_gds_os_property_get("window_minimized", "is"));
	RegEx reg_os_set_vsync = RegEx(make_regex_gds_os_property_set("set_use_vsync"));
	RegEx reg_os_assign_vsync = RegEx(make_regex_gds_os_property_assign("vsync_enabled"));
	RegEx reg_os_is_vsync = RegEx(make_regex_gds_os_property_get("vsync_enabled", "is"));
	// OS properties specific cases & specific replacements.
	RegEx reg_os_assign_screen_orient = RegEx("^(\\s*)OS\\.screen_orientation\\s*=\\s*([^#]+)"); // $1 - indent, $2 - value
	RegEx reg_os_set_always_on_top = RegEx(make_regex_gds_os_property_set("set_window_always_on_top"));
	RegEx reg_os_is_always_on_top = RegEx("\\bOS\\.is_window_always_on_top\\s*\\(.*\\)");
	RegEx reg_os_set_borderless = RegEx(make_regex_gds_os_property_set("set_borderless_window"));
	RegEx reg_os_get_borderless = RegEx("\\bOS\\.get_borderless_window\\s*\\(\\s*\\)");
	RegEx reg_os_screen_orient_enum = RegEx("\\bOS\\.SCREEN_ORIENTATION_(\\w+)\\b"); // $1 - constant suffix

	// GDScript keywords.
	RegEx keyword_gdscript_tool = RegEx("^tool");
	RegEx keyword_gdscript_export_single = RegEx("^export");
	RegEx keyword_gdscript_export_multi = RegEx("([\t]+)export\\b");
	RegEx keyword_gdscript_onready = RegEx("^onready");
	RegEx keyword_gdscript_remote = RegEx("^remote func");
	RegEx keyword_gdscript_remotesync = RegEx("^remotesync func");
	RegEx keyword_gdscript_sync = RegEx("^sync func");
	RegEx keyword_gdscript_slave = RegEx("^slave func");
	RegEx keyword_gdscript_puppet = RegEx("^puppet func");
	RegEx keyword_gdscript_puppetsync = RegEx("^puppetsync func");
	RegEx keyword_gdscript_master = RegEx("^master func");
	RegEx keyword_gdscript_mastersync = RegEx("^mastersync func");

	RegEx gdscript_comment = RegEx("^\\s*#");
	RegEx csharp_comment = RegEx("^\\s*\\/\\/");

	// CSharp keywords.
	RegEx keyword_csharp_remote = RegEx("\\[Remote(Attribute)?(\\(\\))?\\]");
	RegEx keyword_csharp_remotesync = RegEx("\\[(Remote)?Sync(Attribute)?(\\(\\))?\\]");
	RegEx keyword_csharp_puppet = RegEx("\\[(Puppet|Slave)(Attribute)?(\\(\\))?\\]");
	RegEx keyword_csharp_puppetsync = RegEx("\\[PuppetSync(Attribute)?(\\(\\))?\\]");
	RegEx keyword_csharp_master = RegEx("\\[Master(Attribute)?(\\(\\))?\\]");
	RegEx keyword_csharp_mastersync = RegEx("\\[MasterSync(Attribute)?(\\(\\))?\\]");

	// Colors.
	LocalVector<RegEx *> color_regexes;
	LocalVector<String> color_renamed;

	RegEx color_hexadecimal_short_constructor = RegEx("Color\\(\"#?([a-fA-F0-9]{1})([a-fA-F0-9]{3})\\b");
	RegEx color_hexadecimal_full_constructor = RegEx("Color\\(\"#?([a-fA-F0-9]{2})([a-fA-F0-9]{6})\\b");

	// Classes.
	LocalVector<RegEx *> class_tscn_regexes;
	LocalVector<RegEx *> class_gd_regexes;
	LocalVector<RegEx *> class_shader_regexes;

	// Keycode.
	RegEx input_map_keycode = RegEx("\\b,\"((physical_)?)scancode\":(\\d+)\\b");

	// Button index and joypad axis.
	RegEx joypad_button_index = RegEx("\\b,\"button_index\":(\\d+),(\"pressure\":\\d+\\.\\d+,\"pressed\":(false|true))\\b");
	RegEx joypad_axis = RegEx("\\b,\"axis\":(\\d+)\\b");

	// Index represents Godot 3's value, entry represents Godot 4 value equivalency.
	// i.e: Button4(L1 - Godot3) -> joypad_button_mappings[4]=9 -> Button9(L1 - Godot4).
	int joypad_button_mappings[23] = { 0, 1, 2, 3, 9, 10, -1 /*L2*/, -1 /*R2*/, 7, 8, 4, 6, 11, 12, 13, 14, 5, 15, 16, 17, 18, 19, 20 };
	// Entries for L2 and R2 are -1 since they match to joypad axes and no longer to joypad buttons in Godot 4.

	LocalVector<RegEx *> class_regexes;

	RegEx class_temp_tscn = RegEx("\\bTEMP_RENAMED_CLASS.tscn\\b");
	RegEx class_temp_gd = RegEx("\\bTEMP_RENAMED_CLASS.gd\\b");
	RegEx class_temp_shader = RegEx("\\bTEMP_RENAMED_CLASS.shader\\b");

	LocalVector<String> class_temp_tscn_renames;
	LocalVector<String> class_temp_gd_renames;
	LocalVector<String> class_temp_shader_renames;

	// Common.
	LocalVector<RegEx *> enum_regexes;
	LocalVector<RegEx *> gdscript_function_regexes;
	LocalVector<RegEx *> project_settings_regexes;
	LocalVector<RegEx *> project_godot_regexes;
	LocalVector<RegEx *> input_map_regexes;
	LocalVector<RegEx *> gdscript_properties_regexes;
	LocalVector<RegEx *> gdscript_signals_regexes;
	LocalVector<RegEx *> shaders_regexes;
	LocalVector<RegEx *> builtin_types_regexes;
	LocalVector<RegEx *> theme_override_regexes;
	LocalVector<RegEx *> csharp_function_regexes;
	LocalVector<RegEx *> csharp_properties_regexes;
	LocalVector<RegEx *> csharp_signal_regexes;

	RegExContainer() {
		// Common.
		{
			// Enum.
			for (unsigned int current_index = 0; RenamesMap3To4::enum_renames[current_index][0]; current_index++) {
				enum_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::enum_renames[current_index][0] + "\\b")));
			}
			// GDScript functions.
			for (unsigned int current_index = 0; RenamesMap3To4::gdscript_function_renames[current_index][0]; current_index++) {
				gdscript_function_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::gdscript_function_renames[current_index][0] + "\\b")));
			}
			// Project Settings in scripts.
			for (unsigned int current_index = 0; RenamesMap3To4::project_settings_renames[current_index][0]; current_index++) {
				project_settings_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::project_settings_renames[current_index][0] + "\\b")));
			}
			// Project Settings in project.godot.
			for (unsigned int current_index = 0; RenamesMap3To4::project_godot_renames[current_index][0]; current_index++) {
				project_godot_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::project_godot_renames[current_index][0] + "\\b")));
			}
			// Input Map.
			for (unsigned int current_index = 0; RenamesMap3To4::input_map_renames[current_index][0]; current_index++) {
				input_map_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::input_map_renames[current_index][0] + "\\b")));
			}
			// GDScript properties.
			for (unsigned int current_index = 0; RenamesMap3To4::gdscript_properties_renames[current_index][0]; current_index++) {
				gdscript_properties_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::gdscript_properties_renames[current_index][0] + "\\b")));
			}
			// GDScript Signals.
			for (unsigned int current_index = 0; RenamesMap3To4::gdscript_signals_renames[current_index][0]; current_index++) {
				gdscript_signals_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::gdscript_signals_renames[current_index][0] + "\\b")));
			}
			// Shaders.
			for (unsigned int current_index = 0; RenamesMap3To4::shaders_renames[current_index][0]; current_index++) {
				shaders_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::shaders_renames[current_index][0] + "\\b")));
			}
			// Builtin types.
			for (unsigned int current_index = 0; RenamesMap3To4::builtin_types_renames[current_index][0]; current_index++) {
				builtin_types_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::builtin_types_renames[current_index][0] + "\\b")));
			}
			// Theme overrides.
			for (unsigned int current_index = 0; RenamesMap3To4::theme_override_renames[current_index][0]; current_index++) {
				theme_override_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::theme_override_renames[current_index][0] + "\\b")));
			}
			// CSharp function renames.
			for (unsigned int current_index = 0; RenamesMap3To4::csharp_function_renames[current_index][0]; current_index++) {
				csharp_function_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::csharp_function_renames[current_index][0] + "\\b")));
			}
			// CSharp properties renames.
			for (unsigned int current_index = 0; RenamesMap3To4::csharp_properties_renames[current_index][0]; current_index++) {
				csharp_properties_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::csharp_properties_renames[current_index][0] + "\\b")));
			}
			// CSharp signals renames.
			for (unsigned int current_index = 0; RenamesMap3To4::csharp_signals_renames[current_index][0]; current_index++) {
				csharp_signal_regexes.push_back(memnew(RegEx(String("\\b") + RenamesMap3To4::csharp_signals_renames[current_index][0] + "\\b")));
			}
		}

		// Colors.
		{
			for (unsigned int current_index = 0; RenamesMap3To4::color_renames[current_index][0]; current_index++) {
				color_regexes.push_back(memnew(RegEx(String("\\bColor.") + RenamesMap3To4::color_renames[current_index][0] + "\\b")));
				color_renamed.push_back(String("Color.") + RenamesMap3To4::color_renames[current_index][1]);
			}
		}
		// Classes.
		{
			for (unsigned int current_index = 0; RenamesMap3To4::class_renames[current_index][0]; current_index++) {
				const String class_name = RenamesMap3To4::class_renames[current_index][0];
				class_tscn_regexes.push_back(memnew(RegEx(String("\\b") + class_name + ".tscn\\b")));
				class_gd_regexes.push_back(memnew(RegEx(String("\\b") + class_name + ".gd\\b")));
				class_shader_regexes.push_back(memnew(RegEx(String("\\b") + class_name + ".shader\\b")));

				class_regexes.push_back(memnew(RegEx(String("\\b") + class_name + "\\b")));

				class_temp_tscn_renames.push_back(class_name + ".tscn");
				class_temp_gd_renames.push_back(class_name + ".gd");
				class_temp_shader_renames.push_back(class_name + ".shader");
			}
		}
	}
	~RegExContainer() {
		for (RegEx *regex : color_regexes) {
			memdelete(regex);
		}
		for (unsigned int i = 0; i < class_tscn_regexes.size(); i++) {
			memdelete(class_tscn_regexes[i]);
			memdelete(class_gd_regexes[i]);
			memdelete(class_shader_regexes[i]);
			memdelete(class_regexes[i]);
		}
		for (RegEx *regex : enum_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : gdscript_function_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : project_settings_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : project_godot_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : input_map_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : gdscript_properties_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : gdscript_signals_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : shaders_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : builtin_types_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : theme_override_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : csharp_function_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : csharp_properties_regexes) {
			memdelete(regex);
		}
		for (RegEx *regex : csharp_signal_regexes) {
			memdelete(regex);
		}
	}
};

ProjectConverter3To4::ProjectConverter3To4(int p_maximum_file_size_kb, int p_maximum_line_length) {
	maximum_file_size = p_maximum_file_size_kb * 1024;
	maximum_line_length = p_maximum_line_length;
}

// Function responsible for converting project.
bool ProjectConverter3To4::convert() {
	print_line("Starting conversion.");
	uint64_t conversion_start_time = Time::get_singleton()->get_ticks_msec();

	RegExContainer reg_container = RegExContainer();

	int cached_maximum_line_length = maximum_line_length;
	maximum_line_length = 10000; // Use only for tests bigger value, to not break them.

	ERR_FAIL_COND_V_MSG(!test_array_names(), false, "Cannot start converting due to problems with data in arrays.");
	ERR_FAIL_COND_V_MSG(!test_conversion(reg_container), false, "Aborting conversion due to validation tests failing");

	maximum_line_length = cached_maximum_line_length;

	// Checking if folder contains valid Godot 3 project.
	// Project should not be converted more than once.
	{
		String converter_text = "; Project was converted by built-in tool to Godot 4";

		ERR_FAIL_COND_V_MSG(!FileAccess::exists("project.godot"), false, "Current working directory doesn't contain a \"project.godot\" file for a Godot 3 project.");

		Error err = OK;
		String project_godot_content = FileAccess::get_file_as_string("project.godot", &err);

		ERR_FAIL_COND_V_MSG(err != OK, false, "Unable to read \"project.godot\".");
		ERR_FAIL_COND_V_MSG(project_godot_content.contains(converter_text), false, "Project was already converted with this tool.");

		Ref<FileAccess> file = FileAccess::open("project.godot", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(file.is_null(), false, "Unable to open \"project.godot\".");

		file->store_string(converter_text + "\n" + project_godot_content);
	}

	Vector<String> collected_files = check_for_files();

	uint32_t converted_files = 0;

	// Check file by file.
	for (int i = 0; i < collected_files.size(); i++) {
		String file_name = collected_files[i];
		Vector<SourceLine> source_lines;
		uint32_t ignored_lines = 0;
		{
			Ref<FileAccess> file = FileAccess::open(file_name, FileAccess::READ);
			ERR_CONTINUE_MSG(file.is_null(), vformat("Unable to read content of \"%s\".", file_name));
			while (!file->eof_reached()) {
				String line = file->get_line();

				SourceLine source_line;
				source_line.line = line;
				source_line.is_comment = reg_container.gdscript_comment.search_all(line).size() > 0 || reg_container.csharp_comment.search_all(line).size() > 0;
				source_lines.append(source_line);
			}
		}
		String file_content_before = collect_string_from_vector(source_lines);
		uint64_t hash_before = file_content_before.hash();
		uint64_t file_size = file_content_before.size();
		print_line(vformat("Trying to convert\t%d/%d file - \"%s\" with size - %d KB", i + 1, collected_files.size(), file_name.trim_prefix("res://"), file_size / 1024));

		Vector<String> reason;
		bool is_ignored = false;
		uint64_t start_time = Time::get_singleton()->get_ticks_msec();

		if (file_name.ends_with(".shader")) {
			DirAccess::remove_file_or_error(file_name.trim_prefix("res://"));
			file_name = file_name.replace(".shader", ".gdshader");
		}

		if (file_size < uint64_t(maximum_file_size)) {
			// ".tscn" must work exactly the same as ".gd" files because they may contain built-in Scripts.
			if (file_name.ends_with(".gd")) {
				fix_tool_declaration(source_lines, reg_container);

				rename_classes(source_lines, reg_container); // Using only specialized function.

				rename_common(RenamesMap3To4::enum_renames, reg_container.enum_regexes, source_lines);
				rename_colors(source_lines, reg_container); // Require to additional rename.

				rename_common(RenamesMap3To4::gdscript_function_renames, reg_container.gdscript_function_regexes, source_lines);
				rename_gdscript_functions(source_lines, reg_container, false); // Require to additional rename.

				rename_common(RenamesMap3To4::project_settings_renames, reg_container.project_settings_regexes, source_lines);
				rename_gdscript_keywords(source_lines, reg_container, false);
				rename_common(RenamesMap3To4::gdscript_properties_renames, reg_container.gdscript_properties_regexes, source_lines);
				rename_common(RenamesMap3To4::gdscript_signals_renames, reg_container.gdscript_signals_regexes, source_lines);
				rename_common(RenamesMap3To4::shaders_renames, reg_container.shaders_regexes, source_lines);
				rename_common(RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, source_lines);
				rename_common(RenamesMap3To4::theme_override_renames, reg_container.theme_override_regexes, source_lines);

				custom_rename(source_lines, "\\.shader", ".gdshader");

				convert_hexadecimal_colors(source_lines, reg_container);
			} else if (file_name.ends_with(".tscn")) {
				fix_pause_mode(source_lines, reg_container);

				rename_classes(source_lines, reg_container); // Using only specialized function.

				rename_common(RenamesMap3To4::enum_renames, reg_container.enum_regexes, source_lines);
				rename_colors(source_lines, reg_container); // Require to do additional renames.

				rename_common(RenamesMap3To4::gdscript_function_renames, reg_container.gdscript_function_regexes, source_lines);
				rename_gdscript_functions(source_lines, reg_container, true); // Require to do additional renames.

				rename_common(RenamesMap3To4::project_settings_renames, reg_container.project_settings_regexes, source_lines);
				rename_gdscript_keywords(source_lines, reg_container, true);
				rename_common(RenamesMap3To4::gdscript_properties_renames, reg_container.gdscript_properties_regexes, source_lines);
				rename_common(RenamesMap3To4::gdscript_signals_renames, reg_container.gdscript_signals_regexes, source_lines);
				rename_common(RenamesMap3To4::shaders_renames, reg_container.shaders_regexes, source_lines);
				rename_common(RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, source_lines);
				rename_common(RenamesMap3To4::theme_override_renames, reg_container.theme_override_regexes, source_lines);

				custom_rename(source_lines, "\\.shader", ".gdshader");

				convert_hexadecimal_colors(source_lines, reg_container);
			} else if (file_name.ends_with(".cs")) { // TODO, C# should use different methods.
				rename_classes(source_lines, reg_container); // Using only specialized function.
				rename_common(RenamesMap3To4::csharp_function_renames, reg_container.csharp_function_regexes, source_lines);
				rename_common(RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, source_lines);
				rename_common(RenamesMap3To4::csharp_properties_renames, reg_container.csharp_properties_regexes, source_lines);
				rename_common(RenamesMap3To4::csharp_signals_renames, reg_container.csharp_signal_regexes, source_lines);
				rename_csharp_functions(source_lines, reg_container);
				rename_csharp_attributes(source_lines, reg_container);
				custom_rename(source_lines, "public class ", "public partial class ");
				convert_hexadecimal_colors(source_lines, reg_container);
			} else if (file_name.ends_with(".gdshader") || file_name.ends_with(".shader")) {
				rename_common(RenamesMap3To4::shaders_renames, reg_container.shaders_regexes, source_lines);
			} else if (file_name.ends_with("tres")) {
				rename_classes(source_lines, reg_container); // Using only specialized function.

				rename_common(RenamesMap3To4::shaders_renames, reg_container.shaders_regexes, source_lines);
				rename_common(RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, source_lines);

				custom_rename(source_lines, "\\.shader", ".gdshader");
			} else if (file_name.ends_with("project.godot")) {
				rename_common(RenamesMap3To4::project_godot_renames, reg_container.project_godot_regexes, source_lines);
				rename_common(RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, source_lines);
				rename_input_map_scancode(source_lines, reg_container);
				rename_joypad_buttons_and_axes(source_lines, reg_container);
				rename_common(RenamesMap3To4::input_map_renames, reg_container.input_map_regexes, source_lines);
				custom_rename(source_lines, "config_version=4", "config_version=5");
			} else if (file_name.ends_with(".csproj")) {
				// TODO
			} else if (file_name.ends_with(".import")) {
				for (SourceLine &source_line : source_lines) {
					String &line = source_line.line;
					if (line.contains("nodes/root_type=\"Spatial\"")) {
						line = "nodes/root_type=\"Node3D\"";
					} else if (line == "importer=\"ogg_vorbis\"") {
						line = "importer=\"oggvorbisstr\"";
					}
				}
			} else {
				ERR_PRINT(file_name + " is not supported!");
				continue;
			}

			for (SourceLine &source_line : source_lines) {
				if (source_line.is_comment) {
					continue;
				}

				String &line = source_line.line;
				if (uint64_t(line.length()) > maximum_line_length) {
					ignored_lines += 1;
				}
			}
		} else {
			reason.append(vformat("    ERROR: File has exceeded the maximum size allowed - %d KB", maximum_file_size / 1024));
			is_ignored = true;
		}

		uint64_t end_time = Time::get_singleton()->get_ticks_msec();
		if (is_ignored) {
			String end_message = vformat("    Checking file took %d ms.", end_time - start_time);
			print_line(end_message);
		} else {
			String file_content_after = collect_string_from_vector(source_lines);
			uint64_t hash_after = file_content_after.hash64();
			// Don't need to save file without any changes.
			// Save if this is a shader, because it was renamed.
			if (hash_before != hash_after || file_name.ends_with(".gdshader")) {
				converted_files++;

				Ref<FileAccess> file = FileAccess::open(file_name, FileAccess::WRITE);
				ERR_CONTINUE_MSG(file.is_null(), vformat("Unable to apply changes to \"%s\", no writing access.", file_name));
				file->store_string(file_content_after);
				reason.append(vformat("    File was changed, conversion took %d ms.", end_time - start_time));
			} else {
				reason.append(vformat("    File was left unchanged, checking took %d ms.", end_time - start_time));
			}
			if (ignored_lines != 0) {
				reason.append(vformat("    Ignored %d lines, because their length exceeds maximum allowed characters - %d.", ignored_lines, maximum_line_length));
			}
		}
		for (int k = 0; k < reason.size(); k++) {
			print_line(reason[k]);
		}
	}
	print_line(vformat("Conversion ended - all files(%d), converted files: (%d), not converted files: (%d).", collected_files.size(), converted_files, collected_files.size() - converted_files));
	uint64_t conversion_end_time = Time::get_singleton()->get_ticks_msec();
	print_line(vformat("Conversion of all files took %10.3f seconds.", (conversion_end_time - conversion_start_time) / 1000.0));
	return true;
}

// Function responsible for validating project conversion.
bool ProjectConverter3To4::validate_conversion() {
	print_line("Starting checking if project conversion can be done.");
	uint64_t conversion_start_time = Time::get_singleton()->get_ticks_msec();

	RegExContainer reg_container = RegExContainer();

	int cached_maximum_line_length = maximum_line_length;
	maximum_line_length = 10000; // To avoid breaking the tests, only use this for the their larger value.

	ERR_FAIL_COND_V_MSG(!test_array_names(), false, "Cannot start converting due to problems with data in arrays.");
	ERR_FAIL_COND_V_MSG(!test_conversion(reg_container), false, "Aborting conversion due to validation tests failing");

	maximum_line_length = cached_maximum_line_length;

	// Checking if folder contains valid Godot 3 project.
	// Project should not be converted more than once.
	{
		String conventer_text = "; Project was converted by built-in tool to Godot 4";

		ERR_FAIL_COND_V_MSG(!FileAccess::exists("project.godot"), false, "Current directory doesn't contain any Godot 3 project");

		Error err = OK;
		String project_godot_content = FileAccess::get_file_as_string("project.godot", &err);

		ERR_FAIL_COND_V_MSG(err != OK, false, "Failed to read content of \"project.godot\" file.");
		ERR_FAIL_COND_V_MSG(project_godot_content.contains(conventer_text), false, "Project already was converted with this tool.");
	}

	Vector<String> collected_files = check_for_files();

	uint32_t converted_files = 0;

	// Check file by file.
	for (int i = 0; i < collected_files.size(); i++) {
		const String &file_name = collected_files[i];
		Vector<String> lines;
		uint32_t ignored_lines = 0;
		uint64_t file_size = 0;
		{
			Ref<FileAccess> file = FileAccess::open(file_name, FileAccess::READ);
			ERR_CONTINUE_MSG(file.is_null(), vformat("Unable to read content of \"%s\".", file_name));
			while (!file->eof_reached()) {
				String line = file->get_line();
				file_size += line.size();
				lines.append(line);
			}
		}
		print_line(vformat("Checking for conversion - %d/%d file - \"%s\" with size - %d KB", i + 1, collected_files.size(), file_name.trim_prefix("res://"), file_size / 1024));

		Vector<String> changed_elements;
		Vector<String> reason;
		bool is_ignored = false;
		uint64_t start_time = Time::get_singleton()->get_ticks_msec();

		if (file_name.ends_with(".shader")) {
			reason.append("\tFile extension will be renamed from \"shader\" to \"gdshader\".");
		}

		if (file_size < uint64_t(maximum_file_size)) {
			if (file_name.ends_with(".gd")) {
				changed_elements.append_array(check_for_rename_classes(lines, reg_container));

				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::enum_renames, reg_container.enum_regexes, lines));
				changed_elements.append_array(check_for_rename_colors(lines, reg_container));

				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::gdscript_function_renames, reg_container.gdscript_function_regexes, lines));
				changed_elements.append_array(check_for_rename_gdscript_functions(lines, reg_container, false));

				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::project_settings_renames, reg_container.project_settings_regexes, lines));
				changed_elements.append_array(check_for_rename_gdscript_keywords(lines, reg_container, false));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::gdscript_properties_renames, reg_container.gdscript_properties_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::gdscript_signals_renames, reg_container.gdscript_signals_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::shaders_renames, reg_container.shaders_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::theme_override_renames, reg_container.theme_override_regexes, lines));

				changed_elements.append_array(check_for_custom_rename(lines, "\\.shader", ".gdshader"));
			} else if (file_name.ends_with(".tscn")) {
				changed_elements.append_array(check_for_rename_classes(lines, reg_container));

				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::enum_renames, reg_container.enum_regexes, lines));
				changed_elements.append_array(check_for_rename_colors(lines, reg_container));

				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::gdscript_function_renames, reg_container.gdscript_function_regexes, lines));
				changed_elements.append_array(check_for_rename_gdscript_functions(lines, reg_container, true));

				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::project_settings_renames, reg_container.project_settings_regexes, lines));
				changed_elements.append_array(check_for_rename_gdscript_keywords(lines, reg_container, true));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::gdscript_properties_renames, reg_container.gdscript_properties_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::gdscript_signals_renames, reg_container.gdscript_signals_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::shaders_renames, reg_container.shaders_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::theme_override_renames, reg_container.theme_override_regexes, lines));

				changed_elements.append_array(check_for_custom_rename(lines, "\\.shader", ".gdshader"));
			} else if (file_name.ends_with(".cs")) {
				changed_elements.append_array(check_for_rename_classes(lines, reg_container));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::csharp_function_renames, reg_container.csharp_function_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::csharp_properties_renames, reg_container.csharp_properties_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::csharp_signals_renames, reg_container.csharp_signal_regexes, lines));
				changed_elements.append_array(check_for_rename_csharp_functions(lines, reg_container));
				changed_elements.append_array(check_for_rename_csharp_attributes(lines, reg_container));
				changed_elements.append_array(check_for_custom_rename(lines, "public class ", "public partial class "));
			} else if (file_name.ends_with(".gdshader") || file_name.ends_with(".shader")) {
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::shaders_renames, reg_container.shaders_regexes, lines));
			} else if (file_name.ends_with("tres")) {
				changed_elements.append_array(check_for_rename_classes(lines, reg_container));

				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::shaders_renames, reg_container.shaders_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, lines));

				changed_elements.append_array(check_for_custom_rename(lines, "\\.shader", ".gdshader"));
			} else if (file_name.ends_with("project.godot")) {
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::project_godot_renames, reg_container.project_godot_regexes, lines));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, lines));
				changed_elements.append_array(check_for_rename_input_map_scancode(lines, reg_container));
				changed_elements.append_array(check_for_rename_joypad_buttons_and_axes(lines, reg_container));
				changed_elements.append_array(check_for_rename_common(RenamesMap3To4::input_map_renames, reg_container.input_map_regexes, lines));
			} else if (file_name.ends_with(".csproj")) {
				// TODO
			} else {
				ERR_PRINT(vformat("\"%s\", is not supported!", file_name));
				continue;
			}

			for (String &line : lines) {
				if (uint64_t(line.length()) > maximum_line_length) {
					ignored_lines += 1;
				}
			}
		} else {
			reason.append(vformat("\tERROR: File has exceeded the maximum size allowed  - %d KB.", maximum_file_size / 1024));
			is_ignored = true;
		}

		uint64_t end_time = Time::get_singleton()->get_ticks_msec();
		String end_message = vformat("    Checking file took %10.3f ms.", (end_time - start_time) / 1000.0);
		if (ignored_lines != 0) {
			end_message += vformat(" Ignored %d lines, because their length exceeds maximum allowed characters - %d.", ignored_lines, maximum_line_length);
		}
		print_line(end_message);

		for (int k = 0; k < reason.size(); k++) {
			print_line(reason[k]);
		}

		if (changed_elements.size() > 0 && !is_ignored) {
			converted_files++;

			for (int k = 0; k < changed_elements.size(); k++) {
				print_line(String("\t\t") + changed_elements[k]);
			}
		}
	}

	print_line(vformat("Checking for valid conversion ended - all files(%d), files which would be converted(%d), files which would not be converted(%d).", collected_files.size(), converted_files, collected_files.size() - converted_files));
	uint64_t conversion_end_time = Time::get_singleton()->get_ticks_msec();
	print_line(vformat("Conversion of all files took %10.3f seconds.", (conversion_end_time - conversion_start_time) / 1000.0));
	return true;
}

// Collect files which will be checked, excluding ".txt", ".mp4", ".wav" etc. files.
Vector<String> ProjectConverter3To4::check_for_files() {
	Vector<String> collected_files = Vector<String>();

	Vector<String> directories_to_check = Vector<String>();
	directories_to_check.push_back("res://");

	while (!directories_to_check.is_empty()) {
		String path = directories_to_check.get(directories_to_check.size() - 1); // Is there any pop_back function?
		directories_to_check.resize(directories_to_check.size() - 1); // Remove last element

		Ref<DirAccess> dir = DirAccess::open(path);
		if (dir.is_valid()) {
			dir->set_include_hidden(true);
			dir->list_dir_begin();
			String current_dir = dir->get_current_dir();
			String file_name = dir->_get_next();

			while (file_name != "") {
				if (file_name == ".git" || file_name == ".godot") {
					file_name = dir->_get_next();
					continue;
				}
				if (dir->current_is_dir()) {
					directories_to_check.append(current_dir.path_join(file_name) + "/");
				} else {
					bool proper_extension = false;
					if (file_name.ends_with(".gd") || file_name.ends_with(".shader") || file_name.ends_with(".gdshader") || file_name.ends_with(".tscn") || file_name.ends_with(".tres") || file_name.ends_with(".godot") || file_name.ends_with(".cs") || file_name.ends_with(".csproj") || file_name.ends_with(".import")) {
						proper_extension = true;
					}

					if (proper_extension) {
						collected_files.append(current_dir.path_join(file_name));
					}
				}
				file_name = dir->_get_next();
			}
		} else {
			print_verbose("Failed to open " + path);
		}
	}
	return collected_files;
}

Vector<SourceLine> ProjectConverter3To4::split_lines(const String &text) {
	Vector<String> lines = text.split("\n");
	Vector<SourceLine> source_lines;
	for (String &line : lines) {
		SourceLine source_line;
		source_line.line = line;
		source_line.is_comment = false;

		source_lines.append(source_line);
	}
	return source_lines;
}

// Test expected results of gdscript
bool ProjectConverter3To4::test_conversion_gdscript_builtin(const String &name, const String &expected, void (ProjectConverter3To4::*func)(Vector<SourceLine> &, const RegExContainer &, bool), const String &what, const RegExContainer &reg_container, bool builtin_script) {
	Vector<SourceLine> got = split_lines(name);

	(this->*func)(got, reg_container, builtin_script);
	String got_str = collect_string_from_vector(got);
	ERR_FAIL_COND_V_MSG(expected != got_str, false, vformat("Failed to convert %s \"%s\" to \"%s\", got instead \"%s\"", what, name, expected, got_str));

	return true;
}

bool ProjectConverter3To4::test_conversion_with_regex(const String &name, const String &expected, void (ProjectConverter3To4::*func)(Vector<SourceLine> &, const RegExContainer &), const String &what, const RegExContainer &reg_container) {
	Vector<SourceLine> got = split_lines(name);

	(this->*func)(got, reg_container);
	String got_str = collect_string_from_vector(got);
	ERR_FAIL_COND_V_MSG(expected != got_str, false, vformat("Failed to convert %s \"%s\" to \"%s\", got instead \"%s\"", what, name, expected, got_str));

	return true;
}

bool ProjectConverter3To4::test_conversion_basic(const String &name, const String &expected, const char *array[][2], LocalVector<RegEx *> &regex_cache, const String &what) {
	Vector<SourceLine> got = split_lines(name);

	rename_common(array, regex_cache, got);
	String got_str = collect_string_from_vector(got);
	ERR_FAIL_COND_V_MSG(expected != got_str, false, vformat("Failed to convert %s \"%s\" to \"%s\", got instead \"%s\"", what, name, expected, got_str));

	return true;
}

// Validate if conversions are proper.
bool ProjectConverter3To4::test_conversion(RegExContainer &reg_container) {
	bool valid = true;

	valid = valid && test_conversion_with_regex("tool", "@tool", &ProjectConverter3To4::fix_tool_declaration, "gdscript keyword", reg_container);
	valid = valid && test_conversion_with_regex("\n    tool", "\n    tool", &ProjectConverter3To4::fix_tool_declaration, "gdscript keyword", reg_container);
	valid = valid && test_conversion_with_regex("\n\ntool", "@tool\n\n", &ProjectConverter3To4::fix_tool_declaration, "gdscript keyword", reg_container);

	valid = valid && test_conversion_with_regex("pause_mode = 2", "pause_mode = 3", &ProjectConverter3To4::fix_pause_mode, "pause_mode", reg_container);
	valid = valid && test_conversion_with_regex("pause_mode = 1", "pause_mode = 1", &ProjectConverter3To4::fix_pause_mode, "pause_mode", reg_container);
	valid = valid && test_conversion_with_regex("pause_mode = 3", "pause_mode = 3", &ProjectConverter3To4::fix_pause_mode, "pause_mode", reg_container);
	valid = valid && test_conversion_with_regex("somepause_mode = 2", "somepause_mode = 2", &ProjectConverter3To4::fix_pause_mode, "pause_mode", reg_container);
	valid = valid && test_conversion_with_regex("pause_mode_ext = 2", "pause_mode_ext = 2", &ProjectConverter3To4::fix_pause_mode, "pause_mode", reg_container);

	valid = valid && test_conversion_basic("TYPE_REAL", "TYPE_FLOAT", RenamesMap3To4::enum_renames, reg_container.enum_regexes, "enum");

	valid = valid && test_conversion_basic("can_instance", "can_instantiate", RenamesMap3To4::gdscript_function_renames, reg_container.gdscript_function_regexes, "gdscript function");

	valid = valid && test_conversion_basic("CanInstance", "CanInstantiate", RenamesMap3To4::csharp_function_renames, reg_container.csharp_function_regexes, "csharp function");

	valid = valid && test_conversion_basic("translation", "position", RenamesMap3To4::gdscript_properties_renames, reg_container.gdscript_properties_regexes, "gdscript property");

	valid = valid && test_conversion_basic("Translation", "Position", RenamesMap3To4::csharp_properties_renames, reg_container.csharp_properties_regexes, "csharp property");

	valid = valid && test_conversion_basic("NORMALMAP", "NORMAL_MAP", RenamesMap3To4::shaders_renames, reg_container.shaders_regexes, "shader");

	valid = valid && test_conversion_basic("text_entered", "text_submitted", RenamesMap3To4::gdscript_signals_renames, reg_container.gdscript_signals_regexes, "gdscript signal");

	valid = valid && test_conversion_basic("TextEntered", "TextSubmitted", RenamesMap3To4::csharp_signals_renames, reg_container.csharp_signal_regexes, "csharp signal");

	valid = valid && test_conversion_basic("audio/channel_disable_threshold_db", "audio/buses/channel_disable_threshold_db", RenamesMap3To4::project_settings_renames, reg_container.project_settings_regexes, "project setting");

	valid = valid && test_conversion_basic("\"device\":-1,\"alt\":false,\"shift\":false,\"control\":false,\"meta\":false,\"doubleclick\":false,\"scancode\":0,\"physical_scancode\":16777254,\"script\":null", "\"device\":-1,\"alt_pressed\":false,\"shift_pressed\":false,\"ctrl_pressed\":false,\"meta_pressed\":false,\"double_click\":false,\"keycode\":0,\"physical_keycode\":16777254,\"script\":null", RenamesMap3To4::input_map_renames, reg_container.input_map_regexes, "input map");

	valid = valid && test_conversion_basic("Transform", "Transform3D", RenamesMap3To4::builtin_types_renames, reg_container.builtin_types_regexes, "builtin type");

	valid = valid && test_conversion_basic("custom_constants/margin_right", "theme_override_constants/margin_right", RenamesMap3To4::theme_override_renames, reg_container.theme_override_regexes, "theme overrides");

	// Custom Renames.

	valid = valid && test_conversion_with_regex("(Connect(A,B,C,D,E,F,G) != OK):", "(Connect(A, new Callable(B, C), D, E, F, G) != OK):", &ProjectConverter3To4::rename_csharp_functions, "custom rename csharp", reg_container);
	valid = valid && test_conversion_with_regex("(Disconnect(A,B,C) != OK):", "(Disconnect(A, new Callable(B, C)) != OK):", &ProjectConverter3To4::rename_csharp_functions, "custom rename csharp", reg_container);
	valid = valid && test_conversion_with_regex("(IsConnected(A,B,C) != OK):", "(IsConnected(A, new Callable(B, C)) != OK):", &ProjectConverter3To4::rename_csharp_functions, "custom rename", reg_container);

	valid = valid && test_conversion_with_regex("[Remote]", "[RPC(MultiplayerAPI.RPCMode.AnyPeer)]", &ProjectConverter3To4::rename_csharp_attributes, "custom rename csharp", reg_container);
	valid = valid && test_conversion_with_regex("[RemoteSync]", "[RPC(MultiplayerAPI.RPCMode.AnyPeer, CallLocal = true)]", &ProjectConverter3To4::rename_csharp_attributes, "custom rename csharp", reg_container);
	valid = valid && test_conversion_with_regex("[Sync]", "[RPC(MultiplayerAPI.RPCMode.AnyPeer, CallLocal = true)]", &ProjectConverter3To4::rename_csharp_attributes, "custom rename csharp", reg_container);
	valid = valid && test_conversion_with_regex("[Slave]", "[RPC]", &ProjectConverter3To4::rename_csharp_attributes, "custom rename csharp", reg_container);
	valid = valid && test_conversion_with_regex("[Puppet]", "[RPC]", &ProjectConverter3To4::rename_csharp_attributes, "custom rename csharp", reg_container);
	valid = valid && test_conversion_with_regex("[PuppetSync]", "[RPC(CallLocal = true)]", &ProjectConverter3To4::rename_csharp_attributes, "custom rename csharp", reg_container);
	valid = valid && test_conversion_with_regex("[Master]", "The master and mastersync rpc behavior is not officially supported anymore. Try using another keyword or making custom logic using Multiplayer.GetRemoteSenderId()\n[RPC]", &ProjectConverter3To4::rename_csharp_attributes, "custom rename csharp", reg_container);
	valid = valid && test_conversion_with_regex("[MasterSync]", "The master and mastersync rpc behavior is not officially supported anymore. Try using another keyword or making custom logic using Multiplayer.GetRemoteSenderId()\n[RPC(CallLocal = true)]", &ProjectConverter3To4::rename_csharp_attributes, "custom rename csharp", reg_container);

	valid = valid && test_conversion_gdscript_builtin("\tif OS.window_resizable: pass", "\tif (not get_window().unresizable): pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tif OS.is_window_resizable(): pass", "\tif (not get_window().unresizable): pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.set_window_resizable(Settings.resizable)", "\tget_window().unresizable = not (Settings.resizable)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.window_resizable = Settings.resizable", "\tget_window().unresizable = not (Settings.resizable)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("\tif OS.window_fullscreen: pass", "\tif ((get_window().mode == Window.MODE_EXCLUSIVE_FULLSCREEN) or (get_window().mode == Window.MODE_FULLSCREEN)): pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tif OS.is_window_fullscreen(): pass", "\tif ((get_window().mode == Window.MODE_EXCLUSIVE_FULLSCREEN) or (get_window().mode == Window.MODE_FULLSCREEN)): pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.set_window_fullscreen(Settings.fullscreen)", "\tget_window().mode = Window.MODE_EXCLUSIVE_FULLSCREEN if (Settings.fullscreen) else Window.MODE_WINDOWED", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.window_fullscreen = Settings.fullscreen", "\tget_window().mode = Window.MODE_EXCLUSIVE_FULLSCREEN if (Settings.fullscreen) else Window.MODE_WINDOWED", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("\tif OS.window_maximized: pass", "\tif (get_window().mode == Window.MODE_MAXIMIZED): pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tif OS.is_window_maximized(): pass", "\tif (get_window().mode == Window.MODE_MAXIMIZED): pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.set_window_maximized(Settings.maximized)", "\tget_window().mode = Window.MODE_MAXIMIZED if (Settings.maximized) else Window.MODE_WINDOWED", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.window_maximized = Settings.maximized", "\tget_window().mode = Window.MODE_MAXIMIZED if (Settings.maximized) else Window.MODE_WINDOWED", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("\tif OS.window_minimized: pass", "\tif (get_window().mode == Window.MODE_MINIMIZED): pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tif OS.is_window_minimized(): pass", "\tif (get_window().mode == Window.MODE_MINIMIZED): pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.set_window_minimized(Settings.minimized)", "\tget_window().mode = Window.MODE_MINIMIZED if (Settings.minimized) else Window.MODE_WINDOWED", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.window_minimized = Settings.minimized", "\tget_window().mode = Window.MODE_MINIMIZED if (Settings.minimized) else Window.MODE_WINDOWED", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("\tif OS.vsync_enabled: pass", "\tif (DisplayServer.window_get_vsync_mode() != DisplayServer.VSYNC_DISABLED): pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tif OS.is_vsync_enabled(): pass", "\tif (DisplayServer.window_get_vsync_mode() != DisplayServer.VSYNC_DISABLED): pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.set_use_vsync(Settings.vsync)", "\tDisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_ENABLED if (Settings.vsync) else DisplayServer.VSYNC_DISABLED)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.vsync_enabled = Settings.vsync", "\tDisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_ENABLED if (Settings.vsync) else DisplayServer.VSYNC_DISABLED)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("\tif OS.screen_orientation = OS.SCREEN_ORIENTATION_VERTICAL: pass", "\tif DisplayServer.screen_get_orientation() = DisplayServer.SCREEN_VERTICAL: pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tif OS.get_screen_orientation() = OS.SCREEN_ORIENTATION_LANDSCAPE: pass", "\tif DisplayServer.screen_get_orientation() = DisplayServer.SCREEN_LANDSCAPE: pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.set_screen_orientation(Settings.orient)", "\tDisplayServer.screen_set_orientation(Settings.orient)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.screen_orientation = Settings.orient", "\tDisplayServer.screen_set_orientation(Settings.orient)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("\tif OS.is_window_always_on_top(): pass", "\tif get_window().always_on_top: pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.set_window_always_on_top(Settings.alwaystop)", "\tget_window().always_on_top = (Settings.alwaystop)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("\tif OS.get_borderless_window(): pass", "\tif get_window().borderless: pass", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tOS.set_borderless_window(Settings.borderless)", "\tget_window().borderless = (Settings.borderless)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("\tvar aa = roman(r.move_and_slide( a, b, c, d, e, f )) # Roman", "\tr.set_velocity(a)\n\tr.set_up_direction(b)\n\tr.set_floor_stop_on_slope_enabled(c)\n\tr.set_max_slides(d)\n\tr.set_floor_max_angle(e)\n\t# TODOConverter3To4 infinite_inertia were removed in Godot 4 - previous value `f`\n\tr.move_and_slide()\n\tvar aa = roman(r.velocity) # Roman", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tmove_and_slide( a, b, c, d, e, f ) # Roman", "\tset_velocity(a)\n\tset_up_direction(b)\n\tset_floor_stop_on_slope_enabled(c)\n\tset_max_slides(d)\n\tset_floor_max_angle(e)\n\t# TODOConverter3To4 infinite_inertia were removed in Godot 4 - previous value `f`\n\tmove_and_slide() # Roman", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tvar aa = roman(r.move_and_slide_with_snap( a, g, b, c, d, e, f )) # Roman", "\tr.set_velocity(a)\n\t# TODOConverter3To4 looks that snap in Godot 4 is float, not vector like in Godot 3 - previous value `g`\n\tr.set_up_direction(b)\n\tr.set_floor_stop_on_slope_enabled(c)\n\tr.set_max_slides(d)\n\tr.set_floor_max_angle(e)\n\t# TODOConverter3To4 infinite_inertia were removed in Godot 4 - previous value `f`\n\tr.move_and_slide()\n\tvar aa = roman(r.velocity) # Roman", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tmove_and_slide_with_snap( a, g, b, c, d, e, f ) # Roman", "\tset_velocity(a)\n\t# TODOConverter3To4 looks that snap in Godot 4 is float, not vector like in Godot 3 - previous value `g`\n\tset_up_direction(b)\n\tset_floor_stop_on_slope_enabled(c)\n\tset_max_slides(d)\n\tset_floor_max_angle(e)\n\t# TODOConverter3To4 infinite_inertia were removed in Godot 4 - previous value `f`\n\tmove_and_slide() # Roman", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("remove_and_slide(a,b,c,d,e,f)", "remove_and_slide(a,b,c,d,e,f)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("list_dir_begin( a , b )", "list_dir_begin() # TODOConverter3To4 fill missing arguments https://github.com/godotengine/godot/pull/40547", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("list_dir_begin( a )", "list_dir_begin() # TODOConverter3To4 fill missing arguments https://github.com/godotengine/godot/pull/40547", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("list_dir_begin( )", "list_dir_begin() # TODOConverter3To4 fill missing arguments https://github.com/godotengine/godot/pull/40547", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("sort_custom( a , b )", "sort_custom(Callable(a, b))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("func c(var a, var b)", "func c(a, b)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("draw_line(1, 2, 3, 4, 5)", "draw_line(1, 2, 3, 4)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("\timage.lock()", "\tfalse # image.lock() # TODOConverter3To4, Image no longer requires locking, `false` helps to not break one line if/else, so it can freely be removed", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\timage.unlock()", "\tfalse # image.unlock() # TODOConverter3To4, Image no longer requires locking, `false` helps to not break one line if/else, so it can freely be removed", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\troman.image.unlock()", "\tfalse # roman.image.unlock() # TODOConverter3To4, Image no longer requires locking, `false` helps to not break one line if/else, so it can freely be removed", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tmtx.lock()", "\tmtx.lock()", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tmutex.unlock()", "\tmutex.unlock()", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_with_regex("extends CSGBox", "extends CSGBox3D", &ProjectConverter3To4::rename_classes, "classes", reg_container);
	valid = valid && test_conversion_with_regex("CSGBox", "CSGBox3D", &ProjectConverter3To4::rename_classes, "classes", reg_container);
	valid = valid && test_conversion_with_regex("Spatial", "Node3D", &ProjectConverter3To4::rename_classes, "classes", reg_container);
	valid = valid && test_conversion_with_regex("Spatial.tscn", "Spatial.tscn", &ProjectConverter3To4::rename_classes, "classes", reg_container);
	valid = valid && test_conversion_with_regex("Spatial.gd", "Spatial.gd", &ProjectConverter3To4::rename_classes, "classes", reg_container);
	valid = valid && test_conversion_with_regex("Spatial.shader", "Spatial.shader", &ProjectConverter3To4::rename_classes, "classes", reg_container);
	valid = valid && test_conversion_with_regex("Spatial.other", "Node3D.other", &ProjectConverter3To4::rename_classes, "classes", reg_container);

	valid = valid && test_conversion_gdscript_builtin("\nonready", "\n@onready", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("onready", "@onready", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin(" onready", " onready", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\nexport", "\n@export", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\texport", "\t@export", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\texport_dialog", "\texport_dialog", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("export", "@export", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin(" export", " export", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\n\nremote func", "\n\n@rpc(\"any_peer\") func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\n\nremote func", "\n\n@rpc(\\\"any_peer\\\") func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, true);
	valid = valid && test_conversion_gdscript_builtin("\n\nremotesync func", "\n\n@rpc(\"any_peer\", \"call_local\") func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\n\nremotesync func", "\n\n@rpc(\\\"any_peer\\\", \\\"call_local\\\") func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, true);
	valid = valid && test_conversion_gdscript_builtin("\n\nsync func", "\n\n@rpc(\"any_peer\", \"call_local\") func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\n\nsync func", "\n\n@rpc(\\\"any_peer\\\", \\\"call_local\\\") func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, true);
	valid = valid && test_conversion_gdscript_builtin("\n\nslave func", "\n\n@rpc func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\n\npuppet func", "\n\n@rpc func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\n\npuppetsync func", "\n\n@rpc(\"call_local\") func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\n\npuppetsync func", "\n\n@rpc(\\\"call_local\\\") func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, true);
	valid = valid && test_conversion_gdscript_builtin("\n\nmaster func", "\n\nThe master and mastersync rpc behavior is not officially supported anymore. Try using another keyword or making custom logic using get_multiplayer().get_remote_sender_id()\n@rpc func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\n\nmastersync func", "\n\nThe master and mastersync rpc behavior is not officially supported anymore. Try using another keyword or making custom logic using get_multiplayer().get_remote_sender_id()\n@rpc(\"call_local\") func", &ProjectConverter3To4::rename_gdscript_keywords, "gdscript keyword", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("var size: Vector2 = Vector2() setget set_function, get_function", "var size: Vector2 = Vector2(): get = get_function, set = set_function", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("var size: Vector2 = Vector2() setget set_function, ", "var size: Vector2 = Vector2(): set = set_function", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("var size: Vector2 = Vector2() setget set_function", "var size: Vector2 = Vector2(): set = set_function", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("var size: Vector2 = Vector2() setget , get_function", "var size: Vector2 = Vector2(): get = get_function", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("get_node(@", "get_node(", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("yield(this, \"timeout\")", "await this.timeout", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("yield(this, \\\"timeout\\\")", "await this.timeout", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, true);

	valid = valid && test_conversion_gdscript_builtin(" Transform.xform(Vector3(a,b,c) + Vector3.UP) ", " Transform * (Vector3(a,b,c) + Vector3.UP) ", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin(" Transform.xform_inv(Vector3(a,b,c) + Vector3.UP) ", " (Vector3(a,b,c) + Vector3.UP) * Transform ", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("export(float) var lifetime = 3.0", "export var lifetime: float = 3.0", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("export (int)var spaces=1", "export var spaces: int=1", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("export(String, 'AnonymousPro', 'CourierPrime') var _font_name = 'AnonymousPro'", "export var _font_name = 'AnonymousPro' # (String, 'AnonymousPro', 'CourierPrime')", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false); // TODO, this is only a workaround
	valid = valid && test_conversion_gdscript_builtin("export(PackedScene) var mob_scene", "export var mob_scene: PackedScene", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("export(float) var lifetime: float = 3.0", "export var lifetime: float = 3.0", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("export var lifetime: float = 3.0", "export var lifetime: float = 3.0", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("export var lifetime := 3.0", "export var lifetime := 3.0", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("export(float) var lifetime := 3.0", "export var lifetime := 3.0", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("var d = parse_json(roman(sfs))", "var test_json_conv = JSON.new()\ntest_json_conv.parse(roman(sfs))\nvar d = test_json_conv.get_data()", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("to_json( AA ) szon", "JSON.new().stringify( AA ) szon", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("s to_json", "s JSON.new().stringify", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("AF to_json2", "AF to_json2", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("var rr = JSON.parse(a)", "var test_json_conv = JSON.new()\ntest_json_conv.parse(a)\nvar rr = test_json_conv.get_data()", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("empty()", "is_empty()", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin(".empty", ".empty", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin(").roman(", ").roman(", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\t.roman(", "\tsuper.roman(", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin(" .roman(", " super.roman(", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin(".1", ".1", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin(" .1", " .1", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("'.'", "'.'", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("'.a'", "'.a'", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\t._input(_event)", "\tsuper._input(_event)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("(connect(A,B,C) != OK):", "(connect(A, Callable(B, C)) != OK):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("(connect(A,B,C,D) != OK):", "(connect(A, Callable(B, C).bind(D)) != OK):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("(connect(A,B,C,[D]) != OK):", "(connect(A, Callable(B, C).bind(D)) != OK):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("(connect(A,B,C,[D,E]) != OK):", "(connect(A, Callable(B, C).bind(D,E)) != OK):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("(connect(A,B,C,[D,E],F) != OK):", "(connect(A, Callable(B, C).bind(D,E), F) != OK):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("(connect(A,B,C,D,E) != OK):", "(connect(A, Callable(B, C).bind(D), E) != OK):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin(".connect(A,B,C)", ".connect(A, Callable(B, C))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("abc.connect(A,B,C)", "abc.connect(A, Callable(B, C))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("\tconnect(A,B,C)", "\tconnect(A, Callable(B, C))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin(" connect(A,B,C)", " connect(A, Callable(B, C))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("_connect(A,B,C)", "_connect(A,B,C)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("do_connect(A,B,C)", "do_connect(A,B,C)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("$connect(A,B,C)", "$connect(A,B,C)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("@connect(A,B,C)", "@connect(A,B,C)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("(start(A,B) != OK):", "(start(Callable(A, B)) != OK):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("func start(A,B):", "func start(A,B):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("(start(A,B,C,D,E,F,G) != OK):", "(start(Callable(A, B).bind(C), D, E, F, G) != OK):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("disconnect(A,B,C) != OK):", "disconnect(A, Callable(B, C)) != OK):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("is_connected(A,B,C) != OK):", "is_connected(A, Callable(B, C)) != OK):", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("is_connected(A,B,C))", "is_connected(A, Callable(B, C)))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("(tween_method(A,B,C,D,E).foo())", "(tween_method(Callable(A, B), C, D, E).foo())", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("(tween_method(A,B,C,D,E,[F,G]).foo())", "(tween_method(Callable(A, B).bind(F,G), C, D, E).foo())", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("(tween_callback(A,B).foo())", "(tween_callback(Callable(A, B)).foo())", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("(tween_callback(A,B,[C,D]).foo())", "(tween_callback(Callable(A, B).bind(C,D)).foo())", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("func _init(", "func _init(", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("func _init(a,b,c).(d,e,f):", "func _init(a,b,c):\n\tsuper(d,e,f)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("func _init(a,b,c).(a.b(),c.d()):", "func _init(a,b,c):\n\tsuper(a.b(),c.d())", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("func _init(p_x:int)->void:", "func _init(p_x:int)->void:", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("func _init(a: int).(d,e,f) -> void:", "func _init(a: int) -> void:\n\tsuper(d,e,f)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("q_PackedDataContainer._iter_init(variable1)", "q_PackedDataContainer._iter_init(variable1)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("create_from_image(aa, bb)", "create_from_image(aa) #,bb", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("q_ImageTexture.create_from_image(variable1, variable2)", "q_ImageTexture.create_from_image(variable1) #,variable2", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("set_cell_item(a, b, c, d ,e) # AA", "set_cell_item(Vector3(a, b, c), d, e) # AA", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("set_cell_item(a, b)", "set_cell_item(a, b)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("get_cell_item_orientation(a, b,c)", "get_cell_item_orientation(Vector3i(a, b, c))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("get_cell_item(a, b,c)", "get_cell_item(Vector3i(a, b, c))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("map_to_world(a, b,c)", "map_to_local(Vector3i(a, b, c))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("PackedStringArray(req_godot).join('.')", "'.'.join(PackedStringArray(req_godot))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("=PackedStringArray(req_godot).join('.')", "='.'.join(PackedStringArray(req_godot))", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_gdscript_builtin("apply_force(position, impulse)", "apply_force(impulse, position)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("apply_impulse(position, impulse)", "apply_impulse(impulse, position)", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("draw_rect(a,b,c,d,e).abc", "draw_rect(a, b, c, d).abc# e) TODOConverter3To4 Antialiasing argument is missing", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("get_focus_owner()", "get_viewport().gui_get_focus_owner()", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("button.pressed = 1", "button.button_pressed = 1", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("button.pressed=1", "button.button_pressed=1", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);
	valid = valid && test_conversion_gdscript_builtin("button.pressed SF", "button.pressed SF", &ProjectConverter3To4::rename_gdscript_functions, "custom rename", reg_container, false);

	valid = valid && test_conversion_with_regex("Color(\"#f47d\")", "Color(\"#47df\")", &ProjectConverter3To4::convert_hexadecimal_colors, "color literals", reg_container);
	valid = valid && test_conversion_with_regex("Color(\"#ff478cbf\")", "Color(\"#478cbfff\")", &ProjectConverter3To4::convert_hexadecimal_colors, "color literals", reg_container);
	valid = valid && test_conversion_with_regex("Color(\"#de32bf\")", "Color(\"#de32bf\")", &ProjectConverter3To4::convert_hexadecimal_colors, "color literals", reg_container);
	valid = valid && test_conversion_with_regex("AAA Color.white AF", "AAA Color.WHITE AF", &ProjectConverter3To4::rename_colors, "color constants", reg_container);

	// Note: Do not change to *scancode*, it is applied before that conversion.
	valid = valid && test_conversion_with_regex("\"device\":-1,\"scancode\":16777231,\"physical_scancode\":16777232", "\"device\":-1,\"scancode\":4194319,\"physical_scancode\":4194320", &ProjectConverter3To4::rename_input_map_scancode, "custom rename", reg_container);
	valid = valid && test_conversion_with_regex("\"device\":-1,\"scancode\":65,\"physical_scancode\":66", "\"device\":-1,\"scancode\":65,\"physical_scancode\":66", &ProjectConverter3To4::rename_input_map_scancode, "custom rename", reg_container);

	valid = valid && test_conversion_with_regex("\"device\":0,\"button_index\":5,\"pressure\":0.0,\"pressed\":false,", "\"device\":0,\"button_index\":10,\"pressure\":0.0,\"pressed\":false,", &ProjectConverter3To4::rename_joypad_buttons_and_axes, "custom rename", reg_container);
	valid = valid && test_conversion_with_regex("\"device\":0,\"axis\":6,", "\"device\":0,\"axis\":4,", &ProjectConverter3To4::rename_joypad_buttons_and_axes, "custom rename", reg_container);
	valid = valid && test_conversion_with_regex("InputEventJoypadButton,\"button_index\":7,\"pressure\":0.0,\"pressed\":false,\"script\":null", "InputEventJoypadMotion,\"axis\":5,\"axis_value\":1.0,\"script\":null", &ProjectConverter3To4::rename_joypad_buttons_and_axes, "custom rename", reg_container);

	// Custom rule conversion
	{
		String from = "instance";
		String to = "instantiate";
		String name = "AA.instance()";

		Vector<SourceLine> got = split_lines(name);

		String expected = "AA.instantiate()";
		custom_rename(got, from, to);
		String got_str = collect_string_from_vector(got);
		if (got_str != expected) {
			ERR_PRINT(vformat("Failed to convert custom rename \"%s\" to \"%s\", got \"%s\", instead.", name, expected, got_str));
		}
		valid = valid && (got_str == expected);
	}

	// get_object_of_execution
	{
		String base = "var roman = kieliszek.";
		String expected = "kieliszek.";
		String got = get_object_of_execution(base);
		if (got != expected) {
			ERR_PRINT(vformat("Failed to get proper data from get_object_of_execution. \"%s\" should return \"%s\"(%d), got \"%s\"(%d), instead.", base, expected, expected.size(), got, got.size()));
		}
		valid = valid && (got == expected);
	}
	{
		String base = "r.";
		String expected = "r.";
		String got = get_object_of_execution(base);
		if (got != expected) {
			ERR_PRINT(vformat("Failed to get proper data from get_object_of_execution. \"%s\" should return \"%s\"(%d), got \"%s\"(%d), instead.", base, expected, expected.size(), got, got.size()));
		}
		valid = valid && (got == expected);
	}
	{
		String base = "mortadela(";
		String expected = "";
		String got = get_object_of_execution(base);
		if (got != expected) {
			ERR_PRINT(vformat("Failed to get proper data from get_object_of_execution. \"%s\" should return \"%s\"(%d), got \"%s\"(%d), instead.", base, expected, expected.size(), got, got.size()));
		}
		valid = valid && (got == expected);
	}
	{
		String base = "var node = $world/ukraine/lviv.";
		String expected = "$world/ukraine/lviv.";
		String got = get_object_of_execution(base);
		if (got != expected) {
			ERR_PRINT(vformat("Failed to get proper data from get_object_of_execution. \"%s\" should return \"%s\"(%d), got \"%s\"(%d), instead.", base, expected, expected.size(), got, got.size()));
		}
		valid = valid && (got == expected);
	}

	// get_starting_space
	{
		String base = "\t\t\t var roman = kieliszek.";
		String expected = "\t\t\t";
		String got = get_starting_space(base);
		if (got != expected) {
			ERR_PRINT(vformat("Failed to get proper data from get_object_of_execution. \"%s\" should return \"%s\"(%d), got \"%s\"(%d), instead.", base, expected, expected.size(), got, got.size()));
		}
		valid = valid && (got == expected);
	}

	// Parse Arguments
	{
		String line = "( )";
		Vector<String> got_vector = parse_arguments(line);
		String got = "";
		String expected = "";
		for (String &part : got_vector) {
			got += part + "|||";
		}
		if (got != expected) {
			ERR_PRINT(vformat("Failed to get proper data from parse_arguments. \"%s\" should return \"%s\"(%d), got \"%s\"(%d), instead.", line, expected, expected.size(), got, got.size()));
		}
		valid = valid && (got == expected);
	}
	{
		String line = "(a , b , c)";
		Vector<String> got_vector = parse_arguments(line);
		String got = "";
		String expected = "a|||b|||c|||";
		for (String &part : got_vector) {
			got += part + "|||";
		}
		if (got != expected) {
			ERR_PRINT(vformat("Failed to get proper data from parse_arguments. \"%s\" should return \"%s\"(%d), got \"%s\"(%d), instead.", line, expected, expected.size(), got, got.size()));
		}
		valid = valid && (got == expected);
	}
	{
		String line = "(a , \"b,\" , c)";
		Vector<String> got_vector = parse_arguments(line);
		String got = "";
		String expected = "a|||\"b,\"|||c|||";
		for (String &part : got_vector) {
			got += part + "|||";
		}
		if (got != expected) {
			ERR_PRINT(vformat("Failed to get proper data from parse_arguments. \"%s\" should return \"%s\"(%d), got \"%s\"(%d), instead.", line, expected, expected.size(), got, got.size()));
		}
		valid = valid && (got == expected);
	}
	{
		String line = "(a , \"(,),,,,\" , c)";
		Vector<String> got_vector = parse_arguments(line);
		String got = "";
		String expected = "a|||\"(,),,,,\"|||c|||";
		for (String &part : got_vector) {
			got += part + "|||";
		}
		if (got != expected) {
			ERR_PRINT(vformat("Failed to get proper data from parse_arguments. \"%s\" should return \"%s\"(%d), got \"%s\"(%d), instead.", line, expected, expected.size(), got, got.size()));
		}
		valid = valid && (got == expected);
	}

	return valid;
}

// Validate in all arrays if names don't do cyclic renames "Node" -> "Node2D" | "Node2D" -> "2DNode"
bool ProjectConverter3To4::test_array_names() {
	bool valid = true;
	Vector<String> names = Vector<String>();

	// Validate if all classes are valid.
	{
		for (unsigned int current_index = 0; RenamesMap3To4::class_renames[current_index][0]; current_index++) {
			const String old_class = RenamesMap3To4::class_renames[current_index][0];
			const String new_class = RenamesMap3To4::class_renames[current_index][1];

			// Light2D, Texture, Viewport are special classes(probably virtual ones).
			if (ClassDB::class_exists(StringName(old_class)) && old_class != "Light2D" && old_class != "Texture" && old_class != "Viewport") {
				ERR_PRINT(vformat("Class \"%s\" exists in Godot 4, so it cannot be renamed to something else.", old_class));
				valid = false; // This probably should be only a warning, but not 100% sure - this would need to be added to CI.
			}

			// Callable is special class, to which normal classes may be renamed.
			if (!ClassDB::class_exists(StringName(new_class)) && new_class != "Callable") {
				ERR_PRINT(vformat("Class \"%s\" does not exist in Godot 4, so it cannot be used in the conversion.", new_class));
				valid = false; // This probably should be only a warning, but not 100% sure - this would need to be added to CI.
			}
		}
	}

	{
		HashSet<String> all_functions;

		// List of excluded functions from builtin types and global namespace, because currently it is not possible to get list of functions from them.
		// This will be available when https://github.com/godotengine/godot/pull/49053 or similar will be included into Godot.
		static const char *builtin_types_excluded_functions[] = { "dict_to_inst", "inst_to_dict", "bytes_to_var", "bytes_to_var_with_objects", "db_to_linear", "deg_to_rad", "linear_to_db", "rad_to_deg", "randf_range", "snapped", "str_to_var", "var_to_str", "var_to_bytes", "var_to_bytes_with_objects", "move_toward", "uri_encode", "uri_decode", "remove_at", "get_rotation_quaternion", "limit_length", "grow_side", "is_absolute_path", "is_valid_int", "lerp", "to_ascii_buffer", "to_utf8_buffer", "to_utf32_buffer", "to_wchar_buffer", "snapped", "remap", "rfind", nullptr };
		for (int current_index = 0; builtin_types_excluded_functions[current_index]; current_index++) {
			all_functions.insert(builtin_types_excluded_functions[current_index]);
		}

		//for (int type = Variant::Type::NIL + 1; type < Variant::Type::VARIANT_MAX; type++) {
		//	List<MethodInfo> method_list;
		//	Variant::get_method_list_by_type(&method_list, Variant::Type(type));
		//	for (MethodInfo &function_data : method_list) {
		//		if (!all_functions.has(function_data.name)) {
		//			all_functions.insert(function_data.name);
		//		}
		//	}
		//}

		List<StringName> classes_list;
		ClassDB::get_class_list(&classes_list);
		for (StringName &name_of_class : classes_list) {
			List<MethodInfo> method_list;
			ClassDB::get_method_list(name_of_class, &method_list, true);
			for (MethodInfo &function_data : method_list) {
				if (!all_functions.has(function_data.name)) {
					all_functions.insert(function_data.name);
				}
			}
		}

		int current_element = 0;
		while (RenamesMap3To4::gdscript_function_renames[current_element][0] != nullptr) {
			String name_3_x = RenamesMap3To4::gdscript_function_renames[current_element][0];
			String name_4_0 = RenamesMap3To4::gdscript_function_renames[current_element][1];
			if (!all_functions.has(name_4_0)) {
				ERR_PRINT(vformat("Missing GDScript function in pair (%s - ===> %s <===)", name_3_x, name_4_0));
				valid = false;
			}
			current_element++;
		}
	}
	if (!valid) {
		ERR_PRINT("Found function which is used in the converter, but it cannot be found in Godot 4. Rename this element or remove its entry if it's obsolete.");
	}

	valid = valid && test_single_array(RenamesMap3To4::enum_renames);
	valid = valid && test_single_array(RenamesMap3To4::class_renames, true);
	valid = valid && test_single_array(RenamesMap3To4::gdscript_function_renames, true);
	valid = valid && test_single_array(RenamesMap3To4::csharp_function_renames, true);
	valid = valid && test_single_array(RenamesMap3To4::gdscript_properties_renames, true);
	valid = valid && test_single_array(RenamesMap3To4::csharp_properties_renames, true);
	valid = valid && test_single_array(RenamesMap3To4::shaders_renames, true);
	valid = valid && test_single_array(RenamesMap3To4::gdscript_signals_renames);
	valid = valid && test_single_array(RenamesMap3To4::project_settings_renames);
	valid = valid && test_single_array(RenamesMap3To4::project_godot_renames);
	valid = valid && test_single_array(RenamesMap3To4::input_map_renames);
	valid = valid && test_single_array(RenamesMap3To4::builtin_types_renames);
	valid = valid && test_single_array(RenamesMap3To4::color_renames);

	return valid;
}

// Validates the array to prevent cyclic renames, such as `Node` -> `Node2D`, then `Node2D` -> `2DNode`.
// Also checks if names contain leading or trailing spaces.
bool ProjectConverter3To4::test_single_array(const char *p_array[][2], bool p_ignore_4_0_name) {
	bool valid = true;
	Vector<String> names = Vector<String>();

	for (unsigned int current_index = 0; p_array[current_index][0]; current_index++) {
		String name_3_x = p_array[current_index][0];
		String name_4_0 = p_array[current_index][1];
		if (name_3_x != name_3_x.strip_edges()) {
			ERR_PRINT(vformat("Invalid Entry \"%s\" contains leading or trailing spaces.", name_3_x));
			valid = false;
		}
		if (names.has(name_3_x)) {
			ERR_PRINT(vformat("Found duplicated entry, pair ( -> %s , %s)", name_3_x, name_4_0));
			valid = false;
		}
		names.append(name_3_x);

		if (name_4_0 != name_4_0.strip_edges()) {
			ERR_PRINT(vformat("Invalid Entry \"%s\" contains leading or trailing spaces.", name_3_x));
			valid = false;
		}
		if (names.has(name_4_0)) {
			ERR_PRINT(vformat("Found duplicated entry, pair ( -> %s , %s)", name_3_x, name_4_0));
			valid = false;
		}
		if (!p_ignore_4_0_name) {
			names.append(name_4_0);
		}
	}
	return valid;
}

// Returns arguments from given function execution, this cannot be really done as regex.
// `abc(d,e(f,g),h)` -> [d], [e(f,g)], [h]
Vector<String> ProjectConverter3To4::parse_arguments(const String &line) {
	Vector<String> parts;
	int string_size = line.length();
	int start_part = 0; // Index of beginning of start part.
	int parts_counter = 0;
	char32_t previous_character = '\0';
	bool is_inside_string = false; // If true, it ignores these 3 characters ( , ) inside string.

	ERR_FAIL_COND_V_MSG(line.count("(") != line.count(")"), parts, vformat("Converter internal bug: substring should have equal number of open and close parentheses in line - \"%s\".", line));

	for (int current_index = 0; current_index < string_size; current_index++) {
		char32_t character = line.get(current_index);
		switch (character) {
			case '(':
			case '[':
			case '{': {
				parts_counter++;
				if (parts_counter == 1 && !is_inside_string) {
					start_part = current_index;
				}
				break;
			};
			case ')':
			case '}': {
				parts_counter--;
				if (parts_counter == 0 && !is_inside_string) {
					parts.append(line.substr(start_part + 1, current_index - start_part - 1));
					start_part = current_index;
				}
				break;
			};
			case ']': {
				parts_counter--;
				if (parts_counter == 0 && !is_inside_string) {
					parts.append(line.substr(start_part, current_index - start_part));
					start_part = current_index;
				}
				break;
			};
			case ',': {
				if (parts_counter == 1 && !is_inside_string) {
					parts.append(line.substr(start_part + 1, current_index - start_part - 1));
					start_part = current_index;
				}
				break;
			};
			case '"': {
				if (previous_character != '\\') {
					is_inside_string = !is_inside_string;
				}
			}
		}
		previous_character = character;
	}

	Vector<String> clean_parts;
	for (String &part : parts) {
		part = part.strip_edges();
		if (!part.is_empty()) {
			clean_parts.append(part);
		}
	}

	return clean_parts;
}

// Finds latest parenthesis owned by function.
// `function(abc(a,b),DD)):` finds this parenthess `function(abc(a,b),DD => ) <= ):`
int ProjectConverter3To4::get_end_parenthesis(const String &line) const {
	int current_state = 0;
	for (int current_index = 0; line.length() > current_index; current_index++) {
		char32_t character = line.get(current_index);
		if (character == '(') {
			current_state++;
		}
		if (character == ')') {
			current_state--;
			if (current_state == 0) {
				return current_index;
			}
		}
	}
	return -1;
}

// Merges multiple arguments into a single String.
// Needed when after processing e.g. 2 arguments, later arguments are not changed in any way.
String ProjectConverter3To4::connect_arguments(const Vector<String> &arguments, int from, int to) const {
	if (to == -1) {
		to = arguments.size();
	}

	String value;
	if (arguments.size() > 0 && from != 0 && from < to) {
		value = ", ";
	}

	for (int i = from; i < to; i++) {
		value += arguments[i];
		if (i != to - 1) {
			value += ", ";
		}
	}
	return value;
}

// Returns the indentation (spaces and tabs) at the start of the line e.g. `\t\tmove_this` returns `\t\t`.
String ProjectConverter3To4::get_starting_space(const String &line) const {
	String empty_space;
	int current_character = 0;

	if (line.is_empty()) {
		return empty_space;
	}

	if (line[0] == ' ') {
		while (current_character < line.size()) {
			if (line[current_character] == ' ') {
				empty_space += ' ';
				current_character++;
			} else {
				break;
			}
		}
	}
	if (line[0] == '\t') {
		while (current_character < line.size()) {
			if (line[current_character] == '\t') {
				empty_space += '\t';
				current_character++;
			} else {
				break;
			}
		}
	}
	return empty_space;
}

// Returns the object that’s executing the function in the line.
// e.g. Passing the line "var roman = kieliszek.funkcja()" to this function returns "kieliszek".
String ProjectConverter3To4::get_object_of_execution(const String &line) const {
	int end = line.size() - 1; // Last one is \0
	int variable_start = end - 1;
	int start = end - 1;

	bool is_possibly_nodepath = false;
	bool is_valid_nodepath = false;

	while (start >= 0) {
		char32_t character = line[start];
		bool is_variable_char = (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') || character == '.' || character == '_';
		bool is_nodepath_start = character == '$';
		bool is_nodepath_sep = character == '/';
		if (is_variable_char || is_nodepath_start || is_nodepath_sep) {
			if (start == 0) {
				break;
			} else if (is_nodepath_sep) {
				// Freeze variable_start, try to fetch more chars since this might be a Node path literal.
				is_possibly_nodepath = true;
			} else if (is_nodepath_start) {
				// Found $, this is a Node path literal.
				is_valid_nodepath = true;
				break;
			}
			if (!is_possibly_nodepath) {
				variable_start--;
			}
			start--;
			continue;
		} else {
			// Abandon all hope, this is neither a variable nor a Node path literal.
			variable_start++; // Found invalid character, needs to be ignored.
			break;
		}
	}
	if (is_valid_nodepath) {
		variable_start = start;
	}
	return line.substr(variable_start, (end - variable_start));
}

void ProjectConverter3To4::rename_colors(Vector<SourceLine> &source_lines, const RegExContainer &reg_container) {
	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}

		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			if (line.contains("Color.")) {
				for (unsigned int current_index = 0; RenamesMap3To4::color_renames[current_index][0]; current_index++) {
					line = reg_container.color_regexes[current_index]->sub(line, reg_container.color_renamed[current_index], true);
				}
			}
		}
	}
}

// Convert hexadecimal colors from ARGB to RGBA
void ProjectConverter3To4::convert_hexadecimal_colors(Vector<SourceLine> &source_lines, const RegExContainer &reg_container) {
	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}

		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			if (line.contains("Color(\"")) {
				line = reg_container.color_hexadecimal_short_constructor.sub(line, "Color(\"#$2$1", true);
				line = reg_container.color_hexadecimal_full_constructor.sub(line, "Color(\"#$2$1", true);
			}
		}
	}
}

Vector<String> ProjectConverter3To4::check_for_rename_colors(Vector<String> &lines, const RegExContainer &reg_container) {
	Vector<String> found_renames;

	int current_line = 1;
	for (String &line : lines) {
		if (uint64_t(line.length()) <= maximum_line_length) {
			if (line.contains("Color.")) {
				for (unsigned int current_index = 0; RenamesMap3To4::color_renames[current_index][0]; current_index++) {
					TypedArray<RegExMatch> reg_match = reg_container.color_regexes[current_index]->search_all(line);
					if (reg_match.size() > 0) {
						found_renames.append(line_formatter(current_line, RenamesMap3To4::color_renames[current_index][0], RenamesMap3To4::color_renames[current_index][1], line));
					}
				}
			}
		}
		current_line++;
	}

	return found_renames;
}

void ProjectConverter3To4::fix_tool_declaration(Vector<SourceLine> &source_lines, const RegExContainer &reg_container) {
	// In godot4, "tool" became "@tool" and must be located at the top of the file.
	for (int i = 0; i < source_lines.size(); ++i) {
		if (source_lines[i].line == "tool") {
			source_lines.remove_at(i);
			source_lines.insert(0, { "@tool", false });
			return; // assuming there's at most 1 tool declaration.
		}
	}
}

void ProjectConverter3To4::fix_pause_mode(Vector<SourceLine> &source_lines, const RegExContainer &reg_container) {
	// In Godot 3, the pause_mode 2 equals the PAUSE_MODE_PROCESS value.
	// In Godot 4, the pause_mode PAUSE_MODE_PROCESS was renamed to PROCESS_MODE_ALWAYS and equals the number 3.
	// We therefore convert pause_mode = 2 to pause_mode = 3.
	for (SourceLine &source_line : source_lines) {
		String &line = source_line.line;

		if (line == "pause_mode = 2") {
			// Note: pause_mode is renamed to process_mode later on, so no need to do it here.
			line = "pause_mode = 3";
		}
	}
}

void ProjectConverter3To4::rename_classes(Vector<SourceLine> &source_lines, const RegExContainer &reg_container) {
	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}

		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			for (unsigned int current_index = 0; RenamesMap3To4::class_renames[current_index][0]; current_index++) {
				if (line.contains(RenamesMap3To4::class_renames[current_index][0])) {
					bool found_ignored_items = false;
					// Renaming Spatial.tscn to TEMP_RENAMED_CLASS.tscn.
					if (line.contains(String(RenamesMap3To4::class_renames[current_index][0]) + ".")) {
						found_ignored_items = true;
						line = reg_container.class_tscn_regexes[current_index]->sub(line, "TEMP_RENAMED_CLASS.tscn", true);
						line = reg_container.class_gd_regexes[current_index]->sub(line, "TEMP_RENAMED_CLASS.gd", true);
						line = reg_container.class_shader_regexes[current_index]->sub(line, "TEMP_RENAMED_CLASS.shader", true);
					}

					// Causal renaming Spatial -> Node3D.
					line = reg_container.class_regexes[current_index]->sub(line, RenamesMap3To4::class_renames[current_index][1], true);

					// Restore Spatial.tscn from TEMP_RENAMED_CLASS.tscn.
					if (found_ignored_items) {
						line = reg_container.class_temp_tscn.sub(line, reg_container.class_temp_tscn_renames[current_index], true);
						line = reg_container.class_temp_gd.sub(line, reg_container.class_temp_gd_renames[current_index], true);
						line = reg_container.class_temp_shader.sub(line, reg_container.class_temp_shader_renames[current_index], true);
					}
				}
			}
		}
	}
}

Vector<String> ProjectConverter3To4::check_for_rename_classes(Vector<String> &lines, const RegExContainer &reg_container) {
	Vector<String> found_renames;

	int current_line = 1;

	for (String &line : lines) {
		if (uint64_t(line.length()) <= maximum_line_length) {
			for (unsigned int current_index = 0; RenamesMap3To4::class_renames[current_index][0]; current_index++) {
				if (line.contains(RenamesMap3To4::class_renames[current_index][0])) {
					String old_line = line;
					bool found_ignored_items = false;
					// Renaming Spatial.tscn to TEMP_RENAMED_CLASS.tscn.
					if (line.contains(String(RenamesMap3To4::class_renames[current_index][0]) + ".")) {
						found_ignored_items = true;
						line = reg_container.class_tscn_regexes[current_index]->sub(line, "TEMP_RENAMED_CLASS.tscn", true);
						line = reg_container.class_gd_regexes[current_index]->sub(line, "TEMP_RENAMED_CLASS.gd", true);
						line = reg_container.class_shader_regexes[current_index]->sub(line, "TEMP_RENAMED_CLASS.shader", true);
					}

					// Causal renaming Spatial -> Node3D.
					TypedArray<RegExMatch> reg_match = reg_container.class_regexes[current_index]->search_all(line);
					if (reg_match.size() > 0) {
						found_renames.append(line_formatter(current_line, RenamesMap3To4::class_renames[current_index][0], RenamesMap3To4::class_renames[current_index][1], old_line));
					}

					// Restore Spatial.tscn from TEMP_RENAMED_CLASS.tscn.
					if (found_ignored_items) {
						line = reg_container.class_temp_tscn.sub(line, reg_container.class_temp_tscn_renames[current_index], true);
						line = reg_container.class_temp_gd.sub(line, reg_container.class_temp_gd_renames[current_index], true);
						line = reg_container.class_temp_shader.sub(line, reg_container.class_temp_shader_renames[current_index], true);
					}
				}
			}
		}
		current_line++;
	}
	return found_renames;
}

void ProjectConverter3To4::rename_gdscript_functions(Vector<SourceLine> &source_lines, const RegExContainer &reg_container, bool builtin) {
	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}

		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			process_gdscript_line(line, reg_container, builtin);
		}
	}
}

Vector<String> ProjectConverter3To4::check_for_rename_gdscript_functions(Vector<String> &lines, const RegExContainer &reg_container, bool builtin) {
	int current_line = 1;

	Vector<String> found_renames;

	for (String &line : lines) {
		if (uint64_t(line.length()) <= maximum_line_length) {
			String old_line = line;
			process_gdscript_line(line, reg_container, builtin);
			if (old_line != line) {
				found_renames.append(simple_line_formatter(current_line, old_line, line));
			}
		}
	}

	return found_renames;
}

bool ProjectConverter3To4::contains_function_call(const String &line, const String &function) const {
	// We want to convert the function only if it is completely standalone.
	// For example, when we search for "connect(", we don't want to accidentally convert "reconnect(".
	if (!line.contains(function)) {
		return false;
	}

	int index = line.find(function);
	if (index == 0) {
		return true;
	}

	char32_t previous_char = line.get(index - 1);
	return (previous_char < '0' || previous_char > '9') && (previous_char < 'a' || previous_char > 'z') && (previous_char < 'A' || previous_char > 'Z') && previous_char != '_' && previous_char != '$' && previous_char != '@';
}

// TODO, this function should run only on all ".gd" files and also on lines in ".tscn" files which are parts of built-in Scripts.
void ProjectConverter3To4::process_gdscript_line(String &line, const RegExContainer &reg_container, bool builtin) {
	// In this and other functions, reg.sub() is used only after checking lines with str.contains().
	// With longer lines, doing so can sometimes be significantly faster.

	if ((line.contains(".lock") || line.contains(".unlock")) && !line.contains("mtx") && !line.contains("mutex") && !line.contains("Mutex")) {
		line = reg_container.reg_image_lock.sub(line, "false # $1.lock() # TODOConverter3To4, Image no longer requires locking, `false` helps to not break one line if/else, so it can freely be removed", true);
		line = reg_container.reg_image_unlock.sub(line, "false # $1.unlock() # TODOConverter3To4, Image no longer requires locking, `false` helps to not break one line if/else, so it can freely be removed", true);
	}

	// PackedStringArray(req_godot).join('.') -> '.'.join(PackedStringArray(req_godot))       PoolStringArray
	if (line.contains(".join")) {
		line = reg_container.reg_join.sub(line, "$2.join($1)", true);
	}

	// -- empty() -> is_empty()       Pool*Array
	if (line.contains("empty")) {
		line = reg_container.reg_is_empty.sub(line, "is_empty(", true);
	}

	// -- \t.func() -> \tsuper.func()       Object
	if (line.contains_char('(') && line.contains_char('.')) {
		line = reg_container.reg_super.sub(line, "$1super.$2", true); // TODO, not sure if possible, but for now this broke String text e.g. "Chosen .gitignore" -> "Chosen super.gitignore"
	}

	// -- JSON.parse(a) -> JSON.new().parse(a) etc.    JSON
	if (line.contains("parse")) {
		line = reg_container.reg_json_non_new.sub(line, "$1var test_json_conv = JSON.new()\n$1test_json_conv.parse($3\n$1$2test_json_conv.get_data()", true);
	}

	// -- to_json(a) -> JSON.new().stringify(a)     Object
	if (line.contains("to_json")) {
		line = reg_container.reg_json_to.sub(line, "JSON.new().stringify", true);
	}
	// -- parse_json(a) -> JSON.get_data() etc.    Object
	if (line.contains("parse_json")) {
		line = reg_container.reg_json_parse.sub(line, "$1var test_json_conv = JSON.new()\n$1test_json_conv.parse($3\n$1$2test_json_conv.get_data()", true);
	}
	// -- JSON.print( -> JSON.stringify(
	if (line.contains("JSON.print(")) {
		line = reg_container.reg_json_print.sub(line, "JSON.stringify(", true);
	}

	// -- get_node(@ -> get_node(       Node
	if (line.contains("get_node")) {
		line = line.replace("get_node(@", "get_node(");
	}

	if (line.contains("export")) {
		// 1. export(float) var lifetime: float = 3.0 -> export var lifetime: float = 3.0
		line = reg_container.reg_export_typed.sub(line, "export var $2: $1");
		// 2. export(float) var lifetime := 3.0 -> export var lifetime := 3.0
		line = reg_container.reg_export_inferred_type.sub(line, "export var $1 :=");
		// 3. export(float) var lifetime = 3.0 -> export var lifetime: float = 3.0     GDScript
		line = reg_container.reg_export_simple.sub(line, "export var $2: $1");
		// 4. export(String, 'AnonymousPro', 'CourierPrime') var _font_name = 'AnonymousPro' -> export var _font_name = 'AnonymousPro' #(String, 'AnonymousPro', 'CourierPrime')   GDScript
		line = reg_container.reg_export_advanced.sub(line, "export var $2$3 # ($1)");
	}

	// Setget Setget
	if (line.contains("setget")) {
		line = reg_container.reg_setget_setget.sub(line, "var $1$2: get = $4, set = $3", true);
	}

	// Setget set
	if (line.contains("setget")) {
		line = reg_container.reg_setget_set.sub(line, "var $1$2: set = $3", true);
	}

	// Setget get
	if (line.contains("setget")) {
		line = reg_container.reg_setget_get.sub(line, "var $1$2: get = $3", true);
	}

	if (line.contains("window_resizable")) {
		// OS.set_window_resizable(a) -> get_window().unresizable = not (a)
		line = reg_container.reg_os_set_window_resizable.sub(line, "get_window().unresizable = not ($1)", true);
		// OS.window_resizable = a -> same
		line = reg_container.reg_os_assign_window_resizable.sub(line, "get_window().unresizable = not ($1)", true);
		// OS.[is_]window_resizable() -> (not get_window().unresizable)
		line = reg_container.reg_os_is_window_resizable.sub(line, "(not get_window().unresizable)", true);
	}

	if (line.contains("window_fullscreen")) {
		// OS.window_fullscreen(a) -> get_window().mode = Window.MODE_EXCLUSIVE_FULLSCREEN if (a) else Window.MODE_WINDOWED
		line = reg_container.reg_os_set_fullscreen.sub(line, "get_window().mode = Window.MODE_EXCLUSIVE_FULLSCREEN if ($1) else Window.MODE_WINDOWED", true);
		// window_fullscreen = a -> same
		line = reg_container.reg_os_assign_fullscreen.sub(line, "get_window().mode = Window.MODE_EXCLUSIVE_FULLSCREEN if ($1) else Window.MODE_WINDOWED", true);
		// OS.[is_]window_fullscreen() -> ((get_window().mode == Window.MODE_EXCLUSIVE_FULLSCREEN) or (get_window().mode == Window.MODE_FULLSCREEN))
		line = reg_container.reg_os_is_fullscreen.sub(line, "((get_window().mode == Window.MODE_EXCLUSIVE_FULLSCREEN) or (get_window().mode == Window.MODE_FULLSCREEN))", true);
	}

	if (line.contains("window_maximized")) {
		// OS.window_maximized(a) -> get_window().mode = Window.MODE_MAXIMIZED if (a) else Window.MODE_WINDOWED
		line = reg_container.reg_os_set_maximized.sub(line, "get_window().mode = Window.MODE_MAXIMIZED if ($1) else Window.MODE_WINDOWED", true);
		// window_maximized = a -> same
		line = reg_container.reg_os_assign_maximized.sub(line, "get_window().mode = Window.MODE_MAXIMIZED if ($1) else Window.MODE_WINDOWED", true);
		// OS.[is_]window_maximized() -> (get_window().mode == Window.MODE_MAXIMIZED)
		line = reg_container.reg_os_is_maximized.sub(line, "(get_window().mode == Window.MODE_MAXIMIZED)", true);
	}

	if (line.contains("window_minimized")) {
		// OS.window_minimized(a) -> get_window().mode = Window.MODE_MINIMIZED if (a) else Window.MODE_WINDOWED
		line = reg_container.reg_os_set_minimized.sub(line, "get_window().mode = Window.MODE_MINIMIZED if ($1) else Window.MODE_WINDOWED", true);
		// window_minimized = a -> same
		line = reg_container.reg_os_assign_minimized.sub(line, "get_window().mode = Window.MODE_MINIMIZED if ($1) else Window.MODE_WINDOWED", true);
		// OS.[is_]window_minimized() -> (get_window().mode == Window.MODE_MINIMIZED)
		line = reg_container.reg_os_is_minimized.sub(line, "(get_window().mode == Window.MODE_MINIMIZED)", true);
	}

	if (line.contains("set_use_vsync")) {
		// OS.set_use_vsync(a) -> get_window().window_set_vsync_mode(DisplayServer.VSYNC_ENABLED if (a) else DisplayServer.VSYNC_DISABLED)
		line = reg_container.reg_os_set_vsync.sub(line, "DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_ENABLED if ($1) else DisplayServer.VSYNC_DISABLED)", true);
	}
	if (line.contains("vsync_enabled")) {
		// vsync_enabled = a -> get_window().window_set_vsync_mode(DisplayServer.VSYNC_ENABLED if (a) else DisplayServer.VSYNC_DISABLED)
		line = reg_container.reg_os_assign_vsync.sub(line, "DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_ENABLED if ($1) else DisplayServer.VSYNC_DISABLED)", true);
		// OS.[is_]vsync_enabled() -> (DisplayServer.window_get_vsync_mode() != DisplayServer.VSYNC_DISABLED)
		line = reg_container.reg_os_is_vsync.sub(line, "(DisplayServer.window_get_vsync_mode() != DisplayServer.VSYNC_DISABLED)", true);
	}

	if (line.contains("OS.screen_orientation")) { // keep "OS." at start
		// OS.screen_orientation = a -> DisplayServer.screen_set_orientation(a)
		line = reg_container.reg_os_assign_screen_orient.sub(line, "$1DisplayServer.screen_set_orientation($2)", true); // assignment
		line = line.replace("OS.screen_orientation", "DisplayServer.screen_get_orientation()"); // value access
	}

	if (line.contains("_window_always_on_top")) {
		// OS.set_window_always_on_top(a) -> get_window().always_on_top = (a)
		line = reg_container.reg_os_set_always_on_top.sub(line, "get_window().always_on_top = ($1)", true);
		// OS.is_window_always_on_top() -> get_window().always_on_top
		line = reg_container.reg_os_is_always_on_top.sub(line, "get_window().always_on_top", true);
	}

	if (line.contains("et_borderless_window")) {
		// OS.set_borderless_window(a) -> get_window().borderless = (a)
		line = reg_container.reg_os_set_borderless.sub(line, "get_window().borderless = ($1)", true);
		// OS.get_borderless_window() -> get_window().borderless
		line = reg_container.reg_os_get_borderless.sub(line, "get_window().borderless", true);
	}

	// OS.SCREEN_ORIENTATION_* -> DisplayServer.SCREEN_*
	if (line.contains("OS.SCREEN_ORIENTATION_")) {
		line = reg_container.reg_os_screen_orient_enum.sub(line, "DisplayServer.SCREEN_$1", true);
	}

	// OS -> Window simple replacements with optional set/get.
	if (line.contains("current_screen")) {
		line = reg_container.reg_os_current_screen.sub(line, "get_window().$1current_screen", true);
	}
	if (line.contains("min_window_size")) {
		line = reg_container.reg_os_min_window_size.sub(line, "get_window().$1min_size", true);
	}
	if (line.contains("max_window_size")) {
		line = reg_container.reg_os_max_window_size.sub(line, "get_window().$1max_size", true);
	}
	if (line.contains("window_position")) {
		line = reg_container.reg_os_window_position.sub(line, "get_window().$1position", true);
	}
	if (line.contains("window_size")) {
		line = reg_container.reg_os_window_size.sub(line, "get_window().$1size", true);
	}
	if (line.contains("et_screen_orientation")) {
		line = reg_container.reg_os_getset_screen_orient.sub(line, "DisplayServer.screen_$1et_orientation", true);
	}

	// Instantiate
	if (contains_function_call(line, "instance")) {
		line = reg_container.reg_instantiate.sub(line, ".instantiate($1)", true);
	}

	// -- r.move_and_slide( a, b, c, d, e )  ->  r.set_velocity(a) ... r.move_and_slide()         KinematicBody
	if (contains_function_call(line, "move_and_slide(")) {
		int start = line.find("move_and_slide(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			String base_obj = get_object_of_execution(line.substr(0, start));
			String starting_space = get_starting_space(line);

			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() >= 1) {
				String line_new;

				// motion_velocity
				line_new += starting_space + base_obj + "set_velocity(" + parts[0] + ")\n";

				// up_direction
				if (parts.size() >= 2) {
					line_new += starting_space + base_obj + "set_up_direction(" + parts[1] + ")\n";
				}

				// stop_on_slope
				if (parts.size() >= 3) {
					line_new += starting_space + base_obj + "set_floor_stop_on_slope_enabled(" + parts[2] + ")\n";
				}

				// max_slides
				if (parts.size() >= 4) {
					line_new += starting_space + base_obj + "set_max_slides(" + parts[3] + ")\n";
				}

				// floor_max_angle
				if (parts.size() >= 5) {
					line_new += starting_space + base_obj + "set_floor_max_angle(" + parts[4] + ")\n";
				}

				// infiinite_interia
				if (parts.size() >= 6) {
					line_new += starting_space + "# TODOConverter3To4 infinite_inertia were removed in Godot 4 - previous value `" + parts[5] + "`\n";
				}

				line_new += starting_space + base_obj + "move_and_slide()";

				if (!line.begins_with(starting_space + "move_and_slide")) {
					line = line_new + "\n" + line.substr(0, start) + "velocity" + line.substr(end + start);
				} else {
					line = line_new + line.substr(end + start);
				}
			}
		}
	}

	// -- r.move_and_slide_with_snap( a, b, c, d, e )  ->  r.set_velocity(a) ... r.move_and_slide()         KinematicBody
	if (contains_function_call(line, "move_and_slide_with_snap(")) {
		int start = line.find("move_and_slide_with_snap(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			String base_obj = get_object_of_execution(line.substr(0, start));
			String starting_space = get_starting_space(line);

			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() >= 1) {
				String line_new;

				// motion_velocity
				line_new += starting_space + base_obj + "set_velocity(" + parts[0] + ")\n";

				// snap
				if (parts.size() >= 2) {
					line_new += starting_space + "# TODOConverter3To4 looks that snap in Godot 4 is float, not vector like in Godot 3 - previous value `" + parts[1] + "`\n";
				}

				// up_direction
				if (parts.size() >= 3) {
					line_new += starting_space + base_obj + "set_up_direction(" + parts[2] + ")\n";
				}

				// stop_on_slope
				if (parts.size() >= 4) {
					line_new += starting_space + base_obj + "set_floor_stop_on_slope_enabled(" + parts[3] + ")\n";
				}

				// max_slides
				if (parts.size() >= 5) {
					line_new += starting_space + base_obj + "set_max_slides(" + parts[4] + ")\n";
				}

				// floor_max_angle
				if (parts.size() >= 6) {
					line_new += starting_space + base_obj + "set_floor_max_angle(" + parts[5] + ")\n";
				}

				// infiinite_interia
				if (parts.size() >= 7) {
					line_new += starting_space + "# TODOConverter3To4 infinite_inertia were removed in Godot 4 - previous value `" + parts[6] + "`\n";
				}

				line_new += starting_space + base_obj + "move_and_slide()";

				if (!line.begins_with(starting_space + "move_and_slide_with_snap")) {
					line = line_new + "\n" + line.substr(0, start) + "velocity" + line.substr(end + start);
				} else {
					line = line_new + line.substr(end + start);
				}
			}
		}
	}

	// -- sort_custom( a , b )  ->  sort_custom(Callable( a , b ))            Object
	if (contains_function_call(line, "sort_custom(")) {
		int start = line.find("sort_custom(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 2) {
				line = line.substr(0, start) + "sort_custom(Callable(" + parts[0] + ", " + parts[1] + "))" + line.substr(end + start);
			}
		}
	}

	// -- list_dir_begin( )  ->  list_dir_begin()            Object
	if (contains_function_call(line, "list_dir_begin(")) {
		int start = line.find("list_dir_begin(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			line = line.substr(0, start) + "list_dir_begin() " + line.substr(end + start) + "# TODOConverter3To4 fill missing arguments https://github.com/godotengine/godot/pull/40547";
		}
	}

	// -- draw_line(1,2,3,4,5) -> draw_line(1, 2, 3, 4)            CanvasItem
	if (contains_function_call(line, "draw_line(")) {
		int start = line.find("draw_line(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 5) {
				line = line.substr(0, start) + "draw_line(" + parts[0] + ", " + parts[1] + ", " + parts[2] + ", " + parts[3] + ")" + line.substr(end + start);
			}
		}
	}

	// -- func c(var a, var b) -> func c(a, b)
	if (line.contains("func ") && line.contains("var ")) {
		int start = line.find("func ");
		start = line.substr(start).find_char('(') + start;
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));

			String start_string = line.substr(0, start) + "(";
			for (int i = 0; i < parts.size(); i++) {
				start_string += parts[i].strip_edges().trim_prefix("var ");
				if (i != parts.size() - 1) {
					start_string += ", ";
				}
			}
			line = start_string + ")" + line.substr(end + start);
		}
	}

	// -- yield(this, \"timeout\") -> await this.timeout         GDScript
	if (contains_function_call(line, "yield(")) {
		int start = line.find("yield(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 2) {
				if (builtin) {
					line = line.substr(0, start) + "await " + parts[0] + "." + parts[1].replace("\\\"", "").replace("\\'", "").replace(" ", "") + line.substr(end + start);
				} else {
					line = line.substr(0, start) + "await " + parts[0] + "." + parts[1].replace("\"", "").replace("\'", "").replace(" ", "") + line.substr(end + start);
				}
			}
		}
	}

	// -- parse_json( AA ) -> TODO       Object
	if (contains_function_call(line, "parse_json(")) {
		int start = line.find("parse_json(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			line = line.substr(0, start) + "JSON.new().stringify(" + connect_arguments(parts, 0) + ")" + line.substr(end + start);
		}
	}

	// -- .xform(Vector3(a,b,c)) -> * Vector3(a,b,c)            Transform
	if (line.contains(".xform(")) {
		int start = line.find(".xform(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 1) {
				line = line.substr(0, start) + " * (" + parts[0] + ")" + line.substr(end + start);
			}
		}
	}

	// -- .xform_inv(Vector3(a,b,c)) -> * Vector3(a,b,c)       Transform
	if (line.contains(".xform_inv(")) {
		int start = line.find(".xform_inv(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			String object_exec = get_object_of_execution(line.substr(0, start));
			if (line.contains(object_exec + ".xform")) {
				int start2 = line.find(object_exec + ".xform");
				Vector<String> parts = parse_arguments(line.substr(start, end));
				if (parts.size() == 1) {
					line = line.substr(0, start2) + "(" + parts[0] + ") * " + object_exec + line.substr(end + start);
				}
			}
		}
	}

	// -- "(connect(A,B,C,D,E) != OK):", "(connect(A, Callable(B, C).bind(D), E)      Object
	if (contains_function_call(line, "connect(")) {
		int start = line.find("connect(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 3) {
				line = line.substr(0, start) + "connect(" + parts[0] + ", Callable(" + parts[1] + ", " + parts[2] + "))" + line.substr(end + start);
			} else if (parts.size() >= 4) {
				line = line.substr(0, start) + "connect(" + parts[0] + ", Callable(" + parts[1] + ", " + parts[2] + ").bind(" + parts[3].lstrip(" [").rstrip("] ") + ")" + connect_arguments(parts, 4) + ")" + line.substr(end + start);
			}
		}
	}
	// -- disconnect(a,b,c) -> disconnect(a,Callable(b,c))      Object
	if (contains_function_call(line, "disconnect(")) {
		int start = line.find("disconnect(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 3) {
				line = line.substr(0, start) + "disconnect(" + parts[0] + ", Callable(" + parts[1] + ", " + parts[2] + "))" + line.substr(end + start);
			}
		}
	}
	// -- is_connected(a,b,c) -> is_connected(a,Callable(b,c))      Object
	if (contains_function_call(line, "is_connected(")) {
		int start = line.find("is_connected(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 3) {
				line = line.substr(0, start) + "is_connected(" + parts[0] + ", Callable(" + parts[1] + ", " + parts[2] + "))" + line.substr(end + start);
			}
		}
	}
	// -- "(tween_method(A,B,C,D,E) != OK):", "(tween_method(Callable(A,B),C,D,E)      Object
	// -- "(tween_method(A,B,C,D,E,[F,G]) != OK):", "(tween_method(Callable(A,B).bind(F,G),C,D,E)      Object
	if (contains_function_call(line, "tween_method(")) {
		int start = line.find("tween_method(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 5) {
				line = line.substr(0, start) + "tween_method(Callable(" + parts[0] + ", " + parts[1] + "), " + parts[2] + ", " + parts[3] + ", " + parts[4] + ")" + line.substr(end + start);
			} else if (parts.size() >= 6) {
				line = line.substr(0, start) + "tween_method(Callable(" + parts[0] + ", " + parts[1] + ").bind(" + connect_arguments(parts, 5).substr(1).lstrip(" [").rstrip("] ") + "), " + parts[2] + ", " + parts[3] + ", " + parts[4] + ")" + line.substr(end + start);
			}
		}
	}
	// -- "(tween_callback(A,B,[C,D]) != OK):", "(connect(Callable(A,B).bind(C,D))      Object
	if (contains_function_call(line, "tween_callback(")) {
		int start = line.find("tween_callback(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 2) {
				line = line.substr(0, start) + "tween_callback(Callable(" + parts[0] + ", " + parts[1] + "))" + line.substr(end + start);
			} else if (parts.size() >= 3) {
				line = line.substr(0, start) + "tween_callback(Callable(" + parts[0] + ", " + parts[1] + ").bind(" + connect_arguments(parts, 2).substr(1).lstrip(" [").rstrip("] ") + "))" + line.substr(end + start);
			}
		}
	}
	// -- start(a,b) -> start(Callable(a, b))      Thread
	// -- start(a,b,c,d) -> start(Callable(a, b).bind(c), d)      Thread
	if (contains_function_call(line, "start(")) {
		int start = line.find("start(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		// Protection from 'func start'
		if (!line.begins_with("func ")) {
			if (end > -1) {
				Vector<String> parts = parse_arguments(line.substr(start, end));
				if (parts.size() == 2) {
					line = line.substr(0, start) + "start(Callable(" + parts[0] + ", " + parts[1] + "))" + line.substr(end + start);
				} else if (parts.size() >= 3) {
					line = line.substr(0, start) + "start(Callable(" + parts[0] + ", " + parts[1] + ").bind(" + parts[2] + ")" + connect_arguments(parts, 3) + ")" + line.substr(end + start);
				}
			}
		}
	}
	// -- func _init(p_x:int).(p_x):  -> func _init(p_x:int):\n\tsuper(p_x)    Object # https://github.com/godotengine/godot/issues/70542
	if (line.contains(" _init(") && line.rfind_char(':') > 0) {
		//     func _init(p_arg1).(super4, super5, super6)->void:
		// ^--^indent            ^super_start   super_end^
		int indent = line.count("\t", 0, line.find("func"));
		int super_start = line.find(".(");
		int super_end = line.rfind_char(')');
		if (super_start > 0 && super_end > super_start) {
			line = line.substr(0, super_start) + line.substr(super_end + 1) + "\n" + String("\t").repeat(indent + 1) + "super" + line.substr(super_start + 1, super_end - super_start);
		}
	}

	//  create_from_image(aa, bb)  ->   create_from_image(aa) #, bb   ImageTexture
	if (contains_function_call(line, "create_from_image(")) {
		int start = line.find("create_from_image(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 2) {
				line = line.substr(0, start) + "create_from_image(" + parts[0] + ") " + "#," + parts[1] + line.substr(end + start);
			}
		}
	}
	//  set_cell_item(a, b, c, d ,e)  ->   set_cell_item(Vector3(a, b, c), d ,e)
	if (contains_function_call(line, "set_cell_item(")) {
		int start = line.find("set_cell_item(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() > 2) {
				line = line.substr(0, start) + "set_cell_item(Vector3(" + parts[0] + ", " + parts[1] + ", " + parts[2] + ")" + connect_arguments(parts, 3).lstrip(" ") + ")" + line.substr(end + start);
			}
		}
	}
	//  get_cell_item(a, b, c)  ->   get_cell_item(Vector3i(a, b, c))
	if (contains_function_call(line, "get_cell_item(")) {
		int start = line.find("get_cell_item(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 3) {
				line = line.substr(0, start) + "get_cell_item(Vector3i(" + parts[0] + ", " + parts[1] + ", " + parts[2] + "))" + line.substr(end + start);
			}
		}
	}
	//  get_cell_item_orientation(a, b, c)  ->   get_cell_item_orientation(Vector3i(a, b, c))
	if (contains_function_call(line, "get_cell_item_orientation(")) {
		int start = line.find("get_cell_item_orientation(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 3) {
				line = line.substr(0, start) + "get_cell_item_orientation(Vector3i(" + parts[0] + ", " + parts[1] + ", " + parts[2] + "))" + line.substr(end + start);
			}
		}
	}
	//  apply_impulse(A, B)  ->   apply_impulse(B, A)
	if (contains_function_call(line, "apply_impulse(")) {
		int start = line.find("apply_impulse(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 2) {
				line = line.substr(0, start) + "apply_impulse(" + parts[1] + ", " + parts[0] + ")" + line.substr(end + start);
			}
		}
	}
	//  apply_force(A, B)  ->   apply_force(B, A)
	if (contains_function_call(line, "apply_force(")) {
		int start = line.find("apply_force(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 2) {
				line = line.substr(0, start) + "apply_force(" + parts[1] + ", " + parts[0] + ")" + line.substr(end + start);
			}
		}
	}
	//  map_to_world(a, b, c)  ->   map_to_local(Vector3i(a, b, c))
	if (contains_function_call(line, "map_to_world(")) {
		int start = line.find("map_to_world(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 3) {
				line = line.substr(0, start) + "map_to_local(Vector3i(" + parts[0] + ", " + parts[1] + ", " + parts[2] + "))" + line.substr(end + start);
			} else if (parts.size() == 1) {
				line = line.substr(0, start) + "map_to_local(" + parts[0] + ")" + line.substr(end + start);
			}
		}
	}

	//  set_rotating(true)  ->   set_ignore_rotation(false)
	if (contains_function_call(line, "set_rotating(")) {
		int start = line.find("set_rotating(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 1) {
				String opposite = parts[0] == "true" ? "false" : "true";
				line = line.substr(0, start) + "set_ignore_rotation(" + opposite + ")";
			}
		}
	}

	//  OS.get_window_safe_area()  ->   DisplayServer.get_display_safe_area()
	if (line.contains("OS.get_window_safe_area(")) {
		int start = line.find("OS.get_window_safe_area(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 0) {
				line = line.substr(0, start) + "DisplayServer.get_display_safe_area()" + line.substr(end + start);
			}
		}
	}
	//  draw_rect(a,b,c,d,e)  ->   draw_rect(a,b,c,d)#e) TODOConverter3To4 Antialiasing argument is missing
	if (contains_function_call(line, "draw_rect(")) {
		int start = line.find("draw_rect(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 5) {
				line = line.substr(0, start) + "draw_rect(" + parts[0] + ", " + parts[1] + ", " + parts[2] + ", " + parts[3] + ")" + line.substr(end + start) + "# " + parts[4] + ") TODOConverter3To4 Antialiasing argument is missing";
			}
		}
	}
	// get_focus_owner() -> get_viewport().gui_get_focus_owner()
	if (contains_function_call(line, "get_focus_owner()")) {
		line = line.replace("get_focus_owner()", "get_viewport().gui_get_focus_owner()");
	}

	// button.pressed = 1 -> button.button_pressed = 1
	if (line.contains(".pressed")) {
		int start = line.find(".pressed");
		bool foundNextEqual = false;
		String line_to_check = line.substr(start + String(".pressed").length());
		for (int current_index = 0; line_to_check.length() > current_index; current_index++) {
			char32_t chr = line_to_check.get(current_index);
			if (chr == '\t' || chr == ' ') {
				continue;
			} else if (chr == '=') {
				foundNextEqual = true;
			} else {
				break;
			}
		}
		if (foundNextEqual) {
			line = line.substr(0, start) + ".button_pressed" + line.substr(start + String(".pressed").length());
		}
	}

	// rotating = true  ->   ignore_rotation = false # reversed "rotating" for Camera2D
	if (contains_function_call(line, "rotating")) {
		String function_name = "rotating";
		int start = line.find(function_name);
		bool foundNextEqual = false;
		String line_to_check = line.substr(start + function_name.length());
		String assigned_value;
		for (int current_index = 0; line_to_check.length() > current_index; current_index++) {
			char32_t chr = line_to_check.get(current_index);
			if (chr == '\t' || chr == ' ') {
				continue;
			} else if (chr == '=') {
				foundNextEqual = true;
				assigned_value = line.substr(start + function_name.length() + current_index + 1).strip_edges();
				assigned_value = assigned_value == "true" ? "false" : "true";
			} else {
				break;
			}
		}
		if (foundNextEqual) {
			line = line.substr(0, start) + "ignore_rotation = " + assigned_value + " # reversed \"rotating\" for Camera2D";
		}
	}

	// OS -> Time functions
	if (line.contains("OS.get_ticks_msec")) {
		line = line.replace("OS.get_ticks_msec", "Time.get_ticks_msec");
	}
	if (line.contains("OS.get_ticks_usec")) {
		line = line.replace("OS.get_ticks_usec", "Time.get_ticks_usec");
	}
	if (line.contains("OS.get_unix_time")) {
		line = line.replace("OS.get_unix_time", "Time.get_unix_time_from_system");
	}
	if (line.contains("OS.get_datetime")) {
		line = line.replace("OS.get_datetime", "Time.get_datetime_dict_from_system");
	}

	// OS -> DisplayServer
	if (line.contains("OS.get_display_cutouts")) {
		line = line.replace("OS.get_display_cutouts", "DisplayServer.get_display_cutouts");
	}
	if (line.contains("OS.get_screen_count")) {
		line = line.replace("OS.get_screen_count", "DisplayServer.get_screen_count");
	}
	if (line.contains("OS.get_screen_dpi")) {
		line = line.replace("OS.get_screen_dpi", "DisplayServer.screen_get_dpi");
	}
	if (line.contains("OS.get_screen_max_scale")) {
		line = line.replace("OS.get_screen_max_scale", "DisplayServer.screen_get_max_scale");
	}
	if (line.contains("OS.get_screen_position")) {
		line = line.replace("OS.get_screen_position", "DisplayServer.screen_get_position");
	}
	if (line.contains("OS.get_screen_refresh_rate")) {
		line = line.replace("OS.get_screen_refresh_rate", "DisplayServer.screen_get_refresh_rate");
	}
	if (line.contains("OS.get_screen_scale")) {
		line = line.replace("OS.get_screen_scale", "DisplayServer.screen_get_scale");
	}
	if (line.contains("OS.get_screen_size")) {
		line = line.replace("OS.get_screen_size", "DisplayServer.screen_get_size");
	}
	if (line.contains("OS.set_icon")) {
		line = line.replace("OS.set_icon", "DisplayServer.set_icon");
	}
	if (line.contains("OS.set_native_icon")) {
		line = line.replace("OS.set_native_icon", "DisplayServer.set_native_icon");
	}

	// OS -> Window
	if (line.contains("OS.window_borderless")) {
		line = line.replace("OS.window_borderless", "get_window().borderless");
	}
	if (line.contains("OS.get_real_window_size")) {
		line = line.replace("OS.get_real_window_size", "get_window().get_size_with_decorations");
	}
	if (line.contains("OS.is_window_focused")) {
		line = line.replace("OS.is_window_focused", "get_window().has_focus");
	}
	if (line.contains("OS.move_window_to_foreground")) {
		line = line.replace("OS.move_window_to_foreground", "get_window().grab_focus");
	}
	if (line.contains("OS.request_attention")) {
		line = line.replace("OS.request_attention", "get_window().request_attention");
	}
	if (line.contains("OS.set_window_title")) {
		line = line.replace("OS.set_window_title", "get_window().set_title");
	}

	// get_tree().set_input_as_handled() -> get_viewport().set_input_as_handled()
	if (line.contains("get_tree().set_input_as_handled()")) {
		line = line.replace("get_tree().set_input_as_handled()", "get_viewport().set_input_as_handled()");
	}

	// Fix the simple case of using _unhandled_key_input
	// func _unhandled_key_input(event: InputEventKey) -> _unhandled_key_input(event: InputEvent)
	if (line.contains("_unhandled_key_input(event: InputEventKey)")) {
		line = line.replace("_unhandled_key_input(event: InputEventKey)", "_unhandled_key_input(event: InputEvent)");
	}

	if (line.contains("Engine.editor_hint")) {
		line = line.replace("Engine.editor_hint", "Engine.is_editor_hint()");
	}
}

void ProjectConverter3To4::process_csharp_line(String &line, const RegExContainer &reg_container) {
	line = line.replace("OS.GetWindowSafeArea()", "DisplayServer.ScreenGetUsableRect()");

	// GetTree().SetInputAsHandled() -> GetViewport().SetInputAsHandled()
	if (line.contains("GetTree().SetInputAsHandled()")) {
		line = line.replace("GetTree().SetInputAsHandled()", "GetViewport().SetInputAsHandled()");
	}

	// Fix the simple case of using _UnhandledKeyInput
	// func _UnhandledKeyInput(InputEventKey @event) -> _UnhandledKeyInput(InputEvent @event)
	if (line.contains("_UnhandledKeyInput(InputEventKey @event)")) {
		line = line.replace("_UnhandledKeyInput(InputEventKey @event)", "_UnhandledKeyInput(InputEvent @event)");
	}

	// -- Connect(,,,things) -> Connect(,Callable(,),things)      Object
	if (line.contains("Connect(")) {
		int start = line.find("Connect(");
		// Protection from disconnect
		if (start == 0 || line.get(start - 1) != 's') {
			int end = get_end_parenthesis(line.substr(start)) + 1;
			if (end > -1) {
				Vector<String> parts = parse_arguments(line.substr(start, end));
				if (parts.size() >= 3) {
					line = line.substr(0, start) + "Connect(" + parts[0] + ", new Callable(" + parts[1] + ", " + parts[2] + ")" + connect_arguments(parts, 3) + ")" + line.substr(end + start);
				}
			}
		}
	}
	// -- Disconnect(a,b,c) -> Disconnect(a,Callable(b,c))      Object
	if (line.contains("Disconnect(")) {
		int start = line.find("Disconnect(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 3) {
				line = line.substr(0, start) + "Disconnect(" + parts[0] + ", new Callable(" + parts[1] + ", " + parts[2] + "))" + line.substr(end + start);
			}
		}
	}
	// -- IsConnected(a,b,c) -> IsConnected(a,Callable(b,c))      Object
	if (line.contains("IsConnected(")) {
		int start = line.find("IsConnected(");
		int end = get_end_parenthesis(line.substr(start)) + 1;
		if (end > -1) {
			Vector<String> parts = parse_arguments(line.substr(start, end));
			if (parts.size() == 3) {
				line = line.substr(0, start) + "IsConnected(" + parts[0] + ", new Callable(" + parts[1] + ", " + parts[2] + "))" + line.substr(end + start);
			}
		}
	}
}

void ProjectConverter3To4::rename_csharp_functions(Vector<SourceLine> &source_lines, const RegExContainer &reg_container) {
	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}

		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			process_csharp_line(line, reg_container);
		}
	}
}

Vector<String> ProjectConverter3To4::check_for_rename_csharp_functions(Vector<String> &lines, const RegExContainer &reg_container) {
	int current_line = 1;

	Vector<String> found_renames;

	for (String &line : lines) {
		if (uint64_t(line.length()) <= maximum_line_length) {
			String old_line = line;
			process_csharp_line(line, reg_container);
			if (old_line != line) {
				found_renames.append(simple_line_formatter(current_line, old_line, line));
			}
		}
	}

	return found_renames;
}

void ProjectConverter3To4::rename_csharp_attributes(Vector<SourceLine> &source_lines, const RegExContainer &reg_container) {
	static String error_message = "The master and mastersync rpc behavior is not officially supported anymore. Try using another keyword or making custom logic using Multiplayer.GetRemoteSenderId()\n";

	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}

		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			line = reg_container.keyword_csharp_remote.sub(line, "[RPC(MultiplayerAPI.RPCMode.AnyPeer)]", true);
			line = reg_container.keyword_csharp_remotesync.sub(line, "[RPC(MultiplayerAPI.RPCMode.AnyPeer, CallLocal = true)]", true);
			line = reg_container.keyword_csharp_puppet.sub(line, "[RPC]", true);
			line = reg_container.keyword_csharp_puppetsync.sub(line, "[RPC(CallLocal = true)]", true);
			line = reg_container.keyword_csharp_master.sub(line, error_message + "[RPC]", true);
			line = reg_container.keyword_csharp_mastersync.sub(line, error_message + "[RPC(CallLocal = true)]", true);
		}
	}
}

Vector<String> ProjectConverter3To4::check_for_rename_csharp_attributes(Vector<String> &lines, const RegExContainer &reg_container) {
	int current_line = 1;

	Vector<String> found_renames;

	for (String &line : lines) {
		if (uint64_t(line.length()) <= maximum_line_length) {
			String old;
			old = line;
			line = reg_container.keyword_csharp_remote.sub(line, "[RPC(MultiplayerAPI.RPCMode.AnyPeer)]", true);
			if (old != line) {
				found_renames.append(line_formatter(current_line, "[Remote]", "[RPC(MultiplayerAPI.RPCMode.AnyPeer)]", line));
			}

			old = line;
			line = reg_container.keyword_csharp_remotesync.sub(line, "[RPC(MultiplayerAPI.RPCMode.AnyPeer, CallLocal = true)]", true);
			if (old != line) {
				found_renames.append(line_formatter(current_line, "[RemoteSync]", "[RPC(MultiplayerAPI.RPCMode.AnyPeer, CallLocal = true)]", line));
			}

			old = line;
			line = reg_container.keyword_csharp_puppet.sub(line, "[RPC]", true);
			if (old != line) {
				found_renames.append(line_formatter(current_line, "[Puppet]", "[RPC]", line));
			}

			old = line;
			line = reg_container.keyword_csharp_puppetsync.sub(line, "[RPC(CallLocal = true)]", true);
			if (old != line) {
				found_renames.append(line_formatter(current_line, "[PuppetSync]", "[RPC(CallLocal = true)]", line));
			}

			old = line;
			line = reg_container.keyword_csharp_master.sub(line, "[RPC]", true);
			if (old != line) {
				found_renames.append(line_formatter(current_line, "[Master]", "[RPC]", line));
			}

			old = line;
			line = reg_container.keyword_csharp_mastersync.sub(line, "[RPC(CallLocal = true)]", true);
			if (old != line) {
				found_renames.append(line_formatter(current_line, "[MasterSync]", "[RPC(CallLocal = true)]", line));
			}
		}
		current_line++;
	}

	return found_renames;
}

_FORCE_INLINE_ static String builtin_escape(const String &p_str, bool p_builtin) {
	if (p_builtin) {
		return p_str.replace("\"", "\\\"");
	} else {
		return p_str;
	}
}

void ProjectConverter3To4::rename_gdscript_keywords(Vector<SourceLine> &source_lines, const RegExContainer &reg_container, bool builtin) {
	static String error_message = "The master and mastersync rpc behavior is not officially supported anymore. Try using another keyword or making custom logic using get_multiplayer().get_remote_sender_id()\n";

	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}

		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			if (line.contains("export")) {
				line = reg_container.keyword_gdscript_export_single.sub(line, "@export", true);
			}
			if (line.contains("export")) {
				line = reg_container.keyword_gdscript_export_multi.sub(line, "$1@export", true);
			}
			if (line.contains("onready")) {
				line = reg_container.keyword_gdscript_onready.sub(line, "@onready", true);
			}
			if (line.contains("remote")) {
				line = reg_container.keyword_gdscript_remote.sub(line, builtin_escape("@rpc(\"any_peer\") func", builtin), true);
			}
			if (line.contains("remote")) {
				line = reg_container.keyword_gdscript_remotesync.sub(line, builtin_escape("@rpc(\"any_peer\", \"call_local\") func", builtin), true);
			}
			if (line.contains("sync")) {
				line = reg_container.keyword_gdscript_sync.sub(line, builtin_escape("@rpc(\"any_peer\", \"call_local\") func", builtin), true);
			}
			if (line.contains("slave")) {
				line = reg_container.keyword_gdscript_slave.sub(line, "@rpc func", true);
			}
			if (line.contains("puppet")) {
				line = reg_container.keyword_gdscript_puppet.sub(line, "@rpc func", true);
			}
			if (line.contains("puppet")) {
				line = reg_container.keyword_gdscript_puppetsync.sub(line, builtin_escape("@rpc(\"call_local\") func", builtin), true);
			}
			if (line.contains("master")) {
				line = reg_container.keyword_gdscript_master.sub(line, error_message + "@rpc func", true);
			}
			if (line.contains("master")) {
				line = reg_container.keyword_gdscript_mastersync.sub(line, error_message + builtin_escape("@rpc(\"call_local\") func", builtin), true);
			}
		}
	}
}

Vector<String> ProjectConverter3To4::check_for_rename_gdscript_keywords(Vector<String> &lines, const RegExContainer &reg_container, bool builtin) {
	Vector<String> found_renames;

	int current_line = 1;
	for (String &line : lines) {
		if (uint64_t(line.length()) <= maximum_line_length) {
			String old;

			if (line.contains("tool")) {
				old = line;
				line = reg_container.keyword_gdscript_tool.sub(line, "@tool", true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "tool", "@tool", line));
				}
			}

			if (line.contains("export")) {
				old = line;
				line = reg_container.keyword_gdscript_export_single.sub(line, "$1@export", true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "export", "@export", line));
				}
			}

			if (line.contains("export")) {
				old = line;
				line = reg_container.keyword_gdscript_export_multi.sub(line, "@export", true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "export", "@export", line));
				}
			}

			if (line.contains("onready")) {
				old = line;
				line = reg_container.keyword_gdscript_tool.sub(line, "@onready", true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "onready", "@onready", line));
				}
			}

			if (line.contains("remote")) {
				old = line;
				line = reg_container.keyword_gdscript_remote.sub(line, builtin_escape("@rpc(\"any_peer\") func", builtin), true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "remote func", builtin_escape("@rpc(\"any_peer\") func", builtin), line));
				}
			}

			if (line.contains("remote")) {
				old = line;
				line = reg_container.keyword_gdscript_remotesync.sub(line, builtin_escape("@rpc(\"any_peer\", \"call_local\")) func", builtin), true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "remotesync func", builtin_escape("@rpc(\"any_peer\", \"call_local\")) func", builtin), line));
				}
			}

			if (line.contains("sync")) {
				old = line;
				line = reg_container.keyword_gdscript_sync.sub(line, builtin_escape("@rpc(\"any_peer\", \"call_local\")) func", builtin), true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "sync func", builtin_escape("@rpc(\"any_peer\", \"call_local\")) func", builtin), line));
				}
			}

			if (line.contains("slave")) {
				old = line;
				line = reg_container.keyword_gdscript_slave.sub(line, "@rpc func", true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "slave func", "@rpc func", line));
				}
			}

			if (line.contains("puppet")) {
				old = line;
				line = reg_container.keyword_gdscript_puppet.sub(line, "@rpc func", true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "puppet func", "@rpc func", line));
				}
			}

			if (line.contains("puppet")) {
				old = line;
				line = reg_container.keyword_gdscript_puppetsync.sub(line, builtin_escape("@rpc(\"call_local\") func", builtin), true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "puppetsync func", builtin_escape("@rpc(\"call_local\") func", builtin), line));
				}
			}

			if (line.contains("master")) {
				old = line;
				line = reg_container.keyword_gdscript_master.sub(line, "@rpc func", true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "master func", "@rpc func", line));
				}
			}

			if (line.contains("master")) {
				old = line;
				line = reg_container.keyword_gdscript_master.sub(line, builtin_escape("@rpc(\"call_local\") func", builtin), true);
				if (old != line) {
					found_renames.append(line_formatter(current_line, "mastersync func", builtin_escape("@rpc(\"call_local\") func", builtin), line));
				}
			}
		}
		current_line++;
	}

	return found_renames;
}

void ProjectConverter3To4::rename_input_map_scancode(Vector<SourceLine> &source_lines, const RegExContainer &reg_container) {
	// The old Special Key, now colliding with CMD_OR_CTRL.
	const int old_spkey = (1 << 24);

	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}

		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			TypedArray<RegExMatch> reg_match = reg_container.input_map_keycode.search_all(line);

			for (int i = 0; i < reg_match.size(); ++i) {
				Ref<RegExMatch> match = reg_match[i];
				PackedStringArray strings = match->get_strings();
				int key = strings[3].to_int();

				if (key & old_spkey) {
					// Create new key, clearing old Special Key and setting new one.
					key = (key & ~old_spkey) | (int)Key::SPECIAL;

					line = line.replace(strings[0], String(",\"") + strings[1] + "scancode\":" + String::num_int64(key));
				}
			}
		}
	}
}

void ProjectConverter3To4::rename_joypad_buttons_and_axes(Vector<SourceLine> &source_lines, const RegExContainer &reg_container) {
	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}
		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			// Remap button indexes.
			TypedArray<RegExMatch> reg_match = reg_container.joypad_button_index.search_all(line);
			for (int i = 0; i < reg_match.size(); ++i) {
				Ref<RegExMatch> match = reg_match[i];
				PackedStringArray strings = match->get_strings();
				const String &button_index_entry = strings[0];
				int button_index_value = strings[1].to_int();
				if (button_index_value == 6) { // L2 and R2 are mapped to joypad axes in Godot 4.
					line = line.replace("InputEventJoypadButton", "InputEventJoypadMotion");
					line = line.replace(button_index_entry, ",\"axis\":4,\"axis_value\":1.0");
				} else if (button_index_value == 7) {
					line = line.replace("InputEventJoypadButton", "InputEventJoypadMotion");
					line = line.replace(button_index_entry, ",\"axis\":5,\"axis_value\":1.0");
				} else if (button_index_value < 22) { // There are no mappings for indexes greater than 22 in both Godot 3 & 4.
					const String &pressure_and_pressed_properties = strings[2];
					line = line.replace(button_index_entry, ",\"button_index\":" + String::num_int64(reg_container.joypad_button_mappings[button_index_value]) + "," + pressure_and_pressed_properties);
				}
			}
			// Remap axes. Only L2 and R2 need remapping.
			reg_match = reg_container.joypad_axis.search_all(line);
			for (int i = 0; i < reg_match.size(); ++i) {
				Ref<RegExMatch> match = reg_match[i];
				PackedStringArray strings = match->get_strings();
				const String &axis_entry = strings[0];
				int axis_value = strings[1].to_int();
				if (axis_value == 6) {
					line = line.replace(axis_entry, ",\"axis\":4");
				} else if (axis_value == 7) {
					line = line.replace(axis_entry, ",\"axis\":5");
				}
			}
		}
	}
}

Vector<String> ProjectConverter3To4::check_for_rename_joypad_buttons_and_axes(Vector<String> &lines, const RegExContainer &reg_container) {
	Vector<String> found_renames;
	int current_line = 1;
	for (String &line : lines) {
		if (uint64_t(line.length()) <= maximum_line_length) {
			// Remap button indexes.
			TypedArray<RegExMatch> reg_match = reg_container.joypad_button_index.search_all(line);
			for (int i = 0; i < reg_match.size(); ++i) {
				Ref<RegExMatch> match = reg_match[i];
				PackedStringArray strings = match->get_strings();
				const String &button_index_entry = strings[0];
				int button_index_value = strings[1].to_int();
				if (button_index_value == 6) { // L2 and R2 are mapped to joypad axes in Godot 4.
					found_renames.append(line_formatter(current_line, "InputEventJoypadButton", "InputEventJoypadMotion", line));
					found_renames.append(line_formatter(current_line, button_index_entry, ",\"axis\":4", line));
				} else if (button_index_value == 7) {
					found_renames.append(line_formatter(current_line, "InputEventJoypadButton", "InputEventJoypadMotion", line));
					found_renames.append(line_formatter(current_line, button_index_entry, ",\"axis\":5", line));
				} else if (button_index_value < 22) { // There are no mappings for indexes greater than 22 in both Godot 3 & 4.
					found_renames.append(line_formatter(current_line, "\"button_index\":" + strings[1], "\"button_index\":" + String::num_int64(reg_container.joypad_button_mappings[button_index_value]), line));
				}
			}
			// Remap axes. Only L2 and R2 need remapping.
			reg_match = reg_container.joypad_axis.search_all(line);
			for (int i = 0; i < reg_match.size(); ++i) {
				Ref<RegExMatch> match = reg_match[i];
				PackedStringArray strings = match->get_strings();
				const String &axis_entry = strings[0];
				int axis_value = strings[1].to_int();
				if (axis_value == 6) {
					found_renames.append(line_formatter(current_line, axis_entry, ",\"axis\":4", line));
				} else if (axis_value == 7) {
					found_renames.append(line_formatter(current_line, axis_entry, ",\"axis\":5", line));
				}
			}
			current_line++;
		}
	}
	return found_renames;
}

Vector<String> ProjectConverter3To4::check_for_rename_input_map_scancode(Vector<String> &lines, const RegExContainer &reg_container) {
	Vector<String> found_renames;

	// The old Special Key, now colliding with CMD_OR_CTRL.
	const int old_spkey = (1 << 24);

	int current_line = 1;
	for (String &line : lines) {
		if (uint64_t(line.length()) <= maximum_line_length) {
			TypedArray<RegExMatch> reg_match = reg_container.input_map_keycode.search_all(line);

			for (int i = 0; i < reg_match.size(); ++i) {
				Ref<RegExMatch> match = reg_match[i];
				PackedStringArray strings = match->get_strings();
				int key = strings[3].to_int();

				if (key & old_spkey) {
					// Create new key, clearing old Special Key and setting new one.
					key = (key & ~old_spkey) | (int)Key::SPECIAL;

					found_renames.append(line_formatter(current_line, strings[3], String::num_int64(key), line));
				}
			}
		}
		current_line++;
	}
	return found_renames;
}

void ProjectConverter3To4::custom_rename(Vector<SourceLine> &source_lines, const String &from, const String &to) {
	RegEx reg = RegEx(String("\\b") + from + "\\b");
	CRASH_COND(!reg.is_valid());
	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}

		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			line = reg.sub(line, to, true);
		}
	}
}

Vector<String> ProjectConverter3To4::check_for_custom_rename(Vector<String> &lines, const String &from, const String &to) {
	Vector<String> found_renames;

	RegEx reg = RegEx(String("\\b") + from + "\\b");
	CRASH_COND(!reg.is_valid());

	int current_line = 1;
	for (String &line : lines) {
		if (uint64_t(line.length()) <= maximum_line_length) {
			TypedArray<RegExMatch> reg_match = reg.search_all(line);
			if (reg_match.size() > 0) {
				found_renames.append(line_formatter(current_line, from.replace("\\.", "."), to, line)); // Without replacing it will print "\.shader" instead ".shader".
			}
		}
		current_line++;
	}
	return found_renames;
}

void ProjectConverter3To4::rename_common(const char *array[][2], LocalVector<RegEx *> &cached_regexes, Vector<SourceLine> &source_lines) {
	for (SourceLine &source_line : source_lines) {
		if (source_line.is_comment) {
			continue;
		}

		String &line = source_line.line;
		if (uint64_t(line.length()) <= maximum_line_length) {
			for (unsigned int current_index = 0; current_index < cached_regexes.size(); current_index++) {
				if (line.contains(array[current_index][0])) {
					line = cached_regexes[current_index]->sub(line, array[current_index][1], true);
				}
			}
		}
	}
}

Vector<String> ProjectConverter3To4::check_for_rename_common(const char *array[][2], LocalVector<RegEx *> &cached_regexes, Vector<String> &lines) {
	Vector<String> found_renames;

	int current_line = 1;

	for (String &line : lines) {
		if (uint64_t(line.length()) <= maximum_line_length) {
			for (unsigned int current_index = 0; current_index < cached_regexes.size(); current_index++) {
				if (line.contains(array[current_index][0])) {
					TypedArray<RegExMatch> reg_match = cached_regexes[current_index]->search_all(line);
					if (reg_match.size() > 0) {
						found_renames.append(line_formatter(current_line, array[current_index][0], array[current_index][1], line));
					}
				}
			}
		}
		current_line++;
	}

	return found_renames;
}

// Prints full info about renamed things e.g.:
// Line (67) remove -> remove_at  -  LINE """ doubler._blacklist.remove(0) """
String ProjectConverter3To4::line_formatter(int current_line, String from, String to, String line) {
	if (from.size() > 200) {
		from = from.substr(0, 197) + "...";
	}
	if (to.size() > 200) {
		to = to.substr(0, 197) + "...";
	}
	if (line.size() > 400) {
		line = line.substr(0, 397) + "...";
	}

	from = from.strip_escapes();
	to = to.strip_escapes();
	line = line.replace("\r", "").replace("\n", "").strip_edges();

	return vformat("Line(%d), %s -> %s  -  LINE \"\"\" %s \"\"\"", current_line, from, to, line);
}

// Prints only full lines e.g.:
// Line (1) - FULL LINES - """yield(get_tree().create_timer(3), 'timeout')"""  =====>  """ await get_tree().create_timer(3).timeout """
String ProjectConverter3To4::simple_line_formatter(int current_line, String old_line, String new_line) {
	if (old_line.size() > 1000) {
		old_line = old_line.substr(0, 997) + "...";
	}
	if (new_line.size() > 1000) {
		new_line = new_line.substr(0, 997) + "...";
	}

	old_line = old_line.replace("\r", "").replace("\n", "").strip_edges();
	new_line = new_line.replace("\r", "").replace("\n", "").strip_edges();

	return vformat("Line (%d) - FULL LINES - \"\"\" %s \"\"\"  =====>  \"\"\" %s \"\"\"", current_line, old_line, new_line);
}

// Collects string from vector strings
String ProjectConverter3To4::collect_string_from_vector(Vector<SourceLine> &vector) {
	String string = "";
	for (int i = 0; i < vector.size(); i++) {
		string += vector[i].line;

		if (i != vector.size() - 1) {
			string += "\n";
		}
	}
	return string;
}

#endif // MODULE_REGEX_ENABLED

#endif // DISABLE_DEPRECATED
