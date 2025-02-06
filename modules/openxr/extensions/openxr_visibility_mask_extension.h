/**************************************************************************/
/*  openxr_visibility_mask_extension.h                                    */
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

#include "../util.h"

#include "core/templates/vector.h"
#include "openxr_extension_wrapper.h"
#include "scene/resources/mesh.h"

// The OpenXR visibility mask extension provides a mesh for each eye that
// can be used as a mask to determine which part of our rendered result
// is actually visible to the user. Due to lens distortion the edges of
// the rendered image are never used in the final result output on the HMD.
//
// Blacking out this are of the render result can remove a fair amount of
// overhead in rendering part of the screen that is unused.
//
// https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_KHR_visibility_mask

class OpenXRVisibilityMaskExtension : public OpenXRExtensionWrapper {
public:
	static OpenXRVisibilityMaskExtension *get_singleton();

	OpenXRVisibilityMaskExtension();
	virtual ~OpenXRVisibilityMaskExtension() override;

	virtual HashMap<String, bool *> get_requested_extensions() override;

	virtual void on_instance_created(const XrInstance p_instance) override;

	virtual void on_session_created(const XrSession p_instance) override;
	virtual void on_session_destroyed() override;

	virtual void on_pre_render() override;
	virtual bool on_event_polled(const XrEventDataBuffer &event) override;

	bool is_available();
	RID get_mesh();

private:
	static OpenXRVisibilityMaskExtension *singleton;

	bool available = false;
	bool is_dirty = false;

	RID shader;
	RID material;
	RID mesh;

	struct MeshData {
		Vector<XrVector2f> vertices;
		Vector<uint32_t> indices;
	};

	uint32_t mesh_count = 0;
	MeshData mesh_data[4];

	void _update_mesh_data(uint32_t p_view);
	void _update_mesh();

	// OpenXR API call wrappers
	EXT_PROTO_XRRESULT_FUNC5(xrGetVisibilityMaskKHR, (XrSession), session, (XrViewConfigurationType), viewConfigurationType, (uint32_t), viewIndex, (XrVisibilityMaskTypeKHR), visibilityMaskType, (XrVisibilityMaskKHR *), visibilityMask);
};
