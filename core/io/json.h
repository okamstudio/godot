/**************************************************************************/
/*  json.h                                                                */
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

#ifndef JSON_H
#define JSON_H

#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/variant/variant.h"

class JSON : public Resource {
	GDCLASS(JSON, Resource);

	enum TokenType {
		TK_CURLY_BRACKET_OPEN,
		TK_CURLY_BRACKET_CLOSE,
		TK_BRACKET_OPEN,
		TK_BRACKET_CLOSE,
		TK_IDENTIFIER,
		TK_STRING,
		TK_NUMBER,
		TK_COLON,
		TK_COMMA,
		TK_EOF,
		TK_MAX
	};

	enum Expecting {
		EXPECT_OBJECT,
		EXPECT_OBJECT_KEY,
		EXPECT_COLON,
		EXPECT_OBJECT_VALUE,
	};

	struct Token {
		TokenType type;
		Variant value;
	};

	String text;
	Variant data;
	String err_str;
	int err_line = 0;

	static const char *tk_name[];

	static String _make_indent(const String &p_indent, int p_size);
	static String _stringify(const Variant &p_var, const String &p_indent, int p_cur_indent, bool p_sort_keys, HashSet<const void *> &p_markers, bool p_full_precision = false);
	static Error _get_token(const char32_t *p_str, int &index, int p_len, Token &r_token, int &line, String &r_err_str);
	static Error _parse_value(Variant &value, Token &token, const char32_t *p_str, int &index, int p_len, int &line, int p_depth, String &r_err_str);
	static Error _parse_array(Array &array, const char32_t *p_str, int &index, int p_len, int &line, int p_depth, String &r_err_str);
	static Error _parse_object(Dictionary &object, const char32_t *p_str, int &index, int p_len, int &line, int p_depth, String &r_err_str);
	static Error _parse_string(const String &p_json, Variant &r_ret, String &r_err_str, int &r_err_line);

	static Variant _from_native(const Variant &p_variant, bool p_full_objects, int p_depth);
	static Variant _to_native(const Variant &p_json, bool p_allow_objects, int p_depth);

protected:
	static void _bind_methods();

public:
	Error parse(const String &p_json_string, bool p_keep_text = false);
	String get_parsed_text() const;

	static String stringify(const Variant &p_var, const String &p_indent = "", bool p_sort_keys = true, bool p_full_precision = false);
	static Variant parse_string(const String &p_json_string);

	_FORCE_INLINE_ static Variant from_native(const Variant &p_variant, bool p_full_objects = false) {
		return _from_native(p_variant, p_full_objects, 0);
	}
	_FORCE_INLINE_ static Variant to_native(const Variant &p_json, bool p_allow_objects = false) {
		return _to_native(p_json, p_allow_objects, 0);
	}

	void set_data(const Variant &p_data);
	_FORCE_INLINE_ Variant get_data() const { return data; }

	_FORCE_INLINE_ int get_error_line() const { return err_line; }
	_FORCE_INLINE_ String get_error_message() const { return err_str; }
};

class ResourceFormatLoaderJSON : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;

	// Treat JSON as a text file, do not generate a `*.json.uid` file.
	virtual ResourceUID::ID get_resource_uid(const String &p_path) const override { return ResourceUID::INVALID_ID; }
	virtual bool has_custom_uid_support() const override { return true; }
};

class ResourceFormatSaverJSON : public ResourceFormatSaver {
public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) override;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const override;
	virtual bool recognize(const Ref<Resource> &p_resource) const override;
};

#endif // JSON_H
