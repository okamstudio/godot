/**************************************************************************/
/*  script_language_extension.cpp                                         */
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

#include "script_language_extension.h"

void ScriptExtension::_bind_methods() {
	GDVIRTUAL_BIND(_editor_can_reload_from_file);
	GDVIRTUAL_BIND(_placeholder_erased, "placeholder");

	GDVIRTUAL_BIND(_can_instantiate);
	GDVIRTUAL_BIND(_get_base_script);
	GDVIRTUAL_BIND(_get_global_name);
	GDVIRTUAL_BIND(_inherits_script, "script");

	GDVIRTUAL_BIND(_get_instance_base_type);
	GDVIRTUAL_BIND(_instance_create, "for_object");
	GDVIRTUAL_BIND(_placeholder_instance_create, "for_object");

	GDVIRTUAL_BIND(_instance_has, "object");

	GDVIRTUAL_BIND(_has_source_code);
	GDVIRTUAL_BIND(_get_source_code);

	GDVIRTUAL_BIND(_set_source_code, "code");
	GDVIRTUAL_BIND(_reload, "keep_state");

	GDVIRTUAL_BIND(_get_doc_class_name);
	GDVIRTUAL_BIND(_get_documentation);
	GDVIRTUAL_BIND(_get_class_icon_path);

	GDVIRTUAL_BIND(_has_method, "method");
	GDVIRTUAL_BIND(_has_static_method, "method");

	GDVIRTUAL_BIND(_get_script_method_argument_count, "method");

	GDVIRTUAL_BIND(_get_method_info, "method");

	GDVIRTUAL_BIND(_is_tool);
	GDVIRTUAL_BIND(_is_valid);
	GDVIRTUAL_BIND(_is_abstract);
	GDVIRTUAL_BIND(_get_language);

	GDVIRTUAL_BIND(_has_script_signal, "signal");
	GDVIRTUAL_BIND(_get_script_signal_list);

	GDVIRTUAL_BIND(_has_property_default_value, "property");
	GDVIRTUAL_BIND(_get_property_default_value, "property");

	GDVIRTUAL_BIND(_update_exports);
	GDVIRTUAL_BIND(_get_script_method_list);
	GDVIRTUAL_BIND(_get_script_property_list);

	GDVIRTUAL_BIND(_get_member_line, "member");

	GDVIRTUAL_BIND(_get_constants);
	GDVIRTUAL_BIND(_get_members);
	GDVIRTUAL_BIND(_is_placeholder_fallback_enabled);

	GDVIRTUAL_BIND(_get_rpc_config);
}

void ScriptLanguageExtension::_bind_methods() {
	GDVIRTUAL_BIND(_get_name);
	GDVIRTUAL_BIND(_init);
	GDVIRTUAL_BIND(_get_type);
	GDVIRTUAL_BIND(_get_extension);
	GDVIRTUAL_BIND(_finish);

	GDVIRTUAL_BIND(_get_reserved_words);
	GDVIRTUAL_BIND(_is_control_flow_keyword, "keyword");
	GDVIRTUAL_BIND(_get_comment_delimiters);
	GDVIRTUAL_BIND(_get_doc_comment_delimiters);
	GDVIRTUAL_BIND(_get_string_delimiters);
	GDVIRTUAL_BIND(_make_template, "template", "class_name", "base_class_name");
	GDVIRTUAL_BIND(_get_built_in_templates, "object");
	GDVIRTUAL_BIND(_is_using_templates);
	GDVIRTUAL_BIND(_validate, "script", "path", "validate_functions", "validate_errors", "validate_warnings", "validate_safe_lines");

	GDVIRTUAL_BIND(_validate_path, "path");
	GDVIRTUAL_BIND(_create_script);
#ifndef DISABLE_DEPRECATED
	GDVIRTUAL_BIND(_has_named_classes);
#endif
	GDVIRTUAL_BIND(_supports_builtin_mode);
	GDVIRTUAL_BIND(_supports_documentation);
	GDVIRTUAL_BIND(_can_inherit_from_file);
	GDVIRTUAL_BIND(_find_function, "function", "code");
	GDVIRTUAL_BIND(_make_function, "class_name", "function_name", "function_args");
	GDVIRTUAL_BIND(_can_make_function);
	GDVIRTUAL_BIND(_open_in_external_editor, "script", "line", "column");
	GDVIRTUAL_BIND(_overrides_external_editor);
	GDVIRTUAL_BIND(_preferred_file_name_casing);

	GDVIRTUAL_BIND(_complete_code, "code", "path", "owner");
	GDVIRTUAL_BIND(_lookup_code, "code", "symbol", "path", "owner");
	GDVIRTUAL_BIND(_auto_indent_code, "code", "from_line", "to_line");

	GDVIRTUAL_BIND(_add_global_constant, "name", "value");
	GDVIRTUAL_BIND(_add_named_global_constant, "name", "value");
	GDVIRTUAL_BIND(_remove_named_global_constant, "name");

	GDVIRTUAL_BIND(_thread_enter);
	GDVIRTUAL_BIND(_thread_exit);
	GDVIRTUAL_BIND(_debug_get_error);
	GDVIRTUAL_BIND(_debug_get_stack_level_count);

	GDVIRTUAL_BIND(_debug_get_stack_level_line, "level");
	GDVIRTUAL_BIND(_debug_get_stack_level_function, "level");
	GDVIRTUAL_BIND(_debug_get_stack_level_source, "level");
	GDVIRTUAL_BIND(_debug_get_stack_level_locals, "level", "max_subitems", "max_depth");
	GDVIRTUAL_BIND(_debug_get_stack_level_members, "level", "max_subitems", "max_depth");
	GDVIRTUAL_BIND(_debug_get_stack_level_instance, "level");
	GDVIRTUAL_BIND(_debug_get_globals, "max_subitems", "max_depth");
	GDVIRTUAL_BIND(_debug_parse_stack_level_expression, "level", "expression", "max_subitems", "max_depth");

	GDVIRTUAL_BIND(_debug_get_current_stack_info);

	GDVIRTUAL_BIND(_reload_all_scripts);
	GDVIRTUAL_BIND(_reload_scripts, "scripts", "soft_reload");
	GDVIRTUAL_BIND(_reload_tool_script, "script", "soft_reload");

	GDVIRTUAL_BIND(_get_recognized_extensions);
	GDVIRTUAL_BIND(_get_public_functions);
	GDVIRTUAL_BIND(_get_public_constants);
	GDVIRTUAL_BIND(_get_public_annotations);

	GDVIRTUAL_BIND(_profiling_start);
	GDVIRTUAL_BIND(_profiling_stop);
	GDVIRTUAL_BIND(_profiling_set_save_native_calls, "enable");

	GDVIRTUAL_BIND(_profiling_get_accumulated_data, "info_array", "info_max");
	GDVIRTUAL_BIND(_profiling_get_frame_data, "info_array", "info_max");

	GDVIRTUAL_BIND(_frame);

	GDVIRTUAL_BIND(_handles_global_class_type, "type");
	GDVIRTUAL_BIND(_get_global_class_name, "path");

	BIND_ENUM_CONSTANT(LOOKUP_RESULT_SCRIPT_LOCATION);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_CONSTANT);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_PROPERTY);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_METHOD);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_SIGNAL);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_ENUM);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_TBD_GLOBALSCOPE); // Deprecated.
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_CLASS_ANNOTATION);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_LOCAL_CONSTANT);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_LOCAL_VARIABLE);
	BIND_ENUM_CONSTANT(LOOKUP_RESULT_MAX);

	BIND_ENUM_CONSTANT(LOCATION_LOCAL);
	BIND_ENUM_CONSTANT(LOCATION_PARENT_MASK);
	BIND_ENUM_CONSTANT(LOCATION_OTHER_USER_CODE);
	BIND_ENUM_CONSTANT(LOCATION_OTHER);

	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_CLASS);
	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_FUNCTION);
	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_SIGNAL);
	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_VARIABLE);
	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_MEMBER);
	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_ENUM);
	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_CONSTANT);
	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_NODE_PATH);
	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_FILE_PATH);
	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_PLAIN_TEXT);
	BIND_ENUM_CONSTANT(CODE_COMPLETION_KIND_MAX);
}
