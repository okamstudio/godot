/**************************************************************************/
/*  node_dock.cpp                                                         */
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

#include "node_dock.h"

#include "editor/connections_dialog.h"
#include "editor/themes/editor_scale.h"

void NodeDock::show_groups() {
	groups_button->set_pressed(true);
	connections_button->set_pressed(false);
	groups->show();
	connections->hide();
}

void NodeDock::show_connections() {
	groups_button->set_pressed(false);
	connections_button->set_pressed(true);
	groups->hide();
	connections->show();
}

void NodeDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			connections_button->set_button_icon(get_editor_theme_icon(SNAME("Signals")));
			groups_button->set_button_icon(get_editor_theme_icon(SNAME("Groups")));
		} break;
	}
}

NodeDock *NodeDock::singleton = nullptr;

void NodeDock::update_lists() {
	connections->update_tree();
}

void NodeDock::set_node(Node *p_node) {
	connections->set_node(p_node);
	groups->set_current(p_node);

	if (p_node) {
		if (connections_button->is_pressed()) {
			connections->show();
		} else {
			groups->show();
		}

		mode_hb->show();
		select_a_node->hide();
	} else {
		connections->hide();
		groups->hide();
		mode_hb->hide();
		select_a_node->show();
	}
}

NodeDock::NodeDock() {
	singleton = this;

	set_name("Node");
	mode_hb = memnew(HBoxContainer);
	add_child(mode_hb);
	mode_hb->hide();

	connections_button = memnew(Button);
	connections_button->set_theme_type_variation(SceneStringName(FlatButton));
	connections_button->set_text(TTR("Signals"));
	connections_button->set_toggle_mode(true);
	connections_button->set_pressed(true);
	connections_button->set_h_size_flags(SIZE_EXPAND_FILL);
	connections_button->set_clip_text(true);
	mode_hb->add_child(connections_button);
	connections_button->connect(SceneStringName(pressed), callable_mp(this, &NodeDock::show_connections));

	groups_button = memnew(Button);
	groups_button->set_theme_type_variation(SceneStringName(FlatButton));
	groups_button->set_text(TTR("Groups"));
	groups_button->set_toggle_mode(true);
	groups_button->set_pressed(false);
	groups_button->set_h_size_flags(SIZE_EXPAND_FILL);
	groups_button->set_clip_text(true);
	mode_hb->add_child(groups_button);
	groups_button->connect(SceneStringName(pressed), callable_mp(this, &NodeDock::show_groups));

	connections = memnew(ConnectionsDock);
	add_child(connections);
	connections->set_v_size_flags(SIZE_EXPAND_FILL);
	connections->hide();

	groups = memnew(GroupsEditor);
	add_child(groups);
	groups->set_v_size_flags(SIZE_EXPAND_FILL);
	groups->hide();

	select_a_node = memnew(Label);
	select_a_node->set_text(TTR("Select a single node to edit its signals and groups."));
	select_a_node->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	select_a_node->set_v_size_flags(SIZE_EXPAND_FILL);
	select_a_node->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	select_a_node->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	select_a_node->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	add_child(select_a_node);
}

NodeDock::~NodeDock() {
	singleton = nullptr;
}
