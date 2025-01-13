/**************************************************************************/
/*  graph_edit.compat.inc                                                 */
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

#ifndef DISABLE_DEPRECATED

bool GraphEdit::_is_arrange_nodes_button_hidden_bind_compat_81582() const {
	return !is_showing_arrange_button();
}

void GraphEdit::_set_arrange_nodes_button_hidden_bind_compat_81582(bool p_enable) {
	set_show_arrange_button(!p_enable);
}

PackedVector2Array GraphEdit::_get_connection_line_bind_compat_86158(const Vector2 &p_from, const Vector2 &p_to) {
	return get_connection_line(p_from, p_to);
}

Error GraphEdit::_connect_node_bind_compat_97449(const StringName &p_from, int p_from_port, const StringName &p_to, int p_to_port) {
	return connect_node(p_from, p_from_port, p_to, p_to_port);
}

void GraphEdit::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("is_arrange_nodes_button_hidden"), &GraphEdit::_is_arrange_nodes_button_hidden_bind_compat_81582);
	ClassDB::bind_compatibility_method(D_METHOD("set_arrange_nodes_button_hidden", "enable"), &GraphEdit::_set_arrange_nodes_button_hidden_bind_compat_81582);
	ClassDB::bind_compatibility_method(D_METHOD("get_connection_line", "from_node", "to_node"), &GraphEdit::_get_connection_line_bind_compat_86158);
	ClassDB::bind_compatibility_method(D_METHOD("connect_node", "from_node", "from_port", "to_node", "to_port"), &GraphEdit::_connect_node_bind_compat_97449);
}

#endif
