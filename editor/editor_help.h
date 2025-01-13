/**************************************************************************/
/*  editor_help.h                                                         */
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

#ifndef EDITOR_HELP_H
#define EDITOR_HELP_H

#include "core/os/thread.h"
#include "editor/code_editor.h"
#include "editor/doc_tools.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/text_edit.h"
#include "scene/main/timer.h"

#include "modules/modules_enabled.gen.h" // For gdscript, mono.

class FindBar : public HBoxContainer {
	GDCLASS(FindBar, HBoxContainer);

	LineEdit *search_text = nullptr;
	Button *find_prev = nullptr;
	Button *find_next = nullptr;
	Label *matches_label = nullptr;
	TextureButton *hide_button = nullptr;
	String prev_search;

	RichTextLabel *rich_text_label = nullptr;

	int results_count = 0;

	void _hide_bar();

	void _search_text_changed(const String &p_text);
	void _search_text_submitted(const String &p_text);

	void _update_results_count();
	void _update_matches_label();

protected:
	void _notification(int p_what);
	virtual void unhandled_input(const Ref<InputEvent> &p_event) override;

	bool _search(bool p_search_previous = false);

public:
	void set_rich_text_label(RichTextLabel *p_rich_text_label);

	void popup_search();

	bool search_prev();
	bool search_next();

	FindBar();
};

class EditorHelp : public VBoxContainer {
	GDCLASS(EditorHelp, VBoxContainer);

	enum MethodType {
		METHOD_TYPE_METHOD,
		METHOD_TYPE_CONSTRUCTOR,
		METHOD_TYPE_OPERATOR,
		METHOD_TYPE_MAX
	};

	bool select_locked = false;

	String prev_search;

	String edited_class;

	Vector<Pair<String, int>> section_line;
	HashMap<String, int> method_line;
	HashMap<String, int> signal_line;
	HashMap<String, int> property_line;
	HashMap<String, int> theme_property_line;
	HashMap<String, int> constant_line;
	HashMap<String, int> annotation_line;
	HashMap<String, int> enum_line;
	HashMap<String, HashMap<String, int>> enum_values_line;
	int description_line = 0;

	RichTextLabel *class_desc = nullptr;
	HSplitContainer *h_split = nullptr;
	static DocTools *doc;
	static DocTools *ext_doc;

	ConfirmationDialog *search_dialog = nullptr;
	LineEdit *search = nullptr;
	FindBar *find_bar = nullptr;
	HBoxContainer *status_bar = nullptr;
	Button *toggle_scripts_button = nullptr;

	String base_path;

	struct ThemeCache {
		Ref<StyleBox> background_style;

		Color text_color;
		Color title_color;
		Color headline_color;
		Color comment_color;
		Color symbol_color;
		Color value_color;
		Color qualifier_color;
		Color type_color;
		Color override_color;

		Ref<Font> doc_font;
		Ref<Font> doc_bold_font;
		Ref<Font> doc_italic_font;
		Ref<Font> doc_title_font;
		Ref<Font> doc_code_font;
		Ref<Font> doc_kbd_font;

		int doc_font_size = 0;
		int doc_title_font_size = 0;
		int doc_code_font_size = 0;
		int doc_kbd_font_size = 0;
	} theme_cache;

	int scroll_to = -1;

	void _help_callback(const String &p_topic);

	void _add_text(const String &p_bbcode);
	bool scroll_locked = false;

	//void _button_pressed(int p_idx);
	void _add_type(const String &p_type, const String &p_enum = String(), bool p_is_bitfield = false);
	void _add_type_icon(const String &p_type, int p_size = 0, const String &p_fallback = "");
	void _add_method(const DocData::MethodDoc &p_method, bool p_overview, bool p_override = true);

	void _add_bulletpoint();

	void _push_normal_font();
	void _pop_normal_font();
	void _push_title_font();
	void _pop_title_font();
	void _push_code_font();
	void _pop_code_font();

	void _class_desc_finished();
	void _class_list_select(const String &p_select);
	void _class_desc_select(const String &p_select);
	void _class_desc_input(const Ref<InputEvent> &p_input);
	void _class_desc_resized(bool p_force_update_theme);
	int display_margin = 0;

	Error _goto_desc(const String &p_class);
	//void _update_history_buttons();
	void _update_method_list(MethodType p_method_type, const Vector<DocData::MethodDoc> &p_methods);
	void _update_method_descriptions(const DocData::ClassDoc &p_classdoc, MethodType p_method_type, const Vector<DocData::MethodDoc> &p_methods);
	void _update_doc();

	void _request_help(const String &p_string);
	void _search(bool p_search_previous = false);

	String _fix_constant(const String &p_constant) const;
	void _toggle_scripts_pressed();

	static int doc_generation_count;
	static String doc_version_hash;
	static Thread worker_thread;

	static void _wait_for_thread();
	static void _load_doc_thread(void *p_udata);
	static void _gen_doc_thread(void *p_udata);
	static void _gen_extensions_docs();
	static void _compute_doc_version_hash();

	struct PropertyCompare {
		_FORCE_INLINE_ bool operator()(const DocData::PropertyDoc &p_l, const DocData::PropertyDoc &p_r) const {
			// Sort overridden properties above all else.
			if (p_l.overridden == p_r.overridden) {
				return p_l.name.naturalcasecmp_to(p_r.name) < 0;
			}
			return p_l.overridden;
		}
	};

protected:
	virtual void _update_theme_item_cache() override;

	void _notification(int p_what);
	static void _bind_methods();

public:
	static void generate_doc(bool p_use_cache = true);
	static DocTools *get_doc_data();
	static void cleanup_doc();
	static String get_cache_full_path();

	static void load_xml_buffer(const uint8_t *p_buffer, int p_size);
	static void remove_class(const String &p_class);

	void go_to_help(const String &p_help);
	void go_to_class(const String &p_class);
	void update_doc();

	Vector<Pair<String, int>> get_sections();
	void scroll_to_section(int p_section_index);

	void popup_search();
	void search_again(bool p_search_previous = false);

	String get_class();

	void set_focused() { class_desc->grab_focus(); }

	int get_scroll() const;
	void set_scroll(int p_scroll);

	void update_toggle_scripts_button();

	static void init_gdext_pointers();

	EditorHelp();
	~EditorHelp();
};

class EditorHelpBit : public VBoxContainer {
	GDCLASS(EditorHelpBit, VBoxContainer);

	struct DocType {
		String type;
		String enumeration;
		bool is_bitfield = false;
	};

	struct ArgumentData {
		String name;
		DocType doc_type;
		String default_value;
	};

	struct HelpData {
		String description;
		String deprecated_message;
		String experimental_message;
		DocType doc_type; // For method return type.
		Vector<ArgumentData> arguments; // For methods and signals.
	};

	inline static HashMap<StringName, HelpData> doc_class_cache;
	inline static HashMap<StringName, HashMap<StringName, HelpData>> doc_property_cache;
	inline static HashMap<StringName, HashMap<StringName, HelpData>> doc_method_cache;
	inline static HashMap<StringName, HashMap<StringName, HelpData>> doc_signal_cache;
	inline static HashMap<StringName, HashMap<StringName, HelpData>> doc_theme_item_cache;

	RichTextLabel *title = nullptr;
	RichTextLabel *content = nullptr;

	String symbol_class_name;
	String symbol_type;
	String symbol_visible_type;
	String symbol_name;

	HelpData help_data;

	float content_min_height = 0.0;
	float content_max_height = 0.0;

	static HelpData _get_class_help_data(const StringName &p_class_name);
	static HelpData _get_property_help_data(const StringName &p_class_name, const StringName &p_property_name);
	static HelpData _get_method_help_data(const StringName &p_class_name, const StringName &p_method_name);
	static HelpData _get_signal_help_data(const StringName &p_class_name, const StringName &p_signal_name);
	static HelpData _get_theme_item_help_data(const StringName &p_class_name, const StringName &p_theme_item_name);

	void _add_type_to_title(const DocType &p_doc_type);
	void _update_labels();
	void _go_to_help(const String &p_what);
	void _meta_clicked(const String &p_select);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void parse_symbol(const String &p_symbol, const String &p_prologue = String());
	void set_custom_text(const String &p_type, const String &p_name, const String &p_description);

	void set_content_height_limits(float p_min, float p_max);
	void update_content_height();

	EditorHelpBit(const String &p_symbol = String(), const String &p_prologue = String(), bool p_allow_selection = true);
};

// Standard tooltips do not allow you to hover over them.
// This class is intended as a temporary workaround.
class EditorHelpBitTooltip : public PopupPanel {
	GDCLASS(EditorHelpBitTooltip, PopupPanel);

	static bool _is_tooltip_visible;

	Timer *timer = nullptr;
	uint64_t _enter_tree_time = 0;
	bool _is_mouse_inside_tooltip = false;

	static Control *_make_invisible_control();

	void _start_timer();
	void _target_gui_input(const Ref<InputEvent> &p_event);

protected:
	void _notification(int p_what);

public:
	static Control *show_tooltip(Control *p_target, const String &p_symbol, const String &p_prologue = String());

	void popup_under_cursor();

	EditorHelpBitTooltip(Control *p_target);
};

#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
class EditorSyntaxHighlighter;

class EditorHelpHighlighter {
public:
	enum Language {
		LANGUAGE_GDSCRIPT,
		LANGUAGE_CSHARP,
		LANGUAGE_MAX,
	};

private:
	using HighlightData = Vector<Pair<int, Color>>;

	static EditorHelpHighlighter *singleton;

	HashMap<String, HighlightData> highlight_data_caches[LANGUAGE_MAX];

	TextEdit *text_edits[LANGUAGE_MAX];
	Ref<Script> scripts[LANGUAGE_MAX];
	Ref<EditorSyntaxHighlighter> highlighters[LANGUAGE_MAX];

	HighlightData _get_highlight_data(Language p_language, const String &p_source, bool p_use_cache);

public:
	static void create_singleton();
	static void free_singleton();
	static EditorHelpHighlighter *get_singleton();

	void highlight(RichTextLabel *p_rich_text_label, Language p_language, const String &p_source, bool p_use_cache);
	void reset_cache();

	EditorHelpHighlighter();
	virtual ~EditorHelpHighlighter();
};
#endif // defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)

#endif // EDITOR_HELP_H
