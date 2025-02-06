/**************************************************************************/
/*  doc_tools.h                                                           */
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

#include "core/doc_data.h"
#include "core/templates/rb_set.h"

class DocTools {
public:
	String version;
	HashMap<String, DocData::ClassDoc> class_list;
	HashMap<String, RBSet<String, NaturalNoCaseComparator>> inheriting;

	static Error erase_classes(const String &p_dir);

	void merge_from(const DocTools &p_data);
	void add_doc(const DocData::ClassDoc &p_class_doc);
	void remove_doc(const String &p_class_name);
	bool has_doc(const String &p_class_name);
	enum GenerateFlags {
		GENERATE_FLAG_SKIP_BASIC_TYPES = (1 << 0),
		GENERATE_FLAG_EXTENSION_CLASSES_ONLY = (1 << 1),
	};
	void generate(BitField<GenerateFlags> p_flags = {});
	Error load_classes(const String &p_dir);
	Error save_classes(const String &p_default_path, const HashMap<String, String> &p_class_path, bool p_use_relative_schema = true);

	Error _load(Ref<XMLParser> parser);
	Error load_compressed(const uint8_t *p_data, int p_compressed_size, int p_uncompressed_size);
	Error load_xml(const uint8_t *p_data, int p_size);
};
