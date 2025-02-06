/**************************************************************************/
/*  cpp_scons_gdext_creator.cpp                                           */
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

#include "cpp_scons_gdext_creator.h"

#include "core/core_bind.h"
#include "core/io/dir_access.h"
#include "core/string/string_builder.h"
#include "core/version.h"
#include "gdextension_template_files.gen.h"

#include "editor/editor_node.h"

void CppSconsGDExtensionCreator::_git_clone_godot_cpp(const String &p_parent_path, bool p_compile) {
	EditorProgress ep("Preparing GDExtension C++ plugin", "Preparing GDExtension C++ plugin", 3);
	List<String> args;
	args.push_back("clone");
	args.push_back("--single-branch");
	args.push_back("--branch");
	args.push_back(VERSION_BRANCH);
	args.push_back("https://github.com/godotengine/godot-cpp");
	const String godot_cpp_path = p_parent_path.trim_prefix("res://").path_join("godot-cpp");
	args.push_back(godot_cpp_path);
	ep.step(TTR("Cloning godot-cpp..."), 1);
	String output = "";
	int result = OS::get_singleton()->execute("git", args, &output);
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (result != 0 || !dir->dir_exists(godot_cpp_path)) {
		args.get(3) = "master";
		output = "";
		result = OS::get_singleton()->execute("git", args, &output);
	}
	ERR_FAIL_COND_MSG(result != 0 || !dir->dir_exists(godot_cpp_path), "Failed to clone godot-cpp. Please clone godot-cpp manually in order to have a working GDExtension plugin.");
	if (p_compile) {
		ep.step(TTR("Performing initial compile... (this may take several minutes)"), 2);
		result = OS::get_singleton()->execute("scons", List<String>());
		ERR_FAIL_COND_MSG(result != 0, "Failed to compile godot-cpp. Please ensure SCons is installed, then run the `scons` command in your project.");
	}
	ep.step(TTR("Done!"), 3);
}

String CppSconsGDExtensionCreator::_process_template(const String &p_contents) {
	String ret;
	if (strip_module_defines) {
		StringBuilder builder;
		bool keep = true;
		PackedStringArray lines = p_contents.split("\n");
		for (const String &line : lines) {
			if (line == "#if GDEXTENSION" || line == "#else") {
				continue;
			} else if (line == "#elif GODOT_MODULE") {
				keep = false;
				continue;
			} else if (line == "#endif") {
				keep = true;
				continue;
			}
			if (keep) {
				builder += line;
				builder += "\n";
			}
		}
		ret = builder.as_string();
	} else {
		ret = p_contents;
	}
	if (ClassDB::class_exists("ExampleNode")) {
		ret = ret.replace("ExampleNode", example_node_name);
	}
	ret = ret.replace("__BASE_NAME__", base_name);
	ret = ret.replace("__BASE_NAME_UPPER__", base_name.to_upper());
	ret = ret.replace("__LIBRARY_NAME__", library_name);
	ret = ret.replace("__LIBRARY_NAME_UPPER__", library_name.to_upper());
	ret = ret.replace("__GODOT_VERSION__", VERSION_BRANCH);
	ret = ret.replace("__BASE_PATH__", res_path.trim_prefix("res://"));
	ret = ret.replace("__UPDIR_DOTS__", updir_dots);
	return ret;
}

void CppSconsGDExtensionCreator::_write_file(const String &p_file_path, const String &p_contents) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_MSG(err != OK, "Couldn't write file at path: " + p_file_path + ".");
	file->store_string(_process_template(p_contents));
	file->close();
}

void CppSconsGDExtensionCreator::_ensure_file_contains(const String &p_file_path, const String &p_new_contents) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::READ_WRITE, &err);
	if (err != OK) {
		_write_file(p_file_path, p_new_contents);
		return;
	}
	String new_contents = _process_template(p_new_contents);
	String existing_contents = file->get_as_text();
	if (existing_contents.is_empty()) {
		file->store_string(new_contents);
	} else {
		file->seek_end();
		PackedStringArray lines = new_contents.split("\n", false);
		for (const String &line : lines) {
			if (!existing_contents.contains(line)) {
				file->store_string(line + "\n");
			}
		}
	}
	file->close();
}

void CppSconsGDExtensionCreator::_write_common_files_and_dirs() {
	DirAccess::make_dir_recursive_absolute(res_path.path_join("doc_classes"));
	DirAccess::make_dir_recursive_absolute(res_path.path_join("icons"));
	DirAccess::make_dir_recursive_absolute(res_path.path_join("src"));
	_ensure_file_contains("res://SConstruct", SCONSTRUCT_TOP_LEVEL);
	_write_file(res_path.path_join("doc_classes/" + example_node_name + ".xml"), EXAMPLENODE_XML);
	_write_file(res_path.path_join("icons/" + example_node_name + ".svg"), EXAMPLENODE_SVG);
	_write_file(res_path.path_join("icons/" + example_node_name + ".svg.import"), EXAMPLENODE_SVG_IMPORT);
	_write_file(res_path.path_join("src/.gdignore"), "");
	_write_file(res_path.path_join(".gitignore"), GDEXT_GITIGNORE + "\n*.obj");
	_write_file(res_path.path_join(library_name + ".gdextension"), LIBRARY_NAME_GDEXTENSION);
}

void CppSconsGDExtensionCreator::_write_gdext_only_files() {
	_ensure_file_contains("res://.gitignore", "*.dblite");
	_write_file(res_path.path_join("src/example_node.cpp"), EXAMPLE_NODE_CPP);
	_write_file(res_path.path_join("src/example_node.h"), EXAMPLE_NODE_H);
	_write_file(res_path.path_join("src/register_types.cpp"), REGISTER_TYPES_CPP);
	_write_file(res_path.path_join("src/register_types.h"), REGISTER_TYPES_H);
	_write_file(res_path.path_join("src/" + library_name + "_defines.h"), GDEXT_DEFINES_H);
	_write_file(res_path.path_join("src/initialize_gdextension.cpp"), INITIALIZE_GDEXTENSION_CPP.replace("#include \"__UPDIR_DOTS__/../", "#include \""));
	_write_file(res_path.path_join("SConstruct"), SCONSTRUCT_ADDON.replace(" + Glob(\"__UPDIR_DOTS__/*.cpp\")", "").replace(",__UPDIR_DOTS__/", "").replace("__UPDIR_DOTS__/editor", "src/editor"));
}

void CppSconsGDExtensionCreator::_write_gdext_module_files() {
	_ensure_file_contains("res://.gitignore", GDEXT_GITIGNORE);
	DirAccess::make_dir_recursive_absolute("res://tests");
	_write_file("res://SCsub", SCSUB);
	_write_file("res://config.py", CONFIG_PY);
	_write_file("res://example_node.cpp", EXAMPLE_NODE_CPP);
	_write_file("res://example_node.h", EXAMPLE_NODE_H);
	_write_file("res://register_types.cpp", REGISTER_TYPES_CPP);
	_write_file("res://register_types.h", REGISTER_TYPES_H);
	_write_file("res://" + library_name + "_defines.h", SHARED_DEFINES_H);
	_write_file("res://tests/test_" + base_name + ".h", TEST_BASE_NAME_H);
	_write_file("res://tests/test_example_node.h", TEST_EXAMPLE_NODE_H);
	_write_file(res_path.path_join("src/initialize_gdextension.cpp"), INITIALIZE_GDEXTENSION_CPP);
	_write_file(res_path.path_join("SConstruct"), SCONSTRUCT_ADDON);
}

void CppSconsGDExtensionCreator::create_gdextension(const String &p_path, const String &p_base_name, const String &p_library_name, int p_variation_index, bool p_compile) {
	res_path = p_path;
	base_name = p_base_name;
	library_name = p_library_name;
	updir_dots = String("../").repeat(p_path.count("/", 6)) + "..";
	strip_module_defines = p_variation_index == LANG_VAR_GDEXT_ONLY;
	if (ClassDB::class_exists("ExampleNode")) {
		int discriminator = 2;
		example_node_name = "ExampleNode2";
		while (ClassDB::class_exists(example_node_name)) {
			discriminator++;
			example_node_name = "ExampleNode" + itos(discriminator);
		}
	}
	_write_common_files_and_dirs();
	if (p_variation_index == LANG_VAR_GDEXT_ONLY) {
		_write_gdext_only_files();
	} else {
		_write_gdext_module_files();
	}
	if (does_git_exist) {
		_git_clone_godot_cpp(p_path.path_join("src"), p_compile);
	}
}

void CppSconsGDExtensionCreator::setup_creator() {
	// Check for Git and SCons.
	List<String> args;
	args.push_back("--version");
	String output;
	OS::get_singleton()->execute("git", args, &output);
	if (output.is_empty()) {
		does_git_exist = false;
	} else {
		does_git_exist = true;
		output = "";
		OS::get_singleton()->execute("scons", args, &output);
		does_scons_exist = !output.is_empty();
	}
}

PackedStringArray CppSconsGDExtensionCreator::get_language_variations() const {
	PackedStringArray variants;
	// Keep this in sync with enum LanguageVariation.
	variants.push_back("C++ with SCons, GDExtension only");
	variants.push_back("C++ with SCons, GDExtension and engine module");
	return variants;
}

Dictionary CppSconsGDExtensionCreator::get_validation_messages(const String &p_path, const String &p_base_name, const String &p_library_name, int p_variation_index, bool p_compile) {
	Dictionary messages;
	// Check for Git and SCons.
	MessageType compile_consequence = p_compile ? MSG_ERROR : MSG_WARNING;
	if (does_git_exist) {
		if (does_scons_exist) {
#ifdef WINDOWS_ENABLED
			messages[TTR("Both Git and SCons were found. You also need a C++17-compatible compiler, such as GCC, Clang/LLVM, or MSVC from Visual Studio.")] = MSG_OK;
#else
			messages[TTR("Both Git and SCons were found. You also need a C++17-compatible compiler, such as GCC or Clang/LLVM.")] = MSG_OK;
#endif
		} else {
			messages[TTR("Cannot compile now, SCons was not found.")] = compile_consequence;
		}
	} else {
		messages[TTR("Cannot compile now, Git was not found.")] = compile_consequence;
	}
	// Check for existing engine module.
	if (p_variation_index == LANG_VAR_GDEXT_MODULE) {
		Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (dir->file_exists("SCsub")) {
			messages[TTR("This project already contains a C++ engine module.")] = MSG_ERROR;
		} else {
			messages[TTR("Able to create engine module in this Godot project.")] = MSG_OK;
			messages[TTR("Warning: This will turn the root of your project into an engine module!")] = MSG_WARNING;
		}
	}
	return messages;
}
