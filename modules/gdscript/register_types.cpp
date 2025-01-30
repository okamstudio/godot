/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "gdscript.h"
#include "gdscript_analyzer.h"
#include "gdscript_cache.h"
#include "gdscript_parser.h"
#include "gdscript_tokenizer_buffer.h"
#include "gdscript_utility_functions.h"

#ifdef TOOLS_ENABLED
#include "editor/gdscript_highlighter.h"
#include "editor/gdscript_translation_parser_plugin.h"

#ifndef GDSCRIPT_NO_LSP
#include "language_server/gdscript_language_server.h"
#endif
#endif // TOOLS_ENABLED

#ifdef TESTS_ENABLED
#include "tests/test_gdscript.h"
#endif

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/editor_translation_parser.h"
#include "editor/export/editor_export.h"

#ifndef GDSCRIPT_NO_LSP
#include "core/config/engine.h"
#endif
#endif // TOOLS_ENABLED

#ifdef TESTS_ENABLED
#include "tests/test_macros.h"
#endif

GDScriptLanguage *script_language_gd = nullptr;
Ref<ResourceFormatLoaderGDScript> resource_loader_gd;
Ref<ResourceFormatSaverGDScript> resource_saver_gd;
GDScriptCache *gdscript_cache = nullptr;

#ifdef TOOLS_ENABLED

Ref<GDScriptEditorTranslationParserPlugin> gdscript_translation_parser_plugin;

class EditorExportGDScript : public EditorExportPlugin {
	GDCLASS(EditorExportGDScript, EditorExportPlugin);

	uint32_t customization_hash = 0;
	HashSet<String> features;

	static constexpr int DEFAULT_SCRIPT_MODE = EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED;
	int script_mode = DEFAULT_SCRIPT_MODE;

protected:
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override {
		script_mode = DEFAULT_SCRIPT_MODE;

		const Ref<EditorExportPreset> &preset = get_export_preset();
		if (preset.is_valid()) {
			script_mode = preset->get_script_export_mode();
		}

		// If features change, the scripts must be reanalyzed.
		for (const String &feature : p_features) {
			customization_hash = hash_murmur3_one_64(feature.hash64(), customization_hash);
		}
		features = p_features;
	}

	virtual uint64_t _get_customization_configuration_hash() const override {
		return customization_hash;
	}

	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override {
		if (p_path.get_extension() != "gd") {
			return;
		}

		PackedByteArray source_bytes = FileAccess::get_file_as_bytes(p_path);
		if (source_bytes.is_empty()) {
			return;
		}

		String source;
		source.parse_utf8(reinterpret_cast<const char *>(source_bytes.ptr()), source_bytes.size());

		// Avoid parsing if the script doesn't look like it actually uses @if_features.
		bool source_changed = false;
		if (source.contains("@if_features")) {
			// 1. Parse and analyze script, as little as needed to have annotations processed.
			GDScriptParserRef parser;
			{
				parser.set_path(ResourceLoader::path_remap(p_path));
				Error err = parser.get_parser()->parse(source, p_path, false);
				ERR_FAIL_COND(err);
				parser.get_parser()->set_export_features(p_features); // Needed for the analyzer step. If done early, parser's clear would remove this info.
				err = parser.get_analyzer()->resolve_interface(); // Enough for annotations to be applied.
				ERR_FAIL_COND(err);
			}

			// 2. Strip functions unfitting @if_features.
			{
				LocalVector<Pair<GDScriptParser::ClassNode *, GDScriptParser::FunctionNode *>> unfitting_functions;
				parser.get_parser()->collect_unfitting_functions(parser.get_parser()->get_tree(), unfitting_functions);
				if (unfitting_functions.size()) {
					Vector<String> lines = source.split("\n");
					for (const Pair<GDScriptParser::ClassNode *, GDScriptParser::FunctionNode *> &class_and_func : unfitting_functions) {
						GDScriptParser::FunctionNode *function = class_and_func.second;
						const String &class_part = class_and_func.first->identifier ? String(class_and_func.first->identifier->name) + ":" : String();

						// Strip annotations as well (not covered by function's start_line).
						int start_line = function->start_line;
						if (function->annotations.size()) {
							start_line = function->annotations.front()->get()->start_line;
						}

						print_verbose(vformat("Stripping function %s%s:%s (%d-%d)", p_path, class_part, function->identifier->name, start_line, function->end_line));
						for (int i = start_line - 1; i <= function->end_line - 1; i++) {
							lines.write[i] = "";
						}
					}
					source = String("\n").join(lines);
					source_changed = true;
				}
			}
		}

		PackedByteArray res;
		if (script_mode == EditorExportPreset::MODE_SCRIPT_TEXT) {
			if (source_changed) {
				skip();
				source_bytes = source.to_utf8_buffer();
				add_file(p_path, source_bytes, false);
			}
		} else {
			GDScriptTokenizerBuffer::CompressMode compress_mode = script_mode == EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED ? GDScriptTokenizerBuffer::COMPRESS_ZSTD : GDScriptTokenizerBuffer::COMPRESS_NONE;
			source_bytes = GDScriptTokenizerBuffer::parse_code_string(source, compress_mode);
			if (source_bytes.is_empty()) {
				return;
			}

			add_file(p_path.get_basename() + ".gdc", source_bytes, true);
		}
	}

public:
	virtual String get_name() const override { return "GDScript"; }
};

static void _editor_init() {
	Ref<EditorExportGDScript> gd_export;
	gd_export.instantiate();
	EditorExport::get_singleton()->add_export_plugin(gd_export);

#ifdef TOOLS_ENABLED
	Ref<GDScriptSyntaxHighlighter> gdscript_syntax_highlighter;
	gdscript_syntax_highlighter.instantiate();
	ScriptEditor::get_singleton()->register_syntax_highlighter(gdscript_syntax_highlighter);
#endif

#ifndef GDSCRIPT_NO_LSP
	register_lsp_types();
	GDScriptLanguageServer *lsp_plugin = memnew(GDScriptLanguageServer);
	EditorNode::get_singleton()->add_editor_plugin(lsp_plugin);
	Engine::get_singleton()->add_singleton(Engine::Singleton("GDScriptLanguageProtocol", GDScriptLanguageProtocol::get_singleton()));
#endif // !GDSCRIPT_NO_LSP
}

#endif // TOOLS_ENABLED

void initialize_gdscript_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		GDREGISTER_CLASS(GDScript);

		script_language_gd = memnew(GDScriptLanguage);
		ScriptServer::register_language(script_language_gd);

		resource_loader_gd.instantiate();
		ResourceLoader::add_resource_format_loader(resource_loader_gd);

		resource_saver_gd.instantiate();
		ResourceSaver::add_resource_format_saver(resource_saver_gd);

		gdscript_cache = memnew(GDScriptCache);

		GDScriptUtilityFunctions::register_functions();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		EditorNode::add_init_callback(_editor_init);

		gdscript_translation_parser_plugin.instantiate();
		EditorTranslationParser::get_singleton()->add_parser(gdscript_translation_parser_plugin, EditorTranslationParser::STANDARD);
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		ClassDB::APIType prev_api = ClassDB::get_current_api();
		ClassDB::set_current_api(ClassDB::API_EDITOR);

		GDREGISTER_CLASS(GDScriptSyntaxHighlighter);

		ClassDB::set_current_api(prev_api);
	}
#endif // TOOLS_ENABLED
}

void uninitialize_gdscript_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		ScriptServer::unregister_language(script_language_gd);

		if (gdscript_cache) {
			memdelete(gdscript_cache);
		}

		if (script_language_gd) {
			memdelete(script_language_gd);
		}

		ResourceLoader::remove_resource_format_loader(resource_loader_gd);
		resource_loader_gd.unref();

		ResourceSaver::remove_resource_format_saver(resource_saver_gd);
		resource_saver_gd.unref();

		GDScriptParser::cleanup();
		GDScriptUtilityFunctions::unregister_functions();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorTranslationParser::get_singleton()->remove_parser(gdscript_translation_parser_plugin, EditorTranslationParser::STANDARD);
		gdscript_translation_parser_plugin.unref();
	}
#endif // TOOLS_ENABLED
}

#ifdef TESTS_ENABLED
void test_tokenizer() {
	GDScriptTests::test(GDScriptTests::TestType::TEST_TOKENIZER);
}

void test_tokenizer_buffer() {
	GDScriptTests::test(GDScriptTests::TestType::TEST_TOKENIZER_BUFFER);
}

void test_parser() {
	GDScriptTests::test(GDScriptTests::TestType::TEST_PARSER);
}

void test_compiler() {
	GDScriptTests::test(GDScriptTests::TestType::TEST_COMPILER);
}

void test_bytecode() {
	GDScriptTests::test(GDScriptTests::TestType::TEST_BYTECODE);
}

REGISTER_TEST_COMMAND("gdscript-tokenizer", &test_tokenizer);
REGISTER_TEST_COMMAND("gdscript-tokenizer-buffer", &test_tokenizer_buffer);
REGISTER_TEST_COMMAND("gdscript-parser", &test_parser);
REGISTER_TEST_COMMAND("gdscript-compiler", &test_compiler);
REGISTER_TEST_COMMAND("gdscript-bytecode", &test_bytecode);
#endif
