/**************************************************************************/
/*  grid_container.h                                                      */
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

#ifndef GRID_CONTAINER_H
#define GRID_CONTAINER_H

#include "scene/gui/container.h"

class GridContainer : public Container {
	GDCLASS(GridContainer, Container);

	struct ThemeCache {
		int h_separation = 0;
		int v_separation = 0;
	} theme_cache;

private:
	int rows = 1;
	int columns = 1;
	bool vertical = true;

protected:
	bool is_fixed = false;

	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

public:
	void set_rows(int p_rows);
	int get_rows() const;

	void set_columns(int p_columns);
	int get_columns() const;

	void set_vertical(bool p_vertical);
	bool is_vertical() const;

	virtual Size2 get_minimum_size() const override;

	int get_h_separation() const;

	GridContainer(bool p_vertical = true);
};

class HGridContainer : public GridContainer {
	GDCLASS(HGridContainer, GridContainer);

public:
	HGridContainer() :
			GridContainer(false) { is_fixed = true; }
};

class VGridContainer : public GridContainer {
	GDCLASS(VGridContainer, GridContainer);

public:
	VGridContainer() :
			GridContainer(true) { is_fixed = true; }
};

#endif // GRID_CONTAINER_H
