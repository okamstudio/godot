/**************************************************************************/
/*  resource_preloader.cpp                                                */
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

#include "resource_preloader.h"
#include "core/templates/rb_set.h"
void ResourcePreloader::_set_resources(const Array &p_data) {
	resources.clear();

	ERR_FAIL_COND(p_data.size() != 2);
	Vector<String> names = p_data[0];
	Array resdata = p_data[1];

	ERR_FAIL_COND(names.size() != resdata.size());

	for (int i = 0; i < resdata.size(); i++) {
		Ref<Resource> resource = resdata[i];
		ERR_CONTINUE(resource.is_null());
		resources[names[i]] = resource;

		//add_resource(names[i],resource);
	}
}

Array ResourcePreloader::_get_resources() const {
	Vector<String> names;
	Array arr;
	arr.resize(resources.size());
	names.resize(resources.size());

	RBSet<String> sorted_names;

	for (const KeyValue<StringName, Ref<Resource>> &E : resources) {
		sorted_names.insert(E.key);
	}

	int i = 0;
	for (const String &E : sorted_names) {
		names.set(i, E);
		arr[i] = resources[E];
		i++;
	}

	Array res;
	res.push_back(names);
	res.push_back(arr);
	return res;
}

void ResourcePreloader::add_resource(const StringName &p_name, const Ref<Resource> &p_resource) {
	ERR_FAIL_COND(p_resource.is_null());
	if (resources.has(p_name)) {
		StringName new_name;
		int idx = 2;

		while (true) {
			new_name = p_name.operator String() + " " + itos(idx);
			if (resources.has(new_name)) {
				idx++;
				continue;
			}

			break;
		}

		add_resource(new_name, p_resource);
	} else {
		resources[p_name] = p_resource;
	}
}

void ResourcePreloader::remove_resource(const StringName &p_name) {
	ERR_FAIL_COND(!resources.has(p_name));
	resources.erase(p_name);
}

void ResourcePreloader::rename_resource(const StringName &p_from_name, const StringName &p_to_name) {
	ERR_FAIL_COND(!resources.has(p_from_name));

	Ref<Resource> res = resources[p_from_name];

	resources.erase(p_from_name);
	add_resource(p_to_name, res);
}

bool ResourcePreloader::has_resource(const StringName &p_name) const {
	return resources.has(p_name);
}

Ref<Resource> ResourcePreloader::get_resource(const StringName &p_name) const {
	ERR_FAIL_COND_V(!resources.has(p_name), Ref<Resource>());
	return resources[p_name];
}

Vector<String> ResourcePreloader::_get_resource_list() const {
	Vector<String> res;
	res.resize(resources.size());
	int i = 0;
	for (const KeyValue<StringName, Ref<Resource>> &E : resources) {
		res.set(i, E.key);
		i++;
	}

	return res;
}

void ResourcePreloader::get_resource_list(List<StringName> *p_list) {
	for (const KeyValue<StringName, Ref<Resource>> &E : resources) {
		p_list->push_back(E.key);
	}
}

void ResourcePreloader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_set_resources", "resources"), &ResourcePreloader::_set_resources);
	ClassDB::bind_method(D_METHOD("_get_resources"), &ResourcePreloader::_get_resources);

	ClassDB::bind_method(D_METHOD("add_resource", "name", "resource"), &ResourcePreloader::add_resource);
	ClassDB::bind_method(D_METHOD("remove_resource", "name"), &ResourcePreloader::remove_resource);
	ClassDB::bind_method(D_METHOD("rename_resource", "name", "newname"), &ResourcePreloader::rename_resource);
	ClassDB::bind_method(D_METHOD("has_resource", "name"), &ResourcePreloader::has_resource);
	ClassDB::bind_method(D_METHOD("get_resource", "name"), &ResourcePreloader::get_resource);
	ClassDB::bind_method(D_METHOD("get_resource_list"), &ResourcePreloader::_get_resource_list);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "resources", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_resources", "_get_resources");
}

ResourcePreloader::ResourcePreloader() {
}
