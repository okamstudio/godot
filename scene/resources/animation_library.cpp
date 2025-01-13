/**************************************************************************/
/*  animation_library.cpp                                                 */
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

#include "animation_library.h"

#include "scene/scene_string_names.h"

bool AnimationLibrary::is_valid_animation_name(const String &p_name) {
	return !(p_name.is_empty() || p_name.contains_char('/') || p_name.contains_char(':') || p_name.contains_char(',') || p_name.contains_char('['));
}

bool AnimationLibrary::is_valid_library_name(const String &p_name) {
	return !(p_name.contains_char('/') || p_name.contains_char(':') || p_name.contains_char(',') || p_name.contains_char('['));
}

String AnimationLibrary::validate_library_name(const String &p_name) {
	String name = p_name;
	const char *characters = "/:,[";
	for (const char *p = characters; *p; p++) {
		name = name.replace(String::chr(*p), "_");
	}
	return name;
}

Error AnimationLibrary::add_animation(const StringName &p_name, const Ref<Animation> &p_animation) {
	ERR_FAIL_COND_V_MSG(!is_valid_animation_name(p_name), ERR_INVALID_PARAMETER, "Invalid animation name: '" + String(p_name) + "'.");
	ERR_FAIL_COND_V(p_animation.is_null(), ERR_INVALID_PARAMETER);

	if (animations.has(p_name)) {
		animations.get(p_name)->disconnect_changed(callable_mp(this, &AnimationLibrary::_animation_changed));
		animations.erase(p_name);
		emit_signal(SNAME("animation_removed"), p_name);
	}

	animations.insert(p_name, p_animation);
	animations.get(p_name)->connect_changed(callable_mp(this, &AnimationLibrary::_animation_changed).bind(p_name));
	emit_signal(SNAME("animation_added"), p_name);
	notify_property_list_changed();
	return OK;
}

void AnimationLibrary::remove_animation(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!animations.has(p_name), vformat("Animation not found: %s.", p_name));

	animations.get(p_name)->disconnect_changed(callable_mp(this, &AnimationLibrary::_animation_changed));
	animations.erase(p_name);
	emit_signal(SNAME("animation_removed"), p_name);
	notify_property_list_changed();
}

void AnimationLibrary::rename_animation(const StringName &p_name, const StringName &p_new_name) {
	ERR_FAIL_COND_MSG(!animations.has(p_name), vformat("Animation not found: %s.", p_name));
	ERR_FAIL_COND_MSG(!is_valid_animation_name(p_new_name), "Invalid animation name: '" + String(p_new_name) + "'.");
	ERR_FAIL_COND_MSG(animations.has(p_new_name), vformat("Animation name \"%s\" already exists in library.", p_new_name));

	animations.get(p_name)->disconnect_changed(callable_mp(this, &AnimationLibrary::_animation_changed));
	animations.get(p_name)->connect_changed(callable_mp(this, &AnimationLibrary::_animation_changed).bind(p_new_name));
	animations.insert(p_new_name, animations[p_name]);
	animations.erase(p_name);
	emit_signal(SNAME("animation_renamed"), p_name, p_new_name);
}

bool AnimationLibrary::has_animation(const StringName &p_name) const {
	return animations.has(p_name);
}

Ref<Animation> AnimationLibrary::get_animation(const StringName &p_name) const {
	ERR_FAIL_COND_V_MSG(!animations.has(p_name), Ref<Animation>(), vformat("Animation not found: \"%s\".", p_name));

	return animations[p_name];
}

TypedArray<StringName> AnimationLibrary::_get_animation_list() const {
	TypedArray<StringName> ret;
	List<StringName> names;
	get_animation_list(&names);
	for (const StringName &K : names) {
		ret.push_back(K);
	}
	return ret;
}

void AnimationLibrary::_animation_changed(const StringName &p_name) {
	emit_signal(SceneStringName(animation_changed), p_name);
}

void AnimationLibrary::get_animation_list(List<StringName> *p_animations) const {
	List<StringName> anims;

	for (const KeyValue<StringName, Ref<Animation>> &E : animations) {
		anims.push_back(E.key);
	}

	anims.sort_custom<StringName::AlphCompare>();

	for (const StringName &E : anims) {
		p_animations->push_back(E);
	}
}

int AnimationLibrary::get_animation_list_size() const {
	return animations.size();
}

void AnimationLibrary::_set_data(const Dictionary &p_data) {
	for (KeyValue<StringName, Ref<Animation>> &K : animations) {
		K.value->disconnect_changed(callable_mp(this, &AnimationLibrary::_animation_changed));
	}
	animations.clear();
	List<Variant> keys;
	p_data.get_key_list(&keys);
	for (const Variant &K : keys) {
		add_animation(K, p_data[K]);
	}
}

Dictionary AnimationLibrary::_get_data() const {
	Dictionary ret;
	for (const KeyValue<StringName, Ref<Animation>> &K : animations) {
		ret[K.key] = K.value;
	}
	return ret;
}

#ifdef TOOLS_ENABLED
void AnimationLibrary::get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const {
	const String pf = p_function;
	if (p_idx == 0 && (pf == "get_animation" || pf == "has_animation" || pf == "rename_animation" || pf == "remove_animation")) {
		List<StringName> names;
		get_animation_list(&names);
		for (const StringName &E : names) {
			r_options->push_back(E.operator String().quote());
		}
	}
	Resource::get_argument_options(p_function, p_idx, r_options);
}
#endif

void AnimationLibrary::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_animation", "name", "animation"), &AnimationLibrary::add_animation);
	ClassDB::bind_method(D_METHOD("remove_animation", "name"), &AnimationLibrary::remove_animation);
	ClassDB::bind_method(D_METHOD("rename_animation", "name", "newname"), &AnimationLibrary::rename_animation);
	ClassDB::bind_method(D_METHOD("has_animation", "name"), &AnimationLibrary::has_animation);
	ClassDB::bind_method(D_METHOD("get_animation", "name"), &AnimationLibrary::get_animation);
	ClassDB::bind_method(D_METHOD("get_animation_list"), &AnimationLibrary::_get_animation_list);
	ClassDB::bind_method(D_METHOD("get_animation_list_size"), &AnimationLibrary::get_animation_list_size);

	ClassDB::bind_method(D_METHOD("_set_data", "data"), &AnimationLibrary::_set_data);
	ClassDB::bind_method(D_METHOD("_get_data"), &AnimationLibrary::_get_data);

	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "_data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "_set_data", "_get_data");

	ADD_SIGNAL(MethodInfo("animation_added", PropertyInfo(Variant::STRING_NAME, "name")));
	ADD_SIGNAL(MethodInfo("animation_removed", PropertyInfo(Variant::STRING_NAME, "name")));
	ADD_SIGNAL(MethodInfo("animation_renamed", PropertyInfo(Variant::STRING_NAME, "name"), PropertyInfo(Variant::STRING_NAME, "to_name")));
	ADD_SIGNAL(MethodInfo("animation_changed", PropertyInfo(Variant::STRING_NAME, "name")));
}
AnimationLibrary::AnimationLibrary() {
}
