/**************************************************************************/
/*  gltf_skin.cpp                                                         */
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

#include "gltf_skin.h"

#include "../gltf_template_convert.h"

#include "core/variant/typed_array.h"
#include "scene/resources/3d/skin.h"

void GLTFSkin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_skin_root"), &GLTFSkin::get_skin_root);
	ClassDB::bind_method(D_METHOD("set_skin_root", "skin_root"), &GLTFSkin::set_skin_root);
	ClassDB::bind_method(D_METHOD("get_joints_original"), &GLTFSkin::get_joints_original);
	ClassDB::bind_method(D_METHOD("set_joints_original", "joints_original"), &GLTFSkin::set_joints_original);
	ClassDB::bind_method(D_METHOD("get_inverse_binds"), &GLTFSkin::get_inverse_binds);
	ClassDB::bind_method(D_METHOD("set_inverse_binds", "inverse_binds"), &GLTFSkin::set_inverse_binds);
	ClassDB::bind_method(D_METHOD("get_joints"), &GLTFSkin::get_joints);
	ClassDB::bind_method(D_METHOD("set_joints", "joints"), &GLTFSkin::set_joints);
	ClassDB::bind_method(D_METHOD("get_non_joints"), &GLTFSkin::get_non_joints);
	ClassDB::bind_method(D_METHOD("set_non_joints", "non_joints"), &GLTFSkin::set_non_joints);
	ClassDB::bind_method(D_METHOD("get_roots"), &GLTFSkin::get_roots);
	ClassDB::bind_method(D_METHOD("set_roots", "roots"), &GLTFSkin::set_roots);
	ClassDB::bind_method(D_METHOD("get_skeleton"), &GLTFSkin::get_skeleton);
	ClassDB::bind_method(D_METHOD("set_skeleton", "skeleton"), &GLTFSkin::set_skeleton);
	ClassDB::bind_method(D_METHOD("get_joint_i_to_bone_i"), &GLTFSkin::get_joint_i_to_bone_i);
	ClassDB::bind_method(D_METHOD("set_joint_i_to_bone_i", "joint_i_to_bone_i"), &GLTFSkin::set_joint_i_to_bone_i);
	ClassDB::bind_method(D_METHOD("get_joint_i_to_name"), &GLTFSkin::get_joint_i_to_name);
	ClassDB::bind_method(D_METHOD("set_joint_i_to_name", "joint_i_to_name"), &GLTFSkin::set_joint_i_to_name);
	ClassDB::bind_method(D_METHOD("get_godot_skin"), &GLTFSkin::get_godot_skin);
	ClassDB::bind_method(D_METHOD("set_godot_skin", "godot_skin"), &GLTFSkin::set_godot_skin);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "skin_root"), "set_skin_root", "get_skin_root"); // GLTFNodeIndex
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "joints_original"), "set_joints_original", "get_joints_original"); // Vector<GLTFNodeIndex>
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "inverse_binds", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL), "set_inverse_binds", "get_inverse_binds"); // Vector<Transform3D>
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "joints"), "set_joints", "get_joints"); // Vector<GLTFNodeIndex>
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "non_joints"), "set_non_joints", "get_non_joints"); // Vector<GLTFNodeIndex>
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "roots"), "set_roots", "get_roots"); // Vector<GLTFNodeIndex>
	ADD_PROPERTY(PropertyInfo(Variant::INT, "skeleton"), "set_skeleton", "get_skeleton"); // int
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "joint_i_to_bone_i", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL), "set_joint_i_to_bone_i", "get_joint_i_to_bone_i"); // RBMap<int,
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "joint_i_to_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_INTERNAL), "set_joint_i_to_name", "get_joint_i_to_name"); // RBMap<int,
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "godot_skin", PROPERTY_HINT_RESOURCE_TYPE, "Skin"), "set_godot_skin", "get_godot_skin"); // Ref<Skin>
}

GLTFNodeIndex GLTFSkin::get_skin_root() {
	return skin_root;
}

void GLTFSkin::set_skin_root(GLTFNodeIndex p_skin_root) {
	skin_root = p_skin_root;
}

Vector<GLTFNodeIndex> GLTFSkin::get_joints_original() {
	return joints_original;
}

void GLTFSkin::set_joints_original(Vector<GLTFNodeIndex> p_joints_original) {
	joints_original = p_joints_original;
}

TypedArray<Transform3D> GLTFSkin::get_inverse_binds() {
	return GLTFTemplateConvert::to_array(inverse_binds);
}

void GLTFSkin::set_inverse_binds(TypedArray<Transform3D> p_inverse_binds) {
	GLTFTemplateConvert::set_from_array(inverse_binds, p_inverse_binds);
}

Vector<GLTFNodeIndex> GLTFSkin::get_joints() {
	return joints;
}

void GLTFSkin::set_joints(Vector<GLTFNodeIndex> p_joints) {
	joints = p_joints;
}

Vector<GLTFNodeIndex> GLTFSkin::get_non_joints() {
	return non_joints;
}

void GLTFSkin::set_non_joints(Vector<GLTFNodeIndex> p_non_joints) {
	non_joints = p_non_joints;
}

Vector<GLTFNodeIndex> GLTFSkin::get_roots() {
	return roots;
}

void GLTFSkin::set_roots(Vector<GLTFNodeIndex> p_roots) {
	roots = p_roots;
}

int GLTFSkin::get_skeleton() {
	return skeleton;
}

void GLTFSkin::set_skeleton(int p_skeleton) {
	skeleton = p_skeleton;
}

Dictionary GLTFSkin::get_joint_i_to_bone_i() {
	return GLTFTemplateConvert::to_dictionary(joint_i_to_bone_i);
}

void GLTFSkin::set_joint_i_to_bone_i(Dictionary p_joint_i_to_bone_i) {
	GLTFTemplateConvert::set_from_dictionary(joint_i_to_bone_i, p_joint_i_to_bone_i);
}

Dictionary GLTFSkin::get_joint_i_to_name() {
	Dictionary ret;
	HashMap<int, StringName>::Iterator elem = joint_i_to_name.begin();
	while (elem) {
		ret[elem->key] = String(elem->value);
		++elem;
	}
	return ret;
}

void GLTFSkin::set_joint_i_to_name(Dictionary p_joint_i_to_name) {
	joint_i_to_name = HashMap<int, StringName>();
	List<Variant> keys;
	p_joint_i_to_name.get_key_list(&keys);
	for (const Variant &key : keys) {
		joint_i_to_name[key] = p_joint_i_to_name[key];
	}
}

Ref<Skin> GLTFSkin::get_godot_skin() {
	return godot_skin;
}

void GLTFSkin::set_godot_skin(Ref<Skin> p_godot_skin) {
	godot_skin = p_godot_skin;
}

Error GLTFSkin::from_dictionary(const Dictionary &dict) {
	ERR_FAIL_COND_V(!dict.has("skin_root"), ERR_INVALID_DATA);
	skin_root = dict["skin_root"];

	ERR_FAIL_COND_V(!dict.has("joints_original"), ERR_INVALID_DATA);
	Array joints_original_array = dict["joints_original"];
	joints_original.clear();
	for (int i = 0; i < joints_original_array.size(); ++i) {
		joints_original.push_back(joints_original_array[i]);
	}

	ERR_FAIL_COND_V(!dict.has("inverse_binds"), ERR_INVALID_DATA);
	Array inverse_binds_array = dict["inverse_binds"];
	inverse_binds.clear();
	for (int i = 0; i < inverse_binds_array.size(); ++i) {
		ERR_FAIL_COND_V(inverse_binds_array[i].get_type() != Variant::TRANSFORM3D, ERR_INVALID_DATA);
		inverse_binds.push_back(inverse_binds_array[i]);
	}

	ERR_FAIL_COND_V(!dict.has("joints"), ERR_INVALID_DATA);
	Array joints_array = dict["joints"];
	joints.clear();
	for (int i = 0; i < joints_array.size(); ++i) {
		joints.push_back(joints_array[i]);
	}

	ERR_FAIL_COND_V(!dict.has("non_joints"), ERR_INVALID_DATA);
	Array non_joints_array = dict["non_joints"];
	non_joints.clear();
	for (int i = 0; i < non_joints_array.size(); ++i) {
		non_joints.push_back(non_joints_array[i]);
	}

	ERR_FAIL_COND_V(!dict.has("roots"), ERR_INVALID_DATA);
	Array roots_array = dict["roots"];
	roots.clear();
	for (int i = 0; i < roots_array.size(); ++i) {
		roots.push_back(roots_array[i]);
	}

	ERR_FAIL_COND_V(!dict.has("skeleton"), ERR_INVALID_DATA);
	skeleton = dict["skeleton"];

	ERR_FAIL_COND_V(!dict.has("joint_i_to_bone_i"), ERR_INVALID_DATA);
	Dictionary joint_i_to_bone_i_dict = dict["joint_i_to_bone_i"];
	joint_i_to_bone_i.clear();
	{
		List<Variant> keys;
		joint_i_to_bone_i_dict.get_key_list(&keys);
		for (int key : keys) {
			int value = joint_i_to_bone_i_dict[key];
			joint_i_to_bone_i[key] = value;
		}
	}

	ERR_FAIL_COND_V(!dict.has("joint_i_to_name"), ERR_INVALID_DATA);
	Dictionary joint_i_to_name_dict = dict["joint_i_to_name"];
	joint_i_to_name.clear();
	{
		List<Variant> keys;
		joint_i_to_name_dict.get_key_list(&keys);
		for (int key : keys) {
			StringName value = joint_i_to_name_dict[key];
			joint_i_to_name[key] = value;
		}
	}
	if (dict.has("godot_skin")) {
		godot_skin = dict["godot_skin"];
	}
	return OK;
}

Dictionary GLTFSkin::to_dictionary() {
	Dictionary dict;
	dict["skin_root"] = skin_root;

	Array joints_original_array;
	for (int i = 0; i < joints_original.size(); ++i) {
		joints_original_array.push_back(joints_original[i]);
	}
	dict["joints_original"] = joints_original_array;

	Array inverse_binds_array;
	for (int i = 0; i < inverse_binds.size(); ++i) {
		inverse_binds_array.push_back(inverse_binds[i]);
	}
	dict["inverse_binds"] = inverse_binds_array;

	Array joints_array;
	for (int i = 0; i < joints.size(); ++i) {
		joints_array.push_back(joints[i]);
	}
	dict["joints"] = joints_array;

	Array non_joints_array;
	for (int i = 0; i < non_joints.size(); ++i) {
		non_joints_array.push_back(non_joints[i]);
	}
	dict["non_joints"] = non_joints_array;

	Array roots_array;
	for (int i = 0; i < roots.size(); ++i) {
		roots_array.push_back(roots[i]);
	}
	dict["roots"] = roots_array;

	dict["skeleton"] = skeleton;

	Dictionary joint_i_to_bone_i_dict;
	for (HashMap<int, int>::Iterator E = joint_i_to_bone_i.begin(); E; ++E) {
		joint_i_to_bone_i_dict[E->key] = E->value;
	}
	dict["joint_i_to_bone_i"] = joint_i_to_bone_i_dict;

	Dictionary joint_i_to_name_dict;
	for (HashMap<int, StringName>::Iterator E = joint_i_to_name.begin(); E; ++E) {
		joint_i_to_name_dict[E->key] = E->value;
	}
	dict["joint_i_to_name"] = joint_i_to_name_dict;

	dict["godot_skin"] = godot_skin;
	return dict;
}
