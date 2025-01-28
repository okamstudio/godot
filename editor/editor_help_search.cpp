/**************************************************************************/
/*  editor_help_search.cpp                                                */
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

#include "editor_help_search.h"

#include "editor/editor_feature_profile.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"

bool EditorHelpSearch::_all_terms_in_name(const Vector<String> &p_terms, const String &p_name) const {
	for (int i = 0; i < p_terms.size(); i++) {
		if (!p_name.containsn(p_terms[i])) {
			return false;
		}
	}
	return true;
}

void EditorHelpSearch::_match_method_name_and_push_back(const String &p_term, const Vector<String> &p_terms, Vector<DocData::MethodDoc> &p_methods, const String &p_type, const String &p_metatype, const String &p_class_name, Dictionary &r_result) const {
	// Constructors, Methods, Operators...
	for (int i = 0; i < p_methods.size(); i++) {
		String method_name = p_methods[i].name.to_lower();
		if (_all_terms_in_name(p_terms, method_name) ||
				(p_term.begins_with(".") && method_name.begins_with(p_term.substr(1))) ||
				(p_term.ends_with("(") && method_name.ends_with(p_term.left(p_term.length() - 1).strip_edges())) ||
				(p_term.begins_with(".") && p_term.ends_with("(") && method_name == p_term.substr(1, p_term.length() - 2).strip_edges())) {
			r_result[vformat("class_%s:%s:%s", p_metatype, p_class_name, p_methods[i].name)] = vformat("%s > %s: %s", p_class_name, p_type, p_methods[i].name);
		}
	}
}

void EditorHelpSearch::_match_const_name_and_push_back(const String &p_term, const Vector<String> &p_terms, Vector<DocData::ConstantDoc> &p_constants, const String &p_type, const String &p_metatype, const String &p_class_name, Dictionary &r_result) const {
	for (int i = 0; i < p_constants.size(); i++) {
		String method_name = p_constants[i].name.to_lower();
		if (_all_terms_in_name(p_terms, method_name) ||
				(p_term.begins_with(".") && method_name.begins_with(p_term.substr(1))) ||
				(p_term.ends_with("(") && method_name.ends_with(p_term.left(p_term.length() - 1).strip_edges())) ||
				(p_term.begins_with(".") && p_term.ends_with("(") && method_name == p_term.substr(1, p_term.length() - 2).strip_edges())) {
			r_result[vformat("class_%s:%s:%s", p_metatype, p_class_name, p_constants[i].name)] = vformat("%s > %s: %s", p_class_name, p_type, p_constants[i].name);
		}
	}
}

void EditorHelpSearch::_match_property_name_and_push_back(const String &p_term, const Vector<String> &p_terms, Vector<DocData::PropertyDoc> &p_properties, const String &p_type, const String &p_metatype, const String &p_class_name, Dictionary &r_result) const {
	for (int i = 0; i < p_properties.size(); i++) {
		String method_name = p_properties[i].name.to_lower();
		if (_all_terms_in_name(p_terms, method_name) ||
				(p_term.begins_with(".") && method_name.begins_with(p_term.substr(1))) ||
				(p_term.ends_with("(") && method_name.ends_with(p_term.left(p_term.length() - 1).strip_edges())) ||
				(p_term.begins_with(".") && p_term.ends_with("(") && method_name == p_term.substr(1, p_term.length() - 2).strip_edges())) {
			r_result[vformat("class_%s:%s:%s", p_metatype, p_class_name, p_properties[i].name)] = vformat("%s > %s: %s", p_class_name, p_type, p_properties[i].name);
		}
	}
}

void EditorHelpSearch::_match_theme_property_name_and_push_back(const String &p_term, const Vector<String> &p_terms, Vector<DocData::ThemeItemDoc> &p_properties, const String &p_type, const String &p_metatype, const String &p_class_name, Dictionary &r_result) const {
	for (int i = 0; i < p_properties.size(); i++) {
		String method_name = p_properties[i].name.to_lower();
		if (_all_terms_in_name(p_terms, method_name) ||
				(p_term.begins_with(".") && method_name.begins_with(p_term.substr(1))) ||
				(p_term.ends_with("(") && method_name.ends_with(p_term.left(p_term.length() - 1).strip_edges())) ||
				(p_term.begins_with(".") && p_term.ends_with("(") && method_name == p_term.substr(1, p_term.length() - 2).strip_edges())) {
			r_result[vformat("class_%s:%s:%s", p_metatype, p_class_name, p_properties[i].name)] = vformat("%s > %s: %s", p_class_name, p_type, p_properties[i].name);
		}
	}
}

Dictionary EditorHelpSearch::_native_search_cb(const String &p_search_string, int p_result_limit) {
	Dictionary ret;
	const String &term = p_search_string.strip_edges().to_lower();
	Vector<String> terms = term.split_spaces();
	if (terms.is_empty()) {
		terms.append(term);
	}

	for (HashMap<String, DocData::ClassDoc>::Iterator iterator_doc = EditorHelp::get_doc_data()->class_list.begin(); iterator_doc; ++iterator_doc) {
		DocData::ClassDoc &class_doc = iterator_doc->value;
		if (class_doc.name.is_empty()) {
			continue;
		}
		if (class_doc.name.containsn(term)) {
			ret[vformat("class_name:%s", class_doc.name)] = class_doc.name;
		}
		if (term.length() > 1 || term == "@") {
			_match_method_name_and_push_back(term, terms, class_doc.constructors, TTRC("Constructor"), "method", class_doc.name, ret);
			_match_method_name_and_push_back(term, terms, class_doc.methods, TTRC("Method"), "method", class_doc.name, ret);
			_match_method_name_and_push_back(term, terms, class_doc.operators, TTRC("Operator"), "method", class_doc.name, ret);
			_match_method_name_and_push_back(term, terms, class_doc.signals, TTRC("Signal"), "signal", class_doc.name, ret);
			_match_const_name_and_push_back(term, terms, class_doc.constants, TTRC("Constant"), "constant", class_doc.name, ret);
			_match_property_name_and_push_back(term, terms, class_doc.properties, TTRC("Property"), "property", class_doc.name, ret);
			_match_theme_property_name_and_push_back(term, terms, class_doc.theme_properties, TTRC("Theme Property"), "theme_item", class_doc.name, ret);
			_match_method_name_and_push_back(term, terms, class_doc.annotations, TTRC("Annotation"), "annotation", class_doc.name, ret);
		}
		if (ret.size() > p_result_limit) {
			break;
		}
	}
	return ret;
}

void EditorHelpSearch::_native_action_cb(const String &p_item_string) {
	emit_signal(SNAME("go_to_help"), p_item_string);
}

void EditorHelpSearch::_update_results() {
	const String term = search_box->get_text().strip_edges();

	int search_flags = filter_combo->get_selected_id();

	// Process separately if term is not short, or is "@" for annotations.
	if (term.length() > 1 || term == "@") {
		case_sensitive_button->set_disabled(false);
		hierarchy_button->set_disabled(false);

		if (case_sensitive_button->is_pressed()) {
			search_flags |= SEARCH_CASE_SENSITIVE;
		}
		if (hierarchy_button->is_pressed()) {
			search_flags |= SEARCH_SHOW_HIERARCHY;
		}

		search.instantiate(results_tree, results_tree, &tree_cache, term, search_flags);

		// Clear old search flags to force rebuild on short term.
		old_search_flags = 0;
		set_process(true);
	} else {
		// Disable hierarchy and case sensitive options, not used for short searches.
		case_sensitive_button->set_disabled(true);
		hierarchy_button->set_disabled(true);

		// Always show hierarchy for short searches.
		search.instantiate(results_tree, results_tree, &tree_cache, term, search_flags | SEARCH_SHOW_HIERARCHY);

		old_search_flags = search_flags;
		set_process(true);
	}
}

void EditorHelpSearch::_search_box_gui_input(const Ref<InputEvent> &p_event) {
	// Redirect navigational key events to the tree.
	Ref<InputEventKey> key = p_event;
	if (key.is_valid()) {
		if (key->is_action("ui_up", true) || key->is_action("ui_down", true) || key->is_action("ui_page_up") || key->is_action("ui_page_down")) {
			results_tree->gui_input(key);
			search_box->accept_event();
		}
	}
}

void EditorHelpSearch::_search_box_text_changed(const String &p_text) {
	_update_results();
}

void EditorHelpSearch::_filter_combo_item_selected(int p_option) {
	_update_results();
}

void EditorHelpSearch::_confirmed() {
	TreeItem *item = results_tree->get_selected();
	if (!item) {
		return;
	}

	// Activate the script editor and emit the signal with the documentation link to display.
	EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_SCRIPT);

	emit_signal(SNAME("go_to_help"), item->get_metadata(0));

	hide();
}

void EditorHelpSearch::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_NATIVE_HELP)) {
				DisplayServer::get_singleton()->help_set_search_callbacks(callable_mp(this, &EditorHelpSearch::_native_search_cb), callable_mp(this, &EditorHelpSearch::_native_action_cb));
			}
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_NATIVE_HELP)) {
				DisplayServer::get_singleton()->help_set_search_callbacks();
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				tree_cache.clear();
				results_tree->get_vscroll_bar()->set_value(0);
				search = Ref<Runner>();
				callable_mp(results_tree, &Tree::clear).call_deferred(); // Wait for the Tree's mouse event propagation.
				get_ok_button()->set_disabled(true);
				EditorSettings::get_singleton()->set_project_metadata("dialog_bounds", "search_help", Rect2(get_position(), get_size()));
			}
		} break;

		case NOTIFICATION_READY: {
			connect(SceneStringName(confirmed), callable_mp(this, &EditorHelpSearch::_confirmed));
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!EditorThemeManager::is_generated_theme_outdated()) {
				break;
			}
			[[fallthrough]];
		}
		case NOTIFICATION_THEME_CHANGED: {
			const int icon_width = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));
			results_tree->add_theme_constant_override("icon_max_width", icon_width);

			search_box->set_right_icon(get_editor_theme_icon(SNAME("Search")));
			search_box->add_theme_icon_override("right_icon", get_editor_theme_icon(SNAME("Search")));

			case_sensitive_button->set_button_icon(get_editor_theme_icon(SNAME("MatchCase")));
			hierarchy_button->set_button_icon(get_editor_theme_icon(SNAME("ClassList")));

			if (is_visible()) {
				_update_results();
			}
		} break;

		case NOTIFICATION_PROCESS: {
			// Update background search.
			if (search.is_valid()) {
				if (search->work()) {
					// Search done.

					// Only point to the match if it's a new search, and not just reopening a old one.
					if (!old_search) {
						results_tree->ensure_cursor_is_visible();
					} else {
						old_search = false;
					}

					get_ok_button()->set_disabled(!results_tree->get_selected());

					search = Ref<Runner>();
					set_process(false);
				}
			} else {
				set_process(false);
			}
		} break;
	}
}

void EditorHelpSearch::_bind_methods() {
	ADD_SIGNAL(MethodInfo("go_to_help"));
}

void EditorHelpSearch::popup_dialog() {
	popup_dialog(search_box->get_text());
}

void EditorHelpSearch::popup_dialog(const String &p_term) {
	// Restore valid window bounds or pop up at default size.
	Rect2 saved_size = EditorSettings::get_singleton()->get_project_metadata("dialog_bounds", "search_help", Rect2());
	if (saved_size != Rect2()) {
		popup(saved_size);
	} else {
		popup_centered_ratio(0.5F);
	}

	old_search_flags = 0;
	if (p_term.is_empty()) {
		search_box->clear();
	} else {
		if (old_term == p_term) {
			old_search = true;
		} else {
			old_term = p_term;
		}

		search_box->set_text(p_term);
		search_box->select_all();
	}
	search_box->grab_focus();
	_update_results();
}

EditorHelpSearch::EditorHelpSearch() {
	set_hide_on_ok(false);
	set_clamp_to_embedder(true);

	set_title(TTR("Search Help"));

	get_ok_button()->set_disabled(true);
	set_ok_button_text(TTR("Open"));

	// Split search and results area.
	VBoxContainer *vbox = memnew(VBoxContainer);
	add_child(vbox);

	// Create the search box and filter controls (at the top).
	HBoxContainer *hbox = memnew(HBoxContainer);
	vbox->add_child(hbox);

	search_box = memnew(LineEdit);
	search_box->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	search_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	search_box->set_clear_button_enabled(true);
	search_box->connect(SceneStringName(gui_input), callable_mp(this, &EditorHelpSearch::_search_box_gui_input));
	search_box->connect(SceneStringName(text_changed), callable_mp(this, &EditorHelpSearch::_search_box_text_changed));
	register_text_enter(search_box);
	hbox->add_child(search_box);

	case_sensitive_button = memnew(Button);
	case_sensitive_button->set_theme_type_variation(SceneStringName(FlatButton));
	case_sensitive_button->set_tooltip_text(TTR("Case Sensitive"));
	case_sensitive_button->connect(SceneStringName(pressed), callable_mp(this, &EditorHelpSearch::_update_results));
	case_sensitive_button->set_toggle_mode(true);
	case_sensitive_button->set_focus_mode(Control::FOCUS_NONE);
	hbox->add_child(case_sensitive_button);

	hierarchy_button = memnew(Button);
	hierarchy_button->set_theme_type_variation(SceneStringName(FlatButton));
	hierarchy_button->set_tooltip_text(TTR("Show Hierarchy"));
	hierarchy_button->connect(SceneStringName(pressed), callable_mp(this, &EditorHelpSearch::_update_results));
	hierarchy_button->set_toggle_mode(true);
	hierarchy_button->set_pressed(true);
	hierarchy_button->set_focus_mode(Control::FOCUS_NONE);
	hbox->add_child(hierarchy_button);

	filter_combo = memnew(OptionButton);
	filter_combo->set_custom_minimum_size(Size2(200, 0) * EDSCALE);
	filter_combo->set_stretch_ratio(0); // Fixed width.
	filter_combo->add_item(TTR("Display All"), SEARCH_ALL);
	filter_combo->add_separator();
	filter_combo->add_item(TTR("Classes Only"), SEARCH_CLASSES);
	filter_combo->add_item(TTR("Constructors Only"), SEARCH_CONSTRUCTORS);
	filter_combo->add_item(TTR("Methods Only"), SEARCH_METHODS);
	filter_combo->add_item(TTR("Operators Only"), SEARCH_OPERATORS);
	filter_combo->add_item(TTR("Signals Only"), SEARCH_SIGNALS);
	filter_combo->add_item(TTR("Annotations Only"), SEARCH_ANNOTATIONS);
	filter_combo->add_item(TTR("Constants Only"), SEARCH_CONSTANTS);
	filter_combo->add_item(TTR("Properties Only"), SEARCH_PROPERTIES);
	filter_combo->add_item(TTR("Theme Properties Only"), SEARCH_THEME_ITEMS);
	filter_combo->connect(SceneStringName(item_selected), callable_mp(this, &EditorHelpSearch::_filter_combo_item_selected));
	hbox->add_child(filter_combo);

	// Create the results tree.
	results_tree = memnew(Tree);
	results_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	results_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	results_tree->set_columns(2);
	results_tree->set_column_title(0, TTR("Name"));
	results_tree->set_column_clip_content(0, true);
	results_tree->set_column_title(1, TTR("Member Type"));
	results_tree->set_column_expand(1, false);
	results_tree->set_column_custom_minimum_width(1, 150 * EDSCALE);
	results_tree->set_column_clip_content(1, true);
	results_tree->set_custom_minimum_size(Size2(0, 100) * EDSCALE);
	results_tree->set_hide_root(true);
	results_tree->set_select_mode(Tree::SELECT_ROW);
	results_tree->connect("item_activated", callable_mp(this, &EditorHelpSearch::_confirmed));
	results_tree->connect(SceneStringName(item_selected), callable_mp((BaseButton *)get_ok_button(), &BaseButton::set_disabled).bind(false));
	vbox->add_child(results_tree, true);
}

void EditorHelpSearch::TreeCache::clear() {
	for (const KeyValue<String, TreeItem *> &E : item_cache) {
		memdelete(E.value);
	}
	item_cache.clear();
}

bool EditorHelpSearch::Runner::_is_class_disabled_by_feature_profile(const StringName &p_class) {
	Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
	if (profile.is_null()) {
		return false;
	}

	StringName class_name = p_class;
	while (class_name != StringName()) {
		if (!ClassDB::class_exists(class_name)) {
			return false;
		}

		if (profile->is_class_disabled(class_name)) {
			return true;
		}
		class_name = ClassDB::get_parent_class(class_name);
	}

	return false;
}

bool EditorHelpSearch::Runner::_fill() {
	bool phase_done = false;
	switch (phase) {
		case PHASE_MATCH_CLASSES_INIT:
			phase_done = _phase_fill_classes_init();
			break;
		case PHASE_MATCH_CLASSES:
			phase_done = _phase_fill_classes();
			break;
		case PHASE_CLASS_ITEMS_INIT:
		case PHASE_CLASS_ITEMS:
			phase_done = true;
			break;
		case PHASE_MEMBER_ITEMS_INIT:
			phase_done = _phase_fill_member_items_init();
			break;
		case PHASE_MEMBER_ITEMS:
			phase_done = _phase_fill_member_items();
			break;
		case PHASE_SELECT_MATCH:
			phase_done = _phase_select_match();
			break;
		case PHASE_MAX:
			return true;
		default:
			WARN_PRINT("Invalid or unhandled phase in EditorHelpSearch::Runner, aborting search.");
			return true;
	}

	if (phase_done) {
		phase++;
	}
	return false;
}

bool EditorHelpSearch::Runner::_phase_fill_classes_init() {
	// Initialize fill.
	iterator_stack.clear();
	matched_classes.clear();
	matched_item = nullptr;
	match_highest_score = 0;

	// Initialize stack of iterators to fill, in reverse.
	iterator_stack.push_back(EditorHelp::get_doc_data()->inheriting[""].back());

	return true;
}

bool EditorHelpSearch::Runner::_phase_fill_classes() {
	if (iterator_stack.is_empty()) {
		return true;
	}

	if (iterator_stack[iterator_stack.size() - 1]) {
		DocData::ClassDoc *class_doc = EditorHelp::get_doc_data()->class_list.getptr(iterator_stack[iterator_stack.size() - 1]->get());

		// Decrement stack.
		iterator_stack[iterator_stack.size() - 1] = iterator_stack[iterator_stack.size() - 1]->prev();

		// Drop last element of stack if empty.
		if (!iterator_stack[iterator_stack.size() - 1]) {
			iterator_stack.resize(iterator_stack.size() - 1);
		}

		if (!class_doc || class_doc->name.is_empty()) {
			return false;
		}

		// If class matches the flags, add it to the matched stack.
		const bool class_matched =
				(search_flags & SEARCH_CLASSES) ||
				((search_flags & SEARCH_CONSTRUCTORS) && !class_doc->constructors.is_empty()) ||
				((search_flags & SEARCH_METHODS) && !class_doc->methods.is_empty()) ||
				((search_flags & SEARCH_OPERATORS) && !class_doc->operators.is_empty()) ||
				((search_flags & SEARCH_SIGNALS) && !class_doc->signals.is_empty()) ||
				((search_flags & SEARCH_CONSTANTS) && !class_doc->constants.is_empty()) ||
				((search_flags & SEARCH_PROPERTIES) && !class_doc->properties.is_empty()) ||
				((search_flags & SEARCH_THEME_ITEMS) && !class_doc->theme_properties.is_empty()) ||
				((search_flags & SEARCH_ANNOTATIONS) && !class_doc->annotations.is_empty());

		if (class_matched) {
			if (term.is_empty() || class_doc->name.containsn(term)) {
				matched_classes.push_back(Pair<DocData::ClassDoc *, String>(class_doc, String()));
			} else if (String keyword = _match_keywords(term, class_doc->keywords); !keyword.is_empty()) {
				matched_classes.push_back(Pair<DocData::ClassDoc *, String>(class_doc, keyword));
			}
		}

		// Add inheriting classes, in reverse.
		if (class_doc && EditorHelp::get_doc_data()->inheriting.has(class_doc->name)) {
			iterator_stack.push_back(EditorHelp::get_doc_data()->inheriting[class_doc->name].back());
		}

		return false;
	}

	// Drop last element of stack if empty.
	if (!iterator_stack[iterator_stack.size() - 1]) {
		iterator_stack.resize(iterator_stack.size() - 1);
	}

	return iterator_stack.is_empty();
}

bool EditorHelpSearch::Runner::_phase_fill_member_items_init() {
	// Prepare tree.
	class_items.clear();
	_populate_cache();

	return true;
}

TreeItem *EditorHelpSearch::Runner::_create_category_item(TreeItem *p_parent, const String &p_class, const StringName &p_icon, const String &p_text, const String &p_metatype) {
	const String item_meta = "class_" + p_metatype + ":" + p_class;

	TreeItem *item = nullptr;
	if (_find_or_create_item(p_parent, item_meta, item)) {
		item->set_icon(0, ui_service->get_editor_theme_icon(p_icon));
		item->set_text(0, p_text);
		item->set_metadata(0, item_meta);
	}
	item->set_collapsed(true);

	return item;
}

bool EditorHelpSearch::Runner::_phase_fill_member_items() {
	if (matched_classes.is_empty()) {
		return true;
	}

	// Pop working item from stack.
	Pair<DocData::ClassDoc *, String> match = matched_classes[matched_classes.size() - 1];
	DocData::ClassDoc *class_doc = match.first;
	const String &keyword = match.second;
	matched_classes.resize(matched_classes.size() - 1);

	if (class_doc) {
		TreeItem *item = _create_class_hierarchy(class_doc, keyword, !(search_flags & SEARCH_CLASSES));

		// If the class has no inheriting classes, fold its item.
		item->set_collapsed(!item->get_first_child());

		if (search_flags & SEARCH_CLASSES) {
			item->clear_custom_color(0);
			item->clear_custom_color(1);
		} else {
			item->set_custom_color(0, disabled_color);
			item->set_custom_color(1, disabled_color);
		}

		// Create common header if required.
		const bool search_all = (search_flags & SEARCH_ALL) == SEARCH_ALL;

		if ((search_flags & SEARCH_CONSTRUCTORS) && !class_doc->constructors.is_empty()) {
			TreeItem *parent_item = item;
			if (search_all) {
				parent_item = _create_category_item(parent_item, class_doc->name, SNAME("MemberConstructor"), TTRC("Constructors"), "constructors");
			}
			for (const DocData::MethodDoc &constructor_doc : class_doc->constructors) {
				_create_constructor_item(parent_item, class_doc, &constructor_doc);
			}
		}
		if ((search_flags & SEARCH_METHODS) && !class_doc->methods.is_empty()) {
			TreeItem *parent_item = item;
			if (search_all) {
				parent_item = _create_category_item(parent_item, class_doc->name, SNAME("MemberMethod"), TTRC("Methods"), "methods");
			}
			for (const DocData::MethodDoc &method_doc : class_doc->methods) {
				_create_method_item(parent_item, class_doc, &method_doc);
			}
		}
		if ((search_flags & SEARCH_OPERATORS) && !class_doc->operators.is_empty()) {
			TreeItem *parent_item = item;
			if (search_all) {
				parent_item = _create_category_item(parent_item, class_doc->name, SNAME("MemberOperator"), TTRC("Operators"), "operators");
			}
			for (const DocData::MethodDoc &operator_doc : class_doc->operators) {
				_create_operator_item(parent_item, class_doc, &operator_doc);
			}
		}
		if ((search_flags & SEARCH_SIGNALS) && !class_doc->signals.is_empty()) {
			TreeItem *parent_item = item;
			if (search_all) {
				parent_item = _create_category_item(parent_item, class_doc->name, SNAME("MemberSignal"), TTRC("Signals"), "signals");
			}
			for (const DocData::MethodDoc &signal_doc : class_doc->signals) {
				_create_signal_item(parent_item, class_doc, &signal_doc);
			}
		}
		if ((search_flags & SEARCH_CONSTANTS) && !class_doc->constants.is_empty()) {
			TreeItem *parent_item = item;
			if (search_all) {
				parent_item = _create_category_item(parent_item, class_doc->name, SNAME("MemberConstant"), TTRC("Constants"), "constants");
			}
			for (const DocData::ConstantDoc &constant_doc : class_doc->constants) {
				_create_constant_item(parent_item, class_doc, &constant_doc);
			}
		}
		if ((search_flags & SEARCH_PROPERTIES) && !class_doc->properties.is_empty()) {
			TreeItem *parent_item = item;
			if (search_all) {
				parent_item = _create_category_item(parent_item, class_doc->name, SNAME("MemberProperty"), TTRC("Properties"), "properties");
			}
			for (const DocData::PropertyDoc &property_doc : class_doc->properties) {
				_create_property_item(parent_item, class_doc, &property_doc);
			}
		}
		if ((search_flags & SEARCH_THEME_ITEMS) && !class_doc->theme_properties.is_empty()) {
			TreeItem *parent_item = item;
			if (search_all) {
				parent_item = _create_category_item(parent_item, class_doc->name, SNAME("MemberTheme"), TTRC("Theme Properties"), "theme_items");
			}
			for (const DocData::ThemeItemDoc &theme_property_doc : class_doc->theme_properties) {
				_create_theme_property_item(parent_item, class_doc, &theme_property_doc);
			}
		}
		if ((search_flags & SEARCH_ANNOTATIONS) && !class_doc->annotations.is_empty()) {
			TreeItem *parent_item = item;
			if (search_all) {
				parent_item = _create_category_item(parent_item, class_doc->name, SNAME("MemberAnnotation"), TTRC("Annotations"), "annotations");
			}
			for (const DocData::MethodDoc &annotation_doc : class_doc->annotations) {
				_create_annotation_item(parent_item, class_doc, &annotation_doc);
			}
		}
	}

	return matched_classes.is_empty();
}

bool EditorHelpSearch::Runner::_slice() {
	bool phase_done = false;
	switch (phase) {
		case PHASE_MATCH_CLASSES_INIT:
			phase_done = _phase_match_classes_init();
			break;
		case PHASE_MATCH_CLASSES:
			phase_done = _phase_match_classes();
			break;
		case PHASE_CLASS_ITEMS_INIT:
			phase_done = _phase_class_items_init();
			break;
		case PHASE_CLASS_ITEMS:
			phase_done = _phase_class_items();
			break;
		case PHASE_MEMBER_ITEMS_INIT:
			phase_done = _phase_member_items_init();
			break;
		case PHASE_MEMBER_ITEMS:
			phase_done = _phase_member_items();
			break;
		case PHASE_SELECT_MATCH:
			phase_done = _phase_select_match();
			break;
		case PHASE_MAX:
			return true;
		default:
			WARN_PRINT("Invalid or unhandled phase in EditorHelpSearch::Runner, aborting search.");
			return true;
	}

	if (phase_done) {
		phase++;
	}
	return false;
}

bool EditorHelpSearch::Runner::_phase_match_classes_init() {
	iterator_doc = nullptr;
	iterator_stack.clear();
	if (search_flags & SEARCH_SHOW_HIERARCHY) {
		iterator_stack.push_back(EditorHelp::get_doc_data()->inheriting[""].front());
	} else {
		iterator_doc = EditorHelp::get_doc_data()->class_list.begin();
	}
	matches.clear();
	matched_item = nullptr;
	match_highest_score = 0;

	if (!term.is_empty()) {
		terms = term.split_spaces();
		if (terms.is_empty()) {
			terms.append(term);
		}
	}

	return true;
}

bool EditorHelpSearch::Runner::_phase_match_classes() {
	if (!iterator_doc && iterator_stack.is_empty()) {
		return true;
	}

	DocData::ClassDoc *class_doc = nullptr;
	if (iterator_doc) {
		class_doc = &iterator_doc->value;
	} else if (!iterator_stack.is_empty() && iterator_stack[iterator_stack.size() - 1]) {
		class_doc = EditorHelp::get_doc_data()->class_list.getptr(iterator_stack[iterator_stack.size() - 1]->get());
	}

	if (class_doc && class_doc->name.is_empty()) {
		class_doc = nullptr;
	}

	if (class_doc && !_is_class_disabled_by_feature_profile(class_doc->name)) {
		ClassMatch match;
		match.doc = class_doc;

		// Match class name.
		if (search_flags & SEARCH_CLASSES) {
			match.name = _match_string(term, class_doc->name);
			match.keyword = _match_keywords(term, class_doc->keywords);
		}

		if (search_flags & SEARCH_CONSTRUCTORS) {
			_match_method_name_and_push_back(class_doc->constructors, &match.constructors);
		}
		if (search_flags & SEARCH_METHODS) {
			_match_method_name_and_push_back(class_doc->methods, &match.methods);
		}
		if (search_flags & SEARCH_OPERATORS) {
			_match_method_name_and_push_back(class_doc->operators, &match.operators);
		}
		if (search_flags & SEARCH_SIGNALS) {
			for (const DocData::MethodDoc &signal_doc : class_doc->signals) {
				MemberMatch<DocData::MethodDoc> signal;
				signal.name = _all_terms_in_name(signal_doc.name);
				signal.keyword = _match_keywords_in_all_terms(signal_doc.keywords);
				if (signal.name || !signal.keyword.is_empty()) {
					signal.doc = &signal_doc;
					match.signals.push_back(signal);
				}
			}
		}
		if (search_flags & SEARCH_CONSTANTS) {
			for (const DocData::ConstantDoc &constant_doc : class_doc->constants) {
				MemberMatch<DocData::ConstantDoc> constant;
				constant.name = _all_terms_in_name(constant_doc.name);
				constant.keyword = _match_keywords_in_all_terms(constant_doc.keywords);
				if (constant.name || !constant.keyword.is_empty()) {
					constant.doc = &constant_doc;
					match.constants.push_back(constant);
				}
			}
		}
		if (search_flags & SEARCH_PROPERTIES) {
			for (const DocData::PropertyDoc &property_doc : class_doc->properties) {
				MemberMatch<DocData::PropertyDoc> property;
				property.name = _all_terms_in_name(property_doc.name);
				property.keyword = _match_keywords_in_all_terms(property_doc.keywords);
				if (property.name || !property.keyword.is_empty()) {
					property.doc = &property_doc;
					match.properties.push_back(property);
				}
			}
		}
		if (search_flags & SEARCH_THEME_ITEMS) {
			for (const DocData::ThemeItemDoc &theme_property_doc : class_doc->theme_properties) {
				MemberMatch<DocData::ThemeItemDoc> theme_property;
				theme_property.name = _all_terms_in_name(theme_property_doc.name);
				theme_property.keyword = _match_keywords_in_all_terms(theme_property_doc.keywords);
				if (theme_property.name || !theme_property.keyword.is_empty()) {
					theme_property.doc = &theme_property_doc;
					match.theme_properties.push_back(theme_property);
				}
			}
		}
		if (search_flags & SEARCH_ANNOTATIONS) {
			for (const DocData::MethodDoc &annotation_doc : class_doc->annotations) {
				MemberMatch<DocData::MethodDoc> annotation;
				annotation.name = _all_terms_in_name(annotation_doc.name);
				annotation.keyword = _match_keywords_in_all_terms(annotation_doc.keywords);
				if (annotation.name || !annotation.keyword.is_empty()) {
					annotation.doc = &annotation_doc;
					match.annotations.push_back(annotation);
				}
			}
		}
		matches[class_doc->name] = match;
	}

	if (iterator_doc) {
		++iterator_doc;
		return !iterator_doc;
	}

	if (!iterator_stack.is_empty()) {
		// Iterate on stack.
		if (iterator_stack[iterator_stack.size() - 1]) {
			iterator_stack[iterator_stack.size() - 1] = iterator_stack[iterator_stack.size() - 1]->next();
		}
		// Drop last element of stack.
		if (!iterator_stack[iterator_stack.size() - 1]) {
			iterator_stack.resize(iterator_stack.size() - 1);
		}
	}

	if (class_doc && EditorHelp::get_doc_data()->inheriting.has(class_doc->name)) {
		iterator_stack.push_back(EditorHelp::get_doc_data()->inheriting[class_doc->name].front());
	}

	return iterator_stack.is_empty();
}

void EditorHelpSearch::Runner::_populate_cache() {
	// Deselect to prevent re-selection issues.
	results_tree->deselect_all();

	root_item = results_tree->get_root();

	if (root_item) {
		LocalVector<TreeItem *> stack;

		// Add children of root item to stack.
		for (TreeItem *child = root_item->get_first_child(); child; child = child->get_next()) {
			stack.push_back(child);
		}

		// Traverse stack and cache items.
		while (!stack.is_empty()) {
			TreeItem *cur_item = stack[stack.size() - 1];
			stack.resize(stack.size() - 1);

			// Add to the cache.
			tree_cache->item_cache.insert(cur_item->get_metadata(0).operator String(), cur_item);

			// Add any children to the stack.
			for (TreeItem *child = cur_item->get_first_child(); child; child = child->get_next()) {
				stack.push_back(child);
			}

			// Remove from parent.
			cur_item->get_parent()->remove_child(cur_item);
		}
	} else {
		root_item = results_tree->create_item();
	}
}

bool EditorHelpSearch::Runner::_phase_class_items_init() {
	iterator_match = matches.begin();

	_populate_cache();
	class_items.clear();

	return true;
}

bool EditorHelpSearch::Runner::_phase_class_items() {
	if (!iterator_match) {
		return true;
	}
	ClassMatch &match = iterator_match->value;

	if (search_flags & SEARCH_SHOW_HIERARCHY) {
		if (match.required()) {
			_create_class_hierarchy(match);
		}
	} else {
		if (match.name || !match.keyword.is_empty()) {
			_create_class_item(root_item, match.doc, false, match.name ? String() : match.keyword);
		}
	}

	++iterator_match;
	return !iterator_match;
}

bool EditorHelpSearch::Runner::_phase_member_items_init() {
	iterator_match = matches.begin();

	return true;
}

bool EditorHelpSearch::Runner::_phase_member_items() {
	if (!iterator_match) {
		return true;
	}

	ClassMatch &match = iterator_match->value;

	if (!match.doc || match.doc->name.is_empty()) {
		++iterator_match;
		return false;
	}

	// Pick appropriate parent item if showing hierarchy, otherwise pick root.
	TreeItem *parent_item = (search_flags & SEARCH_SHOW_HIERARCHY) ? class_items[match.doc->name] : root_item;

	for (const MemberMatch<DocData::MethodDoc> &constructor_item : match.constructors) {
		_create_constructor_item(parent_item, match.doc, constructor_item);
	}
	for (const MemberMatch<DocData::MethodDoc> &method_item : match.methods) {
		_create_method_item(parent_item, match.doc, method_item);
	}
	for (const MemberMatch<DocData::MethodDoc> &operator_item : match.operators) {
		_create_operator_item(parent_item, match.doc, operator_item);
	}
	for (const MemberMatch<DocData::MethodDoc> &signal_item : match.signals) {
		_create_signal_item(parent_item, match.doc, signal_item);
	}
	for (const MemberMatch<DocData::ConstantDoc> &constant_item : match.constants) {
		_create_constant_item(parent_item, match.doc, constant_item);
	}
	for (const MemberMatch<DocData::PropertyDoc> &property_item : match.properties) {
		_create_property_item(parent_item, match.doc, property_item);
	}
	for (const MemberMatch<DocData::ThemeItemDoc> &theme_property_item : match.theme_properties) {
		_create_theme_property_item(parent_item, match.doc, theme_property_item);
	}
	for (const MemberMatch<DocData::MethodDoc> &annotation_item : match.annotations) {
		_create_annotation_item(parent_item, match.doc, annotation_item);
	}

	++iterator_match;
	return !iterator_match;
}

bool EditorHelpSearch::Runner::_phase_select_match() {
	if (matched_item) {
		matched_item->select(0);
	}
	return true;
}

void EditorHelpSearch::Runner::_match_method_name_and_push_back(Vector<DocData::MethodDoc> &p_methods, LocalVector<MemberMatch<DocData::MethodDoc>> *r_match_methods) {
	// Constructors, Methods, Operators...
	for (int i = 0; i < p_methods.size(); i++) {
		String method_name = (search_flags & SEARCH_CASE_SENSITIVE) ? p_methods[i].name : p_methods[i].name.to_lower();
		String keywords = (search_flags & SEARCH_CASE_SENSITIVE) ? p_methods[i].keywords : p_methods[i].keywords.to_lower();
		MemberMatch<DocData::MethodDoc> method;
		method.name = _all_terms_in_name(method_name);
		method.keyword = _match_keywords_in_all_terms(keywords);
		if (method.name || !method.keyword.is_empty() ||
				(term.begins_with(".") && method_name.begins_with(term.substr(1))) ||
				(term.ends_with("(") && method_name.ends_with(term.left(term.length() - 1).strip_edges())) ||
				(term.begins_with(".") && term.ends_with("(") && method_name == term.substr(1, term.length() - 2).strip_edges())) {
			method.doc = &p_methods[i];
			r_match_methods->push_back(method);
		}
	}
}

bool EditorHelpSearch::Runner::_all_terms_in_name(const String &p_name) const {
	for (int i = 0; i < terms.size(); i++) {
		if (!_match_string(terms[i], p_name)) {
			return false;
		}
	}
	return true;
}

String EditorHelpSearch::Runner::_match_keywords_in_all_terms(const String &p_keywords) const {
	String matching_keyword;
	for (int i = 0; i < terms.size(); i++) {
		matching_keyword = _match_keywords(terms[i], p_keywords);
		if (matching_keyword.is_empty()) {
			return String();
		}
	}
	return matching_keyword;
}

bool EditorHelpSearch::Runner::_match_string(const String &p_term, const String &p_string) const {
	if (search_flags & SEARCH_CASE_SENSITIVE) {
		return p_string.contains(p_term);
	} else {
		return p_string.containsn(p_term);
	}
}

String EditorHelpSearch::Runner::_match_keywords(const String &p_term, const String &p_keywords) const {
	for (const String &k : p_keywords.split(",")) {
		const String keyword = k.strip_edges();
		if (_match_string(p_term, keyword)) {
			return keyword;
		}
	}
	return String();
}

void EditorHelpSearch::Runner::_match_item(TreeItem *p_item, const String &p_text, bool p_is_keywords) {
	if (p_text.is_empty()) {
		return;
	}

	float inverse_length = 1.0f / float(p_text.length());

	// Favor types where search term is a substring close to the start of the type.
	float w = 0.5f;
	int pos = p_text.findn(term);
	float score = (pos > -1) ? 1.0f - w * MIN(1, 3 * pos * inverse_length) : MAX(0.0f, 0.9f - w);

	// Favor shorter items: they resemble the search term more.
	w = 0.1f;
	score *= (1 - w) + w * (term.length() * inverse_length);

	// Reduce the score of keywords, since they are an indirect match.
	if (p_is_keywords) {
		score *= 0.9f;
	}

	// Replace current match if term is short as we are searching in reverse.
	if (match_highest_score == 0 || score > match_highest_score || (score == match_highest_score && term.length() == 1)) {
		matched_item = p_item;
		match_highest_score = score;
	}
}

String EditorHelpSearch::Runner::_build_method_tooltip(const DocData::ClassDoc *p_class_doc, const DocData::MethodDoc *p_doc) const {
	String tooltip = p_doc->return_type + " " + p_class_doc->name + "." + p_doc->name + "(";
	for (int i = 0; i < p_doc->arguments.size(); i++) {
		const DocData::ArgumentDoc &arg = p_doc->arguments[i];
		tooltip += arg.type + " " + arg.name;
		if (!arg.default_value.is_empty()) {
			tooltip += " = " + arg.default_value;
		}
		if (i < p_doc->arguments.size() - 1) {
			tooltip += ", ";
		}
	}
	tooltip += ")";
	tooltip += _build_keywords_tooltip(p_doc->keywords);
	return tooltip;
}

String EditorHelpSearch::Runner::_build_keywords_tooltip(const String &p_keywords) const {
	String tooltip;
	if (p_keywords.is_empty()) {
		return tooltip;
	}

	tooltip = "\n\n" + TTR("Keywords") + ": ";

	for (const String &keyword : p_keywords.split(",")) {
		tooltip += keyword.strip_edges().quote() + ", ";
	}

	// Remove trailing comma and space.
	return tooltip.left(-2);
}

TreeItem *EditorHelpSearch::Runner::_create_class_hierarchy(const DocData::ClassDoc *p_class_doc, const String &p_matching_keyword, bool p_gray) {
	if (p_class_doc->name.is_empty()) {
		return nullptr;
	}
	if (TreeItem **found = class_items.getptr(p_class_doc->name)) {
		return *found;
	}

	// Ensure parent nodes are created first.
	TreeItem *parent_item = root_item;
	if (!p_class_doc->inherits.is_empty()) {
		if (class_items.has(p_class_doc->inherits)) {
			parent_item = class_items[p_class_doc->inherits];
		} else if (const DocData::ClassDoc *found = EditorHelp::get_doc_data()->class_list.getptr(p_class_doc->inherits)) {
			parent_item = _create_class_hierarchy(found, String(), true);
		}
	}

	TreeItem *class_item = _create_class_item(parent_item, p_class_doc, p_gray, p_matching_keyword);
	class_items[p_class_doc->name] = class_item;
	return class_item;
}

TreeItem *EditorHelpSearch::Runner::_create_class_hierarchy(const ClassMatch &p_match) {
	if (p_match.doc->name.is_empty()) {
		return nullptr;
	}
	if (class_items.has(p_match.doc->name)) {
		return class_items[p_match.doc->name];
	}

	// Ensure parent nodes are created first.
	TreeItem *parent_item = root_item;
	if (!p_match.doc->inherits.is_empty()) {
		if (class_items.has(p_match.doc->inherits)) {
			parent_item = class_items[p_match.doc->inherits];
		} else {
			ClassMatch &base_match = matches[p_match.doc->inherits];
			if (base_match.doc) {
				parent_item = _create_class_hierarchy(base_match);
			}
		}
	}

	TreeItem *class_item = _create_class_item(parent_item, p_match.doc, !p_match.name && p_match.keyword.is_empty(), p_match.name ? String() : p_match.keyword);
	class_items[p_match.doc->name] = class_item;
	return class_item;
}

bool EditorHelpSearch::Runner::_find_or_create_item(TreeItem *p_parent, const String &p_item_meta, TreeItem *&r_item) {
	// Attempt to find in cache.
	if (tree_cache->item_cache.has(p_item_meta)) {
		r_item = tree_cache->item_cache[p_item_meta];

		// Remove from cache.
		tree_cache->item_cache.erase(p_item_meta);

		// Add to tree.
		p_parent->add_child(r_item);

		return false;
	} else {
		// Otherwise create item.
		r_item = results_tree->create_item(p_parent);

		return true;
	}
}

TreeItem *EditorHelpSearch::Runner::_create_class_item(TreeItem *p_parent, const DocData::ClassDoc *p_doc, bool p_gray, const String &p_matching_keyword) {
	String tooltip = DTR(p_doc->brief_description.strip_edges());
	tooltip += _build_keywords_tooltip(p_doc->keywords);

	const String item_meta = "class_name:" + p_doc->name;

	TreeItem *item = nullptr;
	if (_find_or_create_item(p_parent, item_meta, item)) {
		item->set_icon(0, EditorNode::get_singleton()->get_class_icon(p_doc->name));
		item->set_text(1, TTR("Class"));
		item->set_tooltip_text(0, tooltip);
		item->set_tooltip_text(1, tooltip);
		item->set_metadata(0, item_meta);
		if (p_doc->is_deprecated) {
			Ref<Texture2D> error_icon = ui_service->get_editor_theme_icon(SNAME("StatusError"));
			item->add_button(0, error_icon, 0, false, TTR("This class is marked as deprecated."));
		} else if (p_doc->is_experimental) {
			Ref<Texture2D> warning_icon = ui_service->get_editor_theme_icon(SNAME("NodeWarning"));
			item->add_button(0, warning_icon, 0, false, TTR("This class is marked as experimental."));
		}
	}
	// Cached item might be collapsed.
	item->set_collapsed(false);

	if (p_gray) {
		item->set_custom_color(0, disabled_color);
		item->set_custom_color(1, disabled_color);
	} else {
		item->clear_custom_color(0);
		item->clear_custom_color(1);
	}

	if (p_matching_keyword.is_empty()) {
		item->set_text(0, p_doc->name);
	} else {
		item->set_text(0, p_doc->name + "      - " + vformat(TTR("Matches the \"%s\" keyword."), p_matching_keyword));
	}

	if (!term.is_empty()) {
		_match_item(item, p_doc->name);
	}
	for (const String &keyword : p_doc->keywords.split(",")) {
		_match_item(item, keyword.strip_edges(), true);
	}

	return item;
}

TreeItem *EditorHelpSearch::Runner::_create_constructor_item(TreeItem *p_parent, const DocData::ClassDoc *p_class_doc, const MemberMatch<DocData::MethodDoc> &p_match) {
	String tooltip = p_class_doc->name + "(";
	String text = p_class_doc->name + "(";
	for (int i = 0; i < p_match.doc->arguments.size(); i++) {
		const DocData::ArgumentDoc &arg = p_match.doc->arguments[i];
		tooltip += arg.type + " " + arg.name;
		text += arg.type;
		if (!arg.default_value.is_empty()) {
			tooltip += " = " + arg.default_value;
		}
		if (i < p_match.doc->arguments.size() - 1) {
			tooltip += ", ";
			text += ", ";
		}
	}
	tooltip += ")";
	tooltip += _build_keywords_tooltip(p_match.doc->keywords);
	text += ")";
	return _create_member_item(p_parent, p_class_doc->name, SNAME("MemberConstructor"), p_match.doc->name, text, TTRC("Constructor"), "method", tooltip, p_match.doc->keywords, p_match.doc->is_deprecated, p_match.doc->is_experimental, p_match.name ? String() : p_match.keyword);
}

TreeItem *EditorHelpSearch::Runner::_create_method_item(TreeItem *p_parent, const DocData::ClassDoc *p_class_doc, const MemberMatch<DocData::MethodDoc> &p_match) {
	String tooltip = _build_method_tooltip(p_class_doc, p_match.doc);
	return _create_member_item(p_parent, p_class_doc->name, SNAME("MemberMethod"), p_match.doc->name, p_match.doc->name, TTRC("Method"), "method", tooltip, p_match.doc->keywords, p_match.doc->is_deprecated, p_match.doc->is_experimental, p_match.name ? String() : p_match.keyword);
}

TreeItem *EditorHelpSearch::Runner::_create_operator_item(TreeItem *p_parent, const DocData::ClassDoc *p_class_doc, const MemberMatch<DocData::MethodDoc> &p_match) {
	String tooltip = _build_method_tooltip(p_class_doc, p_match.doc);
	String text = p_match.doc->name;
	if (!p_match.doc->arguments.is_empty()) {
		text += "(" + p_match.doc->arguments[0].type + ")";
	}
	return _create_member_item(p_parent, p_class_doc->name, SNAME("MemberOperator"), p_match.doc->name, text, TTRC("Operator"), "method", tooltip, p_match.doc->keywords, p_match.doc->is_deprecated, p_match.doc->is_experimental, p_match.name ? String() : p_match.keyword);
}

TreeItem *EditorHelpSearch::Runner::_create_signal_item(TreeItem *p_parent, const DocData::ClassDoc *p_class_doc, const MemberMatch<DocData::MethodDoc> &p_match) {
	String tooltip = _build_method_tooltip(p_class_doc, p_match.doc);
	return _create_member_item(p_parent, p_class_doc->name, SNAME("MemberSignal"), p_match.doc->name, p_match.doc->name, TTRC("Signal"), "signal", tooltip, p_match.doc->keywords, p_match.doc->is_deprecated, p_match.doc->is_experimental, p_match.name ? String() : p_match.keyword);
}

TreeItem *EditorHelpSearch::Runner::_create_annotation_item(TreeItem *p_parent, const DocData::ClassDoc *p_class_doc, const MemberMatch<DocData::MethodDoc> &p_match) {
	String tooltip = _build_method_tooltip(p_class_doc, p_match.doc);
	// Hide the redundant leading @ symbol.
	String text = p_match.doc->name.substr(1);
	return _create_member_item(p_parent, p_class_doc->name, SNAME("MemberAnnotation"), p_match.doc->name, text, TTRC("Annotation"), "annotation", tooltip, p_match.doc->keywords, p_match.doc->is_deprecated, p_match.doc->is_experimental, p_match.name ? String() : p_match.keyword);
}

TreeItem *EditorHelpSearch::Runner::_create_constant_item(TreeItem *p_parent, const DocData::ClassDoc *p_class_doc, const MemberMatch<DocData::ConstantDoc> &p_match) {
	String tooltip = p_class_doc->name + "." + p_match.doc->name;
	tooltip += _build_keywords_tooltip(p_match.doc->keywords);
	return _create_member_item(p_parent, p_class_doc->name, SNAME("MemberConstant"), p_match.doc->name, p_match.doc->name, TTRC("Constant"), "constant", tooltip, p_match.doc->keywords, p_match.doc->is_deprecated, p_match.doc->is_experimental, p_match.name ? String() : p_match.keyword);
}

TreeItem *EditorHelpSearch::Runner::_create_property_item(TreeItem *p_parent, const DocData::ClassDoc *p_class_doc, const MemberMatch<DocData::PropertyDoc> &p_match) {
	String tooltip = p_match.doc->type + " " + p_class_doc->name + "." + p_match.doc->name;
	tooltip += "\n    " + p_class_doc->name + "." + p_match.doc->setter + "(value) setter";
	tooltip += "\n    " + p_class_doc->name + "." + p_match.doc->getter + "() getter";
	return _create_member_item(p_parent, p_class_doc->name, SNAME("MemberProperty"), p_match.doc->name, p_match.doc->name, TTRC("Property"), "property", tooltip, p_match.doc->keywords, p_match.doc->is_deprecated, p_match.doc->is_experimental, p_match.name ? String() : p_match.keyword);
}

TreeItem *EditorHelpSearch::Runner::_create_theme_property_item(TreeItem *p_parent, const DocData::ClassDoc *p_class_doc, const MemberMatch<DocData::ThemeItemDoc> &p_match) {
	String tooltip = p_match.doc->type + " " + p_class_doc->name + "." + p_match.doc->name;
	tooltip += _build_keywords_tooltip(p_match.doc->keywords);
	return _create_member_item(p_parent, p_class_doc->name, SNAME("MemberTheme"), p_match.doc->name, p_match.doc->name, TTRC("Theme Property"), "theme_item", p_match.doc->keywords, tooltip, p_match.doc->is_deprecated, p_match.doc->is_experimental, p_match.name ? String() : p_match.keyword);
}

TreeItem *EditorHelpSearch::Runner::_create_member_item(TreeItem *p_parent, const String &p_class_name, const StringName &p_icon, const String &p_name, const String &p_text, const String &p_type, const String &p_metatype, const String &p_tooltip, const String &p_keywords, bool p_is_deprecated, bool p_is_experimental, const String &p_matching_keyword) {
	const String item_meta = "class_" + p_metatype + ":" + p_class_name + ":" + p_name;

	TreeItem *item = nullptr;
	if (_find_or_create_item(p_parent, item_meta, item)) {
		item->set_icon(0, ui_service->get_editor_theme_icon(p_icon));
		item->set_text(1, TTRGET(p_type));
		item->set_tooltip_text(0, p_tooltip);
		item->set_tooltip_text(1, p_tooltip);
		item->set_metadata(0, item_meta);

		if (p_is_deprecated) {
			Ref<Texture2D> error_icon = ui_service->get_editor_theme_icon(SNAME("StatusError"));
			item->add_button(0, error_icon, 0, false, TTR("This member is marked as deprecated."));
		} else if (p_is_experimental) {
			Ref<Texture2D> warning_icon = ui_service->get_editor_theme_icon(SNAME("NodeWarning"));
			item->add_button(0, warning_icon, 0, false, TTR("This member is marked as experimental."));
		}
	}

	String text;
	if (search_flags & SEARCH_SHOW_HIERARCHY) {
		text = p_text;
	} else {
		text = p_class_name + "." + p_text;
	}
	if (!p_matching_keyword.is_empty()) {
		text += "      - " + vformat(TTR("Matches the \"%s\" keyword."), p_matching_keyword);
	}
	item->set_text(0, text);

	// Don't match member items for short searches.
	if (term.length() > 1 || term == "@") {
		_match_item(item, p_name);
	}
	for (const String &keyword : p_keywords.split(",")) {
		_match_item(item, keyword.strip_edges(), true);
	}

	return item;
}

bool EditorHelpSearch::Runner::work(uint64_t slot) {
	// Return true when the search has been completed, otherwise false.
	const uint64_t until = OS::get_singleton()->get_ticks_usec() + slot;
	if (term.length() > 1 || term == "@") {
		while (!_slice()) {
			if (OS::get_singleton()->get_ticks_usec() > until) {
				return false;
			}
		}
	} else {
		while (!_fill()) {
			if (OS::get_singleton()->get_ticks_usec() > until) {
				return false;
			}
		}
	}
	return true;
}

EditorHelpSearch::Runner::Runner(Control *p_icon_service, Tree *p_results_tree, TreeCache *p_tree_cache, const String &p_term, int p_search_flags) :
		ui_service(p_icon_service),
		results_tree(p_results_tree),
		tree_cache(p_tree_cache),
		term((p_search_flags & SEARCH_CASE_SENSITIVE) == 0 ? p_term.to_lower() : p_term),
		search_flags(p_search_flags),
		disabled_color(ui_service->get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor))) {
}
