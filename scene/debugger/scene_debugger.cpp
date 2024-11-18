/**************************************************************************/
/*  scene_debugger.cpp                                                    */
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

#include "scene_debugger.h"

#include "core/debugger/debugger_marshalls.h"
#include "core/debugger/engine_debugger.h"
#include "core/io/marshalls.h"
#include "core/math/math_fieldwise.h"
#include "core/object/script_language.h"
#include "core/templates/local_vector.h"
#include "scene/2d/physics/collision_object_2d.h"
#include "scene/2d/physics/collision_polygon_2d.h"
#include "scene/2d/physics/collision_shape_2d.h"
#ifndef _3D_DISABLED
#include "scene/3d/physics/collision_object_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#include "scene/resources/surface_tool.h"
#endif // _3D_DISABLED
#include "scene/gui/popup_menu.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"
#include "scene/theme/theme_db.h"

SceneDebugger::SceneDebugger() {
	singleton = this;

#ifdef DEBUG_ENABLED
	LiveEditor::singleton = memnew(LiveEditor);
	RuntimeNodeSelect::singleton = memnew(RuntimeNodeSelect);

	EngineDebugger::register_message_capture("scene", EngineDebugger::Capture(nullptr, SceneDebugger::parse_message));
#endif // DEBUG_ENABLED
}

SceneDebugger::~SceneDebugger() {
#ifdef DEBUG_ENABLED
	if (LiveEditor::singleton) {
		EngineDebugger::unregister_message_capture("scene");
		memdelete(LiveEditor::singleton);
		LiveEditor::singleton = nullptr;
	}

	if (RuntimeNodeSelect::singleton) {
		memdelete(RuntimeNodeSelect::singleton);
		RuntimeNodeSelect::singleton = nullptr;
	}
#endif // DEBUG_ENABLED

	singleton = nullptr;
}

void SceneDebugger::initialize() {
	if (EngineDebugger::is_active()) {
		memnew(SceneDebugger);
	}
}

void SceneDebugger::deinitialize() {
	if (singleton) {
		memdelete(singleton);
	}
}

#ifdef DEBUG_ENABLED
void SceneDebugger::_handle_input(const Ref<InputEvent> &p_event, const Ref<Shortcut> &p_shortcut) {
	Ref<InputEventKey> k = p_event;
	if (p_shortcut.is_valid() && k.is_valid() && k->is_pressed() && !k->is_echo() && p_shortcut->matches_event(k)) {
		EngineDebugger::get_singleton()->send_message("request_quit", Array());
	}
}

Error SceneDebugger::parse_message(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return ERR_UNCONFIGURED;
	}

	LiveEditor *live_editor = LiveEditor::get_singleton();
	if (!live_editor) {
		return ERR_UNCONFIGURED;
	}
	RuntimeNodeSelect *runtime_node_select = RuntimeNodeSelect::get_singleton();
	if (!runtime_node_select) {
		return ERR_UNCONFIGURED;
	}

	r_captured = true;
	if (p_msg == "setup_scene") {
		SceneTree::get_singleton()->get_root()->connect(SceneStringName(window_input), callable_mp_static(SceneDebugger::_handle_input).bind(DebuggerMarshalls::deserialize_key_shortcut(p_args)));

	} else if (p_msg == "request_scene_tree") { /// Scene Tree
		live_editor->_send_tree();

	} else if (p_msg == "save_node") {
		ERR_FAIL_COND_V(p_args.size() < 2, ERR_INVALID_DATA);
		_save_node(p_args[0], p_args[1]);
		Array arr;
		arr.append(p_args[1]);
		EngineDebugger::get_singleton()->send_message("filesystem:update_file", { arr });

	} else if (p_msg == "inspect_objects") { /// Object Inspect
		ERR_FAIL_COND_V(p_args.size() < 2, ERR_INVALID_DATA);
		Vector<ObjectID> ids;
		for (const Variant &id : (Array)p_args[0]) {
			ids.append(ObjectID(id.operator uint64_t()));
		}
		_send_object_ids(ids, p_args[1]);

	} else if (p_msg == "clear_selection") {
		runtime_node_select->_clear_selection();

	} else if (p_msg == "suspend_changed") {
		ERR_FAIL_COND_V(p_args.is_empty(), ERR_INVALID_DATA);
		bool suspended = p_args[0];
		scene_tree->set_suspend(suspended);
		runtime_node_select->_update_input_state();

	} else if (p_msg == "next_frame") {
		_next_frame();

	} else if (p_msg == "override_cameras") { /// Camera
		ERR_FAIL_COND_V(p_args.is_empty(), ERR_INVALID_DATA);
		bool enable = p_args[0];
		bool from_editor = p_args[1];
		scene_tree->get_root()->enable_canvas_transform_override(enable);
#ifndef _3D_DISABLED
		scene_tree->get_root()->enable_camera_3d_override(enable);
#endif // _3D_DISABLED
		runtime_node_select->_set_camera_override_enabled(enable && !from_editor);

	} else if (p_msg == "transform_camera_2d") {
		ERR_FAIL_COND_V(p_args.is_empty(), ERR_INVALID_DATA);
		Transform2D transform = p_args[0];
		scene_tree->get_root()->set_canvas_transform_override(transform);
		runtime_node_select->_queue_selection_update();

#ifndef _3D_DISABLED
	} else if (p_msg == "transform_camera_3d") {
		ERR_FAIL_COND_V(p_args.size() < 5, ERR_INVALID_DATA);
		Transform3D transform = p_args[0];
		bool is_perspective = p_args[1];
		float size_or_fov = p_args[2];
		float depth_near = p_args[3];
		float depth_far = p_args[4];
		if (is_perspective) {
			scene_tree->get_root()->set_camera_3d_override_perspective(size_or_fov, depth_near, depth_far);
		} else {
			scene_tree->get_root()->set_camera_3d_override_orthogonal(size_or_fov, depth_near, depth_far);
		}
		scene_tree->get_root()->set_camera_3d_override_transform(transform);
		runtime_node_select->_queue_selection_update();
#endif // _3D_DISABLED

	} else if (p_msg == "set_object_property") {
		ERR_FAIL_COND_V(p_args.size() < 3, ERR_INVALID_DATA);
		_set_object_property(p_args[0], p_args[1], p_args[2]);
		runtime_node_select->_queue_selection_update();

	} else if (p_msg == "set_object_property_field") {
		ERR_FAIL_COND_V(p_args.size() < 4, ERR_INVALID_DATA);
		_set_object_property(p_args[0], p_args[1], p_args[2], p_args[3]);
		runtime_node_select->_queue_selection_update();

	} else if (p_msg == "reload_cached_files") {
		ERR_FAIL_COND_V(p_args.is_empty(), ERR_INVALID_DATA);
		PackedStringArray files = p_args[0];
		reload_cached_files(files);

	} else if (p_msg.begins_with("live_")) { /// Live Edit
		if (p_msg == "live_set_root") {
			ERR_FAIL_COND_V(p_args.size() < 2, ERR_INVALID_DATA);
			live_editor->_root_func(p_args[0], p_args[1]);

		} else if (p_msg == "live_node_path") {
			ERR_FAIL_COND_V(p_args.size() < 2, ERR_INVALID_DATA);
			live_editor->_node_path_func(p_args[0], p_args[1]);

		} else if (p_msg == "live_res_path") {
			ERR_FAIL_COND_V(p_args.size() < 2, ERR_INVALID_DATA);
			live_editor->_res_path_func(p_args[0], p_args[1]);

		} else if (p_msg == "live_node_prop_res") {
			ERR_FAIL_COND_V(p_args.size() < 3, ERR_INVALID_DATA);
			live_editor->_node_set_res_func(p_args[0], p_args[1], p_args[2]);

		} else if (p_msg == "live_node_prop") {
			ERR_FAIL_COND_V(p_args.size() < 3, ERR_INVALID_DATA);
			live_editor->_node_set_func(p_args[0], p_args[1], p_args[2]);

		} else if (p_msg == "live_res_prop_res") {
			ERR_FAIL_COND_V(p_args.size() < 3, ERR_INVALID_DATA);
			live_editor->_res_set_res_func(p_args[0], p_args[1], p_args[2]);

		} else if (p_msg == "live_res_prop") {
			ERR_FAIL_COND_V(p_args.size() < 3, ERR_INVALID_DATA);
			live_editor->_res_set_func(p_args[0], p_args[1], p_args[2]);

		} else if (p_msg == "live_node_call") {
			ERR_FAIL_COND_V(p_args.size() < 2, ERR_INVALID_DATA);
			LocalVector<Variant> args;
			LocalVector<Variant *> argptrs;
			args.resize(p_args.size() - 2);
			argptrs.resize(args.size());
			for (uint32_t i = 0; i < args.size(); i++) {
				args[i] = p_args[i + 2];
				argptrs[i] = &args[i];
			}
			live_editor->_node_call_func(p_args[0], p_args[1], argptrs.size() ? (const Variant **)argptrs.ptr() : nullptr, argptrs.size());

		} else if (p_msg == "live_res_call") {
			ERR_FAIL_COND_V(p_args.size() < 2, ERR_INVALID_DATA);
			LocalVector<Variant> args;
			LocalVector<Variant *> argptrs;
			args.resize(p_args.size() - 2);
			argptrs.resize(args.size());
			for (uint32_t i = 0; i < args.size(); i++) {
				args[i] = p_args[i + 2];
				argptrs[i] = &args[i];
			}
			live_editor->_res_call_func(p_args[0], p_args[1], argptrs.size() ? (const Variant **)argptrs.ptr() : nullptr, argptrs.size());

		} else if (p_msg == "live_create_node") {
			ERR_FAIL_COND_V(p_args.size() < 3, ERR_INVALID_DATA);
			live_editor->_create_node_func(p_args[0], p_args[1], p_args[2]);

		} else if (p_msg == "live_instantiate_node") {
			ERR_FAIL_COND_V(p_args.size() < 3, ERR_INVALID_DATA);
			live_editor->_instance_node_func(p_args[0], p_args[1], p_args[2]);

		} else if (p_msg == "live_remove_node") {
			ERR_FAIL_COND_V(p_args.is_empty(), ERR_INVALID_DATA);
			live_editor->_remove_node_func(p_args[0]);
			runtime_node_select->_queue_selection_update();

		} else if (p_msg == "live_remove_and_keep_node") {
			ERR_FAIL_COND_V(p_args.size() < 2, ERR_INVALID_DATA);
			live_editor->_remove_and_keep_node_func(p_args[0], p_args[1]);
			runtime_node_select->_queue_selection_update();

		} else if (p_msg == "live_restore_node") {
			ERR_FAIL_COND_V(p_args.size() < 3, ERR_INVALID_DATA);
			live_editor->_restore_node_func(p_args[0], p_args[1], p_args[2]);

		} else if (p_msg == "live_duplicate_node") {
			ERR_FAIL_COND_V(p_args.size() < 2, ERR_INVALID_DATA);
			live_editor->_duplicate_node_func(p_args[0], p_args[1]);

		} else if (p_msg == "live_reparent_node") {
			ERR_FAIL_COND_V(p_args.size() < 4, ERR_INVALID_DATA);
			live_editor->_reparent_node_func(p_args[0], p_args[1], p_args[2], p_args[3]);

		} else {
			return ERR_SKIP;
		}

	} else if (p_msg.begins_with("runtime_node_select_")) { /// Runtime Node Selection
		if (p_msg == "runtime_node_select_setup") {
			ERR_FAIL_COND_V(p_args.is_empty() || p_args[0].get_type() != Variant::DICTIONARY, ERR_INVALID_DATA);
			runtime_node_select->_setup(p_args[0]);

		} else if (p_msg == "runtime_node_select_set_type") {
			ERR_FAIL_COND_V(p_args.is_empty(), ERR_INVALID_DATA);
			RuntimeNodeSelect::NodeType type = (RuntimeNodeSelect::NodeType)(int)p_args[0];
			runtime_node_select->_node_set_type(type);

		} else if (p_msg == "runtime_node_select_set_mode") {
			ERR_FAIL_COND_V(p_args.is_empty(), ERR_INVALID_DATA);
			RuntimeNodeSelect::SelectMode mode = (RuntimeNodeSelect::SelectMode)(int)p_args[0];
			runtime_node_select->_select_set_mode(mode);

		} else if (p_msg == "runtime_node_select_set_visible") {
			ERR_FAIL_COND_V(p_args.is_empty(), ERR_INVALID_DATA);
			bool visible = p_args[0];
			runtime_node_select->_set_selection_visible(visible);

		} else if (p_msg == "runtime_node_select_reset_camera_2d") {
			runtime_node_select->_reset_camera_2d();

#ifndef _3D_DISABLED
		} else if (p_msg == "runtime_node_select_reset_camera_3d") {
			runtime_node_select->_reset_camera_3d();
#endif // _3D_DISABLED

		} else {
			return ERR_SKIP;
		}

	} else {
		r_captured = false;
	}

	return OK;
}

void SceneDebugger::_save_node(ObjectID id, const String &p_path) {
	Node *node = Object::cast_to<Node>(ObjectDB::get_instance(id));
	ERR_FAIL_NULL(node);

#ifdef TOOLS_ENABLED
	HashMap<const Node *, Node *> duplimap;
	Node *copy = node->duplicate_from_editor(duplimap);
#else
	Node *copy = node->duplicate();
#endif // TOOLS_ENABLED

	// Handle Unique Nodes.
	for (int i = 0; i < copy->get_child_count(false); i++) {
		_set_node_owner_recursive(copy->get_child(i, false), copy);
	}
	// Root node cannot ever be unique name in its own Scene!
	copy->set_unique_name_in_owner(false);

	Ref<PackedScene> ps = memnew(PackedScene);
	ps->pack(copy);
	ResourceSaver::save(ps, p_path);

	memdelete(copy);
}

void SceneDebugger::_set_node_owner_recursive(Node *p_node, Node *p_owner) {
	if (!p_node->get_owner()) {
		p_node->set_owner(p_owner);
	}

	for (int i = 0; i < p_node->get_child_count(false); i++) {
		_set_node_owner_recursive(p_node->get_child(i, false), p_owner);
	}
}

void SceneDebugger::_send_object_ids(const Vector<ObjectID> &p_ids, bool p_update_selection) {
	Vector<ObjectID> ids = p_ids;
	if (ids.size() > RuntimeNodeSelect::get_singleton()->max_selection) {
		ids.resize(RuntimeNodeSelect::get_singleton()->max_selection);
		EngineDebugger::get_singleton()->send_message("show_selection_limit_warning", Array());
	}

	LocalVector<Node *> nodes;
	Array objs;
	bool objs_missing = false;
	for (const ObjectID &id : ids) {
		SceneDebuggerObject obj(id);
		if (obj.id.is_null()) {
			objs_missing = true;
			continue;
		}

		if (p_update_selection) {
			if (Node *node = Object::cast_to<Node>(ObjectDB::get_instance(id))) {
				nodes.push_back(node);
			}
		}

		Array arr;
		obj.serialize(arr);
		objs.append(arr);
	}

	if (p_update_selection) {
		RuntimeNodeSelect::get_singleton()->_set_selected_nodes(nodes);
	}

	if (objs_missing) {
		Array invalid_selection;
		for (const ObjectID &id : ids) {
			invalid_selection.append(id);
		}

		Array arr;
		arr.append(invalid_selection);
		EngineDebugger::get_singleton()->send_message("remote_selection_invalidated", arr);

		EngineDebugger::get_singleton()->send_message(objs.is_empty() ? "remote_nothing_clicked" : "remote_nodes_clicked", objs);
	} else {
		EngineDebugger::get_singleton()->send_message("scene:inspect_objects", objs);
	}
}

void SceneDebugger::_set_object_property(ObjectID p_id, const String &p_property, const Variant &p_value, const String &p_field) {
	Object *obj = ObjectDB::get_instance(p_id);
	if (!obj) {
		return;
	}

	String prop_name = p_property;
	if (p_property.begins_with("Members/")) {
		Vector<String> ss = p_property.split("/");
		prop_name = ss[ss.size() - 1];
	}

	Variant value;
	if (p_field.is_empty()) {
		// Whole value.
		value = p_value;
	} else {
		// Only one field.
		value = fieldwise_assign(obj->get(prop_name), p_value, p_field);
	}

	obj->set(prop_name, value);
}

void SceneDebugger::_next_frame() {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree->is_suspended()) {
		return;
	}

	scene_tree->set_suspend(false);
	RenderingServer::get_singleton()->connect("frame_post_draw", callable_mp(scene_tree, &SceneTree::set_suspend).bind(true), Object::CONNECT_ONE_SHOT);
}

void SceneDebugger::add_to_cache(const String &p_filename, Node *p_node) {
	LiveEditor *debugger = LiveEditor::get_singleton();
	if (!debugger) {
		return;
	}

	if (EngineDebugger::get_script_debugger() && !p_filename.is_empty()) {
		debugger->live_scene_edit_cache[p_filename].insert(p_node);
	}
}

void SceneDebugger::remove_from_cache(const String &p_filename, Node *p_node) {
	LiveEditor *debugger = LiveEditor::get_singleton();
	if (!debugger) {
		return;
	}

	HashMap<String, HashSet<Node *>> &edit_cache = debugger->live_scene_edit_cache;
	HashMap<String, HashSet<Node *>>::Iterator E = edit_cache.find(p_filename);
	if (E) {
		E->value.erase(p_node);
		if (E->value.size() == 0) {
			edit_cache.remove(E);
		}
	}

	HashMap<Node *, HashMap<ObjectID, Node *>> &remove_list = debugger->live_edit_remove_list;
	HashMap<Node *, HashMap<ObjectID, Node *>>::Iterator F = remove_list.find(p_node);
	if (F) {
		for (const KeyValue<ObjectID, Node *> &G : F->value) {
			memdelete(G.value);
		}
		remove_list.remove(F);
	}
}

void SceneDebugger::reload_cached_files(const PackedStringArray &p_files) {
	for (const String &file : p_files) {
		Ref<Resource> res = ResourceCache::get_ref(file);
		if (res.is_valid()) {
			res->reload_from_file();
		}
	}
}

/// SceneDebuggerObject
SceneDebuggerObject::SceneDebuggerObject(ObjectID p_id) {
	id = ObjectID();
	Object *obj = ObjectDB::get_instance(p_id);
	if (!obj) {
		return;
	}

	id = p_id;
	class_name = obj->get_class();

	if (ScriptInstance *si = obj->get_script_instance()) {
		// Read script instance constants and variables
		if (!si->get_script().is_null()) {
			Script *s = si->get_script().ptr();
			_parse_script_properties(s, si);
		}
	}

	if (Node *node = Object::cast_to<Node>(obj)) {
		// For debugging multiplayer.
		{
			PropertyInfo pi(Variant::INT, String("Node/multiplayer_authority"), PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY);
			properties.push_back(SceneDebuggerProperty(pi, node->get_multiplayer_authority()));
		}

		// Add specialized NodePath info (if inside tree).
		if (node->is_inside_tree()) {
			PropertyInfo pi(Variant::NODE_PATH, String("Node/path"));
			properties.push_back(SceneDebuggerProperty(pi, node->get_path()));
		} else { // Can't ask for path if a node is not in tree.
			PropertyInfo pi(Variant::STRING, String("Node/path"));
			properties.push_back(SceneDebuggerProperty(pi, "[Orphan]"));
		}
	} else if (Script *s = Object::cast_to<Script>(obj)) {
		// Add script constants (no instance).
		_parse_script_properties(s, nullptr);
	}

	// Add base object properties.
	List<PropertyInfo> pinfo;
	obj->get_property_list(&pinfo, true);
	for (const PropertyInfo &E : pinfo) {
		if (E.usage & (PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CATEGORY)) {
			properties.push_back(SceneDebuggerProperty(E, obj->get(E.name)));
		}
	}
}

void SceneDebuggerObject::_parse_script_properties(Script *p_script, ScriptInstance *p_instance) {
	typedef HashMap<const Script *, HashSet<StringName>> ScriptMemberMap;
	typedef HashMap<const Script *, HashMap<StringName, Variant>> ScriptConstantsMap;

	ScriptMemberMap members;
	if (p_instance) {
		members[p_script] = HashSet<StringName>();
		p_script->get_members(&(members[p_script]));
	}

	ScriptConstantsMap constants;
	constants[p_script] = HashMap<StringName, Variant>();
	p_script->get_constants(&(constants[p_script]));

	Ref<Script> base = p_script->get_base_script();
	while (base.is_valid()) {
		if (p_instance) {
			members[base.ptr()] = HashSet<StringName>();
			base->get_members(&(members[base.ptr()]));
		}

		constants[base.ptr()] = HashMap<StringName, Variant>();
		base->get_constants(&(constants[base.ptr()]));

		base = base->get_base_script();
	}

	// Members
	for (KeyValue<const Script *, HashSet<StringName>> sm : members) {
		for (const StringName &E : sm.value) {
			Variant m;
			if (p_instance->get(E, m)) {
				String script_path = sm.key == p_script ? "" : sm.key->get_path().get_file() + "/";
				PropertyInfo pi(m.get_type(), "Members/" + script_path + E);
				properties.push_back(SceneDebuggerProperty(pi, m));
			}
		}
	}
	// Constants
	for (KeyValue<const Script *, HashMap<StringName, Variant>> &sc : constants) {
		for (const KeyValue<StringName, Variant> &E : sc.value) {
			String script_path = sc.key == p_script ? "" : sc.key->get_path().get_file() + "/";
			if (E.value.get_type() == Variant::OBJECT) {
				Variant inst_id = ((Object *)E.value)->get_instance_id();
				PropertyInfo pi(inst_id.get_type(), "Constants/" + E.key, PROPERTY_HINT_OBJECT_ID, "Object");
				properties.push_back(SceneDebuggerProperty(pi, inst_id));
			} else {
				PropertyInfo pi(E.value.get_type(), "Constants/" + script_path + E.key);
				properties.push_back(SceneDebuggerProperty(pi, E.value));
			}
		}
	}
}

void SceneDebuggerObject::serialize(Array &r_arr, int p_max_size) {
	Array send_props;
	for (SceneDebuggerObject::SceneDebuggerProperty &property : properties) {
		const PropertyInfo &pi = property.first;
		Variant &var = property.second;

		Ref<Resource> res = var;

		Array prop;
		prop.push_back(pi.name);
		prop.push_back(pi.type);

		PropertyHint hint = pi.hint;
		String hint_string = pi.hint_string;
		if (res.is_valid() && !res->get_path().is_empty()) {
			var = res->get_path();
		} else { //only send information that can be sent..
			int len = 0; //test how big is this to encode
			encode_variant(var, nullptr, len);
			if (len > p_max_size) { //limit to max size
				hint = PROPERTY_HINT_OBJECT_TOO_BIG;
				hint_string = "";
				var = Variant();
			}
		}
		prop.push_back(hint);
		prop.push_back(hint_string);
		prop.push_back(pi.usage);
		prop.push_back(var);
		send_props.push_back(prop);
	}
	r_arr.push_back(uint64_t(id));
	r_arr.push_back(class_name);
	r_arr.push_back(send_props);
}

void SceneDebuggerObject::deserialize(const Array &p_arr) {
#define CHECK_TYPE(p_what, p_type) ERR_FAIL_COND(p_what.get_type() != Variant::p_type);
	ERR_FAIL_COND(p_arr.size() < 3);
	CHECK_TYPE(p_arr[0], INT);
	CHECK_TYPE(p_arr[1], STRING);
	CHECK_TYPE(p_arr[2], ARRAY);

	id = uint64_t(p_arr[0]);
	class_name = p_arr[1];
	Array props = p_arr[2];

	for (int i = 0; i < props.size(); i++) {
		CHECK_TYPE(props[i], ARRAY);
		Array prop = props[i];

		ERR_FAIL_COND(prop.size() != 6);
		CHECK_TYPE(prop[0], STRING);
		CHECK_TYPE(prop[1], INT);
		CHECK_TYPE(prop[2], INT);
		CHECK_TYPE(prop[3], STRING);
		CHECK_TYPE(prop[4], INT);

		PropertyInfo pinfo;
		pinfo.name = prop[0];
		pinfo.type = Variant::Type(int(prop[1]));
		pinfo.hint = PropertyHint(int(prop[2]));
		pinfo.hint_string = prop[3];
		pinfo.usage = PropertyUsageFlags(int(prop[4]));
		Variant var = prop[5];

		if (pinfo.type == Variant::OBJECT) {
			if (var.is_zero()) {
				var = Ref<Resource>();
			} else if (var.get_type() == Variant::OBJECT) {
				if (((Object *)var)->is_class("EncodedObjectAsID")) {
					var = Object::cast_to<EncodedObjectAsID>(var)->get_object_id();
					pinfo.type = var.get_type();
					pinfo.hint = PROPERTY_HINT_OBJECT_ID;
					pinfo.hint_string = "Object";
				}
			}
		}
		properties.push_back(SceneDebuggerProperty(pinfo, var));
	}
}

/// SceneDebuggerTree
SceneDebuggerTree::SceneDebuggerTree(Node *p_root) {
	// Flatten tree into list, depth first, use stack to avoid recursion.
	List<Node *> stack;
	stack.push_back(p_root);
	bool is_root = true;
	const StringName &is_visible_sn = SNAME("is_visible");
	const StringName &is_visible_in_tree_sn = SNAME("is_visible_in_tree");
	while (stack.size()) {
		Node *n = stack.front()->get();
		stack.pop_front();

		int count = n->get_child_count();
		for (int i = 0; i < count; i++) {
			stack.push_front(n->get_child(count - i - 1));
		}

		int view_flags = 0;
		if (is_root) {
			// Prevent root window visibility from being changed.
			is_root = false;
		} else if (n->has_method(is_visible_sn)) {
			const Variant visible = n->call(is_visible_sn);
			if (visible.get_type() == Variant::BOOL) {
				view_flags = RemoteNode::VIEW_HAS_VISIBLE_METHOD;
				view_flags |= uint8_t(visible) * RemoteNode::VIEW_VISIBLE;
			}
			if (n->has_method(is_visible_in_tree_sn)) {
				const Variant visible_in_tree = n->call(is_visible_in_tree_sn);
				if (visible_in_tree.get_type() == Variant::BOOL) {
					view_flags |= uint8_t(visible_in_tree) * RemoteNode::VIEW_VISIBLE_IN_TREE;
				}
			}
		}

		String class_name;
		ScriptInstance *script_instance = n->get_script_instance();
		if (script_instance) {
			Ref<Script> script = script_instance->get_script();
			if (script.is_valid()) {
				class_name = script->get_global_name();

				if (class_name.is_empty()) {
					// If there is no class_name in this script we just take the script path.
					class_name = script->get_path();
				}
			}
		}
		nodes.push_back(RemoteNode(count, n->get_name(), class_name.is_empty() ? n->get_class() : class_name, n->get_instance_id(), n->get_scene_file_path(), view_flags));
	}
}

void SceneDebuggerTree::serialize(Array &p_arr) {
	for (const RemoteNode &n : nodes) {
		p_arr.push_back(n.child_count);
		p_arr.push_back(n.name);
		p_arr.push_back(n.type_name);
		p_arr.push_back(n.id);
		p_arr.push_back(n.scene_file_path);
		p_arr.push_back(n.view_flags);
	}
}

void SceneDebuggerTree::deserialize(const Array &p_arr) {
	int idx = 0;
	while (p_arr.size() > idx) {
		ERR_FAIL_COND(p_arr.size() < 6);
		CHECK_TYPE(p_arr[idx], INT); // child_count.
		CHECK_TYPE(p_arr[idx + 1], STRING); // name.
		CHECK_TYPE(p_arr[idx + 2], STRING); // type_name.
		CHECK_TYPE(p_arr[idx + 3], INT); // id.
		CHECK_TYPE(p_arr[idx + 4], STRING); // scene_file_path.
		CHECK_TYPE(p_arr[idx + 5], INT); // view_flags.
		nodes.push_back(RemoteNode(p_arr[idx], p_arr[idx + 1], p_arr[idx + 2], p_arr[idx + 3], p_arr[idx + 4], p_arr[idx + 5]));
		idx += 6;
	}
}

/// LiveEditor
LiveEditor *LiveEditor::get_singleton() {
	return singleton;
}

void LiveEditor::_send_tree() {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return;
	}

	Array arr;
	// Encoded as a flat list depth first.
	SceneDebuggerTree tree(scene_tree->root);
	tree.serialize(arr);
	EngineDebugger::get_singleton()->send_message("scene:scene_tree", arr);
}

void LiveEditor::_node_path_func(const NodePath &p_path, int p_id) {
	live_edit_node_path_cache[p_id] = p_path;
}

void LiveEditor::_res_path_func(const String &p_path, int p_id) {
	live_edit_resource_cache[p_id] = p_path;
}

void LiveEditor::_node_set_func(int p_id, const StringName &p_prop, const Variant &p_value) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return;
	}

	if (!live_edit_node_path_cache.has(p_id)) {
		return;
	}

	NodePath np = live_edit_node_path_cache[p_id];
	Node *base = nullptr;
	if (scene_tree->root->has_node(live_edit_root)) {
		base = scene_tree->root->get_node(live_edit_root);
	}

	HashMap<String, HashSet<Node *>>::Iterator E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Node *F : E->value) {
		Node *n = F;

		if (base && !base->is_ancestor_of(n)) {
			continue;
		}

		if (!n->has_node(np)) {
			continue;
		}
		Node *n2 = n->get_node(np);

		// Do not change transform of edited scene root, unless it's the scene being played.
		// See GH-86659 for additional context.
		bool keep_transform = (n2 == n) && (n2->get_parent() != scene_tree->root);
		Variant orig_tf;

		if (keep_transform) {
			if (n2->is_class("Node3D")) {
				orig_tf = n2->call("get_transform");
			} else if (n2->is_class("CanvasItem")) {
				orig_tf = n2->call("_edit_get_state");
			}
		}

		n2->set(p_prop, p_value);

		if (keep_transform) {
			if (n2->is_class("Node3D")) {
				Variant new_tf = n2->call("get_transform");
				if (new_tf != orig_tf) {
					n2->call("set_transform", orig_tf);
				}
			} else if (n2->is_class("CanvasItem")) {
				Variant new_tf = n2->call("_edit_get_state");
				if (new_tf != orig_tf) {
					n2->call("_edit_set_state", orig_tf);
				}
			}
		}
	}
}

void LiveEditor::_node_set_res_func(int p_id, const StringName &p_prop, const String &p_value) {
	Ref<Resource> r = ResourceLoader::load(p_value);
	if (r.is_null()) {
		return;
	}
	_node_set_func(p_id, p_prop, r);
}

void LiveEditor::_node_call_func(int p_id, const StringName &p_method, const Variant **p_args, int p_argcount) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return;
	}
	if (!live_edit_node_path_cache.has(p_id)) {
		return;
	}

	NodePath np = live_edit_node_path_cache[p_id];
	Node *base = nullptr;
	if (scene_tree->root->has_node(live_edit_root)) {
		base = scene_tree->root->get_node(live_edit_root);
	}

	HashMap<String, HashSet<Node *>>::Iterator E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Node *F : E->value) {
		Node *n = F;

		if (base && !base->is_ancestor_of(n)) {
			continue;
		}

		if (!n->has_node(np)) {
			continue;
		}
		Node *n2 = n->get_node(np);

		// Do not change transform of edited scene root, unless it's the scene being played.
		// See GH-86659 for additional context.
		bool keep_transform = (n2 == n) && (n2->get_parent() != scene_tree->root);
		Variant orig_tf;

		if (keep_transform) {
			if (n2->is_class("Node3D")) {
				orig_tf = n2->call("get_transform");
			} else if (n2->is_class("CanvasItem")) {
				orig_tf = n2->call("_edit_get_state");
			}
		}

		Callable::CallError ce;
		n2->callp(p_method, p_args, p_argcount, ce);

		if (keep_transform) {
			if (n2->is_class("Node3D")) {
				Variant new_tf = n2->call("get_transform");
				if (new_tf != orig_tf) {
					n2->call("set_transform", orig_tf);
				}
			} else if (n2->is_class("CanvasItem")) {
				Variant new_tf = n2->call("_edit_get_state");
				if (new_tf != orig_tf) {
					n2->call("_edit_set_state", orig_tf);
				}
			}
		}
	}
}

void LiveEditor::_res_set_func(int p_id, const StringName &p_prop, const Variant &p_value) {
	if (!live_edit_resource_cache.has(p_id)) {
		return;
	}

	String resp = live_edit_resource_cache[p_id];

	if (!ResourceCache::has(resp)) {
		return;
	}

	Ref<Resource> r = ResourceCache::get_ref(resp);
	if (r.is_null()) {
		return;
	}

	r->set(p_prop, p_value);
}

void LiveEditor::_res_set_res_func(int p_id, const StringName &p_prop, const String &p_value) {
	Ref<Resource> r = ResourceLoader::load(p_value);
	if (r.is_null()) {
		return;
	}
	_res_set_func(p_id, p_prop, r);
}

void LiveEditor::_res_call_func(int p_id, const StringName &p_method, const Variant **p_args, int p_argcount) {
	if (!live_edit_resource_cache.has(p_id)) {
		return;
	}

	String resp = live_edit_resource_cache[p_id];

	if (!ResourceCache::has(resp)) {
		return;
	}

	Ref<Resource> r = ResourceCache::get_ref(resp);
	if (r.is_null()) {
		return;
	}

	Callable::CallError ce;
	r->callp(p_method, p_args, p_argcount, ce);
}

void LiveEditor::_root_func(const NodePath &p_scene_path, const String &p_scene_from) {
	live_edit_root = p_scene_path;
	live_edit_scene = p_scene_from;
}

void LiveEditor::_create_node_func(const NodePath &p_parent, const String &p_type, const String &p_name) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return;
	}

	Node *base = nullptr;
	if (scene_tree->root->has_node(live_edit_root)) {
		base = scene_tree->root->get_node(live_edit_root);
	}

	HashMap<String, HashSet<Node *>>::Iterator E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Node *F : E->value) {
		Node *n = F;

		if (base && !base->is_ancestor_of(n)) {
			continue;
		}

		if (!n->has_node(p_parent)) {
			continue;
		}
		Node *n2 = n->get_node(p_parent);

		Node *no = Object::cast_to<Node>(ClassDB::instantiate(p_type));
		if (!no) {
			continue;
		}

		no->set_name(p_name);
		n2->add_child(no);
	}
}

void LiveEditor::_instance_node_func(const NodePath &p_parent, const String &p_path, const String &p_name) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return;
	}

	Ref<PackedScene> ps = ResourceLoader::load(p_path);

	if (ps.is_null()) {
		return;
	}

	Node *base = nullptr;
	if (scene_tree->root->has_node(live_edit_root)) {
		base = scene_tree->root->get_node(live_edit_root);
	}

	HashMap<String, HashSet<Node *>>::Iterator E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Node *F : E->value) {
		Node *n = F;

		if (base && !base->is_ancestor_of(n)) {
			continue;
		}

		if (!n->has_node(p_parent)) {
			continue;
		}
		Node *n2 = n->get_node(p_parent);

		Node *no = ps->instantiate();
		if (!no) {
			continue;
		}

		no->set_name(p_name);
		n2->add_child(no);
	}
}

void LiveEditor::_remove_node_func(const NodePath &p_at) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return;
	}

	Node *base = nullptr;
	if (scene_tree->root->has_node(live_edit_root)) {
		base = scene_tree->root->get_node(live_edit_root);
	}

	HashMap<String, HashSet<Node *>>::Iterator E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	Vector<Node *> to_delete;

	for (HashSet<Node *>::Iterator F = E->value.begin(); F; ++F) {
		Node *n = *F;

		if (base && !base->is_ancestor_of(n)) {
			continue;
		}

		if (!n->has_node(p_at)) {
			continue;
		}
		Node *n2 = n->get_node(p_at);

		to_delete.push_back(n2);
	}

	for (int i = 0; i < to_delete.size(); i++) {
		memdelete(to_delete[i]);
	}
}

void LiveEditor::_remove_and_keep_node_func(const NodePath &p_at, ObjectID p_keep_id) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return;
	}

	Node *base = nullptr;
	if (scene_tree->root->has_node(live_edit_root)) {
		base = scene_tree->root->get_node(live_edit_root);
	}

	HashMap<String, HashSet<Node *>>::Iterator E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	Vector<Node *> to_remove;
	for (HashSet<Node *>::Iterator F = E->value.begin(); F; ++F) {
		Node *n = *F;

		if (base && !base->is_ancestor_of(n)) {
			continue;
		}

		if (!n->has_node(p_at)) {
			continue;
		}

		to_remove.push_back(n);
	}

	for (int i = 0; i < to_remove.size(); i++) {
		Node *n = to_remove[i];
		Node *n2 = n->get_node(p_at);
		n2->get_parent()->remove_child(n2);
		live_edit_remove_list[n][p_keep_id] = n2;
	}
}

void LiveEditor::_restore_node_func(ObjectID p_id, const NodePath &p_at, int p_at_pos) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return;
	}

	Node *base = nullptr;
	if (scene_tree->root->has_node(live_edit_root)) {
		base = scene_tree->root->get_node(live_edit_root);
	}

	HashMap<String, HashSet<Node *>>::Iterator E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (HashSet<Node *>::Iterator F = E->value.begin(); F;) {
		HashSet<Node *>::Iterator N = F;
		++N;

		Node *n = *F;

		if (base && !base->is_ancestor_of(n)) {
			continue;
		}

		if (!n->has_node(p_at)) {
			continue;
		}
		Node *n2 = n->get_node(p_at);

		HashMap<Node *, HashMap<ObjectID, Node *>>::Iterator EN = live_edit_remove_list.find(n);

		if (!EN) {
			continue;
		}

		HashMap<ObjectID, Node *>::Iterator FN = EN->value.find(p_id);

		if (!FN) {
			continue;
		}
		n2->add_child(FN->value);

		EN->value.remove(FN);

		if (EN->value.size() == 0) {
			live_edit_remove_list.remove(EN);
		}

		F = N;
	}
}

void LiveEditor::_duplicate_node_func(const NodePath &p_at, const String &p_new_name) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return;
	}

	Node *base = nullptr;
	if (scene_tree->root->has_node(live_edit_root)) {
		base = scene_tree->root->get_node(live_edit_root);
	}

	HashMap<String, HashSet<Node *>>::Iterator E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Node *F : E->value) {
		Node *n = F;

		if (base && !base->is_ancestor_of(n)) {
			continue;
		}

		if (!n->has_node(p_at)) {
			continue;
		}
		Node *n2 = n->get_node(p_at);

		Node *dup = n2->duplicate(Node::DUPLICATE_SIGNALS | Node::DUPLICATE_GROUPS | Node::DUPLICATE_SCRIPTS);

		if (!dup) {
			continue;
		}

		dup->set_name(p_new_name);
		n2->get_parent()->add_child(dup);
	}
}

void LiveEditor::_reparent_node_func(const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree) {
		return;
	}

	Node *base = nullptr;
	if (scene_tree->root->has_node(live_edit_root)) {
		base = scene_tree->root->get_node(live_edit_root);
	}

	HashMap<String, HashSet<Node *>>::Iterator E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Node *F : E->value) {
		Node *n = F;

		if (base && !base->is_ancestor_of(n)) {
			continue;
		}

		if (!n->has_node(p_at)) {
			continue;
		}
		Node *nfrom = n->get_node(p_at);

		if (!n->has_node(p_new_place)) {
			continue;
		}
		Node *nto = n->get_node(p_new_place);

		nfrom->get_parent()->remove_child(nfrom);
		nfrom->set_name(p_new_name);

		nto->add_child(nfrom);
		if (p_at_pos >= 0) {
			nto->move_child(nfrom, p_at_pos);
		}
	}
}

/// RuntimeNodeSelect
RuntimeNodeSelect *RuntimeNodeSelect::get_singleton() {
	return singleton;
}

RuntimeNodeSelect::~RuntimeNodeSelect() {
	if (selection_list && !selection_list->is_visible()) {
		memdelete(selection_list);
	}

	if (draw_canvas.is_valid()) {
		RS::get_singleton()->free(sel_drag_ci);
		RS::get_singleton()->free(sbox_2d_ci);
		RS::get_singleton()->free(draw_canvas);
	}
}

void RuntimeNodeSelect::_setup(const Dictionary &p_settings) {
	Window *root = SceneTree::get_singleton()->get_root();
	ERR_FAIL_COND(root->is_connected(SceneStringName(window_input), callable_mp(this, &RuntimeNodeSelect::_root_window_input)));

	root->connect(SceneStringName(window_input), callable_mp(this, &RuntimeNodeSelect::_root_window_input));
	root->connect("size_changed", callable_mp(this, &RuntimeNodeSelect::_queue_selection_update), CONNECT_DEFERRED);

	max_selection = p_settings.get("debugger/max_node_selection", 1);

	panner.instantiate();
	panner->set_callbacks(callable_mp(this, &RuntimeNodeSelect::_pan_callback), callable_mp(this, &RuntimeNodeSelect::_zoom_callback));

	ViewPanner::ControlScheme panning_scheme = (ViewPanner::ControlScheme)p_settings.get("editors/panning/2d_editor_panning_scheme", 0).operator int();
	bool simple_panning = p_settings.get("editors/panning/simple_panning", false);
	int pan_speed = p_settings.get("editors/panning/2d_editor_pan_speed", 20);
	Array keys = p_settings.get("canvas_item_editor/pan_view", Array()).operator Array();
	panner->setup(panning_scheme, DebuggerMarshalls::deserialize_key_shortcut(keys), simple_panning);
	panner->setup_warped_panning(root, p_settings.get("editors/panning/warped_mouse_panning", true));
	panner->set_scroll_speed(pan_speed);

	sel_2d_grab_dist = p_settings.get("editors/polygon_editor/point_grab_radius", 0);

	selection_area_fill = p_settings.get("box_selection_fill_color", Color());
	selection_area_outline = p_settings.get("box_selection_stroke_color", Color());

	draw_canvas = RS::get_singleton()->canvas_create();
	sel_drag_ci = RS::get_singleton()->canvas_item_create();

	/// 2D Selection Box Generation

	sbox_2d_ci = RS::get_singleton()->canvas_item_create();
	RS::get_singleton()->viewport_attach_canvas(root->get_viewport_rid(), draw_canvas);
	RS::get_singleton()->canvas_item_set_parent(sel_drag_ci, draw_canvas);
	RS::get_singleton()->canvas_item_set_parent(sbox_2d_ci, draw_canvas);

#ifndef _3D_DISABLED
	cursor = Cursor();

	/// 3D Selection Box Generation
	// Copied from the Node3DEditor implementation.

	sbox_3d_color = p_settings.get("editors/3d/selection_box_color", Color());

	// Use two AABBs to create the illusion of a slightly thicker line.
	AABB aabb(Vector3(), Vector3(1, 1, 1));

	// Create a x-ray (visible through solid surfaces) and standard version of the selection box.
	// Both will be drawn at the same position, but with different opacity.
	// This lets the user see where the selection is while still having a sense of depth.
	Ref<SurfaceTool> st = memnew(SurfaceTool);
	Ref<SurfaceTool> st_xray = memnew(SurfaceTool);

	st->begin(Mesh::PRIMITIVE_LINES);
	st_xray->begin(Mesh::PRIMITIVE_LINES);
	for (int i = 0; i < 12; i++) {
		Vector3 a, b;
		aabb.get_edge(i, a, b);

		st->add_vertex(a);
		st->add_vertex(b);
		st_xray->add_vertex(a);
		st_xray->add_vertex(b);
	}

	Ref<StandardMaterial3D> mat = memnew(StandardMaterial3D);
	mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	mat->set_albedo(sbox_3d_color);
	mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	st->set_material(mat);
	sbox_3d_mesh = st->commit();

	Ref<StandardMaterial3D> mat_xray = memnew(StandardMaterial3D);
	mat_xray->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	mat_xray->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	mat_xray->set_flag(StandardMaterial3D::FLAG_DISABLE_DEPTH_TEST, true);
	mat_xray->set_albedo(sbox_3d_color * Color(1, 1, 1, 0.15));
	mat_xray->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
	st_xray->set_material(mat_xray);
	sbox_3d_mesh_xray = st_xray->commit();
#endif // _3D_DISABLED

	SceneTree::get_singleton()->connect("process_frame", callable_mp(this, &RuntimeNodeSelect::_process_frame));
	SceneTree::get_singleton()->connect("physics_frame", callable_mp(this, &RuntimeNodeSelect::_physics_frame));

	// This function will be called before the root enters the tree at first when the Game view is passing its settings to
	// the debugger, so queue the update for after it enters.
	root->connect(SceneStringName(tree_entered), callable_mp(this, &RuntimeNodeSelect::_update_input_state), Object::CONNECT_ONE_SHOT);
}

void RuntimeNodeSelect::_node_set_type(NodeType p_type) {
	node_select_type = p_type;
	_update_input_state();
}

void RuntimeNodeSelect::_select_set_mode(SelectMode p_mode) {
	node_select_mode = p_mode;
}

void RuntimeNodeSelect::_set_camera_override_enabled(bool p_enabled) {
	camera_override = p_enabled;

	if (p_enabled) {
		_update_view_2d();
	}

#ifndef _3D_DISABLED
	if (camera_first_override) {
		_reset_camera_2d();
		_reset_camera_3d();

		camera_first_override = false;
	} else if (p_enabled) {
		_update_view_2d();

		SceneTree::get_singleton()->get_root()->set_camera_3d_override_transform(_get_cursor_transform());
		SceneTree::get_singleton()->get_root()->set_camera_3d_override_perspective(CAMERA_BASE_FOV * cursor.fov_scale, CAMERA_ZNEAR, CAMERA_ZFAR);
	}
#endif // _3D_DISABLED
}

void RuntimeNodeSelect::_root_window_input(const Ref<InputEvent> &p_event) {
	Window *root = SceneTree::get_singleton()->get_root();
	if (node_select_type == NODE_TYPE_NONE || (selection_list && selection_list->is_visible())) {
		// Workaround for platforms that don't allow subwindows.
		if (selection_list && selection_list->is_visible() && selection_list->is_embedded()) {
			root->set_disable_input_override(false);
			selection_list->push_input(p_event);
			callable_mp(root->get_viewport(), &Viewport::set_disable_input_override).call_deferred(true);
		}

		return;
	}

	bool is_dragging_camera = false;
	if (camera_override) {
		if (node_select_type == NODE_TYPE_2D) {
			is_dragging_camera = panner->gui_input(p_event, Rect2(Vector2(), root->get_size()));
		} else if (node_select_type == NODE_TYPE_3D && selection_drag_state == SELECTION_DRAG_NONE) {
#ifndef _3D_DISABLED
			if (_handle_3d_input(p_event)) {
				return;
			}
#endif // _3D_DISABLED
		}
	}

	Ref<InputEventMouseButton> b = p_event;

	if (selection_drag_state == SELECTION_DRAG_MOVE) {
		Ref<InputEventMouseMotion> m = p_event;
		if (m.is_valid()) {
			_update_selection_drag(root->get_screen_transform().affine_inverse().xform(m->get_position()));
			return;
		} else if (b.is_valid()) {
			// Account for actions like zooming.
			_update_selection_drag(root->get_screen_transform().affine_inverse().xform(b->get_position()));
		}
	}

	if (b.is_null()) {
		return;
	}

	// Ignore mouse wheel inputs.
	if (b->get_button_index() != MouseButton::LEFT && b->get_button_index() != MouseButton::RIGHT) {
		return;
	}

	if (selection_drag_state == SELECTION_DRAG_MOVE && !b->is_pressed() && b->get_button_index() == MouseButton::LEFT) {
		selection_drag_state = SELECTION_DRAG_END;
		selection_drag_area = selection_drag_area.abs();
		_update_selection_drag();

		// Trigger a selection in the position on release.
		if (multi_shortcut_pressed) {
			selection_position = root->get_screen_transform().affine_inverse().xform(b->get_position());
		}
	}

	if (!is_dragging_camera && b->is_pressed()) {
		multi_shortcut_pressed = b->is_shift_pressed();
		list_shortcut_pressed = node_select_mode == SELECT_MODE_SINGLE && b->get_button_index() == MouseButton::RIGHT && b->is_alt_pressed();
		if (list_shortcut_pressed || b->get_button_index() == MouseButton::LEFT) {
			selection_position = root->get_screen_transform().affine_inverse().xform(b->get_position());
		}
	}
}

void RuntimeNodeSelect::_items_popup_index_pressed(int p_index, PopupMenu *p_popup) {
	Object *obj = p_popup->get_item_metadata(p_index).get_validated_object();
	if (obj) {
		Vector<Node *> node;
		node.append(Object::cast_to<Node>(obj));
		_send_ids(node);
	}
}

void RuntimeNodeSelect::_update_input_state() {
	SceneTree *scene_tree = SceneTree::get_singleton();
	// This function can be called at the very beginning, when the root hasn't entered the tree yet.
	// So check first to avoid a crash.
	if (!scene_tree->get_root()->is_inside_tree()) {
		return;
	}

	bool disable_input = scene_tree->is_suspended() || node_select_type != RuntimeNodeSelect::NODE_TYPE_NONE;
	Input::get_singleton()->set_disable_input(disable_input);
	Input::get_singleton()->set_mouse_mode_override_enabled(disable_input);
	scene_tree->get_root()->set_disable_input_override(disable_input);
}

void RuntimeNodeSelect::_process_frame() {
#ifndef _3D_DISABLED
	if (camera_freelook) {
		Transform3D transform = _get_cursor_transform();
		Vector3 forward = transform.basis.xform(Vector3(0, 0, -1));
		const Vector3 right = transform.basis.xform(Vector3(1, 0, 0));
		Vector3 up = transform.basis.xform(Vector3(0, 1, 0));

		Vector3 direction;

		Input *input = Input::get_singleton();
		bool was_input_disabled = input->is_input_disabled();
		if (was_input_disabled) {
			input->set_disable_input(false);
		}

		if (input->is_physical_key_pressed(Key::A)) {
			direction -= right;
		}
		if (input->is_physical_key_pressed(Key::D)) {
			direction += right;
		}
		if (input->is_physical_key_pressed(Key::W)) {
			direction += forward;
		}
		if (input->is_physical_key_pressed(Key::S)) {
			direction -= forward;
		}
		if (input->is_physical_key_pressed(Key::E)) {
			direction += up;
		}
		if (input->is_physical_key_pressed(Key::Q)) {
			direction -= up;
		}

		real_t speed = FREELOOK_BASE_SPEED;
		if (input->is_physical_key_pressed(Key::SHIFT)) {
			speed *= 3.0;
		}
		if (input->is_physical_key_pressed(Key::ALT)) {
			speed *= 0.333333;
		}

		if (was_input_disabled) {
			input->set_disable_input(true);
		}

		if (direction != Vector3()) {
			// Calculate the process time manually, as the time scale is frozen.
			const double process_time = (1.0 / Engine::get_singleton()->get_frames_per_second()) * Engine::get_singleton()->get_unfrozen_time_scale();
			const Vector3 motion = direction * speed * process_time;
			cursor.pos += motion;
			cursor.eye_pos += motion;

			SceneTree::get_singleton()->get_root()->set_camera_3d_override_transform(_get_cursor_transform());
		}
	}
#endif // _3D_DISABLED

	if (selection_update_queued || !SceneTree::get_singleton()->is_suspended()) {
		selection_update_queued = false;
		if (has_selection) {
			_update_selection();
		}
	}
}

void RuntimeNodeSelect::_physics_frame() {
	if (selection_drag_state != SELECTION_DRAG_END && (selection_drag_state == SELECTION_DRAG_MOVE || Math::is_inf(selection_position.x))) {
		return;
	}

	Window *root = SceneTree::get_singleton()->get_root();
	bool selection_drag_valid = selection_drag_state == SELECTION_DRAG_END && selection_drag_area.get_area() > SELECTION_MIN_AREA;
	Vector<SelectResult> items;

	if (node_select_type == NODE_TYPE_2D) {
		if (selection_drag_valid) {
			for (int i = 0; i < root->get_child_count(); i++) {
				_find_canvas_items_at_rect(selection_drag_area, root->get_child(i), items);
			}
		} else if (!Math::is_inf(selection_position.x)) {
			for (int i = 0; i < root->get_child_count(); i++) {
				_find_canvas_items_at_pos(selection_position, root->get_child(i), items);
			}
		}

		// Remove possible duplicates.
		for (int i = 0; i < items.size(); i++) {
			Node *item = items[i].item;
			for (int j = 0; j < i; j++) {
				if (items[j].item == item) {
					items.remove_at(i);
					i--;

					break;
				}
			}
		}
	} else if (node_select_type == NODE_TYPE_3D) {
#ifndef _3D_DISABLED
		if (selection_drag_valid) {
			_find_3d_items_at_rect(selection_drag_area, items);
		} else {
			_find_3d_items_at_pos(selection_position, items);
		}
#endif // _3D_DISABLED
	}

	items.sort();

	switch (selection_drag_state) {
		case SELECTION_DRAG_END: {
			selection_position = Point2(INFINITY, INFINITY);
			selection_drag_state = SELECTION_DRAG_NONE;

			if (selection_drag_area.get_area() > SELECTION_MIN_AREA) {
				if (!items.is_empty()) {
					Vector<Node *> nodes;
					for (const SelectResult item : items) {
						nodes.append(item.item);
					}
					_send_ids(nodes, false);
				}

				_update_selection_drag();
				return;
			}

			_update_selection_drag();
		} break;

		case SELECTION_DRAG_NONE: {
			if (node_select_mode == SELECT_MODE_LIST) {
				break;
			}

			if (multi_shortcut_pressed) {
				// Allow forcing box selection when an item was clicked.
				selection_drag_state = SELECTION_DRAG_MOVE;
			} else if (items.is_empty()) {
				if (!selected_ci_nodes.is_empty() || !selected_3d_nodes.is_empty()) {
					EngineDebugger::get_singleton()->send_message("remote_nothing_clicked", Array());
					_clear_selection();
				}

				selection_drag_state = SELECTION_DRAG_MOVE;
			} else {
				break;
			}

			[[fallthrough]];
		}

		case SELECTION_DRAG_MOVE: {
			selection_drag_area.position = selection_position;

			// Stop selection on click, so it can happen on release if the selection area doesn't pass the threshold.
			if (multi_shortcut_pressed) {
				return;
			}
		}
	}

	if (items.is_empty()) {
		selection_position = Point2(INFINITY, INFINITY);
		return;
	}
	if ((!list_shortcut_pressed && node_select_mode == SELECT_MODE_SINGLE) || items.size() == 1) {
		selection_position = Point2(INFINITY, INFINITY);

		Vector<Node *> node;
		node.append(items[0].item);
		_send_ids(node);

		return;
	}

	if (!selection_list && (list_shortcut_pressed || node_select_mode == SELECT_MODE_LIST)) {
		_open_selection_list(items, selection_position);
	}

	selection_position = Point2(INFINITY, INFINITY);
}

void RuntimeNodeSelect::_send_ids(const Vector<Node *> &p_picked_nodes, bool p_invert_new_selections) {
	ERR_FAIL_COND(p_picked_nodes.is_empty());

	Vector<Node *> picked_nodes = p_picked_nodes;
	Array message;

	if (!multi_shortcut_pressed) {
		if (picked_nodes.size() > max_selection) {
			picked_nodes.resize(max_selection);
			EngineDebugger::get_singleton()->send_message("show_selection_limit_warning", Array());
		}

		for (const Node *node : picked_nodes) {
			SceneDebuggerObject obj(node->get_instance_id());
			Array arr;
			obj.serialize(arr);
			message.append(arr);
		}

		EngineDebugger::get_singleton()->send_message("remote_nodes_clicked", message);
		_set_selected_nodes(picked_nodes);

		return;
	}

	const int limit = max_selection - (selected_ci_nodes.size() + selected_3d_nodes.size());
	if (limit <= 0) {
		return;
	}
	if (picked_nodes.size() > limit) {
		picked_nodes.resize(limit);
		EngineDebugger::get_singleton()->send_message("show_selection_limit_warning", Array());
	}

	LocalVector<Node *> nodes;
	LocalVector<ObjectID> ids;
	for (Node *node : picked_nodes) {
		ObjectID id = node->get_instance_id();
		if (CanvasItem *ci = Object::cast_to<CanvasItem>(node)) {
			if (selected_ci_nodes.has(id)) {
				if (p_invert_new_selections) {
					selected_ci_nodes.erase(id);
				}
			} else {
				ids.push_back(id);
				nodes.push_back(ci);
			}
		} else {
#ifndef _3D_DISABLED
			if (Node3D *node3d = Object::cast_to<Node3D>(node)) {
				if (selected_3d_nodes.has(id)) {
					if (p_invert_new_selections) {
						selected_3d_nodes.erase(id);
					}
				} else {
					ids.push_back(id);
					nodes.push_back(node3d);
				}
			}
#endif // _3D_DISABLED
		}
	}

	for (ObjectID id : selected_ci_nodes) {
		ids.push_back(id);
		nodes.push_back(Object::cast_to<Node>(ObjectDB::get_instance(id)));
	}
#ifndef _3D_DISABLED
	for (const KeyValue<ObjectID, Ref<SelectionBox3D>> &KV : selected_3d_nodes) {
		ids.push_back(KV.key);
		nodes.push_back(Object::cast_to<Node>(ObjectDB::get_instance(KV.key)));
	}
#endif // _3D_DISABLED

	if (ids.size() > (unsigned)max_selection) {
		ids.resize(max_selection);
		EngineDebugger::get_singleton()->send_message("show_selection_limit_warning", Array());
	}

	if (ids.is_empty()) {
		EngineDebugger::get_singleton()->send_message("remote_nothing_clicked", message);
	} else {
		for (const ObjectID &id : ids) {
			SceneDebuggerObject obj(id);
			Array arr;
			obj.serialize(arr);
			message.append(arr);
		}

		EngineDebugger::get_singleton()->send_message("remote_nodes_clicked", message);
	}

	_set_selected_nodes(nodes);
}

void RuntimeNodeSelect::_set_selected_nodes(const Vector<Node *> &p_nodes) {
	if (p_nodes.is_empty()) {
		_clear_selection();
		return;
	}

	bool changed = false;
	LocalVector<ObjectID> nodes_ci;
#ifndef _3D_DISABLED
	HashMap<ObjectID, Ref<SelectionBox3D>> nodes_3d;
#endif // _3D_DISABLED

	for (Node *node : p_nodes) {
		ObjectID id = node->get_instance_id();
		if (Object::cast_to<CanvasItem>(node)) {
			if (!changed || !selected_ci_nodes.has(id)) {
				changed = true;
			}

			nodes_ci.push_back(id);
		} else {
#ifndef _3D_DISABLED
			Node3D *node_3d = Object::cast_to<Node3D>(node);
			if (!node_3d || !node_3d->is_inside_world()) {
				continue;
			}

			if (!changed || !selected_3d_nodes.has(id)) {
				changed = true;
			}

			if (selected_3d_nodes.has(id)) {
				// Assign an already available visual instance.
				nodes_3d[id] = selected_3d_nodes.get(id);
				continue;
			}

			if (sbox_3d_mesh.is_null() || sbox_3d_mesh_xray.is_null()) {
				continue;
			}

			Ref<SelectionBox3D> sb;
			sb.instantiate();
			nodes_3d[id] = sb;

			RID scenario = node_3d->get_world_3d()->get_scenario();

			sb->instance = RS::get_singleton()->instance_create2(sbox_3d_mesh->get_rid(), scenario);
			sb->instance_ofs = RS::get_singleton()->instance_create2(sbox_3d_mesh->get_rid(), scenario);
			RS::get_singleton()->instance_geometry_set_cast_shadows_setting(sb->instance, RS::SHADOW_CASTING_SETTING_OFF);
			RS::get_singleton()->instance_geometry_set_cast_shadows_setting(sb->instance_ofs, RS::SHADOW_CASTING_SETTING_OFF);
			RS::get_singleton()->instance_geometry_set_flag(sb->instance, RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
			RS::get_singleton()->instance_geometry_set_flag(sb->instance, RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);
			RS::get_singleton()->instance_geometry_set_flag(sb->instance_ofs, RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
			RS::get_singleton()->instance_geometry_set_flag(sb->instance_ofs, RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);

			sb->instance_xray = RS::get_singleton()->instance_create2(sbox_3d_mesh_xray->get_rid(), scenario);
			sb->instance_xray_ofs = RS::get_singleton()->instance_create2(sbox_3d_mesh_xray->get_rid(), scenario);
			RS::get_singleton()->instance_geometry_set_cast_shadows_setting(sb->instance_xray, RS::SHADOW_CASTING_SETTING_OFF);
			RS::get_singleton()->instance_geometry_set_cast_shadows_setting(sb->instance_xray_ofs, RS::SHADOW_CASTING_SETTING_OFF);
			RS::get_singleton()->instance_geometry_set_flag(sb->instance_xray, RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
			RS::get_singleton()->instance_geometry_set_flag(sb->instance_xray, RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);
			RS::get_singleton()->instance_geometry_set_flag(sb->instance_xray_ofs, RS::INSTANCE_FLAG_IGNORE_OCCLUSION_CULLING, true);
			RS::get_singleton()->instance_geometry_set_flag(sb->instance_xray_ofs, RS::INSTANCE_FLAG_USE_BAKED_LIGHT, false);
#endif // _3D_DISABLED
		}
	}

	if (!changed && nodes_ci.size() == selected_ci_nodes.size() && nodes_3d.size() == selected_3d_nodes.size()) {
		return;
	}

	_clear_selection();
	selected_ci_nodes = nodes_ci;
	has_selection = !nodes_ci.is_empty();

#ifndef _3D_DISABLED
	if (!nodes_3d.is_empty()) {
		selected_3d_nodes = nodes_3d;
		has_selection = true;
	}
#endif // _3D_DISABLED

	_queue_selection_update();
}

void RuntimeNodeSelect::_queue_selection_update() {
	if (has_selection && selection_visible) {
		if (SceneTree::get_singleton()->is_suspended()) {
			_update_selection();
		} else {
			selection_update_queued = true;
		}
	}
}

void RuntimeNodeSelect::_update_selection() {
	Window *root = SceneTree::get_singleton()->get_root();

	RS::get_singleton()->canvas_item_clear(sbox_2d_ci);
	RS::get_singleton()->canvas_item_set_visible(sbox_2d_ci, selection_visible);

	for (LocalVector<ObjectID>::Iterator E = selected_ci_nodes.begin(); E != selected_ci_nodes.end(); ++E) {
		ObjectID id = *E;
		CanvasItem *ci = Object::cast_to<CanvasItem>(ObjectDB::get_instance(id));
		if (!ci) {
			selected_ci_nodes.erase(id);
			--E;
			continue;
		}

		Transform2D xform;
		// Cameras (overridden or not) don't affect `CanvasLayer`s.
		if (root->is_canvas_transform_override_enabled() && !(ci->get_canvas_layer_node() && !ci->get_canvas_layer_node()->is_following_viewport())) {
			xform = root->get_canvas_transform_override() * ci->get_global_transform();
		} else {
			xform = ci->get_global_transform_with_canvas();
		}

		// Fallback.
		Rect2 rect = Rect2(Vector2(), Vector2(10, 10));

		if (ci->_edit_use_rect()) {
			rect = ci->_edit_get_rect();
		} else {
			CollisionShape2D *collision_shape = Object::cast_to<CollisionShape2D>(ci);
			if (collision_shape) {
				Ref<Shape2D> shape = collision_shape->get_shape();
				if (shape.is_valid()) {
					rect = shape->get_rect();
				}
			}
		}

		const Vector2 endpoints[4] = {
			xform.xform(rect.position),
			xform.xform(rect.position + Point2(rect.size.x, 0)),
			xform.xform(rect.position + rect.size),
			xform.xform(rect.position + Point2(0, rect.size.y))
		};

		const Color selection_color_2d = Color(1, 0.6, 0.4, 0.7);
		for (int i = 0; i < 4; i++) {
			RS::get_singleton()->canvas_item_add_line(sbox_2d_ci, endpoints[i], endpoints[(i + 1) % 4], selection_color_2d, 2);
		}
	}

#ifndef _3D_DISABLED
	for (HashMap<ObjectID, Ref<SelectionBox3D>>::ConstIterator KV = selected_3d_nodes.begin(); KV != selected_3d_nodes.end(); ++KV) {
		ObjectID id = KV->key;
		Node3D *node_3d = Object::cast_to<Node3D>(ObjectDB::get_instance(id));
		if (!node_3d) {
			selected_3d_nodes.erase(id);
			--KV;
			continue;
		}

		// Fallback.
		AABB bounds(Vector3(-0.5, -0.5, -0.5), Vector3(1, 1, 1));

		VisualInstance3D *visual_instance = Object::cast_to<VisualInstance3D>(node_3d);
		if (visual_instance) {
			bounds = visual_instance->get_aabb();
		} else {
			CollisionShape3D *collision_shape = Object::cast_to<CollisionShape3D>(node_3d);
			if (collision_shape) {
				Ref<Shape3D> shape = collision_shape->get_shape();
				if (shape.is_valid()) {
					bounds = shape->get_debug_mesh()->get_aabb();
				}
			}
		}

		Transform3D xform_to_top_level_parent_space = node_3d->get_global_transform().affine_inverse() * node_3d->get_global_transform();
		bounds = xform_to_top_level_parent_space.xform(bounds);
		Transform3D t = node_3d->get_global_transform();

		Ref<SelectionBox3D> sb = KV->value;
		if (t == sb->transform && bounds == sb->bounds) {
			continue; // Nothing changed.
		}
		sb->transform = t;
		sb->bounds = bounds;

		Transform3D t_offset = t;

		// Apply AABB scaling before item's global transform.
		{
			const Vector3 offset(0.005, 0.005, 0.005);
			Basis aabb_s;
			aabb_s.scale(bounds.size + offset);
			t.translate_local(bounds.position - offset / 2);
			t.basis = t.basis * aabb_s;
		}
		{
			const Vector3 offset(0.01, 0.01, 0.01);
			Basis aabb_s;
			aabb_s.scale(bounds.size + offset);
			t_offset.translate_local(bounds.position - offset / 2);
			t_offset.basis = t_offset.basis * aabb_s;
		}

		RS::get_singleton()->instance_set_visible(sb->instance, selection_visible);
		RS::get_singleton()->instance_set_visible(sb->instance_ofs, selection_visible);
		RS::get_singleton()->instance_set_visible(sb->instance_xray, selection_visible);
		RS::get_singleton()->instance_set_visible(sb->instance_xray_ofs, selection_visible);

		RS::get_singleton()->instance_set_transform(sb->instance, t);
		RS::get_singleton()->instance_set_transform(sb->instance_ofs, t_offset);
		RS::get_singleton()->instance_set_transform(sb->instance_xray, t);
		RS::get_singleton()->instance_set_transform(sb->instance_xray_ofs, t_offset);
	}
#endif // _3D_DISABLED
}

void RuntimeNodeSelect::_clear_selection() {
	selected_ci_nodes.clear();
	if (draw_canvas.is_valid()) {
		RS::get_singleton()->canvas_item_clear(sbox_2d_ci);
	}

#ifndef _3D_DISABLED
	selected_3d_nodes.clear();
#endif // _3D_DISABLED

	has_selection = false;
}

void RuntimeNodeSelect::_update_selection_drag(const Point2 &p_end_pos) {
	RS::get_singleton()->canvas_item_clear(sel_drag_ci);

	if (selection_drag_state != SELECTION_DRAG_MOVE) {
		return;
	}

	selection_drag_area.size = p_end_pos - selection_drag_area.position;

	if (selection_drag_state == SELECTION_DRAG_END) {
		return;
	}

	Window *root = SceneTree::get_singleton()->get_root();
	Rect2 selection_drawing;
	int thickness;
	if (root->is_canvas_transform_override_enabled()) {
		Transform2D xform = root->get_canvas_transform_override();
		RS::get_singleton()->canvas_item_set_transform(sel_drag_ci, xform);

		selection_drawing.position = xform.affine_inverse().xform(selection_drag_area.position);
		selection_drawing.size = xform.affine_inverse().xform(p_end_pos);
		thickness = MAX(1, Math::ceil(1 / view_2d_zoom));
	} else {
		RS::get_singleton()->canvas_item_set_transform(sel_drag_ci, Transform2D());

		selection_drawing.position = selection_drag_area.position;
		selection_drawing.size = p_end_pos;
		thickness = 1;
	}
	selection_drawing.size -= selection_drawing.position;
	selection_drawing = selection_drawing.abs();

	const Vector2 endpoints[4] = {
		selection_drawing.position,
		selection_drawing.position + Point2(selection_drawing.size.x, 0),
		selection_drawing.position + selection_drawing.size,
		selection_drawing.position + Point2(0, selection_drawing.size.y)
	};

	// Draw fill.
	RS::get_singleton()->canvas_item_add_rect(sel_drag_ci, selection_drawing, selection_area_fill);
	// Draw outline.
	for (int i = 0; i < 4; i++) {
		RS::get_singleton()->canvas_item_add_line(sel_drag_ci, endpoints[i], endpoints[(i + 1) % 4], selection_area_outline, thickness);
	}
}

void RuntimeNodeSelect::_open_selection_list(const Vector<SelectResult> &p_items, const Point2 &p_pos) {
	Window *root = SceneTree::get_singleton()->get_root();

	selection_list = memnew(PopupMenu);
	selection_list->set_theme(ThemeDB::get_singleton()->get_default_theme());
	selection_list->set_auto_translate_mode(Node::AUTO_TRANSLATE_MODE_DISABLED);
	selection_list->set_force_native(true);
	selection_list->connect("index_pressed", callable_mp(this, &RuntimeNodeSelect::_items_popup_index_pressed).bind(selection_list));
	selection_list->connect("popup_hide", callable_mp(this, &RuntimeNodeSelect::_close_selection_list));

	root->add_child(selection_list);

	for (const SelectResult &I : p_items) {
		selection_list->add_item(I.item->get_name());
		selection_list->set_item_metadata(-1, I.item);
	}

	selection_list->set_position(selection_list->is_embedded() ? p_pos : (Input::get_singleton()->get_mouse_position() + root->get_position()));
	selection_list->reset_size();
	selection_list->popup();
	// FIXME: Ugly hack that stops the popup from hiding when the button is released.
	selection_list->call_deferred(SNAME("set_position"), selection_list->get_position() + Point2(1, 0));
}

void RuntimeNodeSelect::_close_selection_list() {
	selection_list->queue_free();
	selection_list = nullptr;
}

void RuntimeNodeSelect::_set_selection_visible(bool p_visible) {
	selection_visible = p_visible;

	if (has_selection) {
		_update_selection();
	}
}

// Copied and trimmed from the CanvasItemEditor implementation.
void RuntimeNodeSelect::_find_canvas_items_at_pos(const Point2 &p_pos, Node *p_node, Vector<SelectResult> &r_items, const Transform2D &p_parent_xform, const Transform2D &p_canvas_xform) {
	if (!p_node || Object::cast_to<Viewport>(p_node)) {
		return;
	}

	CanvasItem *ci = Object::cast_to<CanvasItem>(p_node);
	for (int i = p_node->get_child_count() - 1; i >= 0; i--) {
		if (ci) {
			if (!ci->is_set_as_top_level()) {
				_find_canvas_items_at_pos(p_pos, p_node->get_child(i), r_items, p_parent_xform * ci->get_transform(), p_canvas_xform);
			} else {
				_find_canvas_items_at_pos(p_pos, p_node->get_child(i), r_items, ci->get_transform(), p_canvas_xform);
			}
		} else {
			CanvasLayer *cl = Object::cast_to<CanvasLayer>(p_node);
			_find_canvas_items_at_pos(p_pos, p_node->get_child(i), r_items, Transform2D(), cl ? cl->get_transform() : p_canvas_xform);
		}
	}

	if (!ci || !ci->is_visible_in_tree()) {
		return;
	}

	Transform2D xform = p_canvas_xform;
	if (!ci->is_set_as_top_level()) {
		xform *= p_parent_xform;
	}

	Window *root = SceneTree::get_singleton()->get_root();
	Point2 pos;
	// Cameras (overridden or not) don't affect `CanvasLayer`s.
	if (!ci->get_canvas_layer_node() || ci->get_canvas_layer_node()->is_following_viewport()) {
		pos = (root->is_canvas_transform_override_enabled() ? root->get_canvas_transform_override() : root->get_canvas_transform()).affine_inverse().xform(p_pos);
	} else {
		pos = p_pos;
	}

	xform = (xform * ci->get_transform()).affine_inverse();
	const real_t local_grab_distance = xform.basis_xform(Vector2(sel_2d_grab_dist, 0)).length() / view_2d_zoom;
	if (ci->_edit_is_selected_on_click(xform.xform(pos), local_grab_distance)) {
		SelectResult res;
		res.item = ci;
		res.order = ci->get_effective_z_index() + ci->get_canvas_layer();
		r_items.push_back(res);

		// If it's a shape, get the collision object it's from.
		// FIXME: If the collision object has multiple shapes, only the topmost will be above it in the list.
		if (Object::cast_to<CollisionShape2D>(ci) || Object::cast_to<CollisionPolygon2D>(ci)) {
			CollisionObject2D *collision_object = Object::cast_to<CollisionObject2D>(ci->get_parent());
			if (collision_object) {
				SelectResult res_col;
				res_col.item = ci->get_parent();
				res_col.order = collision_object->get_z_index() + ci->get_canvas_layer();
				r_items.push_back(res_col);
			}
		}
	}
}

// Copied and trimmed from the CanvasItemEditor implementation.
void RuntimeNodeSelect::_find_canvas_items_at_rect(const Rect2 &p_rect, Node *p_node, Vector<SelectResult> &r_items, const Transform2D &p_parent_xform, const Transform2D &p_canvas_xform) {
	if (!p_node || Object::cast_to<Viewport>(p_node)) {
		return;
	}

	CanvasItem *ci = Object::cast_to<CanvasItem>(p_node);
	for (int i = p_node->get_child_count() - 1; i >= 0; i--) {
		if (ci) {
			if (!ci->is_set_as_top_level()) {
				_find_canvas_items_at_rect(p_rect, p_node->get_child(i), r_items, p_parent_xform * ci->get_transform(), p_canvas_xform);
			} else {
				_find_canvas_items_at_rect(p_rect, p_node->get_child(i), r_items, ci->get_transform(), p_canvas_xform);
			}
		} else {
			CanvasLayer *cl = Object::cast_to<CanvasLayer>(p_node);
			_find_canvas_items_at_rect(p_rect, p_node->get_child(i), r_items, Transform2D(), cl ? cl->get_transform() : p_canvas_xform);
		}
	}

	if (!ci || !ci->is_visible_in_tree()) {
		return;
	}

	Transform2D xform = p_canvas_xform;
	if (!ci->is_set_as_top_level()) {
		xform *= p_parent_xform;
	}

	Window *root = SceneTree::get_singleton()->get_root();
	Rect2 rect;
	// Cameras (overridden or not) don't affect `CanvasLayer`s.
	if (!ci->get_canvas_layer_node() || ci->get_canvas_layer_node()->is_following_viewport()) {
		rect = (root->is_canvas_transform_override_enabled() ? root->get_canvas_transform_override() : root->get_canvas_transform()).affine_inverse().xform(p_rect);
	} else {
		rect = p_rect;
	}
	rect = (xform * ci->get_transform()).affine_inverse().xform(rect);

	bool selected = false;
	if (ci->_edit_use_rect()) {
		Rect2 ci_rect = ci->_edit_get_rect();
		if (rect.has_point(ci_rect.position) &&
				rect.has_point(ci_rect.position + Vector2(ci_rect.size.x, 0)) &&
				rect.has_point(ci_rect.position + Vector2(ci_rect.size.x, ci_rect.size.y)) &&
				rect.has_point(ci_rect.position + Vector2(0, ci_rect.size.y))) {
			selected = true;
		}
	} else if (rect.has_point(Point2())) {
		selected = true;
	}

	if (selected) {
		SelectResult res;
		res.item = ci;
		res.order = ci->get_effective_z_index() + ci->get_canvas_layer();
		r_items.push_back(res);
	}
}

void RuntimeNodeSelect::_pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event) {
	view_2d_offset.x -= p_scroll_vec.x / view_2d_zoom;
	view_2d_offset.y -= p_scroll_vec.y / view_2d_zoom;

	_update_view_2d();
}

// A very shallow copy of the same function inside CanvasItemEditor.
void RuntimeNodeSelect::_zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event) {
	real_t prev_zoom = view_2d_zoom;
	view_2d_zoom = CLAMP(view_2d_zoom * p_zoom_factor, VIEW_2D_MIN_ZOOM, VIEW_2D_MAX_ZOOM);

	Vector2 pos = SceneTree::get_singleton()->get_root()->get_screen_transform().affine_inverse().xform(p_origin);
	view_2d_offset += pos / prev_zoom - pos / view_2d_zoom;

	// We want to align in-scene pixels to screen pixels, this prevents blurry rendering
	// of small details (texts, lines).
	// This correction adds a jitter movement when zooming, so we correct only when the
	// zoom factor is an integer. (in the other cases, all pixels won't be aligned anyway)
	const real_t closest_zoom_factor = Math::round(view_2d_zoom);
	if (Math::is_zero_approx(view_2d_zoom - closest_zoom_factor)) {
		// Make sure scene pixel at view_offset is aligned on a screen pixel.
		Vector2 view_offset_int = view_2d_offset.floor();
		Vector2 view_offset_frac = view_2d_offset - view_offset_int;
		view_2d_offset = view_offset_int + (view_offset_frac * closest_zoom_factor).round() / closest_zoom_factor;
	}

	_update_view_2d();
}

void RuntimeNodeSelect::_reset_camera_2d() {
	view_2d_offset = -SceneTree::get_singleton()->get_root()->get_canvas_transform().get_origin();
	view_2d_zoom = 1;

	_update_view_2d();
}

void RuntimeNodeSelect::_update_view_2d() {
	Transform2D transform = Transform2D();
	transform.scale_basis(Size2(view_2d_zoom, view_2d_zoom));
	transform.columns[2] = -view_2d_offset * view_2d_zoom;

	SceneTree::get_singleton()->get_root()->set_canvas_transform_override(transform);

	_queue_selection_update();
}

#ifndef _3D_DISABLED
void RuntimeNodeSelect::_find_3d_items_at_pos(const Point2 &p_pos, Vector<SelectResult> &r_items) {
	Window *root = SceneTree::get_singleton()->get_root();

	Vector3 ray, pos, to;
	if (root->is_camera_3d_override_enabled()) {
		ray = root->camera_3d_override_project_ray_normal(p_pos);
		pos = root->camera_3d_override_project_ray_origin(p_pos);
		to = pos + ray * root->get_camera_3d_override_properties()["z_far"];
	} else {
		Camera3D *camera = root->get_camera_3d();
		if (!camera) {
			return;
		}

		ray = camera->project_ray_normal(p_pos);
		pos = camera->project_ray_origin(p_pos);
		to = pos + ray * camera->get_far();
	}

	// Start with physical objects.
	PhysicsDirectSpaceState3D *ss = root->get_world_3d()->get_direct_space_state();
	PhysicsDirectSpaceState3D::RayResult result;
	HashSet<RID> excluded;
	PhysicsDirectSpaceState3D::RayParameters ray_params;
	ray_params.from = pos;
	ray_params.to = to;
	ray_params.collide_with_areas = true;
	while (true) {
		ray_params.exclude = excluded;
		if (ss->intersect_ray(ray_params, result)) {
			SelectResult res;
			res.item = Object::cast_to<Node>(result.collider);
			res.order = -pos.distance_to(result.position);

			// Fetch collision shapes.
			CollisionObject3D *collision = Object::cast_to<CollisionObject3D>(result.collider);
			if (collision) {
				List<uint32_t> owners;
				collision->get_shape_owners(&owners);
				for (uint32_t &I : owners) {
					SelectResult res_shape;
					res_shape.item = Object::cast_to<Node>(collision->shape_owner_get_owner(I));
					res_shape.order = res.order;
					r_items.push_back(res_shape);
				}
			}

			r_items.push_back(res);

			excluded.insert(result.rid);
		} else {
			break;
		}
	}

	// Then go for the meshes.
	Vector<ObjectID> items = RS::get_singleton()->instances_cull_ray(pos, to, root->get_world_3d()->get_scenario());
	for (int i = 0; i < items.size(); i++) {
		Object *obj = ObjectDB::get_instance(items[i]);

		GeometryInstance3D *geo_instance = Object::cast_to<GeometryInstance3D>(obj);
		if (geo_instance) {
			Ref<TriangleMesh> mesh_collision = geo_instance->generate_triangle_mesh();

			if (mesh_collision.is_valid()) {
				Transform3D gt = geo_instance->get_global_transform();
				Transform3D ai = gt.affine_inverse();
				Vector3 point, normal;
				if (mesh_collision->intersect_ray(ai.xform(pos), ai.basis.xform(ray).normalized(), point, normal)) {
					SelectResult res;
					res.item = Object::cast_to<Node>(obj);
					res.order = -pos.distance_to(gt.xform(point));
					r_items.push_back(res);

					continue;
				}
			}
		}

		items.remove_at(i);
		i--;
	}
}

void RuntimeNodeSelect::_find_3d_items_at_rect(const Rect2 &p_rect, Vector<SelectResult> &r_items) {
	Window *root = SceneTree::get_singleton()->get_root();
	Camera3D *camera = root->get_camera_3d();
	if (!camera) {
		return;
	}

	bool cam_override = root->is_camera_3d_override_enabled();
	Vector3 cam_pos;
	Vector3 dist_pos;
	if (cam_override) {
		cam_pos = root->get_camera_3d_override_transform().origin;
		dist_pos = root->camera_3d_override_project_ray_origin(p_rect.position + p_rect.size / 2);
	} else {
		cam_pos = camera->get_global_position();
		dist_pos = camera->project_ray_origin(p_rect.position + p_rect.size / 2);
	}

	real_t znear, zfar = 0;
	real_t zofs = 5.0;
	if (cam_override) {
		HashMap<StringName, real_t> override_props = root->get_camera_3d_override_properties();
		znear = override_props["z_near"];
		zfar = override_props["z_far"];
		zofs -= znear;
	} else {
		znear = camera->get_near();
		zfar = camera->get_far();
		zofs -= znear;
	}
	zofs = MAX(0.0, zofs);

	const Point2 pos_end = p_rect.position + p_rect.size;
	Vector3 box[4] = {
		Vector3(
				MIN(p_rect.position.x, pos_end.x),
				MIN(p_rect.position.y, pos_end.y),
				zofs),
		Vector3(
				MAX(p_rect.position.x, pos_end.x),
				MIN(p_rect.position.y, pos_end.y),
				zofs),
		Vector3(
				MAX(p_rect.position.x, pos_end.x),
				MAX(p_rect.position.y, pos_end.y),
				zofs),
		Vector3(
				MIN(p_rect.position.x, pos_end.x),
				MAX(p_rect.position.y, pos_end.y),
				zofs)
	};

	Vector<Plane> frustum;
	for (int i = 0; i < 4; i++) {
		Vector3 a = _get_screen_to_space(box[i]);
		Vector3 b = _get_screen_to_space(box[(i + 1) % 4]);
		frustum.push_back(Plane(a, b, cam_pos));
	}

	Plane near_plane;
	// Get the camera normal.
	if (cam_override) {
		near_plane = Plane(root->get_camera_3d_override_transform().basis.get_column(2), cam_pos);
	} else {
		near_plane = Plane(camera->get_global_transform().basis.get_column(2), cam_pos);
	}
	near_plane.d -= znear;
	frustum.push_back(near_plane);

	Plane far_plane = -near_plane;
	far_plane.d += zfar;
	frustum.push_back(far_plane);

	Vector<Vector3> points = Geometry3D::compute_convex_mesh_points(&frustum[0], frustum.size());
	Ref<ConvexPolygonShape3D> shape;
	shape.instantiate();
	shape->set_points(points);

	// Start with physical objects.
	PhysicsDirectSpaceState3D *ss = root->get_world_3d()->get_direct_space_state();
	PhysicsDirectSpaceState3D::ShapeResult result;
	PhysicsDirectSpaceState3D::ShapeParameters shape_params;
	shape_params.shape_rid = shape->get_rid();
	shape_params.collide_with_areas = true;
	if (ss->intersect_shape(shape_params, &result, 32)) {
		SelectResult res;
		res.item = Object::cast_to<Node>(result.collider);
		res.order = -dist_pos.distance_to(Object::cast_to<Node3D>(res.item)->get_global_transform().origin);

		// Fetch collision shapes.
		CollisionObject3D *collision = Object::cast_to<CollisionObject3D>(result.collider);
		if (collision) {
			List<uint32_t> owners;
			collision->get_shape_owners(&owners);
			for (uint32_t &I : owners) {
				SelectResult res_shape;
				res_shape.item = Object::cast_to<Node>(collision->shape_owner_get_owner(I));
				res_shape.order = res.order;
				r_items.push_back(res_shape);
			}
		}

		r_items.push_back(res);
	}

	// Then go for the meshes.
	Vector<ObjectID> items = RS::get_singleton()->instances_cull_convex(frustum, root->get_world_3d()->get_scenario());
	for (int i = 0; i < items.size(); i++) {
		Object *obj = ObjectDB::get_instance(items[i]);
		GeometryInstance3D *geo_instance = Object::cast_to<GeometryInstance3D>(obj);
		if (geo_instance) {
			Ref<TriangleMesh> mesh_collision = geo_instance->generate_triangle_mesh();

			if (mesh_collision.is_valid()) {
				Transform3D gt = geo_instance->get_global_transform();
				Vector3 mesh_scale = gt.get_basis().get_scale();
				gt.orthonormalize();

				Transform3D it = gt.affine_inverse();

				Vector<Plane> transformed_frustum;
				int plane_count = frustum.size();
				transformed_frustum.resize(plane_count);

				for (int j = 0; j < plane_count; j++) {
					transformed_frustum.write[j] = it.xform(frustum[j]);
				}
				Vector<Vector3> convex_points = Geometry3D::compute_convex_mesh_points(transformed_frustum.ptr(), plane_count);
				if (mesh_collision->inside_convex_shape(transformed_frustum.ptr(), transformed_frustum.size(), convex_points.ptr(), convex_points.size(), mesh_scale)) {
					SelectResult res;
					res.item = Object::cast_to<Node>(obj);
					res.order = -dist_pos.distance_to(gt.origin);
					r_items.push_back(res);

					continue;
				}
			}
		}

		items.remove_at(i);
		i--;
	}
}

Vector3 RuntimeNodeSelect::_get_screen_to_space(const Vector3 &p_vector3) {
	Window *root = SceneTree::get_singleton()->get_root();
	Camera3D *camera = root->get_camera_3d();

	Size2 size = root->get_size();
	real_t znear = 0;

	Projection cm;
	Transform3D camera_transform;
	if (root->is_camera_3d_override_enabled()) {
		HashMap<StringName, real_t> override_props = root->get_camera_3d_override_properties();
		znear = override_props["z_near"];
		cm.set_perspective(override_props["fov"], size.aspect(), znear + p_vector3.z, override_props["z_far"]);

		camera_transform.translate_local(cursor.pos);
		camera_transform.basis.rotate(Vector3(1, 0, 0), -cursor.x_rot);
		camera_transform.basis.rotate(Vector3(0, 1, 0), -cursor.y_rot);
		camera_transform.translate_local(0, 0, cursor.distance);
	} else {
		znear = camera->get_near();
		cm.set_perspective(camera->get_fov(), size.aspect(), znear + p_vector3.z, camera->get_far());

		camera_transform = camera->get_camera_transform();
	}

	Vector2 screen_he = cm.get_viewport_half_extents();
	return camera_transform.xform(Vector3(((p_vector3.x / size.width) * 2.0 - 1.0) * screen_he.x, ((1.0 - (p_vector3.y / size.height)) * 2.0 - 1.0) * screen_he.y, -(znear + p_vector3.z)));
}

bool RuntimeNodeSelect::_handle_3d_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> b = p_event;
	if (b.is_valid()) {
		const real_t zoom_factor = 1.08 * b->get_factor();
		switch (b->get_button_index()) {
			case MouseButton::WHEEL_UP: {
				if (!camera_freelook) {
					_cursor_scale_distance(1.0 / zoom_factor);
				}

				return true;
			} break;
			case MouseButton::WHEEL_DOWN: {
				if (!camera_freelook) {
					_cursor_scale_distance(zoom_factor);
				}

				return true;
			} break;
			case MouseButton::RIGHT: {
				_set_camera_freelook_enabled(b->is_pressed());
				return true;
			} break;
			default: {
			}
		}
	}

	Ref<InputEventMouseMotion> m = p_event;
	if (m.is_valid()) {
		if (camera_freelook) {
			_cursor_look(m);
		} else if (m->get_button_mask().has_flag(MouseButtonMask::MIDDLE)) {
			if (m->is_shift_pressed()) {
				_cursor_pan(m);
			} else {
				_cursor_orbit(m);
			}
		}

		return true;
	}

	Ref<InputEventKey> k = p_event;
	if (k.is_valid()) {
		if (k->get_physical_keycode() == Key::ESCAPE) {
			_set_camera_freelook_enabled(false);
			return true;
		} else if (k->is_ctrl_pressed()) {
			switch (k->get_physical_keycode()) {
				case Key::EQUAL: {
					cursor.fov_scale = CLAMP(cursor.fov_scale - 0.05, CAMERA_MIN_FOV_SCALE, CAMERA_MAX_FOV_SCALE);
					SceneTree::get_singleton()->get_root()->set_camera_3d_override_perspective(CAMERA_BASE_FOV * cursor.fov_scale, CAMERA_ZNEAR, CAMERA_ZFAR);

					return true;
				} break;
				case Key::MINUS: {
					cursor.fov_scale = CLAMP(cursor.fov_scale + 0.05, CAMERA_MIN_FOV_SCALE, CAMERA_MAX_FOV_SCALE);
					SceneTree::get_singleton()->get_root()->set_camera_3d_override_perspective(CAMERA_BASE_FOV * cursor.fov_scale, CAMERA_ZNEAR, CAMERA_ZFAR);

					return true;
				} break;
				case Key::KEY_0: {
					cursor.fov_scale = 1;
					SceneTree::get_singleton()->get_root()->set_camera_3d_override_perspective(CAMERA_BASE_FOV, CAMERA_ZNEAR, CAMERA_ZFAR);

					return true;
				} break;
				default: {
				}
			}
		}
	}

	// TODO: Handle magnify and pan input gestures.

	return false;
}

void RuntimeNodeSelect::_set_camera_freelook_enabled(bool p_enabled) {
	camera_freelook = p_enabled;

	if (p_enabled) {
		// Make sure eye_pos is synced, because freelook referential is eye pos rather than orbit pos
		Vector3 forward = _get_cursor_transform().basis.xform(Vector3(0, 0, -1));
		cursor.eye_pos = cursor.pos - cursor.distance * forward;

		previous_mouse_position = SceneTree::get_singleton()->get_root()->get_mouse_position();

		// Hide mouse like in an FPS (warping doesn't work).
		Input::get_singleton()->set_mouse_mode_override(Input::MOUSE_MODE_CAPTURED);

	} else {
		// Restore mouse.
		Input::get_singleton()->set_mouse_mode_override(Input::MOUSE_MODE_VISIBLE);

		// Restore the previous mouse position when leaving freelook mode.
		// This is done because leaving `Input.MOUSE_MODE_CAPTURED` will center the cursor
		// due to OS limitations.
		Input::get_singleton()->warp_mouse(previous_mouse_position);
	}
}

void RuntimeNodeSelect::_cursor_scale_distance(real_t p_scale) {
	real_t min_distance = MAX(CAMERA_ZNEAR * 4, VIEW_3D_MIN_ZOOM);
	real_t max_distance = MIN(CAMERA_ZFAR / 4, VIEW_3D_MAX_ZOOM);
	cursor.distance = CLAMP(cursor.distance * p_scale, min_distance, max_distance);

	SceneTree::get_singleton()->get_root()->set_camera_3d_override_transform(_get_cursor_transform());
}

void RuntimeNodeSelect::_cursor_look(Ref<InputEventWithModifiers> p_event) {
	Window *root = SceneTree::get_singleton()->get_root();
	const Vector2 relative = Input::get_singleton()->warp_mouse_motion(p_event, Rect2(Vector2(), root->get_size()));
	const Transform3D prev_camera_transform = _get_cursor_transform();

	cursor.x_rot += relative.y * RADS_PER_PIXEL;
	// Clamp the Y rotation to roughly -90..90 degrees so the user can't look upside-down and end up disoriented.
	cursor.x_rot = CLAMP(cursor.x_rot, -1.57, 1.57);

	cursor.y_rot += relative.x * RADS_PER_PIXEL;

	// Look is like the opposite of Orbit: the focus point rotates around the camera.
	Transform3D camera_transform = _get_cursor_transform();
	Vector3 pos = camera_transform.xform(Vector3(0, 0, 0));
	Vector3 prev_pos = prev_camera_transform.xform(Vector3(0, 0, 0));
	Vector3 diff = prev_pos - pos;
	cursor.pos += diff;

	SceneTree::get_singleton()->get_root()->set_camera_3d_override_transform(_get_cursor_transform());
}

void RuntimeNodeSelect::_cursor_pan(Ref<InputEventWithModifiers> p_event) {
	Window *root = SceneTree::get_singleton()->get_root();
	// Reduce all sides of the area by 1, so warping works when windows are maximized/fullscreen.
	const Vector2 relative = Input::get_singleton()->warp_mouse_motion(p_event, Rect2(Vector2(1, 1), root->get_size() - Vector2(2, 2)));
	const real_t pan_speed = 1 / 150.0;

	Transform3D camera_transform;
	camera_transform.translate_local(cursor.pos);
	camera_transform.basis.rotate(Vector3(1, 0, 0), -cursor.x_rot);
	camera_transform.basis.rotate(Vector3(0, 1, 0), -cursor.y_rot);

	Vector3 translation(1 * -relative.x * pan_speed, relative.y * pan_speed, 0);
	translation *= cursor.distance / 4;
	camera_transform.translate_local(translation);
	cursor.pos = camera_transform.origin;

	SceneTree::get_singleton()->get_root()->set_camera_3d_override_transform(_get_cursor_transform());
}

void RuntimeNodeSelect::_cursor_orbit(Ref<InputEventWithModifiers> p_event) {
	Window *root = SceneTree::get_singleton()->get_root();
	// Reduce all sides of the area by 1, so warping works when windows are maximized/fullscreen.
	const Vector2 relative = Input::get_singleton()->warp_mouse_motion(p_event, Rect2(Vector2(1, 1), root->get_size() - Vector2(2, 2)));

	cursor.x_rot += relative.y * RADS_PER_PIXEL;
	// Clamp the Y rotation to roughly -90..90 degrees so the user can't look upside-down and end up disoriented.
	cursor.x_rot = CLAMP(cursor.x_rot, -1.57, 1.57);

	cursor.y_rot += relative.x * RADS_PER_PIXEL;

	SceneTree::get_singleton()->get_root()->set_camera_3d_override_transform(_get_cursor_transform());
}

Transform3D RuntimeNodeSelect::_get_cursor_transform() {
	Transform3D camera_transform;
	camera_transform.translate_local(cursor.pos);
	camera_transform.basis.rotate(Vector3(1, 0, 0), -cursor.x_rot);
	camera_transform.basis.rotate(Vector3(0, 1, 0), -cursor.y_rot);
	camera_transform.translate_local(0, 0, cursor.distance);

	return camera_transform;
}

void RuntimeNodeSelect::_reset_camera_3d() {
	camera_first_override = true;

	cursor = Cursor();
	Window *root = SceneTree::get_singleton()->get_root();
	Camera3D *camera = root->get_camera_3d();
	if (camera) {
		Transform3D transform = camera->get_global_transform();
		transform.translate_local(0, 0, -cursor.distance);
		cursor.pos = transform.origin;

		cursor.x_rot = -camera->get_global_rotation().x;
		cursor.y_rot = -camera->get_global_rotation().y;

		cursor.fov_scale = CLAMP(camera->get_fov() / CAMERA_BASE_FOV, CAMERA_MIN_FOV_SCALE, CAMERA_MAX_FOV_SCALE);
	} else {
		cursor.fov_scale = 1.0;
	}

	SceneTree::get_singleton()->get_root()->set_camera_3d_override_transform(_get_cursor_transform());
	SceneTree::get_singleton()->get_root()->set_camera_3d_override_perspective(CAMERA_BASE_FOV * cursor.fov_scale, CAMERA_ZNEAR, CAMERA_ZFAR);
}
#endif // _3D_DISABLED

#endif // DEBUG_ENABLED
