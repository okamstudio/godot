/**************************************************************************/
/*  openxr_action.h                                                       */
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

#pragma once

#include "core/io/resource.h"

class OpenXRActionSet;

class OpenXRAction : public Resource {
	GDCLASS(OpenXRAction, Resource);

public:
	enum ActionType {
		OPENXR_ACTION_BOOL,
		OPENXR_ACTION_FLOAT,
		OPENXR_ACTION_VECTOR2,
		OPENXR_ACTION_POSE,
		OPENXR_ACTION_HAPTIC,
		OPENXR_ACTION_MAX
	};

private:
	String localized_name;
	ActionType action_type = OPENXR_ACTION_FLOAT;

	PackedStringArray toplevel_paths;

protected:
	friend class OpenXRActionSet;

	OpenXRActionSet *action_set = nullptr; // action belongs to this action set.

	static void _bind_methods();

public:
	static Ref<OpenXRAction> new_action(const char *p_name, const char *p_localized_name, const ActionType p_action_type, const char *p_toplevel_paths); // Helper function to add and configure an action
	OpenXRActionSet *get_action_set() const { return action_set; } // Get the action set this action belongs to

	String get_name_with_set() const; // Retrieve the name of this action as <action_set>/<action>

	void set_localized_name(const String p_localized_name); // Set the localized name of this action
	String get_localized_name() const; // Get the localized name of this action

	void set_action_type(const ActionType p_action_type); // Set the type of this action
	ActionType get_action_type() const; // Get the type of this action

	void set_toplevel_paths(const PackedStringArray p_toplevel_paths); // Set the toplevel paths of this action
	PackedStringArray get_toplevel_paths() const; // Get the toplevel paths of this action

	void add_toplevel_path(const String p_toplevel_path); // Add a top level path to this action
	void rem_toplevel_path(const String p_toplevel_path); // Remove a toplevel path from this action

	void parse_toplevel_paths(const String p_toplevel_paths); // Parse and set the top level paths from a comma separated string
};

VARIANT_ENUM_CAST(OpenXRAction::ActionType);
