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

#include "core/core_string_names.h"
#include "core/engine.h"
#include "core/io/resource_loader.h"
#include "core/project_settings.h"
#include "editor/editor_inspector.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/spatial.h"
#include "scene/gui/control.h"
#include "scene/main/instance_placeholder.h"
#include "scene/property_utils.h"

#define PACKED_SCENE_VERSION 2

bool SceneState::can_instance() const {
	return nodes.size() > 0;
}

static Array _sanitize_node_pinned_properties(Node *p_node) {
	if (!p_node->has_meta("_edit_pinned_properties_")) {
		return Array();
	}
	Array pinned = p_node->get_meta("_edit_pinned_properties_");
	if (pinned.empty()) {
		return Array();
	}
	Set<StringName> storable_properties;
	p_node->get_storable_properties(storable_properties);
	int i = 0;
	do {
		if (storable_properties.has(pinned[i])) {
			i++;
		} else {
			pinned.remove(i);
		}
	} while (i < pinned.size());
	if (pinned.empty()) {
		p_node->remove_meta("_edit_pinned_properties_");
	}
	return pinned;
}

Node *SceneState::instance(GenEditState p_edit_state) const {
	// nodes where instancing failed (because something is missing)
	List<Node *> stray_instances;

#define NODE_FROM_ID(p_name, p_id)                   \
	Node *p_name;                                    \
	if (p_id & FLAG_ID_IS_PATH) {                    \
		NodePath np = node_paths[p_id & FLAG_MASK];  \
		p_name = ret_nodes[0]->get_node_or_null(np); \
	} else {                                         \
		ERR_FAIL_INDEX_V(p_id &FLAG_MASK, nc, NULL); \
		p_name = ret_nodes[p_id & FLAG_MASK];        \
	}

	int nc = nodes.size();
	ERR_FAIL_COND_V(nc == 0, nullptr);

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

	bool gen_node_path_cache = p_edit_state != GEN_EDIT_STATE_DISABLED && node_path_cache.empty();

	Map<Ref<Resource>, Ref<Resource>> resources_local_to_scene;

	for (int i = 0; i < nc; i++) {
		const NodeData &n = nd[i];

		Node *parent = nullptr;
		String old_parent_path;

		if (i > 0) {
			ERR_FAIL_COND_V_MSG(n.parent == -1, nullptr, vformat("Invalid scene: node %s does not specify its parent node.", snames[n.name]));
			NODE_FROM_ID(nparent, n.parent);
#ifdef DEBUG_ENABLED
			if (!nparent && (n.parent & FLAG_ID_IS_PATH)) {
				WARN_PRINT(String("Parent path '" + String(node_paths[n.parent & FLAG_MASK]) + "' for node '" + String(snames[n.name]) + "' has vanished when instancing: '" + get_path() + "'.").ascii().get_data());
				old_parent_path = String(node_paths[n.parent & FLAG_MASK]).trim_prefix("./").replace("/", "@");
				nparent = ret_nodes[0];
			}
#endif
			parent = nparent;
		} else {
			// i == 0 is root node.
			ERR_FAIL_COND_V_MSG(n.parent != -1, nullptr, vformat("Invalid scene: root node %s cannot specify a parent node.", snames[n.name]));
			ERR_FAIL_COND_V_MSG(n.type == TYPE_INSTANCED && base_scene_idx < 0, nullptr, vformat("Invalid scene: root node %s in an instance, but there's no base scene.", snames[n.name]));
		}

		Node *node = nullptr;

		if (i == 0 && base_scene_idx >= 0) {
			//scene inheritance on root node
			Ref<PackedScene> sdata = props[base_scene_idx];
			ERR_FAIL_COND_V(!sdata.is_valid(), nullptr);
			node = sdata->instance(p_edit_state == GEN_EDIT_STATE_DISABLED ? PackedScene::GEN_EDIT_STATE_DISABLED : PackedScene::GEN_EDIT_STATE_INSTANCE); //only main gets main edit state
			ERR_FAIL_COND_V(!node, nullptr);
			if (p_edit_state != GEN_EDIT_STATE_DISABLED) {
				node->set_scene_inherited_state(sdata->get_state());
			}

		} else if (n.instance >= 0) {
			//instance a scene into this node
			if (n.instance & FLAG_INSTANCE_IS_PLACEHOLDER) {
				String path = props[n.instance & FLAG_MASK];
				if (disable_placeholders) {
					Ref<PackedScene> sdata = ResourceLoader::load(path, "PackedScene");
					ERR_FAIL_COND_V(!sdata.is_valid(), nullptr);
					node = sdata->instance(p_edit_state == GEN_EDIT_STATE_DISABLED ? PackedScene::GEN_EDIT_STATE_DISABLED : PackedScene::GEN_EDIT_STATE_INSTANCE);
					ERR_FAIL_COND_V(!node, nullptr);
				} else {
					InstancePlaceholder *ip = memnew(InstancePlaceholder);
					ip->set_instance_path(path);
					node = ip;
				}
				node->set_scene_instance_load_placeholder(true);
			} else {
				Ref<PackedScene> sdata = props[n.instance & FLAG_MASK];
				ERR_FAIL_COND_V(!sdata.is_valid(), nullptr);
				node = sdata->instance(p_edit_state == GEN_EDIT_STATE_DISABLED ? PackedScene::GEN_EDIT_STATE_DISABLED : PackedScene::GEN_EDIT_STATE_INSTANCE);
				ERR_FAIL_COND_V(!node, nullptr);
			}

		} else if (n.type == TYPE_INSTANCED) {
			//get the node from somewhere, it likely already exists from another instance
			if (parent) {
				node = parent->_get_child_by_name(snames[n.name]);
#ifdef DEBUG_ENABLED
				if (!node) {
					WARN_PRINT(String("Node '" + String(ret_nodes[0]->get_path_to(parent)) + "/" + String(snames[n.name]) + "' was modified from inside an instance, but it has vanished.").ascii().get_data());
				}
#endif
			}
		} else {
			//node belongs to this scene and must be created
			Object *obj = ClassDB::instance(snames[n.type]);

			node = Object::cast_to<Node>(obj);

			if (!node) {
				if (obj) {
					memdelete(obj);
					obj = nullptr;
				}
				WARN_PRINT(vformat("Node %s of type %s cannot be created. A placeholder will be created instead.", snames[n.name], snames[n.type]).ascii().get_data());
				if (n.parent >= 0 && n.parent < nc && ret_nodes[n.parent]) {
					if (Object::cast_to<Spatial>(ret_nodes[n.parent])) {
						obj = memnew(Spatial);
					} else if (Object::cast_to<Control>(ret_nodes[n.parent])) {
						obj = memnew(Control);
					} else if (Object::cast_to<Node2D>(ret_nodes[n.parent])) {
						obj = memnew(Node2D);
					}
				}

				if (!obj) {
					obj = memnew(Node);
				}

				node = Object::cast_to<Node>(obj);
			}
		}

		if (node) {
			// may not have found the node (part of instanced scene and removed)
			// if found all is good, otherwise ignore

			//properties
			int nprop_count = n.properties.size();
			if (nprop_count) {
				const NodeData::Property *nprops = &n.properties[0];

				for (int j = 0; j < nprop_count; j++) {
					bool valid;
					ERR_FAIL_INDEX_V(nprops[j].name, sname_count, nullptr);
					ERR_FAIL_INDEX_V(nprops[j].value, prop_count, nullptr);

					if (snames[nprops[j].name] == CoreStringNames::get_singleton()->_script) {
						//work around to avoid old script variables from disappearing, should be the proper fix to:
						//https://github.com/godotengine/godot/issues/2958

						//store old state
						List<Pair<StringName, Variant>> old_state;
						if (node->get_script_instance()) {
							node->get_script_instance()->get_property_state(old_state);
						}

						node->set(snames[nprops[j].name], props[nprops[j].value], &valid);

						//restore old state for new script, if exists
						for (List<Pair<StringName, Variant>>::Element *E = old_state.front(); E; E = E->next()) {
							node->set(E->get().first, E->get().second);
						}
					} else {
						Variant value = props[nprops[j].value];

						if (value.get_type() == Variant::OBJECT) {
							//handle resources that are local to scene by duplicating them if needed
							Ref<Resource> res = value;
							if (res.is_valid()) {
								if (res->is_local_to_scene()) {
									Map<Ref<Resource>, Ref<Resource>>::Element *E = resources_local_to_scene.find(res);

									if (E) {
										value = E->get();
									} else {
										Node *base = i == 0 ? node : ret_nodes[0];

										if (p_edit_state == GEN_EDIT_STATE_MAIN || p_edit_state == GEN_EDIT_STATE_MAIN_INHERITED) {
											//for the main scene, use the resource as is
											res->configure_for_local_scene(base, resources_local_to_scene);
											resources_local_to_scene[res] = res;

										} else {
											//for instances, a copy must be made
											Node *base2 = i == 0 ? node : ret_nodes[0];
											Ref<Resource> local_dupe = res->duplicate_for_local_scene(base2, resources_local_to_scene);
											resources_local_to_scene[res] = local_dupe;
											res = local_dupe;
											value = local_dupe;
										}
									}
									//must make a copy, because this res is local to scene
								}
							}
						} else if (p_edit_state == GEN_EDIT_STATE_INSTANCE) {
							value = value.duplicate(true); // Duplicate arrays and dictionaries for the editor
						}
						node->set(snames[nprops[j].name], value, &valid);
					}
				}
			}

			//name

			//groups
			for (int j = 0; j < n.groups.size(); j++) {
				ERR_FAIL_INDEX_V(n.groups[j], sname_count, nullptr);
				node->add_to_group(snames[n.groups[j]], true);
			}

			if (n.instance >= 0 || n.type != TYPE_INSTANCED || i == 0) {
				//if node was not part of instance, must set its name, parenthood and ownership
				if (i > 0) {
					if (parent) {
						parent->_add_child_nocheck(node, snames[n.name]);
						if (n.index >= 0 && n.index < parent->get_child_count() - 1) {
							parent->move_child(node, n.index);
						}
					} else {
						//it may be possible that an instanced scene has changed
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

			if (!old_parent_path.empty()) {
				node->_set_name_nocheck(old_parent_path + "@" + node->get_name());
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

			// we only want to deal with pinned flag if instancing as pure main (no instance, no inheriting)
			if (p_edit_state == GEN_EDIT_STATE_MAIN) {
				_sanitize_node_pinned_properties(node);
			} else {
				node->remove_meta("_edit_pinned_properties_");
			}
		}

		ret_nodes[i] = node;

		if (node && gen_node_path_cache && ret_nodes[0]) {
			NodePath n2 = ret_nodes[0]->get_path_to(node);
			node_path_cache[n2] = i;
		}
	}

	for (Map<Ref<Resource>, Ref<Resource>>::Element *E = resources_local_to_scene.front(); E; E = E->next()) {
		E->get()->setup_local_to_scene();
	}

	//do connections

	int cc = connections.size();
	const ConnectionData *cdata = connections.ptr();

	for (int i = 0; i < cc; i++) {
		const ConnectionData &c = cdata[i];
		//ERR_FAIL_INDEX_V( c.from, nc, NULL );
		//ERR_FAIL_INDEX_V( c.to, nc, NULL );

		NODE_FROM_ID(cfrom, c.from);
		NODE_FROM_ID(cto, c.to);

		if (!cfrom || !cto) {
			continue;
		}

		Vector<Variant> binds;
		if (c.binds.size()) {
			binds.resize(c.binds.size());
			for (int j = 0; j < c.binds.size(); j++) {
				binds.write[j] = props[c.binds[j]];
			}
		}

		cfrom->connect(snames[c.signal], cto, snames[c.method], binds, CONNECT_PERSIST | c.flags);
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

static int _nm_get_string(const String &p_string, Map<StringName, int> &name_map) {
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

Error SceneState::_parse_node(Node *p_owner, Node *p_node, int p_parent_idx, Map<StringName, int> &name_map, HashMap<Variant, int, VariantHasher, VariantComparator> &variant_map, Map<Node *, int> &node_map, Map<Node *, int> &nodepath_map) {
	// this function handles all the work related to properly packing scenes, be it
	// instanced or inherited.
	// given the complexity of this process, an attempt will be made to properly
	// document it. if you fail to understand something, please ask!

	//discard nodes that do not belong to be processed
	if (p_node != p_owner && p_node->get_owner() != p_owner && !p_owner->is_editable_instance(p_node->get_owner())) {
		return OK;
	}

	bool is_editable_instance = false;

	// save the child instantiated scenes that are chosen as editable, so they can be restored
	// upon load back
	if (p_node != p_owner && p_node->get_filename() != String() && p_owner->is_editable_instance(p_node)) {
		editable_instances.push_back(p_owner->get_path_to(p_node));
		// Node is the root of an editable instance.
		is_editable_instance = true;
	} else if (p_node->get_owner() && p_owner->is_a_parent_of(p_node->get_owner()) && p_owner->is_editable_instance(p_node->get_owner())) {
		// Node is part of an editable instance.
		is_editable_instance = true;
	}

	NodeData nd;

	nd.name = _nm_get_string(p_node->get_name(), name_map);
	nd.instance = -1; //not instanced by default

	//really convoluted condition, but it basically checks that index is only saved when part of an inherited scene OR the node parent is from the edited scene
	if (p_owner->get_scene_inherited_state().is_null() && (p_node == p_owner || (p_node->get_owner() == p_owner && (p_node->get_parent() == p_owner || p_node->get_parent()->get_owner() == p_owner)))) {
		//do not save index, because it belongs to saved scene and scene is not inherited
		nd.index = -1;
	} else if (p_node == p_owner) {
		//This (hopefully) happens if the node is a scene root, so its index is irrelevant.
		nd.index = -1;
	} else {
		//part of an inherited scene, or parent is from an instanced scene
		nd.index = p_node->get_index();
	}

	// if this node is part of an instanced scene or sub-instanced scene
	// we need to get the corresponding instance states.
	// with the instance states, we can query for identical properties/groups
	// and only save what has changed

	bool instanced_by_owner = false;
	Vector<SceneState::PackState> states_stack = PropertyUtils::get_node_states_stack(p_node, p_owner, &instanced_by_owner);

	if (p_node->get_filename() != String() && p_node->get_owner() == p_owner && instanced_by_owner) {
		if (p_node->get_scene_instance_load_placeholder()) {
			//it's a placeholder, use the placeholder path
			nd.instance = _vm_get_variant(p_node->get_filename(), variant_map);
			nd.instance |= FLAG_INSTANCE_IS_PLACEHOLDER;
		} else {
			//must instance ourselves
			Ref<PackedScene> instance = ResourceLoader::load(p_node->get_filename());
			if (!instance.is_valid()) {
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

	for (List<PropertyInfo>::Element *E = plist.front(); E; E = E->next()) {
		if (!(E->get().usage & PROPERTY_USAGE_STORAGE)) {
			continue;
		}

		Variant forced_value;

		// If instance or inheriting, not saving if property requested so, or it's meta
		if (states_stack.size()) {
			if ((E->get().usage & PROPERTY_USAGE_NO_INSTANCE_STATE)) {
				continue;
			}
			// Meta is normally not saved in instances/inherited (see GH-12838), but we need to save the pinned list
			if (E->get().name == "__meta__") {
				if (pinned_props.size()) {
					Dictionary meta_override;
					meta_override["_edit_pinned_properties_"] = pinned_props;
					forced_value = meta_override;
				}
			}
		}

		String name = E->get().name;
		Variant value = forced_value.get_type() == Variant::NIL ? p_node->get(name) : forced_value;

		if (!pinned_props.has(name) && forced_value.get_type() == Variant::NIL) {
			bool is_valid_default = false;
			Variant default_value = PropertyUtils::get_property_default_value(p_node, name, &is_valid_default, &states_stack, true);
			if (is_valid_default && !PropertyUtils::is_property_value_different(value, default_value)) {
				continue;
			}
		}

		NodeData::Property prop;
		prop.name = _nm_get_string(name, name_map);
		prop.value = _vm_get_variant(value, variant_map);
		nd.properties.push_back(prop);
	}

	// save the groups this node is into
	// discard groups that come from the original scene

	List<Node::GroupInfo> groups;
	p_node->get_groups(&groups);
	for (List<Node::GroupInfo>::Element *E = groups.front(); E; E = E->next()) {
		Node::GroupInfo &gi = E->get();

		if (!gi.persistent) {
			continue;
		}
		/*
		if (instance_state_node>=0 && instance_state->is_node_in_group(instance_state_node,gi.name))
			continue; //group was instanced, don't add here
		*/

		bool skip = false;
		for (int i = 0; i < states_stack.size(); ++i) {
			const auto &ia = states_stack[i];
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
	// for nodes of instanced scenes this is >0

	if (p_node == p_owner) {
		//saved scene root
		nd.owner = -1;
	} else if (p_node->get_owner() == p_owner) {
		//part of saved scene
		nd.owner = 0;
	} else {
		nd.owner = -1;
	}

	// Save the right type. If this node was created by an instance
	// then flag that the node should not be created but reused
	if (states_stack.empty() && !is_editable_instance) {
		//this node is not part of an instancing process, so save the type
		nd.type = _nm_get_string(p_node->get_class(), name_map);
	} else {
		// this node is part of an instanced process, so do not save the type.
		// instead, save that it was instanced
		nd.type = TYPE_INSTANCED;
	}

	// determine whether to save this node or not
	// if this node is part of an instanced sub-scene, we can skip storing it if basically
	// no properties changed and no groups were added to it.
	// below condition is true for all nodes of the scene being saved, and ones in subscenes
	// that hold changes

	bool save_node = nd.properties.size() || nd.groups.size(); // some local properties or groups exist
	save_node = save_node || p_node == p_owner; // owner is always saved
	save_node = save_node || (p_node->get_owner() == p_owner && instanced_by_owner); //part of scene and not instanced

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

Error SceneState::_parse_connections(Node *p_owner, Node *p_node, Map<StringName, int> &name_map, HashMap<Variant, int, VariantHasher, VariantComparator> &variant_map, Map<Node *, int> &node_map, Map<Node *, int> &nodepath_map) {
	if (p_node != p_owner && p_node->get_owner() && p_node->get_owner() != p_owner && !p_owner->is_editable_instance(p_node->get_owner())) {
		return OK;
	}

	List<MethodInfo> _signals;
	p_node->get_signal_list(&_signals);
	_signals.sort();

	//ERR_FAIL_COND_V( !node_map.has(p_node), ERR_BUG);
	//NodeData &nd = nodes[node_map[p_node]];

	for (List<MethodInfo>::Element *E = _signals.front(); E; E = E->next()) {
		List<Node::Connection> conns;
		p_node->get_signal_connection_list(E->get().name, &conns);

		conns.sort();

		for (List<Node::Connection>::Element *F = conns.front(); F; F = F->next()) {
			const Node::Connection &c = F->get();

			if (!(c.flags & CONNECT_PERSIST)) { //only persistent connections get saved
				continue;
			}

			// only connections that originate or end into main saved scene are saved
			// everything else is discarded

			Node *target = Object::cast_to<Node>(c.target);

			if (!target) {
				continue;
			}

			//find if this connection already exists
			Node *common_parent = target->find_common_parent_with(p_node);

			ERR_CONTINUE(!common_parent);

			if (common_parent != p_owner && common_parent->get_filename() == String()) {
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

					if (ps->has_connection(signal_from, c.signal, signal_to, c.method)) {
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
								if (state->is_connection(from_node, c.signal, to_node, c.method)) {
									exists2 = true;
									break;
								}
							}
						}

						nl = nullptr;
					} else {
						if (nl->get_filename() != String()) {
							//is an instance
							Ref<SceneState> state = nl->get_scene_instance_state();
							if (state.is_valid()) {
								int from_node = state->find_node_by_path(nl->get_path_to(p_node));
								int to_node = state->find_node_by_path(nl->get_path_to(target));

								if (from_node >= 0 && to_node >= 0) {
									//this one has state for this node, save
									if (state->is_connection(from_node, c.signal, to_node, c.method)) {
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
			cd.method = _nm_get_string(c.method, name_map);
			cd.signal = _nm_get_string(c.signal, name_map);
			cd.flags = c.flags;
			for (int i = 0; i < c.binds.size(); i++) {
				cd.binds.push_back(_vm_get_variant(c.binds[i], variant_map));
			}
			connections.push_back(cd);
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *c = p_node->get_child(i);
		Error err = _parse_connections(p_owner, c, name_map, variant_map, node_map, nodepath_map);
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

	Map<StringName, int> name_map;
	HashMap<Variant, int, VariantHasher, VariantComparator> variant_map;
	Map<Node *, int> node_map;
	Map<Node *, int> nodepath_map;

	// If using scene inheritance, pack the scene it inherits from.
	if (scene->get_scene_inherited_state().is_valid()) {
		String path = scene->get_scene_inherited_state()->get_path();
		Ref<PackedScene> instance = ResourceLoader::load(path);
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

	for (Map<StringName, int>::Element *E = name_map.front(); E; E = E->next()) {
		names.write[E->get()] = E->key();
	}

	variants.resize(variant_map.size());
	const Variant *K = nullptr;
	while ((K = variant_map.next(K))) {
		int idx = variant_map[*K];
		variants.write[idx] = *K;
	}

	node_paths.resize(nodepath_map.size());
	for (Map<Node *, int>::Element *E = nodepath_map.front(); E; E = E->next()) {
		node_paths.write[E->get()] = scene->get_path_to(E->key());
	}

	if (Engine::get_singleton()->is_editor_hint()) {
		// Build node path cache
		for (Map<Node *, int>::Element *E = node_map.front(); E; E = E->next()) {
			node_path_cache[scene->get_path_to(E->key())] = E->get();
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
	ERR_FAIL_COND_V_MSG(node_path_cache.size() == 0, -1, "This operation requires the node cache to have been built.");

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
		//the node in the instanced scene, as a property may be missing
		//from the local one
		int idx = get_base_scene_state()->find_node_by_path(p_node);
		if (idx != -1) {
			base_scene_node_remap[nid] = idx;
		}
	}

	return nid;
}

int SceneState::_find_base_scene_node_remap_key(int p_idx) const {
	for (Map<int, int>::Element *E = base_scene_node_remap.front(); E; E = E->next()) {
		if (E->value() == p_idx) {
			return E->key();
		}
	}
	return -1;
}

Variant SceneState::get_property_value(int p_node, const StringName &p_property, bool &found) const {
	found = false;

	ERR_FAIL_COND_V(p_node < 0, Variant());

	if (p_node < nodes.size()) {
		//find in built-in nodes
		int pc = nodes[p_node].properties.size();
		const StringName *namep = names.ptr();

		const NodeData::Property *p = nodes[p_node].properties.ptr();
		for (int i = 0; i < pc; i++) {
			if (p_property == namep[p[i].name]) {
				found = true;
				return variants[p[i].value];
			}
		}
	}

	//property not found, try on instance

	if (base_scene_node_remap.has(p_node)) {
		return get_base_scene_state()->get_property_value(base_scene_node_remap[p_node], p_property, found);
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
	const PoolVector<int> snodes = p_dictionary["nodes"];
	ERR_FAIL_COND(snodes.size() < node_count);

	const int conn_count = p_dictionary["conn_count"];
	const PoolVector<int> sconns = p_dictionary["conns"];
	ERR_FAIL_COND(sconns.size() < conn_count);

	PoolVector<String> snames = p_dictionary["names"];
	if (snames.size()) {
		int namecount = snames.size();
		names.resize(namecount);
		PoolVector<String>::Read r = snames.read();
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
		PoolVector<int>::Read r = snodes.read();
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
		PoolVector<int>::Read r = sconns.read();
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
	PoolVector<String> rnames;
	rnames.resize(names.size());

	if (names.size()) {
		PoolVector<String>::Write r = rnames.write();

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
	if (nodes[p_idx].type == TYPE_INSTANCED) {
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

	if (sub_path.empty()) {
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
	return names[nodes[p_idx].properties[p_prop].name];
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

Array SceneState::get_connection_binds(int p_idx) const {
	ERR_FAIL_INDEX_V(p_idx, connections.size(), Array());
	Array binds;
	for (int i = 0; i < connections[p_idx].binds.size(); i++) {
		binds.push_back(variants[connections[p_idx].binds[i]]);
	}
	return binds;
}

bool SceneState::has_connection(const NodePath &p_node_from, const StringName &p_signal, const NodePath &p_node_to, const StringName &p_method) {
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

		ss = ss->get_base_scene_state();
	} while (ss.is_valid());

	return false;
}

Vector<NodePath> SceneState::get_editable_instances() const {
	return editable_instances;
}
//add

int SceneState::add_name(const StringName &p_name) {
	names.push_back(p_name);
	return names.size() - 1;
}

int SceneState::find_name(const StringName &p_name) const {
	for (int i = 0; i < names.size(); i++) {
		if (names[i] == p_name) {
			return i;
		}
	}

	return -1;
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
void SceneState::add_node_property(int p_node, int p_name, int p_value) {
	ERR_FAIL_INDEX(p_node, nodes.size());
	ERR_FAIL_INDEX(p_name, names.size());
	ERR_FAIL_INDEX(p_value, variants.size());

	NodeData::Property prop;
	prop.name = p_name;
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
void SceneState::add_connection(int p_from, int p_to, int p_signal, int p_method, int p_flags, const Vector<int> &p_binds) {
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
	c.binds = p_binds;
	connections.push_back(c);
}
void SceneState::add_editable_instance(const NodePath &p_path) {
	editable_instances.push_back(p_path);
}

bool SceneState::remove_group_references(const StringName &p_name) {
	bool edited = false;
	for (int node_idx = 0; node_idx < get_node_count(); node_idx++) {
		for (int group_idx = 0; group_idx < nodes[node_idx].groups.size(); group_idx++) {
			if (names[nodes[node_idx].groups[group_idx]] == p_name) {
				nodes.write[node_idx].groups.remove(group_idx);
				edited = true;
				break;
			}
		}
	}
	return edited;
}

bool SceneState::rename_group_references(const StringName &p_old_name, const StringName &p_new_name) {
	bool edited = false;
	for (int node_idx = 0; node_idx < get_node_count(); node_idx++) {
		for (int group_idx = 0; group_idx < nodes[node_idx].groups.size(); group_idx++) {
			if (names[nodes[node_idx].groups[group_idx]] == p_old_name) {
				names.set(nodes[node_idx].groups[group_idx], p_new_name);
				edited = true;
				break;
			}
		}
	}
	return edited;
}

PoolVector<String> SceneState::_get_node_groups(int p_idx) const {
	Vector<StringName> groups = get_node_groups(p_idx);
	PoolVector<String> ret;

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

	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_DISABLED);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_INSTANCE);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_MAIN);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_MAIN_INHERITED);
}

SceneState::SceneState() {
	base_scene_idx = -1;
	last_modified_time = 0;
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

bool PackedScene::can_instance() const {
	return state->can_instance();
}

Node *PackedScene::instance(GenEditState p_edit_state) const {
#ifndef TOOLS_ENABLED
	ERR_FAIL_COND_V_MSG(p_edit_state != GEN_EDIT_STATE_DISABLED, NULL, "Edit state is only for editors, does not work without tools compiled.");
#endif

	Node *s = state->instance((SceneState::GenEditState)p_edit_state);
	if (!s) {
		return nullptr;
	}

	if (p_edit_state != GEN_EDIT_STATE_DISABLED) {
		s->set_scene_instance_state(state);
	}

	if (get_path() != "" && get_path().find("::") == -1) {
		s->set_filename(get_path());
	}

	s->notification(Node::NOTIFICATION_INSTANCED);

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
	state = Ref<SceneState>(memnew(SceneState));
	state->set_path(get_path());
#ifdef TOOLS_ENABLED
	state->set_last_modified_time(get_last_modified_time());
#endif
}

Ref<SceneState> PackedScene::get_state() {
	return state;
}

void PackedScene::set_path(const String &p_path, bool p_take_over) {
	state->set_path(p_path);
	Resource::set_path(p_path, p_take_over);
}

void PackedScene::_bind_methods() {
	ClassDB::bind_method(D_METHOD("pack", "path"), &PackedScene::pack);
	ClassDB::bind_method(D_METHOD("instance", "edit_state"), &PackedScene::instance, DEFVAL(GEN_EDIT_STATE_DISABLED));
	ClassDB::bind_method(D_METHOD("can_instance"), &PackedScene::can_instance);
	ClassDB::bind_method(D_METHOD("_set_bundled_scene"), &PackedScene::_set_bundled_scene);
	ClassDB::bind_method(D_METHOD("_get_bundled_scene"), &PackedScene::_get_bundled_scene);
	ClassDB::bind_method(D_METHOD("get_state"), &PackedScene::get_state);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_bundled"), "_set_bundled_scene", "_get_bundled_scene");

	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_DISABLED);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_INSTANCE);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_MAIN);
	BIND_ENUM_CONSTANT(GEN_EDIT_STATE_MAIN_INHERITED);
}

PackedScene::PackedScene() {
	state = Ref<SceneState>(memnew(SceneState));
}
