/**************************************************************************/
/*  cpp_scons_gdext_creator.h                                             */
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

#ifndef CPP_SCONS_GDEXT_CREATOR_H
#define CPP_SCONS_GDEXT_CREATOR_H

#include "../gdextension_creator_plugin.h"

class CppSconsGDExtensionCreator : public GDExtensionCreatorPlugin {
	// Keep this in sync with get_language_variations().
	enum LanguageVariation {
		LANG_VAR_GDEXT_ONLY,
		LANG_VAR_GDEXT_MODULE,
	};

	// Used by _process_template.
	String base_name;
	String library_name;
	String example_node_name = "ExampleNode";
	String res_path;
	String updir_dots;
	bool strip_module_defines = false;

	bool does_git_exist = false;
	bool does_scons_exist = false;

	void _git_clone_godot_cpp(const String &p_parent_path, bool p_compile);
	String _process_template(const String &p_contents);
	void _write_common_files_and_dirs();
	void _write_gdext_only_files();
	void _write_gdext_module_files();
	void _write_file(const String &p_file_path, const String &p_contents);
	void _ensure_file_contains(const String &p_file_path, const String &p_new_contents);

public:
	void create_gdextension(const String &p_path, const String &p_base_name, const String &p_library_name, int p_variation_index, bool p_compile) override;
	void setup_creator() override;
	PackedStringArray get_language_variations() const override;
	Dictionary get_validation_messages(const String &p_path, const String &p_base_name, const String &p_library_name, int p_variation_index, bool p_compile) override;
};

#endif // CPP_SCONS_GDEXT_CREATOR_H
