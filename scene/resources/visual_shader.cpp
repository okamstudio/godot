/**************************************************************************/
/*  visual_shader.cpp                                                     */
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

#include "visual_shader.h"

#include "core/templates/rb_map.h"
#include "core/templates/vmap.h"
#include "core/variant/variant_utility.h"
#include "servers/rendering/shader_types.h"
#include "visual_shader_nodes.h"
#include "visual_shader_particle_nodes.h"

String make_unique_id(VisualShader::Type p_type, int p_id, const String &p_name) {
	static const char *typepf[VisualShader::TYPE_MAX] = { "vtx", "frg", "lgt", "start", "process", "collide", "start_custom", "process_custom", "sky", "fog" };
	return p_name + "_" + String(typepf[p_type]) + "_" + itos(p_id);
}

bool VisualShaderNode::is_simple_decl() const {
	return simple_decl;
}

int VisualShaderNode::get_default_input_port(PortType p_type) const {
	return 0;
}

void VisualShaderNode::set_output_port_for_preview(int p_index) {
	port_preview = p_index;
}

int VisualShaderNode::get_output_port_for_preview() const {
	return port_preview;
}

void VisualShaderNode::set_input_port_default_value(int p_port, const Variant &p_value, const Variant &p_prev_value) {
	Variant value = p_value;

	if (p_prev_value.get_type() != Variant::NIL) {
		switch (p_value.get_type()) {
			case Variant::FLOAT: {
				switch (p_prev_value.get_type()) {
					case Variant::INT: {
						value = (float)p_prev_value;
					} break;
					case Variant::FLOAT: {
						value = p_prev_value;
					} break;
					case Variant::VECTOR2: {
						Vector2 pv = p_prev_value;
						value = pv.x;
					} break;
					case Variant::VECTOR3: {
						Vector3 pv = p_prev_value;
						value = pv.x;
					} break;
					case Variant::QUATERNION: {
						Quaternion pv = p_prev_value;
						value = pv.x;
					} break;
					default:
						break;
				}
			} break;
			case Variant::INT: {
				switch (p_prev_value.get_type()) {
					case Variant::INT: {
						value = p_prev_value;
					} break;
					case Variant::FLOAT: {
						value = (int)p_prev_value;
					} break;
					case Variant::VECTOR2: {
						Vector2 pv = p_prev_value;
						value = (int)pv.x;
					} break;
					case Variant::VECTOR3: {
						Vector3 pv = p_prev_value;
						value = (int)pv.x;
					} break;
					case Variant::QUATERNION: {
						Quaternion pv = p_prev_value;
						value = (int)pv.x;
					} break;
					default:
						break;
				}
			} break;
			case Variant::VECTOR2: {
				switch (p_prev_value.get_type()) {
					case Variant::INT: {
						float pv = (float)(int)p_prev_value;
						value = Vector2(pv, pv);
					} break;
					case Variant::FLOAT: {
						float pv = p_prev_value;
						value = Vector2(pv, pv);
					} break;
					case Variant::VECTOR2: {
						value = p_prev_value;
					} break;
					case Variant::VECTOR3: {
						Vector3 pv = p_prev_value;
						value = Vector2(pv.x, pv.y);
					} break;
					case Variant::QUATERNION: {
						Quaternion pv = p_prev_value;
						value = Vector2(pv.x, pv.y);
					} break;
					default:
						break;
				}
			} break;
			case Variant::VECTOR3: {
				switch (p_prev_value.get_type()) {
					case Variant::INT: {
						float pv = (float)(int)p_prev_value;
						value = Vector3(pv, pv, pv);
					} break;
					case Variant::FLOAT: {
						float pv = p_prev_value;
						value = Vector3(pv, pv, pv);
					} break;
					case Variant::VECTOR2: {
						Vector2 pv = p_prev_value;
						value = Vector3(pv.x, pv.y, pv.y);
					} break;
					case Variant::VECTOR3: {
						value = p_prev_value;
					} break;
					case Variant::QUATERNION: {
						Quaternion pv = p_prev_value;
						value = Vector3(pv.x, pv.y, pv.z);
					} break;
					default:
						break;
				}
			} break;
			case Variant::QUATERNION: {
				switch (p_prev_value.get_type()) {
					case Variant::INT: {
						float pv = (float)(int)p_prev_value;
						value = Quaternion(pv, pv, pv, pv);
					} break;
					case Variant::FLOAT: {
						float pv = p_prev_value;
						value = Quaternion(pv, pv, pv, pv);
					} break;
					case Variant::VECTOR2: {
						Vector2 pv = p_prev_value;
						value = Quaternion(pv.x, pv.y, pv.y, pv.y);
					} break;
					case Variant::VECTOR3: {
						Vector3 pv = p_prev_value;
						value = Quaternion(pv.x, pv.y, pv.z, pv.z);
					} break;
					case Variant::QUATERNION: {
						value = p_prev_value;
					} break;
					default:
						break;
				}
			} break;
			default:
				break;
		}
	}
	default_input_values[p_port] = value;
	emit_changed();
}

Variant VisualShaderNode::get_input_port_default_value(int p_port) const {
	if (default_input_values.has(p_port)) {
		return default_input_values[p_port];
	}

	return Variant();
}

void VisualShaderNode::remove_input_port_default_value(int p_port) {
	if (default_input_values.has(p_port)) {
		default_input_values.erase(p_port);
		emit_changed();
	}
}

void VisualShaderNode::clear_default_input_values() {
	if (!default_input_values.is_empty()) {
		default_input_values.clear();
		emit_changed();
	}
}

bool VisualShaderNode::is_port_separator(int p_index) const {
	return false;
}

bool VisualShaderNode::is_output_port_connected(int p_port) const {
	if (connected_output_ports.has(p_port)) {
		return connected_output_ports[p_port] > 0;
	}
	return false;
}

void VisualShaderNode::set_output_port_connected(int p_port, bool p_connected) {
	if (p_connected) {
		connected_output_ports[p_port]++;
	} else {
		connected_output_ports[p_port]--;
	}
}

bool VisualShaderNode::is_input_port_connected(int p_port) const {
	if (connected_input_ports.has(p_port)) {
		return connected_input_ports[p_port];
	}
	return false;
}

void VisualShaderNode::set_input_port_connected(int p_port, bool p_connected) {
	connected_input_ports[p_port] = p_connected;
}

bool VisualShaderNode::is_any_port_connected() const {
	for (const KeyValue<int, bool> &E : connected_input_ports) {
		if (E.value) {
			return true;
		}
	}
	for (const KeyValue<int, int> &E : connected_output_ports) {
		if (E.value > 0) {
			return true;
		}
	}
	return false;
}

bool VisualShaderNode::is_generate_input_var(int p_port) const {
	return true;
}

bool VisualShaderNode::is_output_port_expandable(int p_port) const {
	VisualShaderNode::PortType port = get_output_port_type(p_port);
	if (get_output_port_count() == 1 && (port == PORT_TYPE_VECTOR_2D || port == PORT_TYPE_VECTOR_3D || port == PORT_TYPE_VECTOR_4D)) {
		return true;
	}
	return false;
}

bool VisualShaderNode::has_output_port_preview(int p_port) const {
	return true;
}

void VisualShaderNode::_set_output_ports_expanded(const Array &p_values) {
	for (int i = 0; i < p_values.size(); i++) {
		expanded_output_ports[p_values[i]] = true;
	}
	emit_changed();
}

Array VisualShaderNode::_get_output_ports_expanded() const {
	Array arr;
	for (int i = 0; i < get_output_port_count(); i++) {
		if (_is_output_port_expanded(i)) {
			arr.push_back(i);
		}
	}
	return arr;
}

void VisualShaderNode::_set_output_port_expanded(int p_port, bool p_expanded) {
	expanded_output_ports[p_port] = p_expanded;
	emit_changed();
}

bool VisualShaderNode::_is_output_port_expanded(int p_port) const {
	if (expanded_output_ports.has(p_port)) {
		return expanded_output_ports[p_port];
	}
	return false;
}

int VisualShaderNode::get_expanded_output_port_count() const {
	int count = get_output_port_count();
	int count2 = count;
	for (int i = 0; i < count; i++) {
		if (is_output_port_expandable(i) && _is_output_port_expanded(i)) {
			switch (get_output_port_type(i)) {
				case PORT_TYPE_VECTOR_2D: {
					count2 += 2;
				} break;
				case PORT_TYPE_VECTOR_3D: {
					count2 += 3;
				} break;
				case PORT_TYPE_VECTOR_4D: {
					count2 += 4;
				} break;
				default:
					break;
			}
		}
	}
	return count2;
}

bool VisualShaderNode::is_code_generated() const {
	return true;
}

bool VisualShaderNode::is_show_prop_names() const {
	return false;
}

bool VisualShaderNode::is_use_prop_slots() const {
	return false;
}

bool VisualShaderNode::is_disabled() const {
	return disabled;
}

void VisualShaderNode::set_disabled(bool p_disabled) {
	disabled = p_disabled;
}

bool VisualShaderNode::is_deletable() const {
	return closable;
}

void VisualShaderNode::set_deletable(bool p_closable) {
	closable = p_closable;
}

void VisualShaderNode::set_frame(int p_node) {
	linked_parent_graph_frame = p_node;
}

int VisualShaderNode::get_frame() const {
	return linked_parent_graph_frame;
}

Vector<VisualShader::DefaultTextureParam> VisualShaderNode::get_default_texture_parameters(VisualShader::Type p_type, int p_id) const {
	return Vector<VisualShader::DefaultTextureParam>();
}

String VisualShaderNode::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	return String();
}

String VisualShaderNode::generate_global_per_node(Shader::Mode p_mode, int p_id) const {
	return String();
}

String VisualShaderNode::generate_global_per_func(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	return String();
}

Vector<StringName> VisualShaderNode::get_editable_properties() const {
	return Vector<StringName>();
}

HashMap<StringName, String> VisualShaderNode::get_editable_properties_names() const {
	return HashMap<StringName, String>();
}

Array VisualShaderNode::get_default_input_values() const {
	Array ret;
	for (const KeyValue<int, Variant> &E : default_input_values) {
		ret.push_back(E.key);
		ret.push_back(E.value);
	}
	return ret;
}

void VisualShaderNode::set_default_input_values(const Array &p_values) {
	if (p_values.size() % 2 == 0) {
		for (int i = 0; i < p_values.size(); i += 2) {
			default_input_values[p_values[i + 0]] = p_values[i + 1];
		}
	}

	emit_changed();
}

String VisualShaderNode::get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const {
	return String();
}

VisualShaderNode::Category VisualShaderNode::get_category() const {
	return CATEGORY_NONE;
}

bool VisualShaderNode::is_input_port_default(int p_port, Shader::Mode p_mode) const {
	return false;
}

void VisualShaderNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_default_input_port", "type"), &VisualShaderNode::get_default_input_port);

	ClassDB::bind_method(D_METHOD("set_output_port_for_preview", "port"), &VisualShaderNode::set_output_port_for_preview);
	ClassDB::bind_method(D_METHOD("get_output_port_for_preview"), &VisualShaderNode::get_output_port_for_preview);

	ClassDB::bind_method(D_METHOD("_set_output_port_expanded", "port"), &VisualShaderNode::_set_output_port_expanded);
	ClassDB::bind_method(D_METHOD("_is_output_port_expanded"), &VisualShaderNode::_is_output_port_expanded);

	ClassDB::bind_method(D_METHOD("_set_output_ports_expanded", "values"), &VisualShaderNode::_set_output_ports_expanded);
	ClassDB::bind_method(D_METHOD("_get_output_ports_expanded"), &VisualShaderNode::_get_output_ports_expanded);

	ClassDB::bind_method(D_METHOD("set_input_port_default_value", "port", "value", "prev_value"), &VisualShaderNode::set_input_port_default_value, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_input_port_default_value", "port"), &VisualShaderNode::get_input_port_default_value);

	ClassDB::bind_method(D_METHOD("remove_input_port_default_value", "port"), &VisualShaderNode::remove_input_port_default_value);
	ClassDB::bind_method(D_METHOD("clear_default_input_values"), &VisualShaderNode::clear_default_input_values);

	ClassDB::bind_method(D_METHOD("set_default_input_values", "values"), &VisualShaderNode::set_default_input_values);
	ClassDB::bind_method(D_METHOD("get_default_input_values"), &VisualShaderNode::get_default_input_values);

	ClassDB::bind_method(D_METHOD("set_frame", "frame"), &VisualShaderNode::set_frame);
	ClassDB::bind_method(D_METHOD("get_frame"), &VisualShaderNode::get_frame);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "output_port_for_preview"), "set_output_port_for_preview", "get_output_port_for_preview");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "default_input_values", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_default_input_values", "get_default_input_values");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "expanded_output_ports", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_output_ports_expanded", "_get_output_ports_expanded");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "linked_parent_graph_frame", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_frame", "get_frame");

	BIND_ENUM_CONSTANT(PORT_TYPE_SCALAR);
	BIND_ENUM_CONSTANT(PORT_TYPE_SCALAR_INT);
	BIND_ENUM_CONSTANT(PORT_TYPE_SCALAR_UINT);
	BIND_ENUM_CONSTANT(PORT_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(PORT_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(PORT_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(PORT_TYPE_BOOLEAN);
	BIND_ENUM_CONSTANT(PORT_TYPE_TRANSFORM);
	BIND_ENUM_CONSTANT(PORT_TYPE_SAMPLER);
	BIND_ENUM_CONSTANT(PORT_TYPE_MAX);
}

VisualShaderNode::VisualShaderNode() {
}

/////////////////////////////////////////////////////////

void VisualShaderNodeCustom::update_property_default_values() {
	int prop_count;
	if (GDVIRTUAL_CALL(_get_property_count, prop_count)) {
		for (int i = 0; i < prop_count; i++) {
			int selected = 0;
			if (GDVIRTUAL_CALL(_get_property_default_index, i, selected)) {
				dp_selected_cache[i] = selected;
			}
		}
	}
}

void VisualShaderNodeCustom::update_input_port_default_values() {
	int input_port_count;
	if (GDVIRTUAL_CALL(_get_input_port_count, input_port_count)) {
		for (int i = 0; i < input_port_count; i++) {
			Variant value;
			if (GDVIRTUAL_CALL(_get_input_port_default_value, i, value)) {
				default_input_values[i] = value;
			}
		}
	}
}

void VisualShaderNodeCustom::update_ports() {
	{
		dp_props.clear();
		int prop_count;
		if (GDVIRTUAL_CALL(_get_property_count, prop_count)) {
			for (int i = 0; i < prop_count; i++) {
				DropDownListProperty prop;
				if (!GDVIRTUAL_CALL(_get_property_name, i, prop.name)) {
					prop.name = "prop";
				}
				if (!GDVIRTUAL_CALL(_get_property_options, i, prop.options)) {
					prop.options.push_back("Default");
				}
				dp_props.push_back(prop);
			}
		}
	}

	{
		Vector<String> vprops = properties.split(";", false);
		for (int i = 0; i < vprops.size(); i++) {
			Vector<String> arr = vprops[i].split(",", false);
			ERR_FAIL_COND(arr.size() != 2);
			ERR_FAIL_COND(!arr[0].is_valid_int());
			ERR_FAIL_COND(!arr[1].is_valid_int());
			int index = arr[0].to_int();
			int selected = arr[1].to_int();
			dp_selected_cache[index] = selected;
		}
	}

	{
		input_ports.clear();
		int input_port_count;
		if (GDVIRTUAL_CALL(_get_input_port_count, input_port_count)) {
			for (int i = 0; i < input_port_count; i++) {
				Port port;
				if (!GDVIRTUAL_CALL(_get_input_port_name, i, port.name)) {
					port.name = "in" + itos(i);
				}
				PortType port_type;
				if (GDVIRTUAL_CALL(_get_input_port_type, i, port_type)) {
					port.type = (int)port_type;
				} else {
					port.type = (int)PortType::PORT_TYPE_SCALAR;
				}

				input_ports.push_back(port);
			}
		}
	}

	{
		output_ports.clear();
		int output_port_count;
		if (GDVIRTUAL_CALL(_get_output_port_count, output_port_count)) {
			for (int i = 0; i < output_port_count; i++) {
				Port port;
				if (!GDVIRTUAL_CALL(_get_output_port_name, i, port.name)) {
					port.name = "out" + itos(i);
				}
				PortType port_type;
				if (GDVIRTUAL_CALL(_get_output_port_type, i, port_type)) {
					port.type = (int)port_type;
				} else {
					port.type = (int)PortType::PORT_TYPE_SCALAR;
				}

				output_ports.push_back(port);
			}
		}
	}
}

void VisualShaderNodeCustom::update_properties() {
	properties = "";
	for (const KeyValue<int, int> &p : dp_selected_cache) {
		if (p.value != 0) {
			properties += itos(p.key) + "," + itos(p.value) + ";";
		}
	}
}

String VisualShaderNodeCustom::get_caption() const {
	String ret = "Unnamed";
	GDVIRTUAL_CALL(_get_name, ret);
	return ret;
}

int VisualShaderNodeCustom::get_input_port_count() const {
	return input_ports.size();
}

VisualShaderNodeCustom::PortType VisualShaderNodeCustom::get_input_port_type(int p_port) const {
	ERR_FAIL_INDEX_V(p_port, input_ports.size(), PORT_TYPE_SCALAR);
	return (PortType)input_ports.get(p_port).type;
}

String VisualShaderNodeCustom::get_input_port_name(int p_port) const {
	ERR_FAIL_INDEX_V(p_port, input_ports.size(), "");
	return input_ports.get(p_port).name;
}

int VisualShaderNodeCustom::get_default_input_port(PortType p_type) const {
	int ret = 0;
	GDVIRTUAL_CALL(_get_default_input_port, p_type, ret);
	return ret;
}

int VisualShaderNodeCustom::get_output_port_count() const {
	return output_ports.size();
}

VisualShaderNodeCustom::PortType VisualShaderNodeCustom::get_output_port_type(int p_port) const {
	ERR_FAIL_INDEX_V(p_port, output_ports.size(), PORT_TYPE_SCALAR);
	return (PortType)output_ports.get(p_port).type;
}

String VisualShaderNodeCustom::get_output_port_name(int p_port) const {
	ERR_FAIL_INDEX_V(p_port, output_ports.size(), "");
	return output_ports.get(p_port).name;
}

String VisualShaderNodeCustom::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	ERR_FAIL_COND_V(!GDVIRTUAL_IS_OVERRIDDEN(_get_code), "");
	TypedArray<String> input_vars;
	for (int i = 0; i < get_input_port_count(); i++) {
		input_vars.push_back(p_input_vars[i]);
	}
	TypedArray<String> output_vars;
	for (int i = 0; i < get_output_port_count(); i++) {
		output_vars.push_back(p_output_vars[i]);
	}

	String _code;
	GDVIRTUAL_CALL(_get_code, input_vars, output_vars, p_mode, p_type, _code);
	if (_is_valid_code(_code)) {
		String code = "	{\n";
		bool nend = _code.ends_with("\n");
		_code = _code.insert(0, "		");
		_code = _code.replace("\n", "\n		");
		code += _code;
		if (!nend) {
			code += "\n	}";
		} else {
			code.remove_at(code.size() - 1);
			code += "}";
		}
		code += "\n";
		return code;
	}
	return String();
}

String VisualShaderNodeCustom::generate_global_per_node(Shader::Mode p_mode, int p_id) const {
	String _code;
	if (GDVIRTUAL_CALL(_get_global_code, p_mode, _code)) {
		if (_is_valid_code(_code)) {
			String code = "// " + get_caption() + "\n";
			code += _code;
			code += "\n";
			return code;
		}
	}
	return String();
}

String VisualShaderNodeCustom::generate_global_per_func(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	String _code;
	if (GDVIRTUAL_CALL(_get_func_code, p_mode, p_type, _code)) {
		if (_is_valid_code(_code)) {
			bool nend = _code.ends_with("\n");
			String code = "// " + get_caption() + "\n";
			code += "	{\n";
			_code = _code.insert(0, "	");
			_code = _code.replace("\n", "\n		");
			code += _code;
			if (!nend) {
				code += "\n	}";
			} else {
				code.remove_at(code.size() - 1);
				code += "}";
			}
			code += "\n";
			return code;
		}
	}
	return String();
}

bool VisualShaderNodeCustom::is_available(Shader::Mode p_mode, VisualShader::Type p_type) const {
	bool ret = true;
	GDVIRTUAL_CALL(_is_available, p_mode, p_type, ret);
	return ret;
}

void VisualShaderNodeCustom::set_input_port_default_value(int p_port, const Variant &p_value, const Variant &p_prev_value) {
	if (!is_initialized) {
		VisualShaderNode::set_input_port_default_value(p_port, p_value, p_prev_value);
	}
}

void VisualShaderNodeCustom::set_default_input_values(const Array &p_values) {
	if (!is_initialized) {
		VisualShaderNode::set_default_input_values(p_values);
	}
}

void VisualShaderNodeCustom::remove_input_port_default_value(int p_port) {
	if (!is_initialized) {
		VisualShaderNode::remove_input_port_default_value(p_port);
	}
}

void VisualShaderNodeCustom::clear_default_input_values() {
	if (!is_initialized) {
		VisualShaderNode::clear_default_input_values();
	}
}

void VisualShaderNodeCustom::_set_input_port_default_value(int p_port, const Variant &p_value) {
	VisualShaderNode::set_input_port_default_value(p_port, p_value);
}

bool VisualShaderNodeCustom::_is_valid_code(const String &p_code) const {
	if (p_code.is_empty() || p_code == "null") {
		return false;
	}
	return true;
}

bool VisualShaderNodeCustom::_is_initialized() {
	return is_initialized;
}

void VisualShaderNodeCustom::_set_initialized(bool p_enabled) {
	is_initialized = p_enabled;
}

void VisualShaderNodeCustom::_set_properties(const String &p_properties) {
	properties = p_properties;
}

String VisualShaderNodeCustom::_get_properties() const {
	return properties;
}

String VisualShaderNodeCustom::_get_name() const {
	String ret;
	GDVIRTUAL_CALL(_get_name, ret);
	return ret;
}

String VisualShaderNodeCustom::_get_description() const {
	String ret;
	GDVIRTUAL_CALL(_get_description, ret);
	return ret;
}

String VisualShaderNodeCustom::_get_category() const {
	String ret;
	GDVIRTUAL_CALL(_get_category, ret);
	return ret;
}

VisualShaderNodeCustom::PortType VisualShaderNodeCustom::_get_return_icon_type() const {
	PortType ret = PORT_TYPE_SCALAR;
	GDVIRTUAL_CALL(_get_return_icon_type, ret);
	return ret;
}

bool VisualShaderNodeCustom::_is_highend() const {
	bool ret = false;
	GDVIRTUAL_CALL(_is_highend, ret);
	return ret;
}

void VisualShaderNodeCustom::_set_option_index(int p_option, int p_value) {
	dp_selected_cache[p_option] = p_value;
	update_properties();
	update_ports();
	update_input_port_default_values();
	emit_changed();
}

int VisualShaderNodeCustom::get_option_index(int p_option) const {
	if (!dp_selected_cache.has(p_option)) {
		return 0;
	}
	return dp_selected_cache[p_option];
}

void VisualShaderNodeCustom::_bind_methods() {
	GDVIRTUAL_BIND(_get_name);
	GDVIRTUAL_BIND(_get_description);
	GDVIRTUAL_BIND(_get_category);
	GDVIRTUAL_BIND(_get_return_icon_type);
	GDVIRTUAL_BIND(_get_input_port_count);
	GDVIRTUAL_BIND(_get_input_port_type, "port");
	GDVIRTUAL_BIND(_get_input_port_name, "port");
	GDVIRTUAL_BIND(_get_input_port_default_value, "port");
	GDVIRTUAL_BIND(_get_default_input_port, "type");
	GDVIRTUAL_BIND(_get_output_port_count);
	GDVIRTUAL_BIND(_get_output_port_type, "port");
	GDVIRTUAL_BIND(_get_output_port_name, "port");
	GDVIRTUAL_BIND(_get_property_count);
	GDVIRTUAL_BIND(_get_property_name, "index");
	GDVIRTUAL_BIND(_get_property_default_index, "index");
	GDVIRTUAL_BIND(_get_property_options, "index");
	GDVIRTUAL_BIND(_get_code, "input_vars", "output_vars", "mode", "type");
	GDVIRTUAL_BIND(_get_func_code, "mode", "type");
	GDVIRTUAL_BIND(_get_global_code, "mode");
	GDVIRTUAL_BIND(_is_highend);
	GDVIRTUAL_BIND(_is_available, "mode", "type");

	ClassDB::bind_method(D_METHOD("_set_initialized", "enabled"), &VisualShaderNodeCustom::_set_initialized);
	ClassDB::bind_method(D_METHOD("_is_initialized"), &VisualShaderNodeCustom::_is_initialized);
	ClassDB::bind_method(D_METHOD("_set_input_port_default_value", "port", "value"), &VisualShaderNodeCustom::_set_input_port_default_value);
	ClassDB::bind_method(D_METHOD("_set_option_index", "option", "value"), &VisualShaderNodeCustom::_set_option_index);
	ClassDB::bind_method(D_METHOD("_set_properties", "properties"), &VisualShaderNodeCustom::_set_properties);
	ClassDB::bind_method(D_METHOD("_get_properties"), &VisualShaderNodeCustom::_get_properties);

	ClassDB::bind_method(D_METHOD("get_option_index", "option"), &VisualShaderNodeCustom::get_option_index);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "initialized", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_initialized", "_is_initialized");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "properties", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_properties", "_get_properties");
}

VisualShaderNodeCustom::VisualShaderNodeCustom() {
	simple_decl = false;
}

/////////////////////////////////////////////////////////

void VisualShader::set_shader_type(Type p_type) {
	current_type = p_type;
}

VisualShader::Type VisualShader::get_shader_type() const {
	return current_type;
}

void VisualShader::add_varying(const String &p_name, VaryingMode p_mode, VaryingType p_type) {
	ERR_FAIL_COND(!p_name.is_valid_ascii_identifier());
	ERR_FAIL_INDEX((int)p_mode, (int)VARYING_MODE_MAX);
	ERR_FAIL_INDEX((int)p_type, (int)VARYING_TYPE_MAX);
	ERR_FAIL_COND(varyings.has(p_name));
	Varying var = Varying(p_name, p_mode, p_type);
	varyings[p_name] = var;
	varyings_list.push_back(var);
	_queue_update();
}

void VisualShader::remove_varying(const String &p_name) {
	ERR_FAIL_COND(!varyings.has(p_name));
	varyings.erase(p_name);
	for (List<Varying>::Element *E = varyings_list.front(); E; E = E->next()) {
		if (E->get().name == p_name) {
			varyings_list.erase(E);
			break;
		}
	}
	_queue_update();
}

bool VisualShader::has_varying(const String &p_name) const {
	return varyings.has(p_name);
}

int VisualShader::get_varyings_count() const {
	return varyings_list.size();
}

const VisualShader::Varying *VisualShader::get_varying_by_index(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, varyings_list.size(), nullptr);
	return &varyings_list.get(p_idx);
}

void VisualShader::set_varying_mode(const String &p_name, VaryingMode p_mode) {
	ERR_FAIL_INDEX((int)p_mode, (int)VARYING_MODE_MAX);
	ERR_FAIL_COND(!varyings.has(p_name));
	if (varyings[p_name].mode == p_mode) {
		return;
	}
	varyings[p_name].mode = p_mode;
	_queue_update();
}

VisualShader::VaryingMode VisualShader::get_varying_mode(const String &p_name) {
	ERR_FAIL_COND_V(!varyings.has(p_name), VARYING_MODE_MAX);
	return varyings[p_name].mode;
}

void VisualShader::set_varying_type(const String &p_name, VaryingType p_type) {
	ERR_FAIL_INDEX((int)p_type, (int)VARYING_TYPE_MAX);
	ERR_FAIL_COND(!varyings.has(p_name));
	if (varyings[p_name].type == p_type) {
		return;
	}
	varyings[p_name].type = p_type;
	_queue_update();
}

VisualShader::VaryingType VisualShader::get_varying_type(const String &p_name) {
	ERR_FAIL_COND_V(!varyings.has(p_name), VARYING_TYPE_MAX);
	return varyings[p_name].type;
}

void VisualShader::_set_preview_shader_parameter(const String &p_name, const Variant &p_value) {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		if (p_value.get_type() == Variant::NIL) {
			if (!preview_params.erase(p_name)) {
				return;
			}
		} else {
			Variant *var = preview_params.getptr(p_name);
			if (var != nullptr && *var == p_value) {
				return;
			}
			preview_params.insert(p_name, p_value);
		}
		emit_changed();
	}
#endif // TOOLS_ENABLED
}

Variant VisualShader::_get_preview_shader_parameter(const String &p_name) const {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		ERR_FAIL_COND_V(!preview_params.has(p_name), Variant());
		return preview_params.get(p_name);
	}
#endif // TOOLS_ENABLED
	return Variant();
}

bool VisualShader::_has_preview_shader_parameter(const String &p_name) const {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		return preview_params.has(p_name);
	}
#endif // TOOLS_ENABLED
	return false;
}

void VisualShader::add_node(Type p_type, const Ref<VisualShaderNode> &p_node, const Vector2 &p_position, int p_id) {
	ERR_FAIL_COND(p_node.is_null());
	ERR_FAIL_COND(p_id < 2);
	ERR_FAIL_INDEX(p_type, TYPE_MAX);
	Graph *g = &graph[p_type];
	ERR_FAIL_COND(g->nodes.has(p_id));
	Node n;
	n.node = p_node;
	n.position = p_position;

	Ref<VisualShaderNodeParameter> parameter = n.node;
	if (parameter.is_valid()) {
		String valid_name = validate_parameter_name(parameter->get_parameter_name(), parameter);
		parameter->set_parameter_name(valid_name);
	}

	Ref<VisualShaderNodeInput> input = n.node;
	if (input.is_valid()) {
		input->shader_mode = shader_mode;
		input->shader_type = p_type;
	}

	n.node->connect_changed(callable_mp(this, &VisualShader::_queue_update));

	Ref<VisualShaderNodeCustom> custom = n.node;
	if (custom.is_valid()) {
		custom->update_ports();
	}

	g->nodes[p_id] = n;

	_queue_update();
}

void VisualShader::set_node_position(Type p_type, int p_id, const Vector2 &p_position) {
	ERR_FAIL_INDEX(p_type, TYPE_MAX);
	Graph *g = &graph[p_type];
	ERR_FAIL_COND(!g->nodes.has(p_id));
	g->nodes[p_id].position = p_position;
}

Vector2 VisualShader::get_node_position(Type p_type, int p_id) const {
	ERR_FAIL_INDEX_V(p_type, TYPE_MAX, Vector2());
	const Graph *g = &graph[p_type];
	ERR_FAIL_COND_V(!g->nodes.has(p_id), Vector2());
	return g->nodes[p_id].position;
}

Ref<VisualShaderNode> VisualShader::get_node(Type p_type, int p_id) const {
	ERR_FAIL_INDEX_V(p_type, TYPE_MAX, Ref<VisualShaderNode>());
	const Graph *g = &graph[p_type];
	if (!g->nodes.has(p_id)) {
		return Ref<VisualShaderNode>();
	}
	ERR_FAIL_COND_V(!g->nodes.has(p_id), Ref<VisualShaderNode>());
	return g->nodes[p_id].node;
}

Vector<int> VisualShader::get_node_list(Type p_type) const {
	ERR_FAIL_INDEX_V(p_type, TYPE_MAX, Vector<int>());
	const Graph *g = &graph[p_type];

	Vector<int> ret;
	for (const KeyValue<int, Node> &E : g->nodes) {
		ret.push_back(E.key);
	}

	return ret;
}

int VisualShader::get_valid_node_id(Type p_type) const {
	ERR_FAIL_INDEX_V(p_type, TYPE_MAX, NODE_ID_INVALID);
	const Graph *g = &graph[p_type];
	return g->nodes.size() ? MAX(2, g->nodes.back()->key() + 1) : 2;
}

int VisualShader::find_node_id(Type p_type, const Ref<VisualShaderNode> &p_node) const {
	for (const KeyValue<int, Node> &E : graph[p_type].nodes) {
		if (E.value.node == p_node) {
			return E.key;
		}
	}

	return NODE_ID_INVALID;
}

void VisualShader::remove_node(Type p_type, int p_id) {
	ERR_FAIL_INDEX(p_type, TYPE_MAX);
	ERR_FAIL_COND(p_id < 2);
	Graph *g = &graph[p_type];
	ERR_FAIL_COND(!g->nodes.has(p_id));

	g->nodes[p_id].node->disconnect_changed(callable_mp(this, &VisualShader::_queue_update));

	g->nodes.erase(p_id);

	for (List<Connection>::Element *E = g->connections.front(); E;) {
		List<Connection>::Element *N = E->next();
		const VisualShader::Connection &connection = E->get();
		if (connection.from_node == p_id || connection.to_node == p_id) {
			if (connection.from_node == p_id) {
				g->nodes[connection.to_node].prev_connected_nodes.erase(p_id);
				g->nodes[connection.to_node].node->set_input_port_connected(connection.to_port, false);
			} else if (connection.to_node == p_id) {
				g->nodes[connection.from_node].next_connected_nodes.erase(p_id);
				g->nodes[connection.from_node].node->set_output_port_connected(connection.from_port, false);
			}
			g->connections.erase(E);
		}
		E = N;
	}

	_queue_update();
}

void VisualShader::replace_node(Type p_type, int p_id, const StringName &p_new_class) {
	ERR_FAIL_INDEX(p_type, TYPE_MAX);
	ERR_FAIL_COND(p_id < 2);
	Graph *g = &graph[p_type];
	ERR_FAIL_COND(!g->nodes.has(p_id));

	if (g->nodes[p_id].node->get_class_name() == p_new_class) {
		return;
	}
	VisualShaderNode *vsn = Object::cast_to<VisualShaderNode>(ClassDB::instantiate(p_new_class));
	VisualShaderNode *prev_vsn = g->nodes[p_id].node.ptr();

	// Update connection data.
	for (int i = 0; i < vsn->get_output_port_count(); i++) {
		if (i < prev_vsn->get_output_port_count()) {
			if (prev_vsn->is_output_port_connected(i)) {
				vsn->set_output_port_connected(i, true);
			}

			if (prev_vsn->is_output_port_expandable(i) && prev_vsn->_is_output_port_expanded(i) && vsn->is_output_port_expandable(i)) {
				vsn->_set_output_port_expanded(i, true);

				int component_count = 0;
				switch (prev_vsn->get_output_port_type(i)) {
					case VisualShaderNode::PORT_TYPE_VECTOR_2D:
						component_count = 2;
						break;
					case VisualShaderNode::PORT_TYPE_VECTOR_3D:
						component_count = 3;
						break;
					case VisualShaderNode::PORT_TYPE_VECTOR_4D:
						component_count = 4;
						break;
					default:
						break;
				}

				for (int j = 0; j < component_count; j++) {
					int sub_port = i + 1 + j;

					if (prev_vsn->is_output_port_connected(sub_port)) {
						vsn->set_output_port_connected(sub_port, true);
					}
				}

				i += component_count;
			}
		} else {
			break;
		}
	}

	vsn->connect_changed(callable_mp(this, &VisualShader::_queue_update));
	g->nodes[p_id].node = Ref<VisualShaderNode>(vsn);

	_queue_update();
}

bool VisualShader::is_node_connection(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) const {
	ERR_FAIL_INDEX_V(p_type, TYPE_MAX, false);
	const Graph *g = &graph[p_type];

	for (const Connection &E : g->connections) {
		if (E.from_node == p_from_node && E.from_port == p_from_port && E.to_node == p_to_node && E.to_port == p_to_port) {
			return true;
		}
	}

	return false;
}

bool VisualShader::is_nodes_connected_relatively(const Graph *p_graph, int p_node, int p_target) const {
	bool result = false;

	const VisualShader::Node &node = p_graph->nodes[p_node];

	for (const int &E : node.prev_connected_nodes) {
		if (E == p_target) {
			return true;
		}

		result = is_nodes_connected_relatively(p_graph, E, p_target);
		if (result) {
			break;
		}
	}
	return result;
}

bool VisualShader::_check_reroute_subgraph(Type p_type, int p_target_port_type, int p_reroute_node, List<int> *r_visited_reroute_nodes) const {
	const Graph *g = &graph[p_type];

	// BFS to check whether connecting to the given subgraph (rooted at p_reroute_node) is valid.
	List<int> queue;
	queue.push_back(p_reroute_node);
	if (r_visited_reroute_nodes != nullptr) {
		r_visited_reroute_nodes->push_back(p_reroute_node);
	}
	while (!queue.is_empty()) {
		int current_node_id = queue.front()->get();
		VisualShader::Node current_node = g->nodes[current_node_id];
		queue.pop_front();
		for (const int &next_node_id : current_node.next_connected_nodes) {
			Ref<VisualShaderNodeReroute> next_vsnode = g->nodes[next_node_id].node;
			if (next_vsnode.is_valid()) {
				queue.push_back(next_node_id);
				if (r_visited_reroute_nodes != nullptr) {
					r_visited_reroute_nodes->push_back(next_node_id);
				}
				continue;
			}
			// Check whether all ports connected with the reroute node are compatible.
			for (const Connection &c : g->connections) {
				VisualShaderNode::PortType to_port_type = g->nodes[next_node_id].node->get_input_port_type(c.to_port);
				if (c.from_node == current_node_id &&
						c.to_node == next_node_id &&
						!is_port_types_compatible(p_target_port_type, to_port_type)) {
					return false;
				}
			}
		}
	}
	return true;
}

bool VisualShader::can_connect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) const {
	ERR_FAIL_INDEX_V(p_type, TYPE_MAX, false);
	const Graph *g = &graph[p_type];

	if (!g->nodes.has(p_from_node)) {
		return false;
	}

	if (p_from_node == p_to_node) {
		return false;
	}

	if (p_from_port < 0 || p_from_port >= g->nodes[p_from_node].node->get_expanded_output_port_count()) {
		return false;
	}

	if (!g->nodes.has(p_to_node)) {
		return false;
	}

	if (p_to_port < 0 || p_to_port >= g->nodes[p_to_node].node->get_input_port_count()) {
		return false;
	}

	VisualShaderNode::PortType from_port_type = g->nodes[p_from_node].node->get_output_port_type(p_from_port);
	VisualShaderNode::PortType to_port_type = g->nodes[p_to_node].node->get_input_port_type(p_to_port);

	Ref<VisualShaderNodeReroute> to_node_reroute = g->nodes[p_to_node].node;
	if (to_node_reroute.is_valid()) {
		if (!_check_reroute_subgraph(p_type, from_port_type, p_to_node)) {
			return false;
		}
	} else if (!is_port_types_compatible(from_port_type, to_port_type)) {
		return false;
	}

	for (const Connection &E : g->connections) {
		if (E.from_node == p_from_node && E.from_port == p_from_port && E.to_node == p_to_node && E.to_port == p_to_port) {
			return false;
		}
	}

	if (is_nodes_connected_relatively(g, p_from_node, p_to_node)) {
		return false;
	}
	return true;
}

bool VisualShader::is_port_types_compatible(int p_a, int p_b) const {
	return MAX(0, p_a - (int)VisualShaderNode::PORT_TYPE_BOOLEAN) == (MAX(0, p_b - (int)VisualShaderNode::PORT_TYPE_BOOLEAN));
}

void VisualShader::attach_node_to_frame(Type p_type, int p_node, int p_frame) {
	ERR_FAIL_INDEX(p_type, TYPE_MAX);
	ERR_FAIL_COND(p_frame < 0);
	Graph *g = &graph[p_type];

	ERR_FAIL_COND(!g->nodes.has(p_node));

	g->nodes[p_node].node->set_frame(p_frame);

	Ref<VisualShaderNodeFrame> vsnode_frame = g->nodes[p_frame].node;
	if (vsnode_frame.is_valid()) {
		vsnode_frame->add_attached_node(p_node);
	}
}

void VisualShader::detach_node_from_frame(Type p_type, int p_node) {
	ERR_FAIL_INDEX(p_type, TYPE_MAX);
	Graph *g = &graph[p_type];

	ERR_FAIL_COND(!g->nodes.has(p_node));

	int parent_frame_id = g->nodes[p_node].node->get_frame();
	Ref<VisualShaderNodeFrame> vsnode_frame = g->nodes[parent_frame_id].node;
	if (vsnode_frame.is_valid()) {
		vsnode_frame->remove_attached_node(p_node);
	}

	g->nodes[p_node].node->set_frame(-1);
}

String VisualShader::get_reroute_parameter_name(Type p_type, int p_reroute_node) const {
	ERR_FAIL_INDEX_V(p_type, TYPE_MAX, "");
	const Graph *g = &graph[p_type];

	ERR_FAIL_COND_V(!g->nodes.has(p_reroute_node), "");

	const VisualShader::Node *node = &g->nodes[p_reroute_node];
	while (node->prev_connected_nodes.size() > 0) {
		int connected_node_id = node->prev_connected_nodes[0];
		node = &g->nodes[connected_node_id];
		Ref<VisualShaderNodeParameter> parameter_node = node->node;
		if (parameter_node.is_valid() && parameter_node->get_output_port_type(0) == VisualShaderNode::PORT_TYPE_SAMPLER) {
			return parameter_node->get_parameter_name();
		}
		Ref<VisualShaderNodeInput> input_node = node->node;
		if (input_node.is_valid() && input_node->get_output_port_type(0) == VisualShaderNode::PORT_TYPE_SAMPLER) {
			return input_node->get_input_real_name();
		}
	}
	return "";
}

void VisualShader::connect_nodes_forced(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) {
	ERR_FAIL_INDEX(p_type, TYPE_MAX);
	Graph *g = &graph[p_type];

	ERR_FAIL_COND(!g->nodes.has(p_from_node));
	ERR_FAIL_INDEX(p_from_port, g->nodes[p_from_node].node->get_expanded_output_port_count());
	ERR_FAIL_COND(!g->nodes.has(p_to_node));
	ERR_FAIL_INDEX(p_to_port, g->nodes[p_to_node].node->get_input_port_count());

	for (const Connection &E : g->connections) {
		if (E.from_node == p_from_node && E.from_port == p_from_port && E.to_node == p_to_node && E.to_port == p_to_port) {
			return;
		}
	}

	Connection c;
	c.from_node = p_from_node;
	c.from_port = p_from_port;
	c.to_node = p_to_node;
	c.to_port = p_to_port;
	g->connections.push_back(c);
	g->nodes[p_from_node].next_connected_nodes.push_back(p_to_node);
	g->nodes[p_to_node].prev_connected_nodes.push_back(p_from_node);
	g->nodes[p_from_node].node->set_output_port_connected(p_from_port, true);
	g->nodes[p_to_node].node->set_input_port_connected(p_to_port, true);

	_queue_update();
}

Error VisualShader::connect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) {
	ERR_FAIL_INDEX_V(p_type, TYPE_MAX, ERR_CANT_CONNECT);
	Graph *g = &graph[p_type];

	ERR_FAIL_COND_V(!g->nodes.has(p_from_node), ERR_INVALID_PARAMETER);
	ERR_FAIL_INDEX_V(p_from_port, g->nodes[p_from_node].node->get_expanded_output_port_count(), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!g->nodes.has(p_to_node), ERR_INVALID_PARAMETER);
	ERR_FAIL_INDEX_V(p_to_port, g->nodes[p_to_node].node->get_input_port_count(), ERR_INVALID_PARAMETER);

	Ref<VisualShaderNodeReroute> from_node_reroute = g->nodes[p_from_node].node;
	Ref<VisualShaderNodeReroute> to_node_reroute = g->nodes[p_to_node].node;

	// Allow connection with incompatible port types only if the reroute node isn't connected to anything.
	VisualShaderNode::PortType from_port_type = g->nodes[p_from_node].node->get_output_port_type(p_from_port);
	VisualShaderNode::PortType to_port_type = g->nodes[p_to_node].node->get_input_port_type(p_to_port);
	bool port_types_are_compatible = is_port_types_compatible(from_port_type, to_port_type);

	if (to_node_reroute.is_valid()) {
		List<int> visited_reroute_nodes;
		port_types_are_compatible = _check_reroute_subgraph(p_type, from_port_type, p_to_node, &visited_reroute_nodes);
		if (port_types_are_compatible) {
			// Set the port type of all reroute nodes.
			for (const int &E : visited_reroute_nodes) {
				Ref<VisualShaderNodeReroute> reroute_node = g->nodes[E].node;
				reroute_node->_set_port_type(from_port_type);
			}
		}
	} else if (from_node_reroute.is_valid() && !from_node_reroute->is_input_port_connected(0)) {
		from_node_reroute->_set_port_type(to_port_type);
		port_types_are_compatible = true;
	}

	ERR_FAIL_COND_V_MSG(!port_types_are_compatible, ERR_INVALID_PARAMETER, "Incompatible port types.");

	for (const Connection &E : g->connections) {
		if (E.from_node == p_from_node && E.from_port == p_from_port && E.to_node == p_to_node && E.to_port == p_to_port) {
			ERR_FAIL_V(ERR_ALREADY_EXISTS);
		}
	}

	Connection c;
	c.from_node = p_from_node;
	c.from_port = p_from_port;
	c.to_node = p_to_node;
	c.to_port = p_to_port;
	g->connections.push_back(c);
	g->nodes[p_from_node].next_connected_nodes.push_back(p_to_node);
	g->nodes[p_to_node].prev_connected_nodes.push_back(p_from_node);
	g->nodes[p_from_node].node->set_output_port_connected(p_from_port, true);
	g->nodes[p_to_node].node->set_input_port_connected(p_to_port, true);

	_queue_update();
	return OK;
}

void VisualShader::disconnect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) {
	ERR_FAIL_INDEX(p_type, TYPE_MAX);
	Graph *g = &graph[p_type];

	for (List<Connection>::Element *E = g->connections.front(); E; E = E->next()) {
		if (E->get().from_node == p_from_node && E->get().from_port == p_from_port && E->get().to_node == p_to_node && E->get().to_port == p_to_port) {
			g->connections.erase(E);
			g->nodes[p_from_node].next_connected_nodes.erase(p_to_node);
			g->nodes[p_to_node].prev_connected_nodes.erase(p_from_node);
			g->nodes[p_from_node].node->set_output_port_connected(p_from_port, false);
			g->nodes[p_to_node].node->set_input_port_connected(p_to_port, false);
			_queue_update();
			return;
		}
	}
}

TypedArray<Dictionary> VisualShader::_get_node_connections(Type p_type) const {
	ERR_FAIL_INDEX_V(p_type, TYPE_MAX, Array());
	const Graph *g = &graph[p_type];

	TypedArray<Dictionary> ret;
	for (const Connection &E : g->connections) {
		Dictionary d;
		d["from_node"] = E.from_node;
		d["from_port"] = E.from_port;
		d["to_node"] = E.to_node;
		d["to_port"] = E.to_port;
		ret.push_back(d);
	}

	return ret;
}

void VisualShader::get_node_connections(Type p_type, List<Connection> *r_connections) const {
	ERR_FAIL_INDEX(p_type, TYPE_MAX);
	const Graph *g = &graph[p_type];

	for (const Connection &E : g->connections) {
		r_connections->push_back(E);
	}
}

void VisualShader::set_mode(Mode p_mode) {
	ERR_FAIL_INDEX_MSG(p_mode, Mode::MODE_MAX, vformat("Invalid shader mode: %d.", p_mode));

	if (shader_mode == p_mode) {
		return;
	}

	//erase input/output connections
	modes.clear();
	flags.clear();
	shader_mode = p_mode;
	for (int i = 0; i < TYPE_MAX; i++) {
		for (KeyValue<int, Node> &E : graph[i].nodes) {
			Ref<VisualShaderNodeInput> input = E.value.node;
			if (input.is_valid()) {
				input->shader_mode = shader_mode;
				//input->input_index = 0;
			}
		}

		Ref<VisualShaderNodeOutput> output = graph[i].nodes[NODE_ID_OUTPUT].node;
		output->shader_mode = shader_mode;

		// clear connections since they are no longer valid
		for (List<Connection>::Element *E = graph[i].connections.front(); E;) {
			bool keep = true;

			List<Connection>::Element *N = E->next();

			int from = E->get().from_node;
			int to = E->get().to_node;

			if (!graph[i].nodes.has(from)) {
				keep = false;
			} else {
				Ref<VisualShaderNode> from_node = graph[i].nodes[from].node;
				if (from_node->is_class("VisualShaderNodeOutput") || from_node->is_class("VisualShaderNodeInput")) {
					keep = false;
				}
			}

			if (!graph[i].nodes.has(to)) {
				keep = false;
			} else {
				Ref<VisualShaderNode> to_node = graph[i].nodes[to].node;
				if (to_node->is_class("VisualShaderNodeOutput") || to_node->is_class("VisualShaderNodeInput")) {
					keep = false;
				}
			}

			if (!keep) {
				graph[i].connections.erase(E);
			}
			E = N;
		}
	}

	_queue_update();
	notify_property_list_changed();
}

void VisualShader::set_graph_offset(const Vector2 &p_offset) {
	graph_offset = p_offset;
}

Vector2 VisualShader::get_graph_offset() const {
	return graph_offset;
}

Shader::Mode VisualShader::get_mode() const {
	return shader_mode;
}

bool VisualShader::is_text_shader() const {
	return false;
}

String VisualShader::generate_preview_shader(Type p_type, int p_node, int p_port, Vector<DefaultTextureParam> &default_tex_params) const {
	Ref<VisualShaderNode> node = get_node(p_type, p_node);
	ERR_FAIL_COND_V(node.is_null(), String());
	ERR_FAIL_COND_V(p_port < 0 || p_port >= node->get_expanded_output_port_count(), String());
	ERR_FAIL_COND_V(node->get_output_port_type(p_port) == VisualShaderNode::PORT_TYPE_TRANSFORM, String());

	StringBuilder global_code;
	StringBuilder global_code_per_node;
	HashMap<Type, StringBuilder> global_code_per_func;
	StringBuilder shader_code;
	HashSet<StringName> classes;

	global_code += String() + "shader_type canvas_item;\n";

	String global_expressions;
	for (int i = 0, index = 0; i < TYPE_MAX; i++) {
		for (const KeyValue<int, Node> &E : graph[i].nodes) {
			Ref<VisualShaderNodeGlobalExpression> global_expression = E.value.node;
			if (global_expression.is_valid()) {
				String expr = "";
				expr += "// " + global_expression->get_caption() + ":" + itos(index++) + "\n";
				expr += global_expression->generate_global(get_mode(), Type(i), -1);
				expr = expr.replace("\n", "\n	");
				expr += "\n";
				global_expressions += expr;
			}
		}
	}

	global_code += "\n";
	global_code += global_expressions;

	//make it faster to go around through shader
	VMap<ConnectionKey, const List<Connection>::Element *> input_connections;
	VMap<ConnectionKey, const List<Connection>::Element *> output_connections;

	for (const List<Connection>::Element *E = graph[p_type].connections.front(); E; E = E->next()) {
		ConnectionKey from_key;
		from_key.node = E->get().from_node;
		from_key.port = E->get().from_port;

		output_connections.insert(from_key, E);

		ConnectionKey to_key;
		to_key.node = E->get().to_node;
		to_key.port = E->get().to_port;

		input_connections.insert(to_key, E);
	}

	shader_code += "\nvoid fragment() {\n";

	HashSet<int> processed;
	Error err = _write_node(p_type, &global_code, &global_code_per_node, &global_code_per_func, shader_code, default_tex_params, input_connections, output_connections, p_node, processed, true, classes);
	ERR_FAIL_COND_V(err != OK, String());

	switch (node->get_output_port_type(p_port)) {
		case VisualShaderNode::PORT_TYPE_SCALAR: {
			shader_code += "	COLOR.rgb = vec3(n_out" + itos(p_node) + "p" + itos(p_port) + ");\n";
		} break;
		case VisualShaderNode::PORT_TYPE_SCALAR_INT: {
			shader_code += "	COLOR.rgb = vec3(float(n_out" + itos(p_node) + "p" + itos(p_port) + "));\n";
		} break;
		case VisualShaderNode::PORT_TYPE_SCALAR_UINT: {
			shader_code += "	COLOR.rgb = vec3(float(n_out" + itos(p_node) + "p" + itos(p_port) + "));\n";
		} break;
		case VisualShaderNode::PORT_TYPE_BOOLEAN: {
			shader_code += "	COLOR.rgb = vec3(n_out" + itos(p_node) + "p" + itos(p_port) + " ? 1.0 : 0.0);\n";
		} break;
		case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
			shader_code += "	COLOR.rgb = vec3(n_out" + itos(p_node) + "p" + itos(p_port) + ", 0.0);\n";
		} break;
		case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
			shader_code += "	COLOR.rgb = n_out" + itos(p_node) + "p" + itos(p_port) + ";\n";
		} break;
		case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
			shader_code += "	COLOR = n_out" + itos(p_node) + "p" + itos(p_port) + ";\n";
		} break;
		default: {
			shader_code += "	COLOR.rgb = vec3(0.0);\n";
		} break;
	}

	shader_code += "}\n";

	//set code secretly
	global_code += "\n\n";
	String final_code = global_code;
	final_code += global_code_per_node;
	final_code += shader_code;
	return final_code;
}

String VisualShader::validate_port_name(const String &p_port_name, VisualShaderNode *p_node, int p_port_id, bool p_output) const {
	String port_name = p_port_name;

	if (port_name.is_empty()) {
		return String();
	}

	while (port_name.length() && !is_ascii_alphabet_char(port_name[0])) {
		port_name = port_name.substr(1, port_name.length() - 1);
	}

	if (!port_name.is_empty()) {
		String valid_name;

		for (int i = 0; i < port_name.length(); i++) {
			if (is_ascii_identifier_char(port_name[i])) {
				valid_name += String::chr(port_name[i]);
			} else if (port_name[i] == ' ') {
				valid_name += "_";
			}
		}

		port_name = valid_name;
	} else {
		return String();
	}

	List<String> input_names;
	List<String> output_names;

	for (int i = 0; i < p_node->get_input_port_count(); i++) {
		if (!p_output && i == p_port_id) {
			continue;
		}
		if (port_name == p_node->get_input_port_name(i)) {
			return String();
		}
	}
	for (int i = 0; i < p_node->get_output_port_count(); i++) {
		if (p_output && i == p_port_id) {
			continue;
		}
		if (port_name == p_node->get_output_port_name(i)) {
			return String();
		}
	}

	return port_name;
}

String VisualShader::validate_parameter_name(const String &p_name, const Ref<VisualShaderNodeParameter> &p_parameter) const {
	String param_name = p_name; //validate name first
	while (param_name.length() && !is_ascii_alphabet_char(param_name[0])) {
		param_name = param_name.substr(1, param_name.length() - 1);
	}
	if (!param_name.is_empty()) {
		String valid_name;

		for (int i = 0; i < param_name.length(); i++) {
			if (is_ascii_identifier_char(param_name[i])) {
				valid_name += String::chr(param_name[i]);
			} else if (param_name[i] == ' ') {
				valid_name += "_";
			}
		}

		param_name = valid_name;
	}

	if (param_name.is_empty()) {
		param_name = p_parameter->get_caption();
	}

	int attempt = 1;

	while (true) {
		bool exists = false;
		for (int i = 0; i < TYPE_MAX; i++) {
			for (const KeyValue<int, Node> &E : graph[i].nodes) {
				Ref<VisualShaderNodeParameter> node = E.value.node;
				if (node == p_parameter) { //do not test on self
					continue;
				}
				if (node.is_valid() && node->get_parameter_name() == param_name) {
					exists = true;
					break;
				}
			}
			if (exists) {
				break;
			}
		}

		if (exists) {
			//remove numbers, put new and try again
			attempt++;
			while (param_name.length() && is_digit(param_name[param_name.length() - 1])) {
				param_name = param_name.substr(0, param_name.length() - 1);
			}
			ERR_FAIL_COND_V(param_name.is_empty(), String());
			param_name += itos(attempt);
		} else {
			break;
		}
	}

	return param_name;
}

static const char *type_string[VisualShader::TYPE_MAX] = {
	"vertex",
	"fragment",
	"light",
	"start",
	"process",
	"collide",
	"start_custom",
	"process_custom",
	"sky",
	"fog",
};

bool VisualShader::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name;
	if (prop_name == "mode") {
		set_mode(Shader::Mode(int(p_value)));
		return true;
	} else if (prop_name.begins_with("flags/")) {
		StringName flag = prop_name.get_slicec('/', 1);
		bool enable = p_value;
		if (enable) {
			flags.insert(flag);
		} else {
			flags.erase(flag);
		}
		_queue_update();
		return true;
	} else if (prop_name.begins_with("modes/")) {
		String mode_name = prop_name.get_slicec('/', 1);
		int value = p_value;
		if (value == 0) {
			modes.erase(mode_name); //means it's default anyway, so don't store it
		} else {
			modes[mode_name] = value;
		}
		_queue_update();
		return true;
	} else if (prop_name.begins_with("varyings/")) {
		String var_name = prop_name.get_slicec('/', 1);
		Varying value = Varying();
		value.name = var_name;
		if (value.from_string(p_value) && !varyings.has(var_name)) {
			varyings[var_name] = value;
			varyings_list.push_back(value);
		}
		_queue_update();
		return true;
	}
#ifdef TOOLS_ENABLED
	else if (prop_name.begins_with("preview_params/") && Engine::get_singleton()->is_editor_hint()) {
		String param_name = prop_name.get_slicec('/', 1);
		Variant value = VariantUtilityFunctions::str_to_var(p_value);
		preview_params[param_name] = value;
		return true;
	}
#endif
	else if (prop_name.begins_with("nodes/")) {
		String typestr = prop_name.get_slicec('/', 1);
		Type type = TYPE_VERTEX;
		for (int i = 0; i < TYPE_MAX; i++) {
			if (typestr == type_string[i]) {
				type = Type(i);
				break;
			}
		}

		String index = prop_name.get_slicec('/', 2);
		if (index == "connections") {
			Vector<int> conns = p_value;
			if (conns.size() % 4 == 0) {
				for (int i = 0; i < conns.size(); i += 4) {
					connect_nodes_forced(type, conns[i + 0], conns[i + 1], conns[i + 2], conns[i + 3]);
				}
			}
			return true;
		}

		int id = index.to_int();
		String what = prop_name.get_slicec('/', 3);

		if (what == "node") {
			add_node(type, p_value, Vector2(), id);
			return true;
		} else if (what == "position") {
			set_node_position(type, id, p_value);
			return true;
		} else if (what == "size") {
			((VisualShaderNodeResizableBase *)get_node(type, id).ptr())->set_size(p_value);
			return true;
		} else if (what == "input_ports") {
			((VisualShaderNodeGroupBase *)get_node(type, id).ptr())->set_inputs(p_value);
			return true;
		} else if (what == "output_ports") {
			((VisualShaderNodeGroupBase *)get_node(type, id).ptr())->set_outputs(p_value);
			return true;
		} else if (what == "expression") {
			((VisualShaderNodeExpression *)get_node(type, id).ptr())->set_expression(p_value);
			return true;
		}
	}
	return false;
}

bool VisualShader::_get(const StringName &p_name, Variant &r_ret) const {
	String prop_name = p_name;
	if (prop_name == "mode") {
		r_ret = get_mode();
		return true;
	} else if (prop_name.begins_with("flags/")) {
		StringName flag = prop_name.get_slicec('/', 1);
		r_ret = flags.has(flag);
		return true;
	} else if (prop_name.begins_with("modes/")) {
		String mode_name = prop_name.get_slicec('/', 1);
		if (modes.has(mode_name)) {
			r_ret = modes[mode_name];
		} else {
			r_ret = 0;
		}
		return true;
	} else if (prop_name.begins_with("varyings/")) {
		String var_name = prop_name.get_slicec('/', 1);
		if (varyings.has(var_name)) {
			r_ret = varyings[var_name].to_string();
		} else {
			r_ret = String();
		}
		return true;
	}
#ifdef TOOLS_ENABLED
	else if (prop_name.begins_with("preview_params/") && Engine::get_singleton()->is_editor_hint()) {
		String param_name = prop_name.get_slicec('/', 1);
		if (preview_params.has(param_name)) {
			r_ret = VariantUtilityFunctions::var_to_str(preview_params[param_name]);
		} else {
			r_ret = String();
		}
		return true;
	}
#endif // TOOLS_ENABLED
	else if (prop_name.begins_with("nodes/")) {
		String typestr = prop_name.get_slicec('/', 1);
		Type type = TYPE_VERTEX;
		for (int i = 0; i < TYPE_MAX; i++) {
			if (typestr == type_string[i]) {
				type = Type(i);
				break;
			}
		}

		String index = prop_name.get_slicec('/', 2);
		if (index == "connections") {
			Vector<int> conns;
			for (const Connection &E : graph[type].connections) {
				conns.push_back(E.from_node);
				conns.push_back(E.from_port);
				conns.push_back(E.to_node);
				conns.push_back(E.to_port);
			}

			r_ret = conns;
			return true;
		}

		int id = index.to_int();
		String what = prop_name.get_slicec('/', 3);

		if (what == "node") {
			r_ret = get_node(type, id);
			return true;
		} else if (what == "position") {
			r_ret = get_node_position(type, id);
			return true;
		} else if (what == "size") {
			r_ret = ((VisualShaderNodeResizableBase *)get_node(type, id).ptr())->get_size();
			return true;
		} else if (what == "input_ports") {
			r_ret = ((VisualShaderNodeGroupBase *)get_node(type, id).ptr())->get_inputs();
			return true;
		} else if (what == "output_ports") {
			r_ret = ((VisualShaderNodeGroupBase *)get_node(type, id).ptr())->get_outputs();
			return true;
		} else if (what == "expression") {
			r_ret = ((VisualShaderNodeExpression *)get_node(type, id).ptr())->get_expression();
			return true;
		}
	}
	return false;
}

void VisualShader::reset_state() {
	// TODO: Everything needs to be cleared here.
	emit_changed();
}

void VisualShader::_get_property_list(List<PropertyInfo> *p_list) const {
	//mode
	p_list->push_back(PropertyInfo(Variant::INT, PNAME("mode"), PROPERTY_HINT_ENUM, "Spatial,CanvasItem,Particles,Sky,Fog"));
	//render modes

	HashMap<String, String> blend_mode_enums;
	HashSet<String> toggles;

	const Vector<ShaderLanguage::ModeInfo> &rmodes = ShaderTypes::get_singleton()->get_modes(RenderingServer::ShaderMode(shader_mode));

	for (int i = 0; i < rmodes.size(); i++) {
		const ShaderLanguage::ModeInfo &info = rmodes[i];

		if (!info.options.is_empty()) {
			const String begin = String(info.name);

			for (int j = 0; j < info.options.size(); j++) {
				const String option = String(info.options[j]).capitalize();

				if (!blend_mode_enums.has(begin)) {
					blend_mode_enums[begin] = option;
				} else {
					blend_mode_enums[begin] += "," + option;
				}
			}
		} else {
			toggles.insert(String(info.name));
		}
	}

	for (const KeyValue<String, String> &E : blend_mode_enums) {
		p_list->push_back(PropertyInfo(Variant::INT, vformat("%s/%s", PNAME("modes"), E.key), PROPERTY_HINT_ENUM, E.value));
	}

	for (const String &E : toggles) {
		p_list->push_back(PropertyInfo(Variant::BOOL, vformat("%s/%s", PNAME("flags"), E)));
	}

	for (const KeyValue<String, Varying> &E : varyings) {
		p_list->push_back(PropertyInfo(Variant::STRING, vformat("%s/%s", "varyings", E.key), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		for (const KeyValue<String, Variant> &E : preview_params) {
			p_list->push_back(PropertyInfo(Variant::STRING, vformat("%s/%s", "preview_params", E.key), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
		}
	}
#endif // TOOLS_ENABLED

	for (int i = 0; i < TYPE_MAX; i++) {
		for (const KeyValue<int, Node> &E : graph[i].nodes) {
			String prop_name = "nodes/";
			prop_name += type_string[i];
			prop_name += "/" + itos(E.key);

			if (E.key != NODE_ID_OUTPUT) {
				p_list->push_back(PropertyInfo(Variant::OBJECT, prop_name + "/node", PROPERTY_HINT_RESOURCE_TYPE, "VisualShaderNode", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_ALWAYS_DUPLICATE));
			}
			p_list->push_back(PropertyInfo(Variant::VECTOR2, prop_name + "/position", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));

			if (Object::cast_to<VisualShaderNodeGroupBase>(E.value.node.ptr()) != nullptr) {
				p_list->push_back(PropertyInfo(Variant::VECTOR2, prop_name + "/size", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
				p_list->push_back(PropertyInfo(Variant::STRING, prop_name + "/input_ports", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
				p_list->push_back(PropertyInfo(Variant::STRING, prop_name + "/output_ports", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
			}
			if (Object::cast_to<VisualShaderNodeExpression>(E.value.node.ptr()) != nullptr) {
				p_list->push_back(PropertyInfo(Variant::STRING, prop_name + "/expression", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
			}
		}
		p_list->push_back(PropertyInfo(Variant::PACKED_INT32_ARRAY, "nodes/" + String(type_string[i]) + "/connections", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	}
}

Error VisualShader::_write_node(Type type, StringBuilder *p_global_code, StringBuilder *p_global_code_per_node, HashMap<Type, StringBuilder> *p_global_code_per_func, StringBuilder &r_code, Vector<VisualShader::DefaultTextureParam> &r_def_tex_params, const VMap<ConnectionKey, const List<Connection>::Element *> &p_input_connections, const VMap<ConnectionKey, const List<Connection>::Element *> &p_output_connections, int p_node, HashSet<int> &r_processed, bool p_for_preview, HashSet<StringName> &r_classes) const {
	const Ref<VisualShaderNode> vsnode = graph[type].nodes[p_node].node;

	if (vsnode->is_disabled()) {
		r_code += "// " + vsnode->get_caption() + ":" + itos(p_node) + "\n";
		r_code += "	// Node is disabled and code is not generated.\n";
		return OK;
	}

	//check inputs recursively first
	int input_count = vsnode->get_input_port_count();
	for (int i = 0; i < input_count; i++) {
		ConnectionKey ck;
		ck.node = p_node;
		ck.port = i;

		if (p_input_connections.has(ck)) {
			int from_node = p_input_connections[ck]->get().from_node;
			if (r_processed.has(from_node)) {
				continue;
			}

			Error err = _write_node(type, p_global_code, p_global_code_per_node, p_global_code_per_func, r_code, r_def_tex_params, p_input_connections, p_output_connections, from_node, r_processed, p_for_preview, r_classes);
			if (err) {
				return err;
			}
		}
	}

	// then this node

	Vector<VisualShader::DefaultTextureParam> params = vsnode->get_default_texture_parameters(type, p_node);
	for (int i = 0; i < params.size(); i++) {
		r_def_tex_params.push_back(params[i]);
	}

	Ref<VisualShaderNodeInput> input = vsnode;
	bool skip_global = input.is_valid() && p_for_preview;

	if (!skip_global) {
		Ref<VisualShaderNodeParameter> parameter = vsnode;
		if (parameter.is_null() || !parameter->is_global_code_generated()) {
			if (p_global_code) {
				*p_global_code += vsnode->generate_global(get_mode(), type, p_node);
			}
		}

		String class_name = vsnode->get_class_name();
		if (class_name == "VisualShaderNodeCustom") {
			class_name = vsnode->get_script_instance()->get_script()->get_path();
		}
		if (!r_classes.has(class_name)) {
			if (p_global_code_per_node) {
				*p_global_code_per_node += vsnode->generate_global_per_node(get_mode(), p_node);
			}
			for (int i = 0; i < TYPE_MAX; i++) {
				if (p_global_code_per_func) {
					(*p_global_code_per_func)[Type(i)] += vsnode->generate_global_per_func(get_mode(), Type(i), p_node);
				}
			}
			r_classes.insert(class_name);
		}
	}

	if (!vsnode->is_code_generated()) { // just generate globals and ignore locals
		r_processed.insert(p_node);
		return OK;
	}

	String node_name = "// " + vsnode->get_caption() + ":" + itos(p_node) + "\n";
	String node_code;
	Vector<String> input_vars;

	input_vars.resize(vsnode->get_input_port_count());
	String *inputs = input_vars.ptrw();

	for (int i = 0; i < input_count; i++) {
		ConnectionKey ck;
		ck.node = p_node;
		ck.port = i;

		if (p_input_connections.has(ck)) {
			//connected to something, use that output
			int from_node = p_input_connections[ck]->get().from_node;

			if (graph[type].nodes[from_node].node->is_disabled()) {
				continue;
			}

			int from_port = p_input_connections[ck]->get().from_port;

			VisualShaderNode::PortType in_type = vsnode->get_input_port_type(i);
			VisualShaderNode::PortType out_type = graph[type].nodes[from_node].node->get_output_port_type(from_port);

			String src_var = "n_out" + itos(from_node) + "p" + itos(from_port);

			if (in_type == VisualShaderNode::PORT_TYPE_SAMPLER && out_type == VisualShaderNode::PORT_TYPE_SAMPLER) {
				Ref<VisualShaderNode> ref = graph[type].nodes[from_node].node;
				// FIXME: This needs to be refactored at some point.
				if (ref->has_method("get_input_real_name")) {
					inputs[i] = ref->call("get_input_real_name");
				} else if (ref->has_method("get_parameter_name")) {
					inputs[i] = ref->call("get_parameter_name");
				} else {
					Ref<VisualShaderNodeReroute> reroute = graph[type].nodes[from_node].node;
					if (reroute.is_valid()) {
						inputs[i] = get_reroute_parameter_name(type, from_node);
					} else {
						inputs[i] = "";
					}
				}
			} else if (in_type == out_type) {
				inputs[i] = src_var;
			} else {
				switch (in_type) {
					case VisualShaderNode::PORT_TYPE_SCALAR: {
						switch (out_type) {
							case VisualShaderNode::PORT_TYPE_SCALAR_INT: {
								inputs[i] = "float(" + src_var + ")";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_UINT: {
								inputs[i] = "float(" + src_var + ")";
							} break;
							case VisualShaderNode::PORT_TYPE_BOOLEAN: {
								inputs[i] = "(" + src_var + " ? 1.0 : 0.0)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
								inputs[i] = src_var + ".x";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
								inputs[i] = src_var + ".x";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
								inputs[i] = src_var + ".x";
							} break;
							default:
								break;
						}
					} break;
					case VisualShaderNode::PORT_TYPE_SCALAR_INT: {
						switch (out_type) {
							case VisualShaderNode::PORT_TYPE_SCALAR: {
								inputs[i] = "int(" + src_var + ")";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_UINT: {
								inputs[i] = "int(" + src_var + ")";
							} break;
							case VisualShaderNode::PORT_TYPE_BOOLEAN: {
								inputs[i] = "(" + src_var + " ? 1 : 0)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
								inputs[i] = "int(" + src_var + ".x)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
								inputs[i] = "int(" + src_var + ".x)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
								inputs[i] = "int(" + src_var + ".x)";
							} break;
							default:
								break;
						}
					} break;
					case VisualShaderNode::PORT_TYPE_SCALAR_UINT: {
						switch (out_type) {
							case VisualShaderNode::PORT_TYPE_SCALAR: {
								inputs[i] = "uint(" + src_var + ")";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_INT: {
								inputs[i] = "uint(" + src_var + ")";
							} break;
							case VisualShaderNode::PORT_TYPE_BOOLEAN: {
								inputs[i] = "(" + src_var + " ? 1u : 0u)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
								inputs[i] = "uint(" + src_var + ".x)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
								inputs[i] = "uint(" + src_var + ".x)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
								inputs[i] = "uint(" + src_var + ".x)";
							} break;
							default:
								break;
						}
					} break;
					case VisualShaderNode::PORT_TYPE_BOOLEAN: {
						switch (out_type) {
							case VisualShaderNode::PORT_TYPE_SCALAR: {
								inputs[i] = src_var + " > 0.0 ? true : false";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_INT: {
								inputs[i] = src_var + " > 0 ? true : false";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_UINT: {
								inputs[i] = src_var + " > 0u ? true : false";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
								inputs[i] = "all(bvec2(" + src_var + "))";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
								inputs[i] = "all(bvec3(" + src_var + "))";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
								inputs[i] = "all(bvec4(" + src_var + "))";
							} break;
							default:
								break;
						}
					} break;
					case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
						switch (out_type) {
							case VisualShaderNode::PORT_TYPE_SCALAR: {
								inputs[i] = "vec2(" + src_var + ")";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_INT: {
								inputs[i] = "vec2(float(" + src_var + "))";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_UINT: {
								inputs[i] = "vec2(float(" + src_var + "))";
							} break;
							case VisualShaderNode::PORT_TYPE_BOOLEAN: {
								inputs[i] = "vec2(" + src_var + " ? 1.0 : 0.0)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_3D:
							case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
								inputs[i] = "vec2(" + src_var + ".xy)";
							} break;
							default:
								break;
						}
					} break;

					case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
						switch (out_type) {
							case VisualShaderNode::PORT_TYPE_SCALAR: {
								inputs[i] = "vec3(" + src_var + ")";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_INT: {
								inputs[i] = "vec3(float(" + src_var + "))";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_UINT: {
								inputs[i] = "vec3(float(" + src_var + "))";
							} break;
							case VisualShaderNode::PORT_TYPE_BOOLEAN: {
								inputs[i] = "vec3(" + src_var + " ? 1.0 : 0.0)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
								inputs[i] = "vec3(" + src_var + ", 0.0)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
								inputs[i] = "vec3(" + src_var + ".xyz)";
							} break;
							default:
								break;
						}
					} break;
					case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
						switch (out_type) {
							case VisualShaderNode::PORT_TYPE_SCALAR: {
								inputs[i] = "vec4(" + src_var + ")";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_INT: {
								inputs[i] = "vec4(float(" + src_var + "))";
							} break;
							case VisualShaderNode::PORT_TYPE_SCALAR_UINT: {
								inputs[i] = "vec4(float(" + src_var + "))";
							} break;
							case VisualShaderNode::PORT_TYPE_BOOLEAN: {
								inputs[i] = "vec4(" + src_var + " ? 1.0 : 0.0)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
								inputs[i] = "vec4(" + src_var + ", 0.0, 0.0)";
							} break;
							case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
								inputs[i] = "vec4(" + src_var + ", 0.0)";
							} break;
							default:
								break;
						}
					} break;
					default:
						break;
				}
			}
		} else {
			if (!vsnode->is_generate_input_var(i)) {
				continue;
			}

			Variant defval = vsnode->get_input_port_default_value(i);
			if (defval.get_type() == Variant::FLOAT) {
				float val = defval;
				inputs[i] = "n_in" + itos(p_node) + "p" + itos(i);
				node_code += "	float " + inputs[i] + " = " + vformat("%.5f", val) + ";\n";
			} else if (defval.get_type() == Variant::INT) {
				int val = defval;
				inputs[i] = "n_in" + itos(p_node) + "p" + itos(i);
				if (vsnode->get_input_port_type(i) == VisualShaderNode::PORT_TYPE_SCALAR_UINT) {
					node_code += "	uint " + inputs[i] + " = " + itos(val) + "u;\n";
				} else {
					node_code += "	int " + inputs[i] + " = " + itos(val) + ";\n";
				}
			} else if (defval.get_type() == Variant::BOOL) {
				bool val = defval;
				inputs[i] = "n_in" + itos(p_node) + "p" + itos(i);
				node_code += "	bool " + inputs[i] + " = " + (val ? "true" : "false") + ";\n";
			} else if (defval.get_type() == Variant::VECTOR2) {
				Vector2 val = defval;
				inputs[i] = "n_in" + itos(p_node) + "p" + itos(i);
				node_code += "	vec2 " + inputs[i] + " = " + vformat("vec2(%.5f, %.5f);\n", val.x, val.y);
			} else if (defval.get_type() == Variant::VECTOR3) {
				Vector3 val = defval;
				inputs[i] = "n_in" + itos(p_node) + "p" + itos(i);
				node_code += "	vec3 " + inputs[i] + " = " + vformat("vec3(%.5f, %.5f, %.5f);\n", val.x, val.y, val.z);
			} else if (defval.get_type() == Variant::VECTOR4) {
				Vector4 val = defval;
				inputs[i] = "n_in" + itos(p_node) + "p" + itos(i);
				node_code += "	vec4 " + inputs[i] + " = " + vformat("vec4(%.5f, %.5f, %.5f, %.5f);\n", val.x, val.y, val.z, val.w);
			} else if (defval.get_type() == Variant::QUATERNION) {
				Quaternion val = defval;
				inputs[i] = "n_in" + itos(p_node) + "p" + itos(i);
				node_code += "	vec4 " + inputs[i] + " = " + vformat("vec4(%.5f, %.5f, %.5f, %.5f);\n", val.x, val.y, val.z, val.w);
			} else if (defval.get_type() == Variant::TRANSFORM3D) {
				Transform3D val = defval;
				val.basis.transpose();
				inputs[i] = "n_in" + itos(p_node) + "p" + itos(i);
				Array values;
				for (int j = 0; j < 3; j++) {
					values.push_back(val.basis[j].x);
					values.push_back(val.basis[j].y);
					values.push_back(val.basis[j].z);
				}
				values.push_back(val.origin.x);
				values.push_back(val.origin.y);
				values.push_back(val.origin.z);
				bool err = false;
				node_code += "	mat4 " + inputs[i] + " = " + String("mat4(vec4(%.5f, %.5f, %.5f, 0.0), vec4(%.5f, %.5f, %.5f, 0.0), vec4(%.5f, %.5f, %.5f, 0.0), vec4(%.5f, %.5f, %.5f, 1.0));\n").sprintf(values, &err);
			} else {
				//will go empty, node is expected to know what it is doing at this point and handle it
			}
		}
	}

	int output_count = vsnode->get_output_port_count();
	int initial_output_count = output_count;

	HashMap<int, bool> expanded_output_ports;

	for (int i = 0; i < initial_output_count; i++) {
		bool expanded = false;

		if (vsnode->is_output_port_expandable(i) && vsnode->_is_output_port_expanded(i)) {
			expanded = true;

			switch (vsnode->get_output_port_type(i)) {
				case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
					output_count += 2;
				} break;
				case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
					output_count += 3;
				} break;
				case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
					output_count += 4;
				} break;
				default:
					break;
			}
		}
		expanded_output_ports.insert(i, expanded);
	}

	Vector<String> output_vars;
	output_vars.resize(output_count);
	String *outputs = output_vars.ptrw();

	if (vsnode->is_simple_decl()) { // less code to generate for some simple_decl nodes
		for (int i = 0, j = 0; i < initial_output_count; i++, j++) {
			String var_name = "n_out" + itos(p_node) + "p" + itos(j);
			switch (vsnode->get_output_port_type(i)) {
				case VisualShaderNode::PORT_TYPE_SCALAR:
					outputs[i] = "float " + var_name;
					break;
				case VisualShaderNode::PORT_TYPE_SCALAR_INT:
					outputs[i] = "int " + var_name;
					break;
				case VisualShaderNode::PORT_TYPE_SCALAR_UINT:
					outputs[i] = "uint " + var_name;
					break;
				case VisualShaderNode::PORT_TYPE_VECTOR_2D:
					outputs[i] = "vec2 " + var_name;
					break;
				case VisualShaderNode::PORT_TYPE_VECTOR_3D:
					outputs[i] = "vec3 " + var_name;
					break;
				case VisualShaderNode::PORT_TYPE_VECTOR_4D:
					outputs[i] = "vec4 " + var_name;
					break;
				case VisualShaderNode::PORT_TYPE_BOOLEAN:
					outputs[i] = "bool " + var_name;
					break;
				case VisualShaderNode::PORT_TYPE_TRANSFORM:
					outputs[i] = "mat4 " + var_name;
					break;
				default:
					break;
			}
			if (expanded_output_ports[i]) {
				switch (vsnode->get_output_port_type(i)) {
					case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
						j += 2;
					} break;
					case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
						j += 3;
					} break;
					case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
						j += 4;
					} break;
					default:
						break;
				}
			}
		}

	} else {
		for (int i = 0, j = 0; i < initial_output_count; i++, j++) {
			outputs[i] = "n_out" + itos(p_node) + "p" + itos(j);
			switch (vsnode->get_output_port_type(i)) {
				case VisualShaderNode::PORT_TYPE_SCALAR:
					r_code += "	float " + outputs[i] + ";\n";
					break;
				case VisualShaderNode::PORT_TYPE_SCALAR_INT:
					r_code += "	int " + outputs[i] + ";\n";
					break;
				case VisualShaderNode::PORT_TYPE_SCALAR_UINT:
					r_code += "	uint " + outputs[i] + ";\n";
					break;
				case VisualShaderNode::PORT_TYPE_VECTOR_2D:
					r_code += "	vec2 " + outputs[i] + ";\n";
					break;
				case VisualShaderNode::PORT_TYPE_VECTOR_3D:
					r_code += "	vec3 " + outputs[i] + ";\n";
					break;
				case VisualShaderNode::PORT_TYPE_VECTOR_4D:
					r_code += "	vec4 " + outputs[i] + ";\n";
					break;
				case VisualShaderNode::PORT_TYPE_BOOLEAN:
					r_code += "	bool " + outputs[i] + ";\n";
					break;
				case VisualShaderNode::PORT_TYPE_TRANSFORM:
					r_code += "	mat4 " + outputs[i] + ";\n";
					break;
				default:
					break;
			}
			if (expanded_output_ports[i]) {
				switch (vsnode->get_output_port_type(i)) {
					case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
						j += 2;
					} break;
					case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
						j += 3;
					} break;
					case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
						j += 4;
					} break;
					default:
						break;
				}
			}
		}
	}

	node_code += vsnode->generate_code(get_mode(), type, p_node, inputs, outputs, p_for_preview);
	if (!node_code.is_empty()) {
		r_code += node_name;
		r_code += node_code;
	}

	for (int i = 0; i < output_count; i++) {
		if (expanded_output_ports[i]) {
			switch (vsnode->get_output_port_type(i)) {
				case VisualShaderNode::PORT_TYPE_VECTOR_2D: {
					if (vsnode->is_output_port_connected(i + 1) || (p_for_preview && vsnode->get_output_port_for_preview() == (i + 1))) { // red-component
						String r = "n_out" + itos(p_node) + "p" + itos(i + 1);
						r_code += "	float " + r + " = n_out" + itos(p_node) + "p" + itos(i) + ".r;\n";
						outputs[i + 1] = r;
					}

					if (vsnode->is_output_port_connected(i + 2) || (p_for_preview && vsnode->get_output_port_for_preview() == (i + 2))) { // green-component
						String g = "n_out" + itos(p_node) + "p" + itos(i + 2);
						r_code += "	float " + g + " = n_out" + itos(p_node) + "p" + itos(i) + ".g;\n";
						outputs[i + 2] = g;
					}

					i += 2;
				} break;
				case VisualShaderNode::PORT_TYPE_VECTOR_3D: {
					if (vsnode->is_output_port_connected(i + 1) || (p_for_preview && vsnode->get_output_port_for_preview() == (i + 1))) { // red-component
						String r = "n_out" + itos(p_node) + "p" + itos(i + 1);
						r_code += "	float " + r + " = n_out" + itos(p_node) + "p" + itos(i) + ".r;\n";
						outputs[i + 1] = r;
					}

					if (vsnode->is_output_port_connected(i + 2) || (p_for_preview && vsnode->get_output_port_for_preview() == (i + 2))) { // green-component
						String g = "n_out" + itos(p_node) + "p" + itos(i + 2);
						r_code += "	float " + g + " = n_out" + itos(p_node) + "p" + itos(i) + ".g;\n";
						outputs[i + 2] = g;
					}

					if (vsnode->is_output_port_connected(i + 3) || (p_for_preview && vsnode->get_output_port_for_preview() == (i + 3))) { // blue-component
						String b = "n_out" + itos(p_node) + "p" + itos(i + 3);
						r_code += "	float " + b + " = n_out" + itos(p_node) + "p" + itos(i) + ".b;\n";
						outputs[i + 3] = b;
					}

					i += 3;
				} break;
				case VisualShaderNode::PORT_TYPE_VECTOR_4D: {
					if (vsnode->is_output_port_connected(i + 1) || (p_for_preview && vsnode->get_output_port_for_preview() == (i + 1))) { // red-component
						String r = "n_out" + itos(p_node) + "p" + itos(i + 1);
						r_code += "	float " + r + " = n_out" + itos(p_node) + "p" + itos(i) + ".r;\n";
						outputs[i + 1] = r;
					}

					if (vsnode->is_output_port_connected(i + 2) || (p_for_preview && vsnode->get_output_port_for_preview() == (i + 2))) { // green-component
						String g = "n_out" + itos(p_node) + "p" + itos(i + 2);
						r_code += "	float " + g + " = n_out" + itos(p_node) + "p" + itos(i) + ".g;\n";
						outputs[i + 2] = g;
					}

					if (vsnode->is_output_port_connected(i + 3) || (p_for_preview && vsnode->get_output_port_for_preview() == (i + 3))) { // blue-component
						String b = "n_out" + itos(p_node) + "p" + itos(i + 3);
						r_code += "	float " + b + " = n_out" + itos(p_node) + "p" + itos(i) + ".b;\n";
						outputs[i + 3] = b;
					}

					if (vsnode->is_output_port_connected(i + 4) || (p_for_preview && vsnode->get_output_port_for_preview() == (i + 4))) { // alpha-component
						String a = "n_out" + itos(p_node) + "p" + itos(i + 4);
						r_code += "	float " + a + " = n_out" + itos(p_node) + "p" + itos(i) + ".a;\n";
						outputs[i + 4] = a;
					}

					i += 4;
				} break;
				default:
					break;
			}
		}
	}

	if (!node_code.is_empty()) {
		r_code += "\n\n";
	}

	r_processed.insert(p_node);

	return OK;
}

bool VisualShader::has_func_name(RenderingServer::ShaderMode p_mode, const String &p_func_name) const {
	if (!ShaderTypes::get_singleton()->get_functions(p_mode).has(p_func_name)) {
		if (p_mode == RenderingServer::ShaderMode::SHADER_PARTICLES) {
			if (p_func_name == "start_custom" || p_func_name == "process_custom" || p_func_name == "collide") {
				return true;
			}
		}
		return false;
	}

	return true;
}

void VisualShader::_update_shader() const {
	if (!dirty.is_set()) {
		return;
	}

	dirty.clear();

	StringBuilder global_code;
	StringBuilder global_code_per_node;
	HashMap<Type, StringBuilder> global_code_per_func;
	StringBuilder shader_code;
	Vector<VisualShader::DefaultTextureParam> default_tex_params;
	HashSet<StringName> classes;
	HashMap<int, int> insertion_pos;
	static const char *shader_mode_str[Shader::MODE_MAX] = { "spatial", "canvas_item", "particles", "sky", "fog" };

	global_code += String() + "shader_type " + shader_mode_str[shader_mode] + ";\n";

	String render_mode;

	{
		const Vector<ShaderLanguage::ModeInfo> &rmodes = ShaderTypes::get_singleton()->get_modes(RenderingServer::ShaderMode(shader_mode));
		Vector<String> flag_names;

		// Add enum modes first.
		for (int i = 0; i < rmodes.size(); i++) {
			const ShaderLanguage::ModeInfo &info = rmodes[i];
			const String temp = String(info.name);

			if (!info.options.is_empty()) {
				if (!render_mode.is_empty()) {
					render_mode += ", ";
				}
				// Always write out a render_mode for the enumerated modes as having no render mode is not always
				// the same as the default. i.e. for depth_draw_opaque, the render mode has to be declared for it
				// to work properly, no render mode is an invalid option.
				if (modes.has(temp) && modes[temp] < info.options.size()) {
					render_mode += temp + "_" + info.options[modes[temp]];
				} else {
					// Use the default.
					render_mode += temp + "_" + info.options[0];
				}
			} else if (flags.has(temp)) {
				flag_names.push_back(temp);
			}
		}

		// Add flags afterward.
		for (int i = 0; i < flag_names.size(); i++) {
			if (!render_mode.is_empty()) {
				render_mode += ", ";
			}
			render_mode += flag_names[i];
		}
	}

	if (!render_mode.is_empty()) {
		global_code += "render_mode " + render_mode + ";\n\n";
	}

	static const char *func_name[TYPE_MAX] = { "vertex", "fragment", "light", "start", "process", "collide", "start_custom", "process_custom", "sky", "fog" };

	String global_expressions;
	HashSet<String> used_parameter_names;
	List<VisualShaderNodeParameter *> parameters;
	HashMap<int, List<int>> emitters;
	HashMap<int, List<int>> varying_setters;

	for (int i = 0, index = 0; i < TYPE_MAX; i++) {
		if (!has_func_name(RenderingServer::ShaderMode(shader_mode), func_name[i])) {
			continue;
		}

		for (const KeyValue<int, Node> &E : graph[i].nodes) {
			Ref<VisualShaderNodeGlobalExpression> global_expression = E.value.node;
			if (global_expression.is_valid()) {
				String expr = "";
				expr += "// " + global_expression->get_caption() + ":" + itos(index++) + "\n";
				expr += global_expression->generate_global(get_mode(), Type(i), -1);
				expr = expr.replace("\n", "\n	");
				expr += "\n";
				global_expressions += expr;
			}
			Ref<VisualShaderNodeParameterRef> parameter_ref = E.value.node;
			if (parameter_ref.is_valid()) {
				used_parameter_names.insert(parameter_ref->get_parameter_name());
			}
			Ref<VisualShaderNodeParameter> parameter = E.value.node;
			if (parameter.is_valid()) {
				parameters.push_back(parameter.ptr());
			}
			Ref<VisualShaderNodeVaryingSetter> varying_setter = E.value.node;
			if (varying_setter.is_valid() && varying_setter->is_input_port_connected(0)) {
				if (!varying_setters.has(i)) {
					varying_setters.insert(i, List<int>());
				}
				varying_setters[i].push_back(E.key);
			}
			Ref<VisualShaderNodeParticleEmit> emit_particle = E.value.node;
			if (emit_particle.is_valid()) {
				if (!emitters.has(i)) {
					emitters.insert(i, List<int>());
				}
				emitters[i].push_back(E.key);
			}
		}
	}

	int idx = 0;
	for (List<VisualShaderNodeParameter *>::Iterator itr = parameters.begin(); itr != parameters.end(); ++itr, ++idx) {
		VisualShaderNodeParameter *parameter = *itr;
		if (used_parameter_names.has(parameter->get_parameter_name())) {
			global_code += parameter->generate_global(get_mode(), Type(idx), -1);
			parameter->set_global_code_generated(true);
		} else {
			parameter->set_global_code_generated(false);
		}
	}

	if (!varyings.is_empty()) {
		global_code += "\n// Varyings\n";

		for (const KeyValue<String, Varying> &E : varyings) {
			global_code += "varying ";
			switch (E.value.type) {
				case VaryingType::VARYING_TYPE_FLOAT:
					global_code += "float ";
					break;
				case VaryingType::VARYING_TYPE_INT:
					if (E.value.mode == VaryingMode::VARYING_MODE_VERTEX_TO_FRAG_LIGHT) {
						global_code += "flat ";
					}
					global_code += "int ";
					break;
				case VaryingType::VARYING_TYPE_UINT:
					if (E.value.mode == VaryingMode::VARYING_MODE_VERTEX_TO_FRAG_LIGHT) {
						global_code += "flat ";
					}
					global_code += "uint ";
					break;
				case VaryingType::VARYING_TYPE_VECTOR_2D:
					global_code += "vec2 ";
					break;
				case VaryingType::VARYING_TYPE_VECTOR_3D:
					global_code += "vec3 ";
					break;
				case VaryingType::VARYING_TYPE_VECTOR_4D:
					global_code += "vec4 ";
					break;
				case VaryingType::VARYING_TYPE_BOOLEAN:
					global_code += "bool ";
					break;
				case VaryingType::VARYING_TYPE_TRANSFORM:
					global_code += "mat4 ";
					break;
				default:
					break;
			}
			global_code += vformat("var_%s;\n", E.key);
		}

		global_code += "\n";
	}

	HashMap<int, String> code_map;
	HashSet<int> empty_funcs;

	for (int i = 0; i < TYPE_MAX; i++) {
		if (!has_func_name(RenderingServer::ShaderMode(shader_mode), func_name[i])) {
			continue;
		}

		//make it faster to go around through shader
		VMap<ConnectionKey, const List<Connection>::Element *> input_connections;
		VMap<ConnectionKey, const List<Connection>::Element *> output_connections;

		StringBuilder func_code;
		HashSet<int> processed;

		bool is_empty_func = false;
		if (shader_mode != Shader::MODE_PARTICLES && shader_mode != Shader::MODE_SKY && shader_mode != Shader::MODE_FOG) {
			is_empty_func = true;
		}

		String varying_code;
		if (shader_mode == Shader::MODE_SPATIAL || shader_mode == Shader::MODE_CANVAS_ITEM) {
			for (const KeyValue<String, Varying> &E : varyings) {
				if ((E.value.mode == VARYING_MODE_VERTEX_TO_FRAG_LIGHT && i == TYPE_VERTEX) || (E.value.mode == VARYING_MODE_FRAG_TO_LIGHT && i == TYPE_FRAGMENT)) {
					bool found = false;
					for (int key : varying_setters[i]) {
						Ref<VisualShaderNodeVaryingSetter> setter = graph[i].nodes[key].node;
						if (setter.is_valid() && E.value.name == setter->get_varying_name()) {
							found = true;
							break;
						}
					}

					if (!found) {
						String code2;
						switch (E.value.type) {
							case VaryingType::VARYING_TYPE_FLOAT:
								code2 += "0.0";
								break;
							case VaryingType::VARYING_TYPE_INT:
								code2 += "0";
								break;
							case VaryingType::VARYING_TYPE_UINT:
								code2 += "0u";
								break;
							case VaryingType::VARYING_TYPE_VECTOR_2D:
								code2 += "vec2(0.0)";
								break;
							case VaryingType::VARYING_TYPE_VECTOR_3D:
								code2 += "vec3(0.0)";
								break;
							case VaryingType::VARYING_TYPE_VECTOR_4D:
								code2 += "vec4(0.0)";
								break;
							case VaryingType::VARYING_TYPE_BOOLEAN:
								code2 += "false";
								break;
							case VaryingType::VARYING_TYPE_TRANSFORM:
								code2 += "mat4(1.0)";
								break;
							default:
								break;
						}
						varying_code += vformat("	var_%s = %s;\n", E.key, code2);
					}
					is_empty_func = false;
				}
			}
		}

		for (const List<Connection>::Element *E = graph[i].connections.front(); E; E = E->next()) {
			ConnectionKey from_key;
			from_key.node = E->get().from_node;
			from_key.port = E->get().from_port;

			output_connections.insert(from_key, E);

			ConnectionKey to_key;
			to_key.node = E->get().to_node;
			to_key.port = E->get().to_port;

			input_connections.insert(to_key, E);

			if (is_empty_func && to_key.node == NODE_ID_OUTPUT) {
				is_empty_func = false;
			}
		}

		if (is_empty_func) {
			empty_funcs.insert(i);
			continue;
		}

		if (shader_mode != Shader::MODE_PARTICLES) {
			func_code += "\nvoid " + String(func_name[i]) + "() {\n";
		}
		insertion_pos.insert(i, shader_code.get_string_length() + func_code.get_string_length());

		Error err = _write_node(Type(i), &global_code, &global_code_per_node, &global_code_per_func, func_code, default_tex_params, input_connections, output_connections, NODE_ID_OUTPUT, processed, false, classes);
		ERR_FAIL_COND(err != OK);

		if (varying_setters.has(i)) {
			for (int &E : varying_setters[i]) {
				err = _write_node(Type(i), &global_code, &global_code_per_node, nullptr, func_code, default_tex_params, input_connections, output_connections, E, processed, false, classes);
				ERR_FAIL_COND(err != OK);
			}
		}

		if (emitters.has(i)) {
			for (int &E : emitters[i]) {
				err = _write_node(Type(i), &global_code, &global_code_per_node, &global_code_per_func, func_code, default_tex_params, input_connections, output_connections, E, processed, false, classes);
				ERR_FAIL_COND(err != OK);
			}
		}

		if (shader_mode == Shader::MODE_PARTICLES) {
			code_map.insert(i, func_code);
		} else {
			func_code += varying_code;
			func_code += "}\n";
			shader_code += func_code;
		}
	}

	String global_compute_code;

	if (shader_mode == Shader::MODE_PARTICLES) {
		bool has_start_custom = !code_map[TYPE_START_CUSTOM].is_empty();
		bool has_process = !code_map[TYPE_PROCESS].is_empty();
		bool has_process_custom = !code_map[TYPE_PROCESS_CUSTOM].is_empty();
		bool has_collide = !code_map[TYPE_COLLIDE].is_empty();

		shader_code += "void start() {\n";
		shader_code += "	uint __seed = __hash(NUMBER + uint(1) + RANDOM_SEED);\n";
		shader_code += "\n";
		shader_code += "	{\n";
		shader_code += code_map[TYPE_START].replace("\n	", "\n		");
		shader_code += "	}\n";
		if (has_start_custom) {
			shader_code += "	\n";
			shader_code += "	{\n";
			shader_code += code_map[TYPE_START_CUSTOM].replace("\n	", "\n		");
			shader_code += "	}\n";
		}
		shader_code += "}\n\n";

		if (has_process || has_process_custom || has_collide) {
			shader_code += "void process() {\n";
			shader_code += "	uint __seed = __hash(NUMBER + uint(1) + RANDOM_SEED);\n";
			shader_code += "\n";
			if (has_process || has_collide) {
				shader_code += "	{\n";
			}
			String tab = "	";
			if (has_collide) {
				shader_code += "		if (COLLIDED) {\n\n";
				shader_code += code_map[TYPE_COLLIDE].replace("\n	", "\n			");
				if (has_process) {
					shader_code += "		} else {\n\n";
					tab += "	";
				}
			}
			if (has_process) {
				shader_code += code_map[TYPE_PROCESS].replace("\n	", "\n	" + tab);
			}
			if (has_collide) {
				shader_code += "		}\n";
			}
			if (has_process || has_collide) {
				shader_code += "	}\n";
			}

			if (has_process_custom) {
				if (has_process || has_collide) {
					shader_code += "	\n";
				}
				shader_code += "	{\n";
				shader_code += code_map[TYPE_PROCESS_CUSTOM].replace("\n	", "\n		");
				shader_code += "	}\n";
			}

			shader_code += "}\n\n";
		}

		global_compute_code += "float __rand_from_seed(inout uint seed) {\n";
		global_compute_code += "	int k;\n";
		global_compute_code += "	int s = int(seed);\n";
		global_compute_code += "	if (s == 0)\n";
		global_compute_code += "	s = 305420679;\n";
		global_compute_code += "	k = s / 127773;\n";
		global_compute_code += "	s = 16807 * (s - k * 127773) - 2836 * k;\n";
		global_compute_code += "	if (s < 0)\n";
		global_compute_code += "		s += 2147483647;\n";
		global_compute_code += "	seed = uint(s);\n";
		global_compute_code += "	return float(seed % uint(65536)) / 65535.0;\n";
		global_compute_code += "}\n\n";

		global_compute_code += "float __rand_from_seed_m1_p1(inout uint seed) {\n";
		global_compute_code += "	return __rand_from_seed(seed) * 2.0 - 1.0;\n";
		global_compute_code += "}\n\n";

		global_compute_code += "float __randf_range(inout uint seed, float from, float to) {\n";
		global_compute_code += "	return __rand_from_seed(seed) * (to - from) + from;\n";
		global_compute_code += "}\n\n";

		global_compute_code += "uint __hash(uint x) {\n";
		global_compute_code += "	x = ((x >> uint(16)) ^ x) * uint(73244475);\n";
		global_compute_code += "	x = ((x >> uint(16)) ^ x) * uint(73244475);\n";
		global_compute_code += "	x = (x >> uint(16)) ^ x;\n";
		global_compute_code += "	return x;\n";
		global_compute_code += "}\n\n";

		global_compute_code += "mat3 __build_rotation_mat3(vec3 axis, float angle) {\n";
		global_compute_code += "	axis = normalize(axis);\n";
		global_compute_code += "	float s = sin(angle);\n";
		global_compute_code += "	float c = cos(angle);\n";
		global_compute_code += "	float oc = 1.0 - c;\n";
		global_compute_code += "	return mat3(vec3(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s), vec3(oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s), vec3(oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c));\n";
		global_compute_code += "}\n\n";

		global_compute_code += "mat4 __build_rotation_mat4(vec3 axis, float angle) {\n";
		global_compute_code += "	axis = normalize(axis);\n";
		global_compute_code += "	float s = sin(angle);\n";
		global_compute_code += "	float c = cos(angle);\n";
		global_compute_code += "	float oc = 1.0 - c;\n";
		global_compute_code += "	return mat4(vec4(oc * axis.x * axis.x + c, oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s, 0), vec4(oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c, oc * axis.y * axis.z - axis.x * s, 0), vec4(oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c, 0), vec4(0, 0, 0, 1));\n";
		global_compute_code += "}\n\n";

		global_compute_code += "vec2 __get_random_unit_vec2(inout uint seed) {\n";
		global_compute_code += "	return normalize(vec2(__rand_from_seed_m1_p1(seed), __rand_from_seed_m1_p1(seed)));\n";
		global_compute_code += "}\n\n";

		global_compute_code += "vec3 __get_random_unit_vec3(inout uint seed) {\n";
		global_compute_code += "	return normalize(vec3(__rand_from_seed_m1_p1(seed), __rand_from_seed_m1_p1(seed), __rand_from_seed_m1_p1(seed)));\n";
		global_compute_code += "}\n\n";
	}

	//set code secretly
	global_code += "\n\n";
	String final_code = global_code;
	final_code += global_compute_code;
	final_code += global_code_per_node;
	final_code += global_expressions;
	String tcode = shader_code;
	for (int i = 0; i < TYPE_MAX; i++) {
		if (!has_func_name(RenderingServer::ShaderMode(shader_mode), func_name[i])) {
			continue;
		}
		String func_code = global_code_per_func[Type(i)].as_string();
		if (empty_funcs.has(Type(i)) && !func_code.is_empty()) {
			func_code = vformat("%s%s%s", String("\nvoid " + String(func_name[i]) + "() {\n"), func_code, "}\n");
		}
		tcode = tcode.insert(insertion_pos[i], func_code);
	}
	final_code += tcode;

	const_cast<VisualShader *>(this)->set_code(final_code);
	for (int i = 0; i < default_tex_params.size(); i++) {
		int j = 0;
		for (List<Ref<Texture>>::ConstIterator itr = default_tex_params[i].params.begin(); itr != default_tex_params[i].params.end(); ++itr, ++j) {
			const_cast<VisualShader *>(this)->set_default_texture_parameter(default_tex_params[i].name, *itr, j);
		}
	}
	if (previous_code != final_code) {
		const_cast<VisualShader *>(this)->emit_signal(CoreStringName(changed));
	}
	previous_code = final_code;
}

void VisualShader::_queue_update() {
	if (dirty.is_set()) {
		return;
	}

	dirty.set();
	call_deferred(SNAME("_update_shader"));
}

void VisualShader::rebuild() {
	dirty.set();
	_update_shader();
}

void VisualShader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &VisualShader::set_mode);

	ClassDB::bind_method(D_METHOD("add_node", "type", "node", "position", "id"), &VisualShader::add_node);
	ClassDB::bind_method(D_METHOD("get_node", "type", "id"), &VisualShader::get_node);

	ClassDB::bind_method(D_METHOD("set_node_position", "type", "id", "position"), &VisualShader::set_node_position);
	ClassDB::bind_method(D_METHOD("get_node_position", "type", "id"), &VisualShader::get_node_position);

	ClassDB::bind_method(D_METHOD("get_node_list", "type"), &VisualShader::get_node_list);
	ClassDB::bind_method(D_METHOD("get_valid_node_id", "type"), &VisualShader::get_valid_node_id);

	ClassDB::bind_method(D_METHOD("remove_node", "type", "id"), &VisualShader::remove_node);
	ClassDB::bind_method(D_METHOD("replace_node", "type", "id", "new_class"), &VisualShader::replace_node);

	ClassDB::bind_method(D_METHOD("is_node_connection", "type", "from_node", "from_port", "to_node", "to_port"), &VisualShader::is_node_connection);
	ClassDB::bind_method(D_METHOD("can_connect_nodes", "type", "from_node", "from_port", "to_node", "to_port"), &VisualShader::can_connect_nodes);

	ClassDB::bind_method(D_METHOD("connect_nodes", "type", "from_node", "from_port", "to_node", "to_port"), &VisualShader::connect_nodes);
	ClassDB::bind_method(D_METHOD("disconnect_nodes", "type", "from_node", "from_port", "to_node", "to_port"), &VisualShader::disconnect_nodes);
	ClassDB::bind_method(D_METHOD("connect_nodes_forced", "type", "from_node", "from_port", "to_node", "to_port"), &VisualShader::connect_nodes_forced);

	ClassDB::bind_method(D_METHOD("get_node_connections", "type"), &VisualShader::_get_node_connections);

	ClassDB::bind_method(D_METHOD("set_graph_offset", "offset"), &VisualShader::set_graph_offset);
	ClassDB::bind_method(D_METHOD("get_graph_offset"), &VisualShader::get_graph_offset);

	ClassDB::bind_method(D_METHOD("attach_node_to_frame", "type", "id", "frame"), &VisualShader::attach_node_to_frame);
	ClassDB::bind_method(D_METHOD("detach_node_from_frame", "type", "id"), &VisualShader::detach_node_from_frame);

	ClassDB::bind_method(D_METHOD("add_varying", "name", "mode", "type"), &VisualShader::add_varying);
	ClassDB::bind_method(D_METHOD("remove_varying", "name"), &VisualShader::remove_varying);
	ClassDB::bind_method(D_METHOD("has_varying", "name"), &VisualShader::has_varying);

	ClassDB::bind_method(D_METHOD("_set_preview_shader_parameter", "name", "value"), &VisualShader::_set_preview_shader_parameter);
	ClassDB::bind_method(D_METHOD("_get_preview_shader_parameter", "name"), &VisualShader::_get_preview_shader_parameter);
	ClassDB::bind_method(D_METHOD("_has_preview_shader_parameter", "name"), &VisualShader::_has_preview_shader_parameter);

	ClassDB::bind_method(D_METHOD("_update_shader"), &VisualShader::_update_shader);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "graph_offset", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_graph_offset", "get_graph_offset");

	ADD_PROPERTY_DEFAULT("code", ""); // Inherited from Shader, prevents showing default code as override in docs.

	BIND_ENUM_CONSTANT(TYPE_VERTEX);
	BIND_ENUM_CONSTANT(TYPE_FRAGMENT);
	BIND_ENUM_CONSTANT(TYPE_LIGHT);
	BIND_ENUM_CONSTANT(TYPE_START);
	BIND_ENUM_CONSTANT(TYPE_PROCESS);
	BIND_ENUM_CONSTANT(TYPE_COLLIDE);
	BIND_ENUM_CONSTANT(TYPE_START_CUSTOM);
	BIND_ENUM_CONSTANT(TYPE_PROCESS_CUSTOM);
	BIND_ENUM_CONSTANT(TYPE_SKY);
	BIND_ENUM_CONSTANT(TYPE_FOG);
	BIND_ENUM_CONSTANT(TYPE_MAX);

	BIND_ENUM_CONSTANT(VARYING_MODE_VERTEX_TO_FRAG_LIGHT);
	BIND_ENUM_CONSTANT(VARYING_MODE_FRAG_TO_LIGHT);
	BIND_ENUM_CONSTANT(VARYING_MODE_MAX);

	BIND_ENUM_CONSTANT(VARYING_TYPE_FLOAT);
	BIND_ENUM_CONSTANT(VARYING_TYPE_INT);
	BIND_ENUM_CONSTANT(VARYING_TYPE_UINT);
	BIND_ENUM_CONSTANT(VARYING_TYPE_VECTOR_2D);
	BIND_ENUM_CONSTANT(VARYING_TYPE_VECTOR_3D);
	BIND_ENUM_CONSTANT(VARYING_TYPE_VECTOR_4D);
	BIND_ENUM_CONSTANT(VARYING_TYPE_BOOLEAN);
	BIND_ENUM_CONSTANT(VARYING_TYPE_TRANSFORM);
	BIND_ENUM_CONSTANT(VARYING_TYPE_MAX);

	BIND_CONSTANT(NODE_ID_INVALID);
	BIND_CONSTANT(NODE_ID_OUTPUT);
}

VisualShader::VisualShader() {
	dirty.set();
	for (int i = 0; i < TYPE_MAX; i++) {
		if (i > (int)TYPE_LIGHT && i < (int)TYPE_SKY) {
			Ref<VisualShaderNodeParticleOutput> output;
			output.instantiate();
			output->shader_type = Type(i);
			output->shader_mode = shader_mode;
			graph[i].nodes[NODE_ID_OUTPUT].node = output;
		} else {
			Ref<VisualShaderNodeOutput> output;
			output.instantiate();
			output->shader_type = Type(i);
			output->shader_mode = shader_mode;
			graph[i].nodes[NODE_ID_OUTPUT].node = output;
		}

		graph[i].nodes[NODE_ID_OUTPUT].position = Vector2(400, 150);
	}
}

///////////////////////////////////////////////////////////

const VisualShaderNodeInput::Port VisualShaderNodeInput::ports[] = {
	// Spatial

	// Node3D, Vertex
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "binormal", "BINORMAL" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "camera_direction_world", "CAMERA_DIRECTION_WORLD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "camera_position_world", "CAMERA_POSITION_WORLD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "camera_visible_layers", "CAMERA_VISIBLE_LAYERS" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "clip_space_far", "CLIP_SPACE_FAR" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "COLOR" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom0", "CUSTOM0" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom1", "CUSTOM1" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom2", "CUSTOM2" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom3", "CUSTOM3" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "exposure", "EXPOSURE" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "eye_offset", "EYE_OFFSET" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "instance_custom", "INSTANCE_CUSTOM" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR_INT, "instance_id", "INSTANCE_ID" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_projection_matrix", "INV_PROJECTION_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_view_matrix", "INV_VIEW_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "model_matrix", "MODEL_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "modelview_matrix", "MODELVIEW_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "node_position_view", "NODE_POSITION_VIEW" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "node_position_world", "NODE_POSITION_WORLD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "normal", "NORMAL" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_BOOLEAN, "output_is_srgb", "OUTPUT_IS_SRGB" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "point_size", "POINT_SIZE" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "projection_matrix", "PROJECTION_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "roughness", "ROUGHNESS" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "tangent", "TANGENT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv2", "UV2" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "vertex", "VERTEX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR_INT, "vertex_id", "VERTEX_ID" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR_INT, "view_index", "VIEW_INDEX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "view_matrix", "VIEW_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR_INT, "view_mono_left", "VIEW_MONO_LEFT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR_INT, "view_right", "VIEW_RIGHT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "viewport_size", "VIEWPORT_SIZE" },

	// Node3D, Fragment
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "binormal", "BINORMAL" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "camera_direction_world", "CAMERA_DIRECTION_WORLD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "camera_position_world", "CAMERA_POSITION_WORLD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "camera_visible_layers", "CAMERA_VISIBLE_LAYERS" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "clip_space_far", "CLIP_SPACE_FAR" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "COLOR" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "exposure", "EXPOSURE" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "eye_offset", "EYE_OFFSET" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "fragcoord", "FRAGCOORD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_BOOLEAN, "front_facing", "FRONT_FACING" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_projection_matrix", "INV_PROJECTION_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_view_matrix", "INV_VIEW_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_TRANSFORM, "model_matrix", "MODEL_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "node_position_view", "NODE_POSITION_VIEW" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "node_position_world", "NODE_POSITION_WORLD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "normal", "NORMAL" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_BOOLEAN, "output_is_srgb", "OUTPUT_IS_SRGB" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "point_coord", "POINT_COORD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_TRANSFORM, "projection_matrix", "PROJECTION_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "screen_uv", "SCREEN_UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "tangent", "TANGENT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv2", "UV2" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "vertex", "VERTEX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "view", "VIEW" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR_INT, "view_index", "VIEW_INDEX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_TRANSFORM, "view_matrix", "VIEW_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR_INT, "view_mono_left", "VIEW_MONO_LEFT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR_INT, "view_right", "VIEW_RIGHT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "viewport_size", "VIEWPORT_SIZE" },

	// Node3D, Light
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "albedo", "ALBEDO" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "attenuation", "ATTENUATION" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "backlight", "BACKLIGHT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "clip_space_far", "CLIP_SPACE_FAR" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "diffuse", "DIFFUSE_LIGHT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "exposure", "EXPOSURE" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "fragcoord", "FRAGCOORD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_projection_matrix", "INV_PROJECTION_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_TRANSFORM, "inv_view_matrix", "INV_VIEW_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light", "LIGHT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light_color", "LIGHT_COLOR" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_BOOLEAN, "light_is_directional", "LIGHT_IS_DIRECTIONAL" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "metallic", "METALLIC" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_TRANSFORM, "model_matrix", "MODEL_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "normal", "NORMAL" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_BOOLEAN, "output_is_srgb", "OUTPUT_IS_SRGB" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_TRANSFORM, "projection_matrix", "PROJECTION_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "roughness", "ROUGHNESS" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "screen_uv", "SCREEN_UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "specular", "SPECULAR_LIGHT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv2", "UV2" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "view", "VIEW" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_TRANSFORM, "view_matrix", "VIEW_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "viewport_size", "VIEWPORT_SIZE" },

	// Canvas Item

	// Canvas Item, Vertex
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_BOOLEAN, "at_light_pass", "AT_LIGHT_PASS" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "canvas_matrix", "CANVAS_MATRIX" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "COLOR" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom0", "CUSTOM0" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom1", "CUSTOM1" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "instance_custom", "INSTANCE_CUSTOM" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR_INT, "instance_id", "INSTANCE_ID" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "model_matrix", "MODEL_MATRIX" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "point_size", "POINT_SIZE" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "screen_matrix", "SCREEN_MATRIX" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "texture_pixel_size", "TEXTURE_PIXEL_SIZE" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "vertex", "VERTEX" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR_INT, "vertex_id", "VERTEX_ID" },

	// Canvas Item, Fragment
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_BOOLEAN, "at_light_pass", "AT_LIGHT_PASS" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "COLOR" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "fragcoord", "FRAGCOORD" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SAMPLER, "normal_texture", "NORMAL_TEXTURE" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "point_coord", "POINT_COORD" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "screen_pixel_size", "SCREEN_PIXEL_SIZE" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "screen_uv", "SCREEN_UV" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "specular_shininess", "SPECULAR_SHININESS" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SAMPLER, "specular_shininess_texture", "SPECULAR_SHININESS_TEXTURE" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SAMPLER, "texture", "TEXTURE" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "texture_pixel_size", "TEXTURE_PIXEL_SIZE" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "vertex", "VERTEX" },

	// Canvas Item, Light
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "COLOR" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "fragcoord", "FRAGCOORD" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "light", "LIGHT" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "light_color", "LIGHT_COLOR" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light_direction", "LIGHT_DIRECTION" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "light_energy", "LIGHT_ENERGY" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_BOOLEAN, "light_is_directional", "LIGHT_IS_DIRECTIONAL" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light_position", "LIGHT_POSITION" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light_vertex", "LIGHT_VERTEX" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "normal", "NORMAL" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "point_coord", "POINT_COORD" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "screen_uv", "SCREEN_UV" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "shadow", "SHADOW_MODULATE" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "specular_shininess", "SPECULAR_SHININESS" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SAMPLER, "texture", "TEXTURE" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "texture_pixel_size", "TEXTURE_PIXEL_SIZE" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },

	// Particles, Start
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_BOOLEAN, "active", "ACTIVE" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_VECTOR_3D, "attractor_force", "ATTRACTOR_FORCE" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "COLOR" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom", "CUSTOM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_SCALAR, "delta", "DELTA" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_TRANSFORM, "emission_transform", "EMISSION_TRANSFORM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "index", "INDEX" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_SCALAR, "lifetime", "LIFETIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "number", "NUMBER" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "random_seed", "RANDOM_SEED" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_BOOLEAN, "restart", "RESTART" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_TRANSFORM, "transform", "TRANSFORM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_VECTOR_3D, "velocity", "VELOCITY" },

	// Particles, Start (Custom)
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_BOOLEAN, "active", "ACTIVE" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_VECTOR_3D, "attractor_force", "ATTRACTOR_FORCE" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "COLOR" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom", "CUSTOM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR, "delta", "DELTA" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_TRANSFORM, "emission_transform", "EMISSION_TRANSFORM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "index", "INDEX" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR, "lifetime", "LIFETIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "number", "NUMBER" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "random_seed", "RANDOM_SEED" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_BOOLEAN, "restart", "RESTART" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_TRANSFORM, "transform", "TRANSFORM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_VECTOR_3D, "velocity", "VELOCITY" },

	// Particles, Process
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_BOOLEAN, "active", "ACTIVE" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_VECTOR_3D, "attractor_force", "ATTRACTOR_FORCE" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "COLOR" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom", "CUSTOM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_SCALAR, "delta", "DELTA" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_TRANSFORM, "emission_transform", "EMISSION_TRANSFORM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "index", "INDEX" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_SCALAR, "lifetime", "LIFETIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "number", "NUMBER" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "random_seed", "RANDOM_SEED" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_BOOLEAN, "restart", "RESTART" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_TRANSFORM, "transform", "TRANSFORM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_VECTOR_3D, "velocity", "VELOCITY" },

	// Particles, Process (Custom)
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_BOOLEAN, "active", "ACTIVE" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_VECTOR_3D, "attractor_force", "ATTRACTOR_FORCE" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "COLOR" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom", "CUSTOM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR, "delta", "DELTA" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_TRANSFORM, "emission_transform", "EMISSION_TRANSFORM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "index", "INDEX" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR, "lifetime", "LIFETIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "number", "NUMBER" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "random_seed", "RANDOM_SEED" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_BOOLEAN, "restart", "RESTART" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_TRANSFORM, "transform", "TRANSFORM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_VECTOR_3D, "velocity", "VELOCITY" },

	// Particles, Collide
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_BOOLEAN, "active", "ACTIVE" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_VECTOR_3D, "attractor_force", "ATTRACTOR_FORCE" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_SCALAR, "collision_depth", "COLLISION_DEPTH" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_VECTOR_3D, "collision_normal", "COLLISION_NORMAL" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "COLOR" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_VECTOR_4D, "custom", "CUSTOM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_SCALAR, "delta", "DELTA" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_TRANSFORM, "emission_transform", "EMISSION_TRANSFORM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "index", "INDEX" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_SCALAR, "lifetime", "LIFETIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "number", "NUMBER" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_SCALAR_UINT, "random_seed", "RANDOM_SEED" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_BOOLEAN, "restart", "RESTART" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_TRANSFORM, "transform", "TRANSFORM" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_VECTOR_3D, "velocity", "VELOCITY" },

	// Sky, Sky
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_BOOLEAN, "at_cubemap_pass", "AT_CUBEMAP_PASS" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_BOOLEAN, "at_half_res_pass", "AT_HALF_RES_PASS" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_BOOLEAN, "at_quarter_res_pass", "AT_QUARTER_RES_PASS" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "eyedir", "EYEDIR" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_4D, "fragcoord", "FRAGCOORD" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_4D, "half_res_color", "HALF_RES_COLOR" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light0_color", "LIGHT0_COLOR" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light0_direction", "LIGHT0_DIRECTION" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_BOOLEAN, "light0_enabled", "LIGHT0_ENABLED" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_SCALAR, "light0_energy", "LIGHT0_ENERGY" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light1_color", "LIGHT1_COLOR" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light1_direction", "LIGHT1_DIRECTION" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_BOOLEAN, "light1_enabled", "LIGHT1_ENABLED" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_SCALAR, "light1_energy", "LIGHT1_ENERGY" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light2_color", "LIGHT2_COLOR" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light2_direction", "LIGHT2_DIRECTION" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_BOOLEAN, "light2_enabled", "LIGHT2_ENABLED" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_SCALAR, "light2_energy", "LIGHT2_ENERGY" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light3_color", "LIGHT3_COLOR" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "light3_direction", "LIGHT3_DIRECTION" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_BOOLEAN, "light3_enabled", "LIGHT3_ENABLED" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_SCALAR, "light3_energy", "LIGHT3_ENERGY" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "position", "POSITION" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_4D, "quarter_res_color", "QUARTER_RES_COLOR" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_SAMPLER, "radiance", "RADIANCE" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_2D, "screen_uv", "SCREEN_UV" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_2D, "sky_coords", "SKY_COORDS" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },

	// Fog, Fog
	{ Shader::MODE_FOG, VisualShader::TYPE_FOG, VisualShaderNode::PORT_TYPE_VECTOR_3D, "object_position", "OBJECT_POSITION" },
	{ Shader::MODE_FOG, VisualShader::TYPE_FOG, VisualShaderNode::PORT_TYPE_SCALAR, "sdf", "SDF" },
	{ Shader::MODE_FOG, VisualShader::TYPE_FOG, VisualShaderNode::PORT_TYPE_VECTOR_3D, "size", "SIZE" },
	{ Shader::MODE_FOG, VisualShader::TYPE_FOG, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_FOG, VisualShader::TYPE_FOG, VisualShaderNode::PORT_TYPE_VECTOR_3D, "uvw", "UVW" },
	{ Shader::MODE_FOG, VisualShader::TYPE_FOG, VisualShaderNode::PORT_TYPE_VECTOR_3D, "world_position", "WORLD_POSITION" },

	{ Shader::MODE_MAX, VisualShader::TYPE_MAX, VisualShaderNode::PORT_TYPE_TRANSFORM, nullptr, nullptr },
};

const VisualShaderNodeInput::Port VisualShaderNodeInput::preview_ports[] = {
	// Spatial, Vertex

	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "binormal", "vec3(1.0, 0.0, 0.0)" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "vec4(1.0)" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "normal", "vec3(0.0, 0.0, 1.0)" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "tangent", "vec3(0.0, 1.0, 0.0)" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv2", "UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "viewport_size", "vec2(1.0)" },

	// Spatial, Fragment

	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "binormal", "vec3(1.0, 0.0, 0.0)" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "vec4(1.0)" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "fragcoord", "FRAGCOORD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "normal", "vec3(0.0, 0.0, 1.0)" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "screen_uv", "SCREEN_UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "tangent", "vec3(0.0, 1.0, 0.0)" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv2", "UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "viewport_size", "vec2(1.0)" },

	// Spatial, Light

	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "fragcoord", "FRAGCOORD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "normal", "vec3(0.0, 0.0, 1.0)" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv2", "UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "viewport_size", "vec2(1.0)" },

	// Canvas Item, Vertex

	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "vec4(1.0)" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "vertex", "VERTEX" },

	// Canvas Item, Fragment

	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "vec4(1.0)" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "fragcoord", "FRAGCOORD" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "screen_uv", "UV" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },

	// Canvas Item, Light

	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "color", "vec4(1.0)" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_4D, "fragcoord", "FRAGCOORD" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "normal", "vec3(0.0, 0.0, 1.0)" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "screen_uv", "UV" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "uv", "UV" },

	// Particles

	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_COLLIDE, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_START_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },
	{ Shader::MODE_PARTICLES, VisualShader::TYPE_PROCESS_CUSTOM, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },

	// Sky

	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_2D, "screen_uv", "UV" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },

	// Fog

	{ Shader::MODE_FOG, VisualShader::TYPE_FOG, VisualShaderNode::PORT_TYPE_SCALAR, "time", "TIME" },

	{ Shader::MODE_MAX, VisualShader::TYPE_MAX, VisualShaderNode::PORT_TYPE_TRANSFORM, nullptr, nullptr },
};

int VisualShaderNodeInput::get_input_port_count() const {
	return 0;
}

VisualShaderNodeInput::PortType VisualShaderNodeInput::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeInput::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeInput::get_output_port_count() const {
	return 1;
}

VisualShaderNodeInput::PortType VisualShaderNodeInput::get_output_port_type(int p_port) const {
	return p_port == 0 ? get_input_type_by_name(input_name) : PORT_TYPE_SCALAR;
}

String VisualShaderNodeInput::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeInput::get_caption() const {
	return "Input";
}

bool VisualShaderNodeInput::is_output_port_expandable(int p_port) const {
	if (p_port == 0) {
		switch (get_input_type_by_name(input_name)) {
			case PORT_TYPE_VECTOR_2D:
				return true;
			case PORT_TYPE_VECTOR_3D:
				return true;
			case PORT_TYPE_VECTOR_4D:
				return true;
			default:
				return false;
		}
	}
	return false;
}

String VisualShaderNodeInput::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	if (get_output_port_type(0) == PORT_TYPE_SAMPLER) {
		return "";
	}

	if (p_for_preview) {
		int idx = 0;

		String code;

		while (preview_ports[idx].mode != Shader::MODE_MAX) {
			if (preview_ports[idx].mode == shader_mode && preview_ports[idx].shader_type == shader_type && preview_ports[idx].name == input_name) {
				code = "	" + p_output_vars[0] + " = " + preview_ports[idx].string + ";\n";
				break;
			}
			idx++;
		}

		if (code.is_empty()) {
			switch (get_output_port_type(0)) {
				case PORT_TYPE_SCALAR: {
					code = "	" + p_output_vars[0] + " = 0.0;\n";
				} break;
				case PORT_TYPE_SCALAR_INT: {
					code = "	" + p_output_vars[0] + " = 0;\n";
				} break;
				case PORT_TYPE_VECTOR_2D: {
					code = "	" + p_output_vars[0] + " = vec2(0.0);\n";
				} break;
				case PORT_TYPE_VECTOR_3D: {
					code = "	" + p_output_vars[0] + " = vec3(0.0);\n";
				} break;
				case PORT_TYPE_VECTOR_4D: {
					code = "	" + p_output_vars[0] + " = vec4(0.0);\n";
				} break;
				case PORT_TYPE_BOOLEAN: {
					code = "	" + p_output_vars[0] + " = false;\n";
				} break;
				default:
					break;
			}
		}

		return code;

	} else {
		int idx = 0;

		String code;

		while (ports[idx].mode != Shader::MODE_MAX) {
			if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type && ports[idx].name == input_name) {
				code = "	" + p_output_vars[0] + " = " + ports[idx].string + ";\n";
				break;
			}
			idx++;
		}

		if (code.is_empty()) {
			code = "	" + p_output_vars[0] + " = 0.0;\n"; //default (none found) is scalar
		}

		return code;
	}
}

void VisualShaderNodeInput::set_input_name(String p_name) {
	PortType prev_type = get_input_type_by_name(input_name);
	input_name = p_name;
	emit_changed();
	if (get_input_type_by_name(input_name) != prev_type) {
		emit_signal(SNAME("input_type_changed"));
	}
}

String VisualShaderNodeInput::get_input_name() const {
	return input_name;
}

String VisualShaderNodeInput::get_input_real_name() const {
	int idx = 0;

	while (ports[idx].mode != Shader::MODE_MAX) {
		if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type && ports[idx].name == input_name) {
			return String(ports[idx].string);
		}
		idx++;
	}

	return "";
}

VisualShaderNodeInput::PortType VisualShaderNodeInput::get_input_type_by_name(String p_name) const {
	int idx = 0;

	while (ports[idx].mode != Shader::MODE_MAX) {
		if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type && ports[idx].name == p_name) {
			return ports[idx].type;
		}
		idx++;
	}

	return PORT_TYPE_SCALAR;
}

int VisualShaderNodeInput::get_input_index_count() const {
	int idx = 0;
	int count = 0;

	while (ports[idx].mode != Shader::MODE_MAX) {
		if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
			count++;
		}
		idx++;
	}

	return count;
}

VisualShaderNodeInput::PortType VisualShaderNodeInput::get_input_index_type(int p_index) const {
	int idx = 0;
	int count = 0;

	while (ports[idx].mode != Shader::MODE_MAX) {
		if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
			if (count == p_index) {
				return ports[idx].type;
			}
			count++;
		}
		idx++;
	}

	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeInput::get_input_index_name(int p_index) const {
	int idx = 0;
	int count = 0;

	while (ports[idx].mode != Shader::MODE_MAX) {
		if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
			if (count == p_index) {
				return ports[idx].name;
			}
			count++;
		}
		idx++;
	}

	return "";
}

void VisualShaderNodeInput::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == "input_name") {
		String port_list;

		int idx = 0;

		while (ports[idx].mode != Shader::MODE_MAX) {
			if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
				if (!port_list.is_empty()) {
					port_list += ",";
				}
				port_list += ports[idx].name;
			}
			idx++;
		}

		if (port_list.is_empty()) {
			port_list = RTR("None");
		}
		p_property.hint_string = port_list;
	}
}

Vector<StringName> VisualShaderNodeInput::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("input_name");
	return props;
}

void VisualShaderNodeInput::set_shader_type(VisualShader::Type p_shader_type) {
	shader_type = p_shader_type;
}

void VisualShaderNodeInput::set_shader_mode(Shader::Mode p_shader_mode) {
	shader_mode = p_shader_mode;
}

void VisualShaderNodeInput::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_input_name", "name"), &VisualShaderNodeInput::set_input_name);
	ClassDB::bind_method(D_METHOD("get_input_name"), &VisualShaderNodeInput::get_input_name);
	ClassDB::bind_method(D_METHOD("get_input_real_name"), &VisualShaderNodeInput::get_input_real_name);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "input_name", PROPERTY_HINT_ENUM, ""), "set_input_name", "get_input_name");
	ADD_SIGNAL(MethodInfo("input_type_changed"));
}

VisualShaderNodeInput::VisualShaderNodeInput() {
}

////////////// ParameterRef

RBMap<RID, List<VisualShaderNodeParameterRef::Parameter>> parameters;

void VisualShaderNodeParameterRef::add_parameter(RID p_shader_rid, const String &p_name, ParameterType p_type) {
	parameters[p_shader_rid].push_back({ p_name, p_type });
}

void VisualShaderNodeParameterRef::clear_parameters(RID p_shader_rid) {
	parameters[p_shader_rid].clear();
}

bool VisualShaderNodeParameterRef::has_parameter(RID p_shader_rid, const String &p_name) {
	for (const VisualShaderNodeParameterRef::Parameter &E : parameters[p_shader_rid]) {
		if (E.name == p_name) {
			return true;
		}
	}
	return false;
}

String VisualShaderNodeParameterRef::get_caption() const {
	return "ParameterRef";
}

int VisualShaderNodeParameterRef::get_input_port_count() const {
	return 0;
}

VisualShaderNodeParameterRef::PortType VisualShaderNodeParameterRef::get_input_port_type(int p_port) const {
	return PortType::PORT_TYPE_SCALAR;
}

String VisualShaderNodeParameterRef::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeParameterRef::get_output_port_count() const {
	switch (param_type) {
		case PARAMETER_TYPE_FLOAT:
			return 1;
		case PARAMETER_TYPE_INT:
			return 1;
		case PARAMETER_TYPE_UINT:
			return 1;
		case PARAMETER_TYPE_BOOLEAN:
			return 1;
		case PARAMETER_TYPE_VECTOR2:
			return 1;
		case PARAMETER_TYPE_VECTOR3:
			return 1;
		case PARAMETER_TYPE_VECTOR4:
			return 1;
		case PARAMETER_TYPE_TRANSFORM:
			return 1;
		case PARAMETER_TYPE_COLOR:
			return 2;
		case UNIFORM_TYPE_SAMPLER:
			return 1;
		default:
			break;
	}
	return 1;
}

VisualShaderNodeParameterRef::PortType VisualShaderNodeParameterRef::get_output_port_type(int p_port) const {
	switch (param_type) {
		case PARAMETER_TYPE_FLOAT:
			return PortType::PORT_TYPE_SCALAR;
		case PARAMETER_TYPE_INT:
			return PortType::PORT_TYPE_SCALAR_INT;
		case PARAMETER_TYPE_UINT:
			return PortType::PORT_TYPE_SCALAR_UINT;
		case PARAMETER_TYPE_BOOLEAN:
			return PortType::PORT_TYPE_BOOLEAN;
		case PARAMETER_TYPE_VECTOR2:
			return PortType::PORT_TYPE_VECTOR_2D;
		case PARAMETER_TYPE_VECTOR3:
			return PortType::PORT_TYPE_VECTOR_3D;
		case PARAMETER_TYPE_VECTOR4:
			return PortType::PORT_TYPE_VECTOR_4D;
		case PARAMETER_TYPE_TRANSFORM:
			return PortType::PORT_TYPE_TRANSFORM;
		case PARAMETER_TYPE_COLOR:
			if (p_port == 0) {
				return PortType::PORT_TYPE_VECTOR_3D;
			} else if (p_port == 1) {
				return PORT_TYPE_SCALAR;
			}
			break;
		case UNIFORM_TYPE_SAMPLER:
			return PortType::PORT_TYPE_SAMPLER;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeParameterRef::get_output_port_name(int p_port) const {
	switch (param_type) {
		case PARAMETER_TYPE_FLOAT:
			return "";
		case PARAMETER_TYPE_INT:
			return "";
		case PARAMETER_TYPE_UINT:
			return "";
		case PARAMETER_TYPE_BOOLEAN:
			return "";
		case PARAMETER_TYPE_VECTOR2:
			return "";
		case PARAMETER_TYPE_VECTOR3:
			return "";
		case PARAMETER_TYPE_VECTOR4:
			return "";
		case PARAMETER_TYPE_TRANSFORM:
			return "";
		case PARAMETER_TYPE_COLOR:
			if (p_port == 0) {
				return "rgb";
			} else if (p_port == 1) {
				return "alpha";
			}
			break;
		case UNIFORM_TYPE_SAMPLER:
			return "";
			break;
		default:
			break;
	}
	return "";
}

bool VisualShaderNodeParameterRef::is_shader_valid() const {
	return shader_rid.is_valid();
}

void VisualShaderNodeParameterRef::set_shader_rid(const RID &p_shader_rid) {
	shader_rid = p_shader_rid;
}

void VisualShaderNodeParameterRef::set_parameter_name(const String &p_name) {
	parameter_name = p_name;
	if (shader_rid.is_valid()) {
		update_parameter_type();
	}
	emit_changed();
}

void VisualShaderNodeParameterRef::update_parameter_type() {
	if (parameter_name != "[None]") {
		param_type = get_parameter_type_by_name(parameter_name);
	} else {
		param_type = PARAMETER_TYPE_FLOAT;
	}
}

String VisualShaderNodeParameterRef::get_parameter_name() const {
	return parameter_name;
}

int VisualShaderNodeParameterRef::get_parameters_count() const {
	ERR_FAIL_COND_V(!shader_rid.is_valid(), 0);

	return parameters[shader_rid].size();
}

String VisualShaderNodeParameterRef::get_parameter_name_by_index(int p_idx) const {
	ERR_FAIL_COND_V(!shader_rid.is_valid(), String());

	if (p_idx >= 0 && p_idx < parameters[shader_rid].size()) {
		return parameters[shader_rid].get(p_idx).name;
	}
	return "";
}

VisualShaderNodeParameterRef::ParameterType VisualShaderNodeParameterRef::get_parameter_type_by_name(const String &p_name) const {
	ERR_FAIL_COND_V(!shader_rid.is_valid(), PARAMETER_TYPE_FLOAT);

	for (const VisualShaderNodeParameterRef::Parameter &parameter : parameters[shader_rid]) {
		if (parameter.name == p_name) {
			return parameter.type;
		}
	}
	return PARAMETER_TYPE_FLOAT;
}

VisualShaderNodeParameterRef::ParameterType VisualShaderNodeParameterRef::get_parameter_type_by_index(int p_idx) const {
	ERR_FAIL_COND_V(!shader_rid.is_valid(), PARAMETER_TYPE_FLOAT);

	if (p_idx >= 0 && p_idx < parameters[shader_rid].size()) {
		return parameters[shader_rid].get(p_idx).type;
	}
	return PARAMETER_TYPE_FLOAT;
}

VisualShaderNodeParameterRef::PortType VisualShaderNodeParameterRef::get_port_type_by_index(int p_idx) const {
	ERR_FAIL_COND_V(!shader_rid.is_valid(), PORT_TYPE_SCALAR);

	if (p_idx >= 0 && p_idx < parameters[shader_rid].size()) {
		switch (parameters[shader_rid].get(p_idx).type) {
			case PARAMETER_TYPE_FLOAT:
				return PORT_TYPE_SCALAR;
			case PARAMETER_TYPE_INT:
				return PORT_TYPE_SCALAR_INT;
			case PARAMETER_TYPE_UINT:
				return PORT_TYPE_SCALAR_UINT;
			case UNIFORM_TYPE_SAMPLER:
				return PORT_TYPE_SAMPLER;
			case PARAMETER_TYPE_VECTOR2:
				return PORT_TYPE_VECTOR_2D;
			case PARAMETER_TYPE_VECTOR3:
				return PORT_TYPE_VECTOR_3D;
			case PARAMETER_TYPE_VECTOR4:
				return PORT_TYPE_VECTOR_4D;
			case PARAMETER_TYPE_TRANSFORM:
				return PORT_TYPE_TRANSFORM;
			case PARAMETER_TYPE_COLOR:
				return PORT_TYPE_VECTOR_3D;
			default:
				break;
		}
	}
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeParameterRef::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	switch (param_type) {
		case PARAMETER_TYPE_FLOAT:
			if (parameter_name == "[None]") {
				return "	" + p_output_vars[0] + " = 0.0;\n";
			}
			break;
		case PARAMETER_TYPE_COLOR: {
			String code = "	" + p_output_vars[0] + " = " + get_parameter_name() + ".rgb;\n";
			code += "	" + p_output_vars[1] + " = " + get_parameter_name() + ".a;\n";
			return code;
		} break;
		case UNIFORM_TYPE_SAMPLER:
			return String();
		default:
			break;
	}

	return "	" + p_output_vars[0] + " = " + get_parameter_name() + ";\n";
}

void VisualShaderNodeParameterRef::_set_parameter_type(int p_type) {
	param_type = (ParameterType)p_type;
}

int VisualShaderNodeParameterRef::_get_parameter_type() const {
	return (int)param_type;
}

void VisualShaderNodeParameterRef::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_parameter_name", "name"), &VisualShaderNodeParameterRef::set_parameter_name);
	ClassDB::bind_method(D_METHOD("get_parameter_name"), &VisualShaderNodeParameterRef::get_parameter_name);

	ClassDB::bind_method(D_METHOD("_set_parameter_type", "type"), &VisualShaderNodeParameterRef::_set_parameter_type);
	ClassDB::bind_method(D_METHOD("_get_parameter_type"), &VisualShaderNodeParameterRef::_get_parameter_type);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "parameter_name", PROPERTY_HINT_ENUM, ""), "set_parameter_name", "get_parameter_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "param_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_parameter_type", "_get_parameter_type");
}

Vector<StringName> VisualShaderNodeParameterRef::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("parameter_name");
	props.push_back("param_type");
	return props;
}

VisualShaderNodeParameterRef::VisualShaderNodeParameterRef() {
}

////////////////////////////////////////////

const VisualShaderNodeOutput::Port VisualShaderNodeOutput::ports[] = {
	////////////////////////////////////////////////////////////////////////
	// Spatial.
	////////////////////////////////////////////////////////////////////////
	// Spatial, Vertex.
	////////////////////////////////////////////////////////////////////////
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Vertex", "VERTEX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Normal", "NORMAL" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Tangent", "TANGENT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Binormal", "BINORMAL" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "UV", "UV" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "UV2", "UV2" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Color", "COLOR.rgb" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "Alpha", "COLOR.a" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "Roughness", "ROUGHNESS" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "Point Size", "POINT_SIZE" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "Model View Matrix", "MODELVIEW_MATRIX" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_TRANSFORM, "Projection Matrix", "PROJECTION_MATRIX" },
	////////////////////////////////////////////////////////////////////////
	// Spatial, Fragment.
	////////////////////////////////////////////////////////////////////////
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Albedo", "ALBEDO" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Alpha", "ALPHA" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Metallic", "METALLIC" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Roughness", "ROUGHNESS" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Specular", "SPECULAR" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Emission", "EMISSION" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "AO", "AO" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "AO Light Affect", "AO_LIGHT_AFFECT" },

	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Normal", "NORMAL" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Normal Map", "NORMAL_MAP" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Normal Map Depth", "NORMAL_MAP_DEPTH" },

	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Rim", "RIM" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Rim Tint", "RIM_TINT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Clearcoat", "CLEARCOAT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Clearcoat Roughness", "CLEARCOAT_ROUGHNESS" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Anisotropy", "ANISOTROPY" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "Anisotropy Flow", "ANISOTROPY_FLOW" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Subsurf Scatter", "SSS_STRENGTH" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Backlight", "BACKLIGHT" },

	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Alpha Scissor Threshold", "ALPHA_SCISSOR_THRESHOLD" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Alpha Hash Scale", "ALPHA_HASH_SCALE" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Alpha AA Edge", "ALPHA_ANTIALIASING_EDGE" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "Alpha UV", "ALPHA_TEXTURE_COORDINATE" },

	{ Shader::MODE_SPATIAL, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Depth", "DEPTH" },

	////////////////////////////////////////////////////////////////////////
	// Spatial, Light.
	////////////////////////////////////////////////////////////////////////
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Diffuse", "DIFFUSE_LIGHT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Specular", "SPECULAR_LIGHT" },
	{ Shader::MODE_SPATIAL, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "Alpha", "ALPHA" },

	////////////////////////////////////////////////////////////////////////
	// Canvas Item.
	////////////////////////////////////////////////////////////////////////
	// Canvas Item, Vertex.
	////////////////////////////////////////////////////////////////////////
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "Vertex", "VERTEX" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_2D, "UV", "UV" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Color", "COLOR.rgb" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "Alpha", "COLOR.a" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_VERTEX, VisualShaderNode::PORT_TYPE_SCALAR, "Point Size", "POINT_SIZE" },
	////////////////////////////////////////////////////////////////////////
	// Canvas Item, Fragment.
	////////////////////////////////////////////////////////////////////////
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Color", "COLOR.rgb" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Alpha", "COLOR.a" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Normal", "NORMAL" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Normal Map", "NORMAL_MAP" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_SCALAR, "Normal Map Depth", "NORMAL_MAP_DEPTH" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Light Vertex", "LIGHT_VERTEX" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_FRAGMENT, VisualShaderNode::PORT_TYPE_VECTOR_2D, "Shadow Vertex", "SHADOW_VERTEX" },
	////////////////////////////////////////////////////////////////////////
	// Canvas Item, Light.
	////////////////////////////////////////////////////////////////////////
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Light", "LIGHT.rgb" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_SCALAR, "Light Alpha", "LIGHT.a" },
	{ Shader::MODE_CANVAS_ITEM, VisualShader::TYPE_LIGHT, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Shadow Modulate", "SHADOW_MODULATE.rgb" },

	////////////////////////////////////////////////////////////////////////
	// Sky, Sky.
	////////////////////////////////////////////////////////////////////////
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Color", "COLOR" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_SCALAR, "Alpha", "ALPHA" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Fog", "FOG.rgb" },
	{ Shader::MODE_SKY, VisualShader::TYPE_SKY, VisualShaderNode::PORT_TYPE_SCALAR, "Fog Alpha", "FOG.a" },

	////////////////////////////////////////////////////////////////////////
	// Fog, Fog.
	////////////////////////////////////////////////////////////////////////
	{ Shader::MODE_FOG, VisualShader::TYPE_FOG, VisualShaderNode::PORT_TYPE_SCALAR, "Density", "DENSITY" },
	{ Shader::MODE_FOG, VisualShader::TYPE_FOG, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Albedo", "ALBEDO" },
	{ Shader::MODE_FOG, VisualShader::TYPE_FOG, VisualShaderNode::PORT_TYPE_VECTOR_3D, "Emission", "EMISSION" },

	////////////////////////////////////////////////////////////////////////
	{ Shader::MODE_MAX, VisualShader::TYPE_MAX, VisualShaderNode::PORT_TYPE_TRANSFORM, nullptr, nullptr },
};

int VisualShaderNodeOutput::get_input_port_count() const {
	int idx = 0;
	int count = 0;

	while (ports[idx].mode != Shader::MODE_MAX) {
		if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
			count++;
		}
		idx++;
	}

	return count;
}

VisualShaderNodeOutput::PortType VisualShaderNodeOutput::get_input_port_type(int p_port) const {
	int idx = 0;
	int count = 0;

	while (ports[idx].mode != Shader::MODE_MAX) {
		if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
			if (count == p_port) {
				return ports[idx].type;
			}
			count++;
		}
		idx++;
	}

	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeOutput::get_input_port_name(int p_port) const {
	int idx = 0;
	int count = 0;

	while (ports[idx].mode != Shader::MODE_MAX) {
		if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
			if (count == p_port) {
				return String(ports[idx].name);
			}
			count++;
		}
		idx++;
	}

	return String();
}

Variant VisualShaderNodeOutput::get_input_port_default_value(int p_port) const {
	return Variant();
}

int VisualShaderNodeOutput::get_output_port_count() const {
	return 0;
}

VisualShaderNodeOutput::PortType VisualShaderNodeOutput::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeOutput::get_output_port_name(int p_port) const {
	return String();
}

bool VisualShaderNodeOutput::is_port_separator(int p_index) const {
	if (shader_mode == Shader::MODE_SPATIAL && shader_type == VisualShader::TYPE_VERTEX) {
		String port_name = get_input_port_name(p_index);
		return bool(port_name == "Model View Matrix");
	}
	if (shader_mode == Shader::MODE_SPATIAL && shader_type == VisualShader::TYPE_FRAGMENT) {
		String port_name = get_input_port_name(p_index);
		return bool(port_name == "AO" || port_name == "Normal" || port_name == "Rim" || port_name == "Clearcoat" || port_name == "Anisotropy" || port_name == "Subsurf Scatter" || port_name == "Alpha Scissor Threshold" || port_name == "Depth");
	}
	return false;
}

String VisualShaderNodeOutput::get_caption() const {
	return "Output";
}

String VisualShaderNodeOutput::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	int idx = 0;
	int count = 0;

	String shader_code;
	while (ports[idx].mode != Shader::MODE_MAX) {
		if (ports[idx].mode == shader_mode && ports[idx].shader_type == shader_type) {
			if (!p_input_vars[count].is_empty()) {
				String s = ports[idx].string;
				if (s.contains_char(':')) {
					shader_code += "	" + s.get_slicec(':', 0) + " = " + p_input_vars[count] + "." + s.get_slicec(':', 1) + ";\n";
				} else {
					shader_code += "	" + s + " = " + p_input_vars[count] + ";\n";
				}
			}
			count++;
		}
		idx++;
	}

	return shader_code;
}

VisualShaderNodeOutput::VisualShaderNodeOutput() {
}

///////////////////////////

void VisualShaderNodeParameter::set_parameter_name(const String &p_name) {
	parameter_name = p_name;
	emit_signal(SNAME("name_changed"));
	emit_changed();
}

String VisualShaderNodeParameter::get_parameter_name() const {
	return parameter_name;
}

void VisualShaderNodeParameter::set_qualifier(VisualShaderNodeParameter::Qualifier p_qual) {
	ERR_FAIL_INDEX(int(p_qual), int(QUAL_MAX));
	if (qualifier == p_qual) {
		return;
	}
	qualifier = p_qual;
	emit_changed();
}

VisualShaderNodeParameter::Qualifier VisualShaderNodeParameter::get_qualifier() const {
	return qualifier;
}

void VisualShaderNodeParameter::set_global_code_generated(bool p_enabled) {
	global_code_generated = p_enabled;
}

bool VisualShaderNodeParameter::is_global_code_generated() const {
	return global_code_generated;
}

#ifndef DISABLE_DEPRECATED
// Kept for compatibility from 3.x to 4.0.
bool VisualShaderNodeParameter::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "uniform_name") {
		set_parameter_name(p_value);
		return true;
	}
	return false;
}
#endif

void VisualShaderNodeParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_parameter_name", "name"), &VisualShaderNodeParameter::set_parameter_name);
	ClassDB::bind_method(D_METHOD("get_parameter_name"), &VisualShaderNodeParameter::get_parameter_name);

	ClassDB::bind_method(D_METHOD("set_qualifier", "qualifier"), &VisualShaderNodeParameter::set_qualifier);
	ClassDB::bind_method(D_METHOD("get_qualifier"), &VisualShaderNodeParameter::get_qualifier);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "parameter_name"), "set_parameter_name", "get_parameter_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "qualifier", PROPERTY_HINT_ENUM, "None,Global,Instance"), "set_qualifier", "get_qualifier");

	BIND_ENUM_CONSTANT(QUAL_NONE);
	BIND_ENUM_CONSTANT(QUAL_GLOBAL);
	BIND_ENUM_CONSTANT(QUAL_INSTANCE);
	BIND_ENUM_CONSTANT(QUAL_MAX);
}

String VisualShaderNodeParameter::_get_qual_str() const {
	if (is_qualifier_supported(qualifier)) {
		switch (qualifier) {
			case QUAL_NONE:
				break;
			case QUAL_GLOBAL:
				return "global ";
			case QUAL_INSTANCE:
				return "instance ";
			default:
				break;
		}
	}
	return String();
}

String VisualShaderNodeParameter::get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const {
	List<String> keyword_list;
	ShaderLanguage::get_keyword_list(&keyword_list);
	if (keyword_list.find(parameter_name)) {
		return RTR("Shader keywords cannot be used as parameter names.\nChoose another name.");
	}
	if (!is_qualifier_supported(qualifier)) {
		String qualifier_str;
		switch (qualifier) {
			case QUAL_NONE:
				break;
			case QUAL_GLOBAL:
				qualifier_str = "global";
				break;
			case QUAL_INSTANCE:
				qualifier_str = "instance";
				break;
			default:
				break;
		}
		return vformat(RTR("This parameter type does not support the '%s' qualifier."), qualifier_str);
	} else if (qualifier == Qualifier::QUAL_GLOBAL) {
		RS::GlobalShaderParameterType gvt = RS::get_singleton()->global_shader_parameter_get_type(parameter_name);
		if (gvt == RS::GLOBAL_VAR_TYPE_MAX) {
			return vformat(RTR("Global parameter '%s' does not exist.\nCreate it in the Project Settings."), parameter_name);
		}
		bool incompatible_type = false;
		switch (gvt) {
			case RS::GLOBAL_VAR_TYPE_FLOAT: {
				if (!Object::cast_to<VisualShaderNodeFloatParameter>(this)) {
					incompatible_type = true;
				}
			} break;
			case RS::GLOBAL_VAR_TYPE_INT: {
				if (!Object::cast_to<VisualShaderNodeIntParameter>(this)) {
					incompatible_type = true;
				}
			} break;
			case RS::GLOBAL_VAR_TYPE_BOOL: {
				if (!Object::cast_to<VisualShaderNodeBooleanParameter>(this)) {
					incompatible_type = true;
				}
			} break;
			case RS::GLOBAL_VAR_TYPE_COLOR: {
				if (!Object::cast_to<VisualShaderNodeColorParameter>(this)) {
					incompatible_type = true;
				}
			} break;
			case RS::GLOBAL_VAR_TYPE_VEC3: {
				if (!Object::cast_to<VisualShaderNodeVec3Parameter>(this)) {
					incompatible_type = true;
				}
			} break;
			case RS::GLOBAL_VAR_TYPE_VEC4: {
				if (!Object::cast_to<VisualShaderNodeVec4Parameter>(this)) {
					incompatible_type = true;
				}
			} break;
			case RS::GLOBAL_VAR_TYPE_TRANSFORM: {
				if (!Object::cast_to<VisualShaderNodeTransformParameter>(this)) {
					incompatible_type = true;
				}
			} break;
			case RS::GLOBAL_VAR_TYPE_SAMPLER2D: {
				if (!Object::cast_to<VisualShaderNodeTextureParameter>(this)) {
					incompatible_type = true;
				}
			} break;
			case RS::GLOBAL_VAR_TYPE_SAMPLER3D: {
				if (!Object::cast_to<VisualShaderNodeTexture3DParameter>(this)) {
					incompatible_type = true;
				}
			} break;
			case RS::GLOBAL_VAR_TYPE_SAMPLER2DARRAY: {
				if (!Object::cast_to<VisualShaderNodeTexture2DArrayParameter>(this)) {
					incompatible_type = true;
				}
			} break;
			case RS::GLOBAL_VAR_TYPE_SAMPLERCUBE: {
				if (!Object::cast_to<VisualShaderNodeCubemapParameter>(this)) {
					incompatible_type = true;
				}
			} break;
			default:
				break;
		}
		if (incompatible_type) {
			return vformat(RTR("Global parameter '%s' has an incompatible type for this kind of node.\nChange it in the Project Settings."), parameter_name);
		}
	}

	return String();
}

Vector<StringName> VisualShaderNodeParameter::get_editable_properties() const {
	Vector<StringName> props;
	props.push_back("qualifier");
	return props;
}

VisualShaderNodeParameter::VisualShaderNodeParameter() {
}

////////////// ResizeableBase

void VisualShaderNodeResizableBase::set_size(const Size2 &p_size) {
	size = p_size;
}

Size2 VisualShaderNodeResizableBase::get_size() const {
	return size;
}

void VisualShaderNodeResizableBase::set_allow_v_resize(bool p_enabled) {
	allow_v_resize = p_enabled;
}

bool VisualShaderNodeResizableBase::is_allow_v_resize() const {
	return allow_v_resize;
}

void VisualShaderNodeResizableBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &VisualShaderNodeResizableBase::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &VisualShaderNodeResizableBase::get_size);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "size"), "set_size", "get_size");
}

VisualShaderNodeResizableBase::VisualShaderNodeResizableBase() {
	set_allow_v_resize(true);
}

////////////// Frame

String VisualShaderNodeFrame::get_caption() const {
	return title;
}

int VisualShaderNodeFrame::get_input_port_count() const {
	return 0;
}

VisualShaderNodeFrame::PortType VisualShaderNodeFrame::get_input_port_type(int p_port) const {
	return PortType::PORT_TYPE_SCALAR;
}

String VisualShaderNodeFrame::get_input_port_name(int p_port) const {
	return String();
}

int VisualShaderNodeFrame::get_output_port_count() const {
	return 0;
}

VisualShaderNodeFrame::PortType VisualShaderNodeFrame::get_output_port_type(int p_port) const {
	return PortType::PORT_TYPE_SCALAR;
}

String VisualShaderNodeFrame::get_output_port_name(int p_port) const {
	return String();
}

void VisualShaderNodeFrame::set_title(const String &p_title) {
	title = p_title;
}

String VisualShaderNodeFrame::get_title() const {
	return title;
}

void VisualShaderNodeFrame::set_tint_color_enabled(bool p_enabled) {
	tint_color_enabled = p_enabled;
}

bool VisualShaderNodeFrame::is_tint_color_enabled() const {
	return tint_color_enabled;
}

void VisualShaderNodeFrame::set_tint_color(const Color &p_color) {
	tint_color = p_color;
}

Color VisualShaderNodeFrame::get_tint_color() const {
	return tint_color;
}

void VisualShaderNodeFrame::set_autoshrink_enabled(bool p_enable) {
	autoshrink = p_enable;
}

bool VisualShaderNodeFrame::is_autoshrink_enabled() const {
	return autoshrink;
}

String VisualShaderNodeFrame::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return String();
}

void VisualShaderNodeFrame::add_attached_node(int p_node) {
	attached_nodes.insert(p_node);
}

void VisualShaderNodeFrame::remove_attached_node(int p_node) {
	attached_nodes.erase(p_node);
}

void VisualShaderNodeFrame::set_attached_nodes(const PackedInt32Array &p_attached_nodes) {
	attached_nodes.clear();
	for (const int &node_id : p_attached_nodes) {
		attached_nodes.insert(node_id);
	}
}

PackedInt32Array VisualShaderNodeFrame::get_attached_nodes() const {
	PackedInt32Array attached_nodes_arr;
	for (const int &node_id : attached_nodes) {
		attached_nodes_arr.push_back(node_id);
	}
	return attached_nodes_arr;
}

void VisualShaderNodeFrame::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_title", "title"), &VisualShaderNodeFrame::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &VisualShaderNodeFrame::get_title);

	ClassDB::bind_method(D_METHOD("set_tint_color_enabled", "enable"), &VisualShaderNodeFrame::set_tint_color_enabled);
	ClassDB::bind_method(D_METHOD("is_tint_color_enabled"), &VisualShaderNodeFrame::is_tint_color_enabled);

	ClassDB::bind_method(D_METHOD("set_tint_color", "color"), &VisualShaderNodeFrame::set_tint_color);
	ClassDB::bind_method(D_METHOD("get_tint_color"), &VisualShaderNodeFrame::get_tint_color);

	ClassDB::bind_method(D_METHOD("set_autoshrink_enabled", "enable"), &VisualShaderNodeFrame::set_autoshrink_enabled);
	ClassDB::bind_method(D_METHOD("is_autoshrink_enabled"), &VisualShaderNodeFrame::is_autoshrink_enabled);

	ClassDB::bind_method(D_METHOD("add_attached_node", "node"), &VisualShaderNodeFrame::add_attached_node);
	ClassDB::bind_method(D_METHOD("remove_attached_node", "node"), &VisualShaderNodeFrame::remove_attached_node);
	ClassDB::bind_method(D_METHOD("set_attached_nodes", "attached_nodes"), &VisualShaderNodeFrame::set_attached_nodes);
	ClassDB::bind_method(D_METHOD("get_attached_nodes"), &VisualShaderNodeFrame::get_attached_nodes);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "tint_color_enabled"), "set_tint_color_enabled", "is_tint_color_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "tint_color"), "set_tint_color", "get_tint_color");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoshrink"), "set_autoshrink_enabled", "is_autoshrink_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "attached_nodes", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR), "set_attached_nodes", "get_attached_nodes");
}

VisualShaderNodeFrame::VisualShaderNodeFrame() {
}

////////////// Comment (Deprecated)

#ifndef DISABLE_DEPRECATED
void VisualShaderNodeComment::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_description", "description"), &VisualShaderNodeComment::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &VisualShaderNodeComment::get_description);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description"), "set_description", "get_description");
}

void VisualShaderNodeComment::set_description(const String &p_description) {
	description = p_description;
}

String VisualShaderNodeComment::get_description() const {
	return description;
}
#endif

////////////// GroupBase

void VisualShaderNodeGroupBase::set_inputs(const String &p_inputs) {
	if (inputs == p_inputs) {
		return;
	}

	clear_input_ports();

	inputs = p_inputs;

	Vector<String> input_strings = inputs.split(";", false);

	int input_port_count = input_strings.size();

	for (int i = 0; i < input_port_count; i++) {
		Vector<String> arr = input_strings[i].split(",");
		ERR_FAIL_COND(arr.size() != 3);

		int port_idx = arr[0].to_int();
		int port_type = arr[1].to_int();
		String port_name = arr[2];

		Port port;
		port.type = (PortType)port_type;
		port.name = port_name;
		input_ports[port_idx] = port;
	}
}

String VisualShaderNodeGroupBase::get_inputs() const {
	return inputs;
}

void VisualShaderNodeGroupBase::set_outputs(const String &p_outputs) {
	if (outputs == p_outputs) {
		return;
	}

	clear_output_ports();

	outputs = p_outputs;

	Vector<String> output_strings = outputs.split(";", false);

	int output_port_count = output_strings.size();

	for (int i = 0; i < output_port_count; i++) {
		Vector<String> arr = output_strings[i].split(",");
		ERR_FAIL_COND(arr.size() != 3);

		int port_idx = arr[0].to_int();
		int port_type = arr[1].to_int();
		String port_name = arr[2];

		Port port;
		port.type = (PortType)port_type;
		port.name = port_name;
		output_ports[port_idx] = port;
	}
}

String VisualShaderNodeGroupBase::get_outputs() const {
	return outputs;
}

bool VisualShaderNodeGroupBase::is_valid_port_name(const String &p_name) const {
	if (!p_name.is_valid_ascii_identifier()) {
		return false;
	}
	for (int i = 0; i < get_input_port_count(); i++) {
		if (get_input_port_name(i) == p_name) {
			return false;
		}
	}
	for (int i = 0; i < get_output_port_count(); i++) {
		if (get_output_port_name(i) == p_name) {
			return false;
		}
	}
	return true;
}

void VisualShaderNodeGroupBase::add_input_port(int p_id, int p_type, const String &p_name) {
	ERR_FAIL_INDEX(p_type, int(PORT_TYPE_MAX));
	ERR_FAIL_COND(!is_valid_port_name(p_name));

	String str = itos(p_id) + "," + itos(p_type) + "," + p_name + ";";
	Vector<String> inputs_strings = inputs.split(";", false);
	int index = 0;
	if (p_id < inputs_strings.size()) {
		for (int i = 0; i < inputs_strings.size(); i++) {
			if (i == p_id) {
				inputs = inputs.insert(index, str);
				break;
			}
			index += inputs_strings[i].size();
		}
	} else {
		inputs += str;
	}

	inputs_strings = inputs.split(";", false);
	index = 0;

	for (int i = 0; i < inputs_strings.size(); i++) {
		int count = 0;
		for (int j = 0; j < inputs_strings[i].size(); j++) {
			if (inputs_strings[i][j] == ',') {
				break;
			}
			count++;
		}

		inputs = inputs.left(index) + inputs.substr(index + count);
		inputs = inputs.insert(index, itos(i));
		index += inputs_strings[i].size();
	}

	_apply_port_changes();
	emit_changed();
}

void VisualShaderNodeGroupBase::remove_input_port(int p_id) {
	ERR_FAIL_COND(!has_input_port(p_id));

	Vector<String> inputs_strings = inputs.split(";", false);
	int count = 0;
	int index = 0;
	for (int i = 0; i < inputs_strings.size(); i++) {
		Vector<String> arr = inputs_strings[i].split(",");
		if (arr[0].to_int() == p_id) {
			count = inputs_strings[i].size();
			break;
		}
		index += inputs_strings[i].size();
	}
	inputs = inputs.left(index) + inputs.substr(index + count);

	inputs_strings = inputs.split(";", false);
	inputs = inputs.substr(0, index);

	for (int i = p_id; i < inputs_strings.size(); i++) {
		inputs += inputs_strings[i].replace_first(inputs_strings[i].split(",")[0], itos(i)) + ";";
	}

	_apply_port_changes();
	emit_changed();
}

int VisualShaderNodeGroupBase::get_input_port_count() const {
	return input_ports.size();
}

bool VisualShaderNodeGroupBase::has_input_port(int p_id) const {
	return input_ports.has(p_id);
}

void VisualShaderNodeGroupBase::add_output_port(int p_id, int p_type, const String &p_name) {
	ERR_FAIL_INDEX(p_type, int(PORT_TYPE_MAX));
	ERR_FAIL_COND(!is_valid_port_name(p_name));

	String str = itos(p_id) + "," + itos(p_type) + "," + p_name + ";";
	Vector<String> outputs_strings = outputs.split(";", false);
	int index = 0;
	if (p_id < outputs_strings.size()) {
		for (int i = 0; i < outputs_strings.size(); i++) {
			if (i == p_id) {
				outputs = outputs.insert(index, str);
				break;
			}
			index += outputs_strings[i].size();
		}
	} else {
		outputs += str;
	}

	outputs_strings = outputs.split(";", false);
	index = 0;

	for (int i = 0; i < outputs_strings.size(); i++) {
		int count = 0;
		for (int j = 0; j < outputs_strings[i].size(); j++) {
			if (outputs_strings[i][j] == ',') {
				break;
			}
			count++;
		}

		outputs = outputs.left(index) + outputs.substr(index + count);
		outputs = outputs.insert(index, itos(i));
		index += outputs_strings[i].size();
	}

	_apply_port_changes();
	emit_changed();
}

void VisualShaderNodeGroupBase::remove_output_port(int p_id) {
	ERR_FAIL_COND(!has_output_port(p_id));

	Vector<String> outputs_strings = outputs.split(";", false);
	int count = 0;
	int index = 0;
	for (int i = 0; i < outputs_strings.size(); i++) {
		Vector<String> arr = outputs_strings[i].split(",");
		if (arr[0].to_int() == p_id) {
			count = outputs_strings[i].size();
			break;
		}
		index += outputs_strings[i].size();
	}
	outputs = outputs.left(index) + outputs.substr(index + count);

	outputs_strings = outputs.split(";", false);
	outputs = outputs.substr(0, index);

	for (int i = p_id; i < outputs_strings.size(); i++) {
		outputs += outputs_strings[i].replace_first(outputs_strings[i].split(",")[0], itos(i)) + ";";
	}

	_apply_port_changes();
	emit_changed();
}

int VisualShaderNodeGroupBase::get_output_port_count() const {
	return output_ports.size();
}

bool VisualShaderNodeGroupBase::has_output_port(int p_id) const {
	return output_ports.has(p_id);
}

void VisualShaderNodeGroupBase::clear_input_ports() {
	input_ports.clear();
}

void VisualShaderNodeGroupBase::clear_output_ports() {
	output_ports.clear();
}

void VisualShaderNodeGroupBase::set_input_port_type(int p_id, int p_type) {
	ERR_FAIL_COND(!has_input_port(p_id));
	ERR_FAIL_INDEX(p_type, int(PORT_TYPE_MAX));

	if (input_ports[p_id].type == p_type) {
		return;
	}

	Vector<String> inputs_strings = inputs.split(";", false);
	int count = 0;
	int index = 0;
	for (int i = 0; i < inputs_strings.size(); i++) {
		Vector<String> arr = inputs_strings[i].split(",");
		ERR_FAIL_COND(arr.size() != 3);

		if (arr[0].to_int() == p_id) {
			index += arr[0].size();
			count = arr[1].size() - 1;
			break;
		}
		index += inputs_strings[i].size();
	}

	inputs = inputs.left(index) + inputs.substr(index + count);
	inputs = inputs.insert(index, itos(p_type));

	_apply_port_changes();
	emit_changed();
}

VisualShaderNodeGroupBase::PortType VisualShaderNodeGroupBase::get_input_port_type(int p_id) const {
	ERR_FAIL_COND_V(!input_ports.has(p_id), (PortType)0);
	return input_ports[p_id].type;
}

void VisualShaderNodeGroupBase::set_input_port_name(int p_id, const String &p_name) {
	ERR_FAIL_COND(!has_input_port(p_id));
	ERR_FAIL_COND(!is_valid_port_name(p_name));

	if (input_ports[p_id].name == p_name) {
		return;
	}

	Vector<String> inputs_strings = inputs.split(";", false);
	int count = 0;
	int index = 0;
	for (int i = 0; i < inputs_strings.size(); i++) {
		Vector<String> arr = inputs_strings[i].split(",");
		ERR_FAIL_COND(arr.size() != 3);

		if (arr[0].to_int() == p_id) {
			index += arr[0].size() + arr[1].size();
			count = arr[2].size() - 1;
			break;
		}
		index += inputs_strings[i].size();
	}

	inputs = inputs.left(index) + inputs.substr(index + count);
	inputs = inputs.insert(index, p_name);

	_apply_port_changes();
	emit_changed();
}

String VisualShaderNodeGroupBase::get_input_port_name(int p_id) const {
	ERR_FAIL_COND_V(!input_ports.has(p_id), "");
	return input_ports[p_id].name;
}

void VisualShaderNodeGroupBase::set_output_port_type(int p_id, int p_type) {
	ERR_FAIL_COND(!has_output_port(p_id));
	ERR_FAIL_INDEX(p_type, int(PORT_TYPE_MAX));

	if (output_ports[p_id].type == p_type) {
		return;
	}

	Vector<String> output_strings = outputs.split(";", false);
	int count = 0;
	int index = 0;
	for (int i = 0; i < output_strings.size(); i++) {
		Vector<String> arr = output_strings[i].split(",");
		ERR_FAIL_COND(arr.size() != 3);

		if (arr[0].to_int() == p_id) {
			index += arr[0].size();
			count = arr[1].size() - 1;
			break;
		}
		index += output_strings[i].size();
	}

	outputs = outputs.left(index) + outputs.substr(index + count);

	outputs = outputs.insert(index, itos(p_type));

	_apply_port_changes();
	emit_changed();
}

VisualShaderNodeGroupBase::PortType VisualShaderNodeGroupBase::get_output_port_type(int p_id) const {
	ERR_FAIL_COND_V(!output_ports.has(p_id), (PortType)0);
	return output_ports[p_id].type;
}

void VisualShaderNodeGroupBase::set_output_port_name(int p_id, const String &p_name) {
	ERR_FAIL_COND(!has_output_port(p_id));
	ERR_FAIL_COND(!is_valid_port_name(p_name));

	if (output_ports[p_id].name == p_name) {
		return;
	}

	Vector<String> output_strings = outputs.split(";", false);
	int count = 0;
	int index = 0;
	for (int i = 0; i < output_strings.size(); i++) {
		Vector<String> arr = output_strings[i].split(",");
		ERR_FAIL_COND(arr.size() != 3);

		if (arr[0].to_int() == p_id) {
			index += arr[0].size() + arr[1].size();
			count = arr[2].size() - 1;
			break;
		}
		index += output_strings[i].size();
	}

	outputs = outputs.left(index) + outputs.substr(index + count);

	outputs = outputs.insert(index, p_name);

	_apply_port_changes();
	emit_changed();
}

String VisualShaderNodeGroupBase::get_output_port_name(int p_id) const {
	ERR_FAIL_COND_V(!output_ports.has(p_id), "");
	return output_ports[p_id].name;
}

int VisualShaderNodeGroupBase::get_free_input_port_id() const {
	return input_ports.size();
}

int VisualShaderNodeGroupBase::get_free_output_port_id() const {
	return output_ports.size();
}

void VisualShaderNodeGroupBase::set_ctrl_pressed(Control *p_control, int p_index) {
	controls[p_index] = p_control;
}

Control *VisualShaderNodeGroupBase::is_ctrl_pressed(int p_index) {
	ERR_FAIL_COND_V(!controls.has(p_index), nullptr);
	return controls[p_index];
}

void VisualShaderNodeGroupBase::_apply_port_changes() {
	Vector<String> inputs_strings = inputs.split(";", false);
	Vector<String> outputs_strings = outputs.split(";", false);

	clear_input_ports();
	clear_output_ports();

	for (int i = 0; i < inputs_strings.size(); i++) {
		Vector<String> arr = inputs_strings[i].split(",");
		ERR_FAIL_COND(arr.size() != 3);

		Port port;
		port.type = (PortType)arr[1].to_int();
		port.name = arr[2];
		input_ports[i] = port;
	}
	for (int i = 0; i < outputs_strings.size(); i++) {
		Vector<String> arr = outputs_strings[i].split(",");
		ERR_FAIL_COND(arr.size() != 3);

		Port port;
		port.type = (PortType)arr[1].to_int();
		port.name = arr[2];
		output_ports[i] = port;
	}
}

void VisualShaderNodeGroupBase::set_editable(bool p_enabled) {
	editable = p_enabled;
}

bool VisualShaderNodeGroupBase::is_editable() const {
	return editable;
}

void VisualShaderNodeGroupBase::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_inputs", "inputs"), &VisualShaderNodeGroupBase::set_inputs);
	ClassDB::bind_method(D_METHOD("get_inputs"), &VisualShaderNodeGroupBase::get_inputs);

	ClassDB::bind_method(D_METHOD("set_outputs", "outputs"), &VisualShaderNodeGroupBase::set_outputs);
	ClassDB::bind_method(D_METHOD("get_outputs"), &VisualShaderNodeGroupBase::get_outputs);

	ClassDB::bind_method(D_METHOD("is_valid_port_name", "name"), &VisualShaderNodeGroupBase::is_valid_port_name);

	ClassDB::bind_method(D_METHOD("add_input_port", "id", "type", "name"), &VisualShaderNodeGroupBase::add_input_port);
	ClassDB::bind_method(D_METHOD("remove_input_port", "id"), &VisualShaderNodeGroupBase::remove_input_port);
	ClassDB::bind_method(D_METHOD("get_input_port_count"), &VisualShaderNodeGroupBase::get_input_port_count);
	ClassDB::bind_method(D_METHOD("has_input_port", "id"), &VisualShaderNodeGroupBase::has_input_port);
	ClassDB::bind_method(D_METHOD("clear_input_ports"), &VisualShaderNodeGroupBase::clear_input_ports);

	ClassDB::bind_method(D_METHOD("add_output_port", "id", "type", "name"), &VisualShaderNodeGroupBase::add_output_port);
	ClassDB::bind_method(D_METHOD("remove_output_port", "id"), &VisualShaderNodeGroupBase::remove_output_port);
	ClassDB::bind_method(D_METHOD("get_output_port_count"), &VisualShaderNodeGroupBase::get_output_port_count);
	ClassDB::bind_method(D_METHOD("has_output_port", "id"), &VisualShaderNodeGroupBase::has_output_port);
	ClassDB::bind_method(D_METHOD("clear_output_ports"), &VisualShaderNodeGroupBase::clear_output_ports);

	ClassDB::bind_method(D_METHOD("set_input_port_name", "id", "name"), &VisualShaderNodeGroupBase::set_input_port_name);
	ClassDB::bind_method(D_METHOD("set_input_port_type", "id", "type"), &VisualShaderNodeGroupBase::set_input_port_type);
	ClassDB::bind_method(D_METHOD("set_output_port_name", "id", "name"), &VisualShaderNodeGroupBase::set_output_port_name);
	ClassDB::bind_method(D_METHOD("set_output_port_type", "id", "type"), &VisualShaderNodeGroupBase::set_output_port_type);

	ClassDB::bind_method(D_METHOD("get_free_input_port_id"), &VisualShaderNodeGroupBase::get_free_input_port_id);
	ClassDB::bind_method(D_METHOD("get_free_output_port_id"), &VisualShaderNodeGroupBase::get_free_output_port_id);
}

String VisualShaderNodeGroupBase::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	return "";
}

VisualShaderNodeGroupBase::VisualShaderNodeGroupBase() {
	simple_decl = false;
}

////////////// Expression

String VisualShaderNodeExpression::get_caption() const {
	return "Expression";
}

void VisualShaderNodeExpression::set_expression(const String &p_expression) {
	expression = p_expression;
	emit_changed();
}

String VisualShaderNodeExpression::get_expression() const {
	return expression;
}

bool VisualShaderNodeExpression::_is_valid_identifier_char(char32_t p_c) const {
	return p_c == '_' || (p_c >= 'A' && p_c <= 'Z') || (p_c >= 'a' && p_c <= 'z') || (p_c >= '0' && p_c <= '9');
}

String VisualShaderNodeExpression::_replace_port_names(const Vector<Pair<String, String>> &p_pairs, const String &p_expression) const {
	String _expression = p_expression;

	for (const Pair<String, String> &pair : p_pairs) {
		String from = pair.first;
		String to = pair.second;
		int search_idx = 0;
		int len = from.length();

		while (true) {
			int index = _expression.find(from, search_idx);
			if (index == -1) {
				break;
			}

			int left_index = index - 1;
			int right_index = index + len;
			bool left_correct = left_index <= 0 || !_is_valid_identifier_char(_expression[left_index]);
			bool right_correct = right_index >= _expression.length() || !_is_valid_identifier_char(_expression[right_index]);

			if (left_correct && right_correct) {
				_expression = _expression.erase(index, len);
				_expression = _expression.insert(index, to);

				search_idx = index + to.length();
			} else {
				search_idx = index + len;
			}
		}
	}

	return _expression;
}

String VisualShaderNodeExpression::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String _expression = expression;

	_expression = _expression.insert(0, "\n");
	_expression = _expression.replace("\n", "\n		");

	Vector<Pair<String, String>> input_port_names;
	for (int i = 0; i < get_input_port_count(); i++) {
		input_port_names.push_back(Pair<String, String>(get_input_port_name(i), p_input_vars[i]));
	}
	_expression = _replace_port_names(input_port_names, _expression);

	Vector<Pair<String, String>> output_port_names;
	for (int i = 0; i < get_output_port_count(); i++) {
		output_port_names.push_back(Pair<String, String>(get_output_port_name(i), p_output_vars[i]));
	}
	_expression = _replace_port_names(output_port_names, _expression);

	String output_initializer;

	for (int i = 0; i < get_output_port_count(); i++) {
		int port_type = get_output_port_type(i);
		String tk = "";
		switch (port_type) {
			case PORT_TYPE_SCALAR:
				tk = "0.0";
				break;
			case PORT_TYPE_SCALAR_INT:
				tk = "0";
				break;
			case PORT_TYPE_VECTOR_2D:
				tk = "vec2(0.0, 0.0)";
				break;
			case PORT_TYPE_VECTOR_3D:
				tk = "vec3(0.0, 0.0, 0.0)";
				break;
			case PORT_TYPE_VECTOR_4D:
				tk = "vec4(0.0, 0.0, 0.0, 0.0)";
				break;
			case PORT_TYPE_BOOLEAN:
				tk = "false";
				break;
			case PORT_TYPE_TRANSFORM:
				tk = "mat4(1.0)";
				break;
			default:
				continue;
		}
		output_initializer += "	" + p_output_vars[i] + " = " + tk + ";\n";
	}

	String code;
	code += output_initializer;
	code += "	{";
	code += _expression;
	code += "\n	}\n";

	return code;
}

bool VisualShaderNodeExpression::is_output_port_expandable(int p_port) const {
	return false;
}

void VisualShaderNodeExpression::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_expression", "expression"), &VisualShaderNodeExpression::set_expression);
	ClassDB::bind_method(D_METHOD("get_expression"), &VisualShaderNodeExpression::get_expression);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "expression"), "set_expression", "get_expression");
}

VisualShaderNodeExpression::VisualShaderNodeExpression() {
	set_editable(true);
}

////////////// Global Expression

String VisualShaderNodeGlobalExpression::get_caption() const {
	return "GlobalExpression";
}

String VisualShaderNodeGlobalExpression::generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const {
	return expression;
}

VisualShaderNodeGlobalExpression::VisualShaderNodeGlobalExpression() {
	set_editable(false);
}

////////////// Varying

RBMap<RID, List<VisualShaderNodeVarying::Varying>> varyings;

void VisualShaderNodeVarying::add_varying(RID p_shader_rid, const String &p_name, VisualShader::VaryingMode p_mode, VisualShader::VaryingType p_type) { // static
	varyings[p_shader_rid].push_back({ p_name, p_mode, p_type });
}

void VisualShaderNodeVarying::clear_varyings(RID p_shader_rid) { // static
	varyings[p_shader_rid].clear();
}

bool VisualShaderNodeVarying::has_varying(RID p_shader_rid, const String &p_name) { // static
	for (const VisualShaderNodeVarying::Varying &E : varyings[p_shader_rid]) {
		if (E.name == p_name) {
			return true;
		}
	}
	return false;
}

void VisualShaderNodeVarying::set_shader_rid(const RID &p_shader_rid) {
	shader_rid = p_shader_rid;
}

int VisualShaderNodeVarying::get_varyings_count() const {
	return varyings[shader_rid].size();
}

String VisualShaderNodeVarying::get_varying_name_by_index(int p_idx) const {
	if (p_idx >= 0 && p_idx < varyings[shader_rid].size()) {
		return varyings[shader_rid].get(p_idx).name;
	}
	return "";
}

VisualShader::VaryingType VisualShaderNodeVarying::get_varying_type_by_name(const String &p_name) const {
	for (const VisualShaderNodeVarying::Varying &varying : varyings[shader_rid]) {
		if (varying.name == p_name) {
			return varying.type;
		}
	}
	return VisualShader::VARYING_TYPE_FLOAT;
}

VisualShader::VaryingType VisualShaderNodeVarying::get_varying_type_by_index(int p_idx) const {
	if (p_idx >= 0 && p_idx < varyings[shader_rid].size()) {
		return varyings[shader_rid].get(p_idx).type;
	}
	return VisualShader::VARYING_TYPE_FLOAT;
}

VisualShader::VaryingMode VisualShaderNodeVarying::get_varying_mode_by_name(const String &p_name) const {
	for (const VisualShaderNodeVarying::Varying &varying : varyings[shader_rid]) {
		if (varying.name == p_name) {
			return varying.mode;
		}
	}
	return VisualShader::VARYING_MODE_VERTEX_TO_FRAG_LIGHT;
}

VisualShader::VaryingMode VisualShaderNodeVarying::get_varying_mode_by_index(int p_idx) const {
	if (p_idx >= 0 && p_idx < varyings[shader_rid].size()) {
		return varyings[shader_rid].get(p_idx).mode;
	}
	return VisualShader::VARYING_MODE_VERTEX_TO_FRAG_LIGHT;
}

VisualShaderNodeVarying::PortType VisualShaderNodeVarying::get_port_type_by_index(int p_idx) const {
	if (p_idx >= 0 && p_idx < varyings[shader_rid].size()) {
		return get_port_type(varyings[shader_rid].get(p_idx).type, 0);
	}
	return PORT_TYPE_SCALAR;
}

//////////////

void VisualShaderNodeVarying::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_varying_name", "name"), &VisualShaderNodeVarying::set_varying_name);
	ClassDB::bind_method(D_METHOD("get_varying_name"), &VisualShaderNodeVarying::get_varying_name);

	ClassDB::bind_method(D_METHOD("set_varying_type", "type"), &VisualShaderNodeVarying::set_varying_type);
	ClassDB::bind_method(D_METHOD("get_varying_type"), &VisualShaderNodeVarying::get_varying_type);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "varying_name"), "set_varying_name", "get_varying_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "varying_type", PROPERTY_HINT_ENUM, "Float,Int,Vector2,Vector3,Vector4,Boolean,Transform"), "set_varying_type", "get_varying_type");
}

String VisualShaderNodeVarying::get_type_str() const {
	switch (varying_type) {
		case VisualShader::VARYING_TYPE_FLOAT:
			return "float";
		case VisualShader::VARYING_TYPE_INT:
			return "int";
		case VisualShader::VARYING_TYPE_UINT:
			return "uint";
		case VisualShader::VARYING_TYPE_VECTOR_2D:
			return "vec2";
		case VisualShader::VARYING_TYPE_VECTOR_3D:
			return "vec3";
		case VisualShader::VARYING_TYPE_VECTOR_4D:
			return "vec4";
		case VisualShader::VARYING_TYPE_BOOLEAN:
			return "bool";
		case VisualShader::VARYING_TYPE_TRANSFORM:
			return "mat4";
		default:
			break;
	}
	return "";
}

VisualShaderNodeVarying::PortType VisualShaderNodeVarying::get_port_type(VisualShader::VaryingType p_type, int p_port) const {
	switch (p_type) {
		case VisualShader::VARYING_TYPE_INT:
			return PORT_TYPE_SCALAR_INT;
		case VisualShader::VARYING_TYPE_UINT:
			return PORT_TYPE_SCALAR_UINT;
		case VisualShader::VARYING_TYPE_VECTOR_2D:
			return p_port == 0 ? PORT_TYPE_VECTOR_2D : PORT_TYPE_SCALAR;
		case VisualShader::VARYING_TYPE_VECTOR_3D:
			return p_port == 0 ? PORT_TYPE_VECTOR_3D : PORT_TYPE_SCALAR;
		case VisualShader::VARYING_TYPE_VECTOR_4D:
			return p_port == 0 ? PORT_TYPE_VECTOR_4D : PORT_TYPE_SCALAR;
		case VisualShader::VARYING_TYPE_BOOLEAN:
			return PORT_TYPE_BOOLEAN;
		case VisualShader::VARYING_TYPE_TRANSFORM:
			return PORT_TYPE_TRANSFORM;
		default:
			break;
	}
	return PORT_TYPE_SCALAR;
}

void VisualShaderNodeVarying::set_varying_name(String p_varying_name) {
	if (varying_name == p_varying_name) {
		return;
	}
	varying_name = p_varying_name;
	emit_changed();
}

String VisualShaderNodeVarying::get_varying_name() const {
	return varying_name;
}

void VisualShaderNodeVarying::set_varying_type(VisualShader::VaryingType p_varying_type) {
	ERR_FAIL_INDEX(p_varying_type, VisualShader::VARYING_TYPE_MAX);
	if (varying_type == p_varying_type) {
		return;
	}
	varying_type = p_varying_type;
	emit_changed();
}

VisualShader::VaryingType VisualShaderNodeVarying::get_varying_type() const {
	return varying_type;
}

VisualShaderNodeVarying::VisualShaderNodeVarying() {
}

////////////// Varying Setter

String VisualShaderNodeVaryingSetter::get_caption() const {
	return "VaryingSetter";
}

int VisualShaderNodeVaryingSetter::get_input_port_count() const {
	return 1;
}

VisualShaderNodeVaryingSetter::PortType VisualShaderNodeVaryingSetter::get_input_port_type(int p_port) const {
	return get_port_type(varying_type, p_port);
}

String VisualShaderNodeVaryingSetter::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeVaryingSetter::get_output_port_count() const {
	return 0;
}

VisualShaderNodeVaryingSetter::PortType VisualShaderNodeVaryingSetter::get_output_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeVaryingSetter::get_output_port_name(int p_port) const {
	return "";
}

String VisualShaderNodeVaryingSetter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String code;
	if (varying_name == "[None]") {
		return code;
	}
	code += vformat("	var_%s = %s;\n", varying_name, p_input_vars[0]);
	return code;
}

VisualShaderNodeVaryingSetter::VisualShaderNodeVaryingSetter() {
}

////////////// Varying Getter

String VisualShaderNodeVaryingGetter::get_caption() const {
	return "VaryingGetter";
}

int VisualShaderNodeVaryingGetter::get_input_port_count() const {
	return 0;
}

VisualShaderNodeVaryingGetter::PortType VisualShaderNodeVaryingGetter::get_input_port_type(int p_port) const {
	return PORT_TYPE_SCALAR;
}

String VisualShaderNodeVaryingGetter::get_input_port_name(int p_port) const {
	return "";
}

int VisualShaderNodeVaryingGetter::get_output_port_count() const {
	return 1;
}

VisualShaderNodeVaryingGetter::PortType VisualShaderNodeVaryingGetter::get_output_port_type(int p_port) const {
	return get_port_type(varying_type, p_port);
}

String VisualShaderNodeVaryingGetter::get_output_port_name(int p_port) const {
	return "";
}

bool VisualShaderNodeVaryingGetter::has_output_port_preview(int p_port) const {
	return false;
}

String VisualShaderNodeVaryingGetter::generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview) const {
	String from = varying_name;
	String from2;

	if (varying_name == "[None]" || p_for_preview) {
		switch (varying_type) {
			case VisualShader::VARYING_TYPE_FLOAT:
				from = "0.0";
				break;
			case VisualShader::VARYING_TYPE_INT:
				from = "0";
				break;
			case VisualShader::VARYING_TYPE_UINT:
				from = "0u";
				break;
			case VisualShader::VARYING_TYPE_VECTOR_2D:
				from = "vec2(0.0)";
				break;
			case VisualShader::VARYING_TYPE_VECTOR_3D:
				from = "vec3(0.0)";
				break;
			case VisualShader::VARYING_TYPE_VECTOR_4D:
				from = "vec4(0.0)";
				break;
			case VisualShader::VARYING_TYPE_BOOLEAN:
				from = "false";
				break;
			case VisualShader::VARYING_TYPE_TRANSFORM:
				from = "mat4(1.0)";
				break;
			default:
				break;
		}
		return vformat("	%s = %s;\n", p_output_vars[0], from);
	}
	return vformat("	%s = var_%s;\n", p_output_vars[0], from);
}

VisualShaderNodeVaryingGetter::VisualShaderNodeVaryingGetter() {
	varying_name = "[None]";
}
