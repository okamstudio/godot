/**************************************************************************/
/*  semver.h                                                              */
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

#ifndef SEMVER_H
#define SEMVER_H

#include "core/string/ustring.h"

#include "modules/regex/regex.h"

// <sys/sysmacros.h> is included somewhere, which defines major(dev) to gnu_dev_major(dev)
#if defined(major)
#undef major
#endif
#if defined(minor)
#undef minor
#endif

namespace godotsharp {

struct SemVer {
private:
	static bool parse_digit_only_field(const String &p_field, uint64_t &r_result);

	static int cmp(const SemVer &p_a, const SemVer &p_b);

public:
	int major = 0;
	int minor = 0;
	int patch = 0;
	String prerelease;
	String build_metadata;

	bool operator==(const SemVer &b) const {
		return cmp(*this, b) == 0;
	}

	bool operator!=(const SemVer &b) const {
		return !operator==(b);
	}

	bool operator<(const SemVer &b) const {
		return cmp(*this, b) < 0;
	}

	bool operator>(const SemVer &b) const {
		return cmp(*this, b) > 0;
	}

	bool operator<=(const SemVer &b) const {
		return cmp(*this, b) <= 0;
	}

	bool operator>=(const SemVer &b) const {
		return cmp(*this, b) >= 0;
	}

	SemVer() {}

	SemVer(int p_major, int p_minor, int p_patch,
			const String &p_prerelease, const String &p_build_metadata) :
			major(p_major),
			minor(p_minor),
			patch(p_patch),
			prerelease(p_prerelease),
			build_metadata(p_build_metadata) {
	}
};

struct SemVerParser {
private:
	RegEx regex;

public:
	bool parse(const String &p_ver_text, SemVer &r_semver);
};

} //namespace godotsharp

#endif // SEMVER_H
