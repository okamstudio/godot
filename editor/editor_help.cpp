/**************************************************************************/
/*  editor_help.cpp                                                       */
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

#include "editor_help.h"

#include "core/config/project_settings.h"
#include "core/core_constants.h"
#include "core/extension/gdextension.h"
#include "core/input/input.h"
#include "core/object/script_language.h"
#include "core/os/keyboard.h"
#include "core/string/string_builder.h"
#include "core/version_generated.gen.h"
#include "editor/doc_data_compressed.gen.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_property_name_processor.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/line_edit.h"

#include "modules/modules_enabled.gen.h" // For gdscript, mono.

// For syntax highlighting.
#ifdef MODULE_GDSCRIPT_ENABLED
#include "modules/gdscript/editor/gdscript_highlighter.h"
#include "modules/gdscript/gdscript.h"
#endif

// For syntax highlighting.
#ifdef MODULE_MONO_ENABLED
#include "editor/plugins/script_editor_plugin.h"
#include "modules/mono/csharp_script.h"
#endif

#define CONTRIBUTE_URL vformat("%s/contributing/documentation/updating_the_class_reference.html", VERSION_DOCS_URL)

#ifdef MODULE_MONO_ENABLED
// Sync with the types mentioned in https://docs.godotengine.org/en/stable/tutorials/scripting/c_sharp/c_sharp_differences.html
const Vector<String> classes_with_csharp_differences = {
	"@GlobalScope",
	"String",
	"NodePath",
	"Signal",
	"Callable",
	"RID",
	"Basis",
	"Transform2D",
	"Transform3D",
	"Rect2",
	"Rect2i",
	"AABB",
	"Quaternion",
	"Projection",
	"Color",
	"Array",
	"Dictionary",
	"PackedByteArray",
	"PackedColorArray",
	"PackedFloat32Array",
	"PackedFloat64Array",
	"PackedInt32Array",
	"PackedInt64Array",
	"PackedStringArray",
	"PackedVector2Array",
	"PackedVector3Array",
	"PackedVector4Array",
	"Variant",
};
#endif

const Vector<String> packed_array_types = {
	"PackedByteArray",
	"PackedColorArray",
	"PackedFloat32Array",
	"PackedFloat64Array",
	"PackedInt32Array",
	"PackedInt64Array",
	"PackedStringArray",
	"PackedVector2Array",
	"PackedVector3Array",
	"PackedVector4Array",
};

// TODO: this is sometimes used directly as doc->something, other times as EditorHelp::get_doc_data(), which is thread-safe.
// Might this be a problem?
DocTools *EditorHelp::doc = nullptr;
DocTools *EditorHelp::ext_doc = nullptr;

static bool _attempt_doc_load(const String &p_class) {
	// Docgen always happens in the outer-most class: it also generates docs for inner classes.
	String outer_class = p_class.get_slice(".", 0);
	if (!ScriptServer::is_global_class(outer_class)) {
		return false;
	}

	// ResourceLoader is used in order to have a script-agnostic way to load scripts.
	// This forces GDScript to compile the code, which is unnecessary for docgen, but it's a good compromise right now.
	Ref<Script> script = ResourceLoader::load(ScriptServer::get_global_class_path(outer_class), outer_class);
	if (script.is_valid()) {
		Vector<DocData::ClassDoc> docs = script->get_documentation();
		for (int j = 0; j < docs.size(); j++) {
			const DocData::ClassDoc &doc = docs.get(j);
			EditorHelp::get_doc_data()->add_doc(doc);
		}
		return true;
	}

	return false;
}

// Removes unnecessary prefix from p_class_specifier when within the p_edited_class context
static String _contextualize_class_specifier(const String &p_class_specifier, const String &p_edited_class) {
	// If this is a completely different context than the current class, then keep full path
	if (!p_class_specifier.begins_with(p_edited_class)) {
		return p_class_specifier;
	}

	// Here equal length + begins_with from above implies p_class_specifier == p_edited_class :)
	if (p_class_specifier.length() == p_edited_class.length()) {
		int rfind = p_class_specifier.rfind_char('.');
		if (rfind == -1) { // Single identifier
			return p_class_specifier;
		}
		// Multiple specifiers: keep last one only
		return p_class_specifier.substr(rfind + 1);
	}

	// They share a _name_ prefix but not a _class specifier_ prefix, e.g. Tree & TreeItem
	// begins_with + lengths being different implies p_class_specifier.length() > p_edited_class.length() so this is safe
	if (p_class_specifier[p_edited_class.length()] != '.') {
		return p_class_specifier;
	}

	// Remove class specifier prefix
	return p_class_specifier.substr(p_edited_class.length() + 1);
}

void EditorHelp::_update_theme_item_cache() {
	VBoxContainer::_update_theme_item_cache();

	theme_cache.text_color = get_theme_color(SNAME("text_color"), SNAME("EditorHelp"));
	theme_cache.title_color = get_theme_color(SNAME("title_color"), SNAME("EditorHelp"));
	theme_cache.headline_color = get_theme_color(SNAME("headline_color"), SNAME("EditorHelp"));
	theme_cache.comment_color = get_theme_color(SNAME("comment_color"), SNAME("EditorHelp"));
	theme_cache.symbol_color = get_theme_color(SNAME("symbol_color"), SNAME("EditorHelp"));
	theme_cache.value_color = get_theme_color(SNAME("value_color"), SNAME("EditorHelp"));
	theme_cache.qualifier_color = get_theme_color(SNAME("qualifier_color"), SNAME("EditorHelp"));
	theme_cache.type_color = get_theme_color(SNAME("type_color"), SNAME("EditorHelp"));
	theme_cache.override_color = get_theme_color(SNAME("override_color"), SNAME("EditorHelp"));

	theme_cache.doc_font = get_theme_font(SNAME("doc"), EditorStringName(EditorFonts));
	theme_cache.doc_bold_font = get_theme_font(SNAME("doc_bold"), EditorStringName(EditorFonts));
	theme_cache.doc_italic_font = get_theme_font(SNAME("doc_italic"), EditorStringName(EditorFonts));
	theme_cache.doc_title_font = get_theme_font(SNAME("doc_title"), EditorStringName(EditorFonts));
	theme_cache.doc_code_font = get_theme_font(SNAME("doc_source"), EditorStringName(EditorFonts));
	theme_cache.doc_kbd_font = get_theme_font(SNAME("doc_keyboard"), EditorStringName(EditorFonts));

	theme_cache.doc_font_size = get_theme_font_size(SNAME("doc_size"), EditorStringName(EditorFonts));
	theme_cache.doc_title_font_size = get_theme_font_size(SNAME("doc_title_size"), EditorStringName(EditorFonts));
	theme_cache.doc_code_font_size = get_theme_font_size(SNAME("doc_source_size"), EditorStringName(EditorFonts));
	theme_cache.doc_kbd_font_size = get_theme_font_size(SNAME("doc_keyboard_size"), EditorStringName(EditorFonts));

	theme_cache.background_style = get_theme_stylebox(SNAME("background"), SNAME("EditorHelp"));

	class_desc->begin_bulk_theme_override();
	class_desc->add_theme_font_override("normal_font", theme_cache.doc_font);
	class_desc->add_theme_font_size_override("normal_font_size", theme_cache.doc_font_size);

	class_desc->add_theme_constant_override(SceneStringName(line_separation), get_theme_constant(SceneStringName(line_separation), SNAME("EditorHelp")));
	class_desc->add_theme_constant_override("table_h_separation", get_theme_constant(SNAME("table_h_separation"), SNAME("EditorHelp")));
	class_desc->add_theme_constant_override("table_v_separation", get_theme_constant(SNAME("table_v_separation"), SNAME("EditorHelp")));
	class_desc->add_theme_constant_override("text_highlight_h_padding", get_theme_constant(SNAME("text_highlight_h_padding"), SNAME("EditorHelp")));
	class_desc->add_theme_constant_override("text_highlight_v_padding", get_theme_constant(SNAME("text_highlight_v_padding"), SNAME("EditorHelp")));
	class_desc->end_bulk_theme_override();
}

void EditorHelp::_search(bool p_search_previous) {
	if (p_search_previous) {
		find_bar->search_prev();
	} else {
		find_bar->search_next();
	}
}

void EditorHelp::_class_desc_finished() {
	if (scroll_to >= 0) {
		class_desc->scroll_to_paragraph(scroll_to);
	}
	scroll_to = -1;
}

void EditorHelp::_class_list_select(const String &p_select) {
	_goto_desc(p_select);
}

void EditorHelp::_class_desc_select(const String &p_select) {
	if (p_select.begins_with("$")) { // Enum.
		const String link = p_select.substr(1);

		String enum_class_name;
		String enum_name;
		if (CoreConstants::is_global_enum(link)) {
			enum_class_name = "@GlobalScope";
			enum_name = link;
		} else {
			const int dot_pos = link.rfind_char('.');
			if (dot_pos >= 0) {
				enum_class_name = link.left(dot_pos);
				enum_name = link.substr(dot_pos + 1);
			} else {
				enum_class_name = edited_class;
				enum_name = link;
			}
		}

		emit_signal(SNAME("go_to_help"), "class_enum:" + enum_class_name + ":" + enum_name);
	} else if (p_select.begins_with("#")) { // Class.
		emit_signal(SNAME("go_to_help"), "class_name:" + p_select.substr(1));
	} else if (p_select.begins_with("@")) { // Member.
		const int tag_end = p_select.find_char(' ');
		const String tag = p_select.substr(1, tag_end - 1);
		const String link = p_select.substr(tag_end + 1).lstrip(" ");

		String topic;
		const HashMap<String, int> *table = nullptr;

		if (tag == "method") {
			topic = "class_method";
			table = &method_line;
		} else if (tag == "constructor") {
			topic = "class_method";
			table = &method_line;
		} else if (tag == "operator") {
			topic = "class_method";
			table = &method_line;
		} else if (tag == "member") {
			topic = "class_property";
			table = &property_line;
		} else if (tag == "enum") {
			topic = "class_enum";
			table = &enum_line;
		} else if (tag == "signal") {
			topic = "class_signal";
			table = &signal_line;
		} else if (tag == "constant") {
			topic = "class_constant";
			table = &constant_line;
		} else if (tag == "annotation") {
			topic = "class_annotation";
			table = &annotation_line;
		} else if (tag == "theme_item") {
			topic = "class_theme_item";
			table = &theme_property_line;
		} else {
			return;
		}

		// Case order is important here to correctly handle edge cases like Variant.Type in @GlobalScope.
		if (table->has(link)) {
			// Found in the current page.
			if (class_desc->is_finished()) {
				emit_signal(SNAME("request_save_history"));
				class_desc->scroll_to_paragraph((*table)[link]);
			} else {
				scroll_to = (*table)[link];
			}
		} else {
			// Look for link in @GlobalScope.
			// Note that a link like @GlobalScope.enum_name will not be found in this section, only enum_name will be.
			if (topic == "class_enum") {
				const DocData::ClassDoc &cd = doc->class_list["@GlobalScope"];

				for (const DocData::ConstantDoc &constant : cd.constants) {
					if (constant.enumeration == link) {
						// Found in @GlobalScope.
						emit_signal(SNAME("go_to_help"), topic + ":@GlobalScope:" + link);
						return;
					}
				}
			} else if (topic == "class_constant") {
				const DocData::ClassDoc &cd = doc->class_list["@GlobalScope"];

				for (const DocData::ConstantDoc &constant : cd.constants) {
					if (constant.name == link) {
						// Found in @GlobalScope.
						emit_signal(SNAME("go_to_help"), topic + ":@GlobalScope:" + link);
						return;
					}
				}
			}

			if (link.contains_char('.')) {
				const int class_end = link.find_char('.');
				emit_signal(SNAME("go_to_help"), topic + ":" + link.left(class_end) + ":" + link.substr(class_end + 1));
			}
		}
	} else if (p_select.begins_with("http:") || p_select.begins_with("https:")) {
		OS::get_singleton()->shell_open(p_select);
	} else if (p_select.begins_with("^")) { // Copy button.
		DisplayServer::get_singleton()->clipboard_set(p_select.substr(1));
	}
}

void EditorHelp::_class_desc_input(const Ref<InputEvent> &p_input) {
}

void EditorHelp::_class_desc_resized(bool p_force_update_theme) {
	// Add extra horizontal margins for better readability.
	// The margins increase as the width of the editor help container increases.
	real_t char_width = theme_cache.doc_code_font->get_char_size('x', theme_cache.doc_code_font_size).width;
	const int new_display_margin = MAX(30 * EDSCALE, get_parent_anchorable_rect().size.width - char_width * 120 * EDSCALE) * 0.5;
	if (display_margin != new_display_margin || p_force_update_theme) {
		display_margin = new_display_margin;

		Ref<StyleBox> class_desc_stylebox = theme_cache.background_style->duplicate();
		class_desc_stylebox->set_content_margin(SIDE_LEFT, display_margin);
		class_desc_stylebox->set_content_margin(SIDE_RIGHT, display_margin);
		class_desc->add_theme_style_override(CoreStringName(normal), class_desc_stylebox);
		class_desc->add_theme_style_override("focused", class_desc_stylebox);
	}
}

static void _add_type_to_rt(const String &p_type, const String &p_enum, bool p_is_bitfield, RichTextLabel *p_rt, const Control *p_owner_node, const String &p_class) {
	const Color type_color = p_owner_node->get_theme_color(SNAME("type_color"), SNAME("EditorHelp"));

	if (p_type.is_empty() || p_type == "void") {
		p_rt->push_color(Color(type_color, 0.5));
		p_rt->push_hint(TTR("No return value."));
		p_rt->add_text("void");
		p_rt->pop(); // hint
		p_rt->pop(); // color
		return;
	}

	bool is_enum_type = !p_enum.is_empty();
	bool is_bitfield = p_is_bitfield && is_enum_type;
	bool can_ref = !p_type.contains_char('*') || is_enum_type;

	String link_t = p_type; // For links in metadata
	String display_t; // For display purposes.
	if (is_enum_type) {
		link_t = p_enum; // The link for enums is always the full enum description
		display_t = _contextualize_class_specifier(p_enum, p_class);
	} else {
		display_t = _contextualize_class_specifier(p_type, p_class);
	}

	p_rt->push_color(type_color);
	bool add_typed_container = false;
	if (can_ref) {
		if (link_t.ends_with("[]")) {
			add_typed_container = true;
			link_t = link_t.trim_suffix("[]");
			display_t = display_t.trim_suffix("[]");

			p_rt->push_meta("#Array", RichTextLabel::META_UNDERLINE_ON_HOVER); // class
			p_rt->add_text("Array");
			p_rt->pop(); // meta
			p_rt->add_text("[");
		} else if (link_t.begins_with("Dictionary[")) {
			add_typed_container = true;
			link_t = link_t.trim_prefix("Dictionary[").trim_suffix("]");
			display_t = display_t.trim_prefix("Dictionary[").trim_suffix("]");

			p_rt->push_meta("#Dictionary", RichTextLabel::META_UNDERLINE_ON_HOVER); // class
			p_rt->add_text("Dictionary");
			p_rt->pop(); // meta
			p_rt->add_text("[");
			p_rt->push_meta("#" + link_t.get_slice(", ", 0), RichTextLabel::META_UNDERLINE_ON_HOVER); // class
			p_rt->add_text(_contextualize_class_specifier(display_t.get_slice(", ", 0), p_class));
			p_rt->pop(); // meta
			p_rt->add_text(", ");

			link_t = link_t.get_slice(", ", 1);
			display_t = _contextualize_class_specifier(display_t.get_slice(", ", 1), p_class);
		} else if (is_bitfield) {
			p_rt->push_color(Color(type_color, 0.5));
			p_rt->push_hint(TTR("This value is an integer composed as a bitmask of the following flags."));
			p_rt->add_text("BitField");
			p_rt->pop(); // hint
			p_rt->add_text("[");
			p_rt->pop(); // color
		}

		if (is_enum_type) {
			p_rt->push_meta("$" + link_t, RichTextLabel::META_UNDERLINE_ON_HOVER); // enum
		} else {
			p_rt->push_meta("#" + link_t, RichTextLabel::META_UNDERLINE_ON_HOVER); // class
		}
	}
	p_rt->add_text(display_t);
	if (can_ref) {
		p_rt->pop(); // meta
		if (add_typed_container) {
			p_rt->add_text("]");
		} else if (is_bitfield) {
			p_rt->push_color(Color(type_color, 0.5));
			p_rt->add_text("]");
			p_rt->pop(); // color
		}
	}
	p_rt->pop(); // color
}

void EditorHelp::_add_type(const String &p_type, const String &p_enum, bool p_is_bitfield) {
	_add_type_to_rt(p_type, p_enum, p_is_bitfield, class_desc, this, edited_class);
}

void EditorHelp::_add_type_icon(const String &p_type, int p_size, const String &p_fallback) {
	Ref<Texture2D> icon = EditorNode::get_singleton()->get_class_icon(p_type, p_fallback);
	Vector2i size = Vector2i(icon->get_width(), icon->get_height());
	if (p_size > 0) {
		// Ensures icon scales proportionally on both axes, based on icon height.
		float ratio = p_size / float(size.height);
		size.width *= ratio;
		size.height *= ratio;
	}

	class_desc->add_image(icon, size.width, size.height);
}

String EditorHelp::_fix_constant(const String &p_constant) const {
	if (p_constant.strip_edges() == "4294967295") {
		return "0xFFFFFFFF";
	}

	if (p_constant.strip_edges() == "2147483647") {
		return "0x7FFFFFFF";
	}

	if (p_constant.strip_edges() == "1048575") {
		return "0xFFFFF";
	}

	return p_constant;
}

// Macros for assigning the deprecated/experimental marks to class members in overview.

#define DEPRECATED_DOC_TAG                                                                   \
	class_desc->push_font(theme_cache.doc_bold_font);                                        \
	class_desc->push_color(get_theme_color(SNAME("error_color"), EditorStringName(Editor))); \
	Ref<Texture2D> error_icon = get_editor_theme_icon(SNAME("StatusError"));                 \
	class_desc->add_image(error_icon, error_icon->get_width(), error_icon->get_height());    \
	class_desc->add_text(String::chr(160) + TTR("Deprecated"));                              \
	class_desc->pop();                                                                       \
	class_desc->pop();

#define EXPERIMENTAL_DOC_TAG                                                                    \
	class_desc->push_font(theme_cache.doc_bold_font);                                           \
	class_desc->push_color(get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));  \
	Ref<Texture2D> warning_icon = get_editor_theme_icon(SNAME("NodeWarning"));                  \
	class_desc->add_image(warning_icon, warning_icon->get_width(), warning_icon->get_height()); \
	class_desc->add_text(String::chr(160) + TTR("Experimental"));                               \
	class_desc->pop();                                                                          \
	class_desc->pop();

// Macros for displaying the deprecated/experimental info in class member descriptions.

#define DEPRECATED_DOC_MSG(m_message, m_default_message)                                     \
	Ref<Texture2D> error_icon = get_editor_theme_icon(SNAME("StatusError"));                 \
	class_desc->add_image(error_icon, error_icon->get_width(), error_icon->get_height());    \
	class_desc->add_text(" ");                                                               \
	class_desc->push_color(get_theme_color(SNAME("error_color"), EditorStringName(Editor))); \
	class_desc->push_font(theme_cache.doc_bold_font);                                        \
	class_desc->add_text(TTR("Deprecated:"));                                                \
	class_desc->pop();                                                                       \
	class_desc->pop();                                                                       \
	class_desc->add_text(" ");                                                               \
	if ((m_message).is_empty()) {                                                            \
		class_desc->add_text(m_default_message);                                             \
	} else {                                                                                 \
		_add_text(m_message);                                                                \
	}

#define EXPERIMENTAL_DOC_MSG(m_message, m_default_message)                                      \
	Ref<Texture2D> warning_icon = get_editor_theme_icon(SNAME("NodeWarning"));                  \
	class_desc->add_image(warning_icon, warning_icon->get_width(), warning_icon->get_height()); \
	class_desc->add_text(" ");                                                                  \
	class_desc->push_color(get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));  \
	class_desc->push_font(theme_cache.doc_bold_font);                                           \
	class_desc->add_text(TTR("Experimental:"));                                                 \
	class_desc->pop();                                                                          \
	class_desc->pop();                                                                          \
	class_desc->add_text(" ");                                                                  \
	if ((m_message).is_empty()) {                                                               \
		class_desc->add_text(m_default_message);                                                \
	} else {                                                                                    \
		_add_text(m_message);                                                                   \
	}

void EditorHelp::_add_method(const DocData::MethodDoc &p_method, bool p_overview, bool p_override) {
	if (p_override) {
		method_line[p_method.name] = class_desc->get_paragraph_count() - 2; // Gets overridden if description.
	}

	const bool is_vararg = p_method.qualifiers.contains("vararg");

	if (p_overview) {
		class_desc->push_cell();
		class_desc->push_paragraph(HORIZONTAL_ALIGNMENT_RIGHT, Control::TEXT_DIRECTION_AUTO, "");
	} else {
		_add_bulletpoint();
	}

	_add_type(p_method.return_type, p_method.return_enum, p_method.return_is_bitfield);

	if (p_overview) {
		class_desc->pop(); // paragraph
		class_desc->pop(); // cell
		class_desc->push_cell();
	} else {
		class_desc->add_text(" ");
	}

	const bool is_documented = p_method.is_deprecated || p_method.is_experimental || !p_method.description.strip_edges().is_empty();

	if (p_overview && is_documented) {
		class_desc->push_meta("@method " + p_method.name, RichTextLabel::META_UNDERLINE_ON_HOVER);
	}

	class_desc->push_color(theme_cache.headline_color);
	class_desc->add_text(p_method.name);
	class_desc->pop(); // color

	if (p_overview && is_documented) {
		class_desc->pop(); // meta
	}

	class_desc->push_color(theme_cache.symbol_color);
	class_desc->add_text("(");
	class_desc->pop(); // color

	for (int j = 0; j < p_method.arguments.size(); j++) {
		const DocData::ArgumentDoc &argument = p_method.arguments[j];

		class_desc->push_color(theme_cache.text_color);

		if (j > 0) {
			class_desc->add_text(", ");
		}

		class_desc->add_text(argument.name);
		class_desc->add_text(": ");
		_add_type(argument.type, argument.enumeration, argument.is_bitfield);

		if (!argument.default_value.is_empty()) {
			class_desc->push_color(theme_cache.symbol_color);
			class_desc->add_text(" = ");
			class_desc->pop(); // color

			class_desc->push_color(theme_cache.value_color);
			class_desc->add_text(_fix_constant(argument.default_value));
			class_desc->pop(); // color
		}

		class_desc->pop(); // color
	}

	if (is_vararg) {
		class_desc->push_color(theme_cache.text_color);
		if (!p_method.arguments.is_empty()) {
			class_desc->add_text(", ");
		}
		class_desc->pop(); // color

		class_desc->push_color(theme_cache.symbol_color);
		class_desc->add_text("...");
		class_desc->pop(); // color
	}

	class_desc->push_color(theme_cache.symbol_color);
	class_desc->add_text(")");
	class_desc->pop(); // color
	if (!p_method.qualifiers.is_empty()) {
		class_desc->push_color(theme_cache.qualifier_color);

		PackedStringArray qualifiers = p_method.qualifiers.split_spaces();
		for (const String &qualifier : qualifiers) {
			String hint;
			if (qualifier == "vararg") {
				hint = TTR("This method supports a variable number of arguments.");
			} else if (qualifier == "virtual") {
				hint = TTR("This method is called by the engine.\nIt can be overridden to customize built-in behavior.");
			} else if (qualifier == "const") {
				hint = TTR("This method has no side effects.\nIt does not modify the object in any way.");
			} else if (qualifier == "static") {
				hint = TTR("This method does not need an instance to be called.\nIt can be called directly using the class name.");
			}

			class_desc->add_text(" ");
			if (!hint.is_empty()) {
				class_desc->push_hint(hint);
				class_desc->add_text(qualifier);
				class_desc->pop(); // hint
			} else {
				class_desc->add_text(qualifier);
			}
		}

		class_desc->pop(); // color
	}

	if (p_overview) {
		if (p_method.is_deprecated) {
			class_desc->add_text(" ");
			DEPRECATED_DOC_TAG;
		}

		if (p_method.is_experimental) {
			class_desc->add_text(" ");
			EXPERIMENTAL_DOC_TAG;
		}

		class_desc->pop(); // cell
	}
}

void EditorHelp::_add_bulletpoint() {
	static const char32_t prefix[3] = { 0x25CF /* filled circle */, ' ', 0 };
	class_desc->add_text(String(prefix));
}

void EditorHelp::_push_normal_font() {
	class_desc->push_font(theme_cache.doc_font);
	class_desc->push_font_size(theme_cache.doc_font_size);
}

void EditorHelp::_pop_normal_font() {
	class_desc->pop(); // font_size
	class_desc->pop(); // font
}

void EditorHelp::_push_title_font() {
	class_desc->push_font(theme_cache.doc_title_font);
	class_desc->push_font_size(theme_cache.doc_title_font_size);
	class_desc->push_color(theme_cache.title_color);
}

void EditorHelp::_pop_title_font() {
	class_desc->pop(); // color
	class_desc->pop(); // font_size
	class_desc->pop(); // font
}

void EditorHelp::_push_code_font() {
	class_desc->push_font(theme_cache.doc_code_font);
	class_desc->push_font_size(theme_cache.doc_code_font_size);
}

void EditorHelp::_pop_code_font() {
	class_desc->pop(); // font_size
	class_desc->pop(); // font
}

Error EditorHelp::_goto_desc(const String &p_class) {
	// If class doesn't have docs listed, attempt on-demand docgen
	if (!doc->class_list.has(p_class) && !_attempt_doc_load(p_class)) {
		return ERR_DOES_NOT_EXIST;
	}

	select_locked = true;

	class_desc->show();

	description_line = 0;

	if (p_class == edited_class) {
		return OK; // Already there.
	}

	edited_class = p_class;
	_update_doc();
	return OK;
}

void EditorHelp::_update_method_list(MethodType p_method_type, const Vector<DocData::MethodDoc> &p_methods) {
	class_desc->add_newline();
	class_desc->add_newline();

	static const char *titles_by_type[METHOD_TYPE_MAX] = {
		TTRC("Methods"),
		TTRC("Constructors"),
		TTRC("Operators"),
	};
	const String title = TTRGET(titles_by_type[p_method_type]);

	section_line.push_back(Pair<String, int>(title, class_desc->get_paragraph_count() - 2));
	_push_title_font();
	class_desc->add_text(title);
	_pop_title_font();

	class_desc->add_newline();
	class_desc->add_newline();

	class_desc->push_indent(1);
	_push_code_font();
	class_desc->push_table(2);
	class_desc->set_table_column_expand(1, true);

	bool any_previous = false;
	for (int pass = 0; pass < 2; pass++) {
		Vector<DocData::MethodDoc> m;

		for (const DocData::MethodDoc &method : p_methods) {
			const String &q = method.qualifiers;
			if ((pass == 0 && q.contains("virtual")) || (pass == 1 && !q.contains("virtual"))) {
				m.push_back(method);
			}
		}

		if (any_previous && !m.is_empty()) {
			class_desc->push_cell();
			class_desc->pop(); // cell
			class_desc->push_cell();
			class_desc->pop(); // cell
		}

		String group_prefix;
		for (int i = 0; i < m.size(); i++) {
			const String new_prefix = m[i].name.left(3);
			bool is_new_group = false;

			if (i < m.size() - 1 && new_prefix == m[i + 1].name.left(3) && new_prefix != group_prefix) {
				is_new_group = i > 0;
				group_prefix = new_prefix;
			} else if (!group_prefix.is_empty() && new_prefix != group_prefix) {
				is_new_group = true;
				group_prefix = "";
			}

			if (is_new_group && pass == 1) {
				class_desc->push_cell();
				class_desc->pop(); // cell
				class_desc->push_cell();
				class_desc->pop(); // cell
			}

			// For constructors always point to the first one.
			_add_method(m[i], true, (p_method_type != METHOD_TYPE_CONSTRUCTOR || i == 0));
		}

		any_previous = !m.is_empty();
	}

	class_desc->pop(); // table
	_pop_code_font();
	class_desc->pop(); // indent
}

void EditorHelp::_update_method_descriptions(const DocData::ClassDoc &p_classdoc, MethodType p_method_type, const Vector<DocData::MethodDoc> &p_methods) {
#define HANDLE_DOC(m_string) ((p_classdoc.is_script_doc ? (m_string) : DTR(m_string)).strip_edges())

	class_desc->add_newline();
	class_desc->add_newline();
	class_desc->add_newline();

	static const char *titles_by_type[METHOD_TYPE_MAX] = {
		TTRC("Method Descriptions"),
		TTRC("Constructor Descriptions"),
		TTRC("Operator Descriptions"),
	};
	const String title = TTRGET(titles_by_type[p_method_type]);

	section_line.push_back(Pair<String, int>(title, class_desc->get_paragraph_count() - 2));
	_push_title_font();
	class_desc->add_text(title);
	_pop_title_font();

	String link_color_text = theme_cache.title_color.to_html(false);

	for (int pass = 0; pass < 2; pass++) {
		Vector<DocData::MethodDoc> methods_filtered;

		for (int i = 0; i < p_methods.size(); i++) {
			const String &q = p_methods[i].qualifiers;
			if ((pass == 0 && q.contains("virtual")) || (pass == 1 && !q.contains("virtual"))) {
				methods_filtered.push_back(p_methods[i]);
			}
		}

		for (int i = 0; i < methods_filtered.size(); i++) {
			const DocData::MethodDoc &method = methods_filtered[i];

			class_desc->add_newline();
			class_desc->add_newline();
			class_desc->add_newline();

			_push_code_font();
			// For constructors always point to the first one.
			_add_method(method, false, (p_method_type != METHOD_TYPE_CONSTRUCTOR || i == 0));
			_pop_code_font();

			class_desc->add_newline();
			class_desc->add_newline();

			class_desc->push_indent(1);
			_push_normal_font();
			class_desc->push_color(theme_cache.text_color);

			bool has_prev_text = false;

			if (method.is_deprecated) {
				has_prev_text = true;

				static const char *messages_by_type[METHOD_TYPE_MAX] = {
					TTRC("This method may be changed or removed in future versions."),
					TTRC("This constructor may be changed or removed in future versions."),
					TTRC("This operator may be changed or removed in future versions."),
				};
				DEPRECATED_DOC_MSG(HANDLE_DOC(method.deprecated_message), TTRGET(messages_by_type[p_method_type]));
			}

			if (method.is_experimental) {
				if (has_prev_text) {
					class_desc->add_newline();
					class_desc->add_newline();
				}
				has_prev_text = true;

				static const char *messages_by_type[METHOD_TYPE_MAX] = {
					TTRC("This method may be changed or removed in future versions."),
					TTRC("This constructor may be changed or removed in future versions."),
					TTRC("This operator may be changed or removed in future versions."),
				};
				EXPERIMENTAL_DOC_MSG(HANDLE_DOC(method.experimental_message), TTRGET(messages_by_type[p_method_type]));
			}

			if (!method.errors_returned.is_empty()) {
				if (has_prev_text) {
					class_desc->add_newline();
					class_desc->add_newline();
				}
				has_prev_text = true;

				class_desc->add_text(TTR("Error codes returned:"));
				class_desc->add_newline();
				class_desc->push_list(0, RichTextLabel::LIST_DOTS, false);
				for (int j = 0; j < method.errors_returned.size(); j++) {
					if (j > 0) {
						class_desc->add_newline();
					}

					int val = method.errors_returned[j];
					String text = itos(val);
					for (int k = 0; k < CoreConstants::get_global_constant_count(); k++) {
						if (CoreConstants::get_global_constant_value(k) == val && CoreConstants::get_global_constant_enum(k) == SNAME("Error")) {
							text = CoreConstants::get_global_constant_name(k);
							break;
						}
					}

					class_desc->push_font(theme_cache.doc_bold_font);
					class_desc->add_text(text);
					class_desc->pop(); // font
				}
				class_desc->pop(); // list
			}

			const String descr = HANDLE_DOC(method.description);
			const bool is_documented = method.is_deprecated || method.is_experimental || !descr.is_empty();
			if (!descr.is_empty()) {
				if (has_prev_text) {
					class_desc->add_newline();
					class_desc->add_newline();
				}
				has_prev_text = true;

				_add_text(descr);
			} else if (!is_documented) {
				if (has_prev_text) {
					class_desc->add_newline();
					class_desc->add_newline();
				}
				has_prev_text = true;

				String message;
				if (p_classdoc.is_script_doc) {
					static const char *messages_by_type[METHOD_TYPE_MAX] = {
						TTRC("There is currently no description for this method."),
						TTRC("There is currently no description for this constructor."),
						TTRC("There is currently no description for this operator."),
					};
					message = TTRGET(messages_by_type[p_method_type]);
				} else {
					static const char *messages_by_type[METHOD_TYPE_MAX] = {
						TTRC("There is currently no description for this method. Please help us by [color=$color][url=$url]contributing one[/url][/color]!"),
						TTRC("There is currently no description for this constructor. Please help us by [color=$color][url=$url]contributing one[/url][/color]!"),
						TTRC("There is currently no description for this operator. Please help us by [color=$color][url=$url]contributing one[/url][/color]!"),
					};
					message = TTRGET(messages_by_type[p_method_type]).replace("$url", CONTRIBUTE_URL).replace("$color", link_color_text);
				}

				class_desc->add_image(get_editor_theme_icon(SNAME("Error")));
				class_desc->add_text(" ");
				class_desc->push_color(theme_cache.comment_color);
				class_desc->append_text(message);
				class_desc->pop(); // color
			}

			class_desc->pop(); // color
			_pop_normal_font();
			class_desc->pop(); // indent
		}
	}

#undef HANDLE_DOC
}

void EditorHelp::_update_doc() {
	if (!doc->class_list.has(edited_class)) {
		return;
	}

	scroll_locked = true;

	class_desc->clear();
	method_line.clear();
	section_line.clear();
	section_line.push_back(Pair<String, int>(TTR("Top"), 0));

	String link_color_text = theme_cache.title_color.to_html(false);

	DocData::ClassDoc cd = doc->class_list[edited_class]; // Make a copy, so we can sort without worrying.

#define HANDLE_DOC(m_string) ((cd.is_script_doc ? (m_string) : DTR(m_string)).strip_edges())

	// Class name

	_push_title_font();

	class_desc->add_text(TTR("Class:") + " ");
	_add_type_icon(edited_class, theme_cache.doc_title_font_size, "Object");
	class_desc->add_text(" ");

	class_desc->push_color(theme_cache.headline_color);
	class_desc->add_text(edited_class);
	class_desc->pop(); // color

	_pop_title_font();

	if (cd.is_deprecated) {
		class_desc->add_newline();
		DEPRECATED_DOC_MSG(HANDLE_DOC(cd.deprecated_message), TTR("This class may be changed or removed in future versions."));
	}

	if (cd.is_experimental) {
		class_desc->add_newline();
		EXPERIMENTAL_DOC_MSG(HANDLE_DOC(cd.experimental_message), TTR("This class may be changed or removed in future versions."));
	}

	// Inheritance tree

	const String non_breaking_space = String::chr(160);

	// Ascendents
	if (!cd.inherits.is_empty()) {
		class_desc->add_newline();

		_push_normal_font();
		class_desc->push_color(theme_cache.title_color);

		class_desc->add_text(TTR("Inherits:") + " ");

		String inherits = cd.inherits;
		while (!inherits.is_empty()) {
			_add_type_icon(inherits, theme_cache.doc_font_size, "ArrowRight");
			class_desc->add_text(non_breaking_space); // Otherwise icon borrows hyperlink from _add_type().
			_add_type(inherits);

			inherits = doc->class_list[inherits].inherits;

			if (!inherits.is_empty()) {
				class_desc->add_text(" < ");
			}
		}

		class_desc->pop(); // color
		_pop_normal_font();
	}

	// Descendants
	if ((cd.is_script_doc || ClassDB::class_exists(cd.name)) && doc->inheriting.has(cd.name)) {
		class_desc->add_newline();

		_push_normal_font();
		class_desc->push_color(theme_cache.title_color);
		class_desc->add_text(TTR("Inherited by:") + " ");

		for (RBSet<String, NaturalNoCaseComparator>::Element *itr = doc->inheriting[cd.name].front(); itr; itr = itr->next()) {
			if (itr->prev()) {
				class_desc->add_text(" , ");
			}

			_add_type_icon(itr->get(), theme_cache.doc_font_size, "ArrowRight");
			class_desc->add_text(non_breaking_space); // Otherwise icon borrows hyperlink from _add_type().
			_add_type(itr->get());
		}

		class_desc->pop(); // color
		_pop_normal_font();
	}

	bool has_description = false;

	// Brief description
	const String brief_class_descr = HANDLE_DOC(cd.brief_description);
	if (!brief_class_descr.is_empty()) {
		has_description = true;

		class_desc->add_newline();
		class_desc->add_newline();

		class_desc->push_indent(1);
		class_desc->push_font(theme_cache.doc_bold_font);
		class_desc->push_color(theme_cache.text_color);

		_add_text(brief_class_descr);

		class_desc->pop(); // color
		class_desc->pop(); // font
		class_desc->pop(); // indent
	}

	// Class description
	const String class_descr = HANDLE_DOC(cd.description);
	if (!class_descr.is_empty()) {
		has_description = true;

		class_desc->add_newline();
		class_desc->add_newline();

		section_line.push_back(Pair<String, int>(TTR("Description"), class_desc->get_paragraph_count() - 2));
		description_line = class_desc->get_paragraph_count() - 2;
		_push_title_font();
		class_desc->add_text(TTR("Description"));
		_pop_title_font();

		class_desc->add_newline();
		class_desc->add_newline();

		class_desc->push_indent(1);
		_push_normal_font();
		class_desc->push_color(theme_cache.text_color);

		_add_text(class_descr);

		class_desc->pop(); // color
		_pop_normal_font();
		class_desc->pop(); // indent
	}

	if (!has_description) {
		class_desc->add_newline();
		class_desc->add_newline();

		class_desc->push_indent(1);
		_push_normal_font();

		class_desc->add_image(get_editor_theme_icon(SNAME("Error")));
		class_desc->add_text(" ");

		class_desc->push_color(theme_cache.comment_color);
		if (cd.is_script_doc) {
			class_desc->add_text(TTR("There is currently no description for this class."));
		} else {
			class_desc->append_text(TTR("There is currently no description for this class. Please help us by [color=$color][url=$url]contributing one[/url][/color]!").replace("$url", CONTRIBUTE_URL).replace("$color", link_color_text));
		}
		class_desc->pop(); // color

		_pop_normal_font();
		class_desc->pop(); // indent
	}

#ifdef MODULE_MONO_ENABLED
	if (classes_with_csharp_differences.has(cd.name)) {
		class_desc->add_newline();
		class_desc->add_newline();

		const String &csharp_differences_url = vformat("%s/tutorials/scripting/c_sharp/c_sharp_differences.html", VERSION_DOCS_URL);

		class_desc->push_indent(1);
		_push_normal_font();
		class_desc->push_color(theme_cache.text_color);

		class_desc->append_text("[b]" + TTR("Note:") + "[/b] " + vformat(TTR("There are notable differences when using this API with C#. See [url=%s]C# API differences to GDScript[/url] for more information."), csharp_differences_url));

		class_desc->pop(); // color
		_pop_normal_font();
		class_desc->pop(); // indent
	}
#endif

	// Online tutorials
	if (!cd.tutorials.is_empty()) {
		class_desc->add_newline();
		class_desc->add_newline();

		_push_title_font();
		class_desc->add_text(TTR("Online Tutorials"));
		_pop_title_font();

		class_desc->add_newline();

		class_desc->push_indent(1);
		_push_code_font();
		class_desc->push_color(theme_cache.symbol_color);

		for (const DocData::TutorialDoc &tutorial : cd.tutorials) {
			const String link = HANDLE_DOC(tutorial.link);

			String link_text = HANDLE_DOC(tutorial.title);
			if (link_text.is_empty()) {
				const int sep_pos = link.find("//");
				if (sep_pos >= 0) {
					link_text = link.substr(sep_pos + 2);
				} else {
					link_text = link;
				}
			}

			class_desc->add_newline();
			_add_bulletpoint();
			class_desc->append_text("[url=" + link + "]" + link_text + "[/url]");
		}

		class_desc->pop(); // color
		_pop_code_font();
		class_desc->pop(); // indent
	}

	// Properties overview
	HashSet<String> skip_methods;

	bool has_properties = false;
	bool has_property_descriptions = false;
	for (const DocData::PropertyDoc &prop : cd.properties) {
		const bool is_documented = prop.is_deprecated || prop.is_experimental || !prop.description.strip_edges().is_empty();
		if (!is_documented && prop.name.begins_with("_")) {
			continue;
		}
		has_properties = true;
		if (!prop.overridden) {
			has_property_descriptions = true;
			break;
		}
	}

	if (has_properties) {
		class_desc->add_newline();
		class_desc->add_newline();

		section_line.push_back(Pair<String, int>(TTR("Properties"), class_desc->get_paragraph_count() - 2));
		_push_title_font();
		class_desc->add_text(TTR("Properties"));
		_pop_title_font();

		class_desc->add_newline();
		class_desc->add_newline();

		class_desc->push_indent(1);
		_push_code_font();
		class_desc->push_table(4);
		class_desc->set_table_column_expand(1, true);

		cd.properties.sort_custom<PropertyCompare>();

		bool is_generating_overridden_properties = true; // Set to false as soon as we encounter a non-overridden property.
		bool overridden_property_exists = false;

		for (const DocData::PropertyDoc &prop : cd.properties) {
			// Ignore undocumented private.
			const bool is_documented = prop.is_deprecated || prop.is_experimental || !prop.description.strip_edges().is_empty();
			if (!is_documented && prop.name.begins_with("_")) {
				continue;
			}

			if (is_generating_overridden_properties && !prop.overridden) {
				is_generating_overridden_properties = false;
				// No need for the extra spacing when there's no overridden property.
				if (overridden_property_exists) {
					class_desc->push_cell();
					class_desc->pop(); // cell
					class_desc->push_cell();
					class_desc->pop(); // cell
					class_desc->push_cell();
					class_desc->pop(); // cell
					class_desc->push_cell();
					class_desc->pop(); // cell
				}
			}

			property_line[prop.name] = class_desc->get_paragraph_count() - 2; // Gets overridden if description.

			// Property type.
			class_desc->push_cell();
			class_desc->push_paragraph(HORIZONTAL_ALIGNMENT_RIGHT, Control::TEXT_DIRECTION_AUTO, "");
			_add_type(prop.type, prop.enumeration, prop.is_bitfield);
			class_desc->pop(); // paragraph
			class_desc->pop(); // cell

			bool describe = false;

			if (!prop.setter.is_empty()) {
				skip_methods.insert(prop.setter);
				describe = true;
			}
			if (!prop.getter.is_empty()) {
				skip_methods.insert(prop.getter);
				describe = true;
			}

			if (is_documented) {
				describe = true;
			}

			if (prop.overridden) {
				describe = false;
			}

			// Property name.
			class_desc->push_cell();
			class_desc->push_color(theme_cache.headline_color);

			if (describe) {
				class_desc->push_meta("@member " + prop.name, RichTextLabel::META_UNDERLINE_ON_HOVER);
			}

			class_desc->add_text(prop.name);

			if (describe) {
				class_desc->pop(); // meta
			}

			class_desc->pop(); // color
			class_desc->pop(); // cell

			// Property value.
			class_desc->push_cell();

			if (!prop.default_value.is_empty()) {
				if (prop.overridden) {
					class_desc->push_color(theme_cache.override_color);
					class_desc->add_text("[");
					const String link = vformat("[url=@member %s.%s]%s[/url]", prop.overrides, prop.name, prop.overrides);
					class_desc->append_text(vformat(TTR("overrides %s:"), link));
					class_desc->add_text(" " + _fix_constant(prop.default_value) + "]");
					class_desc->pop(); // color
					overridden_property_exists = true;
				} else {
					class_desc->push_color(theme_cache.symbol_color);
					class_desc->add_text("[" + TTR("default:") + " ");
					class_desc->pop(); // color

					class_desc->push_color(theme_cache.value_color);
					class_desc->add_text(_fix_constant(prop.default_value));
					class_desc->pop(); // color

					class_desc->push_color(theme_cache.symbol_color);
					class_desc->add_text("]");
					class_desc->pop(); // color
				}
			}

			class_desc->pop(); // cell

			// Property setter/getter and deprecated/experimental marks.
			class_desc->push_cell();

			bool has_prev_text = false;

			if (cd.is_script_doc && (!prop.setter.is_empty() || !prop.getter.is_empty())) {
				has_prev_text = true;

				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text("[" + TTR("property:") + " ");
				class_desc->pop(); // color

				if (!prop.setter.is_empty()) {
					class_desc->push_color(theme_cache.value_color);
					class_desc->add_text("setter");
					class_desc->pop(); // color
				}
				if (!prop.getter.is_empty()) {
					if (!prop.setter.is_empty()) {
						class_desc->push_color(theme_cache.symbol_color);
						class_desc->add_text(", ");
						class_desc->pop(); // color
					}
					class_desc->push_color(theme_cache.value_color);
					class_desc->add_text("getter");
					class_desc->pop(); // color
				}

				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text("]");
				class_desc->pop(); // color
			}

			if (prop.is_deprecated) {
				if (has_prev_text) {
					class_desc->add_text(" ");
				}
				has_prev_text = true;
				DEPRECATED_DOC_TAG;
			}

			if (prop.is_experimental) {
				if (has_prev_text) {
					class_desc->add_text(" ");
				}
				has_prev_text = true;
				EXPERIMENTAL_DOC_TAG;
			}

			class_desc->pop(); // cell
		}

		class_desc->pop(); // table
		_pop_code_font();
		class_desc->pop(); // indent
	}

	// Methods overview
	bool sort_methods = EDITOR_GET("text_editor/help/sort_functions_alphabetically");

	Vector<DocData::MethodDoc> methods;

	for (const DocData::MethodDoc &method : cd.methods) {
		if (skip_methods.has(method.name)) {
			if (method.arguments.is_empty() /* getter */ || (method.arguments.size() == 1 && method.return_type == "void" /* setter */)) {
				continue;
			}
		}
		// Ignore undocumented non virtual private.
		const bool is_documented = method.is_deprecated || method.is_experimental || !method.description.strip_edges().is_empty();
		if (!is_documented && method.name.begins_with("_") && !method.qualifiers.contains("virtual")) {
			continue;
		}
		methods.push_back(method);
	}

	if (!cd.constructors.is_empty()) {
		if (sort_methods) {
			cd.constructors.sort();
		}
		_update_method_list(METHOD_TYPE_CONSTRUCTOR, cd.constructors);
	}

	if (!methods.is_empty()) {
		if (sort_methods) {
			methods.sort();
		}
		_update_method_list(METHOD_TYPE_METHOD, methods);
	}

	if (!cd.operators.is_empty()) {
		if (sort_methods) {
			cd.operators.sort();
		}
		_update_method_list(METHOD_TYPE_OPERATOR, cd.operators);
	}

	// Theme properties
	if (!cd.theme_properties.is_empty()) {
		class_desc->add_newline();
		class_desc->add_newline();

		section_line.push_back(Pair<String, int>(TTR("Theme Properties"), class_desc->get_paragraph_count() - 2));
		_push_title_font();
		class_desc->add_text(TTR("Theme Properties"));
		_pop_title_font();

		String theme_data_type;
		HashMap<String, String> data_type_names;
		data_type_names["color"] = TTR("Colors");
		data_type_names["constant"] = TTR("Constants");
		data_type_names["font"] = TTR("Fonts");
		data_type_names["font_size"] = TTR("Font Sizes");
		data_type_names["icon"] = TTR("Icons");
		data_type_names["style"] = TTR("Styles");

		for (const DocData::ThemeItemDoc &theme_item : cd.theme_properties) {
			if (theme_data_type != theme_item.data_type) {
				theme_data_type = theme_item.data_type;

				class_desc->add_newline();
				class_desc->add_newline();

				class_desc->push_indent(1);
				_push_title_font();

				if (data_type_names.has(theme_data_type)) {
					class_desc->add_text(data_type_names[theme_data_type]);
				} else {
					class_desc->add_text(theme_data_type);
				}

				_pop_title_font();
				class_desc->pop(); // indent
			}

			class_desc->add_newline();
			class_desc->add_newline();

			theme_property_line[theme_item.name] = class_desc->get_paragraph_count() - 2; // Gets overridden if description.

			class_desc->push_indent(1);

			// Theme item header.
			_push_code_font();
			_add_bulletpoint();

			// Theme item object type.
			_add_type(theme_item.type);

			// Theme item name.
			class_desc->push_color(theme_cache.headline_color);
			class_desc->add_text(" ");
			class_desc->add_text(theme_item.name);
			class_desc->pop(); // color

			// Theme item default value.
			if (!theme_item.default_value.is_empty()) {
				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text(" [" + TTR("default:") + " ");
				class_desc->pop(); // color

				class_desc->push_color(theme_cache.value_color);
				class_desc->add_text(_fix_constant(theme_item.default_value));
				class_desc->pop(); // color

				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text("]");
				class_desc->pop(); // color
			}

			_pop_code_font();

			// Theme item description.
			class_desc->push_indent(1);
			_push_normal_font();
			class_desc->push_color(theme_cache.comment_color);

			bool has_prev_text = false;

			if (theme_item.is_deprecated) {
				has_prev_text = true;
				DEPRECATED_DOC_MSG(HANDLE_DOC(theme_item.deprecated_message), TTR("This theme property may be changed or removed in future versions."));
			}

			if (theme_item.is_experimental) {
				if (has_prev_text) {
					class_desc->add_newline();
					class_desc->add_newline();
				}
				has_prev_text = true;
				EXPERIMENTAL_DOC_MSG(HANDLE_DOC(theme_item.experimental_message), TTR("This theme property may be changed or removed in future versions."));
			}

			const String descr = HANDLE_DOC(theme_item.description);
			if (!descr.is_empty()) {
				if (has_prev_text) {
					class_desc->add_newline();
					class_desc->add_newline();
				}
				has_prev_text = true;
				_add_text(descr);
			} else if (!has_prev_text) {
				class_desc->add_image(get_editor_theme_icon(SNAME("Error")));
				class_desc->add_text(" ");
				class_desc->push_color(theme_cache.comment_color);
				if (cd.is_script_doc) {
					class_desc->add_text(TTR("There is currently no description for this theme property."));
				} else {
					class_desc->append_text(TTR("There is currently no description for this theme property. Please help us by [color=$color][url=$url]contributing one[/url][/color]!").replace("$url", CONTRIBUTE_URL).replace("$color", link_color_text));
				}
				class_desc->pop(); // color
			}

			class_desc->pop(); // color
			_pop_normal_font();
			class_desc->pop(); // indent

			class_desc->pop(); // indent
		}
	}

	// Signals
	if (!cd.signals.is_empty()) {
		if (sort_methods) {
			cd.signals.sort();
		}

		class_desc->add_newline();
		class_desc->add_newline();

		section_line.push_back(Pair<String, int>(TTR("Signals"), class_desc->get_paragraph_count() - 2));
		_push_title_font();
		class_desc->add_text(TTR("Signals"));
		_pop_title_font();

		for (const DocData::MethodDoc &signal : cd.signals) {
			class_desc->add_newline();
			class_desc->add_newline();

			signal_line[signal.name] = class_desc->get_paragraph_count() - 2; // Gets overridden if description.

			class_desc->push_indent(1);

			// Signal header.
			_push_code_font();
			_add_bulletpoint();
			class_desc->push_color(theme_cache.headline_color);
			class_desc->add_text(signal.name);
			class_desc->pop(); // color

			class_desc->push_color(theme_cache.symbol_color);
			class_desc->add_text("(");
			class_desc->pop(); // color

			for (int j = 0; j < signal.arguments.size(); j++) {
				const DocData::ArgumentDoc &argument = signal.arguments[j];

				class_desc->push_color(theme_cache.text_color);

				if (j > 0) {
					class_desc->add_text(", ");
				}

				class_desc->add_text(argument.name);
				class_desc->add_text(": ");
				_add_type(argument.type, argument.enumeration, argument.is_bitfield);

				// Signals currently do not support default argument values, neither the core nor GDScript.
				// This code is just for completeness.
				if (!argument.default_value.is_empty()) {
					class_desc->push_color(theme_cache.symbol_color);
					class_desc->add_text(" = ");
					class_desc->pop(); // color

					class_desc->push_color(theme_cache.value_color);
					class_desc->add_text(_fix_constant(argument.default_value));
					class_desc->pop(); // color
				}

				class_desc->pop(); // color
			}

			class_desc->push_color(theme_cache.symbol_color);
			class_desc->add_text(")");
			class_desc->pop(); // color

			_pop_code_font();

			class_desc->add_newline();

			// Signal description.
			class_desc->push_indent(1);
			_push_normal_font();
			class_desc->push_color(theme_cache.comment_color);

			const String descr = HANDLE_DOC(signal.description);
			const bool is_multiline = descr.find_char('\n') > 0;
			bool has_prev_text = false;

			if (signal.is_deprecated) {
				has_prev_text = true;
				DEPRECATED_DOC_MSG(HANDLE_DOC(signal.deprecated_message), TTR("This signal may be changed or removed in future versions."));
			}

			if (signal.is_experimental) {
				if (has_prev_text) {
					class_desc->add_newline();
					if (is_multiline) {
						class_desc->add_newline();
					}
				}
				has_prev_text = true;
				EXPERIMENTAL_DOC_MSG(HANDLE_DOC(signal.experimental_message), TTR("This signal may be changed or removed in future versions."));
			}

			if (!descr.is_empty()) {
				if (has_prev_text) {
					class_desc->add_newline();
					if (is_multiline) {
						class_desc->add_newline();
					}
				}
				has_prev_text = true;
				_add_text(descr);
			} else if (!has_prev_text) {
				class_desc->add_image(get_editor_theme_icon(SNAME("Error")));
				class_desc->add_text(" ");
				class_desc->push_color(theme_cache.comment_color);
				if (cd.is_script_doc) {
					class_desc->add_text(TTR("There is currently no description for this signal."));
				} else {
					class_desc->append_text(TTR("There is currently no description for this signal. Please help us by [color=$color][url=$url]contributing one[/url][/color]!").replace("$url", CONTRIBUTE_URL).replace("$color", link_color_text));
				}
				class_desc->pop(); // color
			}

			class_desc->pop(); // color
			_pop_normal_font();
			class_desc->pop(); // indent

			class_desc->pop(); // indent
		}
	}

	// Constants and enums
	if (!cd.constants.is_empty()) {
		HashMap<String, Vector<DocData::ConstantDoc>> enums;
		Vector<DocData::ConstantDoc> constants;

		for (const DocData::ConstantDoc &constant : cd.constants) {
			if (!constant.enumeration.is_empty()) {
				if (!enums.has(constant.enumeration)) {
					enums[constant.enumeration] = Vector<DocData::ConstantDoc>();
				}

				enums[constant.enumeration].push_back(constant);
			} else {
				// Ignore undocumented private.
				const bool is_documented = constant.is_deprecated || constant.is_experimental || !constant.description.strip_edges().is_empty();
				if (!is_documented && constant.name.begins_with("_")) {
					continue;
				}
				constants.push_back(constant);
			}
		}

		// Enums
		bool has_enums = enums.size() && !cd.is_script_doc;
		if (enums.size() && !has_enums) {
			for (KeyValue<String, DocData::EnumDoc> &E : cd.enums) {
				const bool is_documented = E.value.is_deprecated || E.value.is_experimental || !E.value.description.strip_edges().is_empty();
				if (!is_documented && E.key.begins_with("_")) {
					continue;
				}
				has_enums = true;
				break;
			}
		}
		if (has_enums) {
			class_desc->add_newline();
			class_desc->add_newline();

			section_line.push_back(Pair<String, int>(TTR("Enumerations"), class_desc->get_paragraph_count() - 2));
			_push_title_font();
			class_desc->add_text(TTR("Enumerations"));
			_pop_title_font();

			for (KeyValue<String, Vector<DocData::ConstantDoc>> &E : enums) {
				String key = E.key;
				if ((key.get_slice_count(".") > 1) && (key.get_slice(".", 0) == edited_class)) {
					key = key.get_slice(".", 1);
				}
				if (cd.enums.has(key)) {
					const bool is_documented = cd.enums[key].is_deprecated || cd.enums[key].is_experimental || !cd.enums[key].description.strip_edges().is_empty();
					if (!is_documented && cd.is_script_doc && E.key.begins_with("_")) {
						continue;
					}
				}

				class_desc->add_newline();
				class_desc->add_newline();

				// Enum header.
				_push_code_font();

				enum_line[E.key] = class_desc->get_paragraph_count() - 2;
				class_desc->push_color(theme_cache.title_color);
				if (E.value.size() && E.value[0].is_bitfield) {
					class_desc->add_text("flags ");
				} else {
					class_desc->add_text("enum ");
				}
				class_desc->pop(); // color

				class_desc->push_color(theme_cache.headline_color);
				class_desc->add_text(key);
				class_desc->pop(); // color

				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text(":");
				class_desc->pop(); // color

				_pop_code_font();

				// Enum description.
				if (key != "@unnamed_enums" && cd.enums.has(key)) {
					const String descr = HANDLE_DOC(cd.enums[key].description);
					const bool is_multiline = descr.find_char('\n') > 0;
					if (cd.enums[key].is_deprecated || cd.enums[key].is_experimental || !descr.is_empty()) {
						class_desc->add_newline();

						class_desc->push_indent(1);
						_push_normal_font();
						class_desc->push_color(theme_cache.text_color);

						bool has_prev_text = false;

						if (cd.enums[key].is_deprecated) {
							has_prev_text = true;
							DEPRECATED_DOC_MSG(HANDLE_DOC(cd.enums[key].deprecated_message), TTR("This enumeration may be changed or removed in future versions."));
						}

						if (cd.enums[key].is_experimental) {
							if (has_prev_text) {
								class_desc->add_newline();
								if (is_multiline) {
									class_desc->add_newline();
								}
							}
							has_prev_text = true;
							EXPERIMENTAL_DOC_MSG(HANDLE_DOC(cd.enums[key].experimental_message), TTR("This enumeration may be changed or removed in future versions."));
						}

						if (!descr.is_empty()) {
							if (has_prev_text) {
								class_desc->add_newline();
								if (is_multiline) {
									class_desc->add_newline();
								}
							}
							has_prev_text = true;
							_add_text(descr);
						}

						class_desc->pop(); // color
						_pop_normal_font();
						class_desc->pop(); // indent
					}
				}

				HashMap<String, int> enum_values;
				const int enum_start_line = enum_line[E.key];

				bool prev_is_multiline = true; // Use a large margin for the first item.
				for (const DocData::ConstantDoc &enum_value : E.value) {
					const String descr = HANDLE_DOC(enum_value.description);
					const bool is_multiline = descr.find_char('\n') > 0;

					class_desc->add_newline();
					if (prev_is_multiline || is_multiline) {
						class_desc->add_newline();
					}
					prev_is_multiline = is_multiline;

					if (cd.name == "@GlobalScope") {
						enum_values[enum_value.name] = enum_start_line;
					}

					// Add the enum constant line to the constant_line map so we can locate it as a constant.
					constant_line[enum_value.name] = class_desc->get_paragraph_count() - 2;

					class_desc->push_indent(1);

					// Enum value header.
					_push_code_font();
					_add_bulletpoint();

					class_desc->push_color(theme_cache.headline_color);
					class_desc->add_text(enum_value.name);
					class_desc->pop(); // color

					class_desc->push_color(theme_cache.symbol_color);
					class_desc->add_text(" = ");
					class_desc->pop(); // color

					class_desc->push_color(theme_cache.value_color);
					class_desc->add_text(_fix_constant(enum_value.value));
					class_desc->pop(); // color

					_pop_code_font();

					// Enum value description.
					if (enum_value.is_deprecated || enum_value.is_experimental || !descr.is_empty()) {
						class_desc->add_newline();

						class_desc->push_indent(1);
						_push_normal_font();
						class_desc->push_color(theme_cache.comment_color);

						bool has_prev_text = false;

						if (enum_value.is_deprecated) {
							has_prev_text = true;
							DEPRECATED_DOC_MSG(HANDLE_DOC(enum_value.deprecated_message), TTR("This constant may be changed or removed in future versions."));
						}

						if (enum_value.is_experimental) {
							if (has_prev_text) {
								class_desc->add_newline();
								if (is_multiline) {
									class_desc->add_newline();
								}
							}
							has_prev_text = true;
							EXPERIMENTAL_DOC_MSG(HANDLE_DOC(enum_value.experimental_message), TTR("This constant may be changed or removed in future versions."));
						}

						if (!descr.is_empty()) {
							if (has_prev_text) {
								class_desc->add_newline();
								if (is_multiline) {
									class_desc->add_newline();
								}
							}
							has_prev_text = true;
							_add_text(descr);
						}

						class_desc->pop(); // color
						_pop_normal_font();
						class_desc->pop(); // indent
					}

					class_desc->pop(); // indent
				}

				if (cd.name == "@GlobalScope") {
					enum_values_line[E.key] = enum_values;
				}
			}
		}

		// Constants
		if (!constants.is_empty()) {
			class_desc->add_newline();
			class_desc->add_newline();

			section_line.push_back(Pair<String, int>(TTR("Constants"), class_desc->get_paragraph_count() - 2));
			_push_title_font();
			class_desc->add_text(TTR("Constants"));
			_pop_title_font();

			bool prev_is_multiline = true; // Use a large margin for the first item.
			for (const DocData::ConstantDoc &constant : constants) {
				const String descr = HANDLE_DOC(constant.description);
				const bool is_multiline = descr.find_char('\n') > 0;

				class_desc->add_newline();
				if (prev_is_multiline || is_multiline) {
					class_desc->add_newline();
				}
				prev_is_multiline = is_multiline;

				constant_line[constant.name] = class_desc->get_paragraph_count() - 2;

				class_desc->push_indent(1);

				// Constant header.
				_push_code_font();

				if (constant.value.begins_with("Color(") && constant.value.ends_with(")")) {
					String stripped = constant.value.replace(" ", "").replace("Color(", "").replace(")", "");
					PackedFloat64Array color = stripped.split_floats(",");
					if (color.size() >= 3) {
						class_desc->push_color(Color(color[0], color[1], color[2]));
						_add_bulletpoint();
						class_desc->pop(); // color
					}
				} else {
					_add_bulletpoint();
				}

				class_desc->push_color(theme_cache.headline_color);
				class_desc->add_text(constant.name);
				class_desc->pop(); // color

				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text(" = ");
				class_desc->pop(); // color

				class_desc->push_color(theme_cache.value_color);
				class_desc->add_text(_fix_constant(constant.value));
				class_desc->pop(); // color

				_pop_code_font();

				// Constant description.
				if (constant.is_deprecated || constant.is_experimental || !descr.is_empty()) {
					class_desc->add_newline();

					class_desc->push_indent(1);
					_push_normal_font();
					class_desc->push_color(theme_cache.comment_color);

					bool has_prev_text = false;

					if (constant.is_deprecated) {
						has_prev_text = true;
						DEPRECATED_DOC_MSG(HANDLE_DOC(constant.deprecated_message), TTR("This constant may be changed or removed in future versions."));
					}

					if (constant.is_experimental) {
						if (has_prev_text) {
							class_desc->add_newline();
							if (is_multiline) {
								class_desc->add_newline();
							}
						}
						has_prev_text = true;
						EXPERIMENTAL_DOC_MSG(HANDLE_DOC(constant.experimental_message), TTR("This constant may be changed or removed in future versions."));
					}

					if (!descr.is_empty()) {
						if (has_prev_text) {
							class_desc->add_newline();
							if (is_multiline) {
								class_desc->add_newline();
							}
						}
						has_prev_text = true;
						_add_text(descr);
					}

					class_desc->pop(); // color
					_pop_normal_font();
					class_desc->pop(); // indent
				}

				class_desc->pop(); // indent
			}
		}
	}

	// Annotations
	if (!cd.annotations.is_empty()) {
		if (sort_methods) {
			cd.annotations.sort();
		}

		class_desc->add_newline();
		class_desc->add_newline();

		section_line.push_back(Pair<String, int>(TTR("Annotations"), class_desc->get_paragraph_count() - 2));
		_push_title_font();
		class_desc->add_text(TTR("Annotations"));
		_pop_title_font();

		for (const DocData::MethodDoc &annotation : cd.annotations) {
			class_desc->add_newline();
			class_desc->add_newline();

			annotation_line[annotation.name] = class_desc->get_paragraph_count() - 2; // Gets overridden if description.

			class_desc->push_indent(1);

			// Annotation header.
			_push_code_font();
			_add_bulletpoint();

			class_desc->push_color(theme_cache.headline_color);
			class_desc->add_text(annotation.name);
			class_desc->pop(); // color

			if (!annotation.arguments.is_empty()) {
				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text("(");
				class_desc->pop(); // color

				for (int j = 0; j < annotation.arguments.size(); j++) {
					const DocData::ArgumentDoc &argument = annotation.arguments[j];

					class_desc->push_color(theme_cache.text_color);

					if (j > 0) {
						class_desc->add_text(", ");
					}

					class_desc->add_text(argument.name);
					class_desc->add_text(": ");
					_add_type(argument.type, argument.enumeration, argument.is_bitfield);

					if (!argument.default_value.is_empty()) {
						class_desc->push_color(theme_cache.symbol_color);
						class_desc->add_text(" = ");
						class_desc->pop(); // color

						class_desc->push_color(theme_cache.value_color);
						class_desc->add_text(_fix_constant(argument.default_value));
						class_desc->pop(); // color
					}

					class_desc->pop(); // color
				}

				if (annotation.qualifiers.contains("vararg")) {
					class_desc->push_color(theme_cache.text_color);
					if (!annotation.arguments.is_empty()) {
						class_desc->add_text(", ");
					}
					class_desc->pop(); // color

					class_desc->push_color(theme_cache.symbol_color);
					class_desc->add_text("...");
					class_desc->pop(); // color
				}

				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text(")");
				class_desc->pop(); // color
			}

			if (!annotation.qualifiers.is_empty()) {
				class_desc->push_color(theme_cache.qualifier_color);
				class_desc->add_text(" ");
				class_desc->add_text(annotation.qualifiers);
				class_desc->pop(); // color
			}

			_pop_code_font();

			class_desc->add_newline();

			// Annotation description.
			class_desc->push_indent(1);
			_push_normal_font();
			class_desc->push_color(theme_cache.comment_color);

			const String descr = HANDLE_DOC(annotation.description);
			if (!descr.is_empty()) {
				_add_text(descr);
			} else {
				class_desc->add_image(get_editor_theme_icon(SNAME("Error")));
				class_desc->add_text(" ");
				class_desc->push_color(theme_cache.comment_color);
				if (cd.is_script_doc) {
					class_desc->add_text(TTR("There is currently no description for this annotation."));
				} else {
					class_desc->append_text(TTR("There is currently no description for this annotation. Please help us by [color=$color][url=$url]contributing one[/url][/color]!").replace("$url", CONTRIBUTE_URL).replace("$color", link_color_text));
				}
				class_desc->pop(); // color
			}

			class_desc->pop(); // color
			_pop_normal_font();
			class_desc->pop(); // indent

			class_desc->pop(); // indent
		}
	}

	// Property descriptions
	if (has_property_descriptions) {
		class_desc->add_newline();
		class_desc->add_newline();
		class_desc->add_newline();

		section_line.push_back(Pair<String, int>(TTR("Property Descriptions"), class_desc->get_paragraph_count() - 2));
		_push_title_font();
		class_desc->add_text(TTR("Property Descriptions"));
		_pop_title_font();

		for (const DocData::PropertyDoc &prop : cd.properties) {
			if (prop.overridden) {
				continue;
			}
			// Ignore undocumented private.
			const bool is_documented = prop.is_deprecated || prop.is_experimental || !prop.description.strip_edges().is_empty();
			if (!is_documented && prop.name.begins_with("_")) {
				continue;
			}

			class_desc->add_newline();
			class_desc->add_newline();
			class_desc->add_newline();

			property_line[prop.name] = class_desc->get_paragraph_count() - 2;

			class_desc->push_table(2);
			class_desc->set_table_column_expand(1, true);

			class_desc->push_cell();
			_push_code_font();
			_add_bulletpoint();
			_add_type(prop.type, prop.enumeration, prop.is_bitfield);
			_pop_code_font();
			class_desc->pop(); // cell

			class_desc->push_cell();
			_push_code_font();

			class_desc->push_color(theme_cache.headline_color);
			class_desc->add_text(prop.name);
			class_desc->pop(); // color

			if (!prop.default_value.is_empty()) {
				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text(" [" + TTR("default:") + " ");
				class_desc->pop(); // color

				class_desc->push_color(theme_cache.value_color);
				class_desc->add_text(_fix_constant(prop.default_value));
				class_desc->pop(); // color

				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text("]");
				class_desc->pop(); // color
			}

			if (cd.is_script_doc && (!prop.setter.is_empty() || !prop.getter.is_empty())) {
				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text(" [" + TTR("property:") + " ");
				class_desc->pop(); // color

				if (!prop.setter.is_empty()) {
					class_desc->push_color(theme_cache.value_color);
					class_desc->add_text("setter");
					class_desc->pop(); // color
				}
				if (!prop.getter.is_empty()) {
					if (!prop.setter.is_empty()) {
						class_desc->push_color(theme_cache.symbol_color);
						class_desc->add_text(", ");
						class_desc->pop(); // color
					}
					class_desc->push_color(theme_cache.value_color);
					class_desc->add_text("getter");
					class_desc->pop(); // color
				}

				class_desc->push_color(theme_cache.symbol_color);
				class_desc->add_text("]");
				class_desc->pop(); // color
			}

			_pop_code_font();
			class_desc->pop(); // cell

			// Script doc doesn't have setter, getter.
			if (!cd.is_script_doc) {
				HashMap<String, DocData::MethodDoc> method_map;
				for (int j = 0; j < methods.size(); j++) {
					method_map[methods[j].name] = methods[j];
				}

				if (!prop.setter.is_empty()) {
					class_desc->push_cell();
					class_desc->pop(); // cell

					class_desc->push_cell();
					_push_code_font();
					class_desc->push_color(theme_cache.text_color);

					if (method_map[prop.setter].arguments.size() > 1) {
						// Setters with additional arguments are exposed in the method list, so we link them here for quick access.
						class_desc->push_meta("@method " + prop.setter);
						class_desc->add_text(prop.setter + TTR("(value)"));
						class_desc->pop(); // meta
					} else {
						class_desc->add_text(prop.setter + TTR("(value)"));
					}

					class_desc->pop(); // color
					class_desc->push_color(theme_cache.comment_color);
					class_desc->add_text(" setter");
					class_desc->pop(); // color
					_pop_code_font();
					class_desc->pop(); // cell

					method_line[prop.setter] = property_line[prop.name];
				}

				if (!prop.getter.is_empty()) {
					class_desc->push_cell();
					class_desc->pop(); // cell

					class_desc->push_cell();
					_push_code_font();
					class_desc->push_color(theme_cache.text_color);

					if (!method_map[prop.getter].arguments.is_empty()) {
						// Getters with additional arguments are exposed in the method list, so we link them here for quick access.
						class_desc->push_meta("@method " + prop.getter);
						class_desc->add_text(prop.getter + "()");
						class_desc->pop(); // meta
					} else {
						class_desc->add_text(prop.getter + "()");
					}

					class_desc->pop(); // color
					class_desc->push_color(theme_cache.comment_color);
					class_desc->add_text(" getter");
					class_desc->pop(); // color
					_pop_code_font();
					class_desc->pop(); // cell

					method_line[prop.getter] = property_line[prop.name];
				}
			}

			class_desc->pop(); // table

			class_desc->add_newline();
			class_desc->add_newline();

			class_desc->push_indent(1);
			_push_normal_font();
			class_desc->push_color(theme_cache.text_color);

			bool has_prev_text = false;

			if (prop.is_deprecated) {
				has_prev_text = true;
				DEPRECATED_DOC_MSG(HANDLE_DOC(prop.deprecated_message), TTR("This property may be changed or removed in future versions."));
			}

			if (prop.is_experimental) {
				if (has_prev_text) {
					class_desc->add_newline();
					class_desc->add_newline();
				}
				has_prev_text = true;
				EXPERIMENTAL_DOC_MSG(HANDLE_DOC(prop.experimental_message), TTR("This property may be changed or removed in future versions."));
			}

			const String descr = HANDLE_DOC(prop.description);
			if (!descr.is_empty()) {
				if (has_prev_text) {
					class_desc->add_newline();
					class_desc->add_newline();
				}
				has_prev_text = true;
				_add_text(descr);
			} else if (!has_prev_text) {
				class_desc->add_image(get_editor_theme_icon(SNAME("Error")));
				class_desc->add_text(" ");
				class_desc->push_color(theme_cache.comment_color);
				if (cd.is_script_doc) {
					class_desc->add_text(TTR("There is currently no description for this property."));
				} else {
					class_desc->append_text(TTR("There is currently no description for this property. Please help us by [color=$color][url=$url]contributing one[/url][/color]!").replace("$url", CONTRIBUTE_URL).replace("$color", link_color_text));
				}
				class_desc->pop(); // color
			}

			// Add copy note to built-in properties returning `Packed*Array`.
			if (!cd.is_script_doc && packed_array_types.has(prop.type)) {
				class_desc->add_newline();
				class_desc->add_newline();
				_add_text(vformat(TTR("[b]Note:[/b] The returned array is [i]copied[/i] and any changes to it will not update the original property value. See [%s] for more details."), prop.type));
			}

			class_desc->pop(); // color
			_pop_normal_font();
			class_desc->pop(); // indent
		}
	}

	// Constructor descriptions
	if (!cd.constructors.is_empty()) {
		_update_method_descriptions(cd, METHOD_TYPE_CONSTRUCTOR, cd.constructors);
	}

	// Method descriptions
	if (!methods.is_empty()) {
		_update_method_descriptions(cd, METHOD_TYPE_METHOD, methods);
	}

	// Operator descriptions
	if (!cd.operators.is_empty()) {
		_update_method_descriptions(cd, METHOD_TYPE_OPERATOR, cd.operators);
	}

	// Allow the document to be scrolled slightly below the end.
	class_desc->add_newline();
	class_desc->add_newline();

	// Free the scroll.
	scroll_locked = false;

#undef HANDLE_DOC
}

void EditorHelp::_request_help(const String &p_string) {
	Error err = _goto_desc(p_string);
	if (err == OK) {
		EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_SCRIPT);
	}
}

void EditorHelp::_help_callback(const String &p_topic) {
	String what = p_topic.get_slice(":", 0);
	String clss = p_topic.get_slice(":", 1);
	String name;
	if (p_topic.get_slice_count(":") == 3) {
		name = p_topic.get_slice(":", 2);
	}

	_request_help(clss); // First go to class.

	int line = 0;

	if (what == "class_desc") {
		line = description_line;
	} else if (what == "class_signal") {
		if (signal_line.has(name)) {
			line = signal_line[name];
		}
	} else if (what == "class_method" || what == "class_method_desc") {
		if (method_line.has(name)) {
			line = method_line[name];
		}
	} else if (what == "class_property") {
		if (property_line.has(name)) {
			line = property_line[name];
		}
	} else if (what == "class_enum") {
		if (enum_line.has(name)) {
			line = enum_line[name];
		}
	} else if (what == "class_theme_item") {
		if (theme_property_line.has(name)) {
			line = theme_property_line[name];
		}
	} else if (what == "class_constant") {
		if (constant_line.has(name)) {
			line = constant_line[name];
		}
	} else if (what == "class_annotation") {
		if (annotation_line.has(name)) {
			line = annotation_line[name];
		}
	} else if (what == "class_global") {
		if (constant_line.has(name)) {
			line = constant_line[name];
		} else if (method_line.has(name)) {
			line = method_line[name];
		} else {
			HashMap<String, HashMap<String, int>>::Iterator iter = enum_values_line.begin();
			while (true) {
				if (iter->value.has(name)) {
					line = iter->value[name];
					break;
				} else if (iter == enum_values_line.last()) {
					break;
				} else {
					++iter;
				}
			}
		}
	}

	if (class_desc->is_finished()) {
		class_desc->scroll_to_paragraph(line);
	} else {
		scroll_to = line;
	}
}

static void _add_text_to_rt(const String &p_bbcode, RichTextLabel *p_rt, const Control *p_owner_node, const String &p_class) {
	const DocTools *doc = EditorHelp::get_doc_data();

	bool is_native = false;
	{
		const HashMap<String, DocData::ClassDoc>::ConstIterator E = doc->class_list.find(p_class);
		if (E && !E->value.is_script_doc) {
			is_native = true;
		}
	}

	const bool using_tab_indent = int(EDITOR_GET("text_editor/behavior/indent/type")) == 0;

	const Ref<Font> doc_font = p_owner_node->get_theme_font(SNAME("doc"), EditorStringName(EditorFonts));
	const Ref<Font> doc_bold_font = p_owner_node->get_theme_font(SNAME("doc_bold"), EditorStringName(EditorFonts));
	const Ref<Font> doc_italic_font = p_owner_node->get_theme_font(SNAME("doc_italic"), EditorStringName(EditorFonts));
	const Ref<Font> doc_code_font = p_owner_node->get_theme_font(SNAME("doc_source"), EditorStringName(EditorFonts));
	const Ref<Font> doc_kbd_font = p_owner_node->get_theme_font(SNAME("doc_keyboard"), EditorStringName(EditorFonts));

	const int doc_font_size = p_owner_node->get_theme_font_size(SNAME("doc_size"), EditorStringName(EditorFonts));
	const int doc_code_font_size = p_owner_node->get_theme_font_size(SNAME("doc_source_size"), EditorStringName(EditorFonts));
	const int doc_kbd_font_size = p_owner_node->get_theme_font_size(SNAME("doc_keyboard_size"), EditorStringName(EditorFonts));

	const Color type_color = p_owner_node->get_theme_color(SNAME("type_color"), SNAME("EditorHelp"));
	const Color code_color = p_owner_node->get_theme_color(SNAME("code_color"), SNAME("EditorHelp"));
	const Color kbd_color = p_owner_node->get_theme_color(SNAME("kbd_color"), SNAME("EditorHelp"));
	const Color code_dark_color = Color(code_color, 0.8);

	const Color link_color = p_owner_node->get_theme_color(SNAME("link_color"), SNAME("EditorHelp"));
	const Color link_method_color = p_owner_node->get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
	const Color link_property_color = link_color.lerp(p_owner_node->get_theme_color(SNAME("accent_color"), EditorStringName(Editor)), 0.25);
	const Color link_annotation_color = link_color.lerp(p_owner_node->get_theme_color(SNAME("accent_color"), EditorStringName(Editor)), 0.5);

	const Color code_bg_color = p_owner_node->get_theme_color(SNAME("code_bg_color"), SNAME("EditorHelp"));
	const Color kbd_bg_color = p_owner_node->get_theme_color(SNAME("kbd_bg_color"), SNAME("EditorHelp"));
	const Color param_bg_color = p_owner_node->get_theme_color(SNAME("param_bg_color"), SNAME("EditorHelp"));

	String bbcode = p_bbcode.dedent().replace("\t", "").replace("\r", "").strip_edges();

	// Select the correct code examples.
	switch ((int)EDITOR_GET("text_editor/help/class_reference_examples")) {
		case 0: // GDScript
			bbcode = bbcode.replace("[gdscript", "[codeblock lang=gdscript"); // Tag can have extra arguments.
			bbcode = bbcode.replace("[/gdscript]", "[/codeblock]");

			for (int pos = bbcode.find("[csharp"); pos != -1; pos = bbcode.find("[csharp")) {
				int end_pos = bbcode.find("[/csharp]");
				if (end_pos == -1) {
					WARN_PRINT("Unclosed [csharp] block or parse fail in code (search for tag errors)");
					break;
				}

				bbcode = bbcode.left(pos) + bbcode.substr(end_pos + 9); // 9 is length of "[/csharp]".
				while (bbcode[pos] == '\n') {
					bbcode = bbcode.left(pos) + bbcode.substr(pos + 1);
				}
			}
			break;
		case 1: // C#
			bbcode = bbcode.replace("[csharp", "[codeblock lang=csharp"); // Tag can have extra arguments.
			bbcode = bbcode.replace("[/csharp]", "[/codeblock]");

			for (int pos = bbcode.find("[gdscript"); pos != -1; pos = bbcode.find("[gdscript")) {
				int end_pos = bbcode.find("[/gdscript]");
				if (end_pos == -1) {
					WARN_PRINT("Unclosed [gdscript] block or parse fail in code (search for tag errors)");
					break;
				}

				bbcode = bbcode.left(pos) + bbcode.substr(end_pos + 11); // 11 is length of "[/gdscript]".
				while (bbcode[pos] == '\n') {
					bbcode = bbcode.left(pos) + bbcode.substr(pos + 1);
				}
			}
			break;
		case 2: // GDScript and C#
			bbcode = bbcode.replace("[csharp", "[b]C#:[/b]\n[codeblock lang=csharp"); // Tag can have extra arguments.
			bbcode = bbcode.replace("[gdscript", "[b]GDScript:[/b]\n[codeblock lang=gdscript"); // Tag can have extra arguments.

			bbcode = bbcode.replace("[/csharp]", "[/codeblock]");
			bbcode = bbcode.replace("[/gdscript]", "[/codeblock]");
			break;
	}

	// Remove codeblocks (they would be printed otherwise).
	bbcode = bbcode.replace("[codeblocks]\n", "");
	bbcode = bbcode.replace("\n[/codeblocks]", "");
	bbcode = bbcode.replace("[codeblocks]", "");
	bbcode = bbcode.replace("[/codeblocks]", "");

	// Remove `\n` here because `\n` is replaced by `\n\n` later.
	// Will be compensated when parsing `[/codeblock]`.
	bbcode = bbcode.replace("[/codeblock]\n", "[/codeblock]");

	List<String> tag_stack;

	int pos = 0;
	while (pos < bbcode.length()) {
		int brk_pos = bbcode.find_char('[', pos);

		if (brk_pos < 0) {
			brk_pos = bbcode.length();
		}

		if (brk_pos > pos) {
			p_rt->add_text(bbcode.substr(pos, brk_pos - pos).replace("\n", "\n\n"));
		}

		if (brk_pos == bbcode.length()) {
			break; // Nothing else to add.
		}

		int brk_end = bbcode.find_char(']', brk_pos + 1);

		if (brk_end == -1) {
			p_rt->add_text(bbcode.substr(brk_pos, bbcode.length() - brk_pos).replace("\n", "\n\n"));
			break;
		}

		const String tag = bbcode.substr(brk_pos + 1, brk_end - brk_pos - 1);

		if (tag.begins_with("/")) {
			bool tag_ok = tag_stack.size() && tag_stack.front()->get() == tag.substr(1);

			if (!tag_ok) {
				p_rt->add_text("[");
				pos = brk_pos + 1;
				continue;
			}

			tag_stack.pop_front();
			pos = brk_end + 1;
			if (tag == "/img") {
				// Nothing to do.
			} else if (tag == "/url") {
				p_rt->pop(); // meta
				p_rt->pop(); // color
				p_rt->add_text(" ");
				p_rt->add_image(p_owner_node->get_editor_theme_icon(SNAME("ExternalLink")), 0, doc_font_size, link_color);
			} else {
				p_rt->pop();
			}
		} else if (tag.begins_with("method ") || tag.begins_with("constructor ") || tag.begins_with("operator ") || tag.begins_with("member ") || tag.begins_with("signal ") || tag.begins_with("enum ") || tag.begins_with("constant ") || tag.begins_with("annotation ") || tag.begins_with("theme_item ")) {
			const int tag_end = tag.find_char(' ');
			const String link_tag = tag.left(tag_end);
			const String link_target = tag.substr(tag_end + 1).lstrip(" ");

			Color target_color = link_color;
			RichTextLabel::MetaUnderline underline_mode = RichTextLabel::META_UNDERLINE_ON_HOVER;
			if (link_tag == "method" || link_tag == "constructor" || link_tag == "operator") {
				target_color = link_method_color;
			} else if (link_tag == "member" || link_tag == "signal" || link_tag == "theme_item") {
				target_color = link_property_color;
			} else if (link_tag == "annotation") {
				target_color = link_annotation_color;
			} else {
				// Better visibility for constants, enums, etc.
				underline_mode = RichTextLabel::META_UNDERLINE_ALWAYS;
			}

			// Use monospace font to make clickable references
			// easier to distinguish from inline code and other text.
			p_rt->push_font(doc_code_font);
			p_rt->push_font_size(doc_code_font_size);
			p_rt->push_color(target_color);
			p_rt->push_meta("@" + link_tag + " " + link_target, underline_mode);

			if (link_tag == "member" &&
					((!link_target.contains_char('.') && (p_class == "ProjectSettings" || p_class == "EditorSettings")) ||
							link_target.begins_with("ProjectSettings.") || link_target.begins_with("EditorSettings."))) {
				// Special formatting for both ProjectSettings and EditorSettings.
				String prefix;
				if (link_target.begins_with("EditorSettings.")) {
					prefix = "(" + TTR("Editor") + ") ";
				}

				const String setting_name = link_target.trim_prefix("ProjectSettings.").trim_prefix("EditorSettings.");
				PackedStringArray setting_sections;
				for (const String &section : setting_name.split("/", false)) {
					setting_sections.append(EditorPropertyNameProcessor::get_singleton()->process_name(section, EditorPropertyNameProcessor::get_settings_style()));
				}

				p_rt->push_bold();
				p_rt->add_text(prefix + String(" > ").join(setting_sections));
				p_rt->pop(); // bold
			} else {
				p_rt->add_text(link_target + (link_tag == "method" ? "()" : ""));
			}

			p_rt->pop(); // meta
			p_rt->pop(); // color
			p_rt->pop(); // font_size
			p_rt->pop(); // font

			pos = brk_end + 1;
		} else if (tag.begins_with("param ")) {
			const int tag_end = tag.find_char(' ');
			const String param_name = tag.substr(tag_end + 1).lstrip(" ");

			// Use monospace font with translucent background color to make code easier to distinguish from other text.
			p_rt->push_font(doc_code_font);
			p_rt->push_font_size(doc_code_font_size);
			p_rt->push_bgcolor(param_bg_color);
			p_rt->push_color(code_color);

			p_rt->add_text(param_name);

			p_rt->pop(); // color
			p_rt->pop(); // bgcolor
			p_rt->pop(); // font_size
			p_rt->pop(); // font

			pos = brk_end + 1;
		} else if (tag == p_class) {
			// Use a bold font when class reference tags are in their own page.
			p_rt->push_font(doc_bold_font);
			p_rt->add_text(tag);
			p_rt->pop(); // font

			pos = brk_end + 1;
		} else if (doc->class_list.has(tag)) {
			// Use a monospace font for class reference tags such as [Node2D] or [SceneTree].

			p_rt->push_font(doc_code_font);
			p_rt->push_font_size(doc_code_font_size);
			p_rt->push_color(type_color);
			p_rt->push_meta("#" + tag, RichTextLabel::META_UNDERLINE_ON_HOVER);

			p_rt->add_text(tag);

			p_rt->pop(); // meta
			p_rt->pop(); // color
			p_rt->pop(); // font_size
			p_rt->pop(); // font

			pos = brk_end + 1;
		} else if (tag == "b") {
			// Use bold font.
			p_rt->push_font(doc_bold_font);

			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "i") {
			// Use italics font.
			p_rt->push_font(doc_italic_font);

			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "code" || tag.begins_with("code ")) {
			int end_pos = bbcode.find("[/code]", brk_end + 1);
			if (end_pos < 0) {
				end_pos = bbcode.length();
			}

			// Use monospace font with darkened background color to make code easier to distinguish from other text.
			p_rt->push_font(doc_code_font);
			p_rt->push_font_size(doc_code_font_size);
			p_rt->push_bgcolor(code_bg_color);
			p_rt->push_color(code_color.lerp(p_owner_node->get_theme_color(SNAME("error_color"), EditorStringName(Editor)), 0.6));

			p_rt->add_text(bbcode.substr(brk_end + 1, end_pos - (brk_end + 1)));

			p_rt->pop(); // color
			p_rt->pop(); // bgcolor
			p_rt->pop(); // font_size
			p_rt->pop(); // font

			pos = end_pos + 7; // `len("[/code]")`.
		} else if (tag == "codeblock" || tag.begins_with("codeblock ")) {
			int end_pos = bbcode.find("[/codeblock]", brk_end + 1);
			if (end_pos < 0) {
				end_pos = bbcode.length();
			}

			const String codeblock_text = bbcode.substr(brk_end + 1, end_pos - (brk_end + 1)).strip_edges();

			String codeblock_copy_text = codeblock_text;
			if (using_tab_indent) {
				// Replace the code block's space indentation with tabs.
				StringBuilder builder;
				PackedStringArray text_lines = codeblock_copy_text.split("\n");
				for (const String &line : text_lines) {
					const String stripped_line = line.dedent();
					const int space_count = line.length() - stripped_line.length();

					if (builder.num_strings_appended() > 0) {
						builder.append("\n");
					}
					if (space_count > 0) {
						builder.append(String("\t").repeat(MAX(space_count / 4, 1)) + stripped_line);
					} else {
						builder.append(line);
					}
				}
				codeblock_copy_text = builder.as_string();
			}

			String lang;
			const PackedStringArray args = tag.trim_prefix("codeblock").split(" ", false);
			for (int i = args.size() - 1; i >= 0; i--) {
				if (args[i].begins_with("lang=")) {
					lang = args[i].trim_prefix("lang=");
					break;
				}
			}

			// Use monospace font with darkened background color to make code easier to distinguish from other text.
			// Use a single-column table with cell row background color instead of `[bgcolor]`.
			// This makes the background color highlight cover the entire block, rather than individual lines.
			p_rt->push_font(doc_code_font);
			p_rt->push_font_size(doc_code_font_size);
			p_rt->push_table(2);

			p_rt->push_cell();
			p_rt->set_cell_row_background_color(code_bg_color, Color(code_bg_color, 0.99));
			p_rt->set_cell_padding(Rect2(10 * EDSCALE, 10 * EDSCALE, 10 * EDSCALE, 10 * EDSCALE));
			p_rt->push_color(code_dark_color);

			bool codeblock_printed = false;

#ifdef MODULE_GDSCRIPT_ENABLED
			if (!codeblock_printed && (lang.is_empty() || lang == "gdscript")) {
				EditorHelpHighlighter::get_singleton()->highlight(p_rt, EditorHelpHighlighter::LANGUAGE_GDSCRIPT, codeblock_text, is_native);
				codeblock_printed = true;
			}
#endif

#ifdef MODULE_MONO_ENABLED
			if (!codeblock_printed && lang == "csharp") {
				EditorHelpHighlighter::get_singleton()->highlight(p_rt, EditorHelpHighlighter::LANGUAGE_CSHARP, codeblock_text, is_native);
				codeblock_printed = true;
			}
#endif

			if (!codeblock_printed) {
				p_rt->add_text(codeblock_text);
				codeblock_printed = true;
			}

			p_rt->pop(); // color
			p_rt->pop(); // cell

			// Copy codeblock button.
			p_rt->push_cell();
			p_rt->set_cell_row_background_color(code_bg_color, Color(code_bg_color, 0.99));
			p_rt->set_cell_padding(Rect2(0, 10 * EDSCALE, 0, 10 * EDSCALE));
			p_rt->set_cell_size_override(Vector2(1, 1), Vector2(10, 10) * EDSCALE);
			p_rt->push_meta("^" + codeblock_copy_text, RichTextLabel::META_UNDERLINE_ON_HOVER);
			p_rt->add_image(p_owner_node->get_editor_theme_icon(SNAME("ActionCopy")), 24 * EDSCALE, 24 * EDSCALE, Color(link_property_color, 0.3), INLINE_ALIGNMENT_BOTTOM_TO, Rect2(), Variant(), false, TTR("Click to copy."));
			p_rt->pop(); // meta
			p_rt->pop(); // cell

			p_rt->pop(); // table
			p_rt->pop(); // font_size
			p_rt->pop(); // font

			pos = end_pos + 12; // `len("[/codeblock]")`.

			// Compensate for `\n` removed before the loop.
			if (pos < bbcode.length()) {
				p_rt->add_newline();
			}
		} else if (tag == "kbd") {
			int end_pos = bbcode.find("[/kbd]", brk_end + 1);
			if (end_pos < 0) {
				end_pos = bbcode.length();
			}

			// Use keyboard font with custom color and background color.
			p_rt->push_font(doc_kbd_font);
			p_rt->push_font_size(doc_kbd_font_size);
			p_rt->push_bgcolor(kbd_bg_color);
			p_rt->push_color(kbd_color);

			p_rt->add_text(bbcode.substr(brk_end + 1, end_pos - (brk_end + 1)));

			p_rt->pop(); // color
			p_rt->pop(); // bgcolor
			p_rt->pop(); // font_size
			p_rt->pop(); // font

			pos = end_pos + 6; // `len("[/kbd]")`.
		} else if (tag == "center") {
			// Align to center.
			p_rt->push_paragraph(HORIZONTAL_ALIGNMENT_CENTER, Control::TEXT_DIRECTION_AUTO, "");
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "br") {
			// Force a line break.
			p_rt->add_newline();
			pos = brk_end + 1;
		} else if (tag == "u") {
			// Use underline.
			p_rt->push_underline();
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "s") {
			// Use strikethrough.
			p_rt->push_strikethrough();
			pos = brk_end + 1;
			tag_stack.push_front(tag);
		} else if (tag == "lb") {
			p_rt->add_text("[");
			pos = brk_end + 1;
		} else if (tag == "rb") {
			p_rt->add_text("]");
			pos = brk_end + 1;
		} else if (tag == "url" || tag.begins_with("url=")) {
			String url;
			if (tag.begins_with("url=")) {
				url = tag.substr(4);
			} else {
				int end = bbcode.find_char('[', brk_end);
				if (end == -1) {
					end = bbcode.length();
				}
				url = bbcode.substr(brk_end + 1, end - brk_end - 1);
			}

			p_rt->push_color(link_color);
			p_rt->push_meta(url, RichTextLabel::META_UNDERLINE_ON_HOVER, url + "\n\n" + TTR("Click to open in browser."));

			pos = brk_end + 1;
			tag_stack.push_front("url");
		} else if (tag.begins_with("img")) {
			int width = 0;
			int height = 0;
			bool size_in_percent = false;
			if (tag.length() > 4) {
				Vector<String> subtags = tag.substr(4).split(" ");
				HashMap<String, String> bbcode_options;
				for (int i = 0; i < subtags.size(); i++) {
					const String &expr = subtags[i];
					int value_pos = expr.find_char('=');
					if (value_pos > -1) {
						bbcode_options[expr.left(value_pos)] = expr.substr(value_pos + 1).unquote();
					}
				}
				HashMap<String, String>::Iterator width_option = bbcode_options.find("width");
				if (width_option) {
					width = width_option->value.to_int();
					if (width_option->value.ends_with("%")) {
						size_in_percent = true;
					}
				}

				HashMap<String, String>::Iterator height_option = bbcode_options.find("height");
				if (height_option) {
					height = height_option->value.to_int();
					if (height_option->value.ends_with("%")) {
						size_in_percent = true;
					}
				}
			}
			int end = bbcode.find_char('[', brk_end);
			if (end == -1) {
				end = bbcode.length();
			}

			String image_path = bbcode.substr(brk_end + 1, end - brk_end - 1);
			p_rt->add_image(ResourceLoader::load(image_path, "Texture2D"), width, height, Color(1, 1, 1), INLINE_ALIGNMENT_CENTER, Rect2(), Variant(), false, String(), size_in_percent);

			pos = end;
			tag_stack.push_front("img");
		} else if (tag.begins_with("color=")) {
			String col = tag.substr(6);
			Color color = Color::from_string(col, Color());
			p_rt->push_color(color);

			pos = brk_end + 1;
			tag_stack.push_front("color");
		} else if (tag.begins_with("font=")) {
			String font_path = tag.substr(5);
			Ref<Font> font = ResourceLoader::load(font_path, "Font");
			if (font.is_valid()) {
				p_rt->push_font(font);
			} else {
				p_rt->push_font(doc_font);
			}

			pos = brk_end + 1;
			tag_stack.push_front("font");
		} else {
			p_rt->add_text("["); // Ignore.
			pos = brk_pos + 1;
		}
	}

	// Close unclosed tags.
	for (const String &tag : tag_stack) {
		if (tag != "img") {
			p_rt->pop();
		}
	}
}

void EditorHelp::_add_text(const String &p_bbcode) {
	_add_text_to_rt(p_bbcode, class_desc, this, edited_class);
}

int EditorHelp::doc_generation_count = 0;
String EditorHelp::doc_version_hash;
Thread EditorHelp::worker_thread;

void EditorHelp::_wait_for_thread() {
	if (worker_thread.is_started()) {
		worker_thread.wait_to_finish();
	}
}

void EditorHelp::_compute_doc_version_hash() {
	uint32_t version_hash = Engine::get_singleton()->get_version_info().hash();
	doc_version_hash = vformat("%d/%d/%d/%s", version_hash, ClassDB::get_api_hash(ClassDB::API_CORE), ClassDB::get_api_hash(ClassDB::API_EDITOR), _doc_data_hash);
}

String EditorHelp::get_cache_full_path() {
	return EditorPaths::get_singleton()->get_cache_dir().path_join(vformat("editor_doc_cache-%d.%d.res", VERSION_MAJOR, VERSION_MINOR));
}

void EditorHelp::load_xml_buffer(const uint8_t *p_buffer, int p_size) {
	if (!ext_doc) {
		ext_doc = memnew(DocTools);
	}

	ext_doc->load_xml(p_buffer, p_size);

	if (doc) {
		doc->load_xml(p_buffer, p_size);
	}
}

void EditorHelp::remove_class(const String &p_class) {
	if (ext_doc && ext_doc->has_doc(p_class)) {
		ext_doc->remove_doc(p_class);
	}

	if (doc && doc->has_doc(p_class)) {
		doc->remove_doc(p_class);
	}
}

void EditorHelp::_load_doc_thread(void *p_udata) {
	Ref<Resource> cache_res = ResourceLoader::load(get_cache_full_path());
	if (cache_res.is_valid() && cache_res->get_meta("version_hash", "") == doc_version_hash) {
		Array classes = cache_res->get_meta("classes", Array());
		for (int i = 0; i < classes.size(); i++) {
			doc->add_doc(DocData::ClassDoc::from_dict(classes[i]));
		}

		// Extensions' docs are not cached. Generate them now (on the main thread).
		callable_mp_static(&EditorHelp::_gen_extensions_docs).call_deferred();
	} else {
		// We have to go back to the main thread to start from scratch, bypassing any possibly existing cache.
		callable_mp_static(&EditorHelp::generate_doc).call_deferred(false);
	}

	OS::get_singleton()->benchmark_end_measure("EditorHelp", vformat("Generate Documentation (Run %d)", doc_generation_count));
}

void EditorHelp::_gen_doc_thread(void *p_udata) {
	DocTools compdoc;
	compdoc.load_compressed(_doc_data_compressed, _doc_data_compressed_size, _doc_data_uncompressed_size);
	doc->merge_from(compdoc); // Ensure all is up to date.

	Ref<Resource> cache_res;
	cache_res.instantiate();
	cache_res->set_meta("version_hash", doc_version_hash);
	Array classes;
	for (const KeyValue<String, DocData::ClassDoc> &E : doc->class_list) {
		if (ClassDB::class_exists(E.value.name)) {
			ClassDB::APIType api = ClassDB::get_api_type(E.value.name);
			if (api == ClassDB::API_EXTENSION || api == ClassDB::API_EDITOR_EXTENSION) {
				continue;
			}
		}
		classes.push_back(DocData::ClassDoc::to_dict(E.value));
	}
	cache_res->set_meta("classes", classes);
	Error err = ResourceSaver::save(cache_res, get_cache_full_path(), ResourceSaver::FLAG_COMPRESS);
	if (err) {
		ERR_PRINT("Cannot save editor help cache (" + get_cache_full_path() + ").");
	}

	OS::get_singleton()->benchmark_end_measure("EditorHelp", vformat("Generate Documentation (Run %d)", doc_generation_count));
}

void EditorHelp::_gen_extensions_docs() {
	doc->generate((DocTools::GENERATE_FLAG_SKIP_BASIC_TYPES | DocTools::GENERATE_FLAG_EXTENSION_CLASSES_ONLY));

	// Append extra doc data, as it gets overridden by the generation step.
	if (ext_doc) {
		doc->merge_from(*ext_doc);
	}
}

void EditorHelp::generate_doc(bool p_use_cache) {
	doc_generation_count++;
	OS::get_singleton()->benchmark_begin_measure("EditorHelp", vformat("Generate Documentation (Run %d)", doc_generation_count));

	// In case not the first attempt.
	_wait_for_thread();

	if (!doc) {
		doc = memnew(DocTools);
	}

	if (doc_version_hash.is_empty()) {
		_compute_doc_version_hash();
	}

	if (p_use_cache && FileAccess::exists(get_cache_full_path())) {
		worker_thread.start(_load_doc_thread, nullptr);
	} else {
		print_verbose("Regenerating editor help cache");
		doc->generate();
		worker_thread.start(_gen_doc_thread, nullptr);
	}
}

void EditorHelp::_toggle_scripts_pressed() {
	ScriptEditor::get_singleton()->toggle_scripts_panel();
	update_toggle_scripts_button();
}

void EditorHelp::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			// Requires theme to be up to date.
			_class_desc_resized(false);
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			bool need_update = false;
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("text_editor/help")) {
				need_update = true;
			}
#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
			if (!need_update && EditorSettings::get_singleton()->check_changed_settings_in_group("text_editor/theme/highlighting")) {
				need_update = true;
			}
#endif
			if (!need_update) {
				break;
			}
			[[fallthrough]];
		}
		case NOTIFICATION_READY: {
			_wait_for_thread();
			_update_doc();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			if (is_inside_tree()) {
				_class_desc_resized(true);
			}
			update_toggle_scripts_button();
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			update_toggle_scripts_button();
		} break;
	}
}

void EditorHelp::go_to_help(const String &p_help) {
	_wait_for_thread();
	_help_callback(p_help);
}

void EditorHelp::go_to_class(const String &p_class) {
	_wait_for_thread();
	_goto_desc(p_class);
}

void EditorHelp::update_doc() {
	_wait_for_thread();
	ERR_FAIL_COND(!doc->class_list.has(edited_class));
	ERR_FAIL_COND(!doc->class_list[edited_class].is_script_doc);
	_update_doc();
}

void EditorHelp::cleanup_doc() {
	_wait_for_thread();
	memdelete(doc);
	doc = nullptr;
}

Vector<Pair<String, int>> EditorHelp::get_sections() {
	_wait_for_thread();
	Vector<Pair<String, int>> sections;

	for (int i = 0; i < section_line.size(); i++) {
		sections.push_back(Pair<String, int>(section_line[i].first, i));
	}
	return sections;
}

void EditorHelp::scroll_to_section(int p_section_index) {
	_wait_for_thread();
	int line = section_line[p_section_index].second;
	if (class_desc->is_finished()) {
		class_desc->scroll_to_paragraph(line);
	} else {
		scroll_to = line;
	}
}

void EditorHelp::popup_search() {
	_wait_for_thread();
	find_bar->popup_search();
}

String EditorHelp::get_class() {
	return edited_class;
}

void EditorHelp::search_again(bool p_search_previous) {
	_search(p_search_previous);
}

int EditorHelp::get_scroll() const {
	return class_desc->get_v_scroll_bar()->get_value();
}

void EditorHelp::set_scroll(int p_scroll) {
	class_desc->get_v_scroll_bar()->set_value(p_scroll);
}

void EditorHelp::update_toggle_scripts_button() {
	if (is_layout_rtl()) {
		toggle_scripts_button->set_button_icon(get_editor_theme_icon(ScriptEditor::get_singleton()->is_scripts_panel_toggled() ? SNAME("Forward") : SNAME("Back")));
	} else {
		toggle_scripts_button->set_button_icon(get_editor_theme_icon(ScriptEditor::get_singleton()->is_scripts_panel_toggled() ? SNAME("Back") : SNAME("Forward")));
	}
	toggle_scripts_button->set_tooltip_text(vformat("%s (%s)", TTR("Toggle Scripts Panel"), ED_GET_SHORTCUT("script_editor/toggle_scripts_panel")->get_as_text()));
}

void EditorHelp::_bind_methods() {
	ClassDB::bind_method("_class_list_select", &EditorHelp::_class_list_select);
	ClassDB::bind_method("_request_help", &EditorHelp::_request_help);
	ClassDB::bind_method("_search", &EditorHelp::_search);
	ClassDB::bind_method("_help_callback", &EditorHelp::_help_callback);

	ADD_SIGNAL(MethodInfo("go_to_help"));
	ADD_SIGNAL(MethodInfo("request_save_history"));
}

void EditorHelp::init_gdext_pointers() {
	GDExtensionEditorHelp::editor_help_load_xml_buffer = &EditorHelp::load_xml_buffer;
	GDExtensionEditorHelp::editor_help_remove_class = &EditorHelp::remove_class;
}

EditorHelp::EditorHelp() {
	set_custom_minimum_size(Size2(150 * EDSCALE, 0));

	class_desc = memnew(RichTextLabel);
	class_desc->set_tab_size(8);
	add_child(class_desc);
	class_desc->set_threaded(true);
	class_desc->set_v_size_flags(SIZE_EXPAND_FILL);

	class_desc->connect(SceneStringName(finished), callable_mp(this, &EditorHelp::_class_desc_finished));
	class_desc->connect("meta_clicked", callable_mp(this, &EditorHelp::_class_desc_select));
	class_desc->connect(SceneStringName(gui_input), callable_mp(this, &EditorHelp::_class_desc_input));
	class_desc->connect(SceneStringName(resized), callable_mp(this, &EditorHelp::_class_desc_resized).bind(false));

	// Added second so it opens at the bottom so it won't offset the entire widget.
	find_bar = memnew(FindBar);
	add_child(find_bar);
	find_bar->hide();
	find_bar->set_rich_text_label(class_desc);

	status_bar = memnew(HBoxContainer);
	add_child(status_bar);
	status_bar->set_h_size_flags(SIZE_EXPAND_FILL);
	status_bar->set_custom_minimum_size(Size2(0, 24 * EDSCALE));

	toggle_scripts_button = memnew(Button);
	toggle_scripts_button->set_flat(true);
	toggle_scripts_button->connect(SceneStringName(pressed), callable_mp(this, &EditorHelp::_toggle_scripts_pressed));
	status_bar->add_child(toggle_scripts_button);

	class_desc->set_selection_enabled(true);
	class_desc->set_context_menu_enabled(true);

	class_desc->hide();
}

EditorHelp::~EditorHelp() {
}

DocTools *EditorHelp::get_doc_data() {
	_wait_for_thread();
	return doc;
}

/// EditorHelpBit ///

#define HANDLE_DOC(m_string) ((is_native ? DTR(m_string) : (m_string)).strip_edges())

EditorHelpBit::HelpData EditorHelpBit::_get_class_help_data(const StringName &p_class_name) {
	if (doc_class_cache.has(p_class_name)) {
		return doc_class_cache[p_class_name];
	}

	HelpData result;

	const HashMap<String, DocData::ClassDoc>::ConstIterator E = EditorHelp::get_doc_data()->class_list.find(p_class_name);
	if (E) {
		// Non-native class shouldn't be cached, nor translated.
		const bool is_native = !E->value.is_script_doc;

		result.description = HANDLE_DOC(E->value.brief_description);
		if (E->value.is_deprecated) {
			if (E->value.deprecated_message.is_empty()) {
				result.deprecated_message = TTR("This class may be changed or removed in future versions.");
			} else {
				result.deprecated_message = HANDLE_DOC(E->value.deprecated_message);
			}
		}
		if (E->value.is_experimental) {
			if (E->value.experimental_message.is_empty()) {
				result.experimental_message = TTR("This class may be changed or removed in future versions.");
			} else {
				result.experimental_message = HANDLE_DOC(E->value.experimental_message);
			}
		}

		if (is_native) {
			doc_class_cache[p_class_name] = result;
		}
	}

	return result;
}

EditorHelpBit::HelpData EditorHelpBit::_get_property_help_data(const StringName &p_class_name, const StringName &p_property_name) {
	if (doc_property_cache.has(p_class_name) && doc_property_cache[p_class_name].has(p_property_name)) {
		return doc_property_cache[p_class_name][p_property_name];
	}

	HelpData result;

	const DocTools *dd = EditorHelp::get_doc_data();
	const HashMap<String, DocData::ClassDoc>::ConstIterator E = dd->class_list.find(p_class_name);
	if (E) {
		// Non-native properties shouldn't be cached, nor translated.
		const bool is_native = !E->value.is_script_doc;

		for (const DocData::PropertyDoc &property : E->value.properties) {
			HelpData current;
			current.description = HANDLE_DOC(property.description);
			if (property.is_deprecated) {
				if (property.deprecated_message.is_empty()) {
					current.deprecated_message = TTR("This property may be changed or removed in future versions.");
				} else {
					current.deprecated_message = HANDLE_DOC(property.deprecated_message);
				}
			}
			if (property.is_experimental) {
				if (property.experimental_message.is_empty()) {
					current.experimental_message = TTR("This property may be changed or removed in future versions.");
				} else {
					current.experimental_message = HANDLE_DOC(property.experimental_message);
				}
			}

			String enum_class_name;
			String enum_name;
			if (CoreConstants::is_global_enum(property.enumeration)) {
				enum_class_name = "@GlobalScope";
				enum_name = property.enumeration;
			} else {
				const int dot_pos = property.enumeration.rfind_char('.');
				if (dot_pos >= 0) {
					enum_class_name = property.enumeration.left(dot_pos);
					enum_name = property.enumeration.substr(dot_pos + 1);
				}
			}

			if (!enum_class_name.is_empty() && !enum_name.is_empty()) {
				// Classes can use enums from other classes, so check from which it came.
				const HashMap<String, DocData::ClassDoc>::ConstIterator enum_class = dd->class_list.find(enum_class_name);
				if (enum_class) {
					const String enum_prefix = EditorPropertyNameProcessor::get_singleton()->process_name(enum_name, EditorPropertyNameProcessor::STYLE_CAPITALIZED) + " ";
					for (DocData::ConstantDoc constant : enum_class->value.constants) {
						// Don't display `_MAX` enum value descriptions, as these are never exposed in the inspector.
						if (constant.enumeration == enum_name && !constant.name.ends_with("_MAX")) {
							// Prettify the enum value display, so that "<ENUM_NAME>_<ITEM>" becomes "Item".
							const String item_name = EditorPropertyNameProcessor::get_singleton()->process_name(constant.name, EditorPropertyNameProcessor::STYLE_CAPITALIZED).trim_prefix(enum_prefix);
							String item_descr = HANDLE_DOC(constant.description);
							if (item_descr.is_empty()) {
								item_descr = "[color=<EditorHelpBitCommentColor>][i]" + TTR("No description available.") + "[/i][/color]";
							}
							current.description += vformat("\n[b]%s:[/b] %s", item_name, item_descr);
						}
					}
					current.description = current.description.lstrip("\n");
				}
			}

			if (property.name == p_property_name) {
				result = current;

				if (!is_native) {
					break;
				}
			}

			if (is_native) {
				doc_property_cache[p_class_name][property.name] = current;
			}
		}
	}

	return result;
}

EditorHelpBit::HelpData EditorHelpBit::_get_method_help_data(const StringName &p_class_name, const StringName &p_method_name) {
	if (doc_method_cache.has(p_class_name) && doc_method_cache[p_class_name].has(p_method_name)) {
		return doc_method_cache[p_class_name][p_method_name];
	}

	HelpData result;

	const HashMap<String, DocData::ClassDoc>::ConstIterator E = EditorHelp::get_doc_data()->class_list.find(p_class_name);
	if (E) {
		// Non-native methods shouldn't be cached, nor translated.
		const bool is_native = !E->value.is_script_doc;

		for (const DocData::MethodDoc &method : E->value.methods) {
			HelpData current;
			current.description = HANDLE_DOC(method.description);
			if (method.is_deprecated) {
				if (method.deprecated_message.is_empty()) {
					current.deprecated_message = TTR("This method may be changed or removed in future versions.");
				} else {
					current.deprecated_message = HANDLE_DOC(method.deprecated_message);
				}
			}
			if (method.is_experimental) {
				if (method.experimental_message.is_empty()) {
					current.experimental_message = TTR("This method may be changed or removed in future versions.");
				} else {
					current.experimental_message = HANDLE_DOC(method.experimental_message);
				}
			}
			current.doc_type = { method.return_type, method.return_enum, method.return_is_bitfield };
			for (const DocData::ArgumentDoc &argument : method.arguments) {
				const DocType argument_type = { argument.type, argument.enumeration, argument.is_bitfield };
				current.arguments.push_back({ argument.name, argument_type, argument.default_value });
			}

			if (method.name == p_method_name) {
				result = current;

				if (!is_native) {
					break;
				}
			}

			if (is_native) {
				doc_method_cache[p_class_name][method.name] = current;
			}
		}
	}

	return result;
}

EditorHelpBit::HelpData EditorHelpBit::_get_signal_help_data(const StringName &p_class_name, const StringName &p_signal_name) {
	if (doc_signal_cache.has(p_class_name) && doc_signal_cache[p_class_name].has(p_signal_name)) {
		return doc_signal_cache[p_class_name][p_signal_name];
	}

	HelpData result;

	const HashMap<String, DocData::ClassDoc>::ConstIterator E = EditorHelp::get_doc_data()->class_list.find(p_class_name);
	if (E) {
		// Non-native signals shouldn't be cached, nor translated.
		const bool is_native = !E->value.is_script_doc;

		for (const DocData::MethodDoc &signal : E->value.signals) {
			HelpData current;
			current.description = HANDLE_DOC(signal.description);
			if (signal.is_deprecated) {
				if (signal.deprecated_message.is_empty()) {
					current.deprecated_message = TTR("This signal may be changed or removed in future versions.");
				} else {
					current.deprecated_message = HANDLE_DOC(signal.deprecated_message);
				}
			}
			if (signal.is_experimental) {
				if (signal.experimental_message.is_empty()) {
					current.experimental_message = TTR("This signal may be changed or removed in future versions.");
				} else {
					current.experimental_message = HANDLE_DOC(signal.experimental_message);
				}
			}
			for (const DocData::ArgumentDoc &argument : signal.arguments) {
				const DocType argument_type = { argument.type, argument.enumeration, argument.is_bitfield };
				current.arguments.push_back({ argument.name, argument_type, argument.default_value });
			}

			if (signal.name == p_signal_name) {
				result = current;

				if (!is_native) {
					break;
				}
			}

			if (is_native) {
				doc_signal_cache[p_class_name][signal.name] = current;
			}
		}
	}

	return result;
}

EditorHelpBit::HelpData EditorHelpBit::_get_theme_item_help_data(const StringName &p_class_name, const StringName &p_theme_item_name) {
	if (doc_theme_item_cache.has(p_class_name) && doc_theme_item_cache[p_class_name].has(p_theme_item_name)) {
		return doc_theme_item_cache[p_class_name][p_theme_item_name];
	}

	HelpData result;

	bool found = false;
	const DocTools *dd = EditorHelp::get_doc_data();
	HashMap<String, DocData::ClassDoc>::ConstIterator E = dd->class_list.find(p_class_name);
	while (E) {
		// Non-native theme items shouldn't be cached, nor translated.
		const bool is_native = !E->value.is_script_doc;

		for (const DocData::ThemeItemDoc &theme_item : E->value.theme_properties) {
			HelpData current;
			current.description = HANDLE_DOC(theme_item.description);
			if (theme_item.is_deprecated) {
				if (theme_item.deprecated_message.is_empty()) {
					current.deprecated_message = TTR("This theme property may be changed or removed in future versions.");
				} else {
					current.deprecated_message = HANDLE_DOC(theme_item.deprecated_message);
				}
			}
			if (theme_item.is_experimental) {
				if (theme_item.experimental_message.is_empty()) {
					current.experimental_message = TTR("This theme property may be changed or removed in future versions.");
				} else {
					current.experimental_message = HANDLE_DOC(theme_item.experimental_message);
				}
			}

			if (theme_item.name == p_theme_item_name) {
				result = current;
				found = true;

				if (!is_native) {
					break;
				}
			}

			if (is_native) {
				doc_theme_item_cache[p_class_name][theme_item.name] = current;
			}
		}

		if (found || E->value.inherits.is_empty()) {
			break;
		}

		// Check for inherited theme items.
		E = dd->class_list.find(E->value.inherits);
	}

	return result;
}

#undef HANDLE_DOC

void EditorHelpBit::_add_type_to_title(const DocType &p_doc_type) {
	_add_type_to_rt(p_doc_type.type, p_doc_type.enumeration, p_doc_type.is_bitfield, title, this, symbol_class_name);
}

void EditorHelpBit::_update_labels() {
	const Ref<Font> doc_bold_font = get_theme_font(SNAME("doc_bold"), EditorStringName(EditorFonts));

	if (!symbol_visible_type.is_empty() || !symbol_name.is_empty()) {
		title->clear();

		title->push_font(doc_bold_font);

		if (!symbol_visible_type.is_empty()) {
			title->push_color(get_theme_color(SNAME("title_color"), SNAME("EditorHelp")));
			title->add_text(symbol_visible_type);
			title->pop(); // color
		}

		if (!symbol_visible_type.is_empty() && !symbol_name.is_empty()) {
			title->add_text(" ");
		}

		if (!symbol_name.is_empty()) {
			title->push_underline();
			title->add_text(symbol_name);
			title->pop(); // underline
		}

		title->pop(); // font

		if (symbol_type == "method" || symbol_type == "signal") {
			const Color symbol_color = get_theme_color(SNAME("symbol_color"), SNAME("EditorHelp"));
			const Color value_color = get_theme_color(SNAME("value_color"), SNAME("EditorHelp"));

			title->push_font(get_theme_font(SNAME("doc_source"), EditorStringName(EditorFonts)));
			title->push_font_size(get_theme_font_size(SNAME("doc_source_size"), EditorStringName(EditorFonts)) * 0.9);

			title->push_color(symbol_color);
			title->add_text("(");
			title->pop(); // color

			for (int i = 0; i < help_data.arguments.size(); i++) {
				const ArgumentData &argument = help_data.arguments[i];

				if (i > 0) {
					title->push_color(symbol_color);
					title->add_text(", ");
					title->pop(); // color
				}

				title->add_text(argument.name);

				title->push_color(symbol_color);
				title->add_text(": ");
				title->pop(); // color

				_add_type_to_title(argument.doc_type);

				if (!argument.default_value.is_empty()) {
					title->push_color(symbol_color);
					title->add_text(" = ");
					title->pop(); // color

					title->push_color(value_color);
					title->add_text(argument.default_value);
					title->pop(); // color
				}
			}

			title->push_color(symbol_color);
			title->add_text(")");
			title->pop(); // color

			if (symbol_type == "method") {
				title->push_color(symbol_color);
				title->add_text(" -> ");
				title->pop(); // color

				_add_type_to_title(help_data.doc_type);
			}

			title->pop(); // font_size
			title->pop(); // font
		}

		title->show();
	} else {
		title->hide();
	}

	content->clear();

	bool has_prev_text = false;

	if (!help_data.deprecated_message.is_empty()) {
		has_prev_text = true;

		Ref<Texture2D> error_icon = get_editor_theme_icon(SNAME("StatusError"));
		content->add_image(error_icon, error_icon->get_width(), error_icon->get_height());
		content->add_text(" ");
		content->push_color(get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		content->push_font(doc_bold_font);
		content->add_text(TTR("Deprecated:"));
		content->pop(); // font
		content->pop(); // color
		content->add_text(" ");
		_add_text_to_rt(help_data.deprecated_message, content, this, symbol_class_name);
	}

	if (!help_data.experimental_message.is_empty()) {
		if (has_prev_text) {
			content->add_newline();
			content->add_newline();
		}
		has_prev_text = true;

		Ref<Texture2D> warning_icon = get_editor_theme_icon(SNAME("NodeWarning"));
		content->add_image(warning_icon, warning_icon->get_width(), warning_icon->get_height());
		content->add_text(" ");
		content->push_color(get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		content->push_font(doc_bold_font);
		content->add_text(TTR("Experimental:"));
		content->pop(); // font
		content->pop(); // color
		content->add_text(" ");
		_add_text_to_rt(help_data.experimental_message, content, this, symbol_class_name);
	}

	if (!help_data.description.is_empty()) {
		if (has_prev_text) {
			content->add_newline();
			content->add_newline();
		}
		has_prev_text = true;

		const Color comment_color = get_theme_color(SNAME("comment_color"), SNAME("EditorHelp"));
		_add_text_to_rt(help_data.description.replace("<EditorHelpBitCommentColor>", comment_color.to_html()), content, this, symbol_class_name);
	}

	if (is_inside_tree()) {
		update_content_height();
	}
}

void EditorHelpBit::_go_to_help(const String &p_what) {
	EditorNode::get_singleton()->get_editor_main_screen()->select(EditorMainScreen::EDITOR_SCRIPT);
	ScriptEditor::get_singleton()->goto_help(p_what);
	emit_signal(SNAME("request_hide"));
}

void EditorHelpBit::_meta_clicked(const String &p_select) {
	if (p_select.begins_with("$")) { // Enum.
		const String link = p_select.substr(1);

		String enum_class_name;
		String enum_name;
		if (CoreConstants::is_global_enum(link)) {
			enum_class_name = "@GlobalScope";
			enum_name = link;
		} else {
			const int dot_pos = link.rfind_char('.');
			if (dot_pos >= 0) {
				enum_class_name = link.left(dot_pos);
				enum_name = link.substr(dot_pos + 1);
			} else {
				enum_class_name = symbol_class_name;
				enum_name = link;
			}
		}

		_go_to_help("class_enum:" + enum_class_name + ":" + enum_name);
	} else if (p_select.begins_with("#")) { // Class.
		_go_to_help("class_name:" + p_select.substr(1));
	} else if (p_select.begins_with("@")) { // Member.
		const int tag_end = p_select.find_char(' ');
		const String tag = p_select.substr(1, tag_end - 1);
		const String link = p_select.substr(tag_end + 1).lstrip(" ");

		String topic;
		if (tag == "method") {
			topic = "class_method";
		} else if (tag == "constructor") {
			topic = "class_method";
		} else if (tag == "operator") {
			topic = "class_method";
		} else if (tag == "member") {
			topic = "class_property";
		} else if (tag == "enum") {
			topic = "class_enum";
		} else if (tag == "signal") {
			topic = "class_signal";
		} else if (tag == "constant") {
			topic = "class_constant";
		} else if (tag == "annotation") {
			topic = "class_annotation";
		} else if (tag == "theme_item") {
			topic = "class_theme_item";
		} else {
			return;
		}

		if (link.contains_char('.')) {
			const int class_end = link.find_char('.');
			_go_to_help(topic + ":" + link.left(class_end) + ":" + link.substr(class_end + 1));
		} else {
			_go_to_help(topic + ":" + symbol_class_name + ":" + link);
		}
	} else if (p_select.begins_with("http:") || p_select.begins_with("https:")) {
		OS::get_singleton()->shell_open(p_select);
	} else if (p_select.begins_with("^")) { // Copy button.
		DisplayServer::get_singleton()->clipboard_set(p_select.substr(1));
	}
}

void EditorHelpBit::_bind_methods() {
	ADD_SIGNAL(MethodInfo("request_hide"));
}

void EditorHelpBit::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
			_update_labels();
			break;
	}
}

void EditorHelpBit::parse_symbol(const String &p_symbol, const String &p_prologue) {
	const PackedStringArray slices = p_symbol.split("|", true, 2);
	ERR_FAIL_COND_MSG(slices.size() < 3, "Invalid doc id. The expected format is 'item_type|class_name|item_name'.");

	const String &item_type = slices[0];
	const String &class_name = slices[1];
	const String &item_name = slices[2];

	String visible_type;
	String name = item_name;

	if (item_type == "class") {
		visible_type = TTR("Class:");
		name = class_name;
		help_data = _get_class_help_data(class_name);
	} else if (item_type == "property") {
		if (name.begins_with("metadata/")) {
			visible_type = TTR("Metadata:");
			name = name.trim_prefix("metadata/");
		} else if (class_name == "ProjectSettings" || class_name == "EditorSettings") {
			visible_type = TTR("Setting:");
		} else {
			visible_type = TTR("Property:");
		}
		help_data = _get_property_help_data(class_name, item_name);
	} else if (item_type == "internal_property") {
		visible_type = TTR("Internal Property:");
		help_data = HelpData();
		help_data.description = "[color=<EditorHelpBitCommentColor>][i]" + TTR("This property can only be set in the Inspector.") + "[/i][/color]";
	} else if (item_type == "method") {
		visible_type = TTR("Method:");
		help_data = _get_method_help_data(class_name, item_name);
	} else if (item_type == "signal") {
		visible_type = TTR("Signal:");
		help_data = _get_signal_help_data(class_name, item_name);
	} else if (item_type == "theme_item") {
		visible_type = TTR("Theme Property:");
		help_data = _get_theme_item_help_data(class_name, item_name);
	} else {
		ERR_FAIL_MSG("Invalid tooltip type '" + item_type + "'. Valid types are 'class', 'property', 'internal_property', 'method', 'signal', and 'theme_item'.");
	}

	symbol_class_name = class_name;
	symbol_type = item_type;
	symbol_visible_type = visible_type;
	symbol_name = name;

	if (!p_prologue.is_empty()) {
		if (help_data.description.is_empty()) {
			help_data.description = p_prologue;
		} else {
			help_data.description = p_prologue + "\n" + help_data.description;
		}
	}

	if (help_data.description.is_empty()) {
		help_data.description = "[color=<EditorHelpBitCommentColor>][i]" + TTR("No description available.") + "[/i][/color]";
	}

	if (is_inside_tree()) {
		_update_labels();
	}
}

void EditorHelpBit::set_custom_text(const String &p_type, const String &p_name, const String &p_description) {
	symbol_class_name = String();
	symbol_type = String();
	symbol_visible_type = p_type;
	symbol_name = p_name;

	help_data = HelpData();
	help_data.description = p_description;

	if (is_inside_tree()) {
		_update_labels();
	}
}

void EditorHelpBit::set_content_height_limits(float p_min, float p_max) {
	ERR_FAIL_COND(p_min > p_max);
	content_min_height = p_min;
	content_max_height = p_max;

	if (is_inside_tree()) {
		update_content_height();
	}
}

void EditorHelpBit::update_content_height() {
	float content_height = content->get_content_height();
	const Ref<StyleBox> style = content->get_theme_stylebox(CoreStringName(normal));
	if (style.is_valid()) {
		content_height += style->get_content_margin(SIDE_TOP) + style->get_content_margin(SIDE_BOTTOM);
	}
	content->set_custom_minimum_size(Size2(content->get_custom_minimum_size().x, CLAMP(content_height, content_min_height, content_max_height)));
}

EditorHelpBit::EditorHelpBit(const String &p_symbol, const String &p_prologue, bool p_allow_selection) {
	add_theme_constant_override("separation", 0);

	title = memnew(RichTextLabel);
	title->set_theme_type_variation("EditorHelpBitTitle");
	title->set_custom_minimum_size(Size2(512 * EDSCALE, 0)); // GH-93031. Set the minimum width even if `fit_content` is true.
	title->set_fit_content(true);
	title->set_selection_enabled(p_allow_selection);
	title->set_context_menu_enabled(p_allow_selection);
	title->connect("meta_clicked", callable_mp(this, &EditorHelpBit::_meta_clicked));
	title->hide();
	add_child(title);

	content_min_height = 48 * EDSCALE;
	content_max_height = 360 * EDSCALE;

	content = memnew(RichTextLabel);
	content->set_theme_type_variation("EditorHelpBitContent");
	content->set_custom_minimum_size(Size2(512 * EDSCALE, content_min_height));
	content->set_selection_enabled(p_allow_selection);
	content->set_context_menu_enabled(p_allow_selection);
	content->connect("meta_clicked", callable_mp(this, &EditorHelpBit::_meta_clicked));
	add_child(content);

	if (!p_symbol.is_empty()) {
		parse_symbol(p_symbol, p_prologue);
	}
}

/// EditorHelpBitTooltip ///

bool EditorHelpBitTooltip::_is_tooltip_visible = false;

Control *EditorHelpBitTooltip::_make_invisible_control() {
	Control *control = memnew(Control);
	control->set_visible(false);
	return control;
}

void EditorHelpBitTooltip::_start_timer() {
	if (timer->is_inside_tree() && timer->is_stopped()) {
		timer->start();
	}
}

void EditorHelpBitTooltip::_target_gui_input(const Ref<InputEvent> &p_event) {
	// Only scrolling is not checked in `NOTIFICATION_INTERNAL_PROCESS`.
	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		switch (mb->get_button_index()) {
			case MouseButton::WHEEL_UP:
			case MouseButton::WHEEL_DOWN:
			case MouseButton::WHEEL_LEFT:
			case MouseButton::WHEEL_RIGHT:
				queue_free();
				break;
			default:
				break;
		}
	}
}

void EditorHelpBitTooltip::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
			_is_tooltip_visible = true;
			_enter_tree_time = OS::get_singleton()->get_ticks_msec();
			break;
		case NOTIFICATION_EXIT_TREE:
			_is_tooltip_visible = false;
			break;
		case NOTIFICATION_WM_MOUSE_ENTER:
			_is_mouse_inside_tooltip = true;
			timer->stop();
			break;
		case NOTIFICATION_WM_MOUSE_EXIT:
			_is_mouse_inside_tooltip = false;
			_start_timer();
			break;
		case NOTIFICATION_INTERNAL_PROCESS:
			// A workaround to hide the tooltip since the window does not receive keyboard events
			// with `FLAG_POPUP` and `FLAG_NO_FOCUS` flags, so we can't use `_input_from_window()`.
			if (is_inside_tree()) {
				if (Input::get_singleton()->is_action_just_pressed(SNAME("ui_cancel"), true)) {
					queue_free();
					get_parent_viewport()->set_input_as_handled();
				} else if (Input::get_singleton()->is_anything_pressed_except_mouse()) {
					queue_free();
				} else if (!Input::get_singleton()->get_mouse_button_mask().is_empty()) {
					if (!_is_mouse_inside_tooltip) {
						queue_free();
					}
				} else if (!Input::get_singleton()->get_last_mouse_velocity().is_zero_approx()) {
					if (!_is_mouse_inside_tooltip && OS::get_singleton()->get_ticks_msec() - _enter_tree_time > 250) {
						_start_timer();
					}
				}
			}
			break;
	}
}

Control *EditorHelpBitTooltip::show_tooltip(Control *p_target, const String &p_symbol, const String &p_prologue) {
	// Show the custom tooltip only if it is not already visible.
	// The viewport will retrigger `make_custom_tooltip()` every few seconds
	// because the return control is not visible even if the custom tooltip is displayed.
	if (_is_tooltip_visible || Input::get_singleton()->is_anything_pressed()) {
		return _make_invisible_control();
	}

	EditorHelpBit *help_bit = memnew(EditorHelpBit(p_symbol, p_prologue, false));

	EditorHelpBitTooltip *tooltip = memnew(EditorHelpBitTooltip(p_target));
	help_bit->connect("request_hide", callable_mp(static_cast<Node *>(tooltip), &Node::queue_free));
	tooltip->add_child(help_bit);
	p_target->add_child(tooltip);

	help_bit->update_content_height();
	tooltip->popup_under_cursor();

	return _make_invisible_control();
}

// Copy-paste from `Viewport::_gui_show_tooltip()`.
void EditorHelpBitTooltip::popup_under_cursor() {
	Point2 mouse_pos = get_mouse_position();
	Point2 tooltip_offset = GLOBAL_GET("display/mouse_cursor/tooltip_position_offset");
	Rect2 r(mouse_pos + tooltip_offset, get_contents_minimum_size());
	r.size = r.size.min(get_max_size());

	Window *window = get_parent_visible_window();
	Rect2i vr;
	if (is_embedded()) {
		vr = get_embedder()->get_visible_rect();
	} else {
		vr = window->get_usable_parent_rect();
	}

	if (r.size.x + r.position.x > vr.size.x + vr.position.x) {
		// Place it in the opposite direction. If it fails, just hug the border.
		r.position.x = mouse_pos.x - r.size.x - tooltip_offset.x;

		if (r.position.x < vr.position.x) {
			r.position.x = vr.position.x + vr.size.x - r.size.x;
		}
	} else if (r.position.x < vr.position.x) {
		r.position.x = vr.position.x;
	}

	if (r.size.y + r.position.y > vr.size.y + vr.position.y) {
		// Same as above.
		r.position.y = mouse_pos.y - r.size.y - tooltip_offset.y;

		if (r.position.y < vr.position.y) {
			r.position.y = vr.position.y + vr.size.y - r.size.y;
		}
	} else if (r.position.y < vr.position.y) {
		r.position.y = vr.position.y;
	}

	// When `FLAG_POPUP` is false, it prevents the editor from losing focus when displaying the tooltip.
	// This way, clicks and double-clicks are still available outside the tooltip.
	set_flag(Window::FLAG_POPUP, false);
	set_flag(Window::FLAG_NO_FOCUS, true);
	popup(r);
}

EditorHelpBitTooltip::EditorHelpBitTooltip(Control *p_target) {
	set_theme_type_variation("TooltipPanel");

	timer = memnew(Timer);
	timer->set_wait_time(0.25);
	timer->connect("timeout", callable_mp(static_cast<Node *>(this), &Node::queue_free));
	add_child(timer);

	ERR_FAIL_NULL(p_target);
	p_target->connect(SceneStringName(mouse_exited), callable_mp(this, &EditorHelpBitTooltip::_start_timer));
	p_target->connect(SceneStringName(gui_input), callable_mp(this, &EditorHelpBitTooltip::_target_gui_input));

	set_process_internal(true);
}

#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
/// EditorHelpHighlighter ///

EditorHelpHighlighter *EditorHelpHighlighter::singleton = nullptr;

void EditorHelpHighlighter::create_singleton() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = memnew(EditorHelpHighlighter);
}

void EditorHelpHighlighter::free_singleton() {
	ERR_FAIL_NULL(singleton);
	memdelete(singleton);
	singleton = nullptr;
}

EditorHelpHighlighter *EditorHelpHighlighter::get_singleton() {
	return singleton;
}

EditorHelpHighlighter::HighlightData EditorHelpHighlighter::_get_highlight_data(Language p_language, const String &p_source, bool p_use_cache) {
	switch (p_language) {
		case LANGUAGE_GDSCRIPT:
#ifndef MODULE_GDSCRIPT_ENABLED
			ERR_FAIL_V_MSG(HighlightData(), "GDScript module is disabled.");
#endif
			break;
		case LANGUAGE_CSHARP:
#ifndef MODULE_MONO_ENABLED
			ERR_FAIL_V_MSG(HighlightData(), "Mono module is disabled.");
#endif
			break;
		default:
			ERR_FAIL_V_MSG(HighlightData(), "Invalid parameter \"p_language\".");
	}

	if (p_use_cache) {
		const HashMap<String, HighlightData>::ConstIterator E = highlight_data_caches[p_language].find(p_source);
		if (E) {
			return E->value;
		}
	}

	text_edits[p_language]->set_text(p_source);
	if (scripts[p_language].is_valid()) { // See GH-89610.
		scripts[p_language]->set_source_code(p_source);
	}
	highlighters[p_language]->_update_cache();

	HighlightData result;

	int source_offset = 0;
	int result_index = 0;
	for (int i = 0; i < text_edits[p_language]->get_line_count(); i++) {
		const Dictionary dict = highlighters[p_language]->_get_line_syntax_highlighting_impl(i);

		result.resize(result.size() + dict.size());

		const Variant *key = nullptr;
		int prev_column = -1;
		while ((key = dict.next(key)) != nullptr) {
			const int column = *key;
			ERR_FAIL_COND_V(column <= prev_column, HighlightData());
			prev_column = column;

			const Color color = dict[*key].operator Dictionary().get("color", Color());

			result.write[result_index] = { source_offset + column, color };
			result_index++;
		}

		source_offset += text_edits[p_language]->get_line(i).length() + 1; // Plus newline.
	}

	if (p_use_cache) {
		highlight_data_caches[p_language][p_source] = result;
	}

	return result;
}

void EditorHelpHighlighter::highlight(RichTextLabel *p_rich_text_label, Language p_language, const String &p_source, bool p_use_cache) {
	ERR_FAIL_NULL(p_rich_text_label);

	const HighlightData highlight_data = _get_highlight_data(p_language, p_source, p_use_cache);

	if (!highlight_data.is_empty()) {
		for (int i = 1; i < highlight_data.size(); i++) {
			const Pair<int, Color> &prev = highlight_data[i - 1];
			const Pair<int, Color> &curr = highlight_data[i];
			p_rich_text_label->push_color(prev.second);
			p_rich_text_label->add_text(p_source.substr(prev.first, curr.first - prev.first));
			p_rich_text_label->pop(); // color
		}

		const Pair<int, Color> &last = highlight_data[highlight_data.size() - 1];
		p_rich_text_label->push_color(last.second);
		p_rich_text_label->add_text(p_source.substr(last.first));
		p_rich_text_label->pop(); // color
	}
}

void EditorHelpHighlighter::reset_cache() {
	const Color text_color = EDITOR_GET("text_editor/theme/highlighting/text_color");

#ifdef MODULE_GDSCRIPT_ENABLED
	highlight_data_caches[LANGUAGE_GDSCRIPT].clear();
	text_edits[LANGUAGE_GDSCRIPT]->add_theme_color_override(SceneStringName(font_color), text_color);
#endif

#ifdef MODULE_MONO_ENABLED
	highlight_data_caches[LANGUAGE_CSHARP].clear();
	text_edits[LANGUAGE_CSHARP]->add_theme_color_override(SceneStringName(font_color), text_color);
#endif
}

EditorHelpHighlighter::EditorHelpHighlighter() {
	const Color text_color = EDITOR_GET("text_editor/theme/highlighting/text_color");

#ifdef MODULE_GDSCRIPT_ENABLED
	TextEdit *gdscript_text_edit = memnew(TextEdit);
	gdscript_text_edit->add_theme_color_override(SceneStringName(font_color), text_color);

	Ref<GDScript> gdscript;
	gdscript.instantiate();

	Ref<GDScriptSyntaxHighlighter> gdscript_highlighter;
	gdscript_highlighter.instantiate();
	gdscript_highlighter->set_text_edit(gdscript_text_edit);
	gdscript_highlighter->_set_edited_resource(gdscript);

	text_edits[LANGUAGE_GDSCRIPT] = gdscript_text_edit;
	scripts[LANGUAGE_GDSCRIPT] = gdscript;
	highlighters[LANGUAGE_GDSCRIPT] = gdscript_highlighter;
#endif

#ifdef MODULE_MONO_ENABLED
	TextEdit *csharp_text_edit = memnew(TextEdit);
	csharp_text_edit->add_theme_color_override(SceneStringName(font_color), text_color);

	// See GH-89610.
	//Ref<CSharpScript> csharp;
	//csharp.instantiate();

	Ref<EditorStandardSyntaxHighlighter> csharp_highlighter;
	csharp_highlighter.instantiate();
	csharp_highlighter->set_text_edit(csharp_text_edit);
	//csharp_highlighter->_set_edited_resource(csharp);
	csharp_highlighter->_set_script_language(CSharpLanguage::get_singleton());

	text_edits[LANGUAGE_CSHARP] = csharp_text_edit;
	//scripts[LANGUAGE_CSHARP] = csharp;
	highlighters[LANGUAGE_CSHARP] = csharp_highlighter;
#endif
}

EditorHelpHighlighter::~EditorHelpHighlighter() {
#ifdef MODULE_GDSCRIPT_ENABLED
	memdelete(text_edits[LANGUAGE_GDSCRIPT]);
#endif

#ifdef MODULE_MONO_ENABLED
	memdelete(text_edits[LANGUAGE_CSHARP]);
#endif
}

#endif // defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)

/// FindBar ///

FindBar::FindBar() {
	search_text = memnew(LineEdit);
	add_child(search_text);
	search_text->set_custom_minimum_size(Size2(100 * EDSCALE, 0));
	search_text->set_h_size_flags(SIZE_EXPAND_FILL);
	search_text->connect(SceneStringName(text_changed), callable_mp(this, &FindBar::_search_text_changed));
	search_text->connect(SceneStringName(text_submitted), callable_mp(this, &FindBar::_search_text_submitted));

	matches_label = memnew(Label);
	add_child(matches_label);
	matches_label->hide();

	find_prev = memnew(Button);
	find_prev->set_flat(true);
	add_child(find_prev);
	find_prev->set_focus_mode(FOCUS_NONE);
	find_prev->connect(SceneStringName(pressed), callable_mp(this, &FindBar::search_prev));

	find_next = memnew(Button);
	find_next->set_flat(true);
	add_child(find_next);
	find_next->set_focus_mode(FOCUS_NONE);
	find_next->connect(SceneStringName(pressed), callable_mp(this, &FindBar::search_next));

	Control *space = memnew(Control);
	add_child(space);
	space->set_custom_minimum_size(Size2(4, 0) * EDSCALE);

	hide_button = memnew(TextureButton);
	add_child(hide_button);
	hide_button->set_focus_mode(FOCUS_NONE);
	hide_button->set_ignore_texture_size(true);
	hide_button->set_stretch_mode(TextureButton::STRETCH_KEEP_CENTERED);
	hide_button->connect(SceneStringName(pressed), callable_mp(this, &FindBar::_hide_bar));
}

void FindBar::popup_search() {
	show();
	bool grabbed_focus = false;
	if (!search_text->has_focus()) {
		search_text->grab_focus();
		grabbed_focus = true;
	}

	if (!search_text->get_text().is_empty()) {
		search_text->select_all();
		search_text->set_caret_column(search_text->get_text().length());
		if (grabbed_focus) {
			_search();
		}
	}
}

void FindBar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			find_prev->set_button_icon(get_editor_theme_icon(SNAME("MoveUp")));
			find_next->set_button_icon(get_editor_theme_icon(SNAME("MoveDown")));
			hide_button->set_texture_normal(get_editor_theme_icon(SNAME("Close")));
			hide_button->set_texture_hover(get_editor_theme_icon(SNAME("Close")));
			hide_button->set_texture_pressed(get_editor_theme_icon(SNAME("Close")));
			hide_button->set_custom_minimum_size(hide_button->get_texture_normal()->get_size());
			matches_label->add_theme_color_override(SceneStringName(font_color), results_count > 0 ? get_theme_color(SceneStringName(font_color), SNAME("Label")) : get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			set_process_unhandled_input(is_visible_in_tree());
		} break;
	}
}

void FindBar::set_rich_text_label(RichTextLabel *p_rich_text_label) {
	rich_text_label = p_rich_text_label;
}

bool FindBar::search_next() {
	return _search();
}

bool FindBar::search_prev() {
	return _search(true);
}

bool FindBar::_search(bool p_search_previous) {
	String stext = search_text->get_text();
	bool keep = prev_search == stext;

	bool ret = rich_text_label->search(stext, keep, p_search_previous);

	prev_search = stext;

	if (ret) {
		_update_results_count();
	} else {
		results_count = 0;
	}
	_update_matches_label();

	return ret;
}

void FindBar::_update_results_count() {
	results_count = 0;

	String searched = search_text->get_text();
	if (searched.is_empty()) {
		return;
	}

	String full_text = rich_text_label->get_parsed_text();

	int from_pos = 0;

	while (true) {
		int pos = full_text.findn(searched, from_pos);
		if (pos == -1) {
			break;
		}

		results_count++;
		from_pos = pos + searched.length();
	}
}

void FindBar::_update_matches_label() {
	if (search_text->get_text().is_empty() || results_count == -1) {
		matches_label->hide();
	} else {
		matches_label->show();

		matches_label->add_theme_color_override(SceneStringName(font_color), results_count > 0 ? get_theme_color(SceneStringName(font_color), SNAME("Label")) : get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
		matches_label->set_text(vformat(results_count == 1 ? TTR("%d match.") : TTR("%d matches."), results_count));
	}
}

void FindBar::_hide_bar() {
	if (search_text->has_focus()) {
		rich_text_label->grab_focus();
	}

	hide();
}

void FindBar::unhandled_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_action_pressed(SNAME("ui_cancel"), false, true)) {
		if (rich_text_label->has_focus() || is_ancestor_of(get_viewport()->gui_get_focus_owner())) {
			_hide_bar();
			accept_event();
		}
	}
}

void FindBar::_search_text_changed(const String &p_text) {
	search_next();
}

void FindBar::_search_text_submitted(const String &p_text) {
	if (Input::get_singleton()->is_key_pressed(Key::SHIFT)) {
		search_prev();
	} else {
		search_next();
	}
}
