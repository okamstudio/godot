/**************************************************************************/
/*  openxr_action_map.h                                                   */
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

#ifndef OPENXR_ACTION_MAP_H
#define OPENXR_ACTION_MAP_H

#include "openxr_action.h"
#include "openxr_action_set.h"
#include "openxr_interaction_profile.h"

#include "core/io/resource.h"

class OpenXRActionMap : public Resource {
	GDCLASS(OpenXRActionMap, Resource);

private:
	Array action_sets;
	Array interaction_profiles;

protected:
	static void _bind_methods();

public:
	void set_action_sets(Array p_action_sets); // Set our actions sets by providing an array with action sets (for loading from resource)
	Array get_action_sets() const; // Get our action sets as an array (for saving to resource)

	int get_action_set_count() const; // Retrieve the number of action sets we have
	Ref<OpenXRActionSet> find_action_set(String p_name) const; // Find an action set by name
	Ref<OpenXRActionSet> get_action_set(int p_idx) const; // Retrieve an action set by index
	void add_action_set(Ref<OpenXRActionSet> p_action_set); // Add an action set to our action map
	void remove_action_set(Ref<OpenXRActionSet> p_action_set); // Remove an action set from our action map

	void clear_interaction_profiles(); // Remove all our interaction profiles
	void set_interaction_profiles(Array p_interaction_profiles); // Set our interaction profiles by providing an array (for loading from resource)
	Array get_interaction_profiles() const; // Get our interaction profiles as an array (for saving to resource)

	int get_interaction_profile_count() const; // Retrieve the number of interaction profiles we have
	Ref<OpenXRInteractionProfile> find_interaction_profile(String p_path) const; // Find an interaction profile by path
	Ref<OpenXRInteractionProfile> get_interaction_profile(int p_idx) const; // Retrieve an interaction profile by index
	void add_interaction_profile(Ref<OpenXRInteractionProfile> p_interaction_profile); // Add an interaction profile to our action map
	void remove_interaction_profile(Ref<OpenXRInteractionProfile> p_interaction_profile); // remove an interaction profile from our action map

	void create_default_action_sets(); // Create our default action set for runtime
	void create_editor_action_sets(); // Create our action set for the editor

	// Helper functions for editor
	Ref<OpenXRAction> get_action(const String p_path) const; // Retrieve an action using <action name>/<action> as our parameter
	void remove_action(const String p_path, bool p_remove_interaction_profiles = false); // Remove action from action set, also removes it from interaction profiles
	PackedStringArray get_top_level_paths(const Ref<OpenXRAction> p_action); // Determines the top level paths based on where an action is bound in interaction profiles

	// TODO add validation to display in the interface that checks if we have action sets with the same name or if we have interaction profiles for the same path

	~OpenXRActionMap();
};

#endif // OPENXR_ACTION_MAP_H
