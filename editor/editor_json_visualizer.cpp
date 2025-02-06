/**************************************************************************/
/*  editor_json_visualizer.cpp                                            */
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

#include "editor_json_visualizer.h"

#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/text_edit.h"
#include "servers/rendering/shader_language.h"

EditorJsonVisualizerSyntaxHighlighter::EditorJsonVisualizerSyntaxHighlighter(const List<String> &p_keywords) {
	set_number_color(EDITOR_GET("text_editor/theme/highlighting/number_color"));
	set_symbol_color(EDITOR_GET("text_editor/theme/highlighting/symbol_color"));
	set_function_color(EDITOR_GET("text_editor/theme/highlighting/function_color"));
	set_member_variable_color(EDITOR_GET("text_editor/theme/highlighting/member_variable_color"));

	clear_keyword_colors();
	const Color keyword_color = EDITOR_GET("text_editor/theme/highlighting/keyword_color");
	const Color control_flow_keyword_color = EDITOR_GET("text_editor/theme/highlighting/control_flow_keyword_color");

	for (const String &keyword : p_keywords) {
		if (ShaderLanguage::is_control_flow_keyword(keyword)) {
			add_keyword_color(keyword, control_flow_keyword_color);
		} else {
			add_keyword_color(keyword, keyword_color);
		}
	}

	// Colorize comments.
	const Color comment_color = EDITOR_GET("text_editor/theme/highlighting/comment_color");
	clear_color_regions();
	add_color_region("/*", "*/", comment_color, false);
	add_color_region("//", "", comment_color, true);

	// Colorize preprocessor statements.
	const Color user_type_color = EDITOR_GET("text_editor/theme/highlighting/user_type_color");
	add_color_region("#", "", user_type_color, true);

	set_uint_suffix_enabled(true);
}

void EditorJsonVisualizer::load_theme(Ref<EditorJsonVisualizerSyntaxHighlighter> p_syntax_highlighter) {
	set_editable(false);
	set_syntax_highlighter(p_syntax_highlighter);
	add_theme_font_override(SceneStringName(font), get_theme_font("source", EditorStringName(EditorFonts)));
	add_theme_font_size_override(SceneStringName(font_size), get_theme_font_size("source_size", EditorStringName(EditorFonts)));
	add_theme_constant_override("line_spacing", EDITOR_GET("text_editor/theme/line_spacing"));

	// Appearance: Caret
	set_caret_type((TextEdit::CaretType)EDITOR_GET("text_editor/appearance/caret/type").operator int());
	set_caret_blink_enabled(EDITOR_GET("text_editor/appearance/caret/caret_blink"));
	set_caret_blink_interval(EDITOR_GET("text_editor/appearance/caret/caret_blink_interval"));
	set_highlight_current_line(EDITOR_GET("text_editor/appearance/caret/highlight_current_line"));
	set_highlight_all_occurrences(EDITOR_GET("text_editor/appearance/caret/highlight_all_occurrences"));

	// Appearance: Gutters
	set_draw_line_numbers(EDITOR_GET("text_editor/appearance/gutters/show_line_numbers"));
	set_line_numbers_zero_padded(EDITOR_GET("text_editor/appearance/gutters/line_numbers_zero_padded"));

	// Appearance: Minimap
	set_draw_minimap(EDITOR_GET("text_editor/appearance/minimap/show_minimap"));
	set_minimap_width((int)EDITOR_GET("text_editor/appearance/minimap/minimap_width") * EDSCALE);

	// Appearance: Lines
	set_line_folding_enabled(EDITOR_GET("text_editor/appearance/lines/code_folding"));
	set_draw_fold_gutter(EDITOR_GET("text_editor/appearance/lines/code_folding"));
	set_line_wrapping_mode((TextEdit::LineWrappingMode)EDITOR_GET("text_editor/appearance/lines/word_wrap").operator int());
	set_autowrap_mode((TextServer::AutowrapMode)EDITOR_GET("text_editor/appearance/lines/autowrap_mode").operator int());

	// Appearance: Whitespace
	set_draw_tabs(EDITOR_GET("text_editor/appearance/whitespace/draw_tabs"));
	set_draw_spaces(EDITOR_GET("text_editor/appearance/whitespace/draw_spaces"));
	add_theme_constant_override("line_spacing", EDITOR_GET("text_editor/appearance/whitespace/line_spacing"));

	// Behavior: Navigation
	set_scroll_past_end_of_file_enabled(EDITOR_GET("text_editor/behavior/navigation/scroll_past_end_of_file"));
	set_smooth_scroll_enabled(EDITOR_GET("text_editor/behavior/navigation/smooth_scrolling"));
	set_v_scroll_speed(EDITOR_GET("text_editor/behavior/navigation/v_scroll_speed"));
	set_drag_and_drop_selection_enabled(EDITOR_GET("text_editor/behavior/navigation/drag_and_drop_selection"));

	// Behavior: Indent
	set_indent_size(EDITOR_GET("text_editor/behavior/indent/size"));
	set_auto_indent_enabled(EDITOR_GET("text_editor/behavior/indent/auto_indent"));
	set_indent_wrapped_lines(EDITOR_GET("text_editor/behavior/indent/indent_wrapped_lines"));
}

void EditorJsonVisualizer::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		Ref<Font> source_font = get_theme_font("source", EditorStringName(EditorFonts));
		int source_font_size = get_theme_font_size("source_size", EditorStringName(EditorFonts));
		int line_spacing = EDITOR_GET("text_editor/theme/line_spacing");
		if (get_theme_font(SceneStringName(font)) != source_font) {
			add_theme_font_override(SceneStringName(font), source_font);
		}
		if (get_theme_font_size(SceneStringName(font_size)) != source_font_size) {
			add_theme_font_size_override(SceneStringName(font_size), source_font_size);
		}
		if (get_theme_constant("line_spacing") != line_spacing) {
			add_theme_constant_override("line_spacing", line_spacing);
		}
	}
}
