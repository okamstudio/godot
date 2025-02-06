/**************************************************************************/
/*  theme_editor_preview.cpp                                              */
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

#include "theme_editor_preview.h"

#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/check_button.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"
#include "scene/resources/packed_scene.h"
#include "scene/theme/theme_db.h"

constexpr double REFRESH_TIMER = 1.5;

void ThemeEditorPreview::set_preview_theme(const Ref<Theme> &p_theme) {
	preview_content->set_theme(p_theme);
}

void ThemeEditorPreview::add_preview_overlay(Control *p_overlay) {
	preview_overlay->add_child(p_overlay);
	p_overlay->hide();
}

void ThemeEditorPreview::_propagate_redraw(Control *p_at) {
	p_at->notification(NOTIFICATION_THEME_CHANGED);
	p_at->update_minimum_size();
	p_at->queue_redraw();
	for (int i = 0; i < p_at->get_child_count(); i++) {
		Control *a = Object::cast_to<Control>(p_at->get_child(i));
		if (a) {
			_propagate_redraw(a);
		}
	}
}

void ThemeEditorPreview::_refresh_interval() {
	// In case the project settings have changed.
	preview_bg->set_color(GLOBAL_GET("rendering/environment/defaults/default_clear_color"));

	_propagate_redraw(preview_bg);
	_propagate_redraw(preview_content);
}

void ThemeEditorPreview::_preview_visibility_changed() {
	set_process(is_visible_in_tree());
}

void ThemeEditorPreview::_picker_button_cbk() {
	picker_overlay->set_visible(picker_button->is_pressed());
	if (picker_button->is_pressed()) {
		_reset_picker_overlay();
	}
}

Control *ThemeEditorPreview::_find_hovered_control(Control *p_parent, Vector2 p_mouse_position) {
	Control *found = nullptr;

	for (int i = p_parent->get_child_count() - 1; i >= 0; i--) {
		Control *cc = Object::cast_to<Control>(p_parent->get_child(i));
		if (!cc || !cc->is_visible()) {
			continue;
		}

		Rect2 crect = cc->get_rect();
		if (crect.has_point(p_mouse_position)) {
			// Check if there is a child control under mouse.
			if (cc->get_child_count() > 0) {
				found = _find_hovered_control(cc, p_mouse_position - cc->get_position());
			}

			// If there are no applicable children, use the control itself.
			if (!found) {
				found = cc;
			}
			break;
		}
	}

	return found;
}

void ThemeEditorPreview::_draw_picker_overlay() {
	if (!picker_button->is_pressed()) {
		return;
	}

	picker_overlay->draw_rect(Rect2(Vector2(0.0, 0.0), picker_overlay->get_size()), theme_cache.preview_picker_overlay_color);
	if (hovered_control) {
		Rect2 highlight_rect = hovered_control->get_global_rect();
		highlight_rect.position = picker_overlay->get_global_transform().affine_inverse().xform(highlight_rect.position);
		picker_overlay->draw_style_box(theme_cache.preview_picker_overlay, highlight_rect);

		String highlight_name = hovered_control->get_theme_type_variation();
		if (highlight_name == StringName()) {
			highlight_name = hovered_control->get_class_name();
		}

		Rect2 highlight_label_rect = highlight_rect;
		highlight_label_rect.size = theme_cache.preview_picker_font->get_string_size(highlight_name, HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size);

		int margin_top = theme_cache.preview_picker_label->get_margin(SIDE_TOP);
		int margin_left = theme_cache.preview_picker_label->get_margin(SIDE_LEFT);
		int margin_bottom = theme_cache.preview_picker_label->get_margin(SIDE_BOTTOM);
		int margin_right = theme_cache.preview_picker_label->get_margin(SIDE_RIGHT);
		highlight_label_rect.size.x += margin_left + margin_right;
		highlight_label_rect.size.y += margin_top + margin_bottom;

		highlight_label_rect.position = highlight_label_rect.position.clamp(Vector2(), picker_overlay->get_size());

		picker_overlay->draw_style_box(theme_cache.preview_picker_label, highlight_label_rect);

		Point2 label_pos = highlight_label_rect.position;
		label_pos.y += highlight_label_rect.size.y - margin_bottom;
		label_pos.x += margin_left;
		picker_overlay->draw_string(theme_cache.preview_picker_font, label_pos, highlight_name, HORIZONTAL_ALIGNMENT_LEFT, -1, theme_cache.font_size);
	}
}

void ThemeEditorPreview::_gui_input_picker_overlay(const Ref<InputEvent> &p_event) {
	if (!picker_button->is_pressed()) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		if (hovered_control) {
			StringName theme_type = hovered_control->get_theme_type_variation();
			if (theme_type == StringName()) {
				theme_type = hovered_control->get_class_name();
			}

			emit_signal(SNAME("control_picked"), theme_type);
			picker_button->set_pressed(false);
			picker_overlay->set_visible(false);
			return;
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		Vector2 mp = preview_content->get_local_mouse_position();
		hovered_control = _find_hovered_control(preview_content, mp);
		picker_overlay->queue_redraw();
	}

	// Forward input to the scroll container underneath to allow scrolling.
	preview_container->gui_input(p_event);
}

void ThemeEditorPreview::_reset_picker_overlay() {
	hovered_control = nullptr;
	picker_overlay->queue_redraw();
}

void ThemeEditorPreview::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (is_visible_in_tree()) {
				set_process(true);
			}

			connect(SceneStringName(visibility_changed), callable_mp(this, &ThemeEditorPreview::_preview_visibility_changed));
		} break;

		case NOTIFICATION_READY: {
			Vector<Ref<Theme>> preview_themes;
			preview_themes.push_back(ThemeDB::get_singleton()->get_default_theme());
			ThemeDB::get_singleton()->create_theme_context(preview_root, preview_themes);
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			picker_button->set_button_icon(get_editor_theme_icon(SNAME("ColorPick")));

			theme_cache.preview_picker_overlay = get_theme_stylebox(SNAME("preview_picker_overlay"), SNAME("ThemeEditor"));
			theme_cache.preview_picker_overlay_color = get_theme_color(SNAME("preview_picker_overlay_color"), SNAME("ThemeEditor"));
			theme_cache.preview_picker_label = get_theme_stylebox(SNAME("preview_picker_label"), SNAME("ThemeEditor"));
			theme_cache.preview_picker_font = get_theme_font(SNAME("status_source"), EditorStringName(EditorFonts));
			theme_cache.font_size = get_theme_default_font_size();
		} break;

		case NOTIFICATION_PROCESS: {
			time_left -= get_process_delta_time();
			if (time_left < 0) {
				time_left = REFRESH_TIMER;
				_refresh_interval();
			}
		} break;
	}
}

void ThemeEditorPreview::_bind_methods() {
	ADD_SIGNAL(MethodInfo("control_picked", PropertyInfo(Variant::STRING, "class_name")));
}

ThemeEditorPreview::ThemeEditorPreview() {
	preview_toolbar = memnew(HBoxContainer);
	add_child(preview_toolbar);

	picker_button = memnew(Button);
	preview_toolbar->add_child(picker_button);
	picker_button->set_theme_type_variation(SceneStringName(FlatButton));
	picker_button->set_toggle_mode(true);
	picker_button->set_tooltip_text(TTR("Toggle the control picker, allowing to visually select control types for edit."));
	picker_button->connect(SceneStringName(pressed), callable_mp(this, &ThemeEditorPreview::_picker_button_cbk));

	MarginContainer *preview_body = memnew(MarginContainer);
	preview_body->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	preview_body->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(preview_body);

	preview_container = memnew(ScrollContainer);
	preview_body->add_child(preview_container);

	preview_root = memnew(MarginContainer);
	preview_container->add_child(preview_root);
	preview_root->set_clip_contents(true);
	preview_root->set_custom_minimum_size(Size2(450, 0) * EDSCALE);
	preview_root->set_v_size_flags(SIZE_EXPAND_FILL);
	preview_root->set_h_size_flags(SIZE_EXPAND_FILL);

	preview_bg = memnew(ColorRect);
	preview_bg->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	preview_bg->set_color(GLOBAL_GET("rendering/environment/defaults/default_clear_color"));
	preview_root->add_child(preview_bg);

	preview_content = memnew(MarginContainer);
	preview_content->add_theme_constant_override("margin_right", 4 * EDSCALE);
	preview_content->add_theme_constant_override("margin_top", 4 * EDSCALE);
	preview_content->add_theme_constant_override("margin_left", 4 * EDSCALE);
	preview_content->add_theme_constant_override("margin_bottom", 4 * EDSCALE);
	preview_root->add_child(preview_content);

	preview_overlay = memnew(MarginContainer);
	preview_overlay->set_mouse_filter(MOUSE_FILTER_IGNORE);
	preview_overlay->set_clip_contents(true);
	preview_body->add_child(preview_overlay);

	picker_overlay = memnew(Control);
	add_preview_overlay(picker_overlay);
	picker_overlay->connect(SceneStringName(draw), callable_mp(this, &ThemeEditorPreview::_draw_picker_overlay));
	picker_overlay->connect(SceneStringName(gui_input), callable_mp(this, &ThemeEditorPreview::_gui_input_picker_overlay));
	picker_overlay->connect(SceneStringName(mouse_exited), callable_mp(this, &ThemeEditorPreview::_reset_picker_overlay));
}

void DefaultThemeEditorPreview::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			test_color_picker_button->set_custom_minimum_size(Size2(0, get_theme_constant(SNAME("color_picker_button_height"), EditorStringName(Editor))));
		} break;
	}
}

DefaultThemeEditorPreview::DefaultThemeEditorPreview() {
	Panel *main_panel = memnew(Panel);
	preview_content->add_child(main_panel);

	MarginContainer *main_mc = memnew(MarginContainer);
	main_mc->add_theme_constant_override("margin_right", 4 * EDSCALE);
	main_mc->add_theme_constant_override("margin_top", 4 * EDSCALE);
	main_mc->add_theme_constant_override("margin_left", 4 * EDSCALE);
	main_mc->add_theme_constant_override("margin_bottom", 4 * EDSCALE);
	preview_content->add_child(main_mc);

	HBoxContainer *main_hb = memnew(HBoxContainer);
	main_mc->add_child(main_hb);
	main_hb->add_theme_constant_override("separation", 20 * EDSCALE);

	VBoxContainer *first_vb = memnew(VBoxContainer);
	main_hb->add_child(first_vb);
	first_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	first_vb->add_theme_constant_override("separation", 10 * EDSCALE);

	first_vb->add_child(memnew(Label("Label")));

	first_vb->add_child(memnew(Button("Button")));
	Button *bt = memnew(Button);
	bt->set_text(TTR("Toggle Button"));
	bt->set_toggle_mode(true);
	bt->set_pressed(true);
	first_vb->add_child(bt);
	bt = memnew(Button);
	bt->set_text(TTR("Disabled Button"));
	bt->set_disabled(true);
	first_vb->add_child(bt);
	Button *tb = memnew(Button);
	tb->set_flat(true);
	tb->set_text("Flat Button");
	first_vb->add_child(tb);

	CheckButton *cb = memnew(CheckButton);
	cb->set_text("CheckButton");
	first_vb->add_child(cb);
	CheckBox *cbx = memnew(CheckBox);
	cbx->set_text("CheckBox");
	first_vb->add_child(cbx);

	MenuButton *test_menu_button = memnew(MenuButton);
	test_menu_button->set_text("MenuButton");
	test_menu_button->get_popup()->add_item(TTR("Item"));
	test_menu_button->get_popup()->add_item(TTR("Disabled Item"));
	test_menu_button->get_popup()->set_item_disabled(1, true);
	test_menu_button->get_popup()->add_separator();
	test_menu_button->get_popup()->add_check_item(TTR("Check Item"));
	test_menu_button->get_popup()->add_check_item(TTR("Checked Item"));
	test_menu_button->get_popup()->set_item_checked(4, true);
	test_menu_button->get_popup()->add_separator();
	test_menu_button->get_popup()->add_radio_check_item(TTR("Radio Item"));
	test_menu_button->get_popup()->add_radio_check_item(TTR("Checked Radio Item"));
	test_menu_button->get_popup()->set_item_checked(7, true);
	test_menu_button->get_popup()->add_separator(TTR("Named Separator"));

	PopupMenu *test_submenu = memnew(PopupMenu);
	test_menu_button->get_popup()->add_submenu_node_item(TTR("Submenu"), test_submenu);
	test_submenu->add_item(TTR("Subitem 1"));
	test_submenu->add_item(TTR("Subitem 2"));
	first_vb->add_child(test_menu_button);

	OptionButton *test_option_button = memnew(OptionButton);
	test_option_button->add_item("OptionButton");
	test_option_button->add_separator();
	test_option_button->add_item(TTR("Has"));
	test_option_button->add_item(TTR("Many"));
	test_option_button->add_item(TTR("Options"));
	first_vb->add_child(test_option_button);
	test_color_picker_button = memnew(ColorPickerButton);
	first_vb->add_child(test_color_picker_button);

	VBoxContainer *second_vb = memnew(VBoxContainer);
	second_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	main_hb->add_child(second_vb);
	second_vb->add_theme_constant_override("separation", 10 * EDSCALE);
	LineEdit *le = memnew(LineEdit);
	le->set_text("LineEdit");
	second_vb->add_child(le);
	le = memnew(LineEdit);
	le->set_text(TTR("Disabled LineEdit"));
	le->set_editable(false);
	second_vb->add_child(le);
	TextEdit *te = memnew(TextEdit);
	te->set_text("TextEdit");
	te->set_custom_minimum_size(Size2(0, 100) * EDSCALE);
	second_vb->add_child(te);
	second_vb->add_child(memnew(SpinBox));

	HBoxContainer *vhb = memnew(HBoxContainer);
	second_vb->add_child(vhb);
	vhb->set_custom_minimum_size(Size2(0, 100) * EDSCALE);
	vhb->add_child(memnew(VSlider));
	VScrollBar *vsb = memnew(VScrollBar);
	vsb->set_page(25);
	vhb->add_child(vsb);
	vhb->add_child(memnew(VSeparator));
	VBoxContainer *hvb = memnew(VBoxContainer);
	vhb->add_child(hvb);
	hvb->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	hvb->set_h_size_flags(SIZE_EXPAND_FILL);
	hvb->add_child(memnew(HSlider));
	HScrollBar *hsb = memnew(HScrollBar);
	hsb->set_page(25);
	hvb->add_child(hsb);
	HSlider *hs = memnew(HSlider);
	hs->set_editable(false);
	hvb->add_child(hs);
	hvb->add_child(memnew(HSeparator));
	ProgressBar *pb = memnew(ProgressBar);
	pb->set_value(50);
	hvb->add_child(pb);

	VBoxContainer *third_vb = memnew(VBoxContainer);
	third_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	third_vb->add_theme_constant_override("separation", 10 * EDSCALE);
	main_hb->add_child(third_vb);

	TabContainer *tc = memnew(TabContainer);
	third_vb->add_child(tc);
	tc->set_custom_minimum_size(Size2(0, 135) * EDSCALE);
	Control *tcc = memnew(Control);
	tcc->set_name(TTR("Tab 1"));
	tc->add_child(tcc);
	tcc = memnew(Control);
	tcc->set_name(TTR("Tab 2"));
	tc->add_child(tcc);
	tcc = memnew(Control);
	tcc->set_name(TTR("Tab 3"));
	tc->add_child(tcc);
	tc->set_tab_disabled(2, true);

	Tree *test_tree = memnew(Tree);
	third_vb->add_child(test_tree);
	test_tree->set_custom_minimum_size(Size2(0, 175) * EDSCALE);

	TreeItem *item = test_tree->create_item();
	item->set_text(0, "Tree");
	item = test_tree->create_item(test_tree->get_root());
	item->set_text(0, "Item");
	item = test_tree->create_item(test_tree->get_root());
	item->set_editable(0, true);
	item->set_text(0, TTR("Editable Item"));
	TreeItem *sub_tree = test_tree->create_item(test_tree->get_root());
	sub_tree->set_text(0, TTR("Subtree"));
	item = test_tree->create_item(sub_tree);
	item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
	item->set_editable(0, true);
	item->set_text(0, "Check Item");
	item = test_tree->create_item(sub_tree);
	item->set_cell_mode(0, TreeItem::CELL_MODE_RANGE);
	item->set_editable(0, true);
	item->set_range_config(0, 0, 20, 0.1);
	item->set_range(0, 2);
	item = test_tree->create_item(sub_tree);
	item->set_cell_mode(0, TreeItem::CELL_MODE_RANGE);
	item->set_editable(0, true);
	item->set_text(0, TTR("Has,Many,Options"));
	item->set_range(0, 2);
}

void SceneThemeEditorPreview::_reload_scene() {
	if (loaded_scene.is_null()) {
		return;
	}

	if (loaded_scene->get_path().is_empty() || !ResourceLoader::exists(loaded_scene->get_path())) {
		EditorNode::get_singleton()->show_warning(TTR("Invalid path, the PackedScene resource was probably moved or removed."));
		emit_signal(SNAME("scene_invalidated"));
		return;
	}

	for (int i = preview_content->get_child_count() - 1; i >= 0; i--) {
		Node *node = preview_content->get_child(i);
		node->queue_free();
		preview_content->remove_child(node);
	}

	Node *instance = loaded_scene->instantiate();
	if (!instance || !Object::cast_to<Control>(instance)) {
		EditorNode::get_singleton()->show_warning(TTR("Invalid PackedScene resource, must have a Control node at its root."));
		emit_signal(SNAME("scene_invalidated"));
		return;
	}

	preview_content->add_child(instance);
	emit_signal(SNAME("scene_reloaded"));
}

void SceneThemeEditorPreview::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			reload_scene_button->set_button_icon(get_editor_theme_icon(SNAME("Reload")));
		} break;
	}
}

void SceneThemeEditorPreview::_bind_methods() {
	ADD_SIGNAL(MethodInfo("scene_invalidated"));
	ADD_SIGNAL(MethodInfo("scene_reloaded"));
}

bool SceneThemeEditorPreview::set_preview_scene(const String &p_path) {
	loaded_scene = ResourceLoader::load(p_path);
	if (loaded_scene.is_null()) {
		EditorNode::get_singleton()->show_warning(TTR("Invalid file, not a PackedScene resource."));
		return false;
	}

	Node *instance = loaded_scene->instantiate();
	if (!instance || !Object::cast_to<Control>(instance)) {
		EditorNode::get_singleton()->show_warning(TTR("Invalid PackedScene resource, must have a Control node at its root."));
		return false;
	}

	preview_content->add_child(instance);
	return true;
}

String SceneThemeEditorPreview::get_preview_scene_path() const {
	if (loaded_scene.is_null()) {
		return "";
	}

	return loaded_scene->get_path();
}

SceneThemeEditorPreview::SceneThemeEditorPreview() {
	preview_toolbar->add_child(memnew(VSeparator));

	reload_scene_button = memnew(Button);
	reload_scene_button->set_flat(true);
	reload_scene_button->set_tooltip_text(TTR("Reload the scene to reflect its most actual state."));
	preview_toolbar->add_child(reload_scene_button);
	reload_scene_button->connect(SceneStringName(pressed), callable_mp(this, &SceneThemeEditorPreview::_reload_scene));
}
