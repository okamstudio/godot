/**************************************************************************/
/*  canvas_item_material.h                                                */
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

#include "scene/resources/material.h"

class CanvasItemMaterial : public Material {
	GDCLASS(CanvasItemMaterial, Material);

public:
	enum BlendMode {
		BLEND_MODE_MIX,
		BLEND_MODE_ADD,
		BLEND_MODE_SUB,
		BLEND_MODE_MUL,
		BLEND_MODE_PREMULT_ALPHA,
		BLEND_MODE_DISABLED
	};

	enum LightMode {
		LIGHT_MODE_NORMAL,
		LIGHT_MODE_UNSHADED,
		LIGHT_MODE_LIGHT_ONLY
	};

private:
	union MaterialKey {
		struct {
			uint32_t blend_mode : 4;
			uint32_t light_mode : 4;
			uint32_t particles_animation : 1;
			uint32_t invalid_key : 1;
		};

		uint32_t key = 0;

		static uint32_t hash(const MaterialKey &p_key) {
			return hash_murmur3_one_32(p_key.key);
		}
		bool operator==(const MaterialKey &p_key) const {
			return key == p_key.key;
		}
	};

	struct ShaderNames {
		StringName particles_anim_h_frames;
		StringName particles_anim_v_frames;
		StringName particles_anim_loop;
	};

	static ShaderNames *shader_names;

	struct ShaderData {
		RID shader;
		int users = 0;
	};

	static HashMap<MaterialKey, ShaderData, MaterialKey> shader_map;

	MaterialKey current_key;

	_FORCE_INLINE_ MaterialKey _compute_key() const {
		MaterialKey mk;
		mk.key = 0;
		mk.blend_mode = blend_mode;
		mk.light_mode = light_mode;
		mk.particles_animation = particles_animation;
		return mk;
	}

	static Mutex material_mutex;
	static SelfList<CanvasItemMaterial>::List dirty_materials;
	SelfList<CanvasItemMaterial> element;

	void _update_shader();
	_FORCE_INLINE_ void _queue_shader_change();

	BlendMode blend_mode = BLEND_MODE_MIX;
	LightMode light_mode = LIGHT_MODE_NORMAL;
	bool particles_animation = false;

	// Proper values set in constructor.
	int particles_anim_h_frames = 0;
	int particles_anim_v_frames = 0;
	bool particles_anim_loop = false;

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	void set_blend_mode(BlendMode p_blend_mode);
	BlendMode get_blend_mode() const;

	void set_light_mode(LightMode p_light_mode);
	LightMode get_light_mode() const;

	void set_particles_animation(bool p_particles_anim);
	bool get_particles_animation() const;

	void set_particles_anim_h_frames(int p_frames);
	int get_particles_anim_h_frames() const;
	void set_particles_anim_v_frames(int p_frames);
	int get_particles_anim_v_frames() const;

	void set_particles_anim_loop(bool p_loop);
	bool get_particles_anim_loop() const;

	static void init_shaders();
	static void finish_shaders();
	static void flush_changes();

	virtual RID get_shader_rid() const override;

	virtual Shader::Mode get_shader_mode() const override;

	CanvasItemMaterial();
	virtual ~CanvasItemMaterial();
};

VARIANT_ENUM_CAST(CanvasItemMaterial::BlendMode)
VARIANT_ENUM_CAST(CanvasItemMaterial::LightMode)
