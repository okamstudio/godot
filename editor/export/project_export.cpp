/**************************************************************************/
/*  project_export.cpp                                                    */
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

#include "project_export.h"

#include "core/config/project_settings.h"
#include "core/version.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_properties.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/export/editor_export.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/import/resource_importer_texture_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_box.h"
#include "scene/gui/check_button.h"
#include "scene/gui/item_list.h"
#include "scene/gui/link_button.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tree.h"

void ProjectExportTextureFormatError::_on_fix_texture_format_pressed() {
	ProjectSettings::get_singleton()->set_setting(setting_identifier, true);
	ProjectSettings::get_singleton()->save();
	EditorFileSystem::get_singleton()->scan_changes();
	emit_signal("texture_format_enabled");
}

void ProjectExportTextureFormatError::_bind_methods() {
	ADD_SIGNAL(MethodInfo("texture_format_enabled"));
}

void ProjectExportTextureFormatError::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			texture_format_error_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		} break;
	}
}

void ProjectExportTextureFormatError::show_for_texture_format(const String &p_friendly_name, const String &p_setting_identifier) {
	texture_format_error_label->set_text(vformat(TTR("Target platform requires '%s' texture compression. Enable 'Import %s' to fix."), p_friendly_name, p_friendly_name.replace("/", " ")));
	setting_identifier = p_setting_identifier;
	show();
}

ProjectExportTextureFormatError::ProjectExportTextureFormatError() {
	// Set up the label.
	texture_format_error_label = memnew(Label);
	add_child(texture_format_error_label);
	// Set up the fix button.
	fix_texture_format_button = memnew(LinkButton);
	fix_texture_format_button->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	fix_texture_format_button->set_text(TTR("Fix Import"));
	add_child(fix_texture_format_button);
	fix_texture_format_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectExportTextureFormatError::_on_fix_texture_format_pressed));
}

void ProjectExportDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				EditorSettings::get_singleton()->set_project_metadata("dialog_bounds", "export", Rect2(get_position(), get_size()));
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			duplicate_preset->set_button_icon(presets->get_editor_theme_icon(SNAME("Duplicate")));
			delete_preset->set_button_icon(presets->get_editor_theme_icon(SNAME("Remove")));
			patch_add_btn->set_button_icon(get_editor_theme_icon(SNAME("Add")));
		} break;

		case NOTIFICATION_READY: {
			duplicate_preset->set_button_icon(presets->get_editor_theme_icon(SNAME("Duplicate")));
			delete_preset->set_button_icon(presets->get_editor_theme_icon(SNAME("Remove")));
			patch_add_btn->set_button_icon(get_editor_theme_icon(SNAME("Add")));
			connect(SceneStringName(confirmed), callable_mp(this, &ProjectExportDialog::_export_pck_zip));
			_update_export_all();
		} break;
	}
}

void ProjectExportDialog::popup_export() {
	add_preset->get_popup()->clear();
	for (int i = 0; i < EditorExport::get_singleton()->get_export_platform_count(); i++) {
		Ref<EditorExportPlatform> plat = EditorExport::get_singleton()->get_export_platform(i);

		add_preset->get_popup()->add_icon_item(plat->get_logo(), plat->get_name());
	}

	_update_presets();
	if (presets->get_current() >= 0) {
		_update_current_preset(); // triggers rescan for templates if newly installed
	}

	// Restore valid window bounds or pop up at default size.
	Rect2 saved_size = EditorSettings::get_singleton()->get_project_metadata("dialog_bounds", "export", Rect2());
	if (saved_size != Rect2()) {
		popup(saved_size);
	} else {
		popup_centered_clamped(Size2(900, 500) * EDSCALE, 0.7);
	}
}

void ProjectExportDialog::_add_preset(int p_platform) {
	Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_platform(p_platform)->create_preset();
	ERR_FAIL_COND(!preset.is_valid());

	String preset_name = EditorExport::get_singleton()->get_export_platform(p_platform)->get_name();
	bool make_runnable = true;
	int attempt = 1;
	while (true) {
		bool valid = true;

		for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
			Ref<EditorExportPreset> p = EditorExport::get_singleton()->get_export_preset(i);
			if (p->get_platform() == preset->get_platform() && p->is_runnable()) {
				make_runnable = false;
			}
			if (p->get_name() == preset_name) {
				valid = false;
				break;
			}
		}

		if (valid) {
			break;
		}

		attempt++;
		preset_name = EditorExport::get_singleton()->get_export_platform(p_platform)->get_name() + " " + itos(attempt);
	}

	preset->set_name(preset_name);
	if (make_runnable) {
		preset->set_runnable(make_runnable);
	}
	EditorExport::get_singleton()->add_export_preset(preset);
	_update_presets();
	_edit_preset(EditorExport::get_singleton()->get_export_preset_count() - 1);
}

void ProjectExportDialog::_force_update_current_preset_parameters() {
	// Force the parameters section to refresh its UI.
	parameters->edit(nullptr);
	_update_current_preset();
}

void ProjectExportDialog::_update_current_preset() {
	_edit_preset(presets->get_current());
}

void ProjectExportDialog::_update_presets() {
	updating = true;

	Ref<EditorExportPreset> current;
	if (presets->get_current() >= 0 && presets->get_current() < presets->get_item_count()) {
		current = get_current_preset();
	}

	int current_idx = -1;
	presets->clear();
	for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
		Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(i);
		if (preset == current) {
			current_idx = i;
		}

		String preset_name = preset->get_name();
		if (preset->is_runnable()) {
			preset_name += " (" + TTR("Runnable") + ")";
		}
		preset->update_files();
		presets->add_item(preset_name, preset->get_platform()->get_logo());
	}

	if (current_idx != -1) {
		presets->select(current_idx);
	}

	updating = false;
}

void ProjectExportDialog::_update_export_all() {
	bool can_export = EditorExport::get_singleton()->get_export_preset_count() > 0;

	for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
		Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(i);
		bool needs_templates;
		String error;
		if (preset->get_export_path().is_empty() || !preset->get_platform()->can_export(preset, error, needs_templates)) {
			can_export = false;
			break;
		}
	}

	export_all_button->set_disabled(!can_export);

	if (can_export) {
		export_all_button->set_tooltip_text(TTR("Export the project for all the presets defined."));
	} else {
		export_all_button->set_tooltip_text(TTR("All presets must have an export path defined for Export All to work."));
	}
}

void ProjectExportDialog::_edit_preset(int p_index) {
	if (p_index < 0 || p_index >= presets->get_item_count()) {
		name->set_text("");
		name->set_editable(false);
		export_path->hide();
		advanced_options->set_disabled(true);
		runnable->set_disabled(true);
		parameters->edit(nullptr);
		presets->deselect_all();
		duplicate_preset->set_disabled(true);
		delete_preset->set_disabled(true);
		sections->hide();
		patches->clear();
		export_error->hide();
		export_templates_error->hide();
		return;
	}

	Ref<EditorExportPreset> current = EditorExport::get_singleton()->get_export_preset(p_index);
	ERR_FAIL_COND(current.is_null());

	updating = true;

	presets->select(p_index);
	sections->show();

	name->set_editable(true);
	export_path->show();
	duplicate_preset->set_disabled(false);
	delete_preset->set_disabled(false);
	get_ok_button()->set_disabled(false);
	name->set_text(current->get_name());

	List<String> extension_list = current->get_platform()->get_binary_extensions(current);
	Vector<String> extension_vector;
	for (const String &extension : extension_list) {
		extension_vector.push_back("*." + extension);
	}

	export_path->setup(extension_vector, false, true);
	export_path->update_property();
	advanced_options->set_disabled(false);
	advanced_options->set_pressed(current->are_advanced_options_enabled());
	runnable->set_disabled(false);
	runnable->set_pressed(current->is_runnable());
	if (parameters->get_edited_object() != current.ptr()) {
		current->update_value_overrides();
	}
	parameters->set_object_class(current->get_platform()->get_class_name());
	parameters->edit(current.ptr());

	export_filter->select(current->get_export_filter());
	include_filters->set_text(current->get_include_filter());
	include_label->set_text(_get_resource_export_header(current->get_export_filter()));
	exclude_filters->set_text(current->get_exclude_filter());
	server_strip_message->set_visible(current->get_export_filter() == EditorExportPreset::EXPORT_CUSTOMIZED);

	patches->clear();
	TreeItem *patch_root = patches->create_item();
	Vector<String> patch_list = current->get_patches();
	for (int i = 0; i < patch_list.size(); i++) {
		TreeItem *patch = patches->create_item(patch_root);
		const String &patch_path = patch_list[i];
		patch->set_cell_mode(0, TreeItem::CELL_MODE_STRING);
		patch->set_editable(0, true);
		patch->set_text(0, patch_path.get_file());
		patch->set_tooltip_text(0, patch_path);
		patch->set_metadata(0, i);
		patch->add_button(0, get_editor_theme_icon(SNAME("Remove")), 0);
		patch->add_button(0, get_editor_theme_icon(SNAME("FileBrowse")), 1);
	}

	_fill_resource_tree();

	bool needs_templates;
	String error;
	if (!current->get_platform()->can_export(current, error, needs_templates)) {
		if (!error.is_empty()) {
			Vector<String> items = error.split("\n", false);
			error = "";
			for (int i = 0; i < items.size(); i++) {
				if (i > 0) {
					error += "\n";
				}
				error += " - " + items[i];
			}

			export_error->set_text(error);
			export_error->show();
		} else {
			export_error->hide();
		}
		if (needs_templates) {
			export_templates_error->show();
		} else {
			export_templates_error->hide();
		}

		export_warning->hide();
		export_button->set_disabled(true);
	} else {
		if (error != String()) {
			Vector<String> items = error.split("\n", false);
			error = "";
			for (int i = 0; i < items.size(); i++) {
				if (i > 0) {
					error += "\n";
				}
				error += " - " + items[i];
			}
			export_warning->set_text(error);
			export_warning->show();
		} else {
			export_warning->hide();
		}

		export_error->hide();
		export_templates_error->hide();
		export_button->set_disabled(false);
	}

	custom_features->set_text(current->get_custom_features());
	_update_feature_list();
	_update_export_all();
	child_controls_changed();

	if ((feature_set.has("s3tc") || feature_set.has("bptc")) && !ResourceImporterTextureSettings::should_import_s3tc_bptc()) {
		export_texture_format_error->show_for_texture_format("S3TC/BPTC", "rendering/textures/vram_compression/import_s3tc_bptc");
	} else if ((feature_set.has("etc2") || feature_set.has("astc")) && !ResourceImporterTextureSettings::should_import_etc2_astc()) {
		export_texture_format_error->show_for_texture_format("ETC2/ASTC", "rendering/textures/vram_compression/import_etc2_astc");
	} else {
		export_texture_format_error->hide();
	}

	String enc_in_filters_str = current->get_enc_in_filter();
	String enc_ex_filters_str = current->get_enc_ex_filter();
	if (!updating_enc_filters) {
		enc_in_filters->set_text(enc_in_filters_str);
		enc_ex_filters->set_text(enc_ex_filters_str);
	}

	bool enc_pck_mode = current->get_enc_pck();
	enc_pck->set_pressed(enc_pck_mode);

	uint64_t seed = current->get_seed();
	if (!updating_seed) {
		seed_input->set_text(itos(seed));
	}

	enc_directory->set_disabled(!enc_pck_mode);
	enc_in_filters->set_editable(enc_pck_mode);
	enc_ex_filters->set_editable(enc_pck_mode);
	script_key->set_editable(enc_pck_mode);
	seed_input->set_editable(enc_pck_mode);

	bool enc_directory_mode = current->get_enc_directory();
	enc_directory->set_pressed(enc_directory_mode);

	String key = current->get_script_encryption_key();
	if (!updating_script_key) {
		script_key->set_text(key);
	}
	if (enc_pck_mode) {
		script_key->set_editable(true);

		bool key_valid = _validate_script_encryption_key(key);
		if (key_valid) {
			script_key_error->hide();
		} else {
			script_key_error->show();
		}
	} else {
		script_key->set_editable(false);
		script_key_error->hide();
	}

	int script_export_mode = current->get_script_export_mode();
	script_mode->select(script_export_mode);

	updating = false;
}

void ProjectExportDialog::_update_feature_list() {
	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	List<String> features_list;

	current->get_platform()->get_platform_features(&features_list);
	current->get_platform()->get_preset_features(current, &features_list);

	String custom = current->get_custom_features();
	Vector<String> custom_list = custom.split(",");
	for (int i = 0; i < custom_list.size(); i++) {
		String f = custom_list[i].strip_edges();
		if (!f.is_empty()) {
			features_list.push_back(f);
		}
	}

	feature_set.clear();
	for (const String &E : features_list) {
		feature_set.insert(E);
	}

#ifdef REAL_T_IS_DOUBLE
	feature_set.insert("double");
#else
	feature_set.insert("single");
#endif // REAL_T_IS_DOUBLE

	custom_feature_display->clear();
	String text;
	bool first = true;
	for (const String &E : feature_set) {
		if (!first) {
			text += ", ";
		} else {
			first = false;
		}
		text += E;
	}
	custom_feature_display->add_text(text);
}

void ProjectExportDialog::_custom_features_changed(const String &p_text) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_custom_features(p_text);
	_update_feature_list();
}

void ProjectExportDialog::_tab_changed(int) {
	_update_feature_list();
}

void ProjectExportDialog::_update_parameters(const String &p_edited_property) {
	_update_current_preset();
}

void ProjectExportDialog::_advanced_options_pressed() {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_advanced_options_enabled(advanced_options->is_pressed());
	_update_presets();
}

void ProjectExportDialog::_runnable_pressed() {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	if (runnable->is_pressed()) {
		for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
			Ref<EditorExportPreset> p = EditorExport::get_singleton()->get_export_preset(i);
			if (p->get_platform() == current->get_platform()) {
				p->set_runnable(current == p);
			}
		}
	} else {
		current->set_runnable(false);
	}

	_update_presets();
}

void ProjectExportDialog::_name_changed(const String &p_string) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_name(p_string);
	_update_presets();
}

void ProjectExportDialog::set_export_path(const String &p_value) {
	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_export_path(p_value);
}

String ProjectExportDialog::get_export_path() {
	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND_V(current.is_null(), String(""));

	return current->get_export_path();
}

Ref<EditorExportPreset> ProjectExportDialog::get_current_preset() const {
	return EditorExport::get_singleton()->get_export_preset(presets->get_current());
}

void ProjectExportDialog::_export_path_changed(const StringName &p_property, const Variant &p_value, const String &p_field, bool p_changing) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_export_path(p_value);
	_update_presets();
	_update_export_all();
}

void ProjectExportDialog::_enc_filters_changed(const String &p_filters) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_enc_in_filter(enc_in_filters->get_text());
	current->set_enc_ex_filter(enc_ex_filters->get_text());

	updating_enc_filters = true;
	_update_current_preset();
	updating_enc_filters = false;
}

void ProjectExportDialog::_open_key_help_link() {
	OS::get_singleton()->shell_open(vformat("%s/contributing/development/compiling/compiling_with_script_encryption_key.html", VERSION_DOCS_URL));
}

void ProjectExportDialog::_enc_pck_changed(bool p_pressed) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_enc_pck(p_pressed);
	enc_directory->set_disabled(!p_pressed);
	enc_in_filters->set_editable(p_pressed);
	enc_ex_filters->set_editable(p_pressed);
	script_key->set_editable(p_pressed);

	_update_current_preset();
}

void ProjectExportDialog::_seed_input_changed(const String &p_text) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_seed(seed_input->get_text().to_int());

	updating_seed = true;
	_update_current_preset();
	updating_seed = false;
}

void ProjectExportDialog::_enc_directory_changed(bool p_pressed) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_enc_directory(p_pressed);

	_update_current_preset();
}

void ProjectExportDialog::_script_encryption_key_changed(const String &p_key) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_script_encryption_key(p_key);

	updating_script_key = true;
	_update_current_preset();
	updating_script_key = false;
}

bool ProjectExportDialog::_validate_script_encryption_key(const String &p_key) {
	bool is_valid = false;

	if (!p_key.is_empty() && p_key.is_valid_hex_number(false) && p_key.length() == 64) {
		is_valid = true;
	}
	return is_valid;
}

void ProjectExportDialog::_script_export_mode_changed(int p_mode) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	current->set_script_export_mode(p_mode);

	_update_current_preset();
}

void ProjectExportDialog::_duplicate_preset() {
	Ref<EditorExportPreset> current = get_current_preset();
	if (current.is_null()) {
		return;
	}

	Ref<EditorExportPreset> preset = current->get_platform()->create_preset();
	ERR_FAIL_COND(!preset.is_valid());

	String preset_name = current->get_name() + " (copy)";
	bool make_runnable = true;
	while (true) {
		bool valid = true;

		for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
			Ref<EditorExportPreset> p = EditorExport::get_singleton()->get_export_preset(i);
			if (p->get_platform() == preset->get_platform() && p->is_runnable()) {
				make_runnable = false;
			}
			if (p->get_name() == preset_name) {
				valid = false;
				break;
			}
		}

		if (valid) {
			break;
		}

		preset_name += " (copy)";
	}

	preset->set_name(preset_name);
	if (make_runnable) {
		preset->set_runnable(make_runnable);
	}
	preset->set_advanced_options_enabled(current->are_advanced_options_enabled());
	preset->set_dedicated_server(current->is_dedicated_server());
	preset->set_export_filter(current->get_export_filter());
	preset->set_include_filter(current->get_include_filter());
	preset->set_exclude_filter(current->get_exclude_filter());
	preset->set_patches(current->get_patches());
	preset->set_custom_features(current->get_custom_features());
	preset->set_enc_in_filter(current->get_enc_in_filter());
	preset->set_enc_ex_filter(current->get_enc_ex_filter());
	preset->set_enc_pck(current->get_enc_pck());
	preset->set_enc_directory(current->get_enc_directory());
	preset->set_script_encryption_key(current->get_script_encryption_key());
	preset->set_script_export_mode(current->get_script_export_mode());

	for (const KeyValue<StringName, Variant> &E : current->get_values()) {
		preset->set(E.key, E.value);
	}

	EditorExport::get_singleton()->add_export_preset(preset);
	_update_presets();
	_edit_preset(EditorExport::get_singleton()->get_export_preset_count() - 1);
}

void ProjectExportDialog::_delete_preset() {
	Ref<EditorExportPreset> current = get_current_preset();
	if (current.is_null()) {
		return;
	}

	delete_confirm->set_text(vformat(TTR("Delete preset '%s'?"), current->get_name()));
	delete_confirm->popup_centered();
}

void ProjectExportDialog::_delete_preset_confirm() {
	int idx = presets->get_current();
	_edit_preset(-1);
	export_button->set_disabled(true);
	get_ok_button()->set_disabled(true);
	EditorExport::get_singleton()->remove_export_preset(idx);
	_update_presets();

	// The Export All button might become enabled (if all other presets have an export path defined),
	// or it could be disabled (if there are no presets anymore).
	_update_export_all();
}

Variant ProjectExportDialog::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (p_from == presets) {
		int pos = presets->get_item_at_position(p_point, true);

		if (pos >= 0) {
			Dictionary d;
			d["type"] = "export_preset";
			d["preset"] = pos;

			HBoxContainer *drag = memnew(HBoxContainer);
			TextureRect *tr = memnew(TextureRect);
			tr->set_texture(presets->get_item_icon(pos));
			drag->add_child(tr);
			Label *label = memnew(Label);
			label->set_text(presets->get_item_text(pos));
			label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED); // Don't translate user input.
			drag->add_child(label);

			presets->set_drag_preview(drag);

			return d;
		}
	} else if (p_from == patches) {
		TreeItem *item = patches->get_item_at_position(p_point);

		if (item) {
			int item_metadata = item->get_metadata(0);
			Dictionary d;
			d["type"] = "export_patch";
			d["patch"] = item_metadata;

			Label *label = memnew(Label);
			label->set_text(item->get_text(0));
			patches->set_drag_preview(label);

			return d;
		}
	}
	return Variant();
}

bool ProjectExportDialog::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	if (p_from == presets) {
		Dictionary d = p_data;
		if (!d.has("type") || String(d["type"]) != "export_preset") {
			return false;
		}

		if (presets->get_item_at_position(p_point, true) < 0 && !presets->is_pos_at_end_of_items(p_point)) {
			return false;
		}
	} else if (p_from == patches) {
		Dictionary d = p_data;
		if (d.get("type", "") != "export_patch") {
			return false;
		}

		TreeItem *item = patches->get_item_at_position(p_point);
		if (!item) {
			return false;
		}

		patches->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);
	}

	return true;
}

void ProjectExportDialog::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (p_from == presets) {
		Dictionary d = p_data;
		int from_pos = d["preset"];

		int to_pos = -1;

		if (presets->get_item_at_position(p_point, true) >= 0) {
			to_pos = presets->get_item_at_position(p_point, true);
		}

		if (to_pos == -1 && !presets->is_pos_at_end_of_items(p_point)) {
			return;
		}

		if (to_pos == from_pos) {
			return;
		} else if (to_pos > from_pos) {
			to_pos--;
		}

		Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(from_pos);
		EditorExport::get_singleton()->remove_export_preset(from_pos);
		EditorExport::get_singleton()->add_export_preset(preset, to_pos);

		_update_presets();
		if (to_pos >= 0) {
			_edit_preset(to_pos);
		} else {
			_edit_preset(presets->get_item_count() - 1);
		}
	} else if (p_from == patches) {
		Dictionary d = p_data;
		int from_pos = d["patch"];

		TreeItem *item = patches->get_item_at_position(p_point);
		if (!item) {
			return;
		}

		int to_pos = item->get_metadata(0);

		if (patches->get_drop_section_at_position(p_point) > 0) {
			to_pos++;
		}

		if (to_pos > from_pos) {
			to_pos--;
		}

		Ref<EditorExportPreset> preset = get_current_preset();
		String patch = preset->get_patch(from_pos);
		preset->remove_patch(from_pos);
		preset->add_patch(patch, to_pos);

		_update_current_preset();
	}
}

void ProjectExportDialog::_export_type_changed(int p_which) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	if (current.is_null()) {
		return;
	}

	EditorExportPreset::ExportFilter filter_type = (EditorExportPreset::ExportFilter)p_which;
	current->set_export_filter(filter_type);
	current->set_dedicated_server(filter_type == EditorExportPreset::EXPORT_CUSTOMIZED);
	server_strip_message->set_visible(filter_type == EditorExportPreset::EXPORT_CUSTOMIZED);

	// Default to stripping everything when first switching to server build.
	if (filter_type == EditorExportPreset::EXPORT_CUSTOMIZED && current->get_customized_files_count() == 0) {
		current->set_file_export_mode("res://", EditorExportPreset::MODE_FILE_STRIP);
	}
	include_label->set_text(_get_resource_export_header(current->get_export_filter()));

	updating = true;
	_fill_resource_tree();
	updating = false;
}

String ProjectExportDialog::_get_resource_export_header(EditorExportPreset::ExportFilter p_filter) const {
	switch (p_filter) {
		case EditorExportPreset::EXCLUDE_SELECTED_RESOURCES:
			return TTR("Resources to exclude:");
		case EditorExportPreset::EXPORT_CUSTOMIZED:
			return TTR("Resources to override export behavior:");
		default:
			return TTR("Resources to export:");
	}
}

void ProjectExportDialog::_filter_changed(const String &p_filter) {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	if (current.is_null()) {
		return;
	}

	current->set_include_filter(include_filters->get_text());
	current->set_exclude_filter(exclude_filters->get_text());
}

void ProjectExportDialog::_fill_resource_tree() {
	include_files->clear();
	include_label->hide();
	include_margin->hide();

	Ref<EditorExportPreset> current = get_current_preset();
	if (current.is_null()) {
		return;
	}

	EditorExportPreset::ExportFilter f = current->get_export_filter();

	if (f == EditorExportPreset::EXPORT_ALL_RESOURCES) {
		return;
	}

	TreeItem *root = include_files->create_item();

	if (f == EditorExportPreset::EXPORT_CUSTOMIZED) {
		include_files->set_columns(2);
		include_files->set_column_expand(1, false);
		include_files->set_column_custom_minimum_width(1, 250 * EDSCALE);
	} else {
		include_files->set_columns(1);
	}

	include_label->show();
	include_margin->show();

	_fill_tree(EditorFileSystem::get_singleton()->get_filesystem(), root, current, f);

	if (f == EditorExportPreset::EXPORT_CUSTOMIZED) {
		_propagate_file_export_mode(include_files->get_root(), EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED);
	}
}

void ProjectExportDialog::_setup_item_for_file_mode(TreeItem *p_item, EditorExportPreset::FileExportMode p_mode) {
	if (p_mode == EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED) {
		p_item->set_checked(0, false);
		p_item->set_cell_mode(1, TreeItem::CELL_MODE_STRING);
		p_item->set_editable(1, false);
		p_item->set_selectable(1, false);
		p_item->set_custom_color(1, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
	} else {
		p_item->set_checked(0, true);
		p_item->set_cell_mode(1, TreeItem::CELL_MODE_CUSTOM);
		p_item->set_editable(1, true);
		p_item->set_selectable(1, true);
		p_item->clear_custom_color(1);
	}
	p_item->set_metadata(1, p_mode);
}

bool ProjectExportDialog::_fill_tree(EditorFileSystemDirectory *p_dir, TreeItem *p_item, Ref<EditorExportPreset> &current, EditorExportPreset::ExportFilter p_export_filter) {
	p_item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
	p_item->set_icon(0, presets->get_theme_icon(SNAME("folder"), SNAME("FileDialog")));
	p_item->set_text(0, p_dir->get_name() + "/");
	p_item->set_editable(0, true);
	p_item->set_metadata(0, p_dir->get_path());

	if (p_export_filter == EditorExportPreset::EXPORT_CUSTOMIZED) {
		_setup_item_for_file_mode(p_item, current->get_file_export_mode(p_dir->get_path()));
	}

	bool used = false;
	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		TreeItem *subdir = include_files->create_item(p_item);
		if (_fill_tree(p_dir->get_subdir(i), subdir, current, p_export_filter)) {
			used = true;
		} else {
			memdelete(subdir);
		}
	}

	for (int i = 0; i < p_dir->get_file_count(); i++) {
		String type = p_dir->get_file_type(i);
		if (p_export_filter == EditorExportPreset::EXPORT_SELECTED_SCENES && type != "PackedScene") {
			continue;
		}
		if (type == "TextFile" || type == "OtherFile") {
			continue;
		}

		TreeItem *file = include_files->create_item(p_item);
		file->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		file->set_text(0, p_dir->get_file(i));

		String path = p_dir->get_file_path(i);

		file->set_icon(0, EditorNode::get_singleton()->get_class_icon(type));
		file->set_editable(0, true);
		file->set_metadata(0, path);

		if (p_export_filter == EditorExportPreset::EXPORT_CUSTOMIZED) {
			_setup_item_for_file_mode(file, current->get_file_export_mode(path));
		} else {
			file->set_checked(0, current->has_export_file(path));
			file->propagate_check(0);
		}

		used = true;
	}
	return used;
}

void ProjectExportDialog::_propagate_file_export_mode(TreeItem *p_item, EditorExportPreset::FileExportMode p_inherited_export_mode) {
	EditorExportPreset::FileExportMode file_export_mode = (EditorExportPreset::FileExportMode)(int)p_item->get_metadata(1);
	bool is_inherited = false;
	if (file_export_mode == EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED) {
		file_export_mode = p_inherited_export_mode;
		is_inherited = true;
	}

	if (file_export_mode == EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED) {
		p_item->set_text(1, "");
	} else {
		String text = file_mode_popup->get_item_text(file_mode_popup->get_item_index(file_export_mode));
		if (is_inherited) {
			text += " " + TTR("(Inherited)");
		}
		p_item->set_text(1, text);
	}

	for (int i = 0; i < p_item->get_child_count(); i++) {
		_propagate_file_export_mode(p_item->get_child(i), file_export_mode);
	}
}

void ProjectExportDialog::_tree_changed() {
	if (updating) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	if (current.is_null()) {
		return;
	}

	TreeItem *item = include_files->get_edited();
	if (!item) {
		return;
	}

	if (current->get_export_filter() == EditorExportPreset::EXPORT_CUSTOMIZED) {
		EditorExportPreset::FileExportMode file_mode = EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED;
		String path = item->get_metadata(0);

		if (item->is_checked(0)) {
			file_mode = current->get_file_export_mode(path, EditorExportPreset::MODE_FILE_STRIP);
		}

		current->set_file_export_mode(path, file_mode);
		_setup_item_for_file_mode(item, file_mode);
		_propagate_file_export_mode(include_files->get_root(), EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED);
	} else {
		item->propagate_check(0);
	}
}

void ProjectExportDialog::_check_propagated_to_item(Object *p_obj, int column) {
	Ref<EditorExportPreset> current = get_current_preset();
	if (current.is_null()) {
		return;
	}
	TreeItem *item = Object::cast_to<TreeItem>(p_obj);
	String path = item->get_metadata(0);
	if (item && !path.ends_with("/")) {
		bool added = item->is_checked(0);
		if (added) {
			current->add_export_file(path);
		} else {
			current->remove_export_file(path);
		}
	}
}

void ProjectExportDialog::_tree_popup_edited(bool p_arrow_clicked) {
	Rect2 bounds = include_files->get_custom_popup_rect();
	bounds.position += get_global_canvas_transform().get_origin();
	bounds.size *= get_global_canvas_transform().get_scale();
	if (!is_embedding_subwindows()) {
		bounds.position += get_position();
	}
	file_mode_popup->popup(bounds);
}

void ProjectExportDialog::_set_file_export_mode(int p_id) {
	Ref<EditorExportPreset> current = get_current_preset();
	if (current.is_null()) {
		return;
	}

	TreeItem *item = include_files->get_edited();
	String path = item->get_metadata(0);

	EditorExportPreset::FileExportMode file_export_mode = (EditorExportPreset::FileExportMode)p_id;
	current->set_file_export_mode(path, file_export_mode);
	item->set_metadata(1, file_export_mode);
	_propagate_file_export_mode(include_files->get_root(), EditorExportPreset::MODE_FILE_NOT_CUSTOMIZED);
}

void ProjectExportDialog::_patch_tree_button_clicked(Object *p_item, int p_column, int p_id, int p_mouse_button_index) {
	TreeItem *ti = Object::cast_to<TreeItem>(p_item);

	patch_index = ti->get_metadata(0);

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	if (p_id == 0) {
		Vector<String> preset_patches = current->get_patches();
		ERR_FAIL_INDEX(patch_index, preset_patches.size());
		patch_erase->set_text(vformat(TTR("Delete patch '%s' from list?"), preset_patches[patch_index].get_file()));
		patch_erase->popup_centered();
	} else {
		patch_dialog->popup_file_dialog();
	}
}

void ProjectExportDialog::_patch_tree_item_edited() {
	TreeItem *item = patches->get_edited();
	if (!item) {
		return;
	}

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	int index = item->get_metadata(0);
	String patch_path = item->get_text(0);

	current->set_patch(index, patch_path);
	item->set_tooltip_text(0, patch_path);
}

void ProjectExportDialog::_patch_file_selected(const String &p_path) {
	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	String relative_path = ProjectSettings::get_singleton()->get_resource_path().path_to_file(p_path);

	Vector<String> preset_patches = current->get_patches();
	if (patch_index >= preset_patches.size()) {
		current->add_patch(relative_path);
	} else {
		current->set_patch(patch_index, relative_path);
	}

	_update_current_preset();
}

void ProjectExportDialog::_patch_delete_confirmed() {
	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	Vector<String> preset_patches = current->get_patches();
	if (patch_index < preset_patches.size()) {
		current->remove_patch(patch_index);
		_update_current_preset();
	}
}

void ProjectExportDialog::_patch_add_pack_pressed() {
	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	patch_index = current->get_patches().size();
	patch_dialog->popup_file_dialog();
}

void ProjectExportDialog::_export_pck_zip() {
	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());

	String dir = current->get_export_path().get_base_dir();
	export_pck_zip->set_current_dir(dir);

	export_pck_zip->popup_file_dialog();
}

void ProjectExportDialog::_export_pck_zip_selected(const String &p_path) {
	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());
	Ref<EditorExportPlatform> platform = current->get_platform();
	ERR_FAIL_COND(platform.is_null());

	const Dictionary &fd_option = export_pck_zip->get_selected_options();
	bool export_debug = fd_option.get(TTR("Export With Debug"), true);
	bool export_as_patch = fd_option.get(TTR("Export As Patch"), true);

	EditorSettings::get_singleton()->set_project_metadata("export_options", "export_debug", export_debug);
	EditorSettings::get_singleton()->set_project_metadata("export_options", "export_as_patch", export_as_patch);

	if (p_path.ends_with(".zip")) {
		if (export_as_patch) {
			platform->export_zip_patch(current, export_debug, p_path);
		} else {
			platform->export_zip(current, export_debug, p_path);
		}
	} else if (p_path.ends_with(".pck")) {
		if (export_as_patch) {
			platform->export_pack_patch(current, export_debug, p_path);
		} else {
			platform->export_pack(current, export_debug, p_path);
		}
	} else {
		ERR_FAIL_MSG("Path must end with .pck or .zip");
	}
}

void ProjectExportDialog::_open_export_template_manager() {
	hide();
	EditorNode::get_singleton()->open_export_template_manager();
}

void ProjectExportDialog::_validate_export_path(const String &p_path) {
	// Disable export via OK button or Enter key if LineEdit has an empty filename
	bool invalid_path = (p_path.get_file().get_basename().is_empty());

	// Check if state change before needlessly messing with signals
	if (invalid_path && export_project->get_ok_button()->is_disabled()) {
		return;
	}
	if (!invalid_path && !export_project->get_ok_button()->is_disabled()) {
		return;
	}

	if (invalid_path) {
		export_project->get_ok_button()->set_disabled(true);
		export_project->get_line_edit()->disconnect(SceneStringName(text_submitted), callable_mp(export_project, &EditorFileDialog::_file_submitted));
	} else {
		export_project->get_ok_button()->set_disabled(false);
		export_project->get_line_edit()->connect(SceneStringName(text_submitted), callable_mp(export_project, &EditorFileDialog::_file_submitted));
	}
}

void ProjectExportDialog::_export_project() {
	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND(current.is_null());
	Ref<EditorExportPlatform> platform = current->get_platform();
	ERR_FAIL_COND(platform.is_null());

	export_project->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	export_project->clear_filters();

	List<String> extension_list = platform->get_binary_extensions(current);
	for (const String &extension : extension_list) {
		// TRANSLATORS: This is the name of a project export file format. %s will be replaced by the platform name.
		export_project->add_filter("*." + extension, vformat(TTR("%s Export"), platform->get_name()));
	}

	if (!current->get_export_path().is_empty()) {
		export_project->set_current_path(current->get_export_path());
	} else {
		if (extension_list.size() >= 1) {
			export_project->set_current_file(default_filename + "." + extension_list.front()->get());
		} else {
			export_project->set_current_file(default_filename);
		}
	}

	// Ensure that signal is connected if previous attempt left it disconnected
	// with _validate_export_path.
	// FIXME: This is a hack, we should instead change EditorFileDialog to allow
	// disabling validation by the "text_submitted" signal.
	if (!export_project->get_line_edit()->is_connected(SceneStringName(text_submitted), callable_mp(export_project, &EditorFileDialog::_file_submitted))) {
		export_project->get_ok_button()->set_disabled(false);
		export_project->get_line_edit()->connect(SceneStringName(text_submitted), callable_mp(export_project, &EditorFileDialog::_file_submitted));
	}

	export_project->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	export_project->popup_file_dialog();
}

void ProjectExportDialog::_export_project_to_path(const String &p_path) {
	// Save this name for use in future exports (but drop the file extension)
	default_filename = p_path.get_file().get_basename();
	EditorSettings::get_singleton()->set_project_metadata("export_options", "default_filename", default_filename);

	Ref<EditorExportPreset> current = get_current_preset();
	ERR_FAIL_COND_MSG(current.is_null(), "Failed to start the export: current preset is invalid.");
	Ref<EditorExportPlatform> platform = current->get_platform();
	ERR_FAIL_COND_MSG(platform.is_null(), "Failed to start the export: current preset has no valid platform.");
	current->set_export_path(p_path);

	exporting = true;

	platform->clear_messages();
	current->update_value_overrides();
	Dictionary fd_option = export_project->get_selected_options();
	bool export_debug = fd_option.get(TTR("Export With Debug"), true);

	EditorSettings::get_singleton()->set_project_metadata("export_options", "export_debug", export_debug);

	Error err = platform->export_project(current, export_debug, current->get_export_path(), 0);
	result_dialog_log->clear();
	if (err != ERR_SKIP) {
		if (platform->fill_log_messages(result_dialog_log, err)) {
			result_dialog->popup_centered_ratio(0.5);
		}
	}

	exporting = false;
}

void ProjectExportDialog::_export_all_dialog() {
	export_all_dialog->show();
	export_all_dialog->popup_centered(Size2(300, 80));
}

void ProjectExportDialog::_export_all_dialog_action(const String &p_str) {
	export_all_dialog->hide();

	_export_all(p_str != "release");
}

void ProjectExportDialog::_export_all(bool p_debug) {
	exporting = true;
	bool show_dialog = false;

	{ // Scope for the editor progress, we must free it before showing the dialog at the end.
		String export_target = p_debug ? TTR("Debug") : TTR("Release");
		EditorProgress ep("exportall", TTR("Exporting All") + " " + export_target, EditorExport::get_singleton()->get_export_preset_count(), true);

		result_dialog_log->clear();
		for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
			Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(i);
			if (preset.is_null()) {
				exporting = false;
				ERR_FAIL_MSG("Failed to start the export: one of the presets is invalid.");
			}

			Ref<EditorExportPlatform> platform = preset->get_platform();
			if (platform.is_null()) {
				exporting = false;
				ERR_FAIL_MSG("Failed to start the export: one of the presets has no valid platform.");
			}

			ep.step(preset->get_name(), i);

			platform->clear_messages();
			preset->update_value_overrides();
			Error err = platform->export_project(preset, p_debug, preset->get_export_path(), 0);
			if (err == ERR_SKIP) {
				exporting = false;
				return;
			}
			bool has_messages = platform->fill_log_messages(result_dialog_log, err);
			show_dialog = show_dialog || has_messages;
		}
	}

	if (show_dialog) {
		result_dialog->popup_centered_ratio(0.5);
	}

	exporting = false;
}

void ProjectExportDialog::_bind_methods() {
	ClassDB::bind_method("set_export_path", &ProjectExportDialog::set_export_path);
	ClassDB::bind_method("get_export_path", &ProjectExportDialog::get_export_path);
	ClassDB::bind_method("get_current_preset", &ProjectExportDialog::get_current_preset);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "export_path"), "set_export_path", "get_export_path");
}

ProjectExportDialog::ProjectExportDialog() {
	set_title(TTR("Export"));
	set_clamp_to_embedder(true);

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);

	HSplitContainer *hbox = memnew(HSplitContainer);
	main_vb->add_child(hbox);
	hbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	// Presets list.

	VBoxContainer *preset_vb = memnew(VBoxContainer);
	preset_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hbox->add_child(preset_vb);

	Label *l = memnew(Label(TTR("Presets")));
	l->set_theme_type_variation("HeaderSmall");

	HBoxContainer *preset_hb = memnew(HBoxContainer);
	preset_hb->add_child(l);
	preset_hb->add_spacer();
	preset_vb->add_child(preset_hb);

	add_preset = memnew(MenuButton);
	add_preset->set_text(TTR("Add..."));
	add_preset->get_popup()->connect("index_pressed", callable_mp(this, &ProjectExportDialog::_add_preset));
	preset_hb->add_child(add_preset);
	MarginContainer *mc = memnew(MarginContainer);
	preset_vb->add_child(mc);
	mc->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	presets = memnew(ItemList);
	presets->set_theme_type_variation("ItemListSecondary");
	presets->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	SET_DRAG_FORWARDING_GCD(presets, ProjectExportDialog);
	mc->add_child(presets);
	presets->connect(SceneStringName(item_selected), callable_mp(this, &ProjectExportDialog::_edit_preset));
	duplicate_preset = memnew(Button);
	duplicate_preset->set_tooltip_text(TTR("Duplicate"));
	duplicate_preset->set_flat(true);
	preset_hb->add_child(duplicate_preset);
	duplicate_preset->connect(SceneStringName(pressed), callable_mp(this, &ProjectExportDialog::_duplicate_preset));
	delete_preset = memnew(Button);
	delete_preset->set_tooltip_text(TTR("Delete"));
	delete_preset->set_flat(true);
	preset_hb->add_child(delete_preset);
	delete_preset->connect(SceneStringName(pressed), callable_mp(this, &ProjectExportDialog::_delete_preset));

	// Preset settings.

	VBoxContainer *settings_vb = memnew(VBoxContainer);
	settings_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hbox->add_child(settings_vb);

	name = memnew(LineEdit);
	settings_vb->add_margin_child(TTR("Name:"), name);
	name->connect(SceneStringName(text_changed), callable_mp(this, &ProjectExportDialog::_name_changed));

	runnable = memnew(CheckButton);
	runnable->set_text(TTR("Runnable"));
	runnable->set_tooltip_text(TTR("If checked, the preset will be available for use in one-click deploy.\nOnly one preset per platform may be marked as runnable."));
	runnable->connect(SceneStringName(pressed), callable_mp(this, &ProjectExportDialog::_runnable_pressed));

	advanced_options = memnew(CheckButton);
	advanced_options->set_text(TTR("Advanced Options"));
	advanced_options->set_tooltip_text(TTR("If checked, the advanced options will be shown."));
	advanced_options->connect(SceneStringName(pressed), callable_mp(this, &ProjectExportDialog::_advanced_options_pressed));

	HBoxContainer *preset_configs_container = memnew(HBoxContainer);
	preset_configs_container->add_spacer(true);
	preset_configs_container->add_child(advanced_options);
	preset_configs_container->add_child(runnable);
	settings_vb->add_child(preset_configs_container);

	export_path = memnew(EditorPropertyPath);
	settings_vb->add_child(export_path);
	export_path->set_label(TTR("Export Path"));
	export_path->set_object_and_property(this, "export_path");
	export_path->set_save_mode();
	export_path->connect("property_changed", callable_mp(this, &ProjectExportDialog::_export_path_changed));

	// Subsections.

	sections = memnew(TabContainer);
	sections->set_use_hidden_tabs_for_min_size(true);
	sections->set_theme_type_variation("TabContainerOdd");
	settings_vb->add_child(sections);
	sections->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	// Main preset parameters.

	parameters = memnew(EditorInspector);
	sections->add_child(parameters);
	parameters->set_name(TTR("Options"));
	parameters->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	parameters->set_use_doc_hints(true);
	parameters->connect("property_edited", callable_mp(this, &ProjectExportDialog::_update_parameters));
	EditorExport::get_singleton()->connect("export_presets_updated", callable_mp(this, &ProjectExportDialog::_force_update_current_preset_parameters));

	// Resources export parameters.

	ScrollContainer *resources_scroll_container = memnew(ScrollContainer);
	resources_scroll_container->set_name(TTR("Resources"));
	resources_scroll_container->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	sections->add_child(resources_scroll_container);

	VBoxContainer *resources_vb = memnew(VBoxContainer);
	resources_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	resources_vb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	resources_scroll_container->add_child(resources_vb);

	export_filter = memnew(OptionButton);
	export_filter->add_item(TTR("Export all resources in the project"));
	export_filter->add_item(TTR("Export selected scenes (and dependencies)"));
	export_filter->add_item(TTR("Export selected resources (and dependencies)"));
	export_filter->add_item(TTR("Export all resources in the project except resources checked below"));
	export_filter->add_item(TTR("Export as dedicated server"));
	resources_vb->add_margin_child(TTR("Export Mode:"), export_filter);
	export_filter->connect(SceneStringName(item_selected), callable_mp(this, &ProjectExportDialog::_export_type_changed));

	include_label = memnew(Label);
	include_label->set_text(TTR("Resources to export:"));
	resources_vb->add_child(include_label);
	include_margin = memnew(MarginContainer);
	include_margin->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	resources_vb->add_child(include_margin);

	include_files = memnew(Tree);
	include_files->set_custom_minimum_size(Size2(1, 75 * EDSCALE));
	include_margin->add_child(include_files);
	include_files->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	include_files->connect("item_edited", callable_mp(this, &ProjectExportDialog::_tree_changed));
	include_files->connect("check_propagated_to_item", callable_mp(this, &ProjectExportDialog::_check_propagated_to_item));
	include_files->connect("custom_popup_edited", callable_mp(this, &ProjectExportDialog::_tree_popup_edited));

	server_strip_message = memnew(Label);
	server_strip_message->set_visible(false);
	server_strip_message->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	server_strip_message->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
	resources_vb->add_child(server_strip_message);

	{
		List<StringName> resource_names;
		ClassDB::get_inheriters_from_class("Resource", &resource_names);

		PackedStringArray strippable;
		for (const StringName &resource_name : resource_names) {
			if (ClassDB::has_method(resource_name, "create_placeholder", true)) {
				strippable.push_back(resource_name);
			}
		}
		strippable.sort();

		String message = TTR("\"Strip Visuals\" will replace the following resources with placeholders:") + " ";
		message += String(", ").join(strippable);
		server_strip_message->set_text(message);
	}

	file_mode_popup = memnew(PopupMenu);
	add_child(file_mode_popup);
	file_mode_popup->add_item(TTR("Strip Visuals"), EditorExportPreset::MODE_FILE_STRIP);
	file_mode_popup->add_item(TTR("Keep"), EditorExportPreset::MODE_FILE_KEEP);
	file_mode_popup->add_item(TTR("Remove"), EditorExportPreset::MODE_FILE_REMOVE);
	file_mode_popup->connect(SceneStringName(id_pressed), callable_mp(this, &ProjectExportDialog::_set_file_export_mode));

	include_filters = memnew(LineEdit);
	resources_vb->add_margin_child(
			TTR("Filters to export non-resource files/folders\n(comma-separated, e.g: *.json, *.txt, docs/*)"),
			include_filters);
	include_filters->connect(SceneStringName(text_changed), callable_mp(this, &ProjectExportDialog::_filter_changed));

	exclude_filters = memnew(LineEdit);
	resources_vb->add_margin_child(
			TTR("Filters to exclude files/folders from project\n(comma-separated, e.g: *.json, *.txt, docs/*)"),
			exclude_filters);
	exclude_filters->connect(SceneStringName(text_changed), callable_mp(this, &ProjectExportDialog::_filter_changed));

	// Patch packages.

	VBoxContainer *patch_vb = memnew(VBoxContainer);
	sections->add_child(patch_vb);
	patch_vb->set_name(TTR("Patches"));

	patches = memnew(Tree);
	patches->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	patches->set_hide_root(true);
	patches->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	patches->connect("button_clicked", callable_mp(this, &ProjectExportDialog::_patch_tree_button_clicked));
	patches->connect("item_edited", callable_mp(this, &ProjectExportDialog::_patch_tree_item_edited));
	SET_DRAG_FORWARDING_GCD(patches, ProjectExportDialog);
	patches->set_edit_checkbox_cell_only_when_checkbox_is_pressed(true);
	patch_vb->add_margin_child(TTR("Base Packs:"), patches, true);

	patch_dialog = memnew(EditorFileDialog);
	patch_dialog->add_filter("*.pck", TTR("Godot Project Pack"));
	patch_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	patch_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	patch_dialog->connect("file_selected", callable_mp(this, &ProjectExportDialog::_patch_file_selected));
	add_child(patch_dialog);

	patch_erase = memnew(ConfirmationDialog);
	patch_erase->set_ok_button_text(TTR("Delete"));
	patch_erase->connect(SceneStringName(confirmed), callable_mp(this, &ProjectExportDialog::_patch_delete_confirmed));
	add_child(patch_erase);

	patch_add_btn = memnew(Button);
	patch_add_btn->set_text(TTR("Add Pack"));
	patch_add_btn->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	patch_add_btn->connect(SceneStringName(pressed), callable_mp(this, &ProjectExportDialog::_patch_add_pack_pressed));
	patch_vb->add_child(patch_add_btn);

	// Feature tags.

	VBoxContainer *feature_vb = memnew(VBoxContainer);
	feature_vb->set_name(TTR("Features"));
	feature_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	custom_features = memnew(LineEdit);
	custom_features->connect(SceneStringName(text_changed), callable_mp(this, &ProjectExportDialog::_custom_features_changed));
	feature_vb->add_margin_child(TTR("Custom (comma-separated):"), custom_features);
	custom_feature_display = memnew(RichTextLabel);
	custom_feature_display->set_custom_minimum_size(Size2(1, 75 * EDSCALE));
	custom_feature_display->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	feature_vb->add_margin_child(TTR("Feature List:"), custom_feature_display, true);
	sections->add_child(feature_vb);

	// Encryption export parameters.

	ScrollContainer *sec_scroll_container = memnew(ScrollContainer);
	sec_scroll_container->set_name(TTR("Encryption"));
	sec_scroll_container->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);

	VBoxContainer *sec_vb = memnew(VBoxContainer);
	sec_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	sec_scroll_container->add_child(sec_vb);

	enc_pck = memnew(CheckButton);
	enc_pck->connect(SceneStringName(toggled), callable_mp(this, &ProjectExportDialog::_enc_pck_changed));
	enc_pck->set_text(TTR("Encrypt Exported PCK"));
	sec_vb->add_child(enc_pck);

	enc_directory = memnew(CheckButton);
	enc_directory->connect(SceneStringName(toggled), callable_mp(this, &ProjectExportDialog::_enc_directory_changed));
	enc_directory->set_text(TTR("Encrypt Index (File Names and Info)"));
	sec_vb->add_child(enc_directory);

	enc_in_filters = memnew(LineEdit);
	enc_in_filters->connect(SceneStringName(text_changed), callable_mp(this, &ProjectExportDialog::_enc_filters_changed));
	sec_vb->add_margin_child(
			TTR("Filters to include files/folders\n(comma-separated, e.g: *.tscn, *.tres, scenes/*)"),
			enc_in_filters);

	enc_ex_filters = memnew(LineEdit);
	enc_ex_filters->connect(SceneStringName(text_changed), callable_mp(this, &ProjectExportDialog::_enc_filters_changed));
	sec_vb->add_margin_child(
			TTR("Filters to exclude files/folders\n(comma-separated, e.g: *.ctex, *.import, music/*)"),
			enc_ex_filters);

	script_key = memnew(LineEdit);
	script_key->connect(SceneStringName(text_changed), callable_mp(this, &ProjectExportDialog::_script_encryption_key_changed));
	script_key_error = memnew(Label);
	script_key_error->set_text(String::utf8("•  ") + TTR("Invalid Encryption Key (must be 64 hexadecimal characters long)"));
	script_key_error->add_theme_color_override(SceneStringName(font_color), EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("error_color"), EditorStringName(Editor)));
	sec_vb->add_margin_child(TTR("Encryption Key (256-bits as hexadecimal):"), script_key);
	sec_vb->add_child(script_key_error);
	sections->add_child(sec_scroll_container);

	seed_input = memnew(LineEdit);
	seed_input->connect(SceneStringName(text_changed), callable_mp(this, &ProjectExportDialog::_seed_input_changed));
	sec_vb->add_margin_child(TTR("Initialization vector seed"), seed_input);

	Label *sec_info = memnew(Label);
	sec_info->set_text(TTR("Note: Encryption key needs to be stored in the binary,\nyou need to build the export templates from source."));
	sec_vb->add_child(sec_info);

	LinkButton *sec_more_info = memnew(LinkButton);
	sec_more_info->set_text(TTR("More Info..."));
	sec_more_info->connect(SceneStringName(pressed), callable_mp(this, &ProjectExportDialog::_open_key_help_link));
	sec_vb->add_child(sec_more_info);

	// Script export parameters.

	VBoxContainer *script_vb = memnew(VBoxContainer);
	script_vb->set_name(TTR("Scripts"));

	script_mode = memnew(OptionButton);
	script_vb->add_margin_child(TTR("GDScript Export Mode:"), script_mode);
	script_mode->add_item(TTR("Text (easier debugging)"), (int)EditorExportPreset::MODE_SCRIPT_TEXT);
	script_mode->add_item(TTR("Binary tokens (faster loading)"), (int)EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS);
	script_mode->add_item(TTR("Compressed binary tokens (smaller files)"), (int)EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED);
	script_mode->connect(SceneStringName(item_selected), callable_mp(this, &ProjectExportDialog::_script_export_mode_changed));

	sections->add_child(script_vb);

	sections->connect("tab_changed", callable_mp(this, &ProjectExportDialog::_tab_changed));

	// Disable by default.
	name->set_editable(false);
	export_path->hide();
	advanced_options->set_disabled(true);
	runnable->set_disabled(true);
	duplicate_preset->set_disabled(true);
	delete_preset->set_disabled(true);
	script_key_error->hide();
	sections->hide();
	parameters->edit(nullptr);

	// Deletion dialog.

	delete_confirm = memnew(ConfirmationDialog);
	add_child(delete_confirm);
	delete_confirm->set_ok_button_text(TTR("Delete"));
	delete_confirm->connect(SceneStringName(confirmed), callable_mp(this, &ProjectExportDialog::_delete_preset_confirm));

	// Export buttons, dialogs and errors.

	set_cancel_button_text(TTR("Close"));
	set_ok_button_text(TTR("Export PCK/ZIP..."));
	get_ok_button()->set_tooltip_text(TTR("Export the project resources as a PCK or ZIP package. This is not a playable build, only the project data without a Godot executable."));
	get_ok_button()->set_disabled(true);

	export_button = add_button(TTR("Export Project..."), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "export");
	export_button->set_tooltip_text(TTR("Export the project as a playable build (Godot executable and project data) for the selected preset."));
	export_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectExportDialog::_export_project));
	// Disable initially before we select a valid preset
	export_button->set_disabled(true);

	export_all_dialog = memnew(ConfirmationDialog);
	add_child(export_all_dialog);
	export_all_dialog->set_title(TTR("Export All"));
	export_all_dialog->set_text(TTR("Choose an export mode:"));
	export_all_dialog->get_ok_button()->hide();
	export_all_dialog->add_button(TTR("Debug"), true, "debug");
	export_all_dialog->add_button(TTR("Release"), true, "release");
	export_all_dialog->connect("custom_action", callable_mp(this, &ProjectExportDialog::_export_all_dialog_action));

	export_all_button = add_button(TTR("Export All..."), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "export");
	export_all_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectExportDialog::_export_all_dialog));
	export_all_button->set_disabled(true);

	export_pck_zip = memnew(EditorFileDialog);
	export_pck_zip->add_filter("*.zip", TTR("ZIP File"));
	export_pck_zip->add_filter("*.pck", TTR("Godot Project Pack"));
	export_pck_zip->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	export_pck_zip->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	add_child(export_pck_zip);
	export_pck_zip->connect("file_selected", callable_mp(this, &ProjectExportDialog::_export_pck_zip_selected));

	// Export warnings and errors bottom section.

	export_texture_format_error = memnew(ProjectExportTextureFormatError);
	main_vb->add_child(export_texture_format_error);
	export_texture_format_error->hide();
	export_texture_format_error->connect("texture_format_enabled", callable_mp(this, &ProjectExportDialog::_update_current_preset));

	export_error = memnew(Label);
	main_vb->add_child(export_error);
	export_error->hide();
	export_error->add_theme_color_override(SceneStringName(font_color), EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("error_color"), EditorStringName(Editor)));

	export_warning = memnew(Label);
	main_vb->add_child(export_warning);
	export_warning->hide();
	export_warning->add_theme_color_override(SceneStringName(font_color), EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("warning_color"), EditorStringName(Editor)));

	export_templates_error = memnew(HBoxContainer);
	main_vb->add_child(export_templates_error);
	export_templates_error->hide();

	Label *export_error2 = memnew(Label);
	export_templates_error->add_child(export_error2);
	export_error2->add_theme_color_override(SceneStringName(font_color), EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("error_color"), EditorStringName(Editor)));
	export_error2->set_text(String::utf8("•  ") + TTR("Export templates for this platform are missing:") + " ");

	result_dialog = memnew(AcceptDialog);
	result_dialog->set_title(TTR("Project Export"));
	result_dialog_log = memnew(RichTextLabel);
	result_dialog_log->set_custom_minimum_size(Size2(300, 80) * EDSCALE);
	result_dialog->add_child(result_dialog_log);

	main_vb->add_child(result_dialog);
	result_dialog->hide();

	LinkButton *download_templates = memnew(LinkButton);
	download_templates->set_text(TTR("Manage Export Templates"));
	download_templates->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	export_templates_error->add_child(download_templates);
	download_templates->connect(SceneStringName(pressed), callable_mp(this, &ProjectExportDialog::_open_export_template_manager));

	// Export project file dialog.

	export_project = memnew(EditorFileDialog);
	export_project->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	add_child(export_project);
	export_project->connect("file_selected", callable_mp(this, &ProjectExportDialog::_export_project_to_path));
	export_project->get_line_edit()->connect(SceneStringName(text_changed), callable_mp(this, &ProjectExportDialog::_validate_export_path));

	export_project->add_option(TTR("Export With Debug"), Vector<String>(), EditorSettings::get_singleton()->get_project_metadata("export_options", "export_debug", true));
	export_pck_zip->add_option(TTR("Export With Debug"), Vector<String>(), EditorSettings::get_singleton()->get_project_metadata("export_options", "export_debug", true));
	export_pck_zip->add_option(TTR("Export As Patch"), Vector<String>(), EditorSettings::get_singleton()->get_project_metadata("export_options", "export_as_patch", true));

	set_hide_on_ok(false);

	default_filename = EditorSettings::get_singleton()->get_project_metadata("export_options", "default_filename", "");
	// If no default set, use project name
	if (default_filename.is_empty()) {
		// If no project name defined, use a sane default
		default_filename = GLOBAL_GET("application/config/name");
		if (default_filename.is_empty()) {
			default_filename = "UnnamedProject";
		}
	}
}

ProjectExportDialog::~ProjectExportDialog() {
}
