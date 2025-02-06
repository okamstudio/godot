/**************************************************************************/
/*  jolt_custom_user_data_shape.h                                         */
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

#include "jolt_custom_decorated_shape.h"
#include "jolt_custom_shape_type.h"

class JoltCustomUserDataShapeSettings final : public JoltCustomDecoratedShapeSettings {
public:
	using JoltCustomDecoratedShapeSettings::JoltCustomDecoratedShapeSettings;

	virtual ShapeResult Create() const override;
};

class JoltCustomUserDataShape final : public JoltCustomDecoratedShape {
public:
	static void register_type();

	JoltCustomUserDataShape() :
			JoltCustomDecoratedShape(JoltCustomShapeSubType::OVERRIDE_USER_DATA) {}

	JoltCustomUserDataShape(const JoltCustomUserDataShapeSettings &p_settings, ShapeResult &p_result) :
			JoltCustomDecoratedShape(JoltCustomShapeSubType::OVERRIDE_USER_DATA, p_settings, p_result) {
		if (!p_result.HasError()) {
			p_result.Set(this);
		}
	}

	virtual JPH::uint64 GetSubShapeUserData(const JPH::SubShapeID &p_sub_shape_id) const override { return GetUserData(); }
};
