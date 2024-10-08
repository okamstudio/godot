/**************************************************************************/
/*  editor_about.cpp                                                      */
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

#include "editor_about.h"

#include "core/authors.gen.h"
#include "core/donors.gen.h"
#include "core/license.gen.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_version_button.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/item_list.h"
#include "scene/resources/style_box.h"

void EditorAbout::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			const Ref<Font> font = get_theme_font(SNAME("source"), EditorStringName(EditorFonts));
			const int font_size = get_theme_font_size(SNAME("source_size"), EditorStringName(EditorFonts));

			_tpl_text->begin_bulk_theme_override();
			_tpl_text->add_theme_font_override("normal_font", font);
			_tpl_text->add_theme_font_size_override("normal_font_size", font_size);
			_tpl_text->add_theme_constant_override(SceneStringName(line_separation), 4 * EDSCALE);
			_tpl_text->end_bulk_theme_override();

			license_text_label->begin_bulk_theme_override();
			license_text_label->add_theme_font_override("normal_font", font);
			license_text_label->add_theme_font_size_override("normal_font_size", font_size);
			license_text_label->add_theme_constant_override(SceneStringName(line_separation), 4 * EDSCALE);
			license_text_label->end_bulk_theme_override();

			_logo->set_texture(get_editor_theme_icon(SNAME("Logo")));

			for (ItemList *il : name_lists) {
				for (int i = 0; i < il->get_item_count(); i++) {
					if (il->get_item_metadata(i)) {
						il->set_item_icon(i, get_theme_icon(SNAME("ExternalLink"), EditorStringName(EditorIcons)));
						il->set_item_icon_modulate(i, get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));
					}
				}
			}
		} break;
	}
}

void EditorAbout::_license_tree_selected() {
	TreeItem *selected = _tpl_tree->get_selected();
	_tpl_text->scroll_to_line(0);
	_tpl_text->set_text(selected->get_metadata(0));
}

void EditorAbout::_item_with_website_selected(int p_id, ItemList *p_il) {
	const String website = p_il->get_item_metadata(p_id);
	if (!website.is_empty()) {
		OS::get_singleton()->shell_open(website);
	}
}

void EditorAbout::_item_list_resized(ItemList *p_il) {
	p_il->set_fixed_column_width(p_il->get_size().x / 3.0 - 16 * EDSCALE * 2.5); // Weird. Should be 3.0 and that's it?.
}

ScrollContainer *EditorAbout::_populate_list(const String &p_name, const List<String> &p_sections, const char *const *const p_src[], const int p_single_column_flags, const bool p_allow_website) {
	ScrollContainer *sc = memnew(ScrollContainer);
	sc->set_name(p_name);
	sc->set_v_size_flags(Control::SIZE_EXPAND);

	VBoxContainer *vbc = memnew(VBoxContainer);
	vbc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	sc->add_child(vbc);

	Ref<StyleBoxEmpty> empty_stylebox = memnew(StyleBoxEmpty);

	int i = 0;
	for (List<String>::ConstIterator itr = p_sections.begin(); itr != p_sections.end(); ++itr, ++i) {
		bool single_column = p_single_column_flags & (1 << i);
		const char *const *names_ptr = p_src[i];
		if (*names_ptr) {
			Label *lbl = memnew(Label);
			lbl->set_theme_type_variation("HeaderSmall");
			lbl->set_text(*itr);
			vbc->add_child(lbl);

			ItemList *il = memnew(ItemList);
			il->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
			il->set_h_size_flags(Control::SIZE_EXPAND_FILL);
			il->set_same_column_width(true);
			il->set_auto_height(true);
			il->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
			il->set_focus_mode(Control::FOCUS_NONE);
			il->add_theme_constant_override("h_separation", 16 * EDSCALE);
			if (p_allow_website) {
				il->set_focus_mode(Control::FOCUS_CLICK);
				il->set_mouse_filter(Control::MOUSE_FILTER_PASS);

				il->connect("item_activated", callable_mp(this, &EditorAbout::_item_with_website_selected).bind(il));
				il->connect(SceneStringName(resized), callable_mp(this, &EditorAbout::_item_list_resized).bind(il));
				il->connect(SceneStringName(focus_exited), callable_mp(il, &ItemList::deselect_all));

				il->add_theme_style_override("focus", empty_stylebox);
				il->add_theme_style_override("selected", empty_stylebox);

				while (*names_ptr) {
					const String name = String::utf8(*names_ptr++);
					const String identifier = name.get_slice("<", 0);
					const String website = name.get_slice_count("<") == 1 ? "" : name.get_slice("<", 1).trim_suffix(">");

					const int name_item_id = il->add_item(identifier, nullptr, false);
					il->set_item_tooltip_enabled(name_item_id, false);

					if (!website.is_empty()) {
						il->set_item_selectable(name_item_id, true);
						il->set_item_metadata(name_item_id, website);
						il->set_item_tooltip(name_item_id, website + "\n\n" + TTR("Double-click to open in browser."));
						il->set_item_tooltip_enabled(name_item_id, true);
					}

					if (!*names_ptr && name.contains(" anonymous ")) {
						il->set_item_disabled(name_item_id, true);
					}
				}
			} else {
				while (*names_ptr) {
					il->add_item(String::utf8(*names_ptr++), nullptr, false);
				}
			}
			il->set_max_columns(single_column ? 1 : 16);

			name_lists.append(il);

			vbc->add_child(il);

			HSeparator *hs = memnew(HSeparator);
			hs->set_modulate(Color(0, 0, 0, 0));
			vbc->add_child(hs);
		}
	}

	return sc;
}

EditorAbout::EditorAbout() {
	set_title(TTR("Thanks from the Godot community!"));
	set_hide_on_ok(true);

	VBoxContainer *vbc = memnew(VBoxContainer);
	add_child(vbc);

	HBoxContainer *hbc = memnew(HBoxContainer);
	hbc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	hbc->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	hbc->add_theme_constant_override("separation", 30 * EDSCALE);
	vbc->add_child(hbc);

	_logo = memnew(TextureRect);
	_logo->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	hbc->add_child(_logo);

	VBoxContainer *version_info_vbc = memnew(VBoxContainer);

	// Add a dummy control node for spacing.
	Control *v_spacer = memnew(Control);
	version_info_vbc->add_child(v_spacer);

	version_info_vbc->add_child(memnew(EditorVersionButton(EditorVersionButton::FORMAT_WITH_NAME_AND_BUILD)));

	Label *about_text = memnew(Label);
	about_text->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	about_text->set_text(
			String::utf8("\xc2\xa9 2014-present ") + TTR("Godot Engine contributors") + "." +
			String::utf8("\n\xc2\xa9 2007-2014 Juan Linietsky, Ariel Manzur.\n"));
	version_info_vbc->add_child(about_text);

	hbc->add_child(version_info_vbc);

	TabContainer *tc = memnew(TabContainer);
	tc->set_tab_alignment(TabBar::ALIGNMENT_CENTER);
	tc->set_custom_minimum_size(Size2(400, 200) * EDSCALE);
	tc->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tc->set_theme_type_variation("TabContainerOdd");
	vbc->add_child(tc);

	// Authors.

	List<String> dev_sections;
	dev_sections.push_back(TTR("Project Founders"));
	dev_sections.push_back(TTR("Lead Developer"));
	// TRANSLATORS: This refers to a job title.
	dev_sections.push_back(TTR("Project Manager", "Job Title"));
	dev_sections.push_back(TTR("Developers"));
	const char *const *dev_src[] = {
		AUTHORS_FOUNDERS,
		AUTHORS_LEAD_DEVELOPERS,
		AUTHORS_PROJECT_MANAGERS,
		AUTHORS_DEVELOPERS,
	};
	tc->add_child(_populate_list(TTR("Authors"), dev_sections, dev_src, 0b1)); // First section (Project Founders) is always one column.

	// Donors.

	List<String> donor_sections;
	donor_sections.push_back(TTR("Patrons"));
	donor_sections.push_back(TTR("Platinum Sponsors"));
	donor_sections.push_back(TTR("Gold Sponsors"));
	donor_sections.push_back(TTR("Silver Sponsors"));
	donor_sections.push_back(TTR("Diamond Members"));
	donor_sections.push_back(TTR("Titanium Members"));
	donor_sections.push_back(TTR("Platinum Members"));
	donor_sections.push_back(TTR("Gold Members"));
	const char *const *donor_src[] = {
		DONORS_PATRONS,
		DONORS_SPONSORS_PLATINUM,
		DONORS_SPONSORS_GOLD,
		DONORS_SPONSORS_SILVER,
		DONORS_MEMBERS_DIAMOND,
		DONORS_MEMBERS_TITANIUM,
		DONORS_MEMBERS_PLATINUM,
		DONORS_MEMBERS_GOLD,
	};
	tc->add_child(_populate_list(TTR("Donors"), donor_sections, donor_src, 0b1, true)); // First section (Patron) is one column.

	// License.

	license_text_label = memnew(RichTextLabel);
	license_text_label->set_threaded(true);
	license_text_label->set_name(TTR("License"));
	license_text_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	license_text_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	license_text_label->set_text(String::utf8(GODOT_LICENSE_TEXT));
	tc->add_child(license_text_label);

	// Thirdparty License.

	VBoxContainer *license_thirdparty = memnew(VBoxContainer);
	license_thirdparty->set_name(TTR("Third-party Licenses"));
	license_thirdparty->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tc->add_child(license_thirdparty);

	Label *tpl_label = memnew(Label);
	tpl_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tpl_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	tpl_label->set_text(TTR("Godot Engine relies on a number of third-party free and open source libraries, all compatible with the terms of its MIT license. The following is an exhaustive list of all such third-party components with their respective copyright statements and license terms."));
	tpl_label->set_size(Size2(630, 1) * EDSCALE);
	license_thirdparty->add_child(tpl_label);

	HSplitContainer *tpl_hbc = memnew(HSplitContainer);
	tpl_hbc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tpl_hbc->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tpl_hbc->set_split_offset(240 * EDSCALE);
	license_thirdparty->add_child(tpl_hbc);

	_tpl_tree = memnew(Tree);
	_tpl_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	_tpl_tree->set_hide_root(true);
	TreeItem *root = _tpl_tree->create_item();
	TreeItem *tpl_ti_all = _tpl_tree->create_item(root);
	tpl_ti_all->set_text(0, TTR("All Components"));
	TreeItem *tpl_ti_tp = _tpl_tree->create_item(root);
	tpl_ti_tp->set_text(0, TTR("Components"));
	tpl_ti_tp->set_selectable(0, false);
	TreeItem *tpl_ti_lc = _tpl_tree->create_item(root);
	tpl_ti_lc->set_text(0, TTR("Licenses"));
	tpl_ti_lc->set_selectable(0, false);
	String long_text = "";
	for (int component_index = 0; component_index < COPYRIGHT_INFO_COUNT; component_index++) {
		const ComponentCopyright &component = COPYRIGHT_INFO[component_index];
		TreeItem *ti = _tpl_tree->create_item(tpl_ti_tp);
		String component_name = String::utf8(component.name);
		ti->set_text(0, component_name);
		String text = component_name + "\n";
		long_text += "- " + component_name + "\n";
		for (int part_index = 0; part_index < component.part_count; part_index++) {
			const ComponentCopyrightPart &part = component.parts[part_index];
			text += "\n    Files:";
			for (int file_num = 0; file_num < part.file_count; file_num++) {
				text += "\n        " + String::utf8(part.files[file_num]);
			}
			String copyright;
			for (int copyright_index = 0; copyright_index < part.copyright_count; copyright_index++) {
				copyright += String::utf8("\n    \xc2\xa9 ") + String::utf8(part.copyright_statements[copyright_index]);
			}
			text += copyright;
			long_text += copyright;
			String license = "\n    License: " + String::utf8(part.license) + "\n";
			text += license;
			long_text += license + "\n";
		}
		ti->set_metadata(0, text);
	}
	for (int i = 0; i < LICENSE_COUNT; i++) {
		TreeItem *ti = _tpl_tree->create_item(tpl_ti_lc);
		String licensename = String::utf8(LICENSE_NAMES[i]);
		ti->set_text(0, licensename);
		long_text += "- " + licensename + "\n\n";
		String licensebody = String::utf8(LICENSE_BODIES[i]);
		ti->set_metadata(0, licensebody);
		long_text += "    " + licensebody.replace("\n", "\n    ") + "\n\n";
	}
	tpl_ti_all->set_metadata(0, long_text);
	tpl_hbc->add_child(_tpl_tree);

	_tpl_text = memnew(RichTextLabel);
	_tpl_text->set_threaded(true);
	_tpl_text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_tpl_text->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tpl_hbc->add_child(_tpl_text);

	_tpl_tree->connect(SceneStringName(item_selected), callable_mp(this, &EditorAbout::_license_tree_selected));
	tpl_ti_all->select(0);
	_tpl_text->set_text(tpl_ti_all->get_metadata(0));
}

EditorAbout::~EditorAbout() {}
