/**************************************************************************/
/*  config_file.cpp                                                       */
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

#include "config_file.h"

#include "core/io/file_access_encrypted.h"
#include "core/string/string_builder.h"
#include "core/variant/variant_parser.h"

PackedStringArray ConfigFile::_get_sections() const {
	List<String> s;
	get_sections(&s);
	PackedStringArray arr;
	arr.resize(s.size());
	int idx = 0;
	for (const String &E : s) {
		arr.set(idx++, E);
	}

	return arr;
}

PackedStringArray ConfigFile::_get_section_keys(const String &p_section) const {
	List<String> s;
	get_section_keys(p_section, &s);
	PackedStringArray arr;
	arr.resize(s.size());
	int idx = 0;
	for (const String &E : s) {
		arr.set(idx++, E);
	}

	return arr;
}

void ConfigFile::_create_entry_if_needed(const String &p_section, const String &p_key) {
	if (!entries.has(p_section)) {
		entries.insert(p_section, HashMap<String, Entry>(), p_section.is_empty());
	}
	if (!entries[p_section].has(p_key)) {
		entries[p_section].insert(p_key, Entry());
	}
}

String ConfigFile::_get_entry_as_string(const String &p_key, const Entry &p_entry) const {
	String estr;

	if (!p_entry.comment.is_empty()) {
		PackedStringArray lines = p_entry.comment.split("\n", true); // allow empty for multi-line separation in detailed comments
		
		// marker for removing the empty front lines
		int first_populated_line = -1;

		// remove empty lines in the back
		for (int i = lines.size() - 1; i > -1; i--) {
			if (lines[i].strip_edges().is_empty()) {
				if (first_populated_line == -1) {
					lines.remove_at(i);
				}
			} else {
				first_populated_line = i;
			}
		}
		// remove empty lines in the front
		for (int i = first_populated_line; i > -1; i--) {
			if (lines[i].strip_edges().is_empty()) {
				lines.remove_at(i);
			}
		}

		if (!lines.is_empty()) {
			for (int i = 0; i < lines.size(); i++) {
				String line = lines[i].strip_edges(false, true); // no reason to keep the right edge i think?
				estr += ("; " + line + "\n"); // it should be ok to always indent the comment, right? maybe make the auto-indentation a project setting...
			}
		}
	}

	String vstr;
	VariantWriter::write_to_string(p_entry.value, vstr);
	estr += (p_key.property_name_encode() + "=" + vstr + "\n");

	return estr;
}

void ConfigFile::set_value(const String &p_section, const String &p_key, const Variant &p_value) {
	if (p_value.get_type() == Variant::NIL) { // Erase key.
		if (!entries.has(p_section)) {
			return;
		}

		entries[p_section].erase(p_key);
		if (entries[p_section].is_empty()) {
			entries.erase(p_section);
		}
	} else {
		_create_entry_if_needed(p_section, p_key);
		entries[p_section][p_key].value = p_value;
	}
}

Variant ConfigFile::get_value(const String &p_section, const String &p_key, const Variant &p_default) const {
	if (!entries.has(p_section) || !entries[p_section].has(p_key)) {
		ERR_FAIL_COND_V_MSG(p_default.get_type() == Variant::NIL, Variant(),
				vformat("Couldn't find the given section \"%s\" and key \"%s\", and no default was given.", p_section, p_key));
		return p_default;
	}

	return entries[p_section][p_key].value;
}

void ConfigFile::set_section_key_comment(const String &p_section, const String &p_key, const String &p_comment) {
	// It might not be desirable to give an error for this case. For example, assigning a comment to a value previously made null would break...
	ERR_FAIL_COND_MSG(!has_section_key(p_section, p_key), vformat("Couldn't find the given section \"%s\" and key \"%s\".", p_section, p_key));
	entries[p_section][p_key].comment = p_comment;
}

String ConfigFile::get_section_key_comment(const String &p_section, const String &p_key) const {
	ERR_FAIL_COND_V_MSG(
			!entries.has(p_section) || !entries[p_section].has(p_key),
			String(),
			vformat("Couldn't find the given section \"%s\" and key \"%s\".", p_section, p_key));

	return entries[p_section][p_key].comment;
}

bool ConfigFile::has_section(const String &p_section) const {
	return entries.has(p_section);
}

bool ConfigFile::has_section_key(const String &p_section, const String &p_key) const {
	if (!entries.has(p_section)) {
		return false;
	}
	return entries[p_section].has(p_key);
}

bool ConfigFile::has_section_key_comment(const String &p_section, const String &p_key) const {
	if (!has_section_key(p_section, p_key)) {
		return false;
	}
	return !entries[p_section][p_key].comment.is_empty();
}

void ConfigFile::get_sections(List<String> *r_sections) const {
	for (const KeyValue<String, HashMap<String, Entry>> &E : entries) {
		r_sections->push_back(E.key);
	}
}

void ConfigFile::get_section_keys(const String &p_section, List<String> *r_keys) const {
	ERR_FAIL_COND_MSG(!entries.has(p_section), vformat("Cannot get keys from nonexistent section \"%s\".", p_section));

	for (const KeyValue<String, Entry> &E : entries[p_section]) {
		r_keys->push_back(E.key);
	}
}

void ConfigFile::erase_section(const String &p_section) {
	ERR_FAIL_COND_MSG(!entries.has(p_section), vformat("Cannot erase nonexistent section \"%s\".", p_section));
	entries.erase(p_section);
}

void ConfigFile::erase_section_key(const String &p_section, const String &p_key) {
	ERR_FAIL_COND_MSG(!entries.has(p_section), vformat("Cannot erase key \"%s\" from nonexistent section \"%s\".", p_key, p_section));
	ERR_FAIL_COND_MSG(!entries[p_section].has(p_key), vformat("Cannot erase nonexistent key \"%s\" from section \"%s\".", p_key, p_section));

	entries[p_section].erase(p_key);
	if (entries[p_section].is_empty()) {
		entries.erase(p_section);
	}
}

void ConfigFile::erase_section_key_comment(const String &p_section, const String &p_key) {
	ERR_FAIL_COND_MSG(!entries.has(p_section), vformat("Cannot erase comment from nonexistent section \"%s\".", p_section));
	ERR_FAIL_COND_MSG(!entries[p_section].has(p_key), vformat("Cannot erase comment from nonexistent key \"%s\" in section \"%s\".", p_key, p_section));

	entries[p_section][p_key].comment = String();
}

String ConfigFile::encode_to_text() const {
	StringBuilder sb;
	bool first = true;
	for (const KeyValue<String, HashMap<String, Entry>> &E : entries) {
		if (first) {
			first = false;
		} else {
			sb.append("\n");
		}
		if (!E.key.is_empty()) {
			sb.append("[" + E.key + "]\n\n");
		}

		for (const KeyValue<String, Entry> &F : E.value) {
			sb.append(_get_entry_as_string(F.key, F.value));
		}
	}
	return sb.as_string();
}

Error ConfigFile::save(const String &p_path) {
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);

	if (err) {
		return err;
	}

	return _internal_save(file);
}

Error ConfigFile::save_encrypted(const String &p_path, const Vector<uint8_t> &p_key) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);

	if (err) {
		return err;
	}

	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	err = fae->open_and_parse(f, p_key, FileAccessEncrypted::MODE_WRITE_AES256);
	if (err) {
		return err;
	}
	return _internal_save(fae);
}

Error ConfigFile::save_encrypted_pass(const String &p_path, const String &p_pass) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);

	if (err) {
		return err;
	}

	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	err = fae->open_and_parse_password(f, p_pass, FileAccessEncrypted::MODE_WRITE_AES256);
	if (err) {
		return err;
	}

	return _internal_save(fae);
}

Error ConfigFile::_internal_save(Ref<FileAccess> file) {
	bool first = true;
	for (const KeyValue<String, HashMap<String, Entry>> &E : entries) {
		if (first) {
			first = false;
		} else {
			file->store_string("\n");
		}
		if (!E.key.is_empty()) {
			file->store_string("[" + E.key.replace("]", "\\]") + "]\n\n");
		}

		for (const KeyValue<String, Entry> &F : E.value) {
			file->store_string(_get_entry_as_string(F.key, F.value));
		}
	}

	return OK;
}

Error ConfigFile::load(const String &p_path) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	if (f.is_null()) {
		return err;
	}

	return _internal_load(p_path, f);
}

Error ConfigFile::load_encrypted(const String &p_path, const Vector<uint8_t> &p_key) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	if (err) {
		return err;
	}

	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	err = fae->open_and_parse(f, p_key, FileAccessEncrypted::MODE_READ);
	if (err) {
		return err;
	}
	return _internal_load(p_path, fae);
}

Error ConfigFile::load_encrypted_pass(const String &p_path, const String &p_pass) {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	if (err) {
		return err;
	}

	Ref<FileAccessEncrypted> fae;
	fae.instantiate();
	err = fae->open_and_parse_password(f, p_pass, FileAccessEncrypted::MODE_READ);
	if (err) {
		return err;
	}

	return _internal_load(p_path, fae);
}

Error ConfigFile::_internal_load(const String &p_path, Ref<FileAccess> f) {
	VariantParser::StreamFile stream;
	stream.f = f;

	Error err = _parse(p_path, &stream);

	return err;
}

Error ConfigFile::parse(const String &p_data) {
	VariantParser::StreamString stream;
	stream.s = p_data;
	return _parse("<string>", &stream);
}

Error ConfigFile::_parse(const String &p_path, VariantParser::Stream *p_stream) {
	String assign;
	Variant value;
	VariantParser::Tag next_tag;

	int lines = 0;
	String error_text;

	String section;

	while (true) {
		assign = Variant();
		next_tag.fields.clear();
		next_tag.name = String();

		Error err = VariantParser::parse_tag_assign_eof(p_stream, lines, error_text, next_tag, assign, value, nullptr, true);
		if (err == ERR_FILE_EOF) {
			return OK;
		} else if (err != OK) {
			ERR_PRINT(vformat("ConfigFile parse error at %s:%d: %s.", p_path, lines, error_text));
			return err;
		}

		if (!assign.is_empty()) {
			set_value(section, assign, value);
		} else if (!next_tag.name.is_empty()) {
			section = next_tag.name.replace("\\]", "]");
		}
	}

	return OK;
}

void ConfigFile::clear() {
	entries.clear();
}

void ConfigFile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_value", "section", "key", "value"), &ConfigFile::set_value);
	ClassDB::bind_method(D_METHOD("get_value", "section", "key", "default"), &ConfigFile::get_value, DEFVAL(Variant()));

	ClassDB::bind_method(D_METHOD("set_section_key_comment", "section", "key", "comment"), &ConfigFile::set_section_key_comment);
	ClassDB::bind_method(D_METHOD("get_section_key_comment", "section", "key"), &ConfigFile::get_section_key_comment);

	ClassDB::bind_method(D_METHOD("has_section", "section"), &ConfigFile::has_section);
	ClassDB::bind_method(D_METHOD("has_section_key", "section", "key"), &ConfigFile::has_section_key);
	ClassDB::bind_method(D_METHOD("has_section_key_comment", "section", "key"), &ConfigFile::has_section_key_comment);

	ClassDB::bind_method(D_METHOD("get_sections"), &ConfigFile::_get_sections);
	ClassDB::bind_method(D_METHOD("get_section_keys", "section"), &ConfigFile::_get_section_keys);

	ClassDB::bind_method(D_METHOD("erase_section", "section"), &ConfigFile::erase_section);
	ClassDB::bind_method(D_METHOD("erase_section_key", "section", "key"), &ConfigFile::erase_section_key);
	ClassDB::bind_method(D_METHOD("erase_section_key_comment", "section", "key"), &ConfigFile::erase_section_key_comment);

	ClassDB::bind_method(D_METHOD("load", "path"), &ConfigFile::load);
	ClassDB::bind_method(D_METHOD("parse", "data"), &ConfigFile::parse);
	ClassDB::bind_method(D_METHOD("save", "path"), &ConfigFile::save);

	ClassDB::bind_method(D_METHOD("encode_to_text"), &ConfigFile::encode_to_text);

	BIND_METHOD_ERR_RETURN_DOC("load", ERR_FILE_CANT_OPEN);

	ClassDB::bind_method(D_METHOD("load_encrypted", "path", "key"), &ConfigFile::load_encrypted);
	ClassDB::bind_method(D_METHOD("load_encrypted_pass", "path", "password"), &ConfigFile::load_encrypted_pass);

	ClassDB::bind_method(D_METHOD("save_encrypted", "path", "key"), &ConfigFile::save_encrypted);
	ClassDB::bind_method(D_METHOD("save_encrypted_pass", "path", "password"), &ConfigFile::save_encrypted_pass);

	ClassDB::bind_method(D_METHOD("clear"), &ConfigFile::clear);
}
