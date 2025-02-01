/**************************************************************************/
/*  openxr_composition_layer_quad.h                                       */
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

#include <openxr/openxr.h>

#include "openxr_composition_layer.h"

class OpenXRCompositionLayerQuad : public OpenXRCompositionLayer {
	GDCLASS(OpenXRCompositionLayerQuad, OpenXRCompositionLayer);

	XrCompositionLayerQuad composition_layer = {
		XR_TYPE_COMPOSITION_LAYER_QUAD, // type
		nullptr, // next
		0, // layerFlags
		XR_NULL_HANDLE, // space
		XR_EYE_VISIBILITY_BOTH, // eyeVisibility
		{}, // subImage
		{ { 0, 0, 0, 0 }, { 0, 0, 0 } }, // pose
		{ 1.0, 1.0 }, // size
	};

	Size2 quad_size = Size2(1.0, 1.0);

protected:
	static void _bind_methods();

	void _notification(int p_what);

	void update_transform();

	virtual Ref<Mesh> _create_fallback_mesh() override;

public:
	void set_quad_size(const Size2 &p_size);
	Size2 get_quad_size() const;

	virtual Vector2 intersects_ray(const Vector3 &p_origin, const Vector3 &p_direction) const override;

	OpenXRCompositionLayerQuad();
	~OpenXRCompositionLayerQuad();
};
