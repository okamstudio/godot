/**************************************************************************/
/*  openxr_visibility_mask_extension.cpp                                  */
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

#include "openxr_visibility_mask_extension.h"

#include "../openxr_api.h"
#include "core/string/print_string.h"
#include "core/variant/array.h"
#include "core/variant/variant.h"
#include "servers/rendering_server.h"

static const char *VISIBILITY_MASK_SHADER_CODE =
		"shader_type spatial;\n"
		"render_mode unshaded, shadows_disabled, cull_disabled;\n"
		"void vertex() {\n"
		"\tif (int(VERTEX.z) == VIEW_INDEX) {\n"
		"\t\tVERTEX.z = -1.0;\n"
		"\t\tVERTEX += EYE_OFFSET;\n"
		"\t\tPOSITION = PROJECTION_MATRIX * vec4(VERTEX, 1.0);\n"
		"\t\tPOSITION.xy /= POSITION.w;\n"
		"\t\tPOSITION.z = 1.0;\n"
		"\t\tPOSITION.w = 1.0;\n"
		"\t} else {\n"
		"\t\tPOSITION = vec4(2.0, 2.0, 2.0, 1.0);\n"
		"\t}\n"
		"}\n"
		"void fragment() {\n"
		"\tALBEDO = vec3(0.0, 0.0, 0.0);\n"
		"}\n";

OpenXRVisibilityMaskExtension *OpenXRVisibilityMaskExtension::singleton = nullptr;

OpenXRVisibilityMaskExtension *OpenXRVisibilityMaskExtension::get_singleton() {
	return singleton;
}

OpenXRVisibilityMaskExtension::OpenXRVisibilityMaskExtension() {
	singleton = this;
}

OpenXRVisibilityMaskExtension::~OpenXRVisibilityMaskExtension() {
	singleton = nullptr;
}

HashMap<String, bool *> OpenXRVisibilityMaskExtension::get_requested_extensions() {
	HashMap<String, bool *> request_extensions;

	request_extensions[XR_KHR_VISIBILITY_MASK_EXTENSION_NAME] = &available;

	return request_extensions;
}

void OpenXRVisibilityMaskExtension::on_instance_created(const XrInstance p_instance) {
	if (available) {
		EXT_INIT_XR_FUNC(xrGetVisibilityMaskKHR);
	}
}

void OpenXRVisibilityMaskExtension::on_session_created(const XrSession p_instance) {
	if (available) {
		RS *rendering_server = RS::get_singleton();
		ERR_FAIL_NULL(rendering_server);

		OpenXRAPI *openxr_api = (OpenXRAPI *)OpenXRAPI::get_singleton();
		ERR_FAIL_NULL(openxr_api);

		// Create our shader.
		shader = rendering_server->shader_create();
		rendering_server->shader_set_code(shader, VISIBILITY_MASK_SHADER_CODE);

		// Create our material.
		material = rendering_server->material_create();
		rendering_server->material_set_shader(material, shader);
		rendering_server->material_set_render_priority(material, 99);

		// Create our mesh.
		mesh = rendering_server->mesh_create();

		// Get our initial mesh data.
		mesh_count = openxr_api->get_view_count(); // We need a mesh for each view.
		for (uint32_t i = 0; i < mesh_count; i++) {
			_update_mesh_data(i);
		}

		// And update our mesh
		_update_mesh();
	}
}

void OpenXRVisibilityMaskExtension::on_session_destroyed() {
	RS *rendering_server = RS::get_singleton();
	ERR_FAIL_NULL(rendering_server);

	// Free our mesh.
	if (mesh.is_valid()) {
		rendering_server->free(mesh);
		mesh = RID();
	}

	// Free our material.
	if (material.is_valid()) {
		rendering_server->free(material);
		material = RID();
	}

	// Free our shader.
	if (shader.is_valid()) {
		rendering_server->free(shader);
		shader = RID();
	}

	mesh_count = 0;
}

void OpenXRVisibilityMaskExtension::on_pre_render() {
	// Update mesh data if its dirty.
	// Here we call this from the rendering thread however as we're going through the rendering server this is safe.
	_update_mesh();
}

bool OpenXRVisibilityMaskExtension::on_event_polled(const XrEventDataBuffer &event) {
	if (event.type == XR_TYPE_EVENT_DATA_VISIBILITY_MASK_CHANGED_KHR) {
		XrEventDataVisibilityMaskChangedKHR *vismask_event = (XrEventDataVisibilityMaskChangedKHR *)&event;

		PRINT_VERBOSE("OpenXR EVENT: Visibility mask changed for view " + String::num_uint64(vismask_event->viewIndex));

		if (available) { // This event won't be called if this extension is not available but better safe than sorry.
			_update_mesh_data(vismask_event->viewIndex);
		}

		return true;
	}

	return false;
}

bool OpenXRVisibilityMaskExtension::is_available() {
	return available;
}

RID OpenXRVisibilityMaskExtension::get_mesh() {
	return mesh;
}

void OpenXRVisibilityMaskExtension::_update_mesh_data(uint32_t p_view) {
	if (available) {
		ERR_FAIL_UNSIGNED_INDEX(p_view, 4);

		OpenXRAPI *openxr_api = OpenXRAPI::get_singleton();
		ERR_FAIL_NULL(openxr_api);

		XrSession session = openxr_api->get_session();
		XrViewConfigurationType view_configuration_type = openxr_api->get_view_configuration();

		// Figure out how much data we're getting.
		XrVisibilityMaskKHR visibility_mask_data = {
			XR_TYPE_VISIBILITY_MASK_KHR,
			nullptr,
			0,
			0,
			nullptr,
			0,
			0,
			nullptr,
		};

		XrResult result = xrGetVisibilityMaskKHR(session, view_configuration_type, p_view, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, &visibility_mask_data);
		if (XR_FAILED(result)) {
			print_line("OpenXR: Unable to obtain visibility mask metrics [", openxr_api->get_error_string(result), "]");
			return;
		}

		// Resize buffers
		mesh_data[p_view].vertices.resize(visibility_mask_data.vertexCountOutput);
		mesh_data[p_view].indices.resize(visibility_mask_data.indexCountOutput);

		visibility_mask_data.vertexCapacityInput = visibility_mask_data.vertexCountOutput;
		visibility_mask_data.vertices = mesh_data[p_view].vertices.ptrw();
		visibility_mask_data.indexCapacityInput = visibility_mask_data.indexCountOutput;
		visibility_mask_data.indices = mesh_data[p_view].indices.ptrw();

		result = xrGetVisibilityMaskKHR(session, view_configuration_type, p_view, XR_VISIBILITY_MASK_TYPE_HIDDEN_TRIANGLE_MESH_KHR, &visibility_mask_data);
		if (XR_FAILED(result)) {
			print_line("OpenXR: Unable to obtain visibility mask data [", openxr_api->get_error_string(result), "]");
			return;
		}

		// Mark as dirty, we have updated mesh data.
		is_dirty = true;
	}
}

void OpenXRVisibilityMaskExtension::_update_mesh() {
	if (available && is_dirty && mesh_count > 0) {
		RS *rendering_server = RS::get_singleton();
		ERR_FAIL_NULL(rendering_server);

		OpenXRAPI *openxr_api = (OpenXRAPI *)OpenXRAPI::get_singleton();
		ERR_FAIL_NULL(openxr_api);

		// Combine all vertex and index buffers into one.
		PackedVector3Array vertices;
		PackedInt32Array indices;

		uint64_t vertice_count = 0;
		uint64_t index_count = 0;

		for (uint32_t i = 0; i < mesh_count; i++) {
			vertice_count += mesh_data[i].vertices.size();
			index_count += mesh_data[i].indices.size();
		}

		vertices.resize(vertice_count);
		indices.resize(index_count);
		uint64_t offset = 0;

		Vector3 *v_out = vertices.ptrw();
		int32_t *i_out = indices.ptrw();

		for (uint32_t i = 0; i < mesh_count; i++) {
			const XrVector2f *v_in = mesh_data[i].vertices.ptr();
			for (uint32_t j = 0; j < mesh_data[i].vertices.size(); j++) {
				v_out->x = v_in->x;
				v_out->y = v_in->y;
				v_out->z = float(i); // We store our view in our Z component, our shader will filter the right faces out.
				v_out++;
				v_in++;
			}
			const uint32_t *i_in = mesh_data[i].indices.ptr();
			for (uint32_t j = 0; j < mesh_data[i].indices.size(); j++) {
				*i_out = offset + *i_in;
				i_out++;
				i_in++;
			}

			offset += mesh_data[i].vertices.size();
		}

		// Update our mesh.
		Array arr;
		arr.resize(RS::ARRAY_MAX);
		arr[RS::ARRAY_VERTEX] = vertices;
		arr[RS::ARRAY_INDEX] = indices;

		rendering_server->mesh_clear(mesh);
		rendering_server->mesh_add_surface_from_arrays(mesh, RS::PRIMITIVE_TRIANGLES, arr);
		rendering_server->mesh_surface_set_material(mesh, 0, material);

		// Set no longer dirty.
		is_dirty = false;
	}
}
