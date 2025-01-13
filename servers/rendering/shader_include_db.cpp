/**************************************************************************/
/*  shader_include_db.cpp                                                 */
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

#include "shader_include_db.h"

HashMap<String, String> ShaderIncludeDB::built_in_includes;

void ShaderIncludeDB::_bind_methods() {
	ClassDB::bind_static_method("ShaderIncludeDB", D_METHOD("list_built_in_include_files"), &ShaderIncludeDB::list_built_in_include_files);
	ClassDB::bind_static_method("ShaderIncludeDB", D_METHOD("has_built_in_include_file", "filename"), &ShaderIncludeDB::has_built_in_include_file);
	ClassDB::bind_static_method("ShaderIncludeDB", D_METHOD("get_built_in_include_file", "filename"), &ShaderIncludeDB::get_built_in_include_file);
}

void ShaderIncludeDB::register_built_in_include_file(const String &p_filename, const String &p_shader_code) {
	built_in_includes[p_filename] = p_shader_code;
}

PackedStringArray ShaderIncludeDB::list_built_in_include_files() {
	PackedStringArray ret;

	for (const KeyValue<String, String> &e : built_in_includes) {
		ret.push_back(e.key);
	}

	return ret;
}

bool ShaderIncludeDB::has_built_in_include_file(const String &p_filename) {
	return built_in_includes.has(p_filename);
}

String ShaderIncludeDB::get_built_in_include_file(const String &p_filename) {
	const String *ptr = built_in_includes.getptr(p_filename);

	return ptr ? *ptr : String();
}

String ShaderIncludeDB::parse_include_files(const String &p_code) {
	// Prevent needless processing if we don't have any includes.
	if (p_code.find("#include ") == -1) {
		return p_code;
	}

	const String include = "#include ";
	String parsed_code;

	Vector<String> lines = p_code.split("\n");
	int line_count = lines.size();
	for (int i = 0; i < line_count; i++) {
		const String &l = lines[i];

		if (l.begins_with(include)) {
			String include_file = l.replace(include, "").strip_edges();
			if (include_file[0] == '"') {
				int end_pos = include_file.find_char('"', 1);
				if (end_pos >= 0) {
					include_file = include_file.substr(1, end_pos - 1);

					String include_code = ShaderIncludeDB::get_built_in_include_file(include_file);
					if (!include_code.is_empty()) {
						// Add these lines into our parse list so we parse them as well.
						Vector<String> include_lines = include_code.split("\n");

						for (int j = include_lines.size() - 1; j >= 0; j--) {
							lines.insert(i + 1, include_lines[j]);
						}

						line_count = lines.size();
					} else {
						// Just add it back in, this will cause a compile error to alert the user.
						parsed_code += l + "\n";
					}
				} else {
					// Include as is.
					parsed_code += l + "\n";
				}
			} else {
				// Include as is.
				parsed_code += l + "\n";
			}
		} else {
			// Include as is.
			parsed_code += l + "\n";
		}
	}

	return parsed_code;
}
