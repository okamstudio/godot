/**************************************************************************/
/*  openxr_binding_modifier.h                                             */
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

#include "../action_map/openxr_action.h"
#include "core/io/resource.h"

// Part of implementation for:
// https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XR_KHR_binding_modification

class OpenXRInteractionProfile;
class OpenXRIPBinding;

class OpenXRBindingModifier : public Resource {
	GDCLASS(OpenXRBindingModifier, Resource);

protected:
	static void _bind_methods();

	GDVIRTUAL0RC_REQUIRED(String, _get_description)
	GDVIRTUAL0R_REQUIRED(PackedByteArray, _get_ip_modification)

public:
	virtual String get_description() const; // Returns the description shown in the editor
	virtual PackedByteArray get_ip_modification(); // Return the XrBindingModificationsKHR binding modifier struct data used when calling xrSuggestInteractionProfileBindings
};

class OpenXRIPBindingModifier : public OpenXRBindingModifier {
	GDCLASS(OpenXRIPBindingModifier, OpenXRBindingModifier);

protected:
	friend class OpenXRInteractionProfile;

	OpenXRInteractionProfile *interaction_profile = nullptr; // action belongs to this interaction profile

public:
	OpenXRInteractionProfile *get_interaction_profile() const { return interaction_profile; }
};

class OpenXRActionBindingModifier : public OpenXRBindingModifier {
	GDCLASS(OpenXRActionBindingModifier, OpenXRBindingModifier);

protected:
	friend class OpenXRIPBinding;

	OpenXRIPBinding *ip_binding = nullptr; // action belongs to this binding

public:
	OpenXRIPBinding *get_ip_binding() const { return ip_binding; }
};
