/**************************************************************************/
/*  editor_sectioned_inspector.cpp                                        */
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

#include "editor_sectioned_inspector.h"

#include "editor/editor_inspector.h"
#include "editor/editor_property_name_processor.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_button.h"
#include "scene/gui/tree.h"

static bool _property_path_matches(const String &p_property_path, const String &p_filter, EditorPropertyNameProcessor::Style p_style) {
	if (p_property_path.containsn(p_filter)) {
		return true;
	}

	const Vector<String> sections = p_property_path.split("/");
	for (int i = 0; i < sections.size(); i++) {
		if (p_filter.is_subsequence_ofn(EditorPropertyNameProcessor::get_singleton()->process_name(sections[i], p_style, p_property_path))) {
			return true;
		}
	}
	return false;
}

class SectionedInspectorFilter : public Object {
	GDCLASS(SectionedInspectorFilter, Object);

	Object *edited = nullptr;
	String section;
	bool allow_sub = false;

	bool _set(const StringName &p_name, const Variant &p_value) {
		if (!edited) {
			return false;
		}

		String name = p_name;
		if (!section.is_empty()) {
			name = section + "/" + name;
		}

		bool valid;
		edited->set(name, p_value, &valid);
		return valid;
	}

	bool _get(const StringName &p_name, Variant &r_ret) const {
		if (!edited) {
			return false;
		}

		String name = p_name;
		if (!section.is_empty()) {
			name = section + "/" + name;
		}

		bool valid = false;

		r_ret = edited->get(name, &valid);
		return valid;
	}
	void _get_property_list(List<PropertyInfo> *p_list) const {
		if (!edited) {
			return;
		}

		List<PropertyInfo> pinfo;
		edited->get_property_list(&pinfo);
		for (PropertyInfo &pi : pinfo) {
			int sp = pi.name.find_char('/');

			if (pi.name == "resource_path" || pi.name == "resource_name" || pi.name == "resource_local_to_scene" || pi.name.begins_with("script/") || pi.name.begins_with("_global_script")) { //skip resource stuff
				continue;
			}

			if (sp == -1) {
				pi.name = "global/" + pi.name;
			}

			if (pi.name.begins_with(section + "/")) {
				pi.name = pi.name.replace_first(section + "/", "");
				if (!allow_sub && pi.name.contains_char('/')) {
					continue;
				}
				p_list->push_back(pi);
			}
		}
	}

	bool _property_can_revert(const StringName &p_name) const {
		return edited->property_can_revert(section + "/" + p_name);
	}

	bool _property_get_revert(const StringName &p_name, Variant &r_property) const {
		r_property = edited->property_get_revert(section + "/" + p_name);
		return true;
	}

public:
	void set_section(const String &p_section, bool p_allow_sub) {
		section = p_section;
		allow_sub = p_allow_sub;
		notify_property_list_changed();
	}

	void set_edited(Object *p_edited) {
		edited = p_edited;
		notify_property_list_changed();
	}
};

void SectionedInspector::_bind_methods() {
	ClassDB::bind_method("update_category_list", &SectionedInspector::update_category_list);
}

void SectionedInspector::_section_selected() {
	if (!sections->get_selected()) {
		return;
	}

	selected_category = sections->get_selected()->get_metadata(0);
	filter->set_section(selected_category, sections->get_selected()->get_first_child() == nullptr);
	inspector->set_property_prefix(selected_category + "/");
}

void SectionedInspector::set_current_section(const String &p_section) {
	if (section_map.has(p_section)) {
		TreeItem *item = section_map[p_section];
		item->select(0);
		sections->scroll_to_item(item);
	}
}

String SectionedInspector::get_current_section() const {
	if (sections->get_selected()) {
		return sections->get_selected()->get_metadata(0);
	} else {
		return "";
	}
}

String SectionedInspector::get_full_item_path(const String &p_item) {
	String base = get_current_section();

	if (!base.is_empty()) {
		return base + "/" + p_item;
	} else {
		return p_item;
	}
}

void SectionedInspector::edit(Object *p_object) {
	if (!p_object) {
		obj = ObjectID();
		sections->clear();

		filter->set_edited(nullptr);
		inspector->edit(nullptr);

		return;
	}

	ObjectID id = p_object->get_instance_id();

	inspector->set_object_class(p_object->get_class());

	if (obj != id) {
		obj = id;
		update_category_list();

		filter->set_edited(p_object);
		inspector->edit(filter);

		TreeItem *first_item = sections->get_root();
		if (first_item) {
			while (first_item->get_first_child()) {
				first_item = first_item->get_first_child();
			}

			first_item->select(0);
			selected_category = first_item->get_metadata(0);
		}
	} else {
		update_category_list();
	}
}

void SectionedInspector::update_category_list() {
	sections->clear();

	Object *o = ObjectDB::get_instance(obj);

	if (!o) {
		return;
	}

	List<PropertyInfo> pinfo;
	o->get_property_list(&pinfo);

	section_map.clear();

	TreeItem *root = sections->create_item();
	section_map[""] = root;

	String filter_text;
	if (search_box) {
		filter_text = search_box->get_text();
	}

	const EditorPropertyNameProcessor::Style name_style = EditorPropertyNameProcessor::get_settings_style();
	const EditorPropertyNameProcessor::Style tooltip_style = EditorPropertyNameProcessor::get_tooltip_style(name_style);

	for (PropertyInfo &pi : pinfo) {
		if (pi.usage & PROPERTY_USAGE_CATEGORY) {
			continue;
		} else if (!(pi.usage & PROPERTY_USAGE_EDITOR) ||
				(filter_text.is_empty() && restrict_to_basic && !(pi.usage & PROPERTY_USAGE_EDITOR_BASIC_SETTING))) {
			continue;
		}

		if (pi.name.contains_char(':') || pi.name == "script" || pi.name == "resource_name" || pi.name == "resource_path" || pi.name == "resource_local_to_scene" || pi.name.begins_with("_global_script")) {
			continue;
		}

		if (!filter_text.is_empty() && !_property_path_matches(pi.name, filter_text, name_style)) {
			continue;
		}

		int sp = pi.name.find_char('/');
		if (sp == -1) {
			pi.name = "global/" + pi.name;
		}

		Vector<String> sectionarr = pi.name.split("/");
		String metasection;

		int sc = MIN(2, sectionarr.size() - 1);

		for (int i = 0; i < sc; i++) {
			TreeItem *parent = section_map[metasection];
			//parent->set_custom_bg_color(0, get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor)));
			parent->set_custom_font(0, get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));

			if (i > 0) {
				metasection += "/" + sectionarr[i];
			} else {
				metasection = sectionarr[i];
			}

			if (!section_map.has(metasection)) {
				TreeItem *ms = sections->create_item(parent);
				section_map[metasection] = ms;

				const String text = EditorPropertyNameProcessor::get_singleton()->process_name(sectionarr[i], name_style, pi.name);
				const String tooltip = EditorPropertyNameProcessor::get_singleton()->process_name(sectionarr[i], tooltip_style, pi.name);

				ms->set_text(0, text);
				ms->set_tooltip_text(0, tooltip);
				ms->set_metadata(0, metasection);
				ms->set_selectable(0, false);
			}

			if (i == sc - 1) {
				//if it has children, make selectable
				section_map[metasection]->set_selectable(0, true);
			}
		}
	}

	if (section_map.has(selected_category)) {
		section_map[selected_category]->select(0);
	}

	inspector->update_tree();
}

void SectionedInspector::register_search_box(LineEdit *p_box) {
	search_box = p_box;
	inspector->register_text_enter(p_box);
	search_box->connect(SceneStringName(text_changed), callable_mp(this, &SectionedInspector::_search_changed));
}

void SectionedInspector::register_advanced_toggle(CheckButton *p_toggle) {
	advanced_toggle = p_toggle;
	advanced_toggle->connect(SceneStringName(toggled), callable_mp(this, &SectionedInspector::_advanced_toggled));
	_advanced_toggled(advanced_toggle->is_pressed());
}

void SectionedInspector::_search_changed(const String &p_what) {
	if (advanced_toggle) {
		if (p_what.is_empty()) {
			advanced_toggle->set_pressed_no_signal(!restrict_to_basic);
			advanced_toggle->set_disabled(false);
			advanced_toggle->set_tooltip_text(String());
		} else {
			advanced_toggle->set_pressed_no_signal(true);
			advanced_toggle->set_disabled(true);
			advanced_toggle->set_tooltip_text(TTR("Advanced settings are always shown when searching."));
		}
	}
	update_category_list();
}

void SectionedInspector::_advanced_toggled(bool p_toggled_on) {
	restrict_to_basic = !p_toggled_on;
	update_category_list();
	inspector->set_restrict_to_basic_settings(restrict_to_basic);
}

EditorInspector *SectionedInspector::get_inspector() {
	return inspector;
}

SectionedInspector::SectionedInspector() :
		sections(memnew(Tree)),
		filter(memnew(SectionedInspectorFilter)),
		inspector(memnew(EditorInspector)) {
	add_theme_constant_override("autohide", 1); // Fixes the dragger always showing up

	VBoxContainer *left_vb = memnew(VBoxContainer);
	left_vb->set_custom_minimum_size(Size2(190, 0) * EDSCALE);
	add_child(left_vb);

	sections->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	sections->set_v_size_flags(SIZE_EXPAND_FILL);
	sections->set_hide_root(true);
	sections->set_theme_type_variation("TreeSecondary");

	left_vb->add_child(sections, true);

	VBoxContainer *right_vb = memnew(VBoxContainer);
	right_vb->set_custom_minimum_size(Size2(300, 0) * EDSCALE);
	right_vb->set_h_size_flags(SIZE_EXPAND_FILL);
	add_child(right_vb);

	inspector->set_v_size_flags(SIZE_EXPAND_FILL);
	right_vb->add_child(inspector, true);
	inspector->set_use_doc_hints(true);

	sections->connect("cell_selected", callable_mp(this, &SectionedInspector::_section_selected));
}

SectionedInspector::~SectionedInspector() {
	memdelete(filter);
}
