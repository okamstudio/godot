/**************************************************************************/
/*  gdscript_translation_parser_plugin.h                                  */
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

#pragma once

#include "../gdscript_parser.h"
#include "../gdscript_tokenizer.h"

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "editor/editor_translation_parser.h"

class GDScriptEditorTranslationParserPlugin : public EditorTranslationParserPlugin {
	GDCLASS(GDScriptEditorTranslationParserPlugin, EditorTranslationParserPlugin);

	const HashMap<int, GDScriptTokenizer::CommentData> *comment_data = nullptr;

	Vector<String> *ids = nullptr;
	Vector<Vector<String>> *ids_ctx_plural = nullptr;

	Vector<String> ids_comment;
	Vector<String> ids_ctx_plural_comment;

	// List of patterns used for extracting translation strings.
	StringName tr_func = "tr";
	StringName trn_func = "tr_n";
	StringName atr_func = "atr";
	StringName atrn_func = "atr_n";
	HashSet<StringName> assignment_patterns;
	HashSet<StringName> first_arg_patterns;
	HashSet<StringName> second_arg_patterns;
	// FileDialog patterns.
	StringName fd_add_filter = "add_filter";
	StringName fd_set_filter = "set_filters";
	StringName fd_filters = "filters";

	static bool _is_constant_string(const GDScriptParser::ExpressionNode *p_expression);

	String _parse_comment(int p_line, bool &r_skip) const;

	void _add_id(const String &p_id, int p_line);
	void _add_id_ctx_plural(const Vector<String> &p_id_ctx_plural, int p_line);

	void _traverse_class(const GDScriptParser::ClassNode *p_class);
	void _traverse_function(const GDScriptParser::FunctionNode *p_func);
	void _traverse_block(const GDScriptParser::SuiteNode *p_suite);

	void _assess_expression(const GDScriptParser::ExpressionNode *p_expression);
	void _assess_assignment(const GDScriptParser::AssignmentNode *p_assignment);
	void _assess_call(const GDScriptParser::CallNode *p_call);

	void _extract_fd_filter_string(const GDScriptParser::ExpressionNode *p_expression, int p_line);
	void _extract_fd_filter_array(const GDScriptParser::ExpressionNode *p_expression);

public:
	virtual Error parse_file(const String &p_path, Vector<String> *r_ids, Vector<Vector<String>> *r_ids_ctx_plural) override;
	virtual void get_comments(Vector<String> *r_ids_comment, Vector<String> *r_ids_ctx_plural_comment) override;
	virtual void get_recognized_extensions(List<String> *r_extensions) const override;

	GDScriptEditorTranslationParserPlugin();
};
