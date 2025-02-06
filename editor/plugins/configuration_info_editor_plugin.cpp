/**************************************************************************/
/*  configuration_info_editor_plugin.cpp                                  */
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

#include "configuration_info_editor_plugin.h"

#include "editor/editor_configuration_info.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/texture_rect.h"

// Inspector controls.

String ConfigurationInfoList::_get_summary_text(const Vector<ConfigurationInfo> &p_config_infos) const {
	int errors = 0;
	int warnings = 0;
	int infos = 0;

	for (const ConfigurationInfo &config_info : p_config_infos) {
		switch (config_info.get_severity()) {
			case ConfigurationInfo::Severity::ERROR: {
				errors++;
			} break;
			case ConfigurationInfo::Severity::WARNING: {
				warnings++;
			} break;
			case ConfigurationInfo::Severity::INFO: {
				infos++;
			} break;
			default:
				break;
		}
	}

	PackedStringArray summary_parts;

	if (errors > 0) {
		summary_parts.append(vformat(TTRN("%d configuration error", "%d configuration errors", errors), errors));
	}

	if (warnings > 0) {
		if (summary_parts.is_empty()) {
			summary_parts.append(vformat(TTRN("%d configuration warning", "%d configuration warnings", warnings), warnings));
		} else {
			summary_parts.append(vformat(TTRN("%d warning", "%d warnings", warnings), warnings));
		}
	}

	if (infos > 0) {
		if (summary_parts.is_empty()) {
			summary_parts.append(vformat(TTRN("%d configuration info", "%d configuration infos", infos), infos));
		} else {
			summary_parts.append(vformat(TTRN("%d info", "%d infos", infos), infos));
		}
	}

	return String(", ").join(summary_parts);
}

void ConfigurationInfoList::_update_content() {
	if (!object) {
		hide();
		return;
	}

	const Vector<ConfigurationInfo> config_infos = EditorConfigurationInfo::get_configuration_info(object);
	if (config_infos.is_empty()) {
		hide();
		return;
	}

	title_label->set_text(_get_summary_text(config_infos));

	const Color &warning_color = get_theme_color("warning_color", EditorStringName(Editor));
	const Color &error_color = get_theme_color("error_color", EditorStringName(Editor));

	ConfigurationInfo::Severity max_severity = EditorConfigurationInfo::get_max_severity(config_infos);
	if (max_severity == ConfigurationInfo::Severity::WARNING) {
		title_label->add_theme_color_override(SceneStringName(font_color), warning_color);
	} else if (max_severity == ConfigurationInfo::Severity::ERROR) {
		title_label->add_theme_color_override(SceneStringName(font_color), error_color);
	} else {
		title_label->remove_theme_color_override(SceneStringName(font_color));
	}

	config_info_text->clear();
	for (const ConfigurationInfo &config_info : config_infos) {
		const String text = EditorConfigurationInfo::format_as_string(config_info, false, true);
		ConfigurationInfo::Severity severity = config_info.get_severity();
		const StringName icon = EditorConfigurationInfo::get_severity_icon(severity);

		config_info_text->push_context();
		config_info_text->push_paragraph(HORIZONTAL_ALIGNMENT_LEFT);

		if (!icon.is_empty()) {
			Ref<Texture2D> image = get_editor_theme_icon(icon);
			config_info_text->add_image(image);
		}

		if (severity == ConfigurationInfo::Severity::WARNING) {
			config_info_text->push_color(warning_color);
		} else if (severity == ConfigurationInfo::Severity::ERROR) {
			config_info_text->push_color(error_color);
		}

		config_info_text->add_text(" ");
		config_info_text->add_text(text);

		config_info_text->pop_context();
	}

	show();
}

void ConfigurationInfoList::_update_toggler() {
	Ref<Texture2D> arrow;
	if (config_info_text->is_visible()) {
		arrow = get_theme_icon("arrow", "Tree");
		set_tooltip_text(TTR("Collapse configuration info."));
	} else {
		if (is_layout_rtl()) {
			arrow = get_theme_icon("arrow_collapsed_mirrored", "Tree");
		} else {
			arrow = get_theme_icon("arrow_collapsed", "Tree");
		}
		set_tooltip_text(TTR("Expand configuration info."));
	}

	expand_icon->set_texture(arrow);
}

void ConfigurationInfoList::_update_background(bool p_hovering) {
	if (p_hovering) {
		if (bg_style_hover.is_valid()) {
			bg_panel->add_theme_style_override(SceneStringName(panel), bg_style_hover);
		}
	} else {
		if (bg_style.is_valid()) {
			bg_panel->add_theme_style_override(SceneStringName(panel), bg_style);
		}
	}
}

void ConfigurationInfoList::set_object(Object *p_object) {
	object = p_object;
	_update_content();
}

void ConfigurationInfoList::gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		bool state = !config_info_text->is_visible();

		config_info_text->set_visible(state);
		list_filler_right->set_visible(state);
		EditorSettings::get_singleton()->set_project_metadata("editor_metadata", "configuration_info_expanded_in_inspector", state);

		_update_toggler();
	}
}

void ConfigurationInfoList::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_MOUSE_ENTER: {
			_update_background(true);
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			_update_background(false);
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			bg_style = get_theme_stylebox(CoreStringName(normal), "Button");
			bg_style_hover = get_theme_stylebox("hover", "Button");

			_update_background(false);
			_update_content();
			_update_toggler();
		} break;
	}
}

ConfigurationInfoList::ConfigurationInfoList() {
	set_mouse_filter(MOUSE_FILTER_STOP);
	hide();

	bg_panel = memnew(PanelContainer);
	bg_panel->set_mouse_filter(MOUSE_FILTER_IGNORE);
	add_child(bg_panel);

	grid = memnew(GridContainer);
	grid->set_columns(2);
	bg_panel->add_child(grid);

	expand_icon = memnew(TextureRect);
	expand_icon->set_stretch_mode(TextureRect::StretchMode::STRETCH_KEEP_CENTERED);
	grid->add_child(expand_icon);

	title_label = memnew(Label);
	title_label->set_autowrap_mode(TextServer::AutowrapMode::AUTOWRAP_WORD);
	title_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_label->set_vertical_alignment(VerticalAlignment::VERTICAL_ALIGNMENT_CENTER);
	grid->add_child(title_label);

	list_filler_right = memnew(Control);
	grid->add_child(list_filler_right);

	config_info_text = memnew(RichTextLabel);
	config_info_text->add_theme_constant_override(SceneStringName(line_separation), 5);
	config_info_text->set_fit_content(true);
	config_info_text->set_selection_enabled(true);
	bool last_visible = EditorSettings::get_singleton()->get_project_metadata("editor_metadata", "configuration_info_expanded_in_inspector", false);
	config_info_text->set_visible(last_visible);
	grid->add_child(config_info_text);
}

bool EditorInspectorPluginConfigurationInfo::can_handle(Object *p_object) {
	return Object::cast_to<Node>(p_object) != nullptr || Object::cast_to<Resource>(p_object) != nullptr;
}

void EditorInspectorPluginConfigurationInfo::parse_begin(Object *p_object) {
	ConfigurationInfoList *config_info_list = memnew(ConfigurationInfoList);
	config_info_list->set_object(p_object);
	add_custom_control(config_info_list);
}

// Editor plugin.

ConfigurationInfoEditorPlugin::ConfigurationInfoEditorPlugin() {
	Ref<EditorInspectorPluginConfigurationInfo> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);
}
