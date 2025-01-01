/**************************************************************************/
/*  connections_dialog.cpp                                                */
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

#include "connections_dialog.h"

#include "core/config/project_settings.h"
#include "core/templates/hash_set.h"
#include "editor/editor_help.h"
#include "editor/editor_inspector.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/scene_tree_editor.h"
#include "editor/node_dock.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/scene_tree_dock.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/spin_box.h"

static Node *_find_first_script(Node *p_root, Node *p_node) {
	if (p_node != p_root && p_node->get_owner() != p_root) {
		return nullptr;
	}
	if (!p_node->get_script().is_null()) {
		return p_node;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *ret = _find_first_script(p_root, p_node->get_child(i));
		if (ret) {
			return ret;
		}
	}

	return nullptr;
}

class ConnectDialogBinds : public Object {
	GDCLASS(ConnectDialogBinds, Object);

public:
	Vector<Variant> params;

	bool _set(const StringName &p_name, const Variant &p_value) {
		String name = p_name;

		if (name.begins_with("bind/argument_")) {
			int which = name.get_slice("_", 1).to_int() - 1;
			ERR_FAIL_INDEX_V(which, params.size(), false);
			params.write[which] = p_value;
		} else {
			return false;
		}

		return true;
	}

	bool _get(const StringName &p_name, Variant &r_ret) const {
		String name = p_name;

		if (name.begins_with("bind/argument_")) {
			int which = name.get_slice("_", 1).to_int() - 1;
			ERR_FAIL_INDEX_V(which, params.size(), false);
			r_ret = params[which];
		} else {
			return false;
		}

		return true;
	}

	void _get_property_list(List<PropertyInfo> *p_list) const {
		for (int i = 0; i < params.size(); i++) {
			p_list->push_back(PropertyInfo(params[i].get_type(), "bind/argument_" + itos(i + 1)));
		}
	}

	void notify_changed() {
		notify_property_list_changed();
	}

	ConnectDialogBinds() {
	}
};

/*
 * Signal automatically called by parent dialog.
 */
void ConnectDialog::ok_pressed() {
	String method_name = dst_method->get_text();

	if (method_name.is_empty()) {
		error->set_text(TTR("Method in target node must be specified."));
		error->popup_centered();
		return;
	}

	if (!TS->is_valid_identifier(method_name.strip_edges())) {
		error->set_text(TTR("Method name must be a valid identifier."));
		error->popup_centered();
		return;
	}

	Node *target = tree->get_selected();
	if (!target) {
		return; // Nothing selected in the tree, not an error.
	}
	if (target->get_script().is_null()) {
		if (!target->has_method(method_name)) {
			error->set_text(TTR("Target method not found. Specify a valid method or attach a script to the target node."));
			error->popup_centered();
			return;
		}
	}
	emit_signal(SNAME("connected"));
	hide();
}

void ConnectDialog::_cancel_pressed() {
	hide();
}

void ConnectDialog::_item_activated() {
	_ok_pressed(); // From AcceptDialog.
}

/*
 * Called each time a target node is selected within the target node tree.
 */
void ConnectDialog::_tree_node_selected() {
	Node *current = tree->get_selected();

	if (!current) {
		return;
	}

	dst_path = source->get_path_to(current);
	if (!edit_mode) {
		set_dst_method(generate_method_callback_name(source, signal, current));
	}
	_update_method_tree();
	_update_warning_label();
	_update_ok_enabled();
}

void ConnectDialog::_focus_currently_connected() {
	tree->set_selected(source);
}

void ConnectDialog::_unbind_count_changed(double p_count) {
	for (Control *control : bind_controls) {
		BaseButton *b = Object::cast_to<BaseButton>(control);
		if (b) {
			b->set_disabled(p_count > 0);
		}

		EditorInspector *e = Object::cast_to<EditorInspector>(control);
		if (e) {
			e->set_read_only(p_count > 0);
		}
	}
}

void ConnectDialog::_method_selected() {
	TreeItem *selected_item = method_tree->get_selected();
	dst_method->set_text(selected_item->get_metadata(0));
}

/*
 * Adds a new parameter bind to connection.
 */
void ConnectDialog::_add_bind() {
	Variant::Type type = (Variant::Type)type_list->get_item_id(type_list->get_selected());

	Variant value;
	Callable::CallError err;
	Variant::construct(type, value, nullptr, 0, err);

	cdbinds->params.push_back(value);
	cdbinds->notify_changed();
}

/*
 * Remove parameter bind from connection.
 */
void ConnectDialog::_remove_bind() {
	String st = bind_editor->get_selected_path();
	if (st.is_empty()) {
		return;
	}
	int idx = st.get_slice("/", 1).to_int() - 1;

	ERR_FAIL_INDEX(idx, cdbinds->params.size());
	cdbinds->params.remove_at(idx);
	cdbinds->notify_changed();
}
/*
 * Automatically generates a name for the callback method.
 */
StringName ConnectDialog::generate_method_callback_name(Node *p_source, const String &p_signal_name, Node *p_target) {
	String node_name = p_source->get_name();
	for (int i = 0; i < node_name.length(); i++) { // TODO: Regex filter may be cleaner.
		char32_t c = node_name[i];
		if ((i == 0 && !is_unicode_identifier_start(c)) || (i > 0 && !is_unicode_identifier_continue(c))) {
			if (c == ' ') {
				// Replace spaces with underlines.
				c = '_';
			} else {
				// Remove any other characters.
				node_name.remove_at(i);
				i--;
				continue;
			}
		}
		node_name[i] = c;
	}

	Dictionary subst;
	subst["NodeName"] = node_name.to_pascal_case();
	subst["nodeName"] = node_name.to_camel_case();
	subst["node_name"] = node_name.to_snake_case();

	subst["SignalName"] = p_signal_name.to_pascal_case();
	subst["signalName"] = p_signal_name.to_camel_case();
	subst["signal_name"] = p_signal_name.to_snake_case();

	String dst_method;
	if (p_source == p_target) {
		dst_method = String(GLOBAL_GET("editor/naming/default_signal_callback_to_self_name")).format(subst);
	} else {
		dst_method = String(GLOBAL_GET("editor/naming/default_signal_callback_name")).format(subst);
	}

	return dst_method;
}

void ConnectDialog::_create_method_tree_items(const List<MethodInfo> &p_methods, TreeItem *p_parent_item) {
	for (const MethodInfo &mi : p_methods) {
		TreeItem *method_item = method_tree->create_item(p_parent_item);
		method_item->set_text(0, get_signature(mi));
		method_item->set_metadata(0, mi.name);
	}
}

List<MethodInfo> ConnectDialog::_filter_method_list(const List<MethodInfo> &p_methods, const MethodInfo &p_signal, const String &p_search_string) const {
	bool check_signal = compatible_methods_only->is_pressed();
	List<MethodInfo> ret;

	List<Pair<Variant::Type, StringName>> effective_args;
	int unbind = get_unbinds();
	for (int i = 0; i < p_signal.arguments.size() - unbind; i++) {
		PropertyInfo pi = p_signal.arguments.get(i);
		effective_args.push_back(Pair(pi.type, pi.class_name));
	}
	if (unbind == 0) {
		for (const Variant &variant : get_binds()) {
			effective_args.push_back(Pair(variant.get_type(), StringName()));
		}
	}

	for (const MethodInfo &mi : p_methods) {
		if (mi.name.begins_with("@")) {
			// GH-92782. GDScript inline setters/getters are historically present in `get_method_list()`
			// and can be called using `Object.call()`. However, these functions are meant to be internal
			// and their names are not valid identifiers, so let's hide them from the user.
			continue;
		}

		if (!p_search_string.is_empty() && !mi.name.containsn(p_search_string)) {
			continue;
		}

		if (check_signal) {
			if (mi.arguments.size() != effective_args.size()) {
				continue;
			}

			bool type_mismatch = false;
			const List<Pair<Variant::Type, StringName>>::Element *E = effective_args.front();
			for (const List<PropertyInfo>::Element *F = mi.arguments.front(); F; F = F->next(), E = E->next()) {
				Variant::Type stype = E->get().first;
				Variant::Type mtype = F->get().type;

				if (stype != Variant::NIL && mtype != Variant::NIL && stype != mtype) {
					type_mismatch = true;
					break;
				}

				if (stype == Variant::OBJECT && mtype == Variant::OBJECT && !ClassDB::is_parent_class(E->get().second, F->get().class_name)) {
					type_mismatch = true;
					break;
				}
			}

			if (type_mismatch) {
				continue;
			}
		}

		ret.push_back(mi);
	}

	return ret;
}

void ConnectDialog::_update_method_tree() {
	method_tree->clear();

	Color disabled_color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor)) * 0.7;
	String search_string = method_search->get_text();
	Node *target = tree->get_selected();
	if (!target) {
		return;
	}

	MethodInfo signal_info;
	if (compatible_methods_only->is_pressed()) {
		List<MethodInfo> signals;
		source->get_signal_list(&signals);
		for (const MethodInfo &mi : signals) {
			if (mi.name == signal) {
				signal_info = mi;
				break;
			}
		}
	}

	TreeItem *root_item = method_tree->create_item();
	root_item->set_text(0, TTR("Methods"));
	root_item->set_selectable(0, false);

	// If a script is attached, get methods from it.
	ScriptInstance *si = target->get_script_instance();
	if (si) {
		if (si->get_script()->is_built_in()) {
			si->get_script()->reload();
		}
		List<MethodInfo> methods;
		si->get_method_list(&methods);
		methods = _filter_method_list(methods, signal_info, search_string);

		if (!methods.is_empty()) {
			TreeItem *si_item = method_tree->create_item(root_item);
			si_item->set_text(0, TTR("Attached Script"));
			si_item->set_icon(0, get_editor_theme_icon(SNAME("Script")));
			si_item->set_selectable(0, false);

			_create_method_tree_items(methods, si_item);
		}
	}

	if (script_methods_only->is_pressed()) {
		empty_tree_label->set_visible(root_item->get_first_child() == nullptr);
		return;
	}

	// Get methods from each class in the hierarchy.
	StringName current_class = target->get_class_name();
	do {
		TreeItem *class_item = method_tree->create_item(root_item);
		class_item->set_text(0, current_class);
		Ref<Texture2D> icon = get_editor_theme_icon(SNAME("Node"));
		if (has_theme_icon(current_class, EditorStringName(EditorIcons))) {
			icon = get_editor_theme_icon(current_class);
		}
		class_item->set_icon(0, icon);
		class_item->set_selectable(0, false);

		List<MethodInfo> methods;
		ClassDB::get_method_list(current_class, &methods, true);
		methods = _filter_method_list(methods, signal_info, search_string);

		if (methods.is_empty()) {
			class_item->set_custom_color(0, disabled_color);
		} else {
			_create_method_tree_items(methods, class_item);
		}
		current_class = ClassDB::get_parent_class_nocheck(current_class);
	} while (current_class != StringName());

	empty_tree_label->set_visible(root_item->get_first_child() == nullptr);
}

void ConnectDialog::_method_check_button_pressed(const CheckButton *p_button) {
	if (p_button == script_methods_only) {
		EditorSettings::get_singleton()->set_project_metadata("editor_metadata", "show_script_methods_only", p_button->is_pressed());
	} else if (p_button == compatible_methods_only) {
		EditorSettings::get_singleton()->set_project_metadata("editor_metadata", "show_compatible_methods_only", p_button->is_pressed());
	}
	_update_method_tree();
}

void ConnectDialog::_open_method_popup() {
	method_popup->popup_centered();
	method_search->clear();
	method_search->grab_focus();
}

/*
 * Enables or disables the connect button. The connect button is enabled if a
 * node is selected and valid in the selected mode.
 */
void ConnectDialog::_update_ok_enabled() {
	Node *target = tree->get_selected();

	if (target == nullptr) {
		get_ok_button()->set_disabled(true);
		return;
	}

	if (dst_method->get_text().is_empty()) {
		get_ok_button()->set_disabled(true);
		return;
	}

	get_ok_button()->set_disabled(false);
}

void ConnectDialog::_update_warning_label() {
	Ref<Script> scr = source->get_node(dst_path)->get_script();
	if (scr.is_null()) {
		warning_label->set_visible(false);
		return;
	}

	ScriptLanguage *language = scr->get_language();
	if (language->can_make_function()) {
		warning_label->set_visible(false);
		return;
	}

	warning_label->set_text(vformat(TTR("%s: Callback code won't be generated, please add it manually."), language->get_name()));
	warning_label->set_visible(true);
}

void ConnectDialog::_post_popup() {
	callable_mp((Control *)dst_method, &Control::grab_focus).call_deferred();
	callable_mp(dst_method, &LineEdit::select_all).call_deferred();
}

void ConnectDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			bind_editor->edit(cdbinds);

			[[fallthrough]];
		}
		case NOTIFICATION_THEME_CHANGED: {
			for (int i = 0; i < type_list->get_item_count(); i++) {
				String type_name = Variant::get_type_name((Variant::Type)type_list->get_item_id(i));
				type_list->set_item_icon(i, get_editor_theme_icon(type_name));
			}

			method_search->set_right_icon(get_editor_theme_icon("Search"));
			open_method_tree->set_button_icon(get_editor_theme_icon("Edit"));
		} break;
	}
}

void ConnectDialog::_bind_methods() {
	ADD_SIGNAL(MethodInfo("connected"));
}

Node *ConnectDialog::get_source() const {
	return source;
}

ConnectDialog::ConnectionData ConnectDialog::get_source_connection_data() const {
	return source_connection_data;
}

StringName ConnectDialog::get_signal_name() const {
	return signal;
}

PackedStringArray ConnectDialog::get_signal_args() const {
	return signal_args;
}

NodePath ConnectDialog::get_dst_path() const {
	return dst_path;
}

void ConnectDialog::set_dst_node(Node *p_node) {
	tree->set_selected(p_node);
}

StringName ConnectDialog::get_dst_method_name() const {
	String txt = dst_method->get_text();
	if (txt.contains_char('(')) {
		txt = txt.left(txt.find_char('(')).strip_edges();
	}
	return txt;
}

void ConnectDialog::set_dst_method(const StringName &p_method) {
	dst_method->set_text(p_method);
}

int ConnectDialog::get_unbinds() const {
	return int(unbind_count->get_value());
}

Vector<Variant> ConnectDialog::get_binds() const {
	return cdbinds->params;
}

String ConnectDialog::get_signature(const MethodInfo &p_method, PackedStringArray *r_arg_names) {
	PackedStringArray signature;
	signature.append(p_method.name);
	signature.append("(");

	int i = 0;
	for (List<PropertyInfo>::ConstIterator itr = p_method.arguments.begin(); itr != p_method.arguments.end(); ++itr, ++i) {
		if (itr != p_method.arguments.begin()) {
			signature.append(", ");
		}

		const PropertyInfo &pi = *itr;
		String type_name;
		switch (pi.type) {
			case Variant::NIL:
				type_name = "Variant";
				break;
			case Variant::INT:
				if ((pi.usage & PROPERTY_USAGE_CLASS_IS_ENUM) && pi.class_name != StringName() && !String(pi.class_name).begins_with("res://")) {
					type_name = pi.class_name;
				} else {
					type_name = "int";
				}
				break;
			case Variant::ARRAY:
				if (pi.hint == PROPERTY_HINT_ARRAY_TYPE && !pi.hint_string.is_empty() && !pi.hint_string.begins_with("res://")) {
					type_name = "Array[" + pi.hint_string + "]";
				} else {
					type_name = "Array";
				}
				break;
			case Variant::DICTIONARY:
				type_name = "Dictionary";
				if (pi.hint == PROPERTY_HINT_DICTIONARY_TYPE && !pi.hint_string.is_empty()) {
					String key_hint = pi.hint_string.get_slice(";", 0);
					String value_hint = pi.hint_string.get_slice(";", 1);
					if (key_hint.is_empty() || key_hint.begins_with("res://")) {
						key_hint = "Variant";
					}
					if (value_hint.is_empty() || value_hint.begins_with("res://")) {
						value_hint = "Variant";
					}
					if (key_hint != "Variant" || value_hint != "Variant") {
						type_name += "[" + key_hint + ", " + value_hint + "]";
					}
				}
				break;
			case Variant::OBJECT:
				if (pi.class_name != StringName()) {
					type_name = pi.class_name;
				} else {
					type_name = "Object";
				}
				break;
			default:
				type_name = Variant::get_type_name(pi.type);
				break;
		}

		String arg_name = pi.name.is_empty() ? "arg" + itos(i) : pi.name;
		signature.append(arg_name + ": " + type_name);
		if (r_arg_names) {
			r_arg_names->push_back(arg_name + ":" + type_name);
		}
	}

	signature.append(")");
	return String().join(signature);
}

bool ConnectDialog::get_deferred() const {
	return deferred->is_pressed();
}

bool ConnectDialog::get_one_shot() const {
	return one_shot->is_pressed();
}

/*
 * Returns true if ConnectDialog is being used to edit an existing connection.
 */
bool ConnectDialog::is_editing() const {
	return edit_mode;
}

void ConnectDialog::shortcut_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventKey> &key = p_event;

	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (ED_IS_SHORTCUT("editor/open_search", p_event)) {
			filter_nodes->grab_focus();
			filter_nodes->select_all();
			filter_nodes->accept_event();
		}
	}
}

/*
 * Initialize ConnectDialog and populate fields with expected data.
 * If creating a connection from scratch, sensible defaults are used.
 * If editing an existing connection, previous data is retained.
 */
void ConnectDialog::init(const ConnectionData &p_cd, const PackedStringArray &p_signal_args, bool p_edit) {
	set_hide_on_ok(false);

	source = static_cast<Node *>(p_cd.source);
	signal = p_cd.signal;
	signal_args = p_signal_args;

	tree->set_selected(nullptr);
	tree->set_marked(source);

	if (p_cd.target) {
		set_dst_node(static_cast<Node *>(p_cd.target));
		set_dst_method(p_cd.method);
	}

	_update_ok_enabled();

	bool b_deferred = (p_cd.flags & CONNECT_DEFERRED) == CONNECT_DEFERRED;
	bool b_oneshot = (p_cd.flags & CONNECT_ONE_SHOT) == CONNECT_ONE_SHOT;

	deferred->set_pressed(b_deferred);
	one_shot->set_pressed(b_oneshot);

	unbind_count->set_max(p_signal_args.size());

	unbind_count->set_value(p_cd.unbinds);
	_unbind_count_changed(p_cd.unbinds);

	cdbinds->params.clear();
	cdbinds->params = p_cd.binds;
	cdbinds->notify_changed();

	edit_mode = p_edit;

	source_connection_data = p_cd;
}

void ConnectDialog::popup_dialog(const String &p_for_signal) {
	from_signal->set_text(p_for_signal);
	warning_label->add_theme_color_override(SceneStringName(font_color), warning_label->get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
	error_label->add_theme_color_override(SceneStringName(font_color), error_label->get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
	filter_nodes->clear();

	if (!advanced->is_pressed()) {
		error_label->set_visible(!_find_first_script(get_tree()->get_edited_scene_root(), get_tree()->get_edited_scene_root()));
	}

	if (first_popup) {
		first_popup = false;
		_advanced_pressed();
	}

	popup_centered();
}

void ConnectDialog::_advanced_pressed() {
	if (advanced->is_pressed()) {
		connect_to_label->set_text(TTR("Connect to Node:"));
		tree->set_connect_to_script_mode(false);

		vbc_right->show();
		error_label->hide();
	} else {
		reset_size();
		connect_to_label->set_text(TTR("Connect to Script:"));
		tree->set_connect_to_script_mode(true);

		vbc_right->hide();
		error_label->set_visible(!_find_first_script(get_tree()->get_edited_scene_root(), get_tree()->get_edited_scene_root()));
	}

	EditorSettings::get_singleton()->set_project_metadata("editor_metadata", "use_advanced_connections", advanced->is_pressed());

	popup_centered();
}

ConnectDialog::ConnectDialog() {
	set_min_size(Size2(0, 500) * EDSCALE);

	HBoxContainer *main_hb = memnew(HBoxContainer);
	add_child(main_hb);

	VBoxContainer *vbc_left = memnew(VBoxContainer);
	main_hb->add_child(vbc_left);
	vbc_left->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbc_left->set_custom_minimum_size(Vector2(400 * EDSCALE, 0));

	from_signal = memnew(LineEdit);
	vbc_left->add_margin_child(TTR("From Signal:"), from_signal);
	from_signal->set_editable(false);

	tree = memnew(SceneTreeEditor(false));
	tree->set_update_when_invisible(false);
	tree->set_connecting_signal(true);
	tree->set_show_enabled_subscene(true);
	tree->set_v_size_flags(Control::SIZE_FILL | Control::SIZE_EXPAND);
	tree->get_scene_tree()->connect("item_activated", callable_mp(this, &ConnectDialog::_item_activated));
	tree->connect("node_selected", callable_mp(this, &ConnectDialog::_tree_node_selected));
	tree->set_connect_to_script_mode(true);

	HBoxContainer *hbc_filter = memnew(HBoxContainer);

	filter_nodes = memnew(LineEdit);
	hbc_filter->add_child(filter_nodes);
	filter_nodes->set_h_size_flags(Control::SIZE_FILL | Control::SIZE_EXPAND);
	filter_nodes->set_placeholder(TTR("Filter Nodes"));
	filter_nodes->set_clear_button_enabled(true);
	filter_nodes->connect(SceneStringName(text_changed), callable_mp(tree, &SceneTreeEditor::set_filter));

	Button *focus_current = memnew(Button);
	hbc_filter->add_child(focus_current);
	focus_current->set_text(TTR("Go to Source"));
	focus_current->connect(SceneStringName(pressed), callable_mp(this, &ConnectDialog::_focus_currently_connected));

	Node *mc = vbc_left->add_margin_child(TTR("Connect to Script:"), hbc_filter, false);
	connect_to_label = Object::cast_to<Label>(vbc_left->get_child(mc->get_index() - 1));
	vbc_left->add_child(tree);

	warning_label = memnew(Label);
	vbc_left->add_child(warning_label);
	warning_label->hide();

	error_label = memnew(Label);
	error_label->set_text(TTR("Scene does not contain any script."));
	vbc_left->add_child(error_label);
	error_label->hide();

	method_popup = memnew(AcceptDialog);
	method_popup->set_title(TTR("Select Method"));
	method_popup->set_min_size(Vector2(400, 600) * EDSCALE);
	add_child(method_popup);

	VBoxContainer *method_vbc = memnew(VBoxContainer);
	method_popup->add_child(method_vbc);

	method_search = memnew(LineEdit);
	method_vbc->add_child(method_search);
	method_search->set_placeholder(TTR("Filter Methods"));
	method_search->set_clear_button_enabled(true);
	method_search->connect(SceneStringName(text_changed), callable_mp(this, &ConnectDialog::_update_method_tree).unbind(1));

	method_tree = memnew(Tree);
	method_vbc->add_child(method_tree);
	method_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	method_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	method_tree->set_hide_root(true);
	method_tree->connect(SceneStringName(item_selected), callable_mp(this, &ConnectDialog::_method_selected));
	method_tree->connect("item_activated", callable_mp((Window *)method_popup, &Window::hide));

	empty_tree_label = memnew(Label(TTR("No method found matching given filters.")));
	method_popup->add_child(empty_tree_label);
	empty_tree_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	empty_tree_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	empty_tree_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);

	script_methods_only = memnew(CheckButton(TTR("Script Methods Only")));
	method_vbc->add_child(script_methods_only);
	script_methods_only->set_h_size_flags(Control::SIZE_SHRINK_END);
	script_methods_only->set_pressed(EditorSettings::get_singleton()->get_project_metadata("editor_metadata", "show_script_methods_only", true));
	script_methods_only->connect(SceneStringName(pressed), callable_mp(this, &ConnectDialog::_method_check_button_pressed).bind(script_methods_only));

	compatible_methods_only = memnew(CheckButton(TTR("Compatible Methods Only")));
	method_vbc->add_child(compatible_methods_only);
	compatible_methods_only->set_h_size_flags(Control::SIZE_SHRINK_END);
	compatible_methods_only->set_pressed(EditorSettings::get_singleton()->get_project_metadata("editor_metadata", "show_compatible_methods_only", true));
	compatible_methods_only->connect(SceneStringName(pressed), callable_mp(this, &ConnectDialog::_method_check_button_pressed).bind(compatible_methods_only));

	vbc_right = memnew(VBoxContainer);
	main_hb->add_child(vbc_right);
	vbc_right->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbc_right->set_custom_minimum_size(Vector2(150 * EDSCALE, 0));
	vbc_right->hide();

	HBoxContainer *add_bind_hb = memnew(HBoxContainer);

	type_list = memnew(OptionButton);
	type_list->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_bind_hb->add_child(type_list);
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		if (i == Variant::NIL || i == Variant::OBJECT || i == Variant::CALLABLE || i == Variant::SIGNAL || i == Variant::RID) {
			// These types can't be constructed or serialized properly, so skip them.
			continue;
		}

		type_list->add_item(Variant::get_type_name(Variant::Type(i)), i);
	}
	bind_controls.push_back(type_list);

	Button *add_bind = memnew(Button);
	add_bind->set_text(TTR("Add"));
	add_bind_hb->add_child(add_bind);
	add_bind->connect(SceneStringName(pressed), callable_mp(this, &ConnectDialog::_add_bind));
	bind_controls.push_back(add_bind);

	Button *del_bind = memnew(Button);
	del_bind->set_text(TTR("Remove"));
	add_bind_hb->add_child(del_bind);
	del_bind->connect(SceneStringName(pressed), callable_mp(this, &ConnectDialog::_remove_bind));
	bind_controls.push_back(del_bind);

	vbc_right->add_margin_child(TTR("Add Extra Call Argument:"), add_bind_hb);

	bind_editor = memnew(EditorInspector);
	bind_controls.push_back(bind_editor);

	vbc_right->add_margin_child(TTR("Extra Call Arguments:"), bind_editor, true);

	unbind_count = memnew(SpinBox);
	unbind_count->set_tooltip_text(TTR("Allows to drop arguments sent by signal emitter."));
	unbind_count->connect(SceneStringName(value_changed), callable_mp(this, &ConnectDialog::_unbind_count_changed));

	vbc_right->add_margin_child(TTR("Unbind Signal Arguments:"), unbind_count);

	HBoxContainer *hbc_method = memnew(HBoxContainer);
	vbc_left->add_margin_child(TTR("Receiver Method:"), hbc_method);

	dst_method = memnew(LineEdit);
	dst_method->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dst_method->connect(SceneStringName(text_changed), callable_mp(method_tree, &Tree::deselect_all).unbind(1));
	hbc_method->add_child(dst_method);
	register_text_enter(dst_method);

	open_method_tree = memnew(Button);
	hbc_method->add_child(open_method_tree);
	open_method_tree->set_text("Pick");
	open_method_tree->connect(SceneStringName(pressed), callable_mp(this, &ConnectDialog::_open_method_popup));

	advanced = memnew(CheckButton(TTR("Advanced")));
	vbc_left->add_child(advanced);
	advanced->set_h_size_flags(Control::SIZE_SHRINK_BEGIN | Control::SIZE_EXPAND);
	advanced->set_pressed(EditorSettings::get_singleton()->get_project_metadata("editor_metadata", "use_advanced_connections", false));
	advanced->connect(SceneStringName(pressed), callable_mp(this, &ConnectDialog::_advanced_pressed));

	HBoxContainer *hbox = memnew(HBoxContainer);
	vbc_right->add_child(hbox);

	deferred = memnew(CheckBox);
	deferred->set_h_size_flags(0);
	deferred->set_text(TTR("Deferred"));
	deferred->set_tooltip_text(TTR("Defers the signal, storing it in a queue and only firing it at idle time."));
	hbox->add_child(deferred);

	one_shot = memnew(CheckBox);
	one_shot->set_h_size_flags(0);
	one_shot->set_text(TTR("One Shot"));
	one_shot->set_tooltip_text(TTR("Disconnects the signal after its first emission."));
	hbox->add_child(one_shot);

	cdbinds = memnew(ConnectDialogBinds);

	error = memnew(AcceptDialog);
	add_child(error);
	error->set_title(TTR("Cannot connect signal"));
	error->set_ok_button_text(TTR("Close"));
	set_ok_button_text(TTR("Connect"));
}

ConnectDialog::~ConnectDialog() {
	memdelete(cdbinds);
}

//////////////////////////////////////////

Control *ConnectionsDockTree::make_custom_tooltip(const String &p_text) const {
	// If it's not a doc tooltip, fallback to the default one.
	if (p_text.is_empty() || p_text.contains("::")) {
		return nullptr;
	}

	return EditorHelpBitTooltip::show_tooltip(const_cast<ConnectionsDockTree *>(this), p_text);
}

struct _ConnectionsDockMethodInfoSort {
	_FORCE_INLINE_ bool operator()(const MethodInfo &a, const MethodInfo &b) const {
		return a.name < b.name;
	}
};

void ConnectionsDock::_filter_changed(const String &p_text) {
	update_tree();
}

/*
 * Post-ConnectDialog callback for creating/editing connections.
 * Creates or edits connections based on state of the ConnectDialog when "Connect" is pressed.
 */
void ConnectionsDock::_make_or_edit_connection() {
	NodePath dst_path = connect_dialog->get_dst_path();
	Node *target = selected_node->get_node(dst_path);
	ERR_FAIL_NULL(target);

	ConnectDialog::ConnectionData cd;
	cd.source = connect_dialog->get_source();
	cd.target = target;
	cd.signal = connect_dialog->get_signal_name();
	cd.method = connect_dialog->get_dst_method_name();
	cd.unbinds = connect_dialog->get_unbinds();
	if (cd.unbinds == 0) {
		cd.binds = connect_dialog->get_binds();
	}
	bool b_deferred = connect_dialog->get_deferred();
	bool b_oneshot = connect_dialog->get_one_shot();
	cd.flags = CONNECT_PERSIST | (b_deferred ? CONNECT_DEFERRED : 0) | (b_oneshot ? CONNECT_ONE_SHOT : 0);

	// If the function is found in target's own script, check the editor setting
	// to determine if the script should be opened.
	// If the function is found in an inherited class or script no need to do anything
	// except making a connection.
	bool add_script_function_request = false;
	Ref<Script> scr = target->get_script();

	if (scr.is_valid() && !ClassDB::has_method(target->get_class(), cd.method)) {
		// Check in target's own script.
		int line = scr->get_language()->find_function(cd.method, scr->get_source_code());
		if (line != -1) {
			add_script_function_request = EDITOR_GET("text_editor/behavior/navigation/open_script_when_connecting_signal_to_existing_method");
		} else {
			// There is a chance that the method is inherited from another script.
			bool found_inherited_function = false;
			Ref<Script> inherited_scr = scr->get_base_script();
			while (inherited_scr.is_valid()) {
				int inherited_line = inherited_scr->get_language()->find_function(cd.method, inherited_scr->get_source_code());
				if (inherited_line != -1) {
					found_inherited_function = true;
					break;
				}

				inherited_scr = inherited_scr->get_base_script();
			}

			add_script_function_request = !found_inherited_function;
		}
	}

	if (connect_dialog->is_editing()) {
		_disconnect(connect_dialog->get_source_connection_data());
		_connect(cd);
	} else {
		_connect(cd);
	}

	if (add_script_function_request) {
		PackedStringArray script_function_args = connect_dialog->get_signal_args();
		script_function_args.resize(script_function_args.size() - cd.unbinds);
		for (int i = 0; i < cd.binds.size(); i++) {
			script_function_args.push_back("extra_arg_" + itos(i) + ":" + Variant::get_type_name(cd.binds[i].get_type()));
		}

		EditorNode::get_singleton()->emit_signal(SNAME("script_add_function_request"), target, cd.method, script_function_args);
	}

	update_tree();
}

/*
 * Creates single connection w/ undo-redo functionality.
 */
void ConnectionsDock::_connect(const ConnectDialog::ConnectionData &p_cd) {
	Node *source = Object::cast_to<Node>(p_cd.source);
	Node *target = Object::cast_to<Node>(p_cd.target);

	if (!source || !target) {
		return;
	}

	Callable callable = p_cd.get_callable();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Connect '%s' to '%s'"), String(p_cd.signal), String(p_cd.method)));
	undo_redo->add_do_method(source, "connect", p_cd.signal, callable, p_cd.flags);
	undo_redo->add_undo_method(source, "disconnect", p_cd.signal, callable);
	undo_redo->add_do_method(this, "update_tree");
	undo_redo->add_undo_method(this, "update_tree");
	undo_redo->add_do_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree"); // To force redraw of scene tree.
	undo_redo->add_undo_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree");

	undo_redo->commit_action();
}

/*
 * Break single connection w/ undo-redo functionality.
 */
void ConnectionsDock::_disconnect(const ConnectDialog::ConnectionData &p_cd) {
	ERR_FAIL_COND(p_cd.source != selected_node); // Shouldn't happen but... Bugcheck.

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Disconnect '%s' from '%s'"), p_cd.signal, p_cd.method));

	Callable callable = p_cd.get_callable();
	undo_redo->add_do_method(selected_node, "disconnect", p_cd.signal, callable);
	undo_redo->add_undo_method(selected_node, "connect", p_cd.signal, callable, p_cd.flags);
	undo_redo->add_do_method(this, "update_tree");
	undo_redo->add_undo_method(this, "update_tree");
	undo_redo->add_do_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree"); // To force redraw of scene tree.
	undo_redo->add_undo_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree");

	undo_redo->commit_action();
}

/*
 * Break all connections of currently selected signal.
 * Can undo-redo as a single action.
 */
void ConnectionsDock::_disconnect_all() {
	TreeItem *item = tree->get_selected();
	if (!item || _get_item_type(*item) != TREE_ITEM_TYPE_SIGNAL) {
		return;
	}

	TreeItem *child = item->get_first_child();
	String signal_name = item->get_metadata(0).operator Dictionary()["name"];
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Disconnect all from signal: '%s'"), signal_name));

	while (child) {
		Connection connection = child->get_metadata(0);
		if (!_is_connection_inherited(connection)) {
			ConnectDialog::ConnectionData cd = connection;
			undo_redo->add_do_method(selected_node, "disconnect", cd.signal, cd.get_callable());
			undo_redo->add_undo_method(selected_node, "connect", cd.signal, cd.get_callable(), cd.flags);
		}
		child = child->get_next();
	}

	undo_redo->add_do_method(this, "update_tree");
	undo_redo->add_undo_method(this, "update_tree");
	undo_redo->add_do_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree");
	undo_redo->add_undo_method(SceneTreeDock::get_singleton()->get_tree_editor(), "update_tree");

	undo_redo->commit_action();
}

void ConnectionsDock::_tree_item_selected() {
	TreeItem *item = tree->get_selected();
	if (item && _get_item_type(*item) == TREE_ITEM_TYPE_SIGNAL) {
		connect_button->set_text(TTR("Connect..."));
		connect_button->set_button_icon(get_editor_theme_icon(SNAME("Instance")));
		connect_button->set_disabled(false);
	} else if (item && _get_item_type(*item) == TREE_ITEM_TYPE_CONNECTION) {
		connect_button->set_text(TTR("Disconnect"));
		connect_button->set_button_icon(get_editor_theme_icon(SNAME("Unlinked")));

		Object::Connection connection = item->get_metadata(0);
		connect_button->set_disabled(_is_connection_inherited(connection));
	} else {
		connect_button->set_text(TTR("Connect..."));
		connect_button->set_button_icon(get_editor_theme_icon(SNAME("Instance")));
		connect_button->set_disabled(true);
	}
}

void ConnectionsDock::_tree_item_activated() { // "Activation" on double-click.
	TreeItem *item = tree->get_selected();
	if (!item) {
		return;
	}

	if (_get_item_type(*item) == TREE_ITEM_TYPE_SIGNAL) {
		_open_connection_dialog(*item);
	} else if (_get_item_type(*item) == TREE_ITEM_TYPE_CONNECTION) {
		_go_to_method(*item);
	}
}

ConnectionsDock::TreeItemType ConnectionsDock::_get_item_type(const TreeItem &p_item) const {
	if (&p_item == tree->get_root()) {
		return TREE_ITEM_TYPE_ROOT;
	} else if (p_item.get_parent() == tree->get_root()) {
		return TREE_ITEM_TYPE_CLASS;
	} else if (p_item.get_parent()->get_parent() == tree->get_root()) {
		return TREE_ITEM_TYPE_SIGNAL;
	} else {
		return TREE_ITEM_TYPE_CONNECTION;
	}
}

bool ConnectionsDock::_is_connection_inherited(Connection &p_connection) {
	return bool(p_connection.flags & CONNECT_INHERITED);
}

/*
 * Open connection dialog with TreeItem data to CREATE a brand-new connection.
 */
void ConnectionsDock::_open_connection_dialog(TreeItem &p_item) {
	Dictionary sinfo = p_item.get_metadata(0);
	String signal_name = sinfo["name"];
	PackedStringArray signal_args = sinfo["args"];

	Node *dst_node = selected_node->get_owner() ? selected_node->get_owner() : selected_node;
	if (!dst_node || dst_node->get_script().is_null()) {
		dst_node = _find_first_script(get_tree()->get_edited_scene_root(), get_tree()->get_edited_scene_root());
	}

	ConnectDialog::ConnectionData cd;
	cd.source = selected_node;
	cd.signal = StringName(signal_name);
	cd.target = dst_node;
	cd.method = ConnectDialog::generate_method_callback_name(cd.source, signal_name, cd.target);
	connect_dialog->init(cd, signal_args);
	connect_dialog->set_title(TTR("Connect a Signal to a Method"));
	connect_dialog->popup_dialog(signal_name + "(" + String(", ").join(signal_args) + ")");
}

/*
 * Open connection dialog with Connection data to EDIT an existing connection.
 */
void ConnectionsDock::_open_edit_connection_dialog(TreeItem &p_item) {
	TreeItem *signal_item = p_item.get_parent();
	ERR_FAIL_NULL(signal_item);

	Connection connection = p_item.get_metadata(0);
	ConnectDialog::ConnectionData cd = connection;

	Node *src = Object::cast_to<Node>(cd.source);
	Node *dst = Object::cast_to<Node>(cd.target);

	if (src && dst) {
		const String &signal_name_ref = cd.signal;
		PackedStringArray signal_args = signal_item->get_metadata(0).operator Dictionary()["args"];

		connect_dialog->set_title(vformat(TTR("Edit Connection: '%s'"), cd.signal));
		connect_dialog->popup_dialog(signal_name_ref);
		connect_dialog->init(cd, signal_args, true);
	}
}

/*
 * Open slot method location in script editor.
 */
void ConnectionsDock::_go_to_method(TreeItem &p_item) {
	if (_get_item_type(p_item) != TREE_ITEM_TYPE_CONNECTION) {
		return;
	}

	Connection connection = p_item.get_metadata(0);
	ConnectDialog::ConnectionData cd = connection;
	ERR_FAIL_COND(cd.source != selected_node); // Shouldn't happen but... bugcheck.

	if (!cd.target) {
		return;
	}

	Ref<Script> scr = cd.target->get_script();

	if (scr.is_null()) {
		return;
	}

	if (scr.is_valid() && ScriptEditor::get_singleton()->script_goto_method(scr, cd.method)) {
		EditorNode::get_editor_main_screen()->select(EditorMainScreen::EDITOR_SCRIPT);
	}
}

void ConnectionsDock::_handle_class_menu_option(int p_option) {
	switch (p_option) {
		case CLASS_MENU_OPEN_DOCS:
			ScriptEditor::get_singleton()->goto_help("class:" + class_menu_doc_class_name);
			EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_SCRIPT);
			break;
	}
}

void ConnectionsDock::_class_menu_about_to_popup() {
	class_menu->set_item_disabled(class_menu->get_item_index(CLASS_MENU_OPEN_DOCS), class_menu_doc_class_name.is_empty());
}

void ConnectionsDock::_handle_signal_menu_option(int p_option) {
	TreeItem *item = tree->get_selected();
	if (!item || _get_item_type(*item) != TREE_ITEM_TYPE_SIGNAL) {
		return;
	}

	Dictionary meta = item->get_metadata(0);

	switch (p_option) {
		case SIGNAL_MENU_CONNECT: {
			_open_connection_dialog(*item);
		} break;
		case SIGNAL_MENU_DISCONNECT_ALL: {
			disconnect_all_dialog->set_text(vformat(TTR("Are you sure you want to remove all connections from the \"%s\" signal?"), meta["name"]));
			disconnect_all_dialog->popup_centered();
		} break;
		case SIGNAL_MENU_COPY_NAME: {
			DisplayServer::get_singleton()->clipboard_set(meta["name"]);
		} break;
		case SIGNAL_MENU_OPEN_DOCS: {
			ScriptEditor::get_singleton()->goto_help("class_signal:" + String(meta["class"]) + ":" + String(meta["name"]));
			EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_SCRIPT);
		} break;
	}
}

void ConnectionsDock::_signal_menu_about_to_popup() {
	TreeItem *item = tree->get_selected();
	if (!item || _get_item_type(*item) != TREE_ITEM_TYPE_SIGNAL) {
		return;
	}

	Dictionary meta = item->get_metadata(0);

	bool disable_disconnect_all = true;
	for (int i = 0; i < item->get_child_count(); i++) {
		if (!item->get_child(i)->has_meta("_inherited_connection")) {
			disable_disconnect_all = false;
		}
	}

	signal_menu->set_item_disabled(signal_menu->get_item_index(SIGNAL_MENU_DISCONNECT_ALL), disable_disconnect_all);
	signal_menu->set_item_disabled(signal_menu->get_item_index(SIGNAL_MENU_OPEN_DOCS), String(meta["class"]).is_empty());
}

void ConnectionsDock::_handle_slot_menu_option(int p_option) {
	TreeItem *item = tree->get_selected();
	if (!item || _get_item_type(*item) != TREE_ITEM_TYPE_CONNECTION) {
		return;
	}

	switch (p_option) {
		case SLOT_MENU_EDIT: {
			_open_edit_connection_dialog(*item);
		} break;
		case SLOT_MENU_GO_TO_METHOD: {
			_go_to_method(*item);
		} break;
		case SLOT_MENU_DISCONNECT: {
			Connection connection = item->get_metadata(0);
			_disconnect(connection);
			update_tree();
		} break;
	}
}

void ConnectionsDock::_slot_menu_about_to_popup() {
	TreeItem *item = tree->get_selected();
	if (!item || _get_item_type(*item) != TREE_ITEM_TYPE_CONNECTION) {
		return;
	}

	bool connection_is_inherited = item->has_meta("_inherited_connection");

	slot_menu->set_item_disabled(slot_menu->get_item_index(SLOT_MENU_EDIT), connection_is_inherited);
	slot_menu->set_item_disabled(slot_menu->get_item_index(SLOT_MENU_DISCONNECT), connection_is_inherited);
}

void ConnectionsDock::_tree_gui_input(const Ref<InputEvent> &p_event) {
	const Ref<InputEventKey> &key = p_event;

	if (key.is_valid() && key->is_pressed() && !key->is_echo()) {
		if (ED_IS_SHORTCUT("connections_editor/disconnect", p_event)) {
			TreeItem *item = tree->get_selected();
			if (item && _get_item_type(*item) == TREE_ITEM_TYPE_CONNECTION) {
				Connection connection = item->get_metadata(0);
				_disconnect(connection);
				update_tree();

				// Stop the Delete input from propagating elsewhere.
				accept_event();
				return;
			}
		} else if (ED_IS_SHORTCUT("editor/open_search", p_event)) {
			search_box->grab_focus();
			search_box->select_all();

			accept_event();
			return;
		}
	}

	// Handle RMB press.
	const Ref<InputEventMouseButton> &mb_event = p_event;

	if (mb_event.is_valid() && mb_event->is_pressed() && mb_event->get_button_index() == MouseButton::RIGHT) {
		TreeItem *item = tree->get_item_at_position(mb_event->get_position());
		if (!item) {
			return;
		}

		if (item->is_selectable(0)) {
			// Update selection now, before `about_to_popup` signal. Needed for SIGNAL and CONNECTION context menus.
			tree->set_selected(item);
		}

		Vector2 screen_position = tree->get_screen_position() + mb_event->get_position();

		switch (_get_item_type(*item)) {
			case TREE_ITEM_TYPE_ROOT:
				break;
			case TREE_ITEM_TYPE_CLASS:
				class_menu_doc_class_name = item->get_metadata(0);
				class_menu->set_position(screen_position);
				class_menu->reset_size();
				class_menu->popup();
				accept_event(); // Don't collapse item.
				break;
			case TREE_ITEM_TYPE_SIGNAL:
				signal_menu->set_position(screen_position);
				signal_menu->reset_size();
				signal_menu->popup();
				break;
			case TREE_ITEM_TYPE_CONNECTION:
				slot_menu->set_position(screen_position);
				slot_menu->reset_size();
				slot_menu->popup();
				break;
		}
	}
}

void ConnectionsDock::_close() {
	hide();
}

void ConnectionsDock::_connect_pressed() {
	TreeItem *item = tree->get_selected();
	if (!item) {
		connect_button->set_disabled(true);
		return;
	}

	if (_get_item_type(*item) == TREE_ITEM_TYPE_SIGNAL) {
		_open_connection_dialog(*item);
	} else if (_get_item_type(*item) == TREE_ITEM_TYPE_CONNECTION) {
		Connection connection = item->get_metadata(0);
		_disconnect(connection);
		update_tree();
	}
}

void ConnectionsDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			search_box->set_right_icon(get_editor_theme_icon(SNAME("Search")));

			class_menu->set_item_icon(class_menu->get_item_index(CLASS_MENU_OPEN_DOCS), get_editor_theme_icon(SNAME("Help")));

			signal_menu->set_item_icon(signal_menu->get_item_index(SIGNAL_MENU_CONNECT), get_editor_theme_icon(SNAME("Instance")));
			signal_menu->set_item_icon(signal_menu->get_item_index(SIGNAL_MENU_DISCONNECT_ALL), get_editor_theme_icon(SNAME("Unlinked")));
			signal_menu->set_item_icon(signal_menu->get_item_index(SIGNAL_MENU_COPY_NAME), get_editor_theme_icon(SNAME("ActionCopy")));
			signal_menu->set_item_icon(signal_menu->get_item_index(SIGNAL_MENU_OPEN_DOCS), get_editor_theme_icon(SNAME("Help")));

			slot_menu->set_item_icon(slot_menu->get_item_index(SLOT_MENU_EDIT), get_editor_theme_icon(SNAME("Edit")));
			slot_menu->set_item_icon(slot_menu->get_item_index(SLOT_MENU_GO_TO_METHOD), get_editor_theme_icon(SNAME("ArrowRight")));
			slot_menu->set_item_icon(slot_menu->get_item_index(SLOT_MENU_DISCONNECT), get_editor_theme_icon(SNAME("Unlinked")));

			tree->add_theme_constant_override("icon_max_width", get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor)));

			update_tree();
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editors")) {
				update_tree();
			}
		} break;
	}
}

void ConnectionsDock::_bind_methods() {
	ClassDB::bind_method("update_tree", &ConnectionsDock::update_tree);
}

void ConnectionsDock::set_node(Node *p_node) {
	selected_node = p_node;
	update_tree();
}

void ConnectionsDock::update_tree() {
	String prev_selected;
	if (tree->is_anything_selected()) {
		prev_selected = tree->get_selected()->get_text(0);
	}
	tree->clear();

	if (!selected_node) {
		return;
	}

	TreeItem *root = tree->create_item();
	DocTools *doc_data = EditorHelp::get_doc_data();
	EditorData &editor_data = EditorNode::get_editor_data();
	StringName native_base = selected_node->get_class();
	Ref<Script> script_base = selected_node->get_script();

	while (native_base != StringName()) {
		String class_name;
		String doc_class_name;
		Ref<Texture2D> class_icon;
		List<MethodInfo> class_signals;

		if (script_base.is_valid()) {
			class_name = script_base->get_global_name();
			if (class_name.is_empty()) {
				class_name = script_base->get_path().get_file();
			}

			doc_class_name = script_base->get_global_name();
			if (doc_class_name.is_empty()) {
				doc_class_name = script_base->get_path().trim_prefix("res://").quote();
			}
			if (!doc_class_name.is_empty() && !doc_data->class_list.find(doc_class_name)) {
				doc_class_name = String();
			}

			class_icon = editor_data.get_script_icon(script_base);
			if (class_icon.is_null() && has_theme_icon(native_base, EditorStringName(EditorIcons))) {
				class_icon = get_editor_theme_icon(native_base);
			}

			script_base->get_script_signal_list(&class_signals);

			// TODO: Core: Add optional parameter to ignore base classes (no_inheritance like in ClassDB).
			Ref<Script> base = script_base->get_base_script();
			if (base.is_valid()) {
				List<MethodInfo> base_signals;
				base->get_script_signal_list(&base_signals);
				HashSet<String> base_signal_names;
				for (List<MethodInfo>::Element *F = base_signals.front(); F; F = F->next()) {
					base_signal_names.insert(F->get().name);
				}
				for (List<MethodInfo>::Element *F = class_signals.front(); F; F = F->next()) {
					if (base_signal_names.has(F->get().name)) {
						class_signals.erase(F);
					}
				}
			}

			script_base = base;
		} else {
			class_name = native_base;
			doc_class_name = native_base;

			if (!doc_data->class_list.find(doc_class_name)) {
				doc_class_name = String();
			}

			if (has_theme_icon(native_base, EditorStringName(EditorIcons))) {
				class_icon = get_editor_theme_icon(native_base);
			}

			ClassDB::get_signal_list(native_base, &class_signals, true);

			native_base = ClassDB::get_parent_class(native_base);
		}

		if (class_icon.is_null()) {
			class_icon = get_editor_theme_icon(SNAME("Object"));
		}

		TreeItem *section_item = nullptr;

		// Create subsections.
		if (!class_signals.is_empty()) {
			class_signals.sort();

			section_item = tree->create_item(root);
			section_item->set_text(0, class_name);
			// `|` separators used in `EditorHelpBit`.
			section_item->set_tooltip_text(0, "class|" + doc_class_name + "|");
			section_item->set_icon(0, class_icon);
			section_item->set_selectable(0, false);
			section_item->set_editable(0, false);
			section_item->set_custom_bg_color(0, get_theme_color(SNAME("prop_subsection"), EditorStringName(Editor)));
			section_item->set_metadata(0, doc_class_name);
		}

		for (MethodInfo &mi : class_signals) {
			const StringName &signal_name = mi.name;
			if (!search_box->get_text().is_subsequence_ofn(signal_name)) {
				continue;
			}
			PackedStringArray argnames;

			// Create the children of the subsection - the actual list of signals.
			TreeItem *signal_item = tree->create_item(section_item);
			String signame = connect_dialog->get_signature(mi, &argnames);
			signal_item->set_text(0, signame);

			if (signame == prev_selected) {
				signal_item->select(0);
				prev_selected = "";
			}

			Dictionary sinfo;
			sinfo["class"] = doc_class_name;
			sinfo["name"] = signal_name;
			sinfo["args"] = argnames;
			signal_item->set_metadata(0, sinfo);
			signal_item->set_icon(0, get_editor_theme_icon(SNAME("Signal")));
			// `|` separators used in `EditorHelpBit`.
			signal_item->set_tooltip_text(0, "signal|" + doc_class_name + "|" + String(signal_name));

			// List existing connections.
			List<Object::Connection> existing_connections;
			selected_node->get_signal_connection_list(signal_name, &existing_connections);

			for (const Object::Connection &F : existing_connections) {
				Connection connection = F;
				if (!(connection.flags & CONNECT_PERSIST)) {
					continue;
				}
				ConnectDialog::ConnectionData cd = connection;

				Node *target = Object::cast_to<Node>(cd.target);
				if (!target) {
					continue;
				}

				String path = String(selected_node->get_path_to(target)) + " :: " + cd.method + "()";
				if (cd.flags & CONNECT_DEFERRED) {
					path += " (deferred)";
				}
				if (cd.flags & CONNECT_ONE_SHOT) {
					path += " (one-shot)";
				}
				if (cd.unbinds > 0) {
					path += " unbinds(" + itos(cd.unbinds) + ")";
				} else if (!cd.binds.is_empty()) {
					path += " binds(";
					for (int i = 0; i < cd.binds.size(); i++) {
						if (i > 0) {
							path += ", ";
						}
						path += cd.binds[i].operator String();
					}
					path += ")";
				}

				TreeItem *connection_item = tree->create_item(signal_item);
				connection_item->set_text(0, path);
				connection_item->set_metadata(0, connection);
				connection_item->set_icon(0, get_editor_theme_icon(SNAME("Slot")));

				if (_is_connection_inherited(connection)) {
					// The scene inherits this connection.
					connection_item->set_custom_color(0, get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
					connection_item->set_meta("_inherited_connection", true);
				}
			}
		}
	}

	connect_button->set_text(TTR("Connect..."));
	connect_button->set_button_icon(get_editor_theme_icon(SNAME("Instance")));
	connect_button->set_disabled(true);
}

ConnectionsDock::ConnectionsDock() {
	set_name(TTR("Signals"));

	VBoxContainer *vbc = this;

	search_box = memnew(LineEdit);
	search_box->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	search_box->set_placeholder(TTR("Filter Signals"));
	search_box->set_clear_button_enabled(true);
	search_box->connect(SceneStringName(text_changed), callable_mp(this, &ConnectionsDock::_filter_changed));
	vbc->add_child(search_box);

	tree = memnew(ConnectionsDockTree);
	tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	tree->set_columns(1);
	tree->set_select_mode(Tree::SELECT_ROW);
	tree->set_hide_root(true);
	tree->set_column_clip_content(0, true);
	vbc->add_child(tree);
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->set_allow_rmb_select(true);

	connect_button = memnew(Button);
	HBoxContainer *hb = memnew(HBoxContainer);
	vbc->add_child(hb);
	hb->add_spacer();
	hb->add_child(connect_button);
	connect_button->connect(SceneStringName(pressed), callable_mp(this, &ConnectionsDock::_connect_pressed));

	connect_dialog = memnew(ConnectDialog);
	connect_dialog->set_process_shortcut_input(true);
	add_child(connect_dialog);

	disconnect_all_dialog = memnew(ConfirmationDialog);
	add_child(disconnect_all_dialog);
	disconnect_all_dialog->connect(SceneStringName(confirmed), callable_mp(this, &ConnectionsDock::_disconnect_all));
	disconnect_all_dialog->set_text(TTR("Are you sure you want to remove all connections from this signal?"));

	class_menu = memnew(PopupMenu);
	class_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ConnectionsDock::_handle_class_menu_option));
	class_menu->connect("about_to_popup", callable_mp(this, &ConnectionsDock::_class_menu_about_to_popup));
	class_menu->add_item(TTR("Open Documentation"), CLASS_MENU_OPEN_DOCS);
	add_child(class_menu);

	signal_menu = memnew(PopupMenu);
	signal_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ConnectionsDock::_handle_signal_menu_option));
	signal_menu->connect("about_to_popup", callable_mp(this, &ConnectionsDock::_signal_menu_about_to_popup));
	signal_menu->add_item(TTR("Connect..."), SIGNAL_MENU_CONNECT);
	signal_menu->add_item(TTR("Disconnect All"), SIGNAL_MENU_DISCONNECT_ALL);
	signal_menu->add_item(TTR("Copy Name"), SIGNAL_MENU_COPY_NAME);
	signal_menu->add_separator();
	signal_menu->add_item(TTR("Open Documentation"), SIGNAL_MENU_OPEN_DOCS);
	add_child(signal_menu);

	slot_menu = memnew(PopupMenu);
	slot_menu->connect(SceneStringName(id_pressed), callable_mp(this, &ConnectionsDock::_handle_slot_menu_option));
	slot_menu->connect("about_to_popup", callable_mp(this, &ConnectionsDock::_slot_menu_about_to_popup));
	slot_menu->add_item(TTR("Edit..."), SLOT_MENU_EDIT);
	slot_menu->add_item(TTR("Go to Method"), SLOT_MENU_GO_TO_METHOD);
	slot_menu->add_shortcut(ED_SHORTCUT("connections_editor/disconnect", TTRC("Disconnect"), Key::KEY_DELETE), SLOT_MENU_DISCONNECT);
	add_child(slot_menu);

	connect_dialog->connect("connected", callable_mp(this, &ConnectionsDock::_make_or_edit_connection));
	tree->connect(SceneStringName(item_selected), callable_mp(this, &ConnectionsDock::_tree_item_selected));
	tree->connect("item_activated", callable_mp(this, &ConnectionsDock::_tree_item_activated));
	tree->connect(SceneStringName(gui_input), callable_mp(this, &ConnectionsDock::_tree_gui_input));

	add_theme_constant_override("separation", 3 * EDSCALE);
}

ConnectionsDock::~ConnectionsDock() {
}
