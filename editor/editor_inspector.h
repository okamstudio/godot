/**************************************************************************/
/*  editor_inspector.h                                                    */
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

#ifndef EDITOR_INSPECTOR_H
#define EDITOR_INSPECTOR_H

#include "editor/add_metadata_dialog.h"
#include "editor_property_name_processor.h"
#include "scene/gui/box_container.h"
#include "scene/gui/scroll_container.h"

class AcceptDialog;
class Button;
class ConfirmationDialog;
class EditorInspector;
class EditorValidationPanel;
class HSeparator;
class LineEdit;
class MarginContainer;
class OptionButton;
class PanelContainer;
class PopupMenu;
class SpinBox;
class StyleBoxFlat;
class TextureRect;

class EditorPropertyRevert {
public:
	static Variant get_property_revert_value(Object *p_object, const StringName &p_property, bool *r_is_valid);
	static bool can_property_revert(Object *p_object, const StringName &p_property, const Variant *p_custom_current_value = nullptr);
};

class EditorProperty : public Container {
	GDCLASS(EditorProperty, Container);

public:
	enum MenuItems {
		MENU_COPY_VALUE,
		MENU_PASTE_VALUE,
		MENU_COPY_PROPERTY_PATH,
		MENU_FAVORITE_PROPERTY,
		MENU_PIN_VALUE,
		MENU_OPEN_DOCUMENTATION,
	};

	enum ColorationMode {
		COLORATION_CONTAINER_RESOURCE,
		COLORATION_RESOURCE,
		COLORATION_EXTERNAL,
	};

private:
	String label;
	int text_size;
	friend class EditorInspector;
	Object *object = nullptr;
	StringName property;
	String property_path;
	String doc_path;
	bool internal = false;
	bool has_doc_tooltip = false;

	int property_usage;

	bool draw_label = true;
	bool draw_background = true;
	bool read_only = false;
	bool checkable = false;
	bool checked = false;
	bool draw_warning = false;
	bool draw_prop_warning = false;
	bool keying = false;
	bool deletable = false;

	Rect2 right_child_rect;
	Rect2 bottom_child_rect;

	Rect2 keying_rect;
	bool keying_hover = false;
	Rect2 revert_rect;
	bool revert_hover = false;
	Rect2 check_rect;
	bool check_hover = false;
	Rect2 delete_rect;
	bool delete_hover = false;

	bool can_revert = false;
	bool can_pin = false;
	bool pin_hidden = false;
	bool pinned = false;

	bool can_favorite = false;
	bool favorited = false;

	bool use_folding = false;
	bool draw_top_bg = true;

	void _update_popup();
	void _focusable_focused(int p_index);

	bool selectable = true;
	bool selected = false;
	int selected_focusable;

	float split_ratio;

	Vector<Control *> focusables;
	Control *label_reference = nullptr;
	Control *bottom_editor = nullptr;
	PopupMenu *menu = nullptr;

	HashMap<StringName, Variant> cache;

	GDVIRTUAL0(_update_property)
	GDVIRTUAL1(_set_read_only, bool)

	void _update_flags();

protected:
	bool has_borders = false;

	void _notification(int p_what);
	static void _bind_methods();
	virtual void _set_read_only(bool p_read_only);

	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;
	const Color *_get_property_colors();

	virtual Variant _get_cache_value(const StringName &p_prop, bool &r_valid) const;
	virtual StringName _get_revert_property() const;

	void _update_property_bg();

public:
	void emit_changed(const StringName &p_property, const Variant &p_value, const StringName &p_field = StringName(), bool p_changing = false);

	String get_tooltip_string(const String &p_string) const;

	virtual Size2 get_minimum_size() const override;

	void set_label(const String &p_label);
	String get_label() const;

	void set_read_only(bool p_read_only);
	bool is_read_only() const;

	void set_draw_label(bool p_draw_label);
	bool is_draw_label() const;

	void set_draw_background(bool p_draw_background);
	bool is_draw_background() const;

	Object *get_edited_object();
	StringName get_edited_property() const;
	inline Variant get_edited_property_value() const {
		ERR_FAIL_NULL_V(object, Variant());
		return object->get(property);
	}
	EditorInspector *get_parent_inspector() const;

	void set_doc_path(const String &p_doc_path);
	void set_internal(bool p_internal);

	virtual void update_property();
	void update_editor_property_status();

	virtual bool use_keying_next() const;

	void set_checkable(bool p_checkable);
	bool is_checkable() const;

	void set_checked(bool p_checked);
	bool is_checked() const;

	void set_draw_warning(bool p_draw_warning);
	bool is_draw_warning() const;

	void set_keying(bool p_keying);
	bool is_keying() const;

	virtual bool is_colored(ColorationMode p_mode) { return false; }

	void set_deletable(bool p_enable);
	bool is_deletable() const;
	void add_focusable(Control *p_control);
	void grab_focus(int p_focusable = -1);
	void select(int p_focusable = -1);
	void deselect();
	bool is_selected() const;

	void set_label_reference(Control *p_control);
	void set_bottom_editor(Control *p_control);

	void set_use_folding(bool p_use_folding);
	bool is_using_folding() const;

	virtual void expand_all_folding();
	virtual void collapse_all_folding();
	virtual void expand_revertable();

	virtual Variant get_drag_data(const Point2 &p_point) override;
	virtual void update_cache();
	virtual bool is_cache_valid() const;

	void set_selectable(bool p_selectable);
	bool is_selectable() const;

	void set_name_split_ratio(float p_ratio);
	float get_name_split_ratio() const;

	void set_favoritable(bool p_favoritable);
	bool is_favoritable() const;

	void set_object_and_property(Object *p_object, const StringName &p_property);
	virtual Control *make_custom_tooltip(const String &p_text) const override;

	void set_draw_top_bg(bool p_draw) { draw_top_bg = p_draw; }

	bool can_revert_to_default() const { return can_revert; }

	void menu_option(int p_option);

	EditorProperty();
};

class EditorInspectorPlugin : public RefCounted {
	GDCLASS(EditorInspectorPlugin, RefCounted);

public:
	friend class EditorInspector;
	struct AddedEditor {
		Control *property_editor = nullptr;
		Vector<String> properties;
		String label;
		bool add_to_end = false;
	};

	List<AddedEditor> added_editors;

protected:
	static void _bind_methods();

	GDVIRTUAL1RC(bool, _can_handle, Object *)
	GDVIRTUAL1(_parse_begin, Object *)
	GDVIRTUAL2(_parse_category, Object *, String)
	GDVIRTUAL2(_parse_group, Object *, String)
	GDVIRTUAL7R(bool, _parse_property, Object *, Variant::Type, String, PropertyHint, String, BitField<PropertyUsageFlags>, bool)
	GDVIRTUAL1(_parse_end, Object *)

#ifndef DISABLE_DEPRECATED
	void _add_property_editor_bind_compat_92322(const String &p_for_property, Control *p_prop, bool p_add_to_end);
	static void _bind_compatibility_methods();
#endif // DISABLE_DEPRECATED
public:
	void add_custom_control(Control *control);
	void add_property_editor(const String &p_for_property, Control *p_prop, bool p_add_to_end = false, const String &p_label = String());
	void add_property_editor_for_multiple_properties(const String &p_label, const Vector<String> &p_properties, Control *p_prop);

	virtual bool can_handle(Object *p_object);
	virtual void parse_begin(Object *p_object);
	virtual void parse_category(Object *p_object, const String &p_category);
	virtual void parse_group(Object *p_object, const String &p_group);
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false);
	virtual void parse_end(Object *p_object);
};

class EditorInspectorCategory : public Control {
	GDCLASS(EditorInspectorCategory, Control);

	friend class EditorInspector;

	// Right-click context menu options.
	enum ClassMenuOption {
		MENU_OPEN_DOCS,
	};

	Ref<Texture2D> icon;
	String label;
	String doc_class_name;
	PopupMenu *menu = nullptr;
	bool is_favorite = false;

	void _handle_menu_option(int p_option);

protected:
	void _notification(int p_what);
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	void set_as_favorite(EditorInspector *p_for_inspector);

	virtual Size2 get_minimum_size() const override;
	virtual Control *make_custom_tooltip(const String &p_text) const override;
};

class EditorInspectorSection : public Container {
	GDCLASS(EditorInspectorSection, Container);

	String label;
	String section;
	bool vbox_added = false; // Optimization.
	Color bg_color;
	bool foldable = false;
	int indent_depth = 0;
	int level = 1;

	Timer *dropping_unfold_timer = nullptr;
	bool dropping_for_unfold = false;

	HashSet<StringName> revertable_properties;

	void _test_unfold();
	int _get_header_height();
	Ref<Texture2D> _get_arrow();

protected:
	Object *object = nullptr;
	VBoxContainer *vbox = nullptr;

	void _notification(int p_what);
	static void _bind_methods();
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

public:
	virtual Size2 get_minimum_size() const override;

	void setup(const String &p_section, const String &p_label, Object *p_object, const Color &p_bg_color, bool p_foldable, int p_indent_depth = 0, int p_level = 1);
	String get_section() const;
	VBoxContainer *get_vbox();
	void unfold();
	void fold();
	void set_bg_color(const Color &p_bg_color);

	bool has_revertable_properties() const;
	void property_can_revert_changed(const String &p_path, bool p_can_revert);

	EditorInspectorSection();
	~EditorInspectorSection();
};

class EditorInspectorArray : public EditorInspectorSection {
	GDCLASS(EditorInspectorArray, EditorInspectorSection);

	enum Mode {
		MODE_NONE,
		MODE_USE_COUNT_PROPERTY,
		MODE_USE_MOVE_ARRAY_ELEMENT_FUNCTION,
	} mode = MODE_NONE;
	StringName count_property;
	StringName array_element_prefix;
	String swap_method;

	int count = 0;

	VBoxContainer *elements_vbox = nullptr;

	Control *control_dropping = nullptr;
	bool dropping = false;

	Button *add_button = nullptr;

	AcceptDialog *resize_dialog = nullptr;
	SpinBox *new_size_spin_box = nullptr;

	// Pagination.
	int page_length = 5;
	int page = 0;
	int max_page = 0;
	int begin_array_index = 0;
	int end_array_index = 0;

	bool read_only = false;
	bool movable = true;
	bool is_const = false;
	bool numbered = false;

	enum MenuOptions {
		OPTION_MOVE_UP = 0,
		OPTION_MOVE_DOWN,
		OPTION_NEW_BEFORE,
		OPTION_NEW_AFTER,
		OPTION_REMOVE,
		OPTION_CLEAR_ARRAY,
		OPTION_RESIZE_ARRAY,
	};
	int popup_array_index_pressed = -1;
	PopupMenu *rmb_popup = nullptr;

	struct ArrayElement {
		PanelContainer *panel = nullptr;
		MarginContainer *margin = nullptr;
		HBoxContainer *hbox = nullptr;
		Button *move_up = nullptr;
		TextureRect *move_texture_rect = nullptr;
		Button *move_down = nullptr;
		Label *number = nullptr;
		VBoxContainer *vbox = nullptr;
		Button *erase = nullptr;
	};
	LocalVector<ArrayElement> array_elements;

	Ref<StyleBoxFlat> odd_style;
	Ref<StyleBoxFlat> even_style;

	int _get_array_count();
	void _add_button_pressed();
	void _paginator_page_changed(int p_page);

	void _rmb_popup_id_pressed(int p_id);

	void _control_dropping_draw();

	void _vbox_visibility_changed();

	void _panel_draw(int p_index);
	void _panel_gui_input(Ref<InputEvent> p_event, int p_index);
	void _move_element(int p_element_index, int p_to_pos);
	void _clear_array();
	void _resize_array(int p_size);
	Array _extract_properties_as_array(const List<PropertyInfo> &p_list);
	int _drop_position() const;

	void _new_size_spin_box_value_changed(float p_value);
	void _new_size_spin_box_text_submitted(const String &p_text);
	void _resize_dialog_confirmed();

	void _update_elements_visibility();
	void _setup();

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;

	void _remove_item(int p_index);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void setup_with_move_element_function(Object *p_object, const String &p_label, const StringName &p_array_element_prefix, int p_page, const Color &p_bg_color, bool p_foldable, bool p_movable = true, bool p_is_const = false, bool p_numbered = false, int p_page_length = 5, const String &p_add_item_text = "");
	void setup_with_count_property(Object *p_object, const String &p_label, const StringName &p_count_property, const StringName &p_array_element_prefix, int p_page, const Color &p_bg_color, bool p_foldable, bool p_movable = true, bool p_is_const = false, bool p_numbered = false, int p_page_length = 5, const String &p_add_item_text = "", const String &p_swap_method = "");
	VBoxContainer *get_vbox(int p_index);

	EditorInspectorArray(bool p_read_only);
};

class EditorPaginator : public HBoxContainer {
	GDCLASS(EditorPaginator, HBoxContainer);

	int page = 0;
	int max_page = 0;
	Button *first_page_button = nullptr;
	Button *prev_page_button = nullptr;
	LineEdit *page_line_edit = nullptr;
	Label *page_count_label = nullptr;
	Button *next_page_button = nullptr;
	Button *last_page_button = nullptr;

	void _first_page_button_pressed();
	void _prev_page_button_pressed();
	void _page_line_edit_text_submitted(const String &p_text);
	void _next_page_button_pressed();
	void _last_page_button_pressed();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void update(int p_page, int p_max_page);

	EditorPaginator();
};

class EditorInspector : public ScrollContainer {
	GDCLASS(EditorInspector, ScrollContainer);

	friend class EditorInspectorCategory;
	friend class EditorPropertyResource;

	enum {
		MAX_PLUGINS = 1024
	};
	static Ref<EditorInspectorPlugin> inspector_plugins[MAX_PLUGINS];
	static int inspector_plugin_count;

	// Right-click context menu options.
	enum ClassMenuOption {
		MENU_UNFAVORITE_ALL,
	};

	bool can_favorite = false;
	PackedStringArray current_favorites;
	VBoxContainer *favorites_section = nullptr;
	EditorInspectorCategory *favorites_category = nullptr;
	VBoxContainer *favorites_vbox = nullptr;
	VBoxContainer *favorites_groups_vbox = nullptr;
	HSeparator *favorites_separator = nullptr;

	EditorInspector *root_inspector = nullptr;

	VBoxContainer *base_vbox = nullptr;
	VBoxContainer *begin_vbox = nullptr;
	VBoxContainer *main_vbox = nullptr;

	// Map used to cache the instantiated editors.
	HashMap<StringName, List<EditorProperty *>> editor_property_map;
	List<EditorInspectorSection *> sections;
	HashSet<StringName> pending;

	void _clear(bool p_hide_plugins = true);
	Object *object = nullptr;
	Object *next_object = nullptr;

	//

	LineEdit *search_box = nullptr;
	bool show_standard_categories = false;
	bool show_custom_categories = false;
	bool hide_script = true;
	bool hide_metadata = true;
	bool use_doc_hints = false;
	EditorPropertyNameProcessor::Style property_name_style = EditorPropertyNameProcessor::STYLE_CAPITALIZED;
	bool use_settings_name_style = true;
	bool use_filter = false;
	bool autoclear = false;
	bool use_folding = false;
	int changing;
	bool update_all_pending = false;
	bool read_only = false;
	bool keying = false;
	bool wide_editors = false;
	bool deletable_properties = false;

	float refresh_countdown;
	bool update_tree_pending = false;
	StringName _prop_edited;
	StringName property_selected;
	int property_focusable;
	int update_scroll_request;

	struct DocCacheInfo {
		String doc_path;
		String theme_item_name;
	};

	HashMap<StringName, HashMap<StringName, DocCacheInfo>> doc_cache;
	HashSet<StringName> restart_request_props;
	HashMap<String, String> custom_property_descriptions;

	HashMap<ObjectID, int> scroll_cache;

	String property_prefix; // Used for sectioned inspector.
	String object_class;
	Variant property_clipboard;

	bool restrict_to_basic = false;

	void _edit_set(const String &p_name, const Variant &p_value, bool p_refresh_all, const String &p_changed_field);

	void _property_changed(const String &p_path, const Variant &p_value, const String &p_name = "", bool p_changing = false, bool p_update_all = false);
	void _multiple_properties_changed(const Vector<String> &p_paths, const Array &p_values, bool p_changing = false);
	void _property_keyed(const String &p_path, bool p_advance);
	void _property_keyed_with_value(const String &p_path, const Variant &p_value, bool p_advance);
	void _property_deleted(const String &p_path);
	void _property_checked(const String &p_path, bool p_checked);
	void _property_pinned(const String &p_path, bool p_pinned);
	bool _property_path_matches(const String &p_property_path, const String &p_filter, EditorPropertyNameProcessor::Style p_style);
	bool _resource_properties_matches(const Ref<Resource> &p_resource, const String &p_filter);

	void _resource_selected(const String &p_path, Ref<Resource> p_resource);
	void _property_selected(const String &p_path, int p_focusable);
	void _object_id_selected(const String &p_path, ObjectID p_id);

	void _update_current_favorites();
	void _set_property_favorited(const String &p_path, bool p_favorited);
	void _clear_current_favorites();

	void _node_removed(Node *p_node);

	HashMap<StringName, int> per_array_page;
	void _page_change_request(int p_new_page, const StringName &p_array_prefix);

	void _changed_callback();
	void _edit_request_change(Object *p_object, const String &p_prop);

	void _keying_changed();

	void _parse_added_editors(VBoxContainer *current_vbox, EditorInspectorSection *p_section, Ref<EditorInspectorPlugin> ped);

	void _vscroll_changed(double);

	void _feature_profile_changed();

	bool _is_property_disabled_by_feature_profile(const StringName &p_property);

	AddMetadataDialog *add_meta_dialog = nullptr;
	LineEdit *add_meta_name = nullptr;
	OptionButton *add_meta_type = nullptr;
	EditorValidationPanel *validation_panel = nullptr;

	void _add_meta_confirm();
	void _show_add_meta_dialog();

	void _handle_menu_option(int p_option);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	static void add_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin);
	static void remove_inspector_plugin(const Ref<EditorInspectorPlugin> &p_plugin);
	static void cleanup_plugins();
	static Button *create_inspector_action_button(const String &p_text);

	static EditorProperty *instantiate_property_editor(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const uint32_t p_usage, const bool p_wide = false);

	bool is_main_editor_inspector() const;
	String get_selected_path() const;

	void update_tree();
	void update_property(const String &p_prop);
	void edit(Object *p_object);
	Object *get_edited_object();
	Object *get_next_edited_object();

	void set_keying(bool p_active);
	void set_read_only(bool p_read_only);

	EditorPropertyNameProcessor::Style get_property_name_style() const;
	void set_property_name_style(EditorPropertyNameProcessor::Style p_style);

	// If true, the inspector will update its property name style according to the current editor settings.
	void set_use_settings_name_style(bool p_enable);

	void set_autoclear(bool p_enable);

	void set_show_categories(bool p_show_standard, bool p_show_custom);
	void set_use_doc_hints(bool p_enable);
	void set_hide_script(bool p_hide);
	void set_hide_metadata(bool p_hide);

	void set_use_filter(bool p_use);
	void register_text_enter(Node *p_line_edit);

	void set_use_folding(bool p_use_folding, bool p_update_tree = true);
	bool is_using_folding();

	void collapse_all_folding();
	void expand_all_folding();
	void expand_revertable();

	void set_scroll_offset(int p_offset);
	int get_scroll_offset() const;

	void set_property_prefix(const String &p_prefix);
	String get_property_prefix() const;

	void add_custom_property_description(const String &p_class, const String &p_property, const String &p_description);
	String get_custom_property_description(const String &p_property) const;

	void set_object_class(const String &p_class);
	String get_object_class() const;

	void set_use_wide_editors(bool p_enable);
	void set_root_inspector(EditorInspector *p_root_inspector);
	EditorInspector *get_root_inspector() { return is_sub_inspector() ? root_inspector : this; }
	bool is_sub_inspector() const { return root_inspector != nullptr; }

	void set_use_deletable_properties(bool p_enabled);

	void set_restrict_to_basic_settings(bool p_restrict);
	void set_property_clipboard(const Variant &p_value);
	Variant get_property_clipboard() const;

	EditorInspector();
};

#endif // EDITOR_INSPECTOR_H
