/**************************************************************************/
/*  gdscript_parser.cpp                                                   */
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

#include "gdscript_parser.h"

#include "core/core_string_names.h"
#include "core/engine.h"
#include "core/io/resource_loader.h"
#include "core/os/file_access.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/reference.h"
#include "core/script_language.h"
#include "gdscript.h"

template <class T>
T *GDScriptParser::alloc_node() {
	T *t = memnew(T);

	t->next = list;
	list = t;

	if (!head) {
		head = t;
	}

	t->line = tokenizer->get_token_line();
	t->column = tokenizer->get_token_column();
	return t;
}

static String _lookup_autoload_path_for_identifier(const String &p_identifier) {
	String autoload_path;
	String autoload_setting_path = "autoload/" + p_identifier;
	if (ProjectSettings::get_singleton()->has_setting(autoload_setting_path)) {
		autoload_path = ProjectSettings::get_singleton()->get(autoload_setting_path);
		if (autoload_path.begins_with("*")) {
			autoload_path = autoload_path.right(1);
		}
		if (!autoload_path.begins_with("res://")) {
			autoload_path = "res://" + autoload_path;
		}
	}
	return autoload_path;
}

#ifdef DEBUG_ENABLED
static String _find_function_name(const GDScriptParser::OperatorNode *p_call);
#endif // DEBUG_ENABLED

bool GDScriptParser::_end_statement() {
	if (tokenizer->get_token() == GDScriptTokenizer::TK_SEMICOLON) {
		tokenizer->advance();
		return true; //handle next
	} else if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE || tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {
		return true; //will be handled properly
	}

	return false;
}

void GDScriptParser::_set_end_statement_error(String p_name) {
	String error_msg;
	if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER) {
		error_msg = vformat("Expected end of statement (\"%s\"), got %s (\"%s\") instead.", p_name, tokenizer->get_token_name(tokenizer->get_token()), tokenizer->get_token_identifier());
	} else {
		error_msg = vformat("Expected end of statement (\"%s\"), got %s instead.", p_name, tokenizer->get_token_name(tokenizer->get_token()));
	}
	_set_error(error_msg);
}

bool GDScriptParser::_enter_indent_block(BlockNode *p_block) {
	if (tokenizer->get_token() != GDScriptTokenizer::TK_COLON) {
		// report location at the previous token (on the previous line)
		int error_line = tokenizer->get_token_line(-1);
		int error_column = tokenizer->get_token_column(-1);
		_set_error("':' expected at end of line.", error_line, error_column);
		return false;
	}
	tokenizer->advance();

	if (tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {
		return false;
	}

	if (tokenizer->get_token() != GDScriptTokenizer::TK_NEWLINE) {
		// be more python-like
		IndentLevel current_level = indent_level.back()->get();
		indent_level.push_back(current_level);
		return true;
		//_set_error("newline expected after ':'.");
		//return false;
	}

	while (true) {
		if (tokenizer->get_token() != GDScriptTokenizer::TK_NEWLINE) {
			return false; //wtf
		} else if (tokenizer->get_token(1) == GDScriptTokenizer::TK_EOF) {
			return false;
		} else if (tokenizer->get_token(1) != GDScriptTokenizer::TK_NEWLINE) {
			int indent = tokenizer->get_token_line_indent();
			int tabs = tokenizer->get_token_line_tab_indent();
			IndentLevel current_level = indent_level.back()->get();
			IndentLevel new_indent(indent, tabs);
			if (new_indent.is_mixed(current_level)) {
				_set_error("Mixed tabs and spaces in indentation.");
				return false;
			}

			if (indent <= current_level.indent) {
				return false;
			}

			indent_level.push_back(new_indent);
			tokenizer->advance();
			return true;

		} else if (p_block) {
			NewLineNode *nl = alloc_node<NewLineNode>();
			nl->line = tokenizer->get_token_line();
			p_block->statements.push_back(nl);
		}

		tokenizer->advance(); // go to next newline
	}
}

bool GDScriptParser::_parse_arguments(Node *p_parent, Vector<Node *> &p_args, bool p_static, bool p_can_codecomplete, bool p_parsing_constant) {
	if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
		tokenizer->advance();
	} else {
		parenthesis++;
		int argidx = 0;

		while (true) {
			if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
				_make_completable_call(argidx);
				completion_node = p_parent;
			} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT && tokenizer->get_token_constant().get_type() == Variant::STRING && tokenizer->get_token(1) == GDScriptTokenizer::TK_CURSOR) {
				//completing a string argument..
				completion_cursor = tokenizer->get_token_constant();

				_make_completable_call(argidx);
				completion_node = p_parent;
				tokenizer->advance(1);
				return false;
			}

			Node *arg = _parse_expression(p_parent, p_static, false, p_parsing_constant);
			if (!arg) {
				return false;
			}

			p_args.push_back(arg);

			if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
				tokenizer->advance();
				break;

			} else if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
				if (tokenizer->get_token(1) == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
					_set_error("Expression expected");
					return false;
				}

				tokenizer->advance();
				argidx++;
			} else {
				// something is broken
				_set_error("Expected ',' or ')'");
				return false;
			}
		}
		parenthesis--;
	}

	return true;
}

void GDScriptParser::_make_completable_call(int p_arg) {
	completion_cursor = StringName();
	completion_type = COMPLETION_CALL_ARGUMENTS;
	completion_class = current_class;
	completion_function = current_function;
	completion_line = tokenizer->get_token_line();
	completion_argument = p_arg;
	completion_block = current_block;
	completion_found = true;
	tokenizer->advance();
}

bool GDScriptParser::_get_completable_identifier(CompletionType p_type, StringName &identifier) {
	identifier = StringName();
	if (tokenizer->is_token_literal()) {
		identifier = tokenizer->get_token_literal();
		tokenizer->advance();
	}
	if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
		completion_cursor = identifier;
		completion_type = p_type;
		completion_class = current_class;
		completion_function = current_function;
		completion_line = tokenizer->get_token_line();
		completion_block = current_block;
		completion_found = true;
		completion_ident_is_call = false;
		tokenizer->advance();

		if (tokenizer->is_token_literal()) {
			identifier = identifier.operator String() + tokenizer->get_token_literal().operator String();
			tokenizer->advance();
		}

		if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
			completion_ident_is_call = true;
		}
		return true;
	}

	return false;
}

GDScriptParser::Node *GDScriptParser::_parse_expression(Node *p_parent, bool p_static, bool p_allow_assign, bool p_parsing_constant) {
	//Vector<Node*> expressions;
	//Vector<OperatorNode::Operator> operators;

	Vector<Expression> expression;

	Node *expr = nullptr;

	int op_line = tokenizer->get_token_line(); // when operators are created at the bottom, the line might have been changed (\n found)

	while (true) {
		/*****************/
		/* Parse Operand */
		/*****************/

		if (parenthesis > 0) {
			//remove empty space (only allowed if inside parenthesis
			while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
				tokenizer->advance();
			}
		}

		// Check that the next token is not TK_CURSOR and if it is, the offset should be incremented.
		int next_valid_offset = 1;
		if (tokenizer->get_token(next_valid_offset) == GDScriptTokenizer::TK_CURSOR) {
			next_valid_offset++;
			// There is a chunk of the identifier that also needs to be ignored (not always there!)
			if (tokenizer->get_token(next_valid_offset) == GDScriptTokenizer::TK_IDENTIFIER) {
				next_valid_offset++;
			}
		}

		if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
			//subexpression ()
			tokenizer->advance();
			parenthesis++;
			Node *subexpr = _parse_expression(p_parent, p_static, p_allow_assign, p_parsing_constant);
			parenthesis--;
			if (!subexpr) {
				return nullptr;
			}

			if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
				_set_error("Expected ')' in expression");
				return nullptr;
			}

			tokenizer->advance();
			expr = subexpr;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_DOLLAR) {
			tokenizer->advance();

			String path;

			bool need_identifier = true;
			bool done = false;
			int line = tokenizer->get_token_line();

			while (!done) {
				switch (tokenizer->get_token()) {
					case GDScriptTokenizer::TK_CURSOR: {
						completion_type = COMPLETION_GET_NODE;
						completion_class = current_class;
						completion_function = current_function;
						completion_line = tokenizer->get_token_line();
						completion_cursor = path;
						completion_argument = 0;
						completion_block = current_block;
						completion_found = true;
						tokenizer->advance();
					} break;
					case GDScriptTokenizer::TK_CONSTANT: {
						if (!need_identifier) {
							done = true;
							break;
						}

						if (tokenizer->get_token_constant().get_type() != Variant::STRING) {
							_set_error("Expected string constant or identifier after '$' or '/'.");
							return nullptr;
						}

						path += String(tokenizer->get_token_constant());
						tokenizer->advance();
						need_identifier = false;

					} break;
					case GDScriptTokenizer::TK_OP_DIV: {
						if (need_identifier) {
							done = true;
							break;
						}

						path += "/";
						tokenizer->advance();
						need_identifier = true;

					} break;
					default: {
						// Instead of checking for TK_IDENTIFIER, we check with is_token_literal, as this allows us to use match/sync/etc. as a name
						if (need_identifier && tokenizer->is_token_literal()) {
							path += String(tokenizer->get_token_literal());
							tokenizer->advance();
							need_identifier = false;
						} else {
							done = true;
						}

						break;
					}
				}
			}

			if (path == "") {
				_set_error("Path expected after $.");
				return nullptr;
			}

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = OperatorNode::OP_CALL;
			op->line = line;
			op->arguments.push_back(alloc_node<SelfNode>());
			op->arguments[0]->line = line;

			IdentifierNode *funcname = alloc_node<IdentifierNode>();
			funcname->name = "get_node";
			funcname->line = line;
			op->arguments.push_back(funcname);

			ConstantNode *nodepath = alloc_node<ConstantNode>();
			nodepath->value = NodePath(StringName(path));
			nodepath->datatype = _type_from_variant(nodepath->value);
			nodepath->line = line;
			op->arguments.push_back(nodepath);

			expr = op;

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
			tokenizer->advance();
			continue; //no point in cursor in the middle of expression

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT) {
			//constant defined by tokenizer
			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = tokenizer->get_token_constant();
			constant->datatype = _type_from_variant(constant->value);
			tokenizer->advance();
			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONST_PI) {
			//constant defined by tokenizer
			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = Math_PI;
			constant->datatype = _type_from_variant(constant->value);
			tokenizer->advance();
			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONST_TAU) {
			//constant defined by tokenizer
			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = Math_TAU;
			constant->datatype = _type_from_variant(constant->value);
			tokenizer->advance();
			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONST_INF) {
			//constant defined by tokenizer
			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = Math_INF;
			constant->datatype = _type_from_variant(constant->value);
			tokenizer->advance();
			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONST_NAN) {
			//constant defined by tokenizer
			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = Math_NAN;
			constant->datatype = _type_from_variant(constant->value);
			tokenizer->advance();
			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_PRELOAD) {
			//constant defined by tokenizer
			tokenizer->advance();
			if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
				_set_error("Expected '(' after 'preload'");
				return nullptr;
			}
			tokenizer->advance();

			if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
				completion_cursor = StringName();
				completion_node = p_parent;
				completion_type = COMPLETION_RESOURCE_PATH;
				completion_class = current_class;
				completion_function = current_function;
				completion_line = tokenizer->get_token_line();
				completion_block = current_block;
				completion_argument = 0;
				completion_found = true;
				tokenizer->advance();
			}

			String path;
			bool found_constant = false;
			bool valid = false;
			ConstantNode *cn;

			parenthesis++;
			Node *subexpr = _parse_and_reduce_expression(p_parent, p_static);
			parenthesis--;
			if (subexpr) {
				if (subexpr->type == Node::TYPE_CONSTANT) {
					cn = static_cast<ConstantNode *>(subexpr);
					found_constant = true;
				}
				if (subexpr->type == Node::TYPE_IDENTIFIER) {
					IdentifierNode *in = static_cast<IdentifierNode *>(subexpr);

					// Try to find the constant expression by the identifier
					if (current_class->constant_expressions.has(in->name)) {
						Node *cn_exp = current_class->constant_expressions[in->name].expression;
						if (cn_exp->type == Node::TYPE_CONSTANT) {
							cn = static_cast<ConstantNode *>(cn_exp);
							found_constant = true;
						}
					}
				}

				if (found_constant && cn->value.get_type() == Variant::STRING) {
					valid = true;
					path = (String)cn->value;
				}
			}

			if (!valid) {
				_set_error("expected string constant as 'preload' argument.");
				return nullptr;
			}

			if (!path.is_abs_path() && base_path != "") {
				path = base_path.plus_file(path);
			}
			path = path.replace("///", "//").simplify_path();
			if (path == self_path) {
				_set_error("Can't preload itself (use 'get_script()').");
				return nullptr;
			}

			Ref<Resource> res;
			dependencies.push_back(path);
			if (!dependencies_only) {
				if (!validating) {
					//this can be too slow for just validating code
					if (for_completion && ScriptCodeCompletionCache::get_singleton() && FileAccess::exists(path)) {
						res = ScriptCodeCompletionCache::get_singleton()->get_cached_resource(path);
					} else if (!for_completion || FileAccess::exists(path)) {
						res = ResourceLoader::load(path);
					}
				} else {
					if (!FileAccess::exists(path)) {
						_set_error("Can't preload resource at path: " + path);
						return nullptr;
					} else if (ScriptCodeCompletionCache::get_singleton()) {
						res = ScriptCodeCompletionCache::get_singleton()->get_cached_resource(path);
					} else {
						res = ResourceLoader::load(path);
					}
				}
				if (!res.is_valid()) {
					_set_error("Can't preload resource at path: " + path);
					return nullptr;
				}
			}

			if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
				_set_error("Expected ')' after 'preload' path");
				return nullptr;
			}

			Ref<GDScript> gds = res;
			if (gds.is_valid() && !gds->is_valid()) {
				_set_error("Couldn't fully preload the script, possible cyclic reference or compilation error. Use \"load()\" instead if a cyclic reference is intended.");
				return nullptr;
			}

			tokenizer->advance();

			ConstantNode *constant = alloc_node<ConstantNode>();
			constant->value = res;
			constant->datatype = _type_from_variant(constant->value);

			expr = constant;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_YIELD) {
			if (!current_function) {
				_set_error("\"yield()\" can only be used inside function blocks.");
				return nullptr;
			}

			current_function->has_yield = true;

			tokenizer->advance();
			if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
				_set_error("Expected \"(\" after \"yield\".");
				return nullptr;
			}

			tokenizer->advance();

			OperatorNode *yield = alloc_node<OperatorNode>();
			yield->op = OperatorNode::OP_YIELD;

			while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
				tokenizer->advance();
			}

			if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
				expr = yield;
				tokenizer->advance();
			} else {
				parenthesis++;

				Node *object = _parse_and_reduce_expression(p_parent, p_static);
				if (!object) {
					return nullptr;
				}
				yield->arguments.push_back(object);

				if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
					_set_error("Expected \",\" after the first argument of \"yield\".");
					return nullptr;
				}

				tokenizer->advance();

				if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
					completion_cursor = StringName();
					completion_node = object;
					completion_type = COMPLETION_YIELD;
					completion_class = current_class;
					completion_function = current_function;
					completion_line = tokenizer->get_token_line();
					completion_argument = 0;
					completion_block = current_block;
					completion_found = true;
					tokenizer->advance();
				}

				Node *signal = _parse_and_reduce_expression(p_parent, p_static);
				if (!signal) {
					return nullptr;
				}
				yield->arguments.push_back(signal);

				if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
					_set_error("Expected \")\" after the second argument of \"yield\".");
					return nullptr;
				}

				parenthesis--;

				tokenizer->advance();

				expr = yield;
			}

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_SELF) {
			if (p_static) {
				_set_error("\"self\" isn't allowed in a static function or constant expression.");
				return nullptr;
			}
			//constant defined by tokenizer
			SelfNode *self = alloc_node<SelfNode>();
			tokenizer->advance();
			expr = self;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE && tokenizer->get_token(1) == GDScriptTokenizer::TK_PERIOD) {
			Variant::Type bi_type = tokenizer->get_token_type();
			tokenizer->advance(2);

			StringName identifier;

			if (_get_completable_identifier(COMPLETION_BUILT_IN_TYPE_CONSTANT, identifier)) {
				completion_built_in_constant = bi_type;
			}

			if (identifier == StringName()) {
				_set_error("Built-in type constant or static function expected after \".\".");
				return nullptr;
			}
			if (!Variant::has_constant(bi_type, identifier)) {
				if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_OPEN &&
						Variant::is_method_const(bi_type, identifier) &&
						Variant::get_method_return_type(bi_type, identifier) == bi_type) {
					tokenizer->advance();

					OperatorNode *construct = alloc_node<OperatorNode>();
					construct->op = OperatorNode::OP_CALL;

					TypeNode *tn = alloc_node<TypeNode>();
					tn->vtype = bi_type;
					construct->arguments.push_back(tn);

					OperatorNode *op = alloc_node<OperatorNode>();
					op->op = OperatorNode::OP_CALL;
					op->arguments.push_back(construct);

					IdentifierNode *id = alloc_node<IdentifierNode>();
					id->name = identifier;
					op->arguments.push_back(id);

					if (!_parse_arguments(op, op->arguments, p_static, true, p_parsing_constant)) {
						return nullptr;
					}

					expr = op;
				} else {
					// Object is a special case
					bool valid = false;
					if (bi_type == Variant::OBJECT) {
						int object_constant = ClassDB::get_integer_constant("Object", identifier, &valid);
						if (valid) {
							ConstantNode *cn = alloc_node<ConstantNode>();
							cn->value = object_constant;
							cn->datatype = _type_from_variant(cn->value);
							expr = cn;
						}
					}
					if (!valid) {
						_set_error("Static constant  '" + identifier.operator String() + "' not present in built-in type " + Variant::get_type_name(bi_type) + ".");
						return nullptr;
					}
				}
			} else {
				ConstantNode *cn = alloc_node<ConstantNode>();
				cn->value = Variant::get_constant_value(bi_type, identifier);
				cn->datatype = _type_from_variant(cn->value);
				expr = cn;
			}

		} else if (tokenizer->get_token(next_valid_offset) == GDScriptTokenizer::TK_PARENTHESIS_OPEN && tokenizer->is_token_literal()) {
			// We check with is_token_literal, as this allows us to use match/sync/etc. as a name
			//function or constructor

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = OperatorNode::OP_CALL;

			//Do a quick Array and Dictionary Check.  Replace if either require no arguments.
			bool replaced = false;

			if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE) {
				Variant::Type ct = tokenizer->get_token_type();
				if (!p_parsing_constant) {
					if (ct == Variant::ARRAY) {
						if (tokenizer->get_token(2) == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
							ArrayNode *arr = alloc_node<ArrayNode>();
							expr = arr;
							replaced = true;
							tokenizer->advance(3);
						}
					}
					if (ct == Variant::DICTIONARY) {
						if (tokenizer->get_token(2) == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
							DictionaryNode *dict = alloc_node<DictionaryNode>();
							expr = dict;
							replaced = true;
							tokenizer->advance(3);
						}
					}
				}

				if (!replaced) {
					TypeNode *tn = alloc_node<TypeNode>();
					tn->vtype = tokenizer->get_token_type();
					op->arguments.push_back(tn);
					tokenizer->advance(2);
				}
			} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_FUNC) {
				BuiltInFunctionNode *bn = alloc_node<BuiltInFunctionNode>();
				bn->function = tokenizer->get_token_built_in_func();
				op->arguments.push_back(bn);
				tokenizer->advance(2);
			} else {
				SelfNode *self = alloc_node<SelfNode>();
				op->arguments.push_back(self);

				StringName identifier;
				if (_get_completable_identifier(COMPLETION_FUNCTION, identifier)) {
				}

				IdentifierNode *id = alloc_node<IdentifierNode>();
				id->name = identifier;
				op->arguments.push_back(id);
				tokenizer->advance(1);
			}

			if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
				_make_completable_call(0);
				completion_node = op;
			}
			if (!replaced) {
				if (!_parse_arguments(op, op->arguments, p_static, true, p_parsing_constant)) {
					return nullptr;
				}
				expr = op;
			}
		} else if (tokenizer->is_token_literal(0, true)) {
			// We check with is_token_literal, as this allows us to use match/sync/etc. as a name
			//identifier (reference)

			const ClassNode *cln = current_class;
			bool bfn = false;
			StringName identifier;
			int id_line = tokenizer->get_token_line();
			if (_get_completable_identifier(COMPLETION_IDENTIFIER, identifier)) {
			}

			BlockNode *b = current_block;
			while (!bfn && b) {
				if (b->variables.has(identifier)) {
					IdentifierNode *id = alloc_node<IdentifierNode>();
					id->name = identifier;
					id->declared_block = b;
					id->line = id_line;
					expr = id;
					bfn = true;

#ifdef DEBUG_ENABLED
					LocalVarNode *lv = b->variables[identifier];
					switch (tokenizer->get_token()) {
						case GDScriptTokenizer::TK_OP_ASSIGN_ADD:
						case GDScriptTokenizer::TK_OP_ASSIGN_BIT_AND:
						case GDScriptTokenizer::TK_OP_ASSIGN_BIT_OR:
						case GDScriptTokenizer::TK_OP_ASSIGN_BIT_XOR:
						case GDScriptTokenizer::TK_OP_ASSIGN_DIV:
						case GDScriptTokenizer::TK_OP_ASSIGN_MOD:
						case GDScriptTokenizer::TK_OP_ASSIGN_MUL:
						case GDScriptTokenizer::TK_OP_ASSIGN_SHIFT_LEFT:
						case GDScriptTokenizer::TK_OP_ASSIGN_SHIFT_RIGHT:
						case GDScriptTokenizer::TK_OP_ASSIGN_SUB: {
							if (lv->assignments == 0) {
								if (!lv->datatype.has_type) {
									_set_error("Using assignment with operation on a variable that was never assigned.");
									return nullptr;
								}
								_add_warning(GDScriptWarning::UNASSIGNED_VARIABLE_OP_ASSIGN, -1, identifier.operator String());
							}
							FALLTHROUGH;
						}
						case GDScriptTokenizer::TK_OP_ASSIGN: {
							lv->assignments += 1;
							lv->usages--; // Assignment is not really usage
						} break;
						default: {
							lv->usages++;
						}
					}
#endif // DEBUG_ENABLED
					break;
				}
				b = b->parent_block;
			}

			if (!bfn && p_parsing_constant) {
				if (cln->constant_expressions.has(identifier)) {
					expr = cln->constant_expressions[identifier].expression;
					bfn = true;
				} else if (GDScriptLanguage::get_singleton()->get_global_map().has(identifier)) {
					//check from constants
					ConstantNode *constant = alloc_node<ConstantNode>();
					constant->value = GDScriptLanguage::get_singleton()->get_global_array()[GDScriptLanguage::get_singleton()->get_global_map()[identifier]];
					constant->datatype = _type_from_variant(constant->value);
					constant->line = id_line;
					expr = constant;
					bfn = true;
				}

				if (!bfn && GDScriptLanguage::get_singleton()->get_named_globals_map().has(identifier)) {
					//check from singletons
					ConstantNode *constant = alloc_node<ConstantNode>();
					constant->value = GDScriptLanguage::get_singleton()->get_named_globals_map()[identifier];
					expr = constant;
					bfn = true;
				}

				if (!dependencies_only) {
					if (!bfn && ScriptServer::is_global_class(identifier)) {
						Ref<Script> scr = ResourceLoader::load(ScriptServer::get_global_class_path(identifier));
						if (scr.is_valid() && scr->is_valid()) {
							ConstantNode *constant = alloc_node<ConstantNode>();
							constant->value = scr;
							expr = constant;
							bfn = true;
						}
					}

					// Check parents for the constant
					if (!bfn) {
						// Using current_class instead of cln here, since cln is const*
						_determine_inheritance(current_class, false);
						if (cln->base_type.has_type && cln->base_type.kind == DataType::GDSCRIPT && cln->base_type.script_type->is_valid()) {
							Map<StringName, Variant> parent_constants;
							current_class->base_type.script_type->get_constants(&parent_constants);
							if (parent_constants.has(identifier)) {
								ConstantNode *constant = alloc_node<ConstantNode>();
								constant->value = parent_constants[identifier];
								expr = constant;
								bfn = true;
							}
						}
					}
				}
			}

			if (!bfn) {
#ifdef DEBUG_ENABLED
				if (current_function) {
					int arg_idx = current_function->arguments.find(identifier);
					if (arg_idx != -1) {
						switch (tokenizer->get_token()) {
							case GDScriptTokenizer::TK_OP_ASSIGN_ADD:
							case GDScriptTokenizer::TK_OP_ASSIGN_BIT_AND:
							case GDScriptTokenizer::TK_OP_ASSIGN_BIT_OR:
							case GDScriptTokenizer::TK_OP_ASSIGN_BIT_XOR:
							case GDScriptTokenizer::TK_OP_ASSIGN_DIV:
							case GDScriptTokenizer::TK_OP_ASSIGN_MOD:
							case GDScriptTokenizer::TK_OP_ASSIGN_MUL:
							case GDScriptTokenizer::TK_OP_ASSIGN_SHIFT_LEFT:
							case GDScriptTokenizer::TK_OP_ASSIGN_SHIFT_RIGHT:
							case GDScriptTokenizer::TK_OP_ASSIGN_SUB:
							case GDScriptTokenizer::TK_OP_ASSIGN: {
								// Assignment is not really usage
							} break;
							default: {
								current_function->arguments_usage.write[arg_idx] = current_function->arguments_usage[arg_idx] + 1;
							}
						}
					}
				}
#endif // DEBUG_ENABLED
				IdentifierNode *id = alloc_node<IdentifierNode>();
				id->name = identifier;
				id->line = id_line;
				expr = id;
			}

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_ADD || tokenizer->get_token() == GDScriptTokenizer::TK_OP_SUB || tokenizer->get_token() == GDScriptTokenizer::TK_OP_NOT || tokenizer->get_token() == GDScriptTokenizer::TK_OP_BIT_INVERT) {
			//single prefix operators like !expr +expr -expr ++expr --expr
			alloc_node<OperatorNode>();
			Expression e;
			e.is_op = true;

			switch (tokenizer->get_token()) {
				case GDScriptTokenizer::TK_OP_ADD:
					e.op = OperatorNode::OP_POS;
					break;
				case GDScriptTokenizer::TK_OP_SUB:
					e.op = OperatorNode::OP_NEG;
					break;
				case GDScriptTokenizer::TK_OP_NOT:
					e.op = OperatorNode::OP_NOT;
					break;
				case GDScriptTokenizer::TK_OP_BIT_INVERT:
					e.op = OperatorNode::OP_BIT_INVERT;
					break;
				default: {
				}
			}

			tokenizer->advance();

			if (e.op != OperatorNode::OP_NOT && tokenizer->get_token() == GDScriptTokenizer::TK_OP_NOT) {
				_set_error("Misplaced 'not'.");
				return nullptr;
			}

			expression.push_back(e);
			continue; //only exception, must continue...

			/*
			Node *subexpr=_parse_expression(op,p_static);
			if (!subexpr)
				return NULL;
			op->arguments.push_back(subexpr);
			expr=op;*/

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_IS && tokenizer->get_token(1) == GDScriptTokenizer::TK_BUILT_IN_TYPE) {
			// 'is' operator with built-in type
			if (!expr) {
				_set_error("Expected identifier before 'is' operator");
				return nullptr;
			}
			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = OperatorNode::OP_IS_BUILTIN;
			op->arguments.push_back(expr);

			tokenizer->advance();

			TypeNode *tn = alloc_node<TypeNode>();
			tn->vtype = tokenizer->get_token_type();
			op->arguments.push_back(tn);
			tokenizer->advance();

			expr = op;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_OPEN) {
			// array
			tokenizer->advance();

			ArrayNode *arr = alloc_node<ArrayNode>();
			bool expecting_comma = false;

			while (true) {
				if (tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {
					_set_error("Unterminated array");
					return nullptr;

				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_CLOSE) {
					tokenizer->advance();
					break;
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
					tokenizer->advance(); //ignore newline
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
					if (!expecting_comma) {
						_set_error("expression or ']' expected");
						return nullptr;
					}

					expecting_comma = false;
					tokenizer->advance(); //ignore newline
				} else {
					//parse expression
					if (expecting_comma) {
						_set_error("',' or ']' expected");
						return nullptr;
					}
					Node *n = _parse_expression(arr, p_static, p_allow_assign, p_parsing_constant);
					if (!n) {
						return nullptr;
					}
					arr->elements.push_back(n);
					expecting_comma = true;
				}
			}

			expr = arr;
		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_OPEN) {
			// array
			tokenizer->advance();

			DictionaryNode *dict = alloc_node<DictionaryNode>();

			enum DictExpect {

				DICT_EXPECT_KEY,
				DICT_EXPECT_COLON,
				DICT_EXPECT_VALUE,
				DICT_EXPECT_COMMA

			};

			Node *key = nullptr;
			Set<Variant> keys;

			DictExpect expecting = DICT_EXPECT_KEY;

			while (true) {
				if (tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {
					_set_error("Unterminated dictionary");
					return nullptr;

				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {
					if (expecting == DICT_EXPECT_COLON) {
						_set_error("':' expected");
						return nullptr;
					}
					if (expecting == DICT_EXPECT_VALUE) {
						_set_error("value expected");
						return nullptr;
					}
					tokenizer->advance();
					break;
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
					tokenizer->advance(); //ignore newline
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
					if (expecting == DICT_EXPECT_KEY) {
						_set_error("key or '}' expected");
						return nullptr;
					}
					if (expecting == DICT_EXPECT_VALUE) {
						_set_error("value expected");
						return nullptr;
					}
					if (expecting == DICT_EXPECT_COLON) {
						_set_error("':' expected");
						return nullptr;
					}

					expecting = DICT_EXPECT_KEY;
					tokenizer->advance(); //ignore newline

				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
					if (expecting == DICT_EXPECT_KEY) {
						_set_error("key or '}' expected");
						return nullptr;
					}
					if (expecting == DICT_EXPECT_VALUE) {
						_set_error("value expected");
						return nullptr;
					}
					if (expecting == DICT_EXPECT_COMMA) {
						_set_error("',' or '}' expected");
						return nullptr;
					}

					expecting = DICT_EXPECT_VALUE;
					tokenizer->advance(); //ignore newline
				} else {
					if (expecting == DICT_EXPECT_COMMA) {
						_set_error("',' or '}' expected");
						return nullptr;
					}
					if (expecting == DICT_EXPECT_COLON) {
						_set_error("':' expected");
						return nullptr;
					}

					if (expecting == DICT_EXPECT_KEY) {
						if (tokenizer->is_token_literal() && tokenizer->get_token(1) == GDScriptTokenizer::TK_OP_ASSIGN) {
							// We check with is_token_literal, as this allows us to use match/sync/etc. as a name
							//lua style identifier, easier to write
							ConstantNode *cn = alloc_node<ConstantNode>();
							cn->value = tokenizer->get_token_literal();
							cn->datatype = _type_from_variant(cn->value);
							key = cn;
							tokenizer->advance(2);
							expecting = DICT_EXPECT_VALUE;
						} else {
							//python/js style more flexible
							key = _parse_expression(dict, p_static, p_allow_assign, p_parsing_constant);
							if (!key) {
								return nullptr;
							}
							expecting = DICT_EXPECT_COLON;
						}
					}

					if (expecting == DICT_EXPECT_VALUE) {
						Node *value = _parse_expression(dict, p_static, p_allow_assign, p_parsing_constant);
						if (!value) {
							return nullptr;
						}
						expecting = DICT_EXPECT_COMMA;

						const Variant *key_value = _try_to_find_constant_value_for_expression(key);
						if (key_value) {
							if (keys.has(*key_value)) {
								_set_error("Duplicate key \"" + String(*key_value) + "\" found in Dictionary literal",
										key->line,
										key->column);
								return nullptr;
							}
							keys.insert(*key_value);
						}

						DictionaryNode::Pair pair;
						pair.key = key;
						pair.value = value;
						dict->elements.push_back(pair);
						key = nullptr;
					}
				}
			}

			expr = dict;

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD && (tokenizer->is_token_literal(1) || tokenizer->get_token(1) == GDScriptTokenizer::TK_CURSOR)) {
			// We check with is_token_literal, as this allows us to use match/sync/etc. as a name
			// parent call

			tokenizer->advance(); //goto identifier
			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = OperatorNode::OP_PARENT_CALL;

			/*SelfNode *self = alloc_node<SelfNode>();
			op->arguments.push_back(self);
			forbidden for now */
			StringName identifier;
			bool is_completion = _get_completable_identifier(COMPLETION_PARENT_FUNCTION, identifier) && for_completion;

			IdentifierNode *id = alloc_node<IdentifierNode>();
			id->name = identifier;
			op->arguments.push_back(id);

			if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
				if (!is_completion) {
					_set_error("Expected '(' for parent function call.");
					return nullptr;
				}
			} else {
				tokenizer->advance();
				if (!_parse_arguments(op, op->arguments, p_static, false, p_parsing_constant)) {
					return nullptr;
				}
			}

			expr = op;

		} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE && expression.size() > 0 && expression[expression.size() - 1].is_op && expression[expression.size() - 1].op == OperatorNode::OP_IS) {
			Expression e = expression[expression.size() - 1];
			e.op = OperatorNode::OP_IS_BUILTIN;
			expression.write[expression.size() - 1] = e;

			TypeNode *tn = alloc_node<TypeNode>();
			tn->vtype = tokenizer->get_token_type();
			expr = tn;
			tokenizer->advance();
		} else {
			//find list [ or find dictionary {
			_set_error("Error parsing expression, misplaced: " + String(tokenizer->get_token_name(tokenizer->get_token())));
			return nullptr; //nothing
		}

		ERR_FAIL_COND_V_MSG(!expr, nullptr, "GDScriptParser bug, couldn't figure out what expression is.");

		/******************/
		/* Parse Indexing */
		/******************/

		while (true) {
			//expressions can be indexed any number of times

			if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD) {
				//indexing using "."

				if (tokenizer->get_token(1) != GDScriptTokenizer::TK_CURSOR && !tokenizer->is_token_literal(1)) {
					// We check with is_token_literal, as this allows us to use match/sync/etc. as a name
					_set_error("Expected identifier as member");
					return nullptr;
				} else if (tokenizer->get_token(2) == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
					//call!!
					OperatorNode *op = alloc_node<OperatorNode>();
					op->op = OperatorNode::OP_CALL;

					tokenizer->advance();

					IdentifierNode *id = alloc_node<IdentifierNode>();
					StringName identifier;
					if (_get_completable_identifier(COMPLETION_METHOD, identifier)) {
						completion_node = op;
						//indexing stuff
					}

					id->name = identifier;

					op->arguments.push_back(expr); // call what
					op->arguments.push_back(id); // call func
					//get arguments
					tokenizer->advance(1);
					if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
						_make_completable_call(0);
						completion_node = op;
					}
					if (!_parse_arguments(op, op->arguments, p_static, true, p_parsing_constant)) {
						return nullptr;
					}
					expr = op;

				} else {
					//simple indexing!

					OperatorNode *op = alloc_node<OperatorNode>();
					op->op = OperatorNode::OP_INDEX_NAMED;
					tokenizer->advance();

					StringName identifier;
					if (_get_completable_identifier(COMPLETION_INDEX, identifier)) {
						if (identifier == StringName()) {
							identifier = "@temp"; //so it parses alright
						}
						completion_node = op;

						//indexing stuff
					}

					IdentifierNode *id = alloc_node<IdentifierNode>();
					id->name = identifier;

					op->arguments.push_back(expr);
					op->arguments.push_back(id);

					expr = op;
				}

			} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_OPEN) {
				//indexing using "[]"
				OperatorNode *op = alloc_node<OperatorNode>();
				op->op = OperatorNode::OP_INDEX;

				tokenizer->advance(1);

				parenthesis++;
				Node *subexpr = _parse_expression(op, p_static, p_allow_assign, p_parsing_constant);
				parenthesis--;
				if (!subexpr) {
					return nullptr;
				}

				if (tokenizer->get_token() != GDScriptTokenizer::TK_BRACKET_CLOSE) {
					_set_error("Expected ']'");
					return nullptr;
				}

				op->arguments.push_back(expr);
				op->arguments.push_back(subexpr);
				tokenizer->advance(1);
				expr = op;

			} else {
				break;
			}
		}

		/*****************/
		/* Parse Casting */
		/*****************/

		bool has_casting = expr->type == Node::TYPE_CAST;
		if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_AS) {
			if (has_casting) {
				_set_error("Unexpected 'as'.");
				return nullptr;
			}
			CastNode *cn = alloc_node<CastNode>();
			if (!_parse_type(cn->cast_type)) {
				_set_error("Expected type after 'as'.");
				return nullptr;
			}
			has_casting = true;
			cn->source_node = expr;
			expr = cn;
		}

		/******************/
		/* Parse Operator */
		/******************/

		if (parenthesis > 0) {
			//remove empty space (only allowed if inside parenthesis
			while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
				tokenizer->advance();
			}
		}

		Expression e;
		e.is_op = false;
		e.node = expr;
		expression.push_back(e);

		// determine which operator is next

		OperatorNode::Operator op;
		bool valid = true;

//assign, if allowed is only allowed on the first operator
#define _VALIDATE_ASSIGN                  \
	if (!p_allow_assign || has_casting) { \
		_set_error("Unexpected assign."); \
		return NULL;                      \
	}                                     \
	p_allow_assign = false;

		switch (tokenizer->get_token()) { //see operator

			case GDScriptTokenizer::TK_OP_IN:
				op = OperatorNode::OP_IN;
				break;
			case GDScriptTokenizer::TK_OP_EQUAL:
				op = OperatorNode::OP_EQUAL;
				break;
			case GDScriptTokenizer::TK_OP_NOT_EQUAL:
				op = OperatorNode::OP_NOT_EQUAL;
				break;
			case GDScriptTokenizer::TK_OP_LESS:
				op = OperatorNode::OP_LESS;
				break;
			case GDScriptTokenizer::TK_OP_LESS_EQUAL:
				op = OperatorNode::OP_LESS_EQUAL;
				break;
			case GDScriptTokenizer::TK_OP_GREATER:
				op = OperatorNode::OP_GREATER;
				break;
			case GDScriptTokenizer::TK_OP_GREATER_EQUAL:
				op = OperatorNode::OP_GREATER_EQUAL;
				break;
			case GDScriptTokenizer::TK_OP_AND:
				op = OperatorNode::OP_AND;
				break;
			case GDScriptTokenizer::TK_OP_OR:
				op = OperatorNode::OP_OR;
				break;
			case GDScriptTokenizer::TK_OP_ADD:
				op = OperatorNode::OP_ADD;
				break;
			case GDScriptTokenizer::TK_OP_SUB:
				op = OperatorNode::OP_SUB;
				break;
			case GDScriptTokenizer::TK_OP_MUL:
				op = OperatorNode::OP_MUL;
				break;
			case GDScriptTokenizer::TK_OP_DIV:
				op = OperatorNode::OP_DIV;
				break;
			case GDScriptTokenizer::TK_OP_MOD:
				op = OperatorNode::OP_MOD;
				break;
			//case GDScriptTokenizer::TK_OP_NEG: op=OperatorNode::OP_NEG ; break;
			case GDScriptTokenizer::TK_OP_SHIFT_LEFT:
				op = OperatorNode::OP_SHIFT_LEFT;
				break;
			case GDScriptTokenizer::TK_OP_SHIFT_RIGHT:
				op = OperatorNode::OP_SHIFT_RIGHT;
				break;
			case GDScriptTokenizer::TK_OP_ASSIGN: {
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN;

				if (tokenizer->get_token(1) == GDScriptTokenizer::TK_CURSOR) {
					//code complete assignment
					completion_type = COMPLETION_ASSIGN;
					completion_node = expr;
					completion_class = current_class;
					completion_function = current_function;
					completion_line = tokenizer->get_token_line();
					completion_block = current_block;
					completion_found = true;
					tokenizer->advance();
				}

			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_ADD:
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_ADD;
				break;
			case GDScriptTokenizer::TK_OP_ASSIGN_SUB:
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_SUB;
				break;
			case GDScriptTokenizer::TK_OP_ASSIGN_MUL:
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_MUL;
				break;
			case GDScriptTokenizer::TK_OP_ASSIGN_DIV:
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_DIV;
				break;
			case GDScriptTokenizer::TK_OP_ASSIGN_MOD:
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_MOD;
				break;
			case GDScriptTokenizer::TK_OP_ASSIGN_SHIFT_LEFT:
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_SHIFT_LEFT;
				break;
			case GDScriptTokenizer::TK_OP_ASSIGN_SHIFT_RIGHT:
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_SHIFT_RIGHT;
				break;
			case GDScriptTokenizer::TK_OP_ASSIGN_BIT_AND:
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_BIT_AND;
				break;
			case GDScriptTokenizer::TK_OP_ASSIGN_BIT_OR:
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_BIT_OR;
				break;
			case GDScriptTokenizer::TK_OP_ASSIGN_BIT_XOR:
				_VALIDATE_ASSIGN op = OperatorNode::OP_ASSIGN_BIT_XOR;
				break;
			case GDScriptTokenizer::TK_OP_BIT_AND:
				op = OperatorNode::OP_BIT_AND;
				break;
			case GDScriptTokenizer::TK_OP_BIT_OR:
				op = OperatorNode::OP_BIT_OR;
				break;
			case GDScriptTokenizer::TK_OP_BIT_XOR:
				op = OperatorNode::OP_BIT_XOR;
				break;
			case GDScriptTokenizer::TK_PR_IS:
				op = OperatorNode::OP_IS;
				break;
			case GDScriptTokenizer::TK_CF_IF:
				op = OperatorNode::OP_TERNARY_IF;
				break;
			case GDScriptTokenizer::TK_CF_ELSE:
				op = OperatorNode::OP_TERNARY_ELSE;
				break;
			default:
				valid = false;
				break;
		}

		if (valid) {
			e.is_op = true;
			e.op = op;
			expression.push_back(e);
			tokenizer->advance();
		} else {
			break;
		}
	}

	/* Reduce the set set of expressions and place them in an operator tree, respecting precedence */

	while (expression.size() > 1) {
		int next_op = -1;
		int min_priority = 0xFFFFF;
		bool is_unary = false;
		bool is_ternary = false;

		for (int i = 0; i < expression.size(); i++) {
			if (!expression[i].is_op) {
				continue;
			}

			int priority;

			bool unary = false;
			bool ternary = false;
			bool error = false;
			bool right_to_left = false;

			switch (expression[i].op) {
				case OperatorNode::OP_IS:
				case OperatorNode::OP_IS_BUILTIN:
					priority = -1;
					break; //before anything

				case OperatorNode::OP_BIT_INVERT:
					priority = 0;
					unary = true;
					break;
				case OperatorNode::OP_NEG:
				case OperatorNode::OP_POS:
					priority = 1;
					unary = true;
					break;

				case OperatorNode::OP_MUL:
					priority = 2;
					break;
				case OperatorNode::OP_DIV:
					priority = 2;
					break;
				case OperatorNode::OP_MOD:
					priority = 2;
					break;

				case OperatorNode::OP_ADD:
					priority = 3;
					break;
				case OperatorNode::OP_SUB:
					priority = 3;
					break;

				case OperatorNode::OP_SHIFT_LEFT:
					priority = 4;
					break;
				case OperatorNode::OP_SHIFT_RIGHT:
					priority = 4;
					break;

				case OperatorNode::OP_BIT_AND:
					priority = 5;
					break;
				case OperatorNode::OP_BIT_XOR:
					priority = 6;
					break;
				case OperatorNode::OP_BIT_OR:
					priority = 7;
					break;

				case OperatorNode::OP_LESS:
					priority = 8;
					break;
				case OperatorNode::OP_LESS_EQUAL:
					priority = 8;
					break;
				case OperatorNode::OP_GREATER:
					priority = 8;
					break;
				case OperatorNode::OP_GREATER_EQUAL:
					priority = 8;
					break;

				case OperatorNode::OP_EQUAL:
					priority = 8;
					break;
				case OperatorNode::OP_NOT_EQUAL:
					priority = 8;
					break;

				case OperatorNode::OP_IN:
					priority = 10;
					break;

				case OperatorNode::OP_NOT:
					priority = 11;
					unary = true;
					break;
				case OperatorNode::OP_AND:
					priority = 12;
					break;
				case OperatorNode::OP_OR:
					priority = 13;
					break;

				case OperatorNode::OP_TERNARY_IF:
					priority = 14;
					ternary = true;
					right_to_left = true;
					break;
				case OperatorNode::OP_TERNARY_ELSE:
					priority = 14;
					error = true;
					// Rigth-to-left should be false in this case, otherwise it would always error.
					break;

				case OperatorNode::OP_ASSIGN:
					priority = 15;
					break;
				case OperatorNode::OP_ASSIGN_ADD:
					priority = 15;
					break;
				case OperatorNode::OP_ASSIGN_SUB:
					priority = 15;
					break;
				case OperatorNode::OP_ASSIGN_MUL:
					priority = 15;
					break;
				case OperatorNode::OP_ASSIGN_DIV:
					priority = 15;
					break;
				case OperatorNode::OP_ASSIGN_MOD:
					priority = 15;
					break;
				case OperatorNode::OP_ASSIGN_SHIFT_LEFT:
					priority = 15;
					break;
				case OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
					priority = 15;
					break;
				case OperatorNode::OP_ASSIGN_BIT_AND:
					priority = 15;
					break;
				case OperatorNode::OP_ASSIGN_BIT_OR:
					priority = 15;
					break;
				case OperatorNode::OP_ASSIGN_BIT_XOR:
					priority = 15;
					break;

				default: {
					_set_error("GDScriptParser bug, invalid operator in expression: " + itos(expression[i].op));
					return nullptr;
				}
			}

			if (priority < min_priority || (right_to_left && priority == min_priority)) {
				// < is used for left to right (default)
				// <= is used for right to left
				if (error) {
					_set_error("Unexpected operator");
					return nullptr;
				}
				next_op = i;
				min_priority = priority;
				is_unary = unary;
				is_ternary = ternary;
			}
		}

		if (next_op == -1) {
			_set_error("Yet another parser bug....");
			ERR_FAIL_V(nullptr);
		}

		// OK! create operator..
		if (is_unary) {
			int expr_pos = next_op;
			while (expression[expr_pos].is_op) {
				expr_pos++;
				if (expr_pos == expression.size()) {
					//can happen..
					_set_error("Unexpected end of expression...");
					return nullptr;
				}
			}

			//consecutively do unary operators
			for (int i = expr_pos - 1; i >= next_op; i--) {
				OperatorNode *op = alloc_node<OperatorNode>();
				op->op = expression[i].op;
				op->arguments.push_back(expression[i + 1].node);
				op->line = op_line; //line might have been changed from a \n
				expression.write[i].is_op = false;
				expression.write[i].node = op;
				expression.remove(i + 1);
			}

		} else if (is_ternary) {
			if (next_op < 1 || next_op >= (expression.size() - 1)) {
				_set_error("Parser bug...");
				ERR_FAIL_V(nullptr);
			}

			if (next_op >= (expression.size() - 2) || expression[next_op + 2].op != OperatorNode::OP_TERNARY_ELSE) {
				_set_error("Expected else after ternary if.");
				return nullptr;
			}
			if (next_op >= (expression.size() - 3)) {
				_set_error("Expected value after ternary else.");
				return nullptr;
			}

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = expression[next_op].op;
			op->line = op_line; //line might have been changed from a \n

			if (expression[next_op - 1].is_op) {
				_set_error("Parser bug...");
				ERR_FAIL_V(nullptr);
			}

			if (expression[next_op + 1].is_op) {
				// this is not invalid and can really appear
				// but it becomes invalid anyway because no binary op
				// can be followed by a unary op in a valid combination,
				// due to how precedence works, unaries will always disappear first

				_set_error("Unexpected two consecutive operators after ternary if.");
				return nullptr;
			}

			if (expression[next_op + 3].is_op) {
				// this is not invalid and can really appear
				// but it becomes invalid anyway because no binary op
				// can be followed by a unary op in a valid combination,
				// due to how precedence works, unaries will always disappear first

				_set_error("Unexpected two consecutive operators after ternary else.");
				return nullptr;
			}

			op->arguments.push_back(expression[next_op + 1].node); //next expression goes as first
			op->arguments.push_back(expression[next_op - 1].node); //left expression goes as when-true
			op->arguments.push_back(expression[next_op + 3].node); //expression after next goes as when-false

			//replace all 3 nodes by this operator and make it an expression
			expression.write[next_op - 1].node = op;
			expression.remove(next_op);
			expression.remove(next_op);
			expression.remove(next_op);
			expression.remove(next_op);
		} else {
			if (next_op < 1 || next_op >= (expression.size() - 1)) {
				_set_error("Parser bug...");
				ERR_FAIL_V(nullptr);
			}

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = expression[next_op].op;
			op->line = op_line; //line might have been changed from a \n

			if (expression[next_op - 1].is_op) {
				_set_error("Parser bug...");
				ERR_FAIL_V(nullptr);
			}

			if (expression[next_op + 1].is_op) {
				// this is not invalid and can really appear
				// but it becomes invalid anyway because no binary op
				// can be followed by a unary op in a valid combination,
				// due to how precedence works, unaries will always disappear first

				_set_error("Unexpected two consecutive operators.");
				return nullptr;
			}

			op->arguments.push_back(expression[next_op - 1].node); //expression goes as left
			op->arguments.push_back(expression[next_op + 1].node); //next expression goes as right

			//replace all 3 nodes by this operator and make it an expression
			expression.write[next_op - 1].node = op;
			expression.remove(next_op);
			expression.remove(next_op);
		}
	}

	return expression[0].node;
}

GDScriptParser::Node *GDScriptParser::_reduce_expression(Node *p_node, bool p_to_const) {
	switch (p_node->type) {
		case Node::TYPE_BUILT_IN_FUNCTION: {
			//many may probably be optimizable
			return p_node;
		} break;
		case Node::TYPE_ARRAY: {
			ArrayNode *an = static_cast<ArrayNode *>(p_node);
			bool all_constants = true;

			for (int i = 0; i < an->elements.size(); i++) {
				an->elements.write[i] = _reduce_expression(an->elements[i], p_to_const);
				if (an->elements[i]->type != Node::TYPE_CONSTANT) {
					all_constants = false;
				}
			}

			if (all_constants && p_to_const) {
				//reduce constant array expression

				ConstantNode *cn = alloc_node<ConstantNode>();
				Array arr;
				arr.resize(an->elements.size());
				for (int i = 0; i < an->elements.size(); i++) {
					ConstantNode *acn = static_cast<ConstantNode *>(an->elements[i]);
					arr[i] = acn->value;
				}
				cn->value = arr;
				cn->datatype = _type_from_variant(cn->value);
				return cn;
			}

			return an;

		} break;
		case Node::TYPE_DICTIONARY: {
			DictionaryNode *dn = static_cast<DictionaryNode *>(p_node);
			bool all_constants = true;

			for (int i = 0; i < dn->elements.size(); i++) {
				dn->elements.write[i].key = _reduce_expression(dn->elements[i].key, p_to_const);
				if (dn->elements[i].key->type != Node::TYPE_CONSTANT) {
					all_constants = false;
				}
				dn->elements.write[i].value = _reduce_expression(dn->elements[i].value, p_to_const);
				if (dn->elements[i].value->type != Node::TYPE_CONSTANT) {
					all_constants = false;
				}
			}

			if (all_constants && p_to_const) {
				//reduce constant array expression

				ConstantNode *cn = alloc_node<ConstantNode>();
				Dictionary dict;
				for (int i = 0; i < dn->elements.size(); i++) {
					ConstantNode *key_c = static_cast<ConstantNode *>(dn->elements[i].key);
					ConstantNode *value_c = static_cast<ConstantNode *>(dn->elements[i].value);

					dict[key_c->value] = value_c->value;
				}
				cn->value = dict;
				cn->datatype = _type_from_variant(cn->value);
				return cn;
			}

			return dn;

		} break;
		case Node::TYPE_OPERATOR: {
			OperatorNode *op = static_cast<OperatorNode *>(p_node);

			bool all_constants = true;
			int last_not_constant = -1;

			for (int i = 0; i < op->arguments.size(); i++) {
				op->arguments.write[i] = _reduce_expression(op->arguments[i], p_to_const);
				if (op->arguments[i]->type != Node::TYPE_CONSTANT) {
					all_constants = false;
					last_not_constant = i;
				}
			}

			if (op->op == OperatorNode::OP_IS) {
				//nothing much
				return op;
			}
			if (op->op == OperatorNode::OP_PARENT_CALL) {
				//nothing much
				return op;

			} else if (op->op == OperatorNode::OP_CALL) {
				//can reduce base type constructors
				if ((op->arguments[0]->type == Node::TYPE_TYPE || (op->arguments[0]->type == Node::TYPE_BUILT_IN_FUNCTION && GDScriptFunctions::is_deterministic(static_cast<BuiltInFunctionNode *>(op->arguments[0])->function))) && last_not_constant == 0) {
					//native type constructor or intrinsic function
					const Variant **vptr = nullptr;
					Vector<Variant *> ptrs;
					if (op->arguments.size() > 1) {
						ptrs.resize(op->arguments.size() - 1);
						for (int i = 0; i < ptrs.size(); i++) {
							ConstantNode *cn = static_cast<ConstantNode *>(op->arguments[i + 1]);
							ptrs.write[i] = &cn->value;
						}

						vptr = (const Variant **)&ptrs[0];
					}

					Variant::CallError ce;
					Variant v;

					if (op->arguments[0]->type == Node::TYPE_TYPE) {
						TypeNode *tn = static_cast<TypeNode *>(op->arguments[0]);
						v = Variant::construct(tn->vtype, vptr, ptrs.size(), ce);

					} else {
						GDScriptFunctions::Function func = static_cast<BuiltInFunctionNode *>(op->arguments[0])->function;
						GDScriptFunctions::call(func, vptr, ptrs.size(), v, ce);
					}

					if (ce.error != Variant::CallError::CALL_OK) {
						String errwhere;
						if (op->arguments[0]->type == Node::TYPE_TYPE) {
							TypeNode *tn = static_cast<TypeNode *>(op->arguments[0]);
							errwhere = "'" + Variant::get_type_name(tn->vtype) + "' constructor";

						} else {
							GDScriptFunctions::Function func = static_cast<BuiltInFunctionNode *>(op->arguments[0])->function;
							errwhere = String("'") + GDScriptFunctions::get_func_name(func) + "' intrinsic function";
						}

						switch (ce.error) {
							case Variant::CallError::CALL_ERROR_INVALID_ARGUMENT: {
								_set_error("Invalid argument (#" + itos(ce.argument + 1) + ") for " + errwhere + ".");

							} break;
							case Variant::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS: {
								_set_error("Too many arguments for " + errwhere + ".");
							} break;
							case Variant::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS: {
								_set_error("Too few arguments for " + errwhere + ".");
							} break;
							default: {
								_set_error("Invalid arguments for " + errwhere + ".");

							} break;
						}

						error_line = op->line;

						return p_node;
					}

					ConstantNode *cn = alloc_node<ConstantNode>();
					cn->value = v;
					cn->datatype = _type_from_variant(v);
					return cn;
				}

				return op; //don't reduce yet

			} else if (op->op == OperatorNode::OP_YIELD) {
				return op;

			} else if (op->op == OperatorNode::OP_INDEX) {
				//can reduce indices into constant arrays or dictionaries

				if (all_constants) {
					ConstantNode *ca = static_cast<ConstantNode *>(op->arguments[0]);
					ConstantNode *cb = static_cast<ConstantNode *>(op->arguments[1]);

					bool valid;

					Variant v = ca->value.get(cb->value, &valid);
					if (!valid) {
						_set_error("invalid index in constant expression");
						error_line = op->line;
						return op;
					}

					ConstantNode *cn = alloc_node<ConstantNode>();
					cn->value = v;
					cn->datatype = _type_from_variant(v);
					return cn;
				}

				return op;

			} else if (op->op == OperatorNode::OP_INDEX_NAMED) {
				if (op->arguments[0]->type == Node::TYPE_CONSTANT && op->arguments[1]->type == Node::TYPE_IDENTIFIER) {
					ConstantNode *ca = static_cast<ConstantNode *>(op->arguments[0]);
					IdentifierNode *ib = static_cast<IdentifierNode *>(op->arguments[1]);

					bool valid;
					Variant v = ca->value.get_named(ib->name, &valid);
					if (!valid) {
						_set_error("invalid index '" + String(ib->name) + "' in constant expression");
						error_line = op->line;
						return op;
					}

					ConstantNode *cn = alloc_node<ConstantNode>();
					cn->value = v;
					cn->datatype = _type_from_variant(v);
					return cn;
				}

				return op;
			}

			//validate assignment (don't assign to constant expression
			switch (op->op) {
				case OperatorNode::OP_ASSIGN:
				case OperatorNode::OP_ASSIGN_ADD:
				case OperatorNode::OP_ASSIGN_SUB:
				case OperatorNode::OP_ASSIGN_MUL:
				case OperatorNode::OP_ASSIGN_DIV:
				case OperatorNode::OP_ASSIGN_MOD:
				case OperatorNode::OP_ASSIGN_SHIFT_LEFT:
				case OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
				case OperatorNode::OP_ASSIGN_BIT_AND:
				case OperatorNode::OP_ASSIGN_BIT_OR:
				case OperatorNode::OP_ASSIGN_BIT_XOR: {
					if (op->arguments[0]->type == Node::TYPE_CONSTANT) {
						_set_error("Can't assign to constant", tokenizer->get_token_line() - 1);
						error_line = op->line;
						return op;
					} else if (op->arguments[0]->type == Node::TYPE_SELF) {
						_set_error("Can't assign to self.", op->line);
						error_line = op->line;
						return op;
					}

					if (op->arguments[0]->type == Node::TYPE_OPERATOR) {
						OperatorNode *on = static_cast<OperatorNode *>(op->arguments[0]);
						if (on->op != OperatorNode::OP_INDEX && on->op != OperatorNode::OP_INDEX_NAMED) {
							_set_error("Can't assign to an expression", tokenizer->get_token_line() - 1);
							error_line = op->line;
							return op;
						}
					}

				} break;
				default: {
					break;
				}
			}
			//now se if all are constants
			if (!all_constants) {
				return op; //nothing to reduce from here on
			}
#define _REDUCE_UNARY(m_vop)                                                                               \
	bool valid = false;                                                                                    \
	Variant res;                                                                                           \
	Variant::evaluate(m_vop, static_cast<ConstantNode *>(op->arguments[0])->value, Variant(), res, valid); \
	if (!valid) {                                                                                          \
		_set_error("Invalid operand for unary operator");                                                  \
		error_line = op->line;                                                                             \
		return p_node;                                                                                     \
	}                                                                                                      \
	ConstantNode *cn = alloc_node<ConstantNode>();                                                         \
	cn->value = res;                                                                                       \
	cn->datatype = _type_from_variant(res);                                                                \
	return cn;

#define _REDUCE_BINARY(m_vop)                                                                                                                         \
	bool valid = false;                                                                                                                               \
	Variant res;                                                                                                                                      \
	Variant::evaluate(m_vop, static_cast<ConstantNode *>(op->arguments[0])->value, static_cast<ConstantNode *>(op->arguments[1])->value, res, valid); \
	if (!valid) {                                                                                                                                     \
		_set_error("Invalid operands for operator");                                                                                                  \
		error_line = op->line;                                                                                                                        \
		return p_node;                                                                                                                                \
	}                                                                                                                                                 \
	ConstantNode *cn = alloc_node<ConstantNode>();                                                                                                    \
	cn->value = res;                                                                                                                                  \
	cn->datatype = _type_from_variant(res);                                                                                                           \
	return cn;

			switch (op->op) {
				//unary operators
				case OperatorNode::OP_NEG: {
					_REDUCE_UNARY(Variant::OP_NEGATE);
				} break;
				case OperatorNode::OP_POS: {
					_REDUCE_UNARY(Variant::OP_POSITIVE);
				} break;
				case OperatorNode::OP_NOT: {
					_REDUCE_UNARY(Variant::OP_NOT);
				} break;
				case OperatorNode::OP_BIT_INVERT: {
					_REDUCE_UNARY(Variant::OP_BIT_NEGATE);
				} break;
				//binary operators (in precedence order)
				case OperatorNode::OP_IN: {
					_REDUCE_BINARY(Variant::OP_IN);
				} break;
				case OperatorNode::OP_EQUAL: {
					_REDUCE_BINARY(Variant::OP_EQUAL);
				} break;
				case OperatorNode::OP_NOT_EQUAL: {
					_REDUCE_BINARY(Variant::OP_NOT_EQUAL);
				} break;
				case OperatorNode::OP_LESS: {
					_REDUCE_BINARY(Variant::OP_LESS);
				} break;
				case OperatorNode::OP_LESS_EQUAL: {
					_REDUCE_BINARY(Variant::OP_LESS_EQUAL);
				} break;
				case OperatorNode::OP_GREATER: {
					_REDUCE_BINARY(Variant::OP_GREATER);
				} break;
				case OperatorNode::OP_GREATER_EQUAL: {
					_REDUCE_BINARY(Variant::OP_GREATER_EQUAL);
				} break;
				case OperatorNode::OP_AND: {
					_REDUCE_BINARY(Variant::OP_AND);
				} break;
				case OperatorNode::OP_OR: {
					_REDUCE_BINARY(Variant::OP_OR);
				} break;
				case OperatorNode::OP_ADD: {
					_REDUCE_BINARY(Variant::OP_ADD);
				} break;
				case OperatorNode::OP_SUB: {
					_REDUCE_BINARY(Variant::OP_SUBTRACT);
				} break;
				case OperatorNode::OP_MUL: {
					_REDUCE_BINARY(Variant::OP_MULTIPLY);
				} break;
				case OperatorNode::OP_DIV: {
					_REDUCE_BINARY(Variant::OP_DIVIDE);
				} break;
				case OperatorNode::OP_MOD: {
					_REDUCE_BINARY(Variant::OP_MODULE);
				} break;
				case OperatorNode::OP_SHIFT_LEFT: {
					_REDUCE_BINARY(Variant::OP_SHIFT_LEFT);
				} break;
				case OperatorNode::OP_SHIFT_RIGHT: {
					_REDUCE_BINARY(Variant::OP_SHIFT_RIGHT);
				} break;
				case OperatorNode::OP_BIT_AND: {
					_REDUCE_BINARY(Variant::OP_BIT_AND);
				} break;
				case OperatorNode::OP_BIT_OR: {
					_REDUCE_BINARY(Variant::OP_BIT_OR);
				} break;
				case OperatorNode::OP_BIT_XOR: {
					_REDUCE_BINARY(Variant::OP_BIT_XOR);
				} break;
				case OperatorNode::OP_TERNARY_IF: {
					if (static_cast<ConstantNode *>(op->arguments[0])->value.booleanize()) {
						return op->arguments[1];
					} else {
						return op->arguments[2];
					}
				} break;
				default: {
					ERR_FAIL_V(op);
				}
			}

		} break;
		default: {
			return p_node;
		} break;
	}
}

GDScriptParser::Node *GDScriptParser::_parse_and_reduce_expression(Node *p_parent, bool p_static, bool p_reduce_const, bool p_allow_assign) {
	Node *expr = _parse_expression(p_parent, p_static, p_allow_assign, p_reduce_const);
	if (!expr || error_set) {
		return nullptr;
	}
	expr = _reduce_expression(expr, p_reduce_const);
	if (!expr || error_set) {
		return nullptr;
	}
	return expr;
}

bool GDScriptParser::_reduce_export_var_type(Variant &p_value, int p_line) {
	if (p_value.get_type() == Variant::ARRAY) {
		Array arr = p_value;
		for (int i = 0; i < arr.size(); i++) {
			if (!_reduce_export_var_type(arr[i], p_line)) {
				return false;
			}
		}
		return true;
	}

	if (p_value.get_type() == Variant::DICTIONARY) {
		Dictionary dict = p_value;
		for (int i = 0; i < dict.size(); i++) {
			Variant value = dict.get_value_at_index(i);
			if (!_reduce_export_var_type(value, p_line)) {
				return false;
			}
		}
		return true;
	}

	// validate type
	DataType type = _type_from_variant(p_value);
	if (type.kind == DataType::BUILTIN) {
		return true;
	} else if (type.kind == DataType::NATIVE) {
		if (ClassDB::is_parent_class(type.native_type, "Resource")) {
			return true;
		}
	}
	_set_error("Invalid export type. Only built-in and native resource types can be exported.", p_line);
	return false;
}

const Variant *GDScriptParser::_try_to_find_constant_value_for_expression(const Node *p_expr) const {
	if (p_expr->type == Node::TYPE_CONSTANT) {
		return &(static_cast<const ConstantNode *>(p_expr)->value);
	} else if (p_expr->type == Node::TYPE_IDENTIFIER) {
		const StringName &name = static_cast<const IdentifierNode *>(p_expr)->name;
		const Map<StringName, ClassNode::Constant>::Element *element =
				current_class->constant_expressions.find(name);
		if (element) {
			Node *cn_exp = element->value().expression;
			if (cn_exp->type == Node::TYPE_CONSTANT) {
				return &(static_cast<ConstantNode *>(cn_exp)->value);
			}
		}
	} else if (p_expr->type == Node::TYPE_OPERATOR) {
		// Check if expression `p_expr` is a named enum (e.g. `State.IDLE`).
		const OperatorNode *op_node = static_cast<const OperatorNode *>(p_expr);
		if (op_node->op == GDScriptParser::OperatorNode::OP_INDEX_NAMED) {
			const Vector<Node *> &op_args = op_node->arguments;
			if (op_args.size() < 2) {
				return nullptr; // Invalid expression.
			}

			if (op_args[0]->type != Node::TYPE_IDENTIFIER || op_args[1]->type != Node::TYPE_IDENTIFIER) {
				return nullptr; // Not an enum expression.
			}

			const StringName &enum_name = static_cast<const IdentifierNode *>(op_args[0])->name;
			const StringName &const_name = static_cast<const IdentifierNode *>(op_args[1])->name;
			Map<StringName, ClassNode::Constant>::Element *element =
					current_class->constant_expressions.find(enum_name);
			if (element) {
				Node *cn_exp = element->value().expression;
				if (cn_exp->type == Node::TYPE_CONSTANT) {
					const Dictionary &enum_dict = static_cast<ConstantNode *>(cn_exp)->value;
					return enum_dict.getptr(const_name);
				}
			}
		}
	}

	return nullptr;
}

bool GDScriptParser::_recover_from_completion() {
	if (!completion_found) {
		return false; //can't recover if no completion
	}
	//skip stuff until newline
	while (tokenizer->get_token() != GDScriptTokenizer::TK_NEWLINE && tokenizer->get_token() != GDScriptTokenizer::TK_EOF && tokenizer->get_token() != GDScriptTokenizer::TK_ERROR) {
		tokenizer->advance();
	}
	completion_found = false;
	error_set = false;
	if (tokenizer->get_token() == GDScriptTokenizer::TK_ERROR) {
		error_set = true;
	}

	return true;
}

GDScriptParser::PatternNode *GDScriptParser::_parse_pattern(bool p_static) {
	PatternNode *pattern = alloc_node<PatternNode>();

	GDScriptTokenizer::Token token = tokenizer->get_token();
	if (error_set) {
		return nullptr;
	}

	if (token == GDScriptTokenizer::TK_EOF) {
		return nullptr;
	}

	switch (token) {
		// array
		case GDScriptTokenizer::TK_BRACKET_OPEN: {
			tokenizer->advance();
			pattern->pt_type = GDScriptParser::PatternNode::PT_ARRAY;
			while (true) {
				if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_CLOSE) {
					tokenizer->advance();
					break;
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD && tokenizer->get_token(1) == GDScriptTokenizer::TK_PERIOD) {
					// match everything
					tokenizer->advance(2);
					PatternNode *sub_pattern = alloc_node<PatternNode>();
					sub_pattern->pt_type = GDScriptParser::PatternNode::PT_IGNORE_REST;
					pattern->array.push_back(sub_pattern);
					if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA && tokenizer->get_token(1) == GDScriptTokenizer::TK_BRACKET_CLOSE) {
						tokenizer->advance(2);
						break;
					} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_CLOSE) {
						tokenizer->advance(1);
						break;
					} else {
						_set_error("'..' pattern only allowed at the end of an array pattern");
						return nullptr;
					}
				}

				PatternNode *sub_pattern = _parse_pattern(p_static);
				if (!sub_pattern) {
					return nullptr;
				}

				pattern->array.push_back(sub_pattern);

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
					tokenizer->advance();
					continue;
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_BRACKET_CLOSE) {
					tokenizer->advance();
					break;
				} else {
					_set_error("Not a valid pattern");
					return nullptr;
				}
			}
		} break;
		// bind
		case GDScriptTokenizer::TK_PR_VAR: {
			tokenizer->advance();
			if (!tokenizer->is_token_literal()) {
				_set_error("Expected identifier for binding variable name.");
				return nullptr;
			}
			pattern->pt_type = GDScriptParser::PatternNode::PT_BIND;
			pattern->bind = tokenizer->get_token_literal();
			// Check if variable name is already used
			BlockNode *bl = current_block;
			while (bl) {
				if (bl->variables.has(pattern->bind)) {
					_set_error("Binding name of '" + pattern->bind.operator String() + "' is already declared in this scope.");
					return nullptr;
				}
				bl = bl->parent_block;
			}
			// Create local variable for proper identifier detection later
			LocalVarNode *lv = alloc_node<LocalVarNode>();
			lv->name = pattern->bind;
			current_block->variables.insert(lv->name, lv);
			tokenizer->advance();
		} break;
		// dictionary
		case GDScriptTokenizer::TK_CURLY_BRACKET_OPEN: {
			tokenizer->advance();
			pattern->pt_type = GDScriptParser::PatternNode::PT_DICTIONARY;
			while (true) {
				if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {
					tokenizer->advance();
					break;
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD && tokenizer->get_token(1) == GDScriptTokenizer::TK_PERIOD) {
					// match everything
					tokenizer->advance(2);
					PatternNode *sub_pattern = alloc_node<PatternNode>();
					sub_pattern->pt_type = PatternNode::PT_IGNORE_REST;
					pattern->array.push_back(sub_pattern);
					if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA && tokenizer->get_token(1) == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {
						tokenizer->advance(2);
						break;
					} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {
						tokenizer->advance(1);
						break;
					} else {
						_set_error("'..' pattern only allowed at the end of a dictionary pattern");
						return nullptr;
					}
				}

				Node *key = _parse_and_reduce_expression(pattern, p_static);
				if (!key) {
					_set_error("Not a valid key in pattern");
					return nullptr;
				}

				if (key->type != GDScriptParser::Node::TYPE_CONSTANT) {
					_set_error("Not a constant expression as key");
					return nullptr;
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
					tokenizer->advance();

					PatternNode *value = _parse_pattern(p_static);
					if (!value) {
						_set_error("Expected pattern in dictionary value");
						return nullptr;
					}

					pattern->dictionary.insert(static_cast<ConstantNode *>(key), value);
				} else {
					pattern->dictionary.insert(static_cast<ConstantNode *>(key), NULL);
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
					tokenizer->advance();
					continue;
				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {
					tokenizer->advance();
					break;
				} else {
					_set_error("Not a valid pattern");
					return nullptr;
				}
			}
		} break;
		case GDScriptTokenizer::TK_WILDCARD: {
			tokenizer->advance();
			pattern->pt_type = PatternNode::PT_WILDCARD;
		} break;
		// all the constants like strings and numbers
		default: {
			Node *value = _parse_and_reduce_expression(pattern, p_static);
			if (!value) {
				_set_error("Expect constant expression or variables in a pattern");
				return nullptr;
			}

			if (value->type == Node::TYPE_OPERATOR) {
				// Maybe it's SomeEnum.VALUE
				Node *current_value = value;

				while (current_value->type == Node::TYPE_OPERATOR) {
					OperatorNode *op_node = static_cast<OperatorNode *>(current_value);

					if (op_node->op != OperatorNode::OP_INDEX_NAMED) {
						_set_error("Invalid operator in pattern. Only index (`A.B`) is allowed");
						return nullptr;
					}
					current_value = op_node->arguments[0];
				}

				if (current_value->type != Node::TYPE_IDENTIFIER) {
					_set_error("Only constant expression or variables allowed in a pattern");
					return nullptr;
				}

			} else if (value->type != Node::TYPE_IDENTIFIER && value->type != Node::TYPE_CONSTANT) {
				_set_error("Only constant expressions or variables allowed in a pattern");
				return nullptr;
			}

			pattern->pt_type = PatternNode::PT_CONSTANT;
			pattern->constant = value;
		} break;
	}

	return pattern;
}

void GDScriptParser::_parse_pattern_block(BlockNode *p_block, Vector<PatternBranchNode *> &p_branches, bool p_static) {
	IndentLevel current_level = indent_level.back()->get();

	p_block->has_return = true;

	bool catch_all_appeared = false;

	while (true) {
		while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE && _parse_newline()) {
			;
		}

		// GDScriptTokenizer::Token token = tokenizer->get_token();
		if (error_set) {
			return;
		}

		if (current_level.indent > indent_level.back()->get().indent) {
			break; // go back a level
		}

		pending_newline = -1;

		PatternBranchNode *branch = alloc_node<PatternBranchNode>();
		branch->body = alloc_node<BlockNode>();
		branch->body->parent_block = p_block;
		p_block->sub_blocks.push_back(branch->body);
		current_block = branch->body;

		branch->patterns.push_back(_parse_pattern(p_static));
		if (!branch->patterns[0]) {
			break;
		}

		bool has_binding = branch->patterns[0]->pt_type == PatternNode::PT_BIND;
		bool catch_all = has_binding || branch->patterns[0]->pt_type == PatternNode::PT_WILDCARD;

#ifdef DEBUG_ENABLED
		// Branches after a wildcard or binding are unreachable
		if (catch_all_appeared && !current_function->has_unreachable_code) {
			_add_warning(GDScriptWarning::UNREACHABLE_CODE, -1, current_function->name.operator String());
			current_function->has_unreachable_code = true;
		}
#endif

		while (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
			tokenizer->advance();
			branch->patterns.push_back(_parse_pattern(p_static));
			if (!branch->patterns[branch->patterns.size() - 1]) {
				return;
			}

			PatternNode::PatternType pt = branch->patterns[branch->patterns.size() - 1]->pt_type;

			if (pt == PatternNode::PT_BIND) {
				_set_error("Cannot use bindings with multipattern.");
				return;
			}

			catch_all = catch_all || pt == PatternNode::PT_WILDCARD;
		}

		catch_all_appeared = catch_all_appeared || catch_all;

		if (!_enter_indent_block()) {
			_set_error("Expected block in pattern branch");
			return;
		}

		_parse_block(branch->body, p_static);

		current_block = p_block;

		if (!branch->body->has_return) {
			p_block->has_return = false;
		}

		p_branches.push_back(branch);
	}

	// Even if all branches return, there is possibility of default fallthrough
	if (!catch_all_appeared) {
		p_block->has_return = false;
	}
}

void GDScriptParser::_generate_pattern(PatternNode *p_pattern, Node *p_node_to_match, Node *&p_resulting_node, Map<StringName, Node *> &p_bindings) {
	const DataType &to_match_type = p_node_to_match->get_datatype();

	switch (p_pattern->pt_type) {
		case PatternNode::PT_CONSTANT: {
			DataType pattern_type = _reduce_node_type(p_pattern->constant);
			if (error_set) {
				return;
			}

			OperatorNode *type_comp = nullptr;

			// static type check if possible
			if (pattern_type.has_type && to_match_type.has_type) {
				if (!_is_type_compatible(to_match_type, pattern_type) && !_is_type_compatible(pattern_type, to_match_type)) {
					_set_error("The pattern type (" + pattern_type.to_string() + ") isn't compatible with the type of the value to match (" + to_match_type.to_string() + ").",
							p_pattern->line);
					return;
				}
			} else {
				// runtime typecheck
				BuiltInFunctionNode *typeof_node = alloc_node<BuiltInFunctionNode>();
				typeof_node->function = GDScriptFunctions::TYPE_OF;

				OperatorNode *typeof_match_value = alloc_node<OperatorNode>();
				typeof_match_value->op = OperatorNode::OP_CALL;
				typeof_match_value->arguments.push_back(typeof_node);
				typeof_match_value->arguments.push_back(p_node_to_match);

				OperatorNode *typeof_pattern_value = alloc_node<OperatorNode>();
				typeof_pattern_value->op = OperatorNode::OP_CALL;
				typeof_pattern_value->arguments.push_back(typeof_node);
				typeof_pattern_value->arguments.push_back(p_pattern->constant);

				type_comp = alloc_node<OperatorNode>();
				type_comp->op = OperatorNode::OP_EQUAL;
				type_comp->arguments.push_back(typeof_match_value);
				type_comp->arguments.push_back(typeof_pattern_value);
			}

			// compare the actual values
			OperatorNode *value_comp = alloc_node<OperatorNode>();
			value_comp->op = OperatorNode::OP_EQUAL;
			value_comp->arguments.push_back(p_pattern->constant);
			value_comp->arguments.push_back(p_node_to_match);

			if (type_comp) {
				OperatorNode *full_comparison = alloc_node<OperatorNode>();
				full_comparison->op = OperatorNode::OP_AND;
				full_comparison->arguments.push_back(type_comp);
				full_comparison->arguments.push_back(value_comp);

				p_resulting_node = full_comparison;
			} else {
				p_resulting_node = value_comp;
			}

		} break;
		case PatternNode::PT_BIND: {
			p_bindings[p_pattern->bind] = p_node_to_match;

			// a bind always matches
			ConstantNode *true_value = alloc_node<ConstantNode>();
			true_value->value = Variant(true);
			p_resulting_node = true_value;
		} break;
		case PatternNode::PT_ARRAY: {
			bool open_ended = false;

			if (p_pattern->array.size() > 0) {
				if (p_pattern->array[p_pattern->array.size() - 1]->pt_type == PatternNode::PT_IGNORE_REST) {
					open_ended = true;
				}
			}

			// typeof(value_to_match) == TYPE_ARRAY && value_to_match.size() >= length
			// typeof(value_to_match) == TYPE_ARRAY && value_to_match.size() == length

			{
				OperatorNode *type_comp = nullptr;
				// static type check if possible
				if (to_match_type.has_type) {
					// must be an array
					if (to_match_type.kind != DataType::BUILTIN || to_match_type.builtin_type != Variant::ARRAY) {
						_set_error("Cannot match an array pattern with a non-array expression.", p_pattern->line);
						return;
					}
				} else {
					// runtime typecheck
					BuiltInFunctionNode *typeof_node = alloc_node<BuiltInFunctionNode>();
					typeof_node->function = GDScriptFunctions::TYPE_OF;

					OperatorNode *typeof_match_value = alloc_node<OperatorNode>();
					typeof_match_value->op = OperatorNode::OP_CALL;
					typeof_match_value->arguments.push_back(typeof_node);
					typeof_match_value->arguments.push_back(p_node_to_match);

					IdentifierNode *typeof_array = alloc_node<IdentifierNode>();
					typeof_array->name = "TYPE_ARRAY";

					type_comp = alloc_node<OperatorNode>();
					type_comp->op = OperatorNode::OP_EQUAL;
					type_comp->arguments.push_back(typeof_match_value);
					type_comp->arguments.push_back(typeof_array);
				}

				// size
				ConstantNode *length = alloc_node<ConstantNode>();
				length->value = Variant(open_ended ? p_pattern->array.size() - 1 : p_pattern->array.size());

				OperatorNode *call = alloc_node<OperatorNode>();
				call->op = OperatorNode::OP_CALL;
				call->arguments.push_back(p_node_to_match);

				IdentifierNode *size = alloc_node<IdentifierNode>();
				size->name = "size";
				call->arguments.push_back(size);

				OperatorNode *length_comparison = alloc_node<OperatorNode>();
				length_comparison->op = open_ended ? OperatorNode::OP_GREATER_EQUAL : OperatorNode::OP_EQUAL;
				length_comparison->arguments.push_back(call);
				length_comparison->arguments.push_back(length);

				if (type_comp) {
					OperatorNode *type_and_length_comparison = alloc_node<OperatorNode>();
					type_and_length_comparison->op = OperatorNode::OP_AND;
					type_and_length_comparison->arguments.push_back(type_comp);
					type_and_length_comparison->arguments.push_back(length_comparison);

					p_resulting_node = type_and_length_comparison;
				} else {
					p_resulting_node = length_comparison;
				}
			}

			for (int i = 0; i < p_pattern->array.size(); i++) {
				PatternNode *pattern = p_pattern->array[i];

				Node *condition = nullptr;

				ConstantNode *index = alloc_node<ConstantNode>();
				index->value = Variant(i);

				OperatorNode *indexed_value = alloc_node<OperatorNode>();
				indexed_value->op = OperatorNode::OP_INDEX;
				indexed_value->arguments.push_back(p_node_to_match);
				indexed_value->arguments.push_back(index);

				_generate_pattern(pattern, indexed_value, condition, p_bindings);

				// concatenate all the patterns with &&
				OperatorNode *and_node = alloc_node<OperatorNode>();
				and_node->op = OperatorNode::OP_AND;
				and_node->arguments.push_back(p_resulting_node);
				and_node->arguments.push_back(condition);

				p_resulting_node = and_node;
			}

		} break;
		case PatternNode::PT_DICTIONARY: {
			bool open_ended = false;

			if (p_pattern->array.size() > 0) {
				open_ended = true;
			}

			// typeof(value_to_match) == TYPE_DICTIONARY && value_to_match.size() >= length
			// typeof(value_to_match) == TYPE_DICTIONARY && value_to_match.size() == length

			{
				OperatorNode *type_comp = nullptr;
				// static type check if possible
				if (to_match_type.has_type) {
					// must be an dictionary
					if (to_match_type.kind != DataType::BUILTIN || to_match_type.builtin_type != Variant::DICTIONARY) {
						_set_error("Cannot match an dictionary pattern with a non-dictionary expression.", p_pattern->line);
						return;
					}
				} else {
					// runtime typecheck
					BuiltInFunctionNode *typeof_node = alloc_node<BuiltInFunctionNode>();
					typeof_node->function = GDScriptFunctions::TYPE_OF;

					OperatorNode *typeof_match_value = alloc_node<OperatorNode>();
					typeof_match_value->op = OperatorNode::OP_CALL;
					typeof_match_value->arguments.push_back(typeof_node);
					typeof_match_value->arguments.push_back(p_node_to_match);

					IdentifierNode *typeof_dictionary = alloc_node<IdentifierNode>();
					typeof_dictionary->name = "TYPE_DICTIONARY";

					type_comp = alloc_node<OperatorNode>();
					type_comp->op = OperatorNode::OP_EQUAL;
					type_comp->arguments.push_back(typeof_match_value);
					type_comp->arguments.push_back(typeof_dictionary);
				}

				// size
				ConstantNode *length = alloc_node<ConstantNode>();
				length->value = Variant(open_ended ? p_pattern->dictionary.size() - 1 : p_pattern->dictionary.size());

				OperatorNode *call = alloc_node<OperatorNode>();
				call->op = OperatorNode::OP_CALL;
				call->arguments.push_back(p_node_to_match);

				IdentifierNode *size = alloc_node<IdentifierNode>();
				size->name = "size";
				call->arguments.push_back(size);

				OperatorNode *length_comparison = alloc_node<OperatorNode>();
				length_comparison->op = open_ended ? OperatorNode::OP_GREATER_EQUAL : OperatorNode::OP_EQUAL;
				length_comparison->arguments.push_back(call);
				length_comparison->arguments.push_back(length);

				if (type_comp) {
					OperatorNode *type_and_length_comparison = alloc_node<OperatorNode>();
					type_and_length_comparison->op = OperatorNode::OP_AND;
					type_and_length_comparison->arguments.push_back(type_comp);
					type_and_length_comparison->arguments.push_back(length_comparison);

					p_resulting_node = type_and_length_comparison;
				} else {
					p_resulting_node = length_comparison;
				}
			}

			for (Map<ConstantNode *, PatternNode *>::Element *e = p_pattern->dictionary.front(); e; e = e->next()) {
				Node *condition = nullptr;

				// check for has, then for pattern

				IdentifierNode *has = alloc_node<IdentifierNode>();
				has->name = "has";

				OperatorNode *has_call = alloc_node<OperatorNode>();
				has_call->op = OperatorNode::OP_CALL;
				has_call->arguments.push_back(p_node_to_match);
				has_call->arguments.push_back(has);
				has_call->arguments.push_back(e->key());

				if (e->value()) {
					OperatorNode *indexed_value = alloc_node<OperatorNode>();
					indexed_value->op = OperatorNode::OP_INDEX;
					indexed_value->arguments.push_back(p_node_to_match);
					indexed_value->arguments.push_back(e->key());

					_generate_pattern(e->value(), indexed_value, condition, p_bindings);

					OperatorNode *has_and_pattern = alloc_node<OperatorNode>();
					has_and_pattern->op = OperatorNode::OP_AND;
					has_and_pattern->arguments.push_back(has_call);
					has_and_pattern->arguments.push_back(condition);

					condition = has_and_pattern;

				} else {
					condition = has_call;
				}

				// concatenate all the patterns with &&
				OperatorNode *and_node = alloc_node<OperatorNode>();
				and_node->op = OperatorNode::OP_AND;
				and_node->arguments.push_back(p_resulting_node);
				and_node->arguments.push_back(condition);

				p_resulting_node = and_node;
			}

		} break;
		case PatternNode::PT_IGNORE_REST:
		case PatternNode::PT_WILDCARD: {
			// simply generate a `true`
			ConstantNode *true_value = alloc_node<ConstantNode>();
			true_value->value = Variant(true);
			p_resulting_node = true_value;
		} break;
		default: {
		} break;
	}
}

void GDScriptParser::_transform_match_statment(MatchNode *p_match_statement) {
	IdentifierNode *id = alloc_node<IdentifierNode>();
	id->name = "#match_value";
	id->line = p_match_statement->line;
	id->datatype = _reduce_node_type(p_match_statement->val_to_match);
	if (id->datatype.has_type) {
		_mark_line_as_safe(id->line);
	} else {
		_mark_line_as_unsafe(id->line);
	}

	if (error_set) {
		return;
	}

	for (int i = 0; i < p_match_statement->branches.size(); i++) {
		PatternBranchNode *branch = p_match_statement->branches[i];

		MatchNode::CompiledPatternBranch compiled_branch;
		compiled_branch.compiled_pattern = nullptr;

		Map<StringName, Node *> binding;

		for (int j = 0; j < branch->patterns.size(); j++) {
			PatternNode *pattern = branch->patterns[j];
			_mark_line_as_safe(pattern->line);

			Map<StringName, Node *> bindings;
			Node *resulting_node = nullptr;
			_generate_pattern(pattern, id, resulting_node, bindings);

			if (!resulting_node) {
				return;
			}

			if (!binding.empty() && !bindings.empty()) {
				_set_error("Multipatterns can't contain bindings");
				return;
			} else {
				binding = bindings;
			}

			// Result is always a boolean
			DataType resulting_node_type;
			resulting_node_type.has_type = true;
			resulting_node_type.is_constant = true;
			resulting_node_type.kind = DataType::BUILTIN;
			resulting_node_type.builtin_type = Variant::BOOL;
			resulting_node->set_datatype(resulting_node_type);

			if (compiled_branch.compiled_pattern) {
				OperatorNode *or_node = alloc_node<OperatorNode>();
				or_node->op = OperatorNode::OP_OR;
				or_node->arguments.push_back(compiled_branch.compiled_pattern);
				or_node->arguments.push_back(resulting_node);

				compiled_branch.compiled_pattern = or_node;
			} else {
				// single pattern | first one
				compiled_branch.compiled_pattern = resulting_node;
			}
		}

		// prepare the body ...hehe
		for (Map<StringName, Node *>::Element *e = binding.front(); e; e = e->next()) {
			if (!branch->body->variables.has(e->key())) {
				_set_error("Parser bug: missing pattern bind variable.", branch->line);
				ERR_FAIL();
			}

			LocalVarNode *local_var = branch->body->variables[e->key()];
			local_var->assign = e->value();
			local_var->set_datatype(local_var->assign->get_datatype());
			local_var->assignments++;

			IdentifierNode *id2 = alloc_node<IdentifierNode>();
			id2->name = local_var->name;
			id2->datatype = local_var->datatype;
			id2->declared_block = branch->body;
			id2->set_datatype(local_var->assign->get_datatype());

			OperatorNode *op = alloc_node<OperatorNode>();
			op->op = OperatorNode::OP_ASSIGN;
			op->arguments.push_back(id2);
			op->arguments.push_back(local_var->assign);
			local_var->assign_op = op;

			branch->body->statements.insert(0, op);
			branch->body->statements.insert(0, local_var);
		}

		compiled_branch.body = branch->body;

		p_match_statement->compiled_pattern_branches.push_back(compiled_branch);
	}
}

void GDScriptParser::_parse_block(BlockNode *p_block, bool p_static) {
	IndentLevel current_level = indent_level.back()->get();

#ifdef DEBUG_ENABLED

	pending_newline = -1; // reset for the new block

	NewLineNode *nl = alloc_node<NewLineNode>();

	nl->line = tokenizer->get_token_line();
	p_block->statements.push_back(nl);
#endif

	bool is_first_line = true;

	while (true) {
		if (!is_first_line && indent_level.back()->prev() && indent_level.back()->prev()->get().indent == current_level.indent) {
			if (indent_level.back()->prev()->get().is_mixed(current_level)) {
				_set_error("Mixed tabs and spaces in indentation.");
				return;
			}
			// pythonic single-line expression, don't parse future lines
			indent_level.pop_back();
			p_block->end_line = tokenizer->get_token_line();
			return;
		}
		is_first_line = false;

		GDScriptTokenizer::Token token = tokenizer->get_token();
		if (error_set) {
			return;
		}

		if (current_level.indent > indent_level.back()->get().indent) {
			p_block->end_line = tokenizer->get_token_line();
			return; //go back a level
		}

		if (pending_newline != -1) {
			NewLineNode *nl2 = alloc_node<NewLineNode>();
			nl2->line = pending_newline;
			p_block->statements.push_back(nl2);
			pending_newline = -1;
		}

#ifdef DEBUG_ENABLED
		switch (token) {
			case GDScriptTokenizer::TK_EOF:
			case GDScriptTokenizer::TK_ERROR:
			case GDScriptTokenizer::TK_NEWLINE:
			case GDScriptTokenizer::TK_CF_PASS: {
				// will check later
			} break;
			default: {
				if (p_block->has_return && !current_function->has_unreachable_code) {
					_add_warning(GDScriptWarning::UNREACHABLE_CODE, -1, current_function->name.operator String());
					current_function->has_unreachable_code = true;
				}
			} break;
		}
#endif // DEBUG_ENABLED
		switch (token) {
			case GDScriptTokenizer::TK_EOF:
				p_block->end_line = tokenizer->get_token_line();
			case GDScriptTokenizer::TK_ERROR: {
				return; //go back

				//end of file!

			} break;
			case GDScriptTokenizer::TK_NEWLINE: {
				int line = tokenizer->get_token_line();

				if (!_parse_newline()) {
					if (!error_set) {
						p_block->end_line = tokenizer->get_token_line();
						pending_newline = p_block->end_line;
					}
					return;
				}

				_mark_line_as_safe(line);
				NewLineNode *nl2 = alloc_node<NewLineNode>();
				nl2->line = line;
				p_block->statements.push_back(nl2);

			} break;
			case GDScriptTokenizer::TK_CF_PASS: {
				if (tokenizer->get_token(1) != GDScriptTokenizer::TK_SEMICOLON && tokenizer->get_token(1) != GDScriptTokenizer::TK_NEWLINE && tokenizer->get_token(1) != GDScriptTokenizer::TK_EOF) {
					_set_error("Expected \";\" or a line break.");
					return;
				}
				_mark_line_as_safe(tokenizer->get_token_line());
				tokenizer->advance();
				if (tokenizer->get_token() == GDScriptTokenizer::TK_SEMICOLON) {
					// Ignore semicolon after 'pass'.
					tokenizer->advance();
				}
			} break;
			case GDScriptTokenizer::TK_PR_VAR: {
				// Variable declaration and (eventual) initialization.

				tokenizer->advance();
				int var_line = tokenizer->get_token_line();
				if (!tokenizer->is_token_literal(0, true)) {
					_set_error("Expected an identifier for the local variable name.");
					return;
				}
				StringName n = tokenizer->get_token_literal();
				if (current_function) {
					for (int i = 0; i < current_function->arguments.size(); i++) {
						if (n == current_function->arguments[i]) {
							_set_error("Variable \"" + String(n) + "\" already defined in the scope (at line " + itos(current_function->line) + ").");
							return;
						}
					}
				}
				BlockNode *check_block = p_block;
				while (check_block) {
					if (check_block->variables.has(n)) {
						_set_error("Variable \"" + String(n) + "\" already defined in the scope (at line " + itos(check_block->variables[n]->line) + ").");
						return;
					}
					check_block = check_block->parent_block;
				}
				tokenizer->advance();

				//must know when the local variable is declared
				LocalVarNode *lv = alloc_node<LocalVarNode>();
				lv->name = n;
				lv->line = var_line;
				p_block->statements.push_back(lv);

				Node *assigned = nullptr;

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
					if (tokenizer->get_token(1) == GDScriptTokenizer::TK_OP_ASSIGN) {
						lv->datatype = DataType();
						lv->datatype.infer_type = true;
						tokenizer->advance();
					} else if (!_parse_type(lv->datatype)) {
						_set_error("Expected a type for the variable.");
						return;
					}
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_ASSIGN) {
					tokenizer->advance();
					Node *subexpr = _parse_and_reduce_expression(p_block, p_static);
					if (!subexpr) {
						if (_recover_from_completion()) {
							break;
						}
						return;
					}

					lv->assignments++;
					assigned = subexpr;
				} else {
					assigned = _get_default_value_for_type(lv->datatype, var_line);
				}
				//must be added later, to avoid self-referencing.
				p_block->variables.insert(n, lv);

				IdentifierNode *id = alloc_node<IdentifierNode>();
				id->name = n;
				id->declared_block = p_block;
				id->line = var_line;

				OperatorNode *op = alloc_node<OperatorNode>();
				op->op = OperatorNode::OP_ASSIGN;
				op->arguments.push_back(id);
				op->arguments.push_back(assigned);
				op->line = var_line;
				p_block->statements.push_back(op);
				lv->assign_op = op;
				lv->assign = assigned;

				if (!_end_statement()) {
					_set_end_statement_error("var");
					return;
				}

			} break;
			case GDScriptTokenizer::TK_CF_IF: {
				tokenizer->advance();

				Node *condition = _parse_and_reduce_expression(p_block, p_static);
				if (!condition) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}

				ControlFlowNode *cf_if = alloc_node<ControlFlowNode>();

				cf_if->cf_type = ControlFlowNode::CF_IF;
				cf_if->arguments.push_back(condition);

				cf_if->body = alloc_node<BlockNode>();
				cf_if->body->parent_block = p_block;
				cf_if->body->if_condition = condition; //helps code completion

				p_block->sub_blocks.push_back(cf_if->body);

				if (!_enter_indent_block(cf_if->body)) {
					_set_error("Expected an indented block after \"if\".");
					p_block->end_line = tokenizer->get_token_line();
					return;
				}

				current_block = cf_if->body;
				_parse_block(cf_if->body, p_static);
				current_block = p_block;

				if (error_set) {
					return;
				}
				p_block->statements.push_back(cf_if);

				bool all_have_return = cf_if->body->has_return;
				bool have_else = false;

				while (true) {
					while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE && _parse_newline()) {
						;
					}

					if (indent_level.back()->get().indent < current_level.indent) { //not at current indent level
						p_block->end_line = tokenizer->get_token_line();
						return;
					}

					if (tokenizer->get_token() == GDScriptTokenizer::TK_CF_ELIF) {
						if (indent_level.back()->get().indent > current_level.indent) {
							_set_error("Invalid indentation.");
							return;
						}

						tokenizer->advance();

						cf_if->body_else = alloc_node<BlockNode>();
						cf_if->body_else->parent_block = p_block;
						p_block->sub_blocks.push_back(cf_if->body_else);

						ControlFlowNode *cf_else = alloc_node<ControlFlowNode>();
						cf_else->cf_type = ControlFlowNode::CF_IF;

						//condition
						Node *condition2 = _parse_and_reduce_expression(p_block, p_static);
						if (!condition2) {
							if (_recover_from_completion()) {
								break;
							}
							return;
						}
						cf_else->arguments.push_back(condition2);
						cf_else->cf_type = ControlFlowNode::CF_IF;

						cf_if->body_else->statements.push_back(cf_else);
						cf_if = cf_else;
						cf_if->body = alloc_node<BlockNode>();
						cf_if->body->parent_block = p_block;
						p_block->sub_blocks.push_back(cf_if->body);

						if (!_enter_indent_block(cf_if->body)) {
							_set_error("Expected an indented block after \"elif\".");
							p_block->end_line = tokenizer->get_token_line();
							return;
						}

						current_block = cf_else->body;
						_parse_block(cf_else->body, p_static);
						current_block = p_block;
						if (error_set) {
							return;
						}

						all_have_return = all_have_return && cf_else->body->has_return;

					} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CF_ELSE) {
						if (indent_level.back()->get().indent > current_level.indent) {
							_set_error("Invalid indentation.");
							return;
						}

						tokenizer->advance();
						cf_if->body_else = alloc_node<BlockNode>();
						cf_if->body_else->parent_block = p_block;
						p_block->sub_blocks.push_back(cf_if->body_else);

						if (!_enter_indent_block(cf_if->body_else)) {
							_set_error("Expected an indented block after \"else\".");
							p_block->end_line = tokenizer->get_token_line();
							return;
						}
						current_block = cf_if->body_else;
						_parse_block(cf_if->body_else, p_static);
						current_block = p_block;
						if (error_set) {
							return;
						}

						all_have_return = all_have_return && cf_if->body_else->has_return;
						have_else = true;

						break; //after else, exit

					} else {
						break;
					}
				}

				cf_if->body->has_return = all_have_return;
				// If there's no else block, path out of the if might not have a return
				p_block->has_return = all_have_return && have_else;

			} break;
			case GDScriptTokenizer::TK_CF_WHILE: {
				tokenizer->advance();
				Node *condition2 = _parse_and_reduce_expression(p_block, p_static);
				if (!condition2) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}

				ControlFlowNode *cf_while = alloc_node<ControlFlowNode>();

				cf_while->cf_type = ControlFlowNode::CF_WHILE;
				cf_while->arguments.push_back(condition2);

				cf_while->body = alloc_node<BlockNode>();
				cf_while->body->parent_block = p_block;
				cf_while->body->can_break = true;
				cf_while->body->can_continue = true;
				p_block->sub_blocks.push_back(cf_while->body);

				if (!_enter_indent_block(cf_while->body)) {
					_set_error("Expected an indented block after \"while\".");
					p_block->end_line = tokenizer->get_token_line();
					return;
				}

				current_block = cf_while->body;
				_parse_block(cf_while->body, p_static);
				current_block = p_block;
				if (error_set) {
					return;
				}
				p_block->statements.push_back(cf_while);
			} break;
			case GDScriptTokenizer::TK_CF_FOR: {
				tokenizer->advance();

				if (!tokenizer->is_token_literal(0, true)) {
					_set_error("Identifier expected after \"for\".");
				}

				IdentifierNode *id = alloc_node<IdentifierNode>();
				id->name = tokenizer->get_token_identifier();
#ifdef DEBUG_ENABLED
				for (int j = 0; j < current_class->variables.size(); j++) {
					if (current_class->variables[j].identifier == id->name) {
						_add_warning(GDScriptWarning::SHADOWED_VARIABLE, id->line, id->name, itos(current_class->variables[j].line));
					}
				}
#endif // DEBUG_ENABLED

				BlockNode *check_block = p_block;
				while (check_block) {
					if (check_block->variables.has(id->name)) {
						_set_error("Variable \"" + String(id->name) + "\" already defined in the scope (at line " + itos(check_block->variables[id->name]->line) + ").");
						return;
					}
					check_block = check_block->parent_block;
				}

				tokenizer->advance();

				if (tokenizer->get_token() != GDScriptTokenizer::TK_OP_IN) {
					_set_error("\"in\" expected after identifier.");
					return;
				}

				tokenizer->advance();

				Node *container = _parse_and_reduce_expression(p_block, p_static);
				if (!container) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}

				DataType iter_type;

				if (container->type == Node::TYPE_OPERATOR) {
					OperatorNode *op = static_cast<OperatorNode *>(container);
					if (op->op == OperatorNode::OP_CALL && op->arguments[0]->type == Node::TYPE_BUILT_IN_FUNCTION && static_cast<BuiltInFunctionNode *>(op->arguments[0])->function == GDScriptFunctions::GEN_RANGE) {
						//iterating a range, so see if range() can be optimized without allocating memory, by replacing it by vectors (which can work as iterable too!)

						Vector<Node *> args;
						Vector<double> constants;

						bool constant = true;

						for (int i = 1; i < op->arguments.size(); i++) {
							args.push_back(op->arguments[i]);
							if (op->arguments[i]->type == Node::TYPE_CONSTANT) {
								ConstantNode *c = static_cast<ConstantNode *>(op->arguments[i]);
								if (c->value.get_type() == Variant::REAL || c->value.get_type() == Variant::INT) {
									constants.push_back(c->value);
								} else {
									constant = false;
								}
							} else {
								constant = false;
							}
						}

						if (args.size() > 0 && args.size() < 4) {
							if (constant) {
								ConstantNode *cn = alloc_node<ConstantNode>();
								switch (args.size()) {
									case 1:
										cn->value = (int)constants[0];
										break;
									case 2:
										cn->value = Vector2(constants[0], constants[1]);
										break;
									case 3:
										cn->value = Vector3(constants[0], constants[1], constants[2]);
										break;
								}
								cn->datatype = _type_from_variant(cn->value);
								container = cn;
							} else {
								OperatorNode *on = alloc_node<OperatorNode>();
								on->op = OperatorNode::OP_CALL;

								TypeNode *tn = alloc_node<TypeNode>();
								on->arguments.push_back(tn);

								switch (args.size()) {
									case 1:
										tn->vtype = Variant::INT;
										break;
									case 2:
										tn->vtype = Variant::VECTOR2;
										break;
									case 3:
										tn->vtype = Variant::VECTOR3;
										break;
								}

								for (int i = 0; i < args.size(); i++) {
									on->arguments.push_back(args[i]);
								}

								container = on;
							}
						}

						iter_type.has_type = true;
						iter_type.kind = DataType::BUILTIN;
						iter_type.builtin_type = Variant::INT;
					}
				}

				ControlFlowNode *cf_for = alloc_node<ControlFlowNode>();

				cf_for->cf_type = ControlFlowNode::CF_FOR;
				cf_for->arguments.push_back(id);
				cf_for->arguments.push_back(container);

				cf_for->body = alloc_node<BlockNode>();
				cf_for->body->parent_block = p_block;
				cf_for->body->can_break = true;
				cf_for->body->can_continue = true;
				p_block->sub_blocks.push_back(cf_for->body);

				if (!_enter_indent_block(cf_for->body)) {
					_set_error("Expected indented block after \"for\".");
					p_block->end_line = tokenizer->get_token_line();
					return;
				}

				current_block = cf_for->body;

				// this is for checking variable for redefining
				// inside this _parse_block
				LocalVarNode *lv = alloc_node<LocalVarNode>();
				lv->name = id->name;
				lv->line = id->line;
				lv->assignments++;
				id->declared_block = cf_for->body;
				lv->set_datatype(iter_type);
				id->set_datatype(iter_type);
				cf_for->body->variables.insert(id->name, lv);
				_parse_block(cf_for->body, p_static);
				current_block = p_block;

				if (error_set) {
					return;
				}
				p_block->statements.push_back(cf_for);
			} break;
			case GDScriptTokenizer::TK_CF_CONTINUE: {
				BlockNode *upper_block = p_block;
				bool is_continue_valid = false;
				while (upper_block) {
					if (upper_block->can_continue) {
						is_continue_valid = true;
						break;
					}
					upper_block = upper_block->parent_block;
				}

				if (!is_continue_valid) {
					_set_error("Unexpected keyword \"continue\" outside a loop.");
					return;
				}

				_mark_line_as_safe(tokenizer->get_token_line());
				tokenizer->advance();
				ControlFlowNode *cf_continue = alloc_node<ControlFlowNode>();
				cf_continue->cf_type = ControlFlowNode::CF_CONTINUE;
				p_block->statements.push_back(cf_continue);
				if (!_end_statement()) {
					_set_end_statement_error("continue");
					return;
				}
			} break;
			case GDScriptTokenizer::TK_CF_BREAK: {
				BlockNode *upper_block = p_block;
				bool is_break_valid = false;
				while (upper_block) {
					if (upper_block->can_break) {
						is_break_valid = true;
						break;
					}
					upper_block = upper_block->parent_block;
				}

				if (!is_break_valid) {
					_set_error("Unexpected keyword \"break\" outside a loop.");
					return;
				}

				_mark_line_as_safe(tokenizer->get_token_line());
				tokenizer->advance();
				ControlFlowNode *cf_break = alloc_node<ControlFlowNode>();
				cf_break->cf_type = ControlFlowNode::CF_BREAK;
				p_block->statements.push_back(cf_break);
				if (!_end_statement()) {
					_set_end_statement_error("break");
					return;
				}
			} break;
			case GDScriptTokenizer::TK_CF_RETURN: {
				tokenizer->advance();
				ControlFlowNode *cf_return = alloc_node<ControlFlowNode>();
				cf_return->cf_type = ControlFlowNode::CF_RETURN;
				cf_return->line = tokenizer->get_token_line(-1);

				if (tokenizer->get_token() == GDScriptTokenizer::TK_SEMICOLON || tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE || tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {
					//expect end of statement
					p_block->statements.push_back(cf_return);
					if (!_end_statement()) {
						return;
					}
				} else {
					//expect expression
					Node *retexpr = _parse_and_reduce_expression(p_block, p_static);
					if (!retexpr) {
						if (_recover_from_completion()) {
							break;
						}
						return;
					}
					cf_return->arguments.push_back(retexpr);
					p_block->statements.push_back(cf_return);
					if (!_end_statement()) {
						_set_end_statement_error("return");
						return;
					}
				}
				p_block->has_return = true;

			} break;
			case GDScriptTokenizer::TK_CF_MATCH: {
				tokenizer->advance();

				MatchNode *match_node = alloc_node<MatchNode>();

				Node *val_to_match = _parse_and_reduce_expression(p_block, p_static);

				if (!val_to_match) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}

				match_node->val_to_match = val_to_match;

				if (!_enter_indent_block()) {
					_set_error("Expected indented pattern matching block after \"match\".");
					return;
				}

				BlockNode *compiled_branches = alloc_node<BlockNode>();
				compiled_branches->parent_block = p_block;
				compiled_branches->parent_class = p_block->parent_class;
				compiled_branches->can_continue = true;

				p_block->sub_blocks.push_back(compiled_branches);

				_parse_pattern_block(compiled_branches, match_node->branches, p_static);

				if (error_set) {
					return;
				}

				ControlFlowNode *match_cf_node = alloc_node<ControlFlowNode>();
				match_cf_node->cf_type = ControlFlowNode::CF_MATCH;
				match_cf_node->match = match_node;
				match_cf_node->body = compiled_branches;

				p_block->has_return = p_block->has_return || compiled_branches->has_return;
				p_block->statements.push_back(match_cf_node);

				_end_statement();
			} break;
			case GDScriptTokenizer::TK_PR_ASSERT: {
				tokenizer->advance();

				if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
					_set_error("Expected '(' after assert");
					return;
				}

				int assert_line = tokenizer->get_token_line();

				tokenizer->advance();

				Vector<Node *> args;
				const bool result = _parse_arguments(p_block, args, p_static);
				if (!result) {
					return;
				}

				if (args.empty() || args.size() > 2) {
					_set_error("Wrong number of arguments, expected 1 or 2", assert_line);
					return;
				}

				AssertNode *an = alloc_node<AssertNode>();
				an->condition = _reduce_expression(args[0], p_static);
				an->line = assert_line;

				if (args.size() == 2) {
					an->message = _reduce_expression(args[1], p_static);
				} else {
					ConstantNode *message_node = alloc_node<ConstantNode>();
					message_node->value = String();
					an->message = message_node;
				}

				p_block->statements.push_back(an);

				if (!_end_statement()) {
					_set_end_statement_error("assert");
					return;
				}
			} break;
			case GDScriptTokenizer::TK_PR_BREAKPOINT: {
				tokenizer->advance();
				BreakpointNode *bn = alloc_node<BreakpointNode>();
				p_block->statements.push_back(bn);

				if (!_end_statement()) {
					_set_end_statement_error("breakpoint");
					return;
				}
			} break;
			default: {
				Node *expression = _parse_and_reduce_expression(p_block, p_static, false, true);
				if (!expression) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}
				p_block->statements.push_back(expression);
				if (!_end_statement()) {
					// Attempt to guess a better error message if the user "retypes" a variable
					if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON && tokenizer->get_token(1) == GDScriptTokenizer::TK_OP_ASSIGN) {
						_set_error("Unexpected ':=', use '=' instead. Expected end of statement after expression.");
					} else {
						_set_error(vformat("Expected end of statement after expression, got %s instead.", tokenizer->get_token_name(tokenizer->get_token())));
					}
					return;
				}

			} break;
		}
	}
}

bool GDScriptParser::_parse_newline() {
	if (tokenizer->get_token(1) != GDScriptTokenizer::TK_EOF && tokenizer->get_token(1) != GDScriptTokenizer::TK_NEWLINE) {
		IndentLevel current_level = indent_level.back()->get();
		int indent = tokenizer->get_token_line_indent();
		int tabs = tokenizer->get_token_line_tab_indent();
		IndentLevel new_level(indent, tabs);

		if (new_level.is_mixed(current_level)) {
			_set_error("Mixed tabs and spaces in indentation.");
			return false;
		}

		if (indent > current_level.indent) {
			_set_error("Unexpected indentation.");
			return false;
		}

		if (indent < current_level.indent) {
			while (indent < current_level.indent) {
				//exit block
				if (indent_level.size() == 1) {
					_set_error("Invalid indentation. Bug?");
					return false;
				}

				indent_level.pop_back();

				if (indent_level.back()->get().indent < indent) {
					_set_error("Unindent does not match any outer indentation level.");
					return false;
				}

				if (indent_level.back()->get().is_mixed(current_level)) {
					_set_error("Mixed tabs and spaces in indentation.");
					return false;
				}

				current_level = indent_level.back()->get();
			}

			tokenizer->advance();
			return false;
		}
	}

	tokenizer->advance();
	return true;
}

void GDScriptParser::_parse_extends(ClassNode *p_class) {
	if (p_class->extends_used) {
		_set_error("\"extends\" can only be present once per script.");
		return;
	}

	if (!p_class->constant_expressions.empty() || !p_class->subclasses.empty() || !p_class->functions.empty() || !p_class->variables.empty()) {
		_set_error("\"extends\" must be used before anything else.");
		return;
	}

	p_class->extends_used = true;

	tokenizer->advance();

	if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE && tokenizer->get_token_type() == Variant::OBJECT) {
		p_class->extends_class.push_back(Variant::get_type_name(Variant::OBJECT));
		tokenizer->advance();
		return;
	}

	// see if inheritance happens from a file
	if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT) {
		Variant constant = tokenizer->get_token_constant();
		if (constant.get_type() != Variant::STRING) {
			_set_error("\"extends\" constant must be a string.");
			return;
		}

		p_class->extends_file = constant;
		tokenizer->advance();

		// Add parent script as a dependency
		String parent = constant;
		if (parent.is_rel_path()) {
			parent = base_path.plus_file(parent).simplify_path();
		}
		dependencies.push_back(parent);

		if (tokenizer->get_token() != GDScriptTokenizer::TK_PERIOD) {
			return;
		} else {
			tokenizer->advance();
		}
	}

	while (true) {
		switch (tokenizer->get_token()) {
			case GDScriptTokenizer::TK_IDENTIFIER: {
				StringName identifier = tokenizer->get_token_identifier();
				p_class->extends_class.push_back(identifier);
			} break;

			case GDScriptTokenizer::TK_CURSOR:
			case GDScriptTokenizer::TK_PERIOD:
				break;

			default: {
				_set_error("Invalid \"extends\" syntax, expected string constant (path) and/or identifier (parent class).");
				return;
			}
		}

		tokenizer->advance(1);

		switch (tokenizer->get_token()) {
			case GDScriptTokenizer::TK_IDENTIFIER:
			case GDScriptTokenizer::TK_PERIOD:
				continue;
			case GDScriptTokenizer::TK_CURSOR:
				completion_type = COMPLETION_EXTENDS;
				completion_class = current_class;
				completion_function = current_function;
				completion_line = tokenizer->get_token_line();
				completion_block = current_block;
				completion_ident_is_call = false;
				completion_found = true;
				return;
			default:
				return;
		}
	}
}

void GDScriptParser::_parse_class(ClassNode *p_class) {
	IndentLevel current_level = indent_level.back()->get();

	while (true) {
		GDScriptTokenizer::Token token = tokenizer->get_token();
		if (error_set) {
			return;
		}

		if (current_level.indent > indent_level.back()->get().indent) {
			p_class->end_line = tokenizer->get_token_line();
			return; //go back a level
		}

		switch (token) {
			case GDScriptTokenizer::TK_CURSOR: {
				tokenizer->advance();
			} break;
			case GDScriptTokenizer::TK_EOF:
				p_class->end_line = tokenizer->get_token_line();
			case GDScriptTokenizer::TK_ERROR: {
				return; //go back
				//end of file!
			} break;
			case GDScriptTokenizer::TK_NEWLINE: {
				if (!_parse_newline()) {
					if (!error_set) {
						p_class->end_line = tokenizer->get_token_line();
					}
					return;
				}
			} break;
			case GDScriptTokenizer::TK_PR_EXTENDS: {
				_mark_line_as_safe(tokenizer->get_token_line());
				_parse_extends(p_class);
				if (error_set) {
					return;
				}
				if (!_end_statement()) {
					_set_end_statement_error("extends");
					return;
				}

			} break;
			case GDScriptTokenizer::TK_PR_CLASS_NAME: {
				_mark_line_as_safe(tokenizer->get_token_line());
				if (p_class->owner) {
					_set_error("\"class_name\" is only valid for the main class namespace.");
					return;
				}
				if (self_path.begins_with("res://") && self_path.find("::") != -1) {
					_set_error("\"class_name\" isn't allowed in built-in scripts.");
					return;
				}
				if (tokenizer->get_token(1) != GDScriptTokenizer::TK_IDENTIFIER) {
					_set_error("\"class_name\" syntax: \"class_name <UniqueName>\"");
					return;
				}
				if (p_class->classname_used) {
					_set_error("\"class_name\" can only be present once per script.");
					return;
				}

				p_class->classname_used = true;

				p_class->name = tokenizer->get_token_identifier(1);

				if (self_path != String() && ScriptServer::is_global_class(p_class->name) && ScriptServer::get_global_class_path(p_class->name) != self_path) {
					_set_error("Unique global class \"" + p_class->name + "\" already exists at path: " + ScriptServer::get_global_class_path(p_class->name));
					return;
				}

				if (ClassDB::class_exists(p_class->name) || ClassDB::class_exists("_" + p_class->name.operator String())) {
					_set_error("The class \"" + p_class->name + "\" shadows a native class.");
					return;
				}

				if (p_class->classname_used && ProjectSettings::get_singleton()->has_setting("autoload/" + p_class->name)) {
					const String autoload_path = ProjectSettings::get_singleton()->get_setting("autoload/" + p_class->name);
					if (autoload_path.begins_with("*")) {
						// It's a singleton, and not just a regular AutoLoad script.
						_set_error("The class \"" + p_class->name + "\" conflicts with the AutoLoad singleton of the same name, and is therefore redundant. Remove the class_name declaration to fix this error.");
					}
					return;
				}

				tokenizer->advance(2);

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
					tokenizer->advance();

					if ((tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT && tokenizer->get_token_constant().get_type() == Variant::STRING)) {
#ifdef TOOLS_ENABLED
						if (Engine::get_singleton()->is_editor_hint()) {
							Variant constant = tokenizer->get_token_constant();
							String icon_path = constant.operator String();

							String abs_icon_path = icon_path.is_rel_path() ? self_path.get_base_dir().plus_file(icon_path).simplify_path() : icon_path;
							if (!FileAccess::exists(abs_icon_path)) {
								_set_error("No class icon found at: " + abs_icon_path);
								return;
							}

							p_class->icon_path = icon_path;
						}
#endif

						tokenizer->advance();
					} else {
						_set_error("The optional parameter after \"class_name\" must be a string constant file path to an icon.");
						return;
					}

				} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT) {
					_set_error("The class icon must be separated by a comma.");
					return;
				}

			} break;
			case GDScriptTokenizer::TK_PR_TOOL: {
				if (p_class->tool) {
					_set_error("The \"tool\" keyword can only be present once per script.");
					return;
				}

				p_class->tool = true;
				tokenizer->advance();

			} break;
			case GDScriptTokenizer::TK_PR_CLASS: {
				//class inside class :D

				StringName name;

				if (tokenizer->get_token(1) != GDScriptTokenizer::TK_IDENTIFIER) {
					_set_error("\"class\" syntax: \"class <Name>:\" or \"class <Name> extends <BaseClass>:\"");
					return;
				}
				name = tokenizer->get_token_identifier(1);
				tokenizer->advance(2);

				// Check if name is shadowing something else
				if (ClassDB::class_exists(name) || ClassDB::class_exists("_" + name.operator String())) {
					_set_error("The class \"" + String(name) + "\" shadows a native class.");
					return;
				}
				if (ScriptServer::is_global_class(name)) {
					_set_error("Can't override name of the unique global class \"" + name + "\". It already exists at: " + ScriptServer::get_global_class_path(p_class->name));
					return;
				}
				ClassNode *outer_class = p_class;
				while (outer_class) {
					for (int i = 0; i < outer_class->subclasses.size(); i++) {
						if (outer_class->subclasses[i]->name == name) {
							_set_error("Another class named \"" + String(name) + "\" already exists in this scope (at line " + itos(outer_class->subclasses[i]->line) + ").");
							return;
						}
					}
					if (outer_class->constant_expressions.has(name)) {
						_set_error("A constant named \"" + String(name) + "\" already exists in the outer class scope (at line" + itos(outer_class->constant_expressions[name].expression->line) + ").");
						return;
					}
					for (int i = 0; i < outer_class->variables.size(); i++) {
						if (outer_class->variables[i].identifier == name) {
							_set_error("A variable named \"" + String(name) + "\" already exists in the outer class scope (at line " + itos(outer_class->variables[i].line) + ").");
							return;
						}
					}

					outer_class = outer_class->owner;
				}

				ClassNode *newclass = alloc_node<ClassNode>();
				newclass->initializer = alloc_node<BlockNode>();
				newclass->initializer->parent_class = newclass;
				newclass->ready = alloc_node<BlockNode>();
				newclass->ready->parent_class = newclass;
				newclass->name = name;
				newclass->owner = p_class;

				p_class->subclasses.push_back(newclass);

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_EXTENDS) {
					_parse_extends(newclass);
					if (error_set) {
						return;
					}
				}

				if (!_enter_indent_block()) {
					_set_error("Indented block expected.");
					return;
				}
				current_class = newclass;
				_parse_class(newclass);
				current_class = p_class;

			} break;
			/* this is for functions....
			case GDScriptTokenizer::TK_CF_PASS: {

				tokenizer->advance(1);
			} break;
			*/
			case GDScriptTokenizer::TK_PR_STATIC: {
				tokenizer->advance();
				if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
					_set_error("Expected \"func\".");
					return;
				}

				FALLTHROUGH;
			}
			case GDScriptTokenizer::TK_PR_FUNCTION: {
				bool _static = false;
				pending_newline = -1;

				if (tokenizer->get_token(-1) == GDScriptTokenizer::TK_PR_STATIC) {
					_static = true;
				}

				tokenizer->advance();
				StringName name;

				if (_get_completable_identifier(COMPLETION_VIRTUAL_FUNC, name)) {
				}

				if (name == StringName()) {
					_set_error("Expected an identifier after \"func\" (syntax: \"func <identifier>([arguments]):\").");
					return;
				}

				for (int i = 0; i < p_class->functions.size(); i++) {
					if (p_class->functions[i]->name == name) {
						_set_error("The function \"" + String(name) + "\" already exists in this class (at line " + itos(p_class->functions[i]->line) + ").");
					}
				}
				for (int i = 0; i < p_class->static_functions.size(); i++) {
					if (p_class->static_functions[i]->name == name) {
						_set_error("The function \"" + String(name) + "\" already exists in this class (at line " + itos(p_class->static_functions[i]->line) + ").");
					}
				}

#ifdef DEBUG_ENABLED
				if (p_class->constant_expressions.has(name)) {
					_add_warning(GDScriptWarning::FUNCTION_CONFLICTS_CONSTANT, -1, name);
				}
				for (int i = 0; i < p_class->variables.size(); i++) {
					if (p_class->variables[i].identifier == name) {
						_add_warning(GDScriptWarning::FUNCTION_CONFLICTS_VARIABLE, -1, name);
					}
				}
				for (int i = 0; i < p_class->subclasses.size(); i++) {
					if (p_class->subclasses[i]->name == name) {
						_add_warning(GDScriptWarning::FUNCTION_CONFLICTS_CONSTANT, -1, name);
					}
				}
#endif // DEBUG_ENABLED

				if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
					_set_error("Expected \"(\" after the identifier (syntax: \"func <identifier>([arguments]):\" ).");
					return;
				}

				tokenizer->advance();

				Vector<StringName> arguments;
				Vector<DataType> argument_types;
				Vector<Node *> default_values;
#ifdef DEBUG_ENABLED
				Vector<int> arguments_usage;
#endif // DEBUG_ENABLED

				int fnline = tokenizer->get_token_line();

				if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
					//has arguments
					bool defaulting = false;
					while (true) {
						if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
							tokenizer->advance();
							continue;
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_VAR) {
							tokenizer->advance(); //var before the identifier is allowed
						}

						if (!tokenizer->is_token_literal(0, true)) {
							_set_error("Expected an identifier for an argument.");
							return;
						}

						StringName argname = tokenizer->get_token_identifier();
						for (int i = 0; i < arguments.size(); i++) {
							if (arguments[i] == argname) {
								_set_error("The argument name \"" + String(argname) + "\" is defined multiple times.");
								return;
							}
						}
						arguments.push_back(argname);
#ifdef DEBUG_ENABLED
						arguments_usage.push_back(0);
#endif // DEBUG_ENABLED

						tokenizer->advance();

						DataType argtype;
						if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
							if (tokenizer->get_token(1) == GDScriptTokenizer::TK_OP_ASSIGN) {
								argtype.infer_type = true;
								tokenizer->advance();
							} else if (!_parse_type(argtype)) {
								_set_error("Expected a type for an argument.");
								return;
							}
						}
						argument_types.push_back(argtype);

						if (defaulting && tokenizer->get_token() != GDScriptTokenizer::TK_OP_ASSIGN) {
							_set_error("Default parameter expected.");
							return;
						}

						//tokenizer->advance();

						if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_ASSIGN) {
							defaulting = true;
							tokenizer->advance(1);
							Node *defval = _parse_and_reduce_expression(p_class, _static);
							if (!defval || error_set) {
								return;
							}

							OperatorNode *on = alloc_node<OperatorNode>();
							on->op = OperatorNode::OP_ASSIGN;
							on->line = fnline;

							IdentifierNode *in = alloc_node<IdentifierNode>();
							in->name = argname;
							in->line = fnline;

							on->arguments.push_back(in);
							on->arguments.push_back(defval);
							/* no ..
							if (defval->type!=Node::TYPE_CONSTANT) {

								_set_error("default argument must be constant");
							}
							*/
							default_values.push_back(on);
						}

						while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
							tokenizer->advance();
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
							tokenizer->advance();
							continue;
						} else if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
							_set_error("Expected \",\" or \")\".");
							return;
						}

						break;
					}
				}

				tokenizer->advance();

				BlockNode *block = alloc_node<BlockNode>();
				block->parent_class = p_class;

				FunctionNode *function = alloc_node<FunctionNode>();
				function->name = name;
				function->arguments = arguments;
				function->argument_types = argument_types;
				function->default_values = default_values;
				function->_static = _static;
				function->line = fnline;
#ifdef DEBUG_ENABLED
				function->arguments_usage = arguments_usage;
#endif // DEBUG_ENABLED
				function->rpc_mode = rpc_mode;
				rpc_mode = MultiplayerAPI::RPC_MODE_DISABLED;

				if (name == "_init") {
					if (_static) {
						_set_error("The constructor cannot be static.");
						return;
					}

					if (p_class->extends_used) {
						OperatorNode *cparent = alloc_node<OperatorNode>();
						cparent->op = OperatorNode::OP_PARENT_CALL;
						block->statements.push_back(cparent);

						IdentifierNode *id = alloc_node<IdentifierNode>();
						id->name = "_init";
						cparent->arguments.push_back(id);

						if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD) {
							tokenizer->advance();
							if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
								_set_error("Expected \"(\" for parent constructor arguments.");
								return;
							}
							tokenizer->advance();

							if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
								//has arguments
								parenthesis++;
								while (true) {
									current_function = function;
									Node *arg = _parse_and_reduce_expression(p_class, _static);
									if (!arg) {
										return;
									}
									current_function = nullptr;
									cparent->arguments.push_back(arg);

									if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
										tokenizer->advance();
										continue;
									} else if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
										_set_error("Expected \",\" or \")\".");
										return;
									}

									break;
								}
								parenthesis--;
							}

							tokenizer->advance();
						}
					} else {
						if (tokenizer->get_token() == GDScriptTokenizer::TK_PERIOD) {
							_set_error("Parent constructor call found for a class without inheritance.");
							return;
						}
					}
				}

				DataType return_type;
				if (tokenizer->get_token() == GDScriptTokenizer::TK_FORWARD_ARROW) {
					if (!_parse_type(return_type, true)) {
						_set_error("Expected a return type for the function.");
						return;
					}
				}

				if (!_enter_indent_block(block)) {
					_set_error(vformat("Indented block expected after declaration of \"%s\" function.", function->name));
					return;
				}

				function->return_type = return_type;

				if (_static) {
					p_class->static_functions.push_back(function);
				} else {
					p_class->functions.push_back(function);
				}

				current_function = function;
				function->body = block;
				current_block = block;
				_parse_block(block, _static);
				current_block = nullptr;

				//arguments
			} break;
			case GDScriptTokenizer::TK_PR_SIGNAL: {
				_mark_line_as_safe(tokenizer->get_token_line());
				tokenizer->advance();

				if (!tokenizer->is_token_literal()) {
					_set_error("Expected an identifier after \"signal\".");
					return;
				}

				ClassNode::Signal sig;
				sig.name = tokenizer->get_token_identifier();
				sig.emissions = 0;
				sig.line = tokenizer->get_token_line();

				for (int i = 0; i < current_class->_signals.size(); i++) {
					if (current_class->_signals[i].name == sig.name) {
						_set_error("The signal \"" + sig.name + "\" already exists in this class (at line: " + itos(current_class->_signals[i].line) + ").");
						return;
					}
				}

				tokenizer->advance();

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
					tokenizer->advance();
					while (true) {
						if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
							tokenizer->advance();
							continue;
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
							tokenizer->advance();
							break;
						}

						if (!tokenizer->is_token_literal(0, true)) {
							_set_error("Expected an identifier in a \"signal\" argument.");
							return;
						}

						sig.arguments.push_back(tokenizer->get_token_identifier());
						tokenizer->advance();

						while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
							tokenizer->advance();
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
							tokenizer->advance();
						} else if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
							_set_error("Expected \",\" or \")\" after a \"signal\" parameter identifier.");
							return;
						}
					}
				}

				p_class->_signals.push_back(sig);

				if (!_end_statement()) {
					_set_end_statement_error("signal");
					return;
				}
			} break;
			case GDScriptTokenizer::TK_PR_EXPORT: {
				tokenizer->advance();

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_OPEN) {
#define _ADVANCE_AND_CONSUME_NEWLINES \
	do {                              \
		tokenizer->advance();         \
	} while (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE)

					_ADVANCE_AND_CONSUME_NEWLINES;
					parenthesis++;

					String hint_prefix = "";
					bool is_arrayed = false;

					while (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE &&
							tokenizer->get_token_type() == Variant::ARRAY &&
							tokenizer->get_token(1) == GDScriptTokenizer::TK_COMMA) {
						tokenizer->advance(); // Array
						tokenizer->advance(); // Comma
						if (is_arrayed) {
							hint_prefix += itos(Variant::ARRAY) + ":";
						} else {
							is_arrayed = true;
						}
					}

					if (tokenizer->get_token() == GDScriptTokenizer::TK_BUILT_IN_TYPE) {
						Variant::Type type = tokenizer->get_token_type();
						if (type == Variant::NIL) {
							_set_error("Can't export null type.");
							return;
						}
						if (type == Variant::OBJECT) {
							_set_error("Can't export raw object type.");
							return;
						}
						current_export.type = type;
						current_export.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
						_ADVANCE_AND_CONSUME_NEWLINES;

						if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
							// hint expected next!
							_ADVANCE_AND_CONSUME_NEWLINES;

							switch (type) {
								case Variant::INT: {
									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "FLAGS") {
										_ADVANCE_AND_CONSUME_NEWLINES;

										if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											WARN_DEPRECATED_MSG("Exporting bit flags hint requires string constants.");
											break;
										}
										if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
											_set_error("Expected \",\" in the bit flags hint.");
											return;
										}

										current_export.hint = PROPERTY_HINT_FLAGS;
										_ADVANCE_AND_CONSUME_NEWLINES;

										bool first = true;
										while (true) {
											if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || tokenizer->get_token_constant().get_type() != Variant::STRING) {
												current_export = PropertyInfo();
												_set_error("Expected a string constant in the named bit flags hint.");
												return;
											}

											String c = tokenizer->get_token_constant();
											if (!first) {
												current_export.hint_string += ",";
											} else {
												first = false;
											}

											current_export.hint_string += c.xml_escape();

											_ADVANCE_AND_CONSUME_NEWLINES;
											if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
												break;
											}

											if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
												current_export = PropertyInfo();
												_set_error("Expected \")\" or \",\" in the named bit flags hint.");
												return;
											}
											_ADVANCE_AND_CONSUME_NEWLINES;
										}

										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "LAYERS_2D_RENDER") {
										_ADVANCE_AND_CONSUME_NEWLINES;
										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected \")\" in the layers 2D render hint.");
											return;
										}
										current_export.hint = PROPERTY_HINT_LAYERS_2D_RENDER;
										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "LAYERS_2D_PHYSICS") {
										_ADVANCE_AND_CONSUME_NEWLINES;
										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected \")\" in the layers 2D physics hint.");
											return;
										}
										current_export.hint = PROPERTY_HINT_LAYERS_2D_PHYSICS;
										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "LAYERS_2D_NAVIGATION") {
										_ADVANCE_AND_CONSUME_NEWLINES;
										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected \")\" in the layers 2D navigation hint.");
											return;
										}
										current_export.hint = PROPERTY_HINT_LAYERS_2D_NAVIGATION;
										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "LAYERS_3D_RENDER") {
										_ADVANCE_AND_CONSUME_NEWLINES;
										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected \")\" in the layers 3D render hint.");
											return;
										}
										current_export.hint = PROPERTY_HINT_LAYERS_3D_RENDER;
										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "LAYERS_3D_PHYSICS") {
										_ADVANCE_AND_CONSUME_NEWLINES;
										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected \")\" in the layers 3D physics hint.");
											return;
										}
										current_export.hint = PROPERTY_HINT_LAYERS_3D_PHYSICS;
										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "LAYERS_3D_NAVIGATION") {
										_ADVANCE_AND_CONSUME_NEWLINES;
										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected \")\" in the layers 3D navigation hint.");
											return;
										}
										current_export.hint = PROPERTY_HINT_LAYERS_3D_NAVIGATION;
										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT && tokenizer->get_token_constant().get_type() == Variant::STRING) {
										//enumeration
										current_export.hint = PROPERTY_HINT_ENUM;
										bool first = true;
										while (true) {
											if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || tokenizer->get_token_constant().get_type() != Variant::STRING) {
												current_export = PropertyInfo();
												_set_error("Expected a string constant in the enumeration hint.");
												return;
											}

											String c = tokenizer->get_token_constant();
											if (!first) {
												current_export.hint_string += ",";
											} else {
												first = false;
											}

											current_export.hint_string += c.xml_escape();

											_ADVANCE_AND_CONSUME_NEWLINES;
											if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
												break;
											}

											if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
												current_export = PropertyInfo();
												_set_error("Expected \")\" or \",\" in the enumeration hint.");
												return;
											}

											_ADVANCE_AND_CONSUME_NEWLINES;
										}

										break;
									}

									FALLTHROUGH;
								}
								case Variant::REAL: {
									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "EASE") {
										current_export.hint = PROPERTY_HINT_EXP_EASING;
										_ADVANCE_AND_CONSUME_NEWLINES;
										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected \")\" in the hint.");
											return;
										}
										break;
									}

									// range
									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "EXP") {
										current_export.hint = PROPERTY_HINT_EXP_RANGE;
										_ADVANCE_AND_CONSUME_NEWLINES;

										if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											break;
										} else if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
											_set_error("Expected \")\" or \",\" in the exponential range hint.");
											return;
										}
										_ADVANCE_AND_CONSUME_NEWLINES;
									} else {
										current_export.hint = PROPERTY_HINT_RANGE;
									}

									float sign = 1.0;

									if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_SUB) {
										sign = -1;
										_ADVANCE_AND_CONSUME_NEWLINES;
									}
									if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || !tokenizer->get_token_constant().is_num()) {
										current_export = PropertyInfo();
										_set_error("Expected a range in the numeric hint.");
										return;
									}

									current_export.hint_string = rtos(sign * double(tokenizer->get_token_constant()));
									_ADVANCE_AND_CONSUME_NEWLINES;

									if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
										current_export.hint_string = "0," + current_export.hint_string;
										break;
									}

									if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
										current_export = PropertyInfo();
										_set_error("Expected \",\" or \")\" in the numeric range hint.");
										return;
									}

									_ADVANCE_AND_CONSUME_NEWLINES;

									sign = 1.0;
									if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_SUB) {
										sign = -1;
										_ADVANCE_AND_CONSUME_NEWLINES;
									}

									if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || !tokenizer->get_token_constant().is_num()) {
										current_export = PropertyInfo();
										_set_error("Expected a number as upper bound in the numeric range hint.");
										return;
									}

									current_export.hint_string += "," + rtos(sign * double(tokenizer->get_token_constant()));
									_ADVANCE_AND_CONSUME_NEWLINES;

									if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
										break;
									}

									if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
										current_export = PropertyInfo();
										_set_error("Expected \",\" or \")\" in the numeric range hint.");
										return;
									}

									_ADVANCE_AND_CONSUME_NEWLINES;
									sign = 1.0;
									if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_SUB) {
										sign = -1;
										_ADVANCE_AND_CONSUME_NEWLINES;
									}

									if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || !tokenizer->get_token_constant().is_num()) {
										current_export = PropertyInfo();
										_set_error("Expected a number as step in the numeric range hint.");
										return;
									}

									current_export.hint_string += "," + rtos(sign * double(tokenizer->get_token_constant()));
									_ADVANCE_AND_CONSUME_NEWLINES;

								} break;
								case Variant::STRING: {
									if (tokenizer->get_token() == GDScriptTokenizer::TK_CONSTANT && tokenizer->get_token_constant().get_type() == Variant::STRING) {
										//enumeration
										current_export.hint = PROPERTY_HINT_ENUM;
										bool first = true;
										while (true) {
											if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || tokenizer->get_token_constant().get_type() != Variant::STRING) {
												current_export = PropertyInfo();
												_set_error("Expected a string constant in the enumeration hint.");
												return;
											}

											String c = tokenizer->get_token_constant();
											if (!first) {
												current_export.hint_string += ",";
											} else {
												first = false;
											}

											current_export.hint_string += c.xml_escape();
											_ADVANCE_AND_CONSUME_NEWLINES;
											if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
												break;
											}

											if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
												current_export = PropertyInfo();
												_set_error("Expected \")\" or \",\" in the enumeration hint.");
												return;
											}
											_ADVANCE_AND_CONSUME_NEWLINES;
										}

										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "DIR") {
										_ADVANCE_AND_CONSUME_NEWLINES;

										if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											current_export.hint = PROPERTY_HINT_DIR;
										} else if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
											_ADVANCE_AND_CONSUME_NEWLINES;

											if (tokenizer->get_token() != GDScriptTokenizer::TK_IDENTIFIER || !(tokenizer->get_token_identifier() == "GLOBAL")) {
												_set_error("Expected \"GLOBAL\" after comma in the directory hint.");
												return;
											}
											if (!p_class->tool) {
												_set_error("Global filesystem hints may only be used in tool scripts.");
												return;
											}
											current_export.hint = PROPERTY_HINT_GLOBAL_DIR;
											_ADVANCE_AND_CONSUME_NEWLINES;

											if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
												_set_error("Expected \")\" in the hint.");
												return;
											}
										} else {
											_set_error("Expected \")\" or \",\" in the hint.");
											return;
										}
										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "FILE") {
										current_export.hint = PROPERTY_HINT_FILE;
										_ADVANCE_AND_CONSUME_NEWLINES;

										if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
											_ADVANCE_AND_CONSUME_NEWLINES;

											if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "GLOBAL") {
												if (!p_class->tool) {
													_set_error("Global filesystem hints may only be used in tool scripts.");
													return;
												}
												current_export.hint = PROPERTY_HINT_GLOBAL_FILE;
												_ADVANCE_AND_CONSUME_NEWLINES;

												if (tokenizer->get_token() == GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
													break;
												} else if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
													_ADVANCE_AND_CONSUME_NEWLINES;
												} else {
													_set_error("Expected \")\" or \",\" in the hint.");
													return;
												}
											}

											if (tokenizer->get_token() != GDScriptTokenizer::TK_CONSTANT || tokenizer->get_token_constant().get_type() != Variant::STRING) {
												if (current_export.hint == PROPERTY_HINT_GLOBAL_FILE) {
													_set_error("Expected string constant with filter.");
												} else {
													_set_error("Expected \"GLOBAL\" or string constant with filter.");
												}
												return;
											}
											current_export.hint_string = tokenizer->get_token_constant();
											_ADVANCE_AND_CONSUME_NEWLINES;
										}

										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected \")\" in the hint.");
											return;
										}
										break;
									}

									if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "MULTILINE") {
										current_export.hint = PROPERTY_HINT_MULTILINE_TEXT;
										_ADVANCE_AND_CONSUME_NEWLINES;
										if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
											_set_error("Expected \")\" in the hint.");
											return;
										}
										break;
									}
								} break;
								case Variant::COLOR: {
									if (tokenizer->get_token() != GDScriptTokenizer::TK_IDENTIFIER) {
										current_export = PropertyInfo();
										_set_error("Color type hint expects RGB or RGBA as hints.");
										return;
									}

									String identifier = tokenizer->get_token_identifier();
									if (identifier == "RGB") {
										current_export.hint = PROPERTY_HINT_COLOR_NO_ALPHA;
									} else if (identifier == "RGBA") {
										//none
									} else {
										current_export = PropertyInfo();
										_set_error("Color type hint expects RGB or RGBA as hints.");
										return;
									}
									_ADVANCE_AND_CONSUME_NEWLINES;

								} break;
								default: {
									current_export = PropertyInfo();
									_set_error("Type \"" + Variant::get_type_name(type) + "\" can't take hints.");
									return;
								} break;
							}
						}

					} else {
						parenthesis++;
						Node *subexpr = _parse_and_reduce_expression(p_class, true, true);
						if (!subexpr) {
							if (_recover_from_completion()) {
								break;
							}
							return;
						}
						parenthesis--;

						if (subexpr->type != Node::TYPE_CONSTANT) {
							current_export = PropertyInfo();
							_set_error("Expected a constant expression.");
							return;
						}

						Variant constant = static_cast<ConstantNode *>(subexpr)->value;

						if (constant.get_type() == Variant::OBJECT) {
							GDScriptNativeClass *native_class = Object::cast_to<GDScriptNativeClass>(constant);

							if (native_class && ClassDB::is_parent_class(native_class->get_name(), "Resource")) {
								current_export.type = Variant::OBJECT;
								current_export.hint = PROPERTY_HINT_RESOURCE_TYPE;
								current_export.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;

								current_export.hint_string = native_class->get_name();
								current_export.class_name = native_class->get_name();

							} else {
								current_export = PropertyInfo();
								_set_error("The export hint isn't a resource type.");
							}
						} else if (constant.get_type() == Variant::DICTIONARY) {
							// Enumeration
							bool is_flags = false;

							if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
								_ADVANCE_AND_CONSUME_NEWLINES;

								if (tokenizer->get_token() == GDScriptTokenizer::TK_IDENTIFIER && tokenizer->get_token_identifier() == "FLAGS") {
									is_flags = true;
									_ADVANCE_AND_CONSUME_NEWLINES;
								} else {
									current_export = PropertyInfo();
									_set_error("Expected \"FLAGS\" after comma.");
								}
							}

							current_export.type = Variant::INT;
							current_export.hint = is_flags ? PROPERTY_HINT_FLAGS : PROPERTY_HINT_ENUM;
							current_export.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
							Dictionary enum_values = constant;

							List<Variant> keys;
							enum_values.get_key_list(&keys);

							bool first = true;
							for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
								if (enum_values[E->get()].get_type() == Variant::INT) {
									if (!first) {
										current_export.hint_string += ",";
									} else {
										first = false;
									}

									current_export.hint_string += E->get().operator String().capitalize().xml_escape();
									if (!is_flags) {
										current_export.hint_string += ":";
										current_export.hint_string += enum_values[E->get()].operator String().xml_escape();
									}
								}
							}
						} else {
							current_export = PropertyInfo();
							_set_error("Expected type for export.");
							return;
						}
					}

					if (tokenizer->get_token() != GDScriptTokenizer::TK_PARENTHESIS_CLOSE) {
						current_export = PropertyInfo();
						_set_error("Expected \")\" or \",\" after the export hint.");
						return;
					}

					tokenizer->advance();
					parenthesis--;

					if (is_arrayed) {
						hint_prefix += itos(current_export.type);
						if (current_export.hint) {
							hint_prefix += "/" + itos(current_export.hint);
						}
						current_export.hint_string = hint_prefix + ":" + current_export.hint_string;
						current_export.hint = PROPERTY_HINT_TYPE_STRING;
						current_export.type = Variant::ARRAY;
					}
#undef _ADVANCE_AND_CONSUME_NEWLINES
				}

				if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_ONREADY && tokenizer->get_token() != GDScriptTokenizer::TK_PR_REMOTE && tokenizer->get_token() != GDScriptTokenizer::TK_PR_MASTER && tokenizer->get_token() != GDScriptTokenizer::TK_PR_PUPPET && tokenizer->get_token() != GDScriptTokenizer::TK_PR_SYNC && tokenizer->get_token() != GDScriptTokenizer::TK_PR_REMOTESYNC && tokenizer->get_token() != GDScriptTokenizer::TK_PR_MASTERSYNC && tokenizer->get_token() != GDScriptTokenizer::TK_PR_PUPPETSYNC && tokenizer->get_token() != GDScriptTokenizer::TK_PR_SLAVE) {
					current_export = PropertyInfo();
					_set_error("Expected \"var\", \"onready\", \"remote\", \"master\", \"puppet\", \"sync\", \"remotesync\", \"mastersync\", \"puppetsync\".");
					return;
				}

				continue;
			} break;
			case GDScriptTokenizer::TK_PR_ONREADY: {
				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR) {
					_set_error("Expected \"var\".");
					return;
				}

				continue;
			} break;
			case GDScriptTokenizer::TK_PR_REMOTE: {
				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (current_export.type) {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR) {
						_set_error("Expected \"var\".");
						return;
					}

				} else {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
						_set_error("Expected \"var\" or \"func\".");
						return;
					}
				}
				rpc_mode = MultiplayerAPI::RPC_MODE_REMOTE;

				continue;
			} break;
			case GDScriptTokenizer::TK_PR_MASTER: {
				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (current_export.type) {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR) {
						_set_error("Expected \"var\".");
						return;
					}

				} else {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
						_set_error("Expected \"var\" or \"func\".");
						return;
					}
				}

				rpc_mode = MultiplayerAPI::RPC_MODE_MASTER;
				continue;
			} break;
			case GDScriptTokenizer::TK_PR_SLAVE:
#ifdef DEBUG_ENABLED
				_add_warning(GDScriptWarning::DEPRECATED_KEYWORD, tokenizer->get_token_line(), "slave", "puppet");
#endif
				FALLTHROUGH;
			case GDScriptTokenizer::TK_PR_PUPPET: {
				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (current_export.type) {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR) {
						_set_error("Expected \"var\".");
						return;
					}

				} else {
					if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
						_set_error("Expected \"var\" or \"func\".");
						return;
					}
				}

				rpc_mode = MultiplayerAPI::RPC_MODE_PUPPET;
				continue;
			} break;
			case GDScriptTokenizer::TK_PR_REMOTESYNC:
			case GDScriptTokenizer::TK_PR_SYNC: {
				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
					if (current_export.type) {
						_set_error("Expected \"var\".");
					} else {
						_set_error("Expected \"var\" or \"func\".");
					}
					return;
				}

				rpc_mode = MultiplayerAPI::RPC_MODE_REMOTESYNC;
				continue;
			} break;
			case GDScriptTokenizer::TK_PR_MASTERSYNC: {
				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
					if (current_export.type) {
						_set_error("Expected \"var\".");
					} else {
						_set_error("Expected \"var\" or \"func\".");
					}
					return;
				}

				rpc_mode = MultiplayerAPI::RPC_MODE_MASTERSYNC;
				continue;
			} break;
			case GDScriptTokenizer::TK_PR_PUPPETSYNC: {
				//may be fallthrough from export, ignore if so
				tokenizer->advance();
				if (tokenizer->get_token() != GDScriptTokenizer::TK_PR_VAR && tokenizer->get_token() != GDScriptTokenizer::TK_PR_FUNCTION) {
					if (current_export.type) {
						_set_error("Expected \"var\".");
					} else {
						_set_error("Expected \"var\" or \"func\".");
					}
					return;
				}

				rpc_mode = MultiplayerAPI::RPC_MODE_PUPPETSYNC;
				continue;
			} break;
			case GDScriptTokenizer::TK_PR_VAR: {
				// variable declaration and (eventual) initialization

				ClassNode::Member member;

				bool autoexport = tokenizer->get_token(-1) == GDScriptTokenizer::TK_PR_EXPORT;
				if (current_export.type != Variant::NIL) {
					member._export = current_export;
					current_export = PropertyInfo();
				}

				bool onready = tokenizer->get_token(-1) == GDScriptTokenizer::TK_PR_ONREADY;

				tokenizer->advance();
				if (!tokenizer->is_token_literal(0, true)) {
					_set_error("Expected an identifier for the member variable name.");
					return;
				}

				member.identifier = tokenizer->get_token_literal();
				member.expression = nullptr;
				member._export.name = member.identifier;
				member.line = tokenizer->get_token_line();
				member.usages = 0;
				member.rpc_mode = rpc_mode;

				// GH-57496
				if (ClassDB::class_exists(member.identifier) || ClassDB::class_exists("_" + member.identifier.operator String())) {
					_set_error("Variable \"" + String(member.identifier) + "\" shadows a native class.");
					return;
				}

				if (current_class->constant_expressions.has(member.identifier)) {
					_set_error("A constant named \"" + String(member.identifier) + "\" already exists in this class (at line: " +
							itos(current_class->constant_expressions[member.identifier].expression->line) + ").");
					return;
				}

				for (int i = 0; i < current_class->variables.size(); i++) {
					if (current_class->variables[i].identifier == member.identifier) {
						_set_error("Variable \"" + String(member.identifier) + "\" already exists in this class (at line: " +
								itos(current_class->variables[i].line) + ").");
						return;
					}
				}

				for (int i = 0; i < current_class->subclasses.size(); i++) {
					if (current_class->subclasses[i]->name == member.identifier) {
						_set_error("A class named \"" + String(member.identifier) + "\" already exists in this class (at line " + itos(current_class->subclasses[i]->line) + ").");
						return;
					}
				}
#ifdef DEBUG_ENABLED
				for (int i = 0; i < current_class->functions.size(); i++) {
					if (current_class->functions[i]->name == member.identifier) {
						_add_warning(GDScriptWarning::VARIABLE_CONFLICTS_FUNCTION, member.line, member.identifier);
						break;
					}
				}
				for (int i = 0; i < current_class->static_functions.size(); i++) {
					if (current_class->static_functions[i]->name == member.identifier) {
						_add_warning(GDScriptWarning::VARIABLE_CONFLICTS_FUNCTION, member.line, member.identifier);
						break;
					}
				}
#endif // DEBUG_ENABLED
				tokenizer->advance();

				rpc_mode = MultiplayerAPI::RPC_MODE_DISABLED;

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
					if (tokenizer->get_token(1) == GDScriptTokenizer::TK_OP_ASSIGN) {
						member.data_type = DataType();
						member.data_type.infer_type = true;
						tokenizer->advance();
					} else if (!_parse_type(member.data_type)) {
						_set_error("Expected a type for the class variable.");
						return;
					}
				}

				if (autoexport && member.data_type.has_type) {
					if (member.data_type.kind == DataType::BUILTIN) {
						member._export.type = member.data_type.builtin_type;
						member._export.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
					} else if (member.data_type.kind == DataType::NATIVE) {
						if (ClassDB::is_parent_class(member.data_type.native_type, "Resource")) {
							member._export.type = Variant::OBJECT;
							member._export.hint = PROPERTY_HINT_RESOURCE_TYPE;
							member._export.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
							member._export.hint_string = member.data_type.native_type;
							member._export.class_name = member.data_type.native_type;
						} else {
							_set_error("Invalid export type. Only built-in and native resource types can be exported.", member.line);
							return;
						}

					} else {
						_set_error("Invalid export type. Only built-in and native resource types can be exported.", member.line);
						return;
					}
				}

#ifdef TOOLS_ENABLED
				Variant::CallError ce;
				member.default_value = Variant::construct(member._export.type, nullptr, 0, ce);
#endif

				if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_ASSIGN) {
#ifdef DEBUG_ENABLED
					int line = tokenizer->get_token_line();
#endif
					tokenizer->advance();

					Node *subexpr = _parse_and_reduce_expression(p_class, false, autoexport || member._export.type != Variant::NIL);
					if (!subexpr) {
						if (_recover_from_completion()) {
							break;
						}
						return;
					}

					//discourage common error
					if (!onready && subexpr->type == Node::TYPE_OPERATOR) {
						OperatorNode *op = static_cast<OperatorNode *>(subexpr);
						if (op->op == OperatorNode::OP_CALL && op->arguments[0]->type == Node::TYPE_SELF && op->arguments[1]->type == Node::TYPE_IDENTIFIER) {
							IdentifierNode *id = static_cast<IdentifierNode *>(op->arguments[1]);
							if (id->name == "get_node") {
								_set_error("Use \"onready var " + String(member.identifier) + " = get_node(...)\" instead.");
								return;
							}
						}
					}

					member.expression = subexpr;

					if (autoexport && !member.data_type.has_type) {
						if (subexpr->type != Node::TYPE_CONSTANT) {
							_set_error("Type-less export needs a constant expression assigned to infer type.");
							return;
						}

						ConstantNode *cn = static_cast<ConstantNode *>(subexpr);
						if (cn->value.get_type() == Variant::NIL) {
							_set_error("Can't accept a null constant expression for inferring export type.");
							return;
						}

						if (!_reduce_export_var_type(cn->value, member.line)) {
							return;
						}

						member._export.type = cn->value.get_type();
						member._export.usage |= PROPERTY_USAGE_SCRIPT_VARIABLE;
						if (cn->value.get_type() == Variant::OBJECT) {
							Object *obj = cn->value;
							Resource *res = Object::cast_to<Resource>(obj);
							if (res == nullptr) {
								_set_error("The exported constant isn't a type or resource.");
								return;
							}
							member._export.hint = PROPERTY_HINT_RESOURCE_TYPE;
							member._export.hint_string = res->get_class();
						}
					}
#ifdef TOOLS_ENABLED
					// Warn if the default value set is not the same as the export type, since it won't be coerced and
					// may create wrong expectations.
					if (subexpr->type == Node::TYPE_CONSTANT && (member._export.type != Variant::NIL || member.data_type.has_type)) {
						ConstantNode *cn = static_cast<ConstantNode *>(subexpr);
						if (cn->value.get_type() != Variant::NIL) {
							if (member._export.type != Variant::NIL && cn->value.get_type() != member._export.type) {
								if (!Variant::can_convert(cn->value.get_type(), member._export.type)) {
									_set_error("Can't convert the provided value to the export type.");
									return;
								} else if (!member.data_type.has_type) {
									_add_warning(GDScriptWarning::EXPORT_HINT_TYPE_MISTMATCH, member.line, Variant::get_type_name(cn->value.get_type()), Variant::get_type_name(member._export.type));
								}
							}
						}
						member.default_value = cn->value;
					}
#endif

					IdentifierNode *id = alloc_node<IdentifierNode>();
					id->name = member.identifier;
					id->datatype = member.data_type;

					OperatorNode *op = alloc_node<OperatorNode>();
					op->op = OperatorNode::OP_INIT_ASSIGN;
					op->arguments.push_back(id);
					op->arguments.push_back(subexpr);

#ifdef DEBUG_ENABLED
					NewLineNode *nl2 = alloc_node<NewLineNode>();
					nl2->line = line;
					if (onready) {
						p_class->ready->statements.push_back(nl2);
					} else {
						p_class->initializer->statements.push_back(nl2);
					}
#endif
					if (onready) {
						p_class->ready->statements.push_back(op);
					} else {
						p_class->initializer->statements.push_back(op);
					}

					member.initial_assignment = op;

				} else {
					if (autoexport && !member.data_type.has_type) {
						_set_error("Type-less export needs a constant expression assigned to infer type.");
						return;
					}

					Node *expr;

					if (member.data_type.has_type) {
						expr = _get_default_value_for_type(member.data_type);
					} else {
						DataType exported_type;
						exported_type.has_type = true;
						exported_type.kind = DataType::BUILTIN;
						exported_type.builtin_type = member._export.type;
						expr = _get_default_value_for_type(exported_type);
					}

					IdentifierNode *id = alloc_node<IdentifierNode>();
					id->name = member.identifier;
					id->datatype = member.data_type;

					OperatorNode *op = alloc_node<OperatorNode>();
					op->op = OperatorNode::OP_INIT_ASSIGN;
					op->arguments.push_back(id);
					op->arguments.push_back(expr);

					p_class->initializer->statements.push_back(op);

					member.initial_assignment = op;
				}

				if (tokenizer->get_token() == GDScriptTokenizer::TK_PR_SETGET) {
					tokenizer->advance();

					if (tokenizer->get_token() != GDScriptTokenizer::TK_COMMA) {
						//just comma means using only getter
						if (!tokenizer->is_token_literal()) {
							_set_error("Expected an identifier for the setter function after \"setget\".");
						}

						member.setter = tokenizer->get_token_literal();

						tokenizer->advance();
					}

					if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
						//there is a getter
						tokenizer->advance();

						if (!tokenizer->is_token_literal()) {
							_set_error("Expected an identifier for the getter function after \",\".");
						}

						member.getter = tokenizer->get_token_literal();
						tokenizer->advance();
					}
				}

				p_class->variables.push_back(member);

				if (!_end_statement()) {
					_set_end_statement_error("var");
					return;
				}
			} break;
			case GDScriptTokenizer::TK_PR_CONST: {
				// constant declaration and initialization

				ClassNode::Constant constant;

				tokenizer->advance();
				if (!tokenizer->is_token_literal(0, true)) {
					_set_error("Expected an identifier for the constant.");
					return;
				}

				StringName const_id = tokenizer->get_token_literal();
				int line = tokenizer->get_token_line();

				// GH-57496
				if (ClassDB::class_exists(const_id) || ClassDB::class_exists("_" + const_id.operator String())) {
					_set_error("Constant \"" + String(const_id) + "\" shadows a native class.");
					return;
				}

				if (current_class->constant_expressions.has(const_id)) {
					_set_error("Constant \"" + String(const_id) + "\" already exists in this class (at line " +
							itos(current_class->constant_expressions[const_id].expression->line) + ").");
					return;
				}

				for (int i = 0; i < current_class->variables.size(); i++) {
					if (current_class->variables[i].identifier == const_id) {
						_set_error("A variable named \"" + String(const_id) + "\" already exists in this class (at line " +
								itos(current_class->variables[i].line) + ").");
						return;
					}
				}

				for (int i = 0; i < current_class->subclasses.size(); i++) {
					if (current_class->subclasses[i]->name == const_id) {
						_set_error("A class named \"" + String(const_id) + "\" already exists in this class (at line " + itos(current_class->subclasses[i]->line) + ").");
						return;
					}
				}

				tokenizer->advance();

				if (tokenizer->get_token() == GDScriptTokenizer::TK_COLON) {
					if (tokenizer->get_token(1) == GDScriptTokenizer::TK_OP_ASSIGN) {
						constant.type = DataType();
						constant.type.infer_type = true;
						tokenizer->advance();
					} else if (!_parse_type(constant.type)) {
						_set_error("Expected a type for the class constant.");
						return;
					}
				}

				if (tokenizer->get_token() != GDScriptTokenizer::TK_OP_ASSIGN) {
					_set_error("Constants must be assigned immediately.");
					return;
				}

				tokenizer->advance();

				Node *subexpr = _parse_and_reduce_expression(p_class, true, true);
				if (!subexpr) {
					if (_recover_from_completion()) {
						break;
					}
					return;
				}

				if (subexpr->type != Node::TYPE_CONSTANT) {
					_set_error("Expected a constant expression.", line);
					return;
				}
				subexpr->line = line;
				constant.expression = subexpr;

				p_class->constant_expressions.insert(const_id, constant);

				if (!_end_statement()) {
					_set_end_statement_error("const");
					return;
				}

			} break;
			case GDScriptTokenizer::TK_PR_ENUM: {
				//multiple constant declarations..

				int last_assign = -1; // Incremented by 1 right before the assignment.
				String enum_name;
				Dictionary enum_dict;
				int enum_start_line = tokenizer->get_token_line();

				tokenizer->advance();
				if (tokenizer->is_token_literal(0, true)) {
					enum_name = tokenizer->get_token_literal();

					// GH-57496
					if (ClassDB::class_exists(enum_name) || ClassDB::class_exists("_" + enum_name)) {
						_set_error("Enumeration \"" + enum_name + "\" shadows a native class.");
						return;
					}

					if (current_class->constant_expressions.has(enum_name)) {
						_set_error("A constant named \"" + String(enum_name) + "\" already exists in this class (at line " +
								itos(current_class->constant_expressions[enum_name].expression->line) + ").");
						return;
					}

					for (int i = 0; i < current_class->variables.size(); i++) {
						if (current_class->variables[i].identifier == enum_name) {
							_set_error("A variable named \"" + String(enum_name) + "\" already exists in this class (at line " +
									itos(current_class->variables[i].line) + ").");
							return;
						}
					}

					for (int i = 0; i < current_class->subclasses.size(); i++) {
						if (current_class->subclasses[i]->name == enum_name) {
							_set_error("A class named \"" + String(enum_name) + "\" already exists in this class (at line " + itos(current_class->subclasses[i]->line) + ").");
							return;
						}
					}

					tokenizer->advance();
				}
				if (tokenizer->get_token() != GDScriptTokenizer::TK_CURLY_BRACKET_OPEN) {
					_set_error("Expected \"{\" in the enum declaration.");
					return;
				}
				tokenizer->advance();

				while (true) {
					if (tokenizer->get_token() == GDScriptTokenizer::TK_NEWLINE) {
						tokenizer->advance(); // Ignore newlines
					} else if (tokenizer->get_token() == GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE) {
						tokenizer->advance();
						break; // End of enum
					} else if (!tokenizer->is_token_literal(0, true)) {
						if (tokenizer->get_token() == GDScriptTokenizer::TK_EOF) {
							_set_error("Unexpected end of file.");
						} else {
							_set_error(String("Unexpected ") + GDScriptTokenizer::get_token_name(tokenizer->get_token()) + ", expected an identifier.");
						}

						return;
					} else { // tokenizer->is_token_literal(0, true)
						StringName const_id = tokenizer->get_token_literal();

						tokenizer->advance();

						ConstantNode *enum_value_expr;

						if (tokenizer->get_token() == GDScriptTokenizer::TK_OP_ASSIGN) {
							tokenizer->advance();

							Node *subexpr = _parse_and_reduce_expression(p_class, true, true);
							if (!subexpr) {
								if (_recover_from_completion()) {
									break;
								}
								return;
							}

							if (subexpr->type != Node::TYPE_CONSTANT) {
								_set_error("Expected a constant expression.");
								return;
							}

							enum_value_expr = static_cast<ConstantNode *>(subexpr);

							if (enum_value_expr->value.get_type() != Variant::INT) {
								_set_error("Expected an integer value for \"enum\".");
								return;
							}

							last_assign = enum_value_expr->value;

						} else {
							last_assign = last_assign + 1;
							enum_value_expr = alloc_node<ConstantNode>();
							enum_value_expr->value = last_assign;
							enum_value_expr->datatype = _type_from_variant(enum_value_expr->value);
						}

						if (tokenizer->get_token() == GDScriptTokenizer::TK_COMMA) {
							tokenizer->advance();
						} else if (tokenizer->is_token_literal(0, true)) {
							_set_error("Unexpected identifier.");
							return;
						}

						if (enum_name != "") {
							enum_dict[const_id] = enum_value_expr->value;
						} else {
							// GH-57496
							if (ClassDB::class_exists(const_id) || ClassDB::class_exists("_" + const_id.operator String())) {
								_set_error("Constant \"" + String(const_id) + "\" shadows a native class.");
								return;
							}

							if (current_class->constant_expressions.has(const_id)) {
								_set_error("A constant named \"" + String(const_id) + "\" already exists in this class (at line " +
										itos(current_class->constant_expressions[const_id].expression->line) + ").");
								return;
							}

							for (int i = 0; i < current_class->variables.size(); i++) {
								if (current_class->variables[i].identifier == const_id) {
									_set_error("A variable named \"" + String(const_id) + "\" already exists in this class (at line " +
											itos(current_class->variables[i].line) + ").");
									return;
								}
							}

							for (int i = 0; i < current_class->subclasses.size(); i++) {
								if (current_class->subclasses[i]->name == const_id) {
									_set_error("A class named \"" + String(const_id) + "\" already exists in this class (at line " + itos(current_class->subclasses[i]->line) + ").");
									return;
								}
							}

							ClassNode::Constant constant;
							constant.type.has_type = true;
							constant.type.kind = DataType::BUILTIN;
							constant.type.builtin_type = Variant::INT;
							constant.expression = enum_value_expr;
							p_class->constant_expressions.insert(const_id, constant);
						}
					}
				}

				if (enum_name != "") {
					ClassNode::Constant enum_constant;
					ConstantNode *cn = alloc_node<ConstantNode>();
					cn->value = enum_dict;
					cn->datatype = _type_from_variant(cn->value);
					cn->line = enum_start_line;

					enum_constant.expression = cn;
					enum_constant.type = cn->datatype;
					p_class->constant_expressions.insert(enum_name, enum_constant);
				}

				if (!_end_statement()) {
					_set_end_statement_error("enum");
					return;
				}

			} break;

			case GDScriptTokenizer::TK_CONSTANT: {
				if (tokenizer->get_token_constant().get_type() == Variant::STRING) {
					tokenizer->advance();
					// Ignore
				} else {
					_set_error(String() + "Unexpected constant of type: " + Variant::get_type_name(tokenizer->get_token_constant().get_type()));
					return;
				}
			} break;

			case GDScriptTokenizer::TK_CF_PASS: {
				tokenizer->advance();
			} break;

			default: {
				if (token == GDScriptTokenizer::TK_IDENTIFIER) {
					completion_type = COMPLETION_IDENTIFIER;
					completion_class = current_class;
					completion_function = current_function;
					completion_line = tokenizer->get_token_line();
					completion_block = current_block;
					completion_ident_is_call = false;
					completion_found = true;
				}

				_set_error(String() + "Unexpected token: " + tokenizer->get_token_name(tokenizer->get_token()) + ":" + tokenizer->get_token_identifier());
				return;

			} break;
		}
	}
}

void GDScriptParser::_determine_inheritance(ClassNode *p_class, bool p_recursive) {
	if (p_class->base_type.has_type) {
		// Already determined
	} else if (p_class->extends_used) {
		//do inheritance
		String path = p_class->extends_file;

		Ref<GDScript> script;
		StringName native;
		ClassNode *base_class = nullptr;

		if (path != "") {
			//path (and optionally subclasses)

			if (path.is_rel_path()) {
				String base = base_path;

				if (base == "" || base.is_rel_path()) {
					_set_error("Couldn't resolve relative path for the parent class: " + path, p_class->line);
					return;
				}
				path = base.plus_file(path).simplify_path();
			}
			script = ResourceLoader::load(path);
			if (script.is_null()) {
				_set_error("Couldn't load the base class: " + path, p_class->line);
				return;
			}
			if (!script->is_valid()) {
				_set_error("Script isn't fully loaded (cyclic preload?): " + path, p_class->line);
				return;
			}

			if (p_class->extends_class.size()) {
				for (int i = 0; i < p_class->extends_class.size(); i++) {
					String sub = p_class->extends_class[i];
					if (script->get_subclasses().has(sub)) {
						Ref<Script> subclass = script->get_subclasses()[sub]; //avoid reference from disappearing
						script = subclass;
					} else {
						_set_error("Couldn't find the subclass: " + sub, p_class->line);
						return;
					}
				}
			}

		} else {
			if (p_class->extends_class.size() == 0) {
				_set_error("Parser bug: undecidable inheritance.", p_class->line);
				ERR_FAIL();
			}
			//look around for the subclasses

			int extend_iter = 1;
			String base = p_class->extends_class[0];
			ClassNode *p = p_class->owner;
			Ref<GDScript> base_script;

			if (ScriptServer::is_global_class(base)) {
				base_script = ResourceLoader::load(ScriptServer::get_global_class_path(base));
				if (!base_script.is_valid()) {
					_set_error("The class \"" + base + "\" couldn't be fully loaded (script error or cyclic dependency).", p_class->line);
					return;
				}
				p = nullptr;
			} else {
				String autoload_path = _lookup_autoload_path_for_identifier(base);
				if (!autoload_path.empty()) {
					base_script = ResourceLoader::load(autoload_path);
					if (!base_script.is_valid()) {
						_set_error("Class '" + base + "' could not be fully loaded (script error or cyclic inheritance).", p_class->line);
						return;
					}
					p = nullptr;
				}
			}

			while (p) {
				bool found = false;

				for (int i = 0; i < p->subclasses.size(); i++) {
					if (p->subclasses[i]->name == base) {
						ClassNode *test = p->subclasses[i];
						while (test) {
							if (test == p_class) {
								_set_error("Cyclic inheritance.", test->line);
								return;
							}
							if (test->base_type.kind == DataType::CLASS) {
								test = test->base_type.class_type;
							} else {
								break;
							}
						}
						found = true;
						if (extend_iter < p_class->extends_class.size()) {
							// Keep looking at current classes if possible
							base = p_class->extends_class[extend_iter++];
							p = p->subclasses[i];
						} else {
							base_class = p->subclasses[i];
						}
						break;
					}
				}

				if (base_class) {
					break;
				}
				if (found) {
					continue;
				}

				if (p->constant_expressions.has(base)) {
					if (p->constant_expressions[base].expression->type != Node::TYPE_CONSTANT) {
						_set_error("Couldn't resolve the constant \"" + base + "\".", p_class->line);
						return;
					}
					const ConstantNode *cn = static_cast<const ConstantNode *>(p->constant_expressions[base].expression);
					base_script = cn->value;
					if (base_script.is_null()) {
						_set_error("Constant isn't a class: " + base, p_class->line);
						return;
					}
					break;
				}

				p = p->owner;
			}

			if (base_script.is_valid()) {
				String ident = base;
				Ref<GDScript> find_subclass = base_script;

				for (int i = extend_iter; i < p_class->extends_class.size(); i++) {
					String subclass = p_class->extends_class[i];

					ident += ("." + subclass);

					if (find_subclass->get_subclasses().has(subclass)) {
						find_subclass = find_subclass->get_subclasses()[subclass];
					} else if (find_subclass->get_constants().has(subclass)) {
						Ref<GDScript> new_base_class = find_subclass->get_constants()[subclass];
						if (new_base_class.is_null()) {
							_set_error("Constant isn't a class: " + ident, p_class->line);
							return;
						}
						find_subclass = new_base_class;
					} else {
						_set_error("Couldn't find the subclass: " + ident, p_class->line);
						return;
					}
				}

				script = find_subclass;

			} else if (!base_class) {
				if (p_class->extends_class.size() > 1) {
					_set_error("Invalid inheritance (unknown class + subclasses).", p_class->line);
					return;
				}
				//if not found, try engine classes
				if (!GDScriptLanguage::get_singleton()->get_global_map().has(base)) {
					_set_error("Unknown class: \"" + base + "\"", p_class->line);
					return;
				}

				native = base;
			}
		}

		if (base_class) {
			p_class->base_type.has_type = true;
			p_class->base_type.kind = DataType::CLASS;
			p_class->base_type.class_type = base_class;
		} else if (script.is_valid()) {
			p_class->base_type.has_type = true;
			p_class->base_type.kind = DataType::GDSCRIPT;
			p_class->base_type.script_type = script;
			p_class->base_type.native_type = script->get_instance_base_type();
		} else if (native != StringName()) {
			p_class->base_type.has_type = true;
			p_class->base_type.kind = DataType::NATIVE;
			p_class->base_type.native_type = native;
		} else {
			_set_error("Couldn't determine inheritance.", p_class->line);
			return;
		}

	} else {
		// without extends, implicitly extend Reference
		p_class->base_type.has_type = true;
		p_class->base_type.kind = DataType::NATIVE;
		p_class->base_type.native_type = "Reference";
	}

	if (p_recursive) {
		// Recursively determine subclasses
		for (int i = 0; i < p_class->subclasses.size(); i++) {
			_determine_inheritance(p_class->subclasses[i], p_recursive);
		}
	}
}

String GDScriptParser::DataType::to_string() const {
	if (!has_type) {
		return "var";
	}
	switch (kind) {
		case BUILTIN: {
			if (builtin_type == Variant::NIL) {
				return "null";
			}
			return Variant::get_type_name(builtin_type);
		} break;
		case NATIVE: {
			if (is_meta_type) {
				return "GDScriptNativeClass";
			}
			return native_type.operator String();
		} break;

		case GDSCRIPT: {
			Ref<GDScript> gds = script_type;
			const String &gds_class = gds->get_script_class_name();
			if (!gds_class.empty()) {
				return gds_class;
			}
			FALLTHROUGH;
		}
		case SCRIPT: {
			if (is_meta_type) {
				return script_type->get_class_name().operator String();
			}
			String name = script_type->get_name();
			if (name != String()) {
				return name;
			}
			name = script_type->get_path().get_file();
			if (name != String()) {
				return name;
			}
			return native_type.operator String();
		} break;
		case CLASS: {
			ERR_FAIL_COND_V(!class_type, String());
			if (is_meta_type) {
				return "GDScript";
			}
			if (class_type->name == StringName()) {
				return "self";
			}
			return class_type->name.operator String();
		} break;
		case UNRESOLVED: {
		} break;
	}

	return "Unresolved";
}

bool GDScriptParser::_parse_type(DataType &r_type, bool p_can_be_void) {
	tokenizer->advance();
	r_type.has_type = true;

	bool finished = false;
	bool can_index = false;
	String full_name;

	if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
		completion_cursor = StringName();
		completion_type = COMPLETION_TYPE_HINT;
		completion_class = current_class;
		completion_function = current_function;
		completion_line = tokenizer->get_token_line();
		completion_argument = 0;
		completion_block = current_block;
		completion_found = true;
		completion_ident_is_call = p_can_be_void;
		tokenizer->advance();
	}

	switch (tokenizer->get_token()) {
		case GDScriptTokenizer::TK_PR_VOID: {
			if (!p_can_be_void) {
				return false;
			}
			r_type.kind = DataType::BUILTIN;
			r_type.builtin_type = Variant::NIL;
		} break;
		case GDScriptTokenizer::TK_BUILT_IN_TYPE: {
			r_type.builtin_type = tokenizer->get_token_type();
			if (tokenizer->get_token_type() == Variant::OBJECT) {
				r_type.kind = DataType::NATIVE;
				r_type.native_type = "Object";
			} else {
				r_type.kind = DataType::BUILTIN;
			}
		} break;
		case GDScriptTokenizer::TK_IDENTIFIER: {
			r_type.native_type = tokenizer->get_token_identifier();
			if (ClassDB::class_exists(r_type.native_type) || ClassDB::class_exists("_" + r_type.native_type.operator String())) {
				r_type.kind = DataType::NATIVE;
			} else {
				r_type.kind = DataType::UNRESOLVED;
				can_index = true;
				full_name = r_type.native_type;
			}
		} break;
		default: {
			return false;
		}
	}

	tokenizer->advance();

	if (tokenizer->get_token() == GDScriptTokenizer::TK_CURSOR) {
		completion_cursor = r_type.native_type;
		completion_type = COMPLETION_TYPE_HINT;
		completion_class = current_class;
		completion_function = current_function;
		completion_line = tokenizer->get_token_line();
		completion_argument = 0;
		completion_block = current_block;
		completion_found = true;
		completion_ident_is_call = p_can_be_void;
		tokenizer->advance();
	}

	if (can_index) {
		while (!finished) {
			switch (tokenizer->get_token()) {
				case GDScriptTokenizer::TK_PERIOD: {
					if (!can_index) {
						_set_error("Unexpected \".\".");
						return false;
					}
					can_index = false;
					tokenizer->advance();
				} break;
				case GDScriptTokenizer::TK_CURSOR:
				case GDScriptTokenizer::TK_IDENTIFIER: {
					if (can_index) {
						_set_error("Unexpected identifier.");
						return false;
					}

					StringName id;
					bool has_completion = _get_completable_identifier(COMPLETION_TYPE_HINT_INDEX, id);
					if (id == StringName()) {
						id = "@temp";
					}

					full_name += "." + id.operator String();
					can_index = true;
					if (has_completion) {
						completion_cursor = full_name;
					}
				} break;
				default: {
					finished = true;
				} break;
			}
		}

		if (tokenizer->get_token(-1) == GDScriptTokenizer::TK_PERIOD) {
			_set_error("Expected a subclass identifier.");
			return false;
		}

		r_type.native_type = full_name;
	}

	return true;
}

GDScriptParser::DataType GDScriptParser::_resolve_type(const DataType &p_source, int p_line) {
	if (!p_source.has_type) {
		return p_source;
	}
	if (p_source.kind != DataType::UNRESOLVED) {
		return p_source;
	}

	Vector<String> full_name = p_source.native_type.operator String().split(".", false);
	int name_part = 0;

	DataType result;
	result.has_type = true;

	while (name_part < full_name.size()) {
		bool found = false;
		StringName id = full_name[name_part];
		DataType base_type = result;

		ClassNode *p = nullptr;
		if (name_part == 0) {
			if (ScriptServer::is_global_class(id)) {
				String script_path = ScriptServer::get_global_class_path(id);
				if (script_path == self_path) {
					result.kind = DataType::CLASS;
					result.class_type = static_cast<ClassNode *>(head);
				} else {
					Ref<Script> script = ResourceLoader::load(script_path);
					Ref<GDScript> gds = script;
					if (gds.is_valid()) {
						if (!gds->is_valid()) {
							_set_error("The class \"" + id + "\" couldn't be fully loaded (script error or cyclic dependency).", p_line);
							return DataType();
						}
						result.kind = DataType::GDSCRIPT;
						result.script_type = gds;
					} else if (script.is_valid()) {
						result.kind = DataType::SCRIPT;
						result.script_type = script;
					} else {
						_set_error("The class \"" + id + "\" was found in global scope, but its script couldn't be loaded.", p_line);
						return DataType();
					}
				}
				name_part++;
				continue;
			}

			String autoload_path = _lookup_autoload_path_for_identifier(id);
			if (!autoload_path.empty()) {
				Ref<Script> script = ResourceLoader::load(autoload_path);
				Ref<GDScript> gds = script;
				if (gds.is_valid()) {
					if (!gds->is_valid()) {
						_set_error("Class '" + id + "' could not be fully loaded (script error or cyclic inheritance).", p_line);
						return DataType();
					}
					result.kind = DataType::GDSCRIPT;
					result.script_type = gds;
				} else if (script.is_valid()) {
					result.kind = DataType::SCRIPT;
					result.script_type = script;
				} else {
					_set_error("Couldn't fully load singleton script '" + id + "' (possible cyclic reference or parse error).", p_line);
					return DataType();
				}
				name_part++;
				continue;
			}

			p = current_class;
		} else if (base_type.kind == DataType::CLASS) {
			p = base_type.class_type;
		}
		while (p) {
			if (p->constant_expressions.has(id)) {
				if (p->constant_expressions[id].expression->type != Node::TYPE_CONSTANT) {
					_set_error("Parser bug: unresolved constant.", p_line);
					ERR_FAIL_V(result);
				}
				const ConstantNode *cn = static_cast<const ConstantNode *>(p->constant_expressions[id].expression);
				Ref<GDScript> gds = cn->value;
				if (gds.is_valid()) {
					result.kind = DataType::GDSCRIPT;
					result.script_type = gds;
					found = true;
				} else {
					Ref<Script> scr = cn->value;
					if (scr.is_valid()) {
						result.kind = DataType::SCRIPT;
						result.script_type = scr;
						found = true;
					}
				}
				break;
			}

			// Inner classes
			ClassNode *outer_class = p;
			while (outer_class) {
				if (outer_class->name == id) {
					found = true;
					result.kind = DataType::CLASS;
					result.class_type = outer_class;
					break;
				}
				for (int i = 0; i < outer_class->subclasses.size(); i++) {
					if (outer_class->subclasses[i] == p) {
						continue;
					}
					if (outer_class->subclasses[i]->name == id) {
						found = true;
						result.kind = DataType::CLASS;
						result.class_type = outer_class->subclasses[i];
						break;
					}
				}
				if (found) {
					break;
				}
				outer_class = outer_class->owner;
			}

			if (!found && p->base_type.kind == DataType::CLASS) {
				p = p->base_type.class_type;
			} else {
				base_type = p->base_type;
				break;
			}
		}

		// Still look for class constants in parent scripts
		if (!found && (base_type.kind == DataType::GDSCRIPT || base_type.kind == DataType::SCRIPT)) {
			Ref<Script> scr = base_type.script_type;
			ERR_FAIL_COND_V(scr.is_null(), result);
			while (scr.is_valid()) {
				Map<StringName, Variant> constants;
				scr->get_constants(&constants);

				if (constants.has(id)) {
					Ref<GDScript> gds = constants[id];

					if (gds.is_valid()) {
						result.kind = DataType::GDSCRIPT;
						result.script_type = gds;
						found = true;
					} else {
						Ref<Script> scr2 = constants[id];
						if (scr2.is_valid()) {
							result.kind = DataType::SCRIPT;
							result.script_type = scr2;
							found = true;
						}
					}
				}
				if (found) {
					break;
				} else {
					scr = scr->get_base_script();
				}
			}
		}

		if (!found && !for_completion) {
			String base;
			if (name_part == 0) {
				base = "self";
			} else {
				base = result.to_string();
			}
			_set_error("The identifier \"" + String(id) +
							"\" isn't a valid type (not a script or class), or couldn't be found on base \"" + base + "\".",
					p_line);
			return DataType();
		}

		name_part++;
	}

	return result;
}

GDScriptParser::DataType GDScriptParser::_type_from_variant(const Variant &p_value) const {
	DataType result;
	result.has_type = true;
	result.is_constant = true;
	result.kind = DataType::BUILTIN;
	result.builtin_type = p_value.get_type();

	if (result.builtin_type == Variant::OBJECT) {
		Object *obj = p_value.operator Object *();
		if (!obj) {
			return DataType();
		}
		result.native_type = obj->get_class_name();
		Ref<Script> scr = p_value;
		if (scr.is_valid()) {
			result.is_meta_type = true;
		} else {
			result.is_meta_type = false;
			scr = obj->get_script();
		}
		if (scr.is_valid()) {
			result.script_type = scr;
			Ref<GDScript> gds = scr;
			if (gds.is_valid()) {
				result.kind = DataType::GDSCRIPT;
			} else {
				result.kind = DataType::SCRIPT;
			}
			result.native_type = scr->get_instance_base_type();
		} else {
			result.kind = DataType::NATIVE;
		}
	}

	return result;
}

GDScriptParser::DataType GDScriptParser::_type_from_property(const PropertyInfo &p_property, bool p_nil_is_variant) const {
	DataType ret;
	if (p_property.type == Variant::NIL && (p_nil_is_variant || (p_property.usage & PROPERTY_USAGE_NIL_IS_VARIANT))) {
		// Variant
		return ret;
	}
	ret.has_type = true;
	ret.builtin_type = p_property.type;
	if (p_property.type == Variant::OBJECT) {
		ret.kind = DataType::NATIVE;
		ret.native_type = p_property.class_name == StringName() ? "Object" : p_property.class_name;
	} else {
		ret.kind = DataType::BUILTIN;
	}
	return ret;
}

GDScriptParser::DataType GDScriptParser::_type_from_gdtype(const GDScriptDataType &p_gdtype) const {
	DataType result;
	if (!p_gdtype.has_type) {
		return result;
	}

	result.has_type = true;
	result.builtin_type = p_gdtype.builtin_type;
	result.native_type = p_gdtype.native_type;
	result.script_type = Ref<Script>(p_gdtype.script_type);

	switch (p_gdtype.kind) {
		case GDScriptDataType::UNINITIALIZED: {
			ERR_PRINT("Uninitialized datatype. Please report a bug.");
		} break;
		case GDScriptDataType::BUILTIN: {
			result.kind = DataType::BUILTIN;
		} break;
		case GDScriptDataType::NATIVE: {
			result.kind = DataType::NATIVE;
		} break;
		case GDScriptDataType::GDSCRIPT: {
			result.kind = DataType::GDSCRIPT;
		} break;
		case GDScriptDataType::SCRIPT: {
			result.kind = DataType::SCRIPT;
		} break;
	}
	return result;
}

GDScriptParser::DataType GDScriptParser::_get_operation_type(const Variant::Operator p_op, const DataType &p_a, const DataType &p_b, bool &r_valid) const {
	if (!p_a.has_type || !p_b.has_type) {
		r_valid = true;
		return DataType();
	}

	Variant::Type a_type = p_a.kind == DataType::BUILTIN ? p_a.builtin_type : Variant::OBJECT;
	Variant::Type b_type = p_b.kind == DataType::BUILTIN ? p_b.builtin_type : Variant::OBJECT;

	Variant a;
	REF a_ref;
	if (a_type == Variant::OBJECT) {
		a_ref.instance();
		a = a_ref;
	} else {
		Variant::CallError err;
		a = Variant::construct(a_type, nullptr, 0, err);
		if (err.error != Variant::CallError::CALL_OK) {
			r_valid = false;
			return DataType();
		}
	}
	Variant b;
	REF b_ref;
	if (b_type == Variant::OBJECT) {
		b_ref.instance();
		b = b_ref;
	} else {
		Variant::CallError err;
		b = Variant::construct(b_type, nullptr, 0, err);
		if (err.error != Variant::CallError::CALL_OK) {
			r_valid = false;
			return DataType();
		}
	}

	// Avoid division by zero
	if (a_type == Variant::INT || a_type == Variant::REAL) {
		Variant::evaluate(Variant::OP_ADD, a, 1, a, r_valid);
	}
	if (b_type == Variant::INT || b_type == Variant::REAL) {
		Variant::evaluate(Variant::OP_ADD, b, 1, b, r_valid);
	}
	if (a_type == Variant::STRING && b_type != Variant::ARRAY) {
		a = "%s"; // Work around for formatting operator (%)
	}

	Variant ret;
	Variant::evaluate(p_op, a, b, ret, r_valid);

	if (r_valid) {
		return _type_from_variant(ret);
	}

	return DataType();
}

Variant::Operator GDScriptParser::_get_variant_operation(const OperatorNode::Operator &p_op) const {
	switch (p_op) {
		case OperatorNode::OP_NEG: {
			return Variant::OP_NEGATE;
		} break;
		case OperatorNode::OP_POS: {
			return Variant::OP_POSITIVE;
		} break;
		case OperatorNode::OP_NOT: {
			return Variant::OP_NOT;
		} break;
		case OperatorNode::OP_BIT_INVERT: {
			return Variant::OP_BIT_NEGATE;
		} break;
		case OperatorNode::OP_IN: {
			return Variant::OP_IN;
		} break;
		case OperatorNode::OP_EQUAL: {
			return Variant::OP_EQUAL;
		} break;
		case OperatorNode::OP_NOT_EQUAL: {
			return Variant::OP_NOT_EQUAL;
		} break;
		case OperatorNode::OP_LESS: {
			return Variant::OP_LESS;
		} break;
		case OperatorNode::OP_LESS_EQUAL: {
			return Variant::OP_LESS_EQUAL;
		} break;
		case OperatorNode::OP_GREATER: {
			return Variant::OP_GREATER;
		} break;
		case OperatorNode::OP_GREATER_EQUAL: {
			return Variant::OP_GREATER_EQUAL;
		} break;
		case OperatorNode::OP_AND: {
			return Variant::OP_AND;
		} break;
		case OperatorNode::OP_OR: {
			return Variant::OP_OR;
		} break;
		case OperatorNode::OP_ASSIGN_ADD:
		case OperatorNode::OP_ADD: {
			return Variant::OP_ADD;
		} break;
		case OperatorNode::OP_ASSIGN_SUB:
		case OperatorNode::OP_SUB: {
			return Variant::OP_SUBTRACT;
		} break;
		case OperatorNode::OP_ASSIGN_MUL:
		case OperatorNode::OP_MUL: {
			return Variant::OP_MULTIPLY;
		} break;
		case OperatorNode::OP_ASSIGN_DIV:
		case OperatorNode::OP_DIV: {
			return Variant::OP_DIVIDE;
		} break;
		case OperatorNode::OP_ASSIGN_MOD:
		case OperatorNode::OP_MOD: {
			return Variant::OP_MODULE;
		} break;
		case OperatorNode::OP_ASSIGN_BIT_AND:
		case OperatorNode::OP_BIT_AND: {
			return Variant::OP_BIT_AND;
		} break;
		case OperatorNode::OP_ASSIGN_BIT_OR:
		case OperatorNode::OP_BIT_OR: {
			return Variant::OP_BIT_OR;
		} break;
		case OperatorNode::OP_ASSIGN_BIT_XOR:
		case OperatorNode::OP_BIT_XOR: {
			return Variant::OP_BIT_XOR;
		} break;
		case OperatorNode::OP_ASSIGN_SHIFT_LEFT:
		case OperatorNode::OP_SHIFT_LEFT: {
			return Variant::OP_SHIFT_LEFT;
		}
		case OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
		case OperatorNode::OP_SHIFT_RIGHT: {
			return Variant::OP_SHIFT_RIGHT;
		}
		default: {
			return Variant::OP_MAX;
		} break;
	}
}

bool GDScriptParser::_is_type_compatible(const DataType &p_container, const DataType &p_expression, bool p_allow_implicit_conversion) const {
	// Ignore for completion
	if (!check_types || for_completion) {
		return true;
	}
	// Can't test if not all have type
	if (!p_container.has_type || !p_expression.has_type) {
		return true;
	}

	// Should never get here unresolved
	ERR_FAIL_COND_V(p_container.kind == DataType::UNRESOLVED, false);
	ERR_FAIL_COND_V(p_expression.kind == DataType::UNRESOLVED, false);

	if (p_container.kind == DataType::BUILTIN && p_expression.kind == DataType::BUILTIN) {
		bool valid = p_container.builtin_type == p_expression.builtin_type;
		if (p_allow_implicit_conversion) {
			valid = valid || Variant::can_convert_strict(p_expression.builtin_type, p_container.builtin_type);
		}
		return valid;
	}

	if (p_container.kind == DataType::BUILTIN && p_container.builtin_type == Variant::OBJECT) {
		// Object built-in is a special case, it's compatible with any object and with null
		if (p_expression.kind == DataType::BUILTIN) {
			return p_expression.builtin_type == Variant::NIL;
		}
		// If it's not a built-in, must be an object
		return true;
	}

	if (p_container.kind == DataType::BUILTIN || (p_expression.kind == DataType::BUILTIN && p_expression.builtin_type != Variant::NIL)) {
		// Can't mix built-ins with objects
		return false;
	}

	// From now on everything is objects, check polymorphism
	// The container must be the same class or a superclass of the expression

	if (p_expression.kind == DataType::BUILTIN && p_expression.builtin_type == Variant::NIL) {
		// Null can be assigned to object types
		return true;
	}

	StringName expr_native;
	Ref<Script> expr_script;
	ClassNode *expr_class = nullptr;

	switch (p_expression.kind) {
		case DataType::NATIVE: {
			if (p_container.kind != DataType::NATIVE) {
				// Non-native type can't be a superclass of a native type
				return false;
			}
			if (p_expression.is_meta_type) {
				expr_native = GDScriptNativeClass::get_class_static();
			} else {
				expr_native = p_expression.native_type;
			}
		} break;
		case DataType::SCRIPT:
		case DataType::GDSCRIPT: {
			if (p_container.kind == DataType::CLASS) {
				// This cannot be resolved without cyclic dependencies, so just bail out
				return false;
			}
			if (p_expression.is_meta_type) {
				expr_native = p_expression.script_type->get_class_name();
			} else {
				expr_script = p_expression.script_type;
				expr_native = expr_script->get_instance_base_type();
			}
		} break;
		case DataType::CLASS: {
			if (p_expression.is_meta_type) {
				expr_native = GDScript::get_class_static();
			} else {
				expr_class = p_expression.class_type;
				ClassNode *base = expr_class;
				while (base->base_type.kind == DataType::CLASS) {
					base = base->base_type.class_type;
				}
				expr_native = base->base_type.native_type;
				expr_script = base->base_type.script_type;
			}
		} break;
		case DataType::BUILTIN: // Already handled above
		case DataType::UNRESOLVED: // Not allowed, see above
			break;
	}

	// Some classes are prefixed with `_` internally
	if (!ClassDB::class_exists(expr_native)) {
		expr_native = "_" + expr_native;
	}

	switch (p_container.kind) {
		case DataType::NATIVE: {
			if (p_container.is_meta_type) {
				return ClassDB::is_parent_class(expr_native, GDScriptNativeClass::get_class_static());
			} else {
				StringName container_native = ClassDB::class_exists(p_container.native_type) ? p_container.native_type : StringName("_" + p_container.native_type);
				return ClassDB::is_parent_class(expr_native, container_native);
			}
		} break;
		case DataType::SCRIPT:
		case DataType::GDSCRIPT: {
			if (p_container.is_meta_type) {
				return ClassDB::is_parent_class(expr_native, GDScript::get_class_static());
			}
			if (expr_class == head && p_container.script_type->get_path() == self_path) {
				// Special case: container is self script and expression is self
				return true;
			}
			while (expr_script.is_valid()) {
				if (expr_script == p_container.script_type) {
					return true;
				}
				expr_script = expr_script->get_base_script();
			}
			return false;
		} break;
		case DataType::CLASS: {
			if (p_container.is_meta_type) {
				return ClassDB::is_parent_class(expr_native, GDScript::get_class_static());
			}
			if (p_container.class_type == head && expr_script.is_valid() && expr_script->get_path() == self_path) {
				// Special case: container is self and expression is self script
				return true;
			}
			while (expr_class) {
				if (expr_class == p_container.class_type) {
					return true;
				}
				expr_class = expr_class->base_type.class_type;
			}
			return false;
		} break;
		case DataType::BUILTIN: // Already handled above
		case DataType::UNRESOLVED: // Not allowed, see above
			break;
	}

	return false;
}

GDScriptParser::Node *GDScriptParser::_get_default_value_for_type(const DataType &p_type, int p_line) {
	Node *result;

	if (p_type.has_type && p_type.kind == DataType::BUILTIN && p_type.builtin_type != Variant::NIL && p_type.builtin_type != Variant::OBJECT) {
		if (p_type.builtin_type == Variant::ARRAY) {
			result = alloc_node<ArrayNode>();
		} else if (p_type.builtin_type == Variant::DICTIONARY) {
			result = alloc_node<DictionaryNode>();
		} else {
			ConstantNode *c = alloc_node<ConstantNode>();
			Variant::CallError err;
			c->value = Variant::construct(p_type.builtin_type, nullptr, 0, err);
			result = c;
		}
	} else {
		ConstantNode *c = alloc_node<ConstantNode>();
		c->value = Variant();
		result = c;
	}

	result->line = p_line;

	return result;
}

GDScriptParser::DataType GDScriptParser::_reduce_node_type(Node *p_node) {
#ifdef DEBUG_ENABLED
	if (p_node->get_datatype().has_type && p_node->type != Node::TYPE_ARRAY && p_node->type != Node::TYPE_DICTIONARY) {
#else
	if (p_node->get_datatype().has_type) {
#endif
		return p_node->get_datatype();
	}

	DataType node_type;

	switch (p_node->type) {
		case Node::TYPE_CONSTANT: {
			node_type = _type_from_variant(static_cast<ConstantNode *>(p_node)->value);
		} break;
		case Node::TYPE_TYPE: {
			TypeNode *tn = static_cast<TypeNode *>(p_node);
			node_type.has_type = true;
			node_type.is_meta_type = true;
			node_type.kind = DataType::BUILTIN;
			node_type.builtin_type = tn->vtype;
		} break;
		case Node::TYPE_ARRAY: {
			node_type.has_type = true;
			node_type.kind = DataType::BUILTIN;
			node_type.builtin_type = Variant::ARRAY;
#ifdef DEBUG_ENABLED
			// Check stuff inside the array
			ArrayNode *an = static_cast<ArrayNode *>(p_node);
			for (int i = 0; i < an->elements.size(); i++) {
				_reduce_node_type(an->elements[i]);
			}
#endif // DEBUG_ENABLED
		} break;
		case Node::TYPE_DICTIONARY: {
			node_type.has_type = true;
			node_type.kind = DataType::BUILTIN;
			node_type.builtin_type = Variant::DICTIONARY;
#ifdef DEBUG_ENABLED
			// Check stuff inside the dictionarty
			DictionaryNode *dn = static_cast<DictionaryNode *>(p_node);
			for (int i = 0; i < dn->elements.size(); i++) {
				_reduce_node_type(dn->elements[i].key);
				_reduce_node_type(dn->elements[i].value);
			}
#endif // DEBUG_ENABLED
		} break;
		case Node::TYPE_SELF: {
			node_type.has_type = true;
			node_type.kind = DataType::CLASS;
			node_type.class_type = current_class;
			node_type.is_constant = true;
		} break;
		case Node::TYPE_IDENTIFIER: {
			IdentifierNode *id = static_cast<IdentifierNode *>(p_node);
			if (id->declared_block) {
				node_type = id->declared_block->variables[id->name]->get_datatype();
				id->declared_block->variables[id->name]->usages += 1;
			} else if (id->name == "#match_value") {
				// It's a special id just for the match statetement, ignore
				break;
			} else if (current_function && current_function->arguments.find(id->name) >= 0) {
				int idx = current_function->arguments.find(id->name);
				node_type = current_function->argument_types[idx];
			} else {
				node_type = _reduce_identifier_type(nullptr, id->name, id->line, false);
			}
		} break;
		case Node::TYPE_CAST: {
			CastNode *cn = static_cast<CastNode *>(p_node);

			DataType source_type = _reduce_node_type(cn->source_node);
			cn->cast_type = _resolve_type(cn->cast_type, cn->line);
			if (source_type.has_type) {
				bool valid = false;
				if (check_types) {
					if (cn->cast_type.kind == DataType::BUILTIN && source_type.kind == DataType::BUILTIN) {
						valid = Variant::can_convert(source_type.builtin_type, cn->cast_type.builtin_type);
					}
					if (cn->cast_type.kind != DataType::BUILTIN && source_type.kind != DataType::BUILTIN) {
						valid = _is_type_compatible(cn->cast_type, source_type) || _is_type_compatible(source_type, cn->cast_type);
					}

					if (!valid) {
						_set_error("Invalid cast. Cannot convert from \"" + source_type.to_string() +
										"\" to \"" + cn->cast_type.to_string() + "\".",
								cn->line);
						return DataType();
					}
				}
			} else {
#ifdef DEBUG_ENABLED
				_add_warning(GDScriptWarning::UNSAFE_CAST, cn->line, cn->cast_type.to_string());
#endif // DEBUG_ENABLED
				_mark_line_as_unsafe(cn->line);
			}

			node_type = cn->cast_type;

		} break;
		case Node::TYPE_OPERATOR: {
			OperatorNode *op = static_cast<OperatorNode *>(p_node);

			switch (op->op) {
				case OperatorNode::OP_CALL:
				case OperatorNode::OP_PARENT_CALL: {
					node_type = _reduce_function_call_type(op);
				} break;
				case OperatorNode::OP_YIELD: {
					if (op->arguments.size() == 2) {
						DataType base_type = _reduce_node_type(op->arguments[0]);
						DataType signal_type = _reduce_node_type(op->arguments[1]);
						// TODO: Check if signal exists when it's a constant
						if (base_type.has_type && base_type.kind == DataType::BUILTIN && base_type.builtin_type != Variant::NIL && base_type.builtin_type != Variant::OBJECT) {
							_set_error("The first argument of \"yield()\" must be an object.", op->line);
							return DataType();
						}
						if (signal_type.has_type && (signal_type.kind != DataType::BUILTIN || signal_type.builtin_type != Variant::STRING)) {
							_set_error("The second argument of \"yield()\" must be a string.", op->line);
							return DataType();
						}
					}
					// yield can return anything
					node_type.has_type = false;
				} break;
				case OperatorNode::OP_IS:
				case OperatorNode::OP_IS_BUILTIN: {
					if (op->arguments.size() != 2) {
						_set_error("Parser bug: binary operation without 2 arguments.", op->line);
						ERR_FAIL_V(DataType());
					}

					DataType value_type = _reduce_node_type(op->arguments[0]);
					DataType type_type = _reduce_node_type(op->arguments[1]);

					if (check_types && type_type.has_type) {
						if (!type_type.is_meta_type && (type_type.kind != DataType::NATIVE || !ClassDB::is_parent_class(type_type.native_type, "Script"))) {
							_set_error("Invalid \"is\" test: the right operand isn't a type (neither a native type nor a script).", op->line);
							return DataType();
						}
						type_type.is_meta_type = false; // Test the actual type
						if (!_is_type_compatible(type_type, value_type) && !_is_type_compatible(value_type, type_type)) {
							if (op->op == OperatorNode::OP_IS) {
								_set_error("A value of type \"" + value_type.to_string() + "\" will never be an instance of \"" + type_type.to_string() + "\".", op->line);
							} else {
								_set_error("A value of type \"" + value_type.to_string() + "\" will never be of type \"" + type_type.to_string() + "\".", op->line);
							}
							return DataType();
						}
					}

					node_type.has_type = true;
					node_type.is_constant = true;
					node_type.is_meta_type = false;
					node_type.kind = DataType::BUILTIN;
					node_type.builtin_type = Variant::BOOL;
				} break;
				// Unary operators
				case OperatorNode::OP_NEG:
				case OperatorNode::OP_POS:
				case OperatorNode::OP_NOT:
				case OperatorNode::OP_BIT_INVERT: {
					DataType argument_type = _reduce_node_type(op->arguments[0]);
					if (!argument_type.has_type) {
						break;
					}

					Variant::Operator var_op = _get_variant_operation(op->op);
					bool valid = false;
					node_type = _get_operation_type(var_op, argument_type, argument_type, valid);

					if (check_types && !valid) {
						_set_error("Invalid operand type (\"" + argument_type.to_string() +
										"\") to unary operator \"" + Variant::get_operator_name(var_op) + "\".",
								op->line, op->column);
						return DataType();
					}

				} break;
				// Binary operators
				case OperatorNode::OP_IN:
				case OperatorNode::OP_EQUAL:
				case OperatorNode::OP_NOT_EQUAL:
				case OperatorNode::OP_LESS:
				case OperatorNode::OP_LESS_EQUAL:
				case OperatorNode::OP_GREATER:
				case OperatorNode::OP_GREATER_EQUAL:
				case OperatorNode::OP_AND:
				case OperatorNode::OP_OR:
				case OperatorNode::OP_ADD:
				case OperatorNode::OP_SUB:
				case OperatorNode::OP_MUL:
				case OperatorNode::OP_DIV:
				case OperatorNode::OP_MOD:
				case OperatorNode::OP_SHIFT_LEFT:
				case OperatorNode::OP_SHIFT_RIGHT:
				case OperatorNode::OP_BIT_AND:
				case OperatorNode::OP_BIT_OR:
				case OperatorNode::OP_BIT_XOR: {
					if (op->arguments.size() != 2) {
						_set_error("Parser bug: binary operation without 2 arguments.", op->line);
						ERR_FAIL_V(DataType());
					}

					DataType argument_a_type = _reduce_node_type(op->arguments[0]);
					DataType argument_b_type = _reduce_node_type(op->arguments[1]);
					if (!argument_a_type.has_type || !argument_b_type.has_type) {
						_mark_line_as_unsafe(op->line);
						break;
					}

					Variant::Operator var_op = _get_variant_operation(op->op);
					bool valid = false;
					node_type = _get_operation_type(var_op, argument_a_type, argument_b_type, valid);

					if (check_types && !valid) {
						_set_error("Invalid operand types (\"" + argument_a_type.to_string() + "\" and \"" +
										argument_b_type.to_string() + "\") to operator \"" + Variant::get_operator_name(var_op) + "\".",
								op->line, op->column);
						return DataType();
					}
#ifdef DEBUG_ENABLED
					if (var_op == Variant::OP_DIVIDE && argument_a_type.kind == DataType::BUILTIN && argument_a_type.builtin_type == Variant::INT &&
							argument_b_type.kind == DataType::BUILTIN && argument_b_type.builtin_type == Variant::INT) {
						_add_warning(GDScriptWarning::INTEGER_DIVISION, op->line);
					}
#endif // DEBUG_ENABLED

				} break;
				// Ternary operators
				case OperatorNode::OP_TERNARY_IF: {
					if (op->arguments.size() != 3) {
						_set_error("Parser bug: ternary operation without 3 arguments.");
						ERR_FAIL_V(DataType());
					}

					DataType true_type = _reduce_node_type(op->arguments[1]);
					DataType false_type = _reduce_node_type(op->arguments[2]);
					// Check arguments[0] errors.
					_reduce_node_type(op->arguments[0]);

					// If types are equal, then the expression is of the same type
					// If they are compatible, return the broader type
					if (true_type == false_type || _is_type_compatible(true_type, false_type)) {
						node_type = true_type;
					} else if (_is_type_compatible(false_type, true_type)) {
						node_type = false_type;
					} else {
#ifdef DEBUG_ENABLED
						_add_warning(GDScriptWarning::INCOMPATIBLE_TERNARY, op->line);
#endif // DEBUG_ENABLED
					}
				} break;
				// Assignment should never happen within an expression
				case OperatorNode::OP_ASSIGN:
				case OperatorNode::OP_ASSIGN_ADD:
				case OperatorNode::OP_ASSIGN_SUB:
				case OperatorNode::OP_ASSIGN_MUL:
				case OperatorNode::OP_ASSIGN_DIV:
				case OperatorNode::OP_ASSIGN_MOD:
				case OperatorNode::OP_ASSIGN_SHIFT_LEFT:
				case OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
				case OperatorNode::OP_ASSIGN_BIT_AND:
				case OperatorNode::OP_ASSIGN_BIT_OR:
				case OperatorNode::OP_ASSIGN_BIT_XOR:
				case OperatorNode::OP_INIT_ASSIGN: {
					_set_error("Assignment inside an expression isn't allowed (parser bug?).", op->line);
					return DataType();

				} break;
				case OperatorNode::OP_INDEX_NAMED: {
					if (op->arguments.size() != 2) {
						_set_error("Parser bug: named index with invalid arguments.", op->line);
						ERR_FAIL_V(DataType());
					}
					if (op->arguments[1]->type != Node::TYPE_IDENTIFIER) {
						_set_error("Parser bug: named index without identifier argument.", op->line);
						ERR_FAIL_V(DataType());
					}

					DataType base_type = _reduce_node_type(op->arguments[0]);
					IdentifierNode *member_id = static_cast<IdentifierNode *>(op->arguments[1]);

					if (base_type.has_type) {
						if (check_types && base_type.kind == DataType::BUILTIN) {
							// Variant type, just test if it's possible
							DataType result;
							switch (base_type.builtin_type) {
								case Variant::NIL:
								case Variant::DICTIONARY: {
									result.has_type = false;
								} break;
								default: {
									Variant::CallError err;
									Variant temp = Variant::construct(base_type.builtin_type, nullptr, 0, err);

									bool valid = false;
									Variant res = temp.get(member_id->name.operator String(), &valid);

									if (valid) {
										result = _type_from_variant(res);
									} else if (check_types) {
										_set_error("Can't get index \"" + String(member_id->name.operator String()) + "\" on base \"" +
														base_type.to_string() + "\".",
												op->line);
										return DataType();
									}
								} break;
							}
							result.is_constant = false;
							node_type = result;
						} else {
							node_type = _reduce_identifier_type(&base_type, member_id->name, op->line, true);
#ifdef DEBUG_ENABLED
							if (!node_type.has_type) {
								_mark_line_as_unsafe(op->line);
								_add_warning(GDScriptWarning::UNSAFE_PROPERTY_ACCESS, op->line, member_id->name.operator String(), base_type.to_string());
							}
#endif // DEBUG_ENABLED
						}
					} else {
						_mark_line_as_unsafe(op->line);
					}
					if (error_set) {
						return DataType();
					}
				} break;
				case OperatorNode::OP_INDEX: {
					if (op->arguments[1]->type == Node::TYPE_CONSTANT) {
						ConstantNode *cn = static_cast<ConstantNode *>(op->arguments[1]);
						if (cn->value.get_type() == Variant::STRING) {
							// Treat this as named indexing

							IdentifierNode *id = alloc_node<IdentifierNode>();
							id->name = cn->value.operator StringName();
							id->datatype = cn->datatype;

							op->op = OperatorNode::OP_INDEX_NAMED;
							op->arguments.write[1] = id;

							return _reduce_node_type(op);
						}
					}

					DataType base_type = _reduce_node_type(op->arguments[0]);
					DataType index_type = _reduce_node_type(op->arguments[1]);

					if (!base_type.has_type) {
						_mark_line_as_unsafe(op->line);
						break;
					}

					if (check_types && index_type.has_type) {
						if (base_type.kind == DataType::BUILTIN) {
							// Check if indexing is valid
							bool error = index_type.kind != DataType::BUILTIN && base_type.builtin_type != Variant::DICTIONARY;
							if (!error) {
								switch (base_type.builtin_type) {
									// Expect int or real as index
									case Variant::POOL_BYTE_ARRAY:
									case Variant::POOL_COLOR_ARRAY:
									case Variant::POOL_INT_ARRAY:
									case Variant::POOL_REAL_ARRAY:
									case Variant::POOL_STRING_ARRAY:
									case Variant::POOL_VECTOR2_ARRAY:
									case Variant::POOL_VECTOR3_ARRAY:
									case Variant::ARRAY:
									case Variant::STRING: {
										error = index_type.builtin_type != Variant::INT && index_type.builtin_type != Variant::REAL;
									} break;
									// Expect String only
									case Variant::RECT2:
									case Variant::PLANE:
									case Variant::QUAT:
									case Variant::AABB:
									case Variant::OBJECT: {
										error = index_type.builtin_type != Variant::STRING;
									} break;
									// Expect String or number
									case Variant::VECTOR2:
									case Variant::VECTOR3:
									case Variant::TRANSFORM2D:
									case Variant::BASIS:
									case Variant::TRANSFORM: {
										error = index_type.builtin_type != Variant::INT && index_type.builtin_type != Variant::REAL &&
												index_type.builtin_type != Variant::STRING;
									} break;
									// Expect String or int
									case Variant::COLOR: {
										error = index_type.builtin_type != Variant::INT && index_type.builtin_type != Variant::STRING;
									} break;
									default: {
									}
								}
							}
							if (error) {
								_set_error("Invalid index type (" + index_type.to_string() + ") for base \"" + base_type.to_string() + "\".",
										op->line);
								return DataType();
							}

							if (op->arguments[1]->type == GDScriptParser::Node::TYPE_CONSTANT) {
								ConstantNode *cn = static_cast<ConstantNode *>(op->arguments[1]);
								// Index is a constant, just try it if possible
								switch (base_type.builtin_type) {
									// Arrays/string have variable indexing, can't test directly
									case Variant::STRING:
									case Variant::ARRAY:
									case Variant::DICTIONARY:
									case Variant::POOL_BYTE_ARRAY:
									case Variant::POOL_COLOR_ARRAY:
									case Variant::POOL_INT_ARRAY:
									case Variant::POOL_REAL_ARRAY:
									case Variant::POOL_STRING_ARRAY:
									case Variant::POOL_VECTOR2_ARRAY:
									case Variant::POOL_VECTOR3_ARRAY: {
										break;
									}
									default: {
										Variant::CallError err;
										Variant temp = Variant::construct(base_type.builtin_type, nullptr, 0, err);

										bool valid = false;
										Variant res = temp.get(cn->value, &valid);

										if (valid) {
											node_type = _type_from_variant(res);
											node_type.is_constant = false;
										} else if (check_types) {
											_set_error("Can't get index \"" + String(cn->value) + "\" on base \"" +
															base_type.to_string() + "\".",
													op->line);
											return DataType();
										}
									} break;
								}
							} else {
								_mark_line_as_unsafe(op->line);
							}
						} else if (!for_completion && (index_type.kind != DataType::BUILTIN || index_type.builtin_type != Variant::STRING)) {
							_set_error("Only strings can be used as an index in the base type \"" + base_type.to_string() + "\".", op->line);
							return DataType();
						}
					}
					if (check_types && !node_type.has_type && base_type.kind == DataType::BUILTIN) {
						// Can infer indexing type for some variant types
						DataType result;
						result.has_type = true;
						result.kind = DataType::BUILTIN;
						switch (base_type.builtin_type) {
							// Can't index at all
							case Variant::NIL:
							case Variant::BOOL:
							case Variant::INT:
							case Variant::REAL:
							case Variant::NODE_PATH:
							case Variant::_RID: {
								_set_error("Can't index on a value of type \"" + base_type.to_string() + "\".", op->line);
								return DataType();
							} break;
								// Return int
							case Variant::POOL_BYTE_ARRAY:
							case Variant::POOL_INT_ARRAY: {
								result.builtin_type = Variant::INT;
							} break;
								// Return real
							case Variant::POOL_REAL_ARRAY:
							case Variant::VECTOR2:
							case Variant::VECTOR3:
							case Variant::QUAT: {
								result.builtin_type = Variant::REAL;
							} break;
								// Return color
							case Variant::POOL_COLOR_ARRAY: {
								result.builtin_type = Variant::COLOR;
							} break;
								// Return string
							case Variant::POOL_STRING_ARRAY:
							case Variant::STRING: {
								result.builtin_type = Variant::STRING;
							} break;
								// Return Vector2
							case Variant::POOL_VECTOR2_ARRAY:
							case Variant::TRANSFORM2D:
							case Variant::RECT2: {
								result.builtin_type = Variant::VECTOR2;
							} break;
								// Return Vector3
							case Variant::POOL_VECTOR3_ARRAY:
							case Variant::AABB:
							case Variant::BASIS: {
								result.builtin_type = Variant::VECTOR3;
							} break;
								// Depends on the index
							case Variant::TRANSFORM:
							case Variant::PLANE:
							case Variant::COLOR:
							default: {
								result.has_type = false;
							} break;
						}
						node_type = result;
					}
				} break;
				default: {
					_set_error("Parser bug: unhandled operation.", op->line);
					ERR_FAIL_V(DataType());
				}
			}
		} break;
		default: {
		}
	}

	node_type = _resolve_type(node_type, p_node->line);
	p_node->set_datatype(node_type);
	return node_type;
}

bool GDScriptParser::_get_function_signature(DataType &p_base_type, const StringName &p_function, DataType &r_return_type, List<DataType> &r_arg_types, int &r_default_arg_count, bool &r_static, bool &r_vararg) const {
	r_static = false;
	r_default_arg_count = 0;

	DataType original_type = p_base_type;
	ClassNode *base = nullptr;
	FunctionNode *callee = nullptr;

	if (p_base_type.kind == DataType::CLASS) {
		base = p_base_type.class_type;
	}

	// Look up the current file (parse tree)
	while (!callee && base) {
		for (int i = 0; i < base->static_functions.size(); i++) {
			FunctionNode *func = base->static_functions[i];
			if (p_function == func->name) {
				r_static = true;
				callee = func;
				break;
			}
		}
		if (!callee && !p_base_type.is_meta_type) {
			for (int i = 0; i < base->functions.size(); i++) {
				FunctionNode *func = base->functions[i];
				if (p_function == func->name) {
					callee = func;
					break;
				}
			}
		}
		p_base_type = base->base_type;
		if (p_base_type.kind == DataType::CLASS) {
			base = p_base_type.class_type;
		} else {
			break;
		}
	}

	if (callee) {
		r_return_type = callee->get_datatype();
		for (int i = 0; i < callee->argument_types.size(); i++) {
			r_arg_types.push_back(callee->argument_types[i]);
		}
		r_default_arg_count = callee->default_values.size();
		return true;
	}

	// Nothing in current file, check parent script
	Ref<GDScript> base_gdscript;
	Ref<Script> base_script;
	StringName native;
	if (p_base_type.kind == DataType::GDSCRIPT) {
		base_gdscript = p_base_type.script_type;
		if (base_gdscript.is_null() || !base_gdscript->is_valid()) {
			// GDScript wasn't properly compíled, don't bother trying
			return false;
		}
	} else if (p_base_type.kind == DataType::SCRIPT) {
		base_script = p_base_type.script_type;
	} else if (p_base_type.kind == DataType::NATIVE) {
		native = p_base_type.native_type;
	}

	while (base_gdscript.is_valid()) {
		native = base_gdscript->get_instance_base_type();

		Map<StringName, GDScriptFunction *> funcs = base_gdscript->get_member_functions();

		if (funcs.has(p_function)) {
			GDScriptFunction *f = funcs[p_function];
			r_static = f->is_static();
			r_default_arg_count = f->get_default_argument_count();
			r_return_type = _type_from_gdtype(f->get_return_type());
			for (int i = 0; i < f->get_argument_count(); i++) {
				r_arg_types.push_back(_type_from_gdtype(f->get_argument_type(i)));
			}
			return true;
		}

		base_gdscript = base_gdscript->get_base_script();
	}

	while (base_script.is_valid()) {
		native = base_script->get_instance_base_type();
		MethodInfo mi = base_script->get_method_info(p_function);

		if (!(mi == MethodInfo())) {
			r_return_type = _type_from_property(mi.return_val, false);
			r_default_arg_count = mi.default_arguments.size();
			for (List<PropertyInfo>::Element *E = mi.arguments.front(); E; E = E->next()) {
				r_arg_types.push_back(_type_from_property(E->get()));
			}
			return true;
		}
		base_script = base_script->get_base_script();
	}

	if (native == StringName()) {
		// Empty native class, might happen in some Script implementations
		// Just ignore it
		return false;
	}

	// Only native remains
	if (!ClassDB::class_exists(native)) {
		native = "_" + native.operator String();
	}
	if (!ClassDB::class_exists(native)) {
		if (!check_types) {
			return false;
		}
		ERR_FAIL_V_MSG(false, "Parser bug: Class '" + String(native) + "' not found.");
	}

	MethodBind *method = ClassDB::get_method(native, p_function);

	if (!method) {
		// Try virtual methods
		List<MethodInfo> virtuals;
		ClassDB::get_virtual_methods(native, &virtuals);

		for (const List<MethodInfo>::Element *E = virtuals.front(); E; E = E->next()) {
			const MethodInfo &mi = E->get();
			if (mi.name == p_function) {
				r_default_arg_count = mi.default_arguments.size();
				for (const List<PropertyInfo>::Element *pi = mi.arguments.front(); pi; pi = pi->next()) {
					r_arg_types.push_back(_type_from_property(pi->get()));
				}
				r_return_type = _type_from_property(mi.return_val, false);
				r_vararg = mi.flags & METHOD_FLAG_VARARG;
				return true;
			}
		}

		// If the base is a script, it might be trying to access members of the Script class itself
		if (original_type.is_meta_type && !(p_function == "new") && (original_type.kind == DataType::SCRIPT || original_type.kind == DataType::GDSCRIPT)) {
			method = ClassDB::get_method(original_type.script_type->get_class_name(), p_function);

			if (method) {
				r_static = true;
			} else {
				// Try virtual methods of the script type
				virtuals.clear();
				ClassDB::get_virtual_methods(original_type.script_type->get_class_name(), &virtuals);
				for (const List<MethodInfo>::Element *E = virtuals.front(); E; E = E->next()) {
					const MethodInfo &mi = E->get();
					if (mi.name == p_function) {
						r_default_arg_count = mi.default_arguments.size();
						for (const List<PropertyInfo>::Element *pi = mi.arguments.front(); pi; pi = pi->next()) {
							r_arg_types.push_back(_type_from_property(pi->get()));
						}
						r_return_type = _type_from_property(mi.return_val, false);
						r_static = true;
						r_vararg = mi.flags & METHOD_FLAG_VARARG;
						return true;
					}
				}
				return false;
			}
		} else {
			return false;
		}
	}

	r_default_arg_count = method->get_default_argument_count();
	if (method->get_name() == "get_script") {
		r_return_type = DataType(); // Variant for now and let runtime decide.
	} else {
		r_return_type = _type_from_property(method->get_return_info(), false);
	}
	r_vararg = method->is_vararg();

	for (int i = 0; i < method->get_argument_count(); i++) {
		r_arg_types.push_back(_type_from_property(method->get_argument_info(i)));
	}
	return true;
}

GDScriptParser::DataType GDScriptParser::_reduce_function_call_type(const OperatorNode *p_call) {
	if (p_call->arguments.size() < 1) {
		_set_error("Parser bug: function call without enough arguments.", p_call->line);
		ERR_FAIL_V(DataType());
	}

	DataType return_type;
	List<DataType> arg_types;
	int default_args_count = 0;
	String callee_name;
	bool is_vararg = false;
#ifdef DEBUG_ENABLED
	int arg_count = p_call->arguments.size();
#endif

	switch (p_call->arguments[0]->type) {
		case GDScriptParser::Node::TYPE_TYPE: {
			// Built-in constructor, special case
			TypeNode *tn = static_cast<TypeNode *>(p_call->arguments[0]);

			Vector<DataType> par_types;
			par_types.resize(p_call->arguments.size() - 1);
			for (int i = 1; i < p_call->arguments.size(); i++) {
				par_types.write[i - 1] = _reduce_node_type(p_call->arguments[i]);
			}

			if (error_set) {
				return DataType();
			}

			// Special case: check copy constructor. Those are defined implicitly in Variant.
			if (par_types.size() == 1) {
				if (!par_types[0].has_type || (par_types[0].kind == DataType::BUILTIN && par_types[0].builtin_type == tn->vtype)) {
					DataType result;
					result.has_type = true;
					result.kind = DataType::BUILTIN;
					result.builtin_type = tn->vtype;
					return result;
				}
			}

			bool match = false;
			List<MethodInfo> constructors;
			Variant::get_constructor_list(tn->vtype, &constructors);
			PropertyInfo return_type2;

			for (List<MethodInfo>::Element *E = constructors.front(); E; E = E->next()) {
				MethodInfo &mi = E->get();

				if (p_call->arguments.size() - 1 < mi.arguments.size() - mi.default_arguments.size()) {
					continue;
				}
				if (p_call->arguments.size() - 1 > mi.arguments.size()) {
					continue;
				}

				bool types_match = true;
				for (int i = 0; i < par_types.size(); i++) {
					DataType arg_type;
					if (mi.arguments[i].type != Variant::NIL) {
						arg_type.has_type = true;
						arg_type.kind = mi.arguments[i].type == Variant::OBJECT ? DataType::NATIVE : DataType::BUILTIN;
						arg_type.builtin_type = mi.arguments[i].type;
						arg_type.native_type = mi.arguments[i].class_name;
					}

					if (!_is_type_compatible(arg_type, par_types[i], true)) {
						types_match = false;
						break;
					} else {
#ifdef DEBUG_ENABLED
						if (arg_type.kind == DataType::BUILTIN && arg_type.builtin_type == Variant::INT && par_types[i].kind == DataType::BUILTIN && par_types[i].builtin_type == Variant::REAL) {
							_add_warning(GDScriptWarning::NARROWING_CONVERSION, p_call->line, Variant::get_type_name(tn->vtype));
						}
						if (par_types[i].may_yield && p_call->arguments[i + 1]->type == Node::TYPE_OPERATOR) {
							_add_warning(GDScriptWarning::FUNCTION_MAY_YIELD, p_call->line, _find_function_name(static_cast<OperatorNode *>(p_call->arguments[i + 1])));
						}
#endif // DEBUG_ENABLED
					}
				}

				if (types_match) {
					match = true;
					return_type2 = mi.return_val;
					break;
				}
			}

			if (match) {
				return _type_from_property(return_type2, false);
			} else if (check_types) {
				String err = "No constructor of '";
				err += Variant::get_type_name(tn->vtype);
				err += "' matches the signature '";
				err += Variant::get_type_name(tn->vtype) + "(";
				for (int i = 0; i < par_types.size(); i++) {
					if (i > 0) {
						err += ", ";
					}
					err += par_types[i].to_string();
				}
				err += ")'.";
				_set_error(err, p_call->line, p_call->column);
				return DataType();
			}
			return DataType();
		} break;
		case GDScriptParser::Node::TYPE_BUILT_IN_FUNCTION: {
			BuiltInFunctionNode *func = static_cast<BuiltInFunctionNode *>(p_call->arguments[0]);
			MethodInfo mi = GDScriptFunctions::get_info(func->function);

			return_type = _type_from_property(mi.return_val, false);

			// Check all arguments beforehand to solve warnings
			for (int i = 1; i < p_call->arguments.size(); i++) {
				_reduce_node_type(p_call->arguments[i]);
			}

			// Check arguments

			is_vararg = mi.flags & METHOD_FLAG_VARARG;

			default_args_count = mi.default_arguments.size();
			callee_name = mi.name;
#ifdef DEBUG_ENABLED
			arg_count -= 1;
#endif

			// Check each argument type
			for (List<PropertyInfo>::Element *E = mi.arguments.front(); E; E = E->next()) {
				arg_types.push_back(_type_from_property(E->get()));
			}
		} break;
		default: {
			if (p_call->op == OperatorNode::OP_CALL && p_call->arguments.size() < 2) {
				_set_error("Parser bug: self method call without enough arguments.", p_call->line);
				ERR_FAIL_V(DataType());
			}

			int arg_id = p_call->op == OperatorNode::OP_CALL ? 1 : 0;

			if (p_call->arguments[arg_id]->type != Node::TYPE_IDENTIFIER) {
				_set_error("Parser bug: invalid function call argument.", p_call->line);
				ERR_FAIL_V(DataType());
			}

			// Check all arguments beforehand to solve warnings
			for (int i = arg_id + 1; i < p_call->arguments.size(); i++) {
				_reduce_node_type(p_call->arguments[i]);
			}

			IdentifierNode *func_id = static_cast<IdentifierNode *>(p_call->arguments[arg_id]);
			callee_name = func_id->name;
#ifdef DEBUG_ENABLED
			arg_count -= 1 + arg_id;
#endif

			DataType base_type;
			if (p_call->op == OperatorNode::OP_PARENT_CALL) {
				base_type = current_class->base_type;
			} else {
				base_type = _reduce_node_type(p_call->arguments[0]);
			}

			if (!base_type.has_type || (base_type.kind == DataType::BUILTIN && base_type.builtin_type == Variant::NIL)) {
				_mark_line_as_unsafe(p_call->line);
				return DataType();
			}

			if (base_type.kind == DataType::BUILTIN) {
				Variant::CallError err;
				Variant tmp = Variant::construct(base_type.builtin_type, nullptr, 0, err);

				if (check_types) {
					if (!tmp.has_method(callee_name)) {
						_set_error("The method \"" + callee_name + "\" isn't declared on base \"" + base_type.to_string() + "\".", p_call->line);
						return DataType();
					}

					default_args_count = Variant::get_method_default_arguments(base_type.builtin_type, callee_name).size();
					const Vector<Variant::Type> &var_arg_types = Variant::get_method_argument_types(base_type.builtin_type, callee_name);

					for (int i = 0; i < var_arg_types.size(); i++) {
						DataType argtype;
						if (var_arg_types[i] != Variant::NIL) {
							argtype.has_type = true;
							argtype.kind = DataType::BUILTIN;
							argtype.builtin_type = var_arg_types[i];
						}
						arg_types.push_back(argtype);
					}
				}

				bool rets = false;
				return_type.has_type = true;
				return_type.kind = DataType::BUILTIN;
				return_type.builtin_type = Variant::get_method_return_type(base_type.builtin_type, callee_name, &rets);
				// If the method returns, but it might return any type, (Variant::NIL), pretend we don't know the type.
				// At least make sure we know that it returns
				if (rets && return_type.builtin_type == Variant::NIL) {
					return_type.has_type = false;
				}
				break;
			}

			DataType original_type = base_type;
			bool is_initializer = callee_name == "new";
			bool is_get_script = p_call->arguments[0]->type == Node::TYPE_SELF && callee_name == "get_script";
			bool is_static = false;
			bool valid = false;

			if (is_initializer && original_type.is_meta_type) {
				// Try to check it as initializer
				base_type = original_type;
				callee_name = "_init";
				base_type.is_meta_type = false;

				valid = _get_function_signature(base_type, callee_name, return_type, arg_types,
						default_args_count, is_static, is_vararg);

				return_type = original_type;
				return_type.is_meta_type = false;

				valid = true; // There's always an initializer, we can assume this is true
			}

			if (is_get_script) {
				// get_script() can be considered a meta-type.
				return_type.kind = DataType::CLASS;
				return_type.class_type = static_cast<ClassNode *>(head);
				return_type.is_meta_type = true;
				valid = true;
			}

			if (!valid) {
				base_type = original_type;
				return_type = DataType();
				valid = _get_function_signature(base_type, callee_name, return_type, arg_types,
						default_args_count, is_static, is_vararg);
			}

			if (!valid) {
#ifdef DEBUG_ENABLED
				if (p_call->arguments[0]->type == Node::TYPE_SELF) {
					_set_error("The method \"" + callee_name + "\" isn't declared in the current class.", p_call->line);
					return DataType();
				}
				DataType tmp_type;
				valid = _get_member_type(original_type, func_id->name, tmp_type);
				if (valid) {
					if (tmp_type.is_constant) {
						_add_warning(GDScriptWarning::CONSTANT_USED_AS_FUNCTION, p_call->line, callee_name, original_type.to_string());
					} else {
						_add_warning(GDScriptWarning::PROPERTY_USED_AS_FUNCTION, p_call->line, callee_name, original_type.to_string());
					}
				}
				_add_warning(GDScriptWarning::UNSAFE_METHOD_ACCESS, p_call->line, callee_name, original_type.to_string());
				_mark_line_as_unsafe(p_call->line);
#endif // DEBUG_ENABLED
				return DataType();
			}

#ifdef DEBUG_ENABLED
			if (current_function && !for_completion && !is_static && p_call->arguments[0]->type == Node::TYPE_SELF && current_function->_static) {
				_set_error("Can't call non-static function from a static function.", p_call->line);
				return DataType();
			}

			if (check_types && !is_static && !is_initializer && base_type.is_meta_type) {
				_set_error("Non-static function \"" + String(callee_name) + "\" can only be called from an instance.", p_call->line);
				return DataType();
			}

			// Check signal emission for warnings
			if (callee_name == "emit_signal" && p_call->op == OperatorNode::OP_CALL && p_call->arguments[0]->type == Node::TYPE_SELF && p_call->arguments.size() >= 3 && p_call->arguments[2]->type == Node::TYPE_CONSTANT) {
				ConstantNode *sig = static_cast<ConstantNode *>(p_call->arguments[2]);
				String emitted = sig->value.get_type() == Variant::STRING ? sig->value.operator String() : "";
				for (int i = 0; i < current_class->_signals.size(); i++) {
					if (current_class->_signals[i].name == emitted) {
						current_class->_signals.write[i].emissions += 1;
						break;
					}
				}
			}
#endif // DEBUG_ENABLED
		} break;
	}

#ifdef DEBUG_ENABLED
	if (!check_types) {
		return return_type;
	}

	if (arg_count < arg_types.size() - default_args_count) {
		_set_error("Too few arguments for \"" + callee_name + "()\" call. Expected at least " + itos(arg_types.size() - default_args_count) + ".", p_call->line);
		return return_type;
	}
	if (!is_vararg && arg_count > arg_types.size()) {
		_set_error("Too many arguments for \"" + callee_name + "()\" call. Expected at most " + itos(arg_types.size()) + ".", p_call->line);
		return return_type;
	}

	int arg_diff = p_call->arguments.size() - arg_count;
	for (int i = arg_diff; i < p_call->arguments.size(); i++) {
		DataType par_type = _reduce_node_type(p_call->arguments[i]);

		if ((i - arg_diff) >= arg_types.size()) {
			continue;
		}

		DataType arg_type = arg_types[i - arg_diff];

		if (!par_type.has_type) {
			_mark_line_as_unsafe(p_call->line);
			if (par_type.may_yield && p_call->arguments[i]->type == Node::TYPE_OPERATOR) {
				_add_warning(GDScriptWarning::FUNCTION_MAY_YIELD, p_call->line, _find_function_name(static_cast<OperatorNode *>(p_call->arguments[i])));
			}
		} else if (!_is_type_compatible(arg_types[i - arg_diff], par_type, true)) {
			// Supertypes are acceptable for dynamic compliance
			if (!_is_type_compatible(par_type, arg_types[i - arg_diff])) {
				_set_error("At \"" + callee_name + "()\" call, argument " + itos(i - arg_diff + 1) + ". The passed argument's type (" +
								par_type.to_string() + ") doesn't match the function's expected argument type (" +
								arg_types[i - arg_diff].to_string() + ").",
						p_call->line);
				return DataType();
			} else {
				_mark_line_as_unsafe(p_call->line);
			}
		} else {
			if (arg_type.kind == DataType::BUILTIN && arg_type.builtin_type == Variant::INT && par_type.kind == DataType::BUILTIN && par_type.builtin_type == Variant::REAL) {
				_add_warning(GDScriptWarning::NARROWING_CONVERSION, p_call->line, callee_name);
			}
		}
	}
#endif // DEBUG_ENABLED

	return return_type;
}

bool GDScriptParser::_get_member_type(const DataType &p_base_type, const StringName &p_member, DataType &r_member_type, bool *r_is_const) const {
	DataType base_type = p_base_type;

	// Check classes in current file
	ClassNode *base = nullptr;
	if (base_type.kind == DataType::CLASS) {
		base = base_type.class_type;
	}

	while (base) {
		if (base->constant_expressions.has(p_member)) {
			if (r_is_const) {
				*r_is_const = true;
			}
			r_member_type = base->constant_expressions[p_member].expression->get_datatype();
			return true;
		}

		if (!base_type.is_meta_type) {
			for (int i = 0; i < base->variables.size(); i++) {
				if (base->variables[i].identifier == p_member) {
					r_member_type = base->variables[i].data_type;
					base->variables.write[i].usages += 1;
					return true;
				}
			}
		} else {
			for (int i = 0; i < base->subclasses.size(); i++) {
				ClassNode *c = base->subclasses[i];
				if (c->name == p_member) {
					DataType class_type;
					class_type.has_type = true;
					class_type.is_constant = true;
					class_type.is_meta_type = true;
					class_type.kind = DataType::CLASS;
					class_type.class_type = c;
					r_member_type = class_type;
					return true;
				}
			}
		}

		base_type = base->base_type;
		if (base_type.kind == DataType::CLASS) {
			base = base_type.class_type;
		} else {
			break;
		}
	}

	Ref<GDScript> gds;
	if (base_type.kind == DataType::GDSCRIPT) {
		gds = base_type.script_type;
		if (gds.is_null() || !gds->is_valid()) {
			// GDScript wasn't properly compíled, don't bother trying
			return false;
		}
	}

	Ref<Script> scr;
	if (base_type.kind == DataType::SCRIPT) {
		scr = base_type.script_type;
	}

	StringName native;
	if (base_type.kind == DataType::NATIVE) {
		native = base_type.native_type;
	}

	// Check GDScripts
	while (gds.is_valid()) {
		if (gds->get_constants().has(p_member)) {
			Variant c = gds->get_constants()[p_member];
			r_member_type = _type_from_variant(c);
			return true;
		}

		if (!base_type.is_meta_type) {
			if (gds->get_members().has(p_member)) {
				r_member_type = _type_from_gdtype(gds->get_member_type(p_member));
				return true;
			}
		}

		native = gds->get_instance_base_type();
		if (gds->get_base_script().is_valid()) {
			gds = gds->get_base_script();
			scr = gds->get_base_script();
			bool is_meta = base_type.is_meta_type;
			base_type = _type_from_variant(scr.operator Variant());
			base_type.is_meta_type = is_meta;
		} else {
			break;
		}
	}

#define IS_USAGE_MEMBER(m_usage) (!(m_usage & (PROPERTY_USAGE_GROUP | PROPERTY_USAGE_CATEGORY)))

	// Check other script types
	while (scr.is_valid()) {
		Map<StringName, Variant> constants;
		scr->get_constants(&constants);
		if (constants.has(p_member)) {
			r_member_type = _type_from_variant(constants[p_member]);
			return true;
		}

		List<PropertyInfo> properties;
		scr->get_script_property_list(&properties);
		for (List<PropertyInfo>::Element *E = properties.front(); E; E = E->next()) {
			if (E->get().name == p_member && IS_USAGE_MEMBER(E->get().usage)) {
				r_member_type = _type_from_property(E->get());
				return true;
			}
		}

		base_type = _type_from_variant(scr.operator Variant());
		native = scr->get_instance_base_type();
		scr = scr->get_base_script();
	}

	if (native == StringName()) {
		// Empty native class, might happen in some Script implementations
		// Just ignore it
		return false;
	}

	// Check ClassDB
	if (!ClassDB::class_exists(native)) {
		native = "_" + native.operator String();
	}
	if (!ClassDB::class_exists(native)) {
		if (!check_types) {
			return false;
		}
		ERR_FAIL_V_MSG(false, "Parser bug: Class \"" + String(native) + "\" not found.");
	}

	bool valid = false;
	ClassDB::get_integer_constant(native, p_member, &valid);
	if (valid) {
		DataType ct;
		ct.has_type = true;
		ct.is_constant = true;
		ct.kind = DataType::BUILTIN;
		ct.builtin_type = Variant::INT;
		r_member_type = ct;
		return true;
	}

	if (!base_type.is_meta_type) {
		List<PropertyInfo> properties;
		ClassDB::get_property_list(native, &properties);
		for (List<PropertyInfo>::Element *E = properties.front(); E; E = E->next()) {
			if (E->get().name == p_member && IS_USAGE_MEMBER(E->get().usage)) {
				// Check if a getter exists
				StringName getter_name = ClassDB::get_property_getter(native, p_member);
				if (getter_name != StringName()) {
					// Use the getter return type
					MethodBind *getter_method = ClassDB::get_method(native, getter_name);
					if (getter_method) {
						r_member_type = _type_from_property(getter_method->get_return_info());
					} else {
						r_member_type = DataType();
					}
				} else {
					r_member_type = _type_from_property(E->get());
				}
				return true;
			}
		}
	}

	// If the base is a script, it might be trying to access members of the Script class itself
	if (p_base_type.is_meta_type && (p_base_type.kind == DataType::SCRIPT || p_base_type.kind == DataType::GDSCRIPT)) {
		native = p_base_type.script_type->get_class_name();
		ClassDB::get_integer_constant(native, p_member, &valid);
		if (valid) {
			DataType ct;
			ct.has_type = true;
			ct.is_constant = true;
			ct.kind = DataType::BUILTIN;
			ct.builtin_type = Variant::INT;
			r_member_type = ct;
			return true;
		}

		List<PropertyInfo> properties;
		ClassDB::get_property_list(native, &properties);
		for (List<PropertyInfo>::Element *E = properties.front(); E; E = E->next()) {
			if (E->get().name == p_member && IS_USAGE_MEMBER(E->get().usage)) {
				// Check if a getter exists
				StringName getter_name = ClassDB::get_property_getter(native, p_member);
				if (getter_name != StringName()) {
					// Use the getter return type
					MethodBind *getter_method = ClassDB::get_method(native, getter_name);
					if (getter_method) {
						r_member_type = _type_from_property(getter_method->get_return_info());
					} else {
						r_member_type = DataType();
					}
				} else {
					r_member_type = _type_from_property(E->get());
				}
				return true;
			}
		}
	}
#undef IS_USAGE_MEMBER

	return false;
}

GDScriptParser::DataType GDScriptParser::_reduce_identifier_type(const DataType *p_base_type, const StringName &p_identifier, int p_line, bool p_is_indexing) {
	if (p_base_type && !p_base_type->has_type) {
		return DataType();
	}

	DataType base_type;
	DataType member_type;

	if (!p_base_type) {
		base_type.has_type = true;
		base_type.is_constant = true;
		base_type.kind = DataType::CLASS;
		base_type.class_type = current_class;
	} else {
		base_type = DataType(*p_base_type);
	}

	bool is_const = false;
	if (_get_member_type(base_type, p_identifier, member_type, &is_const)) {
		if (!p_base_type && current_function && current_function->_static && !is_const) {
			_set_error("Can't access member variable (\"" + p_identifier.operator String() + "\") from a static function.", p_line);
			return DataType();
		}
		return member_type;
	}

	if (p_is_indexing) {
		// Don't look for globals since this is an indexed identifier
		return DataType();
	}

	if (!p_base_type) {
		// Possibly this is a global, check before failing

		if (ClassDB::class_exists(p_identifier) || ClassDB::class_exists("_" + p_identifier.operator String())) {
			DataType result;
			result.has_type = true;
			result.is_constant = true;
			result.is_meta_type = true;
			if (Engine::get_singleton()->has_singleton(p_identifier) || Engine::get_singleton()->has_singleton("_" + p_identifier.operator String())) {
				result.is_meta_type = false;
			}
			result.kind = DataType::NATIVE;
			result.native_type = p_identifier;
			return result;
		}

		ClassNode *outer_class = current_class;
		while (outer_class) {
			if (outer_class->name == p_identifier) {
				DataType result;
				result.has_type = true;
				result.is_constant = true;
				result.is_meta_type = true;
				result.kind = DataType::CLASS;
				result.class_type = outer_class;
				return result;
			}
			if (outer_class->constant_expressions.has(p_identifier)) {
				return outer_class->constant_expressions[p_identifier].type;
			}
			for (int i = 0; i < outer_class->subclasses.size(); i++) {
				if (outer_class->subclasses[i] == current_class) {
					continue;
				}
				if (outer_class->subclasses[i]->name == p_identifier) {
					DataType result;
					result.has_type = true;
					result.is_constant = true;
					result.is_meta_type = true;
					result.kind = DataType::CLASS;
					result.class_type = outer_class->subclasses[i];
					return result;
				}
			}
			outer_class = outer_class->owner;
		}

		if (ScriptServer::is_global_class(p_identifier)) {
			Ref<Script> scr = ResourceLoader::load(ScriptServer::get_global_class_path(p_identifier));
			if (scr.is_valid()) {
				DataType result;
				result.has_type = true;
				result.script_type = scr;
				result.is_constant = true;
				result.is_meta_type = true;
				Ref<GDScript> gds = scr;
				if (gds.is_valid()) {
					if (!gds->is_valid()) {
						_set_error("The class \"" + p_identifier + "\" couldn't be fully loaded (script error or cyclic dependency).");
						return DataType();
					}
					result.kind = DataType::GDSCRIPT;
				} else {
					result.kind = DataType::SCRIPT;
				}
				return result;
			}
			_set_error("The class \"" + p_identifier + "\" was found in global scope, but its script couldn't be loaded.");
			return DataType();
		}

		if (GDScriptLanguage::get_singleton()->get_global_map().has(p_identifier)) {
			int idx = GDScriptLanguage::get_singleton()->get_global_map()[p_identifier];
			Variant g = GDScriptLanguage::get_singleton()->get_global_array()[idx];
			return _type_from_variant(g);
		}

		if (GDScriptLanguage::get_singleton()->get_named_globals_map().has(p_identifier)) {
			Variant g = GDScriptLanguage::get_singleton()->get_named_globals_map()[p_identifier];
			return _type_from_variant(g);
		}

		// Non-tool singletons aren't loaded, check project settings
		String autoload_path = _lookup_autoload_path_for_identifier(p_identifier);
		if (!autoload_path.empty()) {
			Ref<Script> singleton = ResourceLoader::load(autoload_path);
			if (singleton.is_valid()) {
				DataType result;
				result.has_type = true;
				result.is_constant = true;
				result.script_type = singleton;

				Ref<GDScript> gds = singleton;
				if (gds.is_valid()) {
					if (!gds->is_valid()) {
						_set_error("Couldn't fully load the singleton script \"" + p_identifier + "\" (possible cyclic reference or parse error).", p_line);
						return DataType();
					}
					result.kind = DataType::GDSCRIPT;
				} else {
					result.kind = DataType::SCRIPT;
				}
			}
		}

		// This means looking in the current class, which type is always known
		_set_error("The identifier \"" + p_identifier.operator String() + "\" isn't declared in the current scope.", p_line);
	}

#ifdef DEBUG_ENABLED
	{
		DataType tmp_type;
		List<DataType> arg_types;
		int argcount;
		bool _static;
		bool vararg;
		if (_get_function_signature(base_type, p_identifier, tmp_type, arg_types, argcount, _static, vararg)) {
			_add_warning(GDScriptWarning::FUNCTION_USED_AS_PROPERTY, p_line, p_identifier.operator String(), base_type.to_string());
		}
	}
#endif // DEBUG_ENABLED

	_mark_line_as_unsafe(p_line);
	return DataType();
}

void GDScriptParser::_check_class_level_types(ClassNode *p_class) {
	// Names of internal object properties that we check to avoid overriding them.
	// "__meta__" could also be in here, but since it doesn't really affect object metadata,
	// it is okay to override it on script.
	StringName script_name = CoreStringNames::get_singleton()->_script;

	_mark_line_as_safe(p_class->line);

	// Constants
	for (Map<StringName, ClassNode::Constant>::Element *E = p_class->constant_expressions.front(); E; E = E->next()) {
		ClassNode::Constant &c = E->get();
		_mark_line_as_safe(c.expression->line);
		DataType cont = _resolve_type(c.type, c.expression->line);
		DataType expr = _resolve_type(c.expression->get_datatype(), c.expression->line);

		if (check_types && !_is_type_compatible(cont, expr)) {
			_set_error("The constant value type (" + expr.to_string() + ") isn't compatible with declared type (" + cont.to_string() + ").",
					c.expression->line);
			return;
		}

		expr.is_constant = true;
		c.type = expr;
		c.expression->set_datatype(expr);

		DataType tmp;
		const StringName &constant_name = E->key();
		if (constant_name == script_name || _get_member_type(p_class->base_type, constant_name, tmp)) {
			_set_error("The member \"" + String(constant_name) + "\" already exists in a parent class.", c.expression->line);
			return;
		}
	}

	// Function declarations
	for (int i = 0; i < p_class->static_functions.size(); i++) {
		_check_function_types(p_class->static_functions[i]);
		if (error_set) {
			return;
		}
	}

	for (int i = 0; i < p_class->functions.size(); i++) {
		_check_function_types(p_class->functions[i]);
		if (error_set) {
			return;
		}
	}

	// Class variables
	for (int i = 0; i < p_class->variables.size(); i++) {
		ClassNode::Member &v = p_class->variables.write[i];

		DataType tmp;
		if (v.identifier == script_name || _get_member_type(p_class->base_type, v.identifier, tmp)) {
			_set_error("The member \"" + String(v.identifier) + "\" already exists in a parent class.", v.line);
			return;
		}

		_mark_line_as_safe(v.line);
		v.data_type = _resolve_type(v.data_type, v.line);
		v.initial_assignment->arguments[0]->set_datatype(v.data_type);

		if (v.expression) {
			DataType expr_type = _reduce_node_type(v.expression);

			if (check_types && !_is_type_compatible(v.data_type, expr_type)) {
				// Try supertype test
				if (_is_type_compatible(expr_type, v.data_type)) {
					_mark_line_as_unsafe(v.line);
				} else {
					// Try with implicit conversion
					if (v.data_type.kind != DataType::BUILTIN || !_is_type_compatible(v.data_type, expr_type, true)) {
						_set_error("The assigned expression's type (" + expr_type.to_string() + ") doesn't match the variable's type (" +
										v.data_type.to_string() + ").",
								v.line);
						return;
					}

					// Replace assignment with implicit conversion
					BuiltInFunctionNode *convert = alloc_node<BuiltInFunctionNode>();
					convert->line = v.line;
					convert->function = GDScriptFunctions::TYPE_CONVERT;

					ConstantNode *tgt_type = alloc_node<ConstantNode>();
					tgt_type->line = v.line;
					tgt_type->value = (int)v.data_type.builtin_type;

					OperatorNode *convert_call = alloc_node<OperatorNode>();
					convert_call->line = v.line;
					convert_call->op = OperatorNode::OP_CALL;
					convert_call->arguments.push_back(convert);
					convert_call->arguments.push_back(v.expression);
					convert_call->arguments.push_back(tgt_type);

					v.expression = convert_call;
					v.initial_assignment->arguments.write[1] = convert_call;
				}
			}

			if (v.data_type.infer_type) {
				if (!expr_type.has_type) {
					_set_error("The assigned value doesn't have a set type; the variable type can't be inferred.", v.line);
					return;
				}
				if (expr_type.kind == DataType::BUILTIN && expr_type.builtin_type == Variant::NIL) {
					_set_error("The variable type cannot be inferred because its value is \"null\".", v.line);
					return;
				}
				v.data_type = expr_type;
				v.data_type.is_constant = false;
			}
		}

		// Check export hint
		if (v.data_type.has_type && v._export.type != Variant::NIL) {
			DataType export_type = _type_from_property(v._export);
			if (!_is_type_compatible(v.data_type, export_type, true)) {
				_set_error("The export hint's type (" + export_type.to_string() + ") doesn't match the variable's type (" +
								v.data_type.to_string() + ").",
						v.line);
				return;
			}
		}

		// Setter and getter
		if (v.setter == StringName() && v.getter == StringName()) {
			continue;
		}

		bool found_getter = false;
		bool found_setter = false;
		for (int j = 0; j < p_class->functions.size(); j++) {
			if (v.setter == p_class->functions[j]->name) {
				found_setter = true;
				FunctionNode *setter = p_class->functions[j];

				if (setter->get_required_argument_count() != 1 &&
						!(setter->get_required_argument_count() == 0 && setter->default_values.size() > 0)) {
					_set_error("The setter function needs to receive exactly 1 argument. See \"" + setter->name +
									"()\" definition at line " + itos(setter->line) + ".",
							v.line);
					return;
				}
				if (!_is_type_compatible(v.data_type, setter->argument_types[0])) {
					_set_error("The setter argument's type (" + setter->argument_types[0].to_string() +
									") doesn't match the variable's type (" + v.data_type.to_string() + "). See \"" +
									setter->name + "()\" definition at line " + itos(setter->line) + ".",
							v.line);
					return;
				}
				continue;
			}
			if (v.getter == p_class->functions[j]->name) {
				found_getter = true;
				FunctionNode *getter = p_class->functions[j];

				if (getter->get_required_argument_count() != 0) {
					_set_error("The getter function can't receive arguments. See \"" + getter->name +
									"()\" definition at line " + itos(getter->line) + ".",
							v.line);
					return;
				}
				if (!_is_type_compatible(v.data_type, getter->get_datatype())) {
					_set_error("The getter return type (" + getter->get_datatype().to_string() +
									") doesn't match the variable's type (" + v.data_type.to_string() +
									"). See \"" + getter->name + "()\" definition at line " + itos(getter->line) + ".",
							v.line);
					return;
				}
			}
			if (found_getter && found_setter) {
				break;
			}
		}

		if ((found_getter || v.getter == StringName()) && (found_setter || v.setter == StringName())) {
			continue;
		}

		// Check for static functions
		for (int j = 0; j < p_class->static_functions.size(); j++) {
			if (v.setter == p_class->static_functions[j]->name) {
				FunctionNode *setter = p_class->static_functions[j];
				_set_error("The setter can't be a static function. See \"" + setter->name + "()\" definition at line " + itos(setter->line) + ".", v.line);
				return;
			}
			if (v.getter == p_class->static_functions[j]->name) {
				FunctionNode *getter = p_class->static_functions[j];
				_set_error("The getter can't be a static function. See \"" + getter->name + "()\" definition at line " + itos(getter->line) + ".", v.line);
				return;
			}
		}

		if (!found_setter && v.setter != StringName()) {
			_set_error("The setter function isn't defined.", v.line);
			return;
		}

		if (!found_getter && v.getter != StringName()) {
			_set_error("The getter function isn't defined.", v.line);
			return;
		}
	}

	// Signals
	DataType base = p_class->base_type;

	while (base.kind == DataType::CLASS) {
		ClassNode *base_class = base.class_type;
		for (int i = 0; i < p_class->_signals.size(); i++) {
			for (int j = 0; j < base_class->_signals.size(); j++) {
				if (p_class->_signals[i].name == base_class->_signals[j].name) {
					_set_error("The signal \"" + p_class->_signals[i].name + "\" already exists in a parent class.", p_class->_signals[i].line);
					return;
				}
			}
		}
		base = base_class->base_type;
	}

	StringName native;
	if (base.kind == DataType::GDSCRIPT || base.kind == DataType::SCRIPT) {
		Ref<Script> scr = base.script_type;
		if (scr.is_valid() && scr->is_valid()) {
			native = scr->get_instance_base_type();
			for (int i = 0; i < p_class->_signals.size(); i++) {
				if (scr->has_script_signal(p_class->_signals[i].name)) {
					_set_error("The signal \"" + p_class->_signals[i].name + "\" already exists in a parent class.", p_class->_signals[i].line);
					return;
				}
			}
		}
	} else if (base.kind == DataType::NATIVE) {
		native = base.native_type;
	}

	if (native != StringName()) {
		for (int i = 0; i < p_class->_signals.size(); i++) {
			if (ClassDB::has_signal(native, p_class->_signals[i].name)) {
				_set_error("The signal \"" + p_class->_signals[i].name + "\" already exists in a parent class.", p_class->_signals[i].line);
				return;
			}
		}
	}

	// Inner classes
	for (int i = 0; i < p_class->subclasses.size(); i++) {
		current_class = p_class->subclasses[i];
		_check_class_level_types(current_class);
		if (error_set) {
			return;
		}
		current_class = p_class;
	}
}

void GDScriptParser::_check_function_types(FunctionNode *p_function) {
	p_function->return_type = _resolve_type(p_function->return_type, p_function->line);

	// Arguments
	int defaults_ofs = p_function->arguments.size() - p_function->default_values.size();
	for (int i = 0; i < p_function->arguments.size(); i++) {
		if (i < defaults_ofs) {
			p_function->argument_types.write[i] = _resolve_type(p_function->argument_types[i], p_function->line);
		} else {
			if (p_function->default_values[i - defaults_ofs]->type != Node::TYPE_OPERATOR) {
				_set_error("Parser bug: invalid argument default value.", p_function->line, p_function->column);
				return;
			}

			OperatorNode *op = static_cast<OperatorNode *>(p_function->default_values[i - defaults_ofs]);

			if (op->op != OperatorNode::OP_ASSIGN || op->arguments.size() != 2) {
				_set_error("Parser bug: invalid argument default value operation.", p_function->line);
				return;
			}

			DataType def_type = _reduce_node_type(op->arguments[1]);

			if (p_function->argument_types[i].infer_type) {
				def_type.is_constant = false;
				p_function->argument_types.write[i] = def_type;
			} else {
				p_function->argument_types.write[i] = _resolve_type(p_function->argument_types[i], p_function->line);

				if (!_is_type_compatible(p_function->argument_types[i], def_type, true)) {
					String arg_name = p_function->arguments[i];
					_set_error("Value type (" + def_type.to_string() + ") doesn't match the type of argument '" +
									arg_name + "' (" + p_function->argument_types[i].to_string() + ").",
							p_function->line);
				}
			}
		}
#ifdef DEBUG_ENABLED
		if (p_function->arguments_usage[i] == 0 && !p_function->arguments[i].operator String().begins_with("_")) {
			_add_warning(GDScriptWarning::UNUSED_ARGUMENT, p_function->line, p_function->name, p_function->arguments[i].operator String());
		}
		for (int j = 0; j < current_class->variables.size(); j++) {
			if (current_class->variables[j].identifier == p_function->arguments[i]) {
				_add_warning(GDScriptWarning::SHADOWED_VARIABLE, p_function->line, p_function->arguments[i], itos(current_class->variables[j].line));
			}
		}
#endif // DEBUG_ENABLED
	}

	if (!(p_function->name == "_init")) {
		// Signature for the initializer may vary
#ifdef DEBUG_ENABLED
		DataType return_type;
		List<DataType> arg_types;
		int default_arg_count = 0;
		bool _static = false;
		bool vararg = false;

		DataType base_type = current_class->base_type;
		if (_get_function_signature(base_type, p_function->name, return_type, arg_types, default_arg_count, _static, vararg)) {
			bool valid = _static == p_function->_static;
			valid = valid && return_type == p_function->return_type;
			int argsize_diff = p_function->arguments.size() - arg_types.size();
			valid = valid && argsize_diff >= 0;
			valid = valid && p_function->default_values.size() >= default_arg_count + argsize_diff;
			int i = 0;
			for (List<DataType>::Element *E = arg_types.front(); valid && E; E = E->next()) {
				valid = valid && E->get() == p_function->argument_types[i++];
			}

			if (!valid) {
				String parent_signature = return_type.has_type ? return_type.to_string() : "Variant";
				if (parent_signature == "null") {
					parent_signature = "void";
				}
				parent_signature += " " + p_function->name + "(";
				if (arg_types.size()) {
					int j = 0;
					for (List<DataType>::Element *E = arg_types.front(); E; E = E->next()) {
						if (E != arg_types.front()) {
							parent_signature += ", ";
						}
						String arg = E->get().to_string();
						if (arg == "null" || arg == "var") {
							arg = "Variant";
						}
						parent_signature += arg;
						if (j == arg_types.size() - default_arg_count) {
							parent_signature += "=default";
						}

						j++;
					}
				}
				parent_signature += ")";
				_set_error("The function signature doesn't match the parent. Parent signature is: \"" + parent_signature + "\".", p_function->line);
				return;
			}
		}
#endif // DEBUG_ENABLED
	} else {
		if (p_function->return_type.has_type && (p_function->return_type.kind != DataType::BUILTIN || p_function->return_type.builtin_type != Variant::NIL)) {
			_set_error("The constructor can't return a value.", p_function->line);
			return;
		}
	}

	if (p_function->return_type.has_type && (p_function->return_type.kind != DataType::BUILTIN || p_function->return_type.builtin_type != Variant::NIL)) {
		if (!p_function->body->has_return) {
			_set_error("A non-void function must return a value in all possible paths.", p_function->line);
			return;
		}
	}

	if (p_function->has_yield) {
		// yield() will make the function return a GDScriptFunctionState, so the type is ambiguous
		p_function->return_type.has_type = false;
		p_function->return_type.may_yield = true;
	}
}

void GDScriptParser::_check_class_blocks_types(ClassNode *p_class) {
	// Function blocks
	for (int i = 0; i < p_class->static_functions.size(); i++) {
		current_function = p_class->static_functions[i];
		current_block = current_function->body;
		_mark_line_as_safe(current_function->line);
		_check_block_types(current_block);
		current_block = nullptr;
		current_function = nullptr;
		if (error_set) {
			return;
		}
	}

	for (int i = 0; i < p_class->functions.size(); i++) {
		current_function = p_class->functions[i];
		current_block = current_function->body;
		_mark_line_as_safe(current_function->line);
		_check_block_types(current_block);
		current_block = nullptr;
		current_function = nullptr;
		if (error_set) {
			return;
		}
	}

#ifdef DEBUG_ENABLED
	// Warnings
	for (int i = 0; i < p_class->variables.size(); i++) {
		if (p_class->variables[i].usages == 0) {
			_add_warning(GDScriptWarning::UNUSED_CLASS_VARIABLE, p_class->variables[i].line, p_class->variables[i].identifier);
		}
	}
	for (int i = 0; i < p_class->_signals.size(); i++) {
		if (p_class->_signals[i].emissions == 0) {
			_add_warning(GDScriptWarning::UNUSED_SIGNAL, p_class->_signals[i].line, p_class->_signals[i].name);
		}
	}
#endif // DEBUG_ENABLED

	// Inner classes
	for (int i = 0; i < p_class->subclasses.size(); i++) {
		current_class = p_class->subclasses[i];
		_check_class_blocks_types(current_class);
		if (error_set) {
			return;
		}
		current_class = p_class;
	}
}

#ifdef DEBUG_ENABLED
static String _find_function_name(const GDScriptParser::OperatorNode *p_call) {
	switch (p_call->arguments[0]->type) {
		case GDScriptParser::Node::TYPE_TYPE: {
			return Variant::get_type_name(static_cast<GDScriptParser::TypeNode *>(p_call->arguments[0])->vtype);
		} break;
		case GDScriptParser::Node::TYPE_BUILT_IN_FUNCTION: {
			return GDScriptFunctions::get_func_name(static_cast<GDScriptParser::BuiltInFunctionNode *>(p_call->arguments[0])->function);
		} break;
		default: {
			int id_index = p_call->op == GDScriptParser::OperatorNode::OP_PARENT_CALL ? 0 : 1;
			if (p_call->arguments.size() > id_index && p_call->arguments[id_index]->type == GDScriptParser::Node::TYPE_IDENTIFIER) {
				return static_cast<GDScriptParser::IdentifierNode *>(p_call->arguments[id_index])->name;
			}
		} break;
	}
	return String();
}
#endif // DEBUG_ENABLED

void GDScriptParser::_check_block_types(BlockNode *p_block) {
	Node *last_var_assign = nullptr;

	// Check each statement
	for (int z = 0; z < p_block->statements.size(); z++) {
		Node *statement = p_block->statements[z];
		switch (statement->type) {
			case Node::TYPE_NEWLINE:
			case Node::TYPE_BREAKPOINT: {
				// Nothing to do
			} break;
			case Node::TYPE_ASSERT: {
				AssertNode *an = static_cast<AssertNode *>(statement);
				_mark_line_as_safe(an->line);
				_reduce_node_type(an->condition);
				_reduce_node_type(an->message);
			} break;
			case Node::TYPE_LOCAL_VAR: {
				LocalVarNode *lv = static_cast<LocalVarNode *>(statement);
				lv->datatype = _resolve_type(lv->datatype, lv->line);
				_mark_line_as_safe(lv->line);

				last_var_assign = lv->assign;
				if (lv->assign) {
					lv->assign_op->arguments[0]->set_datatype(lv->datatype);
					DataType assign_type = _reduce_node_type(lv->assign);
#ifdef DEBUG_ENABLED
					if (assign_type.has_type && assign_type.kind == DataType::BUILTIN && assign_type.builtin_type == Variant::NIL) {
						if (lv->assign->type == Node::TYPE_OPERATOR) {
							OperatorNode *call = static_cast<OperatorNode *>(lv->assign);
							if (call->op == OperatorNode::OP_CALL || call->op == OperatorNode::OP_PARENT_CALL) {
								_add_warning(GDScriptWarning::VOID_ASSIGNMENT, lv->line, _find_function_name(call));
							}
						}
					}
					if (lv->datatype.has_type && assign_type.may_yield && lv->assign->type == Node::TYPE_OPERATOR) {
						_add_warning(GDScriptWarning::FUNCTION_MAY_YIELD, lv->line, _find_function_name(static_cast<OperatorNode *>(lv->assign)));
					}
					for (int i = 0; i < current_class->variables.size(); i++) {
						if (current_class->variables[i].identifier == lv->name) {
							_add_warning(GDScriptWarning::SHADOWED_VARIABLE, lv->line, lv->name, itos(current_class->variables[i].line));
						}
					}
#endif // DEBUG_ENABLED

					if (!_is_type_compatible(lv->datatype, assign_type)) {
						// Try supertype test
						if (_is_type_compatible(assign_type, lv->datatype)) {
							_mark_line_as_unsafe(lv->line);
						} else {
							// Try implicit conversion
							if (lv->datatype.kind != DataType::BUILTIN || !_is_type_compatible(lv->datatype, assign_type, true)) {
								_set_error("The assigned value type (" + assign_type.to_string() + ") doesn't match the variable's type (" +
												lv->datatype.to_string() + ").",
										lv->line);
								return;
							}
							// Replace assignment with implicit conversion
							BuiltInFunctionNode *convert = alloc_node<BuiltInFunctionNode>();
							convert->line = lv->line;
							convert->function = GDScriptFunctions::TYPE_CONVERT;

							ConstantNode *tgt_type = alloc_node<ConstantNode>();
							tgt_type->line = lv->line;
							tgt_type->value = (int)lv->datatype.builtin_type;

							OperatorNode *convert_call = alloc_node<OperatorNode>();
							convert_call->line = lv->line;
							convert_call->op = OperatorNode::OP_CALL;
							convert_call->arguments.push_back(convert);
							convert_call->arguments.push_back(lv->assign);
							convert_call->arguments.push_back(tgt_type);

							lv->assign = convert_call;
							lv->assign_op->arguments.write[1] = convert_call;
#ifdef DEBUG_ENABLED
							if (lv->datatype.builtin_type == Variant::INT && assign_type.builtin_type == Variant::REAL) {
								_add_warning(GDScriptWarning::NARROWING_CONVERSION, lv->line);
							}
#endif // DEBUG_ENABLED
						}
					}
					if (lv->datatype.infer_type) {
						if (!assign_type.has_type) {
							_set_error("The assigned value doesn't have a set type; the variable type can't be inferred.", lv->line);
							return;
						}
						if (assign_type.kind == DataType::BUILTIN && assign_type.builtin_type == Variant::NIL) {
							_set_error("The variable type cannot be inferred because its value is \"null\".", lv->line);
							return;
						}
						lv->datatype = assign_type;
						lv->datatype.is_constant = false;
					}
					if (lv->datatype.has_type && !assign_type.has_type) {
						_mark_line_as_unsafe(lv->line);
					}
				}
			} break;
			case Node::TYPE_OPERATOR: {
				OperatorNode *op = static_cast<OperatorNode *>(statement);

				switch (op->op) {
					case OperatorNode::OP_ASSIGN:
					case OperatorNode::OP_ASSIGN_ADD:
					case OperatorNode::OP_ASSIGN_SUB:
					case OperatorNode::OP_ASSIGN_MUL:
					case OperatorNode::OP_ASSIGN_DIV:
					case OperatorNode::OP_ASSIGN_MOD:
					case OperatorNode::OP_ASSIGN_SHIFT_LEFT:
					case OperatorNode::OP_ASSIGN_SHIFT_RIGHT:
					case OperatorNode::OP_ASSIGN_BIT_AND:
					case OperatorNode::OP_ASSIGN_BIT_OR:
					case OperatorNode::OP_ASSIGN_BIT_XOR: {
						if (op->arguments.size() < 2) {
							_set_error("Parser bug: operation without enough arguments.", op->line, op->column);
							return;
						}

						if (op->arguments[1] == last_var_assign) {
							// Assignment was already checked
							break;
						}

						_mark_line_as_safe(op->line);

						DataType lh_type = _reduce_node_type(op->arguments[0]);

						if (error_set) {
							return;
						}

						if (check_types) {
							if (!lh_type.has_type) {
								if (op->arguments[0]->type == Node::TYPE_OPERATOR) {
									_mark_line_as_unsafe(op->line);
								}
							}
							if (lh_type.is_constant) {
								_set_error("Can't assign a new value to a constant.", op->line);
								return;
							}
						}

						DataType rh_type;
						if (op->op != OperatorNode::OP_ASSIGN) {
							// Validate operation
							DataType arg_type = _reduce_node_type(op->arguments[1]);
							if (!arg_type.has_type) {
								_mark_line_as_unsafe(op->line);
								break;
							}

							Variant::Operator oper = _get_variant_operation(op->op);
							bool valid = false;
							rh_type = _get_operation_type(oper, lh_type, arg_type, valid);

							if (check_types && !valid) {
								_set_error("Invalid operand types (\"" + lh_type.to_string() + "\" and \"" + arg_type.to_string() +
												"\") to assignment operator \"" + Variant::get_operator_name(oper) + "\".",
										op->line);
								return;
							}
						} else {
							rh_type = _reduce_node_type(op->arguments[1]);
						}
#ifdef DEBUG_ENABLED
						if (rh_type.has_type && rh_type.kind == DataType::BUILTIN && rh_type.builtin_type == Variant::NIL) {
							if (op->arguments[1]->type == Node::TYPE_OPERATOR) {
								OperatorNode *call = static_cast<OperatorNode *>(op->arguments[1]);
								if (call->op == OperatorNode::OP_CALL || call->op == OperatorNode::OP_PARENT_CALL) {
									_add_warning(GDScriptWarning::VOID_ASSIGNMENT, op->line, _find_function_name(call));
								}
							}
						}
						if (lh_type.has_type && rh_type.may_yield && op->arguments[1]->type == Node::TYPE_OPERATOR) {
							_add_warning(GDScriptWarning::FUNCTION_MAY_YIELD, op->line, _find_function_name(static_cast<OperatorNode *>(op->arguments[1])));
						}

#endif // DEBUG_ENABLED
						bool type_match = lh_type.has_type && rh_type.has_type;
						if (check_types && !_is_type_compatible(lh_type, rh_type)) {
							type_match = false;

							// Try supertype test
							if (_is_type_compatible(rh_type, lh_type)) {
								_mark_line_as_unsafe(op->line);
							} else {
								// Try implicit conversion
								if (lh_type.kind != DataType::BUILTIN || !_is_type_compatible(lh_type, rh_type, true)) {
									_set_error("The assigned value's type (" + rh_type.to_string() + ") doesn't match the variable's type (" +
													lh_type.to_string() + ").",
											op->line);
									return;
								}
								if (op->op == OperatorNode::OP_ASSIGN) {
									// Replace assignment with implicit conversion
									BuiltInFunctionNode *convert = alloc_node<BuiltInFunctionNode>();
									convert->line = op->line;
									convert->function = GDScriptFunctions::TYPE_CONVERT;

									ConstantNode *tgt_type = alloc_node<ConstantNode>();
									tgt_type->line = op->line;
									tgt_type->value = (int)lh_type.builtin_type;

									OperatorNode *convert_call = alloc_node<OperatorNode>();
									convert_call->line = op->line;
									convert_call->op = OperatorNode::OP_CALL;
									convert_call->arguments.push_back(convert);
									convert_call->arguments.push_back(op->arguments[1]);
									convert_call->arguments.push_back(tgt_type);

									op->arguments.write[1] = convert_call;

									type_match = true; // Since we are converting, the type is matching
								}
#ifdef DEBUG_ENABLED
								if (lh_type.builtin_type == Variant::INT && rh_type.builtin_type == Variant::REAL) {
									_add_warning(GDScriptWarning::NARROWING_CONVERSION, op->line);
								}
#endif // DEBUG_ENABLED
							}
						}
#ifdef DEBUG_ENABLED
						if (!rh_type.has_type && (op->op != OperatorNode::OP_ASSIGN || lh_type.has_type || op->arguments[0]->type == Node::TYPE_OPERATOR)) {
							_mark_line_as_unsafe(op->line);
						}
#endif // DEBUG_ENABLED
						op->datatype.has_type = type_match;
					} break;
					case OperatorNode::OP_CALL:
					case OperatorNode::OP_PARENT_CALL: {
						_mark_line_as_safe(op->line);
						DataType func_type = _reduce_function_call_type(op);
#ifdef DEBUG_ENABLED
						if (func_type.has_type && (func_type.kind != DataType::BUILTIN || func_type.builtin_type != Variant::NIL)) {
							// Figure out function name for warning
							String func_name = _find_function_name(op);
							if (func_name.empty()) {
								func_name = "<undetected name>";
							}
							_add_warning(GDScriptWarning::RETURN_VALUE_DISCARDED, op->line, func_name);
						}
#endif // DEBUG_ENABLED
						if (error_set) {
							return;
						}
					} break;
					case OperatorNode::OP_YIELD: {
						_mark_line_as_safe(op->line);
						_reduce_node_type(op);
					} break;
					default: {
						_mark_line_as_safe(op->line);
						_reduce_node_type(op); // Test for safety anyway
#ifdef DEBUG_ENABLED
						if (op->op == OperatorNode::OP_TERNARY_IF) {
							_add_warning(GDScriptWarning::STANDALONE_TERNARY, statement->line);
						} else {
							_add_warning(GDScriptWarning::STANDALONE_EXPRESSION, statement->line);
						}
#endif // DEBUG_ENABLED
					}
				}
			} break;
			case Node::TYPE_CONTROL_FLOW: {
				ControlFlowNode *cf = static_cast<ControlFlowNode *>(statement);
				_mark_line_as_safe(cf->line);

				switch (cf->cf_type) {
					case ControlFlowNode::CF_RETURN: {
						DataType function_type = current_function->get_datatype();

						DataType ret_type;
						if (cf->arguments.size() > 0) {
							ret_type = _reduce_node_type(cf->arguments[0]);
							if (error_set) {
								return;
							}
						}

						if (!function_type.has_type) {
							break;
						}

						if (function_type.kind == DataType::BUILTIN && function_type.builtin_type == Variant::NIL) {
							// Return void, should not have arguments
							if (cf->arguments.size() > 0) {
								_set_error("A void function cannot return a value.", cf->line, cf->column);
								return;
							}
						} else {
							// Return something, cannot be empty
							if (cf->arguments.size() == 0) {
								_set_error("A non-void function must return a value.", cf->line, cf->column);
								return;
							}

							if (!_is_type_compatible(function_type, ret_type)) {
								_set_error("The returned value type (" + ret_type.to_string() + ") doesn't match the function return type (" +
												function_type.to_string() + ").",
										cf->line, cf->column);
								return;
							}
						}
					} break;
					case ControlFlowNode::CF_MATCH: {
						MatchNode *match_node = cf->match;
						_transform_match_statment(match_node);
					} break;
					default: {
						if (cf->body_else) {
							_mark_line_as_safe(cf->body_else->line);
						}
						for (int i = 0; i < cf->arguments.size(); i++) {
							_reduce_node_type(cf->arguments[i]);
						}
					} break;
				}
			} break;
			case Node::TYPE_CONSTANT: {
				ConstantNode *cn = static_cast<ConstantNode *>(statement);
				// Strings are fine since they can be multiline comments
				if (cn->value.get_type() == Variant::STRING) {
					break;
				}
				FALLTHROUGH;
			}
			default: {
				_mark_line_as_safe(statement->line);
				_reduce_node_type(statement); // Test for safety anyway
#ifdef DEBUG_ENABLED
				_add_warning(GDScriptWarning::STANDALONE_EXPRESSION, statement->line);
#endif // DEBUG_ENABLED
			}
		}
	}

	// Parse sub blocks
	for (const List<BlockNode *>::Element *E = p_block->sub_blocks.front(); E; E = E->next()) {
		current_block = E->get();
		_check_block_types(current_block);
		current_block = p_block;
		if (error_set) {
			return;
		}
	}

#ifdef DEBUG_ENABLED
	// Warnings check
	for (Map<StringName, LocalVarNode *>::Element *E = p_block->variables.front(); E; E = E->next()) {
		LocalVarNode *lv = E->get();
		if (!lv->name.operator String().begins_with("_")) {
			if (lv->usages == 0) {
				_add_warning(GDScriptWarning::UNUSED_VARIABLE, lv->line, lv->name);
			} else if (lv->assignments == 0) {
				_add_warning(GDScriptWarning::UNASSIGNED_VARIABLE, lv->line, lv->name);
			}
		}
	}
#endif // DEBUG_ENABLED
}

void GDScriptParser::_set_error(const String &p_error, int p_line, int p_column) {
	if (error_set) {
		return; //allow no further errors
	}

	error = p_error;
	error_line = p_line < 0 ? tokenizer->get_token_line() : p_line;
	error_column = p_column < 0 ? tokenizer->get_token_column() : p_column;
	error_set = true;
}

#ifdef DEBUG_ENABLED
void GDScriptParser::_add_warning(int p_code, int p_line, const String &p_symbol1, const String &p_symbol2, const String &p_symbol3, const String &p_symbol4) {
	Vector<String> symbols;
	if (!p_symbol1.empty()) {
		symbols.push_back(p_symbol1);
	}
	if (!p_symbol2.empty()) {
		symbols.push_back(p_symbol2);
	}
	if (!p_symbol3.empty()) {
		symbols.push_back(p_symbol3);
	}
	if (!p_symbol4.empty()) {
		symbols.push_back(p_symbol4);
	}
	_add_warning(p_code, p_line, symbols);
}

void GDScriptParser::_add_warning(int p_code, int p_line, const Vector<String> &p_symbols) {
	if (GLOBAL_GET("debug/gdscript/warnings/exclude_addons").booleanize() && base_path.begins_with("res://addons/")) {
		return;
	}
	if (tokenizer->is_ignoring_warnings() || !GLOBAL_GET("debug/gdscript/warnings/enable").booleanize()) {
		return;
	}
	String warn_name = GDScriptWarning::get_name_from_code((GDScriptWarning::Code)p_code).to_lower();
	if (tokenizer->get_warning_global_skips().has(warn_name)) {
		return;
	}
	if (!GLOBAL_GET("debug/gdscript/warnings/" + warn_name)) {
		return;
	}

	GDScriptWarning warn;
	warn.code = (GDScriptWarning::Code)p_code;
	warn.symbols = p_symbols;
	warn.line = p_line == -1 ? tokenizer->get_token_line() : p_line;

	List<GDScriptWarning>::Element *before = nullptr;
	for (List<GDScriptWarning>::Element *E = warnings.front(); E; E = E->next()) {
		if (E->get().line > warn.line) {
			break;
		}
		before = E;
	}
	if (before) {
		warnings.insert_after(before, warn);
	} else {
		warnings.push_front(warn);
	}
}
#endif // DEBUG_ENABLED

String GDScriptParser::get_error() const {
	return error;
}

int GDScriptParser::get_error_line() const {
	return error_line;
}
int GDScriptParser::get_error_column() const {
	return error_column;
}

bool GDScriptParser::has_error() const {
	return error_set;
}

Error GDScriptParser::_parse(const String &p_base_path) {
	base_path = p_base_path;

	//assume class
	ClassNode *main_class = alloc_node<ClassNode>();
	main_class->initializer = alloc_node<BlockNode>();
	main_class->initializer->parent_class = main_class;
	main_class->ready = alloc_node<BlockNode>();
	main_class->ready->parent_class = main_class;
	current_class = main_class;

	_parse_class(main_class);

	if (tokenizer->get_token() == GDScriptTokenizer::TK_ERROR) {
		error_set = false;
		_set_error("Parse error: " + tokenizer->get_token_error());
	}

	bool for_completion_error_set = false;
	if (error_set && for_completion) {
		for_completion_error_set = true;
		error_set = false;
	}

	if (error_set) {
		return ERR_PARSE_ERROR;
	}

	if (dependencies_only) {
		return OK;
	}

	_determine_inheritance(main_class);

	if (error_set) {
		return ERR_PARSE_ERROR;
	}

	current_class = main_class;
	current_function = nullptr;
	current_block = nullptr;

	if (for_completion) {
		check_types = false;
	}

	// Resolve all class-level stuff before getting into function blocks
	_check_class_level_types(main_class);

	if (error_set) {
		return ERR_PARSE_ERROR;
	}

	// Resolve the function blocks
	_check_class_blocks_types(main_class);

	if (for_completion_error_set) {
		error_set = true;
	}

	if (error_set) {
		return ERR_PARSE_ERROR;
	}

#ifdef DEBUG_ENABLED

	// Resolve warning ignores
	Vector<Pair<int, String>> warning_skips = tokenizer->get_warning_skips();
	bool warning_is_error = GLOBAL_GET("debug/gdscript/warnings/treat_warnings_as_errors").booleanize();
	for (List<GDScriptWarning>::Element *E = warnings.front(); E;) {
		GDScriptWarning &w = E->get();
		int skip_index = -1;
		for (int i = 0; i < warning_skips.size(); i++) {
			if (warning_skips[i].first > w.line) {
				break;
			}
			skip_index = i;
		}
		List<GDScriptWarning>::Element *next = E->next();
		bool erase = false;
		if (skip_index != -1) {
			if (warning_skips[skip_index].second == GDScriptWarning::get_name_from_code(w.code).to_lower()) {
				erase = true;
			}
			warning_skips.remove(skip_index);
		}
		if (erase) {
			warnings.erase(E);
		} else if (warning_is_error) {
			_set_error(w.get_message() + " (warning treated as error)", w.line);
			return ERR_PARSE_ERROR;
		}
		E = next;
	}
#endif // DEBUG_ENABLED

	return OK;
}

Error GDScriptParser::parse_bytecode(const Vector<uint8_t> &p_bytecode, const String &p_base_path, const String &p_self_path) {
	clear();

	self_path = p_self_path;
	GDScriptTokenizerBuffer *tb = memnew(GDScriptTokenizerBuffer);
	tb->set_code_buffer(p_bytecode);
	tokenizer = tb;
	Error ret = _parse(p_base_path);
	memdelete(tb);
	tokenizer = nullptr;
	return ret;
}

Error GDScriptParser::parse(const String &p_code, const String &p_base_path, bool p_just_validate, const String &p_self_path, bool p_for_completion, Set<int> *r_safe_lines, bool p_dependencies_only) {
	clear();

	self_path = p_self_path;
	GDScriptTokenizerText *tt = memnew(GDScriptTokenizerText);
	tt->set_code(p_code);

	validating = p_just_validate;
	for_completion = p_for_completion;
	dependencies_only = p_dependencies_only;
#ifdef DEBUG_ENABLED
	safe_lines = r_safe_lines;
#endif // DEBUG_ENABLED
	tokenizer = tt;
	Error ret = _parse(p_base_path);
	memdelete(tt);
	tokenizer = nullptr;
	return ret;
}

bool GDScriptParser::is_tool_script() const {
	return (head && head->type == Node::TYPE_CLASS && static_cast<const ClassNode *>(head)->tool);
}

const GDScriptParser::Node *GDScriptParser::get_parse_tree() const {
	return head;
}

void GDScriptParser::clear() {
	while (list) {
		Node *l = list;
		list = list->next;
		memdelete(l);
	}

	head = nullptr;
	list = nullptr;

	completion_type = COMPLETION_NONE;
	completion_node = nullptr;
	completion_class = nullptr;
	completion_function = nullptr;
	completion_block = nullptr;
	current_block = nullptr;
	current_class = nullptr;

	completion_found = false;
	rpc_mode = MultiplayerAPI::RPC_MODE_DISABLED;

	current_function = nullptr;

	validating = false;
	for_completion = false;
	error_set = false;
	indent_level.clear();
	indent_level.push_back(IndentLevel(0, 0));
	error_line = 0;
	error_column = 0;
	pending_newline = -1;
	parenthesis = 0;
	current_export.type = Variant::NIL;
	check_types = true;
	dependencies_only = false;
	dependencies.clear();
	error = "";
#ifdef DEBUG_ENABLED
	safe_lines = nullptr;
#endif // DEBUG_ENABLED
}

GDScriptParser::CompletionType GDScriptParser::get_completion_type() {
	return completion_type;
}

StringName GDScriptParser::get_completion_cursor() {
	return completion_cursor;
}

int GDScriptParser::get_completion_line() {
	return completion_line;
}

Variant::Type GDScriptParser::get_completion_built_in_constant() {
	return completion_built_in_constant;
}

GDScriptParser::Node *GDScriptParser::get_completion_node() {
	return completion_node;
}

GDScriptParser::BlockNode *GDScriptParser::get_completion_block() {
	return completion_block;
}

GDScriptParser::ClassNode *GDScriptParser::get_completion_class() {
	return completion_class;
}

GDScriptParser::FunctionNode *GDScriptParser::get_completion_function() {
	return completion_function;
}

int GDScriptParser::get_completion_argument_index() {
	return completion_argument;
}

int GDScriptParser::get_completion_identifier_is_function() {
	return completion_ident_is_call;
}

GDScriptParser::GDScriptParser() {
	head = nullptr;
	list = nullptr;
	tokenizer = nullptr;
	pending_newline = -1;
	clear();
}

GDScriptParser::~GDScriptParser() {
	clear();
}
