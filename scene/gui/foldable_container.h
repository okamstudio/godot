/**************************************************************************/
/*  foldable_container.h                                                  */
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

#ifndef FOLDABLE_CONTAINER_H
#define FOLDABLE_CONTAINER_H

#include "scene/gui/container.h"
#include "scene/property_list_helper.h"
#include "scene/resources/text_line.h"

class FoldableGroup;

class FoldableContainer : public Container {
	GDCLASS(FoldableContainer, Container)

public:
	enum TitlePosition {
		POSITION_TOP,
		POSITION_BOTTOM,
		POSITION_MAX
	};

private:
	Ref<FoldableGroup> foldable_group;
	bool changing_group = false;
	bool folded = false;
	String text;
	Ref<TextLine> text_buf;
	String language;
	TextDirection text_direction = TEXT_DIRECTION_AUTO;
	HorizontalAlignment text_alignment = HORIZONTAL_ALIGNMENT_LEFT;
	TextServer::OverrunBehavior overrun_behavior = TextServer::OVERRUN_NO_TRIMMING;
	TitlePosition title_position = POSITION_TOP;

	bool is_hovering = false;
	int title_panel_height = 0;

	LocalVector<Control *> title_controls;

	struct ThemeCache {
		Ref<StyleBox> title_style;
		Ref<StyleBox> title_hover_style;
		Ref<StyleBox> title_collapsed_style;
		Ref<StyleBox> title_collapsed_hover_style;
		Ref<StyleBox> panel_style;
		Ref<StyleBox> focus_style;

		Color title_font_color;
		Color title_hovered_font_color;
		Color title_collapsed_font_color;
		Color title_font_outline_color;

		Ref<Font> title_font;
		int title_font_size = 0;
		int title_font_outline_size = 0;

		Ref<Texture2D> expanded_arrow;
		Ref<Texture2D> expanded_arrow_mirrored;
		Ref<Texture2D> folded_arrow;
		Ref<Texture2D> folded_arrow_mirrored;

		int h_separation = 0;
	} theme_cache;

	Ref<StyleBox> _get_title_style() const;
	Ref<Texture2D> _get_title_icon() const;
	Size2 _get_title_panel_min_size() const;
	real_t _get_title_controls_width() const;

	void _shape();
	HorizontalAlignment _get_actual_alignment() const;
	void _update_group();

protected:
	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	virtual String get_tooltip(const Point2 &p_pos) const override;
	void _notification(int p_what);
	static void _bind_methods();

public:
	void fold();
	void expand();

	void set_folded(bool p_folded);
	bool is_folded() const;

	void set_foldable_group(const Ref<FoldableGroup> &p_group);
	Ref<FoldableGroup> get_foldable_group() const;

	void set_text(const String &p_text);
	String get_text() const;

	void set_text_alignment(HorizontalAlignment p_alignment);
	HorizontalAlignment get_text_alignment() const;

	void set_text_direction(TextDirection p_text_direction);
	TextDirection get_text_direction() const;

	void set_text_overrun_behavior(TextServer::OverrunBehavior p_overrun_behavior);
	TextServer::OverrunBehavior get_text_overrun_behavior() const;

	void set_language(const String &p_language);
	String get_language() const;

	void set_title_position(TitlePosition p_title_position);
	TitlePosition get_title_position() const;

	void add_title_bar_control(Control *p_control);
	void remove_title_bar_control(Control *p_control);

	virtual Size2 get_minimum_size() const override;

	virtual Vector<int> get_allowed_size_flags_horizontal() const override { return { SIZE_FILL, SIZE_SHRINK_BEGIN, SIZE_SHRINK_CENTER, SIZE_SHRINK_END }; }
	virtual Vector<int> get_allowed_size_flags_vertical() const override { return { SIZE_FILL, SIZE_SHRINK_BEGIN, SIZE_SHRINK_CENTER, SIZE_SHRINK_END }; }

	FoldableContainer(const String &p_text = String());
	~FoldableContainer();
};

VARIANT_ENUM_CAST(FoldableContainer::TitlePosition);

class FoldableGroup : public Resource {
	GDCLASS(FoldableGroup, Resource);
	friend class FoldableContainer;
	HashSet<FoldableContainer *> containers;
	bool allow_folding_all = false;
	bool updating_group = false;

protected:
	static void _bind_methods();

public:
	FoldableContainer *get_expanded_container();
	void get_containers(List<FoldableContainer *> *r_containers);
	TypedArray<FoldableContainer> _get_containers();
	void set_allow_folding_all(bool p_enabled);
	bool is_allow_folding_all();
	FoldableGroup();
};

#endif // FOLDABLE_CONTAINER_H
