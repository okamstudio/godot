/**************************************************************************/
/*  configuration_info_editor_plugin.h                                    */
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

#ifndef CONFIGURATION_INFO_EDITOR_PLUGIN_H
#define CONFIGURATION_INFO_EDITOR_PLUGIN_H

#include "editor/plugins/editor_plugin.h"

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/margin_container.h"

class GridContainer;
class Label;
class PanelContainer;
class RichTextLabel;
class TextureRect;

// Inspector controls.
class ConfigurationInfoList : public MarginContainer {
	GDCLASS(ConfigurationInfoList, MarginContainer);

	Object *object = nullptr;
	Ref<StyleBox> bg_style;
	Ref<StyleBox> bg_style_hover;

	PanelContainer *bg_panel = nullptr;
	GridContainer *grid = nullptr;
	TextureRect *expand_icon = nullptr;
	Label *title_label = nullptr;
	RichTextLabel *config_info_text = nullptr;
	Control *list_filler_right = nullptr;

	String _get_summary_text(const Vector<ConfigurationInfo> &p_config_infos) const;
	void _update_background(bool p_hovering);
	void _update_content();
	void _update_toggler();
	virtual void gui_input(const Ref<InputEvent> &p_event) override;

protected:
	void _notification(int p_notification);

public:
	void set_object(Object *p_object);

	ConfigurationInfoList();
};

class EditorInspectorPluginConfigurationInfo : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginConfigurationInfo, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

// Editor plugin.
class ConfigurationInfoEditorPlugin : public EditorPlugin {
	GDCLASS(ConfigurationInfoEditorPlugin, EditorPlugin);

public:
	ConfigurationInfoEditorPlugin();
};

#endif // CONFIGURATION_INFO_EDITOR_PLUGIN_H
