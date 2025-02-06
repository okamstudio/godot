/**************************************************************************/
/*  editor_properties_vector.h                                            */
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

#ifndef EDITOR_PROPERTIES_VECTOR_H
#define EDITOR_PROPERTIES_VECTOR_H

#include "editor/editor_inspector.h"

class EditorSpinSlider;
class TextureButton;

class EditorPropertyVectorN : public EditorProperty {
	GDCLASS(EditorPropertyVectorN, EditorProperty);

	static const String COMPONENT_LABELS[4];

	int component_count = 0;
	Variant::Type vector_type;

	Vector<EditorSpinSlider *> spin_sliders;
	TextureButton *linked = nullptr;
	Vector<double> ratio;
	bool is_grabbed = false;

	bool radians_as_degrees = false;

	void _update_ratio();
	void _store_link(bool p_linked);
	void _grab_changed(bool p_grab);
	void _value_changed(double p_val, const String &p_name);

protected:
	virtual void _set_read_only(bool p_read_only) override;
	void _notification(int p_what);

public:
	virtual void update_property() override;
	void setup(double p_min, double p_max, double p_step = 1.0, bool p_hide_slider = true, bool p_link = false, const String &p_suffix = String(), bool p_radians_as_degrees = false, bool p_is_int = false);
	EditorPropertyVectorN(Variant::Type p_type, bool p_force_wide, bool p_horizontal);
};

class EditorPropertyVector2 : public EditorPropertyVectorN {
	GDCLASS(EditorPropertyVector2, EditorPropertyVectorN);

public:
	EditorPropertyVector2(bool p_force_wide = false);
};

class EditorPropertyVector2i : public EditorPropertyVectorN {
	GDCLASS(EditorPropertyVector2i, EditorPropertyVectorN);

public:
	EditorPropertyVector2i(bool p_force_wide = false);
};

class EditorPropertyVector3 : public EditorPropertyVectorN {
	GDCLASS(EditorPropertyVector3, EditorPropertyVectorN);

public:
	EditorPropertyVector3(bool p_force_wide = false);
};

class EditorPropertyVector3i : public EditorPropertyVectorN {
	GDCLASS(EditorPropertyVector3i, EditorPropertyVectorN);

public:
	EditorPropertyVector3i(bool p_force_wide = false);
};

class EditorPropertyVector4 : public EditorPropertyVectorN {
	GDCLASS(EditorPropertyVector4, EditorPropertyVectorN);

public:
	EditorPropertyVector4(bool p_force_wide = false);
};

class EditorPropertyVector4i : public EditorPropertyVectorN {
	GDCLASS(EditorPropertyVector4i, EditorPropertyVectorN);

public:
	EditorPropertyVector4i(bool p_force_wide = false);
};

#endif // EDITOR_PROPERTIES_VECTOR_H
