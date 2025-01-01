/**************************************************************************/
/*  audio_stream_interactive_editor_plugin.h                              */
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

#ifndef AUDIO_STREAM_INTERACTIVE_EDITOR_PLUGIN_H
#define AUDIO_STREAM_INTERACTIVE_EDITOR_PLUGIN_H

#include "editor/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/dialogs.h"

class CheckBox;
class HSplitContainer;
class VSplitContainer;
class Tree;
class TreeItem;
class AudioStreamInteractive;

class AudioStreamInteractiveTransitionEditor : public AcceptDialog {
	GDCLASS(AudioStreamInteractiveTransitionEditor, AcceptDialog);

	AudioStreamInteractive *audio_stream_interactive = nullptr;

	HSplitContainer *split = nullptr;
	Tree *tree = nullptr;

	Vector<TreeItem *> rows;

	CheckBox *transition_enabled = nullptr;
	OptionButton *transition_from = nullptr;
	OptionButton *transition_to = nullptr;
	OptionButton *fade_mode = nullptr;
	SpinBox *fade_beats = nullptr;
	OptionButton *filler_clip = nullptr;
	CheckBox *hold_previous = nullptr;

	bool updating_selection = false;
	int order_counter = 0;
	HashMap<Vector2i, int> selection_order;

	Vector<Vector2i> selected;
	bool updating = false;
	void _cell_selected(TreeItem *p_item, int p_column, bool p_selected);
	void _update_transitions();

	void _update_selection();
	void _edited();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void edit(Object *p_obj);

	AudioStreamInteractiveTransitionEditor();
};

//

class EditorInspectorPluginAudioStreamInteractive : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginAudioStreamInteractive, EditorInspectorPlugin);

	AudioStreamInteractiveTransitionEditor *audio_stream_interactive_transition_editor = nullptr;

	void _edit(Object *p_object);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_end(Object *p_object) override;

	EditorInspectorPluginAudioStreamInteractive();
};

class AudioStreamInteractiveEditorPlugin : public EditorPlugin {
	GDCLASS(AudioStreamInteractiveEditorPlugin, EditorPlugin);

public:
	virtual String get_plugin_name() const override { return "AudioStreamInteractive"; }

	AudioStreamInteractiveEditorPlugin();
};

#endif // AUDIO_STREAM_INTERACTIVE_EDITOR_PLUGIN_H
