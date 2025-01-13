/**************************************************************************/
/*  gdscript_text_document.cpp                                            */
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

#include "gdscript_text_document.h"

#include "../gdscript.h"
#include "gdscript_extend_parser.h"
#include "gdscript_language_protocol.h"

#include "editor/editor_settings.h"
#include "editor/plugins/script_text_editor.h"
#include "servers/display_server.h"

void GDScriptTextDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("didOpen"), &GDScriptTextDocument::didOpen);
	ClassDB::bind_method(D_METHOD("didClose"), &GDScriptTextDocument::didClose);
	ClassDB::bind_method(D_METHOD("didChange"), &GDScriptTextDocument::didChange);
	ClassDB::bind_method(D_METHOD("willSaveWaitUntil"), &GDScriptTextDocument::willSaveWaitUntil);
	ClassDB::bind_method(D_METHOD("didSave"), &GDScriptTextDocument::didSave);
	ClassDB::bind_method(D_METHOD("nativeSymbol"), &GDScriptTextDocument::nativeSymbol);
	ClassDB::bind_method(D_METHOD("documentSymbol"), &GDScriptTextDocument::documentSymbol);
	ClassDB::bind_method(D_METHOD("completion"), &GDScriptTextDocument::completion);
	ClassDB::bind_method(D_METHOD("resolve"), &GDScriptTextDocument::resolve);
	ClassDB::bind_method(D_METHOD("rename"), &GDScriptTextDocument::rename);
	ClassDB::bind_method(D_METHOD("prepareRename"), &GDScriptTextDocument::prepareRename);
	ClassDB::bind_method(D_METHOD("references"), &GDScriptTextDocument::references);
	ClassDB::bind_method(D_METHOD("foldingRange"), &GDScriptTextDocument::foldingRange);
	ClassDB::bind_method(D_METHOD("codeLens"), &GDScriptTextDocument::codeLens);
	ClassDB::bind_method(D_METHOD("documentLink"), &GDScriptTextDocument::documentLink);
	ClassDB::bind_method(D_METHOD("colorPresentation"), &GDScriptTextDocument::colorPresentation);
	ClassDB::bind_method(D_METHOD("hover"), &GDScriptTextDocument::hover);
	ClassDB::bind_method(D_METHOD("definition"), &GDScriptTextDocument::definition);
	ClassDB::bind_method(D_METHOD("declaration"), &GDScriptTextDocument::declaration);
	ClassDB::bind_method(D_METHOD("signatureHelp"), &GDScriptTextDocument::signatureHelp);
	ClassDB::bind_method(D_METHOD("show_native_symbol_in_editor"), &GDScriptTextDocument::show_native_symbol_in_editor);
}

void GDScriptTextDocument::didOpen(const Variant &p_param) {
	lsp::TextDocumentItem doc = load_document_item(p_param);
	sync_script_content(doc.uri, doc.text);
}

void GDScriptTextDocument::didClose(const Variant &p_param) {
	// Left empty on purpose. Godot does nothing special on closing a document,
	// but it satisfies LSP clients that require didClose be implemented.
}

void GDScriptTextDocument::didChange(const Variant &p_param) {
	lsp::TextDocumentItem doc = load_document_item(p_param);
	Dictionary dict = p_param;
	Array contentChanges = dict["contentChanges"];
	for (int i = 0; i < contentChanges.size(); ++i) {
		lsp::TextDocumentContentChangeEvent evt;
		evt.load(contentChanges[i]);
		doc.text = evt.text;
	}
	sync_script_content(doc.uri, doc.text);
}

void GDScriptTextDocument::willSaveWaitUntil(const Variant &p_param) {
	lsp::TextDocumentItem doc = load_document_item(p_param);

	String path = GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_path(doc.uri);
	Ref<Script> scr = ResourceLoader::load(path);
	if (scr.is_valid()) {
		ScriptEditor::get_singleton()->clear_docs_from_script(scr);
	}
}

void GDScriptTextDocument::didSave(const Variant &p_param) {
	lsp::TextDocumentItem doc = load_document_item(p_param);
	Dictionary dict = p_param;
	String text = dict["text"];

	sync_script_content(doc.uri, text);

	String path = GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_path(doc.uri);
	Ref<GDScript> scr = ResourceLoader::load(path);
	if (scr.is_valid() && (scr->load_source_code(path) == OK)) {
		if (scr->is_tool()) {
			scr->get_language()->reload_tool_script(scr, true);
		} else {
			scr->reload(true);
		}

		scr->update_exports();

		if (!Thread::is_main_thread()) {
			callable_mp(this, &GDScriptTextDocument::reload_script).call_deferred(scr);
		} else {
			reload_script(scr);
		}
	}
}

void GDScriptTextDocument::reload_script(Ref<GDScript> p_to_reload_script) {
	ScriptEditor::get_singleton()->reload_scripts(true);
	ScriptEditor::get_singleton()->update_docs_from_script(p_to_reload_script);
	ScriptEditor::get_singleton()->trigger_live_script_reload(p_to_reload_script->get_path());
}

lsp::TextDocumentItem GDScriptTextDocument::load_document_item(const Variant &p_param) {
	lsp::TextDocumentItem doc;
	Dictionary params = p_param;
	doc.load(params["textDocument"]);
	return doc;
}

void GDScriptTextDocument::notify_client_show_symbol(const lsp::DocumentSymbol *symbol) {
	ERR_FAIL_NULL(symbol);
	GDScriptLanguageProtocol::get_singleton()->notify_client("gdscript/show_native_symbol", symbol->to_json(true));
}

void GDScriptTextDocument::initialize() {
	if (GDScriptLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {
		for (const KeyValue<StringName, ClassMembers> &E : GDScriptLanguageProtocol::get_singleton()->get_workspace()->native_members) {
			const ClassMembers &members = E.value;

			for (const KeyValue<String, const lsp::DocumentSymbol *> &F : members) {
				const lsp::DocumentSymbol *symbol = members.get(F.key);
				lsp::CompletionItem item = symbol->make_completion_item();
				item.data = JOIN_SYMBOLS(String(E.key), F.key);
				native_member_completions.push_back(item.to_json());
			}
		}
	}
}

Variant GDScriptTextDocument::nativeSymbol(const Dictionary &p_params) {
	Variant ret;

	lsp::NativeSymbolInspectParams params;
	params.load(p_params);

	if (const lsp::DocumentSymbol *symbol = GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_native_symbol(params)) {
		ret = symbol->to_json(true);
		notify_client_show_symbol(symbol);
	}

	return ret;
}

Array GDScriptTextDocument::documentSymbol(const Dictionary &p_params) {
	Dictionary params = p_params["textDocument"];
	String uri = params["uri"];
	String path = GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_path(uri);
	Array arr;
	if (HashMap<String, ExtendGDScriptParser *>::ConstIterator parser = GDScriptLanguageProtocol::get_singleton()->get_workspace()->scripts.find(path)) {
		lsp::DocumentSymbol symbol = parser->value->get_symbols();
		arr.push_back(symbol.to_json(true));
	}
	return arr;
}

Array GDScriptTextDocument::completion(const Dictionary &p_params) {
	Array arr;

	lsp::CompletionParams params;
	params.load(p_params);
	Dictionary request_data = params.to_json();

	List<ScriptLanguage::CodeCompletionOption> options;
	GDScriptLanguageProtocol::get_singleton()->get_workspace()->completion(params, &options);

	if (!options.is_empty()) {
		int i = 0;
		arr.resize(options.size());

		for (const ScriptLanguage::CodeCompletionOption &option : options) {
			lsp::CompletionItem item;
			item.label = option.display;
			item.data = request_data;
			item.insertText = option.insert_text;

			switch (option.kind) {
				case ScriptLanguage::CODE_COMPLETION_KIND_ENUM:
					item.kind = lsp::CompletionItemKind::Enum;
					break;
				case ScriptLanguage::CODE_COMPLETION_KIND_CLASS:
					item.kind = lsp::CompletionItemKind::Class;
					break;
				case ScriptLanguage::CODE_COMPLETION_KIND_MEMBER:
					item.kind = lsp::CompletionItemKind::Property;
					break;
				case ScriptLanguage::CODE_COMPLETION_KIND_FUNCTION:
					item.kind = lsp::CompletionItemKind::Method;
					break;
				case ScriptLanguage::CODE_COMPLETION_KIND_SIGNAL:
					item.kind = lsp::CompletionItemKind::Event;
					break;
				case ScriptLanguage::CODE_COMPLETION_KIND_CONSTANT:
					item.kind = lsp::CompletionItemKind::Constant;
					break;
				case ScriptLanguage::CODE_COMPLETION_KIND_VARIABLE:
					item.kind = lsp::CompletionItemKind::Variable;
					break;
				case ScriptLanguage::CODE_COMPLETION_KIND_FILE_PATH:
					item.kind = lsp::CompletionItemKind::File;
					break;
				case ScriptLanguage::CODE_COMPLETION_KIND_NODE_PATH:
					item.kind = lsp::CompletionItemKind::Snippet;
					break;
				case ScriptLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT:
					item.kind = lsp::CompletionItemKind::Text;
					break;
				default: {
				}
			}

			arr[i] = item.to_json();
			i++;
		}
	}
	return arr;
}

Dictionary GDScriptTextDocument::rename(const Dictionary &p_params) {
	lsp::TextDocumentPositionParams params;
	params.load(p_params);
	String new_name = p_params["newName"];

	return GDScriptLanguageProtocol::get_singleton()->get_workspace()->rename(params, new_name);
}

Variant GDScriptTextDocument::prepareRename(const Dictionary &p_params) {
	lsp::TextDocumentPositionParams params;
	params.load(p_params);

	lsp::DocumentSymbol symbol;
	lsp::Range range;
	if (GDScriptLanguageProtocol::get_singleton()->get_workspace()->can_rename(params, symbol, range)) {
		return Variant(range.to_json());
	}

	// `null` -> rename not valid at current location.
	return Variant();
}

Array GDScriptTextDocument::references(const Dictionary &p_params) {
	Array res;

	lsp::ReferenceParams params;
	params.load(p_params);

	const lsp::DocumentSymbol *symbol = GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_symbol(params);
	if (symbol) {
		Vector<lsp::Location> usages = GDScriptLanguageProtocol::get_singleton()->get_workspace()->find_all_usages(*symbol);
		res.resize(usages.size());
		int declaration_adjustment = 0;
		for (int i = 0; i < usages.size(); i++) {
			lsp::Location usage = usages[i];
			if (!params.context.includeDeclaration && usage.range == symbol->range) {
				declaration_adjustment++;
				continue;
			}
			res[i - declaration_adjustment] = usages[i].to_json();
		}

		if (declaration_adjustment > 0) {
			res.resize(res.size() - declaration_adjustment);
		}
	}

	return res;
}

Dictionary GDScriptTextDocument::resolve(const Dictionary &p_params) {
	lsp::CompletionItem item;
	item.load(p_params);

	lsp::CompletionParams params;
	Variant data = p_params["data"];

	const lsp::DocumentSymbol *symbol = nullptr;

	if (data.get_type() == Variant::DICTIONARY) {
		params.load(p_params["data"]);
		symbol = GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_symbol(params, item.label, item.kind == lsp::CompletionItemKind::Method || item.kind == lsp::CompletionItemKind::Function);

	} else if (data.is_string()) {
		String query = data;

		Vector<String> param_symbols = query.split(SYMBOL_SEPARATOR, false);

		if (param_symbols.size() >= 2) {
			StringName class_name = param_symbols[0];
			const String &member_name = param_symbols[param_symbols.size() - 1];
			String inner_class_name;
			if (param_symbols.size() >= 3) {
				inner_class_name = param_symbols[1];
			}

			if (const ClassMembers *members = GDScriptLanguageProtocol::get_singleton()->get_workspace()->native_members.getptr(class_name)) {
				if (const lsp::DocumentSymbol *const *member = members->getptr(member_name)) {
					symbol = *member;
				}
			}

			if (!symbol) {
				if (HashMap<String, ExtendGDScriptParser *>::ConstIterator E = GDScriptLanguageProtocol::get_singleton()->get_workspace()->scripts.find(class_name)) {
					symbol = E->value->get_member_symbol(member_name, inner_class_name);
				}
			}
		}
	}

	if (symbol) {
		item.documentation = symbol->render();
	}

	if (item.kind == lsp::CompletionItemKind::Event) {
		if (params.context.triggerKind == lsp::CompletionTriggerKind::TriggerCharacter && (params.context.triggerCharacter == "(")) {
			const String quote_style = EDITOR_GET("text_editor/completion/use_single_quotes") ? "'" : "\"";
			item.insertText = item.label.quote(quote_style);
		}
	}

	if (item.kind == lsp::CompletionItemKind::Method) {
		bool is_trigger_character = params.context.triggerKind == lsp::CompletionTriggerKind::TriggerCharacter;
		bool is_quote_character = params.context.triggerCharacter == "\"" || params.context.triggerCharacter == "'";

		if (is_trigger_character && is_quote_character && item.insertText.is_quoted()) {
			item.insertText = item.insertText.unquote();
		}
	}

	return item.to_json(true);
}

Array GDScriptTextDocument::foldingRange(const Dictionary &p_params) {
	Array arr;
	return arr;
}

Array GDScriptTextDocument::codeLens(const Dictionary &p_params) {
	Array arr;
	return arr;
}

Array GDScriptTextDocument::documentLink(const Dictionary &p_params) {
	Array ret;

	lsp::DocumentLinkParams params;
	params.load(p_params);

	List<lsp::DocumentLink> links;
	GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_document_links(params.textDocument.uri, links);
	for (const lsp::DocumentLink &E : links) {
		ret.push_back(E.to_json());
	}
	return ret;
}

Array GDScriptTextDocument::colorPresentation(const Dictionary &p_params) {
	Array arr;
	return arr;
}

Variant GDScriptTextDocument::hover(const Dictionary &p_params) {
	lsp::TextDocumentPositionParams params;
	params.load(p_params);

	const lsp::DocumentSymbol *symbol = GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_symbol(params);
	if (symbol) {
		lsp::Hover hover;
		hover.contents = symbol->render();
		hover.range.start = params.position;
		hover.range.end = params.position;
		return hover.to_json();

	} else if (GDScriptLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {
		Dictionary ret;
		Array contents;
		List<const lsp::DocumentSymbol *> list;
		GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_related_symbols(params, list);
		for (const lsp::DocumentSymbol *&E : list) {
			if (const lsp::DocumentSymbol *s = E) {
				contents.push_back(s->render().value);
			}
		}
		ret["contents"] = contents;
		return ret;
	}

	return Variant();
}

Array GDScriptTextDocument::definition(const Dictionary &p_params) {
	lsp::TextDocumentPositionParams params;
	params.load(p_params);
	List<const lsp::DocumentSymbol *> symbols;
	Array arr = find_symbols(params, symbols);
	return arr;
}

Variant GDScriptTextDocument::declaration(const Dictionary &p_params) {
	lsp::TextDocumentPositionParams params;
	params.load(p_params);
	List<const lsp::DocumentSymbol *> symbols;
	Array arr = find_symbols(params, symbols);
	if (arr.is_empty() && !symbols.is_empty() && !symbols.front()->get()->native_class.is_empty()) { // Find a native symbol
		const lsp::DocumentSymbol *symbol = symbols.front()->get();
		if (GDScriptLanguageProtocol::get_singleton()->is_goto_native_symbols_enabled()) {
			String id;
			switch (symbol->kind) {
				case lsp::SymbolKind::Class:
					id = "class_name:" + symbol->name;
					break;
				case lsp::SymbolKind::Constant:
					id = "class_constant:" + symbol->native_class + ":" + symbol->name;
					break;
				case lsp::SymbolKind::Property:
				case lsp::SymbolKind::Variable:
					id = "class_property:" + symbol->native_class + ":" + symbol->name;
					break;
				case lsp::SymbolKind::Enum:
					id = "class_enum:" + symbol->native_class + ":" + symbol->name;
					break;
				case lsp::SymbolKind::Method:
				case lsp::SymbolKind::Function:
					id = "class_method:" + symbol->native_class + ":" + symbol->name;
					break;
				default:
					id = "class_global:" + symbol->native_class + ":" + symbol->name;
					break;
			}
			callable_mp(this, &GDScriptTextDocument::show_native_symbol_in_editor).call_deferred(id);
		} else {
			notify_client_show_symbol(symbol);
		}
	}
	return arr;
}

Variant GDScriptTextDocument::signatureHelp(const Dictionary &p_params) {
	Variant ret;

	lsp::TextDocumentPositionParams params;
	params.load(p_params);

	lsp::SignatureHelp s;
	if (OK == GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_signature(params, s)) {
		ret = s.to_json();
	}

	return ret;
}

GDScriptTextDocument::GDScriptTextDocument() {
	file_checker = FileAccess::create(FileAccess::ACCESS_RESOURCES);
}

void GDScriptTextDocument::sync_script_content(const String &p_path, const String &p_content) {
	String path = GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_path(p_path);
	GDScriptLanguageProtocol::get_singleton()->get_workspace()->parse_script(path, p_content);
}

void GDScriptTextDocument::show_native_symbol_in_editor(const String &p_symbol_id) {
	callable_mp(ScriptEditor::get_singleton(), &ScriptEditor::goto_help).call_deferred(p_symbol_id);

	DisplayServer::get_singleton()->window_move_to_foreground();
}

Array GDScriptTextDocument::find_symbols(const lsp::TextDocumentPositionParams &p_location, List<const lsp::DocumentSymbol *> &r_list) {
	Array arr;
	const lsp::DocumentSymbol *symbol = GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_symbol(p_location);
	if (symbol) {
		lsp::Location location;
		location.uri = symbol->uri;
		location.range = symbol->selectionRange;
		const String &path = GDScriptLanguageProtocol::get_singleton()->get_workspace()->get_file_path(symbol->uri);
		if (file_checker->file_exists(path)) {
			arr.push_back(location.to_json());
		}
		r_list.push_back(symbol);
	} else if (GDScriptLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {
		List<const lsp::DocumentSymbol *> list;
		GDScriptLanguageProtocol::get_singleton()->get_workspace()->resolve_related_symbols(p_location, list);
		for (const lsp::DocumentSymbol *&E : list) {
			if (const lsp::DocumentSymbol *s = E) {
				if (!s->uri.is_empty()) {
					lsp::Location location;
					location.uri = s->uri;
					location.range = s->selectionRange;
					arr.push_back(location.to_json());
					r_list.push_back(s);
				}
			}
		}
	}
	return arr;
}
