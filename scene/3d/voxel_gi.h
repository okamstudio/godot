/**************************************************************************/
/*  voxel_gi.h                                                            */
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

#include "scene/3d/visual_instance_3d.h"

class CameraAttributes;

class VoxelGIData : public Resource {
	GDCLASS(VoxelGIData, Resource);

	RID probe;

	void _set_data(const Dictionary &p_data);
	Dictionary _get_data() const;

	Transform3D to_cell_xform;
	AABB bounds;
	Vector3 octree_size;

	float dynamic_range = 2.0;
	float energy = 1.0;
	float bias = 1.5;
	float normal_bias = 0.0;
	float propagation = 0.5;
	bool interior = false;
	bool use_two_bounces = true;

protected:
	static void _bind_methods();

public:
	void allocate(const Transform3D &p_to_cell_xform, const AABB &p_aabb, const Vector3 &p_octree_size, const Vector<uint8_t> &p_octree_cells, const Vector<uint8_t> &p_data_cells, const Vector<uint8_t> &p_distance_field, const Vector<int> &p_level_counts);
	AABB get_bounds() const;
	Vector3 get_octree_size() const;
	Vector<uint8_t> get_octree_cells() const;
	Vector<uint8_t> get_data_cells() const;
	Vector<uint8_t> get_distance_field() const;
	Vector<int> get_level_counts() const;
	Transform3D get_to_cell_xform() const;

	void set_dynamic_range(float p_range);
	float get_dynamic_range() const;

	void set_propagation(float p_propagation);
	float get_propagation() const;

	void set_energy(float p_energy);
	float get_energy() const;

	void set_bias(float p_bias);
	float get_bias() const;

	void set_normal_bias(float p_normal_bias);
	float get_normal_bias() const;

	void set_interior(bool p_enable);
	bool is_interior() const;

	void set_use_two_bounces(bool p_enable);
	bool is_using_two_bounces() const;

	virtual RID get_rid() const override;

	VoxelGIData();
	~VoxelGIData();
};

class VoxelGI : public VisualInstance3D {
	GDCLASS(VoxelGI, VisualInstance3D);

public:
	enum Subdiv {
		SUBDIV_64,
		SUBDIV_128,
		SUBDIV_256,
		SUBDIV_512,
		SUBDIV_MAX

	};

	typedef void (*BakeBeginFunc)();
	typedef bool (*BakeStepFunc)(int, const String &);
	typedef void (*BakeEndFunc)();

private:
	Ref<VoxelGIData> probe_data;

	RID voxel_gi;

	Subdiv subdiv = SUBDIV_128;
	Vector3 size = Vector3(20, 20, 20);
	Ref<CameraAttributes> camera_attributes;

	struct PlotMesh {
		Ref<Material> override_material;
		Vector<Ref<Material>> instance_materials;
		Ref<Mesh> mesh;
		Transform3D local_xform;
	};

	void _find_meshes(Node *p_at_node, List<PlotMesh> &plot_meshes);
	void _debug_bake();

	float _get_camera_exposure_normalization();

protected:
	static void _bind_methods();
#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_property) const;
#endif // DISABLE_DEPRECATED

public:
	static BakeBeginFunc bake_begin_function;
	static BakeStepFunc bake_step_function;
	static BakeEndFunc bake_end_function;

	void set_probe_data(const Ref<VoxelGIData> &p_data);
	Ref<VoxelGIData> get_probe_data() const;

	void set_subdiv(Subdiv p_subdiv);
	Subdiv get_subdiv() const;

	void set_size(const Vector3 &p_size);
	Vector3 get_size() const;

	void set_camera_attributes(const Ref<CameraAttributes> &p_camera_attributes);
	Ref<CameraAttributes> get_camera_attributes() const;

	Vector3i get_estimated_cell_size() const;

	void bake(Node *p_from_node = nullptr, bool p_create_visual_debug = false);

	virtual AABB get_aabb() const override;

	PackedStringArray get_configuration_warnings() const override;

	VoxelGI();
	~VoxelGI();
};

VARIANT_ENUM_CAST(VoxelGI::Subdiv)
