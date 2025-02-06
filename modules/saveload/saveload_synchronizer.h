/**************************************************************************/
/*  saveload_synchronizer.h                                               */
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

#ifndef SAVELOAD_SYNCHRONIZER_H
#define SAVELOAD_SYNCHRONIZER_H

#include "scene/main/node.h"

#include "scene_saveload_config.h"

class SaveloadSynchronizer : public Node {
	GDCLASS(SaveloadSynchronizer, Node);

public:
	struct SyncherState {
		HashMap<const NodePath, Variant> property_map;

		Dictionary to_dict() const;

		SyncherState(HashMap<const NodePath, Variant> p_property_map) { property_map = p_property_map; }
		SyncherState(const Dictionary &p_dict);
		SyncherState() {}
	};

private:
	Ref<SceneSaveloadConfig> saveload_config;
	NodePath root_path = NodePath(".."); // Start with parent, like with AnimationPlayer.

	ObjectID root_node_cache;

	void _start();
	void _stop();
	void _update_process();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	SyncherState get_syncher_state() const;
	Error set_syncher_state(const SyncherState &p_syncher_state);

	PackedStringArray get_configuration_warnings() const override;

	void set_saveload_config(Ref<SceneSaveloadConfig> p_config);
	Ref<SceneSaveloadConfig> get_saveload_config() const;

	Node *get_root_node() const;
	void set_root_path(const NodePath &p_path);
	NodePath get_root_path() const;

	SaveloadSynchronizer() {}
};

#endif // SAVELOAD_SYNCHRONIZER_H
