/**************************************************************************/
/*  animation_blend_tree_editor_plugin.cpp                                */
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

#include "animation_blend_tree_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/io/resource_loader.h"
#include "core/os/keyboard.h"
#include "editor/editor_inspector.h"
#include "editor/editor_node.h"
#include "editor/editor_properties.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/check_box.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/separator.h"
#include "scene/gui/view_panner.h"
#include "scene/main/window.h"
#include "scene/resources/style_box_flat.h"

void AnimationNodeBlendTreeEditor::add_custom_type(const String &p_name, const Ref<Script> &p_script) {
	for (int i = 0; i < add_options.size(); i++) {
		ERR_FAIL_COND(add_options[i].script == p_script);
	}

	AddOption ao;
	ao.name = p_name;
	ao.script = p_script;
	add_options.push_back(ao);

	_update_options_menu();
}

void AnimationNodeBlendTreeEditor::remove_custom_type(const Ref<Script> &p_script) {
	for (int i = 0; i < add_options.size(); i++) {
		if (add_options[i].script == p_script) {
			add_options.remove_at(i);
			return;
		}
	}

	_update_options_menu();
}

void AnimationNodeBlendTreeEditor::_update_options_menu(bool p_has_input_ports) {
	add_node->get_popup()->clear();
	add_node->get_popup()->reset_size();
	for (int i = 0; i < add_options.size(); i++) {
		if (p_has_input_ports && add_options[i].input_port_count == 0) {
			continue;
		}
		add_node->get_popup()->add_item(add_options[i].name, i);
	}

	Ref<AnimationNode> clipb = EditorSettings::get_singleton()->get_resource_clipboard();
	if (clipb.is_valid()) {
		add_node->get_popup()->add_separator();
		add_node->get_popup()->add_item(TTR("Paste"), MENU_PASTE);
	}
	add_node->get_popup()->add_separator();
	add_node->get_popup()->add_item(TTR("Load..."), MENU_LOAD_FILE);
	use_position_from_popup_menu = false;
}

Size2 AnimationNodeBlendTreeEditor::get_minimum_size() const {
	return Size2(10, 200);
}

void AnimationNodeBlendTreeEditor::_property_changed(const StringName &p_property, const Variant &p_value, const String &p_field, bool p_changing) {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return;
	}
	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(vformat(TTR("Parameter Changed: %s"), p_property), UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(tree, p_property, p_value);
	undo_redo->add_undo_property(tree, p_property, tree->get(p_property));
	undo_redo->add_do_method(this, "update_graph");
	undo_redo->add_undo_method(this, "update_graph");
	undo_redo->commit_action();
	updating = false;
}

void AnimationNodeBlendTreeEditor::update_graph() {
	if (updating || blend_tree.is_null()) {
		return;
	}

	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return;
	}

	visible_properties.clear();

	graph->set_scroll_offset(blend_tree->get_graph_offset() * EDSCALE);

	graph->clear_connections();
	//erase all nodes
	for (int i = 0; i < graph->get_child_count(); i++) {
		if (Object::cast_to<GraphNode>(graph->get_child(i))) {
			memdelete(graph->get_child(i));
			i--;
		}
	}

	animations.clear();

	List<StringName> nodes;
	blend_tree->get_node_list(&nodes);

	for (const StringName &E : nodes) {
		GraphNode *node = memnew(GraphNode);
		graph->add_child(node);

		node->set_draggable(!read_only);

		Ref<AnimationNode> agnode = blend_tree->get_node(E);
		ERR_CONTINUE(!agnode.is_valid());

		node->set_position_offset(blend_tree->get_node_position(E) * EDSCALE);

		node->set_title(agnode->get_caption());
		node->set_name(E);

		int base = 0;
		if (String(E) != "output") {
			LineEdit *name = memnew(LineEdit);
			name->set_text(E);
			name->set_editable(!read_only);
			name->set_expand_to_text_length_enabled(true);
			node->add_child(name);
			node->set_slot(0, false, 0, Color(), true, read_only ? -1 : 0, get_theme_color(SceneStringName(font_color), SNAME("Label")));
			name->connect("text_submitted", callable_mp(this, &AnimationNodeBlendTreeEditor::_node_renamed).bind(agnode), CONNECT_DEFERRED);
			name->connect(SceneStringName(focus_exited), callable_mp(this, &AnimationNodeBlendTreeEditor::_node_renamed_focus_out).bind(agnode), CONNECT_DEFERRED);
			name->connect(SceneStringName(text_changed), callable_mp(this, &AnimationNodeBlendTreeEditor::_node_rename_lineedit_changed), CONNECT_DEFERRED);
			base = 1;
			agnode->set_deletable(true);

			if (!read_only) {
				Button *delete_button = memnew(Button);
				delete_button->set_flat(true);
				delete_button->set_focus_mode(FOCUS_NONE);
				delete_button->set_icon(get_editor_theme_icon(SNAME("Close")));
				delete_button->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendTreeEditor::_delete_node_request).bind(E), CONNECT_DEFERRED);
				node->get_titlebar_hbox()->add_child(delete_button);
			}
		}

		for (int i = 0; i < agnode->get_input_count(); i++) {
			Label *in_name = memnew(Label);
			node->add_child(in_name);
			in_name->set_text(agnode->get_input_name(i));
			node->set_slot(base + i, true, read_only ? -1 : 0, get_theme_color(SceneStringName(font_color), SNAME("Label")), false, 0, Color());
		}

		List<PropertyInfo> pinfo;
		agnode->get_parameter_list(&pinfo);
		for (const PropertyInfo &F : pinfo) {
			if (!(F.usage & PROPERTY_USAGE_EDITOR)) {
				continue;
			}
			String base_path = AnimationTreeEditor::get_singleton()->get_base_path() + String(E) + "/" + F.name;
			EditorProperty *prop = EditorInspector::instantiate_property_editor(tree, F.type, base_path, F.hint, F.hint_string, F.usage);
			if (prop) {
				prop->set_read_only(read_only || (F.usage & PROPERTY_USAGE_READ_ONLY));
				prop->set_object_and_property(tree, base_path);
				prop->update_property();
				prop->set_name_split_ratio(0);
				prop->connect("property_changed", callable_mp(this, &AnimationNodeBlendTreeEditor::_property_changed));
				node->add_child(prop);
				visible_properties.push_back(prop);
			}
		}

		node->connect("dragged", callable_mp(this, &AnimationNodeBlendTreeEditor::_node_dragged).bind(E));

		if (AnimationTreeEditor::get_singleton()->can_edit(agnode)) {
			node->add_child(memnew(HSeparator));
			Button *open_in_editor = memnew(Button);
			open_in_editor->set_text(TTR("Open Editor"));
			open_in_editor->set_icon(get_editor_theme_icon(SNAME("Edit")));
			node->add_child(open_in_editor);
			open_in_editor->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendTreeEditor::_open_in_editor).bind(E), CONNECT_DEFERRED);
			open_in_editor->set_h_size_flags(SIZE_SHRINK_CENTER);
		}

		if (agnode->has_filter()) {
			node->add_child(memnew(HSeparator));
			Button *inspect_filters = memnew(Button);
			if (read_only) {
				inspect_filters->set_text(TTR("Inspect Filters"));
			} else {
				inspect_filters->set_text(TTR("Edit Filters"));
			}
			inspect_filters->set_icon(get_editor_theme_icon(SNAME("AnimationFilter")));
			node->add_child(inspect_filters);
			inspect_filters->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendTreeEditor::_inspect_filters).bind(E), CONNECT_DEFERRED);
			inspect_filters->set_h_size_flags(SIZE_SHRINK_CENTER);
		}

		Ref<AnimationNodeAnimation> anim = agnode;
		if (anim.is_valid()) {
			MenuButton *mb = memnew(MenuButton);
			mb->set_text(anim->get_animation());
			mb->set_icon(get_editor_theme_icon(SNAME("Animation")));
			mb->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
			mb->set_disabled(read_only);
			Array options;

			node->add_child(memnew(HSeparator));
			node->add_child(mb);

			ProgressBar *pb = memnew(ProgressBar);

			List<StringName> anims;
			tree->get_animation_list(&anims);

			for (const StringName &F : anims) {
				mb->get_popup()->add_item(F);
				options.push_back(F);
			}

			pb->set_show_percentage(false);
			pb->set_custom_minimum_size(Vector2(0, 14) * EDSCALE);
			animations[E] = pb;
			node->add_child(pb);

			mb->get_popup()->connect("index_pressed", callable_mp(this, &AnimationNodeBlendTreeEditor::_anim_selected).bind(options, E), CONNECT_DEFERRED);
		}

		// TODO: Avoid using strings, expose a method on GraphNode instead.
		Ref<StyleBoxFlat> sb = node->get_theme_stylebox(SceneStringName(panel));
		Color c = sb->get_border_color();
		Color mono_color = ((c.r + c.g + c.b) / 3) < 0.7 ? Color(1.0, 1.0, 1.0) : Color(0.0, 0.0, 0.0);
		mono_color.a = 0.85;
		c = mono_color;

		node->add_theme_color_override("title_color", c);
		c.a = 0.7;
		node->add_theme_color_override("close_color", c);
		node->add_theme_color_override("resizer_color", c);
	}

	List<AnimationNodeBlendTree::NodeConnection> node_connections;
	blend_tree->get_node_connections(&node_connections);

	for (const AnimationNodeBlendTree::NodeConnection &E : node_connections) {
		StringName from = E.output_node;
		StringName to = E.input_node;
		int to_idx = E.input_index;

		graph->connect_node(from, 0, to, to_idx);
	}

	float graph_minimap_opacity = EDITOR_GET("editors/visual_editors/minimap_opacity");
	graph->set_minimap_opacity(graph_minimap_opacity);
	float graph_lines_curvature = EDITOR_GET("editors/visual_editors/lines_curvature");
	graph->set_connection_lines_curvature(graph_lines_curvature);
}

void AnimationNodeBlendTreeEditor::_file_opened(const String &p_file) {
	file_loaded = ResourceLoader::load(p_file);
	if (file_loaded.is_valid()) {
		_add_node(MENU_LOAD_FILE_CONFIRM);
	} else {
		EditorNode::get_singleton()->show_warning(TTR("This type of node can't be used. Only animation nodes are allowed."));
	}
}

void AnimationNodeBlendTreeEditor::_add_node(int p_idx) {
	Ref<AnimationNode> anode;

	String base_name;

	if (p_idx == MENU_LOAD_FILE) {
		open_file->clear_filters();
		List<String> ext_filters;
		ResourceLoader::get_recognized_extensions_for_type("AnimationNode", &ext_filters);
		for (const String &E : ext_filters) {
			open_file->add_filter("*." + E);
		}
		open_file->popup_file_dialog();
		return;
	} else if (p_idx == MENU_LOAD_FILE_CONFIRM) {
		anode = file_loaded;
		file_loaded.unref();
		base_name = anode->get_class();
	} else if (p_idx == MENU_PASTE) {
		anode = EditorSettings::get_singleton()->get_resource_clipboard();
		ERR_FAIL_COND(!anode.is_valid());
		base_name = anode->get_class();
	} else if (!add_options[p_idx].type.is_empty()) {
		AnimationNode *an = Object::cast_to<AnimationNode>(ClassDB::instantiate(add_options[p_idx].type));
		ERR_FAIL_NULL(an);
		anode = Ref<AnimationNode>(an);
		base_name = add_options[p_idx].name;
	} else {
		ERR_FAIL_COND(add_options[p_idx].script.is_null());
		StringName base_type = add_options[p_idx].script->get_instance_base_type();
		AnimationNode *an = Object::cast_to<AnimationNode>(ClassDB::instantiate(base_type));
		ERR_FAIL_NULL(an);
		anode = Ref<AnimationNode>(an);
		anode->set_script(add_options[p_idx].script);
		base_name = add_options[p_idx].name;
	}

	Ref<AnimationNodeOutput> out = anode;
	if (out.is_valid()) {
		EditorNode::get_singleton()->show_warning(TTR("Output node can't be added to the blend tree."));
		return;
	}

	if (!from_node.is_empty() && anode->get_input_count() == 0) {
		from_node = "";
		return;
	}

	Point2 instance_pos = graph->get_scroll_offset();
	if (use_position_from_popup_menu) {
		instance_pos += position_from_popup_menu;
	} else {
		instance_pos += graph->get_size() * 0.5;
	}

	instance_pos /= graph->get_zoom();

	int base = 1;
	String name = base_name;
	while (blend_tree->has_node(name)) {
		base++;
		name = base_name + " " + itos(base);
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Add Node to BlendTree"));
	undo_redo->add_do_method(blend_tree.ptr(), "add_node", name, anode, instance_pos / EDSCALE);
	undo_redo->add_undo_method(blend_tree.ptr(), "remove_node", name);

	if (!from_node.is_empty()) {
		undo_redo->add_do_method(blend_tree.ptr(), "connect_node", name, 0, from_node);
		from_node = "";
	}
	if (!to_node.is_empty() && to_slot != -1) {
		undo_redo->add_do_method(blend_tree.ptr(), "connect_node", to_node, to_slot, name);
		to_node = "";
		to_slot = -1;
	}

	undo_redo->add_do_method(this, "update_graph");
	undo_redo->add_undo_method(this, "update_graph");
	undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_popup(bool p_has_input_ports, const Vector2 &p_node_position) {
	_update_options_menu(p_has_input_ports);
	use_position_from_popup_menu = true;
	position_from_popup_menu = p_node_position;
	add_node->get_popup()->set_position(graph->get_screen_position() + graph->get_local_mouse_position());
	add_node->get_popup()->reset_size();
	add_node->get_popup()->popup();
}

void AnimationNodeBlendTreeEditor::_popup_request(const Vector2 &p_position) {
	if (read_only) {
		return;
	}

	_popup(false, p_position);
}

void AnimationNodeBlendTreeEditor::_connection_to_empty(const String &p_from, int p_from_slot, const Vector2 &p_release_position) {
	if (read_only) {
		return;
	}

	Ref<AnimationNode> node = blend_tree->get_node(p_from);
	if (node.is_valid()) {
		from_node = p_from;
		_popup(true, p_release_position);
	}
}

void AnimationNodeBlendTreeEditor::_connection_from_empty(const String &p_to, int p_to_slot, const Vector2 &p_release_position) {
	if (read_only) {
		return;
	}

	Ref<AnimationNode> node = blend_tree->get_node(p_to);
	if (node.is_valid()) {
		to_node = p_to;
		to_slot = p_to_slot;
		_popup(false, p_release_position);
	}
}

void AnimationNodeBlendTreeEditor::_popup_hide() {
	to_node = "";
	to_slot = -1;
}

void AnimationNodeBlendTreeEditor::_node_dragged(const Vector2 &p_from, const Vector2 &p_to, const StringName &p_which) {
	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Node Moved"));
	undo_redo->add_do_method(blend_tree.ptr(), "set_node_position", p_which, p_to / EDSCALE);
	undo_redo->add_undo_method(blend_tree.ptr(), "set_node_position", p_which, p_from / EDSCALE);
	undo_redo->add_do_method(this, "update_graph");
	undo_redo->add_undo_method(this, "update_graph");
	undo_redo->commit_action();
	updating = false;
}

void AnimationNodeBlendTreeEditor::_connection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index) {
	if (read_only) {
		return;
	}

	AnimationNodeBlendTree::ConnectionError err = blend_tree->can_connect_node(p_to, p_to_index, p_from);

	if (err == AnimationNodeBlendTree::CONNECTION_ERROR_CONNECTION_EXISTS) {
		blend_tree->disconnect_node(p_to, p_to_index);
		err = blend_tree->can_connect_node(p_to, p_to_index, p_from);
	}

	if (err != AnimationNodeBlendTree::CONNECTION_OK) {
		EditorNode::get_singleton()->show_warning(TTR("Unable to connect, port may be in use or connection may be invalid."));
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Nodes Connected"));
	undo_redo->add_do_method(blend_tree.ptr(), "connect_node", p_to, p_to_index, p_from);
	undo_redo->add_undo_method(blend_tree.ptr(), "disconnect_node", p_to, p_to_index);
	undo_redo->add_do_method(this, "update_graph");
	undo_redo->add_undo_method(this, "update_graph");
	undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_disconnection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index) {
	if (read_only) {
		return;
	}

	graph->disconnect_node(p_from, p_from_index, p_to, p_to_index);

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Nodes Disconnected"));
	undo_redo->add_do_method(blend_tree.ptr(), "disconnect_node", p_to, p_to_index);
	undo_redo->add_undo_method(blend_tree.ptr(), "connect_node", p_to, p_to_index, p_from);
	undo_redo->add_do_method(this, "update_graph");
	undo_redo->add_undo_method(this, "update_graph");
	undo_redo->commit_action();
	updating = false;
}

void AnimationNodeBlendTreeEditor::_anim_selected(int p_index, const Array &p_options, const String &p_node) {
	String option = p_options[p_index];

	Ref<AnimationNodeAnimation> anim = blend_tree->get_node(p_node);
	ERR_FAIL_COND(!anim.is_valid());

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Set Animation"));
	undo_redo->add_do_method(anim.ptr(), "set_animation", option);
	undo_redo->add_undo_method(anim.ptr(), "set_animation", anim->get_animation());
	undo_redo->add_do_method(this, "update_graph");
	undo_redo->add_undo_method(this, "update_graph");
	undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_delete_node_request(const String &p_which) {
	if (read_only) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Delete Node"));
	undo_redo->add_do_method(blend_tree.ptr(), "remove_node", p_which);
	undo_redo->add_undo_method(blend_tree.ptr(), "add_node", p_which, blend_tree->get_node(p_which), blend_tree.ptr()->get_node_position(p_which));

	List<AnimationNodeBlendTree::NodeConnection> conns;
	blend_tree->get_node_connections(&conns);

	for (const AnimationNodeBlendTree::NodeConnection &E : conns) {
		if (E.output_node == p_which || E.input_node == p_which) {
			undo_redo->add_undo_method(blend_tree.ptr(), "connect_node", E.input_node, E.input_index, E.output_node);
		}
	}

	undo_redo->add_do_method(this, "update_graph");
	undo_redo->add_undo_method(this, "update_graph");
	undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_delete_nodes_request(const TypedArray<StringName> &p_nodes) {
	if (read_only) {
		return;
	}

	List<StringName> to_erase;

	if (p_nodes.is_empty()) {
		for (int i = 0; i < graph->get_child_count(); i++) {
			GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
			if (gn && gn->is_selected()) {
				Ref<AnimationNode> anode = blend_tree->get_node(gn->get_name());
				if (anode->is_deletable()) {
					to_erase.push_back(gn->get_name());
				}
			}
		}
	} else {
		for (int i = 0; i < p_nodes.size(); i++) {
			Ref<AnimationNode> anode = blend_tree->get_node(p_nodes[i]);
			if (anode->is_deletable()) {
				to_erase.push_back(p_nodes[i]);
			}
		}
	}

	if (to_erase.is_empty()) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Delete Node(s)"));

	for (const StringName &F : to_erase) {
		_delete_node_request(F);
	}

	undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_node_selected(Object *p_node) {
	if (read_only) {
		return;
	}

	GraphNode *gn = Object::cast_to<GraphNode>(p_node);
	ERR_FAIL_NULL(gn);

	String name = gn->get_name();

	Ref<AnimationNode> anode = blend_tree->get_node(name);
	ERR_FAIL_COND(!anode.is_valid());

	EditorNode::get_singleton()->push_item(anode.ptr(), "", true);
}

void AnimationNodeBlendTreeEditor::_open_in_editor(const String &p_which) {
	Ref<AnimationNode> an = blend_tree->get_node(p_which);
	ERR_FAIL_COND(!an.is_valid());
	AnimationTreeEditor::get_singleton()->enter_editor(p_which);
}

void AnimationNodeBlendTreeEditor::_filter_toggled() {
	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Toggle Filter On/Off"));
	undo_redo->add_do_method(_filter_edit.ptr(), "set_filter_enabled", filter_enabled->is_pressed());
	undo_redo->add_undo_method(_filter_edit.ptr(), "set_filter_enabled", _filter_edit->is_filter_enabled());
	undo_redo->add_do_method(this, "_update_filters", _filter_edit);
	undo_redo->add_undo_method(this, "_update_filters", _filter_edit);
	undo_redo->commit_action();
	updating = false;
}

void AnimationNodeBlendTreeEditor::_filter_edited() {
	TreeItem *edited = filters->get_edited();
	ERR_FAIL_NULL(edited);

	NodePath edited_path = edited->get_metadata(0);
	bool filtered = edited->is_checked(0);

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Change Filter"));
	undo_redo->add_do_method(_filter_edit.ptr(), "set_filter_path", edited_path, filtered);
	undo_redo->add_undo_method(_filter_edit.ptr(), "set_filter_path", edited_path, _filter_edit->is_path_filtered(edited_path));
	undo_redo->add_do_method(this, "_update_filters", _filter_edit);
	undo_redo->add_undo_method(this, "_update_filters", _filter_edit);
	undo_redo->commit_action();
	updating = false;
}

void AnimationNodeBlendTreeEditor::_filter_fill_selection() {
	TreeItem *ti = filters->get_root();
	if (!ti) {
		return;
	}

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Fill Selected Filter Children"));

	_filter_fill_selection_recursive(undo_redo, ti, false);

	undo_redo->add_do_method(this, "_update_filters", _filter_edit);
	undo_redo->add_undo_method(this, "_update_filters", _filter_edit);
	undo_redo->commit_action();
	updating = false;

	_update_filters(_filter_edit);
}

void AnimationNodeBlendTreeEditor::_filter_invert_selection() {
	TreeItem *ti = filters->get_root();
	if (!ti) {
		return;
	}

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Invert Filter Selection"));

	_filter_invert_selection_recursive(undo_redo, ti);

	undo_redo->add_do_method(this, "_update_filters", _filter_edit);
	undo_redo->add_undo_method(this, "_update_filters", _filter_edit);
	undo_redo->commit_action();
	updating = false;

	_update_filters(_filter_edit);
}

void AnimationNodeBlendTreeEditor::_filter_clear_selection() {
	TreeItem *ti = filters->get_root();
	if (!ti) {
		return;
	}

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Clear Filter Selection"));

	_filter_clear_selection_recursive(undo_redo, ti);

	undo_redo->add_do_method(this, "_update_filters", _filter_edit);
	undo_redo->add_undo_method(this, "_update_filters", _filter_edit);
	undo_redo->commit_action();
	updating = false;

	_update_filters(_filter_edit);
}

void AnimationNodeBlendTreeEditor::_filter_fill_selection_recursive(EditorUndoRedoManager *p_undo_redo, TreeItem *p_item, bool p_parent_filtered) {
	TreeItem *ti = p_item->get_first_child();
	bool parent_filtered = p_parent_filtered;
	while (ti) {
		NodePath item_path = ti->get_metadata(0);
		bool filtered = _filter_edit->is_path_filtered(item_path);
		parent_filtered |= filtered;

		p_undo_redo->add_do_method(_filter_edit.ptr(), "set_filter_path", item_path, parent_filtered);
		p_undo_redo->add_undo_method(_filter_edit.ptr(), "set_filter_path", item_path, filtered);

		_filter_fill_selection_recursive(p_undo_redo, ti, parent_filtered);
		ti = ti->get_next();
		parent_filtered = p_parent_filtered;
	}
}

void AnimationNodeBlendTreeEditor::_filter_invert_selection_recursive(EditorUndoRedoManager *p_undo_redo, TreeItem *p_item) {
	TreeItem *ti = p_item->get_first_child();
	while (ti) {
		NodePath item_path = ti->get_metadata(0);
		bool filtered = _filter_edit->is_path_filtered(item_path);

		p_undo_redo->add_do_method(_filter_edit.ptr(), "set_filter_path", item_path, !filtered);
		p_undo_redo->add_undo_method(_filter_edit.ptr(), "set_filter_path", item_path, filtered);

		_filter_invert_selection_recursive(p_undo_redo, ti);
		ti = ti->get_next();
	}
}

void AnimationNodeBlendTreeEditor::_filter_clear_selection_recursive(EditorUndoRedoManager *p_undo_redo, TreeItem *p_item) {
	TreeItem *ti = p_item->get_first_child();
	while (ti) {
		NodePath item_path = ti->get_metadata(0);
		bool filtered = _filter_edit->is_path_filtered(item_path);

		p_undo_redo->add_do_method(_filter_edit.ptr(), "set_filter_path", item_path, false);
		p_undo_redo->add_undo_method(_filter_edit.ptr(), "set_filter_path", item_path, filtered);

		_filter_clear_selection_recursive(p_undo_redo, ti);
		ti = ti->get_next();
	}
}

bool AnimationNodeBlendTreeEditor::_update_filters(const Ref<AnimationNode> &anode) {
	if (updating || _filter_edit != anode) {
		return false;
	}

	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return false;
	}

	Node *base = tree->get_node(tree->get_root_node());
	if (!base) {
		EditorNode::get_singleton()->show_warning(TTR("Animation player has no valid root node path, so unable to retrieve track names."));
		return false;
	}

	updating = true;

	HashSet<String> paths;
	HashMap<String, RBSet<String>> types;
	{
		List<StringName> animation_list;
		tree->get_animation_list(&animation_list);

		for (const StringName &E : animation_list) {
			Ref<Animation> anim = tree->get_animation(E);
			for (int i = 0; i < anim->get_track_count(); i++) {
				String track_path = anim->track_get_path(i);
				paths.insert(track_path);

				String track_type_name;
				Animation::TrackType track_type = anim->track_get_type(i);
				switch (track_type) {
					case Animation::TrackType::TYPE_ANIMATION: {
						track_type_name = TTR("Anim Clips");
					} break;
					case Animation::TrackType::TYPE_AUDIO: {
						track_type_name = TTR("Audio Clips");
					} break;
					case Animation::TrackType::TYPE_METHOD: {
						track_type_name = TTR("Functions");
					} break;
					default: {
					} break;
				}
				if (!track_type_name.is_empty()) {
					types[track_path].insert(track_type_name);
				}
			}
		}
	}

	filter_enabled->set_pressed(anode->is_filter_enabled());
	filters->clear();
	TreeItem *root = filters->create_item();

	HashMap<String, TreeItem *> parenthood;

	for (const String &E : paths) {
		NodePath path = E;
		TreeItem *ti = nullptr;
		String accum;
		for (int i = 0; i < path.get_name_count(); i++) {
			String name = path.get_name(i);
			if (!accum.is_empty()) {
				accum += "/";
			}
			accum += name;
			if (!parenthood.has(accum)) {
				if (ti) {
					ti = filters->create_item(ti);
				} else {
					ti = filters->create_item(root);
				}
				parenthood[accum] = ti;
				ti->set_text(0, name);
				ti->set_selectable(0, false);
				ti->set_editable(0, false);

				if (base->has_node(accum)) {
					Node *node = base->get_node(accum);
					ti->set_icon(0, EditorNode::get_singleton()->get_object_icon(node, "Node"));
				}

			} else {
				ti = parenthood[accum];
			}
		}

		Node *node = nullptr;
		if (base->has_node(accum)) {
			node = base->get_node(accum);
		}
		if (!node) {
			continue; //no node, can't edit
		}

		if (path.get_subname_count()) {
			String concat = path.get_concatenated_subnames();

			Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(node);
			if (skeleton && skeleton->find_bone(concat) != -1) {
				//path in skeleton
				const String &bone = concat;
				int idx = skeleton->find_bone(bone);
				List<String> bone_path;
				while (idx != -1) {
					bone_path.push_front(skeleton->get_bone_name(idx));
					idx = skeleton->get_bone_parent(idx);
				}

				accum += ":";
				for (List<String>::Element *F = bone_path.front(); F; F = F->next()) {
					if (F != bone_path.front()) {
						accum += "/";
					}

					accum += F->get();
					if (!parenthood.has(accum)) {
						ti = filters->create_item(ti);
						parenthood[accum] = ti;
						ti->set_text(0, F->get());
						ti->set_selectable(0, false);
						ti->set_editable(0, false);
						ti->set_icon(0, get_editor_theme_icon(SNAME("Bone")));
					} else {
						ti = parenthood[accum];
					}
				}

				ti->set_editable(0, !read_only);
				ti->set_selectable(0, true);
				ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				ti->set_text(0, concat);
				ti->set_checked(0, anode->is_path_filtered(path));
				ti->set_icon(0, get_editor_theme_icon(SNAME("Bone")));
				ti->set_metadata(0, path);

			} else {
				//just a property
				ti = filters->create_item(ti);
				ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				ti->set_text(0, concat);
				ti->set_editable(0, !read_only);
				ti->set_selectable(0, true);
				ti->set_checked(0, anode->is_path_filtered(path));
				ti->set_metadata(0, path);
			}
		} else {
			if (ti) {
				//just a node, not a property track
				String types_text = "[";
				if (types.has(path)) {
					RBSet<String>::Iterator F = types[path].begin();
					types_text += *F;
					while (F) {
						types_text += " / " + *F;
						;
						++F;
					}
				}
				types_text += "]";
				ti = filters->create_item(ti);
				ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				ti->set_text(0, types_text);
				ti->set_editable(0, !read_only);
				ti->set_selectable(0, true);
				ti->set_checked(0, anode->is_path_filtered(path));
				ti->set_metadata(0, path);
			}
		}
	}

	updating = false;

	return true;
}

void AnimationNodeBlendTreeEditor::_inspect_filters(const String &p_which) {
	if (read_only) {
		filter_dialog->set_title(TTR("Inspect Filtered Tracks:"));
	} else {
		filter_dialog->set_title(TTR("Edit Filtered Tracks:"));
	}

	filter_enabled->set_disabled(read_only);

	Ref<AnimationNode> anode = blend_tree->get_node(p_which);
	ERR_FAIL_COND(!anode.is_valid());

	_filter_edit = anode;
	if (!_update_filters(anode)) {
		return;
	}

	filter_dialog->popup_centered(Size2(500, 500) * EDSCALE);
}

void AnimationNodeBlendTreeEditor::_update_editor_settings() {
	graph->get_panner()->setup((ViewPanner::ControlScheme)EDITOR_GET("editors/panning/sub_editors_panning_scheme").operator int(), ED_GET_SHORTCUT("canvas_item_editor/pan_view"), bool(EDITOR_GET("editors/panning/simple_panning")));
	graph->set_warped_panning(bool(EDITOR_GET("editors/panning/warped_mouse_panning")));
}

void AnimationNodeBlendTreeEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_editor_settings();
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("editors/panning")) {
				_update_editor_settings();
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			error_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SceneStringName(panel), SNAME("Tree")));
			error_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));

			if (is_visible_in_tree()) {
				update_graph();
			}
		} break;

		case NOTIFICATION_PROCESS: {
			AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
			if (!tree) {
				return; // Node has been changed.
			}

			String error;

			if (!tree->is_active()) {
				error = TTR("AnimationTree is inactive.\nActivate to enable playback, check node warnings if activation fails.");
			} else if (tree->is_state_invalid()) {
				error = tree->get_invalid_state_reason();
			}

			if (error != error_label->get_text()) {
				error_label->set_text(error);
				if (!error.is_empty()) {
					error_panel->show();
				} else {
					error_panel->hide();
				}
			}

			List<AnimationNodeBlendTree::NodeConnection> conns;
			blend_tree->get_node_connections(&conns);
			for (const AnimationNodeBlendTree::NodeConnection &E : conns) {
				float activity = 0;
				StringName path = AnimationTreeEditor::get_singleton()->get_base_path() + E.input_node;
				if (!tree->is_state_invalid()) {
					activity = tree->get_connection_activity(path, E.input_index);
				}
				graph->set_connection_activity(E.output_node, 0, E.input_node, E.input_index, activity);
			}

			for (const KeyValue<StringName, ProgressBar *> &E : animations) {
				Ref<AnimationNodeAnimation> an = blend_tree->get_node(E.key);
				if (an.is_valid()) {
					if (tree->has_animation(an->get_animation())) {
						Ref<Animation> anim = tree->get_animation(an->get_animation());
						if (anim.is_valid()) {
							//StringName path = AnimationTreeEditor::get_singleton()->get_base_path() + E.input_node;
							StringName length_path = AnimationTreeEditor::get_singleton()->get_base_path() + String(E.key) + "/current_length";
							StringName time_path = AnimationTreeEditor::get_singleton()->get_base_path() + String(E.key) + "/current_position";
							E.value->set_max(tree->get(length_path));
							E.value->set_value(tree->get(time_path));
						}
					}
				}
			}

			for (int i = 0; i < visible_properties.size(); i++) {
				visible_properties[i]->update_property();
			}
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			set_process(is_visible_in_tree());
		} break;
	}
}

void AnimationNodeBlendTreeEditor::_scroll_changed(const Vector2 &p_scroll) {
	if (read_only) {
		return;
	}

	if (updating) {
		return;
	}
	updating = true;
	blend_tree->set_graph_offset(p_scroll / EDSCALE);
	updating = false;
}

void AnimationNodeBlendTreeEditor::_bind_methods() {
	ClassDB::bind_method("update_graph", &AnimationNodeBlendTreeEditor::update_graph);
	ClassDB::bind_method("_update_filters", &AnimationNodeBlendTreeEditor::_update_filters);
}

AnimationNodeBlendTreeEditor *AnimationNodeBlendTreeEditor::singleton = nullptr;

// AnimationNode's "node_changed" signal means almost update_input.
void AnimationNodeBlendTreeEditor::_node_changed(const StringName &p_node_name) {
	// TODO:
	// Here is executed during the commit of EditorNode::undo_redo, it is not possible to create an undo_redo action here.
	// The disconnect when the number of enabled inputs decreases is done in AnimationNodeBlendTree and update_graph().
	// This means that there is no place to register undo_redo actions.
	// In order to implement undo_redo correctly, we may need to implement AnimationNodeEdit such as AnimationTrackKeyEdit
	// and add it to _node_selected() with EditorNode::get_singleton()->push_item(AnimationNodeEdit).
	update_graph();
}

void AnimationNodeBlendTreeEditor::_node_renamed(const String &p_text, Ref<AnimationNode> p_node) {
	if (blend_tree.is_null()) {
		return;
	}

	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	if (!tree) {
		return;
	}

	String prev_name = blend_tree->get_node_name(p_node);
	ERR_FAIL_COND(prev_name.is_empty());
	GraphNode *gn = Object::cast_to<GraphNode>(graph->get_node(prev_name));
	ERR_FAIL_NULL(gn);

	const String &new_name = p_text;

	ERR_FAIL_COND(new_name.is_empty() || new_name.contains(".") || new_name.contains("/"));

	if (new_name == prev_name) {
		return; //nothing to do
	}

	const String &base_name = new_name;
	int base = 1;
	String name = base_name;
	while (blend_tree->has_node(name)) {
		base++;
		name = base_name + " " + itos(base);
	}

	String base_path = AnimationTreeEditor::get_singleton()->get_base_path();

	updating = true;
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Node Renamed"));
	undo_redo->add_do_method(blend_tree.ptr(), "rename_node", prev_name, name);
	undo_redo->add_undo_method(blend_tree.ptr(), "rename_node", name, prev_name);
	undo_redo->add_do_method(this, "update_graph");
	undo_redo->add_undo_method(this, "update_graph");
	undo_redo->commit_action();
	updating = false;
	gn->set_name(new_name);
	gn->set_size(gn->get_minimum_size());

	//change editors accordingly
	for (int i = 0; i < visible_properties.size(); i++) {
		String pname = visible_properties[i]->get_edited_property().operator String();
		if (pname.begins_with(base_path + prev_name)) {
			String new_name2 = pname.replace_first(base_path + prev_name, base_path + name);
			visible_properties[i]->set_object_and_property(visible_properties[i]->get_edited_object(), new_name2);
		}
	}

	//recreate connections
	graph->clear_connections();

	List<AnimationNodeBlendTree::NodeConnection> node_connections;
	blend_tree->get_node_connections(&node_connections);

	for (const AnimationNodeBlendTree::NodeConnection &E : node_connections) {
		StringName from = E.output_node;
		StringName to = E.input_node;
		int to_idx = E.input_index;

		graph->connect_node(from, 0, to, to_idx);
	}

	//update animations
	for (const KeyValue<StringName, ProgressBar *> &E : animations) {
		if (E.key == prev_name) {
			animations[new_name] = animations[prev_name];
			animations.erase(prev_name);
			break;
		}
	}

	update_graph(); // Needed to update the signal connections with the new name.
	current_node_rename_text = String();
}

void AnimationNodeBlendTreeEditor::_node_renamed_focus_out(Ref<AnimationNode> p_node) {
	if (current_node_rename_text.is_empty()) {
		return; // The text_submitted signal triggered the graph update and freed the LineEdit.
	}
	_node_renamed(current_node_rename_text, p_node);
}

void AnimationNodeBlendTreeEditor::_node_rename_lineedit_changed(const String &p_text) {
	current_node_rename_text = p_text;
}

bool AnimationNodeBlendTreeEditor::can_edit(const Ref<AnimationNode> &p_node) {
	Ref<AnimationNodeBlendTree> bt = p_node;
	return bt.is_valid();
}

void AnimationNodeBlendTreeEditor::edit(const Ref<AnimationNode> &p_node) {
	if (blend_tree.is_valid()) {
		blend_tree->disconnect("node_changed", callable_mp(this, &AnimationNodeBlendTreeEditor::_node_changed));
	}

	blend_tree = p_node;

	read_only = false;

	if (blend_tree.is_null()) {
		hide();
	} else {
		read_only = EditorNode::get_singleton()->is_resource_read_only(blend_tree);

		blend_tree->connect("node_changed", callable_mp(this, &AnimationNodeBlendTreeEditor::_node_changed));

		update_graph();
	}

	add_node->set_disabled(read_only);
	graph->set_show_arrange_button(!read_only);
}

AnimationNodeBlendTreeEditor::AnimationNodeBlendTreeEditor() {
	singleton = this;
	updating = false;
	use_position_from_popup_menu = false;

	graph = memnew(GraphEdit);
	add_child(graph);
	graph->add_valid_right_disconnect_type(0);
	graph->add_valid_left_disconnect_type(0);
	graph->set_v_size_flags(SIZE_EXPAND_FILL);
	graph->connect("connection_request", callable_mp(this, &AnimationNodeBlendTreeEditor::_connection_request), CONNECT_DEFERRED);
	graph->connect("disconnection_request", callable_mp(this, &AnimationNodeBlendTreeEditor::_disconnection_request), CONNECT_DEFERRED);
	graph->connect("node_selected", callable_mp(this, &AnimationNodeBlendTreeEditor::_node_selected));
	graph->connect("scroll_offset_changed", callable_mp(this, &AnimationNodeBlendTreeEditor::_scroll_changed));
	graph->connect("delete_nodes_request", callable_mp(this, &AnimationNodeBlendTreeEditor::_delete_nodes_request));
	graph->connect("popup_request", callable_mp(this, &AnimationNodeBlendTreeEditor::_popup_request));
	graph->connect("connection_to_empty", callable_mp(this, &AnimationNodeBlendTreeEditor::_connection_to_empty));
	graph->connect("connection_from_empty", callable_mp(this, &AnimationNodeBlendTreeEditor::_connection_from_empty));
	float graph_minimap_opacity = EDITOR_GET("editors/visual_editors/minimap_opacity");
	graph->set_minimap_opacity(graph_minimap_opacity);
	float graph_lines_curvature = EDITOR_GET("editors/visual_editors/lines_curvature");
	graph->set_connection_lines_curvature(graph_lines_curvature);

	VSeparator *vs = memnew(VSeparator);
	graph->get_menu_hbox()->add_child(vs);
	graph->get_menu_hbox()->move_child(vs, 0);

	add_node = memnew(MenuButton);
	graph->get_menu_hbox()->add_child(add_node);
	add_node->set_text(TTR("Add Node..."));
	graph->get_menu_hbox()->move_child(add_node, 0);
	add_node->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &AnimationNodeBlendTreeEditor::_add_node));
	add_node->get_popup()->connect("popup_hide", callable_mp(this, &AnimationNodeBlendTreeEditor::_popup_hide), CONNECT_DEFERRED);
	add_node->connect("about_to_popup", callable_mp(this, &AnimationNodeBlendTreeEditor::_update_options_menu).bind(false));
	add_node->set_disabled(read_only);

	add_options.push_back(AddOption("Animation", "AnimationNodeAnimation"));
	add_options.push_back(AddOption("OneShot", "AnimationNodeOneShot", 2));
	add_options.push_back(AddOption("Add2", "AnimationNodeAdd2", 2));
	add_options.push_back(AddOption("Add3", "AnimationNodeAdd3", 3));
	add_options.push_back(AddOption("Blend2", "AnimationNodeBlend2", 2));
	add_options.push_back(AddOption("Blend3", "AnimationNodeBlend3", 3));
	add_options.push_back(AddOption("Sub2", "AnimationNodeSub2", 2));
	add_options.push_back(AddOption("TimeSeek", "AnimationNodeTimeSeek", 1));
	add_options.push_back(AddOption("TimeScale", "AnimationNodeTimeScale", 1));
	add_options.push_back(AddOption("Transition", "AnimationNodeTransition"));
	add_options.push_back(AddOption("BlendTree", "AnimationNodeBlendTree"));
	add_options.push_back(AddOption("BlendSpace1D", "AnimationNodeBlendSpace1D"));
	add_options.push_back(AddOption("BlendSpace2D", "AnimationNodeBlendSpace2D"));
	add_options.push_back(AddOption("StateMachine", "AnimationNodeStateMachine"));
	_update_options_menu();

	error_panel = memnew(PanelContainer);
	add_child(error_panel);
	error_label = memnew(Label);
	error_panel->add_child(error_label);
	error_label->set_text("eh");

	filter_dialog = memnew(AcceptDialog);
	add_child(filter_dialog);
	filter_dialog->set_title(TTR("Edit Filtered Tracks:"));

	VBoxContainer *filter_vbox = memnew(VBoxContainer);
	filter_dialog->add_child(filter_vbox);

	HBoxContainer *filter_hbox = memnew(HBoxContainer);
	filter_vbox->add_child(filter_hbox);

	filter_enabled = memnew(CheckBox);
	filter_enabled->set_text(TTR("Enable Filtering"));
	filter_enabled->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendTreeEditor::_filter_toggled));
	filter_hbox->add_child(filter_enabled);

	filter_fill_selection = memnew(Button);
	filter_fill_selection->set_text(TTR("Fill Selected Children"));
	filter_fill_selection->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendTreeEditor::_filter_fill_selection));
	filter_hbox->add_child(filter_fill_selection);

	filter_invert_selection = memnew(Button);
	filter_invert_selection->set_text(TTR("Invert"));
	filter_invert_selection->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendTreeEditor::_filter_invert_selection));
	filter_hbox->add_child(filter_invert_selection);

	filter_clear_selection = memnew(Button);
	filter_clear_selection->set_text(TTR("Clear"));
	filter_clear_selection->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeBlendTreeEditor::_filter_clear_selection));
	filter_hbox->add_child(filter_clear_selection);

	filters = memnew(Tree);
	filter_vbox->add_child(filters);
	filters->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	filters->set_v_size_flags(SIZE_EXPAND_FILL);
	filters->set_hide_root(true);
	filters->connect("item_edited", callable_mp(this, &AnimationNodeBlendTreeEditor::_filter_edited));

	open_file = memnew(EditorFileDialog);
	add_child(open_file);
	open_file->set_title(TTR("Open Animation Node"));
	open_file->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	open_file->connect("file_selected", callable_mp(this, &AnimationNodeBlendTreeEditor::_file_opened));

	animation_node_inspector_plugin = Ref<EditorInspectorPluginAnimationNodeAnimation>(memnew(EditorInspectorPluginAnimationNodeAnimation));
	EditorInspector::add_inspector_plugin(animation_node_inspector_plugin);
}

AnimationNodeBlendTreeEditor::~AnimationNodeBlendTreeEditor() {
}

// EditorPluginAnimationNodeAnimation

void AnimationNodeAnimationEditor::_open_set_custom_timeline_from_marker_dialog() {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_animation_tree();
	StringName anim_name = animation_node_animation->get_animation();
	PackedStringArray markers = tree->has_animation(anim_name) ? tree->get_animation(anim_name)->get_marker_names() : PackedStringArray();

	dialog->select_start->clear();
	dialog->select_start->add_icon_item(get_editor_theme_icon(SNAME("PlayStart")), TTR("Start of Animation"));
	dialog->select_start->add_separator();
	dialog->select_end->clear();
	dialog->select_end->add_icon_item(get_editor_theme_icon(SNAME("PlayStartBackwards")), TTR("End of Animation"));
	dialog->select_end->add_separator();

	for (const String &marker : markers) {
		dialog->select_start->add_item(marker);
		dialog->select_end->add_item(marker);
	}

	// Because the default selections are always valid, and marker times won't change during the dialog, we can ensure that the user can only select valid markers.
	// This invariant is maintained by _validate_markers.
	dialog->select_start->select(0);
	dialog->select_end->select(0);

	dialog->popup_centered(Size2(200, 0) * EDSCALE);
}

void AnimationNodeAnimationEditor::_validate_markers(int p_id) {
	// Note: p_id is ignored. It is included because OptionButton's item_changed signal always passes it.
	int start_id = dialog->select_start->get_selected_id();
	int end_id = dialog->select_end->get_selected_id();

	StringName anim_name = animation_node_animation->get_animation();
	Ref<Animation> animation = AnimationTreeEditor::get_singleton()->get_animation_tree()->get_animation(anim_name);
	ERR_FAIL_COND(animation.is_null());

	double start_time = start_id < 2 ? 0 : animation->get_marker_time(dialog->select_start->get_item_text(start_id));
	double end_time = end_id < 2 ? animation->get_length() : animation->get_marker_time(dialog->select_end->get_item_text(end_id));

	// p_start and p_end have the same item count.
	for (int i = 2; i < dialog->select_start->get_item_count(); i++) {
		String start_marker = dialog->select_start->get_item_text(i);
		String end_marker = dialog->select_end->get_item_text(i);
		dialog->select_start->set_item_disabled(i, end_id >= 2 && (i == end_id || animation->get_marker_time(start_marker) > end_time));
		dialog->select_end->set_item_disabled(i, start_id >= 2 && (i == start_id || start_time > animation->get_marker_time(end_marker)));
	}
}

void AnimationNodeAnimationEditor::_confirm_set_custom_timeline_from_marker_dialog() {
	int start_id = dialog->select_start->get_selected_id();
	int end_id = dialog->select_end->get_selected_id();

	Ref<Animation> animation = AnimationTreeEditor::get_singleton()->get_animation_tree()->get_animation(animation_node_animation->get_animation());
	ERR_FAIL_COND(animation.is_null());
	double start_time = start_id < 2 ? 0 : animation->get_marker_time(dialog->select_start->get_item_text(start_id));
	double end_time = end_id < 2 ? animation->get_length() : animation->get_marker_time(dialog->select_end->get_item_text(end_id));
	double length = end_time - start_time;

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Set Custom Timeline from Marker"));
	undo_redo->add_do_method(*animation_node_animation, "set_start_offset", start_time);
	undo_redo->add_undo_method(*animation_node_animation, "set_start_offset", animation_node_animation->get_start_offset());
	undo_redo->add_do_method(*animation_node_animation, "set_stretch_time_scale", false);
	undo_redo->add_undo_method(*animation_node_animation, "set_stretch_time_scale", animation_node_animation->is_stretching_time_scale());
	undo_redo->add_do_method(*animation_node_animation, "set_timeline_length", length);
	undo_redo->add_undo_method(*animation_node_animation, "set_timeline_length", animation_node_animation->get_timeline_length());
	undo_redo->add_do_method(*animation_node_animation, "notify_property_list_changed");
	undo_redo->add_undo_method(*animation_node_animation, "notify_property_list_changed");
	undo_redo->commit_action();
}

AnimationNodeAnimationEditor::AnimationNodeAnimationEditor(Ref<AnimationNodeAnimation> p_animation_node_animation) {
	animation_node_animation = p_animation_node_animation;

	dialog = memnew(AnimationNodeAnimationEditorDialog);
	add_child(dialog);
	dialog->set_hide_on_ok(false);
	dialog->select_start->connect(SceneStringName(item_selected), callable_mp(this, &AnimationNodeAnimationEditor::_validate_markers));
	dialog->select_end->connect(SceneStringName(item_selected), callable_mp(this, &AnimationNodeAnimationEditor::_validate_markers));
	dialog->connect(SceneStringName(confirmed), callable_mp(this, &AnimationNodeAnimationEditor::_confirm_set_custom_timeline_from_marker_dialog));

	Control *top_spacer = memnew(Control);
	add_child(top_spacer);
	top_spacer->set_custom_minimum_size(Size2(0, 2) * EDSCALE);

	button = memnew(Button);
	add_child(button);
	button->set_text(TTR("Set Custom Timeline from Marker"));
	button->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	button->connect(SceneStringName(pressed), callable_mp(this, &AnimationNodeAnimationEditor::_open_set_custom_timeline_from_marker_dialog));

	Control *bottom_spacer = memnew(Control);
	add_child(bottom_spacer);
	bottom_spacer->set_custom_minimum_size(Size2(0, 2) * EDSCALE);
}

AnimationNodeAnimationEditor::~AnimationNodeAnimationEditor() {
}

void AnimationNodeAnimationEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			button->set_theme_type_variation(SNAME("InspectorActionButton"));
			button->set_icon(get_editor_theme_icon(SNAME("Edit")));
		} break;
	}
}

bool EditorInspectorPluginAnimationNodeAnimation::can_handle(Object *p_object) {
	Ref<AnimationNodeAnimation> ana(Object::cast_to<AnimationNodeAnimation>(p_object));
	return ana.is_valid() && ana->is_using_custom_timeline();
}

bool EditorInspectorPluginAnimationNodeAnimation::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	Ref<AnimationNodeAnimation> ana(Object::cast_to<AnimationNodeAnimation>(p_object));
	ERR_FAIL_COND_V(ana.is_null(), false);

	if (p_path == "timeline_length") {
		add_custom_control(memnew(AnimationNodeAnimationEditor(ana)));
	}

	return false;
}

AnimationNodeAnimationEditorDialog::AnimationNodeAnimationEditorDialog() {
	set_title(TTR("Select Markers..."));
	VBoxContainer *vbox = memnew(VBoxContainer);
	add_child(vbox);
	vbox->set_offsets_preset(Control::PRESET_FULL_RECT);

	HBoxContainer *container_start = memnew(HBoxContainer);
	vbox->add_child(container_start);
	Label *label_start = memnew(Label);
	container_start->add_child(label_start);
	label_start->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label_start->set_stretch_ratio(1);
	label_start->set_text(TTR("Start Marker"));
	select_start = memnew(OptionButton);
	container_start->add_child(select_start);
	select_start->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	select_start->set_stretch_ratio(2);

	HBoxContainer *container_end = memnew(HBoxContainer);
	vbox->add_child(container_end);
	Label *label_end = memnew(Label);
	container_end->add_child(label_end);
	label_end->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label_end->set_stretch_ratio(1);
	label_end->set_text(TTR("End Marker"));
	select_end = memnew(OptionButton);
	container_end->add_child(select_end);
	select_end->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	select_end->set_stretch_ratio(2);
}

AnimationNodeAnimationEditorDialog::~AnimationNodeAnimationEditorDialog() {
}
