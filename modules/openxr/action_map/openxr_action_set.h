/**************************************************************************/
/*  openxr_action_set.h                                                   */
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

#include "openxr_action.h"

#include "core/io/resource.h"

class OpenXRActionSet : public Resource {
	GDCLASS(OpenXRActionSet, Resource);

private:
	String localized_name;
	int priority = 0;

	Array actions;
	void clear_actions();

protected:
	static void _bind_methods();

public:
	static Ref<OpenXRActionSet> new_action_set(const char *p_name, const char *p_localized_name, const int p_priority = 0); // Helper function for adding and setting up an action set

	void set_localized_name(const String p_localized_name); // Set the localized name of this action set
	String get_localized_name() const; // Get the localized name of this action set

	void set_priority(const int p_priority); // Set the priority of this action set
	int get_priority() const; // Get the priority of this action set

	int get_action_count() const; // Retrieve the number of actions in our action set
	void set_actions(Array p_actions); // Set our actions using an array of actions (for loading a resource)
	Array get_actions() const; // Get our actions as an array (for saving a resource)

	Ref<OpenXRAction> get_action(const String p_name) const; // Retrieve an action by name
	void add_action(Ref<OpenXRAction> p_action); // Add a new action to our action set
	void remove_action(Ref<OpenXRAction> p_action); // remove a action from our action set

	Ref<OpenXRAction> add_new_action(const char *p_name, const char *p_localized_name, const OpenXRAction::ActionType p_action_type, const char *p_toplevel_paths); // Helper function for adding and setting up an action

	// TODO add validation to display in the interface that checks if we have duplicate action names within our action set

	~OpenXRActionSet();
};
