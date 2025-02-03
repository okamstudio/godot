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

#include "editor/editor_resource_preview.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "editor_settings.h"
#include "filesystem_dock.h"
#include "scene/main/viewport.h"
#include "scene/resources/dynamic_font.h"

HashMap<StringName, List<StringName>> EditorResourcePicker::allowed_types_cache;

void EditorResourcePicker::clear_caches() {
	allowed_types_cache.clear();
}

void EditorResourcePicker::_update_resource() {
	preview_rect->set_texture(Ref<Texture>());
	assign_button->set_custom_minimum_size(Size2(1, 1));

	if (edited_resource == RES()) {
		assign_button->set_icon(Ref<Texture>());
		assign_button->set_text(TTR("[empty]"));
		assign_button->set_tooltip("");
	} else {
		assign_button->set_icon(EditorNode::get_singleton()->get_object_icon(edited_resource.operator->(), "Object"));

		if (edited_resource->get_name() != String()) {
			assign_button->set_text(edited_resource->get_name());
		} else if (edited_resource->get_path().is_resource_file()) {
			assign_button->set_text(edited_resource->get_path().get_file());
		} else {
			assign_button->set_text(edited_resource->get_class());
		}

		String resource_path;
		if (edited_resource->get_path().is_resource_file()) {
			resource_path = edited_resource->get_path() + "\n";
		}
		assign_button->set_tooltip(resource_path + TTR("Type:") + " " + edited_resource->get_class());

		// Preview will override the above, so called at the end.
		EditorResourcePreview::get_singleton()->queue_edited_resource_preview(edited_resource, this, "_update_resource_preview", edited_resource->get_instance_id());
	}
}

void EditorResourcePicker::_update_resource_preview(const String &p_path, const Ref<Texture> &p_preview, const Ref<Texture> &p_small_preview, ObjectID p_obj) {
	if (!edited_resource.is_valid() || edited_resource->get_instance_id() != p_obj) {
		return;
	}

	String type = edited_resource->get_class_name();
	if (ClassDB::class_exists(type) && ClassDB::is_parent_class(type, "Script")) {
		assign_button->set_text(edited_resource->get_path().get_file());
		return;
	}

	if (p_preview.is_valid()) {
		preview_rect->set_margin(MARGIN_LEFT, assign_button->get_icon()->get_width() + assign_button->get_stylebox("normal")->get_default_margin(MARGIN_LEFT) + get_constant("hseparation", "Button"));

		if (type == "GradientTexture" || type == "Gradient") {
			preview_rect->set_stretch_mode(TextureRect::STRETCH_SCALE);
			assign_button->set_custom_minimum_size(Size2(1, 1));
		} else {
			preview_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
			int thumbnail_size = EditorSettings::get_singleton()->get("filesystem/file_dialog/thumbnail_size");
			thumbnail_size *= EDSCALE;
			assign_button->set_custom_minimum_size(Size2(1, thumbnail_size));
		}

		preview_rect->set_texture(p_preview);
		assign_button->set_text("");
	}
}

void EditorResourcePicker::_resource_selected() {
	if (edited_resource.is_null()) {
		edit_button->set_pressed(true);
		_update_menu();
		return;
	}

	emit_signal("resource_selected", edited_resource, false);
}

void EditorResourcePicker::_file_selected(const String &p_path) {
	RES loaded_resource = ResourceLoader::load(p_path);
	ERR_FAIL_COND_MSG(loaded_resource.is_null(), "Cannot load resource from path '" + p_path + "'.");

	if (base_type != "") {
		bool any_type_matches = false;

		for (int i = 0; i < base_type.get_slice_count(","); i++) {
			String base = base_type.get_slice(",", i);
			if (loaded_resource->is_class(base)) {
				any_type_matches = true;
				break;
			}
		}

		if (!any_type_matches) {
			EditorNode::get_singleton()->show_warning(vformat(TTR("The selected resource (%s) does not match any type expected for this property (%s)."), loaded_resource->get_class(), base_type));
			return;
		}
	}

	edited_resource = loaded_resource;
	emit_signal("resource_changed", edited_resource);
	_update_resource();
}

void EditorResourcePicker::_file_quick_selected() {
	_file_selected(quick_open->get_selected());
}

void EditorResourcePicker::_update_menu() {
	_update_menu_items();

	Rect2 gt = edit_button->get_global_rect();
	edit_menu->set_as_minsize();
	int ms = edit_menu->get_combined_minimum_size().width;
	Vector2 popup_pos = gt.position + gt.size - Vector2(ms, 0);
	edit_menu->set_global_position(popup_pos);
	edit_menu->popup();
}

void EditorResourcePicker::_update_menu_items() {
	edit_menu->clear();

	// Add options for creating specific subtypes of the base resource type.
	set_create_options(edit_menu);

	// Add an option to load a resource from a file using the QuickOpen dialog.
	edit_menu->add_icon_item(get_icon("Load", "EditorIcons"), TTR("Quick Load"), OBJ_MENU_QUICKLOAD);

	// Add an option to load a resource from a file using the regular file dialog.
	edit_menu->add_icon_item(get_icon("Load", "EditorIcons"), TTR("Load"), OBJ_MENU_LOAD);

	// Add options for changing existing value of the resource.
	if (edited_resource.is_valid()) {
		edit_menu->add_icon_item(get_icon("Edit", "EditorIcons"), TTR("Edit"), OBJ_MENU_EDIT);
		edit_menu->add_icon_item(get_icon("Clear", "EditorIcons"), TTR("Clear"), OBJ_MENU_CLEAR);
		edit_menu->add_icon_item(get_icon("Duplicate", "EditorIcons"), TTR("Make Unique"), OBJ_MENU_MAKE_UNIQUE);
		edit_menu->add_icon_item(get_icon("Save", "EditorIcons"), TTR("Save"), OBJ_MENU_SAVE);

		if (edited_resource->get_path().is_resource_file()) {
			edit_menu->add_separator();
			edit_menu->add_item(TTR("Show in FileSystem"), OBJ_MENU_SHOW_IN_FILE_SYSTEM);
		}
	}

	// Add options to copy/paste resource.
	RES cb = EditorSettings::get_singleton()->get_resource_clipboard();
	bool paste_valid = false;
	if (cb.is_valid()) {
		if (base_type == "") {
			paste_valid = true;
		} else {
			for (int i = 0; i < base_type.get_slice_count(","); i++) {
				if (ClassDB::class_exists(cb->get_class()) && ClassDB::is_parent_class(cb->get_class(), base_type.get_slice(",", i))) {
					paste_valid = true;
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
	if (edited_resource.is_valid()) {
		Vector<Ref<EditorResourceConversionPlugin>> conversions = EditorNode::get_singleton()->find_resource_conversion_plugin(edited_resource);
		if (conversions.size()) {
			edit_menu->add_separator();
		}
		for (int i = 0; i < conversions.size(); i++) {
			String what = conversions[i]->converts_to();
			Ref<Texture> icon;
			if (has_icon(what, "EditorIcons")) {
				icon = get_icon(what, "EditorIcons");
			} else {
				icon = get_icon(what, "Resource");
			}

			edit_menu->add_icon_item(icon, vformat(TTR("Convert to %s"), what), CONVERT_BASE_ID + i);
		}
	}
}

void EditorResourcePicker::_edit_menu_cbk(int p_which) {
	switch (p_which) {
		case OBJ_MENU_LOAD: {
			List<String> extensions;
			for (int i = 0; i < base_type.get_slice_count(","); i++) {
				String base = base_type.get_slice(",", i);
				ResourceLoader::get_recognized_extensions_for_type(base, &extensions);
			}

			Set<String> valid_extensions;
			for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
				valid_extensions.insert(E->get());
			}

			if (!file_dialog) {
				file_dialog = memnew(EditorFileDialog);
				file_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
				add_child(file_dialog);
				file_dialog->connect("file_selected", this, "_file_selected");
			}

			file_dialog->clear_filters();
			for (Set<String>::Element *E = valid_extensions.front(); E; E = E->next()) {
				file_dialog->add_filter("*." + E->get(), E->get().to_upper());
			}

			file_dialog->popup_centered_ratio();
		} break;

		case OBJ_MENU_QUICKLOAD: {
			if (!quick_open) {
				quick_open = memnew(EditorQuickOpen);
				add_child(quick_open);
				quick_open->connect("quick_open", this, "_file_quick_selected");
			}

			quick_open->popup_dialog(base_type);
			quick_open->set_title(TTR("Resource"));
		} break;

		case OBJ_MENU_EDIT: {
			if (edited_resource.is_valid()) {
				emit_signal("resource_selected", edited_resource, true);
			}
		} break;

		case OBJ_MENU_CLEAR: {
			edited_resource = RES();
			emit_signal("resource_changed", edited_resource);
			_update_resource();
		} break;

		case OBJ_MENU_MAKE_UNIQUE: {
			if (edited_resource.is_null()) {
				return;
			}

			List<PropertyInfo> property_list;
			edited_resource->get_property_list(&property_list);
			List<Pair<String, Variant>> propvalues;
			for (List<PropertyInfo>::Element *E = property_list.front(); E; E = E->next()) {
				Pair<String, Variant> p;
				PropertyInfo &pi = E->get();
				if (pi.usage & PROPERTY_USAGE_STORAGE) {
					p.first = pi.name;
					p.second = edited_resource->get(pi.name);
				}

				propvalues.push_back(p);
			}

			String orig_type = edited_resource->get_class();
			Object *inst = ClassDB::instance(orig_type);
			Ref<Resource> unique_resource = Ref<Resource>(Object::cast_to<Resource>(inst));
			ERR_FAIL_COND(unique_resource.is_null());

			for (List<Pair<String, Variant>>::Element *E = propvalues.front(); E; E = E->next()) {
				Pair<String, Variant> &p = E->get();
				unique_resource->set(p.first, p.second);
			}

			edited_resource = unique_resource;
			emit_signal("resource_changed", edited_resource);
			_update_resource();
		} break;

		case OBJ_MENU_SAVE: {
			if (edited_resource.is_null()) {
				return;
			}
			EditorNode::get_singleton()->save_resource(edited_resource);
		} break;

		case OBJ_MENU_COPY: {
			EditorSettings::get_singleton()->set_resource_clipboard(edited_resource);
		} break;

		case OBJ_MENU_PASTE: {
			edited_resource = EditorSettings::get_singleton()->get_resource_clipboard();
			emit_signal("resource_changed", edited_resource);
			_update_resource();
		} break;

		case OBJ_MENU_SHOW_IN_FILE_SYSTEM: {
			FileSystemDock *file_system_dock = EditorNode::get_singleton()->get_filesystem_dock();
			file_system_dock->navigate_to_path(edited_resource->get_path());

			// Ensure that the FileSystem dock is visible.
			TabContainer *tab_container = (TabContainer *)file_system_dock->get_parent_control();
			tab_container->set_current_tab(file_system_dock->get_index());
		} break;

		default: {
			// Allow subclasses to handle their own options first, only then fallback on the default branch logic.
			if (handle_menu_selected(p_which)) {
				break;
			}

			if (p_which >= CONVERT_BASE_ID) {
				int to_type = p_which - CONVERT_BASE_ID;
				Vector<Ref<EditorResourceConversionPlugin>> conversions = EditorNode::get_singleton()->find_resource_conversion_plugin(edited_resource);
				ERR_FAIL_INDEX(to_type, conversions.size());

				edited_resource = conversions[to_type]->convert(edited_resource);
				emit_signal("resource_changed", edited_resource);
				_update_resource();
				break;
			}

			ERR_FAIL_COND(inheritors_array.empty());

			String intype = inheritors_array[p_which - TYPE_BASE_ID];
			Variant obj;

			if (ScriptServer::is_global_class(intype)) {
				obj = ClassDB::instance(ScriptServer::get_global_class_native_base(intype));
				if (obj) {
					Ref<Script> script = ResourceLoader::load(ScriptServer::get_global_class_path(intype));
					if (script.is_valid()) {
						((Object *)obj)->set_script(script.get_ref_ptr());
					}
				}
			} else {
				obj = ClassDB::instance(intype);
			}

			if (!obj) {
				obj = EditorNode::get_editor_data().instance_custom_type(intype, "Resource");
			}

			Resource *resp = Object::cast_to<Resource>(obj);
			ERR_BREAK(!resp);

			edited_resource = RES(resp);
			emit_signal("resource_changed", edited_resource);
			_update_resource();
		} break;
	}
}

void EditorResourcePicker::set_create_options(Object *p_menu_node) {
	// If a subclass implements this method, use it to replace all create items.
	if (get_script_instance() && get_script_instance()->has_method("set_create_options")) {
		get_script_instance()->call("set_create_options", p_menu_node);
		return;
	}

	// By default provide generic "New ..." options.
	if (base_type != "") {
		int idx = 0;

		Set<String> allowed_types;
		_get_allowed_types(false, &allowed_types);

		Vector<EditorData::CustomType> custom_resources;
		if (EditorNode::get_editor_data().get_custom_types().has("Resource")) {
			custom_resources = EditorNode::get_editor_data().get_custom_types()["Resource"];
		}

		for (Set<String>::Element *E = allowed_types.front(); E; E = E->next()) {
			const String &t = E->get();

			bool is_custom_resource = false;
			Ref<Texture> icon;
			if (!custom_resources.empty()) {
				for (int j = 0; j < custom_resources.size(); j++) {
					if (custom_resources[j].name == t) {
						is_custom_resource = true;
						if (custom_resources[j].icon.is_valid()) {
							icon = custom_resources[j].icon;
						}
						break;
					}
				}
			}

			if (!is_custom_resource && !(ScriptServer::is_global_class(t) || ClassDB::can_instance(t))) {
				continue;
			}

			inheritors_array.push_back(t);

			if (!icon.is_valid()) {
				icon = get_icon(has_icon(t, "EditorIcons") ? t : "Object", "EditorIcons");
			}

			int id = TYPE_BASE_ID + idx;
			edit_menu->add_icon_item(icon, vformat(TTR("New %s"), t), id);

			idx++;
		}

		if (edit_menu->get_item_count()) {
			edit_menu->add_separator();
		}
	}
}

bool EditorResourcePicker::handle_menu_selected(int p_which) {
	if (get_script_instance() && get_script_instance()->has_method("handle_menu_selected")) {
		return get_script_instance()->call("handle_menu_selected", p_which);
	}

	return false;
}

void EditorResourcePicker::_button_draw() {
	if (dropping) {
		Color color = get_color("accent_color", "Editor");
		assign_button->draw_rect(Rect2(Point2(), assign_button->get_size()), color, false);
	}
}

void EditorResourcePicker::_button_input(const Ref<InputEvent> &p_event) {
	if (!editable) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		if (mb->is_pressed() && mb->get_button_index() == BUTTON_RIGHT) {
			_update_menu_items();

			Vector2 pos = get_global_position() + mb->get_position();
			edit_menu->set_as_minsize();
			edit_menu->set_global_position(pos);
			edit_menu->popup();
		}
	}
}

void EditorResourcePicker::_get_allowed_types(bool p_with_convert, Set<String> *p_vector) const {
	Vector<String> allowed_types = base_type.split(",");
	int size = allowed_types.size();

	List<StringName> global_classes;
	ScriptServer::get_global_class_list(&global_classes);

	for (int i = 0; i < size; i++) {
		String base = allowed_types[i].strip_edges();
		p_vector->insert(base);

		// If we hit a familiar base type, take all the data from cache.
		if (allowed_types_cache.has(base)) {
			List<StringName> allowed_subtypes = allowed_types_cache[base];
			for (List<StringName>::Element *E = allowed_subtypes.front(); E; E = E->next()) {
				p_vector->insert(E->get());
			}
		} else {
			List<StringName> allowed_subtypes;

			List<StringName> inheriters;
			ClassDB::get_inheriters_from_class(base, &inheriters);
			for (List<StringName>::Element *E = inheriters.front(); E; E = E->next()) {
				p_vector->insert(E->get());
				allowed_subtypes.push_back(E->get());
			}

			for (List<StringName>::Element *E = global_classes.front(); E; E = E->next()) {
				if (EditorNode::get_editor_data().script_class_is_parent(E->get(), base)) {
					p_vector->insert(E->get());
					allowed_subtypes.push_back(E->get());
				}
			}

			// Store the subtypes of the base type in the cache for future use.
			allowed_types_cache[base] = allowed_subtypes;
		}

		if (p_with_convert) {
			if (base == "SpatialMaterial" || base == "ORMSpatialMaterial") {
				p_vector->insert("Texture");
			} else if (base == "ShaderMaterial") {
				p_vector->insert("Shader");
			} else if (base == "Texture") {
				p_vector->insert("Image");
			}
		}
	}

	if (EditorNode::get_editor_data().get_custom_types().has("Resource")) {
		Vector<EditorData::CustomType> custom_resources = EditorNode::get_editor_data().get_custom_types()["Resource"];

		for (int i = 0; i < custom_resources.size(); i++) {
			p_vector->insert(custom_resources[i].name);
		}
	}
}

bool EditorResourcePicker::_is_drop_valid(const Dictionary &p_drag_data) const {
	if (base_type.empty()) {
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
	}

	Set<String> allowed_types;
	_get_allowed_types(true, &allowed_types);

	if (res.is_valid() && _is_type_valid(res->get_class(), allowed_types)) {
		return true;
	}

	if (res.is_valid() && !res->get_script().is_null()) {
		Ref<Script> res_script = res->get_script();
		StringName custom_class = EditorNode::get_singleton()->get_object_custom_type_name(res_script.ptr());
		if (_is_type_valid(custom_class, allowed_types)) {
			return true;
		}
	}

	if (drag_data.has("type") && String(drag_data["type"]) == "files") {
		Vector<String> files = drag_data["files"];

		if (files.size() == 1) {
			String file = files[0];

			String file_type = EditorFileSystem::get_singleton()->get_file_type(file);
			if (file_type != "" && _is_type_valid(file_type, allowed_types)) {
				return true;
			}
		}
	}

	return false;
}

bool EditorResourcePicker::_is_type_valid(const String p_type_name, Set<String> p_allowed_types) const {
	for (Set<String>::Element *E = p_allowed_types.front(); E; E = E->next()) {
		String at = E->get().strip_edges();
		if (p_type_name == at || (ClassDB::class_exists(p_type_name) && ClassDB::is_parent_class(p_type_name, at)) || EditorNode::get_editor_data().script_class_is_parent(p_type_name, at)) {
			return true;
		}
	}

	return false;
}

Variant EditorResourcePicker::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (edited_resource.is_valid()) {
		return EditorNode::get_singleton()->drag_resource(edited_resource, p_from);
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

	if (!dropped_resource.is_valid() && drag_data.has("type") && String(drag_data["type"]) == "files") {
		Vector<String> files = drag_data["files"];

		if (files.size() == 1) {
			String file = files[0];
			dropped_resource = ResourceLoader::load(file);
		}
	}

	if (dropped_resource.is_valid()) {
		Set<String> allowed_types;
		_get_allowed_types(false, &allowed_types);

		// If the accepted dropped resource is from the extended list, it requires conversion.
		if (!_is_type_valid(dropped_resource->get_class(), allowed_types)) {
			for (Set<String>::Element *E = allowed_types.front(); E; E = E->next()) {
				String at = E->get().strip_edges();

				if (at == "SpatialMaterial" && ClassDB::is_parent_class(dropped_resource->get_class(), "Texture")) {
					// Use existing resource if possible and only replace its data.
					Ref<SpatialMaterial> mat = edited_resource;
					if (mat.is_null()) {
						mat.instance();
					}
					mat->set_texture(Material3D::TextureParam::TEXTURE_ALBEDO, dropped_resource);
					dropped_resource = mat;
					break;
				}

				if (at == "ORMSpatialMaterial" && ClassDB::is_parent_class(dropped_resource->get_class(), "Texture")) {
					// Use existing resource if possible and only replace its data.
					Ref<ORMSpatialMaterial> mat = edited_resource;
					if (mat.is_null()) {
						mat.instance();
					}
					mat->set_texture(Material3D::TextureParam::TEXTURE_ALBEDO, dropped_resource);
					dropped_resource = mat;
					break;
				}

				if (at == "ShaderMaterial" && ClassDB::is_parent_class(dropped_resource->get_class(), "Shader")) {
					Ref<ShaderMaterial> mat = edited_resource;
					if (mat.is_null()) {
						mat.instance();
					}
					mat->set_shader(dropped_resource);
					dropped_resource = mat;
					break;
				}

				if (at == "Texture" && ClassDB::is_parent_class(dropped_resource->get_class(), "Image")) {
					Ref<ImageTexture> texture = edited_resource;
					if (texture.is_null()) {
						texture.instance();
					}
					texture->create_from_image(dropped_resource);
					dropped_resource = texture;
					break;
				}
			}
		}

		edited_resource = dropped_resource;
		emit_signal("resource_changed", edited_resource);
		_update_resource();
	}
}

void EditorResourcePicker::_bind_methods() {
	// Internal binds.
	ClassDB::bind_method(D_METHOD("_file_selected"), &EditorResourcePicker::_file_selected);
	ClassDB::bind_method(D_METHOD("_file_quick_selected"), &EditorResourcePicker::_file_quick_selected);
	ClassDB::bind_method(D_METHOD("_resource_selected"), &EditorResourcePicker::_resource_selected);
	ClassDB::bind_method(D_METHOD("_button_draw"), &EditorResourcePicker::_button_draw);
	ClassDB::bind_method(D_METHOD("_button_input"), &EditorResourcePicker::_button_input);
	ClassDB::bind_method(D_METHOD("_update_menu"), &EditorResourcePicker::_update_menu);
	ClassDB::bind_method(D_METHOD("_edit_menu_cbk"), &EditorResourcePicker::_edit_menu_cbk);

	// Public binds.
	ClassDB::bind_method(D_METHOD("_update_resource_preview"), &EditorResourcePicker::_update_resource_preview);
	ClassDB::bind_method(D_METHOD("get_drag_data_fw", "position", "from"), &EditorResourcePicker::get_drag_data_fw);
	ClassDB::bind_method(D_METHOD("can_drop_data_fw", "position", "data", "from"), &EditorResourcePicker::can_drop_data_fw);
	ClassDB::bind_method(D_METHOD("drop_data_fw", "position", "data", "from"), &EditorResourcePicker::drop_data_fw);

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

	ClassDB::add_virtual_method(get_class_static(), MethodInfo("set_create_options", PropertyInfo(Variant::OBJECT, "menu_node")));
	ClassDB::add_virtual_method(get_class_static(), MethodInfo(Variant::BOOL, "handle_menu_selected", PropertyInfo(Variant::INT, "id")));

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "base_type"), "set_base_type", "get_base_type");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "edited_resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource", 0), "set_edited_resource", "get_edited_resource");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "editable"), "set_editable", "is_editable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "toggle_mode"), "set_toggle_mode", "is_toggle_mode");

	ADD_SIGNAL(MethodInfo("resource_selected", PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource"), PropertyInfo(Variant::BOOL, "edit")));
	ADD_SIGNAL(MethodInfo("resource_changed", PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource")));
}

void EditorResourcePicker::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_resource();
			FALLTHROUGH;
		}
		case NOTIFICATION_THEME_CHANGED: {
			edit_button->set_icon(get_icon("select_arrow", "Tree"));
		} break;

		case NOTIFICATION_DRAW: {
			draw_style_box(get_stylebox("bg", "Tree"), Rect2(Point2(), get_size()));
		} break;

		case NOTIFICATION_DRAG_BEGIN: {
			if (editable && _is_drop_valid(get_viewport()->gui_get_drag_data())) {
				dropping = true;
				assign_button->update();
			}
		} break;

		case NOTIFICATION_DRAG_END: {
			if (dropping) {
				dropping = false;
				assign_button->update();
			}
		} break;
	}
}

void EditorResourcePicker::set_base_type(const String &p_base_type) {
	base_type = p_base_type;

	// There is a possibility that the new base type is conflicting with the existing value.
	// Keep the value, but warn the user that there is a potential mistake.
	if (!base_type.empty() && edited_resource.is_valid()) {
		Set<String> allowed_types;
		_get_allowed_types(true, &allowed_types);

		StringName custom_class;
		bool is_custom = false;
		if (!edited_resource->get_script().is_null()) {
			Ref<Script> res_script = edited_resource->get_script();
			custom_class = EditorNode::get_singleton()->get_object_custom_type_name(res_script.ptr());
			is_custom = _is_type_valid(custom_class, allowed_types);
		}

		if (!is_custom && !_is_type_valid(edited_resource->get_class(), allowed_types)) {
			String class_str = (custom_class == StringName() ? edited_resource->get_class() : vformat("%s (%s)", custom_class, edited_resource->get_class()));
			WARN_PRINT(vformat("Value mismatch between the new base type of this EditorResourcePicker, '%s', and the type of the value it already has, '%s'.", base_type, class_str));
		}
	} else {
		// Call the method to build the cache immediately.
		Set<String> allowed_types;
		_get_allowed_types(false, &allowed_types);
	}
}

String EditorResourcePicker::get_base_type() const {
	return base_type;
}

Vector<String> EditorResourcePicker::get_allowed_types() const {
	Set<String> allowed_types;
	_get_allowed_types(false, &allowed_types);

	Vector<String> types;
	types.resize(allowed_types.size());

	int i = 0;
	String *w = types.ptrw();
	for (Set<String>::Element *E = allowed_types.front(); E; E = E->next(), i++) {
		w[i] = E->get();
	}

	return types;
}

void EditorResourcePicker::set_edited_resource(RES p_resource) {
	if (!p_resource.is_valid()) {
		edited_resource = RES();
		_update_resource();
		return;
	}

	if (!base_type.empty()) {
		Set<String> allowed_types;
		_get_allowed_types(true, &allowed_types);

		StringName custom_class;
		bool is_custom = false;
		if (!p_resource->get_script().is_null()) {
			Ref<Script> res_script = p_resource->get_script();
			custom_class = EditorNode::get_singleton()->get_object_custom_type_name(res_script.ptr());
			is_custom = _is_type_valid(custom_class, allowed_types);
		}

		if (!is_custom && !_is_type_valid(p_resource->get_class(), allowed_types)) {
			String class_str = (custom_class == StringName() ? p_resource->get_class() : vformat("%s (%s)", custom_class, p_resource->get_class()));
			ERR_FAIL_MSG(vformat("Failed to set a resource of the type '%s' because this EditorResourcePicker only accepts '%s' and its derivatives.", class_str, base_type));
		}
	}

	edited_resource = p_resource;
	_update_resource();
}

RES EditorResourcePicker::get_edited_resource() {
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

void EditorResourcePicker::set_editable(bool p_editable) {
	editable = p_editable;
	assign_button->set_disabled(!editable);
	edit_button->set_visible(editable);
}

bool EditorResourcePicker::is_editable() const {
	return editable;
}

EditorResourcePicker::EditorResourcePicker() {
	assign_button = memnew(Button);
	assign_button->set_flat(true);
	assign_button->set_h_size_flags(SIZE_EXPAND_FILL);
	assign_button->set_clip_text(true);
	assign_button->set_drag_forwarding(this);
	add_child(assign_button);
	assign_button->connect("pressed", this, "_resource_selected");
	assign_button->connect("draw", this, "_button_draw");
	assign_button->connect("gui_input", this, "_button_input");

	preview_rect = memnew(TextureRect);
	preview_rect->set_expand(true);
	preview_rect->set_anchors_and_margins_preset(PRESET_WIDE);
	preview_rect->set_margin(MARGIN_TOP, 1);
	preview_rect->set_margin(MARGIN_BOTTOM, -1);
	preview_rect->set_margin(MARGIN_RIGHT, -1);
	assign_button->add_child(preview_rect);

	edit_button = memnew(Button);
	edit_button->set_flat(true);
	edit_button->set_toggle_mode(true);
	edit_button->connect("pressed", this, "_update_menu");
	add_child(edit_button);
	edit_button->connect("gui_input", this, "_button_input");
	edit_menu = memnew(PopupMenu);
	add_child(edit_menu);
	edit_menu->connect("id_pressed", this, "_edit_menu_cbk");
	edit_menu->connect("popup_hide", edit_button, "set_pressed", varray(false));

	add_constant_override("separation", 0);
}

void EditorScriptPicker::set_create_options(Object *p_menu_node) {
	PopupMenu *menu_node = Object::cast_to<PopupMenu>(p_menu_node);
	if (!menu_node) {
		return;
	}

	menu_node->add_icon_item(get_icon("ScriptCreate", "EditorIcons"), TTR("New Script"), OBJ_MENU_NEW_SCRIPT);
	if (script_owner) {
		Ref<Script> script = script_owner->get_script();
		if (script.is_valid()) {
			menu_node->add_icon_item(get_icon("ScriptExtend", "EditorIcons"), TTR("Extend Script"), OBJ_MENU_EXTEND_SCRIPT);
		}
	}
	menu_node->add_separator();
}

bool EditorScriptPicker::handle_menu_selected(int p_which) {
	switch (p_which) {
		case OBJ_MENU_NEW_SCRIPT: {
			if (script_owner) {
				EditorNode::get_singleton()->get_scene_tree_dock()->open_script_dialog(script_owner, false);
			}
			return true;
		}

		case OBJ_MENU_EXTEND_SCRIPT: {
			if (script_owner) {
				EditorNode::get_singleton()->get_scene_tree_dock()->open_script_dialog(script_owner, true);
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

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "script_owner", PROPERTY_HINT_RESOURCE_TYPE, "Node", 0), "set_script_owner", "get_script_owner");
}

EditorScriptPicker::EditorScriptPicker() {
}
