/**************************************************************************/
/*  configuration_info.h                                                  */
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

#ifndef CONFIGURATION_INFO_H
#define CONFIGURATION_INFO_H

#ifdef TOOLS_ENABLED

#include "core/templates/hash_set.h"
#include "core/variant/variant.h"

#ifdef ERROR
#undef ERROR // Define from Windows APIs
#endif

#define CONFIG_WARNING(message) infos.push_back(ConfigurationInfo(message, "", ConfigurationInfo::Severity::WARNING));
#define CONFIG_WARNING_P(message, property_name) infos.push_back(ConfigurationInfo(message, property_name, ConfigurationInfo::Severity::WARNING));

class ConfigurationInfo {
public:
	enum class Severity {
		INFO,
		WARNING,
		ERROR,
		MAX,
		NONE = -1,
	};

private:
	String message;
	StringName property_name;
	Severity severity;

	inline static HashSet<String> queued_errors_to_print;

	static void queue_error_print(const String &p_error);
	static void _print_errors_from_queue();

public:
	static void (*configuration_info_changed_func)(Object *p_object);

	static ConfigurationInfo from_variant(const Variant &p_variant);
	static Severity string_to_severity(const String &p_severity);

	bool ensure_valid(Object *p_owner) const;
	String get_message() const { return message; }
	StringName get_property_name() const { return property_name; }
	Severity get_severity() const { return severity; }

	bool operator==(const ConfigurationInfo &p_val) const {
		return (message == p_val.message) &&
				(property_name == p_val.property_name) &&
				(severity == p_val.severity);
	}

	ConfigurationInfo();
	ConfigurationInfo(const Dictionary &p_dict);
	ConfigurationInfo(const String &p_message, const StringName &p_property_name = StringName(), Severity p_severity = Severity::WARNING);
};

#endif // TOOLS_ENABLED

#endif // CONFIGURATION_INFO_H
