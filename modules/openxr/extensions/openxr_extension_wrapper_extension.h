/**************************************************************************/
/*  openxr_extension_wrapper_extension.h                                  */
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

#ifndef OPENXR_EXTENSION_WRAPPER_EXTENSION_H
#define OPENXR_EXTENSION_WRAPPER_EXTENSION_H

#include "../openxr_api_extension.h"
#include "openxr_extension_wrapper.h"

#include "core/object/ref_counted.h"
#include "core/os/os.h"
#include "core/os/thread_safe.h"
#include "core/variant/native_ptr.h"
#include "core/variant/typed_array.h"

class OpenXRExtensionWrapperExtension : public Object, public OpenXRExtensionWrapper, public OpenXRCompositionLayerProvider {
	GDCLASS(OpenXRExtensionWrapperExtension, Object);

protected:
	_THREAD_SAFE_CLASS_

	static void _bind_methods();

	Ref<OpenXRAPIExtension> openxr_api;

public:
	virtual HashMap<String, bool *> get_requested_extensions() override;

	GDVIRTUAL0R(Dictionary, _get_requested_extensions);

	virtual void *set_system_properties_and_get_next_pointer(void *p_next_pointer) override;
	virtual void *set_instance_create_info_and_get_next_pointer(void *p_next_pointer) override;
	virtual void *set_session_create_and_get_next_pointer(void *p_next_pointer) override;
	virtual void *set_swapchain_create_info_and_get_next_pointer(void *p_next_pointer) override;
	virtual void *set_hand_joint_locations_and_get_next_pointer(int p_hand_index, void *p_next_pointer) override;
	virtual void *set_projection_views_and_get_next_pointer(int p_view_index, void *p_next_pointer) override;

	virtual int get_composition_layer_count() override;
	virtual XrCompositionLayerBaseHeader *get_composition_layer(int p_index) override;
	virtual int get_composition_layer_order(int p_index) override;

	//TODO workaround as GDExtensionPtr<void> return type results in build error in godot-cpp
	GDVIRTUAL1R(uint64_t, _set_system_properties_and_get_next_pointer, GDExtensionPtr<void>);
	GDVIRTUAL1R(uint64_t, _set_instance_create_info_and_get_next_pointer, GDExtensionPtr<void>);
	GDVIRTUAL1R(uint64_t, _set_session_create_and_get_next_pointer, GDExtensionPtr<void>);
	GDVIRTUAL1R(uint64_t, _set_swapchain_create_info_and_get_next_pointer, GDExtensionPtr<void>);
	GDVIRTUAL2R(uint64_t, _set_hand_joint_locations_and_get_next_pointer, int, GDExtensionPtr<void>);
	GDVIRTUAL2R(uint64_t, _set_projection_views_and_get_next_pointer, int, GDExtensionPtr<void>);
	GDVIRTUAL0R(int, _get_composition_layer_count);
	GDVIRTUAL1R(uint64_t, _get_composition_layer, int);
	GDVIRTUAL1R(int, _get_composition_layer_order, int);

	virtual PackedStringArray get_suggested_tracker_names() override;

	GDVIRTUAL0R(PackedStringArray, _get_suggested_tracker_names);

	virtual void on_register_metadata() override;
	virtual void on_before_instance_created() override;
	virtual void on_instance_created(const XrInstance p_instance) override;
	virtual void on_instance_destroyed() override;
	virtual void on_session_created(const XrSession p_session) override;
	virtual void on_process() override;
	virtual void on_pre_render() override;
	virtual void on_main_swapchains_created() override;
	virtual void on_session_destroyed() override;
	virtual void on_pre_draw_viewport(RID p_render_target) override;
	virtual void on_post_draw_viewport(RID p_render_target) override;

	GDVIRTUAL0(_on_register_metadata);
	GDVIRTUAL0(_on_before_instance_created);
	GDVIRTUAL1(_on_instance_created, uint64_t);
	GDVIRTUAL0(_on_instance_destroyed);
	GDVIRTUAL1(_on_session_created, uint64_t);
	GDVIRTUAL0(_on_process);
	GDVIRTUAL0(_on_pre_render);
	GDVIRTUAL0(_on_main_swapchains_created);
	GDVIRTUAL0(_on_session_destroyed);
	GDVIRTUAL1(_on_pre_draw_viewport, RID);
	GDVIRTUAL1(_on_post_draw_viewport, RID);

	virtual void on_state_idle() override;
	virtual void on_state_ready() override;
	virtual void on_state_synchronized() override;
	virtual void on_state_visible() override;
	virtual void on_state_focused() override;
	virtual void on_state_stopping() override;
	virtual void on_state_loss_pending() override;
	virtual void on_state_exiting() override;

	GDVIRTUAL0(_on_state_idle);
	GDVIRTUAL0(_on_state_ready);
	GDVIRTUAL0(_on_state_synchronized);
	GDVIRTUAL0(_on_state_visible);
	GDVIRTUAL0(_on_state_focused);
	GDVIRTUAL0(_on_state_stopping);
	GDVIRTUAL0(_on_state_loss_pending);
	GDVIRTUAL0(_on_state_exiting);

	virtual bool on_event_polled(const XrEventDataBuffer &p_event) override;

	GDVIRTUAL1R(bool, _on_event_polled, GDExtensionConstPtr<void>);

	virtual void *set_viewport_composition_layer_and_get_next_pointer(const XrCompositionLayerBaseHeader *p_layer, const Dictionary &p_property_values, void *p_next_pointer) override;
	virtual void on_viewport_composition_layer_destroyed(const XrCompositionLayerBaseHeader *p_layer) override;
	virtual void get_viewport_composition_layer_extension_properties(List<PropertyInfo> *p_property_list) override;
	virtual Dictionary get_viewport_composition_layer_extension_property_defaults() override;
	virtual void *set_android_surface_swapchain_create_info_and_get_next_pointer(const Dictionary &p_property_values, void *p_next_pointer) override;

	GDVIRTUAL3R(uint64_t, _set_viewport_composition_layer_and_get_next_pointer, GDExtensionConstPtr<void>, Dictionary, GDExtensionPtr<void>);
	GDVIRTUAL1(_on_viewport_composition_layer_destroyed, GDExtensionConstPtr<void>);
	GDVIRTUAL0R(TypedArray<Dictionary>, _get_viewport_composition_layer_extension_properties);
	GDVIRTUAL0R(Dictionary, _get_viewport_composition_layer_extension_property_defaults);
	GDVIRTUAL2R(uint64_t, _set_android_surface_swapchain_create_info_and_get_next_pointer, Dictionary, GDExtensionPtr<void>);

	Ref<OpenXRAPIExtension> get_openxr_api();

	void register_extension_wrapper();

	OpenXRExtensionWrapperExtension();
	virtual ~OpenXRExtensionWrapperExtension() override;
};

#endif // OPENXR_EXTENSION_WRAPPER_EXTENSION_H
