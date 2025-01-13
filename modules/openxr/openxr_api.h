/**************************************************************************/
/*  openxr_api.h                                                          */
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

#ifndef OPENXR_API_H
#define OPENXR_API_H

#include "action_map/openxr_action.h"
#include "extensions/openxr_composition_layer_provider.h"
#include "extensions/openxr_extension_wrapper.h"
#include "util.h"

#include "core/error/error_macros.h"
#include "core/math/projection.h"
#include "core/math/transform_3d.h"
#include "core/math/vector2.h"
#include "core/string/ustring.h"
#include "core/templates/rid_owner.h"
#include "core/templates/vector.h"
#include "servers/rendering_server.h"
#include "servers/xr/xr_pose.h"

#include <openxr/openxr.h>

// forward declarations, we don't want to include these fully
class OpenXRInterface;

class OpenXRAPI {
public:
	class OpenXRSwapChainInfo {
	private:
		XrSwapchain swapchain = XR_NULL_HANDLE;
		void *swapchain_graphics_data = nullptr;
		uint32_t image_index = 0;
		bool image_acquired = false;
		bool skip_acquire_swapchain = false;

		static Vector<OpenXRSwapChainInfo> free_queue;

	public:
		_FORCE_INLINE_ XrSwapchain get_swapchain() const { return swapchain; }
		_FORCE_INLINE_ bool is_image_acquired() const { return image_acquired; }

		bool create(XrSwapchainCreateFlags p_create_flags, XrSwapchainUsageFlags p_usage_flags, int64_t p_swapchain_format, uint32_t p_width, uint32_t p_height, uint32_t p_sample_count, uint32_t p_array_size);
		void queue_free();
		static void free_queued();
		void free();

		bool acquire(bool &p_should_render);
		bool release();
		RID get_image();
	};

private:
	// our singleton
	static OpenXRAPI *singleton;

	// Registered extension wrappers
	static Vector<OpenXRExtensionWrapper *> registered_extension_wrappers;

	// linked XR interface
	OpenXRInterface *xr_interface = nullptr;

	// layers
	uint32_t num_layer_properties = 0;
	XrApiLayerProperties *layer_properties = nullptr;

	// extensions
	uint32_t num_supported_extensions = 0;
	XrExtensionProperties *supported_extensions = nullptr;
	Vector<CharString> enabled_extensions;

	// composition layer providers
	Vector<OpenXRCompositionLayerProvider *> composition_layer_providers;

	// projection views extensions
	Vector<OpenXRExtensionWrapper *> projection_views_extensions;

	// view configuration
	uint32_t num_view_configuration_types = 0;
	XrViewConfigurationType *supported_view_configuration_types = nullptr;

	// reference spaces
	uint32_t num_reference_spaces = 0;
	XrReferenceSpaceType *supported_reference_spaces = nullptr;

	// swapchains (note these are platform dependent)
	uint32_t num_swapchain_formats = 0;
	int64_t *supported_swapchain_formats = nullptr;

	// system info
	String runtime_name;
	String runtime_version;

	// configuration
	XrFormFactor form_factor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	XrViewConfigurationType view_configuration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	XrReferenceSpaceType requested_reference_space = XR_REFERENCE_SPACE_TYPE_STAGE;
	XrReferenceSpaceType reference_space = XR_REFERENCE_SPACE_TYPE_LOCAL;
	bool submit_depth_buffer = false; // if set to true we submit depth buffers to OpenXR if a suitable extension is enabled.

	// blend mode
	XrEnvironmentBlendMode environment_blend_mode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	XrEnvironmentBlendMode requested_environment_blend_mode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	uint32_t num_supported_environment_blend_modes = 0;
	XrEnvironmentBlendMode *supported_environment_blend_modes = nullptr;
	bool emulate_environment_blend_mode_alpha_blend = false;

	// state
	XrInstance instance = XR_NULL_HANDLE;
	XrSystemId system_id = 0;
	String system_name;
	uint32_t vendor_id = 0;
	XrSystemTrackingProperties tracking_properties;
	XrSession session = XR_NULL_HANDLE;
	XrSessionState session_state = XR_SESSION_STATE_UNKNOWN;
	bool running = false;
	XrFrameState frame_state = { XR_TYPE_FRAME_STATE, nullptr, 0, 0, false };
	double render_target_size_multiplier = 1.0;
	Rect2i render_region;

	OpenXRGraphicsExtensionWrapper *graphics_extension = nullptr;
	XrSystemGraphicsProperties graphics_properties;

	uint32_t view_count = 0;
	XrViewConfigurationView *view_configuration_views = nullptr;

	enum OpenXRSwapChainTypes {
		OPENXR_SWAPCHAIN_COLOR,
		OPENXR_SWAPCHAIN_DEPTH,
		OPENXR_SWAPCHAIN_MAX
	};

	int64_t color_swapchain_format = 0;
	int64_t depth_swapchain_format = 0;

	bool play_space_is_dirty = true;
	XrSpace play_space = XR_NULL_HANDLE;
	XrSpace view_space = XR_NULL_HANDLE;
	XRPose::TrackingConfidence head_pose_confidence = XRPose::XR_TRACKING_CONFIDENCE_NONE;

	RID velocity_texture;
	RID velocity_depth_texture;
	Size2i velocity_target_size;

	// When LOCAL_FLOOR isn't supported, we use an approach based on the example code in the
	// OpenXR spec in order to emulate it.
	// See: https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_EXT_local_floor
	struct LocalFloorEmulation {
		bool enabled = false;
		XrSpace local_space = XR_NULL_HANDLE;
		XrSpace stage_space = XR_NULL_HANDLE;
		bool should_reset_floor_height = false;
	} local_floor_emulation;

	bool reset_emulated_floor_height();

	bool load_layer_properties();
	bool load_supported_extensions();
	bool is_extension_supported(const String &p_extension) const;
	bool is_extension_enabled(const String &p_extension) const;

	bool openxr_loader_init();
	bool resolve_instance_openxr_symbols();

#ifdef ANDROID_ENABLED
	// On Android we keep tracker of our external OpenXR loader
	void *openxr_loader_library_handle = nullptr;
#endif

	// function pointers
#ifdef ANDROID_ENABLED
	// On non-Android platforms we use the OpenXR symbol linked into the engine binary.
	PFN_xrGetInstanceProcAddr xrGetInstanceProcAddr = nullptr;
#endif
	EXT_PROTO_XRRESULT_FUNC3(xrAcquireSwapchainImage, (XrSwapchain), swapchain, (const XrSwapchainImageAcquireInfo *), acquireInfo, (uint32_t *), index)
	EXT_PROTO_XRRESULT_FUNC3(xrApplyHapticFeedback, (XrSession), session, (const XrHapticActionInfo *), hapticActionInfo, (const XrHapticBaseHeader *), hapticFeedback)
	EXT_PROTO_XRRESULT_FUNC2(xrAttachSessionActionSets, (XrSession), session, (const XrSessionActionSetsAttachInfo *), attachInfo)
	EXT_PROTO_XRRESULT_FUNC2(xrBeginFrame, (XrSession), session, (const XrFrameBeginInfo *), frameBeginInfo)
	EXT_PROTO_XRRESULT_FUNC2(xrBeginSession, (XrSession), session, (const XrSessionBeginInfo *), beginInfo)
	EXT_PROTO_XRRESULT_FUNC3(xrCreateAction, (XrActionSet), actionSet, (const XrActionCreateInfo *), createInfo, (XrAction *), action)
	EXT_PROTO_XRRESULT_FUNC3(xrCreateActionSet, (XrInstance), instance, (const XrActionSetCreateInfo *), createInfo, (XrActionSet *), actionSet)
	EXT_PROTO_XRRESULT_FUNC3(xrCreateActionSpace, (XrSession), session, (const XrActionSpaceCreateInfo *), createInfo, (XrSpace *), space)
	EXT_PROTO_XRRESULT_FUNC2(xrCreateInstance, (const XrInstanceCreateInfo *), createInfo, (XrInstance *), instance)
	EXT_PROTO_XRRESULT_FUNC3(xrCreateReferenceSpace, (XrSession), session, (const XrReferenceSpaceCreateInfo *), createInfo, (XrSpace *), space)
	EXT_PROTO_XRRESULT_FUNC3(xrCreateSession, (XrInstance), instance, (const XrSessionCreateInfo *), createInfo, (XrSession *), session)
	EXT_PROTO_XRRESULT_FUNC3(xrCreateSwapchain, (XrSession), session, (const XrSwapchainCreateInfo *), createInfo, (XrSwapchain *), swapchain)
	EXT_PROTO_XRRESULT_FUNC1(xrDestroyAction, (XrAction), action)
	EXT_PROTO_XRRESULT_FUNC1(xrDestroyActionSet, (XrActionSet), actionSet)
	EXT_PROTO_XRRESULT_FUNC1(xrDestroyInstance, (XrInstance), instance)
	EXT_PROTO_XRRESULT_FUNC1(xrDestroySession, (XrSession), session)
	EXT_PROTO_XRRESULT_FUNC1(xrDestroySpace, (XrSpace), space)
	EXT_PROTO_XRRESULT_FUNC1(xrDestroySwapchain, (XrSwapchain), swapchain)
	EXT_PROTO_XRRESULT_FUNC2(xrEndFrame, (XrSession), session, (const XrFrameEndInfo *), frameEndInfo)
	EXT_PROTO_XRRESULT_FUNC1(xrEndSession, (XrSession), session)
	EXT_PROTO_XRRESULT_FUNC3(xrEnumerateApiLayerProperties, (uint32_t), propertyCapacityInput, (uint32_t *), propertyCountOutput, (XrApiLayerProperties *), properties)
	EXT_PROTO_XRRESULT_FUNC6(xrEnumerateEnvironmentBlendModes, (XrInstance), instance, (XrSystemId), systemId, (XrViewConfigurationType), viewConfigurationType, (uint32_t), environmentBlendModeCapacityInput, (uint32_t *), environmentBlendModeCountOutput, (XrEnvironmentBlendMode *), environmentBlendModes)
	EXT_PROTO_XRRESULT_FUNC4(xrEnumerateInstanceExtensionProperties, (const char *), layerName, (uint32_t), propertyCapacityInput, (uint32_t *), propertyCountOutput, (XrExtensionProperties *), properties)
	EXT_PROTO_XRRESULT_FUNC4(xrEnumerateReferenceSpaces, (XrSession), session, (uint32_t), spaceCapacityInput, (uint32_t *), spaceCountOutput, (XrReferenceSpaceType *), spaces)
	EXT_PROTO_XRRESULT_FUNC4(xrEnumerateSwapchainFormats, (XrSession), session, (uint32_t), formatCapacityInput, (uint32_t *), formatCountOutput, (int64_t *), formats)
	EXT_PROTO_XRRESULT_FUNC5(xrEnumerateViewConfigurations, (XrInstance), instance, (XrSystemId), systemId, (uint32_t), viewConfigurationTypeCapacityInput, (uint32_t *), viewConfigurationTypeCountOutput, (XrViewConfigurationType *), viewConfigurationTypes)
	EXT_PROTO_XRRESULT_FUNC6(xrEnumerateViewConfigurationViews, (XrInstance), instance, (XrSystemId), systemId, (XrViewConfigurationType), viewConfigurationType, (uint32_t), viewCapacityInput, (uint32_t *), viewCountOutput, (XrViewConfigurationView *), views)
	EXT_PROTO_XRRESULT_FUNC3(xrGetActionStateBoolean, (XrSession), session, (const XrActionStateGetInfo *), getInfo, (XrActionStateBoolean *), state)
	EXT_PROTO_XRRESULT_FUNC3(xrGetActionStateFloat, (XrSession), session, (const XrActionStateGetInfo *), getInfo, (XrActionStateFloat *), state)
	EXT_PROTO_XRRESULT_FUNC3(xrGetActionStateVector2f, (XrSession), session, (const XrActionStateGetInfo *), getInfo, (XrActionStateVector2f *), state)
	EXT_PROTO_XRRESULT_FUNC3(xrGetCurrentInteractionProfile, (XrSession), session, (XrPath), topLevelUserPath, (XrInteractionProfileState *), interactionProfile)
	EXT_PROTO_XRRESULT_FUNC2(xrGetInstanceProperties, (XrInstance), instance, (XrInstanceProperties *), instanceProperties)
	EXT_PROTO_XRRESULT_FUNC3(xrGetReferenceSpaceBoundsRect, (XrSession), session, (XrReferenceSpaceType), referenceSpaceType, (XrExtent2Df *), bounds)
	EXT_PROTO_XRRESULT_FUNC3(xrGetSystem, (XrInstance), instance, (const XrSystemGetInfo *), getInfo, (XrSystemId *), systemId)
	EXT_PROTO_XRRESULT_FUNC3(xrGetSystemProperties, (XrInstance), instance, (XrSystemId), systemId, (XrSystemProperties *), properties)
	EXT_PROTO_XRRESULT_FUNC4(xrLocateSpace, (XrSpace), space, (XrSpace), baseSpace, (XrTime), time, (XrSpaceLocation *), location)
	EXT_PROTO_XRRESULT_FUNC6(xrLocateViews, (XrSession), session, (const XrViewLocateInfo *), viewLocateInfo, (XrViewState *), viewState, (uint32_t), viewCapacityInput, (uint32_t *), viewCountOutput, (XrView *), views)
	EXT_PROTO_XRRESULT_FUNC5(xrPathToString, (XrInstance), instance, (XrPath), path, (uint32_t), bufferCapacityInput, (uint32_t *), bufferCountOutput, (char *), buffer)
	EXT_PROTO_XRRESULT_FUNC2(xrPollEvent, (XrInstance), instance, (XrEventDataBuffer *), eventData)
	EXT_PROTO_XRRESULT_FUNC2(xrReleaseSwapchainImage, (XrSwapchain), swapchain, (const XrSwapchainImageReleaseInfo *), releaseInfo)
	EXT_PROTO_XRRESULT_FUNC3(xrResultToString, (XrInstance), instance, (XrResult), value, (char *), buffer)
	EXT_PROTO_XRRESULT_FUNC3(xrStringToPath, (XrInstance), instance, (const char *), pathString, (XrPath *), path)
	EXT_PROTO_XRRESULT_FUNC2(xrSuggestInteractionProfileBindings, (XrInstance), instance, (const XrInteractionProfileSuggestedBinding *), suggestedBindings)
	EXT_PROTO_XRRESULT_FUNC2(xrSyncActions, (XrSession), session, (const XrActionsSyncInfo *), syncInfo)
	EXT_PROTO_XRRESULT_FUNC3(xrWaitFrame, (XrSession), session, (const XrFrameWaitInfo *), frameWaitInfo, (XrFrameState *), frameState)
	EXT_PROTO_XRRESULT_FUNC2(xrWaitSwapchainImage, (XrSwapchain), swapchain, (const XrSwapchainImageWaitInfo *), waitInfo)

	// instance
	bool create_instance();
	bool get_system_info();
	bool load_supported_view_configuration_types();
	bool load_supported_environmental_blend_modes();
	bool is_view_configuration_supported(XrViewConfigurationType p_configuration_type) const;
	bool load_supported_view_configuration_views(XrViewConfigurationType p_configuration_type);
	void destroy_instance();

	// session
	bool create_session();
	bool load_supported_reference_spaces();
	bool is_reference_space_supported(XrReferenceSpaceType p_reference_space);
	bool setup_play_space();
	bool setup_view_space();
	bool load_supported_swapchain_formats();
	bool is_swapchain_format_supported(int64_t p_swapchain_format);
	bool obtain_swapchain_formats();
	bool create_main_swapchains(Size2i p_size);
	void free_main_swapchains();
	void destroy_session();

	// action map
	struct Tracker { // Trackers represent tracked physical objects such as controllers, pucks, etc.
		String name; // Name for this tracker (i.e. "/user/hand/left")
		XrPath toplevel_path; // OpenXR XrPath for this tracker
		RID active_profile_rid; // RID of the active profile for this tracker
	};
	RID_Owner<Tracker, true> tracker_owner;
	RID get_tracker_rid(XrPath p_path);

	struct ActionSet { // Action sets define a set of actions that can be enabled together
		String name; // Name for this action set (i.e. "godot_action_set")
		bool is_attached; // If true our action set has been attached to the session and can no longer be modified
		XrActionSet handle; // OpenXR handle for this action set
	};
	RID_Owner<ActionSet, true> action_set_owner;

	struct ActionTracker { // Links and action to a tracker
		RID tracker_rid; // RID of the tracker
		XrSpace space; // Optional space for pose actions
		bool was_location_valid; // If true the last position we obtained was valid
	};

	struct Action { // Actions define the inputs and outputs in OpenXR
		RID action_set_rid; // RID of the action set this action belongs to
		String name; // Name for this action (i.e. "aim_pose")
		XrActionType action_type; // Type of action (bool, float, etc.)
		Vector<ActionTracker> trackers; // The trackers this action can be used with
		XrAction handle; // OpenXR handle for this action
	};
	RID_Owner<Action, true> action_owner;
	RID get_action_rid(XrAction p_action);

	struct InteractionProfile { // Interaction profiles define suggested bindings between the physical inputs on controller types and our actions
		String name; // Name of the interaction profile (i.e. "/interaction_profiles/valve/index_controller")
		XrPath path; // OpenXR path for this profile
		Vector<XrActionSuggestedBinding> bindings; // OpenXR action bindings
		Vector<PackedByteArray> modifiers; // Array of modifiers we'll add into XrBindingModificationsKHR
	};
	RID_Owner<InteractionProfile, true> interaction_profile_owner;
	RID get_interaction_profile_rid(XrPath p_path);
	XrPath get_interaction_profile_path(RID p_interaction_profile);

	struct OrderedCompositionLayer {
		const XrCompositionLayerBaseHeader *composition_layer;
		int sort_order;

		_FORCE_INLINE_ bool operator()(const OrderedCompositionLayer &a, const OrderedCompositionLayer &b) const {
			return a.sort_order < b.sort_order || (a.sort_order == b.sort_order && uint64_t(a.composition_layer) < uint64_t(b.composition_layer));
		}
	};

	// state changes
	bool poll_events();
	bool on_state_idle();
	bool on_state_ready();
	bool on_state_synchronized();
	bool on_state_visible();
	bool on_state_focused();
	bool on_state_stopping();
	bool on_state_loss_pending();
	bool on_state_exiting();

	// convenience
	void copy_string_to_char_buffer(const String p_string, char *p_buffer, int p_buffer_len);

	// Render state, Only accessible in rendering thread
	struct RenderState {
		bool running = false;
		bool should_render = false;
		bool has_xr_viewport = false;
		XrTime predicted_display_time = 0;
		XrSpace play_space = XR_NULL_HANDLE;
		double render_target_size_multiplier = 1.0;
		uint64_t frame = 0;
		Rect2i render_region;

		uint32_t view_count = 0;
		XrView *views = nullptr;
		XrCompositionLayerProjectionView *projection_views = nullptr;
		XrCompositionLayerDepthInfoKHR *depth_views = nullptr; // Only used by Composition Layer Depth Extension if available
		bool submit_depth_buffer = false; // if set to true we submit depth buffers to OpenXR if a suitable extension is enabled.
		bool view_pose_valid = false;

		double z_near = 0.0;
		double z_far = 0.0;

		XrCompositionLayerProjection projection_layer = {
			XR_TYPE_COMPOSITION_LAYER_PROJECTION, // type
			nullptr, // next
			0, // layerFlags
			XR_NULL_HANDLE, // space
			0, // viewCount
			nullptr // views
		};

		Size2i main_swapchain_size;
		OpenXRSwapChainInfo main_swapchains[OPENXR_SWAPCHAIN_MAX];
	} render_state;

	static void _allocate_view_buffers(uint32_t p_view_count, bool p_submit_depth_buffer);
	static void _set_render_session_running(bool p_is_running);
	static void _set_render_display_info(XrTime p_predicted_display_time, bool p_should_render);
	static void _set_render_play_space(uint64_t p_play_space);
	static void _set_render_state_multiplier(double p_render_target_size_multiplier);
	static void _set_render_state_render_region(const Rect2i &p_render_region);

	_FORCE_INLINE_ void allocate_view_buffers(uint32_t p_view_count, bool p_submit_depth_buffer) {
		// If we're rendering on a separate thread, we may still be processing the last frame, don't communicate this till we're ready...
		RenderingServer *rendering_server = RenderingServer::get_singleton();
		ERR_FAIL_NULL(rendering_server);

		rendering_server->call_on_render_thread(callable_mp_static(&OpenXRAPI::_allocate_view_buffers).bind(p_view_count, p_submit_depth_buffer));
	}

	_FORCE_INLINE_ void set_render_session_running(bool p_is_running) {
		// If we're rendering on a separate thread, we may still be processing the last frame, don't communicate this till we're ready...
		RenderingServer *rendering_server = RenderingServer::get_singleton();
		ERR_FAIL_NULL(rendering_server);

		rendering_server->call_on_render_thread(callable_mp_static(&OpenXRAPI::_set_render_session_running).bind(p_is_running));
	}

	_FORCE_INLINE_ void set_render_display_info(XrTime p_predicted_display_time, bool p_should_render) {
		// If we're rendering on a separate thread, we may still be processing the last frame, don't communicate this till we're ready...
		RenderingServer *rendering_server = RenderingServer::get_singleton();
		ERR_FAIL_NULL(rendering_server);

		rendering_server->call_on_render_thread(callable_mp_static(&OpenXRAPI::_set_render_display_info).bind(p_predicted_display_time, p_should_render));
	}

	_FORCE_INLINE_ void set_render_play_space(XrSpace p_play_space) {
		// If we're rendering on a separate thread, we may still be processing the last frame, don't communicate this till we're ready...
		RenderingServer *rendering_server = RenderingServer::get_singleton();
		ERR_FAIL_NULL(rendering_server);

		rendering_server->call_on_render_thread(callable_mp_static(&OpenXRAPI::_set_render_play_space).bind(uint64_t(p_play_space)));
	}

	_FORCE_INLINE_ void set_render_state_multiplier(double p_render_target_size_multiplier) {
		// If we're rendering on a separate thread, we may still be processing the last frame, don't communicate this till we're ready...
		RenderingServer *rendering_server = RenderingServer::get_singleton();
		ERR_FAIL_NULL(rendering_server);

		rendering_server->call_on_render_thread(callable_mp_static(&OpenXRAPI::_set_render_state_multiplier).bind(p_render_target_size_multiplier));
	}

	_FORCE_INLINE_ void set_render_state_render_region(const Rect2i &p_render_region) {
		RenderingServer *rendering_server = RenderingServer::get_singleton();
		ERR_FAIL_NULL(rendering_server);

		rendering_server->call_on_render_thread(callable_mp_static(&OpenXRAPI::_set_render_state_render_region).bind(p_render_region));
	}

public:
	XrInstance get_instance() const { return instance; }
	XrSystemId get_system_id() const { return system_id; }
	XrSession get_session() const { return session; }
	OpenXRGraphicsExtensionWrapper *get_graphics_extension() const { return graphics_extension; }
	String get_runtime_name() const { return runtime_name; }
	String get_runtime_version() const { return runtime_version; }

	// helper method to convert an XrPosef to a Transform3D
	Transform3D transform_from_pose(const XrPosef &p_pose);

	// helper method to get a valid Transform3D from an openxr space location
	XRPose::TrackingConfidence transform_from_location(const XrSpaceLocation &p_location, Transform3D &r_transform);
	XRPose::TrackingConfidence transform_from_location(const XrHandJointLocationEXT &p_location, Transform3D &r_transform);
	void parse_velocities(const XrSpaceVelocity &p_velocity, Vector3 &r_linear_velocity, Vector3 &r_angular_velocity);
	bool xr_result(XrResult result, const char *format, Array args = Array()) const;
	XrPath get_xr_path(const String &p_path);
	bool is_top_level_path_supported(const String &p_toplevel_path);
	bool is_interaction_profile_supported(const String &p_ip_path);
	bool interaction_profile_supports_io_path(const String &p_ip_path, const String &p_io_path);

	static bool openxr_is_enabled(bool p_check_run_in_editor = true);
	_FORCE_INLINE_ static OpenXRAPI *get_singleton() { return singleton; }

	XrResult try_get_instance_proc_addr(const char *p_name, PFN_xrVoidFunction *p_addr);
	XrResult get_instance_proc_addr(const char *p_name, PFN_xrVoidFunction *p_addr);
	String get_error_string(XrResult result) const;
	String get_swapchain_format_name(int64_t p_swapchain_format) const;
	void set_object_name(XrObjectType p_object_type, uint64_t p_object_handle, const String &p_object_name);
	void begin_debug_label_region(const String &p_label_name);
	void end_debug_label_region();
	void insert_debug_label(const String &p_label_name);

	OpenXRInterface *get_xr_interface() const { return xr_interface; }
	void set_xr_interface(OpenXRInterface *p_xr_interface);
	static void register_extension_wrapper(OpenXRExtensionWrapper *p_extension_wrapper);
	static void unregister_extension_wrapper(OpenXRExtensionWrapper *p_extension_wrapper);
	static const Vector<OpenXRExtensionWrapper *> &get_registered_extension_wrappers();
	static void register_extension_metadata();
	static void cleanup_extension_wrappers();
	static PackedStringArray get_all_requested_extensions();

	void set_form_factor(XrFormFactor p_form_factor);
	XrFormFactor get_form_factor() const { return form_factor; }

	uint32_t get_view_count();
	void set_view_configuration(XrViewConfigurationType p_view_configuration);
	XrViewConfigurationType get_view_configuration() const { return view_configuration; }

	bool set_requested_reference_space(XrReferenceSpaceType p_requested_reference_space);
	XrReferenceSpaceType get_requested_reference_space() const { return requested_reference_space; }
	XrReferenceSpaceType get_reference_space() const { return reference_space; }

	void set_submit_depth_buffer(bool p_submit_depth_buffer);
	bool get_submit_depth_buffer() const { return submit_depth_buffer; }

	bool is_initialized();
	bool is_running();
	bool initialize(const String &p_rendering_driver);
	bool initialize_session();
	void finish();

	_FORCE_INLINE_ XrSpace get_play_space() const { return play_space; }
	_FORCE_INLINE_ XrTime get_predicted_display_time() { return frame_state.predictedDisplayTime; }
	_FORCE_INLINE_ XrTime get_next_frame_time() { return frame_state.predictedDisplayTime + frame_state.predictedDisplayPeriod; }
	_FORCE_INLINE_ bool can_render() {
		return instance != XR_NULL_HANDLE && session != XR_NULL_HANDLE && running && frame_state.shouldRender;
	}

	XrHandTrackerEXT get_hand_tracker(int p_hand_index);

	Size2 get_recommended_target_size();
	XRPose::TrackingConfidence get_head_center(Transform3D &r_transform, Vector3 &r_linear_velocity, Vector3 &r_angular_velocity);
	bool get_view_transform(uint32_t p_view, Transform3D &r_transform);
	bool get_view_projection(uint32_t p_view, double p_z_near, double p_z_far, Projection &p_camera_matrix);
	Vector2 get_eye_focus(uint32_t p_view, float p_aspect);
	bool process();

	void pre_render();
	bool pre_draw_viewport(RID p_render_target);
	XrSwapchain get_color_swapchain();
	RID get_color_texture();
	RID get_depth_texture();
	void set_velocity_texture(RID p_render_target);
	RID get_velocity_texture();
	void set_velocity_depth_texture(RID p_render_target);
	RID get_velocity_depth_texture();
	void set_velocity_target_size(const Size2i &p_target_size);
	Size2i get_velocity_target_size();
	const XrCompositionLayerProjection *get_projection_layer() const;
	void post_draw_viewport(RID p_render_target);
	void end_frame();

	// Display refresh rate
	float get_display_refresh_rate() const;
	void set_display_refresh_rate(float p_refresh_rate);
	Array get_available_display_refresh_rates() const;

	// Render Target size multiplier
	double get_render_target_size_multiplier() const;
	void set_render_target_size_multiplier(double multiplier);

	Rect2i get_render_region() const;
	void set_render_region(const Rect2i &p_render_region);

	// Foveation settings
	bool is_foveation_supported() const;

	int get_foveation_level() const;
	void set_foveation_level(int p_foveation_level);

	bool get_foveation_dynamic() const;
	void set_foveation_dynamic(bool p_foveation_dynamic);

	// Play space.
	Size2 get_play_space_bounds() const;

	// swapchains
	PackedInt64Array get_supported_swapchain_formats();
	int64_t get_color_swapchain_format() const { return color_swapchain_format; }
	int64_t get_depth_swapchain_format() const { return depth_swapchain_format; }

	double get_render_state_z_near() const { return render_state.z_near; }
	double get_render_state_z_far() const { return render_state.z_far; }

	// action map
	String get_default_action_map_resource_name();

	RID tracker_create(const String p_name);
	String tracker_get_name(RID p_tracker);
	void tracker_check_profile(RID p_tracker, XrSession p_session = XR_NULL_HANDLE);
	void tracker_free(RID p_tracker);

	RID action_set_create(const String p_name, const String p_localized_name, const int p_priority);
	String action_set_get_name(RID p_action_set);
	XrActionSet action_set_get_handle(RID p_action_set);
	bool attach_action_sets(const Vector<RID> &p_action_sets);
	void action_set_free(RID p_action_set);

	RID action_create(RID p_action_set, const String p_name, const String p_localized_name, OpenXRAction::ActionType p_action_type, const Vector<RID> &p_trackers);
	String action_get_name(RID p_action);
	XrAction action_get_handle(RID p_action);
	void action_free(RID p_action);

	RID interaction_profile_create(const String p_name);
	String interaction_profile_get_name(RID p_interaction_profile);
	void interaction_profile_clear_bindings(RID p_interaction_profile);
	int interaction_profile_add_binding(RID p_interaction_profile, RID p_action, const String p_path);
	bool interaction_profile_add_modifier(RID p_interaction_profile, const PackedByteArray &p_modifier);
	bool interaction_profile_suggest_bindings(RID p_interaction_profile);
	void interaction_profile_free(RID p_interaction_profile);

	RID find_tracker(const String &p_name);
	RID find_action_set(const String p_name);
	RID find_action(const String &p_name, const RID &p_action_set = RID());

	bool sync_action_sets(const Vector<RID> p_active_sets);
	bool get_action_bool(RID p_action, RID p_tracker);
	float get_action_float(RID p_action, RID p_tracker);
	Vector2 get_action_vector2(RID p_action, RID p_tracker);
	XRPose::TrackingConfidence get_action_pose(RID p_action, RID p_tracker, Transform3D &r_transform, Vector3 &r_linear_velocity, Vector3 &r_angular_velocity);
	bool trigger_haptic_pulse(RID p_action, RID p_tracker, float p_frequency, float p_amplitude, XrDuration p_duration_ns);

	void register_composition_layer_provider(OpenXRCompositionLayerProvider *provider);
	void unregister_composition_layer_provider(OpenXRCompositionLayerProvider *provider);

	void register_projection_views_extension(OpenXRExtensionWrapper *p_extension);
	void unregister_projection_views_extension(OpenXRExtensionWrapper *p_extension);

	const XrEnvironmentBlendMode *get_supported_environment_blend_modes(uint32_t &count);
	bool is_environment_blend_mode_supported(XrEnvironmentBlendMode p_blend_mode) const;
	bool set_environment_blend_mode(XrEnvironmentBlendMode p_blend_mode);
	XrEnvironmentBlendMode get_environment_blend_mode() const { return requested_environment_blend_mode; }

	enum OpenXRAlphaBlendModeSupport {
		OPENXR_ALPHA_BLEND_MODE_SUPPORT_NONE = 0,
		OPENXR_ALPHA_BLEND_MODE_SUPPORT_REAL = 1,
		OPENXR_ALPHA_BLEND_MODE_SUPPORT_EMULATING = 2,
	};

	void set_emulate_environment_blend_mode_alpha_blend(bool p_enabled);
	OpenXRAlphaBlendModeSupport is_environment_blend_mode_alpha_blend_supported();

	OpenXRAPI();
	~OpenXRAPI();
};

#endif // OPENXR_API_H
