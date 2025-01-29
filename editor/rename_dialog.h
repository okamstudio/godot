/**************************************************************************/
/*  rename_dialog.h                                                       */
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

#ifndef RENAME_DIALOG_H
#define RENAME_DIALOG_H

#include "editor/gui/scene_tree_editor.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"

class Button;
class CheckBox;
class CheckButton;
class Label;
class OptionButton;
class SpinBox;
class TabContainer;

class RenameDialog : public ConfirmationDialog {
	GDCLASS(RenameDialog, ConfirmationDialog);

	virtual void ok_pressed() override { rename(); }
	void _cancel_pressed() {}
	void _features_toggled(bool pressed);
	void _insert_text(const String &text);
	void _update_substitute();
	bool _is_main_field(LineEdit *line_edit);

	void _iterate_scene(const Node *node, const Array &selection, int *count);
	String _apply_rename(const Node *node, int count);
	String _substitute(const String &subject, const Node *node, int count);
	String _regex(const String &pattern, const String &subject, const String &replacement);
	String _postprocess(const String &subject);
	void _update_preview(const String &new_text = "");
	void _update_preview_int(int new_value = 0);
	static void _error_handler(void *p_self, const char *p_func, const char *p_file, int p_line, const char *p_error, const char *p_errorexp, bool p_editor_notify, ErrorHandlerType p_type);

	SceneTreeEditor *scene_tree_editor = nullptr;
	int global_count = 0;

	LineEdit *lne_search = nullptr;
	LineEdit *lne_replace = nullptr;
	LineEdit *lne_prefix = nullptr;
	LineEdit *lne_suffix = nullptr;

	TabContainer *tabc_features = nullptr;

	CheckBox *cbut_substitute = nullptr;
	CheckButton *cbut_regex = nullptr;
	CheckBox *cbut_process = nullptr;
	CheckBox *chk_per_level_counter = nullptr;

	Button *but_insert_name = nullptr;
	Button *but_insert_parent = nullptr;
	Button *but_insert_type = nullptr;
	Button *but_insert_scene = nullptr;
	Button *but_insert_root = nullptr;
	Button *but_insert_count = nullptr;

	SpinBox *spn_count_start = nullptr;
	SpinBox *spn_count_step = nullptr;
	SpinBox *spn_count_padding = nullptr;

	OptionButton *opt_style = nullptr;
	OptionButton *opt_case = nullptr;

	Label *lbl_preview_title = nullptr;
	Label *lbl_preview = nullptr;

	List<Pair<NodePath, String>> to_rename;
	Node *preview_node = nullptr;
	bool lock_preview_update = false;
	ErrorHandlerList eh;
	bool has_errors = false;

protected:
	static void _bind_methods();
	virtual void _post_popup() override;

public:
	void reset();
	void rename();

	RenameDialog(SceneTreeEditor *p_scene_tree_editor);
};

#endif // RENAME_DIALOG_H
