/**************************************************************************/
/*  mesh_library.h                                                        */
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
#include "core/templates/rb_map.h"
#include "scene/resources/mesh.h"
#include "scene/resources/navigation_mesh.h"
#include "servers/rendering_server.h"
#include "shape_3d.h"

class MeshLibrary : public Resource {
	GDCLASS(MeshLibrary, Resource);
	RES_BASE_EXTENSION("meshlib");

public:
	struct ShapeData {
		Ref<Shape3D> shape;
		Transform3D local_transform;
	};
	struct Item {
		String name;
		Ref<Mesh> mesh;
		Transform3D mesh_transform;
		RS::ShadowCastingSetting mesh_cast_shadow = RS::ShadowCastingSetting::SHADOW_CASTING_SETTING_ON;
		Vector<ShapeData> shapes;
		Ref<Texture2D> preview;
		Ref<NavigationMesh> navigation_mesh;
		Transform3D navigation_mesh_transform;
		uint32_t navigation_layers = 1;
	};

	RBMap<int, Item> item_map;

	void _set_item_shapes(int p_item, const Array &p_shapes);
	Array _get_item_shapes(int p_item) const;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	virtual void reset_state() override;
	static void _bind_methods();

public:
	void create_item(int p_item);
	void set_item_name(int p_item, const String &p_name);
	void set_item_mesh(int p_item, const Ref<Mesh> &p_mesh);
	void set_item_mesh_transform(int p_item, const Transform3D &p_transform);
	void set_item_mesh_cast_shadow(int p_item, RS::ShadowCastingSetting p_shadow_casting_setting);
	void set_item_navigation_mesh(int p_item, const Ref<NavigationMesh> &p_navigation_mesh);
	void set_item_navigation_mesh_transform(int p_item, const Transform3D &p_transform);
	void set_item_navigation_layers(int p_item, uint32_t p_navigation_layers);
	void set_item_shapes(int p_item, const Vector<ShapeData> &p_shapes);
	void set_item_preview(int p_item, const Ref<Texture2D> &p_preview);
	String get_item_name(int p_item) const;
	Ref<Mesh> get_item_mesh(int p_item) const;
	Transform3D get_item_mesh_transform(int p_item) const;
	RS::ShadowCastingSetting get_item_mesh_cast_shadow(int p_item) const;
	Ref<NavigationMesh> get_item_navigation_mesh(int p_item) const;
	Transform3D get_item_navigation_mesh_transform(int p_item) const;
	uint32_t get_item_navigation_layers(int p_item) const;
	Vector<ShapeData> get_item_shapes(int p_item) const;
	Ref<Texture2D> get_item_preview(int p_item) const;

	void remove_item(int p_item);
	bool has_item(int p_item) const;

	void clear();

	int find_item_by_name(const String &p_name) const;

	Vector<int> get_item_list() const;
	int get_last_unused_item_id() const;

	MeshLibrary();
	~MeshLibrary();
};
