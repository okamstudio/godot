/**************************************************************************/
/*  editor_translation.cpp                                                */
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

#include "editor/editor_translation.h"

#include "core/io/compression.h"
#include "core/io/file_access_memory.h"
#include "core/io/translation_loader_po.h"
#include "core/string/translation_server.h"
#include "editor/doc_translations.gen.h"
#include "editor/editor_translations.gen.h"
#include "editor/extractable_translations.gen.h"
#include "editor/property_translations.gen.h"

Vector<String> get_editor_locales() {
	Vector<String> locales;

	EditorTranslationList *etl = _editor_translations;
	while (etl->data) {
		const String &locale = etl->lang;
		locales.push_back(locale);

		etl++;
	}

	return locales;
}

void load_editor_translations(const String &p_locale) {
	const Ref<TranslationDomain> domain = TranslationServer::get_singleton()->get_or_add_domain("godot.editor");

	EditorTranslationList *etl = _editor_translations;
	while (etl->data) {
		if (etl->lang == p_locale) {
			Vector<uint8_t> data;
			data.resize(etl->uncomp_size);
			int ret = Compression::decompress(data.ptrw(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);
			ERR_FAIL_COND_MSG(ret == -1, "Compressed file is corrupt.");

			Ref<FileAccessMemory> fa;
			fa.instantiate();
			fa->open_custom(data.ptr(), data.size());

			Ref<Translation> tr = TranslationLoaderPO::load_translation(fa);

			if (tr.is_valid()) {
				tr->set_locale(etl->lang);
				domain->add_translation(tr);
				break;
			}
		}

		etl++;
	}
}

void load_property_translations(const String &p_locale) {
	const Ref<TranslationDomain> domain = TranslationServer::get_singleton()->get_or_add_domain("godot.properties");

	PropertyTranslationList *etl = _property_translations;
	while (etl->data) {
		if (etl->lang == p_locale) {
			Vector<uint8_t> data;
			data.resize(etl->uncomp_size);
			int ret = Compression::decompress(data.ptrw(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);
			ERR_FAIL_COND_MSG(ret == -1, "Compressed file is corrupt.");

			Ref<FileAccessMemory> fa;
			fa.instantiate();
			fa->open_custom(data.ptr(), data.size());

			Ref<Translation> tr = TranslationLoaderPO::load_translation(fa);

			if (tr.is_valid()) {
				tr->set_locale(etl->lang);
				domain->add_translation(tr);
				break;
			}
		}

		etl++;
	}
}

void load_doc_translations(const String &p_locale) {
	const Ref<TranslationDomain> domain = TranslationServer::get_singleton()->get_or_add_domain("godot.documentation");

	DocTranslationList *dtl = _doc_translations;
	while (dtl->data) {
		if (dtl->lang == p_locale) {
			Vector<uint8_t> data;
			data.resize(dtl->uncomp_size);
			int ret = Compression::decompress(data.ptrw(), dtl->uncomp_size, dtl->data, dtl->comp_size, Compression::MODE_DEFLATE);
			ERR_FAIL_COND_MSG(ret == -1, "Compressed file is corrupt.");

			Ref<FileAccessMemory> fa;
			fa.instantiate();
			fa->open_custom(data.ptr(), data.size());

			Ref<Translation> tr = TranslationLoaderPO::load_translation(fa);

			if (tr.is_valid()) {
				tr->set_locale(dtl->lang);
				domain->add_translation(tr);
				break;
			}
		}

		dtl++;
	}
}

void load_extractable_translations(const String &p_locale) {
	const Ref<TranslationDomain> domain = TranslationServer::get_singleton()->get_or_add_domain("godot.editor");

	ExtractableTranslationList *etl = _extractable_translations;
	while (etl->data) {
		if (etl->lang == p_locale) {
			Vector<uint8_t> data;
			data.resize(etl->uncomp_size);
			int ret = Compression::decompress(data.ptrw(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);
			ERR_FAIL_COND_MSG(ret == -1, "Compressed file is corrupt.");

			Ref<FileAccessMemory> fa;
			fa.instantiate();
			fa->open_custom(data.ptr(), data.size());

			Ref<Translation> tr = TranslationLoaderPO::load_translation(fa);

			if (tr.is_valid()) {
				tr->set_locale(etl->lang);
				domain->add_translation(tr);
				break;
			}
		}

		etl++;
	}
}

Vector<Vector<String>> get_extractable_message_list() {
	ExtractableTranslationList *etl = _extractable_translations;
	Vector<Vector<String>> list;

	while (etl->data) {
		if (strcmp(etl->lang, "source")) {
			etl++;
			continue;
		}

		Vector<uint8_t> data;
		data.resize(etl->uncomp_size);
		int ret = Compression::decompress(data.ptrw(), etl->uncomp_size, etl->data, etl->comp_size, Compression::MODE_DEFLATE);
		ERR_FAIL_COND_V_MSG(ret == -1, list, "Compressed file is corrupt.");

		Ref<FileAccessMemory> fa;
		fa.instantiate();
		fa->open_custom(data.ptr(), data.size());

		// Taken from TranslationLoaderPO, modified to work specifically with POTs.
		{
			const String path = fa->get_path();

			fa->seek(0);

			enum Status {
				STATUS_NONE,
				STATUS_READING_ID,
				STATUS_READING_STRING,
				STATUS_READING_CONTEXT,
				STATUS_READING_PLURAL,
			};

			Status status = STATUS_NONE;

			String msg_id;
			String msg_id_plural;
			String msg_context;

			int line = 1;
			bool entered_context = false;
			bool is_eof = false;

			while (!is_eof) {
				String l = fa->get_line().strip_edges();
				is_eof = fa->eof_reached();

				// If we reached last line and it's not a content line, break, otherwise let processing that last loop.
				if (is_eof && l.is_empty()) {
					if (status == STATUS_READING_ID || status == STATUS_READING_CONTEXT || status == STATUS_READING_PLURAL) {
						ERR_FAIL_V_MSG(Vector<Vector<String>>(), "Unexpected EOF while reading POT file at: " + path + ":" + itos(line));
					} else {
						break;
					}
				}

				if (l.begins_with("msgctxt")) {
					ERR_FAIL_COND_V_MSG(status != STATUS_READING_STRING && status != STATUS_READING_PLURAL, Vector<Vector<String>>(),
							"Unexpected 'msgctxt', was expecting 'msgid_plural' or 'msgstr' before 'msgctxt' while parsing: " + path + ":" + itos(line));

					// In POT files, "msgctxt" appears before "msgid". If we encounter a "msgctxt", we add what we have read
					// and set "entered_context" to true to prevent adding twice.
					if (!msg_id.is_empty()) {
						Vector<String> msgs;
						msgs.push_back(msg_id);
						msgs.push_back(msg_context);
						msgs.push_back(msg_id_plural);
						list.push_back(msgs);
					}
					msg_context = "";
					l = l.substr(7).strip_edges();
					status = STATUS_READING_CONTEXT;
					entered_context = true;
				}

				if (l.begins_with("msgid_plural")) {
					if (status != STATUS_READING_ID) {
						ERR_FAIL_V_MSG(Vector<Vector<String>>(), "Unexpected 'msgid_plural', was expecting 'msgid' before 'msgid_plural' while parsing: " + path + ":" + itos(line));
					}
					l = l.substr(12).strip_edges();
					status = STATUS_READING_PLURAL;
				} else if (l.begins_with("msgid")) {
					ERR_FAIL_COND_V_MSG(status == STATUS_READING_ID, Vector<Vector<String>>(), "Unexpected 'msgid', was expecting 'msgstr' while parsing: " + path + ":" + itos(line));

					if (!msg_id.is_empty() && !entered_context) {
						Vector<String> msgs;
						msgs.push_back(msg_id);
						msgs.push_back(msg_context);
						msgs.push_back(msg_id_plural);
						list.push_back(msgs);
					}

					l = l.substr(5).strip_edges();
					status = STATUS_READING_ID;
					// If we did not encounter msgctxt, we reset context to empty to reset it.
					if (!entered_context) {
						msg_context = "";
					}
					msg_id = "";
					msg_id_plural = "";
					entered_context = false;
				}

				if (l.begins_with("msgstr[")) {
					ERR_FAIL_COND_V_MSG(status != STATUS_READING_PLURAL, Vector<Vector<String>>(),
							"Unexpected 'msgstr[]', was expecting 'msgid_plural' before 'msgstr[]' while parsing: " + path + ":" + itos(line));
					l = l.substr(9).strip_edges();
				} else if (l.begins_with("msgstr")) {
					ERR_FAIL_COND_V_MSG(status != STATUS_READING_ID, Vector<Vector<String>>(),
							"Unexpected 'msgstr', was expecting 'msgid' before 'msgstr' while parsing: " + path + ":" + itos(line));
					l = l.substr(6).strip_edges();
					status = STATUS_READING_STRING;
				}

				if (l.is_empty() || l.begins_with("#")) {
					line++;
					continue; // Nothing to read or comment.
				}

				ERR_FAIL_COND_V_MSG(!l.begins_with("\"") || status == STATUS_NONE, Vector<Vector<String>>(), "Invalid line '" + l + "' while parsing: " + path + ":" + itos(line));

				l = l.substr(1);
				// Find final quote, ignoring escaped ones (\").
				// The escape_next logic is necessary to properly parse things like \\"
				// where the backslash is the one being escaped, not the quote.
				int end_pos = -1;
				bool escape_next = false;
				for (int i = 0; i < l.length(); i++) {
					if (l[i] == '\\' && !escape_next) {
						escape_next = true;
						continue;
					}

					if (l[i] == '"' && !escape_next) {
						end_pos = i;
						break;
					}

					escape_next = false;
				}

				ERR_FAIL_COND_V_MSG(end_pos == -1, Vector<Vector<String>>(), "Expected '\"' at end of message while parsing: " + path + ":" + itos(line));

				l = l.substr(0, end_pos);
				l = l.c_unescape();

				if (status == STATUS_READING_ID) {
					msg_id += l;
				} else if (status == STATUS_READING_CONTEXT) {
					msg_context += l;
				} else if (status == STATUS_READING_PLURAL) {
					msg_id_plural += l;
				}

				line++;
			}
		}

		etl++;
	}

	return list;
}
