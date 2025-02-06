/**************************************************************************/
/*  debug_adapter_protocol.cpp                                            */
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

#include "debug_adapter_protocol.h"

#include "core/config/project_settings.h"
#include "core/debugger/debugger_marshalls.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_log.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/gui/editor_run_bar.h"

DebugAdapterProtocol *DebugAdapterProtocol::singleton = nullptr;

Error DAPeer::handle_data() {
	int read = 0;
	// Read headers
	if (!has_header) {
		if (!connection->get_available_bytes()) {
			return OK;
		}
		while (true) {
			if (req_pos >= DAP_MAX_BUFFER_SIZE) {
				req_pos = 0;
				ERR_FAIL_V_MSG(ERR_OUT_OF_MEMORY, "Response header too big");
			}
			Error err = connection->get_partial_data(&req_buf[req_pos], 1, read);
			if (err != OK) {
				return FAILED;
			} else if (read != 1) { // Busy, wait until next poll
				return ERR_BUSY;
			}
			char *r = (char *)req_buf;
			int l = req_pos;

			// End of headers
			if (l > 3 && r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
				r[l - 3] = '\0'; // Null terminate to read string
				String header;
				header.parse_utf8(r);
				content_length = header.substr(16).to_int();
				has_header = true;
				req_pos = 0;
				break;
			}
			req_pos++;
		}
	}
	if (has_header) {
		while (req_pos < content_length) {
			if (content_length >= DAP_MAX_BUFFER_SIZE) {
				req_pos = 0;
				has_header = false;
				ERR_FAIL_COND_V_MSG(req_pos >= DAP_MAX_BUFFER_SIZE, ERR_OUT_OF_MEMORY, "Response content too big");
			}
			Error err = connection->get_partial_data(&req_buf[req_pos], content_length - req_pos, read);
			if (err != OK) {
				return FAILED;
			} else if (read < content_length - req_pos) {
				return ERR_BUSY;
			}
			req_pos += read;
		}

		// Parse data
		String msg;
		msg.parse_utf8((const char *)req_buf, req_pos);

		// Apply a timestamp if it there's none yet
		if (!timestamp) {
			timestamp = OS::get_singleton()->get_ticks_msec();
		}

		// Response
		if (DebugAdapterProtocol::get_singleton()->process_message(msg)) {
			// Reset to read again
			req_pos = 0;
			has_header = false;
			timestamp = 0;
		}
	}
	return OK;
}

Error DAPeer::send_data() {
	while (res_queue.size()) {
		Dictionary data = res_queue.front()->get();
		if (!data.has("seq")) {
			data["seq"] = ++seq;
		}
		String formatted_data = format_output(data);

		int data_sent = 0;
		while (data_sent < formatted_data.length()) {
			int curr_sent = 0;
			Error err = connection->put_partial_data((const uint8_t *)formatted_data.utf8().get_data(), formatted_data.size() - data_sent - 1, curr_sent);
			if (err != OK) {
				return err;
			}
			data_sent += curr_sent;
		}
		res_queue.pop_front();
	}
	return OK;
}

String DAPeer::format_output(const Dictionary &p_params) const {
	String response = Variant(p_params).to_json_string();
	String header = "Content-Length: ";
	CharString charstr = response.utf8();
	size_t len = charstr.length();
	header += itos(len);
	header += "\r\n\r\n";

	return header + response;
}

Error DebugAdapterProtocol::on_client_connected() {
	ERR_FAIL_COND_V_MSG(clients.size() >= DAP_MAX_CLIENTS, FAILED, "Max client limits reached");

	Ref<StreamPeerTCP> tcp_peer = server->take_connection();
	tcp_peer->set_no_delay(true);
	Ref<DAPeer> peer = memnew(DAPeer);
	peer->connection = tcp_peer;
	clients.push_back(peer);

	EditorDebuggerNode::get_singleton()->get_default_debugger()->set_move_to_foreground(false);
	EditorNode::get_log()->add_message("[DAP] Connection Taken", EditorLog::MSG_TYPE_EDITOR);
	return OK;
}

void DebugAdapterProtocol::on_client_disconnected(const Ref<DAPeer> &p_peer) {
	clients.erase(p_peer);
	if (!clients.size()) {
		reset_ids();
		EditorDebuggerNode::get_singleton()->get_default_debugger()->set_move_to_foreground(true);
	}
	EditorNode::get_log()->add_message("[DAP] Disconnected", EditorLog::MSG_TYPE_EDITOR);
}

void DebugAdapterProtocol::reset_current_info() {
	_current_request = "";
	_current_peer.unref();
}

void DebugAdapterProtocol::reset_ids() {
	breakpoint_id = 0;
	breakpoint_list.clear();

	reset_stack_info();
}

void DebugAdapterProtocol::reset_stack_info() {
	stackframe_id = 0;
	variable_id = 1;

	stackframe_list.clear();
	variable_list.clear();
	object_list.clear();
	object_pending_set.clear();
}

int DebugAdapterProtocol::parse_variant(const Variant &p_var) {
	switch (p_var.get_type()) {
		case Variant::VECTOR2:
		case Variant::VECTOR2I: {
			int id = variable_id++;
			Vector2 vec = p_var;
			const String type_scalar = Variant::get_type_name(p_var.get_type() == Variant::VECTOR2 ? Variant::FLOAT : Variant::INT);
			DAP::Variable x, y;
			x.name = "x";
			y.name = "y";
			x.type = type_scalar;
			y.type = type_scalar;
			x.value = rtos(vec.x);
			y.value = rtos(vec.y);

			Array arr;
			arr.push_back(x.to_json());
			arr.push_back(y.to_json());
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::RECT2:
		case Variant::RECT2I: {
			int id = variable_id++;
			Rect2 rect = p_var;
			const String type_scalar = Variant::get_type_name(p_var.get_type() == Variant::RECT2 ? Variant::FLOAT : Variant::INT);
			DAP::Variable x, y, w, h;
			x.name = "x";
			y.name = "y";
			w.name = "w";
			h.name = "h";
			x.type = type_scalar;
			y.type = type_scalar;
			w.type = type_scalar;
			h.type = type_scalar;
			x.value = rtos(rect.position.x);
			y.value = rtos(rect.position.y);
			w.value = rtos(rect.size.x);
			h.value = rtos(rect.size.y);

			Array arr;
			arr.push_back(x.to_json());
			arr.push_back(y.to_json());
			arr.push_back(w.to_json());
			arr.push_back(h.to_json());
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::VECTOR3:
		case Variant::VECTOR3I: {
			int id = variable_id++;
			Vector3 vec = p_var;
			const String type_scalar = Variant::get_type_name(p_var.get_type() == Variant::VECTOR3 ? Variant::FLOAT : Variant::INT);
			DAP::Variable x, y, z;
			x.name = "x";
			y.name = "y";
			z.name = "z";
			x.type = type_scalar;
			y.type = type_scalar;
			z.type = type_scalar;
			x.value = rtos(vec.x);
			y.value = rtos(vec.y);
			z.value = rtos(vec.z);

			Array arr;
			arr.push_back(x.to_json());
			arr.push_back(y.to_json());
			arr.push_back(z.to_json());
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::TRANSFORM2D: {
			int id = variable_id++;
			Transform2D transform = p_var;
			const String type_vec2 = Variant::get_type_name(Variant::VECTOR2);
			DAP::Variable x, y, origin;
			x.name = "x";
			y.name = "y";
			origin.name = "origin";
			x.type = type_vec2;
			y.type = type_vec2;
			origin.type = type_vec2;
			x.value = transform.columns[0];
			y.value = transform.columns[1];
			origin.value = transform.columns[2];
			x.variablesReference = parse_variant(transform.columns[0]);
			y.variablesReference = parse_variant(transform.columns[1]);
			origin.variablesReference = parse_variant(transform.columns[2]);

			Array arr;
			arr.push_back(x.to_json());
			arr.push_back(y.to_json());
			arr.push_back(origin.to_json());
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PLANE: {
			int id = variable_id++;
			Plane plane = p_var;
			DAP::Variable d, normal;
			d.name = "d";
			normal.name = "normal";
			d.type = Variant::get_type_name(Variant::FLOAT);
			normal.type = Variant::get_type_name(Variant::VECTOR3);
			d.value = rtos(plane.d);
			normal.value = plane.normal;
			normal.variablesReference = parse_variant(plane.normal);

			Array arr;
			arr.push_back(d.to_json());
			arr.push_back(normal.to_json());
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::QUATERNION: {
			int id = variable_id++;
			Quaternion quat = p_var;
			const String type_float = Variant::get_type_name(Variant::FLOAT);
			DAP::Variable x, y, z, w;
			x.name = "x";
			y.name = "y";
			z.name = "z";
			w.name = "w";
			x.type = type_float;
			y.type = type_float;
			z.type = type_float;
			w.type = type_float;
			x.value = rtos(quat.x);
			y.value = rtos(quat.y);
			z.value = rtos(quat.z);
			w.value = rtos(quat.w);

			Array arr;
			arr.push_back(x.to_json());
			arr.push_back(y.to_json());
			arr.push_back(z.to_json());
			arr.push_back(w.to_json());
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::AABB: {
			int id = variable_id++;
			AABB aabb = p_var;
			const String type_vec3 = Variant::get_type_name(Variant::VECTOR3);
			DAP::Variable position, size;
			position.name = "position";
			size.name = "size";
			position.type = type_vec3;
			size.type = type_vec3;
			position.value = aabb.position;
			size.value = aabb.size;
			position.variablesReference = parse_variant(aabb.position);
			size.variablesReference = parse_variant(aabb.size);

			Array arr;
			arr.push_back(position.to_json());
			arr.push_back(size.to_json());
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::BASIS: {
			int id = variable_id++;
			Basis basis = p_var;
			const String type_vec3 = Variant::get_type_name(Variant::VECTOR3);
			DAP::Variable x, y, z;
			x.name = "x";
			y.name = "y";
			z.name = "z";
			x.type = type_vec3;
			y.type = type_vec3;
			z.type = type_vec3;
			x.value = basis.rows[0];
			y.value = basis.rows[1];
			z.value = basis.rows[2];
			x.variablesReference = parse_variant(basis.rows[0]);
			y.variablesReference = parse_variant(basis.rows[1]);
			z.variablesReference = parse_variant(basis.rows[2]);

			Array arr;
			arr.push_back(x.to_json());
			arr.push_back(y.to_json());
			arr.push_back(z.to_json());
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::TRANSFORM3D: {
			int id = variable_id++;
			Transform3D transform = p_var;
			DAP::Variable basis, origin;
			basis.name = "basis";
			origin.name = "origin";
			basis.type = Variant::get_type_name(Variant::BASIS);
			origin.type = Variant::get_type_name(Variant::VECTOR3);
			basis.value = transform.basis;
			origin.value = transform.origin;
			basis.variablesReference = parse_variant(transform.basis);
			origin.variablesReference = parse_variant(transform.origin);

			Array arr;
			arr.push_back(basis.to_json());
			arr.push_back(origin.to_json());
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::COLOR: {
			int id = variable_id++;
			Color color = p_var;
			const String type_float = Variant::get_type_name(Variant::FLOAT);
			DAP::Variable r, g, b, a;
			r.name = "r";
			g.name = "g";
			b.name = "b";
			a.name = "a";
			r.type = type_float;
			g.type = type_float;
			b.type = type_float;
			a.type = type_float;
			r.value = rtos(color.r);
			g.value = rtos(color.g);
			b.value = rtos(color.b);
			a.value = rtos(color.a);

			Array arr;
			arr.push_back(r.to_json());
			arr.push_back(g.to_json());
			arr.push_back(b.to_json());
			arr.push_back(a.to_json());
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::ARRAY: {
			int id = variable_id++;
			Array array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = Variant::get_type_name(array[i].get_type());
				var.value = array[i];
				var.variablesReference = parse_variant(array[i]);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::DICTIONARY: {
			int id = variable_id++;
			Dictionary dictionary = p_var;
			Array arr;

			for (int i = 0; i < dictionary.size(); i++) {
				DAP::Variable var;
				var.name = dictionary.get_key_at_index(i);
				Variant value = dictionary.get_value_at_index(i);
				var.type = Variant::get_type_name(value.get_type());
				var.value = value;
				var.variablesReference = parse_variant(value);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PACKED_BYTE_ARRAY: {
			int id = variable_id++;
			PackedByteArray array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = "byte";
				var.value = itos(array[i]);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PACKED_INT32_ARRAY: {
			int id = variable_id++;
			PackedInt32Array array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = "int";
				var.value = itos(array[i]);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PACKED_INT64_ARRAY: {
			int id = variable_id++;
			PackedInt64Array array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = "long";
				var.value = itos(array[i]);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PACKED_FLOAT32_ARRAY: {
			int id = variable_id++;
			PackedFloat32Array array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = "float";
				var.value = rtos(array[i]);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PACKED_FLOAT64_ARRAY: {
			int id = variable_id++;
			PackedFloat64Array array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = "double";
				var.value = rtos(array[i]);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PACKED_STRING_ARRAY: {
			int id = variable_id++;
			PackedStringArray array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = Variant::get_type_name(Variant::STRING);
				var.value = array[i];
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PACKED_VECTOR2_ARRAY: {
			int id = variable_id++;
			PackedVector2Array array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = Variant::get_type_name(Variant::VECTOR2);
				var.value = array[i];
				var.variablesReference = parse_variant(array[i]);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PACKED_VECTOR3_ARRAY: {
			int id = variable_id++;
			PackedVector3Array array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = Variant::get_type_name(Variant::VECTOR3);
				var.value = array[i];
				var.variablesReference = parse_variant(array[i]);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PACKED_COLOR_ARRAY: {
			int id = variable_id++;
			PackedColorArray array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = Variant::get_type_name(Variant::COLOR);
				var.value = array[i];
				var.variablesReference = parse_variant(array[i]);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::PACKED_VECTOR4_ARRAY: {
			int id = variable_id++;
			PackedVector4Array array = p_var;
			DAP::Variable size;
			size.name = "size";
			size.type = Variant::get_type_name(Variant::INT);
			size.value = itos(array.size());

			Array arr;
			arr.push_back(size.to_json());

			for (int i = 0; i < array.size(); i++) {
				DAP::Variable var;
				var.name = itos(i);
				var.type = Variant::get_type_name(Variant::VECTOR4);
				var.value = array[i];
				var.variablesReference = parse_variant(array[i]);
				arr.push_back(var.to_json());
			}
			variable_list.insert(id, arr);
			return id;
		}
		case Variant::OBJECT: {
			// Objects have to be requested from the debuggee. This has do be done
			// in a lazy way, as retrieving object properties takes time.
			EncodedObjectAsID *encoded_obj = Object::cast_to<EncodedObjectAsID>(p_var);

			// Object may be null; in that case, return early.
			if (!encoded_obj) {
				return 0;
			}

			// Object may have been already requested.
			ObjectID object_id = encoded_obj->get_object_id();
			if (object_list.has(object_id)) {
				return object_list[object_id];
			}

			// Queue requesting the object.
			int id = variable_id++;
			object_list.insert(object_id, id);
			return id;
		}
		default:
			// Simple atomic stuff, or too complex to be manipulated
			return 0;
	}
}

void DebugAdapterProtocol::parse_object(SceneDebuggerObject &p_obj) {
	// If the object is not on the pending list, we weren't expecting it. Ignore it.
	ObjectID object_id = p_obj.id;
	if (!object_pending_set.erase(object_id)) {
		return;
	}

	// Populate DAP::Variable's with the object's properties. These properties will be divided by categories.
	Array properties;
	Array script_members;
	Array script_constants;
	Array script_node;
	DAP::Variable node_type;
	Array node_properties;

	for (SceneDebuggerObject::SceneDebuggerProperty &property : p_obj.properties) {
		PropertyInfo &info = property.first;

		// Script members ("Members/" prefix)
		if (info.name.begins_with("Members/")) {
			info.name = info.name.trim_prefix("Members/");
			script_members.push_back(parse_object_variable(property));
		}

		// Script constants ("Constants/" prefix)
		else if (info.name.begins_with("Constants/")) {
			info.name = info.name.trim_prefix("Constants/");
			script_constants.push_back(parse_object_variable(property));
		}

		// Script node ("Node/" prefix)
		else if (info.name.begins_with("Node/")) {
			info.name = info.name.trim_prefix("Node/");
			script_node.push_back(parse_object_variable(property));
		}

		// Regular categories (with type Variant::NIL)
		else if (info.type == Variant::NIL) {
			if (!node_properties.is_empty()) {
				node_type.value = itos(node_properties.size());
				variable_list.insert(node_type.variablesReference, node_properties.duplicate());
				properties.push_back(node_type.to_json());
			}

			node_type.name = info.name;
			node_type.type = "Category";
			node_type.variablesReference = variable_id++;
			node_properties.clear();
		}

		// Regular properties.
		else {
			node_properties.push_back(parse_object_variable(property));
		}
	}

	// Add the last category.
	if (!node_properties.is_empty()) {
		node_type.value = itos(node_properties.size());
		variable_list.insert(node_type.variablesReference, node_properties.duplicate());
		properties.push_back(node_type.to_json());
	}

	// Add the script categories, in reverse order to be at the front of the array:
	// ( [members; constants; node; category1; category2; ...] )
	if (!script_node.is_empty()) {
		DAP::Variable node;
		node.name = "Node";
		node.type = "Category";
		node.value = itos(script_node.size());
		node.variablesReference = variable_id++;
		variable_list.insert(node.variablesReference, script_node);
		properties.push_front(node.to_json());
	}

	if (!script_constants.is_empty()) {
		DAP::Variable constants;
		constants.name = "Constants";
		constants.type = "Category";
		constants.value = itos(script_constants.size());
		constants.variablesReference = variable_id++;
		variable_list.insert(constants.variablesReference, script_constants);
		properties.push_front(constants.to_json());
	}

	if (!script_members.is_empty()) {
		DAP::Variable members;
		members.name = "Members";
		members.type = "Category";
		members.value = itos(script_members.size());
		members.variablesReference = variable_id++;
		variable_list.insert(members.variablesReference, script_members);
		properties.push_front(members.to_json());
	}

	ERR_FAIL_COND(!object_list.has(object_id));
	variable_list.insert(object_list[object_id], properties);
}

void DebugAdapterProtocol::parse_evaluation(DebuggerMarshalls::ScriptStackVariable &p_var) {
	// If the eval is not on the pending list, we weren't expecting it. Ignore it.
	String eval = p_var.name;
	if (!eval_pending_list.erase(eval)) {
		return;
	}

	DAP::Variable variable;
	variable.name = p_var.name;
	variable.value = p_var.value;
	variable.type = Variant::get_type_name(p_var.value.get_type());
	variable.variablesReference = parse_variant(p_var.value);

	eval_list.insert(variable.name, variable);
}

const Variant DebugAdapterProtocol::parse_object_variable(const SceneDebuggerObject::SceneDebuggerProperty &p_property) {
	const PropertyInfo &info = p_property.first;
	const Variant &value = p_property.second;

	DAP::Variable var;
	var.name = info.name;
	var.type = Variant::get_type_name(info.type);
	var.value = value;
	var.variablesReference = parse_variant(value);

	return var.to_json();
}

ObjectID DebugAdapterProtocol::search_object_id(DAPVarID p_var_id) {
	for (const KeyValue<ObjectID, DAPVarID> &E : object_list) {
		if (E.value == p_var_id) {
			return E.key;
		}
	}
	return ObjectID();
}

bool DebugAdapterProtocol::request_remote_object(const ObjectID &p_object_id) {
	// If the object is already on the pending list, we don't need to request it again.
	if (object_pending_set.has(p_object_id)) {
		return false;
	}

	TypedArray<uint64_t> arr;
	arr.append(p_object_id);
	EditorDebuggerNode::get_singleton()->get_default_debugger()->request_remote_objects(arr);
	object_pending_set.insert(p_object_id);

	return true;
}

bool DebugAdapterProtocol::request_remote_evaluate(const String &p_eval, int p_stack_frame) {
	// If the eval is already on the pending list, we don't need to request it again
	if (eval_pending_list.has(p_eval)) {
		return false;
	}

	EditorDebuggerNode::get_singleton()->get_default_debugger()->request_remote_evaluate(p_eval, p_stack_frame);
	eval_pending_list.insert(p_eval);

	return true;
}

bool DebugAdapterProtocol::process_message(const String &p_text) {
	JSON json;
	ERR_FAIL_COND_V_MSG(json.parse(p_text) != OK, true, "Malformed message!");
	Dictionary params = json.get_data();
	bool completed = true;

	if (OS::get_singleton()->get_ticks_msec() - _current_peer->timestamp > _request_timeout) {
		Dictionary response = parser->prepare_error_response(params, DAP::ErrorType::TIMEOUT);
		_current_peer->res_queue.push_front(response);
		return true;
	}

	// Append "req_" to any command received; prevents name clash with existing functions, and possibly exploiting
	String command = "req_" + (String)params["command"];
	if (parser->has_method(command)) {
		_current_request = params["command"];

		Array args;
		args.push_back(params);
		Dictionary response = parser->callv(command, args);
		if (!response.is_empty()) {
			_current_peer->res_queue.push_front(response);
		} else {
			// Launch request needs to be deferred until we receive a configurationDone request.
			if (command != "req_launch") {
				completed = false;
			}
		}
	}

	reset_current_info();
	return completed;
}

void DebugAdapterProtocol::notify_initialized() {
	Dictionary event = parser->ev_initialized();
	_current_peer->res_queue.push_back(event);
}

void DebugAdapterProtocol::notify_process() {
	String launch_mode = _current_peer->attached ? "attach" : "launch";

	Dictionary event = parser->ev_process(launch_mode);
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		E->get()->res_queue.push_back(event);
	}
}

void DebugAdapterProtocol::notify_terminated() {
	Dictionary event = parser->ev_terminated();
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		if ((_current_request == "launch" || _current_request == "restart") && _current_peer == E->get()) {
			continue;
		}
		E->get()->res_queue.push_back(event);
	}
}

void DebugAdapterProtocol::notify_exited(const int &p_exitcode) {
	Dictionary event = parser->ev_exited(p_exitcode);
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		if ((_current_request == "launch" || _current_request == "restart") && _current_peer == E->get()) {
			continue;
		}
		E->get()->res_queue.push_back(event);
	}
}

void DebugAdapterProtocol::notify_stopped_paused() {
	Dictionary event = parser->ev_stopped_paused();
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		E->get()->res_queue.push_back(event);
	}
}

void DebugAdapterProtocol::notify_stopped_exception(const String &p_error) {
	Dictionary event = parser->ev_stopped_exception(p_error);
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		E->get()->res_queue.push_back(event);
	}
}

void DebugAdapterProtocol::notify_stopped_breakpoint(const int &p_id) {
	Dictionary event = parser->ev_stopped_breakpoint(p_id);
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		E->get()->res_queue.push_back(event);
	}
}

void DebugAdapterProtocol::notify_stopped_step() {
	Dictionary event = parser->ev_stopped_step();
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		E->get()->res_queue.push_back(event);
	}
}

void DebugAdapterProtocol::notify_continued() {
	Dictionary event = parser->ev_continued();
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		if (_current_request == "continue" && E->get() == _current_peer) {
			continue;
		}
		E->get()->res_queue.push_back(event);
	}

	reset_stack_info();
}

void DebugAdapterProtocol::notify_output(const String &p_message, RemoteDebugger::MessageType p_type) {
	Dictionary event = parser->ev_output(p_message, p_type);
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		E->get()->res_queue.push_back(event);
	}
}

void DebugAdapterProtocol::notify_custom_data(const String &p_msg, const Array &p_data) {
	Dictionary event = parser->ev_custom_data(p_msg, p_data);
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		Ref<DAPeer> peer = E->get();
		if (peer->supportsCustomData) {
			peer->res_queue.push_back(event);
		}
	}
}

void DebugAdapterProtocol::notify_breakpoint(const DAP::Breakpoint &p_breakpoint, const bool &p_enabled) {
	Dictionary event = parser->ev_breakpoint(p_breakpoint, p_enabled);
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		if (_current_request == "setBreakpoints" && E->get() == _current_peer) {
			continue;
		}
		E->get()->res_queue.push_back(event);
	}
}

Array DebugAdapterProtocol::update_breakpoints(const String &p_path, const Array &p_lines) {
	Array updated_breakpoints;

	// Add breakpoints
	for (int i = 0; i < p_lines.size(); i++) {
		EditorDebuggerNode::get_singleton()->get_default_debugger()->_set_breakpoint(p_path, p_lines[i], true);
		DAP::Breakpoint breakpoint;
		breakpoint.line = p_lines[i];
		breakpoint.source.path = p_path;

		ERR_FAIL_COND_V(!breakpoint_list.find(breakpoint), Array());
		updated_breakpoints.push_back(breakpoint_list.find(breakpoint)->get().to_json());
	}

	// Remove breakpoints
	for (List<DAP::Breakpoint>::Element *E = breakpoint_list.front(); E; E = E->next()) {
		DAP::Breakpoint b = E->get();
		if (b.source.path == p_path && !p_lines.has(b.line)) {
			EditorDebuggerNode::get_singleton()->get_default_debugger()->_set_breakpoint(p_path, b.line, false);
		}
	}

	return updated_breakpoints;
}

void DebugAdapterProtocol::on_debug_paused() {
	if (EditorRunBar::get_singleton()->get_pause_button()->is_pressed()) {
		notify_stopped_paused();
	} else {
		notify_continued();
	}
}

void DebugAdapterProtocol::on_debug_stopped() {
	notify_exited();
	notify_terminated();
}

void DebugAdapterProtocol::on_debug_output(const String &p_message, int p_type) {
	notify_output(p_message, RemoteDebugger::MessageType(p_type));
}

void DebugAdapterProtocol::on_debug_breaked(const bool &p_reallydid, const bool &p_can_debug, const String &p_reason, const bool &p_has_stackdump) {
	if (!p_reallydid) {
		notify_continued();
		return;
	}

	if (p_reason == "Breakpoint") {
		if (_stepping) {
			notify_stopped_step();
			_stepping = false;
		} else {
			_processing_breakpoint = true; // Wait for stack_dump to find where the breakpoint happened
		}
	} else {
		notify_stopped_exception(p_reason);
	}

	_processing_stackdump = p_has_stackdump;
}

void DebugAdapterProtocol::on_debug_breakpoint_toggled(const String &p_path, const int &p_line, const bool &p_enabled) {
	DAP::Breakpoint breakpoint;
	breakpoint.verified = true;
	breakpoint.source.path = ProjectSettings::get_singleton()->globalize_path(p_path);
	breakpoint.source.compute_checksums();
	breakpoint.line = p_line;

	if (p_enabled) {
		// Add the breakpoint
		breakpoint.id = breakpoint_id++;
		breakpoint_list.push_back(breakpoint);
	} else {
		// Remove the breakpoint
		List<DAP::Breakpoint>::Element *E = breakpoint_list.find(breakpoint);
		if (E) {
			breakpoint.id = E->get().id;
			breakpoint_list.erase(E);
		}
	}

	notify_breakpoint(breakpoint, p_enabled);
}

void DebugAdapterProtocol::on_debug_stack_dump(const Array &p_stack_dump) {
	if (_processing_breakpoint && !p_stack_dump.is_empty()) {
		// Find existing breakpoint
		Dictionary d = p_stack_dump[0];
		DAP::Breakpoint breakpoint;
		breakpoint.source.path = ProjectSettings::get_singleton()->globalize_path(d["file"]);
		breakpoint.line = d["line"];

		List<DAP::Breakpoint>::Element *E = breakpoint_list.find(breakpoint);
		if (E) {
			notify_stopped_breakpoint(E->get().id);
		}

		_processing_breakpoint = false;
	}

	stackframe_id = 0;
	stackframe_list.clear();

	// Fill in stacktrace information
	for (int i = 0; i < p_stack_dump.size(); i++) {
		Dictionary stack_info = p_stack_dump[i];
		DAP::StackFrame stackframe;
		stackframe.id = stackframe_id++;
		stackframe.name = stack_info["function"];
		stackframe.line = stack_info["line"];
		stackframe.column = 0;
		stackframe.source.path = ProjectSettings::get_singleton()->globalize_path(stack_info["file"]);
		stackframe.source.compute_checksums();

		// Information for "Locals", "Members" and "Globals" variables respectively
		List<int> scope_ids;
		for (int j = 0; j < 3; j++) {
			scope_ids.push_back(variable_id++);
		}

		stackframe_list.insert(stackframe, scope_ids);
	}

	_current_frame = 0;
	_processing_stackdump = false;
}

void DebugAdapterProtocol::on_debug_stack_frame_vars(const int &p_size) {
	_remaining_vars = p_size;
	DAP::StackFrame frame;
	frame.id = _current_frame;
	ERR_FAIL_COND(!stackframe_list.has(frame));
	List<int> scope_ids = stackframe_list.find(frame)->value;
	for (List<int>::Element *E = scope_ids.front(); E; E = E->next()) {
		int var_id = E->get();
		if (variable_list.has(var_id)) {
			variable_list.find(var_id)->value.clear();
		} else {
			variable_list.insert(var_id, Array());
		}
	}
}

void DebugAdapterProtocol::on_debug_stack_frame_var(const Array &p_data) {
	DebuggerMarshalls::ScriptStackVariable stack_var;
	stack_var.deserialize(p_data);

	ERR_FAIL_COND(stackframe_list.is_empty());
	DAP::StackFrame frame;
	frame.id = _current_frame;

	List<int> scope_ids = stackframe_list.find(frame)->value;
	ERR_FAIL_COND(scope_ids.size() != 3);
	ERR_FAIL_INDEX(stack_var.type, 4);
	int var_id = scope_ids.get(stack_var.type);

	DAP::Variable variable;

	variable.name = stack_var.name;
	variable.value = stack_var.value;
	variable.type = Variant::get_type_name(stack_var.value.get_type());
	variable.variablesReference = parse_variant(stack_var.value);

	variable_list.find(var_id)->value.push_back(variable.to_json());
	_remaining_vars--;
}

void DebugAdapterProtocol::on_debug_data(const String &p_msg, const Array &p_data) {
	// Ignore data that is already handled by DAP
	if (p_msg == "debug_enter" || p_msg == "debug_exit" || p_msg == "stack_dump" || p_msg == "stack_frame_vars" || p_msg == "stack_frame_var" || p_msg == "output" || p_msg == "request_quit") {
		return;
	}

	if (p_msg == "scene:inspect_object") {
		// An object was requested from the debuggee; parse it.
		SceneDebuggerObject remote_obj;
		remote_obj.deserialize(p_data);

		parse_object(remote_obj);
	} else if (p_msg == "evaluation_return") {
		// An evaluation was requested from the debuggee; parse it.
		DebuggerMarshalls::ScriptStackVariable remote_evaluation;
		remote_evaluation.deserialize(p_data);

		parse_evaluation(remote_evaluation);
	}

	notify_custom_data(p_msg, p_data);
}

void DebugAdapterProtocol::poll() {
	if (server->is_connection_available()) {
		on_client_connected();
	}
	List<Ref<DAPeer>> to_delete;
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		Ref<DAPeer> peer = E->get();
		peer->connection->poll();
		StreamPeerTCP::Status status = peer->connection->get_status();
		if (status == StreamPeerTCP::STATUS_NONE || status == StreamPeerTCP::STATUS_ERROR) {
			to_delete.push_back(peer);
		} else {
			_current_peer = peer;
			Error err = peer->handle_data();
			if (err != OK && err != ERR_BUSY) {
				to_delete.push_back(peer);
			}
			err = peer->send_data();
			if (err != OK && err != ERR_BUSY) {
				to_delete.push_back(peer);
			}
		}
	}

	for (List<Ref<DAPeer>>::Element *E = to_delete.front(); E; E = E->next()) {
		on_client_disconnected(E->get());
	}
	to_delete.clear();
}

Error DebugAdapterProtocol::start(int p_port, const IPAddress &p_bind_ip) {
	_request_timeout = (uint64_t)_EDITOR_GET("network/debug_adapter/request_timeout");
	_sync_breakpoints = (bool)_EDITOR_GET("network/debug_adapter/sync_breakpoints");
	_initialized = true;
	return server->listen(p_port, p_bind_ip);
}

void DebugAdapterProtocol::stop() {
	for (List<Ref<DAPeer>>::Element *E = clients.front(); E; E = E->next()) {
		E->get()->connection->disconnect_from_host();
	}

	clients.clear();
	server->stop();
	_initialized = false;
}

DebugAdapterProtocol::DebugAdapterProtocol() {
	server.instantiate();
	singleton = this;
	parser = memnew(DebugAdapterParser);

	reset_ids();

	EditorRunBar::get_singleton()->get_pause_button()->connect(SceneStringName(pressed), callable_mp(this, &DebugAdapterProtocol::on_debug_paused));

	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	debugger_node->connect("breakpoint_toggled", callable_mp(this, &DebugAdapterProtocol::on_debug_breakpoint_toggled));

	debugger_node->get_default_debugger()->connect("stopped", callable_mp(this, &DebugAdapterProtocol::on_debug_stopped));
	debugger_node->get_default_debugger()->connect(SceneStringName(output), callable_mp(this, &DebugAdapterProtocol::on_debug_output));
	debugger_node->get_default_debugger()->connect("breaked", callable_mp(this, &DebugAdapterProtocol::on_debug_breaked));
	debugger_node->get_default_debugger()->connect("stack_dump", callable_mp(this, &DebugAdapterProtocol::on_debug_stack_dump));
	debugger_node->get_default_debugger()->connect("stack_frame_vars", callable_mp(this, &DebugAdapterProtocol::on_debug_stack_frame_vars));
	debugger_node->get_default_debugger()->connect("stack_frame_var", callable_mp(this, &DebugAdapterProtocol::on_debug_stack_frame_var));
	debugger_node->get_default_debugger()->connect("debug_data", callable_mp(this, &DebugAdapterProtocol::on_debug_data));
}

DebugAdapterProtocol::~DebugAdapterProtocol() {
	memdelete(parser);
}
