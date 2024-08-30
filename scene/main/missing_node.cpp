/**************************************************************************/
/*  missing_node.cpp                                                      */
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

#include "missing_node.h"

bool MissingNode::_set(const StringName &p_name, const Variant &p_value) {
	if (is_recording_properties()) {
		properties.insert(p_name, p_value);
		return true; //always valid to set (add)
	} else {
		if (!properties.has(p_name)) {
			return false;
		}

		properties[p_name] = p_value;
		return true;
	}
}

bool MissingNode::_get(const StringName &p_name, Variant &r_ret) const {
	if (!properties.has(p_name)) {
		return false;
	}
	r_ret = properties[p_name];
	return true;
}

void MissingNode::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const KeyValue<StringName, Variant> &E : properties) {
		p_list->push_back(PropertyInfo(E.value.get_type(), E.key));
	}
}

void MissingNode::set_original_class(const String &p_class) {
	original_class = p_class;
}

String MissingNode::get_original_class() const {
	return original_class;
}

void MissingNode::set_original_scene(const String &p_scene) {
	original_scene = p_scene;
}

String MissingNode::get_original_scene() const {
	return original_scene;
}

void MissingNode::set_recording_properties(bool p_enable) {
	recording_properties = p_enable;
}

bool MissingNode::is_recording_properties() const {
	return recording_properties;
}

#ifdef TOOLS_ENABLED
Vector<ConfigurationInfo> MissingNode::get_configuration_info() const {
	Vector<ConfigurationInfo> infos = Node::get_configuration_info();
	if (!original_scene.is_empty()) {
		CONFIG_WARNING(vformat(RTR("This node was an instance of scene '%s', which was no longer available when this scene was loaded."), original_scene));
		CONFIG_WARNING(vformat(RTR("Saving current scene will discard instance and all its properties, including editable children edits (if existing).")));
	} else if (!original_class.is_empty()) {
		CONFIG_WARNING(vformat(RTR("This node was saved as class type '%s', which was no longer available when this scene was loaded."), original_class));
		CONFIG_WARNING(RTR("Data from the original node is kept as a placeholder until this type of node is available again. It can hence be safely re-saved without risk of data loss."));
	} else {
		CONFIG_WARNING(RTR("Unrecognized missing node. Check scene dependency errors for details."));
	}
	return infos;
}
#endif

void MissingNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_original_class", "name"), &MissingNode::set_original_class);
	ClassDB::bind_method(D_METHOD("get_original_class"), &MissingNode::get_original_class);

	ClassDB::bind_method(D_METHOD("set_original_scene", "name"), &MissingNode::set_original_class);
	ClassDB::bind_method(D_METHOD("get_original_scene"), &MissingNode::get_original_class);

	ClassDB::bind_method(D_METHOD("set_recording_properties", "enable"), &MissingNode::set_recording_properties);
	ClassDB::bind_method(D_METHOD("is_recording_properties"), &MissingNode::is_recording_properties);

	// Expose, but not save.
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "original_class", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_original_class", "get_original_class");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "original_scene", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_original_scene", "get_original_scene");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "recording_properties", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_recording_properties", "is_recording_properties");
}

MissingNode::MissingNode() {
}
