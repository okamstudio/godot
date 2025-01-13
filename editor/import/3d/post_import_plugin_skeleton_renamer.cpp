/**************************************************************************/
/*  post_import_plugin_skeleton_renamer.cpp                               */
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

#include "post_import_plugin_skeleton_renamer.h"

#include "scene/3d/bone_attachment_3d.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/resources/bone_map.h"

void PostImportPluginSkeletonRenamer::get_internal_import_options(InternalImportCategory p_category, List<ResourceImporter::ImportOption> *r_options) {
	if (p_category == INTERNAL_IMPORT_CATEGORY_SKELETON_3D_NODE) {
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "retarget/bone_renamer/rename_bones", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), true));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "retarget/bone_renamer/unique_node/make_unique"), true));
		r_options->push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::STRING, "retarget/bone_renamer/unique_node/skeleton_name"), "GeneralSkeleton"));
	}
}

void PostImportPluginSkeletonRenamer::_internal_process(InternalImportCategory p_category, Node *p_base_scene, Node *p_node, Ref<Resource> p_resource, const Dictionary &p_options, const HashMap<String, String> &p_rename_map) {
	// Prepare objects.
	Object *map = p_options["retarget/bone_map"].get_validated_object();
	if (!map || !bool(p_options["retarget/bone_renamer/rename_bones"])) {
		return;
	}
	Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(p_node);
	if (skeleton) {
		// Rename bones in Skeleton3D.
		int len = skeleton->get_bone_count();
		for (int i = 0; i < len; i++) {
			String current_bone_name = skeleton->get_bone_name(i);
			const HashMap<String, String>::ConstIterator new_bone_name = p_rename_map.find(current_bone_name);
			if (new_bone_name) {
				skeleton->set_bone_name(i, new_bone_name->value);
			}
		}
	}

	// Rename bones in Skin.
	{
		TypedArray<Node> nodes = p_base_scene->find_children("*", "ImporterMeshInstance3D");
		while (nodes.size()) {
			ImporterMeshInstance3D *mi = Object::cast_to<ImporterMeshInstance3D>(nodes.pop_back());
			Ref<Skin> skin = mi->get_skin();
			if (skin.is_valid()) {
				Node *node = mi->get_node(mi->get_skeleton_path());
				if (node) {
					Skeleton3D *mesh_skeleton = Object::cast_to<Skeleton3D>(node);
					if (mesh_skeleton && node == skeleton) {
						int len = skin->get_bind_count();

						for (int i = 0; i < len; i++) {
							String current_bone_name = skin->get_bind_name(i);
							const HashMap<String, String>::ConstIterator new_bone_name = p_rename_map.find(current_bone_name);

							if (new_bone_name) {
								skin->set_bind_name(i, new_bone_name->value);
							}
						}
					}
				}
			}
		}
	}

	// Rename bones in AnimationPlayer.
	{
		TypedArray<Node> nodes = p_base_scene->find_children("*", "AnimationPlayer");
		while (nodes.size()) {
			AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(nodes.pop_back());
			List<StringName> anims;
			ap->get_animation_list(&anims);
			for (const StringName &name : anims) {
				Ref<Animation> anim = ap->get_animation(name);
				int len = anim->get_track_count();
				for (int i = 0; i < len; i++) {
					if (anim->track_get_path(i).get_subname_count() != 1 || !(anim->track_get_type(i) == Animation::TYPE_POSITION_3D || anim->track_get_type(i) == Animation::TYPE_ROTATION_3D || anim->track_get_type(i) == Animation::TYPE_SCALE_3D)) {
						continue;
					}
					String track_path = String(anim->track_get_path(i).get_concatenated_names());
					Node *node = (ap->get_node(ap->get_root_node()))->get_node(NodePath(track_path));
					if (node) {
						Skeleton3D *track_skeleton = Object::cast_to<Skeleton3D>(node);
						if (track_skeleton && track_skeleton == skeleton) {
							String current_bone_name = anim->track_get_path(i).get_subname(0);
							const HashMap<String, String>::ConstIterator new_bone_name = p_rename_map.find(current_bone_name);
							if (new_bone_name) {
								String new_track_path = track_path + ":" + new_bone_name->value;
								anim->track_set_path(i, new_track_path);
							}
						}
					}
				}
			}
		}
	}

	// Rename bones in all Nodes by calling method.
	{
		Dictionary rename_map_dict;
		for (HashMap<String, String>::ConstIterator E = p_rename_map.begin(); E; ++E) {
			rename_map_dict[E->key] = E->value;
		}

		TypedArray<Node> nodes = p_base_scene->find_children("*", "BoneAttachment3D");
		while (nodes.size()) {
			BoneAttachment3D *attachment = Object::cast_to<BoneAttachment3D>(nodes.pop_back());
			if (attachment) {
				attachment->notify_skeleton_bones_renamed(p_base_scene, skeleton, rename_map_dict);
			}
		}
	}
}

void PostImportPluginSkeletonRenamer::internal_process(InternalImportCategory p_category, Node *p_base_scene, Node *p_node, Ref<Resource> p_resource, const Dictionary &p_options) {
	if (p_category == INTERNAL_IMPORT_CATEGORY_SKELETON_3D_NODE) {
		// Prepare objects.
		Object *map = p_options["retarget/bone_map"].get_validated_object();
		if (!map || !bool(p_options["retarget/bone_renamer/rename_bones"])) {
			return;
		}
		Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(p_node);
		BoneMap *bone_map = Object::cast_to<BoneMap>(map);
		int len = skeleton->get_bone_count();

		// First, prepare main rename map.
		HashMap<String, String> main_rename_map;
		for (int i = 0; i < len; i++) {
			String bone_name = skeleton->get_bone_name(i);
			String target_name = bone_map->find_profile_bone_name(bone_name);
			if (target_name.is_empty()) {
				continue;
			}
			main_rename_map.insert(bone_name, target_name);
		}

		// Preprocess of renaming bones to avoid to conflict with original bone name.
		HashMap<String, String> pre_rename_map; // HashMap<skeleton bone name, target(profile) bone name>
		{
			Vector<String> solved_name_stack;
			for (int i = 0; i < len; i++) {
				String bone_name = skeleton->get_bone_name(i);
				String target_name = bone_map->find_profile_bone_name(bone_name);
				if (target_name.is_empty() || bone_name == target_name || skeleton->find_bone(target_name) == -1) {
					continue; // No conflicting.
				}

				// Solve conflicting.
				Ref<SkeletonProfile> profile = bone_map->get_profile();
				String solved_name = target_name;
				for (int j = 2; skeleton->find_bone(solved_name) >= 0 || profile->find_bone(solved_name) >= 0 || solved_name_stack.has(solved_name); j++) {
					solved_name = target_name + itos(j);
				}
				solved_name_stack.push_back(solved_name);
				pre_rename_map.insert(target_name, solved_name);
			}
			_internal_process(p_category, p_base_scene, p_node, p_resource, p_options, pre_rename_map);
		}

		// Main process of renaming bones.
		{
			// Apply pre-renaming result to prepared main rename map.
			Vector<String> remove_queue;
			for (HashMap<String, String>::Iterator E = main_rename_map.begin(); E; ++E) {
				if (pre_rename_map.has(E->key)) {
					remove_queue.push_back(E->key);
				}
			}
			for (int i = 0; i < remove_queue.size(); i++) {
				main_rename_map.insert(pre_rename_map[remove_queue[i]], main_rename_map[remove_queue[i]]);
				main_rename_map.erase(remove_queue[i]);
			}
			_internal_process(p_category, p_base_scene, p_node, p_resource, p_options, main_rename_map);
		}

		// Make unique skeleton.
		if (bool(p_options["retarget/bone_renamer/unique_node/make_unique"])) {
			String unique_name = String(p_options["retarget/bone_renamer/unique_node/skeleton_name"]);
			ERR_FAIL_COND_MSG(unique_name.is_empty(), "Skeleton unique name cannot be empty.");

			TypedArray<Node> nodes = p_base_scene->find_children("*", "AnimationPlayer");
			while (nodes.size()) {
				AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(nodes.pop_back());
				List<StringName> anims;
				ap->get_animation_list(&anims);
				for (const StringName &name : anims) {
					Ref<Animation> anim = ap->get_animation(name);
					int track_len = anim->get_track_count();
					for (int i = 0; i < track_len; i++) {
						String track_path = String(anim->track_get_path(i).get_concatenated_names());
						Node *orig_node = (ap->get_node(ap->get_root_node()))->get_node(NodePath(track_path));
						Node *node = (ap->get_node(ap->get_root_node()))->get_node(NodePath(track_path));
						while (node) {
							Skeleton3D *track_skeleton = Object::cast_to<Skeleton3D>(node);
							if (track_skeleton && track_skeleton == skeleton) {
								if (node == orig_node) {
									if (anim->track_get_path(i).get_subname_count() > 0) {
										anim->track_set_path(i, UNIQUE_NODE_PREFIX + unique_name + String(":") + anim->track_get_path(i).get_concatenated_subnames());
									} else {
										anim->track_set_path(i, UNIQUE_NODE_PREFIX + unique_name);
									}
								} else {
									if (anim->track_get_path(i).get_subname_count() > 0) {
										anim->track_set_path(i, UNIQUE_NODE_PREFIX + unique_name + "/" + node->get_path_to(orig_node) + String(":") + anim->track_get_path(i).get_concatenated_subnames());
									} else {
										anim->track_set_path(i, UNIQUE_NODE_PREFIX + unique_name + "/" + node->get_path_to(orig_node));
									}
								}
								break;
							}
							node = node->get_parent();
						}
					}
				}
			}
			skeleton->set_name(unique_name);
			skeleton->set_unique_name_in_owner(true);
		}
	}
}

PostImportPluginSkeletonRenamer::PostImportPluginSkeletonRenamer() {
}
