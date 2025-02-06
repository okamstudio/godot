/**************************************************************************/
/*  popup.h                                                               */
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

#include "scene/main/window.h"

#include "core/templates/local_vector.h"

class Panel;

class Popup : public Window {
	GDCLASS(Popup, Window);

	LocalVector<Window *> visible_parents;
	bool popped_up = false;

public:
	enum HideReason {
		HIDE_REASON_NONE,
		HIDE_REASON_CANCELED, // E.g., because of rupture of UI flow (app unfocused). Includes closed programmatically.
		HIDE_REASON_UNFOCUSED, // E.g., user clicked outside.
	};

private:
	HideReason hide_reason = HIDE_REASON_NONE;

	void _initialize_visible_parents();
	void _deinitialize_visible_parents();

protected:
	void _close_pressed();
	virtual Rect2i _popup_adjust_rect() const override;
	virtual void _input_from_window(const Ref<InputEvent> &p_event) override;

	void _notification(int p_what);
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();

	virtual void _parent_focused();

	virtual void _post_popup() override;

public:
	HideReason get_hide_reason() const { return hide_reason; }

	Popup();
	~Popup();
};

class PopupPanel : public Popup {
	GDCLASS(PopupPanel, Popup);

	Panel *panel = nullptr;

	struct ThemeCache {
		Ref<StyleBox> panel_style;
	} theme_cache;

	mutable Rect2i pre_popup_rect;

protected:
	virtual void _input_from_window(const Ref<InputEvent> &p_event) override;

	virtual Rect2i _popup_adjust_rect() const override;

	void _update_shadow_offsets() const;
	void _update_child_rects() const;

	void _notification(int p_what);
	static void _bind_methods();

	virtual Size2 _get_contents_minimum_size() const override;

public:
#ifdef TOOLS_ENABLED
	PackedStringArray get_configuration_warnings() const override;
#endif

	PopupPanel();
};
