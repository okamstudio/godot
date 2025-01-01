/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "remote_debugger_peer_websocket.h"
#include "websocket_multiplayer_peer.h"
#include "websocket_peer.h"

#ifdef WEB_ENABLED
#include "emws_peer.h"
#else
#include "wsl_peer.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/editor_debugger_server_websocket.h"
#endif

#include "core/debugger/engine_debugger.h"
#include "core/error/error_macros.h"

#ifdef TOOLS_ENABLED
#include "editor/debugger/editor_debugger_server.h"
#include "editor/editor_node.h"
#endif

#ifdef TOOLS_ENABLED
static void _editor_init_callback() {
	EditorDebuggerServer::register_protocol_handler("ws://", EditorDebuggerServerWebSocket::create);
}
#endif

void initialize_websocket_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
#ifdef WEB_ENABLED
		EMWSPeer::initialize();
#else
		WSLPeer::initialize();
#endif

		GDREGISTER_CLASS(WebSocketMultiplayerPeer);
		ClassDB::register_custom_instance_class<WebSocketPeer>();

		EngineDebugger::register_uri_handler("ws://", RemoteDebuggerPeerWebSocket::create);
		EngineDebugger::register_uri_handler("wss://", RemoteDebuggerPeerWebSocket::create);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorNode::add_init_callback(&_editor_init_callback);
	}
#endif
}

void uninitialize_websocket_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_CORE) {
		return;
	}
#ifndef WEB_ENABLED
	WSLPeer::deinitialize();
#endif
}
