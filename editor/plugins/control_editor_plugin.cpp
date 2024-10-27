/**************************************************************************/
/*  control_editor_plugin.cpp                                             */
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

#include "control_editor_plugin.h"

#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_button.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/separator.h"

// Inspector controls.

void ControlPositioningWarning::_update_warning() {
	if (!control_node) {
		title_icon->set_texture(nullptr);
		title_label->set_text("");
		hint_label->set_text("");
		return;
	}

	Node *parent_node = control_node->get_parent_control();
	if (!parent_node) {
		title_icon->set_texture(get_editor_theme_icon(SNAME("SubViewport")));
		title_label->set_text(TTR("This node doesn't have a control parent."));
		hint_label->set_text(TTR("Use the appropriate layout properties depending on where you are going to put it."));
	} else if (Object::cast_to<Container>(parent_node)) {
		title_icon->set_texture(get_editor_theme_icon(SNAME("ContainerLayout")));
		title_label->set_text(TTR("This node is a child of a container."));
		hint_label->set_text(TTR("Use container properties for positioning."));
	} else {
		title_icon->set_texture(get_editor_theme_icon(SNAME("ControlLayout")));
		title_label->set_text(TTR("This node is a child of a regular control."));
		hint_label->set_text(TTR("Use anchors and the rectangle for positioning."));
	}

	bg_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("bg_group_note"), SNAME("EditorProperty")));
}

void ControlPositioningWarning::_update_toggler() {
	Ref<Texture2D> arrow;
	if (hint_label->is_visible()) {
		arrow = get_theme_icon(SNAME("arrow"), SNAME("Tree"));
		set_tooltip_text(TTR("Collapse positioning hint."));
	} else {
		if (is_layout_rtl()) {
			arrow = get_theme_icon(SNAME("arrow_collapsed"), SNAME("Tree"));
		} else {
			arrow = get_theme_icon(SNAME("arrow_collapsed_mirrored"), SNAME("Tree"));
		}
		set_tooltip_text(TTR("Expand positioning hint."));
	}

	hint_icon->set_texture(arrow);
}

void ControlPositioningWarning::set_control(Control *p_node) {
	control_node = p_node;
	_update_warning();
}

void ControlPositioningWarning::gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		bool state = !hint_label->is_visible();

		hint_filler_left->set_visible(state);
		hint_label->set_visible(state);
		hint_filler_right->set_visible(state);

		_update_toggler();
	}
}

void ControlPositioningWarning::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED:
			_update_warning();
			_update_toggler();
			break;
	}
}

ControlPositioningWarning::ControlPositioningWarning() {
	set_mouse_filter(MOUSE_FILTER_STOP);

	bg_panel = memnew(PanelContainer);
	bg_panel->set_mouse_filter(MOUSE_FILTER_IGNORE);
	add_child(bg_panel);

	grid = memnew(GridContainer);
	grid->set_columns(3);
	bg_panel->add_child(grid);

	title_icon = memnew(TextureRect);
	title_icon->set_stretch_mode(TextureRect::StretchMode::STRETCH_KEEP_CENTERED);
	grid->add_child(title_icon);

	title_label = memnew(Label);
	title_label->set_autowrap_mode(TextServer::AutowrapMode::AUTOWRAP_WORD);
	title_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_label->set_vertical_alignment(VerticalAlignment::VERTICAL_ALIGNMENT_CENTER);
	grid->add_child(title_label);

	hint_icon = memnew(TextureRect);
	hint_icon->set_stretch_mode(TextureRect::StretchMode::STRETCH_KEEP_CENTERED);
	grid->add_child(hint_icon);

	// Filler.
	hint_filler_left = memnew(Control);
	hint_filler_left->hide();
	grid->add_child(hint_filler_left);

	hint_label = memnew(Label);
	hint_label->set_autowrap_mode(TextServer::AutowrapMode::AUTOWRAP_WORD);
	hint_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hint_label->set_vertical_alignment(VerticalAlignment::VERTICAL_ALIGNMENT_CENTER);
	hint_label->hide();
	grid->add_child(hint_label);

	// Filler.
	hint_filler_right = memnew(Control);
	hint_filler_right->hide();
	grid->add_child(hint_filler_right);
}

void EditorPropertyAnchorsPreset::_set_read_only(bool p_read_only) {
	options->set_disabled(p_read_only);
};

void EditorPropertyAnchorsPreset::_option_selected(int p_which) {
	int64_t val = options->get_item_metadata(p_which);
	emit_changed(get_edited_property(), val);
}

void EditorPropertyAnchorsPreset::update_property() {
	int64_t which = get_edited_property_value();

	for (int i = 0; i < options->get_item_count(); i++) {
		Variant val = options->get_item_metadata(i);
		if (val != Variant() && which == (int64_t)val) {
			options->select(i);
			return;
		}
	}
}

void EditorPropertyAnchorsPreset::setup(const Vector<String> &p_options) {
	options->clear();

	Vector<String> split_after;
	split_after.append("Custom");
	split_after.append("PresetFullRect");
	split_after.append("PresetBottomLeft");
	split_after.append("PresetCenter");

	for (int i = 0, j = 0; i < p_options.size(); i++, j++) {
		Vector<String> text_split = p_options[i].split(":");
		int64_t current_val = text_split[1].to_int();

		const String &option_name = text_split[0];
		if (option_name.begins_with("Preset")) {
			String preset_name = option_name.trim_prefix("Preset");
			String humanized_name = preset_name.capitalize();
			String icon_name = "ControlAlign" + preset_name;
			options->add_icon_item(EditorNode::get_singleton()->get_editor_theme()->get_icon(icon_name, EditorStringName(EditorIcons)), humanized_name);
		} else {
			options->add_item(option_name);
		}

		options->set_item_metadata(j, current_val);
		if (split_after.has(option_name)) {
			options->add_separator();
			j++;
		}
	}
}

EditorPropertyAnchorsPreset::EditorPropertyAnchorsPreset() {
	options = memnew(OptionButton);
	options->set_clip_text(true);
	options->set_flat(true);
	add_child(options);
	add_focusable(options);
	options->connect(SceneStringName(item_selected), callable_mp(this, &EditorPropertyAnchorsPreset::_option_selected));
}

void EditorPropertySizeFlags::_set_read_only(bool p_read_only) {
	for (CheckBox *check : flag_checks) {
		check->set_disabled(p_read_only);
	}
	flag_presets->set_disabled(p_read_only);
};

void EditorPropertySizeFlags::_preset_selected(int p_which) {
	int preset = flag_presets->get_item_id(p_which);
	if (preset == SIZE_FLAGS_PRESET_CUSTOM) {
		flag_options->set_visible(true);
		return;
	}
	flag_options->set_visible(false);

	uint32_t value = 0;
	switch (preset) {
		case SIZE_FLAGS_PRESET_FILL:
			value = Control::SIZE_FILL;
			break;
		case SIZE_FLAGS_PRESET_SHRINK_BEGIN:
			value = Control::SIZE_SHRINK_BEGIN;
			break;
		case SIZE_FLAGS_PRESET_SHRINK_CENTER:
			value = Control::SIZE_SHRINK_CENTER;
			break;
		case SIZE_FLAGS_PRESET_SHRINK_END:
			value = Control::SIZE_SHRINK_END;
			break;
	}

	bool is_expand = flag_expand->is_visible() && flag_expand->is_pressed();
	if (is_expand) {
		value |= Control::SIZE_EXPAND;
	}

	emit_changed(get_edited_property(), value);
}

void EditorPropertySizeFlags::_expand_toggled() {
	uint32_t value = get_edited_property_value();

	if (flag_expand->is_visible() && flag_expand->is_pressed()) {
		value |= Control::SIZE_EXPAND;
	} else {
		value ^= Control::SIZE_EXPAND;
	}

	// Keep the custom preset selected as we toggle individual flags.
	keep_selected_preset = true;
	emit_changed(get_edited_property(), value);
}

void EditorPropertySizeFlags::_flag_toggled() {
	uint32_t value = 0;
	for (int i = 0; i < flag_checks.size(); i++) {
		if (flag_checks[i]->is_pressed()) {
			int flag_value = flag_checks[i]->get_meta("_value");
			value |= flag_value;
		}
	}

	bool is_expand = flag_expand->is_visible() && flag_expand->is_pressed();
	if (is_expand) {
		value |= Control::SIZE_EXPAND;
	}

	// Keep the custom preset selected as we toggle individual flags.
	keep_selected_preset = true;
	emit_changed(get_edited_property(), value);
}

void EditorPropertySizeFlags::update_property() {
	uint32_t value = get_edited_property_value();

	for (int i = 0; i < flag_checks.size(); i++) {
		int flag_value = flag_checks[i]->get_meta("_value");
		if (value & flag_value) {
			flag_checks[i]->set_pressed(true);
		} else {
			flag_checks[i]->set_pressed(false);
		}
	}

	bool is_expand = value & Control::SIZE_EXPAND;
	flag_expand->set_pressed(is_expand);

	if (keep_selected_preset) {
		keep_selected_preset = false;
		return;
	}

	FlagPreset preset = SIZE_FLAGS_PRESET_CUSTOM;
	if (value == Control::SIZE_FILL || value == (Control::SIZE_FILL | Control::SIZE_EXPAND)) {
		preset = SIZE_FLAGS_PRESET_FILL;
	} else if (value == Control::SIZE_SHRINK_BEGIN || value == (Control::SIZE_SHRINK_BEGIN | Control::SIZE_EXPAND)) {
		preset = SIZE_FLAGS_PRESET_SHRINK_BEGIN;
	} else if (value == Control::SIZE_SHRINK_CENTER || value == (Control::SIZE_SHRINK_CENTER | Control::SIZE_EXPAND)) {
		preset = SIZE_FLAGS_PRESET_SHRINK_CENTER;
	} else if (value == Control::SIZE_SHRINK_END || value == (Control::SIZE_SHRINK_END | Control::SIZE_EXPAND)) {
		preset = SIZE_FLAGS_PRESET_SHRINK_END;
	}

	int preset_idx = flag_presets->get_item_index(preset);
	if (preset_idx >= 0) {
		flag_presets->select(preset_idx);
	}
	flag_options->set_visible(preset == SIZE_FLAGS_PRESET_CUSTOM);
}

void EditorPropertySizeFlags::setup(const Vector<String> &p_options, bool p_vertical) {
	vertical = p_vertical;

	if (p_options.size() == 0) {
		flag_presets->clear();
		flag_presets->add_item(TTR("Container Default"));
		flag_presets->set_disabled(true);
		flag_expand->set_visible(false);
		return;
	}

	HashMap<int, String> flags;
	for (int i = 0, j = 0; i < p_options.size(); i++, j++) {
		Vector<String> text_split = p_options[i].split(":");
		int64_t current_val = text_split[1].to_int();
		flags[current_val] = text_split[0];

		if (current_val == SIZE_EXPAND) {
			continue;
		}

		CheckBox *cb = memnew(CheckBox);
		cb->set_text(text_split[0]);
		cb->set_clip_text(true);
		cb->set_meta("_value", current_val);
		cb->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertySizeFlags::_flag_toggled));
		add_focusable(cb);

		flag_options->add_child(cb);
		flag_checks.append(cb);
	}

	Control *gui_base = EditorNode::get_singleton()->get_gui_base();
	StringName wide_preset_icon = SNAME("ControlAlignHCenterWide");
	StringName begin_preset_icon = SNAME("ControlAlignCenterLeft");
	StringName end_preset_icon = SNAME("ControlAlignCenterRight");
	if (vertical) {
		wide_preset_icon = SNAME("ControlAlignVCenterWide");
		begin_preset_icon = SNAME("ControlAlignCenterTop");
		end_preset_icon = SNAME("ControlAlignCenterBottom");
	}

	flag_presets->clear();
	if (flags.has(SIZE_FILL)) {
		flag_presets->add_icon_item(gui_base->get_editor_theme_icon(wide_preset_icon), TTR("Fill"), SIZE_FLAGS_PRESET_FILL);
	}
	// Shrink Begin is the same as no flags at all, as such it cannot be disabled.
	flag_presets->add_icon_item(gui_base->get_editor_theme_icon(begin_preset_icon), TTR("Shrink Begin"), SIZE_FLAGS_PRESET_SHRINK_BEGIN);
	if (flags.has(SIZE_SHRINK_CENTER)) {
		flag_presets->add_icon_item(gui_base->get_editor_theme_icon(SNAME("ControlAlignCenter")), TTR("Shrink Center"), SIZE_FLAGS_PRESET_SHRINK_CENTER);
	}
	if (flags.has(SIZE_SHRINK_END)) {
		flag_presets->add_icon_item(gui_base->get_editor_theme_icon(end_preset_icon), TTR("Shrink End"), SIZE_FLAGS_PRESET_SHRINK_END);
	}
	flag_presets->add_separator();
	flag_presets->add_item(TTR("Custom"), SIZE_FLAGS_PRESET_CUSTOM);

	flag_expand->set_visible(flags.has(SIZE_EXPAND));
}

EditorPropertySizeFlags::EditorPropertySizeFlags() {
	VBoxContainer *vb = memnew(VBoxContainer);
	add_child(vb);

	flag_presets = memnew(OptionButton);
	flag_presets->set_clip_text(true);
	flag_presets->set_flat(true);
	vb->add_child(flag_presets);
	add_focusable(flag_presets);
	set_label_reference(flag_presets);
	flag_presets->connect(SceneStringName(item_selected), callable_mp(this, &EditorPropertySizeFlags::_preset_selected));

	flag_options = memnew(VBoxContainer);
	flag_options->hide();
	vb->add_child(flag_options);

	flag_expand = memnew(CheckBox);
	flag_expand->set_text(TTR("Expand"));
	vb->add_child(flag_expand);
	add_focusable(flag_expand);
	flag_expand->connect(SceneStringName(pressed), callable_mp(this, &EditorPropertySizeFlags::_expand_toggled));
}

bool EditorInspectorPluginControl::can_handle(Object *p_object) {
	return Object::cast_to<Control>(p_object) != nullptr;
}

void EditorInspectorPluginControl::parse_category(Object *p_object, const String &p_category) {
	inside_control_category = p_category == "Control";
}

void EditorInspectorPluginControl::parse_group(Object *p_object, const String &p_group) {
	if (!inside_control_category) {
		return;
	}

	Control *control = Object::cast_to<Control>(p_object);
	if (!control || p_group != "Layout") {
		return;
	}

	ControlPositioningWarning *pos_warning = memnew(ControlPositioningWarning);
	pos_warning->set_control(control);
	add_custom_control(pos_warning);
}

bool EditorInspectorPluginControl::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	Control *control = Object::cast_to<Control>(p_object);
	if (!control) {
		return false;
	}

	if (p_path == "anchors_preset") {
		EditorPropertyAnchorsPreset *prop_editor = memnew(EditorPropertyAnchorsPreset);
		Vector<String> options = p_hint_text.split(",");
		prop_editor->setup(options);
		add_property_editor(p_path, prop_editor);

		return true;
	}

	if (p_path == "size_flags_horizontal" || p_path == "size_flags_vertical") {
		EditorPropertySizeFlags *prop_editor = memnew(EditorPropertySizeFlags);
		Vector<String> options;
		if (!p_hint_text.is_empty()) {
			options = p_hint_text.split(",");
		}
		prop_editor->setup(options, p_path == "size_flags_vertical");
		add_property_editor(p_path, prop_editor);

		return true;
	}

	return false;
}

// Toolbars controls.

Size2 ControlEditorPopupButton::get_minimum_size() const {
	Vector2 base_size = Vector2(26, 26) * EDSCALE;

	if (arrow_icon.is_null()) {
		return base_size;
	}

	Vector2 final_size;
	final_size.x = base_size.x + arrow_icon->get_width();
	final_size.y = MAX(base_size.y, arrow_icon->get_height());

	return final_size;
}

void ControlEditorPopupButton::toggled(bool p_pressed) {
	if (!p_pressed) {
		return;
	}

	Size2 size = get_size() * get_viewport()->get_canvas_transform().get_scale();

	popup_panel->set_size(Size2(size.width, 0));
	Point2 gp = get_screen_position();
	gp.y += size.y;
	if (is_layout_rtl()) {
		gp.x += size.width - popup_panel->get_size().width;
	}
	popup_panel->set_position(gp);

	popup_panel->popup();
}

void ControlEditorPopupButton::_popup_visibility_changed(bool p_visible) {
	set_pressed(p_visible);
}

void ControlEditorPopupButton::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			arrow_icon = get_theme_icon("select_arrow", "Tree");
		} break;

		case NOTIFICATION_DRAW: {
			if (arrow_icon.is_valid()) {
				Vector2 arrow_pos = Point2(26, 0) * EDSCALE;
				if (is_layout_rtl()) {
					arrow_pos.x = get_size().x - arrow_pos.x - arrow_icon->get_width();
				}
				arrow_pos.y = get_size().y / 2 - arrow_icon->get_height() / 2;
				draw_texture(arrow_icon, arrow_pos);
			}
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED: {
			popup_panel->set_layout_direction((Window::LayoutDirection)get_layout_direction());
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible_in_tree()) {
				popup_panel->hide();
			}
		} break;
	}
}

ControlEditorPopupButton::ControlEditorPopupButton() {
	set_theme_type_variation("FlatButton");
	set_toggle_mode(true);
	set_focus_mode(FOCUS_NONE);

	popup_panel = memnew(PopupPanel);
	add_child(popup_panel);
	popup_panel->connect("about_to_popup", callable_mp(this, &ControlEditorPopupButton::_popup_visibility_changed).bind(true));
	popup_panel->connect("popup_hide", callable_mp(this, &ControlEditorPopupButton::_popup_visibility_changed).bind(false));

	popup_vbox = memnew(VBoxContainer);
	popup_panel->add_child(popup_vbox);
}

void ControlEditorPresetPicker::_add_row_button(HBoxContainer *p_row, const int p_preset, const String &p_name) {
	ERR_FAIL_COND(preset_buttons.has(p_preset));

	Button *b = memnew(Button);
	b->set_custom_minimum_size(Size2i(36, 36) * EDSCALE);
	b->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	b->set_tooltip_text(p_name);
	b->set_flat(true);
	p_row->add_child(b);
	b->connect(SceneStringName(pressed), callable_mp(this, &ControlEditorPresetPicker::_preset_button_pressed).bind(p_preset));

	preset_buttons[p_preset] = b;
}

void ControlEditorPresetPicker::_add_separator(BoxContainer *p_box, Separator *p_separator) {
	p_separator->add_theme_constant_override("separation", grid_separation);
	p_separator->set_custom_minimum_size(Size2i(1, 1));
	p_box->add_child(p_separator);
}

void AnchorPresetPicker::_preset_button_pressed(const int p_preset) {
	emit_signal("anchors_preset_selected", p_preset);
}

void AnchorPresetPicker::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			preset_buttons[PRESET_TOP_LEFT]->set_icon(get_editor_theme_icon(SNAME("ControlAlignTopLeft")));
			preset_buttons[PRESET_CENTER_TOP]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenterTop")));
			preset_buttons[PRESET_TOP_RIGHT]->set_icon(get_editor_theme_icon(SNAME("ControlAlignTopRight")));

			preset_buttons[PRESET_CENTER_LEFT]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenterLeft")));
			preset_buttons[PRESET_CENTER]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenter")));
			preset_buttons[PRESET_CENTER_RIGHT]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenterRight")));

			preset_buttons[PRESET_BOTTOM_LEFT]->set_icon(get_editor_theme_icon(SNAME("ControlAlignBottomLeft")));
			preset_buttons[PRESET_CENTER_BOTTOM]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenterBottom")));
			preset_buttons[PRESET_BOTTOM_RIGHT]->set_icon(get_editor_theme_icon(SNAME("ControlAlignBottomRight")));

			preset_buttons[PRESET_TOP_WIDE]->set_icon(get_editor_theme_icon(SNAME("ControlAlignTopWide")));
			preset_buttons[PRESET_HCENTER_WIDE]->set_icon(get_editor_theme_icon(SNAME("ControlAlignHCenterWide")));
			preset_buttons[PRESET_BOTTOM_WIDE]->set_icon(get_editor_theme_icon(SNAME("ControlAlignBottomWide")));

			preset_buttons[PRESET_LEFT_WIDE]->set_icon(get_editor_theme_icon(SNAME("ControlAlignLeftWide")));
			preset_buttons[PRESET_VCENTER_WIDE]->set_icon(get_editor_theme_icon(SNAME("ControlAlignVCenterWide")));
			preset_buttons[PRESET_RIGHT_WIDE]->set_icon(get_editor_theme_icon(SNAME("ControlAlignRightWide")));

			preset_buttons[PRESET_FULL_RECT]->set_icon(get_editor_theme_icon(SNAME("ControlAlignFullRect")));
		} break;
	}
}

void AnchorPresetPicker::_bind_methods() {
	ADD_SIGNAL(MethodInfo("anchors_preset_selected", PropertyInfo(Variant::INT, "preset")));
}

AnchorPresetPicker::AnchorPresetPicker() {
	VBoxContainer *main_vb = memnew(VBoxContainer);
	main_vb->add_theme_constant_override("separation", grid_separation);
	add_child(main_vb);

	HBoxContainer *top_row = memnew(HBoxContainer);
	top_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	top_row->add_theme_constant_override("separation", grid_separation);
	main_vb->add_child(top_row);

	_add_row_button(top_row, PRESET_TOP_LEFT, TTR("Top Left"));
	_add_row_button(top_row, PRESET_CENTER_TOP, TTR("Center Top"));
	_add_row_button(top_row, PRESET_TOP_RIGHT, TTR("Top Right"));
	_add_separator(top_row, memnew(VSeparator));
	_add_row_button(top_row, PRESET_TOP_WIDE, TTR("Top Wide"));

	HBoxContainer *mid_row = memnew(HBoxContainer);
	mid_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	mid_row->add_theme_constant_override("separation", grid_separation);
	main_vb->add_child(mid_row);

	_add_row_button(mid_row, PRESET_CENTER_LEFT, TTR("Center Left"));
	_add_row_button(mid_row, PRESET_CENTER, TTR("Center"));
	_add_row_button(mid_row, PRESET_CENTER_RIGHT, TTR("Center Right"));
	_add_separator(mid_row, memnew(VSeparator));
	_add_row_button(mid_row, PRESET_HCENTER_WIDE, TTR("HCenter Wide"));

	HBoxContainer *bot_row = memnew(HBoxContainer);
	bot_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	bot_row->add_theme_constant_override("separation", grid_separation);
	main_vb->add_child(bot_row);

	_add_row_button(bot_row, PRESET_BOTTOM_LEFT, TTR("Bottom Left"));
	_add_row_button(bot_row, PRESET_CENTER_BOTTOM, TTR("Center Bottom"));
	_add_row_button(bot_row, PRESET_BOTTOM_RIGHT, TTR("Bottom Right"));
	_add_separator(bot_row, memnew(VSeparator));
	_add_row_button(bot_row, PRESET_BOTTOM_WIDE, TTR("Bottom Wide"));

	_add_separator(main_vb, memnew(HSeparator));

	HBoxContainer *extra_row = memnew(HBoxContainer);
	extra_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	extra_row->add_theme_constant_override("separation", grid_separation);
	main_vb->add_child(extra_row);

	_add_row_button(extra_row, PRESET_LEFT_WIDE, TTR("Left Wide"));
	_add_row_button(extra_row, PRESET_VCENTER_WIDE, TTR("VCenter Wide"));
	_add_row_button(extra_row, PRESET_RIGHT_WIDE, TTR("Right Wide"));
	_add_separator(extra_row, memnew(VSeparator));
	_add_row_button(extra_row, PRESET_FULL_RECT, TTR("Full Rect"));
}

void SizeFlagPresetPicker::_preset_button_pressed(const int p_preset) {
	int flags = (SizeFlags)p_preset;
	if (expand_button->is_pressed()) {
		flags |= SIZE_EXPAND;
	}

	emit_signal("size_flags_selected", flags);
}

void SizeFlagPresetPicker::_expand_button_pressed() {
	emit_signal("expand_flag_toggled", expand_button->is_pressed());
}

void SizeFlagPresetPicker::set_allowed_flags(Vector<SizeFlags> &p_flags) {
	preset_buttons[SIZE_SHRINK_BEGIN]->set_disabled(!p_flags.has(SIZE_SHRINK_BEGIN));
	preset_buttons[SIZE_SHRINK_CENTER]->set_disabled(!p_flags.has(SIZE_SHRINK_CENTER));
	preset_buttons[SIZE_SHRINK_END]->set_disabled(!p_flags.has(SIZE_SHRINK_END));
	preset_buttons[SIZE_FILL]->set_disabled(!p_flags.has(SIZE_FILL));

	expand_button->set_disabled(!p_flags.has(SIZE_EXPAND));
	if (p_flags.has(SIZE_EXPAND)) {
		expand_button->set_tooltip_text(TTR("Enable to also set the Expand flag.\nDisable to only set Shrink/Fill flags."));
	} else {
		expand_button->set_pressed(false);
		expand_button->set_tooltip_text(TTR("Some parents of the selected nodes do not support the Expand flag."));
	}
}

void SizeFlagPresetPicker::set_expand_flag(bool p_expand) {
	expand_button->set_pressed(p_expand);
}

void SizeFlagPresetPicker::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			if (vertical) {
				preset_buttons[SIZE_SHRINK_BEGIN]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenterTop")));
				preset_buttons[SIZE_SHRINK_CENTER]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenter")));
				preset_buttons[SIZE_SHRINK_END]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenterBottom")));

				preset_buttons[SIZE_FILL]->set_icon(get_editor_theme_icon(SNAME("ControlAlignVCenterWide")));
			} else {
				preset_buttons[SIZE_SHRINK_BEGIN]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenterLeft")));
				preset_buttons[SIZE_SHRINK_CENTER]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenter")));
				preset_buttons[SIZE_SHRINK_END]->set_icon(get_editor_theme_icon(SNAME("ControlAlignCenterRight")));

				preset_buttons[SIZE_FILL]->set_icon(get_editor_theme_icon(SNAME("ControlAlignHCenterWide")));
			}
		} break;
	}
}

void SizeFlagPresetPicker::_bind_methods() {
	ADD_SIGNAL(MethodInfo("size_flags_selected", PropertyInfo(Variant::INT, "size_flags")));
	ADD_SIGNAL(MethodInfo("expand_flag_toggled", PropertyInfo(Variant::BOOL, "expand_flag")));
}

SizeFlagPresetPicker::SizeFlagPresetPicker(bool p_vertical) {
	vertical = p_vertical;

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);

	HBoxContainer *main_row = memnew(HBoxContainer);
	main_row->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	main_row->add_theme_constant_override("separation", grid_separation);
	main_vb->add_child(main_row);

	_add_row_button(main_row, SIZE_SHRINK_BEGIN, TTR("Shrink Begin"));
	_add_row_button(main_row, SIZE_SHRINK_CENTER, TTR("Shrink Center"));
	_add_row_button(main_row, SIZE_SHRINK_END, TTR("Shrink End"));
	_add_separator(main_row, memnew(VSeparator));
	_add_row_button(main_row, SIZE_FILL, TTR("Fill"));

	expand_button = memnew(CheckButton);
	expand_button->set_flat(true);
	expand_button->set_text(TTR("Expand"));
	expand_button->set_tooltip_text(TTR("Enable to also set the Expand flag.\nDisable to only set Shrink/Fill flags."));
	expand_button->connect(SceneStringName(pressed), callable_mp(this, &SizeFlagPresetPicker::_expand_button_pressed));
	main_vb->add_child(expand_button);
}

// Toolbar.

void ControlEditorToolbar::_anchors_preset_selected(int p_preset) {
	LayoutPreset preset = (LayoutPreset)p_preset;
	List<Node *> selection = editor_selection->get_selected_node_list();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Change Anchors, Offsets, Grow Direction"));

	for (Node *E : selection) {
		Control *control = Object::cast_to<Control>(E);
		if (control) {
			undo_redo->add_do_property(control, "layout_mode", LayoutMode::LAYOUT_MODE_ANCHORS);
			undo_redo->add_do_property(control, "anchors_preset", preset);
			undo_redo->add_undo_method(control, "_edit_set_state", control->_edit_get_state());
		}
	}

	undo_redo->commit_action();

	anchors_mode = false;
	anchor_mode_button->set_pressed(anchors_mode);
}

void ControlEditorToolbar::_anchors_to_current_ratio() {
	List<Node *> selection = editor_selection->get_selected_node_list();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Change Anchors, Offsets (Keep Ratio)"));

	for (Node *E : selection) {
		Control *control = Object::cast_to<Control>(E);
		if (control) {
			Point2 top_left_anchor = _position_to_anchor(control, Point2());
			Point2 bottom_right_anchor = _position_to_anchor(control, control->get_size());
			undo_redo->add_do_method(control, "set_anchor", SIDE_LEFT, top_left_anchor.x, false, true);
			undo_redo->add_do_method(control, "set_anchor", SIDE_RIGHT, bottom_right_anchor.x, false, true);
			undo_redo->add_do_method(control, "set_anchor", SIDE_TOP, top_left_anchor.y, false, true);
			undo_redo->add_do_method(control, "set_anchor", SIDE_BOTTOM, bottom_right_anchor.y, false, true);
			undo_redo->add_do_method(control, "set_meta", "_edit_use_anchors_", true);

			const bool use_anchors = control->get_meta("_edit_use_anchors_", false);
			undo_redo->add_undo_method(control, "_edit_set_state", control->_edit_get_state());
			if (use_anchors) {
				undo_redo->add_undo_method(control, "set_meta", "_edit_use_anchors_", true);
			} else {
				undo_redo->add_undo_method(control, "remove_meta", "_edit_use_anchors_");
			}

			anchors_mode = true;
			anchor_mode_button->set_pressed(anchors_mode);
		}
	}

	undo_redo->commit_action();
}

void ControlEditorToolbar::_anchor_mode_toggled(bool p_status) {
	List<Control *> selection = _get_edited_controls();
	for (Control *E : selection) {
		if (Object::cast_to<Container>(E->get_parent())) {
			continue;
		}

		if (p_status) {
			E->set_meta("_edit_use_anchors_", true);
		} else {
			E->remove_meta("_edit_use_anchors_");
		}
	}

	anchors_mode = p_status;
	CanvasItemEditor::get_singleton()->update_viewport();
}

void ControlEditorToolbar::_container_flags_selected(int p_flags, bool p_vertical) {
	List<Node *> selection = editor_selection->get_selected_node_list();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (p_vertical) {
		undo_redo->create_action(TTR("Change Vertical Size Flags"));
	} else {
		undo_redo->create_action(TTR("Change Horizontal Size Flags"));
	}

	for (Node *E : selection) {
		Control *control = Object::cast_to<Control>(E);
		if (control) {
			int old_flags = p_vertical ? control->get_v_size_flags() : control->get_h_size_flags();
			if (p_vertical) {
				undo_redo->add_do_method(control, "set_v_size_flags", p_flags);
				undo_redo->add_undo_method(control, "set_v_size_flags", old_flags);
			} else {
				undo_redo->add_do_method(control, "set_h_size_flags", p_flags);
				undo_redo->add_undo_method(control, "set_h_size_flags", old_flags);
			}
		}
	}

	undo_redo->commit_action();
}

void ControlEditorToolbar::_expand_flag_toggled(bool p_expand, bool p_vertical) {
	List<Node *> selection = editor_selection->get_selected_node_list();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	if (p_vertical) {
		undo_redo->create_action(TTR("Change Vertical Expand Flag"));
	} else {
		undo_redo->create_action(TTR("Change Horizontal Expand Flag"));
	}

	for (Node *E : selection) {
		Control *control = Object::cast_to<Control>(E);
		if (control) {
			int old_flags = p_vertical ? control->get_v_size_flags() : control->get_h_size_flags();
			int new_flags = old_flags;

			if (p_expand) {
				new_flags |= Control::SIZE_EXPAND;
			} else {
				new_flags &= ~Control::SIZE_EXPAND;
			}

			if (p_vertical) {
				undo_redo->add_do_method(control, "set_v_size_flags", new_flags);
				undo_redo->add_undo_method(control, "set_v_size_flags", old_flags);
			} else {
				undo_redo->add_do_method(control, "set_h_size_flags", new_flags);
				undo_redo->add_undo_method(control, "set_h_size_flags", old_flags);
			}
		}
	}

	undo_redo->commit_action();
}

Vector2 ControlEditorToolbar::_position_to_anchor(const Control *p_control, Vector2 position) {
	ERR_FAIL_NULL_V(p_control, Vector2());

	Rect2 parent_rect = p_control->get_parent_anchorable_rect();

	Vector2 output;
	if (p_control->is_layout_rtl()) {
		output.x = (parent_rect.size.x == 0) ? 0.0 : (parent_rect.size.x - p_control->get_transform().xform(position).x - parent_rect.position.x) / parent_rect.size.x;
	} else {
		output.x = (parent_rect.size.x == 0) ? 0.0 : (p_control->get_transform().xform(position).x - parent_rect.position.x) / parent_rect.size.x;
	}
	output.y = (parent_rect.size.y == 0) ? 0.0 : (p_control->get_transform().xform(position).y - parent_rect.position.y) / parent_rect.size.y;
	return output;
}

bool ControlEditorToolbar::_is_node_locked(const Node *p_node) {
	return p_node->get_meta("_edit_lock_", false);
}

List<Control *> ControlEditorToolbar::_get_edited_controls() {
	List<Control *> selection;
	for (const KeyValue<Node *, Object *> &E : editor_selection->get_selection()) {
		Control *control = Object::cast_to<Control>(E.key);
		if (control && control->is_visible_in_tree() && control->get_viewport() == EditorNode::get_singleton()->get_scene_root() && !_is_node_locked(control)) {
			selection.push_back(control);
		}
	}

	return selection;
}

void ControlEditorToolbar::_selection_changed() {
	// Update toolbar visibility.
	bool has_controls = false;
	bool has_control_parents = false;
	bool has_container_parents = false;

	// Also update which size flags can be configured for the selected nodes.
	Vector<SizeFlags> allowed_h_flags = {
		SIZE_SHRINK_BEGIN,
		SIZE_SHRINK_CENTER,
		SIZE_SHRINK_END,
		SIZE_FILL,
		SIZE_EXPAND,
	};
	Vector<SizeFlags> allowed_v_flags = {
		SIZE_SHRINK_BEGIN,
		SIZE_SHRINK_CENTER,
		SIZE_SHRINK_END,
		SIZE_FILL,
		SIZE_EXPAND,
	};

	for (const KeyValue<Node *, Object *> &E : editor_selection->get_selection()) {
		Control *control = Object::cast_to<Control>(E.key);
		if (!control) {
			continue;
		}
		has_controls = true;

		if (Object::cast_to<Control>(control->get_parent())) {
			has_control_parents = true;
		}
		if (Object::cast_to<Container>(control->get_parent())) {
			has_container_parents = true;

			Container *parent_container = Object::cast_to<Container>(control->get_parent());

			Vector<int> container_h_flags = parent_container->get_allowed_size_flags_horizontal();
			Vector<SizeFlags> tmp_flags = allowed_h_flags.duplicate();
			for (int i = 0; i < allowed_h_flags.size(); i++) {
				if (!container_h_flags.has((int)allowed_h_flags[i])) {
					tmp_flags.erase(allowed_h_flags[i]);
				}
			}
			allowed_h_flags = tmp_flags;

			Vector<int> container_v_flags = parent_container->get_allowed_size_flags_vertical();
			tmp_flags = allowed_v_flags.duplicate();
			for (int i = 0; i < allowed_v_flags.size(); i++) {
				if (!container_v_flags.has((int)allowed_v_flags[i])) {
					tmp_flags.erase(allowed_v_flags[i]);
				}
			}
			allowed_v_flags = tmp_flags;
		}
	}

	// Set general toolbar visibility.
	set_visible(has_controls);

	// Set anchor tools visibility.
	if (has_controls && (!has_control_parents || !has_container_parents)) {
		anchors_button->set_visible(true);
		anchor_mode_button->set_visible(true);

		// Update anchor mode.
		int nb_valid_controls = 0;
		int nb_anchors_mode = 0;

		List<Node *> selection = editor_selection->get_selected_node_list();
		for (Node *E : selection) {
			Control *control = Object::cast_to<Control>(E);
			if (!control) {
				continue;
			}
			if (Object::cast_to<Container>(control->get_parent())) {
				continue;
			}

			nb_valid_controls++;
			if (control->get_meta("_edit_use_anchors_", false)) {
				nb_anchors_mode++;
			}
		}

		anchors_mode = (nb_valid_controls == nb_anchors_mode);
		anchor_mode_button->set_pressed(anchors_mode);
	} else {
		anchors_button->set_visible(false);
		anchor_mode_button->set_visible(false);
		anchor_mode_button->set_pressed(false);
	}

	// Set container tools visibility.
	if (has_controls && (!has_control_parents || has_container_parents)) {
		containers_button->set_visible(true);

		// Update allowed size flags.
		if (has_container_parents) {
			container_h_picker->set_allowed_flags(allowed_h_flags);
			container_v_picker->set_allowed_flags(allowed_v_flags);
		} else {
			Vector<SizeFlags> allowed_all_flags = {
				SIZE_SHRINK_BEGIN,
				SIZE_SHRINK_CENTER,
				SIZE_SHRINK_END,
				SIZE_FILL,
				SIZE_EXPAND,
			};

			container_h_picker->set_allowed_flags(allowed_all_flags);
			container_v_picker->set_allowed_flags(allowed_all_flags);
		}

		// Update expand toggles.
		int nb_valid_controls = 0;
		int nb_h_expand = 0;
		int nb_v_expand = 0;

		List<Node *> selection = editor_selection->get_selected_node_list();
		for (Node *E : selection) {
			Control *control = Object::cast_to<Control>(E);
			if (!control) {
				continue;
			}

			nb_valid_controls++;
			if (control->get_h_size_flags() & Control::SIZE_EXPAND) {
				nb_h_expand++;
			}
			if (control->get_v_size_flags() & Control::SIZE_EXPAND) {
				nb_v_expand++;
			}
		}

		container_h_picker->set_expand_flag(nb_valid_controls == nb_h_expand);
		container_v_picker->set_expand_flag(nb_valid_controls == nb_v_expand);
	} else {
		containers_button->set_visible(false);
	}
}

void ControlEditorToolbar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			anchors_button->set_icon(get_editor_theme_icon(SNAME("ControlLayout")));
			anchor_mode_button->set_icon(get_editor_theme_icon(SNAME("Anchor")));
			containers_button->set_icon(get_editor_theme_icon(SNAME("ContainerLayout")));
		} break;
	}
}

ControlEditorToolbar::ControlEditorToolbar() {
	// Anchor and offset tools.
	anchors_button = memnew(ControlEditorPopupButton);
	anchors_button->set_tooltip_text(TTR("Presets for the anchor and offset values of a Control node."));
	add_child(anchors_button);

	Label *anchors_label = memnew(Label);
	anchors_label->set_text(TTR("Anchor preset"));
	anchors_button->get_popup_hbox()->add_child(anchors_label);
	AnchorPresetPicker *anchors_picker = memnew(AnchorPresetPicker);
	anchors_picker->set_h_size_flags(SIZE_SHRINK_CENTER);
	anchors_button->get_popup_hbox()->add_child(anchors_picker);
	anchors_picker->connect("anchors_preset_selected", callable_mp(this, &ControlEditorToolbar::_anchors_preset_selected));

	anchors_button->get_popup_hbox()->add_child(memnew(HSeparator));

	Button *keep_ratio_button = memnew(Button);
	keep_ratio_button->set_text_alignment(HORIZONTAL_ALIGNMENT_LEFT);
	keep_ratio_button->set_text(TTR("Set to Current Ratio"));
	keep_ratio_button->set_tooltip_text(TTR("Adjust anchors and offsets to match the current rect size."));
	anchors_button->get_popup_hbox()->add_child(keep_ratio_button);
	keep_ratio_button->connect(SceneStringName(pressed), callable_mp(this, &ControlEditorToolbar::_anchors_to_current_ratio));

	anchor_mode_button = memnew(Button);
	anchor_mode_button->set_theme_type_variation("FlatButton");
	anchor_mode_button->set_toggle_mode(true);
	anchor_mode_button->set_tooltip_text(TTR("When active, moving Control nodes changes their anchors instead of their offsets."));
	add_child(anchor_mode_button);
	anchor_mode_button->connect(SceneStringName(toggled), callable_mp(this, &ControlEditorToolbar::_anchor_mode_toggled));

	// Container tools.
	containers_button = memnew(ControlEditorPopupButton);
	containers_button->set_tooltip_text(TTR("Sizing settings for children of a Container node."));
	add_child(containers_button);

	Label *container_h_label = memnew(Label);
	container_h_label->set_text(TTR("Horizontal alignment"));
	containers_button->get_popup_hbox()->add_child(container_h_label);
	container_h_picker = memnew(SizeFlagPresetPicker(false));
	containers_button->get_popup_hbox()->add_child(container_h_picker);
	container_h_picker->connect("size_flags_selected", callable_mp(this, &ControlEditorToolbar::_container_flags_selected).bind(false));
	container_h_picker->connect("expand_flag_toggled", callable_mp(this, &ControlEditorToolbar::_expand_flag_toggled).bind(false));

	containers_button->get_popup_hbox()->add_child(memnew(HSeparator));

	Label *container_v_label = memnew(Label);
	container_v_label->set_text(TTR("Vertical alignment"));
	containers_button->get_popup_hbox()->add_child(container_v_label);
	container_v_picker = memnew(SizeFlagPresetPicker(true));
	containers_button->get_popup_hbox()->add_child(container_v_picker);
	container_v_picker->connect("size_flags_selected", callable_mp(this, &ControlEditorToolbar::_container_flags_selected).bind(true));
	container_v_picker->connect("expand_flag_toggled", callable_mp(this, &ControlEditorToolbar::_expand_flag_toggled).bind(true));

	// Editor connections.
	editor_selection = EditorNode::get_singleton()->get_editor_selection();
	editor_selection->add_editor_plugin(this);
	editor_selection->connect("selection_changed", callable_mp(this, &ControlEditorToolbar::_selection_changed));

	singleton = this;
}

ControlEditorToolbar *ControlEditorToolbar::singleton = nullptr;

// Editor plugin.

ControlEditorPlugin::ControlEditorPlugin() {
	toolbar = memnew(ControlEditorToolbar);
	toolbar->hide();
	add_control_to_container(CONTAINER_CANVAS_EDITOR_MENU, toolbar);

	Ref<EditorInspectorPluginControl> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
