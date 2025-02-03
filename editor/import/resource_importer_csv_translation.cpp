/**************************************************************************/
/*  resource_importer_csv_translation.cpp                                 */
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

#include "resource_importer_csv_translation.h"

#include "core/compressed_translation.h"
#include "core/io/resource_saver.h"
#include "core/os/file_access.h"
#include "core/translation.h"

String ResourceImporterCSVTranslation::get_importer_name() const {
	return "csv_translation";
}

String ResourceImporterCSVTranslation::get_visible_name() const {
	return "CSV Translation";
}
void ResourceImporterCSVTranslation::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("csv");
}

String ResourceImporterCSVTranslation::get_save_extension() const {
	return ""; //does not save a single resource
}

String ResourceImporterCSVTranslation::get_resource_type() const {
	return "Translation";
}

bool ResourceImporterCSVTranslation::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {
	return true;
}

int ResourceImporterCSVTranslation::get_preset_count() const {
	return 0;
}
String ResourceImporterCSVTranslation::get_preset_name(int p_idx) const {
	return "";
}

void ResourceImporterCSVTranslation::get_import_options(List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "compress"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "delimiter", PROPERTY_HINT_ENUM, "Comma,Semicolon,Tab"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "unescape_keys"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "unescape_translations"), true));
}

Error ResourceImporterCSVTranslation::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	const bool compress = p_options["compress"];
	const bool unescape_keys = p_options.has("unescape_keys") ? bool(p_options["unescape_keys"]) : false;
	const bool unescape_translations = p_options.has("unescape_translations") ? bool(p_options["unescape_translations"]) : true;

	String delimiter;
	switch ((int)p_options["delimiter"]) {
		case 0:
			delimiter = ",";
			break;
		case 1:
			delimiter = ";";
			break;
		case 2:
			delimiter = "\t";
			break;
	}

	FileAccessRef f = FileAccess::open(p_source_file, FileAccess::READ);

	ERR_FAIL_COND_V_MSG(!f, ERR_INVALID_PARAMETER, "Cannot open file from path '" + p_source_file + "'.");

	Vector<String> line = f->get_csv_line(delimiter);
	ERR_FAIL_COND_V(line.size() <= 1, ERR_PARSE_ERROR);

	Vector<String> locales;
	Vector<Ref<Translation>> translations;

	for (int i = 1; i < line.size(); i++) {
		String locale = TranslationServer::get_singleton()->standardize_locale(line[i]);

		locales.push_back(locale);
		Ref<Translation> translation;
		translation.instance();
		translation->set_locale(locale);
		translations.push_back(translation);
	}

	line = f->get_csv_line(delimiter);

	while (line.size() == locales.size() + 1) {
		const String key = unescape_keys ? line[0].c_unescape() : line[0];
		if (key != "") {
			for (int i = 1; i < line.size(); i++) {
				translations.write[i - 1]->add_message(key, unescape_translations ? line[i].c_unescape() : line[i]);
			}
		}

		line = f->get_csv_line(delimiter);
	}

	for (int i = 0; i < translations.size(); i++) {
		Ref<Translation> xlt = translations[i];

		if (compress) {
			Ref<PHashTranslation> cxl = memnew(PHashTranslation);
			cxl->generate(xlt);
			xlt = cxl;
		}

		String save_path = p_source_file.get_basename() + "." + translations[i]->get_locale() + ".translation";

		ResourceSaver::save(save_path, xlt);
		if (r_gen_files) {
			r_gen_files->push_back(save_path);
		}
	}

	return OK;
}

ResourceImporterCSVTranslation::ResourceImporterCSVTranslation() {
}
