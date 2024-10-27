/**************************************************************************/
/*  openxr_interaction_profile.cpp                                        */
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

#include "openxr_interaction_profile.h"

void OpenXRIPBinding::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_action", "action"), &OpenXRIPBinding::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &OpenXRIPBinding::get_action);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "action", PROPERTY_HINT_RESOURCE_TYPE, "OpenXRAction"), "set_action", "get_action");

	ClassDB::bind_method(D_METHOD("set_binding_path", "binding_path"), &OpenXRIPBinding::set_binding_path);
	ClassDB::bind_method(D_METHOD("get_binding_path"), &OpenXRIPBinding::get_binding_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "binding_path"), "set_binding_path", "get_binding_path");

	// Deprecated
#ifndef DISABLE_DEPRECATED
	ClassDB::bind_method(D_METHOD("set_paths", "paths"), &OpenXRIPBinding::set_paths);
	ClassDB::bind_method(D_METHOD("get_paths"), &OpenXRIPBinding::get_paths);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "paths", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_paths", "get_paths");

	ClassDB::bind_method(D_METHOD("get_path_count"), &OpenXRIPBinding::get_path_count);
	ClassDB::bind_method(D_METHOD("has_path", "path"), &OpenXRIPBinding::has_path);
	ClassDB::bind_method(D_METHOD("add_path", "path"), &OpenXRIPBinding::add_path);
	ClassDB::bind_method(D_METHOD("remove_path", "path"), &OpenXRIPBinding::remove_path);
#endif // DISABLE_DEPRECATED
}

Ref<OpenXRIPBinding> OpenXRIPBinding::new_binding(const Ref<OpenXRAction> p_action, const String &p_binding_path) {
	// This is a helper function to help build our default action sets

	Ref<OpenXRIPBinding> binding;
	binding.instantiate();
	binding->set_action(p_action);
	binding->set_binding_path(p_binding_path);

	return binding;
}

void OpenXRIPBinding::set_action(const Ref<OpenXRAction> p_action) {
	action = p_action;
	emit_changed();
}

Ref<OpenXRAction> OpenXRIPBinding::get_action() const {
	return action;
}

void OpenXRIPBinding::set_binding_path(const String &path) {
	binding_path = path;
	emit_changed();
}

String OpenXRIPBinding::get_binding_path() const {
	return binding_path;
}

#ifndef DISABLE_DEPRECATED

void OpenXRIPBinding::set_paths(const PackedStringArray p_paths) { // Deprecated, but needed for loading old action maps.
	// Fallback logic, this should ONLY be called when loading older action maps.
	// We'll parse this momentarily and extract individual bindings.
	binding_path = "";
	for (const String &path : p_paths) {
		if (!binding_path.is_empty()) {
			binding_path += ",";
		}
		binding_path += path;
	}
}

PackedStringArray OpenXRIPBinding::get_paths() const { // Deprecated, but needed for converting old action maps.
	// Fallback logic, return an array.
	// If we just loaded an old action map from disc, this will be a comma separated list of actions.
	// Once parsed there should be only one path in our array.
	PackedStringArray paths = binding_path.split(",", false);

	return paths;
}

int OpenXRIPBinding::get_path_count() const { // Deprecated.
	// Fallback logic, we only have one entry.
	return binding_path.is_empty() ? 0 : 1;
}

bool OpenXRIPBinding::has_path(const String p_path) const { // Deprecated.
	// Fallback logic, return true if this is our path.
	return binding_path == p_path;
}

void OpenXRIPBinding::add_path(const String p_path) { // Deprecated.
	// Fallback logic, only assign first time this is called.
	if (binding_path != p_path) {
		ERR_FAIL_COND_MSG(!binding_path.is_empty(), "Method add_path has been deprecated. A binding path was already set, create separate binding resources for each path and use set_binding_path instead.");

		binding_path = p_path;
		emit_changed();
	}
}

void OpenXRIPBinding::remove_path(const String p_path) { // Deprecated.
	ERR_FAIL_COND_MSG(binding_path != p_path, "Method remove_path has been deprecated. Attempt at removing a different binding path, remove the correct binding record from the interaction profile instead.");

	// Fallback logic, clear if this is our path.
	binding_path = p_path;
	emit_changed();
}

#endif // DISABLE_DEPRECATED

OpenXRIPBinding::~OpenXRIPBinding() {
	action.unref();
}

void OpenXRInteractionProfile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_interaction_profile_path", "interaction_profile_path"), &OpenXRInteractionProfile::set_interaction_profile_path);
	ClassDB::bind_method(D_METHOD("get_interaction_profile_path"), &OpenXRInteractionProfile::get_interaction_profile_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "interaction_profile_path"), "set_interaction_profile_path", "get_interaction_profile_path");

	ClassDB::bind_method(D_METHOD("get_binding_count"), &OpenXRInteractionProfile::get_binding_count);
	ClassDB::bind_method(D_METHOD("get_binding", "index"), &OpenXRInteractionProfile::get_binding);
	ClassDB::bind_method(D_METHOD("set_bindings", "bindings"), &OpenXRInteractionProfile::set_bindings);
	ClassDB::bind_method(D_METHOD("get_bindings"), &OpenXRInteractionProfile::get_bindings);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "bindings", PROPERTY_HINT_RESOURCE_TYPE, "OpenXRIPBinding", PROPERTY_USAGE_NO_EDITOR), "set_bindings", "get_bindings");
}

Ref<OpenXRInteractionProfile> OpenXRInteractionProfile::new_profile(const char *p_input_profile_path) {
	Ref<OpenXRInteractionProfile> profile;
	profile.instantiate();
	profile->set_interaction_profile_path(String(p_input_profile_path));

	return profile;
}

void OpenXRInteractionProfile::set_interaction_profile_path(const String p_input_profile_path) {
	OpenXRInteractionProfileMetadata *pmd = OpenXRInteractionProfileMetadata::get_singleton();
	if (pmd) {
		interaction_profile_path = pmd->check_profile_name(p_input_profile_path);
	} else {
		// OpenXR module not enabled, ignore checks.
		interaction_profile_path = p_input_profile_path;
	}
	emit_changed();
}

String OpenXRInteractionProfile::get_interaction_profile_path() const {
	return interaction_profile_path;
}

int OpenXRInteractionProfile::get_binding_count() const {
	return bindings.size();
}

Ref<OpenXRIPBinding> OpenXRInteractionProfile::get_binding(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, bindings.size(), Ref<OpenXRIPBinding>());

	return bindings[p_index];
}

void OpenXRInteractionProfile::set_bindings(Array p_bindings) {
	bindings.clear();

	for (Ref<OpenXRIPBinding> binding : p_bindings) {
		String binding_path = binding->get_binding_path();
		if (binding_path.find_char(',') >= 0) {
			// Convert old binding approach to new...
			add_new_binding(binding->get_action(), binding_path);
		} else {
			add_binding(binding);
		}
	}

	emit_changed();
}

Array OpenXRInteractionProfile::get_bindings() const {
	return bindings;
}

Ref<OpenXRIPBinding> OpenXRInteractionProfile::find_binding(const Ref<OpenXRAction> p_action, const String &p_binding_path) const {
	for (Ref<OpenXRIPBinding> binding : bindings) {
		if (binding->get_action() == p_action && binding->get_binding_path() == p_binding_path) {
			return binding;
		}
	}

	return Ref<OpenXRIPBinding>();
}

Vector<Ref<OpenXRIPBinding>> OpenXRInteractionProfile::get_bindings_for_action(const Ref<OpenXRAction> p_action) const {
	Vector<Ref<OpenXRIPBinding>> ret_bindings;

	for (Ref<OpenXRIPBinding> binding : bindings) {
		if (binding->get_action() == p_action) {
			ret_bindings.push_back(binding);
		}
	}

	return ret_bindings;
}

void OpenXRInteractionProfile::add_binding(Ref<OpenXRIPBinding> p_binding) {
	ERR_FAIL_COND(p_binding.is_null());

	if (!bindings.has(p_binding)) {
		ERR_FAIL_COND_MSG(find_binding(p_binding->get_action(), p_binding->get_binding_path()).is_valid(), "There is already a binding for this action and binding path in this interaction profile.");

		bindings.push_back(p_binding);
		emit_changed();
	}
}

void OpenXRInteractionProfile::remove_binding(Ref<OpenXRIPBinding> p_binding) {
	int idx = bindings.find(p_binding);
	if (idx != -1) {
		bindings.remove_at(idx);
		emit_changed();
	}
}

void OpenXRInteractionProfile::add_new_binding(const Ref<OpenXRAction> p_action, const String &p_paths) {
	// This is a helper function to help build our default action sets

	PackedStringArray paths = p_paths.split(",", false);

	for (const String &path : paths) {
		Ref<OpenXRIPBinding> binding = OpenXRIPBinding::new_binding(p_action, path);
		add_binding(binding);
	}
}

void OpenXRInteractionProfile::remove_binding_for_action(const Ref<OpenXRAction> p_action) {
	for (int i = bindings.size() - 1; i >= 0; i--) {
		Ref<OpenXRIPBinding> binding = bindings[i];
		if (binding->get_action() == p_action) {
			remove_binding(binding);
		}
	}
}

bool OpenXRInteractionProfile::has_binding_for_action(const Ref<OpenXRAction> p_action) {
	for (int i = bindings.size() - 1; i >= 0; i--) {
		Ref<OpenXRIPBinding> binding = bindings[i];
		if (binding->get_action() == p_action) {
			return true;
		}
	}

	return false;
}

OpenXRInteractionProfile::~OpenXRInteractionProfile() {
	bindings.clear();
}
