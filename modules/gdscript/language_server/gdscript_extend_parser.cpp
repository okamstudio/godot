/**************************************************************************/
/*  gdscript_extend_parser.cpp                                            */
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

#include "gdscript_extend_parser.h"

#include "../gdscript.h"
#include "../gdscript_analyzer.h"
#include "editor/editor_settings.h"
#include "gdscript_language_protocol.h"
#include "gdscript_workspace.h"

int get_indent_size() {
	if (EditorSettings::get_singleton()) {
		return EditorSettings::get_singleton()->get_setting("text_editor/behavior/indent/size");
	} else {
		return 4;
	}
}

lsp::Position GodotPosition::to_lsp(const Vector<String> &p_lines) const {
	lsp::Position res;

	// Special case: `line = 0` -> root class (range covers everything).
	if (line <= 0) {
		return res;
	}
	// Special case: `line = p_lines.size() + 1` -> root class (range covers everything).
	if (line >= p_lines.size() + 1) {
		res.line = p_lines.size();
		return res;
	}
	res.line = line - 1;

	// Special case: `column = 0` -> Starts at beginning of line.
	if (column <= 0) {
		return res;
	}

	// Note: character outside of `pos_line.length()-1` is valid.
	res.character = column - 1;

	String pos_line = p_lines[res.line];
	if (pos_line.contains_char('\t')) {
		int tab_size = get_indent_size();

		int in_col = 1;
		int res_char = 0;

		while (res_char < pos_line.size() && in_col < column) {
			if (pos_line[res_char] == '\t') {
				in_col += tab_size;
				res_char++;
			} else {
				in_col++;
				res_char++;
			}
		}

		res.character = res_char;
	}

	return res;
}

GodotPosition GodotPosition::from_lsp(const lsp::Position p_pos, const Vector<String> &p_lines) {
	GodotPosition res(p_pos.line + 1, p_pos.character + 1);

	// Line outside of actual text is valid (-> pos/cursor at end of text).
	if (res.line > p_lines.size()) {
		return res;
	}

	String line = p_lines[p_pos.line];
	int tabs_before_char = 0;
	for (int i = 0; i < p_pos.character && i < line.length(); i++) {
		if (line[i] == '\t') {
			tabs_before_char++;
		}
	}

	if (tabs_before_char > 0) {
		int tab_size = get_indent_size();
		res.column += tabs_before_char * (tab_size - 1);
	}

	return res;
}

lsp::Range GodotRange::to_lsp(const Vector<String> &p_lines) const {
	lsp::Range res;
	res.start = start.to_lsp(p_lines);
	res.end = end.to_lsp(p_lines);
	return res;
}

GodotRange GodotRange::from_lsp(const lsp::Range &p_range, const Vector<String> &p_lines) {
	GodotPosition start = GodotPosition::from_lsp(p_range.start, p_lines);
	GodotPosition end = GodotPosition::from_lsp(p_range.end, p_lines);
	return GodotRange(start, end);
}

void ExtendGDScriptParser::update_diagnostics() {
	diagnostics.clear();

	const List<ParserError> &parser_errors = get_errors();
	for (const ParserError &error : parser_errors) {
		lsp::Diagnostic diagnostic;
		diagnostic.severity = lsp::DiagnosticSeverity::Error;
		diagnostic.message = error.message;
		diagnostic.source = "gdscript";
		diagnostic.code = -1;
		lsp::Range range;
		lsp::Position pos;
		const PackedStringArray line_array = get_lines();
		int line = CLAMP(LINE_NUMBER_TO_INDEX(error.line), 0, line_array.size() - 1);
		const String &line_text = line_array[line];
		pos.line = line;
		pos.character = line_text.length() - line_text.strip_edges(true, false).length();
		range.start = pos;
		range.end = range.start;
		range.end.character = line_text.strip_edges(false).length();
		diagnostic.range = range;
		diagnostics.push_back(diagnostic);
	}

	const List<GDScriptWarning> &parser_warnings = get_warnings();
	for (const GDScriptWarning &warning : parser_warnings) {
		lsp::Diagnostic diagnostic;
		diagnostic.severity = lsp::DiagnosticSeverity::Warning;
		diagnostic.message = "(" + warning.get_name() + "): " + warning.get_message();
		diagnostic.source = "gdscript";
		diagnostic.code = warning.code;
		lsp::Range range;
		lsp::Position pos;
		int line = LINE_NUMBER_TO_INDEX(warning.start_line);
		const String &line_text = get_lines()[line];
		pos.line = line;
		pos.character = line_text.length() - line_text.strip_edges(true, false).length();
		range.start = pos;
		range.end = pos;
		range.end.character = line_text.strip_edges(false).length();
		diagnostic.range = range;
		diagnostics.push_back(diagnostic);
	}
}

void ExtendGDScriptParser::update_symbols() {
	members.clear();

	if (const GDScriptParser::ClassNode *gdclass = dynamic_cast<const GDScriptParser::ClassNode *>(get_tree())) {
		parse_class_symbol(gdclass, class_symbol);

		for (int i = 0; i < class_symbol.children.size(); i++) {
			const lsp::DocumentSymbol &symbol = class_symbol.children[i];
			members.insert(symbol.name, &symbol);

			// Cache level one inner classes.
			if (symbol.kind == lsp::SymbolKind::Class) {
				ClassMembers inner_class;
				for (int j = 0; j < symbol.children.size(); j++) {
					const lsp::DocumentSymbol &s = symbol.children[j];
					inner_class.insert(s.name, &s);
				}
				inner_classes.insert(symbol.name, inner_class);
			}
		}
	}
}

void ExtendGDScriptParser::update_document_links(const String &p_code) {
	document_links.clear();

	GDScriptTokenizerText scr_tokenizer;
	Ref<FileAccess> fs = FileAccess::create(FileAccess::ACCESS_RESOURCES);
	scr_tokenizer.set_source_code(p_code);
	while (true) {
		GDScriptTokenizer::Token token = scr_tokenizer.scan();
		if (token.type == GDScriptTokenizer::Token::TK_EOF) {
			break;
		} else if (token.type == GDScriptTokenizer::Token::LITERAL) {
			const Variant &const_val = token.literal;
			if (const_val.get_type() == Variant::STRING) {
				String scr_path = const_val;
				bool exists = fs->file_exists(scr_path);
				if (!exists) {
					scr_path = get_path().get_base_dir() + "/" + scr_path;
					exists = fs->file_exists(scr_path);
				}
				if (exists) {
					String value = const_val;
					lsp::DocumentLink link;
					link.target = GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_uri(scr_path);
					link.range = GodotRange(GodotPosition(token.start_line, token.start_column), GodotPosition(token.end_line, token.end_column)).to_lsp(lines);
					document_links.push_back(link);
				}
			}
		}
	}
}

lsp::Range ExtendGDScriptParser::range_of_node(const GDScriptParser::Node *p_node) const {
	GodotPosition start(p_node->start_line, p_node->start_column);
	GodotPosition end(p_node->end_line, p_node->end_column);
	return GodotRange(start, end).to_lsp(lines);
}

void ExtendGDScriptParser::parse_class_symbol(const GDScriptParser::ClassNode *p_class, lsp::DocumentSymbol &r_symbol) {
	const String uri = get_uri();

	r_symbol.uri = uri;
	r_symbol.script_path = path;
	r_symbol.children.clear();
	r_symbol.name = p_class->identifier != nullptr ? String(p_class->identifier->name) : String();
	if (r_symbol.name.is_empty()) {
		r_symbol.name = path.get_file();
	}
	r_symbol.kind = lsp::SymbolKind::Class;
	r_symbol.deprecated = false;
	r_symbol.range = range_of_node(p_class);
	if (p_class->identifier) {
		r_symbol.selectionRange = range_of_node(p_class->identifier);
	} else {
		// No meaningful `selectionRange`, but we must ensure that it is inside of `range`.
		r_symbol.selectionRange.start = r_symbol.range.start;
		r_symbol.selectionRange.end = r_symbol.range.start;
	}
	r_symbol.detail = "class " + r_symbol.name;
	{
		String doc = p_class->doc_data.description;
		if (!p_class->doc_data.description.is_empty()) {
			doc += "\n\n" + p_class->doc_data.description;
		}

		if (!p_class->doc_data.tutorials.is_empty()) {
			doc += "\n";
			for (const Pair<String, String> &tutorial : p_class->doc_data.tutorials) {
				if (tutorial.first.is_empty()) {
					doc += vformat("\n@tutorial: %s", tutorial.second);
				} else {
					doc += vformat("\n@tutorial(%s): %s", tutorial.first, tutorial.second);
				}
			}
		}
		r_symbol.documentation = doc;
	}

	for (int i = 0; i < p_class->members.size(); i++) {
		const ClassNode::Member &m = p_class->members[i];

		switch (m.type) {
			case ClassNode::Member::VARIABLE: {
				lsp::DocumentSymbol symbol;
				symbol.name = m.variable->identifier->name;
				symbol.kind = m.variable->property == VariableNode::PROP_NONE ? lsp::SymbolKind::Variable : lsp::SymbolKind::Property;
				symbol.deprecated = false;
				symbol.range = range_of_node(m.variable);
				symbol.selectionRange = range_of_node(m.variable->identifier);
				if (m.variable->exported) {
					symbol.detail += "@export ";
				}
				symbol.detail += "var " + m.variable->identifier->name;
				if (m.get_datatype().is_hard_type()) {
					symbol.detail += ": " + m.get_datatype().to_string();
				}
				if (m.variable->initializer != nullptr && m.variable->initializer->is_constant) {
					symbol.detail += " = " + m.variable->initializer->reduced_value.to_json_string();
				}

				symbol.documentation = m.variable->doc_data.description;
				symbol.uri = uri;
				symbol.script_path = path;

				if (m.variable->initializer && m.variable->initializer->type == GDScriptParser::Node::LAMBDA) {
					GDScriptParser::LambdaNode *lambda_node = (GDScriptParser::LambdaNode *)m.variable->initializer;
					lsp::DocumentSymbol lambda;
					parse_function_symbol(lambda_node->function, lambda);
					// Merge lambda into current variable.
					symbol.children.append_array(lambda.children);
				}

				if (m.variable->getter && m.variable->getter->type == GDScriptParser::Node::FUNCTION) {
					lsp::DocumentSymbol get_symbol;
					parse_function_symbol(m.variable->getter, get_symbol);
					get_symbol.local = true;
					symbol.children.push_back(get_symbol);
				}
				if (m.variable->setter && m.variable->setter->type == GDScriptParser::Node::FUNCTION) {
					lsp::DocumentSymbol set_symbol;
					parse_function_symbol(m.variable->setter, set_symbol);
					set_symbol.local = true;
					symbol.children.push_back(set_symbol);
				}

				r_symbol.children.push_back(symbol);
			} break;
			case ClassNode::Member::CONSTANT: {
				lsp::DocumentSymbol symbol;

				symbol.name = m.constant->identifier->name;
				symbol.kind = lsp::SymbolKind::Constant;
				symbol.deprecated = false;
				symbol.range = range_of_node(m.constant);
				symbol.selectionRange = range_of_node(m.constant->identifier);
				symbol.documentation = m.constant->doc_data.description;
				symbol.uri = uri;
				symbol.script_path = path;

				symbol.detail = "const " + symbol.name;
				if (m.constant->get_datatype().is_hard_type()) {
					symbol.detail += ": " + m.constant->get_datatype().to_string();
				}

				const Variant &default_value = m.constant->initializer->reduced_value;
				String value_text;
				if (default_value.get_type() == Variant::OBJECT) {
					Ref<Resource> res = default_value;
					if (res.is_valid() && !res->get_path().is_empty()) {
						value_text = "preload(\"" + res->get_path() + "\")";
						if (symbol.documentation.is_empty()) {
							if (HashMap<String, ExtendGDScriptParser *>::Iterator S = GDScriptLanguageProtocol::get_singleton()->get_workspace()->scripts.find(res->get_path())) {
								symbol.documentation = S->value->class_symbol.documentation;
							}
						}
					} else {
						value_text = default_value.to_json_string();
					}
				} else {
					value_text = default_value.to_json_string();
				}
				if (!value_text.is_empty()) {
					symbol.detail += " = " + value_text;
				}

				r_symbol.children.push_back(symbol);
			} break;
			case ClassNode::Member::SIGNAL: {
				lsp::DocumentSymbol symbol;
				symbol.name = m.signal->identifier->name;
				symbol.kind = lsp::SymbolKind::Event;
				symbol.deprecated = false;
				symbol.range = range_of_node(m.signal);
				symbol.selectionRange = range_of_node(m.signal->identifier);
				symbol.documentation = m.signal->doc_data.description;
				symbol.uri = uri;
				symbol.script_path = path;
				symbol.detail = "signal " + String(m.signal->identifier->name) + "(";
				for (int j = 0; j < m.signal->parameters.size(); j++) {
					if (j > 0) {
						symbol.detail += ", ";
					}
					symbol.detail += m.signal->parameters[j]->identifier->name;
				}
				symbol.detail += ")";

				for (GDScriptParser::ParameterNode *param : m.signal->parameters) {
					lsp::DocumentSymbol param_symbol;
					param_symbol.name = param->identifier->name;
					param_symbol.kind = lsp::SymbolKind::Variable;
					param_symbol.deprecated = false;
					param_symbol.local = true;
					param_symbol.range = range_of_node(param);
					param_symbol.selectionRange = range_of_node(param->identifier);
					param_symbol.uri = uri;
					param_symbol.script_path = path;
					param_symbol.detail = "var " + param_symbol.name;
					if (param->get_datatype().is_hard_type()) {
						param_symbol.detail += ": " + param->get_datatype().to_string();
					}
					symbol.children.push_back(param_symbol);
				}
				r_symbol.children.push_back(symbol);
			} break;
			case ClassNode::Member::ENUM_VALUE: {
				lsp::DocumentSymbol symbol;

				symbol.name = m.enum_value.identifier->name;
				symbol.kind = lsp::SymbolKind::EnumMember;
				symbol.deprecated = false;
				symbol.range.start = GodotPosition(m.enum_value.line, m.enum_value.leftmost_column).to_lsp(lines);
				symbol.range.end = GodotPosition(m.enum_value.line, m.enum_value.rightmost_column).to_lsp(lines);
				symbol.selectionRange = range_of_node(m.enum_value.identifier);
				symbol.documentation = m.enum_value.doc_data.description;
				symbol.uri = uri;
				symbol.script_path = path;

				symbol.detail = symbol.name + " = " + itos(m.enum_value.value);

				r_symbol.children.push_back(symbol);
			} break;
			case ClassNode::Member::ENUM: {
				lsp::DocumentSymbol symbol;
				symbol.name = m.m_enum->identifier->name;
				symbol.kind = lsp::SymbolKind::Enum;
				symbol.range = range_of_node(m.m_enum);
				symbol.selectionRange = range_of_node(m.m_enum->identifier);
				symbol.documentation = m.m_enum->doc_data.description;
				symbol.uri = uri;
				symbol.script_path = path;

				symbol.detail = "enum " + String(m.m_enum->identifier->name) + "{";
				for (int j = 0; j < m.m_enum->values.size(); j++) {
					if (j > 0) {
						symbol.detail += ", ";
					}
					symbol.detail += String(m.m_enum->values[j].identifier->name) + " = " + itos(m.m_enum->values[j].value);
				}
				symbol.detail += "}";

				for (GDScriptParser::EnumNode::Value value : m.m_enum->values) {
					lsp::DocumentSymbol child;

					child.name = value.identifier->name;
					child.kind = lsp::SymbolKind::EnumMember;
					child.deprecated = false;
					child.range.start = GodotPosition(value.line, value.leftmost_column).to_lsp(lines);
					child.range.end = GodotPosition(value.line, value.rightmost_column).to_lsp(lines);
					child.selectionRange = range_of_node(value.identifier);
					child.documentation = value.doc_data.description;
					child.uri = uri;
					child.script_path = path;

					child.detail = child.name + " = " + itos(value.value);

					symbol.children.push_back(child);
				}

				r_symbol.children.push_back(symbol);
			} break;
			case ClassNode::Member::FUNCTION: {
				lsp::DocumentSymbol symbol;
				parse_function_symbol(m.function, symbol);
				r_symbol.children.push_back(symbol);
			} break;
			case ClassNode::Member::CLASS: {
				lsp::DocumentSymbol symbol;
				parse_class_symbol(m.m_class, symbol);
				r_symbol.children.push_back(symbol);
			} break;
			case ClassNode::Member::GROUP:
				break; // No-op, but silences warnings.
			case ClassNode::Member::UNDEFINED:
				break; // Unreachable.
		}
	}
}

void ExtendGDScriptParser::parse_function_symbol(const GDScriptParser::FunctionNode *p_func, lsp::DocumentSymbol &r_symbol) {
	const String uri = get_uri();

	bool is_named = p_func->identifier != nullptr;

	r_symbol.name = is_named ? p_func->identifier->name : "";
	r_symbol.kind = (p_func->is_static || p_func->source_lambda != nullptr) ? lsp::SymbolKind::Function : lsp::SymbolKind::Method;
	r_symbol.detail = "func";
	if (is_named) {
		r_symbol.detail += " " + String(p_func->identifier->name);
	}
	r_symbol.detail += "(";
	r_symbol.deprecated = false;
	r_symbol.range = range_of_node(p_func);
	if (is_named) {
		r_symbol.selectionRange = range_of_node(p_func->identifier);
	} else {
		r_symbol.selectionRange.start = r_symbol.selectionRange.end = r_symbol.range.start;
	}
	r_symbol.documentation = p_func->doc_data.description;
	r_symbol.uri = uri;
	r_symbol.script_path = path;

	String parameters;
	for (int i = 0; i < p_func->parameters.size(); i++) {
		const ParameterNode *parameter = p_func->parameters[i];
		if (i > 0) {
			parameters += ", ";
		}
		parameters += String(parameter->identifier->name);
		if (parameter->get_datatype().is_hard_type()) {
			parameters += ": " + parameter->get_datatype().to_string();
		}
		if (parameter->initializer != nullptr) {
			parameters += " = " + parameter->initializer->reduced_value.to_json_string();
		}
	}
	r_symbol.detail += parameters + ")";
	if (p_func->get_datatype().is_hard_type()) {
		r_symbol.detail += " -> " + p_func->get_datatype().to_string();
	}

	List<GDScriptParser::SuiteNode *> function_nodes;

	List<GDScriptParser::Node *> node_stack;
	node_stack.push_back(p_func->body);

	while (!node_stack.is_empty()) {
		GDScriptParser::Node *node = node_stack.front()->get();
		node_stack.pop_front();

		switch (node->type) {
			case GDScriptParser::TypeNode::IF: {
				GDScriptParser::IfNode *if_node = (GDScriptParser::IfNode *)node;
				node_stack.push_back(if_node->true_block);
				if (if_node->false_block) {
					node_stack.push_back(if_node->false_block);
				}
			} break;

			case GDScriptParser::TypeNode::FOR: {
				GDScriptParser::ForNode *for_node = (GDScriptParser::ForNode *)node;
				node_stack.push_back(for_node->loop);
			} break;

			case GDScriptParser::TypeNode::WHILE: {
				GDScriptParser::WhileNode *while_node = (GDScriptParser::WhileNode *)node;
				node_stack.push_back(while_node->loop);
			} break;

			case GDScriptParser::TypeNode::MATCH: {
				GDScriptParser::MatchNode *match_node = (GDScriptParser::MatchNode *)node;
				for (GDScriptParser::MatchBranchNode *branch_node : match_node->branches) {
					node_stack.push_back(branch_node);
				}
			} break;

			case GDScriptParser::TypeNode::MATCH_BRANCH: {
				GDScriptParser::MatchBranchNode *match_node = (GDScriptParser::MatchBranchNode *)node;
				node_stack.push_back(match_node->block);
			} break;

			case GDScriptParser::TypeNode::SUITE: {
				GDScriptParser::SuiteNode *suite_node = (GDScriptParser::SuiteNode *)node;
				function_nodes.push_back(suite_node);
				for (int i = 0; i < suite_node->statements.size(); ++i) {
					node_stack.push_back(suite_node->statements[i]);
				}
			} break;

			default:
				continue;
		}
	}

	for (List<GDScriptParser::SuiteNode *>::Element *N = function_nodes.front(); N; N = N->next()) {
		const GDScriptParser::SuiteNode *suite_node = N->get();
		for (int i = 0; i < suite_node->locals.size(); i++) {
			const SuiteNode::Local &local = suite_node->locals[i];
			lsp::DocumentSymbol symbol;
			symbol.name = local.name;
			symbol.kind = local.type == SuiteNode::Local::CONSTANT ? lsp::SymbolKind::Constant : lsp::SymbolKind::Variable;
			switch (local.type) {
				case SuiteNode::Local::CONSTANT:
					symbol.range = range_of_node(local.constant);
					symbol.selectionRange = range_of_node(local.constant->identifier);
					break;
				case SuiteNode::Local::VARIABLE:
					symbol.range = range_of_node(local.variable);
					symbol.selectionRange = range_of_node(local.variable->identifier);
					if (local.variable->initializer && local.variable->initializer->type == GDScriptParser::Node::LAMBDA) {
						GDScriptParser::LambdaNode *lambda_node = (GDScriptParser::LambdaNode *)local.variable->initializer;
						lsp::DocumentSymbol lambda;
						parse_function_symbol(lambda_node->function, lambda);
						// Merge lambda into current variable.
						// -> Only interested in new variables, not lambda itself.
						symbol.children.append_array(lambda.children);
					}
					break;
				case SuiteNode::Local::PARAMETER:
					symbol.range = range_of_node(local.parameter);
					symbol.selectionRange = range_of_node(local.parameter->identifier);
					break;
				case SuiteNode::Local::FOR_VARIABLE:
				case SuiteNode::Local::PATTERN_BIND:
					symbol.range = range_of_node(local.bind);
					symbol.selectionRange = range_of_node(local.bind);
					break;
				default:
					// Fallback.
					symbol.range.start = GodotPosition(local.start_line, local.start_column).to_lsp(get_lines());
					symbol.range.end = GodotPosition(local.end_line, local.end_column).to_lsp(get_lines());
					symbol.selectionRange = symbol.range;
					break;
			}
			symbol.local = true;
			symbol.uri = uri;
			symbol.script_path = path;
			symbol.detail = local.type == SuiteNode::Local::CONSTANT ? "const " : "var ";
			symbol.detail += symbol.name;
			if (local.get_datatype().is_hard_type()) {
				symbol.detail += ": " + local.get_datatype().to_string();
			}
			switch (local.type) {
				case SuiteNode::Local::CONSTANT:
					symbol.documentation = local.constant->doc_data.description;
					break;
				case SuiteNode::Local::VARIABLE:
					symbol.documentation = local.variable->doc_data.description;
					break;
				default:
					break;
			}
			r_symbol.children.push_back(symbol);
		}
	}
}

String ExtendGDScriptParser::get_text_for_completion(const lsp::Position &p_cursor) const {
	String longthing;
	int len = lines.size();
	for (int i = 0; i < len; i++) {
		if (i == p_cursor.line) {
			longthing += lines[i].substr(0, p_cursor.character);
			longthing += String::chr(0xFFFF); // Not unicode, represents the cursor.
			longthing += lines[i].substr(p_cursor.character, lines[i].size());
		} else {
			longthing += lines[i];
		}

		if (i != len - 1) {
			longthing += "\n";
		}
	}

	return longthing;
}

String ExtendGDScriptParser::get_text_for_lookup_symbol(const lsp::Position &p_cursor, const String &p_symbol, bool p_func_required) const {
	String longthing;
	int len = lines.size();
	for (int i = 0; i < len; i++) {
		if (i == p_cursor.line) {
			String line = lines[i];
			String first_part = line.substr(0, p_cursor.character);
			String last_part = line.substr(p_cursor.character, lines[i].length());
			if (!p_symbol.is_empty()) {
				String left_cursor_text;
				for (int c = p_cursor.character - 1; c >= 0; c--) {
					left_cursor_text = line.substr(c, p_cursor.character - c);
					if (p_symbol.begins_with(left_cursor_text)) {
						first_part = line.substr(0, c);
						first_part += p_symbol;
						break;
					}
				}
			}

			longthing += first_part;
			longthing += String::chr(0xFFFF); // Not unicode, represents the cursor.
			if (p_func_required) {
				longthing += "("; // Tell the parser this is a function call.
			}
			longthing += last_part;
		} else {
			longthing += lines[i];
		}

		if (i != len - 1) {
			longthing += "\n";
		}
	}

	return longthing;
}

String ExtendGDScriptParser::get_identifier_under_position(const lsp::Position &p_position, lsp::Range &r_range) const {
	ERR_FAIL_INDEX_V(p_position.line, lines.size(), "");
	String line = lines[p_position.line];
	if (line.is_empty()) {
		return "";
	}
	ERR_FAIL_INDEX_V(p_position.character, line.size(), "");

	// `p_position` cursor is BETWEEN chars, not ON chars.
	// ->
	// ```gdscript
	// var member| := some_func|(some_variable|)
	//           ^             ^              ^
	//           |             |              | cursor on `some_variable, position on `)`
	//           |             |
	//           |             | cursor on `some_func`, pos on `(`
	//           |
	//           | cursor on `member`, pos on ` ` (space)
	// ```
	// -> Move position to previous character if:
	//    * Position not on valid identifier char.
	//    * Prev position is valid identifier char.
	lsp::Position pos = p_position;
	if (
			pos.character >= line.length() // Cursor at end of line.
			|| (!is_ascii_identifier_char(line[pos.character]) // Not on valid identifier char.
					   && (pos.character > 0 // Not line start -> there is a prev char.
								  && is_ascii_identifier_char(line[pos.character - 1]) // Prev is valid identifier char.
								  ))) {
		pos.character--;
	}

	int start_pos = pos.character;
	for (int c = pos.character; c >= 0; c--) {
		start_pos = c;
		char32_t ch = line[c];
		bool valid_char = is_ascii_identifier_char(ch);
		if (!valid_char) {
			break;
		}
	}

	int end_pos = pos.character;
	for (int c = pos.character; c < line.length(); c++) {
		char32_t ch = line[c];
		bool valid_char = is_ascii_identifier_char(ch);
		if (!valid_char) {
			break;
		}
		end_pos = c;
	}

	if (start_pos < end_pos) {
		r_range.start.line = r_range.end.line = pos.line;
		r_range.start.character = start_pos + 1;
		r_range.end.character = end_pos + 1;
		return line.substr(start_pos + 1, end_pos - start_pos);
	}

	return "";
}

String ExtendGDScriptParser::get_uri() const {
	return GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_uri(path);
}

const lsp::DocumentSymbol *ExtendGDScriptParser::search_symbol_defined_at_line(int p_line, const lsp::DocumentSymbol &p_parent, const String &p_symbol_name) const {
	const lsp::DocumentSymbol *ret = nullptr;
	if (p_line < p_parent.range.start.line) {
		return ret;
	} else if (p_parent.range.start.line == p_line && (p_symbol_name.is_empty() || p_parent.name == p_symbol_name)) {
		return &p_parent;
	} else {
		for (int i = 0; i < p_parent.children.size(); i++) {
			ret = search_symbol_defined_at_line(p_line, p_parent.children[i], p_symbol_name);
			if (ret) {
				break;
			}
		}
	}
	return ret;
}

Error ExtendGDScriptParser::get_left_function_call(const lsp::Position &p_position, lsp::Position &r_func_pos, int &r_arg_index) const {
	ERR_FAIL_INDEX_V(p_position.line, lines.size(), ERR_INVALID_PARAMETER);

	int bracket_stack = 0;
	int index = 0;

	bool found = false;
	for (int l = p_position.line; l >= 0; --l) {
		String line = lines[l];
		int c = line.length() - 1;
		if (l == p_position.line) {
			c = MIN(c, p_position.character - 1);
		}

		while (c >= 0) {
			const char32_t &character = line[c];
			if (character == ')') {
				++bracket_stack;
			} else if (character == '(') {
				--bracket_stack;
				if (bracket_stack < 0) {
					found = true;
				}
			}
			if (bracket_stack <= 0 && character == ',') {
				++index;
			}
			--c;
			if (found) {
				r_func_pos.character = c;
				break;
			}
		}

		if (found) {
			r_func_pos.line = l;
			r_arg_index = index;
			return OK;
		}
	}

	return ERR_METHOD_NOT_FOUND;
}

const lsp::DocumentSymbol *ExtendGDScriptParser::get_symbol_defined_at_line(int p_line, const String &p_symbol_name) const {
	if (p_line <= 0) {
		return &class_symbol;
	}
	return search_symbol_defined_at_line(p_line, class_symbol, p_symbol_name);
}

const lsp::DocumentSymbol *ExtendGDScriptParser::get_member_symbol(const String &p_name, const String &p_subclass) const {
	if (p_subclass.is_empty()) {
		const lsp::DocumentSymbol *const *ptr = members.getptr(p_name);
		if (ptr) {
			return *ptr;
		}
	} else {
		if (const ClassMembers *_class = inner_classes.getptr(p_subclass)) {
			const lsp::DocumentSymbol *const *ptr = _class->getptr(p_name);
			if (ptr) {
				return *ptr;
			}
		}
	}

	return nullptr;
}

const List<lsp::DocumentLink> &ExtendGDScriptParser::get_document_links() const {
	return document_links;
}

const Array &ExtendGDScriptParser::get_member_completions() {
	if (member_completions.is_empty()) {
		for (const KeyValue<String, const lsp::DocumentSymbol *> &E : members) {
			const lsp::DocumentSymbol *symbol = E.value;
			lsp::CompletionItem item = symbol->make_completion_item();
			item.data = JOIN_SYMBOLS(path, E.key);
			member_completions.push_back(item.to_json());
		}

		for (const KeyValue<String, ClassMembers> &E : inner_classes) {
			const ClassMembers *inner_class = &E.value;

			for (const KeyValue<String, const lsp::DocumentSymbol *> &F : *inner_class) {
				const lsp::DocumentSymbol *symbol = F.value;
				lsp::CompletionItem item = symbol->make_completion_item();
				item.data = JOIN_SYMBOLS(path, JOIN_SYMBOLS(E.key, F.key));
				member_completions.push_back(item.to_json());
			}
		}
	}

	return member_completions;
}

Dictionary ExtendGDScriptParser::dump_function_api(const GDScriptParser::FunctionNode *p_func) const {
	Dictionary func;
	ERR_FAIL_NULL_V(p_func, func);
	func["name"] = p_func->identifier->name;
	func["return_type"] = p_func->get_datatype().to_string();
	func["rpc_config"] = p_func->rpc_config;
	Array parameters;
	for (int i = 0; i < p_func->parameters.size(); i++) {
		Dictionary arg;
		arg["name"] = p_func->parameters[i]->identifier->name;
		arg["type"] = p_func->parameters[i]->get_datatype().to_string();
		if (p_func->parameters[i]->initializer != nullptr) {
			arg["default_value"] = p_func->parameters[i]->initializer->reduced_value;
		}
		parameters.push_back(arg);
	}
	if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(p_func->start_line))) {
		func["signature"] = symbol->detail;
		func["description"] = symbol->documentation;
	}
	func["arguments"] = parameters;
	return func;
}

Dictionary ExtendGDScriptParser::dump_class_api(const GDScriptParser::ClassNode *p_class) const {
	Dictionary class_api;

	ERR_FAIL_NULL_V(p_class, class_api);

	class_api["name"] = p_class->identifier != nullptr ? String(p_class->identifier->name) : String();
	class_api["path"] = path;
	Array extends_class;
	for (int i = 0; i < p_class->extends.size(); i++) {
		extends_class.append(String(p_class->extends[i]->name));
	}
	class_api["extends_class"] = extends_class;
	class_api["extends_file"] = String(p_class->extends_path);
	class_api["icon"] = String(p_class->icon_path);

	if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(p_class->start_line))) {
		class_api["signature"] = symbol->detail;
		class_api["description"] = symbol->documentation;
	}

	Array nested_classes;
	Array constants;
	Array class_members;
	Array signals;
	Array methods;
	Array static_functions;

	for (int i = 0; i < p_class->members.size(); i++) {
		const ClassNode::Member &m = p_class->members[i];
		switch (m.type) {
			case ClassNode::Member::CLASS:
				nested_classes.push_back(dump_class_api(m.m_class));
				break;
			case ClassNode::Member::CONSTANT: {
				Dictionary api;
				api["name"] = m.constant->identifier->name;
				api["value"] = m.constant->initializer->reduced_value;
				api["data_type"] = m.constant->get_datatype().to_string();
				if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(m.constant->start_line))) {
					api["signature"] = symbol->detail;
					api["description"] = symbol->documentation;
				}
				constants.push_back(api);
			} break;
			case ClassNode::Member::ENUM_VALUE: {
				Dictionary api;
				api["name"] = m.enum_value.identifier->name;
				api["value"] = m.enum_value.value;
				api["data_type"] = m.get_datatype().to_string();
				if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(m.enum_value.line))) {
					api["signature"] = symbol->detail;
					api["description"] = symbol->documentation;
				}
				constants.push_back(api);
			} break;
			case ClassNode::Member::ENUM: {
				Dictionary enum_dict;
				for (int j = 0; j < m.m_enum->values.size(); j++) {
					enum_dict[m.m_enum->values[j].identifier->name] = m.m_enum->values[j].value;
				}

				Dictionary api;
				api["name"] = m.m_enum->identifier->name;
				api["value"] = enum_dict;
				api["data_type"] = m.get_datatype().to_string();
				if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(m.m_enum->start_line))) {
					api["signature"] = symbol->detail;
					api["description"] = symbol->documentation;
				}
				constants.push_back(api);
			} break;
			case ClassNode::Member::VARIABLE: {
				Dictionary api;
				api["name"] = m.variable->identifier->name;
				api["data_type"] = m.variable->get_datatype().to_string();
				api["default_value"] = m.variable->initializer != nullptr ? m.variable->initializer->reduced_value : Variant();
				api["setter"] = m.variable->setter ? ("@" + String(m.variable->identifier->name) + "_setter") : (m.variable->setter_pointer != nullptr ? String(m.variable->setter_pointer->name) : String());
				api["getter"] = m.variable->getter ? ("@" + String(m.variable->identifier->name) + "_getter") : (m.variable->getter_pointer != nullptr ? String(m.variable->getter_pointer->name) : String());
				api["export"] = m.variable->exported;
				if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(m.variable->start_line))) {
					api["signature"] = symbol->detail;
					api["description"] = symbol->documentation;
				}
				class_members.push_back(api);
			} break;
			case ClassNode::Member::SIGNAL: {
				Dictionary api;
				api["name"] = m.signal->identifier->name;
				Array pars;
				for (int j = 0; j < m.signal->parameters.size(); j++) {
					pars.append(String(m.signal->parameters[j]->identifier->name));
				}
				api["arguments"] = pars;
				if (const lsp::DocumentSymbol *symbol = get_symbol_defined_at_line(LINE_NUMBER_TO_INDEX(m.signal->start_line))) {
					api["signature"] = symbol->detail;
					api["description"] = symbol->documentation;
				}
				signals.push_back(api);
			} break;
			case ClassNode::Member::FUNCTION: {
				if (m.function->is_static) {
					static_functions.append(dump_function_api(m.function));
				} else {
					methods.append(dump_function_api(m.function));
				}
			} break;
			case ClassNode::Member::GROUP:
				break; // No-op, but silences warnings.
			case ClassNode::Member::UNDEFINED:
				break; // Unreachable.
		}
	}

	class_api["sub_classes"] = nested_classes;
	class_api["constants"] = constants;
	class_api["members"] = class_members;
	class_api["signals"] = signals;
	class_api["methods"] = methods;
	class_api["static_functions"] = static_functions;

	return class_api;
}

Dictionary ExtendGDScriptParser::generate_api() const {
	Dictionary api;
	if (const GDScriptParser::ClassNode *gdclass = dynamic_cast<const GDScriptParser::ClassNode *>(get_tree())) {
		api = dump_class_api(gdclass);
	}
	return api;
}

Error ExtendGDScriptParser::parse(const String &p_code, const String &p_path) {
	path = p_path;
	lines = p_code.split("\n");

	Error err = GDScriptParser::parse(p_code, p_path, false);
	GDScriptAnalyzer analyzer(this);

	if (err == OK) {
		err = analyzer.analyze();
	}
	update_diagnostics();
	update_symbols();
	update_document_links(p_code);
	return err;
}
