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

#ifndef VARIANT_PARSER_H
#define VARIANT_PARSER_H

#include "core/error_list.h"
#include "core/os/file_access.h"
#include "core/resource.h"
#include "core/variant.h"

class VariantParser {
public:
	struct Stream {
	protected:
		// The readahead buffer is set by derived classes,
		// and can be a single character to effectively turn off readahead.
		CharType *readahead_buffer = nullptr;
		uint32_t readahead_size = 0;

		// The eof is NOT necessarily the source (e.g. file) eof,
		// because the readahead will reach source eof BEFORE
		// the stream catches up.
		bool _eof = false;

		// Number of characters we have read through already
		// in the buffer (points to the next one to read).
		uint32_t readahead_pointer = 0;

		// Characters filled in the buffer
		uint32_t readahead_filled = 0;

		virtual uint32_t _read_buffer(CharType *p_buffer, uint32_t p_num_chars) = 0;
		virtual bool _is_eof() const = 0;

		void invalidate_readahead(bool p_eof);
		bool readahead_enabled() const { return readahead_size != 1; }

		// The offset between the current read position (usually behind) and the current
		// file position (usually ahead), due to the readahead cache.
		uint32_t get_readahead_offset() const;

	public:
		enum Readahead {
			READAHEAD_DISABLED,
			READAHEAD_ENABLED,
		};

		CharType saved;

		CharType get_char();
		virtual bool is_utf8() const = 0;
		virtual uint64_t get_position() const = 0;
		bool is_eof() const;

		Stream() :
				saved(0) {}
		virtual ~Stream() {}
	};

	struct StreamFile : public Stream {
	private:
		enum { READAHEAD_SIZE = 2048 };

#ifdef DEV_ENABLED
		// These are used exclusively to check for out of sync errors.
		uint64_t _readahead_start_source_pos = 0;
		uint64_t _readahead_end_source_pos = 0;
#endif
		FileAccess *_file = nullptr;

		// Owned files are opened using open_file() rather than set_file().
		// They cannot be modified from outside StreamFile,
		// so are safe for readahead.
		// They will also be automatically closed when StreamFile
		// is destructed, or can optionally be closed using close_file().
		bool _is_owned_file = false;

		// Put buffer last in struct to keep the rest in cache
		CharType _buffer[READAHEAD_SIZE];

		void invalidate_readahead();

	protected:
		virtual uint32_t _read_buffer(CharType *p_buffer, uint32_t p_num_chars);
		virtual bool _is_eof() const;

	public:
		// NOTE:
		// Whenever possible, prefer to use the open_file() / close_file() functions
		// for the StreamFile to have internal ownership of the FileAccess.

		// If you intend to manipulate the file / file position from outside StreamFile,
		// make sure to either set readahead to false (to avoid syncing errors),
		// otherwise the readahead will GET OUT OF SYNC, and corrupt files may ensue.

		// Readahead is approx twice as fast, so should be used when possible.
		void set_file(FileAccess *p_file, Readahead p_readahead = READAHEAD_DISABLED);

		// Rather than have external ownership, it is safer to allow StreamFile
		// internal ownership of the file, to prevent external changes to the file
		// (via seeking / closing etc) which would make the readahead lose sync.
		Error open_file(const String &p_path);
		Error close_file();

		virtual bool is_utf8() const;
		virtual uint64_t get_position() const;

		StreamFile() {
			readahead_buffer = _buffer;
			readahead_size = READAHEAD_SIZE;
		}
		virtual ~StreamFile();
	};

	struct StreamString : public Stream {
	private:
		// Keep the buffer as compact as possible for String,
		// as it will have little effect, but to prevent the need for a virtual get_char() function.
		enum { READAHEAD_SIZE = 8 };
		int _pos;

	public:
		String s;

	private:
		// Put buffer last in struct to keep the rest in cache
		CharType _buffer[READAHEAD_SIZE];

	protected:
		virtual uint32_t _read_buffer(CharType *p_buffer, uint32_t p_num_chars);
		virtual bool _is_eof() const;

	public:
		virtual bool is_utf8() const;
		virtual uint64_t get_position() const;

		StreamString() {
			readahead_buffer = _buffer;
			readahead_size = READAHEAD_SIZE;
			_pos = 0;
		}
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
		Map<String, Variant> fields;
	};

private:
	static const char *tk_name[TK_MAX];

	template <class T>
	static Error _parse_construct(Stream *p_stream, Vector<T> &r_construct, int &line, String &r_err_str);
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
	typedef String (*EncodeResourceFunc)(void *ud, const RES &p_resource);

	static Error write(const Variant &p_variant, StoreStringFunc p_store_string_func, void *p_store_string_ud, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud, int p_recursion_count = 0);
	static Error write_to_string(const Variant &p_variant, String &r_string, EncodeResourceFunc p_encode_res_func = nullptr, void *p_encode_res_ud = nullptr);
};

#endif // VARIANT_PARSER_H
