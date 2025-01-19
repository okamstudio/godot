/**************************************************************************/
/*  groups_dock.cpp                                                       */
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

#include "groups_dock.h"

#include "core/io/config_file.h"
#include "editor/themes/editor_scale.h"

void GroupsDock::set_node(Node *p_node) {
	groups->set_current(p_node);

	if (p_node) {
		groups->show();
		select_a_node->hide();
	} else {
		groups->hide();
		select_a_node->show();
	}
}

GroupsDock::GroupsDock() {
	singleton = this;

	set_name("Groups");
	groups = memnew(GroupsEditor);
	add_child(groups);
	groups->set_v_size_flags(SIZE_EXPAND_FILL);
	groups->hide();

	select_a_node = memnew(Label);
	select_a_node->set_text(TTR("Select a single node to edit its groups."));
	select_a_node->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	select_a_node->set_v_size_flags(SIZE_EXPAND_FILL);
	select_a_node->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	select_a_node->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	select_a_node->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	add_child(select_a_node);
}

GroupsDock::~GroupsDock() {
	singleton = nullptr;
}
