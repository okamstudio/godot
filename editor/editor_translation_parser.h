/**************************************************************************/
/*  editor_translation_parser.h                                           */
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

#ifndef EDITOR_TRANSLATION_PARSER_H
#define EDITOR_TRANSLATION_PARSER_H

#include "core/error/error_list.h"
#include "core/object/gdvirtual.gen.inc"
#include "core/object/ref_counted.h"
#include "core/variant/typed_array.h"

class EditorTranslationParserPlugin : public RefCounted {
	GDCLASS(EditorTranslationParserPlugin, RefCounted);

protected:
	static void _bind_methods();

	GDVIRTUAL1R(TypedArray<PackedStringArray>, _parse_file, String)
	GDVIRTUAL0RC(Vector<String>, _get_recognized_extensions)

#ifndef DISABLE_DEPRECATED
	GDVIRTUAL3_COMPAT(_parse_file_bind_compat_99297, _parse_file, String, TypedArray<String>, TypedArray<Array>)
#endif

public:
	virtual Error parse_file(const String &p_path, Vector<String> *r_ids, Vector<String> *r_ctxts, Vector<String> *r_ids_plural, Vector<String> *r_comments);
	virtual void get_recognized_extensions(List<String> *r_extensions) const;
};

class EditorTranslationParser {
	static EditorTranslationParser *singleton;

public:
	enum ParserType {
		STANDARD, // GDScript, CSharp, ...
		CUSTOM // User-defined parser plugins. This will override standard parsers if the same extension type is defined.
	};

	static EditorTranslationParser *get_singleton();

	Vector<Ref<EditorTranslationParserPlugin>> standard_parsers;
	Vector<Ref<EditorTranslationParserPlugin>> custom_parsers;

	void get_recognized_extensions(List<String> *r_extensions) const;
	bool can_parse(const String &p_extension) const;
	Ref<EditorTranslationParserPlugin> get_parser(const String &p_extension) const;
	void add_parser(const Ref<EditorTranslationParserPlugin> &p_parser, ParserType p_type);
	void remove_parser(const Ref<EditorTranslationParserPlugin> &p_parser, ParserType p_type);
	void clean_parsers();

	EditorTranslationParser();
	~EditorTranslationParser();
};

#endif // EDITOR_TRANSLATION_PARSER_H
