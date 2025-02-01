/**************************************************************************/
/*  variant_parser.h                                                      */
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

#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/variant/variant.h"

class VariantParser {
public:
	struct Stream {
	private:
		enum { READAHEAD_SIZE = 2048 };
		char32_t readahead_buffer[READAHEAD_SIZE];
		uint32_t readahead_pointer = 0;
		uint32_t readahead_filled = 0;
		bool eof = false;

	protected:
		bool readahead_enabled = true;
		virtual uint32_t _read_buffer(char32_t *p_buffer, uint32_t p_num_chars) = 0;
		virtual bool _is_eof() const = 0;

	public:
		char32_t saved = 0;

		char32_t get_char();
		virtual bool is_utf8() const = 0;
		bool is_eof() const;

		Stream() {}
		virtual ~Stream() {}
	};

	struct StreamFile : public Stream {
	protected:
		virtual uint32_t _read_buffer(char32_t *p_buffer, uint32_t p_num_chars) override;
		virtual bool _is_eof() const override;

	public:
		Ref<FileAccess> f;

		virtual bool is_utf8() const override;

		StreamFile(bool p_readahead_enabled = true) { readahead_enabled = p_readahead_enabled; }
	};

	struct StreamString : public Stream {
		String s;

	private:
		int pos = 0;

	protected:
		virtual uint32_t _read_buffer(char32_t *p_buffer, uint32_t p_num_chars) override;
		virtual bool _is_eof() const override;

	public:
		virtual bool is_utf8() const override;
		StreamString(bool p_readahead_enabled = true) { readahead_enabled = p_readahead_enabled; }
	};

	typedef Error (*ParseResourceFunc)(void *p_self, Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str);

	struct ResourceParser {
		void *userdata = nullptr;
		ParseResourceFunc func = nullptr;
		ParseResourceFunc ext_func = nullptr;
		ParseResourceFunc sub_func = nullptr;
	};

	enum TokenType {
		TK_CURLY_BRACKET_OPEN,
		TK_CURLY_BRACKET_CLOSE,
		TK_BRACKET_OPEN,
		TK_BRACKET_CLOSE,
		TK_PARENTHESIS_OPEN,
		TK_PARENTHESIS_CLOSE,
		TK_IDENTIFIER,
		TK_STRING,
		TK_STRING_NAME,
		TK_NUMBER,
		TK_COLOR,
		TK_COLON,
		TK_COMMA,
		TK_PERIOD,
		TK_EQUAL,
		TK_EOF,
		TK_ERROR,
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

	struct Tag {
		String name;
		HashMap<String, Variant> fields;
	};

private:
	static const char *tk_name[TK_MAX];

	template <typename T>
	static Error _parse_construct(Stream *p_stream, Vector<T> &r_construct, int &line, String &r_err_str);
	static Error _parse_byte_array(Stream *p_stream, Vector<uint8_t> &r_construct, int &line, String &r_err_str);
	static Error _parse_enginecfg(Stream *p_stream, Vector<String> &strings, int &line, String &r_err_str);
	static Error _parse_dictionary(Dictionary &object, Stream *p_stream, int &line, String &r_err_str, ResourceParser *p_res_parser = nullptr);
	static Error _parse_array(Array &array, Stream *p_stream, int &line, String &r_err_str, ResourceParser *p_res_parser = nullptr);
	static Error _parse_tag(Token &token, Stream *p_stream, int &line, String &r_err_str, Tag &r_tag, ResourceParser *p_res_parser = nullptr, bool p_simple_tag = false);

public:
	static Error parse_tag(Stream *p_stream, int &line, String &r_err_str, Tag &r_tag, ResourceParser *p_res_parser = nullptr, bool p_simple_tag = false);
	static Error parse_tag_assign_eof(Stream *p_stream, int &line, String &r_err_str, Tag &r_tag, String &r_assign, Variant &r_value, ResourceParser *p_res_parser = nullptr, bool p_simple_tag = false);

	static Error parse_value(Token &token, Variant &value, Stream *p_stream, int &line, String &r_err_str, ResourceParser *p_res_parser = nullptr);
	static Error get_token(Stream *p_stream, Token &r_token, int &line, String &r_err_str);
	static Error parse(Stream *p_stream, Variant &r_ret, String &r_err_str, int &r_err_line, ResourceParser *p_res_parser = nullptr);
};

class VariantWriter {
public:
	typedef Error (*StoreStringFunc)(void *ud, const String &p_string);
	typedef String (*EncodeResourceFunc)(void *ud, const Ref<Resource> &p_resource);

	static Error write(const Variant &p_variant, StoreStringFunc p_store_string_func, void *p_store_string_ud, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud, int p_recursion_count = 0, bool p_compat = true);
	static Error write_to_string(const Variant &p_variant, String &r_string, EncodeResourceFunc p_encode_res_func = nullptr, void *p_encode_res_ud = nullptr, bool p_compat = true);
};
