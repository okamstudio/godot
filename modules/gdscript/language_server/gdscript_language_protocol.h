/**************************************************************************/
/*  gdscript_language_protocol.h                                          */
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

#ifndef GDSCRIPT_LANGUAGE_PROTOCOL_H
#define GDSCRIPT_LANGUAGE_PROTOCOL_H

#include "gdscript_text_document.h"
#include "gdscript_workspace.h"

#include "core/io/stream_peer_tcp.h"
#include "core/io/tcp_server.h"

#include "modules/modules_enabled.gen.h" // For jsonrpc.
#ifdef MODULE_JSONRPC_ENABLED
#include "modules/jsonrpc/jsonrpc.h"
#else
#error "Can't build GDScript LSP without JSONRPC module."
#endif

#define LSP_MAX_BUFFER_SIZE 4194304
#define LSP_MAX_CLIENTS 8

class GDScriptLanguageProtocol : public JSONRPC {
	GDCLASS(GDScriptLanguageProtocol, JSONRPC)

private:
	struct LSPeer : RefCounted {
		Ref<StreamPeerTCP> connection;

		uint8_t req_buf[LSP_MAX_BUFFER_SIZE];
		int req_pos = 0;
		bool has_header = false;
		bool has_content = false;
		int content_length = 0;
		Vector<CharString> res_queue;
		int res_sent = 0;

		Error handle_data();
		Error send_data();
	};

	enum LSPErrorCode {
		RequestCancelled = -32800,
		ContentModified = -32801,
	};

	static GDScriptLanguageProtocol *singleton;

	HashMap<int, Ref<LSPeer>> clients;
	Ref<TCPServer> server;
	int latest_client_id = 0;
	int next_client_id = 0;

	int next_server_id = 0;

	Ref<GDScriptTextDocument> text_document;
	Ref<GDScriptWorkspace> workspace;

	Error on_client_connected();
	void on_client_disconnected(const int &p_client_id);

	String process_message(const String &p_text);
	String format_output(const String &p_text);

	bool _initialized = false;

protected:
	static void _bind_methods();

	Dictionary initialize(const Dictionary &p_params);
	void initialized(const Variant &p_params);

public:
	_FORCE_INLINE_ static GDScriptLanguageProtocol *get_singleton() { return singleton; }
	_FORCE_INLINE_ Ref<GDScriptWorkspace> get_workspace() { return workspace; }
	_FORCE_INLINE_ Ref<GDScriptTextDocument> get_text_document() { return text_document; }
	_FORCE_INLINE_ bool is_initialized() const { return _initialized; }

	void poll(int p_limit_usec);
	Error start(int p_port, const IPAddress &p_bind_ip);
	void stop();

	void notify_client(const String &p_method, const Variant &p_params = Variant(), int p_client_id = -1);
	void request_client(const String &p_method, const Variant &p_params = Variant(), int p_client_id = -1);

	bool is_smart_resolve_enabled() const;
	bool is_goto_native_symbols_enabled() const;

	GDScriptLanguageProtocol();
};

#endif // GDSCRIPT_LANGUAGE_PROTOCOL_H
