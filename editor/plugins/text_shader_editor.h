/**************************************************************************/
/*  text_shader_editor.h                                                  */
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

#ifndef TEXT_SHADER_EDITOR_H
#define TEXT_SHADER_EDITOR_H

#include "editor/code_editor.h"
#include "editor/plugins/shader/shader_editor.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/rich_text_label.h"
#include "servers/rendering/shader_warnings.h"

class GDShaderSyntaxHighlighter : public CodeHighlighter {
	GDCLASS(GDShaderSyntaxHighlighter, CodeHighlighter)

private:
	Vector<Point2i> disabled_branch_regions;
	Color disabled_branch_color;

public:
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override;

	void add_disabled_branch_region(const Point2i &p_region);
	void clear_disabled_branch_regions();
	void set_disabled_branch_color(const Color &p_color);
};

class ShaderTextEditor : public CodeTextEditor {
	GDCLASS(ShaderTextEditor, CodeTextEditor);

	Color marked_line_color = Color(1, 1, 1);

	struct WarningsComparator {
		_ALWAYS_INLINE_ bool operator()(const ShaderWarning &p_a, const ShaderWarning &p_b) const { return (p_a.get_line() < p_b.get_line()); }
	};

	Ref<GDShaderSyntaxHighlighter> syntax_highlighter;
	RichTextLabel *warnings_panel = nullptr;
	Ref<Shader> shader;
	Ref<ShaderInclude> shader_inc;
	List<ShaderWarning> warnings;
	Error last_compile_result = Error::OK;

	void _check_shader_mode();
	void _update_warning_panel();

	bool block_shader_changed = false;
	void _shader_changed();

	uint32_t dependencies_version = 0; // Incremented if deps changed

protected:
	void _notification(int p_what);
	static void _bind_methods();
	virtual void _load_theme_settings() override;

	virtual void _code_complete_script(const String &p_code, List<ScriptLanguage::CodeCompletionOption> *r_options) override;

public:
	void set_block_shader_changed(bool p_block) { block_shader_changed = p_block; }
	uint32_t get_dependencies_version() const { return dependencies_version; }

	virtual void _validate_script() override;

	void reload_text();
	void set_warnings_panel(RichTextLabel *p_warnings_panel);

	Ref<Shader> get_edited_shader() const;
	Ref<ShaderInclude> get_edited_shader_include() const;

	void set_edited_shader(const Ref<Shader> &p_shader);
	void set_edited_shader(const Ref<Shader> &p_shader, const String &p_code);
	void set_edited_shader_include(const Ref<ShaderInclude> &p_include);
	void set_edited_shader_include(const Ref<ShaderInclude> &p_include, const String &p_code);
	void set_edited_code(const String &p_code);

	ShaderTextEditor();
};

class TextShaderEditor : public ShaderEditor {
	GDCLASS(TextShaderEditor, ShaderEditor);

	enum {
		EDIT_UNDO,
		EDIT_REDO,
		EDIT_CUT,
		EDIT_COPY,
		EDIT_PASTE,
		EDIT_SELECT_ALL,
		EDIT_MOVE_LINE_UP,
		EDIT_MOVE_LINE_DOWN,
		EDIT_INDENT,
		EDIT_UNINDENT,
		EDIT_DELETE_LINE,
		EDIT_DUPLICATE_SELECTION,
		EDIT_DUPLICATE_LINES,
		EDIT_TOGGLE_WORD_WRAP,
		EDIT_TOGGLE_COMMENT,
		EDIT_COMPLETE,
		SEARCH_FIND,
		SEARCH_FIND_NEXT,
		SEARCH_FIND_PREV,
		SEARCH_REPLACE,
		SEARCH_GOTO_LINE,
		BOOKMARK_TOGGLE,
		BOOKMARK_GOTO_NEXT,
		BOOKMARK_GOTO_PREV,
		BOOKMARK_REMOVE_ALL,
		HELP_DOCS,
	};

	MenuButton *edit_menu = nullptr;
	MenuButton *search_menu = nullptr;
	PopupMenu *bookmarks_menu = nullptr;
	MenuButton *help_menu = nullptr;
	PopupMenu *context_menu = nullptr;
	RichTextLabel *warnings_panel = nullptr;
	uint64_t idle = 0;

	GotoLinePopup *goto_line_popup = nullptr;
	ConfirmationDialog *erase_tab_confirm = nullptr;
	ConfirmationDialog *disk_changed = nullptr;

	ShaderTextEditor *code_editor = nullptr;
	bool compilation_success = true;

	void _menu_option(int p_option);
	void _prepare_edit_menu();
	mutable Ref<Shader> shader;
	mutable Ref<ShaderInclude> shader_inc;

	void _editor_settings_changed();
	void _apply_editor_settings();
	void _project_settings_changed();

	void _check_for_external_edit();
	void _reload_shader_from_disk();
	void _reload_shader_include_from_disk();
	void _reload();
	void _show_warnings_panel(bool p_show);
	void _warning_clicked(const Variant &p_line);
	void _update_warnings(bool p_validate);

	void _script_validated(bool p_valid) {
		compilation_success = p_valid;
		emit_signal(SNAME("validation_changed"));
	}

	uint32_t dependencies_version = 0xFFFFFFFF;

	bool trim_trailing_whitespace_on_save;
	bool trim_final_newlines_on_save;

protected:
	void _notification(int p_what);
	static void _bind_methods();
	void _make_context_menu(bool p_selection, Vector2 p_position);
	void _text_edit_gui_input(const Ref<InputEvent> &p_ev);

	void _update_bookmark_list();
	void _bookmark_item_pressed(int p_idx);

public:
	virtual void edit_shader(const Ref<Shader> &p_shader) override;
	virtual void edit_shader_include(const Ref<ShaderInclude> &p_shader_inc) override;

	virtual void apply_shaders() override;
	virtual bool is_unsaved() const override;
	virtual void save_external_data(const String &p_str = "") override;
	virtual void validate_script() override;

	bool was_compilation_successful() const { return compilation_success; }
	bool get_trim_trailing_whitespace_on_save() const { return trim_trailing_whitespace_on_save; }
	bool get_trim_final_newlines_on_save() const { return trim_final_newlines_on_save; }
	void ensure_select_current();
	void goto_line_selection(int p_line, int p_begin, int p_end);
	void trim_trailing_whitespace();
	void trim_final_newlines();
	void tag_saved_version();
	ShaderTextEditor *get_code_editor() { return code_editor; }

	virtual Size2 get_minimum_size() const override { return Size2(0, 200); }

	TextShaderEditor();
};

#endif // TEXT_SHADER_EDITOR_H
