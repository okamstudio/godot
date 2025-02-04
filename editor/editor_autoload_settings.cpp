/**************************************************************************/
/*  editor_autoload_settings.cpp                                          */
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

#include "editor_autoload_settings.h"

#include "core/config/project_settings.h"
#include "core/core_constants.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/filesystem_dock.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/project_settings_editor.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"

#define PREVIEW_LIST_MAX_SIZE 10

void EditorAutoloadSettings::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			List<String> afn;
			ResourceLoader::get_recognized_extensions_for_type("Script", &afn);
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &afn);

			for (const String &E : afn) {
				file_dialog->add_filter("*." + E);
			}

			browse_button->set_button_icon(get_editor_theme_icon(SNAME("Folder")));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			browse_button->set_button_icon(get_editor_theme_icon(SNAME("Folder")));
			add_autoload->set_button_icon(get_editor_theme_icon(SNAME("Add")));
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			FileSystemDock *dock = FileSystemDock::get_singleton();

			if (dock != nullptr) {
				ScriptCreateDialog *dialog = dock->get_script_create_dialog();

				if (dialog != nullptr) {
					Callable script_created = callable_mp(this, &EditorAutoloadSettings::_script_created);

					if (is_visible_in_tree()) {
						if (!dialog->is_connected(SNAME("script_created"), script_created)) {
							dialog->connect("script_created", script_created);
						}
					} else {
						if (dialog->is_connected(SNAME("script_created"), script_created)) {
							dialog->disconnect("script_created", script_created);
						}
					}
				}
			}
		} break;
	}
}

bool EditorAutoloadSettings::_autoload_name_is_valid(const String &p_name, String *r_error) {
	if (!p_name.is_valid_unicode_identifier()) {
		if (r_error) {
			*r_error = TTR("Invalid name.") + " " + TTR("Must be a valid Unicode identifier.");
		}

		return false;
	}

	if (ClassDB::class_exists(p_name)) {
		if (r_error) {
			*r_error = TTR("Invalid name.") + " " + TTR("Must not collide with an existing engine class name.");
		}

		return false;
	}

	if (ScriptServer::is_global_class(p_name)) {
		if (r_error) {
			*r_error = TTR("Invalid name.") + "\n" + TTR("Must not collide with an existing global script class name.");
		}

		return false;
	}

	if (Variant::get_type_by_name(p_name) < Variant::VARIANT_MAX) {
		if (r_error) {
			*r_error = TTR("Invalid name.") + " " + TTR("Must not collide with an existing built-in type name.");
		}

		return false;
	}

	for (int i = 0; i < CoreConstants::get_global_constant_count(); i++) {
		if (CoreConstants::get_global_constant_name(i) == p_name) {
			if (r_error) {
				*r_error = TTR("Invalid name.") + " " + TTR("Must not collide with an existing global constant name.");
			}

			return false;
		}
	}

	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		List<String> keywords;
		ScriptServer::get_language(i)->get_reserved_words(&keywords);
		for (const String &E : keywords) {
			if (E == p_name) {
				if (r_error) {
					*r_error = TTR("Invalid name.") + " " + TTR("Keyword cannot be used as an Autoload name.");
				}

				return false;
			}
		}
	}

	return true;
}

void EditorAutoloadSettings::_autoload_add() {
	if (autoload_add_path->get_text().is_empty()) {
		ScriptCreateDialog *dialog = FileSystemDock::get_singleton()->get_script_create_dialog();
		String fpath = path;
		if (!fpath.ends_with("/")) {
			fpath = fpath.get_base_dir();
		}
		dialog->config("Node", fpath.path_join(vformat("%s.gd", autoload_add_name->get_text())), false, false);
		dialog->popup_centered();
	} else {
		if (autoload_add(autoload_add_name->get_text(), autoload_add_path->get_text())) {
			autoload_add_path->set_text("");
		}

		autoload_add_name->set_text("");
		add_autoload->set_disabled(true);
	}
}

void EditorAutoloadSettings::_autoload_selected() {
	TreeItem *ti = tree->get_selected();

	if (!ti) {
		return;
	}

	selected_autoload = "autoload/" + ti->get_text(0);
}

void EditorAutoloadSettings::_autoload_edited() {
	if (updating_autoload) {
		return;
	}

	TreeItem *ti = tree->get_edited();
	int column = tree->get_edited_column();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	if (column == 0) {
		String name = ti->get_text(0);
		String old_name = selected_autoload.get_slice("/", 1);

		if (name == old_name) {
			return;
		}

		String error;
		if (!_autoload_name_is_valid(name, &error)) {
			ti->set_text(0, old_name);
			EditorNode::get_singleton()->show_warning(error);
			return;
		}

		if (ProjectSettings::get_singleton()->has_setting("autoload/" + name)) {
			ti->set_text(0, old_name);
			EditorNode::get_singleton()->show_warning(vformat(TTR("Autoload '%s' already exists!"), name));
			return;
		}

		updating_autoload = true;

		name = "autoload/" + name;

		int order = ProjectSettings::get_singleton()->get_order(selected_autoload);
		String scr_path = GLOBAL_GET(selected_autoload);

		undo_redo->create_action(TTR("Rename Autoload"));

		undo_redo->add_do_property(ProjectSettings::get_singleton(), name, scr_path);
		undo_redo->add_do_method(ProjectSettings::get_singleton(), "set_order", name, order);
		undo_redo->add_do_method(ProjectSettings::get_singleton(), "clear", selected_autoload);

		undo_redo->add_undo_property(ProjectSettings::get_singleton(), selected_autoload, scr_path);
		undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", selected_autoload, order);
		undo_redo->add_undo_method(ProjectSettings::get_singleton(), "clear", name);

		undo_redo->add_do_method(this, CoreStringName(call_deferred), "update_autoload");
		undo_redo->add_undo_method(this, CoreStringName(call_deferred), "update_autoload");

		undo_redo->add_do_method(this, "emit_signal", autoload_changed);
		undo_redo->add_undo_method(this, "emit_signal", autoload_changed);

		undo_redo->commit_action();

		selected_autoload = name;
	} else if (column == 2) {
		updating_autoload = true;

		bool checked = ti->is_checked(2);
		String base = "autoload/" + ti->get_text(0);

		int order = ProjectSettings::get_singleton()->get_order(base);
		String scr_path = GLOBAL_GET(base);

		if (scr_path.begins_with("*")) {
			scr_path = scr_path.substr(1);
		}

		// Singleton autoloads are represented with a leading "*" in their path.
		if (checked) {
			scr_path = "*" + scr_path;
		}

		undo_redo->create_action(TTR("Toggle Autoload Globals"));

		undo_redo->add_do_property(ProjectSettings::get_singleton(), base, scr_path);
		undo_redo->add_undo_property(ProjectSettings::get_singleton(), base, GLOBAL_GET(base));

		undo_redo->add_do_method(ProjectSettings::get_singleton(), "set_order", base, order);
		undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", base, order);

		undo_redo->add_do_method(this, CoreStringName(call_deferred), "update_autoload");
		undo_redo->add_undo_method(this, CoreStringName(call_deferred), "update_autoload");

		undo_redo->add_do_method(this, "emit_signal", autoload_changed);
		undo_redo->add_undo_method(this, "emit_signal", autoload_changed);

		undo_redo->commit_action();
	}

	updating_autoload = false;
}

void EditorAutoloadSettings::_autoload_button_pressed(Object *p_item, int p_column, int p_button, MouseButton p_mouse_button) {
	if (p_mouse_button != MouseButton::LEFT) {
		return;
	}
	TreeItem *ti = Object::cast_to<TreeItem>(p_item);

	String name = "autoload/" + ti->get_text(0);

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	switch (p_button) {
		case BUTTON_OPEN: {
			_autoload_open(ti->get_text(1));
		} break;
		case BUTTON_MOVE_UP:
		case BUTTON_MOVE_DOWN: {
			TreeItem *swap = nullptr;

			if (p_button == BUTTON_MOVE_UP) {
				swap = ti->get_prev();
			} else {
				swap = ti->get_next();
			}

			if (!swap) {
				return;
			}

			String swap_name = "autoload/" + swap->get_text(0);

			int order = ProjectSettings::get_singleton()->get_order(name);
			int swap_order = ProjectSettings::get_singleton()->get_order(swap_name);

			undo_redo->create_action(TTR("Move Autoload"));

			undo_redo->add_do_method(ProjectSettings::get_singleton(), "set_order", name, swap_order);
			undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", name, order);

			undo_redo->add_do_method(ProjectSettings::get_singleton(), "set_order", swap_name, order);
			undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", swap_name, swap_order);

			undo_redo->add_do_method(this, "update_autoload");
			undo_redo->add_undo_method(this, "update_autoload");

			undo_redo->add_do_method(this, "emit_signal", autoload_changed);
			undo_redo->add_undo_method(this, "emit_signal", autoload_changed);

			undo_redo->commit_action();
		} break;
		case BUTTON_DELETE: {
			int order = ProjectSettings::get_singleton()->get_order(name);

			undo_redo->create_action(TTR("Remove Autoload"));

			undo_redo->add_do_property(ProjectSettings::get_singleton(), name, Variant());

			undo_redo->add_undo_property(ProjectSettings::get_singleton(), name, GLOBAL_GET(name));
			undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", name, order);

			undo_redo->add_do_method(this, "update_autoload");
			undo_redo->add_undo_method(this, "update_autoload");

			undo_redo->add_do_method(this, "emit_signal", autoload_changed);
			undo_redo->add_undo_method(this, "emit_signal", autoload_changed);

			undo_redo->commit_action();
		} break;
	}
}

void EditorAutoloadSettings::_autoload_activated() {
	TreeItem *ti = tree->get_selected();
	if (!ti) {
		return;
	}
	_autoload_open(ti->get_text(1));
}

void EditorAutoloadSettings::_autoload_open(const String &fpath) {
	if (ResourceLoader::get_resource_type(fpath) == "PackedScene") {
		EditorNode::get_singleton()->open_request(fpath);
	} else {
		EditorNode::get_singleton()->load_resource(fpath);
	}
	ProjectSettingsEditor::get_singleton()->hide();
}

void EditorAutoloadSettings::_autoload_file_callback(const String &p_path) {
	// Convert the file name to PascalCase, which is the convention for classes in GDScript.
	const String class_name = p_path.get_file().get_basename().to_pascal_case();

	// If the name collides with a built-in class, prefix the name to make it possible to add without having to edit the name.
	// The prefix is subjective, but it provides better UX than leaving the Add button disabled :)
	const String prefix = ClassDB::class_exists(class_name) ? "Global" : "";

	autoload_add_name->set_text(prefix + class_name);
	add_autoload->set_disabled(false);
}

void EditorAutoloadSettings::_autoload_text_submitted(const String &p_name) {
	if (!autoload_add_path->get_text().is_empty() && _autoload_name_is_valid(p_name, nullptr)) {
		_autoload_add();
	}
}

void EditorAutoloadSettings::_autoload_path_text_changed(const String &p_path) {
	add_autoload->set_disabled(!_autoload_name_is_valid(autoload_add_name->get_text(), nullptr));
}

void EditorAutoloadSettings::_autoload_text_changed(const String &p_name) {
	String error_string;
	bool is_name_valid = _autoload_name_is_valid(p_name, &error_string);
	add_autoload->set_disabled(!is_name_valid);
	error_message->set_text(error_string);
	error_message->set_visible(!autoload_add_name->get_text().is_empty() && !is_name_valid);
}

Node *EditorAutoloadSettings::_create_autoload(const String &p_path) {
	Node *n = nullptr;
	if (ResourceLoader::get_resource_type(p_path) == "PackedScene") {
		// Cache the scene reference before loading it (for cyclic references)
		Ref<PackedScene> scn;
		scn.instantiate();
		scn->set_path(p_path);
		scn->reload_from_file();
		ERR_FAIL_COND_V_MSG(scn.is_null(), nullptr, vformat("Failed to create an autoload, can't load from path: %s.", p_path));

		if (scn.is_valid()) {
			n = scn->instantiate();
		}
	} else {
		Ref<Resource> res = ResourceLoader::load(p_path);
		ERR_FAIL_COND_V_MSG(res.is_null(), nullptr, vformat("Failed to create an autoload, can't load from path: %s.", p_path));

		Ref<Script> scr = res;
		if (scr.is_valid()) {
			ERR_FAIL_COND_V_MSG(!scr->is_valid(), nullptr, vformat("Failed to create an autoload, script '%s' is not compiling.", p_path));

			StringName ibt = scr->get_instance_base_type();
			bool valid_type = ClassDB::is_parent_class(ibt, "Node");
			ERR_FAIL_COND_V_MSG(!valid_type, nullptr, vformat("Failed to create an autoload, script '%s' does not inherit from 'Node'.", p_path));

			Object *obj = ClassDB::instantiate(ibt);
			ERR_FAIL_NULL_V_MSG(obj, nullptr, vformat("Failed to create an autoload, cannot instantiate '%s'.", ibt));

			n = Object::cast_to<Node>(obj);
			n->set_script(scr);
		}
	}

	ERR_FAIL_NULL_V_MSG(n, nullptr, vformat("Failed to create an autoload, path is not pointing to a scene or a script: %s.", p_path));

	return n;
}

void EditorAutoloadSettings::init_autoloads() {
	for (AutoloadInfo &info : autoload_cache) {
		info.node = _create_autoload(info.path);

		if (info.node) {
			Ref<Script> scr = info.node->get_script();
			info.in_editor = scr.is_valid() && scr->is_tool();
			info.node->set_name(info.name);
		}

		if (info.is_singleton) {
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->add_named_global_constant(info.name, info.node);
			}
		}

		if (!info.is_singleton && !info.in_editor && info.node != nullptr) {
			memdelete(info.node);
			info.node = nullptr;
		}
	}

	for (const AutoloadInfo &info : autoload_cache) {
		if (info.node && info.in_editor) {
			// It's important to add the node without deferring because code in plugins or tool scripts
			// could use the autoload node when they are enabled.
			get_tree()->get_root()->add_child(info.node);
		}
	}
}

void EditorAutoloadSettings::update_autoload() {
	if (updating_autoload) {
		return;
	}

	updating_autoload = true;

	HashMap<String, AutoloadInfo> to_remove;
	List<AutoloadInfo *> to_add;

	for (const AutoloadInfo &info : autoload_cache) {
		to_remove.insert(info.name, info);
	}

	autoload_cache.clear();

	tree->clear();
	TreeItem *root = tree->create_item();

	List<PropertyInfo> props;
	ProjectSettings::get_singleton()->get_property_list(&props);

	for (const PropertyInfo &pi : props) {
		if (!pi.name.begins_with("autoload/")) {
			continue;
		}

		String name = pi.name.get_slice("/", 1);
		String scr_path = GLOBAL_GET(pi.name);

		if (name.is_empty()) {
			continue;
		}

		AutoloadInfo info;
		info.is_singleton = scr_path.begins_with("*");

		if (info.is_singleton) {
			scr_path = scr_path.substr(1);
		}

		info.name = name;
		info.path = scr_path;
		info.order = ProjectSettings::get_singleton()->get_order(pi.name);

		bool need_to_add = true;
		if (to_remove.has(name)) {
			AutoloadInfo &old_info = to_remove[name];
			if (old_info.path == info.path) {
				// Still the same resource, check status
				info.node = old_info.node;
				if (info.node) {
					Ref<Script> scr = info.node->get_script();
					info.in_editor = scr.is_valid() && scr->is_tool();
					if (info.is_singleton == old_info.is_singleton && info.in_editor == old_info.in_editor) {
						to_remove.erase(name);
						need_to_add = false;
					} else {
						info.node = nullptr;
					}
				}
			}
		}

		autoload_cache.push_back(info);

		if (need_to_add) {
			to_add.push_back(&(autoload_cache.back()->get()));
		}

		TreeItem *item = tree->create_item(root);
		item->set_text(0, name);
		item->set_editable(0, true);

		item->set_text(1, scr_path);
		item->set_selectable(1, true);

		item->set_cell_mode(2, TreeItem::CELL_MODE_CHECK);
		item->set_editable(2, true);
		item->set_text(2, TTR("Enable"));
		item->set_checked(2, info.is_singleton);
		item->add_button(3, get_editor_theme_icon(SNAME("Load")), BUTTON_OPEN);
		item->add_button(3, get_editor_theme_icon(SNAME("MoveUp")), BUTTON_MOVE_UP);
		item->add_button(3, get_editor_theme_icon(SNAME("MoveDown")), BUTTON_MOVE_DOWN);
		item->add_button(3, get_editor_theme_icon(SNAME("Remove")), BUTTON_DELETE);
		item->set_selectable(3, false);
	}

	// Remove deleted/changed autoloads
	for (KeyValue<String, AutoloadInfo> &E : to_remove) {
		AutoloadInfo &info = E.value;
		if (info.is_singleton) {
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->remove_named_global_constant(info.name);
			}
		}
		if (info.in_editor) {
			ERR_CONTINUE(!info.node);
			callable_mp((Node *)get_tree()->get_root(), &Node::remove_child).call_deferred(info.node);
		}

		if (info.node) {
			info.node->queue_free();
			info.node = nullptr;
		}
	}

	// Load new/changed autoloads
	List<Node *> nodes_to_add;
	for (AutoloadInfo *info : to_add) {
		info->node = _create_autoload(info->path);

		ERR_CONTINUE(!info->node);
		info->node->set_name(info->name);

		Ref<Script> scr = info->node->get_script();
		info->in_editor = scr.is_valid() && scr->is_tool();

		if (info->in_editor) {
			//defer so references are all valid on _ready()
			nodes_to_add.push_back(info->node);
		}

		if (info->is_singleton) {
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->add_named_global_constant(info->name, info->node);
			}
		}

		if (!info->in_editor && !info->is_singleton) {
			// No reason to keep this node
			memdelete(info->node);
			info->node = nullptr;
		}
	}

	for (Node *E : nodes_to_add) {
		get_tree()->get_root()->add_child(E);
	}

	updating_autoload = false;
}

void EditorAutoloadSettings::_script_created(Ref<Script> p_script) {
	FileSystemDock::get_singleton()->get_script_create_dialog()->hide();
	path = p_script->get_path().get_base_dir();
	autoload_add_path->set_text(p_script->get_path());
	autoload_add_name->set_text(p_script->get_path().get_file().get_basename().to_pascal_case());
	_autoload_add();
}

LineEdit *EditorAutoloadSettings::get_path_box() const {
	return autoload_add_path;
}

Variant EditorAutoloadSettings::get_drag_data_fw(const Point2 &p_point, Control *p_control) {
	if (autoload_cache.size() <= 1) {
		return false;
	}

	PackedStringArray autoloads;

	TreeItem *next = tree->get_next_selected(nullptr);

	while (next) {
		autoloads.push_back(next->get_text(0));
		next = tree->get_next_selected(next);
	}

	if (autoloads.size() == 0 || autoloads.size() == autoload_cache.size()) {
		return Variant();
	}

	VBoxContainer *preview = memnew(VBoxContainer);

	int max_size = MIN(PREVIEW_LIST_MAX_SIZE, autoloads.size());

	for (int i = 0; i < max_size; i++) {
		Label *label = memnew(Label(autoloads[i]));
		label->set_self_modulate(Color(1, 1, 1, Math::lerp(1, 0, float(i) / PREVIEW_LIST_MAX_SIZE)));
		label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

		preview->add_child(label);
	}

	tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN);
	tree->set_drag_preview(preview);

	Dictionary drop_data;
	drop_data["type"] = "autoload";
	drop_data["autoloads"] = autoloads;

	return drop_data;
}

bool EditorAutoloadSettings::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_control) const {
	if (updating_autoload) {
		return false;
	}

	Dictionary drop_data = p_data;

	if (!drop_data.has("type")) {
		return false;
	}

	if (drop_data.has("type")) {
		TreeItem *ti = tree->get_item_at_position(p_point);

		if (!ti) {
			return false;
		}

		int section = tree->get_drop_section_at_position(p_point);

		return section >= -1;
	}

	return false;
}

void EditorAutoloadSettings::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_control) {
	TreeItem *ti = tree->get_item_at_position(p_point);

	if (!ti) {
		return;
	}

	int section = tree->get_drop_section_at_position(p_point);

	if (section < -1) {
		return;
	}

	String name;
	bool move_to_back = false;

	if (section < 0) {
		name = ti->get_text(0);
	} else if (ti->get_next()) {
		name = ti->get_next()->get_text(0);
	} else {
		name = ti->get_text(0);
		move_to_back = true;
	}

	int order = ProjectSettings::get_singleton()->get_order("autoload/" + name);

	AutoloadInfo aux;
	List<AutoloadInfo>::Element *E = nullptr;

	if (!move_to_back) {
		aux.order = order;
		E = autoload_cache.find(aux);
	}

	Dictionary drop_data = p_data;
	PackedStringArray autoloads = drop_data["autoloads"];

	// Store the initial order of the autoloads for comparison.
	Vector<int> initial_orders;
	initial_orders.resize(autoload_cache.size());
	int idx = 0;
	for (const AutoloadInfo &F : autoload_cache) {
		initial_orders.write[idx++] = F.order;
	}

	// Perform the drag-and-drop operation.
	Vector<int> orders;
	orders.resize(autoload_cache.size());

	for (int i = 0; i < autoloads.size(); i++) {
		aux.order = ProjectSettings::get_singleton()->get_order("autoload/" + autoloads[i]);

		List<AutoloadInfo>::Element *I = autoload_cache.find(aux);

		if (move_to_back) {
			autoload_cache.move_to_back(I);
		} else if (E != I) {
			autoload_cache.move_before(I, E);
		} else if (E->next()) {
			E = E->next();
		} else {
			break;
		}
	}

	idx = 0;
	for (const AutoloadInfo &F : autoload_cache) {
		orders.write[idx++] = F.order;
	}

	// If the order didn't change, we shouldn't create undo/redo actions.
	if (orders == initial_orders) {
		return;
	}

	orders.sort();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	undo_redo->create_action(TTR("Rearrange Autoloads"));

	idx = 0;
	for (const AutoloadInfo &F : autoload_cache) {
		undo_redo->add_do_method(ProjectSettings::get_singleton(), "set_order", "autoload/" + F.name, orders[idx++]);
		undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", "autoload/" + F.name, F.order);
	}

	orders.clear();

	undo_redo->add_do_method(this, "update_autoload");
	undo_redo->add_undo_method(this, "update_autoload");

	undo_redo->add_do_method(this, "emit_signal", autoload_changed);
	undo_redo->add_undo_method(this, "emit_signal", autoload_changed);

	undo_redo->commit_action();
}

bool EditorAutoloadSettings::autoload_add(const String &p_name, const String &p_path) {
	String name = p_name;

	String error;
	if (!_autoload_name_is_valid(name, &error)) {
		EditorNode::get_singleton()->show_warning(TTR("Can't add Autoload:") + "\n" + error);
		return false;
	}

	if (!FileAccess::exists(p_path)) {
		EditorNode::get_singleton()->show_warning(TTR("Can't add Autoload:") + "\n" + vformat(TTR("%s is an invalid path. File does not exist."), p_path));
		return false;
	}

	if (!p_path.begins_with("res://")) {
		EditorNode::get_singleton()->show_warning(TTR("Can't add Autoload:") + "\n" + vformat(TTR("%s is an invalid path. Not in resource path (res://)."), p_path));
		return false;
	}

	name = "autoload/" + name;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	undo_redo->create_action(TTR("Add Autoload"));
	// Singleton autoloads are represented with a leading "*" in their path.
	undo_redo->add_do_property(ProjectSettings::get_singleton(), name, "*" + p_path);

	if (ProjectSettings::get_singleton()->has_setting(name)) {
		undo_redo->add_undo_property(ProjectSettings::get_singleton(), name, GLOBAL_GET(name));
	} else {
		undo_redo->add_undo_property(ProjectSettings::get_singleton(), name, Variant());
	}

	undo_redo->add_do_method(this, "update_autoload");
	undo_redo->add_undo_method(this, "update_autoload");

	undo_redo->add_do_method(this, "emit_signal", autoload_changed);
	undo_redo->add_undo_method(this, "emit_signal", autoload_changed);

	undo_redo->commit_action();

	return true;
}

void EditorAutoloadSettings::autoload_remove(const String &p_name) {
	String name = "autoload/" + p_name;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	int order = ProjectSettings::get_singleton()->get_order(name);

	undo_redo->create_action(TTR("Remove Autoload"));

	undo_redo->add_do_property(ProjectSettings::get_singleton(), name, Variant());

	undo_redo->add_undo_property(ProjectSettings::get_singleton(), name, GLOBAL_GET(name));
	undo_redo->add_undo_method(ProjectSettings::get_singleton(), "set_order", name, order);

	undo_redo->add_do_method(this, "update_autoload");
	undo_redo->add_undo_method(this, "update_autoload");

	undo_redo->add_do_method(this, "emit_signal", autoload_changed);
	undo_redo->add_undo_method(this, "emit_signal", autoload_changed);

	undo_redo->commit_action();
}

void EditorAutoloadSettings::_bind_methods() {
	ClassDB::bind_method("update_autoload", &EditorAutoloadSettings::update_autoload);
	ClassDB::bind_method("autoload_add", &EditorAutoloadSettings::autoload_add);
	ClassDB::bind_method("autoload_remove", &EditorAutoloadSettings::autoload_remove);

	ADD_SIGNAL(MethodInfo("autoload_changed"));
}

EditorAutoloadSettings::EditorAutoloadSettings() {
	ProjectSettings::get_singleton()->add_hidden_prefix("autoload/");

	// Make first cache
	List<PropertyInfo> props;
	ProjectSettings::get_singleton()->get_property_list(&props);
	for (const PropertyInfo &pi : props) {
		if (!pi.name.begins_with("autoload/")) {
			continue;
		}

		String name = pi.name.get_slice("/", 1);
		String scr_path = GLOBAL_GET(pi.name);

		if (name.is_empty()) {
			continue;
		}

		AutoloadInfo info;
		info.is_singleton = scr_path.begins_with("*");

		if (info.is_singleton) {
			scr_path = scr_path.substr(1);
		}

		info.name = name;
		info.path = scr_path;
		info.order = ProjectSettings::get_singleton()->get_order(pi.name);

		if (info.is_singleton) {
			// Make sure name references work before parsing scripts
			for (int i = 0; i < ScriptServer::get_language_count(); i++) {
				ScriptServer::get_language(i)->add_named_global_constant(info.name, Variant());
			}
		}

		autoload_cache.push_back(info);
	}

	HBoxContainer *hbc = memnew(HBoxContainer);
	add_child(hbc);

	error_message = memnew(Label);
	error_message->hide();
	error_message->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	error_message->add_theme_color_override(SceneStringName(font_color), EditorNode::get_singleton()->get_editor_theme()->get_color(SNAME("error_color"), EditorStringName(Editor)));
	add_child(error_message);

	Label *l = memnew(Label);
	l->set_text(TTR("Path:"));
	hbc->add_child(l);

	autoload_add_path = memnew(LineEdit);
	hbc->add_child(autoload_add_path);
	autoload_add_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	autoload_add_path->set_clear_button_enabled(true);
	autoload_add_path->set_placeholder(vformat(TTR("Set path or press \"%s\" to create a script."), TTR("Add")));
	autoload_add_path->connect(SceneStringName(text_changed), callable_mp(this, &EditorAutoloadSettings::_autoload_path_text_changed));

	browse_button = memnew(Button);
	hbc->add_child(browse_button);
	browse_button->connect(SceneStringName(pressed), callable_mp(this, &EditorAutoloadSettings::_browse_autoload_add_path));

	file_dialog = memnew(EditorFileDialog);
	hbc->add_child(file_dialog);
	file_dialog->connect("file_selected", callable_mp(this, &EditorAutoloadSettings::_set_autoload_add_path));
	file_dialog->connect("dir_selected", callable_mp(this, &EditorAutoloadSettings::_set_autoload_add_path));
	file_dialog->connect("files_selected", callable_mp(this, &EditorAutoloadSettings::_set_autoload_add_path));

	hbc->set_h_size_flags(SIZE_EXPAND_FILL);
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	file_dialog->connect("file_selected", callable_mp(this, &EditorAutoloadSettings::_autoload_file_callback));

	l = memnew(Label);
	l->set_text(TTR("Node Name:"));
	hbc->add_child(l);

	autoload_add_name = memnew(LineEdit);
	autoload_add_name->set_h_size_flags(SIZE_EXPAND_FILL);
	autoload_add_name->connect(SceneStringName(text_submitted), callable_mp(this, &EditorAutoloadSettings::_autoload_text_submitted));
	autoload_add_name->connect(SceneStringName(text_changed), callable_mp(this, &EditorAutoloadSettings::_autoload_text_changed));
	hbc->add_child(autoload_add_name);

	add_autoload = memnew(Button);
	add_autoload->set_text(TTR("Add"));
	add_autoload->connect(SceneStringName(pressed), callable_mp(this, &EditorAutoloadSettings::_autoload_add));
	// The button will be enabled once a valid name is entered (either automatically or manually).
	add_autoload->set_disabled(true);
	hbc->add_child(add_autoload);

	tree = memnew(Tree);
	tree->set_hide_root(true);
	tree->set_select_mode(Tree::SELECT_MULTI);
	tree->set_allow_reselect(true);

	SET_DRAG_FORWARDING_GCD(tree, EditorAutoloadSettings);

	tree->set_columns(4);
	tree->set_column_titles_visible(true);

	tree->set_column_title(0, TTR("Name"));
	tree->set_column_expand(0, true);
	tree->set_column_expand_ratio(0, 1);

	tree->set_column_title(1, TTR("Path"));
	tree->set_column_expand(1, true);
	tree->set_column_clip_content(1, true);
	tree->set_column_expand_ratio(1, 2);

	tree->set_column_title(2, TTR("Global Variable"));
	tree->set_column_expand(2, false);

	tree->set_column_expand(3, false);

	tree->connect("cell_selected", callable_mp(this, &EditorAutoloadSettings::_autoload_selected));
	tree->connect("item_edited", callable_mp(this, &EditorAutoloadSettings::_autoload_edited));
	tree->connect("button_clicked", callable_mp(this, &EditorAutoloadSettings::_autoload_button_pressed));
	tree->connect("item_activated", callable_mp(this, &EditorAutoloadSettings::_autoload_activated));
	tree->set_v_size_flags(SIZE_EXPAND_FILL);

	add_child(tree, true);
}

EditorAutoloadSettings::~EditorAutoloadSettings() {
	for (const AutoloadInfo &info : autoload_cache) {
		if (info.node && !info.in_editor) {
			memdelete(info.node);
		}
	}
}

void EditorAutoloadSettings::_set_autoload_add_path(const String &p_text) {
	autoload_add_path->set_text(p_text);
	autoload_add_path->emit_signal(SceneStringName(text_submitted), p_text);
}

void EditorAutoloadSettings::_browse_autoload_add_path() {
	file_dialog->popup_file_dialog();
}
