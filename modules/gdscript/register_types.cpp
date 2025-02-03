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

#include "core/io/file_access_encrypted.h"
#include "core/io/resource_loader.h"
#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "gdscript.h"
#include "gdscript_error_checker.h"
#include "gdscript_tokenizer.h"

GDScriptLanguage *script_language_gd = nullptr;
Ref<ResourceFormatLoaderGDScript> resource_loader_gd;
Ref<ResourceFormatSaverGDScript> resource_saver_gd;

#ifdef TOOLS_ENABLED

#include "editor/editor_export.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/gdscript_highlighter.h"

#ifndef GDSCRIPT_NO_LSP
#include "core/engine.h"
#include "language_server/gdscript_language_server.h"
#endif // !GDSCRIPT_NO_LSP

class EditorExportGDScript : public EditorExportPlugin {
	GDCLASS(EditorExportGDScript, EditorExportPlugin);

public:
	virtual void _export_file(const String &p_path, const String &p_type, const Set<String> &p_features) {
		int script_mode = EditorExportPreset::MODE_SCRIPT_COMPILED;
		String script_key;

		const Ref<EditorExportPreset> &preset = get_export_preset();

		if (preset.is_valid()) {
			script_mode = preset->get_script_export_mode();
			script_key = preset->get_script_encryption_key().to_lower();
		}

		if (!p_path.ends_with(".gd") || script_mode == EditorExportPreset::MODE_SCRIPT_TEXT) {
			return;
		}

		Vector<uint8_t> file = FileAccess::get_file_as_array(p_path);
		if (file.empty()) {
			return;
		}

		String txt;
		txt.parse_utf8((const char *)file.ptr(), file.size());
		file = GDScriptTokenizerBuffer::parse_code_string(txt);

		if (!file.empty()) {
			if (script_mode == EditorExportPreset::MODE_SCRIPT_ENCRYPTED) {
				String tmp_path = EditorSettings::get_singleton()->get_cache_dir().plus_file("script.gde");
				FileAccess *fa = FileAccess::open(tmp_path, FileAccess::WRITE);

				Vector<uint8_t> key;
				key.resize(32);
				for (int i = 0; i < 32; i++) {
					int v = 0;
					if (i * 2 < script_key.length()) {
						CharType ct = script_key[i * 2];
						if (ct >= '0' && ct <= '9') {
							ct = ct - '0';
						} else if (ct >= 'a' && ct <= 'f') {
							ct = 10 + ct - 'a';
						}
						v |= ct << 4;
					}

					if (i * 2 + 1 < script_key.length()) {
						CharType ct = script_key[i * 2 + 1];
						if (ct >= '0' && ct <= '9') {
							ct = ct - '0';
						} else if (ct >= 'a' && ct <= 'f') {
							ct = 10 + ct - 'a';
						}
						v |= ct;
					}
					key.write[i] = v;
				}
				FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
				Error err = fae->open_and_parse(fa, key, FileAccessEncrypted::MODE_WRITE_AES256);

				if (err == OK) {
					fae->store_buffer(file.ptr(), file.size());
				}

				memdelete(fae);

				file = FileAccess::get_file_as_array(tmp_path);
				add_file(p_path.get_basename() + ".gde", file, true);

				// Clean up temporary file.
				DirAccess::remove_file_or_error(tmp_path);

			} else {
				add_file(p_path.get_basename() + ".gdc", file, true);
			}
		}
	}
};

static void _editor_init() {
	Ref<EditorExportGDScript> gd_export;
	gd_export.instance();
	EditorExport::get_singleton()->add_export_plugin(gd_export);

#ifndef GDSCRIPT_NO_LSP
	register_lsp_types();
	GDScriptLanguageServer *lsp_plugin = memnew(GDScriptLanguageServer);
	EditorNode::get_singleton()->add_editor_plugin(lsp_plugin);
	Engine::get_singleton()->add_singleton(Engine::Singleton("GDScriptLanguageProtocol", GDScriptLanguageProtocol::get_singleton()));
#endif // !GDSCRIPT_NO_LSP
}

#endif // TOOLS_ENABLED

void register_gdscript_types() {
	ClassDB::register_class<GDScript>();
	ClassDB::register_virtual_class<GDScriptFunctionState>();
	ClassDB::register_class<GDScriptErrorChecker>();

	script_language_gd = memnew(GDScriptLanguage);
	ScriptServer::register_language(script_language_gd);

	resource_loader_gd.instance();
	ResourceLoader::add_resource_format_loader(resource_loader_gd);

	resource_saver_gd.instance();
	ResourceSaver::add_resource_format_saver(resource_saver_gd);

#ifdef TOOLS_ENABLED
	ScriptEditor::register_create_syntax_highlighter_function(GDScriptSyntaxHighlighter::create);
	EditorNode::add_init_callback(_editor_init);
#endif // TOOLS_ENABLED
}

void unregister_gdscript_types() {
	ScriptServer::unregister_language(script_language_gd);

	if (script_language_gd) {
		memdelete(script_language_gd);
	}

	ResourceLoader::remove_resource_format_loader(resource_loader_gd);
	resource_loader_gd.unref();

	ResourceSaver::remove_resource_format_saver(resource_saver_gd);
	resource_saver_gd.unref();
}
