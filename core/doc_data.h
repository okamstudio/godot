/**************************************************************************/
/*  doc_data.h                                                            */
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

#ifndef DOC_DATA_H
#define DOC_DATA_H

#include "core/io/xml_parser.h"
#include "core/variant/variant.h"

class DocData {
public:
	struct ArgumentDoc {
		String name;
		String type;
		String enumeration;
		bool is_bitfield = false;
		String default_value;
		bool operator<(const ArgumentDoc &p_arg) const {
			if (name == p_arg.name) {
				return type < p_arg.type;
			}
			return name < p_arg.name;
		}
		static ArgumentDoc from_dict(const Dictionary &p_dict) {
			ArgumentDoc doc;

			if (p_dict.has("name")) {
				doc.name = p_dict["name"];
			}

			if (p_dict.has("type")) {
				doc.type = p_dict["type"];
			}

			if (p_dict.has("enumeration")) {
				doc.enumeration = p_dict["enumeration"];
				if (p_dict.has("is_bitfield")) {
					doc.is_bitfield = p_dict["is_bitfield"];
				}
			}

			if (p_dict.has("default_value")) {
				doc.default_value = p_dict["default_value"];
			}

			return doc;
		}
		static Dictionary to_dict(const ArgumentDoc &p_doc) {
			Dictionary dict;

			if (!p_doc.name.is_empty()) {
				dict["name"] = p_doc.name;
			}

			if (!p_doc.type.is_empty()) {
				dict["type"] = p_doc.type;
			}

			if (!p_doc.enumeration.is_empty()) {
				dict["enumeration"] = p_doc.enumeration;
				dict["is_bitfield"] = p_doc.is_bitfield;
			}

			if (!p_doc.default_value.is_empty()) {
				dict["default_value"] = p_doc.default_value;
			}

			return dict;
		}
	};

	struct MethodDoc {
		String name;
		String return_type;
		String return_enum;
		bool return_is_bitfield = false;
		String qualifiers;
		String description;
		bool is_deprecated = false;
		String deprecated_message;
		bool is_experimental = false;
		String experimental_message;
		Vector<ArgumentDoc> arguments;
		Vector<int> errors_returned;
		String keywords;
		bool operator<(const MethodDoc &p_method) const {
			if (name == p_method.name) {
				// Must be an operator or a constructor since there is no other overloading
				if (name.left(8) == "operator") {
					if (arguments.size() == p_method.arguments.size()) {
						if (arguments.size() == 0) {
							return false;
						}
						return arguments[0].type < p_method.arguments[0].type;
					}
					return arguments.size() < p_method.arguments.size();
				} else {
					// Must be a constructor
					// We want this arbitrary order for a class "Foo":
					// - 1. Default constructor: Foo()
					// - 2. Copy constructor: Foo(Foo)
					// - 3+. Other constructors Foo(Bar, ...) based on first argument's name
					if (arguments.size() == 0 || p_method.arguments.size() == 0) { // 1.
						return arguments.size() < p_method.arguments.size();
					}
					if (arguments[0].type == return_type || p_method.arguments[0].type == p_method.return_type) { // 2.
						return (arguments[0].type == return_type) || (p_method.arguments[0].type != p_method.return_type);
					}
					return arguments[0] < p_method.arguments[0];
				}
			}
			return name.naturalcasecmp_to(p_method.name) < 0;
		}
		static MethodDoc from_dict(const Dictionary &p_dict) {
			MethodDoc doc;

			if (p_dict.has("name")) {
				doc.name = p_dict["name"];
			}

			if (p_dict.has("return_type")) {
				doc.return_type = p_dict["return_type"];
			}

			if (p_dict.has("return_enum")) {
				doc.return_enum = p_dict["return_enum"];
				if (p_dict.has("return_is_bitfield")) {
					doc.return_is_bitfield = p_dict["return_is_bitfield"];
				}
			}

			if (p_dict.has("qualifiers")) {
				doc.qualifiers = p_dict["qualifiers"];
			}

			if (p_dict.has("description")) {
				doc.description = p_dict["description"];
			}

#ifndef DISABLE_DEPRECATED
			if (p_dict.has("is_deprecated")) {
				doc.is_deprecated = p_dict["is_deprecated"];
			}

			if (p_dict.has("is_experimental")) {
				doc.is_experimental = p_dict["is_experimental"];
			}
#endif

			if (p_dict.has("deprecated")) {
				doc.is_deprecated = true;
				doc.deprecated_message = p_dict["deprecated"];
			}

			if (p_dict.has("experimental")) {
				doc.is_experimental = true;
				doc.experimental_message = p_dict["experimental"];
			}

			Array arguments;
			if (p_dict.has("arguments")) {
				arguments = p_dict["arguments"];
			}
			for (int i = 0; i < arguments.size(); i++) {
				doc.arguments.push_back(ArgumentDoc::from_dict(arguments[i]));
			}

			Array errors_returned;
			if (p_dict.has("errors_returned")) {
				errors_returned = p_dict["errors_returned"];
			}
			for (int i = 0; i < errors_returned.size(); i++) {
				doc.errors_returned.push_back(errors_returned[i]);
			}

			if (p_dict.has("keywords")) {
				doc.keywords = p_dict["keywords"];
			}

			return doc;
		}
		static Dictionary to_dict(const MethodDoc &p_doc) {
			Dictionary dict;

			if (!p_doc.name.is_empty()) {
				dict["name"] = p_doc.name;
			}

			if (!p_doc.return_type.is_empty()) {
				dict["return_type"] = p_doc.return_type;
			}

			if (!p_doc.return_enum.is_empty()) {
				dict["return_enum"] = p_doc.return_enum;
				dict["return_is_bitfield"] = p_doc.return_is_bitfield;
			}

			if (!p_doc.qualifiers.is_empty()) {
				dict["qualifiers"] = p_doc.qualifiers;
			}

			if (!p_doc.description.is_empty()) {
				dict["description"] = p_doc.description;
			}

			if (p_doc.is_deprecated) {
				dict["deprecated"] = p_doc.deprecated_message;
			}

			if (p_doc.is_experimental) {
				dict["experimental"] = p_doc.experimental_message;
			}

			if (!p_doc.keywords.is_empty()) {
				dict["keywords"] = p_doc.keywords;
			}

			if (!p_doc.arguments.is_empty()) {
				Array arguments;
				for (int i = 0; i < p_doc.arguments.size(); i++) {
					arguments.push_back(ArgumentDoc::to_dict(p_doc.arguments[i]));
				}
				dict["arguments"] = arguments;
			}

			if (!p_doc.errors_returned.is_empty()) {
				Array errors_returned;
				for (int i = 0; i < p_doc.errors_returned.size(); i++) {
					errors_returned.push_back(p_doc.errors_returned[i]);
				}
				dict["errors_returned"] = errors_returned;
			}

			return dict;
		}
	};

	struct ConstantDoc {
		String name;
		String value;
		bool is_value_valid = false;
		String type;
		String enumeration;
		bool is_bitfield = false;
		String description;
		bool is_deprecated = false;
		String deprecated_message;
		bool is_experimental = false;
		String experimental_message;
		String keywords;
		bool operator<(const ConstantDoc &p_const) const {
			return name < p_const.name;
		}
		static ConstantDoc from_dict(const Dictionary &p_dict) {
			ConstantDoc doc;

			if (p_dict.has("name")) {
				doc.name = p_dict["name"];
			}

			if (p_dict.has("value")) {
				doc.value = p_dict["value"];
			}

			if (p_dict.has("is_value_valid")) {
				doc.is_value_valid = p_dict["is_value_valid"];
			}

			if (p_dict.has("type")) {
				doc.type = p_dict["type"];
			}

			if (p_dict.has("enumeration")) {
				doc.enumeration = p_dict["enumeration"];
				if (p_dict.has("is_bitfield")) {
					doc.is_bitfield = p_dict["is_bitfield"];
				}
			}

			if (p_dict.has("description")) {
				doc.description = p_dict["description"];
			}

#ifndef DISABLE_DEPRECATED
			if (p_dict.has("is_deprecated")) {
				doc.is_deprecated = p_dict["is_deprecated"];
			}

			if (p_dict.has("is_experimental")) {
				doc.is_experimental = p_dict["is_experimental"];
			}
#endif

			if (p_dict.has("deprecated")) {
				doc.is_deprecated = true;
				doc.deprecated_message = p_dict["deprecated"];
			}

			if (p_dict.has("experimental")) {
				doc.is_experimental = true;
				doc.experimental_message = p_dict["experimental"];
			}

			if (p_dict.has("keywords")) {
				doc.keywords = p_dict["keywords"];
			}

			return doc;
		}
		static Dictionary to_dict(const ConstantDoc &p_doc) {
			Dictionary dict;

			if (!p_doc.name.is_empty()) {
				dict["name"] = p_doc.name;
			}

			if (!p_doc.value.is_empty()) {
				dict["value"] = p_doc.value;
			}

			dict["is_value_valid"] = p_doc.is_value_valid;

			dict["type"] = p_doc.type;

			if (!p_doc.enumeration.is_empty()) {
				dict["enumeration"] = p_doc.enumeration;
				dict["is_bitfield"] = p_doc.is_bitfield;
			}

			if (!p_doc.description.is_empty()) {
				dict["description"] = p_doc.description;
			}

			if (p_doc.is_deprecated) {
				dict["deprecated"] = p_doc.deprecated_message;
			}

			if (p_doc.is_experimental) {
				dict["experimental"] = p_doc.experimental_message;
			}

			if (!p_doc.keywords.is_empty()) {
				dict["keywords"] = p_doc.keywords;
			}

			return dict;
		}
	};

	struct PropertyDoc {
		String name;
		String type;
		String enumeration;
		bool is_bitfield = false;
		String description;
		String setter, getter;
		String default_value;
		bool overridden = false;
		String overrides;
		bool is_deprecated = false;
		String deprecated_message;
		bool is_experimental = false;
		String experimental_message;
		String keywords;
		bool operator<(const PropertyDoc &p_prop) const {
			return name.naturalcasecmp_to(p_prop.name) < 0;
		}
		static PropertyDoc from_dict(const Dictionary &p_dict) {
			PropertyDoc doc;

			if (p_dict.has("name")) {
				doc.name = p_dict["name"];
			}

			if (p_dict.has("type")) {
				doc.type = p_dict["type"];
			}

			if (p_dict.has("enumeration")) {
				doc.enumeration = p_dict["enumeration"];
				if (p_dict.has("is_bitfield")) {
					doc.is_bitfield = p_dict["is_bitfield"];
				}
			}

			if (p_dict.has("description")) {
				doc.description = p_dict["description"];
			}

			if (p_dict.has("setter")) {
				doc.setter = p_dict["setter"];
			}

			if (p_dict.has("getter")) {
				doc.getter = p_dict["getter"];
			}

			if (p_dict.has("default_value")) {
				doc.default_value = p_dict["default_value"];
			}

			if (p_dict.has("overridden")) {
				doc.overridden = p_dict["overridden"];
			}

			if (p_dict.has("overrides")) {
				doc.overrides = p_dict["overrides"];
			}

#ifndef DISABLE_DEPRECATED
			if (p_dict.has("is_deprecated")) {
				doc.is_deprecated = p_dict["is_deprecated"];
			}

			if (p_dict.has("is_experimental")) {
				doc.is_experimental = p_dict["is_experimental"];
			}
#endif

			if (p_dict.has("deprecated")) {
				doc.is_deprecated = true;
				doc.deprecated_message = p_dict["deprecated"];
			}

			if (p_dict.has("experimental")) {
				doc.is_experimental = true;
				doc.experimental_message = p_dict["experimental"];
			}

			if (p_dict.has("keywords")) {
				doc.keywords = p_dict["keywords"];
			}

			return doc;
		}
		static Dictionary to_dict(const PropertyDoc &p_doc) {
			Dictionary dict;

			if (!p_doc.name.is_empty()) {
				dict["name"] = p_doc.name;
			}

			if (!p_doc.type.is_empty()) {
				dict["type"] = p_doc.type;
			}

			if (!p_doc.enumeration.is_empty()) {
				dict["enumeration"] = p_doc.enumeration;
				dict["is_bitfield"] = p_doc.is_bitfield;
			}

			if (!p_doc.description.is_empty()) {
				dict["description"] = p_doc.description;
			}

			if (!p_doc.setter.is_empty()) {
				dict["setter"] = p_doc.setter;
			}

			if (!p_doc.getter.is_empty()) {
				dict["getter"] = p_doc.getter;
			}

			if (!p_doc.default_value.is_empty()) {
				dict["default_value"] = p_doc.default_value;
			}

			dict["overridden"] = p_doc.overridden;

			if (!p_doc.overrides.is_empty()) {
				dict["overrides"] = p_doc.overrides;
			}

			if (p_doc.is_deprecated) {
				dict["deprecated"] = p_doc.deprecated_message;
			}

			if (p_doc.is_experimental) {
				dict["experimental"] = p_doc.experimental_message;
			}

			if (!p_doc.keywords.is_empty()) {
				dict["keywords"] = p_doc.keywords;
			}

			return dict;
		}
	};

	struct ThemeItemDoc {
		String name;
		String type;
		String data_type;
		String description;
		bool is_deprecated = false;
		String deprecated_message;
		bool is_experimental = false;
		String experimental_message;
		String default_value;
		String keywords;
		bool operator<(const ThemeItemDoc &p_theme_item) const {
			// First sort by the data type, then by name.
			if (data_type == p_theme_item.data_type) {
				return name.naturalcasecmp_to(p_theme_item.name) < 0;
			}
			return data_type < p_theme_item.data_type;
		}
		static ThemeItemDoc from_dict(const Dictionary &p_dict) {
			ThemeItemDoc doc;

			if (p_dict.has("name")) {
				doc.name = p_dict["name"];
			}

			if (p_dict.has("type")) {
				doc.type = p_dict["type"];
			}

			if (p_dict.has("data_type")) {
				doc.data_type = p_dict["data_type"];
			}

			if (p_dict.has("description")) {
				doc.description = p_dict["description"];
			}

			if (p_dict.has("deprecated")) {
				doc.is_deprecated = true;
				doc.deprecated_message = p_dict["deprecated"];
			}

			if (p_dict.has("experimental")) {
				doc.is_experimental = true;
				doc.experimental_message = p_dict["experimental"];
			}

			if (p_dict.has("default_value")) {
				doc.default_value = p_dict["default_value"];
			}

			if (p_dict.has("keywords")) {
				doc.keywords = p_dict["keywords"];
			}

			return doc;
		}
		static Dictionary to_dict(const ThemeItemDoc &p_doc) {
			Dictionary dict;

			if (!p_doc.name.is_empty()) {
				dict["name"] = p_doc.name;
			}

			if (!p_doc.type.is_empty()) {
				dict["type"] = p_doc.type;
			}

			if (!p_doc.data_type.is_empty()) {
				dict["data_type"] = p_doc.data_type;
			}

			if (!p_doc.description.is_empty()) {
				dict["description"] = p_doc.description;
			}

			if (p_doc.is_deprecated) {
				dict["deprecated"] = p_doc.deprecated_message;
			}

			if (p_doc.is_experimental) {
				dict["experimental"] = p_doc.experimental_message;
			}

			if (!p_doc.default_value.is_empty()) {
				dict["default_value"] = p_doc.default_value;
			}

			if (!p_doc.keywords.is_empty()) {
				dict["keywords"] = p_doc.keywords;
			}

			return dict;
		}
	};

	struct TutorialDoc {
		String link;
		String title;
		static TutorialDoc from_dict(const Dictionary &p_dict) {
			TutorialDoc doc;

			if (p_dict.has("link")) {
				doc.link = p_dict["link"];
			}

			if (p_dict.has("title")) {
				doc.title = p_dict["title"];
			}

			return doc;
		}
		static Dictionary to_dict(const TutorialDoc &p_doc) {
			Dictionary dict;

			if (!p_doc.link.is_empty()) {
				dict["link"] = p_doc.link;
			}

			if (!p_doc.title.is_empty()) {
				dict["title"] = p_doc.title;
			}

			return dict;
		}
	};

	struct EnumDoc {
		String description;
		bool is_deprecated = false;
		String deprecated_message;
		bool is_experimental = false;
		String experimental_message;
		static EnumDoc from_dict(const Dictionary &p_dict) {
			EnumDoc doc;

			if (p_dict.has("description")) {
				doc.description = p_dict["description"];
			}

#ifndef DISABLE_DEPRECATED
			if (p_dict.has("is_deprecated")) {
				doc.is_deprecated = p_dict["is_deprecated"];
			}

			if (p_dict.has("is_experimental")) {
				doc.is_experimental = p_dict["is_experimental"];
			}
#endif

			if (p_dict.has("deprecated")) {
				doc.is_deprecated = true;
				doc.deprecated_message = p_dict["deprecated"];
			}

			if (p_dict.has("experimental")) {
				doc.is_experimental = true;
				doc.experimental_message = p_dict["experimental"];
			}

			return doc;
		}
		static Dictionary to_dict(const EnumDoc &p_doc) {
			Dictionary dict;

			if (!p_doc.description.is_empty()) {
				dict["description"] = p_doc.description;
			}

			if (p_doc.is_deprecated) {
				dict["deprecated"] = p_doc.deprecated_message;
			}

			if (p_doc.is_experimental) {
				dict["experimental"] = p_doc.experimental_message;
			}

			return dict;
		}
	};

	struct ClassDoc {
		String name;
		String inherits;
		String brief_description;
		String description;
		String keywords;
		Vector<TutorialDoc> tutorials;
		Vector<MethodDoc> constructors;
		Vector<MethodDoc> methods;
		Vector<MethodDoc> operators;
		Vector<MethodDoc> signals;
		Vector<ConstantDoc> constants;
		HashMap<String, EnumDoc> enums;
		Vector<PropertyDoc> properties;
		Vector<MethodDoc> annotations;
		Vector<ThemeItemDoc> theme_properties;
		bool is_deprecated = false;
		String deprecated_message;
		bool is_experimental = false;
		String experimental_message;
		bool is_script_doc = false;
		String script_path;
		bool operator<(const ClassDoc &p_class) const {
			return name < p_class.name;
		}
		static ClassDoc from_dict(const Dictionary &p_dict) {
			ClassDoc doc;

			if (p_dict.has("name")) {
				doc.name = p_dict["name"];
			}

			if (p_dict.has("inherits")) {
				doc.inherits = p_dict["inherits"];
			}

			if (p_dict.has("brief_description")) {
				doc.brief_description = p_dict["brief_description"];
			}

			if (p_dict.has("description")) {
				doc.description = p_dict["description"];
			}

			if (p_dict.has("keywords")) {
				doc.keywords = p_dict["keywords"];
			}

			Array tutorials;
			if (p_dict.has("tutorials")) {
				tutorials = p_dict["tutorials"];
			}
			for (int i = 0; i < tutorials.size(); i++) {
				doc.tutorials.push_back(TutorialDoc::from_dict(tutorials[i]));
			}

			Array constructors;
			if (p_dict.has("constructors")) {
				constructors = p_dict["constructors"];
			}
			for (int i = 0; i < constructors.size(); i++) {
				doc.constructors.push_back(MethodDoc::from_dict(constructors[i]));
			}

			Array methods;
			if (p_dict.has("methods")) {
				methods = p_dict["methods"];
			}
			for (int i = 0; i < methods.size(); i++) {
				doc.methods.push_back(MethodDoc::from_dict(methods[i]));
			}

			Array operators;
			if (p_dict.has("operators")) {
				operators = p_dict["operators"];
			}
			for (int i = 0; i < operators.size(); i++) {
				doc.operators.push_back(MethodDoc::from_dict(operators[i]));
			}

			Array signals;
			if (p_dict.has("signals")) {
				signals = p_dict["signals"];
			}
			for (int i = 0; i < signals.size(); i++) {
				doc.signals.push_back(MethodDoc::from_dict(signals[i]));
			}

			Array constants;
			if (p_dict.has("constants")) {
				constants = p_dict["constants"];
			}
			for (int i = 0; i < constants.size(); i++) {
				doc.constants.push_back(ConstantDoc::from_dict(constants[i]));
			}

			Dictionary enums;
			if (p_dict.has("enums")) {
				enums = p_dict["enums"];
			}
			for (int i = 0; i < enums.size(); i++) {
				doc.enums[enums.get_key_at_index(i)] = EnumDoc::from_dict(enums.get_value_at_index(i));
			}

			Array properties;
			if (p_dict.has("properties")) {
				properties = p_dict["properties"];
			}
			for (int i = 0; i < properties.size(); i++) {
				doc.properties.push_back(PropertyDoc::from_dict(properties[i]));
			}

			Array annotations;
			if (p_dict.has("annotations")) {
				annotations = p_dict["annotations"];
			}
			for (int i = 0; i < annotations.size(); i++) {
				doc.annotations.push_back(MethodDoc::from_dict(annotations[i]));
			}

			Array theme_properties;
			if (p_dict.has("theme_properties")) {
				theme_properties = p_dict["theme_properties"];
			}
			for (int i = 0; i < theme_properties.size(); i++) {
				doc.theme_properties.push_back(ThemeItemDoc::from_dict(theme_properties[i]));
			}

#ifndef DISABLE_DEPRECATED
			if (p_dict.has("is_deprecated")) {
				doc.is_deprecated = p_dict["is_deprecated"];
			}

			if (p_dict.has("is_experimental")) {
				doc.is_experimental = p_dict["is_experimental"];
			}
#endif

			if (p_dict.has("deprecated")) {
				doc.is_deprecated = true;
				doc.deprecated_message = p_dict["deprecated"];
			}

			if (p_dict.has("experimental")) {
				doc.is_experimental = true;
				doc.experimental_message = p_dict["experimental"];
			}

			if (p_dict.has("is_script_doc")) {
				doc.is_script_doc = p_dict["is_script_doc"];
			}

			if (p_dict.has("script_path")) {
				doc.script_path = p_dict["script_path"];
			}

			return doc;
		}
		static Dictionary to_dict(const ClassDoc &p_doc) {
			Dictionary dict;

			if (!p_doc.name.is_empty()) {
				dict["name"] = p_doc.name;
			}

			if (!p_doc.inherits.is_empty()) {
				dict["inherits"] = p_doc.inherits;
			}

			if (!p_doc.brief_description.is_empty()) {
				dict["brief_description"] = p_doc.brief_description;
			}

			if (!p_doc.description.is_empty()) {
				dict["description"] = p_doc.description;
			}

			if (!p_doc.tutorials.is_empty()) {
				Array tutorials;
				for (int i = 0; i < p_doc.tutorials.size(); i++) {
					tutorials.push_back(TutorialDoc::to_dict(p_doc.tutorials[i]));
				}
				dict["tutorials"] = tutorials;
			}

			if (!p_doc.constructors.is_empty()) {
				Array constructors;
				for (int i = 0; i < p_doc.constructors.size(); i++) {
					constructors.push_back(MethodDoc::to_dict(p_doc.constructors[i]));
				}
				dict["constructors"] = constructors;
			}

			if (!p_doc.methods.is_empty()) {
				Array methods;
				for (int i = 0; i < p_doc.methods.size(); i++) {
					methods.push_back(MethodDoc::to_dict(p_doc.methods[i]));
				}
				dict["methods"] = methods;
			}

			if (!p_doc.operators.is_empty()) {
				Array operators;
				for (int i = 0; i < p_doc.operators.size(); i++) {
					operators.push_back(MethodDoc::to_dict(p_doc.operators[i]));
				}
				dict["operators"] = operators;
			}

			if (!p_doc.signals.is_empty()) {
				Array signals;
				for (int i = 0; i < p_doc.signals.size(); i++) {
					signals.push_back(MethodDoc::to_dict(p_doc.signals[i]));
				}
				dict["signals"] = signals;
			}

			if (!p_doc.constants.is_empty()) {
				Array constants;
				for (int i = 0; i < p_doc.constants.size(); i++) {
					constants.push_back(ConstantDoc::to_dict(p_doc.constants[i]));
				}
				dict["constants"] = constants;
			}

			if (!p_doc.enums.is_empty()) {
				Dictionary enums;
				for (const KeyValue<String, EnumDoc> &E : p_doc.enums) {
					enums[E.key] = EnumDoc::to_dict(E.value);
				}
				dict["enums"] = enums;
			}

			if (!p_doc.properties.is_empty()) {
				Array properties;
				for (int i = 0; i < p_doc.properties.size(); i++) {
					properties.push_back(PropertyDoc::to_dict(p_doc.properties[i]));
				}
				dict["properties"] = properties;
			}

			if (!p_doc.annotations.is_empty()) {
				Array annotations;
				for (int i = 0; i < p_doc.annotations.size(); i++) {
					annotations.push_back(MethodDoc::to_dict(p_doc.annotations[i]));
				}
				dict["annotations"] = annotations;
			}

			if (!p_doc.theme_properties.is_empty()) {
				Array theme_properties;
				for (int i = 0; i < p_doc.theme_properties.size(); i++) {
					theme_properties.push_back(ThemeItemDoc::to_dict(p_doc.theme_properties[i]));
				}
				dict["theme_properties"] = theme_properties;
			}

			if (p_doc.is_deprecated) {
				dict["deprecated"] = p_doc.deprecated_message;
			}

			if (p_doc.is_experimental) {
				dict["experimental"] = p_doc.experimental_message;
			}

			dict["is_script_doc"] = p_doc.is_script_doc;

			if (!p_doc.script_path.is_empty()) {
				dict["script_path"] = p_doc.script_path;
			}

			if (!p_doc.keywords.is_empty()) {
				dict["keywords"] = p_doc.keywords;
			}

			return dict;
		}
	};

	static String get_default_value_string(const Variant &p_value);

	static void return_doc_from_retinfo(DocData::MethodDoc &p_method, const PropertyInfo &p_retinfo);
	static void argument_doc_from_arginfo(DocData::ArgumentDoc &p_argument, const PropertyInfo &p_arginfo);
	static void method_doc_from_methodinfo(DocData::MethodDoc &p_method, const MethodInfo &p_methodinfo, const String &p_desc);
};

#endif // DOC_DATA_H
