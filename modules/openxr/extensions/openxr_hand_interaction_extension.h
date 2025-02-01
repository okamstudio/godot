/**************************************************************************/
/*  openxr_hand_interaction_extension.h                                   */
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

#include "openxr_extension_wrapper.h"

// When supported the hand interaction extension introduces an interaction
// profile that becomes active when the user either lets go of their
// controllers or isn't using controllers at all.
//
// The OpenXR specification states that all XR runtimes that support this
// interaction profile should also allow it's controller to use this
// interaction profile.
// This means that if you only supply this interaction profile in your
// action map, it should work both when the player is holding a controller
// or using visual hand tracking.
//
// This allows easier portability between games that use controller
// tracking or hand tracking.
//
// See: https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_EXT_hand_interaction
// for more information.

class OpenXRHandInteractionExtension : public OpenXRExtensionWrapper {
public:
	static OpenXRHandInteractionExtension *get_singleton();

	OpenXRHandInteractionExtension();
	virtual ~OpenXRHandInteractionExtension() override;

	virtual HashMap<String, bool *> get_requested_extensions() override;

	bool is_available();

	virtual void on_register_metadata() override;

private:
	static OpenXRHandInteractionExtension *singleton;

	bool available = false;
};
