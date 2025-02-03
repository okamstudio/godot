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

#include "core/io/resource_loader.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/project_settings.h"
#include "editor/editor_inspector.h"
#include "editor/editor_scale.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/progress_bar.h"
#include "scene/main/viewport.h"

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
			add_options.remove(i);
			return;
		}
	}

	_update_options_menu();
}

void AnimationNodeBlendTreeEditor::_update_options_menu(bool p_has_input_ports) {
	add_node->get_popup()->clear();
	add_node->get_popup()->set_size(Size2i(-1, -1));
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
	use_popup_menu_position = false;
}

Size2 AnimationNodeBlendTreeEditor::get_minimum_size() const {
	return Size2(10, 200);
}

void AnimationNodeBlendTreeEditor::_property_changed(const StringName &p_property, const Variant &p_value, const String &p_field, bool p_changing) {
	AnimationTree *tree = AnimationTreeEditor::get_singleton()->get_tree();
	updating = true;
	undo_redo->create_action(TTR("Parameter Changed:") + " " + String(p_property), UndoRedo::MERGE_ENDS);
	undo_redo->add_do_property(tree, p_property, p_value);
	undo_redo->add_undo_property(tree, p_property, tree->get(p_property));
	undo_redo->add_do_method(this, "_update_graph");
	undo_redo->add_undo_method(this, "_update_graph");
	undo_redo->commit_action();
	updating = false;
}

void AnimationNodeBlendTreeEditor::_update_graph() {
	if (updating || blend_tree.is_null()) {
		return;
	}

	visible_properties.clear();

	graph->set_scroll_ofs(blend_tree->get_graph_offset() * EDSCALE);

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

	for (List<StringName>::Element *E = nodes.front(); E; E = E->next()) {
		GraphNode *node = memnew(GraphNode);
		graph->add_child(node);

		Ref<AnimationNode> agnode = blend_tree->get_node(E->get());

		node->set_offset(blend_tree->get_node_position(E->get()) * EDSCALE);

		node->set_title(agnode->get_caption());
		node->set_name(E->get());

		int base = 0;
		if (String(E->get()) != "output") {
			LineEdit *name = memnew(LineEdit);
			name->set_text(E->get());
			name->set_expand_to_text_length(true);
			node->add_child(name);
			node->set_slot(0, false, 0, Color(), true, 0, get_color("font_color", "Label"));
			name->connect("text_entered", this, "_node_renamed", varray(agnode), CONNECT_DEFERRED);
			name->connect("focus_exited", this, "_node_renamed_focus_out", varray(name, agnode), CONNECT_DEFERRED);
			base = 1;
			node->set_show_close_button(true);
			node->connect("close_request", this, "_delete_request", varray(E->get()), CONNECT_DEFERRED);
		}

		for (int i = 0; i < agnode->get_input_count(); i++) {
			Label *in_name = memnew(Label);
			node->add_child(in_name);
			in_name->set_text(agnode->get_input_name(i));
			node->set_slot(base + i, true, 0, get_color("font_color", "Label"), false, 0, Color());
		}

		List<PropertyInfo> pinfo;
		agnode->get_parameter_list(&pinfo);
		for (List<PropertyInfo>::Element *F = pinfo.front(); F; F = F->next()) {
			if (!(F->get().usage & PROPERTY_USAGE_EDITOR)) {
				continue;
			}
			String base_path = AnimationTreeEditor::get_singleton()->get_base_path() + String(E->get()) + "/" + F->get().name;
			EditorProperty *prop = EditorInspector::instantiate_property_editor(AnimationTreeEditor::get_singleton()->get_tree(), F->get().type, base_path, F->get().hint, F->get().hint_string, F->get().usage);
			if (prop) {
				prop->set_object_and_property(AnimationTreeEditor::get_singleton()->get_tree(), base_path);
				prop->update_property();
				prop->set_name_split_ratio(0);
				prop->connect("property_changed", this, "_property_changed");
				node->add_child(prop);
				visible_properties.push_back(prop);
			}
		}

		node->connect("dragged", this, "_node_dragged", varray(E->get()));

		if (AnimationTreeEditor::get_singleton()->can_edit(agnode)) {
			node->add_child(memnew(HSeparator));
			Button *open_in_editor = memnew(Button);
			open_in_editor->set_text(TTR("Open Editor"));
			open_in_editor->set_icon(get_icon("Edit", "EditorIcons"));
			node->add_child(open_in_editor);
			open_in_editor->connect("pressed", this, "_open_in_editor", varray(E->get()), CONNECT_DEFERRED);
			open_in_editor->set_h_size_flags(SIZE_SHRINK_CENTER);
		}

		if (agnode->has_filter()) {
			node->add_child(memnew(HSeparator));
			Button *edit_filters = memnew(Button);
			edit_filters->set_text(TTR("Edit Filters"));
			edit_filters->set_icon(get_icon("AnimationFilter", "EditorIcons"));
			node->add_child(edit_filters);
			edit_filters->connect("pressed", this, "_edit_filters", varray(E->get()), CONNECT_DEFERRED);
			edit_filters->set_h_size_flags(SIZE_SHRINK_CENTER);
		}

		Ref<AnimationNodeAnimation> anim = agnode;
		if (anim.is_valid()) {
			MenuButton *mb = memnew(MenuButton);
			mb->set_text(anim->get_animation());
			mb->set_icon(get_icon("Animation", "EditorIcons"));
			Array options;

			node->add_child(memnew(HSeparator));
			node->add_child(mb);

			ProgressBar *pb = memnew(ProgressBar);

			AnimationTree *player = AnimationTreeEditor::get_singleton()->get_tree();
			if (player->has_node(player->get_animation_player())) {
				AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(player->get_node(player->get_animation_player()));
				if (ap) {
					List<StringName> anims;
					ap->get_animation_list(&anims);

					for (List<StringName>::Element *F = anims.front(); F; F = F->next()) {
						mb->get_popup()->add_item(F->get());
						options.push_back(F->get());
					}

					if (ap->has_animation(anim->get_animation())) {
						pb->set_max(ap->get_animation(anim->get_animation())->get_length());
					}
				}
			}

			pb->set_percent_visible(false);
			pb->set_custom_minimum_size(Vector2(0, 14) * EDSCALE);
			animations[E->get()] = pb;
			node->add_child(pb);

			mb->get_popup()->connect("index_pressed", this, "_anim_selected", varray(options, E->get()), CONNECT_DEFERRED);
		}

		if (EditorSettings::get_singleton()->get("interface/theme/use_graph_node_headers")) {
			Ref<StyleBoxFlat> sb = node->get_stylebox("frame", "GraphNode");
			Color c = sb->get_border_color();
			Color mono_color = ((c.r + c.g + c.b) / 3) < 0.7 ? Color(1.0, 1.0, 1.0) : Color(0.0, 0.0, 0.0);
			mono_color.a = 0.85;
			c = mono_color;

			node->add_color_override("title_color", c);
			c.a = 0.7;
			node->add_color_override("close_color", c);
			node->add_color_override("resizer_color", c);
		}
	}

	List<AnimationNodeBlendTree::NodeConnection> connections;
	blend_tree->get_node_connections(&connections);

	for (List<AnimationNodeBlendTree::NodeConnection>::Element *E = connections.front(); E; E = E->next()) {
		StringName from = E->get().output_node;
		StringName to = E->get().input_node;
		int to_idx = E->get().input_index;

		graph->connect_node(from, 0, to, to_idx);
	}

	float graph_minimap_opacity = EditorSettings::get_singleton()->get("editors/visual_editors/minimap_opacity");
	graph->set_minimap_opacity(graph_minimap_opacity);
}

void AnimationNodeBlendTreeEditor::_file_opened(const String &p_file) {
	file_loaded = ResourceLoader::load(p_file);
	if (file_loaded.is_valid()) {
		_add_node(MENU_LOAD_FILE_CONFIRM);
	}
}

void AnimationNodeBlendTreeEditor::_add_node(int p_idx) {
	Ref<AnimationNode> anode;

	String base_name;

	if (p_idx == MENU_LOAD_FILE) {
		open_file->clear_filters();
		List<String> filters;
		ResourceLoader::get_recognized_extensions_for_type("AnimationNode", &filters);
		for (List<String>::Element *E = filters.front(); E; E = E->next()) {
			open_file->add_filter("*." + E->get());
		}
		open_file->popup_centered_ratio();
		return;
	} else if (p_idx == MENU_LOAD_FILE_CONFIRM) {
		anode = file_loaded;
		file_loaded.unref();
		base_name = anode->get_class();
	} else if (p_idx == MENU_PASTE) {
		anode = EditorSettings::get_singleton()->get_resource_clipboard();
		ERR_FAIL_COND(!anode.is_valid());
		base_name = anode->get_class();
	} else if (add_options[p_idx].type != String()) {
		AnimationNode *an = Object::cast_to<AnimationNode>(ClassDB::instance(add_options[p_idx].type));
		ERR_FAIL_COND(!an);
		anode = Ref<AnimationNode>(an);
		base_name = add_options[p_idx].name;
	} else {
		ERR_FAIL_COND(add_options[p_idx].script.is_null());
		String base_type = add_options[p_idx].script->get_instance_base_type();
		AnimationNode *an = Object::cast_to<AnimationNode>(ClassDB::instance(base_type));
		ERR_FAIL_COND(!an);
		anode = Ref<AnimationNode>(an);
		anode->set_script(add_options[p_idx].script.get_ref_ptr());
		base_name = add_options[p_idx].name;
	}

	Ref<AnimationNodeOutput> out = anode;
	if (out.is_valid()) {
		EditorNode::get_singleton()->show_warning(TTR("Output node can't be added to the blend tree."));
		return;
	}

	if (!from_node.empty() && anode->get_input_count() == 0) {
		from_node = "";
		return;
	}

	Point2 instance_pos = graph->get_scroll_ofs();
	if (use_popup_menu_position) {
		instance_pos += popup_menu_position;
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

	undo_redo->create_action(TTR("Add Node to BlendTree"));
	undo_redo->add_do_method(blend_tree.ptr(), "add_node", name, anode, instance_pos / EDSCALE);
	undo_redo->add_undo_method(blend_tree.ptr(), "remove_node", name);

	if (!from_node.empty()) {
		undo_redo->add_do_method(blend_tree.ptr(), "connect_node", name, 0, from_node);
		from_node = "";
	}
	if (!to_node.empty() && to_slot != -1) {
		undo_redo->add_do_method(blend_tree.ptr(), "connect_node", to_node, to_slot, name);
		to_node = "";
		to_slot = -1;
	}

	undo_redo->add_do_method(this, "_update_graph");
	undo_redo->add_undo_method(this, "_update_graph");
	undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_popup(bool p_has_input_ports, const Vector2 &p_popup_position, const Vector2 &p_node_position) {
	_update_options_menu(p_has_input_ports);
	use_popup_menu_position = true;
	popup_menu_position = p_popup_position;
	add_node->get_popup()->set_position(p_node_position);
	add_node->get_popup()->popup();
}

void AnimationNodeBlendTreeEditor::_popup_request(const Vector2 &p_position) {
	_popup(false, graph->get_local_mouse_position(), p_position);
}

void AnimationNodeBlendTreeEditor::_connection_to_empty(const String &p_from, int p_from_slot, const Vector2 &p_release_position) {
	Ref<AnimationNode> node = blend_tree->get_node(p_from);
	if (node.is_valid()) {
		from_node = p_from;
		_popup(true, p_release_position, graph->get_global_mouse_position());
	}
}

void AnimationNodeBlendTreeEditor::_connection_from_empty(const String &p_to, int p_to_slot, const Vector2 &p_release_position) {
	Ref<AnimationNode> node = blend_tree->get_node(p_to);
	if (node.is_valid()) {
		to_node = p_to;
		to_slot = p_to_slot;
		_popup(false, p_release_position, graph->get_global_mouse_position());
	}
}

void AnimationNodeBlendTreeEditor::_node_dragged(const Vector2 &p_from, const Vector2 &p_to, const StringName &p_which) {
	updating = true;
	undo_redo->create_action(TTR("Node Moved"));
	undo_redo->add_do_method(blend_tree.ptr(), "set_node_position", p_which, p_to / EDSCALE);
	undo_redo->add_undo_method(blend_tree.ptr(), "set_node_position", p_which, p_from / EDSCALE);
	undo_redo->add_do_method(this, "_update_graph");
	undo_redo->add_undo_method(this, "_update_graph");
	undo_redo->commit_action();
	updating = false;
}

void AnimationNodeBlendTreeEditor::_connection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index) {
	AnimationNodeBlendTree::ConnectionError err = blend_tree->can_connect_node(p_to, p_to_index, p_from);

	if (err != AnimationNodeBlendTree::CONNECTION_OK) {
		EditorNode::get_singleton()->show_warning(TTR("Unable to connect, port may be in use or connection may be invalid."));
		return;
	}

	undo_redo->create_action(TTR("Nodes Connected"));
	undo_redo->add_do_method(blend_tree.ptr(), "connect_node", p_to, p_to_index, p_from);
	undo_redo->add_undo_method(blend_tree.ptr(), "disconnect_node", p_to, p_to_index);
	undo_redo->add_do_method(this, "_update_graph");
	undo_redo->add_undo_method(this, "_update_graph");
	undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_disconnection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index) {
	graph->disconnect_node(p_from, p_from_index, p_to, p_to_index);

	updating = true;
	undo_redo->create_action(TTR("Nodes Disconnected"));
	undo_redo->add_do_method(blend_tree.ptr(), "disconnect_node", p_to, p_to_index);
	undo_redo->add_undo_method(blend_tree.ptr(), "connect_node", p_to, p_to_index, p_from);
	undo_redo->add_do_method(this, "_update_graph");
	undo_redo->add_undo_method(this, "_update_graph");
	undo_redo->commit_action();
	updating = false;
}

void AnimationNodeBlendTreeEditor::_anim_selected(int p_index, Array p_options, const String &p_node) {
	String option = p_options[p_index];

	Ref<AnimationNodeAnimation> anim = blend_tree->get_node(p_node);
	ERR_FAIL_COND(!anim.is_valid());

	undo_redo->create_action(TTR("Set Animation"));
	undo_redo->add_do_method(anim.ptr(), "set_animation", option);
	undo_redo->add_undo_method(anim.ptr(), "set_animation", anim->get_animation());
	undo_redo->add_do_method(this, "_update_graph");
	undo_redo->add_undo_method(this, "_update_graph");
	undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_delete_request(const String &p_which) {
	undo_redo->create_action(TTR("Delete Node"));
	undo_redo->add_do_method(blend_tree.ptr(), "remove_node", p_which);
	undo_redo->add_undo_method(blend_tree.ptr(), "add_node", p_which, blend_tree->get_node(p_which), blend_tree.ptr()->get_node_position(p_which));

	List<AnimationNodeBlendTree::NodeConnection> conns;
	blend_tree->get_node_connections(&conns);

	for (List<AnimationNodeBlendTree::NodeConnection>::Element *E = conns.front(); E; E = E->next()) {
		if (E->get().output_node == p_which || E->get().input_node == p_which) {
			undo_redo->add_undo_method(blend_tree.ptr(), "connect_node", E->get().input_node, E->get().input_index, E->get().output_node);
		}
	}

	undo_redo->add_do_method(this, "_update_graph");
	undo_redo->add_undo_method(this, "_update_graph");
	undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_delete_nodes_request(const Array &p_nodes) {
	List<StringName> to_erase;

	if (p_nodes.empty()) {
		for (int i = 0; i < graph->get_child_count(); i++) {
			GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
			if (gn) {
				if (gn->is_selected() && gn->is_close_button_visible()) {
					to_erase.push_back(gn->get_name());
				}
			}
		}
	} else {
		for (int i = 0; i < p_nodes.size(); i++) {
			to_erase.push_back(p_nodes[i]);
		}
	}

	if (to_erase.empty()) {
		return;
	}

	undo_redo->create_action(TTR("Delete Node(s)"));

	for (List<StringName>::Element *F = to_erase.front(); F; F = F->next()) {
		_delete_request(F->get());
	}

	undo_redo->commit_action();
}

void AnimationNodeBlendTreeEditor::_node_selected(Object *p_node) {
	GraphNode *gn = Object::cast_to<GraphNode>(p_node);
	ERR_FAIL_COND(!gn);

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
	ERR_FAIL_COND(!edited);

	NodePath edited_path = edited->get_metadata(0);
	bool filtered = edited->is_checked(0);

	updating = true;
	undo_redo->create_action(TTR("Change Filter"));
	undo_redo->add_do_method(_filter_edit.ptr(), "set_filter_path", edited_path, filtered);
	undo_redo->add_undo_method(_filter_edit.ptr(), "set_filter_path", edited_path, _filter_edit->is_path_filtered(edited_path));
	undo_redo->add_do_method(this, "_update_filters", _filter_edit);
	undo_redo->add_undo_method(this, "_update_filters", _filter_edit);
	undo_redo->commit_action();
	updating = false;
}

bool AnimationNodeBlendTreeEditor::_update_filters(const Ref<AnimationNode> &anode) {
	if (updating || _filter_edit != anode) {
		return false;
	}

	NodePath player_path = AnimationTreeEditor::get_singleton()->get_tree()->get_animation_player();

	if (!AnimationTreeEditor::get_singleton()->get_tree()->has_node(player_path)) {
		EditorNode::get_singleton()->show_warning(TTR("No animation player set, so unable to retrieve track names."));
		return false;
	}

	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(AnimationTreeEditor::get_singleton()->get_tree()->get_node(player_path));
	if (!player) {
		EditorNode::get_singleton()->show_warning(TTR("Player path set is invalid, so unable to retrieve track names."));
		return false;
	}

	Node *base = player->get_node(player->get_root());

	if (!base) {
		EditorNode::get_singleton()->show_warning(TTR("Animation player has no valid root node path, so unable to retrieve track names."));
		return false;
	}

	updating = true;

	Set<String> paths;
	HashMap<String, Set<String>> types;
	{
		List<StringName> animations;
		player->get_animation_list(&animations);

		for (List<StringName>::Element *E = animations.front(); E; E = E->next()) {
			Ref<Animation> anim = player->get_animation(E->get());
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
				if (!track_type_name.empty()) {
					types[track_path].insert(track_type_name);
				}
			}
		}
	}

	filter_enabled->set_pressed(anode->is_filter_enabled());
	filters->clear();
	TreeItem *root = filters->create_item();

	Map<String, TreeItem *> parenthood;

	for (Set<String>::Element *E = paths.front(); E; E = E->next()) {
		NodePath path = E->get();
		TreeItem *ti = nullptr;
		String accum;
		for (int i = 0; i < path.get_name_count(); i++) {
			String name = path.get_name(i);
			if (accum != String()) {
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

			Skeleton *skeleton = Object::cast_to<Skeleton>(node);
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
						ti->set_icon(0, get_icon("BoneAttachment", "EditorIcons"));
					} else {
						ti = parenthood[accum];
					}
				}

				ti->set_editable(0, true);
				ti->set_selectable(0, true);
				ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				ti->set_text(0, concat);
				ti->set_checked(0, anode->is_path_filtered(path));
				ti->set_icon(0, get_icon("BoneAttachment", "EditorIcons"));
				ti->set_metadata(0, path);

			} else {
				//just a property
				ti = filters->create_item(ti);
				ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				ti->set_text(0, concat);
				ti->set_editable(0, true);
				ti->set_selectable(0, true);
				ti->set_checked(0, anode->is_path_filtered(path));
				ti->set_metadata(0, path);
			}
		} else {
			if (ti) {
				//just a node, not a property track
				String types_text = "[";
				if (types.has(path)) {
					Set<String>::Element *F = types[path].front();
					types_text += F->get();
					while (F->next()) {
						F = F->next();
						types_text += " / " + F->get();
					}
				}
				types_text += "]";
				ti = filters->create_item(ti);
				ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				ti->set_text(0, types_text);
				ti->set_editable(0, true);
				ti->set_selectable(0, true);
				ti->set_checked(0, anode->is_path_filtered(path));
				ti->set_metadata(0, path);
			}
		}
	}

	updating = false;

	return true;
}

void AnimationNodeBlendTreeEditor::_edit_filters(const String &p_which) {
	Ref<AnimationNode> anode = blend_tree->get_node(p_which);
	ERR_FAIL_COND(!anode.is_valid());

	_filter_edit = anode;
	if (!_update_filters(anode)) {
		return;
	}

	filter_dialog->popup_centered_minsize(Size2(500, 500) * EDSCALE);
}

void AnimationNodeBlendTreeEditor::_removed_from_graph() {
	if (is_visible()) {
		EditorNode::get_singleton()->edit_item(nullptr);
	}
}

void AnimationNodeBlendTreeEditor::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE || p_what == NOTIFICATION_THEME_CHANGED) {
		error_panel->add_style_override("panel", get_stylebox("bg", "Tree"));
		error_label->add_color_override("font_color", get_color("error_color", "Editor"));

		if (p_what == NOTIFICATION_THEME_CHANGED && is_visible_in_tree()) {
			_update_graph();
		}
	}

	if (p_what == NOTIFICATION_PROCESS) {
		String error;

		if (!AnimationTreeEditor::get_singleton()->get_tree()->is_active()) {
			error = TTR("AnimationTree is inactive.\nActivate to enable playback, check node warnings if activation fails.");
		} else if (AnimationTreeEditor::get_singleton()->get_tree()->is_state_invalid()) {
			error = AnimationTreeEditor::get_singleton()->get_tree()->get_invalid_state_reason();
		}

		if (error != error_label->get_text()) {
			error_label->set_text(error);
			if (error != String()) {
				error_panel->show();
			} else {
				error_panel->hide();
			}
		}

		List<AnimationNodeBlendTree::NodeConnection> conns;
		blend_tree->get_node_connections(&conns);
		for (List<AnimationNodeBlendTree::NodeConnection>::Element *E = conns.front(); E; E = E->next()) {
			float activity = 0;
			StringName path = AnimationTreeEditor::get_singleton()->get_base_path() + E->get().input_node;
			if (AnimationTreeEditor::get_singleton()->get_tree() && !AnimationTreeEditor::get_singleton()->get_tree()->is_state_invalid()) {
				activity = AnimationTreeEditor::get_singleton()->get_tree()->get_connection_activity(path, E->get().input_index);
			}
			graph->set_connection_activity(E->get().output_node, 0, E->get().input_node, E->get().input_index, activity);
		}

		AnimationTree *graph_player = AnimationTreeEditor::get_singleton()->get_tree();
		AnimationPlayer *player = nullptr;
		if (graph_player->has_node(graph_player->get_animation_player())) {
			player = Object::cast_to<AnimationPlayer>(graph_player->get_node(graph_player->get_animation_player()));
		}

		if (player) {
			for (Map<StringName, ProgressBar *>::Element *E = animations.front(); E; E = E->next()) {
				Ref<AnimationNodeAnimation> an = blend_tree->get_node(E->key());
				if (an.is_valid()) {
					if (player->has_animation(an->get_animation())) {
						Ref<Animation> anim = player->get_animation(an->get_animation());
						if (anim.is_valid()) {
							E->get()->set_max(anim->get_length());
							//StringName path = AnimationTreeEditor::get_singleton()->get_base_path() + E->get().input_node;
							StringName time_path = AnimationTreeEditor::get_singleton()->get_base_path() + String(E->key()) + "/time";
							E->get()->set_value(AnimationTreeEditor::get_singleton()->get_tree()->get(time_path));
						}
					}
				}
			}
		}

		for (int i = 0; i < visible_properties.size(); i++) {
			visible_properties[i]->update_property();
		}
	}

	if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		set_process(is_visible_in_tree());
	}
}

void AnimationNodeBlendTreeEditor::_scroll_changed(const Vector2 &p_scroll) {
	if (updating) {
		return;
	}
	updating = true;
	blend_tree->set_graph_offset(p_scroll / EDSCALE);
	updating = false;
}

void AnimationNodeBlendTreeEditor::_bind_methods() {
	ClassDB::bind_method("_update_graph", &AnimationNodeBlendTreeEditor::_update_graph);
	ClassDB::bind_method("_add_node", &AnimationNodeBlendTreeEditor::_add_node);
	ClassDB::bind_method("_node_dragged", &AnimationNodeBlendTreeEditor::_node_dragged);
	ClassDB::bind_method("_node_renamed", &AnimationNodeBlendTreeEditor::_node_renamed);
	ClassDB::bind_method("_node_renamed_focus_out", &AnimationNodeBlendTreeEditor::_node_renamed_focus_out);
	ClassDB::bind_method("_connection_request", &AnimationNodeBlendTreeEditor::_connection_request);
	ClassDB::bind_method("_disconnection_request", &AnimationNodeBlendTreeEditor::_disconnection_request);
	ClassDB::bind_method("_node_selected", &AnimationNodeBlendTreeEditor::_node_selected);
	ClassDB::bind_method("_open_in_editor", &AnimationNodeBlendTreeEditor::_open_in_editor);
	ClassDB::bind_method("_scroll_changed", &AnimationNodeBlendTreeEditor::_scroll_changed);
	ClassDB::bind_method("_delete_request", &AnimationNodeBlendTreeEditor::_delete_request);
	ClassDB::bind_method("_delete_nodes_request", &AnimationNodeBlendTreeEditor::_delete_nodes_request);
	ClassDB::bind_method("_popup_request", &AnimationNodeBlendTreeEditor::_popup_request);
	ClassDB::bind_method("_edit_filters", &AnimationNodeBlendTreeEditor::_edit_filters);
	ClassDB::bind_method("_update_filters", &AnimationNodeBlendTreeEditor::_update_filters);
	ClassDB::bind_method("_filter_edited", &AnimationNodeBlendTreeEditor::_filter_edited);
	ClassDB::bind_method("_filter_toggled", &AnimationNodeBlendTreeEditor::_filter_toggled);
	ClassDB::bind_method("_removed_from_graph", &AnimationNodeBlendTreeEditor::_removed_from_graph);
	ClassDB::bind_method("_property_changed", &AnimationNodeBlendTreeEditor::_property_changed);
	ClassDB::bind_method("_file_opened", &AnimationNodeBlendTreeEditor::_file_opened);
	ClassDB::bind_method("_update_options_menu", &AnimationNodeBlendTreeEditor::_update_options_menu);
	ClassDB::bind_method("_connection_to_empty", &AnimationNodeBlendTreeEditor::_connection_to_empty);
	ClassDB::bind_method("_connection_from_empty", &AnimationNodeBlendTreeEditor::_connection_from_empty);

	ClassDB::bind_method("_anim_selected", &AnimationNodeBlendTreeEditor::_anim_selected);
}

AnimationNodeBlendTreeEditor *AnimationNodeBlendTreeEditor::singleton = nullptr;

void AnimationNodeBlendTreeEditor::_node_renamed(const String &p_text, Ref<AnimationNode> p_node) {
	if (blend_tree.is_null()) {
		return;
	}

	String prev_name = blend_tree->get_node_name(p_node);
	ERR_FAIL_COND(prev_name == String());
	GraphNode *gn = Object::cast_to<GraphNode>(graph->get_node(prev_name));
	ERR_FAIL_COND(!gn);

	const String &new_name = p_text;

	ERR_FAIL_COND(new_name == "" || new_name.find(".") != -1 || new_name.find("/") != -1);

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
	undo_redo->create_action(TTR("Node Renamed"));
	undo_redo->add_do_method(blend_tree.ptr(), "rename_node", prev_name, name);
	undo_redo->add_undo_method(blend_tree.ptr(), "rename_node", name, prev_name);
	undo_redo->add_do_method(AnimationTreeEditor::get_singleton()->get_tree(), "rename_parameter", base_path + prev_name, base_path + name);
	undo_redo->add_undo_method(AnimationTreeEditor::get_singleton()->get_tree(), "rename_parameter", base_path + name, base_path + prev_name);
	undo_redo->add_do_method(this, "_update_graph");
	undo_redo->add_undo_method(this, "_update_graph");
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

	List<AnimationNodeBlendTree::NodeConnection> connections;
	blend_tree->get_node_connections(&connections);

	for (List<AnimationNodeBlendTree::NodeConnection>::Element *E = connections.front(); E; E = E->next()) {
		StringName from = E->get().output_node;
		StringName to = E->get().input_node;
		int to_idx = E->get().input_index;

		graph->connect_node(from, 0, to, to_idx);
	}

	//update animations
	for (Map<StringName, ProgressBar *>::Element *E = animations.front(); E; E = E->next()) {
		if (E->key() == prev_name) {
			animations[new_name] = animations[prev_name];
			animations.erase(prev_name);
			break;
		}
	}

	_update_graph(); // Needed to update the signal connections with the new name.
}

void AnimationNodeBlendTreeEditor::_node_renamed_focus_out(Node *le, Ref<AnimationNode> p_node) {
	if (le == nullptr) {
		return; // The text_submitted signal triggered the graph update and freed the LineEdit.
	}
	_node_renamed(le->call("get_text"), p_node);
}

bool AnimationNodeBlendTreeEditor::can_edit(const Ref<AnimationNode> &p_node) {
	Ref<AnimationNodeBlendTree> bt = p_node;
	return bt.is_valid();
}

void AnimationNodeBlendTreeEditor::edit(const Ref<AnimationNode> &p_node) {
	if (blend_tree.is_valid()) {
		blend_tree->disconnect("removed_from_graph", this, "_removed_from_graph");
	}

	blend_tree = p_node;

	if (blend_tree.is_null()) {
		hide();
	} else {
		blend_tree->connect("removed_from_graph", this, "_removed_from_graph");

		_update_graph();
	}
}

AnimationNodeBlendTreeEditor::AnimationNodeBlendTreeEditor() {
	singleton = this;
	updating = false;
	use_popup_menu_position = false;

	graph = memnew(GraphEdit);
	add_child(graph);
	graph->add_valid_right_disconnect_type(0);
	graph->add_valid_left_disconnect_type(0);
	graph->set_v_size_flags(SIZE_EXPAND_FILL);
	graph->connect("connection_request", this, "_connection_request", varray(), CONNECT_DEFERRED);
	graph->connect("disconnection_request", this, "_disconnection_request", varray(), CONNECT_DEFERRED);
	graph->connect("node_selected", this, "_node_selected");
	graph->connect("scroll_offset_changed", this, "_scroll_changed");
	graph->connect("delete_nodes_request", this, "_delete_nodes_request");
	graph->connect("popup_request", this, "_popup_request");
	graph->connect("connection_to_empty", this, "_connection_to_empty");
	graph->connect("connection_from_empty", this, "_connection_from_empty");
	float graph_minimap_opacity = EditorSettings::get_singleton()->get("editors/visual_editors/minimap_opacity");
	graph->set_minimap_opacity(graph_minimap_opacity);

	VSeparator *vs = memnew(VSeparator);
	graph->get_zoom_hbox()->add_child(vs);
	graph->get_zoom_hbox()->move_child(vs, 0);

	add_node = memnew(MenuButton);
	graph->get_zoom_hbox()->add_child(add_node);
	add_node->set_text(TTR("Add Node..."));
	graph->get_zoom_hbox()->move_child(add_node, 0);
	add_node->get_popup()->connect("id_pressed", this, "_add_node");
	add_node->connect("about_to_show", this, "_update_options_menu", varray(false));

	add_options.push_back(AddOption("Animation", "AnimationNodeAnimation"));
	add_options.push_back(AddOption("OneShot", "AnimationNodeOneShot", 2));
	add_options.push_back(AddOption("Sub2", "AnimationNodeSub2", 2));
	add_options.push_back(AddOption("Add2", "AnimationNodeAdd2", 2));
	add_options.push_back(AddOption("Add3", "AnimationNodeAdd3", 3));
	add_options.push_back(AddOption("Blend2", "AnimationNodeBlend2", 2));
	add_options.push_back(AddOption("Blend3", "AnimationNodeBlend3", 3));
	add_options.push_back(AddOption("Seek", "AnimationNodeTimeSeek", 1));
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

	filter_enabled = memnew(CheckBox);
	filter_enabled->set_text(TTR("Enable Filtering"));
	filter_enabled->connect("pressed", this, "_filter_toggled");
	filter_vbox->add_child(filter_enabled);

	filters = memnew(Tree);
	filter_vbox->add_child(filters);
	filters->set_v_size_flags(SIZE_EXPAND_FILL);
	filters->set_hide_root(true);
	filters->connect("item_edited", this, "_filter_edited");

	open_file = memnew(EditorFileDialog);
	add_child(open_file);
	open_file->set_title(TTR("Open Animation Node"));
	open_file->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	open_file->connect("file_selected", this, "_file_opened");
	undo_redo = EditorNode::get_undo_redo();
}
