/**************************************************************************/
/*  xml_parser.cpp                                                        */
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

#include "xml_parser.h"

#include "core/io/file_access.h"

//#define DEBUG_XML

static inline bool _is_white_space(char c) {
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

//! sets the state that text was found. Returns true if set should be set
bool XMLParser::_set_text(const char *start, const char *end) {
	// check if text is more than 2 characters, and if not, check if there is
	// only white space, so that this text won't be reported
	if (end - start < 3) {
		const char *p = start;
		for (; p != end; ++p) {
			if (!_is_white_space(*p)) {
				break;
			}
		}

		if (p == end) {
			return false;
		}
	}

	// set current text to the parsed text, and replace xml special characters
	String s = String::utf8(start, (int)(end - start));
	node_name = s.xml_unescape();

	// current XML node type is text
	node_type = NODE_TEXT;

	return true;
}

void XMLParser::_parse_closing_xml_element() {
	node_type = NODE_ELEMENT_END;
	node_empty = false;
	attributes.clear();

	next_char();
	const char *pBeginClose = P;

	while (*P && *P != '>') {
		next_char();
	}

	node_name = String::utf8(pBeginClose, (int)(P - pBeginClose));
#ifdef DEBUG_XML
	print_line("XML CLOSE: " + node_name);
#endif

	if (*P) {
		next_char();
	}
}

void XMLParser::_ignore_definition() {
	node_type = NODE_UNKNOWN;

	const char *F = P;
	// move until end marked with '>' reached
	while (*P && *P != '>') {
		next_char();
	}
	node_name.parse_utf8(F, P - F);

	if (*P) {
		next_char();
	}
}

bool XMLParser::_parse_cdata() {
	if (*(P + 1) != '[') {
		return false;
	}

	node_type = NODE_CDATA;

	// skip '<![CDATA['
	int count = 0;
	while (*P && count < 8) {
		next_char();
		++count;
	}

	if (!*P) {
		node_name = "";
		return true;
	}

	const char *cDataBegin = P;
	const char *cDataEnd = nullptr;

	// find end of CDATA
	while (*P && !cDataEnd) {
		if (*P == '>' &&
				(*(P - 1) == ']') &&
				(*(P - 2) == ']')) {
			cDataEnd = P - 2;
		}

		next_char();
	}

	if (!cDataEnd) {
		cDataEnd = P;
	}
	node_name = String::utf8(cDataBegin, (int)(cDataEnd - cDataBegin));
#ifdef DEBUG_XML
	print_line("XML CDATA: " + node_name);
#endif

	return true;
}

void XMLParser::_parse_comment() {
	node_type = NODE_COMMENT;
	P += 1;

	const char *pEndOfInput = data + length;
	const char *pCommentBegin;
	const char *pCommentEnd;

	if (P + 1 < pEndOfInput && P[0] == '-' && P[1] == '-') {
		// Comment, use '-->' as end.
		pCommentBegin = P + 2;
		for (pCommentEnd = pCommentBegin; pCommentEnd + 2 < pEndOfInput; pCommentEnd++) {
			if (pCommentEnd[0] == '-' && pCommentEnd[1] == '-' && pCommentEnd[2] == '>') {
				break;
			}
		}
		if (pCommentEnd + 2 < pEndOfInput) {
			P = pCommentEnd + 3;
		} else {
			P = pCommentEnd = pEndOfInput;
		}
	} else {
		// Like document type definition, match angle brackets.
		pCommentBegin = P;

		int count = 1;
		while (*P && count) {
			if (*P == '>') {
				--count;
			} else if (*P == '<') {
				++count;
			}
			next_char();
		}

		if (count) {
			pCommentEnd = P;
		} else {
			pCommentEnd = P - 1;
		}
	}

	node_name = String::utf8(pCommentBegin, (int)(pCommentEnd - pCommentBegin));
#ifdef DEBUG_XML
	print_line("XML COMMENT: " + node_name);
#endif
}

void XMLParser::_parse_opening_xml_element() {
	node_type = NODE_ELEMENT;
	node_empty = false;
	attributes.clear();

	// find name
	const char *startName = P;

	// find end of element
	while (*P && *P != '>' && !_is_white_space(*P)) {
		next_char();
	}

	const char *endName = P;

	// find attributes
	while (*P && *P != '>') {
		if (_is_white_space(*P)) {
			next_char();
		} else {
			if (*P != '/') {
				// we've got an attribute

				// read the attribute names
				const char *attributeNameBegin = P;

				while (*P && !_is_white_space(*P) && *P != '=') {
					next_char();
				}

				if (!*P) {
					break;
				}

				const char *attributeNameEnd = P;
				next_char();

				// read the attribute value
				// check for quotes and single quotes, thx to murphy
				while ((*P != '\"') && (*P != '\'') && *P) {
					next_char();
				}

				if (!*P) { // malformatted xml file
					break;
				}

				const char attributeQuoteChar = *P;

				next_char();
				const char *attributeValueBegin = P;

				while (*P != attributeQuoteChar && *P) {
					next_char();
				}

				const char *attributeValueEnd = P;
				if (*P) {
					next_char();
				}

				Attribute attr;
				attr.name = String::utf8(attributeNameBegin,
						(int)(attributeNameEnd - attributeNameBegin));

				String s = String::utf8(attributeValueBegin,
						(int)(attributeValueEnd - attributeValueBegin));

				attr.value = s.xml_unescape();
				attributes.push_back(attr);
			} else {
				// tag is closed directly
				next_char();
				node_empty = true;
				break;
			}
		}
	}

	// check if this tag is closing directly
	if (endName > startName && *(endName - 1) == '/') {
		// directly closing tag
		node_empty = true;
		endName--;
	}

	node_name = String::utf8(startName, (int)(endName - startName));
#ifdef DEBUG_XML
	print_line("XML OPEN: " + node_name);
#endif

	if (*P) {
		next_char();
	}
}

void XMLParser::_parse_current_node() {
	const char *start = P;
	node_offset = P - data;

	// more forward until '<' found
	while (*P != '<' && *P) {
		next_char();
	}

	if (P - start > 0) {
		// we found some text, store it
		if (_set_text(start, P)) {
			return;
		}
	}

	if (!*P) {
		return;
	}

	next_char();

	// based on current token, parse and report next element
	switch (*P) {
		case '/':
			_parse_closing_xml_element();
			break;
		case '?':
			_ignore_definition();
			break;
		case '!':
			if (!_parse_cdata()) {
				_parse_comment();
			}
			break;
		default:
			_parse_opening_xml_element();
			break;
	}
}

uint64_t XMLParser::get_node_offset() const {
	return node_offset;
}

Error XMLParser::seek(uint64_t p_pos) {
	ERR_FAIL_NULL_V(data, ERR_FILE_EOF);
	ERR_FAIL_COND_V(p_pos >= length, ERR_FILE_EOF);

	P = data + p_pos;

	return read();
}

void XMLParser::_bind_methods() {
	ClassDB::bind_method(D_METHOD("read"), &XMLParser::read);
	ClassDB::bind_method(D_METHOD("get_node_type"), &XMLParser::get_node_type);
	ClassDB::bind_method(D_METHOD("get_node_name"), &XMLParser::get_node_name);
	ClassDB::bind_method(D_METHOD("get_node_data"), &XMLParser::get_node_data);
	ClassDB::bind_method(D_METHOD("get_node_offset"), &XMLParser::get_node_offset);
	ClassDB::bind_method(D_METHOD("get_attribute_count"), &XMLParser::get_attribute_count);
	ClassDB::bind_method(D_METHOD("get_attribute_name", "idx"), &XMLParser::get_attribute_name);
	ClassDB::bind_method(D_METHOD("get_attribute_value", "idx"), &XMLParser::get_attribute_value);
	ClassDB::bind_method(D_METHOD("has_attribute", "name"), &XMLParser::has_attribute);
	ClassDB::bind_method(D_METHOD("get_named_attribute_value", "name"), &XMLParser::get_named_attribute_value);
	ClassDB::bind_method(D_METHOD("get_named_attribute_value_safe", "name"), &XMLParser::get_named_attribute_value_safe);
	ClassDB::bind_method(D_METHOD("is_empty"), &XMLParser::is_empty);
	ClassDB::bind_method(D_METHOD("get_current_line"), &XMLParser::get_current_line);
	ClassDB::bind_method(D_METHOD("skip_section"), &XMLParser::skip_section);
	ClassDB::bind_method(D_METHOD("seek", "position"), &XMLParser::seek);
	ClassDB::bind_method(D_METHOD("open", "file"), &XMLParser::open);
	ClassDB::bind_method(D_METHOD("open_buffer", "buffer"), &XMLParser::open_buffer);

	BIND_ENUM_CONSTANT(NODE_NONE);
	BIND_ENUM_CONSTANT(NODE_ELEMENT);
	BIND_ENUM_CONSTANT(NODE_ELEMENT_END);
	BIND_ENUM_CONSTANT(NODE_TEXT);
	BIND_ENUM_CONSTANT(NODE_COMMENT);
	BIND_ENUM_CONSTANT(NODE_CDATA);
	BIND_ENUM_CONSTANT(NODE_UNKNOWN);
}

Error XMLParser::read() {
	// if end not reached, parse the node
	if (P && (P - data) < (int64_t)length - 1 && *P != 0) {
		_parse_current_node();
		return OK;
	}

	return ERR_FILE_EOF;
}

XMLParser::NodeType XMLParser::get_node_type() {
	return node_type;
}

String XMLParser::get_node_data() const {
	ERR_FAIL_COND_V(node_type != NODE_TEXT, "");
	return node_name;
}

String XMLParser::get_node_name() const {
	ERR_FAIL_COND_V(node_type == NODE_TEXT, "");
	return node_name;
}

int XMLParser::get_attribute_count() const {
	return attributes.size();
}

String XMLParser::get_attribute_name(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, attributes.size(), "");
	return attributes[p_idx].name;
}

String XMLParser::get_attribute_value(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, attributes.size(), "");
	return attributes[p_idx].value;
}

bool XMLParser::has_attribute(const String &p_name) const {
	for (int i = 0; i < attributes.size(); i++) {
		if (attributes[i].name == p_name) {
			return true;
		}
	}

	return false;
}

String XMLParser::get_named_attribute_value(const String &p_name) const {
	int idx = -1;
	for (int i = 0; i < attributes.size(); i++) {
		if (attributes[i].name == p_name) {
			idx = i;
			break;
		}
	}

	ERR_FAIL_COND_V_MSG(idx < 0, "", vformat("Attribute not found: '%s'.", p_name));

	return attributes[idx].value;
}

String XMLParser::get_named_attribute_value_safe(const String &p_name) const {
	int idx = -1;
	for (int i = 0; i < attributes.size(); i++) {
		if (attributes[i].name == p_name) {
			idx = i;
			break;
		}
	}

	if (idx < 0) {
		return "";
	}
	return attributes[idx].value;
}

bool XMLParser::is_empty() const {
	return node_empty;
}

Error XMLParser::open_buffer(const Vector<uint8_t> &p_buffer) {
	ERR_FAIL_COND_V(p_buffer.is_empty(), ERR_INVALID_DATA);

	if (data_copy) {
		memdelete_arr(data_copy);
		data_copy = nullptr;
	}

	length = p_buffer.size();
	data_copy = memnew_arr(char, length + 1);
	memcpy(data_copy, p_buffer.ptr(), length);
	data_copy[length] = 0;
	data = data_copy;
	P = data;
	current_line = 0;

	return OK;
}

Error XMLParser::_open_buffer(const uint8_t *p_buffer, size_t p_size) {
	ERR_FAIL_COND_V(p_size == 0, ERR_INVALID_DATA);
	ERR_FAIL_NULL_V(p_buffer, ERR_INVALID_DATA);

	if (data_copy) {
		memdelete_arr(data_copy);
		data_copy = nullptr;
	}

	length = p_size;
	data = (const char *)p_buffer;
	P = data;
	current_line = 0;

	return OK;
}

Error XMLParser::open(const String &p_path) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot open file '%s'.", p_path));

	length = file->get_length();
	ERR_FAIL_COND_V(length < 1, ERR_FILE_CORRUPT);

	if (data_copy) {
		memdelete_arr(data_copy);
		data_copy = nullptr;
	}

	data_copy = memnew_arr(char, length + 1);
	file->get_buffer((uint8_t *)data_copy, length);
	data_copy[length] = 0;
	data = data_copy;
	P = data;
	current_line = 0;

	return OK;
}

void XMLParser::skip_section() {
	// skip if this element is empty anyway.
	if (is_empty()) {
		return;
	}

	// read until we've reached the last element in this section
	int tagcount = 1;

	while (tagcount && read() == OK) {
		if (get_node_type() == XMLParser::NODE_ELEMENT &&
				!is_empty()) {
			++tagcount;
		} else if (get_node_type() == XMLParser::NODE_ELEMENT_END) {
			--tagcount;
		}
	}
}

void XMLParser::close() {
	if (data_copy) {
		memdelete_arr(data);
		data_copy = nullptr;
	}
	data = nullptr;
	length = 0;
	P = nullptr;
	node_empty = false;
	node_type = NODE_NONE;
	node_offset = 0;
}

int XMLParser::get_current_line() const {
	return current_line;
}

XMLParser::~XMLParser() {
	if (data_copy) {
		memdelete_arr(data_copy);
		data_copy = nullptr;
	}
}
