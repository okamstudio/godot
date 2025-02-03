/**************************************************************************/
/*  instance_placeholder.cpp                                              */
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

#include "instance_placeholder.h"

#include "core/io/resource_loader.h"
#include "scene/resources/packed_scene.h"

bool InstancePlaceholder::_set(const StringName &p_name, const Variant &p_value) {
	PropSet ps;
	ps.name = p_name;
	ps.value = p_value;
	stored_values.push_back(ps);
	return true;
}

bool InstancePlaceholder::_get(const StringName &p_name, Variant &r_ret) const {
	for (const List<PropSet>::Element *E = stored_values.front(); E; E = E->next()) {
		if (E->get().name == p_name) {
			r_ret = E->get().value;
			return true;
		}
	}
	return false;
}

void InstancePlaceholder::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const List<PropSet>::Element *E = stored_values.front(); E; E = E->next()) {
		PropertyInfo pi;
		pi.name = E->get().name;
		pi.type = E->get().value.get_type();
		pi.usage = PROPERTY_USAGE_STORAGE;

		p_list->push_back(pi);
	}
}

void InstancePlaceholder::set_instance_path(const String &p_name) {
	path = p_name;
}

String InstancePlaceholder::get_instance_path() const {
	return path;
}

Node *InstancePlaceholder::create_instance(bool p_replace, const Ref<PackedScene> &p_custom_scene) {
	ERR_FAIL_COND_V(!is_inside_tree(), nullptr);

	Node *base = get_parent();
	if (!base) {
		return nullptr;
	}

	Ref<PackedScene> ps;
	if (p_custom_scene.is_valid()) {
		ps = p_custom_scene;
	} else {
		ps = ResourceLoader::load(path, "PackedScene");
	}

	if (!ps.is_valid()) {
		return nullptr;
	}
	Node *scene = ps->instance();
	if (!scene) {
		return nullptr;
	}
	scene->set_name(get_name());

	for (List<PropSet>::Element *E = stored_values.front(); E; E = E->next()) {
		scene->set(E->get().name, E->get().value);
	}

	if (p_replace) {
		queue_delete();
		base->remove_child(this);
	}

	return scene;
}

void InstancePlaceholder::replace_by_instance(const Ref<PackedScene> &p_custom_scene) {
	//Deprecated by
	create_instance(true, p_custom_scene);
}

Dictionary InstancePlaceholder::get_stored_values(bool p_with_order) {
	Dictionary ret;
	PoolStringArray order;

	for (List<PropSet>::Element *E = stored_values.front(); E; E = E->next()) {
		ret[E->get().name] = E->get().value;
		if (p_with_order) {
			order.push_back(E->get().name);
		}
	};

	if (p_with_order) {
		ret[".order"] = order;
	}

	return ret;
};

void InstancePlaceholder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_stored_values", "with_order"), &InstancePlaceholder::get_stored_values, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("create_instance", "replace", "custom_scene"), &InstancePlaceholder::create_instance, DEFVAL(false), DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("replace_by_instance", "custom_scene"), &InstancePlaceholder::replace_by_instance, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("get_instance_path"), &InstancePlaceholder::get_instance_path);
}

InstancePlaceholder::InstancePlaceholder() {
}
