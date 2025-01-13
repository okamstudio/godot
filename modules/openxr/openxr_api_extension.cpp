/**************************************************************************/
/*  openxr_api_extension.cpp                                              */
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

#include "openxr_api_extension.h"

#include "extensions/openxr_extension_wrapper_extension.h"

void OpenXRAPIExtension::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_instance"), &OpenXRAPIExtension::get_instance);
	ClassDB::bind_method(D_METHOD("get_system_id"), &OpenXRAPIExtension::get_system_id);
	ClassDB::bind_method(D_METHOD("get_session"), &OpenXRAPIExtension::get_session);

	ClassDB::bind_method(D_METHOD("transform_from_pose", "pose"), &OpenXRAPIExtension::transform_from_pose);
	ClassDB::bind_method(D_METHOD("xr_result", "result", "format", "args"), &OpenXRAPIExtension::xr_result);
	ClassDB::bind_static_method("OpenXRAPIExtension", D_METHOD("openxr_is_enabled", "check_run_in_editor"), &OpenXRAPIExtension::openxr_is_enabled);
	ClassDB::bind_method(D_METHOD("get_instance_proc_addr", "name"), &OpenXRAPIExtension::get_instance_proc_addr);
	ClassDB::bind_method(D_METHOD("get_error_string", "result"), &OpenXRAPIExtension::get_error_string);
	ClassDB::bind_method(D_METHOD("get_swapchain_format_name", "swapchain_format"), &OpenXRAPIExtension::get_swapchain_format_name);
	ClassDB::bind_method(D_METHOD("set_object_name", "object_type", "object_handle", "object_name"), &OpenXRAPIExtension::set_object_name);
	ClassDB::bind_method(D_METHOD("begin_debug_label_region", "label_name"), &OpenXRAPIExtension::begin_debug_label_region);
	ClassDB::bind_method(D_METHOD("end_debug_label_region"), &OpenXRAPIExtension::end_debug_label_region);
	ClassDB::bind_method(D_METHOD("insert_debug_label", "label_name"), &OpenXRAPIExtension::insert_debug_label);

	ClassDB::bind_method(D_METHOD("is_initialized"), &OpenXRAPIExtension::is_initialized);
	ClassDB::bind_method(D_METHOD("is_running"), &OpenXRAPIExtension::is_running);

	ClassDB::bind_method(D_METHOD("get_play_space"), &OpenXRAPIExtension::get_play_space);
	ClassDB::bind_method(D_METHOD("get_predicted_display_time"), &OpenXRAPIExtension::get_predicted_display_time);
	ClassDB::bind_method(D_METHOD("get_next_frame_time"), &OpenXRAPIExtension::get_next_frame_time);
	ClassDB::bind_method(D_METHOD("can_render"), &OpenXRAPIExtension::can_render);

	ClassDB::bind_method(D_METHOD("get_hand_tracker", "hand_index"), &OpenXRAPIExtension::get_hand_tracker);

	ClassDB::bind_method(D_METHOD("register_composition_layer_provider", "extension"), &OpenXRAPIExtension::register_composition_layer_provider);
	ClassDB::bind_method(D_METHOD("unregister_composition_layer_provider", "extension"), &OpenXRAPIExtension::unregister_composition_layer_provider);

	ClassDB::bind_method(D_METHOD("register_projection_views_extension", "extension"), &OpenXRAPIExtension::register_projection_views_extension);
	ClassDB::bind_method(D_METHOD("unregister_projection_views_extension", "extension"), &OpenXRAPIExtension::unregister_projection_views_extension);

	ClassDB::bind_method(D_METHOD("get_render_state_z_near"), &OpenXRAPIExtension::get_render_state_z_near);
	ClassDB::bind_method(D_METHOD("get_render_state_z_far"), &OpenXRAPIExtension::get_render_state_z_far);

	ClassDB::bind_method(D_METHOD("set_velocity_texture", "render_target"), &OpenXRAPIExtension::set_velocity_texture);
	ClassDB::bind_method(D_METHOD("set_velocity_depth_texture", "render_target"), &OpenXRAPIExtension::set_velocity_depth_texture);
	ClassDB::bind_method(D_METHOD("set_velocity_target_size", "target_size"), &OpenXRAPIExtension::set_velocity_target_size);

	ClassDB::bind_method(D_METHOD("get_supported_swapchain_formats"), &OpenXRAPIExtension::get_supported_swapchain_formats);

	ClassDB::bind_method(D_METHOD("openxr_swapchain_create", "create_flags", "usage_flags", "swapchain_format", "width", "height", "sample_count", "array_size"), &OpenXRAPIExtension::openxr_swapchain_create);
	ClassDB::bind_method(D_METHOD("openxr_swapchain_free", "swapchain"), &OpenXRAPIExtension::openxr_swapchain_free);
	ClassDB::bind_method(D_METHOD("openxr_swapchain_get_swapchain", "swapchain"), &OpenXRAPIExtension::openxr_swapchain_get_swapchain);
	ClassDB::bind_method(D_METHOD("openxr_swapchain_acquire", "swapchain"), &OpenXRAPIExtension::openxr_swapchain_acquire);
	ClassDB::bind_method(D_METHOD("openxr_swapchain_get_image", "swapchain"), &OpenXRAPIExtension::openxr_swapchain_get_image);
	ClassDB::bind_method(D_METHOD("openxr_swapchain_release", "swapchain"), &OpenXRAPIExtension::openxr_swapchain_release);

	ClassDB::bind_method(D_METHOD("set_emulate_environment_blend_mode_alpha_blend", "enabled"), &OpenXRAPIExtension::set_emulate_environment_blend_mode_alpha_blend);
	ClassDB::bind_method(D_METHOD("is_environment_blend_mode_alpha_supported"), &OpenXRAPIExtension::is_environment_blend_mode_alpha_blend_supported);

	BIND_ENUM_CONSTANT(OPENXR_ALPHA_BLEND_MODE_SUPPORT_NONE);
	BIND_ENUM_CONSTANT(OPENXR_ALPHA_BLEND_MODE_SUPPORT_REAL);
	BIND_ENUM_CONSTANT(OPENXR_ALPHA_BLEND_MODE_SUPPORT_EMULATING);
}

uint64_t OpenXRAPIExtension::get_instance() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0);
	return (uint64_t)OpenXRAPI::get_singleton()->get_instance();
}

uint64_t OpenXRAPIExtension::get_system_id() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0);
	return (uint64_t)OpenXRAPI::get_singleton()->get_system_id();
}

uint64_t OpenXRAPIExtension::get_session() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0);
	return (uint64_t)OpenXRAPI::get_singleton()->get_session();
}

Transform3D OpenXRAPIExtension::transform_from_pose(GDExtensionConstPtr<const void> p_pose) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), Transform3D());
	return OpenXRAPI::get_singleton()->transform_from_pose(*(XrPosef *)p_pose.data);
}

bool OpenXRAPIExtension::xr_result(uint64_t result, String format, Array args) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), false);
	return OpenXRAPI::get_singleton()->xr_result((XrResult)result, format.utf8().get_data(), args);
}

bool OpenXRAPIExtension::openxr_is_enabled(bool p_check_run_in_editor) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), false);
	return OpenXRAPI::openxr_is_enabled(p_check_run_in_editor);
}

uint64_t OpenXRAPIExtension::get_instance_proc_addr(String p_name) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0);
	CharString str = p_name.utf8();
	PFN_xrVoidFunction addr = nullptr;
	XrResult result = OpenXRAPI::get_singleton()->get_instance_proc_addr(str.get_data(), &addr);
	if (result != XR_SUCCESS) {
		return 0;
	}
	return reinterpret_cast<uint64_t>(addr);
}

String OpenXRAPIExtension::get_error_string(uint64_t result) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), String());
	return OpenXRAPI::get_singleton()->get_error_string((XrResult)result);
}

String OpenXRAPIExtension::get_swapchain_format_name(int64_t p_swapchain_format) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), String());
	return OpenXRAPI::get_singleton()->get_swapchain_format_name(p_swapchain_format);
}

void OpenXRAPIExtension::set_object_name(int64_t p_object_type, uint64_t p_object_handle, const String &p_object_name) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());

	OpenXRAPI::get_singleton()->set_object_name(XrObjectType(p_object_type), p_object_handle, p_object_name);
}

void OpenXRAPIExtension::begin_debug_label_region(const String &p_label_name) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());

	OpenXRAPI::get_singleton()->begin_debug_label_region(p_label_name);
}

void OpenXRAPIExtension::end_debug_label_region() {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());

	OpenXRAPI::get_singleton()->end_debug_label_region();
}

void OpenXRAPIExtension::insert_debug_label(const String &p_label_name) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());

	OpenXRAPI::get_singleton()->insert_debug_label(p_label_name);
}

bool OpenXRAPIExtension::is_initialized() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), false);
	return OpenXRAPI::get_singleton()->is_initialized();
}

bool OpenXRAPIExtension::is_running() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), false);
	return OpenXRAPI::get_singleton()->is_running();
}

uint64_t OpenXRAPIExtension::get_play_space() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0);
	return (uint64_t)OpenXRAPI::get_singleton()->get_play_space();
}

int64_t OpenXRAPIExtension::get_predicted_display_time() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0);
	return (XrTime)OpenXRAPI::get_singleton()->get_predicted_display_time();
}

int64_t OpenXRAPIExtension::get_next_frame_time() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0);

	// In the past we needed to look a frame ahead, may be calling this unintentionally so lets warn the dev.
	WARN_PRINT_ONCE("OpenXR: Next frame timing called, verify this is intended.");

	return (XrTime)OpenXRAPI::get_singleton()->get_next_frame_time();
}

bool OpenXRAPIExtension::can_render() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), false);
	return OpenXRAPI::get_singleton()->can_render();
}

uint64_t OpenXRAPIExtension::get_hand_tracker(int p_hand_index) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0);
	return (uint64_t)OpenXRAPI::get_singleton()->get_hand_tracker(p_hand_index);
}

void OpenXRAPIExtension::register_composition_layer_provider(OpenXRExtensionWrapperExtension *p_extension) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());
	OpenXRAPI::get_singleton()->register_composition_layer_provider(p_extension);
}

void OpenXRAPIExtension::unregister_composition_layer_provider(OpenXRExtensionWrapperExtension *p_extension) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());
	OpenXRAPI::get_singleton()->unregister_composition_layer_provider(p_extension);
}

void OpenXRAPIExtension::register_projection_views_extension(OpenXRExtensionWrapperExtension *p_extension) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());
	OpenXRAPI::get_singleton()->register_projection_views_extension(p_extension);
}

void OpenXRAPIExtension::unregister_projection_views_extension(OpenXRExtensionWrapperExtension *p_extension) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());
	OpenXRAPI::get_singleton()->unregister_projection_views_extension(p_extension);
}

double OpenXRAPIExtension::get_render_state_z_near() {
	ERR_NOT_ON_RENDER_THREAD_V(0.0);
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0.0);
	return OpenXRAPI::get_singleton()->get_render_state_z_near();
}

double OpenXRAPIExtension::get_render_state_z_far() {
	ERR_NOT_ON_RENDER_THREAD_V(0.0);
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0.0);
	return OpenXRAPI::get_singleton()->get_render_state_z_far();
}

void OpenXRAPIExtension::set_velocity_texture(RID p_render_target) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());
	OpenXRAPI::get_singleton()->set_velocity_texture(p_render_target);
}

void OpenXRAPIExtension::set_velocity_depth_texture(RID p_render_target) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());
	OpenXRAPI::get_singleton()->set_velocity_depth_texture(p_render_target);
}

void OpenXRAPIExtension::set_velocity_target_size(const Size2i &p_target_size) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());
	OpenXRAPI::get_singleton()->set_velocity_target_size(p_target_size);
}

PackedInt64Array OpenXRAPIExtension::get_supported_swapchain_formats() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), PackedInt64Array());
	return OpenXRAPI::get_singleton()->get_supported_swapchain_formats();
}

uint64_t OpenXRAPIExtension::openxr_swapchain_create(XrSwapchainCreateFlags p_create_flags, XrSwapchainUsageFlags p_usage_flags, int64_t p_swapchain_format, uint32_t p_width, uint32_t p_height, uint32_t p_sample_count, uint32_t p_array_size) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0);

	OpenXRAPI::OpenXRSwapChainInfo *new_swapchain_info = memnew(OpenXRAPI::OpenXRSwapChainInfo);
	new_swapchain_info->create(p_create_flags, p_usage_flags, p_swapchain_format, p_width, p_height, p_sample_count, p_array_size);
	return reinterpret_cast<uint64_t>(new_swapchain_info);
}

void OpenXRAPIExtension::openxr_swapchain_free(uint64_t p_swapchain_info) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());

	OpenXRAPI::OpenXRSwapChainInfo *swapchain_info = reinterpret_cast<OpenXRAPI::OpenXRSwapChainInfo *>(p_swapchain_info);
	swapchain_info->free();
	memfree(swapchain_info);
}

uint64_t OpenXRAPIExtension::openxr_swapchain_get_swapchain(uint64_t p_swapchain_info) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), 0);

	OpenXRAPI::OpenXRSwapChainInfo *swapchain_info = reinterpret_cast<OpenXRAPI::OpenXRSwapChainInfo *>(p_swapchain_info);
	XrSwapchain swapchain = swapchain_info->get_swapchain();
	return reinterpret_cast<uint64_t>(swapchain);
}

void OpenXRAPIExtension::openxr_swapchain_acquire(uint64_t p_swapchain_info) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());

	OpenXRAPI::OpenXRSwapChainInfo *swapchain_info = reinterpret_cast<OpenXRAPI::OpenXRSwapChainInfo *>(p_swapchain_info);
	bool should_render = true; // Can ignore should_render.
	swapchain_info->acquire(should_render);
}

RID OpenXRAPIExtension::openxr_swapchain_get_image(uint64_t p_swapchain_info) {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), RID());

	OpenXRAPI::OpenXRSwapChainInfo *swapchain_info = reinterpret_cast<OpenXRAPI::OpenXRSwapChainInfo *>(p_swapchain_info);
	return swapchain_info->get_image();
}

void OpenXRAPIExtension::openxr_swapchain_release(uint64_t p_swapchain_info) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());

	OpenXRAPI::OpenXRSwapChainInfo *swapchain_info = reinterpret_cast<OpenXRAPI::OpenXRSwapChainInfo *>(p_swapchain_info);
	swapchain_info->release();
}

void OpenXRAPIExtension::set_emulate_environment_blend_mode_alpha_blend(bool p_enabled) {
	ERR_FAIL_NULL(OpenXRAPI::get_singleton());
	OpenXRAPI::get_singleton()->set_emulate_environment_blend_mode_alpha_blend(p_enabled);
}

OpenXRAPIExtension::OpenXRAlphaBlendModeSupport OpenXRAPIExtension::is_environment_blend_mode_alpha_blend_supported() {
	ERR_FAIL_NULL_V(OpenXRAPI::get_singleton(), OPENXR_ALPHA_BLEND_MODE_SUPPORT_NONE);
	return (OpenXRAPIExtension::OpenXRAlphaBlendModeSupport)OpenXRAPI::get_singleton()->is_environment_blend_mode_alpha_blend_supported();
}

OpenXRAPIExtension::OpenXRAPIExtension() {
}
