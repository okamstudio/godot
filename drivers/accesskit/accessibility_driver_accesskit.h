/**************************************************************************/
/*  accessibility_driver_accesskit.h                                      */
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

#ifndef ACCESSIBILITY_DRIVER_ACCESSKIT_H
#define ACCESSIBILITY_DRIVER_ACCESSKIT_H

#ifdef ACCESSKIT_ENABLED

#include "core/templates/rid_owner.h"
#include "servers/display_server.h"

#ifdef ACCESSKIT_DYNAMIC
#ifdef LINUXBSD_ENABLED
#include "drivers/accesskit/dynwrappers/accesskit-so_wrap.h"
#endif
#ifdef MACOS_ENABLED
#include "drivers/accesskit/dynwrappers/accesskit-dylib_wrap.h"
#endif
#ifdef WINDOWS_ENABLED
#include "drivers/accesskit/dynwrappers/accesskit-dll_wrap.h"
#endif
#else
#include <accesskit.h>
#endif

class AccessibilityDriverAccessKit : public AccessibilityDriver {
	static AccessibilityDriverAccessKit *singleton;

	struct AccessibilityElement {
		HashMap<accesskit_action, Callable> actions;

		DisplayServer::WindowID window_id = DisplayServer::INVALID_WINDOW_ID;
		RID parent;
		LocalVector<RID> children;
		Vector3i run;
		Variant meta;
		String name;
		String name_extra_info;

		accesskit_role role = ACCESSKIT_ROLE_UNKNOWN;
		accesskit_node *node = nullptr;
	};
	mutable RID_PtrOwner<AccessibilityElement> rid_owner;

	struct WindowData {
		// Adapter.
#ifdef WINDOWS_ENABLED
		accesskit_windows_subclassing_adapter *adapter = nullptr;
#endif
#ifdef MACOS_ENABLED
		accesskit_macos_subclassing_adapter *adapter = nullptr;
#endif
#ifdef LINUXBSD_ENABLED
		accesskit_unix_adapter *adapter = nullptr;
#endif

		RID root_id;
		HashSet<RID> update;
	};

	RID focus;

	HashMap<DisplayServer::WindowID, WindowData> windows;

	HashMap<DisplayServer::AccessibilityRole, accesskit_role> role_map;
	HashMap<DisplayServer::AccessibilityAction, accesskit_action> action_map;

	_FORCE_INLINE_ accesskit_role _accessibility_role(DisplayServer::AccessibilityRole p_role) const;
	_FORCE_INLINE_ accesskit_action _accessibility_action(DisplayServer::AccessibilityAction p_action) const;

	void _free_recursive(WindowData *p_wd, const RID &p_id);
	_FORCE_INLINE_ void _ensure_node(const RID &p_id, AccessibilityElement *p_ae);

	static void _accessibility_action_callback(struct accesskit_action_request *p_request, void *p_user_data);
	static accesskit_tree_update *_accessibility_initial_tree_update_callback(void *p_user_data);
	static void _accessibility_deactivation_callback(void *p_user_data);
	static accesskit_tree_update *_accessibility_build_tree_update(void *p_user_data);

	bool in_accessibility_update = false;
	Callable update_cb;

public:
	Error init() override;

	bool window_create(DisplayServer::WindowID p_window_id, void *p_handle) override;
	void window_destroy(DisplayServer::WindowID p_window_id) override;

	RID accessibility_create_element(DisplayServer::WindowID p_window_id, DisplayServer::AccessibilityRole p_role) override;
	RID accessibility_create_sub_element(const RID &p_parent_rid, DisplayServer::AccessibilityRole p_role, int p_insert_pos = -1) override;
	virtual RID accessibility_create_sub_text_edit_elements(const RID &p_parent_rid, const RID &p_shaped_text, float p_min_height, int p_insert_pos = -1) override;
	bool accessibility_has_element(const RID &p_id) const override;
	void accessibility_free_element(const RID &p_id) override;

	void accessibility_element_set_meta(const RID &p_id, const Variant &p_meta) override;
	Variant accessibility_element_get_meta(const RID &p_id) const override;

	void accessibility_update_if_active(const Callable &p_callable) override;

	void accessibility_update_set_focus(const RID &p_id) override;
	RID accessibility_get_window_root(DisplayServer::WindowID p_window_id) const override;

	void accessibility_set_window_rect(DisplayServer::WindowID p_window_id, const Rect2 &p_rect_out, const Rect2 &p_rect_in) override;
	void accessibility_set_window_focused(DisplayServer::WindowID p_window_id, bool p_focused) override;

	void accessibility_update_set_role(const RID &p_id, DisplayServer::AccessibilityRole p_role) override;
	void accessibility_update_set_name(const RID &p_id, const String &p_name) override;
	void accessibility_update_set_extra_info(const RID &p_id, const String &p_name_extra_info) override;
	void accessibility_update_set_description(const RID &p_id, const String &p_description) override;
	void accessibility_update_set_value(const RID &p_id, const String &p_value) override;
	void accessibility_update_set_tooltip(const RID &p_id, const String &p_tooltip) override;
	void accessibility_update_set_bounds(const RID &p_id, const Rect2 &p_rect) override;
	void accessibility_update_set_transform(const RID &p_id, const Transform2D &p_transform) override;
	void accessibility_update_add_child(const RID &p_id, const RID &p_child_id) override;
	void accessibility_update_add_related_controls(const RID &p_id, const RID &p_related_id) override;
	void accessibility_update_add_related_details(const RID &p_id, const RID &p_related_id) override;
	void accessibility_update_add_related_described_by(const RID &p_id, const RID &p_related_id) override;
	void accessibility_update_add_related_flow_to(const RID &p_id, const RID &p_related_id) override;
	void accessibility_update_add_related_labeled_by(const RID &p_id, const RID &p_related_id) override;
	void accessibility_update_add_related_radio_group(const RID &p_id, const RID &p_related_id) override;
	void accessibility_update_set_active_descendant(const RID &p_id, const RID &p_other_id) override;
	void accessibility_update_set_next_on_line(const RID &p_id, const RID &p_other_id) override;
	void accessibility_update_set_previous_on_line(const RID &p_id, const RID &p_other_id) override;
	void accessibility_update_set_member_of(const RID &p_id, const RID &p_other_id) override;
	void accessibility_update_set_in_page_link_target(const RID &p_id, const RID &p_other_id) override;
	void accessibility_update_set_error_message(const RID &p_id, const RID &p_other_id) override;
	void accessibility_update_set_live(const RID &p_id, DisplayServer::AccessibilityLiveMode p_live) override;
	void accessibility_update_add_action(const RID &p_id, DisplayServer::AccessibilityAction p_action, const Callable &p_callable) override;
	void accessibility_update_add_custom_action(const RID &p_id, int p_action_id, const String &p_action_description) override;
	void accessibility_update_set_table_row_count(const RID &p_id, int p_count) override;
	void accessibility_update_set_table_column_count(const RID &p_id, int p_count) override;
	void accessibility_update_set_table_row_index(const RID &p_id, int p_index) override;
	void accessibility_update_set_table_column_index(const RID &p_id, int p_index) override;
	void accessibility_update_set_table_cell_position(const RID &p_id, int p_row_index, int p_column_index) override;
	void accessibility_update_set_table_cell_span(const RID &p_id, int p_row_span, int p_column_span) override;
	void accessibility_update_set_list_item_count(const RID &p_id, int p_size) override;
	void accessibility_update_set_list_item_index(const RID &p_id, int p_index) override;
	void accessibility_update_set_list_item_level(const RID &p_id, int p_level) override;
	void accessibility_update_set_list_item_selected(const RID &p_id, bool p_selected) override;
	void accessibility_update_set_list_item_expanded(const RID &p_id, bool p_expanded) override;
	void accessibility_update_set_popup_type(const RID &p_id, DisplayServer::AccessibilityPopupType p_popup) override;
	void accessibility_update_set_checked(const RID &p_id, bool p_checekd) override;
	void accessibility_update_set_num_value(const RID &p_id, double p_position) override;
	void accessibility_update_set_num_range(const RID &p_id, double p_min, double p_max) override;
	void accessibility_update_set_num_step(const RID &p_id, double p_step) override;
	void accessibility_update_set_num_jump(const RID &p_id, double p_jump) override;
	void accessibility_update_set_scroll_x(const RID &p_id, double p_position) override;
	void accessibility_update_set_scroll_x_range(const RID &p_id, double p_min, double p_max) override;
	void accessibility_update_set_scroll_y(const RID &p_id, double p_position) override;
	void accessibility_update_set_scroll_y_range(const RID &p_id, double p_min, double p_max) override;
	void accessibility_update_set_text_decorations(const RID &p_id, bool p_underline, bool p_strikethrough, bool p_overline) override;
	void accessibility_update_set_text_align(const RID &p_id, HorizontalAlignment p_align) override;
	void accessibility_update_set_text_selection(const RID &p_id, const RID &p_text_start_id, int p_start_char, const RID &p_text_end_id, int p_end_char) override;
	void accessibility_update_set_flag(const RID &p_id, DisplayServer::AccessibilityFlags p_flag, bool p_value) override;
	void accessibility_update_set_classname(const RID &p_id, const String &p_classname) override;
	void accessibility_update_set_placeholder(const RID &p_id, const String &p_placeholder) override;
	void accessibility_update_set_language(const RID &p_id, const String &p_language) override;
	void accessibility_update_set_text_orientation(const RID &p_id, bool p_vertical) override;
	void accessibility_update_set_list_orientation(const RID &p_id, bool p_vertical) override;
	void accessibility_update_set_shortcut(const RID &p_id, const String &p_shortcut) override;
	void accessibility_update_set_url(const RID &p_id, const String &p_url) override;
	void accessibility_update_set_role_description(const RID &p_id, const String &p_description) override;
	void accessibility_update_set_state_description(const RID &p_id, const String &p_description) override;
	void accessibility_update_set_color_value(const RID &p_id, const Color &p_color) override;
	void accessibility_update_set_background_color(const RID &p_id, const Color &p_color) override;
	void accessibility_update_set_foreground_color(const RID &p_id, const Color &p_color) override;

	AccessibilityDriverAccessKit();
	~AccessibilityDriverAccessKit();
};

#endif // ACCESSKIT_ENABLED

#endif // ACCESSIBILITY_DRIVER_ACCESSKIT_H
