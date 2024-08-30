/**************************************************************************/
/*  openxr_visibility_mask.cpp                                            */
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

#include "openxr_visibility_mask.h"

#include "../extensions/openxr_visibility_mask_extension.h"
#include "../openxr_interface.h"
#include "scene/3d/xr_nodes.h"

void OpenXRVisibilityMask::_bind_methods() {
}

void OpenXRVisibilityMask::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			OpenXRVisibilityMaskExtension *vis_mask_ext = OpenXRVisibilityMaskExtension::get_singleton();
			if (vis_mask_ext && vis_mask_ext->is_available()) {
				set_base(vis_mask_ext->get_mesh());
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			set_base(RID());
		} break;
	}
}

void OpenXRVisibilityMask::_on_openxr_session_begun() {
	if (is_inside_tree()) {
		OpenXRVisibilityMaskExtension *vis_mask_ext = OpenXRVisibilityMaskExtension::get_singleton();
		if (vis_mask_ext && vis_mask_ext->is_available()) {
			set_base(vis_mask_ext->get_mesh());
		}
	}
}

void OpenXRVisibilityMask::_on_openxr_session_stopping() {
	set_base(RID());
}

#ifdef TOOLS_ENABLED
Vector<ConfigurationInfo> OpenXRVisibilityMask::get_configuration_info() const {
	Vector<ConfigurationInfo> infos = VisualInstance3D::get_configuration_info();

	if (is_visible() && is_inside_tree()) {
		XRCamera3D *camera = Object::cast_to<XRCamera3D>(get_parent());
		if (camera == nullptr) {
			CONFIG_WARNING(RTR("OpenXR visibility mask must have an XRCamera3D node as their parent."));
		}
	}

	return infos;
}
#endif

AABB OpenXRVisibilityMask::get_aabb() const {
	AABB ret;

	// Make sure it's always visible, this is positioned through its shader.
	ret.position = Vector3(-1000.0, -1000.0, -1000.0);
	ret.size = Vector3(2000.0, 2000.0, 2000.0);

	return ret;
}

OpenXRVisibilityMask::OpenXRVisibilityMask() {
	Ref<OpenXRInterface> openxr_interface = XRServer::get_singleton()->find_interface("OpenXR");
	if (openxr_interface.is_valid()) {
		openxr_interface->connect("session_begun", callable_mp(this, &OpenXRVisibilityMask::_on_openxr_session_begun));
		openxr_interface->connect("session_stopping", callable_mp(this, &OpenXRVisibilityMask::_on_openxr_session_stopping));
	}

	RS::get_singleton()->instance_geometry_set_cast_shadows_setting(get_instance(), RS::SHADOW_CASTING_SETTING_OFF);
}

OpenXRVisibilityMask::~OpenXRVisibilityMask() {
	Ref<OpenXRInterface> openxr_interface = XRServer::get_singleton()->find_interface("OpenXR");
	if (openxr_interface.is_valid()) {
		openxr_interface->disconnect("session_begun", callable_mp(this, &OpenXRVisibilityMask::_on_openxr_session_begun));
		openxr_interface->disconnect("session_stopping", callable_mp(this, &OpenXRVisibilityMask::_on_openxr_session_stopping));
	}
}
