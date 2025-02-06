/**************************************************************************/
/*  xr_vrs.h                                                              */
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

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/rid.h"
#include "core/variant/variant.h"

/* This is a helper class for generating stereoscopic VRS images */

class XRVRS : public Object {
	GDCLASS(XRVRS, Object);

private:
	float vrs_min_radius = 20.0;
	float vrs_strength = 1.0;
	Rect2i vrs_render_region;
	bool vrs_dirty = true;

	RID vrs_texture;
	Size2i target_size;
	PackedVector2Array eye_foci;

protected:
	static void _bind_methods();

public:
	~XRVRS();

	float get_vrs_min_radius() const;
	void set_vrs_min_radius(float p_vrs_min_radius);
	float get_vrs_strength() const;
	void set_vrs_strength(float p_vrs_strength);
	Rect2i get_vrs_render_region() const;
	void set_vrs_render_region(const Rect2i &p_vrs_render_region);

	RID make_vrs_texture(const Size2 &p_target_size, const PackedVector2Array &p_eye_foci);
};
