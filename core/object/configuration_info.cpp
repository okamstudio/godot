/**************************************************************************/
/*  configuration_info.cpp                                                */
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

#include "core/object/callable_method_pointer.h"

#include "configuration_info.h"

#ifdef TOOLS_ENABLED

// Registered in editor, to avoid tight coupling.
void (*ConfigurationInfo::configuration_info_changed_func)(Object *p_object) = nullptr;

void ConfigurationInfo::queue_error_print(const String &p_error) {
	if (queued_errors_to_print.is_empty()) {
		callable_mp_static(&ConfigurationInfo::_print_errors_from_queue).call_deferred();
	}
	queued_errors_to_print.insert(p_error);
}

void ConfigurationInfo::_print_errors_from_queue() {
	for (const String &error : queued_errors_to_print) {
		ERR_PRINT(error);
	}
	queued_errors_to_print.clear();
}

ConfigurationInfo ConfigurationInfo::from_variant(const Variant &p_variant) {
	if (p_variant.get_type() == Variant::STRING) {
		return ConfigurationInfo(String(p_variant));
	} else if (p_variant.get_type() == Variant::DICTIONARY) {
		return ConfigurationInfo(Dictionary(p_variant));
	} else {
		queue_error_print("Attempted to convert a ConfigurationInfo which is neither a string nor a dictionary, but a " + Variant::get_type_name(p_variant.get_type()));
		return ConfigurationInfo();
	}
}

ConfigurationInfo::Severity ConfigurationInfo::string_to_severity(const String &p_severity) {
	if (p_severity == "error") {
		return ConfigurationInfo::Severity::ERROR;
	} else if (p_severity == "warning") {
		return ConfigurationInfo::Severity::WARNING;
	} else if (p_severity == "info") {
		return ConfigurationInfo::Severity::INFO;
	} else {
		queue_error_print("Severity of Configuration Info must be one of \"error\", \"warning\" or \"info\", received \"" + p_severity + "\".");
		return ConfigurationInfo::Severity::NONE;
	}
}

bool ConfigurationInfo::ensure_valid(Object *p_owner) const {
	if (message.is_empty()) {
		queue_error_print("Configuration Info may not have an empty message.");
		return false;
	}

	if (p_owner != nullptr && !property_name.is_empty()) {
		bool has_property = false;
		p_owner->get(property_name, &has_property);
		if (!has_property) {
			queue_error_print(vformat("Configuration Info on %s refers to property \"%s\" that does not exist.", p_owner->get_class_name(), property_name));
			return false;
		}
	}

	return true;
}

ConfigurationInfo::ConfigurationInfo() :
		ConfigurationInfo(String()) {
}

ConfigurationInfo::ConfigurationInfo(const Dictionary &p_dict) {
	message = p_dict.get("message", "");
	property_name = p_dict.get("property", StringName());
	severity = string_to_severity(p_dict.get("severity", "warning"));
}

ConfigurationInfo::ConfigurationInfo(const String &p_message, const StringName &p_property_name, Severity p_severity) {
	message = p_message;
	property_name = p_property_name;
	severity = p_severity;
}

#endif // TOOLS_ENABLED
