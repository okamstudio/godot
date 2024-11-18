/**************************************************************************/
/*  editor_debugger_tree.cpp                                              */
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

#include "editor_debugger_tree.h"

#include "editor/debugger/editor_debugger_node.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_toaster.h"
#include "editor/scene_tree_dock.h"
#include "scene/debugger/scene_debugger.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/packed_scene.h"
#include "servers/display_server.h"

EditorDebuggerTree::EditorDebuggerTree() {
	set_v_size_flags(SIZE_EXPAND_FILL);
	set_allow_rmb_select(true);
	set_select_mode(SELECT_MULTI);

	// Popup
	item_menu = memnew(PopupMenu);
	item_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorDebuggerTree::_item_menu_id_pressed));
	add_child(item_menu);

	// File Dialog
	file_dialog = memnew(EditorFileDialog);
	file_dialog->connect("file_selected", callable_mp(this, &EditorDebuggerTree::_file_selected));
	add_child(file_dialog);

	accept = memnew(AcceptDialog);
	add_child(accept);
}

void EditorDebuggerTree::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

			connect("multi_selected", callable_mp(this, &EditorDebuggerTree::_scene_tree_selection_changed));
			connect("nothing_selected", callable_mp(this, &EditorDebuggerTree::_scene_tree_nothing_selected));
			connect("item_collapsed", callable_mp(this, &EditorDebuggerTree::_scene_tree_folded));
			connect("item_mouse_selected", callable_mp(this, &EditorDebuggerTree::_scene_tree_rmb_selected));
		} break;

		case NOTIFICATION_ENTER_TREE: {
			update_icon_max_width();
		} break;
	}
}

void EditorDebuggerTree::_bind_methods() {
	ADD_SIGNAL(MethodInfo("objects_selected", PropertyInfo(Variant::ARRAY, "object_ids"), PropertyInfo(Variant::INT, "debugger")));
	ADD_SIGNAL(MethodInfo("selection_cleared", PropertyInfo(Variant::INT, "debugger")));
	ADD_SIGNAL(MethodInfo("save_node", PropertyInfo(Variant::INT, "object_id"), PropertyInfo(Variant::STRING, "filename"), PropertyInfo(Variant::INT, "debugger")));
	ADD_SIGNAL(MethodInfo("open"));
}

void EditorDebuggerTree::_scene_tree_selection_changed(TreeItem *p_item, int p_column, bool p_selected) {
	if (updating_scene_tree || !p_item) {
		return;
	}

	uint64_t id = uint64_t(p_item->get_metadata(0));
	if (p_selected) {
		if (inspected_object_ids.size() == (int)EDITOR_GET("debugger/max_node_selection")) {
			selection_surpassed_limit = true;
			p_item->deselect(0);
			return;
		}

		if (!inspected_object_ids.has(id)) {
			inspected_object_ids.append(id);
		}
	} else if (inspected_object_ids.has(id)) {
		inspected_object_ids.erase(id);
	}

	if (!notify_selection_queued) {
		callable_mp(this, &EditorDebuggerTree::_notify_selection_changed).call_deferred();
		notify_selection_queued = true;
	}
}

void EditorDebuggerTree::_scene_tree_nothing_selected() {
	deselect_all();
	inspected_object_ids.clear();
	emit_signal(SNAME("selection_cleared"), debugger_id);
}

void EditorDebuggerTree::_notify_selection_changed() {
	notify_selection_queued = false;

	if (inspected_object_ids.is_empty()) {
		emit_signal(SNAME("selection_cleared"), debugger_id);
	} else {
		emit_signal(SNAME("objects_selected"), inspected_object_ids.duplicate(), debugger_id);
	}

	if (selection_surpassed_limit) {
		selection_surpassed_limit = false;
		EditorToaster::get_singleton()->popup_str(vformat(TTR("Some remote nodes were not selected, as the configured maximum selection is %d. This can be changed at \"debugger/max_node_selection\" in the Editor Settings."), EDITOR_GET("debugger/max_node_selection")), EditorToaster::SEVERITY_WARNING);
	}
}

void EditorDebuggerTree::_scene_tree_folded(Object *p_obj) {
	if (updating_scene_tree) {
		return;
	}
	TreeItem *item = Object::cast_to<TreeItem>(p_obj);

	if (!item) {
		return;
	}

	ObjectID id = ObjectID(uint64_t(item->get_metadata(0)));
	if (unfold_cache.has(id)) {
		unfold_cache.erase(id);
	} else {
		unfold_cache.insert(id);
	}
}

void EditorDebuggerTree::_scene_tree_rmb_selected(const Vector2 &p_position, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}

	TreeItem *item = get_item_at_position(p_position);
	if (!item) {
		return;
	}

	item->select(0);

	item_menu->clear();
	item_menu->add_icon_item(get_editor_theme_icon(SNAME("CreateNewSceneFrom")), TTR("Save Branch as Scene..."), ITEM_MENU_SAVE_REMOTE_NODE);
	item_menu->add_icon_item(get_editor_theme_icon(SNAME("CopyNodePath")), TTR("Copy Node Path"), ITEM_MENU_COPY_NODE_PATH);
	item_menu->add_icon_item(get_editor_theme_icon(SNAME("Collapse")), TTR("Expand/Collapse Branch"), ITEM_MENU_EXPAND_COLLAPSE);
	item_menu->set_position(get_screen_position() + get_local_mouse_position());
	item_menu->reset_size();
	item_menu->popup();
}

/// Populates inspect_scene_tree given data in nodes as a flat list, encoded depth first.
///
/// Given a nodes array like [R,A,B,C,D,E] the following Tree will be generated, assuming
/// filter is an empty String, R and A child count are 2, B is 1 and C, D and E are 0.
///
/// R
/// |-A
/// | |-B
/// | | |-C
/// | |
/// | |-D
/// |
/// |-E
///
void EditorDebuggerTree::update_scene_tree(const SceneDebuggerTree *p_tree, int p_debugger) {
	set_hide_root(false);

	updating_scene_tree = true;
	const String last_path = get_selected_path();
	const String filter = SceneTreeDock::get_singleton()->get_filter();
	LocalVector<TreeItem *> select_items;
	bool hide_filtered_out_parents = EDITOR_GET("docks/scene_tree/hide_filtered_out_parents");

	bool should_scroll = scrolling_to_item || filter != last_filter;
	scrolling_to_item = false;
	TreeItem *scroll_item = nullptr;
	TypedArray<uint64_t> ids_present;

	// Nodes are in a flatten list, depth first. Use a stack of parents, avoid recursion.
	List<ParentItem> parents;
	for (const SceneDebuggerTree::RemoteNode &node : p_tree->nodes) {
		TreeItem *parent = nullptr;
		Pair<TreeItem *, TreeItem *> move_from_to;
		if (parents.size()) { // Find last parent.
			ParentItem &p = parents.front()->get();
			parent = p.tree_item;
			if (!(--p.child_count)) { // If no child left, remove it.
				parents.pop_front();

				if (hide_filtered_out_parents && !filter.is_subsequence_ofn(parent->get_text(0))) {
					if (parent == get_root()) {
						set_hide_root(true);
					} else {
						move_from_to.first = parent;
						// Find the closest ancestor that matches the filter.
						for (const ParentItem p2 : parents) {
							move_from_to.second = p2.tree_item;
							if (p2.matches_filter || move_from_to.second == get_root()) {
								break;
							}
						}

						if (!move_from_to.second) {
							move_from_to.second = get_root();
						}
					}
				}
			}
		}

		// Add this node.
		TreeItem *item = create_item(parent);
		item->set_text(0, node.name);
		if (node.scene_file_path.is_empty()) {
			item->set_tooltip_text(0, node.name + "\n" + TTR("Type:") + " " + node.type_name);
		} else {
			item->set_tooltip_text(0, node.name + "\n" + TTR("Instance:") + " " + node.scene_file_path + "\n" + TTR("Type:") + " " + node.type_name);
		}
		Ref<Texture2D> icon = EditorNode::get_singleton()->get_class_icon(node.type_name, "");
		if (icon.is_valid()) {
			item->set_icon(0, icon);
		}
		item->set_metadata(0, node.id);

		String current_path;
		if (parent) {
			current_path += (String)parent->get_meta("node_path");

			// Set current item as collapsed if necessary (root is never collapsed).
			if (!unfold_cache.has(node.id)) {
				item->set_collapsed(true);
			}
		}
		item->set_meta("node_path", current_path + "/" + item->get_text(0));

		// Select previously selected nodes.
		if (debugger_id == p_debugger) { // Can use remote id.
			if (inspected_object_ids.has(uint64_t(node.id))) {
				ids_present.append(node.id);

				if (selection_uncollapse_all) {
					selection_uncollapse_all = false;

					// Temporarily set to `false`, to allow caching the unfolds.
					updating_scene_tree = false;
					item->uncollapse_tree();
					updating_scene_tree = true;
				}

				select_items.push_back(item);
				if (should_scroll) {
					scroll_item = item;
				}
			}
		} else if (last_path == (String)item->get_meta("node_path")) { // Must use path.
			updating_scene_tree = false; // Force emission of new selections.
			select_items.push_back(item);
			if (should_scroll) {
				scroll_item = item;
			}
			updating_scene_tree = true;
		}

		// Add buttons.
		const Color remote_button_color = Color(1, 1, 1, 0.8);
		if (!node.scene_file_path.is_empty()) {
			String node_scene_file_path = node.scene_file_path;
			Ref<Texture2D> button_icon = get_editor_theme_icon(SNAME("InstanceOptions"));
			String tooltip = vformat(TTR("This node has been instantiated from a PackedScene file:\n%s\nClick to open the original file in the Editor."), node_scene_file_path);

			item->set_meta("scene_file_path", node_scene_file_path);
			item->add_button(0, button_icon, BUTTON_SUBSCENE, false, tooltip);
			item->set_button_color(0, item->get_button_count(0) - 1, remote_button_color);
		}

		if (node.view_flags & SceneDebuggerTree::RemoteNode::VIEW_HAS_VISIBLE_METHOD) {
			bool node_visible = node.view_flags & SceneDebuggerTree::RemoteNode::VIEW_VISIBLE;
			bool node_visible_in_tree = node.view_flags & SceneDebuggerTree::RemoteNode::VIEW_VISIBLE_IN_TREE;
			Ref<Texture2D> button_icon = get_editor_theme_icon(node_visible ? SNAME("GuiVisibilityVisible") : SNAME("GuiVisibilityHidden"));
			String tooltip = TTR("Toggle Visibility");

			item->set_meta("visible", node_visible);
			item->add_button(0, button_icon, BUTTON_VISIBILITY, false, tooltip);
			if (ClassDB::is_parent_class(node.type_name, "CanvasItem") || ClassDB::is_parent_class(node.type_name, "Node3D")) {
				item->set_button_color(0, item->get_button_count(0) - 1, node_visible_in_tree ? remote_button_color : Color(1, 1, 1, 0.6));
			} else {
				item->set_button_color(0, item->get_button_count(0) - 1, remote_button_color);
			}
		}

		// Add in front of the parents stack if children are expected.
		if (node.child_count) {
			parents.push_front(ParentItem(item, node.child_count, filter.is_subsequence_ofn(item->get_text(0))));
		} else {
			// Apply filters.
			while (parent) {
				const bool had_siblings = item->get_prev() || item->get_next();
				if (filter.is_subsequence_ofn(item->get_text(0))) {
					break; // Filter matches, must survive.
				}

				if (select_items.has(item) || scroll_item == item) {
					select_items.resize(select_items.size() - 1);
					scroll_item = nullptr;
				}
				parent->remove_child(item);
				memdelete(item);

				if (had_siblings) {
					break; // Parent must survive.
				}

				item = parent;
				parent = item->get_parent();
				// Check if parent expects more children.
				for (ParentItem &pair : parents) {
					if (pair.tree_item == item) {
						parent = nullptr;
						break; // Might have more children.
					}
				}
			}
		}

		// Move all children to the ancestor that matches the filter, if picked.
		if (move_from_to.first) {
			TreeItem *from = move_from_to.first;
			TypedArray<TreeItem> children = from->get_children();
			if (!children.is_empty()) {
				for (Variant &c : children) {
					TreeItem *ti = Object::cast_to<TreeItem>(c);
					from->remove_child(ti);
					move_from_to.second->add_child(ti);
				}

				from->get_parent()->remove_child(from);
				memdelete(from);
				if (select_items.has(from) || scroll_item == from) {
					select_items.erase(from);
					scroll_item = nullptr;
				}
			}
		}
	}

	inspected_object_ids = ids_present;

	debugger_id = p_debugger; // Needed by hook, could be avoided if every debugger had its own tree.

	for (TreeItem *item : select_items) {
		item->select(0);
	}
	if (scroll_item) {
		scroll_to_item(scroll_item, false);
	}

	last_filter = filter;
	updating_scene_tree = false;
}

void EditorDebuggerTree::select_nodes(const TypedArray<int64_t> &p_ids) {
	// Manually select, as the tree control may be out-of-date for some reason (e.g. not shown yet).
	selection_uncollapse_all = true;
	inspected_object_ids = p_ids;
	scrolling_to_item = true;

	if (!updating_scene_tree) {
		// Request a tree refresh.
		EditorDebuggerNode::get_singleton()->request_remote_tree();
	}
	// Set the value immediately, so no update flooding happens and causes a crash.
	updating_scene_tree = true;
}

void EditorDebuggerTree::clear_selection() {
	inspected_object_ids.clear();

	if (!updating_scene_tree) {
		// Request a tree refresh.
		EditorDebuggerNode::get_singleton()->request_remote_tree();
	}
	// Set the value immediately, so no update flooding happens and causes a crash.
	updating_scene_tree = true;
}

Variant EditorDebuggerTree::get_drag_data(const Point2 &p_point) {
	if (get_button_id_at_position(p_point) != -1) {
		return Variant();
	}

	TreeItem *selected = get_selected();
	if (!selected) {
		return Variant();
	}

	String path = selected->get_text(0);
	const int icon_size = get_theme_constant(SNAME("class_icon_size"), EditorStringName(Editor));

	HBoxContainer *hb = memnew(HBoxContainer);
	TextureRect *tf = memnew(TextureRect);
	tf->set_texture(selected->get_icon(0));
	tf->set_custom_minimum_size(Size2(icon_size, icon_size));
	tf->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	tf->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	hb->add_child(tf);
	Label *label = memnew(Label(path));
	hb->add_child(label);
	set_drag_preview(hb);

	if (!selected->get_parent() || !selected->get_parent()->get_parent()) {
		path = ".";
	} else {
		while (selected->get_parent()->get_parent() != get_root()) {
			selected = selected->get_parent();
			path = selected->get_text(0) + "/" + path;
		}
	}

	return vformat("\"%s\"", path);
}

void EditorDebuggerTree::update_icon_max_width() {
	add_theme_constant_override("icon_max_width", get_theme_constant("class_icon_size", EditorStringName(Editor)));
}

String EditorDebuggerTree::get_selected_path() {
	if (!get_selected()) {
		return "";
	}
	return get_selected()->get_meta("node_path");
}

void EditorDebuggerTree::_item_menu_id_pressed(int p_option) {
	switch (p_option) {
		case ITEM_MENU_SAVE_REMOTE_NODE: {
			file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
			file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);

			List<String> extensions;
			Ref<PackedScene> sd = memnew(PackedScene);
			ResourceSaver::get_recognized_extensions(sd, &extensions);
			file_dialog->clear_filters();
			for (const String &extension : extensions) {
				file_dialog->add_filter("*." + extension, extension.to_upper());
			}

			String filename = get_selected_path().get_file() + "." + extensions.front()->get().to_lower();
			file_dialog->set_current_path(filename);
			file_dialog->popup_file_dialog();
		} break;
		case ITEM_MENU_COPY_NODE_PATH: {
			String text = get_selected_path();
			if (text.is_empty()) {
				return;
			} else if (text == "/root") {
				text = ".";
			} else {
				text = text.replace("/root/", "");
				int slash = text.find_char('/');
				if (slash < 0) {
					text = ".";
				} else {
					text = text.substr(slash + 1);
				}
			}
			DisplayServer::get_singleton()->clipboard_set(text);
		} break;
		case ITEM_MENU_EXPAND_COLLAPSE: {
			TreeItem *s_item = get_selected();

			if (!s_item) {
				s_item = get_root();
				if (!s_item) {
					break;
				}
			}

			bool collapsed = s_item->is_any_collapsed();
			s_item->set_collapsed_recursive(!collapsed);

			ensure_cursor_is_visible();
		}
	}
}

void EditorDebuggerTree::_file_selected(const String &p_file) {
	if (inspected_object_ids.size() != 1) {
		accept->set_text(vformat(TTR("Saving the branch as a scene requires selecting only one node, but you have selected %d nodes."), inspected_object_ids.size()));
		accept->popup_centered();
		return;
	}

	emit_signal(SNAME("save_node"), inspected_object_ids[0], p_file, debugger_id);
}
