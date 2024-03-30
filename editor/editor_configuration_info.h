/**************************************************************************/
/*  editor_configuration_info.h                                           */
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

#ifndef EDITOR_CONFIGURATION_INFO_H
#define EDITOR_CONFIGURATION_INFO_H

#include "core/object/configuration_info.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class EditorConfigurationInfo {
public:
	EditorConfigurationInfo() {}

	static Vector<ConfigurationInfo> get_configuration_info(Object *p_object);
	static ConfigurationInfo::Severity get_max_severity(const Vector<ConfigurationInfo> &p_config_infos);
	static StringName get_severity_icon(ConfigurationInfo::Severity p_severity);

	static Vector<ConfigurationInfo> filter_list_for_property(const Vector<ConfigurationInfo> &p_config_infos, const StringName &p_property_name);
	static String format_as_string(const ConfigurationInfo &p_config_info, bool p_wrap_lines, bool p_prefix_property_name);
	static String format_list_as_string(const Vector<ConfigurationInfo> &p_config_infos, bool p_wrap_lines, bool p_prefix_property_name);
};

#endif // EDITOR_CONFIGURATION_INFO_H
