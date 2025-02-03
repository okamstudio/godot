/**************************************************************************/
/*  ustring.h                                                             */
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

#ifndef USTRING_GODOT_H
#define USTRING_GODOT_H

#include "core/array.h"
#include "core/cowdata.h"
#include "core/typedefs.h"
#include "core/vector.h"

template <class T>
class CharProxy {
	friend class CharString;
	friend class String;

	const int _index;
	CowData<T> &_cowdata;
	static const T _null = 0;

	_FORCE_INLINE_ CharProxy(const int &p_index, CowData<T> &p_cowdata) :
			_index(p_index),
			_cowdata(p_cowdata) {}

public:
	_FORCE_INLINE_ CharProxy(const CharProxy<T> &p_other) :
			_index(p_other._index),
			_cowdata(p_other._cowdata) {}

	_FORCE_INLINE_ operator T() const {
		if (unlikely(_index == _cowdata.size())) {
			return _null;
		}

		return _cowdata.get(_index);
	}

	_FORCE_INLINE_ const T *operator&() const {
		return _cowdata.ptr() + _index;
	}

	_FORCE_INLINE_ void operator=(const T &p_other) const {
		_cowdata.set(_index, p_other);
	}

	_FORCE_INLINE_ void operator=(const CharProxy<T> &p_other) const {
		_cowdata.set(_index, p_other.operator T());
	}
};

class CharString {
	CowData<char> _cowdata;
	static const char _null;

public:
	_FORCE_INLINE_ char *ptrw() { return _cowdata.ptrw(); }
	_FORCE_INLINE_ const char *ptr() const { return _cowdata.ptr(); }
	_FORCE_INLINE_ int size() const { return _cowdata.size(); }
	Error resize(int p_size) { return _cowdata.resize(p_size); }

	_FORCE_INLINE_ char get(int p_index) const { return _cowdata.get(p_index); }
	_FORCE_INLINE_ void set(int p_index, const char &p_elem) { _cowdata.set(p_index, p_elem); }
	_FORCE_INLINE_ const char &operator[](int p_index) const {
		if (unlikely(p_index == _cowdata.size())) {
			return _null;
		}

		return _cowdata.get(p_index);
	}
	_FORCE_INLINE_ CharProxy<char> operator[](int p_index) { return CharProxy<char>(p_index, _cowdata); }

	_FORCE_INLINE_ CharString() {}
	_FORCE_INLINE_ CharString(const CharString &p_str) { _cowdata._ref(p_str._cowdata); }
	_FORCE_INLINE_ CharString operator=(const CharString &p_str) {
		_cowdata._ref(p_str._cowdata);
		return *this;
	}
	_FORCE_INLINE_ CharString(const char *p_cstr) { copy_from(p_cstr); }

	CharString &operator=(const char *p_cstr);
	bool operator<(const CharString &p_right) const;
	CharString &operator+=(char p_char);
	int length() const { return size() ? size() - 1 : 0; }
	const char *get_data() const;
	operator const char *() const { return get_data(); };

protected:
	void copy_from(const char *p_cstr);
};

typedef wchar_t CharType;

struct StrRange {
	const CharType *c_str;
	int len;

	StrRange(const CharType *p_c_str = nullptr, int p_len = 0) {
		c_str = p_c_str;
		len = p_len;
	}
};

class String {
	CowData<CharType> _cowdata;
	static const CharType _null;

	void copy_from(const char *p_cstr);
	void copy_from(const CharType *p_cstr, const int p_clip_to = -1);
	void copy_from(const CharType &p_char);
	void copy_from_unchecked(const CharType *p_char, const int p_length);
	bool _base_is_subsequence_of(const String &p_string, bool case_insensitive) const;
	int _count(const String &p_string, int p_from, int p_to, bool p_case_insensitive) const;
	String _replace(const String &p_old, const String &p_new, int p_count = -1, bool p_is_case_sensitive = true) const;
	String _replace(const char *p_old, const char *p_new, int p_count = -1, bool p_is_case_sensitive = true) const;
	String _rreplace(const String &p_old, const String &p_new, int p_count = -1, bool p_is_case_sensitive = true) const;
	String _rreplace(const char *p_old, const char *p_new, int p_count = -1, bool p_is_case_sensitive = true) const;

public:
	enum {

		npos = -1 ///<for "some" compatibility with std::string (npos is a huge value in std::string)
	};

	_FORCE_INLINE_ CharType *ptrw() { return _cowdata.ptrw(); }
	_FORCE_INLINE_ const CharType *ptr() const { return _cowdata.ptr(); }

	void remove(int p_index) { _cowdata.remove(p_index); }

	_FORCE_INLINE_ void clear() { resize(0); }

	_FORCE_INLINE_ CharType get(int p_index) const { return _cowdata.get(p_index); }
	_FORCE_INLINE_ void set(int p_index, const CharType &p_elem) { _cowdata.set(p_index, p_elem); }
	_FORCE_INLINE_ int size() const { return _cowdata.size(); }
	Error resize(int p_size) { return _cowdata.resize(p_size); }

	_FORCE_INLINE_ const CharType &operator[](int p_index) const {
		if (unlikely(p_index == _cowdata.size())) {
			return _null;
		}

		return _cowdata.get(p_index);
	}
	_FORCE_INLINE_ CharProxy<CharType> operator[](int p_index) { return CharProxy<CharType>(p_index, _cowdata); }

	bool operator==(const String &p_str) const;
	bool operator!=(const String &p_str) const;
	String operator+(const String &p_str) const;
	//String operator+(CharType p_char) const;

	String &operator+=(const String &);
	String &operator+=(CharType p_char);
	String &operator+=(const char *p_str);
	String &operator+=(const CharType *p_str);

	/* Compatibility Operators */

	void operator=(const char *p_str);
	void operator=(const CharType *p_str);
	bool operator==(const char *p_str) const;
	bool operator==(const CharType *p_str) const;
	bool operator==(const StrRange &p_str_range) const;
	bool operator!=(const char *p_str) const;
	bool operator!=(const CharType *p_str) const;
	bool operator<(const CharType *p_str) const;
	bool operator<(const char *p_str) const;
	bool operator<(const String &p_str) const;
	bool operator<=(const String &p_str) const;

	signed char casecmp_to(const String &p_str) const;
	signed char nocasecmp_to(const String &p_str) const;
	signed char naturalnocasecmp_to(const String &p_str) const;

	const CharType *c_str() const;
	/* standard size stuff */

	_FORCE_INLINE_ int length() const {
		int s = size();
		return s ? (s - 1) : 0; // length does not include zero
	}

	/* complex helpers */
	String substr(int p_from, int p_chars = -1) const;
	int find(const String &p_str, int p_from = 0) const; ///< return <0 if failed
	int find(const char *p_str, int p_from = 0) const; ///< return <0 if failed
	int find_char(const CharType &p_char, int p_from = 0) const; ///< return <0 if failed
	int find_last(const String &p_str) const; ///< return <0 if failed
	int findn(const String &p_str, int p_from = 0) const; ///< return <0 if failed, case insensitive
	int rfind(const String &p_str, int p_from = -1) const; ///< return <0 if failed
	int rfindn(const String &p_str, int p_from = -1) const; ///< return <0 if failed, case insensitive
	int findmk(const Vector<String> &p_keys, int p_from = 0, int *r_key = nullptr) const; ///< return <0 if failed
	bool match(const String &p_wildcard) const;
	bool matchn(const String &p_wildcard) const;
	bool begins_with(const String &p_string) const;
	bool begins_with(const char *p_string) const;
	bool ends_with(const String &p_string) const;
	bool is_enclosed_in(const String &p_string) const;
	bool is_subsequence_of(const String &p_string) const;
	bool is_subsequence_ofi(const String &p_string) const;
	bool is_quoted() const;
	Vector<String> bigrams() const;
	float similarity(const String &p_string) const;
	String format(const Variant &values, String placeholder = "{_}") const;
	String replace_first(const String &p_key, const String &p_with) const;
	String replace(const String &p_old, const String &p_new, int p_count = -1) const;
	String replace(const char *p_old, const char *p_new, int p_count = -1) const;
	String replacen(const String &p_old, const String &p_new, int p_count = -1) const;
	String replacen(const char *p_old, const char *p_new, int p_count = -1) const;
	String rreplace(const String &p_old, const String &p_new, int p_count = -1) const;
	String rreplace(const char *p_old, const char *p_new, int p_count = -1) const;
	String rreplacen(const String &p_old, const String &p_new, int p_count = -1) const;
	String rreplacen(const char *p_old, const char *p_new, int p_count = -1) const;
	String repeat(int p_count) const;
	String insert(int p_at_pos, const String &p_string) const;
	String pad_decimals(int p_digits) const;
	String pad_zeros(int p_digits) const;
	String trim_prefix(const String &p_prefix) const;
	String trim_suffix(const String &p_suffix) const;
	String lpad(int min_length, const String &character = " ") const;
	String rpad(int min_length, const String &character = " ") const;
	String sprintf(const Array &values, bool *error) const;
	String quote(String quotechar = "\"") const;
	String unquote() const;
	static String num(double p_num, int p_decimals = -1);
	static String num_scientific(double p_num);
	static String num_real(double p_num);
	static String num_int64(int64_t p_num, int base = 10, bool capitalize_hex = false);
	static String num_uint64(uint64_t p_num, int base = 10, bool capitalize_hex = false);
	static String chr(CharType p_char);
	static String md5(const uint8_t *p_md5);
	static String hex_encode_buffer(const uint8_t *p_buffer, int p_len);
	bool is_numeric() const;
	double to_double() const;
	float to_float() const;
	int hex_to_int(bool p_with_prefix = true) const;
	int to_int() const;

	int64_t hex_to_int64(bool p_with_prefix = true) const;
	int64_t bin_to_int64(bool p_with_prefix = true) const;
	int64_t to_int64() const;
	static int to_int(const char *p_str, int p_len = -1);
	static double to_double(const char *p_str);
	static double to_double(const CharType *p_str, const CharType **r_end = nullptr);
	static int64_t to_int(const CharType *p_str, int p_len = -1);
	String capitalize() const;
	String camelcase_to_underscore(bool lowercase = true) const;

	int get_slice_count(String p_splitter) const;
	String get_slice(String p_splitter, int p_slice) const;
	String get_slicec(CharType p_splitter, int p_slice) const;

	Vector<String> split(const String &p_splitter, bool p_allow_empty = true, int p_maxsplit = 0) const;
	Vector<String> rsplit(const String &p_splitter, bool p_allow_empty = true, int p_maxsplit = 0) const;
	Vector<String> split_spaces() const;
	Vector<float> split_floats(const String &p_splitter, bool p_allow_empty = true) const;
	Vector<float> split_floats_mk(const Vector<String> &p_splitters, bool p_allow_empty = true) const;
	Vector<int> split_ints(const String &p_splitter, bool p_allow_empty = true) const;
	Vector<int> split_ints_mk(const Vector<String> &p_splitters, bool p_allow_empty = true) const;

	String join(const Vector<String> &parts) const;

	static CharType char_uppercase(CharType p_char);
	static CharType char_lowercase(CharType p_char);
	String to_upper() const;
	String to_lower() const;

	int count(const String &p_string, int p_from = 0, int p_to = 0) const;
	int countn(const String &p_string, int p_from = 0, int p_to = 0) const;

	String left(int p_pos) const;
	String right(int p_pos) const;
	String indent(const String &p_prefix) const;
	String dedent() const;
	String strip_edges(bool left = true, bool right = true) const;
	String strip_escapes() const;
	String lstrip(const String &p_chars) const;
	String rstrip(const String &p_chars) const;
	String get_extension() const;
	String get_basename() const;
	String plus_file(const String &p_file) const;
	CharType ord_at(int p_idx) const;

	void erase(int p_pos, int p_chars);

	CharString ascii(bool p_allow_extended = false) const;
	CharString utf8() const;
	bool parse_utf8(const char *p_utf8, int p_len = -1, bool p_skip_cr = false); //return true on error
	static String utf8(const char *p_utf8, int p_len = -1);

	static uint32_t hash(const CharType *p_cstr, int p_len); /* hash the string */
	static uint32_t hash(const CharType *p_cstr); /* hash the string */
	static uint32_t hash(const char *p_cstr, int p_len); /* hash the string */
	static uint32_t hash(const char *p_cstr); /* hash the string */
	uint32_t hash() const; /* hash the string */
	uint64_t hash64() const; /* hash the string */
	String md5_text() const;
	String sha1_text() const;
	String sha256_text() const;
	Vector<uint8_t> md5_buffer() const;
	Vector<uint8_t> sha1_buffer() const;
	Vector<uint8_t> sha256_buffer() const;

	_FORCE_INLINE_ bool empty() const { return length() == 0; }

	// path functions
	bool is_abs_path() const;
	bool is_rel_path() const;
	bool is_resource_file() const;
	String path_to(const String &p_path) const;
	String path_to_file(const String &p_path) const;
	String get_base_dir() const;
	String get_file() const;
	static String humanize_size(uint64_t p_size);
	String simplify_path() const;
	bool is_network_share_path() const;

	String xml_escape(bool p_escape_quotes = false) const;
	String xml_unescape() const;
	String http_escape() const;
	String http_unescape() const;
	String c_escape() const;
	String c_escape_multiline() const;
	String c_unescape() const;
	String json_escape() const;
	String word_wrap(int p_chars_per_line) const;
	Error parse_url(String &r_scheme, String &r_host, int &r_port, String &r_path) const;

	String percent_encode() const;
	String percent_decode() const;

	String property_name_encode() const;

	// node functions
	static const String invalid_node_name_characters;
	String validate_node_name() const;
	String validate_identifier() const;

	bool is_valid_identifier() const;
	bool is_valid_integer() const;
	bool is_valid_float() const;
	bool is_valid_hex_number(bool p_with_prefix) const;
	bool is_valid_html_color() const;
	bool is_valid_ip_address() const;
	bool is_valid_filename() const;

	/**
	 * The constructors must not depend on other overloads
	 */
	/*	String(CharType p_char);*/

	_FORCE_INLINE_ String() {}
	_FORCE_INLINE_ String(const String &p_str) { _cowdata._ref(p_str._cowdata); }
	String operator=(const String &p_str) {
		_cowdata._ref(p_str._cowdata);
		return *this;
	}

	String(const char *p_str);
	String(const CharType *p_str, int p_clip_to_len = -1);
	String(const StrRange &p_range);
};

bool operator==(const char *p_chr, const String &p_str);

String operator+(const char *p_chr, const String &p_str);
String operator+(CharType p_chr, const String &p_str);

String itos(int64_t p_val);
String uitos(uint64_t p_val);
String rtos(double p_val);
String rtoss(double p_val); //scientific version

struct NoCaseComparator {
	bool operator()(const String &p_a, const String &p_b) const {
		return p_a.nocasecmp_to(p_b) < 0;
	}
};

struct NaturalNoCaseComparator {
	bool operator()(const String &p_a, const String &p_b) const {
		return p_a.naturalnocasecmp_to(p_b) < 0;
	}
};

template <typename L, typename R>
_FORCE_INLINE_ bool is_str_less(const L *l_ptr, const R *r_ptr) {
	while (true) {
		if (*l_ptr == 0 && *r_ptr == 0) {
			return false;
		} else if (*l_ptr == 0) {
			return true;
		} else if (*r_ptr == 0) {
			return false;
		} else if (*l_ptr < *r_ptr) {
			return true;
		} else if (*l_ptr > *r_ptr) {
			return false;
		}

		l_ptr++;
		r_ptr++;
	}
}

/* end of namespace */

// Tool translate (TTR and variants) for the editor UI,
// and doc translate for the class reference (DTR).
#ifdef TOOLS_ENABLED
// Gets parsed.
String TTR(const String &p_text, const String &p_context = "");
String DTR(const String &);
// Use for C strings.
#define TTRC(m_value) (m_value)
// Use to avoid parsing (for use later with C strings).
#define TTRGET(m_value) TTR(m_value)

#else
#define TTR(m_value) (String())
#define DTR(m_value) (String())
#define TTRC(m_value) (m_value)
#define TTRGET(m_value) (m_value)
#endif

// Use this to mark property names for editor translation.
// Often for dynamic properties defined in _get_property_list().
// Property names defined directly inside EDITOR_DEF, GLOBAL_DEF, and ADD_PROPERTY macros don't need this.
#define PNAME(m_value) (m_value)

// Similar to PNAME, but to mark groups, i.e. properties with PROPERTY_USAGE_GROUP.
// Groups defined directly inside ADD_GROUP macros don't need this.
// The arguments are the same as ADD_GROUP. m_prefix is only used for extraction.
#define GNAME(m_value, m_prefix) (m_value)

// Runtime translate for the public node API.
String RTR(const String &);

bool is_symbol(CharType c);
bool select_word(const String &p_s, int p_col, int &r_beg, int &r_end);

#endif // USTRING_GODOT_H
