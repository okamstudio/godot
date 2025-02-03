/**************************************************************************/
/*  gdscript_compiler.cpp                                                 */
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

#include "gdscript_compiler.h"

#include "gdscript.h"

//#define DEBUG_CHAINED_SETGET
#ifdef DEBUG_CHAINED_SETGET
#include <stdio.h>

static String to_string(const GDScriptCodeGenerator::Address &p_address) {
	using AddressMode = GDScriptCodeGenerator::Address::AddressMode;
	String str;
	switch (p_address.mode) {
		case AddressMode::SELF: {
			str += "self";
		} break;
		case AddressMode::CLASS: {
			str += "class";
		} break;
		case AddressMode::MEMBER: {
			str += "member";
		} break;
		case AddressMode::CLASS_CONSTANT: {
			str += "class_constant";
		} break;
		case AddressMode::LOCAL_CONSTANT: {
			str += "local_constant";
		} break;
		case AddressMode::STACK: {
			str += "stack";
		} break;
		case AddressMode::STACK_VARIABLE: {
			str += "stack_variable";
		} break;
		case AddressMode::GLOBAL: {
			str += "global";
		} break;
		case AddressMode::NAMED_GLOBAL: {
			str += "named_global";
		} break;
		case AddressMode::NIL: {
			str += "named_global";
		} break;
		default: {
			str += "???"; // unknown addressing mode
		} break;
	}
	str += "(";
	str += itos(p_address.address);
	str += ")";
	return str;
}
#endif

#define _ADJUST_STACK_LEVEL_OR_RETURN_V(m_codegen, m_address, m_slevel, m_bad_address_return, m_adjust_code) \
	if (true) {                                                                                              \
		if (!m_address) {                                                                                    \
			return m_bad_address_return;                                                                     \
		}                                                                                                    \
		if (m_codegen.adjust_stack_level(m_address, m_slevel)) {                                             \
			m_adjust_code;                                                                                   \
		}                                                                                                    \
	} else                                                                                                   \
		((void)0)

// if the address is a stack address that is above the current stack level then
// adjust the stack level to be the next immediate address.
// NOTE: if the supplied address is invalid then the calling function will return that address.
#define ADJUST_STACK_LEVEL_OR_RETURN(m_codegen, m_address, m_slevel) \
	_ADJUST_STACK_LEVEL_OR_RETURN_V(m_codegen, m_address, m_slevel, m_address, {})
// if the address is a stack address that is above the current stack level then
// adjust the stack level to be the next immediate address.
// NOTE: if the supplied address is invalid then the calling function will return false.
#define ADJUST_STACK_LEVEL_OR_RETURN_FALSE(m_codegen, m_address, m_slevel) \
	_ADJUST_STACK_LEVEL_OR_RETURN_V(m_codegen, m_address, m_slevel, false, {})
// if the address is a stack address that is above the current stack level then
// adjust the stack level to be the next immediate address and allocate up to it for use.
// NOTE: if the supplied address is invalid then the calling function will return.
#define ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(m_codegen, m_address, m_slevel) \
	_ADJUST_STACK_LEVEL_OR_RETURN_V(m_codegen, m_address, m_slevel, m_address, m_codegen.alloc_stack(m_slevel))

//

bool GDScriptCompiler::_is_class_member_property(CodeGen &codegen, const StringName &p_name) {
	if (codegen.function_node && codegen.function_node->_static) {
		return false;
	}

	if (codegen.stack_identifiers.has(p_name)) {
		return false; //shadowed
	}

	return _is_class_member_property(codegen.script, p_name);
}

bool GDScriptCompiler::_is_class_member_property(GDScript *owner, const StringName &p_name) {
	GDScript *scr = owner;
	GDScriptNativeClass *nc = nullptr;
	while (scr) {
		if (scr->native.is_valid()) {
			nc = scr->native.ptr();
		}
		scr = scr->_base;
	}

	ERR_FAIL_COND_V(!nc, false);

	return ClassDB::has_property(nc->get_name(), p_name);
}

void GDScriptCompiler::_set_error(const String &p_error, const GDScriptParser::Node *p_node) {
	if (error != "") {
		return;
	}

	error = p_error;
	if (p_node) {
		err_line = p_node->line;
		err_column = p_node->column;
	} else {
		err_line = 0;
		err_column = 0;
	}
}

bool GDScriptCompiler::_create_unary_operator(CodeGen &codegen, const GDScriptParser::OperatorNode *on, Variant::Operator op, int p_stack_level) {
	ERR_FAIL_COND_V(on->arguments.size() != 1, false);

	GDScriptCodeGenerator::Address src_address_a = _parse_expression(codegen, on->arguments[0], p_stack_level);
	if (!src_address_a) {
		return false;
	}

	codegen.append(GDScriptFunction::OPCODE_OPERATOR); // perform operator
	codegen.append(op); //which operator
	codegen.append(src_address_a); // argument 1
	codegen.append(src_address_a); // argument 2 (repeated)
	//codegen.append(NIL_ADDRESS); // argument 2 (unary only takes one parameter)
	return true;
}

bool GDScriptCompiler::_create_binary_operator(CodeGen &codegen, const GDScriptParser::OperatorNode *on, Variant::Operator op, const int p_stack_level, bool p_initializer, GDScriptCodeGenerator::Address p_index_addr) {
	ERR_FAIL_COND_V(on->arguments.size() != 2, false);

	int slevel = p_stack_level;

	GDScriptCodeGenerator::Address src_address_a = _parse_expression(codegen, on->arguments[0], slevel, false, p_initializer, p_index_addr);
	ADJUST_STACK_LEVEL_OR_RETURN_FALSE(codegen, src_address_a, slevel); //uses stack for return, increase stack

	GDScriptCodeGenerator::Address src_address_b = _parse_expression(codegen, on->arguments[1], slevel, false, p_initializer);
	ADJUST_STACK_LEVEL_OR_RETURN_FALSE(codegen, src_address_b, slevel); //uses stack for return, increase stack

	codegen.append(GDScriptFunction::OPCODE_OPERATOR); // perform operator
	codegen.append(op); //which operator
	codegen.append(src_address_a); // argument 1
	codegen.append(src_address_b); // argument 2 (unary only takes one parameter)
	return true;
}

GDScriptDataType GDScriptCompiler::_gdtype_from_datatype(const GDScriptParser::DataType &p_datatype, GDScript *p_owner) const {
	if (!p_datatype.has_type) {
		return GDScriptDataType();
	}

	GDScriptDataType result;
	result.has_type = true;

	switch (p_datatype.kind) {
		case GDScriptParser::DataType::BUILTIN: {
			result.kind = GDScriptDataType::BUILTIN;
			result.builtin_type = p_datatype.builtin_type;
		} break;
		case GDScriptParser::DataType::NATIVE: {
			result.kind = GDScriptDataType::NATIVE;
			result.native_type = p_datatype.native_type;
		} break;
		case GDScriptParser::DataType::SCRIPT: {
			result.kind = GDScriptDataType::SCRIPT;
			result.script_type_ref = Ref<Script>(p_datatype.script_type);
			result.script_type = result.script_type_ref.ptr();
			result.native_type = result.script_type->get_instance_base_type();
		} break;
		case GDScriptParser::DataType::GDSCRIPT: {
			result.kind = GDScriptDataType::GDSCRIPT;
			result.script_type_ref = Ref<Script>(p_datatype.script_type);
			result.script_type = result.script_type_ref.ptr();
			result.native_type = result.script_type->get_instance_base_type();
		} break;
		case GDScriptParser::DataType::CLASS: {
			// Locate class by constructing the path to it and following that path
			GDScriptParser::ClassNode *class_type = p_datatype.class_type;
			List<StringName> names;
			while (class_type->owner) {
				names.push_back(class_type->name);
				class_type = class_type->owner;
			}

			Ref<GDScript> script = Ref<GDScript>(main_script);
			while (names.back()) {
				if (!script->subclasses.has(names.back()->get())) {
					ERR_PRINT("Parser bug: Cannot locate datatype class.");
					result.has_type = false;
					return GDScriptDataType();
				}
				script = script->subclasses[names.back()->get()];
				names.pop_back();
			}

			result.kind = GDScriptDataType::GDSCRIPT;
			result.script_type_ref = Ref<Script>(script);
			result.script_type = result.script_type_ref.ptr();
			result.native_type = script->get_instance_base_type();
		} break;
		default: {
			ERR_PRINT("Parser bug: converting unresolved type.");
			return GDScriptDataType();
		}
	}

	// Only hold strong reference to the script if it's not the owner of the
	// element qualified with this type, to avoid cyclic references (leaks).
	if (result.script_type && result.script_type == p_owner) {
		result.script_type_ref = Ref<Script>();
	}

	return result;
}

GDScriptCodeGenerator::Address GDScriptCompiler::_parse_assign_right_expression(CodeGen &codegen, const GDScriptParser::OperatorNode *p_expression, int p_stack_level, GDScriptCodeGenerator::Address p_index_addr) {
	Variant::Operator var_op = Variant::OP_MAX;

	switch (p_expression->op) {
		case GDScriptParser::OperatorNode::OP_ASSIGN_ADD:
			var_op = Variant::OP_ADD;
			break;
		case GDScriptParser::OperatorNode::OP_ASSIGN_SUB:
			var_op = Variant::OP_SUBTRACT;
			break;
		case GDScriptParser::OperatorNode::OP_ASSIGN_MUL:
			var_op = Variant::OP_MULTIPLY;
			break;
		case GDScriptParser::OperatorNode::OP_ASSIGN_DIV:
			var_op = Variant::OP_DIVIDE;
			break;
		case GDScriptParser::OperatorNode::OP_ASSIGN_MOD:
			var_op = Variant::OP_MODULE;
			break;
		case GDScriptParser::OperatorNode::OP_ASSIGN_SHIFT_LEFT:
			var_op = Variant::OP_SHIFT_LEFT;
			break;
		case GDScriptParser::OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
			var_op = Variant::OP_SHIFT_RIGHT;
			break;
		case GDScriptParser::OperatorNode::OP_ASSIGN_BIT_AND:
			var_op = Variant::OP_BIT_AND;
			break;
		case GDScriptParser::OperatorNode::OP_ASSIGN_BIT_OR:
			var_op = Variant::OP_BIT_OR;
			break;
		case GDScriptParser::OperatorNode::OP_ASSIGN_BIT_XOR:
			var_op = Variant::OP_BIT_XOR;
			break;
		case GDScriptParser::OperatorNode::OP_INIT_ASSIGN:
		case GDScriptParser::OperatorNode::OP_ASSIGN: {
			//none
		} break;
		default: {
			ERR_FAIL_V(GDScriptCodeGenerator::Address::INVALID);
		}
	}

	bool initializer = p_expression->op == GDScriptParser::OperatorNode::OP_INIT_ASSIGN;

	if (var_op == Variant::OP_MAX) {
		return _parse_expression(codegen, p_expression->arguments[1], p_stack_level, false, initializer);
	}

	if (!_create_binary_operator(codegen, p_expression, var_op, p_stack_level, initializer, p_index_addr)) {
		return GDScriptCodeGenerator::Address::INVALID;
	}

	auto dst_addr = GDScriptCodeGenerator::StackAddress(p_stack_level);
	codegen.append(dst_addr); // append the stack level as destination address of the opcode
	codegen.alloc_stack(p_stack_level);
	return dst_addr;
}

GDScriptCodeGenerator::Address GDScriptCompiler::_parse_expression(CodeGen &codegen, const GDScriptParser::Node *p_expression, const int p_stack_level, bool p_root, bool p_initializer, GDScriptCodeGenerator::Address p_index_addr) {
	switch (p_expression->type) {
		//should parse variable declaration and adjust stack accordingly...
		case GDScriptParser::Node::TYPE_IDENTIFIER: {
			//return identifier
			//wait, identifier could be a local variable or something else... careful here, must reference properly
			//as stack may be more interesting to work with

			//This could be made much simpler by just indexing "self", but done this way (with custom self-addressing modes) increases performance a lot.

			const GDScriptParser::IdentifierNode *in = static_cast<const GDScriptParser::IdentifierNode *>(p_expression);

			StringName identifier = in->name;

			// TRY STACK!
			if (!p_initializer && codegen.stack_identifiers.has(identifier)) {
				return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::STACK_VARIABLE, codegen.stack_identifiers[identifier]);
			}

			// TRY CLASS MEMBER
			if (_is_class_member_property(codegen, identifier)) {
				//get property
				codegen.append(GDScriptFunction::OPCODE_GET_MEMBER); // perform operator
				codegen.append(codegen.get_name_map_pos(identifier)); // argument 2 (unary only takes one parameter)
				auto dst_addr = GDScriptCodeGenerator::StackAddress(p_stack_level);
				codegen.append(dst_addr); // append the stack level as destination address of the opcode
				codegen.alloc_stack(p_stack_level);
				return dst_addr;
			}

			//TRY MEMBERS!
			if (!codegen.function_node || !codegen.function_node->_static) {
				// TRY MEMBER VARIABLES!
				//static function
				if (codegen.script->member_indices.has(identifier)) {
					return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::MEMBER, codegen.script->member_indices[identifier].index); // argument (stack root)
				}
			}

			//TRY CLASS CONSTANTS

			GDScript *owner = codegen.script;
			while (owner) {
				GDScript *scr = owner;
				GDScriptNativeClass *nc = nullptr;
				while (scr) {
					if (scr->constants.has(identifier)) {
						//int idx=scr->constants[identifier];
						int idx = codegen.get_name_map_pos(identifier);
						return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::CLASS_CONSTANT, idx); // argument (stack root)
					}
					if (scr->native.is_valid()) {
						nc = scr->native.ptr();
					}
					scr = scr->_base;
				}

				// CLASS C++ Integer Constant

				if (nc) {
					bool success = false;
					int constant = ClassDB::get_integer_constant(nc->get_name(), identifier, &success);
					if (success) {
						Variant key = constant;
						int idx;

						if (!codegen.constant_map.has(key)) {
							idx = codegen.constant_map.size();
							codegen.constant_map[key] = idx;
						} else {
							idx = codegen.constant_map[key];
						}

						return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::LOCAL_CONSTANT, idx); //make it a local constant (faster access)
					}
				}

				owner = owner->_owner;
			}

			if (GDScriptLanguage::get_singleton()->get_global_map().has(identifier)) {
				int idx = GDScriptLanguage::get_singleton()->get_global_map()[identifier];
				return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::GLOBAL, idx); //argument (stack root)
			}

			/* TRY GLOBAL CLASSES */

			if (ScriptServer::is_global_class(identifier)) {
				const GDScriptParser::ClassNode *class_node = codegen.class_node;
				while (class_node->owner) {
					class_node = class_node->owner;
				}

				if (class_node->name == identifier) {
					_set_error("Using own name in class file is not allowed (creates a cyclic reference)", p_expression);
					return GDScriptCodeGenerator::Address::INVALID;
				}

				RES res = ResourceLoader::load(ScriptServer::get_global_class_path(identifier));
				if (res.is_null()) {
					_set_error("Can't load global class " + String(identifier) + ", cyclic reference?", p_expression);
					return GDScriptCodeGenerator::Address::INVALID;
				}

				Variant key = res;
				int idx;

				if (!codegen.constant_map.has(key)) {
					idx = codegen.constant_map.size();
					codegen.constant_map[key] = idx;

				} else {
					idx = codegen.constant_map[key];
				}

				return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::LOCAL_CONSTANT, idx); //make it a local constant (faster access)
			}

#ifdef TOOLS_ENABLED
			if (GDScriptLanguage::get_singleton()->get_named_globals_map().has(identifier)) {
				int idx = codegen.named_globals.find(identifier);
				if (idx == -1) {
					idx = codegen.named_globals.size();
					codegen.named_globals.push_back(identifier);
				}
				return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::NAMED_GLOBAL, idx);
			}
#endif

			//not found, error

			_set_error("Identifier not found: " + String(identifier), p_expression);
			return GDScriptCodeGenerator::Address::INVALID;

		} break;
		case GDScriptParser::Node::TYPE_CONSTANT: {
			//return constant
			const GDScriptParser::ConstantNode *cn = static_cast<const GDScriptParser::ConstantNode *>(p_expression);

			int idx;

			if (!codegen.constant_map.has(cn->value)) {
				idx = codegen.constant_map.size();
				codegen.constant_map[cn->value] = idx;

			} else {
				idx = codegen.constant_map[cn->value];
			}

			return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::LOCAL_CONSTANT, idx); //argument (stack root)

		} break;
		case GDScriptParser::Node::TYPE_SELF: {
			//return constant
			if (codegen.function_node && codegen.function_node->_static) {
				_set_error("'self' not present in static function!", p_expression);
				return GDScriptCodeGenerator::Address::INVALID;
			}
			return GDScriptCodeGenerator::Address::SELF;

		} break;
		case GDScriptParser::Node::TYPE_ARRAY: {
			const GDScriptParser::ArrayNode *an = static_cast<const GDScriptParser::ArrayNode *>(p_expression);
			Vector<GDScriptCodeGenerator::Address> values;

			int slevel = p_stack_level;

			for (int i = 0; i < an->elements.size(); i++) {
				auto ret = _parse_expression(codegen, an->elements[i], slevel);
				ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, ret, slevel);
				values.push_back(ret);
			}

			codegen.append(GDScriptFunction::OPCODE_CONSTRUCT_ARRAY);
			codegen.append(values.size());
			for (int i = 0; i < values.size(); i++) {
				codegen.append(values[i]);
			}

			auto dst_addr = GDScriptCodeGenerator::StackAddress(p_stack_level);
			codegen.append(dst_addr); // append the stack level as destination address of the opcode
			codegen.alloc_stack(p_stack_level);
			return dst_addr;

		} break;
		case GDScriptParser::Node::TYPE_DICTIONARY: {
			const GDScriptParser::DictionaryNode *dn = static_cast<const GDScriptParser::DictionaryNode *>(p_expression);
			Vector<GDScriptCodeGenerator::Address> values;

			int slevel = p_stack_level;

			for (int i = 0; i < dn->elements.size(); i++) {
				auto ret = _parse_expression(codegen, dn->elements[i].key, slevel);
				ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, ret, slevel);
				values.push_back(ret);

				ret = _parse_expression(codegen, dn->elements[i].value, slevel);
				ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, ret, slevel);
				values.push_back(ret);
			}

			codegen.append(GDScriptFunction::OPCODE_CONSTRUCT_DICTIONARY);
			codegen.append(dn->elements.size());
			for (int i = 0; i < values.size(); i++) {
				codegen.append(values[i]);
			}

			auto dst_addr = GDScriptCodeGenerator::StackAddress(p_stack_level);
			codegen.append(dst_addr); // append the stack level as destination address of the opcode
			codegen.alloc_stack(p_stack_level);
			return dst_addr;

		} break;
		case GDScriptParser::Node::TYPE_CAST: {
			const GDScriptParser::CastNode *cn = static_cast<const GDScriptParser::CastNode *>(p_expression);

			int slevel = p_stack_level;
			auto src_addr = _parse_expression(codegen, cn->source_node, slevel);
			ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, src_addr, slevel);

			GDScriptDataType cast_type = _gdtype_from_datatype(cn->cast_type);

			switch (cast_type.kind) {
				case GDScriptDataType::BUILTIN: {
					codegen.append(GDScriptFunction::OPCODE_CAST_TO_BUILTIN);
					codegen.append(cast_type.builtin_type);
				} break;
				case GDScriptDataType::NATIVE: {
					if (!GDScriptLanguage::get_singleton()->get_global_map().has(cast_type.native_type)) {
						_set_error("Invalid native class type '" + String(cast_type.native_type) + "'.", cn);
						return GDScriptCodeGenerator::Address::INVALID;
					}
					int class_idx = GDScriptLanguage::get_singleton()->get_global_map()[cast_type.native_type];
					codegen.append(GDScriptFunction::OPCODE_CAST_TO_NATIVE); // perform operator
					codegen.append(GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::GLOBAL, class_idx)); // variable type
				} break;
				case GDScriptDataType::SCRIPT:
				case GDScriptDataType::GDSCRIPT: {
					Variant script = cast_type.script_type;
					int idx = codegen.get_constant_pos(script); // make it a local constant for faster access
					codegen.append(GDScriptFunction::OPCODE_CAST_TO_SCRIPT); // perform operator
					codegen.append(GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::LOCAL_CONSTANT, idx)); // variable type
				} break;
				default: {
					_set_error("Parser bug: unresolved data type.", cn);
					return GDScriptCodeGenerator::Address::INVALID;
				}
			}

			codegen.append(src_addr); // source address
			auto dst_addr = GDScriptCodeGenerator::StackAddress(p_stack_level);
			codegen.append(dst_addr); // append the stack level as destination address of the opcode
			codegen.alloc_stack(p_stack_level);
			return dst_addr;

		} break;
		case GDScriptParser::Node::TYPE_OPERATOR: {
			//hell breaks loose

			const GDScriptParser::OperatorNode *on = static_cast<const GDScriptParser::OperatorNode *>(p_expression);
			switch (on->op) {
				//call/constructor operator
				case GDScriptParser::OperatorNode::OP_PARENT_CALL: {
					ERR_FAIL_COND_V(on->arguments.size() < 1, GDScriptCodeGenerator::Address::INVALID);

					const GDScriptParser::IdentifierNode *in = (const GDScriptParser::IdentifierNode *)on->arguments[0];

					Vector<GDScriptCodeGenerator::Address> arguments;
					int slevel = p_stack_level;
					for (int i = 1; i < on->arguments.size(); i++) {
						auto ret = _parse_expression(codegen, on->arguments[i], slevel);
						ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, ret, slevel);
						arguments.push_back(ret);
					}

					//push call bytecode
					codegen.append(GDScriptFunction::OPCODE_CALL_SELF_BASE); // basic type constructor

					codegen.append(codegen.get_name_map_pos(in->name)); //instance
					codegen.append(arguments.size()); //argument count
					codegen.alloc_call(arguments.size());
					for (int i = 0; i < arguments.size(); i++) {
						codegen.append(arguments[i]); //arguments
					}

				} break;
				case GDScriptParser::OperatorNode::OP_CALL: {
					if (on->arguments[0]->type == GDScriptParser::Node::TYPE_TYPE) {
						//construct a basic type
						ERR_FAIL_COND_V(on->arguments.size() < 1, GDScriptCodeGenerator::Address::INVALID);

						const GDScriptParser::TypeNode *tn = (const GDScriptParser::TypeNode *)on->arguments[0];
						int vtype = tn->vtype;

						Vector<GDScriptCodeGenerator::Address> arguments;
						int slevel = p_stack_level;
						for (int i = 1; i < on->arguments.size(); i++) {
							auto ret = _parse_expression(codegen, on->arguments[i], slevel);
							ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, ret, slevel);
							arguments.push_back(ret);
						}

						//push call bytecode
						codegen.append(GDScriptFunction::OPCODE_CONSTRUCT); // basic type constructor
						codegen.append(vtype); //instance
						codegen.append(arguments.size()); //argument count
						codegen.alloc_call(arguments.size());
						for (int i = 0; i < arguments.size(); i++) {
							codegen.append(arguments[i]); //arguments
						}

					} else if (on->arguments[0]->type == GDScriptParser::Node::TYPE_BUILT_IN_FUNCTION) {
						//built in function

						ERR_FAIL_COND_V(on->arguments.size() < 1, GDScriptCodeGenerator::Address::INVALID);

						Vector<GDScriptCodeGenerator::Address> arguments;
						int slevel = p_stack_level;
						for (int i = 1; i < on->arguments.size(); i++) {
							auto ret = _parse_expression(codegen, on->arguments[i], slevel);
							ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, ret, slevel);
							arguments.push_back(ret);
						}

						codegen.append(GDScriptFunction::OPCODE_CALL_BUILT_IN);
						codegen.append(static_cast<const GDScriptParser::BuiltInFunctionNode *>(on->arguments[0])->function);
						codegen.append(on->arguments.size() - 1);
						codegen.alloc_call(on->arguments.size() - 1);
						for (int i = 0; i < arguments.size(); i++) {
							codegen.append(arguments[i]);
						}

					} else {
						//regular function
						ERR_FAIL_COND_V(on->arguments.size() < 2, GDScriptCodeGenerator::Address::INVALID);

						const GDScriptParser::Node *instance = on->arguments[0];

						if (instance->type == GDScriptParser::Node::TYPE_SELF) {
							//room for optimization
						}

						Vector<int> arguments;
						int slevel = p_stack_level;

						for (int i = 0; i < on->arguments.size(); i++) {
							GDScriptCodeGenerator::Address ret = GDScriptCodeGenerator::Address::INVALID;

							if (i == 0 && on->arguments[i]->type == GDScriptParser::Node::TYPE_SELF && codegen.function_node && codegen.function_node->_static) {
								ret = GDScriptCodeGenerator::Address::CLASS; // static call to self
								arguments.push_back(codegen.address_of(ret));
							} else if (i == 1) {
								if (on->arguments[i]->type != GDScriptParser::Node::TYPE_IDENTIFIER) {
									_set_error("Attempt to call a non-identifier.", on);
									return GDScriptCodeGenerator::Address::INVALID;
								}
								GDScriptParser::IdentifierNode *id = static_cast<GDScriptParser::IdentifierNode *>(on->arguments[i]);
								arguments.push_back(codegen.get_name_map_pos(id->name));
							} else {
								ret = _parse_expression(codegen, on->arguments[i], slevel);
								ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, ret, slevel);
								arguments.push_back(codegen.address_of(ret));
							}
						}

						codegen.append(p_root ? GDScriptFunction::OPCODE_CALL : GDScriptFunction::OPCODE_CALL_RETURN); // perform operator
						codegen.append(on->arguments.size() - 2);
						codegen.alloc_call(on->arguments.size() - 2);
						for (int i = 0; i < arguments.size(); i++) {
							codegen.append(arguments[i]);
						}
					}
				} break;
				case GDScriptParser::OperatorNode::OP_YIELD: {
					ERR_FAIL_COND_V(on->arguments.size() && on->arguments.size() != 2, GDScriptCodeGenerator::Address::INVALID);

					Vector<GDScriptCodeGenerator::Address> arguments;
					int slevel = p_stack_level;
					for (int i = 0; i < on->arguments.size(); i++) {
						auto ret = _parse_expression(codegen, on->arguments[i], slevel);
						ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, ret, slevel);
						arguments.push_back(ret);
					}

					//push call bytecode
					codegen.append(arguments.size() == 0 ? GDScriptFunction::OPCODE_YIELD : GDScriptFunction::OPCODE_YIELD_SIGNAL); // basic type constructor
					for (int i = 0; i < arguments.size(); i++) {
						codegen.append(arguments[i]); //arguments
					}
					codegen.append(GDScriptFunction::OPCODE_YIELD_RESUME);
					//next will be where to place the result :)

				} break;

				//indexing operator
				case GDScriptParser::OperatorNode::OP_INDEX:
				case GDScriptParser::OperatorNode::OP_INDEX_NAMED: {
					ERR_FAIL_COND_V(on->arguments.size() != 2, GDScriptCodeGenerator::Address::INVALID);

					int slevel = p_stack_level;
					bool is_named = (on->op == GDScriptParser::OperatorNode::OP_INDEX_NAMED);
					StringName name;

					auto from = _parse_expression(codegen, on->arguments[0], slevel);
					if (!from) {
						return from;
					}

					GDScriptCodeGenerator::Address index = GDScriptCodeGenerator::Address::INVALID;
					if (p_index_addr.mode != GDScriptCodeGenerator::Address::INVALID) {
						index = p_index_addr;
					} else if (is_named) {
						if (on->arguments[0]->type == GDScriptParser::Node::TYPE_SELF && codegen.script && codegen.function_node && !codegen.function_node->_static) {
							GDScriptParser::IdentifierNode *identifier = static_cast<GDScriptParser::IdentifierNode *>(on->arguments[1]);
							const Map<StringName, GDScript::MemberInfo>::Element *MI = codegen.script->member_indices.find(identifier->name);

#ifdef DEBUG_ENABLED
							if (MI && MI->get().getter == codegen.function_node->name) {
								String n = static_cast<GDScriptParser::IdentifierNode *>(on->arguments[1])->name;
								_set_error("Must use '" + n + "' instead of 'self." + n + "' in getter.", on);
								return GDScriptCodeGenerator::Address::INVALID;
							}
#endif

							if (MI && MI->get().getter == "") {
								// Faster than indexing self (as if no self. had been used)
								return GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::MEMBER, MI->get().index);
							}
						}

						name = static_cast<GDScriptParser::IdentifierNode *>(on->arguments[1])->name;

					} else {
						if (on->arguments[1]->type == GDScriptParser::Node::TYPE_CONSTANT && static_cast<const GDScriptParser::ConstantNode *>(on->arguments[1])->value.get_type() == Variant::STRING) {
							//also, somehow, named (speed up anyway)
							name = static_cast<const GDScriptParser::ConstantNode *>(on->arguments[1])->value;
							is_named = true;

						} else {
							//regular indexing
							if (codegen.adjust_stack_level(from, slevel)) {
								codegen.alloc_stack(slevel);
							}

							index = _parse_expression(codegen, on->arguments[1], slevel);
							if (!index) {
								return index;
							}
						}
					}

					codegen.append(is_named ? GDScriptFunction::OPCODE_GET_NAMED : GDScriptFunction::OPCODE_GET); // perform operator
					codegen.append(from); // argument 1
					codegen.append(is_named ? codegen.get_name_map_pos(name) : codegen.address_of(index)); // argument 2 (unary only takes one parameter)

				} break;
				case GDScriptParser::OperatorNode::OP_AND: {
					// AND operator with early out on failure

					auto res = _parse_expression(codegen, on->arguments[0], p_stack_level);
					if (!res) {
						return res;
					}
					codegen.append(GDScriptFunction::OPCODE_JUMP_IF_NOT);
					codegen.append(res);
					int jump_fail_pos = codegen.opcodes.size();
					codegen.append(0);

					res = _parse_expression(codegen, on->arguments[1], p_stack_level);
					if (!res) {
						return res;
					}

					codegen.append(GDScriptFunction::OPCODE_JUMP_IF_NOT);
					codegen.append(res);
					int jump_fail_pos2 = codegen.opcodes.size();
					codegen.append(0);

					codegen.alloc_stack(p_stack_level); //it will be used..
					codegen.append(GDScriptFunction::OPCODE_ASSIGN_TRUE);
					codegen.append(GDScriptCodeGenerator::StackAddress(p_stack_level));
					codegen.append(GDScriptFunction::OPCODE_JUMP);
					codegen.append(codegen.opcodes.size() + 3);
					codegen.opcodes.write[jump_fail_pos] = codegen.opcodes.size();
					codegen.opcodes.write[jump_fail_pos2] = codegen.opcodes.size();
					codegen.append(GDScriptFunction::OPCODE_ASSIGN_FALSE);
					codegen.append(GDScriptCodeGenerator::StackAddress(p_stack_level));
					return GDScriptCodeGenerator::StackAddress(p_stack_level);

				} break;
				case GDScriptParser::OperatorNode::OP_OR: {
					// OR operator with early out on success

					auto res = _parse_expression(codegen, on->arguments[0], p_stack_level);
					if (!res) {
						return res;
					}
					codegen.append(GDScriptFunction::OPCODE_JUMP_IF);
					codegen.append(res);
					int jump_success_pos = codegen.opcodes.size();
					codegen.append(0);

					res = _parse_expression(codegen, on->arguments[1], p_stack_level);
					if (!res) {
						return res;
					}

					codegen.append(GDScriptFunction::OPCODE_JUMP_IF);
					codegen.append(res);
					int jump_success_pos2 = codegen.opcodes.size();
					codegen.append(0);

					codegen.alloc_stack(p_stack_level); //it will be used..
					codegen.append(GDScriptFunction::OPCODE_ASSIGN_FALSE);
					codegen.append(GDScriptCodeGenerator::StackAddress(p_stack_level));
					codegen.append(GDScriptFunction::OPCODE_JUMP);
					codegen.append(codegen.opcodes.size() + 3);
					codegen.opcodes.write[jump_success_pos] = codegen.opcodes.size();
					codegen.opcodes.write[jump_success_pos2] = codegen.opcodes.size();
					codegen.append(GDScriptFunction::OPCODE_ASSIGN_TRUE);
					codegen.append(GDScriptCodeGenerator::StackAddress(p_stack_level));
					return GDScriptCodeGenerator::StackAddress(p_stack_level);

				} break;
				// ternary operators
				case GDScriptParser::OperatorNode::OP_TERNARY_IF: {
					// x IF a ELSE y operator with early out on failure

					auto res = _parse_expression(codegen, on->arguments[0], p_stack_level);
					if (!res) {
						return res;
					}
					codegen.append(GDScriptFunction::OPCODE_JUMP_IF_NOT);
					codegen.append(res);
					int jump_fail_pos = codegen.opcodes.size();
					codegen.append(0);

					res = _parse_expression(codegen, on->arguments[1], p_stack_level);
					if (!res) {
						return res;
					}

					codegen.alloc_stack(p_stack_level); //it will be used..
					codegen.append(GDScriptFunction::OPCODE_ASSIGN);
					codegen.append(GDScriptCodeGenerator::StackAddress(p_stack_level));
					codegen.append(res);
					codegen.append(GDScriptFunction::OPCODE_JUMP);
					int jump_past_pos = codegen.opcodes.size();
					codegen.append(0);

					codegen.opcodes.write[jump_fail_pos] = codegen.opcodes.size();
					res = _parse_expression(codegen, on->arguments[2], p_stack_level);
					if (!res) {
						return res;
					}

					codegen.append(GDScriptFunction::OPCODE_ASSIGN);
					codegen.append(GDScriptCodeGenerator::StackAddress(p_stack_level));
					codegen.append(res);

					codegen.opcodes.write[jump_past_pos] = codegen.opcodes.size();

					return GDScriptCodeGenerator::StackAddress(p_stack_level);

				} break;
				//unary operators
				case GDScriptParser::OperatorNode::OP_NEG: {
					if (!_create_unary_operator(codegen, on, Variant::OP_NEGATE, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_POS: {
					if (!_create_unary_operator(codegen, on, Variant::OP_POSITIVE, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_NOT: {
					if (!_create_unary_operator(codegen, on, Variant::OP_NOT, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_BIT_INVERT: {
					if (!_create_unary_operator(codegen, on, Variant::OP_BIT_NEGATE, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				//binary operators (in precedence order)
				case GDScriptParser::OperatorNode::OP_IN: {
					if (!_create_binary_operator(codegen, on, Variant::OP_IN, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_EQUAL: {
					if (!_create_binary_operator(codegen, on, Variant::OP_EQUAL, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_NOT_EQUAL: {
					if (!_create_binary_operator(codegen, on, Variant::OP_NOT_EQUAL, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_LESS: {
					if (!_create_binary_operator(codegen, on, Variant::OP_LESS, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_LESS_EQUAL: {
					if (!_create_binary_operator(codegen, on, Variant::OP_LESS_EQUAL, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_GREATER: {
					if (!_create_binary_operator(codegen, on, Variant::OP_GREATER, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_GREATER_EQUAL: {
					if (!_create_binary_operator(codegen, on, Variant::OP_GREATER_EQUAL, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_ADD: {
					if (!_create_binary_operator(codegen, on, Variant::OP_ADD, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_SUB: {
					if (!_create_binary_operator(codegen, on, Variant::OP_SUBTRACT, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_MUL: {
					if (!_create_binary_operator(codegen, on, Variant::OP_MULTIPLY, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_DIV: {
					if (!_create_binary_operator(codegen, on, Variant::OP_DIVIDE, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_MOD: {
					if (!_create_binary_operator(codegen, on, Variant::OP_MODULE, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				//case GDScriptParser::OperatorNode::OP_SHIFT_LEFT: { if (!_create_binary_operator(codegen,on,Variant::OP_SHIFT_LEFT,p_stack_level)) return GDScriptCodeGenerator::Address::INVALID;} break;
				//case GDScriptParser::OperatorNode::OP_SHIFT_RIGHT: { if (!_create_binary_operator(codegen,on,Variant::OP_SHIFT_RIGHT,p_stack_level)) return GDScriptCodeGenerator::Address::INVALID;} break;
				case GDScriptParser::OperatorNode::OP_BIT_AND: {
					if (!_create_binary_operator(codegen, on, Variant::OP_BIT_AND, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_BIT_OR: {
					if (!_create_binary_operator(codegen, on, Variant::OP_BIT_OR, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_BIT_XOR: {
					if (!_create_binary_operator(codegen, on, Variant::OP_BIT_XOR, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				//shift
				case GDScriptParser::OperatorNode::OP_SHIFT_LEFT: {
					if (!_create_binary_operator(codegen, on, Variant::OP_SHIFT_LEFT, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				case GDScriptParser::OperatorNode::OP_SHIFT_RIGHT: {
					if (!_create_binary_operator(codegen, on, Variant::OP_SHIFT_RIGHT, p_stack_level)) {
						return GDScriptCodeGenerator::Address::INVALID;
					}
				} break;
				//assignment operators
				case GDScriptParser::OperatorNode::OP_ASSIGN_ADD:
				case GDScriptParser::OperatorNode::OP_ASSIGN_SUB:
				case GDScriptParser::OperatorNode::OP_ASSIGN_MUL:
				case GDScriptParser::OperatorNode::OP_ASSIGN_DIV:
				case GDScriptParser::OperatorNode::OP_ASSIGN_MOD:
				case GDScriptParser::OperatorNode::OP_ASSIGN_SHIFT_LEFT:
				case GDScriptParser::OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
				case GDScriptParser::OperatorNode::OP_ASSIGN_BIT_AND:
				case GDScriptParser::OperatorNode::OP_ASSIGN_BIT_OR:
				case GDScriptParser::OperatorNode::OP_ASSIGN_BIT_XOR:
				case GDScriptParser::OperatorNode::OP_INIT_ASSIGN:
				case GDScriptParser::OperatorNode::OP_ASSIGN: {
					ERR_FAIL_COND_V(on->arguments.size() != 2, GDScriptCodeGenerator::Address::INVALID);

					if (on->arguments[0]->type == GDScriptParser::Node::TYPE_OPERATOR && (static_cast<GDScriptParser::OperatorNode *>(on->arguments[0])->op == GDScriptParser::OperatorNode::OP_INDEX || static_cast<GDScriptParser::OperatorNode *>(on->arguments[0])->op == GDScriptParser::OperatorNode::OP_INDEX_NAMED)) {
						// SET (chained) MODE!
#ifdef DEBUG_ENABLED
						if (static_cast<GDScriptParser::OperatorNode *>(on->arguments[0])->op == GDScriptParser::OperatorNode::OP_INDEX_NAMED) {
							const GDScriptParser::OperatorNode *inon = static_cast<GDScriptParser::OperatorNode *>(on->arguments[0]);

							if (inon->arguments[0]->type == GDScriptParser::Node::TYPE_SELF && codegen.script && codegen.function_node && !codegen.function_node->_static) {
								const Map<StringName, GDScript::MemberInfo>::Element *MI = codegen.script->member_indices.find(static_cast<GDScriptParser::IdentifierNode *>(inon->arguments[1])->name);
								if (MI && MI->get().setter == codegen.function_node->name) {
									String n = static_cast<GDScriptParser::IdentifierNode *>(inon->arguments[1])->name;
									_set_error("Must use '" + n + "' instead of 'self." + n + "' in setter.", inon);
									return GDScriptCodeGenerator::Address::INVALID;
								}
							}
						}
#endif

						int slevel = p_stack_level;

						GDScriptParser::OperatorNode *op = static_cast<GDScriptParser::OperatorNode *>(on->arguments[0]);

						/* Find chain of sets */

						StringName assign_property;

						List<GDScriptParser::OperatorNode *> chain;

						{
							//create get/set chain
							GDScriptParser::OperatorNode *n = op;
							while (true) {
								chain.push_back(n);
								if (n->arguments[0]->type != GDScriptParser::Node::TYPE_OPERATOR) {
									//check for a built-in property
									if (n->arguments[0]->type == GDScriptParser::Node::TYPE_IDENTIFIER) {
										GDScriptParser::IdentifierNode *identifier = static_cast<GDScriptParser::IdentifierNode *>(n->arguments[0]);
										if (_is_class_member_property(codegen, identifier->name)) {
											assign_property = identifier->name;
										}
									}
									break;
								}
								n = static_cast<GDScriptParser::OperatorNode *>(n->arguments[0]);
								if (n->op != GDScriptParser::OperatorNode::OP_INDEX && n->op != GDScriptParser::OperatorNode::OP_INDEX_NAMED) {
									break;
								}
							}
						}

						/* Chain of gets */

						//get at (potential) root stack pos, so it can be returned
						const auto retval = _parse_expression(codegen, chain.back()->get()->arguments[0], slevel);
						auto prev_base = retval;

						ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, retval, slevel);

						struct ChainInfo {
							bool is_named = false;
							GDScriptCodeGenerator::Address base = GDScriptCodeGenerator::Address::INVALID;
							GDScriptCodeGenerator::Address key = GDScriptCodeGenerator::Address::INVALID;
							StringName name;
						};

						List<ChainInfo> setchain;

						for (List<GDScriptParser::OperatorNode *>::Element *E = chain.back(); E; E = E->prev()) {
							if (E == chain.front()) { //ignore first
								break;
							}

							GDScriptCodeGenerator::Address key_idx = GDScriptCodeGenerator::Address::INVALID;
							bool is_named = E->get()->op == GDScriptParser::OperatorNode::OP_INDEX_NAMED;
							StringName name;

							if (is_named) {
								name = static_cast<const GDScriptParser::IdentifierNode *>(E->get()->arguments[1])->name;
							} else {
								GDScriptParser::Node *key = E->get()->arguments[1];
								key_idx = _parse_expression(codegen, key, slevel);

								// if key_idx was a stack value...
								ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, key_idx, slevel);
							}

							auto dst_pos = GDScriptCodeGenerator::StackAddress(slevel);

							codegen.append(is_named ? GDScriptFunction::OPCODE_GET_NAMED : GDScriptFunction::OPCODE_GET);
							codegen.append(prev_base);
							codegen.append(is_named ? codegen.get_name_map_pos(name) : codegen.address_of(key_idx));
							codegen.append(dst_pos);

#ifdef DEBUG_CHAINED_SETGET
							if (is_named) {
								printf("get named: src = %ls, idx = named(%d, %ls), dst = %ls\n",
										to_string(prev_base).c_str(),
										codegen.get_name_map_pos(name),
										String(name).c_str(),
										to_string(dst_pos).c_str());
							} else {
								printf("get: src = %ls, idx = %ls, dst = %ls\n",
										to_string(prev_base).c_str(),
										to_string(key_idx).c_str(),
										to_string(dst_pos).c_str());
							}
#endif

							setchain.push_back({ is_named, prev_base, key_idx, name });
							prev_base = dst_pos;

							// if the previous position was a stack value, save it for reuse
							if (codegen.adjust_stack_level(prev_base, slevel)) {
								codegen.alloc_stack(slevel);
							}
						}

						{ // Perform the assignment.
							GDScriptCodeGenerator::Address set_index = GDScriptCodeGenerator::Address::INVALID;
							bool is_named = op->op == GDScriptParser::OperatorNode::OP_INDEX_NAMED;
							StringName name;

							if (is_named) {
								name = static_cast<const GDScriptParser::IdentifierNode *>(op->arguments[1])->name;
							} else {
								set_index = _parse_expression(codegen, op->arguments[1], slevel);
								ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, set_index, slevel);
							}

							auto set_value = _parse_assign_right_expression(codegen, on, slevel, is_named ? GDScriptCodeGenerator::Address::INVALID : set_index);
							if (!set_value) { //error
								return set_value;
							}

							codegen.append(is_named ? GDScriptFunction::OPCODE_SET_NAMED : GDScriptFunction::OPCODE_SET);
							codegen.append(prev_base);
							codegen.append(is_named ? codegen.get_name_map_pos(name) : codegen.address_of(set_index));
							codegen.append(set_value);

#ifdef DEBUG_CHAINED_SETGET
							if (is_named) {
								printf("> set named: dst = %ls, idx = named(%d, %ls), val = %ls\n",
										to_string(prev_base).c_str(),
										codegen.get_name_map_pos(name),
										String(name).c_str(),
										to_string(set_value).c_str());
							} else {
								printf("> set: dst = %ls, idx = %ls, val = %ls\n",
										to_string(prev_base).c_str(),
										to_string(set_index).c_str(),
										to_string(set_value).c_str());
							}
#endif
						}

						// Set back the values into their bases (iterate in reverse).
						for (const List<ChainInfo>::Element *E = setchain.back(); E; E = E->prev()) {
							const ChainInfo &info = E->get();

#ifdef DEBUG_CHAINED_SETGET
							if (info.is_named) {
								printf("set named: dst = %ls, idx = named(%d, %ls), val = %ls\n",
										to_string(info.base).c_str(),
										codegen.get_name_map_pos(info.name),
										String(info.name).c_str(),
										to_string(prev_base).c_str());
							} else {
								printf("set: dst = %ls, idx = %ls, val = %ls\n",
										to_string(info.base).c_str(),
										to_string(info.key).c_str(),
										to_string(prev_base).c_str());
							}
#endif

							codegen.append(info.is_named ? GDScriptFunction::OPCODE_SET_NAMED : GDScriptFunction::OPCODE_SET);
							codegen.append(info.base);
							codegen.append(info.is_named ? codegen.get_name_map_pos(info.name) : codegen.address_of(info.key));
							codegen.append(prev_base);
							prev_base = info.base;
						}

						// If this is a property, also assign to it.
						// This allows things like: position.x += 2.0
						if (assign_property) {
							codegen.append(GDScriptFunction::OPCODE_SET_MEMBER);
							codegen.append(codegen.get_name_map_pos(assign_property));
							codegen.append(retval);

#ifdef DEBUG_CHAINED_SETGET
							printf("set member: idx = named(%d, %ls), val = %ls\n",
									codegen.get_name_map_pos(assign_property),
									String(assign_property).c_str(),
									to_string(retval).c_str());
#endif
						}

#ifdef DEBUG_CHAINED_SETGET
						printf("\n");
#endif
						return retval;

					} else if (on->arguments[0]->type == GDScriptParser::Node::TYPE_IDENTIFIER && _is_class_member_property(codegen, static_cast<GDScriptParser::IdentifierNode *>(on->arguments[0])->name)) {
						//assignment to member property

						int slevel = p_stack_level;

						auto src_address = _parse_assign_right_expression(codegen, on, slevel);
						if (!src_address) {
							return GDScriptCodeGenerator::Address::INVALID;
						}

						StringName name = static_cast<GDScriptParser::IdentifierNode *>(on->arguments[0])->name;

						codegen.append(GDScriptFunction::OPCODE_SET_MEMBER);
						codegen.append(codegen.get_name_map_pos(name));
						codegen.append(src_address);

						return GDScriptCodeGenerator::Address::NIL;
					} else {
						//REGULAR ASSIGNMENT MODE!!

						int slevel = p_stack_level;

						auto dst_address_a = _parse_expression(codegen, on->arguments[0], slevel, false, on->op == GDScriptParser::OperatorNode::OP_INIT_ASSIGN);
						ADJUST_AND_ALLOC_STACK_LEVEL_OR_RETURN(codegen, dst_address_a, slevel);

						auto src_address_b = _parse_assign_right_expression(codegen, on, slevel);
						ADJUST_STACK_LEVEL_OR_RETURN(codegen, src_address_b, slevel);

						GDScriptDataType assign_type = _gdtype_from_datatype(on->arguments[0]->get_datatype());

						if (assign_type.has_type && !on->datatype.has_type) {
							// Typed assignment
							switch (assign_type.kind) {
								case GDScriptDataType::BUILTIN: {
									codegen.append(GDScriptFunction::OPCODE_ASSIGN_TYPED_BUILTIN); // perform operator
									codegen.append(assign_type.builtin_type); // variable type
									codegen.append(dst_address_a); // argument 1
									codegen.append(src_address_b); // argument 2
								} break;
								case GDScriptDataType::NATIVE: {
									if (!GDScriptLanguage::get_singleton()->get_global_map().has(assign_type.native_type)) {
										_set_error("Invalid native class type '" + String(assign_type.native_type) + "'.", on->arguments[0]);
										return GDScriptCodeGenerator::Address::INVALID;
									}
									int class_idx = GDScriptLanguage::get_singleton()->get_global_map()[assign_type.native_type];
									codegen.append(GDScriptFunction::OPCODE_ASSIGN_TYPED_NATIVE); // perform operator
									codegen.append(GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::GLOBAL, class_idx)); // variable type
									codegen.append(dst_address_a); // argument 1
									codegen.append(src_address_b); // argument 2
								} break;
								case GDScriptDataType::SCRIPT:
								case GDScriptDataType::GDSCRIPT: {
									Variant script = assign_type.script_type;
									int idx = codegen.get_constant_pos(script); //make it a local constant for faster access
									codegen.append(GDScriptFunction::OPCODE_ASSIGN_TYPED_SCRIPT); // perform operator
									codegen.append(GDScriptCodeGenerator::Address(GDScriptCodeGenerator::Address::LOCAL_CONSTANT, idx)); // variable type
									codegen.append(dst_address_a); // argument 1
									codegen.append(src_address_b); // argument 2
								} break;
								default: {
									ERR_PRINT("Compiler bug: unresolved assign.");

									// Shouldn't get here, but fail-safe to a regular assignment
									codegen.append(GDScriptFunction::OPCODE_ASSIGN); // perform operator
									codegen.append(dst_address_a); // argument 1
									codegen.append(src_address_b); // argument 2 (unary only takes one parameter)
								}
							}
						} else {
							// Either untyped assignment or already type-checked by the parser
							codegen.append(GDScriptFunction::OPCODE_ASSIGN); // perform operator
							codegen.append(dst_address_a); // argument 1
							codegen.append(src_address_b); // argument 2 (unary only takes one parameter)
						}

						return dst_address_a; //if anything, returns whatever was assigned or correct stack position
					}
				} break;
				case GDScriptParser::OperatorNode::OP_IS: {
					ERR_FAIL_COND_V(on->arguments.size() != 2, GDScriptCodeGenerator::Address::INVALID);

					int slevel = p_stack_level;

					auto src_address_a = _parse_expression(codegen, on->arguments[0], slevel);
					ADJUST_STACK_LEVEL_OR_RETURN(codegen, src_address_a, slevel); //uses stack for return, increase stack

					auto src_address_b = _parse_expression(codegen, on->arguments[1], slevel);
					ADJUST_STACK_LEVEL_OR_RETURN(codegen, src_address_b, slevel); //uses stack for return, increase stack

					codegen.append(GDScriptFunction::OPCODE_EXTENDS_TEST); // perform operator
					codegen.append(src_address_a); // argument 1
					codegen.append(src_address_b); // argument 2 (unary only takes one parameter)

				} break;
				case GDScriptParser::OperatorNode::OP_IS_BUILTIN: {
					ERR_FAIL_COND_V(on->arguments.size() != 2, GDScriptCodeGenerator::Address::INVALID);
					ERR_FAIL_COND_V(on->arguments[1]->type != GDScriptParser::Node::TYPE_TYPE, GDScriptCodeGenerator::Address::INVALID);

					int slevel = p_stack_level;

					auto src_address_a = _parse_expression(codegen, on->arguments[0], slevel);
					codegen.adjust_stack_level(src_address_a, slevel); //uses stack for return, increase stack

					const GDScriptParser::TypeNode *tn = static_cast<const GDScriptParser::TypeNode *>(on->arguments[1]);

					codegen.append(GDScriptFunction::OPCODE_IS_BUILTIN); // perform operator
					codegen.append(src_address_a); // argument 1
					codegen.append((int)tn->vtype); // argument 2 (unary only takes one parameter)
				} break;
				default: {
					ERR_FAIL_V_MSG(GDScriptCodeGenerator::Address::INVALID, "Bug in bytecode compiler, unexpected operator #" + itos(on->op) + " in parse tree while parsing expression."); //unreachable code

				} break;
			}

			auto dst_addr = GDScriptCodeGenerator::StackAddress(p_stack_level);
			codegen.append(dst_addr); // append the stack level as destination address of the opcode
			codegen.alloc_stack(p_stack_level);
			return dst_addr;
		} break;
		//TYPE_TYPE,
		default: {
			ERR_FAIL_V_MSG(GDScriptCodeGenerator::Address::INVALID, "Bug in bytecode compiler, unexpected node in parse tree while parsing expression."); //unreachable code
		} break;
	}
}

Error GDScriptCompiler::_parse_block(CodeGen &codegen, const GDScriptParser::BlockNode *p_block, int p_stack_level, int p_break_addr, int p_continue_addr) {
	codegen.push_stack_identifiers();
	codegen.current_line = p_block->line;

	for (int i = 0; i < p_block->statements.size(); i++) {
		const GDScriptParser::Node *s = p_block->statements[i];

		switch (s->type) {
			case GDScriptParser::Node::TYPE_NEWLINE: {
#ifdef DEBUG_ENABLED
				const GDScriptParser::NewLineNode *nl = static_cast<const GDScriptParser::NewLineNode *>(s);
				codegen.append(GDScriptFunction::OPCODE_LINE);
				codegen.append(nl->line);
				codegen.current_line = nl->line;
#endif
			} break;
			case GDScriptParser::Node::TYPE_CONTROL_FLOW: {
				// try subblocks

				const GDScriptParser::ControlFlowNode *cf = static_cast<const GDScriptParser::ControlFlowNode *>(s);

				switch (cf->cf_type) {
					case GDScriptParser::ControlFlowNode::CF_MATCH: {
						GDScriptParser::MatchNode *match = cf->match;

						GDScriptParser::IdentifierNode *id = memnew(GDScriptParser::IdentifierNode);
						id->name = "#match_value";

						// var #match_value
						// copied because there is no _parse_statement :(
						codegen.add_stack_identifier(id->name, p_stack_level++);
						codegen.alloc_stack(p_stack_level);

						GDScriptParser::OperatorNode *op = memnew(GDScriptParser::OperatorNode);
						op->op = GDScriptParser::OperatorNode::OP_ASSIGN;
						op->arguments.push_back(id);
						op->arguments.push_back(match->val_to_match);

						auto ret = _parse_expression(codegen, op, p_stack_level);
						if (!ret) {
							memdelete(id);
							memdelete(op);
							return ERR_PARSE_ERROR;
						}

						// break address
						codegen.append(GDScriptFunction::OPCODE_JUMP);
						codegen.append(codegen.opcodes.size() + 3);
						int break_addr = codegen.opcodes.size();
						codegen.append(GDScriptFunction::OPCODE_JUMP);
						codegen.append(0); // break addr

						for (int j = 0; j < match->compiled_pattern_branches.size(); j++) {
							GDScriptParser::MatchNode::CompiledPatternBranch branch = match->compiled_pattern_branches[j];

							// jump over continue
							// jump unconditionally
							// continue address
							// compile the condition
							auto ret2 = _parse_expression(codegen, branch.compiled_pattern, p_stack_level);
							if (!ret2) {
								memdelete(id);
								memdelete(op);
								return ERR_PARSE_ERROR;
							}

							codegen.append(GDScriptFunction::OPCODE_JUMP_IF);
							codegen.append(ret2);
							codegen.append(codegen.opcodes.size() + 3);
							int continue_addr = codegen.opcodes.size();
							codegen.append(GDScriptFunction::OPCODE_JUMP);
							codegen.append(0);

							Error err = _parse_block(codegen, branch.body, p_stack_level, p_break_addr, continue_addr);
							if (err) {
								memdelete(id);
								memdelete(op);
								return ERR_PARSE_ERROR;
							}

							codegen.append(GDScriptFunction::OPCODE_JUMP);
							codegen.append(break_addr);

							codegen.opcodes.write[continue_addr + 1] = codegen.opcodes.size();
						}

						codegen.opcodes.write[break_addr + 1] = codegen.opcodes.size();

						memdelete(id);
						memdelete(op);

					} break;

					case GDScriptParser::ControlFlowNode::CF_IF: {
						auto ret2 = _parse_expression(codegen, cf->arguments[0], p_stack_level, false);
						if (!ret2) {
							return ERR_PARSE_ERROR;
						}

						codegen.append(GDScriptFunction::OPCODE_JUMP_IF_NOT);
						codegen.append(ret2);
						int else_addr = codegen.opcodes.size();
						codegen.append(0); //temporary

						Error err = _parse_block(codegen, cf->body, p_stack_level, p_break_addr, p_continue_addr);
						if (err) {
							return err;
						}

						if (cf->body_else) {
							codegen.append(GDScriptFunction::OPCODE_JUMP);
							int end_addr = codegen.opcodes.size();
							codegen.append(0);
							codegen.opcodes.write[else_addr] = codegen.opcodes.size();

							codegen.append(GDScriptFunction::OPCODE_LINE);
							codegen.append(cf->body_else->line);
							codegen.current_line = cf->body_else->line;

							Error err2 = _parse_block(codegen, cf->body_else, p_stack_level, p_break_addr, p_continue_addr);
							if (err2) {
								return err2;
							}

							codegen.opcodes.write[end_addr] = codegen.opcodes.size();
						} else {
							//end without else
							codegen.opcodes.write[else_addr] = codegen.opcodes.size();
						}

					} break;
					case GDScriptParser::ControlFlowNode::CF_FOR: {
						int slevel = p_stack_level;
						int iter_stack_pos = slevel;
						auto iterator_pos = GDScriptCodeGenerator::StackAddress(slevel++);
						auto counter_pos = GDScriptCodeGenerator::StackAddress(slevel++);
						auto container_pos = GDScriptCodeGenerator::StackAddress(slevel++);
						codegen.alloc_stack(slevel);

						codegen.push_stack_identifiers();
						codegen.add_stack_identifier(static_cast<const GDScriptParser::IdentifierNode *>(cf->arguments[0])->name, iter_stack_pos);

						auto ret2 = _parse_expression(codegen, cf->arguments[1], slevel, false);
						if (!ret2) {
							return ERR_COMPILATION_FAILED;
						}

						//assign container
						codegen.append(GDScriptFunction::OPCODE_ASSIGN);
						codegen.append(container_pos);
						codegen.append(ret2);

						//begin loop
						codegen.append(GDScriptFunction::OPCODE_ITERATE_BEGIN);
						codegen.append(counter_pos);
						codegen.append(container_pos);
						codegen.append(codegen.opcodes.size() + 4);
						codegen.append(iterator_pos);
						codegen.append(GDScriptFunction::OPCODE_JUMP); //skip code for next
						codegen.append(codegen.opcodes.size() + 8);
						//break loop
						int break_pos = codegen.opcodes.size();
						codegen.append(GDScriptFunction::OPCODE_JUMP); //skip code for next
						codegen.append(0); //skip code for next
						//next loop
						int continue_pos = codegen.opcodes.size();
						codegen.append(GDScriptFunction::OPCODE_ITERATE);
						codegen.append(counter_pos);
						codegen.append(container_pos);
						codegen.append(break_pos);
						codegen.append(iterator_pos);

						Error err = _parse_block(codegen, cf->body, slevel, break_pos, continue_pos);
						if (err) {
							return err;
						}

						codegen.append(GDScriptFunction::OPCODE_JUMP);
						codegen.append(continue_pos);
						codegen.opcodes.write[break_pos + 1] = codegen.opcodes.size();

						codegen.pop_stack_identifiers();

					} break;
					case GDScriptParser::ControlFlowNode::CF_WHILE: {
						codegen.append(GDScriptFunction::OPCODE_JUMP);
						codegen.append(codegen.opcodes.size() + 3);
						int break_addr = codegen.opcodes.size();
						codegen.append(GDScriptFunction::OPCODE_JUMP);
						codegen.append(0);
						int continue_addr = codegen.opcodes.size();

						auto ret2 = _parse_expression(codegen, cf->arguments[0], p_stack_level, false);
						if (!ret2) {
							return ERR_PARSE_ERROR;
						}
						codegen.append(GDScriptFunction::OPCODE_JUMP_IF_NOT);
						codegen.append(ret2);
						codegen.append(break_addr);
						Error err = _parse_block(codegen, cf->body, p_stack_level, break_addr, continue_addr);
						if (err) {
							return err;
						}
						codegen.append(GDScriptFunction::OPCODE_JUMP);
						codegen.append(continue_addr);

						codegen.opcodes.write[break_addr + 1] = codegen.opcodes.size();

					} break;
					case GDScriptParser::ControlFlowNode::CF_BREAK: {
						if (!p_break_addr) {
							_set_error("'break'' not within loop", cf);
							return ERR_COMPILATION_FAILED;
						}
						codegen.append(GDScriptFunction::OPCODE_JUMP);
						codegen.append(p_break_addr);

					} break;
					case GDScriptParser::ControlFlowNode::CF_CONTINUE: {
						if (!p_continue_addr) {
							_set_error("'continue' not within loop", cf);
							return ERR_COMPILATION_FAILED;
						}

						codegen.append(GDScriptFunction::OPCODE_JUMP);
						codegen.append(p_continue_addr);

					} break;
					case GDScriptParser::ControlFlowNode::CF_RETURN: {
						GDScriptCodeGenerator::Address ret2 = GDScriptCodeGenerator::Address::INVALID;

						if (cf->arguments.size()) {
							ret2 = _parse_expression(codegen, cf->arguments[0], p_stack_level, false);
							if (!ret2) {
								return ERR_PARSE_ERROR;
							}

						} else {
							ret2 = GDScriptCodeGenerator::Address::NIL;
						}

						codegen.append(GDScriptFunction::OPCODE_RETURN);
						codegen.append(ret2);

					} break;
				}
			} break;
			case GDScriptParser::Node::TYPE_ASSERT: {
#ifdef DEBUG_ENABLED
				// try subblocks

				const GDScriptParser::AssertNode *as = static_cast<const GDScriptParser::AssertNode *>(s);

				auto ret2 = _parse_expression(codegen, as->condition, p_stack_level, false);
				if (!ret2) {
					return ERR_PARSE_ERROR;
				}

				GDScriptCodeGenerator::Address message_ret = GDScriptCodeGenerator::Address::NIL;
				if (as->message) {
					message_ret = _parse_expression(codegen, as->message, p_stack_level + 1, false);
					if (!message_ret) {
						return ERR_PARSE_ERROR;
					}
				}

				codegen.append(GDScriptFunction::OPCODE_ASSERT);
				codegen.append(ret2);
				codegen.append(message_ret);
#endif
			} break;
			case GDScriptParser::Node::TYPE_BREAKPOINT: {
#ifdef DEBUG_ENABLED
				// try subblocks
				codegen.append(GDScriptFunction::OPCODE_BREAKPOINT);
#endif
			} break;
			case GDScriptParser::Node::TYPE_LOCAL_VAR: {
				const GDScriptParser::LocalVarNode *lv = static_cast<const GDScriptParser::LocalVarNode *>(s);

				// since we are using properties now for most class access, allow shadowing of class members to make user's life easier.
				//
				//if (_is_class_member_property(codegen, lv->name)) {
				//	_set_error("Name for local variable '" + String(lv->name) + "' can't shadow class property of the same name.", lv);
				//	return ERR_ALREADY_EXISTS;
				//}

				codegen.add_stack_identifier(lv->name, p_stack_level++);
				codegen.alloc_stack(p_stack_level);
			} break;
			default: {
				//expression
				auto ret2 = _parse_expression(codegen, s, p_stack_level, true);
				if (!ret2) {
					return ERR_PARSE_ERROR;
				}
			} break;
		}
	}
	codegen.pop_stack_identifiers();
	return OK;
}

Error GDScriptCompiler::_parse_function(GDScript *p_script, const GDScriptParser::ClassNode *p_class, const GDScriptParser::FunctionNode *p_func, bool p_for_ready) {
	Vector<int> bytecode;
	CodeGen codegen;

	codegen.class_node = p_class;
	codegen.script = p_script;
	codegen.function_node = p_func;
	codegen.stack_max = 0;
	codegen.current_line = 0;
	codegen.call_max = 0;
	codegen.debug_stack = ScriptDebugger::get_singleton() != nullptr;
#ifdef TOOLS_ENABLED
	Vector<StringName> argnames;
	Vector<Variant> default_arg_values;
#endif

	int stack_level = 0;

	if (p_func) {
		for (int i = 0; i < p_func->arguments.size(); i++) {
			// since we are using properties now for most class access, allow shadowing of class members to make user's life easier.
			//
			//if (_is_class_member_property(p_script, p_func->arguments[i])) {
			//	_set_error("Name for argument '" + String(p_func->arguments[i]) + "' can't shadow class property of the same name.", p_func);
			//	return ERR_ALREADY_EXISTS;
			//}

			codegen.add_stack_identifier(p_func->arguments[i], i);
#ifdef TOOLS_ENABLED
			argnames.push_back(p_func->arguments[i]);
#endif
		}
		stack_level = p_func->arguments.size();
	}

	codegen.alloc_stack(stack_level);

	/* Parse initializer -if applies- */

	bool is_initializer = !p_for_ready && !p_func;

	if (is_initializer || (p_func && String(p_func->name) == "_init")) {
		//parse initializer for class members
		if (!p_func && p_class->extends_used && p_script->native.is_null()) {
			//call implicit parent constructor
			codegen.append(GDScriptFunction::OPCODE_CALL_SELF_BASE);
			codegen.append(codegen.get_name_map_pos("_init"));
			codegen.append(0);
			codegen.append(GDScriptCodeGenerator::StackAddress(0));
		}
		Error err = _parse_block(codegen, p_class->initializer, stack_level);
		if (err) {
			return err;
		}
		is_initializer = true;
	}

	if (p_for_ready || (p_func && String(p_func->name) == "_ready")) {
		//parse initializer for class members
		if (p_class->ready->statements.size()) {
			Error err = _parse_block(codegen, p_class->ready, stack_level);
			if (err) {
				return err;
			}
		}
	}

	/* Parse default argument code -if applies- */

	Vector<int> defarg_addr;
	StringName func_name;

	if (p_func) {
		if (p_func->default_values.size()) {
			codegen.append(GDScriptFunction::OPCODE_JUMP_TO_DEF_ARGUMENT);
			defarg_addr.push_back(codegen.opcodes.size());
			for (int i = 0; i < p_func->default_values.size(); i++) {
				_parse_expression(codegen, p_func->default_values[i], stack_level, true);
				defarg_addr.push_back(codegen.opcodes.size());
#ifdef TOOLS_ENABLED
				const GDScriptParser::OperatorNode *assign = static_cast<const GDScriptParser::OperatorNode *>(p_func->default_values[i]);
				if (assign->arguments.size() >= 2 && assign->arguments[1]->type == GDScriptParser::Node::TYPE_CONSTANT) {
					const GDScriptParser::ConstantNode *cn = static_cast<const GDScriptParser::ConstantNode *>(assign->arguments[1]);
					default_arg_values.push_back(cn->value);
				} else {
					default_arg_values.push_back(Variant());
				}
#endif
			}

			defarg_addr.invert();
		}

		Error err = _parse_block(codegen, p_func->body, stack_level);
		if (err) {
			return err;
		}

		func_name = p_func->name;
	} else {
		if (p_for_ready) {
			func_name = "_ready";
		} else {
			func_name = "_init";
		}
	}

	codegen.append(GDScriptFunction::OPCODE_END);

	/*
	if (String(p_func->name)=="") { //initializer func
		gdfunc = &p_script->initializer;
	*/
	//} else { //regular func
	p_script->member_functions[func_name] = memnew(GDScriptFunction);
	GDScriptFunction *gdfunc = p_script->member_functions[func_name];
	//}

	if (p_func) {
		gdfunc->_static = p_func->_static;
		gdfunc->rpc_mode = p_func->rpc_mode;
		gdfunc->argument_types.resize(p_func->argument_types.size());
		for (int i = 0; i < p_func->argument_types.size(); i++) {
			gdfunc->argument_types.write[i] = _gdtype_from_datatype(p_func->argument_types[i], p_script);
		}
		gdfunc->return_type = _gdtype_from_datatype(p_func->return_type, p_script);
	} else {
		gdfunc->_static = false;
		gdfunc->rpc_mode = MultiplayerAPI::RPC_MODE_DISABLED;
		gdfunc->return_type = GDScriptDataType();
		gdfunc->return_type.has_type = true;
		gdfunc->return_type.kind = GDScriptDataType::BUILTIN;
		gdfunc->return_type.builtin_type = Variant::NIL;
	}

#ifdef TOOLS_ENABLED
	gdfunc->arg_names = argnames;
	gdfunc->default_arg_values = default_arg_values;
#endif
	//constants
	if (codegen.constant_map.size()) {
		gdfunc->_constant_count = codegen.constant_map.size();
		gdfunc->constants.resize(codegen.constant_map.size());
		gdfunc->_constants_ptr = gdfunc->constants.ptrw();
		const Variant *K = nullptr;
		while ((K = codegen.constant_map.next(K))) {
			int idx = codegen.constant_map[*K];
			gdfunc->constants.write[idx] = *K;
		}
	} else {
		gdfunc->_constants_ptr = nullptr;
		gdfunc->_constant_count = 0;
	}
	//global names
	if (codegen.name_map.size()) {
		gdfunc->global_names.resize(codegen.name_map.size());
		gdfunc->_global_names_ptr = &gdfunc->global_names[0];
		for (Map<StringName, int>::Element *E = codegen.name_map.front(); E; E = E->next()) {
			gdfunc->global_names.write[E->get()] = E->key();
		}
		gdfunc->_global_names_count = gdfunc->global_names.size();

	} else {
		gdfunc->_global_names_ptr = nullptr;
		gdfunc->_global_names_count = 0;
	}

#ifdef TOOLS_ENABLED
	// Named globals
	if (codegen.named_globals.size()) {
		gdfunc->named_globals.resize(codegen.named_globals.size());
		gdfunc->_named_globals_ptr = gdfunc->named_globals.ptr();
		for (int i = 0; i < codegen.named_globals.size(); i++) {
			gdfunc->named_globals.write[i] = codegen.named_globals[i];
		}
		gdfunc->_named_globals_count = gdfunc->named_globals.size();
	}
#endif

	if (codegen.opcodes.size()) {
		gdfunc->code = codegen.opcodes;
		gdfunc->_code_ptr = &gdfunc->code[0];
		gdfunc->_code_size = codegen.opcodes.size();

	} else {
		gdfunc->_code_ptr = nullptr;
		gdfunc->_code_size = 0;
	}

	if (defarg_addr.size()) {
		gdfunc->default_arguments = defarg_addr;
		gdfunc->_default_arg_count = defarg_addr.size() - 1;
		gdfunc->_default_arg_ptr = &gdfunc->default_arguments[0];
	} else {
		gdfunc->_default_arg_count = 0;
		gdfunc->_default_arg_ptr = nullptr;
	}

	gdfunc->_argument_count = p_func ? p_func->arguments.size() : 0;
	gdfunc->_stack_size = codegen.stack_max;
	gdfunc->_call_size = codegen.call_max;
	gdfunc->name = func_name;
#ifdef DEBUG_ENABLED
	if (ScriptDebugger::get_singleton()) {
		String signature;
		//path
		if (p_script->get_path() != String()) {
			signature += p_script->get_path();
		}
		//loc
		if (p_func) {
			signature += "::" + itos(p_func->body->line);
		} else {
			signature += "::0";
		}

		//function and class

		if (p_class->name) {
			signature += "::" + String(p_class->name) + "." + String(func_name);
		} else {
			signature += "::" + String(func_name);
		}

		gdfunc->profile.signature = signature;
	}
#endif
	gdfunc->_script = p_script;
	gdfunc->source = source;

#ifdef DEBUG_ENABLED

	{
		gdfunc->func_cname = (String(source) + " - " + String(func_name)).utf8();
		gdfunc->_func_cname = gdfunc->func_cname.get_data();
	}

#endif
	if (p_func) {
		gdfunc->_initial_line = p_func->line;
#ifdef TOOLS_ENABLED

		p_script->member_lines[func_name] = p_func->line;
#endif
	} else {
		gdfunc->_initial_line = 0;
	}

	if (codegen.debug_stack) {
		gdfunc->stack_debug = codegen.stack_debug;
	}

	if (is_initializer) {
		p_script->initializer = gdfunc;
	}

	return OK;
}

Error GDScriptCompiler::_parse_class_level(GDScript *p_script, const GDScriptParser::ClassNode *p_class, bool p_keep_state) {
	parsing_classes.insert(p_script);

	if (p_class->owner && p_class->owner->owner) {
		// Owner is not root
		if (!parsed_classes.has(p_script->_owner)) {
			if (parsing_classes.has(p_script->_owner)) {
				_set_error("Cyclic class reference for '" + String(p_class->name) + "'.", p_class);
				return ERR_PARSE_ERROR;
			}
			Error err = _parse_class_level(p_script->_owner, p_class->owner, p_keep_state);
			if (err) {
				return err;
			}
		}
	}

	p_script->native = Ref<GDScriptNativeClass>();
	p_script->base = Ref<GDScript>();
	p_script->_base = nullptr;
	p_script->members.clear();
	p_script->constants.clear();
	for (Map<StringName, GDScriptFunction *>::Element *E = p_script->member_functions.front(); E; E = E->next()) {
		memdelete(E->get());
	}
	p_script->member_functions.clear();
	p_script->member_indices.clear();
	p_script->member_info.clear();
	p_script->_signals.clear();
	p_script->initializer = nullptr;

	p_script->tool = p_class->tool;
	p_script->name = p_class->name;

	Ref<GDScriptNativeClass> native;

	GDScriptDataType base_type = _gdtype_from_datatype(p_class->base_type);
	// Inheritance
	switch (base_type.kind) {
		case GDScriptDataType::NATIVE: {
			int native_idx = GDScriptLanguage::get_singleton()->get_global_map()[base_type.native_type];
			native = GDScriptLanguage::get_singleton()->get_global_array()[native_idx];
			ERR_FAIL_COND_V(native.is_null(), ERR_BUG);
			p_script->native = native;
		} break;
		case GDScriptDataType::GDSCRIPT: {
			Ref<GDScript> base = Ref<Script>(base_type.script_type);
			p_script->base = base;
			p_script->_base = base.ptr();
			p_script->member_indices = base->member_indices;

			if (p_class->base_type.kind == GDScriptParser::DataType::CLASS) {
				if (!parsed_classes.has(p_script->_base)) {
					if (parsing_classes.has(p_script->_base)) {
						_set_error("Cyclic class reference for '" + String(p_class->name) + "'.", p_class);
						return ERR_PARSE_ERROR;
					}
					Error err = _parse_class_level(p_script->_base, p_class->base_type.class_type, p_keep_state);
					if (err) {
						return err;
					}
				}
			}
		} break;
		default: {
			_set_error("Parser bug: invalid inheritance.", p_class);
			return ERR_BUG;
		} break;
	}

	for (int i = 0; i < p_class->variables.size(); i++) {
		StringName name = p_class->variables[i].identifier;

		GDScript::MemberInfo minfo;
		minfo.index = p_script->member_indices.size();
		minfo.setter = p_class->variables[i].setter;
		minfo.getter = p_class->variables[i].getter;
		minfo.rpc_mode = p_class->variables[i].rpc_mode;
		minfo.data_type = _gdtype_from_datatype(p_class->variables[i].data_type, p_script);

		PropertyInfo prop_info = minfo.data_type;
		prop_info.name = name;
		PropertyInfo export_info = p_class->variables[i]._export;

		if (export_info.type != Variant::NIL) {
			if (!minfo.data_type.has_type) {
				prop_info.type = export_info.type;
				prop_info.class_name = export_info.class_name;
			}
			prop_info.hint = export_info.hint;
			prop_info.hint_string = export_info.hint_string;
			prop_info.usage = export_info.usage;
#ifdef TOOLS_ENABLED
			if (p_class->variables[i].default_value.get_type() != Variant::NIL) {
				p_script->member_default_values[name] = p_class->variables[i].default_value;
			}
#endif
		} else {
			prop_info.usage = PROPERTY_USAGE_SCRIPT_VARIABLE;
		}

		p_script->member_info[name] = prop_info;
		p_script->member_indices[name] = minfo;
		p_script->members.insert(name);

#ifdef TOOLS_ENABLED
		p_script->member_lines[name] = p_class->variables[i].line;
#endif
	}

	for (Map<StringName, GDScriptParser::ClassNode::Constant>::Element *E = p_class->constant_expressions.front(); E; E = E->next()) {
		StringName name = E->key();

		ERR_CONTINUE(E->get().expression->type != GDScriptParser::Node::TYPE_CONSTANT);

		GDScriptParser::ConstantNode *constant = static_cast<GDScriptParser::ConstantNode *>(E->get().expression);

		p_script->constants.insert(name, constant->value);
#ifdef TOOLS_ENABLED

		p_script->member_lines[name] = E->get().expression->line;
#endif
	}

	for (int i = 0; i < p_class->_signals.size(); i++) {
		StringName name = p_class->_signals[i].name;

		GDScript *c = p_script;

		while (c) {
			if (c->_signals.has(name)) {
				_set_error("Signal '" + name + "' redefined (in current or parent class)", p_class);
				return ERR_ALREADY_EXISTS;
			}

			if (c->base.is_valid()) {
				c = c->base.ptr();
			} else {
				c = nullptr;
			}
		}

		if (native.is_valid()) {
			if (ClassDB::has_signal(native->get_name(), name)) {
				_set_error("Signal '" + name + "' redefined (original in native class '" + String(native->get_name()) + "')", p_class);
				return ERR_ALREADY_EXISTS;
			}
		}

		p_script->_signals[name] = p_class->_signals[i].arguments;
	}

	parsed_classes.insert(p_script);
	parsing_classes.erase(p_script);

	//parse sub-classes

	for (int i = 0; i < p_class->subclasses.size(); i++) {
		StringName name = p_class->subclasses[i]->name;
		Ref<GDScript> &subclass = p_script->subclasses[name];
		GDScript *subclass_ptr = subclass.ptr();

		// Subclass might still be parsing, just skip it
		if (!parsed_classes.has(subclass_ptr) && !parsing_classes.has(subclass_ptr)) {
			Error err = _parse_class_level(subclass_ptr, p_class->subclasses[i], p_keep_state);
			if (err) {
				return err;
			}
		}

#ifdef TOOLS_ENABLED

		p_script->member_lines[name] = p_class->subclasses[i]->line;
#endif

		p_script->constants.insert(name, subclass); //once parsed, goes to the list of constants
	}

	return OK;
}

Error GDScriptCompiler::_parse_class_blocks(GDScript *p_script, const GDScriptParser::ClassNode *p_class, bool p_keep_state) {
	//parse methods

	bool has_initializer = false;
	bool has_ready = false;

	for (int i = 0; i < p_class->functions.size(); i++) {
		if (!has_initializer && p_class->functions[i]->name == "_init") {
			has_initializer = true;
		}
		if (!has_ready && p_class->functions[i]->name == "_ready") {
			has_ready = true;
		}
		Error err = _parse_function(p_script, p_class, p_class->functions[i]);
		if (err) {
			return err;
		}
	}

	//parse static methods

	for (int i = 0; i < p_class->static_functions.size(); i++) {
		Error err = _parse_function(p_script, p_class, p_class->static_functions[i]);
		if (err) {
			return err;
		}
	}

	if (!has_initializer) {
		//create a constructor
		Error err = _parse_function(p_script, p_class, nullptr);
		if (err) {
			return err;
		}
	}

	if (!has_ready && p_class->ready->statements.size()) {
		//create a constructor
		Error err = _parse_function(p_script, p_class, nullptr, true);
		if (err) {
			return err;
		}
	}

#ifdef DEBUG_ENABLED

	//validate instances if keeping state

	if (p_keep_state) {
		for (Set<Object *>::Element *E = p_script->instances.front(); E;) {
			Set<Object *>::Element *N = E->next();

			ScriptInstance *si = E->get()->get_script_instance();
			if (si->is_placeholder()) {
#ifdef TOOLS_ENABLED
				PlaceHolderScriptInstance *psi = static_cast<PlaceHolderScriptInstance *>(si);

				if (p_script->is_tool()) {
					//re-create as an instance
					p_script->placeholders.erase(psi); //remove placeholder

					GDScriptInstance *instance = memnew(GDScriptInstance);
					instance->base_ref = Object::cast_to<Reference>(E->get());
					instance->members.resize(p_script->member_indices.size());
					instance->script = Ref<GDScript>(p_script);
					instance->owner = E->get();

					//needed for hot reloading
					for (Map<StringName, GDScript::MemberInfo>::Element *F = p_script->member_indices.front(); F; F = F->next()) {
						instance->member_indices_cache[F->key()] = F->get().index;
					}
					instance->owner->set_script_instance(instance);

					/* STEP 2, INITIALIZE AND CONSTRUCT */

					Variant::CallError ce;
					p_script->initializer->call(instance, nullptr, 0, ce);

					if (ce.error != Variant::CallError::CALL_OK) {
						//well, tough luck, not goinna do anything here
					}
				}
#endif
			} else {
				GDScriptInstance *gi = static_cast<GDScriptInstance *>(si);
				gi->reload_members();
			}

			E = N;
		}
	}
#endif

	for (int i = 0; i < p_class->subclasses.size(); i++) {
		StringName name = p_class->subclasses[i]->name;
		GDScript *subclass = p_script->subclasses[name].ptr();

		Error err = _parse_class_blocks(subclass, p_class->subclasses[i], p_keep_state);
		if (err) {
			return err;
		}
	}

	p_script->valid = true;
	return OK;
}

void GDScriptCompiler::_make_scripts(GDScript *p_script, const GDScriptParser::ClassNode *p_class, bool p_keep_state) {
	Map<StringName, Ref<GDScript>> old_subclasses;

	if (p_keep_state) {
		old_subclasses = p_script->subclasses;
	}

	p_script->subclasses.clear();

	for (int i = 0; i < p_class->subclasses.size(); i++) {
		StringName name = p_class->subclasses[i]->name;

		Ref<GDScript> subclass;
		String fully_qualified_name = p_script->fully_qualified_name + "::" + name;

		if (old_subclasses.has(name)) {
			subclass = old_subclasses[name];
		} else {
			Ref<GDScript> orphan_subclass = GDScriptLanguage::get_singleton()->get_orphan_subclass(fully_qualified_name);
			if (orphan_subclass.is_valid()) {
				subclass = orphan_subclass;
			} else {
				subclass.instance();
			}
		}

		subclass->_owner = p_script;
		subclass->fully_qualified_name = fully_qualified_name;
		p_script->subclasses.insert(name, subclass);

		_make_scripts(subclass.ptr(), p_class->subclasses[i], false);
	}
}

Error GDScriptCompiler::compile(const GDScriptParser *p_parser, GDScript *p_script, bool p_keep_state) {
	err_line = -1;
	err_column = -1;
	error = "";
	parser = p_parser;
	main_script = p_script;
	const GDScriptParser::Node *root = parser->get_parse_tree();
	ERR_FAIL_COND_V(root->type != GDScriptParser::Node::TYPE_CLASS, ERR_INVALID_DATA);

	source = p_script->get_path();

	// The best fully qualified name for a base level script is its file path
	p_script->fully_qualified_name = p_script->path;

	// Create scripts for subclasses beforehand so they can be referenced
	_make_scripts(p_script, static_cast<const GDScriptParser::ClassNode *>(root), p_keep_state);

	p_script->_owner = nullptr;
	Error err = _parse_class_level(p_script, static_cast<const GDScriptParser::ClassNode *>(root), p_keep_state);
	if (err) {
		return err;
	}

	err = _parse_class_blocks(p_script, static_cast<const GDScriptParser::ClassNode *>(root), p_keep_state);
	if (err) {
		return err;
	}

	return OK;
}

String GDScriptCompiler::get_error() const {
	return error;
}
int GDScriptCompiler::get_error_line() const {
	return err_line;
}
int GDScriptCompiler::get_error_column() const {
	return err_column;
}

GDScriptCompiler::GDScriptCompiler() {
}
