/**************************************************************************/
/*  editor_debugger_server.h                                              */
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

#include "core/debugger/remote_debugger_peer.h"
#include "core/object/ref_counted.h"

class EditorDebuggerServer : public RefCounted {
public:
	typedef EditorDebuggerServer *(*CreateServerFunc)(const String &p_uri);

private:
	static HashMap<StringName, CreateServerFunc> protocols;

public:
	static void initialize();
	static void deinitialize();

	static void register_protocol_handler(const String &p_protocol, CreateServerFunc p_func);
	static EditorDebuggerServer *create(const String &p_protocol);

	virtual String get_uri() const = 0;
	virtual void poll() = 0;
	virtual Error start(const String &p_uri) = 0;
	virtual void stop() = 0;
	virtual bool is_active() const = 0;
	virtual bool is_connection_available() const = 0;
	virtual Ref<RemoteDebuggerPeer> take_connection() = 0;
};
