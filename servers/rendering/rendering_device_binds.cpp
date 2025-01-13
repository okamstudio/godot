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

#include "shader_include_db.h"

Error RDShaderFile::parse_versions_from_text(const String &p_text, const String p_defines, OpenIncludeFunction p_include_func, void *p_include_func_userdata) {
	ERR_FAIL_NULL_V_MSG(
			RenderingDevice::get_singleton(),
			ERR_UNAVAILABLE,
			"Cannot import custom .glsl shaders when running without a RenderingDevice. This can happen if you are using the headless more or the Compatibility renderer.");

	Vector<String> lines = p_text.split("\n");

	bool reading_versions = false;
	bool stage_found[RD::SHADER_STAGE_MAX] = { false, false, false, false, false };
	RD::ShaderStage stage = RD::SHADER_STAGE_MAX;
	static const char *stage_str[RD::SHADER_STAGE_MAX] = {
		"vertex",
		"fragment",
		"tesselation_control",
		"tesselation_evaluation",
		"compute",
	};
	String stage_code[RD::SHADER_STAGE_MAX];
	int stages_found = 0;
	HashMap<StringName, String> version_texts;

	versions.clear();
	base_error = "";

	for (int lidx = 0; lidx < lines.size(); lidx++) {
		String line = lines[lidx];

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
					for (int i = 0; i < RD::SHADER_STAGE_MAX; i++) {
						if (section == stage_str[i]) {
							if (stage_found[i]) {
								base_error = "Invalid shader file, stage appears twice: " + section;
								break;
							}

							stage_found[i] = true;
							stages_found++;

							stage = RD::ShaderStage(i);
							reading_versions = false;
							break;
						}
					}

					if (!base_error.is_empty()) {
						break;
					}
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
					if (p_include_func) {
						//process include
						String include = line.replace("#include", "").strip_edges();
						if (!include.begins_with("\"") || !include.ends_with("\"")) {
							base_error = "Malformed #include syntax, expected #include \"<path>\", found instead: " + include;
							break;
						}
						include = include.substr(1, include.length() - 2).strip_edges();

						String include_code = ShaderIncludeDB::get_built_in_include_file(include);
						if (!include_code.is_empty()) {
							stage_code[stage] += "\n" + include_code + "\n";
						} else {
							String include_text = p_include_func(include, p_include_func_userdata);
							if (!include_text.is_empty()) {
								stage_code[stage] += "\n" + include_text + "\n";
							} else {
								base_error = "#include failed for file '" + include + "'.";
							}
						}
					} else {
						base_error = "#include used, but no include function provided.";
					}
				} else {
					stage_code[stage] += line + "\n";
				}
			}
		}
	}

	if (base_error.is_empty()) {
		if (stage_found[RD::SHADER_STAGE_COMPUTE] && stages_found > 1) {
			ERR_FAIL_V_MSG(ERR_PARSE_ERROR, "When writing compute shaders, [compute] mustbe the only stage present.");
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
				String error;
				Vector<uint8_t> spirv = RenderingDevice::get_singleton()->shader_compile_spirv_from_source(RD::ShaderStage(i), code, RD::SHADER_LANGUAGE_GLSL, &error, false);
				bytecode->set_stage_bytecode(RD::ShaderStage(i), spirv);
				if (!error.is_empty()) {
					error += String() + "\n\nStage '" + stage_str[i] + "' source code: \n\n";
					Vector<String> sclines = code.split("\n");
					for (int j = 0; j < sclines.size(); j++) {
						error += itos(j + 1) + "\t\t" + sclines[j] + "\n";
					}
					errors_found = true;
				}
				bytecode->set_stage_compile_error(RD::ShaderStage(i), error);
			}

			set_bytecode(bytecode, E.key);
		}

		return errors_found ? ERR_PARSE_ERROR : OK;
	} else {
		return ERR_PARSE_ERROR;
	}
}
