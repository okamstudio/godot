/**************************************************************************/
/*  gdscript_extend_parser.h                                              */
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

#ifndef GDSCRIPT_EXTEND_PARSER_H
#define GDSCRIPT_EXTEND_PARSER_H

#include "../gdscript_parser.h"
#include "godot_lsp.h"

#include "core/variant/variant.h"

#ifndef LINE_NUMBER_TO_INDEX
#define LINE_NUMBER_TO_INDEX(p_line) ((p_line) - 1)
#endif
#ifndef COLUMN_NUMBER_TO_INDEX
#define COLUMN_NUMBER_TO_INDEX(p_column) ((p_column) - 1)
#endif

#ifndef SYMBOL_SEPARATOR
#define SYMBOL_SEPARATOR "::"
#endif

#ifndef JOIN_SYMBOLS
#define JOIN_SYMBOLS(p_path, name) ((p_path) + SYMBOL_SEPARATOR + (name))
#endif

typedef HashMap<String, const lsp::DocumentSymbol *> ClassMembers;

/**
 * Represents a Position as used by GDScript Parser. Used for conversion to and from `lsp::Position`.
 *
 * Difference to `lsp::Position`:
 * * Line & Char/column: 1-based
 * 		* LSP: both 0-based
 * * Tabs are expanded to columns using tab size (`text_editor/behavior/indent/size`).
 *   	* LSP: tab is single char
 *
 * Example:
 * ```gdscript
 * →→var my_value = 42
 * ```
 * `_` is at:
 * * Godot: `column=12`
 * 	* using `indent/size=4`
 * 	* Note: counting starts at `1`
 * * LSP: `character=8`
 * 	* Note: counting starts at `0`
 */
struct GodotPosition {
	int line;
	int column;

	GodotPosition(int p_line, int p_column) :
			line(p_line), column(p_column) {}

	lsp::Position to_lsp(const Vector<String> &p_lines) const;
	static GodotPosition from_lsp(const lsp::Position p_pos, const Vector<String> &p_lines);

	bool operator==(const GodotPosition &p_other) const {
		return line == p_other.line && column == p_other.column;
	}

	String to_string() const {
		return vformat("(%d,%d)", line, column);
	}
};

struct GodotRange {
	GodotPosition start;
	GodotPosition end;

	GodotRange(GodotPosition p_start, GodotPosition p_end) :
			start(p_start), end(p_end) {}

	lsp::Range to_lsp(const Vector<String> &p_lines) const;
	static GodotRange from_lsp(const lsp::Range &p_range, const Vector<String> &p_lines);

	bool operator==(const GodotRange &p_other) const {
		return start == p_other.start && end == p_other.end;
	}

	String to_string() const {
		return vformat("[%s:%s]", start.to_string(), end.to_string());
	}
};

class ExtendGDScriptParser : public GDScriptParser {
	String path;
	Vector<String> lines;

	lsp::DocumentSymbol class_symbol;
	Vector<lsp::Diagnostic> diagnostics;
	List<lsp::DocumentLink> document_links;
	ClassMembers members;
	HashMap<String, ClassMembers> inner_classes;

	lsp::Range range_of_node(const GDScriptParser::Node *p_node) const;

	void update_diagnostics();

	void update_symbols();
	void update_document_links(const String &p_code);
	void parse_class_symbol(const GDScriptParser::ClassNode *p_class, lsp::DocumentSymbol &r_symbol);
	void parse_function_symbol(const GDScriptParser::FunctionNode *p_func, lsp::DocumentSymbol &r_symbol);

	Dictionary dump_function_api(const GDScriptParser::FunctionNode *p_func) const;
	Dictionary dump_class_api(const GDScriptParser::ClassNode *p_class) const;

	const lsp::DocumentSymbol *search_symbol_defined_at_line(int p_line, const lsp::DocumentSymbol &p_parent, const String &p_symbol_name = "") const;

	Array member_completions;

public:
	_FORCE_INLINE_ const String &get_path() const { return path; }
	_FORCE_INLINE_ const Vector<String> &get_lines() const { return lines; }
	_FORCE_INLINE_ const lsp::DocumentSymbol &get_symbols() const { return class_symbol; }
	_FORCE_INLINE_ const Vector<lsp::Diagnostic> &get_diagnostics() const { return diagnostics; }
	_FORCE_INLINE_ const ClassMembers &get_members() const { return members; }
	_FORCE_INLINE_ const HashMap<String, ClassMembers> &get_inner_classes() const { return inner_classes; }

	Error get_left_function_call(const lsp::Position &p_position, lsp::Position &r_func_pos, int &r_arg_index) const;

	String get_text_for_completion(const lsp::Position &p_cursor) const;
	String get_text_for_lookup_symbol(const lsp::Position &p_cursor, const String &p_symbol = "", bool p_func_required = false) const;
	String get_identifier_under_position(const lsp::Position &p_position, lsp::Range &r_range) const;
	String get_uri() const;

	/**
	 * `p_symbol_name` gets ignored if empty. Otherwise symbol must match passed in named.
	 *
	 * Necessary when multiple symbols at same line for example with `func`:
	 * `func handle_arg(arg: int):`
	 * -> Without `p_symbol_name`: returns `handle_arg`. Even if parameter (`arg`) is wanted.
	 *    With `p_symbol_name`: symbol name MUST match `p_symbol_name`: returns `arg`.
	 */
	const lsp::DocumentSymbol *get_symbol_defined_at_line(int p_line, const String &p_symbol_name = "") const;
	const lsp::DocumentSymbol *get_member_symbol(const String &p_name, const String &p_subclass = "") const;
	const List<lsp::DocumentLink> &get_document_links() const;

	const Array &get_member_completions();
	Dictionary generate_api() const;

	Error parse(const String &p_code, const String &p_path);
};

#endif // GDSCRIPT_EXTEND_PARSER_H
