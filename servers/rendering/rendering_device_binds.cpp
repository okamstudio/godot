/**************************************************************************/
/*  rendering_device_binds.cpp                                            */
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

#include "rendering_device_binds.h"
#include "core/string/string_builder.h"

#include "shader_include_db.h"

Error RDShaderFile::parse_versions_from_text(const String &p_text, const String p_defines, OpenIncludeFunction p_include_func, void *p_include_func_userdata) {
	ERR_FAIL_NULL_V_MSG(
			RenderingDevice::get_singleton(),
			ERR_UNAVAILABLE,
			"Cannot import custom .glsl shaders when running without a RenderingDevice. This can happen if you are using the headless more or the Compatibility renderer.");

	Vector<String> lines = p_text.split("\n");
	versions.clear();
	base_error = "";

	// A correct shader file starts with either #[section_name] or #version XYZ,
	// and that identifies the correct parser to use.
	for (const String &line : lines) {
		String stripped = line.strip_edges();
		if (stripped.begins_with("#[")) {
			return _parse_sectioned_text(lines, p_defines, p_include_func, p_include_func_userdata);
		} else if (stripped.begins_with("#version")) {
			return _parse_pragma_text(lines, p_defines, p_include_func, p_include_func_userdata);
		}
	}

	base_error = "Invalid shader file, could not find #[section] or #version";
	return ERR_PARSE_ERROR;
}

Error RDShaderFile::_parse_sectioned_text(const Vector<String> &p_lines, const String p_defines, OpenIncludeFunction p_include_func, void *p_include_func_userdata) {
	bool reading_versions = false;
	bool stage_found[RD::SHADER_STAGE_MAX] = { false, false, false, false, false };
	RD::ShaderStage stage = RD::SHADER_STAGE_MAX;
	String stage_code[RD::SHADER_STAGE_MAX];
	int stages_found = 0;
	HashMap<StringName, String> version_texts;

	for (int lidx = 0; lidx < p_lines.size(); lidx++) {
		String line = p_lines[lidx];

		{
			String ls = line.strip_edges();
			if (ls.begins_with("#[") && ls.ends_with("]")) {
				String section = ls.substr(2, ls.length() - 3).strip_edges();
				if (section == "versions") {
					if (stages_found) {
						base_error = "Invalid shader file, #[versions] must be the first section found.";
						break;
					}
					reading_versions = true;
				} else {
					stage = _str_to_stage(section);
					if (stage == RD::SHADER_STAGE_MAX) {
						base_error = "Unknown shader section type: " + section;
						break;
					}
					if (stage_found[stage]) {
						base_error = "Invalid shader file, stage appears twice: " + section;
						break;
					}

					stage_found[stage] = true;
					stages_found++;
					reading_versions = false;
				}
				continue;
			}
		}

		if (stage == RD::SHADER_STAGE_MAX && !line.strip_edges().is_empty()) {
			line = line.strip_edges();
			if (line.begins_with("//") || line.begins_with("/*")) {
				continue; //assuming comment (single line)
			}
		}

		if (reading_versions) {
			String l = line.strip_edges();
			if (!l.is_empty()) {
				if (!l.contains_char('=')) {
					base_error = "Missing `=` in '" + l + "'. Version syntax is `version = \"<defines with C escaping>\";`.";
					break;
				}
				if (!l.contains_char(';')) {
					// We don't require a semicolon per se, but it's needed for clang-format to handle things properly.
					base_error = "Missing `;` in '" + l + "'. Version syntax is `version = \"<defines with C escaping>\";`.";
					break;
				}
				Vector<String> slices = l.get_slice(";", 0).split("=");
				String version = slices[0].strip_edges();
				if (!version.is_valid_ascii_identifier()) {
					base_error = "Version names must be valid identifiers, found '" + version + "' instead.";
					break;
				}
				String define = slices[1].strip_edges();
				if (!define.begins_with("\"") || !define.ends_with("\"")) {
					base_error = "Version text must be quoted using \"\", instead found '" + define + "'.";
					break;
				}
				define = "\n" + define.substr(1, define.length() - 2).c_unescape() + "\n"; // Add newline before and after just in case.

				version_texts[version] = define + "\n" + p_defines;
			}
		} else {
			if (stage == RD::SHADER_STAGE_MAX && !line.strip_edges().is_empty()) {
				base_error = "Text was found that does not belong to a valid section: " + line;
				break;
			}

			if (stage != RD::SHADER_STAGE_MAX) {
				if (line.strip_edges().begins_with("#include")) {
					stage_code[stage] += _expand_include(line, p_include_func, p_include_func_userdata) + "\n";
					if (!base_error.is_empty()) {
						break;
					}
				} else {
					stage_code[stage] += line + "\n";
				}
			}
		}
	}

	if (base_error.is_empty()) {
		if (stage_found[RD::SHADER_STAGE_COMPUTE] && stages_found > 1) {
			base_error = "When writing compute shaders, [compute] must be the only stage present.";
			return ERR_PARSE_ERROR;
		}

		if (version_texts.is_empty()) {
			version_texts[""] = ""; //make sure a default version exists
		}

		bool errors_found = false;

		/* STEP 2, Compile the versions, add to shader file */

		for (const KeyValue<StringName, String> &E : version_texts) {
			Ref<RDShaderSPIRV> bytecode;
			bytecode.instantiate();

			for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
				String code = stage_code[i];
				if (code.is_empty()) {
					continue;
				}
				code = code.replace("VERSION_DEFINES", E.value);
				if (!_compile_shader(RD::ShaderStage(i), code, bytecode)) {
					errors_found = true;
				}
			}

			set_bytecode(bytecode, E.key);
		}

		return errors_found ? ERR_PARSE_ERROR : OK;
	} else {
		return ERR_PARSE_ERROR;
	}
}

Error RDShaderFile::_parse_pragma_text(const Vector<String> &p_lines, const String p_defines, OpenIncludeFunction p_include_func, void *p_include_func_userdata) {
	Vector<RD::ShaderStage> stages;
	HashMap<String, String> normalized_versions;
	bool pragmas_allowed = false;
	String version_header;
	StringBuilder codeb;

	for (const String &line : p_lines) {
		String stripped = line.strip_edges();

		if (stripped.is_empty() || stripped.begins_with("//")) {
			codeb.append(line);
			codeb.append("\n");
			continue;
		}

		// Note, this reorders the code slightly: #version gets hoisted above
		// all other source lines. In a valid GLSL file, the only valid lines
		// before #version are comments and empty lines, so this doesn't matter.
		if (stripped.begins_with("#version")) {
			if (!version_header.is_empty()) {
				base_error = vformat("Duplicate #version statement: %s", stripped);
				return ERR_PARSE_ERROR;
			}
			version_header = stripped + "\n";
			pragmas_allowed = true;
			continue;
		}

		if (stripped.begins_with("#pragma ")) {
			String pragma;
			Vector<String> vals;
			Error err = _parse_godot_pragma(stripped, &pragma, &vals);
			if (err != OK) {
				return err;
			} else if (pragma.is_empty()) {
				// Non-Godot pragma, leave it for the compiler
				codeb.append(line);
				codeb.append("\n");
				continue;
			} else if (!pragmas_allowed) {
				base_error = "Pragmas must appear after #version but before any other code.";
				return ERR_PARSE_ERROR;
			} else if (pragma == "godot_shader_stages") {
				if (!stages.is_empty()) {
					base_error = "Duplicate #pragma godot_shader_stages: " + stripped;
					return ERR_PARSE_ERROR;
				}
				for (const String &value : vals) {
					RD::ShaderStage s = _str_to_stage(value);
					if (s == RD::SHADER_STAGE_MAX) {
						base_error = "Unknown shader stage: " + value;
						return ERR_PARSE_ERROR;
					}
					if (stages.has(s)) {
						base_error = "Duplicate shader stage: " + value;
						return ERR_PARSE_ERROR;
					}
					stages.push_back(s);
				}
				continue;
			} else if (pragma == "godot_shader_versions") {
				if (!normalized_versions.is_empty()) {
					base_error = "Duplicate #pragma godot_shader_versions";
					return ERR_PARSE_ERROR;
				}
				// We can't allow multiple versions called "foo", "FOO", "FoO" because the
				// preprocessor symbols will be the same.
				for (const String &version : vals) {
					String up = version.to_upper();
					if (normalized_versions.has(up)) {
						base_error = "Duplicate version: " + version;
						return ERR_PARSE_ERROR;
					}
					normalized_versions[up] = version;
				}
				continue;
			} else {
				base_error = "Unknown pragma: " + pragma;
				return ERR_PARSE_ERROR;
			}
		}

		// All the stuff that can happen before/during pragmas is handled above.
		pragmas_allowed = false;
		codeb.append(_expand_include(line, p_include_func, p_include_func_userdata));
		codeb.append("\n");
	}

	if (version_header.is_empty()) {
		base_error = "Invalid shader, missing #version statement at beginning";
		return ERR_PARSE_ERROR;
	}
	if (normalized_versions.is_empty()) {
		// Simple shaders don't need to explicitly declare a version.
		normalized_versions[""] = "";
	}
	if (stages.is_empty()) {
		base_error = "Invalid shader, #pragma godot_shader_stages not found";
		return ERR_PARSE_ERROR;
	}
	if (stages.size() > 1 && stages.has(RD::SHADER_STAGE_COMPUTE)) {
		base_error = "Compute shaders must not include other shader stages";
		return ERR_PARSE_ERROR;
	}

	String code = codeb.as_string();
	bool errors_found = false;
	for (const KeyValue<String, String> &version : normalized_versions) {
		String version_def = version.key.is_empty() ? "" : vformat("#define GODOT_VERSION_%s\n", version.key);
		Ref<RDShaderSPIRV> bytecode;
		bytecode.instantiate();

		for (const RD::ShaderStage stage : stages) {
			String stage_def = vformat("#define GODOT_STAGE_%s\n", _stage_to_str(stage).to_upper());
			String src = version_header + stage_def + version_def + code;
			if (!_compile_shader(stage, src, bytecode)) {
				errors_found = true;
			}
		}

		set_bytecode(bytecode, version.value);
	}

	return errors_found ? ERR_PARSE_ERROR : OK;
}

const char *RDShaderFile::_stage_str[RD::SHADER_STAGE_MAX] = {
	"vertex",
	"fragment",
	"tessellation_control",
	"tessellation_evaluation",
	"compute",
};

RD::ShaderStage RDShaderFile::_str_to_stage(const String &s) {
	for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
		if (s == _stage_str[i]) {
			return RD::ShaderStage(i);
		}
	}
	// Compatibility names (only one L in "tessellation" instead of two)
	// for historical typo, so that older shaders keep working.
	if (s == "tesselation_control") {
		return RD::SHADER_STAGE_TESSELATION_CONTROL;
	} else if (s == "tesselation_evaluation") {
		return RD::SHADER_STAGE_TESSELATION_EVALUATION;
	}
	return RD::SHADER_STAGE_MAX;
}

String RDShaderFile::_stage_to_str(const RD::ShaderStage s) {
	if (s < 0 || s >= RD::SHADER_STAGE_MAX) {
		return "<invalid>";
	}
	return _stage_str[s];
}

bool RDShaderFile::_compile_shader(const RD::ShaderStage p_stage, const String &p_code, Ref<RDShaderSPIRV> p_bytecode) {
	String error;
	Vector<uint8_t> spirv = RenderingDevice::get_singleton()->shader_compile_spirv_from_source(p_stage, p_code, RD::SHADER_LANGUAGE_GLSL, &error, false);
	p_bytecode->set_stage_bytecode(p_stage, spirv);
	if (!error.is_empty()) {
		StringBuilder errb;
		errb.append(error);
		errb.append(vformat("\n\nStage '%s' source code:\n\n", _stage_to_str(p_stage)));
		Vector<String> lines = p_code.split("\n");
		for (int i = 0; i < lines.size(); i++) {
			errb.append(vformat("%d\t\t%s\n", i + 1, lines[i]));
		}
		p_bytecode->set_stage_compile_error(p_stage, errb.as_string());
	}
	return error.is_empty();
}

String RDShaderFile::_expand_include(const String &p_line, OpenIncludeFunction p_include_func, void *p_include_func_userdata) {
	if (!p_line.strip_edges().begins_with("#include ")) {
		return p_line;
	}

	if (!p_include_func) {
		base_error = "#include used, but no include function provided.";
		return "";
	}

	String include = p_line.trim_prefix("#include ").strip_edges();
	if (!include.is_quoted()) {
		base_error = "Malformed #include syntax, expected #include \"<path>\", found instead: " + include;
		return "";
	}
	include = include.unquote();

	String include_text = ShaderIncludeDB::get_built_in_include_file(include);
	if (include_text.is_empty()) {
		include_text = p_include_func(include, p_include_func_userdata);
	}
	if (include_text.is_empty()) {
		base_error = "#include failed for file '" + include + "'";
		return "";
	}
	return include_text;
}

Error RDShaderFile::_parse_godot_pragma(const String &p_line, String *r_pragma_name, Vector<String> *r_pragma_vals) {
	*r_pragma_name = "";

	String pragma = p_line.trim_prefix("#pragma ").strip_edges();
	if (!pragma.begins_with("godot_")) {
		// Not a Godot pragma, report success. Empty r_pragma_name tells the
		// caller what happened.
		return OK;
	}

	if (!pragma.ends_with(")") || pragma.count("(") != 1) {
		base_error = "Malformed #pragma syntax, expected #pragma godot_pragma_name(vals), found instead: " + p_line;
		return ERR_PARSE_ERROR;
	}

	Vector<String> parts = pragma.trim_suffix(")").split("(");
	*r_pragma_name = parts[0].strip_edges();
	*r_pragma_vals = parts[1].split(",");

	if (!r_pragma_name->is_valid_ascii_identifier()) {
		base_error = "Pragma names must be valid identifiers, found instead: " + *r_pragma_name;
		return ERR_PARSE_ERROR;
	}
	if (r_pragma_vals->is_empty()) {
		base_error = "Invalid empty #pragma value: " + p_line;
		return ERR_PARSE_ERROR;
	}
	for (String &value : *r_pragma_vals) {
		value = value.strip_edges();
		if (!value.is_valid_ascii_identifier()) {
			base_error = "Pragma values must be valid identifiers, found instead: " + value;
			return ERR_PARSE_ERROR;
		}
	}
	return OK;
}
