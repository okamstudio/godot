/**************************************************************************/
/*  scene_tree.cpp                                                        */
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

#include "scene_tree.h"

#include "core/io/marshalls.h"
#include "core/io/resource_loader.h"
#include "core/message_queue.h"
#include "core/os/dir_access.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/project_settings.h"
#include "core/variant_parser.h"
#include "main/input_default.h"
#include "node.h"
#include "scene/animation/scene_tree_tween.h"
#include "scene/debugger/script_debugger_remote.h"
#include "scene/gui/shortcut.h"
#include "scene/resources/dynamic_font.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/scene_string_names.h"
#include "servers/navigation_server.h"
#include "servers/physics_2d_server.h"
#include "servers/physics_server.h"
#include "viewport.h"

#include "modules/modules_enabled.gen.h" // For freetype.

#include <stdio.h>
#include <stdlib.h>

void SceneTreeTimer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_time_left", "time"), &SceneTreeTimer::set_time_left);
	ClassDB::bind_method(D_METHOD("get_time_left"), &SceneTreeTimer::get_time_left);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "time_left"), "set_time_left", "get_time_left");

	ADD_SIGNAL(MethodInfo("timeout"));
}

void SceneTreeTimer::set_time_left(float p_time) {
	time_left = p_time;
}

float SceneTreeTimer::get_time_left() const {
	return time_left;
}

void SceneTreeTimer::set_pause_mode_process(bool p_pause_mode_process) {
	process_pause = p_pause_mode_process;
}

bool SceneTreeTimer::is_pause_mode_process() {
	return process_pause;
}

void SceneTreeTimer::set_ignore_time_scale(bool p_ignore) {
	ignore_time_scale = p_ignore;
}

bool SceneTreeTimer::is_ignore_time_scale() {
	return ignore_time_scale;
}

void SceneTreeTimer::release_connections() {
	List<Connection> connections;
	get_all_signal_connections(&connections);

	for (List<Connection>::Element *E = connections.front(); E; E = E->next()) {
		Connection const &connection = E->get();
		disconnect(connection.signal, connection.target, connection.method);
	}
}

SceneTreeTimer::SceneTreeTimer() {
	time_left = 0;
	process_pause = true;
}

// This should be called once per physics tick, to make sure the transform previous and current
// is kept up to date on the few spatials that are using client side physics interpolation
void SceneTree::ClientPhysicsInterpolation::physics_process() {
	for (SelfList<Spatial> *E = _spatials_list.first(); E;) {
		Spatial *spatial = E->self();

		SelfList<Spatial> *current = E;

		// get the next element here BEFORE we potentially delete one
		E = E->next();

		// This will return false if the spatial has timed out ..
		// i.e. If get_global_transform_interpolated() has not been called
		// for a few seconds, we can delete from the list to keep processing
		// to a minimum.
		if (!spatial->update_client_physics_interpolation_data()) {
			_spatials_list.remove(current);
		}
	}
}

void SceneTree::tree_changed() {
	tree_version++;
	emit_signal(tree_changed_name);
}

void SceneTree::node_added(Node *p_node) {
	emit_signal(node_added_name, p_node);
}

void SceneTree::node_removed(Node *p_node) {
	if (current_scene == p_node) {
		current_scene = nullptr;
	}
	emit_signal(node_removed_name, p_node);
	if (call_lock > 0) {
		call_skip.insert(p_node);
	}
}

void SceneTree::node_renamed(Node *p_node) {
	emit_signal(node_renamed_name, p_node);
}

SceneTree::Group *SceneTree::add_to_group(const StringName &p_group, Node *p_node) {
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	if (!E) {
		E = group_map.insert(p_group, Group());
	}

	ERR_FAIL_COND_V_MSG(E->get().nodes.find(p_node) != -1, &E->get(), "Already in group: " + p_group + ".");
	E->get().nodes.push_back(p_node);
	//E->get().last_tree_version=0;
	E->get().changed = true;
	return &E->get();
}

void SceneTree::remove_from_group(const StringName &p_group, Node *p_node) {
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	ERR_FAIL_COND(!E);

	E->get().nodes.erase(p_node);
	if (E->get().nodes.empty()) {
		group_map.erase(E);
	}
}

void SceneTree::make_group_changed(const StringName &p_group) {
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	if (E) {
		E->get().changed = true;
	}
}

void SceneTree::flush_transform_notifications() {
	SelfList<Node> *n = xform_change_list.first();
	while (n) {
		Node *node = n->self();
		SelfList<Node> *nx = n->next();
		xform_change_list.remove(n);
		n = nx;
		node->notification(NOTIFICATION_TRANSFORM_CHANGED);
	}
}

void SceneTree::_flush_ugc() {
	ugc_locked = true;

	while (unique_group_calls.size()) {
		Map<UGCall, Vector<Variant>>::Element *E = unique_group_calls.front();

		Variant v[VARIANT_ARG_MAX];
		for (int i = 0; i < E->get().size(); i++) {
			v[i] = E->get()[i];
		}

		static_assert(VARIANT_ARG_MAX == 8, "This code needs to be updated if VARIANT_ARG_MAX != 8");
		call_group_flags(GROUP_CALL_REALTIME, E->key().group, E->key().call, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);

		unique_group_calls.erase(E);
	}

	ugc_locked = false;
}

void SceneTree::_update_group_order(Group &g, bool p_use_priority) {
	if (!g.changed) {
		return;
	}
	if (g.nodes.empty()) {
		return;
	}

	Node **nodes = g.nodes.ptrw();
	int node_count = g.nodes.size();

	if (p_use_priority) {
		SortArray<Node *, Node::ComparatorWithPriority> node_sort;
		node_sort.sort(nodes, node_count);
	} else {
		SortArray<Node *, Node::Comparator> node_sort;
		node_sort.sort(nodes, node_count);
	}
	g.changed = false;
}

void SceneTree::call_group_flags(uint32_t p_call_flags, const StringName &p_group, const StringName &p_function, VARIANT_ARG_DECLARE) {
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	if (!E) {
		return;
	}
	Group &g = E->get();
	if (g.nodes.empty()) {
		return;
	}

	if (p_call_flags & GROUP_CALL_UNIQUE && !(p_call_flags & GROUP_CALL_REALTIME)) {
		ERR_FAIL_COND(ugc_locked);

		UGCall ug;
		ug.call = p_function;
		ug.group = p_group;

		if (unique_group_calls.has(ug)) {
			return;
		}

		VARIANT_ARGPTRS;

		Vector<Variant> args;
		for (int i = 0; i < VARIANT_ARG_MAX; i++) {
			if (argptr[i]->get_type() == Variant::NIL) {
				break;
			}
			args.push_back(*argptr[i]);
		}

		unique_group_calls[ug] = args;
		return;
	}

	_update_group_order(g);

	Vector<Node *> nodes_copy = g.nodes;
	Node **nodes = nodes_copy.ptrw();
	int node_count = nodes_copy.size();

	call_lock++;

	if (p_call_flags & GROUP_CALL_REVERSE) {
		for (int i = node_count - 1; i >= 0; i--) {
			if (call_lock && call_skip.has(nodes[i])) {
				continue;
			}

			if (p_call_flags & GROUP_CALL_REALTIME) {
				if (p_call_flags & GROUP_CALL_MULTILEVEL) {
					nodes[i]->call_multilevel(p_function, VARIANT_ARG_PASS);
				} else {
					nodes[i]->call(p_function, VARIANT_ARG_PASS);
				}
			} else {
				MessageQueue::get_singleton()->push_call(nodes[i], p_function, VARIANT_ARG_PASS);
			}
		}

	} else {
		for (int i = 0; i < node_count; i++) {
			if (call_lock && call_skip.has(nodes[i])) {
				continue;
			}

			if (p_call_flags & GROUP_CALL_REALTIME) {
				if (p_call_flags & GROUP_CALL_MULTILEVEL) {
					nodes[i]->call_multilevel(p_function, VARIANT_ARG_PASS);
				} else {
					nodes[i]->call(p_function, VARIANT_ARG_PASS);
				}
			} else {
				MessageQueue::get_singleton()->push_call(nodes[i], p_function, VARIANT_ARG_PASS);
			}
		}
	}

	call_lock--;
	if (call_lock == 0) {
		call_skip.clear();
	}
}

void SceneTree::notify_group_flags(uint32_t p_call_flags, const StringName &p_group, int p_notification) {
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	if (!E) {
		return;
	}
	Group &g = E->get();
	if (g.nodes.empty()) {
		return;
	}

	_update_group_order(g);

	Vector<Node *> nodes_copy = g.nodes;
	Node **nodes = nodes_copy.ptrw();
	int node_count = nodes_copy.size();

	call_lock++;

	if (p_call_flags & GROUP_CALL_REVERSE) {
		for (int i = node_count - 1; i >= 0; i--) {
			if (call_lock && call_skip.has(nodes[i])) {
				continue;
			}

			if (p_call_flags & GROUP_CALL_REALTIME) {
				nodes[i]->notification(p_notification);
			} else {
				MessageQueue::get_singleton()->push_notification(nodes[i], p_notification);
			}
		}

	} else {
		for (int i = 0; i < node_count; i++) {
			if (call_lock && call_skip.has(nodes[i])) {
				continue;
			}

			if (p_call_flags & GROUP_CALL_REALTIME) {
				nodes[i]->notification(p_notification);
			} else {
				MessageQueue::get_singleton()->push_notification(nodes[i], p_notification);
			}
		}
	}

	call_lock--;
	if (call_lock == 0) {
		call_skip.clear();
	}
}

void SceneTree::set_group_flags(uint32_t p_call_flags, const StringName &p_group, const String &p_name, const Variant &p_value) {
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	if (!E) {
		return;
	}
	Group &g = E->get();
	if (g.nodes.empty()) {
		return;
	}

	_update_group_order(g);

	Vector<Node *> nodes_copy = g.nodes;
	Node **nodes = nodes_copy.ptrw();
	int node_count = nodes_copy.size();

	call_lock++;

	if (p_call_flags & GROUP_CALL_REVERSE) {
		for (int i = node_count - 1; i >= 0; i--) {
			if (call_lock && call_skip.has(nodes[i])) {
				continue;
			}

			if (p_call_flags & GROUP_CALL_REALTIME) {
				nodes[i]->set(p_name, p_value);
			} else {
				MessageQueue::get_singleton()->push_set(nodes[i], p_name, p_value);
			}
		}

	} else {
		for (int i = 0; i < node_count; i++) {
			if (call_lock && call_skip.has(nodes[i])) {
				continue;
			}

			if (p_call_flags & GROUP_CALL_REALTIME) {
				nodes[i]->set(p_name, p_value);
			} else {
				MessageQueue::get_singleton()->push_set(nodes[i], p_name, p_value);
			}
		}
	}

	call_lock--;
	if (call_lock == 0) {
		call_skip.clear();
	}
}

void SceneTree::call_group(const StringName &p_group, const StringName &p_function, VARIANT_ARG_DECLARE) {
	call_group_flags(0, p_group, p_function, VARIANT_ARG_PASS);
}

void SceneTree::notify_group(const StringName &p_group, int p_notification) {
	notify_group_flags(0, p_group, p_notification);
}

void SceneTree::set_group(const StringName &p_group, const String &p_name, const Variant &p_value) {
	set_group_flags(0, p_group, p_name, p_value);
}

void SceneTree::set_input_as_handled() {
	input_handled = true;
}

void SceneTree::input_text(const String &p_text) {
	root_lock++;

	call_group_flags(GROUP_CALL_REALTIME, "_viewports", "_vp_input_text", p_text); //special one for GUI, as controls use their own process check

	root_lock--;
}

bool SceneTree::is_input_handled() {
	return input_handled;
}

void SceneTree::input_event(const Ref<InputEvent> &p_event) {
	if (Engine::get_singleton()->is_editor_hint() && (Object::cast_to<InputEventJoypadButton>(p_event.ptr()) || Object::cast_to<InputEventJoypadMotion>(*p_event))) {
		return; //avoid joy input on editor
	}

	current_event++;
	root_lock++;

	input_handled = false;

	// Don't make const ref unless you can find and fix what caused GH-34691.
	Ref<InputEvent> ev = p_event;

	MainLoop::input_event(ev);

	call_group_flags(GROUP_CALL_REALTIME, "_viewports", "_vp_input", ev); //special one for GUI, as controls use their own process check

	if (ScriptDebugger::get_singleton() && ScriptDebugger::get_singleton()->is_remote()) {
		// Quit from game window using the stop shortcut (F8 by default).
		// The custom shortcut is provided via environment variable when running from the editor.
		if (debugger_stop_shortcut.is_null()) {
			String shortcut_str = OS::get_singleton()->get_environment("__GODOT_EDITOR_STOP_SHORTCUT__");
			if (!shortcut_str.empty()) {
				Variant shortcut_var;

				VariantParser::StreamString ss;
				ss.s = shortcut_str;

				String errs;
				int line;
				VariantParser::parse(&ss, shortcut_var, errs, line);
				debugger_stop_shortcut = shortcut_var;
			}

			if (debugger_stop_shortcut.is_null()) {
				// Define a default shortcut if it wasn't provided or is invalid.
				Ref<InputEventKey> ie;
				ie.instance();
				ie->set_scancode(KEY_F8);
				ie->set_unicode(KEY_F8);
				debugger_stop_shortcut.instance();
				debugger_stop_shortcut->set_shortcut(ie);
			}
		}

		Ref<InputEventKey> k = ev;
		if (k.is_valid() && k->is_pressed() && !k->is_echo() && debugger_stop_shortcut->is_shortcut(k)) {
			ScriptDebugger::get_singleton()->request_quit();
		}
	}

	_flush_ugc();
	root_lock--;
	//MessageQueue::get_singleton()->flush(); //flushing here causes UI and other places slowness

	root_lock++;

	if (!input_handled) {
		call_group_flags(GROUP_CALL_REALTIME, "_viewports", "_vp_unhandled_input", ev); //special one for GUI, as controls use their own process check
		_flush_ugc();
		//		input_handled = true; - no reason to set this as handled
		root_lock--;
		//MessageQueue::get_singleton()->flush(); //flushing here causes UI and other places slowness
	} else {
		//		input_handled = true; - no reason to set this as handled
		root_lock--;
	}

	_call_idle_callbacks();
}

void SceneTree::init() {
	ERR_FAIL_COND(!root);
	initialized = true;
	root->_set_tree(this);
	MainLoop::init();
}

void SceneTree::set_physics_interpolation_enabled(bool p_enabled) {
	// disallow interpolation in editor
	if (Engine::get_singleton()->is_editor_hint()) {
		p_enabled = false;
	}

	if (p_enabled == _physics_interpolation_enabled) {
		return;
	}

	_physics_interpolation_enabled = p_enabled;
	VisualServer::get_singleton()->set_physics_interpolation_enabled(p_enabled);
}

bool SceneTree::is_physics_interpolation_enabled() const {
	return _physics_interpolation_enabled;
}

void SceneTree::client_physics_interpolation_add_spatial(SelfList<Spatial> *p_elem) {
	// This ensures that _update_physics_interpolation_data() will be called at least once every
	// physics tick, to ensure the previous and current transforms are kept up to date.
	_client_physics_interpolation._spatials_list.add(p_elem);
}

void SceneTree::client_physics_interpolation_remove_spatial(SelfList<Spatial> *p_elem) {
	_client_physics_interpolation._spatials_list.remove(p_elem);
}

void SceneTree::iteration_prepare() {
	if (_physics_interpolation_enabled) {
		// Make sure any pending transforms from the last tick / frame
		// are flushed before pumping the interpolation prev and currents.
		flush_transform_notifications();
		VisualServer::get_singleton()->tick();

		// Any objects performing client physics interpolation
		// should be given an opportunity to keep their previous transforms
		// up to date before each new physics tick.
		_client_physics_interpolation.physics_process();
	}
}

void SceneTree::iteration_end() {
	// When physics interpolation is active, we want all pending transforms
	// to be flushed to the VisualServer before finishing a physics tick.
	if (_physics_interpolation_enabled) {
		flush_transform_notifications();
	}
}

bool SceneTree::iteration(float p_time) {
	root_lock++;

	current_frame++;

	flush_transform_notifications();

	if (MainLoop::iteration(p_time)) {
		_quit = true;
	}
	physics_process_time = p_time;

	emit_signal("physics_frame");

	_notify_group_pause("physics_process_internal", Node::NOTIFICATION_INTERNAL_PHYSICS_PROCESS);
	if (GLOBAL_GET("physics/common/enable_pause_aware_picking")) {
		call_group_flags(GROUP_CALL_REALTIME, "_viewports", "_process_picking", true);
	}
	_notify_group_pause("physics_process", Node::NOTIFICATION_PHYSICS_PROCESS);
	_flush_ugc();
	MessageQueue::get_singleton()->flush(); //small little hack

	process_tweens(p_time, true);

	flush_transform_notifications();
	call_group_flags(GROUP_CALL_REALTIME, "_viewports", "update_worlds");
	root_lock--;

	_flush_delete_queue();
	_call_idle_callbacks();

	return _quit;
}

void SceneTree::_update_font_oversampling(float p_ratio) {
#ifdef MODULE_FREETYPE_ENABLED
	if (use_font_oversampling) {
		DynamicFontAtSize::font_oversampling = p_ratio;
		DynamicFont::update_oversampling();
	}
#endif // MODULE_FREETYPE_ENABLED
}

bool SceneTree::idle(float p_time) {
	//print_line("ram: "+itos(OS::get_singleton()->get_static_memory_usage())+" sram: "+itos(OS::get_singleton()->get_dynamic_memory_usage()));
	//print_line("node count: "+itos(get_node_count()));
	//print_line("TEXTURE RAM: "+itos(VS::get_singleton()->get_render_info(VS::INFO_TEXTURE_MEM_USED)));

	root_lock++;

	if (MainLoop::idle(p_time)) {
		_quit = true;
	}

	idle_process_time = p_time;

	if (multiplayer_poll) {
		multiplayer->poll();
	}

	emit_signal("idle_frame");

	MessageQueue::get_singleton()->flush(); //small little hack

	flush_transform_notifications();

	_notify_group_pause("idle_process_internal", Node::NOTIFICATION_INTERNAL_PROCESS);
	_notify_group_pause("idle_process", Node::NOTIFICATION_PROCESS);

	Size2 win_size = OS::get_singleton()->get_window_size();

	if (win_size != last_screen_size) {
		last_screen_size = win_size;
		_update_root_rect();
		emit_signal("screen_resized");
	}

	_flush_ugc();
	MessageQueue::get_singleton()->flush(); //small little hack
	flush_transform_notifications(); //transforms after world update, to avoid unnecessary enter/exit notifications
	call_group_flags(GROUP_CALL_REALTIME, "_viewports", "update_worlds");

	root_lock--;

	_flush_delete_queue();

	//go through timers

	List<Ref<SceneTreeTimer>>::Element *L = timers.back(); //last element

	for (List<Ref<SceneTreeTimer>>::Element *E = timers.front(); E;) {
		List<Ref<SceneTreeTimer>>::Element *N = E->next();
		if (pause && !E->get()->is_pause_mode_process()) {
			if (E == L) {
				break; //break on last, so if new timers were added during list traversal, ignore them.
			}
			E = N;
			continue;
		}

		float time_left = E->get()->get_time_left();
		if (E->get()->is_ignore_time_scale()) {
			time_left -= Engine::get_singleton()->get_idle_frame_step();
		} else {
			time_left -= p_time;
		}
		E->get()->set_time_left(time_left);

		if (time_left < 0) {
			E->get()->emit_signal("timeout");
			timers.erase(E);
		}
		if (E == L) {
			break; //break on last, so if new timers were added during list traversal, ignore them.
		}
		E = N;
	}

	process_tweens(p_time, false);

	flush_transform_notifications(); //additional transforms after timers update

	_call_idle_callbacks();

	ProjectSettings::get_singleton()->update();

#ifdef TOOLS_ENABLED

	if (Engine::get_singleton()->is_editor_hint()) {
		//simple hack to reload fallback environment if it changed from editor
		String env_path = ProjectSettings::get_singleton()->get("rendering/environment/default_environment");
		env_path = env_path.strip_edges(); //user may have added a space or two
		String cpath;
		Ref<Environment> fallback = get_root()->get_world()->get_fallback_environment();
		if (fallback.is_valid()) {
			cpath = fallback->get_path();
		}
		if (cpath != env_path) {
			if (env_path != String()) {
				fallback = ResourceLoader::load(env_path);
				if (fallback.is_null()) {
					//could not load fallback, set as empty
					ProjectSettings::get_singleton()->set("rendering/environment/default_environment", "");
				}
			} else {
				fallback.unref();
			}
			get_root()->get_world()->set_fallback_environment(fallback);
		}
	}

#endif

	VisualServer::get_singleton()->pre_draw(true);

	return _quit;
}

void SceneTree::process_tweens(float p_delta, bool p_physics) {
	// This methods works similarly to how SceneTreeTimers are handled.
	List<Ref<SceneTreeTween>>::Element *L = tweens.back();

	for (List<Ref<SceneTreeTween>>::Element *E = tweens.front(); E;) {
		List<Ref<SceneTreeTween>>::Element *N = E->next();
		// Don't process if paused or process mode doesn't match.
		if (!E->get()->can_process(pause) || (p_physics == (E->get()->get_process_mode() == Tween::TWEEN_PROCESS_IDLE))) {
			if (E == L) {
				break;
			}
			E = N;
			continue;
		}

		if (!E->get()->step(p_delta)) {
			E->get()->clear();
			tweens.erase(E);
		}
		if (E == L) {
			break;
		}
		E = N;
	}
}

void SceneTree::finish() {
	_flush_delete_queue();

	_flush_ugc();

	initialized = false;

	MainLoop::finish();

	if (root) {
		root->_set_tree(nullptr);
		root->_propagate_after_exit_branch(true);
		memdelete(root); //delete root
		root = nullptr;
	}

	// In case deletion of some objects was queued when destructing the `root`.
	// E.g. if `queue_free()` was called for some node outside the tree when handling NOTIFICATION_PREDELETE for some node in the tree.
	_flush_delete_queue();

	// Cleanup timers.
	for (List<Ref<SceneTreeTimer>>::Element *E = timers.front(); E; E = E->next()) {
		E->get()->release_connections();
	}
	timers.clear();

	// Cleanup tweens.
	for (List<Ref<SceneTreeTween>>::Element *E = tweens.front(); E; E = E->next()) {
		E->get()->clear();
	}
	tweens.clear();
}

void SceneTree::quit(int p_exit_code) {
	if (p_exit_code >= 0) {
		// Override the exit code if a positive argument is given (the default is `-1`).
		// This is a shorthand for calling `set_exit_code()` on the OS singleton then quitting.
		OS::get_singleton()->set_exit_code(p_exit_code);
	} else if (!OS::get_singleton()->is_custom_exit_code()) {
		// Must customize exit code, otherwise it will default to a non-zero value
		OS::get_singleton()->set_exit_code(EXIT_SUCCESS);
	}

	_quit = true;
}

void SceneTree::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_WM_QUIT_REQUEST: {
			get_root()->propagate_notification(p_notification);

			if (accept_quit) {
				_quit = true;
				break;
			}
		} break;

		case NOTIFICATION_WM_GO_BACK_REQUEST: {
			get_root()->propagate_notification(p_notification);

			if (quit_on_go_back) {
				_quit = true;
				break;
			}
		} break;

		case NOTIFICATION_WM_FOCUS_IN: {
			InputDefault *id = Object::cast_to<InputDefault>(Input::get_singleton());
			if (id) {
				id->ensure_touch_mouse_raised();
			}

			get_root()->propagate_notification(p_notification);
		} break;

		case NOTIFICATION_TRANSLATION_CHANGED: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				get_root()->propagate_notification(p_notification);
			}
		} break;

		case NOTIFICATION_WM_UNFOCUS_REQUEST: {
			notify_group_flags(GROUP_CALL_REALTIME | GROUP_CALL_MULTILEVEL, "input", NOTIFICATION_WM_UNFOCUS_REQUEST);

			get_root()->propagate_notification(p_notification);

		} break;

		case NOTIFICATION_OS_MEMORY_WARNING:
		case NOTIFICATION_OS_IME_UPDATE:
		case NOTIFICATION_WM_MOUSE_ENTER:
		case NOTIFICATION_WM_MOUSE_EXIT:
		case NOTIFICATION_WM_FOCUS_OUT:
		case NOTIFICATION_WM_ABOUT:
		case NOTIFICATION_CRASH:
		case NOTIFICATION_APP_RESUMED:
		case NOTIFICATION_APP_PAUSED: {
			get_root()->propagate_notification(p_notification);
		} break;

		default:
			break;
	};
};

bool SceneTree::is_auto_accept_quit() const {
	return accept_quit;
}

void SceneTree::set_auto_accept_quit(bool p_enable) {
	accept_quit = p_enable;
}

bool SceneTree::is_quit_on_go_back() const {
	return quit_on_go_back;
}

void SceneTree::set_quit_on_go_back(bool p_enable) {
	quit_on_go_back = p_enable;
}

#ifdef TOOLS_ENABLED

bool SceneTree::is_node_being_edited(const Node *p_node) const {
	return Engine::get_singleton()->is_editor_hint() && edited_scene_root && (edited_scene_root->is_a_parent_of(p_node) || edited_scene_root == p_node);
}
#endif

#ifdef DEBUG_ENABLED
void SceneTree::set_debug_collisions_hint(bool p_enabled) {
	debug_collisions_hint = p_enabled;
}

bool SceneTree::is_debugging_collisions_hint() const {
	return debug_collisions_hint;
}

void SceneTree::set_debug_navigation_hint(bool p_enabled) {
	debug_navigation_hint = p_enabled;
}

bool SceneTree::is_debugging_navigation_hint() const {
	return debug_navigation_hint;
}
#endif

void SceneTree::set_debug_collisions_color(const Color &p_color) {
	debug_collisions_color = p_color;
}

Color SceneTree::get_debug_collisions_color() const {
	return debug_collisions_color;
}

void SceneTree::set_debug_collision_contact_color(const Color &p_color) {
	debug_collision_contact_color = p_color;
}

Color SceneTree::get_debug_collision_contact_color() const {
	return debug_collision_contact_color;
}

void SceneTree::set_debug_navigation_color(const Color &p_color) {
	debug_navigation_color = p_color;
}

Color SceneTree::get_debug_navigation_color() const {
	return debug_navigation_color;
}

void SceneTree::set_debug_navigation_disabled_color(const Color &p_color) {
	debug_navigation_disabled_color = p_color;
}

Color SceneTree::get_debug_navigation_disabled_color() const {
	return debug_navigation_disabled_color;
}

Ref<Material> SceneTree::get_debug_navigation_material() {
	if (navigation_material.is_valid()) {
		return navigation_material;
	}

	Ref<Material3D> line_material = Ref<Material3D>(memnew(SpatialMaterial));
	line_material->set_flag(Material3D::FLAG_UNSHADED, true);
	line_material->set_feature(Material3D::FEATURE_TRANSPARENT, true);
	line_material->set_flag(Material3D::FLAG_SRGB_VERTEX_COLOR, true);
	line_material->set_flag(Material3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	line_material->set_albedo(get_debug_navigation_color());

	navigation_material = line_material;

	return navigation_material;
}

Ref<Material> SceneTree::get_debug_navigation_disabled_material() {
	if (navigation_disabled_material.is_valid()) {
		return navigation_disabled_material;
	}

	Ref<Material3D> line_material = Ref<Material3D>(memnew(SpatialMaterial));
	line_material->set_flag(Material3D::FLAG_UNSHADED, true);
	line_material->set_feature(Material3D::FEATURE_TRANSPARENT, true);
	line_material->set_flag(Material3D::FLAG_SRGB_VERTEX_COLOR, true);
	line_material->set_flag(Material3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	line_material->set_albedo(get_debug_navigation_disabled_color());

	navigation_disabled_material = line_material;

	return navigation_disabled_material;
}
Ref<Material> SceneTree::get_debug_collision_material() {
	if (collision_material.is_valid()) {
		return collision_material;
	}

	Ref<Material3D> line_material = Ref<Material3D>(memnew(SpatialMaterial));
	line_material->set_flag(Material3D::FLAG_UNSHADED, true);
	line_material->set_feature(Material3D::FEATURE_TRANSPARENT, true);
	line_material->set_flag(Material3D::FLAG_SRGB_VERTEX_COLOR, true);
	line_material->set_flag(Material3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	line_material->set_albedo(get_debug_collisions_color());

	collision_material = line_material;

	return collision_material;
}

Ref<ArrayMesh> SceneTree::get_debug_contact_mesh() {
	if (debug_contact_mesh.is_valid()) {
		return debug_contact_mesh;
	}

	debug_contact_mesh = Ref<ArrayMesh>(memnew(ArrayMesh));

	Ref<Material3D> mat = Ref<Material3D>(memnew(SpatialMaterial));
	mat->set_flag(Material3D::FLAG_UNSHADED, true);
	mat->set_feature(Material3D::FEATURE_TRANSPARENT, true);
	mat->set_flag(Material3D::FLAG_SRGB_VERTEX_COLOR, true);
	mat->set_flag(Material3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	mat->set_albedo(get_debug_collision_contact_color());

	Vector3 diamond[6] = {
		Vector3(-1, 0, 0),
		Vector3(1, 0, 0),
		Vector3(0, -1, 0),
		Vector3(0, 1, 0),
		Vector3(0, 0, -1),
		Vector3(0, 0, 1)
	};

	/* clang-format off */
	int diamond_faces[8 * 3] = {
		0, 2, 4,
		0, 3, 4,
		1, 2, 4,
		1, 3, 4,
		0, 2, 5,
		0, 3, 5,
		1, 2, 5,
		1, 3, 5,
	};
	/* clang-format on */

	PoolVector<int> indices;
	for (int i = 0; i < 8 * 3; i++) {
		indices.push_back(diamond_faces[i]);
	}

	PoolVector<Vector3> vertices;
	for (int i = 0; i < 6; i++) {
		vertices.push_back(diamond[i] * 0.1);
	}

	Array arr;
	arr.resize(Mesh::ARRAY_MAX);
	arr[Mesh::ARRAY_VERTEX] = vertices;
	arr[Mesh::ARRAY_INDEX] = indices;

	debug_contact_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arr);
	debug_contact_mesh->surface_set_material(0, mat);

	return debug_contact_mesh;
}

void SceneTree::set_pause(bool p_enabled) {
	if (p_enabled == pause) {
		return;
	}
	pause = p_enabled;
	NavigationServer::get_singleton()->set_active(!p_enabled);
	PhysicsServer::get_singleton()->set_active(!p_enabled);
	Physics2DServer::get_singleton()->set_active(!p_enabled);
	if (get_root()) {
		get_root()->propagate_notification(p_enabled ? Node::NOTIFICATION_PAUSED : Node::NOTIFICATION_UNPAUSED);
	}
}

bool SceneTree::is_paused() const {
	return pause;
}

void SceneTree::_call_input_pause(const StringName &p_group, const StringName &p_method, const Ref<InputEvent> &p_input) {
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	if (!E) {
		return;
	}
	Group &g = E->get();
	if (g.nodes.empty()) {
		return;
	}

	_update_group_order(g);

	//copy, so copy on write happens in case something is removed from process while being called
	//performance is not lost because only if something is added/removed the vector is copied.
	Vector<Node *> nodes_copy = g.nodes;

	int node_count = nodes_copy.size();
	Node **nodes = nodes_copy.ptrw();

	Variant arg = p_input;
	const Variant *v[1] = { &arg };

	call_lock++;

	for (int i = node_count - 1; i >= 0; i--) {
		if (input_handled) {
			break;
		}

		Node *n = nodes[i];
		if (call_lock && call_skip.has(n)) {
			continue;
		}

		if (!n->can_process()) {
			continue;
		}

		n->call_multilevel(p_method, (const Variant **)v, 1);
		//ERR_FAIL_COND(node_count != g.nodes.size());
	}

	call_lock--;
	if (call_lock == 0) {
		call_skip.clear();
	}
}

void SceneTree::_notify_group_pause(const StringName &p_group, int p_notification) {
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	if (!E) {
		return;
	}
	Group &g = E->get();
	if (g.nodes.empty()) {
		return;
	}

	_update_group_order(g, p_notification == Node::NOTIFICATION_PROCESS || p_notification == Node::NOTIFICATION_INTERNAL_PROCESS || p_notification == Node::NOTIFICATION_PHYSICS_PROCESS || p_notification == Node::NOTIFICATION_INTERNAL_PHYSICS_PROCESS);

	//copy, so copy on write happens in case something is removed from process while being called
	//performance is not lost because only if something is added/removed the vector is copied.
	Vector<Node *> nodes_copy = g.nodes;

	int node_count = nodes_copy.size();
	Node **nodes = nodes_copy.ptrw();

	call_lock++;

	for (int i = 0; i < node_count; i++) {
		Node *n = nodes[i];
		if (call_lock && call_skip.has(n)) {
			continue;
		}

		if (!n->can_process()) {
			continue;
		}
		if (!n->can_process_notification(p_notification)) {
			continue;
		}

		n->notification(p_notification);
		//ERR_FAIL_COND(node_count != g.nodes.size());
	}

	call_lock--;
	if (call_lock == 0) {
		call_skip.clear();
	}
}

/*
void SceneMainLoop::_update_listener_2d() {

	if (listener_2d.is_valid()) {

		SpatialSound2DServer::get_singleton()->listener_set_space( listener_2d, world_2d->get_sound_space() );
	}

}
*/

Variant SceneTree::_call_group_flags(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	r_error.error = Variant::CallError::CALL_OK;

	ERR_FAIL_COND_V(p_argcount < 3, Variant());
	ERR_FAIL_COND_V(!p_args[0]->is_num(), Variant());
	ERR_FAIL_COND_V(p_args[1]->get_type() != Variant::STRING, Variant());
	ERR_FAIL_COND_V(p_args[2]->get_type() != Variant::STRING, Variant());

	int flags = *p_args[0];
	StringName group = *p_args[1];
	StringName method = *p_args[2];
	Variant v[VARIANT_ARG_MAX];

	for (int i = 0; i < MIN(p_argcount - 3, VARIANT_ARG_MAX); i++) {
		v[i] = *p_args[i + 3];
	}

	static_assert(VARIANT_ARG_MAX == 8, "This code needs to be updated if VARIANT_ARG_MAX != 8");
	call_group_flags(flags, group, method, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
	return Variant();
}

Variant SceneTree::_call_group(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	r_error.error = Variant::CallError::CALL_OK;

	ERR_FAIL_COND_V(p_argcount < 2, Variant());
	ERR_FAIL_COND_V(p_args[0]->get_type() != Variant::STRING, Variant());
	ERR_FAIL_COND_V(p_args[1]->get_type() != Variant::STRING, Variant());

	StringName group = *p_args[0];
	StringName method = *p_args[1];
	Variant v[VARIANT_ARG_MAX];

	for (int i = 0; i < MIN(p_argcount - 2, 5); i++) {
		v[i] = *p_args[i + 2];
	}

	call_group_flags(0, group, method, v[0], v[1], v[2], v[3], v[4]);
	return Variant();
}

int64_t SceneTree::get_frame() const {
	return current_frame;
}
int64_t SceneTree::get_event_count() const {
	return current_event;
}

Array SceneTree::_get_nodes_in_group(const StringName &p_group) {
	Array ret;
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	if (!E) {
		return ret;
	}

	_update_group_order(E->get()); //update order just in case
	int nc = E->get().nodes.size();
	if (nc == 0) {
		return ret;
	}

	ret.resize(nc);

	Node **ptr = E->get().nodes.ptrw();
	for (int i = 0; i < nc; i++) {
		ret[i] = ptr[i];
	}

	return ret;
}

Node *SceneTree::_get_first_node_in_group(const StringName &p_group) {
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	if (!E) {
		return nullptr; // No group.
	}

	_update_group_order(E->get()); // Update order just in case.

	if (E->get().nodes.empty()) {
		return nullptr;
	}

	return E->get().nodes[0];
}

bool SceneTree::has_group(const StringName &p_identifier) const {
	return group_map.has(p_identifier);
}
void SceneTree::get_nodes_in_group(const StringName &p_group, List<Node *> *p_list) {
	Map<StringName, Group>::Element *E = group_map.find(p_group);
	if (!E) {
		return;
	}

	_update_group_order(E->get()); //update order just in case
	int nc = E->get().nodes.size();
	if (nc == 0) {
		return;
	}
	Node **ptr = E->get().nodes.ptrw();
	for (int i = 0; i < nc; i++) {
		p_list->push_back(ptr[i]);
	}
}

void SceneTree::_flush_delete_queue() {
	_THREAD_SAFE_METHOD_

	// Sorting the delete queue by child count (in respect to their parent)
	// is an optimization because nodes benefit immensely from being deleted
	// in reverse order to their child count. This is partly due to ordered_remove(), and partly
	// due to notifications being sent to children that are moved, further in the child list.
	struct ObjectIDComparator {
		_FORCE_INLINE_ bool operator()(const DeleteQueueElement &p, const DeleteQueueElement &q) const {
			return (p.child_list_id > q.child_list_id);
		}
	};

	delete_queue.sort_custom<ObjectIDComparator>();

	for (uint32_t e = 0; e < delete_queue.size(); e++) {
		ObjectID id = delete_queue[e].id;
		Object *obj = ObjectDB::get_instance(id);
		if (obj) {
			memdelete(obj);
		}
	}

	delete_queue.clear();
}

void SceneTree::queue_delete(Object *p_object) {
	_THREAD_SAFE_METHOD_
	ERR_FAIL_NULL(p_object);

	// Guard against the user queueing multiple times,
	// which is unnecessary.
	if (p_object->is_queued_for_deletion()) {
		return;
	}

	p_object->_is_queued_for_deletion = true;

	DeleteQueueElement e;
	e.id = p_object->get_instance_id();

	// Storing the list id within the parent allows us
	// to sort the delete queue in reverse for more efficient
	// deletion.
	// Note that data.pos could alternatively be read during flush_delete_queue(),
	// however reading it here avoids an extra lookup, and should be correct in most cases.
	// And worst case if the child_list_id changes in the meantime, it will still work, it may just
	// be slightly slower.
	const Node *node = Object::cast_to<Node>(p_object);
	if (node) {
		e.child_list_id = node->data.pos;

		// Have some grouping by parent object ID,
		// so that children tend to be deleted together.
		// This should be more cache friendly.
		if (node->data.parent) {
			ObjectID parent_id = node->data.parent->get_instance_id();

			// Use a prime number to combine the group with the child id.
			// Provided there are less than the prime number children in a node,
			// there will be no collisions. Even if there are collisions, it is no problem.
			uint32_t group = parent_id * 937;

			// Rollover the group, we never want the group + the child id
			// to overflow 31 bits
			group &= ~(0b111 << 29);
			e.child_list_id += (int32_t)group;
		}
	} else {
		// For non-nodes, there is no point in sorting them.
		e.child_list_id = -2;
	}

	delete_queue.push_back(e);
}

int SceneTree::get_node_count() const {
	return node_count;
}

void SceneTree::_update_root_rect() {
	if (stretch_mode == STRETCH_MODE_DISABLED) {
		_update_font_oversampling(stretch_scale);
		root->set_size(last_screen_size.floor());
		root->set_attach_to_screen_rect(Rect2(Point2(), last_screen_size));
		root->set_size_override_stretch(true);
		root->set_size_override(true, (last_screen_size / stretch_scale).floor());
		root->update_canvas_items();
		return; //user will take care
	}

	//actual screen video mode
	Size2 video_mode = OS::get_singleton()->get_window_size();
	Size2 desired_res = stretch_min;

	Size2 viewport_size;
	Size2 screen_size;

	float viewport_aspect = desired_res.aspect();
	float video_mode_aspect = video_mode.aspect();

	if (stretch_aspect == STRETCH_ASPECT_IGNORE || Math::is_equal_approx(viewport_aspect, video_mode_aspect)) {
		//same aspect or ignore aspect
		viewport_size = desired_res;
		screen_size = video_mode;
	} else if (viewport_aspect < video_mode_aspect) {
		// screen ratio is smaller vertically

		if (stretch_aspect == STRETCH_ASPECT_KEEP_HEIGHT || stretch_aspect == STRETCH_ASPECT_EXPAND) {
			//will stretch horizontally
			viewport_size.x = desired_res.y * video_mode_aspect;
			viewport_size.y = desired_res.y;
			screen_size = video_mode;

		} else {
			//will need black bars
			viewport_size = desired_res;
			screen_size.x = video_mode.y * viewport_aspect;
			screen_size.y = video_mode.y;
		}
	} else {
		//screen ratio is smaller horizontally
		if (stretch_aspect == STRETCH_ASPECT_KEEP_WIDTH || stretch_aspect == STRETCH_ASPECT_EXPAND) {
			//will stretch horizontally
			viewport_size.x = desired_res.x;
			viewport_size.y = desired_res.x / video_mode_aspect;
			screen_size = video_mode;

		} else {
			//will need black bars
			viewport_size = desired_res;
			screen_size.x = video_mode.x;
			screen_size.y = video_mode.x / viewport_aspect;
		}
	}

	screen_size = screen_size.floor();
	viewport_size = viewport_size.floor();

	Size2 margin;
	Size2 offset;
	//black bars and margin
	if (stretch_aspect != STRETCH_ASPECT_EXPAND && screen_size.x < video_mode.x) {
		margin.x = Math::round((video_mode.x - screen_size.x) / 2.0);
		VisualServer::get_singleton()->black_bars_set_margins(margin.x, 0, margin.x, 0);
		offset.x = Math::round(margin.x * viewport_size.y / screen_size.y);
	} else if (stretch_aspect != STRETCH_ASPECT_EXPAND && screen_size.y < video_mode.y) {
		margin.y = Math::round((video_mode.y - screen_size.y) / 2.0);
		VisualServer::get_singleton()->black_bars_set_margins(0, margin.y, 0, margin.y);
		offset.y = Math::round(margin.y * viewport_size.x / screen_size.x);
	} else {
		VisualServer::get_singleton()->black_bars_set_margins(0, 0, 0, 0);
	}

	switch (stretch_mode) {
		case STRETCH_MODE_DISABLED: {
			// Already handled above
		} break;
		case STRETCH_MODE_2D: {
			Size2 render_size = screen_size;
			if (max_render_size.x > 0 && max_render_size.y > 0 && (screen_size.x > max_render_size.x || screen_size.y > max_render_size.y)) {
				render_size = max_render_size;
			}
			_update_font_oversampling((render_size.x / viewport_size.x) * stretch_scale); //screen / viewport ratio drives oversampling
			root->set_size(render_size);
			root->set_attach_to_screen_rect(Rect2(margin, screen_size));
			root->set_size_override_stretch(true);
			root->set_size_override(true, (viewport_size / stretch_scale).floor());
			root->update_canvas_items(); //force them to update just in case

		} break;
		case STRETCH_MODE_VIEWPORT: {
			_update_font_oversampling(1.0);
			root->set_size((viewport_size / stretch_scale).floor());
			root->set_attach_to_screen_rect(Rect2(margin, screen_size));
			root->set_size_override_stretch(false);
			root->set_size_override(false, Size2());
			root->update_canvas_items(); //force them to update just in case

		} break;
	}
}

void SceneTree::set_screen_stretch(StretchMode p_mode, StretchAspect p_aspect, const Size2 &p_minsize, real_t p_scale) {
	stretch_mode = p_mode;
	stretch_aspect = p_aspect;
	stretch_min = p_minsize;
	stretch_scale = p_scale;
	_update_root_rect();
}

void SceneTree::set_max_render_size(const Size2 &p_max_render_size) {
	max_render_size = p_max_render_size;
	_update_root_rect();
}

Size2 SceneTree::get_max_render_size() {
	return max_render_size;
}

void SceneTree::set_edited_scene_root(Node *p_node) {
#ifdef TOOLS_ENABLED
	edited_scene_root = p_node;
#endif
}

Node *SceneTree::get_edited_scene_root() const {
#ifdef TOOLS_ENABLED
	return edited_scene_root;
#else
	return NULL;
#endif
}

void SceneTree::set_current_scene(Node *p_scene) {
	ERR_FAIL_COND(p_scene && p_scene->get_parent() != root);
	current_scene = p_scene;
}

Node *SceneTree::get_current_scene() const {
	return current_scene;
}

void SceneTree::_change_scene(Node *p_to) {
	if (current_scene) {
		memdelete(current_scene);
		current_scene = nullptr;
	}

	// If we're quitting, abort.
	if (unlikely(_quit)) {
		if (p_to) { // Prevent memory leak.
			memdelete(p_to);
		}
		return;
	}

	if (p_to) {
		current_scene = p_to;
		root->add_child(p_to);
	}
}

Error SceneTree::change_scene(const String &p_path) {
	Ref<PackedScene> new_scene = ResourceLoader::load(p_path);
	if (new_scene.is_null()) {
		return ERR_CANT_OPEN;
	}

	return change_scene_to(new_scene);
}

Error SceneTree::change_scene_to(const Ref<PackedScene> &p_scene) {
	Node *new_scene = nullptr;
	if (p_scene.is_valid()) {
		new_scene = p_scene->instance();
		ERR_FAIL_COND_V(!new_scene, ERR_CANT_CREATE);
	}

	call_deferred("_change_scene", new_scene);
	return OK;
}

Error SceneTree::reload_current_scene() {
	ERR_FAIL_COND_V(!current_scene, ERR_UNCONFIGURED);
	String fname = current_scene->get_filename();
	return change_scene(fname);
}

void SceneTree::add_current_scene(Node *p_current) {
	current_scene = p_current;
	root->add_child(p_current);
}

#ifdef DEBUG_ENABLED

static void _fill_array(Node *p_node, Array &array, int p_level) {
	array.push_back(p_node->get_child_count());
	array.push_back(p_node->get_name());
	array.push_back(p_node->get_class());
	array.push_back(p_node->get_instance_id());
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_fill_array(p_node->get_child(i), array, p_level + 1);
	}
}

void SceneTree::_debugger_request_tree() {
	Array arr;
	_fill_array(root, arr, 0);
	ScriptDebugger::get_singleton()->send_message("scene_tree", arr);
}

void SceneTree::_live_edit_node_path_func(const NodePath &p_path, int p_id) {
	live_edit_node_path_cache[p_id] = p_path;
}

void SceneTree::_live_edit_res_path_func(const String &p_path, int p_id) {
	live_edit_resource_cache[p_id] = p_path;
}

void SceneTree::_live_edit_node_set_func(int p_id, const StringName &p_prop, const Variant &p_value) {
	if (!live_edit_node_path_cache.has(p_id)) {
		return;
	}

	NodePath np = live_edit_node_path_cache[p_id];
	Node *base = nullptr;
	if (root->has_node(live_edit_root)) {
		base = root->get_node(live_edit_root);
	}

	Map<String, Set<Node *>>::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {
		Node *n = F->get();

		if (base && !base->is_a_parent_of(n)) {
			continue;
		}

		if (!n->has_node(np)) {
			continue;
		}
		Node *n2 = n->get_node(np);

		n2->set(p_prop, p_value);
	}
}

void SceneTree::_live_edit_node_set_res_func(int p_id, const StringName &p_prop, const String &p_value) {
	RES r = ResourceLoader::load(p_value);
	if (!r.is_valid()) {
		return;
	}
	_live_edit_node_set_func(p_id, p_prop, r);
}
void SceneTree::_live_edit_node_call_func(int p_id, const StringName &p_method, VARIANT_ARG_DECLARE) {
	if (!live_edit_node_path_cache.has(p_id)) {
		return;
	}

	NodePath np = live_edit_node_path_cache[p_id];
	Node *base = nullptr;
	if (root->has_node(live_edit_root)) {
		base = root->get_node(live_edit_root);
	}

	Map<String, Set<Node *>>::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {
		Node *n = F->get();

		if (base && !base->is_a_parent_of(n)) {
			continue;
		}

		if (!n->has_node(np)) {
			continue;
		}
		Node *n2 = n->get_node(np);

		n2->call(p_method, VARIANT_ARG_PASS);
	}
}
void SceneTree::_live_edit_res_set_func(int p_id, const StringName &p_prop, const Variant &p_value) {
	if (!live_edit_resource_cache.has(p_id)) {
		return;
	}

	String resp = live_edit_resource_cache[p_id];

	if (!ResourceCache::has(resp)) {
		return;
	}

	RES r = ResourceCache::get(resp);
	if (!r.is_valid()) {
		return;
	}

	r->set(p_prop, p_value);
}
void SceneTree::_live_edit_res_set_res_func(int p_id, const StringName &p_prop, const String &p_value) {
	RES r = ResourceLoader::load(p_value);
	if (!r.is_valid()) {
		return;
	}
	_live_edit_res_set_func(p_id, p_prop, r);
}
void SceneTree::_live_edit_res_call_func(int p_id, const StringName &p_method, VARIANT_ARG_DECLARE) {
	if (!live_edit_resource_cache.has(p_id)) {
		return;
	}

	String resp = live_edit_resource_cache[p_id];

	if (!ResourceCache::has(resp)) {
		return;
	}

	RES r = ResourceCache::get(resp);
	if (!r.is_valid()) {
		return;
	}

	r->call(p_method, VARIANT_ARG_PASS);
}

void SceneTree::_live_edit_root_func(const NodePath &p_scene_path, const String &p_scene_from) {
	live_edit_root = p_scene_path;
	live_edit_scene = p_scene_from;
}

void SceneTree::_live_edit_create_node_func(const NodePath &p_parent, const String &p_type, const String &p_name) {
	Node *base = nullptr;
	if (root->has_node(live_edit_root)) {
		base = root->get_node(live_edit_root);
	}

	Map<String, Set<Node *>>::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {
		Node *n = F->get();

		if (base && !base->is_a_parent_of(n)) {
			continue;
		}

		if (!n->has_node(p_parent)) {
			continue;
		}
		Node *n2 = n->get_node(p_parent);

		Node *no = Object::cast_to<Node>(ClassDB::instance(p_type));
		if (!no) {
			continue;
		}

		no->set_name(p_name);
		n2->add_child(no);
	}
}
void SceneTree::_live_edit_instance_node_func(const NodePath &p_parent, const String &p_path, const String &p_name) {
	Ref<PackedScene> ps = ResourceLoader::load(p_path);

	if (!ps.is_valid()) {
		return;
	}

	Node *base = nullptr;
	if (root->has_node(live_edit_root)) {
		base = root->get_node(live_edit_root);
	}

	Map<String, Set<Node *>>::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {
		Node *n = F->get();

		if (base && !base->is_a_parent_of(n)) {
			continue;
		}

		if (!n->has_node(p_parent)) {
			continue;
		}
		Node *n2 = n->get_node(p_parent);

		Node *no = ps->instance();
		if (!no) {
			continue;
		}

		no->set_name(p_name);
		n2->add_child(no);
	}
}
void SceneTree::_live_edit_remove_node_func(const NodePath &p_at) {
	Node *base = nullptr;
	if (root->has_node(live_edit_root)) {
		base = root->get_node(live_edit_root);
	}

	Map<String, Set<Node *>>::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Set<Node *>::Element *F = E->get().front(); F;) {
		Set<Node *>::Element *N = F->next();

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n)) {
			continue;
		}

		if (!n->has_node(p_at)) {
			continue;
		}
		Node *n2 = n->get_node(p_at);

		memdelete(n2);

		F = N;
	}
}
void SceneTree::_live_edit_remove_and_keep_node_func(const NodePath &p_at, ObjectID p_keep_id) {
	Node *base = nullptr;
	if (root->has_node(live_edit_root)) {
		base = root->get_node(live_edit_root);
	}

	Map<String, Set<Node *>>::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Set<Node *>::Element *F = E->get().front(); F;) {
		Set<Node *>::Element *N = F->next();

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n)) {
			continue;
		}

		if (!n->has_node(p_at)) {
			continue;
		}

		Node *n2 = n->get_node(p_at);

		n2->get_parent()->remove_child(n2);

		live_edit_remove_list[n][p_keep_id] = n2;

		F = N;
	}
}
void SceneTree::_live_edit_restore_node_func(ObjectID p_id, const NodePath &p_at, int p_at_pos) {
	Node *base = nullptr;
	if (root->has_node(live_edit_root)) {
		base = root->get_node(live_edit_root);
	}

	Map<String, Set<Node *>>::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Set<Node *>::Element *F = E->get().front(); F;) {
		Set<Node *>::Element *N = F->next();

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n)) {
			continue;
		}

		if (!n->has_node(p_at)) {
			continue;
		}
		Node *n2 = n->get_node(p_at);

		Map<Node *, Map<ObjectID, Node *>>::Element *EN = live_edit_remove_list.find(n);

		if (!EN) {
			continue;
		}

		Map<ObjectID, Node *>::Element *FN = EN->get().find(p_id);

		if (!FN) {
			continue;
		}
		n2->add_child(FN->get());

		EN->get().erase(FN);

		if (EN->get().size() == 0) {
			live_edit_remove_list.erase(EN);
		}

		F = N;
	}
}
void SceneTree::_live_edit_duplicate_node_func(const NodePath &p_at, const String &p_new_name) {
	Node *base = nullptr;
	if (root->has_node(live_edit_root)) {
		base = root->get_node(live_edit_root);
	}

	Map<String, Set<Node *>>::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {
		Node *n = F->get();

		if (base && !base->is_a_parent_of(n)) {
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
void SceneTree::_live_edit_reparent_node_func(const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos) {
	Node *base = nullptr;
	if (root->has_node(live_edit_root)) {
		base = root->get_node(live_edit_root);
	}

	Map<String, Set<Node *>>::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E) {
		return; //scene not editable
	}

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {
		Node *n = F->get();

		if (base && !base->is_a_parent_of(n)) {
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

#endif

void SceneTree::drop_files(const Vector<String> &p_files, int p_from_screen) {
	emit_signal("files_dropped", p_files, p_from_screen);
	MainLoop::drop_files(p_files, p_from_screen);
}

void SceneTree::global_menu_action(const Variant &p_id, const Variant &p_meta) {
	emit_signal("global_menu_action", p_id, p_meta);
	MainLoop::global_menu_action(p_id, p_meta);
}

Ref<SceneTreeTimer> SceneTree::create_timer(float p_delay_sec, bool p_process_pause) {
	Ref<SceneTreeTimer> stt;
	stt.instance();
	stt->set_pause_mode_process(p_process_pause);
	stt->set_time_left(p_delay_sec);
	timers.push_back(stt);
	return stt;
}

Ref<SceneTreeTween> SceneTree::create_tween() {
	Ref<SceneTreeTween> tween = memnew(SceneTreeTween(true));
	tweens.push_back(tween);
	return tween;
}

Array SceneTree::get_processed_tweens() {
	Array ret;
	ret.resize(tweens.size());

	for (int i = 0; i < tweens.size(); i++) {
		ret[i] = tweens[i];
	}

	return ret;
}

void SceneTree::_network_peer_connected(int p_id) {
	emit_signal("network_peer_connected", p_id);
}

void SceneTree::_network_peer_disconnected(int p_id) {
	emit_signal("network_peer_disconnected", p_id);
}

void SceneTree::_connected_to_server() {
	emit_signal("connected_to_server");
}

void SceneTree::_connection_failed() {
	emit_signal("connection_failed");
}

void SceneTree::_server_disconnected() {
	emit_signal("server_disconnected");
}

Ref<MultiplayerAPI> SceneTree::get_multiplayer() const {
	return multiplayer;
}

void SceneTree::set_multiplayer_poll_enabled(bool p_enabled) {
	multiplayer_poll = p_enabled;
}

bool SceneTree::is_multiplayer_poll_enabled() const {
	return multiplayer_poll;
}

void SceneTree::set_multiplayer(Ref<MultiplayerAPI> p_multiplayer) {
	ERR_FAIL_COND(!p_multiplayer.is_valid());

	if (multiplayer.is_valid()) {
		multiplayer->disconnect("network_peer_connected", this, "_network_peer_connected");
		multiplayer->disconnect("network_peer_disconnected", this, "_network_peer_disconnected");
		multiplayer->disconnect("connected_to_server", this, "_connected_to_server");
		multiplayer->disconnect("connection_failed", this, "_connection_failed");
		multiplayer->disconnect("server_disconnected", this, "_server_disconnected");
	}

	multiplayer = p_multiplayer;
	multiplayer->set_root_node(root);

	multiplayer->connect("network_peer_connected", this, "_network_peer_connected");
	multiplayer->connect("network_peer_disconnected", this, "_network_peer_disconnected");
	multiplayer->connect("connected_to_server", this, "_connected_to_server");
	multiplayer->connect("connection_failed", this, "_connection_failed");
	multiplayer->connect("server_disconnected", this, "_server_disconnected");
}

void SceneTree::set_network_peer(const Ref<NetworkedMultiplayerPeer> &p_network_peer) {
	multiplayer->set_network_peer(p_network_peer);
}

Ref<NetworkedMultiplayerPeer> SceneTree::get_network_peer() const {
	return multiplayer->get_network_peer();
}

bool SceneTree::is_network_server() const {
	return multiplayer->is_network_server();
}

bool SceneTree::has_network_peer() const {
	return multiplayer->has_network_peer();
}

int SceneTree::get_network_unique_id() const {
	return multiplayer->get_network_unique_id();
}

Vector<int> SceneTree::get_network_connected_peers() const {
	return multiplayer->get_network_connected_peers();
}

int SceneTree::get_rpc_sender_id() const {
	return multiplayer->get_rpc_sender_id();
}

void SceneTree::set_refuse_new_network_connections(bool p_refuse) {
	multiplayer->set_refuse_new_network_connections(p_refuse);
}

bool SceneTree::is_refusing_new_network_connections() const {
	return multiplayer->is_refusing_new_network_connections();
}

void SceneTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_root"), &SceneTree::get_root);
	ClassDB::bind_method(D_METHOD("has_group", "name"), &SceneTree::has_group);

	ClassDB::bind_method(D_METHOD("is_auto_accept_quit"), &SceneTree::is_auto_accept_quit);
	ClassDB::bind_method(D_METHOD("set_auto_accept_quit", "enabled"), &SceneTree::set_auto_accept_quit);
	ClassDB::bind_method(D_METHOD("is_quit_on_go_back"), &SceneTree::is_quit_on_go_back);
	ClassDB::bind_method(D_METHOD("set_quit_on_go_back", "enabled"), &SceneTree::set_quit_on_go_back);

	ClassDB::bind_method(D_METHOD("set_debug_collisions_hint", "enable"), &SceneTree::set_debug_collisions_hint);
	ClassDB::bind_method(D_METHOD("is_debugging_collisions_hint"), &SceneTree::is_debugging_collisions_hint);
	ClassDB::bind_method(D_METHOD("set_debug_navigation_hint", "enable"), &SceneTree::set_debug_navigation_hint);
	ClassDB::bind_method(D_METHOD("is_debugging_navigation_hint"), &SceneTree::is_debugging_navigation_hint);

	ClassDB::bind_method(D_METHOD("set_edited_scene_root", "scene"), &SceneTree::set_edited_scene_root);
	ClassDB::bind_method(D_METHOD("get_edited_scene_root"), &SceneTree::get_edited_scene_root);

	ClassDB::bind_method(D_METHOD("set_pause", "enable"), &SceneTree::set_pause);
	ClassDB::bind_method(D_METHOD("is_paused"), &SceneTree::is_paused);
	ClassDB::bind_method(D_METHOD("set_input_as_handled"), &SceneTree::set_input_as_handled);
	ClassDB::bind_method(D_METHOD("is_input_handled"), &SceneTree::is_input_handled);

	ClassDB::bind_method(D_METHOD("create_timer", "time_sec", "pause_mode_process"), &SceneTree::create_timer, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("create_tween"), &SceneTree::create_tween);
	ClassDB::bind_method(D_METHOD("get_processed_tweens"), &SceneTree::get_processed_tweens);

	ClassDB::bind_method(D_METHOD("get_node_count"), &SceneTree::get_node_count);
	ClassDB::bind_method(D_METHOD("get_frame"), &SceneTree::get_frame);
	ClassDB::bind_method(D_METHOD("quit", "exit_code"), &SceneTree::quit, DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("set_max_render_size", "max_render_size"), &SceneTree::set_max_render_size);
	ClassDB::bind_method(D_METHOD("set_screen_stretch", "mode", "aspect", "minsize", "scale"), &SceneTree::set_screen_stretch, DEFVAL(1));

	ClassDB::bind_method(D_METHOD("set_physics_interpolation_enabled", "enabled"), &SceneTree::set_physics_interpolation_enabled);
	ClassDB::bind_method(D_METHOD("is_physics_interpolation_enabled"), &SceneTree::is_physics_interpolation_enabled);

	ClassDB::bind_method(D_METHOD("queue_delete", "obj"), &SceneTree::queue_delete);

	MethodInfo mi;
	mi.name = "call_group_flags";
	mi.arguments.push_back(PropertyInfo(Variant::INT, "flags"));
	mi.arguments.push_back(PropertyInfo(Variant::STRING, "group"));
	mi.arguments.push_back(PropertyInfo(Variant::STRING, "method"));

	ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "call_group_flags", &SceneTree::_call_group_flags, mi);

	ClassDB::bind_method(D_METHOD("notify_group_flags", "call_flags", "group", "notification"), &SceneTree::notify_group_flags);
	ClassDB::bind_method(D_METHOD("set_group_flags", "call_flags", "group", "property", "value"), &SceneTree::set_group_flags);

	MethodInfo mi2;
	mi2.name = "call_group";
	mi2.arguments.push_back(PropertyInfo(Variant::STRING, "group"));
	mi2.arguments.push_back(PropertyInfo(Variant::STRING, "method"));

	ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "call_group", &SceneTree::_call_group, mi2);

	ClassDB::bind_method(D_METHOD("notify_group", "group", "notification"), &SceneTree::notify_group);
	ClassDB::bind_method(D_METHOD("set_group", "group", "property", "value"), &SceneTree::set_group);

	ClassDB::bind_method(D_METHOD("get_nodes_in_group", "group"), &SceneTree::_get_nodes_in_group);
	ClassDB::bind_method(D_METHOD("get_first_node_in_group", "group"), &SceneTree::_get_first_node_in_group);

	ClassDB::bind_method(D_METHOD("set_current_scene", "child_node"), &SceneTree::set_current_scene);
	ClassDB::bind_method(D_METHOD("get_current_scene"), &SceneTree::get_current_scene);

	ClassDB::bind_method(D_METHOD("change_scene", "path"), &SceneTree::change_scene);
	ClassDB::bind_method(D_METHOD("change_scene_to", "packed_scene"), &SceneTree::change_scene_to);

	ClassDB::bind_method(D_METHOD("reload_current_scene"), &SceneTree::reload_current_scene);

	ClassDB::bind_method(D_METHOD("_change_scene"), &SceneTree::_change_scene);

	ClassDB::bind_method(D_METHOD("set_multiplayer", "multiplayer"), &SceneTree::set_multiplayer);
	ClassDB::bind_method(D_METHOD("get_multiplayer"), &SceneTree::get_multiplayer);
	ClassDB::bind_method(D_METHOD("set_multiplayer_poll_enabled", "enabled"), &SceneTree::set_multiplayer_poll_enabled);
	ClassDB::bind_method(D_METHOD("is_multiplayer_poll_enabled"), &SceneTree::is_multiplayer_poll_enabled);
	ClassDB::bind_method(D_METHOD("set_network_peer", "peer"), &SceneTree::set_network_peer);
	ClassDB::bind_method(D_METHOD("get_network_peer"), &SceneTree::get_network_peer);
	ClassDB::bind_method(D_METHOD("is_network_server"), &SceneTree::is_network_server);
	ClassDB::bind_method(D_METHOD("has_network_peer"), &SceneTree::has_network_peer);
	ClassDB::bind_method(D_METHOD("get_network_connected_peers"), &SceneTree::get_network_connected_peers);
	ClassDB::bind_method(D_METHOD("get_network_unique_id"), &SceneTree::get_network_unique_id);
	ClassDB::bind_method(D_METHOD("get_rpc_sender_id"), &SceneTree::get_rpc_sender_id);
	ClassDB::bind_method(D_METHOD("set_refuse_new_network_connections", "refuse"), &SceneTree::set_refuse_new_network_connections);
	ClassDB::bind_method(D_METHOD("is_refusing_new_network_connections"), &SceneTree::is_refusing_new_network_connections);
	ClassDB::bind_method(D_METHOD("_network_peer_connected"), &SceneTree::_network_peer_connected);
	ClassDB::bind_method(D_METHOD("_network_peer_disconnected"), &SceneTree::_network_peer_disconnected);
	ClassDB::bind_method(D_METHOD("_connected_to_server"), &SceneTree::_connected_to_server);
	ClassDB::bind_method(D_METHOD("_connection_failed"), &SceneTree::_connection_failed);
	ClassDB::bind_method(D_METHOD("_server_disconnected"), &SceneTree::_server_disconnected);

	ClassDB::bind_method(D_METHOD("set_use_font_oversampling", "enable"), &SceneTree::set_use_font_oversampling);
	ClassDB::bind_method(D_METHOD("is_using_font_oversampling"), &SceneTree::is_using_font_oversampling);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_accept_quit"), "set_auto_accept_quit", "is_auto_accept_quit");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "quit_on_go_back"), "set_quit_on_go_back", "is_quit_on_go_back");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_collisions_hint"), "set_debug_collisions_hint", "is_debugging_collisions_hint");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_navigation_hint"), "set_debug_navigation_hint", "is_debugging_navigation_hint");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused"), "set_pause", "is_paused");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "refuse_new_network_connections"), "set_refuse_new_network_connections", "is_refusing_new_network_connections");
	ADD_PROPERTY_DEFAULT("refuse_new_network_connections", false);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_font_oversampling"), "set_use_font_oversampling", "is_using_font_oversampling");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "edited_scene_root", PROPERTY_HINT_RESOURCE_TYPE, "Node", 0), "set_edited_scene_root", "get_edited_scene_root");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "current_scene", PROPERTY_HINT_RESOURCE_TYPE, "Node", 0), "set_current_scene", "get_current_scene");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "network_peer", PROPERTY_HINT_RESOURCE_TYPE, "NetworkedMultiplayerPeer", 0), "set_network_peer", "get_network_peer");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "root", PROPERTY_HINT_RESOURCE_TYPE, "Node", 0), "", "get_root");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "multiplayer", PROPERTY_HINT_RESOURCE_TYPE, "MultiplayerAPI", 0), "set_multiplayer", "get_multiplayer");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "multiplayer_poll"), "set_multiplayer_poll_enabled", "is_multiplayer_poll_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "physics_interpolation"), "set_physics_interpolation_enabled", "is_physics_interpolation_enabled");

	ADD_SIGNAL(MethodInfo("tree_changed"));
	ADD_SIGNAL(MethodInfo("node_added", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("node_removed", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("node_renamed", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("screen_resized"));
	ADD_SIGNAL(MethodInfo("node_configuration_warning_changed", PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));

	ADD_SIGNAL(MethodInfo("idle_frame"));
	ADD_SIGNAL(MethodInfo("physics_frame"));

	ADD_SIGNAL(MethodInfo("files_dropped", PropertyInfo(Variant::POOL_STRING_ARRAY, "files"), PropertyInfo(Variant::INT, "screen")));
	ADD_SIGNAL(MethodInfo("global_menu_action",
			PropertyInfo(Variant::NIL, "id", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT),
			PropertyInfo(Variant::NIL, "meta", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NIL_IS_VARIANT)));
	ADD_SIGNAL(MethodInfo("network_peer_connected", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("network_peer_disconnected", PropertyInfo(Variant::INT, "id")));
	ADD_SIGNAL(MethodInfo("connected_to_server"));
	ADD_SIGNAL(MethodInfo("connection_failed"));
	ADD_SIGNAL(MethodInfo("server_disconnected"));

	BIND_ENUM_CONSTANT(GROUP_CALL_DEFAULT);
	BIND_ENUM_CONSTANT(GROUP_CALL_REVERSE);
	BIND_ENUM_CONSTANT(GROUP_CALL_REALTIME);
	BIND_ENUM_CONSTANT(GROUP_CALL_UNIQUE);

	BIND_ENUM_CONSTANT(STRETCH_MODE_DISABLED);
	BIND_ENUM_CONSTANT(STRETCH_MODE_2D);
	BIND_ENUM_CONSTANT(STRETCH_MODE_VIEWPORT);

	BIND_ENUM_CONSTANT(STRETCH_ASPECT_IGNORE);
	BIND_ENUM_CONSTANT(STRETCH_ASPECT_KEEP);
	BIND_ENUM_CONSTANT(STRETCH_ASPECT_KEEP_WIDTH);
	BIND_ENUM_CONSTANT(STRETCH_ASPECT_KEEP_HEIGHT);
	BIND_ENUM_CONSTANT(STRETCH_ASPECT_EXPAND);
}

SceneTree *SceneTree::singleton = nullptr;

SceneTree::IdleCallback SceneTree::idle_callbacks[SceneTree::MAX_IDLE_CALLBACKS];
int SceneTree::idle_callback_count = 0;

void SceneTree::_call_idle_callbacks() {
	for (int i = 0; i < idle_callback_count; i++) {
		idle_callbacks[i]();
	}
}

void SceneTree::add_idle_callback(IdleCallback p_callback) {
	ERR_FAIL_COND(idle_callback_count >= MAX_IDLE_CALLBACKS);
	idle_callbacks[idle_callback_count++] = p_callback;
}

void SceneTree::set_use_font_oversampling(bool p_oversampling) {
	if (use_font_oversampling == p_oversampling) {
		return;
	}

	use_font_oversampling = p_oversampling;
	_update_root_rect();
}

bool SceneTree::is_using_font_oversampling() const {
	return use_font_oversampling;
}

void SceneTree::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	if (p_function == "change_scene") {
		DirAccessRef dir_access = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		List<String> directories;
		directories.push_back(dir_access->get_current_dir());

		while (!directories.empty()) {
			dir_access->change_dir(directories.back()->get());
			directories.pop_back();

			dir_access->list_dir_begin();
			String filename = dir_access->get_next();

			while (filename != "") {
				if (filename == "." || filename == "..") {
					filename = dir_access->get_next();
					continue;
				}

				if (dir_access->dir_exists(filename)) {
					directories.push_back(dir_access->get_current_dir().plus_file(filename));
				} else if (filename.ends_with(".tscn") || filename.ends_with(".scn")) {
					r_options->push_back("\"" + dir_access->get_current_dir().plus_file(filename) + "\"");
				}

				filename = dir_access->get_next();
			}
		}
	}
}

SceneTree::SceneTree() {
	if (singleton == nullptr) {
		singleton = this;
	}
	_quit = false;
	accept_quit = true;
	quit_on_go_back = true;
	initialized = false;
	use_font_oversampling = false;
#ifdef DEBUG_ENABLED
	debug_collisions_hint = false;
	debug_navigation_hint = false;
#endif
	debug_collisions_color = GLOBAL_DEF("debug/shapes/collision/shape_color", Color(0.0, 0.6, 0.7, 0.42));
	debug_collision_contact_color = GLOBAL_DEF("debug/shapes/collision/contact_color", Color(1.0, 0.2, 0.1, 0.8));
	debug_navigation_color = GLOBAL_DEF("debug/shapes/navigation/geometry_color", Color(0.1, 1.0, 0.7, 0.4));
	debug_navigation_disabled_color = GLOBAL_DEF("debug/shapes/navigation/disabled_geometry_color", Color(1.0, 0.7, 0.1, 0.4));
	collision_debug_contacts = GLOBAL_DEF("debug/shapes/collision/max_contacts_displayed", 10000);
	ProjectSettings::get_singleton()->set_custom_property_info("debug/shapes/collision/max_contacts_displayed", PropertyInfo(Variant::INT, "debug/shapes/collision/max_contacts_displayed", PROPERTY_HINT_RANGE, "0,20000,1")); // No negative

	GLOBAL_DEF("debug/shapes/collision/draw_2d_outlines", true);

	tree_version = 1;
	physics_process_time = 0.f;
	idle_process_time = 0.f;

	root = nullptr;
	input_handled = false;
	pause = false;
	current_frame = 0;
	current_event = 0;
	tree_changed_name = "tree_changed";
	node_added_name = "node_added";
	node_removed_name = "node_removed";
	node_renamed_name = "node_renamed";
	ugc_locked = false;
	call_lock = 0;
	root_lock = 0;
	node_count = 0;
	_physics_interpolation_enabled = false;

	//create with mainloop

	root = memnew(Viewport);
	root->set_name("root");
	root->set_handle_input_locally(false);
	if (!root->get_world().is_valid()) {
		root->set_world(Ref<World>(memnew(World)));
	}

	set_physics_interpolation_enabled(GLOBAL_DEF("physics/common/physics_interpolation", false));
	// Always disable jitter fix if physics interpolation is enabled -
	// Jitter fix will interfere with interpolation, and is not necessary
	// when interpolation is active.
	if (is_physics_interpolation_enabled()) {
		Engine::get_singleton()->set_physics_jitter_fix(0);
	}

	// Initialize network state
	multiplayer_poll = true;
	set_multiplayer(Ref<MultiplayerAPI>(memnew(MultiplayerAPI)));

	root->set_as_audio_listener(true);
	root->set_as_audio_listener_2d(true);
	current_scene = nullptr;

	int ref_atlas_size = GLOBAL_DEF_RST("rendering/quality/reflections/atlas_size", 2048);
	ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/reflections/atlas_size", PropertyInfo(Variant::INT, "rendering/quality/reflections/atlas_size", PROPERTY_HINT_RANGE, "0,8192,1,or_greater")); // next_power_of_2 will return a 0 as min value.
	int ref_atlas_subdiv = GLOBAL_DEF_RST("rendering/quality/reflections/atlas_subdiv", 8);
	ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/reflections/atlas_subdiv", PropertyInfo(Variant::INT, "rendering/quality/reflections/atlas_subdiv", PROPERTY_HINT_RANGE, "0,32,1,or_greater")); // next_power_of_2 will return a 0 as min value.
	int msaa_mode = GLOBAL_DEF("rendering/quality/filters/msaa", 0);
	ProjectSettings::get_singleton()->set_custom_property_info("rendering/quality/filters/msaa", PropertyInfo(Variant::INT, "rendering/quality/filters/msaa", PROPERTY_HINT_ENUM, "Disabled,2x,4x,8x,16x,AndroidVR 2x,AndroidVR 4x"));
	root->set_msaa(Viewport::MSAA(msaa_mode));

	const bool use_fxaa = GLOBAL_DEF("rendering/quality/filters/use_fxaa", false);
	root->set_use_fxaa(use_fxaa);

	const bool transparent_background = GLOBAL_DEF("rendering/viewport/transparent_background", false);
	root->set_transparent_background(transparent_background);

	const bool use_debanding = GLOBAL_DEF("rendering/quality/filters/use_debanding", false);
	root->set_use_debanding(use_debanding);

	const float sharpen_intensity = GLOBAL_GET("rendering/quality/filters/sharpen_intensity");
	root->set_sharpen_intensity(sharpen_intensity);

	GLOBAL_DEF("rendering/quality/depth/hdr", true);
	GLOBAL_DEF("rendering/quality/depth/hdr.mobile", false);

	const bool hdr = GLOBAL_GET("rendering/quality/depth/hdr");
	root->set_hdr(hdr);

	GLOBAL_DEF("rendering/quality/depth/use_32_bpc_depth", false);

	const bool use_32_bpc_depth = GLOBAL_GET("rendering/quality/depth/use_32_bpc_depth");
	root->set_use_32_bpc_depth(use_32_bpc_depth);

	VS::get_singleton()->scenario_set_reflection_atlas_size(root->get_world()->get_scenario(), ref_atlas_size, ref_atlas_subdiv);

	{ //load default fallback environment
		//get possible extensions
		List<String> exts;
		ResourceLoader::get_recognized_extensions_for_type("Environment", &exts);
		String ext_hint;
		for (List<String>::Element *E = exts.front(); E; E = E->next()) {
			if (ext_hint != String()) {
				ext_hint += ",";
			}
			ext_hint += "*." + E->get();
		}
		//get path
		String env_path = GLOBAL_DEF("rendering/environment/default_environment", "");
		//setup property
		ProjectSettings::get_singleton()->set_custom_property_info("rendering/environment/default_environment", PropertyInfo(Variant::STRING, "rendering/viewport/default_environment", PROPERTY_HINT_FILE, ext_hint));
		env_path = env_path.strip_edges();
		if (env_path != String()) {
			Ref<Environment> env = ResourceLoader::load(env_path);
			if (env.is_valid()) {
				root->get_world()->set_fallback_environment(env);
			} else {
				if (Engine::get_singleton()->is_editor_hint()) {
					//file was erased, clear the field.
					ProjectSettings::get_singleton()->set("rendering/environment/default_environment", "");
				} else {
					//file was erased, notify user.
					ERR_PRINT(RTR("Default Environment as specified in Project Settings (Rendering -> Environment -> Default Environment) could not be loaded."));
				}
			}
		}
	}

	stretch_mode = STRETCH_MODE_DISABLED;
	stretch_aspect = STRETCH_ASPECT_IGNORE;
	stretch_scale = 1.0;

	last_screen_size = OS::get_singleton()->get_window_size();
	_update_root_rect();

	if (ScriptDebugger::get_singleton()) {
		if (ScriptDebugger::get_singleton()->is_remote()) {
			ScriptDebuggerRemote *remote_debugger = static_cast<ScriptDebuggerRemote *>(ScriptDebugger::get_singleton());

			remote_debugger->set_scene_tree(this);
		}
		ScriptDebugger::get_singleton()->set_multiplayer(multiplayer);
	}

	root->set_physics_object_picking(GLOBAL_DEF("physics/common/enable_object_picking", true));

#ifdef TOOLS_ENABLED
	edited_scene_root = nullptr;
#endif

#ifdef DEBUG_ENABLED

	live_edit_root = NodePath("/root");

#endif
}

SceneTree::~SceneTree() {
	if (root) {
		root->_set_tree(nullptr);
		root->_propagate_after_exit_branch(true);
		memdelete(root);
	}

	if (singleton == this) {
		singleton = nullptr;
	}
}
