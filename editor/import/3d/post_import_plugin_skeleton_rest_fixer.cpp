/**************************************************************************/
/*  post_import_plugin_skeleton_rest_fixer.cpp                            */
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

#include "post_import_plugin_skeleton_rest_fixer.h"

#include "editor/import/3d/scene_import_settings.h"
#include "scene/3d/bone_attachment_3d.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/retarget_modifier_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/bone_map.h"

void PostImportPluginSkeletonRestFixer::get_internal_import_options(InternalImportCategory p_category, List<ResourceImporter::ImportOption> *r_options) {
	if (p_category == INTERNAL_IMPORT_CATEGORY_SKELETON_3D_NODE) {
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "retarget/rest_fixer/apply_node_transforms"), true));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "retarget/rest_fixer/normalize_position_tracks"), true));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "retarget/rest_fixer/reset_all_bone_poses_after_import"), true));

		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "retarget/rest_fixer/retarget_method", PROPERTY_HINT_ENUM, "None,Overwrite Axis,Use Retarget Modifier", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), 1));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "retarget/rest_fixer/keep_global_rest_on_leftovers"), true));
		String skeleton_bones_must_be_renamed_warning = String(
				"The skeleton modifier option uses SkeletonProfile as a list of bone names and retargets by name matching. Without renaming, retargeting by modifier will not work and the track path of the animation will be broken and it will be not playbacked correctly."); // TODO: translate.
		r_options->push_back(ResourceImporter::ImportOption(
				PropertyInfo(
						Variant::STRING, U"retarget/rest_fixer/\u26A0_validation_warning/skeleton_bones_must_be_renamed",
						PROPERTY_HINT_MULTILINE_TEXT, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY),
				Variant(skeleton_bones_must_be_renamed_warning)));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "retarget/rest_fixer/use_global_pose"), true));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::STRING, "retarget/rest_fixer/original_skeleton_name"), "OriginalSkeleton"));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "retarget/rest_fixer/fix_silhouette/enable", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), false));
		// TODO: PostImportPlugin need to be implemented such as validate_option(PropertyInfo &property, const Dictionary &p_options).
		// get_internal_option_visibility() is not sufficient because it can only retrieve options implemented in the core and can only read option values.
		// r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::ARRAY, "retarget/rest_fixer/filter", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::STRING_NAME, PROPERTY_HINT_ENUM, "Hips,Spine,Chest")), Array()));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::ARRAY, "retarget/rest_fixer/fix_silhouette/filter", PROPERTY_HINT_ARRAY_TYPE, vformat("%s:", Variant::STRING_NAME)), Array()));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "retarget/rest_fixer/fix_silhouette/threshold"), 15));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "retarget/rest_fixer/fix_silhouette/base_height_adjustment", PROPERTY_HINT_RANGE, "-1,1,0.01"), 0.0));
	}
}

Variant PostImportPluginSkeletonRestFixer::get_internal_option_visibility(InternalImportCategory p_category, const String &p_scene_import_type, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	if (p_category == INTERNAL_IMPORT_CATEGORY_SKELETON_3D_NODE) {
		if (p_option.begins_with("retarget/rest_fixer/fix_silhouette/")) {
			if (!bool(p_options["retarget/rest_fixer/fix_silhouette/enable"])) {
				if (!p_option.ends_with("enable")) {
					return false;
				}
			}
		} else if (p_option == "retarget/rest_fixer/keep_global_rest_on_leftovers") {
			return int(p_options["retarget/rest_fixer/retarget_method"]) == 1;
		} else if (p_option == "retarget/rest_fixer/original_skeleton_name" || p_option == "retarget/rest_fixer/use_global_pose") {
			return int(p_options["retarget/rest_fixer/retarget_method"]) == 2;
		} else if (p_option.begins_with("retarget/") && p_option.ends_with("skeleton_bones_must_be_renamed")) {
			return int(p_options["retarget/rest_fixer/retarget_method"]) == 2 && bool(p_options["retarget/bone_renamer/rename_bones"]) == false;
		}
	}
	return true;
}

void PostImportPluginSkeletonRestFixer::internal_process(InternalImportCategory p_category, Node *p_base_scene, Node *p_node, Ref<Resource> p_resource, const Dictionary &p_options) {
	if (p_category == INTERNAL_IMPORT_CATEGORY_SKELETON_3D_NODE) {
		// Prepare objects.
		Object *map = p_options["retarget/bone_map"].get_validated_object();
		if (!map) {
			return;
		}
		BoneMap *bone_map = Object::cast_to<BoneMap>(map);
		Ref<SkeletonProfile> profile = bone_map->get_profile();
		if (!profile.is_valid()) {
			return;
		}
		Skeleton3D *src_skeleton = Object::cast_to<Skeleton3D>(p_node);
		if (!src_skeleton) {
			return;
		}

		bool is_renamed = bool(p_options["retarget/bone_renamer/rename_bones"]);
		Array filter = p_options["retarget/rest_fixer/fix_silhouette/filter"];
		bool is_rest_changed = false;

		// Build profile skeleton.
		Skeleton3D *prof_skeleton = memnew(Skeleton3D);
		{
			int prof_bone_len = profile->get_bone_size();
			// Add single bones.
			for (int i = 0; i < prof_bone_len; i++) {
				prof_skeleton->add_bone(profile->get_bone_name(i));
				prof_skeleton->set_bone_rest(i, profile->get_reference_pose(i));
			}
			// Set parents.
			for (int i = 0; i < prof_bone_len; i++) {
				int parent = profile->find_bone(profile->get_bone_parent(i));
				if (parent >= 0) {
					prof_skeleton->set_bone_parent(i, parent);
				}
			}
		}

		// Get global transform.
		Transform3D global_transform;
		if (bool(p_options["retarget/rest_fixer/apply_node_transforms"])) {
			Node *pr = src_skeleton;
			while (pr) {
				Node3D *pr3d = Object::cast_to<Node3D>(pr);
				if (pr3d) {
					global_transform = pr3d->get_transform() * global_transform;
					pr3d->set_transform(Transform3D());
				}
				pr = pr->get_parent();
			}
			global_transform.origin = Vector3(); // Translation by a Node is not a bone animation, so the retargeted model should be at the origin.
		}

		// Apply node transforms.
		if (bool(p_options["retarget/rest_fixer/apply_node_transforms"])) {
			Vector3 scl = global_transform.basis.get_scale_global();

			Vector<int> bones_to_process = src_skeleton->get_parentless_bones();
			for (int i = 0; i < bones_to_process.size(); i++) {
				src_skeleton->set_bone_rest(bones_to_process[i], global_transform.orthonormalized() * src_skeleton->get_bone_rest(bones_to_process[i]));

				src_skeleton->set_bone_pose_position(bones_to_process[i], global_transform.orthonormalized().xform(src_skeleton->get_bone_pose_position(bones_to_process[i])));
				src_skeleton->set_bone_pose_rotation(bones_to_process[i], global_transform.basis.get_rotation_quaternion() * src_skeleton->get_bone_pose_rotation(bones_to_process[i]));
				src_skeleton->set_bone_pose_scale(bones_to_process[i], (global_transform.orthonormalized().basis * Basis().scaled(src_skeleton->get_bone_pose_scale((bones_to_process[i])))).get_scale());
			}

			while (bones_to_process.size() > 0) {
				int src_idx = bones_to_process[0];
				bones_to_process.erase(src_idx);
				Vector<int> src_children = src_skeleton->get_bone_children(src_idx);
				for (int i = 0; i < src_children.size(); i++) {
					bones_to_process.push_back(src_children[i]);
				}
				src_skeleton->set_bone_rest(src_idx, Transform3D(src_skeleton->get_bone_rest(src_idx).basis, src_skeleton->get_bone_rest(src_idx).origin * scl));
				src_skeleton->set_bone_pose_position(src_idx, src_skeleton->get_bone_pose_position(src_idx) * scl);
			}

			// Fix animation by changing node transform.
			bones_to_process = src_skeleton->get_parentless_bones();
			{
				TypedArray<Node> nodes = p_base_scene->find_children("*", "AnimationPlayer");
				while (nodes.size()) {
					AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(nodes.pop_back());
					List<StringName> anims;
					ap->get_animation_list(&anims);
					for (const StringName &name : anims) {
						Ref<Animation> anim = ap->get_animation(name);
						int track_len = anim->get_track_count();
						for (int i = 0; i < track_len; i++) {
							if (anim->track_get_path(i).get_subname_count() != 1 || !(anim->track_get_type(i) == Animation::TYPE_POSITION_3D || anim->track_get_type(i) == Animation::TYPE_ROTATION_3D || anim->track_get_type(i) == Animation::TYPE_SCALE_3D)) {
								continue;
							}

							if (anim->track_is_compressed(i)) {
								continue; // Shouldn't occur in internal_process().
							}

							String track_path = String(anim->track_get_path(i).get_concatenated_names());
							Node *node = (ap->get_node(ap->get_root_node()))->get_node(NodePath(track_path));
							ERR_CONTINUE(!node);

							Skeleton3D *track_skeleton = Object::cast_to<Skeleton3D>(node);
							if (!track_skeleton || track_skeleton != src_skeleton) {
								continue;
							}

							StringName bn = anim->track_get_path(i).get_subname(0);
							if (!bn) {
								continue;
							}

							int bone_idx = src_skeleton->find_bone(bn);
							int key_len = anim->track_get_key_count(i);
							if (anim->track_get_type(i) == Animation::TYPE_POSITION_3D) {
								if (bones_to_process.has(bone_idx)) {
									for (int j = 0; j < key_len; j++) {
										Vector3 ps = static_cast<Vector3>(anim->track_get_key_value(i, j));
										anim->track_set_key_value(i, j, global_transform.basis.xform(ps) + global_transform.origin);
									}
								} else {
									for (int j = 0; j < key_len; j++) {
										Vector3 ps = static_cast<Vector3>(anim->track_get_key_value(i, j));
										anim->track_set_key_value(i, j, ps * scl);
									}
								}
							} else if (bones_to_process.has(bone_idx)) {
								if (anim->track_get_type(i) == Animation::TYPE_ROTATION_3D) {
									for (int j = 0; j < key_len; j++) {
										Quaternion qt = static_cast<Quaternion>(anim->track_get_key_value(i, j));
										anim->track_set_key_value(i, j, global_transform.basis.get_rotation_quaternion() * qt);
									}
								} else {
									for (int j = 0; j < key_len; j++) {
										Basis sc = Basis().scaled(static_cast<Vector3>(anim->track_get_key_value(i, j)));
										anim->track_set_key_value(i, j, (global_transform.orthonormalized().basis * sc).get_scale());
									}
								}
							}
						}
					}
				}
			}

			is_rest_changed = true;
		}

		// Complement Rotation track for compatibility between different rests.
		{
			TypedArray<Node> nodes = p_base_scene->find_children("*", "AnimationPlayer");
			while (nodes.size()) {
				AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(nodes.pop_back());
				List<StringName> anims;
				ap->get_animation_list(&anims);
				for (const StringName &name : anims) {
					if (String(name).contains_char('/')) {
						continue; // Avoid animation library which may be created by importer dynamically.
					}

					Ref<Animation> anim = ap->get_animation(name);
					int track_len = anim->get_track_count();

					// Detect does the animation have skeleton's TRS track.
					String track_path;
					bool found_skeleton = false;
					for (int i = 0; i < track_len; i++) {
						if (anim->track_get_path(i).get_subname_count() != 1 || !(anim->track_get_type(i) == Animation::TYPE_POSITION_3D || anim->track_get_type(i) == Animation::TYPE_ROTATION_3D || anim->track_get_type(i) == Animation::TYPE_SCALE_3D)) {
							continue;
						}
						track_path = String(anim->track_get_path(i).get_concatenated_names());
						Node *node = (ap->get_node(ap->get_root_node()))->get_node(NodePath(track_path));
						if (node) {
							Skeleton3D *track_skeleton = Object::cast_to<Skeleton3D>(node);
							if (track_skeleton && track_skeleton == src_skeleton) {
								found_skeleton = true;
								break;
							}
						}
					}

					if (!found_skeleton) {
						continue;
					}

					// Search and insert rot track if it doesn't exist.
					for (int prof_idx = 0; prof_idx < prof_skeleton->get_bone_count(); prof_idx++) {
						String bone_name = is_renamed ? prof_skeleton->get_bone_name(prof_idx) : String(bone_map->get_skeleton_bone_name(prof_skeleton->get_bone_name(prof_idx)));
						if (bone_name.is_empty()) {
							continue;
						}
						int src_idx = src_skeleton->find_bone(bone_name);
						if (src_idx == -1) {
							continue;
						}
						String insert_path = track_path + ":" + bone_name;
						int rot_track = anim->find_track(insert_path, Animation::TYPE_ROTATION_3D);
						if (rot_track == -1) {
							int track = anim->add_track(Animation::TYPE_ROTATION_3D);
							anim->track_set_path(track, insert_path);
							anim->track_set_imported(track, true);
							anim->rotation_track_insert_key(track, 0, src_skeleton->get_bone_rest(src_idx).basis.get_rotation_quaternion());
						}
					}
				}
			}
		}

		// Fix silhouette.
		Vector<Transform3D> silhouette_diff; // Transform values to be ignored when overwrite axis.
		silhouette_diff.resize(src_skeleton->get_bone_count());
		Transform3D *silhouette_diff_w = silhouette_diff.ptrw();
		LocalVector<Transform3D> pre_silhouette_skeleton_global_rest;
		for (int i = 0; i < src_skeleton->get_bone_count(); i++) {
			pre_silhouette_skeleton_global_rest.push_back(src_skeleton->get_bone_global_rest(i));
		}
		if (bool(p_options["retarget/rest_fixer/fix_silhouette/enable"])) {
			Vector<int> bones_to_process = prof_skeleton->get_parentless_bones();
			while (bones_to_process.size() > 0) {
				int prof_idx = bones_to_process[0];
				bones_to_process.erase(prof_idx);
				Vector<int> prof_children = prof_skeleton->get_bone_children(prof_idx);
				for (int i = 0; i < prof_children.size(); i++) {
					bones_to_process.push_back(prof_children[i]);
				}

				// Calc virtual/looking direction with origins.
				bool is_filtered = false;
				for (int i = 0; i < filter.size(); i++) {
					if (String(filter[i]) == prof_skeleton->get_bone_name(prof_idx)) {
						is_filtered = true;
						break;
					}
				}
				if (is_filtered) {
					continue;
				}

				int src_idx = src_skeleton->find_bone(is_renamed ? prof_skeleton->get_bone_name(prof_idx) : String(bone_map->get_skeleton_bone_name(prof_skeleton->get_bone_name(prof_idx))));
				if (src_idx < 0 || profile->get_tail_direction(prof_idx) == SkeletonProfile::TAIL_DIRECTION_END) {
					continue;
				}
				Vector3 prof_tail;
				Vector3 src_tail;
				if (profile->get_tail_direction(prof_idx) == SkeletonProfile::TAIL_DIRECTION_AVERAGE_CHILDREN) {
					PackedInt32Array prof_bone_children = prof_skeleton->get_bone_children(prof_idx);
					int children_size = prof_bone_children.size();
					if (children_size == 0) {
						continue;
					}
					bool exist_all_children = true;
					for (int i = 0; i < children_size; i++) {
						int prof_child_idx = prof_bone_children[i];
						int src_child_idx = src_skeleton->find_bone(is_renamed ? prof_skeleton->get_bone_name(prof_child_idx) : String(bone_map->get_skeleton_bone_name(prof_skeleton->get_bone_name(prof_child_idx))));
						if (src_child_idx < 0) {
							exist_all_children = false;
							break;
						}
						prof_tail = prof_tail + prof_skeleton->get_bone_global_rest(prof_child_idx).origin;
						src_tail = src_tail + src_skeleton->get_bone_global_rest(src_child_idx).origin;
					}
					if (!exist_all_children) {
						continue;
					}
					prof_tail = prof_tail / children_size;
					src_tail = src_tail / children_size;
				}
				if (profile->get_tail_direction(prof_idx) == SkeletonProfile::TAIL_DIRECTION_SPECIFIC_CHILD) {
					int prof_tail_idx = prof_skeleton->find_bone(profile->get_bone_tail(prof_idx));
					if (prof_tail_idx < 0) {
						continue;
					}
					int src_tail_idx = src_skeleton->find_bone(is_renamed ? prof_skeleton->get_bone_name(prof_tail_idx) : String(bone_map->get_skeleton_bone_name(prof_skeleton->get_bone_name(prof_tail_idx))));
					if (src_tail_idx < 0) {
						continue;
					}
					prof_tail = prof_skeleton->get_bone_global_rest(prof_tail_idx).origin;
					src_tail = src_skeleton->get_bone_global_rest(src_tail_idx).origin;
				}

				Vector3 prof_head = prof_skeleton->get_bone_global_rest(prof_idx).origin;
				Vector3 src_head = src_skeleton->get_bone_global_rest(src_idx).origin;

				Vector3 prof_dir = prof_tail - prof_head;
				Vector3 src_dir = src_tail - src_head;

				// Rotate rest.
				if (Math::abs(Math::rad_to_deg(src_dir.angle_to(prof_dir))) > float(p_options["retarget/rest_fixer/fix_silhouette/threshold"])) {
					// Get rotation difference.
					Vector3 up_vec; // Need to rotate other than roll axis.
					switch (Vector3(abs(src_dir.x), abs(src_dir.y), abs(src_dir.z)).min_axis_index()) {
						case Vector3::AXIS_X: {
							up_vec = Vector3(1, 0, 0);
						} break;
						case Vector3::AXIS_Y: {
							up_vec = Vector3(0, 1, 0);
						} break;
						case Vector3::AXIS_Z: {
							up_vec = Vector3(0, 0, 1);
						} break;
					}
					Basis src_b;
					src_b = src_b.looking_at(src_dir, up_vec);
					Basis prof_b;
					prof_b = src_b.looking_at(prof_dir, up_vec);
					if (prof_b.is_equal_approx(Basis())) {
						continue; // May not need to rotate.
					}
					Basis diff_b = prof_b * src_b.inverse();

					// Apply rotation difference as global transform to skeleton.
					Basis src_pg;
					int src_parent = src_skeleton->get_bone_parent(src_idx);
					if (src_parent >= 0) {
						src_pg = src_skeleton->get_bone_global_rest(src_parent).basis;
					}
					Transform3D fixed_rest = Transform3D(src_pg.inverse() * diff_b * src_pg * src_skeleton->get_bone_rest(src_idx).basis, src_skeleton->get_bone_rest(src_idx).origin);
					src_skeleton->set_bone_rest(src_idx, fixed_rest);
				}
			}

			// Adjust scale base bone height.
			float base_adjustment = float(p_options["retarget/rest_fixer/fix_silhouette/base_height_adjustment"]);
			if (!Math::is_zero_approx(base_adjustment)) {
				StringName scale_base_bone_name = profile->get_scale_base_bone();
				int src_bone_idx = src_skeleton->find_bone(scale_base_bone_name);
				Transform3D src_rest = src_skeleton->get_bone_rest(src_bone_idx);
				src_skeleton->set_bone_rest(src_bone_idx, Transform3D(src_rest.basis, Vector3(src_rest.origin.x, src_rest.origin.y + base_adjustment, src_rest.origin.z)));

				TypedArray<Node> nodes = p_base_scene->find_children("*", "AnimationPlayer");
				while (nodes.size()) {
					AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(nodes.pop_back());
					List<StringName> anims;
					ap->get_animation_list(&anims);
					for (const StringName &name : anims) {
						Ref<Animation> anim = ap->get_animation(name);
						int track_len = anim->get_track_count();
						for (int i = 0; i < track_len; i++) {
							if (anim->track_get_path(i).get_subname_count() != 1 || anim->track_get_type(i) != Animation::TYPE_POSITION_3D) {
								continue;
							}

							if (anim->track_is_compressed(i)) {
								continue; // Shouldn't occur in internal_process().
							}

							String track_path = String(anim->track_get_path(i).get_concatenated_names());
							Node *node = (ap->get_node(ap->get_root_node()))->get_node(NodePath(track_path));
							ERR_CONTINUE(!node);

							Skeleton3D *track_skeleton = Object::cast_to<Skeleton3D>(node);
							if (!track_skeleton || track_skeleton != src_skeleton) {
								continue;
							}

							StringName bn = anim->track_get_path(i).get_concatenated_subnames();
							if (bn != scale_base_bone_name) {
								continue;
							}

							int key_len = anim->track_get_key_count(i);
							for (int j = 0; j < key_len; j++) {
								Vector3 pos = static_cast<Vector3>(anim->track_get_key_value(i, j));
								pos.y += base_adjustment;
								anim->track_set_key_value(i, j, pos);
							}
						}
					}
				}
			}

			// For skin modification in overwrite rest.
			for (int i = 0; i < src_skeleton->get_bone_count(); i++) {
				silhouette_diff_w[i] = pre_silhouette_skeleton_global_rest[i] * src_skeleton->get_bone_global_rest(i).affine_inverse();
			}

			is_rest_changed = true;
		}

		// Set motion scale to Skeleton if normalize position tracks.
		if (bool(p_options["retarget/rest_fixer/normalize_position_tracks"])) {
			int src_bone_idx = src_skeleton->find_bone(profile->get_scale_base_bone());
			if (src_bone_idx >= 0) {
				real_t motion_scale = abs(src_skeleton->get_bone_global_rest(src_bone_idx).origin.y);
				if (motion_scale > 0) {
					src_skeleton->set_motion_scale(motion_scale);
				}
			}
		}

		bool is_using_modifier = int(p_options["retarget/rest_fixer/retarget_method"]) == 2;
		bool is_using_global_pose = bool(p_options["retarget/rest_fixer/use_global_pose"]);
		Skeleton3D *orig_skeleton = nullptr;
		Skeleton3D *profile_skeleton = nullptr;

		// Retarget in some way.
		if (int(p_options["retarget/rest_fixer/retarget_method"]) > 0) {
			LocalVector<Transform3D> old_skeleton_rest;
			LocalVector<Transform3D> old_skeleton_global_rest;
			for (int i = 0; i < src_skeleton->get_bone_count(); i++) {
				old_skeleton_rest.push_back(src_skeleton->get_bone_rest(i));
				old_skeleton_global_rest.push_back(src_skeleton->get_bone_global_rest(i));
			}

			// Build structure for modifier.
			if (is_using_modifier) {
				orig_skeleton = src_skeleton;

				// Duplicate src_skeleton to modify animation tracks, it will memdelele after that animation track modification.
				src_skeleton = memnew(Skeleton3D);
				for (int i = 0; i < orig_skeleton->get_bone_count(); i++) {
					src_skeleton->add_bone(orig_skeleton->get_bone_name(i));
					src_skeleton->set_bone_rest(i, orig_skeleton->get_bone_rest(i));
					src_skeleton->set_bone_pose(i, orig_skeleton->get_bone_pose(i));
				}
				for (int i = 0; i < orig_skeleton->get_bone_count(); i++) {
					src_skeleton->set_bone_parent(i, orig_skeleton->get_bone_parent(i));
				}
				src_skeleton->set_motion_scale(orig_skeleton->get_motion_scale());

				// Rename orig_skeleton (previous src_skeleton), since it is not animated by animation track with GeneralSkeleton.
				String original_skeleton_name = String(p_options["retarget/rest_fixer/original_skeleton_name"]);
				String skel_name = orig_skeleton->get_name();
				ERR_FAIL_COND_MSG(original_skeleton_name.is_empty(), "Original skeleton name cannot be empty.");
				ERR_FAIL_COND_MSG(original_skeleton_name == skel_name, "Original skeleton name must be different from unique skeleton name.");

				// Rename profile skeleton to be general skeleton.
				profile_skeleton = memnew(Skeleton3D);
				bool is_unique = orig_skeleton->is_unique_name_in_owner();
				if (is_unique) {
					orig_skeleton->set_unique_name_in_owner(false);
				}
				orig_skeleton->set_name(original_skeleton_name);
				profile_skeleton->set_name(skel_name);
				if (is_unique) {
					profile_skeleton->set_unique_name_in_owner(true);
				}
				// Build profile skeleton bones.
				int len = profile->get_bone_size();
				for (int i = 0; i < len; i++) {
					profile_skeleton->add_bone(profile->get_bone_name(i));
					profile_skeleton->set_bone_rest(i, profile->get_reference_pose(i));
				}
				for (int i = 0; i < len; i++) {
					int target_parent = profile_skeleton->find_bone(profile->get_bone_parent(i));
					if (target_parent >= 0) {
						profile_skeleton->set_bone_parent(i, target_parent);
					}
				}
				for (int i = 0; i < len; i++) {
					Vector3 origin;
					int found = orig_skeleton->find_bone(profile->get_bone_name(i));
					String parent_name = profile->get_bone_parent(i);
					if (found >= 0) {
						origin = orig_skeleton->get_bone_global_rest(found).origin;
						if (profile->get_bone_name(i) != profile->get_root_bone()) {
							int src_parent = -1;
							while (src_parent < 0 && !parent_name.is_empty()) {
								src_parent = orig_skeleton->find_bone(parent_name);
								parent_name = profile->get_bone_parent(profile->find_bone(parent_name));
							}
							if (src_parent >= 0) {
								Transform3D parent_grest = orig_skeleton->get_bone_global_rest(src_parent);
								origin = origin - parent_grest.origin;
							}
						}
					}
					int target_parent = profile_skeleton->find_bone(profile->get_bone_parent(i));
					if (target_parent >= 0) {
						origin = profile_skeleton->get_bone_global_rest(target_parent).basis.get_rotation_quaternion().xform_inv(origin);
					}
					profile_skeleton->set_bone_rest(i, Transform3D(profile_skeleton->get_bone_rest(i).basis, origin));
				}
				profile_skeleton->set_motion_scale(orig_skeleton->get_motion_scale());
				profile_skeleton->reset_bone_poses();
				// Make structure with modifier.
				Node *owner = p_node->get_owner();

				Node *pr = orig_skeleton->get_parent();
				pr->add_child(profile_skeleton);
				profile_skeleton->set_owner(owner);

				RetargetModifier3D *mod = memnew(RetargetModifier3D);
				profile_skeleton->add_child(mod);
				mod->set_owner(owner);
				mod->set_name("RetargetModifier3D");

				orig_skeleton->set_owner(nullptr);
				orig_skeleton->reparent(mod, false);
				orig_skeleton->set_owner(owner);
				orig_skeleton->set_unique_name_in_owner(true);

				mod->set_use_global_pose(is_using_global_pose);
				mod->set_profile(profile);

				// Fix skeleton name in animation.
				// Mapped skeleton is animated by %GenerarSkeleton:RenamedBoneName.
				// Unmapped skeleton is animated by %OriginalSkeleton:OriginalBoneName.
				if (is_using_modifier) {
					TypedArray<Node> nodes = p_base_scene->find_children("*", "AnimationPlayer");
					String general_skeleton_pathname = UNIQUE_NODE_PREFIX + profile_skeleton->get_name();
					while (nodes.size()) {
						AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(nodes.pop_back());
						List<StringName> anims;
						ap->get_animation_list(&anims);
						for (const StringName &name : anims) {
							Ref<Animation> anim = ap->get_animation(name);
							int track_len = anim->get_track_count();
							for (int i = 0; i < track_len; i++) {
								if (anim->track_get_path(i).get_name_count() == 0) {
									return;
								}
								if (anim->track_get_path(i).get_name(0) == general_skeleton_pathname) {
									bool replace = false;
									if (anim->track_get_path(i).get_subname_count() > 0) {
										int found = profile_skeleton->find_bone(anim->track_get_path(i).get_concatenated_subnames());
										if (found < 0) {
											replace = true;
										}
									} else {
										replace = true;
									}
									if (replace) {
										String path_string = UNIQUE_NODE_PREFIX + original_skeleton_name;
										if (anim->track_get_path(i).get_name_count() > 1) {
											Vector<StringName> names = anim->track_get_path(i).get_names();
											names.remove_at(0);
											for (int j = 0; j < names.size(); j++) {
												path_string += "/" + names[i].operator String();
											}
										}
										if (anim->track_get_path(i).get_subname_count() > 0) {
											path_string = path_string + String(":") + anim->track_get_path(i).get_concatenated_subnames();
										}
										anim->track_set_path(i, path_string);
									}
								}
							}
						}
					}
				}
			}

			bool keep_global_rest_leftovers = bool(p_options["retarget/rest_fixer/keep_global_rest_on_leftovers"]);

			// Scan hierarchy and populate a whitelist of unmapped bones without mapped descendants.
			// When both is_using_modifier and is_using_global_pose are enabled, this array is used for detecting warning.
			Vector<int> keep_bone_rest;
			if (is_using_modifier || keep_global_rest_leftovers) {
				Vector<int> bones_to_process = src_skeleton->get_parentless_bones();
				while (bones_to_process.size() > 0) {
					int src_idx = bones_to_process[0];
					bones_to_process.erase(src_idx);
					Vector<int> src_children = src_skeleton->get_bone_children(src_idx);
					for (const int &src_child : src_children) {
						bones_to_process.push_back(src_child);
					}

					StringName src_bone_name = is_renamed ? StringName(src_skeleton->get_bone_name(src_idx)) : bone_map->find_profile_bone_name(src_skeleton->get_bone_name(src_idx));
					if (src_bone_name != StringName() && !profile->has_bone(src_bone_name)) {
						// Scan descendants for mapped bones.
						bool found_mapped = false;

						Vector<int> decendants_to_process = src_skeleton->get_bone_children(src_idx);
						while (decendants_to_process.size() > 0) {
							int desc_idx = decendants_to_process[0];
							decendants_to_process.erase(desc_idx);
							Vector<int> desc_children = src_skeleton->get_bone_children(desc_idx);
							for (const int &desc_child : desc_children) {
								decendants_to_process.push_back(desc_child);
							}

							StringName desc_bone_name = is_renamed ? StringName(src_skeleton->get_bone_name(desc_idx)) : bone_map->find_profile_bone_name(src_skeleton->get_bone_name(desc_idx));
							if (desc_bone_name != StringName() && profile->has_bone(desc_bone_name)) {
								found_mapped = true;
								break;
							}
						}

						if (!found_mapped) {
							keep_bone_rest.push_back(src_idx); // No mapped descendants. Add to whitelist.
						}
					}
				}
			}

			Vector<Basis> diffs;
			diffs.resize(src_skeleton->get_bone_count());
			Basis *diffs_w = diffs.ptrw();

			Vector<int> bones_to_process = src_skeleton->get_parentless_bones();
			while (bones_to_process.size() > 0) {
				int src_idx = bones_to_process[0];
				bones_to_process.erase(src_idx);
				Vector<int> src_children = src_skeleton->get_bone_children(src_idx);
				for (int i = 0; i < src_children.size(); i++) {
					bones_to_process.push_back(src_children[i]);
				}

				Basis tgt_rot;
				StringName src_bone_name = is_renamed ? StringName(src_skeleton->get_bone_name(src_idx)) : bone_map->find_profile_bone_name(src_skeleton->get_bone_name(src_idx));
				if (src_bone_name != StringName()) {
					Basis src_pg;
					int src_parent_idx = src_skeleton->get_bone_parent(src_idx);
					if (src_parent_idx >= 0) {
						src_pg = src_skeleton->get_bone_global_rest(src_parent_idx).basis;
					}
					int prof_idx = profile->find_bone(src_bone_name);
					if (prof_idx >= 0) {
						// Mapped bone uses reference pose.
						// It is fine to change rest here even though is_using_modifier is enabled, since next process is aborted with unmapped bones.
						tgt_rot = src_pg.inverse() * prof_skeleton->get_bone_global_rest(prof_idx).basis;
					} else if (keep_global_rest_leftovers && keep_bone_rest.has(src_idx)) {
						// Non-Mapped bones without mapped children keeps global rest.
						tgt_rot = src_pg.inverse() * old_skeleton_global_rest[src_idx].basis;
					}
				}

				if (src_skeleton->get_bone_parent(src_idx) >= 0) {
					diffs_w[src_idx] = tgt_rot.inverse() * diffs[src_skeleton->get_bone_parent(src_idx)] * src_skeleton->get_bone_rest(src_idx).basis;
				} else {
					diffs_w[src_idx] = tgt_rot.inverse() * src_skeleton->get_bone_rest(src_idx).basis;
				}

				Basis diff;
				if (src_skeleton->get_bone_parent(src_idx) >= 0) {
					diff = diffs[src_skeleton->get_bone_parent(src_idx)];
				}
				src_skeleton->set_bone_rest(src_idx, Transform3D(tgt_rot, diff.xform(src_skeleton->get_bone_rest(src_idx).origin)));
			}

			// Fix animation by changing rest.
			bool warning_detected = false;
			{
				TypedArray<Node> nodes = p_base_scene->find_children("*", "AnimationPlayer");
				while (nodes.size()) {
					AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(nodes.pop_back());
					ERR_CONTINUE(!ap);
					List<StringName> anims;
					ap->get_animation_list(&anims);
					for (const StringName &name : anims) {
						Ref<Animation> anim = ap->get_animation(name);
						int track_len = anim->get_track_count();
						for (int i = 0; i < track_len; i++) {
							if (anim->track_get_path(i).get_subname_count() != 1 || !(anim->track_get_type(i) == Animation::TYPE_POSITION_3D || anim->track_get_type(i) == Animation::TYPE_ROTATION_3D || anim->track_get_type(i) == Animation::TYPE_SCALE_3D)) {
								continue;
							}

							if (anim->track_is_compressed(i)) {
								continue; // Shouldn't occur in internal_process().
							}

							String track_path = String(anim->track_get_path(i).get_concatenated_names());
							Node *node = (ap->get_node(ap->get_root_node()))->get_node(NodePath(track_path));
							ERR_CONTINUE(!node);

							Skeleton3D *track_skeleton = Object::cast_to<Skeleton3D>(node);
							if (!track_skeleton ||
									(is_using_modifier && track_skeleton != profile_skeleton && track_skeleton != orig_skeleton) ||
									(!is_using_modifier && track_skeleton != src_skeleton)) {
								continue;
							}

							StringName bn = anim->track_get_path(i).get_subname(0);
							if (!bn) {
								continue;
							}

							int bone_idx = src_skeleton->find_bone(bn);

							if (is_using_modifier) {
								int prof_idx = profile->find_bone(bn);
								if (prof_idx < 0) {
									if (keep_bone_rest.has(bone_idx)) {
										warning_detected = true;
									}
									continue; // If is_using_modifier, the original skeleton rest is not changed.
								}
							}

							Transform3D old_rest = old_skeleton_rest[bone_idx];
							Transform3D new_rest = src_skeleton->get_bone_rest(bone_idx);
							Transform3D old_pg;
							Transform3D new_pg;
							int parent_idx = src_skeleton->get_bone_parent(bone_idx);
							if (parent_idx >= 0) {
								old_pg = old_skeleton_global_rest[parent_idx];
								new_pg = src_skeleton->get_bone_global_rest(parent_idx);
							}

							int key_len = anim->track_get_key_count(i);
							if (anim->track_get_type(i) == Animation::TYPE_ROTATION_3D) {
								Quaternion old_rest_q = old_rest.basis.get_rotation_quaternion();
								Quaternion new_rest_q = new_rest.basis.get_rotation_quaternion();
								Quaternion old_pg_q = old_pg.basis.get_rotation_quaternion();
								Quaternion new_pg_q = new_pg.basis.get_rotation_quaternion();
								for (int j = 0; j < key_len; j++) {
									Quaternion qt = static_cast<Quaternion>(anim->track_get_key_value(i, j));
									anim->track_set_key_value(i, j, new_pg_q.inverse() * old_pg_q * qt * old_rest_q.inverse() * old_pg_q.inverse() * new_pg_q * new_rest_q);
								}
							} else if (anim->track_get_type(i) == Animation::TYPE_SCALE_3D) {
								Basis old_rest_b_inv = old_rest.basis.inverse();
								Basis new_rest_b = new_rest.basis;
								Basis old_pg_b = old_pg.basis;
								Basis new_pg_b = new_pg.basis;
								Basis old_pg_b_inv = old_pg.basis.inverse();
								Basis new_pg_b_inv = new_pg.basis.inverse();
								for (int j = 0; j < key_len; j++) {
									Basis sc = Basis().scaled(static_cast<Vector3>(anim->track_get_key_value(i, j)));
									anim->track_set_key_value(i, j, (new_pg_b_inv * old_pg_b * sc * old_rest_b_inv * old_pg_b_inv * new_pg_b * new_rest_b).get_scale());
								}
							} else {
								Vector3 old_rest_o = old_rest.origin;
								Vector3 new_rest_o = new_rest.origin;
								Basis old_pg_b = old_pg.basis;
								Basis new_pg_b = new_pg.basis;
								for (int j = 0; j < key_len; j++) {
									Vector3 ps = static_cast<Vector3>(anim->track_get_key_value(i, j));
									anim->track_set_key_value(i, j, new_pg_b.xform_inv(old_pg_b.xform(ps - old_rest_o)) + new_rest_o);
								}
							}
						}
					}
				}
			}
			if (is_using_global_pose && warning_detected) {
				// TODO:
				// Theoretically, if A and its conversion are calculated correctly taking into account the difference in the number of bones,
				// there is no need to disable use_global_pose, but this is probably a fairly niche case.
				WARN_PRINT_ED("Animated extra bone between mapped bones detected, consider disabling Use Global Pose option to prevent that the pose origin be overridden by the RetargetModifier3D.");
			}

			if (p_options.has("retarget/rest_fixer/reset_all_bone_poses_after_import") && !bool(p_options["retarget/rest_fixer/reset_all_bone_poses_after_import"])) {
				// If Reset All Bone Poses After Import is disabled, preserve the original bone pose, adjusted for the new bone rolls.
				for (int bone_idx = 0; bone_idx < src_skeleton->get_bone_count(); bone_idx++) {
					Transform3D old_rest = old_skeleton_rest[bone_idx];
					Transform3D new_rest = src_skeleton->get_bone_rest(bone_idx);
					Transform3D old_pg;
					Transform3D new_pg;
					int parent_idx = src_skeleton->get_bone_parent(bone_idx);
					if (parent_idx >= 0) {
						old_pg = old_skeleton_global_rest[parent_idx];
						new_pg = src_skeleton->get_bone_global_rest(parent_idx);
					}

					Quaternion old_pg_q = old_pg.basis.get_rotation_quaternion();
					Quaternion new_pg_q = new_pg.basis.get_rotation_quaternion();
					Quaternion qt = src_skeleton->get_bone_pose_rotation(bone_idx);
					src_skeleton->set_bone_pose_rotation(bone_idx, new_pg_q.inverse() * old_pg_q * qt * old_rest.basis.get_rotation_quaternion().inverse() * old_pg_q.inverse() * new_pg_q * new_rest.basis.get_rotation_quaternion());

					Basis sc = Basis().scaled(src_skeleton->get_bone_pose_scale(bone_idx));
					src_skeleton->set_bone_pose_scale(bone_idx, (new_pg.basis.inverse() * old_pg.basis * sc * old_rest.basis.inverse() * old_pg.basis.inverse() * new_pg.basis * new_rest.basis).get_scale());
					Vector3 ps = src_skeleton->get_bone_pose_position(bone_idx);
					src_skeleton->set_bone_pose_position(bone_idx, new_pg_q.xform_inv(old_pg_q.xform(ps - old_rest.origin)) + new_rest.origin);
				}
			}

			if (is_using_modifier) {
				memdelete(src_skeleton);
				src_skeleton = profile_skeleton;
			}

			is_rest_changed = true;
		}

		// Scale position tracks by motion scale if normalize position tracks.
		if (bool(p_options["retarget/rest_fixer/normalize_position_tracks"])) {
			TypedArray<Node> nodes = p_base_scene->find_children("*", "AnimationPlayer");
			while (nodes.size()) {
				AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(nodes.pop_back());
				List<StringName> anims;
				ap->get_animation_list(&anims);
				for (const StringName &name : anims) {
					Ref<Animation> anim = ap->get_animation(name);
					int track_len = anim->get_track_count();
					for (int i = 0; i < track_len; i++) {
						if (anim->track_get_path(i).get_subname_count() != 1 || anim->track_get_type(i) != Animation::TYPE_POSITION_3D) {
							continue;
						}

						if (anim->track_is_compressed(i)) {
							continue; // Shouldn't occur in internal_process().
						}

						String track_path = String(anim->track_get_path(i).get_concatenated_names());
						Node *node = (ap->get_node(ap->get_root_node()))->get_node(NodePath(track_path));
						ERR_CONTINUE(!node);

						Skeleton3D *track_skeleton = Object::cast_to<Skeleton3D>(node);
						if (!track_skeleton ||
								(is_using_modifier && track_skeleton != profile_skeleton && track_skeleton != orig_skeleton) ||
								(!is_using_modifier && track_skeleton != src_skeleton)) {
							continue;
						}

						real_t mlt = 1 / src_skeleton->get_motion_scale();
						int key_len = anim->track_get_key_count(i);
						for (int j = 0; j < key_len; j++) {
							Vector3 pos = static_cast<Vector3>(anim->track_get_key_value(i, j));
							anim->track_set_key_value(i, j, pos * mlt);
						}
					}
				}
			}
		}

		if (!is_using_modifier && is_rest_changed) {
			// Fix skin.
			{
				HashSet<Ref<Skin>> mutated_skins;
				TypedArray<Node> nodes = p_base_scene->find_children("*", "ImporterMeshInstance3D");
				while (nodes.size()) {
					ImporterMeshInstance3D *mi = Object::cast_to<ImporterMeshInstance3D>(nodes.pop_back());
					ERR_CONTINUE(!mi);

					Ref<Skin> skin = mi->get_skin();
					if (skin.is_null()) {
						continue;
					}
					if (mutated_skins.has(skin)) {
						continue;
					}
					mutated_skins.insert(skin);

					Node *node = mi->get_node(mi->get_skeleton_path());
					ERR_CONTINUE(!node);

					Skeleton3D *mesh_skeleton = Object::cast_to<Skeleton3D>(node);
					if (!mesh_skeleton || mesh_skeleton != src_skeleton) {
						continue;
					}

					int skin_len = skin->get_bind_count();
					for (int i = 0; i < skin_len; i++) {
						StringName bn = skin->get_bind_name(i);
						int bone_idx = src_skeleton->find_bone(bn);
						if (bone_idx >= 0) {
							Transform3D adjust_transform = src_skeleton->get_bone_global_rest(bone_idx).affine_inverse() * silhouette_diff[bone_idx].affine_inverse() * pre_silhouette_skeleton_global_rest[bone_idx];
							adjust_transform.scale(global_transform.basis.get_scale_global());
							skin->set_bind_pose(i, adjust_transform * skin->get_bind_pose(i));
						}
					}
				}
				nodes = src_skeleton->get_children();
				while (nodes.size()) {
					BoneAttachment3D *attachment = Object::cast_to<BoneAttachment3D>(nodes.pop_back());
					if (attachment == nullptr) {
						continue;
					}
					int bone_idx = attachment->get_bone_idx();
					if (bone_idx == -1) {
						bone_idx = src_skeleton->find_bone(attachment->get_bone_name());
					}
					ERR_CONTINUE(bone_idx < 0 || bone_idx >= src_skeleton->get_bone_count());
					Transform3D adjust_transform = src_skeleton->get_bone_global_rest(bone_idx).affine_inverse() * silhouette_diff[bone_idx].affine_inverse() * pre_silhouette_skeleton_global_rest[bone_idx];
					adjust_transform.scale(global_transform.basis.get_scale_global());

					TypedArray<Node> child_nodes = attachment->get_children();
					while (child_nodes.size()) {
						Node3D *child = Object::cast_to<Node3D>(child_nodes.pop_back());
						if (child == nullptr) {
							continue;
						}
						child->set_transform(adjust_transform * child->get_transform());
					}
				}
			}

			if (!p_options.has("retarget/rest_fixer/reset_all_bone_poses_after_import") || bool(p_options["retarget/rest_fixer/reset_all_bone_poses_after_import"])) {
				// Init skeleton pose to new rest.
				for (int i = 0; i < src_skeleton->get_bone_count(); i++) {
					Transform3D fixed_rest = src_skeleton->get_bone_rest(i);
					src_skeleton->set_bone_pose_position(i, fixed_rest.origin);
					src_skeleton->set_bone_pose_rotation(i, fixed_rest.basis.get_rotation_quaternion());
					src_skeleton->set_bone_pose_scale(i, fixed_rest.basis.get_scale());
				}
				if (orig_skeleton) {
					for (int i = 0; i < orig_skeleton->get_bone_count(); i++) {
						Transform3D fixed_rest = orig_skeleton->get_bone_rest(i);
						orig_skeleton->set_bone_pose_position(i, fixed_rest.origin);
						orig_skeleton->set_bone_pose_rotation(i, fixed_rest.basis.get_rotation_quaternion());
						orig_skeleton->set_bone_pose_scale(i, fixed_rest.basis.get_scale());
					}
				}
			}
		}

		memdelete(prof_skeleton);
	}
}

PostImportPluginSkeletonRestFixer::PostImportPluginSkeletonRestFixer() {
}
