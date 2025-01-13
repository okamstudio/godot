/**************************************************************************/
/*  gdscript_analyzer.cpp                                                 */
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

#include "gdscript_analyzer.h"

#include "gdscript.h"
#include "gdscript_utility_callable.h"
#include "gdscript_utility_functions.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/core_constants.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/templates/hash_map.h"
#include "scene/resources/packed_scene.h"

#if defined(TOOLS_ENABLED) && !defined(DISABLE_DEPRECATED)
#define SUGGEST_GODOT4_RENAMES
#include "editor/renames_map_3_to_4.h"
#endif

#define UNNAMED_ENUM "<anonymous enum>"
#define ENUM_SEPARATOR "."

static MethodInfo info_from_utility_func(const StringName &p_function) {
	ERR_FAIL_COND_V(!Variant::has_utility_function(p_function), MethodInfo());

	MethodInfo info(p_function);

	if (Variant::has_utility_function_return_value(p_function)) {
		info.return_val.type = Variant::get_utility_function_return_type(p_function);
		if (info.return_val.type == Variant::NIL) {
			info.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
		}
	}

	if (Variant::is_utility_function_vararg(p_function)) {
		info.flags |= METHOD_FLAG_VARARG;
	} else {
		for (int i = 0; i < Variant::get_utility_function_argument_count(p_function); i++) {
			PropertyInfo pi;
#ifdef DEBUG_METHODS_ENABLED
			pi.name = Variant::get_utility_function_argument_name(p_function, i);
#else
			pi.name = "arg" + itos(i + 1);
#endif
			pi.type = Variant::get_utility_function_argument_type(p_function, i);
			info.arguments.push_back(pi);
		}
	}

	return info;
}

static GDScriptParser::DataType make_callable_type(const MethodInfo &p_info) {
	GDScriptParser::DataType type;
	type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	type.kind = GDScriptParser::DataType::BUILTIN;
	type.builtin_type = Variant::CALLABLE;
	type.is_constant = true;
	type.method_info = p_info;
	return type;
}

static GDScriptParser::DataType make_signal_type(const MethodInfo &p_info) {
	GDScriptParser::DataType type;
	type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	type.kind = GDScriptParser::DataType::BUILTIN;
	type.builtin_type = Variant::SIGNAL;
	type.is_constant = true;
	type.method_info = p_info;
	return type;
}

static GDScriptParser::DataType make_native_meta_type(const StringName &p_class_name) {
	GDScriptParser::DataType type;
	type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	type.kind = GDScriptParser::DataType::NATIVE;
	type.builtin_type = Variant::OBJECT;
	type.native_type = p_class_name;
	type.is_constant = true;
	type.is_meta_type = true;
	return type;
}

static GDScriptParser::DataType make_script_meta_type(const Ref<Script> &p_script) {
	GDScriptParser::DataType type;
	type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	type.kind = GDScriptParser::DataType::SCRIPT;
	type.builtin_type = Variant::OBJECT;
	type.native_type = p_script->get_instance_base_type();
	type.script_type = p_script;
	type.script_path = p_script->get_path();
	type.is_constant = true;
	type.is_meta_type = true;
	return type;
}

// In enum types, native_type is used to store the class (native or otherwise) that the enum belongs to.
// This disambiguates between similarly named enums in base classes or outer classes
static GDScriptParser::DataType make_enum_type(const StringName &p_enum_name, const String &p_base_name, const bool p_meta = false) {
	GDScriptParser::DataType type;
	type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	type.kind = GDScriptParser::DataType::ENUM;
	type.builtin_type = p_meta ? Variant::DICTIONARY : Variant::INT;
	type.enum_type = p_enum_name;
	type.is_constant = true;
	type.is_meta_type = p_meta;

	// For enums, native_type is only used to check compatibility in is_type_compatible()
	// We can set anything readable here for error messages, as long as it uniquely identifies the type of the enum
	if (p_base_name.is_empty()) {
		type.native_type = p_enum_name;
	} else {
		type.native_type = p_base_name + ENUM_SEPARATOR + p_enum_name;
	}

	return type;
}

static GDScriptParser::DataType make_class_enum_type(const StringName &p_enum_name, GDScriptParser::ClassNode *p_class, const String &p_script_path, bool p_meta = true) {
	GDScriptParser::DataType type = make_enum_type(p_enum_name, p_class->fqcn, p_meta);

	type.class_type = p_class;
	type.script_path = p_script_path;

	return type;
}

static GDScriptParser::DataType make_native_enum_type(const StringName &p_enum_name, const StringName &p_native_class, bool p_meta = true) {
	// Find out which base class declared the enum, so the name is always the same even when coming from other contexts.
	StringName native_base = p_native_class;
	while (true && native_base != StringName()) {
		if (ClassDB::has_enum(native_base, p_enum_name, true)) {
			break;
		}
		native_base = ClassDB::get_parent_class_nocheck(native_base);
	}

	GDScriptParser::DataType type = make_enum_type(p_enum_name, native_base, p_meta);
	if (p_meta) {
		// Native enum types are not dictionaries.
		type.builtin_type = Variant::NIL;
		type.is_pseudo_type = true;
	}

	List<StringName> enum_values;
	ClassDB::get_enum_constants(native_base, p_enum_name, &enum_values, true);

	for (const StringName &E : enum_values) {
		type.enum_values[E] = ClassDB::get_integer_constant(native_base, E);
	}

	return type;
}

static GDScriptParser::DataType make_builtin_enum_type(const StringName &p_enum_name, Variant::Type p_type, bool p_meta = true) {
	GDScriptParser::DataType type = make_enum_type(p_enum_name, Variant::get_type_name(p_type), p_meta);
	if (p_meta) {
		// Built-in enum types are not dictionaries.
		type.builtin_type = Variant::NIL;
		type.is_pseudo_type = true;
	}

	List<StringName> enum_values;
	Variant::get_enumerations_for_enum(p_type, p_enum_name, &enum_values);

	for (const StringName &E : enum_values) {
		type.enum_values[E] = Variant::get_enum_value(p_type, p_enum_name, E);
	}

	return type;
}

static GDScriptParser::DataType make_global_enum_type(const StringName &p_enum_name, const StringName &p_base, bool p_meta = true) {
	GDScriptParser::DataType type = make_enum_type(p_enum_name, p_base, p_meta);
	if (p_meta) {
		// Global enum types are not dictionaries.
		type.builtin_type = Variant::NIL;
		type.is_pseudo_type = true;
	}

	HashMap<StringName, int64_t> enum_values;
	CoreConstants::get_enum_values(type.native_type, &enum_values);
	for (const KeyValue<StringName, int64_t> &element : enum_values) {
		type.enum_values[element.key] = element.value;
	}

	return type;
}

static GDScriptParser::DataType make_builtin_meta_type(Variant::Type p_type) {
	GDScriptParser::DataType type;
	type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	type.kind = GDScriptParser::DataType::BUILTIN;
	type.builtin_type = p_type;
	type.is_constant = true;
	type.is_meta_type = true;
	return type;
}

bool GDScriptAnalyzer::has_member_name_conflict_in_script_class(const StringName &p_member_name, const GDScriptParser::ClassNode *p_class, const GDScriptParser::Node *p_member) {
	if (p_class->members_indices.has(p_member_name)) {
		int index = p_class->members_indices[p_member_name];
		const GDScriptParser::ClassNode::Member *member = &p_class->members[index];

		if (member->type == GDScriptParser::ClassNode::Member::VARIABLE ||
				member->type == GDScriptParser::ClassNode::Member::CONSTANT ||
				member->type == GDScriptParser::ClassNode::Member::ENUM ||
				member->type == GDScriptParser::ClassNode::Member::ENUM_VALUE ||
				member->type == GDScriptParser::ClassNode::Member::CLASS ||
				member->type == GDScriptParser::ClassNode::Member::SIGNAL) {
			return true;
		}
		if (p_member->type != GDScriptParser::Node::FUNCTION && member->type == GDScriptParser::ClassNode::Member::FUNCTION) {
			return true;
		}
	}

	return false;
}

bool GDScriptAnalyzer::has_member_name_conflict_in_native_type(const StringName &p_member_name, const StringName &p_native_type_string) {
	if (ClassDB::has_signal(p_native_type_string, p_member_name)) {
		return true;
	}
	if (ClassDB::has_property(p_native_type_string, p_member_name)) {
		return true;
	}
	if (ClassDB::has_integer_constant(p_native_type_string, p_member_name)) {
		return true;
	}
	if (p_member_name == CoreStringName(script)) {
		return true;
	}

	return false;
}

Error GDScriptAnalyzer::check_native_member_name_conflict(const StringName &p_member_name, const GDScriptParser::Node *p_member_node, const StringName &p_native_type_string) {
	if (has_member_name_conflict_in_native_type(p_member_name, p_native_type_string)) {
		push_error(vformat(R"(Member "%s" redefined (original in native class '%s'))", p_member_name, p_native_type_string), p_member_node);
		return ERR_PARSE_ERROR;
	}

	if (class_exists(p_member_name)) {
		push_error(vformat(R"(The member "%s" shadows a native class.)", p_member_name), p_member_node);
		return ERR_PARSE_ERROR;
	}

	if (GDScriptParser::get_builtin_type(p_member_name) < Variant::VARIANT_MAX) {
		push_error(vformat(R"(The member "%s" cannot have the same name as a builtin type.)", p_member_name), p_member_node);
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error GDScriptAnalyzer::check_class_member_name_conflict(const GDScriptParser::ClassNode *p_class_node, const StringName &p_member_name, const GDScriptParser::Node *p_member_node) {
	// TODO check outer classes for static members only
	const GDScriptParser::DataType *current_data_type = &p_class_node->base_type;
	while (current_data_type && current_data_type->kind == GDScriptParser::DataType::Kind::CLASS) {
		GDScriptParser::ClassNode *current_class_node = current_data_type->class_type;
		if (has_member_name_conflict_in_script_class(p_member_name, current_class_node, p_member_node)) {
			String parent_class_name = current_class_node->fqcn;
			if (current_class_node->identifier != nullptr) {
				parent_class_name = current_class_node->identifier->name;
			}
			push_error(vformat(R"(The member "%s" already exists in parent class %s.)", p_member_name, parent_class_name), p_member_node);
			return ERR_PARSE_ERROR;
		}
		current_data_type = &current_class_node->base_type;
	}

	// No need for native class recursion because Node exposes all Object's properties.
	if (current_data_type && current_data_type->kind == GDScriptParser::DataType::Kind::NATIVE) {
		if (current_data_type->native_type != StringName()) {
			return check_native_member_name_conflict(
					p_member_name,
					p_member_node,
					current_data_type->native_type);
		}
	}

	return OK;
}

void GDScriptAnalyzer::get_class_node_current_scope_classes(GDScriptParser::ClassNode *p_node, List<GDScriptParser::ClassNode *> *p_list, GDScriptParser::Node *p_source) {
	ERR_FAIL_NULL(p_node);
	ERR_FAIL_NULL(p_list);

	if (p_list->find(p_node) != nullptr) {
		return;
	}

	p_list->push_back(p_node);

	// TODO: Try to solve class inheritance if not yet resolving.

	// Prioritize node base type over its outer class
	if (p_node->base_type.class_type != nullptr) {
		// TODO: 'ensure_cached_external_parser_for_class()' is only necessary because 'resolve_class_inheritance()' is not getting called here.
		ensure_cached_external_parser_for_class(p_node->base_type.class_type, p_node, "Trying to fetch classes in the current scope", p_source);
		get_class_node_current_scope_classes(p_node->base_type.class_type, p_list, p_source);
	}

	if (p_node->outer != nullptr) {
		// TODO: 'ensure_cached_external_parser_for_class()' is only necessary because 'resolve_class_inheritance()' is not getting called here.
		ensure_cached_external_parser_for_class(p_node->outer, p_node, "Trying to fetch classes in the current scope", p_source);
		get_class_node_current_scope_classes(p_node->outer, p_list, p_source);
	}
}

Error GDScriptAnalyzer::resolve_class_inheritance(GDScriptParser::ClassNode *p_class, const GDScriptParser::Node *p_source) {
	if (p_source == nullptr && parser->has_class(p_class)) {
		p_source = p_class;
	}

	Ref<GDScriptParserRef> parser_ref = ensure_cached_external_parser_for_class(p_class, nullptr, "Trying to resolve class inheritance", p_source);
	Finally finally([&]() {
		for (GDScriptParser::ClassNode *look_class = p_class; look_class != nullptr; look_class = look_class->base_type.class_type) {
			ensure_cached_external_parser_for_class(look_class->base_type.class_type, look_class, "Trying to resolve class inheritance", p_source);
		}
	});

	if (p_class->base_type.is_resolving()) {
		push_error(vformat(R"(Could not resolve class "%s": Cyclic reference.)", type_from_metatype(p_class->get_datatype()).to_string()), p_source);
		return ERR_PARSE_ERROR;
	}

	if (!p_class->base_type.has_no_type()) {
		// Already resolved.
		return OK;
	}

	if (!parser->has_class(p_class)) {
		if (parser_ref.is_null()) {
			// Error already pushed.
			return ERR_PARSE_ERROR;
		}

		Error err = parser_ref->raise_status(GDScriptParserRef::PARSED);
		if (err) {
			push_error(vformat(R"(Could not parse script "%s": %s.)", p_class->get_datatype().script_path, error_names[err]), p_source);
			return ERR_PARSE_ERROR;
		}

		GDScriptAnalyzer *other_analyzer = parser_ref->get_analyzer();
		GDScriptParser *other_parser = parser_ref->get_parser();

		int error_count = other_parser->errors.size();
		other_analyzer->resolve_class_inheritance(p_class);
		if (other_parser->errors.size() > error_count) {
			push_error(vformat(R"(Could not resolve inheritance for class "%s".)", p_class->fqcn), p_source);
			return ERR_PARSE_ERROR;
		}

		return OK;
	}

	GDScriptParser::ClassNode *previous_class = parser->current_class;
	parser->current_class = p_class;

	if (p_class->identifier) {
		StringName class_name = p_class->identifier->name;
		if (GDScriptParser::get_builtin_type(class_name) < Variant::VARIANT_MAX) {
			push_error(vformat(R"(Class "%s" hides a built-in type.)", class_name), p_class->identifier);
		} else if (class_exists(class_name)) {
			push_error(vformat(R"(Class "%s" hides a native class.)", class_name), p_class->identifier);
		} else if (ScriptServer::is_global_class(class_name) && (!GDScript::is_canonically_equal_paths(ScriptServer::get_global_class_path(class_name), parser->script_path) || p_class != parser->head)) {
			push_error(vformat(R"(Class "%s" hides a global script class.)", class_name), p_class->identifier);
		} else if (ProjectSettings::get_singleton()->has_autoload(class_name) && ProjectSettings::get_singleton()->get_autoload(class_name).is_singleton) {
			push_error(vformat(R"(Class "%s" hides an autoload singleton.)", class_name), p_class->identifier);
		}
	}

	GDScriptParser::DataType resolving_datatype;
	resolving_datatype.kind = GDScriptParser::DataType::RESOLVING;
	p_class->base_type = resolving_datatype;

	// Set datatype for class.
	GDScriptParser::DataType class_type;
	class_type.is_constant = true;
	class_type.is_meta_type = true;
	class_type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	class_type.kind = GDScriptParser::DataType::CLASS;
	class_type.class_type = p_class;
	class_type.script_path = parser->script_path;
	class_type.builtin_type = Variant::OBJECT;
	p_class->set_datatype(class_type);

	GDScriptParser::DataType result;
	if (!p_class->extends_used) {
		result.type_source = GDScriptParser::DataType::ANNOTATED_INFERRED;
		result.kind = GDScriptParser::DataType::NATIVE;
		result.builtin_type = Variant::OBJECT;
		result.native_type = SNAME("RefCounted");
	} else {
		result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;

		GDScriptParser::DataType base;

		int extends_index = 0;

		if (!p_class->extends_path.is_empty()) {
			if (p_class->extends_path.is_relative_path()) {
				p_class->extends_path = class_type.script_path.get_base_dir().path_join(p_class->extends_path).simplify_path();
			}
			Ref<GDScriptParserRef> ext_parser = parser->get_depended_parser_for(p_class->extends_path);
			if (ext_parser.is_null()) {
				push_error(vformat(R"(Could not resolve super class path "%s".)", p_class->extends_path), p_class);
				return ERR_PARSE_ERROR;
			}

			Error err = ext_parser->raise_status(GDScriptParserRef::INHERITANCE_SOLVED);
			if (err != OK) {
				push_error(vformat(R"(Could not resolve super class inheritance from "%s".)", p_class->extends_path), p_class);
				return err;
			}

#ifdef DEBUG_ENABLED
			if (!parser->_is_tool && ext_parser->get_parser()->_is_tool) {
				parser->push_warning(p_class, GDScriptWarning::MISSING_TOOL);
			}
#endif

			base = ext_parser->get_parser()->head->get_datatype();
		} else {
			if (p_class->extends.is_empty()) {
				push_error("Could not resolve an empty super class path.", p_class);
				return ERR_PARSE_ERROR;
			}
			GDScriptParser::IdentifierNode *id = p_class->extends[extends_index++];
			const StringName &name = id->name;
			base.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;

			if (ScriptServer::is_global_class(name)) {
				String base_path = ScriptServer::get_global_class_path(name);

				if (GDScript::is_canonically_equal_paths(base_path, parser->script_path)) {
					base = parser->head->get_datatype();
				} else {
					Ref<GDScriptParserRef> base_parser = parser->get_depended_parser_for(base_path);
					if (base_parser.is_null()) {
						push_error(vformat(R"(Could not resolve super class "%s".)", name), id);
						return ERR_PARSE_ERROR;
					}

					Error err = base_parser->raise_status(GDScriptParserRef::INHERITANCE_SOLVED);
					if (err != OK) {
						push_error(vformat(R"(Could not resolve super class inheritance from "%s".)", name), id);
						return err;
					}

#ifdef DEBUG_ENABLED
					if (!parser->_is_tool && base_parser->get_parser()->_is_tool) {
						parser->push_warning(p_class, GDScriptWarning::MISSING_TOOL);
					}
#endif

					base = base_parser->get_parser()->head->get_datatype();
				}
			} else if (ProjectSettings::get_singleton()->has_autoload(name) && ProjectSettings::get_singleton()->get_autoload(name).is_singleton) {
				const ProjectSettings::AutoloadInfo &info = ProjectSettings::get_singleton()->get_autoload(name);
				if (info.path.get_extension().to_lower() != GDScriptLanguage::get_singleton()->get_extension()) {
					push_error(vformat(R"(Singleton %s is not a GDScript.)", info.name), id);
					return ERR_PARSE_ERROR;
				}

				Ref<GDScriptParserRef> info_parser = parser->get_depended_parser_for(info.path);
				if (info_parser.is_null()) {
					push_error(vformat(R"(Could not parse singleton from "%s".)", info.path), id);
					return ERR_PARSE_ERROR;
				}

				Error err = info_parser->raise_status(GDScriptParserRef::INHERITANCE_SOLVED);
				if (err != OK) {
					push_error(vformat(R"(Could not resolve super class inheritance from "%s".)", name), id);
					return err;
				}

#ifdef DEBUG_ENABLED
				if (!parser->_is_tool && info_parser->get_parser()->_is_tool) {
					parser->push_warning(p_class, GDScriptWarning::MISSING_TOOL);
				}
#endif

				base = info_parser->get_parser()->head->get_datatype();
			} else if (class_exists(name)) {
				if (Engine::get_singleton()->has_singleton(name)) {
					push_error(vformat(R"(Cannot inherit native class "%s" because it is an engine singleton.)", name), id);
					return ERR_PARSE_ERROR;
				}
				base.kind = GDScriptParser::DataType::NATIVE;
				base.builtin_type = Variant::OBJECT;
				base.native_type = name;
			} else {
				// Look for other classes in script.
				bool found = false;
				List<GDScriptParser::ClassNode *> script_classes;
				get_class_node_current_scope_classes(p_class, &script_classes, id);
				for (GDScriptParser::ClassNode *look_class : script_classes) {
					if (look_class->identifier && look_class->identifier->name == name) {
						if (!look_class->get_datatype().is_set()) {
							Error err = resolve_class_inheritance(look_class, id);
							if (err) {
								return err;
							}
						}
						base = look_class->get_datatype();
						found = true;
						break;
					}
					if (look_class->has_member(name)) {
						resolve_class_member(look_class, name, id);
						GDScriptParser::ClassNode::Member member = look_class->get_member(name);
						GDScriptParser::DataType member_datatype = member.get_datatype();

						switch (member.type) {
							case GDScriptParser::ClassNode::Member::CLASS:
								break; // OK.
							case GDScriptParser::ClassNode::Member::CONSTANT:
								if (member_datatype.kind != GDScriptParser::DataType::SCRIPT && member_datatype.kind != GDScriptParser::DataType::CLASS) {
									push_error(vformat(R"(Constant "%s" is not a preloaded script or class.)", name), id);
									return ERR_PARSE_ERROR;
								}
								break;
							default:
								push_error(vformat(R"(Cannot use %s "%s" in extends chain.)", member.get_type_name(), name), id);
								return ERR_PARSE_ERROR;
						}

						base = member_datatype;
						found = true;
						break;
					}
				}

				if (!found) {
					push_error(vformat(R"(Could not find base class "%s".)", name), id);
					return ERR_PARSE_ERROR;
				}
			}
		}

		for (int index = extends_index; index < p_class->extends.size(); index++) {
			GDScriptParser::IdentifierNode *id = p_class->extends[index];

			if (base.kind != GDScriptParser::DataType::CLASS) {
				push_error(vformat(R"(Cannot get nested types for extension from non-GDScript type "%s".)", base.to_string()), id);
				return ERR_PARSE_ERROR;
			}

			reduce_identifier_from_base(id, &base);
			GDScriptParser::DataType id_type = id->get_datatype();

			if (!id_type.is_set()) {
				push_error(vformat(R"(Could not find nested type "%s".)", id->name), id);
				return ERR_PARSE_ERROR;
			} else if (id_type.kind != GDScriptParser::DataType::SCRIPT && id_type.kind != GDScriptParser::DataType::CLASS) {
				push_error(vformat(R"(Identifier "%s" is not a preloaded script or class.)", id->name), id);
				return ERR_PARSE_ERROR;
			}

			base = id_type;
		}

		result = base;
	}

	if (!result.is_set() || result.has_no_type()) {
		// TODO: More specific error messages.
		push_error(vformat(R"(Could not resolve inheritance for class "%s".)", p_class->identifier == nullptr ? "<main>" : p_class->identifier->name), p_class);
		return ERR_PARSE_ERROR;
	}

	// Check for cyclic inheritance.
	const GDScriptParser::ClassNode *base_class = result.class_type;
	while (base_class) {
		if (base_class->fqcn == p_class->fqcn) {
			push_error("Cyclic inheritance.", p_class);
			return ERR_PARSE_ERROR;
		}
		base_class = base_class->base_type.class_type;
	}

	p_class->base_type = result;
	class_type.native_type = result.native_type;
	p_class->set_datatype(class_type);

	// Apply annotations.
	for (GDScriptParser::AnnotationNode *&E : p_class->annotations) {
		resolve_annotation(E);
		E->apply(parser, p_class, p_class->outer);
	}

	parser->current_class = previous_class;

	return OK;
}

Error GDScriptAnalyzer::resolve_class_inheritance(GDScriptParser::ClassNode *p_class, bool p_recursive) {
	Error err = resolve_class_inheritance(p_class);
	if (err) {
		return err;
	}

	if (p_recursive) {
		for (int i = 0; i < p_class->members.size(); i++) {
			if (p_class->members[i].type == GDScriptParser::ClassNode::Member::CLASS) {
				err = resolve_class_inheritance(p_class->members[i].m_class, true);
				if (err) {
					return err;
				}
			}
		}
	}

	return OK;
}

GDScriptParser::DataType GDScriptAnalyzer::resolve_datatype(GDScriptParser::TypeNode *p_type) {
	GDScriptParser::DataType bad_type;
	bad_type.kind = GDScriptParser::DataType::VARIANT;
	bad_type.type_source = GDScriptParser::DataType::INFERRED;

	if (p_type == nullptr) {
		return bad_type;
	}

	if (p_type->get_datatype().is_resolving()) {
		push_error(R"(Could not resolve datatype: Cyclic reference.)", p_type);
		return bad_type;
	}

	if (!p_type->get_datatype().has_no_type()) {
		return p_type->get_datatype();
	}

	GDScriptParser::DataType resolving_datatype;
	resolving_datatype.kind = GDScriptParser::DataType::RESOLVING;
	p_type->set_datatype(resolving_datatype);

	GDScriptParser::DataType result;
	result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;

	if (p_type->type_chain.is_empty()) {
		// void.
		result.kind = GDScriptParser::DataType::BUILTIN;
		result.builtin_type = Variant::NIL;
		p_type->set_datatype(result);
		return result;
	}

	const GDScriptParser::IdentifierNode *first_id = p_type->type_chain[0];
	StringName first = first_id->name;
	bool type_found = false;

	if (first_id->suite && first_id->suite->has_local(first)) {
		const GDScriptParser::SuiteNode::Local &local = first_id->suite->get_local(first);
		if (local.type == GDScriptParser::SuiteNode::Local::CONSTANT) {
			result = local.get_datatype();
			if (!result.is_set()) {
				// Don't try to resolve it as the constant can be declared below.
				push_error(vformat(R"(Local constant "%s" is not resolved at this point.)", first), first_id);
				return bad_type;
			}
			if (result.is_meta_type) {
				type_found = true;
			} else if (Ref<Script>(local.constant->initializer->reduced_value).is_valid()) {
				Ref<GDScript> gdscript = local.constant->initializer->reduced_value;
				if (gdscript.is_valid()) {
					Ref<GDScriptParserRef> ref = parser->get_depended_parser_for(gdscript->get_script_path());
					if (ref->raise_status(GDScriptParserRef::INHERITANCE_SOLVED) != OK) {
						push_error(vformat(R"(Could not parse script from "%s".)", gdscript->get_script_path()), first_id);
						return bad_type;
					}
					result = ref->get_parser()->head->get_datatype();
				} else {
					result = make_script_meta_type(local.constant->initializer->reduced_value);
				}
				type_found = true;
			} else {
				push_error(vformat(R"(Local constant "%s" is not a valid type.)", first), first_id);
				return bad_type;
			}
		} else {
			push_error(vformat(R"(Local %s "%s" cannot be used as a type.)", local.get_name(), first), first_id);
			return bad_type;
		}
	}

	if (!type_found) {
		if (first == SNAME("Variant")) {
			if (p_type->type_chain.size() == 2) {
				// May be nested enum.
				const StringName enum_name = p_type->type_chain[1]->name;
				const StringName qualified_name = String(first) + ENUM_SEPARATOR + String(p_type->type_chain[1]->name);
				if (CoreConstants::is_global_enum(qualified_name)) {
					result = make_global_enum_type(enum_name, first, true);
					return result;
				} else {
					push_error(vformat(R"(Name "%s" is not a nested type of "Variant".)", enum_name), p_type->type_chain[1]);
					return bad_type;
				}
			} else if (p_type->type_chain.size() > 2) {
				push_error(R"(Variant only contains enum types, which do not have nested types.)", p_type->type_chain[2]);
				return bad_type;
			}
			result.kind = GDScriptParser::DataType::VARIANT;
		} else if (GDScriptParser::get_builtin_type(first) < Variant::VARIANT_MAX) {
			// Built-in types.
			const Variant::Type builtin_type = GDScriptParser::get_builtin_type(first);

			if (p_type->type_chain.size() == 2) {
				// May be nested enum.
				const StringName enum_name = p_type->type_chain[1]->name;
				if (Variant::has_enum(builtin_type, enum_name)) {
					result = make_builtin_enum_type(enum_name, builtin_type, true);
					return result;
				} else {
					push_error(vformat(R"(Name "%s" is not a nested type of "%s".)", enum_name, first), p_type->type_chain[1]);
					return bad_type;
				}
			} else if (p_type->type_chain.size() > 2) {
				push_error(R"(Built-in types only contain enum types, which do not have nested types.)", p_type->type_chain[2]);
				return bad_type;
			}

			result.kind = GDScriptParser::DataType::BUILTIN;
			result.builtin_type = builtin_type;

			if (builtin_type == Variant::ARRAY) {
				GDScriptParser::DataType container_type = type_from_metatype(resolve_datatype(p_type->get_container_type_or_null(0)));
				if (container_type.kind != GDScriptParser::DataType::VARIANT) {
					container_type.is_constant = false;
					result.set_container_element_type(0, container_type);
				}
			}
			if (builtin_type == Variant::DICTIONARY) {
				GDScriptParser::DataType key_type = type_from_metatype(resolve_datatype(p_type->get_container_type_or_null(0)));
				if (key_type.kind != GDScriptParser::DataType::VARIANT) {
					key_type.is_constant = false;
					result.set_container_element_type(0, key_type);
				}
				GDScriptParser::DataType value_type = type_from_metatype(resolve_datatype(p_type->get_container_type_or_null(1)));
				if (value_type.kind != GDScriptParser::DataType::VARIANT) {
					value_type.is_constant = false;
					result.set_container_element_type(1, value_type);
				}
			}
		} else if (class_exists(first)) {
			// Native engine classes.
			result.kind = GDScriptParser::DataType::NATIVE;
			result.builtin_type = Variant::OBJECT;
			result.native_type = first;
		} else if (ScriptServer::is_global_class(first)) {
			if (GDScript::is_canonically_equal_paths(parser->script_path, ScriptServer::get_global_class_path(first))) {
				result = parser->head->get_datatype();
			} else {
				String path = ScriptServer::get_global_class_path(first);
				String ext = path.get_extension();
				if (ext == GDScriptLanguage::get_singleton()->get_extension()) {
					Ref<GDScriptParserRef> ref = parser->get_depended_parser_for(path);
					if (!ref.is_valid() || ref->raise_status(GDScriptParserRef::INHERITANCE_SOLVED) != OK) {
						push_error(vformat(R"(Could not parse global class "%s" from "%s".)", first, ScriptServer::get_global_class_path(first)), p_type);
						return bad_type;
					}
					result = ref->get_parser()->head->get_datatype();
				} else {
					result = make_script_meta_type(ResourceLoader::load(path, "Script"));
				}
			}
		} else if (ProjectSettings::get_singleton()->has_autoload(first) && ProjectSettings::get_singleton()->get_autoload(first).is_singleton) {
			const ProjectSettings::AutoloadInfo &autoload = ProjectSettings::get_singleton()->get_autoload(first);
			String script_path;
			if (ResourceLoader::get_resource_type(autoload.path) == "PackedScene") {
				// Try to get script from scene if possible.
				if (GDScriptLanguage::get_singleton()->has_any_global_constant(autoload.name)) {
					Variant constant = GDScriptLanguage::get_singleton()->get_any_global_constant(autoload.name);
					Node *node = Object::cast_to<Node>(constant);
					if (node != nullptr) {
						Ref<GDScript> scr = node->get_script();
						if (scr.is_valid()) {
							script_path = scr->get_script_path();
						}
					}
				}
			} else if (ResourceLoader::get_resource_type(autoload.path) == "GDScript") {
				script_path = autoload.path;
			}
			if (script_path.is_empty()) {
				return bad_type;
			}
			Ref<GDScriptParserRef> ref = parser->get_depended_parser_for(script_path);
			if (ref.is_null()) {
				push_error(vformat(R"(The referenced autoload "%s" (from "%s") could not be loaded.)", first, script_path), p_type);
				return bad_type;
			}
			if (ref->raise_status(GDScriptParserRef::INHERITANCE_SOLVED) != OK) {
				push_error(vformat(R"(Could not parse singleton "%s" from "%s".)", first, script_path), p_type);
				return bad_type;
			}
			result = ref->get_parser()->head->get_datatype();
		} else if (ClassDB::has_enum(parser->current_class->base_type.native_type, first)) {
			// Native enum in current class.
			result = make_native_enum_type(first, parser->current_class->base_type.native_type);
		} else if (CoreConstants::is_global_enum(first)) {
			if (p_type->type_chain.size() > 1) {
				push_error(R"(Enums cannot contain nested types.)", p_type->type_chain[1]);
				return bad_type;
			}
			result = make_global_enum_type(first, StringName());
		} else {
			// Classes in current scope.
			List<GDScriptParser::ClassNode *> script_classes;
			bool found = false;
			get_class_node_current_scope_classes(parser->current_class, &script_classes, p_type);
			for (GDScriptParser::ClassNode *script_class : script_classes) {
				if (found) {
					break;
				}

				if (script_class->identifier && script_class->identifier->name == first) {
					result = script_class->get_datatype();
					break;
				}
				if (script_class->members_indices.has(first)) {
					resolve_class_member(script_class, first, p_type);

					GDScriptParser::ClassNode::Member member = script_class->get_member(first);
					switch (member.type) {
						case GDScriptParser::ClassNode::Member::CLASS:
							result = member.get_datatype();
							found = true;
							break;
						case GDScriptParser::ClassNode::Member::ENUM:
							result = member.get_datatype();
							found = true;
							break;
						case GDScriptParser::ClassNode::Member::CONSTANT:
							if (member.get_datatype().is_meta_type) {
								result = member.get_datatype();
								found = true;
								break;
							} else if (Ref<Script>(member.constant->initializer->reduced_value).is_valid()) {
								Ref<GDScript> gdscript = member.constant->initializer->reduced_value;
								if (gdscript.is_valid()) {
									Ref<GDScriptParserRef> ref = parser->get_depended_parser_for(gdscript->get_script_path());
									if (ref->raise_status(GDScriptParserRef::INHERITANCE_SOLVED) != OK) {
										push_error(vformat(R"(Could not parse script from "%s".)", gdscript->get_script_path()), p_type);
										return bad_type;
									}
									result = ref->get_parser()->head->get_datatype();
								} else {
									result = make_script_meta_type(member.constant->initializer->reduced_value);
								}
								found = true;
								break;
							}
							[[fallthrough]];
						default:
							push_error(vformat(R"("%s" is a %s but does not contain a type.)", first, member.get_type_name()), p_type);
							return bad_type;
					}
				}
			}
		}
	}

	if (!result.is_set()) {
		push_error(vformat(R"(Could not find type "%s" in the current scope.)", first), p_type);
		return bad_type;
	}

	if (p_type->type_chain.size() > 1) {
		if (result.kind == GDScriptParser::DataType::CLASS) {
			for (int i = 1; i < p_type->type_chain.size(); i++) {
				GDScriptParser::DataType base = result;
				reduce_identifier_from_base(p_type->type_chain[i], &base);
				result = p_type->type_chain[i]->get_datatype();
				if (!result.is_set()) {
					push_error(vformat(R"(Could not find type "%s" under base "%s".)", p_type->type_chain[i]->name, base.to_string()), p_type->type_chain[1]);
					return bad_type;
				} else if (!result.is_meta_type) {
					push_error(vformat(R"(Member "%s" under base "%s" is not a valid type.)", p_type->type_chain[i]->name, base.to_string()), p_type->type_chain[1]);
					return bad_type;
				}
			}
		} else if (result.kind == GDScriptParser::DataType::NATIVE) {
			// Only enums allowed for native.
			if (ClassDB::has_enum(result.native_type, p_type->type_chain[1]->name)) {
				if (p_type->type_chain.size() > 2) {
					push_error(R"(Enums cannot contain nested types.)", p_type->type_chain[2]);
					return bad_type;
				} else {
					result = make_native_enum_type(p_type->type_chain[1]->name, result.native_type);
				}
			} else {
				push_error(vformat(R"(Could not find type "%s" in "%s".)", p_type->type_chain[1]->name, first), p_type->type_chain[1]);
				return bad_type;
			}
		} else {
			push_error(vformat(R"(Could not find nested type "%s" under base "%s".)", p_type->type_chain[1]->name, result.to_string()), p_type->type_chain[1]);
			return bad_type;
		}
	}

	if (!p_type->container_types.is_empty()) {
		if (result.builtin_type == Variant::ARRAY) {
			if (p_type->container_types.size() != 1) {
				push_error(R"(Typed arrays require exactly one collection element type.)", p_type);
				return bad_type;
			}
		} else if (result.builtin_type == Variant::DICTIONARY) {
			if (p_type->container_types.size() != 2) {
				push_error(R"(Typed dictionaries require exactly two collection element types.)", p_type);
				return bad_type;
			}
		} else {
			push_error(R"(Only arrays and dictionaries can specify collection element types.)", p_type);
			return bad_type;
		}
	}

	p_type->set_datatype(result);
	return result;
}

void GDScriptAnalyzer::resolve_class_member(GDScriptParser::ClassNode *p_class, const StringName &p_name, const GDScriptParser::Node *p_source) {
	ERR_FAIL_COND(!p_class->has_member(p_name));
	resolve_class_member(p_class, p_class->members_indices[p_name], p_source);
}

void GDScriptAnalyzer::resolve_class_member(GDScriptParser::ClassNode *p_class, int p_index, const GDScriptParser::Node *p_source) {
	ERR_FAIL_INDEX(p_index, p_class->members.size());

	GDScriptParser::ClassNode::Member &member = p_class->members.write[p_index];
	if (p_source == nullptr && parser->has_class(p_class)) {
		p_source = member.get_source_node();
	}

	Ref<GDScriptParserRef> parser_ref = ensure_cached_external_parser_for_class(p_class, nullptr, "Trying to resolve class member", p_source);
	Finally finally([&]() {
		ensure_cached_external_parser_for_class(member.get_datatype().class_type, p_class, "Trying to resolve datatype of class member", p_source);
		GDScriptParser::DataType member_type = member.get_datatype();
		for (int i = 0; i < member_type.get_container_element_type_count(); ++i) {
			ensure_cached_external_parser_for_class(member_type.get_container_element_type(i).class_type, p_class, "Trying to resolve datatype of class member", p_source);
		}
	});

	if (member.get_datatype().is_resolving()) {
		push_error(vformat(R"(Could not resolve member "%s": Cyclic reference.)", member.get_name()), p_source);
		return;
	}

	if (member.get_datatype().is_set()) {
		return;
	}

	// If it's already resolving, that's ok.
	if (!p_class->base_type.is_resolving()) {
		Error err = resolve_class_inheritance(p_class);
		if (err) {
			return;
		}
	}

	if (!parser->has_class(p_class)) {
		if (parser_ref.is_null()) {
			// Error already pushed.
			return;
		}

		Error err = parser_ref->raise_status(GDScriptParserRef::PARSED);
		if (err) {
			push_error(vformat(R"(Could not parse script "%s": %s (While resolving external class member "%s").)", p_class->get_datatype().script_path, error_names[err], member.get_name()), p_source);
			return;
		}

		GDScriptAnalyzer *other_analyzer = parser_ref->get_analyzer();
		GDScriptParser *other_parser = parser_ref->get_parser();

		int error_count = other_parser->errors.size();
		other_analyzer->resolve_class_member(p_class, p_index);
		if (other_parser->errors.size() > error_count) {
			push_error(vformat(R"(Could not resolve external class member "%s".)", member.get_name()), p_source);
			return;
		}

		return;
	}

	GDScriptParser::ClassNode *previous_class = parser->current_class;
	parser->current_class = p_class;

	GDScriptParser::DataType resolving_datatype;
	resolving_datatype.kind = GDScriptParser::DataType::RESOLVING;

	{
#ifdef DEBUG_ENABLED
		GDScriptParser::Node *member_node = member.get_source_node();
		if (member_node && member_node->type != GDScriptParser::Node::ANNOTATION) {
			// Apply @warning_ignore annotations before resolving member.
			for (GDScriptParser::AnnotationNode *&E : member_node->annotations) {
				if (E->name == SNAME("@warning_ignore")) {
					resolve_annotation(E);
					E->apply(parser, member.variable, p_class);
				}
			}
		}
#endif
		switch (member.type) {
			case GDScriptParser::ClassNode::Member::VARIABLE: {
				bool previous_static_context = static_context;
				static_context = member.variable->is_static;

				check_class_member_name_conflict(p_class, member.variable->identifier->name, member.variable);

				member.variable->set_datatype(resolving_datatype);
				resolve_variable(member.variable, false);
				resolve_pending_lambda_bodies();

				// Apply annotations.
				for (GDScriptParser::AnnotationNode *&E : member.variable->annotations) {
					if (E->name != SNAME("@warning_ignore")) {
						resolve_annotation(E);
						E->apply(parser, member.variable, p_class);
					}
				}

				static_context = previous_static_context;

#ifdef DEBUG_ENABLED
				if (member.variable->exported && member.variable->onready) {
					parser->push_warning(member.variable, GDScriptWarning::ONREADY_WITH_EXPORT);
				}
				if (member.variable->initializer) {
					// Check if it is call to get_node() on self (using shorthand $ or not), so we can check if @onready is needed.
					// This could be improved by traversing the expression fully and checking the presence of get_node at any level.
					if (!member.variable->is_static && !member.variable->onready && member.variable->initializer && (member.variable->initializer->type == GDScriptParser::Node::GET_NODE || member.variable->initializer->type == GDScriptParser::Node::CALL || member.variable->initializer->type == GDScriptParser::Node::CAST)) {
						GDScriptParser::Node *expr = member.variable->initializer;
						if (expr->type == GDScriptParser::Node::CAST) {
							expr = static_cast<GDScriptParser::CastNode *>(expr)->operand;
						}
						bool is_get_node = expr->type == GDScriptParser::Node::GET_NODE;
						bool is_using_shorthand = is_get_node;
						if (!is_get_node && expr->type == GDScriptParser::Node::CALL) {
							is_using_shorthand = false;
							GDScriptParser::CallNode *call = static_cast<GDScriptParser::CallNode *>(expr);
							if (call->function_name == SNAME("get_node")) {
								switch (call->get_callee_type()) {
									case GDScriptParser::Node::IDENTIFIER: {
										is_get_node = true;
									} break;
									case GDScriptParser::Node::SUBSCRIPT: {
										GDScriptParser::SubscriptNode *subscript = static_cast<GDScriptParser::SubscriptNode *>(call->callee);
										is_get_node = subscript->is_attribute && subscript->base->type == GDScriptParser::Node::SELF;
									} break;
									default:
										break;
								}
							}
						}
						if (is_get_node) {
							parser->push_warning(member.variable, GDScriptWarning::GET_NODE_DEFAULT_WITHOUT_ONREADY, is_using_shorthand ? "$" : "get_node()");
						}
					}
				}
#endif
			} break;
			case GDScriptParser::ClassNode::Member::CONSTANT: {
				check_class_member_name_conflict(p_class, member.constant->identifier->name, member.constant);
				member.constant->set_datatype(resolving_datatype);
				resolve_constant(member.constant, false);

				// Apply annotations.
				for (GDScriptParser::AnnotationNode *&E : member.constant->annotations) {
					resolve_annotation(E);
					E->apply(parser, member.constant, p_class);
				}
			} break;
			case GDScriptParser::ClassNode::Member::SIGNAL: {
				check_class_member_name_conflict(p_class, member.signal->identifier->name, member.signal);

				member.signal->set_datatype(resolving_datatype);

				// This is the _only_ way to declare a signal. Therefore, we can generate its
				// MethodInfo inline so it's a tiny bit more efficient.
				MethodInfo mi = MethodInfo(member.signal->identifier->name);

				for (int j = 0; j < member.signal->parameters.size(); j++) {
					GDScriptParser::ParameterNode *param = member.signal->parameters[j];
					GDScriptParser::DataType param_type = type_from_metatype(resolve_datatype(param->datatype_specifier));
					param->set_datatype(param_type);
#ifdef DEBUG_ENABLED
					if (param->datatype_specifier == nullptr) {
						parser->push_warning(param, GDScriptWarning::UNTYPED_DECLARATION, "Parameter", param->identifier->name);
					}
#endif
					mi.arguments.push_back(param_type.to_property_info(param->identifier->name));
					// Signals do not support parameter default values.
				}
				member.signal->set_datatype(make_signal_type(mi));
				member.signal->method_info = mi;

				// Apply annotations.
				for (GDScriptParser::AnnotationNode *&E : member.signal->annotations) {
					resolve_annotation(E);
					E->apply(parser, member.signal, p_class);
				}
			} break;
			case GDScriptParser::ClassNode::Member::ENUM: {
				check_class_member_name_conflict(p_class, member.m_enum->identifier->name, member.m_enum);

				member.m_enum->set_datatype(resolving_datatype);
				GDScriptParser::DataType enum_type = make_class_enum_type(member.m_enum->identifier->name, p_class, parser->script_path, true);

				const GDScriptParser::EnumNode *prev_enum = current_enum;
				current_enum = member.m_enum;

				Dictionary dictionary;
				for (int j = 0; j < member.m_enum->values.size(); j++) {
					GDScriptParser::EnumNode::Value &element = member.m_enum->values.write[j];

					if (element.custom_value) {
						reduce_expression(element.custom_value);
						if (!element.custom_value->is_constant) {
							push_error(R"(Enum values must be constant.)", element.custom_value);
						} else if (element.custom_value->reduced_value.get_type() != Variant::INT) {
							push_error(R"(Enum values must be integers.)", element.custom_value);
						} else {
							element.value = element.custom_value->reduced_value;
							element.resolved = true;
						}
					} else {
						if (element.index > 0) {
							element.value = element.parent_enum->values[element.index - 1].value + 1;
						} else {
							element.value = 0;
						}
						element.resolved = true;
					}

					enum_type.enum_values[element.identifier->name] = element.value;
					dictionary[String(element.identifier->name)] = element.value;

#ifdef DEBUG_ENABLED
					// Named enum identifiers do not shadow anything since you can only access them with `NamedEnum.ENUM_VALUE`.
					if (member.m_enum->identifier->name == StringName()) {
						is_shadowing(element.identifier, "enum member", false);
					}
#endif
				}

				current_enum = prev_enum;

				dictionary.make_read_only();
				member.m_enum->set_datatype(enum_type);
				member.m_enum->dictionary = dictionary;

				// Apply annotations.
				for (GDScriptParser::AnnotationNode *&E : member.m_enum->annotations) {
					resolve_annotation(E);
					E->apply(parser, member.m_enum, p_class);
				}
			} break;
			case GDScriptParser::ClassNode::Member::FUNCTION:
				for (GDScriptParser::AnnotationNode *&E : member.function->annotations) {
					resolve_annotation(E);
					E->apply(parser, member.function, p_class);
				}
				resolve_function_signature(member.function, p_source);
				break;
			case GDScriptParser::ClassNode::Member::ENUM_VALUE: {
				member.enum_value.identifier->set_datatype(resolving_datatype);

				if (member.enum_value.custom_value) {
					check_class_member_name_conflict(p_class, member.enum_value.identifier->name, member.enum_value.custom_value);

					const GDScriptParser::EnumNode *prev_enum = current_enum;
					current_enum = member.enum_value.parent_enum;
					reduce_expression(member.enum_value.custom_value);
					current_enum = prev_enum;

					if (!member.enum_value.custom_value->is_constant) {
						push_error(R"(Enum values must be constant.)", member.enum_value.custom_value);
					} else if (member.enum_value.custom_value->reduced_value.get_type() != Variant::INT) {
						push_error(R"(Enum values must be integers.)", member.enum_value.custom_value);
					} else {
						member.enum_value.value = member.enum_value.custom_value->reduced_value;
						member.enum_value.resolved = true;
					}
				} else {
					check_class_member_name_conflict(p_class, member.enum_value.identifier->name, member.enum_value.parent_enum);

					if (member.enum_value.index > 0) {
						const GDScriptParser::EnumNode::Value &prev_value = member.enum_value.parent_enum->values[member.enum_value.index - 1];
						resolve_class_member(p_class, prev_value.identifier->name, member.enum_value.identifier);
						member.enum_value.value = prev_value.value + 1;
					} else {
						member.enum_value.value = 0;
					}
					member.enum_value.resolved = true;
				}

				// Also update the original references.
				member.enum_value.parent_enum->values.set(member.enum_value.index, member.enum_value);

				member.enum_value.identifier->set_datatype(make_class_enum_type(UNNAMED_ENUM, p_class, parser->script_path, false));
			} break;
			case GDScriptParser::ClassNode::Member::CLASS:
				check_class_member_name_conflict(p_class, member.m_class->identifier->name, member.m_class);
				// If it's already resolving, that's ok.
				if (!member.m_class->base_type.is_resolving()) {
					resolve_class_inheritance(member.m_class, p_source);
				}
				break;
			case GDScriptParser::ClassNode::Member::GROUP:
				// No-op, but needed to silence warnings.
				break;
			case GDScriptParser::ClassNode::Member::UNDEFINED:
				ERR_PRINT("Trying to resolve undefined member.");
				break;
		}
	}

	parser->current_class = previous_class;
}

void GDScriptAnalyzer::resolve_class_interface(GDScriptParser::ClassNode *p_class, const GDScriptParser::Node *p_source) {
	if (p_source == nullptr && parser->has_class(p_class)) {
		p_source = p_class;
	}

	Ref<GDScriptParserRef> parser_ref = ensure_cached_external_parser_for_class(p_class, nullptr, "Trying to resolve class interface", p_source);

	if (!p_class->resolved_interface) {
#ifdef DEBUG_ENABLED
		bool has_static_data = p_class->has_static_data;
#endif

		if (!parser->has_class(p_class)) {
			if (parser_ref.is_null()) {
				// Error already pushed.
				return;
			}

			Error err = parser_ref->raise_status(GDScriptParserRef::PARSED);
			if (err) {
				push_error(vformat(R"(Could not parse script "%s": %s.)", p_class->get_datatype().script_path, error_names[err]), p_source);
				return;
			}

			GDScriptAnalyzer *other_analyzer = parser_ref->get_analyzer();
			GDScriptParser *other_parser = parser_ref->get_parser();

			int error_count = other_parser->errors.size();
			other_analyzer->resolve_class_interface(p_class);
			if (other_parser->errors.size() > error_count) {
				push_error(vformat(R"(Could not resolve class "%s".)", p_class->fqcn), p_source);
				return;
			}

			return;
		}

		p_class->resolved_interface = true;

		if (resolve_class_inheritance(p_class) != OK) {
			return;
		}

		GDScriptParser::DataType base_type = p_class->base_type;
		if (base_type.kind == GDScriptParser::DataType::CLASS) {
			GDScriptParser::ClassNode *base_class = base_type.class_type;
			resolve_class_interface(base_class, p_class);
		}

		for (int i = 0; i < p_class->members.size(); i++) {
			resolve_class_member(p_class, i);

#ifdef DEBUG_ENABLED
			if (!has_static_data) {
				GDScriptParser::ClassNode::Member member = p_class->members[i];
				if (member.type == GDScriptParser::ClassNode::Member::CLASS) {
					has_static_data = member.m_class->has_static_data;
				}
			}
#endif
		}

#ifdef DEBUG_ENABLED
		if (!has_static_data && p_class->annotated_static_unload) {
			GDScriptParser::Node *static_unload = nullptr;
			for (GDScriptParser::AnnotationNode *node : p_class->annotations) {
				if (node->name == "@static_unload") {
					static_unload = node;
					break;
				}
			}
			parser->push_warning(static_unload ? static_unload : p_class, GDScriptWarning::REDUNDANT_STATIC_UNLOAD);
		}
#endif
	}
}

void GDScriptAnalyzer::resolve_class_interface(GDScriptParser::ClassNode *p_class, bool p_recursive) {
	resolve_class_interface(p_class);

	if (p_recursive) {
		for (int i = 0; i < p_class->members.size(); i++) {
			GDScriptParser::ClassNode::Member member = p_class->members[i];
			if (member.type == GDScriptParser::ClassNode::Member::CLASS) {
				resolve_class_interface(member.m_class, true);
			}
		}
	}
}

void GDScriptAnalyzer::resolve_class_body(GDScriptParser::ClassNode *p_class, const GDScriptParser::Node *p_source) {
	if (p_source == nullptr && parser->has_class(p_class)) {
		p_source = p_class;
	}

	Ref<GDScriptParserRef> parser_ref = ensure_cached_external_parser_for_class(p_class, nullptr, "Trying to resolve class body", p_source);

	if (p_class->resolved_body) {
		return;
	}

	if (!parser->has_class(p_class)) {
		if (parser_ref.is_null()) {
			// Error already pushed.
			return;
		}

		Error err = parser_ref->raise_status(GDScriptParserRef::PARSED);
		if (err) {
			push_error(vformat(R"(Could not parse script "%s": %s.)", p_class->get_datatype().script_path, error_names[err]), p_source);
			return;
		}

		GDScriptAnalyzer *other_analyzer = parser_ref->get_analyzer();
		GDScriptParser *other_parser = parser_ref->get_parser();

		int error_count = other_parser->errors.size();
		other_analyzer->resolve_class_body(p_class);
		if (other_parser->errors.size() > error_count) {
			push_error(vformat(R"(Could not resolve class "%s".)", p_class->fqcn), p_source);
			return;
		}

		return;
	}

	p_class->resolved_body = true;

	GDScriptParser::ClassNode *previous_class = parser->current_class;
	parser->current_class = p_class;

	resolve_class_interface(p_class, p_source);

	GDScriptParser::DataType base_type = p_class->base_type;
	if (base_type.kind == GDScriptParser::DataType::CLASS) {
		GDScriptParser::ClassNode *base_class = base_type.class_type;
		resolve_class_body(base_class, p_class);
	}

	// Do functions, properties, and groups now.
	for (int i = 0; i < p_class->members.size(); i++) {
		GDScriptParser::ClassNode::Member member = p_class->members[i];
		if (member.type == GDScriptParser::ClassNode::Member::FUNCTION) {
			// Apply annotations.
			for (GDScriptParser::AnnotationNode *&E : member.function->annotations) {
				resolve_annotation(E);
				E->apply(parser, member.function, p_class);
			}
			resolve_function_body(member.function);
		} else if (member.type == GDScriptParser::ClassNode::Member::VARIABLE && member.variable->property != GDScriptParser::VariableNode::PROP_NONE) {
			if (member.variable->property == GDScriptParser::VariableNode::PROP_INLINE) {
				if (member.variable->getter != nullptr) {
					member.variable->getter->return_type = member.variable->datatype_specifier;
					member.variable->getter->set_datatype(member.get_datatype());

					resolve_function_body(member.variable->getter);
				}
				if (member.variable->setter != nullptr) {
					ERR_CONTINUE(member.variable->setter->parameters.is_empty());
					member.variable->setter->parameters[0]->datatype_specifier = member.variable->datatype_specifier;
					member.variable->setter->parameters[0]->set_datatype(member.get_datatype());

					resolve_function_body(member.variable->setter);
				}
			}
		} else if (member.type == GDScriptParser::ClassNode::Member::GROUP) {
			// Apply annotation (`@export_{category,group,subgroup}`).
			resolve_annotation(member.annotation);
			member.annotation->apply(parser, nullptr, p_class);
		}
	}

	// Check unused variables and datatypes of property getters and setters.
	for (int i = 0; i < p_class->members.size(); i++) {
		GDScriptParser::ClassNode::Member member = p_class->members[i];
		if (member.type == GDScriptParser::ClassNode::Member::VARIABLE) {
#ifdef DEBUG_ENABLED
			if (member.variable->usages == 0 && String(member.variable->identifier->name).begins_with("_")) {
				parser->push_warning(member.variable->identifier, GDScriptWarning::UNUSED_PRIVATE_CLASS_VARIABLE, member.variable->identifier->name);
			}
#endif

			if (member.variable->property == GDScriptParser::VariableNode::PROP_SETGET) {
				GDScriptParser::FunctionNode *getter_function = nullptr;
				GDScriptParser::FunctionNode *setter_function = nullptr;

				bool has_valid_getter = false;
				bool has_valid_setter = false;

				if (member.variable->getter_pointer != nullptr) {
					if (p_class->has_function(member.variable->getter_pointer->name)) {
						getter_function = p_class->get_member(member.variable->getter_pointer->name).function;
					}

					if (getter_function == nullptr) {
						push_error(vformat(R"(Getter "%s" not found.)", member.variable->getter_pointer->name), member.variable);
					} else {
						GDScriptParser::DataType return_datatype = getter_function->datatype;
						if (getter_function->return_type != nullptr) {
							return_datatype = getter_function->return_type->datatype;
							return_datatype.is_meta_type = false;
						}

						if (getter_function->parameters.size() != 0 || return_datatype.has_no_type()) {
							push_error(vformat(R"(Function "%s" cannot be used as getter because of its signature.)", getter_function->identifier->name), member.variable);
						} else if (!is_type_compatible(member.variable->datatype, return_datatype, true)) {
							push_error(vformat(R"(Function with return type "%s" cannot be used as getter for a property of type "%s".)", return_datatype.to_string(), member.variable->datatype.to_string()), member.variable);

						} else {
							has_valid_getter = true;
#ifdef DEBUG_ENABLED
							if (member.variable->datatype.builtin_type == Variant::INT && return_datatype.builtin_type == Variant::FLOAT) {
								parser->push_warning(member.variable, GDScriptWarning::NARROWING_CONVERSION);
							}
#endif
						}
					}
				}

				if (member.variable->setter_pointer != nullptr) {
					if (p_class->has_function(member.variable->setter_pointer->name)) {
						setter_function = p_class->get_member(member.variable->setter_pointer->name).function;
					}

					if (setter_function == nullptr) {
						push_error(vformat(R"(Setter "%s" not found.)", member.variable->setter_pointer->name), member.variable);

					} else if (setter_function->parameters.size() != 1) {
						push_error(vformat(R"(Function "%s" cannot be used as setter because of its signature.)", setter_function->identifier->name), member.variable);

					} else if (!is_type_compatible(member.variable->datatype, setter_function->parameters[0]->datatype, true)) {
						push_error(vformat(R"(Function with argument type "%s" cannot be used as setter for a property of type "%s".)", setter_function->parameters[0]->datatype.to_string(), member.variable->datatype.to_string()), member.variable);

					} else {
						has_valid_setter = true;

#ifdef DEBUG_ENABLED
						if (member.variable->datatype.builtin_type == Variant::FLOAT && setter_function->parameters[0]->datatype.builtin_type == Variant::INT) {
							parser->push_warning(member.variable, GDScriptWarning::NARROWING_CONVERSION);
						}
#endif
					}
				}

				if (member.variable->datatype.is_variant() && has_valid_getter && has_valid_setter) {
					if (!is_type_compatible(getter_function->datatype, setter_function->parameters[0]->datatype, true)) {
						push_error(vformat(R"(Getter with type "%s" cannot be used along with setter of type "%s".)", getter_function->datatype.to_string(), setter_function->parameters[0]->datatype.to_string()), member.variable);
					}
				}
			}
		} else if (member.type == GDScriptParser::ClassNode::Member::SIGNAL) {
#ifdef DEBUG_ENABLED
			if (member.signal->usages == 0) {
				parser->push_warning(member.signal->identifier, GDScriptWarning::UNUSED_SIGNAL, member.signal->identifier->name);
			}
#endif
		}
	}

	if (!pending_body_resolution_lambdas.is_empty()) {
		ERR_PRINT("GDScript bug (please report): Not all pending lambda bodies were resolved in time.");
		resolve_pending_lambda_bodies();
	}

	parser->current_class = previous_class;
}

void GDScriptAnalyzer::resolve_class_body(GDScriptParser::ClassNode *p_class, bool p_recursive) {
	resolve_class_body(p_class);

	if (p_recursive) {
		for (int i = 0; i < p_class->members.size(); i++) {
			GDScriptParser::ClassNode::Member member = p_class->members[i];
			if (member.type == GDScriptParser::ClassNode::Member::CLASS) {
				resolve_class_body(member.m_class, true);
			}
		}
	}
}

void GDScriptAnalyzer::resolve_node(GDScriptParser::Node *p_node, bool p_is_root) {
	ERR_FAIL_NULL_MSG(p_node, "Trying to resolve type of a null node.");

	switch (p_node->type) {
		case GDScriptParser::Node::NONE:
			break; // Unreachable.
		case GDScriptParser::Node::CLASS:
			// NOTE: Currently this route is never executed, `resolve_class_*()` is called directly.
			if (OK == resolve_class_inheritance(static_cast<GDScriptParser::ClassNode *>(p_node), true)) {
				resolve_class_interface(static_cast<GDScriptParser::ClassNode *>(p_node), true);
				resolve_class_body(static_cast<GDScriptParser::ClassNode *>(p_node), true);
			}
			break;
		case GDScriptParser::Node::CONSTANT:
			resolve_constant(static_cast<GDScriptParser::ConstantNode *>(p_node), true);
			break;
		case GDScriptParser::Node::FOR:
			resolve_for(static_cast<GDScriptParser::ForNode *>(p_node));
			break;
		case GDScriptParser::Node::IF:
			resolve_if(static_cast<GDScriptParser::IfNode *>(p_node));
			break;
		case GDScriptParser::Node::SUITE:
			resolve_suite(static_cast<GDScriptParser::SuiteNode *>(p_node));
			break;
		case GDScriptParser::Node::VARIABLE:
			resolve_variable(static_cast<GDScriptParser::VariableNode *>(p_node), true);
			break;
		case GDScriptParser::Node::WHILE:
			resolve_while(static_cast<GDScriptParser::WhileNode *>(p_node));
			break;
		case GDScriptParser::Node::ANNOTATION:
			resolve_annotation(static_cast<GDScriptParser::AnnotationNode *>(p_node));
			break;
		case GDScriptParser::Node::ASSERT:
			resolve_assert(static_cast<GDScriptParser::AssertNode *>(p_node));
			break;
		case GDScriptParser::Node::MATCH:
			resolve_match(static_cast<GDScriptParser::MatchNode *>(p_node));
			break;
		case GDScriptParser::Node::MATCH_BRANCH:
			resolve_match_branch(static_cast<GDScriptParser::MatchBranchNode *>(p_node), nullptr);
			break;
		case GDScriptParser::Node::PARAMETER:
			resolve_parameter(static_cast<GDScriptParser::ParameterNode *>(p_node));
			break;
		case GDScriptParser::Node::PATTERN:
			resolve_match_pattern(static_cast<GDScriptParser::PatternNode *>(p_node), nullptr);
			break;
		case GDScriptParser::Node::RETURN:
			resolve_return(static_cast<GDScriptParser::ReturnNode *>(p_node));
			break;
		case GDScriptParser::Node::TYPE:
			resolve_datatype(static_cast<GDScriptParser::TypeNode *>(p_node));
			break;
		// Resolving expression is the same as reducing them.
		case GDScriptParser::Node::ARRAY:
		case GDScriptParser::Node::ASSIGNMENT:
		case GDScriptParser::Node::AWAIT:
		case GDScriptParser::Node::BINARY_OPERATOR:
		case GDScriptParser::Node::CALL:
		case GDScriptParser::Node::CAST:
		case GDScriptParser::Node::DICTIONARY:
		case GDScriptParser::Node::GET_NODE:
		case GDScriptParser::Node::IDENTIFIER:
		case GDScriptParser::Node::LAMBDA:
		case GDScriptParser::Node::LITERAL:
		case GDScriptParser::Node::PRELOAD:
		case GDScriptParser::Node::SELF:
		case GDScriptParser::Node::SUBSCRIPT:
		case GDScriptParser::Node::TERNARY_OPERATOR:
		case GDScriptParser::Node::TYPE_TEST:
		case GDScriptParser::Node::UNARY_OPERATOR:
			reduce_expression(static_cast<GDScriptParser::ExpressionNode *>(p_node), p_is_root);
			break;
		case GDScriptParser::Node::BREAK:
		case GDScriptParser::Node::BREAKPOINT:
		case GDScriptParser::Node::CONTINUE:
		case GDScriptParser::Node::ENUM:
		case GDScriptParser::Node::FUNCTION:
		case GDScriptParser::Node::PASS:
		case GDScriptParser::Node::SIGNAL:
			// Nothing to do.
			break;
	}
}

void GDScriptAnalyzer::resolve_annotation(GDScriptParser::AnnotationNode *p_annotation) {
	ERR_FAIL_COND_MSG(!parser->valid_annotations.has(p_annotation->name), vformat(R"(Annotation "%s" not found to validate.)", p_annotation->name));

	if (p_annotation->is_resolved) {
		return;
	}
	p_annotation->is_resolved = true;

	const MethodInfo &annotation_info = parser->valid_annotations[p_annotation->name].info;

	const List<PropertyInfo>::Element *E = annotation_info.arguments.front();
	for (int i = 0; i < p_annotation->arguments.size(); i++) {
		GDScriptParser::ExpressionNode *argument = p_annotation->arguments[i];
		const PropertyInfo &argument_info = E->get();

		if (E->next() != nullptr) {
			E = E->next();
		}

		reduce_expression(argument);

		if (!argument->is_constant) {
			push_error(vformat(R"(Argument %d of annotation "%s" isn't a constant expression.)", i + 1, p_annotation->name), argument);
			return;
		}

		Variant value = argument->reduced_value;

		if (value.get_type() != argument_info.type) {
#ifdef DEBUG_ENABLED
			if (argument_info.type == Variant::INT && value.get_type() == Variant::FLOAT) {
				parser->push_warning(argument, GDScriptWarning::NARROWING_CONVERSION);
			}
#endif

			if (!Variant::can_convert_strict(value.get_type(), argument_info.type)) {
				push_error(vformat(R"(Invalid argument for annotation "%s": argument %d should be "%s" but is "%s".)", p_annotation->name, i + 1, Variant::get_type_name(argument_info.type), argument->get_datatype().to_string()), argument);
				return;
			}

			Variant converted_to;
			const Variant *converted_from = &value;
			Callable::CallError call_error;
			Variant::construct(argument_info.type, converted_to, &converted_from, 1, call_error);

			if (call_error.error != Callable::CallError::CALL_OK) {
				push_error(vformat(R"(Cannot convert argument %d of annotation "%s" from "%s" to "%s".)", i + 1, p_annotation->name, Variant::get_type_name(value.get_type()), Variant::get_type_name(argument_info.type)), argument);
				return;
			}

			value = converted_to;
		}

		p_annotation->resolved_arguments.push_back(value);
	}
}

void GDScriptAnalyzer::resolve_function_signature(GDScriptParser::FunctionNode *p_function, const GDScriptParser::Node *p_source, bool p_is_lambda) {
	if (p_source == nullptr) {
		p_source = p_function;
	}

	StringName function_name = p_function->identifier != nullptr ? p_function->identifier->name : StringName();

	if (p_function->get_datatype().is_resolving()) {
		push_error(vformat(R"(Could not resolve function "%s": Cyclic reference.)", function_name), p_source);
		return;
	}

	if (p_function->resolved_signature) {
		return;
	}
	p_function->resolved_signature = true;

	GDScriptParser::FunctionNode *previous_function = parser->current_function;
	parser->current_function = p_function;
	bool previous_static_context = static_context;
	if (p_is_lambda) {
		// For lambdas this is determined from the context, the `static` keyword is not allowed.
		p_function->is_static = static_context;
	} else {
		// For normal functions, this is determined in the parser by the `static` keyword.
		static_context = p_function->is_static;
	}

	GDScriptParser::DataType prev_datatype = p_function->get_datatype();

	GDScriptParser::DataType resolving_datatype;
	resolving_datatype.kind = GDScriptParser::DataType::RESOLVING;
	p_function->set_datatype(resolving_datatype);

#ifdef TOOLS_ENABLED
	int default_value_count = 0;
#endif // TOOLS_ENABLED

#ifdef DEBUG_ENABLED
	String function_visible_name = function_name;
	if (function_name == StringName()) {
		function_visible_name = p_is_lambda ? "<anonymous lambda>" : "<unknown function>";
	}
#endif

	for (int i = 0; i < p_function->parameters.size(); i++) {
		resolve_parameter(p_function->parameters[i]);
#ifdef DEBUG_ENABLED
		if (p_function->parameters[i]->usages == 0 && !String(p_function->parameters[i]->identifier->name).begins_with("_")) {
			parser->push_warning(p_function->parameters[i]->identifier, GDScriptWarning::UNUSED_PARAMETER, function_visible_name, p_function->parameters[i]->identifier->name);
		}
		is_shadowing(p_function->parameters[i]->identifier, "function parameter", true);
#endif // DEBUG_ENABLED

		if (p_function->parameters[i]->initializer) {
#ifdef TOOLS_ENABLED
			default_value_count++;
#endif // TOOLS_ENABLED

			if (p_function->parameters[i]->initializer->is_constant) {
				p_function->default_arg_values.push_back(p_function->parameters[i]->initializer->reduced_value);
			} else {
				p_function->default_arg_values.push_back(Variant()); // Prevent shift.
			}
		}
	}

	if (!p_is_lambda && function_name == GDScriptLanguage::get_singleton()->strings._init) {
		// Constructor.
		GDScriptParser::DataType return_type = parser->current_class->get_datatype();
		return_type.is_meta_type = false;
		p_function->set_datatype(return_type);
		if (p_function->return_type) {
			GDScriptParser::DataType declared_return = resolve_datatype(p_function->return_type);
			if (declared_return.kind != GDScriptParser::DataType::BUILTIN || declared_return.builtin_type != Variant::NIL) {
				push_error("Constructor cannot have an explicit return type.", p_function->return_type);
			}
		}
	} else if (!p_is_lambda && function_name == GDScriptLanguage::get_singleton()->strings._static_init) {
		// Static constructor.
		GDScriptParser::DataType return_type;
		return_type.kind = GDScriptParser::DataType::BUILTIN;
		return_type.builtin_type = Variant::NIL;
		p_function->set_datatype(return_type);
		if (p_function->return_type) {
			GDScriptParser::DataType declared_return = resolve_datatype(p_function->return_type);
			if (declared_return.kind != GDScriptParser::DataType::BUILTIN || declared_return.builtin_type != Variant::NIL) {
				push_error("Static constructor cannot have an explicit return type.", p_function->return_type);
			}
		}
	} else {
		if (p_function->return_type != nullptr) {
			p_function->set_datatype(type_from_metatype(resolve_datatype(p_function->return_type)));
		} else {
			// In case the function is not typed, we can safely assume it's a Variant, so it's okay to mark as "inferred" here.
			// It's not "undetected" to not mix up with unknown functions.
			GDScriptParser::DataType return_type;
			return_type.type_source = GDScriptParser::DataType::INFERRED;
			return_type.kind = GDScriptParser::DataType::VARIANT;
			p_function->set_datatype(return_type);
		}

#ifdef TOOLS_ENABLED
		// Check if the function signature matches the parent. If not it's an error since it breaks polymorphism.
		// Not for the constructor which can vary in signature.
		GDScriptParser::DataType base_type = parser->current_class->base_type;
		base_type.is_meta_type = false;
		GDScriptParser::DataType parent_return_type;
		List<GDScriptParser::DataType> parameters_types;
		int default_par_count = 0;
		BitField<MethodFlags> method_flags;
		StringName native_base;
		if (!p_is_lambda && get_function_signature(p_function, false, base_type, function_name, parent_return_type, parameters_types, default_par_count, method_flags, &native_base)) {
			bool valid = p_function->is_static == method_flags.has_flag(METHOD_FLAG_STATIC);

			if (p_function->return_type != nullptr) {
				// Check return type covariance.
				GDScriptParser::DataType return_type = p_function->get_datatype();
				if (return_type.is_variant()) {
					// `is_type_compatible()` returns `true` if one of the types is `Variant`.
					// Don't allow an explicitly specified `Variant` if the parent return type is narrower.
					valid = valid && parent_return_type.is_variant();
				} else if (return_type.kind == GDScriptParser::DataType::BUILTIN && return_type.builtin_type == Variant::NIL) {
					// `is_type_compatible()` returns `true` if target is an `Object` and source is `null`.
					// Don't allow `void` if the parent return type is a hard non-`void` type.
					if (parent_return_type.is_hard_type() && !(parent_return_type.kind == GDScriptParser::DataType::BUILTIN && parent_return_type.builtin_type == Variant::NIL)) {
						valid = false;
					}
				} else {
					valid = valid && is_type_compatible(parent_return_type, return_type);
				}
			}

			int par_count_diff = p_function->parameters.size() - parameters_types.size();
			valid = valid && par_count_diff >= 0;
			valid = valid && default_value_count >= default_par_count + par_count_diff;

			if (valid) {
				int i = 0;
				for (const GDScriptParser::DataType &parent_par_type : parameters_types) {
					// Check parameter type contravariance.
					GDScriptParser::DataType current_par_type = p_function->parameters[i++]->get_datatype();
					if (parent_par_type.is_variant() && parent_par_type.is_hard_type()) {
						// `is_type_compatible()` returns `true` if one of the types is `Variant`.
						// Don't allow narrowing a hard `Variant`.
						valid = valid && current_par_type.is_variant();
					} else {
						valid = valid && is_type_compatible(current_par_type, parent_par_type);
					}
				}
			}

			if (!valid) {
				// Compute parent signature as a string to show in the error message.
				String parent_signature = String(function_name) + "(";
				int j = 0;
				for (const GDScriptParser::DataType &par_type : parameters_types) {
					if (j > 0) {
						parent_signature += ", ";
					}
					String parameter = par_type.to_string();
					if (parameter == "null") {
						parameter = "Variant";
					}
					parent_signature += parameter;
					if (j >= parameters_types.size() - default_par_count) {
						parent_signature += " = <default>";
					}

					j++;
				}
				parent_signature += ") -> ";

				const String return_type = parent_return_type.to_string_strict();
				if (return_type == "null") {
					parent_signature += "void";
				} else {
					parent_signature += return_type;
				}

				push_error(vformat(R"(The function signature doesn't match the parent. Parent signature is "%s".)", parent_signature), p_function);
			}
#ifdef DEBUG_ENABLED
			if (native_base != StringName()) {
				parser->push_warning(p_function, GDScriptWarning::NATIVE_METHOD_OVERRIDE, function_name, native_base);
			}
#endif
		}
#endif // TOOLS_ENABLED
	}

#ifdef DEBUG_ENABLED
	if (p_function->return_type == nullptr) {
		parser->push_warning(p_function, GDScriptWarning::UNTYPED_DECLARATION, "Function", function_visible_name);
	}
#endif

	if (p_function->get_datatype().is_resolving()) {
		p_function->set_datatype(prev_datatype);
	}

	parser->current_function = previous_function;
	static_context = previous_static_context;
}

void GDScriptAnalyzer::resolve_function_body(GDScriptParser::FunctionNode *p_function, bool p_is_lambda) {
	if (p_function->resolved_body) {
		return;
	}
	p_function->resolved_body = true;

	GDScriptParser::FunctionNode *previous_function = parser->current_function;
	parser->current_function = p_function;

	bool previous_static_context = static_context;
	static_context = p_function->is_static;

	resolve_suite(p_function->body);

	if (!p_function->get_datatype().is_hard_type() && p_function->body->get_datatype().is_set()) {
		// Use the suite inferred type if return isn't explicitly set.
		p_function->set_datatype(p_function->body->get_datatype());
	} else if (p_function->get_datatype().is_hard_type() && (p_function->get_datatype().kind != GDScriptParser::DataType::BUILTIN || p_function->get_datatype().builtin_type != Variant::NIL)) {
		if (!p_function->body->has_return && (p_is_lambda || p_function->identifier->name != GDScriptLanguage::get_singleton()->strings._init)) {
			push_error(R"(Not all code paths return a value.)", p_function);
		}
	}

	parser->current_function = previous_function;
	static_context = previous_static_context;
}

void GDScriptAnalyzer::decide_suite_type(GDScriptParser::Node *p_suite, GDScriptParser::Node *p_statement) {
	if (p_statement == nullptr) {
		return;
	}
	switch (p_statement->type) {
		case GDScriptParser::Node::IF:
		case GDScriptParser::Node::FOR:
		case GDScriptParser::Node::MATCH:
		case GDScriptParser::Node::PATTERN:
		case GDScriptParser::Node::RETURN:
		case GDScriptParser::Node::WHILE:
			// Use return or nested suite type as this suite type.
			if (p_suite->get_datatype().is_set() && (p_suite->get_datatype() != p_statement->get_datatype())) {
				// Mixed types.
				// TODO: This could use the common supertype instead.
				p_suite->datatype.kind = GDScriptParser::DataType::VARIANT;
				p_suite->datatype.type_source = GDScriptParser::DataType::UNDETECTED;
			} else {
				p_suite->set_datatype(p_statement->get_datatype());
				p_suite->datatype.type_source = GDScriptParser::DataType::INFERRED;
			}
			break;
		default:
			break;
	}
}

void GDScriptAnalyzer::resolve_suite(GDScriptParser::SuiteNode *p_suite) {
	for (int i = 0; i < p_suite->statements.size(); i++) {
		GDScriptParser::Node *stmt = p_suite->statements[i];
		// Apply annotations.
		for (GDScriptParser::AnnotationNode *&E : stmt->annotations) {
			resolve_annotation(E);
			E->apply(parser, stmt, nullptr); // TODO: Provide `p_class`.
		}

		resolve_node(stmt);
		resolve_pending_lambda_bodies();
		decide_suite_type(p_suite, stmt);
	}
}

void GDScriptAnalyzer::resolve_assignable(GDScriptParser::AssignableNode *p_assignable, const char *p_kind) {
	GDScriptParser::DataType type;
	type.kind = GDScriptParser::DataType::VARIANT;

	bool is_constant = p_assignable->type == GDScriptParser::Node::CONSTANT;

#ifdef DEBUG_ENABLED
	if (p_assignable->identifier != nullptr && p_assignable->identifier->suite != nullptr && p_assignable->identifier->suite->parent_block != nullptr) {
		if (p_assignable->identifier->suite->parent_block->has_local(p_assignable->identifier->name)) {
			const GDScriptParser::SuiteNode::Local &local = p_assignable->identifier->suite->parent_block->get_local(p_assignable->identifier->name);
			parser->push_warning(p_assignable->identifier, GDScriptWarning::CONFUSABLE_LOCAL_DECLARATION, local.get_name(), p_assignable->identifier->name);
		}
	}
#endif

	GDScriptParser::DataType specified_type;
	bool has_specified_type = p_assignable->datatype_specifier != nullptr;
	if (has_specified_type) {
		specified_type = type_from_metatype(resolve_datatype(p_assignable->datatype_specifier));
		type = specified_type;
	}

	if (p_assignable->initializer != nullptr) {
		reduce_expression(p_assignable->initializer);

		if (p_assignable->initializer->type == GDScriptParser::Node::ARRAY) {
			GDScriptParser::ArrayNode *array = static_cast<GDScriptParser::ArrayNode *>(p_assignable->initializer);
			if (has_specified_type && specified_type.has_container_element_type(0)) {
				update_array_literal_element_type(array, specified_type.get_container_element_type(0));
			}
		} else if (p_assignable->initializer->type == GDScriptParser::Node::DICTIONARY) {
			GDScriptParser::DictionaryNode *dictionary = static_cast<GDScriptParser::DictionaryNode *>(p_assignable->initializer);
			if (has_specified_type && specified_type.has_container_element_types()) {
				update_dictionary_literal_element_type(dictionary, specified_type.get_container_element_type_or_variant(0), specified_type.get_container_element_type_or_variant(1));
			}
		}

		if (is_constant && !p_assignable->initializer->is_constant) {
			bool is_initializer_value_reduced = false;
			Variant initializer_value = make_expression_reduced_value(p_assignable->initializer, is_initializer_value_reduced);
			if (is_initializer_value_reduced) {
				p_assignable->initializer->is_constant = true;
				p_assignable->initializer->reduced_value = initializer_value;
			} else {
				push_error(vformat(R"(Assigned value for %s "%s" isn't a constant expression.)", p_kind, p_assignable->identifier->name), p_assignable->initializer);
			}
		}

		if (has_specified_type && p_assignable->initializer->is_constant) {
			update_const_expression_builtin_type(p_assignable->initializer, specified_type, "assign");
		}
		GDScriptParser::DataType initializer_type = p_assignable->initializer->get_datatype();

		if (p_assignable->infer_datatype) {
			if (!initializer_type.is_set() || initializer_type.has_no_type() || !initializer_type.is_hard_type()) {
				push_error(vformat(R"(Cannot infer the type of "%s" %s because the value doesn't have a set type.)", p_assignable->identifier->name, p_kind), p_assignable->initializer);
			} else if (initializer_type.kind == GDScriptParser::DataType::BUILTIN && initializer_type.builtin_type == Variant::NIL && !is_constant) {
				push_error(vformat(R"(Cannot infer the type of "%s" %s because the value is "null".)", p_assignable->identifier->name, p_kind), p_assignable->initializer);
			}
#ifdef DEBUG_ENABLED
			if (initializer_type.is_hard_type() && initializer_type.is_variant()) {
				parser->push_warning(p_assignable, GDScriptWarning::INFERENCE_ON_VARIANT, p_kind);
			}
#endif
		} else {
			if (!initializer_type.is_set()) {
				push_error(vformat(R"(Could not resolve type for %s "%s".)", p_kind, p_assignable->identifier->name), p_assignable->initializer);
			}
		}

		if (!has_specified_type) {
			type = initializer_type;

			if (!type.is_set() || (type.is_hard_type() && type.kind == GDScriptParser::DataType::BUILTIN && type.builtin_type == Variant::NIL && !is_constant)) {
				type.kind = GDScriptParser::DataType::VARIANT;
			}

			if (p_assignable->infer_datatype || is_constant) {
				type.type_source = GDScriptParser::DataType::ANNOTATED_INFERRED;
			} else {
				type.type_source = GDScriptParser::DataType::INFERRED;
			}
		} else if (!specified_type.is_variant()) {
			if (initializer_type.is_variant() || !initializer_type.is_hard_type()) {
				mark_node_unsafe(p_assignable->initializer);
				p_assignable->use_conversion_assign = true;
				if (!initializer_type.is_variant() && !is_type_compatible(specified_type, initializer_type, true, p_assignable->initializer)) {
					downgrade_node_type_source(p_assignable->initializer);
				}
			} else if (!is_type_compatible(specified_type, initializer_type, true, p_assignable->initializer)) {
				if (!is_constant && is_type_compatible(initializer_type, specified_type)) {
					mark_node_unsafe(p_assignable->initializer);
					p_assignable->use_conversion_assign = true;
				} else {
					push_error(vformat(R"(Cannot assign a value of type %s to %s "%s" with specified type %s.)", initializer_type.to_string(), p_kind, p_assignable->identifier->name, specified_type.to_string()), p_assignable->initializer);
				}
			} else if ((specified_type.has_container_element_type(0) && !initializer_type.has_container_element_type(0)) || (specified_type.has_container_element_type(1) && !initializer_type.has_container_element_type(1))) {
				mark_node_unsafe(p_assignable->initializer);
#ifdef DEBUG_ENABLED
			} else if (specified_type.builtin_type == Variant::INT && initializer_type.builtin_type == Variant::FLOAT) {
				parser->push_warning(p_assignable->initializer, GDScriptWarning::NARROWING_CONVERSION);
#endif
			}
		}
	}

#ifdef DEBUG_ENABLED
	const bool is_parameter = p_assignable->type == GDScriptParser::Node::PARAMETER;
	if (!has_specified_type) {
		const String declaration_type = is_constant ? "Constant" : (is_parameter ? "Parameter" : "Variable");
		if (p_assignable->infer_datatype || is_constant) {
			// Do not produce the `INFERRED_DECLARATION` warning on type import because there is no way to specify the true type.
			// And removing the metatype makes it impossible to use the constant as a type hint (especially for enums).
			const bool is_type_import = is_constant && p_assignable->initializer != nullptr && p_assignable->initializer->datatype.is_meta_type;
			if (!is_type_import) {
				parser->push_warning(p_assignable, GDScriptWarning::INFERRED_DECLARATION, declaration_type, p_assignable->identifier->name);
			}
		} else {
			parser->push_warning(p_assignable, GDScriptWarning::UNTYPED_DECLARATION, declaration_type, p_assignable->identifier->name);
		}
	} else if (!is_parameter && specified_type.kind == GDScriptParser::DataType::ENUM && p_assignable->initializer == nullptr) {
		// Warn about enum variables without default value. Unless the enum defines the "0" value, then it's fine.
		bool has_zero_value = false;
		for (const KeyValue<StringName, int64_t> &kv : specified_type.enum_values) {
			if (kv.value == 0) {
				has_zero_value = true;
				break;
			}
		}
		if (!has_zero_value) {
			parser->push_warning(p_assignable, GDScriptWarning::ENUM_VARIABLE_WITHOUT_DEFAULT, p_assignable->identifier->name);
		}
	}
#endif

	type.is_constant = is_constant;
	type.is_read_only = false;
	p_assignable->set_datatype(type);
}

void GDScriptAnalyzer::resolve_variable(GDScriptParser::VariableNode *p_variable, bool p_is_local) {
	static constexpr const char *kind = "variable";
	resolve_assignable(p_variable, kind);

#ifdef DEBUG_ENABLED
	if (p_is_local) {
		if (p_variable->usages == 0 && !String(p_variable->identifier->name).begins_with("_")) {
			parser->push_warning(p_variable, GDScriptWarning::UNUSED_VARIABLE, p_variable->identifier->name);
		}
	}
	is_shadowing(p_variable->identifier, kind, p_is_local);
#endif
}

void GDScriptAnalyzer::resolve_constant(GDScriptParser::ConstantNode *p_constant, bool p_is_local) {
	static constexpr const char *kind = "constant";
	resolve_assignable(p_constant, kind);

#ifdef DEBUG_ENABLED
	if (p_is_local) {
		if (p_constant->usages == 0 && !String(p_constant->identifier->name).begins_with("_")) {
			parser->push_warning(p_constant, GDScriptWarning::UNUSED_LOCAL_CONSTANT, p_constant->identifier->name);
		}
	}
	is_shadowing(p_constant->identifier, kind, p_is_local);
#endif
}

void GDScriptAnalyzer::resolve_parameter(GDScriptParser::ParameterNode *p_parameter) {
	static constexpr const char *kind = "parameter";
	resolve_assignable(p_parameter, kind);
}

void GDScriptAnalyzer::resolve_if(GDScriptParser::IfNode *p_if) {
	reduce_expression(p_if->condition);

	resolve_suite(p_if->true_block);
	p_if->set_datatype(p_if->true_block->get_datatype());

	if (p_if->false_block != nullptr) {
		resolve_suite(p_if->false_block);
		decide_suite_type(p_if, p_if->false_block);
	}
}

void GDScriptAnalyzer::resolve_for(GDScriptParser::ForNode *p_for) {
	bool list_resolved = false;

	// Optimize constant range() call to not allocate an array.
	// Use int, Vector2i, Vector3i instead, which also can be used as range iterators.
	if (p_for->list && p_for->list->type == GDScriptParser::Node::CALL) {
		GDScriptParser::CallNode *call = static_cast<GDScriptParser::CallNode *>(p_for->list);
		GDScriptParser::Node::Type callee_type = call->get_callee_type();
		if (callee_type == GDScriptParser::Node::IDENTIFIER) {
			GDScriptParser::IdentifierNode *callee = static_cast<GDScriptParser::IdentifierNode *>(call->callee);
			if (callee->name == "range") {
				list_resolved = true;
				if (call->arguments.size() < 1) {
					push_error(R"*(Invalid call for "range()" function. Expected at least 1 argument, none given.)*", call->callee);
				} else if (call->arguments.size() > 3) {
					push_error(vformat(R"*(Invalid call for "range()" function. Expected at most 3 arguments, %d given.)*", call->arguments.size()), call->callee);
				} else {
					// Now we can optimize it.
					bool can_reduce = true;
					Vector<Variant> args;
					args.resize(call->arguments.size());
					for (int i = 0; i < call->arguments.size(); i++) {
						GDScriptParser::ExpressionNode *argument = call->arguments[i];
						reduce_expression(argument);

						if (argument->is_constant) {
							if (argument->reduced_value.get_type() != Variant::INT && argument->reduced_value.get_type() != Variant::FLOAT) {
								can_reduce = false;
								push_error(vformat(R"*(Invalid argument for "range()" call. Argument %d should be int or float but "%s" was given.)*", i + 1, Variant::get_type_name(argument->reduced_value.get_type())), argument);
							}
							if (can_reduce) {
								args.write[i] = argument->reduced_value;
							}
						} else {
							can_reduce = false;
							GDScriptParser::DataType argument_type = argument->get_datatype();
							if (argument_type.is_variant() || !argument_type.is_hard_type()) {
								mark_node_unsafe(argument);
							}
							if (!argument_type.is_variant() && (argument_type.builtin_type != Variant::INT && argument_type.builtin_type != Variant::FLOAT)) {
								if (!argument_type.is_hard_type()) {
									downgrade_node_type_source(argument);
								} else {
									push_error(vformat(R"*(Invalid argument for "range()" call. Argument %d should be int or float but "%s" was given.)*", i + 1, argument_type.to_string()), argument);
								}
							}
						}
					}

					Variant reduced;

					if (can_reduce) {
						switch (args.size()) {
							case 1:
								reduced = (int32_t)args[0];
								break;
							case 2:
								reduced = Vector2i(args[0], args[1]);
								break;
							case 3:
								reduced = Vector3i(args[0], args[1], args[2]);
								break;
						}
						p_for->list->is_constant = true;
						p_for->list->reduced_value = reduced;
					}
				}

				if (p_for->list->is_constant) {
					p_for->list->set_datatype(type_from_variant(p_for->list->reduced_value, p_for->list));
				} else {
					GDScriptParser::DataType list_type;
					list_type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
					list_type.kind = GDScriptParser::DataType::BUILTIN;
					list_type.builtin_type = Variant::ARRAY;
					p_for->list->set_datatype(list_type);
				}
			}
		}
	}

	GDScriptParser::DataType variable_type;
	String list_visible_type = "<unresolved type>";
	if (list_resolved) {
		variable_type.type_source = GDScriptParser::DataType::ANNOTATED_INFERRED;
		variable_type.kind = GDScriptParser::DataType::BUILTIN;
		variable_type.builtin_type = Variant::INT;
		list_visible_type = "Array[int]"; // NOTE: `range()` has `Array` return type.
	} else if (p_for->list) {
		resolve_node(p_for->list, false);
		GDScriptParser::DataType list_type = p_for->list->get_datatype();
		list_visible_type = list_type.to_string();
		if (!list_type.is_hard_type()) {
			mark_node_unsafe(p_for->list);
		}
		if (list_type.is_variant()) {
			variable_type.kind = GDScriptParser::DataType::VARIANT;
			mark_node_unsafe(p_for->list);
		} else if (list_type.has_container_element_type(0)) {
			variable_type = list_type.get_container_element_type(0);
			variable_type.type_source = list_type.type_source;
		} else if (list_type.is_typed_container_type()) {
			variable_type = list_type.get_typed_container_type();
			variable_type.type_source = list_type.type_source;
		} else if (list_type.builtin_type == Variant::INT || list_type.builtin_type == Variant::FLOAT || list_type.builtin_type == Variant::STRING) {
			variable_type.type_source = list_type.type_source;
			variable_type.kind = GDScriptParser::DataType::BUILTIN;
			variable_type.builtin_type = list_type.builtin_type;
		} else if (list_type.builtin_type == Variant::VECTOR2I || list_type.builtin_type == Variant::VECTOR3I) {
			variable_type.type_source = list_type.type_source;
			variable_type.kind = GDScriptParser::DataType::BUILTIN;
			variable_type.builtin_type = Variant::INT;
		} else if (list_type.builtin_type == Variant::VECTOR2 || list_type.builtin_type == Variant::VECTOR3) {
			variable_type.type_source = list_type.type_source;
			variable_type.kind = GDScriptParser::DataType::BUILTIN;
			variable_type.builtin_type = Variant::FLOAT;
		} else if (list_type.builtin_type == Variant::OBJECT) {
			GDScriptParser::DataType return_type;
			List<GDScriptParser::DataType> par_types;
			int default_arg_count = 0;
			BitField<MethodFlags> method_flags;
			if (get_function_signature(p_for->list, false, list_type, CoreStringName(_iter_get), return_type, par_types, default_arg_count, method_flags)) {
				variable_type = return_type;
				variable_type.type_source = list_type.type_source;
			} else if (!list_type.is_hard_type()) {
				variable_type.kind = GDScriptParser::DataType::VARIANT;
			} else {
				push_error(vformat(R"(Unable to iterate on object of type "%s".)", list_type.to_string()), p_for->list);
			}
		} else if (list_type.builtin_type == Variant::ARRAY || list_type.builtin_type == Variant::DICTIONARY || !list_type.is_hard_type()) {
			variable_type.kind = GDScriptParser::DataType::VARIANT;
		} else {
			push_error(vformat(R"(Unable to iterate on value of type "%s".)", list_type.to_string()), p_for->list);
		}
	}

	if (p_for->variable) {
		if (p_for->datatype_specifier) {
			GDScriptParser::DataType specified_type = type_from_metatype(resolve_datatype(p_for->datatype_specifier));
			if (!specified_type.is_variant()) {
				if (variable_type.is_variant() || !variable_type.is_hard_type()) {
					mark_node_unsafe(p_for->variable);
					p_for->use_conversion_assign = true;
				} else if (!is_type_compatible(specified_type, variable_type, true, p_for->variable)) {
					if (is_type_compatible(variable_type, specified_type)) {
						mark_node_unsafe(p_for->variable);
						p_for->use_conversion_assign = true;
					} else {
						push_error(vformat(R"(Unable to iterate on value of type "%s" with variable of type "%s".)", list_visible_type, specified_type.to_string()), p_for->datatype_specifier);
					}
				} else if (!is_type_compatible(specified_type, variable_type)) {
					p_for->use_conversion_assign = true;
				}
				if (p_for->list) {
					if (p_for->list->type == GDScriptParser::Node::ARRAY) {
						update_array_literal_element_type(static_cast<GDScriptParser::ArrayNode *>(p_for->list), specified_type);
					} else if (p_for->list->type == GDScriptParser::Node::DICTIONARY) {
						update_dictionary_literal_element_type(static_cast<GDScriptParser::DictionaryNode *>(p_for->list), specified_type, GDScriptParser::DataType::get_variant_type());
					}
				}
			}
			p_for->variable->set_datatype(specified_type);
		} else {
			p_for->variable->set_datatype(variable_type);
#ifdef DEBUG_ENABLED
			if (variable_type.is_hard_type()) {
				parser->push_warning(p_for->variable, GDScriptWarning::INFERRED_DECLARATION, R"("for" iterator variable)", p_for->variable->name);
			} else {
				parser->push_warning(p_for->variable, GDScriptWarning::UNTYPED_DECLARATION, R"("for" iterator variable)", p_for->variable->name);
			}
#endif
		}
	}

	resolve_suite(p_for->loop);
	p_for->set_datatype(p_for->loop->get_datatype());
#ifdef DEBUG_ENABLED
	if (p_for->variable) {
		is_shadowing(p_for->variable, R"("for" iterator variable)", true);
	}
#endif
}

void GDScriptAnalyzer::resolve_while(GDScriptParser::WhileNode *p_while) {
	resolve_node(p_while->condition, false);

	resolve_suite(p_while->loop);
	p_while->set_datatype(p_while->loop->get_datatype());
}

void GDScriptAnalyzer::resolve_assert(GDScriptParser::AssertNode *p_assert) {
	reduce_expression(p_assert->condition);
	if (p_assert->message != nullptr) {
		reduce_expression(p_assert->message);
		if (!p_assert->message->get_datatype().has_no_type() && (p_assert->message->get_datatype().kind != GDScriptParser::DataType::BUILTIN || p_assert->message->get_datatype().builtin_type != Variant::STRING)) {
			push_error(R"(Expected string for assert error message.)", p_assert->message);
		}
	}

	p_assert->set_datatype(p_assert->condition->get_datatype());

#ifdef DEBUG_ENABLED
	if (p_assert->condition->is_constant) {
		if (p_assert->condition->reduced_value.booleanize()) {
			parser->push_warning(p_assert->condition, GDScriptWarning::ASSERT_ALWAYS_TRUE);
		} else if (!(p_assert->condition->type == GDScriptParser::Node::LITERAL && static_cast<GDScriptParser::LiteralNode *>(p_assert->condition)->value.get_type() == Variant::BOOL)) {
			parser->push_warning(p_assert->condition, GDScriptWarning::ASSERT_ALWAYS_FALSE);
		}
	}
#endif
}

void GDScriptAnalyzer::resolve_match(GDScriptParser::MatchNode *p_match) {
	reduce_expression(p_match->test);

	for (int i = 0; i < p_match->branches.size(); i++) {
		resolve_match_branch(p_match->branches[i], p_match->test);

		decide_suite_type(p_match, p_match->branches[i]);
	}
}

void GDScriptAnalyzer::resolve_match_branch(GDScriptParser::MatchBranchNode *p_match_branch, GDScriptParser::ExpressionNode *p_match_test) {
	// Apply annotations.
	for (GDScriptParser::AnnotationNode *&E : p_match_branch->annotations) {
		resolve_annotation(E);
		E->apply(parser, p_match_branch, nullptr); // TODO: Provide `p_class`.
	}

	for (int i = 0; i < p_match_branch->patterns.size(); i++) {
		resolve_match_pattern(p_match_branch->patterns[i], p_match_test);
	}

	if (p_match_branch->guard_body) {
		resolve_suite(p_match_branch->guard_body);
	}

	resolve_suite(p_match_branch->block);

	decide_suite_type(p_match_branch, p_match_branch->block);
}

void GDScriptAnalyzer::resolve_match_pattern(GDScriptParser::PatternNode *p_match_pattern, GDScriptParser::ExpressionNode *p_match_test) {
	if (p_match_pattern == nullptr) {
		return;
	}

	GDScriptParser::DataType result;

	switch (p_match_pattern->pattern_type) {
		case GDScriptParser::PatternNode::PT_LITERAL:
			if (p_match_pattern->literal) {
				reduce_literal(p_match_pattern->literal);
				result = p_match_pattern->literal->get_datatype();
			}
			break;
		case GDScriptParser::PatternNode::PT_EXPRESSION:
			if (p_match_pattern->expression) {
				GDScriptParser::ExpressionNode *expr = p_match_pattern->expression;
				reduce_expression(expr);
				result = expr->get_datatype();
				if (!expr->is_constant) {
					while (expr && expr->type == GDScriptParser::Node::SUBSCRIPT) {
						GDScriptParser::SubscriptNode *sub = static_cast<GDScriptParser::SubscriptNode *>(expr);
						if (!sub->is_attribute) {
							expr = nullptr;
						} else {
							expr = sub->base;
						}
					}
					if (!expr || expr->type != GDScriptParser::Node::IDENTIFIER) {
						push_error(R"(Expression in match pattern must be a constant expression, an identifier, or an attribute access ("A.B").)", expr);
					}
				}
			}
			break;
		case GDScriptParser::PatternNode::PT_BIND:
			if (p_match_test != nullptr) {
				result = p_match_test->get_datatype();
			} else {
				result.kind = GDScriptParser::DataType::VARIANT;
			}
			p_match_pattern->bind->set_datatype(result);
#ifdef DEBUG_ENABLED
			is_shadowing(p_match_pattern->bind, "pattern bind", true);
			if (p_match_pattern->bind->usages == 0 && !String(p_match_pattern->bind->name).begins_with("_")) {
				parser->push_warning(p_match_pattern->bind, GDScriptWarning::UNUSED_VARIABLE, p_match_pattern->bind->name);
			}
#endif
			break;
		case GDScriptParser::PatternNode::PT_ARRAY:
			for (int i = 0; i < p_match_pattern->array.size(); i++) {
				resolve_match_pattern(p_match_pattern->array[i], nullptr);
				decide_suite_type(p_match_pattern, p_match_pattern->array[i]);
			}
			result = p_match_pattern->get_datatype();
			break;
		case GDScriptParser::PatternNode::PT_DICTIONARY:
			for (int i = 0; i < p_match_pattern->dictionary.size(); i++) {
				if (p_match_pattern->dictionary[i].key) {
					reduce_expression(p_match_pattern->dictionary[i].key);
					if (!p_match_pattern->dictionary[i].key->is_constant) {
						push_error(R"(Expression in dictionary pattern key must be a constant.)", p_match_pattern->dictionary[i].key);
					}
				}

				if (p_match_pattern->dictionary[i].value_pattern) {
					resolve_match_pattern(p_match_pattern->dictionary[i].value_pattern, nullptr);
					decide_suite_type(p_match_pattern, p_match_pattern->dictionary[i].value_pattern);
				}
			}
			result = p_match_pattern->get_datatype();
			break;
		case GDScriptParser::PatternNode::PT_WILDCARD:
		case GDScriptParser::PatternNode::PT_REST:
			result.kind = GDScriptParser::DataType::VARIANT;
			break;
	}

	p_match_pattern->set_datatype(result);
}

void GDScriptAnalyzer::resolve_return(GDScriptParser::ReturnNode *p_return) {
	GDScriptParser::DataType result;

	GDScriptParser::DataType expected_type;
	bool has_expected_type = parser->current_function != nullptr;
	if (has_expected_type) {
		expected_type = parser->current_function->get_datatype();
	}

	if (p_return->return_value != nullptr) {
		bool is_void_function = has_expected_type && expected_type.is_hard_type() && expected_type.kind == GDScriptParser::DataType::BUILTIN && expected_type.builtin_type == Variant::NIL;
		bool is_call = p_return->return_value->type == GDScriptParser::Node::CALL;
		if (is_void_function && is_call) {
			// Pretend the call is a root expression to allow those that are "void".
			reduce_call(static_cast<GDScriptParser::CallNode *>(p_return->return_value), false, true);
		} else {
			reduce_expression(p_return->return_value);
		}
		if (is_void_function) {
			p_return->void_return = true;
			const GDScriptParser::DataType &return_type = p_return->return_value->datatype;
			if (is_call && !return_type.is_hard_type()) {
				String function_name = parser->current_function->identifier ? parser->current_function->identifier->name.operator String() : String("<anonymous function>");
				String called_function_name = static_cast<GDScriptParser::CallNode *>(p_return->return_value)->function_name.operator String();
#ifdef DEBUG_ENABLED
				parser->push_warning(p_return, GDScriptWarning::UNSAFE_VOID_RETURN, function_name, called_function_name);
#endif
				mark_node_unsafe(p_return);
			} else if (!is_call) {
				push_error("A void function cannot return a value.", p_return);
			}
			result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
			result.kind = GDScriptParser::DataType::BUILTIN;
			result.builtin_type = Variant::NIL;
			result.is_constant = true;
		} else {
			if (p_return->return_value->type == GDScriptParser::Node::ARRAY && has_expected_type && expected_type.has_container_element_type(0)) {
				update_array_literal_element_type(static_cast<GDScriptParser::ArrayNode *>(p_return->return_value), expected_type.get_container_element_type(0));
			} else if (p_return->return_value->type == GDScriptParser::Node::DICTIONARY && has_expected_type && expected_type.has_container_element_types()) {
				update_dictionary_literal_element_type(static_cast<GDScriptParser::DictionaryNode *>(p_return->return_value),
						expected_type.get_container_element_type_or_variant(0), expected_type.get_container_element_type_or_variant(1));
			}
			if (has_expected_type && expected_type.is_hard_type() && p_return->return_value->is_constant) {
				update_const_expression_builtin_type(p_return->return_value, expected_type, "return");
			}
			result = p_return->return_value->get_datatype();
		}
	} else {
		// Return type is null by default.
		result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
		result.kind = GDScriptParser::DataType::BUILTIN;
		result.builtin_type = Variant::NIL;
		result.is_constant = true;
	}

	if (has_expected_type && !expected_type.is_variant()) {
		if (result.is_variant() || !result.is_hard_type()) {
			mark_node_unsafe(p_return);
			if (!is_type_compatible(expected_type, result, true, p_return)) {
				downgrade_node_type_source(p_return);
			}
		} else if (!is_type_compatible(expected_type, result, true, p_return)) {
			mark_node_unsafe(p_return);
			if (!is_type_compatible(result, expected_type)) {
				push_error(vformat(R"(Cannot return value of type "%s" because the function return type is "%s".)", result.to_string(), expected_type.to_string()), p_return);
			}
#ifdef DEBUG_ENABLED
		} else if (expected_type.builtin_type == Variant::INT && result.builtin_type == Variant::FLOAT) {
			parser->push_warning(p_return, GDScriptWarning::NARROWING_CONVERSION);
#endif
		}
	}

	p_return->set_datatype(result);
}

void GDScriptAnalyzer::reduce_expression(GDScriptParser::ExpressionNode *p_expression, bool p_is_root) {
	// This one makes some magic happen.

	if (p_expression == nullptr) {
		return;
	}

	if (p_expression->reduced) {
		// Don't do this more than once.
		return;
	}

	p_expression->reduced = true;

	switch (p_expression->type) {
		case GDScriptParser::Node::ARRAY:
			reduce_array(static_cast<GDScriptParser::ArrayNode *>(p_expression));
			break;
		case GDScriptParser::Node::ASSIGNMENT:
			reduce_assignment(static_cast<GDScriptParser::AssignmentNode *>(p_expression));
			break;
		case GDScriptParser::Node::AWAIT:
			reduce_await(static_cast<GDScriptParser::AwaitNode *>(p_expression));
			break;
		case GDScriptParser::Node::BINARY_OPERATOR:
			reduce_binary_op(static_cast<GDScriptParser::BinaryOpNode *>(p_expression));
			break;
		case GDScriptParser::Node::CALL:
			reduce_call(static_cast<GDScriptParser::CallNode *>(p_expression), false, p_is_root);
			break;
		case GDScriptParser::Node::CAST:
			reduce_cast(static_cast<GDScriptParser::CastNode *>(p_expression));
			break;
		case GDScriptParser::Node::DICTIONARY:
			reduce_dictionary(static_cast<GDScriptParser::DictionaryNode *>(p_expression));
			break;
		case GDScriptParser::Node::GET_NODE:
			reduce_get_node(static_cast<GDScriptParser::GetNodeNode *>(p_expression));
			break;
		case GDScriptParser::Node::IDENTIFIER:
			reduce_identifier(static_cast<GDScriptParser::IdentifierNode *>(p_expression));
			break;
		case GDScriptParser::Node::LAMBDA:
			reduce_lambda(static_cast<GDScriptParser::LambdaNode *>(p_expression));
			break;
		case GDScriptParser::Node::LITERAL:
			reduce_literal(static_cast<GDScriptParser::LiteralNode *>(p_expression));
			break;
		case GDScriptParser::Node::PRELOAD:
			reduce_preload(static_cast<GDScriptParser::PreloadNode *>(p_expression));
			break;
		case GDScriptParser::Node::SELF:
			reduce_self(static_cast<GDScriptParser::SelfNode *>(p_expression));
			break;
		case GDScriptParser::Node::SUBSCRIPT:
			reduce_subscript(static_cast<GDScriptParser::SubscriptNode *>(p_expression));
			break;
		case GDScriptParser::Node::TERNARY_OPERATOR:
			reduce_ternary_op(static_cast<GDScriptParser::TernaryOpNode *>(p_expression), p_is_root);
			break;
		case GDScriptParser::Node::TYPE_TEST:
			reduce_type_test(static_cast<GDScriptParser::TypeTestNode *>(p_expression));
			break;
		case GDScriptParser::Node::UNARY_OPERATOR:
			reduce_unary_op(static_cast<GDScriptParser::UnaryOpNode *>(p_expression));
			break;
		// Non-expressions. Here only to make sure new nodes aren't forgotten.
		case GDScriptParser::Node::NONE:
		case GDScriptParser::Node::ANNOTATION:
		case GDScriptParser::Node::ASSERT:
		case GDScriptParser::Node::BREAK:
		case GDScriptParser::Node::BREAKPOINT:
		case GDScriptParser::Node::CLASS:
		case GDScriptParser::Node::CONSTANT:
		case GDScriptParser::Node::CONTINUE:
		case GDScriptParser::Node::ENUM:
		case GDScriptParser::Node::FOR:
		case GDScriptParser::Node::FUNCTION:
		case GDScriptParser::Node::IF:
		case GDScriptParser::Node::MATCH:
		case GDScriptParser::Node::MATCH_BRANCH:
		case GDScriptParser::Node::PARAMETER:
		case GDScriptParser::Node::PASS:
		case GDScriptParser::Node::PATTERN:
		case GDScriptParser::Node::RETURN:
		case GDScriptParser::Node::SIGNAL:
		case GDScriptParser::Node::SUITE:
		case GDScriptParser::Node::TYPE:
		case GDScriptParser::Node::VARIABLE:
		case GDScriptParser::Node::WHILE:
			ERR_FAIL_MSG("Reaching unreachable case");
	}

	if (p_expression->get_datatype().kind == GDScriptParser::DataType::UNRESOLVED) {
		// Prevent `is_type_compatible()` errors for incomplete expressions.
		// The error can still occur if `reduce_*()` is called directly.
		GDScriptParser::DataType dummy;
		dummy.kind = GDScriptParser::DataType::VARIANT;
		p_expression->set_datatype(dummy);
	}
}

void GDScriptAnalyzer::reduce_array(GDScriptParser::ArrayNode *p_array) {
	for (int i = 0; i < p_array->elements.size(); i++) {
		GDScriptParser::ExpressionNode *element = p_array->elements[i];
		reduce_expression(element);
	}

	// It's array in any case.
	GDScriptParser::DataType arr_type;
	arr_type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	arr_type.kind = GDScriptParser::DataType::BUILTIN;
	arr_type.builtin_type = Variant::ARRAY;
	arr_type.is_constant = true;

	p_array->set_datatype(arr_type);
}

#ifdef DEBUG_ENABLED
static bool enum_has_value(const GDScriptParser::DataType p_type, int64_t p_value) {
	for (const KeyValue<StringName, int64_t> &E : p_type.enum_values) {
		if (E.value == p_value) {
			return true;
		}
	}
	return false;
}
#endif

void GDScriptAnalyzer::update_const_expression_builtin_type(GDScriptParser::ExpressionNode *p_expression, const GDScriptParser::DataType &p_type, const char *p_usage, bool p_is_cast) {
	if (p_expression->get_datatype() == p_type) {
		return;
	}
	if (p_type.kind != GDScriptParser::DataType::BUILTIN && p_type.kind != GDScriptParser::DataType::ENUM) {
		return;
	}

	GDScriptParser::DataType expression_type = p_expression->get_datatype();
	bool is_enum_cast = p_is_cast && p_type.kind == GDScriptParser::DataType::ENUM && p_type.is_meta_type == false && expression_type.builtin_type == Variant::INT;
	if (!is_enum_cast && !is_type_compatible(p_type, expression_type, true, p_expression)) {
		push_error(vformat(R"(Cannot %s a value of type "%s" as "%s".)", p_usage, expression_type.to_string(), p_type.to_string()), p_expression);
		return;
	}

	GDScriptParser::DataType value_type = type_from_variant(p_expression->reduced_value, p_expression);
	if (expression_type.is_variant() && !is_enum_cast && !is_type_compatible(p_type, value_type, true, p_expression)) {
		push_error(vformat(R"(Cannot %s a value of type "%s" as "%s".)", p_usage, value_type.to_string(), p_type.to_string()), p_expression);
		return;
	}

#ifdef DEBUG_ENABLED
	if (p_type.kind == GDScriptParser::DataType::ENUM && value_type.builtin_type == Variant::INT && !enum_has_value(p_type, p_expression->reduced_value)) {
		parser->push_warning(p_expression, GDScriptWarning::INT_AS_ENUM_WITHOUT_MATCH, p_usage, p_expression->reduced_value.stringify(), p_type.to_string());
	}
#endif

	if (value_type.builtin_type == p_type.builtin_type) {
		p_expression->set_datatype(p_type);
		return;
	}

	Variant converted_to;
	const Variant *converted_from = &p_expression->reduced_value;
	Callable::CallError call_error;
	Variant::construct(p_type.builtin_type, converted_to, &converted_from, 1, call_error);
	if (call_error.error) {
		push_error(vformat(R"(Failed to convert a value of type "%s" to "%s".)", value_type.to_string(), p_type.to_string()), p_expression);
		return;
	}

#ifdef DEBUG_ENABLED
	if (p_type.builtin_type == Variant::INT && value_type.builtin_type == Variant::FLOAT) {
		parser->push_warning(p_expression, GDScriptWarning::NARROWING_CONVERSION);
	}
#endif

	p_expression->reduced_value = converted_to;
	p_expression->set_datatype(p_type);
}

// When an array literal is stored (or passed as function argument) to a typed context, we then assume the array is typed.
// This function determines which type is that (if any).
void GDScriptAnalyzer::update_array_literal_element_type(GDScriptParser::ArrayNode *p_array, const GDScriptParser::DataType &p_element_type) {
	GDScriptParser::DataType expected_type = p_element_type;
	expected_type.container_element_types.clear(); // Nested types (like `Array[Array[int]]`) are not currently supported.

	for (int i = 0; i < p_array->elements.size(); i++) {
		GDScriptParser::ExpressionNode *element_node = p_array->elements[i];
		if (element_node->is_constant) {
			update_const_expression_builtin_type(element_node, expected_type, "include");
		}
		const GDScriptParser::DataType &actual_type = element_node->get_datatype();
		if (actual_type.has_no_type() || actual_type.is_variant() || !actual_type.is_hard_type()) {
			mark_node_unsafe(element_node);
			continue;
		}
		if (!is_type_compatible(expected_type, actual_type, true, p_array)) {
			if (is_type_compatible(actual_type, expected_type)) {
				mark_node_unsafe(element_node);
				continue;
			}
			push_error(vformat(R"(Cannot have an element of type "%s" in an array of type "Array[%s]".)", actual_type.to_string(), expected_type.to_string()), element_node);
			return;
		}
	}

	GDScriptParser::DataType array_type = p_array->get_datatype();
	array_type.set_container_element_type(0, expected_type);
	p_array->set_datatype(array_type);
}

// When a dictionary literal is stored (or passed as function argument) to a typed context, we then assume the dictionary is typed.
// This function determines which type is that (if any).
void GDScriptAnalyzer::update_dictionary_literal_element_type(GDScriptParser::DictionaryNode *p_dictionary, const GDScriptParser::DataType &p_key_element_type, const GDScriptParser::DataType &p_value_element_type) {
	GDScriptParser::DataType expected_key_type = p_key_element_type;
	GDScriptParser::DataType expected_value_type = p_value_element_type;
	expected_key_type.container_element_types.clear(); // Nested types (like `Dictionary[String, Array[int]]`) are not currently supported.
	expected_value_type.container_element_types.clear();

	for (int i = 0; i < p_dictionary->elements.size(); i++) {
		GDScriptParser::ExpressionNode *key_element_node = p_dictionary->elements[i].key;
		if (key_element_node->is_constant) {
			update_const_expression_builtin_type(key_element_node, expected_key_type, "include");
		}
		const GDScriptParser::DataType &actual_key_type = key_element_node->get_datatype();
		if (actual_key_type.has_no_type() || actual_key_type.is_variant() || !actual_key_type.is_hard_type()) {
			mark_node_unsafe(key_element_node);
		} else if (!is_type_compatible(expected_key_type, actual_key_type, true, p_dictionary)) {
			if (is_type_compatible(actual_key_type, expected_key_type)) {
				mark_node_unsafe(key_element_node);
			} else {
				push_error(vformat(R"(Cannot have a key of type "%s" in a dictionary of type "Dictionary[%s, %s]".)", actual_key_type.to_string(), expected_key_type.to_string(), expected_value_type.to_string()), key_element_node);
				return;
			}
		}

		GDScriptParser::ExpressionNode *value_element_node = p_dictionary->elements[i].value;
		if (value_element_node->is_constant) {
			update_const_expression_builtin_type(value_element_node, expected_value_type, "include");
		}
		const GDScriptParser::DataType &actual_value_type = value_element_node->get_datatype();
		if (actual_value_type.has_no_type() || actual_value_type.is_variant() || !actual_value_type.is_hard_type()) {
			mark_node_unsafe(value_element_node);
		} else if (!is_type_compatible(expected_value_type, actual_value_type, true, p_dictionary)) {
			if (is_type_compatible(actual_value_type, expected_value_type)) {
				mark_node_unsafe(value_element_node);
			} else {
				push_error(vformat(R"(Cannot have a value of type "%s" in a dictionary of type "Dictionary[%s, %s]".)", actual_value_type.to_string(), expected_key_type.to_string(), expected_value_type.to_string()), value_element_node);
				return;
			}
		}
	}

	GDScriptParser::DataType dictionary_type = p_dictionary->get_datatype();
	dictionary_type.set_container_element_type(0, expected_key_type);
	dictionary_type.set_container_element_type(1, expected_value_type);
	p_dictionary->set_datatype(dictionary_type);
}

void GDScriptAnalyzer::reduce_assignment(GDScriptParser::AssignmentNode *p_assignment) {
	reduce_expression(p_assignment->assigned_value);

#ifdef DEBUG_ENABLED
	// Increment assignment count for local variables.
	// Before we reduce the assignee because we don't want to warn about not being assigned when performing the assignment.
	if (p_assignment->assignee->type == GDScriptParser::Node::IDENTIFIER) {
		GDScriptParser::IdentifierNode *id = static_cast<GDScriptParser::IdentifierNode *>(p_assignment->assignee);
		if (id->source == GDScriptParser::IdentifierNode::LOCAL_VARIABLE && id->variable_source) {
			id->variable_source->assignments++;
		}
	}
#endif

	reduce_expression(p_assignment->assignee);

#ifdef DEBUG_ENABLED
	{
		bool is_subscript = false;
		GDScriptParser::ExpressionNode *base = p_assignment->assignee;
		while (base && base->type == GDScriptParser::Node::SUBSCRIPT) {
			is_subscript = true;
			base = static_cast<GDScriptParser::SubscriptNode *>(base)->base;
		}
		if (base && base->type == GDScriptParser::Node::IDENTIFIER) {
			GDScriptParser::IdentifierNode *id = static_cast<GDScriptParser::IdentifierNode *>(base);
			if (current_lambda && current_lambda->captures_indices.has(id->name)) {
				bool need_warn = false;
				if (is_subscript) {
					const GDScriptParser::DataType &id_type = id->datatype;
					if (id_type.is_hard_type()) {
						switch (id_type.kind) {
							case GDScriptParser::DataType::BUILTIN:
								// TODO: Change `Variant::is_type_shared()` to include packed arrays?
								need_warn = !Variant::is_type_shared(id_type.builtin_type) && id_type.builtin_type < Variant::PACKED_BYTE_ARRAY;
								break;
							case GDScriptParser::DataType::ENUM:
								need_warn = true;
								break;
							default:
								break;
						}
					}
				} else {
					need_warn = true;
				}
				if (need_warn) {
					parser->push_warning(p_assignment, GDScriptWarning::CONFUSABLE_CAPTURE_REASSIGNMENT, id->name);
				}
			}
		}
	}
#endif

	if (p_assignment->assigned_value == nullptr || p_assignment->assignee == nullptr) {
		return;
	}

	GDScriptParser::DataType assignee_type = p_assignment->assignee->get_datatype();

	if (assignee_type.is_constant) {
		push_error("Cannot assign a new value to a constant.", p_assignment->assignee);
		return;
	} else if (p_assignment->assignee->type == GDScriptParser::Node::SUBSCRIPT && static_cast<GDScriptParser::SubscriptNode *>(p_assignment->assignee)->base->is_constant) {
		const GDScriptParser::DataType &base_type = static_cast<GDScriptParser::SubscriptNode *>(p_assignment->assignee)->base->datatype;
		if (base_type.kind != GDScriptParser::DataType::SCRIPT && base_type.kind != GDScriptParser::DataType::CLASS) { // Static variables.
			push_error("Cannot assign a new value to a constant.", p_assignment->assignee);
			return;
		}
	} else if (assignee_type.is_read_only) {
		push_error("Cannot assign a new value to a read-only property.", p_assignment->assignee);
		return;
	} else if (p_assignment->assignee->type == GDScriptParser::Node::SUBSCRIPT) {
		GDScriptParser::SubscriptNode *sub = static_cast<GDScriptParser::SubscriptNode *>(p_assignment->assignee);
		while (sub) {
			const GDScriptParser::DataType &base_type = sub->base->datatype;
			if (base_type.is_hard_type() && base_type.is_read_only) {
				if (base_type.kind == GDScriptParser::DataType::BUILTIN && !Variant::is_type_shared(base_type.builtin_type)) {
					push_error("Cannot assign a new value to a read-only property.", p_assignment->assignee);
					return;
				}
			} else {
				break;
			}
			if (sub->base->type == GDScriptParser::Node::SUBSCRIPT) {
				sub = static_cast<GDScriptParser::SubscriptNode *>(sub->base);
			} else {
				sub = nullptr;
			}
		}
	}

	// Check if assigned value is an array/dictionary literal, so we can make it a typed container too if appropriate.
	if (p_assignment->assigned_value->type == GDScriptParser::Node::ARRAY && assignee_type.is_hard_type() && assignee_type.has_container_element_type(0)) {
		update_array_literal_element_type(static_cast<GDScriptParser::ArrayNode *>(p_assignment->assigned_value), assignee_type.get_container_element_type(0));
	} else if (p_assignment->assigned_value->type == GDScriptParser::Node::DICTIONARY && assignee_type.is_hard_type() && assignee_type.has_container_element_types()) {
		update_dictionary_literal_element_type(static_cast<GDScriptParser::DictionaryNode *>(p_assignment->assigned_value),
				assignee_type.get_container_element_type_or_variant(0), assignee_type.get_container_element_type_or_variant(1));
	}

	if (p_assignment->operation == GDScriptParser::AssignmentNode::OP_NONE && assignee_type.is_hard_type() && p_assignment->assigned_value->is_constant) {
		update_const_expression_builtin_type(p_assignment->assigned_value, assignee_type, "assign");
	}

	GDScriptParser::DataType assigned_value_type = p_assignment->assigned_value->get_datatype();

	bool assignee_is_variant = assignee_type.is_variant();
	bool assignee_is_hard = assignee_type.is_hard_type();
	bool assigned_is_variant = assigned_value_type.is_variant();
	bool assigned_is_hard = assigned_value_type.is_hard_type();
	bool compatible = true;
	bool downgrades_assignee = false;
	bool downgrades_assigned = false;
	GDScriptParser::DataType op_type = assigned_value_type;
	if (p_assignment->operation != GDScriptParser::AssignmentNode::OP_NONE && !op_type.is_variant()) {
		op_type = get_operation_type(p_assignment->variant_op, assignee_type, assigned_value_type, compatible, p_assignment->assigned_value);

		if (assignee_is_variant) {
			// variant assignee
			mark_node_unsafe(p_assignment);
		} else if (!compatible) {
			// incompatible hard types and non-variant assignee
			mark_node_unsafe(p_assignment);
			if (assigned_is_variant) {
				// incompatible hard non-variant assignee and hard variant assigned
				p_assignment->use_conversion_assign = true;
			} else {
				// incompatible hard non-variant types
				push_error(vformat(R"(Invalid operands "%s" and "%s" for assignment operator.)", assignee_type.to_string(), assigned_value_type.to_string()), p_assignment);
			}
		} else if (op_type.type_source == GDScriptParser::DataType::UNDETECTED && !assigned_is_variant) {
			// incompatible non-variant types (at least one weak)
			downgrades_assignee = !assignee_is_hard;
			downgrades_assigned = !assigned_is_hard;
		}
	}
	p_assignment->set_datatype(op_type);

	if (assignee_is_variant) {
		if (!assignee_is_hard) {
			// weak variant assignee
			mark_node_unsafe(p_assignment);
		}
	} else {
		if (assignee_is_hard && !assigned_is_hard) {
			// hard non-variant assignee and weak assigned
			mark_node_unsafe(p_assignment);
			p_assignment->use_conversion_assign = true;
			downgrades_assigned = downgrades_assigned || (!assigned_is_variant && !is_type_compatible(assignee_type, op_type, true, p_assignment->assigned_value));
		} else if (compatible) {
			if (op_type.is_variant()) {
				// non-variant assignee and variant result
				mark_node_unsafe(p_assignment);
				if (assignee_is_hard) {
					// hard non-variant assignee and variant result
					p_assignment->use_conversion_assign = true;
				} else {
					// weak non-variant assignee and variant result
					downgrades_assignee = true;
				}
			} else if (!is_type_compatible(assignee_type, op_type, assignee_is_hard, p_assignment->assigned_value)) {
				// non-variant assignee and incompatible result
				mark_node_unsafe(p_assignment);
				if (assignee_is_hard) {
					if (is_type_compatible(op_type, assignee_type)) {
						// hard non-variant assignee and maybe compatible result
						p_assignment->use_conversion_assign = true;
					} else {
						// hard non-variant assignee and incompatible result
						push_error(vformat(R"(Value of type "%s" cannot be assigned to a variable of type "%s".)", assigned_value_type.to_string(), assignee_type.to_string()), p_assignment->assigned_value);
					}
				} else {
					// weak non-variant assignee and incompatible result
					downgrades_assignee = true;
				}
			} else if ((assignee_type.has_container_element_type(0) && !op_type.has_container_element_type(0)) || (assignee_type.has_container_element_type(1) && !op_type.has_container_element_type(1))) {
				// Typed assignee and untyped result.
				mark_node_unsafe(p_assignment);
			}
		}
	}

	if (downgrades_assignee) {
		downgrade_node_type_source(p_assignment->assignee);
	}
	if (downgrades_assigned) {
		downgrade_node_type_source(p_assignment->assigned_value);
	}

#ifdef DEBUG_ENABLED
	if (assignee_type.is_hard_type() && assignee_type.builtin_type == Variant::INT && assigned_value_type.builtin_type == Variant::FLOAT) {
		parser->push_warning(p_assignment->assigned_value, GDScriptWarning::NARROWING_CONVERSION);
	}
	// Check for assignment with operation before assignment.
	if (p_assignment->operation != GDScriptParser::AssignmentNode::OP_NONE && p_assignment->assignee->type == GDScriptParser::Node::IDENTIFIER) {
		GDScriptParser::IdentifierNode *id = static_cast<GDScriptParser::IdentifierNode *>(p_assignment->assignee);
		// Use == 1 here because this assignment was already counted in the beginning of the function.
		if (id->source == GDScriptParser::IdentifierNode::LOCAL_VARIABLE && id->variable_source && id->variable_source->assignments == 1) {
			parser->push_warning(p_assignment, GDScriptWarning::UNASSIGNED_VARIABLE_OP_ASSIGN, id->name);
		}
	}
#endif
}

void GDScriptAnalyzer::reduce_await(GDScriptParser::AwaitNode *p_await) {
	if (p_await->to_await == nullptr) {
		GDScriptParser::DataType await_type;
		await_type.kind = GDScriptParser::DataType::VARIANT;
		p_await->set_datatype(await_type);
		return;
	}

	if (p_await->to_await->type == GDScriptParser::Node::CALL) {
		reduce_call(static_cast<GDScriptParser::CallNode *>(p_await->to_await), true);
	} else {
		reduce_expression(p_await->to_await);
	}

	GDScriptParser::DataType await_type = p_await->to_await->get_datatype();
	// We cannot infer the type of the result of waiting for a signal.
	if (await_type.is_hard_type() && await_type.kind == GDScriptParser::DataType::BUILTIN && await_type.builtin_type == Variant::SIGNAL) {
		await_type.kind = GDScriptParser::DataType::VARIANT;
		await_type.type_source = GDScriptParser::DataType::UNDETECTED;
	} else if (p_await->to_await->is_constant) {
		p_await->is_constant = p_await->to_await->is_constant;
		p_await->reduced_value = p_await->to_await->reduced_value;
	}
	await_type.is_coroutine = false;
	p_await->set_datatype(await_type);

#ifdef DEBUG_ENABLED
	GDScriptParser::DataType to_await_type = p_await->to_await->get_datatype();
	if (!to_await_type.is_coroutine && !to_await_type.is_variant() && to_await_type.builtin_type != Variant::SIGNAL) {
		parser->push_warning(p_await, GDScriptWarning::REDUNDANT_AWAIT);
	}
#endif
}

void GDScriptAnalyzer::reduce_binary_op(GDScriptParser::BinaryOpNode *p_binary_op) {
	reduce_expression(p_binary_op->left_operand);
	reduce_expression(p_binary_op->right_operand);

	GDScriptParser::DataType left_type;
	if (p_binary_op->left_operand) {
		left_type = p_binary_op->left_operand->get_datatype();
	}
	GDScriptParser::DataType right_type;
	if (p_binary_op->right_operand) {
		right_type = p_binary_op->right_operand->get_datatype();
	}

	if (!left_type.is_set() || !right_type.is_set()) {
		return;
	}

#ifdef DEBUG_ENABLED
	if (p_binary_op->variant_op == Variant::OP_DIVIDE && left_type.builtin_type == Variant::INT && right_type.builtin_type == Variant::INT) {
		parser->push_warning(p_binary_op, GDScriptWarning::INTEGER_DIVISION);
	}
#endif

	if (p_binary_op->left_operand->is_constant && p_binary_op->right_operand->is_constant) {
		p_binary_op->is_constant = true;
		if (p_binary_op->variant_op < Variant::OP_MAX) {
			bool valid = false;
			Variant::evaluate(p_binary_op->variant_op, p_binary_op->left_operand->reduced_value, p_binary_op->right_operand->reduced_value, p_binary_op->reduced_value, valid);
			if (!valid) {
				if (p_binary_op->reduced_value.get_type() == Variant::STRING) {
					push_error(vformat(R"(%s in operator %s.)", p_binary_op->reduced_value, Variant::get_operator_name(p_binary_op->variant_op)), p_binary_op);
				} else {
					push_error(vformat(R"(Invalid operands to operator %s, %s and %s.)",
									   Variant::get_operator_name(p_binary_op->variant_op),
									   Variant::get_type_name(p_binary_op->left_operand->reduced_value.get_type()),
									   Variant::get_type_name(p_binary_op->right_operand->reduced_value.get_type())),
							p_binary_op);
				}
			}
		} else {
			ERR_PRINT("Parser bug: unknown binary operation.");
		}
		p_binary_op->set_datatype(type_from_variant(p_binary_op->reduced_value, p_binary_op));

		return;
	}

	GDScriptParser::DataType result;

	if ((p_binary_op->variant_op == Variant::OP_EQUAL || p_binary_op->variant_op == Variant::OP_NOT_EQUAL) &&
			((left_type.kind == GDScriptParser::DataType::BUILTIN && left_type.builtin_type == Variant::NIL) || (right_type.kind == GDScriptParser::DataType::BUILTIN && right_type.builtin_type == Variant::NIL))) {
		// "==" and "!=" operators always return a boolean when comparing to null.
		result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
		result.kind = GDScriptParser::DataType::BUILTIN;
		result.builtin_type = Variant::BOOL;
	} else if (p_binary_op->variant_op == Variant::OP_MODULE && left_type.builtin_type == Variant::STRING) {
		// The modulo operator (%) on string acts as formatting and will always return a string.
		result.type_source = left_type.type_source;
		result.kind = GDScriptParser::DataType::BUILTIN;
		result.builtin_type = Variant::STRING;
	} else if (left_type.is_variant() || right_type.is_variant()) {
		// Cannot infer type because one operand can be anything.
		result.kind = GDScriptParser::DataType::VARIANT;
		mark_node_unsafe(p_binary_op);
	} else if (p_binary_op->variant_op < Variant::OP_MAX) {
		bool valid = false;
		result = get_operation_type(p_binary_op->variant_op, left_type, right_type, valid, p_binary_op);
		if (!valid) {
			push_error(vformat(R"(Invalid operands "%s" and "%s" for "%s" operator.)", left_type.to_string(), right_type.to_string(), Variant::get_operator_name(p_binary_op->variant_op)), p_binary_op);
		} else if (!result.is_hard_type()) {
			mark_node_unsafe(p_binary_op);
		}
	} else {
		ERR_PRINT("Parser bug: unknown binary operation.");
	}

	p_binary_op->set_datatype(result);
}

#ifdef SUGGEST_GODOT4_RENAMES
const char *get_rename_from_map(const char *map[][2], String key) {
	for (int index = 0; map[index][0]; index++) {
		if (map[index][0] == key) {
			return map[index][1];
		}
	}
	return nullptr;
}

// Checks if an identifier/function name has been renamed in Godot 4, uses ProjectConverter3To4 for rename map.
// Returns the new name if found, nullptr otherwise.
const char *check_for_renamed_identifier(String identifier, GDScriptParser::Node::Type type) {
	switch (type) {
		case GDScriptParser::Node::IDENTIFIER: {
			// Check properties
			const char *result = get_rename_from_map(RenamesMap3To4::gdscript_properties_renames, identifier);
			if (result) {
				return result;
			}
			// Check enum values
			result = get_rename_from_map(RenamesMap3To4::enum_renames, identifier);
			if (result) {
				return result;
			}
			// Check color constants
			result = get_rename_from_map(RenamesMap3To4::color_renames, identifier);
			if (result) {
				return result;
			}
			// Check type names
			result = get_rename_from_map(RenamesMap3To4::class_renames, identifier);
			if (result) {
				return result;
			}
			return get_rename_from_map(RenamesMap3To4::builtin_types_renames, identifier);
		}
		case GDScriptParser::Node::CALL: {
			const char *result = get_rename_from_map(RenamesMap3To4::gdscript_function_renames, identifier);
			if (result) {
				return result;
			}
			// Built-in Types are mistaken for function calls when the built-in type is not found.
			// Check built-in types if function rename not found
			return get_rename_from_map(RenamesMap3To4::builtin_types_renames, identifier);
		}
		// Signal references don't get parsed through the GDScriptAnalyzer. No support for signal rename hints.
		default:
			// No rename found, return null
			return nullptr;
	}
}
#endif // SUGGEST_GODOT4_RENAMES

void GDScriptAnalyzer::reduce_call(GDScriptParser::CallNode *p_call, bool p_is_await, bool p_is_root) {
	bool all_is_constant = true;
	HashMap<int, GDScriptParser::ArrayNode *> arrays; // For array literal to potentially type when passing.
	HashMap<int, GDScriptParser::DictionaryNode *> dictionaries; // Same, but for dictionaries.
	for (int i = 0; i < p_call->arguments.size(); i++) {
		reduce_expression(p_call->arguments[i]);
		if (p_call->arguments[i]->type == GDScriptParser::Node::ARRAY) {
			arrays[i] = static_cast<GDScriptParser::ArrayNode *>(p_call->arguments[i]);
		} else if (p_call->arguments[i]->type == GDScriptParser::Node::DICTIONARY) {
			dictionaries[i] = static_cast<GDScriptParser::DictionaryNode *>(p_call->arguments[i]);
		}
		all_is_constant = all_is_constant && p_call->arguments[i]->is_constant;
	}

	GDScriptParser::Node::Type callee_type = p_call->get_callee_type();
	GDScriptParser::DataType call_type;

	if (!p_call->is_super && callee_type == GDScriptParser::Node::IDENTIFIER) {
		// Call to name directly.
		StringName function_name = p_call->function_name;

		if (function_name == SNAME("Object")) {
			push_error(R"*(Invalid constructor "Object()", use "Object.new()" instead.)*", p_call);
			p_call->set_datatype(call_type);
			return;
		}

		Variant::Type builtin_type = GDScriptParser::get_builtin_type(function_name);
		if (builtin_type < Variant::VARIANT_MAX) {
			// Is a builtin constructor.
			call_type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
			call_type.kind = GDScriptParser::DataType::BUILTIN;
			call_type.builtin_type = builtin_type;

			bool safe_to_fold = true;
			switch (builtin_type) {
				// Those are stored by reference so not suited for compile-time construction.
				// Because in this case they would be the same reference in all constructed values.
				case Variant::OBJECT:
				case Variant::DICTIONARY:
				case Variant::ARRAY:
				case Variant::PACKED_BYTE_ARRAY:
				case Variant::PACKED_INT32_ARRAY:
				case Variant::PACKED_INT64_ARRAY:
				case Variant::PACKED_FLOAT32_ARRAY:
				case Variant::PACKED_FLOAT64_ARRAY:
				case Variant::PACKED_STRING_ARRAY:
				case Variant::PACKED_VECTOR2_ARRAY:
				case Variant::PACKED_VECTOR3_ARRAY:
				case Variant::PACKED_COLOR_ARRAY:
				case Variant::PACKED_VECTOR4_ARRAY:
					safe_to_fold = false;
					break;
				default:
					break;
			}

			if (all_is_constant && safe_to_fold) {
				// Construct here.
				Vector<const Variant *> args;
				for (int i = 0; i < p_call->arguments.size(); i++) {
					args.push_back(&(p_call->arguments[i]->reduced_value));
				}

				Callable::CallError err;
				Variant value;
				Variant::construct(builtin_type, value, (const Variant **)args.ptr(), args.size(), err);

				switch (err.error) {
					case Callable::CallError::CALL_ERROR_INVALID_ARGUMENT:
						push_error(vformat(R"*(Invalid argument for "%s()" constructor: argument %d should be "%s" but is "%s".)*", Variant::get_type_name(builtin_type), err.argument + 1,
										   Variant::get_type_name(Variant::Type(err.expected)), p_call->arguments[err.argument]->get_datatype().to_string()),
								p_call->arguments[err.argument]);
						break;
					case Callable::CallError::CALL_ERROR_INVALID_METHOD: {
						String signature = Variant::get_type_name(builtin_type) + "(";
						for (int i = 0; i < p_call->arguments.size(); i++) {
							if (i > 0) {
								signature += ", ";
							}
							signature += p_call->arguments[i]->get_datatype().to_string();
						}
						signature += ")";
						push_error(vformat(R"(No constructor of "%s" matches the signature "%s".)", Variant::get_type_name(builtin_type), signature), p_call->callee);
					} break;
					case Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS:
						push_error(vformat(R"*(Too many arguments for "%s()" constructor. Received %d but expected %d.)*", Variant::get_type_name(builtin_type), p_call->arguments.size(), err.expected), p_call);
						break;
					case Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS:
						push_error(vformat(R"*(Too few arguments for "%s()" constructor. Received %d but expected %d.)*", Variant::get_type_name(builtin_type), p_call->arguments.size(), err.expected), p_call);
						break;
					case Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL:
					case Callable::CallError::CALL_ERROR_METHOD_NOT_CONST:
						break; // Can't happen in a builtin constructor.
					case Callable::CallError::CALL_OK:
						p_call->is_constant = true;
						p_call->reduced_value = value;
						break;
				}
			} else {
				// If there's one argument, try to use copy constructor (those aren't explicitly defined).
				if (p_call->arguments.size() == 1) {
					GDScriptParser::DataType arg_type = p_call->arguments[0]->get_datatype();
					if (arg_type.is_hard_type() && !arg_type.is_variant()) {
						if (arg_type.kind == GDScriptParser::DataType::BUILTIN && arg_type.builtin_type == builtin_type) {
							// Okay.
							p_call->set_datatype(call_type);
							return;
						}
					} else {
#ifdef DEBUG_ENABLED
						mark_node_unsafe(p_call);
						// Constructors support overloads.
						Vector<String> types;
						for (int i = 0; i < Variant::VARIANT_MAX; i++) {
							if (i != builtin_type && Variant::can_convert_strict((Variant::Type)i, builtin_type)) {
								types.push_back(Variant::get_type_name((Variant::Type)i));
							}
						}
						String expected_types = function_name;
						if (types.size() == 1) {
							expected_types += "\" or \"" + types[0];
						} else if (types.size() >= 2) {
							for (int i = 0; i < types.size() - 1; i++) {
								expected_types += "\", \"" + types[i];
							}
							expected_types += "\", or \"" + types[types.size() - 1];
						}
						parser->push_warning(p_call->arguments[0], GDScriptWarning::UNSAFE_CALL_ARGUMENT, "1", "constructor", function_name, expected_types, "Variant");
#endif
						p_call->set_datatype(call_type);
						return;
					}
				}

				List<MethodInfo> constructors;
				Variant::get_constructor_list(builtin_type, &constructors);
				bool match = false;

				for (const MethodInfo &info : constructors) {
					if (p_call->arguments.size() < info.arguments.size() - info.default_arguments.size()) {
						continue;
					}
					if (p_call->arguments.size() > info.arguments.size()) {
						continue;
					}

					bool types_match = true;

					{
						List<PropertyInfo>::ConstIterator arg_itr = info.arguments.begin();
						for (int i = 0; i < p_call->arguments.size(); ++arg_itr, ++i) {
							GDScriptParser::DataType par_type = type_from_property(*arg_itr, true);
							GDScriptParser::DataType arg_type = p_call->arguments[i]->get_datatype();
							if (!is_type_compatible(par_type, arg_type, true)) {
								types_match = false;
								break;
#ifdef DEBUG_ENABLED
							} else {
								if (par_type.builtin_type == Variant::INT && arg_type.builtin_type == Variant::FLOAT && builtin_type != Variant::INT) {
									parser->push_warning(p_call, GDScriptWarning::NARROWING_CONVERSION, function_name);
								}
#endif
							}
						}
					}

					if (types_match) {
						List<PropertyInfo>::ConstIterator arg_itr = info.arguments.begin();
						for (int i = 0; i < p_call->arguments.size(); ++arg_itr, ++i) {
							GDScriptParser::DataType par_type = type_from_property(*arg_itr, true);
							if (p_call->arguments[i]->is_constant) {
								update_const_expression_builtin_type(p_call->arguments[i], par_type, "pass");
							}
#ifdef DEBUG_ENABLED
							if (!(par_type.is_variant() && par_type.is_hard_type())) {
								GDScriptParser::DataType arg_type = p_call->arguments[i]->get_datatype();
								if (arg_type.is_variant() || !arg_type.is_hard_type()) {
									mark_node_unsafe(p_call);
									parser->push_warning(p_call->arguments[i], GDScriptWarning::UNSAFE_CALL_ARGUMENT, itos(i + 1), "constructor", function_name, par_type.to_string(), arg_type.to_string_strict());
								}
							}
#endif
						}
						match = true;
						call_type = type_from_property(info.return_val);
						break;
					}
				}

				if (!match) {
					String signature = Variant::get_type_name(builtin_type) + "(";
					for (int i = 0; i < p_call->arguments.size(); i++) {
						if (i > 0) {
							signature += ", ";
						}
						signature += p_call->arguments[i]->get_datatype().to_string();
					}
					signature += ")";
					push_error(vformat(R"(No constructor of "%s" matches the signature "%s".)", Variant::get_type_name(builtin_type), signature), p_call);
				}
			}

#ifdef DEBUG_ENABLED
			// Consider `Signal(self, "my_signal")` as an implicit use of the signal.
			if (builtin_type == Variant::SIGNAL && p_call->arguments.size() >= 2) {
				const GDScriptParser::ExpressionNode *object_arg = p_call->arguments[0];
				if (object_arg && object_arg->type == GDScriptParser::Node::SELF) {
					const GDScriptParser::ExpressionNode *signal_arg = p_call->arguments[1];
					if (signal_arg && signal_arg->is_constant) {
						const StringName &signal_name = signal_arg->reduced_value;
						if (parser->current_class->has_member(signal_name)) {
							const GDScriptParser::ClassNode::Member &member = parser->current_class->get_member(signal_name);
							if (member.type == GDScriptParser::ClassNode::Member::SIGNAL) {
								member.signal->usages++;
							}
						}
					}
				}
			}
#endif

			p_call->set_datatype(call_type);
			return;
		} else if (GDScriptUtilityFunctions::function_exists(function_name)) {
			MethodInfo function_info = GDScriptUtilityFunctions::get_function_info(function_name);

			if (!p_is_root && !p_is_await && function_info.return_val.type == Variant::NIL && ((function_info.return_val.usage & PROPERTY_USAGE_NIL_IS_VARIANT) == 0)) {
				push_error(vformat(R"*(Cannot get return value of call to "%s()" because it returns "void".)*", function_name), p_call);
			}

			if (all_is_constant && GDScriptUtilityFunctions::is_function_constant(function_name)) {
				// Can call on compilation.
				Vector<const Variant *> args;
				for (int i = 0; i < p_call->arguments.size(); i++) {
					args.push_back(&(p_call->arguments[i]->reduced_value));
				}

				Variant value;
				Callable::CallError err;
				GDScriptUtilityFunctions::get_function(function_name)(&value, (const Variant **)args.ptr(), args.size(), err);

				switch (err.error) {
					case Callable::CallError::CALL_ERROR_INVALID_ARGUMENT:
						if (value.get_type() == Variant::STRING && !value.operator String().is_empty()) {
							push_error(vformat(R"*(Invalid argument for "%s()" function: %s)*", function_name, value), p_call->arguments[err.argument]);
						} else {
							// Do not use `type_from_property()` for expected type, since utility functions use their own checks.
							push_error(vformat(R"*(Invalid argument for "%s()" function: argument %d should be "%s" but is "%s".)*", function_name, err.argument + 1,
											   Variant::get_type_name((Variant::Type)err.expected), p_call->arguments[err.argument]->get_datatype().to_string()),
									p_call->arguments[err.argument]);
						}
						break;
					case Callable::CallError::CALL_ERROR_INVALID_METHOD:
						push_error(vformat(R"(Invalid call for function "%s".)", function_name), p_call);
						break;
					case Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS:
						push_error(vformat(R"*(Too many arguments for "%s()" call. Expected at most %d but received %d.)*", function_name, err.expected, p_call->arguments.size()), p_call);
						break;
					case Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS:
						push_error(vformat(R"*(Too few arguments for "%s()" call. Expected at least %d but received %d.)*", function_name, err.expected, p_call->arguments.size()), p_call);
						break;
					case Callable::CallError::CALL_ERROR_METHOD_NOT_CONST:
					case Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL:
						break; // Can't happen in a builtin constructor.
					case Callable::CallError::CALL_OK:
						p_call->is_constant = true;
						p_call->reduced_value = value;
						break;
				}
			} else {
				validate_call_arg(function_info, p_call);
			}
			p_call->set_datatype(type_from_property(function_info.return_val));
			return;
		} else if (Variant::has_utility_function(function_name)) {
			MethodInfo function_info = info_from_utility_func(function_name);

			if (!p_is_root && !p_is_await && function_info.return_val.type == Variant::NIL && ((function_info.return_val.usage & PROPERTY_USAGE_NIL_IS_VARIANT) == 0)) {
				push_error(vformat(R"*(Cannot get return value of call to "%s()" because it returns "void".)*", function_name), p_call);
			}

			if (all_is_constant && Variant::get_utility_function_type(function_name) == Variant::UTILITY_FUNC_TYPE_MATH) {
				// Can call on compilation.
				Vector<const Variant *> args;
				for (int i = 0; i < p_call->arguments.size(); i++) {
					args.push_back(&(p_call->arguments[i]->reduced_value));
				}

				Variant value;
				Callable::CallError err;
				Variant::call_utility_function(function_name, &value, (const Variant **)args.ptr(), args.size(), err);

				switch (err.error) {
					case Callable::CallError::CALL_ERROR_INVALID_ARGUMENT:
						if (value.get_type() == Variant::STRING && !value.operator String().is_empty()) {
							push_error(vformat(R"*(Invalid argument for "%s()" function: %s)*", function_name, value), p_call->arguments[err.argument]);
						} else {
							// Do not use `type_from_property()` for expected type, since utility functions use their own checks.
							push_error(vformat(R"*(Invalid argument for "%s()" function: argument %d should be "%s" but is "%s".)*", function_name, err.argument + 1,
											   Variant::get_type_name((Variant::Type)err.expected), p_call->arguments[err.argument]->get_datatype().to_string()),
									p_call->arguments[err.argument]);
						}
						break;
					case Callable::CallError::CALL_ERROR_INVALID_METHOD:
						push_error(vformat(R"(Invalid call for function "%s".)", function_name), p_call);
						break;
					case Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS:
						push_error(vformat(R"*(Too many arguments for "%s()" call. Expected at most %d but received %d.)*", function_name, err.expected, p_call->arguments.size()), p_call);
						break;
					case Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS:
						push_error(vformat(R"*(Too few arguments for "%s()" call. Expected at least %d but received %d.)*", function_name, err.expected, p_call->arguments.size()), p_call);
						break;
					case Callable::CallError::CALL_ERROR_METHOD_NOT_CONST:
					case Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL:
						break; // Can't happen in a builtin constructor.
					case Callable::CallError::CALL_OK:
						p_call->is_constant = true;
						p_call->reduced_value = value;
						break;
				}
			} else {
				validate_call_arg(function_info, p_call);
			}
			p_call->set_datatype(type_from_property(function_info.return_val));
			return;
		}
	}

	GDScriptParser::DataType base_type;
	call_type.kind = GDScriptParser::DataType::VARIANT;
	bool is_self = false;

	if (p_call->is_super) {
		base_type = parser->current_class->base_type;
		base_type.is_meta_type = false;
		is_self = true;

		if (p_call->callee == nullptr && current_lambda != nullptr) {
			push_error("Cannot use `super()` inside a lambda.", p_call);
		}
	} else if (callee_type == GDScriptParser::Node::IDENTIFIER) {
		base_type = parser->current_class->get_datatype();
		base_type.is_meta_type = false;
		is_self = true;
	} else if (callee_type == GDScriptParser::Node::SUBSCRIPT) {
		GDScriptParser::SubscriptNode *subscript = static_cast<GDScriptParser::SubscriptNode *>(p_call->callee);
		if (subscript->base == nullptr) {
			// Invalid syntax, error already set on parser.
			p_call->set_datatype(call_type);
			mark_node_unsafe(p_call);
			return;
		}
		if (!subscript->is_attribute) {
			// Invalid call. Error already sent in parser.
			// TODO: Could check if Callable here.
			p_call->set_datatype(call_type);
			mark_node_unsafe(p_call);
			return;
		}
		if (subscript->attribute == nullptr) {
			// Invalid call. Error already sent in parser.
			p_call->set_datatype(call_type);
			mark_node_unsafe(p_call);
			return;
		}

		GDScriptParser::IdentifierNode *base_id = nullptr;
		if (subscript->base->type == GDScriptParser::Node::IDENTIFIER) {
			base_id = static_cast<GDScriptParser::IdentifierNode *>(subscript->base);
		}
		if (base_id && GDScriptParser::get_builtin_type(base_id->name) < Variant::VARIANT_MAX) {
			base_type = make_builtin_meta_type(GDScriptParser::get_builtin_type(base_id->name));
		} else {
			reduce_expression(subscript->base);
			base_type = subscript->base->get_datatype();
			is_self = subscript->base->type == GDScriptParser::Node::SELF;
		}
	} else {
		// Invalid call. Error already sent in parser.
		// TODO: Could check if Callable here too.
		p_call->set_datatype(call_type);
		mark_node_unsafe(p_call);
		return;
	}

	int default_arg_count = 0;
	BitField<MethodFlags> method_flags;
	GDScriptParser::DataType return_type;
	List<GDScriptParser::DataType> par_types;

	bool is_constructor = (base_type.is_meta_type || (p_call->callee && p_call->callee->type == GDScriptParser::Node::IDENTIFIER)) && p_call->function_name == SNAME("new");

	if (is_constructor && Engine::get_singleton()->has_singleton(base_type.native_type)) {
		push_error(vformat(R"(Cannot construct native class "%s" because it is an engine singleton.)", base_type.native_type), p_call);
		p_call->set_datatype(call_type);
		return;
	}

	if (get_function_signature(p_call, is_constructor, base_type, p_call->function_name, return_type, par_types, default_arg_count, method_flags)) {
		// If the method is implemented in the class hierarchy, the virtual flag will not be set for that MethodInfo and the search stops there.
		// Virtual check only possible for super() calls because class hierarchy is known. Node/Objects may have scripts attached we don't know of at compile-time.
		p_call->is_static = method_flags.has_flag(METHOD_FLAG_STATIC);
		if (p_call->is_super && method_flags.has_flag(METHOD_FLAG_VIRTUAL)) {
			push_error(vformat(R"*(Cannot call the parent class' virtual function "%s()" because it hasn't been defined.)*", p_call->function_name), p_call);
		}

		// If the function requires typed arrays we must make literals be typed.
		for (const KeyValue<int, GDScriptParser::ArrayNode *> &E : arrays) {
			int index = E.key;
			if (index < par_types.size() && par_types.get(index).is_hard_type() && par_types.get(index).has_container_element_type(0)) {
				update_array_literal_element_type(E.value, par_types.get(index).get_container_element_type(0));
			}
		}
		for (const KeyValue<int, GDScriptParser::DictionaryNode *> &E : dictionaries) {
			int index = E.key;
			if (index < par_types.size() && par_types.get(index).is_hard_type() && par_types.get(index).has_container_element_types()) {
				GDScriptParser::DataType key = par_types.get(index).get_container_element_type_or_variant(0);
				GDScriptParser::DataType value = par_types.get(index).get_container_element_type_or_variant(1);
				update_dictionary_literal_element_type(E.value, key, value);
			}
		}
		validate_call_arg(par_types, default_arg_count, method_flags.has_flag(METHOD_FLAG_VARARG), p_call);

		if (base_type.kind == GDScriptParser::DataType::ENUM && base_type.is_meta_type) {
			// Enum type is treated as a dictionary value for function calls.
			base_type.is_meta_type = false;
		}

		if (is_self && static_context && !p_call->is_static) {
			// Get the parent function above any lambda.
			GDScriptParser::FunctionNode *parent_function = parser->current_function;
			while (parent_function && parent_function->source_lambda) {
				parent_function = parent_function->source_lambda->parent_function;
			}

			if (parent_function) {
				push_error(vformat(R"*(Cannot call non-static function "%s()" from the static function "%s()".)*", p_call->function_name, parent_function->identifier->name), p_call);
			} else {
				push_error(vformat(R"*(Cannot call non-static function "%s()" from a static variable initializer.)*", p_call->function_name), p_call);
			}
		} else if (!is_self && base_type.is_meta_type && !p_call->is_static) {
			base_type.is_meta_type = false; // For `to_string()`.
			push_error(vformat(R"*(Cannot call non-static function "%s()" on the class "%s" directly. Make an instance instead.)*", p_call->function_name, base_type.to_string()), p_call);
		} else if (is_self && !p_call->is_static) {
			mark_lambda_use_self();
		}

		if (!p_is_root && !p_is_await && return_type.is_hard_type() && return_type.kind == GDScriptParser::DataType::BUILTIN && return_type.builtin_type == Variant::NIL) {
			push_error(vformat(R"*(Cannot get return value of call to "%s()" because it returns "void".)*", p_call->function_name), p_call);
		}

#ifdef DEBUG_ENABLED
		// FIXME: No warning for built-in constructors and utilities due to early return.
		if (p_is_root && return_type.kind != GDScriptParser::DataType::UNRESOLVED && return_type.builtin_type != Variant::NIL &&
				!(p_call->is_super && p_call->function_name == GDScriptLanguage::get_singleton()->strings._init)) {
			parser->push_warning(p_call, GDScriptWarning::RETURN_VALUE_DISCARDED, p_call->function_name);
		}

		if (method_flags.has_flag(METHOD_FLAG_STATIC) && !is_constructor && !base_type.is_meta_type && !is_self) {
			String caller_type = base_type.to_string();

			parser->push_warning(p_call, GDScriptWarning::STATIC_CALLED_ON_INSTANCE, p_call->function_name, caller_type);
		}

		// Consider `emit_signal()`, `connect()`, and `disconnect()` as implicit uses of the signal.
		if (is_self && (p_call->function_name == SNAME("emit_signal") || p_call->function_name == SNAME("connect") || p_call->function_name == SNAME("disconnect")) && !p_call->arguments.is_empty()) {
			const GDScriptParser::ExpressionNode *signal_arg = p_call->arguments[0];
			if (signal_arg && signal_arg->is_constant) {
				const StringName &signal_name = signal_arg->reduced_value;
				if (parser->current_class->has_member(signal_name)) {
					const GDScriptParser::ClassNode::Member &member = parser->current_class->get_member(signal_name);
					if (member.type == GDScriptParser::ClassNode::Member::SIGNAL) {
						member.signal->usages++;
					}
				}
			}
		}
#endif // DEBUG_ENABLED

		call_type = return_type;
	} else {
		bool found = false;

		// Enums do not have functions other than the built-in dictionary ones.
		if (base_type.kind == GDScriptParser::DataType::ENUM && base_type.is_meta_type) {
			if (base_type.builtin_type == Variant::DICTIONARY) {
				push_error(vformat(R"*(Enums only have Dictionary built-in methods. Function "%s()" does not exist for enum "%s".)*", p_call->function_name, base_type.enum_type), p_call->callee);
			} else {
				push_error(vformat(R"*(The native enum "%s" does not behave like Dictionary and does not have methods of its own.)*", base_type.enum_type), p_call->callee);
			}
		} else if (!p_call->is_super && callee_type != GDScriptParser::Node::NONE) { // Check if the name exists as something else.
			GDScriptParser::IdentifierNode *callee_id;
			if (callee_type == GDScriptParser::Node::IDENTIFIER) {
				callee_id = static_cast<GDScriptParser::IdentifierNode *>(p_call->callee);
			} else {
				// Can only be attribute.
				callee_id = static_cast<GDScriptParser::SubscriptNode *>(p_call->callee)->attribute;
			}
			if (callee_id) {
				reduce_identifier_from_base(callee_id, &base_type);
				GDScriptParser::DataType callee_datatype = callee_id->get_datatype();
				if (callee_datatype.is_set() && !callee_datatype.is_variant()) {
					found = true;
					if (callee_datatype.builtin_type == Variant::CALLABLE) {
						push_error(vformat(R"*(Name "%s" is a Callable. You can call it with "%s.call()" instead.)*", p_call->function_name, p_call->function_name), p_call->callee);
					} else {
						push_error(vformat(R"*(Name "%s" called as a function but is a "%s".)*", p_call->function_name, callee_datatype.to_string()), p_call->callee);
					}
#ifdef DEBUG_ENABLED
				} else if (!is_self && !(base_type.is_hard_type() && base_type.kind == GDScriptParser::DataType::BUILTIN)) {
					parser->push_warning(p_call, GDScriptWarning::UNSAFE_METHOD_ACCESS, p_call->function_name, base_type.to_string());
					mark_node_unsafe(p_call);
#endif
				}
			}
		}
		if (!found && (is_self || (base_type.is_hard_type() && base_type.kind == GDScriptParser::DataType::BUILTIN))) {
			String base_name = is_self && !p_call->is_super ? "self" : base_type.to_string();
#ifdef SUGGEST_GODOT4_RENAMES
			String rename_hint;
			if (GLOBAL_GET(GDScriptWarning::get_settings_path_from_code(GDScriptWarning::RENAMED_IN_GODOT_4_HINT)).booleanize()) {
				const char *renamed_function_name = check_for_renamed_identifier(p_call->function_name, p_call->type);
				if (renamed_function_name) {
					rename_hint = " " + vformat(R"(Did you mean to use "%s"?)", String(renamed_function_name) + "()");
				}
			}
			push_error(vformat(R"*(Function "%s()" not found in base %s.%s)*", p_call->function_name, base_name, rename_hint), p_call->is_super ? p_call : p_call->callee);
#else
			push_error(vformat(R"*(Function "%s()" not found in base %s.)*", p_call->function_name, base_name), p_call->is_super ? p_call : p_call->callee);
#endif // SUGGEST_GODOT4_RENAMES
		} else if (!found && (!p_call->is_super && base_type.is_hard_type() && base_type.is_meta_type)) {
			push_error(vformat(R"*(Static function "%s()" not found in base "%s".)*", p_call->function_name, base_type.to_string()), p_call);
		}
	}

	if (call_type.is_coroutine && !p_is_await && !p_is_root) {
		push_error(vformat(R"*(Function "%s()" is a coroutine, so it must be called with "await".)*", p_call->function_name), p_call);
	}

	p_call->set_datatype(call_type);
}

void GDScriptAnalyzer::reduce_cast(GDScriptParser::CastNode *p_cast) {
	reduce_expression(p_cast->operand);

	GDScriptParser::DataType cast_type = type_from_metatype(resolve_datatype(p_cast->cast_type));

	if (!cast_type.is_set()) {
		mark_node_unsafe(p_cast);
		return;
	}

	p_cast->set_datatype(cast_type);
	if (p_cast->operand->is_constant) {
		update_const_expression_builtin_type(p_cast->operand, cast_type, "cast", true);
		if (cast_type.is_variant() || p_cast->operand->get_datatype() == cast_type) {
			p_cast->is_constant = true;
			p_cast->reduced_value = p_cast->operand->reduced_value;
		}
	}

	if (p_cast->operand->type == GDScriptParser::Node::ARRAY && cast_type.has_container_element_type(0)) {
		update_array_literal_element_type(static_cast<GDScriptParser::ArrayNode *>(p_cast->operand), cast_type.get_container_element_type(0));
	}

	if (p_cast->operand->type == GDScriptParser::Node::DICTIONARY && cast_type.has_container_element_types()) {
		update_dictionary_literal_element_type(static_cast<GDScriptParser::DictionaryNode *>(p_cast->operand),
				cast_type.get_container_element_type_or_variant(0), cast_type.get_container_element_type_or_variant(1));
	}

	if (!cast_type.is_variant()) {
		GDScriptParser::DataType op_type = p_cast->operand->get_datatype();
		if (op_type.is_variant() || !op_type.is_hard_type()) {
			mark_node_unsafe(p_cast);
#ifdef DEBUG_ENABLED
			parser->push_warning(p_cast, GDScriptWarning::UNSAFE_CAST, cast_type.to_string());
#endif
		} else {
			bool valid = false;
			if (op_type.builtin_type == Variant::INT && cast_type.kind == GDScriptParser::DataType::ENUM) {
				mark_node_unsafe(p_cast);
				valid = true;
			} else if (op_type.kind == GDScriptParser::DataType::ENUM && cast_type.builtin_type == Variant::INT) {
				valid = true;
			} else if (op_type.kind == GDScriptParser::DataType::BUILTIN && cast_type.kind == GDScriptParser::DataType::BUILTIN) {
				valid = Variant::can_convert(op_type.builtin_type, cast_type.builtin_type);
			} else if (op_type.kind != GDScriptParser::DataType::BUILTIN && cast_type.kind != GDScriptParser::DataType::BUILTIN) {
				valid = is_type_compatible(cast_type, op_type) || is_type_compatible(op_type, cast_type);
			}

			if (!valid) {
				push_error(vformat(R"(Invalid cast. Cannot convert from "%s" to "%s".)", op_type.to_string(), cast_type.to_string()), p_cast->cast_type);
			}
		}
	}
}

void GDScriptAnalyzer::reduce_dictionary(GDScriptParser::DictionaryNode *p_dictionary) {
	HashMap<Variant, GDScriptParser::ExpressionNode *, VariantHasher, StringLikeVariantComparator> elements;

	for (int i = 0; i < p_dictionary->elements.size(); i++) {
		const GDScriptParser::DictionaryNode::Pair &element = p_dictionary->elements[i];
		if (p_dictionary->style == GDScriptParser::DictionaryNode::PYTHON_DICT) {
			reduce_expression(element.key);
		}
		reduce_expression(element.value);

		if (element.key->is_constant) {
			if (elements.has(element.key->reduced_value)) {
				push_error(vformat(R"(Key "%s" was already used in this dictionary (at line %d).)", element.key->reduced_value, elements[element.key->reduced_value]->start_line), element.key);
			} else {
				elements[element.key->reduced_value] = element.value;
			}
		}
	}

	// It's dictionary in any case.
	GDScriptParser::DataType dict_type;
	dict_type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	dict_type.kind = GDScriptParser::DataType::BUILTIN;
	dict_type.builtin_type = Variant::DICTIONARY;
	dict_type.is_constant = true;

	p_dictionary->set_datatype(dict_type);
}

void GDScriptAnalyzer::reduce_get_node(GDScriptParser::GetNodeNode *p_get_node) {
	GDScriptParser::DataType result;
	result.kind = GDScriptParser::DataType::VARIANT;

	if (!ClassDB::is_parent_class(parser->current_class->base_type.native_type, SNAME("Node"))) {
		push_error(vformat(R"*(Cannot use shorthand "get_node()" notation ("%c") on a class that isn't a node.)*", p_get_node->use_dollar ? '$' : '%'), p_get_node);
		p_get_node->set_datatype(result);
		return;
	}

	if (static_context) {
		push_error(vformat(R"*(Cannot use shorthand "get_node()" notation ("%c") in a static function.)*", p_get_node->use_dollar ? '$' : '%'), p_get_node);
		p_get_node->set_datatype(result);
		return;
	}

	mark_lambda_use_self();

	result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	result.kind = GDScriptParser::DataType::NATIVE;
	result.builtin_type = Variant::OBJECT;
	result.native_type = SNAME("Node");
	p_get_node->set_datatype(result);
}

GDScriptParser::DataType GDScriptAnalyzer::make_global_class_meta_type(const StringName &p_class_name, const GDScriptParser::Node *p_source) {
	GDScriptParser::DataType type;

	String path = ScriptServer::get_global_class_path(p_class_name);
	String ext = path.get_extension();
	if (ext == GDScriptLanguage::get_singleton()->get_extension()) {
		Ref<GDScriptParserRef> ref = parser->get_depended_parser_for(path);
		if (ref.is_null()) {
			push_error(vformat(R"(Could not find script for class "%s".)", p_class_name), p_source);
			type.type_source = GDScriptParser::DataType::UNDETECTED;
			type.kind = GDScriptParser::DataType::VARIANT;
			return type;
		}

		Error err = ref->raise_status(GDScriptParserRef::INHERITANCE_SOLVED);
		if (err) {
			push_error(vformat(R"(Could not resolve class "%s", because of a parser error.)", p_class_name), p_source);
			type.type_source = GDScriptParser::DataType::UNDETECTED;
			type.kind = GDScriptParser::DataType::VARIANT;
			return type;
		}

		return ref->get_parser()->head->get_datatype();
	} else {
		return make_script_meta_type(ResourceLoader::load(path, "Script"));
	}
}

Ref<GDScriptParserRef> GDScriptAnalyzer::ensure_cached_external_parser_for_class(const GDScriptParser::ClassNode *p_class, const GDScriptParser::ClassNode *p_from_class, const char *p_context, const GDScriptParser::Node *p_source) {
	// Delicate piece of code that intentionally doesn't use the GDScript cache or `get_depended_parser_for`.
	// Search dependencies for the parser that owns `p_class` and make a cache entry for it.
	// Required for how we store pointers to classes owned by other parser trees and need to call `resolve_class_member` and such on the same parser tree.
	// Since https://github.com/godotengine/godot/pull/94871 there can technically be multiple parsers for the same script in the same parser tree.
	// Even if unlikely, getting the wrong parser could lead to strange undefined behavior without errors.

	if (p_class == nullptr) {
		return nullptr;
	}

	if (HashMap<const GDScriptParser::ClassNode *, Ref<GDScriptParserRef>>::Iterator E = external_class_parser_cache.find(p_class)) {
		return E->value;
	}

	if (parser->has_class(p_class)) {
		return nullptr;
	}

	if (p_from_class == nullptr) {
		p_from_class = parser->head;
	}

	Ref<GDScriptParserRef> parser_ref;
	for (const GDScriptParser::ClassNode *look_class = p_from_class; look_class != nullptr; look_class = look_class->base_type.class_type) {
		if (parser->has_class(look_class)) {
			parser_ref = find_cached_external_parser_for_class(p_class, parser);
			if (parser_ref.is_valid()) {
				break;
			}
		}

		if (HashMap<const GDScriptParser::ClassNode *, Ref<GDScriptParserRef>>::Iterator E = external_class_parser_cache.find(look_class)) {
			parser_ref = find_cached_external_parser_for_class(p_class, E->value);
			if (parser_ref.is_valid()) {
				break;
			}
		}

		String look_class_script_path = look_class->get_datatype().script_path;
		if (HashMap<String, Ref<GDScriptParserRef>>::Iterator E = parser->depended_parsers.find(look_class_script_path)) {
			parser_ref = find_cached_external_parser_for_class(p_class, E->value);
			if (parser_ref.is_valid()) {
				break;
			}
		}
	}

	if (parser_ref.is_null()) {
		push_error(vformat(R"(Parser bug (please report): Could not find external parser for class "%s". (%s))", p_class->fqcn, p_context), p_source);
		// A null parser will be inserted into the cache, so this error won't spam for the same class.
		// This is ok, the values of external_class_parser_cache are not assumed to be valid references.
	}

	external_class_parser_cache.insert(p_class, parser_ref);
	return parser_ref;
}

Ref<GDScriptParserRef> GDScriptAnalyzer::find_cached_external_parser_for_class(const GDScriptParser::ClassNode *p_class, const Ref<GDScriptParserRef> &p_dependant_parser) {
	if (p_dependant_parser.is_null()) {
		return nullptr;
	}

	if (HashMap<const GDScriptParser::ClassNode *, Ref<GDScriptParserRef>>::Iterator E = p_dependant_parser->get_analyzer()->external_class_parser_cache.find(p_class)) {
		if (E->value.is_valid()) {
			// Silently ensure it's parsed.
			E->value->raise_status(GDScriptParserRef::PARSED);
			if (E->value->get_parser()->has_class(p_class)) {
				return E->value;
			}
		}
	}

	if (p_dependant_parser->get_parser()->has_class(p_class)) {
		return p_dependant_parser;
	}

	// Silently ensure it's parsed.
	p_dependant_parser->raise_status(GDScriptParserRef::PARSED);
	return find_cached_external_parser_for_class(p_class, p_dependant_parser->get_parser());
}

Ref<GDScriptParserRef> GDScriptAnalyzer::find_cached_external_parser_for_class(const GDScriptParser::ClassNode *p_class, GDScriptParser *p_dependant_parser) {
	if (p_dependant_parser == nullptr) {
		return nullptr;
	}

	String script_path = p_class->get_datatype().script_path;
	if (HashMap<String, Ref<GDScriptParserRef>>::Iterator E = p_dependant_parser->depended_parsers.find(script_path)) {
		if (E->value.is_valid()) {
			// Silently ensure it's parsed.
			E->value->raise_status(GDScriptParserRef::PARSED);
			if (E->value->get_parser()->has_class(p_class)) {
				return E->value;
			}
		}
	}

	return nullptr;
}

Ref<GDScript> GDScriptAnalyzer::get_depended_shallow_script(const String &p_path, Error &r_error) {
	// To keep a local cache of the parser for resolving external nodes later.
	parser->get_depended_parser_for(p_path);
	Ref<GDScript> scr = GDScriptCache::get_shallow_script(p_path, r_error, parser->script_path);
	return scr;
}

void GDScriptAnalyzer::reduce_identifier_from_base_set_class(GDScriptParser::IdentifierNode *p_identifier, GDScriptParser::DataType p_identifier_datatype) {
	ERR_FAIL_NULL(p_identifier);

	p_identifier->set_datatype(p_identifier_datatype);
	Error err = OK;
	Ref<GDScript> scr = get_depended_shallow_script(p_identifier_datatype.script_path, err);
	if (err) {
		push_error(vformat(R"(Error while getting cache for script "%s".)", p_identifier_datatype.script_path), p_identifier);
		return;
	}
	p_identifier->reduced_value = scr->find_class(p_identifier_datatype.class_type->fqcn);
	p_identifier->is_constant = true;
}

void GDScriptAnalyzer::reduce_identifier_from_base(GDScriptParser::IdentifierNode *p_identifier, GDScriptParser::DataType *p_base) {
	if (!p_identifier->get_datatype().has_no_type()) {
		return;
	}

	GDScriptParser::DataType base;
	if (p_base == nullptr) {
		base = type_from_metatype(parser->current_class->get_datatype());
	} else {
		base = *p_base;
	}

	StringName name = p_identifier->name;

	if (base.kind == GDScriptParser::DataType::ENUM) {
		if (base.is_meta_type) {
			if (base.enum_values.has(name)) {
				p_identifier->set_datatype(type_from_metatype(base));
				p_identifier->is_constant = true;
				p_identifier->reduced_value = base.enum_values[name];
				return;
			}

			// Enum does not have this value, return.
			return;
		} else {
			push_error(R"(Cannot get property from enum value.)", p_identifier);
			return;
		}
	}

	if (base.kind == GDScriptParser::DataType::BUILTIN) {
		if (base.is_meta_type) {
			bool valid = false;

			if (Variant::has_constant(base.builtin_type, name)) {
				valid = true;

				const Variant constant_value = Variant::get_constant_value(base.builtin_type, name);

				p_identifier->is_constant = true;
				p_identifier->reduced_value = constant_value;
				p_identifier->set_datatype(type_from_variant(constant_value, p_identifier));
			}

			if (!valid) {
				const StringName enum_name = Variant::get_enum_for_enumeration(base.builtin_type, name);
				if (enum_name != StringName()) {
					valid = true;

					p_identifier->is_constant = true;
					p_identifier->reduced_value = Variant::get_enum_value(base.builtin_type, enum_name, name);
					p_identifier->set_datatype(make_builtin_enum_type(enum_name, base.builtin_type, false));
				}
			}

			if (!valid && Variant::has_enum(base.builtin_type, name)) {
				valid = true;

				p_identifier->set_datatype(make_builtin_enum_type(name, base.builtin_type, true));
			}

			if (!valid && base.is_hard_type()) {
#ifdef SUGGEST_GODOT4_RENAMES
				String rename_hint;
				if (GLOBAL_GET(GDScriptWarning::get_settings_path_from_code(GDScriptWarning::RENAMED_IN_GODOT_4_HINT)).booleanize()) {
					const char *renamed_identifier_name = check_for_renamed_identifier(name, p_identifier->type);
					if (renamed_identifier_name) {
						rename_hint = " " + vformat(R"(Did you mean to use "%s"?)", renamed_identifier_name);
					}
				}
				push_error(vformat(R"(Cannot find member "%s" in base "%s".%s)", name, base.to_string(), rename_hint), p_identifier);
#else
				push_error(vformat(R"(Cannot find member "%s" in base "%s".)", name, base.to_string()), p_identifier);
#endif // SUGGEST_GODOT4_RENAMES
			}
		} else {
			switch (base.builtin_type) {
				case Variant::NIL: {
					if (base.is_hard_type()) {
						push_error(vformat(R"(Cannot get property "%s" on a null object.)", name), p_identifier);
					}
					return;
				}
				case Variant::DICTIONARY: {
					GDScriptParser::DataType dummy;
					dummy.kind = GDScriptParser::DataType::VARIANT;
					p_identifier->set_datatype(dummy);
					return;
				}
				default: {
					Callable::CallError temp;
					Variant dummy;
					Variant::construct(base.builtin_type, dummy, nullptr, 0, temp);
					List<PropertyInfo> properties;
					dummy.get_property_list(&properties);
					for (const PropertyInfo &prop : properties) {
						if (prop.name == name) {
							p_identifier->set_datatype(type_from_property(prop));
							return;
						}
					}
					if (Variant::has_builtin_method(base.builtin_type, name)) {
						p_identifier->set_datatype(make_callable_type(Variant::get_builtin_method_info(base.builtin_type, name)));
						return;
					}
					if (base.is_hard_type()) {
#ifdef SUGGEST_GODOT4_RENAMES
						String rename_hint;
						if (GLOBAL_GET(GDScriptWarning::get_settings_path_from_code(GDScriptWarning::RENAMED_IN_GODOT_4_HINT)).booleanize()) {
							const char *renamed_identifier_name = check_for_renamed_identifier(name, p_identifier->type);
							if (renamed_identifier_name) {
								rename_hint = " " + vformat(R"(Did you mean to use "%s"?)", renamed_identifier_name);
							}
						}
						push_error(vformat(R"(Cannot find member "%s" in base "%s".%s)", name, base.to_string(), rename_hint), p_identifier);
#else
						push_error(vformat(R"(Cannot find member "%s" in base "%s".)", name, base.to_string()), p_identifier);
#endif // SUGGEST_GODOT4_RENAMES
					}
				}
			}
		}
		return;
	}

	GDScriptParser::ClassNode *base_class = base.class_type;
	List<GDScriptParser::ClassNode *> script_classes;
	bool is_base = true;

	if (base_class != nullptr) {
		get_class_node_current_scope_classes(base_class, &script_classes, p_identifier);
	}

	bool is_constructor = base.is_meta_type && p_identifier->name == SNAME("new");

	for (GDScriptParser::ClassNode *script_class : script_classes) {
		if (p_base == nullptr && script_class->identifier && script_class->identifier->name == name) {
			reduce_identifier_from_base_set_class(p_identifier, script_class->get_datatype());
			if (script_class->outer != nullptr) {
				p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_CLASS;
			}
			return;
		}

		if (is_constructor) {
			name = "_init";
		}

		if (script_class->has_member(name)) {
			resolve_class_member(script_class, name, p_identifier);

			GDScriptParser::ClassNode::Member member = script_class->get_member(name);
			switch (member.type) {
				case GDScriptParser::ClassNode::Member::CONSTANT: {
					p_identifier->set_datatype(member.get_datatype());
					p_identifier->is_constant = true;
					p_identifier->reduced_value = member.constant->initializer->reduced_value;
					p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_CONSTANT;
					p_identifier->constant_source = member.constant;
					return;
				}

				case GDScriptParser::ClassNode::Member::ENUM_VALUE: {
					p_identifier->set_datatype(member.get_datatype());
					p_identifier->is_constant = true;
					p_identifier->reduced_value = member.enum_value.value;
					p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_CONSTANT;
					return;
				}

				case GDScriptParser::ClassNode::Member::ENUM: {
					p_identifier->set_datatype(member.get_datatype());
					p_identifier->is_constant = true;
					p_identifier->reduced_value = member.m_enum->dictionary;
					p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_CONSTANT;
					return;
				}

				case GDScriptParser::ClassNode::Member::VARIABLE: {
					if (is_base && (!base.is_meta_type || member.variable->is_static)) {
						p_identifier->set_datatype(member.get_datatype());
						p_identifier->source = member.variable->is_static ? GDScriptParser::IdentifierNode::STATIC_VARIABLE : GDScriptParser::IdentifierNode::MEMBER_VARIABLE;
						p_identifier->variable_source = member.variable;
						member.variable->usages += 1;
						return;
					}
				} break;

				case GDScriptParser::ClassNode::Member::SIGNAL: {
					if (is_base && !base.is_meta_type) {
						p_identifier->set_datatype(member.get_datatype());
						p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_SIGNAL;
						p_identifier->signal_source = member.signal;
						member.signal->usages += 1;
						return;
					}
				} break;

				case GDScriptParser::ClassNode::Member::FUNCTION: {
					if (is_base && (!base.is_meta_type || member.function->is_static || is_constructor)) {
						p_identifier->set_datatype(make_callable_type(member.function->info));
						p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_FUNCTION;
						p_identifier->function_source = member.function;
						p_identifier->function_source_is_static = member.function->is_static;
						return;
					}
				} break;

				case GDScriptParser::ClassNode::Member::CLASS: {
					reduce_identifier_from_base_set_class(p_identifier, member.get_datatype());
					p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_CLASS;
					return;
				}

				default: {
					// Do nothing
				}
			}
		}

		if (is_base) {
			is_base = script_class->base_type.class_type != nullptr;
			if (!is_base && p_base != nullptr) {
				break;
			}
		}
	}

	// Check non-GDScript scripts.
	Ref<Script> script_type = base.script_type;

	if (base_class == nullptr && script_type.is_valid()) {
		List<PropertyInfo> property_list;
		script_type->get_script_property_list(&property_list);

		for (const PropertyInfo &property_info : property_list) {
			if (property_info.name != p_identifier->name) {
				continue;
			}

			const GDScriptParser::DataType property_type = GDScriptAnalyzer::type_from_property(property_info, false, false);

			p_identifier->set_datatype(property_type);
			p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_VARIABLE;
			return;
		}

		MethodInfo method_info = script_type->get_method_info(p_identifier->name);

		if (method_info.name == p_identifier->name) {
			p_identifier->set_datatype(make_callable_type(method_info));
			p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_FUNCTION;
			p_identifier->function_source_is_static = method_info.flags & METHOD_FLAG_STATIC;
			return;
		}

		List<MethodInfo> signal_list;
		script_type->get_script_signal_list(&signal_list);

		for (const MethodInfo &signal_info : signal_list) {
			if (signal_info.name != p_identifier->name) {
				continue;
			}

			const GDScriptParser::DataType signal_type = make_signal_type(signal_info);

			p_identifier->set_datatype(signal_type);
			p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_SIGNAL;
			return;
		}

		HashMap<StringName, Variant> constant_map;
		script_type->get_constants(&constant_map);

		if (constant_map.has(p_identifier->name)) {
			Variant constant = constant_map.get(p_identifier->name);

			p_identifier->set_datatype(make_builtin_meta_type(constant.get_type()));
			p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_CONSTANT;
			return;
		}
	}

	// Check native members. No need for native class recursion because Node exposes all Object's properties.
	const StringName &native = base.native_type;

	if (class_exists(native)) {
		if (is_constructor) {
			name = "_init";
		}

		MethodInfo method_info;
		if (ClassDB::has_property(native, name)) {
			StringName getter_name = ClassDB::get_property_getter(native, name);
			MethodBind *getter = ClassDB::get_method(native, getter_name);
			if (getter != nullptr) {
				bool has_setter = ClassDB::get_property_setter(native, name) != StringName();
				p_identifier->set_datatype(type_from_property(getter->get_return_info(), false, !has_setter));
				p_identifier->source = GDScriptParser::IdentifierNode::INHERITED_VARIABLE;
			}
			return;
		}
		if (ClassDB::get_method_info(native, name, &method_info)) {
			// Method is callable.
			p_identifier->set_datatype(make_callable_type(method_info));
			p_identifier->source = GDScriptParser::IdentifierNode::INHERITED_VARIABLE;
			return;
		}
		if (ClassDB::get_signal(native, name, &method_info)) {
			// Signal is a type too.
			p_identifier->set_datatype(make_signal_type(method_info));
			p_identifier->source = GDScriptParser::IdentifierNode::INHERITED_VARIABLE;
			return;
		}
		if (ClassDB::has_enum(native, name)) {
			p_identifier->set_datatype(make_native_enum_type(name, native));
			p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_CONSTANT;
			return;
		}
		bool valid = false;

		int64_t int_constant = ClassDB::get_integer_constant(native, name, &valid);
		if (valid) {
			p_identifier->is_constant = true;
			p_identifier->reduced_value = int_constant;
			p_identifier->source = GDScriptParser::IdentifierNode::MEMBER_CONSTANT;

			// Check whether this constant, which exists, belongs to an enum
			StringName enum_name = ClassDB::get_integer_constant_enum(native, name);
			if (enum_name != StringName()) {
				p_identifier->set_datatype(make_native_enum_type(enum_name, native, false));
			} else {
				p_identifier->set_datatype(type_from_variant(int_constant, p_identifier));
			}
		}
	}
}

void GDScriptAnalyzer::reduce_identifier(GDScriptParser::IdentifierNode *p_identifier, bool can_be_builtin) {
	// TODO: This is an opportunity to further infer types.

	// Check if we are inside an enum. This allows enum values to access other elements of the same enum.
	if (current_enum) {
		for (int i = 0; i < current_enum->values.size(); i++) {
			const GDScriptParser::EnumNode::Value &element = current_enum->values[i];
			if (element.identifier->name == p_identifier->name) {
				StringName enum_name = current_enum->identifier ? current_enum->identifier->name : UNNAMED_ENUM;
				GDScriptParser::DataType type = make_class_enum_type(enum_name, parser->current_class, parser->script_path, false);
				if (element.parent_enum->identifier) {
					type.enum_type = element.parent_enum->identifier->name;
				}
				p_identifier->set_datatype(type);

				if (element.resolved) {
					p_identifier->is_constant = true;
					p_identifier->reduced_value = element.value;
				} else {
					push_error(R"(Cannot use another enum element before it was declared.)", p_identifier);
				}
				return; // Found anyway.
			}
		}
	}

	bool found_source = false;
	// Check if identifier is local.
	// If that's the case, the declaration already was solved before.
	switch (p_identifier->source) {
		case GDScriptParser::IdentifierNode::FUNCTION_PARAMETER:
			p_identifier->set_datatype(p_identifier->parameter_source->get_datatype());
			found_source = true;
			break;
		case GDScriptParser::IdentifierNode::LOCAL_CONSTANT:
		case GDScriptParser::IdentifierNode::MEMBER_CONSTANT:
			p_identifier->set_datatype(p_identifier->constant_source->get_datatype());
			p_identifier->is_constant = true;
			// TODO: Constant should have a value on the node itself.
			p_identifier->reduced_value = p_identifier->constant_source->initializer->reduced_value;
			found_source = true;
			break;
		case GDScriptParser::IdentifierNode::MEMBER_SIGNAL:
			p_identifier->signal_source->usages++;
			[[fallthrough]];
		case GDScriptParser::IdentifierNode::INHERITED_VARIABLE:
			mark_lambda_use_self();
			break;
		case GDScriptParser::IdentifierNode::MEMBER_VARIABLE:
			mark_lambda_use_self();
			p_identifier->variable_source->usages++;
			[[fallthrough]];
		case GDScriptParser::IdentifierNode::STATIC_VARIABLE:
		case GDScriptParser::IdentifierNode::LOCAL_VARIABLE:
			p_identifier->set_datatype(p_identifier->variable_source->get_datatype());
			found_source = true;
#ifdef DEBUG_ENABLED
			if (p_identifier->variable_source && p_identifier->variable_source->assignments == 0 && !(p_identifier->get_datatype().is_hard_type() && p_identifier->get_datatype().kind == GDScriptParser::DataType::BUILTIN)) {
				parser->push_warning(p_identifier, GDScriptWarning::UNASSIGNED_VARIABLE, p_identifier->name);
			}
#endif
			break;
		case GDScriptParser::IdentifierNode::LOCAL_ITERATOR:
			p_identifier->set_datatype(p_identifier->bind_source->get_datatype());
			found_source = true;
			break;
		case GDScriptParser::IdentifierNode::LOCAL_BIND: {
			GDScriptParser::DataType result = p_identifier->bind_source->get_datatype();
			result.is_constant = true;
			p_identifier->set_datatype(result);
			found_source = true;
		} break;
		case GDScriptParser::IdentifierNode::UNDEFINED_SOURCE:
		case GDScriptParser::IdentifierNode::MEMBER_FUNCTION:
		case GDScriptParser::IdentifierNode::MEMBER_CLASS:
			break;
	}

#ifdef DEBUG_ENABLED
	if (!found_source && p_identifier->suite != nullptr && p_identifier->suite->has_local(p_identifier->name)) {
		parser->push_warning(p_identifier, GDScriptWarning::CONFUSABLE_LOCAL_USAGE, p_identifier->name);
	}
#endif

	// Not a local, so check members.

	if (!found_source) {
		reduce_identifier_from_base(p_identifier);
		if (p_identifier->source != GDScriptParser::IdentifierNode::UNDEFINED_SOURCE || p_identifier->get_datatype().is_set()) {
			// Found.
			found_source = true;
		}
	}

	if (found_source) {
		const bool source_is_instance_variable = p_identifier->source == GDScriptParser::IdentifierNode::MEMBER_VARIABLE || p_identifier->source == GDScriptParser::IdentifierNode::INHERITED_VARIABLE;
		const bool source_is_instance_function = p_identifier->source == GDScriptParser::IdentifierNode::MEMBER_FUNCTION && !p_identifier->function_source_is_static;
		const bool source_is_signal = p_identifier->source == GDScriptParser::IdentifierNode::MEMBER_SIGNAL;

		if (static_context && (source_is_instance_variable || source_is_instance_function || source_is_signal)) {
			// Get the parent function above any lambda.
			GDScriptParser::FunctionNode *parent_function = parser->current_function;
			while (parent_function && parent_function->source_lambda) {
				parent_function = parent_function->source_lambda->parent_function;
			}

			String source_type;
			if (source_is_instance_variable) {
				source_type = "non-static variable";
			} else if (source_is_instance_function) {
				source_type = "non-static function";
			} else { // source_is_signal
				source_type = "signal";
			}

			if (parent_function) {
				push_error(vformat(R"*(Cannot access %s "%s" from the static function "%s()".)*", source_type, p_identifier->name, parent_function->identifier->name), p_identifier);
			} else {
				push_error(vformat(R"*(Cannot access %s "%s" from a static variable initializer.)*", source_type, p_identifier->name), p_identifier);
			}
		}

		if (current_lambda != nullptr) {
			// If the identifier is a member variable (including the native class properties), member function, or a signal,
			// we consider the lambda to be using `self`, so we keep a reference to the current instance.
			if (source_is_instance_variable || source_is_instance_function || source_is_signal) {
				mark_lambda_use_self();
				return; // No need to capture.
			}

			switch (p_identifier->source) {
				case GDScriptParser::IdentifierNode::FUNCTION_PARAMETER:
				case GDScriptParser::IdentifierNode::LOCAL_VARIABLE:
				case GDScriptParser::IdentifierNode::LOCAL_ITERATOR:
				case GDScriptParser::IdentifierNode::LOCAL_BIND:
					break; // Need to capture.
				case GDScriptParser::IdentifierNode::UNDEFINED_SOURCE: // A global.
				case GDScriptParser::IdentifierNode::LOCAL_CONSTANT:
				case GDScriptParser::IdentifierNode::MEMBER_VARIABLE:
				case GDScriptParser::IdentifierNode::MEMBER_CONSTANT:
				case GDScriptParser::IdentifierNode::MEMBER_FUNCTION:
				case GDScriptParser::IdentifierNode::MEMBER_SIGNAL:
				case GDScriptParser::IdentifierNode::MEMBER_CLASS:
				case GDScriptParser::IdentifierNode::INHERITED_VARIABLE:
				case GDScriptParser::IdentifierNode::STATIC_VARIABLE:
					return; // No need to capture.
			}

			GDScriptParser::FunctionNode *function_test = current_lambda->function;
			// Make sure we aren't capturing variable in the same lambda.
			// This also add captures for nested lambdas.
			while (function_test != nullptr && function_test != p_identifier->source_function && function_test->source_lambda != nullptr && !function_test->source_lambda->captures_indices.has(p_identifier->name)) {
				function_test->source_lambda->captures_indices[p_identifier->name] = function_test->source_lambda->captures.size();
				function_test->source_lambda->captures.push_back(p_identifier);
				function_test = function_test->source_lambda->parent_function;
			}
		}

		return;
	}

	StringName name = p_identifier->name;
	p_identifier->source = GDScriptParser::IdentifierNode::UNDEFINED_SOURCE;

	// Not a local or a member, so check globals.

	Variant::Type builtin_type = GDScriptParser::get_builtin_type(name);
	if (builtin_type < Variant::VARIANT_MAX) {
		if (can_be_builtin) {
			p_identifier->set_datatype(make_builtin_meta_type(builtin_type));
			return;
		} else {
			push_error(R"(Builtin type cannot be used as a name on its own.)", p_identifier);
		}
	}

	if (class_exists(name)) {
		p_identifier->set_datatype(make_native_meta_type(name));
		return;
	}

	if (ScriptServer::is_global_class(name)) {
		p_identifier->set_datatype(make_global_class_meta_type(name, p_identifier));
		return;
	}

	// Try singletons.
	// Do this before globals because this might be a singleton loading another one before it's compiled.
	if (ProjectSettings::get_singleton()->has_autoload(name)) {
		const ProjectSettings::AutoloadInfo &autoload = ProjectSettings::get_singleton()->get_autoload(name);
		if (autoload.is_singleton) {
			// Singleton exists, so it's at least a Node.
			GDScriptParser::DataType result;
			result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
			result.kind = GDScriptParser::DataType::NATIVE;
			result.builtin_type = Variant::OBJECT;
			result.native_type = SNAME("Node");
			if (ResourceLoader::get_resource_type(autoload.path) == "GDScript") {
				Ref<GDScriptParserRef> singl_parser = parser->get_depended_parser_for(autoload.path);
				if (singl_parser.is_valid()) {
					Error err = singl_parser->raise_status(GDScriptParserRef::INHERITANCE_SOLVED);
					if (err == OK) {
						result = type_from_metatype(singl_parser->get_parser()->head->get_datatype());
					}
				}
			} else if (ResourceLoader::get_resource_type(autoload.path) == "PackedScene") {
				if (GDScriptLanguage::get_singleton()->has_any_global_constant(name)) {
					Variant constant = GDScriptLanguage::get_singleton()->get_any_global_constant(name);
					Node *node = Object::cast_to<Node>(constant);
					if (node != nullptr) {
						Ref<GDScript> scr = node->get_script();
						if (scr.is_valid()) {
							Ref<GDScriptParserRef> singl_parser = parser->get_depended_parser_for(scr->get_script_path());
							if (singl_parser.is_valid()) {
								Error err = singl_parser->raise_status(GDScriptParserRef::INHERITANCE_SOLVED);
								if (err == OK) {
									result = type_from_metatype(singl_parser->get_parser()->head->get_datatype());
								}
							}
						}
					}
				}
			}
			result.is_constant = true;
			p_identifier->set_datatype(result);
			return;
		}
	}

	if (CoreConstants::is_global_constant(name)) {
		int index = CoreConstants::get_global_constant_index(name);
		StringName enum_name = CoreConstants::get_global_constant_enum(index);
		int64_t value = CoreConstants::get_global_constant_value(index);
		if (enum_name != StringName()) {
			p_identifier->set_datatype(make_global_enum_type(enum_name, StringName(), false));
		} else {
			p_identifier->set_datatype(type_from_variant(value, p_identifier));
		}
		p_identifier->is_constant = true;
		p_identifier->reduced_value = value;
		return;
	}

	if (GDScriptLanguage::get_singleton()->has_any_global_constant(name)) {
		Variant constant = GDScriptLanguage::get_singleton()->get_any_global_constant(name);
		p_identifier->set_datatype(type_from_variant(constant, p_identifier));
		p_identifier->is_constant = true;
		p_identifier->reduced_value = constant;
		return;
	}

	if (CoreConstants::is_global_enum(name)) {
		p_identifier->set_datatype(make_global_enum_type(name, StringName(), true));
		if (!can_be_builtin) {
			push_error(vformat(R"(Global enum "%s" cannot be used on its own.)", name), p_identifier);
		}
		return;
	}

	if (Variant::has_utility_function(name) || GDScriptUtilityFunctions::function_exists(name)) {
		p_identifier->is_constant = true;
		p_identifier->reduced_value = Callable(memnew(GDScriptUtilityCallable(name)));
		MethodInfo method_info;
		if (GDScriptUtilityFunctions::function_exists(name)) {
			method_info = GDScriptUtilityFunctions::get_function_info(name);
		} else {
			method_info = Variant::get_utility_function_info(name);
		}
		p_identifier->set_datatype(make_callable_type(method_info));
		return;
	}

	// Allow "Variant" here since it might be used for nested enums.
	if (can_be_builtin && name == SNAME("Variant")) {
		GDScriptParser::DataType variant;
		variant.kind = GDScriptParser::DataType::VARIANT;
		variant.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
		variant.is_meta_type = true;
		variant.is_pseudo_type = true;
		p_identifier->set_datatype(variant);
		return;
	}

	// Not found.
#ifdef SUGGEST_GODOT4_RENAMES
	String rename_hint;
	if (GLOBAL_GET(GDScriptWarning::get_settings_path_from_code(GDScriptWarning::RENAMED_IN_GODOT_4_HINT)).booleanize()) {
		const char *renamed_identifier_name = check_for_renamed_identifier(name, p_identifier->type);
		if (renamed_identifier_name) {
			rename_hint = " " + vformat(R"(Did you mean to use "%s"?)", renamed_identifier_name);
		}
	}
	push_error(vformat(R"(Identifier "%s" not declared in the current scope.%s)", name, rename_hint), p_identifier);
#else
	push_error(vformat(R"(Identifier "%s" not declared in the current scope.)", name), p_identifier);
#endif // SUGGEST_GODOT4_RENAMES
	GDScriptParser::DataType dummy;
	dummy.kind = GDScriptParser::DataType::VARIANT;
	p_identifier->set_datatype(dummy); // Just so type is set to something.
}

void GDScriptAnalyzer::reduce_lambda(GDScriptParser::LambdaNode *p_lambda) {
	// Lambda is always a Callable.
	GDScriptParser::DataType lambda_type;
	lambda_type.type_source = GDScriptParser::DataType::ANNOTATED_INFERRED;
	lambda_type.kind = GDScriptParser::DataType::BUILTIN;
	lambda_type.builtin_type = Variant::CALLABLE;
	p_lambda->set_datatype(lambda_type);

	if (p_lambda->function == nullptr) {
		return;
	}

	GDScriptParser::LambdaNode *previous_lambda = current_lambda;
	current_lambda = p_lambda;
	resolve_function_signature(p_lambda->function, p_lambda, true);
	current_lambda = previous_lambda;

	pending_body_resolution_lambdas.push_back(p_lambda);
}

void GDScriptAnalyzer::reduce_literal(GDScriptParser::LiteralNode *p_literal) {
	p_literal->reduced_value = p_literal->value;
	p_literal->is_constant = true;

	p_literal->set_datatype(type_from_variant(p_literal->reduced_value, p_literal));
}

void GDScriptAnalyzer::reduce_preload(GDScriptParser::PreloadNode *p_preload) {
	if (!p_preload->path) {
		return;
	}

	reduce_expression(p_preload->path);

	if (!p_preload->path->is_constant) {
		push_error("Preloaded path must be a constant string.", p_preload->path);
		return;
	}

	if (p_preload->path->reduced_value.get_type() != Variant::STRING) {
		push_error("Preloaded path must be a constant string.", p_preload->path);
	} else {
		p_preload->resolved_path = p_preload->path->reduced_value;
		// TODO: Save this as script dependency.
		if (p_preload->resolved_path.is_relative_path()) {
			p_preload->resolved_path = parser->script_path.get_base_dir().path_join(p_preload->resolved_path);
		}
		p_preload->resolved_path = p_preload->resolved_path.simplify_path();
		if (!ResourceLoader::exists(p_preload->resolved_path)) {
			Ref<FileAccess> file_check = FileAccess::create(FileAccess::ACCESS_RESOURCES);

			if (file_check->file_exists(p_preload->resolved_path)) {
				push_error(vformat(R"(Preload file "%s" has no resource loaders (unrecognized file extension).)", p_preload->resolved_path), p_preload->path);
			} else {
				push_error(vformat(R"(Preload file "%s" does not exist.)", p_preload->resolved_path), p_preload->path);
			}
		} else {
			// TODO: Don't load if validating: use completion cache.

			// Must load GDScript separately to permit cyclic references
			// as ResourceLoader::load() detects and rejects those.
			const String &res_type = ResourceLoader::get_resource_type(p_preload->resolved_path);
			if (res_type == "GDScript") {
				Error err = OK;
				Ref<GDScript> res = get_depended_shallow_script(p_preload->resolved_path, err);
				p_preload->resource = res;
				if (err != OK) {
					push_error(vformat(R"(Could not preload resource script "%s".)", p_preload->resolved_path), p_preload->path);
				}
			} else {
				Error err = OK;
				p_preload->resource = ResourceLoader::load(p_preload->resolved_path, res_type, ResourceFormatLoader::CACHE_MODE_REUSE, &err);
				if (err == ERR_BUSY) {
					p_preload->resource = ResourceLoader::ensure_resource_ref_override_for_outer_load(p_preload->resolved_path, res_type);
				}
				if (p_preload->resource.is_null()) {
					push_error(vformat(R"(Could not preload resource file "%s".)", p_preload->resolved_path), p_preload->path);
				}
			}
		}
	}

	p_preload->is_constant = true;
	p_preload->reduced_value = p_preload->resource;
	p_preload->set_datatype(type_from_variant(p_preload->reduced_value, p_preload));

	// TODO: Not sure if this is necessary anymore.
	// 'type_from_variant()' should call 'resolve_class_inheritance()' which would call 'ensure_cached_external_parser_for_class()'
	// Better safe than sorry.
	ensure_cached_external_parser_for_class(p_preload->get_datatype().class_type, nullptr, "Trying to resolve preload", p_preload);
}

void GDScriptAnalyzer::reduce_self(GDScriptParser::SelfNode *p_self) {
	p_self->is_constant = false;
	p_self->set_datatype(type_from_metatype(parser->current_class->get_datatype()));
	mark_lambda_use_self();
}

void GDScriptAnalyzer::reduce_subscript(GDScriptParser::SubscriptNode *p_subscript, bool p_can_be_pseudo_type) {
	if (p_subscript->base == nullptr) {
		return;
	}
	if (p_subscript->base->type == GDScriptParser::Node::IDENTIFIER) {
		reduce_identifier(static_cast<GDScriptParser::IdentifierNode *>(p_subscript->base), true);
	} else if (p_subscript->base->type == GDScriptParser::Node::SUBSCRIPT) {
		reduce_subscript(static_cast<GDScriptParser::SubscriptNode *>(p_subscript->base), true);
	} else {
		reduce_expression(p_subscript->base);
	}

	GDScriptParser::DataType result_type;

	if (p_subscript->is_attribute) {
		if (p_subscript->attribute == nullptr) {
			return;
		}

		GDScriptParser::DataType base_type = p_subscript->base->get_datatype();
		bool valid = false;

		// If the base is a metatype, use the analyzer instead.
		if (p_subscript->base->is_constant && !base_type.is_meta_type) {
			// GH-92534. If the base is a GDScript, use the analyzer instead.
			bool base_is_gdscript = false;
			if (p_subscript->base->reduced_value.get_type() == Variant::OBJECT) {
				Ref<GDScript> gdscript = Object::cast_to<GDScript>(p_subscript->base->reduced_value.get_validated_object());
				if (gdscript.is_valid()) {
					base_is_gdscript = true;
					// Makes a metatype from a constant GDScript, since `base_type` is not a metatype.
					GDScriptParser::DataType base_type_meta = type_from_variant(gdscript, p_subscript);
					// First try to reduce the attribute from the metatype.
					reduce_identifier_from_base(p_subscript->attribute, &base_type_meta);
					GDScriptParser::DataType attr_type = p_subscript->attribute->get_datatype();
					if (attr_type.is_set()) {
						valid = !attr_type.is_pseudo_type || p_can_be_pseudo_type;
						result_type = attr_type;
						p_subscript->is_constant = p_subscript->attribute->is_constant;
						p_subscript->reduced_value = p_subscript->attribute->reduced_value;
					}
					if (!valid) {
						// If unsuccessful, reset and return to the normal route.
						p_subscript->attribute->set_datatype(GDScriptParser::DataType());
					}
				}
			}
			if (!base_is_gdscript) {
				// Just try to get it.
				Variant value = p_subscript->base->reduced_value.get_named(p_subscript->attribute->name, valid);
				if (valid) {
					p_subscript->is_constant = true;
					p_subscript->reduced_value = value;
					result_type = type_from_variant(value, p_subscript);
				}
			}
		}

		if (valid) {
			// Do nothing.
		} else if (base_type.is_variant() || !base_type.is_hard_type()) {
			valid = !base_type.is_pseudo_type || p_can_be_pseudo_type;
			result_type.kind = GDScriptParser::DataType::VARIANT;
			if (base_type.is_variant() && base_type.is_hard_type() && base_type.is_meta_type && base_type.is_pseudo_type) {
				// Special case: it may be a global enum with pseudo base (e.g. Variant.Type).
				String enum_name;
				if (p_subscript->base->type == GDScriptParser::Node::IDENTIFIER) {
					enum_name = String(static_cast<GDScriptParser::IdentifierNode *>(p_subscript->base)->name) + ENUM_SEPARATOR + String(p_subscript->attribute->name);
				}
				if (CoreConstants::is_global_enum(enum_name)) {
					result_type = make_global_enum_type(enum_name, StringName());
				} else {
					valid = false;
					mark_node_unsafe(p_subscript);
				}
			} else {
				mark_node_unsafe(p_subscript);
			}
		} else {
			reduce_identifier_from_base(p_subscript->attribute, &base_type);
			GDScriptParser::DataType attr_type = p_subscript->attribute->get_datatype();
			if (attr_type.is_set()) {
				if (base_type.builtin_type == Variant::DICTIONARY && base_type.has_container_element_types()) {
					Variant::Type key_type = base_type.get_container_element_type_or_variant(0).builtin_type;
					valid = key_type == Variant::NIL || key_type == Variant::STRING || key_type == Variant::STRING_NAME;
					if (base_type.has_container_element_type(1)) {
						result_type = base_type.get_container_element_type(1);
						result_type.type_source = base_type.type_source;
					} else {
						result_type.builtin_type = Variant::NIL;
						result_type.kind = GDScriptParser::DataType::VARIANT;
						result_type.type_source = GDScriptParser::DataType::UNDETECTED;
					}
				} else {
					valid = !attr_type.is_pseudo_type || p_can_be_pseudo_type;
					result_type = attr_type;
					p_subscript->is_constant = p_subscript->attribute->is_constant;
					p_subscript->reduced_value = p_subscript->attribute->reduced_value;
				}
			} else if (!base_type.is_meta_type || !base_type.is_constant) {
				valid = base_type.kind != GDScriptParser::DataType::BUILTIN;
#ifdef DEBUG_ENABLED
				if (valid) {
					parser->push_warning(p_subscript, GDScriptWarning::UNSAFE_PROPERTY_ACCESS, p_subscript->attribute->name, base_type.to_string());
				}
#endif
				result_type.kind = GDScriptParser::DataType::VARIANT;
				mark_node_unsafe(p_subscript);
			}
		}

		if (!valid) {
			GDScriptParser::DataType attr_type = p_subscript->attribute->get_datatype();
			if (!p_can_be_pseudo_type && (attr_type.is_pseudo_type || result_type.is_pseudo_type)) {
				push_error(vformat(R"(Type "%s" in base "%s" cannot be used on its own.)", p_subscript->attribute->name, type_from_metatype(base_type).to_string()), p_subscript->attribute);
			} else {
				push_error(vformat(R"(Cannot find member "%s" in base "%s".)", p_subscript->attribute->name, type_from_metatype(base_type).to_string()), p_subscript->attribute);
			}
			result_type.kind = GDScriptParser::DataType::VARIANT;
		}
	} else {
		if (p_subscript->index == nullptr) {
			return;
		}
		reduce_expression(p_subscript->index);

		if (p_subscript->base->is_constant && p_subscript->index->is_constant) {
			// Just try to get it.
			bool valid = false;
			// TODO: Check if `p_subscript->base->reduced_value` is GDScript.
			Variant value = p_subscript->base->reduced_value.get(p_subscript->index->reduced_value, &valid);
			if (!valid) {
				push_error(vformat(R"(Cannot get index "%s" from "%s".)", p_subscript->index->reduced_value, p_subscript->base->reduced_value), p_subscript->index);
				result_type.kind = GDScriptParser::DataType::VARIANT;
			} else {
				p_subscript->is_constant = true;
				p_subscript->reduced_value = value;
				result_type = type_from_variant(value, p_subscript);
			}
		} else {
			GDScriptParser::DataType base_type = p_subscript->base->get_datatype();
			GDScriptParser::DataType index_type = p_subscript->index->get_datatype();

			if (base_type.is_variant()) {
				result_type.kind = GDScriptParser::DataType::VARIANT;
				mark_node_unsafe(p_subscript);
			} else {
				if (base_type.kind == GDScriptParser::DataType::BUILTIN && !index_type.is_variant()) {
					// Check if indexing is valid.
					bool error = index_type.kind != GDScriptParser::DataType::BUILTIN && base_type.builtin_type != Variant::DICTIONARY;
					if (!error) {
						switch (base_type.builtin_type) {
							// Expect int or real as index.
							case Variant::PACKED_BYTE_ARRAY:
							case Variant::PACKED_FLOAT32_ARRAY:
							case Variant::PACKED_FLOAT64_ARRAY:
							case Variant::PACKED_INT32_ARRAY:
							case Variant::PACKED_INT64_ARRAY:
							case Variant::PACKED_STRING_ARRAY:
							case Variant::PACKED_VECTOR2_ARRAY:
							case Variant::PACKED_VECTOR3_ARRAY:
							case Variant::PACKED_COLOR_ARRAY:
							case Variant::PACKED_VECTOR4_ARRAY:
							case Variant::ARRAY:
							case Variant::STRING:
								error = index_type.builtin_type != Variant::INT && index_type.builtin_type != Variant::FLOAT;
								break;
							// Expect String only.
							case Variant::RECT2:
							case Variant::RECT2I:
							case Variant::PLANE:
							case Variant::QUATERNION:
							case Variant::AABB:
							case Variant::OBJECT:
								error = index_type.builtin_type != Variant::STRING && index_type.builtin_type != Variant::STRING_NAME;
								break;
							// Expect String or number.
							case Variant::BASIS:
							case Variant::VECTOR2:
							case Variant::VECTOR2I:
							case Variant::VECTOR3:
							case Variant::VECTOR3I:
							case Variant::VECTOR4:
							case Variant::VECTOR4I:
							case Variant::TRANSFORM2D:
							case Variant::TRANSFORM3D:
							case Variant::PROJECTION:
								error = index_type.builtin_type != Variant::INT && index_type.builtin_type != Variant::FLOAT &&
										index_type.builtin_type != Variant::STRING && index_type.builtin_type != Variant::STRING_NAME;
								break;
							// Expect String or int.
							case Variant::COLOR:
								error = index_type.builtin_type != Variant::INT && index_type.builtin_type != Variant::STRING && index_type.builtin_type != Variant::STRING_NAME;
								break;
							// Don't support indexing, but we will check it later.
							case Variant::RID:
							case Variant::BOOL:
							case Variant::CALLABLE:
							case Variant::FLOAT:
							case Variant::INT:
							case Variant::NIL:
							case Variant::NODE_PATH:
							case Variant::SIGNAL:
							case Variant::STRING_NAME:
								break;
							// Support depends on if the dictionary has a typed key, otherwise anything is valid.
							case Variant::DICTIONARY:
								if (base_type.has_container_element_type(0)) {
									GDScriptParser::DataType key_type = base_type.get_container_element_type(0);
									switch (index_type.builtin_type) {
										// Null value will be treated as an empty object, allow.
										case Variant::NIL:
											error = key_type.builtin_type != Variant::OBJECT;
											break;
										// Objects are parsed for validity in a similar manner to container types.
										case Variant::OBJECT:
											if (key_type.builtin_type == Variant::OBJECT) {
												error = !key_type.can_reference(index_type);
											} else {
												error = key_type.builtin_type != Variant::NIL;
											}
											break;
										// String and StringName interchangeable in this context.
										case Variant::STRING:
										case Variant::STRING_NAME:
											error = key_type.builtin_type != Variant::STRING_NAME && key_type.builtin_type != Variant::STRING;
											break;
										// Ints are valid indices for floats, but not the other way around.
										case Variant::INT:
											error = key_type.builtin_type != Variant::INT && key_type.builtin_type != Variant::FLOAT;
											break;
										// All other cases require the types to match exactly.
										default:
											error = key_type.builtin_type != index_type.builtin_type;
											break;
									}
								}
								break;
							// Here for completeness.
							case Variant::VARIANT_MAX:
								break;
						}

						if (error) {
							push_error(vformat(R"(Invalid index type "%s" for a base of type "%s".)", index_type.to_string(), base_type.to_string()), p_subscript->index);
						}
					}
				} else if (base_type.kind != GDScriptParser::DataType::BUILTIN && !index_type.is_variant()) {
					if (index_type.builtin_type != Variant::STRING && index_type.builtin_type != Variant::STRING_NAME) {
						push_error(vformat(R"(Only "String" or "StringName" can be used as index for type "%s", but received "%s".)", base_type.to_string(), index_type.to_string()), p_subscript->index);
					}
				}

				// Check resulting type if possible.
				result_type.builtin_type = Variant::NIL;
				result_type.kind = GDScriptParser::DataType::BUILTIN;
				result_type.type_source = base_type.is_hard_type() ? GDScriptParser::DataType::ANNOTATED_INFERRED : GDScriptParser::DataType::INFERRED;

				if (base_type.kind != GDScriptParser::DataType::BUILTIN) {
					base_type.builtin_type = Variant::OBJECT;
				}
				switch (base_type.builtin_type) {
					// Can't index at all.
					case Variant::RID:
					case Variant::BOOL:
					case Variant::CALLABLE:
					case Variant::FLOAT:
					case Variant::INT:
					case Variant::NIL:
					case Variant::NODE_PATH:
					case Variant::SIGNAL:
					case Variant::STRING_NAME:
						result_type.kind = GDScriptParser::DataType::VARIANT;
						push_error(vformat(R"(Cannot use subscript operator on a base of type "%s".)", base_type.to_string()), p_subscript->base);
						break;
					// Return int.
					case Variant::PACKED_BYTE_ARRAY:
					case Variant::PACKED_INT32_ARRAY:
					case Variant::PACKED_INT64_ARRAY:
					case Variant::VECTOR2I:
					case Variant::VECTOR3I:
					case Variant::VECTOR4I:
						result_type.builtin_type = Variant::INT;
						break;
					// Return float.
					case Variant::PACKED_FLOAT32_ARRAY:
					case Variant::PACKED_FLOAT64_ARRAY:
					case Variant::VECTOR2:
					case Variant::VECTOR3:
					case Variant::VECTOR4:
					case Variant::QUATERNION:
						result_type.builtin_type = Variant::FLOAT;
						break;
					// Return String.
					case Variant::PACKED_STRING_ARRAY:
					case Variant::STRING:
						result_type.builtin_type = Variant::STRING;
						break;
					// Return Vector2.
					case Variant::PACKED_VECTOR2_ARRAY:
					case Variant::TRANSFORM2D:
					case Variant::RECT2:
						result_type.builtin_type = Variant::VECTOR2;
						break;
					// Return Vector2I.
					case Variant::RECT2I:
						result_type.builtin_type = Variant::VECTOR2I;
						break;
					// Return Vector3.
					case Variant::PACKED_VECTOR3_ARRAY:
					case Variant::AABB:
					case Variant::BASIS:
						result_type.builtin_type = Variant::VECTOR3;
						break;
					// Return Color.
					case Variant::PACKED_COLOR_ARRAY:
						result_type.builtin_type = Variant::COLOR;
						break;
					// Return Vector4.
					case Variant::PACKED_VECTOR4_ARRAY:
						result_type.builtin_type = Variant::VECTOR4;
						break;
					// Depends on the index.
					case Variant::TRANSFORM3D:
					case Variant::PROJECTION:
					case Variant::PLANE:
					case Variant::COLOR:
					case Variant::OBJECT:
						result_type.kind = GDScriptParser::DataType::VARIANT;
						result_type.type_source = GDScriptParser::DataType::UNDETECTED;
						break;
					// Can have an element type.
					case Variant::ARRAY:
						if (base_type.has_container_element_type(0)) {
							result_type = base_type.get_container_element_type(0);
							result_type.type_source = base_type.type_source;
						} else {
							result_type.kind = GDScriptParser::DataType::VARIANT;
							result_type.type_source = GDScriptParser::DataType::UNDETECTED;
						}
						break;
					// Can have two element types, but we only care about the value.
					case Variant::DICTIONARY:
						if (base_type.has_container_element_type(1)) {
							result_type = base_type.get_container_element_type(1);
							result_type.type_source = base_type.type_source;
						} else {
							result_type.kind = GDScriptParser::DataType::VARIANT;
							result_type.type_source = GDScriptParser::DataType::UNDETECTED;
						}
						break;
					// Here for completeness.
					case Variant::VARIANT_MAX:
						break;
				}
			}
		}
	}

	p_subscript->set_datatype(result_type);
}

void GDScriptAnalyzer::reduce_ternary_op(GDScriptParser::TernaryOpNode *p_ternary_op, bool p_is_root) {
	reduce_expression(p_ternary_op->condition);
	reduce_expression(p_ternary_op->true_expr, p_is_root);
	reduce_expression(p_ternary_op->false_expr, p_is_root);

	GDScriptParser::DataType result;

	if (p_ternary_op->condition && p_ternary_op->condition->is_constant && p_ternary_op->true_expr->is_constant && p_ternary_op->false_expr && p_ternary_op->false_expr->is_constant) {
		p_ternary_op->is_constant = true;
		if (p_ternary_op->condition->reduced_value.booleanize()) {
			p_ternary_op->reduced_value = p_ternary_op->true_expr->reduced_value;
		} else {
			p_ternary_op->reduced_value = p_ternary_op->false_expr->reduced_value;
		}
	}

	GDScriptParser::DataType true_type;
	if (p_ternary_op->true_expr) {
		true_type = p_ternary_op->true_expr->get_datatype();
	} else {
		true_type.kind = GDScriptParser::DataType::VARIANT;
	}
	GDScriptParser::DataType false_type;
	if (p_ternary_op->false_expr) {
		false_type = p_ternary_op->false_expr->get_datatype();
	} else {
		false_type.kind = GDScriptParser::DataType::VARIANT;
	}

	if (true_type.is_variant() || false_type.is_variant()) {
		result.kind = GDScriptParser::DataType::VARIANT;
	} else {
		result = true_type;
		if (!is_type_compatible(true_type, false_type)) {
			result = false_type;
			if (!is_type_compatible(false_type, true_type)) {
				result.kind = GDScriptParser::DataType::VARIANT;
#ifdef DEBUG_ENABLED
				parser->push_warning(p_ternary_op, GDScriptWarning::INCOMPATIBLE_TERNARY);
#endif
			}
		}
	}
	result.type_source = true_type.is_hard_type() && false_type.is_hard_type() ? GDScriptParser::DataType::ANNOTATED_INFERRED : GDScriptParser::DataType::INFERRED;

	p_ternary_op->set_datatype(result);
}

void GDScriptAnalyzer::reduce_type_test(GDScriptParser::TypeTestNode *p_type_test) {
	GDScriptParser::DataType result;
	result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	result.kind = GDScriptParser::DataType::BUILTIN;
	result.builtin_type = Variant::BOOL;
	p_type_test->set_datatype(result);

	if (!p_type_test->operand || !p_type_test->test_type) {
		return;
	}

	reduce_expression(p_type_test->operand);
	GDScriptParser::DataType operand_type = p_type_test->operand->get_datatype();
	GDScriptParser::DataType test_type = type_from_metatype(resolve_datatype(p_type_test->test_type));
	p_type_test->test_datatype = test_type;

	if (!operand_type.is_set() || !test_type.is_set()) {
		return;
	}

	if (p_type_test->operand->is_constant) {
		p_type_test->is_constant = true;
		p_type_test->reduced_value = false;

		if (!is_type_compatible(test_type, operand_type)) {
			push_error(vformat(R"(Expression is of type "%s" so it can't be of type "%s".)", operand_type.to_string(), test_type.to_string()), p_type_test->operand);
		} else if (is_type_compatible(test_type, type_from_variant(p_type_test->operand->reduced_value, p_type_test->operand))) {
			p_type_test->reduced_value = test_type.builtin_type != Variant::OBJECT || !p_type_test->operand->reduced_value.is_null();
		}

		return;
	}

	if (!is_type_compatible(test_type, operand_type) && !is_type_compatible(operand_type, test_type)) {
		if (operand_type.is_hard_type()) {
			push_error(vformat(R"(Expression is of type "%s" so it can't be of type "%s".)", operand_type.to_string(), test_type.to_string()), p_type_test->operand);
		} else {
			downgrade_node_type_source(p_type_test->operand);
		}
	}
}

void GDScriptAnalyzer::reduce_unary_op(GDScriptParser::UnaryOpNode *p_unary_op) {
	reduce_expression(p_unary_op->operand);

	GDScriptParser::DataType result;

	if (p_unary_op->operand == nullptr) {
		result.kind = GDScriptParser::DataType::VARIANT;
		p_unary_op->set_datatype(result);
		return;
	}

	GDScriptParser::DataType operand_type = p_unary_op->operand->get_datatype();

	if (p_unary_op->operand->is_constant) {
		p_unary_op->is_constant = true;
		p_unary_op->reduced_value = Variant::evaluate(p_unary_op->variant_op, p_unary_op->operand->reduced_value, Variant());
		result = type_from_variant(p_unary_op->reduced_value, p_unary_op);
	}

	if (operand_type.is_variant()) {
		result.kind = GDScriptParser::DataType::VARIANT;
		mark_node_unsafe(p_unary_op);
	} else {
		bool valid = false;
		result = get_operation_type(p_unary_op->variant_op, operand_type, valid, p_unary_op);

		if (!valid) {
			push_error(vformat(R"(Invalid operand of type "%s" for unary operator "%s".)", operand_type.to_string(), Variant::get_operator_name(p_unary_op->variant_op)), p_unary_op);
		}
	}

	p_unary_op->set_datatype(result);
}

Variant GDScriptAnalyzer::make_expression_reduced_value(GDScriptParser::ExpressionNode *p_expression, bool &is_reduced) {
	Variant value;

	if (p_expression == nullptr) {
		return value;
	}

	if (p_expression->is_constant) {
		is_reduced = true;
		value = p_expression->reduced_value;
	} else if (p_expression->type == GDScriptParser::Node::ARRAY) {
		value = make_array_reduced_value(static_cast<GDScriptParser::ArrayNode *>(p_expression), is_reduced);
	} else if (p_expression->type == GDScriptParser::Node::DICTIONARY) {
		value = make_dictionary_reduced_value(static_cast<GDScriptParser::DictionaryNode *>(p_expression), is_reduced);
	} else if (p_expression->type == GDScriptParser::Node::SUBSCRIPT) {
		value = make_subscript_reduced_value(static_cast<GDScriptParser::SubscriptNode *>(p_expression), is_reduced);
	}

	return value;
}

Variant GDScriptAnalyzer::make_array_reduced_value(GDScriptParser::ArrayNode *p_array, bool &is_reduced) {
	Array array = p_array->get_datatype().has_container_element_type(0) ? make_array_from_element_datatype(p_array->get_datatype().get_container_element_type(0)) : Array();

	array.resize(p_array->elements.size());
	for (int i = 0; i < p_array->elements.size(); i++) {
		GDScriptParser::ExpressionNode *element = p_array->elements[i];

		bool is_element_value_reduced = false;
		Variant element_value = make_expression_reduced_value(element, is_element_value_reduced);
		if (!is_element_value_reduced) {
			return Variant();
		}

		array[i] = element_value;
	}

	array.make_read_only();

	is_reduced = true;
	return array;
}

Variant GDScriptAnalyzer::make_dictionary_reduced_value(GDScriptParser::DictionaryNode *p_dictionary, bool &is_reduced) {
	Dictionary dictionary = p_dictionary->get_datatype().has_container_element_types()
			? make_dictionary_from_element_datatype(p_dictionary->get_datatype().get_container_element_type_or_variant(0), p_dictionary->get_datatype().get_container_element_type_or_variant(1))
			: Dictionary();

	for (int i = 0; i < p_dictionary->elements.size(); i++) {
		const GDScriptParser::DictionaryNode::Pair &element = p_dictionary->elements[i];

		bool is_element_key_reduced = false;
		Variant element_key = make_expression_reduced_value(element.key, is_element_key_reduced);
		if (!is_element_key_reduced) {
			return Variant();
		}

		bool is_element_value_reduced = false;
		Variant element_value = make_expression_reduced_value(element.value, is_element_value_reduced);
		if (!is_element_value_reduced) {
			return Variant();
		}

		dictionary[element_key] = element_value;
	}

	dictionary.make_read_only();

	is_reduced = true;
	return dictionary;
}

Variant GDScriptAnalyzer::make_subscript_reduced_value(GDScriptParser::SubscriptNode *p_subscript, bool &is_reduced) {
	if (p_subscript->base == nullptr || p_subscript->index == nullptr) {
		return Variant();
	}

	bool is_base_value_reduced = false;
	Variant base_value = make_expression_reduced_value(p_subscript->base, is_base_value_reduced);
	if (!is_base_value_reduced) {
		return Variant();
	}

	if (p_subscript->is_attribute) {
		bool is_valid = false;
		Variant value = base_value.get_named(p_subscript->attribute->name, is_valid);
		if (is_valid) {
			is_reduced = true;
			return value;
		} else {
			return Variant();
		}
	} else {
		bool is_index_value_reduced = false;
		Variant index_value = make_expression_reduced_value(p_subscript->index, is_index_value_reduced);
		if (!is_index_value_reduced) {
			return Variant();
		}

		bool is_valid = false;
		Variant value = base_value.get(index_value, &is_valid);
		if (is_valid) {
			is_reduced = true;
			return value;
		} else {
			return Variant();
		}
	}
}

Array GDScriptAnalyzer::make_array_from_element_datatype(const GDScriptParser::DataType &p_element_datatype, const GDScriptParser::Node *p_source_node) {
	Array array;

	if (p_element_datatype.builtin_type == Variant::OBJECT) {
		Ref<Script> script_type = p_element_datatype.script_type;
		if (p_element_datatype.kind == GDScriptParser::DataType::CLASS && script_type.is_null()) {
			Error err = OK;
			Ref<GDScript> scr = get_depended_shallow_script(p_element_datatype.script_path, err);
			if (err) {
				push_error(vformat(R"(Error while getting cache for script "%s".)", p_element_datatype.script_path), p_source_node);
				return array;
			}
			script_type.reference_ptr(scr->find_class(p_element_datatype.class_type->fqcn));
		}

		array.set_typed(p_element_datatype.builtin_type, p_element_datatype.native_type, script_type);
	} else {
		array.set_typed(p_element_datatype.builtin_type, StringName(), Variant());
	}

	return array;
}

Dictionary GDScriptAnalyzer::make_dictionary_from_element_datatype(const GDScriptParser::DataType &p_key_element_datatype, const GDScriptParser::DataType &p_value_element_datatype, const GDScriptParser::Node *p_source_node) {
	Dictionary dictionary;
	StringName key_name;
	Variant key_script;
	StringName value_name;
	Variant value_script;

	if (p_key_element_datatype.builtin_type == Variant::OBJECT) {
		Ref<Script> script_type = p_key_element_datatype.script_type;
		if (p_key_element_datatype.kind == GDScriptParser::DataType::CLASS && script_type.is_null()) {
			Error err = OK;
			Ref<GDScript> scr = get_depended_shallow_script(p_key_element_datatype.script_path, err);
			if (err) {
				push_error(vformat(R"(Error while getting cache for script "%s".)", p_key_element_datatype.script_path), p_source_node);
				return dictionary;
			}
			script_type.reference_ptr(scr->find_class(p_key_element_datatype.class_type->fqcn));
		}

		key_name = p_key_element_datatype.native_type;
		key_script = script_type;
	}

	if (p_value_element_datatype.builtin_type == Variant::OBJECT) {
		Ref<Script> script_type = p_value_element_datatype.script_type;
		if (p_value_element_datatype.kind == GDScriptParser::DataType::CLASS && script_type.is_null()) {
			Error err = OK;
			Ref<GDScript> scr = get_depended_shallow_script(p_value_element_datatype.script_path, err);
			if (err) {
				push_error(vformat(R"(Error while getting cache for script "%s".)", p_value_element_datatype.script_path), p_source_node);
				return dictionary;
			}
			script_type.reference_ptr(scr->find_class(p_value_element_datatype.class_type->fqcn));
		}

		value_name = p_value_element_datatype.native_type;
		value_script = script_type;
	}

	dictionary.set_typed(p_key_element_datatype.builtin_type, key_name, key_script, p_value_element_datatype.builtin_type, value_name, value_script);
	return dictionary;
}

Variant GDScriptAnalyzer::make_variable_default_value(GDScriptParser::VariableNode *p_variable) {
	Variant result = Variant();

	if (p_variable->initializer) {
		bool is_initializer_value_reduced = false;
		Variant initializer_value = make_expression_reduced_value(p_variable->initializer, is_initializer_value_reduced);
		if (is_initializer_value_reduced) {
			result = initializer_value;
		}
	} else {
		GDScriptParser::DataType datatype = p_variable->get_datatype();
		if (datatype.is_hard_type()) {
			if (datatype.kind == GDScriptParser::DataType::BUILTIN && datatype.builtin_type != Variant::OBJECT) {
				if (datatype.builtin_type == Variant::ARRAY && datatype.has_container_element_type(0)) {
					result = make_array_from_element_datatype(datatype.get_container_element_type(0));
				} else if (datatype.builtin_type == Variant::DICTIONARY && datatype.has_container_element_types()) {
					GDScriptParser::DataType key = datatype.get_container_element_type_or_variant(0);
					GDScriptParser::DataType value = datatype.get_container_element_type_or_variant(1);
					result = make_dictionary_from_element_datatype(key, value);
				} else {
					VariantInternal::initialize(&result, datatype.builtin_type);
				}
			} else if (datatype.kind == GDScriptParser::DataType::ENUM) {
				result = 0;
			}
		}
	}

	return result;
}

GDScriptParser::DataType GDScriptAnalyzer::type_from_variant(const Variant &p_value, const GDScriptParser::Node *p_source) {
	GDScriptParser::DataType result;
	result.is_constant = true;
	result.kind = GDScriptParser::DataType::BUILTIN;
	result.builtin_type = p_value.get_type();
	result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT; // Constant has explicit type.

	if (p_value.get_type() == Variant::ARRAY) {
		const Array &array = p_value;
		if (array.get_typed_script()) {
			result.set_container_element_type(0, type_from_metatype(make_script_meta_type(array.get_typed_script())));
		} else if (array.get_typed_class_name()) {
			result.set_container_element_type(0, type_from_metatype(make_native_meta_type(array.get_typed_class_name())));
		} else if (array.get_typed_builtin() != Variant::NIL) {
			result.set_container_element_type(0, type_from_metatype(make_builtin_meta_type((Variant::Type)array.get_typed_builtin())));
		}
	} else if (p_value.get_type() == Variant::DICTIONARY) {
		const Dictionary &dict = p_value;
		if (dict.get_typed_key_script()) {
			result.set_container_element_type(0, type_from_metatype(make_script_meta_type(dict.get_typed_key_script())));
		} else if (dict.get_typed_key_class_name()) {
			result.set_container_element_type(0, type_from_metatype(make_native_meta_type(dict.get_typed_key_class_name())));
		} else if (dict.get_typed_key_builtin() != Variant::NIL) {
			result.set_container_element_type(0, type_from_metatype(make_builtin_meta_type((Variant::Type)dict.get_typed_key_builtin())));
		}
		if (dict.get_typed_value_script()) {
			result.set_container_element_type(1, type_from_metatype(make_script_meta_type(dict.get_typed_value_script())));
		} else if (dict.get_typed_value_class_name()) {
			result.set_container_element_type(1, type_from_metatype(make_native_meta_type(dict.get_typed_value_class_name())));
		} else if (dict.get_typed_value_builtin() != Variant::NIL) {
			result.set_container_element_type(1, type_from_metatype(make_builtin_meta_type((Variant::Type)dict.get_typed_value_builtin())));
		}
	} else if (p_value.get_type() == Variant::OBJECT) {
		// Object is treated as a native type, not a builtin type.
		result.kind = GDScriptParser::DataType::NATIVE;

		Object *obj = p_value;
		if (!obj) {
			return GDScriptParser::DataType();
		}
		result.native_type = obj->get_class_name();

		Ref<Script> scr = p_value; // Check if value is a script itself.
		if (scr.is_valid()) {
			result.is_meta_type = true;
		} else {
			result.is_meta_type = false;
			scr = obj->get_script();
		}
		if (scr.is_valid()) {
			Ref<GDScript> gds = scr;
			if (gds.is_valid()) {
				// This might be an inner class, so we want to get the parser for the root.
				// But still get the inner class from that tree.
				String script_path = gds->get_script_path();
				Ref<GDScriptParserRef> ref = parser->get_depended_parser_for(script_path);
				if (ref.is_null()) {
					push_error(vformat(R"(Could not find script "%s".)", script_path), p_source);
					GDScriptParser::DataType error_type;
					error_type.kind = GDScriptParser::DataType::VARIANT;
					return error_type;
				}
				Error err = ref->raise_status(GDScriptParserRef::INHERITANCE_SOLVED);
				GDScriptParser::ClassNode *found = nullptr;
				if (err == OK) {
					found = ref->get_parser()->find_class(gds->fully_qualified_name);
					if (found != nullptr) {
						err = resolve_class_inheritance(found, p_source);
					}
				}
				if (err || found == nullptr) {
					push_error(vformat(R"(Could not resolve script "%s".)", script_path), p_source);
					GDScriptParser::DataType error_type;
					error_type.kind = GDScriptParser::DataType::VARIANT;
					return error_type;
				}

				result.kind = GDScriptParser::DataType::CLASS;
				result.native_type = found->get_datatype().native_type;
				result.class_type = found;
				result.script_path = ref->get_parser()->script_path;
			} else {
				result.kind = GDScriptParser::DataType::SCRIPT;
				result.native_type = scr->get_instance_base_type();
				result.script_path = scr->get_path();
			}
			result.script_type = scr;
		} else {
			result.kind = GDScriptParser::DataType::NATIVE;
			if (result.native_type == GDScriptNativeClass::get_class_static()) {
				result.is_meta_type = true;
			}
		}
	}

	return result;
}

GDScriptParser::DataType GDScriptAnalyzer::type_from_metatype(const GDScriptParser::DataType &p_meta_type) {
	GDScriptParser::DataType result = p_meta_type;
	result.is_meta_type = false;
	result.is_pseudo_type = false;
	if (p_meta_type.kind == GDScriptParser::DataType::ENUM) {
		result.builtin_type = Variant::INT;
	} else {
		result.is_constant = false;
	}
	return result;
}

GDScriptParser::DataType GDScriptAnalyzer::type_from_property(const PropertyInfo &p_property, bool p_is_arg, bool p_is_readonly) const {
	GDScriptParser::DataType result;
	result.is_read_only = p_is_readonly;
	result.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
	if (p_property.type == Variant::NIL && (p_is_arg || (p_property.usage & PROPERTY_USAGE_NIL_IS_VARIANT))) {
		// Variant
		result.kind = GDScriptParser::DataType::VARIANT;
		return result;
	}
	result.builtin_type = p_property.type;
	if (p_property.type == Variant::OBJECT) {
		if (ScriptServer::is_global_class(p_property.class_name)) {
			result.kind = GDScriptParser::DataType::SCRIPT;
			result.script_path = ScriptServer::get_global_class_path(p_property.class_name);
			result.native_type = ScriptServer::get_global_class_native_base(p_property.class_name);

			Ref<Script> scr = ResourceLoader::load(ScriptServer::get_global_class_path(p_property.class_name));
			if (scr.is_valid()) {
				result.script_type = scr;
			}
		} else {
			result.kind = GDScriptParser::DataType::NATIVE;
			result.native_type = p_property.class_name == StringName() ? "Object" : p_property.class_name;
		}
	} else {
		result.kind = GDScriptParser::DataType::BUILTIN;
		result.builtin_type = p_property.type;
		if (p_property.type == Variant::ARRAY && p_property.hint == PROPERTY_HINT_ARRAY_TYPE) {
			// Check element type.
			StringName elem_type_name = p_property.hint_string;
			GDScriptParser::DataType elem_type;
			elem_type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;

			Variant::Type elem_builtin_type = GDScriptParser::get_builtin_type(elem_type_name);
			if (elem_builtin_type < Variant::VARIANT_MAX) {
				// Builtin type.
				elem_type.kind = GDScriptParser::DataType::BUILTIN;
				elem_type.builtin_type = elem_builtin_type;
			} else if (class_exists(elem_type_name)) {
				elem_type.kind = GDScriptParser::DataType::NATIVE;
				elem_type.builtin_type = Variant::OBJECT;
				elem_type.native_type = elem_type_name;
			} else if (ScriptServer::is_global_class(elem_type_name)) {
				// Just load this as it shouldn't be a GDScript.
				Ref<Script> script = ResourceLoader::load(ScriptServer::get_global_class_path(elem_type_name));
				elem_type.kind = GDScriptParser::DataType::SCRIPT;
				elem_type.builtin_type = Variant::OBJECT;
				elem_type.native_type = script->get_instance_base_type();
				elem_type.script_type = script;
			} else {
				ERR_FAIL_V_MSG(result, "Could not find element type from property hint of a typed array.");
			}
			elem_type.is_constant = false;
			result.set_container_element_type(0, elem_type);
		} else if (p_property.type == Variant::DICTIONARY && p_property.hint == PROPERTY_HINT_DICTIONARY_TYPE) {
			// Check element type.
			StringName key_elem_type_name = p_property.hint_string.get_slice(";", 0);
			GDScriptParser::DataType key_elem_type;
			key_elem_type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;

			Variant::Type key_elem_builtin_type = GDScriptParser::get_builtin_type(key_elem_type_name);
			if (key_elem_builtin_type < Variant::VARIANT_MAX) {
				// Builtin type.
				key_elem_type.kind = GDScriptParser::DataType::BUILTIN;
				key_elem_type.builtin_type = key_elem_builtin_type;
			} else if (class_exists(key_elem_type_name)) {
				key_elem_type.kind = GDScriptParser::DataType::NATIVE;
				key_elem_type.builtin_type = Variant::OBJECT;
				key_elem_type.native_type = key_elem_type_name;
			} else if (ScriptServer::is_global_class(key_elem_type_name)) {
				// Just load this as it shouldn't be a GDScript.
				Ref<Script> script = ResourceLoader::load(ScriptServer::get_global_class_path(key_elem_type_name));
				key_elem_type.kind = GDScriptParser::DataType::SCRIPT;
				key_elem_type.builtin_type = Variant::OBJECT;
				key_elem_type.native_type = script->get_instance_base_type();
				key_elem_type.script_type = script;
			} else {
				ERR_FAIL_V_MSG(result, "Could not find element type from property hint of a typed dictionary.");
			}
			key_elem_type.is_constant = false;

			StringName value_elem_type_name = p_property.hint_string.get_slice(";", 1);
			GDScriptParser::DataType value_elem_type;
			value_elem_type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;

			Variant::Type value_elem_builtin_type = GDScriptParser::get_builtin_type(value_elem_type_name);
			if (value_elem_builtin_type < Variant::VARIANT_MAX) {
				// Builtin type.
				value_elem_type.kind = GDScriptParser::DataType::BUILTIN;
				value_elem_type.builtin_type = value_elem_builtin_type;
			} else if (class_exists(value_elem_type_name)) {
				value_elem_type.kind = GDScriptParser::DataType::NATIVE;
				value_elem_type.builtin_type = Variant::OBJECT;
				value_elem_type.native_type = value_elem_type_name;
			} else if (ScriptServer::is_global_class(value_elem_type_name)) {
				// Just load this as it shouldn't be a GDScript.
				Ref<Script> script = ResourceLoader::load(ScriptServer::get_global_class_path(value_elem_type_name));
				value_elem_type.kind = GDScriptParser::DataType::SCRIPT;
				value_elem_type.builtin_type = Variant::OBJECT;
				value_elem_type.native_type = script->get_instance_base_type();
				value_elem_type.script_type = script;
			} else {
				ERR_FAIL_V_MSG(result, "Could not find element type from property hint of a typed dictionary.");
			}
			value_elem_type.is_constant = false;

			result.set_container_element_type(0, key_elem_type);
			result.set_container_element_type(1, value_elem_type);
		} else if (p_property.type == Variant::INT) {
			// Check if it's enum.
			if ((p_property.usage & PROPERTY_USAGE_CLASS_IS_ENUM) && p_property.class_name != StringName()) {
				if (CoreConstants::is_global_enum(p_property.class_name)) {
					result = make_global_enum_type(p_property.class_name, StringName(), false);
					result.is_constant = false;
				} else {
					Vector<String> names = String(p_property.class_name).split(ENUM_SEPARATOR);
					if (names.size() == 2) {
						result = make_enum_type(names[1], names[0], false);
						result.is_constant = false;
					}
				}
			}
			// PROPERTY_USAGE_CLASS_IS_BITFIELD: BitField[T] isn't supported (yet?), use plain int.
		}
	}
	return result;
}

bool GDScriptAnalyzer::get_function_signature(GDScriptParser::Node *p_source, bool p_is_constructor, GDScriptParser::DataType p_base_type, const StringName &p_function, GDScriptParser::DataType &r_return_type, List<GDScriptParser::DataType> &r_par_types, int &r_default_arg_count, BitField<MethodFlags> &r_method_flags, StringName *r_native_class) {
	r_method_flags = METHOD_FLAGS_DEFAULT;
	r_default_arg_count = 0;
	if (r_native_class) {
		*r_native_class = StringName();
	}
	StringName function_name = p_function;

	bool was_enum = false;
	if (p_base_type.kind == GDScriptParser::DataType::ENUM) {
		was_enum = true;
		if (p_base_type.is_meta_type) {
			// Enum type can be treated as a dictionary value.
			p_base_type.kind = GDScriptParser::DataType::BUILTIN;
			p_base_type.is_meta_type = false;
		} else {
			push_error("Cannot call function on enum value.", p_source);
			return false;
		}
	}

	if (p_base_type.kind == GDScriptParser::DataType::BUILTIN) {
		// Construct a base type to get methods.
		Callable::CallError err;
		Variant dummy;
		Variant::construct(p_base_type.builtin_type, dummy, nullptr, 0, err);
		if (err.error != Callable::CallError::CALL_OK) {
			ERR_FAIL_V_MSG(false, "Could not construct base Variant type.");
		}
		List<MethodInfo> methods;
		dummy.get_method_list(&methods);

		for (const MethodInfo &E : methods) {
			if (E.name == p_function) {
				function_signature_from_info(E, r_return_type, r_par_types, r_default_arg_count, r_method_flags);
				// Cannot use non-const methods on enums.
				if (!r_method_flags.has_flag(METHOD_FLAG_STATIC) && was_enum && !(E.flags & METHOD_FLAG_CONST)) {
					push_error(vformat(R"*(Cannot call non-const Dictionary function "%s()" on enum "%s".)*", p_function, p_base_type.enum_type), p_source);
				}
				return true;
			}
		}

		return false;
	}

	StringName base_native = p_base_type.native_type;
	if (base_native != StringName()) {
		// Empty native class might happen in some Script implementations.
		// Just ignore it.
		if (!class_exists(base_native)) {
			push_error(vformat("Native class %s used in script doesn't exist or isn't exposed.", base_native), p_source);
			return false;
		} else if (p_is_constructor && ClassDB::is_abstract(base_native)) {
			if (p_base_type.kind == GDScriptParser::DataType::CLASS) {
				push_error(vformat(R"(Class "%s" cannot be constructed as it is based on abstract native class "%s".)", p_base_type.class_type->fqcn.get_file(), base_native), p_source);
			} else if (p_base_type.kind == GDScriptParser::DataType::SCRIPT) {
				push_error(vformat(R"(Script "%s" cannot be constructed as it is based on abstract native class "%s".)", p_base_type.script_path.get_file(), base_native), p_source);
			} else {
				push_error(vformat(R"(Native class "%s" cannot be constructed as it is abstract.)", base_native), p_source);
			}
			return false;
		}
	}

	if (p_is_constructor) {
		function_name = GDScriptLanguage::get_singleton()->strings._init;
		r_method_flags.set_flag(METHOD_FLAG_STATIC);
	}

	GDScriptParser::ClassNode *base_class = p_base_type.class_type;
	GDScriptParser::FunctionNode *found_function = nullptr;

	while (found_function == nullptr && base_class != nullptr) {
		if (base_class->has_member(function_name)) {
			if (base_class->get_member(function_name).type != GDScriptParser::ClassNode::Member::FUNCTION) {
				// TODO: If this is Callable it can have a better error message.
				push_error(vformat(R"(Member "%s" is not a function.)", function_name), p_source);
				return false;
			}

			resolve_class_member(base_class, function_name, p_source);
			found_function = base_class->get_member(function_name).function;
		}

		resolve_class_inheritance(base_class, p_source);
		base_class = base_class->base_type.class_type;
	}

	if (found_function != nullptr) {
		if (p_is_constructor || found_function->is_static) {
			r_method_flags.set_flag(METHOD_FLAG_STATIC);
		}
		for (int i = 0; i < found_function->parameters.size(); i++) {
			r_par_types.push_back(found_function->parameters[i]->get_datatype());
			if (found_function->parameters[i]->initializer != nullptr) {
				r_default_arg_count++;
			}
		}
		r_return_type = p_is_constructor ? p_base_type : found_function->get_datatype();
		r_return_type.is_meta_type = false;
		r_return_type.is_coroutine = found_function->is_coroutine;

		return true;
	}

	Ref<Script> base_script = p_base_type.script_type;

	while (base_script.is_valid() && base_script->has_method(function_name)) {
		MethodInfo info = base_script->get_method_info(function_name);

		if (!(info == MethodInfo())) {
			return function_signature_from_info(info, r_return_type, r_par_types, r_default_arg_count, r_method_flags);
		}
		base_script = base_script->get_base_script();
	}

	// If the base is a script, it might be trying to access members of the Script class itself.
	if (p_base_type.is_meta_type && !p_is_constructor && (p_base_type.kind == GDScriptParser::DataType::SCRIPT || p_base_type.kind == GDScriptParser::DataType::CLASS)) {
		MethodInfo info;
		StringName script_class = p_base_type.kind == GDScriptParser::DataType::SCRIPT ? p_base_type.script_type->get_class_name() : StringName(GDScript::get_class_static());

		if (ClassDB::get_method_info(script_class, function_name, &info)) {
			return function_signature_from_info(info, r_return_type, r_par_types, r_default_arg_count, r_method_flags);
		}
	}

	if (p_is_constructor) {
		// Native types always have a default constructor.
		r_return_type = p_base_type;
		r_return_type.type_source = GDScriptParser::DataType::ANNOTATED_EXPLICIT;
		r_return_type.is_meta_type = false;
		return true;
	}

	MethodInfo info;
	if (ClassDB::get_method_info(base_native, function_name, &info)) {
		bool valid = function_signature_from_info(info, r_return_type, r_par_types, r_default_arg_count, r_method_flags);
		if (valid && Engine::get_singleton()->has_singleton(base_native)) {
			r_method_flags.set_flag(METHOD_FLAG_STATIC);
		}
#ifdef DEBUG_ENABLED
		MethodBind *native_method = ClassDB::get_method(base_native, function_name);
		if (native_method && r_native_class) {
			*r_native_class = native_method->get_instance_class();
		}
#endif
		return valid;
	}

	return false;
}

bool GDScriptAnalyzer::function_signature_from_info(const MethodInfo &p_info, GDScriptParser::DataType &r_return_type, List<GDScriptParser::DataType> &r_par_types, int &r_default_arg_count, BitField<MethodFlags> &r_method_flags) {
	r_return_type = type_from_property(p_info.return_val);
	r_default_arg_count = p_info.default_arguments.size();
	r_method_flags = p_info.flags;

	for (const PropertyInfo &E : p_info.arguments) {
		r_par_types.push_back(type_from_property(E, true));
	}
	return true;
}

void GDScriptAnalyzer::validate_call_arg(const MethodInfo &p_method, const GDScriptParser::CallNode *p_call) {
	List<GDScriptParser::DataType> arg_types;

	for (const PropertyInfo &E : p_method.arguments) {
		arg_types.push_back(type_from_property(E, true));
	}

	validate_call_arg(arg_types, p_method.default_arguments.size(), (p_method.flags & METHOD_FLAG_VARARG) != 0, p_call);
}

void GDScriptAnalyzer::validate_call_arg(const List<GDScriptParser::DataType> &p_par_types, int p_default_args_count, bool p_is_vararg, const GDScriptParser::CallNode *p_call) {
	if (p_call->arguments.size() < p_par_types.size() - p_default_args_count) {
		push_error(vformat(R"*(Too few arguments for "%s()" call. Expected at least %d but received %d.)*", p_call->function_name, p_par_types.size() - p_default_args_count, p_call->arguments.size()), p_call);
	}
	if (!p_is_vararg && p_call->arguments.size() > p_par_types.size()) {
		push_error(vformat(R"*(Too many arguments for "%s()" call. Expected at most %d but received %d.)*", p_call->function_name, p_par_types.size(), p_call->arguments.size()), p_call->arguments[p_par_types.size()]);
	}

	List<GDScriptParser::DataType>::ConstIterator par_itr = p_par_types.begin();
	for (int i = 0; i < p_call->arguments.size(); ++par_itr, ++i) {
		if (i >= p_par_types.size()) {
			// Already on vararg place.
			break;
		}
		GDScriptParser::DataType par_type = *par_itr;

		if (par_type.is_hard_type() && p_call->arguments[i]->is_constant) {
			update_const_expression_builtin_type(p_call->arguments[i], par_type, "pass");
		}
		GDScriptParser::DataType arg_type = p_call->arguments[i]->get_datatype();

		if (arg_type.is_variant() || !arg_type.is_hard_type()) {
#ifdef DEBUG_ENABLED
			// Argument can be anything, so this is unsafe (unless the parameter is a hard variant).
			if (!(par_type.is_hard_type() && par_type.is_variant())) {
				mark_node_unsafe(p_call->arguments[i]);
				parser->push_warning(p_call->arguments[i], GDScriptWarning::UNSAFE_CALL_ARGUMENT, itos(i + 1), "function", p_call->function_name, par_type.to_string(), arg_type.to_string_strict());
			}
#endif
		} else if (par_type.is_hard_type() && !is_type_compatible(par_type, arg_type, true)) {
			if (!is_type_compatible(arg_type, par_type)) {
				push_error(vformat(R"*(Invalid argument for "%s()" function: argument %d should be "%s" but is "%s".)*",
								   p_call->function_name, i + 1, par_type.to_string(), arg_type.to_string()),
						p_call->arguments[i]);
#ifdef DEBUG_ENABLED
			} else {
				// Supertypes are acceptable for dynamic compliance, but it's unsafe.
				mark_node_unsafe(p_call);
				parser->push_warning(p_call->arguments[i], GDScriptWarning::UNSAFE_CALL_ARGUMENT, itos(i + 1), "function", p_call->function_name, par_type.to_string(), arg_type.to_string_strict());
#endif
			}
#ifdef DEBUG_ENABLED
		} else if (par_type.kind == GDScriptParser::DataType::BUILTIN && par_type.builtin_type == Variant::INT && arg_type.kind == GDScriptParser::DataType::BUILTIN && arg_type.builtin_type == Variant::FLOAT) {
			parser->push_warning(p_call->arguments[i], GDScriptWarning::NARROWING_CONVERSION, p_call->function_name);
#endif
		}
	}
}

#ifdef DEBUG_ENABLED
void GDScriptAnalyzer::is_shadowing(GDScriptParser::IdentifierNode *p_identifier, const String &p_context, const bool p_in_local_scope) {
	const StringName &name = p_identifier->name;

	{
		List<MethodInfo> gdscript_funcs;
		GDScriptLanguage::get_singleton()->get_public_functions(&gdscript_funcs);

		for (MethodInfo &info : gdscript_funcs) {
			if (info.name == name) {
				parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_GLOBAL_IDENTIFIER, p_context, name, "built-in function");
				return;
			}
		}
		if (Variant::has_utility_function(name)) {
			parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_GLOBAL_IDENTIFIER, p_context, name, "built-in function");
			return;
		} else if (ClassDB::class_exists(name)) {
			parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_GLOBAL_IDENTIFIER, p_context, name, "native class");
			return;
		} else if (ScriptServer::is_global_class(name)) {
			String class_path = ScriptServer::get_global_class_path(name).get_file();
			parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_GLOBAL_IDENTIFIER, p_context, name, vformat(R"(global class defined in "%s")", class_path));
			return;
		} else if (GDScriptParser::get_builtin_type(name) < Variant::VARIANT_MAX) {
			parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_GLOBAL_IDENTIFIER, p_context, name, "built-in type");
			return;
		}
	}

	const GDScriptParser::DataType current_class_type = parser->current_class->get_datatype();
	if (p_in_local_scope) {
		GDScriptParser::ClassNode *base_class = current_class_type.class_type;

		if (base_class != nullptr) {
			if (base_class->has_member(name)) {
				parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_VARIABLE, p_context, p_identifier->name, base_class->get_member(name).get_type_name(), itos(base_class->get_member(name).get_line()));
				return;
			}
			base_class = base_class->base_type.class_type;
		}

		while (base_class != nullptr) {
			if (base_class->has_member(name)) {
				String base_class_name = base_class->get_global_name();
				if (base_class_name.is_empty()) {
					base_class_name = base_class->fqcn;
				}

				parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_VARIABLE_BASE_CLASS, p_context, p_identifier->name, base_class->get_member(name).get_type_name(), itos(base_class->get_member(name).get_line()), base_class_name);
				return;
			}
			base_class = base_class->base_type.class_type;
		}
	}

	StringName native_base_class = current_class_type.native_type;
	while (native_base_class != StringName()) {
		ERR_FAIL_COND_MSG(!class_exists(native_base_class), "Non-existent native base class.");

		if (ClassDB::has_method(native_base_class, name, true)) {
			parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_VARIABLE_BASE_CLASS, p_context, p_identifier->name, "method", native_base_class);
			return;
		} else if (ClassDB::has_signal(native_base_class, name, true)) {
			parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_VARIABLE_BASE_CLASS, p_context, p_identifier->name, "signal", native_base_class);
			return;
		} else if (ClassDB::has_property(native_base_class, name, true)) {
			parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_VARIABLE_BASE_CLASS, p_context, p_identifier->name, "property", native_base_class);
			return;
		} else if (ClassDB::has_integer_constant(native_base_class, name, true)) {
			parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_VARIABLE_BASE_CLASS, p_context, p_identifier->name, "constant", native_base_class);
			return;
		} else if (ClassDB::has_enum(native_base_class, name, true)) {
			parser->push_warning(p_identifier, GDScriptWarning::SHADOWED_VARIABLE_BASE_CLASS, p_context, p_identifier->name, "enum", native_base_class);
			return;
		}
		native_base_class = ClassDB::get_parent_class(native_base_class);
	}
}
#endif // DEBUG_ENABLED

GDScriptParser::DataType GDScriptAnalyzer::get_operation_type(Variant::Operator p_operation, const GDScriptParser::DataType &p_a, bool &r_valid, const GDScriptParser::Node *p_source) {
	// Unary version.
	GDScriptParser::DataType nil_type;
	nil_type.builtin_type = Variant::NIL;
	nil_type.type_source = GDScriptParser::DataType::ANNOTATED_INFERRED;
	return get_operation_type(p_operation, p_a, nil_type, r_valid, p_source);
}

GDScriptParser::DataType GDScriptAnalyzer::get_operation_type(Variant::Operator p_operation, const GDScriptParser::DataType &p_a, const GDScriptParser::DataType &p_b, bool &r_valid, const GDScriptParser::Node *p_source) {
	if (p_operation == Variant::OP_AND || p_operation == Variant::OP_OR) {
		// Those work for any type of argument and always return a boolean.
		// They don't use the Variant operator since they have short-circuit semantics.
		r_valid = true;
		GDScriptParser::DataType result;
		result.type_source = GDScriptParser::DataType::ANNOTATED_INFERRED;
		result.kind = GDScriptParser::DataType::BUILTIN;
		result.builtin_type = Variant::BOOL;
		return result;
	}

	Variant::Type a_type = p_a.builtin_type;
	Variant::Type b_type = p_b.builtin_type;

	if (p_a.kind == GDScriptParser::DataType::ENUM) {
		if (p_a.is_meta_type) {
			a_type = Variant::DICTIONARY;
		} else {
			a_type = Variant::INT;
		}
	}
	if (p_b.kind == GDScriptParser::DataType::ENUM) {
		if (p_b.is_meta_type) {
			b_type = Variant::DICTIONARY;
		} else {
			b_type = Variant::INT;
		}
	}

	GDScriptParser::DataType result;
	bool hard_operation = p_a.is_hard_type() && p_b.is_hard_type();

	if (p_operation == Variant::OP_ADD && a_type == Variant::ARRAY && b_type == Variant::ARRAY) {
		if (p_a.has_container_element_type(0) && p_b.has_container_element_type(0) && p_a.get_container_element_type(0) == p_b.get_container_element_type(0)) {
			r_valid = true;
			result = p_a;
			result.type_source = hard_operation ? GDScriptParser::DataType::ANNOTATED_INFERRED : GDScriptParser::DataType::INFERRED;
			return result;
		}
	}

	Variant::ValidatedOperatorEvaluator op_eval = Variant::get_validated_operator_evaluator(p_operation, a_type, b_type);
	bool validated = op_eval != nullptr;

	if (validated) {
		r_valid = true;
		result.type_source = hard_operation ? GDScriptParser::DataType::ANNOTATED_INFERRED : GDScriptParser::DataType::INFERRED;
		result.kind = GDScriptParser::DataType::BUILTIN;
		result.builtin_type = Variant::get_operator_return_type(p_operation, a_type, b_type);
	} else {
		r_valid = !hard_operation;
		result.kind = GDScriptParser::DataType::VARIANT;
	}

	return result;
}

bool GDScriptAnalyzer::is_type_compatible(const GDScriptParser::DataType &p_target, const GDScriptParser::DataType &p_source, bool p_allow_implicit_conversion, const GDScriptParser::Node *p_source_node) {
#ifdef DEBUG_ENABLED
	if (p_source_node) {
		if (p_target.kind == GDScriptParser::DataType::ENUM) {
			if (p_source.kind == GDScriptParser::DataType::BUILTIN && p_source.builtin_type == Variant::INT) {
				parser->push_warning(p_source_node, GDScriptWarning::INT_AS_ENUM_WITHOUT_CAST);
			}
		}
	}
#endif
	return check_type_compatibility(p_target, p_source, p_allow_implicit_conversion, p_source_node);
}

// TODO: Add safe/unsafe return variable (for variant cases)
bool GDScriptAnalyzer::check_type_compatibility(const GDScriptParser::DataType &p_target, const GDScriptParser::DataType &p_source, bool p_allow_implicit_conversion, const GDScriptParser::Node *p_source_node) {
	// These return "true" so it doesn't affect users negatively.
	ERR_FAIL_COND_V_MSG(!p_target.is_set(), true, "Parser bug (please report): Trying to check compatibility of unset target type");
	ERR_FAIL_COND_V_MSG(!p_source.is_set(), true, "Parser bug (please report): Trying to check compatibility of unset value type");

	if (p_target.kind == GDScriptParser::DataType::VARIANT) {
		// Variant can receive anything.
		return true;
	}

	if (p_source.kind == GDScriptParser::DataType::VARIANT) {
		// TODO: This is acceptable but unsafe. Make sure unsafe line is set.
		return true;
	}

	if (p_target.kind == GDScriptParser::DataType::BUILTIN) {
		bool valid = p_source.kind == GDScriptParser::DataType::BUILTIN && p_target.builtin_type == p_source.builtin_type;
		if (!valid && p_allow_implicit_conversion) {
			valid = Variant::can_convert_strict(p_source.builtin_type, p_target.builtin_type);
		}
		if (!valid && p_target.builtin_type == Variant::INT && p_source.kind == GDScriptParser::DataType::ENUM && !p_source.is_meta_type) {
			// Enum value is also integer.
			valid = true;
		}
		if (valid && p_target.builtin_type == Variant::ARRAY && p_source.builtin_type == Variant::ARRAY) {
			// Check the element type.
			if (p_target.has_container_element_type(0) && p_source.has_container_element_type(0)) {
				valid = p_target.get_container_element_type(0) == p_source.get_container_element_type(0);
			}
		}
		if (valid && p_target.builtin_type == Variant::DICTIONARY && p_source.builtin_type == Variant::DICTIONARY) {
			// Check the element types.
			if (p_target.has_container_element_type(0) && p_source.has_container_element_type(0)) {
				valid = p_target.get_container_element_type(0) == p_source.get_container_element_type(0);
			}
			if (valid && p_target.has_container_element_type(1) && p_source.has_container_element_type(1)) {
				valid = p_target.get_container_element_type(1) == p_source.get_container_element_type(1);
			}
		}
		return valid;
	}

	if (p_target.kind == GDScriptParser::DataType::ENUM) {
		if (p_source.kind == GDScriptParser::DataType::BUILTIN && p_source.builtin_type == Variant::INT) {
			return true;
		}
		if (p_source.kind == GDScriptParser::DataType::ENUM) {
			if (p_source.native_type == p_target.native_type) {
				return true;
			}
		}
		return false;
	}

	// From here on the target type is an object, so we have to test polymorphism.

	if (p_source.kind == GDScriptParser::DataType::BUILTIN && p_source.builtin_type == Variant::NIL) {
		// null is acceptable in object.
		return true;
	}

	StringName src_native;
	Ref<Script> src_script;
	const GDScriptParser::ClassNode *src_class = nullptr;

	switch (p_source.kind) {
		case GDScriptParser::DataType::NATIVE:
			if (p_target.kind != GDScriptParser::DataType::NATIVE) {
				// Non-native class cannot be supertype of native.
				return false;
			}
			if (p_source.is_meta_type) {
				src_native = GDScriptNativeClass::get_class_static();
			} else {
				src_native = p_source.native_type;
			}
			break;
		case GDScriptParser::DataType::SCRIPT:
			if (p_target.kind == GDScriptParser::DataType::CLASS) {
				// A script type cannot be a subtype of a GDScript class.
				return false;
			}
			if (p_source.script_type.is_null()) {
				return false;
			}
			if (p_source.is_meta_type) {
				src_native = p_source.script_type->get_class_name();
			} else {
				src_script = p_source.script_type;
				src_native = src_script->get_instance_base_type();
			}
			break;
		case GDScriptParser::DataType::CLASS:
			if (p_source.is_meta_type) {
				src_native = GDScript::get_class_static();
			} else {
				src_class = p_source.class_type;
				const GDScriptParser::ClassNode *base = src_class;
				while (base->base_type.kind == GDScriptParser::DataType::CLASS) {
					base = base->base_type.class_type;
				}
				src_native = base->base_type.native_type;
				src_script = base->base_type.script_type;
			}
			break;
		case GDScriptParser::DataType::VARIANT:
		case GDScriptParser::DataType::BUILTIN:
		case GDScriptParser::DataType::ENUM:
		case GDScriptParser::DataType::RESOLVING:
		case GDScriptParser::DataType::UNRESOLVED:
			break; // Already solved before.
	}

	switch (p_target.kind) {
		case GDScriptParser::DataType::NATIVE: {
			if (p_target.is_meta_type) {
				return ClassDB::is_parent_class(src_native, GDScriptNativeClass::get_class_static());
			}
			return ClassDB::is_parent_class(src_native, p_target.native_type);
		}
		case GDScriptParser::DataType::SCRIPT:
			if (p_target.is_meta_type) {
				return ClassDB::is_parent_class(src_native, p_target.script_type->get_class_name());
			}
			while (src_script.is_valid()) {
				if (src_script == p_target.script_type) {
					return true;
				}
				src_script = src_script->get_base_script();
			}
			return false;
		case GDScriptParser::DataType::CLASS:
			if (p_target.is_meta_type) {
				return ClassDB::is_parent_class(src_native, GDScript::get_class_static());
			}
			while (src_class != nullptr) {
				if (src_class == p_target.class_type || src_class->fqcn == p_target.class_type->fqcn) {
					return true;
				}
				src_class = src_class->base_type.class_type;
			}
			return false;
		case GDScriptParser::DataType::VARIANT:
		case GDScriptParser::DataType::BUILTIN:
		case GDScriptParser::DataType::ENUM:
		case GDScriptParser::DataType::RESOLVING:
		case GDScriptParser::DataType::UNRESOLVED:
			break; // Already solved before.
	}

	return false;
}

void GDScriptAnalyzer::push_error(const String &p_message, const GDScriptParser::Node *p_origin) {
	mark_node_unsafe(p_origin);
	parser->push_error(p_message, p_origin);
}

void GDScriptAnalyzer::mark_node_unsafe(const GDScriptParser::Node *p_node) {
#ifdef DEBUG_ENABLED
	if (p_node == nullptr) {
		return;
	}

	for (int i = p_node->start_line; i <= p_node->end_line; i++) {
		parser->unsafe_lines.insert(i);
	}
#endif
}

void GDScriptAnalyzer::downgrade_node_type_source(GDScriptParser::Node *p_node) {
	GDScriptParser::IdentifierNode *identifier = nullptr;
	if (p_node->type == GDScriptParser::Node::IDENTIFIER) {
		identifier = static_cast<GDScriptParser::IdentifierNode *>(p_node);
	} else if (p_node->type == GDScriptParser::Node::SUBSCRIPT) {
		GDScriptParser::SubscriptNode *subscript = static_cast<GDScriptParser::SubscriptNode *>(p_node);
		if (subscript->is_attribute) {
			identifier = subscript->attribute;
		}
	}
	if (identifier == nullptr) {
		return;
	}

	GDScriptParser::Node *source = nullptr;
	switch (identifier->source) {
		case GDScriptParser::IdentifierNode::MEMBER_VARIABLE: {
			source = identifier->variable_source;
		} break;
		case GDScriptParser::IdentifierNode::FUNCTION_PARAMETER: {
			source = identifier->parameter_source;
		} break;
		case GDScriptParser::IdentifierNode::LOCAL_VARIABLE: {
			source = identifier->variable_source;
		} break;
		case GDScriptParser::IdentifierNode::LOCAL_ITERATOR: {
			source = identifier->bind_source;
		} break;
		default:
			break;
	}
	if (source == nullptr) {
		return;
	}

	GDScriptParser::DataType datatype;
	datatype.kind = GDScriptParser::DataType::VARIANT;
	source->set_datatype(datatype);
}

void GDScriptAnalyzer::mark_lambda_use_self() {
	GDScriptParser::LambdaNode *lambda = current_lambda;
	while (lambda != nullptr) {
		lambda->use_self = true;
		lambda = lambda->parent_lambda;
	}
}

void GDScriptAnalyzer::resolve_pending_lambda_bodies() {
	if (pending_body_resolution_lambdas.is_empty()) {
		return;
	}

	GDScriptParser::LambdaNode *previous_lambda = current_lambda;
	bool previous_static_context = static_context;

	List<GDScriptParser::LambdaNode *> lambdas = pending_body_resolution_lambdas;
	pending_body_resolution_lambdas.clear();

	for (GDScriptParser::LambdaNode *lambda : lambdas) {
		current_lambda = lambda;
		static_context = lambda->function->is_static;

		resolve_function_body(lambda->function, true);

		int captures_amount = lambda->captures.size();
		if (captures_amount > 0) {
			// Create space for lambda parameters.
			// At the beginning to not mess with optional parameters.
			int param_count = lambda->function->parameters.size();
			lambda->function->parameters.resize(param_count + captures_amount);
			for (int i = param_count - 1; i >= 0; i--) {
				lambda->function->parameters.write[i + captures_amount] = lambda->function->parameters[i];
				lambda->function->parameters_indices[lambda->function->parameters[i]->identifier->name] = i + captures_amount;
			}

			// Add captures as extra parameters at the beginning.
			for (int i = 0; i < lambda->captures.size(); i++) {
				GDScriptParser::IdentifierNode *capture = lambda->captures[i];
				GDScriptParser::ParameterNode *capture_param = parser->alloc_node<GDScriptParser::ParameterNode>();
				capture_param->identifier = capture;
				capture_param->usages = capture->usages;
				capture_param->set_datatype(capture->get_datatype());

				lambda->function->parameters.write[i] = capture_param;
				lambda->function->parameters_indices[capture->name] = i;
			}
		}
	}

	current_lambda = previous_lambda;
	static_context = previous_static_context;
}

bool GDScriptAnalyzer::class_exists(const StringName &p_class) const {
	return ClassDB::class_exists(p_class) && ClassDB::is_class_exposed(p_class);
}

Error GDScriptAnalyzer::resolve_inheritance() {
	return resolve_class_inheritance(parser->head, true);
}

Error GDScriptAnalyzer::resolve_interface() {
	resolve_class_interface(parser->head, true);
	return parser->errors.is_empty() ? OK : ERR_PARSE_ERROR;
}

Error GDScriptAnalyzer::resolve_body() {
	resolve_class_body(parser->head, true);

#ifdef DEBUG_ENABLED
	// Apply here, after all `@warning_ignore`s have been resolved and applied.
	parser->apply_pending_warnings();
#endif

	return parser->errors.is_empty() ? OK : ERR_PARSE_ERROR;
}

Error GDScriptAnalyzer::resolve_dependencies() {
	for (KeyValue<String, Ref<GDScriptParserRef>> &K : parser->depended_parsers) {
		if (K.value.is_null()) {
			return ERR_PARSE_ERROR;
		}
		K.value->raise_status(GDScriptParserRef::INHERITANCE_SOLVED);
	}

	return parser->errors.is_empty() ? OK : ERR_PARSE_ERROR;
}

Error GDScriptAnalyzer::analyze() {
	parser->errors.clear();

	Error err = resolve_inheritance();
	if (err) {
		return err;
	}

	resolve_interface();
	err = resolve_body();
	if (err) {
		return err;
	}

	return resolve_dependencies();
}

GDScriptAnalyzer::GDScriptAnalyzer(GDScriptParser *p_parser) {
	parser = p_parser;
}
