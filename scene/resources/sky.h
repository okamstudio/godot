/**************************************************************************/
/*  sky.h                                                                 */
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
#include "scene/resources/material.h"

class Sky : public Resource {
	GDCLASS(Sky, Resource);

public:
	enum RadianceSize {
		RADIANCE_SIZE_32,
		RADIANCE_SIZE_64,
		RADIANCE_SIZE_128,
		RADIANCE_SIZE_256,
		RADIANCE_SIZE_512,
		RADIANCE_SIZE_1024,
		RADIANCE_SIZE_2048,
		RADIANCE_SIZE_MAX
	};

	enum ProcessMode {
		PROCESS_MODE_AUTOMATIC,
		PROCESS_MODE_QUALITY,
		PROCESS_MODE_INCREMENTAL,
		PROCESS_MODE_REALTIME
	};

private:
	RID sky;
	ProcessMode mode = PROCESS_MODE_AUTOMATIC;
	RadianceSize radiance_size = RADIANCE_SIZE_256;
	Ref<Material> sky_material;

protected:
	static void _bind_methods();

public:
	void set_radiance_size(RadianceSize p_size);
	RadianceSize get_radiance_size() const;

	void set_process_mode(ProcessMode p_mode);
	ProcessMode get_process_mode() const;

	void set_material(const Ref<Material> &p_material);
	Ref<Material> get_material() const;

	virtual RID get_rid() const override;

	Sky();
	~Sky();
};

VARIANT_ENUM_CAST(Sky::RadianceSize)
VARIANT_ENUM_CAST(Sky::ProcessMode)
