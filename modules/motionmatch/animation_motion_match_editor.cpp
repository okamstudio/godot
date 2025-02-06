#include "animation_motion_match_editor.h"
#include <iostream>
#include <limits>

#ifdef TOOLS_ENABLED
#include "core/io/resource_loader.h"
#include "editor/editor_scale.h"

bool AnimationNodeMotionMatchEditor::can_edit(
		const Ref<AnimationNode> &p_node) {
	Ref<AnimationNodeMotionMatch> anmm = p_node;
	return anmm.is_valid();
}

void AnimationNodeMotionMatchEditor::edit(const Ref<AnimationNode> &p_node) {
	motion_match = p_node;

	if (motion_match.is_valid()) {
	}
}

void AnimationNodeMotionMatchEditor::_match_tracks_edited() {
	if (updating) {
		return;
	}

	TreeItem *edited = match_tracks->get_edited();
	ERR_FAIL_COND(!edited);

	NodePath edited_path = edited->get_metadata(0);
	bool matched = edited->is_checked(0);

	UndoRedo *undo_redo = EditorNode::get_singleton()->get_undo_redo();

	updating = true;
	undo_redo->create_action(TTR("Change Match Track"));
	if (matched) {
		undo_redo->add_do_method(motion_match.ptr(), "add_matching_track",
				edited_path);
		undo_redo->add_undo_method(motion_match.ptr(), "remove_matching_track",
				edited_path);
	} else {
		undo_redo->add_do_method(motion_match.ptr(), "remove_matching_track",
				edited_path);
		undo_redo->add_undo_method(motion_match.ptr(), "add_matching_track",
				edited_path);
	}
	undo_redo->add_do_method(this, "_update_match_tracks");
	undo_redo->add_undo_method(this, "_update_match_tracks");
	undo_redo->commit_action();
	updating = false;
}

void AnimationNodeMotionMatchEditor::_edit_match_tracks() {
	_update_match_tracks();
	motion_match->editing = true;
	match_tracks_dialog->popup_centered_clamped(Size2(500, 500) * EDSCALE);
}

void AnimationNodeMotionMatchEditor::_clear_tree() {
	motion_match->clear_keys();
}

void AnimationNodeMotionMatchEditor::_update_match_tracks() {
	if (!is_visible()) {
		return;
	}

	if (updating) {
		return;
	}

	/*Checking for errors*/

	NodePath player_path =
			AnimationTreeEditor::get_singleton()->get_tree()->get_animation_player();
	if (!AnimationTreeEditor::get_singleton()->get_tree()->has_node(
				player_path)) {
		EditorNode::get_singleton()->show_warning(
				TTR("No animation player set, so unable to retrieve track names."));
		return;
	}

	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(
			AnimationTreeEditor::get_singleton()->get_tree()->get_node(player_path));
	if (!player) {
		EditorNode::get_singleton()->show_warning(
				TTR("Player path set is invalid, so unable to retrieve track names."));
		return;
	}

	Node *base = player->get_node(player->get_root());
	if (!base) {
		EditorNode::get_singleton()->show_warning(
				TTR("Animation player has no valid root node path, so unable to "
					"retrieve track names."));
		return;
	}
	/**/

	/*Get the list of all bones and display it*/
	updating = true;

	Set<String> paths;
	List<StringName> animations;
	player->get_animation_list(&animations);

	for (List<StringName>::Element *E = animations.front(); E; E = E->next()) {
		Ref<Animation> anim = player->get_animation(E->get());
		for (int i = 0; i < anim->get_track_count(); i++) {
			paths.insert(anim->track_get_path(i));
		}
	}

	match_tracks->clear();
	TreeItem *root = match_tracks->create_item();

	Map<String, TreeItem *> parenthood;

	for (Set<String>::Element *E = paths.front(); E; E = E->next()) {
		NodePath path = E->get();
		TreeItem *ti = NULL;
		String accum;
		for (int i = 0; i < path.get_name_count(); i++) {
			String name = path.get_name(i);
			if (accum != String()) {
				accum += "/";
			}
			accum += name;
			if (!parenthood.has(accum)) {
				if (ti) {
					ti = match_tracks->create_item(ti);
				} else {
					ti = match_tracks->create_item(root);
				}
				parenthood[accum] = ti;
				ti->set_text(0, name);
				ti->set_selectable(0, false);
				ti->set_editable(0, false);
				if (base->has_node(accum)) {
					Node *node = base->get_node(accum);
					ti->set_icon(
							0, EditorNode::get_singleton()->get_object_icon(node, "Node"));
				}

			} else {
				ti = parenthood[accum];
			}
		}
		Node *node = NULL;
		if (base->has_node(accum)) {
			node = base->get_node(accum);
		}
		if (!node)
			continue; // no node, can't edit

		if (path.get_subname_count()) {
			String concat = path.get_concatenated_subnames();
			this->skeleton = Object::cast_to<Skeleton3D>(node);
			if (skeleton && skeleton->find_bone(concat) != -1) {
				// path in skeleton
				String bone = concat;
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
						ti = match_tracks->create_item(ti);
						parenthood[accum] = ti;
						ti->set_text(0, F->get());
						ti->set_selectable(0, false);
						ti->set_editable(0, false);
						ti->set_icon(0, get_theme_icon("BoneAttachment", "EditorIcons"));
					} else {
						ti = parenthood[accum];
					}
				}

				ti->set_editable(0, true);
				ti->set_selectable(0, true);
				ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				ti->set_text(0, concat);
				ti->set_checked(0, motion_match->is_matching_track(path));
				ti->set_icon(0, get_theme_icon("BoneAttachment", "EditorIcons"));
				ti->set_metadata(0, path);

			} else {
				// just a property
				ti = match_tracks->create_item(ti);
				ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				ti->set_text(0, concat);
				ti->set_editable(0, true);
				ti->set_selectable(0, true);
				ti->set_checked(0, motion_match->is_matching_track(path));
				ti->set_metadata(0, path);
			}
		} else {
			if (ti) {
				// just a node, likely call or animation track
				ti->set_editable(0, true);
				ti->set_selectable(0, true);
				ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
				ti->set_checked(0, motion_match->is_matching_track(path));
				ti->set_metadata(0, path);
			}
		}
	}
	/**/
	updating = false;
}

void AnimationNodeMotionMatchEditor::_update_tracks() {
	print_line(itos(motion_match->get_matching_tracks().size()));
	if (motion_match->get_matching_tracks().size() == 0) {
		EditorNode::get_singleton()->show_warning(
				TTR("Please select tracks to match!"));
	}
	NodePath player_path =
			AnimationTreeEditor::get_singleton()->get_tree()->get_animation_player();
	/*Checking for errors*/
	if (!AnimationTreeEditor::get_singleton()->get_tree()->has_node(
				player_path)) {
		EditorNode::get_singleton()->show_warning(
				TTR("No animation player set, so unable to retrieve track names."));
		return;
	}

	AnimationPlayer *player = Object::cast_to<AnimationPlayer>(
			AnimationTreeEditor::get_singleton()->get_tree()->get_node(player_path));
	if (!player) {
		EditorNode::get_singleton()->show_warning(
				TTR("Player path set is invalid, so unable to retrieve track names."));
		return;
	}

	if ((AnimationTreeEditor::get_singleton()
						->get_tree()
						->get_root_motion_track() == NodePath())) {
		EditorNode::get_singleton()->show_warning(
				TTR("No root motion track was set, unable to build database."));
		return;
	}

	/**/
	/*UPDATING DATABASE*/

	motion_match->clear_keys();
	motion_match->set_dim_len(9);
	List<StringName> Animations;
	player->get_animation_list(&Animations);
	const StringName t = "Tracker";
	for (int i = 0; i < Animations.size(); i++) {
		if (Animations[i] != t) {
			Ref<Animation> anim = player->get_animation(Animations[i]);
			int root = anim->find_track(AnimationTreeEditor::get_singleton()
												->get_tree()
												->get_root_motion_track());
			NodePath root_motion_track = AnimationTreeEditor::get_singleton()->get_tree()->get_root_motion_track();
			int max_count =
					fill_tracks(player, anim.ptr(), root_motion_track);

			motion_match->delta_time =
					anim->track_get_key_time(max_key_track, max_count - 1) / max_count;

			for (int j = 0; j < max_count - snap_x->get_value(); j++) {
				float x = 0;
				float z = 0;
				for (int p = 0; p < 2; p++) {
					x += Math::pow(-1.0, double(p + 1)) *
						 Vector3(Dictionary(anim->track_get_key_value(root, j + p))
										 .get("location", Variant()))[0];
					z += Math::pow(-1.0, double(p + 1)) *
						 Vector3(Dictionary(anim->track_get_key_value(root, j + p))
										 .get("location", Variant()))[2];
				}

				frame_model *key = new frame_model;
				for (int y = 0; y < motion_match->get_matching_tracks().size(); y++) {
					int track = anim->find_track(motion_match->get_matching_tracks()[y]);
					Vector3 loc = Vector3(Dictionary(anim->track_get_key_value(track, j))
												  .get("location", Variant()));
					Vector<float> arr = {};
					for (int l = 0; l < snap_x->get_value(); l++) {
						if (l != 1) {
							arr.append(loc[l]);
						}
					}
					key->bone_data->append(arr);
				}
				Vector3 r_loc = Vector3(Dictionary(anim->track_get_key_value(root, j))
												.get("location", Variant()));
				for (int k = 0; k < snap_x->get_value(); k++) {
					Vector3 loc =
							Vector3(Dictionary(anim->track_get_key_value(root, j + k))
											.get("location", Variant()));
					for (int l = 0; l < 3; l++) {
						if (l != 1) {
							key->traj.append((loc[l] - r_loc[l]) * 10);
						}
					}
				}

				key->time = anim->track_get_key_time(root, j + 1);
				key->anim_num = i;
				keys->append(key);
			}
		}
	}
	motion_match->editing = false;
	motion_match->set_keys_data(keys);
	motion_match->skeleton = skeleton;
	if (motion_match->get_matching_tracks().size() != 0) {
		motion_match->done = true;
	}
	print_line("DONE");
	/**/
}

void AnimationNodeMotionMatchEditor::_bind_methods() {
	ClassDB::bind_method("_match_tracks_edited",
			&AnimationNodeMotionMatchEditor::_match_tracks_edited);
	ClassDB::bind_method("_update_match_tracks",
			&AnimationNodeMotionMatchEditor::_update_match_tracks);
	ClassDB::bind_method("_edit_match_tracks",
			&AnimationNodeMotionMatchEditor::_edit_match_tracks);
	ClassDB::bind_method("_update_tracks",
			&AnimationNodeMotionMatchEditor::_update_tracks);
	ClassDB::bind_method("_clear_tree",
			&AnimationNodeMotionMatchEditor::_clear_tree);
}
AnimationNodeMotionMatchEditor::AnimationNodeMotionMatchEditor() {
	match_tracks_dialog = memnew(AcceptDialog);
	add_child(match_tracks_dialog);
	match_tracks_dialog->set_title(TTR("Tracks to Match:"));

	VBoxContainer *match_tracks_vbox = memnew(VBoxContainer);
	match_tracks_dialog->add_child(match_tracks_vbox);

	match_tracks = memnew(Tree);
	match_tracks_vbox->add_child(match_tracks);
	match_tracks->set_v_size_flags(SIZE_EXPAND_FILL);
	match_tracks->set_hide_root(true);
	match_tracks->connect_compat("item_edited", this, "_match_tracks_edited");

	edit_match_tracks = memnew(Button("Edit Matching Tracks"));
	add_child(edit_match_tracks);
	edit_match_tracks->connect_compat("pressed", this, "_edit_match_tracks");

	snap_x = memnew(SpinBox);
	add_child(snap_x);
	snap_x->set_prefix("Samples:");
	snap_x->set_min(1);
	snap_x->set_step(1);
	snap_x->set_max(100);

	update_tracks = memnew(Button("Update DataBase"));
	add_child(update_tracks);
	update_tracks->connect_compat("pressed", this, "_update_tracks");

	HBoxContainer *velocity_vbox = memnew(HBoxContainer);
	Label *l = memnew(Label);

	l->set_text("Velocity");
	velocity_vbox->add_child(l);

	updating = false;
}

int AnimationNodeMotionMatchEditor::fill_tracks(AnimationPlayer *player,
		Animation *anim,
		NodePath &root) {
	int max_keys = 0;
	Vector<NodePath> tracks_tf = motion_match->get_matching_tracks();
	tracks_tf.push_back(root);
	for (int i = 0; i < tracks_tf.size(); i++) {
		if (anim->track_get_key_count(anim->find_track(tracks_tf[i])) >
				anim->track_get_key_count(anim->find_track(tracks_tf[max_keys]))) {
			max_keys = i;
		}
	}
	for (int i = 0;
			i < anim->track_get_key_count(anim->find_track(tracks_tf[max_keys]));
			i++) {
		float min_time =
				anim->track_get_key_time(anim->find_track(tracks_tf[0]), i);
		for (int p = 0; p < tracks_tf.size(); p++) {
			if (anim->track_get_key_time(anim->find_track(tracks_tf[p]), i) <
					min_time) {
				min_time = anim->track_get_key_time(anim->find_track(tracks_tf[p]), i);
			}
		}
		for (int p = 0; p < tracks_tf.size(); p++) {
			if (anim->track_get_key_time(anim->find_track(tracks_tf[p]), i) >
					min_time) {
				Vector3 t1;
				Quat t2;
				Vector3 t3;
				anim->transform_track_interpolate(anim->find_track(tracks_tf[p]),
						min_time, &t1, &t2, &t3);
				anim->transform_track_insert_key(anim->find_track(tracks_tf[p]),
						min_time, t1, t2, t3);
			}
		}
	}
	max_key_track = anim->find_track(tracks_tf[max_keys]);
	return anim->track_get_key_count(anim->find_track(tracks_tf[max_keys]));
}

#endif
