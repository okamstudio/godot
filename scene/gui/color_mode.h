/**************************************************************************/
/*  color_mode.h                                                          */
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

#ifndef COLOR_MODE_H
#define COLOR_MODE_H

#include "scene/gui/color_picker.h"

class ColorMode {
public:
	ColorPicker *color_picker = nullptr;

	virtual String get_name() const = 0;

	virtual int get_slider_count() const { return 3; }
	virtual float get_slider_step() const = 0;
	virtual float get_spinbox_arrow_step() const { return get_slider_step(); }
	virtual String get_slider_label(int idx) const = 0;
	virtual float get_slider_max(int idx) const = 0;
	virtual float get_slider_value(int idx) const = 0;

	virtual Color get_color() const = 0;

	virtual void _value_changed() {}

	virtual void slider_draw(int p_which) = 0;
	virtual bool apply_theme() const { return false; }
	virtual ColorPicker::PickerShapeType get_shape_override() const { return ColorPicker::SHAPE_MAX; }

	ColorMode(ColorPicker *p_color_picker);
	virtual ~ColorMode() {}
};

class ColorModeHSV : public ColorMode {
public:
	String labels[3] = { "H", "S", "V" };
	float slider_max[4] = { 359, 100, 100, 255 };
	float cached_hue = 0.0;
	float cached_saturation = 0.0;

	virtual String get_name() const override { return "HSV"; }

	virtual float get_slider_step() const override { return 1.0; }
	virtual String get_slider_label(int idx) const override;
	virtual float get_slider_max(int idx) const override;
	virtual float get_slider_value(int idx) const override;

	virtual Color get_color() const override;

	virtual void _value_changed() override;

	virtual void slider_draw(int p_which) override;

	ColorModeHSV(ColorPicker *p_color_picker) :
			ColorMode(p_color_picker) {}
};

class ColorModeRGB : public ColorMode {
public:
	String labels[3] = { "R", "G", "B" };

	virtual String get_name() const override { return "RGB"; }

	virtual float get_slider_step() const override { return 1; }
	virtual String get_slider_label(int idx) const override;
	virtual float get_slider_max(int idx) const override;
	virtual float get_slider_value(int idx) const override;

	virtual Color get_color() const override;

	virtual void slider_draw(int p_which) override;

	ColorModeRGB(ColorPicker *p_color_picker) :
			ColorMode(p_color_picker) {}
};

class ColorModeRAW : public ColorMode {
public:
	String labels[3] = { "R", "G", "B" };
	float slider_max[4] = { 100, 100, 100, 1 };

	virtual String get_name() const override { return "RAW"; }

	virtual float get_slider_step() const override { return 0.001; }
	virtual float get_spinbox_arrow_step() const override { return 0.01; }
	virtual String get_slider_label(int idx) const override;
	virtual float get_slider_max(int idx) const override;
	virtual float get_slider_value(int idx) const override;

	virtual Color get_color() const override;

	virtual void slider_draw(int p_which) override;
	virtual bool apply_theme() const override;

	ColorModeRAW(ColorPicker *p_color_picker) :
			ColorMode(p_color_picker) {}
};

class ColorModeOKHSL : public ColorMode {
public:
	String labels[3] = { "H", "S", "L" };
	float slider_max[4] = { 359, 100, 100, 255 };
	float cached_hue = 0.0;
	float cached_saturation = 0.0;

	virtual String get_name() const override { return "OKHSL"; }

	virtual float get_slider_step() const override { return 1.0; }
	virtual String get_slider_label(int idx) const override;
	virtual float get_slider_max(int idx) const override;
	virtual float get_slider_value(int idx) const override;

	virtual Color get_color() const override;

	virtual void _value_changed() override;

	virtual void slider_draw(int p_which) override;
	virtual ColorPicker::PickerShapeType get_shape_override() const override { return ColorPicker::SHAPE_OKHSL_CIRCLE; }

	ColorModeOKHSL(ColorPicker *p_color_picker) :
			ColorMode(p_color_picker) {}

	~ColorModeOKHSL() {}
};

#endif // COLOR_MODE_H
