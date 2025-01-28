/**************************************************************************/
/*  packed_scene.cpp                                                      */
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

#include "packed_scene.h"

#include "core/config/engine.h"
#include "core/io/missing_resource.h"
#include "core/io/resource_loader.h"
#include "core/templates/local_vector.h"
#include "scene/2d/node_2d.h"
#ifndef _3D_DISABLED
#include "scene/3d/node_3d.h"
#endif // _3D_DISABLED
#include "scene/gui/control.h"
#include "scene/main/instance_placeholder.h"
#include "scene/main/missing_node.h"
#include "scene/property_utils.h"

#define PACKED_SCENE_VERSION 3

#ifdef TOOLS_ENABLED
SceneState::InstantiationWarningNotify SceneState::instantiation_warn_notify = nullptr;
#endif

bool SceneState::can_instantiate() const {
	return nodes.size() > 0;
}

static Array _sanitize_node_pinned_properties(Node *p_node) {
	Array pinned = p_node->get_meta("_edit_pinned_properties_", Array());
	if (pinned.is_empty()) {
		return Array();
	}
	HashSet<StringName> storable_properties;
	p_node->get_storable_properties(storable_properties);
	int i = 0;
	do {
		if (storable_properties.has(pinned[i])) {
			i++;
		} else {
			pinned.remove_at(i);
		}
	} while (i < pinned.size());
	if (pinned.is_empty()) {
		p_node->remove_meta("_edit_pinned_properties_");
	}
	return pinned;
}

Ref<Resource> SceneState::get_remap_resource(const Ref<Resource> &p_resource, HashMap<Ref<Resource>, Ref<Resource>> &remap_cache, const Ref<Resource> &p_fallback, Node *p_for_scene) {
	ERR_FAIL_COND_V(p_resource.is_null(), Ref<Resource>());

	Ref<Resource> remap_resource;

	// Find the shared copy of the source resource.
	HashMap<Ref<Resource>, Ref<Resource>>::Iterator R = remap_cache.find(p_resource);
	if (R) {
		remap_resource = R->value;
	} else if (p_fallback.is_valid() && p_fallback->is_local_to_scene() && p_fallback->get_class() == p_resource->get_class()) {
		// Simply copy the data from the source resource to update the fallback resource that was previously set.

		p_fallback->reset_state(); // May want to reset state.

		List<PropertyInfo> pi;
		p_resource->get_property_list(&pi);
		for (const PropertyInfo &E : pi) {
			if (!(E.usage & PROPERTY_USAGE_STORAGE)) {
				continue;
			}
			if (E.name == "resource_path") {
				continue; // Do not change path.
			}

			Variant value = p_resource->get(E.name);

			// The local-to-scene subresource instance is preserved, thus maintaining the previous sharing relationship.
			// This is mainly used when the sub-scene root is reset in the main scene.
			Ref<Resource> sub_res_of_from = value;
			if (sub_res_of_from.is_valid() && sub_res_of_from->is_local_to_scene()) {
				value = get_remap_resource(sub_res_of_from, remap_cache, p_fallback->get(E.name), p_fallback->get_local_scene());
			}

			p_fallback->set(E.name, value);
		}

		p_fallback->set_scene_unique_id(p_resource->get_scene_unique_id()); // Get the id from the main scene, in case the id changes again when saving the scene.

		remap_cache[p_resource] = p_fallback;
		remap_resource = p_fallback;
	} else { // A copy of the source resource is required to overwrite the previous one.
		Ref<Resource> local_dupe = p_resource->duplicate_for_local_scene(p_for_scene, remap_cache);
		remap_cache[p_resource] = local_dupe;
		remap_resource = local_dupe;
	}

	return remap_resource;
}

Node *SceneState::instantiate(GenEditState p_edit_state) const {
	// Nodes where instantiation failed (because something is missing.)
	List<Node *> stray_instances;

#define NODE_FROM_ID(p_name, p_id)                      \
	Node *p_name;                                       \
	if (p_id & FLAG_ID_IS_PATH) {                       \
		NodePath np = node_paths[p_id & FLAG_MASK];     \
		p_name = ret_nodes[0]->get_node_or_null(np);    \
	} else {                                            \
		ERR_FAIL_INDEX_V(p_id &FLAG_MASK, nc, nullptr); \
		p_name = ret_nodes[p_id & FLAG_MASK];           \
	}

	int nc = nodes.size();
	ERR_FAIL_COND_V_MSG(nc == 0, nullptr, vformat("Failed to instantiate scene state of \"%s\", node count is 0. Make sure the PackedScene resource is valid.", path));

	const StringName *snames = nullptr;
	int sname_count = names.size();
	if (sname_count) {
		snames = &names[0];
	}

	const Variant *props = nullptr;
	int prop_count = variants.size();
	if (prop_count) {
		props = &variants[0];
	}

	//Vector<Variant> properties;

	const NodeData *nd = &nodes[0];

	Node **ret_nodes = (Node **)alloca(sizeof(Node *) * nc);

	bool gen_node_path_cache = p_edit_state != GEN_EDIT_STATE_DISABLED && node_path_cache.is_empty();

	HashMap<Ref<Resource>, Ref<Resource>> resources_local_to_scene;

	LocalVector<DeferredNodePathProperties> deferred_node_paths;

	for (int i = 0; i < nc; i++) {
		const NodeData &n = nd[i];

		Node *parent = nullptr;
		String old_parent_path;

		if (i > 0) {
			ERR_FAIL_COND_V_MSG(n.parent == -1, nullptr, vformat("Invalid scene: node %s does not specify its parent node.", snames[n.name]));
			NODE_FROM_ID(nparent, n.parent);
#ifdef DEBUG_ENABLED
			if (!nparent && (n.parent & FLAG_ID_IS_PATH)) {
				WARN_PRINT(String("Parent path '" + String(node_paths[n.parent & FLAG_MASK]) + "' for node '" + String(snames[n.name]) + "' has vanished when instantiating: '" + get_path() + "'.").ascii().get_data());
				old_parent_path = String(node_paths[n.parent & FLAG_MASK]).trim_prefix("./").replace("/", "@");
				nparent = ret_nodes[0];
			}
#endif
			parent = nparent;
		} else {
			// i == 0 is root node.
			ERR_FAIL_COND_V_MSG(n.parent != -1, nullptr, vformat("Invalid scene: root node %s cannot specify a parent node.", snames[n.name]));
			ERR_FAIL_COND_V_MSG(n.type == TYPE_INSTANTIATED && base_scene_idx < 0, nullptr, vformat("Invalid scene: root node %s in an instance, but there's no base scene.", snames[n.name]));
		}

		Node *node = nullptr;
		MissingNode *missing_node = nullptr;
		bool is_inherited_scene = false;

		if (i == 0 && base_scene_idx >= 0) {
			// Scene inheritance on root node.
			Ref<PackedScene> sdata = props[base_scene_idx];
			ERR_FAIL_COND_V(sdata.is_null(), nullptr);
			node = sdata->instantiate(p_edit_state == GEN_EDIT_STATE_DISABLED ? PackedScene::GEN_EDIT_STATE_DISABLED : PackedScene::GEN_EDIT_STATE_INSTANCE); //only main gets main edit state
			ERR_FAIL_NULL_V(node, nullptr);
			if (p_edit_state != GEN_EDIT_STATE_DISABLED) {
				node->set_scene_inherited_state(sdata->get_state());
			}
			is_inherited_scene = true;
		} else if (n.instance >= 0) {
			// Instance a scene into this node.
			if (n.instance & FLAG_INSTANCE_IS_PLACEHOLDER) {
				const String scene_path = props[n.instance & FLAG_MASK];
				if (disable_placeholders) {
					Ref<PackedScene> sdata = ResourceLoader::load(scene_path, "PackedScene");
					if (sdata.is_valid()) {
						node = sdata->instantiate(p_edit_state == GEN_EDIT_STATE_DISABLED ? PackedScene::GEN_EDIT_STATE_DISABLED : PackedScene::GEN_EDIT_STATE_INSTANCE);
						ERR_FAIL_NULL_V(node, nullptr);
					} else if (ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
						missing_node = memnew(MissingNode);
						missing_node->set_original_scene(scene_path);
						missing_node->set_recording_properties(true);
						node = missing_node;
					} else {
						ERR_FAIL_V_MSG(nullptr, "Placeholder scene is missing.");
					}
				} else {
					InstancePlaceholder *ip = memnew(InstancePlaceholder);
					ip->set_instance_path(scene_path);
					node = ip;
				}
				node->set_scene_instance_load_placeholder(true);
			} else {
				Ref<Resource> res = props[n.instance & FLAG_MASK];
				Ref<PackedScene> sdata = res;
				if (sdata.is_valid()) {
					node = sdata->instantiate(p_edit_state == GEN_EDIT_STATE_DISABLED ? PackedScene::GEN_EDIT_STATE_DISABLED : PackedScene::GEN_EDIT_STATE_INSTANCE);
					ERR_FAIL_NULL_V_MSG(node, nullptr, vformat("Failed to load scene dependency: \"%s\". Make sure the required scene is valid.", sdata->get_path()));
				} else if (ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
					missing_node = memnew(MissingNode);
#ifdef TOOLS_ENABLED
					if (res.is_valid()) {
						missing_node->set_original_scene(res->get_meta("__load_path__", ""));
					}
#endif
					missing_node->set_recording_properties(true);
					node = missing_node;
				} else {
					ERR_FAIL_V_MSG(nullptr, "Scene instance is missing.");
				}
			}

		} else if (n.type == TYPE_INSTANTIATED) {
			// Get the node from somewhere, it likely already exists from another instance.
			if (parent) {
				node = parent->_get_child_by_name(snames[n.name]);
#ifdef DEBUG_ENABLED
				if (!node) {
					WARN_PRINT(String("Node '" + String(ret_nodes[0]->get_path_to(parent)) + "/" + String(snames[n.name]) + "' was modified from inside an instance, but it has vanished.").ascii().get_data());
				}
#endif
			}
		} else {
			// Node belongs to this scene and must be created.
			Object *obj = ClassDB::instantiate(snames[n.type]);

			node = Object::cast_to<Node>(obj);

			if (!node) {
				if (obj) {
					memdelete(obj);
					obj = nullptr;
				}

				if (ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
					missing_node = memnew(MissingNode);
					missing_node->set_original_class(snames[n.type]);
					missing_node->set_recording_properties(true);
					node = missing_node;
					obj = missing_node;
				} else {
					WARN_PRINT(vformat("Node %s of type %s cannot be created. A placeholder will be created instead.", snames[n.name], snames[n.type]).ascii().get_data());
					if (n.parent >= 0 && n.parent < nc && ret_nodes[n.parent]) {
						if (Object::cast_to<Control>(ret_nodes[n.parent])) {
							obj = memnew(Control);
						} else if (Object::cast_to<Node2D>(ret_nodes[n.parent])) {
							obj = memnew(Node2D);
#ifndef _3D_DISABLED
						} else if (Object::cast_to<Node3D>(ret_nodes[n.parent])) {
							obj = memnew(Node3D);
#endif // _3D_DISABLED
						}
					}

					if (!obj) {
						obj = memnew(Node);
					}

					node = Object::cast_to<Node>(obj);
				}
			}
		}

		if (node) {
			// may not have found the node (part of instantiated scene and removed)
			// if found all is good, otherwise ignore

			//properties
			int nprop_count = n.properties.size();
			if (nprop_count) {
				const NodeData::Property *nprops = &n.properties[0];

				Dictionary missing_resource_properties;
				HashMap<Ref<Resource>, Ref<Resource>> resources_local_to_sub_scene; // Record the mappings in the sub-scene.

				for (int j = 0; j < nprop_count; j++) {
					bool valid;

					ERR_FAIL_INDEX_V(nprops[j].value, prop_count, nullptr);

					if (nprops[j].name & FLAG_PATH_PROPERTY_IS_NODE) {
						if (!Engine::get_singleton()->is_editor_hint() && node->get_scene_instance_load_placeholder()) {
							// We cannot know if the referenced nodes exist yet, so instead of deferring, we write the NodePaths directly.

							uint32_t name_idx = nprops[j].name & (FLAG_PATH_PROPERTY_IS_NODE - 1);
							ERR_FAIL_UNSIGNED_INDEX_V(name_idx, (uint32_t)sname_count, nullptr);

							node->set(snames[name_idx], props[nprops[j].value], &valid);
							continue;
						}

						uint32_t name_idx = nprops[j].name & (FLAG_PATH_PROPERTY_IS_NODE - 1);
						ERR_FAIL_UNSIGNED_INDEX_V(name_idx, (uint32_t)sname_count, nullptr);

						DeferredNodePathProperties dnp;
						dnp.value = props[nprops[j].value];
						dnp.base = node;
						dnp.property = snames[name_idx];
						deferred_node_paths.push_back(dnp);
						continue;
					}

					ERR_FAIL_INDEX_V(nprops[j].name, sname_count, nullptr);

					if (snames[nprops[j].name] == CoreStringName(script)) {
						//work around to avoid old script variables from disappearing, should be the proper fix to:
						//https://github.com/godotengine/godot/issues/2958

						//store old state
						List<Pair<StringName, Variant>> old_state;
						if (node->get_script_instance()) {
							node->get_script_instance()->get_property_state(old_state);
						}

						node->set(snames[nprops[j].name], props[nprops[j].value], &valid);

						//restore old state for new script, if exists
						for (const Pair<StringName, Variant> &E : old_state) {
							node->set(E.first, E.second);
						}
					} else {
						Variant value = props[nprops[j].value];

						// Making sure that instances of inherited scenes don't share the same
						// reference between them.
						if (is_inherited_scene) {
							value = value.duplicate(true);
						}

						if (value.get_type() == Variant::OBJECT) {
							//handle resources that are local to scene by duplicating them if needed
							Ref<Resource> res = value;
							if (res.is_valid()) {
								value = make_local_resource(value, n, resources_local_to_sub_scene, node, snames[nprops[j].name], resources_local_to_scene, i, ret_nodes, p_edit_state);
							}
						}

						if (value.get_type() == Variant::ARRAY) {
							Array set_array = value;
							value = setup_resources_in_array(set_array, n, resources_local_to_sub_scene, node, snames[nprops[j].name], resources_local_to_scene, i, ret_nodes, p_edit_state);

							bool is_get_valid = false;
							Variant get_value = node->get(snames[nprops[j].name], &is_get_valid);

							if (is_get_valid && get_value.get_type() == Variant::ARRAY) {
								Array get_array = get_value;
								if (!set_array.is_same_typed(get_array)) {
									value = Array(set_array, get_array.get_typed_builtin(), get_array.get_typed_class_name(), get_array.get_typed_script());
								}
							}
						}

						if (value.get_type() == Variant::DICTIONARY) {
							Dictionary set_dict = value;
							value = setup_resources_in_dictionary(set_dict, n, resources_local_to_sub_scene, node, snames[nprops[j].name], resources_local_to_scene, i, ret_nodes, p_edit_state);

							bool is_get_valid = false;
							Variant get_value = node->get(snames[nprops[j].name], &is_get_valid);

							if (is_get_valid && get_value.get_type() == Variant::DICTIONARY) {
								Dictionary get_dict = get_value;
								if (!set_dict.is_same_typed(get_dict)) {
									value = Dictionary(set_dict, get_dict.get_typed_key_builtin(), get_dict.get_typed_key_class_name(), get_dict.get_typed_key_script(),
											get_dict.get_typed_value_builtin(), get_dict.get_typed_value_class_name(), get_dict.get_typed_value_script());
								}
							}
						}

						bool set_valid = true;
						if (ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled() && value.get_type() == Variant::OBJECT) {
							Ref<MissingResource> mr = value;
							if (mr.is_valid()) {
								missing_resource_properties[snames[nprops[j].name]] = mr;
								set_valid = false;
							}
						}

						if (set_valid) {
							node->set(snames[nprops[j].name], value, &valid);
						}
						if (p_edit_state == GEN_EDIT_STATE_INSTANCE && value.get_type() != Variant::OBJECT) {
							value = value.duplicate(true); // Duplicate arrays and dictionaries for the editor.
						}
					}
				}
				if (!missing_resource_properties.is_empty()) {
					node->set_meta(META_MISSING_RESOURCES, missing_resource_properties);
				}

				for (KeyValue<Ref<Resource>, Ref<Resource>> &E : resources_local_to_sub_scene) {
					if (E.value->get_local_scene() == node) {
						E.value->setup_local_to_scene(); // Setup may be required for the resource to work properly.
					}
				}
			}

			//name

			//groups
			for (int j = 0; j < n.groups.size(); j++) {
				ERR_FAIL_INDEX_V(n.groups[j], sname_count, nullptr);
				node->add_to_group(snames[n.groups[j]], true);
			}

			if (n.instance >= 0 || n.type != TYPE_INSTANTIATED || i == 0) {
				//if node was not part of instance, must set its name, parenthood and ownership
				if (i > 0) {
					if (parent) {
						bool pending_add = true;
#ifdef TOOLS_ENABLED
						if (Engine::get_singleton()->is_editor_hint()) {
							Node *existing = parent->_get_child_by_name(snames[n.name]);
							if (existing) {
								// There's already a node in the same parent with the same name.
								// This means that somehow the node was added both to the scene being
								// loaded and another one instantiated in the former, maybe because of
								// manual editing, or a bug in scene saving, or a loophole in the workflow
								// (with any of the bugs possibly already fixed).
								// Bring consistency back by letting it be assigned a non-clashing name.
								// This simple workaround at least avoids leaks and helps the user realize
								// something awkward has happened.
								if (instantiation_warn_notify) {
									instantiation_warn_notify(vformat(
											TTR("An incoming node's name clashes with %s already in the scene (presumably, from a more nested instance).\nThe less nested node will be renamed. Please fix and re-save the scene."),
											ret_nodes[0]->get_path_to(existing)));
								}
								node->set_name(snames[n.name]);
								parent->add_child(node, true);
								pending_add = false;
							}
						}
#endif
						if (pending_add) {
							parent->_add_child_nocheck(node, snames[n.name]);
						}
						if (n.index >= 0 && n.index < parent->get_child_count() - 1) {
							parent->move_child(node, n.index);
						}
					} else {
						//it may be possible that an instantiated scene has changed
						//and the node has nowhere to go anymore
						stray_instances.push_back(node); //can't be added, go to stray list
					}
				} else {
					if (Engine::get_singleton()->is_editor_hint()) {
						//validate name if using editor, to avoid broken
						node->set_name(snames[n.name]);
					} else {
						node->_set_name_nocheck(snames[n.name]);
					}
				}
			}

			if (!old_parent_path.is_empty()) {
				node->set_name(old_parent_path + "#" + node->get_name());
			}

			if (n.owner >= 0) {
				NODE_FROM_ID(owner, n.owner);
				if (owner) {
					node->_set_owner_nocheck(owner);
					if (node->data.unique_name_in_owner) {
						node->_acquire_unique_name_in_owner();
					}
				}
			}

			// We only want to deal with pinned flag if instantiating as pure main (no instance, no inheriting.)
			if (p_edit_state == GEN_EDIT_STATE_MAIN) {
				_sanitize_node_pinned_properties(node);
			} else {
				node->remove_meta("_edit_pinned_properties_");
			}
		}

		if (missing_node) {
			missing_node->set_recording_properties(false);
		}

		ret_nodes[i] = node;

		if (node && gen_node_path_cache && ret_nodes[0]) {
			NodePath n2 = ret_nodes[0]->get_path_to(node);
			node_path_cache[n2] = i;
		}
	}

	for (const DeferredNodePathProperties &dnp : deferred_node_paths) {
		// Replace properties stored as NodePaths with actual Nodes.
		if (dnp.value.get_type() == Variant::ARRAY) {
			Array paths = dnp.value;

			bool valid;
			Array array = dnp.base->get(dnp.property, &valid);
			ERR_CONTINUE_EDMSG(!valid, vformat("Failed to get property '%s' from node '%s'.", dnp.property, dnp.base->get_name()));
			array = array.duplicate();

			array.resize(paths.size());
			for (int i = 0; i < array.size(); i++) {
				array.set(i, dnp.base->get_node_or_null(paths[i]));
			}
			dnp.base->set(dnp.property, array);
		} else if (dnp.value.get_type() == Variant::DICTIONARY) {
			Dictionary paths = dnp.value;

			bool valid;
			Dictionary dict = dnp.base->get(dnp.property, &valid);
			ERR_CONTINUE_EDMSG(!valid, vformat("Failed to get property '%s' from node '%s'.", dnp.property, dnp.base->get_name()));
			dict = dict.duplicate();
			bool convert_key = dict.get_typed_key_builtin() == Variant::OBJECT &&
					ClassDB::is_parent_class(dict.get_typed_key_class_name(), "Node");
			bool convert_value = dict.get_typed_value_builtin() == Variant::OBJECT &&
					ClassDB::is_parent_class(dict.get_typed_value_class_name(), "Node");

			for (int i = 0; i < paths.size(); i++) {
				Variant key = paths.get_key_at_index(i);
				if (convert_key) {
					key = dnp.base->get_node_or_null(key);
				}
				Variant value = paths.get_value_at_index(i);
				if (convert_value) {
					value = dnp.base->get_node_or_null(value);
				}
				dict[key] = value;
			}
			dnp.base->set(dnp.property, dict);
		} else {
			dnp.base->set(dnp.property, dnp.base->get_node_or_null(dnp.value));
		}
	}

	for (KeyValue<Ref<Resource>, Ref<Resource>> &E : resources_local_to_scene) {
		if (E.value->get_local_scene() == ret_nodes[0]) {
			E.value->setup_local_to_scene();
		}
	}

	//do connections

	int cc = connections.size();
	const ConnectionData *cdata = connections.ptr();

	for (int i = 0; i < cc; i++) {
		const ConnectionData &c = cdata[i];
		//ERR_FAIL_INDEX_V( c.from, nc, nullptr );
		//ERR_FAIL_INDEX_V( c.to, nc, nullptr );

		NODE_FROM_ID(cfrom, c.from);
		NODE_FROM_ID(cto, c.to);

		if (!cfrom || !cto) {
			continue;
		}

		Callable callable(cto, snames[c.method]);
		if (c.unbinds > 0) {
			callable = callable.unbind(c.unbinds);
		} else if (!c.binds.is_empty()) {
			Vector<Variant> binds;
			if (c.binds.size()) {
				binds.resize(c.binds.size());
				for (int j = 0; j < c.binds.size(); j++) {
					binds.write[j] = props[c.binds[j]];
				}
			}

			const Variant **argptrs = (const Variant **)alloca(sizeof(Variant *) * binds.size());
			for (int j = 0; j < binds.size(); j++) {
				argptrs[j] = &binds[j];
			}
			callable = callable.bindp(argptrs, binds.size());
		}

		cfrom->connect(snames[c.signal], callable, CONNECT_PERSIST | c.flags | (p_edit_state == GEN_EDIT_STATE_MAIN ? 0 : CONNECT_INHERITED));
	}

	//Node *s = ret_nodes[0];

	//remove nodes that could not be added, likely as a result that
	while (stray_instances.size()) {
		memdelete(stray_instances.front()->get());
		stray_instances.pop_front();
	}

	for (int i = 0; i < editable_instances.size(); i++) {
		Node *ei = ret_nodes[0]->get_node_or_null(editable_instances[i]);
		if (ei) {
			ret_nodes[0]->set_editable_instance(ei, true);
		}
	}

	return ret_nodes[0];
}

Variant SceneState::make_local_resource(Variant &p_value, const SceneState::NodeData &p_node_data, HashMap<Ref<Resource>, Ref<Resource>> &p_resources_local_to_sub_scene, Node *p_node, const StringName p_sname, HashMap<Ref<Resource>, Ref<Resource>> &p_resources_local_to_scene, int p_i, Node **p_ret_nodes, SceneState::GenEditState p_edit_state) const {
	Ref<Resource> res = p_value;
	if (res.is_null() || !res->is_local_to_scene()) {
		return p_value;
	}

	if (p_node_data.instance >= 0) { // For the root node of a sub-scene, treat it as part of the sub-scene.
		return get_remap_resource(res, p_resources_local_to_sub_scene, p_node->get(p_sname), p_node);
	} else {
		HashMap<Ref<Resource>, Ref<Resource>>::Iterator E = p_resources_local_to_scene.find(res);
		Node *base = p_i == 0 ? p_node : p_ret_nodes[0];
		if (E) {
			return E->value;
		} else if (p_edit_state == GEN_EDIT_STATE_MAIN) { // For the main scene, use the resource as is
			res->configure_for_local_scene(base, p_resources_local_to_scene);
			p_resources_local_to_scene[res] = res;
			return res;
		} else { // For instances, a copy must be made.
			Ref<Resource> local_dupe = res->duplicate_for_local_scene(base, p_resources_local_to_scene);
			p_resources_local_to_scene[res] = local_dupe;
			return local_dupe;
		}
	}
}

Array SceneState::setup_resources_in_array(Array &p_array_to_scan, const SceneState::NodeData &p_n, HashMap<Ref<Resource>, Ref<Resource>> &p_resources_local_to_sub_scene, Node *p_node, const StringName p_sname, HashMap<Ref<Resource>, Ref<Resource>> &p_resources_local_to_scene, int p_i, Node **p_ret_nodes, SceneState::GenEditState p_edit_state) const {
	for (int i = 0; i < p_array_to_scan.size(); i++) {
		if (p_array_to_scan[i].get_type() == Variant::OBJECT) {
			p_array_to_scan[i] = make_local_resource(p_array_to_scan[i], p_n, p_resources_local_to_sub_scene, p_node, p_sname, p_resources_local_to_scene, p_i, p_ret_nodes, p_edit_state);
		}
	}
	return p_array_to_scan;
}

Dictionary SceneState::setup_resources_in_dictionary(Dictionary &p_dictionary_to_scan, const SceneState::NodeData &p_n, HashMap<Ref<Resource>, Ref<Resource>> &p_resources_local_to_sub_scene, Node *p_node, const StringName p_sname, HashMap<Ref<Resource>, Ref<Resource>> &p_resources_local_to_scene, int p_i, Node **p_ret_nodes, SceneState::GenEditState p_edit_state) const {
	const Array keys = p_dictionary_to_scan.keys();
	const Array values = p_dictionary_to_scan.values();

	if (has_local_resource(values) || has_local_resource(keys)) {
		Array duplicated_keys = keys.duplicate(true);
		Array duplicated_values = values.duplicate(true);

		duplicated_keys = setup_resources_in_array(duplicated_keys, p_n, p_resources_local_to_sub_scene, p_node, p_sname, p_resources_local_to_scene, p_i, p_ret_nodes, p_edit_state);
		duplicated_values = setup_resources_in_array(duplicated_values, p_n, p_resources_local_to_sub_scene, p_node, p_sname, p_resources_local_to_scene, p_i, p_ret_nodes, p_edit_state);
		p_dictionary_to_scan.clear();

		for (int i = 0; i < keys.size(); i++) {
			p_dictionary_to_scan[duplicated_keys[i]] = duplicated_values[i];
		}
	}

	return p_dictionary_to_scan;
}

bool SceneState::has_local_resource(const Array &p_array) const {
	for (int i = 0; i < p_array.size(); i++) {
		Ref<Resource> res = p_array[i];
		if (res.is_valid() && res->is_local_to_scene()) {
			return true;
		}
	}
	return false;
}

static int _nm_get_string(const String &p_string, HashMap<StringName, int> &name_map) {
	if (name_map.has(p_string)) {
		return name_map[p_string];
	}

	int idx = name_map.size();
	name_map[p_string] = idx;
	return idx;
}

static int _vm_get_variant(const Variant &p_variant, HashMap<Variant, int, VariantHasher, VariantComparator> &variant_map) {
	if (variant_map.has(p_variant)) {
		return variant_map[p_variant];
	}

	int idx = variant_map.size();
	variant_map[p_variant] = idx;
	return idx;
}

Error SceneState::_parse_node(Node *p_owner, Node *p_node, int p_parent_idx, HashMap<StringName, int> &name_map, HashMap<Variant, int, VariantHasher, VariantComparator> &variant_map, HashMap<Node *, int> &node_map, HashMap<Node *, int> &nodepath_map) {
	// this function handles all the work related to properly packing scenes, be it
	// instantiated or inherited.
	// given the complexity of this process, an attempt will be made to properly
	// document it. if you fail to understand something, please ask!

	//discard nodes that do not belong to be processed
	if (p_node != p_owner && p_node->get_owner() != p_owner && !p_owner->is_editable_instance(p_node->get_owner())) {
		return OK;
	}

	bool is_editable_instance = false;

	// save the child instantiated scenes that are chosen as editable, so they can be restored
	// upon load back
	if (p_node != p_owner && !p_node->get_scene_file_path().is_empty() && p_owner->is_editable_instance(p_node)) {
		editable_instances.push_back(p_owner->get_path_to(p_node));
		// Node is the root of an editable instance.
		is_editable_instance = true;
	} else if (p_node->get_owner() && p_owner->is_ancestor_of(p_node->get_owner()) && p_owner->is_editable_instance(p_node->get_owner())) {
		// Node is part of an editable instance.
		is_editable_instance = true;
	}

	NodeData nd;

	nd.name = _nm_get_string(p_node->get_name(), name_map);
	nd.instance = -1; //not instantiated by default

	//really convoluted condition, but it basically checks that index is only saved when part of an inherited scene OR the node parent is from the edited scene
	if (p_owner->get_scene_inherited_state().is_null() && (p_node == p_owner || (p_node->get_owner() == p_owner && (p_node->get_parent() == p_owner || p_node->get_parent()->get_owner() == p_owner)))) {
		//do not save index, because it belongs to saved scene and scene is not inherited
		nd.index = -1;
	} else if (p_node == p_owner) {
		//This (hopefully) happens if the node is a scene root, so its index is irrelevant.
		nd.index = -1;
	} else {
		//part of an inherited scene, or parent is from an instantiated scene
		nd.index = p_node->get_index();
	}

	// if this node is part of an instantiated scene or sub-instantiated scene
	// we need to get the corresponding instance states.
	// with the instance states, we can query for identical properties/groups
	// and only save what has changed

	bool instantiated_by_owner = false;
	Vector<SceneState::PackState> states_stack = PropertyUtils::get_node_states_stack(p_node, p_owner, &instantiated_by_owner);

	if (!p_node->get_scene_file_path().is_empty() && p_node->get_owner() == p_owner && instantiated_by_owner) {
		if (p_node->get_scene_instance_load_placeholder()) {
			//it's a placeholder, use the placeholder path
			nd.instance = _vm_get_variant(p_node->get_scene_file_path(), variant_map);
			nd.instance |= FLAG_INSTANCE_IS_PLACEHOLDER;
		} else {
			//must instance ourselves
			Ref<PackedScene> instance = ResourceLoader::load(p_node->get_scene_file_path());
			if (instance.is_null()) {
				return ERR_CANT_OPEN;
			}

			nd.instance = _vm_get_variant(instance, variant_map);
		}
	}

	// all setup, we then proceed to check all properties for the node
	// and save the ones that are worth saving

	List<PropertyInfo> plist;
	p_node->get_property_list(&plist);

	Array pinned_props = _sanitize_node_pinned_properties(p_node);
	Dictionary missing_resource_properties = p_node->get_meta(META_MISSING_RESOURCES, Dictionary());

	for (const PropertyInfo &E : plist) {
		if (!(E.usage & PROPERTY_USAGE_STORAGE) && !missing_resource_properties.has(E.name)) {
			continue;
		}

		if (E.name == META_PROPERTY_MISSING_RESOURCES) {
			continue; // Ignore this property when packing.
		}

		// If instance or inheriting, not saving if property requested so.
		if (!states_stack.is_empty()) {
			if ((E.usage & PROPERTY_USAGE_NO_INSTANCE_STATE)) {
				continue;
			}
		}

		StringName name = E.name;
		Variant value = p_node->get(name);
		bool use_deferred_node_path_bit = false;

		if (E.type == Variant::OBJECT && E.hint == PROPERTY_HINT_NODE_TYPE) {
			if (value.get_type() == Variant::OBJECT) {
				if (Node *n = Object::cast_to<Node>(value)) {
					value = p_node->get_path_to(n);
				}
				use_deferred_node_path_bit = true;
			}
			if (value.get_type() != Variant::NODE_PATH) {
				continue; //was never set, ignore.
			}
		} else if (E.type == Variant::OBJECT && missing_resource_properties.has(E.name)) {
			// Was this missing resource overridden? If so do not save the old value.
			Ref<Resource> ures = value;
			if (ures.is_null()) {
				value = missing_resource_properties[E.name];
			}
		} else if (E.type == Variant::ARRAY && E.hint == PROPERTY_HINT_TYPE_STRING) {
			int hint_subtype_separator = E.hint_string.find_char(':');
			if (hint_subtype_separator >= 0) {
				String subtype_string = E.hint_string.substr(0, hint_subtype_separator);
				int slash_pos = subtype_string.find_char('/');
				PropertyHint subtype_hint = PropertyHint::PROPERTY_HINT_NONE;
				if (slash_pos >= 0) {
					subtype_hint = PropertyHint(subtype_string.get_slice("/", 1).to_int());
					subtype_string = subtype_string.substr(0, slash_pos);
				}
				Variant::Type subtype = Variant::Type(subtype_string.to_int());

				if (subtype == Variant::OBJECT && subtype_hint == PROPERTY_HINT_NODE_TYPE) {
					use_deferred_node_path_bit = true;
					Array array = value;
					Array new_array;
					for (int i = 0; i < array.size(); i++) {
						Variant elem = array[i];
						if (elem.get_type() == Variant::OBJECT) {
							if (Node *n = Object::cast_to<Node>(elem)) {
								new_array.push_back(p_node->get_path_to(n));
								continue;
							}
						}
						new_array.push_back(elem);
					}
					value = new_array;
				}
			}
		} else if (E.type == Variant::DICTIONARY && E.hint == PROPERTY_HINT_TYPE_STRING) {
			int key_value_separator = E.hint_string.find_char(';');
			if (key_value_separator >= 0) {
				int key_subtype_separator = E.hint_string.find_char(':');
				String key_subtype_string = E.hint_string.substr(0, key_subtype_separator);
				int key_slash_pos = key_subtype_string.find_char('/');
				PropertyHint key_subtype_hint = PropertyHint::PROPERTY_HINT_NONE;
				if (key_slash_pos >= 0) {
					key_subtype_hint = PropertyHint(key_subtype_string.get_slice("/", 1).to_int());
					key_subtype_string = key_subtype_string.substr(0, key_slash_pos);
				}
				Variant::Type key_subtype = Variant::Type(key_subtype_string.to_int());
				bool convert_key = key_subtype == Variant::OBJECT && key_subtype_hint == PROPERTY_HINT_NODE_TYPE;

				int value_subtype_separator = E.hint_string.find_char(':', key_value_separator) - (key_value_separator + 1);
				String value_subtype_string = E.hint_string.substr(key_value_separator + 1, value_subtype_separator);
				int value_slash_pos = value_subtype_string.find_char('/');
				PropertyHint value_subtype_hint = PropertyHint::PROPERTY_HINT_NONE;
				if (value_slash_pos >= 0) {
					value_subtype_hint = PropertyHint(value_subtype_string.get_slice("/", 1).to_int());
					value_subtype_string = value_subtype_string.substr(0, value_slash_pos);
				}
				Variant::Type value_subtype = Variant::Type(value_subtype_string.to_int());
				bool convert_value = value_subtype == Variant::OBJECT && value_subtype_hint == PROPERTY_HINT_NODE_TYPE;

				if (convert_key || convert_value) {
					use_deferred_node_path_bit = true;
					Dictionary dict = value;
					Dictionary new_dict;
					for (int i = 0; i < dict.size(); i++) {
						Variant new_key = dict.get_key_at_index(i);
						if (convert_key && new_key.get_type() == Variant::OBJECT) {
							if (Node *n = Object::cast_to<Node>(new_key)) {
								new_key = p_node->get_path_to(n);
							}
						}
						Variant new_value = dict.get_value_at_index(i);
						if (convert_value && new_value.get_type() == Variant::OBJECT) {
							if (Node *n = Object::cast_to<Node>(new_value)) {
								new_value = p_node->get_path_to(n);
							}
						}
						new_dict[new_key] = new_value;
					}
					value = new_dict;
				}
			}
		}

		if (!pinned_props.has(name)) {
			bool is_valid_default = false;
			Variant default_value = PropertyUtils::get_property_default_value(p_node, name, &is_valid_default, &states_stack, true);

			if (is_valid_default && !PropertyUtils::is_property_value_different(p_node, value, default_value)) {
				if (value.get_type() == Variant::ARRAY && has_local_resource(value)) {
					// Save anyway
				} else if (value.get_type() == Variant::DICTIONARY) {
					Dictionary dictionary = value;
					if (!has_local_resource(dictionary.values()) && !has_local_resource(dictionary.keys())) {
						continue;
					}
				} else {
					continue;
				}
			}
		}

		NodeData::Property prop;
		prop.name = _nm_get_string(name, name_map);
		prop.value = _vm_get_variant(value, variant_map);
		if (use_deferred_node_path_bit) {
			prop.name |= FLAG_PATH_PROPERTY_IS_NODE;
		}
		nd.properties.push_back(prop);
	}

	// save the groups this node is into
	// discard groups that come from the original scene

	List<Node::GroupInfo> groups;
	p_node->get_groups(&groups);
	for (const Node::GroupInfo &gi : groups) {
		if (!gi.persistent) {
			continue;
		}

		bool skip = false;
		for (const SceneState::PackState &ia : states_stack) {
			//check all levels of pack to see if the group was added somewhere
			if (ia.state->is_node_in_group(ia.node, gi.name)) {
				skip = true;
				break;
			}
		}

		if (skip) {
			continue;
		}

		nd.groups.push_back(_nm_get_string(gi.name, name_map));
	}

	// save the right owner
	// for the saved scene root this is -1
	// for nodes of the saved scene this is 0
	// for nodes of instantiated scenes this is >0

	if (p_node == p_owner) {
		//saved scene root
		nd.owner = -1;
	} else if (p_node->get_owner() == p_owner) {
		//part of saved scene
		nd.owner = 0;
	} else {
		nd.owner = -1;
	}

	MissingNode *missing_node = Object::cast_to<MissingNode>(p_node);

	// Save the right type. If this node was created by an instance
	// then flag that the node should not be created but reused
	if (states_stack.is_empty() && !is_editable_instance) {
		//This node is not part of an instantiation process, so save the type.
		if (missing_node != nullptr) {
			// It's a missing node (type non existent on load).
			nd.type = _nm_get_string(missing_node->get_original_class(), name_map);
		} else {
			nd.type = _nm_get_string(p_node->get_class(), name_map);
		}
	} else {
		// this node is part of an instantiated process, so do not save the type.
		// instead, save that it was instantiated
		nd.type = TYPE_INSTANTIATED;
	}

	// determine whether to save this node or not
	// if this node is part of an instantiated sub-scene, we can skip storing it if basically
	// no properties changed and no groups were added to it.
	// below condition is true for all nodes of the scene being saved, and ones in subscenes
	// that hold changes

	bool save_node = nd.properties.size() || nd.groups.size(); // some local properties or groups exist
	save_node = save_node || p_node == p_owner; // owner is always saved
	save_node = save_node || (p_node->get_owner() == p_owner && instantiated_by_owner); //part of scene and not instanced

	int idx = nodes.size();
	int parent_node = NO_PARENT_SAVED;

	if (save_node) {
		//don't save the node if nothing and subscene

		node_map[p_node] = idx;

		//ok validate parent node
		if (p_parent_idx == NO_PARENT_SAVED) {
			int sidx;
			if (nodepath_map.has(p_node->get_parent())) {
				sidx = nodepath_map[p_node->get_parent()];
			} else {
				sidx = nodepath_map.size();
				nodepath_map[p_node->get_parent()] = sidx;
			}

			nd.parent = FLAG_ID_IS_PATH | sidx;
		} else {
			nd.parent = p_parent_idx;
		}

		parent_node = idx;
		nodes.push_back(nd);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *c = p_node->get_child(i);
		Error err = _parse_node(p_owner, c, parent_node, name_map, variant_map, node_map, nodepath_map);
		if (err) {
			return err;
		}
	}

	return OK;
}

Error SceneState::_parse_connections(Node *p_owner, Node *p_node, HashMap<StringName, int> &name_map, HashMap<Variant, int, VariantHasher, VariantComparator> &variant_map, HashMap<Node *, int> &node_map, HashMap<Node *, int> &nodepath_map) {
	// Ignore nodes that are within a scene instance.
	if (p_node != p_owner && p_node->get_owner() && p_node->get_owner() != p_owner && !p_owner->is_editable_instance(p_node->get_owner())) {
		return OK;
	}

	List<MethodInfo> _signals;
	p_node->get_signal_list(&_signals);
	_signals.sort();

	//ERR_FAIL_COND_V( !node_map.has(p_node), ERR_BUG);
	//NodeData &nd = nodes[node_map[p_node]];

	for (const MethodInfo &E : _signals) {
		List<Node::Connection> conns;
		p_node->get_signal_connection_list(E.name, &conns);

		conns.sort();

		for (const Node::Connection &F : conns) {
			const Node::Connection &c = F;

			// Don't save connections that are not persistent.
			if (!(c.flags & CONNECT_PERSIST)) {
				continue;
			}

			// only connections that originate or end into main saved scene are saved
			// everything else is discarded

			Node *target = Object::cast_to<Node>(c.callable.get_object());

			if (!target) {
				continue;
			}

			Vector<Variant> binds;
			int unbinds = 0;
			Callable base_callable;

			if (c.callable.is_custom()) {
				CallableCustomBind *ccb = dynamic_cast<CallableCustomBind *>(c.callable.get_custom());
				if (ccb) {
					binds = ccb->get_binds();
					base_callable = ccb->get_callable();
				}

				CallableCustomUnbind *ccu = dynamic_cast<CallableCustomUnbind *>(c.callable.get_custom());
				if (ccu) {
					unbinds = ccu->get_unbinds();
					base_callable = ccu->get_callable();
				}
			} else {
				base_callable = c.callable;
			}

			//find if this connection already exists
			Node *common_parent = target->find_common_parent_with(p_node);

			ERR_CONTINUE(!common_parent);

			if (common_parent != p_owner && common_parent->get_scene_file_path().is_empty()) {
				common_parent = common_parent->get_owner();
			}

			bool exists = false;

			//go through ownership chain to see if this exists
			while (common_parent) {
				Ref<SceneState> ps;

				if (common_parent == p_owner) {
					ps = common_parent->get_scene_inherited_state();
				} else {
					ps = common_parent->get_scene_instance_state();
				}

				if (ps.is_valid()) {
					NodePath signal_from = common_parent->get_path_to(p_node);
					NodePath signal_to = common_parent->get_path_to(target);

					if (ps->has_connection(signal_from, c.signal.get_name(), signal_to, base_callable.get_method())) {
						exists = true;
						break;
					}
				}

				if (common_parent == p_owner) {
					break;
				} else {
					common_parent = common_parent->get_owner();
				}
			}

			if (exists) { //already exists (comes from instance or inheritance), so don't save
				continue;
			}

			{
				Node *nl = p_node;

				bool exists2 = false;

				while (nl) {
					if (nl == p_owner) {
						Ref<SceneState> state = nl->get_scene_inherited_state();
						if (state.is_valid()) {
							int from_node = state->find_node_by_path(nl->get_path_to(p_node));
							int to_node = state->find_node_by_path(nl->get_path_to(target));

							if (from_node >= 0 && to_node >= 0) {
								//this one has state for this node, save
								if (state->is_connection(from_node, c.signal.get_name(), to_node, base_callable.get_method())) {
									exists2 = true;
									break;
								}
							}
						}

						nl = nullptr;
					} else {
						if (!nl->get_scene_file_path().is_empty()) {
							//is an instance
							Ref<SceneState> state = nl->get_scene_instance_state();
							if (state.is_valid()) {
								int from_node = state->find_node_by_path(nl->get_path_to(p_node));
								int to_node = state->find_node_by_path(nl->get_path_to(target));

								if (from_node >= 0 && to_node >= 0) {
									//this one has state for this node, save
									if (state->is_connection(from_node, c.signal.get_name(), to_node, base_callable.get_method())) {
										exists2 = true;
										break;
									}
								}
							}
						}
						nl = nl->get_owner();
					}
				}

				if (exists2) {
					continue;
				}
			}

			int src_id;

			if (node_map.has(p_node)) {
				src_id = node_map[p_node];
			} else {
				if (nodepath_map.has(p_node)) {
					src_id = FLAG_ID_IS_PATH | nodepath_map[p_node];
				} else {
					int sidx = nodepath_map.size();
					nodepath_map[p_node] = sidx;
					src_id = FLAG_ID_IS_PATH | sidx;
				}
			}

			int target_id;

			if (node_map.has(target)) {
				target_id = node_map[target];
			} else {
				if (nodepath_map.has(target)) {
					target_id = FLAG_ID_IS_PATH | nodepath_map[target];
				} else {
					int sidx = nodepath_map.size();
					nodepath_map[target] = sidx;
					target_id = FLAG_ID_IS_PATH | sidx;
				}
			}

			ConnectionData cd;
			cd.from = src_id;
			cd.to = target_id;
			cd.method = _nm_get_string(base_callable.get_method(), name_map);
			cd.signal = _nm_get_string(c.signal.get_name(), name_map);
			cd.flags = c.flags & ~CONNECT_INHERITED; // Do not store inherited.
			cd.unbinds = unbinds;

			for (int i = 0; i < binds.size(); i++) {
				cd.binds.push_back(_vm_get_variant(binds[i], variant_map));
			}
			connections.push_back(cd);
		}
	}

	// Recursively parse child connections.
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		Error err = _parse_connections(p_owner, child, name_map, variant_map, node_map, nodepath_map);
		if (err) {
			return err;
		}
	}

	return OK;
}

Error SceneState::pack(Node *p_scene) {
	ERR_FAIL_NULL_V(p_scene, ERR_INVALID_PARAMETER);

	clear();

	Node *scene = p_scene;

	HashMap<StringName, int> name_map;
	HashMap<Variant, int, VariantHasher, VariantComparator> variant_map;
	HashMap<Node *, int> node_map;
	HashMap<Node *, int> nodepath_map;

	// If using scene inheritance, pack the scene it inherits from.
	if (scene->get_scene_inherited_state().is_valid()) {
		String scene_path = scene->get_scene_inherited_state()->get_path();
		Ref<PackedScene> instance = ResourceLoader::load(scene_path);
		if (instance.is_valid()) {
			base_scene_idx = _vm_get_variant(instance, variant_map);
		}
	}

	// Instanced, only direct sub-scenes are supported of course.
	Error err = _parse_node(scene, scene, -1, name_map, variant_map, node_map, nodepath_map);
	if (err) {
		clear();
		ERR_FAIL_V(err);
	}

	err = _parse_connections(scene, scene, name_map, variant_map, node_map, nodepath_map);
	if (err) {
		clear();
		ERR_FAIL_V(err);
	}

	names.resize(name_map.size());

	for (const KeyValue<StringName, int> &E : name_map) {
		names.write[E.value] = E.key;
	}

	variants.resize(variant_map.size());

	for (const KeyValue<Variant, int> &E : variant_map) {
		int idx = E.value;
		variants.write[idx] = E.key;
	}

	node_paths.resize(nodepath_map.size());
	for (const KeyValue<Node *, int> &E : nodepath_map) {
		node_paths.write[E.value] = scene->get_path_to(E.key);
	}

	if (Engine::get_singleton()->is_editor_hint()) {
		// Build node path cache
		for (const KeyValue<Node *, int> &E : node_map) {
			node_path_cache[scene->get_path_to(E.key)] = E.value;
		}
	}

	return OK;
}

void SceneState::set_path(const String &p_path) {
	path = p_path;
}

String SceneState::get_path() const {
	return path;
}

void SceneState::clear() {
	names.clear();
	variants.clear();
	nodes.clear();
	connections.clear();
	node_path_cache.clear();
	node_paths.clear();
	editable_instances.clear();
	base_scene_idx = -1;
}

Error SceneState::copy_from(const Ref<SceneState> &p_scene_state) {
	ERR_FAIL_COND_V(p_scene_state.is_null(), ERR_INVALID_PARAMETER);

	clear();

	for (const StringName &E : p_scene_state->names) {
		names.append(E);
	}
	for (const Variant &E : p_scene_state->variants) {
		variants.append(E);
	}
	for (const SceneState::NodeData &E : p_scene_state->nodes) {
		nodes.append(E);
	}
	for (const SceneState::ConnectionData &E : p_scene_state->connections) {
		connections.append(E);
	}
	for (KeyValue<NodePath, int> &E : p_scene_state->node_path_cache) {
		node_path_cache.insert(E.key, E.value);
	}
	for (const NodePath &E : p_scene_state->node_paths) {
		node_paths.append(E);
	}
	for (const NodePath &E : p_scene_state->editable_instances) {
		editable_instances.append(E);
	}
	base_scene_idx = p_scene_state->base_scene_idx;

	return OK;
}

Ref<SceneState> SceneState::get_base_scene_state() const {
	if (base_scene_idx >= 0) {
		Ref<PackedScene> ps = variants[base_scene_idx];
		if (ps.is_valid()) {
			return ps->get_state();
		}
	}

	return Ref<SceneState>();
}

int SceneState::find_node_by_path(const NodePath &p_node) const {
	ERR_FAIL_COND_V_MSG(node_path_cache.is_empty(), -1, "This operation requires the node cache to have been built.");

	if (!node_path_cache.has(p_node)) {
		if (get_base_scene_state().is_valid()) {
			int idx = get_base_scene_state()->find_node_by_path(p_node);
			if (idx != -1) {
				int rkey = _find_base_scene_node_remap_key(idx);
				if (rkey == -1) {
					rkey = nodes.size() + base_scene_node_remap.size();
					base_scene_node_remap[rkey] = idx;
				}
				return rkey;
			}
		}
		return -1;
	}

	int nid = node_path_cache[p_node];

	if (get_base_scene_state().is_valid() && !base_scene_node_remap.has(nid)) {
		//for nodes that _do_ exist in current scene, still try to look for
		//the node in the instantiated scene, as a property may be missing
		//from the local one
		int idx = get_base_scene_state()->find_node_by_path(p_node);
		if (idx != -1) {
			base_scene_node_remap[nid] = idx;
		}
	}

	return nid;
}

int SceneState::_find_base_scene_node_remap_key(int p_idx) const {
	for (const KeyValue<int, int> &E : base_scene_node_remap) {
		if (E.value == p_idx) {
			return E.key;
		}
	}
	return -1;
}

Variant SceneState::get_property_value(int p_node, const StringName &p_property, bool &r_found, bool &r_node_deferred) const {
	r_found = false;
	r_node_deferred = false;

	ERR_FAIL_COND_V(p_node < 0, Variant());

	if (p_node < nodes.size()) {
		// Find in built-in nodes.
		int pc = nodes[p_node].properties.size();
		const StringName *namep = names.ptr();

		const NodeData::Property *p = nodes[p_node].properties.ptr();
		for (int i = 0; i < pc; i++) {
			if (p_property == namep[p[i].name & FLAG_PROP_NAME_MASK]) {
				r_found = true;
				r_node_deferred = p[i].name & FLAG_PATH_PROPERTY_IS_NODE;
				return variants[p[i].value];
			}
		}
	}

	// Property not found, try on instance.
	HashMap<int, int>::ConstIterator I = base_scene_node_remap.find(p_node);
	if (I) {
		return get_base_scene_state()->get_property_value(I->value, p_property, r_found, r_node_deferred);
	}

	return Variant();
}

bool SceneState::is_node_in_group(int p_node, const StringName &p_group) const {
	ERR_FAIL_COND_V(p_node < 0, false);

	if (p_node < nodes.size()) {
		const StringName *namep = names.ptr();
		for (int i = 0; i < nodes[p_node].groups.size(); i++) {
			if (namep[nodes[p_node].groups[i]] == p_group) {
				return true;
			}
		}
	}

	if (base_scene_node_remap.has(p_node)) {
		return get_base_scene_state()->is_node_in_group(base_scene_node_remap[p_node], p_group);
	}

	return false;
}

bool SceneState::disable_placeholders = false;

void SceneState::set_disable_placeholders(bool p_disable) {
	disable_placeholders = p_disable;
}

bool SceneState::is_connection(int p_node, const StringName &p_signal, int p_to_node, const StringName &p_to_method) const {
	ERR_FAIL_COND_V(p_node < 0, false);
	ERR_FAIL_COND_V(p_to_node < 0, false);

	if (p_node < nodes.size() && p_to_node < nodes.size()) {
		int signal_idx = -1;
		int method_idx = -1;
		for (int i = 0; i < names.size(); i++) {
			if (names[i] == p_signal) {
				signal_idx = i;
			} else if (names[i] == p_to_method) {
				method_idx = i;
			}
		}

		if (signal_idx >= 0 && method_idx >= 0) {
			//signal and method strings are stored..

			for (int i = 0; i < connections.size(); i++) {
				if (connections[i].from == p_node && connections[i].to == p_to_node && connections[i].signal == signal_idx && connections[i].method == method_idx) {
					return true;
				}
			}
		}
	}

	if (base_scene_node_remap.has(p_node) && base_scene_node_remap.has(p_to_node)) {
		return get_base_scene_state()->is_connection(base_scene_node_remap[p_node], p_signal, base_scene_node_remap[p_to_node], p_to_method);
	}

	return false;
}

void SceneState::set_bundled_scene(const Dictionary &p_dictionary) {
	ERR_FAIL_COND(!p_dictionary.has("names"));
	ERR_FAIL_COND(!p_dictionary.has("variants"));
	ERR_FAIL_COND(!p_dictionary.has("node_count"));
	ERR_FAIL_COND(!p_dictionary.has("nodes"));
	ERR_FAIL_COND(!p_dictionary.has("conn_count"));
	ERR_FAIL_COND(!p_dictionary.has("conns"));
	//ERR_FAIL_COND( !p_dictionary.has("path"));

	int version = 1;
	if (p_dictionary.has("version")) {
		version = p_dictionary["version"];
	}

	ERR_FAIL_COND_MSG(version > PACKED_SCENE_VERSION, "Save format version too new.");

	const int node_count = p_dictionary["node_count"];
	const Vector<int> snodes = p_dictionary["nodes"];
	ERR_FAIL_COND(snodes.size() < node_count);

	const int conn_count = p_dictionary["conn_count"];
	const Vector<int> sconns = p_dictionary["conns"];
	ERR_FAIL_COND(sconns.size() < conn_count);

	Vector<String> snames = p_dictionary["names"];
	if (snames.size()) {
		int namecount = snames.size();
		names.resize(namecount);
		const String *r = snames.ptr();
		for (int i = 0; i < names.size(); i++) {
			names.write[i] = r[i];
		}
	}

	Array svariants = p_dictionary["variants"];

	if (svariants.size()) {
		int varcount = svariants.size();
		variants.resize(varcount);
		for (int i = 0; i < varcount; i++) {
			variants.write[i] = svariants[i];
		}

	} else {
		variants.clear();
	}

	nodes.resize(node_count);
	if (node_count) {
		const int *r = snodes.ptr();
		int idx = 0;
		for (int i = 0; i < node_count; i++) {
			NodeData &nd = nodes.write[i];
			nd.parent = r[idx++];
			nd.owner = r[idx++];
			nd.type = r[idx++];
			uint32_t name_index = r[idx++];
			nd.name = name_index & ((1 << NAME_INDEX_BITS) - 1);
			nd.index = (name_index >> NAME_INDEX_BITS);
			nd.index--; //0 is invalid, stored as 1
			nd.instance = r[idx++];
			nd.properties.resize(r[idx++]);
			for (int j = 0; j < nd.properties.size(); j++) {
				nd.properties.write[j].name = r[idx++];
				nd.properties.write[j].value = r[idx++];
			}
			nd.groups.resize(r[idx++]);
			for (int j = 0; j < nd.groups.size(); j++) {
				nd.groups.write[j] = r[idx++];
			}
		}
	}

	connections.resize(conn_count);
	if (conn_count) {
		const int *r = sconns.ptr();
		int idx = 0;
		for (int i = 0; i < conn_count; i++) {
			ConnectionData &cd = connections.write[i];
			cd.from = r[idx++];
			cd.to = r[idx++];
			cd.signal = r[idx++];
			cd.method = r[idx++];
			cd.flags = r[idx++];
			cd.binds.resize(r[idx++]);

			for (int j = 0; j < cd.binds.size(); j++) {
				cd.binds.write[j] = r[idx++];
			}
			if (version >= 3) {
				cd.unbinds = r[idx++];
			}
		}
	}

	Array np;
	if (p_dictionary.has("node_paths")) {
		np = p_dictionary["node_paths"];
	}
	node_paths.resize(np.size());
	for (int i = 0; i < np.size(); i++) {
		node_paths.write[i] = np[i];
	}

	Array ei;
	if (p_dictionary.has("editable_instances")) {
		ei = p_dictionary["editable_instances"];
	}

	if (p_dictionary.has("base_scene")) {
		base_scene_idx = p_dictionary["base_scene"];
	}

	editable_instances.resize(ei.size());
	for (int i = 0; i < editable_instances.size(); i++) {
		editable_instances.write[i] = ei[i];
	}

	//path=p_dictionary["path"];
}

Dictionary SceneState::get_bundled_scene() const {
	Vector<String> rnames;
	rnames.resize(names.size());

	if (names.size()) {
		String *r = rnames.ptrw();

		for (int i = 0; i < names.size(); i++) {
			r[i] = names[i];
		}
	}

	Dictionary d;
	d["names"] = rnames;
	d["variants"] = variants;

	Vector<int> rnodes;
	d["node_count"] = nodes.size();

	for (int i = 0; i < nodes.size(); i++) {
		const NodeData &nd = nodes[i];
		rnodes.push_back(nd.parent);
		rnodes.push_back(nd.owner);
		rnodes.push_back(nd.type);
		uint32_t name_index = nd.name;
		if (nd.index < (1 << (32 - NAME_INDEX_BITS)) - 1) { //save if less than 16k children
			name_index |= uint32_t(nd.index + 1) << NAME_INDEX_BITS; //for backwards compatibility, index 0 is no index
		}
		rnodes.push_back(name_index);
		rnodes.push_back(nd.instance);
		rnodes.push_back(nd.properties.size());
		for (int j = 0; j < nd.properties.size(); j++) {
			rnodes.push_back(nd.properties[j].name);
			rnodes.push_back(nd.properties[j].value);
		}
		rnodes.push_back(nd.groups.size());
		for (int j = 0; j < nd.groups.size(); j++) {
			rnodes.push_back(nd.groups[j]);
		}
	}

	d["nodes"] = rnodes;

	Vector<int> rconns;
	d["conn_count"] = connections.size();

	for (int i = 0; i < connections.size(); i++) {
		const ConnectionData &cd = connections[i];
		rconns.push_back(cd.from);
		rconns.push_back(cd.to);
		rconns.push_back(cd.signal);
		rconns.push_back(cd.method);
		rconns.push_back(cd.flags);
		rconns.push_back(cd.binds.size());
		for (int j = 0; j < cd.binds.size(); j++) {
			rconns.push_back(cd.binds[j]);
		}
		rconns.push_back(cd.unbinds);
	}

	d["conns"] = rconns;

	Array rnode_paths;
	rnode_paths.resize(node_paths.size());
	for (int i = 0; i < node_paths.size(); i++) {
		rnode_paths[i] = node_paths[i];
	}
	d["node_paths"] = rnode_paths;

	Array reditable_instances;
	reditable_instances.resize(editable_instances.size());
	for (int i = 0; i < editable_instances.size(); i++) {
		reditable_instances[i] = editable_instances[i];
	}
	d["editable_instances"] = reditable_instances;
	if (base_scene_idx >= 0) {
		d["base_scene"] = base_scene_idx;
	}

	d["version"] = PACKED_SCENE_VERSION;

	return d;
}

int SceneState::get_node_count() const {
	return nodes.size();
}

StringName SceneState::get_node_type(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), StringName());
	if (nodes[p_idx].type == TYPE_INSTANTIATED) {
		return StringName();
	}
	return names[nodes[p_idx].type];
}

StringName SceneState::get_node_name(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), StringName());
	return names[nodes[p_idx].name];
}

int SceneState::get_node_index(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), -1);
	return nodes[p_idx].index;
}

bool SceneState::is_node_instance_placeholder(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), false);

	return nodes[p_idx].instance >= 0 && (nodes[p_idx].instance & FLAG_INSTANCE_IS_PLACEHOLDER);
}

Ref<PackedScene> SceneState::get_node_instance(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), Ref<PackedScene>());

	if (nodes[p_idx].instance >= 0) {
		if (nodes[p_idx].instance & FLAG_INSTANCE_IS_PLACEHOLDER) {
			return Ref<PackedScene>();
		} else {
			return variants[nodes[p_idx].instance & FLAG_MASK];
		}
	} else if (nodes[p_idx].parent < 0 || nodes[p_idx].parent == NO_PARENT_SAVED) {
		if (base_scene_idx >= 0) {
			return variants[base_scene_idx];
		}
	}

	return Ref<PackedScene>();
}

String SceneState::get_node_instance_placeholder(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), String());

	if (nodes[p_idx].instance >= 0 && (nodes[p_idx].instance & FLAG_INSTANCE_IS_PLACEHOLDER)) {
		return variants[nodes[p_idx].instance & FLAG_MASK];
	}

	return String();
}

Vector<StringName> SceneState::get_node_groups(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), Vector<StringName>());
	Vector<StringName> groups;
	for (int i = 0; i < nodes[p_idx].groups.size(); i++) {
		groups.push_back(names[nodes[p_idx].groups[i]]);
	}
	return groups;
}

NodePath SceneState::get_node_path(int p_idx, bool p_for_parent) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), NodePath());

	if (nodes[p_idx].parent < 0 || nodes[p_idx].parent == NO_PARENT_SAVED) {
		if (p_for_parent) {
			return NodePath();
		} else {
			return NodePath(".");
		}
	}

	Vector<StringName> sub_path;
	NodePath base_path;
	int nidx = p_idx;
	while (true) {
		if (nodes[nidx].parent == NO_PARENT_SAVED || nodes[nidx].parent < 0) {
			sub_path.insert(0, ".");
			break;
		}

		if (!p_for_parent || p_idx != nidx) {
			sub_path.insert(0, names[nodes[nidx].name]);
		}

		if (nodes[nidx].parent & FLAG_ID_IS_PATH) {
			base_path = node_paths[nodes[nidx].parent & FLAG_MASK];
			break;
		} else {
			nidx = nodes[nidx].parent & FLAG_MASK;
		}
	}

	for (int i = base_path.get_name_count() - 1; i >= 0; i--) {
		sub_path.insert(0, base_path.get_name(i));
	}

	if (sub_path.is_empty()) {
		return NodePath(".");
	}

	return NodePath(sub_path, false);
}

int SceneState::get_node_property_count(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), -1);
	return nodes[p_idx].properties.size();
}

StringName SceneState::get_node_property_name(int p_idx, int p_prop) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), StringName());
	ERR_FAIL_INDEX_V(p_prop, nodes[p_idx].properties.size(), StringName());
	return names[nodes[p_idx].properties[p_prop].name & FLAG_PROP_NAME_MASK];
}

Vector<String> SceneState::get_node_deferred_nodepath_properties(int p_idx) const {
	Vector<String> ret;
	ERR_FAIL_COND_V(p_idx < 0, ret);

	if (p_idx < nodes.size()) {
		// Find in built-in nodes.
		for (int i = 0; i < nodes[p_idx].properties.size(); i++) {
			uint32_t idx = nodes[p_idx].properties[i].name;
			if (idx & FLAG_PATH_PROPERTY_IS_NODE) {
				ret.push_back(names[idx & FLAG_PROP_NAME_MASK]);
			}
		}
		return ret;
	}

	// Property not found, try on instance.
	HashMap<int, int>::ConstIterator I = base_scene_node_remap.find(p_idx);
	if (I) {
		return get_base_scene_state()->get_node_deferred_nodepath_properties(I->value);
	}

	return ret;
}

Variant SceneState::get_node_property_value(int p_idx, int p_prop) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), Variant());
	ERR_FAIL_INDEX_V(p_prop, nodes[p_idx].properties.size(), Variant());

	return variants[nodes[p_idx].properties[p_prop].value];
}

NodePath SceneState::get_node_owner_path(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, nodes.size(), NodePath());
	if (nodes[p_idx].owner < 0 || nodes[p_idx].owner == NO_PARENT_SAVED) {
		return NodePath(); //root likely
	}
	if (nodes[p_idx].owner & FLAG_ID_IS_PATH) {
		return node_paths[nodes[p_idx].owner & FLAG_MASK];
	} else {
		return get_node_path(nodes[p_idx].owner & FLAG_MASK);
	}
}

int SceneState::get_connection_count() const {
	return connections.size();
}

NodePath SceneState::get_connection_source(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, connections.size(), NodePath());
	if (connections[p_idx].from & FLAG_ID_IS_PATH) {
		return node_paths[connections[p_idx].from & FLAG_MASK];
	} else {
		return get_node_path(connections[p_idx].from & FLAG_MASK);
	}
}

StringName SceneState::get_connection_signal(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, connections.size(), StringName());
	return names[connections[p_idx].signal];
}

NodePath SceneState::get_connection_target(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, connections.size(), NodePath());
	if (connections[p_idx].to & FLAG_ID_IS_PATH) {
		return node_paths[connections[p_idx].to & FLAG_MASK];
	} else {
		return get_node_path(connections[p_idx].to & FLAG_MASK);
	}
}

StringName SceneState::get_connection_method(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, connections.size(), StringName());
	return names[connections[p_idx].method];
}

int SceneState::get_connection_flags(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, connections.size(), -1);
	return connections[p_idx].flags;
}

int SceneState::get_connection_unbinds(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, connections.size(), -1);
	return connections[p_idx].unbinds;
}

Array SceneState::get_connection_binds(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, connections.size(), Array());
	Array binds;
	for (int i = 0; i < connections[p_idx].binds.size(); i++) {
		binds.push_back(variants[connections[p_idx].binds[i]]);
	}
	return binds;
}

bool SceneState::has_connection(const NodePath &p_node_from, const StringName &p_signal, const NodePath &p_node_to, const StringName &p_method, bool p_no_inheritance) {
	// this method cannot be const because of this
	Ref<SceneState> ss = this;

	do {
		for (int i = 0; i < ss->connections.size(); i++) {
			const ConnectionData &c = ss->connections[i];

			NodePath np_from;

			if (c.from & FLAG_ID_IS_PATH) {
				np_from = ss->node_paths[c.from & FLAG_MASK];
			} else {
				np_from = ss->get_node_path(c.from);
			}

			NodePath np_to;

			if (c.to & FLAG_ID_IS_PATH) {
				np_to = ss->node_paths[c.to & FLAG_MASK];
			} else {
				np_to = ss->get_node_path(c.to);
			}

			StringName sn_signal = ss->names[c.signal];
			StringName sn_method = ss->names[c.method];

			if (np_from == p_node_from && sn_signal == p_signal && np_to == p_node_to && sn_method == p_method) {
				return true;
			}
		}

		if (p_no_inheritance) {
			break;
		}

		ss = ss->get_base_scene_state();
	} while (ss.is_valid());

	return false;
}

Vector<NodePath> SceneState::get_editable_instances() const {
	return editable_instances;
}

Ref<Resource> SceneState::get_sub_resource(const String &p_path) {
	for (const Variant &v : variants) {
		const Ref<Resource> &res = v;
		if (res.is_valid() && res->get_path() == p_path) {
			return res;
		}
	}
	return Ref<Resource>();
}

//add

int SceneState::add_name(const StringName &p_name) {
	names.push_back(p_name);
	return names.size() - 1;
}

int SceneState::add_value(const Variant &p_value) {
	variants.push_back(p_value);
	return variants.size() - 1;
}

int SceneState::add_node_path(const NodePath &p_path) {
	node_paths.push_back(p_path);
	return (node_paths.size() - 1) | FLAG_ID_IS_PATH;
}

int SceneState::add_node(int p_parent, int p_owner, int p_type, int p_name, int p_instance, int p_index) {
	NodeData nd;
	nd.parent = p_parent;
	nd.owner = p_owner;
	nd.type = p_type;
	nd.name = p_name;
	nd.instance = p_instance;
	nd.index = p_index;

	nodes.push_back(nd);

	return nodes.size() - 1;
}

void SceneState::add_node_property(int p_node, int p_name, int p_value, bool p_deferred_node_path) {
	ERR_FAIL_INDEX(p_node, nodes.size());
	ERR_FAIL_INDEX(p_name, names.size());
	ERR_FAIL_INDEX(p_value, variants.size());

	NodeData::Property prop;
	prop.name = p_name;
	if (p_deferred_node_path) {
		prop.name |= FLAG_PATH_PROPERTY_IS_NODE;
	}
	prop.value = p_value;
	nodes.write[p_node].properties.push_back(prop);
}

void SceneState::add_node_group(int p_node, int p_group) {
	ERR_FAIL_INDEX(p_node, nodes.size());
	ERR_FAIL_INDEX(p_group, names.size());
	nodes.write[p_node].groups.push_back(p_group);
}

void SceneState::set_base_scene(int p_idx) {
	ERR_FAIL_INDEX(p_idx, variants.size());
	base_scene_idx = p_idx;
}

void SceneState::add_connection(int p_from, int p_to, int p_signal, int p_method, int p_flags, int p_unbinds, const Vector<int> &p_binds) {
	ERR_FAIL_INDEX(p_signal, names.size());
	ERR_FAIL_INDEX(p_method, names.size());

	for (int i = 0; i < p_binds.size(); i++) {
		ERR_FAIL_INDEX(p_binds[i], variants.size());
	}
	ConnectionData c;
	c.from = p_from;
	c.to = p_to;
	c.signal = p_signal;
	c.method = p_method;
	c.flags = p_flags;
	c.unbinds = p_unbinds;
	c.binds = p_binds;
	connections.push_back(c);
}

void SceneState::add_editable_instance(const NodePath &p_path) {
	editable_instances.push_back(p_path);
}

bool SceneState::remove_group_references(const StringName &p_name) {
	bool edited = false;
	for (NodeData &node : nodes) {
		for (const int &group : node.groups) {
			if (names[group] == p_name) {
				node.groups.erase(group);
				edited = true;
				break;
			}
		}
	}
	return edited;
}

bool SceneState::rename_group_references(const StringName &p_old_name, const StringName &p_new_name) {
	bool edited = false;
	for (const NodeData &node : nodes) {
		for (const int &group : node.groups) {
			if (names[group] == p_old_name) {
				names.write[group] = p_new_name;
				edited = true;
				break;
			}
		}
	}
	return edited;
}

HashSet<StringName> SceneState::get_all_groups() {
	HashSet<StringName> ret;
	for (const NodeData &node : nodes) {
		for (const int &group : node.groups) {
			ret.insert(names[group]);
		}
	}
	return ret;
}

Vector<String> SceneState::_get_node_groups(int p_idx) const {
	Vector<StringName> groups = get_node_groups(p_idx);
	Vector<String> ret;

	for (int i = 0; i < groups.size(); i++) {
		ret.push_back(groups[i]);
	}

	return ret;
}

void SceneState::_bind_methods() {
	//unbuild API

	ClassDB::bind_method(D_METHOD("get_node_count"), &SceneState::get_node_count);
	ClassDB::bind_method(D_METHOD("get_node_type", "idx"), &SceneState::get_node_type);
	ClassDB::bind_method(D_METHOD("get_node_name", "idx"), &SceneState::get_node_name);
	ClassDB::bind_method(D_METHOD("get_node_path", "idx", "for_parent"), &SceneState::get_node_path, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_node_owner_path", "idx"), &SceneState::get_node_owner_path);
	ClassDB::bind_method(D_METHOD("is_node_instance_placeholder", "idx"), &SceneState::is_node_instance_placeholder);
	ClassDB::bind_method(D_METHOD("get_node_instance_placeholder", "idx"), &SceneState::get_node_instance_placeholder);
	ClassDB::bind_method(D_METHOD("get_node_instance", "idx"), &SceneState::get_node_instance);
	ClassDB::bind_method(D_METHOD("get_node_groups", "idx"), &SceneState::_get_node_groups);
	ClassDB::bind_method(D_METHOD("get_node_index", "idx"), &SceneState::get_node_index);
	ClassDB::bind_method(D_METHOD("get_node_property_count", "idx"), &SceneState::get_node_property_count);
	ClassDB::bind_method(D_METHOD("get_node_property_name", "idx", "prop_idx"), &SceneState::get_node_property_name);
	ClassDB::bind_method(D_METHOD("get_node_property_value", "idx", "prop_idx"), &SceneState::get_node_property_value);
	ClassDB::bind_method(D_METHOD("get_connection_count"), &SceneState::get_connection_count);
	ClassDB::bind_method(D_METHOD("get_connection_source", "idx"), &SceneState::get_connection_source);
	ClassDB::bind_method(D_METHOD("get_connection_signal", "idx"), &SceneState::get_connection_signal);
	ClassDB::bind_method(D_METHOD("get_connection_target", "idx"), &SceneState::get_connection_target);
	ClassDB::bind_method(D_METHOD("get_connection_method", "idx"), &SceneState::get_connection_method);
	ClassDB::bind_method(D_METHOD("get_connection_flags", "idx"), &SceneState::get_connection_flags);
	ClassDB::bind_method(D_METHOD("get_connection_binds", "idx"), &SceneState::get_connection_binds);
	ClassDB::bind_method(D_METHOD("get_connection_unbinds", "idx"), &SceneState::get_connection_unbinds);

	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_DISABLED);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_INSTANCE);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_MAIN);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_MAIN_INHERITED);
}

SceneState::SceneState() {
}

////////////////

void PackedScene::_set_bundled_scene(const Dictionary &p_scene) {
	state->set_bundled_scene(p_scene);
}

Dictionary PackedScene::_get_bundled_scene() const {
	return state->get_bundled_scene();
}

Error PackedScene::pack(Node *p_scene) {
	return state->pack(p_scene);
}

void PackedScene::clear() {
	state->clear();
}

void PackedScene::reload_from_file() {
	String path = get_path();
	if (!path.is_resource_file()) {
		return;
	}

	Ref<PackedScene> s = ResourceLoader::load(ResourceLoader::path_remap(path), get_class(), ResourceFormatLoader::CACHE_MODE_IGNORE);
	if (s.is_null()) {
		return;
	}

	// Backup the loaded_state
	Ref<SceneState> loaded_state = s->get_state();
	// This assigns a new state to s->state
	// We do this because of the next step
	s->recreate_state();
	// This has a side-effect to clear s->state
	copy_from(s);
	// Then, we copy the backed-up loaded_state to state
	state->copy_from(loaded_state);
}

bool PackedScene::can_instantiate() const {
	return state->can_instantiate();
}

Node *PackedScene::instantiate(GenEditState p_edit_state) const {
#ifndef TOOLS_ENABLED
	ERR_FAIL_COND_V_MSG(p_edit_state != GEN_EDIT_STATE_DISABLED, nullptr, "Edit state is only for editors, does not work without tools compiled.");
#endif

	Node *s = state->instantiate((SceneState::GenEditState)p_edit_state);
	if (!s) {
		return nullptr;
	}

	if (p_edit_state != GEN_EDIT_STATE_DISABLED) {
		s->set_scene_instance_state(state);
	}

	if (!is_built_in()) {
		s->set_scene_file_path(get_path());
	}

	s->notification(Node::NOTIFICATION_SCENE_INSTANTIATED);

	return s;
}

void PackedScene::replace_state(Ref<SceneState> p_by) {
	state = p_by;
	state->set_path(get_path());
#ifdef TOOLS_ENABLED
	state->set_last_modified_time(get_last_modified_time());
#endif
}

void PackedScene::recreate_state() {
	state.instantiate();
	state->set_path(get_path());
#ifdef TOOLS_ENABLED
	state->set_last_modified_time(get_last_modified_time());
#endif
}

#ifdef TOOLS_ENABLED
HashSet<StringName> PackedScene::get_scene_groups(const String &p_path) {
	{
		Ref<PackedScene> packed_scene = ResourceCache::get_ref(p_path);
		if (packed_scene.is_valid()) {
			return packed_scene->get_state()->get_all_groups();
		}
	}

	if (p_path.get_extension() == "tscn") {
		Ref<FileAccess> scene_file = FileAccess::open(p_path, FileAccess::READ);
		ERR_FAIL_COND_V(scene_file.is_null(), HashSet<StringName>());

		HashSet<StringName> ret;
		while (!scene_file->eof_reached()) {
			const String line = scene_file->get_line();
			if (!line.begins_with("[node")) {
				continue;
			}

			int i = line.find("groups=[");
			if (i == -1) {
				continue;
			}

			int j = line.find_char(']', i);
			while (i < j) {
				i = line.find_char('"', i);
				if (i == -1) {
					break;
				}

				int k = line.find_char('"', i + 1);
				if (k == -1) {
					break;
				}

				ret.insert(line.substr(i + 1, k - i - 1));
				i = k + 1;
			}
		}
		return ret;
	} else {
		Ref<PackedScene> packed_scene = ResourceLoader::load(p_path);
		ERR_FAIL_COND_V(packed_scene.is_null(), HashSet<StringName>());
		return packed_scene->get_state()->get_all_groups();
	}
}
#endif

Ref<SceneState> PackedScene::get_state() const {
	return state;
}

void PackedScene::set_path(const String &p_path, bool p_take_over) {
	state->set_path(p_path);
	Resource::set_path(p_path, p_take_over);
}

void PackedScene::set_path_cache(const String &p_path) {
	state->set_path(p_path);
	Resource::set_path_cache(p_path);
}

void PackedScene::reset_state() {
	clear();
}
void PackedScene::_bind_methods() {
	ClassDB::bind_method(D_METHOD("pack", "path"), &PackedScene::pack);
	ClassDB::bind_method(D_METHOD("instantiate", "edit_state"), &PackedScene::instantiate, DEFVAL(GEN_EDIT_STATE_DISABLED));
	ClassDB::bind_method(D_METHOD("can_instantiate"), &PackedScene::can_instantiate);
	ClassDB::bind_method(D_METHOD("_set_bundled_scene", "scene"), &PackedScene::_set_bundled_scene);
	ClassDB::bind_method(D_METHOD("_get_bundled_scene"), &PackedScene::_get_bundled_scene);
	ClassDB::bind_method(D_METHOD("get_state"), &PackedScene::get_state);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_bundled", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL), "_set_bundled_scene", "_get_bundled_scene");

	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_DISABLED);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_INSTANCE);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_MAIN);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_MAIN_INHERITED);
}

PackedScene::PackedScene() {
	state.instantiate();
}
