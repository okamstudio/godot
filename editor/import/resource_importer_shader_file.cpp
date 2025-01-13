/**************************************************************************/
/*  resource_importer_shader_file.cpp                                     */
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

#include "resource_importer_shader_file.h"

#include "core/io/file_access.h"
#include "core/io/marshalls.h"
#include "core/io/resource_saver.h"
#include "editor/editor_node.h"
#include "editor/plugins/shader_file_editor_plugin.h"
#include "servers/rendering/rendering_device_binds.h"

String ResourceImporterShaderFile::get_importer_name() const {
	return "glsl";
}

String ResourceImporterShaderFile::get_visible_name() const {
	return "GLSL Shader File";
}

void ResourceImporterShaderFile::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("glsl");
}

String ResourceImporterShaderFile::get_save_extension() const {
	return "res";
}

String ResourceImporterShaderFile::get_resource_type() const {
	return "RDShaderFile";
}

int ResourceImporterShaderFile::get_preset_count() const {
	return 0;
}

String ResourceImporterShaderFile::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterShaderFile::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
}

bool ResourceImporterShaderFile::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

static String _include_function(const String &p_path, void *userpointer) {
	Error err;

	String *base_path = (String *)userpointer;

	String include = p_path;
	if (include.is_relative_path()) {
		include = base_path->path_join(include);
	}

	Ref<FileAccess> file_inc = FileAccess::open(include, FileAccess::READ, &err);
	if (err != OK) {
		return String();
	}
	return file_inc->get_as_utf8_string();
}

Error ResourceImporterShaderFile::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	/* STEP 1, Read shader code */
	ERR_FAIL_COND_V_EDMSG((OS::get_singleton()->get_current_rendering_method() == "gl_compatibility"), ERR_UNAVAILABLE, "Cannot import custom .glsl shaders when using the Compatibility renderer. Please switch to the Forward+ or Mobile renderer to use custom shaders.");
	ERR_FAIL_COND_V_EDMSG((DisplayServer::get_singleton()->get_name() == "headless"), ERR_UNAVAILABLE, "Cannot import custom .glsl shaders when running in headless mode.");

	Error err;
	Ref<FileAccess> file = FileAccess::open(p_source_file, FileAccess::READ, &err);
	ERR_FAIL_COND_V(err != OK, ERR_CANT_OPEN);
	ERR_FAIL_COND_V(!file.is_valid(), ERR_CANT_OPEN);

	String file_txt = file->get_as_utf8_string();
	Ref<RDShaderFile> shader_file;
	shader_file.instantiate();
	String base_path = p_source_file.get_base_dir();
	err = shader_file->parse_versions_from_text(file_txt, "", _include_function, &base_path);

	if (err != OK) {
		if (!ShaderFileEditor::singleton->is_visible_in_tree()) {
			callable_mp_static(&EditorNode::add_io_error).call_deferred(vformat(TTR("Error importing GLSL shader file: '%s'. Open the file in the filesystem dock in order to see the reason."), p_source_file));
		}
	}

	ResourceSaver::save(shader_file, p_save_path + ".res");

	return OK;
}

ResourceImporterShaderFile::ResourceImporterShaderFile() {
}
