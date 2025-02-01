/**************************************************************************/
/*  texture_region_editor_plugin.h                                        */
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

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/dialogs.h"

class AtlasTexture;
class NinePatchRect;
class OptionButton;
class PanelContainer;
class Sprite2D;
class Sprite3D;
class StyleBoxTexture;
class ViewPanner;

class TextureRegionEditor : public AcceptDialog {
	GDCLASS(TextureRegionEditor, AcceptDialog);

	enum SnapMode {
		SNAP_NONE,
		SNAP_PIXEL,
		SNAP_GRID,
		SNAP_AUTOSLICE
	};

	friend class TextureRegionEditorPlugin;
	OptionButton *snap_mode_button = nullptr;
	Button *zoom_in = nullptr;
	Button *zoom_reset = nullptr;
	Button *zoom_out = nullptr;
	HBoxContainer *hb_grid = nullptr; //For showing/hiding the grid controls when changing the SnapMode
	SpinBox *sb_step_y = nullptr;
	SpinBox *sb_step_x = nullptr;
	SpinBox *sb_off_y = nullptr;
	SpinBox *sb_off_x = nullptr;
	SpinBox *sb_sep_y = nullptr;
	SpinBox *sb_sep_x = nullptr;

	PanelContainer *texture_preview = nullptr;
	Panel *texture_overlay = nullptr;

	VScrollBar *vscroll = nullptr;
	HScrollBar *hscroll = nullptr;

	Vector2 draw_ofs;
	float draw_zoom = 1.0;
	float min_draw_zoom = 1.0;
	float max_draw_zoom = 1.0;
	bool updating_scroll = false;

	SnapMode snap_mode = SNAP_NONE;
	Vector2 snap_offset;
	Vector2 snap_step;
	Vector2 snap_separation;

	Sprite2D *node_sprite_2d = nullptr;
	Sprite3D *node_sprite_3d = nullptr;
	NinePatchRect *node_ninepatch = nullptr;
	Ref<StyleBoxTexture> res_stylebox;
	Ref<AtlasTexture> res_atlas_texture;

	Rect2 rect;
	Rect2 rect_prev;
	float prev_margin = 0.0f;
	int edited_margin = -1;
	HashMap<RID, List<Rect2>> cache_map;
	List<Rect2> autoslice_cache;
	bool autoslice_is_dirty = true;

	bool drag = false;
	bool creating = false;
	Vector2 drag_from;
	int drag_index = -1;
	bool request_center = false;

	Ref<ViewPanner> panner;
	void _pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event);
	void _zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event);
	void _scroll_changed(float);
	Transform2D _get_offset_transform() const;

	void _set_snap_mode(int p_mode);
	void _set_snap_off_x(float p_val);
	void _set_snap_off_y(float p_val);
	void _set_snap_step_x(float p_val);
	void _set_snap_step_y(float p_val);
	void _set_snap_sep_x(float p_val);
	void _set_snap_sep_y(float p_val);

	void _zoom_on_position(float p_zoom, Point2 p_position = Point2());
	void _zoom_in();
	void _zoom_reset();
	void _zoom_out();

	void _apply_rect(const Rect2 &p_rect);
	void _update_rect();
	void _update_autoslice();

	Ref<Texture2D> _get_edited_object_texture() const;
	Rect2 _get_edited_object_region() const;
	void _texture_changed();
	void _node_removed(Node *p_node);

	void _edit_region();
	void _clear_edited_object();

	void _draw_margin_line(Vector2 p_from, Vector2 p_to);

	void _set_grid_parameters_clamping(bool p_enabled);

protected:
	void _notification(int p_what);
	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;
	static void _bind_methods();

	void _texture_preview_draw();
	void _texture_overlay_draw();
	void _texture_overlay_input(const Ref<InputEvent> &p_input);

	Vector2 snap_point(Vector2 p_target) const;

public:
	void edit(Object *p_obj);

	TextureRegionEditor();
};

//

class EditorInspectorPluginTextureRegion : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginTextureRegion, EditorInspectorPlugin);

	TextureRegionEditor *texture_region_editor = nullptr;

	void _region_edit(Object *p_object);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) override;

	EditorInspectorPluginTextureRegion();
};

class TextureRegionEditorPlugin : public EditorPlugin {
	GDCLASS(TextureRegionEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "TextureRegion"; }

	TextureRegionEditorPlugin();
};
