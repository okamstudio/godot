/**************************************************************************/
/*  gdscript_docgen.cpp                                                   */
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

#include "gdscript_docgen.h"

#include "../gdscript.h"

#include "core/config/project_settings.h"

HashMap<String, String> GDScriptDocGen::singletons;

String GDScriptDocGen::_get_script_name(const String &p_path) {
	const HashMap<String, String>::ConstIterator E = singletons.find(p_path);
	if (E) {
		return E->value;
	}
	return p_path.trim_prefix("res://").quote();
}

String GDScriptDocGen::_get_class_name(const GDP::ClassNode &p_class) {
	const GDP::ClassNode *curr_class = &p_class;
	if (!curr_class->identifier) { // All inner classes have an identifier, so this is the outer class.
		return _get_script_name(curr_class->fqcn);
	}

	String full_name = curr_class->identifier->name;
	while (curr_class->outer) {
		curr_class = curr_class->outer;
		if (!curr_class->identifier) { // All inner classes have an identifier, so this is the outer class.
			return vformat("%s.%s", _get_script_name(curr_class->fqcn), full_name);
		}
		full_name = vformat("%s.%s", curr_class->identifier->name, full_name);
	}
	return full_name;
}

void GDScriptDocGen::_doctype_from_gdtype(const GDType &p_gdtype, String &r_type, String &r_enum, bool p_is_return) {
	if (!p_gdtype.is_hard_type()) {
		r_type = "Variant";
		return;
	}
	switch (p_gdtype.kind) {
		case GDType::BUILTIN:
			if (p_gdtype.builtin_type == Variant::NIL) {
				r_type = p_is_return ? "void" : "null";
				return;
			}
			if (p_gdtype.builtin_type == Variant::ARRAY && p_gdtype.has_container_element_type(0)) {
				_doctype_from_gdtype(p_gdtype.get_container_element_type(0), r_type, r_enum);
				if (!r_enum.is_empty()) {
					r_type = "int[]";
					r_enum += "[]";
					return;
				}
				if (!r_type.is_empty() && r_type != "Variant") {
					r_type += "[]";
					return;
				}
			}
			if (p_gdtype.builtin_type == Variant::DICTIONARY && p_gdtype.has_container_element_types()) {
				String key, value;
				_doctype_from_gdtype(p_gdtype.get_container_element_type_or_variant(0), key, r_enum);
				_doctype_from_gdtype(p_gdtype.get_container_element_type_or_variant(1), value, r_enum);
				if (key != "Variant" || value != "Variant") {
					r_type = "Dictionary[" + key + ", " + value + "]";
					return;
				}
			}
			r_type = Variant::get_type_name(p_gdtype.builtin_type);
			return;
		case GDType::NATIVE:
			if (p_gdtype.is_meta_type) {
				//r_type = GDScriptNativeClass::get_class_static();
				r_type = "Object"; // "GDScriptNativeClass" refers to a blank page.
				return;
			}
			r_type = p_gdtype.native_type;
			return;
		case GDType::SCRIPT:
			if (p_gdtype.is_meta_type) {
				r_type = p_gdtype.script_type.is_valid() ? p_gdtype.script_type->get_class() : Script::get_class_static();
				return;
			}
			if (p_gdtype.script_type.is_valid()) {
				if (p_gdtype.script_type->get_global_name() != StringName()) {
					r_type = p_gdtype.script_type->get_global_name();
					return;
				}
				if (!p_gdtype.script_type->get_path().is_empty()) {
					r_type = _get_script_name(p_gdtype.script_type->get_path());
					return;
				}
			}
			if (!p_gdtype.script_path.is_empty()) {
				r_type = _get_script_name(p_gdtype.script_path);
				return;
			}
			r_type = "Object";
			return;
		case GDType::CLASS:
			if (p_gdtype.is_meta_type) {
				r_type = GDScript::get_class_static();
				return;
			}
			r_type = _get_class_name(*p_gdtype.class_type);
			return;
		case GDType::ENUM:
			if (p_gdtype.is_meta_type) {
				r_type = "Dictionary";
				return;
			}
			r_type = "int";
			r_enum = String(p_gdtype.native_type).replace("::", ".");
			if (r_enum.begins_with("res://")) {
				r_enum = r_enum.trim_prefix("res://");
				int dot_pos = r_enum.rfind(".");
				if (dot_pos >= 0) {
					r_enum = r_enum.left(dot_pos).quote() + r_enum.substr(dot_pos);
				}
			}
			return;
		case GDType::VARIANT:
		case GDType::RESOLVING:
		case GDType::UNRESOLVED:
			r_type = "Variant";
			return;
	}
}

String GDScriptDocGen::_docvalue_from_variant(const Variant &p_variant, int p_recursion_level) {
	constexpr int MAX_RECURSION_LEVEL = 2;

	switch (p_variant.get_type()) {
		case Variant::STRING:
			return String(p_variant).c_escape().quote();
		case Variant::OBJECT:
			return "<Object>";
		case Variant::DICTIONARY: {
			const Dictionary dict = p_variant;
			String result;

			if (dict.is_typed()) {
				result += "Dictionary[";

				Ref<Script> key_script = dict.get_typed_key_script();
				if (key_script.is_valid()) {
					if (key_script->get_global_name() != StringName()) {
						result += key_script->get_global_name();
					} else if (!key_script->get_path().get_file().is_empty()) {
						result += key_script->get_path().get_file();
					} else {
						result += dict.get_typed_key_class_name();
					}
				} else if (dict.get_typed_key_class_name() != StringName()) {
					result += dict.get_typed_key_class_name();
				} else if (dict.is_typed_key()) {
					result += Variant::get_type_name((Variant::Type)dict.get_typed_key_builtin());
				} else {
					result += "Variant";
				}

				result += ", ";

				Ref<Script> value_script = dict.get_typed_value_script();
				if (value_script.is_valid()) {
					if (value_script->get_global_name() != StringName()) {
						result += value_script->get_global_name();
					} else if (!value_script->get_path().get_file().is_empty()) {
						result += value_script->get_path().get_file();
					} else {
						result += dict.get_typed_value_class_name();
					}
				} else if (dict.get_typed_value_class_name() != StringName()) {
					result += dict.get_typed_value_class_name();
				} else if (dict.is_typed_value()) {
					result += Variant::get_type_name((Variant::Type)dict.get_typed_value_builtin());
				} else {
					result += "Variant";
				}

				result += "](";
			}

			if (dict.is_empty()) {
				result += "{}";
			} else if (p_recursion_level > MAX_RECURSION_LEVEL) {
				result += "{...}";
			} else {
				result += "{";

				List<Variant> keys;
				dict.get_key_list(&keys);
				keys.sort_custom<StringLikeVariantOrder>();

				for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
					if (E->prev()) {
						result += ", ";
					}
					result += _docvalue_from_variant(E->get(), p_recursion_level + 1) + ": " + _docvalue_from_variant(dict[E->get()], p_recursion_level + 1);
				}

				result += "}";
			}

			if (dict.is_typed()) {
				result += ")";
			}

			return result;
		} break;
		case Variant::ARRAY: {
			const Array array = p_variant;
			String result;

			if (array.is_typed()) {
				result += "Array[";

				Ref<Script> script = array.get_typed_script();
				if (script.is_valid()) {
					if (script->get_global_name() != StringName()) {
						result += script->get_global_name();
					} else if (!script->get_path().get_file().is_empty()) {
						result += script->get_path().get_file();
					} else {
						result += array.get_typed_class_name();
					}
				} else if (array.get_typed_class_name() != StringName()) {
					result += array.get_typed_class_name();
				} else {
					result += Variant::get_type_name((Variant::Type)array.get_typed_builtin());
				}

				result += "](";
			}

			if (array.is_empty()) {
				result += "[]";
			} else if (p_recursion_level > MAX_RECURSION_LEVEL) {
				result += "[...]";
			} else {
				result += "[";

				for (int i = 0; i < array.size(); i++) {
					if (i > 0) {
						result += ", ";
					}
					result += _docvalue_from_variant(array[i], p_recursion_level + 1);
				}

				result += "]";
			}

			if (array.is_typed()) {
				result += ")";
			}

			return result;
		} break;
		default:
			return p_variant.get_construct_string();
	}
}

String GDScriptDocGen::_docvalue_from_expression(const GDP::ExpressionNode *p_expression) {
	ERR_FAIL_NULL_V(p_expression, String());

	if (p_expression->is_constant) {
		return _docvalue_from_variant(p_expression->reduced_value);
	}

	switch (p_expression->type) {
		case GDP::Node::ARRAY: {
			const GDP::ArrayNode *array = static_cast<const GDP::ArrayNode *>(p_expression);
			return array->elements.is_empty() ? "[]" : "[...]";
		} break;
		case GDP::Node::CALL: {
			const GDP::CallNode *call = static_cast<const GDP::CallNode *>(p_expression);
			return call->function_name.operator String() + (call->arguments.is_empty() ? "()" : "(...)");
		} break;
		case GDP::Node::DICTIONARY: {
			const GDP::DictionaryNode *dict = static_cast<const GDP::DictionaryNode *>(p_expression);
			return dict->elements.is_empty() ? "{}" : "{...}";
		} break;
		case GDP::Node::IDENTIFIER: {
			const GDP::IdentifierNode *id = static_cast<const GDP::IdentifierNode *>(p_expression);
			return id->name;
		} break;
		default: {
			return "<unknown>";
		} break;
	}
}

void GDScriptDocGen::_generate_docs(GDScript *p_script, const GDP::ClassNode *p_class) {
	p_script->_clear_doc();

	DocData::ClassDoc &doc = p_script->doc;

	doc.is_script_doc = true;

	if (p_script->local_name == StringName()) {
		// This is an outer unnamed class.
		doc.name = _get_script_name(p_script->get_script_path());
	} else {
		// This is an inner or global outer class.
		doc.name = p_script->local_name;
		if (p_script->_owner) {
			doc.name = p_script->_owner->doc.name + "." + doc.name;
		}
	}

	doc.script_path = p_script->get_script_path();

	if (p_script->base.is_valid() && p_script->base->is_valid()) {
		if (!p_script->base->doc.name.is_empty()) {
			doc.inherits = p_script->base->doc.name;
		} else {
			doc.inherits = p_script->base->get_instance_base_type();
		}
	} else if (p_script->native.is_valid()) {
		doc.inherits = p_script->native->get_name();
	}

	doc.brief_description = p_class->doc_data.brief;
	doc.description = p_class->doc_data.description;
	for (const Pair<String, String> &p : p_class->doc_data.tutorials) {
		DocData::TutorialDoc td;
		td.title = p.first;
		td.link = p.second;
		doc.tutorials.append(td);
	}
	doc.is_deprecated = p_class->doc_data.is_deprecated;
	doc.deprecated_message = p_class->doc_data.deprecated_message;
	doc.is_experimental = p_class->doc_data.is_experimental;
	doc.experimental_message = p_class->doc_data.experimental_message;

	for (const GDP::ClassNode::Member &member : p_class->members) {
		switch (member.type) {
			case GDP::ClassNode::Member::CLASS: {
				const GDP::ClassNode *inner_class = member.m_class;
				const StringName &class_name = inner_class->identifier->name;

				p_script->member_lines[class_name] = inner_class->start_line;

				// Recursively generate inner class docs.
				// Needs inner GDScripts to exist: previously generated in GDScriptCompiler::make_scripts().
				GDScriptDocGen::_generate_docs(*p_script->subclasses[class_name], inner_class);
			} break;

			case GDP::ClassNode::Member::CONSTANT: {
				const GDP::ConstantNode *m_const = member.constant;
				const StringName &const_name = member.constant->identifier->name;

				p_script->member_lines[const_name] = m_const->start_line;

				DocData::ConstantDoc const_doc;
				const_doc.name = const_name;
				const_doc.value = _docvalue_from_variant(m_const->initializer->reduced_value);
				const_doc.is_value_valid = true;
				const_doc.description = m_const->doc_data.description;
				const_doc.is_deprecated = m_const->doc_data.is_deprecated;
				const_doc.deprecated_message = m_const->doc_data.deprecated_message;
				const_doc.is_experimental = m_const->doc_data.is_experimental;
				const_doc.experimental_message = m_const->doc_data.experimental_message;
				doc.constants.push_back(const_doc);
			} break;

			case GDP::ClassNode::Member::FUNCTION: {
				const GDP::FunctionNode *m_func = member.function;
				const StringName &func_name = m_func->identifier->name;

				p_script->member_lines[func_name] = m_func->start_line;

				DocData::MethodDoc method_doc;
				method_doc.name = func_name;
				method_doc.description = m_func->doc_data.description;
				method_doc.is_deprecated = m_func->doc_data.is_deprecated;
				method_doc.deprecated_message = m_func->doc_data.deprecated_message;
				method_doc.is_experimental = m_func->doc_data.is_experimental;
				method_doc.experimental_message = m_func->doc_data.experimental_message;
				method_doc.qualifiers = m_func->is_static ? "static" : "";

				if (m_func->return_type) {
					// `m_func->return_type->get_datatype()` is a metatype.
					_doctype_from_gdtype(m_func->get_datatype(), method_doc.return_type, method_doc.return_enum, true);
				} else if (!m_func->body->has_return) {
					// If no `return` statement, then return type is `void`, not `Variant`.
					method_doc.return_type = "void";
				} else {
					method_doc.return_type = "Variant";
				}

				for (const GDP::ParameterNode *p : m_func->parameters) {
					DocData::ArgumentDoc arg_doc;
					arg_doc.name = p->identifier->name;
					_doctype_from_gdtype(p->get_datatype(), arg_doc.type, arg_doc.enumeration);
					if (p->initializer != nullptr) {
						arg_doc.default_value = _docvalue_from_expression(p->initializer);
					}
					method_doc.arguments.push_back(arg_doc);
				}

				doc.methods.push_back(method_doc);
			} break;

			case GDP::ClassNode::Member::SIGNAL: {
				const GDP::SignalNode *m_signal = member.signal;
				const StringName &signal_name = m_signal->identifier->name;

				p_script->member_lines[signal_name] = m_signal->start_line;

				DocData::MethodDoc signal_doc;
				signal_doc.name = signal_name;
				signal_doc.description = m_signal->doc_data.description;
				signal_doc.is_deprecated = m_signal->doc_data.is_deprecated;
				signal_doc.deprecated_message = m_signal->doc_data.deprecated_message;
				signal_doc.is_experimental = m_signal->doc_data.is_experimental;
				signal_doc.experimental_message = m_signal->doc_data.experimental_message;

				for (const GDP::ParameterNode *p : m_signal->parameters) {
					DocData::ArgumentDoc arg_doc;
					arg_doc.name = p->identifier->name;
					_doctype_from_gdtype(p->get_datatype(), arg_doc.type, arg_doc.enumeration);
					signal_doc.arguments.push_back(arg_doc);
				}

				doc.signals.push_back(signal_doc);
			} break;

			case GDP::ClassNode::Member::VARIABLE: {
				const GDP::VariableNode *m_var = member.variable;
				const StringName &var_name = m_var->identifier->name;

				p_script->member_lines[var_name] = m_var->start_line;

				DocData::PropertyDoc prop_doc;
				prop_doc.name = var_name;
				prop_doc.description = m_var->doc_data.description;
				prop_doc.is_deprecated = m_var->doc_data.is_deprecated;
				prop_doc.deprecated_message = m_var->doc_data.deprecated_message;
				prop_doc.is_experimental = m_var->doc_data.is_experimental;
				prop_doc.experimental_message = m_var->doc_data.experimental_message;
				_doctype_from_gdtype(m_var->get_datatype(), prop_doc.type, prop_doc.enumeration);

				switch (m_var->property) {
					case GDP::VariableNode::PROP_NONE:
						break;
					case GDP::VariableNode::PROP_INLINE:
						if (m_var->setter != nullptr) {
							prop_doc.setter = m_var->setter->identifier->name;
						}
						if (m_var->getter != nullptr) {
							prop_doc.getter = m_var->getter->identifier->name;
						}
						break;
					case GDP::VariableNode::PROP_SETGET:
						if (m_var->setter_pointer != nullptr) {
							prop_doc.setter = m_var->setter_pointer->name;
						}
						if (m_var->getter_pointer != nullptr) {
							prop_doc.getter = m_var->getter_pointer->name;
						}
						break;
				}

				if (m_var->initializer != nullptr) {
					prop_doc.default_value = _docvalue_from_expression(m_var->initializer);
				}

				prop_doc.overridden = false;

				doc.properties.push_back(prop_doc);
			} break;

			case GDP::ClassNode::Member::ENUM: {
				const GDP::EnumNode *m_enum = member.m_enum;
				StringName name = m_enum->identifier->name;

				p_script->member_lines[name] = m_enum->start_line;

				DocData::EnumDoc enum_doc;
				enum_doc.description = m_enum->doc_data.description;
				enum_doc.is_deprecated = m_enum->doc_data.is_deprecated;
				enum_doc.deprecated_message = m_enum->doc_data.deprecated_message;
				enum_doc.is_experimental = m_enum->doc_data.is_experimental;
				enum_doc.experimental_message = m_enum->doc_data.experimental_message;
				doc.enums[name] = enum_doc;

				for (const GDP::EnumNode::Value &val : m_enum->values) {
					DocData::ConstantDoc const_doc;
					const_doc.name = val.identifier->name;
					const_doc.value = _docvalue_from_variant(val.value);
					const_doc.is_value_valid = true;
					const_doc.enumeration = name;
					const_doc.description = val.doc_data.description;
					const_doc.is_deprecated = val.doc_data.is_deprecated;
					const_doc.deprecated_message = val.doc_data.deprecated_message;
					const_doc.is_experimental = val.doc_data.is_experimental;
					const_doc.experimental_message = val.doc_data.experimental_message;

					doc.constants.push_back(const_doc);
				}

			} break;

			case GDP::ClassNode::Member::ENUM_VALUE: {
				const GDP::EnumNode::Value &m_enum_val = member.enum_value;
				const StringName &name = m_enum_val.identifier->name;

				p_script->member_lines[name] = m_enum_val.identifier->start_line;

				DocData::ConstantDoc const_doc;
				const_doc.name = name;
				const_doc.value = _docvalue_from_variant(m_enum_val.value);
				const_doc.is_value_valid = true;
				const_doc.enumeration = "@unnamed_enums";
				const_doc.description = m_enum_val.doc_data.description;
				const_doc.is_deprecated = m_enum_val.doc_data.is_deprecated;
				const_doc.deprecated_message = m_enum_val.doc_data.deprecated_message;
				const_doc.is_experimental = m_enum_val.doc_data.is_experimental;
				const_doc.experimental_message = m_enum_val.doc_data.experimental_message;
				doc.constants.push_back(const_doc);
			} break;

			default:
				break;
		}
	}

	// Add doc to the outer-most class.
	p_script->_add_doc(doc);
}

void GDScriptDocGen::generate_docs(GDScript *p_script, const GDP::ClassNode *p_class) {
	for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : ProjectSettings::get_singleton()->get_autoload_list()) {
		if (E.value.is_singleton) {
			singletons[E.value.path] = E.key;
		}
	}
	_generate_docs(p_script, p_class);
	singletons.clear();
}
