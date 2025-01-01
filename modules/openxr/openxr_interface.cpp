/**************************************************************************/
/*  openxr_interface.cpp                                                  */
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

#include "openxr_interface.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"

#include "extensions/openxr_eye_gaze_interaction.h"
#include "extensions/openxr_hand_interaction_extension.h"
#include "servers/rendering/renderer_compositor.h"

#include <openxr/openxr.h>

void OpenXRInterface::_bind_methods() {
	// lifecycle signals
	ADD_SIGNAL(MethodInfo("session_begun"));
	ADD_SIGNAL(MethodInfo("session_stopping"));
	ADD_SIGNAL(MethodInfo("session_focussed"));
	ADD_SIGNAL(MethodInfo("session_visible"));
	ADD_SIGNAL(MethodInfo("session_loss_pending"));
	ADD_SIGNAL(MethodInfo("instance_exiting"));
	ADD_SIGNAL(MethodInfo("pose_recentered"));
	ADD_SIGNAL(MethodInfo("refresh_rate_changed", PropertyInfo(Variant::FLOAT, "refresh_rate")));

	// Display refresh rate
	ClassDB::bind_method(D_METHOD("get_display_refresh_rate"), &OpenXRInterface::get_display_refresh_rate);
	ClassDB::bind_method(D_METHOD("set_display_refresh_rate", "refresh_rate"), &OpenXRInterface::set_display_refresh_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "display_refresh_rate"), "set_display_refresh_rate", "get_display_refresh_rate");

	// Render Target size multiplier
	ClassDB::bind_method(D_METHOD("get_render_target_size_multiplier"), &OpenXRInterface::get_render_target_size_multiplier);
	ClassDB::bind_method(D_METHOD("set_render_target_size_multiplier", "multiplier"), &OpenXRInterface::set_render_target_size_multiplier);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "render_target_size_multiplier"), "set_render_target_size_multiplier", "get_render_target_size_multiplier");

	// Foveation level
	ClassDB::bind_method(D_METHOD("is_foveation_supported"), &OpenXRInterface::is_foveation_supported);

	ClassDB::bind_method(D_METHOD("get_foveation_level"), &OpenXRInterface::get_foveation_level);
	ClassDB::bind_method(D_METHOD("set_foveation_level", "foveation_level"), &OpenXRInterface::set_foveation_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "foveation_level"), "set_foveation_level", "get_foveation_level");

	ClassDB::bind_method(D_METHOD("get_foveation_dynamic"), &OpenXRInterface::get_foveation_dynamic);
	ClassDB::bind_method(D_METHOD("set_foveation_dynamic", "foveation_dynamic"), &OpenXRInterface::set_foveation_dynamic);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "foveation_dynamic"), "set_foveation_dynamic", "get_foveation_dynamic");

	// Action sets
	ClassDB::bind_method(D_METHOD("is_action_set_active", "name"), &OpenXRInterface::is_action_set_active);
	ClassDB::bind_method(D_METHOD("set_action_set_active", "name", "active"), &OpenXRInterface::set_action_set_active);
	ClassDB::bind_method(D_METHOD("get_action_sets"), &OpenXRInterface::get_action_sets);

	// Refresh rates
	ClassDB::bind_method(D_METHOD("get_available_display_refresh_rates"), &OpenXRInterface::get_available_display_refresh_rates);

	// Hand tracking.
	ClassDB::bind_method(D_METHOD("set_motion_range", "hand", "motion_range"), &OpenXRInterface::set_motion_range);
	ClassDB::bind_method(D_METHOD("get_motion_range", "hand"), &OpenXRInterface::get_motion_range);

	ClassDB::bind_method(D_METHOD("get_hand_tracking_source", "hand"), &OpenXRInterface::get_hand_tracking_source);

	ClassDB::bind_method(D_METHOD("get_hand_joint_flags", "hand", "joint"), &OpenXRInterface::get_hand_joint_flags);

	ClassDB::bind_method(D_METHOD("get_hand_joint_rotation", "hand", "joint"), &OpenXRInterface::get_hand_joint_rotation);
	ClassDB::bind_method(D_METHOD("get_hand_joint_position", "hand", "joint"), &OpenXRInterface::get_hand_joint_position);
	ClassDB::bind_method(D_METHOD("get_hand_joint_radius", "hand", "joint"), &OpenXRInterface::get_hand_joint_radius);

	ClassDB::bind_method(D_METHOD("get_hand_joint_linear_velocity", "hand", "joint"), &OpenXRInterface::get_hand_joint_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_hand_joint_angular_velocity", "hand", "joint"), &OpenXRInterface::get_hand_joint_angular_velocity);

	ClassDB::bind_method(D_METHOD("is_hand_tracking_supported"), &OpenXRInterface::is_hand_tracking_supported);
	ClassDB::bind_method(D_METHOD("is_hand_interaction_supported"), &OpenXRInterface::is_hand_interaction_supported);
	ClassDB::bind_method(D_METHOD("is_eye_gaze_interaction_supported"), &OpenXRInterface::is_eye_gaze_interaction_supported);

	// VRS
	ClassDB::bind_method(D_METHOD("get_vrs_min_radius"), &OpenXRInterface::get_vrs_min_radius);
	ClassDB::bind_method(D_METHOD("set_vrs_min_radius", "radius"), &OpenXRInterface::set_vrs_min_radius);
	ClassDB::bind_method(D_METHOD("get_vrs_strength"), &OpenXRInterface::get_vrs_strength);
	ClassDB::bind_method(D_METHOD("set_vrs_strength", "strength"), &OpenXRInterface::set_vrs_strength);

	ADD_GROUP("Vulkan VRS", "vrs_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vrs_min_radius", PROPERTY_HINT_RANGE, "1.0,100.0,1.0"), "set_vrs_min_radius", "get_vrs_min_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vrs_strength", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_vrs_strength", "get_vrs_strength");

	BIND_ENUM_CONSTANT(HAND_LEFT);
	BIND_ENUM_CONSTANT(HAND_RIGHT);
	BIND_ENUM_CONSTANT(HAND_MAX);

	BIND_ENUM_CONSTANT(HAND_MOTION_RANGE_UNOBSTRUCTED);
	BIND_ENUM_CONSTANT(HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER);
	BIND_ENUM_CONSTANT(HAND_MOTION_RANGE_MAX);

	BIND_ENUM_CONSTANT(HAND_TRACKED_SOURCE_UNKNOWN);
	BIND_ENUM_CONSTANT(HAND_TRACKED_SOURCE_UNOBSTRUCTED);
	BIND_ENUM_CONSTANT(HAND_TRACKED_SOURCE_CONTROLLER);
	BIND_ENUM_CONSTANT(HAND_TRACKED_SOURCE_MAX);

	BIND_ENUM_CONSTANT(HAND_JOINT_PALM);
	BIND_ENUM_CONSTANT(HAND_JOINT_WRIST);
	BIND_ENUM_CONSTANT(HAND_JOINT_THUMB_METACARPAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_THUMB_PROXIMAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_THUMB_DISTAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_THUMB_TIP);
	BIND_ENUM_CONSTANT(HAND_JOINT_INDEX_METACARPAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_INDEX_PROXIMAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_INDEX_INTERMEDIATE);
	BIND_ENUM_CONSTANT(HAND_JOINT_INDEX_DISTAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_INDEX_TIP);
	BIND_ENUM_CONSTANT(HAND_JOINT_MIDDLE_METACARPAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_MIDDLE_PROXIMAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_MIDDLE_INTERMEDIATE);
	BIND_ENUM_CONSTANT(HAND_JOINT_MIDDLE_DISTAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_MIDDLE_TIP);
	BIND_ENUM_CONSTANT(HAND_JOINT_RING_METACARPAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_RING_PROXIMAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_RING_INTERMEDIATE);
	BIND_ENUM_CONSTANT(HAND_JOINT_RING_DISTAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_RING_TIP);
	BIND_ENUM_CONSTANT(HAND_JOINT_LITTLE_METACARPAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_LITTLE_PROXIMAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_LITTLE_INTERMEDIATE);
	BIND_ENUM_CONSTANT(HAND_JOINT_LITTLE_DISTAL);
	BIND_ENUM_CONSTANT(HAND_JOINT_LITTLE_TIP);
	BIND_ENUM_CONSTANT(HAND_JOINT_MAX);

	BIND_BITFIELD_FLAG(HAND_JOINT_NONE);
	BIND_BITFIELD_FLAG(HAND_JOINT_ORIENTATION_VALID);
	BIND_BITFIELD_FLAG(HAND_JOINT_ORIENTATION_TRACKED);
	BIND_BITFIELD_FLAG(HAND_JOINT_POSITION_VALID);
	BIND_BITFIELD_FLAG(HAND_JOINT_POSITION_TRACKED);
	BIND_BITFIELD_FLAG(HAND_JOINT_LINEAR_VELOCITY_VALID);
	BIND_BITFIELD_FLAG(HAND_JOINT_ANGULAR_VELOCITY_VALID);
}

StringName OpenXRInterface::get_name() const {
	return StringName("OpenXR");
}

uint32_t OpenXRInterface::get_capabilities() const {
	return XRInterface::XR_VR + XRInterface::XR_STEREO;
}

PackedStringArray OpenXRInterface::get_suggested_tracker_names() const {
	// These are hardcoded in OpenXR, note that they will only be available if added to our action map

	PackedStringArray arr = {
		"head", // XRPositionalTracker for the users head (Mapped from OpenXR /user/head)
		"left_hand", // XRControllerTracker for the users left hand (Mapped from OpenXR /user/hand/left)
		"right_hand", // XRControllerTracker for the users right hand (Mapped from OpenXR /user/hand/right)
		"/user/hand_tracker/left", // XRHandTracker for the users left hand
		"/user/hand_tracker/right", // XRHandTracker for the users right hand
		"/user/body_tracker", // XRBodyTracker for the users body
		"/user/face_tracker", // XRFaceTracker for the users face
		"/user/treadmill"
	};

	for (OpenXRExtensionWrapper *wrapper : OpenXRAPI::get_singleton()->get_registered_extension_wrappers()) {
		arr.append_array(wrapper->get_suggested_tracker_names());
	}

	return arr;
}

XRInterface::TrackingStatus OpenXRInterface::get_tracking_status() const {
	return tracking_state;
}

void OpenXRInterface::_load_action_map() {
	ERR_FAIL_NULL(openxr_api);

	// This may seem a bit duplicitous to a little bit of background info here.
	// OpenXRActionMap (with all its sub resource classes) is a class that allows us to configure and store an action map in.
	// This gives the user the ability to edit the action map in a UI and customize the actions.
	// OpenXR however requires us to submit an action map and it takes over from that point and we can no longer change it.
	// This system does that push and we store the info needed to then work with this action map going forward.

	// Within our openxr device we maintain a number of classes that wrap the relevant OpenXR objects for this.
	// Within OpenXRInterface we have a few internal classes that keep track of what we've created.
	// This allow us to process the relevant actions each frame.

	// just in case clean up
	free_trackers();
	free_interaction_profiles();
	free_action_sets();

	Ref<OpenXRActionMap> action_map;
	if (Engine::get_singleton()->is_editor_hint()) {
#ifdef TOOLS_ENABLED
		action_map.instantiate();
		action_map->create_editor_action_sets();
#endif
	} else {
		String default_tres_name = openxr_api->get_default_action_map_resource_name();

		// Check if we can load our default
		if (ResourceLoader::exists(default_tres_name)) {
			action_map = ResourceLoader::load(default_tres_name);
		}

		// Check if we need to create default action set
		if (action_map.is_null()) {
			action_map.instantiate();
			action_map->create_default_action_sets();
#ifdef TOOLS_ENABLED
			// Save our action sets so our user can
			action_map->set_path(default_tres_name, true);
			ResourceSaver::save(action_map, default_tres_name);
#endif
		}
	}

	// process our action map
	if (action_map.is_valid()) {
		HashMap<Ref<OpenXRAction>, Action *> xr_actions;

		Array action_set_array = action_map->get_action_sets();
		for (int i = 0; i < action_set_array.size(); i++) {
			// Create our action set
			Ref<OpenXRActionSet> xr_action_set = action_set_array[i];
			ActionSet *action_set = create_action_set(xr_action_set->get_name(), xr_action_set->get_localized_name(), xr_action_set->get_priority());
			if (!action_set) {
				continue;
			}

			// Now create our actions for these
			Array actions = xr_action_set->get_actions();
			for (int j = 0; j < actions.size(); j++) {
				Ref<OpenXRAction> xr_action = actions[j];

				PackedStringArray toplevel_paths = xr_action->get_toplevel_paths();
				Vector<Tracker *> trackers_for_action;

				for (int k = 0; k < toplevel_paths.size(); k++) {
					// Only check for our tracker if our path is supported.
					if (openxr_api->is_top_level_path_supported(toplevel_paths[k])) {
						Tracker *tracker = find_tracker(toplevel_paths[k], true);
						if (tracker) {
							trackers_for_action.push_back(tracker);
						}
					}
				}

				// Only add our action if we have at least one valid toplevel path
				if (trackers_for_action.size() > 0) {
					Action *action = create_action(action_set, xr_action->get_name(), xr_action->get_localized_name(), xr_action->get_action_type(), trackers_for_action);
					if (action) {
						// add this to our map for creating our interaction profiles
						xr_actions[xr_action] = action;
					}
				}
			}
		}

		// now do our suggestions
		Array interaction_profile_array = action_map->get_interaction_profiles();
		for (int i = 0; i < interaction_profile_array.size(); i++) {
			Ref<OpenXRInteractionProfile> xr_interaction_profile = interaction_profile_array[i];

			// Note, we can only have one entry per interaction profile so if it already exists we clear it out
			RID ip = openxr_api->interaction_profile_create(xr_interaction_profile->get_interaction_profile_path());
			if (ip.is_valid()) {
				openxr_api->interaction_profile_clear_bindings(ip);

				for (Ref<OpenXRBindingModifier> xr_binding_modifier : xr_interaction_profile->get_binding_modifiers()) {
					PackedByteArray bm = xr_binding_modifier->get_ip_modification();
					if (!bm.is_empty()) {
						openxr_api->interaction_profile_add_modifier(ip, bm);
					}
				}

				Array xr_bindings = xr_interaction_profile->get_bindings();
				for (int j = 0; j < xr_bindings.size(); j++) {
					Ref<OpenXRIPBinding> xr_binding = xr_bindings[j];
					Ref<OpenXRAction> xr_action = xr_binding->get_action();

					Action *action = nullptr;
					if (xr_actions.has(xr_action)) {
						action = xr_actions[xr_action];
					} else {
						print_line("Action ", xr_action->get_name(), " isn't part of an action set!");
						continue;
					}

					int binding_no = openxr_api->interaction_profile_add_binding(ip, action->action_rid, xr_binding->get_binding_path());
					if (binding_no >= 0) {
						for (Ref<OpenXRBindingModifier> xr_binding_modifier : xr_binding->get_binding_modifiers()) {
							// Binding modifiers on bindings can be added to the interaction profile.
							PackedByteArray bm = xr_binding_modifier->get_ip_modification();
							if (!bm.is_empty()) {
								openxr_api->interaction_profile_add_modifier(ip, bm);
							}

							// And possibly in the future on the binding itself, we're just preparing for that eventuality.
						}
					}
				}

				// Now submit our suggestions
				openxr_api->interaction_profile_suggest_bindings(ip);

				// And record it in our array so we can clean it up later on
				if (interaction_profile_array.has(ip)) {
					interaction_profile_array.push_back(ip);
				}
			}
		}
	}
}

OpenXRInterface::ActionSet *OpenXRInterface::create_action_set(const String &p_action_set_name, const String &p_localized_name, const int p_priority) {
	ERR_FAIL_NULL_V(openxr_api, nullptr);

	// find if it already exists
	for (int i = 0; i < action_sets.size(); i++) {
		if (action_sets[i]->action_set_name == p_action_set_name) {
			// already exists in this set
			return nullptr;
		}
	}

	ActionSet *action_set = memnew(ActionSet);
	action_set->action_set_name = p_action_set_name;
	action_set->is_active = true;
	action_set->action_set_rid = openxr_api->action_set_create(p_action_set_name, p_localized_name, p_priority);
	action_sets.push_back(action_set);

	return action_set;
}

void OpenXRInterface::free_action_sets() {
	ERR_FAIL_NULL(openxr_api);

	for (int i = 0; i < action_sets.size(); i++) {
		ActionSet *action_set = action_sets[i];

		free_actions(action_set);

		openxr_api->action_set_free(action_set->action_set_rid);

		memfree(action_set);
	}
	action_sets.clear();
}

OpenXRInterface::Action *OpenXRInterface::create_action(ActionSet *p_action_set, const String &p_action_name, const String &p_localized_name, OpenXRAction::ActionType p_action_type, const Vector<Tracker *> p_trackers) {
	ERR_FAIL_NULL_V(openxr_api, nullptr);

	for (int i = 0; i < p_action_set->actions.size(); i++) {
		if (p_action_set->actions[i]->action_name == p_action_name) {
			// already exists in this set
			return nullptr;
		}
	}

	Vector<RID> tracker_rids;
	for (int i = 0; i < p_trackers.size(); i++) {
		tracker_rids.push_back(p_trackers[i]->tracker_rid);
	}

	Action *action = memnew(Action);
	if (p_action_type == OpenXRAction::OPENXR_ACTION_POSE) {
		// We can't have dual action names in OpenXR hence we added _pose,
		// but default, aim and grip and default pose action names in Godot so rename them on the tracker.
		// NOTE need to decide on whether we should keep the naming convention or rename it on Godots side
		if (p_action_name == "default_pose") {
			action->action_name = "default";
		} else if (p_action_name == "aim_pose") {
			action->action_name = "aim";
		} else if (p_action_name == "grip_pose") {
			action->action_name = "grip";
		} else {
			action->action_name = p_action_name;
		}
	} else {
		action->action_name = p_action_name;
	}

	action->action_type = p_action_type;
	action->action_rid = openxr_api->action_create(p_action_set->action_set_rid, p_action_name, p_localized_name, p_action_type, tracker_rids);
	p_action_set->actions.push_back(action);

	// we link our actions back to our trackers so we know which actions to check when we're processing our trackers
	for (int i = 0; i < p_trackers.size(); i++) {
		if (!p_trackers[i]->actions.has(action)) {
			p_trackers[i]->actions.push_back(action);
		}
	}

	return action;
}

OpenXRInterface::Action *OpenXRInterface::find_action(const String &p_action_name) {
	// We just find the first action by this name

	for (int i = 0; i < action_sets.size(); i++) {
		for (int j = 0; j < action_sets[i]->actions.size(); j++) {
			if (action_sets[i]->actions[j]->action_name == p_action_name) {
				return action_sets[i]->actions[j];
			}
		}
	}

	// not found
	return nullptr;
}

void OpenXRInterface::free_actions(ActionSet *p_action_set) {
	ERR_FAIL_NULL(openxr_api);

	for (int i = 0; i < p_action_set->actions.size(); i++) {
		Action *action = p_action_set->actions[i];

		openxr_api->action_free(action->action_rid);

		memdelete(action);
	}
	p_action_set->actions.clear();
}

OpenXRInterface::Tracker *OpenXRInterface::find_tracker(const String &p_tracker_name, bool p_create) {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, nullptr);
	ERR_FAIL_NULL_V(openxr_api, nullptr);

	Tracker *tracker = nullptr;
	for (int i = 0; i < trackers.size(); i++) {
		tracker = trackers[i];
		if (tracker->tracker_name == p_tracker_name) {
			return tracker;
		}
	}

	if (!p_create) {
		return nullptr;
	}

	ERR_FAIL_COND_V(!openxr_api->is_top_level_path_supported(p_tracker_name), nullptr);

	// Create our RID
	RID tracker_rid = openxr_api->tracker_create(p_tracker_name);
	ERR_FAIL_COND_V(tracker_rid.is_null(), nullptr);

	// Create our controller tracker.
	Ref<XRControllerTracker> controller_tracker;
	controller_tracker.instantiate();

	// We have standardized some names to make things nicer to the user so lets recognize the toplevel paths related to these.
	if (p_tracker_name == "/user/hand/left") {
		controller_tracker->set_tracker_name("left_hand");
		controller_tracker->set_tracker_desc("Left hand controller");
		controller_tracker->set_tracker_hand(XRPositionalTracker::TRACKER_HAND_LEFT);
	} else if (p_tracker_name == "/user/hand/right") {
		controller_tracker->set_tracker_name("right_hand");
		controller_tracker->set_tracker_desc("Right hand controller");
		controller_tracker->set_tracker_hand(XRPositionalTracker::TRACKER_HAND_RIGHT);
	} else {
		controller_tracker->set_tracker_name(p_tracker_name);
		controller_tracker->set_tracker_desc(p_tracker_name);
	}
	controller_tracker->set_tracker_profile(INTERACTION_PROFILE_NONE);
	xr_server->add_tracker(controller_tracker);

	// create a new entry
	tracker = memnew(Tracker);
	tracker->tracker_name = p_tracker_name;
	tracker->tracker_rid = tracker_rid;
	tracker->controller_tracker = controller_tracker;
	tracker->interaction_profile = RID();
	trackers.push_back(tracker);

	return tracker;
}

void OpenXRInterface::tracker_profile_changed(RID p_tracker, RID p_interaction_profile) {
	Tracker *tracker = nullptr;
	for (int i = 0; i < trackers.size() && tracker == nullptr; i++) {
		if (trackers[i]->tracker_rid == p_tracker) {
			tracker = trackers[i];
		}
	}
	ERR_FAIL_NULL(tracker);

	tracker->interaction_profile = p_interaction_profile;

	if (p_interaction_profile.is_null()) {
		print_verbose("OpenXR: Interaction profile for " + tracker->tracker_name + " changed to " + INTERACTION_PROFILE_NONE);
		tracker->controller_tracker->set_tracker_profile(INTERACTION_PROFILE_NONE);
	} else {
		String name = openxr_api->interaction_profile_get_name(p_interaction_profile);
		print_verbose("OpenXR: Interaction profile for " + tracker->tracker_name + " changed to " + name);
		tracker->controller_tracker->set_tracker_profile(name);
	}
}

void OpenXRInterface::handle_tracker(Tracker *p_tracker) {
	ERR_FAIL_NULL(openxr_api);
	ERR_FAIL_COND(p_tracker->controller_tracker.is_null());

	// Note, which actions are actually bound to inputs are handled by our interaction profiles however interaction
	// profiles are suggested bindings for controller types we know about. OpenXR runtimes can stray away from these
	// and rebind them or even offer bindings to controllers that are not known to us.

	// We don't really have a consistent way to detect whether a controller is active however as long as it is
	// unbound it seems to be unavailable, so far unknown controller seem to mimic one of the profiles we've
	// supplied.
	if (p_tracker->interaction_profile.is_null()) {
		return;
	}

	// We check all actions that are related to our tracker.
	for (int i = 0; i < p_tracker->actions.size(); i++) {
		Action *action = p_tracker->actions[i];
		switch (action->action_type) {
			case OpenXRAction::OPENXR_ACTION_BOOL: {
				bool pressed = openxr_api->get_action_bool(action->action_rid, p_tracker->tracker_rid);
				p_tracker->controller_tracker->set_input(action->action_name, Variant(pressed));
			} break;
			case OpenXRAction::OPENXR_ACTION_FLOAT: {
				real_t value = openxr_api->get_action_float(action->action_rid, p_tracker->tracker_rid);
				p_tracker->controller_tracker->set_input(action->action_name, Variant(value));
			} break;
			case OpenXRAction::OPENXR_ACTION_VECTOR2: {
				Vector2 value = openxr_api->get_action_vector2(action->action_rid, p_tracker->tracker_rid);
				p_tracker->controller_tracker->set_input(action->action_name, Variant(value));
			} break;
			case OpenXRAction::OPENXR_ACTION_POSE: {
				Transform3D transform;
				Vector3 linear, angular;

				XRPose::TrackingConfidence confidence = openxr_api->get_action_pose(action->action_rid, p_tracker->tracker_rid, transform, linear, angular);

				if (confidence != XRPose::XR_TRACKING_CONFIDENCE_NONE) {
					p_tracker->controller_tracker->set_pose(action->action_name, transform, linear, angular, confidence);
				} else {
					p_tracker->controller_tracker->invalidate_pose(action->action_name);
				}
			} break;
			default: {
				// not yet supported
			} break;
		}
	}
}

void OpenXRInterface::trigger_haptic_pulse(const String &p_action_name, const StringName &p_tracker_name, double p_frequency, double p_amplitude, double p_duration_sec, double p_delay_sec) {
	ERR_FAIL_NULL(openxr_api);

	Action *action = find_action(p_action_name);
	ERR_FAIL_NULL(action);

	// We need to map our tracker name to our OpenXR name for our inbuild names.
	String tracker_name = p_tracker_name;
	if (tracker_name == "left_hand") {
		tracker_name = "/user/hand/left";
	} else if (tracker_name == "right_hand") {
		tracker_name = "/user/hand/right";
	}
	Tracker *tracker = find_tracker(tracker_name);
	ERR_FAIL_NULL(tracker);

	// TODO OpenXR does not support delay, so we may need to add support for that somehow...

	XrDuration duration = XrDuration(p_duration_sec * 1000000000.0); // seconds -> nanoseconds

	openxr_api->trigger_haptic_pulse(action->action_rid, tracker->tracker_rid, p_frequency, p_amplitude, duration);
}

void OpenXRInterface::free_trackers() {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL(xr_server);
	ERR_FAIL_NULL(openxr_api);

	for (int i = 0; i < trackers.size(); i++) {
		Tracker *tracker = trackers[i];

		openxr_api->tracker_free(tracker->tracker_rid);
		xr_server->remove_tracker(tracker->controller_tracker);
		tracker->controller_tracker.unref();

		memdelete(tracker);
	}
	trackers.clear();
}

void OpenXRInterface::free_interaction_profiles() {
	ERR_FAIL_NULL(openxr_api);

	for (const RID &interaction_profile : interaction_profiles) {
		openxr_api->interaction_profile_free(interaction_profile);
	}
	interaction_profiles.clear();
}

bool OpenXRInterface::initialize_on_startup() const {
	if (openxr_api == nullptr) {
		return false;
	} else if (!openxr_api->is_initialized()) {
		return false;
	} else {
		return true;
	}
}

bool OpenXRInterface::is_initialized() const {
	return initialized;
}

bool OpenXRInterface::initialize() {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, false);

	if (openxr_api == nullptr) {
		return false;
	} else if (!openxr_api->is_initialized()) {
		return false;
	} else if (initialized) {
		return true;
	}

	// load up our action sets before setting up our session, note that our profiles are suggestions, OpenXR takes ownership of (re)binding
	_load_action_map();

	if (!openxr_api->initialize_session()) {
		return false;
	}

	// we must create a tracker for our head
	head.instantiate();
	head->set_tracker_type(XRServer::TRACKER_HEAD);
	head->set_tracker_name("head");
	head->set_tracker_desc("Players head");
	xr_server->add_tracker(head);

	// attach action sets
	Vector<RID> loaded_action_sets;
	for (int i = 0; i < action_sets.size(); i++) {
		loaded_action_sets.append(action_sets[i]->action_set_rid);
	}
	openxr_api->attach_action_sets(loaded_action_sets);

	// make this our primary interface
	xr_server->set_primary_interface(this);

	// Register an additional output with the display server, so rendering won't
	// be skipped if no windows are visible.
	DisplayServer::get_singleton()->register_additional_output(this);

	initialized = true;

	return initialized;
}

void OpenXRInterface::uninitialize() {
	// Our OpenXR driver will clean itself up properly when Godot exits, so we just do some basic stuff here

	// end the session if we need to?

	// cleanup stuff
	free_trackers();
	free_interaction_profiles();
	free_action_sets();

	XRServer *xr_server = XRServer::get_singleton();
	if (xr_server) {
		if (head.is_valid()) {
			xr_server->remove_tracker(head);
			head.unref();
		}
	}

	DisplayServer::get_singleton()->unregister_additional_output(this);

	initialized = false;
}

Dictionary OpenXRInterface::get_system_info() {
	Dictionary dict;

	if (openxr_api) {
		dict[SNAME("XRRuntimeName")] = openxr_api->get_runtime_name();
		dict[SNAME("XRRuntimeVersion")] = openxr_api->get_runtime_version();
	}

	return dict;
}

bool OpenXRInterface::supports_play_area_mode(XRInterface::PlayAreaMode p_mode) {
	if (p_mode == XRInterface::XR_PLAY_AREA_3DOF) {
		return false;
	}
	return true;
}

XRInterface::PlayAreaMode OpenXRInterface::get_play_area_mode() const {
	if (!openxr_api || !initialized) {
		return XRInterface::XR_PLAY_AREA_UNKNOWN;
	}

	XrReferenceSpaceType reference_space = openxr_api->get_reference_space();

	if (reference_space == XR_REFERENCE_SPACE_TYPE_LOCAL) {
		return XRInterface::XR_PLAY_AREA_SITTING;
	} else if (reference_space == XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT) {
		return XRInterface::XR_PLAY_AREA_ROOMSCALE;
	} else if (reference_space == XR_REFERENCE_SPACE_TYPE_STAGE) {
		return XRInterface::XR_PLAY_AREA_STAGE;
	}

	return XRInterface::XR_PLAY_AREA_UNKNOWN;
}

bool OpenXRInterface::set_play_area_mode(XRInterface::PlayAreaMode p_mode) {
	ERR_FAIL_NULL_V(openxr_api, false);

	XrReferenceSpaceType reference_space;

	if (p_mode == XRInterface::XR_PLAY_AREA_SITTING) {
		reference_space = XR_REFERENCE_SPACE_TYPE_LOCAL;
	} else if (p_mode == XRInterface::XR_PLAY_AREA_ROOMSCALE) {
		reference_space = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
	} else if (p_mode == XRInterface::XR_PLAY_AREA_STAGE) {
		reference_space = XR_REFERENCE_SPACE_TYPE_STAGE;
	} else {
		return false;
	}

	if (openxr_api->set_requested_reference_space(reference_space)) {
		XRServer *xr_server = XRServer::get_singleton();
		if (xr_server) {
			xr_server->clear_reference_frame();
		}
		return true;
	}

	return false;
}

PackedVector3Array OpenXRInterface::get_play_area() const {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, PackedVector3Array());
	PackedVector3Array arr;

	Vector3 sides[4] = {
		Vector3(-0.5f, 0.0f, -0.5f),
		Vector3(0.5f, 0.0f, -0.5f),
		Vector3(0.5f, 0.0f, 0.5f),
		Vector3(-0.5f, 0.0f, 0.5f),
	};

	if (openxr_api != nullptr && openxr_api->is_initialized()) {
		Size2 extents = openxr_api->get_play_space_bounds();
		if (extents.width != 0.0 && extents.height != 0.0) {
			Transform3D reference_frame = xr_server->get_reference_frame();

			for (int i = 0; i < 4; i++) {
				Vector3 coord = sides[i];

				// Scale it up.
				coord.x *= extents.width;
				coord.z *= extents.height;

				// Now apply our reference.
				Vector3 out = reference_frame.xform(coord);
				arr.push_back(out);
			}
		} else {
			WARN_PRINT_ONCE("OpenXR: No extents available.");
		}
	}

	return arr;
}

float OpenXRInterface::get_display_refresh_rate() const {
	if (openxr_api == nullptr) {
		return 0.0;
	} else if (!openxr_api->is_initialized()) {
		return 0.0;
	} else {
		return openxr_api->get_display_refresh_rate();
	}
}

void OpenXRInterface::set_display_refresh_rate(float p_refresh_rate) {
	if (openxr_api == nullptr) {
		return;
	} else if (!openxr_api->is_initialized()) {
		return;
	} else {
		openxr_api->set_display_refresh_rate(p_refresh_rate);
	}
}

Array OpenXRInterface::get_available_display_refresh_rates() const {
	if (openxr_api == nullptr) {
		return Array();
	} else if (!openxr_api->is_initialized()) {
		return Array();
	} else {
		return openxr_api->get_available_display_refresh_rates();
	}
}

bool OpenXRInterface::is_hand_tracking_supported() {
	if (openxr_api == nullptr) {
		return false;
	} else if (!openxr_api->is_initialized()) {
		return false;
	} else {
		OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
		if (hand_tracking_ext == nullptr) {
			return false;
		} else {
			return hand_tracking_ext->get_active();
		}
	}
}

bool OpenXRInterface::is_hand_interaction_supported() const {
	if (openxr_api == nullptr) {
		return false;
	} else if (!openxr_api->is_initialized()) {
		return false;
	} else {
		OpenXRHandInteractionExtension *hand_interaction_ext = OpenXRHandInteractionExtension::get_singleton();
		if (hand_interaction_ext == nullptr) {
			return false;
		} else {
			return hand_interaction_ext->is_available();
		}
	}
}

bool OpenXRInterface::is_eye_gaze_interaction_supported() {
	if (openxr_api == nullptr) {
		return false;
	} else if (!openxr_api->is_initialized()) {
		return false;
	} else {
		OpenXREyeGazeInteractionExtension *eye_gaze_ext = OpenXREyeGazeInteractionExtension::get_singleton();
		if (eye_gaze_ext == nullptr) {
			return false;
		} else {
			return eye_gaze_ext->supports_eye_gaze_interaction();
		}
	}
}

bool OpenXRInterface::is_action_set_active(const String &p_action_set) const {
	for (ActionSet *action_set : action_sets) {
		if (action_set->action_set_name == p_action_set) {
			return action_set->is_active;
		}
	}

	WARN_PRINT("OpenXR: Unknown action set " + p_action_set);
	return false;
}

void OpenXRInterface::set_action_set_active(const String &p_action_set, bool p_active) {
	for (ActionSet *action_set : action_sets) {
		if (action_set->action_set_name == p_action_set) {
			action_set->is_active = p_active;
			return;
		}
	}

	WARN_PRINT("OpenXR: Unknown action set " + p_action_set);
}

Array OpenXRInterface::get_action_sets() const {
	Array arr;

	for (ActionSet *action_set : action_sets) {
		arr.push_back(action_set->action_set_name);
	}

	return arr;
}

float OpenXRInterface::get_vrs_min_radius() const {
	return xr_vrs.get_vrs_min_radius();
}

void OpenXRInterface::set_vrs_min_radius(float p_vrs_min_radius) {
	xr_vrs.set_vrs_min_radius(p_vrs_min_radius);
}

float OpenXRInterface::get_vrs_strength() const {
	return xr_vrs.get_vrs_strength();
}

void OpenXRInterface::set_vrs_strength(float p_vrs_strength) {
	xr_vrs.set_vrs_strength(p_vrs_strength);
}

double OpenXRInterface::get_render_target_size_multiplier() const {
	if (openxr_api == nullptr) {
		return 1.0;
	} else {
		return openxr_api->get_render_target_size_multiplier();
	}
}

void OpenXRInterface::set_render_target_size_multiplier(double multiplier) {
	if (openxr_api == nullptr) {
		return;
	} else {
		openxr_api->set_render_target_size_multiplier(multiplier);
	}
}

bool OpenXRInterface::is_foveation_supported() const {
	if (openxr_api == nullptr) {
		return false;
	} else {
		return openxr_api->is_foveation_supported();
	}
}

int OpenXRInterface::get_foveation_level() const {
	if (openxr_api == nullptr) {
		return 0;
	} else {
		return openxr_api->get_foveation_level();
	}
}

void OpenXRInterface::set_foveation_level(int p_foveation_level) {
	if (openxr_api == nullptr) {
		return;
	} else {
		openxr_api->set_foveation_level(p_foveation_level);
	}
}

bool OpenXRInterface::get_foveation_dynamic() const {
	if (openxr_api == nullptr) {
		return false;
	} else {
		return openxr_api->get_foveation_dynamic();
	}
}

void OpenXRInterface::set_foveation_dynamic(bool p_foveation_dynamic) {
	if (openxr_api == nullptr) {
		return;
	} else {
		openxr_api->set_foveation_dynamic(p_foveation_dynamic);
	}
}

Size2 OpenXRInterface::get_render_target_size() {
	if (openxr_api == nullptr) {
		return Size2();
	} else {
		return openxr_api->get_recommended_target_size();
	}
}

uint32_t OpenXRInterface::get_view_count() {
	// TODO set this based on our configuration
	return 2;
}

void OpenXRInterface::_set_default_pos(Transform3D &p_transform, double p_world_scale, uint64_t p_eye) {
	p_transform = Transform3D();

	// if we're not tracking, don't put our head on the floor...
	p_transform.origin.y = 1.5 * p_world_scale;

	// overkill but..
	if (p_eye == 1) {
		p_transform.origin.x = 0.03 * p_world_scale;
	} else if (p_eye == 2) {
		p_transform.origin.x = -0.03 * p_world_scale;
	}
}

Transform3D OpenXRInterface::get_camera_transform() {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, Transform3D());

	Transform3D hmd_transform;
	double world_scale = xr_server->get_world_scale();

	// head_transform should be updated in process

	hmd_transform.basis = head_transform.basis;
	hmd_transform.origin = head_transform.origin * world_scale;

	return hmd_transform;
}

Transform3D OpenXRInterface::get_transform_for_view(uint32_t p_view, const Transform3D &p_cam_transform) {
	XRServer *xr_server = XRServer::get_singleton();
	ERR_FAIL_NULL_V(xr_server, Transform3D());
	ERR_FAIL_UNSIGNED_INDEX_V_MSG(p_view, get_view_count(), Transform3D(), "View index outside bounds.");

	Transform3D t;
	if (openxr_api && openxr_api->get_view_transform(p_view, t)) {
		// update our cached value if we have a valid transform
		transform_for_view[p_view] = t;
	} else {
		// reuse cached value
		t = transform_for_view[p_view];
	}

	// Apply our world scale
	double world_scale = xr_server->get_world_scale();
	t.origin *= world_scale;

	return p_cam_transform * xr_server->get_reference_frame() * t;
}

Projection OpenXRInterface::get_projection_for_view(uint32_t p_view, double p_aspect, double p_z_near, double p_z_far) {
	Projection cm;
	ERR_FAIL_UNSIGNED_INDEX_V_MSG(p_view, get_view_count(), cm, "View index outside bounds.");

	if (openxr_api) {
		if (openxr_api->get_view_projection(p_view, p_z_near, p_z_far, cm)) {
			return cm;
		}
	}

	// Failed to get from our OpenXR device? Default to some sort of sensible camera matrix..
	cm.set_for_hmd(p_view + 1, 1.0, 6.0, 14.5, 4.0, 1.5, p_z_near, p_z_far);

	return cm;
}

RID OpenXRInterface::get_color_texture() {
	if (openxr_api) {
		return openxr_api->get_color_texture();
	} else {
		return RID();
	}
}

RID OpenXRInterface::get_depth_texture() {
	if (openxr_api) {
		return openxr_api->get_depth_texture();
	} else {
		return RID();
	}
}

RID OpenXRInterface::get_velocity_texture() {
	if (openxr_api) {
		return openxr_api->get_velocity_texture();
	} else {
		return RID();
	}
}

RID OpenXRInterface::get_velocity_depth_texture() {
	if (openxr_api) {
		return openxr_api->get_velocity_depth_texture();
	} else {
		return RID();
	}
}

Size2i OpenXRInterface::get_velocity_target_size() {
	if (openxr_api) {
		return openxr_api->get_velocity_target_size();
	} else {
		return Size2i();
	}
}

void OpenXRInterface::handle_hand_tracking(const String &p_path, OpenXRHandTrackingExtension::HandTrackedHands p_hand) {
	OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
	if (hand_tracking_ext && hand_tracking_ext->get_active()) {
		OpenXRInterface::Tracker *tracker = find_tracker(p_path);
		if (tracker && tracker->controller_tracker.is_valid()) {
			XrSpaceLocationFlags location_flags = hand_tracking_ext->get_hand_joint_location_flags(p_hand, XR_HAND_JOINT_PALM_EXT);

			if (location_flags & (XR_SPACE_LOCATION_ORIENTATION_VALID_BIT + XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
				static const XrSpaceLocationFlags all_location_flags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT + XR_SPACE_LOCATION_POSITION_VALID_BIT + XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT + XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
				XRPose::TrackingConfidence confidence = XRPose::XR_TRACKING_CONFIDENCE_LOW;
				Transform3D transform;
				Vector3 linear_velocity;
				Vector3 angular_velocity;

				if ((location_flags & all_location_flags) == all_location_flags) {
					// All flags set? confidence is high!
					confidence = XRPose::XR_TRACKING_CONFIDENCE_HIGH;
				}

				if (location_flags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
					transform.basis = Basis(hand_tracking_ext->get_hand_joint_rotation(p_hand, XR_HAND_JOINT_PALM_EXT));
				}
				if (location_flags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
					transform.origin = hand_tracking_ext->get_hand_joint_position(p_hand, XR_HAND_JOINT_PALM_EXT);
				}

				XrSpaceVelocityFlags velocity_flags = hand_tracking_ext->get_hand_joint_location_flags(p_hand, XR_HAND_JOINT_PALM_EXT);
				if (velocity_flags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
					linear_velocity = hand_tracking_ext->get_hand_joint_linear_velocity(p_hand, XR_HAND_JOINT_PALM_EXT);
				}
				if (velocity_flags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
					angular_velocity = hand_tracking_ext->get_hand_joint_angular_velocity(p_hand, XR_HAND_JOINT_PALM_EXT);
				}

				tracker->controller_tracker->set_pose("skeleton", transform, linear_velocity, angular_velocity, confidence);
			} else {
				tracker->controller_tracker->invalidate_pose("skeleton");
			}
		}
	}
}

void OpenXRInterface::process() {
	if (openxr_api) {
		// do our normal process
		if (openxr_api->process()) {
			Transform3D t;
			Vector3 linear_velocity;
			Vector3 angular_velocity;
			head_confidence = openxr_api->get_head_center(t, linear_velocity, angular_velocity);
			if (head_confidence != XRPose::XR_TRACKING_CONFIDENCE_NONE) {
				// Only update our transform if we have one to update it with
				// note that poses are stored without world scale and reference frame applied!
				head_transform = t;
				head_linear_velocity = linear_velocity;
				head_angular_velocity = angular_velocity;
			}
		}

		// handle our action sets....
		Vector<RID> active_sets;
		for (int i = 0; i < action_sets.size(); i++) {
			if (action_sets[i]->is_active) {
				active_sets.push_back(action_sets[i]->action_set_rid);
			}
		}

		if (openxr_api->sync_action_sets(active_sets)) {
			for (int i = 0; i < trackers.size(); i++) {
				handle_tracker(trackers[i]);
			}
		}

		// Handle hand tracking
		handle_hand_tracking("/user/hand/left", OpenXRHandTrackingExtension::OPENXR_TRACKED_LEFT_HAND);
		handle_hand_tracking("/user/hand/right", OpenXRHandTrackingExtension::OPENXR_TRACKED_RIGHT_HAND);
	}

	if (head.is_valid()) {
		head->set_pose("default", head_transform, head_linear_velocity, head_angular_velocity, head_confidence);
	}

	if (reference_stage_changing) {
		// Now that we have updated tracking information in our updated reference space, trigger our pose recentered signal.
		emit_signal(SNAME("pose_recentered"));
		reference_stage_changing = false;
	}
}

void OpenXRInterface::pre_render() {
	if (openxr_api) {
		openxr_api->pre_render();
	}
}

bool OpenXRInterface::pre_draw_viewport(RID p_render_target) {
	if (openxr_api) {
		return openxr_api->pre_draw_viewport(p_render_target);
	} else {
		// don't render
		return false;
	}
}

Vector<BlitToScreen> OpenXRInterface::post_draw_viewport(RID p_render_target, const Rect2 &p_screen_rect) {
	Vector<BlitToScreen> blit_to_screen;

#ifndef ANDROID_ENABLED
	// If separate HMD we should output one eye to screen
	if (p_screen_rect != Rect2()) {
		BlitToScreen blit;

		blit.render_target = p_render_target;
		blit.multi_view.use_layer = true;
		blit.multi_view.layer = 0;
		blit.lens_distortion.apply = false;

		Size2 render_size = get_render_target_size();
		Rect2 dst_rect = p_screen_rect;
		float new_height = dst_rect.size.x * (render_size.y / render_size.x);
		if (new_height > dst_rect.size.y) {
			dst_rect.position.y = (0.5 * dst_rect.size.y) - (0.5 * new_height);
			dst_rect.size.y = new_height;
		} else {
			float new_width = dst_rect.size.y * (render_size.x / render_size.y);

			dst_rect.position.x = (0.5 * dst_rect.size.x) - (0.5 * new_width);
			dst_rect.size.x = new_width;
		}

		blit.dst_rect = dst_rect;
		blit_to_screen.push_back(blit);
	}
#endif

	if (openxr_api) {
		openxr_api->post_draw_viewport(p_render_target);
	}

	return blit_to_screen;
}

void OpenXRInterface::end_frame() {
	if (openxr_api) {
		openxr_api->end_frame();
	}
}

bool OpenXRInterface::is_passthrough_supported() {
	return get_supported_environment_blend_modes().find(XR_ENV_BLEND_MODE_ALPHA_BLEND);
}

bool OpenXRInterface::is_passthrough_enabled() {
	return get_environment_blend_mode() == XR_ENV_BLEND_MODE_ALPHA_BLEND;
}

bool OpenXRInterface::start_passthrough() {
	return set_environment_blend_mode(XR_ENV_BLEND_MODE_ALPHA_BLEND);
}

void OpenXRInterface::stop_passthrough() {
	set_environment_blend_mode(XR_ENV_BLEND_MODE_OPAQUE);
}

Array OpenXRInterface::get_supported_environment_blend_modes() {
	Array modes;

	if (!openxr_api) {
		return modes;
	}

	uint32_t count = 0;
	const XrEnvironmentBlendMode *env_blend_modes = openxr_api->get_supported_environment_blend_modes(count);

	if (!env_blend_modes) {
		return modes;
	}

	for (uint32_t i = 0; i < count; i++) {
		switch (env_blend_modes[i]) {
			case XR_ENVIRONMENT_BLEND_MODE_OPAQUE:
				modes.push_back(XR_ENV_BLEND_MODE_OPAQUE);
				break;
			case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:
				modes.push_back(XR_ENV_BLEND_MODE_ADDITIVE);
				break;
			case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND:
				modes.push_back(XR_ENV_BLEND_MODE_ALPHA_BLEND);
				break;
			default:
				WARN_PRINT("Unsupported blend mode found: " + String::num_int64(int64_t(env_blend_modes[i])));
		}
	}

	if (openxr_api->is_environment_blend_mode_alpha_blend_supported() == OpenXRAPI::OPENXR_ALPHA_BLEND_MODE_SUPPORT_EMULATING) {
		modes.push_back(XR_ENV_BLEND_MODE_ALPHA_BLEND);
	}

	return modes;
}

XRInterface::EnvironmentBlendMode OpenXRInterface::get_environment_blend_mode() const {
	if (openxr_api) {
		XrEnvironmentBlendMode oxr_blend_mode = openxr_api->get_environment_blend_mode();
		switch (oxr_blend_mode) {
			case XR_ENVIRONMENT_BLEND_MODE_OPAQUE: {
				return XR_ENV_BLEND_MODE_OPAQUE;
			} break;
			case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE: {
				return XR_ENV_BLEND_MODE_ADDITIVE;
			} break;
			case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND: {
				return XR_ENV_BLEND_MODE_ALPHA_BLEND;
			} break;
			default:
				break;
		}
	}

	return XR_ENV_BLEND_MODE_OPAQUE;
}

bool OpenXRInterface::set_environment_blend_mode(XRInterface::EnvironmentBlendMode mode) {
	if (openxr_api) {
		XrEnvironmentBlendMode oxr_blend_mode;
		switch (mode) {
			case XR_ENV_BLEND_MODE_OPAQUE:
				oxr_blend_mode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
				break;
			case XR_ENV_BLEND_MODE_ADDITIVE:
				oxr_blend_mode = XR_ENVIRONMENT_BLEND_MODE_ADDITIVE;
				break;
			case XR_ENV_BLEND_MODE_ALPHA_BLEND:
				oxr_blend_mode = XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND;
				break;
			default:
				WARN_PRINT("Unknown blend mode requested: " + String::num_int64(int64_t(mode)));
				oxr_blend_mode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
		}
		return openxr_api->set_environment_blend_mode(oxr_blend_mode);
	}
	return false;
}

void OpenXRInterface::on_state_ready() {
	emit_signal(SNAME("session_begun"));
}

void OpenXRInterface::on_state_visible() {
	emit_signal(SNAME("session_visible"));
}

void OpenXRInterface::on_state_focused() {
	emit_signal(SNAME("session_focussed"));
}

void OpenXRInterface::on_state_stopping() {
	emit_signal(SNAME("session_stopping"));
}

void OpenXRInterface::on_state_loss_pending() {
	emit_signal(SNAME("session_loss_pending"));
}

void OpenXRInterface::on_state_exiting() {
	emit_signal(SNAME("instance_exiting"));
}

void OpenXRInterface::on_reference_space_change_pending() {
	reference_stage_changing = true;
}

void OpenXRInterface::on_refresh_rate_changes(float p_new_rate) {
	emit_signal(SNAME("refresh_rate_changed"), p_new_rate);
}

/** Hand tracking. */
void OpenXRInterface::set_motion_range(const Hand p_hand, const HandMotionRange p_motion_range) {
	ERR_FAIL_INDEX(p_hand, HAND_MAX);
	ERR_FAIL_INDEX(p_motion_range, HAND_MOTION_RANGE_MAX);

	OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
	if (hand_tracking_ext && hand_tracking_ext->get_active()) {
		XrHandJointsMotionRangeEXT xr_motion_range;
		switch (p_motion_range) {
			case HAND_MOTION_RANGE_UNOBSTRUCTED:
				xr_motion_range = XR_HAND_JOINTS_MOTION_RANGE_UNOBSTRUCTED_EXT;
				break;
			case HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER:
				xr_motion_range = XR_HAND_JOINTS_MOTION_RANGE_CONFORMING_TO_CONTROLLER_EXT;
				break;
			default:
				// Shouldn't get here, ERR_FAIL_INDEX should have caught this...
				xr_motion_range = XR_HAND_JOINTS_MOTION_RANGE_CONFORMING_TO_CONTROLLER_EXT;
				break;
		}

		hand_tracking_ext->set_motion_range(OpenXRHandTrackingExtension::HandTrackedHands(p_hand), xr_motion_range);
	}
}

OpenXRInterface::HandMotionRange OpenXRInterface::get_motion_range(const Hand p_hand) const {
	ERR_FAIL_INDEX_V(p_hand, HAND_MAX, HAND_MOTION_RANGE_MAX);

	OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
	if (hand_tracking_ext && hand_tracking_ext->get_active()) {
		XrHandJointsMotionRangeEXT xr_motion_range = hand_tracking_ext->get_motion_range(OpenXRHandTrackingExtension::HandTrackedHands(p_hand));

		switch (xr_motion_range) {
			case XR_HAND_JOINTS_MOTION_RANGE_UNOBSTRUCTED_EXT:
				return HAND_MOTION_RANGE_UNOBSTRUCTED;
			case XR_HAND_JOINTS_MOTION_RANGE_CONFORMING_TO_CONTROLLER_EXT:
				return HAND_MOTION_RANGE_CONFORM_TO_CONTROLLER;
			default:
				ERR_FAIL_V_MSG(HAND_MOTION_RANGE_MAX, "Unknown motion range returned by OpenXR");
		}
	}

	return HAND_MOTION_RANGE_MAX;
}

OpenXRInterface::HandTrackedSource OpenXRInterface::get_hand_tracking_source(const Hand p_hand) const {
	ERR_FAIL_INDEX_V(p_hand, HAND_MAX, HAND_TRACKED_SOURCE_UNKNOWN);

	OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
	if (hand_tracking_ext && hand_tracking_ext->get_active()) {
		OpenXRHandTrackingExtension::HandTrackedSource source = hand_tracking_ext->get_hand_tracking_source(OpenXRHandTrackingExtension::HandTrackedHands(p_hand));
		switch (source) {
			case OpenXRHandTrackingExtension::OPENXR_SOURCE_UNOBSTRUCTED:
				return HAND_TRACKED_SOURCE_UNOBSTRUCTED;
			case OpenXRHandTrackingExtension::OPENXR_SOURCE_CONTROLLER:
				return HAND_TRACKED_SOURCE_CONTROLLER;
			case OpenXRHandTrackingExtension::OPENXR_SOURCE_UNKNOWN:
				return HAND_TRACKED_SOURCE_UNKNOWN;
			default:
				ERR_FAIL_V_MSG(HAND_TRACKED_SOURCE_UNKNOWN, "Unknown hand tracking source returned by OpenXR");
		}
	}

	return HAND_TRACKED_SOURCE_UNKNOWN;
}

BitField<OpenXRInterface::HandJointFlags> OpenXRInterface::get_hand_joint_flags(Hand p_hand, HandJoints p_joint) const {
	BitField<OpenXRInterface::HandJointFlags> bits;

	OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
	if (hand_tracking_ext && hand_tracking_ext->get_active()) {
		XrSpaceLocationFlags location_flags = hand_tracking_ext->get_hand_joint_location_flags(OpenXRHandTrackingExtension::HandTrackedHands(p_hand), XrHandJointEXT(p_joint));
		if (location_flags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
			bits.set_flag(HAND_JOINT_ORIENTATION_VALID);
		}
		if (location_flags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) {
			bits.set_flag(HAND_JOINT_ORIENTATION_TRACKED);
		}
		if (location_flags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
			bits.set_flag(HAND_JOINT_POSITION_VALID);
		}
		if (location_flags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) {
			bits.set_flag(HAND_JOINT_POSITION_TRACKED);
		}

		XrSpaceVelocityFlags velocity_flags = hand_tracking_ext->get_hand_joint_velocity_flags(OpenXRHandTrackingExtension::HandTrackedHands(p_hand), XrHandJointEXT(p_joint));
		if (velocity_flags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT) {
			bits.set_flag(HAND_JOINT_LINEAR_VELOCITY_VALID);
		}
		if (velocity_flags & XR_SPACE_VELOCITY_ANGULAR_VALID_BIT) {
			bits.set_flag(HAND_JOINT_ANGULAR_VELOCITY_VALID);
		}
	}

	return bits;
}

Quaternion OpenXRInterface::get_hand_joint_rotation(Hand p_hand, HandJoints p_joint) const {
	OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
	if (hand_tracking_ext && hand_tracking_ext->get_active()) {
		return hand_tracking_ext->get_hand_joint_rotation(OpenXRHandTrackingExtension::HandTrackedHands(p_hand), XrHandJointEXT(p_joint));
	}

	return Quaternion();
}

Vector3 OpenXRInterface::get_hand_joint_position(Hand p_hand, HandJoints p_joint) const {
	OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
	if (hand_tracking_ext && hand_tracking_ext->get_active()) {
		return hand_tracking_ext->get_hand_joint_position(OpenXRHandTrackingExtension::HandTrackedHands(p_hand), XrHandJointEXT(p_joint));
	}

	return Vector3();
}

float OpenXRInterface::get_hand_joint_radius(Hand p_hand, HandJoints p_joint) const {
	OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
	if (hand_tracking_ext && hand_tracking_ext->get_active()) {
		return hand_tracking_ext->get_hand_joint_radius(OpenXRHandTrackingExtension::HandTrackedHands(p_hand), XrHandJointEXT(p_joint));
	}

	return 0.0;
}

Vector3 OpenXRInterface::get_hand_joint_linear_velocity(Hand p_hand, HandJoints p_joint) const {
	OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
	if (hand_tracking_ext && hand_tracking_ext->get_active()) {
		return hand_tracking_ext->get_hand_joint_linear_velocity(OpenXRHandTrackingExtension::HandTrackedHands(p_hand), XrHandJointEXT(p_joint));
	}

	return Vector3();
}

Vector3 OpenXRInterface::get_hand_joint_angular_velocity(Hand p_hand, HandJoints p_joint) const {
	OpenXRHandTrackingExtension *hand_tracking_ext = OpenXRHandTrackingExtension::get_singleton();
	if (hand_tracking_ext && hand_tracking_ext->get_active()) {
		return hand_tracking_ext->get_hand_joint_angular_velocity(OpenXRHandTrackingExtension::HandTrackedHands(p_hand), XrHandJointEXT(p_joint));
	}

	return Vector3();
}

RID OpenXRInterface::get_vrs_texture() {
	if (!openxr_api) {
		return RID();
	}

	PackedVector2Array eye_foci;

	Size2 target_size = get_render_target_size();
	real_t aspect_ratio = target_size.x / target_size.y;
	uint32_t view_count = get_view_count();

	for (uint32_t v = 0; v < view_count; v++) {
		eye_foci.push_back(openxr_api->get_eye_focus(v, aspect_ratio));
	}

	return xr_vrs.make_vrs_texture(target_size, eye_foci);
}

OpenXRInterface::OpenXRInterface() {
	openxr_api = OpenXRAPI::get_singleton();
	if (openxr_api) {
		openxr_api->set_xr_interface(this);
	}

	// while we don't have head tracking, don't put the headset on the floor...
	_set_default_pos(head_transform, 1.0, 0);
	_set_default_pos(transform_for_view[0], 1.0, 1);
	_set_default_pos(transform_for_view[1], 1.0, 2);
}

OpenXRInterface::~OpenXRInterface() {
	if (is_initialized()) {
		uninitialize();
	}

	if (openxr_api) {
		openxr_api->set_xr_interface(nullptr);
		openxr_api = nullptr;
	}
}
