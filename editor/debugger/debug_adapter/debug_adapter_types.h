/**************************************************************************/
/*  debug_adapter_types.h                                                 */
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

#include "core/io/json.h"
#include "core/variant/dictionary.h"

namespace DAP {

enum ErrorType {
	UNKNOWN,
	WRONG_PATH,
	NOT_RUNNING,
	TIMEOUT,
	UNKNOWN_PLATFORM,
	MISSING_DEVICE
};

struct Checksum {
	String algorithm;
	String checksum;

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["algorithm"] = algorithm;
		dict["checksum"] = checksum;

		return dict;
	}
};

struct Source {
private:
	Array _checksums;

public:
	String name;
	String path;

	void compute_checksums() {
		ERR_FAIL_COND(path.is_empty());

		// MD5
		Checksum md5;
		md5.algorithm = "MD5";
		md5.checksum = FileAccess::get_md5(path);

		// SHA-256
		Checksum sha256;
		sha256.algorithm = "SHA256";
		sha256.checksum = FileAccess::get_sha256(path);

		_checksums.push_back(md5.to_json());
		_checksums.push_back(sha256.to_json());
	}

	_FORCE_INLINE_ void from_json(const Dictionary &p_params) {
		name = p_params["name"];
		path = p_params["path"];
		_checksums = p_params["checksums"];
	}

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["name"] = name;
		dict["path"] = path;
		dict["checksums"] = _checksums;

		return dict;
	}
};

struct Breakpoint {
	int id = 0;
	bool verified = false;
	Source source;
	int line = 0;

	bool operator==(const Breakpoint &p_other) const {
		return source.path == p_other.source.path && line == p_other.line;
	}

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["id"] = id;
		dict["verified"] = verified;
		dict["source"] = source.to_json();
		dict["line"] = line;

		return dict;
	}
};

struct BreakpointLocation {
	int line = 0;
	int endLine = -1;

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["line"] = line;
		if (endLine >= 0) {
			dict["endLine"] = endLine;
		}

		return dict;
	}
};

struct Capabilities {
	bool supportsConfigurationDoneRequest = true;
	bool supportsEvaluateForHovers = true;
	bool supportsSetVariable = true;
	String supportedChecksumAlgorithms[2] = { "MD5", "SHA256" };
	bool supportsRestartRequest = true;
	bool supportsValueFormattingOptions = true;
	bool supportTerminateDebuggee = true;
	bool supportSuspendDebuggee = true;
	bool supportsTerminateRequest = true;
	bool supportsBreakpointLocationsRequest = true;

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["supportsConfigurationDoneRequest"] = supportsConfigurationDoneRequest;
		dict["supportsEvaluateForHovers"] = supportsEvaluateForHovers;
		dict["supportsSetVariable"] = supportsSetVariable;
		dict["supportsRestartRequest"] = supportsRestartRequest;
		dict["supportsValueFormattingOptions"] = supportsValueFormattingOptions;
		dict["supportTerminateDebuggee"] = supportTerminateDebuggee;
		dict["supportSuspendDebuggee"] = supportSuspendDebuggee;
		dict["supportsTerminateRequest"] = supportsTerminateRequest;
		dict["supportsBreakpointLocationsRequest"] = supportsBreakpointLocationsRequest;

		Array arr;
		arr.push_back(supportedChecksumAlgorithms[0]);
		arr.push_back(supportedChecksumAlgorithms[1]);
		dict["supportedChecksumAlgorithms"] = arr;

		return dict;
	}
};

struct Message {
	int id = 0;
	String format;
	bool sendTelemetry = false; // Just in case :)
	bool showUser = false;
	Dictionary variables;

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["id"] = id;
		dict["format"] = format;
		dict["sendTelemetry"] = sendTelemetry;
		dict["showUser"] = showUser;
		dict["variables"] = variables;

		return dict;
	}
};

struct Scope {
	String name;
	String presentationHint;
	int variablesReference = 0;
	bool expensive = false;

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["name"] = name;
		dict["presentationHint"] = presentationHint;
		dict["variablesReference"] = variablesReference;
		dict["expensive"] = expensive;

		return dict;
	}
};

struct SourceBreakpoint {
	int line = 0;

	_FORCE_INLINE_ void from_json(const Dictionary &p_params) {
		line = p_params["line"];
	}
};

struct StackFrame {
	int id = 0;
	String name;
	Source source;
	int line = 0;
	int column = 0;

	static uint32_t hash(const StackFrame &p_frame) {
		return hash_murmur3_one_32(p_frame.id);
	}
	bool operator==(const StackFrame &p_other) const {
		return id == p_other.id;
	}

	_FORCE_INLINE_ void from_json(const Dictionary &p_params) {
		id = p_params["id"];
		name = p_params["name"];
		source.from_json(p_params["source"]);
		line = p_params["line"];
		column = p_params["column"];
	}

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["id"] = id;
		dict["name"] = name;
		dict["source"] = source.to_json();
		dict["line"] = line;
		dict["column"] = column;

		return dict;
	}
};

struct Thread {
	int id = 0;
	String name;

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["id"] = id;
		dict["name"] = name;

		return dict;
	}
};

struct Variable {
	String name;
	String value;
	String type;
	int variablesReference = 0;

	_FORCE_INLINE_ Dictionary to_json() const {
		Dictionary dict;
		dict["name"] = name;
		dict["value"] = value;
		dict["type"] = type;
		dict["variablesReference"] = variablesReference;

		return dict;
	}
};

} // namespace DAP
