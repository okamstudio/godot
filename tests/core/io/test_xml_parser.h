/**************************************************************************/
/*  test_xml_parser.h                                                     */
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

#include "core/io/xml_parser.h"

#include "tests/test_macros.h"

namespace TestXMLParser {
TEST_CASE("[XMLParser] End-to-end") {
	String source = "<?xml version = \"1.0\" encoding=\"UTF-8\" ?>\
<top attr=\"attr value\">\
  Text&lt;&#65;&#x42;&gt;\
</top>";
	Vector<uint8_t> buff = source.to_utf8_buffer();

	XMLParser parser;
	parser.open_buffer(buff);

	// <?xml ...?> gets parsed as NODE_UNKNOWN
	CHECK(parser.read() == OK);
	CHECK(parser.get_node_type() == XMLParser::NodeType::NODE_UNKNOWN);

	CHECK(parser.read() == OK);
	CHECK(parser.get_node_type() == XMLParser::NodeType::NODE_ELEMENT);
	CHECK(parser.get_node_name() == "top");
	CHECK(parser.has_attribute("attr"));
	CHECK(parser.get_named_attribute_value("attr") == "attr value");

	CHECK(parser.read() == OK);
	CHECK(parser.get_node_type() == XMLParser::NodeType::NODE_TEXT);
	CHECK(parser.get_node_data().lstrip(" \t") == "Text<AB>");

	CHECK(parser.read() == OK);
	CHECK(parser.get_node_type() == XMLParser::NodeType::NODE_ELEMENT_END);
	CHECK(parser.get_node_name() == "top");

	parser.close();
}

TEST_CASE("[XMLParser] Comments") {
	XMLParser parser;

	SUBCASE("Missing end of comment") {
		const String input = "<first></first><!-- foo";
		REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
		REQUIRE_EQ(parser.read(), OK);
		REQUIRE_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT);
		REQUIRE_EQ(parser.read(), OK);
		REQUIRE_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT_END);
		REQUIRE_EQ(parser.read(), OK);
		CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_COMMENT);
		CHECK_EQ(parser.get_node_name(), " foo");
	}
	SUBCASE("Bad start of comment") {
		const String input = "<first></first><!-";
		REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
		REQUIRE_EQ(parser.read(), OK);
		REQUIRE_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT);
		REQUIRE_EQ(parser.read(), OK);
		REQUIRE_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT_END);
		REQUIRE_EQ(parser.read(), OK);
		CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_COMMENT);
		CHECK_EQ(parser.get_node_name(), "-");
	}
	SUBCASE("Unblanced angle brackets in comment") {
		const String input = "<!-- example << --><next-tag></next-tag>";
		REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
		REQUIRE_EQ(parser.read(), OK);
		CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_COMMENT);
		CHECK_EQ(parser.get_node_name(), " example << ");
	}
	SUBCASE("Doctype") {
		const String input = "<!DOCTYPE greeting [<!ELEMENT greeting (#PCDATA)>]>";
		REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
		REQUIRE_EQ(parser.read(), OK);
		CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_COMMENT);
		CHECK_EQ(parser.get_node_name(), "DOCTYPE greeting [<!ELEMENT greeting (#PCDATA)>]");
	}
}

TEST_CASE("[XMLParser] Premature endings") {
	SUBCASE("Simple cases") {
		String input;
		String expected_name;
		XMLParser::NodeType expected_type;

		SUBCASE("Incomplete Unknown") {
			input = "<first></first><?xml";
			expected_type = XMLParser::NodeType::NODE_UNKNOWN;
			expected_name = "?xml";
		}
		SUBCASE("Incomplete CDStart") {
			input = "<first></first><![CD";
			expected_type = XMLParser::NodeType::NODE_CDATA;
			expected_name = "";
		}
		SUBCASE("Incomplete CData") {
			input = "<first></first><![CDATA[example";
			expected_type = XMLParser::NodeType::NODE_CDATA;
			expected_name = "example";
		}
		SUBCASE("Incomplete CDEnd") {
			input = "<first></first><![CDATA[example]]";
			expected_type = XMLParser::NodeType::NODE_CDATA;
			expected_name = "example]]";
		}
		SUBCASE("Incomplete start-tag name") {
			input = "<first></first><second";
			expected_type = XMLParser::NodeType::NODE_ELEMENT;
			expected_name = "second";
		}

		XMLParser parser;
		REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
		REQUIRE_EQ(parser.read(), OK);
		REQUIRE_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT);
		REQUIRE_EQ(parser.read(), OK);
		REQUIRE_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT_END);
		REQUIRE_EQ(parser.read(), OK);
		CHECK_EQ(parser.get_node_type(), expected_type);
		CHECK_EQ(parser.get_node_name(), expected_name);
	}

	SUBCASE("With attributes and texts") {
		XMLParser parser;

		SUBCASE("Incomplete start-tag attribute name") {
			const String input = "<first></first><second attr1=\"foo\" attr2";
			REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
			REQUIRE_EQ(parser.read(), OK);
			REQUIRE_EQ(parser.read(), OK);
			REQUIRE_EQ(parser.read(), OK);
			CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT);
			CHECK_EQ(parser.get_node_name(), "second");
			CHECK_EQ(parser.get_attribute_count(), 1);
			CHECK_EQ(parser.get_attribute_name(0), "attr1");
			CHECK_EQ(parser.get_attribute_value(0), "foo");
		}

		SUBCASE("Incomplete start-tag attribute unquoted value") {
			const String input = "<first></first><second attr1=\"foo\" attr2=bar";
			REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
			REQUIRE_EQ(parser.read(), OK);
			REQUIRE_EQ(parser.read(), OK);
			REQUIRE_EQ(parser.read(), OK);
			CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT);
			CHECK_EQ(parser.get_node_name(), "second");
			CHECK_EQ(parser.get_attribute_count(), 1);
			CHECK_EQ(parser.get_attribute_name(0), "attr1");
			CHECK_EQ(parser.get_attribute_value(0), "foo");
		}

		SUBCASE("Incomplete start-tag attribute quoted value") {
			const String input = "<first></first><second attr1=\"foo\" attr2=\"bar";
			REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
			REQUIRE_EQ(parser.read(), OK);
			REQUIRE_EQ(parser.read(), OK);
			REQUIRE_EQ(parser.read(), OK);
			CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT);
			CHECK_EQ(parser.get_node_name(), "second");
			CHECK_EQ(parser.get_attribute_count(), 2);
			CHECK_EQ(parser.get_attribute_name(0), "attr1");
			CHECK_EQ(parser.get_attribute_value(0), "foo");
			CHECK_EQ(parser.get_attribute_name(1), "attr2");
			CHECK_EQ(parser.get_attribute_value(1), "bar");
		}

		SUBCASE("Incomplete end-tag name") {
			const String input = "<first></fir";
			REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
			REQUIRE_EQ(parser.read(), OK);
			REQUIRE_EQ(parser.read(), OK);
			CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT_END);
			CHECK_EQ(parser.get_node_name(), "fir");
		}

		SUBCASE("Trailing text") {
			const String input = "<first></first>example";
			REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
			REQUIRE_EQ(parser.read(), OK);
			REQUIRE_EQ(parser.read(), OK);
			REQUIRE_EQ(parser.read(), OK);
			CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_TEXT);
			CHECK_EQ(parser.get_node_data(), "example");
		}
	}
}

TEST_CASE("[XMLParser] CDATA") {
	const String input = "<a><![CDATA[my cdata content goes here]]></a>";
	XMLParser parser;
	REQUIRE_EQ(parser.open_buffer(input.to_utf8_buffer()), OK);
	REQUIRE_EQ(parser.read(), OK);
	CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT);
	CHECK_EQ(parser.get_node_name(), "a");
	REQUIRE_EQ(parser.read(), OK);
	CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_CDATA);
	CHECK_EQ(parser.get_node_name(), "my cdata content goes here");
	REQUIRE_EQ(parser.read(), OK);
	CHECK_EQ(parser.get_node_type(), XMLParser::NodeType::NODE_ELEMENT_END);
	CHECK_EQ(parser.get_node_name(), "a");
}
} // namespace TestXMLParser
