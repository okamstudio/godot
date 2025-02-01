/**************************************************************************/
/*  gltf_mesh.h                                                           */
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

#include "../gltf_defines.h"

#include "scene/resources/3d/importer_mesh.h"

class GLTFMesh : public Resource {
	GDCLASS(GLTFMesh, Resource);

private:
	String original_name;
	Ref<ImporterMesh> mesh;
	Vector<float> blend_weights;
	TypedArray<Material> instance_materials;
	Dictionary additional_data;

protected:
	static void _bind_methods();

public:
	String get_original_name();
	void set_original_name(String p_name);
	Ref<ImporterMesh> get_mesh();
	void set_mesh(Ref<ImporterMesh> p_mesh);
	Vector<float> get_blend_weights();
	void set_blend_weights(Vector<float> p_blend_weights);
	TypedArray<Material> get_instance_materials();
	void set_instance_materials(TypedArray<Material> p_instance_materials);
	Variant get_additional_data(const StringName &p_extension_name);
	void set_additional_data(const StringName &p_extension_name, Variant p_additional_data);
};
