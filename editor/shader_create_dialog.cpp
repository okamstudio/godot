/**************************************************************************/
/*  shader_create_dialog.cpp                                              */
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

#include "shader_create_dialog.h"

#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_validation_panel.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/shader_include.h"
#include "scene/resources/visual_shader.h"
#include "servers/rendering/shader_types.h"

enum ShaderType {
	SHADER_TYPE_TEXT,
	SHADER_TYPE_VISUAL,
	SHADER_TYPE_INC,
	SHADER_TYPE_MAX,
};

void ShaderCreateDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			String last_lang = EditorSettings::get_singleton()->get_project_metadata("shader_setup", "last_selected_language", "");
			if (!last_lang.is_empty()) {
				for (int i = 0; i < type_menu->get_item_count(); i++) {
					if (type_menu->get_item_text(i) == last_lang) {
						type_menu->select(i);
						current_type = i;
						break;
					}
				}
			} else {
				type_menu->select(default_type);
			}

			current_mode = EditorSettings::get_singleton()->get_project_metadata("shader_setup", "last_selected_mode", 0);
			mode_menu->select(current_mode);
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			static const char *shader_types[3] = { "Shader", "VisualShader", "TextFile" };
			for (int i = 0; i < 3; i++) {
				Ref<Texture2D> icon = get_editor_theme_icon(shader_types[i]);
				if (icon.is_valid()) {
					type_menu->set_item_icon(i, icon);
				}
			}

			path_button->set_button_icon(get_editor_theme_icon(SNAME("Folder")));
		} break;
	}
}

void ShaderCreateDialog::_update_language_info() {
	type_data.clear();

	for (int i = 0; i < SHADER_TYPE_MAX; i++) {
		ShaderTypeData shader_type_data;
		if (i == int(SHADER_TYPE_TEXT)) {
			shader_type_data.use_templates = true;
			shader_type_data.extensions.push_back("gdshader");
			shader_type_data.default_extension = "gdshader";
		} else if (i == int(SHADER_TYPE_INC)) {
			shader_type_data.extensions.push_back("gdshaderinc");
			shader_type_data.default_extension = "gdshaderinc";
		} else {
			shader_type_data.default_extension = "tres";
		}
		shader_type_data.extensions.push_back("res");
		shader_type_data.extensions.push_back("tres");
		type_data.push_back(shader_type_data);
	}
}

void ShaderCreateDialog::_path_hbox_sorted() {
	if (is_visible()) {
		int filename_start_pos = initial_base_path.rfind_char('/') + 1;
		int filename_end_pos = initial_base_path.length();

		if (!is_built_in) {
			file_path->select(filename_start_pos, filename_end_pos);
		}

		file_path->set_caret_column(file_path->get_text().length());
		file_path->set_caret_column(filename_start_pos);

		file_path->grab_focus();
	}
}

void ShaderCreateDialog::_mode_changed(int p_mode) {
	current_mode = p_mode;
	EditorSettings::get_singleton()->set_project_metadata("shader_setup", "last_selected_mode", p_mode);
}

void ShaderCreateDialog::_template_changed(int p_template) {
	current_template = p_template;
	EditorSettings::get_singleton()->set_project_metadata("shader_setup", "last_selected_template", p_template);
}

void ShaderCreateDialog::ok_pressed() {
	if (is_new_shader_created) {
		_create_new();
	} else {
		_load_exist();
	}

	is_new_shader_created = true;
	validation_panel->update();
}

void ShaderCreateDialog::_create_new() {
	Ref<Resource> shader;
	Ref<Resource> shader_inc;

	switch (type_menu->get_selected()) {
		case SHADER_TYPE_TEXT: {
			Ref<Shader> text_shader;
			text_shader.instantiate();
			shader = text_shader;

			StringBuilder code;
			code += vformat("shader_type %s;\n", mode_menu->get_text().to_snake_case());

			if (current_template == 0) { // Default template.
				switch (current_mode) {
					case Shader::MODE_SPATIAL:
						code += R"(
void vertex() {
	// Called for every vertex the material is visible on.
}

void fragment() {
	// Called for every pixel the material is visible on.
}

//void light() {
//	// Called for every pixel for every light affecting the material.
//	// Uncomment to replace the default light processing function with this one.
//}
)";
						break;
					case Shader::MODE_CANVAS_ITEM:
						code += R"(
void vertex() {
	// Called for every vertex the material is visible on.
}

void fragment() {
	// Called for every pixel the material is visible on.
}

//void light() {
//	// Called for every pixel for every light affecting the CanvasItem.
//	// Uncomment to replace the default light processing function with this one.
//}
)";
						break;
					case Shader::MODE_PARTICLES:
						code += R"(
void start() {
	// Called when a particle is spawned.
}

void process() {
	// Called every frame on existing particles (according to the Fixed FPS property).
}
)";
						break;
					case Shader::MODE_SKY:
						code += R"(
void sky() {
	// Called for every visible pixel in the sky background, as well as all pixels
	// in the radiance cubemap.
}
)";
						break;
					case Shader::MODE_FOG:
						code += R"(
void fog() {
	// Called once for every froxel that is touched by an axis-aligned bounding box
	// of the associated FogVolume. This means that froxels that just barely touch
	// a given FogVolume will still be used.
}
)";
				}
			}
			text_shader->set_code(code.as_string());
		} break;
		case SHADER_TYPE_VISUAL: {
			Ref<VisualShader> visual_shader;
			visual_shader.instantiate();
			shader = visual_shader;
			visual_shader->set_mode(Shader::Mode(current_mode));
		} break;
		case SHADER_TYPE_INC: {
			Ref<ShaderInclude> include;
			include.instantiate();
			shader_inc = include;
		} break;
		default: {
		} break;
	}

	if (shader.is_null()) {
		String lpath = ProjectSettings::get_singleton()->localize_path(file_path->get_text());
		shader_inc->set_path(lpath);

		Error error = ResourceSaver::save(shader_inc, lpath, ResourceSaver::FLAG_CHANGE_PATH);
		if (error != OK) {
			alert->set_text(TTR("Error - Could not create shader include in filesystem."));
			alert->popup_centered();
			return;
		}
		EditorNode::get_singleton()->ensure_uid_file(lpath);

		emit_signal(SNAME("shader_include_created"), shader_inc);
	} else {
		if (is_built_in) {
			Node *edited_scene = get_tree()->get_edited_scene_root();
			if (likely(edited_scene)) {
				shader->set_path(edited_scene->get_scene_file_path() + "::");
			}
		} else {
			String lpath = ProjectSettings::get_singleton()->localize_path(file_path->get_text());
			shader->set_path(lpath);

			Error error = ResourceSaver::save(shader, lpath, ResourceSaver::FLAG_CHANGE_PATH);
			if (error != OK) {
				alert->set_text(TTR("Error - Could not create shader in filesystem."));
				alert->popup_centered();
				return;
			}
			EditorNode::get_singleton()->ensure_uid_file(lpath);
		}

		emit_signal(SNAME("shader_created"), shader);
	}

	file_path->set_text(file_path->get_text().get_base_dir());
	hide();
}

void ShaderCreateDialog::_load_exist() {
	String path = file_path->get_text();
	Ref<Resource> p_shader = ResourceLoader::load(path, "Shader");
	if (p_shader.is_null()) {
		alert->set_text(vformat(TTR("Error loading shader from %s"), path));
		alert->popup_centered();
		return;
	}

	emit_signal(SNAME("shader_created"), p_shader);
	hide();
}

void ShaderCreateDialog::_type_changed(int p_language) {
	current_type = p_language;
	ShaderTypeData shader_type_data = type_data.get(p_language);

	String selected_ext = "." + shader_type_data.default_extension;
	String path = file_path->get_text();
	String extension = "";

	if (!path.is_empty()) {
		if (path.contains_char('.')) {
			extension = path.get_extension();
		}
		if (extension.length() == 0) {
			path += selected_ext;
		} else {
			path = path.get_basename() + selected_ext;
		}
	} else {
		path = "shader" + selected_ext;
	}
	_path_changed(path);
	file_path->set_text(path);

	type_menu->set_item_disabled(int(SHADER_TYPE_INC), load_enabled);
	mode_menu->set_disabled(p_language == SHADER_TYPE_INC);
	template_menu->set_disabled(!shader_type_data.use_templates);
	template_menu->clear();

	if (shader_type_data.use_templates) {
		int last_template = EditorSettings::get_singleton()->get_project_metadata("shader_setup", "last_selected_template", 0);

		template_menu->add_item(TTR("Default"));
		template_menu->add_item(TTR("Empty"));

		template_menu->select(last_template);
		current_template = last_template;
	} else {
		template_menu->add_item(TTR("N/A"));
	}

	EditorSettings::get_singleton()->set_project_metadata("shader_setup", "last_selected_language", type_menu->get_item_text(type_menu->get_selected()));
	validation_panel->update();
}

void ShaderCreateDialog::_built_in_toggled(bool p_enabled) {
	is_built_in = p_enabled;
	if (p_enabled) {
		is_new_shader_created = true;
	} else {
		_path_changed(file_path->get_text());
	}
	validation_panel->update();
}

void ShaderCreateDialog::_browse_path() {
	file_browse->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_browse->set_title(TTR("Open Shader / Choose Location"));
	file_browse->set_ok_button_text(TTR("Open"));

	file_browse->set_disable_overwrite_warning(true);
	file_browse->clear_filters();

	List<String> extensions = type_data.get(type_menu->get_selected()).extensions;

	for (const String &E : extensions) {
		file_browse->add_filter("*." + E);
	}

	file_browse->set_current_path(file_path->get_text());
	file_browse->popup_file_dialog();
}

void ShaderCreateDialog::_file_selected(const String &p_file) {
	String p = ProjectSettings::get_singleton()->localize_path(p_file);
	file_path->set_text(p);
	_path_changed(p);

	String filename = p.get_file().get_basename();
	int select_start = p.rfind(filename);
	file_path->select(select_start, select_start + filename.length());
	file_path->set_caret_column(select_start + filename.length());
	file_path->grab_focus();
}

void ShaderCreateDialog::_path_changed(const String &p_path) {
	if (is_built_in) {
		return;
	}

	is_path_valid = false;
	is_new_shader_created = true;

	path_error = _validate_path(p_path);
	if (!path_error.is_empty()) {
		validation_panel->update();
		return;
	}

	Ref<DirAccess> f = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	String p = ProjectSettings::get_singleton()->localize_path(p_path.strip_edges());
	if (f->file_exists(p)) {
		is_new_shader_created = false;
	}

	is_path_valid = true;
	validation_panel->update();
}

void ShaderCreateDialog::_path_submitted(const String &p_path) {
	if (!get_ok_button()->is_disabled()) {
		ok_pressed();
	}
}

void ShaderCreateDialog::config(const String &p_base_path, bool p_built_in_enabled, bool p_load_enabled, int p_preferred_type, int p_preferred_mode) {
	if (!p_base_path.is_empty()) {
		initial_base_path = p_base_path.get_basename();
		file_path->set_text(initial_base_path + "." + type_data.get(type_menu->get_selected()).default_extension);
		current_type = type_menu->get_selected();
	} else {
		initial_base_path = "";
		file_path->set_text("");
	}
	file_path->deselect();

	built_in_enabled = p_built_in_enabled;
	load_enabled = p_load_enabled;

	if (p_preferred_type > -1) {
		type_menu->select(p_preferred_type);
		_type_changed(p_preferred_type);
	}

	if (p_preferred_mode > -1) {
		mode_menu->select(p_preferred_mode);
		_mode_changed(p_preferred_mode);
	}

	_type_changed(current_type);
	_path_changed(file_path->get_text());
}

String ShaderCreateDialog::_validate_path(const String &p_path) {
	String p = p_path.strip_edges();

	if (p.is_empty()) {
		return TTR("Path is empty.");
	}
	if (p.get_file().get_basename().is_empty()) {
		return TTR("Filename is empty.");
	}

	p = ProjectSettings::get_singleton()->localize_path(p);
	if (!p.begins_with("res://")) {
		return TTR("Path is not local.");
	}

	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (d->change_dir(p.get_base_dir()) != OK) {
		return TTR("Invalid base path.");
	}

	Ref<DirAccess> f = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (f->dir_exists(p)) {
		return TTR("A directory with the same name exists.");
	}

	String extension = p.get_extension();
	HashSet<String> extensions;

	List<ShaderCreateDialog::ShaderTypeData>::ConstIterator itr = type_data.begin();
	for (int i = 0; i < SHADER_TYPE_MAX; ++itr, ++i) {
		for (const String &ext : itr->extensions) {
			if (!extensions.has(ext)) {
				extensions.insert(ext);
			}
		}
	}

	bool found = false;
	bool match = false;

	for (const String &ext : extensions) {
		if (ext.nocasecmp_to(extension) == 0) {
			found = true;
			for (const String &type_ext : type_data.get(current_type).extensions) {
				if (type_ext.nocasecmp_to(extension) == 0) {
					match = true;
					break;
				}
			}
			break;
		}
	}

	if (!found) {
		return TTR("Invalid extension.");
	}
	if (!match) {
		return TTR("Wrong extension chosen.");
	}

	return "";
}

void ShaderCreateDialog::_update_dialog() {
	if (!is_built_in && !is_path_valid) {
		validation_panel->set_message(MSG_ID_SHADER, TTR("Invalid path."), EditorValidationPanel::MSG_ERROR);
	}
	if (!is_built_in && !path_error.is_empty()) {
		validation_panel->set_message(MSG_ID_PATH, path_error, EditorValidationPanel::MSG_ERROR);
	} else if (validation_panel->is_valid() && !is_new_shader_created) {
		validation_panel->set_message(MSG_ID_SHADER, TTR("File exists, it will be reused."), EditorValidationPanel::MSG_OK);
	}
	if (!built_in_enabled) {
		internal->set_pressed(false);
	}

	if (is_built_in) {
		file_path->set_editable(false);
		path_button->set_disabled(true);
		re_check_path = true;
	} else {
		file_path->set_editable(true);
		path_button->set_disabled(false);
		if (re_check_path) {
			re_check_path = false;
			_path_changed(file_path->get_text());
		}
	}

	internal->set_disabled(!built_in_enabled);

	if (is_built_in) {
		validation_panel->set_message(MSG_ID_BUILT_IN, TTR("Note: Built-in shaders can't be edited using an external editor."), EditorValidationPanel::MSG_INFO, false);
	}

	if (is_built_in) {
		set_ok_button_text(TTR("Create"));
		validation_panel->set_message(MSG_ID_PATH, TTR("Built-in shader (into scene file)."), EditorValidationPanel::MSG_OK);
	} else if (is_new_shader_created) {
		set_ok_button_text(TTR("Create"));
	} else if (load_enabled) {
		set_ok_button_text(TTR("Load"));
		if (is_path_valid) {
			validation_panel->set_message(MSG_ID_PATH, TTR("Will load an existing shader file."), EditorValidationPanel::MSG_OK);
		}
	} else {
		set_ok_button_text(TTR("Create"));
		validation_panel->set_message(MSG_ID_PATH, TTR("Shader file already exists."), EditorValidationPanel::MSG_ERROR);
	}
}

void ShaderCreateDialog::_bind_methods() {
	ClassDB::bind_method(D_METHOD("config", "path", "built_in_enabled", "load_enabled"), &ShaderCreateDialog::config, DEFVAL(true), DEFVAL(true));

	ADD_SIGNAL(MethodInfo("shader_created", PropertyInfo(Variant::OBJECT, "shader", PROPERTY_HINT_RESOURCE_TYPE, "Shader")));
	ADD_SIGNAL(MethodInfo("shader_include_created", PropertyInfo(Variant::OBJECT, "shader_include", PROPERTY_HINT_RESOURCE_TYPE, "ShaderInclude")));
}

ShaderCreateDialog::ShaderCreateDialog() {
	_update_language_info();

	// Main Controls.

	gc = memnew(GridContainer);
	gc->set_columns(2);

	// Error Fields.

	validation_panel = memnew(EditorValidationPanel);
	validation_panel->add_line(MSG_ID_SHADER, TTR("Shader path/name is valid."));
	validation_panel->add_line(MSG_ID_PATH, TTR("Will create a new shader file."));
	validation_panel->add_line(MSG_ID_BUILT_IN);
	validation_panel->set_update_callback(callable_mp(this, &ShaderCreateDialog::_update_dialog));
	validation_panel->set_accept_button(get_ok_button());

	// Spacing.

	Control *spacing = memnew(Control);
	spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

	VBoxContainer *vb = memnew(VBoxContainer);
	vb->add_child(gc);
	vb->add_child(spacing);
	vb->add_child(validation_panel);
	add_child(vb);

	// Type.

	type_menu = memnew(OptionButton);
	type_menu->set_custom_minimum_size(Size2(250, 0) * EDSCALE);
	type_menu->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	gc->add_child(memnew(Label(TTR("Type:"))));
	gc->add_child(type_menu);

	for (int i = 0; i < SHADER_TYPE_MAX; i++) {
		String type;
		bool invalid = false;
		switch (i) {
			case SHADER_TYPE_TEXT:
				type = "Shader";
				default_type = i;
				break;
			case SHADER_TYPE_VISUAL:
				type = "VisualShader";
				break;
			case SHADER_TYPE_INC:
				type = "ShaderInclude";
				break;
			case SHADER_TYPE_MAX:
				invalid = true;
				break;
			default:
				invalid = true;
				break;
		}
		if (invalid) {
			continue;
		}
		type_menu->add_item(type);
	}
	if (default_type >= 0) {
		type_menu->select(default_type);
	}
	current_type = default_type;
	type_menu->connect(SceneStringName(item_selected), callable_mp(this, &ShaderCreateDialog::_type_changed));

	// Modes.

	mode_menu = memnew(OptionButton);
	for (const String &type_name : ShaderTypes::get_singleton()->get_types_list()) {
		mode_menu->add_item(type_name.capitalize());
	}
	gc->add_child(memnew(Label(TTR("Mode:"))));
	gc->add_child(mode_menu);
	mode_menu->connect(SceneStringName(item_selected), callable_mp(this, &ShaderCreateDialog::_mode_changed));

	// Templates.

	template_menu = memnew(OptionButton);
	gc->add_child(memnew(Label(TTR("Template:"))));
	gc->add_child(template_menu);
	template_menu->connect(SceneStringName(item_selected), callable_mp(this, &ShaderCreateDialog::_template_changed));

	// Built-in Shader.

	internal = memnew(CheckBox);
	internal->set_text(TTR("On"));
	internal->connect(SceneStringName(toggled), callable_mp(this, &ShaderCreateDialog::_built_in_toggled));
	gc->add_child(memnew(Label(TTR("Built-in Shader:"))));
	gc->add_child(internal);

	// Path.

	HBoxContainer *hb = memnew(HBoxContainer);
	hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hb->connect(SceneStringName(sort_children), callable_mp(this, &ShaderCreateDialog::_path_hbox_sorted));
	file_path = memnew(LineEdit);
	file_path->connect(SceneStringName(text_changed), callable_mp(this, &ShaderCreateDialog::_path_changed));
	file_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hb->add_child(file_path);
	register_text_enter(file_path);
	path_button = memnew(Button);
	path_button->connect(SceneStringName(pressed), callable_mp(this, &ShaderCreateDialog::_browse_path));
	hb->add_child(path_button);
	gc->add_child(memnew(Label(TTR("Path:"))));
	gc->add_child(hb);

	// Dialog Setup.

	file_browse = memnew(EditorFileDialog);
	file_browse->connect("file_selected", callable_mp(this, &ShaderCreateDialog::_file_selected));
	file_browse->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	add_child(file_browse);

	alert = memnew(AcceptDialog);
	alert->get_label()->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	alert->get_label()->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	alert->get_label()->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	alert->get_label()->set_custom_minimum_size(Size2(325, 60) * EDSCALE);
	add_child(alert);

	set_ok_button_text(TTR("Create"));
	set_hide_on_ok(false);

	set_title(TTR("Create Shader"));
}
