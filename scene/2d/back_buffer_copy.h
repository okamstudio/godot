/**************************************************************************/
/*  back_buffer_copy.h                                                    */
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

#include "scene/2d/node_2d.h"

class BackBufferCopy : public Node2D {
	GDCLASS(BackBufferCopy, Node2D);

public:
	enum CopyMode {
		COPY_MODE_DISABLED,
		COPY_MODE_RECT,
		COPY_MODE_VIEWPORT
	};

private:
	Rect2 rect = Rect2(-100, -100, 200, 200);
	CopyMode copy_mode = COPY_MODE_RECT;

	void _update_copy_mode();

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
#ifdef DEBUG_ENABLED
	Rect2 _edit_get_rect() const override;
	virtual bool _edit_use_rect() const override;
#endif // DEBUG_ENABLED

	void set_rect(const Rect2 &p_rect);
	Rect2 get_rect() const;
	Rect2 get_anchorable_rect() const override;

	void set_copy_mode(CopyMode p_mode);
	CopyMode get_copy_mode() const;

	BackBufferCopy();
	~BackBufferCopy();
};

VARIANT_ENUM_CAST(BackBufferCopy::CopyMode);
