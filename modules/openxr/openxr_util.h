/**************************************************************************/
/*  openxr_util.h                                                         */
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

#include "core/string/ustring.h"

#include <openxr/openxr.h>

class OpenXRUtil {
public:
	static String get_view_configuration_name(XrViewConfigurationType p_view_configuration);
	static String get_reference_space_name(XrReferenceSpaceType p_reference_space);
	static String get_structure_type_name(XrStructureType p_structure_type);
	static String get_session_state_name(XrSessionState p_session_state);
	static String get_action_type_name(XrActionType p_action_type);
	static String get_environment_blend_mode_name(XrEnvironmentBlendMode p_blend_mode);
	static String make_xr_version_string(XrVersion p_version);

	// Copied from OpenXR xr_linear.h private header, so we can still link against
	// system-provided packages without relying on our `thirdparty` code.

	// Column-major, pre-multiplied. This type does not exist in the OpenXR API and is provided for convenience.
	typedef struct XrMatrix4x4f {
		float m[16];
	} XrMatrix4x4f;

	typedef enum GraphicsAPI {
		GRAPHICS_VULKAN,
		GRAPHICS_OPENGL,
		GRAPHICS_OPENGL_ES,
		GRAPHICS_D3D
	} GraphicsAPI;

	static void XrMatrix4x4f_CreateProjection(XrMatrix4x4f *result, GraphicsAPI graphicsApi, const float tanAngleLeft,
			const float tanAngleRight, const float tanAngleUp, float const tanAngleDown,
			const float nearZ, const float farZ);
	static void XrMatrix4x4f_CreateProjectionFov(XrMatrix4x4f *result, GraphicsAPI graphicsApi, const XrFovf fov,
			const float nearZ, const float farZ);
};
