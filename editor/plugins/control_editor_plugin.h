/**************************************************************************/
/*  control_editor_plugin.h                                               */
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

#ifndef CONTROL_EDITOR_PLUGIN_H
#define CONTROL_EDITOR_PLUGIN_H

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup.h"
#include "scene/gui/separator.h"
#include "scene/gui/texture_rect.h"

class CheckButton;
class EditorSelection;
class GridContainer;

// Inspector controls.
class ControlPositioningWarning : public MarginContainer {
	GDCLASS(ControlPositioningWarning, MarginContainer);

	Control *control_node = nullptr;

	PanelContainer *bg_panel = nullptr;
	GridContainer *grid = nullptr;
	TextureRect *title_icon = nullptr;
	TextureRect *hint_icon = nullptr;
	Label *title_label = nullptr;
	Label *hint_label = nullptr;
	Control *hint_filler_left = nullptr;
	Control *hint_filler_right = nullptr;

	void _update_warning();
	void _update_toggler();
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

protected:
	void _notification(int p_notification);

public:
	void set_control(Control *p_node);

	ControlPositioningWarning();
};

class EditorPropertyAnchorsPreset : public EditorProperty {
	GDCLASS(EditorPropertyAnchorsPreset, EditorProperty);
	OptionButton *options = nullptr;

	void _option_selected(int p_which);

protected:
	virtual void _set_read_only(bool p_read_only) override;

public:
	void setup(const Vector<String> &p_options);
	virtual void update_property() override;
	EditorPropertyAnchorsPreset();
};

class EditorPropertySizeFlags : public EditorProperty {
	GDCLASS(EditorPropertySizeFlags, EditorProperty);

	enum FlagPreset {
		SIZE_FLAGS_PRESET_FILL,
		SIZE_FLAGS_PRESET_SHRINK_BEGIN,
		SIZE_FLAGS_PRESET_SHRINK_CENTER,
		SIZE_FLAGS_PRESET_SHRINK_END,
		SIZE_FLAGS_PRESET_CUSTOM,
	};

	OptionButton *flag_presets = nullptr;
	CheckBox *flag_expand = nullptr;
	VBoxContainer *flag_options = nullptr;
	Vector<CheckBox *> flag_checks;

	bool vertical = false;

	bool keep_selected_preset = false;

	void _preset_selected(int p_which);
	void _expand_toggled();
	void _flag_toggled();

protected:
	virtual void _set_read_only(bool p_read_only) override;

public:
	void setup(const Vector<String> &p_options, bool p_vertical);
	virtual void update_property() override;
	EditorPropertySizeFlags();
};

class EditorInspectorPluginControl : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginControl, EditorInspectorPlugin);

	bool inside_control_category = false;

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_category(Object *p_object, const String &p_category) override;
	virtual void parse_group(Object *p_object, const String &p_group) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

// Toolbar controls.
class ControlEditorPopupButton : public Button {
	GDCLASS(ControlEditorPopupButton, Button);

	Ref<Texture2D> arrow_icon;

	PopupPanel *popup_panel = nullptr;
	VBoxContainer *popup_vbox = nullptr;

	void _popup_visibility_changed(bool p_visible);

protected:
	void _notification(int p_what);

public:
	virtual Size2 get_minimum_size() const override;
	virtual void toggled(bool p_pressed) override;

	VBoxContainer *get_popup_hbox() const { return popup_vbox; }

	ControlEditorPopupButton();
};

class ControlEditorPresetPicker : public MarginContainer {
	GDCLASS(ControlEditorPresetPicker, MarginContainer);

	virtual void _preset_button_pressed(const int p_preset) {}

protected:
	static constexpr int grid_separation = 0;
	HashMap<int, Button *> preset_buttons;

	void _add_row_button(HBoxContainer *p_row, const int p_preset, const String &p_name);
	void _add_separator(BoxContainer *p_box, Separator *p_separator);

public:
	ControlEditorPresetPicker() {}
};

class AnchorPresetPicker : public ControlEditorPresetPicker {
	GDCLASS(AnchorPresetPicker, ControlEditorPresetPicker);

	virtual void _preset_button_pressed(const int p_preset) override;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	AnchorPresetPicker();
};

class SizeFlagPresetPicker : public ControlEditorPresetPicker {
	GDCLASS(SizeFlagPresetPicker, ControlEditorPresetPicker);

	CheckButton *expand_button = nullptr;

	bool vertical = false;

	virtual void _preset_button_pressed(const int p_preset) override;
	void _expand_button_pressed();

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void set_allowed_flags(Vector<SizeFlags> &p_flags);
	void set_expand_flag(bool p_expand);

	SizeFlagPresetPicker(bool p_vertical);
};

class ControlEditorToolbar : public HBoxContainer {
	GDCLASS(ControlEditorToolbar, HBoxContainer);

	EditorSelection *editor_selection = nullptr;

	ControlEditorPopupButton *anchors_button = nullptr;
	ControlEditorPopupButton *containers_button = nullptr;
	Button *anchor_mode_button = nullptr;

	SizeFlagPresetPicker *container_h_picker = nullptr;
	SizeFlagPresetPicker *container_v_picker = nullptr;

	bool anchors_mode = false;

	void _anchors_preset_selected(int p_preset);
	void _anchors_to_current_ratio();
	void _anchor_mode_toggled(bool p_status);
	void _container_flags_selected(int p_flags, bool p_vertical);
	void _expand_flag_toggled(bool p_expand, bool p_vertical);

	Vector2 _position_to_anchor(const Control *p_control, Vector2 position);
	bool _is_node_locked(const Node *p_node);
	List<Control *> _get_edited_controls();
	void _selection_changed();

protected:
	void _notification(int p_notification);

	static ControlEditorToolbar *singleton;

public:
	bool is_anchors_mode_enabled() { return anchors_mode; }

	static ControlEditorToolbar *get_singleton() { return singleton; }

	ControlEditorToolbar();
};

// Editor plugin.
class ControlEditorPlugin : public EditorPlugin {
	GDCLASS(ControlEditorPlugin, EditorPlugin);

	ControlEditorToolbar *toolbar = nullptr;

public:
	virtual String get_plugin_name() const override { return "Control"; }

	ControlEditorPlugin();
};

#endif // CONTROL_EDITOR_PLUGIN_H
