/**************************************************************************/
/*  test_highlighter.h                                                    */
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

#ifndef TEST_HIGHLIGHTER_H
#define TEST_HIGHLIGHTER_H

#ifdef TOOLS_ENABLED

#include "../editor/gdscript_highlighter.h"
#include "../gdscript.h"
#include "gdscript_test_runner.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include "editor/editor_settings.h"
#include "scene/gui/text_edit.h"
#include "tests/test_macros.h"

namespace GDScriptTests {

class TestHighlighter {
	static HashMap<Color, String> color_map;
	static TextEdit *text_edit;
	static Ref<GDScriptSyntaxHighlighter> highlighter;

	static void make_output(int p_line_idx, const String &p_line_str, const Dictionary &p_dict, String &r_output) {
		String symbols;
		String error;

		const Variant *key = nullptr;
		while ((key = p_dict.next(key)) != nullptr) {
			const int column = *key;
			const Color color = p_dict[*key].operator Dictionary().get("color", Color());

			const HashMap<Color, String>::ConstIterator E = color_map.find(color);
			if (!E) {
				error = vformat("Invalid color %s at column %d.", color, column + 1);
				break;
			}
			const String symbol = E->value;

			if (symbols.length() < column) {
				symbols += String(" ").repeat(column - symbols.length());
			} else if (symbols.length() > column) {
				error = vformat("Symbol %s does not fit in column %d.", symbol, column + 1);
				break;
			}

			symbols += symbol;
		}

#define ADD_LINE(m_text) r_output += (m_text).strip_edges(false, true) + "\n";

		ADD_LINE(vformat("%04d %s", p_line_idx + 1, p_line_str.replace("\t", "    ")));
		ADD_LINE(vformat(">>>> %s", symbols));
		if (!error.is_empty()) {
			ADD_LINE(vformat(">>>> ERROR: %s", error));
		}

#undef ADD_LINE
	}

	static void test_directory(const String &p_dir_path) {
		Error err = OK;
		Ref<DirAccess> dir = DirAccess::open(p_dir_path, &err);
		if (err != OK) {
			FAIL(vformat(R"(Cannot open directory "%s".)", p_dir_path));
			return;
		}

		dir->list_dir_begin();
		while (true) {
			const String item_name = dir->get_next();
			if (item_name.is_empty()) {
				break;
			}

			const String path = dir->get_current_dir().path_join(item_name);

			if (dir->current_is_dir()) {
				if (item_name != "." && item_name != "..") {
					test_directory(path);
				}
			} else if (item_name.get_extension() == "gd") {
				Ref<GDScript> gdscript;
				gdscript.instantiate();
				gdscript->set_path(path);
				gdscript->load_source_code(path);

				text_edit->set_text(gdscript->get_source_code());
				highlighter->_set_edited_resource(gdscript);
				highlighter->_update_cache();

				String output;
				for (int i = 0; i < text_edit->get_line_count(); i++) {
					Dictionary dict = highlighter->_get_line_syntax_highlighting_impl(i);
					make_output(i, text_edit->get_line(i), dict, output);
				}

				const String out_path = path.get_basename() + ".out";
				const Ref<FileAccess> out_file = FileAccess::open(out_path, FileAccess::READ, &err);
				if (err != OK) {
					FAIL(vformat(R"(Cannot open file "%s".)", out_path));
					return;
				}

				const String expected = out_file->get_as_utf8_string();
				const bool result = output == expected;
				CHECK_MESSAGE(result, vformat("%s\n\n%s", path, output));
			}
		}
	}

public:
	static void test() {
		text_edit = memnew(TextEdit);
		highlighter.instantiate();
		highlighter->set_text_edit(text_edit);

		struct ColorInfo {
			const char *symbol;
			const char *setting;
		};

		constexpr ColorInfo COLOR_INFO[] = {
			/* clang-format off */
			{ "N",     ""                                                                  },
			{ "S",     "text_editor/theme/highlighting/symbol_color"                       },
			{ "Kw",    "text_editor/theme/highlighting/keyword_color"                      },
			{ "Cf",    "text_editor/theme/highlighting/control_flow_keyword_color"         },
			{ "Bt",    "text_editor/theme/highlighting/base_type_color"                    },
			{ "Et",    "text_editor/theme/highlighting/engine_type_color"                  },
			{ "Ut",    "text_editor/theme/highlighting/user_type_color"                    },
			{ "Com",   "text_editor/theme/highlighting/comment_color"                      },
			{ "Doc",   "text_editor/theme/highlighting/doc_comment_color"                  },
			{ "Str",   "text_editor/theme/highlighting/string_color"                       },
			{ "Num",   "text_editor/theme/highlighting/number_color"                       },
			{ "Fn",    "text_editor/theme/highlighting/function_color"                     },
			{ "Mem",   "text_editor/theme/highlighting/member_variable_color"              },
			{ "Rgn",   "text_editor/theme/highlighting/folded_code_region_color"           },
			{ "Fndef", "text_editor/theme/highlighting/gdscript/function_definition_color" },
			{ "Gfn",   "text_editor/theme/highlighting/gdscript/global_function_color"     },
			{ "Npath", "text_editor/theme/highlighting/gdscript/node_path_color"           },
			{ "Nref",  "text_editor/theme/highlighting/gdscript/node_reference_color"      },
			{ "Annot", "text_editor/theme/highlighting/gdscript/annotation_color"          },
			{ "Sname", "text_editor/theme/highlighting/gdscript/string_name_color"         },
			{ "Mcrit", "text_editor/theme/highlighting/comment_markers/critical_color"     },
			{ "Mwarn", "text_editor/theme/highlighting/comment_markers/warning_color"      },
			{ "Mnote", "text_editor/theme/highlighting/comment_markers/notice_color"       },
			/* clang-format on */
		};

		for (long unsigned int i = 0; i < sizeof(COLOR_INFO) / sizeof(COLOR_INFO[0]); i++) {
			Color color(0, 0, 0.01 * (i + 1));
			color_map[color] = COLOR_INFO[i].symbol;
			if (i == 0) {
				text_edit->add_theme_color_override("font_color", color);
			} else {
				EditorSettings::get_singleton()->set_setting(COLOR_INFO[i].setting, color);
			}
		}

		init_language("modules/gdscript/tests/scripts");

		test_directory("modules/gdscript/tests/scripts/highlighter");

		color_map.clear();
		memdelete(text_edit);
		text_edit = nullptr;
		highlighter.unref();
	}
};

HashMap<Color, String> TestHighlighter::color_map;
TextEdit *TestHighlighter::text_edit = nullptr;
Ref<GDScriptSyntaxHighlighter> TestHighlighter::highlighter;

TEST_SUITE("[Modules][GDScript][Highlighter]") {
	TEST_CASE("[Editor] Check GDScript highlighter") {
		TestHighlighter::test();
	}
}

} // namespace GDScriptTests

#endif // TOOLS_ENABLED

#endif // TEST_HIGHLIGHTER_H
