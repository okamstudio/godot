/**************************************************************************/
/*  openxr_interaction_profile_metadata.h                                 */
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

///////////////////////////////////////////////////////////////////////////
// Stores available interaction profile metadata
//
// OpenXR defines and hardcodes all the supported input devices and their
// paths as part of the OpenXR spec. When support for new devices is
// introduced this often starts life as an extension that needs to be enabled
// until it's adopted into the core. As there is no interface to
// enumerate the possibly paths, and that any OpenXR runtime would likely
// limit such enumeration to those input devices supported by that runtime
// there is no other option than to hardcode this.
//
// Note that we need to include paths of our extensions in our action map
// regardless of whether the developers machine supports the extension or
// not. Unsupported paths are filtered out when the action map is submitted
// to the OpenXR runtime.
//
// Note on action type that automatic conversions between boolean and float
// are supported but otherwise action types should match between action and
// input/output paths.

#include "openxr_action.h"

#include "core/object/object.h"
#include "core/templates/hash_map.h"

#define XR_PATH_UNSUPPORTED_NAME "unsupported"

class OpenXRInteractionProfileMetadata : public Object {
	GDCLASS(OpenXRInteractionProfileMetadata, Object);

public:
	struct TopLevelPath {
		String display_name; // User friendly display name (i.e. Left controller)
		String openxr_path; // Path in OpenXR (i.e. /user/hand/left)
		String openxr_extension_name; // If set, only available if extension is enabled (i.e. XR_HTCX_vive_tracker_interaction)
	};

	struct IOPath {
		String display_name; // User friendly display name (i.e. Grip pose (left controller))
		String top_level_path; // Top level path identifying the usage of the device in relation to this input/output
		String openxr_path; // Path in OpenXR (i.e. /user/hand/left/input/grip/pose)
		String openxr_extension_name; // If set, only available if extension is enabled (i.e. XR_EXT_palm_pose)
		OpenXRAction::ActionType action_type; // Type of input/output
	};

	struct InteractionProfile {
		String display_name; // User friendly display name (i.e. Simple controller)
		String openxr_path; // Path in OpenXR (i.e. /interaction_profiles/khr/simple_controller)
		String openxr_extension_name; // If set, only available if extension is enabled (i.e. XR_HTCX_vive_tracker_interaction)
		Vector<IOPath> io_paths; // Inputs and outputs for this device

		bool has_io_path(const String p_io_path) const;
		const IOPath *get_io_path(const String p_io_path) const;
	};

private:
	static OpenXRInteractionProfileMetadata *singleton;

	HashMap<String, String> profile_renames;
	Vector<TopLevelPath> top_level_paths;
	Vector<InteractionProfile> interaction_profiles;

	void _register_core_metadata();

protected:
	static void _bind_methods();

public:
	static OpenXRInteractionProfileMetadata *get_singleton() { return singleton; }

	OpenXRInteractionProfileMetadata();
	~OpenXRInteractionProfileMetadata();

	void register_profile_rename(const String &p_old_name, const String &p_new_name);
	String check_profile_name(const String &p_name) const;

	void register_top_level_path(const String &p_display_name, const String &p_openxr_path, const String &p_openxr_extension_name);
	bool has_top_level_path(const String p_openxr_path) const;
	String get_top_level_name(const String p_openxr_path) const;
	String get_top_level_extension(const String p_openxr_path) const;

	void register_interaction_profile(const String &p_display_name, const String &p_openxr_path, const String &p_openxr_extension_name);
	bool has_interaction_profile(const String p_openxr_path) const;
	String get_interaction_profile_extension(const String p_openxr_path) const;
	const InteractionProfile *get_profile(const String p_openxr_path) const;
	PackedStringArray get_interaction_profile_paths() const;

	void register_io_path(const String &p_interaction_profile, const String &p_display_name, const String &p_toplevel_path, const String &p_openxr_path, const String &p_openxr_extension_name, OpenXRAction::ActionType p_action_type);
	const IOPath *get_io_path(const String p_interaction_profile, const String p_io_path) const;
};
