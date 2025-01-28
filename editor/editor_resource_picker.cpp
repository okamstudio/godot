/**************************************************************************/
/*  editor_resource_picker.cpp                                            */
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

#include "editor_resource_picker.h"

#include "editor/audio_stream_preview.h"
#include "editor/editor_help.h"
#include "editor/editor_node.h"
#include "editor/editor_resource_preview.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/filesystem_dock.h"
#include "editor/gui/editor_quick_open_dialog.h"
#include "editor/plugins/editor_resource_conversion_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/scene_tree_dock.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/image_texture.h"

void EditorResourcePicker::_update_resource() {
	String resource_path;
	if (edited_resource.is_valid() && edited_resource->get_path().is_resource_file()) {
		resource_path = edited_resource->get_path() + "\n";
	}
	String class_name = _get_resource_type(edited_resource);

	if (preview_rect) {
		preview_rect->set_texture(Ref<Texture2D>());

		assign_button->set_custom_minimum_size(assign_button_min_size);

		if (edited_resource == Ref<Resource>()) {
			assign_button->set_button_icon(Ref<Texture2D>());
			assign_button->set_text(TTR("<empty>"));
			assign_button->set_tooltip_text("");
		} else {
			assign_button->set_button_icon(EditorNode::get_singleton()->get_object_icon(edited_resource.operator->(), SNAME("Object")));

			if (!edited_resource->get_name().is_empty()) {
				assign_button->set_text(edited_resource->get_name());
			} else if (edited_resource->get_path().is_resource_file()) {
				assign_button->set_text(edited_resource->get_path().get_file());
			} else {
				assign_button->set_text(class_name);
			}

			if (edited_resource->get_path().is_resource_file()) {
				resource_path = edited_resource->get_path() + "\n";
			}
			assign_button->set_tooltip_text(resource_path + TTR("Type:") + " " + class_name);

			// Preview will override the above, so called at the end.
			EditorResourcePreview::get_singleton()->queue_edited_resource_preview(edited_resource, this, "_update_resource_preview", edited_resource->get_instance_id());
		}
	} else if (edited_resource.is_valid()) {
		assign_button->set_tooltip_text(resource_path + TTR("Type:") + " " + edited_resource->get_class());
	}

	assign_button->set_disabled(!editable && edited_resource.is_null());
}

void EditorResourcePicker::_update_resource_preview(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, ObjectID p_obj) {
	if (edited_resource.is_null() || edited_resource->get_instance_id() != p_obj) {
		return;
	}

	if (preview_rect) {
		Ref<Script> scr = edited_resource;
		if (scr.is_valid()) {
			assign_button->set_text(scr->get_path().get_file());
			return;
		}

		if (p_preview.is_valid()) {
			preview_rect->set_offset(SIDE_LEFT, assign_button->get_button_icon()->get_width() + assign_button->get_theme_stylebox(CoreStringName(normal))->get_content_margin(SIDE_LEFT) + get_theme_constant(SNAME("h_separation"), SNAME("Button")));

			// Resource-specific stretching.
			if (Ref<GradientTexture1D>(edited_resource).is_valid() || Ref<Gradient>(edited_resource).is_valid()) {
				preview_rect->set_stretch_mode(TextureRect::STRETCH_SCALE);
				assign_button->set_custom_minimum_size(assign_button_min_size);
			} else {
				preview_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
				int thumbnail_size = EDITOR_GET("filesystem/file_dialog/thumbnail_size");
				thumbnail_size *= EDSCALE;
				assign_button->set_custom_minimum_size(assign_button_min_size.max(Size2(1, thumbnail_size)));
			}

			preview_rect->set_texture(p_preview);
			assign_button->set_text("");
		}
	}
}

void EditorResourcePicker::_resource_selected() {
	if (edited_resource.is_null()) {
		edit_button->set_pressed(true);
		_update_menu();
		return;
	}

	emit_signal(SNAME("resource_selected"), edited_resource, false);
}

void EditorResourcePicker::_resource_changed() {
	emit_signal(SNAME("resource_changed"), edited_resource);
	_update_resource();
}

void EditorResourcePicker::_file_selected(const String &p_path) {
	Ref<Resource> loaded_resource = ResourceLoader::load(p_path);
	ERR_FAIL_COND_MSG(loaded_resource.is_null(), "Cannot load resource from path '" + p_path + "'.");

	if (!base_type.is_empty()) {
		bool any_type_matches = false;

		String res_type = loaded_resource->get_class();
		Ref<Script> res_script = loaded_resource->get_script();
		bool is_global_class = false;
		if (res_script.is_valid()) {
			String script_type = EditorNode::get_editor_data().script_class_get_name(res_script->get_path());
			if (!script_type.is_empty()) {
				is_global_class = true;
				res_type = script_type;
			}
		}

		for (int i = 0; i < base_type.get_slice_count(","); i++) {
			String base = base_type.get_slice(",", i);

			any_type_matches = is_global_class ? EditorNode::get_editor_data().script_class_is_parent(res_type, base) : loaded_resource->is_class(base);

			if (any_type_matches) {
				break;
			}
		}

		if (!any_type_matches) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("The selected resource (%s) does not match any type expected for this property (%s)."), res_type, base_type));
			return;
		}
	}

	edited_resource = loaded_resource;
	_resource_changed();
}

void EditorResourcePicker::_resource_saved(Object *p_resource) {
	if (edited_resource.is_valid() && p_resource == edited_resource.ptr()) {
		emit_signal(SNAME("resource_changed"), edited_resource);
		_update_resource();
	}
}

void EditorResourcePicker::_load_button_pressed() {
	Vector<StringName> base_types;
	for (const String &type : base_type.split(",")) {
		base_types.push_back(type);
	}

	EditorNode::get_singleton()->get_quick_open_dialog()->popup_dialog(base_types, callable_mp(this, &EditorResourcePicker::_file_selected));
}

void EditorResourcePicker::_update_menu() {
	_update_menu_items();

	Rect2 gt = edit_button->get_screen_rect();
	edit_menu->reset_size();
	int ms = edit_menu->get_contents_minimum_size().width;
	Vector2 popup_pos = gt.get_end() - Vector2(ms, 0);
	edit_menu->set_position(popup_pos);
	edit_menu->popup();
}

void EditorResourcePicker::_update_menu_items() {
	_ensure_resource_menu();
	edit_menu->clear();

	bool added_create_options = false;

	// Add options for creating specific subtypes of the base resource type.
	if (is_editable()) {
		int previous_item_count = edit_menu->get_item_count();

		set_create_options(edit_menu);

		added_create_options = previous_item_count != edit_menu->get_item_count();
	}

	// Add options for changing existing value of the resource.
	if (edited_resource.is_valid()) {
		if (added_create_options) {
			edit_menu->add_separator();
		}

		// Determine if the edited resource is part of another scene (foreign) which was imported
		bool is_edited_resource_foreign_import = EditorNode::get_singleton()->is_resource_read_only(edited_resource, true);

		// If the resource is determined to be foreign and imported, change the menu entry's description to 'inspect' rather than 'edit'
		// since will only be able to view its properties in read-only mode.
		if (is_edited_resource_foreign_import) {
			// The 'Search' icon is a magnifying glass, which seems appropriate, but maybe a bespoke icon is preferred here.
			edit_menu->add_icon_item(get_editor_theme_icon(SNAME("Search")), TTR("Inspect"), OBJ_MENU_INSPECT);
		} else {
			edit_menu->add_icon_item(get_editor_theme_icon(SNAME("Edit")), TTR("Edit"), OBJ_MENU_INSPECT);
		}

		if (is_editable()) {
			if (!_is_custom_type_script()) {
				edit_menu->add_icon_item(get_editor_theme_icon(SNAME("Clear")), TTR("Clear"), OBJ_MENU_CLEAR);
			}
			edit_menu->add_icon_item(get_editor_theme_icon(SNAME("Duplicate")), TTR("Make Unique"), OBJ_MENU_MAKE_UNIQUE);

			// Check whether the resource has subresources.
			List<PropertyInfo> property_list;
			edited_resource->get_property_list(&property_list);
			bool has_subresources = false;
			for (PropertyInfo &p : property_list) {
				if ((p.type == Variant::OBJECT) && (p.hint == PROPERTY_HINT_RESOURCE_TYPE) && (p.name != "script") && ((Object *)edited_resource->get(p.name) != nullptr)) {
					has_subresources = true;
					break;
				}
			}
			if (has_subresources) {
				edit_menu->add_icon_item(get_editor_theme_icon(SNAME("Duplicate")), TTR("Make Unique (Recursive)"), OBJ_MENU_MAKE_UNIQUE_RECURSIVE);
			}

			edit_menu->add_icon_item(get_editor_theme_icon(SNAME("Save")), TTR("Save"), OBJ_MENU_SAVE);
			edit_menu->add_icon_item(get_editor_theme_icon(SNAME("Save")), TTR("Save As..."), OBJ_MENU_SAVE_AS);
		}

		if (edited_resource->get_path().is_resource_file()) {
			edit_menu->add_separator();
			edit_menu->add_icon_item(get_editor_theme_icon(SNAME("ShowInFileSystem")), TTR("Show in FileSystem"), OBJ_MENU_SHOW_IN_FILE_SYSTEM);
		}
	}

	// Add options to copy/paste resource.
	Ref<Resource> cb = EditorSettings::get_singleton()->get_resource_clipboard();
	bool paste_valid = false;
	if (is_editable() && cb.is_valid()) {
		if (base_type.is_empty()) {
			paste_valid = true;
		} else {
			String res_type = _get_resource_type(cb);

			for (int i = 0; i < base_type.get_slice_count(","); i++) {
				String base = base_type.get_slice(",", i);

				paste_valid = ClassDB::is_parent_class(res_type, base) || EditorNode::get_editor_data().script_class_is_parent(res_type, base);

				if (paste_valid) {
					break;
				}
			}
		}
	}

	if (edited_resource.is_valid() || paste_valid) {
		edit_menu->add_separator();

		if (edited_resource.is_valid()) {
			edit_menu->add_item(TTR("Copy"), OBJ_MENU_COPY);
		}

		if (paste_valid) {
			edit_menu->add_item(TTR("Paste"), OBJ_MENU_PASTE);
		}
	}

	// Add options to convert existing resource to another type of resource.
	if (is_editable() && edited_resource.is_valid()) {
		Vector<Ref<EditorResourceConversionPlugin>> conversions = EditorNode::get_singleton()->find_resource_conversion_plugin_for_resource(edited_resource);
		if (!conversions.is_empty()) {
			edit_menu->add_separator();
		}
		int relative_id = 0;
		for (const Ref<EditorResourceConversionPlugin> &conversion : conversions) {
			String what = conversion->converts_to();
			Ref<Texture2D> icon;
			if (has_theme_icon(what, EditorStringName(EditorIcons))) {
				icon = get_editor_theme_icon(what);
			} else {
				icon = get_editor_theme_icon(SNAME("Object"));
			}

			edit_menu->add_icon_item(icon, vformat(TTR("Convert to %s"), what), CONVERT_BASE_ID + relative_id);
			relative_id++;
		}
	}
}

void EditorResourcePicker::_edit_menu_cbk(int p_which) {
	switch (p_which) {
		case OBJ_MENU_INSPECT: {
			if (edited_resource.is_valid()) {
				emit_signal(SNAME("resource_selected"), edited_resource, true);
			}
		} break;

		case OBJ_MENU_CLEAR: {
			edited_resource = Ref<Resource>();
			_resource_changed();
		} break;

		case OBJ_MENU_MAKE_UNIQUE: {
			if (edited_resource.is_null()) {
				return;
			}

			Ref<Resource> unique_resource = edited_resource->duplicate();
			ERR_FAIL_COND(unique_resource.is_null()); // duplicate() may fail.

			edited_resource = unique_resource;
			_resource_changed();
		} break;

		case OBJ_MENU_MAKE_UNIQUE_RECURSIVE: {
			if (edited_resource.is_null()) {
				return;
			}

			if (!duplicate_resources_dialog) {
				duplicate_resources_dialog = memnew(ConfirmationDialog);
				add_child(duplicate_resources_dialog);
				duplicate_resources_dialog->set_title(TTR("Make Unique (Recursive)"));
				duplicate_resources_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorResourcePicker::_duplicate_selected_resources));

				VBoxContainer *vb = memnew(VBoxContainer);
				duplicate_resources_dialog->add_child(vb);

				Label *label = memnew(Label(TTR("Select resources to make unique:")));
				vb->add_child(label);

				duplicate_resources_tree = memnew(Tree);
				duplicate_resources_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
				vb->add_child(duplicate_resources_tree);
				duplicate_resources_tree->set_columns(2);
				duplicate_resources_tree->set_v_size_flags(SIZE_EXPAND_FILL);
			}

			duplicate_resources_tree->clear();
			TreeItem *root = duplicate_resources_tree->create_item();
			_gather_resources_to_duplicate(edited_resource, root);

			duplicate_resources_dialog->reset_size();
			duplicate_resources_dialog->popup_centered(Vector2(500, 400) * EDSCALE);
		} break;

		case OBJ_MENU_SAVE: {
			if (edited_resource.is_null()) {
				return;
			}
			EditorNode::get_singleton()->save_resource(edited_resource);
		} break;

		case OBJ_MENU_SAVE_AS: {
			if (edited_resource.is_null()) {
				return;
			}
			Callable resource_saved = callable_mp(this, &EditorResourcePicker::_resource_saved);
			if (!EditorNode::get_singleton()->is_connected("resource_saved", resource_saved)) {
				EditorNode::get_singleton()->connect("resource_saved", resource_saved);
			}
			EditorNode::get_singleton()->save_resource_as(edited_resource);
		} break;

		case OBJ_MENU_COPY: {
			EditorSettings::get_singleton()->set_resource_clipboard(edited_resource);
		} break;

		case OBJ_MENU_PASTE: {
			edited_resource = EditorSettings::get_singleton()->get_resource_clipboard();
			if (edited_resource->is_built_in() && EditorNode::get_singleton()->get_edited_scene() &&
					edited_resource->get_path().get_slice("::", 0) != EditorNode::get_singleton()->get_edited_scene()->get_scene_file_path()) {
				// Automatically make resource unique if it belongs to another scene.
				_edit_menu_cbk(OBJ_MENU_MAKE_UNIQUE);
				return;
			}
			_resource_changed();
		} break;

		case OBJ_MENU_SHOW_IN_FILE_SYSTEM: {
			FileSystemDock::get_singleton()->navigate_to_path(edited_resource->get_path());
		} break;

		default: {
			// Allow subclasses to handle their own options first, only then fallback on the default branch logic.
			if (handle_menu_selected(p_which)) {
				break;
			}

			if (p_which >= CONVERT_BASE_ID) {
				int to_type = p_which - CONVERT_BASE_ID;
				Vector<Ref<EditorResourceConversionPlugin>> conversions = EditorNode::get_singleton()->find_resource_conversion_plugin_for_resource(edited_resource);
				ERR_FAIL_INDEX(to_type, conversions.size());

				edited_resource = conversions[to_type]->convert(edited_resource);
				_resource_changed();
				break;
			}

			ERR_FAIL_COND(inheritors_array.is_empty());

			String intype = inheritors_array[p_which - TYPE_BASE_ID];
			Variant obj;

			if (ScriptServer::is_global_class(intype)) {
				obj = EditorNode::get_editor_data().script_class_instance(intype);
			} else {
				obj = ClassDB::instantiate(intype);
			}

			if (!obj) {
				obj = EditorNode::get_editor_data().instantiate_custom_type(intype, "Resource");
			}

			Resource *resp = Object::cast_to<Resource>(obj);
			ERR_BREAK(!resp);

			EditorNode::get_editor_data().instantiate_object_properties(obj);

			// Prevent freeing of the object until the end of the update of the resource (GH-88286).
			Ref<Resource> old_edited_resource = edited_resource;
			edited_resource = Ref<Resource>(resp);
			_resource_changed();
		} break;
	}
}

void EditorResourcePicker::set_create_options(Object *p_menu_node) {
	_ensure_resource_menu();
	// If a subclass implements this method, use it to replace all create items.
	if (GDVIRTUAL_CALL(_set_create_options, p_menu_node)) {
		return;
	}

	// By default provide generic "New ..." options.
	if (!base_type.is_empty()) {
		int idx = 0;

		_ensure_allowed_types();
		HashSet<StringName> allowed_types = allowed_types_without_convert;

		for (const StringName &E : allowed_types) {
			const String &type = E;

			if (!ClassDB::can_instantiate(type)) {
				continue;
			}

			inheritors_array.push_back(type);

			Ref<Texture2D> icon = EditorNode::get_singleton()->get_class_icon(type, "Object");
			int id = TYPE_BASE_ID + idx;
			edit_menu->add_icon_item(icon, vformat(TTR("New %s"), type), id);

			HashMap<String, DocData::ClassDoc>::Iterator class_doc = EditorHelp::get_doc_data()->class_list.find(type);
			if (class_doc) {
				edit_menu->set_item_tooltip(-1, DTR(class_doc->value.brief_description));
			}

			idx++;
		}
	}
}

bool EditorResourcePicker::handle_menu_selected(int p_which) {
	bool success = false;
	GDVIRTUAL_CALL(_handle_menu_selected, p_which, success);
	return success;
}

void EditorResourcePicker::_button_draw() {
	if (dropping) {
		Color color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		assign_button->draw_rect(Rect2(Point2(), assign_button->get_size()), color, false);
	}
}

void EditorResourcePicker::_button_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::RIGHT) {
		// Only attempt to update and show the menu if we have
		// a valid resource or the Picker is editable, as
		// there will otherwise be nothing to display.
		if (edited_resource.is_valid() || is_editable()) {
			_update_menu_items();

			Vector2 pos = get_screen_position() + mb->get_position();
			edit_menu->reset_size();
			edit_menu->set_position(pos);
			edit_menu->popup();
		}
	}
}

String EditorResourcePicker::_get_resource_type(const Ref<Resource> &p_resource) const {
	if (p_resource.is_null()) {
		return String();
	}
	String res_type = p_resource->get_class();

	Ref<Script> res_script = p_resource->get_script();
	if (res_script.is_null()) {
		return res_type;
	}

	// TODO: Replace with EditorFileSystem when PR #60606 is merged to use cached resource type.
	String script_type = EditorNode::get_editor_data().script_class_get_name(res_script->get_path());
	if (!script_type.is_empty()) {
		res_type = script_type;
	}
	return res_type;
}

static void _add_allowed_type(const StringName &p_type, List<StringName> *p_vector) {
	if (p_vector->find(p_type)) {
		// Already added.
		return;
	}

	if (ClassDB::class_exists(p_type)) {
		// Engine class,

		if (!ClassDB::is_virtual(p_type)) {
			p_vector->push_back(p_type);
		}

		List<StringName> inheriters;
		ClassDB::get_inheriters_from_class(p_type, &inheriters);
		for (const StringName &S : inheriters) {
			_add_allowed_type(S, p_vector);
		}
	} else {
		// Script class.
		p_vector->push_back(p_type);
	}

	List<StringName> inheriters;
	ScriptServer::get_inheriters_list(p_type, &inheriters);
	for (const StringName &S : inheriters) {
		_add_allowed_type(S, p_vector);
	}
}

void EditorResourcePicker::_ensure_allowed_types() const {
	if (!allowed_types_without_convert.is_empty()) {
		return;
	}

	List<StringName> final_allowed;

	Vector<String> allowed_types = base_type.split(",");
	int size = allowed_types.size();

	for (const String &S : allowed_types) {
		const String base = S.strip_edges();
		if (base.begins_with("-")) {
			final_allowed.erase(base.right(-1));
			continue;
		}
		_add_allowed_type(base, &final_allowed);
	}

	for (const StringName &SN : final_allowed) {
		allowed_types_without_convert.insert(SN);
	}

	allowed_types_with_convert = HashSet<StringName>(allowed_types_without_convert);

	for (int i = 0; i < size; i++) {
		const String base = allowed_types[i].strip_edges();
		if (base == "BaseMaterial3D") {
			allowed_types_with_convert.insert("Texture2D");
		} else if (ClassDB::is_parent_class("ShaderMaterial", base)) {
			allowed_types_with_convert.insert("Shader");
		} else if (ClassDB::is_parent_class("ImageTexture", base)) {
			allowed_types_with_convert.insert("Image");
		}
	}
}

bool EditorResourcePicker::_is_drop_valid(const Dictionary &p_drag_data) const {
	{
		const ObjectID source_picker = p_drag_data.get("source_picker", ObjectID());
		if (source_picker == get_instance_id()) {
			return false;
		}
	}

	if (base_type.is_empty()) {
		return true;
	}

	Dictionary drag_data = p_drag_data;

	Ref<Resource> res;
	if (drag_data.has("type") && String(drag_data["type"]) == "script_list_element") {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(drag_data["script_list_element"]);
		if (se) {
			res = se->get_edited_resource();
		}
	} else if (drag_data.has("type") && String(drag_data["type"]) == "resource") {
		res = drag_data["resource"];
	} else if (drag_data.has("type") && String(drag_data["type"]) == "files") {
		Vector<String> files = drag_data["files"];

		// TODO: Extract the typename of the dropped filepath's resource in a more performant way, without fully loading it.
		if (files.size() == 1) {
			res = ResourceLoader::load(files[0]);
		}
	}

	_ensure_allowed_types();
	HashSet<StringName> allowed_types = allowed_types_with_convert;

	if (res.is_valid()) {
		String res_type = _get_resource_type(res);

		if (_is_type_valid(res_type, allowed_types)) {
			return true;
		}

		StringName custom_class = EditorNode::get_singleton()->get_object_custom_type_name(res.ptr());
		if (_is_type_valid(custom_class, allowed_types)) {
			return true;
		}
	}

	return false;
}

bool EditorResourcePicker::_is_type_valid(const String &p_type_name, const HashSet<StringName> &p_allowed_types) const {
	for (const StringName &E : p_allowed_types) {
		String at = E;
		if (p_type_name == at || ClassDB::is_parent_class(p_type_name, at) || EditorNode::get_editor_data().script_class_is_parent(p_type_name, at)) {
			return true;
		}
	}

	return false;
}

bool EditorResourcePicker::_is_custom_type_script() const {
	Ref<Script> resource_as_script = edited_resource;

	if (resource_as_script.is_valid() && resource_owner && resource_owner->has_meta(SceneStringName(_custom_type_script))) {
		return true;
	}

	return false;
}

Variant EditorResourcePicker::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (edited_resource.is_valid()) {
		Dictionary drag_data = EditorNode::get_singleton()->drag_resource(edited_resource, p_from);
		drag_data["source_picker"] = get_instance_id();
		return drag_data;
	}

	return Variant();
}

bool EditorResourcePicker::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	return editable && _is_drop_valid(p_data);
}

void EditorResourcePicker::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	ERR_FAIL_COND(!_is_drop_valid(p_data));

	Dictionary drag_data = p_data;

	Ref<Resource> dropped_resource;
	if (drag_data.has("type") && String(drag_data["type"]) == "script_list_element") {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(drag_data["script_list_element"]);
		if (se) {
			dropped_resource = se->get_edited_resource();
		}
	} else if (drag_data.has("type") && String(drag_data["type"]) == "resource") {
		dropped_resource = drag_data["resource"];
	}

	if (dropped_resource.is_null() && drag_data.has("type") && String(drag_data["type"]) == "files") {
		Vector<String> files = drag_data["files"];

		if (files.size() == 1) {
			dropped_resource = ResourceLoader::load(files[0]);
		}
	}

	if (dropped_resource.is_valid()) {
		_ensure_allowed_types();
		HashSet<StringName> allowed_types = allowed_types_without_convert;

		String res_type = _get_resource_type(dropped_resource);

		// If the accepted dropped resource is from the extended list, it requires conversion.
		if (!_is_type_valid(res_type, allowed_types)) {
			for (const StringName &E : allowed_types) {
				String at = E;

				if (at == "BaseMaterial3D" && Ref<Texture2D>(dropped_resource).is_valid()) {
					// Use existing resource if possible and only replace its data.
					Ref<StandardMaterial3D> mat = edited_resource;
					if (mat.is_null()) {
						mat.instantiate();
					}
					mat->set_texture(StandardMaterial3D::TextureParam::TEXTURE_ALBEDO, dropped_resource);
					dropped_resource = mat;
					break;
				}

				if (at == "ShaderMaterial" && Ref<Shader>(dropped_resource).is_valid()) {
					Ref<ShaderMaterial> mat = edited_resource;
					if (mat.is_null()) {
						mat.instantiate();
					}
					mat->set_shader(dropped_resource);
					dropped_resource = mat;
					break;
				}

				if (at == "ImageTexture" && Ref<Image>(dropped_resource).is_valid()) {
					Ref<ImageTexture> texture = edited_resource;
					if (texture.is_null()) {
						texture.instantiate();
					}
					texture->set_image(dropped_resource);
					dropped_resource = texture;
					break;
				}
			}
		}

		edited_resource = dropped_resource;
		_resource_changed();
	}
}

void EditorResourcePicker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_resource_preview"), &EditorResourcePicker::_update_resource_preview);

	ClassDB::bind_method(D_METHOD("set_base_type", "base_type"), &EditorResourcePicker::set_base_type);
	ClassDB::bind_method(D_METHOD("get_base_type"), &EditorResourcePicker::get_base_type);
	ClassDB::bind_method(D_METHOD("get_allowed_types"), &EditorResourcePicker::get_allowed_types);
	ClassDB::bind_method(D_METHOD("set_edited_resource", "resource"), &EditorResourcePicker::set_edited_resource);
	ClassDB::bind_method(D_METHOD("get_edited_resource"), &EditorResourcePicker::get_edited_resource);
	ClassDB::bind_method(D_METHOD("set_toggle_mode", "enable"), &EditorResourcePicker::set_toggle_mode);
	ClassDB::bind_method(D_METHOD("is_toggle_mode"), &EditorResourcePicker::is_toggle_mode);
	ClassDB::bind_method(D_METHOD("set_toggle_pressed", "pressed"), &EditorResourcePicker::set_toggle_pressed);
	ClassDB::bind_method(D_METHOD("set_editable", "enable"), &EditorResourcePicker::set_editable);
	ClassDB::bind_method(D_METHOD("is_editable"), &EditorResourcePicker::is_editable);

	GDVIRTUAL_BIND(_set_create_options, "menu_node");
	GDVIRTUAL_BIND(_handle_menu_selected, "id");

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_type"), "set_base_type", "get_base_type");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "edited_resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource", PROPERTY_USAGE_NONE), "set_edited_resource", "get_edited_resource");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editable"), "set_editable", "is_editable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "toggle_mode"), "set_toggle_mode", "is_toggle_mode");

	ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource"), PropertyInfo(Variant::BOOL, "inspect")));
	ADD_SIGNAL(MethodInfo("resource_changed", PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource")));
}

void EditorResourcePicker::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_resource();
			[[fallthrough]];
		}
		case NOTIFICATION_THEME_CHANGED: {
			const int icon_width = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));
			assign_button->add_theme_constant_override("icon_max_width", icon_width);
			if (edit_menu) {
				edit_menu->add_theme_constant_override("icon_max_width", icon_width);
			}

			load_button->set_button_icon(get_editor_theme_icon(SNAME("Load")));
			edit_button->set_button_icon(get_theme_icon(SNAME("select_arrow"), SNAME("Tree")));
		} break;

		case NOTIFICATION_DRAW: {
			draw_style_box(get_theme_stylebox(SceneStringName(panel), SNAME("Tree")), Rect2(Point2(), get_size()));
		} break;

		case NOTIFICATION_DRAG_BEGIN: {
			if (editable && _is_drop_valid(get_viewport()->gui_get_drag_data())) {
				dropping = true;
				assign_button->queue_redraw();
			}
		} break;

		case NOTIFICATION_DRAG_END: {
			if (dropping) {
				dropping = false;
				assign_button->queue_redraw();
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			Callable resource_saved = callable_mp(this, &EditorResourcePicker::_resource_saved);
			if (EditorNode::get_singleton()->is_connected("resource_saved", resource_saved)) {
				EditorNode::get_singleton()->disconnect("resource_saved", resource_saved);
			}
		} break;
	}
}

void EditorResourcePicker::set_assign_button_min_size(const Size2i &p_size) {
	assign_button_min_size = p_size;
	assign_button->set_custom_minimum_size(assign_button_min_size);
}

void EditorResourcePicker::set_base_type(const String &p_base_type) {
	base_type = p_base_type;

	// There is a possibility that the new base type is conflicting with the existing value.
	// Keep the value, but warn the user that there is a potential mistake.
	if (!base_type.is_empty() && edited_resource.is_valid()) {
		_ensure_allowed_types();
		HashSet<StringName> allowed_types = allowed_types_with_convert;

		StringName custom_class;
		bool is_custom = false;
		if (edited_resource->get_script()) {
			custom_class = EditorNode::get_singleton()->get_object_custom_type_name(edited_resource->get_script());
			is_custom = _is_type_valid(custom_class, allowed_types);
		}

		if (!is_custom && !_is_type_valid(edited_resource->get_class(), allowed_types)) {
			String class_str = (custom_class == StringName() ? edited_resource->get_class() : vformat("%s (%s)", custom_class, edited_resource->get_class()));
			WARN_PRINT(vformat("Value mismatch between the new base type of this EditorResourcePicker, '%s', and the type of the value it already has, '%s'.", base_type, class_str));
		}
	}
}

String EditorResourcePicker::get_base_type() const {
	return base_type;
}

Vector<String> EditorResourcePicker::get_allowed_types() const {
	_ensure_allowed_types();
	HashSet<StringName> allowed_types = allowed_types_without_convert;

	Vector<String> types;
	types.resize(allowed_types.size());

	int i = 0;
	String *w = types.ptrw();
	for (const StringName &E : allowed_types) {
		w[i] = E;
		i++;
	}

	return types;
}

void EditorResourcePicker::set_edited_resource(Ref<Resource> p_resource) {
	if (p_resource.is_null()) {
		edited_resource = Ref<Resource>();
		_update_resource();
		return;
	}

	if (!base_type.is_empty()) {
		_ensure_allowed_types();
		HashSet<StringName> allowed_types = allowed_types_with_convert;

		StringName custom_class;
		bool is_custom = false;
		if (p_resource->get_script()) {
			custom_class = EditorNode::get_singleton()->get_object_custom_type_name(p_resource->get_script());
			is_custom = _is_type_valid(custom_class, allowed_types);
		}

		if (!is_custom && !_is_type_valid(p_resource->get_class(), allowed_types)) {
			String class_str = (custom_class == StringName() ? p_resource->get_class() : vformat("%s (%s)", custom_class, p_resource->get_class()));
			ERR_FAIL_MSG(vformat("Failed to set a resource of the type '%s' because this EditorResourcePicker only accepts '%s' and its derivatives.", class_str, base_type));
		}
	}
	set_edited_resource_no_check(p_resource);
}

void EditorResourcePicker::set_edited_resource_no_check(Ref<Resource> p_resource) {
	edited_resource = p_resource;
	_update_resource();
}

Ref<Resource> EditorResourcePicker::get_edited_resource() {
	return edited_resource;
}

void EditorResourcePicker::set_toggle_mode(bool p_enable) {
	assign_button->set_toggle_mode(p_enable);
}

bool EditorResourcePicker::is_toggle_mode() const {
	return assign_button->is_toggle_mode();
}

void EditorResourcePicker::set_toggle_pressed(bool p_pressed) {
	if (!is_toggle_mode()) {
		return;
	}

	assign_button->set_pressed(p_pressed);
}

bool EditorResourcePicker::is_toggle_pressed() const {
	return assign_button->is_pressed();
}

void EditorResourcePicker::set_resource_owner(Object *p_object) {
	resource_owner = p_object;
}

void EditorResourcePicker::set_editable(bool p_editable) {
	editable = p_editable;
	assign_button->set_disabled(!editable && edited_resource.is_null());
	load_button->set_visible(editable);
	edit_button->set_visible(editable);
}

bool EditorResourcePicker::is_editable() const {
	return editable;
}

void EditorResourcePicker::_ensure_resource_menu() {
	if (edit_menu) {
		return;
	}
	edit_menu = memnew(PopupMenu);
	edit_menu->add_theme_constant_override("icon_max_width", get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor)));
	add_child(edit_menu);
	edit_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorResourcePicker::_edit_menu_cbk));
	edit_menu->connect("popup_hide", callable_mp((BaseButton *)edit_button, &BaseButton::set_pressed).bind(false));
}

void EditorResourcePicker::_gather_resources_to_duplicate(const Ref<Resource> p_resource, TreeItem *p_item, const String &p_property_name) const {
	p_item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);

	String res_name = p_resource->get_name();
	if (res_name.is_empty() && !p_resource->is_built_in()) {
		res_name = p_resource->get_path().get_file();
	}

	if (res_name.is_empty()) {
		p_item->set_text(0, p_resource->get_class());
	} else {
		p_item->set_text(0, vformat("%s (%s)", p_resource->get_class(), res_name));
	}

	p_item->set_icon(0, EditorNode::get_singleton()->get_object_icon(p_resource.ptr()));
	p_item->set_editable(0, true);

	Array meta;
	meta.append(p_resource);
	p_item->set_metadata(0, meta);

	if (!p_property_name.is_empty()) {
		p_item->set_text(1, p_property_name);
	}

	static Vector<String> unique_exceptions = { "Image", "Shader", "Mesh", "FontFile" };
	if (!unique_exceptions.has(p_resource->get_class())) {
		// Automatically select resource, unless it's something that shouldn't be duplicated.
		p_item->set_checked(0, true);
	}

	List<PropertyInfo> plist;
	p_resource->get_property_list(&plist);

	for (const PropertyInfo &E : plist) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE) || E.type != Variant::OBJECT || E.hint != PROPERTY_HINT_RESOURCE_TYPE) {
			continue;
		}

		Ref<Resource> res = p_resource->get(E.name);
		if (res.is_null()) {
			continue;
		}

		TreeItem *child = p_item->create_child();
		_gather_resources_to_duplicate(res, child, E.name);

		meta = child->get_metadata(0);
		// Remember property name.
		meta.append(E.name);

		if ((E.usage & PROPERTY_USAGE_NEVER_DUPLICATE)) {
			// The resource can't be duplicated, but make it appear on the list anyway.
			child->set_checked(0, false);
			child->set_editable(0, false);
		}
	}
}

void EditorResourcePicker::_duplicate_selected_resources() {
	for (TreeItem *item = duplicate_resources_tree->get_root(); item; item = item->get_next_in_tree()) {
		if (!item->is_checked(0)) {
			continue;
		}

		Array meta = item->get_metadata(0);
		Ref<Resource> res = meta[0];
		Ref<Resource> unique_resource = res->duplicate();
		ERR_FAIL_COND(unique_resource.is_null()); // duplicate() may fail.
		meta[0] = unique_resource;

		if (meta.size() == 1) { // Root.
			edited_resource = unique_resource;
			_resource_changed();
		} else {
			Array parent_meta = item->get_parent()->get_metadata(0);
			Ref<Resource> parent = parent_meta[0];
			parent->set(meta[1], unique_resource);
		}
	}
}

EditorResourcePicker::EditorResourcePicker(bool p_hide_assign_button_controls) {
	assign_button = memnew(Button);
	assign_button->set_flat(true);
	assign_button->set_action_mode(BaseButton::ACTION_MODE_BUTTON_PRESS);
	assign_button->set_h_size_flags(SIZE_EXPAND_FILL);
	assign_button->set_expand_icon(true);
	assign_button->set_clip_text(true);
	assign_button->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	SET_DRAG_FORWARDING_GCD(assign_button, EditorResourcePicker);
	add_child(assign_button);
	assign_button->connect(SceneStringName(pressed), callable_mp(this, &EditorResourcePicker::_resource_selected));
	assign_button->connect(SceneStringName(draw), callable_mp(this, &EditorResourcePicker::_button_draw));
	assign_button->connect(SceneStringName(gui_input), callable_mp(this, &EditorResourcePicker::_button_input));

	if (!p_hide_assign_button_controls) {
		preview_rect = memnew(TextureRect);
		preview_rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		preview_rect->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
		preview_rect->set_offset(SIDE_TOP, 1);
		preview_rect->set_offset(SIDE_BOTTOM, -1);
		preview_rect->set_offset(SIDE_RIGHT, -1);
		preview_rect->set_texture_filter(TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
		assign_button->add_child(preview_rect);
	}

	load_button = memnew(Button);
	load_button->set_flat(false);
	add_child(load_button);
	load_button->connect(SceneStringName(pressed), callable_mp(this, &EditorResourcePicker::_load_button_pressed));

	edit_button = memnew(Button);
	edit_button->set_flat(false);
	edit_button->set_toggle_mode(true);
	edit_button->set_action_mode(BaseButton::ACTION_MODE_BUTTON_PRESS);
	add_child(edit_button);
	edit_button->connect(SceneStringName(pressed), callable_mp(this, &EditorResourcePicker::_update_menu));
	edit_button->connect(SceneStringName(gui_input), callable_mp(this, &EditorResourcePicker::_button_input));

	add_theme_constant_override("separation", 0);
}

// EditorScriptPicker

void EditorScriptPicker::set_create_options(Object *p_menu_node) {
	PopupMenu *menu_node = Object::cast_to<PopupMenu>(p_menu_node);
	if (!menu_node) {
		return;
	}

	if (!(script_owner && script_owner->has_meta(SceneStringName(_custom_type_script)))) {
		menu_node->add_icon_item(get_editor_theme_icon(SNAME("ScriptCreate")), TTR("New Script..."), OBJ_MENU_NEW_SCRIPT);
	}

	if (script_owner) {
		Ref<Script> scr = script_owner->get_script();
		if (scr.is_valid()) {
			menu_node->add_icon_item(get_editor_theme_icon(SNAME("ScriptExtend")), TTR("Extend Script..."), OBJ_MENU_EXTEND_SCRIPT);
		}
	}
}

bool EditorScriptPicker::handle_menu_selected(int p_which) {
	switch (p_which) {
		case OBJ_MENU_NEW_SCRIPT: {
			if (script_owner) {
				SceneTreeDock::get_singleton()->open_script_dialog(script_owner, false);
			}
			return true;
		}

		case OBJ_MENU_EXTEND_SCRIPT: {
			if (script_owner) {
				SceneTreeDock::get_singleton()->open_script_dialog(script_owner, true);
			}
			return true;
		}
	}

	return false;
}

void EditorScriptPicker::set_script_owner(Node *p_owner) {
	script_owner = p_owner;
}

Node *EditorScriptPicker::get_script_owner() const {
	return script_owner;
}

void EditorScriptPicker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_script_owner", "owner_node"), &EditorScriptPicker::set_script_owner);
	ClassDB::bind_method(D_METHOD("get_script_owner"), &EditorScriptPicker::get_script_owner);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "script_owner", PROPERTY_HINT_RESOURCE_TYPE, "Node", PROPERTY_USAGE_NONE), "set_script_owner", "get_script_owner");
}

EditorScriptPicker::EditorScriptPicker() {
}

// EditorShaderPicker

void EditorShaderPicker::set_create_options(Object *p_menu_node) {
	PopupMenu *menu_node = Object::cast_to<PopupMenu>(p_menu_node);
	if (!menu_node) {
		return;
	}

	menu_node->add_icon_item(get_editor_theme_icon(SNAME("Shader")), TTR("New Shader..."), OBJ_MENU_NEW_SHADER);
}

bool EditorShaderPicker::handle_menu_selected(int p_which) {
	Ref<ShaderMaterial> ed_material = Ref<ShaderMaterial>(get_edited_material());

	switch (p_which) {
		case OBJ_MENU_NEW_SHADER: {
			if (ed_material.is_valid()) {
				SceneTreeDock::get_singleton()->open_shader_dialog(ed_material, preferred_mode);
				return true;
			}
		} break;
		default:
			break;
	}
	return false;
}

void EditorShaderPicker::set_edited_material(ShaderMaterial *p_material) {
	edited_material = p_material;
}

ShaderMaterial *EditorShaderPicker::get_edited_material() const {
	return edited_material;
}

void EditorShaderPicker::set_preferred_mode(int p_mode) {
	preferred_mode = p_mode;
}

EditorShaderPicker::EditorShaderPicker() {
}

//////////////

void EditorAudioStreamPicker::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY:
		case NOTIFICATION_THEME_CHANGED: {
			_update_resource();
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			Ref<AudioStream> audio_stream = get_edited_resource();
			if (audio_stream.is_valid()) {
				if (audio_stream->get_length() > 0) {
					Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(audio_stream);
					if (preview.is_valid()) {
						if (preview->get_version() != last_preview_version) {
							stream_preview_rect->queue_redraw();
							last_preview_version = preview->get_version();
						}
					}
				}

				uint64_t tagged_frame = audio_stream->get_tagged_frame();
				uint64_t diff_frames = AudioServer::get_singleton()->get_mixed_frames() - tagged_frame;
				uint64_t diff_msec = diff_frames * 1000 / AudioServer::get_singleton()->get_mix_rate();

				if (diff_msec < 300) {
					uint32_t count = audio_stream->get_tagged_frame_count();

					bool differ = false;

					if (count != tagged_frame_offset_count) {
						differ = true;
					}
					float offsets[MAX_TAGGED_FRAMES];

					for (uint32_t i = 0; i < MIN(count, uint32_t(MAX_TAGGED_FRAMES)); i++) {
						offsets[i] = audio_stream->get_tagged_frame_offset(i);
						if (offsets[i] != tagged_frame_offsets[i]) {
							differ = true;
						}
					}

					if (differ) {
						tagged_frame_offset_count = count;
						for (uint32_t i = 0; i < count; i++) {
							tagged_frame_offsets[i] = offsets[i];
						}
					}

					stream_preview_rect->queue_redraw();
				} else {
					if (tagged_frame_offset_count != 0) {
						stream_preview_rect->queue_redraw();
					}
					tagged_frame_offset_count = 0;
				}
			}
		} break;
	}
}

void EditorAudioStreamPicker::_update_resource() {
	EditorResourcePicker::_update_resource();

	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
	Ref<AudioStream> audio_stream = get_edited_resource();
	if (audio_stream.is_valid() && audio_stream->get_length() > 0.0) {
		set_assign_button_min_size(Size2(1, font->get_height(font_size) * 3));
	} else {
		set_assign_button_min_size(Size2(1, font->get_height(font_size) * 1.5));
	}

	stream_preview_rect->queue_redraw();
}

void EditorAudioStreamPicker::_preview_draw() {
	Ref<AudioStream> audio_stream = get_edited_resource();
	if (audio_stream.is_null()) {
		get_assign_button()->set_text(TTR("<empty>"));
		return;
	}

	int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));

	get_assign_button()->set_text("");

	Size2i size = stream_preview_rect->get_size();
	Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));

	Rect2 rect(Point2(), size);

	if (audio_stream->get_length() > 0 && size.width > 0) {
		rect.size.height *= 0.5;

		Ref<AudioStreamPreview> preview = AudioStreamPreviewGenerator::get_singleton()->generate_preview(audio_stream);
		float preview_len = preview->get_length();

		Vector<Vector2> points;
		points.resize(size.width * 2);

		for (int i = 0; i < size.width; i++) {
			float ofs = i * preview_len / size.width;
			float ofs_n = (i + 1) * preview_len / size.width;
			float max = preview->get_max(ofs, ofs_n) * 0.5 + 0.5;
			float min = preview->get_min(ofs, ofs_n) * 0.5 + 0.5;

			int idx = i;
			points.write[idx * 2 + 0] = Vector2(i + 1, rect.position.y + min * rect.size.y);
			points.write[idx * 2 + 1] = Vector2(i + 1, rect.position.y + max * rect.size.y);
		}

		Vector<Color> colors = { get_theme_color(SNAME("contrast_color_2"), EditorStringName(Editor)) };

		RS::get_singleton()->canvas_item_add_multiline(stream_preview_rect->get_canvas_item(), points, colors);

		if (tagged_frame_offset_count) {
			Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));

			for (uint32_t i = 0; i < tagged_frame_offset_count; i++) {
				int x = CLAMP(tagged_frame_offsets[i] * size.width / preview_len, 0, size.width);
				if (x == 0) {
					continue; // Because some may always return 0, ignore offset 0.
				}
				stream_preview_rect->draw_rect(Rect2i(x, 0, 2, rect.size.height), accent);
			}
		}
		rect.position.y += rect.size.height;
	}

	Ref<Texture2D> icon;
	Color icon_modulate(1, 1, 1, 1);

	if (tagged_frame_offset_count > 0) {
		icon = get_editor_theme_icon(SNAME("Play"));
		if ((OS::get_singleton()->get_ticks_msec() % 500) > 250) {
			icon_modulate = Color(1, 0.5, 0.5, 1); // get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		}
	} else {
		icon = EditorNode::get_singleton()->get_object_icon(audio_stream.operator->(), "Object");
	}
	String text;
	if (!audio_stream->get_name().is_empty()) {
		text = audio_stream->get_name();
	} else if (audio_stream->get_path().is_resource_file()) {
		text = audio_stream->get_path().get_file();
	} else {
		text = audio_stream->get_class().replace_first("AudioStream", "");
	}

	stream_preview_rect->draw_texture(icon, Point2i(EDSCALE * 4, rect.position.y + (rect.size.height - icon->get_height()) / 2), icon_modulate);
	stream_preview_rect->draw_string(font, Point2i(EDSCALE * 4 + icon->get_width(), rect.position.y + font->get_ascent(font_size) + (rect.size.height - font->get_height(font_size)) / 2), text, HORIZONTAL_ALIGNMENT_CENTER, size.width - 4 * EDSCALE - icon->get_width(), font_size, get_theme_color(SceneStringName(font_color), EditorStringName(Editor)));
}

EditorAudioStreamPicker::EditorAudioStreamPicker() :
		EditorResourcePicker(true) {
	stream_preview_rect = memnew(Control);

	stream_preview_rect->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	stream_preview_rect->set_offset(SIDE_TOP, 1);
	stream_preview_rect->set_offset(SIDE_BOTTOM, -1);
	stream_preview_rect->set_offset(SIDE_RIGHT, -1);
	stream_preview_rect->set_mouse_filter(MOUSE_FILTER_IGNORE);
	stream_preview_rect->connect(SceneStringName(draw), callable_mp(this, &EditorAudioStreamPicker::_preview_draw));

	get_assign_button()->add_child(stream_preview_rect);
	get_assign_button()->move_child(stream_preview_rect, 0);
	set_process_internal(true);
}
