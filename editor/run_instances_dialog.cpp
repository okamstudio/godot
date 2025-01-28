/**************************************************************************/
/*  run_instances_dialog.cpp                                              */
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

#include "run_instances_dialog.h"

#include "core/config/project_settings.h"
#include "editor/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_box.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/tree.h"
#include "scene/main/timer.h"

void RunInstancesDialog::_fetch_main_args() {
	if (!main_args_edit->has_focus()) { // Only set the text if the user is not currently editing it.
		main_args_edit->set_text(GLOBAL_GET("editor/run/main_run_args"));
	}
}

void RunInstancesDialog::_start_main_timer() {
	main_apply_timer->start();
}

void RunInstancesDialog::_start_instance_timer() {
	instance_apply_timer->start();
}

void RunInstancesDialog::_refresh_argument_count() {
	instance_tree->clear();
	instance_tree->create_item(); // Root.

	while (instance_count->get_value() > stored_data.size()) {
		stored_data.append(Dictionary());
	}

	instances_data.resize(instance_count->get_value());
	InstanceData *instances_write = instances_data.ptrw();

	for (int i = 0; i < instances_data.size(); i++) {
		InstanceData instance;
		const Dictionary &instance_data = stored_data[i];

		_create_instance(instance, instance_data, i + 1);
		instances_write[i] = instance;
	}
}

void RunInstancesDialog::_create_instance(InstanceData &p_instance, const Dictionary &p_data, int p_idx) {
	TreeItem *instance_item = instance_tree->create_item();
	p_instance.item = instance_item;

	instance_item->set_cell_mode(COLUMN_OVERRIDE_ARGS, TreeItem::CELL_MODE_CHECK);
	instance_item->set_editable(COLUMN_OVERRIDE_ARGS, true);
	instance_item->set_text(COLUMN_OVERRIDE_ARGS, TTR("Enabled"));
	instance_item->set_checked(COLUMN_OVERRIDE_ARGS, p_data.get("override_args", false));

	instance_item->set_editable(COLUMN_LAUNCH_ARGUMENTS, true);
	instance_item->set_text(COLUMN_LAUNCH_ARGUMENTS, p_data.get("arguments", String()));

	instance_item->set_cell_mode(COLUMN_OVERRIDE_FEATURES, TreeItem::CELL_MODE_CHECK);
	instance_item->set_editable(COLUMN_OVERRIDE_FEATURES, true);
	instance_item->set_text(COLUMN_OVERRIDE_FEATURES, TTR("Enabled"));
	instance_item->set_checked(COLUMN_OVERRIDE_FEATURES, p_data.get("override_features", false));

	instance_item->set_editable(COLUMN_FEATURE_TAGS, true);
	instance_item->set_text(COLUMN_FEATURE_TAGS, p_data.get("features", String()));
}

void RunInstancesDialog::_save_main_args() {
	ProjectSettings::get_singleton()->set_setting("editor/run/main_run_args", main_args_edit->get_text());
	ProjectSettings::get_singleton()->save();
	EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_main_feature_tags", main_features_edit->get_text());
	EditorSettings::get_singleton()->set_project_metadata("debug_options", "multiple_instances_enabled", enable_multiple_instances_checkbox->is_pressed());
}

void RunInstancesDialog::_save_arguments() {
	for (int i = 0; i < instances_data.size(); i++) {
		const InstanceData &instance = instances_data[i];
		Dictionary dict;
		dict["override_args"] = instance.overrides_run_args();
		dict["arguments"] = instance.get_launch_arguments();
		dict["override_features"] = instance.overrides_features();
		dict["features"] = instance.get_feature_tags();
		stored_data[i] = dict;
	}
	EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_instances_config", stored_data);
	EditorSettings::get_singleton()->set_project_metadata("debug_options", "run_instance_count", instance_count->get_value());
}

Vector<String> RunInstancesDialog::_split_cmdline_args(const String &p_arg_string) const {
	Vector<String> split_args;
	int arg_start = 0;
	bool is_quoted = false;
	char32_t quote_char = '-';
	char32_t arg_char;
	int arg_length;
	for (int i = 0; i < p_arg_string.length(); i++) {
		arg_char = p_arg_string[i];
		if (arg_char == '\"' || arg_char == '\'') {
			if (i == 0 || p_arg_string[i - 1] != '\\') {
				if (is_quoted) {
					if (arg_char == quote_char) {
						is_quoted = false;
						quote_char = '-';
					}
				} else {
					is_quoted = true;
					quote_char = arg_char;
				}
			}
		} else if (!is_quoted && arg_char == ' ') {
			arg_length = i - arg_start;
			if (arg_length > 0) {
				split_args.push_back(p_arg_string.substr(arg_start, arg_length));
			}
			arg_start = i + 1;
		}
	}
	arg_length = p_arg_string.length() - arg_start;
	if (arg_length > 0) {
		split_args.push_back(p_arg_string.substr(arg_start, arg_length));
	}
	return split_args;
}

void RunInstancesDialog::_instance_menu_id_pressed(int p_option) {
	switch (p_option) {
		case CLEAR_ITEM: {
			int item_to_clear = popup_menu->get_item_metadata(0);
			if (item_to_clear >= 0 && item_to_clear < stored_data.size()) {
				stored_data[item_to_clear] = Dictionary();
			}
		} break;
		case CLEAR_ALL: {
			stored_data.clear();
			stored_data.resize(instance_count->get_value());
		} break;
	}

	_start_instance_timer();
	_refresh_argument_count();
}

void RunInstancesDialog::_instance_tree_rmb(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}

	popup_menu->clear();
	popup_menu->add_item(TTR("Clear"), CLEAR_ITEM);

	TreeItem *item = instance_tree->get_item_at_position(p_pos);
	if (item) {
		popup_menu->set_item_metadata(0, item->get_index());
	} else {
		popup_menu->set_item_disabled(0, true);
	}

	popup_menu->add_item(TTR("Clear All"), CLEAR_ALL);
	popup_menu->set_position(instance_tree->get_screen_position() + p_pos);
	popup_menu->reset_size();
	popup_menu->popup();
}

void RunInstancesDialog::popup_dialog() {
	popup_centered(Vector2(1200, 600) * EDSCALE);
}

int RunInstancesDialog::get_instance_count() const {
	if (enable_multiple_instances_checkbox->is_pressed()) {
		return instance_count->get_value();
	} else {
		return 1;
	}
}

void RunInstancesDialog::get_argument_list_for_instance(int p_idx, List<String> &r_list) const {
	bool override_args = instances_data[p_idx].overrides_run_args();
	bool use_multiple_instances = enable_multiple_instances_checkbox->is_pressed();
	String raw_custom_args;

	if (use_multiple_instances) {
		if (override_args) {
			raw_custom_args = instances_data[p_idx].get_launch_arguments();
		} else {
			raw_custom_args = main_args_edit->get_text() + " " + instances_data[p_idx].get_launch_arguments();
		}
	} else {
		raw_custom_args = main_args_edit->get_text();
	}

	String exec = OS::get_singleton()->get_executable_path();

	if (!raw_custom_args.is_empty()) {
		// Allow the user to specify a command to run, similar to Steam's launch options.
		// In this case, Godot will no longer be run directly; it's up to the underlying command
		// to run it. For instance, this can be used on Linux to force a running project
		// to use Optimus using `prime-run` or similar.
		// Example: `prime-run %command% --time-scale 0.5`
		const int placeholder_pos = raw_custom_args.find("%command%");

		Vector<String> custom_args;

		if (placeholder_pos != -1) {
			// Prepend executable-specific custom arguments.
			// If nothing is placed before `%command%`, behave as if no placeholder was specified.
			Vector<String> exec_args = _split_cmdline_args(raw_custom_args.substr(0, placeholder_pos));
			if (exec_args.size() > 0) {
				exec = exec_args[0];
				exec_args.remove_at(0);

				// Append the Godot executable name before we append executable arguments
				// (since the order is reversed when using `push_front()`).
				r_list.push_front(OS::get_singleton()->get_executable_path());
			}

			for (int i = exec_args.size() - 1; i >= 0; i--) {
				// Iterate backwards as we're pushing items in the reverse order.
				r_list.push_front(exec_args[i].replace(" ", "%20"));
			}

			// Append Godot-specific custom arguments.
			custom_args = _split_cmdline_args(raw_custom_args.substr(placeholder_pos + String("%command%").size()));
			for (int i = 0; i < custom_args.size(); i++) {
				r_list.push_back(custom_args[i].replace(" ", "%20"));
			}
		} else {
			// Append Godot-specific custom arguments.
			custom_args = _split_cmdline_args(raw_custom_args);
			for (int i = 0; i < custom_args.size(); i++) {
				r_list.push_back(custom_args[i].replace(" ", "%20"));
			}
		}
	}
}

void RunInstancesDialog::apply_custom_features(int p_instance_idx) {
	const InstanceData &instance = instances_data[p_instance_idx];

	String raw_text;
	if (enable_multiple_instances_checkbox->is_pressed()) {
		if (instance.overrides_features()) {
			raw_text = instance.get_feature_tags();
		} else {
			raw_text = main_features_edit->get_text() + "," + instance.get_feature_tags();
		}
	} else {
		raw_text = main_features_edit->get_text();
	}

	const Vector<String> raw_list = raw_text.split(",");
	Vector<String> stripped_features;

	for (int i = 0; i < raw_list.size(); i++) {
		String f = raw_list[i].strip_edges();
		if (!f.is_empty()) {
			stripped_features.push_back(f);
		}
	}
	OS::get_singleton()->set_environment("GODOT_EDITOR_CUSTOM_FEATURES", String(",").join(stripped_features));
}

RunInstancesDialog::RunInstancesDialog() {
	singleton = this;
	set_title(TTR("Run Instances"));

	main_apply_timer = memnew(Timer);
	main_apply_timer->set_wait_time(0.5);
	main_apply_timer->set_one_shot(true);
	add_child(main_apply_timer);
	main_apply_timer->connect("timeout", callable_mp(this, &RunInstancesDialog::_save_main_args));

	instance_apply_timer = memnew(Timer);
	instance_apply_timer->set_wait_time(0.5);
	instance_apply_timer->set_one_shot(true);
	add_child(instance_apply_timer);
	instance_apply_timer->connect("timeout", callable_mp(this, &RunInstancesDialog::_save_arguments));

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);

	GridContainer *args_gc = memnew(GridContainer);
	args_gc->set_columns(3);
	args_gc->add_theme_constant_override("h_separation", 12 * EDSCALE);
	main_vb->add_child(args_gc);

	enable_multiple_instances_checkbox = memnew(CheckBox);
	enable_multiple_instances_checkbox->set_text(TTR("Enable Multiple Instances"));
	enable_multiple_instances_checkbox->set_pressed(EditorSettings::get_singleton()->get_project_metadata("debug_options", "multiple_instances_enabled", false));
	args_gc->add_child(enable_multiple_instances_checkbox);
	enable_multiple_instances_checkbox->connect(SceneStringName(pressed), callable_mp(this, &RunInstancesDialog::_start_main_timer));

	{
		Label *l = memnew(Label);
		l->set_text(TTR("Main Run Args:"));
		args_gc->add_child(l);
	}

	{
		Label *l = memnew(Label);
		l->set_text(TTR("Main Feature Tags:"));
		args_gc->add_child(l);
	}

	stored_data = TypedArray<Dictionary>(EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_instances_config", TypedArray<Dictionary>()));

	instance_count = memnew(SpinBox);
	instance_count->set_min(1);
	instance_count->set_max(20);
	instance_count->set_value(EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_instance_count", stored_data.size()));

	args_gc->add_child(instance_count);
	instance_count->connect(SceneStringName(value_changed), callable_mp(this, &RunInstancesDialog::_start_instance_timer).unbind(1));
	instance_count->connect(SceneStringName(value_changed), callable_mp(this, &RunInstancesDialog::_refresh_argument_count).unbind(1));
	enable_multiple_instances_checkbox->connect(SceneStringName(toggled), callable_mp(instance_count, &SpinBox::set_editable));
	instance_count->set_editable(enable_multiple_instances_checkbox->is_pressed());

	main_args_edit = memnew(LineEdit);
	main_args_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_args_edit->set_placeholder(TTR("Space-separated arguments, example: host player1 blue"));
	args_gc->add_child(main_args_edit);
	_fetch_main_args();
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &RunInstancesDialog::_fetch_main_args));
	main_args_edit->connect(SceneStringName(text_changed), callable_mp(this, &RunInstancesDialog::_start_main_timer).unbind(1));

	main_features_edit = memnew(LineEdit);
	main_features_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_features_edit->set_placeholder(TTR("Comma-separated tags, example: demo, steam, event"));
	main_features_edit->set_text(EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_main_feature_tags", ""));
	args_gc->add_child(main_features_edit);
	main_features_edit->connect(SceneStringName(text_changed), callable_mp(this, &RunInstancesDialog::_start_main_timer).unbind(1));

	{
		Label *l = memnew(Label);
		l->set_text(TTR("Instance Configuration"));
		l->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
		l->set_theme_type_variation("HeaderSmall");
		main_vb->add_child(l);
	}

	instance_tree = memnew(Tree);
	instance_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	instance_tree->set_h_scroll_enabled(false);
	instance_tree->set_columns(4);
	instance_tree->set_column_titles_visible(true);
	instance_tree->set_column_title(COLUMN_OVERRIDE_ARGS, TTR("Override Main Run Args"));
	instance_tree->set_column_expand(COLUMN_OVERRIDE_ARGS, false);
	instance_tree->set_column_title(COLUMN_LAUNCH_ARGUMENTS, TTR("Launch Arguments"));
	instance_tree->set_column_title(COLUMN_OVERRIDE_FEATURES, TTR("Override Main Tags"));
	instance_tree->set_column_expand(COLUMN_OVERRIDE_FEATURES, false);
	instance_tree->set_column_title(COLUMN_FEATURE_TAGS, TTR("Feature Tags"));
	instance_tree->set_hide_root(true);
	instance_tree->set_allow_rmb_select(true);

	popup_menu = memnew(PopupMenu);
	popup_menu->connect(SceneStringName(id_pressed), callable_mp(this, &RunInstancesDialog::_instance_menu_id_pressed));
	instance_tree->add_child(popup_menu);

	instance_tree->connect("item_mouse_selected", callable_mp(this, &RunInstancesDialog::_instance_tree_rmb));
	instance_tree->connect("empty_clicked", callable_mp(this, &RunInstancesDialog::_instance_tree_rmb));
	main_vb->add_child(instance_tree);

	_refresh_argument_count();
	instance_tree->connect("item_edited", callable_mp(this, &RunInstancesDialog::_start_instance_timer));
}

bool RunInstancesDialog::InstanceData::overrides_run_args() const {
	return item->is_checked(COLUMN_OVERRIDE_ARGS);
}

String RunInstancesDialog::InstanceData::get_launch_arguments() const {
	return item->get_text(COLUMN_LAUNCH_ARGUMENTS);
}

bool RunInstancesDialog::InstanceData::overrides_features() const {
	return item->is_checked(COLUMN_OVERRIDE_FEATURES);
}

String RunInstancesDialog::InstanceData::get_feature_tags() const {
	return item->get_text(COLUMN_FEATURE_TAGS);
}
