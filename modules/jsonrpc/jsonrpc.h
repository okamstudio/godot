/**************************************************************************/
/*  jsonrpc.h                                                             */
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

#include "core/object/class_db.h"
#include "core/variant/variant.h"

class JSONRPC : public Object {
	GDCLASS(JSONRPC, Object)

	HashMap<String, Object *> method_scopes;

protected:
	static void _bind_methods();

public:
	JSONRPC();
	~JSONRPC();

	enum ErrorCode {
		PARSE_ERROR = -32700,
		INVALID_REQUEST = -32600,
		METHOD_NOT_FOUND = -32601,
		INVALID_PARAMS = -32602,
		INTERNAL_ERROR = -32603,
	};

	Dictionary make_response_error(int p_code, const String &p_message, const Variant &p_id = Variant()) const;
	Dictionary make_response(const Variant &p_value, const Variant &p_id);
	Dictionary make_notification(const String &p_method, const Variant &p_params);
	Dictionary make_request(const String &p_method, const Variant &p_params, const Variant &p_id);

	Variant process_action(const Variant &p_action, bool p_process_arr_elements = false);
	String process_string(const String &p_input);

	void set_scope(const String &p_scope, Object *p_obj);
};

VARIANT_ENUM_CAST(JSONRPC::ErrorCode);
