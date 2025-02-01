/**************************************************************************/
/*  scene_replication_config.h                                            */
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

#pragma once

#include "core/io/resource.h"
#include "core/variant/typed_array.h"

class SceneReplicationConfig : public Resource {
	GDCLASS(SceneReplicationConfig, Resource);
	OBJ_SAVE_TYPE(SceneReplicationConfig);
	RES_BASE_EXTENSION("repl");

public:
	enum ReplicationMode {
		REPLICATION_MODE_NEVER,
		REPLICATION_MODE_ALWAYS,
		REPLICATION_MODE_ON_CHANGE,
	};

private:
	struct ReplicationProperty {
		NodePath name;
		bool spawn = true;
		ReplicationMode mode = REPLICATION_MODE_ALWAYS;

		bool operator==(const ReplicationProperty &p_to) {
			return name == p_to.name;
		}

		ReplicationProperty() {}

		ReplicationProperty(const NodePath &p_name) {
			name = p_name;
		}
	};

	List<ReplicationProperty> properties;
	List<NodePath> spawn_props;
	List<NodePath> sync_props;
	List<NodePath> watch_props;
	bool dirty = false;

	void _update();

protected:
	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	virtual void reset_state() override; // Required since we use variable amount of properties.

	TypedArray<NodePath> get_properties() const;

	void add_property(const NodePath &p_path, int p_index = -1);
	void remove_property(const NodePath &p_path);
	bool has_property(const NodePath &p_path) const;

	int property_get_index(const NodePath &p_path) const;
	bool property_get_spawn(const NodePath &p_path);
	void property_set_spawn(const NodePath &p_path, bool p_enabled);

	bool property_get_sync(const NodePath &p_path);
	void property_set_sync(const NodePath &p_path, bool p_enabled);

	bool property_get_watch(const NodePath &p_path);
	void property_set_watch(const NodePath &p_path, bool p_enabled);

	ReplicationMode property_get_replication_mode(const NodePath &p_path);
	void property_set_replication_mode(const NodePath &p_path, ReplicationMode p_mode);

	const List<NodePath> &get_spawn_properties();
	const List<NodePath> &get_sync_properties();
	const List<NodePath> &get_watch_properties();

	SceneReplicationConfig() {}
};

VARIANT_ENUM_CAST(SceneReplicationConfig::ReplicationMode);
