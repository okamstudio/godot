/**************************************************************************/
/*  editor_configuration_info.cpp                                         */
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

#include "editor_configuration_info.h"

#include "editor/editor_property_name_processor.h"
#include "scene/main/node.h"
#include "servers/text_server.h"

Vector<ConfigurationInfo> EditorConfigurationInfo::get_configuration_info(Object *p_object) {
	Vector<ConfigurationInfo> config_infos;
	if (!p_object) {
		return config_infos;
	}

	Node *node = Object::cast_to<Node>(p_object);
	if (node) {
		config_infos = node->get_configuration_info();

#ifndef DISABLE_DEPRECATED
		PackedStringArray warnings = node->get_configuration_warnings();
		for (const String &warning : warnings) {
			config_infos.push_back(ConfigurationInfo::from_variant(warning));
		}
#endif
	} else {
		Resource *resource = Object::cast_to<Resource>(p_object);
		if (resource) {
			config_infos = resource->get_configuration_info();
		}
	}

	Vector<ConfigurationInfo> valid_infos;
	for (const ConfigurationInfo &config_info : config_infos) {
		if (config_info.ensure_valid(p_object)) {
			valid_infos.push_back(config_info);
		}
	}
	return valid_infos;
}

ConfigurationInfo::Severity EditorConfigurationInfo::get_max_severity(const Vector<ConfigurationInfo> &p_config_infos) {
	ConfigurationInfo::Severity max_severity = ConfigurationInfo::Severity::NONE;

	for (const ConfigurationInfo &config_info : p_config_infos) {
		ConfigurationInfo::Severity severity = config_info.get_severity();
		if (severity > max_severity) {
			max_severity = severity;
		}
	}

	return max_severity;
}

StringName EditorConfigurationInfo::get_severity_icon(ConfigurationInfo::Severity p_severity) {
	switch (p_severity) {
		case ConfigurationInfo::Severity::ERROR:
			return SNAME("StatusError");
		case ConfigurationInfo::Severity::WARNING:
			return SNAME("NodeWarning");
		case ConfigurationInfo::Severity::INFO:
			return SNAME("NodeInfo");
		default:
			// Use warning icon as fallback.
			return SNAME("NodeWarning");
	}
}

Vector<ConfigurationInfo> EditorConfigurationInfo::filter_list_for_property(const Vector<ConfigurationInfo> &p_config_infos, const StringName &p_property_name) {
	Vector<ConfigurationInfo> result;
	for (const ConfigurationInfo &config_info : p_config_infos) {
		if (config_info.get_property_name() == p_property_name) {
			result.push_back(config_info);
		}
	}
	return result;
}

String EditorConfigurationInfo::format_as_string(const ConfigurationInfo &p_config_info, bool p_wrap_lines, bool p_prefix_property_name) {
	String text;

	const StringName &property_name = p_config_info.get_property_name();
	if (p_prefix_property_name && !property_name.is_empty()) {
		const EditorPropertyNameProcessor::Style style = EditorPropertyNameProcessor::get_default_inspector_style();
		const String styled_property_name = EditorPropertyNameProcessor::get_singleton()->process_name(property_name, style, property_name);

		text = vformat("[%s] %s", styled_property_name, p_config_info.get_message());
	} else {
		text = p_config_info.get_message();
	}

	if (p_wrap_lines) {
		// Limit the line width while keeping some padding.
		// It is not efficient, but it does not have to be.
		const PackedInt32Array boundaries = TS->string_get_word_breaks(text, "", 80);
		PackedStringArray lines;
		for (int bound = 0; bound < boundaries.size(); bound += 2) {
			const int start = boundaries[bound];
			const int end = boundaries[bound + 1];
			String line = text.substr(start, end - start);
			lines.append(line);
		}
		text = String("\n").join(lines).replace("\n", "\n    ");
	}
	return text;
}

String EditorConfigurationInfo::format_list_as_string(const Vector<ConfigurationInfo> &p_config_infos, bool p_wrap_lines, bool p_prefix_property_name) {
	const String bullet_point = U"•  ";
	PackedStringArray all_lines;
	for (const ConfigurationInfo &config_info : p_config_infos) {
		String text = bullet_point + format_as_string(config_info, p_wrap_lines, p_prefix_property_name);
		all_lines.append(text);
	}
	return String("\n").join(all_lines);
}
