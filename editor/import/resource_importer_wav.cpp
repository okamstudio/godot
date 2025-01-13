/**************************************************************************/
/*  resource_importer_wav.cpp                                             */
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

#include "resource_importer_wav.h"

#include "core/io/file_access.h"
#include "core/io/marshalls.h"
#include "core/io/resource_saver.h"

String ResourceImporterWAV::get_importer_name() const {
	return "wav";
}

String ResourceImporterWAV::get_visible_name() const {
	return "Microsoft WAV";
}

void ResourceImporterWAV::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("wav");
}

String ResourceImporterWAV::get_save_extension() const {
	return "sample";
}

String ResourceImporterWAV::get_resource_type() const {
	return "AudioStreamWAV";
}

bool ResourceImporterWAV::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	if (p_option == "force/max_rate_hz" && !bool(p_options["force/max_rate"])) {
		return false;
	}

	// Don't show begin/end loop points if loop mode is auto-detected or disabled.
	if ((int)p_options["edit/loop_mode"] < 2 && (p_option == "edit/loop_begin" || p_option == "edit/loop_end")) {
		return false;
	}

	return true;
}

int ResourceImporterWAV::get_preset_count() const {
	return 0;
}

String ResourceImporterWAV::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterWAV::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force/8_bit"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force/mono"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force/max_rate", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "force/max_rate_hz", PROPERTY_HINT_RANGE, "11025,192000,1,exp"), 44100));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "edit/trim"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "edit/normalize"), false));
	// Keep the `edit/loop_mode` enum in sync with AudioStreamWAV::LoopMode (note: +1 offset due to "Detect From WAV").
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "edit/loop_mode", PROPERTY_HINT_ENUM, "Detect From WAV,Disabled,Forward,Ping-Pong,Backward", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "edit/loop_begin"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "edit/loop_end"), -1));
	// Quite OK Audio is lightweight enough and supports virtually every significant AudioStreamWAV feature.
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/mode", PROPERTY_HINT_ENUM, "PCM (Uncompressed),IMA ADPCM,Quite OK Audio"), 2));
}

Error ResourceImporterWAV::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	Dictionary options;
	for (const KeyValue<StringName, Variant> &pair : p_options) {
		options[pair.key] = pair.value;
	}

	Ref<AudioStreamWAV> sample = AudioStreamWAV::load_from_file(p_source_file, options);
	ResourceSaver::save(sample, p_save_path + ".sample");
	return OK;
}

ResourceImporterWAV::ResourceImporterWAV() {
}
