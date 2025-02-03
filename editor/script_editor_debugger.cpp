/**************************************************************************/
/*  script_editor_debugger.cpp                                            */
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

#include "script_editor_debugger.h"
#include "core/io/marshalls.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "core/ustring.h"
#include "core/version.h"
#include "editor/editor_log.h"
#include "editor/editor_property_name_processor.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/plugins/spatial_editor_plugin.h"
#include "editor_log.h"
#include "editor_network_profiler.h"
#include "editor_node.h"
#include "editor_profiler.h"
#include "editor_scale.h"
#include "editor_settings.h"
#include "main/performance.h"
#include "property_editor.h"
#include "scene/debugger/script_debugger_remote.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/separator.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/tree.h"
#include "scene/resources/packed_scene.h"

class ScriptEditorDebuggerVariables : public Object {
	GDCLASS(ScriptEditorDebuggerVariables, Object);

	List<PropertyInfo> props;
	Map<StringName, Variant> values;

protected:
	bool _set(const StringName &p_name, const Variant &p_value) {
		return false;
	}

	bool _get(const StringName &p_name, Variant &r_ret) const {
		if (!values.has(p_name)) {
			return false;
		}
		r_ret = values[p_name];
		return true;
	}
	void _get_property_list(List<PropertyInfo> *p_list) const {
		for (const List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
			p_list->push_back(E->get());
		}
	}

public:
	void clear() {
		props.clear();
		values.clear();
	}

	String get_var_value(const String &p_var) const {
		for (Map<StringName, Variant>::Element *E = values.front(); E; E = E->next()) {
			String v = E->key().operator String().get_slice("/", 1);
			if (v == p_var) {
				return E->get();
			}
		}

		return "";
	}

	void add_property(const String &p_name, const Variant &p_value, const PropertyHint &p_hint, const String p_hint_string) {
		PropertyInfo pinfo;
		pinfo.name = p_name;
		pinfo.type = p_value.get_type();
		pinfo.hint = p_hint;
		pinfo.hint_string = p_hint_string;
		props.push_back(pinfo);
		values[p_name] = p_value;
	}

	void update() {
		_change_notify();
	}

	ScriptEditorDebuggerVariables() {
	}
};

class ScriptEditorDebuggerInspectedObject : public Object {
	GDCLASS(ScriptEditorDebuggerInspectedObject, Object);

protected:
	bool _set(const StringName &p_name, const Variant &p_value) {
		if (!prop_values.has(p_name) || String(p_name).begins_with("Constants/")) {
			return false;
		}

		prop_values[p_name] = p_value;
		emit_signal("value_edited", p_name, p_value);
		return true;
	}

	bool _get(const StringName &p_name, Variant &r_ret) const {
		if (!prop_values.has(p_name)) {
			return false;
		}

		r_ret = prop_values[p_name];
		return true;
	}

	void _get_property_list(List<PropertyInfo> *p_list) const {
		p_list->clear(); // Sorry, no want category.
		for (const List<PropertyInfo>::Element *E = prop_list.front(); E; E = E->next()) {
			const PropertyInfo &prop = E->get();
			if (prop.name == "script") {
				// Skip the script property, it's always added by the non-virtual method.
				continue;
			}

			p_list->push_back(prop);
		}
	}

	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_title"), &ScriptEditorDebuggerInspectedObject::get_title);
		ClassDB::bind_method(D_METHOD("get_variant"), &ScriptEditorDebuggerInspectedObject::get_variant);
		ClassDB::bind_method(D_METHOD("clear"), &ScriptEditorDebuggerInspectedObject::clear);
		ClassDB::bind_method(D_METHOD("get_remote_object_id"), &ScriptEditorDebuggerInspectedObject::get_remote_object_id);

		ADD_SIGNAL(MethodInfo("value_edited"));
	}

public:
	String type_name;
	ObjectID remote_object_id;
	List<PropertyInfo> prop_list;
	Map<StringName, Variant> prop_values;

	ObjectID get_remote_object_id() {
		return remote_object_id;
	}

	String get_title() {
		if (remote_object_id) {
			return vformat(TTR("Remote %s:"), String(type_name)) + " " + itos(remote_object_id);
		} else {
			return "<null>";
		}
	}
	Variant get_variant(const StringName &p_name) {
		Variant var;
		_get(p_name, var);
		return var;
	}

	void clear() {
		prop_list.clear();
		prop_values.clear();
	}
	void update() {
		_change_notify();
	}
	void update_single(const char *p_prop) {
		_change_notify(p_prop);
	}

	ScriptEditorDebuggerInspectedObject() {
		remote_object_id = 0;
	}
};

void ScriptEditorDebugger::debug_copy() {
	String msg = reason->get_text();
	if (msg == "") {
		return;
	}
	OS::get_singleton()->set_clipboard(msg);
}

void ScriptEditorDebugger::debug_skip_breakpoints() {
	skip_breakpoints_value = !skip_breakpoints_value;
	if (skip_breakpoints_value) {
		skip_breakpoints->set_icon(get_icon("DebugSkipBreakpointsOn", "EditorIcons"));
	} else {
		skip_breakpoints->set_icon(get_icon("DebugSkipBreakpointsOff", "EditorIcons"));
	}

	if (connection.is_valid()) {
		Array msg;
		msg.push_back("set_skip_breakpoints");
		msg.push_back(skip_breakpoints_value);
		ppeer->put_var(msg);
	}
}

void ScriptEditorDebugger::debug_next() {
	ERR_FAIL_COND(!breaked);
	ERR_FAIL_COND(connection.is_null());
	ERR_FAIL_COND(!connection->is_connected_to_host());
	Array msg;
	msg.push_back("next");
	ppeer->put_var(msg);
	_clear_execution();
}
void ScriptEditorDebugger::debug_step() {
	ERR_FAIL_COND(!breaked);
	ERR_FAIL_COND(connection.is_null());
	ERR_FAIL_COND(!connection->is_connected_to_host());

	Array msg;
	msg.push_back("step");
	ppeer->put_var(msg);
	_clear_execution();
}

void ScriptEditorDebugger::debug_break() {
	ERR_FAIL_COND(breaked);
	ERR_FAIL_COND(connection.is_null());
	ERR_FAIL_COND(!connection->is_connected_to_host());

	Array msg;
	msg.push_back("break");
	ppeer->put_var(msg);
}

void ScriptEditorDebugger::debug_continue() {
	ERR_FAIL_COND(!breaked);
	ERR_FAIL_COND(connection.is_null());
	ERR_FAIL_COND(!connection->is_connected_to_host());

	OS::get_singleton()->enable_for_stealing_focus(EditorNode::get_singleton()->get_child_process_id());

	Array msg;
	_clear_execution();
	msg.push_back("continue");
	ppeer->put_var(msg);
}

void ScriptEditorDebugger::_scene_tree_folded(Object *obj) {
	if (updating_scene_tree) {
		return;
	}
	TreeItem *item = Object::cast_to<TreeItem>(obj);

	if (!item) {
		return;
	}

	ObjectID id = item->get_metadata(0);
	if (unfold_cache.has(id)) {
		unfold_cache.erase(id);
	} else {
		unfold_cache.insert(id);
	}
}

void ScriptEditorDebugger::_scene_tree_selected() {
	if (updating_scene_tree) {
		return;
	}
	TreeItem *item = inspect_scene_tree->get_selected();
	if (!item) {
		return;
	}

	inspected_object_id = item->get_metadata(0);

	Array msg;
	msg.push_back("inspect_object");
	msg.push_back(inspected_object_id);
	ppeer->put_var(msg);
}

void ScriptEditorDebugger::_scene_tree_rmb_selected(const Vector2 &p_position) {
	TreeItem *item = inspect_scene_tree->get_item_at_position(p_position);
	if (!item) {
		return;
	}

	item->select(0);

	item_menu->clear();
	item_menu->add_icon_item(get_icon("CreateNewSceneFrom", "EditorIcons"), TTR("Save Branch as Scene"), ITEM_MENU_SAVE_REMOTE_NODE);
	item_menu->add_icon_item(get_icon("CopyNodePath", "EditorIcons"), TTR("Copy Node Path"), ITEM_MENU_COPY_NODE_PATH);
	item_menu->set_global_position(get_global_mouse_position());
	item_menu->popup();
}

void ScriptEditorDebugger::_file_selected(const String &p_file) {
	switch (file_dialog_mode) {
		case SAVE_NODE: {
			Array msg;
			msg.push_back("save_node");
			msg.push_back(inspected_object_id);
			msg.push_back(p_file);
			ppeer->put_var(msg);
		} break;
		case SAVE_MONITORS_CSV: {
			Error err;
			FileAccessRef file = FileAccess::open(p_file, FileAccess::WRITE, &err);

			if (err != OK) {
				ERR_PRINT("Failed to open " + p_file);
				return;
			}
			Vector<String> line;
			line.resize(Performance::MONITOR_MAX);

			// signatures
			for (int i = 0; i < Performance::MONITOR_MAX; i++) {
				line.write[i] = Performance::get_singleton()->get_monitor_name(Performance::Monitor(i));
			}
			file->store_csv_line(line);

			// values
			List<Vector<float>>::Element *E = perf_history.back();
			while (E) {
				Vector<float> &perf_data = E->get();
				for (int i = 0; i < perf_data.size(); i++) {
					line.write[i] = String::num_real(perf_data[i]);
				}
				file->store_csv_line(line);
				E = E->prev();
			}
			file->store_string("\n");

			Vector<Vector<String>> profiler_data = profiler->get_data_as_csv();
			for (int i = 0; i < profiler_data.size(); i++) {
				file->store_csv_line(profiler_data[i]);
			}

		} break;
		case SAVE_VRAM_CSV: {
			Error err;
			FileAccessRef file = FileAccess::open(p_file, FileAccess::WRITE, &err);

			if (err != OK) {
				ERR_PRINT("Failed to open " + p_file);
				return;
			}

			Vector<String> headers;
			headers.resize(vmem_tree->get_columns());
			for (int i = 0; i < vmem_tree->get_columns(); ++i) {
				headers.write[i] = vmem_tree->get_column_title(i);
			}
			file->store_csv_line(headers);

			if (vmem_tree->get_root()) {
				TreeItem *ti = vmem_tree->get_root()->get_children();
				while (ti) {
					Vector<String> values;
					values.resize(vmem_tree->get_columns());
					for (int i = 0; i < vmem_tree->get_columns(); ++i) {
						values.write[i] = ti->get_text(i);
					}
					file->store_csv_line(values);

					ti = ti->get_next();
				}
			}
		} break;
	}
}

void ScriptEditorDebugger::_scene_tree_property_value_edited(const String &p_prop, const Variant &p_value) {
	Array msg;
	msg.push_back("set_object_property");
	msg.push_back(inspected_object_id);
	msg.push_back(p_prop);
	msg.push_back(p_value);
	ppeer->put_var(msg);
	inspect_edited_object_timeout = 0.7; //avoid annoyance, don't request soon after editing
}

void ScriptEditorDebugger::_scene_tree_property_select_object(ObjectID p_object) {
	inspected_object_id = p_object;
	Array msg;
	msg.push_back("inspect_object");
	msg.push_back(inspected_object_id);
	ppeer->put_var(msg);
}

void ScriptEditorDebugger::_scene_tree_request() {
	ERR_FAIL_COND(connection.is_null());
	ERR_FAIL_COND(!connection->is_connected_to_host());

	Array msg;
	msg.push_back("request_scene_tree");
	ppeer->put_var(msg);
}

/// Populates inspect_scene_tree recursively given data in nodes.
/// Nodes is an array containing 4 elements for each node, it follows this pattern:
/// nodes[i] == number of direct children of this node
/// nodes[i + 1] == node name
/// nodes[i + 2] == node class
/// nodes[i + 3] == node instance id
///
/// Returns the number of items parsed in nodes from current_index.
///
/// Given a nodes array like [R,A,B,C,D,E] the following Tree will be generated, assuming
/// filter is an empty String, R and A child count are 2, B is 1 and C, D and E are 0.
///
/// R
/// |-A
/// | |-B
/// | | |-C
/// | |
/// | |-D
/// |
/// |-E
///
int ScriptEditorDebugger::_update_scene_tree(TreeItem *parent, const Array &nodes, int current_index) {
	String filter = EditorNode::get_singleton()->get_scene_tree_dock()->get_filter();
	String item_text = nodes[current_index + 1];
	String item_type = nodes[current_index + 2];
	bool keep = filter.is_subsequence_ofi(item_text);

	TreeItem *item = inspect_scene_tree->create_item(parent);
	item->set_text(0, item_text);
	item->set_tooltip(0, TTR("Type:") + " " + item_type);
	ObjectID id = ObjectID(nodes[current_index + 3]);
	Ref<Texture> icon = EditorNode::get_singleton()->get_class_icon(nodes[current_index + 2], "");
	if (icon.is_valid()) {
		item->set_icon(0, icon);
	}
	item->set_metadata(0, id);

	bool scroll = false;
	if (id == inspected_object_id) {
		TreeItem *cti = item->get_parent();
		while (cti) {
			cti->set_collapsed(false);
			cti = cti->get_parent();
		}
		item->select(0);
		scroll = filter != last_filter;
	}

	// Set current item as collapsed if necessary
	if (parent) {
		if (!unfold_cache.has(id)) {
			item->set_collapsed(true);
		}
	}

	int children_count = nodes[current_index];
	// Tracks the total number of items parsed in nodes, this is used to skips nodes that
	// are not direct children of the current node since we can't know in advance the total
	// number of children, direct and not, of a node without traversing the nodes array previously.
	// Keeping track of this allows us to build our remote scene tree by traversing the node
	// array just once.
	int items_count = 1;
	for (int i = 0; i < children_count; i++) {
		// Called for each direct child of item.
		// Direct children of current item might not be adjacent so items_count must
		// be incremented by the number of items parsed until now, otherwise we would not
		// be able to access the next child of the current item.
		// items_count is multiplied by 4 since that's the number of elements in the nodes
		// array needed to represent a single node.
		items_count += _update_scene_tree(item, nodes, current_index + items_count * 4);
	}

	// If item has not children and should not be kept delete it
	if (!keep && !item->get_children() && parent) {
		parent->remove_child(item);
		memdelete(item);
	} else if (scroll) {
		inspect_scene_tree->call_deferred("scroll_to_item", item);
	}

	if (!parent) {
		last_filter = filter;
	}

	return items_count;
}

void ScriptEditorDebugger::_video_mem_request() {
	if (connection.is_null() || !connection->is_connected_to_host()) {
		// Video RAM usage is only available while a project is being debugged.
		return;
	}

	Array msg;
	msg.push_back("request_video_mem");
	ppeer->put_var(msg);
}

void ScriptEditorDebugger::_video_mem_export() {
	file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_dialog->clear_filters();
	file_dialog_mode = SAVE_VRAM_CSV;
	file_dialog->popup_centered_ratio();
}

Size2 ScriptEditorDebugger::get_minimum_size() const {
	Size2 ms = MarginContainer::get_minimum_size();
	ms.y = MAX(ms.y, 250 * EDSCALE);
	return ms;
}

void ScriptEditorDebugger::_parse_message(const String &p_msg, const Array &p_data) {
	if (p_msg == "debug_enter") {
		Array msg;
		msg.push_back("get_stack_dump");
		ppeer->put_var(msg);
		ERR_FAIL_COND(p_data.size() != 2);
		bool can_continue = p_data[0];
		String error = p_data[1];
		step->set_disabled(!can_continue);
		next->set_disabled(!can_continue);
		_set_reason_text(error, MESSAGE_ERROR);
		copy->set_disabled(false);
		breaked = true;
		dobreak->set_disabled(true);
		docontinue->set_disabled(false);
		emit_signal("breaked", true, can_continue);
		OS::get_singleton()->move_window_to_foreground();
		if (error != "") {
			tabs->set_current_tab(0);
		}
		profiler->set_enabled(false);
		EditorNode::get_singleton()->get_pause_button()->set_pressed(true);
		EditorNode::get_singleton()->make_bottom_panel_item_visible(this);
		_clear_remote_objects();

	} else if (p_msg == "debug_exit") {
		breaked = false;
		_clear_execution();
		copy->set_disabled(true);
		step->set_disabled(true);
		next->set_disabled(true);
		reason->set_text("");
		reason->set_tooltip("");
		back->set_disabled(true);
		forward->set_disabled(true);
		dobreak->set_disabled(false);
		docontinue->set_disabled(true);
		emit_signal("breaked", false, false, Variant());
		profiler->set_enabled(true);
		profiler->disable_seeking();
		EditorNode::get_singleton()->get_pause_button()->set_pressed(false);
	} else if (p_msg == "message:click_ctrl") {
		clicked_ctrl->set_text(p_data[0]);
		clicked_ctrl_type->set_text(p_data[1]);

	} else if (p_msg == "message:scene_tree") {
		inspect_scene_tree->clear();
		Map<int, TreeItem *> lv;

		updating_scene_tree = true;

		_update_scene_tree(nullptr, p_data, 0);

		updating_scene_tree = false;

		le_clear->set_disabled(false);
		le_set->set_disabled(false);
	} else if (p_msg == "message:inspect_object") {
		ScriptEditorDebuggerInspectedObject *debug_obj = nullptr;

		ObjectID id = p_data[0];
		String type = p_data[1];
		Array properties = p_data[2];

		if (remote_objects.has(id)) {
			debug_obj = remote_objects[id];
		} else {
			debug_obj = memnew(ScriptEditorDebuggerInspectedObject);
			debug_obj->remote_object_id = id;
			debug_obj->type_name = type;
			remote_objects[id] = debug_obj;
			debug_obj->connect("value_edited", this, "_scene_tree_property_value_edited");
		}

		int old_prop_size = debug_obj->prop_list.size();

		debug_obj->prop_list.clear();
		int new_props_added = 0;
		Set<String> changed;
		for (int i = 0; i < properties.size(); i++) {
			Array prop = properties[i];
			if (prop.size() != 6) {
				continue;
			}

			PropertyInfo pinfo;
			pinfo.name = prop[0];
			pinfo.type = Variant::Type(int(prop[1]));
			pinfo.hint = PropertyHint(int(prop[2]));
			pinfo.hint_string = prop[3];
			pinfo.usage = PropertyUsageFlags(int(prop[4]));
			Variant var = prop[5];

			if (pinfo.type == Variant::OBJECT) {
				if (var.is_zero()) {
					var = RES();
				} else if (var.get_type() == Variant::STRING) {
					String path = var;
					if (path.find("::") != -1) {
						// built-in resource
						String base_path = path.get_slice("::", 0);
						RES dependency = ResourceLoader::load(base_path);
						if (dependency.is_valid()) {
							remote_dependencies.insert(dependency);
						}
					}
					var = ResourceLoader::load(path);

					if (pinfo.hint_string == "Script") {
						if (debug_obj->get_script() != var) {
							debug_obj->set_script(RefPtr());
							Ref<Script> script(var);
							if (!script.is_null()) {
								ScriptInstance *script_instance = script->placeholder_instance_create(debug_obj);
								if (script_instance) {
									debug_obj->set_script_and_instance(var, script_instance);
								}
							}
						}
					}
				} else if (var.get_type() == Variant::OBJECT) {
					if (((Object *)var)->is_class("EncodedObjectAsID")) {
						var = Object::cast_to<EncodedObjectAsID>(var)->get_object_id();
						pinfo.type = var.get_type();
						pinfo.hint = PROPERTY_HINT_OBJECT_ID;
						pinfo.hint_string = "Object";
					}
				}
			}

			//always add the property, since props may have been added or removed
			debug_obj->prop_list.push_back(pinfo);

			if (!debug_obj->prop_values.has(pinfo.name)) {
				new_props_added++;
				debug_obj->prop_values[pinfo.name] = var;
			} else {
				// Compare using `deep_equal` so dictionaries/arrays will be compared by value.
				if (!debug_obj->prop_values[pinfo.name].deep_equal(var)) {
					debug_obj->prop_values[pinfo.name] = var;
					changed.insert(pinfo.name);
				}
			}
		}

		if (editor->get_editor_history()->get_current() != debug_obj->get_instance_id()) {
			editor->push_item(debug_obj, "");
		} else {
			if (old_prop_size == debug_obj->prop_list.size() && new_props_added == 0) {
				//only some may have changed, if so, then update those, if exist
				for (Set<String>::Element *E = changed.front(); E; E = E->next()) {
					EditorNode::get_singleton()->get_inspector()->update_property(E->get());
				}
			} else {
				//full update, because props were added or removed
				debug_obj->update();
			}
		}
	} else if (p_msg == "message:video_mem") {
		vmem_tree->clear();
		TreeItem *root = vmem_tree->create_item();

		uint64_t total = 0;

		for (int i = 0; i < p_data.size(); i += 4) {
			TreeItem *it = vmem_tree->create_item(root);
			String type = p_data[i + 1];
			int bytes = p_data[i + 3].operator int();
			it->set_text(0, p_data[i + 0]); //path
			it->set_text(1, type); //type
			it->set_text(2, p_data[i + 2]); //type
			it->set_text(3, String::humanize_size(bytes)); //type
			total += bytes;

			if (has_icon(type, "EditorIcons")) {
				it->set_icon(0, get_icon(type, "EditorIcons"));
			}
		}

		vmem_total->set_tooltip(TTR("Bytes:") + " " + itos(total));
		vmem_total->set_text(String::humanize_size(total));
	} else if (p_msg == "stack_dump") {
		stack_dump->clear();
		TreeItem *r = stack_dump->create_item();

		for (int i = 0; i < p_data.size(); i++) {
			Dictionary d = p_data[i];
			ERR_CONTINUE(!d.has("function"));
			ERR_CONTINUE(!d.has("file"));
			ERR_CONTINUE(!d.has("line"));
			ERR_CONTINUE(!d.has("id"));
			TreeItem *s = stack_dump->create_item(r);
			d["frame"] = i;
			s->set_metadata(0, d);

			String line = itos(i) + " - " + String(d["file"]) + ":" + itos(d["line"]) + " - at function: " + d["function"];
			s->set_text(0, line);

			if (i == 0) {
				s->select(0);
			}
		}
	} else if (p_msg == "stack_frame_vars") {
		variables->clear();

		int ofs = 0;
		int mcount = p_data[ofs];
		ofs++;
		for (int i = 0; i < mcount; i++) {
			String n = p_data[ofs + i * 2 + 0];
			Variant v = p_data[ofs + i * 2 + 1];

			PropertyHint h = PROPERTY_HINT_NONE;
			String hs = String();

			if (v.get_type() == Variant::OBJECT) {
				v = Object::cast_to<EncodedObjectAsID>(v)->get_object_id();
				h = PROPERTY_HINT_OBJECT_ID;
				hs = "Object";
			}

			variables->add_property("Locals/" + n, v, h, hs);
		}

		ofs += mcount * 2;
		mcount = p_data[ofs];
		ofs++;
		for (int i = 0; i < mcount; i++) {
			String n = p_data[ofs + i * 2 + 0];
			Variant v = p_data[ofs + i * 2 + 1];
			PropertyHint h = PROPERTY_HINT_NONE;
			String hs = String();

			if (v.get_type() == Variant::OBJECT) {
				v = Object::cast_to<EncodedObjectAsID>(v)->get_object_id();
				h = PROPERTY_HINT_OBJECT_ID;
				hs = "Object";
			}

			variables->add_property("Members/" + n, v, h, hs);

			if (n == "self") {
				_scene_tree_property_select_object(v);
			}
		}

		ofs += mcount * 2;
		mcount = p_data[ofs];
		ofs++;
		for (int i = 0; i < mcount; i++) {
			String n = p_data[ofs + i * 2 + 0];
			Variant v = p_data[ofs + i * 2 + 1];
			PropertyHint h = PROPERTY_HINT_NONE;
			String hs = String();

			if (v.get_type() == Variant::OBJECT) {
				v = Object::cast_to<EncodedObjectAsID>(v)->get_object_id();
				h = PROPERTY_HINT_OBJECT_ID;
				hs = "Object";
			}

			variables->add_property("Globals/" + n, v, h, hs);
		}

		variables->update();
		inspector->edit(variables);
	} else if (p_msg == "output") {
		//OUT
		for (int i = 0; i < p_data.size(); i++) {
			Array output = p_data[i];
			ERR_FAIL_COND_MSG(output.size() < 2, "Malformed output message from script debugger.");

			String str = output[0];
			ScriptDebuggerRemote::MessageType type = (ScriptDebuggerRemote::MessageType)(int)(output[1]);

			EditorLog::MessageType msg_type;
			switch (type) {
				case ScriptDebuggerRemote::MESSAGE_TYPE_LOG: {
					msg_type = EditorLog::MSG_TYPE_STD;
				} break;
				case ScriptDebuggerRemote::MESSAGE_TYPE_ERROR: {
					msg_type = EditorLog::MSG_TYPE_ERROR;
				} break;
				default: {
					WARN_PRINT("Unhandled script debugger message type: " + itos(type));
					msg_type = EditorLog::MSG_TYPE_STD;
				} break;
			}

			//LOG

			if (!EditorNode::get_log()->is_visible()) {
				if (EditorNode::get_singleton()->are_bottom_panels_hidden()) {
					if (EDITOR_GET("run/output/always_open_output_on_play")) {
						EditorNode::get_singleton()->make_bottom_panel_item_visible(EditorNode::get_log());
					}
				}
			}

			EditorNode::get_log()->add_message(str, msg_type);
		}
	} else if (p_msg == "performance") {
		Array arr = p_data[0];
		Vector<float> p;
		p.resize(arr.size());
		for (int i = 0; i < arr.size(); i++) {
			p.write[i] = arr[i];
			if (i < perf_items.size()) {
				const float value = p[i];
				String label = rtos(value);
				String tooltip = label;
				switch (Performance::MonitorType((int)perf_items[i]->get_metadata(1))) {
					case Performance::MONITOR_TYPE_MEMORY: {
						label = String::humanize_size(value);
						tooltip = label;
					} break;
					case Performance::MONITOR_TYPE_TIME: {
						label = rtos(value * 1000).pad_decimals(2) + " ms";
						tooltip = label;
					} break;
					default: {
						tooltip += " " + perf_items[i]->get_text(0);
					} break;
				}

				perf_items[i]->set_text(1, label);
				perf_items[i]->set_tooltip(1, tooltip);
				if (p[i] > perf_max[i]) {
					perf_max.write[i] = p[i];
				}
			}
		}
		perf_history.push_front(p);
		perf_draw->update();
	} else if (p_msg == "error") {
		// Should have at least two elements, error array and stack items count.
		ERR_FAIL_COND_MSG(p_data.size() < 2, "Malformed error message from script debugger.");

		// Error or warning data.
		Array err = p_data[0];
		ERR_FAIL_COND_MSG(err.size() < 10, "Malformed error message from script debugger.");

		// Format time.
		Array time_vals;
		time_vals.push_back(err[0]);
		time_vals.push_back(err[1]);
		time_vals.push_back(err[2]);
		time_vals.push_back(err[3]);
		bool e;
		String time = String("%d:%02d:%02d.%03d").sprintf(time_vals, &e);

		// Rest of the error data.
		String method = err[4];
		String source_file = err[5];
		String source_line = err[6];
		String error_cond = err[7];
		String error_msg = err[8];
		bool is_warning = err[9];
		bool has_method = !method.empty();
		bool has_error_msg = !error_msg.empty();
		bool source_is_project_file = source_file.begins_with("res://");

		// Metadata to highlight error line in scripts.
		Array source_meta;
		source_meta.push_back(source_file);
		source_meta.push_back(source_line);

		// Create error tree to display above error or warning details.
		TreeItem *r = error_tree->get_root();
		if (!r) {
			r = error_tree->create_item();
		}

		// Also provide the relevant details as tooltip to quickly check without
		// uncollapsing the tree.
		String tooltip = is_warning ? TTR("Warning:") : TTR("Error:");

		TreeItem *error = error_tree->create_item(r);
		error->set_collapsed(true);

		error->set_icon(0, get_icon(is_warning ? "Warning" : "Error", "EditorIcons"));
		error->set_text(0, time);
		error->set_text_align(0, TreeItem::ALIGN_LEFT);

		const Color color = get_color(is_warning ? "warning_color" : "error_color", "Editor");
		error->set_custom_color(0, color);
		error->set_custom_color(1, color);

		String error_title;
		// Include method name, when given, in error title.
		if (has_method) {
			error_title += method + ": ";
		}
		// If we have a (custom) error message, use it as title, and add a C++ Error
		// item with the original error condition.
		error_title += error_msg.empty() ? error_cond : error_msg;
		error->set_text(1, error_title);
		tooltip += " " + error_title + "\n";

		if (has_error_msg) {
			// Add item for C++ error condition.
			TreeItem *cpp_cond = error_tree->create_item(error);
			cpp_cond->set_text(0, "<" + TTR("C++ Error") + ">");
			cpp_cond->set_text(1, error_cond);
			cpp_cond->set_text_align(0, TreeItem::ALIGN_LEFT);
			tooltip += TTR("C++ Error:") + " " + error_cond + "\n";
			if (source_is_project_file) {
				cpp_cond->set_metadata(0, source_meta);
			}
		}

		// Source of the error.
		String source_txt = (source_is_project_file ? source_file.get_file() : source_file) + ":" + source_line;
		if (has_method) {
			source_txt += " @ " + method + "()";
		}

		TreeItem *cpp_source = error_tree->create_item(error);
		cpp_source->set_text(0, "<" + (source_is_project_file ? TTR("Source") : TTR("C++ Source")) + ">");
		cpp_source->set_text(1, source_txt);
		cpp_source->set_text_align(0, TreeItem::ALIGN_LEFT);
		tooltip += (source_is_project_file ? TTR("Source:") : TTR("C++ Source:")) + " " + source_txt + "\n";

		// Set metadata to highlight error line in scripts.
		if (source_is_project_file) {
			error->set_metadata(0, source_meta);
			cpp_source->set_metadata(0, source_meta);
		}

		error->set_tooltip(0, tooltip);
		error->set_tooltip(1, tooltip);

		// Format stack trace.
		// stack_items_count is the number of elements to parse, with 3 items per frame
		// of the stack trace (script, method, line).
		int stack_items_count = p_data[1];

		for (int i = 0; i < stack_items_count; i += 3) {
			String script = p_data[2 + i];
			String method2 = p_data[3 + i];
			int line = p_data[4 + i];
			TreeItem *stack_trace = error_tree->create_item(error);

			Array meta;
			meta.push_back(script);
			meta.push_back(line);
			stack_trace->set_metadata(0, meta);

			if (i == 0) {
				stack_trace->set_text(0, "<" + TTR("Stack Trace") + ">");
				stack_trace->set_text_align(0, TreeItem::ALIGN_LEFT);
				error->set_metadata(0, meta);
			}
			stack_trace->set_text(1, script.get_file() + ":" + itos(line) + " @ " + method2 + "()");
		}

		if (is_warning) {
			warning_count++;
		} else {
			error_count++;
		}
	} else if (p_msg == "profile_sig") {
		//cache a signature
		profiler_signature[p_data[1]] = p_data[0];
	} else if (p_msg == "profile_frame" || p_msg == "profile_total") {
		EditorProfiler::Metric metric;
		metric.valid = true;
		metric.frame_number = p_data[0];
		metric.frame_time = p_data[1];
		metric.process_time = p_data[2];
		metric.physics_time = p_data[3];
		metric.physics_frame_time = p_data[4];
		int frame_data_amount = p_data[6];
		int frame_function_amount = p_data[7];

		if (frame_data_amount) {
			EditorProfiler::Metric::Category frame_time;
			frame_time.signature = "category_frame_time";
			frame_time.name = "Frame Time";
			frame_time.total_time = metric.frame_time;

			EditorProfiler::Metric::Category::Item item;
			item.calls = 1;
			item.line = 0;

			item.name = "Physics Time";
			item.total = metric.physics_time;
			item.self = item.total;
			item.signature = "physics_time";

			frame_time.items.push_back(item);

			item.name = "Process Time";
			item.total = metric.process_time;
			item.self = item.total;
			item.signature = "process_time";

			frame_time.items.push_back(item);

			item.name = "Physics Frame Time";
			item.total = metric.physics_frame_time;
			item.self = item.total;
			item.signature = "physics_frame_time";

			frame_time.items.push_back(item);

			metric.categories.push_back(frame_time);
		}

		int idx = 8;
		for (int i = 0; i < frame_data_amount; i++) {
			EditorProfiler::Metric::Category c;
			String name = p_data[idx++];
			Array values = p_data[idx++];
			c.name = EditorPropertyNameProcessor::get_singleton()->process_name(name, EditorPropertyNameProcessor::STYLE_CAPITALIZED);
			c.items.resize(values.size() / 2);
			c.total_time = 0;
			c.signature = "categ::" + name;
			for (int j = 0; j < values.size(); j += 2) {
				EditorProfiler::Metric::Category::Item item;
				item.calls = 1;
				item.line = 0;
				item.name = values[j];
				item.self = values[j + 1];
				item.total = item.self;
				item.signature = "categ::" + name + "::" + item.name;
				item.name = item.name.capitalize();
				c.total_time += item.total;
				c.items.write[j / 2] = item;
			}
			metric.categories.push_back(c);
		}

		EditorProfiler::Metric::Category funcs;
		funcs.total_time = p_data[5]; //script time
		funcs.items.resize(frame_function_amount);
		funcs.name = "Script Functions";
		funcs.signature = "script_functions";
		for (int i = 0; i < frame_function_amount; i++) {
			int signature = p_data[idx++];
			int calls = p_data[idx++];
			float total = p_data[idx++];
			float self = p_data[idx++];

			EditorProfiler::Metric::Category::Item item;
			if (profiler_signature.has(signature)) {
				item.signature = profiler_signature[signature];

				String name = profiler_signature[signature];
				Vector<String> strings = name.split("::");
				if (strings.size() == 3) {
					item.name = strings[2];
					item.script = strings[0];
					item.line = strings[1].to_int();
				} else if (strings.size() == 4) { //Built-in scripts have an :: in their name
					item.name = strings[3];
					item.script = strings[0] + "::" + strings[1];
					item.line = strings[2].to_int();
				}

			} else {
				item.name = "SigErr " + itos(signature);
			}

			item.calls = calls;
			item.self = self;
			item.total = total;
			funcs.items.write[i] = item;
		}

		metric.categories.push_back(funcs);

		if (p_msg == "profile_frame") {
			profiler->add_frame_metric(metric, false);
		} else {
			profiler->add_frame_metric(metric, true);
		}
	} else if (p_msg == "network_profile") {
		int frame_size = 6;
		for (int i = 0; i < p_data.size(); i += frame_size) {
			MultiplayerAPI::ProfilingInfo pi;
			pi.node = p_data[i + 0];
			pi.node_path = p_data[i + 1];
			pi.incoming_rpc = p_data[i + 2];
			pi.incoming_rset = p_data[i + 3];
			pi.outgoing_rpc = p_data[i + 4];
			pi.outgoing_rset = p_data[i + 5];
			network_profiler->add_node_frame_data(pi);
		}
	} else if (p_msg == "network_bandwidth") {
		network_profiler->set_bandwidth(p_data[0], p_data[1]);
	} else if (p_msg == "kill_me") {
		editor->call_deferred("stop_child_process");
	}
}

void ScriptEditorDebugger::_set_reason_text(const String &p_reason, MessageType p_type) {
	switch (p_type) {
		case MESSAGE_ERROR:
			reason->add_color_override("font_color", get_color("error_color", "Editor"));
			break;
		case MESSAGE_WARNING:
			reason->add_color_override("font_color", get_color("warning_color", "Editor"));
			break;
		default:
			reason->add_color_override("font_color", get_color("success_color", "Editor"));
	}
	reason->set_text(p_reason);
	reason->set_tooltip(p_reason.word_wrap(80));
}

void ScriptEditorDebugger::_performance_select() {
	perf_draw->update();
}

void ScriptEditorDebugger::_performance_draw() {
	Vector<int> which;
	for (int i = 0; i < perf_items.size(); i++) {
		if (perf_items[i]->is_checked(0)) {
			which.push_back(i);
		}
	}

	if (which.empty()) {
		info_message->show();
		return;
	}

	info_message->hide();

	const Ref<StyleBox> graph_sb = get_stylebox("normal", "TextEdit");
	const Ref<Font> graph_font = get_font("font", "TextEdit");

	const int cols = Math::ceil(Math::sqrt((float)which.size()));
	int rows = Math::ceil((float)which.size() / cols);
	if (which.size() == 1) {
		rows = 1;
	}

	const int margin = 3;
	const int point_sep = 5;
	const Size2i s = Size2i(perf_draw->get_size()) / Size2i(cols, rows);

	for (int i = 0; i < which.size(); i++) {
		Point2i p(i % cols, i / cols);
		Rect2i r(p * s, s);
		r.position += Point2(margin, margin);
		r.size -= Point2(margin, margin) * 2.0;
		perf_draw->draw_style_box(graph_sb, r);
		r.position += graph_sb->get_offset();
		r.size -= graph_sb->get_minimum_size();
		const int pi = which[i];

		// Draw horizontal lines with labels.

		int nb_lines = 5;
		// Draw less lines if the monitor isn't tall enough to display 5 labels.
		if (r.size.height <= 160 * EDSCALE) {
			nb_lines = 3;
		} else if (r.size.height <= 240 * EDSCALE) {
			nb_lines = 4;
		}

		const float inv_nb_lines = 1.0 / nb_lines;

		for (int line = 0; line < nb_lines; line += 1) {
			const int from_x = r.position.x;
			const int to_x = r.position.x + r.size.width;
			const int y = r.position.y + (r.size.height * inv_nb_lines + line * inv_nb_lines * r.size.height);
			perf_draw->draw_line(
					Point2(from_x, y),
					Point2i(to_x, y),
					Color(0.5, 0.5, 0.5, 0.25),
					Math::round(EDSCALE));

			String label;
			switch (Performance::MonitorType((int)perf_items[pi]->get_metadata(1))) {
				case Performance::MONITOR_TYPE_MEMORY: {
					label = String::humanize_size(Math::ceil((1 - inv_nb_lines - inv_nb_lines * line) * perf_max[pi]));
				} break;
				case Performance::MONITOR_TYPE_TIME: {
					label = rtos((1 - inv_nb_lines - inv_nb_lines * line) * perf_max[pi] * 1000).pad_decimals(2) + " ms";
				} break;
				default: {
					label = itos(Math::ceil((1 - inv_nb_lines - inv_nb_lines * line) * perf_max[pi]));
				} break;
			}

			perf_draw->draw_string(
					graph_font,
					Point2(from_x, y - graph_font->get_ascent() * 0.25),
					label,
					Color(0.5, 0.5, 0.5, 1.0));
		}

		const float h = (float)which[i] / (float)(perf_items.size());
		// Use a darker color on light backgrounds for better visibility.
		const float value_multiplier = EditorSettings::get_singleton()->is_dark_theme() ? 1.4 : 0.55;
		Color color = get_color("accent_color", "Editor");
		color.set_hsv(Math::fmod(h + 0.4, 0.9), color.get_s() * 0.9, color.get_v() * value_multiplier);

		// Draw the monitor name in the top-left corner.
		color.a = 0.6;
		perf_draw->draw_string(
				graph_font,
				r.position + Point2(0, graph_font->get_ascent()),
				perf_items[pi]->get_text(0),
				color,
				r.size.x);

		// Draw the monitor value in the top-left corner, just below the name.
		color.a = 0.9;
		perf_draw->draw_string(
				graph_font,
				r.position + Point2(0, graph_font->get_ascent() + graph_font->get_height()),
				perf_items[pi]->get_text(1),
				color,
				r.size.y);

		const float spacing = point_sep / float(cols);
		float from = r.size.width;

		const List<Vector<float>>::Element *E = perf_history.front();
		float prev = -1;
		while (from >= 0 && E) {
			float m = perf_max[pi];
			if (m == 0) {
				m = 0.00001;
			}
			float h2 = E->get()[pi] / m;
			h2 = (1.0 - h2) * r.size.y;

			if (E != perf_history.front()) {
				perf_draw->draw_line(
						r.position + Point2(from, h2),
						r.position + Point2(from + spacing, prev),
						color,
						Math::round(EDSCALE));
			}
			prev = h2;
			E = E->next();
			from -= spacing;
		}
	}
}

void ScriptEditorDebugger::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			inspector->edit(variables);
			skip_breakpoints->set_icon(get_icon("DebugSkipBreakpointsOff", "EditorIcons"));
			copy->set_icon(get_icon("ActionCopy", "EditorIcons"));

			step->set_icon(get_icon("DebugStep", "EditorIcons"));
			next->set_icon(get_icon("DebugNext", "EditorIcons"));
			back->set_icon(get_icon("Back", "EditorIcons"));
			forward->set_icon(get_icon("Forward", "EditorIcons"));
			dobreak->set_icon(get_icon("Pause", "EditorIcons"));
			docontinue->set_icon(get_icon("DebugContinue", "EditorIcons"));
			le_set->connect("pressed", this, "_live_edit_set");
			le_clear->connect("pressed", this, "_live_edit_clear");
			error_tree->connect("item_selected", this, "_error_selected");
			error_tree->connect("item_activated", this, "_error_activated");
			vmem_refresh->set_icon(get_icon("Reload", "EditorIcons"));
			vmem_export->set_icon(get_icon("Save", "EditorIcons"));
			search->set_right_icon(get_icon("Search", "EditorIcons"));

			reason->add_color_override("font_color", get_color("error_color", "Editor"));

		} break;
		case NOTIFICATION_PROCESS: {
			if (connection.is_valid()) {
				inspect_scene_tree_timeout -= get_process_delta_time();
				if (inspect_scene_tree_timeout < 0) {
					inspect_scene_tree_timeout = EditorSettings::get_singleton()->get("debugger/remote_scene_tree_refresh_interval");
					if (inspect_scene_tree->is_visible_in_tree()) {
						_scene_tree_request();
					}
				}

				inspect_edited_object_timeout -= get_process_delta_time();
				if (inspect_edited_object_timeout < 0) {
					inspect_edited_object_timeout = EditorSettings::get_singleton()->get("debugger/remote_inspect_refresh_interval");
					if (inspected_object_id) {
						if (ScriptEditorDebuggerInspectedObject *obj = Object::cast_to<ScriptEditorDebuggerInspectedObject>(ObjectDB::get_instance(editor->get_editor_history()->get_current()))) {
							if (obj->remote_object_id == inspected_object_id) {
								//take the chance and re-inspect selected object
								Array msg;
								msg.push_back("inspect_object");
								msg.push_back(inspected_object_id);
								ppeer->put_var(msg);
							}
						}
					}
				}

				if (camera_override == OVERRIDE_2D) {
					CanvasItemEditor *editor = CanvasItemEditor::get_singleton();

					Dictionary state = editor->get_state();
					float zoom = state["zoom"];
					Point2 offset = state["ofs"];
					Transform2D transform;

					transform.scale_basis(Size2(zoom, zoom));
					transform.elements[2] = -offset * zoom;

					Array msg;
					msg.push_back("override_camera_2D:transform");
					msg.push_back(transform);
					ppeer->put_var(msg);

				} else if (camera_override >= OVERRIDE_3D_1) {
					int viewport_idx = camera_override - OVERRIDE_3D_1;
					SpatialEditorViewport *viewport = SpatialEditor::get_singleton()->get_editor_viewport(viewport_idx);
					Camera *const cam = viewport->get_camera();

					Array msg;
					msg.push_back("override_camera_3D:transform");
					msg.push_back(cam->get_camera_transform());
					if (cam->get_projection() == Camera::PROJECTION_ORTHOGONAL) {
						msg.push_back(false);
						msg.push_back(cam->get_size());
					} else {
						msg.push_back(true);
						msg.push_back(cam->get_fov());
					}
					msg.push_back(cam->get_znear());
					msg.push_back(cam->get_zfar());
					ppeer->put_var(msg);
				}
			}

			if (error_count != last_error_count || warning_count != last_warning_count) {
				if (error_count == 0 && warning_count == 0) {
					errors_tab->set_name(TTR("Errors"));
					debugger_button->set_text(TTR("Debugger"));
					debugger_button->add_color_override("font_color", get_color("font_color", "Editor"));
					debugger_button->set_icon(Ref<Texture>());
					tabs->set_tab_icon(errors_tab->get_index(), Ref<Texture>());
				} else {
					errors_tab->set_name(TTR("Errors") + " (" + itos(error_count + warning_count) + ")");
					debugger_button->set_text(TTR("Debugger") + " (" + itos(error_count + warning_count) + ")");
					if (error_count >= 1 && warning_count >= 1) {
						debugger_button->set_icon(get_icon("ErrorWarning", "EditorIcons"));
						// Use error color to represent the highest level of severity reported.
						debugger_button->add_color_override("font_color", get_color("error_color", "Editor"));
						tabs->set_tab_icon(errors_tab->get_index(), get_icon("ErrorWarning", "EditorIcons"));
					} else if (error_count >= 1) {
						debugger_button->set_icon(get_icon("Error", "EditorIcons"));
						debugger_button->add_color_override("font_color", get_color("error_color", "Editor"));
						tabs->set_tab_icon(errors_tab->get_index(), get_icon("Error", "EditorIcons"));
					} else {
						debugger_button->set_icon(get_icon("Warning", "EditorIcons"));
						debugger_button->add_color_override("font_color", get_color("warning_color", "Editor"));
						tabs->set_tab_icon(errors_tab->get_index(), get_icon("Warning", "EditorIcons"));
					}
				}
				last_error_count = error_count;
				last_warning_count = warning_count;
			}

			if (server->is_connection_available()) {
				if (connection.is_valid()) {
					// We already have a valid connection. Disconnecting any new connecting client to prevent it from hanging.
					// (If we don't keep a reference to the connection it will be destroyed and disconnect_from_host will be called internally)
					server->take_connection();
				} else {
					// We just got the first connection.
					connection = server->take_connection();
					if (connection.is_null()) {
						break;
					}

					EditorNode::get_log()->set_scroll_follow(true);
					EditorNode::get_log()->add_message("--- Debugging process started ---", EditorLog::MSG_TYPE_EDITOR);

					ppeer->set_stream_peer(connection);

					//EditorNode::get_singleton()->make_bottom_panel_item_visible(this);
					//emit_signal("show_debugger",true);

					dobreak->set_disabled(false);
					tabs->set_current_tab(0);

					_set_reason_text(TTR("Child process connected."), MESSAGE_SUCCESS);
					profiler->clear();

					inspect_scene_tree->clear();
					le_set->set_disabled(true);
					le_clear->set_disabled(false);
					vmem_refresh->set_disabled(false);
					error_tree->clear();
					error_count = 0;
					warning_count = 0;
					profiler_signature.clear();
					//live_edit_root->set_text("/root");

					EditorNode::get_singleton()->get_pause_button()->set_pressed(false);
					EditorNode::get_singleton()->get_pause_button()->set_disabled(false);

					update_live_edit_root();
					if (profiler->is_profiling()) {
						_profiler_activate(true);
					}

					if (network_profiler->is_profiling()) {
						_network_profiler_activate(true);
					}
				}
			}

			if (connection.is_null()) {
				break;
			}

			if (!connection->is_connected_to_host()) {
				stop();
				editor->notify_child_process_exited(); //somehow, exited
				break;
			};

			if (ppeer->get_available_packet_count() <= 0) {
				break;
			};

			const uint64_t until = OS::get_singleton()->get_ticks_msec() + 20;

			while (ppeer->get_available_packet_count() > 0) {
				if (pending_in_queue) {
					int todo = MIN(ppeer->get_available_packet_count(), pending_in_queue);

					for (int i = 0; i < todo; i++) {
						Variant cmd;
						Error ret = ppeer->get_var(cmd);
						if (ret != OK) {
							stop();
							ERR_FAIL_COND(ret != OK);
						}

						message.push_back(cmd);
						pending_in_queue--;
					}

					if (pending_in_queue == 0) {
						_parse_message(message_type, message);
						message.clear();
					}

				} else {
					if (ppeer->get_available_packet_count() >= 2) {
						Variant cmd;
						Error ret = ppeer->get_var(cmd);
						if (ret != OK) {
							stop();
							ERR_FAIL_COND(ret != OK);
						}
						if (cmd.get_type() != Variant::STRING) {
							stop();
							ERR_FAIL_COND(cmd.get_type() != Variant::STRING);
						}

						message_type = cmd;

						ret = ppeer->get_var(cmd);
						if (ret != OK) {
							stop();
							ERR_FAIL_COND(ret != OK);
						}
						if (cmd.get_type() != Variant::INT) {
							stop();
							ERR_FAIL_COND(cmd.get_type() != Variant::INT);
						}

						pending_in_queue = cmd;

						if (pending_in_queue == 0) {
							_parse_message(message_type, Array());
							message.clear();
						}

					} else {
						break;
					}
				}

				if (OS::get_singleton()->get_ticks_msec() > until) {
					break;
				}
			}
		} break;
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			add_constant_override("margin_left", -EditorNode::get_singleton()->get_gui_base()->get_stylebox("BottomPanelDebuggerOverride", "EditorStyles")->get_margin(MARGIN_LEFT));
			add_constant_override("margin_right", -EditorNode::get_singleton()->get_gui_base()->get_stylebox("BottomPanelDebuggerOverride", "EditorStyles")->get_margin(MARGIN_RIGHT));

			tabs->add_style_override("panel", editor->get_gui_base()->get_stylebox("DebuggerPanel", "EditorStyles"));
			tabs->add_style_override("tab_fg", editor->get_gui_base()->get_stylebox("DebuggerTabFG", "EditorStyles"));
			tabs->add_style_override("tab_bg", editor->get_gui_base()->get_stylebox("DebuggerTabBG", "EditorStyles"));

			copy->set_icon(get_icon("ActionCopy", "EditorIcons"));
			step->set_icon(get_icon("DebugStep", "EditorIcons"));
			next->set_icon(get_icon("DebugNext", "EditorIcons"));
			back->set_icon(get_icon("Back", "EditorIcons"));
			forward->set_icon(get_icon("Forward", "EditorIcons"));
			dobreak->set_icon(get_icon("Pause", "EditorIcons"));
			docontinue->set_icon(get_icon("DebugContinue", "EditorIcons"));
			vmem_refresh->set_icon(get_icon("Reload", "EditorIcons"));
			vmem_export->set_icon(get_icon("Save", "EditorIcons"));
			search->set_right_icon(get_icon("Search", "EditorIcons"));
		} break;
	}
}

void ScriptEditorDebugger::_clear_execution() {
	TreeItem *ti = stack_dump->get_selected();
	if (!ti) {
		return;
	}

	Dictionary d = ti->get_metadata(0);

	stack_script = ResourceLoader::load(d["file"]);
	emit_signal("clear_execution", stack_script);
	stack_script.unref();
	stack_dump->clear();
}

void ScriptEditorDebugger::start(int p_port, const IP_Address &p_bind_address) {
	if (is_inside_tree()) {
		stop();
	}

	if (is_visible_in_tree()) {
		EditorNode::get_singleton()->make_bottom_panel_item_visible(this);
	}

	perf_history.clear();
	for (int i = 0; i < Performance::MONITOR_MAX; i++) {
		perf_max.write[i] = 0;
	}

	const int max_tries = 6;
	if (p_port < 0) {
		remote_port = (int)EditorSettings::get_singleton()->get("network/debug/remote_port");
	} else {
		remote_port = p_port;
	}
	int current_try = 0;
	// Find first available port.
	Error err = server->listen(remote_port);
	while (err != OK && current_try < max_tries) {
		EditorNode::get_log()->add_message(String("Remote debugger failed listening on port: ") + itos(remote_port) + String(" Retrying on new port: " + itos(remote_port + 1)), EditorLog::MSG_TYPE_WARNING);
		current_try++;
		remote_port++;
		OS::get_singleton()->delay_usec(1000);
		err = server->listen(remote_port, p_bind_address);
	}
	// No suitable port found.
	if (err != OK) {
		EditorNode::get_log()->add_message(String("Error listening on port ") + itos(remote_port), EditorLog::MSG_TYPE_ERROR);
		EditorNode::get_log()->add_message(String("Remote debugger error listening for connections. No free port"), EditorLog::MSG_TYPE_ERROR);
	}
	EditorNode::get_singleton()->get_scene_tree_dock()->show_tab_buttons();

	auto_switch_remote_scene_tree = (bool)EditorSettings::get_singleton()->get("debugger/auto_switch_to_remote_scene_tree");
	if (is_inside_tree() && auto_switch_remote_scene_tree) {
		EditorNode::get_singleton()->get_scene_tree_dock()->show_remote_tree();
	}

	set_process(true);
	breaked = false;
	camera_override = OVERRIDE_NONE;
}

void ScriptEditorDebugger::pause() {
}

void ScriptEditorDebugger::unpause() {
}

void ScriptEditorDebugger::stop() {
	set_process(false);
	breaked = false;
	_clear_execution();

	server->stop();
	_clear_remote_objects();
	ppeer->set_stream_peer(Ref<StreamPeer>());

	if (connection.is_valid()) {
		EditorNode::get_log()->add_message("--- Debugging process stopped ---", EditorLog::MSG_TYPE_EDITOR);
		connection.unref();

		reason->set_text("");
		reason->set_tooltip("");
	}

	remote_port = 0;
	pending_in_queue = 0;
	message.clear();

	node_path_cache.clear();
	res_path_cache.clear();
	profiler_signature.clear();
	le_clear->set_disabled(false);
	le_set->set_disabled(true);
	profiler->set_enabled(true);
	vmem_refresh->set_disabled(true);

	inspect_scene_tree->clear();
	inspector->edit(nullptr);
	EditorNode::get_singleton()->get_pause_button()->set_pressed(false);
	EditorNode::get_singleton()->get_pause_button()->set_disabled(true);
	EditorNode::get_singleton()->get_scene_tree_dock()->hide_remote_tree();
	EditorNode::get_singleton()->get_scene_tree_dock()->hide_tab_buttons();

	if (hide_on_stop) {
		if (is_visible_in_tree()) {
			EditorNode::get_singleton()->hide_bottom_panel();
		}
		emit_signal("show_debugger", false);
	}
}

void ScriptEditorDebugger::_profiler_activate(bool p_enable) {
	if (!connection.is_valid()) {
		return;
	}

	if (p_enable) {
		profiler_signature.clear();
		Array msg;
		msg.push_back("start_profiling");
		int max_funcs = EditorSettings::get_singleton()->get("debugger/profiler_frame_max_functions");
		max_funcs = CLAMP(max_funcs, 16, 512);
		msg.push_back(max_funcs);
		ppeer->put_var(msg);
		print_verbose("Starting profiling.");

	} else {
		Array msg;
		msg.push_back("stop_profiling");
		ppeer->put_var(msg);
		print_verbose("Ending profiling.");
	}
}

void ScriptEditorDebugger::_network_profiler_activate(bool p_enable) {
	if (!connection.is_valid()) {
		return;
	}

	if (p_enable) {
		Array msg;
		msg.push_back("start_network_profiling");
		ppeer->put_var(msg);
		print_verbose("Starting network profiling.");

	} else {
		Array msg;
		msg.push_back("stop_network_profiling");
		ppeer->put_var(msg);
		print_verbose("Ending network profiling.");
	}
}

void ScriptEditorDebugger::_profiler_seeked() {
	if (!connection.is_valid() || !connection->is_connected_to_host()) {
		return;
	}

	if (breaked) {
		return;
	}
	debug_break();
}

void ScriptEditorDebugger::_stack_dump_frame_selected() {
	TreeItem *ti = stack_dump->get_selected();
	if (!ti) {
		return;
	}

	Dictionary d = ti->get_metadata(0);

	stack_script = ResourceLoader::load(d["file"]);
	emit_signal("goto_script_line", stack_script, int(d["line"]) - 1);
	emit_signal("set_execution", stack_script, int(d["line"]) - 1);
	stack_script.unref();

	if (connection.is_valid() && connection->is_connected_to_host()) {
		Array msg;
		msg.push_back("get_stack_frame_vars");
		msg.push_back(d["frame"]);
		ppeer->put_var(msg);
	} else {
		inspector->edit(nullptr);
	}
}

void ScriptEditorDebugger::_output_clear() {
	//output->clear();
	//output->push_color(Color(0,0,0));
}

void ScriptEditorDebugger::_export_csv() {
	file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_dialog->clear_filters();
	file_dialog_mode = SAVE_MONITORS_CSV;
	file_dialog->popup_centered_ratio();
}

String ScriptEditorDebugger::get_var_value(const String &p_var) const {
	if (!breaked) {
		return String();
	}
	return variables->get_var_value(p_var);
}

int ScriptEditorDebugger::_get_node_path_cache(const NodePath &p_path) {
	const int *r = node_path_cache.getptr(p_path);
	if (r) {
		return *r;
	}

	last_path_id++;

	node_path_cache[p_path] = last_path_id;
	Array msg;
	msg.push_back("live_node_path");
	msg.push_back(p_path);
	msg.push_back(last_path_id);
	ppeer->put_var(msg);

	return last_path_id;
}

int ScriptEditorDebugger::_get_res_path_cache(const String &p_path) {
	Map<String, int>::Element *E = res_path_cache.find(p_path);

	if (E) {
		return E->get();
	}

	last_path_id++;

	res_path_cache[p_path] = last_path_id;
	Array msg;
	msg.push_back("live_res_path");
	msg.push_back(p_path);
	msg.push_back(last_path_id);
	ppeer->put_var(msg);

	return last_path_id;
}

void ScriptEditorDebugger::_method_changed(Object *p_base, const StringName &p_name, VARIANT_ARG_DECLARE) {
	if (!p_base || !live_debug || !connection.is_valid() || !editor->get_edited_scene()) {
		return;
	}

	Node *node = Object::cast_to<Node>(p_base);

	VARIANT_ARGPTRS

	for (int i = 0; i < VARIANT_ARG_MAX; i++) {
		//no pointers, sorry
		if (argptr[i] && (argptr[i]->get_type() == Variant::OBJECT || argptr[i]->get_type() == Variant::_RID)) {
			return;
		}
	}

	if (node) {
		NodePath path = editor->get_edited_scene()->get_path_to(node);
		int pathid = _get_node_path_cache(path);

		Array msg;
		msg.push_back("live_node_call");
		msg.push_back(pathid);
		msg.push_back(p_name);
		for (int i = 0; i < VARIANT_ARG_MAX; i++) {
			//no pointers, sorry
			msg.push_back(*argptr[i]);
		}
		ppeer->put_var(msg);

		return;
	}

	Resource *res = Object::cast_to<Resource>(p_base);

	if (res && res->get_path() != String()) {
		String respath = res->get_path();
		int pathid = _get_res_path_cache(respath);

		Array msg;
		msg.push_back("live_res_call");
		msg.push_back(pathid);
		msg.push_back(p_name);
		for (int i = 0; i < VARIANT_ARG_MAX; i++) {
			//no pointers, sorry
			msg.push_back(*argptr[i]);
		}
		ppeer->put_var(msg);

		return;
	}
}

void ScriptEditorDebugger::_property_changed(Object *p_base, const StringName &p_property, const Variant &p_value) {
	if (!p_base || !live_debug || !connection.is_valid() || !editor->get_edited_scene()) {
		return;
	}

	Node *node = Object::cast_to<Node>(p_base);

	if (node) {
		NodePath path = editor->get_edited_scene()->get_path_to(node);
		int pathid = _get_node_path_cache(path);

		if (p_value.is_ref()) {
			Ref<Resource> res = p_value;
			if (res.is_valid() && res->get_path() != String()) {
				Array msg;
				msg.push_back("live_node_prop_res");
				msg.push_back(pathid);
				msg.push_back(p_property);
				msg.push_back(res->get_path());
				ppeer->put_var(msg);
			}
		} else {
			Array msg;
			msg.push_back("live_node_prop");
			msg.push_back(pathid);
			msg.push_back(p_property);
			msg.push_back(p_value);
			ppeer->put_var(msg);
		}

		return;
	}

	Resource *res = Object::cast_to<Resource>(p_base);

	if (res && res->get_path() != String()) {
		String respath = res->get_path();
		int pathid = _get_res_path_cache(respath);

		if (p_value.is_ref()) {
			Ref<Resource> res2 = p_value;
			if (res2.is_valid() && res2->get_path() != String()) {
				Array msg;
				msg.push_back("live_res_prop_res");
				msg.push_back(pathid);
				msg.push_back(p_property);
				msg.push_back(res2->get_path());
				ppeer->put_var(msg);
			}
		} else {
			Array msg;
			msg.push_back("live_res_prop");
			msg.push_back(pathid);
			msg.push_back(p_property);
			msg.push_back(p_value);
			ppeer->put_var(msg);
		}

		return;
	}
}

void ScriptEditorDebugger::_method_changeds(void *p_ud, Object *p_base, const StringName &p_name, VARIANT_ARG_DECLARE) {
	ScriptEditorDebugger *sed = (ScriptEditorDebugger *)p_ud;
	sed->_method_changed(p_base, p_name, VARIANT_ARG_PASS);
}

void ScriptEditorDebugger::_property_changeds(void *p_ud, Object *p_base, const StringName &p_property, const Variant &p_value) {
	ScriptEditorDebugger *sed = (ScriptEditorDebugger *)p_ud;
	sed->_property_changed(p_base, p_property, p_value);
}

void ScriptEditorDebugger::set_live_debugging(bool p_enable) {
	live_debug = p_enable;
}

void ScriptEditorDebugger::_live_edit_set() {
	if (!connection.is_valid()) {
		return;
	}

	TreeItem *ti = inspect_scene_tree->get_selected();
	if (!ti) {
		return;
	}
	String path;

	while (ti) {
		String lp = ti->get_text(0);
		path = "/" + lp + path;
		ti = ti->get_parent();
	}

	NodePath np = path;

	editor->get_editor_data().set_edited_scene_live_edit_root(np);

	update_live_edit_root();
}

void ScriptEditorDebugger::_live_edit_clear() {
	NodePath np = NodePath("/root");
	editor->get_editor_data().set_edited_scene_live_edit_root(np);

	update_live_edit_root();
}

void ScriptEditorDebugger::update_live_edit_root() {
	NodePath np = editor->get_editor_data().get_edited_scene_live_edit_root();

	if (connection.is_valid()) {
		Array msg;
		msg.push_back("live_set_root");
		msg.push_back(np);
		if (editor->get_edited_scene()) {
			msg.push_back(editor->get_edited_scene()->get_filename());
		} else {
			msg.push_back("");
		}
		ppeer->put_var(msg);
	}
	live_edit_root->set_text(np);
}

void ScriptEditorDebugger::live_debug_create_node(const NodePath &p_parent, const String &p_type, const String &p_name) {
	if (live_debug && connection.is_valid()) {
		Array msg;
		msg.push_back("live_create_node");
		msg.push_back(p_parent);
		msg.push_back(p_type);
		msg.push_back(p_name);
		ppeer->put_var(msg);
	}
}

void ScriptEditorDebugger::live_debug_instance_node(const NodePath &p_parent, const String &p_path, const String &p_name) {
	if (live_debug && connection.is_valid()) {
		Array msg;
		msg.push_back("live_instance_node");
		msg.push_back(p_parent);
		msg.push_back(p_path);
		msg.push_back(p_name);
		ppeer->put_var(msg);
	}
}
void ScriptEditorDebugger::live_debug_remove_node(const NodePath &p_at) {
	if (live_debug && connection.is_valid()) {
		Array msg;
		msg.push_back("live_remove_node");
		msg.push_back(p_at);
		ppeer->put_var(msg);
	}
}
void ScriptEditorDebugger::live_debug_remove_and_keep_node(const NodePath &p_at, ObjectID p_keep_id) {
	if (live_debug && connection.is_valid()) {
		Array msg;
		msg.push_back("live_remove_and_keep_node");
		msg.push_back(p_at);
		msg.push_back(p_keep_id);
		ppeer->put_var(msg);
	}
}
void ScriptEditorDebugger::live_debug_restore_node(ObjectID p_id, const NodePath &p_at, int p_at_pos) {
	if (live_debug && connection.is_valid()) {
		Array msg;
		msg.push_back("live_restore_node");
		msg.push_back(p_id);
		msg.push_back(p_at);
		msg.push_back(p_at_pos);
		ppeer->put_var(msg);
	}
}
void ScriptEditorDebugger::live_debug_duplicate_node(const NodePath &p_at, const String &p_new_name) {
	if (live_debug && connection.is_valid()) {
		Array msg;
		msg.push_back("live_duplicate_node");
		msg.push_back(p_at);
		msg.push_back(p_new_name);
		ppeer->put_var(msg);
	}
}
void ScriptEditorDebugger::live_debug_reparent_node(const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos) {
	if (live_debug && connection.is_valid()) {
		Array msg;
		msg.push_back("live_reparent_node");
		msg.push_back(p_at);
		msg.push_back(p_new_place);
		msg.push_back(p_new_name);
		msg.push_back(p_at_pos);
		ppeer->put_var(msg);
	}
}

ScriptEditorDebugger::CameraOverride ScriptEditorDebugger::get_camera_override() const {
	return camera_override;
}

void ScriptEditorDebugger::set_camera_override(CameraOverride p_override) {
	if (p_override == OVERRIDE_2D && camera_override != OVERRIDE_2D) {
		if (connection.is_valid()) {
			Array msg;
			msg.push_back("override_camera_2D:set");
			msg.push_back(true);
			ppeer->put_var(msg);
		}
	} else if (p_override != OVERRIDE_2D && camera_override == OVERRIDE_2D) {
		if (connection.is_valid()) {
			Array msg;
			msg.push_back("override_camera_2D:set");
			msg.push_back(false);
			ppeer->put_var(msg);
		}
	} else if (p_override >= OVERRIDE_3D_1 && camera_override < OVERRIDE_3D_1) {
		if (connection.is_valid()) {
			Array msg;
			msg.push_back("override_camera_3D:set");
			msg.push_back(true);
			ppeer->put_var(msg);
		}
	} else if (p_override < OVERRIDE_3D_1 && camera_override >= OVERRIDE_3D_1) {
		if (connection.is_valid()) {
			Array msg;
			msg.push_back("override_camera_3D:set");
			msg.push_back(false);
			ppeer->put_var(msg);
		}
	}

	camera_override = p_override;
}

void ScriptEditorDebugger::set_breakpoint(const String &p_path, int p_line, bool p_enabled) {
	if (connection.is_valid()) {
		Array msg;
		msg.push_back("breakpoint");
		msg.push_back(p_path);
		msg.push_back(p_line);
		msg.push_back(p_enabled);
		ppeer->put_var(msg);
	}
}

void ScriptEditorDebugger::reload_scripts() {
	if (connection.is_valid()) {
		Array msg;
		msg.push_back("reload_scripts");
		ppeer->put_var(msg);
	}
}

bool ScriptEditorDebugger::is_skip_breakpoints() {
	return skip_breakpoints_value;
}

void ScriptEditorDebugger::_error_activated() {
	TreeItem *selected = error_tree->get_selected();

	TreeItem *ci = selected->get_children();
	if (ci) {
		selected->set_collapsed(!selected->is_collapsed());
	}
}

void ScriptEditorDebugger::_error_selected() {
	TreeItem *selected = error_tree->get_selected();

	Array meta = selected->get_metadata(0);

	if (meta.size() == 0) {
		return;
	}

	Ref<Script> s = ResourceLoader::load(meta[0]);
	emit_signal("goto_script_line", s, int(meta[1]) - 1);
}

void ScriptEditorDebugger::_expand_errors_list() {
	TreeItem *root = error_tree->get_root();
	if (!root) {
		return;
	}

	TreeItem *item = root->get_children();
	while (item) {
		item->set_collapsed(false);
		item = item->get_next();
	}
}

void ScriptEditorDebugger::_collapse_errors_list() {
	TreeItem *root = error_tree->get_root();
	if (!root) {
		return;
	}

	TreeItem *item = root->get_children();
	while (item) {
		item->set_collapsed(true);
		item = item->get_next();
	}
}

void ScriptEditorDebugger::set_hide_on_stop(bool p_hide) {
	hide_on_stop = p_hide;
}

bool ScriptEditorDebugger::get_debug_with_external_editor() const {
	return enable_external_editor;
}

String ScriptEditorDebugger::get_connection_string() const {
	String remote_host = EditorSettings::get_singleton()->get("network/debug/remote_host");
	return remote_port ? remote_host + ":" + itos(remote_port) : "";
}

void ScriptEditorDebugger::set_debug_with_external_editor(bool p_enabled) {
	enable_external_editor = p_enabled;
}

Ref<Script> ScriptEditorDebugger::get_dump_stack_script() const {
	return stack_script;
}

void ScriptEditorDebugger::_paused() {
	ERR_FAIL_COND(connection.is_null());
	ERR_FAIL_COND(!connection->is_connected_to_host());

	if (!breaked && EditorNode::get_singleton()->get_pause_button()->is_pressed()) {
		debug_break();
	}

	if (breaked && !EditorNode::get_singleton()->get_pause_button()->is_pressed()) {
		debug_continue();
	}
}

void ScriptEditorDebugger::_set_remote_object(ObjectID p_id, ScriptEditorDebuggerInspectedObject *p_obj) {
	if (remote_objects.has(p_id)) {
		memdelete(remote_objects[p_id]);
	}
	remote_objects[p_id] = p_obj;
}

void ScriptEditorDebugger::_clear_remote_objects() {
	for (Map<ObjectID, ScriptEditorDebuggerInspectedObject *>::Element *E = remote_objects.front(); E; E = E->next()) {
		if (editor->get_editor_history()->get_current() == E->value()->get_instance_id()) {
			editor->push_item(nullptr);
		}
		memdelete(E->value());
	}
	remote_objects.clear();
	remote_dependencies.clear();
}

void ScriptEditorDebugger::_clear_errors_list() {
	error_tree->clear();
	error_count = 0;
	warning_count = 0;
	_notification(NOTIFICATION_PROCESS);
}

// Right click on specific file(s) or folder(s).
void ScriptEditorDebugger::_error_tree_item_rmb_selected(const Vector2 &p_pos) {
	item_menu->clear();
	item_menu->set_size(Size2(1, 1));

	if (error_tree->is_anything_selected()) {
		item_menu->add_icon_item(get_icon("ActionCopy", "EditorIcons"), TTR("Copy Error"), ITEM_MENU_COPY_ERROR);
		item_menu->add_icon_item(get_icon("ExternalLink", "EditorIcons"), TTR("Open C++ Source on GitHub"), ITEM_MENU_OPEN_SOURCE);
	}

	if (item_menu->get_item_count() > 0) {
		item_menu->set_position(error_tree->get_global_position() + p_pos);
		item_menu->popup();
	}
}

void ScriptEditorDebugger::_item_menu_id_pressed(int p_option) {
	switch (p_option) {
		case ITEM_MENU_COPY_ERROR: {
			TreeItem *ti = error_tree->get_selected();
			while (ti->get_parent() != error_tree->get_root()) {
				ti = ti->get_parent();
			}

			String type;

			if (ti->get_icon(0) == get_icon("Warning", "EditorIcons")) {
				type = "W ";
			} else if (ti->get_icon(0) == get_icon("Error", "EditorIcons")) {
				type = "E ";
			}

			String text = ti->get_text(0) + "   ";
			int rpad_len = text.length();

			text = type + text + ti->get_text(1) + "\n";
			TreeItem *ci = ti->get_children();
			while (ci) {
				text += "  " + ci->get_text(0).rpad(rpad_len) + ci->get_text(1) + "\n";
				ci = ci->get_next();
			}

			OS::get_singleton()->set_clipboard(text);

		} break;
		case ITEM_MENU_SAVE_REMOTE_NODE: {
			file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
			file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
			file_dialog_mode = SAVE_NODE;

			List<String> extensions;
			Ref<PackedScene> sd = memnew(PackedScene);
			ResourceSaver::get_recognized_extensions(sd, &extensions);
			file_dialog->clear_filters();
			for (int i = 0; i < extensions.size(); i++) {
				file_dialog->add_filter("*." + extensions[i] + " ; " + extensions[i].to_upper());
			}

			TreeItem *ti = inspect_scene_tree->get_selected();
			String filename = ti->get_text(0) + "." + extensions.front()->get().to_lower();

			file_dialog->set_current_path(filename);
			file_dialog->popup_centered_ratio();
		} break;
		case ITEM_MENU_COPY_NODE_PATH: {
			TreeItem *ti = inspect_scene_tree->get_selected();
			String text = ti->get_text(0);

			if (ti->get_parent() == nullptr) {
				text = ".";
			} else if (ti->get_parent()->get_parent() == nullptr) {
				text = ".";
			} else {
				while (ti->get_parent()->get_parent() != inspect_scene_tree->get_root()) {
					ti = ti->get_parent();
					text = ti->get_text(0) + "/" + text;
				}
			}

			OS::get_singleton()->set_clipboard(text);
		} break;
		case ITEM_MENU_OPEN_SOURCE: {
			TreeItem *ti = error_tree->get_selected();
			while (ti->get_parent() != error_tree->get_root()) {
				ti = ti->get_parent();
			}

			// Find the child with the "C++ Source".
			// It's not at a fixed position as "C++ Error" may come first.
			TreeItem *ci = ti->get_children();
			const String cpp_source = "<" + TTR("C++ Source") + ">";
			while (ci) {
				if (ci->get_text(0) == cpp_source) {
					break;
				}
				ci = ci->get_next();
			}

			if (!ci) {
				WARN_PRINT("No C++ source reference is available for this error.");
				return;
			}

			// Parse back the `file:line @ method()` string.
			const Vector<String> file_line_number = ci->get_text(1).split("@")[0].strip_edges().split(":");
			ERR_FAIL_COND_MSG(file_line_number.size() < 2, "Incorrect C++ source stack trace file:line format (please report).");
			const String file = file_line_number[0];
			const int line_number = file_line_number[1].to_int();

			// Construct a GitHub repository URL and open it in the user's default web browser.
			// If the commit hash is available, use it for greater accuracy. Otherwise fall back to tagged release.
			String git_ref = String(VERSION_HASH).empty() ? String(VERSION_NUMBER) + "-stable" : String(VERSION_HASH);
			OS::get_singleton()->shell_open(vformat("https://github.com/godotengine/godot/blob/%s/%s#L%d",
					git_ref, file, line_number));
		} break;
	}
}

void ScriptEditorDebugger::_tab_changed(int p_tab) {
	if (tabs->get_tab_title(p_tab) == TTR("Video RAM")) {
		// "Video RAM" tab was clicked, refresh the data it's displaying when entering the tab.
		_video_mem_request();
	}
}

void ScriptEditorDebugger::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_stack_dump_frame_selected"), &ScriptEditorDebugger::_stack_dump_frame_selected);

	ClassDB::bind_method(D_METHOD("debug_skip_breakpoints"), &ScriptEditorDebugger::debug_skip_breakpoints);
	ClassDB::bind_method(D_METHOD("debug_copy"), &ScriptEditorDebugger::debug_copy);

	ClassDB::bind_method(D_METHOD("debug_next"), &ScriptEditorDebugger::debug_next);
	ClassDB::bind_method(D_METHOD("debug_step"), &ScriptEditorDebugger::debug_step);
	ClassDB::bind_method(D_METHOD("debug_break"), &ScriptEditorDebugger::debug_break);
	ClassDB::bind_method(D_METHOD("debug_continue"), &ScriptEditorDebugger::debug_continue);
	ClassDB::bind_method(D_METHOD("_output_clear"), &ScriptEditorDebugger::_output_clear);
	ClassDB::bind_method(D_METHOD("_export_csv"), &ScriptEditorDebugger::_export_csv);
	ClassDB::bind_method(D_METHOD("_performance_draw"), &ScriptEditorDebugger::_performance_draw);
	ClassDB::bind_method(D_METHOD("_performance_select"), &ScriptEditorDebugger::_performance_select);
	ClassDB::bind_method(D_METHOD("_scene_tree_request"), &ScriptEditorDebugger::_scene_tree_request);
	ClassDB::bind_method(D_METHOD("_video_mem_request"), &ScriptEditorDebugger::_video_mem_request);
	ClassDB::bind_method(D_METHOD("_video_mem_export"), &ScriptEditorDebugger::_video_mem_export);
	ClassDB::bind_method(D_METHOD("_live_edit_set"), &ScriptEditorDebugger::_live_edit_set);
	ClassDB::bind_method(D_METHOD("_live_edit_clear"), &ScriptEditorDebugger::_live_edit_clear);

	ClassDB::bind_method(D_METHOD("_error_selected"), &ScriptEditorDebugger::_error_selected);
	ClassDB::bind_method(D_METHOD("_error_activated"), &ScriptEditorDebugger::_error_activated);
	ClassDB::bind_method(D_METHOD("_expand_errors_list"), &ScriptEditorDebugger::_expand_errors_list);
	ClassDB::bind_method(D_METHOD("_collapse_errors_list"), &ScriptEditorDebugger::_collapse_errors_list);
	ClassDB::bind_method(D_METHOD("_profiler_activate"), &ScriptEditorDebugger::_profiler_activate);
	ClassDB::bind_method(D_METHOD("_network_profiler_activate"), &ScriptEditorDebugger::_network_profiler_activate);
	ClassDB::bind_method(D_METHOD("_profiler_seeked"), &ScriptEditorDebugger::_profiler_seeked);
	ClassDB::bind_method(D_METHOD("_clear_errors_list"), &ScriptEditorDebugger::_clear_errors_list);

	ClassDB::bind_method(D_METHOD("_error_tree_item_rmb_selected"), &ScriptEditorDebugger::_error_tree_item_rmb_selected);
	ClassDB::bind_method(D_METHOD("_item_menu_id_pressed"), &ScriptEditorDebugger::_item_menu_id_pressed);
	ClassDB::bind_method(D_METHOD("_tab_changed"), &ScriptEditorDebugger::_tab_changed);

	ClassDB::bind_method(D_METHOD("_paused"), &ScriptEditorDebugger::_paused);

	ClassDB::bind_method(D_METHOD("_scene_tree_selected"), &ScriptEditorDebugger::_scene_tree_selected);
	ClassDB::bind_method(D_METHOD("_scene_tree_folded"), &ScriptEditorDebugger::_scene_tree_folded);
	ClassDB::bind_method(D_METHOD("_scene_tree_rmb_selected"), &ScriptEditorDebugger::_scene_tree_rmb_selected);
	ClassDB::bind_method(D_METHOD("_file_selected"), &ScriptEditorDebugger::_file_selected);

	ClassDB::bind_method(D_METHOD("live_debug_create_node"), &ScriptEditorDebugger::live_debug_create_node);
	ClassDB::bind_method(D_METHOD("live_debug_instance_node"), &ScriptEditorDebugger::live_debug_instance_node);
	ClassDB::bind_method(D_METHOD("live_debug_remove_node"), &ScriptEditorDebugger::live_debug_remove_node);
	ClassDB::bind_method(D_METHOD("live_debug_remove_and_keep_node"), &ScriptEditorDebugger::live_debug_remove_and_keep_node);
	ClassDB::bind_method(D_METHOD("live_debug_restore_node"), &ScriptEditorDebugger::live_debug_restore_node);
	ClassDB::bind_method(D_METHOD("live_debug_duplicate_node"), &ScriptEditorDebugger::live_debug_duplicate_node);
	ClassDB::bind_method(D_METHOD("live_debug_reparent_node"), &ScriptEditorDebugger::live_debug_reparent_node);
	ClassDB::bind_method(D_METHOD("_scene_tree_property_select_object"), &ScriptEditorDebugger::_scene_tree_property_select_object);
	ClassDB::bind_method(D_METHOD("_scene_tree_property_value_edited"), &ScriptEditorDebugger::_scene_tree_property_value_edited);

	ADD_SIGNAL(MethodInfo("goto_script_line"));
	ADD_SIGNAL(MethodInfo("set_execution", PropertyInfo("script"), PropertyInfo(Variant::INT, "line")));
	ADD_SIGNAL(MethodInfo("clear_execution", PropertyInfo("script")));
	ADD_SIGNAL(MethodInfo("breaked", PropertyInfo(Variant::BOOL, "reallydid"), PropertyInfo(Variant::BOOL, "can_debug")));
	ADD_SIGNAL(MethodInfo("show_debugger", PropertyInfo(Variant::BOOL, "reallydid")));
}

ScriptEditorDebugger::ScriptEditorDebugger(EditorNode *p_editor) {
	add_constant_override("margin_left", -EditorNode::get_singleton()->get_gui_base()->get_stylebox("BottomPanelDebuggerOverride", "EditorStyles")->get_margin(MARGIN_LEFT));
	add_constant_override("margin_right", -EditorNode::get_singleton()->get_gui_base()->get_stylebox("BottomPanelDebuggerOverride", "EditorStyles")->get_margin(MARGIN_RIGHT));

	ppeer = Ref<PacketPeerStream>(memnew(PacketPeerStream));
	ppeer->set_input_buffer_max_size((1024 * 1024 * 8) - 4); // 8 MiB should be enough, minus 4 bytes for separator.
	editor = p_editor;
	editor->get_inspector()->connect("object_id_selected", this, "_scene_tree_property_select_object");

	tabs = memnew(TabContainer);
	tabs->set_tab_align(TabContainer::ALIGN_LEFT);
	tabs->add_style_override("panel", editor->get_gui_base()->get_stylebox("DebuggerPanel", "EditorStyles"));
	tabs->add_style_override("tab_fg", editor->get_gui_base()->get_stylebox("DebuggerTabFG", "EditorStyles"));
	tabs->add_style_override("tab_bg", editor->get_gui_base()->get_stylebox("DebuggerTabBG", "EditorStyles"));
	tabs->connect("tab_changed", this, "_tab_changed");

	add_child(tabs);

	{ //debugger
		VBoxContainer *vbc = memnew(VBoxContainer);
		vbc->set_name(TTR("Debugger"));
		Control *dbg = vbc;

		HBoxContainer *hbc = memnew(HBoxContainer);
		vbc->add_child(hbc);

		reason = memnew(Label);
		reason->set_text("");
		hbc->add_child(reason);
		reason->set_h_size_flags(SIZE_EXPAND_FILL);
		reason->set_autowrap(true);
		reason->set_max_lines_visible(3);
		reason->set_mouse_filter(Control::MOUSE_FILTER_PASS);

		hbc->add_child(memnew(VSeparator));

		skip_breakpoints = memnew(ToolButton);
		hbc->add_child(skip_breakpoints);
		skip_breakpoints->set_tooltip(TTR("Skip Breakpoints"));
		skip_breakpoints->connect("pressed", this, "debug_skip_breakpoints");

		hbc->add_child(memnew(VSeparator));

		copy = memnew(ToolButton);
		hbc->add_child(copy);
		copy->set_tooltip(TTR("Copy Error"));
		copy->connect("pressed", this, "debug_copy");

		hbc->add_child(memnew(VSeparator));

		step = memnew(ToolButton);
		hbc->add_child(step);
		step->set_tooltip(TTR("Step Into"));
		step->set_shortcut(ED_GET_SHORTCUT("debugger/step_into"));
		step->connect("pressed", this, "debug_step");

		next = memnew(ToolButton);
		hbc->add_child(next);
		next->set_tooltip(TTR("Step Over"));
		next->set_shortcut(ED_GET_SHORTCUT("debugger/step_over"));
		next->connect("pressed", this, "debug_next");

		hbc->add_child(memnew(VSeparator));

		dobreak = memnew(ToolButton);
		hbc->add_child(dobreak);
		dobreak->set_tooltip(TTR("Break"));
		dobreak->set_shortcut(ED_GET_SHORTCUT("debugger/break"));
		dobreak->connect("pressed", this, "debug_break");

		docontinue = memnew(ToolButton);
		hbc->add_child(docontinue);
		docontinue->set_tooltip(TTR("Continue"));
		docontinue->set_shortcut(ED_GET_SHORTCUT("debugger/continue"));
		docontinue->connect("pressed", this, "debug_continue");

		back = memnew(Button);
		hbc->add_child(back);
		back->set_tooltip(TTR("Inspect Previous Instance"));
		back->hide();

		forward = memnew(Button);
		hbc->add_child(forward);
		forward->set_tooltip(TTR("Inspect Next Instance"));
		forward->hide();

		HSplitContainer *sc = memnew(HSplitContainer);
		vbc->add_child(sc);
		sc->set_v_size_flags(SIZE_EXPAND_FILL);

		stack_dump = memnew(Tree);
		stack_dump->set_allow_reselect(true);
		stack_dump->set_columns(1);
		stack_dump->set_column_titles_visible(true);
		stack_dump->set_column_title(0, TTR("Stack Frames"));
		stack_dump->set_h_size_flags(SIZE_EXPAND_FILL);
		stack_dump->set_hide_root(true);
		stack_dump->connect("cell_selected", this, "_stack_dump_frame_selected");
		sc->add_child(stack_dump);

		VBoxContainer *inspector_vbox = memnew(VBoxContainer);
		sc->add_child(inspector_vbox);

		HBoxContainer *tools_hb = memnew(HBoxContainer);
		inspector_vbox->add_child(tools_hb);

		search = memnew(LineEdit);
		search->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		search->set_placeholder(TTR("Filter stack variables"));
		search->set_clear_button_enabled(true);
		tools_hb->add_child(search);

		inspector = memnew(EditorInspector);
		inspector->set_h_size_flags(SIZE_EXPAND_FILL);
		inspector->set_v_size_flags(SIZE_EXPAND_FILL);
		inspector->set_property_name_style(EditorPropertyNameProcessor::STYLE_RAW);
		inspector->set_read_only(true);
		inspector->connect("object_id_selected", this, "_scene_tree_property_select_object");
		inspector->register_text_enter(search);
		inspector->set_use_filter(true);
		inspector_vbox->add_child(inspector);

		server.instance();

		pending_in_queue = 0;

		variables = memnew(ScriptEditorDebuggerVariables);

		breaked = false;

		tabs->add_child(dbg);
	}

	{ //errors
		errors_tab = memnew(VBoxContainer);
		errors_tab->set_name(TTR("Errors"));

		HBoxContainer *errhb = memnew(HBoxContainer);
		errors_tab->add_child(errhb);

		Button *expand_all = memnew(Button);
		expand_all->set_text(TTR("Expand All"));
		expand_all->connect("pressed", this, "_expand_errors_list");
		errhb->add_child(expand_all);

		Button *collapse_all = memnew(Button);
		collapse_all->set_text(TTR("Collapse All"));
		collapse_all->connect("pressed", this, "_collapse_errors_list");
		errhb->add_child(collapse_all);

		Control *space = memnew(Control);
		space->set_h_size_flags(SIZE_EXPAND_FILL);
		errhb->add_child(space);

		clearbutton = memnew(Button);
		clearbutton->set_text(TTR("Clear"));
		clearbutton->set_h_size_flags(0);
		clearbutton->connect("pressed", this, "_clear_errors_list");
		errhb->add_child(clearbutton);

		error_tree = memnew(Tree);
		error_tree->set_columns(2);

		error_tree->set_column_expand(0, false);
		error_tree->set_column_min_width(0, 140);

		error_tree->set_column_expand(1, true);

		error_tree->set_select_mode(Tree::SELECT_ROW);
		error_tree->set_hide_root(true);
		error_tree->set_v_size_flags(SIZE_EXPAND_FILL);
		error_tree->set_allow_rmb_select(true);
		error_tree->connect("item_rmb_selected", this, "_error_tree_item_rmb_selected");
		errors_tab->add_child(error_tree);

		item_menu = memnew(PopupMenu);
		item_menu->connect("id_pressed", this, "_item_menu_id_pressed");
		error_tree->add_child(item_menu);

		tabs->add_child(errors_tab);
	}

	{ // remote scene tree

		inspect_scene_tree = memnew(Tree);
		EditorNode::get_singleton()->get_scene_tree_dock()->add_remote_tree_editor(inspect_scene_tree);
		EditorNode::get_singleton()->get_scene_tree_dock()->connect("remote_tree_selected", this, "_scene_tree_selected");
		inspect_scene_tree->set_v_size_flags(SIZE_EXPAND_FILL);
		inspect_scene_tree->connect("cell_selected", this, "_scene_tree_selected");
		inspect_scene_tree->connect("item_collapsed", this, "_scene_tree_folded");
		inspect_scene_tree->set_allow_rmb_select(true);
		inspect_scene_tree->connect("item_rmb_selected", this, "_scene_tree_rmb_selected");
		auto_switch_remote_scene_tree = EDITOR_DEF("debugger/auto_switch_to_remote_scene_tree", false);
		inspect_scene_tree_timeout = EDITOR_DEF("debugger/remote_scene_tree_refresh_interval", 1.0);
		inspect_edited_object_timeout = EDITOR_DEF("debugger/remote_inspect_refresh_interval", 0.2);
		inspected_object_id = 0;
		updating_scene_tree = false;
	}

	{ // File dialog
		file_dialog = memnew(EditorFileDialog);
		file_dialog->connect("file_selected", this, "_file_selected");
		add_child(file_dialog);
	}

	{ //profiler
		profiler = memnew(EditorProfiler);
		profiler->set_name(TTR("Profiler"));
		tabs->add_child(profiler);
		profiler->connect("enable_profiling", this, "_profiler_activate");
		profiler->connect("break_request", this, "_profiler_seeked");
	}

	{ //network profiler
		network_profiler = memnew(EditorNetworkProfiler);
		network_profiler->set_name(TTR("Network Profiler"));
		tabs->add_child(network_profiler);
		network_profiler->connect("enable_profiling", this, "_network_profiler_activate");
	}

	{ //monitors

		HSplitContainer *hsp = memnew(HSplitContainer);

		perf_monitors = memnew(Tree);
		perf_monitors->set_columns(2);
		perf_monitors->set_column_title(0, TTR("Monitor"));
		perf_monitors->set_column_title(1, TTR("Value"));
		perf_monitors->set_column_titles_visible(true);
		perf_monitors->connect("item_edited", this, "_performance_select");
		hsp->add_child(perf_monitors);

		perf_draw = memnew(Control);
		perf_draw->set_clip_contents(true);
		perf_draw->connect("draw", this, "_performance_draw");
		hsp->add_child(perf_draw);

		hsp->set_name(TTR("Monitors"));
		hsp->set_split_offset(340 * EDSCALE);
		tabs->add_child(hsp);
		perf_max.resize(Performance::MONITOR_MAX);

		Map<String, TreeItem *> bases;
		TreeItem *root = perf_monitors->create_item();
		perf_monitors->set_hide_root(true);
		for (int i = 0; i < Performance::MONITOR_MAX; i++) {
			String n = Performance::get_singleton()->get_monitor_name(Performance::Monitor(i));
			Performance::MonitorType mtype = Performance::get_singleton()->get_monitor_type(Performance::Monitor(i));
			String base = EditorPropertyNameProcessor::get_singleton()->process_name(n.get_slice("/", 0), EditorPropertyNameProcessor::STYLE_CAPITALIZED);
			String name = EditorPropertyNameProcessor::get_singleton()->process_name(n.get_slice("/", 1), EditorPropertyNameProcessor::STYLE_CAPITALIZED);
			if (!bases.has(base)) {
				TreeItem *b = perf_monitors->create_item(root);
				b->set_text(0, base.capitalize());
				b->set_editable(0, false);
				b->set_selectable(0, false);
				b->set_expand_right(0, true);
				bases[base] = b;
			}

			TreeItem *it = perf_monitors->create_item(bases[base]);
			it->set_metadata(1, mtype);
			it->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
			it->set_editable(0, true);
			it->set_selectable(0, false);
			it->set_selectable(1, false);
			it->set_text(0, name.capitalize());
			perf_items.push_back(it);
			perf_max.write[i] = 0;
		}

		info_message = memnew(Label);
		info_message->set_text(TTR("Pick one or more items from the list to display the graph."));
		info_message->set_valign(Label::VALIGN_CENTER);
		info_message->set_align(Label::ALIGN_CENTER);
		info_message->set_autowrap(true);
		info_message->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
		info_message->set_anchors_and_margins_preset(PRESET_WIDE, PRESET_MODE_KEEP_SIZE, 8 * EDSCALE);
		perf_draw->add_child(info_message);
	}

	{ //vmem inspect
		VBoxContainer *vmem_vb = memnew(VBoxContainer);
		HBoxContainer *vmem_hb = memnew(HBoxContainer);
		Label *vmlb = memnew(Label(TTR("List of Video Memory Usage by Resource:") + " "));
		vmlb->set_h_size_flags(SIZE_EXPAND_FILL);
		vmem_hb->add_child(vmlb);
		vmem_hb->add_child(memnew(Label(TTR("Total:") + " ")));
		vmem_total = memnew(LineEdit);
		vmem_total->set_editable(false);
		vmem_total->set_custom_minimum_size(Size2(100, 0) * EDSCALE);
		vmem_hb->add_child(vmem_total);
		vmem_refresh = memnew(ToolButton);
		vmem_refresh->set_disabled(true);
		vmem_hb->add_child(vmem_refresh);
		vmem_export = memnew(ToolButton);
		vmem_export->set_tooltip(TTR("Export list to a CSV file"));
		vmem_hb->add_child(vmem_export);
		vmem_vb->add_child(vmem_hb);
		vmem_refresh->connect("pressed", this, "_video_mem_request");
		vmem_export->connect("pressed", this, "_video_mem_export");

		VBoxContainer *vmmc = memnew(VBoxContainer);
		vmem_tree = memnew(Tree);
		vmem_tree->set_v_size_flags(SIZE_EXPAND_FILL);
		vmem_tree->set_h_size_flags(SIZE_EXPAND_FILL);
		vmmc->add_child(vmem_tree);
		vmmc->set_v_size_flags(SIZE_EXPAND_FILL);
		vmem_vb->add_child(vmmc);

		vmem_vb->set_name(TTR("Video RAM"));
		vmem_tree->set_columns(4);
		vmem_tree->set_column_titles_visible(true);
		vmem_tree->set_column_title(0, TTR("Resource Path"));
		vmem_tree->set_column_expand(0, true);
		vmem_tree->set_column_expand(1, false);
		vmem_tree->set_column_title(1, TTR("Type"));
		vmem_tree->set_column_min_width(1, 100 * EDSCALE);
		vmem_tree->set_column_expand(2, false);
		vmem_tree->set_column_title(2, TTR("Format"));
		vmem_tree->set_column_min_width(2, 150 * EDSCALE);
		vmem_tree->set_column_expand(3, false);
		vmem_tree->set_column_title(3, TTR("Usage"));
		vmem_tree->set_column_min_width(3, 80 * EDSCALE);
		vmem_tree->set_hide_root(true);

		tabs->add_child(vmem_vb);
	}

	{ // misc
		VBoxContainer *misc = memnew(VBoxContainer);
		misc->set_name(TTR("Misc"));
		tabs->add_child(misc);

		GridContainer *info_left = memnew(GridContainer);
		info_left->set_columns(2);
		misc->add_child(info_left);
		clicked_ctrl = memnew(LineEdit);
		clicked_ctrl->set_h_size_flags(SIZE_EXPAND_FILL);
		info_left->add_child(memnew(Label(TTR("Clicked Control:"))));
		info_left->add_child(clicked_ctrl);
		clicked_ctrl_type = memnew(LineEdit);
		info_left->add_child(memnew(Label(TTR("Clicked Control Type:"))));
		info_left->add_child(clicked_ctrl_type);

		live_edit_root = memnew(LineEdit);
		live_edit_root->set_h_size_flags(SIZE_EXPAND_FILL);

		{
			HBoxContainer *lehb = memnew(HBoxContainer);
			Label *l = memnew(Label(TTR("Live Edit Root:")));
			info_left->add_child(l);
			lehb->add_child(live_edit_root);
			le_set = memnew(Button(TTR("Set From Tree")));
			lehb->add_child(le_set);
			le_clear = memnew(Button(TTR("Clear")));
			lehb->add_child(le_clear);
			info_left->add_child(lehb);
			le_set->set_disabled(true);
			le_clear->set_disabled(true);
		}

		misc->add_child(memnew(VSeparator));

		HBoxContainer *buttons = memnew(HBoxContainer);

		export_csv = memnew(Button(TTR("Export measures as CSV")));
		export_csv->connect("pressed", this, "_export_csv");
		buttons->add_child(export_csv);

		misc->add_child(buttons);
	}

	msgdialog = memnew(AcceptDialog);
	add_child(msgdialog);

	p_editor->get_undo_redo()->set_method_notify_callback(_method_changeds, this);
	p_editor->get_undo_redo()->set_property_notify_callback(_property_changeds, this);
	live_debug = true;
	camera_override = OVERRIDE_NONE;
	last_path_id = false;
	error_count = 0;
	warning_count = 0;
	hide_on_stop = true;
	enable_external_editor = false;
	last_error_count = 0;
	last_warning_count = 0;
	remote_port = 0;

	EditorNode::get_singleton()->get_pause_button()->connect("pressed", this, "_paused");
}

ScriptEditorDebugger::~ScriptEditorDebugger() {
	memdelete(variables);

	ppeer->set_stream_peer(Ref<StreamPeer>());

	server->stop();
	_clear_remote_objects();
}
