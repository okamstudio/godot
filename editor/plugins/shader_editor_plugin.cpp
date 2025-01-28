/**************************************************************************/
/*  shader_editor_plugin.cpp                                              */
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

#include "shader_editor_plugin.h"

#include "editor/editor_command_palette.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/filesystem_dock.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/inspector_dock.h"
#include "editor/plugins/text_shader_editor.h"
#include "editor/plugins/visual_shader_editor_plugin.h"
#include "editor/shader_create_dialog.h"
#include "editor/themes/editor_scale.h"
#include "editor/window_wrapper.h"
#include "scene/gui/item_list.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/texture_rect.h"

Ref<Resource> ShaderEditorPlugin::_get_current_shader() {
	int index = shader_tabs->get_current_tab();
	ERR_FAIL_INDEX_V(index, shader_tabs->get_tab_count(), Ref<Resource>());
	if (edited_shaders[index].shader.is_valid()) {
		return edited_shaders[index].shader;
	} else {
		return edited_shaders[index].shader_inc;
	}
}

void ShaderEditorPlugin::_update_shader_list() {
	shader_list->clear();
	for (EditedShader &edited_shader : edited_shaders) {
		Ref<Resource> shader = edited_shader.shader;
		if (shader.is_null()) {
			shader = edited_shader.shader_inc;
		}

		String path = shader->get_path();
		String text = path.get_file();
		if (text.is_empty()) {
			// This appears for newly created built-in shaders before saving the scene.
			text = TTR("[unsaved]");
		} else if (shader->is_built_in()) {
			const String &shader_name = shader->get_name();
			if (!shader_name.is_empty()) {
				text = vformat("%s (%s)", shader_name, text.get_slice("::", 0));
			}
		}

		// When shader is deleted in filesystem dock, need this to correctly close shader editor.
		edited_shader.path = path;

		bool unsaved = false;
		if (edited_shader.shader_editor) {
			unsaved = edited_shader.shader_editor->is_unsaved();
		}
		// TODO: Handle visual shaders too.

		if (unsaved) {
			text += "(*)";
		}

		String _class = shader->get_class();
		if (!shader_list->has_theme_icon(_class, EditorStringName(EditorIcons))) {
			_class = "TextFile";
		}
		Ref<Texture2D> icon = shader_list->get_editor_theme_icon(_class);

		shader_list->add_item(text, icon);
		shader_list->set_item_tooltip(-1, path);
		edited_shader.name = text;
	}

	if (shader_tabs->get_tab_count()) {
		shader_list->select(shader_tabs->get_current_tab());
	}

	_set_file_specific_items_disabled(edited_shaders.is_empty());

	_update_shader_list_status();
}

void ShaderEditorPlugin::_update_shader_list_status() {
	for (int i = 0; i < shader_list->get_item_count(); i++) {
		TextShaderEditor *se = Object::cast_to<TextShaderEditor>(shader_tabs->get_tab_control(i));
		if (se) {
			if (se->was_compilation_successful()) {
				shader_list->set_item_tag_icon(i, Ref<Texture2D>());
			} else {
				shader_list->set_item_tag_icon(i, shader_list->get_editor_theme_icon(SNAME("Error")));
			}
		}
	}
}

void ShaderEditorPlugin::_move_shader_tab(int p_from, int p_to) {
	if (p_from == p_to) {
		return;
	}
	EditedShader es = edited_shaders[p_from];
	edited_shaders.remove_at(p_from);
	edited_shaders.insert(p_to, es);
	shader_tabs->move_child(shader_tabs->get_tab_control(p_from), p_to);
	_update_shader_list();
}

void ShaderEditorPlugin::edit(Object *p_object) {
	if (!p_object) {
		return;
	}

	EditedShader es;

	ShaderInclude *si = Object::cast_to<ShaderInclude>(p_object);
	if (si != nullptr) {
		for (uint32_t i = 0; i < edited_shaders.size(); i++) {
			if (edited_shaders[i].shader_inc.ptr() == si) {
				shader_tabs->set_current_tab(i);
				shader_list->select(i);
				return;
			}
		}
		es.shader_inc = Ref<ShaderInclude>(si);
		TextShaderEditor *text_shader = memnew(TextShaderEditor);
		text_shader->get_code_editor()->set_toggle_list_control(left_panel);
		es.shader_editor = text_shader;
		es.shader_editor->edit_shader_include(si);
		shader_tabs->add_child(es.shader_editor);
	} else {
		Shader *s = Object::cast_to<Shader>(p_object);
		for (uint32_t i = 0; i < edited_shaders.size(); i++) {
			if (edited_shaders[i].shader.ptr() == s) {
				shader_tabs->set_current_tab(i);
				shader_list->select(i);
				return;
			}
		}
		es.shader = Ref<Shader>(s);
		Ref<VisualShader> vs = es.shader;
		if (vs.is_valid()) {
			es.shader_editor = memnew(VisualShaderEditor);
		} else {
			TextShaderEditor *text_shader = memnew(TextShaderEditor);
			text_shader->get_code_editor()->set_toggle_list_control(left_panel);
			es.shader_editor = text_shader;
		}
		shader_tabs->add_child(es.shader_editor);
		es.shader_editor->edit_shader(es.shader);
	}

	TextShaderEditor *text_shader_editor = Object::cast_to<TextShaderEditor>(es.shader_editor);
	if (text_shader_editor) {
		text_shader_editor->connect("validation_changed", callable_mp(this, &ShaderEditorPlugin::_update_shader_list));
		CodeTextEditor *cte = text_shader_editor->get_code_editor();
		if (cte) {
			cte->set_zoom_factor(text_shader_zoom_factor);
			cte->connect("zoomed", callable_mp(this, &ShaderEditorPlugin::_set_text_shader_zoom_factor));
		}
	}

	shader_tabs->set_current_tab(shader_tabs->get_tab_count() - 1);
	edited_shaders.push_back(es);
	_update_shader_list();
}

bool ShaderEditorPlugin::handles(Object *p_object) const {
	return Object::cast_to<Shader>(p_object) != nullptr || Object::cast_to<ShaderInclude>(p_object) != nullptr;
}

void ShaderEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		EditorNode::get_bottom_panel()->make_item_visible(window_wrapper);
	}
}

void ShaderEditorPlugin::selected_notify() {
}

ShaderEditor *ShaderEditorPlugin::get_shader_editor(const Ref<Shader> &p_for_shader) {
	for (EditedShader &edited_shader : edited_shaders) {
		if (edited_shader.shader == p_for_shader) {
			return edited_shader.shader_editor;
		}
	}
	return nullptr;
}

void ShaderEditorPlugin::set_window_layout(Ref<ConfigFile> p_layout) {
	if (EDITOR_GET("interface/multi_window/restore_windows_on_load") && window_wrapper->is_window_available() && p_layout->has_section_key("ShaderEditor", "window_rect")) {
		window_wrapper->restore_window_from_saved_position(
				p_layout->get_value("ShaderEditor", "window_rect", Rect2i()),
				p_layout->get_value("ShaderEditor", "window_screen", -1),
				p_layout->get_value("ShaderEditor", "window_screen_rect", Rect2i()));
	} else {
		window_wrapper->set_window_enabled(false);
	}

	if (!bool(EDITOR_GET("editors/shader_editor/behavior/files/restore_shaders_on_load"))) {
		return;
	}
	if (!p_layout->has_section("ShaderEditor")) {
		return;
	}
	if (!p_layout->has_section_key("ShaderEditor", "open_shaders") ||
			!p_layout->has_section_key("ShaderEditor", "selected_shader")) {
		return;
	}

	Array shaders = p_layout->get_value("ShaderEditor", "open_shaders");
	int selected_shader_idx = 0;
	String selected_shader = p_layout->get_value("ShaderEditor", "selected_shader");
	for (int i = 0; i < shaders.size(); i++) {
		String path = shaders[i];
		Ref<Resource> res = ResourceLoader::load(path);
		if (res.is_valid()) {
			edit(res.ptr());
		}
		if (selected_shader == path) {
			selected_shader_idx = i;
		}
	}

	if (p_layout->has_section_key("ShaderEditor", "split_offset")) {
		main_split->set_split_offset(p_layout->get_value("ShaderEditor", "split_offset"));
	}

	_update_shader_list();
	_shader_selected(selected_shader_idx);

	_set_text_shader_zoom_factor(p_layout->get_value("ShaderEditor", "text_shader_zoom_factor", 1.0f));
}

void ShaderEditorPlugin::get_window_layout(Ref<ConfigFile> p_layout) {
	if (window_wrapper->get_window_enabled()) {
		p_layout->set_value("ShaderEditor", "window_rect", window_wrapper->get_window_rect());
		int screen = window_wrapper->get_window_screen();
		p_layout->set_value("ShaderEditor", "window_screen", screen);
		p_layout->set_value("ShaderEditor", "window_screen_rect", DisplayServer::get_singleton()->screen_get_usable_rect(screen));

	} else {
		if (p_layout->has_section_key("ShaderEditor", "window_rect")) {
			p_layout->erase_section_key("ShaderEditor", "window_rect");
		}
		if (p_layout->has_section_key("ShaderEditor", "window_screen")) {
			p_layout->erase_section_key("ShaderEditor", "window_screen");
		}
		if (p_layout->has_section_key("ShaderEditor", "window_screen_rect")) {
			p_layout->erase_section_key("ShaderEditor", "window_screen_rect");
		}
	}

	Array shaders;
	String selected_shader;
	for (int i = 0; i < shader_tabs->get_tab_count(); i++) {
		EditedShader edited_shader = edited_shaders[i];
		if (edited_shader.shader_editor) {
			String shader_path;
			if (edited_shader.shader.is_valid()) {
				shader_path = edited_shader.shader->get_path();
			} else {
				DEV_ASSERT(edited_shader.shader_inc.is_valid());
				shader_path = edited_shader.shader_inc->get_path();
			}
			shaders.push_back(shader_path);

			ShaderEditor *shader_editor = Object::cast_to<ShaderEditor>(shader_tabs->get_current_tab_control());

			if (shader_editor && edited_shader.shader_editor == shader_editor) {
				selected_shader = shader_path;
			}
		}
	}
	p_layout->set_value("ShaderEditor", "open_shaders", shaders);
	p_layout->set_value("ShaderEditor", "split_offset", main_split->get_split_offset());
	p_layout->set_value("ShaderEditor", "selected_shader", selected_shader);
	p_layout->set_value("ShaderEditor", "text_shader_zoom_factor", text_shader_zoom_factor);
}

String ShaderEditorPlugin::get_unsaved_status(const String &p_for_scene) const {
	// TODO: This should also include visual shaders and shader includes, but save_external_data() doesn't seem to save them...
	PackedStringArray unsaved_shaders;
	for (uint32_t i = 0; i < edited_shaders.size(); i++) {
		if (edited_shaders[i].shader_editor) {
			if (edited_shaders[i].shader_editor->is_unsaved()) {
				if (unsaved_shaders.is_empty()) {
					unsaved_shaders.append(TTR("Save changes to the following shaders(s) before quitting?"));
				}
				unsaved_shaders.append(edited_shaders[i].name.trim_suffix("(*)"));
			}
		}
	}

	if (!p_for_scene.is_empty()) {
		PackedStringArray unsaved_built_in_shaders;

		const String scene_file = p_for_scene.get_file();
		for (const String &E : unsaved_shaders) {
			if (!E.is_resource_file() && E.contains(scene_file)) {
				if (unsaved_built_in_shaders.is_empty()) {
					unsaved_built_in_shaders.append(TTR("There are unsaved changes in the following built-in shaders(s):"));
				}
				unsaved_built_in_shaders.append(E);
			}
		}

		if (!unsaved_built_in_shaders.is_empty()) {
			return String("\n").join(unsaved_built_in_shaders);
		}
		return String();
	}

	return String("\n").join(unsaved_shaders);
}

void ShaderEditorPlugin::save_external_data() {
	for (EditedShader &edited_shader : edited_shaders) {
		if (edited_shader.shader_editor) {
			edited_shader.shader_editor->save_external_data();
		}
	}
	_update_shader_list();
}

void ShaderEditorPlugin::apply_changes() {
	for (EditedShader &edited_shader : edited_shaders) {
		if (edited_shader.shader_editor) {
			edited_shader.shader_editor->apply_shaders();
		}
	}
}

void ShaderEditorPlugin::_shader_selected(int p_index) {
	if (p_index >= (int)edited_shaders.size()) {
		return;
	}

	if (edited_shaders[p_index].shader_editor) {
		edited_shaders[p_index].shader_editor->validate_script();
	}

	shader_tabs->set_current_tab(p_index);
	shader_list->select(p_index);
}

void ShaderEditorPlugin::_shader_list_clicked(int p_item, Vector2 p_local_mouse_pos, MouseButton p_mouse_button_index) {
	if (p_mouse_button_index == MouseButton::MIDDLE) {
		_close_shader(p_item);
	}
	if (p_mouse_button_index == MouseButton::RIGHT) {
		_make_script_list_context_menu();
	}
}

void ShaderEditorPlugin::_setup_popup_menu(PopupMenuType p_type, PopupMenu *p_menu) {
	if (p_type == FILE) {
		p_menu->add_shortcut(ED_SHORTCUT("shader_editor/new", TTRC("New Shader..."), KeyModifierMask::CMD_OR_CTRL | Key::N), FILE_NEW);
		p_menu->add_shortcut(ED_SHORTCUT("shader_editor/new_include", TTRC("New Shader Include..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::N), FILE_NEW_INCLUDE);
		p_menu->add_separator();
		p_menu->add_shortcut(ED_SHORTCUT("shader_editor/open", TTRC("Load Shader File..."), KeyModifierMask::CMD_OR_CTRL | Key::O), FILE_OPEN);
		p_menu->add_shortcut(ED_SHORTCUT("shader_editor/open_include", TTRC("Load Shader Include File..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::O), FILE_OPEN_INCLUDE);
	}

	if (p_type == FILE || p_type == CONTEXT_VALID_ITEM) {
		p_menu->add_shortcut(ED_SHORTCUT("shader_editor/save", TTRC("Save File"), KeyModifierMask::ALT | KeyModifierMask::CMD_OR_CTRL | Key::S), FILE_SAVE);
		p_menu->add_shortcut(ED_SHORTCUT("shader_editor/save_as", TTRC("Save File As...")), FILE_SAVE_AS);
	}

	if (p_type == FILE) {
		p_menu->add_separator();
		p_menu->add_item(TTR("Open File in Inspector"), FILE_INSPECT);
		p_menu->add_item(TTR("Inspect Native Shader Code..."), FILE_INSPECT_NATIVE_SHADER_CODE);
		p_menu->add_separator();
		p_menu->add_shortcut(ED_SHORTCUT("shader_editor/close_file", TTRC("Close File"), KeyModifierMask::CMD_OR_CTRL | Key::W), FILE_CLOSE);
	} else {
		p_menu->add_shortcut(ED_SHORTCUT("shader_editor/close_file", TTRC("Close File"), KeyModifierMask::CMD_OR_CTRL | Key::W), FILE_CLOSE);
		p_menu->add_item(TTR("Close All"), CLOSE_ALL);
		p_menu->add_item(TTR("Close Other Tabs"), CLOSE_OTHER_TABS);
		if (p_type == CONTEXT_VALID_ITEM) {
			p_menu->add_separator();
			p_menu->add_item(TTR("Copy Script Path"), COPY_PATH);
			p_menu->add_item(TTR("Show in FileSystem"), SHOW_IN_FILE_SYSTEM);
		}
	}
}

void ShaderEditorPlugin::_make_script_list_context_menu() {
	context_menu->clear();

	int selected = shader_tabs->get_current_tab();
	if (selected < 0 || selected >= shader_tabs->get_tab_count()) {
		return;
	}

	Control *control = shader_tabs->get_tab_control(selected);
	bool is_valid_editor_control = Object::cast_to<TextShaderEditor>(control) || Object::cast_to<VisualShaderEditor>(control);

	_setup_popup_menu(is_valid_editor_control ? CONTEXT_VALID_ITEM : CONTEXT, context_menu);

	context_menu->set_item_disabled(context_menu->get_item_index(CLOSE_ALL), shader_tabs->get_tab_count() <= 0);
	context_menu->set_item_disabled(context_menu->get_item_index(CLOSE_OTHER_TABS), shader_tabs->get_tab_count() <= 1);

	context_menu->set_position(main_split->get_screen_position() + main_split->get_local_mouse_position());
	context_menu->reset_size();
	context_menu->popup();
}

void ShaderEditorPlugin::_close_shader(int p_index) {
	ERR_FAIL_INDEX(p_index, shader_tabs->get_tab_count());
	Control *c = shader_tabs->get_tab_control(p_index);
	memdelete(c);
	edited_shaders.remove_at(p_index);
	_update_shader_list();
	EditorUndoRedoManager::get_singleton()->clear_history(); // To prevent undo on deleted graphs.

	if (shader_tabs->get_tab_count() == 0) {
		left_panel->show(); // Make sure the panel is visible, because it can't be toggled without open shaders.
	}
}

void ShaderEditorPlugin::_close_builtin_shaders_from_scene(const String &p_scene) {
	for (uint32_t i = 0; i < edited_shaders.size();) {
		Ref<Shader> &shader = edited_shaders[i].shader;
		if (shader.is_valid()) {
			if (shader->is_built_in() && shader->get_path().begins_with(p_scene)) {
				_close_shader(i);
				continue;
			}
		}
		Ref<ShaderInclude> &include = edited_shaders[i].shader_inc;
		if (include.is_valid()) {
			if (include->is_built_in() && include->get_path().begins_with(p_scene)) {
				_close_shader(i);
				continue;
			}
		}
		i++;
	}
}

void ShaderEditorPlugin::_resource_saved(Object *obj) {
	// May have been renamed on save.
	for (EditedShader &edited_shader : edited_shaders) {
		if (edited_shader.shader.ptr() == obj || edited_shader.shader_inc.ptr() == obj) {
			_update_shader_list();
			return;
		}
	}
}

void ShaderEditorPlugin::_menu_item_pressed(int p_index) {
	switch (p_index) {
		case FILE_NEW: {
			String base_path = FileSystemDock::get_singleton()->get_current_path().get_base_dir();
			shader_create_dialog->config(base_path.path_join("new_shader"), false, false, 0);
			shader_create_dialog->popup_centered();
		} break;
		case FILE_NEW_INCLUDE: {
			String base_path = FileSystemDock::get_singleton()->get_current_path().get_base_dir();
			shader_create_dialog->config(base_path.path_join("new_shader"), false, false, 2);
			shader_create_dialog->popup_centered();
		} break;
		case FILE_OPEN: {
			InspectorDock::get_singleton()->open_resource("Shader");
		} break;
		case FILE_OPEN_INCLUDE: {
			InspectorDock::get_singleton()->open_resource("ShaderInclude");
		} break;
		case FILE_SAVE: {
			int index = shader_tabs->get_current_tab();
			ERR_FAIL_INDEX(index, shader_tabs->get_tab_count());
			TextShaderEditor *editor = Object::cast_to<TextShaderEditor>(edited_shaders[index].shader_editor);
			if (editor) {
				if (editor->get_trim_trailing_whitespace_on_save()) {
					editor->trim_trailing_whitespace();
				}

				if (editor->get_trim_final_newlines_on_save()) {
					editor->trim_final_newlines();
				}
			}
			if (edited_shaders[index].shader.is_valid()) {
				EditorNode::get_singleton()->save_resource(edited_shaders[index].shader);
			} else {
				EditorNode::get_singleton()->save_resource(edited_shaders[index].shader_inc);
			}
			if (editor) {
				editor->tag_saved_version();
			}
		} break;
		case FILE_SAVE_AS: {
			int index = shader_tabs->get_current_tab();
			ERR_FAIL_INDEX(index, shader_tabs->get_tab_count());
			TextShaderEditor *editor = Object::cast_to<TextShaderEditor>(edited_shaders[index].shader_editor);
			if (editor) {
				if (editor->get_trim_trailing_whitespace_on_save()) {
					editor->trim_trailing_whitespace();
				}

				if (editor->get_trim_final_newlines_on_save()) {
					editor->trim_final_newlines();
				}
			}
			String path;
			if (edited_shaders[index].shader.is_valid()) {
				path = edited_shaders[index].shader->get_path();
				if (!path.is_resource_file()) {
					path = "";
				}
				EditorNode::get_singleton()->save_resource_as(edited_shaders[index].shader, path);
			} else {
				path = edited_shaders[index].shader_inc->get_path();
				if (!path.is_resource_file()) {
					path = "";
				}
				EditorNode::get_singleton()->save_resource_as(edited_shaders[index].shader_inc, path);
			}
			if (editor) {
				editor->tag_saved_version();
			}
		} break;
		case FILE_INSPECT: {
			int index = shader_tabs->get_current_tab();
			ERR_FAIL_INDEX(index, shader_tabs->get_tab_count());
			if (edited_shaders[index].shader.is_valid()) {
				EditorNode::get_singleton()->push_item(edited_shaders[index].shader.ptr());
			} else {
				EditorNode::get_singleton()->push_item(edited_shaders[index].shader_inc.ptr());
			}
		} break;
		case FILE_INSPECT_NATIVE_SHADER_CODE: {
			int index = shader_tabs->get_current_tab();
			if (edited_shaders[index].shader.is_valid()) {
				edited_shaders[index].shader->inspect_native_shader_code();
			}
		} break;
		case FILE_CLOSE: {
			_close_shader(shader_tabs->get_current_tab());
		} break;
		case CLOSE_ALL: {
			while (shader_tabs->get_tab_count() > 0) {
				_close_shader(0);
			}
		} break;
		case CLOSE_OTHER_TABS: {
			int index = shader_tabs->get_current_tab();
			for (int i = 0; i < index; i++) {
				_close_shader(0);
			}
			while (shader_tabs->get_tab_count() > 1) {
				_close_shader(1);
			}
		} break;
		case SHOW_IN_FILE_SYSTEM: {
			Ref<Resource> shader = _get_current_shader();
			String path = shader->get_path();
			if (!path.is_empty()) {
				FileSystemDock::get_singleton()->navigate_to_path(path);
			}
		} break;
		case COPY_PATH: {
			Ref<Resource> shader = _get_current_shader();
			DisplayServer::get_singleton()->clipboard_set(shader->get_path());
		} break;
	}
}

void ShaderEditorPlugin::_shader_created(Ref<Shader> p_shader) {
	EditorNode::get_singleton()->push_item(p_shader.ptr());
}

void ShaderEditorPlugin::_shader_include_created(Ref<ShaderInclude> p_shader_inc) {
	EditorNode::get_singleton()->push_item(p_shader_inc.ptr());
}

Variant ShaderEditorPlugin::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (shader_list->get_item_count() == 0) {
		return Variant();
	}

	int idx = shader_list->get_item_at_position(p_point);
	if (idx < 0) {
		return Variant();
	}

	HBoxContainer *drag_preview = memnew(HBoxContainer);
	String preview_name = shader_list->get_item_text(idx);
	Ref<Texture2D> preview_icon = shader_list->get_item_icon(idx);

	if (preview_icon.is_valid()) {
		TextureRect *tf = memnew(TextureRect);
		tf->set_texture(preview_icon);
		tf->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
		drag_preview->add_child(tf);
	}
	Label *label = memnew(Label(preview_name));
	label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED); // Don't translate script names.
	drag_preview->add_child(label);
	main_split->set_drag_preview(drag_preview);

	Dictionary drag_data;
	drag_data["type"] = "shader_list_element";
	drag_data["shader_list_element"] = idx;

	return drag_data;
}

bool ShaderEditorPlugin::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	Dictionary d = p_data;
	if (!d.has("type")) {
		return false;
	}

	if (String(d["type"]) == "shader_list_element") {
		return true;
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		if (files.size() == 0) {
			return false;
		}

		for (int i = 0; i < files.size(); i++) {
			const String &file = files[i];
			if (ResourceLoader::exists(file, "Shader")) {
				Ref<Shader> shader = ResourceLoader::load(file);
				if (shader.is_valid()) {
					return true;
				}
			}
			if (ResourceLoader::exists(file, "ShaderInclude")) {
				Ref<ShaderInclude> sinclude = ResourceLoader::load(file);
				if (sinclude.is_valid()) {
					return true;
				}
			}
		}
		return false;
	}

	return false;
}

void ShaderEditorPlugin::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	Dictionary d = p_data;
	if (!d.has("type")) {
		return;
	}

	if (String(d["type"]) == "shader_list_element") {
		int idx = d["shader_list_element"];
		int new_idx = shader_list->get_item_at_position(p_point);
		_move_shader_tab(idx, new_idx);
		return;
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		for (int i = 0; i < files.size(); i++) {
			const String &file = files[i];
			Ref<Resource> res;
			if (ResourceLoader::exists(file, "Shader") || ResourceLoader::exists(file, "ShaderInclude")) {
				res = ResourceLoader::load(file);
			}
			if (res.is_valid()) {
				edit(res.ptr());
			}
		}
	}
}

void ShaderEditorPlugin::_window_changed(bool p_visible) {
	make_floating->set_visible(!p_visible);
}

void ShaderEditorPlugin::_set_text_shader_zoom_factor(float p_zoom_factor) {
	if (text_shader_zoom_factor != p_zoom_factor) {
		text_shader_zoom_factor = p_zoom_factor;
		for (const EditedShader &edited_shader : edited_shaders) {
			TextShaderEditor *text_shader_editor = Object::cast_to<TextShaderEditor>(edited_shader.shader_editor);
			if (text_shader_editor) {
				CodeTextEditor *cte = text_shader_editor->get_code_editor();
				if (cte && cte->get_zoom_factor() != text_shader_zoom_factor) {
					cte->set_zoom_factor(text_shader_zoom_factor);
				}
			}
		}
	}
}

void ShaderEditorPlugin::_file_removed(const String &p_removed_file) {
	for (uint32_t i = 0; i < edited_shaders.size(); i++) {
		if (edited_shaders[i].path == p_removed_file) {
			_close_shader(i);
			break;
		}
	}
}

void ShaderEditorPlugin::_res_saved_callback(const Ref<Resource> &p_res) {
	if (p_res.is_null()) {
		return;
	}
	const String &path = p_res->get_path();

	for (EditedShader &edited : edited_shaders) {
		Ref<Resource> shader_res = edited.shader;
		if (shader_res.is_null()) {
			shader_res = edited.shader_inc;
		}
		ERR_FAIL_COND(shader_res.is_null());

		TextShaderEditor *text_shader_editor = Object::cast_to<TextShaderEditor>(edited.shader_editor);
		if (!text_shader_editor || !shader_res->is_built_in()) {
			continue;
		}

		if (shader_res->get_path().get_slice("::", 0) == path) {
			text_shader_editor->tag_saved_version();
			_update_shader_list();
		}
	}
}

void ShaderEditorPlugin::_set_file_specific_items_disabled(bool p_disabled) {
	PopupMenu *file_popup_menu = file_menu->get_popup();
	file_popup_menu->set_item_disabled(file_popup_menu->get_item_index(FILE_SAVE), p_disabled);
	file_popup_menu->set_item_disabled(file_popup_menu->get_item_index(FILE_SAVE_AS), p_disabled);
	file_popup_menu->set_item_disabled(file_popup_menu->get_item_index(FILE_INSPECT), p_disabled);
	file_popup_menu->set_item_disabled(file_popup_menu->get_item_index(FILE_INSPECT_NATIVE_SHADER_CODE), p_disabled);
	file_popup_menu->set_item_disabled(file_popup_menu->get_item_index(FILE_CLOSE), p_disabled);
}

void ShaderEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			EditorNode::get_singleton()->connect("resource_saved", callable_mp(this, &ShaderEditorPlugin::_resource_saved), CONNECT_DEFERRED);
			EditorNode::get_singleton()->connect("scene_closed", callable_mp(this, &ShaderEditorPlugin::_close_builtin_shaders_from_scene));
			FileSystemDock::get_singleton()->connect("file_removed", callable_mp(this, &ShaderEditorPlugin::_file_removed));
			EditorNode::get_singleton()->connect("resource_saved", callable_mp(this, &ShaderEditorPlugin::_res_saved_callback));
		} break;
	}
}

ShaderEditorPlugin::ShaderEditorPlugin() {
	window_wrapper = memnew(WindowWrapper);
	window_wrapper->set_window_title(vformat(TTR("%s - Godot Engine"), TTR("Shader Editor")));
	window_wrapper->set_margins_enabled(true);

	main_split = memnew(HSplitContainer);
	main_split->set_split_offset(200 * EDSCALE);
	Ref<Shortcut> make_floating_shortcut = ED_SHORTCUT_AND_COMMAND("shader_editor/make_floating", TTRC("Make Floating"));
	window_wrapper->set_wrapped_control(main_split, make_floating_shortcut);

	left_panel = memnew(VBoxContainer);

	HBoxContainer *menu_hb = memnew(HBoxContainer);
	left_panel->add_child(menu_hb);
	file_menu = memnew(MenuButton);
	file_menu->set_text(TTR("File"));
	file_menu->set_shortcut_context(main_split);
	_setup_popup_menu(FILE, file_menu->get_popup());
	file_menu->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &ShaderEditorPlugin::_menu_item_pressed));
	menu_hb->add_child(file_menu);

	_set_file_specific_items_disabled(true);

	context_menu = memnew(PopupMenu);
	add_child(context_menu);
	context_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ShaderEditorPlugin::_menu_item_pressed));

	Control *padding = memnew(Control);
	padding->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	menu_hb->add_child(padding);

	make_floating = memnew(ScreenSelect);
	make_floating->set_flat(true);
	make_floating->connect("request_open_in_screen", callable_mp(window_wrapper, &WindowWrapper::enable_window_on_screen).bind(true));
	if (!make_floating->is_disabled()) {
		// Override default ScreenSelect tooltip if multi-window support is available.
		make_floating->set_tooltip_text(TTR("Make the shader editor floating.\nRight-click to open the screen selector."));
	}

	menu_hb->add_child(make_floating);
	window_wrapper->connect("window_visibility_changed", callable_mp(this, &ShaderEditorPlugin::_window_changed));

	shader_list = memnew(ItemList);
	shader_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	shader_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	shader_list->set_theme_type_variation("ItemListSecondary");
	left_panel->add_child(shader_list);
	shader_list->connect(SceneStringName(item_selected), callable_mp(this, &ShaderEditorPlugin::_shader_selected));
	shader_list->connect("item_clicked", callable_mp(this, &ShaderEditorPlugin::_shader_list_clicked));
	shader_list->set_allow_rmb_select(true);
	SET_DRAG_FORWARDING_GCD(shader_list, ShaderEditorPlugin);

	main_split->add_child(left_panel);
	left_panel->set_custom_minimum_size(Size2(100, 300) * EDSCALE);

	shader_tabs = memnew(TabContainer);
	shader_tabs->set_tabs_visible(false);
	shader_tabs->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_split->add_child(shader_tabs);
	Ref<StyleBoxEmpty> empty;
	empty.instantiate();
	shader_tabs->add_theme_style_override(SceneStringName(panel), empty);

	button = EditorNode::get_bottom_panel()->add_item(TTR("Shader Editor"), window_wrapper, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_shader_editor_bottom_panel", TTRC("Toggle Shader Editor Bottom Panel"), KeyModifierMask::ALT | Key::S));

	shader_create_dialog = memnew(ShaderCreateDialog);
	main_split->add_child(shader_create_dialog);
	shader_create_dialog->connect("shader_created", callable_mp(this, &ShaderEditorPlugin::_shader_created));
	shader_create_dialog->connect("shader_include_created", callable_mp(this, &ShaderEditorPlugin::_shader_include_created));
}

ShaderEditorPlugin::~ShaderEditorPlugin() {
}
