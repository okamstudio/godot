/**************************************************************************/
/*  summary_view.h                                                        */
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

#ifndef SUMMARY_VIEW_H
#define SUMMARY_VIEW_H

#include "../snapshot_data.h"
#include "scene/gui/margin_container.h"
#include "snapshot_view.h"

class CenterContainer;
class RichTextLabel;

class SummaryBlurb : public MarginContainer {
	GDCLASS(SummaryBlurb, MarginContainer);

public:
	RichTextLabel *label = nullptr;

	SummaryBlurb(const String &p_title, const String &p_rtl_content);
};

class SnapshotSummaryView : public SnapshotView {
	GDCLASS(SnapshotSummaryView, SnapshotView);

protected:
	VBoxContainer *blurb_list = nullptr;
	CenterContainer *explainer_text = nullptr;

	void _push_overview_blurb(const String &p_title, GameStateSnapshot *p_snapshot);
	void _push_node_blurb(const String &p_title, GameStateSnapshot *p_snapshot);
	void _push_refcounted_blurb(const String &p_title, GameStateSnapshot *p_snapshot);
	void _push_object_blurb(const String &p_title, GameStateSnapshot *p_snapshot);

public:
	SnapshotSummaryView();

	virtual void show_snapshot(GameStateSnapshot *p_data, GameStateSnapshot *p_diff_data) override;
	virtual void clear_snapshot() override;
};

#endif // SUMMARY_VIEW_H
