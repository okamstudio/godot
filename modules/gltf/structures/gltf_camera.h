/**************************************************************************/
/*  gltf_camera.h                                                         */
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

class Camera3D;
class GLTFObjectModelProperty;

// Reference and test file:
// https://github.com/KhronosGroup/glTF-Tutorials/blob/master/gltfTutorial/gltfTutorial_015_SimpleCameras.md

class GLTFCamera : public Resource {
	GDCLASS(GLTFCamera, Resource);

private:
	// glTF has no default camera values, they should always be specified in
	// the glTF file. Here we default to Godot's default camera settings.
	bool perspective = true;
	real_t fov = Math::deg_to_rad(75.0);
	real_t size_mag = 0.5;
	real_t depth_far = 4000.0;
	real_t depth_near = 0.05;

protected:
	static void _bind_methods();

public:
	static void set_fov_conversion_expressions(Ref<GLTFObjectModelProperty> &r_obj_model_prop);

	bool get_perspective() const { return perspective; }
	void set_perspective(bool p_val) { perspective = p_val; }
	real_t get_fov() const { return fov; }
	void set_fov(real_t p_val) { fov = p_val; }
	real_t get_size_mag() const { return size_mag; }
	void set_size_mag(real_t p_val) { size_mag = p_val; }
	real_t get_depth_far() const { return depth_far; }
	void set_depth_far(real_t p_val) { depth_far = p_val; }
	real_t get_depth_near() const { return depth_near; }
	void set_depth_near(real_t p_val) { depth_near = p_val; }

	static Ref<GLTFCamera> from_node(const Camera3D *p_camera);
	Camera3D *to_node() const;

	static Ref<GLTFCamera> from_dictionary(const Dictionary p_dictionary);
	virtual Dictionary to_dictionary() const;
};
