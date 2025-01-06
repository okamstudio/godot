/**************************************************************************/
/*  openxr_util.cpp                                                       */
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

#include "openxr_util.h"

#include "core/math/math_funcs.h"

#include <openxr/openxr_reflection.h>

#define XR_ENUM_CASE_STR(name, val) \
	case name:                      \
		return #name;
#define XR_ENUM_SWITCH(enumType, var)                                                                                           \
	switch (var) {                                                                                                              \
		XR_LIST_ENUM_##enumType(XR_ENUM_CASE_STR) default : return "Unknown " #enumType ": " + String::num_int64(int64_t(var)); \
	}

String OpenXRUtil::get_view_configuration_name(XrViewConfigurationType p_view_configuration){
	XR_ENUM_SWITCH(XrViewConfigurationType, p_view_configuration)
}

String OpenXRUtil::get_reference_space_name(XrReferenceSpaceType p_reference_space){
	XR_ENUM_SWITCH(XrReferenceSpaceType, p_reference_space)
}

String OpenXRUtil::get_structure_type_name(XrStructureType p_structure_type){
	XR_ENUM_SWITCH(XrStructureType, p_structure_type)
}

String OpenXRUtil::get_session_state_name(XrSessionState p_session_state){
	XR_ENUM_SWITCH(XrSessionState, p_session_state)
}

String OpenXRUtil::get_action_type_name(XrActionType p_action_type){
	XR_ENUM_SWITCH(XrActionType, p_action_type)
}

String OpenXRUtil::get_environment_blend_mode_name(XrEnvironmentBlendMode p_blend_mode) {
	XR_ENUM_SWITCH(XrEnvironmentBlendMode, p_blend_mode);
}

String OpenXRUtil::make_xr_version_string(XrVersion p_version) {
	String version;

	version += String::num_int64(XR_VERSION_MAJOR(p_version));
	version += String(".");
	version += String::num_int64(XR_VERSION_MINOR(p_version));
	version += String(".");
	version += String::num_int64(XR_VERSION_PATCH(p_version));

	return version;
}

// Copied from OpenXR xr_linear.h private header, so we can still link against
// system-provided packages without relying on our `thirdparty` code.

// Copyright (c) 2017 The Khronos Group Inc.
// Copyright (c) 2016 Oculus VR, LLC.
//
// SPDX-License-Identifier: Apache-2.0

// Creates a projection matrix based on the specified dimensions.
// The projection matrix transforms -Z=forward, +Y=up, +X=right to the appropriate clip space for the graphics API.
// The far plane is placed at infinity if farZ <= nearZ.
// An infinite projection matrix is preferred for rasterization because, except for
// things *right* up against the near plane, it always provides better precision:
//              "Tightening the Precision of Perspective Rendering"
//              Paul Upchurch, Mathieu Desbrun
//              Journal of Graphics Tools, Volume 16, Issue 1, 2012
void OpenXRUtil::XrMatrix4x4f_CreateProjection(XrMatrix4x4f *result, GraphicsAPI graphicsApi, const float tanAngleLeft,
		const float tanAngleRight, const float tanAngleUp, float const tanAngleDown,
		const float nearZ, const float farZ) {
	const float tanAngleWidth = tanAngleRight - tanAngleLeft;

	// Set to tanAngleDown - tanAngleUp for a clip space with positive Y down (Vulkan).
	// Set to tanAngleUp - tanAngleDown for a clip space with positive Y up (OpenGL / D3D / Metal).
	const float tanAngleHeight = graphicsApi == GRAPHICS_VULKAN ? (tanAngleDown - tanAngleUp) : (tanAngleUp - tanAngleDown);

	// Set to nearZ for a [-1,1] Z clip space (OpenGL / OpenGL ES).
	// Set to zero for a [0,1] Z clip space (Vulkan / D3D / Metal).
	const float offsetZ = (graphicsApi == GRAPHICS_OPENGL || graphicsApi == GRAPHICS_OPENGL_ES) ? nearZ : 0;

	if (farZ <= nearZ) {
		// place the far plane at infinity
		result->m[0] = 2.0f / tanAngleWidth;
		result->m[4] = 0.0f;
		result->m[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
		result->m[12] = 0.0f;

		result->m[1] = 0.0f;
		result->m[5] = 2.0f / tanAngleHeight;
		result->m[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
		result->m[13] = 0.0f;

		result->m[2] = 0.0f;
		result->m[6] = 0.0f;
		result->m[10] = -1.0f;
		result->m[14] = -(nearZ + offsetZ);

		result->m[3] = 0.0f;
		result->m[7] = 0.0f;
		result->m[11] = -1.0f;
		result->m[15] = 0.0f;
	} else {
		// normal projection
		result->m[0] = 2.0f / tanAngleWidth;
		result->m[4] = 0.0f;
		result->m[8] = (tanAngleRight + tanAngleLeft) / tanAngleWidth;
		result->m[12] = 0.0f;

		result->m[1] = 0.0f;
		result->m[5] = 2.0f / tanAngleHeight;
		result->m[9] = (tanAngleUp + tanAngleDown) / tanAngleHeight;
		result->m[13] = 0.0f;

		result->m[2] = 0.0f;
		result->m[6] = 0.0f;
		result->m[10] = -(farZ + offsetZ) / (farZ - nearZ);
		result->m[14] = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

		result->m[3] = 0.0f;
		result->m[7] = 0.0f;
		result->m[11] = -1.0f;
		result->m[15] = 0.0f;
	}
}

// Creates a projection matrix based on the specified FOV.
void OpenXRUtil::XrMatrix4x4f_CreateProjectionFov(XrMatrix4x4f *result, GraphicsAPI graphicsApi, const XrFovf fov,
		const float nearZ, const float farZ) {
	const float tanLeft = Math::tan(fov.angleLeft);
	const float tanRight = Math::tan(fov.angleRight);

	const float tanDown = Math::tan(fov.angleDown);
	const float tanUp = Math::tan(fov.angleUp);

	XrMatrix4x4f_CreateProjection(result, graphicsApi, tanLeft, tanRight, tanUp, tanDown, nearZ, farZ);
}
