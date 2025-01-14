/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "action_map/openxr_action.h"
#include "action_map/openxr_action_map.h"
#include "action_map/openxr_action_set.h"
#include "action_map/openxr_haptic_feedback.h"
#include "action_map/openxr_interaction_profile.h"
#include "action_map/openxr_interaction_profile_metadata.h"
#include "openxr_interface.h"

#include "extensions/openxr_extension_wrapper_extension.h"

#include "scene/openxr_composition_layer.h"
#include "scene/openxr_composition_layer_cylinder.h"
#include "scene/openxr_composition_layer_equirect.h"
#include "scene/openxr_composition_layer_quad.h"
#include "scene/openxr_hand.h"
#include "scene/openxr_visibility_mask.h"

#include "extensions/openxr_composition_layer_depth_extension.h"
#include "extensions/openxr_composition_layer_extension.h"
#include "extensions/openxr_debug_utils_extension.h"
#include "extensions/openxr_dpad_binding_extension.h"
#include "extensions/openxr_eye_gaze_interaction.h"
#include "extensions/openxr_fb_display_refresh_rate_extension.h"
#include "extensions/openxr_hand_interaction_extension.h"
#include "extensions/openxr_hand_tracking_extension.h"
#include "extensions/openxr_htc_controller_extension.h"
#include "extensions/openxr_htc_vive_tracker_extension.h"
#include "extensions/openxr_huawei_controller_extension.h"
#include "extensions/openxr_local_floor_extension.h"
#include "extensions/openxr_meta_controller_extension.h"
#include "extensions/openxr_ml2_controller_extension.h"
#include "extensions/openxr_mxink_extension.h"
#include "extensions/openxr_palm_pose_extension.h"
#include "extensions/openxr_performance_settings_extension.h"
#include "extensions/openxr_pico_controller_extension.h"
#include "extensions/openxr_valve_analog_threshold_extension.h"
#include "extensions/openxr_visibility_mask_extension.h"
#include "extensions/openxr_wmr_controller_extension.h"

#ifdef TOOLS_ENABLED
#include "editor/openxr_editor_plugin.h"
#endif

#ifdef ANDROID_ENABLED
#include "extensions/platform/openxr_android_extension.h"
#endif

#include "core/config/project_settings.h"
#include "main/main.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"

#include "editor/openxr_binding_modifier_editor.h"
#include "editor/openxr_interaction_profile_editor.h"

#endif

static OpenXRAPI *openxr_api = nullptr;
static OpenXRInteractionProfileMetadata *openxr_interaction_profile_metadata = nullptr;
static Ref<OpenXRInterface> openxr_interface;

#ifdef TOOLS_ENABLED
static void _editor_init() {
	if (OpenXRAPI::openxr_is_enabled(false)) {
		// Only add our OpenXR action map editor if OpenXR is enabled for our project

		if (openxr_interaction_profile_metadata == nullptr) {
			// If we didn't initialize our actionmap metadata at startup, we initialize it now.
			openxr_interaction_profile_metadata = memnew(OpenXRInteractionProfileMetadata);
			ERR_FAIL_NULL(openxr_interaction_profile_metadata);
		}

		OpenXREditorPlugin *openxr_plugin = memnew(OpenXREditorPlugin());
		EditorNode::get_singleton()->add_editor_plugin(openxr_plugin);
	}
}
#endif

void initialize_openxr_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		GDREGISTER_CLASS(OpenXRExtensionWrapperExtension);
		GDREGISTER_CLASS(OpenXRAPIExtension);
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		if (OpenXRAPI::openxr_is_enabled(false)) {
			// Always register our extension wrappers even if we don't initialize OpenXR.
			// Some of these wrappers will add functionality to our editor.
#ifdef ANDROID_ENABLED
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRAndroidExtension));
#endif

			// register our other extensions
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRPalmPoseExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRLocalFloorExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRPicoControllerExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRCompositionLayerDepthExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRCompositionLayerExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRHTCControllerExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRHTCViveTrackerExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRHuaweiControllerExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRDisplayRefreshRateExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRWMRControllerExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRML2ControllerExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRMetaControllerExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXREyeGazeInteractionExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRHandInteractionExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRMxInkExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRVisibilityMaskExtension));
			OpenXRAPI::register_extension_wrapper(memnew(OpenXRPerformanceSettingsExtension));

			// register gated extensions
			if (int(GLOBAL_GET("xr/openxr/extensions/debug_utils")) > 0) {
				OpenXRAPI::register_extension_wrapper(memnew(OpenXRDebugUtilsExtension));
			}
			if (GLOBAL_GET("xr/openxr/extensions/hand_tracking")) {
				OpenXRAPI::register_extension_wrapper(memnew(OpenXRHandTrackingExtension));
			}

			// register gated binding modifiers
			if (GLOBAL_GET("xr/openxr/binding_modifiers/analog_threshold")) {
				OpenXRAPI::register_extension_wrapper(memnew(OpenXRValveAnalogThresholdExtension));
			}
			if (GLOBAL_GET("xr/openxr/binding_modifiers/dpad_binding")) {
				OpenXRAPI::register_extension_wrapper(memnew(OpenXRDPadBindingExtension));
			}
		}

		if (OpenXRAPI::openxr_is_enabled()) {
			openxr_interaction_profile_metadata = memnew(OpenXRInteractionProfileMetadata);
			ERR_FAIL_NULL(openxr_interaction_profile_metadata);
			openxr_api = memnew(OpenXRAPI);
			ERR_FAIL_NULL(openxr_api);

			if (!openxr_api->initialize(Main::get_rendering_driver_name())) {
				const char *init_error_message =
						"OpenXR was requested but failed to start.\n"
						"Please check if your HMD is connected.\n"
#ifdef WINDOWS_ENABLED
						"When using Windows Mixed Reality, note that WMR only has DirectX support. Make sure SteamVR is your default OpenXR runtime.\n"
#endif
						"Godot will start in normal mode.\n";

				WARN_PRINT(init_error_message);

				bool init_show_startup_alert = GLOBAL_GET("xr/openxr/startup_alert");
				if (init_show_startup_alert) {
					OS::get_singleton()->alert(init_error_message);
				}

				memdelete(openxr_api);
				openxr_api = nullptr;
				return;
			}
		}
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(OpenXRInterface);

		GDREGISTER_CLASS(OpenXRAction);
		GDREGISTER_CLASS(OpenXRActionSet);
		GDREGISTER_CLASS(OpenXRActionMap);
		GDREGISTER_CLASS(OpenXRInteractionProfileMetadata);
		GDREGISTER_CLASS(OpenXRIPBinding);
		GDREGISTER_CLASS(OpenXRInteractionProfile);

		GDREGISTER_ABSTRACT_CLASS(OpenXRBindingModifier);
		GDREGISTER_VIRTUAL_CLASS(OpenXRIPBindingModifier);
		GDREGISTER_VIRTUAL_CLASS(OpenXRActionBindingModifier);
		GDREGISTER_CLASS(OpenXRAnalogThresholdModifier);
		GDREGISTER_CLASS(OpenXRDpadBindingModifier);

		GDREGISTER_ABSTRACT_CLASS(OpenXRHapticBase);
		GDREGISTER_CLASS(OpenXRHapticVibration);

		GDREGISTER_ABSTRACT_CLASS(OpenXRCompositionLayer);
		GDREGISTER_CLASS(OpenXRCompositionLayerEquirect);
		GDREGISTER_CLASS(OpenXRCompositionLayerCylinder);
		GDREGISTER_CLASS(OpenXRCompositionLayerQuad);

		GDREGISTER_CLASS(OpenXRHand);

		GDREGISTER_CLASS(OpenXRVisibilityMask);

		XRServer *xr_server = XRServer::get_singleton();
		if (xr_server) {
			openxr_interface.instantiate();
			xr_server->add_interface(openxr_interface);

			if (openxr_interface->initialize_on_startup()) {
				openxr_interface->initialize();
			}
		}

#ifdef TOOLS_ENABLED
		GDREGISTER_ABSTRACT_CLASS(OpenXRInteractionProfileEditorBase);
		GDREGISTER_CLASS(OpenXRInteractionProfileEditor);
		GDREGISTER_CLASS(OpenXRBindingModifierEditor);

		EditorNode::add_init_callback(_editor_init);
#endif
	}
}

void uninitialize_openxr_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	if (openxr_interface.is_valid()) {
		// uninitialize just in case
		if (openxr_interface->is_initialized()) {
			openxr_interface->uninitialize();
		}

		// unregister our interface from the XR server
		XRServer *xr_server = XRServer::get_singleton();
		if (xr_server) {
			if (xr_server->get_primary_interface() == openxr_interface) {
				xr_server->set_primary_interface(Ref<XRInterface>());
			}
			xr_server->remove_interface(openxr_interface);
		}

		// and release
		openxr_interface.unref();
	}

	if (openxr_api) {
		openxr_api->finish();

		memdelete(openxr_api);
		openxr_api = nullptr;
	}

	if (openxr_interaction_profile_metadata) {
		memdelete(openxr_interaction_profile_metadata);
		openxr_interaction_profile_metadata = nullptr;
	}

	// cleanup our extension wrappers
	OpenXRAPI::cleanup_extension_wrappers();
}
