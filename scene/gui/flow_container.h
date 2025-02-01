/**************************************************************************/
/*  flow_container.h                                                      */
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

#include "scene/gui/container.h"

class FlowContainer : public Container {
	GDCLASS(FlowContainer, Container);

public:
	enum AlignmentMode {
		ALIGNMENT_BEGIN,
		ALIGNMENT_CENTER,
		ALIGNMENT_END
	};
	enum LastWrapAlignmentMode {
		LAST_WRAP_ALIGNMENT_INHERIT,
		LAST_WRAP_ALIGNMENT_BEGIN,
		LAST_WRAP_ALIGNMENT_CENTER,
		LAST_WRAP_ALIGNMENT_END
	};

private:
	int cached_size = 0;
	int cached_line_count = 0;
	int cached_line_max_child_count = 0;
	int cached_items_on_last_row = 0;

	bool vertical = false;
	bool reverse_fill = false;
	AlignmentMode alignment = ALIGNMENT_BEGIN;
	LastWrapAlignmentMode last_wrap_alignment = LAST_WRAP_ALIGNMENT_INHERIT;

	struct ThemeCache {
		int h_separation = 0;
		int v_separation = 0;
	} theme_cache;

	void _resort();

protected:
	bool is_fixed = false;

	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

public:
	int get_line_count() const;
	int get_line_max_child_count() const;

	void set_alignment(AlignmentMode p_alignment);
	AlignmentMode get_alignment() const;

	void set_last_wrap_alignment(LastWrapAlignmentMode p_last_wrap_alignment);
	LastWrapAlignmentMode get_last_wrap_alignment() const;

	void set_vertical(bool p_vertical);
	bool is_vertical() const;

	void set_reverse_fill(bool p_reverse_fill);
	bool is_reverse_fill() const;

	virtual Size2 get_minimum_size() const override;

	virtual Vector<int> get_allowed_size_flags_horizontal() const override;
	virtual Vector<int> get_allowed_size_flags_vertical() const override;

	FlowContainer(bool p_vertical = false);
};

class HFlowContainer : public FlowContainer {
	GDCLASS(HFlowContainer, FlowContainer);

public:
	HFlowContainer() :
			FlowContainer(false) { is_fixed = true; }
};

class VFlowContainer : public FlowContainer {
	GDCLASS(VFlowContainer, FlowContainer);

public:
	VFlowContainer() :
			FlowContainer(true) { is_fixed = true; }
};

VARIANT_ENUM_CAST(FlowContainer::AlignmentMode);
VARIANT_ENUM_CAST(FlowContainer::LastWrapAlignmentMode);
