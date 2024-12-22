/**************************************************************************/
/*  naming_utils.cpp                                                      */
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

#include "naming_utils.h"

#include "core/string/ucaps.h"
#include "core/templates/hash_map.h"

HashMap<String, String> _create_hashmap_from_vector(Vector<Pair<String, String>> vector) {
	HashMap<String, String> hashmap = HashMap<String, String>(vector.size());
	for (const Pair<String, String> &pair : vector) {
		hashmap.insert(pair.first, pair.second);
	}
	return hashmap;
}

// Hardcoded collection of PascalCase name conversions.
const HashMap<String, String> pascal_case_name_overrides = _create_hashmap_from_vector({
		{ "BitMap", "Bitmap" },
		{ "JSONRPC", "JsonRpc" },
		{ "Object", "GodotObject" },
		{ "OpenXRIPBinding", "OpenXRIPBinding" },
		{ "SkeletonModification2DCCDIK", "SkeletonModification2DCcdik" },
		{ "SkeletonModification2DFABRIK", "SkeletonModification2DFabrik" },
		{ "SkeletonModification3DCCDIK", "SkeletonModification3DCcdik" },
		{ "SkeletonModification3DFABRIK", "SkeletonModification3DFabrik" },
		{ "System", "System_" },
		{ "Thread", "GodotThread" },
});

// Hardcoded collection of PascalCase part conversions.
const HashMap<String, String> pascal_case_part_overrides = _create_hashmap_from_vector({
		{ "AA", "AA" }, // Anti Aliasing
		{ "AO", "AO" }, // Ambient Occlusion
		{ "FILENAME", "FileName" },
		{ "FADEIN", "FadeIn" },
		{ "FADEOUT", "FadeOut" },
		{ "FX", "FX" },
		{ "GI", "GI" }, // Global Illumination
		{ "GZIP", "GZip" },
		{ "HBOX", "HBox" }, // Horizontal Box
		{ "ID", "Id" },
		{ "IO", "IO" }, // Input/Output
		{ "IP", "IP" }, // Internet Protocol
		{ "IV", "IV" }, // Initialization Vector
		{ "MACOS", "MacOS" },
		{ "NODEPATH", "NodePath" },
		{ "SPIRV", "SpirV" },
		{ "STDIN", "StdIn" },
		{ "STDOUT", "StdOut" },
		{ "USERNAME", "UserName" },
		{ "UV", "UV" },
		{ "UV2", "UV2" },
		{ "VBOX", "VBox" }, // Vertical Box
		{ "WHITESPACE", "WhiteSpace" },
		{ "WM", "WM" },
		{ "XR", "XR" },
		{ "XRAPI", "XRApi" },
});

String _get_pascal_case_part_override(String p_part, bool p_input_is_upper = true) {
	if (!p_input_is_upper) {
		for (int i = 0; i < p_part.length(); i++) {
			p_part[i] = _find_upper(p_part[i]);
		}
	}

	if (pascal_case_part_overrides.has(p_part)) {
		return pascal_case_part_overrides.get(p_part);
	}

	return String();
}

Vector<String> _split_pascal_case(const String &p_identifier) {
	Vector<String> parts;
	int current_part_start = 0;
	bool prev_was_upper = is_ascii_upper_case(p_identifier[0]);
	for (int i = 1; i < p_identifier.length(); i++) {
		if (prev_was_upper) {
			if (is_digit(p_identifier[i]) || is_ascii_lower_case(p_identifier[i])) {
				if (!is_digit(p_identifier[i])) {
					// These conditions only apply when the separator is not a digit.
					if (i - current_part_start == 1) {
						// Upper character was only the beginning of a word.
						prev_was_upper = false;
						continue;
					}
					if (i != p_identifier.length()) {
						// If this is not the last character, the last uppercase
						// character is the start of the next word.
						i--;
					}
				}
				if (i - current_part_start > 0) {
					parts.append(p_identifier.substr(current_part_start, i - current_part_start));
					current_part_start = i;
					prev_was_upper = false;
				}
			}
		} else {
			if (is_digit(p_identifier[i]) || is_ascii_upper_case(p_identifier[i])) {
				parts.append(p_identifier.substr(current_part_start, i - current_part_start));
				current_part_start = i;
				prev_was_upper = true;
			}
		}
	}

	// Add the rest of the identifier as the last part.
	if (current_part_start != p_identifier.length()) {
		parts.append(p_identifier.substr(current_part_start));
	}

	return parts;
}

String pascal_to_pascal_case(const String &p_identifier) {
	if (p_identifier.length() == 0) {
		return p_identifier;
	}

	if (p_identifier.length() <= 2) {
		return p_identifier.to_upper();
	}

	if (pascal_case_name_overrides.has(p_identifier)) {
		// Use hardcoded value for the identifier.
		return pascal_case_name_overrides.get(p_identifier);
	}

	Vector<String> parts = _split_pascal_case(p_identifier);

	String ret;

	for (String &part : parts.write) {
		String part_override = _get_pascal_case_part_override(part);
		if (!part_override.is_empty()) {
			// Use hardcoded value for part.
			ret += part_override;
			continue;
		}

		if (part.length() <= 2 && part.to_upper() == part) {
			// Acronym of length 1 or 2.
			for (int j = 0; j < part.length(); j++) {
				part[j] = _find_upper(part[j]);
			}
			ret += part;
			continue;
		}

		part[0] = _find_upper(part[0]);
		for (int i = 1; i < part.length(); i++) {
			if (is_digit(part[i - 1])) {
				// Use uppercase after digits.
				part[i] = _find_upper(part[i]);
				continue;
			}

			part[i] = _find_lower(part[i]);
		}
		ret += part;
	}

	return ret;
}

String snake_to_pascal_case(const String &p_identifier, bool p_input_is_upper) {
	String ret;
	Vector<String> parts = p_identifier.split("_", true);

	for (int i = 0; i < parts.size(); i++) {
		String part = parts[i];

		String part_override = _get_pascal_case_part_override(part, p_input_is_upper);
		if (!part_override.is_empty()) {
			// Use hardcoded value for part.
			ret += part_override;
			continue;
		}

		if (!part.is_empty()) {
			part[0] = _find_upper(part[0]);
			for (int j = 1; j < part.length(); j++) {
				if (is_digit(part[j - 1])) {
					// Use uppercase after digits.
					part[j] = _find_upper(part[j]);
					continue;
				}

				if (p_input_is_upper) {
					part[j] = _find_lower(part[j]);
				}
			}
			ret += part;
		} else {
			if (i == 0 || i == (parts.size() - 1)) {
				// Preserve underscores at the beginning and end
				ret += "_";
			} else {
				// Preserve contiguous underscores
				if (parts[i - 1].length()) {
					ret += "__";
				} else {
					ret += "_";
				}
			}
		}
	}

	return ret;
}

String snake_to_camel_case(const String &p_identifier, bool p_input_is_upper) {
	String ret;
	Vector<String> parts = p_identifier.split("_", true);

	for (int i = 0; i < parts.size(); i++) {
		String part = parts[i];

		String part_override = _get_pascal_case_part_override(part, p_input_is_upper);
		if (!part_override.is_empty()) {
			// Use hardcoded value for part.
			if (i == 0) {
				part_override[0] = _find_lower(part_override[0]);
			}
			ret += part_override;
			continue;
		}

		if (!part.is_empty()) {
			if (i == 0) {
				part[0] = _find_lower(part[0]);
			} else {
				part[0] = _find_upper(part[0]);
			}
			for (int j = 1; j < part.length(); j++) {
				if (is_digit(part[j - 1])) {
					// Use uppercase after digits.
					part[j] = _find_upper(part[j]);
					continue;
				}

				if (p_input_is_upper) {
					part[j] = _find_lower(part[j]);
				}
			}
			ret += part;
		} else {
			if (i == 0 || i == (parts.size() - 1)) {
				// Preserve underscores at the beginning and end
				ret += "_";
			} else {
				// Preserve contiguous underscores
				if (parts[i - 1].length()) {
					ret += "__";
				} else {
					ret += "_";
				}
			}
		}
	}

	return ret;
}
