/**************************************************************************/
/*  config_file.h                                                         */
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

#pragma once

#include "core/io/file_access.h"
#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant_parser.h"

class ConfigFile : public RefCounted {
	GDCLASS(ConfigFile, RefCounted);

	HashMap<String, HashMap<String, Variant>> values;

	PackedStringArray _get_sections() const;
	PackedStringArray _get_section_keys(const String &p_section) const;
	Error _internal_load(const String &p_path, Ref<FileAccess> f);
	Error _internal_save(Ref<FileAccess> file);

	Error _parse(const String &p_path, VariantParser::Stream *p_stream);

protected:
	static void _bind_methods();

public:
	void set_value(const String &p_section, const String &p_key, const Variant &p_value);
	Variant get_value(const String &p_section, const String &p_key, const Variant &p_default = Variant()) const;

	bool has_section(const String &p_section) const;
	bool has_section_key(const String &p_section, const String &p_key) const;

	void get_sections(List<String> *r_sections) const;
	void get_section_keys(const String &p_section, List<String> *r_keys) const;

	void erase_section(const String &p_section);
	void erase_section_key(const String &p_section, const String &p_key);

	Error save(const String &p_path);
	Error load(const String &p_path);
	Error parse(const String &p_data);

	String encode_to_text() const; // used by exporter

	void clear();

	Error load_encrypted(const String &p_path, const Vector<uint8_t> &p_key);
	Error load_encrypted_pass(const String &p_path, const String &p_pass);

	Error save_encrypted(const String &p_path, const Vector<uint8_t> &p_key);
	Error save_encrypted_pass(const String &p_path, const String &p_pass);
};
