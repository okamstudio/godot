/**************************************************************************/
/*  bindings_generator.cpp                                                */
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

#include "bindings_generator.h"

#if defined(DEBUG_METHODS_ENABLED) && defined(TOOLS_ENABLED)

#include "../godotsharp_defs.h"
#include "../utils/naming_utils.h"
#include "../utils/path_utils.h"
#include "../utils/string_utils.h"

#include "core/config/engine.h"
#include "core/core_constants.h"
#include "core/io/compression.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "main/main.h"

StringBuilder &operator<<(StringBuilder &r_sb, const String &p_string) {
	r_sb.append(p_string);
	return r_sb;
}

StringBuilder &operator<<(StringBuilder &r_sb, const char *p_cstring) {
	r_sb.append(p_cstring);
	return r_sb;
}

#define CS_INDENT "    " // 4 whitespaces

#define INDENT1 CS_INDENT
#define INDENT2 INDENT1 INDENT1
#define INDENT3 INDENT2 INDENT1
#define INDENT4 INDENT3 INDENT1

#define MEMBER_BEGIN "\n" INDENT1

#define OPEN_BLOCK "{\n"
#define CLOSE_BLOCK "}\n"

#define OPEN_BLOCK_L1 INDENT1 OPEN_BLOCK
#define OPEN_BLOCK_L2 INDENT2 OPEN_BLOCK
#define OPEN_BLOCK_L3 INDENT3 OPEN_BLOCK
#define CLOSE_BLOCK_L1 INDENT1 CLOSE_BLOCK
#define CLOSE_BLOCK_L2 INDENT2 CLOSE_BLOCK
#define CLOSE_BLOCK_L3 INDENT3 CLOSE_BLOCK

#define BINDINGS_GLOBAL_SCOPE_CLASS "GD"
#define BINDINGS_NATIVE_NAME_FIELD "NativeName"

#define BINDINGS_CLASS_CONSTRUCTOR "Constructors"
#define BINDINGS_CLASS_CONSTRUCTOR_EDITOR "EditorConstructors"
#define BINDINGS_CLASS_CONSTRUCTOR_DICTIONARY "BuiltInMethodConstructors"

#define CS_PARAM_MEMORYOWN "memoryOwn"
#define CS_PARAM_METHODBIND "method"
#define CS_PARAM_INSTANCE "ptr"
#define CS_STATIC_METHOD_GETINSTANCE "GetPtr"
#define CS_METHOD_CALL "Call"
#define CS_PROPERTY_SINGLETON "Singleton"
#define CS_SINGLETON_INSTANCE_SUFFIX "Instance"
#define CS_METHOD_INVOKE_GODOT_CLASS_METHOD "InvokeGodotClassMethod"
#define CS_METHOD_HAS_GODOT_CLASS_METHOD "HasGodotClassMethod"
#define CS_METHOD_HAS_GODOT_CLASS_SIGNAL "HasGodotClassSignal"

#define CS_STATIC_FIELD_NATIVE_CTOR "NativeCtor"
#define CS_STATIC_FIELD_METHOD_BIND_PREFIX "MethodBind"
#define CS_STATIC_FIELD_METHOD_PROXY_NAME_PREFIX "MethodProxyName_"
#define CS_STATIC_FIELD_SIGNAL_PROXY_NAME_PREFIX "SignalProxyName_"

#define ICALL_PREFIX "godot_icall_"
#define ICALL_CLASSDB_GET_METHOD "ClassDB_get_method"
#define ICALL_CLASSDB_GET_METHOD_WITH_COMPATIBILITY "ClassDB_get_method_with_compatibility"
#define ICALL_CLASSDB_GET_CONSTRUCTOR "ClassDB_get_constructor"

#define C_LOCAL_RET "ret"
#define C_LOCAL_VARARG_RET "vararg_ret"
#define C_LOCAL_PTRCALL_ARGS "call_args"

#define C_CLASS_NATIVE_FUNCS "NativeFuncs"
#define C_NS_MONOUTILS "InteropUtils"
#define C_METHOD_UNMANAGED_GET_MANAGED C_NS_MONOUTILS ".UnmanagedGetManaged"
#define C_METHOD_ENGINE_GET_SINGLETON C_NS_MONOUTILS ".EngineGetSingleton"

#define C_NS_MONOMARSHAL "Marshaling"
#define C_METHOD_MONOSTR_TO_GODOT C_NS_MONOMARSHAL ".ConvertStringToNative"
#define C_METHOD_MONOSTR_FROM_GODOT C_NS_MONOMARSHAL ".ConvertStringToManaged"
#define C_METHOD_MONOARRAY_TO(m_type) C_NS_MONOMARSHAL ".ConvertSystemArrayToNative" #m_type
#define C_METHOD_MONOARRAY_FROM(m_type) C_NS_MONOMARSHAL ".ConvertNative" #m_type "ToSystemArray"
#define C_METHOD_MANAGED_TO_CALLABLE C_NS_MONOMARSHAL ".ConvertCallableToNative"
#define C_METHOD_MANAGED_FROM_CALLABLE C_NS_MONOMARSHAL ".ConvertCallableToManaged"
#define C_METHOD_MANAGED_TO_SIGNAL C_NS_MONOMARSHAL ".ConvertSignalToNative"
#define C_METHOD_MANAGED_FROM_SIGNAL C_NS_MONOMARSHAL ".ConvertSignalToManaged"

// Types that will be ignored by the generator and won't be available in C#.
// This must be kept in sync with `ignored_types` in csharp_script.cpp
const Vector<String> ignored_types = {};

// Special [code] keywords to wrap with <see langword="code"/> instead of <c>code</c>.
// Don't check against all C# reserved words, as many cases are GDScript-specific.
const Vector<String> langword_check = { "true", "false", "null" };

// The following properties currently need to be defined with `new` to avoid warnings. We treat
// them as a special case instead of silencing the warnings altogether, to be warned if more
// shadowing appears.
const Vector<String> prop_allowed_inherited_member_hiding = {
	"ArrayMesh.BlendShapeMode",
	"Button.TextDirection",
	"Label.TextDirection",
	"LineEdit.TextDirection",
	"LinkButton.TextDirection",
	"MenuBar.TextDirection",
	"RichTextLabel.TextDirection",
	"TextEdit.TextDirection",
	"VisualShaderNodeReroute.PortType",
	// The following instances are uniquely egregious violations, hiding `GetType()` from `object`.
	// Included for the sake of CI, with the understanding that they *deserve* warnings.
	"GltfAccessor.GetType",
	"GltfAccessor.MethodName.GetType",
};

void BindingsGenerator::TypeInterface::postsetup_enum_type(BindingsGenerator::TypeInterface &r_enum_itype) {
	// C interface for enums is the same as that of 'uint32_t'. Remember to apply
	// any of the changes done here to the 'uint32_t' type interface as well.

	r_enum_itype.cs_type = r_enum_itype.proxy_name;
	r_enum_itype.cs_in_expr = "(int)%0";
	r_enum_itype.cs_out = "%5return (%2)%0(%1);";

	{
		// The expected types for parameters and return value in ptrcall are 'int64_t' or 'uint64_t'.
		r_enum_itype.c_in = "%5%0 %1_in = %1;\n";
		r_enum_itype.c_out = "%5return (%0)(%1);\n";
		r_enum_itype.c_type = "long";
		r_enum_itype.c_arg_in = "&%s_in";
	}
	r_enum_itype.c_type_in = "int";
	r_enum_itype.c_type_out = r_enum_itype.c_type_in;
	r_enum_itype.class_doc = &EditorHelp::get_doc_data()->class_list[r_enum_itype.proxy_name];
}

static String fix_doc_description(const String &p_bbcode) {
	// This seems to be the correct way to do this. It's the same EditorHelp does.

	return p_bbcode.dedent()
			.replace("\t", "")
			.replace("\r", "")
			.strip_edges();
}

String BindingsGenerator::bbcode_to_text(const String &p_bbcode, const TypeInterface *p_itype) {
	// Based on the version in EditorHelp.

	if (p_bbcode.is_empty()) {
		return String();
	}

	DocTools *doc = EditorHelp::get_doc_data();

	String bbcode = p_bbcode;

	StringBuilder output;

	List<String> tag_stack;
	bool code_tag = false;

	int pos = 0;
	while (pos < bbcode.length()) {
		int brk_pos = bbcode.find_char('[', pos);

		if (brk_pos < 0) {
			brk_pos = bbcode.length();
		}

		if (brk_pos > pos) {
			String text = bbcode.substr(pos, brk_pos - pos);
			if (code_tag || tag_stack.size() > 0) {
				output.append("'" + text + "'");
			} else {
				output.append(text);
			}
		}

		if (brk_pos == bbcode.length()) {
			// Nothing else to add.
			break;
		}

		int brk_end = bbcode.find_char(']', brk_pos + 1);

		if (brk_end == -1) {
			String text = bbcode.substr(brk_pos, bbcode.length() - brk_pos);
			if (code_tag || tag_stack.size() > 0) {
				output.append("'" + text + "'");
			}

			break;
		}

		String tag = bbcode.substr(brk_pos + 1, brk_end - brk_pos - 1);

		if (tag.begins_with("/")) {
			bool tag_ok = tag_stack.size() && tag_stack.front()->get() == tag.substr(1, tag.length());

			if (!tag_ok) {
				output.append("]");
				pos = brk_pos + 1;
				continue;
			}

			tag_stack.pop_front();
			pos = brk_end + 1;
			code_tag = false;
		} else if (code_tag) {
			output.append("[");
			pos = brk_pos + 1;
		} else if (tag.begins_with("method ") || tag.begins_with("constructor ") || tag.begins_with("operator ") || tag.begins_with("member ") || tag.begins_with("signal ") || tag.begins_with("enum ") || tag.begins_with("constant ") || tag.begins_with("theme_item ") || tag.begins_with("param ")) {
			const int tag_end = tag.find_char(' ');
			const String link_tag = tag.substr(0, tag_end);
			const String link_target = tag.substr(tag_end + 1, tag.length()).lstrip(" ");

			const Vector<String> link_target_parts = link_target.split(".");

			if (link_target_parts.size() <= 0 || link_target_parts.size() > 2) {
				ERR_PRINT("Invalid reference format: '" + tag + "'.");

				output.append(tag);

				pos = brk_end + 1;
				continue;
			}

			const TypeInterface *target_itype;
			StringName target_cname;

			if (link_target_parts.size() == 2) {
				target_itype = _get_type_or_null(TypeReference(link_target_parts[0]));
				if (!target_itype) {
					target_itype = _get_type_or_null(TypeReference("_" + link_target_parts[0]));
				}
				target_cname = link_target_parts[1];
			} else {
				target_itype = p_itype;
				target_cname = link_target_parts[0];
			}

			if (link_tag == "method") {
				_append_text_method(output, target_itype, target_cname, link_target, link_target_parts);
			} else if (link_tag == "constructor") {
				// TODO: Support constructors?
				_append_text_undeclared(output, link_target);
			} else if (link_tag == "operator") {
				// TODO: Support operators?
				_append_text_undeclared(output, link_target);
			} else if (link_tag == "member") {
				_append_text_member(output, target_itype, target_cname, link_target, link_target_parts);
			} else if (link_tag == "signal") {
				_append_text_signal(output, target_itype, target_cname, link_target, link_target_parts);
			} else if (link_tag == "enum") {
				_append_text_enum(output, target_itype, target_cname, link_target, link_target_parts);
			} else if (link_tag == "constant") {
				_append_text_constant(output, target_itype, target_cname, link_target, link_target_parts);
			} else if (link_tag == "param") {
				_append_text_param(output, link_target);
			} else if (link_tag == "theme_item") {
				// We do not declare theme_items in any way in C#, so there is nothing to reference.
				_append_text_undeclared(output, link_target);
			}

			pos = brk_end + 1;
		} else if (doc->class_list.has(tag)) {
			if (tag == "Array" || tag == "Dictionary") {
				output.append("'" BINDINGS_NAMESPACE_COLLECTIONS ".");
				output.append(tag);
				output.append("'");
			} else if (tag == "bool" || tag == "int") {
				output.append(tag);
			} else if (tag == "float") {
				output.append(
#ifdef REAL_T_IS_DOUBLE
						"double"
#else
						"float"
#endif
				);
			} else if (tag == "Variant") {
				output.append("'Godot.Variant'");
			} else if (tag == "String") {
				output.append("string");
			} else if (tag == "Nil") {
				output.append("null");
			} else if (tag.begins_with("@")) {
				// @GlobalScope, @GDScript, etc.
				output.append("'" + tag + "'");
			} else if (tag == "PackedByteArray") {
				output.append("byte[]");
			} else if (tag == "PackedInt32Array") {
				output.append("int[]");
			} else if (tag == "PackedInt64Array") {
				output.append("long[]");
			} else if (tag == "PackedFloat32Array") {
				output.append("float[]");
			} else if (tag == "PackedFloat64Array") {
				output.append("double[]");
			} else if (tag == "PackedStringArray") {
				output.append("string[]");
			} else if (tag == "PackedVector2Array") {
				output.append("'" BINDINGS_NAMESPACE ".Vector2[]'");
			} else if (tag == "PackedVector3Array") {
				output.append("'" BINDINGS_NAMESPACE ".Vector3[]'");
			} else if (tag == "PackedColorArray") {
				output.append("'" BINDINGS_NAMESPACE ".Color[]'");
			} else if (tag == "PackedVector4Array") {
				output.append("'" BINDINGS_NAMESPACE ".Vector4[]'");
			} else {
				const TypeInterface *target_itype = _get_type_or_null(TypeReference(tag));

				if (!target_itype) {
					target_itype = _get_type_or_null(TypeReference("_" + tag));
				}

				if (target_itype) {
					output.append("'" + target_itype->proxy_name + "'");
				} else {
					ERR_PRINT("Cannot resolve type reference in documentation: '" + tag + "'.");
					output.append("'" + tag + "'");
				}
			}

			pos = brk_end + 1;
		} else if (tag == "b") {
			// Bold is not supported.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "i") {
			// Italic is not supported.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "code" || tag.begins_with("code ")) {
			code_tag = true;
			pos = brk_end + 1;
			tag_stack.push_front("code");
		} else if (tag == "kbd") {
			// Keyboard combinations are not supported.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "center") {
			// Center alignment is not supported.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "br") {
			// Break is not supported.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "u") {
			// Underline is not supported.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "s") {
			// Strikethrough is not supported.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "url") {
			int end = bbcode.find_char('[', brk_end);
			if (end == -1) {
				end = bbcode.length();
			}
			String url = bbcode.substr(brk_end + 1, end - brk_end - 1);
			// Not supported. Just append the url.
			output.append(url);

			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("url=")) {
			String url = tag.substr(4, tag.length());
			// Not supported. Just append the url.
			output.append(url);

			pos = brk_end + 1;
			tag_stack.push_front("url");
		} else if (tag == "img") {
			int end = bbcode.find_char('[', brk_end);
			if (end == -1) {
				end = bbcode.length();
			}
			String image = bbcode.substr(brk_end + 1, end - brk_end - 1);

			// Not supported. Just append the bbcode.
			output.append("[img]");
			output.append(image);
			output.append("[/img]");

			pos = end;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("color=")) {
			// Not supported.
			pos = brk_end + 1;
			tag_stack.push_front("color");
		} else if (tag.begins_with("font=")) {
			// Not supported.
			pos = brk_end + 1;
			tag_stack.push_front("font");
		} else {
			// Ignore unrecognized tag.
			output.append("[");
			pos = brk_pos + 1;
		}
	}

	return output.as_string();
}

String BindingsGenerator::bbcode_to_xml(const String &p_bbcode, const TypeInterface *p_itype, bool p_is_signal) {
	// Based on the version in EditorHelp.

	if (p_bbcode.is_empty()) {
		return String();
	}

	DocTools *doc = EditorHelp::get_doc_data();

	String bbcode = p_bbcode;

	StringBuilder xml_output;

	xml_output.append("<para>");

	List<String> tag_stack;
	bool code_tag = false;
	bool line_del = false;

	int pos = 0;
	while (pos < bbcode.length()) {
		int brk_pos = bbcode.find_char('[', pos);

		if (brk_pos < 0) {
			brk_pos = bbcode.length();
		}

		if (brk_pos > pos) {
			if (!line_del) {
				String text = bbcode.substr(pos, brk_pos - pos);
				if (code_tag || tag_stack.size() > 0) {
					xml_output.append(text.xml_escape());
				} else {
					Vector<String> lines = text.split("\n");
					for (int i = 0; i < lines.size(); i++) {
						if (i != 0) {
							xml_output.append("<para>");
						}

						xml_output.append(lines[i].xml_escape());

						if (i != lines.size() - 1) {
							xml_output.append("</para>\n");
						}
					}
				}
			}
		}

		if (brk_pos == bbcode.length()) {
			// Nothing else to add.
			break;
		}

		int brk_end = bbcode.find_char(']', brk_pos + 1);

		if (brk_end == -1) {
			if (!line_del) {
				String text = bbcode.substr(brk_pos, bbcode.length() - brk_pos);
				if (code_tag || tag_stack.size() > 0) {
					xml_output.append(text.xml_escape());
				} else {
					Vector<String> lines = text.split("\n");
					for (int i = 0; i < lines.size(); i++) {
						if (i != 0) {
							xml_output.append("<para>");
						}

						xml_output.append(lines[i].xml_escape());

						if (i != lines.size() - 1) {
							xml_output.append("</para>\n");
						}
					}
				}
			}

			break;
		}

		String tag = bbcode.substr(brk_pos + 1, brk_end - brk_pos - 1);

		if (tag.begins_with("/")) {
			bool tag_ok = tag_stack.size() && tag_stack.front()->get() == tag.substr(1, tag.length());

			if (!tag_ok) {
				if (!line_del) {
					xml_output.append("[");
				}
				pos = brk_pos + 1;
				continue;
			}

			tag_stack.pop_front();
			pos = brk_end + 1;
			code_tag = false;

			if (tag == "/url") {
				xml_output.append("</a>");
			} else if (tag == "/code") {
				xml_output.append("</c>");
			} else if (tag == "/codeblock") {
				xml_output.append("</code>");
			} else if (tag == "/b") {
				xml_output.append("</b>");
			} else if (tag == "/i") {
				xml_output.append("</i>");
			} else if (tag == "/csharp") {
				xml_output.append("</code>");
				line_del = true;
			} else if (tag == "/codeblocks") {
				line_del = false;
			}
		} else if (code_tag) {
			xml_output.append("[");
			pos = brk_pos + 1;
		} else if (tag.begins_with("method ") || tag.begins_with("constructor ") || tag.begins_with("operator ") || tag.begins_with("member ") || tag.begins_with("signal ") || tag.begins_with("enum ") || tag.begins_with("constant ") || tag.begins_with("theme_item ") || tag.begins_with("param ")) {
			const int tag_end = tag.find_char(' ');
			const String link_tag = tag.substr(0, tag_end);
			const String link_target = tag.substr(tag_end + 1, tag.length()).lstrip(" ");

			const Vector<String> link_target_parts = link_target.split(".");

			if (link_target_parts.size() <= 0 || link_target_parts.size() > 2) {
				ERR_PRINT("Invalid reference format: '" + tag + "'.");

				xml_output.append("<c>");
				xml_output.append(tag);
				xml_output.append("</c>");

				pos = brk_end + 1;
				continue;
			}

			const TypeInterface *target_itype;
			StringName target_cname;

			if (link_target_parts.size() == 2) {
				target_itype = _get_type_or_null(TypeReference(link_target_parts[0]));
				if (!target_itype) {
					target_itype = _get_type_or_null(TypeReference("_" + link_target_parts[0]));
				}
				target_cname = link_target_parts[1];
			} else {
				target_itype = p_itype;
				target_cname = link_target_parts[0];
			}

			if (link_tag == "method") {
				_append_xml_method(xml_output, target_itype, target_cname, link_target, link_target_parts, p_itype);
			} else if (link_tag == "constructor") {
				// TODO: Support constructors?
				_append_xml_undeclared(xml_output, link_target);
			} else if (link_tag == "operator") {
				// TODO: Support operators?
				_append_xml_undeclared(xml_output, link_target);
			} else if (link_tag == "member") {
				_append_xml_member(xml_output, target_itype, target_cname, link_target, link_target_parts, p_itype);
			} else if (link_tag == "signal") {
				_append_xml_signal(xml_output, target_itype, target_cname, link_target, link_target_parts, p_itype);
			} else if (link_tag == "enum") {
				_append_xml_enum(xml_output, target_itype, target_cname, link_target, link_target_parts, p_itype);
			} else if (link_tag == "constant") {
				_append_xml_constant(xml_output, target_itype, target_cname, link_target, link_target_parts);
			} else if (link_tag == "param") {
				_append_xml_param(xml_output, link_target, p_is_signal);
			} else if (link_tag == "theme_item") {
				// We do not declare theme_items in any way in C#, so there is nothing to reference.
				_append_xml_undeclared(xml_output, link_target);
			}

			pos = brk_end + 1;
		} else if (doc->class_list.has(tag)) {
			if (tag == "Array" || tag == "Dictionary") {
				xml_output.append("<see cref=\"" BINDINGS_NAMESPACE_COLLECTIONS ".");
				xml_output.append(tag);
				xml_output.append("\"/>");
			} else if (tag == "bool" || tag == "int") {
				xml_output.append("<see cref=\"");
				xml_output.append(tag);
				xml_output.append("\"/>");
			} else if (tag == "float") {
				xml_output.append("<see cref=\""
#ifdef REAL_T_IS_DOUBLE
								  "double"
#else
								  "float"
#endif
								  "\"/>");
			} else if (tag == "Variant") {
				xml_output.append("<see cref=\"Godot.Variant\"/>");
			} else if (tag == "String") {
				xml_output.append("<see cref=\"string\"/>");
			} else if (tag == "Nil") {
				xml_output.append("<see langword=\"null\"/>");
			} else if (tag.begins_with("@")) {
				// @GlobalScope, @GDScript, etc.
				xml_output.append("<c>");
				xml_output.append(tag);
				xml_output.append("</c>");
			} else if (tag == "PackedByteArray") {
				xml_output.append("<see cref=\"byte\"/>[]");
			} else if (tag == "PackedInt32Array") {
				xml_output.append("<see cref=\"int\"/>[]");
			} else if (tag == "PackedInt64Array") {
				xml_output.append("<see cref=\"long\"/>[]");
			} else if (tag == "PackedFloat32Array") {
				xml_output.append("<see cref=\"float\"/>[]");
			} else if (tag == "PackedFloat64Array") {
				xml_output.append("<see cref=\"double\"/>[]");
			} else if (tag == "PackedStringArray") {
				xml_output.append("<see cref=\"string\"/>[]");
			} else if (tag == "PackedVector2Array") {
				xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".Vector2\"/>[]");
			} else if (tag == "PackedVector3Array") {
				xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".Vector3\"/>[]");
			} else if (tag == "PackedColorArray") {
				xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".Color\"/>[]");
			} else if (tag == "PackedVector4Array") {
				xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".Vector4\"/>[]");
			} else {
				const TypeInterface *target_itype = _get_type_or_null(TypeReference(tag));

				if (!target_itype) {
					target_itype = _get_type_or_null(TypeReference("_" + tag));
				}

				if (target_itype) {
					if (!_validate_api_type(target_itype, p_itype)) {
						_append_xml_undeclared(xml_output, target_itype->proxy_name);
					} else {
						xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".");
						xml_output.append(target_itype->proxy_name);
						xml_output.append("\"/>");
					}
				} else {
					ERR_PRINT("Cannot resolve type reference in documentation: '" + tag + "'.");

					xml_output.append("<c>");
					xml_output.append(tag);
					xml_output.append("</c>");
				}
			}

			pos = brk_end + 1;
		} else if (tag == "b") {
			xml_output.append("<b>");

			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "i") {
			xml_output.append("<i>");

			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "code" || tag.begins_with("code ")) {
			int end = bbcode.find_char('[', brk_end);
			if (end == -1) {
				end = bbcode.length();
			}
			String code = bbcode.substr(brk_end + 1, end - brk_end - 1);
			if (langword_check.has(code)) {
				xml_output.append("<see langword=\"");
				xml_output.append(code);
				xml_output.append("\"/>");

				pos = brk_end + code.length() + 8;
			} else {
				xml_output.append("<c>");

				code_tag = true;
				pos = brk_end + 1;
				tag_stack.push_front("code");
			}
		} else if (tag == "codeblock" || tag.begins_with("codeblock ")) {
			xml_output.append("<code>");

			code_tag = true;
			pos = brk_end + 1;
			tag_stack.push_front("codeblock");
		} else if (tag == "codeblocks") {
			line_del = true;
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "csharp" || tag.begins_with("csharp ")) {
			xml_output.append("<code>");

			line_del = false;
			code_tag = true;
			pos = brk_end + 1;
			tag_stack.push_front("csharp");
		} else if (tag == "kbd") {
			// Keyboard combinations are not supported in xml comments.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "center") {
			// Center alignment is not supported in xml comments.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "br") {
			xml_output.append("\n"); // FIXME: Should use <para> instead. Luckily this tag isn't used for now.
			pos = brk_end + 1;
		} else if (tag == "u") {
			// Underline is not supported in Rider xml comments.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "s") {
			// Strikethrough is not supported in xml comments.
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "url") {
			int end = bbcode.find_char('[', brk_end);
			if (end == -1) {
				end = bbcode.length();
			}
			String url = bbcode.substr(brk_end + 1, end - brk_end - 1);
			xml_output.append("<a href=\"");
			xml_output.append(url);
			xml_output.append("\">");
			xml_output.append(url);

			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("url=")) {
			String url = tag.substr(4, tag.length());
			xml_output.append("<a href=\"");
			xml_output.append(url);
			xml_output.append("\">");

			pos = brk_end + 1;
			tag_stack.push_front("url");
		} else if (tag == "img") {
			int end = bbcode.find_char('[', brk_end);
			if (end == -1) {
				end = bbcode.length();
			}
			String image = bbcode.substr(brk_end + 1, end - brk_end - 1);

			// Not supported. Just append the bbcode.
			xml_output.append("[img]");
			xml_output.append(image);
			xml_output.append("[/img]");

			pos = end;
			tag_stack.push_front(tag);
		} else if (tag.begins_with("color=")) {
			// Not supported.
			pos = brk_end + 1;
			tag_stack.push_front("color");
		} else if (tag.begins_with("font=")) {
			// Not supported.
			pos = brk_end + 1;
			tag_stack.push_front("font");
		} else {
			if (!line_del) {
				// Ignore unrecognized tag.
				xml_output.append("[");
			}
			pos = brk_pos + 1;
		}
	}

	xml_output.append("</para>");

	return xml_output.as_string();
}

void BindingsGenerator::_append_text_method(StringBuilder &p_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts) {
	if (p_link_target_parts[0] == name_cache.type_at_GlobalScope) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			OS::get_singleton()->print("Cannot resolve @GlobalScope method reference in documentation: %s\n", p_link_target.utf8().get_data());
		}

		// TODO Map what we can
		_append_text_undeclared(p_output, p_link_target);
	} else if (!p_target_itype || !p_target_itype->is_object_type) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			if (p_target_itype) {
				OS::get_singleton()->print("Cannot resolve method reference for non-GodotObject type in documentation: %s\n", p_link_target.utf8().get_data());
			} else {
				OS::get_singleton()->print("Cannot resolve type from method reference in documentation: %s\n", p_link_target.utf8().get_data());
			}
		}

		// TODO Map what we can
		_append_text_undeclared(p_output, p_link_target);
	} else {
		if (p_target_cname == "_init") {
			// The _init method is not declared in C#, reference the constructor instead
			p_output.append("'new " BINDINGS_NAMESPACE ".");
			p_output.append(p_target_itype->proxy_name);
			p_output.append("()'");
		} else {
			const MethodInterface *target_imethod = p_target_itype->find_method_by_name(p_target_cname);

			if (target_imethod) {
				p_output.append("'" BINDINGS_NAMESPACE ".");
				p_output.append(p_target_itype->proxy_name);
				p_output.append(".");
				p_output.append(target_imethod->proxy_name);
				p_output.append("(");
				bool first_key = true;
				for (const ArgumentInterface &iarg : target_imethod->arguments) {
					const TypeInterface *arg_type = _get_type_or_null(iarg.type);

					if (first_key) {
						first_key = false;
					} else {
						p_output.append(", ");
					}
					if (!arg_type) {
						ERR_PRINT("Cannot resolve argument type in documentation: '" + p_link_target + "'.");
						p_output.append(iarg.type.cname);
						continue;
					}
					if (iarg.def_param_mode == ArgumentInterface::NULLABLE_VAL) {
						p_output.append("Nullable<");
					}
					String arg_cs_type = arg_type->cs_type + _get_generic_type_parameters(*arg_type, iarg.type.generic_type_parameters);
					p_output.append(arg_cs_type.replacen("params ", ""));
					if (iarg.def_param_mode == ArgumentInterface::NULLABLE_VAL) {
						p_output.append(">");
					}
				}
				p_output.append(")'");
			} else {
				if (!p_target_itype->is_intentionally_ignored(p_link_target)) {
					ERR_PRINT("Cannot resolve method reference in documentation: '" + p_link_target + "'.");
				}

				_append_text_undeclared(p_output, p_link_target);
			}
		}
	}
}

void BindingsGenerator::_append_text_member(StringBuilder &p_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts) {
	if (p_link_target.contains_char('/')) {
		// Properties with '/' (slash) in the name are not declared in C#, so there is nothing to reference.
		_append_text_undeclared(p_output, p_link_target);
	} else if (!p_target_itype || !p_target_itype->is_object_type) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			if (p_target_itype) {
				OS::get_singleton()->print("Cannot resolve member reference for non-GodotObject type in documentation: %s\n", p_link_target.utf8().get_data());
			} else {
				OS::get_singleton()->print("Cannot resolve type from member reference in documentation: %s\n", p_link_target.utf8().get_data());
			}
		}

		// TODO Map what we can
		_append_text_undeclared(p_output, p_link_target);
	} else {
		const TypeInterface *current_itype = p_target_itype;
		const PropertyInterface *target_iprop = nullptr;

		while (target_iprop == nullptr && current_itype != nullptr) {
			target_iprop = current_itype->find_property_by_name(p_target_cname);
			if (target_iprop == nullptr) {
				current_itype = _get_type_or_null(TypeReference(current_itype->base_name));
			}
		}

		if (target_iprop) {
			p_output.append("'" BINDINGS_NAMESPACE ".");
			p_output.append(current_itype->proxy_name);
			p_output.append(".");
			p_output.append(target_iprop->proxy_name);
			p_output.append("'");
		} else {
			if (!p_target_itype->is_intentionally_ignored(p_link_target)) {
				ERR_PRINT("Cannot resolve member reference in documentation: '" + p_link_target + "'.");
			}

			_append_text_undeclared(p_output, p_link_target);
		}
	}
}

void BindingsGenerator::_append_text_signal(StringBuilder &p_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts) {
	if (!p_target_itype || !p_target_itype->is_object_type) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			if (p_target_itype) {
				OS::get_singleton()->print("Cannot resolve signal reference for non-GodotObject type in documentation: %s\n", p_link_target.utf8().get_data());
			} else {
				OS::get_singleton()->print("Cannot resolve type from signal reference in documentation: %s\n", p_link_target.utf8().get_data());
			}
		}

		// TODO Map what we can
		_append_text_undeclared(p_output, p_link_target);
	} else {
		const SignalInterface *target_isignal = p_target_itype->find_signal_by_name(p_target_cname);

		if (target_isignal) {
			p_output.append("'" BINDINGS_NAMESPACE ".");
			p_output.append(p_target_itype->proxy_name);
			p_output.append(".");
			p_output.append(target_isignal->proxy_name);
			p_output.append("'");
		} else {
			if (!p_target_itype->is_intentionally_ignored(p_link_target)) {
				ERR_PRINT("Cannot resolve signal reference in documentation: '" + p_link_target + "'.");
			}

			_append_text_undeclared(p_output, p_link_target);
		}
	}
}

void BindingsGenerator::_append_text_enum(StringBuilder &p_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts) {
	const StringName search_cname = !p_target_itype ? p_target_cname : StringName(p_target_itype->name + "." + (String)p_target_cname);

	HashMap<StringName, TypeInterface>::ConstIterator enum_match = enum_types.find(search_cname);

	if (!enum_match && search_cname != p_target_cname) {
		enum_match = enum_types.find(p_target_cname);
	}

	if (enum_match) {
		const TypeInterface &target_enum_itype = enum_match->value;

		p_output.append("'" BINDINGS_NAMESPACE ".");
		p_output.append(target_enum_itype.proxy_name); // Includes nesting class if any
		p_output.append("'");
	} else {
		if (!p_target_itype->is_intentionally_ignored(p_link_target)) {
			ERR_PRINT("Cannot resolve enum reference in documentation: '" + p_link_target + "'.");
		}

		_append_text_undeclared(p_output, p_link_target);
	}
}

void BindingsGenerator::_append_text_constant(StringBuilder &p_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts) {
	if (p_link_target_parts[0] == name_cache.type_at_GlobalScope) {
		_append_text_constant_in_global_scope(p_output, p_target_cname, p_link_target);
	} else if (!p_target_itype || !p_target_itype->is_object_type) {
		// Search in @GlobalScope as a last resort if no class was specified
		if (p_link_target_parts.size() == 1) {
			_append_text_constant_in_global_scope(p_output, p_target_cname, p_link_target);
			return;
		}

		if (OS::get_singleton()->is_stdout_verbose()) {
			if (p_target_itype) {
				OS::get_singleton()->print("Cannot resolve constant reference for non-GodotObject type in documentation: %s\n", p_link_target.utf8().get_data());
			} else {
				OS::get_singleton()->print("Cannot resolve type from constant reference in documentation: %s\n", p_link_target.utf8().get_data());
			}
		}

		// TODO Map what we can
		_append_text_undeclared(p_output, p_link_target);
	} else {
		// Try to find the constant in the current class
		if (p_target_itype->is_singleton_instance) {
			// Constants and enums are declared in the static singleton class.
			p_target_itype = &obj_types[p_target_itype->cname];
		}

		const ConstantInterface *target_iconst = find_constant_by_name(p_target_cname, p_target_itype->constants);

		if (target_iconst) {
			// Found constant in current class
			p_output.append("'" BINDINGS_NAMESPACE ".");
			p_output.append(p_target_itype->proxy_name);
			p_output.append(".");
			p_output.append(target_iconst->proxy_name);
			p_output.append("'");
		} else {
			// Try to find as enum constant in the current class
			const EnumInterface *target_ienum = nullptr;

			for (const EnumInterface &ienum : p_target_itype->enums) {
				target_ienum = &ienum;
				target_iconst = find_constant_by_name(p_target_cname, target_ienum->constants);
				if (target_iconst) {
					break;
				}
			}

			if (target_iconst) {
				p_output.append("'" BINDINGS_NAMESPACE ".");
				p_output.append(p_target_itype->proxy_name);
				p_output.append(".");
				p_output.append(target_ienum->proxy_name);
				p_output.append(".");
				p_output.append(target_iconst->proxy_name);
				p_output.append("'");
			} else if (p_link_target_parts.size() == 1) {
				// Also search in @GlobalScope as a last resort if no class was specified
				_append_text_constant_in_global_scope(p_output, p_target_cname, p_link_target);
			} else {
				if (!p_target_itype->is_intentionally_ignored(p_link_target)) {
					ERR_PRINT("Cannot resolve constant reference in documentation: '" + p_link_target + "'.");
				}

				_append_xml_undeclared(p_output, p_link_target);
			}
		}
	}
}

void BindingsGenerator::_append_text_constant_in_global_scope(StringBuilder &p_output, const String &p_target_cname, const String &p_link_target) {
	// Try to find as a global constant
	const ConstantInterface *target_iconst = find_constant_by_name(p_target_cname, global_constants);

	if (target_iconst) {
		// Found global constant
		p_output.append("'" BINDINGS_NAMESPACE "." BINDINGS_GLOBAL_SCOPE_CLASS ".");
		p_output.append(target_iconst->proxy_name);
		p_output.append("'");
	} else {
		// Try to find as global enum constant
		const EnumInterface *target_ienum = nullptr;

		for (const EnumInterface &ienum : global_enums) {
			target_ienum = &ienum;
			target_iconst = find_constant_by_name(p_target_cname, target_ienum->constants);
			if (target_iconst) {
				break;
			}
		}

		if (target_iconst) {
			p_output.append("'" BINDINGS_NAMESPACE ".");
			p_output.append(target_ienum->proxy_name);
			p_output.append(".");
			p_output.append(target_iconst->proxy_name);
			p_output.append("'");
		} else {
			ERR_PRINT("Cannot resolve global constant reference in documentation: '" + p_link_target + "'.");
			_append_text_undeclared(p_output, p_link_target);
		}
	}
}

void BindingsGenerator::_append_text_param(StringBuilder &p_output, const String &p_link_target) {
	const String link_target = snake_to_camel_case(p_link_target);
	p_output.append("'" + link_target + "'");
}

void BindingsGenerator::_append_text_undeclared(StringBuilder &p_output, const String &p_link_target) {
	p_output.append("'" + p_link_target + "'");
}

void BindingsGenerator::_append_xml_method(StringBuilder &p_xml_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts, const TypeInterface *p_source_itype) {
	if (p_link_target_parts[0] == name_cache.type_at_GlobalScope) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			OS::get_singleton()->print("Cannot resolve @GlobalScope method reference in documentation: %s\n", p_link_target.utf8().get_data());
		}

		// TODO Map what we can
		_append_xml_undeclared(p_xml_output, p_link_target);
	} else if (!p_target_itype || !p_target_itype->is_object_type) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			if (p_target_itype) {
				OS::get_singleton()->print("Cannot resolve method reference for non-GodotObject type in documentation: %s\n", p_link_target.utf8().get_data());
			} else {
				OS::get_singleton()->print("Cannot resolve type from method reference in documentation: %s\n", p_link_target.utf8().get_data());
			}
		}

		// TODO Map what we can
		_append_xml_undeclared(p_xml_output, p_link_target);
	} else {
		if (p_target_cname == "_init") {
			// The _init method is not declared in C#, reference the constructor instead
			p_xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".");
			p_xml_output.append(p_target_itype->proxy_name);
			p_xml_output.append(".");
			p_xml_output.append(p_target_itype->proxy_name);
			p_xml_output.append("()\"/>");
		} else {
			const MethodInterface *target_imethod = p_target_itype->find_method_by_name(p_target_cname);

			if (target_imethod) {
				const String method_name = p_target_itype->proxy_name + "." + target_imethod->proxy_name;
				if (!_validate_api_type(p_target_itype, p_source_itype)) {
					_append_xml_undeclared(p_xml_output, method_name);
				} else {
					p_xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".");
					p_xml_output.append(method_name);
					p_xml_output.append("(");
					bool first_key = true;
					for (const ArgumentInterface &iarg : target_imethod->arguments) {
						const TypeInterface *arg_type = _get_type_or_null(iarg.type);

						if (first_key) {
							first_key = false;
						} else {
							p_xml_output.append(", ");
						}
						if (!arg_type) {
							ERR_PRINT("Cannot resolve argument type in documentation: '" + p_link_target + "'.");
							p_xml_output.append(iarg.type.cname);
							continue;
						}
						if (iarg.def_param_mode == ArgumentInterface::NULLABLE_VAL) {
							p_xml_output.append("Nullable{");
						}
						String arg_cs_type = arg_type->cs_type + _get_generic_type_parameters(*arg_type, iarg.type.generic_type_parameters);
						p_xml_output.append(arg_cs_type.replacen("<", "{").replacen(">", "}").replacen("params ", ""));
						if (iarg.def_param_mode == ArgumentInterface::NULLABLE_VAL) {
							p_xml_output.append("}");
						}
					}
					p_xml_output.append(")\"/>");
				}
			} else {
				if (!p_target_itype->is_intentionally_ignored(p_link_target)) {
					ERR_PRINT("Cannot resolve method reference in documentation: '" + p_link_target + "'.");
				}

				_append_xml_undeclared(p_xml_output, p_link_target);
			}
		}
	}
}

void BindingsGenerator::_append_xml_member(StringBuilder &p_xml_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts, const TypeInterface *p_source_itype) {
	if (p_link_target.contains_char('/')) {
		// Properties with '/' (slash) in the name are not declared in C#, so there is nothing to reference.
		_append_xml_undeclared(p_xml_output, p_link_target);
	} else if (!p_target_itype || !p_target_itype->is_object_type) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			if (p_target_itype) {
				OS::get_singleton()->print("Cannot resolve member reference for non-GodotObject type in documentation: %s\n", p_link_target.utf8().get_data());
			} else {
				OS::get_singleton()->print("Cannot resolve type from member reference in documentation: %s\n", p_link_target.utf8().get_data());
			}
		}

		// TODO Map what we can
		_append_xml_undeclared(p_xml_output, p_link_target);
	} else {
		const TypeInterface *current_itype = p_target_itype;
		const PropertyInterface *target_iprop = nullptr;

		while (target_iprop == nullptr && current_itype != nullptr) {
			target_iprop = current_itype->find_property_by_name(p_target_cname);
			if (target_iprop == nullptr) {
				current_itype = _get_type_or_null(TypeReference(current_itype->base_name));
			}
		}

		if (target_iprop) {
			const String member_name = current_itype->proxy_name + "." + target_iprop->proxy_name;
			if (!_validate_api_type(p_target_itype, p_source_itype)) {
				_append_xml_undeclared(p_xml_output, member_name);
			} else {
				p_xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".");
				p_xml_output.append(member_name);
				p_xml_output.append("\"/>");
			}
		} else {
			if (!p_target_itype->is_intentionally_ignored(p_link_target)) {
				ERR_PRINT("Cannot resolve member reference in documentation: '" + p_link_target + "'.");
			}

			_append_xml_undeclared(p_xml_output, p_link_target);
		}
	}
}

void BindingsGenerator::_append_xml_signal(StringBuilder &p_xml_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts, const TypeInterface *p_source_itype) {
	if (!p_target_itype || !p_target_itype->is_object_type) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			if (p_target_itype) {
				OS::get_singleton()->print("Cannot resolve signal reference for non-GodotObject type in documentation: %s\n", p_link_target.utf8().get_data());
			} else {
				OS::get_singleton()->print("Cannot resolve type from signal reference in documentation: %s\n", p_link_target.utf8().get_data());
			}
		}

		// TODO Map what we can
		_append_xml_undeclared(p_xml_output, p_link_target);
	} else {
		const SignalInterface *target_isignal = p_target_itype->find_signal_by_name(p_target_cname);

		if (target_isignal) {
			const String signal_name = p_target_itype->proxy_name + "." + target_isignal->proxy_name;
			if (!_validate_api_type(p_target_itype, p_source_itype)) {
				_append_xml_undeclared(p_xml_output, signal_name);
			} else {
				p_xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".");
				p_xml_output.append(signal_name);
				p_xml_output.append("\"/>");
			}
		} else {
			if (!p_target_itype->is_intentionally_ignored(p_link_target)) {
				ERR_PRINT("Cannot resolve signal reference in documentation: '" + p_link_target + "'.");
			}

			_append_xml_undeclared(p_xml_output, p_link_target);
		}
	}
}

void BindingsGenerator::_append_xml_enum(StringBuilder &p_xml_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts, const TypeInterface *p_source_itype) {
	const StringName search_cname = !p_target_itype ? p_target_cname : StringName(p_target_itype->name + "." + (String)p_target_cname);

	HashMap<StringName, TypeInterface>::ConstIterator enum_match = enum_types.find(search_cname);

	if (!enum_match && search_cname != p_target_cname) {
		enum_match = enum_types.find(p_target_cname);
	}

	if (enum_match) {
		const TypeInterface &target_enum_itype = enum_match->value;

		if (!_validate_api_type(p_target_itype, p_source_itype)) {
			_append_xml_undeclared(p_xml_output, target_enum_itype.proxy_name);
		} else {
			p_xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".");
			p_xml_output.append(target_enum_itype.proxy_name); // Includes nesting class if any
			p_xml_output.append("\"/>");
		}
	} else {
		if (!p_target_itype->is_intentionally_ignored(p_link_target)) {
			ERR_PRINT("Cannot resolve enum reference in documentation: '" + p_link_target + "'.");
		}

		_append_xml_undeclared(p_xml_output, p_link_target);
	}
}

void BindingsGenerator::_append_xml_constant(StringBuilder &p_xml_output, const TypeInterface *p_target_itype, const StringName &p_target_cname, const String &p_link_target, const Vector<String> &p_link_target_parts) {
	if (p_link_target_parts[0] == name_cache.type_at_GlobalScope) {
		_append_xml_constant_in_global_scope(p_xml_output, p_target_cname, p_link_target);
	} else if (!p_target_itype || !p_target_itype->is_object_type) {
		// Search in @GlobalScope as a last resort if no class was specified
		if (p_link_target_parts.size() == 1) {
			_append_xml_constant_in_global_scope(p_xml_output, p_target_cname, p_link_target);
			return;
		}

		if (OS::get_singleton()->is_stdout_verbose()) {
			if (p_target_itype) {
				OS::get_singleton()->print("Cannot resolve constant reference for non-GodotObject type in documentation: %s\n", p_link_target.utf8().get_data());
			} else {
				OS::get_singleton()->print("Cannot resolve type from constant reference in documentation: %s\n", p_link_target.utf8().get_data());
			}
		}

		// TODO Map what we can
		_append_xml_undeclared(p_xml_output, p_link_target);
	} else {
		// Try to find the constant in the current class
		if (p_target_itype->is_singleton_instance) {
			// Constants and enums are declared in the static singleton class.
			p_target_itype = &obj_types[p_target_itype->cname];
		}

		const ConstantInterface *target_iconst = find_constant_by_name(p_target_cname, p_target_itype->constants);

		if (target_iconst) {
			// Found constant in current class
			p_xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".");
			p_xml_output.append(p_target_itype->proxy_name);
			p_xml_output.append(".");
			p_xml_output.append(target_iconst->proxy_name);
			p_xml_output.append("\"/>");
		} else {
			// Try to find as enum constant in the current class
			const EnumInterface *target_ienum = nullptr;

			for (const EnumInterface &ienum : p_target_itype->enums) {
				target_ienum = &ienum;
				target_iconst = find_constant_by_name(p_target_cname, target_ienum->constants);
				if (target_iconst) {
					break;
				}
			}

			if (target_iconst) {
				p_xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".");
				p_xml_output.append(p_target_itype->proxy_name);
				p_xml_output.append(".");
				p_xml_output.append(target_ienum->proxy_name);
				p_xml_output.append(".");
				p_xml_output.append(target_iconst->proxy_name);
				p_xml_output.append("\"/>");
			} else if (p_link_target_parts.size() == 1) {
				// Also search in @GlobalScope as a last resort if no class was specified
				_append_xml_constant_in_global_scope(p_xml_output, p_target_cname, p_link_target);
			} else {
				if (!p_target_itype->is_intentionally_ignored(p_link_target)) {
					ERR_PRINT("Cannot resolve constant reference in documentation: '" + p_link_target + "'.");
				}

				_append_xml_undeclared(p_xml_output, p_link_target);
			}
		}
	}
}

void BindingsGenerator::_append_xml_constant_in_global_scope(StringBuilder &p_xml_output, const String &p_target_cname, const String &p_link_target) {
	// Try to find as a global constant
	const ConstantInterface *target_iconst = find_constant_by_name(p_target_cname, global_constants);

	if (target_iconst) {
		// Found global constant
		p_xml_output.append("<see cref=\"" BINDINGS_NAMESPACE "." BINDINGS_GLOBAL_SCOPE_CLASS ".");
		p_xml_output.append(target_iconst->proxy_name);
		p_xml_output.append("\"/>");
	} else {
		// Try to find as global enum constant
		const EnumInterface *target_ienum = nullptr;

		for (const EnumInterface &ienum : global_enums) {
			target_ienum = &ienum;
			target_iconst = find_constant_by_name(p_target_cname, target_ienum->constants);
			if (target_iconst) {
				break;
			}
		}

		if (target_iconst) {
			p_xml_output.append("<see cref=\"" BINDINGS_NAMESPACE ".");
			p_xml_output.append(target_ienum->proxy_name);
			p_xml_output.append(".");
			p_xml_output.append(target_iconst->proxy_name);
			p_xml_output.append("\"/>");
		} else {
			ERR_PRINT("Cannot resolve global constant reference in documentation: '" + p_link_target + "'.");
			_append_xml_undeclared(p_xml_output, p_link_target);
		}
	}
}

void BindingsGenerator::_append_xml_param(StringBuilder &p_xml_output, const String &p_link_target, bool p_is_signal) {
	const String link_target = snake_to_camel_case(p_link_target);

	if (!p_is_signal) {
		p_xml_output.append("<paramref name=\"");
		p_xml_output.append(link_target);
		p_xml_output.append("\"/>");
	} else {
		// Documentation in C# is added to an event, not the delegate itself;
		// as such, we treat these parameters as codeblocks instead.
		// See: https://github.com/godotengine/godot/pull/65529
		_append_xml_undeclared(p_xml_output, link_target);
	}
}

void BindingsGenerator::_append_xml_undeclared(StringBuilder &p_xml_output, const String &p_link_target) {
	p_xml_output.append("<c>");
	p_xml_output.append(p_link_target);
	p_xml_output.append("</c>");
}

bool BindingsGenerator::_validate_api_type(const TypeInterface *p_target_itype, const TypeInterface *p_source_itype) {
	static constexpr const char *api_types[5] = {
		"Core",
		"Editor",
		"Extension",
		"Editor Extension",
		"None",
	};

	const ClassDB::APIType target_api = p_target_itype ? p_target_itype->api_type : ClassDB::API_NONE;
	ERR_FAIL_INDEX_V((int)target_api, 5, false);
	const ClassDB::APIType source_api = p_source_itype ? p_source_itype->api_type : ClassDB::API_NONE;
	ERR_FAIL_INDEX_V((int)source_api, 5, false);
	bool validate = false;

	switch (target_api) {
		case ClassDB::API_NONE:
		case ClassDB::API_CORE:
		default:
			validate = true;
			break;
		case ClassDB::API_EDITOR:
			validate = source_api == ClassDB::API_EDITOR || source_api == ClassDB::API_EDITOR_EXTENSION;
			break;
		case ClassDB::API_EXTENSION:
			validate = source_api == ClassDB::API_EXTENSION || source_api == ClassDB::API_EDITOR_EXTENSION;
			break;
		case ClassDB::API_EDITOR_EXTENSION:
			validate = source_api == ClassDB::API_EDITOR_EXTENSION;
			break;
	}
	if (!validate) {
		const String target_name = p_target_itype ? p_target_itype->proxy_name : "@GlobalScope";
		const String source_name = p_source_itype ? p_source_itype->proxy_name : "@GlobalScope";
		WARN_PRINT(vformat("Type '%s' has API level '%s'; it cannot be referenced by type '%s' with API level '%s'.",
				target_name, api_types[target_api], source_name, api_types[source_api]));
	}
	return validate;
}

int BindingsGenerator::_determine_enum_prefix(const EnumInterface &p_ienum) {
	CRASH_COND(p_ienum.constants.is_empty());

	const ConstantInterface &front_iconstant = p_ienum.constants.front()->get();
	Vector<String> front_parts = front_iconstant.name.split("_", /* p_allow_empty: */ true);
	int candidate_len = front_parts.size() - 1;

	if (candidate_len == 0) {
		return 0;
	}

	for (const ConstantInterface &iconstant : p_ienum.constants) {
		Vector<String> parts = iconstant.name.split("_", /* p_allow_empty: */ true);

		int i;
		for (i = 0; i < candidate_len && i < parts.size(); i++) {
			if (front_parts[i] != parts[i]) {
				// HARDCODED: Some Flag enums have the prefix 'FLAG_' for everything except 'FLAGS_DEFAULT' (same for 'METHOD_FLAG_' and'METHOD_FLAGS_DEFAULT').
				bool hardcoded_exc = (i == candidate_len - 1 && ((front_parts[i] == "FLAGS" && parts[i] == "FLAG") || (front_parts[i] == "FLAG" && parts[i] == "FLAGS")));
				if (!hardcoded_exc) {
					break;
				}
			}
		}
		candidate_len = i;

		if (candidate_len == 0) {
			return 0;
		}
	}

	return candidate_len;
}

void BindingsGenerator::_apply_prefix_to_enum_constants(BindingsGenerator::EnumInterface &p_ienum, int p_prefix_length) {
	if (p_prefix_length > 0) {
		for (ConstantInterface &iconstant : p_ienum.constants) {
			int curr_prefix_length = p_prefix_length;

			String constant_name = iconstant.name;

			Vector<String> parts = constant_name.split("_", /* p_allow_empty: */ true);

			if (parts.size() <= curr_prefix_length) {
				continue;
			}

			if (is_digit(parts[curr_prefix_length][0])) {
				// The name of enum constants may begin with a numeric digit when strip from the enum prefix,
				// so we make the prefix for this constant one word shorter in those cases.
				for (curr_prefix_length = curr_prefix_length - 1; curr_prefix_length > 0; curr_prefix_length--) {
					if (!is_digit(parts[curr_prefix_length][0])) {
						break;
					}
				}
			}

			constant_name = "";
			for (int i = curr_prefix_length; i < parts.size(); i++) {
				if (i > curr_prefix_length) {
					constant_name += "_";
				}
				constant_name += parts[i];
			}

			iconstant.proxy_name = snake_to_pascal_case(constant_name, true);
		}
	}
}

Error BindingsGenerator::_populate_method_icalls_table(const TypeInterface &p_itype) {
	for (const MethodInterface &imethod : p_itype.methods) {
		if (imethod.is_virtual) {
			continue;
		}

		const TypeInterface *return_type = _get_type_or_null(imethod.return_type);
		ERR_FAIL_NULL_V_MSG(return_type, ERR_BUG, "Return type '" + imethod.return_type.cname + "' was not found.");

		String im_unique_sig = get_ret_unique_sig(return_type) + ",CallMethodBind";

		if (!imethod.is_static) {
			im_unique_sig += ",CallInstance";
		}

		// Get arguments information
		for (const ArgumentInterface &iarg : imethod.arguments) {
			const TypeInterface *arg_type = _get_type_or_null(iarg.type);
			ERR_FAIL_NULL_V_MSG(arg_type, ERR_BUG, "Argument type '" + iarg.type.cname + "' was not found.");

			im_unique_sig += ",";
			im_unique_sig += get_arg_unique_sig(*arg_type);
		}

		// godot_icall_{argc}_{icallcount}
		String icall_method = ICALL_PREFIX;
		icall_method += itos(imethod.arguments.size());
		icall_method += "_";
		icall_method += itos(method_icalls.size());

		InternalCall im_icall = InternalCall(p_itype.api_type, icall_method, im_unique_sig);

		im_icall.is_vararg = imethod.is_vararg;
		im_icall.is_static = imethod.is_static;
		im_icall.return_type = imethod.return_type;

		for (const List<ArgumentInterface>::Element *F = imethod.arguments.front(); F; F = F->next()) {
			im_icall.argument_types.push_back(F->get().type);
		}

		List<InternalCall>::Element *match = method_icalls.find(im_icall);

		if (match) {
			if (p_itype.api_type != ClassDB::API_EDITOR) {
				match->get().editor_only = false;
			}
			method_icalls_map.insert(&imethod, &match->get());
		} else {
			List<InternalCall>::Element *added = method_icalls.push_back(im_icall);
			method_icalls_map.insert(&imethod, &added->get());
		}
	}

	return OK;
}

void BindingsGenerator::_generate_array_extensions(StringBuilder &p_output) {
	p_output.append("namespace " BINDINGS_NAMESPACE ";\n\n");
	p_output.append("using System;\n\n");
	// The class where we put the extensions doesn't matter, so just use "GD".
	p_output.append("public static partial class " BINDINGS_GLOBAL_SCOPE_CLASS "\n{");

#define ARRAY_IS_EMPTY(m_type)                                                                          \
	p_output.append("\n" INDENT1 "/// <summary>\n");                                                    \
	p_output.append(INDENT1 "/// Returns true if this " #m_type " array is empty or doesn't exist.\n"); \
	p_output.append(INDENT1 "/// </summary>\n");                                                        \
	p_output.append(INDENT1 "/// <param name=\"instance\">The " #m_type " array check.</param>\n");     \
	p_output.append(INDENT1 "/// <returns>Whether or not the array is empty.</returns>\n");             \
	p_output.append(INDENT1 "public static bool IsEmpty(this " #m_type "[] instance)\n");               \
	p_output.append(OPEN_BLOCK_L1);                                                                     \
	p_output.append(INDENT2 "return instance == null || instance.Length == 0;\n");                      \
	p_output.append(INDENT1 CLOSE_BLOCK);

#define ARRAY_JOIN(m_type)                                                                                          \
	p_output.append("\n" INDENT1 "/// <summary>\n");                                                                \
	p_output.append(INDENT1 "/// Converts this " #m_type " array to a string delimited by the given string.\n");    \
	p_output.append(INDENT1 "/// </summary>\n");                                                                    \
	p_output.append(INDENT1 "/// <param name=\"instance\">The " #m_type " array to convert.</param>\n");            \
	p_output.append(INDENT1 "/// <param name=\"delimiter\">The delimiter to use between items.</param>\n");         \
	p_output.append(INDENT1 "/// <returns>A single string with all items.</returns>\n");                            \
	p_output.append(INDENT1 "public static string Join(this " #m_type "[] instance, string delimiter = \", \")\n"); \
	p_output.append(OPEN_BLOCK_L1);                                                                                 \
	p_output.append(INDENT2 "return String.Join(delimiter, instance);\n");                                          \
	p_output.append(INDENT1 CLOSE_BLOCK);

#define ARRAY_STRINGIFY(m_type)                                                                          \
	p_output.append("\n" INDENT1 "/// <summary>\n");                                                     \
	p_output.append(INDENT1 "/// Converts this " #m_type " array to a string with brackets.\n");         \
	p_output.append(INDENT1 "/// </summary>\n");                                                         \
	p_output.append(INDENT1 "/// <param name=\"instance\">The " #m_type " array to convert.</param>\n"); \
	p_output.append(INDENT1 "/// <returns>A single string with all items.</returns>\n");                 \
	p_output.append(INDENT1 "public static string Stringify(this " #m_type "[] instance)\n");            \
	p_output.append(OPEN_BLOCK_L1);                                                                      \
	p_output.append(INDENT2 "return \"[\" + instance.Join() + \"]\";\n");                                \
	p_output.append(INDENT1 CLOSE_BLOCK);

#define ARRAY_ALL(m_type)  \
	ARRAY_IS_EMPTY(m_type) \
	ARRAY_JOIN(m_type)     \
	ARRAY_STRINGIFY(m_type)

	ARRAY_ALL(byte);
	ARRAY_ALL(int);
	ARRAY_ALL(long);
	ARRAY_ALL(float);
	ARRAY_ALL(double);
	ARRAY_ALL(string);
	ARRAY_ALL(Color);
	ARRAY_ALL(Vector2);
	ARRAY_ALL(Vector2I);
	ARRAY_ALL(Vector3);
	ARRAY_ALL(Vector3I);
	ARRAY_ALL(Vector4);
	ARRAY_ALL(Vector4I);

#undef ARRAY_ALL
#undef ARRAY_IS_EMPTY
#undef ARRAY_JOIN
#undef ARRAY_STRINGIFY

	p_output.append(CLOSE_BLOCK); // End of GD class.
}

void BindingsGenerator::_generate_global_constants(StringBuilder &p_output) {
	// Constants (in partial GD class)

	p_output.append("namespace " BINDINGS_NAMESPACE ";\n\n");

	p_output.append("public static partial class " BINDINGS_GLOBAL_SCOPE_CLASS "\n" OPEN_BLOCK);

	for (const ConstantInterface &iconstant : global_constants) {
		if (iconstant.const_doc && iconstant.const_doc->description.size()) {
			String xml_summary = bbcode_to_xml(fix_doc_description(iconstant.const_doc->description), nullptr);
			Vector<String> summary_lines = xml_summary.length() ? xml_summary.split("\n") : Vector<String>();

			if (summary_lines.size()) {
				p_output.append(MEMBER_BEGIN "/// <summary>\n");

				for (int i = 0; i < summary_lines.size(); i++) {
					p_output.append(INDENT1 "/// ");
					p_output.append(summary_lines[i]);
					p_output.append("\n");
				}

				p_output.append(INDENT1 "/// </summary>");
			}
		}

		p_output.append(MEMBER_BEGIN "public const long ");
		p_output.append(iconstant.proxy_name);
		p_output.append(" = ");
		p_output.append(itos(iconstant.value));
		p_output.append(";");
	}

	if (!global_constants.is_empty()) {
		p_output.append("\n");
	}

	p_output.append(CLOSE_BLOCK); // end of GD class

	// Enums

	for (const EnumInterface &ienum : global_enums) {
		CRASH_COND(ienum.constants.is_empty());

		String enum_proxy_name = ienum.proxy_name;

		bool enum_in_static_class = false;

		if (enum_proxy_name.find_char('.') > 0) {
			enum_in_static_class = true;
			String enum_class_name = enum_proxy_name.get_slicec('.', 0);
			enum_proxy_name = enum_proxy_name.get_slicec('.', 1);

			CRASH_COND(enum_class_name != "Variant"); // Hard-coded...

			_log("Declaring global enum '%s' inside struct '%s'\n", enum_proxy_name.utf8().get_data(), enum_class_name.utf8().get_data());

			p_output << "\npublic partial struct " << enum_class_name << "\n" OPEN_BLOCK;
		}

		const String maybe_indent = !enum_in_static_class ? "" : INDENT1;

		if (ienum.is_flags) {
			p_output << "\n"
					 << maybe_indent << "[System.Flags]";
		}

		p_output << "\n"
				 << maybe_indent << "public enum " << enum_proxy_name << " : long"
				 << "\n"
				 << maybe_indent << OPEN_BLOCK;

		for (const ConstantInterface &iconstant : ienum.constants) {
			if (iconstant.const_doc && iconstant.const_doc->description.size()) {
				String xml_summary = bbcode_to_xml(fix_doc_description(iconstant.const_doc->description), nullptr);
				Vector<String> summary_lines = xml_summary.length() ? xml_summary.split("\n") : Vector<String>();

				if (summary_lines.size()) {
					p_output << maybe_indent << INDENT1 "/// <summary>\n";

					for (int i = 0; i < summary_lines.size(); i++) {
						p_output << maybe_indent << INDENT1 "/// " << summary_lines[i] << "\n";
					}

					p_output << maybe_indent << INDENT1 "/// </summary>\n";
				}
			}

			p_output << maybe_indent << INDENT1
					 << iconstant.proxy_name
					 << " = "
					 << itos(iconstant.value)
					 << ",\n";
		}

		p_output << maybe_indent << CLOSE_BLOCK;

		if (enum_in_static_class) {
			p_output << CLOSE_BLOCK;
		}
	}
}

Error BindingsGenerator::generate_cs_core_project(const String &p_proj_dir) {
	ERR_FAIL_COND_V(!initialized, ERR_UNCONFIGURED);

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), ERR_CANT_CREATE);

	if (!DirAccess::exists(p_proj_dir)) {
		Error err = da->make_dir_recursive(p_proj_dir);
		ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_CREATE, "Cannot create directory '" + p_proj_dir + "'.");
	}

	da->change_dir(p_proj_dir);
	da->make_dir("Generated");
	da->make_dir("Generated/GodotObjects");

	String base_gen_dir = path::join(p_proj_dir, "Generated");
	String godot_objects_gen_dir = path::join(base_gen_dir, "GodotObjects");

	Vector<String> compile_items;

	// Generate source file for global scope constants and enums
	{
		StringBuilder constants_source;
		_generate_global_constants(constants_source);
		String output_file = path::join(base_gen_dir, BINDINGS_GLOBAL_SCOPE_CLASS "_constants.cs");
		Error save_err = _save_file(output_file, constants_source);
		if (save_err != OK) {
			return save_err;
		}

		compile_items.push_back(output_file);
	}

	// Generate source file for array extensions
	{
		StringBuilder extensions_source;
		_generate_array_extensions(extensions_source);
		String output_file = path::join(base_gen_dir, BINDINGS_GLOBAL_SCOPE_CLASS "_extensions.cs");
		Error save_err = _save_file(output_file, extensions_source);
		if (save_err != OK) {
			return save_err;
		}

		compile_items.push_back(output_file);
	}

	for (const KeyValue<StringName, TypeInterface> &E : obj_types) {
		const TypeInterface &itype = E.value;

		if (itype.api_type == ClassDB::API_EDITOR) {
			continue;
		}

		String output_file = path::join(godot_objects_gen_dir, itype.proxy_name + ".cs");
		Error err = _generate_cs_type(itype, output_file);

		if (err == ERR_SKIP) {
			continue;
		}

		if (err != OK) {
			return err;
		}

		compile_items.push_back(output_file);
	}

	// Generate source file for built-in type constructor dictionary.

	{
		StringBuilder cs_built_in_ctors_content;

		cs_built_in_ctors_content.append("namespace " BINDINGS_NAMESPACE ";\n\n");
		cs_built_in_ctors_content.append("using System;\n"
										 "using System.Collections.Generic;\n"
										 "\n");
		cs_built_in_ctors_content.append("internal static class " BINDINGS_CLASS_CONSTRUCTOR "\n{");

		cs_built_in_ctors_content.append(MEMBER_BEGIN "internal static readonly Dictionary<string, Func<IntPtr, GodotObject>> " BINDINGS_CLASS_CONSTRUCTOR_DICTIONARY ";\n");

		cs_built_in_ctors_content.append(MEMBER_BEGIN "public static GodotObject Invoke(string nativeTypeNameStr, IntPtr nativeObjectPtr)\n");
		cs_built_in_ctors_content.append(INDENT1 OPEN_BLOCK);
		cs_built_in_ctors_content.append(INDENT2 "if (!" BINDINGS_CLASS_CONSTRUCTOR_DICTIONARY ".TryGetValue(nativeTypeNameStr, out var constructor))\n");
		cs_built_in_ctors_content.append(INDENT3 "throw new InvalidOperationException(\"Wrapper class not found for type: \" + nativeTypeNameStr);\n");
		cs_built_in_ctors_content.append(INDENT2 "return constructor(nativeObjectPtr);\n");
		cs_built_in_ctors_content.append(INDENT1 CLOSE_BLOCK);

		cs_built_in_ctors_content.append(MEMBER_BEGIN "static " BINDINGS_CLASS_CONSTRUCTOR "()\n");
		cs_built_in_ctors_content.append(INDENT1 OPEN_BLOCK);
		cs_built_in_ctors_content.append(INDENT2 BINDINGS_CLASS_CONSTRUCTOR_DICTIONARY " = new();\n");

		for (const KeyValue<StringName, TypeInterface> &E : obj_types) {
			const TypeInterface &itype = E.value;

			if (itype.api_type != ClassDB::API_CORE || itype.is_singleton_instance) {
				continue;
			}

			if (itype.is_deprecated) {
				cs_built_in_ctors_content.append("#pragma warning disable CS0618\n");
			}

			cs_built_in_ctors_content.append(INDENT2 BINDINGS_CLASS_CONSTRUCTOR_DICTIONARY ".Add(\"");
			cs_built_in_ctors_content.append(itype.name);
			cs_built_in_ctors_content.append("\", " CS_PARAM_INSTANCE " => new ");
			cs_built_in_ctors_content.append(itype.proxy_name);
			if (itype.is_singleton && !itype.is_compat_singleton) {
				cs_built_in_ctors_content.append("Instance");
			}
			cs_built_in_ctors_content.append("(" CS_PARAM_INSTANCE "));\n");

			if (itype.is_deprecated) {
				cs_built_in_ctors_content.append("#pragma warning restore CS0618\n");
			}
		}

		cs_built_in_ctors_content.append(INDENT1 CLOSE_BLOCK);

		cs_built_in_ctors_content.append(CLOSE_BLOCK);

		String constructors_file = path::join(base_gen_dir, BINDINGS_CLASS_CONSTRUCTOR ".cs");
		Error err = _save_file(constructors_file, cs_built_in_ctors_content);

		if (err != OK) {
			return err;
		}

		compile_items.push_back(constructors_file);
	}

	// Generate native calls

	StringBuilder cs_icalls_content;

	cs_icalls_content.append("namespace " BINDINGS_NAMESPACE ";\n\n");
	cs_icalls_content.append("using System;\n"
							 "using System.Diagnostics.CodeAnalysis;\n"
							 "using System.Runtime.InteropServices;\n"
							 "using Godot.NativeInterop;\n"
							 "\n");
	cs_icalls_content.append("[SuppressMessage(\"ReSharper\", \"InconsistentNaming\")]\n");
	cs_icalls_content.append("[SuppressMessage(\"ReSharper\", \"RedundantUnsafeContext\")]\n");
	cs_icalls_content.append("[SuppressMessage(\"ReSharper\", \"RedundantNameQualifier\")]\n");
	cs_icalls_content.append("[System.Runtime.CompilerServices.SkipLocalsInit]\n");
	cs_icalls_content.append("internal static class " BINDINGS_CLASS_NATIVECALLS "\n{");

	cs_icalls_content.append(MEMBER_BEGIN "internal static ulong godot_api_hash = ");
	cs_icalls_content.append(String::num_uint64(ClassDB::get_api_hash(ClassDB::API_CORE)) + ";\n");

	cs_icalls_content.append(MEMBER_BEGIN "private const int VarArgsSpanThreshold = 10;\n");

	for (const InternalCall &icall : method_icalls) {
		if (icall.editor_only) {
			continue;
		}
		Error err = _generate_cs_native_calls(icall, cs_icalls_content);
		if (err != OK) {
			return err;
		}
	}

	cs_icalls_content.append(CLOSE_BLOCK);

	String internal_methods_file = path::join(base_gen_dir, BINDINGS_CLASS_NATIVECALLS ".cs");

	Error err = _save_file(internal_methods_file, cs_icalls_content);
	if (err != OK) {
		return err;
	}

	compile_items.push_back(internal_methods_file);

	// Generate GeneratedIncludes.props

	StringBuilder includes_props_content;
	includes_props_content.append("<Project>\n"
								  "  <ItemGroup>\n");

	for (int i = 0; i < compile_items.size(); i++) {
		String include = path::relative_to(compile_items[i], p_proj_dir).replace("/", "\\");
		includes_props_content.append("    <Compile Include=\"" + include + "\" />\n");
	}

	includes_props_content.append("  </ItemGroup>\n"
								  "</Project>\n");

	String includes_props_file = path::join(base_gen_dir, "GeneratedIncludes.props");

	err = _save_file(includes_props_file, includes_props_content);
	if (err != OK) {
		return err;
	}

	return OK;
}

Error BindingsGenerator::generate_cs_editor_project(const String &p_proj_dir) {
	ERR_FAIL_COND_V(!initialized, ERR_UNCONFIGURED);

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), ERR_CANT_CREATE);

	if (!DirAccess::exists(p_proj_dir)) {
		Error err = da->make_dir_recursive(p_proj_dir);
		ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);
	}

	da->change_dir(p_proj_dir);
	da->make_dir("Generated");
	da->make_dir("Generated/GodotObjects");

	String base_gen_dir = path::join(p_proj_dir, "Generated");
	String godot_objects_gen_dir = path::join(base_gen_dir, "GodotObjects");

	Vector<String> compile_items;

	for (const KeyValue<StringName, TypeInterface> &E : obj_types) {
		const TypeInterface &itype = E.value;

		if (itype.api_type != ClassDB::API_EDITOR) {
			continue;
		}

		String output_file = path::join(godot_objects_gen_dir, itype.proxy_name + ".cs");
		Error err = _generate_cs_type(itype, output_file);

		if (err == ERR_SKIP) {
			continue;
		}

		if (err != OK) {
			return err;
		}

		compile_items.push_back(output_file);
	}

	// Generate source file for editor type constructor dictionary.

	{
		StringBuilder cs_built_in_ctors_content;

		cs_built_in_ctors_content.append("namespace " BINDINGS_NAMESPACE ";\n\n");
		cs_built_in_ctors_content.append("internal static class " BINDINGS_CLASS_CONSTRUCTOR_EDITOR "\n{");

		cs_built_in_ctors_content.append(MEMBER_BEGIN "private static void AddEditorConstructors()\n");
		cs_built_in_ctors_content.append(INDENT1 OPEN_BLOCK);
		cs_built_in_ctors_content.append(INDENT2 "var builtInMethodConstructors = " BINDINGS_CLASS_CONSTRUCTOR "." BINDINGS_CLASS_CONSTRUCTOR_DICTIONARY ";\n");

		for (const KeyValue<StringName, TypeInterface> &E : obj_types) {
			const TypeInterface &itype = E.value;

			if (itype.api_type != ClassDB::API_EDITOR || itype.is_singleton_instance) {
				continue;
			}

			if (itype.is_deprecated) {
				cs_built_in_ctors_content.append("#pragma warning disable CS0618\n");
			}

			cs_built_in_ctors_content.append(INDENT2 "builtInMethodConstructors.Add(\"");
			cs_built_in_ctors_content.append(itype.name);
			cs_built_in_ctors_content.append("\", " CS_PARAM_INSTANCE " => new ");
			cs_built_in_ctors_content.append(itype.proxy_name);
			if (itype.is_singleton && !itype.is_compat_singleton) {
				cs_built_in_ctors_content.append("Instance");
			}
			cs_built_in_ctors_content.append("(" CS_PARAM_INSTANCE "));\n");

			if (itype.is_deprecated) {
				cs_built_in_ctors_content.append("#pragma warning restore CS0618\n");
			}
		}

		cs_built_in_ctors_content.append(INDENT1 CLOSE_BLOCK);

		cs_built_in_ctors_content.append(CLOSE_BLOCK);

		String constructors_file = path::join(base_gen_dir, BINDINGS_CLASS_CONSTRUCTOR_EDITOR ".cs");
		Error err = _save_file(constructors_file, cs_built_in_ctors_content);

		if (err != OK) {
			return err;
		}

		compile_items.push_back(constructors_file);
	}

	// Generate native calls

	StringBuilder cs_icalls_content;

	cs_icalls_content.append("namespace " BINDINGS_NAMESPACE ";\n\n");
	cs_icalls_content.append("using System;\n"
							 "using System.Diagnostics.CodeAnalysis;\n"
							 "using System.Runtime.InteropServices;\n"
							 "using Godot.NativeInterop;\n"
							 "\n");
	cs_icalls_content.append("[SuppressMessage(\"ReSharper\", \"InconsistentNaming\")]\n");
	cs_icalls_content.append("[SuppressMessage(\"ReSharper\", \"RedundantUnsafeContext\")]\n");
	cs_icalls_content.append("[SuppressMessage(\"ReSharper\", \"RedundantNameQualifier\")]\n");
	cs_icalls_content.append("[System.Runtime.CompilerServices.SkipLocalsInit]\n");
	cs_icalls_content.append("internal static class " BINDINGS_CLASS_NATIVECALLS_EDITOR "\n" OPEN_BLOCK);

	cs_icalls_content.append(INDENT1 "internal static ulong godot_api_hash = ");
	cs_icalls_content.append(String::num_uint64(ClassDB::get_api_hash(ClassDB::API_EDITOR)) + ";\n");

	cs_icalls_content.append(MEMBER_BEGIN "private const int VarArgsSpanThreshold = 10;\n");

	cs_icalls_content.append("\n");

	for (const InternalCall &icall : method_icalls) {
		if (!icall.editor_only) {
			continue;
		}
		Error err = _generate_cs_native_calls(icall, cs_icalls_content);
		if (err != OK) {
			return err;
		}
	}

	cs_icalls_content.append(CLOSE_BLOCK);

	String internal_methods_file = path::join(base_gen_dir, BINDINGS_CLASS_NATIVECALLS_EDITOR ".cs");

	Error err = _save_file(internal_methods_file, cs_icalls_content);
	if (err != OK) {
		return err;
	}

	compile_items.push_back(internal_methods_file);

	// Generate GeneratedIncludes.props

	StringBuilder includes_props_content;
	includes_props_content.append("<Project>\n"
								  "  <ItemGroup>\n");

	for (int i = 0; i < compile_items.size(); i++) {
		String include = path::relative_to(compile_items[i], p_proj_dir).replace("/", "\\");
		includes_props_content.append("    <Compile Include=\"" + include + "\" />\n");
	}

	includes_props_content.append("  </ItemGroup>\n"
								  "</Project>\n");

	String includes_props_file = path::join(base_gen_dir, "GeneratedIncludes.props");

	err = _save_file(includes_props_file, includes_props_content);
	if (err != OK) {
		return err;
	}

	return OK;
}

Error BindingsGenerator::generate_cs_api(const String &p_output_dir) {
	ERR_FAIL_COND_V(!initialized, ERR_UNCONFIGURED);

	String output_dir = path::abspath(path::realpath(p_output_dir));

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), ERR_CANT_CREATE);

	if (!DirAccess::exists(output_dir)) {
		Error err = da->make_dir_recursive(output_dir);
		ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);
	}

	Error proj_err;

	// Generate GodotSharp source files

	String core_proj_dir = output_dir.path_join(CORE_API_ASSEMBLY_NAME);

	proj_err = generate_cs_core_project(core_proj_dir);
	if (proj_err != OK) {
		ERR_PRINT("Generation of the Core API C# project failed.");
		return proj_err;
	}

	// Generate GodotSharpEditor source files

	String editor_proj_dir = output_dir.path_join(EDITOR_API_ASSEMBLY_NAME);

	proj_err = generate_cs_editor_project(editor_proj_dir);
	if (proj_err != OK) {
		ERR_PRINT("Generation of the Editor API C# project failed.");
		return proj_err;
	}

	_log("The Godot API sources were successfully generated\n");

	return OK;
}

// FIXME: There are some members that hide other inherited members.
// - In the case of both members being the same kind, the new one must be declared
// explicitly as 'new' to avoid the warning (and we must print a message about it).
// - In the case of both members being of a different kind, then the new one must
// be renamed to avoid the name collision (and we must print a warning about it).
// - Csc warning e.g.:
// ObjectType/LineEdit.cs(140,38): warning CS0108: 'LineEdit.FocusMode' hides inherited member 'Control.FocusMode'. Use the new keyword if hiding was intended.
Error BindingsGenerator::_generate_cs_type(const TypeInterface &itype, const String &p_output_file) {
	CRASH_COND(!itype.is_object_type);

	bool is_derived_type = itype.base_name != StringName();

	if (!is_derived_type) {
		// Some GodotObject assertions
		CRASH_COND(itype.cname != name_cache.type_Object);
		CRASH_COND(!itype.is_instantiable);
		CRASH_COND(itype.api_type != ClassDB::API_CORE);
		CRASH_COND(itype.is_ref_counted);
		CRASH_COND(itype.is_singleton);
	}

	_log("Generating %s.cs...\n", itype.proxy_name.utf8().get_data());

	StringBuilder output;

	output.append("namespace " BINDINGS_NAMESPACE ";\n\n");

	output.append("using System;\n"); // IntPtr
	output.append("using System.ComponentModel;\n"); // EditorBrowsable
	output.append("using System.Diagnostics;\n"); // DebuggerBrowsable
	output.append("using Godot.NativeInterop;\n");

	output.append("\n#nullable disable\n");

	const DocData::ClassDoc *class_doc = itype.class_doc;

	if (class_doc && class_doc->description.size()) {
		String xml_summary = bbcode_to_xml(fix_doc_description(class_doc->description), &itype);
		Vector<String> summary_lines = xml_summary.length() ? xml_summary.split("\n") : Vector<String>();

		if (summary_lines.size()) {
			output.append("/// <summary>\n");

			for (int i = 0; i < summary_lines.size(); i++) {
				output.append("/// ");
				output.append(summary_lines[i]);
				output.append("\n");
			}

			output.append("/// </summary>\n");
		}
	}

	if (itype.is_deprecated) {
		output.append("[Obsolete(\"");
		output.append(bbcode_to_text(itype.deprecation_message, &itype));
		output.append("\")]\n");
	}

	// We generate a `GodotClassName` attribute if the engine class name is not the same as the
	// generated C# class name. This allows introspection code to find the name associated with
	// the class. If the attribute is not present, the C# class name can be used instead.
	if (itype.name != itype.proxy_name) {
		output << "[GodotClassName(\"" << itype.name << "\")]\n";
	}

	output.append("public ");
	if (itype.is_singleton) {
		output.append("static partial class ");
	} else {
		// Even if the class is not instantiable, we can't declare it abstract because
		// the engine can still instantiate them and return them via the scripting API.
		// Example: `SceneTreeTimer` returned from `SceneTree.create_timer`.
		// See the reverted commit: ef5672d3f94a7321ed779c922088bb72adbb1521
		output.append("partial class ");
	}
	output.append(itype.proxy_name);

	if (is_derived_type && !itype.is_singleton) {
		if (obj_types.has(itype.base_name)) {
			TypeInterface base_type = obj_types[itype.base_name];
			output.append(" : ");
			output.append(base_type.proxy_name);
			if (base_type.is_singleton) {
				// If the type is a singleton, use the instance type.
				output.append(CS_SINGLETON_INSTANCE_SUFFIX);
			}
		} else {
			ERR_PRINT("Base type '" + itype.base_name.operator String() + "' does not exist, for class '" + itype.name + "'.");
			return ERR_INVALID_DATA;
		}
	}

	output.append("\n{");

	// Add constants

	for (const ConstantInterface &iconstant : itype.constants) {
		if (iconstant.const_doc && iconstant.const_doc->description.size()) {
			String xml_summary = bbcode_to_xml(fix_doc_description(iconstant.const_doc->description), &itype);
			Vector<String> summary_lines = xml_summary.length() ? xml_summary.split("\n") : Vector<String>();

			if (summary_lines.size()) {
				output.append(MEMBER_BEGIN "/// <summary>\n");

				for (int i = 0; i < summary_lines.size(); i++) {
					output.append(INDENT1 "/// ");
					output.append(summary_lines[i]);
					output.append("\n");
				}

				output.append(INDENT1 "/// </summary>");
			}
		}

		if (iconstant.is_deprecated) {
			output.append(MEMBER_BEGIN "[Obsolete(\"");
			output.append(bbcode_to_text(iconstant.deprecation_message, &itype));
			output.append("\")]");
		}

		output.append(MEMBER_BEGIN "public const long ");
		output.append(iconstant.proxy_name);
		output.append(" = ");
		output.append(itos(iconstant.value));
		output.append(";");
	}

	if (itype.constants.size()) {
		output.append("\n");
	}

	// Add enums

	for (const EnumInterface &ienum : itype.enums) {
		ERR_FAIL_COND_V(ienum.constants.is_empty(), ERR_BUG);

		if (ienum.is_flags) {
			output.append(MEMBER_BEGIN "[System.Flags]");
		}

		output.append(MEMBER_BEGIN "public enum ");
		output.append(ienum.proxy_name);
		output.append(" : long");
		output.append(MEMBER_BEGIN OPEN_BLOCK);

		const ConstantInterface &last = ienum.constants.back()->get();
		for (const ConstantInterface &iconstant : ienum.constants) {
			if (iconstant.const_doc && iconstant.const_doc->description.size()) {
				String xml_summary = bbcode_to_xml(fix_doc_description(iconstant.const_doc->description), &itype);
				Vector<String> summary_lines = xml_summary.length() ? xml_summary.split("\n") : Vector<String>();

				if (summary_lines.size()) {
					output.append(INDENT2 "/// <summary>\n");

					for (int i = 0; i < summary_lines.size(); i++) {
						output.append(INDENT2 "/// ");
						output.append(summary_lines[i]);
						output.append("\n");
					}

					output.append(INDENT2 "/// </summary>\n");
				}
			}

			if (iconstant.is_deprecated) {
				output.append(INDENT2 "[Obsolete(\"");
				output.append(bbcode_to_text(iconstant.deprecation_message, &itype));
				output.append("\")]\n");
			}

			output.append(INDENT2);
			output.append(iconstant.proxy_name);
			output.append(" = ");
			output.append(itos(iconstant.value));
			output.append(&iconstant != &last ? ",\n" : "\n");
		}

		output.append(INDENT1 CLOSE_BLOCK);
	}

	// Add properties

	for (const PropertyInterface &iprop : itype.properties) {
		Error prop_err = _generate_cs_property(itype, iprop, output);
		ERR_FAIL_COND_V_MSG(prop_err != OK, prop_err,
				"Failed to generate property '" + iprop.cname.operator String() +
						"' for class '" + itype.name + "'.");
	}

	// Add native name static field and cached type.

	if (is_derived_type && !itype.is_singleton) {
		output << MEMBER_BEGIN "private static readonly System.Type CachedType = typeof(" << itype.proxy_name << ");\n";
	}

	output.append(MEMBER_BEGIN "private static readonly StringName " BINDINGS_NATIVE_NAME_FIELD " = \"");
	output.append(itype.name);
	output.append("\";\n");

	if (itype.is_singleton || itype.is_compat_singleton) {
		// Add the Singleton static property.

		String instance_type_name;

		if (itype.is_singleton) {
			StringName instance_name = itype.name + CS_SINGLETON_INSTANCE_SUFFIX;
			instance_type_name = obj_types.has(instance_name)
					? obj_types[instance_name].proxy_name
					: "GodotObject";
		} else {
			instance_type_name = itype.proxy_name;
		}

		output.append(MEMBER_BEGIN "private static " + instance_type_name + " singleton;\n");

		output << MEMBER_BEGIN "public static " + instance_type_name + " " CS_PROPERTY_SINGLETON " =>\n"
			   << INDENT2 "singleton \?\?= (" + instance_type_name + ")"
			   << C_METHOD_ENGINE_GET_SINGLETON "(\"" << itype.name << "\");\n";
	}

	if (!itype.is_singleton) {
		// IMPORTANT: We also generate the static fields for GodotObject instead of declaring
		// them manually in the `GodotObject.base.cs` partial class declaration, because they're
		// required by other static fields in this generated partial class declaration.
		// Static fields are initialized in order of declaration, but when they're in different
		// partial class declarations then it becomes harder to tell (Rider warns about this).

		if (itype.is_instantiable) {
			// Add native constructor static field

			output << MEMBER_BEGIN << "[DebuggerBrowsable(DebuggerBrowsableState.Never)]\n"
				   << INDENT1 "private static readonly unsafe delegate* unmanaged<godot_bool, IntPtr> "
				   << CS_STATIC_FIELD_NATIVE_CTOR " = " ICALL_CLASSDB_GET_CONSTRUCTOR
				   << "(" BINDINGS_NATIVE_NAME_FIELD ");\n";
		}

		if (is_derived_type) {
			// Add default constructor
			if (itype.is_instantiable) {
				output << MEMBER_BEGIN "public " << itype.proxy_name << "() : this("
					   << (itype.memory_own ? "true" : "false") << ")\n" OPEN_BLOCK_L1
					   << INDENT2 "unsafe\n" INDENT2 OPEN_BLOCK
					   << INDENT3 "ConstructAndInitialize(" CS_STATIC_FIELD_NATIVE_CTOR ", "
					   << BINDINGS_NATIVE_NAME_FIELD ", CachedType, refCounted: "
					   << (itype.is_ref_counted ? "true" : "false") << ");\n"
					   << CLOSE_BLOCK_L2 CLOSE_BLOCK_L1;
			} else {
				// Hide the constructor
				output << MEMBER_BEGIN "internal " << itype.proxy_name << "() : this("
					   << (itype.memory_own ? "true" : "false") << ")\n" OPEN_BLOCK_L1
					   << INDENT2 "unsafe\n" INDENT2 OPEN_BLOCK
					   << INDENT3 "ConstructAndInitialize(null, "
					   << BINDINGS_NATIVE_NAME_FIELD ", CachedType, refCounted: "
					   << (itype.is_ref_counted ? "true" : "false") << ");\n"
					   << CLOSE_BLOCK_L2 CLOSE_BLOCK_L1;
			}

			output << MEMBER_BEGIN "internal " << itype.proxy_name << "(IntPtr " CS_PARAM_INSTANCE ") : this("
				   << (itype.memory_own ? "true" : "false") << ")\n" OPEN_BLOCK_L1
				   << INDENT2 "NativePtr = " CS_PARAM_INSTANCE ";\n"
				   << INDENT2 "unsafe\n" INDENT2 OPEN_BLOCK
				   << INDENT3 "ConstructAndInitialize(null, "
				   << BINDINGS_NATIVE_NAME_FIELD ", CachedType, refCounted: "
				   << (itype.is_ref_counted ? "true" : "false") << ");\n"
				   << CLOSE_BLOCK_L2 CLOSE_BLOCK_L1;

			// Add.. em.. trick constructor. Sort of.
			output.append(MEMBER_BEGIN "internal ");
			output.append(itype.proxy_name);
			output.append("(bool " CS_PARAM_MEMORYOWN ") : base(" CS_PARAM_MEMORYOWN ") { }\n");
		}
	}

	// Methods

	int method_bind_count = 0;
	for (const MethodInterface &imethod : itype.methods) {
		Error method_err = _generate_cs_method(itype, imethod, method_bind_count, output, false);
		ERR_FAIL_COND_V_MSG(method_err != OK, method_err,
				"Failed to generate method '" + imethod.name + "' for class '" + itype.name + "'.");
		if (imethod.is_internal) {
			// No need to generate span overloads for internal methods.
			continue;
		}

		method_err = _generate_cs_method(itype, imethod, method_bind_count, output, true);
		ERR_FAIL_COND_V_MSG(method_err != OK, method_err,
				"Failed to generate span overload method '" + imethod.name + "' for class '" + itype.name + "'.");
	}

	// Signals

	for (const SignalInterface &isignal : itype.signals_) {
		Error method_err = _generate_cs_signal(itype, isignal, output);
		ERR_FAIL_COND_V_MSG(method_err != OK, method_err,
				"Failed to generate signal '" + isignal.name + "' for class '" + itype.name + "'.");
	}

	// Script members look-up

	if (!itype.is_singleton && (is_derived_type || itype.has_virtual_methods)) {
		// Generate method names cache fields

		for (const MethodInterface &imethod : itype.methods) {
			if (!imethod.is_virtual) {
				continue;
			}

			output << MEMBER_BEGIN "// ReSharper disable once InconsistentNaming\n"
				   << INDENT1 "[DebuggerBrowsable(DebuggerBrowsableState.Never)]\n"
				   << INDENT1 "private static readonly StringName "
				   << CS_STATIC_FIELD_METHOD_PROXY_NAME_PREFIX << imethod.name
				   << " = \"" << imethod.proxy_name << "\";\n";
		}

		// Generate signal names cache fields

		for (const SignalInterface &isignal : itype.signals_) {
			output << MEMBER_BEGIN "// ReSharper disable once InconsistentNaming\n"
				   << INDENT1 "[DebuggerBrowsable(DebuggerBrowsableState.Never)]\n"
				   << INDENT1 "private static readonly StringName "
				   << CS_STATIC_FIELD_SIGNAL_PROXY_NAME_PREFIX << isignal.name
				   << " = \"" << isignal.proxy_name << "\";\n";
		}

		// TODO: Only generate HasGodotClassMethod and InvokeGodotClassMethod if there's any method

		// Generate InvokeGodotClassMethod

		output << MEMBER_BEGIN "/// <summary>\n"
			   << INDENT1 "/// Invokes the method with the given name, using the given arguments.\n"
			   << INDENT1 "/// This method is used by Godot to invoke methods from the engine side.\n"
			   << INDENT1 "/// Do not call or override this method.\n"
			   << INDENT1 "/// </summary>\n"
			   << INDENT1 "/// <param name=\"method\">Name of the method to invoke.</param>\n"
			   << INDENT1 "/// <param name=\"args\">Arguments to use with the invoked method.</param>\n"
			   << INDENT1 "/// <param name=\"ret\">Value returned by the invoked method.</param>\n";

		// Avoid raising diagnostics because of calls to obsolete methods.
		output << "#pragma warning disable CS0618 // Member is obsolete\n";

		output << INDENT1 "protected internal " << (is_derived_type ? "override" : "virtual")
			   << " bool " CS_METHOD_INVOKE_GODOT_CLASS_METHOD "(in godot_string_name method, "
			   << "NativeVariantPtrArgs args, out godot_variant ret)\n"
			   << INDENT1 "{\n";

		for (const MethodInterface &imethod : itype.methods) {
			if (!imethod.is_virtual) {
				continue;
			}

			// We also call HasGodotClassMethod to ensure the method is overridden and avoid calling
			// the stub implementation. This solution adds some extra overhead to calls, but it's
			// much simpler than other solutions. This won't be a problem once we move to function
			// pointers of generated wrappers for each method, as lookup will only happen once.

			// We check both native names (snake_case) and proxy names (PascalCase)
			output << INDENT2 "if ((method == " << CS_STATIC_FIELD_METHOD_PROXY_NAME_PREFIX << imethod.name
				   << " || method == MethodName." << imethod.proxy_name
				   << ") && args.Count == " << itos(imethod.arguments.size())
				   << " && " << CS_METHOD_HAS_GODOT_CLASS_METHOD << "((godot_string_name)"
				   << CS_STATIC_FIELD_METHOD_PROXY_NAME_PREFIX << imethod.name << ".NativeValue))\n"
				   << INDENT2 "{\n";

			if (imethod.return_type.cname != name_cache.type_void) {
				output << INDENT3 "var callRet = ";
			} else {
				output << INDENT3;
			}

			output << imethod.proxy_name << "(";

			int i = 0;
			for (List<BindingsGenerator::ArgumentInterface>::ConstIterator itr = imethod.arguments.begin(); itr != imethod.arguments.end(); ++itr, ++i) {
				const ArgumentInterface &iarg = *itr;

				const TypeInterface *arg_type = _get_type_or_null(iarg.type);
				ERR_FAIL_NULL_V_MSG(arg_type, ERR_BUG, "Argument type '" + iarg.type.cname + "' was not found.");

				if (i != 0) {
					output << ", ";
				}

				if (arg_type->cname == name_cache.type_Array_generic || arg_type->cname == name_cache.type_Dictionary_generic) {
					String arg_cs_type = arg_type->cs_type + _get_generic_type_parameters(*arg_type, iarg.type.generic_type_parameters);

					output << "new " << arg_cs_type << "(" << sformat(arg_type->cs_variant_to_managed, "args[" + itos(i) + "]", arg_type->cs_type, arg_type->name) << ")";
				} else {
					output << sformat(arg_type->cs_variant_to_managed,
							"args[" + itos(i) + "]", arg_type->cs_type, arg_type->name);
				}
			}

			output << ");\n";

			if (imethod.return_type.cname != name_cache.type_void) {
				const TypeInterface *return_type = _get_type_or_null(imethod.return_type);
				ERR_FAIL_NULL_V_MSG(return_type, ERR_BUG, "Return type '" + imethod.return_type.cname + "' was not found.");

				output << INDENT3 "ret = "
					   << sformat(return_type->cs_managed_to_variant, "callRet", return_type->cs_type, return_type->name)
					   << ";\n"
					   << INDENT3 "return true;\n";
			} else {
				output << INDENT3 "ret = default;\n"
					   << INDENT3 "return true;\n";
			}

			output << INDENT2 "}\n";
		}

		if (is_derived_type) {
			output << INDENT2 "return base." CS_METHOD_INVOKE_GODOT_CLASS_METHOD "(method, args, out ret);\n";
		} else {
			output << INDENT2 "ret = default;\n"
				   << INDENT2 "return false;\n";
		}

		output << INDENT1 "}\n";

		output << "#pragma warning restore CS0618\n";

		// Generate HasGodotClassMethod

		output << MEMBER_BEGIN "/// <summary>\n"
			   << INDENT1 "/// Check if the type contains a method with the given name.\n"
			   << INDENT1 "/// This method is used by Godot to check if a method exists before invoking it.\n"
			   << INDENT1 "/// Do not call or override this method.\n"
			   << INDENT1 "/// </summary>\n"
			   << INDENT1 "/// <param name=\"method\">Name of the method to check for.</param>\n";

		output << MEMBER_BEGIN "protected internal " << (is_derived_type ? "override" : "virtual")
			   << " bool " CS_METHOD_HAS_GODOT_CLASS_METHOD "(in godot_string_name method)\n"
			   << INDENT1 "{\n";

		for (const MethodInterface &imethod : itype.methods) {
			if (!imethod.is_virtual) {
				continue;
			}

			// We check for native names (snake_case). If we detect one, we call HasGodotClassMethod
			// again, but this time with the respective proxy name (PascalCase). It's the job of
			// user derived classes to override the method and check for those. Our C# source
			// generators take care of generating those override methods.
			output << INDENT2 "if (method == MethodName." << imethod.proxy_name
				   << ")\n" INDENT2 "{\n"
				   << INDENT3 "if (" CS_METHOD_HAS_GODOT_CLASS_METHOD "("
				   << CS_STATIC_FIELD_METHOD_PROXY_NAME_PREFIX << imethod.name
				   << ".NativeValue.DangerousSelfRef))\n" INDENT3 "{\n"
				   << INDENT4 "return true;\n"
				   << INDENT3 "}\n" INDENT2 "}\n";
		}

		if (is_derived_type) {
			output << INDENT2 "return base." CS_METHOD_HAS_GODOT_CLASS_METHOD "(method);\n";
		} else {
			output << INDENT2 "return false;\n";
		}

		output << INDENT1 "}\n";

		// Generate HasGodotClassSignal

		output << MEMBER_BEGIN "/// <summary>\n"
			   << INDENT1 "/// Check if the type contains a signal with the given name.\n"
			   << INDENT1 "/// This method is used by Godot to check if a signal exists before raising it.\n"
			   << INDENT1 "/// Do not call or override this method.\n"
			   << INDENT1 "/// </summary>\n"
			   << INDENT1 "/// <param name=\"signal\">Name of the signal to check for.</param>\n";

		output << MEMBER_BEGIN "protected internal " << (is_derived_type ? "override" : "virtual")
			   << " bool " CS_METHOD_HAS_GODOT_CLASS_SIGNAL "(in godot_string_name signal)\n"
			   << INDENT1 "{\n";

		for (const SignalInterface &isignal : itype.signals_) {
			// We check for native names (snake_case). If we detect one, we call HasGodotClassSignal
			// again, but this time with the respective proxy name (PascalCase). It's the job of
			// user derived classes to override the method and check for those. Our C# source
			// generators take care of generating those override methods.
			output << INDENT2 "if (signal == SignalName." << isignal.proxy_name
				   << ")\n" INDENT2 "{\n"
				   << INDENT3 "if (" CS_METHOD_HAS_GODOT_CLASS_SIGNAL "("
				   << CS_STATIC_FIELD_SIGNAL_PROXY_NAME_PREFIX << isignal.name
				   << ".NativeValue.DangerousSelfRef))\n" INDENT3 "{\n"
				   << INDENT4 "return true;\n"
				   << INDENT3 "}\n" INDENT2 "}\n";
		}

		if (is_derived_type) {
			output << INDENT2 "return base." CS_METHOD_HAS_GODOT_CLASS_SIGNAL "(signal);\n";
		} else {
			output << INDENT2 "return false;\n";
		}

		output << INDENT1 "}\n";
	}

	//Generate StringName for all class members
	bool is_inherit = !itype.is_singleton && obj_types.has(itype.base_name);
	//PropertyName
	output << MEMBER_BEGIN "/// <summary>\n"
		   << INDENT1 "/// Cached StringNames for the properties and fields contained in this class, for fast lookup.\n"
		   << INDENT1 "/// </summary>\n";
	if (is_inherit) {
		output << INDENT1 "public new class PropertyName : " << obj_types[itype.base_name].proxy_name << ".PropertyName";
	} else {
		output << INDENT1 "public class PropertyName";
	}
	output << "\n"
		   << INDENT1 "{\n";
	for (const PropertyInterface &iprop : itype.properties) {
		output << INDENT2 "/// <summary>\n"
			   << INDENT2 "/// Cached name for the '" << iprop.cname << "' property.\n"
			   << INDENT2 "/// </summary>\n"
			   << INDENT2 "public static "
			   << (prop_allowed_inherited_member_hiding.has(itype.proxy_name + ".PropertyName." + iprop.proxy_name) ? "new " : "")
			   << "readonly StringName " << iprop.proxy_name << " = \"" << iprop.cname << "\";\n";
	}
	output << INDENT1 "}\n";
	//MethodName
	output << MEMBER_BEGIN "/// <summary>\n"
		   << INDENT1 "/// Cached StringNames for the methods contained in this class, for fast lookup.\n"
		   << INDENT1 "/// </summary>\n";
	if (is_inherit) {
		output << INDENT1 "public new class MethodName : " << obj_types[itype.base_name].proxy_name << ".MethodName";
	} else {
		output << INDENT1 "public class MethodName";
	}
	output << "\n"
		   << INDENT1 "{\n";
	HashMap<String, StringName> method_names;
	for (const MethodInterface &imethod : itype.methods) {
		if (method_names.has(imethod.proxy_name)) {
			ERR_FAIL_COND_V_MSG(method_names[imethod.proxy_name] != imethod.cname, ERR_BUG, "Method name '" + imethod.proxy_name + "' already exists with a different value.");
			continue;
		}
		method_names[imethod.proxy_name] = imethod.cname;
		output << INDENT2 "/// <summary>\n"
			   << INDENT2 "/// Cached name for the '" << imethod.cname << "' method.\n"
			   << INDENT2 "/// </summary>\n"
			   << INDENT2 "public static "
			   << (prop_allowed_inherited_member_hiding.has(itype.proxy_name + ".MethodName." + imethod.proxy_name) ? "new " : "")
			   << "readonly StringName " << imethod.proxy_name << " = \"" << imethod.cname << "\";\n";
	}
	output << INDENT1 "}\n";
	//SignalName
	output << MEMBER_BEGIN "/// <summary>\n"
		   << INDENT1 "/// Cached StringNames for the signals contained in this class, for fast lookup.\n"
		   << INDENT1 "/// </summary>\n";
	if (is_inherit) {
		output << INDENT1 "public new class SignalName : " << obj_types[itype.base_name].proxy_name << ".SignalName";
	} else {
		output << INDENT1 "public class SignalName";
	}
	output << "\n"
		   << INDENT1 "{\n";
	for (const SignalInterface &isignal : itype.signals_) {
		output << INDENT2 "/// <summary>\n"
			   << INDENT2 "/// Cached name for the '" << isignal.cname << "' signal.\n"
			   << INDENT2 "/// </summary>\n"
			   << INDENT2 "public static "
			   << (prop_allowed_inherited_member_hiding.has(itype.proxy_name + ".SignalName." + isignal.proxy_name) ? "new " : "")
			   << "readonly StringName " << isignal.proxy_name << " = \"" << isignal.cname << "\";\n";
	}
	output << INDENT1 "}\n";

	output.append(CLOSE_BLOCK /* class */);

	return _save_file(p_output_file, output);
}

Error BindingsGenerator::_generate_cs_property(const BindingsGenerator::TypeInterface &p_itype, const PropertyInterface &p_iprop, StringBuilder &p_output) {
	const MethodInterface *setter = p_itype.find_method_by_name(p_iprop.setter);

	// Search it in base types too
	const TypeInterface *current_type = &p_itype;
	while (!setter && current_type->base_name != StringName()) {
		HashMap<StringName, TypeInterface>::Iterator base_match = obj_types.find(current_type->base_name);
		ERR_FAIL_COND_V_MSG(!base_match, ERR_BUG, "Type not found '" + current_type->base_name + "'. Inherited by '" + current_type->name + "'.");
		current_type = &base_match->value;
		setter = current_type->find_method_by_name(p_iprop.setter);
	}

	const MethodInterface *getter = p_itype.find_method_by_name(p_iprop.getter);

	// Search it in base types too
	current_type = &p_itype;
	while (!getter && current_type->base_name != StringName()) {
		HashMap<StringName, TypeInterface>::Iterator base_match = obj_types.find(current_type->base_name);
		ERR_FAIL_COND_V_MSG(!base_match, ERR_BUG, "Type not found '" + current_type->base_name + "'. Inherited by '" + current_type->name + "'.");
		current_type = &base_match->value;
		getter = current_type->find_method_by_name(p_iprop.getter);
	}

	ERR_FAIL_COND_V(!setter && !getter, ERR_BUG);

	if (setter) {
		int setter_argc = p_iprop.index != -1 ? 2 : 1;
		ERR_FAIL_COND_V(setter->arguments.size() != setter_argc, ERR_BUG);
	}

	if (getter) {
		int getter_argc = p_iprop.index != -1 ? 1 : 0;
		ERR_FAIL_COND_V(getter->arguments.size() != getter_argc, ERR_BUG);
	}

	if (getter && setter) {
		const ArgumentInterface &setter_first_arg = setter->arguments.back()->get();
		if (getter->return_type.cname != setter_first_arg.type.cname) {
			// Special case for Node::set_name
			bool whitelisted = getter->return_type.cname == name_cache.type_StringName &&
					setter_first_arg.type.cname == name_cache.type_String;

			ERR_FAIL_COND_V_MSG(!whitelisted, ERR_BUG,
					"Return type from getter doesn't match first argument of setter for property: '" +
							p_itype.name + "." + String(p_iprop.cname) + "'.");
		}
	}

	const TypeReference &proptype_name = getter ? getter->return_type : setter->arguments.back()->get().type;

	const TypeInterface *prop_itype = _get_type_or_singleton_or_null(proptype_name);
	ERR_FAIL_NULL_V_MSG(prop_itype, ERR_BUG, "Property type '" + proptype_name.cname + "' was not found.");

	ERR_FAIL_COND_V_MSG(prop_itype->is_singleton, ERR_BUG,
			"Property type is a singleton: '" + p_itype.name + "." + String(p_iprop.cname) + "'.");

	if (p_itype.api_type == ClassDB::API_CORE) {
		ERR_FAIL_COND_V_MSG(prop_itype->api_type == ClassDB::API_EDITOR, ERR_BUG,
				"Property '" + p_itype.name + "." + String(p_iprop.cname) + "' has type '" + prop_itype->name +
						"' from the editor API. Core API cannot have dependencies on the editor API.");
	}

	if (p_iprop.prop_doc && p_iprop.prop_doc->description.size()) {
		String xml_summary = bbcode_to_xml(fix_doc_description(p_iprop.prop_doc->description), &p_itype);
		Vector<String> summary_lines = xml_summary.length() ? xml_summary.split("\n") : Vector<String>();

		if (summary_lines.size()) {
			p_output.append(MEMBER_BEGIN "/// <summary>\n");

			for (int i = 0; i < summary_lines.size(); i++) {
				p_output.append(INDENT1 "/// ");
				p_output.append(summary_lines[i]);
				p_output.append("\n");
			}

			p_output.append(INDENT1 "/// </summary>");
		}
	}

	if (p_iprop.is_deprecated) {
		p_output.append(MEMBER_BEGIN "[Obsolete(\"");
		p_output.append(bbcode_to_text(p_iprop.deprecation_message, &p_itype));
		p_output.append("\")]");
	}

	if (p_iprop.is_hidden) {
		p_output.append(MEMBER_BEGIN "[EditorBrowsable(EditorBrowsableState.Never)]");
	}

	p_output.append(MEMBER_BEGIN "public ");

	if (prop_allowed_inherited_member_hiding.has(p_itype.proxy_name + "." + p_iprop.proxy_name)) {
		p_output.append("new ");
	}

	if (p_itype.is_singleton) {
		p_output.append("static ");
	}

	String prop_cs_type = prop_itype->cs_type + _get_generic_type_parameters(*prop_itype, proptype_name.generic_type_parameters);

	p_output.append(prop_cs_type);
	p_output.append(" ");
	p_output.append(p_iprop.proxy_name);
	p_output.append("\n" OPEN_BLOCK_L1);

	if (getter) {
		p_output.append(INDENT2 "get\n" OPEN_BLOCK_L2 INDENT3);

		p_output.append("return ");
		p_output.append(getter->proxy_name + "(");
		if (p_iprop.index != -1) {
			const ArgumentInterface &idx_arg = getter->arguments.front()->get();
			if (idx_arg.type.cname != name_cache.type_int) {
				// Assume the index parameter is an enum
				const TypeInterface *idx_arg_type = _get_type_or_null(idx_arg.type);
				CRASH_COND(idx_arg_type == nullptr);
				p_output.append("(" + idx_arg_type->proxy_name + ")(" + itos(p_iprop.index) + ")");
			} else {
				p_output.append(itos(p_iprop.index));
			}
		}
		p_output.append(");\n" CLOSE_BLOCK_L2);
	}

	if (setter) {
		p_output.append(INDENT2 "set\n" OPEN_BLOCK_L2 INDENT3);

		p_output.append(setter->proxy_name + "(");
		if (p_iprop.index != -1) {
			const ArgumentInterface &idx_arg = setter->arguments.front()->get();
			if (idx_arg.type.cname != name_cache.type_int) {
				// Assume the index parameter is an enum
				const TypeInterface *idx_arg_type = _get_type_or_null(idx_arg.type);
				CRASH_COND(idx_arg_type == nullptr);
				p_output.append("(" + idx_arg_type->proxy_name + ")(" + itos(p_iprop.index) + "), ");
			} else {
				p_output.append(itos(p_iprop.index) + ", ");
			}
		}
		p_output.append("value);\n" CLOSE_BLOCK_L2);
	}

	p_output.append(CLOSE_BLOCK_L1);

	return OK;
}

Error BindingsGenerator::_generate_cs_method(const BindingsGenerator::TypeInterface &p_itype, const BindingsGenerator::MethodInterface &p_imethod, int &p_method_bind_count, StringBuilder &p_output, bool p_use_span) {
	const TypeInterface *return_type = _get_type_or_singleton_or_null(p_imethod.return_type);
	ERR_FAIL_NULL_V_MSG(return_type, ERR_BUG, "Return type '" + p_imethod.return_type.cname + "' was not found.");

	ERR_FAIL_COND_V_MSG(return_type->is_singleton, ERR_BUG,
			"Method return type is a singleton: '" + p_itype.name + "." + p_imethod.name + "'.");

	if (p_itype.api_type == ClassDB::API_CORE) {
		ERR_FAIL_COND_V_MSG(return_type->api_type == ClassDB::API_EDITOR, ERR_BUG,
				"Method '" + p_itype.name + "." + p_imethod.name + "' has return type '" + return_type->name +
						"' from the editor API. Core API cannot have dependencies on the editor API.");
	}

	if (p_imethod.is_virtual && p_use_span) {
		return OK;
	}

	bool has_span_argument = false;

	if (p_use_span) {
		if (p_imethod.is_vararg) {
			has_span_argument = true;
		} else {
			for (const ArgumentInterface &iarg : p_imethod.arguments) {
				const TypeInterface *arg_type = _get_type_or_singleton_or_null(iarg.type);
				ERR_FAIL_NULL_V_MSG(arg_type, ERR_BUG, "Argument type '" + iarg.type.cname + "' was not found.");

				if (arg_type->is_span_compatible) {
					has_span_argument = true;
					break;
				}
			}
		}

		if (has_span_argument) {
			// Span overloads use the same method bind as the array overloads.
			// Since both overloads are generated one after the other, we can decrease the count here
			// to ensure the span overload uses the same method bind.
			p_method_bind_count--;
		}
	}

	String method_bind_field = CS_STATIC_FIELD_METHOD_BIND_PREFIX + itos(p_method_bind_count);

	String arguments_sig;
	StringBuilder cs_in_statements;
	bool cs_in_expr_is_unsafe = false;

	String icall_params = method_bind_field;

	if (!p_imethod.is_static) {
		String self_reference = "this";
		if (p_itype.is_singleton) {
			self_reference = CS_PROPERTY_SINGLETON;
		}

		if (p_itype.cs_in.size()) {
			cs_in_statements << sformat(p_itype.cs_in, p_itype.c_type, self_reference,
					String(), String(), String(), INDENT2);
		}

		icall_params += ", " + sformat(p_itype.cs_in_expr, self_reference);
	}

	StringBuilder default_args_doc;

	// Retrieve information from the arguments
	const ArgumentInterface &first = p_imethod.arguments.front()->get();
	for (const ArgumentInterface &iarg : p_imethod.arguments) {
		const TypeInterface *arg_type = _get_type_or_singleton_or_null(iarg.type);
		ERR_FAIL_NULL_V_MSG(arg_type, ERR_BUG, "Argument type '" + iarg.type.cname + "' was not found.");

		ERR_FAIL_COND_V_MSG(arg_type->is_singleton, ERR_BUG,
				"Argument type is a singleton: '" + iarg.name + "' of method '" + p_itype.name + "." + p_imethod.name + "'.");

		if (p_itype.api_type == ClassDB::API_CORE) {
			ERR_FAIL_COND_V_MSG(arg_type->api_type == ClassDB::API_EDITOR, ERR_BUG,
					"Argument '" + iarg.name + "' of method '" + p_itype.name + "." + p_imethod.name + "' has type '" +
							arg_type->name + "' from the editor API. Core API cannot have dependencies on the editor API.");
		}

		if (iarg.default_argument.size()) {
			CRASH_COND_MSG(!_arg_default_value_is_assignable_to_type(iarg.def_param_value, *arg_type),
					"Invalid default value for parameter '" + iarg.name + "' of method '" + p_itype.name + "." + p_imethod.name + "'.");
		}

		String arg_cs_type = arg_type->cs_type + _get_generic_type_parameters(*arg_type, iarg.type.generic_type_parameters);

		bool use_span_for_arg = p_use_span && arg_type->is_span_compatible;

		// Add the current arguments to the signature
		// If the argument has a default value which is not a constant, we will make it Nullable
		{
			if (&iarg != &first) {
				arguments_sig += ", ";
			}

			if (iarg.def_param_mode == ArgumentInterface::NULLABLE_VAL) {
				arguments_sig += "Nullable<";
			}

			if (use_span_for_arg) {
				arguments_sig += arg_type->c_type_in;
			} else {
				arguments_sig += arg_cs_type;
			}

			if (iarg.def_param_mode == ArgumentInterface::NULLABLE_VAL) {
				arguments_sig += "> ";
			} else {
				arguments_sig += " ";
			}

			arguments_sig += iarg.name;

			if (!p_use_span && !p_imethod.is_compat && iarg.default_argument.size()) {
				if (iarg.def_param_mode != ArgumentInterface::CONSTANT) {
					arguments_sig += " = null";
				} else {
					arguments_sig += " = " + sformat(iarg.default_argument, arg_type->cs_type);
				}
			}
		}

		icall_params += ", ";

		if (iarg.default_argument.size() && iarg.def_param_mode != ArgumentInterface::CONSTANT && !use_span_for_arg) {
			// The default value of an argument must be constant. Otherwise we make it Nullable and do the following:
			// Type arg_in = arg.HasValue ? arg.Value : <non-const default value>;
			String arg_or_defval_local = iarg.name;
			arg_or_defval_local += "OrDefVal";

			cs_in_statements << INDENT2 << arg_cs_type << " " << arg_or_defval_local << " = " << iarg.name;

			if (iarg.def_param_mode == ArgumentInterface::NULLABLE_VAL) {
				cs_in_statements << ".HasValue ? ";
			} else {
				cs_in_statements << " != null ? ";
			}

			cs_in_statements << iarg.name;

			if (iarg.def_param_mode == ArgumentInterface::NULLABLE_VAL) {
				cs_in_statements << ".Value : ";
			} else {
				cs_in_statements << " : ";
			}

			String cs_type = arg_cs_type;
			if (cs_type.ends_with("[]")) {
				cs_type = cs_type.substr(0, cs_type.length() - 2);
			}

			String def_arg = sformat(iarg.default_argument, cs_type);

			cs_in_statements << def_arg << ";\n";

			if (arg_type->cs_in.size()) {
				cs_in_statements << sformat(arg_type->cs_in, arg_type->c_type, arg_or_defval_local,
						String(), String(), String(), INDENT2);
			}

			if (arg_type->cs_in_expr.is_empty()) {
				icall_params += arg_or_defval_local;
			} else {
				icall_params += sformat(arg_type->cs_in_expr, arg_or_defval_local, arg_type->c_type);
			}

			// Apparently the name attribute must not include the @
			String param_tag_name = iarg.name.begins_with("@") ? iarg.name.substr(1, iarg.name.length()) : iarg.name;
			// Escape < and > in the attribute default value
			String param_def_arg = def_arg.replacen("<", "&lt;").replacen(">", "&gt;");

			default_args_doc.append(MEMBER_BEGIN "/// <param name=\"" + param_tag_name + "\">If the parameter is null, then the default value is <c>" + param_def_arg + "</c>.</param>");
		} else {
			if (arg_type->cs_in.size()) {
				cs_in_statements << sformat(arg_type->cs_in, arg_type->c_type, iarg.name,
						String(), String(), String(), INDENT2);
			}

			icall_params += arg_type->cs_in_expr.is_empty() ? iarg.name : sformat(arg_type->cs_in_expr, iarg.name, arg_type->c_type);
		}

		cs_in_expr_is_unsafe |= arg_type->cs_in_expr_is_unsafe;
	}

	if (p_use_span && !has_span_argument) {
		return OK;
	}

	// Collect caller name for MethodBind
	if (p_imethod.is_vararg) {
		icall_params += ", (godot_string_name)MethodName." + p_imethod.proxy_name + ".NativeValue";
	}

	// Generate method
	{
		if (!p_imethod.is_virtual && !p_imethod.requires_object_call && !p_use_span) {
			p_output << MEMBER_BEGIN "[DebuggerBrowsable(DebuggerBrowsableState.Never)]\n"
					 << INDENT1 "private static readonly IntPtr " << method_bind_field << " = ";

			if (p_itype.is_singleton) {
				// Singletons are static classes. They don't derive GodotObject,
				// so we need to specify the type to call the static method.
				p_output << "GodotObject.";
			}

			p_output << ICALL_CLASSDB_GET_METHOD_WITH_COMPATIBILITY "(" BINDINGS_NATIVE_NAME_FIELD ", MethodName."
					 << p_imethod.proxy_name << ", " << itos(p_imethod.hash) << "ul"
					 << ");\n";
		}

		if (p_imethod.method_doc && p_imethod.method_doc->description.size()) {
			String xml_summary = bbcode_to_xml(fix_doc_description(p_imethod.method_doc->description), &p_itype);
			Vector<String> summary_lines = xml_summary.length() ? xml_summary.split("\n") : Vector<String>();

			if (summary_lines.size()) {
				p_output.append(MEMBER_BEGIN "/// <summary>\n");

				for (int i = 0; i < summary_lines.size(); i++) {
					p_output.append(INDENT1 "/// ");
					p_output.append(summary_lines[i]);
					p_output.append("\n");
				}

				p_output.append(INDENT1 "/// </summary>");
			}
		}

		if (default_args_doc.get_string_length()) {
			p_output.append(default_args_doc.as_string());
		}

		if (p_imethod.is_deprecated) {
			p_output.append(MEMBER_BEGIN "[Obsolete(\"");
			p_output.append(bbcode_to_text(p_imethod.deprecation_message, &p_itype));
			p_output.append("\")]");
		}

		if (p_imethod.is_hidden) {
			p_output.append(MEMBER_BEGIN "[EditorBrowsable(EditorBrowsableState.Never)]");
		}

		p_output.append(MEMBER_BEGIN);
		p_output.append(p_imethod.is_internal ? "internal " : "public ");

		if (prop_allowed_inherited_member_hiding.has(p_itype.proxy_name + "." + p_imethod.proxy_name)) {
			p_output.append("new ");
		}

		if (p_itype.is_singleton || p_imethod.is_static) {
			p_output.append("static ");
		} else if (p_imethod.is_virtual) {
			p_output.append("virtual ");
		}

		if (cs_in_expr_is_unsafe) {
			p_output.append("unsafe ");
		}

		String return_cs_type = return_type->cs_type + _get_generic_type_parameters(*return_type, p_imethod.return_type.generic_type_parameters);

		p_output.append(return_cs_type + " ");
		p_output.append(p_imethod.proxy_name + "(");
		p_output.append(arguments_sig + ")\n" OPEN_BLOCK_L1);

		if (p_imethod.is_virtual) {
			// Godot virtual method must be overridden, therefore we return a default value by default.

			if (return_type->cname == name_cache.type_void) {
				p_output.append(CLOSE_BLOCK_L1);
			} else {
				p_output.append(INDENT2 "return default;\n" CLOSE_BLOCK_L1);
			}

			return OK; // Won't increment method bind count
		}

		if (p_imethod.requires_object_call) {
			// Fallback to Godot's object.Call(string, params)

			p_output.append(INDENT2 CS_METHOD_CALL "(\"");
			p_output.append(p_imethod.name);
			p_output.append("\"");

			for (const ArgumentInterface &iarg : p_imethod.arguments) {
				p_output.append(", ");
				p_output.append(iarg.name);
			}

			p_output.append(");\n" CLOSE_BLOCK_L1);

			return OK; // Won't increment method bind count
		}

		HashMap<const MethodInterface *, const InternalCall *>::ConstIterator match = method_icalls_map.find(&p_imethod);
		ERR_FAIL_NULL_V(match, ERR_BUG);

		const InternalCall *im_icall = match->value;

		String im_call = im_icall->editor_only ? BINDINGS_CLASS_NATIVECALLS_EDITOR : BINDINGS_CLASS_NATIVECALLS;
		im_call += ".";
		im_call += im_icall->name;

		if (p_imethod.arguments.size() && cs_in_statements.get_string_length() > 0) {
			p_output.append(cs_in_statements.as_string());
		}

		if (return_type->cname == name_cache.type_void) {
			p_output << INDENT2 << im_call << "(" << icall_params << ");\n";
		} else if (return_type->cs_out.is_empty()) {
			p_output << INDENT2 "return " << im_call << "(" << icall_params << ");\n";
		} else {
			p_output.append(sformat(return_type->cs_out, im_call, icall_params,
					return_cs_type, return_type->c_type_out, String(), INDENT2));
			p_output.append("\n");
		}

		p_output.append(CLOSE_BLOCK_L1);
	}

	p_method_bind_count++;

	return OK;
}

Error BindingsGenerator::_generate_cs_signal(const BindingsGenerator::TypeInterface &p_itype, const BindingsGenerator::SignalInterface &p_isignal, StringBuilder &p_output) {
	String arguments_sig;

	// Retrieve information from the arguments
	const ArgumentInterface &first = p_isignal.arguments.front()->get();
	for (const ArgumentInterface &iarg : p_isignal.arguments) {
		const TypeInterface *arg_type = _get_type_or_singleton_or_null(iarg.type);
		ERR_FAIL_NULL_V_MSG(arg_type, ERR_BUG, "Argument type '" + iarg.type.cname + "' was not found.");

		ERR_FAIL_COND_V_MSG(arg_type->is_singleton, ERR_BUG,
				"Argument type is a singleton: '" + iarg.name + "' of signal '" + p_itype.name + "." + p_isignal.name + "'.");

		if (p_itype.api_type == ClassDB::API_CORE) {
			ERR_FAIL_COND_V_MSG(arg_type->api_type == ClassDB::API_EDITOR, ERR_BUG,
					"Argument '" + iarg.name + "' of signal '" + p_itype.name + "." + p_isignal.name + "' has type '" +
							arg_type->name + "' from the editor API. Core API cannot have dependencies on the editor API.");
		}

		// Add the current arguments to the signature

		if (&iarg != &first) {
			arguments_sig += ", ";
		}

		String arg_cs_type = arg_type->cs_type + _get_generic_type_parameters(*arg_type, iarg.type.generic_type_parameters);

		arguments_sig += arg_cs_type;
		arguments_sig += " ";
		arguments_sig += iarg.name;
	}

	// Generate signal
	{
		bool is_parameterless = p_isignal.arguments.size() == 0;

		// Delegate name is [SignalName]EventHandler
		String delegate_name = is_parameterless ? "Action" : p_isignal.proxy_name + "EventHandler";

		if (!is_parameterless) {
			p_output.append(MEMBER_BEGIN "/// <summary>\n");
			p_output.append(INDENT1 "/// ");
			p_output.append("Represents the method that handles the ");
			p_output.append("<see cref=\"" BINDINGS_NAMESPACE "." + p_itype.proxy_name + "." + p_isignal.proxy_name + "\"/>");
			p_output.append(" event of a ");
			p_output.append("<see cref=\"" BINDINGS_NAMESPACE "." + p_itype.proxy_name + "\"/>");
			p_output.append(" class.\n");
			p_output.append(INDENT1 "/// </summary>");

			if (p_isignal.is_deprecated) {
				p_output.append(MEMBER_BEGIN "[Obsolete(\"");
				p_output.append(bbcode_to_text(p_isignal.deprecation_message, &p_itype));
				p_output.append("\")]");
			}

			// Generate delegate
			p_output.append(MEMBER_BEGIN "public delegate void ");
			p_output.append(delegate_name);
			p_output.append("(");
			p_output.append(arguments_sig);
			p_output.append(");\n");

			// Generate Callable trampoline for the delegate
			p_output << MEMBER_BEGIN "private static void " << p_isignal.proxy_name << "Trampoline"
					 << "(object delegateObj, NativeVariantPtrArgs args, out godot_variant ret)\n"
					 << INDENT1 "{\n"
					 << INDENT2 "Callable.ThrowIfArgCountMismatch(args, " << itos(p_isignal.arguments.size()) << ");\n"
					 << INDENT2 "((" << delegate_name << ")delegateObj)(";

			int idx = 0;
			for (const ArgumentInterface &iarg : p_isignal.arguments) {
				const TypeInterface *arg_type = _get_type_or_null(iarg.type);
				ERR_FAIL_NULL_V_MSG(arg_type, ERR_BUG, "Argument type '" + iarg.type.cname + "' was not found.");

				if (idx != 0) {
					p_output << ", ";
				}

				if (arg_type->cname == name_cache.type_Array_generic || arg_type->cname == name_cache.type_Dictionary_generic) {
					String arg_cs_type = arg_type->cs_type + _get_generic_type_parameters(*arg_type, iarg.type.generic_type_parameters);

					p_output << "new " << arg_cs_type << "(" << sformat(arg_type->cs_variant_to_managed, "args[" + itos(idx) + "]", arg_type->cs_type, arg_type->name) << ")";
				} else {
					p_output << sformat(arg_type->cs_variant_to_managed,
							"args[" + itos(idx) + "]", arg_type->cs_type, arg_type->name);
				}

				idx++;
			}

			p_output << ");\n"
					 << INDENT2 "ret = default;\n"
					 << INDENT1 "}\n";
		}

		if (p_isignal.method_doc && p_isignal.method_doc->description.size()) {
			String xml_summary = bbcode_to_xml(fix_doc_description(p_isignal.method_doc->description), &p_itype, true);
			Vector<String> summary_lines = xml_summary.length() ? xml_summary.split("\n") : Vector<String>();

			if (summary_lines.size()) {
				p_output.append(MEMBER_BEGIN "/// <summary>\n");

				for (int i = 0; i < summary_lines.size(); i++) {
					p_output.append(INDENT1 "/// ");
					p_output.append(summary_lines[i]);
					p_output.append("\n");
				}

				p_output.append(INDENT1 "/// </summary>");
			}
		}

		if (p_isignal.is_deprecated) {
			p_output.append(MEMBER_BEGIN "[Obsolete(\"");
			p_output.append(bbcode_to_text(p_isignal.deprecation_message, &p_itype));
			p_output.append("\")]");
		}

		// TODO:
		// Could we assume the StringName instance of signal name will never be freed (it's stored in ClassDB) before the managed world is unloaded?
		// If so, we could store the pointer we get from `data_unique_pointer()` instead of allocating StringName here.

		// Generate event
		p_output.append(MEMBER_BEGIN "public ");

		if (p_itype.is_singleton) {
			p_output.append("static ");
		}

		if (!is_parameterless) {
			// `unsafe` is needed for taking the trampoline's function pointer
			p_output << "unsafe ";
		}

		p_output.append("event ");
		p_output.append(delegate_name);
		p_output.append(" ");
		p_output.append(p_isignal.proxy_name);
		p_output.append("\n" OPEN_BLOCK_L1 INDENT2);

		if (p_itype.is_singleton) {
			p_output.append("add => " CS_PROPERTY_SINGLETON ".Connect(SignalName.");
		} else {
			p_output.append("add => Connect(SignalName.");
		}

		if (is_parameterless) {
			// Delegate type is Action. No need for custom trampoline.
			p_output << p_isignal.proxy_name << ", Callable.From(value));\n";
		} else {
			p_output << p_isignal.proxy_name
					 << ", Callable.CreateWithUnsafeTrampoline(value, &" << p_isignal.proxy_name << "Trampoline));\n";
		}

		if (p_itype.is_singleton) {
			p_output.append(INDENT2 "remove => " CS_PROPERTY_SINGLETON ".Disconnect(SignalName.");
		} else {
			p_output.append(INDENT2 "remove => Disconnect(SignalName.");
		}

		if (is_parameterless) {
			// Delegate type is Action. No need for custom trampoline.
			p_output << p_isignal.proxy_name << ", Callable.From(value));\n";
		} else {
			p_output << p_isignal.proxy_name
					 << ", Callable.CreateWithUnsafeTrampoline(value, &" << p_isignal.proxy_name << "Trampoline));\n";
		}

		p_output.append(CLOSE_BLOCK_L1);

		// Generate EmitSignal{EventName} method to raise the event.
		if (!p_itype.is_singleton) {
			p_output.append(MEMBER_BEGIN "protected void ");
			p_output << "EmitSignal" << p_isignal.proxy_name;
			if (is_parameterless) {
				p_output.append("()\n" OPEN_BLOCK_L1 INDENT2);
				p_output << "EmitSignal(SignalName." << p_isignal.proxy_name << ");\n";
				p_output.append(CLOSE_BLOCK_L1);
			} else {
				p_output.append("(");

				StringBuilder cs_emitsignal_params;

				int idx = 0;
				for (const ArgumentInterface &iarg : p_isignal.arguments) {
					const TypeInterface *arg_type = _get_type_or_null(iarg.type);
					ERR_FAIL_NULL_V_MSG(arg_type, ERR_BUG, "Argument type '" + iarg.type.cname + "' was not found.");

					if (idx != 0) {
						p_output << ", ";
						cs_emitsignal_params << ", ";
					}

					String arg_cs_type = arg_type->cs_type + _get_generic_type_parameters(*arg_type, iarg.type.generic_type_parameters);

					p_output << arg_cs_type << " " << iarg.name;

					if (arg_type->is_enum) {
						cs_emitsignal_params << "(long)";
					}

					cs_emitsignal_params << iarg.name;

					idx++;
				}

				p_output.append(")\n" OPEN_BLOCK_L1 INDENT2);
				p_output << "EmitSignal(SignalName." << p_isignal.proxy_name << ", " << cs_emitsignal_params << ");\n";
				p_output.append(CLOSE_BLOCK_L1);
			}
		}
	}

	return OK;
}

Error BindingsGenerator::_generate_cs_native_calls(const InternalCall &p_icall, StringBuilder &r_output) {
	bool ret_void = p_icall.return_type.cname == name_cache.type_void;

	const TypeInterface *return_type = _get_type_or_null(p_icall.return_type);
	ERR_FAIL_NULL_V_MSG(return_type, ERR_BUG, "Return type '" + p_icall.return_type.cname + "' was not found.");

	StringBuilder c_func_sig;
	StringBuilder c_in_statements;
	StringBuilder c_args_var_content;

	c_func_sig << "IntPtr " CS_PARAM_METHODBIND;

	if (!p_icall.is_static) {
		c_func_sig += ", IntPtr " CS_PARAM_INSTANCE;
	}

	// Get arguments information
	int i = 0;
	for (const TypeReference &arg_type_ref : p_icall.argument_types) {
		const TypeInterface *arg_type = _get_type_or_null(arg_type_ref);
		ERR_FAIL_NULL_V_MSG(arg_type, ERR_BUG, "Argument type '" + arg_type_ref.cname + "' was not found.");

		String c_param_name = "arg" + itos(i + 1);

		if (p_icall.is_vararg) {
			if (i < p_icall.get_arguments_count() - 1) {
				String c_in_vararg = arg_type->c_in_vararg;

				if (arg_type->is_object_type) {
					c_in_vararg = "%5using godot_variant %1_in = VariantUtils.CreateFromGodotObjectPtr(%1);\n";
				}

				ERR_FAIL_COND_V_MSG(c_in_vararg.is_empty(), ERR_BUG,
						"VarArg support not implemented for parameter type: " + arg_type->name);

				c_in_statements
						<< sformat(c_in_vararg, return_type->c_type, c_param_name,
								   String(), String(), String(), INDENT3)
						<< INDENT3 C_LOCAL_PTRCALL_ARGS "[" << itos(i)
						<< "] = new IntPtr(&" << c_param_name << "_in);\n";
			}
		} else {
			if (i > 0) {
				c_args_var_content << ", ";
			}
			if (arg_type->c_in.size()) {
				c_in_statements << sformat(arg_type->c_in, arg_type->c_type, c_param_name,
						String(), String(), String(), INDENT2);
			}
			c_args_var_content << sformat(arg_type->c_arg_in, c_param_name);
		}

		c_func_sig << ", " << arg_type->c_type_in << " " << c_param_name;

		i++;
	}

	// Collect caller name for MethodBind
	if (p_icall.is_vararg) {
		c_func_sig << ", godot_string_name caller";
	}

	String icall_method = p_icall.name;

	// Generate icall function

	r_output << MEMBER_BEGIN "internal static unsafe " << (ret_void ? "void" : return_type->c_type_out) << " "
			 << icall_method << "(" << c_func_sig.as_string() << ")\n" OPEN_BLOCK_L1;

	if (!p_icall.is_static) {
		r_output << INDENT2 "ExceptionUtils.ThrowIfNullPtr(" CS_PARAM_INSTANCE ");\n";
	}

	if (!ret_void && (!p_icall.is_vararg || return_type->cname != name_cache.type_Variant)) {
		String ptrcall_return_type;
		String initialization;

		if (return_type->is_object_type) {
			ptrcall_return_type = return_type->is_ref_counted ? "godot_ref" : return_type->c_type;
			initialization = " = default";
		} else {
			ptrcall_return_type = return_type->c_type;
		}

		r_output << INDENT2;

		if (return_type->is_ref_counted || return_type->c_type_is_disposable_struct) {
			r_output << "using ";

			if (initialization.is_empty()) {
				initialization = " = default";
			}
		} else if (return_type->c_ret_needs_default_initialization) {
			initialization = " = default";
		}

		r_output << ptrcall_return_type << " " C_LOCAL_RET << initialization << ";\n";
	}

	String argc_str = itos(p_icall.get_arguments_count());

	auto generate_call_and_return_stmts = [&](const char *base_indent) {
		if (p_icall.is_vararg) {
			// MethodBind Call
			r_output << base_indent;

			// VarArg methods always return Variant, but there are some cases in which MethodInfo provides
			// a specific return type. We trust this information is valid. We need a temporary local to keep
			// the Variant alive until the method returns. Otherwise, if the returned Variant holds a RefPtr,
			// it could be deleted too early. This is the case with GDScript.new() which returns OBJECT.
			// Alternatively, we could just return Variant, but that would result in a worse API.

			if (!ret_void) {
				if (return_type->cname != name_cache.type_Variant) {
					// Usually the return value takes ownership, but in this case the variant is only used
					// for conversion to another return type. As such, the local variable takes ownership.
					r_output << "using godot_variant " << C_LOCAL_VARARG_RET " = ";
				} else {
					// Variant's [c_out] takes ownership of the variant value
					r_output << "godot_variant " << C_LOCAL_RET " = ";
				}
			}

			r_output << C_CLASS_NATIVE_FUNCS ".godotsharp_method_bind_call("
					 << CS_PARAM_METHODBIND ", " << (p_icall.is_static ? "IntPtr.Zero" : CS_PARAM_INSTANCE)
					 << ", " << (p_icall.get_arguments_count() ? "(godot_variant**)" C_LOCAL_PTRCALL_ARGS : "null")
					 << ", total_length, out godot_variant_call_error vcall_error);\n";

			r_output << base_indent << "ExceptionUtils.DebugCheckCallError(caller"
					 << ", " << (p_icall.is_static ? "IntPtr.Zero" : CS_PARAM_INSTANCE)
					 << ", " << (p_icall.get_arguments_count() ? "(godot_variant**)" C_LOCAL_PTRCALL_ARGS : "null")
					 << ", total_length, vcall_error);\n";

			if (!ret_void) {
				if (return_type->cname != name_cache.type_Variant) {
					if (return_type->cname == name_cache.enum_Error) {
						r_output << base_indent << C_LOCAL_RET " = VariantUtils.ConvertToInt64(" C_LOCAL_VARARG_RET ");\n";
					} else {
						// TODO: Use something similar to c_in_vararg (see usage above, with error if not implemented)
						CRASH_NOW_MSG("Custom VarArg return type not implemented: " + return_type->name);
						r_output << base_indent << C_LOCAL_RET " = " C_LOCAL_VARARG_RET ";\n";
					}
				}
			}
		} else {
			// MethodBind PtrCall
			r_output << base_indent << C_CLASS_NATIVE_FUNCS ".godotsharp_method_bind_ptrcall("
					 << CS_PARAM_METHODBIND ", " << (p_icall.is_static ? "IntPtr.Zero" : CS_PARAM_INSTANCE)
					 << ", " << (p_icall.get_arguments_count() ? C_LOCAL_PTRCALL_ARGS : "null")
					 << ", " << (!ret_void ? "&" C_LOCAL_RET ");\n" : "null);\n");
		}

		// Return statement

		if (!ret_void) {
			if (return_type->c_out.is_empty()) {
				r_output << base_indent << "return " C_LOCAL_RET ";\n";
			} else {
				r_output << sformat(return_type->c_out, return_type->c_type_out, C_LOCAL_RET,
						return_type->name, String(), String(), base_indent);
			}
		}
	};

	if (p_icall.get_arguments_count()) {
		if (p_icall.is_vararg) {
			String vararg_arg = "arg" + argc_str;
			String real_argc_str = itos(p_icall.get_arguments_count() - 1); // Arguments count without vararg

			p_icall.get_arguments_count();

			r_output << INDENT2 "int vararg_length = " << vararg_arg << ".Length;\n"
					 << INDENT2 "int total_length = " << real_argc_str << " + vararg_length;\n";

			r_output << INDENT2 "Span<godot_variant.movable> varargs_span = vararg_length <= VarArgsSpanThreshold ?\n"
					 << INDENT3 "stackalloc godot_variant.movable[VarArgsSpanThreshold] :\n"
					 << INDENT3 "new godot_variant.movable[vararg_length];\n";

			r_output << INDENT2 "Span<IntPtr> " C_LOCAL_PTRCALL_ARGS "_span = total_length <= VarArgsSpanThreshold ?\n"
					 << INDENT3 "stackalloc IntPtr[VarArgsSpanThreshold] :\n"
					 << INDENT3 "new IntPtr[total_length];\n";

			r_output << INDENT2 "fixed (godot_variant.movable* varargs = &MemoryMarshal.GetReference(varargs_span))\n"
					 << INDENT2 "fixed (IntPtr* " C_LOCAL_PTRCALL_ARGS " = "
								"&MemoryMarshal.GetReference(" C_LOCAL_PTRCALL_ARGS "_span))\n"
					 << OPEN_BLOCK_L2;

			r_output << c_in_statements.as_string();

			r_output << INDENT3 "for (int i = 0; i < vararg_length; i++)\n" OPEN_BLOCK_L3
					 << INDENT4 "varargs[i] = " << vararg_arg << "[i].NativeVar;\n"
					 << INDENT4 C_LOCAL_PTRCALL_ARGS "[" << real_argc_str << " + i] = new IntPtr(&varargs[i]);\n"
					 << CLOSE_BLOCK_L3;

			generate_call_and_return_stmts(INDENT3);

			r_output << CLOSE_BLOCK_L2;
		} else {
			r_output << c_in_statements.as_string();

			r_output << INDENT2 "void** " C_LOCAL_PTRCALL_ARGS " = stackalloc void*["
					 << argc_str << "] { " << c_args_var_content.as_string() << " };\n";

			generate_call_and_return_stmts(INDENT2);
		}
	} else {
		generate_call_and_return_stmts(INDENT2);
	}

	r_output << CLOSE_BLOCK_L1;

	return OK;
}

Error BindingsGenerator::_save_file(const String &p_path, const StringBuilder &p_content) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_FILE_CANT_WRITE, "Cannot open file: '" + p_path + "'.");

	file->store_string(p_content.as_string());

	return OK;
}

const BindingsGenerator::TypeInterface *BindingsGenerator::_get_type_or_null(const TypeReference &p_typeref) {
	HashMap<StringName, TypeInterface>::ConstIterator builtin_type_match = builtin_types.find(p_typeref.cname);

	if (builtin_type_match) {
		return &builtin_type_match->value;
	}

	HashMap<StringName, TypeInterface>::ConstIterator obj_type_match = obj_types.find(p_typeref.cname);

	if (obj_type_match) {
		return &obj_type_match->value;
	}

	if (p_typeref.is_enum) {
		HashMap<StringName, TypeInterface>::ConstIterator enum_match = enum_types.find(p_typeref.cname);

		if (enum_match) {
			return &enum_match->value;
		}

		// Enum not found. Most likely because none of its constants were bound, so it's empty. That's fine. Use int instead.
		HashMap<StringName, TypeInterface>::ConstIterator int_match = builtin_types.find(name_cache.type_int);
		ERR_FAIL_NULL_V(int_match, nullptr);
		return &int_match->value;
	}

	return nullptr;
}

const BindingsGenerator::TypeInterface *BindingsGenerator::_get_type_or_singleton_or_null(const TypeReference &p_typeref) {
	const TypeInterface *itype = _get_type_or_null(p_typeref);
	if (itype == nullptr) {
		return nullptr;
	}

	if (itype->is_singleton) {
		StringName instance_type_name = itype->name + CS_SINGLETON_INSTANCE_SUFFIX;
		itype = &obj_types.find(instance_type_name)->value;
	}

	return itype;
}

const String BindingsGenerator::_get_generic_type_parameters(const TypeInterface &p_itype, const List<TypeReference> &p_generic_type_parameters) {
	if (p_generic_type_parameters.is_empty()) {
		return "";
	}

	ERR_FAIL_COND_V_MSG(p_itype.type_parameter_count != p_generic_type_parameters.size(), "",
			"Generic type parameter count mismatch for type '" + p_itype.name + "'." +
					" Found " + itos(p_generic_type_parameters.size()) + ", but requires " +
					itos(p_itype.type_parameter_count) + ".");

	int i = 0;
	String params = "<";
	for (const TypeReference &param_type : p_generic_type_parameters) {
		const TypeInterface *param_itype = _get_type_or_singleton_or_null(param_type);
		ERR_FAIL_NULL_V_MSG(param_itype, "", "Parameter type '" + param_type.cname + "' was not found.");

		ERR_FAIL_COND_V_MSG(param_itype->is_singleton, "",
				"Generic type parameter is a singleton: '" + param_itype->name + "'.");

		if (p_itype.api_type == ClassDB::API_CORE) {
			ERR_FAIL_COND_V_MSG(param_itype->api_type == ClassDB::API_EDITOR, "",
					"Generic type parameter '" + param_itype->name + "' has type from the editor API." +
							" Core API cannot have dependencies on the editor API.");
		}

		params += param_itype->cs_type;
		if (i < p_generic_type_parameters.size() - 1) {
			params += ", ";
		}

		i++;
	}
	params += ">";

	return params;
}

StringName BindingsGenerator::_get_type_name_from_meta(Variant::Type p_type, GodotTypeInfo::Metadata p_meta) {
	if (p_type == Variant::INT) {
		return _get_int_type_name_from_meta(p_meta);
	} else if (p_type == Variant::FLOAT) {
		return _get_float_type_name_from_meta(p_meta);
	} else {
		return Variant::get_type_name(p_type);
	}
}

StringName BindingsGenerator::_get_int_type_name_from_meta(GodotTypeInfo::Metadata p_meta) {
	switch (p_meta) {
		case GodotTypeInfo::METADATA_INT_IS_INT8:
			return "sbyte";
			break;
		case GodotTypeInfo::METADATA_INT_IS_INT16:
			return "short";
			break;
		case GodotTypeInfo::METADATA_INT_IS_INT32:
			return "int";
			break;
		case GodotTypeInfo::METADATA_INT_IS_INT64:
			return "long";
			break;
		case GodotTypeInfo::METADATA_INT_IS_UINT8:
			return "byte";
			break;
		case GodotTypeInfo::METADATA_INT_IS_UINT16:
			return "ushort";
			break;
		case GodotTypeInfo::METADATA_INT_IS_UINT32:
			return "uint";
			break;
		case GodotTypeInfo::METADATA_INT_IS_UINT64:
			return "ulong";
			break;
		case GodotTypeInfo::METADATA_INT_IS_CHAR16:
			return "char";
			break;
		case GodotTypeInfo::METADATA_INT_IS_CHAR32:
			// To prevent breaking compatibility, C# bindings need to keep using `long`.
			return "long";
		default:
			// Assume INT64
			return "long";
	}
}

StringName BindingsGenerator::_get_float_type_name_from_meta(GodotTypeInfo::Metadata p_meta) {
	switch (p_meta) {
		case GodotTypeInfo::METADATA_REAL_IS_FLOAT:
			return "float";
			break;
		case GodotTypeInfo::METADATA_REAL_IS_DOUBLE:
			return "double";
			break;
		default:
			// Assume FLOAT64
			return "double";
	}
}

bool BindingsGenerator::_arg_default_value_is_assignable_to_type(const Variant &p_val, const TypeInterface &p_arg_type) {
	if (p_arg_type.name == name_cache.type_Variant) {
		// Variant can take anything
		return true;
	}

	switch (p_val.get_type()) {
		case Variant::NIL:
			return p_arg_type.is_object_type ||
					name_cache.is_nullable_type(p_arg_type.name);
		case Variant::BOOL:
			return p_arg_type.name == name_cache.type_bool;
		case Variant::INT:
			return p_arg_type.name == name_cache.type_sbyte ||
					p_arg_type.name == name_cache.type_short ||
					p_arg_type.name == name_cache.type_int ||
					p_arg_type.name == name_cache.type_byte ||
					p_arg_type.name == name_cache.type_ushort ||
					p_arg_type.name == name_cache.type_uint ||
					p_arg_type.name == name_cache.type_long ||
					p_arg_type.name == name_cache.type_ulong ||
					p_arg_type.name == name_cache.type_float ||
					p_arg_type.name == name_cache.type_double ||
					p_arg_type.is_enum;
		case Variant::FLOAT:
			return p_arg_type.name == name_cache.type_float ||
					p_arg_type.name == name_cache.type_double;
		case Variant::STRING:
		case Variant::STRING_NAME:
			return p_arg_type.name == name_cache.type_String ||
					p_arg_type.name == name_cache.type_StringName ||
					p_arg_type.name == name_cache.type_NodePath;
		case Variant::NODE_PATH:
			return p_arg_type.name == name_cache.type_NodePath;
		case Variant::TRANSFORM2D:
		case Variant::TRANSFORM3D:
		case Variant::BASIS:
		case Variant::QUATERNION:
		case Variant::PLANE:
		case Variant::AABB:
		case Variant::COLOR:
		case Variant::VECTOR2:
		case Variant::RECT2:
		case Variant::VECTOR3:
		case Variant::RID:
		case Variant::PACKED_BYTE_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
		case Variant::PACKED_STRING_ARRAY:
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_VECTOR4_ARRAY:
		case Variant::PACKED_COLOR_ARRAY:
		case Variant::CALLABLE:
		case Variant::SIGNAL:
			return p_arg_type.name == Variant::get_type_name(p_val.get_type());
		case Variant::ARRAY:
			return p_arg_type.name == Variant::get_type_name(p_val.get_type()) || p_arg_type.cname == name_cache.type_Array_generic;
		case Variant::DICTIONARY:
			return p_arg_type.name == Variant::get_type_name(p_val.get_type()) || p_arg_type.cname == name_cache.type_Dictionary_generic;
		case Variant::OBJECT:
			return p_arg_type.is_object_type;
		case Variant::VECTOR2I:
			return p_arg_type.name == name_cache.type_Vector2 ||
					p_arg_type.name == Variant::get_type_name(p_val.get_type());
		case Variant::RECT2I:
			return p_arg_type.name == name_cache.type_Rect2 ||
					p_arg_type.name == Variant::get_type_name(p_val.get_type());
		case Variant::VECTOR3I:
			return p_arg_type.name == name_cache.type_Vector3 ||
					p_arg_type.name == Variant::get_type_name(p_val.get_type());
		default:
			CRASH_NOW_MSG("Unexpected Variant type: " + itos(p_val.get_type()));
			break;
	}

	return false;
}

bool method_has_ptr_parameter(MethodInfo p_method_info) {
	if (p_method_info.return_val.type == Variant::INT && p_method_info.return_val.hint == PROPERTY_HINT_INT_IS_POINTER) {
		return true;
	}
	for (PropertyInfo arg : p_method_info.arguments) {
		if (arg.type == Variant::INT && arg.hint == PROPERTY_HINT_INT_IS_POINTER) {
			return true;
		}
	}
	return false;
}

struct SortMethodWithHashes {
	_FORCE_INLINE_ bool operator()(const Pair<MethodInfo, uint32_t> &p_a, const Pair<MethodInfo, uint32_t> &p_b) const {
		return p_a.first < p_b.first;
	}
};

bool BindingsGenerator::_populate_object_type_interfaces() {
	obj_types.clear();

	List<StringName> class_list;
	ClassDB::get_class_list(&class_list);
	class_list.sort_custom<StringName::AlphCompare>();

	while (class_list.size()) {
		StringName type_cname = class_list.front()->get();

		ClassDB::APIType api_type = ClassDB::get_api_type(type_cname);

		if (api_type == ClassDB::API_NONE) {
			class_list.pop_front();
			continue;
		}

		if (ignored_types.has(type_cname)) {
			_log("Ignoring type '%s' because it's in the list of ignored types\n", String(type_cname).utf8().get_data());
			class_list.pop_front();
			continue;
		}

		if (!ClassDB::is_class_exposed(type_cname)) {
			_log("Ignoring type '%s' because it's not exposed\n", String(type_cname).utf8().get_data());
			class_list.pop_front();
			continue;
		}

		if (!ClassDB::is_class_enabled(type_cname)) {
			_log("Ignoring type '%s' because it's not enabled\n", String(type_cname).utf8().get_data());
			class_list.pop_front();
			continue;
		}

		ClassDB::ClassInfo *class_info = ClassDB::classes.getptr(type_cname);

		TypeInterface itype = TypeInterface::create_object_type(type_cname, pascal_to_pascal_case(type_cname), api_type);

		itype.base_name = ClassDB::get_parent_class(type_cname);
		itype.is_singleton = Engine::get_singleton()->has_singleton(type_cname);
		itype.is_instantiable = class_info->creation_func && !itype.is_singleton;
		itype.is_ref_counted = ClassDB::is_parent_class(type_cname, name_cache.type_RefCounted);
		itype.memory_own = itype.is_ref_counted;

		if (itype.class_doc) {
			itype.is_deprecated = itype.class_doc->is_deprecated;
			itype.deprecation_message = itype.class_doc->deprecated_message;

			if (itype.is_deprecated && itype.deprecation_message.is_empty()) {
				WARN_PRINT("An empty deprecation message is discouraged. Type: '" + itype.proxy_name + "'.");
				itype.deprecation_message = "This class is deprecated.";
			}
		}

		if (itype.is_singleton && compat_singletons.has(itype.cname)) {
			itype.is_singleton = false;
			itype.is_compat_singleton = true;
		}

		itype.c_out = "%5return ";
		itype.c_out += C_METHOD_UNMANAGED_GET_MANAGED;
		itype.c_out += itype.is_ref_counted ? "(%1.Reference);\n" : "(%1);\n";

		itype.cs_type = itype.proxy_name;

		itype.cs_in_expr = "GodotObject." CS_STATIC_METHOD_GETINSTANCE "(%0)";

		itype.cs_out = "%5return (%2)%0(%1);";

		itype.c_arg_in = "&%s";
		itype.c_type = "IntPtr";
		itype.c_type_in = itype.c_type;
		itype.c_type_out = "GodotObject";

		// Populate properties

		List<PropertyInfo> property_list;
		ClassDB::get_property_list(type_cname, &property_list, true);

		HashMap<StringName, StringName> accessor_methods;

		for (const PropertyInfo &property : property_list) {
			if (property.usage & PROPERTY_USAGE_GROUP || property.usage & PROPERTY_USAGE_SUBGROUP || property.usage & PROPERTY_USAGE_CATEGORY || (property.type == Variant::NIL && property.usage & PROPERTY_USAGE_ARRAY)) {
				continue;
			}

			if (property.name.contains_char('/')) {
				// Ignore properties with '/' (slash) in the name. These are only meant for use in the inspector.
				continue;
			}

			PropertyInterface iprop;
			iprop.cname = property.name;
			iprop.setter = ClassDB::get_property_setter(type_cname, iprop.cname);
			iprop.getter = ClassDB::get_property_getter(type_cname, iprop.cname);

			// If the property is internal hide it; otherwise, hide the getter and setter.
			if (property.usage & PROPERTY_USAGE_INTERNAL) {
				iprop.is_hidden = true;
			} else {
				if (iprop.setter != StringName()) {
					accessor_methods[iprop.setter] = iprop.cname;
				}
				if (iprop.getter != StringName()) {
					accessor_methods[iprop.getter] = iprop.cname;
				}
			}

			bool valid = false;
			iprop.index = ClassDB::get_property_index(type_cname, iprop.cname, &valid);
			ERR_FAIL_COND_V_MSG(!valid, false, "Invalid property: '" + itype.name + "." + String(iprop.cname) + "'.");

			iprop.proxy_name = escape_csharp_keyword(snake_to_pascal_case(iprop.cname));

			// Prevent the property and its enclosing type from sharing the same name
			if (iprop.proxy_name == itype.proxy_name) {
				_log("Name of property '%s' is ambiguous with the name of its enclosing class '%s'. Renaming property to '%s_'\n",
						iprop.proxy_name.utf8().get_data(), itype.proxy_name.utf8().get_data(), iprop.proxy_name.utf8().get_data());

				iprop.proxy_name += "_";
			}

			iprop.prop_doc = nullptr;

			for (int i = 0; i < itype.class_doc->properties.size(); i++) {
				const DocData::PropertyDoc &prop_doc = itype.class_doc->properties[i];

				if (prop_doc.name == iprop.cname) {
					iprop.prop_doc = &prop_doc;
					break;
				}
			}

			if (iprop.prop_doc) {
				iprop.is_deprecated = iprop.prop_doc->is_deprecated;
				iprop.deprecation_message = iprop.prop_doc->deprecated_message;

				if (iprop.is_deprecated && iprop.deprecation_message.is_empty()) {
					WARN_PRINT("An empty deprecation message is discouraged. Property: '" + itype.proxy_name + "." + iprop.proxy_name + "'.");
					iprop.deprecation_message = "This property is deprecated.";
				}
			}

			itype.properties.push_back(iprop);
		}

		// Populate methods

		List<MethodInfo> virtual_method_list;
		ClassDB::get_virtual_methods(type_cname, &virtual_method_list, true);

		List<Pair<MethodInfo, uint32_t>> method_list_with_hashes;
		ClassDB::get_method_list_with_compatibility(type_cname, &method_list_with_hashes, true);
		method_list_with_hashes.sort_custom_inplace<SortMethodWithHashes>();

		List<MethodInterface> compat_methods;
		for (const Pair<MethodInfo, uint32_t> &E : method_list_with_hashes) {
			const MethodInfo &method_info = E.first;
			const uint32_t hash = E.second;

			if (method_info.name.is_empty()) {
				continue;
			}

			String cname = method_info.name;

			if (blacklisted_methods.find(itype.cname) && blacklisted_methods[itype.cname].find(cname)) {
				continue;
			}

			if (method_has_ptr_parameter(method_info)) {
				// Pointers are not supported.
				itype.ignored_members.insert(method_info.name);
				continue;
			}

			MethodInterface imethod;
			imethod.name = method_info.name;
			imethod.cname = cname;
			imethod.hash = hash;

			if (method_info.flags & METHOD_FLAG_STATIC) {
				imethod.is_static = true;
			}

			if (method_info.flags & METHOD_FLAG_VIRTUAL) {
				imethod.is_virtual = true;
				itype.has_virtual_methods = true;
			}

			PropertyInfo return_info = method_info.return_val;

			MethodBind *m = nullptr;

			if (!imethod.is_virtual) {
				bool method_exists = false;
				m = ClassDB::get_method_with_compatibility(type_cname, method_info.name, hash, &method_exists, &imethod.is_compat);

				if (unlikely(!method_exists)) {
					ERR_FAIL_COND_V_MSG(!virtual_method_list.find(method_info), false,
							"Missing MethodBind for non-virtual method: '" + itype.name + "." + imethod.name + "'.");
				}
			}

			imethod.is_vararg = m && m->is_vararg();

			if (!m && !imethod.is_virtual) {
				ERR_FAIL_COND_V_MSG(!virtual_method_list.find(method_info), false,
						"Missing MethodBind for non-virtual method: '" + itype.name + "." + imethod.name + "'.");

				// A virtual method without the virtual flag. This is a special case.

				// There is no method bind, so let's fallback to Godot's object.Call(string, params)
				imethod.requires_object_call = true;

				// The method Object.free is registered as a virtual method, but without the virtual flag.
				// This is because this method is not supposed to be overridden, but called.
				// We assume the return type is void.
				imethod.return_type.cname = name_cache.type_void;

				// Actually, more methods like this may be added in the future, which could return
				// something different. Let's put this check to notify us if that ever happens.
				if (itype.cname != name_cache.type_Object || imethod.name != "free") {
					WARN_PRINT("Notification: New unexpected virtual non-overridable method found."
							   " We only expected Object.free, but found '" +
							itype.name + "." + imethod.name + "'.");
				}
			} else if (return_info.type == Variant::INT && return_info.usage & (PROPERTY_USAGE_CLASS_IS_ENUM | PROPERTY_USAGE_CLASS_IS_BITFIELD)) {
				imethod.return_type.cname = return_info.class_name;
				imethod.return_type.is_enum = true;
			} else if (return_info.class_name != StringName()) {
				imethod.return_type.cname = return_info.class_name;

				bool bad_reference_hint = !imethod.is_virtual && return_info.hint != PROPERTY_HINT_RESOURCE_TYPE &&
						ClassDB::is_parent_class(return_info.class_name, name_cache.type_RefCounted);
				ERR_FAIL_COND_V_MSG(bad_reference_hint, false,
						String() + "Return type is reference but hint is not '" _STR(PROPERTY_HINT_RESOURCE_TYPE) "'." +
								" Are you returning a reference type by pointer? Method: '" + itype.name + "." + imethod.name + "'.");
			} else if (return_info.type == Variant::ARRAY && return_info.hint == PROPERTY_HINT_ARRAY_TYPE) {
				imethod.return_type.cname = Variant::get_type_name(return_info.type) + "_@generic";
				imethod.return_type.generic_type_parameters.push_back(TypeReference(return_info.hint_string));
			} else if (return_info.type == Variant::DICTIONARY && return_info.hint == PROPERTY_HINT_DICTIONARY_TYPE) {
				imethod.return_type.cname = Variant::get_type_name(return_info.type) + "_@generic";
				Vector<String> split = return_info.hint_string.split(";");
				imethod.return_type.generic_type_parameters.push_back(TypeReference(split.get(0)));
				imethod.return_type.generic_type_parameters.push_back(TypeReference(split.get(1)));
			} else if (return_info.hint == PROPERTY_HINT_RESOURCE_TYPE) {
				imethod.return_type.cname = return_info.hint_string;
			} else if (return_info.type == Variant::NIL && return_info.usage & PROPERTY_USAGE_NIL_IS_VARIANT) {
				imethod.return_type.cname = name_cache.type_Variant;
			} else if (return_info.type == Variant::NIL) {
				imethod.return_type.cname = name_cache.type_void;
			} else {
				imethod.return_type.cname = _get_type_name_from_meta(return_info.type, m ? m->get_argument_meta(-1) : (GodotTypeInfo::Metadata)method_info.return_val_metadata);
			}

			int idx = 0;
			for (List<PropertyInfo>::ConstIterator itr = method_info.arguments.begin(); itr != method_info.arguments.end(); ++itr, ++idx) {
				const PropertyInfo &arginfo = *itr;

				String orig_arg_name = arginfo.name;

				ArgumentInterface iarg;
				iarg.name = orig_arg_name;

				if (arginfo.type == Variant::INT && arginfo.usage & (PROPERTY_USAGE_CLASS_IS_ENUM | PROPERTY_USAGE_CLASS_IS_BITFIELD)) {
					iarg.type.cname = arginfo.class_name;
					iarg.type.is_enum = true;
				} else if (arginfo.class_name != StringName()) {
					iarg.type.cname = arginfo.class_name;
				} else if (arginfo.type == Variant::ARRAY && arginfo.hint == PROPERTY_HINT_ARRAY_TYPE) {
					iarg.type.cname = Variant::get_type_name(arginfo.type) + "_@generic";
					iarg.type.generic_type_parameters.push_back(TypeReference(arginfo.hint_string));
				} else if (arginfo.type == Variant::DICTIONARY && arginfo.hint == PROPERTY_HINT_DICTIONARY_TYPE) {
					iarg.type.cname = Variant::get_type_name(arginfo.type) + "_@generic";
					Vector<String> split = arginfo.hint_string.split(";");
					iarg.type.generic_type_parameters.push_back(TypeReference(split.get(0)));
					iarg.type.generic_type_parameters.push_back(TypeReference(split.get(1)));
				} else if (arginfo.hint == PROPERTY_HINT_RESOURCE_TYPE) {
					iarg.type.cname = arginfo.hint_string;
				} else if (arginfo.type == Variant::NIL) {
					iarg.type.cname = name_cache.type_Variant;
				} else {
					iarg.type.cname = _get_type_name_from_meta(arginfo.type, m ? m->get_argument_meta(idx) : (GodotTypeInfo::Metadata)method_info.get_argument_meta(idx));
				}

				iarg.name = escape_csharp_keyword(snake_to_camel_case(iarg.name));

				if (m && m->has_default_argument(idx)) {
					bool defval_ok = _arg_default_value_from_variant(m->get_default_argument(idx), iarg);
					ERR_FAIL_COND_V_MSG(!defval_ok, false,
							"Cannot determine default value for argument '" + orig_arg_name + "' of method '" + itype.name + "." + imethod.name + "'.");
				}

				imethod.add_argument(iarg);
			}

			if (imethod.is_vararg) {
				ArgumentInterface ivararg;
				ivararg.type.cname = name_cache.type_VarArg;
				ivararg.name = "@args";
				imethod.add_argument(ivararg);
			}

			imethod.proxy_name = escape_csharp_keyword(snake_to_pascal_case(imethod.name));

			// Prevent the method and its enclosing type from sharing the same name
			if (imethod.proxy_name == itype.proxy_name) {
				_log("Name of method '%s' is ambiguous with the name of its enclosing class '%s'. Renaming method to '%s_'\n",
						imethod.proxy_name.utf8().get_data(), itype.proxy_name.utf8().get_data(), imethod.proxy_name.utf8().get_data());

				imethod.proxy_name += "_";
			}

			HashMap<StringName, StringName>::Iterator accessor = accessor_methods.find(imethod.cname);
			if (accessor) {
				// We only hide an accessor method if it's in the same class as the property.
				// It's easier this way, but also we don't know if an accessor method in a different class
				// could have other purposes, so better leave those untouched.
				imethod.is_hidden = true;
			}

			if (itype.class_doc) {
				for (int i = 0; i < itype.class_doc->methods.size(); i++) {
					if (itype.class_doc->methods[i].name == imethod.name) {
						imethod.method_doc = &itype.class_doc->methods[i];
						break;
					}
				}
			}

			if (imethod.method_doc) {
				imethod.is_deprecated = imethod.method_doc->is_deprecated;
				imethod.deprecation_message = imethod.method_doc->deprecated_message;

				if (imethod.is_deprecated && imethod.deprecation_message.is_empty()) {
					WARN_PRINT("An empty deprecation message is discouraged. Method: '" + itype.proxy_name + "." + imethod.proxy_name + "'.");
					imethod.deprecation_message = "This method is deprecated.";
				}
			}

			ERR_FAIL_COND_V_MSG(itype.find_property_by_name(imethod.cname), false,
					"Method name conflicts with property: '" + itype.name + "." + imethod.name + "'.");

			// Compat methods aren't added to the type yet, they need to be checked for conflicts
			// after all the non-compat methods have been added. The compat methods are added in
			// reverse so the most recently added ones take precedence over older compat methods.
			if (imethod.is_compat) {
				imethod.is_hidden = true;
				compat_methods.push_front(imethod);
				continue;
			}

			// Methods starting with an underscore are ignored unless they're used as a property setter or getter
			if (!imethod.is_virtual && imethod.name[0] == '_') {
				for (const PropertyInterface &iprop : itype.properties) {
					if (iprop.setter == imethod.name || iprop.getter == imethod.name) {
						imethod.is_internal = true;
						itype.methods.push_back(imethod);
						break;
					}
				}
			} else {
				itype.methods.push_back(imethod);
			}
		}

		// Add compat methods that don't conflict with other methods in the type.
		for (const MethodInterface &imethod : compat_methods) {
			if (_method_has_conflicting_signature(imethod, itype)) {
				WARN_PRINT("Method '" + imethod.name + "' conflicts with an already existing method in type '" + itype.name + "' and has been ignored.");
				continue;
			}
			itype.methods.push_back(imethod);
		}

		// Populate signals

		const HashMap<StringName, MethodInfo> &signal_map = class_info->signal_map;

		for (const KeyValue<StringName, MethodInfo> &E : signal_map) {
			SignalInterface isignal;

			const MethodInfo &method_info = E.value;

			isignal.name = method_info.name;
			isignal.cname = method_info.name;

			int idx = 0;
			for (List<PropertyInfo>::ConstIterator itr = method_info.arguments.begin(); itr != method_info.arguments.end(); ++itr, ++idx) {
				const PropertyInfo &arginfo = *itr;

				String orig_arg_name = arginfo.name;

				ArgumentInterface iarg;
				iarg.name = orig_arg_name;

				if (arginfo.type == Variant::INT && arginfo.usage & (PROPERTY_USAGE_CLASS_IS_ENUM | PROPERTY_USAGE_CLASS_IS_BITFIELD)) {
					iarg.type.cname = arginfo.class_name;
					iarg.type.is_enum = true;
				} else if (arginfo.class_name != StringName()) {
					iarg.type.cname = arginfo.class_name;
				} else if (arginfo.type == Variant::ARRAY && arginfo.hint == PROPERTY_HINT_ARRAY_TYPE) {
					iarg.type.cname = Variant::get_type_name(arginfo.type) + "_@generic";
					iarg.type.generic_type_parameters.push_back(TypeReference(arginfo.hint_string));
				} else if (arginfo.type == Variant::DICTIONARY && arginfo.hint == PROPERTY_HINT_DICTIONARY_TYPE) {
					iarg.type.cname = Variant::get_type_name(arginfo.type) + "_@generic";
					Vector<String> split = arginfo.hint_string.split(";");
					iarg.type.generic_type_parameters.push_back(TypeReference(split.get(0)));
					iarg.type.generic_type_parameters.push_back(TypeReference(split.get(1)));
				} else if (arginfo.hint == PROPERTY_HINT_RESOURCE_TYPE) {
					iarg.type.cname = arginfo.hint_string;
				} else if (arginfo.type == Variant::NIL) {
					iarg.type.cname = name_cache.type_Variant;
				} else {
					iarg.type.cname = _get_type_name_from_meta(arginfo.type, (GodotTypeInfo::Metadata)method_info.get_argument_meta(idx));
				}

				iarg.name = escape_csharp_keyword(snake_to_camel_case(iarg.name));

				isignal.add_argument(iarg);
			}

			isignal.proxy_name = escape_csharp_keyword(snake_to_pascal_case(isignal.name));

			// Prevent the signal and its enclosing type from sharing the same name
			if (isignal.proxy_name == itype.proxy_name) {
				_log("Name of signal '%s' is ambiguous with the name of its enclosing class '%s'. Renaming signal to '%s_'\n",
						isignal.proxy_name.utf8().get_data(), itype.proxy_name.utf8().get_data(), isignal.proxy_name.utf8().get_data());

				isignal.proxy_name += "_";
			}

			if (itype.find_property_by_proxy_name(isignal.proxy_name) || itype.find_method_by_proxy_name(isignal.proxy_name)) {
				// ClassDB allows signal names that conflict with method or property names.
				// While registering a signal with a conflicting name is considered wrong,
				// it may still happen and it may take some time until someone fixes the name.
				// We can't allow the bindings to be in a broken state while we wait for a fix;
				// that's why we must handle this possibility by renaming the signal.
				isignal.proxy_name += "Signal";
			}

			if (itype.class_doc) {
				for (int i = 0; i < itype.class_doc->signals.size(); i++) {
					const DocData::MethodDoc &signal_doc = itype.class_doc->signals[i];
					if (signal_doc.name == isignal.name) {
						isignal.method_doc = &signal_doc;
						break;
					}
				}
			}

			if (isignal.method_doc) {
				isignal.is_deprecated = isignal.method_doc->is_deprecated;
				isignal.deprecation_message = isignal.method_doc->deprecated_message;

				if (isignal.is_deprecated && isignal.deprecation_message.is_empty()) {
					WARN_PRINT("An empty deprecation message is discouraged. Signal: '" + itype.proxy_name + "." + isignal.proxy_name + "'.");
					isignal.deprecation_message = "This signal is deprecated.";
				}
			}

			itype.signals_.push_back(isignal);
		}

		// Populate enums and constants

		List<String> constants;
		ClassDB::get_integer_constant_list(type_cname, &constants, true);

		const HashMap<StringName, ClassDB::ClassInfo::EnumInfo> &enum_map = class_info->enum_map;

		for (const KeyValue<StringName, ClassDB::ClassInfo::EnumInfo> &E : enum_map) {
			StringName enum_proxy_cname = E.key;
			String enum_proxy_name = pascal_to_pascal_case(enum_proxy_cname.operator String());
			if (itype.find_property_by_proxy_name(enum_proxy_name) || itype.find_method_by_proxy_name(enum_proxy_name) || itype.find_signal_by_proxy_name(enum_proxy_name)) {
				// In case the enum name conflicts with other PascalCase members,
				// we append 'Enum' to the enum name in those cases.
				// We have several conflicts between enums and PascalCase properties.
				enum_proxy_name += "Enum";
				enum_proxy_cname = StringName(enum_proxy_name);
			}
			EnumInterface ienum(enum_proxy_cname, enum_proxy_name, E.value.is_bitfield);
			const List<StringName> &enum_constants = E.value.constants;
			for (const StringName &constant_cname : enum_constants) {
				String constant_name = constant_cname.operator String();
				int64_t *value = class_info->constant_map.getptr(constant_cname);
				ERR_FAIL_NULL_V(value, false);
				constants.erase(constant_name);

				ConstantInterface iconstant(constant_name, snake_to_pascal_case(constant_name, true), *value);

				iconstant.const_doc = nullptr;
				for (int i = 0; i < itype.class_doc->constants.size(); i++) {
					const DocData::ConstantDoc &const_doc = itype.class_doc->constants[i];

					if (const_doc.name == iconstant.name) {
						iconstant.const_doc = &const_doc;
						break;
					}
				}

				if (iconstant.const_doc) {
					iconstant.is_deprecated = iconstant.const_doc->is_deprecated;
					iconstant.deprecation_message = iconstant.const_doc->deprecated_message;

					if (iconstant.is_deprecated && iconstant.deprecation_message.is_empty()) {
						WARN_PRINT("An empty deprecation message is discouraged. Enum member: '" + itype.proxy_name + "." + ienum.proxy_name + "." + iconstant.proxy_name + "'.");
						iconstant.deprecation_message = "This enum member is deprecated.";
					}
				}

				ienum.constants.push_back(iconstant);
			}

			int prefix_length = _determine_enum_prefix(ienum);

			_apply_prefix_to_enum_constants(ienum, prefix_length);

			itype.enums.push_back(ienum);

			TypeInterface enum_itype;
			enum_itype.is_enum = true;
			enum_itype.name = itype.name + "." + String(E.key);
			enum_itype.cname = StringName(enum_itype.name);
			enum_itype.proxy_name = itype.proxy_name + "." + enum_proxy_name;
			TypeInterface::postsetup_enum_type(enum_itype);
			enum_types.insert(enum_itype.cname, enum_itype);
		}

		for (const String &constant_name : constants) {
			int64_t *value = class_info->constant_map.getptr(StringName(constant_name));
			ERR_FAIL_NULL_V(value, false);

			String constant_proxy_name = snake_to_pascal_case(constant_name, true);

			if (itype.find_property_by_proxy_name(constant_proxy_name) || itype.find_method_by_proxy_name(constant_proxy_name) || itype.find_signal_by_proxy_name(constant_proxy_name)) {
				// In case the constant name conflicts with other PascalCase members,
				// we append 'Constant' to the constant name in those cases.
				constant_proxy_name += "Constant";
			}

			ConstantInterface iconstant(constant_name, constant_proxy_name, *value);

			iconstant.const_doc = nullptr;
			for (int i = 0; i < itype.class_doc->constants.size(); i++) {
				const DocData::ConstantDoc &const_doc = itype.class_doc->constants[i];

				if (const_doc.name == iconstant.name) {
					iconstant.const_doc = &const_doc;
					break;
				}
			}

			if (iconstant.const_doc) {
				iconstant.is_deprecated = iconstant.const_doc->is_deprecated;
				iconstant.deprecation_message = iconstant.const_doc->deprecated_message;

				if (iconstant.is_deprecated && iconstant.deprecation_message.is_empty()) {
					WARN_PRINT("An empty deprecation message is discouraged. Constant: '" + itype.proxy_name + "." + iconstant.proxy_name + "'.");
					iconstant.deprecation_message = "This constant is deprecated.";
				}
			}

			itype.constants.push_back(iconstant);
		}

		obj_types.insert(itype.cname, itype);

		if (itype.is_singleton) {
			// Add singleton instance type.
			itype.proxy_name += CS_SINGLETON_INSTANCE_SUFFIX;
			itype.is_singleton = false;
			itype.is_singleton_instance = true;

			// Remove constants and enums, those will remain in the static class.
			itype.constants.clear();
			itype.enums.clear();

			obj_types.insert(itype.name + CS_SINGLETON_INSTANCE_SUFFIX, itype);
		}

		class_list.pop_front();
	}

	return true;
}

static String _get_vector2_cs_ctor_args(const Vector2 &p_vec2) {
	return String::num_real(p_vec2.x, true) + "f, " +
			String::num_real(p_vec2.y, true) + "f";
}

static String _get_vector3_cs_ctor_args(const Vector3 &p_vec3) {
	return String::num_real(p_vec3.x, true) + "f, " +
			String::num_real(p_vec3.y, true) + "f, " +
			String::num_real(p_vec3.z, true) + "f";
}

static String _get_vector4_cs_ctor_args(const Vector4 &p_vec4) {
	return String::num_real(p_vec4.x, true) + "f, " +
			String::num_real(p_vec4.y, true) + "f, " +
			String::num_real(p_vec4.z, true) + "f, " +
			String::num_real(p_vec4.w, true) + "f";
}

static String _get_vector2i_cs_ctor_args(const Vector2i &p_vec2i) {
	return itos(p_vec2i.x) + ", " + itos(p_vec2i.y);
}

static String _get_vector3i_cs_ctor_args(const Vector3i &p_vec3i) {
	return itos(p_vec3i.x) + ", " + itos(p_vec3i.y) + ", " + itos(p_vec3i.z);
}

static String _get_vector4i_cs_ctor_args(const Vector4i &p_vec4i) {
	return itos(p_vec4i.x) + ", " + itos(p_vec4i.y) + ", " + itos(p_vec4i.z) + ", " + itos(p_vec4i.w);
}

static String _get_color_cs_ctor_args(const Color &p_color) {
	return String::num(p_color.r, 4) + "f, " +
			String::num(p_color.g, 4) + "f, " +
			String::num(p_color.b, 4) + "f, " +
			String::num(p_color.a, 4) + "f";
}

bool BindingsGenerator::_arg_default_value_from_variant(const Variant &p_val, ArgumentInterface &r_iarg) {
	r_iarg.def_param_value = p_val;

	switch (p_val.get_type()) {
		case Variant::NIL:
			// Either Object type or Variant
			r_iarg.default_argument = "default";
			break;
		// Atomic types
		case Variant::BOOL:
			r_iarg.default_argument = bool(p_val) ? "true" : "false";
			break;
		case Variant::INT:
			if (r_iarg.type.cname != name_cache.type_int) {
				r_iarg.default_argument = "(%s)(" + p_val.operator String() + ")";
			} else {
				r_iarg.default_argument = p_val.operator String();
			}
			break;
		case Variant::FLOAT:
			r_iarg.default_argument = p_val.operator String();

			if (r_iarg.type.cname == name_cache.type_float) {
				r_iarg.default_argument += "f";
			}
			break;
		case Variant::STRING:
		case Variant::STRING_NAME:
		case Variant::NODE_PATH:
			if (r_iarg.type.cname == name_cache.type_StringName || r_iarg.type.cname == name_cache.type_NodePath) {
				if (r_iarg.default_argument.length() > 0) {
					r_iarg.default_argument = "(%s)\"" + p_val.operator String() + "\"";
					r_iarg.def_param_mode = ArgumentInterface::NULLABLE_REF;
				} else {
					// No need for a special `in` statement to change `null` to `""`. Marshaling takes care of this already.
					r_iarg.default_argument = "null";
				}
			} else {
				CRASH_COND(r_iarg.type.cname != name_cache.type_String);
				r_iarg.default_argument = "\"" + p_val.operator String() + "\"";
			}
			break;
		case Variant::PLANE: {
			Plane plane = p_val.operator Plane();
			r_iarg.default_argument = "new Plane(new Vector3(" +
					_get_vector3_cs_ctor_args(plane.normal) + "), " + rtos(plane.d) + "f)";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
		} break;
		case Variant::AABB: {
			AABB aabb = p_val.operator ::AABB();
			r_iarg.default_argument = "new Aabb(new Vector3(" +
					_get_vector3_cs_ctor_args(aabb.position) + "), new Vector3(" +
					_get_vector3_cs_ctor_args(aabb.size) + "))";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
		} break;
		case Variant::RECT2: {
			Rect2 rect = p_val.operator Rect2();
			r_iarg.default_argument = "new Rect2(new Vector2(" +
					_get_vector2_cs_ctor_args(rect.position) + "), new Vector2(" +
					_get_vector2_cs_ctor_args(rect.size) + "))";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
		} break;
		case Variant::RECT2I: {
			Rect2i rect = p_val.operator Rect2i();
			r_iarg.default_argument = "new Rect2I(new Vector2I(" +
					_get_vector2i_cs_ctor_args(rect.position) + "), new Vector2I(" +
					_get_vector2i_cs_ctor_args(rect.size) + "))";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
		} break;
		case Variant::COLOR:
			r_iarg.default_argument = "new Color(" + _get_color_cs_ctor_args(p_val.operator Color()) + ")";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
			break;
		case Variant::VECTOR2:
			r_iarg.default_argument = "new Vector2(" + _get_vector2_cs_ctor_args(p_val.operator Vector2()) + ")";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
			break;
		case Variant::VECTOR2I:
			r_iarg.default_argument = "new Vector2I(" + _get_vector2i_cs_ctor_args(p_val.operator Vector2i()) + ")";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
			break;
		case Variant::VECTOR3:
			r_iarg.default_argument = "new Vector3(" + _get_vector3_cs_ctor_args(p_val.operator Vector3()) + ")";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
			break;
		case Variant::VECTOR3I:
			r_iarg.default_argument = "new Vector3I(" + _get_vector3i_cs_ctor_args(p_val.operator Vector3i()) + ")";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
			break;
		case Variant::VECTOR4:
			r_iarg.default_argument = "new Vector4(" + _get_vector4_cs_ctor_args(p_val.operator Vector4()) + ")";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
			break;
		case Variant::VECTOR4I:
			r_iarg.default_argument = "new Vector4I(" + _get_vector4i_cs_ctor_args(p_val.operator Vector4i()) + ")";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
			break;
		case Variant::OBJECT:
			ERR_FAIL_COND_V_MSG(!p_val.is_zero(), false,
					"Parameter of type '" + String(r_iarg.type.cname) + "' can only have null/zero as the default value.");

			r_iarg.default_argument = "null";
			break;
		case Variant::DICTIONARY:
			ERR_FAIL_COND_V_MSG(!p_val.operator Dictionary().is_empty(), false,
					"Default value of type 'Dictionary' must be an empty dictionary.");
			// The [cs_in] expression already interprets null values as empty dictionaries.
			r_iarg.default_argument = "null";
			r_iarg.def_param_mode = ArgumentInterface::CONSTANT;
			break;
		case Variant::RID:
			ERR_FAIL_COND_V_MSG(r_iarg.type.cname != name_cache.type_RID, false,
					"Parameter of type '" + String(r_iarg.type.cname) + "' cannot have a default value of type '" + String(name_cache.type_RID) + "'.");

			ERR_FAIL_COND_V_MSG(!p_val.is_zero(), false,
					"Parameter of type '" + String(r_iarg.type.cname) + "' can only have null/zero as the default value.");

			r_iarg.default_argument = "default";
			break;
		case Variant::ARRAY:
			ERR_FAIL_COND_V_MSG(!p_val.operator Array().is_empty(), false,
					"Default value of type 'Array' must be an empty array.");
			// The [cs_in] expression already interprets null values as empty arrays.
			r_iarg.default_argument = "null";
			r_iarg.def_param_mode = ArgumentInterface::CONSTANT;
			break;
		case Variant::PACKED_BYTE_ARRAY:
		case Variant::PACKED_INT32_ARRAY:
		case Variant::PACKED_INT64_ARRAY:
		case Variant::PACKED_FLOAT32_ARRAY:
		case Variant::PACKED_FLOAT64_ARRAY:
		case Variant::PACKED_STRING_ARRAY:
		case Variant::PACKED_VECTOR2_ARRAY:
		case Variant::PACKED_VECTOR3_ARRAY:
		case Variant::PACKED_VECTOR4_ARRAY:
		case Variant::PACKED_COLOR_ARRAY:
			r_iarg.default_argument = "Array.Empty<%s>()";
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_REF;
			break;
		case Variant::TRANSFORM2D: {
			Transform2D transform = p_val.operator Transform2D();
			if (transform == Transform2D()) {
				r_iarg.default_argument = "Transform2D.Identity";
			} else {
				r_iarg.default_argument = "new Transform2D(new Vector2(" +
						_get_vector2_cs_ctor_args(transform.columns[0]) + "), new Vector2(" +
						_get_vector2_cs_ctor_args(transform.columns[1]) + "), new Vector2(" +
						_get_vector2_cs_ctor_args(transform.columns[2]) + "))";
			}
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
		} break;
		case Variant::TRANSFORM3D: {
			Transform3D transform = p_val.operator Transform3D();
			if (transform == Transform3D()) {
				r_iarg.default_argument = "Transform3D.Identity";
			} else {
				Basis basis = transform.basis;
				r_iarg.default_argument = "new Transform3D(new Vector3(" +
						_get_vector3_cs_ctor_args(basis.get_column(0)) + "), new Vector3(" +
						_get_vector3_cs_ctor_args(basis.get_column(1)) + "), new Vector3(" +
						_get_vector3_cs_ctor_args(basis.get_column(2)) + "), new Vector3(" +
						_get_vector3_cs_ctor_args(transform.origin) + "))";
			}
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
		} break;
		case Variant::PROJECTION: {
			Projection projection = p_val.operator Projection();
			if (projection == Projection()) {
				r_iarg.default_argument = "Projection.Identity";
			} else {
				r_iarg.default_argument = "new Projection(new Vector4(" +
						_get_vector4_cs_ctor_args(projection.columns[0]) + "), new Vector4(" +
						_get_vector4_cs_ctor_args(projection.columns[1]) + "), new Vector4(" +
						_get_vector4_cs_ctor_args(projection.columns[2]) + "), new Vector4(" +
						_get_vector4_cs_ctor_args(projection.columns[3]) + "))";
			}
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
		} break;
		case Variant::BASIS: {
			Basis basis = p_val.operator Basis();
			if (basis == Basis()) {
				r_iarg.default_argument = "Basis.Identity";
			} else {
				r_iarg.default_argument = "new Basis(new Vector3(" +
						_get_vector3_cs_ctor_args(basis.get_column(0)) + "), new Vector3(" +
						_get_vector3_cs_ctor_args(basis.get_column(1)) + "), new Vector3(" +
						_get_vector3_cs_ctor_args(basis.get_column(2)) + "))";
			}
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
		} break;
		case Variant::QUATERNION: {
			Quaternion quaternion = p_val.operator Quaternion();
			if (quaternion == Quaternion()) {
				r_iarg.default_argument = "Quaternion.Identity";
			} else {
				r_iarg.default_argument = "new Quaternion(" +
						String::num_real(quaternion.x, false) + "f, " +
						String::num_real(quaternion.y, false) + "f, " +
						String::num_real(quaternion.z, false) + "f, " +
						String::num_real(quaternion.w, false) + "f)";
			}
			r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
		} break;
		case Variant::CALLABLE:
			ERR_FAIL_COND_V_MSG(r_iarg.type.cname != name_cache.type_Callable, false,
					"Parameter of type '" + String(r_iarg.type.cname) + "' cannot have a default value of type '" + String(name_cache.type_Callable) + "'.");
			ERR_FAIL_COND_V_MSG(!p_val.is_zero(), false,
					"Parameter of type '" + String(r_iarg.type.cname) + "' can only have null/zero as the default value.");
			r_iarg.default_argument = "default";
			break;
		case Variant::SIGNAL:
			ERR_FAIL_COND_V_MSG(r_iarg.type.cname != name_cache.type_Signal, false,
					"Parameter of type '" + String(r_iarg.type.cname) + "' cannot have a default value of type '" + String(name_cache.type_Signal) + "'.");
			ERR_FAIL_COND_V_MSG(!p_val.is_zero(), false,
					"Parameter of type '" + String(r_iarg.type.cname) + "' can only have null/zero as the default value.");
			r_iarg.default_argument = "default";
			break;
		default:
			ERR_FAIL_V_MSG(false, "Unexpected Variant type: " + itos(p_val.get_type()));
			break;
	}

	if (r_iarg.def_param_mode == ArgumentInterface::CONSTANT && r_iarg.type.cname == name_cache.type_Variant && r_iarg.default_argument != "default") {
		r_iarg.def_param_mode = ArgumentInterface::NULLABLE_VAL;
	}

	return true;
}

void BindingsGenerator::_populate_builtin_type_interfaces() {
	builtin_types.clear();

	TypeInterface itype;

#define INSERT_STRUCT_TYPE(m_type, m_proxy_name)                                          \
	{                                                                                     \
		itype = TypeInterface::create_value_type(String(#m_type), String(#m_proxy_name)); \
		itype.cs_in_expr = "&%0";                                                         \
		itype.cs_in_expr_is_unsafe = true;                                                \
		builtin_types.insert(itype.cname, itype);                                         \
	}

	INSERT_STRUCT_TYPE(Vector2, Vector2)
	INSERT_STRUCT_TYPE(Vector2i, Vector2I)
	INSERT_STRUCT_TYPE(Rect2, Rect2)
	INSERT_STRUCT_TYPE(Rect2i, Rect2I)
	INSERT_STRUCT_TYPE(Transform2D, Transform2D)
	INSERT_STRUCT_TYPE(Vector3, Vector3)
	INSERT_STRUCT_TYPE(Vector3i, Vector3I)
	INSERT_STRUCT_TYPE(Basis, Basis)
	INSERT_STRUCT_TYPE(Quaternion, Quaternion)
	INSERT_STRUCT_TYPE(Transform3D, Transform3D)
	INSERT_STRUCT_TYPE(AABB, Aabb)
	INSERT_STRUCT_TYPE(Color, Color)
	INSERT_STRUCT_TYPE(Plane, Plane)
	INSERT_STRUCT_TYPE(Vector4, Vector4)
	INSERT_STRUCT_TYPE(Vector4i, Vector4I)
	INSERT_STRUCT_TYPE(Projection, Projection)

#undef INSERT_STRUCT_TYPE

	// bool
	itype = TypeInterface::create_value_type(String("bool"));
	itype.cs_in_expr = "%0.ToGodotBool()";
	itype.cs_out = "%5return %0(%1).ToBool();";
	itype.c_type = "godot_bool";
	itype.c_type_in = itype.c_type;
	itype.c_type_out = itype.c_type;
	itype.c_arg_in = "&%s";
	itype.c_in_vararg = "%5using godot_variant %1_in = VariantUtils.CreateFromBool(%1);\n";
	builtin_types.insert(itype.cname, itype);

	// Integer types
	{
		// C interface for 'uint32_t' is the same as that of enums. Remember to apply
		// any of the changes done here to 'TypeInterface::postsetup_enum_type' as well.
#define INSERT_INT_TYPE(m_name, m_int_struct_name)                                             \
	{                                                                                          \
		itype = TypeInterface::create_value_type(String(m_name));                              \
		if (itype.name != "long" && itype.name != "ulong") {                                   \
			itype.c_in = "%5%0 %1_in = %1;\n";                                                 \
			itype.c_out = "%5return (%0)(%1);\n";                                              \
			itype.c_type = "long";                                                             \
			itype.c_arg_in = "&%s_in";                                                         \
		} else {                                                                               \
			itype.c_arg_in = "&%s";                                                            \
		}                                                                                      \
		itype.c_type_in = itype.name;                                                          \
		itype.c_type_out = itype.name;                                                         \
		itype.c_in_vararg = "%5using godot_variant %1_in = VariantUtils.CreateFromInt(%1);\n"; \
		builtin_types.insert(itype.cname, itype);                                              \
	}

		// The expected type for all integers in ptrcall is 'int64_t', so that's what we use for 'c_type'

		INSERT_INT_TYPE("sbyte", "Int8");
		INSERT_INT_TYPE("short", "Int16");
		INSERT_INT_TYPE("int", "Int32");
		INSERT_INT_TYPE("long", "Int64");
		INSERT_INT_TYPE("byte", "UInt8");
		INSERT_INT_TYPE("ushort", "UInt16");
		INSERT_INT_TYPE("uint", "UInt32");
		INSERT_INT_TYPE("ulong", "UInt64");

#undef INSERT_INT_TYPE
	}

	// Floating point types
	{
		// float
		itype = TypeInterface();
		itype.name = "float";
		itype.cname = itype.name;
		itype.proxy_name = "float";
		itype.cs_type = itype.proxy_name;
		{
			// The expected type for 'float' in ptrcall is 'double'
			itype.c_in = "%5%0 %1_in = %1;\n";
			itype.c_out = "%5return (%0)%1;\n";
			itype.c_type = "double";
			itype.c_arg_in = "&%s_in";
		}
		itype.c_type_in = itype.proxy_name;
		itype.c_type_out = itype.proxy_name;
		itype.c_in_vararg = "%5using godot_variant %1_in = VariantUtils.CreateFromFloat(%1);\n";
		builtin_types.insert(itype.cname, itype);

		// double
		itype = TypeInterface();
		itype.name = "double";
		itype.cname = itype.name;
		itype.proxy_name = "double";
		itype.cs_type = itype.proxy_name;
		itype.c_type = "double";
		itype.c_arg_in = "&%s";
		itype.c_type_in = itype.proxy_name;
		itype.c_type_out = itype.proxy_name;
		itype.c_in_vararg = "%5using godot_variant %1_in = VariantUtils.CreateFromFloat(%1);\n";
		builtin_types.insert(itype.cname, itype);
	}

	// String
	itype = TypeInterface();
	itype.name = "String";
	itype.cname = itype.name;
	itype.proxy_name = "string";
	itype.cs_type = itype.proxy_name;
	itype.c_in = "%5using %0 %1_in = " C_METHOD_MONOSTR_TO_GODOT "(%1);\n";
	itype.c_out = "%5return " C_METHOD_MONOSTR_FROM_GODOT "(%1);\n";
	itype.c_arg_in = "&%s_in";
	itype.c_type = "godot_string";
	itype.c_type_in = itype.cs_type;
	itype.c_type_out = itype.cs_type;
	itype.c_type_is_disposable_struct = true;
	itype.c_in_vararg = "%5using godot_variant %1_in = VariantUtils.CreateFromString(%1);\n";
	builtin_types.insert(itype.cname, itype);

	// StringName
	itype = TypeInterface();
	itype.name = "StringName";
	itype.cname = itype.name;
	itype.proxy_name = "StringName";
	itype.cs_type = itype.proxy_name;
	itype.cs_in_expr = "(%1)(%0?.NativeValue ?? default)";
	// Cannot pass null StringName to ptrcall
	itype.c_out = "%5return %0.CreateTakingOwnershipOfDisposableValue(%1);\n";
	itype.c_arg_in = "&%s";
	itype.c_type = "godot_string_name";
	itype.c_type_in = itype.c_type;
	itype.c_type_out = itype.cs_type;
	itype.c_in_vararg = "%5using godot_variant %1_in = VariantUtils.CreateFromStringName(%1);\n";
	itype.c_type_is_disposable_struct = false; // [c_out] takes ownership
	itype.c_ret_needs_default_initialization = true;
	builtin_types.insert(itype.cname, itype);

	// NodePath
	itype = TypeInterface();
	itype.name = "NodePath";
	itype.cname = itype.name;
	itype.proxy_name = "NodePath";
	itype.cs_type = itype.proxy_name;
	itype.cs_in_expr = "(%1)(%0?.NativeValue ?? default)";
	// Cannot pass null NodePath to ptrcall
	itype.c_out = "%5return %0.CreateTakingOwnershipOfDisposableValue(%1);\n";
	itype.c_arg_in = "&%s";
	itype.c_type = "godot_node_path";
	itype.c_type_in = itype.c_type;
	itype.c_type_out = itype.cs_type;
	itype.c_type_is_disposable_struct = false; // [c_out] takes ownership
	itype.c_ret_needs_default_initialization = true;
	builtin_types.insert(itype.cname, itype);

	// RID
	itype = TypeInterface();
	itype.name = "RID";
	itype.cname = itype.name;
	itype.proxy_name = "Rid";
	itype.cs_type = itype.proxy_name;
	itype.c_arg_in = "&%s";
	itype.c_type = itype.cs_type;
	itype.c_type_in = itype.c_type;
	itype.c_type_out = itype.c_type;
	builtin_types.insert(itype.cname, itype);

	// Variant
	itype = TypeInterface();
	itype.name = "Variant";
	itype.cname = itype.name;
	itype.proxy_name = "Variant";
	itype.cs_type = itype.proxy_name;
	itype.c_in = "%5%0 %1_in = (%0)%1.NativeVar;\n";
	itype.c_out = "%5return Variant.CreateTakingOwnershipOfDisposableValue(%1);\n";
	itype.c_arg_in = "&%s_in";
	itype.c_type = "godot_variant";
	itype.c_type_in = itype.cs_type;
	itype.c_type_out = itype.cs_type;
	itype.c_type_is_disposable_struct = false; // [c_out] takes ownership
	itype.c_ret_needs_default_initialization = true;
	builtin_types.insert(itype.cname, itype);

	// Callable
	itype = TypeInterface::create_value_type(String("Callable"));
	itype.cs_in_expr = "%0";
	itype.c_in = "%5using %0 %1_in = " C_METHOD_MANAGED_TO_CALLABLE "(in %1);\n";
	itype.c_out = "%5return " C_METHOD_MANAGED_FROM_CALLABLE "(in %1);\n";
	itype.c_arg_in = "&%s_in";
	itype.c_type = "godot_callable";
	itype.c_type_in = "in " + itype.cs_type;
	itype.c_type_out = itype.cs_type;
	itype.c_type_is_disposable_struct = true;
	builtin_types.insert(itype.cname, itype);

	// Signal
	itype = TypeInterface();
	itype.name = "Signal";
	itype.cname = itype.name;
	itype.proxy_name = "Signal";
	itype.cs_type = itype.proxy_name;
	itype.cs_in_expr = "%0";
	itype.c_in = "%5using %0 %1_in = " C_METHOD_MANAGED_TO_SIGNAL "(in %1);\n";
	itype.c_out = "%5return " C_METHOD_MANAGED_FROM_SIGNAL "(in %1);\n";
	itype.c_arg_in = "&%s_in";
	itype.c_type = "godot_signal";
	itype.c_type_in = "in " + itype.cs_type;
	itype.c_type_out = itype.cs_type;
	itype.c_type_is_disposable_struct = true;
	builtin_types.insert(itype.cname, itype);

	// VarArg (fictitious type to represent variable arguments)
	itype = TypeInterface();
	itype.name = "VarArg";
	itype.cname = itype.name;
	itype.proxy_name = "ReadOnlySpan<Variant>";
	itype.cs_type = "params Variant[]";
	itype.cs_in_expr = "%0";
	// c_type, c_in and c_arg_in are hard-coded in the generator.
	// c_out and c_type_out are not applicable to VarArg.
	itype.c_arg_in = "&%s_in";
	itype.c_type_in = "ReadOnlySpan<Variant>";
	itype.is_span_compatible = true;
	builtin_types.insert(itype.cname, itype);

#define INSERT_ARRAY_FULL(m_name, m_type, m_managed_type, m_proxy_t)                \
	{                                                                               \
		itype = TypeInterface();                                                    \
		itype.name = #m_name;                                                       \
		itype.cname = itype.name;                                                   \
		itype.proxy_name = #m_proxy_t "[]";                                         \
		itype.cs_type = itype.proxy_name;                                           \
		itype.c_in = "%5using %0 %1_in = " C_METHOD_MONOARRAY_TO(m_type) "(%1);\n"; \
		itype.c_out = "%5return " C_METHOD_MONOARRAY_FROM(m_type) "(%1);\n";        \
		itype.c_arg_in = "&%s_in";                                                  \
		itype.c_type = #m_managed_type;                                             \
		itype.c_type_in = "ReadOnlySpan<" #m_proxy_t ">";                           \
		itype.c_type_out = itype.proxy_name;                                        \
		itype.c_type_is_disposable_struct = true;                                   \
		itype.is_span_compatible = true;                                            \
		builtin_types.insert(itype.name, itype);                                    \
	}

#define INSERT_ARRAY(m_type, m_managed_type, m_proxy_t) INSERT_ARRAY_FULL(m_type, m_type, m_managed_type, m_proxy_t)

	INSERT_ARRAY(PackedInt32Array, godot_packed_int32_array, int);
	INSERT_ARRAY(PackedInt64Array, godot_packed_int64_array, long);
	INSERT_ARRAY_FULL(PackedByteArray, PackedByteArray, godot_packed_byte_array, byte);

	INSERT_ARRAY(PackedFloat32Array, godot_packed_float32_array, float);
	INSERT_ARRAY(PackedFloat64Array, godot_packed_float64_array, double);

	INSERT_ARRAY(PackedStringArray, godot_packed_string_array, string);

	INSERT_ARRAY(PackedColorArray, godot_packed_color_array, Color);
	INSERT_ARRAY(PackedVector2Array, godot_packed_vector2_array, Vector2);
	INSERT_ARRAY(PackedVector3Array, godot_packed_vector3_array, Vector3);
	INSERT_ARRAY(PackedVector4Array, godot_packed_vector4_array, Vector4);

#undef INSERT_ARRAY

	// Array
	itype = TypeInterface();
	itype.name = "Array";
	itype.cname = itype.name;
	itype.proxy_name = itype.name;
	itype.type_parameter_count = 1;
	itype.cs_type = BINDINGS_NAMESPACE_COLLECTIONS "." + itype.proxy_name;
	itype.cs_in_expr = "(%1)(%0 ?? new()).NativeValue";
	itype.c_out = "%5return %0.CreateTakingOwnershipOfDisposableValue(%1);\n";
	itype.c_arg_in = "&%s";
	itype.c_type = "godot_array";
	itype.c_type_in = itype.c_type;
	itype.c_type_out = itype.cs_type;
	itype.c_type_is_disposable_struct = false; // [c_out] takes ownership
	itype.c_ret_needs_default_initialization = true;
	builtin_types.insert(itype.cname, itype);

	// Array_@generic
	// Reuse Array's itype
	itype.name = "Array_@generic";
	itype.cname = itype.name;
	itype.cs_out = "%5return new %2(%0(%1));";
	// For generic Godot collections, Variant.From<T>/As<T> is slower, so we need this special case
	itype.cs_variant_to_managed = "VariantUtils.ConvertToArray(%0)";
	itype.cs_managed_to_variant = "VariantUtils.CreateFromArray(%0)";
	builtin_types.insert(itype.cname, itype);

	// Dictionary
	itype = TypeInterface();
	itype.name = "Dictionary";
	itype.cname = itype.name;
	itype.proxy_name = itype.name;
	itype.type_parameter_count = 2;
	itype.cs_type = BINDINGS_NAMESPACE_COLLECTIONS "." + itype.proxy_name;
	itype.cs_in_expr = "(%1)(%0 ?? new()).NativeValue";
	itype.c_out = "%5return %0.CreateTakingOwnershipOfDisposableValue(%1);\n";
	itype.c_arg_in = "&%s";
	itype.c_type = "godot_dictionary";
	itype.c_type_in = itype.c_type;
	itype.c_type_out = itype.cs_type;
	itype.c_type_is_disposable_struct = false; // [c_out] takes ownership
	itype.c_ret_needs_default_initialization = true;
	builtin_types.insert(itype.cname, itype);

	// Dictionary_@generic
	// Reuse Dictionary's itype
	itype.name = "Dictionary_@generic";
	itype.cname = itype.name;
	itype.cs_out = "%5return new %2(%0(%1));";
	// For generic Godot collections, Variant.From<T>/As<T> is slower, so we need this special case
	itype.cs_variant_to_managed = "VariantUtils.ConvertToDictionary(%0)";
	itype.cs_managed_to_variant = "VariantUtils.CreateFromDictionary(%0)";
	builtin_types.insert(itype.cname, itype);

	// void (fictitious type to represent the return type of methods that do not return anything)
	itype = TypeInterface();
	itype.name = "void";
	itype.cname = itype.name;
	itype.proxy_name = itype.name;
	itype.cs_type = itype.proxy_name;
	itype.c_type = itype.proxy_name;
	itype.c_type_in = itype.c_type;
	itype.c_type_out = itype.c_type;
	builtin_types.insert(itype.cname, itype);
}

void BindingsGenerator::_populate_global_constants() {
	int global_constants_count = CoreConstants::get_global_constant_count();

	if (global_constants_count > 0) {
		HashMap<String, DocData::ClassDoc>::Iterator match = EditorHelp::get_doc_data()->class_list.find("@GlobalScope");

		CRASH_COND_MSG(!match, "Could not find '@GlobalScope' in DocData.");

		const DocData::ClassDoc &global_scope_doc = match->value;

		for (int i = 0; i < global_constants_count; i++) {
			String constant_name = CoreConstants::get_global_constant_name(i);

			const DocData::ConstantDoc *const_doc = nullptr;
			for (int j = 0; j < global_scope_doc.constants.size(); j++) {
				const DocData::ConstantDoc &curr_const_doc = global_scope_doc.constants[j];

				if (curr_const_doc.name == constant_name) {
					const_doc = &curr_const_doc;
					break;
				}
			}

			int64_t constant_value = CoreConstants::get_global_constant_value(i);
			StringName enum_name = CoreConstants::get_global_constant_enum(i);

			ConstantInterface iconstant(constant_name, snake_to_pascal_case(constant_name, true), constant_value);
			iconstant.const_doc = const_doc;

			if (enum_name != StringName()) {
				EnumInterface ienum(enum_name, pascal_to_pascal_case(enum_name.operator String()), CoreConstants::is_global_constant_bitfield(i));
				List<EnumInterface>::Element *enum_match = global_enums.find(ienum);
				if (enum_match) {
					enum_match->get().constants.push_back(iconstant);
				} else {
					ienum.constants.push_back(iconstant);
					global_enums.push_back(ienum);
				}
			} else {
				global_constants.push_back(iconstant);
			}
		}

		for (EnumInterface &ienum : global_enums) {
			TypeInterface enum_itype;
			enum_itype.is_enum = true;
			enum_itype.name = ienum.cname.operator String();
			enum_itype.cname = ienum.cname;
			enum_itype.proxy_name = ienum.proxy_name;
			TypeInterface::postsetup_enum_type(enum_itype);
			enum_types.insert(enum_itype.cname, enum_itype);

			int prefix_length = _determine_enum_prefix(ienum);

			// HARDCODED: The Error enum have the prefix 'ERR_' for everything except 'OK' and 'FAILED'.
			if (ienum.cname == name_cache.enum_Error) {
				if (prefix_length > 0) { // Just in case it ever changes
					ERR_PRINT("Prefix for enum '" _STR(Error) "' is not empty.");
				}

				prefix_length = 1; // 'ERR_'
			}

			_apply_prefix_to_enum_constants(ienum, prefix_length);
		}
	}

	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		if (i == Variant::OBJECT) {
			continue;
		}

		const Variant::Type type = Variant::Type(i);

		List<StringName> enum_names;
		Variant::get_enums_for_type(type, &enum_names);

		for (const StringName &enum_name : enum_names) {
			TypeInterface enum_itype;
			enum_itype.is_enum = true;
			enum_itype.name = Variant::get_type_name(type) + "." + enum_name;
			enum_itype.cname = enum_itype.name;
			enum_itype.proxy_name = pascal_to_pascal_case(enum_itype.name);
			TypeInterface::postsetup_enum_type(enum_itype);
			enum_types.insert(enum_itype.cname, enum_itype);
		}
	}
}

bool BindingsGenerator::_method_has_conflicting_signature(const MethodInterface &p_imethod, const TypeInterface &p_itype) {
	// Compare p_imethod with all the methods already registered in p_itype.
	for (const MethodInterface &method : p_itype.methods) {
		if (method.proxy_name == p_imethod.proxy_name) {
			if (_method_has_conflicting_signature(p_imethod, method)) {
				return true;
			}
		}
	}

	return false;
}

bool BindingsGenerator::_method_has_conflicting_signature(const MethodInterface &p_imethod_left, const MethodInterface &p_imethod_right) {
	// Check if a method already exists in p_itype with a method signature that would conflict with p_imethod.
	// The return type is ignored because only changing the return type is not enough to avoid conflicts.
	// The const keyword is also ignored since it doesn't generate different C# code.

	if (p_imethod_left.arguments.size() != p_imethod_right.arguments.size()) {
		// Different argument count, so no conflict.
		return false;
	}

	List<BindingsGenerator::ArgumentInterface>::ConstIterator left_itr = p_imethod_left.arguments.begin();
	List<BindingsGenerator::ArgumentInterface>::ConstIterator right_itr = p_imethod_right.arguments.begin();
	for (; left_itr != p_imethod_left.arguments.end(); ++left_itr, ++right_itr) {
		const ArgumentInterface &iarg_left = *left_itr;
		const ArgumentInterface &iarg_right = *right_itr;

		if (iarg_left.type.cname != iarg_right.type.cname) {
			// Different types for arguments in the same position, so no conflict.
			return false;
		}

		if (iarg_left.def_param_mode != iarg_right.def_param_mode) {
			// If the argument is a value type and nullable, it will be 'Nullable<T>' instead of 'T'
			// and will not create a conflict.
			if (iarg_left.def_param_mode == ArgumentInterface::NULLABLE_VAL || iarg_right.def_param_mode == ArgumentInterface::NULLABLE_VAL) {
				return false;
			}
		}
	}

	return true;
}

void BindingsGenerator::_initialize_blacklisted_methods() {
	blacklisted_methods["Object"].push_back("to_string"); // there is already ToString
	blacklisted_methods["Object"].push_back("_to_string"); // override ToString instead
	blacklisted_methods["Object"].push_back("_init"); // never called in C# (TODO: implement it)
}

void BindingsGenerator::_initialize_compat_singletons() {
	compat_singletons.insert("EditorInterface");
}

void BindingsGenerator::_log(const char *p_format, ...) {
	if (log_print_enabled) {
		va_list list;

		va_start(list, p_format);
		OS::get_singleton()->print("%s", str_format(p_format, list).utf8().get_data());
		va_end(list);
	}
}

void BindingsGenerator::_initialize() {
	initialized = false;

	EditorHelp::generate_doc(false);

	enum_types.clear();

	_initialize_blacklisted_methods();

	_initialize_compat_singletons();

	bool obj_type_ok = _populate_object_type_interfaces();
	ERR_FAIL_COND_MSG(!obj_type_ok, "Failed to generate object type interfaces");

	_populate_builtin_type_interfaces();

	_populate_global_constants();

	// Generate internal calls (after populating type interfaces and global constants)

	for (const KeyValue<StringName, TypeInterface> &E : obj_types) {
		const TypeInterface &itype = E.value;
		Error err = _populate_method_icalls_table(itype);
		ERR_FAIL_COND_MSG(err != OK, "Failed to generate icalls table for type: " + itype.name);
	}

	initialized = true;
}

static String generate_all_glue_option = "--generate-mono-glue";

static void handle_cmdline_options(String glue_dir_path) {
	BindingsGenerator bindings_generator;
	bindings_generator.set_log_print_enabled(true);

	if (!bindings_generator.is_initialized()) {
		ERR_PRINT("Failed to initialize the bindings generator");
		return;
	}

	CRASH_COND(glue_dir_path.is_empty());

	if (bindings_generator.generate_cs_api(glue_dir_path.path_join(API_SOLUTION_NAME)) != OK) {
		ERR_PRINT(generate_all_glue_option + ": Failed to generate the C# API.");
	}
}

static void cleanup_and_exit_godot() {
	// Exit once done.
	Main::cleanup(true);
	::exit(0);
}

void BindingsGenerator::handle_cmdline_args(const List<String> &p_cmdline_args) {
	String glue_dir_path;

	const List<String>::Element *elem = p_cmdline_args.front();

	while (elem) {
		if (elem->get() == generate_all_glue_option) {
			const List<String>::Element *path_elem = elem->next();

			if (path_elem) {
				glue_dir_path = path_elem->get();
				elem = elem->next();
			} else {
				ERR_PRINT(generate_all_glue_option + ": No output directory specified (expected path to '{GODOT_ROOT}/modules/mono/glue').");
				// Exit once done with invalid command line arguments.
				cleanup_and_exit_godot();
			}

			break;
		}

		elem = elem->next();
	}

	if (glue_dir_path.length()) {
		if (Engine::get_singleton()->is_editor_hint() ||
				Engine::get_singleton()->is_project_manager_hint()) {
			handle_cmdline_options(glue_dir_path);
		} else {
			// Running from a project folder, which doesn't make sense and crashes.
			ERR_PRINT(generate_all_glue_option + ": Cannot generate Mono glue while running a game project. Change current directory or enable --editor.");
		}
		// Exit once done.
		cleanup_and_exit_godot();
	}
}

#endif
