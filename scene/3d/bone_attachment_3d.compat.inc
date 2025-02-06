/**************************************************************************/
/*  bone_attachment_3d.compat.inc                                         */
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

#ifndef DISABLE_DEPRECATED

#include "bone_attachment_3d.h"

void BoneAttachment3D::_on_bone_pose_update_bind_compat_90575(int p_bone_index) {
	return on_skeleton_update();
}

void BoneAttachment3D::_bind_compatibility_methods() {
	ClassDB::bind_compatibility_method(D_METHOD("on_bone_pose_update", "bone_index"), &BoneAttachment3D::_on_bone_pose_update_bind_compat_90575);

	ClassDB::bind_method(D_METHOD("set_external_skeleton", "external_skeleton"), &BoneAttachment3D::set_external_skeleton_path);
	ClassDB::bind_method(D_METHOD("get_external_skeleton"), &BoneAttachment3D::get_external_skeleton_path);
}

#endif // DISABLE_DEPRECATED
