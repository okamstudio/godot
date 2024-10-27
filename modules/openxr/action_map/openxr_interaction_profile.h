/**************************************************************************/
/*  openxr_interaction_profile.h                                          */
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

#ifndef OPENXR_INTERACTION_PROFILE_H
#define OPENXR_INTERACTION_PROFILE_H

#include "openxr_action.h"
#include "openxr_interaction_profile_metadata.h"

#include "core/io/resource.h"

class OpenXRIPBinding : public Resource {
	GDCLASS(OpenXRIPBinding, Resource);

private:
	Ref<OpenXRAction> action;
	String binding_path;

protected:
	static void _bind_methods();

public:
	static Ref<OpenXRIPBinding> new_binding(const Ref<OpenXRAction> p_action, const String &p_binding_path); // Helper function for adding a new binding.

	void set_action(const Ref<OpenXRAction> p_action); // Set the action for this binding.
	Ref<OpenXRAction> get_action() const; // Get the action for this binding.

	void set_binding_path(const String &path);
	String get_binding_path() const;

	// Deprecated.
#ifndef DISABLE_DEPRECATED
	void set_paths(const PackedStringArray p_paths); // Set our paths (for loading from resource), needed for loading old action maps.
	PackedStringArray get_paths() const; // Get our paths (for saving to resource), needed for converted old action maps.
	int get_path_count() const; // Get the number of io paths.
	bool has_path(const String p_path) const; // Has this io path.
	void add_path(const String p_path); // Add an io path.
	void remove_path(const String p_path); // Remove an io path.
#endif // DISABLE_DEPRECATED

	// TODO add validation that we can display in the interface that checks if no two paths belong to the same top level path

	~OpenXRIPBinding();
};

class OpenXRInteractionProfile : public Resource {
	GDCLASS(OpenXRInteractionProfile, Resource);

private:
	String interaction_profile_path;
	Array bindings;

protected:
	static void _bind_methods();

public:
	static Ref<OpenXRInteractionProfile> new_profile(const char *p_input_profile_path); // Helper function to create a new interaction profile

	void set_interaction_profile_path(const String p_input_profile_path); // Set our input profile path
	String get_interaction_profile_path() const; // get our input profile path

	int get_binding_count() const; // Retrieve the number of bindings in this profile path
	Ref<OpenXRIPBinding> get_binding(int p_index) const;
	void set_bindings(Array p_bindings); // Set the bindings (for loading from a resource)
	Array get_bindings() const; // Get the bindings (for saving to a resource)

	Ref<OpenXRIPBinding> find_binding(const Ref<OpenXRAction> p_action, const String &p_binding_path) const; // Get our binding record
	Vector<Ref<OpenXRIPBinding>> get_bindings_for_action(const Ref<OpenXRAction> p_action) const; // Get our binding record for a given action
	void add_binding(Ref<OpenXRIPBinding> p_binding); // Add a binding object
	void remove_binding(Ref<OpenXRIPBinding> p_binding); // Remove a binding object

	void add_new_binding(const Ref<OpenXRAction> p_action, const String &p_paths); // Create a new binding for this profile
	void remove_binding_for_action(const Ref<OpenXRAction> p_action); // Remove all bindings for this action
	bool has_binding_for_action(const Ref<OpenXRAction> p_action); // Returns true if we have a binding for this action

	~OpenXRInteractionProfile();
};

#endif // OPENXR_INTERACTION_PROFILE_H
