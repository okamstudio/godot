/**************************************************************************/
/*  godot_lsp.h                                                           */
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

#ifndef GODOT_LSP_H
#define GODOT_LSP_H

#include "core/doc_data.h"
#include "core/object/class_db.h"
#include "core/templates/list.h"

namespace lsp {

typedef String DocumentUri;

/** Format BBCode documentation from DocData to markdown */
static String marked_documentation(const String &p_bbcode);

/**
 * Text documents are identified using a URI. On the protocol level, URIs are passed as strings.
 */
struct TextDocumentIdentifier {
	/**
	 * The text document's URI.
	 */
	DocumentUri uri;

	_FORCE_INLINE_ void load(const Dictionary &p_params) {
		uri = p_params["uri"];
	}

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["uri"] = uri;
		return dict;
	}
};

/**
 * Position in a text document expressed as zero-based line and zero-based character offset.
 * A position is between two characters like an ‘insert’ cursor in a editor.
 * Special values like for example -1 to denote the end of a line are not supported.
 */
struct Position {
	/**
	 * Line position in a document (zero-based).
	 */
	int line = 0;

	/**
	 * Character offset on a line in a document (zero-based). Assuming that the line is
	 * represented as a string, the `character` value represents the gap between the
	 * `character` and `character + 1`.
	 *
	 * If the character value is greater than the line length it defaults back to the
	 * line length.
	 */
	int character = 0;

	_FORCE_INLINE_ bool operator==(const Position &p_other) const {
		return line == p_other.line && character == p_other.character;
	}

	String to_string() const {
		return vformat("(%d,%d)", line, character);
	}

	_FORCE_INLINE_ void load(const Dictionary &p_params) {
		line = p_params["line"];
		character = p_params["character"];
	}

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["line"] = line;
		dict["character"] = character;
		return dict;
	}
};

/**
 * A range in a text document expressed as (zero-based) start and end positions.
 * A range is comparable to a selection in an editor. Therefore the end position is exclusive.
 * If you want to specify a range that contains a line including the line ending character(s) then use an end position denoting the start of the next line.
 */
struct Range {
	/**
	 * The range's start position.
	 */
	Position start;

	/**
	 * The range's end position.
	 */
	Position end;

	_FORCE_INLINE_ bool operator==(const Range &p_other) const {
		return start == p_other.start && end == p_other.end;
	}

	bool contains(const Position &p_pos) const {
		// Inside line range.
		if (start.line <= p_pos.line && p_pos.line <= end.line) {
			// If on start line: must come after start char.
			bool start_ok = p_pos.line == start.line ? start.character <= p_pos.character : true;
			// If on end line: must come before end char.
			bool end_ok = p_pos.line == end.line ? p_pos.character <= end.character : true;
			return start_ok && end_ok;
		} else {
			return false;
		}
	}

	String to_string() const {
		return vformat("[%s:%s]", start.to_string(), end.to_string());
	}

	_FORCE_INLINE_ void load(const Dictionary &p_params) {
		start.load(p_params["start"]);
		end.load(p_params["end"]);
	}

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["start"] = start.to_json();
		dict["end"] = end.to_json();
		return dict;
	}
};

/**
 * Represents a location inside a resource, such as a line inside a text file.
 */
struct Location {
	DocumentUri uri;
	Range range;

	_FORCE_INLINE_ void load(const Dictionary &p_params) {
		uri = p_params["uri"];
		range.load(p_params["range"]);
	}

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["uri"] = uri;
		dict["range"] = range.to_json();
		return dict;
	}
};

/**
 * Represents a link between a source and a target location.
 */
struct LocationLink {
	/**
	 * Span of the origin of this link.
	 *
	 * Used as the underlined span for mouse interaction. Defaults to the word range at
	 * the mouse position.
	 */
	Range *originSelectionRange = nullptr;

	/**
	 * The target resource identifier of this link.
	 */
	String targetUri;

	/**
	 * The full target range of this link. If the target for example is a symbol then target range is the
	 * range enclosing this symbol not including leading/trailing whitespace but everything else
	 * like comments. This information is typically used to highlight the range in the editor.
	 */
	Range targetRange;

	/**
	 * The range that should be selected and revealed when this link is being followed, e.g the name of a function.
	 * Must be contained by the `targetRange`. See also `DocumentSymbol#range`
	 */
	Range targetSelectionRange;
};

/**
 * A parameter literal used in requests to pass a text document and a position inside that document.
 */
struct TextDocumentPositionParams {
	/**
	 * The text document.
	 */
	TextDocumentIdentifier textDocument;

	/**
	 * The position inside the text document.
	 */
	Position position;

	_FORCE_INLINE_ void load(const Dictionary &p_params) {
		textDocument.load(p_params["textDocument"]);
		position.load(p_params["position"]);
	}

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["textDocument"] = textDocument.to_json();
		dict["position"] = position.to_json();
		return dict;
	}
};

struct ReferenceContext {
	/**
	 * Include the declaration of the current symbol.
	 */
	bool includeDeclaration = false;
};

struct ReferenceParams : TextDocumentPositionParams {
	ReferenceContext context;
};

struct DocumentLinkParams {
	/**
	 * The document to provide document links for.
	 */
	TextDocumentIdentifier textDocument;

	_FORCE_INLINE_ void load(const Dictionary &p_params) {
		textDocument.load(p_params["textDocument"]);
	}
};

/**
 * A document link is a range in a text document that links to an internal or external resource, like another
 * text document or a web site.
 */
struct DocumentLink {
	/**
	 * The range this link applies to.
	 */
	Range range;

	/**
	 * The uri this link points to. If missing a resolve request is sent later.
	 */
	DocumentUri target;

	Dictionary to_json() const {
		Dictionary dict;
		dict["range"] = range.to_json();
		dict["target"] = target;
		return dict;
	}
};

/**
 * A textual edit applicable to a text document.
 */
struct TextEdit {
	/**
	 * The range of the text document to be manipulated. To insert
	 * text into a document create a range where start === end.
	 */
	Range range;

	/**
	 * The string to be inserted. For delete operations use an
	 * empty string.
	 */
	String newText;
};

/**
 * The edits to be applied.
 */
struct WorkspaceEdit {
	/**
	 * Holds changes to existing resources.
	 */
	HashMap<String, Vector<TextEdit>> changes;

	_FORCE_INLINE_ void add_edit(const String &uri, const TextEdit &edit) {
		if (changes.has(uri)) {
			changes[uri].push_back(edit);
		} else {
			Vector<TextEdit> edits;
			edits.push_back(edit);
			changes[uri] = edits;
		}
	}

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;

		Dictionary out_changes;
		for (const KeyValue<String, Vector<TextEdit>> &E : changes) {
			Array edits;
			for (int i = 0; i < E.value.size(); ++i) {
				Dictionary text_edit;
				text_edit["range"] = E.value[i].range.to_json();
				text_edit["newText"] = E.value[i].newText;
				edits.push_back(text_edit);
			}
			out_changes[E.key] = edits;
		}
		dict["changes"] = out_changes;

		return dict;
	}

	_FORCE_INLINE_ void add_change(const String &uri, const int &line, const int &start_character, const int &end_character, const String &new_text) {
		TextEdit new_edit;
		new_edit.newText = new_text;
		new_edit.range.start.line = line;
		new_edit.range.start.character = start_character;
		new_edit.range.end.line = line;
		new_edit.range.end.character = end_character;

		if (HashMap<String, Vector<TextEdit>>::Iterator E = changes.find(uri)) {
			E->value.push_back(new_edit);
		} else {
			Vector<TextEdit> edit_list;
			edit_list.push_back(new_edit);
			changes.insert(uri, edit_list);
		}
	}
};

/**
 * Represents a reference to a command.
 * Provides a title which will be used to represent a command in the UI.
 * Commands are identified by a string identifier.
 * The recommended way to handle commands is to implement their execution on the server side if the client and server provides the corresponding capabilities.
 * Alternatively the tool extension code could handle the command. The protocol currently doesn’t specify a set of well-known commands.
 */
struct Command {
	/**
	 * Title of the command, like `save`.
	 */
	String title;
	/**
	 * The identifier of the actual command handler.
	 */
	String command;
	/**
	 * Arguments that the command handler should be
	 * invoked with.
	 */
	Array arguments;

	Dictionary to_json() const {
		Dictionary dict;
		dict["title"] = title;
		dict["command"] = command;
		if (arguments.size()) {
			dict["arguments"] = arguments;
		}
		return dict;
	}
};

// Use namespace instead of enumeration to follow the LSP specifications.
// `lsp::EnumName::EnumValue` is OK but `lsp::EnumValue` is not.

namespace TextDocumentSyncKind {
/**
 * Documents should not be synced at all.
 */
static const int None = 0;

/**
 * Documents are synced by always sending the full content
 * of the document.
 */
static const int Full = 1;

/**
 * Documents are synced by sending the full content on open.
 * After that only incremental updates to the document are
 * send.
 */
static const int Incremental = 2;
}; // namespace TextDocumentSyncKind

/**
 * Completion options.
 */
struct CompletionOptions {
	/**
	 * The server provides support to resolve additional
	 * information for a completion item.
	 */
	bool resolveProvider = true;

	/**
	 * The characters that trigger completion automatically.
	 */
	Vector<String> triggerCharacters;

	CompletionOptions() {
		triggerCharacters.push_back(".");
		triggerCharacters.push_back("$");
		triggerCharacters.push_back("'");
		triggerCharacters.push_back("\"");
	}

	Dictionary to_json() const {
		Dictionary dict;
		dict["resolveProvider"] = resolveProvider;
		dict["triggerCharacters"] = triggerCharacters;
		return dict;
	}
};

/**
 * Signature help options.
 */
struct SignatureHelpOptions {
	/**
	 * The characters that trigger signature help
	 * automatically.
	 */
	Vector<String> triggerCharacters;

	Dictionary to_json() {
		Dictionary dict;
		dict["triggerCharacters"] = triggerCharacters;
		return dict;
	}
};

/**
 * Code Lens options.
 */
struct CodeLensOptions {
	/**
	 * Code lens has a resolve provider as well.
	 */
	bool resolveProvider = false;

	Dictionary to_json() {
		Dictionary dict;
		dict["resolveProvider"] = resolveProvider;
		return dict;
	}
};

/**
 * Rename options
 */
struct RenameOptions {
	/**
	 * Renames should be checked and tested before being executed.
	 */
	bool prepareProvider = true;

	Dictionary to_json() {
		Dictionary dict;
		dict["prepareProvider"] = prepareProvider;
		return dict;
	}
};

/**
 * Document link options.
 */
struct DocumentLinkOptions {
	/**
	 * Document links have a resolve provider as well.
	 */
	bool resolveProvider = false;

	Dictionary to_json() {
		Dictionary dict;
		dict["resolveProvider"] = resolveProvider;
		return dict;
	}
};

/**
 * Execute command options.
 */
struct ExecuteCommandOptions {
	/**
	 * The commands to be executed on the server
	 */
	Vector<String> commands;

	Dictionary to_json() {
		Dictionary dict;
		dict["commands"] = commands;
		return dict;
	}
};

/**
 * Save options.
 */
struct SaveOptions {
	/**
	 * The client is supposed to include the content on save.
	 */
	bool includeText = true;

	Dictionary to_json() {
		Dictionary dict;
		dict["includeText"] = includeText;
		return dict;
	}
};

/**
 * Color provider options.
 */
struct ColorProviderOptions {
	Dictionary to_json() {
		Dictionary dict;
		return dict;
	}
};

/**
 * Folding range provider options.
 */
struct FoldingRangeProviderOptions {
	Dictionary to_json() {
		Dictionary dict;
		return dict;
	}
};

struct TextDocumentSyncOptions {
	/**
	 * Open and close notifications are sent to the server. If omitted open close notification should not
	 * be sent.
	 */
	bool openClose = true;

	/**
	 * Change notifications are sent to the server. See TextDocumentSyncKind.None, TextDocumentSyncKind.Full
	 * and TextDocumentSyncKind.Incremental. If omitted it defaults to TextDocumentSyncKind.None.
	 */
	int change = TextDocumentSyncKind::Full;

	/**
	 * If present will save notifications are sent to the server. If omitted the notification should not be
	 * sent.
	 */
	bool willSave = false;

	/**
	 * If present will save wait until requests are sent to the server. If omitted the request should not be
	 * sent.
	 */
	bool willSaveWaitUntil = true;

	/**
	 * If present save notifications are sent to the server. If omitted the notification should not be
	 * sent.
	 */
	SaveOptions save;

	Dictionary to_json() {
		Dictionary dict;
		dict["willSaveWaitUntil"] = willSaveWaitUntil;
		dict["willSave"] = willSave;
		dict["openClose"] = openClose;
		dict["change"] = change;
		dict["save"] = save.to_json();
		return dict;
	}
};

/**
 * Static registration options to be returned in the initialize request.
 */
struct StaticRegistrationOptions {
	/**
	 * The id used to register the request. The id can be used to deregister
	 * the request again. See also Registration#id.
	 */
	String id;
};

/**
 * Format document on type options.
 */
struct DocumentOnTypeFormattingOptions {
	/**
	 * A character on which formatting should be triggered, like `}`.
	 */
	String firstTriggerCharacter;

	/**
	 * More trigger characters.
	 */
	Vector<String> moreTriggerCharacter;

	Dictionary to_json() {
		Dictionary dict;
		dict["firstTriggerCharacter"] = firstTriggerCharacter;
		dict["moreTriggerCharacter"] = moreTriggerCharacter;
		return dict;
	}
};

struct TextDocumentItem {
	/**
	 * The text document's URI.
	 */
	DocumentUri uri;

	/**
	 * The text document's language identifier.
	 */
	String languageId;

	/**
	 * The version number of this document (it will increase after each
	 * change, including undo/redo).
	 */
	int version = 0;

	/**
	 * The content of the opened text document.
	 */
	String text;

	void load(const Dictionary &p_dict) {
		uri = p_dict["uri"];
		languageId = p_dict["languageId"];
		version = p_dict["version"];
		text = p_dict["text"];
	}

	Dictionary to_json() const {
		Dictionary dict;
		dict["uri"] = uri;
		dict["languageId"] = languageId;
		dict["version"] = version;
		dict["text"] = text;
		return dict;
	}
};

/**
 * An event describing a change to a text document. If range and rangeLength are omitted
 * the new text is considered to be the full content of the document.
 */
struct TextDocumentContentChangeEvent {
	/**
	 * The range of the document that changed.
	 */
	Range range;

	/**
	 * The length of the range that got replaced.
	 */
	int rangeLength = 0;

	/**
	 * The new text of the range/document.
	 */
	String text;

	void load(const Dictionary &p_params) {
		text = p_params["text"];
		rangeLength = p_params["rangeLength"];
		range.load(p_params["range"]);
	}
};

// Use namespace instead of enumeration to follow the LSP specifications
namespace DiagnosticSeverity {
/**
 * Reports an error.
 */
static const int Error = 1;
/**
 * Reports a warning.
 */
static const int Warning = 2;
/**
 * Reports an information.
 */
static const int Information = 3;
/**
 * Reports a hint.
 */
static const int Hint = 4;
}; // namespace DiagnosticSeverity

/**
 * Represents a related message and source code location for a diagnostic. This should be
 * used to point to code locations that cause or related to a diagnostics, e.g when duplicating
 * a symbol in a scope.
 */
struct DiagnosticRelatedInformation {
	/**
	 * The location of this related diagnostic information.
	 */
	Location location;

	/**
	 * The message of this related diagnostic information.
	 */
	String message;

	Dictionary to_json() const {
		Dictionary dict;
		dict["location"] = location.to_json();
		dict["message"] = message;
		return dict;
	}
};

/**
 * Represents a diagnostic, such as a compiler error or warning.
 * Diagnostic objects are only valid in the scope of a resource.
 */
struct Diagnostic {
	/**
	 * The range at which the message applies.
	 */
	Range range;

	/**
	 * The diagnostic's severity. Can be omitted. If omitted it is up to the
	 * client to interpret diagnostics as error, warning, info or hint.
	 */
	int severity = 0;

	/**
	 * The diagnostic's code, which might appear in the user interface.
	 */
	int code = 0;

	/**
	 * A human-readable string describing the source of this
	 * diagnostic, e.g. 'typescript' or 'super lint'.
	 */
	String source;

	/**
	 * The diagnostic's message.
	 */
	String message;

	/**
	 * An array of related diagnostic information, e.g. when symbol-names within
	 * a scope collide all definitions can be marked via this property.
	 */
	Vector<DiagnosticRelatedInformation> relatedInformation;

	Dictionary to_json() const {
		Dictionary dict;
		dict["range"] = range.to_json();
		dict["code"] = code;
		dict["severity"] = severity;
		dict["message"] = message;
		dict["source"] = source;
		if (!relatedInformation.is_empty()) {
			Array arr;
			arr.resize(relatedInformation.size());
			for (int i = 0; i < relatedInformation.size(); i++) {
				arr[i] = relatedInformation[i].to_json();
			}
			dict["relatedInformation"] = arr;
		}
		return dict;
	}
};

// Use namespace instead of enumeration to follow the LSP specifications
/**
 * Describes the content type that a client supports in various
 * result literals like `Hover`, `ParameterInfo` or `CompletionItem`.
 *
 * Please note that `MarkupKinds` must not start with a `$`. This kinds
 * are reserved for internal usage.
 */
namespace MarkupKind {
static const String PlainText = "plaintext";
static const String Markdown = "markdown";
}; // namespace MarkupKind

/**
 * A `MarkupContent` literal represents a string value which content is interpreted base on its
 * kind flag. Currently the protocol supports `plaintext` and `markdown` as markup kinds.
 *
 * If the kind is `markdown` then the value can contain fenced code blocks like in GitHub issues.
 * See https://help.github.com/articles/creating-and-highlighting-code-blocks/#syntax-highlighting
 *
 * Here is an example how such a string can be constructed using JavaScript / TypeScript:
 * ```typescript
 * let markdown: MarkdownContent = {
 *  kind: MarkupKind.Markdown,
 *	value: [
 *		'# Header',
 *		'Some text',
 *		'```typescript',
 *		'someCode();',
 *		'```'
 *	].join('\n')
 * };
 * ```
 *
 * *Please Note* that clients might sanitize the return markdown. A client could decide to
 * remove HTML from the markdown to avoid script execution.
 */
struct MarkupContent {
	/**
	 * The type of the Markup.
	 */
	String kind;

	/**
	 * The content itself.
	 */
	String value;

	MarkupContent() {
		kind = MarkupKind::Markdown;
	}

	MarkupContent(const String &p_value) {
		value = p_value;
		kind = MarkupKind::Markdown;
	}

	Dictionary to_json() const {
		Dictionary dict;
		dict["kind"] = kind;
		dict["value"] = value;
		return dict;
	}
};

// Use namespace instead of enumeration to follow the LSP specifications
// `lsp::EnumName::EnumValue` is OK but `lsp::EnumValue` is not.
// And here C++ compilers are unhappy with our enumeration name like `Color`, `File`, `RefCounted` etc.
/**
 * The kind of a completion entry.
 */
namespace CompletionItemKind {
static const int Text = 1;
static const int Method = 2;
static const int Function = 3;
static const int Constructor = 4;
static const int Field = 5;
static const int Variable = 6;
static const int Class = 7;
static const int Interface = 8;
static const int Module = 9;
static const int Property = 10;
static const int Unit = 11;
static const int Value = 12;
static const int Enum = 13;
static const int Keyword = 14;
static const int Snippet = 15;
static const int Color = 16;
static const int File = 17;
static const int RefCounted = 18;
static const int Folder = 19;
static const int EnumMember = 20;
static const int Constant = 21;
static const int Struct = 22;
static const int Event = 23;
static const int Operator = 24;
static const int TypeParameter = 25;
}; // namespace CompletionItemKind

// Use namespace instead of enumeration to follow the LSP specifications.
/**
 * Defines whether the insert text in a completion item should be interpreted as
 * plain text or a snippet.
 */
namespace InsertTextFormat {
/**
 * The primary text to be inserted is treated as a plain string.
 */
static const int PlainText = 1;

/**
 * The primary text to be inserted is treated as a snippet.
 *
 * A snippet can define tab stops and placeholders with `$1`, `$2`
 * and `${3:foo}`. `$0` defines the final tab stop, it defaults to
 * the end of the snippet. Placeholders with equal identifiers are linked,
 * that is typing in one will update others too.
 */
static const int Snippet = 2;
}; // namespace InsertTextFormat

struct CompletionItem {
	/**
	 * The label of this completion item. By default
	 * also the text that is inserted when selecting
	 * this completion.
	 */
	String label;

	/**
	 * The kind of this completion item. Based of the kind
	 * an icon is chosen by the editor. The standardized set
	 * of available values is defined in `CompletionItemKind`.
	 */
	int kind = 0;

	/**
	 * A human-readable string with additional information
	 * about this item, like type or symbol information.
	 */
	String detail;

	/**
	 * A human-readable string that represents a doc-comment.
	 */
	MarkupContent documentation;

	/**
	 * Indicates if this item is deprecated.
	 */
	bool deprecated = false;

	/**
	 * Select this item when showing.
	 *
	 * *Note* that only one completion item can be selected and that the
	 * tool / client decides which item that is. The rule is that the *first*
	 * item of those that match best is selected.
	 */
	bool preselect = false;

	/**
	 * A string that should be used when comparing this item
	 * with other items. When omitted the label is used
	 * as the filter text for this item.
	 */
	String sortText;

	/**
	 * A string that should be used when filtering a set of
	 * completion items. When omitted the label is used as the
	 * filter text for this item.
	 */
	String filterText;

	/**
	 * A string that should be inserted into a document when selecting
	 * this completion. When omitted the label is used as the insert text
	 * for this item.
	 *
	 * The `insertText` is subject to interpretation by the client side.
	 * Some tools might not take the string literally. For example
	 * VS Code when code complete is requested in this example
	 * `con<cursor position>` and a completion item with an `insertText` of
	 * `console` is provided it will only insert `sole`. Therefore it is
	 * recommended to use `textEdit` instead since it avoids additional client
	 * side interpretation.
	 */
	String insertText;

	/**
	 * The format of the insert text. The format applies to both the `insertText` property
	 * and the `newText` property of a provided `textEdit`.
	 */
	int insertTextFormat = 0;

	/**
	 * An edit which is applied to a document when selecting this completion. When an edit is provided the value of
	 * `insertText` is ignored.
	 *
	 * *Note:* The range of the edit must be a single line range and it must contain the position at which completion
	 * has been requested.
	 */
	TextEdit textEdit;

	/**
	 * An optional array of additional text edits that are applied when
	 * selecting this completion. Edits must not overlap (including the same insert position)
	 * with the main edit nor with themselves.
	 *
	 * Additional text edits should be used to change text unrelated to the current cursor position
	 * (for example adding an import statement at the top of the file if the completion item will
	 * insert an unqualified type).
	 */
	Vector<TextEdit> additionalTextEdits;

	/**
	 * An optional set of characters that when pressed while this completion is active will accept it first and
	 * then type that character. *Note* that all commit characters should have `length=1` and that superfluous
	 * characters will be ignored.
	 */
	Vector<String> commitCharacters;

	/**
	 * An optional command that is executed *after* inserting this completion. *Note* that
	 * additional modifications to the current document should be described with the
	 * additionalTextEdits-property.
	 */
	Command command;

	/**
	 * A data entry field that is preserved on a completion item between
	 * a completion and a completion resolve request.
	 */
	Variant data;

	_FORCE_INLINE_ Dictionary to_json(bool resolved = false) const {
		Dictionary dict;
		dict["label"] = label;
		dict["kind"] = kind;
		dict["data"] = data;
		if (!insertText.is_empty()) {
			dict["insertText"] = insertText;
		}
		if (resolved) {
			dict["detail"] = detail;
			dict["documentation"] = documentation.to_json();
			dict["deprecated"] = deprecated;
			dict["preselect"] = preselect;
			if (!sortText.is_empty()) {
				dict["sortText"] = sortText;
			}
			if (!filterText.is_empty()) {
				dict["filterText"] = filterText;
			}
			if (commitCharacters.size()) {
				dict["commitCharacters"] = commitCharacters;
			}
			if (!command.command.is_empty()) {
				dict["command"] = command.to_json();
			}
		}
		return dict;
	}

	void load(const Dictionary &p_dict) {
		if (p_dict.has("label")) {
			label = p_dict["label"];
		}
		if (p_dict.has("kind")) {
			kind = p_dict["kind"];
		}
		if (p_dict.has("detail")) {
			detail = p_dict["detail"];
		}
		if (p_dict.has("documentation")) {
			Variant doc = p_dict["documentation"];
			if (doc.is_string()) {
				documentation.value = doc;
			} else if (doc.get_type() == Variant::DICTIONARY) {
				Dictionary v = doc;
				documentation.value = v["value"];
			}
		}
		if (p_dict.has("deprecated")) {
			deprecated = p_dict["deprecated"];
		}
		if (p_dict.has("preselect")) {
			preselect = p_dict["preselect"];
		}
		if (p_dict.has("sortText")) {
			sortText = p_dict["sortText"];
		}
		if (p_dict.has("filterText")) {
			filterText = p_dict["filterText"];
		}
		if (p_dict.has("insertText")) {
			insertText = p_dict["insertText"];
		}
		if (p_dict.has("data")) {
			data = p_dict["data"];
		}
	}
};

/**
 * Represents a collection of [completion items](#CompletionItem) to be presented
 * in the editor.
 */
struct CompletionList {
	/**
	 * This list it not complete. Further typing should result in recomputing
	 * this list.
	 */
	bool isIncomplete = false;

	/**
	 * The completion items.
	 */
	Vector<CompletionItem> items;
};

// Use namespace instead of enumeration to follow the LSP specifications
// `lsp::EnumName::EnumValue` is OK but `lsp::EnumValue` is not
// And here C++ compilers are unhappy with our enumeration name like `String`, `Array`, `Object` etc
/**
 * A symbol kind.
 */
namespace SymbolKind {
static const int File = 1;
static const int Module = 2;
static const int Namespace = 3;
static const int Package = 4;
static const int Class = 5;
static const int Method = 6;
static const int Property = 7;
static const int Field = 8;
static const int Constructor = 9;
static const int Enum = 10;
static const int Interface = 11;
static const int Function = 12;
static const int Variable = 13;
static const int Constant = 14;
static const int String = 15;
static const int Number = 16;
static const int Boolean = 17;
static const int Array = 18;
static const int Object = 19;
static const int Key = 20;
static const int Null = 21;
static const int EnumMember = 22;
static const int Struct = 23;
static const int Event = 24;
static const int Operator = 25;
static const int TypeParameter = 26;
}; // namespace SymbolKind

/**
 * Represents programming constructs like variables, classes, interfaces etc. that appear in a document. Document symbols can be
 * hierarchical and they have two ranges: one that encloses its definition and one that points to its most interesting range,
 * e.g. the range of an identifier.
 */
struct DocumentSymbol {
	/**
	 * The name of this symbol. Will be displayed in the user interface and therefore must not be
	 * an empty string or a string only consisting of white spaces.
	 */
	String name;

	/**
	 * More detail for this symbol, e.g the signature of a function.
	 */
	String detail;

	/**
	 * Documentation for this symbol.
	 */
	String documentation;

	/**
	 * Class name for the native symbols.
	 */
	String native_class;

	/**
	 * The kind of this symbol.
	 */
	int kind = SymbolKind::File;

	/**
	 * Indicates if this symbol is deprecated.
	 */
	bool deprecated = false;

	/**
	 * If `true`: Symbol is local to script and cannot be accessed somewhere else.
	 *
	 * For example: local variable inside a `func`.
	 */
	bool local = false;

	/**
	 * The range enclosing this symbol not including leading/trailing whitespace but everything else
	 * like comments. This information is typically used to determine if the clients cursor is
	 * inside the symbol to reveal in the symbol in the UI.
	 */
	Range range;

	/**
	 * The range that should be selected and revealed when this symbol is being picked, e.g the name of a function.
	 * Must be contained by the `range`.
	 */
	Range selectionRange;

	DocumentUri uri;
	String script_path;

	/**
	 * Children of this symbol, e.g. properties of a class.
	 */
	Vector<DocumentSymbol> children;

	Dictionary to_json(bool with_doc = false) const {
		Dictionary dict;
		dict["name"] = name;
		dict["detail"] = detail;
		dict["kind"] = kind;
		dict["deprecated"] = deprecated;
		dict["range"] = range.to_json();
		dict["selectionRange"] = selectionRange.to_json();
		if (with_doc) {
			dict["documentation"] = documentation;
			dict["native_class"] = native_class;
		}
		if (!children.is_empty()) {
			Array arr;
			for (int i = 0; i < children.size(); i++) {
				if (children[i].local) {
					continue;
				}
				arr.push_back(children[i].to_json(with_doc));
			}
			if (!children.is_empty()) {
				dict["children"] = arr;
			}
		}
		return dict;
	}

	_FORCE_INLINE_ MarkupContent render() const {
		MarkupContent markdown;
		if (detail.length()) {
			markdown.value = "\t" + detail + "\n\n";
		}
		if (documentation.length()) {
			markdown.value += marked_documentation(documentation) + "\n\n";
		}
		if (script_path.length()) {
			markdown.value += "Defined in [" + script_path + "](" + uri + ")";
		}
		return markdown;
	}

	_FORCE_INLINE_ CompletionItem make_completion_item(bool resolved = false) const {
		lsp::CompletionItem item;
		item.label = name;

		if (resolved) {
			item.documentation = render();
		}

		switch (kind) {
			case lsp::SymbolKind::Enum:
				item.kind = lsp::CompletionItemKind::Enum;
				break;
			case lsp::SymbolKind::Class:
				item.kind = lsp::CompletionItemKind::Class;
				break;
			case lsp::SymbolKind::Property:
				item.kind = lsp::CompletionItemKind::Property;
				break;
			case lsp::SymbolKind::Method:
			case lsp::SymbolKind::Function:
				item.kind = lsp::CompletionItemKind::Method;
				break;
			case lsp::SymbolKind::Event:
				item.kind = lsp::CompletionItemKind::Event;
				break;
			case lsp::SymbolKind::Constant:
				item.kind = lsp::CompletionItemKind::Constant;
				break;
			case lsp::SymbolKind::Variable:
				item.kind = lsp::CompletionItemKind::Variable;
				break;
			case lsp::SymbolKind::File:
				item.kind = lsp::CompletionItemKind::File;
				break;
			default:
				item.kind = lsp::CompletionItemKind::Text;
				break;
		}

		return item;
	}
};

struct ApplyWorkspaceEditParams {
	WorkspaceEdit edit;

	Dictionary to_json() {
		Dictionary dict;

		dict["edit"] = edit.to_json();

		return dict;
	}
};

struct NativeSymbolInspectParams {
	String native_class;
	String symbol_name;

	void load(const Dictionary &p_params) {
		native_class = p_params["native_class"];
		symbol_name = p_params["symbol_name"];
	}
};

/**
 * Enum of known range kinds
 */
namespace FoldingRangeKind {
/**
 * Folding range for a comment
 */
static const String Comment = "comment";
/**
 * Folding range for a imports or includes
 */
static const String Imports = "imports";
/**
 * Folding range for a region (e.g. `#region`)
 */
static const String Region = "region";
} // namespace FoldingRangeKind

/**
 * Represents a folding range.
 */
struct FoldingRange {
	/**
	 * The zero-based line number from where the folded range starts.
	 */
	int startLine = 0;

	/**
	 * The zero-based character offset from where the folded range starts. If not defined, defaults to the length of the start line.
	 */
	int startCharacter = 0;

	/**
	 * The zero-based line number where the folded range ends.
	 */
	int endLine = 0;

	/**
	 * The zero-based character offset before the folded range ends. If not defined, defaults to the length of the end line.
	 */
	int endCharacter = 0;

	/**
	 * Describes the kind of the folding range such as `comment' or 'region'. The kind
	 * is used to categorize folding ranges and used by commands like 'Fold all comments'. See
	 * [FoldingRangeKind](#FoldingRangeKind) for an enumeration of standardized kinds.
	 */
	String kind = FoldingRangeKind::Region;

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["startLine"] = startLine;
		dict["startCharacter"] = startCharacter;
		dict["endLine"] = endLine;
		dict["endCharacter"] = endCharacter;
		return dict;
	}
};

// Use namespace instead of enumeration to follow the LSP specifications
/**
 * How a completion was triggered
 */
namespace CompletionTriggerKind {
/**
 * Completion was triggered by typing an identifier (24x7 code
 * complete), manual invocation (e.g Ctrl+Space) or via API.
 */
static const int Invoked = 1;

/**
 * Completion was triggered by a trigger character specified by
 * the `triggerCharacters` properties of the `CompletionRegistrationOptions`.
 */
static const int TriggerCharacter = 2;

/**
 * Completion was re-triggered as the current completion list is incomplete.
 */
static const int TriggerForIncompleteCompletions = 3;
} // namespace CompletionTriggerKind

/**
 * Contains additional information about the context in which a completion request is triggered.
 */
struct CompletionContext {
	/**
	 * How the completion was triggered.
	 */
	int triggerKind = CompletionTriggerKind::TriggerCharacter;

	/**
	 * The trigger character (a single character) that has trigger code complete.
	 * Is undefined if `triggerKind !== CompletionTriggerKind.TriggerCharacter`
	 */
	String triggerCharacter;

	void load(const Dictionary &p_params) {
		triggerKind = int(p_params["triggerKind"]);
		triggerCharacter = p_params["triggerCharacter"];
	}
};

struct CompletionParams : public TextDocumentPositionParams {
	/**
	 * The completion context. This is only available if the client specifies
	 * to send this using `ClientCapabilities.textDocument.completion.contextSupport === true`
	 */
	CompletionContext context;

	void load(const Dictionary &p_params) {
		TextDocumentPositionParams::load(p_params);
		context.load(p_params["context"]);
	}

	Dictionary to_json() {
		Dictionary ctx;
		ctx["triggerCharacter"] = context.triggerCharacter;
		ctx["triggerKind"] = context.triggerKind;

		Dictionary dict;
		dict = TextDocumentPositionParams::to_json();
		dict["context"] = ctx;
		return dict;
	}
};

/**
 * The result of a hover request.
 */
struct Hover {
	/**
	 * The hover's content
	 */
	MarkupContent contents;

	/**
	 * An optional range is a range inside a text document
	 * that is used to visualize a hover, e.g. by changing the background color.
	 */
	Range range;

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["range"] = range.to_json();
		dict["contents"] = contents.to_json();
		return dict;
	}
};

/**
 * Represents a parameter of a callable-signature. A parameter can
 * have a label and a doc-comment.
 */
struct ParameterInformation {
	/**
	 * The label of this parameter information.
	 *
	 * Either a string or an inclusive start and exclusive end offsets within its containing
	 * signature label. (see SignatureInformation.label). The offsets are based on a UTF-16
	 * string representation as `Position` and `Range` does.
	 *
	 * *Note*: a label of type string should be a substring of its containing signature label.
	 * Its intended use case is to highlight the parameter label part in the `SignatureInformation.label`.
	 */
	String label;

	/**
	 * The human-readable doc-comment of this parameter. Will be shown
	 * in the UI but can be omitted.
	 */
	MarkupContent documentation;

	Dictionary to_json() const {
		Dictionary dict;
		dict["label"] = label;
		dict["documentation"] = documentation.to_json();
		return dict;
	}
};

/**
 * Represents the signature of something callable. A signature
 * can have a label, like a function-name, a doc-comment, and
 * a set of parameters.
 */
struct SignatureInformation {
	/**
	 * The label of this signature. Will be shown in
	 * the UI.
	 */
	String label;

	/**
	 * The human-readable doc-comment of this signature. Will be shown
	 * in the UI but can be omitted.
	 */
	MarkupContent documentation;

	/**
	 * The parameters of this signature.
	 */
	Vector<ParameterInformation> parameters;

	Dictionary to_json() const {
		Dictionary dict;
		dict["label"] = label;
		dict["documentation"] = documentation.to_json();
		Array args;
		for (int i = 0; i < parameters.size(); i++) {
			args.push_back(parameters[i].to_json());
		}
		dict["parameters"] = args;
		return dict;
	}
};

/**
 * Signature help represents the signature of something
 * callable. There can be multiple signature but only one
 * active and only one active parameter.
 */
struct SignatureHelp {
	/**
	 * One or more signatures.
	 */
	Vector<SignatureInformation> signatures;

	/**
	 * The active signature. If omitted or the value lies outside the
	 * range of `signatures` the value defaults to zero or is ignored if
	 * `signatures.length === 0`. Whenever possible implementers should
	 * make an active decision about the active signature and shouldn't
	 * rely on a default value.
	 * In future version of the protocol this property might become
	 * mandatory to better express this.
	 */
	int activeSignature = 0;

	/**
	 * The active parameter of the active signature. If omitted or the value
	 * lies outside the range of `signatures[activeSignature].parameters`
	 * defaults to 0 if the active signature has parameters. If
	 * the active signature has no parameters it is ignored.
	 * In future version of the protocol this property might become
	 * mandatory to better express the active parameter if the
	 * active signature does have any.
	 */
	int activeParameter = 0;

	Dictionary to_json() const {
		Dictionary dict;
		Array sigs;
		for (int i = 0; i < signatures.size(); i++) {
			sigs.push_back(signatures[i].to_json());
		}
		dict["signatures"] = sigs;
		dict["activeSignature"] = activeSignature;
		dict["activeParameter"] = activeParameter;
		return dict;
	}
};

/**
 * A pattern to describe in which file operation requests or notifications
 * the server is interested in.
 */
struct FileOperationPattern {
	/**
	 * The glob pattern to match.
	 */
	String glob = "**/*.gd";

	/**
	 * Whether to match `file`s or `folder`s with this pattern.
	 *
	 * Matches both if undefined.
	 */
	String matches = "file";

	Dictionary to_json() const {
		Dictionary dict;

		dict["glob"] = glob;
		dict["matches"] = matches;

		return dict;
	}
};

/**
 * A filter to describe in which file operation requests or notifications
 * the server is interested in.
 */
struct FileOperationFilter {
	/**
	 * The actual file operation pattern.
	 */
	FileOperationPattern pattern;

	Dictionary to_json() const {
		Dictionary dict;

		dict["pattern"] = pattern.to_json();

		return dict;
	}
};

/**
 * The options to register for file operations.
 */
struct FileOperationRegistrationOptions {
	/**
	 * The actual filters.
	 */
	Vector<FileOperationFilter> filters;

	FileOperationRegistrationOptions() {
		filters.push_back(FileOperationFilter());
	}

	Dictionary to_json() const {
		Dictionary dict;

		Array filts;
		for (int i = 0; i < filters.size(); i++) {
			filts.push_back(filters[i].to_json());
		}
		dict["filters"] = filts;

		return dict;
	}
};

/**
 * The server is interested in file notifications/requests.
 */
struct FileOperations {
	/**
	 * The server is interested in receiving didDeleteFiles file notifications.
	 */
	FileOperationRegistrationOptions didDelete;

	Dictionary to_json() const {
		Dictionary dict;

		dict["didDelete"] = didDelete.to_json();

		return dict;
	}
};

/**
 * Workspace specific server capabilities
 */
struct Workspace {
	/**
	 * The server is interested in file notifications/requests.
	 */
	FileOperations fileOperations;

	Dictionary to_json() const {
		Dictionary dict;

		dict["fileOperations"] = fileOperations.to_json();

		return dict;
	}
};

struct ServerCapabilities {
	/**
	 * Defines how text documents are synced. Is either a detailed structure defining each notification or
	 * for backwards compatibility the TextDocumentSyncKind number. If omitted it defaults to `TextDocumentSyncKind.None`.
	 */
	TextDocumentSyncOptions textDocumentSync;

	/**
	 * The server provides hover support.
	 */
	bool hoverProvider = true;

	/**
	 * The server provides completion support.
	 */
	CompletionOptions completionProvider;

	/**
	 * The server provides signature help support.
	 */
	SignatureHelpOptions signatureHelpProvider;

	/**
	 * The server provides goto definition support.
	 */
	bool definitionProvider = true;

	/**
	 * The server provides Goto Type Definition support.
	 *
	 * Since 3.6.0
	 */
	bool typeDefinitionProvider = false;

	/**
	 * The server provides Goto Implementation support.
	 *
	 * Since 3.6.0
	 */
	bool implementationProvider = false;

	/**
	 * The server provides find references support.
	 */
	bool referencesProvider = true;

	/**
	 * The server provides document highlight support.
	 */
	bool documentHighlightProvider = false;

	/**
	 * The server provides document symbol support.
	 */
	bool documentSymbolProvider = true;

	/**
	 * The server provides workspace symbol support.
	 */
	bool workspaceSymbolProvider = true;

	/**
	 * The server supports workspace folder.
	 */
	Workspace workspace;

	/**
	 * The server provides code actions. The `CodeActionOptions` return type is only
	 * valid if the client signals code action literal support via the property
	 * `textDocument.codeAction.codeActionLiteralSupport`.
	 */
	bool codeActionProvider = false;

	/**
	 * The server provides code lens.
	 */
	CodeLensOptions codeLensProvider;

	/**
	 * The server provides document formatting.
	 */
	bool documentFormattingProvider = false;

	/**
	 * The server provides document range formatting.
	 */
	bool documentRangeFormattingProvider = false;

	/**
	 * The server provides document formatting on typing.
	 */
	DocumentOnTypeFormattingOptions documentOnTypeFormattingProvider;

	/**
	 * The server provides rename support. RenameOptions may only be
	 * specified if the client states that it supports
	 * `prepareSupport` in its initial `initialize` request.
	 */
	RenameOptions renameProvider;

	/**
	 * The server provides document link support.
	 */
	DocumentLinkOptions documentLinkProvider;

	/**
	 * The server provides color provider support.
	 *
	 * Since 3.6.0
	 */
	ColorProviderOptions colorProvider;

	/**
	 * The server provides folding provider support.
	 *
	 * Since 3.10.0
	 */
	FoldingRangeProviderOptions foldingRangeProvider;

	/**
	 * The server provides go to declaration support.
	 *
	 * Since 3.14.0
	 */
	bool declarationProvider = true;

	/**
	 * The server provides execute command support.
	 */
	ExecuteCommandOptions executeCommandProvider;

	_FORCE_INLINE_ Dictionary to_json() {
		Dictionary dict;
		dict["textDocumentSync"] = textDocumentSync.to_json();
		dict["completionProvider"] = completionProvider.to_json();
		signatureHelpProvider.triggerCharacters.push_back(",");
		signatureHelpProvider.triggerCharacters.push_back("(");
		dict["signatureHelpProvider"] = signatureHelpProvider.to_json();
		//dict["codeLensProvider"] = codeLensProvider.to_json();
		dict["documentOnTypeFormattingProvider"] = documentOnTypeFormattingProvider.to_json();
		dict["renameProvider"] = renameProvider.to_json();
		dict["documentLinkProvider"] = documentLinkProvider.to_json();
		dict["colorProvider"] = false; // colorProvider.to_json();
		dict["foldingRangeProvider"] = false; //foldingRangeProvider.to_json();
		dict["executeCommandProvider"] = executeCommandProvider.to_json();
		dict["hoverProvider"] = hoverProvider;
		dict["definitionProvider"] = definitionProvider;
		dict["typeDefinitionProvider"] = typeDefinitionProvider;
		dict["implementationProvider"] = implementationProvider;
		dict["referencesProvider"] = referencesProvider;
		dict["documentHighlightProvider"] = documentHighlightProvider;
		dict["documentSymbolProvider"] = documentSymbolProvider;
		dict["workspaceSymbolProvider"] = workspaceSymbolProvider;
		dict["workspace"] = workspace.to_json();
		dict["codeActionProvider"] = codeActionProvider;
		dict["documentFormattingProvider"] = documentFormattingProvider;
		dict["documentRangeFormattingProvider"] = documentRangeFormattingProvider;
		dict["declarationProvider"] = declarationProvider;
		return dict;
	}
};

struct InitializeResult {
	/**
	 * The capabilities the language server provides.
	 */
	ServerCapabilities capabilities;

	_FORCE_INLINE_ Dictionary to_json() {
		Dictionary dict;
		dict["capabilities"] = capabilities.to_json();
		return dict;
	}
};

struct GodotNativeClassInfo {
	String name;
	const DocData::ClassDoc *class_doc = nullptr;
	const ClassDB::ClassInfo *class_info = nullptr;

	Dictionary to_json() {
		Dictionary dict;
		dict["name"] = name;
		dict["inherits"] = class_doc->inherits;
		return dict;
	}
};

/** Features not included in the standard lsp specifications */
struct GodotCapabilities {
	/**
	 * Native class list
	 */
	List<GodotNativeClassInfo> native_classes;

	Dictionary to_json() {
		Dictionary dict;
		Array classes;
		for (List<GodotNativeClassInfo>::Element *E = native_classes.front(); E; E = E->next()) {
			classes.push_back(E->get().to_json());
		}
		dict["native_classes"] = classes;
		return dict;
	}
};

/** Format BBCode documentation from DocData to markdown */
static String marked_documentation(const String &p_bbcode) {
	String markdown = p_bbcode.strip_edges();

	Vector<String> lines = markdown.split("\n");
	bool in_code_block = false;
	int code_block_indent = -1;

	markdown = "";
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		int block_start = line.find("[codeblock]");
		if (block_start != -1) {
			code_block_indent = block_start;
			in_code_block = true;
			line = "\n";
		} else if (in_code_block) {
			line = "\t" + line.substr(code_block_indent);
		}

		if (in_code_block && line.contains("[/codeblock]")) {
			line = "\n";
			in_code_block = false;
		}

		if (!in_code_block) {
			line = line.strip_edges();
			line = line.replace("[code]", "`");
			line = line.replace("[/code]", "`");
			line = line.replace("[i]", "*");
			line = line.replace("[/i]", "*");
			line = line.replace("[b]", "**");
			line = line.replace("[/b]", "**");
			line = line.replace("[u]", "__");
			line = line.replace("[/u]", "__");
			line = line.replace("[method ", "`");
			line = line.replace("[member ", "`");
			line = line.replace("[signal ", "`");
			line = line.replace("[enum ", "`");
			line = line.replace("[constant ", "`");
			line = line.replace("[", "`");
			line = line.replace("]", "`");
		}

		if (!in_code_block && i < lines.size() - 1) {
			line += "\n\n";
		} else if (i < lines.size() - 1) {
			line += "\n";
		}
		markdown += line;
	}
	return markdown;
}
} // namespace lsp

#endif // GODOT_LSP_H
